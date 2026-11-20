/*
 * tests/kabul/cekirdek — ortak düzenek (girdi kurucular, çok örnekli çağrı, alan alan
 * serileştirme, genel değişmezler, parametre bloğu). Açıklama başlıkta.
 *
 * Model yalnız A4'ün açıkça yazdığı şeyleri izler: son kabul edilen zaman çerçevesi
 * (4.14 "son geçerli"), olay kimliği (A4-K61), arka plan dilim sayımı (A4-K109),
 * BAD_INPUT dönen arka plan çağrıları (A4-K117), duvar saati geri adımı (A4-K10),
 * bekletilen kapanma ve günlükler (3.11 (b2)).
 */
#include "kabul_cekirdek.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "unity.h"

/* ---- Hata iletisi ---------------------------------------------------------------- */

static char g_baglam[160];

_Noreturn void kc_basarisiz(int satir, const char *bicim, ...)
{
    static char msj[4][768];
    static unsigned k;
    char govde[600];
    va_list ap;
    va_start(ap, bicim);
    (void)vsnprintf(govde, sizeof govde, bicim, ap);
    va_end(ap);
    k = (k + 1u) % 4u;
    (void)snprintf(msj[k], sizeof msj[k], "%s [%s]", govde, g_baglam);
    UnityFail(msj[k], (UNITY_LINE_TYPE)satir);
}

/* ---- Sözde rastgele sayı ---------------------------------------------------------- */

void kc_rng_kur(kc_rng_t *r, uint64_t tohum)
{
    r->s = tohum;
}

uint64_t kc_rng_u64(kc_rng_t *r)
{
    uint64_t z;
    r->s += UINT64_C(0x9E3779B97F4A7C15);
    z = r->s;
    z = (z ^ (z >> 30)) * UINT64_C(0xBF58476D1CE4E5B9);
    z = (z ^ (z >> 27)) * UINT64_C(0x94D049BB133111EB);
    return z ^ (z >> 31);
}

uint32_t kc_rng_aralik(kc_rng_t *r, uint32_t n)
{
    if (n == 0u) {
        return 0u;
    }
    return (uint32_t)(kc_rng_u64(r) % (uint64_t)n);
}

static void rastgele_doldur(void *p, size_t n, uint64_t tohum)
{
    kc_rng_t r;
    unsigned char *b = (unsigned char *)p;
    kc_rng_kur(&r, tohum);
    for (size_t i = 0; i < n; ++i) {
        b[i] = (unsigned char)(kc_rng_u64(&r) & 0xFFu);
    }
}

/* ---- Örnek havuzu ----------------------------------------------------------------- */

#define KC_HAVUZ ((size_t)1 << 20)
#define KC_SER_AZAMI ((size_t)32768)

typedef union {
    long double ld;
    uint64_t u;
    void *p;
    double d;
    unsigned char b[KC_HAVUZ];
} kc_havuz_t;

static kc_havuz_t g_havuz[KC_N_AZAMI];
static unsigned char g_kopya[KC_N_AZAMI][KC_HAVUZ];

/* Model (A4'ün açık kurallarından). */
typedef struct {
    int n;
    size_t boy;
    bool denetim;
    /* başlatma */
    bool kurulu;
    ida_rc_t init_rc;
    uint32_t boot_count;
    bool boot_ok;
    bool olay_acik;
    uint8_t reset_cause;
    uint8_t consecutive_faults;
    int32_t param_load_arg1; /* -1: bilinmiyor */
    /* zaman çerçevesi izi (4.14 "son geçerli") */
    bool iz_var;
    uint32_t iz_seq;
    int64_t iz_t;
    /* olay kimliği (A4-K61) */
    bool olay_var;
    uint32_t son_ctr;
    uint64_t son_id;
    /* arka plan (A4-K109, A4-K117) */
    uint32_t bg_sayim; /* son step/tick'ten beri IDLE/MORE dönen arka plan çağrısı */
    bool bg_elle;
    uint32_t bg_elle_deger;
    uint32_t bg_bad_sayi; /* son yazılabilir çağrıdan beri BAD dönen arka plan çağrısı */
    /* duvar saati (A4-K10) */
    bool now_var;
    int64_t now_azami;            /* geri adım izi; tabanı init'in now_w'si (A4-K10) */
    int64_t now_ust;              /* init dahil görülen en büyük geçerli now_w */
    int64_t init_now;             /* RESET_INFO ve PARAM_LOAD'ın t_w'si (3.8) */
    uint32_t geri_adim;           /* son yazılabilir çağrıdan beri geri adım */
    uint32_t geri_adim_cagri;     /* bunların step/tick'te olanları (günlük üretir) */
    bool geri_adim_belirsiz;      /* kullanılmıyor: aralık dışı now_w geri adım sayılmaz (A4-K10) */
    bool geri_adim_log_belirsiz;  /* arka plan geri adımı (günlüğün hangi çağrıda çıkacağı açık değil) */
    /* (b2) bekletilenler; t_w'leri üretildikleri out NULL çağrısının kullanılan now_w'si (3.8) */
    int64_t out_null_now[32];
    int n_out_null_now;
    bool bekleyen_kapanma;
    /* A4-K73: son SESSION_END'den beri SESSION_START gelmedi (yeni kapanma üretilmez) */
    bool oturum_kapali;
    uint32_t oturum_olayi; /* son yazılabilir çağrıdan beri link_event ≠ NONE çağrı sayısı */
    bool oturum_belirsiz;
    bool out_null_var;
    /* yazılabilir çıktılar */
    bool yazilabilir_var;
    ida_core_counters_t onceki_cnt;
    ida_out_t son;
    ida_out_t onceki;
    uint64_t cagri_no;
} kc_model_t;

static kc_model_t M;
static ida_out_t g_out[KC_N_AZAMI];
static uint8_t g_ser[KC_N_AZAMI][KC_SER_AZAMI];
static size_t g_ser_n[KC_N_AZAMI];

static const unsigned char BELLEK_DESEN[KC_N_AZAMI] = {0x00u, 0xFFu, 0x5Au, 0x00u};
static const unsigned char CIKTI_DESEN[KC_N_AZAMI] = {0x00u, 0xFFu, 0xA5u, 0x00u};

void kc_ortam_kur(int n_ornek)
{
    ida_core_info_t bilgi;
    memset(&bilgi, 0, sizeof bilgi);
    ida_core_info(&bilgi);
    KC_DOGRULA(n_ornek >= 1 && n_ornek <= KC_N_AZAMI, "örnek sayısı %d", n_ornek);
    KC_DOGRULA(bilgi.core_size > 0u && (size_t)bilgi.core_size <= KC_HAVUZ,
               "core_size %lu düzeneğin havuzuna (%lu) sığmıyor", (unsigned long)bilgi.core_size,
               (unsigned long)KC_HAVUZ);
    KC_DOGRULA(bilgi.core_align > 0u && (size_t)bilgi.core_align <= _Alignof(kc_havuz_t),
               "core_align %lu", (unsigned long)bilgi.core_align);
    memset(&M, 0, sizeof M);
    M.n = n_ornek;
    M.boy = (size_t)bilgi.core_size;
    M.denetim = true;
    M.param_load_arg1 = -1;
    for (int i = 0; i < n_ornek; ++i) {
        if (i == KC_N_AZAMI - 1) {
            rastgele_doldur(g_havuz[i].b, M.boy, UINT64_C(0xC0DE0000) + (uint64_t)i);
        } else {
            memset(g_havuz[i].b, BELLEK_DESEN[i], M.boy);
        }
    }
    (void)snprintf(g_baglam, sizeof g_baglam, "ortam kuruldu");
}

int kc_ornek_sayisi(void)
{
    return M.n;
}

ida_core_t *kc_core(int i)
{
    return (ida_core_t *)(void *)g_havuz[i].b;
}

unsigned char *kc_bellek(int i)
{
    return g_havuz[i].b;
}

size_t kc_core_boyu(void)
{
    return M.boy;
}

void kc_genel_denetim_ac(bool acik)
{
    M.denetim = acik;
}

/* ---- Küçük yardımcılar ------------------------------------------------------------ */

int kc_bit_sayisi(uint32_t x)
{
    int n = 0;
    while (x != 0u) {
        n += (int)(x & 1u);
        x >>= 1;
    }
    return n;
}

void kc_le16(uint8_t *p, uint16_t v)
{
    p[0] = (uint8_t)(v & 0xFFu);
    p[1] = (uint8_t)(v >> 8);
}

void kc_le32(uint8_t *p, uint32_t v)
{
    for (int i = 0; i < 4; ++i) {
        p[i] = (uint8_t)((v >> (8 * i)) & 0xFFu);
    }
}

void kc_le64(uint8_t *p, uint64_t v)
{
    for (int i = 0; i < 8; ++i) {
        p[i] = (uint8_t)((v >> (8 * i)) & 0xFFu);
    }
}

uint32_t kc_param_ham(const ida_param_value_t *v)
{
    uint32_t u;
    memcpy(&u, &v->v, sizeof u);
    return u;
}

static uint8_t bayt_oku(const void *p)
{
    uint8_t b;
    memcpy(&b, p, 1);
    return b;
}

static bool sifir_mi(const void *p, size_t n)
{
    const unsigned char *b = (const unsigned char *)p;
    for (size_t i = 0; i < n; ++i) {
        if (b[i] != 0u) {
            return false;
        }
    }
    return true;
}

/* ---- Başlatma --------------------------------------------------------------------- */

void kc_init_girdi(ida_init_in_t *ii)
{
    memset(ii, 0, sizeof *ii);
    ii->port_major = (uint16_t)IDA_PORT_VERSION_MAJOR;
    ii->port_minor = (uint16_t)IDA_PORT_VERSION_MINOR;
    ii->now_w.ns = KC_T0;
    ii->boot_count = KC_BOOT;
    ii->boot_count_ok = true;
    ii->reset_cause = (uint8_t)IDA_RESET_SIL_START;
    ii->consecutive_faults = 0u;
    ii->param_src = (uint8_t)IDA_PSRC_NONE;
    ii->param_blob = NULL;
    ii->param_len = 0u;
}

ida_rc_t kc_init(const ida_init_in_t *ii)
{
    ida_rc_t rc0 = IDA_RC_OK;
    for (int i = 0; i < M.n; ++i) {
        ida_rc_t rc = ida_core_init(kc_core(i), ii);
        if (i == 0) {
            rc0 = rc;
        } else {
            KC_DOGRULA(rc == rc0, "init dönüşü örnekler arasında farklı: %d / %d (A4-K24)", (int)rc0,
                       (int)rc);
        }
    }
    M.kurulu = (ii != NULL) && (rc0 == IDA_RC_OK || rc0 == IDA_RC_PARAM_DEFAULTS);
    M.init_rc = rc0;
    M.iz_var = false;
    M.iz_seq = 0u;
    M.iz_t = 0;
    M.olay_var = false;
    M.son_ctr = 0u;
    M.son_id = 0u;
    M.bg_sayim = 0u;
    M.bg_elle = false;
    M.bg_bad_sayi = 0u;
    M.geri_adim = 0u;
    M.geri_adim_cagri = 0u;
    M.geri_adim_belirsiz = false;
    M.geri_adim_log_belirsiz = false;
    M.now_ust = (ii != NULL && ii->now_w.ns >= 0 && ii->now_w.ns <= IDA_TIME_MAX_NS) ? ii->now_w.ns : 0;
    /* A4-K10: geri adım izinin tabanı init'in now_w'sidir. */
    M.now_var = M.kurulu;
    M.now_azami = M.now_ust;
    M.init_now = M.now_ust;
    M.n_out_null_now = 0;
    M.oturum_kapali = false;
    M.bekleyen_kapanma = false;
    M.oturum_olayi = 0u;
    M.oturum_belirsiz = false;
    M.out_null_var = false;
    M.yazilabilir_var = false;
    memset(&M.onceki_cnt, 0, sizeof M.onceki_cnt);
    M.param_load_arg1 = -1;
    if (ii != NULL) {
        M.boot_count = ii->boot_count;
        M.boot_ok = ii->boot_count_ok;
        M.olay_acik = ii->boot_count_ok && ii->boot_count >= IDA_BOOT_COUNT_MIN &&
                      ii->boot_count <= IDA_BOOT_COUNT_MAX;
        M.reset_cause = (uint8_t)(ii->reset_cause <= IDA_RESET_SIL_REBOOT ? ii->reset_cause : 0u);
        M.consecutive_faults = ii->consecutive_faults;
        if (ii->param_blob == NULL || ii->param_len == 0u) {
            M.param_load_arg1 = 0;
        }
    }
    return rc0;
}

