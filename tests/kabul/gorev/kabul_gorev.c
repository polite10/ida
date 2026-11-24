/*
 * tests/kabul/gorev — ortak düzenek: kurulum, sensör akışı, girdi kurucular, çağrı ve her
 * çağrıda denetlenen değişmezler (D1–D9; liste başlıkta).
 *
 * Model yalnız sözleşmenin açıkça yazdığını izler:
 *   - son kabul edilen zaman çerçevesi (A4-K25),
 *   - emniyet alanlarının etkin değeri (A4-K116: kısıtlayan içerik her çağrıda uygulanır,
 *     gevşetme yalnız kabul edilen çağrıdaki kabul edilen inst 0 örneğinden; init ve
 *     SESSION_END kısıtlayan),
 *   - yineleme penceresi (A4-K48; kabul edilen çağrıdaki daraltıcı olmayan her komut
 *     girer). DUPLICATE yanıtlanan komutun pencereye girip girmediği yazılı değildir;
 *     iki okuma ayrı tutulur, beklenti yalnız ikisinin uyuştuğu yerde yazılır.
 *
 * ARM ön koşulları mimar kararı W3-K3'e göredir (docs/gorevler/W3-ortak.md; yalnız A4 3.17
 * satırı): kestirim düzeyi OK ya da DEGRADED yeterli, INIT/BAD/FAULT'ta ret; arıza eylemi
 * (HOLD ve üstü) ile sızıntı ve düşük/kritik batarya nedeni ret; yalnız WARN düzeyi, "arm
 * reddi gerektiren kayıt" olup olmadığı uygulamanın tanımı olduğundan, iki sonuca da izin
 * verir; bağlantı durumu ayrı ön koşul değildir. W3-K1: sanal emniyet girdileri (acil
 * durdurma, fiş, pim) araç ARM'ını engellemez ve aracı silahsızlandırmaz. W3-K2: "emniyet
 * kısıtı" çağrıda uygulanan daraltmadır (A4-K44). Emniyet modeli (A4-K116) acil durdurmanın
 * silahsızlandırmadığını (D3) ve emniyet kapanmasının daraltıcı girdi olup olmadığını
 * (A4-K111) belirlemek için tutulur.
 */
#include "kabul_gorev.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "unity.h"

/* ---- Hata iletisi ---------------------------------------------------------------- */

static char g_baglam_metni[200];
static uint64_t g_cagri_no_ileti;

void kg_baglam(const char *bicim, ...)
{
    va_list ap;
    va_start(ap, bicim);
    (void)vsnprintf(g_baglam_metni, sizeof g_baglam_metni, bicim, ap);
    va_end(ap);
}

_Noreturn void kg_basarisiz(const char *dosya, int satir, const char *bicim, ...)
{
    const char *ad = dosya;
    for (const char *p = dosya; *p != '\0'; ++p) {
        if (*p == '/' || *p == '\\') {
            ad = p + 1;
        }
    }
    static char msj[4][900];
    static unsigned k;
    char govde[640];
    va_list ap;
    va_start(ap, bicim);
    (void)vsnprintf(govde, sizeof govde, bicim, ap);
    va_end(ap);
    k = (k + 1u) % 4u;
    (void)snprintf(msj[k], sizeof msj[k], "%s [%s; çağrı #%llu; %s:%d]", govde, g_baglam_metni,
                   (unsigned long long)g_cagri_no_ileti, ad, satir);
    UnityFail(msj[k], (UNITY_LINE_TYPE)satir);
}

/* ---- Sözde rastgele sayı ---------------------------------------------------------- */

void kg_rng_kur(kg_rng_t *r, uint64_t tohum)
{
    r->s = tohum;
}

static uint64_t rng_u64(kg_rng_t *r)
{
    uint64_t z;
    r->s += UINT64_C(0x9E3779B97F4A7C15);
    z = r->s;
    z = (z ^ (z >> 30)) * UINT64_C(0xBF58476D1CE4E5B9);
    z = (z ^ (z >> 27)) * UINT64_C(0x94D049BB133111EB);
    return z ^ (z >> 31);
}

uint32_t kg_rng(kg_rng_t *r, uint32_t n)
{
    if (n == 0u) {
        return 0u;
    }
    return (uint32_t)(rng_u64(r) % (uint64_t)n);
}

bool kg_olasi(kg_rng_t *r, uint32_t binde)
{
    return kg_rng(r, 1000u) < binde;
}

/* ---- Durum ------------------------------------------------------------------------ */

#define KG_HAVUZ ((size_t)1 << 20)
#define KG_PENCERE 32u

typedef union {
    long double ld;
    uint64_t u;
    void *p;
    double d;
    unsigned char b[KG_HAVUZ];
} kg_havuz_t;

static kg_havuz_t g_havuz;
static unsigned char g_anlik_bellek[KG_HAVUZ];

/* Emniyet alanları: 0 acil durdurma, 1 tüp 0 fiş, 2 tüp 0 pim, 3 tüp 1 fiş, 4 tüp 1 pim. */
#define KG_ALAN_N 5

typedef struct {
    uint32_t id[KG_PENCERE];
    unsigned n;
    unsigned bas;
} kg_pencere_t;

/* Bir önceki çağrının sonundaki gözlenen durum. */
typedef struct {
    bool var;
    bool armed;
    uint8_t mode;
    uint8_t fs_level;
    uint32_t fs_reasons;
    bool param_ok;
    bool imu_lag;
    bool est_var;
    uint8_t est;
    bool link;
} kg_onceki_t;

typedef struct {
    size_t boy;
    bool kurulu;
    bool oturum_baslat;
    bool iz_var;
    uint32_t son_seq;
    int64_t son_t;
    int64_t now;
    uint32_t seq_sonraki;
    uint32_t imu_seq;
    uint32_t ornek_seq[16];
    uint32_t cmd_id;
    uint32_t mandal_id;
    uint32_t ping_no;
    uint64_t cagri_no;
    bool kisit[KG_ALAN_N];
    kg_pencere_t pen_a; /* DUPLICATE yanıtlanan da girer */
    kg_pencere_t pen_b; /* DUPLICATE yanıtlanan girmez */
    kg_onceki_t P;
    bool son_var;
    ida_out_t son;
    ida_out_t onceki;
    ida_rc_t son_rc;
    kg_dunya_t dunya;
} kg_model_t;

static kg_model_t M;
static kg_model_t g_anlik_M;
static ida_in_t g_in;
static ida_in_t g_son_in;
static ida_out_t g_out;
static kg_sayac_t g_sayac;
/* Çekirdek bir emniyet alanını gevşettiğini (SAFETY_INPUT günlüğünde gevşek değer) en az bir
   kez gösterdi mi (A4-K116, 3.15 kod 19). Çekirdek yapısının özelliğidir; init'te sıfırlanmaz. */
static bool g_gevsetme_gozlendi;

static ida_core_t *core(void)
{
    return (ida_core_t *)(void *)g_havuz.b;
}

kg_sayac_t *kg_sayac(void)
{
    return &g_sayac;
}

kg_dunya_t *kg_dunya(void)
{
    return &M.dunya;
}

void kg_dunya_olagan(void)
{
    kg_dunya_t *d = &M.dunya;
    memset(d, 0, sizeof *d);
    d->link = true;
    d->pstore = (uint8_t)IDA_PSTORE_IDLE;
    d->pgen = 0u;
    d->imu = true;
    d->mag = true;
    d->baro = true;
    d->gnss = true;
    d->dvl = true;
    d->ping1d = true;
    d->batt = true;
    d->sizinti_olcer = true;
    d->emniyet = true;
    d->hb = true;
    d->estop = 0u;
    d->tube[0] = KG_TUP_ACIK;
    d->tube[1] = KG_TUP_ACIK;
    d->sizinti = 0u;
    d->batt_v = 16.0f;
    d->kestirim_bozuk = false;
}

/* ---- Küçük yardımcılar ------------------------------------------------------------ */

