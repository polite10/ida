/*
 * test_gorev_kip_ariza.c — W3-gorev kabul md.1 (kip tablosu her kip × olay) ve md.3 (kip ×
 * arıza matrisinin her hücresi; hiçbir arıza eylemi armed ya da mode'u genişletmez; mandal ve
 * FAILSAFE_RESET).
 *
 * Görev modülü saf iç işlevleriyle (gorev_ic.h) yapay girdiyle sürülür: yer tutucu kestirim
 * çekirdek yoluyla INIT'ten çıkmadığı için kestirim düzeyi, IMU bayatlığı ve duvar saati testte
 * kurulur. Beklenen değerler kaynağın tablolarından değil, testteki kurallardan hesaplanır.
 */
#include <string.h>

#include "cekirdek.h"
#include "gorev/gorev_ic.h"
#include "unity.h"

void setUp(void);
void tearDown(void);
void setUp(void) {}
void tearDown(void) {}

#define MS 1000000LL

/* ---- Ortam ------------------------------------------------------------------------------ */

typedef struct {
    ida_gor_t g;
    ida_emn_t e;
    ida_param_t p;
    ida_gunluk_t gun;
    ida_gor_girdi_t gi;
    int64_t now;
    bool besle_batt;   /* her adımda taze batarya örneği */
    bool besle_leak;   /* her adımda kuru sızıntı örneği */
    bool besle_hb;     /* her adımda HEARTBEAT */
    float batt_v;
    uint32_t seq;
} ortam_t;

static ortam_t O;
static ida_in_t IN;

static void gunluk_bosalt(ortam_t *o)
{
    ida_gun_kur(&o->gun);
}

static uint32_t gunluk_say(const ortam_t *o, uint16_t kod)
{
    uint32_t n = 0u;
    for (uint32_t i = 0u; i < (uint32_t)o->gun.n; i++) {
        if (o->gun.kayit[i].code == kod) {
            n++;
        }
    }
    return n;
}

static ida_cmd_result_t komut(ortam_t *o, uint8_t tur, uint8_t kip_arg)
{
    ida_komut_baglam_t b;
    (void)memset(&b, 0, sizeof(b));
    b.now_w.ns = o->now;
    b.oturum_var = true;
    b.armed = o->e.armed;
    b.mode = o->e.mode;
    b.est_level = o->gi.est_level;
    b.daraldi = o->e.d.herhangi;
    b.events_ok = true;
    b.param_ok = true;
    ida_cmd_t c;
    (void)memset(&c, 0, sizeof(c));
    o->seq++;
    c.cmd_id = o->seq;
    c.type = tur;
    c.u.set_mode.mode = kip_arg;
    ida_cmd_result_t r;
    (void)memset(&r, 0, sizeof(r));
    r.cmd_id = c.cmd_id;
    r.kind = (uint8_t)IDA_RES_ACK;
    ida_gor_komut_ic(&o->g, &o->e, &b, &c, &r, &o->p, &o->gun);
    return r;
}

static void batt(ortam_t *o, float v)
{
    ida_sample_t s;
    (void)memset(&s, 0, sizeof(s));
    s.hdr.type = (uint8_t)IDA_SENS_BATT;
    s.hdr.t_rx.ns = o->now;
    s.u.batt.voltage = v;
    ida_gor_ornek(&o->g, &s);
}

static void leak(ortam_t *o, uint8_t inst, uint8_t mask)
{
    ida_sample_t s;
    (void)memset(&s, 0, sizeof(s));
    s.hdr.type = (uint8_t)IDA_SENS_LEAK;
    s.hdr.inst = inst;
    s.hdr.t_rx.ns = o->now;
    s.u.leak.mask = mask;
    ida_gor_ornek(&o->g, &s);
}

/* Bileşen geçerliliği düzeyden (gorev.c'nin geçici eşlemesiyle aynı kural): OK → hepsi;
   DEGRADED → tutum, yön, derinlik, dikey hız (havuz: konum yok; W3-K5); diğerleri → hiçbiri. */
static uint8_t gecerlilik_turet(uint8_t est)
{
    const uint8_t dikey =
        (uint8_t)(IDA_GOR_G_TUTUM | IDA_GOR_G_YON | IDA_GOR_G_DERINLIK | IDA_GOR_G_DIKEY_HIZ);
    if (est == IDA_EST_OK) {
        return (uint8_t)(dikey | IDA_GOR_G_YATAY_HIZ | IDA_GOR_G_KONUM);
    }
    return (est == IDA_EST_DEGRADED) ? dikey : (uint8_t)0u;
}

/* Bir adım: beslemeler (örnekler adımdan önce, komutlar adımdan sonra gelir; cerceve.c), adım. */
static void adim(ortam_t *o)
{
    if (o->besle_batt) {
        batt(o, o->batt_v);
    }
    if (o->besle_leak) {
        leak(o, 0u, 0u);
    }
    o->gi.now_ns = o->now;
    o->gi.gecerlilik = gecerlilik_turet(o->gi.est_level);
    (void)memset(&o->e.d, 0, sizeof(o->e.d));
    ida_gor_adim_ic(&o->g, &o->e, &o->gi, &o->gun);
    if (o->besle_hb) {
        (void)komut(o, (uint8_t)IDA_CMD_HEARTBEAT, 0u);
    }
}

/* `sure_ms` boyunca 100 ms adımlarla ilerler. */
static void ilerle(ortam_t *o, int64_t sure_ms)
{
    const int64_t son = o->now + (sure_ms * MS);
    while (o->now < son) {
        o->now += 100 * MS;
        adim(o);
    }
}

/* Sağlıklı ortam: kestirim OK, IMU taze, parametre geçerli, batarya 16 V, sızıntı kuru,
   HEARTBEAT var, acil durdurma serbest, 2 m derinlik. Kip DISARMED'e geçer (HAZIR). */