void kc_param_load_beklenen(int32_t arg1)
{
    M.param_load_arg1 = arg1;
}

void kc_init_bloksuz(void)
{
    ida_init_in_t ii;
    kc_init_girdi(&ii);
    ida_rc_t rc = kc_init(&ii);
    KC_DOGRULA(rc == IDA_RC_PARAM_DEFAULTS, "bloksuz init %d döndü, PARAM_DEFAULTS bekleniyordu (A4-K78)",
               (int)rc);
}

static uint8_t g_blok[IDA_PARAM_BLOB_MAX + 64u];
static kc_pkayit_t g_kayit[KC_PARAM_AZAMI];

void kc_init_gecerli(void)
{
    ida_init_in_t ii;
    size_t n = kc_varsayilan_kayitlar(g_kayit, KC_PARAM_AZAMI);
    size_t boy = kc_blok_kur(g_blok, sizeof g_blok, g_kayit, n, kc_sema_ozeti());
    kc_init_girdi(&ii);
    ii.param_src = (uint8_t)IDA_PSRC_SIL;
    ii.param_blob = g_blok;
    ii.param_len = (uint32_t)boy;
    ida_rc_t rc = kc_init(&ii);
    KC_DOGRULA(rc == IDA_RC_OK, "şema varsayılanlarından kurulan geçerli blokla init %d döndü (A4-K80)",
               (int)rc);
    M.param_load_arg1 = 0;
}

/* ---- Girdi kurucular -------------------------------------------------------------- */

void kc_in_bos(ida_in_t *in)
{
    memset(in, 0, sizeof *in);
    in->plat.link_state = (uint8_t)IDA_LINK_RUNNING;
    in->plat.link_event = (uint8_t)IDA_LINKEV_NONE;
    in->plat.param_store = (uint8_t)IDA_PSTORE_IDLE;
    in->ping360 = NULL;
}

void kc_imu_kur(ida_imu_sample_t *imu, uint32_t seq, int64_t t)
{
    memset(imu, 0, sizeof *imu);
    imu->hdr.t_meas.ns = t;
    imu->hdr.t_rx.ns = t;
    imu->hdr.seq = seq;
    imu->hdr.window_ns = (uint32_t)KC_P;
    imu->hdr.flags = (uint16_t)IDA_HF_VALID;
    imu->hdr.type = (uint8_t)IDA_SENS_IMU;
    imu->hdr.inst = 0u;
    imu->imu.gyro[0] = 0.0f;
    imu->imu.gyro[1] = 0.0f;
    imu->imu.gyro[2] = 0.0f;
    imu->imu.accel[0] = 0.0f;
    imu->imu.accel[1] = 0.0f;
    imu->imu.accel[2] = -9.80665f;
    imu->imu.temp_c = 20.0f;
}

void kc_in_step(ida_in_t *in, uint32_t seq, int64_t t)
{
    kc_in_bos(in);
    in->now_w.ns = t;
    in->tf.present = true;
    in->tf.seq = seq;
    in->tf.t.ns = t;
    in->tf.t_rx.ns = t;
    in->imu_present = true;
    kc_imu_kur(&in->imu, seq, t);
}

void kc_in_tick(ida_in_t *in, uint32_t seq, int64_t t)
{
    kc_in_bos(in);
    in->now_w.ns = t;
    in->tf.present = true;
    in->tf.seq = seq;
    in->tf.t.ns = t;
    in->tf.t_rx.ns = t;
    in->imu_present = false;
}

void kc_in_izgara(ida_in_t *in, int64_t now)
{
    kc_in_bos(in);
    in->now_w.ns = now;
    in->tf.present = false;
    in->imu_present = false;
}

void kc_in_oturum_sonu(ida_in_t *in, int64_t now)
{
    kc_in_izgara(in, now);
    in->plat.link_event = (uint8_t)IDA_LINKEV_SESSION_END;
    in->plat.link_state = (uint8_t)IDA_LINK_NONE;
}

ida_sample_t *kc_ornek_ekle(ida_in_t *in, uint8_t type, uint8_t inst, uint32_t seq, int64_t t)
{
    KC_DOGRULA(in->n_samples < IDA_PORT_MAX_SAMPLES, "örnek dizisi dolu");
    ida_sample_t *s = &in->samples[in->n_samples];
    in->n_samples = (uint8_t)(in->n_samples + 1u);
    memset(s, 0, sizeof *s);
    s->hdr.t_meas.ns = t;
    s->hdr.t_rx.ns = t;
    s->hdr.seq = seq;
    s->hdr.window_ns = 0u;
    s->hdr.flags = (uint16_t)IDA_HF_VALID;
    s->hdr.type = type;
    s->hdr.inst = inst;
    if (type == IDA_SENS_MAG) {
        s->u.mag.field[0] = 2.0e-5f;
        s->u.mag.field[1] = 0.0f;
        s->u.mag.field[2] = 4.0e-5f;
    } else if (type == IDA_SENS_BARO) {
        s->hdr.window_ns = (uint32_t)(10 * KC_MS);
        s->u.baro.p_abs = 101325.0f;
        s->u.baro.temp_c = 15.0f;
    }
    return s;
}

ida_sample_t *kc_emniyet_ekle(ida_in_t *in, uint8_t inst, uint32_t seq, int64_t t, uint8_t estop,
                              uint8_t tube0, uint8_t tube1)
{
    ida_sample_t *s = kc_ornek_ekle(in, (uint8_t)IDA_SENS_SAFETY, inst, seq, t);
    s->u.safety.estop = estop;
    s->u.safety.tube[0] = tube0;
    s->u.safety.tube[1] = tube1;
    return s;
}

ida_cmd_t *kc_komut_ekle(ida_in_t *in, uint32_t cmd_id, uint8_t type)
{
    KC_DOGRULA(in->n_cmds < IDA_PORT_MAX_CMDS, "komut dizisi dolu");
    ida_cmd_t *c = &in->cmds[in->n_cmds];
    in->n_cmds = (uint8_t)(in->n_cmds + 1u);
    memset(c, 0, sizeof *c);
    c->cmd_id = cmd_id;
    c->type = type;
    c->source = 255u;
    return c;
}

ida_cmd_t *kc_fc_ekle(ida_in_t *in, uint32_t cmd_id, uint8_t op, uint8_t tube)
{
    ida_cmd_t *c = kc_komut_ekle(in, cmd_id, (uint8_t)IDA_CMD_FC);
    c->u.fc.op = op;
    c->u.fc.tube = tube;
    c->u.fc.arg0 = 0u;
    c->u.fc.arg1 = 0u;
    return c;
}

void kc_mandal_kur(ida_in_t *in, uint8_t bitler, uint32_t cmd_id_taban)
{
    in->narrow.mask = (uint8_t)(in->narrow.mask | bitler);
    for (unsigned i = 0; i < IDA_PORT_N_NARROW; ++i) {
        if ((bitler & (1u << i)) != 0u) {
            in->narrow.cmd_id[i] = cmd_id_taban + (uint32_t)i;
        }
    }
    if ((bitler & IDA_NARROW_MODE) != 0u && in->narrow.mode == 0u) {
        in->narrow.mode = (uint8_t)IDA_MODE_STATION_KEEP;
    }
}

/* ---- Komut sınıfları (A4 §3.4, A4-K44, A4-K104) ----------------------------------- */

uint8_t kc_sonuc_turu(uint8_t cmd_type)
{
    if (cmd_type == IDA_CMD_PARAM_GET || cmd_type == IDA_CMD_PARAM_SET) {
        return (uint8_t)IDA_RES_PARAM_VALUE;
    }
    if (cmd_type == IDA_CMD_PARAM_SAVE) {
        return (uint8_t)IDA_RES_PARAM_SAVE;
    }
    return (uint8_t)IDA_RES_ACK;
}

bool kc_daraltici_komut(const ida_cmd_t *c)
{
    if (c->type == IDA_CMD_DISARM) {
        return true;
    }
    if (c->type == IDA_CMD_SET_MODE) {
        return c->u.set_mode.mode != IDA_MODE_GUIDED && c->u.set_mode.mode != IDA_MODE_AUTO;
    }
    if (c->type == IDA_CMD_FC) {
        return c->u.fc.op == IDA_FC_DISARM || c->u.fc.op == IDA_FC_ABORT;
    }
    return false;
}

bool kc_genisletici_komut(const ida_cmd_t *c)
{
    if (c->type == IDA_CMD_ARM || c->type == IDA_CMD_FAILSAFE_RESET) {
        return true;
    }
    if (c->type == IDA_CMD_SET_MODE) {
        return c->u.set_mode.mode == IDA_MODE_GUIDED || c->u.set_mode.mode == IDA_MODE_AUTO;
    }
    if (c->type == IDA_CMD_FC) {
        return c->u.fc.op == IDA_FC_ARM || c->u.fc.op == IDA_FC_FIRE || c->u.fc.op == IDA_FC_RESET ||
               c->u.fc.op == IDA_FC_TEST_ENTER;
    }
    return false;
}

static bool daraltma_var(const ida_in_t *in)
{
    if ((in->narrow.mask & IDA_NARROW_DEFINED) != 0u) {
        return true;
    }
    if (in->plat.link_event == IDA_LINKEV_SESSION_END) {
        return true;
    }
    unsigned n = in->n_cmds < IDA_PORT_MAX_CMDS ? in->n_cmds : IDA_PORT_MAX_CMDS;
    for (unsigned j = 0; j < n; ++j) {
        if (kc_daraltici_komut(&in->cmds[j])) {
            return true;
        }
    }
    return false;
}

int kc_kapanma_sayisi(const ida_out_t *o, uint8_t tube)
{
    int n = 0;
    for (unsigned i = 0; i < o->n_events && i < IDA_PORT_MAX_EVENTS; ++i) {
        if (o->events[i].kind == IDA_EVT_SAFETY_CLOSE && o->events[i].tube == tube) {
            ++n;
        }
    }
    return n;
}

int kc_gunluk_say(const ida_out_t *o, uint16_t code)
{
    int n = 0;
    for (unsigned i = 0; i < o->n_logs && i < IDA_PORT_MAX_LOGS; ++i) {
        if (o->logs[i].code == code) {
            ++n;
        }
    }
    return n;
}

const ida_log_t *kc_gunluk_bul(const ida_out_t *o, uint16_t code)
{
    for (unsigned i = 0; i < o->n_logs && i < IDA_PORT_MAX_LOGS; ++i) {
        if (o->logs[i].code == code) {
            return &o->logs[i];
        }
    }
    return NULL;
}