static void le16(uint8_t *p, uint16_t v)
{
    p[0] = (uint8_t)(v & 0xFFu);
    p[1] = (uint8_t)(v >> 8);
}

static void le32(uint8_t *p, uint32_t v)
{
    for (int i = 0; i < 4; ++i) {
        p[i] = (uint8_t)((v >> (8 * i)) & 0xFFu);
    }
}

static void le64(uint8_t *p, uint64_t v)
{
    for (int i = 0; i < 8; ++i) {
        p[i] = (uint8_t)((v >> (8 * i)) & 0xFFu);
    }
}

static uint32_t crc32_iso_hdlc(const uint8_t *p, size_t n)
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

static int bit_sayisi(uint32_t x)
{
    int n = 0;
    while (x != 0u) {
        n += (int)(x & 1u);
        x >>= 1;
    }
    return n;
}

/* ---- Parametre bloğu (A4 §3.10, A4-K80) ------------------------------------------- */

#define KG_PARAM_AZAMI 997

typedef struct {
    uint16_t id;
    uint8_t type;
    uint32_t ham;
} kg_pkayit_t;

static kg_pkayit_t g_kayit[KG_PARAM_AZAMI];
static uint8_t g_blok[IDA_PARAM_BLOB_MAX + 64u];

static size_t blok_kur(bool bozuk)
{
    ida_core_info_t bilgi;
    uint16_t n = ida_core_param_count();
    memset(&bilgi, 0, sizeof bilgi);
    ida_core_info(&bilgi);
    KG_DOGRULA(n <= KG_PARAM_AZAMI, "param_count %u > 997 (A4 §3.10)", (unsigned)n);
    for (unsigned iu = 0; iu < (unsigned)n && iu < KG_PARAM_AZAMI; ++iu) {
        ida_param_desc_t d;
        uint32_t ham;
        memset(&d, 0, sizeof d);
        KG_DOGRULA(ida_core_param_desc((uint16_t)iu, &d) == IDA_RC_OK, "param_desc(%u) (A4-K85)", iu);
        memcpy(&ham, &d.def.v, sizeof ham);
        g_kayit[iu].id = d.id;
        g_kayit[iu].type = d.type;
        g_kayit[iu].ham = ham;
    }
    /* id'ye göre artan sıra (A4 §3.10). */
    for (size_t i = 1; i < (size_t)n && i < KG_PARAM_AZAMI; ++i) {
        kg_pkayit_t t = g_kayit[i];
        size_t j = i;
        while (j > 0u && g_kayit[j - 1u].id > t.id) {
            g_kayit[j] = g_kayit[j - 1u];
            --j;
        }
        g_kayit[j] = t;
    }
    size_t yuk = (size_t)n * IDA_PBLOB_REC_SIZE;
    size_t boy = IDA_PBLOB_HDR_SIZE + yuk;
    KG_DOGRULA(boy <= IDA_PARAM_BLOB_MAX, "blok boyu %lu > 8000", (unsigned long)boy);
    le32(&g_blok[0], IDA_PBLOB_MAGIC);
    le16(&g_blok[4], (uint16_t)IDA_PBLOB_FMT_VER);
    le16(&g_blok[6], 0u);
    le64(&g_blok[8], bilgi.param_schema_hash);
    le32(&g_blok[16], (uint32_t)yuk);
    for (size_t i = 0; i < (size_t)n && i < KG_PARAM_AZAMI; ++i) {
        uint8_t *r = &g_blok[IDA_PBLOB_HDR_SIZE + i * IDA_PBLOB_REC_SIZE];
        le16(&r[0], g_kayit[i].id);
        r[2] = g_kayit[i].type;
        r[3] = 0u;
        le32(&r[4], g_kayit[i].ham);
    }
    uint32_t crc = crc32_iso_hdlc(&g_blok[IDA_PBLOB_HDR_SIZE], yuk);
    if (bozuk) {
        crc ^= 0x00010000u; /* A4-K80 denetim (4) */
    }
    le32(&g_blok[20], crc);
    return boy;
}

/* ---- Kurulum ---------------------------------------------------------------------- */

void kg_kurulum_varsayilan(kg_kurulum_t *k)
{
    k->blok = KG_BLOK_GECERLI;
    k->boot_ok = true;
    k->boot_count = KG_BOOT;
}

void kg_kur(const kg_kurulum_t *k)
{
    ida_core_info_t bilgi;
    ida_init_in_t ii;
    memset(&bilgi, 0, sizeof bilgi);
    ida_core_info(&bilgi);
    KG_DOGRULA(bilgi.core_size > 0u && (size_t)bilgi.core_size <= KG_HAVUZ, "core_size %lu",
               (unsigned long)bilgi.core_size);
    KG_DOGRULA(bilgi.core_align > 0u && (size_t)bilgi.core_align <= _Alignof(kg_havuz_t), "core_align %lu",
               (unsigned long)bilgi.core_align);
    memset(&M, 0, sizeof M);
    M.boy = (size_t)bilgi.core_size <= KG_HAVUZ ? (size_t)bilgi.core_size : KG_HAVUZ; /* C6386 */
    memset(g_havuz.b, 0x5A, M.boy);

    memset(&ii, 0, sizeof ii);
    ii.port_major = (uint16_t)IDA_PORT_VERSION_MAJOR;
    ii.port_minor = (uint16_t)IDA_PORT_VERSION_MINOR;
    ii.now_w.ns = KG_T0;
    ii.boot_count = k->boot_count;
    ii.boot_count_ok = k->boot_ok;
    ii.reset_cause = (uint8_t)IDA_RESET_SIL_START;
    ii.param_src = (uint8_t)IDA_PSRC_SIL;
    if (k->blok == KG_BLOK_YOK) {
        ii.param_src = (uint8_t)IDA_PSRC_NONE;
        ii.param_blob = NULL;
        ii.param_len = 0u;
    } else {
        size_t boy = blok_kur(k->blok == KG_BLOK_BOZUK);
        ii.param_blob = g_blok;
        ii.param_len = (uint32_t)boy;
    }
    ida_rc_t rc = ida_core_init(core(), &ii);
    ida_rc_t beklenen = k->blok == KG_BLOK_GECERLI ? IDA_RC_OK : IDA_RC_PARAM_DEFAULTS;
    KG_DOGRULA(rc == beklenen, "init %d döndü, %d bekleniyordu (A4-K78, K80)", (int)rc, (int)beklenen);

    M.kurulu = true;
    M.oturum_baslat = true;
    M.iz_var = false;
    M.now = KG_T0;
    M.seq_sonraki = 1u;
    M.cmd_id = 1000u;
    M.mandal_id = 0x40000000u;
    for (int i = 0; i < KG_ALAN_N; ++i) {
        M.kisit[i] = true; /* A4-K116: init'te kısıtlayan */
    }
    M.P.var = false;
    M.P.armed = false; /* A4-K77 */
    M.P.mode = (uint8_t)IDA_MODE_INIT;
    kg_dunya_olagan();
}

void kg_kur_varsayilan(void)
{
    kg_kurulum_t k;
    kg_kurulum_varsayilan(&k);
    kg_kur(&k);
}

/* ---- Girdi kurucular -------------------------------------------------------------- */

uint32_t kg_yeni_id(void)
{
    return M.cmd_id++;
}

ida_cmd_t *kg_komut_id(ida_in_t *in, uint8_t type, uint32_t cmd_id)
{
    KG_DOGRULA(in->n_cmds < IDA_PORT_MAX_CMDS, "komut dizisi dolu");
    unsigned yer = in->n_cmds < IDA_PORT_MAX_CMDS ? in->n_cmds : IDA_PORT_MAX_CMDS - 1u; /* C6386 */
    ida_cmd_t *c = &in->cmds[yer];
    in->n_cmds = (uint8_t)(in->n_cmds + 1u);
    memset(c, 0, sizeof *c);
    c->cmd_id = cmd_id;
    c->type = type;
    c->source = 255u;
    return c;
}

