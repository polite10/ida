/*
 * tests/kabul/atis — düzenek: dünya, sahte alıcı, model ve her çağrıda denetlenen emniyet
 * değişmezleri. Açıklama başlıkta (kabul_atis.h).
 *
 * Modelin dayandığı kurallar (yalnız sözleşme ve Ek):
 *   - etkin emniyet değerleri: A4-K116, K130, K128; SESSION_END'de kısıtlayana döner (A4-K73);
 *     bayatlık: G-Ç29 (iki ölçüt; kesin büyük);
 *   - izin: G-Ç11 (APPLIED alınmış ve olayı çıkaran çağrının now_w'sinden hold_ns dolmamış);
 *     sonradan çıkan SAFETY_CLOSE (tüp ya da ALL) çekirdeğin o izni kapattığını gösterir;
 *   - yetki: bir FIRE(t), son yetki kesiminden sonra kabul edilmiş ve henüz tüketilmemiş bir
 *     FC_FIRE(t)'ye bağlıdır (G-Ç21, W5-K3). Yetki kesimleri yalnız kesin olanlardır:
 *     FC_DISARM/FC_ABORT (W5-K7: bütün tüpler), SESSION_END (G-Ç18), etkin acil durdurmanın
 *     kısıtlayana dönmesi ya da bayatlık (G-Ç29: FC güvenli duruma), tüpün fiş/piminin
 *     kısıtlayana dönmesi (G-Ç19), tüpün SAFETY_OPEN'ının APPLIED dışı sonucu ya da
 *     sonucunun bilinmeyen kalması (G-Ç23).
 */
#include "kabul_atis.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "unity.h"

void setUp(void)
{
}

static char g_ka_on[160];

void ka_baglam_on(const char *on)
{
    if (on == NULL) {
        g_ka_on[0] = '\0';
    } else {
        (void)snprintf(g_ka_on, sizeof g_ka_on, "%s", on);
    }
}

static void iz_yazdir(void);

void tearDown(void)
{
    if (Unity.CurrentTestFailed != 0u) {
        if (g_ka_on[0] != '\0') {
            (void)printf("kabul_atis: düşen dizi: %s\n", g_ka_on);
        }
        iz_yazdir();
    }
    g_ka_on[0] = '\0';
}

/* ---- Hata iletisi ve bağlam ----------------------------------------------------------- */

static char g_ka_baglam[256];

void ka_baglam(const char *bicim, ...)
{
    va_list ap;
    va_start(ap, bicim);
    (void)vsnprintf(g_ka_baglam, sizeof g_ka_baglam, bicim, ap);
    va_end(ap);
}

_Noreturn void ka_basarisiz(int satir, const char *bicim, ...)
{
    static char govde[640];
    va_list ap;
    va_start(ap, bicim);
    (void)vsnprintf(govde, sizeof govde, bicim, ap);
    va_end(ap);
    kc_basarisiz(satir, "%s {%s%s%s}", govde, g_ka_on, g_ka_on[0] != '\0' ? "; " : "", g_ka_baglam);
}

/* ---- Test tablosu --------------------------------------------------------------------- */

int ka_main(int argc, char **argv, const char *dosya, const ka_test_t *t, size_t n)
{
    int bulunan = 0;
    UnityBegin(dosya);
    for (size_t i = 0; i < n; ++i) {
        if (argc > 1 && strcmp(argv[1], t[i].ad) != 0) {
            continue;
        }
        ++bulunan;
        UnityDefaultTestRun(t[i].fn, t[i].ad, (UNITY_LINE_TYPE)__LINE__);
    }
    if (argc > 1 && bulunan == 0) {
        (void)fprintf(stderr, "kabul_atis: '%s' adlı test yok\n", argv[1]);
        return 2;
    }
    return UnityEnd();
}

/* ---- Ayarlar ---------------------------------------------------------------------------- */

ka_ayar_t ka_ayar;

void ka_ayar_varsayilan(void)
{
    memset(&ka_ayar, 0, sizeof ka_ayar);
    ka_ayar.sensorler = true;
    ka_ayar.heartbeat = true;
    ka_ayar.emniyet = true;
    ka_ayar.estop = 0u;
    ka_ayar.tup[0] = KC_TUP_ACIK;
    ka_ayar.tup[1] = KC_TUP_ACIK;
    ka_ayar.link_state = (uint8_t)IDA_LINK_RUNNING;
    for (unsigned k = 0; k < KA_TUR_SAYISI; ++k) {
        ka_ayar.politika[k].kod = (uint8_t)IDA_EVR_APPLIED;
        ka_ayar.politika[k].gecikme = 1u;
        ka_ayar.politika[k].uygula = true;
    }
}

/* ---- Olay geçmişi --------------------------------------------------------------------- */

#define KA_HALKA 8192u
#define KA_BEKLEYEN 256u
#define KA_TUP_OPEN 8u

static ka_olay_t g_olay[KA_HALKA];
static size_t g_olay_n;    /* toplam çıkan olay */

typedef struct {
    size_t i;              /* halka dizini (mutlak sıra no) */
    uint64_t id;
} ka_ref_t;

/* Sonucu henüz verilmemiş olaylar (planlanmış ya da kayıp). */
static ka_ref_t g_bekleyen[KA_BEKLEYEN];
static size_t g_bekleyen_n;
/* Elle eklenen ya da bu çağrıya konan sonuçların izi (teslim edilenleri işaretlemek için). */
static ka_ref_t g_bu_cagri_sonuc[IDA_PORT_MAX_EVRES];
static size_t g_bu_cagri_sonuc_n;

static ka_olay_t *olay_ref(size_t mutlak)
{
    if (mutlak >= g_olay_n || g_olay_n - mutlak > KA_HALKA) {
        return NULL;
    }
    return &g_olay[mutlak % KA_HALKA];
}

size_t ka_olay_sayisi(void)
{
    return g_olay_n;
}

const ka_olay_t *ka_olay_no(size_t i)
{
    return olay_ref(i);
}

const ka_olay_t *ka_son_olay(uint8_t kind, int tube)
{
    size_t taban = g_olay_n > KA_HALKA ? g_olay_n - KA_HALKA : 0u;
    for (size_t k = g_olay_n; k > taban; --k) {
        const ka_olay_t *o = olay_ref(k - 1u);
        if (o != NULL && o->e.kind == kind && (tube < 0 || o->e.tube == (uint8_t)tube)) {
            return o;
        }
    }
    return NULL;
}

const ka_olay_t *ka_olay_kimlikle(uint64_t event_id)
{
    size_t taban = g_olay_n > KA_HALKA ? g_olay_n - KA_HALKA : 0u;
    for (size_t k = g_olay_n; k > taban; --k) {
        const ka_olay_t *o = olay_ref(k - 1u);
        if (o != NULL && o->e.event_id == event_id) {
            return o;
        }
    }
    return NULL;
}

/* ---- Model ------------------------------------------------------------------------------ */

typedef struct {
    bool kurulu;
    /* etkin emniyet değerleri (A4-K116): true = kısıtlayan */
    bool estop_taban;                 /* bayatlık hariç */
    bool fis[IDA_PORT_N_TUBE];
    bool pim[IDA_PORT_N_TUBE];
    bool bayat;                        /* G-Ç29: kesin bayat */
    bool kabul_var;                    /* init/SESSION_END'den beri inst 0 kabul edildi */
    bool iz_var;                       /* son kabul edilen inst 0 izi (SESSION_START sıfırlar) */
    uint32_t iz_seq;
    int64_t iz_tmeas;
    int64_t son_rx;                    /* son kabul edilen inst 0'ın t_rx'i */
    int64_t tmeas_ilerleme;            /* t_meas'in son ilerlediği örneğin t_rx'i */
    int64_t referans;                  /* init / SESSION_START anı */
    int64_t tmeas_max;
    /* yetki */
    int fire_istek[IDA_PORT_N_TUBE];   /* kabul edilmiş, FIRE'a dönüşmemiş FC_FIRE sayısı */
    bool yetki[IDA_PORT_N_TUBE];       /* son kesimden beri kabul edilmiş FC_FIRE var (FIRE sonucu gelene dek) */
    /* izin: tüp başına son SAFETY_OPEN'lar (mutlak olay no) */
    size_t open_i[IDA_PORT_N_TUBE][KA_TUP_OPEN];
    size_t open_n[IDA_PORT_N_TUBE];
    /* bilinmeyen sonuç süreleri (kayıp sonuçlu FIRE ve SAFETY_CLOSE; G-Ç42, G-Ç49, G-Ç50) */
    ka_ref_t kayip[64];
    bool kayip_tamam[64];
    size_t kayip_n;
    /* (g): B sınıfı emniyet parametrelerinin kimlikleri */
    int bparam[8];
    size_t bparam_n;
} ka_model_t;

static ka_model_t K;
static int64_t g_T_cagri; /* bu çağrıda geçerli T_emn_bayat (commit adım sonunda uygulanır) */
static bool g_degismez = true;
static ka_kapsam_t g_kapsam;

/* Alıcının (sim) durumu. */
typedef struct {
    int64_t izin_bitis[IDA_PORT_N_TUBE]; /* sim zamanı; < : izin açık */
    bool atildi[IDA_PORT_N_TUBE];        /* FIRE uygulandı: LOADED = 0 */
} ka_alici_t;

static ka_alici_t A;

/* Dünya. */
static ida_in_t g_in;
static bool g_in_hazir;
static bool g_in_sonuclu;   /* bu girdiye alıcı sonuçları kondu */
static uint32_t g_cmd_id;
static uint32_t g_sseq;
static uint32_t g_eseq;
static int64_t g_son_emniyet_gonderim;
static int64_t g_son_tmeas;
static uint64_t g_cagri;
static int64_t g_simdi;
static ida_out_t g_onceki;
static bool g_onceki_var;
static ida_init_in_t g_init_in;
static int g_n_ornek;