static void ortam_kur(ortam_t *o)
{
    (void)memset(o, 0, sizeof(*o));
    ida_gor_kur(&o->g);
    ida_emn_kur(&o->e);
    ida_prm_varsayilan(&o->p.yururluk);
    o->p.param_ok = true;
    ida_gun_kur(&o->gun);
    o->now = 10000 * MS;
    o->gi.now_ns = o->now;
    o->gi.est_level = (uint8_t)IDA_EST_OK;
    o->gi.imu_bayat = false;
    o->gi.param_ok = true;
    o->gi.nav_derinlik = 2.0f;
    o->gi.nav_yon = 0.5f;
    o->gi.nav_ne[0] = 1.0f;
    o->gi.nav_ne[1] = 2.0f;
    o->gi.prm = &o->p;
    o->besle_batt = true;
    o->besle_leak = true;
    o->besle_hb = true;
    o->batt_v = 16.0f;
    o->e.kisitli = 0u; /* test kancası: W4'ün inst 0 gevşetmesinin yerine */
    ilerle(o, 500);
    gunluk_bosalt(o);
}

/* Kipi test kancasıyla kurar (iz de güncellenir; tutma hedefi yakalama gürültüsü olmasın). */
static void kip_kur(ortam_t *o, uint8_t kip)
{
    o->e.mode = kip;
    o->g.son_kip = kip;
    o->g.mission_state = (kip == (uint8_t)IDA_MODE_AUTO) ? (uint8_t)IDA_MS_RUNNING
                                                          : (uint8_t)IDA_MS_IDLE;
}

/* ---- md.1: kip tablosu ------------------------------------------------------------------- */

/* Kurallardan beklenen hücre (02 §2.6; A4-K46, K127; W3-gorev emniyet ilkesi). */
static uint8_t beklenen_gecis(uint32_t kip, uint32_t olay)
{
    if ((kip >= 8u) || (olay >= (uint32_t)IDA_GOR_OLAY_SAYI)) {
        return (uint8_t)IDA_GOR_KIP_SILAHSIZ;
    }
    const bool init = kip == IDA_MODE_INIT;
    switch (olay) {
    case IDA_GOR_OLAY_HAZIR:
        return init ? (uint8_t)IDA_MODE_DISARMED : (uint8_t)IDA_GOR_KIP_KAL;
    case IDA_GOR_OLAY_GUIDED:
        return init ? (uint8_t)IDA_GOR_KIP_RET : (uint8_t)IDA_MODE_GUIDED;
    case IDA_GOR_OLAY_AUTO:
        return init ? (uint8_t)IDA_GOR_KIP_RET : (uint8_t)IDA_MODE_AUTO;
    case IDA_GOR_OLAY_KONUM_YOK:
        return (kip == IDA_MODE_STATION_KEEP) ? (uint8_t)IDA_MODE_DEPTH_HOLD
                                              : (uint8_t)IDA_GOR_KIP_KAL;
    case IDA_GOR_OLAY_GOREV_YOK:
        return (kip == IDA_MODE_AUTO) ? (uint8_t)IDA_MODE_DEPTH_HOLD : (uint8_t)IDA_GOR_KIP_KAL;
    case IDA_GOR_OLAY_DARALT_MANUAL:
        return init ? (uint8_t)IDA_GOR_KIP_KAL : (uint8_t)IDA_MODE_MANUAL;
    case IDA_GOR_OLAY_DARALT_STABILIZE:
        return init ? (uint8_t)IDA_GOR_KIP_KAL : (uint8_t)IDA_MODE_STABILIZE;
    case IDA_GOR_OLAY_DARALT_DEPTH_HOLD:
        return init ? (uint8_t)IDA_GOR_KIP_KAL : (uint8_t)IDA_MODE_DEPTH_HOLD;
    case IDA_GOR_OLAY_DARALT_STATION_KEEP:
        return init ? (uint8_t)IDA_GOR_KIP_KAL : (uint8_t)IDA_MODE_STATION_KEEP;
    default:
        return (uint8_t)IDA_GOR_KIP_KAL; /* DARALT_GECERSIZ */
    }
}

static void test_md1_kip_tablosu_her_hucre(void)
{
    uint32_t sayi = 0u;
    for (uint32_t kip = 0u; kip < 256u; kip++) {
        for (uint32_t olay = 0u; olay < 256u; olay++) {
            const uint8_t h = ida_gor_kip_gecis((uint8_t)kip, (uint8_t)olay);
            TEST_ASSERT_EQUAL_HEX8_MESSAGE(beklenen_gecis(kip, olay), h, "kip × olay");
            if ((kip < 8u) && (olay < (uint32_t)IDA_GOR_OLAY_SAYI)) {
                sayi++;
                /* Hücre tanımlı: hedef kip, KAL ya da RET; SILAHSIZ yalnız tablo dışında. */
                TEST_ASSERT_TRUE((h < 8u) || (h == IDA_GOR_KIP_KAL) || (h == IDA_GOR_KIP_RET));
                /* Genişletme (GUIDED/AUTO hedefi) yalnız komut olaylarından gelir. */
                if ((h == IDA_MODE_GUIDED) || (h == IDA_MODE_AUTO)) {
                    TEST_ASSERT_TRUE((olay == IDA_GOR_OLAY_GUIDED) || (olay == IDA_GOR_OLAY_AUTO) ||
                                     (h == kip));
                }
            }
        }
    }
    TEST_ASSERT_EQUAL_UINT32(80u, sayi);
}