ida_cmd_t *kg_komut(ida_in_t *in, uint8_t type)
{
    return kg_komut_id(in, type, kg_yeni_id());
}

ida_cmd_t *kg_kip_komutu(ida_in_t *in, uint8_t mode)
{
    ida_cmd_t *c = kg_komut(in, (uint8_t)IDA_CMD_SET_MODE);
    c->u.set_mode.mode = mode;
    return c;
}

void kg_mandal(ida_in_t *in, uint8_t bit, uint8_t mode)
{
    in->narrow.mask = (uint8_t)(in->narrow.mask | bit);
    for (unsigned i = 0; i < IDA_PORT_N_NARROW; ++i) {
        if ((bit & (1u << i)) != 0u) {
            in->narrow.cmd_id[i] = M.mandal_id++;
        }
    }
    if ((bit & IDA_NARROW_MODE) != 0u) {
        in->narrow.mode = mode;
    }
}

static int64_t ornek_zamani(const ida_in_t *in)
{
    return in->tf.present ? in->tf.t.ns : in->now_w.ns;
}

ida_sample_t *kg_ornek(ida_in_t *in, uint8_t type, uint8_t inst)
{
    KG_DOGRULA(in->n_samples < IDA_PORT_MAX_SAMPLES, "örnek dizisi dolu");
    unsigned yer = in->n_samples < IDA_PORT_MAX_SAMPLES ? in->n_samples : IDA_PORT_MAX_SAMPLES - 1u; /* C6386 */
    ida_sample_t *s = &in->samples[yer];
    in->n_samples = (uint8_t)(in->n_samples + 1u);
    memset(s, 0, sizeof *s);
    M.ornek_seq[type & 15u]++;
    s->hdr.t_meas.ns = ornek_zamani(in);
    s->hdr.t_rx.ns = in->now_w.ns;
    s->hdr.seq = M.ornek_seq[type & 15u];
    s->hdr.window_ns = 0u;
    if (type == IDA_SENS_BARO) {
        s->hdr.window_ns = (uint32_t)(10 * KG_MS);
    } else if (type == IDA_SENS_BATT) {
        s->hdr.window_ns = (uint32_t)(50 * KG_MS);
    }
    s->hdr.flags = (uint16_t)IDA_HF_VALID;
    s->hdr.type = type;
    s->hdr.inst = inst;
    return s;
}

ida_sample_t *kg_ornek_bul_ya_da_ekle(ida_in_t *in, uint8_t type)
{
    for (unsigned i = 0; i < in->n_samples && i < IDA_PORT_MAX_SAMPLES; ++i) {
        if (in->samples[i].hdr.type == type && in->samples[i].hdr.inst == 0u) {
            return &in->samples[i];
        }
    }
    return kg_ornek(in, type, 0u);
}

ida_sample_t *kg_emniyet(ida_in_t *in, uint8_t inst, uint8_t estop, uint8_t tube0, uint8_t tube1, bool gecerli)
{
    ida_sample_t *s = NULL;
    /* Aynı inst'in periyodik örneği varsa onun yerine yazılır (tek örnek). */
    for (unsigned i = 0; i < in->n_samples && i < IDA_PORT_MAX_SAMPLES; ++i) {
        if (in->samples[i].hdr.type == IDA_SENS_SAFETY && in->samples[i].hdr.inst == inst) {
            s = &in->samples[i];
        }
    }
    if (s == NULL) {
        s = kg_ornek(in, (uint8_t)IDA_SENS_SAFETY, inst);
    }
    s->u.safety.estop = estop;
    s->u.safety.tube[0] = tube0;
    s->u.safety.tube[1] = tube1;
    s->hdr.flags = (uint16_t)(gecerli ? IDA_HF_VALID : 0u);
    return s;
}

static void imu_kur(ida_in_t *in)
{
    const kg_dunya_t *d = &M.dunya;
    ida_imu_sample_t *imu = &in->imu;
    memset(imu, 0, sizeof *imu);
    M.imu_seq++;
    imu->hdr.t_meas.ns = in->tf.present ? in->tf.t.ns : in->now_w.ns;
    imu->hdr.t_rx.ns = in->now_w.ns;
    imu->hdr.seq = M.imu_seq;
    imu->hdr.window_ns = (uint32_t)KG_P;
    imu->hdr.flags = (uint16_t)IDA_HF_VALID;
    imu->hdr.type = (uint8_t)IDA_SENS_IMU;
    imu->hdr.inst = 0u;
    /* Durgun, yatay araç; IMU çerçevesi gövdeye paralel (A5-K89): özgül kuvvet -g (z aşağı). */
    imu->imu.accel[2] = -9.80665f;
    imu->imu.temp_c = 20.0f;
    if (d->kestirim_bozuk) {
        /* Jiroskop dönüş, ivmeölçer yatay ivme söylüyor; manyetometre, GNSS ve DVL durgun. */
        imu->imu.gyro[2] = 0.8f;
        imu->imu.accel[0] = 4.0f;
    }
}

static void periyodik_ornekler(ida_in_t *in, uint64_t k)
{
    const kg_dunya_t *d = &M.dunya;
    ida_sample_t *s;
    if (d->mag && (k % 2u) == 0u) {
        s = kg_ornek(in, (uint8_t)IDA_SENS_MAG, 0u);
        s->u.mag.field[0] = 2.0e-5f;
        s->u.mag.field[1] = 0.0f;
        s->u.mag.field[2] = 4.0e-5f;
    }
    if (d->baro && (k % 4u) == 1u) {
        s = kg_ornek(in, (uint8_t)IDA_SENS_BARO, 0u);
        s->u.baro.p_abs = 103335.0f; /* yaklaşık 0,2 m derinlik */
        s->u.baro.temp_c = 15.0f;
    }
    if (d->gnss && (k % 40u) == 3u) {
        s = kg_ornek(in, (uint8_t)IDA_SENS_GNSS, 0u);
        s->u.gnss.lat_e7 = 410000000;
        s->u.gnss.lon_e7 = 290000000;
        s->u.gnss.alt_mm = 0;
        s->u.gnss.eph = 1.5f;
        s->u.gnss.epv = 3.0f;
        s->u.gnss.fix_type = (uint8_t)IDA_GNSS_FIX_3D;
        s->u.gnss.n_sv = 12u;
    }
    if (d->dvl && (k % 40u) == 23u) {
        s = kg_ornek(in, (uint8_t)IDA_SENS_DVL, 0u);
        s->u.dvl.fom = 0.005f;
        for (int i = 0; i < 4; ++i) {
            s->u.dvl.beam_dist[i] = 11.5f;
        }
        s->u.dvl.altitude = 10.0f;
        s->u.dvl.dvl_flags = (uint8_t)(IDA_DVL_VEL_VALID | IDA_DVL_TRACK_BOTTOM);
        s->u.dvl.beam_valid = 0x0Fu;
    }
    if (d->ping1d && (k % 20u) == 13u) {
        s = kg_ornek(in, (uint8_t)IDA_SENS_PING1D, 0u);
        M.ping_no++;
        s->u.ping1d.distance = 10.0f;
        s->u.ping1d.scan_start = 0.5f;
        s->u.ping1d.scan_length = 30.0f;
        s->u.ping1d.ping_number = M.ping_no;
        s->u.ping1d.tx_offset_ns = 13333333u;
        s->u.ping1d.confidence = 100u;
    }
    if (d->batt && (k % 20u) == 5u) {
        s = kg_ornek(in, (uint8_t)IDA_SENS_BATT, 0u);
        s->u.batt.voltage = d->batt_v;
        s->u.batt.current = 1.0f;
        s->u.batt.temp_c = 25.0f;
    }
    if (d->sizinti_olcer && (k % 20u) == 15u) {
        s = kg_ornek(in, (uint8_t)IDA_SENS_LEAK, 0u);
        s->u.leak.mask = d->sizinti;
    }
    if (d->emniyet && (k % 20u) == 0u) {
        (void)kg_emniyet(in, 0u, d->estop, d->tube[0], d->tube[1], true);
    }
}