void ka_degismez_ac(bool acik)
{
    g_degismez = acik;
}

const ka_kapsam_t *ka_kapsam(void)
{
    return &g_kapsam;
}

bool ka_model_estop(void)
{
    return K.estop_taban || K.bayat;
}

bool ka_model_fis_cikik(uint8_t tube)
{
    return tube < IDA_PORT_N_TUBE && K.fis[tube];
}

bool ka_model_pim_takili(uint8_t tube)
{
    return tube < IDA_PORT_N_TUBE && K.pim[tube];
}

bool ka_model_bayat(void)
{
    return K.bayat;
}

bool ka_alici_izin(uint8_t tube, int64_t t)
{
    return tube < IDA_PORT_N_TUBE && t < A.izin_bitis[tube];
}

/* ---- Parametreler --------------------------------------------------------------------- */

int ka_param_id(const char *ad)
{
    uint16_t id = 0;
    if (ida_core_param_find(ad, &id) != IDA_RC_OK) {
        return -1;
    }
    return (int)id;
}

uint32_t ka_param_u32(const char *ad, uint32_t yoksa)
{
    int id = ka_param_id(ad);
    ida_param_value_t v;
    if (id < 0) {
        return yoksa;
    }
    memset(&v, 0, sizeof v);
    if (ida_core_param_get(kc_core(0), (uint16_t)id, &v) != IDA_RC_OK) {
        return yoksa;
    }
    return kc_param_ham(&v);
}

int64_t ka_t_emn_bayat(void)
{
    uint32_t ms = ka_param_u32("T_emn_bayat", (uint32_t)KA_T_EMN_BAYAT_VARSAYILAN_MS);
    return (int64_t)ms * KA_MS;
}

static void bparam_kur(void)
{
    static const char *const adlar[] = {"FC_APPLY_BY_NS", "FC_HOLD_NS", "T_emn_bayat", "T_hb_fc", "T_pencere"};
    K.bparam_n = 0;
    for (size_t i = 0; i < sizeof adlar / sizeof adlar[0]; ++i) {
        int id = ka_param_id(adlar[i]);
        if (id >= 0 && K.bparam_n < sizeof K.bparam / sizeof K.bparam[0]) {
            K.bparam[K.bparam_n++] = id;
        }
    }
}

/* ---- Kurulum ---------------------------------------------------------------------------- */

static void model_sifirla(void)
{
    memset(&K, 0, sizeof K);
    memset(&A, 0, sizeof A);
    K.kurulu = true;
    K.estop_taban = true;
    for (unsigned i = 0; i < IDA_PORT_N_TUBE; ++i) {
        K.fis[i] = true;
        K.pim[i] = true;
    }
    K.referans = KC_T0;
    g_olay_n = 0;
    g_bekleyen_n = 0;
    g_bu_cagri_sonuc_n = 0;
    g_in_hazir = false;
    g_onceki_var = false;
    g_son_emniyet_gonderim = INT64_MIN;
    g_son_tmeas = -1;
    g_simdi = KC_T0;
    bparam_kur();
}

void ka_kur(int n_ornek)
{
    g_degismez = true;
    ka_ayar_varsayilan();
    kc_ortam_kur(n_ornek);
    g_n_ornek = n_ornek;
    kc_init_gecerli();
    kc_init_girdi(&g_init_in);
    g_cmd_id = 1000u;
    g_sseq = 1u;
    g_eseq = 1u;
    g_cagri = 0u;
    memset(&g_kapsam, 0, sizeof g_kapsam);
    model_sifirla();
    ka_baglam("kurulum");
}

ida_rc_t ka_kur_init(int n_ornek, const ida_init_in_t *ii)
{
    g_degismez = true;
    ka_ayar_varsayilan();
    kc_ortam_kur(n_ornek);
    g_n_ornek = n_ornek;
    g_init_in = *ii;
    ida_rc_t rc = kc_init(ii);
    g_cmd_id = 1000u;
    g_sseq = 1u;
    g_eseq = 1u;
    g_cagri = 0u;
    memset(&g_kapsam, 0, sizeof g_kapsam);
    model_sifirla();
    return rc;
}

ida_rc_t ka_yeniden_init(void)
{
    /* Aynı örnek belleklerinde (A4-K77): geçerli blokla init'in eşdeğeri. */
    int n = g_n_ornek;
    (void)n;
    kc_init_gecerli();
    model_sifirla();
    g_sseq = 1u;
    g_eseq = 1u;
    ka_ayar_varsayilan();
    /* now_w izinin geri gitmemesi için init'in now_w'si son çağrınınkinden küçük olamaz; kc
       düzeneği init'i KC_T0 ile yapar, bu yüzden sonraki çağrılar oradan sürer. */
    return IDA_RC_OK;
}

/* ---- Komutlar ------------------------------------------------------------------------- */

uint32_t ka_yeni_id(void)
{
    return g_cmd_id++;
}

uint32_t ka_komut(ida_in_t *in, uint8_t type)
{
    uint32_t id = ka_yeni_id();
    (void)kc_komut_ekle(in, id, type);
    return id;
}

uint32_t ka_fc(ida_in_t *in, uint8_t op, uint8_t tube)
{
    uint32_t id = ka_yeni_id();
    (void)kc_fc_ekle(in, id, op, tube);
    return id;
}

uint32_t ka_mandal(ida_in_t *in, uint8_t bit)
{
    uint32_t id = ka_yeni_id();
    for (unsigned i = 0; i < IDA_PORT_N_NARROW; ++i) {
        if (bit == (uint8_t)(1u << i)) {
            in->narrow.mask = (uint8_t)(in->narrow.mask | bit);
            in->narrow.cmd_id[i] = id;
        }
    }
    if (bit == IDA_NARROW_MODE && in->narrow.mode == 0u) {
        in->narrow.mode = (uint8_t)IDA_MODE_STATION_KEEP;
    }
    return id;
}

ida_cmd_t *ka_son_komut(ida_in_t *in)
{
    unsigned n = in->n_cmds;
    if (n == 0u || n > IDA_PORT_MAX_CMDS) {
        ka_basarisiz(__LINE__, "ka_son_komut: n_cmds %u", n);
    }
    return &in->cmds[(n - 1u) % IDA_PORT_MAX_CMDS];
}

ida_sample_t *ka_emniyet_ekle(ida_in_t *in, uint8_t inst, uint8_t estop, uint8_t t0, uint8_t t1)
{
    int64_t t = in->tf.present ? in->tf.t.ns : in->now_w.ns;
    ida_sample_t *s = kc_emniyet_ekle(in, inst, g_eseq++, t, estop, t0, t1);
    s->hdr.t_rx.ns = in->now_w.ns;
    if (inst == 0u) {
        g_son_tmeas = t;
    }
    return s;
}

const ida_cmd_result_t *ka_sonuc(uint32_t cmd_id)
{
    const ida_out_t *o = kc_out();
    for (unsigned i = 0; i < o->n_results && i < IDA_PORT_MAX_RESULTS; ++i) {
        if (o->results[i].cmd_id == cmd_id) {
            return &o->results[i];
        }
    }
    return NULL;
}

static const char *ack_adi(uint8_t a)
{
    switch (a) {
    case IDA_ACK_ACCEPTED: return "ACCEPTED";
    case IDA_ACK_REJECTED: return "REJECTED";
    case IDA_ACK_IN_PROGRESS: return "IN_PROGRESS";
    case IDA_ACK_DUPLICATE: return "DUPLICATE";
    case IDA_ACK_UNSUPPORTED: return "UNSUPPORTED";
    default: return "?";
    }
}

static const char *neden_adi(uint16_t r)
{
    static const char *const ad[16] = {"NONE",     "CALL_REJECTED", "ARMED",    "NOT_ARMABLE",
                                       "PARAM_BUSY", "PARAM_INVALID", "OUT_OF_RANGE", "BAD_MODE",
                                       "EVENTS_OFF", "SESSION",       "NARROWED", "IMU_LAG",
                                       "IMU_REJECTED", "ONE_PER_CALL", "NOT_INIT", "COALESCED"};
    return r < 16u ? ad[r] : "?";
}

const char *ka_durum_adi(uint8_t s)
{
    static const char *const ad[6] = {"SAFE", "ARMED", "PERMIT_OPEN", "FIRE_PENDING", "TEST", "FAULT_LATCHED"};
    return s < 6u ? ad[s] : "?";
}

void ka_sonuc_bekle(int satir, uint32_t cmd_id, uint8_t ack, uint16_t reason, const char *ne)
{
    const ida_cmd_result_t *r = ka_sonuc(cmd_id);
    const ida_out_t *o = kc_out();
    if (r == NULL) {
        ka_basarisiz(satir, "%s: cmd_id %lu için sonuç yok", ne, (unsigned long)cmd_id);
    }
    if (r->ack != ack || r->reason != reason) {
        ka_basarisiz(satir, "%s: sonuç %s/%s, beklenen %s/%s (fc.state %s, inhibit 0x%05lx)", ne, ack_adi(r->ack),
                     neden_adi(r->reason), ack_adi(ack), neden_adi(reason), ka_durum_adi(o->fc.state),
                     (unsigned long)o->fc.inhibit_mask);
    }
}

/* ---- Çıktı yardımcıları --------------------------------------------------------------- */

int ka_olay_say(const ida_out_t *o, uint8_t kind, int tube)
{
    int n = 0;
    for (unsigned i = 0; i < o->n_events && i < IDA_PORT_MAX_EVENTS; ++i) {
        if (o->events[i].kind == kind && (tube < 0 || o->events[i].tube == (uint8_t)tube)) {
            ++n;
        }
    }
    return n;
}