/* ---- Alan alan serileştirme (dolgu baytları girmez; A4-K24, A4-K27) ---------------- */

typedef struct {
    uint8_t *p;
    size_t n;
    size_t cap;
} yazici_t;

static void yb(yazici_t *y, const void *v, size_t n)
{
    if (y->n + n <= y->cap) {
        memcpy(y->p + y->n, v, n);
    }
    y->n += n;
}

#define YA(y, alan) yb((y), &(alan), sizeof(alan))

size_t kc_serilestir(const ida_out_t *o, uint8_t *buf, size_t cap)
{
    yazici_t y;
    y.p = buf;
    y.n = 0;
    y.cap = cap;
    YA(&y, o->thr_valid);
    YA(&y, o->is_safe);
    YA(&y, o->armed);
    YA(&y, o->src);
    YA(&y, o->fs_level);
    YA(&y, o->seq);
    YA(&y, o->t_ref.ns);
    YA(&y, o->valid_for_ns);
    YA(&y, o->thr_cmd);
    YA(&y, o->n_events);
    for (unsigned i = 0; i < IDA_PORT_MAX_EVENTS; ++i) {
        const ida_event_t *e = &o->events[i];
        YA(&y, e->event_id);
        YA(&y, e->kind);
        YA(&y, e->tube);
        YA(&y, e->seq);
        YA(&y, e->t_decide.ns);
        YA(&y, e->apply_by_ns);
        YA(&y, e->hold_ns);
    }
    YA(&y, o->n_results);
    for (unsigned i = 0; i < IDA_PORT_MAX_RESULTS; ++i) {
        const ida_cmd_result_t *r = &o->results[i];
        uint8_t blob_var = (uint8_t)(r->save.blob != NULL ? 1u : 0u);
        uint32_t ham = kc_param_ham(&r->value);
        YA(&y, r->cmd_id);
        YA(&y, r->kind);
        YA(&y, r->ack);
        YA(&y, r->reason);
        YA(&y, r->value.id);
        YA(&y, r->value.type);
        YA(&y, ham);
        YA(&y, r->save.gen);
        YA(&y, blob_var);
        YA(&y, r->save.len);
        /* İşaretçinin değeri değil gösterdiği içerik karşılaştırılır (A4-K24). */
        if (blob_var != 0u && r->kind == IDA_RES_PARAM_SAVE && r->ack == IDA_ACK_ACCEPTED &&
            r->save.len <= IDA_PARAM_BLOB_MAX && (unsigned)i < o->n_results) {
            yb(&y, r->save.blob, r->save.len);
        }
    }
    YA(&y, o->nav_valid);
    YA(&y, o->nav.t.ns);
    YA(&y, o->nav.p_ned);
    YA(&y, o->nav.v_ned);
    YA(&y, o->nav.q_nb);
    YA(&y, o->nav.omega_b);
    YA(&y, o->nav.depth);
    YA(&y, o->nav.altitude);
    YA(&y, o->nav.p_sigma);
    YA(&y, o->nav.v_sigma);
    YA(&y, o->nav.att_sigma);
    YA(&y, o->nav.origin_lat_e7);
    YA(&y, o->nav.origin_lon_e7);
    YA(&y, o->nav.est_level);
    YA(&y, o->nav.reset_counter);
    YA(&y, o->status.mode);
    YA(&y, o->status.armed);
    YA(&y, o->status.fs_level);
    YA(&y, o->status.fs_reasons);
    YA(&y, o->status.regime);
    YA(&y, o->status.phase);
    YA(&y, o->status.mission_state);
    YA(&y, o->status.mission_item);
    YA(&y, o->status.param_ok);
    YA(&y, o->status.events_ok);
    YA(&y, o->status.boot_count);
    YA(&y, o->status.cnt); /* yalnız uint32_t alanlar: dolgu yok */
    YA(&y, o->health);     /* yalnız bool alanlar: dolgu yok */
    YA(&y, o->n_logs);
    for (unsigned i = 0; i < IDA_PORT_MAX_LOGS; ++i) {
        const ida_log_t *l = &o->logs[i];
        YA(&y, l->t_w.ns);
        YA(&y, l->code);
        YA(&y, l->severity);
        YA(&y, l->arg0);
        YA(&y, l->arg1);
    }
    YA(&y, o->fc.state);
    YA(&y, o->fc.inhibit_mask);
    YA(&y, o->fc.last_event_id);
    YA(&y, o->fc.last_event_result);
    YA(&y, o->n_det_tx);
    YA(&y, o->det_tx); /* A8 başlığı dolgusuzluğu kendisi denetler */
    YA(&y, o->bg.submitted);
    YA(&y, o->bg.dropped);
    return y.n;
}

static uint8_t g_kars_a[KC_SER_AZAMI];
static uint8_t g_kars_b[KC_SER_AZAMI];

bool kc_out_esit(const ida_out_t *a, const ida_out_t *b)
{
    size_t na = kc_serilestir(a, g_kars_a, sizeof g_kars_a);
    size_t nb = kc_serilestir(b, g_kars_b, sizeof g_kars_b);
    return na == nb && na <= sizeof g_kars_a && memcmp(g_kars_a, g_kars_b, na) == 0;
}

static size_t ilk_fark(const uint8_t *a, const uint8_t *b, size_t n)
{
    for (size_t i = 0; i < n; ++i) {
        if (a[i] != b[i]) {
            return i;
        }
    }
    return n;
}

/* ---- A4-K126: NOT_INIT çıktısının sabit içeriği ----------------------------------- */

void kc_not_init_beklenen(const ida_in_t *in, uint8_t src, ida_out_t *b)
{
    memset(b, 0, sizeof *b);
    b->thr_valid = false;
    b->is_safe = true;
    b->armed = false;
    b->src = src;
    b->fs_level = (uint8_t)IDA_FS_DISARM;
    b->status.mode = (uint8_t)IDA_MODE_INIT;
    b->status.fs_level = (uint8_t)IDA_FS_DISARM;
    b->status.mission_state = (uint8_t)IDA_MS_IDLE;
    b->health.core_ok = true;
    b->fc.state = (uint8_t)IDA_FCS_SAFE;
    b->fc.inhibit_mask = UINT32_C(0x00026FFF);
    /* A4-K114: mandal bitleri ve (n_cmds ≤ 8 ise) komutlar REJECTED / NOT_INIT. */
    unsigned k = 0;
    for (unsigned i = 0; i < IDA_PORT_N_NARROW; ++i) {
        if ((in->narrow.mask & (1u << i)) != 0u) {
            ida_cmd_result_t *r = &b->results[k++];
            r->cmd_id = in->narrow.cmd_id[i];
            r->kind = (uint8_t)IDA_RES_ACK;
            r->ack = (uint8_t)IDA_ACK_REJECTED;
            r->reason = (uint16_t)IDA_REASON_NOT_INIT;
        }
    }
    if (in->n_cmds <= IDA_PORT_MAX_CMDS) {
        for (unsigned j = 0; j < in->n_cmds; ++j) {
            const ida_cmd_t *c = &in->cmds[j];
            ida_cmd_result_t *r = &b->results[k++];
            r->cmd_id = c->cmd_id;
            r->kind = kc_sonuc_turu(c->type);
            r->ack = (uint8_t)IDA_ACK_REJECTED;
            r->reason = (uint16_t)IDA_REASON_NOT_INIT;
            if (r->kind == IDA_RES_PARAM_VALUE) {
                r->value.id = c->u.param.id; /* 3.14: type ve v 0 */
            }
        }
    }
    b->n_results = (uint8_t)k;
}

/* ---- Günlük önemleri (3.15; kapalı tablo) ----------------------------------------- */

static int beklenen_onem(const ida_log_t *l)
{
    switch (l->code) {
    case IDA_LOG_RESET_INFO:
        return (int)IDA_SEV_NOTICE;
    case IDA_LOG_PARAM_LOAD:
        return l->arg0 == (int32_t)IDA_RC_PARAM_DEFAULTS ? (int)IDA_SEV_WARNING : (int)IDA_SEV_INFO;
    case IDA_LOG_PARAM_COMMIT:
        return l->arg0 == 1 ? (int)IDA_SEV_INFO : (int)IDA_SEV_WARNING;
    case IDA_LOG_PARAM_SAVE:
        return l->arg1 == (int32_t)IDA_PSTORE_FAIL ? (int)IDA_SEV_WARNING : (int)IDA_SEV_INFO;
    case IDA_LOG_MODE_CHANGE:
        return (int)IDA_SEV_INFO;
    case IDA_LOG_ARM_CHANGE:
        return (int)IDA_SEV_NOTICE;
    case IDA_LOG_FAILSAFE_CHANGE:
        return l->arg0 >= (int32_t)IDA_FS_SURFACE ? (int)IDA_SEV_CRITICAL : (int)IDA_SEV_WARNING;
    case IDA_LOG_SESSION:
        return (int)IDA_SEV_NOTICE;
    case IDA_LOG_IMU_GAP:
        return (int)IDA_SEV_WARNING;
    case IDA_LOG_IMU_LAG:
        return l->arg0 == 1 ? (int)IDA_SEV_WARNING : (int)IDA_SEV_INFO;
    case IDA_LOG_IMU_STALE:
        return l->arg0 == 1 ? (int)IDA_SEV_CRITICAL : (int)IDA_SEV_INFO;
    case IDA_LOG_WALL_BACKSTEP:
    case IDA_LOG_BG_SLICE_MISMATCH:
    case IDA_LOG_BG_BAD_INPUT:
    case IDA_LOG_EVENT_UNKNOWN:
        return (int)IDA_SEV_ERROR;
    case IDA_LOG_SAMPLE_CONFLICT:
        return (int)IDA_SEV_WARNING;
    case IDA_LOG_DET_STREAM:
        return l->arg0 == 1 ? (int)IDA_SEV_WARNING : (int)IDA_SEV_INFO;
    case IDA_LOG_EVENT_RESULT:
        return (l->arg1 == (int32_t)IDA_EVR_APPLIED || l->arg1 == (int32_t)IDA_EVR_DUPLICATE)
                   ? (int)IDA_SEV_INFO
                   : (int)IDA_SEV_WARNING;
    case IDA_LOG_SAFETY_INPUT:
        return (int)IDA_SEV_NOTICE;
    case IDA_LOG_FC_STATE:
        return l->arg0 == (int32_t)IDA_FCS_FAULT_LATCHED ? (int)IDA_SEV_ERROR : (int)IDA_SEV_INFO;
    case IDA_LOG_FC_INHIBIT:
        return (int)IDA_SEV_DEBUG;
    case IDA_LOG_NUMERIC_FAULT:
    case IDA_LOG_CORE_INTEGRITY:
        return (int)IDA_SEV_CRITICAL;
    default:
        return -1;
    }
}

/* ---- Genel değişmezler ------------------------------------------------------------ */

typedef struct {
    kc_islev_t f;
    const ida_in_t *in;
    ida_rc_t rc;
    bool kabul;        /* OK ya da IMU_REJECTED */
    bool daraltma;     /* (b) adımında açık daraltıcı girdi (A4-K45 son cümlesi) */
    uint32_t beklenen_seq;
    int64_t beklenen_t;
    bool now_gecerli;
    int64_t kullanilan_now;
    bool uyusmazlik;   /* bg_slices karşılaştırması tutmadı (A4-K109) */
    uint32_t cekirdek_bg;
    bool ilk_yazilabilir;
    bool oturum_sonu_kapanma; /* bu çağrının SESSION_END'i yeni kapanma üretir (A4-K73) */
} baglam_t;

