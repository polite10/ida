/*
 * test_gorev_arm.c — W3-gorev kabul md.2 (arm ön koşullarının her biri: tutmazken ret ve doğru
 * neden, tutarken kabul), kontrol isteği (kapsam md.4) ve çekirdek yolu (çerçeve, dağıtıcı,
 * günlük) üzerinden bütünleşik vakalar.
 *
 * Ön koşul vakaları görev modülünün saf iç işlevleriyle yapay girdiyle sürülür (yer tutucu
 * kestirim çekirdek yoluyla INIT'ten çıkmaz). Çekirdek vakaları yalnız port işlevlerini ve
 * W2 testlerinin kullandığı yapı alanı kancalarını kullanır.
 */
#include <string.h>

#include "cekirdek.h"
#include "destek.h"
#include "gorev/gorev_ic.h"
#include "unity.h"

void setUp(void);
void tearDown(void);
void setUp(void) {}
void tearDown(void) {}

#define MS 1000000LL

/* ---- Ortam (test_gorev_kip_ariza.c ile aynı düzen) ------------------------------------- */

typedef struct {
    ida_gor_t g;
    ida_emn_t e;
    ida_param_t p;
    ida_gunluk_t gun;
    ida_gor_girdi_t gi;
    int64_t now;
    bool besle_batt;
    bool besle_leak;
    bool besle_hb;
    bool oturum;
    float batt_v;
    uint32_t seq;
} ortam_t;

static ortam_t O;

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
    b.oturum_var = o->oturum;
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
    ida_gor_komut_ic(&o->g, &o->e, &b, &c, &r, &o->p, &o->gun);
    return r;
}

static void ornek(ortam_t *o, uint8_t tur, float v, uint8_t mask)
{
    ida_sample_t s;
    (void)memset(&s, 0, sizeof(s));
    s.hdr.type = tur;
    s.hdr.t_rx.ns = o->now;
    if (tur == (uint8_t)IDA_SENS_BATT) {
        s.u.batt.voltage = v;
    } else {
        s.u.leak.mask = mask;
    }
    ida_gor_ornek(&o->g, &s);
}

/* Bileşen geçerliliği düzeyden (gorev.c'nin geçici eşlemesi); g_elle ise testin değeri. */
static bool g_elle;

static uint8_t gecerlilik_turet(uint8_t est)
{
    const uint8_t dikey =
        (uint8_t)(IDA_KES_G_TUTUM | IDA_KES_G_YON | IDA_KES_G_DERINLIK | IDA_KES_G_DIKEY_HIZ);
    if (est == IDA_EST_OK) {
        return (uint8_t)(dikey | IDA_KES_G_YATAY_HIZ | IDA_KES_G_KONUM);
    }
    return (est == IDA_EST_DEGRADED) ? dikey : (uint8_t)0u;
}

static void adim(ortam_t *o)
{
    if (o->besle_batt) {
        ornek(o, (uint8_t)IDA_SENS_BATT, o->batt_v, 0u);
    }
    if (o->besle_leak) {
        ornek(o, (uint8_t)IDA_SENS_LEAK, 0.0f, 0u);
    }
    o->gi.now_ns = o->now;
    if (!g_elle) {
        o->gi.gecerlilik = gecerlilik_turet(o->gi.est_level);
    }
    (void)memset(&o->e.d, 0, sizeof(o->e.d));
    ida_gor_adim_ic(&o->g, &o->e, &o->gi, &o->gun);
    if (o->besle_hb) {
        (void)komut(o, (uint8_t)IDA_CMD_HEARTBEAT, 0u);
    }
}

static void ilerle(ortam_t *o, int64_t sure_ms)
{
    const int64_t son = o->now + (sure_ms * MS);
    while (o->now < son) {
        o->now += 100 * MS;
        adim(o);
    }
}

/* İki adım (komutlar son adımın görüntüsüyle değerlendirilir). İkinci adım, ilkinde uygulanmış
   olabilecek arıza daraltmasının (W3-K2; o çağrıda NARROWED) etkisinden sonraki durumu sınar;
   W3-K2'nin kendisi test_gorev_kip_ariza.c'de sınanır. */
static void bir_adim(ortam_t *o)
{
    o->now += 5 * MS;
    adim(o);
    o->now += 5 * MS;
    adim(o);
    ida_gun_kur(&o->gun);
}