/* DARALT_* satırları emniyet modülünün (b) davranışıyla aynı (A4-K46, K127). */
static void test_md1_daralt_satirlari_emniyetle_ayni(void)
{
    static const uint8_t hedefler[5] = {IDA_MODE_MANUAL, IDA_MODE_STABILIZE, IDA_MODE_DEPTH_HOLD,
                                        IDA_MODE_STATION_KEEP, 1u /* geçersiz hedef */};
    for (uint32_t kip = 0u; kip < 8u; kip++) {
        for (uint32_t i = 0u; i < 5u; i++) {
            ida_emn_t e;
            ida_emn_kur(&e);
            e.mode = (uint8_t)kip;
            ida_gunluk_t gun;
            ida_gun_kur(&gun);
            (void)memset(&IN, 0, sizeof(IN));
            IN.narrow.mask = IDA_NARROW_MODE;
            IN.narrow.mode = hedefler[i];
            ida_emn_baglam_t eb;
            eb.gun = &gun;
            eb.now.ns = 1000 * MS;
            ida_emn_daralt(&e, &IN, &eb);
            const uint8_t h =
                ida_gor_kip_gecis((uint8_t)kip, (uint8_t)(IDA_GOR_OLAY_DARALT_MANUAL + i));
            const uint8_t beklenen = (h == IDA_GOR_KIP_KAL) ? (uint8_t)kip : h;
            TEST_ASSERT_EQUAL_UINT8(beklenen, e.mode);
        }
    }
}

static void test_md1_adimdaki_olaylar(void)
{
    /* INIT: kestirim INIT'te kalır; HAZIR ile DISARMED (MODE_CHANGE). */
    ortam_t *o = &O;
    (void)memset(o, 0, sizeof(*o));
    ida_gor_kur(&o->g);
    ida_emn_kur(&o->e);
    ida_prm_varsayilan(&o->p.yururluk);
    ida_gun_kur(&o->gun);
    o->now = 5000 * MS;
    o->gi.prm = &o->p;
    o->gi.est_level = (uint8_t)IDA_EST_INIT;
    o->gi.param_ok = true;
    adim(o);
    TEST_ASSERT_EQUAL_UINT8(IDA_MODE_INIT, o->e.mode);
    o->gi.est_level = (uint8_t)IDA_EST_DEGRADED;
    o->now += 5 * MS;
    adim(o);
    TEST_ASSERT_EQUAL_UINT8(IDA_MODE_DISARMED, o->e.mode);
    TEST_ASSERT_EQUAL_UINT32(1u, gunluk_say(o, (uint16_t)IDA_LOG_MODE_CHANGE));
    /* Geri dönüş yok: kestirim yeniden INIT olsa da kip INIT olmaz. */
    o->gi.est_level = (uint8_t)IDA_EST_INIT;
    o->now += 5 * MS;
    adim(o);
    TEST_ASSERT_EQUAL_UINT8(IDA_MODE_DISARMED, o->e.mode);

    /* STATION_KEEP + kestirim OK değil → DEPTH_HOLD. */
    ortam_kur(o);
    kip_kur(o, (uint8_t)IDA_MODE_STATION_KEEP);
    o->gi.est_level = (uint8_t)IDA_EST_DEGRADED;
    o->now += 5 * MS;
    adim(o);
    TEST_ASSERT_EQUAL_UINT8(IDA_MODE_DEPTH_HOLD, o->e.mode);

    /* AUTO + yürüyen görev yok → DEPTH_HOLD. */
    ortam_kur(o);
    o->e.mode = (uint8_t)IDA_MODE_AUTO;
    o->now += 5 * MS;
    adim(o);
    TEST_ASSERT_EQUAL_UINT8(IDA_MODE_DEPTH_HOLD, o->e.mode);

    /* Tanımsız kip + silahlı → silahsız ve DISARMED (EMNİYET). */
    ortam_kur(o);
    o->e.mode = 9u;
    o->e.armed = true;
    gunluk_bosalt(o);
    o->now += 5 * MS;
    adim(o);
    TEST_ASSERT_FALSE(o->e.armed);
    TEST_ASSERT_EQUAL_UINT8(IDA_MODE_DISARMED, o->e.mode);
    TEST_ASSERT_EQUAL_UINT32(1u, gunluk_say(o, (uint16_t)IDA_LOG_ARM_CHANGE));
    TEST_ASSERT_EQUAL_UINT32(1u, gunluk_say(o, (uint16_t)IDA_LOG_MODE_CHANGE));
}