static bool olay_sifir(const ida_event_t *e)
{
    return e->event_id == 0u && e->kind == 0u && e->tube == 0u && e->seq == 0u && e->t_decide.ns == 0 &&
           e->apply_by_ns == 0u && e->hold_ns == 0u;
}

static bool deger_sifir(const ida_param_value_t *v)
{
    return v->id == 0u && v->type == 0u && kc_param_ham(v) == 0u;
}

static bool kayit_sifir(const ida_param_save_t *s)
{
    return s->gen == 0u && s->blob == NULL && s->len == 0u;
}

static bool sonuc_sifir(const ida_cmd_result_t *r)
{
    return r->cmd_id == 0u && r->kind == 0u && r->ack == 0u && r->reason == 0u && deger_sifir(&r->value) &&
           kayit_sifir(&r->save);
}

static bool gunluk_sifir(const ida_log_t *l)
{
    return l->t_w.ns == 0 && l->code == 0u && l->severity == 0u && l->arg0 == 0 && l->arg1 == 0;
}

static bool nav_sifir(const ida_nav_out_t *n)
{
    ida_nav_out_t z;
    memset(&z, 0, sizeof z);
    return n->t.ns == 0 && memcmp(n->p_ned, z.p_ned, sizeof z.p_ned) == 0 &&
           memcmp(n->v_ned, z.v_ned, sizeof z.v_ned) == 0 && memcmp(n->q_nb, z.q_nb, sizeof z.q_nb) == 0 &&
           memcmp(n->omega_b, z.omega_b, sizeof z.omega_b) == 0 && sifir_mi(&n->depth, sizeof n->depth) &&
           sifir_mi(&n->altitude, sizeof n->altitude) && memcmp(n->p_sigma, z.p_sigma, sizeof z.p_sigma) == 0 &&
           memcmp(n->v_sigma, z.v_sigma, sizeof z.v_sigma) == 0 &&
           memcmp(n->att_sigma, z.att_sigma, sizeof z.att_sigma) == 0 && n->origin_lat_e7 == 0 &&
           n->origin_lon_e7 == 0 && n->est_level == 0u && n->reset_counter == 0u;
}

static void bool_denetle(const void *p, const char *ad)
{
    uint8_t b = bayt_oku(p);
    KC_DOGRULA(b == 0u || b == 1u, "bool alan %s = %u (yazılmamış ya da geçersiz; A4-K27)", ad, (unsigned)b);
}

static void sonuclari_denetle(const baglam_t *b, const ida_out_t *o)
{
    const ida_in_t *in = b->in;
    /* (b)'de uygulanan daraltma: açık girdiler ya da A4-K111 kapanması (health.narrowed;
       ters yönü genel_denetim kapanma olayıyla sınar). */
    bool dar = b->daraltma || o->health.narrowed;
    unsigned nlatch = (unsigned)kc_bit_sayisi((uint32_t)(in->narrow.mask & IDA_NARROW_DEFINED));
    unsigned ncmd = in->n_cmds <= IDA_PORT_MAX_CMDS ? in->n_cmds : 0u;
    KC_DOGRULA(o->n_results == nlatch + ncmd,
               "n_results = %u, beklenen %u (kurulu mandal biti %u + komut %u; A4-K43, A4-K114)",
               (unsigned)o->n_results, nlatch + ncmd, nlatch, ncmd);
    unsigned k = 0;
    for (unsigned i = 0; i < IDA_PORT_N_NARROW; ++i) {
        if ((in->narrow.mask & (1u << i)) == 0u) {
            continue;
        }
        const ida_cmd_result_t *r = &o->results[k];
        KC_DOGRULA(r->cmd_id == in->narrow.cmd_id[i] && r->kind == IDA_RES_ACK,
                   "mandal biti %u sonucu: cmd_id %lu kind %u (beklenen %lu, ACK; A4-K43)", i,
                   (unsigned long)r->cmd_id, (unsigned)r->kind, (unsigned long)in->narrow.cmd_id[i]);
        KC_DOGRULA(r->ack == IDA_ACK_ACCEPTED, "mandal biti %u ack %u, ACCEPTED bekleniyordu (A4-K114, K46)", i,
                   (unsigned)r->ack);
        KC_DOGRULA(r->reason == IDA_REASON_NONE, "mandal biti %u ACCEPTED, reason %u (NONE olmalı; A4-K120)", i,
                   (unsigned)r->reason);
        KC_DOGRULA(deger_sifir(&r->value) && kayit_sifir(&r->save),
                   "mandal biti sonucunda value/save sıfır değil (A4-K121)");
        ++k;
    }
    for (unsigned j = 0; j < ncmd; ++j, ++k) {
        const ida_cmd_t *c = &in->cmds[j];
        const ida_cmd_result_t *r = &o->results[k];
        KC_DOGRULA(r->cmd_id == c->cmd_id, "results[%u].cmd_id %lu, beklenen %lu (A4-K43 sıra)", k,
                   (unsigned long)r->cmd_id, (unsigned long)c->cmd_id);
        KC_DOGRULA(r->kind == kc_sonuc_turu(c->type), "komut türü %u sonucu kind %u, beklenen %u (A4-K43)",
                   (unsigned)c->type, (unsigned)r->kind, (unsigned)kc_sonuc_turu(c->type));
        KC_DOGRULA(r->ack >= IDA_ACK_ACCEPTED && r->ack <= IDA_ACK_UNSUPPORTED, "ack %u tanımsız",
                   (unsigned)r->ack);
        KC_DOGRULA(r->reason <= IDA_REASON_CORE_LAST, "reason %u > 15 (A4-K120)", (unsigned)r->reason);
        /* A4-K120: çekirdeğin REJECTED dışındaki her sonucunda (ACCEPTED, DUPLICATE,
           IN_PROGRESS, UNSUPPORTED) reason = NONE. */
        if (r->ack != IDA_ACK_REJECTED) {
            KC_DOGRULA(r->reason == IDA_REASON_NONE, "komut (tür %u) ack %u, reason %u (NONE olmalı; A4-K120, B9)",
                       (unsigned)c->type, (unsigned)r->ack, (unsigned)r->reason);
        }
        if (kc_daraltici_komut(c)) {
            KC_DOGRULA(r->ack == IDA_ACK_ACCEPTED,
                       "cmds[]'teki daraltıcı komut (tür %u) ack %u, ACCEPTED bekleniyordu (A4-K46, K114)",
                       (unsigned)c->type, (unsigned)r->ack);
        } else if (!b->kabul) {
            KC_DOGRULA(r->ack == IDA_ACK_REJECTED && r->reason == IDA_REASON_CALL_REJECTED,
                       "reddedilen çağrıda komut (tür %u) %u/%u, REJECTED/CALL_REJECTED bekleniyordu (A4-K114)",
                       (unsigned)c->type, (unsigned)r->ack, (unsigned)r->reason);
        } else {
            KC_DOGRULA(r->reason != IDA_REASON_CALL_REJECTED && r->reason != IDA_REASON_NOT_INIT,
                       "kabul edilen çağrıda reason %u (A4-K114)", (unsigned)r->reason);
            if (c->type == 0u || c->type > IDA_CMD_FC) {
                KC_DOGRULA(r->ack == IDA_ACK_DUPLICATE ||
                               (r->ack == IDA_ACK_UNSUPPORTED && r->reason == IDA_REASON_NONE),
                           "bilinmeyen tür %u: %u/%u, UNSUPPORTED/NONE bekleniyordu (3.17)", (unsigned)c->type,
                           (unsigned)r->ack, (unsigned)r->reason);
            }
            if (kc_genisletici_komut(c)) {
                KC_DOGRULA(r->ack != IDA_ACK_ACCEPTED && r->ack != IDA_ACK_IN_PROGRESS,
                           "genişletici (tür %u, fc.op %u) kabul edildi: ack %u (yer tutucu; A4-K124, M4)",
                           (unsigned)c->type, (unsigned)c->u.fc.op, (unsigned)r->ack);
                if (r->ack != IDA_ACK_DUPLICATE) {
                    if (dar) {
                        KC_DOGRULA(r->ack == IDA_ACK_REJECTED && r->reason == IDA_REASON_NARROWED,
                                   "daraltmalı çağrıda genişletici (tür %u) %u/%u, REJECTED/NARROWED "
                                   "bekleniyordu (A4-K45; 3.17 (2))",
                                   (unsigned)c->type, (unsigned)r->ack, (unsigned)r->reason);
                    } else if (b->rc == IDA_RC_IMU_REJECTED) {
                        KC_DOGRULA(r->ack == IDA_ACK_REJECTED && r->reason == IDA_REASON_IMU_REJECTED,
                                   "IMU_REJECTED çağrısında genişletici (tür %u) %u/%u, REJECTED/IMU_REJECTED "
                                   "bekleniyordu (A4-K103; 3.17 (2))",
                                   (unsigned)c->type, (unsigned)r->ack, (unsigned)r->reason);
                    } else {
                        KC_DOGRULA(r->reason != IDA_REASON_NARROWED && r->reason != IDA_REASON_IMU_REJECTED,
                                   "daraltmasız çağrıda genişletici reason %u (A4-K45, K103)",
                                   (unsigned)r->reason);
                    }
                }
            } else {
                KC_DOGRULA(r->reason != IDA_REASON_NARROWED && r->reason != IDA_REASON_IMU_LAG &&
                               r->reason != IDA_REASON_IMU_REJECTED,
                           "genişletici olmayan komut (tür %u) reason %u (A4-K45, K103, K108)", (unsigned)c->type,
                           (unsigned)r->reason);
            }
        }
        if (r->kind == IDA_RES_ACK) {
            KC_DOGRULA(deger_sifir(&r->value) && kayit_sifir(&r->save), "ACK sonucunda value/save sıfır değil (3.14)");
        } else if (r->kind == IDA_RES_PARAM_VALUE) {
            KC_DOGRULA(kayit_sifir(&r->save), "PARAM_VALUE sonucunda save sıfır değil (3.14)");
            KC_DOGRULA(r->value.id == c->u.param.id, "PARAM_VALUE value.id %u, komutun param.id %u (3.14)",
                       (unsigned)r->value.id, (unsigned)c->u.param.id);
            if (r->ack != IDA_ACK_ACCEPTED) {
                KC_DOGRULA(r->value.type == 0u && kc_param_ham(&r->value) == 0u,
                           "kabul edilmeyen PARAM_VALUE sonucunda type/v sıfır değil (3.14)");
            }
        } else {
            KC_DOGRULA(deger_sifir(&r->value), "PARAM_SAVE sonucunda value sıfır değil (3.14)");
            if (r->ack != IDA_ACK_ACCEPTED) {
                KC_DOGRULA(kayit_sifir(&r->save), "kabul edilmeyen PARAM_SAVE sonucunda save sıfır değil (3.14)");
            } else {
                KC_DOGRULA(r->save.blob != NULL && r->save.len >= IDA_PBLOB_HDR_SIZE &&
                               r->save.len <= IDA_PARAM_BLOB_MAX,
                           "kabul edilen PARAM_SAVE: blob/len geçersiz (A4-K82)");
            }
        }
    }
    for (unsigned i = o->n_results; i < IDA_PORT_MAX_RESULTS; ++i) {
        KC_DOGRULA(sonuc_sifir(&o->results[i]), "results[%u] (n_results = %u) sıfır değil (A4-K121)", i,
                   (unsigned)o->n_results);
    }
}