ida_in_t *kg_girdi(kg_tur_t tur)
{
    const kg_dunya_t *d = &M.dunya;
    ida_in_t *in = &g_in;
    int64_t t = (M.son_t > M.now ? M.son_t : M.now) + KG_P;
    bool step = false;
    memset(in, 0, sizeof *in);
    in->ping360 = NULL;
    in->plat.link_state = (uint8_t)(d->link ? IDA_LINK_RUNNING : IDA_LINK_NONE);
    in->plat.link_event = (uint8_t)IDA_LINKEV_NONE;
    if (M.oturum_baslat && d->link && tur != KG_OTURUM_SONU) {
        in->plat.link_event = (uint8_t)IDA_LINKEV_SESSION_START;
    }
    in->plat.param_store = d->pstore;
    in->plat.param_gen = d->pgen;
    if (tur == KG_OLAGAN) {
        tur = d->imu ? KG_OLAGAN : KG_TICK_TF;
        step = d->imu;
    }
    in->now_w.ns = t;
    switch (tur) {
    case KG_OLAGAN:
    case KG_TICK_TF:
        in->tf.present = true;
        in->tf.seq = M.seq_sonraki;
        in->tf.t.ns = t;
        in->tf.t_rx.ns = t;
        break;
    case KG_IZGARA:
        in->tf.present = false;
        break;
    case KG_YINELENEN:
        KG_DOGRULA(M.iz_var, "yinelenen parti için kabul edilmiş zaman çerçevesi yok");
        in->tf.present = true;
        in->tf.seq = M.son_seq;
        in->tf.t.ns = M.son_t;
        in->tf.t_rx.ns = t;
        step = d->imu;
        break;
    case KG_ESKI:
        KG_DOGRULA(M.iz_var && M.son_t > 3 * KG_P, "eski parti için iz yok");
        in->tf.present = true;
        in->tf.seq = M.son_seq - 3u;
        in->tf.t.ns = M.son_t - 3 * KG_P;
        in->tf.t_rx.ns = t;
        step = d->imu;
        break;
    case KG_OTURUM_SONU:
    default:
        in->tf.present = false;
        in->plat.link_event = (uint8_t)IDA_LINKEV_SESSION_END;
        in->plat.link_state = (uint8_t)IDA_LINK_NONE;
        break;
    }
    in->imu_present = step;
    if (step) {
        imu_kur(in);
    }
    if (tur != KG_OTURUM_SONU) {
        periyodik_ornekler(in, M.cagri_no);
        if (d->hb && (M.cagri_no % 40u) == 0u) {
            (void)kg_komut(in, (uint8_t)IDA_CMD_HEARTBEAT);
        }
    }
    return in;
}

/* ---- Komut sınıfları (A4 §3.4, A4-K44, A4-K104) ----------------------------------- */

static bool kip_daraltici_hedef(uint8_t m)
{
    return m == IDA_MODE_MANUAL || m == IDA_MODE_STABILIZE || m == IDA_MODE_DEPTH_HOLD ||
           m == IDA_MODE_STATION_KEEP;
}

static bool daraltici_komut(const ida_cmd_t *c)
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

static bool genisletici_komut(const ida_cmd_t *c)
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

/* ---- Yineleme penceresi (A4-K48) -------------------------------------------------- */

static void pencere_ekle(kg_pencere_t *p, uint32_t id)
{
    if (p->n < KG_PENCERE) {
        p->id[(p->bas + p->n) % KG_PENCERE] = id;
        p->n++;
    } else {
        p->id[p->bas % KG_PENCERE] = id;
        p->bas = (p->bas + 1u) % KG_PENCERE;
    }
}

static bool pencerede(const kg_pencere_t *p, uint32_t id)
{
    for (unsigned i = 0; i < p->n && i < KG_PENCERE; ++i) {
        if (p->id[(p->bas + i) % KG_PENCERE] == id) {
            return true;
        }
    }
    return false;
}

enum { KG_YIN_YOK = 0, KG_YIN_KESIN = 1, KG_YIN_BELIRSIZ = 2 };

static int yinelenme(uint32_t id)
{
    bool a = pencerede(&M.pen_a, id);
    bool b = pencerede(&M.pen_b, id);
    if (a && b) {
        return KG_YIN_KESIN;
    }
    if (!a && !b) {
        return KG_YIN_YOK;
    }
    return KG_YIN_BELIRSIZ;
}

/* ---- Emniyet modeli (A4-K116, A4-K44, A4-K73, A4-K111) ---------------------------- */

static bool tup_bitleri_gecerli(uint8_t t)
{
    return (t & 0xF0u) == 0u; /* W3-K14: (uint8_t)~X yerine (C4310) */
}

/* Alan değerinin kısıtlayıcı olup olmadığı (A4-K116). */
static bool alan_kisitli(const ida_safety_t *s, int alan)
{
    switch (alan) {
    case 0:
        return s->estop != 0u;
    case 1:
        return (s->tube[0] & IDA_TUBE_PLUG_IN) == 0u;
    case 2:
        return (s->tube[0] & IDA_TUBE_PIN_OUT) == 0u;
    case 3:
        return (s->tube[1] & IDA_TUBE_PLUG_IN) == 0u;
    default:
        return (s->tube[1] & IDA_TUBE_PIN_OUT) == 0u;
    }
}

/* Çağrının emniyet modelini günceller; bir alan bu çağrıda kısıtlayana döndüyse true. */
static bool emniyet_guncelle(const ida_in_t *in, ida_rc_t rc)
{
    bool kabul = rc == IDA_RC_OK || rc == IDA_RC_IMU_REJECTED;
    bool kisit_cagri[KG_ALAN_N] = {false, false, false, false, false};
    bool gevset[KG_ALAN_N] = {false, false, false, false, false};
    bool kapanma = false;
    unsigned n = in->n_samples < IDA_PORT_MAX_SAMPLES ? in->n_samples : IDA_PORT_MAX_SAMPLES;
    for (unsigned i = 0; i < n; ++i) {
        const ida_sample_t *s = &in->samples[i];
        if (s->hdr.type != IDA_SENS_SAFETY || s->hdr.inst >= IDA_PORT_MAX_INST) {
            continue;
        }
        for (int a = 0; a < KG_ALAN_N; ++a) {
            if (alan_kisitli(&s->u.safety, a)) {
                kisit_cagri[a] = true;
            } else if (kabul && s->hdr.inst == 0u && (s->hdr.flags & IDA_HF_VALID) != 0u && s->u.safety.estop <= 1u &&
                       tup_bitleri_gecerli(s->u.safety.tube[0]) && tup_bitleri_gecerli(s->u.safety.tube[1])) {
                gevset[a] = true;
            }
        }
    }
    if (in->plat.link_event == IDA_LINKEV_SESSION_END) {
        for (int a = 0; a < KG_ALAN_N; ++a) {
            kisit_cagri[a] = true;
        }
    }
    for (int a = 0; a < KG_ALAN_N; ++a) {
        bool once = M.kisit[a];
        if (kisit_cagri[a]) {
            M.kisit[a] = true;
        } else if (gevset[a]) {
            M.kisit[a] = false;
        }
        if (!once && M.kisit[a]) {
            kapanma = true;
        }
    }
    return kapanma;
}

bool kg_kisit_estop(void)
{
    return M.kisit[0];
}

bool kg_gevsetme_gozlendi(void)
{
    return g_gevsetme_gozlendi;
}

/* ---- ARM sonucunun izinli kümesi (D8) --------------------------------------------- */

/* W3-K3: sızıntı ve düşük/kritik batarya arm'ı engeller. */
#define KG_FSR_ARM_ENGELI (IDA_FSR_LEAK | IDA_FSR_BATT_LOW | IDA_FSR_BATT_CRIT)
/* W3-K3: DEGRADED yeterli; bu yüzden yalnız kestirim bozulması nedeni hazır olmayı bozmaz. */
#define KG_FSR_HAZIR_IZINLI IDA_FSR_EST_DEGRADED

