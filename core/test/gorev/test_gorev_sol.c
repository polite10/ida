/*
 * test_gorev_sol.c — sol incelemesinin (docs/gorevler/inceleme/W3-gorev-sol.md) karşı örnekleri
 * ve mimar kararları W3-K7, W3-K9. Her test bir bulgunun karşı örneğidir; düzeltmeden önce
 * kalır, sonra geçer.
 *
 *   B1 kip değişince arıza düzeyi ve ARM engeli eski kipten kalıyordu (iki komut sırası;
 *      W3-K13: her sıra ardışık iki çağrıyla aynı sonucu verir);
 *   B2 çağrı içinde ıslak, ıslak, kuru: sızıntı eşik aşımı kayboluyordu;
 *   B3 yatay geçerlilik iki tanımla değerlendiriliyordu (yalnız yatay hız kaybı);
 *   B4 bir sızıntı algılayıcısının kaybını öteki gizliyordu;
 *   B5 tanımsız arıza düzeyi güvenli eylem uygulanmadan siliniyordu; ikinci tur: aynı
 *      çağrıda örneğin ya da oturum başının üzerine yazdığı bozulma (port yolu);
 *   K9 adımın arıza daraltması dağıtıcıda A4-K45 konumunda NARROWED alır (IMU_REJECTED'den
 *      önce) ve widen_blocked'a sayılır;
 *   K7 GUIDED girişi yatay geçerlilik istemez; konum kaybı GUIDED'da eylem üretmez.
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
    bool g_elle;
    uint32_t seq;
} ortam_t;

static ortam_t O;
static ortam_t O2;

static uint8_t gecerlilik_turet(uint8_t est)
{
    const uint8_t dikey =
        (uint8_t)(IDA_KES_G_TUTUM | IDA_KES_G_YON | IDA_KES_G_DERINLIK | IDA_KES_G_DIKEY_HIZ);
    if (est == IDA_EST_OK) {
        return (uint8_t)(dikey | IDA_KES_G_YATAY_HIZ | IDA_KES_G_KONUM);
    }
    return (est == IDA_EST_DEGRADED) ? dikey : (uint8_t)0u;
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
    ida_gor_komut_ic(&o->g, &o->e, &b, &c, &r, &o->p, &o->gun);
    return r;
}

static void ornek(ortam_t *o, uint8_t tur, uint8_t inst, float v, uint8_t mask)
{
    ida_sample_t s;
    (void)memset(&s, 0, sizeof(s));
    s.hdr.type = tur;
    s.hdr.inst = inst;
    s.hdr.t_rx.ns = o->now;
    if (tur == (uint8_t)IDA_SENS_BATT) {
        s.u.batt.voltage = v;
    } else {
        s.u.leak.mask = mask;
    }
    ida_gor_ornek(&o->g, &s);
}

static void adim(ortam_t *o)
{
    if (o->besle_batt) {
        ornek(o, (uint8_t)IDA_SENS_BATT, 0u, 16.0f, 0u);
    }
    if (o->besle_leak) {
        ornek(o, (uint8_t)IDA_SENS_LEAK, 0u, 0.0f, 0u);
    }
    o->gi.now_ns = o->now;
    if (!o->g_elle) {
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
    ilerle(o, 500);
    ida_gun_kur(&o->gun);
}

static void kip_kur(ortam_t *o, uint8_t kip)
{
    o->e.mode = kip;
    o->g.son_kip = kip;
    o->g.mission_state = (kip == (uint8_t)IDA_MODE_AUTO) ? (uint8_t)IDA_MS_RUNNING
                                                          : (uint8_t)IDA_MS_IDLE;
}

/* ---- B1 ------------------------------------------------------------------------------------ */

/* Silahsız DEPTH_HOLD, süregelen HEARTBEAT kaybı (DEPTH_HOLD'da WARN, GUIDED'da HOLD), geçerli
   yatay kestirim; aynı çağrıda SET_MODE(GUIDED) ve ARM. */