const ida_event_t *ka_olay_bul(const ida_out_t *o, uint8_t kind, int tube)
{
    for (unsigned i = 0; i < o->n_events && i < IDA_PORT_MAX_EVENTS; ++i) {
        if (o->events[i].kind == kind && (tube < 0 || o->events[i].tube == (uint8_t)tube)) {
            return &o->events[i];
        }
    }
    return NULL;
}

bool ka_kapanma_var(const ida_out_t *o, uint8_t tube)
{
    return ka_olay_say(o, (uint8_t)IDA_EVT_SAFETY_CLOSE, (int)tube) > 0 ||
           ka_olay_say(o, (uint8_t)IDA_EVT_SAFETY_CLOSE, (int)IDA_TUBE_ALL) > 0;
}

bool ka_genisletici_olay_var(const ida_out_t *o)
{
    return ka_olay_say(o, (uint8_t)IDA_EVT_SAFETY_OPEN, -1) + ka_olay_say(o, (uint8_t)IDA_EVT_FIRE, -1) > 0;
}

const ida_out_t *ka_out(void)
{
    return kc_out();
}

const ida_out_t *ka_onceki(void)
{
    return kc_onceki_out();
}

uint64_t ka_cagri_no(void)
{
    return g_cagri;
}

int64_t ka_simdi(void)
{
    return g_simdi;
}

/* ---- Alıcı ------------------------------------------------------------------------------ */

static void bekleyen_ekle(size_t mutlak, uint64_t id)
{
    if (g_bekleyen_n < KA_BEKLEYEN) {
        g_bekleyen[g_bekleyen_n].i = mutlak;
        g_bekleyen[g_bekleyen_n].id = id;
        ++g_bekleyen_n;
    }
}

static void bekleyen_cikar(size_t k)
{
    if (k < g_bekleyen_n) {
        g_bekleyen[k] = g_bekleyen[g_bekleyen_n - 1u];
        --g_bekleyen_n;
    }
}

void ka_sonuc_ekle(ida_in_t *in, uint64_t event_id, uint8_t kod, int64_t t_apply)
{
    KA_DOGRULA(in->n_evres < IDA_PORT_MAX_EVRES, "evres dizisi dolu");
    ida_event_result_t *r = &in->evres[in->n_evres];
    in->n_evres = (uint8_t)(in->n_evres + 1u);
    r->event_id = event_id;
    r->result = kod;
    r->t_apply.ns = t_apply;
    /* Geçmişte bekleyen bir olaysa: sonuç bu çağrıda verilmiş sayılır (model). */
    for (size_t k = 0; k < g_bekleyen_n; ++k) {
        ka_olay_t *o = olay_ref(g_bekleyen[k].i);
        if (o != NULL && o->e.event_id == event_id && o->e.event_id == g_bekleyen[k].id &&
            g_bu_cagri_sonuc_n < IDA_PORT_MAX_EVRES && (kod >= 1u && (kod <= 6u || (kod >= 16u && kod <= 19u)))) {
            o->sonuc = kod;
            o->t_apply = t_apply;
            g_bu_cagri_sonuc[g_bu_cagri_sonuc_n++] = g_bekleyen[k];
            bekleyen_cikar(k);
            return;
        }
    }
}

void ka_sonuc_planla(uint64_t event_id, uint8_t kod, uint32_t gecikme)
{
    for (size_t k = 0; k < g_bekleyen_n; ++k) {
        ka_olay_t *o = olay_ref(g_bekleyen[k].i);
        if (o != NULL && o->e.event_id == event_id) {
            o->sonuc = kod;
            o->teslim_cagri = g_cagri + (gecikme == 0u ? 1u : gecikme);
            o->t_apply = (kod == IDA_EVR_APPLIED || kod == IDA_EVR_DUPLICATE) ? o->e.t_decide.ns + KA_MS : 0;
            return;
        }
    }
    ka_basarisiz(__LINE__, "ka_sonuc_planla: olay %llx bekleyenlerde yok", (unsigned long long)event_id);
}

/* Alıcı olayı uygular (sim davranışı; A1-K85 sırasının sade karşılığı). */
static void alici_uygula(const ka_olay_t *o)
{
    if (o->e.kind == IDA_EVT_SAFETY_OPEN && o->e.tube < IDA_PORT_N_TUBE) {
        A.izin_bitis[o->e.tube] = o->t_apply + (int64_t)o->e.hold_ns;
    } else if (o->e.kind == IDA_EVT_SAFETY_CLOSE) {
        for (unsigned t = 0; t < IDA_PORT_N_TUBE; ++t) {
            if (o->e.tube == IDA_TUBE_ALL || o->e.tube == t) {
                A.izin_bitis[t] = 0;
            }
        }
    } else if (o->e.kind == IDA_EVT_FIRE && o->e.tube < IDA_PORT_N_TUBE) {
        A.atildi[o->e.tube] = true;
    }
}

/* ---- Girdi kurma ------------------------------------------------------------------------ */

static void sensorleri_ekle(ida_in_t *in, int64_t t)
{
    ida_sample_t *s;
    uint32_t q = g_sseq++;
    s = kc_ornek_ekle(in, (uint8_t)IDA_SENS_BARO, 0u, q, t);
    s->hdr.t_rx.ns = in->now_w.ns;
    s = kc_ornek_ekle(in, (uint8_t)IDA_SENS_MAG, 0u, q, t);
    s->hdr.t_rx.ns = in->now_w.ns;
    s = kc_ornek_ekle(in, (uint8_t)IDA_SENS_GNSS, 0u, q, t);
    s->hdr.t_rx.ns = in->now_w.ns;
    s->u.gnss.lat_e7 = 410000000;
    s->u.gnss.lon_e7 = 290000000;
    s->u.gnss.alt_mm = 0;
    s->u.gnss.fix_type = (uint8_t)IDA_GNSS_FIX_3D;
    s->u.gnss.n_sv = 12u;
    s->u.gnss.eph = 1.0f;
    s->u.gnss.epv = 2.0f;
    s = kc_ornek_ekle(in, (uint8_t)IDA_SENS_DVL, 0u, q, t);
    s->hdr.t_rx.ns = in->now_w.ns;
    s->u.dvl.dvl_flags = (uint8_t)(IDA_DVL_VEL_VALID | IDA_DVL_TRACK_BOTTOM | IDA_DVL_COV_PRESENT);
    s->u.dvl.cov[0] = 1.0e-4f;
    s->u.dvl.cov[3] = 1.0e-4f;
    s->u.dvl.cov[5] = 1.0e-4f;
    s->u.dvl.fom = 0.01f;
    for (unsigned b = 0; b < 4u; ++b) {
        s->u.dvl.beam_dist[b] = 10.0f;
    }
    s->u.dvl.altitude = 10.0f;
    s->u.dvl.beam_valid = 0x0Fu;
    s = kc_ornek_ekle(in, (uint8_t)IDA_SENS_LEAK, 0u, q, t);
    s->hdr.t_rx.ns = in->now_w.ns;
    s->u.leak.mask = ka_ayar.sizinti;
    s = kc_ornek_ekle(in, (uint8_t)IDA_SENS_BATT, 0u, q, t);
    s->hdr.t_rx.ns = in->now_w.ns;
    s->hdr.window_ns = (uint32_t)(10 * KA_MS);
    s->u.batt.voltage = 16.0f;
    s->u.batt.current = 1.0f;
    s->u.batt.temp_c = 25.0f;
}

static uint8_t geri_okuma(unsigned tube, int64_t t)
{
    uint8_t b = (uint8_t)(ka_ayar.tup[tube] & (IDA_TUBE_LOADED | IDA_TUBE_PLUG_IN | IDA_TUBE_PIN_OUT));
    if (!ka_ayar.loaded_elle && A.atildi[tube]) {
        b = (uint8_t)(b & (uint8_t)~IDA_TUBE_LOADED);
    }
    if (ka_ayar.permit_elle) {
        if (ka_ayar.permit_deger[tube] != 0u) {
            b = (uint8_t)(b | IDA_TUBE_PERMIT_OPEN);
        }
    } else if (ka_alici_izin((uint8_t)tube, t)) {
        b = (uint8_t)(b | IDA_TUBE_PERMIT_OPEN);
    }
    return b;
}

static void sonuclari_koy(ida_in_t *in)
{
    g_bu_cagri_sonuc_n = 0;
    if (ka_ayar.sonuclari_tut) {
        return;
    }
    /* Teslim zamanı gelenler, çıkış sırasıyla (eski önce). */
    for (;;) {
        if (in->n_evres >= IDA_PORT_MAX_EVRES) {
            break;
        }
        size_t en = KA_BEKLEYEN;
        size_t en_i = 0;
        for (size_t k = 0; k < g_bekleyen_n; ++k) {
            ka_olay_t *o = olay_ref(g_bekleyen[k].i);
            if (o == NULL || o->e.event_id != g_bekleyen[k].id) {
                continue;
            }
            if (o->sonuc == KA_SONUC_YOK || o->teslim_cagri > g_cagri + 1u) {
                continue;
            }
            if (en == KA_BEKLEYEN || g_bekleyen[k].i < en_i) {
                en = k;
                en_i = g_bekleyen[k].i;
            }
        }
        if (en == KA_BEKLEYEN) {
            break;
        }
        ka_olay_t *o = olay_ref(g_bekleyen[en].i);
        size_t once_n = g_bekleyen_n;
        ka_sonuc_ekle(in, o->e.event_id, o->sonuc, o->t_apply); /* bekleyenden çıkarır */
        if (g_bekleyen_n == once_n) {
            bekleyen_cikar(en); /* tanımsız kod: yalnız kuyruktan düşer */
        }
    }
}