static bool est_yeterli(uint8_t e)
{
    return e == IDA_EST_OK || e == IDA_EST_DEGRADED;
}

#define KG_R(r) (UINT32_C(1) << (r))
#define KG_IZ_KABUL (UINT32_C(1) << 16)
#define KG_IZ_YIN (UINT32_C(1) << 17)

static uint32_t sonuc_biti(const ida_cmd_result_t *r)
{
    if (r->ack == IDA_ACK_ACCEPTED) {
        return KG_IZ_KABUL;
    }
    if (r->ack == IDA_ACK_DUPLICATE) {
        return KG_IZ_YIN;
    }
    if (r->ack == IDA_ACK_REJECTED && r->reason <= IDA_REASON_CORE_LAST) {
        return KG_R(r->reason);
    }
    return UINT32_C(1) << 30; /* hiçbir kümede değil */
}

/*
 * Kabul edilen çağrıdaki yinelenmeyen ARM'ın izinli sonuçları. Dayanak: 4.14 son paragraf
 * (NARROWED > IMU_REJECTED > IMU_LAG > komutun kendi nedeni), 3.17 ARM satırı (EVENTS_OFF,
 * PARAM_INVALID, PARAM_BUSY, NOT_ARMABLE, sonra ACCEPTED), A4-K61, K78, K83, K103, K108;
 * NOT_ARMABLE'ın koşulları mimar kararı W3-K3 ile (dosya başı).
 */
static uint32_t arm_izinli(const ida_in_t *in, ida_rc_t rc, const ida_out_t *o, bool daraltma, bool sonra_kip)
{
    const kg_onceki_t *P = &M.P;
    uint32_t s = 0u;
    if (daraltma) {
        return KG_R(IDA_REASON_NARROWED);
    }
    if (rc == IDA_RC_IMU_REJECTED) {
        return KG_R(IDA_REASON_IMU_REJECTED);
    }
    if (P->imu_lag && o->health.imu_lag) {
        return KG_R(IDA_REASON_IMU_LAG);
    }
    if (P->imu_lag || o->health.imu_lag) {
        s |= KG_R(IDA_REASON_IMU_LAG);
    }
    if (!o->status.events_ok) {
        return s | KG_R(IDA_REASON_EVENTS_OFF);
    }
    bool pok_once = P->var ? P->param_ok : o->status.param_ok;
    if (!pok_once && !o->status.param_ok) {
        return s | KG_R(IDA_REASON_PARAM_INVALID);
    }
    if (pok_once != o->status.param_ok) {
        s |= KG_R(IDA_REASON_PARAM_INVALID);
    }
    if (in->plat.param_store == IDA_PSTORE_BUSY) {
        return s | KG_R(IDA_REASON_PARAM_BUSY);
    }
    /* W3-K3: kestirim INIT/BAD/FAULT, arıza eylemi (HOLD ve üstü), sızıntı ve batarya nedeni. */
    bool est_ret = P->est_var && o->nav_valid && !est_yeterli(P->est) && !est_yeterli(o->nav.est_level);
    bool neden_engel = (o->status.fs_reasons & KG_FSR_ARM_ENGELI) != 0u || est_ret;
    if (o->fs_level >= IDA_FS_HOLD && !neden_engel && sonra_kip) {
        /* W3-K13: ARM'dan sonra kabul edilen SET_MODE arıza düzeyini yeni kipe göre yeniden
           hesaplar; çağrı sonundaki düzey ARM işlendiği andaki düzey olmayabilir. */
        return s | KG_R(IDA_REASON_NOT_ARMABLE) | KG_IZ_KABUL;
    }
    if (o->fs_level >= IDA_FS_HOLD || neden_engel) {
        return s | KG_R(IDA_REASON_NOT_ARMABLE);
    }
    bool hazir = P->var && P->fs_level <= IDA_FS_WARN && o->fs_level <= IDA_FS_WARN &&
                 (P->fs_reasons & ~KG_FSR_HAZIR_IZINLI) == 0u && (o->status.fs_reasons & ~KG_FSR_HAZIR_IZINLI) == 0u &&
                 P->est_var && est_yeterli(P->est) && (!o->nav_valid || est_yeterli(o->nav.est_level)) &&
                 P->mode != IDA_MODE_INIT && o->status.mode != IDA_MODE_INIT;
    if (hazir) {
        return s | KG_IZ_KABUL;
    }
    return s | KG_R(IDA_REASON_NOT_ARMABLE) | KG_IZ_KABUL;
}

/* ---- Çağrı ve değişmezler --------------------------------------------------------- */

/* results[i]; indis sınırı açıkça denetlenir (MSVC /analyze C6385). */
static const ida_cmd_result_t *sonuc_al(const ida_out_t *o, unsigned i)
{
    unsigned yer = i < IDA_PORT_MAX_RESULTS ? i : IDA_PORT_MAX_RESULTS - 1u;
    KG_DOGRULA(i < IDA_PORT_MAX_RESULTS, "sonuç indisi %u sınır dışı", i);
    return &o->results[yer];
}