static void test_md1_kip_komutlari(void)
{
    ortam_t *o = &O;
    ortam_kur(o);
    TEST_ASSERT_EQUAL_UINT8(IDA_MODE_DISARMED, o->e.mode);
    /* DISARMED → GUIDED (kestirim OK). */
    ida_cmd_result_t r = komut(o, (uint8_t)IDA_CMD_SET_MODE, (uint8_t)IDA_MODE_GUIDED);
    TEST_ASSERT_EQUAL_UINT8(IDA_ACK_ACCEPTED, r.ack);
    TEST_ASSERT_EQUAL_UINT8(IDA_MODE_GUIDED, o->e.mode);
    TEST_ASSERT_EQUAL_UINT32(1u, gunluk_say(o, (uint16_t)IDA_LOG_MODE_CHANGE));
    /* Yineleme: etkisiz kabul, günlük yok. */
    gunluk_bosalt(o);
    r = komut(o, (uint8_t)IDA_CMD_SET_MODE, (uint8_t)IDA_MODE_GUIDED);
    TEST_ASSERT_EQUAL_UINT8(IDA_ACK_ACCEPTED, r.ack);
    TEST_ASSERT_EQUAL_UINT32(0u, gunluk_say(o, (uint16_t)IDA_LOG_MODE_CHANGE));
    /* AUTO: yüklü görev yok → BAD_MODE, kip değişmez. */
    r = komut(o, (uint8_t)IDA_CMD_SET_MODE, (uint8_t)IDA_MODE_AUTO);
    TEST_ASSERT_EQUAL_UINT8(IDA_ACK_REJECTED, r.ack);
    TEST_ASSERT_EQUAL_UINT16(IDA_REASON_BAD_MODE, r.reason);
    TEST_ASSERT_EQUAL_UINT8(IDA_MODE_GUIDED, o->e.mode);
    /* AUTO: yüklü görevle (test kancası) kabul. */
    o->g.mission_state = (uint8_t)IDA_MS_LOADED;
    r = komut(o, (uint8_t)IDA_CMD_SET_MODE, (uint8_t)IDA_MODE_AUTO);
    TEST_ASSERT_EQUAL_UINT8(IDA_ACK_ACCEPTED, r.ack);
    TEST_ASSERT_EQUAL_UINT8(IDA_MODE_AUTO, o->e.mode);
    /* Kestirim BAD (tutum, yön, derinlik yok) → GUIDED BAD_MODE (W3-K7). */
    ortam_kur(o);
    o->gi.est_level = (uint8_t)IDA_EST_BAD;
    o->now += 5 * MS;
    adim(o);
    r = komut(o, (uint8_t)IDA_CMD_SET_MODE, (uint8_t)IDA_MODE_GUIDED);
    TEST_ASSERT_EQUAL_UINT16(IDA_REASON_BAD_MODE, r.reason);
    TEST_ASSERT_EQUAL_UINT8(IDA_MODE_DISARMED, o->e.mode);
    /* DEGRADED (yatay yok): GUIDED kabul (W3-K7), AUTO BAD_MODE. */
    ortam_kur(o);
    o->gi.est_level = (uint8_t)IDA_EST_DEGRADED;
    o->now += 5 * MS;
    adim(o);
    o->g.mission_state = (uint8_t)IDA_MS_LOADED;
    r = komut(o, (uint8_t)IDA_CMD_SET_MODE, (uint8_t)IDA_MODE_AUTO);
    TEST_ASSERT_EQUAL_UINT16(IDA_REASON_BAD_MODE, r.reason);
    r = komut(o, (uint8_t)IDA_CMD_SET_MODE, (uint8_t)IDA_MODE_GUIDED);
    TEST_ASSERT_EQUAL_UINT8(IDA_ACK_ACCEPTED, r.ack);
    TEST_ASSERT_EQUAL_UINT8(IDA_MODE_GUIDED, o->e.mode);
    /* INIT'ten genişletme yok (tablo RET). */
    ortam_kur(o);
    o->e.mode = (uint8_t)IDA_MODE_INIT;
    r = komut(o, (uint8_t)IDA_CMD_SET_MODE, (uint8_t)IDA_MODE_GUIDED);
    TEST_ASSERT_EQUAL_UINT16(IDA_REASON_BAD_MODE, r.reason);
    TEST_ASSERT_EQUAL_UINT8(IDA_MODE_INIT, o->e.mode);
    /* Aynı çağrıda daraltma varsa (savunma): NARROWED, kip değişmez. */
    ortam_kur(o);
    o->e.d.herhangi = true;
    r = komut(o, (uint8_t)IDA_CMD_SET_MODE, (uint8_t)IDA_MODE_GUIDED);
    TEST_ASSERT_EQUAL_UINT16(IDA_REASON_NARROWED, r.reason);
    TEST_ASSERT_EQUAL_UINT8(IDA_MODE_DISARMED, o->e.mode);
    /* Tanımsız kipte genişletme isteği: silahsız, DISARMED, BAD_MODE (EMNİYET). */
    ortam_kur(o);
    o->e.mode = 12u;
    o->e.armed = true;
    r = komut(o, (uint8_t)IDA_CMD_SET_MODE, (uint8_t)IDA_MODE_GUIDED);
    TEST_ASSERT_EQUAL_UINT16(IDA_REASON_BAD_MODE, r.reason);
    TEST_ASSERT_FALSE(o->e.armed);
    TEST_ASSERT_EQUAL_UINT8(IDA_MODE_DISARMED, o->e.mode);
    /* Görev komutları dilim 2: UNSUPPORTED / NONE. */
    r = komut(o, (uint8_t)IDA_CMD_MISSION_BEGIN, 0u);
    TEST_ASSERT_EQUAL_UINT8(IDA_ACK_UNSUPPORTED, r.ack);
    TEST_ASSERT_EQUAL_UINT16(IDA_REASON_NONE, r.reason);
}

/* ---- md.3: kip × arıza matrisi ---------------------------------------------------------- */

/* 02 §4.3, sütun grupları; INIT: eylem yok (okuma). Testin kendi kopyası. */
enum { N_ = 0, W_ = 1, H_ = 2, S_ = 3, P_ = 4 };
typedef struct {
    uint8_t d, man, stb, dh, sk, g, a;
} satir_t;

static const satir_t MATRIS[IDA_GOR_K_SAYI] = {
    /* SIZINTI        */ {W_, S_, S_, S_, S_, S_, S_},
    /* SIZINTI_BAYAT  */ {W_, W_, W_, W_, W_, W_, W_},
    /* GCS_1          */ {N_, H_, H_, W_, W_, H_, N_},
    /* GCS_2          */ {N_, S_, S_, S_, S_, S_, N_}, /* AUTO su altında yok */
    /* BATT_DUSUK     */ {W_, W_, W_, W_, W_, W_, S_},
    /* BATT_KRIT      */ {W_, S_, S_, S_, S_, S_, S_},
    /* BATT_KRIT_2    */ {W_, P_, P_, P_, P_, P_, P_},
    /* BATT_BAYAT     */ {W_, W_, W_, W_, W_, W_, W_},
    /* EST_BOZUK      */ {N_, N_, N_, N_, N_, N_, H_}, /* yatay kaybı yalnız AUTO (W3-K7) */
    /* EST_BOZUK_UZUN */ {N_, N_, N_, N_, S_, N_, S_}, /* SK zaten DH'ye iner */
    /* EST_KOTU       */ {W_, W_, P_, P_, P_, P_, P_},
    /* SAYISAL        */ {W_, P_, P_, P_, P_, P_, P_},
    /* IMU_BAYAT      */ {N_, P_, P_, P_, P_, P_, P_},
    /* PARAM          */ {W_, N_, N_, N_, N_, N_, N_},
};

static uint8_t matris(uint32_t k, uint8_t kip)
{
    const satir_t *s = &MATRIS[k];
    switch (kip) {
    case IDA_MODE_INIT:
        return N_;
    case IDA_MODE_DISARMED:
        return s->d;
    case IDA_MODE_MANUAL:
        return s->man;
    case IDA_MODE_STABILIZE:
        return s->stb;
    case IDA_MODE_DEPTH_HOLD:
        return s->dh;
    case IDA_MODE_STATION_KEEP:
        return s->sk;
    case IDA_MODE_GUIDED:
        return s->g;
    default:
        return s->a;
    }
}