static ida_in_t *girdi_kur(bool seq_elle, uint32_t seq_deger)
{
    ida_in_t *in = &g_in;
    int64_t t = kc_sonraki_t();
    uint32_t seq = seq_elle ? seq_deger : kc_sonraki_seq();
    if (ka_ayar.izgara) {
        kc_in_izgara(in, t);
    } else if (ka_ayar.tick) {
        kc_in_tick(in, seq, t);
    } else {
        kc_in_step(in, seq, t);
        if (ka_ayar.imu_bozuk) {
            in->imu.hdr.flags = 0u;
        }
        if (ka_ayar.imu_gecikme > 0) {
            in->imu.hdr.t_meas.ns = t - ka_ayar.imu_gecikme;
        }
    }
    in->plat.link_state = ka_ayar.link_state;
    if (!ka_ayar.izgara) {
        if (ka_ayar.sensorler) {
            sensorleri_ekle(in, t);
        }
        if (ka_ayar.emniyet &&
            (ka_ayar.emniyet_aralik <= 0 || g_son_emniyet_gonderim == INT64_MIN ||
             t - g_son_emniyet_gonderim >= ka_ayar.emniyet_aralik)) {
            int64_t tm = (ka_ayar.tmeas_dondur && g_son_tmeas >= 0) ? g_son_tmeas : t;
            ida_sample_t *s = kc_emniyet_ekle(in, 0u, g_eseq++, tm, ka_ayar.estop, geri_okuma(0u, t), geri_okuma(1u, t));
            s->hdr.t_rx.ns = t;
            g_son_tmeas = tm;
            g_son_emniyet_gonderim = t;
        }
        if (ka_ayar.heartbeat) {
            (void)ka_komut(in, (uint8_t)IDA_CMD_HEARTBEAT);
        }
    }
    sonuclari_koy(in);
    g_in_hazir = true;
    g_in_sonuclu = g_bu_cagri_sonuc_n > 0u;
    return in;
}

ida_in_t *ka_girdi(void)
{
    return girdi_kur(false, 0u);
}

ida_in_t *ka_girdi_seq(uint32_t seq)
{
    bool tut = ka_ayar.sonuclari_tut;
    ka_ayar.sonuclari_tut = true; /* reddedilecek çağrıya sonuç konmaz */
    ida_in_t *in = girdi_kur(true, seq);
    ka_ayar.sonuclari_tut = tut;
    return in;
}

/* ---- Modelin güncellenmesi ---------------------------------------------------------------- */

static bool seri_yeni(uint32_t a, uint32_t b)
{
    uint32_t d = a - b;
    return d >= 1u && d <= 0x7FFFFFFFu;
}

static bool emniyet_yapisal(const ida_sample_t *s)
{
    return s->hdr.type == IDA_SENS_SAFETY && s->hdr.inst < IDA_PORT_MAX_INST;
}

static bool emniyet_esit_anahtar(const ida_sample_t *a, const ida_sample_t *b)
{
    return a->hdr.t_meas.ns == b->hdr.t_meas.ns && a->hdr.type == b->hdr.type && a->hdr.inst == b->hdr.inst &&
           a->hdr.seq == b->hdr.seq;
}

static bool emniyet_esit_icerik(const ida_sample_t *a, const ida_sample_t *b)
{
    return a->hdr.t_rx.ns == b->hdr.t_rx.ns && a->hdr.window_ns == b->hdr.window_ns && a->hdr.flags == b->hdr.flags &&
           a->u.safety.estop == b->u.safety.estop && a->u.safety.tube[0] == b->u.safety.tube[0] &&
           a->u.safety.tube[1] == b->u.safety.tube[1];
}

typedef struct {
    bool estop_kis;
    bool fis_kis[IDA_PORT_N_TUBE];
    bool pim_kis[IDA_PORT_N_TUBE];
    bool estop_gev;
    bool fis_gev[IDA_PORT_N_TUBE];
    bool pim_gev[IDA_PORT_N_TUBE];
} ka_cagri_emniyet_t;

/* Bu çağrının emniyet örneklerinden kısıtlayan ve gevşeten içerik (A4-K116, K130, K115). */
static void emniyet_cagri(const ida_in_t *in, bool kabul_cagri, ka_cagri_emniyet_t *c)
{
    memset(c, 0, sizeof *c);
    unsigned n = in->n_samples < IDA_PORT_MAX_SAMPLES ? in->n_samples : IDA_PORT_MAX_SAMPLES;
    int64_t now = in->now_w.ns;
    for (unsigned i = 0; i < n; ++i) {
        const ida_sample_t *s = &in->samples[i];
        if (!emniyet_yapisal(s)) {
            continue;
        }
        if (s->u.safety.estop != 0u) {
            c->estop_kis = true;
        }
        for (unsigned t = 0; t < IDA_PORT_N_TUBE; ++t) {
            if ((s->u.safety.tube[t] & IDA_TUBE_PLUG_IN) == 0u) {
                c->fis_kis[t] = true;
            }
            if ((s->u.safety.tube[t] & IDA_TUBE_PIN_OUT) == 0u) {
                c->pim_kis[t] = true;
            }
        }
        /* Kabul (A4-K33, K34): yalnız inst 0'ın gevşetmesi etkilidir. */
        bool kabul = kabul_cagri && s->hdr.inst == 0u && (s->hdr.flags & IDA_HF_VALID) != 0u &&
                     s->u.safety.estop <= 1u &&
                     (s->u.safety.tube[0] & (uint8_t)~0x0Fu) == 0u && (s->u.safety.tube[1] & (uint8_t)~0x0Fu) == 0u &&
                     s->hdr.window_ns == 0u && s->hdr.t_rx.ns <= now && s->hdr.t_meas.ns >= 0 &&
                     (!in->tf.present || s->hdr.t_meas.ns <= in->tf.t.ns);
        if (kabul && K.iz_var) {
            kabul = seri_yeni(s->hdr.seq, K.iz_seq) && s->hdr.t_meas.ns >= K.iz_tmeas;
        }
        /* A4-K115: eşit anahtarlı farklı içerik: hepsi reddedilir. */
        for (unsigned j = 0; kabul && j < n; ++j) {
            const ida_sample_t *u = &in->samples[j];
            if (j != i && emniyet_yapisal(u) && (u->hdr.flags & IDA_HF_VALID) != 0u && emniyet_esit_anahtar(s, u) &&
                !emniyet_esit_icerik(s, u)) {
                kabul = false;
            }
        }
        if (!kabul) {
            continue;
        }
        K.iz_var = true;
        K.iz_seq = s->hdr.seq;
        if (!K.kabul_var || s->hdr.t_meas.ns > K.tmeas_max) {
            K.tmeas_ilerleme = s->hdr.t_rx.ns;
            K.tmeas_max = s->hdr.t_meas.ns;
        }
        K.iz_tmeas = s->hdr.t_meas.ns;
        K.son_rx = s->hdr.t_rx.ns;
        K.kabul_var = true;
        if (s->u.safety.estop == 0u) {
            c->estop_gev = true;
        }
        for (unsigned t = 0; t < IDA_PORT_N_TUBE; ++t) {
            if ((s->u.safety.tube[t] & IDA_TUBE_PLUG_IN) != 0u) {
                c->fis_gev[t] = true;
            }
            if ((s->u.safety.tube[t] & IDA_TUBE_PIN_OUT) != 0u) {
                c->pim_gev[t] = true;
            }
        }
    }
}

static void yetki_kes(int tube)
{
    for (unsigned t = 0; t < IDA_PORT_N_TUBE; ++t) {
        if (tube < 0 || (unsigned)tube == t) {
            K.fire_istek[t] = 0;
            K.yetki[t] = false;
        }
    }
}

/* (b) adımının modeli: her kurulu çağrıda (dönüş kodundan bağımsız). */
static void model_daralt(const ida_in_t *in, bool kabul_cagri)
{
    ka_cagri_emniyet_t c;
    bool once_estop = ka_model_estop();
    bool once_fis[IDA_PORT_N_TUBE];
    bool once_pim[IDA_PORT_N_TUBE];
    for (unsigned t = 0; t < IDA_PORT_N_TUBE; ++t) {
        once_fis[t] = K.fis[t];
        once_pim[t] = K.pim[t];
    }
    if (in->plat.link_event == IDA_LINKEV_SESSION_START) {
        /* (b3): örnek izi sıfırlanır; etkin değerler değişmez. */
        K.iz_var = false;
        K.referans = in->now_w.ns;
    }
    emniyet_cagri(in, kabul_cagri, &c);
    K.estop_taban = c.estop_kis || (K.estop_taban && !c.estop_gev);
    for (unsigned t = 0; t < IDA_PORT_N_TUBE; ++t) {
        K.fis[t] = c.fis_kis[t] || (K.fis[t] && !c.fis_gev[t]);
        K.pim[t] = c.pim_kis[t] || (K.pim[t] && !c.pim_gev[t]);
    }
    bool oturum_sonu = in->plat.link_event == IDA_LINKEV_SESSION_END;
    if (oturum_sonu) {
        K.estop_taban = true;
        for (unsigned t = 0; t < IDA_PORT_N_TUBE; ++t) {
            K.fis[t] = true;
            K.pim[t] = true;
        }
        K.kabul_var = false;
        yetki_kes(-1);
        /* Alıcı: oturum sonunda iki tüpün izni kapanır, bekleyen olaylar SESSION_END alır
           (A1-K22, K23; A4-K71). */
        A.izin_bitis[0] = 0;
        A.izin_bitis[1] = 0;
        for (size_t k = 0; k < g_bekleyen_n; ++k) {
            ka_olay_t *o = olay_ref(g_bekleyen[k].i);
            if (o != NULL && o->e.event_id == g_bekleyen[k].id) {
                o->sonuc = (uint8_t)IDA_EVR_SESSION_END;
                o->t_apply = 0;
                o->teslim_cagri = g_cagri + 1u;
            }
        }
    }
    /* Bayatlık (G-Ç29): kesin bayat = kabul edilmiş bir örnekten sonra iki ölçütten biri
       T_emn_bayat'ı aştı. Kabul hiç yoksa alan zaten kısıtlayandır (A4-K128 benzeri). */
    int64_t T = g_T_cagri;
    int64_t now = in->now_w.ns;
    bool bayat = false;
    if (K.kabul_var) {
        int64_t rx = K.son_rx > K.referans ? K.son_rx : K.referans;
        int64_t tm = K.tmeas_ilerleme > K.referans ? K.tmeas_ilerleme : K.referans;
        bayat = (now - rx > T) || (now - tm > T);
    }
    K.bayat = bayat;
    /* Yetki kesimleri. */
    bool fc_dar = (in->narrow.mask & (IDA_NARROW_FC_DISARM | IDA_NARROW_FC_ABORT)) != 0u;
    unsigned nc = in->n_cmds < IDA_PORT_MAX_CMDS ? in->n_cmds : IDA_PORT_MAX_CMDS;
    for (unsigned j = 0; j < nc; ++j) {
        const ida_cmd_t *cm = &in->cmds[j];
        if (cm->type == IDA_CMD_FC && (cm->u.fc.op == IDA_FC_DISARM || cm->u.fc.op == IDA_FC_ABORT)) {
            fc_dar = true;
        }
    }
    if (fc_dar) {
        yetki_kes(-1);
    }
    if (!once_estop && ka_model_estop()) {
        yetki_kes(-1);
    }
    for (unsigned t = 0; t < IDA_PORT_N_TUBE; ++t) {
        if ((!once_fis[t] && K.fis[t]) || (!once_pim[t] && K.pim[t])) {
            yetki_kes((int)t);
        }
    }
}