static void degismezler(const ida_in_t *in, ida_rc_t rc, const ida_out_t *o, bool kapanma)
{
    const kg_onceki_t *P = &M.P;
    bool kabul = rc == IDA_RC_OK || rc == IDA_RC_IMU_REJECTED;
    bool ret = rc == IDA_RC_BAD_INPUT || rc == IDA_RC_DUP_SEQ || rc == IDA_RC_OLD_SEQ;
    KG_DOGRULA(kabul || ret, "beklenmeyen dönüş %d", (int)rc);
    if (!in->imu_present) {
        KG_DOGRULA(rc != IDA_RC_IMU_REJECTED, "tick IMU_REJECTED döndü (4.15)");
    }

    /* D9 (3.8): üst düzey alanlar ile status aynı çağrı sonu durumunu taşır. */
    KG_DOGRULA(o->armed == o->status.armed, "out.armed %d, status.armed %d (3.8)", (int)o->armed,
               (int)o->status.armed);
    KG_DOGRULA(o->fs_level == o->status.fs_level, "out.fs_level %u, status.fs_level %u (3.8)",
               (unsigned)o->fs_level, (unsigned)o->status.fs_level);
    KG_DOGRULA(o->status.mode <= IDA_MODE_AUTO, "status.mode %u tanımsız (A4 §3.4)", (unsigned)o->status.mode);
    KG_DOGRULA(o->fs_level <= IDA_FS_DISARM, "fs_level %u tanımsız (3.8)", (unsigned)o->fs_level);

    uint8_t bitler = (uint8_t)(in->narrow.mask & IDA_NARROW_DEFINED);
    unsigned nbit = (unsigned)bit_sayisi(bitler);
    unsigned nk = in->n_cmds <= IDA_PORT_MAX_CMDS ? in->n_cmds : 0u;
    KG_DOGRULA(o->n_results == nbit + nk, "n_results %u, beklenen %u (A4-K43, K114)", (unsigned)o->n_results,
               nbit + nk);

    /* Mandal sonuçları (A4-K43, K46, K114): her tanımlı bit ACCEPTED. */
    unsigned k = 0;
    for (unsigned i = 0; i < IDA_PORT_N_NARROW; ++i) {
        if ((bitler & (1u << i)) == 0u) {
            continue;
        }
        const ida_cmd_result_t *r = sonuc_al(o, k++);
        KG_DOGRULA(r->cmd_id == in->narrow.cmd_id[i] && r->kind == IDA_RES_ACK && r->ack == IDA_ACK_ACCEPTED &&
                       r->reason == IDA_REASON_NONE,
                   "mandal biti %u sonucu (id %lu, ack %u, neden %u); ACCEPTED bekleniyordu (A4-K43, K46, K114)", i,
                   (unsigned long)r->cmd_id, (unsigned)r->ack, (unsigned)r->reason);
    }

    /* Daraltıcı girdiler (A4-K44). */
    bool disarm = (bitler & IDA_NARROW_DISARM) != 0u || in->plat.link_event == IDA_LINKEV_SESSION_END;
    bool kip_dar = (bitler & IDA_NARROW_MODE) != 0u && kip_daraltici_hedef(in->narrow.mode);
    bool cmd_dar = false;
    for (unsigned j = 0; j < nk; ++j) {
        const ida_cmd_t *c = &in->cmds[j];
        if (daraltici_komut(c)) {
            cmd_dar = true;
            if (c->type == IDA_CMD_DISARM) {
                disarm = true;
            }
            if (c->type == IDA_CMD_SET_MODE && kip_daraltici_hedef(c->u.set_mode.mode)) {
                kip_dar = true;
            }
        }
    }
    /* Emniyet kapanmasının (A4-K111) daraltıcı girdi sayılması çekirdeğin gevşetmeyi
       uygulamasına bağlıdır (gevşetmeden önce alan zaten kısıtlayandır). Gevşetme W4'tedir
       (W2-cekirdek M9); çekirdek bir gevşetmeyi SAFETY_INPUT günlüğüyle göstermediyse
       kapanma yalnız "olası" daraltmadır ve NARROWED beklenmez, ama izinlidir. */
    bool kapanma_kesin = kapanma && g_gevsetme_gozlendi;
    bool daraltma = bitler != 0u || cmd_dar || in->plat.link_event == IDA_LINKEV_SESSION_END || kapanma_kesin;
    /* W3-K9: görev modülünün adım daraltması (arıza eyleminin ağırlaşması, kip düşmesi, kendi
       silahsızlandırması) çağrı kısıtına katılır ve NARROWED A4-K45'in konumunda verilir;
       dışarıdan fs_level artışı, komutsuz kip değişimi ya da DISARM'sız silahsızlanma olarak
       gözlenir. Daraltmanın komutlardan önce mi sonra mı oluştuğu (W3-K13: kip değişimiyle
       yeniden hesap) dışarıdan ayrılamadığı için bu çağrıda NARROWED izinlidir, şart değildir. */
    bool kip_komut_kabul = false;
    for (unsigned j = 0; j < nk; ++j) {
        if (in->cmds[j].type == IDA_CMD_SET_MODE && sonuc_al(o, nbit + j)->ack == IDA_ACK_ACCEPTED) {
            kip_komut_kabul = true;
        }
    }
    bool adim_daraltma = P->var && (o->fs_level > P->fs_level ||
                                    (P->mode != IDA_MODE_INIT && o->status.mode != P->mode && !kip_dar &&
                                     !kip_komut_kabul) ||
                                    (P->armed && !o->armed && !disarm));
    bool daraltma_olasi = !daraltma && (kapanma || adim_daraltma);

    int arm_sayisi = 0;
    int arm_kabul = 0;
    bool kip_kabul[8] = {false, false, false, false, false, false, false, false};
    bool fsr_kabul = false;
    for (unsigned j = 0; j < nk; ++j) {
        const ida_cmd_t *c = &in->cmds[j];
        const ida_cmd_result_t *r = sonuc_al(o, nbit + j);
        KG_DOGRULA(r->cmd_id == c->cmd_id, "cmds[%u] sonucunun kimliği %lu, komutunki %lu (A4-K43)", j,
                   (unsigned long)r->cmd_id, (unsigned long)c->cmd_id);
        if (c->type == IDA_CMD_ARM) {
            arm_sayisi++;
        }
        if (daraltici_komut(c)) {
            /* A4-K46, K114: cmds[]'teki daraltıcı her dönüşte ACCEPTED. */
            KG_DOGRULA(r->ack == IDA_ACK_ACCEPTED, "cmds[%u] daraltıcı (tür %u) ack %u; ACCEPTED bekleniyordu (A4-K46)",
                       j, (unsigned)c->type, (unsigned)r->ack);
            continue;
        }
        if (ret) {
            /* D6 (A4-K114, K26): reddedilen çağrıda daraltıcı olmayan her komut CALL_REJECTED. */
            KG_DOGRULA(r->ack == IDA_ACK_REJECTED && r->reason == IDA_REASON_CALL_REJECTED,
                       "reddedilen çağrıda (dönüş %d) cmds[%u] (tür %u) ack %u neden %u; REJECTED/CALL_REJECTED "
                       "bekleniyordu (A4-K114)",
                       (int)rc, j, (unsigned)c->type, (unsigned)r->ack, (unsigned)r->reason);
            continue;
        }
        int yin = yinelenme(c->cmd_id);
        /* D7 (A4-K48). */
        if (yin == KG_YIN_KESIN) {
            g_sayac.yinelenen++;
            KG_DOGRULA(r->ack == IDA_ACK_DUPLICATE,
                       "cmds[%u] (tür %u, id %lu) son 32 komut içinde; ack %u neden %u, DUPLICATE bekleniyordu (A4-K48)",
                       j, (unsigned)c->type, (unsigned long)c->cmd_id, (unsigned)r->ack, (unsigned)r->reason);
        } else if (yin == KG_YIN_YOK) {
            KG_DOGRULA(r->ack != IDA_ACK_DUPLICATE, "cmds[%u] (tür %u, id %lu) yeni kimlik ama DUPLICATE (A4-K48)", j,
                       (unsigned)c->type, (unsigned long)c->cmd_id);
        }
        /* D6 (A4-K45): daraltıcı girdili kabul edilen çağrıda genişletici NARROWED. */
        if (genisletici_komut(c) && daraltma && r->ack != IDA_ACK_DUPLICATE) {
            KG_DOGRULA(r->ack == IDA_ACK_REJECTED && r->reason == IDA_REASON_NARROWED,
                       "daraltıcı girdili çağrıda genişletici cmds[%u] (tür %u) ack %u neden %u; REJECTED/NARROWED "
                       "bekleniyordu (A4-K45) (mandal 0x%x, cmds daraltıcı %d, link_event %u, emniyet kapanması %d, "
                       "health.narrowed %d, dönüş %d)",
                       j, (unsigned)c->type, (unsigned)r->ack, (unsigned)r->reason, (unsigned)bitler, (int)cmd_dar,
                       (unsigned)in->plat.link_event, (int)kapanma, (int)o->health.narrowed, (int)rc);
        }
        /* D8: ARM sonucunun izinli kümesi. */
        if (c->type == IDA_CMD_ARM) {
            bool sonra_kip = false;
            for (unsigned q = j + 1u; q < nk; ++q) {
                if (in->cmds[q].type == IDA_CMD_SET_MODE && sonuc_al(o, nbit + q)->ack == IDA_ACK_ACCEPTED) {
                    sonra_kip = true;
                }
            }
            uint32_t izin = yin == KG_YIN_KESIN ? KG_IZ_YIN : arm_izinli(in, rc, o, daraltma, sonra_kip);
            if (yin == KG_YIN_BELIRSIZ) {
                izin |= KG_IZ_YIN;
            }
            if (daraltma_olasi && yin != KG_YIN_KESIN) {
                izin |= KG_R(IDA_REASON_NARROWED);
            }
            uint32_t gercek = sonuc_biti(r);
            KG_DOGRULA((izin & gercek) != 0u,
                       "ARM cmds[%u] ack %u neden %u izinli kümede değil (küme 0x%05lx; armed önce %d; est önce %u/%d, "
                       "şimdi %u/%d; fs %u→%u; fs_reasons 0x%lx; estop modeli %d; link %u; pstore %u; param_ok %d; "
                       "events_ok %d; kip %u→%u; imu_lag %d/%d) (3.17, 4.14; görev md.2)",
                       j, (unsigned)r->ack, (unsigned)r->reason, (unsigned long)izin, (int)P->armed, (unsigned)P->est,
                       (int)P->est_var, (unsigned)o->nav.est_level, (int)o->nav_valid, (unsigned)P->fs_level,
                       (unsigned)o->fs_level, (unsigned long)o->status.fs_reasons, (int)M.kisit[0],
                       (unsigned)in->plat.link_state, (unsigned)in->plat.param_store, (int)o->status.param_ok,
                       (int)o->status.events_ok, (unsigned)P->mode, (unsigned)o->status.mode, (int)P->imu_lag,
                       (int)o->health.imu_lag);
            if (r->ack == IDA_ACK_ACCEPTED) {
                arm_kabul++;
            }
        }
        if (r->ack == IDA_ACK_ACCEPTED && c->type == IDA_CMD_SET_MODE) {
            kip_kabul[c->u.set_mode.mode & 7u] = true;
        }
        if (r->ack == IDA_ACK_ACCEPTED && c->type == IDA_CMD_FAILSAFE_RESET) {
            fsr_kabul = true;
        }
        /* Pencere (A4-K48): kabul edilen çağrıdaki daraltıcı olmayan her komut. */
        if (kabul) {
            pencere_ekle(&M.pen_a, c->cmd_id);
            if (r->ack != IDA_ACK_DUPLICATE) {
                pencere_ekle(&M.pen_b, c->cmd_id);
            }
        }
    }

    /* D1 (A4-K104; görev md.3): silahlanma yalnız kabul edilen ARM ile. */
    if (!P->armed && o->armed) {
        KG_DOGRULA(arm_kabul > 0, "armed 0 → 1, ama bu çağrıda ACCEPTED ARM yok (A4-K104; görev md.3)");
    }
    /* D8 sonu: ACCEPTED ARM'dan sonra armed = 1 (3.17 (4); kısıtlayan genişletenden önce). */
    if (arm_kabul > 0) {
        KG_DOGRULA(o->armed, "ARM ACCEPTED ama çağrı sonunda armed = 0 (3.17; görev md.2)");
    }
    /* D4 (A4-K44, K45, K46, K73, K113; görev md.4). */
    if (disarm) {
        KG_DOGRULA(!o->armed, "DISARM ya da SESSION_END taşıyan çağrının sonunda armed = 1 (A4-K46, K73, K113)");
        KG_DOGRULA(arm_kabul == 0, "DISARM ile aynı çağrıdaki ARM ACCEPTED (A4-K45; görev md.4)");
        if (arm_sayisi > 0) {
            g_sayac.disarm_ve_arm++;
        }
    }
    /* D3 (W3-K1): acil durdurma aracı silahsızlandırmaz. DISARM/SESSION_END yoksa ve arıza
       eylemi değişmediyse (DISARM düzeyinde değilse) silahlı araç, acil durdurma kısıtlayanken
       de silahlı kalır. */
    if (M.kisit[0]) {
        if (arm_sayisi > 0) {
            g_sayac.estop_kisitli_arm++;
        }
        if (P->armed && kabul && !disarm && o->fs_level == P->fs_level && o->fs_level < IDA_FS_DISARM) {
            KG_DOGRULA(o->armed, "acil durdurma kısıtlayanken silahlı araç silahsızlandı (W3-K1)");
            if (kapanma) {
                g_sayac.estop_kisitli_silahli_once++;
            }
        }
    }
    /* D2 (A4-K104; görev md.3): GUIDED/AUTO'ya giriş yalnız kabul edilen genişleticiyle. */
    if (o->status.mode != P->mode && (o->status.mode == IDA_MODE_GUIDED || o->status.mode == IDA_MODE_AUTO)) {
        KG_DOGRULA(kip_kabul[o->status.mode & 7u] || fsr_kabul,
                   "kip %u → %u, ama bu çağrıda o hedefe ACCEPTED SET_MODE ya da FAILSAFE_RESET yok (A4-K104; görev md.3)",
                   (unsigned)P->mode, (unsigned)o->status.mode);
        g_sayac.guided_giris++;
    }
    /* D5 (A4-K44, K45, K46, K127). */
    if (kip_dar) {
        KG_DOGRULA(o->status.mode != IDA_MODE_GUIDED && o->status.mode != IDA_MODE_AUTO,
                   "geçerli daraltıcı kip hedefli çağrının sonunda kip %u (A4-K45, K46)", (unsigned)o->status.mode);
    }

    g_sayac.cagri++;
    if (o->armed) {
        g_sayac.silahli_cagri++;
    }
    g_sayac.arm_kabul += (uint32_t)arm_kabul;
    g_sayac.arm_ret += (uint32_t)(arm_sayisi - arm_kabul);
}