static void ortam_kur(ortam_t *o)
{
    (void)memset(o, 0, sizeof(*o));
    ida_gor_kur(&o->g);
    ida_emn_kur(&o->e);
    ida_prm_varsayilan(&o->p.yururluk);
    o->p.param_ok = true;
    ida_gun_kur(&o->gun);
    o->now = 10000 * MS;
    o->gi.est_level = (uint8_t)IDA_EST_OK;
    o->gi.param_ok = true;
    o->gi.nav_derinlik = 2.0f;
    o->gi.nav_yon = 0.5f;
    o->gi.nav_ne[0] = 1.0f;
    o->gi.nav_ne[1] = 2.0f;
    o->gi.prm = &o->p;
    o->besle_batt = true;
    o->besle_leak = true;
    o->besle_hb = true;
    o->oturum = true;
    o->batt_v = 16.0f;
    o->e.kisitli = 0u; /* test kancası: W4'ün inst 0 gevşetmesinin yerine */
    ilerle(o, 500);
    ida_gun_kur(&o->gun);
}

static void arm_ret_bekle(ortam_t *o, const char *msj)
{
    const ida_cmd_result_t r = komut(o, (uint8_t)IDA_CMD_ARM, 0u);
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(IDA_ACK_REJECTED, r.ack, msj);
    TEST_ASSERT_EQUAL_UINT16_MESSAGE(IDA_REASON_NOT_ARMABLE, r.reason, msj);
    TEST_ASSERT_FALSE_MESSAGE(o->e.armed, msj);
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(0u, gunluk_say(o, (uint16_t)IDA_LOG_ARM_CHANGE), msj);
}

static void arm_kabul_bekle(ortam_t *o, const char *msj)
{
    const ida_cmd_result_t r = komut(o, (uint8_t)IDA_CMD_ARM, 0u);
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(IDA_ACK_ACCEPTED, r.ack, msj);
    TEST_ASSERT_EQUAL_UINT16_MESSAGE(IDA_REASON_NONE, r.reason, msj);
    TEST_ASSERT_TRUE_MESSAGE(o->e.armed, msj);
}

/* ---- md.2: arm ön koşulları ------------------------------------------------------------- */

static void test_md2_tum_kosullar_tutarken_kabul(void)
{
    ortam_t *o = &O;
    ortam_kur(o);
    TEST_ASSERT_EQUAL_UINT8(IDA_MODE_DISARMED, o->e.mode);
    arm_kabul_bekle(o, "sağlıklı");
    TEST_ASSERT_EQUAL_UINT32(1u, gunluk_say(o, (uint16_t)IDA_LOG_ARM_CHANGE));
    TEST_ASSERT_EQUAL_INT32(1, o->gun.kayit[0].arg0);
    TEST_ASSERT_EQUAL_INT32(0, o->gun.kayit[0].arg1); /* komutla değişim: neden 0 (3.15) */
    /* Zaten silahlı: etkisiz kabul, günlük yok (yinelenen komut ek etki üretmez). */
    ida_gun_kur(&o->gun);
    arm_kabul_bekle(o, "zaten silahlı");
    TEST_ASSERT_EQUAL_UINT32(0u, gunluk_say(o, (uint16_t)IDA_LOG_ARM_CHANGE));
}

static void test_md2_kestirim_duzeyi(void)
{
    /* W3-K3: OK ya da DEGRADED yeterli; INIT, BAD, FAULT'ta ret. */
    ortam_t *o = &O;
    ortam_kur(o);
    o->gi.est_level = (uint8_t)IDA_EST_DEGRADED;
    bir_adim(o);
    arm_kabul_bekle(o, "DEGRADED yeterli");
    static const uint8_t kotu[2] = {IDA_EST_BAD, IDA_EST_FAULT};
    for (uint32_t i = 0u; i < 2u; i++) {
        ortam_kur(o);
        o->gi.est_level = kotu[i];
        bir_adim(o);
        arm_ret_bekle(o, "kestirim BAD/FAULT");
    }
    /* INIT dışı kipte kestirim INIT */
    ortam_kur(o);
    o->gi.est_level = (uint8_t)IDA_EST_INIT;
    bir_adim(o);
    arm_ret_bekle(o, "kestirim INIT");
    /* Kip INIT: çekirdek başlatması bitmedi. */
    ortam_kur(o);
    o->e.mode = (uint8_t)IDA_MODE_INIT;
    o->g.son_kip = (uint8_t)IDA_MODE_INIT;
    o->gi.est_level = (uint8_t)IDA_EST_INIT;
    bir_adim(o);
    TEST_ASSERT_EQUAL_UINT8(IDA_MODE_INIT, o->e.mode);
    arm_ret_bekle(o, "kip INIT");
}