static void b1_kur(ortam_t *o)
{
    ortam_kur(o);
    kip_kur(o, (uint8_t)IDA_MODE_DEPTH_HOLD);
    o->besle_hb = false;
    ilerle(o, 3500);
    o->now += 5 * MS;
    adim(o);
    o->now += 5 * MS;
    adim(o);
    TEST_ASSERT_EQUAL_UINT8(IDA_FS_WARN, o->g.fs_level);
    TEST_ASSERT_FALSE(o->g.adim_daralt);
}

/* W3-K13: SET_MODE yalnız §3.17'ye göre kabul ya da reddedilir. Kabul edilen kip değişiminde
   arıza düzeyi yeni kipin sütununa göre o anda yeniden hesaplanır; aynı çağrıda sonra gelen
   komutlar güncel düzeyi görür. Her sıranın sonucu, iki komutun ardışık iki çağrıda
   işlenmesindeki sonuçla aynıdır. ARM, işlendiği anda arıza eylemi varken kabul edilmez. */

typedef struct {
    ida_cmd_result_t ilk;
    ida_cmd_result_t ikinci;
    bool armed;
    uint8_t kip;
    uint8_t fs;
} b1_sonuc_t;

static void b1_sonuc_al(const ortam_t *o, const ida_cmd_result_t *ilk,
                        const ida_cmd_result_t *ikinci, b1_sonuc_t *s)
{
    s->ilk = *ilk;
    s->ikinci = *ikinci;
    s->armed = o->e.armed;
    s->kip = o->e.mode;
    s->fs = o->g.fs_level;
}

/* Aynı çağrıda iki komut (tur1, tur2) ya da ardışık iki çağrıda (araya bir adım). */
static void b1_kos(ortam_t *o, uint8_t tur1, uint8_t tur2, bool ardisik, b1_sonuc_t *s)
{
    b1_kur(o);
    const ida_cmd_result_t r1 = komut(o, tur1, (uint8_t)IDA_MODE_GUIDED);
    if (ardisik) {
        o->now += 5 * MS;
        adim(o);
    }
    const ida_cmd_result_t r2 = komut(o, tur2, (uint8_t)IDA_MODE_GUIDED);
    b1_sonuc_al(o, &r1, &r2, s);
    /* Çağrı sonundaki düzey bulunulan kipin sütunudur: sonraki adım onu değiştirmez. */
    o->now += 5 * MS;
    adim(o);
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(s->fs, o->g.fs_level, "çağrı sonundaki düzey eski kipten");
}

static void b1_ayni(const b1_sonuc_t *a, const b1_sonuc_t *b)
{
    TEST_ASSERT_EQUAL_UINT8(a->ilk.ack, b->ilk.ack);
    TEST_ASSERT_EQUAL_UINT16(a->ilk.reason, b->ilk.reason);
    TEST_ASSERT_EQUAL_UINT8(a->ikinci.ack, b->ikinci.ack);
    TEST_ASSERT_EQUAL_UINT16(a->ikinci.reason, b->ikinci.reason);
    TEST_ASSERT_EQUAL(a->armed, b->armed);
    TEST_ASSERT_EQUAL_UINT8(a->kip, b->kip);
    TEST_ASSERT_EQUAL_UINT8(a->fs, b->fs);
}