ida_rc_t kg_cagir(ida_in_t *in)
{
    ida_rc_t rc;
    KG_DOGRULA(M.kurulu, "örnek kurulmadı");
    M.cagri_no++;
    g_cagri_no_ileti = M.cagri_no;
    memset(&g_out, 0xA5, sizeof g_out);
    if (in->imu_present) {
        rc = ida_core_step(core(), in, &g_out);
    } else {
        rc = ida_core_tick(core(), in, &g_out);
    }
    g_son_in = *in;
    for (unsigned q = 0; q < g_out.n_logs && q < IDA_PORT_MAX_LOGS; ++q) {
        const ida_log_t *l = &g_out.logs[q];
        if (l->code == IDA_LOG_SAFETY_INPUT && (l->arg0 == 0 || l->arg1 != 0)) {
            g_gevsetme_gozlendi = true;
        }
    }
    bool kapanma = emniyet_guncelle(in, rc);
    degismezler(in, rc, &g_out, kapanma);

    /* Model (A4-K25, K72, K10). */
    if (in->plat.link_event == IDA_LINKEV_SESSION_START) {
        M.oturum_baslat = false;
    } else if (in->plat.link_event == IDA_LINKEV_SESSION_END) {
        M.oturum_baslat = true; /* platform yeni oturumda SESSION_START verir (A4-K72) */
    }
    if ((rc == IDA_RC_OK || rc == IDA_RC_IMU_REJECTED) && in->tf.present) {
        M.iz_var = true;
        M.son_seq = in->tf.seq;
        M.son_t = in->tf.t.ns;
        M.seq_sonraki = in->tf.seq + 1u;
    }
    if (in->now_w.ns > M.now) {
        M.now = in->now_w.ns;
    }
    M.P.var = true;
    M.P.armed = g_out.armed;
    M.P.mode = g_out.status.mode;
    M.P.fs_level = g_out.fs_level;
    M.P.fs_reasons = g_out.status.fs_reasons;
    M.P.param_ok = g_out.status.param_ok;
    M.P.imu_lag = g_out.health.imu_lag;
    M.P.est_var = g_out.nav_valid;
    M.P.est = (uint8_t)(g_out.nav_valid ? g_out.nav.est_level : 0u);
    M.P.link = in->plat.link_state == IDA_LINK_RUNNING;
    M.onceki = M.son;
    M.son = g_out;
    M.son_var = true;
    M.son_rc = rc;
    return rc;
}

ida_rc_t kg_adim(void)
{
    return kg_cagir(kg_girdi(KG_OLAGAN));
}

void kg_adimlar(int n)
{
    for (int i = 0; i < n; ++i) {
        (void)kg_adim();
    }
}

const ida_out_t *kg_out(void)
{
    return &M.son;
}

const ida_out_t *kg_onceki(void)
{
    return &M.onceki;
}

const ida_in_t *kg_son_girdi(void)
{
    return &g_son_in;
}

ida_rc_t kg_son_rc(void)
{
    return M.son_rc;
}

const ida_cmd_result_t *kg_sonuc(int j)
{
    unsigned nbit = (unsigned)bit_sayisi((uint32_t)(g_son_in.narrow.mask & IDA_NARROW_DEFINED));
    KG_DOGRULA(j >= 0 && (unsigned)j < g_son_in.n_cmds && nbit + (unsigned)j < M.son.n_results,
               "cmds[%d] için sonuç yok", j);
    return sonuc_al(&M.son, nbit + (unsigned)j);
}

int kg_komut_dizini(uint8_t type)
{
    for (unsigned j = 0; j < g_son_in.n_cmds && j < IDA_PORT_MAX_CMDS; ++j) {
        if (g_son_in.cmds[j].type == type) {
            return (int)j;
        }
    }
    return -1;
}