static void test_md2_ariza_eylemi(void)
{
    /* HOLD ve üstü, arm reddi gerektirmeyen bir kayıttan: MANUAL'da HEARTBEAT kaybı → HOLD. */
    ortam_t *o = &O;
    ortam_kur(o);
    o->e.mode = (uint8_t)IDA_MODE_MANUAL;
    o->g.son_kip = (uint8_t)IDA_MODE_MANUAL;
    o->besle_hb = false;
    ilerle(o, 3500);
    TEST_ASSERT_EQUAL_UINT8(IDA_FS_HOLD, o->g.fs_level);
    ida_gun_kur(&o->gun);
    arm_ret_bekle(o, "arıza eylemi HOLD");
    /* Aynı kayıt DISARMED'de eylem üretmez: kabul (bağlantı ayrı ön koşul değil, W3-K3). */
    ortam_kur(o);
    o->besle_hb = false;
    ilerle(o, 3500);
    TEST_ASSERT_EQUAL_UINT8(IDA_FS_NONE, o->g.fs_level);
    ida_gun_kur(&o->gun);
    arm_kabul_bekle(o, "HEARTBEAT kaybı DISARMED'de");
    /* Yalnız WARN, kaydı arm reddi gerektirmiyor: sızıntı algılayıcısı bayat. */
    ortam_kur(o);
    o->besle_leak = false;
    ilerle(o, 2500);
    TEST_ASSERT_EQUAL_UINT8(IDA_FS_WARN, o->g.fs_level);
    ida_gun_kur(&o->gun);
    arm_kabul_bekle(o, "WARN, arm reddi yok");
}

static void test_md2_arm_reddi_gerektiren_ariza_kayitlari(void)
{
    ortam_t *o = &O;
    /* Sızıntı (mandallı) */
    ortam_kur(o);
    o->besle_leak = false;
    ornek(o, (uint8_t)IDA_SENS_LEAK, 0.0f, 1u);
    o->now += 10 * MS;
    ornek(o, (uint8_t)IDA_SENS_LEAK, 0.0f, 1u);
    bir_adim(o);
    arm_ret_bekle(o, "sızıntı");
    /* Düşük batarya (W3-K3), DISARMED'de yalnız WARN olsa da */
    ortam_kur(o);
    o->batt_v = 13.0f;
    ilerle(o, 10500);
    TEST_ASSERT_EQUAL_UINT8(IDA_FS_WARN, o->g.fs_level);
    ida_gun_kur(&o->gun);
    arm_ret_bekle(o, "düşük batarya");
    /* Düşük batarya süresi dolmadan: kabul */
    ortam_kur(o);
    o->batt_v = 13.0f;
    ilerle(o, 5000);
    ida_gun_kur(&o->gun);
    arm_kabul_bekle(o, "düşük batarya süresi dolmadı");
    /* Kritik batarya */
    ortam_kur(o);
    o->batt_v = 12.0f;
    ilerle(o, 10500);
    ida_gun_kur(&o->gun);
    arm_ret_bekle(o, "kritik batarya");
    /* Parametre kümesi geçersiz */
    ortam_kur(o);
    o->gi.param_ok = false;
    bir_adim(o);
    arm_ret_bekle(o, "parametre");
    /* Mandallı kestirim kötülüğü, kestirim düzelmiş olsa da (FAILSAFE_RESET yok) */
    ortam_kur(o);
    o->gi.est_level = (uint8_t)IDA_EST_BAD;
    ilerle(o, 2500);
    o->gi.est_level = (uint8_t)IDA_EST_DEGRADED;
    ilerle(o, 3000);
    ida_gun_kur(&o->gun);
    arm_ret_bekle(o, "mandallı EST_BAD");
    TEST_ASSERT_EQUAL_UINT8(IDA_ACK_ACCEPTED, komut(o, (uint8_t)IDA_CMD_FAILSAFE_RESET, 0u).ack);
    bir_adim(o);
    arm_kabul_bekle(o, "FAILSAFE_RESET sonrası");
}