static void test_b1_kip_degisimi_set_mode_sonra_arm(void)
{
    b1_sonuc_t ayni;
    b1_sonuc_t ardisik;
    b1_kos(&O, (uint8_t)IDA_CMD_SET_MODE, (uint8_t)IDA_CMD_ARM, false, &ayni);
    /* SET_MODE §3.17'ye göre kabul (ek ret koşulu yok); GUIDED'da HEARTBEAT kaybı HOLD; ARM
       güncel düzeyi görür: NOT_ARMABLE. */
    TEST_ASSERT_EQUAL_UINT8(IDA_ACK_ACCEPTED, ayni.ilk.ack);
    TEST_ASSERT_EQUAL_UINT16(IDA_REASON_NONE, ayni.ilk.reason);
    TEST_ASSERT_EQUAL_UINT8(IDA_ACK_REJECTED, ayni.ikinci.ack);
    TEST_ASSERT_EQUAL_UINT16(IDA_REASON_NOT_ARMABLE, ayni.ikinci.reason);
    TEST_ASSERT_FALSE(ayni.armed);
    TEST_ASSERT_EQUAL_UINT8(IDA_MODE_GUIDED, ayni.kip);
    TEST_ASSERT_EQUAL_UINT8(IDA_FS_HOLD, ayni.fs);
    b1_kos(&O2, (uint8_t)IDA_CMD_SET_MODE, (uint8_t)IDA_CMD_ARM, true, &ardisik);
    b1_ayni(&ayni, &ardisik);
}

static void test_b1_kip_degisimi_arm_sonra_set_mode(void)
{
    b1_sonuc_t ayni;
    b1_sonuc_t ardisik;
    b1_kos(&O, (uint8_t)IDA_CMD_ARM, (uint8_t)IDA_CMD_SET_MODE, false, &ayni);
    /* ARM kendi anında (DEPTH_HOLD: WARN) kabul; kip değişir; araç yeni kipin eylemiyle (HOLD)
       silahlı kalır. */
    TEST_ASSERT_EQUAL_UINT8(IDA_ACK_ACCEPTED, ayni.ilk.ack);
    TEST_ASSERT_EQUAL_UINT8(IDA_ACK_ACCEPTED, ayni.ikinci.ack);
    TEST_ASSERT_TRUE(ayni.armed);
    TEST_ASSERT_EQUAL_UINT8(IDA_MODE_GUIDED, ayni.kip);
    TEST_ASSERT_EQUAL_UINT8(IDA_FS_HOLD, ayni.fs);
    b1_kos(&O2, (uint8_t)IDA_CMD_ARM, (uint8_t)IDA_CMD_SET_MODE, true, &ardisik);
    b1_ayni(&ayni, &ardisik);
}

/* Kip değişiminin yeni sütunda kurduğu mandal ve düzey aynı çağrıda görünür; FAILSAFE_CHANGE
   üretilir. */
static void test_b1_kip_degisimi_duzeyi_aninda_yayinlar(void)
{
    ortam_t *o = &O;
    b1_kur(o);
    ida_gun_kur(&o->gun);
    const ida_cmd_result_t r = komut(o, (uint8_t)IDA_CMD_SET_MODE, (uint8_t)IDA_MODE_GUIDED);
    TEST_ASSERT_EQUAL_UINT8(IDA_ACK_ACCEPTED, r.ack);
    TEST_ASSERT_EQUAL_UINT8(IDA_FS_HOLD, o->g.fs_level);
    TEST_ASSERT_TRUE(o->g.arm_engeli);
    uint32_t n = 0u;
    for (uint32_t i = 0u; i < (uint32_t)o->gun.n; i++) {
        if (o->gun.kayit[i].code == (uint16_t)IDA_LOG_FAILSAFE_CHANGE) {
            n++;
        }
    }
    TEST_ASSERT_EQUAL_UINT32(1u, n);
}

/* ---- B2 ------------------------------------------------------------------------------------ */