/* G-Ç11 modeli: tüpün izni bu çağrıda çekirdekçe açık sayılabilir mi. */
static bool model_izin_acik(unsigned tube, int64_t now)
{
    for (size_t k = 0; k < K.open_n[tube]; ++k) {
        const ka_olay_t *o = olay_ref(K.open_i[tube][k]);
        if (o == NULL || o->kapandi || !o->teslim_edildi || o->sonuc != IDA_EVR_APPLIED) {
            continue;
        }
        if (now - o->now_w < (int64_t)o->e.hold_ns) {
            return true;
        }
    }
    return false;
}

static void open_kaydet(unsigned tube, size_t mutlak)
{
    if (K.open_n[tube] < KA_TUP_OPEN) {
        K.open_i[tube][K.open_n[tube]++] = mutlak;
    } else {
        memmove(&K.open_i[tube][0], &K.open_i[tube][1], (KA_TUP_OPEN - 1u) * sizeof(size_t));
        K.open_i[tube][KA_TUP_OPEN - 1u] = mutlak;
    }
}

/* ---- Çağrı ------------------------------------------------------------------------------ */

static bool kabul_rc(ida_rc_t rc)
{
    return rc == IDA_RC_OK || rc == IDA_RC_IMU_REJECTED;
}

static void bu_cagri_sonuclari(bool kabul)
{
    for (size_t k = 0; k < g_bu_cagri_sonuc_n; ++k) {
        ka_olay_t *o = olay_ref(g_bu_cagri_sonuc[k].i);
        if (o == NULL || o->e.event_id != g_bu_cagri_sonuc[k].id) {
            continue;
        }
        bool gec = g_in.now_w.ns > o->now_w + (int64_t)o->e.apply_by_ns + (int64_t)IDA_EVT_RESULT_MARGIN_NS;
        if (kabul && gec) {
            /* Bilinmeyen sayılmış olayın geç sonucu yok sayılır (A4-K69, K70): izin vermez. */
            o->bilinmeyen = true;
        } else if (kabul) {
            o->teslim_edildi = true;
            o->teslim_edilen_cagri = g_cagri;
            o->teslim_now = g_in.now_w.ns;
            if (o->e.kind == IDA_EVT_SAFETY_OPEN && o->sonuc != IDA_EVR_APPLIED && o->e.tube < IDA_PORT_N_TUBE) {
                yetki_kes((int)o->e.tube); /* G-Ç23 */
            }
            if (o->e.kind == IDA_EVT_FIRE && o->e.tube < IDA_PORT_N_TUBE && K.fire_istek[o->e.tube] == 0) {
                K.yetki[o->e.tube] = false; /* G-Ç49: atış sonucu geldi */
            }
        } else {
            /* Reddedilen çağrıda sonuç kabul edilmez (A4-K26): yeniden kuyruğa. */
            o->teslim_cagri = g_cagri + 1u;
            bekleyen_ekle(g_bu_cagri_sonuc[k].i, g_bu_cagri_sonuc[k].id);
        }
    }
    g_bu_cagri_sonuc_n = 0;
}

static uint8_t sonuc_kodu_fc(const ida_in_t *in, const ida_out_t *o, unsigned j, uint8_t *ack)
{
    unsigned nlatch = (unsigned)kc_bit_sayisi((uint32_t)(in->narrow.mask & IDA_NARROW_DEFINED));
    unsigned k = nlatch + j;
    if (k >= o->n_results || k >= IDA_PORT_MAX_RESULTS) {
        *ack = 0u;
        return 0u;
    }
    *ack = o->results[k].ack;
    return (uint8_t)o->results[k].reason;
}

static void degismez_komutlar(const ida_in_t *in, const ida_out_t *o, bool kabul)
{
    unsigned nc = in->n_cmds <= IDA_PORT_MAX_CMDS ? in->n_cmds : 0u;
    uint8_t once = g_onceki_var ? g_onceki.fc.state : (uint8_t)IDA_FCS_SAFE;
    bool arm_once[IDA_PORT_N_TUBE] = {false, false};
    bool reset_ya_da_cikis = false;
    for (unsigned j = 0; kabul && j < nc; ++j) {
        const ida_cmd_t *c = &in->cmds[j];
        uint8_t ack = 0;
        (void)sonuc_kodu_fc(in, o, j, &ack);
        if (c->type != IDA_CMD_FC || ack != IDA_ACK_ACCEPTED) {
            continue;
        }
        uint8_t tube = c->u.fc.tube;
        switch (c->u.fc.op) {
        case IDA_FC_ARM:
            g_kapsam.fc_arm_kabul++;
            if (g_degismez && !reset_ya_da_cikis) {
                KA_DOGRULA(once != IDA_FCS_TEST && once != IDA_FCS_FAULT_LATCHED,
                           "FC_ARM %s durumunda ACCEPTED (G-Ç22, W5-K3)", ka_durum_adi(once));
                KA_DOGRULA(c->u.fc.arg0 == 0u && c->u.fc.arg1 == 0u && tube < IDA_PORT_N_TUBE,
                           "alanları geçersiz FC_ARM ACCEPTED (G-Ç04)");
            }
            if (tube < IDA_PORT_N_TUBE) {
                arm_once[tube] = true;
            }
            break;
        case IDA_FC_FIRE:
            g_kapsam.fc_fire_kabul++;
            if (g_degismez) {
                KA_DOGRULA(c->u.fc.arg0 == 0u && c->u.fc.arg1 == 0u && tube < IDA_PORT_N_TUBE,
                           "alanları geçersiz FC_FIRE ACCEPTED (G-Ç04)");
                bool ok = (once != IDA_FCS_SAFE && once != IDA_FCS_TEST && once != IDA_FCS_FAULT_LATCHED) ||
                          (tube < IDA_PORT_N_TUBE && arm_once[tube]);
                KA_DOGRULA(ok, "FC_FIRE(tüp %u) önceki durum %s iken ACCEPTED (G-Ç22: yalnız ARMED'de; değişmez (b))",
                           (unsigned)tube, ka_durum_adi(once));
            }
            if (tube < IDA_PORT_N_TUBE) {
                K.fire_istek[tube]++;
                K.yetki[tube] = true;
            }
            break;
        case IDA_FC_TEST_ENTER:
            if (g_degismez && !reset_ya_da_cikis) {
                KA_DOGRULA(once != IDA_FCS_TEST && once != IDA_FCS_FAULT_LATCHED,
                           "FC_TEST_ENTER %s durumunda ACCEPTED (G-Ç22)", ka_durum_adi(once));
            }
            break;
        case IDA_FC_RESET:
        case IDA_FC_TEST_EXIT:
            reset_ya_da_cikis = true;
            break;
        default:
            break;
        }
    }
    (void)reset_ya_da_cikis;
}

static bool fc_reset_kabul(const ida_in_t *in, const ida_out_t *o)
{
    unsigned nc = in->n_cmds <= IDA_PORT_MAX_CMDS ? in->n_cmds : 0u;
    for (unsigned j = 0; j < nc; ++j) {
        uint8_t ack = 0;
        (void)sonuc_kodu_fc(in, o, j, &ack);
        if (in->cmds[j].type == IDA_CMD_FC && in->cmds[j].u.fc.op == IDA_FC_RESET && ack == IDA_ACK_ACCEPTED) {
            return true;
        }
    }
    return false;
}

static bool fc_test_exit_kabul(const ida_in_t *in, const ida_out_t *o)
{
    unsigned nc = in->n_cmds <= IDA_PORT_MAX_CMDS ? in->n_cmds : 0u;
    for (unsigned j = 0; j < nc; ++j) {
        uint8_t ack = 0;
        (void)sonuc_kodu_fc(in, o, j, &ack);
        if (in->cmds[j].type == IDA_CMD_FC && in->cmds[j].u.fc.op == IDA_FC_TEST_EXIT && ack == IDA_ACK_ACCEPTED) {
            return true;
        }
    }
    return false;
}

static bool arac_dar(const ida_in_t *in)
{
    if ((in->narrow.mask & IDA_NARROW_DISARM) != 0u || in->plat.link_event == IDA_LINKEV_SESSION_END) {
        return true;
    }
    unsigned nc = in->n_cmds < IDA_PORT_MAX_CMDS ? in->n_cmds : IDA_PORT_MAX_CMDS;
    for (unsigned j = 0; j < nc; ++j) {
        if (in->cmds[j].type == IDA_CMD_DISARM) {
            return true;
        }
    }
    return false;
}