static void test_md2_emniyet_girdileri_arm_kosulu_degil(void)
{
    /* W3-K1: sanal emniyet girdileri (acil durdurma, fiş, pim) araç ARM'ının ön koşulu
       değildir. */
    ortam_t *o = &O;
    ortam_kur(o);
    o->e.kisitli = (uint8_t)IDA_EMN_TUMU;
    bir_adim(o);
    arm_kabul_bekle(o, "init etkin değerleri");
    /* Bağlantı: A1 oturumu ayrı ön koşul değil (W3-K3); hiç HEARTBEAT gelmemiş olsa da. */
    ortam_kur(o);
    ida_gor_kur(&o->g);
    o->besle_hb = false;
    o->oturum = false;
    o->e.mode = (uint8_t)IDA_MODE_INIT;
    ilerle(o, 500);
    TEST_ASSERT_EQUAL_UINT8(IDA_MODE_DISARMED, o->e.mode);
    ida_gun_kur(&o->gun);
    arm_kabul_bekle(o, "HEARTBEAT ve oturum yok");
}

static void test_md2_kosul_tutmazken_silahliyken(void)
{
    /* 3.17 sırası: ön koşul tutmuyorsa NOT_ARMABLE, "zaten silahlı" ondan sonra; silahsızlandırmaz. */
    ortam_t *o = &O;
    ortam_kur(o);
    arm_kabul_bekle(o, "ilk");
    o->gi.est_level = (uint8_t)IDA_EST_BAD;
    bir_adim(o);
    const ida_cmd_result_t r = komut(o, (uint8_t)IDA_CMD_ARM, 0u);
    TEST_ASSERT_EQUAL_UINT16(IDA_REASON_NOT_ARMABLE, r.reason);
    TEST_ASSERT_TRUE(o->e.armed);
}

static void test_md2_daraltmayla_ayni_adim(void)
{
    /* (b) daraltması: dağıtıcı NARROWED verir; modül de savunma olarak reddeder. */
    ortam_t *o = &O;
    ortam_kur(o);
    o->e.d.herhangi = true;
    o->e.d.disarm = true;
    const ida_cmd_result_t r = komut(o, (uint8_t)IDA_CMD_ARM, 0u);
    TEST_ASSERT_EQUAL_UINT16(IDA_REASON_NARROWED, r.reason);
    TEST_ASSERT_FALSE(o->e.armed);
}

/* ---- Kontrol isteği (kapsam md.4; W3-kontrol'ün ida_kon_istek_t'si) --------------------- */

static void istek_al(ortam_t *o, ida_kon_istek_t *ist)
{
    ida_gor_istek(&o->g, &o->e, ist);
}