static void test_b2_cagri_ici_islak_islak_kuru(void)
{
    ortam_t *o = &O;
    ortam_kur(o);
    kip_kur(o, (uint8_t)IDA_MODE_DEPTH_HOLD);
    o->e.armed = true;
    o->besle_leak = false;
    o->now += 5 * MS;
    ornek(o, (uint8_t)IDA_SENS_LEAK, 0u, 0.0f, 1u);
    o->now += 1 * MS;
    ornek(o, (uint8_t)IDA_SENS_LEAK, 0u, 0.0f, 1u);
    o->now += 1 * MS;
    ornek(o, (uint8_t)IDA_SENS_LEAK, 0u, 0.0f, 0u);
    adim(o); /* üç örnek aynı çağrıda */
    TEST_ASSERT_TRUE((o->g.mandal & IDA_GOR_K_BIT(IDA_GOR_K_SIZINTI)) != 0u);
    TEST_ASSERT_EQUAL_UINT8(IDA_FS_SURFACE, o->g.fs_level);
    TEST_ASSERT_TRUE(o->g.arm_engeli);
}

/* ---- B3 ------------------------------------------------------------------------------------ */

static void test_b3_yalniz_yatay_hiz_kaybi(void)
{
    ortam_t *o = &O;
    ortam_kur(o);
    kip_kur(o, (uint8_t)IDA_MODE_AUTO);
    o->g_elle = true;
    o->gi.gecerlilik = (uint8_t)(gecerlilik_turet(IDA_EST_OK) & (uint8_t)(0xFFu ^ IDA_KES_G_YATAY_HIZ));
    o->now += 5 * MS;
    adim(o);
    TEST_ASSERT_TRUE((o->g.etkin & IDA_GOR_K_BIT(IDA_GOR_K_EST_BOZUK)) != 0u);
    TEST_ASSERT_EQUAL_UINT8(IDA_FS_HOLD, o->g.fs_level);
    ilerle(o, 30500);
    TEST_ASSERT_EQUAL_UINT8(IDA_FS_SURFACE, o->g.fs_level);
}

static void test_b3_yalniz_konum_kaybi(void)
{
    ortam_t *o = &O;
    ortam_kur(o);
    kip_kur(o, (uint8_t)IDA_MODE_AUTO);
    o->g_elle = true;
    o->gi.gecerlilik = (uint8_t)(gecerlilik_turet(IDA_EST_OK) & (uint8_t)(0xFFu ^ IDA_KES_G_KONUM));
    o->now += 5 * MS;
    adim(o);
    TEST_ASSERT_EQUAL_UINT8(IDA_FS_HOLD, o->g.fs_level);
    /* STATION_KEEP iki kayıpta da DEPTH_HOLD'a iner. */
    ortam_kur(o);
    kip_kur(o, (uint8_t)IDA_MODE_STATION_KEEP);
    o->g_elle = true;
    o->gi.gecerlilik = (uint8_t)(gecerlilik_turet(IDA_EST_OK) & (uint8_t)(0xFFu ^ IDA_KES_G_YATAY_HIZ));
    o->now += 5 * MS;
    adim(o);
    TEST_ASSERT_EQUAL_UINT8(IDA_MODE_DEPTH_HOLD, o->e.mode);
}

/* ---- B4 ------------------------------------------------------------------------------------ */

static void test_b4_bir_sizinti_algilayicisi_kayboldu(void)
{
    ortam_t *o = &O;
    ortam_kur(o);
    /* inst 0 her adımda (besleme), inst 1 bir süre geldi, sonra kesildi. */
    for (uint32_t i = 0u; i < 5u; i++) {
        o->now += 100 * MS;
        ornek(o, (uint8_t)IDA_SENS_LEAK, 1u, 0.0f, 0u);
        adim(o);
    }
    TEST_ASSERT_TRUE((o->g.etkin & IDA_GOR_K_BIT(IDA_GOR_K_SIZINTI_BAYAT)) == 0u);
    ilerle(o, 2500);
    TEST_ASSERT_TRUE((o->g.etkin & IDA_GOR_K_BIT(IDA_GOR_K_SIZINTI_BAYAT)) != 0u);
    TEST_ASSERT_EQUAL_UINT8(IDA_FS_WARN, o->g.fs_level);
}

/* ---- B5 ------------------------------------------------------------------------------------ */