static void test_md3_tablo_kaynakla_ayni(void)
{
    for (uint32_t k = 0u; k < (uint32_t)IDA_GOR_K_SAYI; k++) {
        for (uint8_t kip = 0u; kip < 8u; kip++) {
            uint8_t b = matris(k, kip);
            const uint8_t h = ida_gor_ariza_hucre((uint8_t)k, kip);
            if ((k == (uint32_t)IDA_GOR_K_GCS_2) && (kip == IDA_MODE_AUTO)) {
                b = S_; /* tabloda S; yüzeyde olmama koşulu ayrıca (yayınlamada) */
            }
            TEST_ASSERT_EQUAL_UINT8_MESSAGE(b, h, "hücre");
        }
    }
    TEST_ASSERT_EQUAL_UINT8(IDA_FS_DISARM, ida_gor_ariza_hucre(14u, 1u));
    TEST_ASSERT_EQUAL_UINT8(IDA_FS_DISARM, ida_gor_ariza_hucre(0u, 8u));
}

/* Bir koşulu ortamda oluşturur; oluşan koşul kümesini döner (bazı koşullar ötekini içerir). */
static uint32_t kosul_olustur(ortam_t *o, uint32_t k)
{
    uint32_t kume = IDA_GOR_K_BIT(k);
    switch (k) {
    case IDA_GOR_K_SIZINTI:
        o->besle_leak = false;
        leak(o, 1u, 1u);
        o->now += 10 * MS;
        leak(o, 1u, 1u);
        o->now += 10 * MS;
        adim(o);
        break;
    case IDA_GOR_K_SIZINTI_BAYAT:
        o->besle_leak = false;
        ilerle(o, 2500);
        break;
    case IDA_GOR_K_GCS_1:
        o->besle_hb = false;
        ilerle(o, 3500);
        break;
    case IDA_GOR_K_GCS_2:
        o->besle_hb = false;
        ilerle(o, 61000);
        kume |= IDA_GOR_K_BIT(IDA_GOR_K_GCS_1);
        break;
    case IDA_GOR_K_BATT_DUSUK:
        o->batt_v = 13.0f;
        ilerle(o, 10500);
        break;
    case IDA_GOR_K_BATT_KRIT:
        o->batt_v = 12.0f;
        ilerle(o, 10500);
        kume |= IDA_GOR_K_BIT(IDA_GOR_K_BATT_DUSUK);
        break;
    case IDA_GOR_K_BATT_KRIT_2:
        o->batt_v = 12.0f;
        ilerle(o, 71000);
        kume |= IDA_GOR_K_BIT(IDA_GOR_K_BATT_DUSUK) | IDA_GOR_K_BIT(IDA_GOR_K_BATT_KRIT);
        break;
    case IDA_GOR_K_BATT_BAYAT:
        o->besle_batt = false;
        ilerle(o, 2500);
        break;
    case IDA_GOR_K_EST_BOZUK:
        o->gi.est_level = (uint8_t)IDA_EST_DEGRADED;
        o->now += 10 * MS;
        adim(o);
        break;
    case IDA_GOR_K_EST_BOZUK_UZUN:
        o->gi.est_level = (uint8_t)IDA_EST_DEGRADED;
        ilerle(o, 30500);
        kume |= IDA_GOR_K_BIT(IDA_GOR_K_EST_BOZUK);
        break;
    case IDA_GOR_K_EST_KOTU:
        o->gi.est_level = (uint8_t)IDA_EST_BAD;
        ilerle(o, 2500);
        kume |= IDA_GOR_K_BIT(IDA_GOR_K_EST_BOZUK); /* bileşen yok: konum da yok */
        break;
    case IDA_GOR_K_SAYISAL:
        o->gi.est_level = (uint8_t)IDA_EST_FAULT;
        o->now += 10 * MS;
        adim(o);
        kume |= IDA_GOR_K_BIT(IDA_GOR_K_EST_BOZUK);
        break;
    case IDA_GOR_K_IMU_BAYAT:
        o->gi.imu_bayat = true;
        o->now += 10 * MS;
        adim(o);
        break;
    default: /* PARAM */
        o->gi.param_ok = false;
        o->now += 10 * MS;
        adim(o);
        break;
    }
    return kume;
}

/* "koşul K, kip M, silahlı S" (snprintf kartta bellek ayırır; elle yazılır). */
static void vaka_mesaji(char msj[96], uint32_t k, uint32_t kip, uint32_t silahli)
{
    static const char sablon[] = "kosul .., kip ., silahli .";
    (void)memcpy(msj, sablon, sizeof(sablon));
    msj[6] = (char)('0' + (char)(k / 10u));
    msj[7] = (char)('0' + (char)(k % 10u));
    msj[14] = (char)('0' + (char)kip);
    msj[25] = (char)('0' + (char)silahli);
}