static void test_istek(void)
{
    ortam_t *o = &O;
    ida_kon_istek_t ist;
    /* Silahsız: KAPALI, alanlar 0. */
    ortam_kur(o);
    o->e.mode = (uint8_t)IDA_MODE_DEPTH_HOLD;
    bir_adim(o);
    istek_al(o, &ist);
    TEST_ASSERT_EQUAL_UINT8(IDA_KON_KIP_KAPALI, ist.kip);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, ist.derinlik);
    /* DEPTH_HOLD, silahlı, kestirim DEGRADED (havuz): derinlik ve yön tutulur, X/Y operatörden. */
    o->gi.est_level = (uint8_t)IDA_EST_DEGRADED;
    bir_adim(o);
    arm_kabul_bekle(o, "DH");
    o->gi.nav_derinlik = 2.5f; /* araç hareket etti; hedef yakalanan değer */
    bir_adim(o);
    istek_al(o, &ist);
    TEST_ASSERT_EQUAL_UINT8(IDA_KON_KIP_DALIS, ist.kip);
    TEST_ASSERT_EQUAL_UINT8(IDA_KON_DIKEY_DERINLIK, ist.dikey);
    TEST_ASSERT_EQUAL_UINT8(IDA_KON_TUTUM_ACI, ist.tutum);
    TEST_ASSERT_EQUAL_UINT8(IDA_KON_YON_ACI, ist.yon_kanali);
    TEST_ASSERT_EQUAL_UINT8(IDA_KON_YATAY_KUVVET, ist.yatay);
    TEST_ASSERT_EQUAL_FLOAT(2.0f, ist.derinlik);
    TEST_ASSERT_EQUAL_FLOAT(0.5f, ist.yon);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, ist.yalpa);
    /* Acil durdurma itkiye girmez (W3-K1). */
    o->e.kisitli = (uint8_t)IDA_EMN_TUMU;
    istek_al(o, &ist);
    TEST_ASSERT_EQUAL_UINT8(IDA_KON_KIP_DALIS, ist.kip);
    /* Yön bileşeni gider: yön açısal hız 0; derinlik bileşeni gider: dikey kuvvet 0. */
    g_elle = true;
    o->gi.gecerlilik = (uint8_t)(IDA_KES_G_TUTUM | IDA_KES_G_DERINLIK | IDA_KES_G_DIKEY_HIZ);
    bir_adim(o);
    istek_al(o, &ist);
    TEST_ASSERT_EQUAL_UINT8(IDA_KON_YON_HIZ, ist.yon_kanali);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, ist.yon_hizi);
    TEST_ASSERT_EQUAL_UINT8(IDA_KON_DIKEY_DERINLIK, ist.dikey);
    o->gi.gecerlilik = (uint8_t)IDA_KES_G_TUTUM;
    bir_adim(o);
    istek_al(o, &ist);
    TEST_ASSERT_EQUAL_UINT8(IDA_KON_DIKEY_KUVVET, ist.dikey);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, ist.kuvvet[2]);
    o->gi.gecerlilik = 0u;
    bir_adim(o);
    istek_al(o, &ist);
    TEST_ASSERT_EQUAL_UINT8(IDA_KON_TUTUM_HIZ, ist.tutum);
    g_elle = false;

    /* MANUAL: kuvvet ve moment kanalları (operatör, A4-K49). */
    ortam_kur(o);
    o->e.mode = (uint8_t)IDA_MODE_MANUAL;
    bir_adim(o);
    arm_kabul_bekle(o, "MANUAL");
    bir_adim(o);
    istek_al(o, &ist);
    TEST_ASSERT_EQUAL_UINT8(IDA_KON_KIP_DALIS, ist.kip);
    TEST_ASSERT_EQUAL_UINT8(IDA_KON_DIKEY_KUVVET, ist.dikey);
    TEST_ASSERT_EQUAL_UINT8(IDA_KON_TUTUM_MOMENT, ist.tutum);
    TEST_ASSERT_EQUAL_UINT8(IDA_KON_YON_MOMENT, ist.yon_kanali);
    TEST_ASSERT_EQUAL_UINT8(IDA_KON_YATAY_KUVVET, ist.yatay);
    /* HEARTBEAT kaybı MANUAL'da HOLD: operatör kanalı yok, derinlik/yön tutulur, konum OK ise
       yatay hız 0. */
    o->besle_hb = false;
    ilerle(o, 3500);
    TEST_ASSERT_EQUAL_UINT8(IDA_FS_HOLD, o->g.fs_level);
    istek_al(o, &ist);
    TEST_ASSERT_EQUAL_UINT8(IDA_KON_DIKEY_DERINLIK, ist.dikey);
    TEST_ASSERT_EQUAL_UINT8(IDA_KON_YON_ACI, ist.yon_kanali);
    TEST_ASSERT_EQUAL_UINT8(IDA_KON_YATAY_HIZ, ist.yatay);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, ist.hiz[0]);
    /* 60 s: SURFACE → yükselme hızı. */
    ilerle(o, 57000);
    TEST_ASSERT_EQUAL_UINT8(IDA_FS_SURFACE, o->g.fs_level);
    istek_al(o, &ist);
    TEST_ASSERT_EQUAL_UINT8(IDA_KON_DIKEY_HIZ, ist.dikey);
    TEST_ASSERT_EQUAL_FLOAT(-0.25f, ist.dikey_hiz);
    /* Yüzeye çıktı: YUZEY (dikey kapalı). */
    o->gi.nav_derinlik = 0.2f;
    bir_adim(o);
    istek_al(o, &ist);
    TEST_ASSERT_EQUAL_UINT8(IDA_KON_KIP_YUZEY, ist.kip);

    /* HOLD, konum yok: yatay itki sıfır (kuvvet 0). */
    ortam_kur(o);
    o->e.mode = (uint8_t)IDA_MODE_MANUAL;
    o->gi.est_level = (uint8_t)IDA_EST_DEGRADED;
    bir_adim(o);
    arm_kabul_bekle(o, "MANUAL 2");
    o->besle_hb = false;
    ilerle(o, 3500);
    istek_al(o, &ist);
    TEST_ASSERT_EQUAL_UINT8(IDA_KON_YATAY_KUVVET, ist.yatay);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, ist.kuvvet[0]);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, ist.kuvvet[1]);

    /* STATION_KEEP (konum OK): derinlik, yön, yatay hız 0. */
    ortam_kur(o);
    o->e.mode = (uint8_t)IDA_MODE_STATION_KEEP;
    bir_adim(o);
    arm_kabul_bekle(o, "SK");
    bir_adim(o);
    istek_al(o, &ist);
    TEST_ASSERT_EQUAL_UINT8(IDA_KON_YATAY_HIZ, ist.yatay);
    TEST_ASSERT_EQUAL_UINT8(IDA_KON_DIKEY_DERINLIK, ist.dikey);

    /* PASSIVE_SURFACE: KAPALI, arm korunur (02 §4.2). */
    ortam_kur(o);
    o->e.mode = (uint8_t)IDA_MODE_STABILIZE;
    bir_adim(o);
    arm_kabul_bekle(o, "STB");
    istek_al(o, &ist);
    TEST_ASSERT_EQUAL_UINT8(IDA_KON_YON_HIZ, ist.yon_kanali);
    o->gi.imu_bayat = true;
    bir_adim(o);
    TEST_ASSERT_EQUAL_UINT8(IDA_FS_PASSIVE_SURFACE, o->g.fs_level);
    TEST_ASSERT_TRUE(o->e.armed);
    istek_al(o, &ist);
    TEST_ASSERT_EQUAL_UINT8(IDA_KON_KIP_KAPALI, ist.kip);

    /* Sızıntı algılayıcısı bayat: derinleşme yok (hedef, bulunulan derinliğe kırpılır). */
    ortam_kur(o);
    o->e.mode = (uint8_t)IDA_MODE_DEPTH_HOLD;
    o->gi.nav_derinlik = 3.0f;
    bir_adim(o);
    arm_kabul_bekle(o, "DH 3 m");
    o->besle_leak = false;
    o->gi.nav_derinlik = 2.0f;
    ilerle(o, 2500);
    istek_al(o, &ist);
    TEST_ASSERT_EQUAL_UINT8(IDA_KON_KIP_DALIS, ist.kip);
    TEST_ASSERT_EQUAL_FLOAT(2.0f, ist.derinlik);

    /* DISARMED kipinde silahlı (A4-K127 bağımsız): motorlar nötr. */
    ortam_kur(o);
    arm_kabul_bekle(o, "DISARMED kipi");
    bir_adim(o);
    istek_al(o, &ist);
    TEST_ASSERT_EQUAL_UINT8(IDA_KON_KIP_KAPALI, ist.kip);
    TEST_ASSERT_EQUAL_UINT8(0u, ida_gor_itici_disi(&o->g));
}