static void test_b5_tanimsiz_ariza_duzeyi(void)
{
    ortam_t *o = &O;
    ortam_kur(o);
    kip_kur(o, (uint8_t)IDA_MODE_DEPTH_HOLD);
    TEST_ASSERT_EQUAL_UINT8(IDA_ACK_ACCEPTED, komut(o, (uint8_t)IDA_CMD_ARM, 0u).ack);
    o->now += 5 * MS;
    adim(o);
    ida_gun_kur(&o->gun);
    o->g.fs_level = 6u; /* bozulma (test kancası) */
    o->now += 5 * MS;
    adim(o);
    /* Çağrı sonu: silahsız, düzey DISARM, istek kapalı, genişletme engelli. */
    TEST_ASSERT_FALSE(o->e.armed);
    TEST_ASSERT_EQUAL_UINT8(IDA_FS_DISARM, o->g.fs_level);
    ida_kon_istek_t ist;
    ida_gor_istek(&o->g, &o->e, &ist);
    TEST_ASSERT_EQUAL_UINT8(IDA_KON_KIP_KAPALI, ist.kip);
    TEST_ASSERT_TRUE(o->g.adim_daralt);
    const ida_cmd_result_t r = komut(o, (uint8_t)IDA_CMD_ARM, 0u);
    TEST_ASSERT_EQUAL_UINT8(IDA_ACK_REJECTED, r.ack);
    TEST_ASSERT_FALSE(o->e.armed);
    TEST_ASSERT_TRUE(ida_gor_tutarli(&o->g));
    /* Sonraki çağrılarda da (init'e kadar) DISARM ve arm yok. */
    ilerle(o, 1000);
    TEST_ASSERT_EQUAL_UINT8(IDA_FS_DISARM, o->g.fs_level);
    TEST_ASSERT_EQUAL_UINT16(IDA_REASON_NOT_ARMABLE, komut(o, (uint8_t)IDA_CMD_ARM, 0u).reason);
}

/* ---- K7 ------------------------------------------------------------------------------------ */

static void test_k7_guided_yatay_istemez(void)
{
    ortam_t *o = &O;
    ortam_kur(o);
    o->gi.est_level = (uint8_t)IDA_EST_DEGRADED; /* havuz: tutum, yön, derinlik; yatay yok */
    o->now += 5 * MS;
    adim(o);
    o->now += 5 * MS;
    adim(o);
    const ida_cmd_result_t r = komut(o, (uint8_t)IDA_CMD_SET_MODE, (uint8_t)IDA_MODE_GUIDED);
    TEST_ASSERT_EQUAL_UINT8(IDA_ACK_ACCEPTED, r.ack);
    TEST_ASSERT_EQUAL_UINT8(IDA_MODE_GUIDED, o->e.mode);
    ilerle(o, 31000);
    TEST_ASSERT_EQUAL_UINT8(IDA_FS_NONE, o->g.fs_level); /* konum kaybı GUIDED'da eylem yok */
    /* AUTO yatay ister. */
    o->g.mission_state = (uint8_t)IDA_MS_LOADED;
    TEST_ASSERT_EQUAL_UINT16(IDA_REASON_BAD_MODE,
                             komut(o, (uint8_t)IDA_CMD_SET_MODE, (uint8_t)IDA_MODE_AUTO).reason);
    /* GUIDED derinlik bileşeni ister. */
    ortam_kur(o);
    o->g_elle = true;
    o->gi.gecerlilik = (uint8_t)(IDA_KES_G_TUTUM | IDA_KES_G_YON);
    o->now += 5 * MS;
    adim(o);
    TEST_ASSERT_EQUAL_UINT16(IDA_REASON_BAD_MODE,
                             komut(o, (uint8_t)IDA_CMD_SET_MODE, (uint8_t)IDA_MODE_GUIDED).reason);
}

/* ---- K9: çekirdek yolu ------------------------------------------------------------------- */

static ida_core_t C;
static ida_in_t IN;
static ida_out_t OUT;