static bool arm_var(const ida_in_t *in)
{
    unsigned nc = in->n_cmds < IDA_PORT_MAX_CMDS ? in->n_cmds : IDA_PORT_MAX_CMDS;
    for (unsigned j = 0; j < nc; ++j) {
        if (in->cmds[j].type == IDA_CMD_ARM) {
            return true;
        }
    }
    return false;
}

static void olaylari_isle(const ida_in_t *in, const ida_out_t *o, bool test_kipi)
{
    int64_t now = in->now_w.ns;
    for (unsigned i = 0; i < o->n_events && i < IDA_PORT_MAX_EVENTS; ++i) {
        const ida_event_t *e = &o->events[i];
        size_t mutlak = g_olay_n;
        ka_olay_t *k = &g_olay[mutlak % KA_HALKA];
        memset(k, 0, sizeof *k);
        k->e = *e;
        k->now_w = now;
        k->cagri = g_cagri;
        ++g_olay_n;
        unsigned t = e->tube;
        if (e->kind == IDA_EVT_SAFETY_CLOSE) {
            g_kapsam.close++;
            for (unsigned u = 0; u < IDA_PORT_N_TUBE; ++u) {
                if (t != IDA_TUBE_ALL && t != u) {
                    continue;
                }
                for (size_t q = 0; q < K.open_n[u]; ++q) {
                    ka_olay_t *p = olay_ref(K.open_i[u][q]);
                    if (p != NULL) {
                        p->kapandi = true;
                    }
                }
            }
        } else if (g_degismez) {
            /* Genişletici olay: değişmez (a), (d), G-Ç16, G-Ç35 (A4-K108). */
            KA_DOGRULA(!ka_model_estop(), "%s(tüp %u): etkin acil durdurma kısıtlayan ya da bayat iken (değişmez (a); G-Ç16, G-Ç29)",
                       e->kind == IDA_EVT_FIRE ? "FIRE" : "SAFETY_OPEN", t);
            KA_DOGRULA(t < IDA_PORT_N_TUBE, "genişletici olay tüpü %u", t);
            KA_DOGRULA(!K.fis[t] && !K.pim[t], "%s(tüp %u): tüpün fişi çıkık ya da pimi takılı iken (G-Ç16)",
                       e->kind == IDA_EVT_FIRE ? "FIRE" : "SAFETY_OPEN", t);
            KA_DOGRULA(!test_kipi, "TEST durumunda %s (değişmez (d); G-Ç24, W5-K9)",
                       e->kind == IDA_EVT_FIRE ? "FIRE" : "SAFETY_OPEN");
            KA_DOGRULA(!o->health.imu_lag, "IMU_LAG kuruluyken genişletici olay (G-Ç35; A4-K108)");
            KA_DOGRULA(K.fire_istek[t] > 0 || (e->kind == IDA_EVT_SAFETY_OPEN && K.yetki[t]),
                       "%s(tüp %u) son yetki kesiminden sonra kabul edilmiş, tüketilmemiş FC_FIRE(tüp %u) yokken "
                       "(G-Ç21, G-Ç47; değişmez (b), (e))",
                       e->kind == IDA_EVT_FIRE ? "FIRE" : "SAFETY_OPEN", t, t);
        }
        if (e->kind == IDA_EVT_SAFETY_OPEN) {
            g_kapsam.open++;
            if (t < IDA_PORT_N_TUBE) {
                open_kaydet(t, mutlak);
            }
        } else if (e->kind == IDA_EVT_FIRE) {
            g_kapsam.fire++;
            if (t < IDA_PORT_N_TUBE) {
                if (g_degismez) {
                    KA_DOGRULA(model_izin_acik(t, now),
                               "FIRE(tüp %u): o tüpün izni çekirdekçe açık sayılamazken (G-Ç11: APPLIED alınmış ve "
                               "hold_ns dolmamış SAFETY_OPEN yok; değişmez (b), (f))",
                               t);
                }
                if (K.fire_istek[t] > 0) {
                    K.fire_istek[t]--;
                }
            }
        }
        /* Alıcı: sonuç planı. */
        ka_politika_t p = ka_ayar.politika[e->kind < KA_TUR_SAYISI ? e->kind : 0u];
        if (in->plat.link_state != IDA_LINK_RUNNING) {
            p.kod = (uint8_t)IDA_EVR_SESSION_END; /* ilk kabul kapısı (A4-K67 (2)) */
            p.gecikme = 1u;
            p.uygula = false;
        }
        k->sonuc = p.kod;
        k->teslim_cagri = g_cagri + (p.gecikme == 0u ? 1u : p.gecikme);
        k->t_apply = (p.kod == IDA_EVR_APPLIED || p.kod == IDA_EVR_DUPLICATE) ? e->t_decide.ns + KA_MS : 0;
        if (p.kod == IDA_EVR_APPLIED || (p.uygula && p.kod == KA_SONUC_YOK)) {
            if (k->t_apply == 0) {
                k->t_apply = e->t_decide.ns + KA_MS;
            }
            alici_uygula(k);
        }
        /* A4-K129: küme doluyken çıkan SAFETY_CLOSE izlenmez; kapanmanın ayrılmış yuvası tek
           olduğundan yalnız başka bekleyen kapanma yokken çıkan kapanma izlenir sayılır. */
        bool izlenir = e->kind == IDA_EVT_FIRE;
        if (e->kind == IDA_EVT_SAFETY_CLOSE) {
            izlenir = true;
            for (size_t q = 0; q < g_bekleyen_n; ++q) {
                const ka_olay_t *b = olay_ref(g_bekleyen[q].i);
                if (b != NULL && b->e.event_id == g_bekleyen[q].id && b->e.kind == IDA_EVT_SAFETY_CLOSE &&
                    now <= b->now_w + (int64_t)b->e.apply_by_ns + (int64_t)IDA_EVT_RESULT_MARGIN_NS) {
                    izlenir = false;
                }
            }
            if (g_bekleyen_n >= 8u) {
                izlenir = false;
            }
        }
        bekleyen_ekle(mutlak, e->event_id);
        if (p.kod == KA_SONUC_YOK && izlenir && K.kayip_n < sizeof K.kayip / sizeof K.kayip[0]) {
            K.kayip[K.kayip_n].i = mutlak;
            K.kayip[K.kayip_n].id = e->event_id;
            K.kayip_tamam[K.kayip_n] = false;
            ++K.kayip_n;
        }
    }
}

/* Sonucu hiç gelmeyen FIRE ve SAFETY_CLOSE: apply_by + 100 ms sonra FAULT_LATCHED (G-Ç42,
   G-Ç49, G-Ç50; W5-K2, W5-K11 (a)). */
static void kayiplari_denetle(const ida_in_t *in, const ida_out_t *o)
{
    int64_t now = in->now_w.ns;
    bool fl = o->fc.state == IDA_FCS_FAULT_LATCHED ||
              (g_onceki_var && g_onceki.fc.state == IDA_FCS_FAULT_LATCHED && fc_reset_kabul(in, o));
    for (size_t k = 0; k < K.kayip_n;) {
        const ka_olay_t *e = olay_ref(K.kayip[k].i);
        bool sil = false;
        if (e == NULL || e->e.event_id != K.kayip[k].id || e->sonuc != KA_SONUC_YOK) {
            sil = true; /* sonuç sonradan planlandı (ör. oturum sonu) */
        } else {
            int64_t son = e->now_w + (int64_t)e->e.apply_by_ns + (int64_t)IDA_EVT_RESULT_MARGIN_NS;
            if (now >= son - KA_P && fl) {
                K.kayip_tamam[k] = true;
            }
            if (now > son) {
                if (g_degismez) {
                    KA_DOGRULA(K.kayip_tamam[k],
                               "sonucu gelmeyen %s (tüp %u, olay %llx): apply_by + 100 ms (%lld ns) sonra fc.state %s, "
                               "FAULT_LATCHED bekleniyordu (G-Ç42, %s)",
                               e->e.kind == IDA_EVT_FIRE ? "FIRE" : "SAFETY_CLOSE", (unsigned)e->e.tube,
                               (unsigned long long)e->e.event_id, (long long)(now - e->now_w),
                               ka_durum_adi(o->fc.state), e->e.kind == IDA_EVT_FIRE ? "G-Ç49" : "G-Ç50");
                }
                sil = true;
            }
        }
        if (sil) {
            K.kayip[k] = K.kayip[K.kayip_n - 1u];
            K.kayip_tamam[k] = K.kayip_tamam[K.kayip_n - 1u];
            --K.kayip_n;
        } else {
            ++k;
        }
    }
    /* Sonucu bilinmeyen SAFETY_OPEN: izin kapalı kalır ve FC SAFE'e geçer (G-Ç23): yetki
       kesilir (bir kez). */
    for (unsigned t = 0; t < IDA_PORT_N_TUBE; ++t) {
        for (size_t q = 0; q < K.open_n[t]; ++q) {
            ka_olay_t *e = olay_ref(K.open_i[t][q]);
            if (e != NULL && !e->teslim_edildi && !e->bilinmeyen &&
                now > e->now_w + (int64_t)e->e.apply_by_ns + (int64_t)IDA_EVT_RESULT_MARGIN_NS) {
                e->bilinmeyen = true;
                yetki_kes((int)t);
            }
        }
    }
}

static void bparam_oku(uint32_t *v)
{
    for (size_t i = 0; i < K.bparam_n; ++i) {
        ida_param_value_t p;
        memset(&p, 0, sizeof p);
        (void)ida_core_param_get(kc_core(0), (uint16_t)K.bparam[i], &p);
        v[i] = kc_param_ham(&p);
    }
}