/* ---- Çekirdek yolu: dağıtıcı, çerçeve, günlük ------------------------------------------- */

static ida_core_t C;
static ida_in_t IN;
static ida_out_t OUT;
static uint32_t g_seq;
static int64_t g_t;

static ida_rc_t step(void)
{
    g_seq++;
    g_t += 5 * DS_MS;
    IN.now_w.ns = g_t;
    IN.tf.present = true;
    IN.tf.seq = g_seq;
    IN.tf.t.ns = g_t;
    IN.tf.t_rx.ns = g_t;
    IN.imu_present = true;
    ds_imu(&IN.imu, g_t, g_seq);
    const ida_rc_t rc = ida_core_step(&C, &IN, &OUT);
    ds_cikti_degismezleri(rc, &IN, &OUT);
    return rc;
}

static void komut_ekle(uint32_t i, uint32_t id, uint8_t tur)
{
    (void)memset(&IN.cmds[i], 0, sizeof(IN.cmds[i]));
    IN.cmds[i].cmd_id = id;
    IN.cmds[i].type = tur;
    if (IN.n_cmds <= i) {
        IN.n_cmds = (uint8_t)(i + 1u);
    }
}

static void hazirla(bool blok)
{
    TEST_ASSERT_EQUAL(blok ? IDA_RC_OK : IDA_RC_PARAM_DEFAULTS,
                      blok ? ds_kur(&C) : ds_kur_bloksuz(&C));
    g_seq = 0u;
    g_t = 2000 * DS_MS;
    ds_girdi(&IN, g_t, 0u, true);
}