static void olaylari_denetle(const baglam_t *b, const ida_out_t *o)
{
    KC_DOGRULA(o->n_events <= IDA_PORT_MAX_EVENTS, "n_events %u", (unsigned)o->n_events);
    if (!M.olay_acik) {
        KC_DOGRULA(o->n_events == 0u, "olay kimliği kullanılamazken %u olay (A4-K61)", (unsigned)o->n_events);
    }
    int tum = 0;
    int t0 = 0;
    int t1 = 0;
    for (unsigned i = 0; i < o->n_events; ++i) {
        const ida_event_t *e = &o->events[i];
        KC_DOGRULA(e->kind != IDA_EVT_SAFETY_OPEN && e->kind != IDA_EVT_FIRE,
                   "genişletici olay üretildi: kind %u (yer tutucu; A4-K28, K64, M4)", (unsigned)e->kind);
        KC_DOGRULA(e->kind == IDA_EVT_SAFETY_CLOSE, "tanımsız olay türü %u", (unsigned)e->kind);
        KC_DOGRULA(e->tube == 0u || e->tube == 1u || e->tube == IDA_TUBE_ALL, "SAFETY_CLOSE tube %u",
                   (unsigned)e->tube);
        KC_DOGRULA(e->seq == o->seq && e->t_decide.ns == o->t_ref.ns,
                   "olay seq/t_decide (%lu, %lld) çıktının seq/t_ref'i (%lu, %lld) değil (A4-K57, K63)",
                   (unsigned long)e->seq, (long long)e->t_decide.ns, (unsigned long)o->seq,
                   (long long)o->t_ref.ns);
        KC_DOGRULA(e->hold_ns == 0u, "SAFETY_CLOSE hold_ns %lu (A4-K66)", (unsigned long)e->hold_ns);
        KC_DOGRULA(e->apply_by_ns >= IDA_APPLY_BY_MIN_NS && e->apply_by_ns <= IDA_APPLY_BY_MAX_NS,
                   "apply_by_ns %lu aralık dışı (A4-K66)", (unsigned long)e->apply_by_ns);
        KC_DOGRULA((uint32_t)(e->event_id >> 32) == M.boot_count, "event_id üst 32 biti %lu, boot_count %lu (A4-K61)",
                   (unsigned long)(e->event_id >> 32), (unsigned long)M.boot_count);
        uint32_t ctr = (uint32_t)(e->event_id & 0xFFFFFFFFu);
        KC_DOGRULA(ctr >= IDA_EVT_CTR_FIRST && (!M.olay_var || ctr > M.son_ctr),
                   "olay sayacı %lu artmıyor (önceki %lu; A4-K61, K63)", (unsigned long)ctr,
                   (unsigned long)M.son_ctr);
        M.olay_var = true;
        M.son_ctr = ctr;
        M.son_id = e->event_id;
        if (e->tube == IDA_TUBE_ALL) {
            ++tum;
        } else if (e->tube == 0u) {
            ++t0;
        } else {
            ++t1;
        }
    }
    KC_DOGRULA(tum <= 1 && t0 <= 1 && t1 <= 1, "aynı kapsamda birden çok SAFETY_CLOSE (A4-K125)");
    if (tum == 1) {
        KC_DOGRULA(o->n_events == 1u, "ALL kapanmasıyla birlikte %u olay (A4-K125)", (unsigned)o->n_events);
    }
    for (unsigned i = o->n_events; i < IDA_PORT_MAX_EVENTS; ++i) {
        KC_DOGRULA(olay_sifir(&o->events[i]), "events[%u] (n_events %u) sıfır değil (A4-K121)", i,
                   (unsigned)o->n_events);
    }
    /* A4-K73: SESSION_END kapanması yalnız son SESSION_END'den beri SESSION_START geldiyse
       (ya da init'ten beri ilk SESSION_END ise) üretilir. */
    bool oturum_sonu = b->oturum_sonu_kapanma;
    if (M.olay_acik && (oturum_sonu || M.bekleyen_kapanma)) {
        KC_DOGRULA(o->n_events == 1u && o->events[0].kind == IDA_EVT_SAFETY_CLOSE &&
                       o->events[0].tube == IDA_TUBE_ALL,
                   "SESSION_END (bu çağrıda %d, bekletilen %d): tek SAFETY_CLOSE(ALL) bekleniyordu, %u olay "
                   "(A4-K73, K125, 3.11 (b2))",
                   (int)oturum_sonu, (int)M.bekleyen_kapanma, (unsigned)o->n_events);
    }
    KC_DOGRULA(o->fc.last_event_id == (M.olay_var ? M.son_id : 0u),
               "fc.last_event_id %llx, son çıkan olay %llx (A4-K91)", (unsigned long long)o->fc.last_event_id,
               (unsigned long long)(M.olay_var ? M.son_id : 0u));
}

static void gunlukleri_denetle(const baglam_t *b, const ida_out_t *o, uint32_t dusen)
{
    KC_DOGRULA(o->n_logs <= IDA_PORT_MAX_LOGS, "n_logs %u", (unsigned)o->n_logs);
    if (dusen > 0u) {
        KC_DOGRULA(o->n_logs == IDA_PORT_MAX_LOGS, "log_dropped arttığı halde n_logs %u (A4-K90)",
                   (unsigned)o->n_logs);
    }
    for (unsigned i = 0; i < o->n_logs; ++i) {
        const ida_log_t *l = &o->logs[i];
        KC_DOGRULA(l->code >= 1u && l->code <= IDA_LOG_CODE_LAST, "günlük kodu %u kapalı tabloda yok (A4-K90)",
                   (unsigned)l->code);
        KC_DOGRULA((int)l->severity == beklenen_onem(l), "günlük %u (arg0 %ld, arg1 %ld) önemi %u, beklenen %d (3.15)",
                   (unsigned)l->code, (long)l->arg0, (long)l->arg1, (unsigned)l->severity, beklenen_onem(l));
        KC_DOGRULA(l->t_w.ns >= 0 && l->t_w.ns <= M.now_ust, "günlük t_w %lld aralık dışı",
                   (long long)l->t_w.ns);
        /* 3.8 t_w: kaydın üretildiği çağrının kullanılan now_w'si; (b2) ile bekletilende o out
           NULL çağrısınınki; RESET_INFO ve PARAM_LOAD'da init'in now_w'si. */
        if (l->code == IDA_LOG_RESET_INFO || l->code == IDA_LOG_PARAM_LOAD) {
            KC_DOGRULA(l->t_w.ns == M.init_now, "günlük %u t_w %lld, init'in now_w'si %lld (3.8)", (unsigned)l->code,
                       (long long)l->t_w.ns, (long long)M.init_now);
        } else if (!M.geri_adim_log_belirsiz) {
            bool uygun = b->now_gecerli ? l->t_w.ns == b->kullanilan_now : l->t_w.ns == M.now_azami;
            for (int k = 0; k < M.n_out_null_now && !uygun; ++k) {
                uygun = l->t_w.ns == M.out_null_now[k];
            }
            /* Aralık dışı now_w taşıyan çağrıda kullanılan now_w geri adım izinin değeridir
               (A4-K10; eski belirsiz R2). */
            KC_DOGRULA(uygun,
                       "günlük %u t_w %lld: bu çağrının kullanılan now_w'si (%lld) ya da bekletildiği out NULL "
                       "çağrısınınki değil (3.8)",
                       (unsigned)l->code, (long long)l->t_w.ns, (long long)b->kullanilan_now);
        }
    }
    for (unsigned i = o->n_logs; i < IDA_PORT_MAX_LOGS; ++i) {
        KC_DOGRULA(gunluk_sifir(&o->logs[i]), "logs[%u] (n_logs %u) sıfır değil (A4-K121)", i, (unsigned)o->n_logs);
    }
    if (dusen != 0u) {
        return;
    }
    /* Kayıt düşmediyse kural gereği üretilmesi gereken kayıtlar (3.15, A4-K90). */
    int n_reset = kc_gunluk_say(o, (uint16_t)IDA_LOG_RESET_INFO);
    int n_pload = kc_gunluk_say(o, (uint16_t)IDA_LOG_PARAM_LOAD);
    if (b->ilk_yazilabilir) {
        const ida_log_t *r = kc_gunluk_bul(o, (uint16_t)IDA_LOG_RESET_INFO);
        const ida_log_t *p = kc_gunluk_bul(o, (uint16_t)IDA_LOG_PARAM_LOAD);
        KC_DOGRULA(n_reset == 1 && n_pload == 1 && r != NULL && p != NULL,
                   "init sonrası ilk yazılabilir çağrıda RESET_INFO %d, PARAM_LOAD %d (3.15)", n_reset, n_pload);
        KC_DOGRULA(r->arg0 == (int32_t)M.reset_cause && r->arg1 == (int32_t)M.consecutive_faults,
                   "RESET_INFO arg0/arg1 %ld/%ld, beklenen %u/%u (3.15)", (long)r->arg0, (long)r->arg1,
                   (unsigned)M.reset_cause, (unsigned)M.consecutive_faults);
        KC_DOGRULA(p->arg0 == (int32_t)M.init_rc, "PARAM_LOAD arg0 %ld, init dönüşü %d (3.15)", (long)p->arg0,
                   (int)M.init_rc);
        if (M.param_load_arg1 >= 0) {
            KC_DOGRULA(p->arg1 == M.param_load_arg1, "PARAM_LOAD arg1 %ld, beklenen denetim numarası %ld (A4-K80)",
                       (long)p->arg1, (long)M.param_load_arg1);
        }
        KC_DOGRULA(kc_gunluk_say(o, (uint16_t)IDA_LOG_FC_STATE) == 0 &&
                       kc_gunluk_say(o, (uint16_t)IDA_LOG_FC_INHIBIT) == 0,
                   "init sonrası ilk yazılabilir çağrıda FC_STATE/FC_INHIBIT (A4-K90 taban)");
    } else {
        KC_DOGRULA(n_reset == 0 && n_pload == 0, "RESET_INFO/PARAM_LOAD ilk yazılabilir çağrıdan sonra yinelendi");
        const ida_out_t *p = &M.son; /* bir önceki yazılabilir çıktı */
        int n_fs = kc_gunluk_say(o, (uint16_t)IDA_LOG_FC_STATE);
        int n_fi = kc_gunluk_say(o, (uint16_t)IDA_LOG_FC_INHIBIT);
        if (o->fc.inhibit_mask != p->fc.inhibit_mask) {
            const ida_log_t *l = kc_gunluk_bul(o, (uint16_t)IDA_LOG_FC_INHIBIT);
            KC_DOGRULA(n_fi == 1 && l != NULL && (uint32_t)l->arg0 == o->fc.inhibit_mask &&
                           (uint32_t)l->arg1 == p->fc.inhibit_mask,
                       "inhibit_mask %lx -> %lx değişti; FC_INHIBIT(yeni, önceki) bekleniyordu (A4-K90)",
                       (unsigned long)p->fc.inhibit_mask, (unsigned long)o->fc.inhibit_mask);
        } else {
            KC_DOGRULA(n_fi == 0, "inhibit_mask değişmediği halde FC_INHIBIT (A4-K90)");
        }
        if (o->fc.state != p->fc.state) {
            const ida_log_t *l = kc_gunluk_bul(o, (uint16_t)IDA_LOG_FC_STATE);
            KC_DOGRULA(n_fs == 1 && l != NULL && l->arg0 == (int32_t)o->fc.state && l->arg1 == (int32_t)p->fc.state,
                       "fc.state değişti; FC_STATE(yeni, önceki) bekleniyordu (A4-K90)");
        } else {
            KC_DOGRULA(n_fs == 0, "fc.state değişmediği halde FC_STATE (A4-K90)");
        }
    }
    if (!M.oturum_belirsiz) {
        KC_DOGRULA(kc_gunluk_say(o, (uint16_t)IDA_LOG_SESSION) == (int)M.oturum_olayi,
                   "SESSION günlüğü %d, link_event ≠ NONE çağrı sayısı %lu (3.15, 3.11 (b2))",
                   kc_gunluk_say(o, (uint16_t)IDA_LOG_SESSION), (unsigned long)M.oturum_olayi);
    }
    if (!M.geri_adim_log_belirsiz) {
        KC_DOGRULA(kc_gunluk_say(o, (uint16_t)IDA_LOG_WALL_BACKSTEP) == (int)M.geri_adim_cagri,
                   "WALL_BACKSTEP günlüğü %d, geri adımlı çağrı %lu (A4-K10, 3.11 (b2))",
                   kc_gunluk_say(o, (uint16_t)IDA_LOG_WALL_BACKSTEP), (unsigned long)M.geri_adim_cagri);
    }
    {
        const ida_log_t *l = kc_gunluk_bul(o, (uint16_t)IDA_LOG_BG_BAD_INPUT);
        if (M.bg_bad_sayi > 0u) {
            KC_DOGRULA(kc_gunluk_say(o, (uint16_t)IDA_LOG_BG_BAD_INPUT) == 1 && l != NULL &&
                           l->arg0 == (int32_t)M.bg_bad_sayi,
                       "BG_BAD_INPUT günlüğü (arg0 = %lu) bekleniyordu (A4-K117, 3.15)",
                       (unsigned long)M.bg_bad_sayi);
        } else {
            KC_DOGRULA(l == NULL, "BAD_INPUT dönen arka plan çağrısı yokken BG_BAD_INPUT günlüğü");
        }
    }
    {
        const ida_log_t *l = kc_gunluk_bul(o, (uint16_t)IDA_LOG_BG_SLICE_MISMATCH);
        if (b->uyusmazlik) {
            KC_DOGRULA(kc_gunluk_say(o, (uint16_t)IDA_LOG_BG_SLICE_MISMATCH) == 1 && l != NULL &&
                           l->arg0 == (int32_t)b->in->bg_slices && l->arg1 == (int32_t)b->cekirdek_bg,
                       "BG_SLICE_MISMATCH(%lu, %lu) bekleniyordu (A4-K109, 3.15)",
                       (unsigned long)b->in->bg_slices, (unsigned long)b->cekirdek_bg);
        } else {
            KC_DOGRULA(l == NULL, "bg_slices tutarken BG_SLICE_MISMATCH günlüğü (A4-K109)");
        }
    }
}