static void test_md3_her_hucre_bir_vaka(void)
{
    char msj[96];
    for (uint32_t k = 0u; k < (uint32_t)IDA_GOR_K_SAYI; k++) {
        for (uint8_t kip = 0u; kip < 8u; kip++) {
            for (uint32_t silahli = 0u; silahli < 2u; silahli++) {
                ortam_t *o = &O;
                ortam_kur(o);
                if (kip == IDA_MODE_INIT) {
                    /* INIT sütunu: kestirim INIT (başka düzey INIT'ten çıkarır). */
                    o->gi.est_level = (uint8_t)IDA_EST_INIT;
                }
                kip_kur(o, kip);
                o->e.armed = (silahli != 0u) && (kip != IDA_MODE_INIT);
                o->now += 5 * MS;
                adim(o);
                TEST_ASSERT_EQUAL_UINT8_MESSAGE(IDA_FS_NONE, o->g.fs_level, "sağlıklı ortam");
                const bool armed0 = o->e.armed;
                const uint32_t kume = kosul_olustur(o, k);
                /* Beklenen kip: kestirim koşulları INIT'ten çıkarır ve SK'yi DH'ye indirir. */
                uint8_t bkip = kip;
                const bool est_kosulu = (k == (uint32_t)IDA_GOR_K_EST_BOZUK) ||
                                        (k == (uint32_t)IDA_GOR_K_EST_BOZUK_UZUN) ||
                                        (k == (uint32_t)IDA_GOR_K_EST_KOTU) ||
                                        (k == (uint32_t)IDA_GOR_K_SAYISAL);
                if (est_kosulu && (kip == IDA_MODE_INIT)) {
                    bkip = (uint8_t)IDA_MODE_DISARMED;
                }
                if (est_kosulu && (kip == IDA_MODE_STATION_KEEP)) {
                    bkip = (uint8_t)IDA_MODE_DEPTH_HOLD;
                }
                uint8_t beklenen = N_;
                uint32_t nedenler = 0u;
                for (uint32_t j = 0u; j < (uint32_t)IDA_GOR_K_SAYI; j++) {
                    if ((kume & IDA_GOR_K_BIT(j)) == 0u) {
                        continue;
                    }
                    /* KRIT_2 yalnız KRIT mandallandıysa (KRIT hücresi ≥ WARN) oluşur. */
                    if ((j == (uint32_t)IDA_GOR_K_BATT_KRIT_2) &&
                        (matris(IDA_GOR_K_BATT_KRIT, bkip) < W_)) {
                        continue;
                    }
                    const uint8_t h = matris(j, bkip);
                    if (h >= W_) {
                        nedenler |= ida_gor_ariza_nedeni((uint8_t)j);
                    }
                    if (h > beklenen) {
                        beklenen = h;
                    }
                }
                vaka_mesaji(msj, k, kip, silahli);
                TEST_ASSERT_EQUAL_UINT8_MESSAGE(bkip, o->e.mode, msj);
                TEST_ASSERT_EQUAL_UINT8_MESSAGE(beklenen, o->g.fs_level, msj);
                TEST_ASSERT_EQUAL_HEX32_MESSAGE(nedenler, o->g.fs_nedenleri, msj);
                /* Arıza eylemi hiçbir zaman genişletmez: armed 0 → 1 olmaz; kip GUIDED/AUTO'ya
                   gitmez. (DISARM düzeyi tabloda yok; silahlı kalabilir.) */
                TEST_ASSERT_TRUE_MESSAGE(!o->e.armed || armed0, msj);
                TEST_ASSERT_TRUE_MESSAGE((o->e.mode == kip) || (o->e.mode == bkip), msj);
                TEST_ASSERT_TRUE_MESSAGE(ida_gor_tutarli(&o->g), msj);
            }
        }
    }
}

/* AUTO × GCS_2: yüzeyde SURFACE, su altında eylem yok (02 §4.3). */
static void test_md3_auto_gcs2_yuzeyde(void)
{
    ortam_t *o = &O;
    ortam_kur(o);
    kip_kur(o, (uint8_t)IDA_MODE_AUTO);
    o->gi.nav_derinlik = 0.1f;
    o->besle_hb = false;
    ilerle(o, 61000);
    TEST_ASSERT_EQUAL_UINT8(IDA_FS_SURFACE, o->g.fs_level);
    TEST_ASSERT_TRUE((o->g.fs_nedenleri & IDA_FSR_GCS_LOSS) != 0u);
    /* HEARTBEAT geri gelince kendiliğinden temizlenir (mandalsız). */
    o->besle_hb = true;
    ilerle(o, 300);
    TEST_ASSERT_EQUAL_UINT8(IDA_FS_NONE, o->g.fs_level);
}

/* ---- Mandallar ve FAILSAFE_RESET (3.17) -------------------------------------------------- */