/* ---- Hazır olma ------------------------------------------------------------------- */

static char g_hazir_neden[160];

bool kg_hazir(void)
{
    const ida_out_t *o = &M.son;
    const kg_dunya_t *d = &M.dunya;
    const char *n = NULL;
    if (!M.son_var || M.son_rc != IDA_RC_OK || !o->nav_valid) {
        n = "son çağrı OK dönen step değil";
    } else if (!est_yeterli(o->nav.est_level)) {
        n = "nav.est_level OK ya da DEGRADED değil";
    } else if (o->status.mode == IDA_MODE_INIT) {
        n = "kip INIT";
    } else if (o->fs_level > IDA_FS_WARN || (o->status.fs_reasons & ~KG_FSR_HAZIR_IZINLI) != 0u) {
        n = "arıza eylemi ya da arıza nedeni var";
    } else if (!o->status.events_ok || !o->status.param_ok) {
        n = "events_ok ya da param_ok 0";
    } else if (o->health.imu_lag || !o->health.data_ok) {
        n = "imu_lag ya da data_ok = 0";
    } else if (d->pstore == IDA_PSTORE_BUSY) {
        n = "parametre kaydı sürüyor";
    }
    if (n != NULL) {
        (void)snprintf(g_hazir_neden, sizeof g_hazir_neden, "%s (est %u, kip %u, fs %u, fs_reasons 0x%lx, "
                                                            "param_ok %d, events_ok %d, imu_lag %d, data_ok %d)",
                       n, (unsigned)o->nav.est_level, (unsigned)o->status.mode, (unsigned)o->fs_level,
                       (unsigned long)o->status.fs_reasons, (int)o->status.param_ok, (int)o->status.events_ok,
                       (int)o->health.imu_lag, (int)o->health.data_ok);
        return false;
    }
    g_hazir_neden[0] = '\0';
    return true;
}

const char *kg_hazir_neden(void)
{
    return g_hazir_neden;
}

bool kg_hazirla(int azami)
{
    int ardisik = 0;
    for (int i = 0; i < azami; ++i) {
        (void)kg_adim();
        if (kg_hazir()) {
            if (++ardisik >= 10) {
                return true;
            }
        } else {
            ardisik = 0;
        }
    }
    return false;
}

void kg_hazir_olmali(int azami, const char *amac)
{
    bool hazir = kg_hazirla(azami);
    KG_DOGRULA(hazir,
               "%s: silahlandırmanın gözlenen ön koşulları %d adımda (%.1f s) sağlanmadı: %s. Bu sınama kestirimin "
               "OK ya da DEGRADED düzeyine, kipin INIT dışına çıkmasına ve arıza nedeni kalmamasına dayanır (W3-K3)",
               amac, azami, (double)azami * 0.005, kg_hazir_neden());
}

void kg_kip_sec_konumlu(uint8_t mode)
{
    /* W3-K5: konum gerektiren kip (STATION_KEEP) yatay geçerlilik ister; geçerlilik yokken
       kipe girilmez ya da kip konum gerektirmeyen bir kipe iner (hedef kip sözleşmede yazılı
       değil, sabitlenmez). Daraltıcı istek her durumda GUIDED/AUTO'ya götürmez (A4-K45, K46). */
    ida_in_t *in = kg_girdi(KG_OLAGAN);
    kg_mandal(in, (uint8_t)IDA_NARROW_MODE, mode);
    ida_rc_t rc = kg_cagir(in);
    KG_DOGRULA(rc == IDA_RC_OK, "kip seçimi çağrısı %d döndü", (int)rc);
    uint8_t m = kg_out()->status.mode;
    KG_DOGRULA(m != IDA_MODE_GUIDED && m != IDA_MODE_AUTO && m != IDA_MODE_INIT,
               "NARROW_MODE %u sonrası kip %u (A4-K46, K127; W3-K5)", (unsigned)mode, (unsigned)m);
}

void kg_kip_sec(uint8_t mode)
{
    ida_in_t *in = kg_girdi(KG_OLAGAN);
    kg_mandal(in, (uint8_t)IDA_NARROW_MODE, mode);
    ida_rc_t rc = kg_cagir(in);
    KG_DOGRULA(rc == IDA_RC_OK, "kip seçimi çağrısı %d döndü", (int)rc);
    KG_DOGRULA(kg_out()->status.mode == mode, "NARROW_MODE %u sonrası kip %u (A4-K46, K127)", (unsigned)mode,
               (unsigned)kg_out()->status.mode);
}

void kg_guided(void)
{
    /* W3-K5, K7: GUIDED'e giriş kestirim düzeyine değil tutum, yön ve derinlik bileşenlerinin
       geçerliliğine bakar; bu dışarıdan yalnız SET_MODE(GUIDED)'ın sonucuyla gözlenir. Hazır
       durumda en çok 2 s boyunca her adımda yeni kimlikle denenir; ret yalnız BAD_MODE
       olabilir (3.17 SET_MODE satırı). */
    for (int deneme = 0; deneme < 400; ++deneme) {
        ida_in_t *in = kg_girdi(KG_OLAGAN);
        ida_cmd_t *c = kg_kip_komutu(in, (uint8_t)IDA_MODE_GUIDED);
        int j = (int)(c - in->cmds);
        ida_rc_t rc = kg_cagir(in);
        const ida_cmd_result_t *r = kg_sonuc(j);
        KG_DOGRULA(rc == IDA_RC_OK, "SET_MODE(GUIDED) çağrısı %d döndü", (int)rc);
        if (r->ack == IDA_ACK_ACCEPTED) {
            KG_DOGRULA(kg_out()->status.mode == IDA_MODE_GUIDED, "SET_MODE(GUIDED) ACCEPTED ama kip %u",
                       (unsigned)kg_out()->status.mode);
            return;
        }
        KG_DOGRULA(r->ack == IDA_ACK_REJECTED && r->reason == IDA_REASON_BAD_MODE,
                   "SET_MODE(GUIDED) ack %u neden %u; ACCEPTED ya da BAD_MODE bekleniyordu (3.17)", (unsigned)r->ack,
                   (unsigned)r->reason);
    }
    kg_basarisiz(__FILE__, __LINE__, "hazır durumda SET_MODE(GUIDED) 2 s boyunca BAD_MODE aldı (3.17; W3-K7)");
}

uint32_t kg_silahlan(void)
{
    ida_in_t *in = kg_girdi(KG_OLAGAN);
    ida_cmd_t *c = kg_komut(in, (uint8_t)IDA_CMD_ARM);
    uint32_t id = c->cmd_id;
    int j = (int)(c - in->cmds);
    ida_rc_t rc = kg_cagir(in);
    const ida_cmd_result_t *r = kg_sonuc(j);
    KG_DOGRULA(rc == IDA_RC_OK && r->ack == IDA_ACK_ACCEPTED && r->reason == IDA_REASON_NONE,
               "hazır durumda ARM: dönüş %d, ack %u, neden %u; ACCEPTED bekleniyordu (3.17 (4); görev md.2)", (int)rc,
               (unsigned)r->ack, (unsigned)r->reason);
    KG_DOGRULA(kg_out()->armed, "ARM ACCEPTED ama armed = 0 (3.17)");
    return id;
}

void kg_silahsizlan(void)
{
    ida_in_t *in = kg_girdi(KG_OLAGAN);
    kg_mandal(in, (uint8_t)IDA_NARROW_DISARM, 0u);
    (void)kg_cagir(in);
    KG_DOGRULA(!kg_out()->armed, "DISARM mandalı sonrası armed = 1 (A4-K46)");
}

/* ---- Anlık görüntü ---------------------------------------------------------------- */

void kg_anlik_al(void)
{
    memcpy(g_anlik_bellek, g_havuz.b, M.boy);
    g_anlik_M = M;
}

void kg_anlik_don(void)
{
    M = g_anlik_M;
    memcpy(g_havuz.b, g_anlik_bellek, M.boy);
}