/* Sayaç farkı mod 2^32 (A4-K88: sayaçlar 2^32'de sarar). Üyeye offsetof + bayt işaretçisi +
   memcpy ile erişilir; yapı dizi gibi dolaşılmaz (sol bulgu 4). */
uint32_t kc_sayac_farki(const ida_core_counters_t *yeni, const ida_core_counters_t *eski, size_t konum)
{
    uint32_t y;
    uint32_t e;
    memcpy(&y, (const unsigned char *)yeni + konum, sizeof y);
    memcpy(&e, (const unsigned char *)eski + konum, sizeof e);
    return (uint32_t)(y - e);
}

static void sayaclari_denetle(const baglam_t *b, const ida_out_t *o)
{
    /* Genel "yeni >= eski" koşulu yok: sarma A4-K88'de izinlidir; beklenen artışlar mod 2^32
       farkla denetlenir (sol bulgu 5). */
    const ida_core_counters_t *c = &o->status.cnt;
    const ida_core_counters_t *p = &M.onceki_cnt;
    unsigned n_red = 0;
    for (unsigned i = 0; i < o->n_results && i < IDA_PORT_MAX_RESULTS; ++i) {
        if (o->results[i].ack == IDA_ACK_REJECTED) {
            ++n_red;
        }
    }
    KC_DOGRULA(kc_sayac_farki(c, p, offsetof(ida_core_counters_t, cmd_rejected)) == n_red,
               "cnt.cmd_rejected farkı %lu, çıktıdaki REJECTED sonuç %u (3.8)",
               (unsigned long)kc_sayac_farki(c, p, offsetof(ida_core_counters_t, cmd_rejected)), n_red);
    KC_DOGRULA(c->bad_input - p->bad_input == (b->rc == IDA_RC_BAD_INPUT ? 1u : 0u),
               "cnt.bad_input farkı %lu (A4-K88; (b2) saymaz)", (unsigned long)(c->bad_input - p->bad_input));
    KC_DOGRULA(c->dup_seq - p->dup_seq == (b->rc == IDA_RC_DUP_SEQ ? 1u : 0u), "cnt.dup_seq farkı %lu",
               (unsigned long)(c->dup_seq - p->dup_seq));
    KC_DOGRULA(c->old_seq - p->old_seq == (b->rc == IDA_RC_OLD_SEQ ? 1u : 0u), "cnt.old_seq farkı %lu",
               (unsigned long)(c->old_seq - p->old_seq));
    KC_DOGRULA(c->imu_rejected - p->imu_rejected == (b->rc == IDA_RC_IMU_REJECTED ? 1u : 0u),
               "cnt.imu_rejected farkı %lu (A4-K103)", (unsigned long)(c->imu_rejected - p->imu_rejected));
    if (!M.geri_adim_belirsiz) {
        KC_DOGRULA(c->wall_backstep - p->wall_backstep == M.geri_adim,
                   "cnt.wall_backstep farkı %lu, beklenen %lu (A4-K10)",
                   (unsigned long)(c->wall_backstep - p->wall_backstep), (unsigned long)M.geri_adim);
    }
    KC_DOGRULA(c->bg_bad_input - p->bg_bad_input == M.bg_bad_sayi, "cnt.bg_bad_input farkı %lu, beklenen %lu (A4-K117)",
               (unsigned long)(c->bg_bad_input - p->bg_bad_input), (unsigned long)M.bg_bad_sayi);
    KC_DOGRULA(c->bg_slice_mismatch - p->bg_slice_mismatch == (b->uyusmazlik ? 1u : 0u),
               "cnt.bg_slice_mismatch farkı %lu (A4-K109)",
               (unsigned long)(c->bg_slice_mismatch - p->bg_slice_mismatch));
}