static void test_md3_mandal_ve_failsafe_reset(void)
{
    ortam_t *o = &O;
    ortam_kur(o);
    kip_kur(o, (uint8_t)IDA_MODE_DEPTH_HOLD);
    /* Mandal yokken: etkisiz kabul. */
    ida_cmd_result_t r = komut(o, (uint8_t)IDA_CMD_FAILSAFE_RESET, 0u);
    TEST_ASSERT_EQUAL_UINT8(IDA_ACK_ACCEPTED, r.ack);
    /* Sızıntı: SURFACE, mandallı. */
    o->e.armed = true;
    (void)kosul_olustur(o, IDA_GOR_K_SIZINTI);
    TEST_ASSERT_EQUAL_UINT8(IDA_FS_SURFACE, o->g.fs_level);
    /* Algılayıcı kurudu ama silahlı: temizlenmez (02 §4.3: devre dışı + reset). */
    leak(o, 1u, 0u);
    ilerle(o, 200);
    TEST_ASSERT_EQUAL_UINT8(IDA_FS_SURFACE, o->g.fs_level);
    r = komut(o, (uint8_t)IDA_CMD_FAILSAFE_RESET, 0u);
    TEST_ASSERT_EQUAL_UINT16(IDA_REASON_NOT_ARMABLE, r.reason);
    /* Silahsız ama hâlâ ıslak: temizlenmez. */
    o->e.armed = false;
    leak(o, 1u, 1u);
    ilerle(o, 200);
    r = komut(o, (uint8_t)IDA_CMD_FAILSAFE_RESET, 0u);
    TEST_ASSERT_EQUAL_UINT16(IDA_REASON_NOT_ARMABLE, r.reason);
    TEST_ASSERT_EQUAL_UINT8(IDA_FS_SURFACE, o->g.fs_level);
    /* Kuru ve silahsız: temizlenir, aynı çağrıda düzey düşer, FAILSAFE_CHANGE. */
    leak(o, 1u, 0u);
    ilerle(o, 200);
    gunluk_bosalt(o);
    r = komut(o, (uint8_t)IDA_CMD_FAILSAFE_RESET, 0u);
    TEST_ASSERT_EQUAL_UINT8(IDA_ACK_ACCEPTED, r.ack);
    TEST_ASSERT_EQUAL_UINT8(IDA_FS_NONE, o->g.fs_level);
    TEST_ASSERT_EQUAL_UINT32(0u, o->g.mandal);
    TEST_ASSERT_EQUAL_UINT32(1u, gunluk_say(o, (uint16_t)IDA_LOG_FAILSAFE_CHANGE));

    /* EST_KOTU: temizleme EST_OK süresi ister. */
    ortam_kur(o);
    kip_kur(o, (uint8_t)IDA_MODE_DEPTH_HOLD);
    (void)kosul_olustur(o, IDA_GOR_K_EST_KOTU);
    TEST_ASSERT_EQUAL_UINT8(IDA_FS_PASSIVE_SURFACE, o->g.fs_level);
    o->gi.est_level = (uint8_t)IDA_EST_OK;
    ilerle(o, 1000);
    TEST_ASSERT_EQUAL_UINT8(IDA_FS_PASSIVE_SURFACE, o->g.fs_level); /* mandallı */
    r = komut(o, (uint8_t)IDA_CMD_FAILSAFE_RESET, 0u);
    TEST_ASSERT_EQUAL_UINT16(IDA_REASON_NOT_ARMABLE, r.reason);
    ilerle(o, 1500);
    r = komut(o, (uint8_t)IDA_CMD_FAILSAFE_RESET, 0u);
    TEST_ASSERT_EQUAL_UINT8(IDA_ACK_ACCEPTED, r.ack);
    TEST_ASSERT_EQUAL_UINT8(IDA_FS_NONE, o->g.fs_level);

    /* IMU bayat: taze IMU 1 s ister. */
    ortam_kur(o);
    kip_kur(o, (uint8_t)IDA_MODE_MANUAL);
    (void)kosul_olustur(o, IDA_GOR_K_IMU_BAYAT);
    TEST_ASSERT_EQUAL_UINT8(IDA_FS_PASSIVE_SURFACE, o->g.fs_level);
    o->gi.imu_bayat = false;
    ilerle(o, 500);
    r = komut(o, (uint8_t)IDA_CMD_FAILSAFE_RESET, 0u);
    TEST_ASSERT_EQUAL_UINT16(IDA_REASON_NOT_ARMABLE, r.reason);
    ilerle(o, 600);
    r = komut(o, (uint8_t)IDA_CMD_FAILSAFE_RESET, 0u);
    TEST_ASSERT_EQUAL_UINT8(IDA_ACK_ACCEPTED, r.ack);

    /* Düşük batarya: histerezis. Hepsi ya da hiçbiri: iki mandal, biri tutmuyorsa hiçbiri. */
    ortam_kur(o);
    kip_kur(o, (uint8_t)IDA_MODE_MANUAL);
    (void)kosul_olustur(o, IDA_GOR_K_BATT_DUSUK);
    (void)kosul_olustur(o, IDA_GOR_K_IMU_BAYAT);
    o->gi.imu_bayat = false;
    o->batt_v = 13.3f; /* eşik üstü, histerezis altı */
    ilerle(o, 1500);
    r = komut(o, (uint8_t)IDA_CMD_FAILSAFE_RESET, 0u);
    TEST_ASSERT_EQUAL_UINT16(IDA_REASON_NOT_ARMABLE, r.reason);
    TEST_ASSERT_EQUAL_UINT32(IDA_GOR_K_BIT(IDA_GOR_K_BATT_DUSUK) | IDA_GOR_K_BIT(IDA_GOR_K_IMU_BAYAT),
                             o->g.mandal);
    o->batt_v = 13.6f;
    ilerle(o, 200);
    r = komut(o, (uint8_t)IDA_CMD_FAILSAFE_RESET, 0u);
    TEST_ASSERT_EQUAL_UINT8(IDA_ACK_ACCEPTED, r.ack);
    TEST_ASSERT_EQUAL_UINT32(0u, o->g.mandal);

    /* Mandalsız koşul (GCS) FAILSAFE_RESET'le temizlenmez, kendiliğinden kalkar. */
    ortam_kur(o);
    kip_kur(o, (uint8_t)IDA_MODE_MANUAL);
    (void)kosul_olustur(o, IDA_GOR_K_GCS_1);
    TEST_ASSERT_EQUAL_UINT8(IDA_FS_HOLD, o->g.fs_level);
    r = komut(o, (uint8_t)IDA_CMD_FAILSAFE_RESET, 0u);
    TEST_ASSERT_EQUAL_UINT8(IDA_ACK_ACCEPTED, r.ack); /* mandal yok: etkisiz */
    TEST_ASSERT_EQUAL_UINT8(IDA_FS_HOLD, o->g.fs_level);
}

/* INIT'te mandal kurulmaz (eylem yok); INIT'ten çıkınca koşul sürüyorsa kurulur. */
static void test_md3_init_mandal_kurmaz(void)
{
    ortam_t *o = &O;
    ortam_kur(o);
    o->gi.est_level = (uint8_t)IDA_EST_INIT;
    kip_kur(o, (uint8_t)IDA_MODE_INIT);
    o->batt_v = 13.0f;
    ilerle(o, 11000);
    TEST_ASSERT_EQUAL_UINT8(IDA_FS_NONE, o->g.fs_level);
    TEST_ASSERT_EQUAL_UINT32(0u, o->g.mandal);
    TEST_ASSERT_TRUE((o->g.etkin & IDA_GOR_K_BIT(IDA_GOR_K_BATT_DUSUK)) != 0u);
    o->gi.est_level = (uint8_t)IDA_EST_OK;
    o->now += 10 * MS;
    adim(o);
    TEST_ASSERT_EQUAL_UINT8(IDA_MODE_DISARMED, o->e.mode);
    TEST_ASSERT_EQUAL_UINT8(IDA_FS_WARN, o->g.fs_level);
    TEST_ASSERT_EQUAL_UINT32(IDA_GOR_K_BIT(IDA_GOR_K_BATT_DUSUK), o->g.mandal);
}