static void test_cekirdek_komut_satirlari(void)
{
    hazirla(true);
    komut_ekle(0u, 1u, (uint8_t)IDA_CMD_HEARTBEAT);
    komut_ekle(1u, 2u, (uint8_t)IDA_CMD_ARM);
    komut_ekle(2u, 3u, (uint8_t)IDA_CMD_FAILSAFE_RESET);
    komut_ekle(3u, 4u, (uint8_t)IDA_CMD_MISSION_BEGIN);
    komut_ekle(4u, 5u, (uint8_t)IDA_CMD_SET_MODE);
    IN.cmds[4].u.set_mode.mode = (uint8_t)IDA_MODE_GUIDED;
    TEST_ASSERT_EQUAL(IDA_RC_OK, step());
    TEST_ASSERT_EQUAL_UINT8(IDA_ACK_ACCEPTED, ds_sonuc(&OUT, 1u)->ack);
    /* yer tutucu kestirim INIT: arm ön koşulu tutmuyor */
    TEST_ASSERT_EQUAL_UINT16(IDA_REASON_NOT_ARMABLE, ds_sonuc(&OUT, 2u)->reason);
    TEST_ASSERT_EQUAL_UINT8(IDA_ACK_ACCEPTED, ds_sonuc(&OUT, 3u)->ack); /* mandal yok (3.17) */
    TEST_ASSERT_EQUAL_UINT8(IDA_ACK_UNSUPPORTED, ds_sonuc(&OUT, 4u)->ack);
    TEST_ASSERT_EQUAL_UINT16(IDA_REASON_BAD_MODE, ds_sonuc(&OUT, 5u)->reason);
    TEST_ASSERT_FALSE(OUT.armed);
    TEST_ASSERT_EQUAL_UINT8(IDA_MODE_INIT, OUT.status.mode);
    TEST_ASSERT_EQUAL_UINT8(IDA_FS_NONE, OUT.status.fs_level);
    TEST_ASSERT_TRUE(OUT.health.core_ok);
    TEST_ASSERT_TRUE(C.gor.gcs_var);
    /* Yineleme: DUPLICATE, etkisiz (A4-K48, dağıtıcı). */
    IN.n_cmds = 0u;
    komut_ekle(0u, 2u, (uint8_t)IDA_CMD_ARM);
    TEST_ASSERT_EQUAL(IDA_RC_OK, step());
    TEST_ASSERT_EQUAL_UINT8(IDA_ACK_DUPLICATE, ds_sonuc(&OUT, 2u)->ack);
}

static void test_cekirdek_w2_kurallari_once(void)
{
    /* Bloksuz init: PARAM_INVALID (W2) görev modülünden önce. */
    hazirla(false);
    komut_ekle(0u, 1u, (uint8_t)IDA_CMD_ARM);
    TEST_ASSERT_EQUAL(IDA_RC_OK, step());
    TEST_ASSERT_EQUAL_UINT16(IDA_REASON_PARAM_INVALID, ds_sonuc(&OUT, 1u)->reason);
    /* DISARM mandalı + ARM: NARROWED (A4-K45). */
    hazirla(true);
    IN.narrow.mask = IDA_NARROW_DISARM;
    IN.narrow.cmd_id[0] = 7u;
    komut_ekle(0u, 8u, (uint8_t)IDA_CMD_ARM);
    komut_ekle(1u, 9u, (uint8_t)IDA_CMD_FAILSAFE_RESET);
    TEST_ASSERT_EQUAL(IDA_RC_OK, step());
    TEST_ASSERT_EQUAL_UINT16(IDA_REASON_NARROWED, ds_sonuc(&OUT, 8u)->reason);
    TEST_ASSERT_EQUAL_UINT16(IDA_REASON_NARROWED, ds_sonuc(&OUT, 9u)->reason);
}