static void genel_denetim(const baglam_t *b, const ida_out_t *o)
{
    const ida_in_t *in = b->in;
    bool step_ok = b->f == KC_STEP && b->rc == IDA_RC_OK;
    if (b->f == KC_STEP) {
        KC_DOGRULA(b->rc == IDA_RC_OK || b->rc == IDA_RC_IMU_REJECTED || b->rc == IDA_RC_DUP_SEQ ||
                       b->rc == IDA_RC_OLD_SEQ || b->rc == IDA_RC_BAD_INPUT,
                   "step dönüşü %d 4.15 listesinde değil", (int)b->rc);
    } else {
        KC_DOGRULA(b->rc == IDA_RC_OK || b->rc == IDA_RC_DUP_SEQ || b->rc == IDA_RC_OLD_SEQ ||
                       b->rc == IDA_RC_BAD_INPUT,
                   "tick dönüşü %d 4.15 listesinde değil", (int)b->rc);
    }
    if (!in->tf.present) {
        KC_DOGRULA(b->rc != IDA_RC_DUP_SEQ && b->rc != IDA_RC_OLD_SEQ, "tf.present = 0 iken %d (A4-K25)", (int)b->rc);
    }
    /* bool alanlar */
    bool_denetle(&o->thr_valid, "thr_valid");
    bool_denetle(&o->is_safe, "is_safe");
    bool_denetle(&o->armed, "armed");
    bool_denetle(&o->nav_valid, "nav_valid");
    bool_denetle(&o->status.armed, "status.armed");
    bool_denetle(&o->status.param_ok, "status.param_ok");
    bool_denetle(&o->status.events_ok, "status.events_ok");
    bool_denetle(&o->health.core_ok, "health.core_ok");
    bool_denetle(&o->health.data_ok, "health.data_ok");
    bool_denetle(&o->health.imu_rejected, "health.imu_rejected");
    bool_denetle(&o->health.imu_lag, "health.imu_lag");
    bool_denetle(&o->health.input_dropped, "health.input_dropped");
    bool_denetle(&o->health.narrowed, "health.narrowed");
    bool_denetle(&o->health.sample_conflict, "health.sample_conflict");
    bool_denetle(&o->health.bg_bad_input, "health.bg_bad_input");
    bool_denetle(&o->bg.submitted, "bg.submitted");
    /* Sürekli komut: yer tutucu hiçbir zaman itki üretmez, silahlanmaz (M4, A4-K55). */
    for (unsigned i = 0; i < IDA_PORT_N_THR; ++i) {
        KC_DOGRULA(o->thr_cmd[i] == 0.0f, "thr_cmd[%u] ≠ 0 (yer tutucu; A4-K28, K55, K56)", i);
    }
    KC_DOGRULA(!o->armed && !o->status.armed, "armed = 1: genişletici kabul edilmiş (yer tutucu; A4-K124, M4)");
    KC_DOGRULA(o->is_safe, "is_safe = 0 iken armed = 0 (A4-K55)");
    KC_DOGRULA(o->src == (b->f == KC_STEP ? IDA_SRC_STEP : IDA_SRC_TICK), "src %u (A4-K57)", (unsigned)o->src);
    KC_DOGRULA(o->fs_level <= IDA_FS_DISARM && o->fs_level == o->status.fs_level, "fs_level %u / status %u",
               (unsigned)o->fs_level, (unsigned)o->status.fs_level);
    if (b->kabul) {
        KC_DOGRULA(o->thr_valid, "kabul edilen çağrıda thr_valid = 0 (is_safe = 1; 4.14, A4-K56)");
    } else {
        KC_DOGRULA(!o->thr_valid, "hata dönüşünde thr_valid = 1 (A4-K28)");
    }
    if (o->thr_valid) {
        KC_DOGRULA(o->valid_for_ns >= IDA_VALID_FOR_MIN_NS && o->valid_for_ns <= IDA_VALID_FOR_MAX_NS,
                   "valid_for_ns %lu aralık dışı (A4-K58)", (unsigned long)o->valid_for_ns);
    } else {
        KC_DOGRULA(o->valid_for_ns == 0u, "thr_valid = 0 iken valid_for_ns %lu (A4-K28, K58)",
                   (unsigned long)o->valid_for_ns);
    }
    KC_DOGRULA(o->seq == b->beklenen_seq && o->t_ref.ns == b->beklenen_t,
               "seq/t_ref (%lu, %lld), beklenen (%lu, %lld) (A4-K57, 4.14 son geçerli)", (unsigned long)o->seq,
               (long long)o->t_ref.ns, (unsigned long)b->beklenen_seq, (long long)b->beklenen_t);
    /* Seyrüsefer (A4-K87, K121). */
    KC_DOGRULA(o->nav_valid == step_ok, "nav_valid %d (yalnız step OK'de 1; A4-K87)", (int)o->nav_valid);
    if (o->nav_valid) {
        KC_DOGRULA(o->nav.est_level <= IDA_EST_FAULT, "est_level %u", (unsigned)o->nav.est_level);
        if (o->nav.est_level == IDA_EST_INIT) {
            ida_nav_out_t z = o->nav;
            bool q_birim = z.q_nb[0] == 1.0f && z.q_nb[1] == 0.0f && z.q_nb[2] == 0.0f && z.q_nb[3] == 0.0f;
            z.t.ns = 0;
            memset(z.q_nb, 0, sizeof z.q_nb);
            KC_DOGRULA(q_birim && nav_sifir(&z), "est_level INIT iken nav alanları 0 / q = (1,0,0,0) değil (A4-K87)");
            KC_DOGRULA(o->status.regime == 0u && o->status.phase == 0u, "est_level INIT iken regime/phase ≠ 0 (3.8)");
        }
    } else {
        KC_DOGRULA(nav_sifir(&o->nav), "nav_valid = 0 iken nav sıfır değil (A4-K121)");
    }
    /* Algılama ve arka plan (A4-K92, 4.14). */
    KC_DOGRULA(o->n_det_tx <= IDA_PORT_MAX_DET_OUT, "n_det_tx %u", (unsigned)o->n_det_tx);
    if (!step_ok) {
        KC_DOGRULA(o->n_det_tx == 0u && !o->bg.submitted, "step OK dışında det_tx %u / bg.submitted %d (4.14)",
                   (unsigned)o->n_det_tx, (int)o->bg.submitted);
    }
    for (unsigned i = o->n_det_tx; i < IDA_PORT_MAX_DET_OUT; ++i) {
        KC_DOGRULA(sifir_mi(&o->det_tx[i], sizeof o->det_tx[i]), "det_tx[%u] sıfır değil (A4-K121)", i);
    }
    /* Durum ve sağlık (A4-K88, K89). */
    /* A4-K127: yer tutucu çekirdek INIT'ten çıkmaz. */
    KC_DOGRULA(o->status.mode == IDA_MODE_INIT, "status.mode %u; yer tutucu INIT'ten çıkmaz (A4-K127)",
               (unsigned)o->status.mode);
    KC_DOGRULA(o->status.mission_state <= IDA_MS_ABORTED, "mission_state %u", (unsigned)o->status.mission_state);
    KC_DOGRULA((o->status.fs_reasons & ~UINT32_C(0x7FFF)) == 0u, "fs_reasons tanımsız bit %lx",
               (unsigned long)o->status.fs_reasons);
    /* 3.8: boot_count init'te verilen değerdir, boot_count_ok = 0 iken de aynen taşınır. */
    KC_DOGRULA(o->status.boot_count == M.boot_count, "status.boot_count %lu, init'te %lu (3.8)",
               (unsigned long)o->status.boot_count, (unsigned long)M.boot_count);
    /* 3.8 health.input_dropped: plat.input_dropped'un yansıması, hata dönüşleri dahil. */
    KC_DOGRULA(o->health.input_dropped == b->in->plat.input_dropped, "health.input_dropped %d, plat %d (3.8, A4-K102)",
               (int)o->health.input_dropped, (int)b->in->plat.input_dropped);
    KC_DOGRULA(o->status.events_ok == M.olay_acik, "status.events_ok %d, beklenen %d (A4-K61)",
               (int)o->status.events_ok, (int)M.olay_acik);
    if (M.init_rc == IDA_RC_OK) {
        KC_DOGRULA(o->status.param_ok, "geçerli blokla init sonrası param_ok = 0 (A4-K78)");
    }
    KC_DOGRULA(o->health.core_ok, "core_ok = 0 (A4-K89)");
    KC_DOGRULA(o->health.imu_rejected == (b->rc == IDA_RC_IMU_REJECTED), "health.imu_rejected %d (A4-K89, K103)",
               (int)o->health.imu_rejected);
    if (b->daraltma) {
        KC_DOGRULA(o->health.narrowed, "daraltıcı girdi uygulandı ama health.narrowed = 0 (A4-K45)");
    } else if (o->health.narrowed) {
        KC_DOGRULA(!M.olay_acik || kc_kapanma_sayisi(o, 0u) + kc_kapanma_sayisi(o, 1u) +
                                           kc_kapanma_sayisi(o, (uint8_t)IDA_TUBE_ALL) >
                                       0,
                   "açık daraltıcı girdi ve kapanma olmadan health.narrowed = 1 (A4-K45, K111)");
    }
    KC_DOGRULA(o->health.bg_bad_input == (M.bg_bad_sayi > 0u), "health.bg_bad_input %d, beklenen %d (A4-K117)",
               (int)o->health.bg_bad_input, (int)(M.bg_bad_sayi > 0u));
    /* Atış kontrol durumu (A4-K91; yer tutucuda izin hiç açılmaz). */
    KC_DOGRULA(o->fc.state <= IDA_FCS_FAULT_LATCHED && o->fc.state != IDA_FCS_PERMIT_OPEN &&
                   o->fc.state != IDA_FCS_FIRE_PENDING,
               "fc.state %u (izin açılmış ya da atış bekleniyor; yer tutucu)", (unsigned)o->fc.state);
    KC_DOGRULA((o->fc.inhibit_mask & ~IDA_FCI_DEFINED) == 0u, "inhibit_mask ayrılmış bit %lx (A4-K91)",
               (unsigned long)o->fc.inhibit_mask);
    KC_DOGRULA((o->fc.inhibit_mask & IDA_FCI_NOT_ARMED) != 0u, "silahsızken NOT_ARMED biti yok (3.16)");
    if (!M.olay_acik) {
        KC_DOGRULA((o->fc.inhibit_mask & IDA_FCI_EVENTS_OFF) != 0u, "olay kimliği yokken EVENTS_OFF biti yok (3.16)");
    }
    KC_DOGRULA(o->fc.last_event_result == 0u ||
                   (o->fc.last_event_result >= IDA_EVR_APPLIED && o->fc.last_event_result <= IDA_EVR_REJ_SESSION) ||
                   (o->fc.last_event_result >= IDA_EVR_DROPPED_GATE && o->fc.last_event_result <= IDA_EVR_SESSION_END),
               "last_event_result %u", (unsigned)o->fc.last_event_result);
    sonuclari_denetle(b, o);
    olaylari_denetle(b, o);
    uint32_t dusen = o->status.cnt.log_dropped - M.onceki_cnt.log_dropped;
    gunlukleri_denetle(b, o, dusen);
    sayaclari_denetle(b, o);
}

/* ---- Çağrılar --------------------------------------------------------------------- */

static void desen_doldur(ida_out_t *o, int i, uint64_t cagri)
{
    if (i == KC_N_AZAMI - 1) {
        rastgele_doldur(o, sizeof *o, UINT64_C(0x0B5E0000) ^ cagri);
    } else {
        memset(o, CIKTI_DESEN[i], sizeof *o);
    }
}

static bool zaman_gecerli(int64_t ns)
{
    return ns >= 0 && ns <= IDA_TIME_MAX_NS;
}

static ida_out_t g_beklenen;
static uint8_t g_ser_bek[KC_SER_AZAMI];

static ida_rc_t cagri_yurut(kc_islev_t f, ida_in_t *in, bool out_var)
{
    ida_rc_t rc0 = IDA_RC_OK;
    M.cagri_no++;
    (void)snprintf(g_baglam, sizeof g_baglam, "çağrı #%llu %s%s", (unsigned long long)M.cagri_no,
                   f == KC_STEP ? "step" : "tick", out_var ? "" : " (out NULL)");
    if (M.bg_elle) {
        in->bg_slices = M.bg_elle_deger;
        M.bg_elle = false;
    } else {
        in->bg_slices = M.bg_sayim;
    }
    for (int i = 0; i < M.n; ++i) {
        ida_out_t *o = out_var ? &g_out[i] : NULL;
        ida_rc_t rc;
        if (o != NULL) {
            desen_doldur(o, i, M.cagri_no);
        }
        rc = f == KC_STEP ? ida_core_step(kc_core(i), in, o) : ida_core_tick(kc_core(i), in, o);
        if (i == 0) {
            rc0 = rc;
        } else {
            KC_DOGRULA(rc == rc0, "örnek %d dönüşü %d, örnek 0 dönüşü %d (A4-K24)", i, (int)rc, (int)rc0);
        }
        if (o != NULL) {
            g_ser_n[i] = kc_serilestir(o, g_ser[i], KC_SER_AZAMI);
            KC_DOGRULA(g_ser_n[i] <= KC_SER_AZAMI, "serileştirme tamponu küçük");
        }
    }
    if (out_var) {
        for (int i = 1; i < M.n; ++i) {
            size_t k = ilk_fark(g_ser[0], g_ser[i], g_ser_n[0] < g_ser_n[i] ? g_ser_n[0] : g_ser_n[i]);
            KC_DOGRULA(g_ser_n[0] == g_ser_n[i] && k == g_ser_n[0],
                       "örnek %d çıktısı örnek 0'dan farklı (serileştirilmiş bayt %lu): önceki içerik ya da "
                       "örnek belleği çıktıya sızdı ya da çekirdek belirlenimci değil (A4-K24, K27)",
                       i, (unsigned long)k);
        }
    }
    if (!M.kurulu) {
        KC_DOGRULA(rc0 == IDA_RC_NOT_INIT, "kurulmamış örnekte dönüş %d, NOT_INIT bekleniyordu (3.11 (a2))", (int)rc0);
        if (out_var) {
            kc_not_init_beklenen(in, (uint8_t)(f == KC_STEP ? IDA_SRC_STEP : IDA_SRC_TICK), &g_beklenen);
            size_t nb = kc_serilestir(&g_beklenen, g_ser_bek, sizeof g_ser_bek);
            size_t k = ilk_fark(g_ser[0], g_ser_bek, nb < g_ser_n[0] ? nb : g_ser_n[0]);
            KC_DOGRULA(nb == g_ser_n[0] && k == nb,
                       "NOT_INIT çıktısı A4-K126'nın sabit içeriğinden farklı (serileştirilmiş bayt %lu)",
                       (unsigned long)k);
            M.onceki = M.son;
            M.son = g_out[0];
        }
        return rc0;
    }
    /* Kurulu örnek: model. */
    baglam_t b;
    memset(&b, 0, sizeof b);
    b.f = f;
    b.in = in;
    b.rc = rc0;
    b.kabul = rc0 == IDA_RC_OK || rc0 == IDA_RC_IMU_REJECTED;
    b.daraltma = daraltma_var(in);
    b.ilk_yazilabilir = !M.yazilabilir_var;
    if (in->plat.link_event == IDA_LINKEV_SESSION_START) {
        M.iz_var = false; /* (b3) */
        M.oturum_kapali = false;
    }
    if (in->plat.link_event == IDA_LINKEV_SESSION_END) {
        /* A4-K73: arada SESSION_START yoksa yeni kapanma yok (yine daraltıcı girdi). */
        b.oturum_sonu_kapanma = !M.oturum_kapali;
        M.oturum_kapali = true;
    }
    if (in->plat.link_event == IDA_LINKEV_SESSION_START || in->plat.link_event == IDA_LINKEV_SESSION_END) {
        M.oturum_olayi++;
    } else if (in->plat.link_event != IDA_LINKEV_NONE) {
        M.oturum_belirsiz = true;
    }
    b.now_gecerli = zaman_gecerli(in->now_w.ns);
    if (b.now_gecerli) {
        if (M.now_var && in->now_w.ns < M.now_azami) {
            M.geri_adim++;
            M.geri_adim_cagri++;
        } else {
            M.now_azami = in->now_w.ns;
            M.now_var = true;
        }
        if (in->now_w.ns > M.now_ust) {
            M.now_ust = in->now_w.ns;
        }
        b.kullanilan_now = M.now_azami;
    } else {
        /* A4-K10: aralık dışı now_w geri adım sayılmaz; iz, sayaç ve günlük değişmez. */
        b.kullanilan_now = M.now_azami;
    }
    b.cekirdek_bg = M.bg_sayim;
    M.bg_sayim = 0u;
    if (!out_var) {
        KC_DOGRULA(rc0 == IDA_RC_BAD_INPUT, "out NULL çağrısı %d döndü, BAD_INPUT bekleniyordu (3.11 (b2))", (int)rc0);
        if (b.oturum_sonu_kapanma && M.olay_acik) {
            M.bekleyen_kapanma = true;
        }
        if (M.n_out_null_now < 32) {
            M.out_null_now[M.n_out_null_now++] = b.kullanilan_now;
        }
        M.out_null_var = true;
        return rc0;
    }
    b.uyusmazlik = in->bg_slices != b.cekirdek_bg;
    if (b.kabul && in->tf.present) {
        b.beklenen_seq = in->tf.seq;
        b.beklenen_t = in->tf.t.ns;
    } else {
        b.beklenen_seq = M.iz_var ? M.iz_seq : 0u;
        b.beklenen_t = M.iz_var ? M.iz_t : 0;
    }
    if (M.denetim) {
        genel_denetim(&b, &g_out[0]);
    }
    if (b.kabul && in->tf.present) {
        M.iz_var = true;
        M.iz_seq = in->tf.seq;
        M.iz_t = in->tf.t.ns;
    }
    /* Pencere sıfırlanır. */
    M.bekleyen_kapanma = false;
    M.oturum_olayi = 0u;
    M.oturum_belirsiz = false;
    M.geri_adim = 0u;
    M.geri_adim_cagri = 0u;
    M.geri_adim_belirsiz = false;
    M.geri_adim_log_belirsiz = false;
    M.bg_bad_sayi = 0u;
    M.out_null_var = false;
    M.n_out_null_now = 0;
    M.yazilabilir_var = true;
    M.onceki = M.son;
    M.son = g_out[0];
    M.onceki_cnt = g_out[0].status.cnt;
    /* Olay modeli denetim kapalıyken de güncel kalsın. */
    if (!M.denetim) {
        for (unsigned i = 0; i < g_out[0].n_events && i < IDA_PORT_MAX_EVENTS; ++i) {
            M.olay_var = true;
            M.son_ctr = (uint32_t)(g_out[0].events[i].event_id & 0xFFFFFFFFu);
            M.son_id = g_out[0].events[i].event_id;
        }
    }
    return rc0;
}