/* W3-K1: sanal emniyet girdileri (acil durdurma dahil) aracı silahsızlandırmaz. */
static void test_k1_estop_silahsizlandirmaz(void)
{
    ortam_t *o = &O;
    ortam_kur(o);
    kip_kur(o, (uint8_t)IDA_MODE_DEPTH_HOLD);
    o->e.armed = true;
    o->e.kisitli = (uint8_t)IDA_EMN_TUMU;
    gunluk_bosalt(o);
    ilerle(o, 500);
    TEST_ASSERT_TRUE(o->e.armed);
    TEST_ASSERT_EQUAL_UINT8(IDA_FS_NONE, o->g.fs_level);
    TEST_ASSERT_EQUAL_UINT32(0u, gunluk_say(o, (uint16_t)IDA_LOG_ARM_CHANGE));
}

/* W3-K5: kestirim DEGRADED (havuz: konum yok) derinlik ve yön tutmayı sonlandırmaz ve yüzeye
   çıkış başlatmaz; konum kipleri (STATION_KEEP) DEPTH_HOLD'a iner; GUIDED reddedilir. */
static void test_k5_degraded_derinlik_tutma_surer(void)
{
    ortam_t *o = &O;
    ortam_kur(o);
    kip_kur(o, (uint8_t)IDA_MODE_DEPTH_HOLD);
    o->e.armed = true;
    o->gi.est_level = (uint8_t)IDA_EST_DEGRADED;
    ilerle(o, 61000);
    TEST_ASSERT_EQUAL_UINT8(IDA_MODE_DEPTH_HOLD, o->e.mode);
    TEST_ASSERT_EQUAL_UINT8(IDA_FS_NONE, o->g.fs_level);
    TEST_ASSERT_TRUE(o->e.armed);
    o->g.mission_state = (uint8_t)IDA_MS_LOADED;
    ida_cmd_result_t r = komut(o, (uint8_t)IDA_CMD_SET_MODE, (uint8_t)IDA_MODE_AUTO);
    TEST_ASSERT_EQUAL_UINT16(IDA_REASON_BAD_MODE, r.reason); /* AUTO yatay ister */
    o->g.mission_state = (uint8_t)IDA_MS_IDLE;
    /* STATION_KEEP → DEPTH_HOLD, eylem yok. */
    kip_kur(o, (uint8_t)IDA_MODE_STATION_KEEP);
    o->now += 10 * MS;
    adim(o);
    TEST_ASSERT_EQUAL_UINT8(IDA_MODE_DEPTH_HOLD, o->e.mode);
    TEST_ASSERT_EQUAL_UINT8(IDA_FS_NONE, o->g.fs_level);
    /* Derinlik bileşeni gidince de kip sürer; istek dikeyi kuvvet 0'a indirir (test_gorev_arm). */
}

/* W3-K2: adımın arıza daraltması (eylem ağırlaşması, kip düşmesi) uygulanan çağrıda
   genişleticiler etkisiz (NARROWED); sonraki çağrıda olağan. */
static void test_k2_adim_daraltmasi(void)
{
    ortam_t *o = &O;
    ortam_kur(o);
    kip_kur(o, (uint8_t)IDA_MODE_STATION_KEEP);
    o->besle_hb = false;
    o->gi.est_level = (uint8_t)IDA_EST_DEGRADED; /* SK → DH bu adımda */
    o->now += 10 * MS;
    adim(o);
    TEST_ASSERT_TRUE(o->g.adim_daralt);
    ida_cmd_result_t r = komut(o, (uint8_t)IDA_CMD_ARM, 0u);
    TEST_ASSERT_EQUAL_UINT16(IDA_REASON_NARROWED, r.reason);
    TEST_ASSERT_FALSE(o->e.armed);
    r = komut(o, (uint8_t)IDA_CMD_FAILSAFE_RESET, 0u);
    TEST_ASSERT_EQUAL_UINT16(IDA_REASON_NARROWED, r.reason);
    r = komut(o, (uint8_t)IDA_CMD_HEARTBEAT, 0u); /* nötr: etkilenmez */
    TEST_ASSERT_EQUAL_UINT8(IDA_ACK_ACCEPTED, r.ack);
    o->now += 10 * MS;
    adim(o);
    TEST_ASSERT_FALSE(o->g.adim_daralt);
    r = komut(o, (uint8_t)IDA_CMD_ARM, 0u);
    TEST_ASSERT_EQUAL_UINT8(IDA_ACK_ACCEPTED, r.ack);
}

static void test_tutarlilik(void)
{
    ortam_t *o = &O;
    ortam_kur(o);
    TEST_ASSERT_TRUE(ida_gor_tutarli(&o->g));
    o->g.fs_level = 6u;
    TEST_ASSERT_FALSE(ida_gor_tutarli(&o->g));
    ortam_kur(o);
    o->g.son_kip = 8u;
    TEST_ASSERT_FALSE(ida_gor_tutarli(&o->g));
    ortam_kur(o);
    o->g.mandal = UINT32_C(1) << 20;
    TEST_ASSERT_FALSE(ida_gor_tutarli(&o->g));
    ortam_kur(o);
    const uint8_t iki = 2u;
    (void)memcpy(&o->g.gcs_var, &iki, 1u);
    TEST_ASSERT_FALSE(ida_gor_tutarli(&o->g));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_md1_kip_tablosu_her_hucre);
    RUN_TEST(test_md1_daralt_satirlari_emniyetle_ayni);
    RUN_TEST(test_md1_adimdaki_olaylar);
    RUN_TEST(test_md1_kip_komutlari);
    RUN_TEST(test_md3_tablo_kaynakla_ayni);
    RUN_TEST(test_md3_her_hucre_bir_vaka);
    RUN_TEST(test_md3_auto_gcs2_yuzeyde);
    RUN_TEST(test_md3_mandal_ve_failsafe_reset);
    RUN_TEST(test_md3_init_mandal_kurmaz);
    RUN_TEST(test_k1_estop_silahsizlandirmaz);
    RUN_TEST(test_k5_degraded_derinlik_tutma_surer);
    RUN_TEST(test_k2_adim_daraltmasi);
    RUN_TEST(test_tutarlilik);
    return UNITY_END();
}