static void test_cekirdek_gunluk_yolu(void)
{
    /* Test kancası: INIT dışı kipte silahlı, acil durdurma init değerinde (kısıtlayan): W3-K1
       gereği silahlı kalır. Arıza düzeyi ve FAILSAFE_CHANGE çekirdeğin günlüğünden çıkar. */
    hazirla(true);
    TEST_ASSERT_EQUAL(IDA_RC_OK, step());
    C.emn.mode = (uint8_t)IDA_MODE_DEPTH_HOLD;
    C.gor.son_kip = (uint8_t)IDA_MODE_DEPTH_HOLD;
    C.emn.armed = true;
    TEST_ASSERT_EQUAL(IDA_RC_OK, step());
    TEST_ASSERT_TRUE(OUT.armed);
    TEST_ASSERT_EQUAL_UINT8(IDA_MODE_DEPTH_HOLD, OUT.status.mode);
    /* DEPTH_HOLD'da batarya ve sızıntı örneği yok: WARN, nedenler LEAK ve BATT_LOW. */
    TEST_ASSERT_EQUAL_UINT8(IDA_FS_WARN, OUT.status.fs_level);
    TEST_ASSERT_EQUAL_HEX32(IDA_FSR_LEAK | IDA_FSR_BATT_LOW,
                            OUT.status.fs_reasons & ~IDA_FSR_IMU_LAG);
    TEST_ASSERT_EQUAL_UINT32(1u, ds_gunluk_say(&OUT, (uint16_t)IDA_LOG_FAILSAFE_CHANGE));
    /* Tanımsız kip (bozulma): core_ok 0, silahsız ve kip DISARMED (EMNİYET). */
    C.emn.mode = 11u;
    TEST_ASSERT_EQUAL(IDA_RC_OK, step());
    TEST_ASSERT_FALSE(OUT.health.core_ok);
    TEST_ASSERT_FALSE(OUT.armed);
    TEST_ASSERT_EQUAL_UINT8(IDA_MODE_DISARMED, OUT.status.mode);
    TEST_ASSERT_EQUAL_UINT32(1u, ds_gunluk_say(&OUT, (uint16_t)IDA_LOG_MODE_CHANGE));
    TEST_ASSERT_EQUAL_UINT32(1u, ds_gunluk_say(&OUT, (uint16_t)IDA_LOG_ARM_CHANGE));
    /* Güdüm erişimcisi (kontrol modülünün tek okuma noktası) aynı isteği verir. */
    ida_kon_istek_t a;
    ida_kon_istek_t b;
    ida_gud_kon_istegi(&C.gud, &C, &a);
    ida_gor_istek(&C.gor, &C.emn, &b);
    TEST_ASSERT_EQUAL_MEMORY(&b, &a, sizeof(a));
    TEST_ASSERT_EQUAL_UINT8(IDA_KON_KIP_KAPALI, a.kip);
}

static void test_cekirdek_ornekler_ve_param_grubu(void)
{
    hazirla(true);
    ida_sample_t *s = &IN.samples[0];
    (void)memset(s, 0, sizeof(*s));
    s->hdr.type = (uint8_t)IDA_SENS_BATT;
    s->hdr.flags = IDA_HF_VALID;
    s->hdr.seq = 1u;
    s->hdr.window_ns = 100000000u;
    s->hdr.t_meas.ns = g_t;
    s->hdr.t_rx.ns = g_t;
    s->u.batt.voltage = 15.5f;
    IN.n_samples = 1u;
    TEST_ASSERT_EQUAL(IDA_RC_OK, step());
    TEST_ASSERT_TRUE(C.gor.batt_var);
    TEST_ASSERT_EQUAL_FLOAT(15.5f, C.gor.batt_v);
    /* Grup: BATT_KRIT_V ≥ BATT_DUSUK_V ya da GCS_KAYIP_MS ≥ GCS_KAYIP2_MS geçersiz. */
    ida_prm_kume_t k;
    ida_prm_varsayilan(&k);
    TEST_ASSERT_EQUAL_UINT8(0u, ida_gor_param_grup(&k));
    k.ham[IDA_PRM_gor_gcs1_ms] = k.ham[IDA_PRM_gor_gcs2_ms];
    TEST_ASSERT_EQUAL_UINT8(1u, ida_gor_param_grup(&k));
    ida_prm_varsayilan(&k);
    k.ham[IDA_PRM_gor_batt_krit_v] = k.ham[IDA_PRM_gor_batt_dusuk_v];
    TEST_ASSERT_EQUAL_UINT8(1u, ida_gor_param_grup(&k));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_md2_tum_kosullar_tutarken_kabul);
    RUN_TEST(test_md2_kestirim_duzeyi);
    RUN_TEST(test_md2_ariza_eylemi);
    RUN_TEST(test_md2_arm_reddi_gerektiren_ariza_kayitlari);
    RUN_TEST(test_md2_emniyet_girdileri_arm_kosulu_degil);
    RUN_TEST(test_md2_kosul_tutmazken_silahliyken);
    RUN_TEST(test_md2_daraltmayla_ayni_adim);
    RUN_TEST(test_istek);
    RUN_TEST(test_cekirdek_komut_satirlari);
    RUN_TEST(test_cekirdek_w2_kurallari_once);
    RUN_TEST(test_cekirdek_gunluk_yolu);
    RUN_TEST(test_cekirdek_ornekler_ve_param_grubu);
    return UNITY_END();
}