static ida_rc_t cagri_yurut(bool out_null)
{
    KA_DOGRULA(g_in_hazir, "ka_cagir: önce ka_girdi");
    g_in_hazir = false;
    ida_in_t *in = &g_in;
    bool step = in->imu_present;
    uint32_t b_once[8] = {0};
    bparam_oku(b_once);
    g_T_cagri = ka_t_emn_bayat();
    bool once_armed = g_onceki_var ? g_onceki.armed : false;
    ++g_cagri;
    ka_baglam("çağrı %llu, now %lld ns", (unsigned long long)g_cagri, (long long)in->now_w.ns);
    ida_rc_t rc;
    if (out_null) {
        rc = kc_cagir_out_null(step ? KC_STEP : KC_TICK, in);
    } else {
        rc = step ? kc_step(in) : kc_tick(in);
    }
    if (in->now_w.ns > g_simdi) {
        g_simdi = in->now_w.ns;
    }
    bool kabul = !out_null && kabul_rc(rc);
    bu_cagri_sonuclari(kabul);
    model_daralt(in, kabul);
    if (out_null) {
        return rc;
    }
    const ida_out_t *o = kc_out();
    bool test_kipi = o->fc.state == IDA_FCS_TEST ||
                     (g_onceki_var && g_onceki.fc.state == IDA_FCS_TEST && !fc_test_exit_kabul(in, o));
    degismez_komutlar(in, o, kabul);
    olaylari_isle(in, o, test_kipi);
    kayiplari_denetle(in, o);
    if (g_degismez) {
        /* Değişmez (c): FAULT_LATCHED yalnız kabul edilen FC_RESET ile terk edilir (G-Ç19, G-Ç20). */
        if (g_onceki_var && g_onceki.fc.state == IDA_FCS_FAULT_LATCHED && o->fc.state != IDA_FCS_FAULT_LATCHED) {
            KA_DOGRULA(fc_reset_kabul(in, o), "FAULT_LATCHED → %s: bu çağrıda kabul edilen FC_RESET yok (değişmez (c); G-Ç19)",
                       ka_durum_adi(o->fc.state));
        }
        /* G-Ç03 / 3.16: FAULT_LATCHED biti durumla aynı. */
        KA_DOGRULA(((o->fc.inhibit_mask & IDA_FCI_FAULT_LATCHED) != 0u) == (o->fc.state == IDA_FCS_FAULT_LATCHED),
                   "FAULT_LATCHED biti %d, fc.state %s (G-Ç03; A4 §3.16)",
                   (int)((o->fc.inhibit_mask & IDA_FCI_FAULT_LATCHED) != 0u), ka_durum_adi(o->fc.state));
        /* Değişmez (f), G-Ç11: PERMIT_OPEN ancak bir tüpün izni modelce açıksa. */
        if (o->fc.state == IDA_FCS_PERMIT_OPEN) {
            KA_DOGRULA(model_izin_acik(0u, in->now_w.ns) || model_izin_acik(1u, in->now_w.ns),
                       "fc.state PERMIT_OPEN ama hiçbir tüpte APPLIED alınmış ve hold_ns dolmamış SAFETY_OPEN yok "
                       "(G-Ç11; değişmez (f))");
        }
        /* Değişmez (e): araç devre dışı bırakma girdisi aynı çağrıda uygulanır. */
        if (arac_dar(in)) {
            KA_DOGRULA(!o->armed, "DISARM / SESSION_END çağrısının sonunda armed = 1 (G-Ç06, G-Ç18; değişmez (e))");
        }
        /* G-Ç07: genişletici olay yalnız step OK çıktısında. */
        if (ka_genisletici_olay_var(o)) {
            KA_DOGRULA(step && rc == IDA_RC_OK, "genişletici olay step OK dışında (G-Ç07)");
            KA_DOGRULA(!o->health.narrowed, "daraltmalı çağrıda genişletici olay (G-Ç06)");
        }
        /* Değişmez (g): silahlıyken (ya da aynı çağrıda ARM varken) emniyet parametresi değişmez. */
        if ((once_armed && o->armed && !arac_dar(in)) || arm_var(in)) {
            uint32_t b_sonra[8] = {0};
            bparam_oku(b_sonra);
            for (size_t i = 0; i < K.bparam_n; ++i) {
                KA_DOGRULA(b_once[i] == b_sonra[i],
                           "silahlıyken emniyet parametresi (id %d) %lu → %lu değişti (değişmez (g); G-Ç28, W5-K6)",
                           K.bparam[i], (unsigned long)b_once[i], (unsigned long)b_sonra[i]);
            }
        }
    }
    if (o->fc.state < 6u) {
        g_kapsam.durum[o->fc.state]++;
    }
    g_onceki = *o;
    g_onceki_var = true;
    return rc;
}

ida_rc_t ka_cagir(void)
{
    return cagri_yurut(false);
}

ida_rc_t ka_cagir_out_null(void)
{
    return cagri_yurut(true);
}

ida_rc_t ka_adim(void)
{
    (void)ka_girdi();
    return ka_cagir();
}

void ka_adimlar(int n)
{
    for (int i = 0; i < n; ++i) {
        (void)ka_adim();
    }
}

/* ---- Parametre yazma -------------------------------------------------------------------- */

void ka_param_yaz(const char *ad, uint32_t deger)
{
    int id = ka_param_id(ad);
    KA_DOGRULA(id >= 0, "parametre '%s' şemada yok", ad);
    ida_param_desc_t d;
    memset(&d, 0, sizeof d);
    uint16_t sayi = ida_core_param_count();
    uint8_t tur = (uint8_t)IDA_PT_U32;
    for (uint16_t i = 0; i < sayi; ++i) {
        if (ida_core_param_desc(i, &d) == IDA_RC_OK && d.id == (uint16_t)id) {
            tur = d.type;
            break;
        }
    }
    ida_in_t *in = ka_girdi();
    uint32_t cid = ka_yeni_id();
    ida_cmd_t *c = kc_komut_ekle(in, cid, (uint8_t)IDA_CMD_PARAM_SET);
    c->u.param.id = (uint16_t)id;
    c->u.param.type = tur;
    c->u.param.v.u32 = deger;
    uint32_t com = ka_komut(in, (uint8_t)IDA_CMD_PARAM_COMMIT);
    (void)ka_cagir();
    KA_SONUC(cid, IDA_ACK_ACCEPTED, IDA_REASON_NONE, "ka_param_yaz PARAM_SET");
    KA_SONUC(com, IDA_ACK_ACCEPTED, IDA_REASON_NONE, "ka_param_yaz PARAM_COMMIT");
    KA_DOGRULA(ka_param_u32(ad, ~deger) == deger, "parametre '%s' commit sonrası %lu değil", ad, (unsigned long)deger);
}

/* ---- Hazırlık --------------------------------------------------------------------------- */

void ka_oturum_ac(void)
{
    ida_in_t *in = ka_girdi();
    in->plat.link_event = (uint8_t)IDA_LINKEV_SESSION_START;
    (void)ka_cagir();
    for (int k = 0; k < 2000; ++k) {
        const ida_out_t *o = ka_out();
        if (o->status.mode != IDA_MODE_INIT && o->nav_valid && o->nav.est_level != IDA_EST_INIT) {
            return;
        }
        (void)ka_adim();
    }
    const ida_out_t *o = ka_out();
    ka_basarisiz(__LINE__, "hazırlık: 10 s'de kip INIT'ten (%u) ya da kestirim INIT'ten (%u) çıkmadı", (unsigned)o->status.mode,
               (unsigned)o->nav.est_level);
}

void ka_oturum_baslat(void)
{
    ida_in_t *in = ka_girdi();
    in->plat.link_event = (uint8_t)IDA_LINKEV_SESSION_START;
    (void)ka_cagir();
    (void)ka_adim();
}

void ka_kilitle(void)
{
    ka_ayar.permit_elle = false;
    ka_ayar.tup[0] = KC_TUP_ACIK;
    ka_ayar.tup[1] = KC_TUP_ACIK;
    ka_ayar.estop = 0u;
    (void)ka_adim();
    KA_DOGRULA(!ka_model_pim_takili(0u) && !ka_model_estop(), "ka_kilitle: emniyet girdileri gevşek değil");
    ka_ayar.tup[0] = (uint8_t)(KC_TUP_ACIK & (uint8_t)~IDA_TUBE_PIN_OUT);
    (void)ka_adim();
    KA_DOGRULA(ka_kapanma_var(ka_out(), 0u), "ka_kilitle: tüp 0'ın pimi takılınca SAFETY_CLOSE yok (G-Ç15; A4-K111)");
    ka_ayar.permit_elle = true;
    ka_ayar.permit_deger[0] = 1u;
    ka_ayar.permit_deger[1] = 0u;
    for (int k = 0; k < 3; ++k) {
        (void)ka_adim();
        if (ka_out()->fc.state == IDA_FCS_FAULT_LATCHED) {
            return;
        }
    }
    ka_basarisiz(__LINE__, "ka_kilitle: SAFETY_CLOSE(tüp 0) APPLIED olduktan sonra t_meas > t_apply örnekte PERMIT_OPEN = 1 iken "
               "fc.state %s, FAULT_LATCHED bekleniyordu (G-Ç53; W5-K4)",
               ka_durum_adi(ka_out()->fc.state));
}

void ka_kilit_nedeni_kaldir(void)
{
    ka_ayar.permit_elle = false;
    ka_ayar.tup[0] = KC_TUP_ACIK;
    ka_ayar.tup[1] = KC_TUP_ACIK;
    ka_ayar.estop = 0u;
}