static void test_k9_adim_daraltmasi_dagiticida(void)
{
    TEST_ASSERT_EQUAL(IDA_RC_OK, ds_kur(&C));
    int64_t t = 2000 * DS_MS;
    ds_girdi(&IN, t, 1u, true);
    TEST_ASSERT_EQUAL(IDA_RC_OK, ida_core_step(&C, &IN, &OUT));
    /* Test kancası: tanımsız arıza düzeyi → adımda güvenli daraltma. IMU'su reddedilen step:
       NARROWED (A4-K45 konumu) IMU_REJECTED'den önce gelir; widen_blocked artar. */
    C.gor.fs_level = 7u;
    t += 5 * DS_MS;
    ds_girdi(&IN, t, 2u, true);
    IN.imu.hdr.flags = 0u; /* IMU reddi */
    IN.n_cmds = 2u;
    IN.cmds[0].cmd_id = 50u;
    IN.cmds[0].type = (uint8_t)IDA_CMD_FAILSAFE_RESET;
    IN.cmds[1].cmd_id = 51u;
    IN.cmds[1].type = (uint8_t)IDA_CMD_HEARTBEAT;
    const uint32_t wb0 = OUT.status.cnt.widen_blocked;
    TEST_ASSERT_EQUAL(IDA_RC_IMU_REJECTED, ida_core_step(&C, &IN, &OUT));
    TEST_ASSERT_EQUAL_UINT8(IDA_ACK_REJECTED, ds_sonuc(&OUT, 50u)->ack);
    TEST_ASSERT_EQUAL_UINT16(IDA_REASON_NARROWED, ds_sonuc(&OUT, 50u)->reason);
    TEST_ASSERT_EQUAL_UINT8(IDA_ACK_ACCEPTED, ds_sonuc(&OUT, 51u)->ack); /* nötr */
    TEST_ASSERT_EQUAL_UINT32(wb0 + 1u, OUT.status.cnt.widen_blocked);
    TEST_ASSERT_EQUAL_UINT8(IDA_FS_DISARM, OUT.status.fs_level);
    TEST_ASSERT_FALSE(OUT.health.core_ok);
}

/* ---- B5 (ikinci tur): üzerine yazılan alan bozulması, gerçek çerçeve sırasıyla ----------- */

static void b5_hazirla(int64_t *t)
{
    TEST_ASSERT_EQUAL(IDA_RC_OK, ds_kur(&C));
    *t = 2000 * DS_MS;
    ds_girdi(&IN, *t, 1u, true);
    TEST_ASSERT_EQUAL(IDA_RC_OK, ida_core_step(&C, &IN, &OUT));
    /* Test kancası: silahlı DEPTH_HOLD, arıza eylemi yok. */
    C.emn.mode = (uint8_t)IDA_MODE_DEPTH_HOLD;
    C.gor.son_kip = (uint8_t)IDA_MODE_DEPTH_HOLD;
    C.emn.armed = true;
    C.gor.fs_level = (uint8_t)IDA_FS_NONE;
}

/* Çağrı sonu: silahsız, DISARM, istek KAPALI, aynı çağrının genişleticisi NARROWED; sonraki
   çağrıda da arm yok. */