ida_rc_t kc_cagir(kc_islev_t f, ida_in_t *in)
{
    return cagri_yurut(f, in, true);
}

ida_rc_t kc_cagir_out_null(kc_islev_t f, ida_in_t *in)
{
    return cagri_yurut(f, in, false);
}

ida_rc_t kc_step(ida_in_t *in)
{
    return cagri_yurut(KC_STEP, in, true);
}

ida_rc_t kc_tick(ida_in_t *in)
{
    return cagri_yurut(KC_TICK, in, true);
}

ida_bg_rc_t kc_bg(int64_t now)
{
    ida_bg_rc_t rc0 = IDA_BG_IDLE;
    ida_wtime_t w;
    w.ns = now;
    M.cagri_no++;
    (void)snprintf(g_baglam, sizeof g_baglam, "çağrı #%llu background", (unsigned long long)M.cagri_no);
    for (int i = 0; i < M.n; ++i) {
        if (!M.kurulu) {
            memcpy(g_kopya[i], kc_bellek(i), M.boy);
        }
        ida_bg_rc_t rc = ida_core_background(kc_core(i), w);
        if (i == 0) {
            rc0 = rc;
        } else {
            KC_DOGRULA(rc == rc0, "arka plan dönüşü örnekler arasında farklı (A4-K24, K110)");
        }
        if (!M.kurulu) {
            KC_DOGRULA(memcmp(g_kopya[i], kc_bellek(i), M.boy) == 0,
                       "kurulmamış örnekte arka plan çağrısı örnek belleğine yazdı (A4-K117 (2))");
        }
    }
    if (!M.kurulu) {
        KC_DOGRULA(rc0 == IDA_BG_IDLE, "kurulmamış örnekte arka plan %d, IDLE bekleniyordu (A4-K117 (2))", (int)rc0);
        return rc0;
    }
    if (zaman_gecerli(now)) {
        KC_DOGRULA(rc0 == IDA_BG_IDLE || rc0 == IDA_BG_MORE, "geçerli arka plan çağrısı %d (A4-K93)", (int)rc0);
        M.bg_sayim++;
        if (M.now_var && now < M.now_azami) {
            M.geri_adim++;
            M.geri_adim_log_belirsiz = true; /* günlüğün hangi çağrıda çıkacağı A4'te açık değil */
        } else {
            M.now_azami = now;
            M.now_var = true;
        }
        if (now > M.now_ust) {
            M.now_ust = now;
        }
    } else {
        KC_DOGRULA(rc0 == IDA_BG_BAD_INPUT, "aralık dışı now_w ile arka plan %d, BAD_INPUT bekleniyordu (A4-K117)",
                   (int)rc0);
        M.bg_bad_sayi++;
    }
    return rc0;
}

void kc_bg_slices_elle(uint32_t deger)
{
    M.bg_elle = true;
    M.bg_elle_deger = deger;
}

const ida_out_t *kc_out(void)
{
    return &M.son;
}

const ida_out_t *kc_onceki_out(void)
{
    return &M.onceki;
}

uint32_t kc_fark(size_t alan_konumu)
{
    return kc_sayac_farki(&M.son.status.cnt, &M.onceki.status.cnt, alan_konumu);
}

/* ---- Parametre şeması ve blok ----------------------------------------------------- */

static ida_param_desc_t g_desc[KC_PARAM_AZAMI];
static uint16_t g_desc_n;
static bool g_desc_okundu;

static void desc_oku(void)
{
    if (g_desc_okundu) {
        return;
    }
    uint16_t n = ida_core_param_count();
    KC_DOGRULA(n <= KC_PARAM_AZAMI, "param_count %u > 997 (A4 §3.10)", (unsigned)n);
    for (unsigned iu = 0; iu < (unsigned)(n); ++iu) {
        uint16_t i = (uint16_t)iu;
        ida_rc_t rc = ida_core_param_desc(i, &g_desc[i]);
        KC_DOGRULA(rc == IDA_RC_OK, "param_desc(%u) %d (A4-K85)", (unsigned)i, (int)rc);
    }
    g_desc_n = n;
    g_desc_okundu = true;
}

uint16_t kc_param_sayisi(void)
{
    desc_oku();
    return g_desc_n;
}

const ida_param_desc_t *kc_param_desc(uint16_t i)
{
    desc_oku();
    return &g_desc[i];
}

size_t kc_varsayilan_kayitlar(kc_pkayit_t *k, size_t cap)
{
    desc_oku();
    KC_DOGRULA((size_t)g_desc_n <= cap, "kayıt kapasitesi");
    for (unsigned iu = 0; iu < (unsigned)(g_desc_n); ++iu) {
        uint16_t i = (uint16_t)iu;
        k[i].id = g_desc[i].id;
        k[i].type = g_desc[i].type;
        k[i].rsv = 0u;
        k[i].ham = kc_param_ham(&g_desc[i].def);
    }
    /* id'ye göre artan sıra (A4 §3.10). */
    for (size_t i = 1; i < g_desc_n; ++i) {
        kc_pkayit_t t = k[i];
        size_t j = i;
        while (j > 0 && k[j - 1].id > t.id) {
            k[j] = k[j - 1];
            --j;
        }
        k[j] = t;
    }
    return g_desc_n;
}

uint32_t kc_crc32(const uint8_t *p, size_t n)
{
    uint32_t c = 0xFFFFFFFFu;
    for (size_t i = 0; i < n; ++i) {
        c ^= p[i];
        for (int b = 0; b < 8; ++b) {
            c = (c & 1u) != 0u ? (c >> 1) ^ 0xEDB88320u : c >> 1;
        }
    }
    return c ^ 0xFFFFFFFFu;
}

size_t kc_blok_kur(uint8_t *buf, size_t cap, const kc_pkayit_t *k, size_t n, uint64_t hash)
{
    size_t boy = IDA_PBLOB_HDR_SIZE + n * IDA_PBLOB_REC_SIZE;
    KC_DOGRULA(boy <= cap, "blok tamponu küçük");
    kc_le32(&buf[0], IDA_PBLOB_MAGIC);
    kc_le16(&buf[4], (uint16_t)IDA_PBLOB_FMT_VER);
    kc_le16(&buf[6], 0u);
    kc_le64(&buf[8], hash);
    kc_le32(&buf[16], (uint32_t)(n * IDA_PBLOB_REC_SIZE));
    for (size_t i = 0; i < n; ++i) {
        uint8_t *r = &buf[IDA_PBLOB_HDR_SIZE + i * IDA_PBLOB_REC_SIZE];
        kc_le16(&r[0], k[i].id);
        r[2] = k[i].type;
        r[3] = k[i].rsv;
        kc_le32(&r[4], k[i].ham);
    }
    kc_le32(&buf[20], kc_crc32(&buf[IDA_PBLOB_HDR_SIZE], n * IDA_PBLOB_REC_SIZE));
    return boy;
}

uint64_t kc_sema_ozeti(void)
{
    ida_core_info_t bilgi;
    memset(&bilgi, 0, sizeof bilgi);
    ida_core_info(&bilgi);
    return bilgi.param_schema_hash;
}

int kc_param_imza(uint8_t type, uint32_t def, uint32_t min, uint32_t max)
{
    desc_oku();
    for (unsigned iu = 0; iu < (unsigned)(g_desc_n); ++iu) {
        uint16_t i = (uint16_t)iu;
        const ida_param_desc_t *d = &g_desc[i];
        if (d->type == type && kc_param_ham(&d->def) == def && kc_param_ham(&d->min) == min &&
            kc_param_ham(&d->max) == max) {
            return (int)i;
        }
    }
    return -1;
}

/* ---- Kolaylık ---------------------------------------------------------------------- */

static ida_in_t g_adim_in;

uint32_t kc_son_seq(void)
{
    return M.iz_var ? M.iz_seq : 0u;
}

int64_t kc_son_t(void)
{
    return M.iz_var ? M.iz_t : 0;
}

int64_t kc_son_now(void)
{
    return M.now_var ? M.now_azami : M.now_ust;
}

uint32_t kc_sonraki_seq(void)
{
    return kc_son_seq() + 1u;
}

int64_t kc_sonraki_t(void)
{
    int64_t t = kc_son_t();
    int64_t n = kc_son_now();
    return (t > n ? t : n) + KC_P;
}

ida_rc_t kc_adim(void)
{
    kc_in_step(&g_adim_in, kc_sonraki_seq(), kc_sonraki_t());
    return kc_step(&g_adim_in);
}

void kc_hazirla(int n_step)
{
    kc_init_bloksuz();
    for (int k = 1; k <= n_step; ++k) {
        kc_in_step(&g_adim_in, (uint32_t)k, KC_T0 + (int64_t)k * KC_P);
        ida_rc_t rc = kc_step(&g_adim_in);
        KC_DOGRULA(rc == IDA_RC_OK, "hazırlık step'i %d: dönüş %d, OK bekleniyordu", k, (int)rc);
    }
}