void ka_arac_silahla(void)
{
    for (int k = 0; k < 20; ++k) {
        ida_in_t *in = ka_girdi();
        uint32_t id = ka_komut(in, (uint8_t)IDA_CMD_ARM);
        (void)ka_cagir();
        if (ka_out()->armed) {
            return;
        }
        const ida_cmd_result_t *r = ka_sonuc(id);
        if (k == 19) {
            ka_basarisiz(__LINE__, "hazırlık: ARM kabul edilmedi (%s/%s; est_level %u, fs_level %u, fs_reasons 0x%lx)",
                       r != NULL ? ack_adi(r->ack) : "-", r != NULL ? neden_adi(r->reason) : "-",
                       (unsigned)ka_out()->nav.est_level, (unsigned)ka_out()->fs_level,
                       (unsigned long)ka_out()->status.fs_reasons);
        }
        ka_adimlar(10);
    }
}

static bool kip_dene(uint8_t kip)
{
    ida_in_t *in = ka_girdi();
    if (kip == IDA_MODE_GUIDED || kip == IDA_MODE_AUTO) {
        uint32_t id = ka_yeni_id();
        kc_komut_ekle(in, id, (uint8_t)IDA_CMD_SET_MODE)->u.set_mode.mode = kip;
    } else {
        (void)ka_mandal(in, (uint8_t)IDA_NARROW_MODE);
        in->narrow.mode = kip;
    }
    (void)ka_cagir();
    ka_adimlar(2);
    return ka_out()->status.mode == kip && (ka_out()->fc.inhibit_mask & IDA_FCI_MODE) == 0u;
}

void ka_kip_sec(void)
{
    static const uint8_t adaylar[] = {IDA_MODE_GUIDED, IDA_MODE_STATION_KEEP, IDA_MODE_DEPTH_HOLD, IDA_MODE_STABILIZE,
                                      IDA_MODE_MANUAL};
    if ((ka_out()->fc.inhibit_mask & IDA_FCI_MODE) == 0u) {
        return;
    }
    for (size_t i = 0; i < sizeof adaylar; ++i) {
        if (kip_dene(adaylar[i])) {
            return;
        }
    }
    ka_basarisiz(__LINE__, "hazırlık: MODE biti GUIDED, STATION_KEEP, DEPTH_HOLD, STABILIZE ve MANUAL'ın hiçbirinde kalkmadı "
               "(kip %u, inhibit 0x%05lx; A4 §3.16 MODE: atış izni kümesi)",
               (unsigned)ka_out()->status.mode, (unsigned long)ka_out()->fc.inhibit_mask);
}

void ka_fc_hazir_bekle(void)
{
    for (int k = 0; k < 2000; ++k) {
        if ((ka_out()->fc.inhibit_mask & KA_FCARM_ENGEL) == 0u && ka_out()->fc.state == IDA_FCS_SAFE) {
            return;
        }
        (void)ka_adim();
    }
    const ida_out_t *o = ka_out();
    ka_basarisiz(__LINE__, "hazırlık: 10 s'de FC_ARM engel bitleri kalkmadı: inhibit 0x%05lx (engel 0x%05lx), fc.state %s, "
               "est_level %u, fs_level %u, kip %u, armed %d (A4 §3.17 FC_ARM satırı; EST biti kestirim düzeyi OK "
               "değilken kuruludur)",
               (unsigned long)o->fc.inhibit_mask, (unsigned long)(o->fc.inhibit_mask & KA_FCARM_ENGEL),
               ka_durum_adi(o->fc.state), (unsigned)o->nav.est_level, (unsigned)o->fs_level, (unsigned)o->status.mode,
               (int)o->armed);
}

void ka_fc_silahla(uint8_t tube)
{
    ida_in_t *in = ka_girdi();
    uint32_t id = ka_fc(in, (uint8_t)IDA_FC_ARM, tube);
    (void)ka_cagir();
    KA_SONUC(id, IDA_ACK_ACCEPTED, IDA_REASON_NONE, "hazırlık FC_ARM");
    KA_DOGRULA(ka_out()->fc.state == IDA_FCS_ARMED, "hazırlık: kabul edilen FC_ARM sonrası fc.state %s, ARMED bekleniyordu (W5-K3)",
               ka_durum_adi(ka_out()->fc.state));
}

void ka_hazir_oncesi(int n_ornek)
{
    ka_kur(n_ornek);
    ka_oturum_ac();
    ka_arac_silahla();
    ka_kip_sec();
    ka_fc_hazir_bekle();
}

void ka_hazir(int n_ornek, uint8_t tube)
{
    ka_hazir_oncesi(n_ornek);
    ka_fc_silahla(tube);
}

const ka_olay_t *ka_olay_bekle(uint8_t kind, int tube, int max_adim)
{
    size_t once = g_olay_n;
    for (int k = 0; k < max_adim; ++k) {
        (void)ka_adim();
        for (size_t i = once; i < g_olay_n; ++i) {
            const ka_olay_t *o = olay_ref(i);
            if (o != NULL && o->e.kind == kind && (tube < 0 || o->e.tube == (uint8_t)tube)) {
                return o;
            }
        }
    }
    return NULL;
}

const ka_olay_t *ka_izin_iste(uint8_t tube, int max_adim)
{
    ida_in_t *in = ka_girdi();
    uint32_t id = ka_fc(in, (uint8_t)IDA_FC_FIRE, tube);
    size_t once = g_olay_n;
    (void)ka_cagir();
    KA_SONUC(id, IDA_ACK_ACCEPTED, IDA_REASON_NONE, "ARMED'de FC_FIRE");
    for (size_t i = once; i < g_olay_n; ++i) {
        const ka_olay_t *o = olay_ref(i);
        if (o != NULL && o->e.kind == IDA_EVT_SAFETY_OPEN && o->e.tube == tube) {
            return o;
        }
    }
    const ka_olay_t *o = ka_olay_bekle((uint8_t)IDA_EVT_SAFETY_OPEN, (int)tube, max_adim);
    KA_DOGRULA(o != NULL, "kabul edilen FC_FIRE(tüp %u) sonrası %d adımda SAFETY_OPEN çıkmadı (G-Ç47, W5-K3)", (unsigned)tube,
               max_adim);
    return o;
}

/* ---- Hata izi: son çağrının girdisi ve çıktısı (yalnız test düştüğünde) ------------------- */

static void iz_yazdir(void)
{
    const ida_in_t *in = &g_in;
    const ida_out_t *o = kc_out();
    (void)printf("kabul_atis: son girdi: now %lld, tf %d seq %lu, imu %d (flags %u, t_meas %lld), link %u/%u, "
                 "narrow 0x%02x (mode %u), n_cmds %u, n_samples %u, n_evres %u\n",
                 (long long)in->now_w.ns, (int)in->tf.present, (unsigned long)in->tf.seq, (int)in->imu_present,
                 (unsigned)in->imu.hdr.flags, (long long)in->imu.hdr.t_meas.ns, (unsigned)in->plat.link_state,
                 (unsigned)in->plat.link_event, (unsigned)in->narrow.mask, (unsigned)in->narrow.mode, (unsigned)in->n_cmds,
                 (unsigned)in->n_samples, (unsigned)in->n_evres);
    for (unsigned j = 0; j < in->n_cmds && j < IDA_PORT_MAX_CMDS; ++j) {
        const ida_cmd_t *c = &in->cmds[j];
        (void)printf("  cmd[%u] id %lu type %u fc.op %u tube %u arg %lu/%lu\n", j, (unsigned long)c->cmd_id, (unsigned)c->type,
                     c->type == IDA_CMD_FC ? (unsigned)c->u.fc.op : 0u, c->type == IDA_CMD_FC ? (unsigned)c->u.fc.tube : 0u,
                     c->type == IDA_CMD_FC ? (unsigned long)c->u.fc.arg0 : 0ul,
                     c->type == IDA_CMD_FC ? (unsigned long)c->u.fc.arg1 : 0ul);
    }
    for (unsigned i = 0; i < in->n_samples && i < IDA_PORT_MAX_SAMPLES; ++i) {
        const ida_sample_t *sm = &in->samples[i];
        if (sm->hdr.type == IDA_SENS_SAFETY) {
            (void)printf("  safety[%u] inst %u flags %u seq %lu t_meas %lld t_rx %lld estop %u tube %02x/%02x\n", i,
                         (unsigned)sm->hdr.inst, (unsigned)sm->hdr.flags, (unsigned long)sm->hdr.seq,
                         (long long)sm->hdr.t_meas.ns, (long long)sm->hdr.t_rx.ns, (unsigned)sm->u.safety.estop,
                         (unsigned)sm->u.safety.tube[0], (unsigned)sm->u.safety.tube[1]);
        }
    }
    for (unsigned i = 0; i < in->n_evres && i < IDA_PORT_MAX_EVRES; ++i) {
        (void)printf("  evres[%u] id %llx result %u t_apply %lld\n", i, (unsigned long long)in->evres[i].event_id,
                     (unsigned)in->evres[i].result, (long long)in->evres[i].t_apply.ns);
    }
    (void)printf("kabul_atis: son çıktı: armed %d, fc.state %u, inhibit 0x%05lx, narrowed %d, imu_lag %d, mode %u, "
                 "n_events %u, n_results %u\n",
                 (int)o->armed, (unsigned)o->fc.state, (unsigned long)o->fc.inhibit_mask, (int)o->health.narrowed,
                 (int)o->health.imu_lag, (unsigned)o->status.mode, (unsigned)o->n_events, (unsigned)o->n_results);
    for (unsigned i = 0; i < o->n_results && i < IDA_PORT_MAX_RESULTS; ++i) {
        (void)printf("  result id %lu ack %u reason %u\n", (unsigned long)o->results[i].cmd_id, (unsigned)o->results[i].ack,
                     (unsigned)o->results[i].reason);
    }
    for (unsigned i = 0; i < o->n_events && i < IDA_PORT_MAX_EVENTS; ++i) {
        (void)printf("  event id %llx kind %u tube %u\n", (unsigned long long)o->events[i].event_id, (unsigned)o->events[i].kind,
                     (unsigned)o->events[i].tube);
    }
}