static void b5_cagri_sonu(int64_t *t)
{
    TEST_ASSERT_FALSE(OUT.health.core_ok);
    TEST_ASSERT_FALSE(OUT.armed);
    TEST_ASSERT_FALSE(C.emn.armed);
    TEST_ASSERT_EQUAL_UINT8(IDA_FS_DISARM, OUT.status.fs_level);
    TEST_ASSERT_TRUE(OUT.is_safe);
    ida_kon_istek_t ist;
    ida_gud_kon_istegi(&C.gud, &C, &ist);
    TEST_ASSERT_EQUAL_UINT8(IDA_KON_KIP_KAPALI, ist.kip);
    TEST_ASSERT_EQUAL_UINT16(IDA_REASON_NARROWED, ds_sonuc(&OUT, 60u)->reason);
    *t += 5 * DS_MS;
    ds_girdi(&IN, *t, 4u, true);
    IN.n_cmds = 1u;
    IN.cmds[0].cmd_id = 61u;
    IN.cmds[0].type = (uint8_t)IDA_CMD_ARM;
    TEST_ASSERT_EQUAL(IDA_RC_OK, ida_core_step(&C, &IN, &OUT));
    TEST_ASSERT_EQUAL_UINT16(IDA_REASON_NOT_ARMABLE, ds_sonuc(&OUT, 61u)->reason);
    TEST_ASSERT_FALSE(OUT.armed);
}

static void test_b5_ornegin_ustune_yazdigi_bozulma(void)
{
    int64_t t = 0;
    b5_hazirla(&t);
    const uint8_t iki = 2u;
    (void)memcpy(&C.gor.batt_var, &iki, 1u); /* bozulma: bool ham baytı 2 */
    t += 5 * DS_MS;
    ds_girdi(&IN, t, 3u, true);
    ida_sample_t *s = &IN.samples[0];
    (void)memset(s, 0, sizeof(*s));
    s->hdr.type = (uint8_t)IDA_SENS_BATT;
    s->hdr.flags = IDA_HF_VALID;
    s->hdr.seq = 1u;
    s->hdr.window_ns = 100000000u;
    s->hdr.t_meas.ns = t;
    s->hdr.t_rx.ns = t;
    s->u.batt.voltage = 16.0f;
    IN.n_samples = 1u; /* aynı çağrıda batt_var'ı yazan geçerli örnek */
    IN.n_cmds = 1u;
    IN.cmds[0].cmd_id = 60u;
    IN.cmds[0].type = (uint8_t)IDA_CMD_FAILSAFE_RESET;
    TEST_ASSERT_EQUAL(IDA_RC_OK, ida_core_step(&C, &IN, &OUT));
    b5_cagri_sonu(&t);
}

static void test_b5_oturum_basinin_ustune_yazdigi_bozulma(void)
{
    int64_t t = 0;
    b5_hazirla(&t);
    const uint8_t iki = 2u;
    (void)memcpy(&C.gor.tut_derinlik_var, &iki, 1u);
    t += 5 * DS_MS;
    ds_girdi(&IN, t, 3u, true);
    IN.plat.link_event = (uint8_t)IDA_LINKEV_SESSION_START; /* (e)'de ida_gor_oturum_basi */
    IN.n_cmds = 1u;
    IN.cmds[0].cmd_id = 60u;
    IN.cmds[0].type = (uint8_t)IDA_CMD_FAILSAFE_RESET;
    TEST_ASSERT_EQUAL(IDA_RC_OK, ida_core_step(&C, &IN, &OUT));
    b5_cagri_sonu(&t);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_b1_kip_degisimi_set_mode_sonra_arm);
    RUN_TEST(test_b1_kip_degisimi_arm_sonra_set_mode);
    RUN_TEST(test_b1_kip_degisimi_duzeyi_aninda_yayinlar);
    RUN_TEST(test_b2_cagri_ici_islak_islak_kuru);
    RUN_TEST(test_b3_yalniz_yatay_hiz_kaybi);
    RUN_TEST(test_b3_yalniz_konum_kaybi);
    RUN_TEST(test_b4_bir_sizinti_algilayicisi_kayboldu);
    RUN_TEST(test_b5_tanimsiz_ariza_duzeyi);
    RUN_TEST(test_k7_guided_yatay_istemez);
    RUN_TEST(test_k9_adim_daraltmasi_dagiticida);
    RUN_TEST(test_b5_ornegin_ustune_yazdigi_bozulma);
    RUN_TEST(test_b5_oturum_basinin_ustune_yazdigi_bozulma);
    return UNITY_END();
}
