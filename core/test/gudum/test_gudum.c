/*
 * test_gudum.c — güdüm: GUIDED_SP (A4-K50, tablo 3.17, 3.4 aralıkları; W3-K7) ve kontrol
 * isteğine çevirisi (W3-zincir md. 2).
 */
#include <string.h>

#include "destek.h"
#include "gudum/gudum.h"
#include "unity.h"

void setUp(void);
void tearDown(void);
void setUp(void) {}
void tearDown(void) {}

static ida_core_t C;
static ida_in_t IN;
static ida_out_t OUT;
static uint32_t g_seq;
static int64_t g_t;

static float f32_bit(uint32_t u)
{
    float f = 0.0f;
    (void)memcpy(&f, &u, sizeof(f));
    return f;
}

static void hazirla(void)
{
    TEST_ASSERT_EQUAL(IDA_RC_OK, ds_kur(&C));
    g_seq = 0u;
    g_t = 2000 * DS_MS;
    ds_girdi(&IN, g_t, 0u, true);
}

static ida_rc_t step(void)
{
    g_seq++;
    g_t += 5 * DS_MS;
    IN.now_w.ns = g_t;
    IN.tf.seq = g_seq;
    IN.tf.t.ns = g_t;
    IN.tf.t_rx.ns = g_t;
    ds_imu(&IN.imu, g_t, g_seq);
    const ida_rc_t rc = ida_core_step(&C, &IN, &OUT);
    ds_cikti_degismezleri(rc, &IN, &OUT);
    return rc;
}

static ida_guided_sp_t *sp_ekle(uint32_t i, uint32_t id, uint8_t mask)
{
    ida_cmd_t *k = &IN.cmds[i];
    (void)memset(k, 0, sizeof(*k));
    k->cmd_id = id;
    k->type = (uint8_t)IDA_CMD_GUIDED_SP;
    k->u.guided.mask = mask;
    if (IN.n_cmds <= i) {
        IN.n_cmds = (uint8_t)(i + 1u);
    }
    return &k->u.guided;
}

static void sonuc_bekle(uint32_t id, uint8_t ack, uint16_t neden)
{
    const ida_cmd_result_t *r = ds_sonuc(&OUT, id);
    TEST_ASSERT_NOT_NULL(r);
    TEST_ASSERT_EQUAL_UINT8(ack, r->ack);
    TEST_ASSERT_EQUAL_UINT16(neden, r->reason);
}

/* 3.17 GUIDED_SP satırı: OUT_OF_RANGE BAD_MODE'dan önce; maskesiz alan denetlenmez. Kip INIT
   (GUIDED değil): geçerli içerik BAD_MODE alır. */
static void test_317_araliklar_ve_sira(void)
{
    hazirla();
    const float pi_f = f32_bit(0x40490FDBu);    /* float π > π: aralık dışı */
    const float pi_alti = f32_bit(0x40490FDAu); /* π'nin altındaki en büyük float */
    const float eksi_pi_f = f32_bit(0xC0490FDBu);
    sp_ekle(0u, 1u, 0u);                                     /* mask 0 */
    sp_ekle(1u, 2u, 0x80u);                                  /* tanımsız bit */
    sp_ekle(2u, 3u, (uint8_t)IDA_SP_DEPTH)->depth = -1.0f;
    sp_ekle(3u, 4u, (uint8_t)IDA_SP_ALTITUDE)->altitude = -1.0f;
    sp_ekle(4u, 5u, (uint8_t)IDA_SP_YAW)->yaw = 4.0f;
    sp_ekle(5u, 6u, (uint8_t)IDA_SP_YAW)->yaw = pi_f;
    sp_ekle(6u, 7u, (uint8_t)IDA_SP_YAW)->yaw = eksi_pi_f;
    sp_ekle(7u, 8u, (uint8_t)IDA_SP_POS_NE)->pos_ne[1] = ds_nan();
    TEST_ASSERT_EQUAL(IDA_RC_OK, step());
    for (uint32_t id = 1u; id <= 8u; id++) {
        sonuc_bekle(id, (uint8_t)IDA_ACK_REJECTED, (uint16_t)IDA_REASON_OUT_OF_RANGE);
    }
    IN.n_cmds = 0u;
    sp_ekle(0u, 11u, (uint8_t)IDA_SP_YAW)->yaw = pi_alti;
    sp_ekle(1u, 12u, (uint8_t)IDA_SP_YAW)->yaw = -pi_alti;
    ida_guided_sp_t *s = sp_ekle(2u, 13u, (uint8_t)IDA_SP_DEPTH);
    s->depth = 1.0f;
    s->altitude = -1.0f; /* maskesiz: denetlenmez */
    s->yaw = 99.0f;
    s->vel_ned[2] = ds_sonsuz();
    sp_ekle(3u, 14u, (uint8_t)IDA_SP_ALTITUDE)->altitude = 0.0f;
    sp_ekle(4u, 15u, (uint8_t)IDA_SP_VEL_D)->vel_ned[2] = ds_sonsuz();
    sp_ekle(5u, 16u, (uint8_t)IDA_SP_YAW_RATE)->yaw_rate = ds_nan();
    sp_ekle(6u, 17u, (uint8_t)IDA_SP_VEL_NE)->vel_ned[0] = ds_nan();
    TEST_ASSERT_EQUAL(IDA_RC_OK, step());
    for (uint32_t id = 11u; id <= 14u; id++) {
        sonuc_bekle(id, (uint8_t)IDA_ACK_REJECTED, (uint16_t)IDA_REASON_BAD_MODE);
    }
    for (uint32_t id = 15u; id <= 17u; id++) {
        sonuc_bekle(id, (uint8_t)IDA_ACK_REJECTED, (uint16_t)IDA_REASON_OUT_OF_RANGE);
    }
    /* Reddedilen hedef saklanmaz. */
    TEST_ASSERT_EQUAL_UINT8(IDA_GUD_DIKEY_YOK, C.gud.dikey);
    TEST_ASSERT_EQUAL_UINT8(IDA_GUD_YON_YOK, C.gud.yon_turu);
}

/* GUIDED kipinde kabul; maskesiz kanal korunur; kanal içi öncelik. */
static void test_guided_kipinde_kabul_ve_saklama(void)
{
    hazirla();
    C.emn.mode = (uint8_t)IDA_MODE_GUIDED; /* beyaz kutu: kip doğrudan kurulur */
    C.gor.son_kip = (uint8_t)IDA_MODE_GUIDED;
    ida_guided_sp_t *s = sp_ekle(0u, 21u, (uint8_t)(IDA_SP_DEPTH | IDA_SP_YAW));
    s->depth = 1.5f;
    s->yaw = 1.0f;
    TEST_ASSERT_EQUAL(IDA_RC_OK, step());
    sonuc_bekle(21u, (uint8_t)IDA_ACK_ACCEPTED, (uint16_t)IDA_REASON_NONE);
    /* Kip GUIDED kaldıysa hedef saklıdır (silahsızken de: kip ve armed bağımsız, A4-K127). */
    TEST_ASSERT_EQUAL_UINT8(IDA_MODE_GUIDED, C.emn.mode);
    TEST_ASSERT_EQUAL_UINT8(IDA_GUD_DIKEY_DERINLIK, C.gud.dikey);
    TEST_ASSERT_EQUAL_FLOAT(1.5f, C.gud.derinlik);
    TEST_ASSERT_EQUAL_UINT8(IDA_GUD_YON_ACI, C.gud.yon_turu);
    TEST_ASSERT_EQUAL_FLOAT(1.0f, C.gud.yon);
    /* Yalnız VEL_D ve YAW_RATE: dikey ve yön kanalı değişir; DEPTH + VEL_D birlikte: DEPTH. */
    IN.n_cmds = 0u;
    s = sp_ekle(0u, 22u, (uint8_t)(IDA_SP_VEL_D | IDA_SP_YAW_RATE | IDA_SP_VEL_NE));
    s->vel_ned[0] = 0.2f;
    s->vel_ned[2] = 0.1f;
    s->yaw_rate = -0.2f;
    TEST_ASSERT_EQUAL(IDA_RC_OK, step());
    sonuc_bekle(22u, (uint8_t)IDA_ACK_ACCEPTED, (uint16_t)IDA_REASON_NONE);
    TEST_ASSERT_EQUAL_UINT8(IDA_GUD_DIKEY_HIZ, C.gud.dikey);
    TEST_ASSERT_EQUAL_UINT8(IDA_GUD_YON_HIZ, C.gud.yon_turu);
    TEST_ASSERT_EQUAL_UINT8(IDA_GUD_YATAY_HIZ, C.gud.yatay);
    IN.n_cmds = 0u;
    s = sp_ekle(0u, 23u, (uint8_t)(IDA_SP_DEPTH | IDA_SP_VEL_D | IDA_SP_POS_NE | IDA_SP_VEL_NE));
    s->depth = 2.0f;
    s->pos_ne[0] = 3.0f;
    TEST_ASSERT_EQUAL(IDA_RC_OK, step());
    TEST_ASSERT_EQUAL_UINT8(IDA_GUD_DIKEY_DERINLIK, C.gud.dikey);
    TEST_ASSERT_EQUAL_FLOAT(2.0f, C.gud.derinlik);
    TEST_ASSERT_EQUAL_UINT8(IDA_GUD_YATAY_KONUM, C.gud.yatay);
    TEST_ASSERT_EQUAL_UINT8(IDA_GUD_YON_HIZ, C.gud.yon_turu); /* maskesiz: korunur */
}

/* Kip, komutun işlendiği andaki kiptir (aynı çağrıda önce gelen SET_MODE(GUIDED); W3-K13
   ilkesi): bağlamın komutlardan önceki kipi değil, çekirdeğin o anki kipi okunur. */
static void test_kip_islendigi_andaki_kip(void)
{
    hazirla();
    ida_komut_baglam_t b;
    (void)memset(&b, 0, sizeof(b));
    b.mode = (uint8_t)IDA_MODE_DISARMED;
    ida_cmd_t k;
    (void)memset(&k, 0, sizeof(k));
    k.type = (uint8_t)IDA_CMD_GUIDED_SP;
    k.u.guided.mask = (uint8_t)IDA_SP_DEPTH;
    k.u.guided.depth = 1.0f;
    ida_cmd_result_t r;
    (void)memset(&r, 0, sizeof(r));
    C.emn.mode = (uint8_t)IDA_MODE_GUIDED;
    ida_gud_komut(&C, &b, &k, &r);
    TEST_ASSERT_EQUAL_UINT8(IDA_ACK_ACCEPTED, r.ack);
    b.mode = (uint8_t)IDA_MODE_GUIDED;
    C.emn.mode = (uint8_t)IDA_MODE_DEPTH_HOLD;
    ida_gud_komut(&C, &b, &k, &r);
    TEST_ASSERT_EQUAL_UINT8(IDA_ACK_REJECTED, r.ack);
    TEST_ASSERT_EQUAL_UINT16(IDA_REASON_BAD_MODE, r.reason);
}

/* ---- Kontrol isteği ---------------------------------------------------------------------- */

static void istek_ortami(uint8_t gecerlilik)
{
    hazirla();
    C.emn.armed = true;
    C.emn.mode = (uint8_t)IDA_MODE_GUIDED;
    C.gor.fs_level = (uint8_t)IDA_FS_NONE;
    C.gor.gecerlilik = gecerlilik;
    C.gor.nav_tutum_var = (gecerlilik & IDA_KES_G_TUTUM) != 0u;
    C.gor.nav_yon_var = (gecerlilik & IDA_KES_G_YON) != 0u;
    C.gor.nav_derinlik_var = (gecerlilik & IDA_KES_G_DERINLIK) != 0u;
    C.gor.nav_konum_var = (gecerlilik & (IDA_KES_G_KONUM | IDA_KES_G_YATAY_HIZ)) ==
                          (IDA_KES_G_KONUM | IDA_KES_G_YATAY_HIZ);
    C.gor.nav_derinlik = 0.5f;
    C.gor.nav_yon = 0.0f;
    C.gor.nav_ne[0] = 0.0f;
    C.gor.nav_ne[1] = 0.0f;
    C.gor.tut_derinlik_var = true;
    C.gor.tut_derinlik = 0.5f;
    C.gor.tut_yon_var = true;
    C.gor.tut_yon = 0.0f;
    C.gor.tut_konum_var = C.gor.nav_konum_var;
}

static void hedef_kur(uint8_t mask, float derinlik, float yon)
{
    ida_komut_baglam_t b;
    (void)memset(&b, 0, sizeof(b));
    ida_cmd_t k;
    (void)memset(&k, 0, sizeof(k));
    k.type = (uint8_t)IDA_CMD_GUIDED_SP;
    k.u.guided.mask = mask;
    k.u.guided.depth = derinlik;
    k.u.guided.altitude = 1.0f;
    k.u.guided.yaw = yon;
    k.u.guided.pos_ne[0] = 10.0f; /* 10 m kuzey */
    k.u.guided.vel_ned[0] = 0.0f;
    k.u.guided.vel_ned[1] = 0.3f;
    k.u.guided.vel_ned[2] = 0.2f;
    k.u.guided.yaw_rate = 0.1f;
    ida_cmd_result_t r;
    ida_gud_komut(&C, &b, &k, &r);
    TEST_ASSERT_EQUAL_UINT8(IDA_ACK_ACCEPTED, r.ack);
}

#define G_DIKEY ((uint8_t)(IDA_KES_G_TUTUM | IDA_KES_G_YON | IDA_KES_G_DERINLIK | IDA_KES_G_DIKEY_HIZ))
#define G_TUM ((uint8_t)(G_DIKEY | IDA_KES_G_YATAY_HIZ | IDA_KES_G_KONUM))

static void test_istek_derinlik_ve_yon(void)
{
    istek_ortami(G_DIKEY);
    hedef_kur((uint8_t)(IDA_SP_DEPTH | IDA_SP_YAW), 1.5f, 1.5707963f);
    ida_kon_istek_t i;
    ida_gud_kon_istegi(&C.gud, &C, &i);
    TEST_ASSERT_EQUAL_UINT8(IDA_KON_KIP_DALIS, i.kip);
    TEST_ASSERT_EQUAL_UINT8(IDA_KON_DIKEY_DERINLIK, i.dikey);
    TEST_ASSERT_EQUAL_FLOAT(1.5f, i.derinlik);
    TEST_ASSERT_EQUAL_UINT8(IDA_KON_YON_ACI, i.yon_kanali);
    TEST_ASSERT_EQUAL_FLOAT(1.5707963f, i.yon);
    TEST_ASSERT_EQUAL_UINT8(IDA_KON_TUTUM_ACI, i.tutum);
    TEST_ASSERT_EQUAL_UINT8(IDA_KON_YATAY_KUVVET, i.yatay); /* yatay geçerlilik yok */
    /* Sızıntı algılayıcısı bayat: derinleşme yok. */
    C.gor.dalis_yasak = true;
    ida_gud_kon_istegi(&C.gud, &C, &i);
    TEST_ASSERT_EQUAL_FLOAT(0.5f, i.derinlik);
    C.gor.dalis_yasak = false;
    /* Yön geçersiz: açısal hız 0 (W3-K7); geçerli olunca hedefe döner. */
    C.gor.nav_yon_var = false;
    C.gor.gecerlilik = (uint8_t)(G_DIKEY ^ IDA_KES_G_YON);
    ida_gud_kon_istegi(&C.gud, &C, &i);
    TEST_ASSERT_EQUAL_UINT8(IDA_KON_YON_HIZ, i.yon_kanali);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, i.yon_hizi);
    C.gor.nav_yon_var = true;
    C.gor.gecerlilik = G_DIKEY;
    ida_gud_kon_istegi(&C.gud, &C, &i);
    TEST_ASSERT_EQUAL_UINT8(IDA_KON_YON_ACI, i.yon_kanali);
    /* Derinlik geçersiz, dikey hız da geçersiz: dikey kuvvet 0. */
    C.gor.nav_derinlik_var = false;
    C.gor.gecerlilik = (uint8_t)(IDA_KES_G_TUTUM | IDA_KES_G_YON);
    ida_gud_kon_istegi(&C.gud, &C, &i);
    TEST_ASSERT_EQUAL_UINT8(IDA_KON_DIKEY_KUVVET, i.dikey);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, i.kuvvet[IDA_EKS_Z]);
    /* Derinlik geçersiz ama dikey hız geçerli: dikey hız 0. */
    C.gor.gecerlilik = (uint8_t)(IDA_KES_G_TUTUM | IDA_KES_G_YON | IDA_KES_G_DIKEY_HIZ);
    ida_gud_kon_istegi(&C.gud, &C, &i);
    TEST_ASSERT_EQUAL_UINT8(IDA_KON_DIKEY_HIZ, i.dikey);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, i.dikey_hiz);
}

static void test_istek_yatay_ve_hiz_kanallari(void)
{
    /* Yatay geçerlilik yokken konum hedefi: yatay kuvvet 0. */
    istek_ortami(G_DIKEY);
    hedef_kur((uint8_t)(IDA_SP_POS_NE | IDA_SP_VEL_D | IDA_SP_YAW_RATE), 0.0f, 0.0f);
    ida_kon_istek_t i;
    ida_gud_kon_istegi(&C.gud, &C, &i);
    TEST_ASSERT_EQUAL_UINT8(IDA_KON_YATAY_KUVVET, i.yatay);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, i.kuvvet[IDA_EKS_X]);
    TEST_ASSERT_EQUAL_UINT8(IDA_KON_DIKEY_HIZ, i.dikey);
    TEST_ASSERT_EQUAL_FLOAT(0.2f, i.dikey_hiz);
    TEST_ASSERT_EQUAL_UINT8(IDA_KON_YON_HIZ, i.yon_kanali);
    TEST_ASSERT_EQUAL_FLOAT(0.1f, i.yon_hizi);
    /* Sızıntı algılayıcısı bayatken aşağı hız istenmez. */
    C.gor.dalis_yasak = true;
    ida_gud_kon_istegi(&C.gud, &C, &i);
    TEST_ASSERT_EQUAL_UINT8(IDA_KON_DIKEY_HIZ, i.dikey);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, i.dikey_hiz);
    C.gor.dalis_yasak = false;
    /* Yatay geçerli: konum hatası (10 m kuzey) × GUD_POS_KP, yön çerçevesinde; yön π/2'de
       kuzey yana (−) düşer. */
    istek_ortami(G_TUM);
    hedef_kur((uint8_t)IDA_SP_POS_NE, 0.0f, 0.0f);
    C.gor.nav_yon = 1.5707963f;
    ida_gud_kon_istegi(&C.gud, &C, &i);
    TEST_ASSERT_EQUAL_UINT8(IDA_KON_YATAY_HIZ, i.yatay);
    const float kp = ida_prm_f32(&C.prm, IDA_PRM_gud_konum_kp);
    TEST_ASSERT_FLOAT_WITHIN(1.0e-4f, 0.0f, i.hiz[0]);
    TEST_ASSERT_FLOAT_WITHIN(1.0e-4f, -10.0f * kp, i.hiz[1]);
    /* Hız hedefi: doğuya 0,3 m/s, yön 0 → yan +0,3. */
    hedef_kur((uint8_t)IDA_SP_VEL_NE, 0.0f, 0.0f);
    C.gor.nav_yon = 0.0f;
    ida_gud_kon_istegi(&C.gud, &C, &i);
    TEST_ASSERT_FLOAT_WITHIN(1.0e-6f, 0.0f, i.hiz[0]);
    TEST_ASSERT_FLOAT_WITHIN(1.0e-6f, 0.3f, i.hiz[1]);
    /* İrtifa kestirimi yok: sönümlü, sıfır hedefli. */
    hedef_kur((uint8_t)IDA_SP_ALTITUDE, 0.0f, 0.0f);
    ida_gud_kon_istegi(&C.gud, &C, &i);
    TEST_ASSERT_EQUAL_UINT8(IDA_KON_DIKEY_HIZ, i.dikey);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, i.dikey_hiz);
}

/* GUIDED_SP yalnız GUIDED'da, silahlıyken ve arıza eylemi HOLD'dan hafifken uygulanır. */
static void test_istek_uygulanma_kosullari(void)
{
    istek_ortami(G_DIKEY);
    hedef_kur((uint8_t)IDA_SP_DEPTH, 1.5f, 0.0f);
    ida_kon_istek_t i;
    C.gor.fs_level = (uint8_t)IDA_FS_HOLD; /* arıza > operatör: görev modülünün tutması */
    ida_gud_kon_istegi(&C.gud, &C, &i);
    TEST_ASSERT_EQUAL_UINT8(IDA_KON_DIKEY_DERINLIK, i.dikey);
    TEST_ASSERT_EQUAL_FLOAT(0.5f, i.derinlik);
    C.gor.fs_level = (uint8_t)IDA_FS_WARN; /* uyarı eylemsiz: hedef uygulanır */
    ida_gud_kon_istegi(&C.gud, &C, &i);
    TEST_ASSERT_EQUAL_FLOAT(1.5f, i.derinlik);
    C.emn.armed = false;
    ida_gud_kon_istegi(&C.gud, &C, &i);
    TEST_ASSERT_EQUAL_UINT8(IDA_KON_KIP_KAPALI, i.kip);
    C.emn.armed = true;
    C.emn.mode = (uint8_t)IDA_MODE_DEPTH_HOLD;
    ida_gud_kon_istegi(&C.gud, &C, &i);
    TEST_ASSERT_EQUAL_FLOAT(0.5f, i.derinlik); /* görev modülünün tutma hedefi */
}

/* Hedefin silinmesi: kip GUIDED değil, arıza eylemi HOLD ve üstü, daraltma kuşağı değişti
   (silahsızlanma; beyaz kutuda emniyet modülünün yaptığı gibi kuşak artırılır), oturum başı.
   Silinen hedefe kendiliğinden dönülmez (EMNİYET). */
static void test_hedef_silinmesi(void)
{
    istek_ortami(G_DIKEY);
    hedef_kur((uint8_t)(IDA_SP_DEPTH | IDA_SP_YAW), 1.5f, 1.0f);
    ida_gud_adim(&C.gud, &C);
    TEST_ASSERT_EQUAL_UINT8(IDA_GUD_DIKEY_DERINLIK, C.gud.dikey); /* koşullar tutuyor */
    C.emn.armed = false;
    ida_emn_kusak_artir(&C.emn);
    ida_gud_adim(&C.gud, &C);
    TEST_ASSERT_EQUAL_UINT8(IDA_GUD_DIKEY_YOK, C.gud.dikey);
    TEST_ASSERT_EQUAL_UINT8(IDA_GUD_YON_YOK, C.gud.yon_turu);
    /* Silahsızken alınan hedef silahlanmada korunur (silahlanma kuşağı değiştirmez). */
    hedef_kur((uint8_t)IDA_SP_DEPTH, 1.0f, 0.0f);
    ida_gud_adim(&C.gud, &C);
    C.emn.armed = true;
    ida_gud_adim(&C.gud, &C);
    TEST_ASSERT_EQUAL_UINT8(IDA_GUD_DIKEY_DERINLIK, C.gud.dikey);
    C.gor.fs_level = (uint8_t)IDA_FS_HOLD;
    ida_gud_adim(&C.gud, &C);
    TEST_ASSERT_EQUAL_UINT8(IDA_GUD_DIKEY_YOK, C.gud.dikey);
    C.gor.fs_level = (uint8_t)IDA_FS_NONE;
    hedef_kur((uint8_t)IDA_SP_DEPTH, 1.0f, 0.0f);
    C.emn.mode = (uint8_t)IDA_MODE_DEPTH_HOLD;
    ida_gud_adim(&C.gud, &C);
    TEST_ASSERT_EQUAL_UINT8(IDA_GUD_DIKEY_YOK, C.gud.dikey);
    C.emn.mode = (uint8_t)IDA_MODE_GUIDED;
    hedef_kur((uint8_t)IDA_SP_DEPTH, 1.0f, 0.0f);
    ida_gud_oturum_basi(&C.gud);
    TEST_ASSERT_EQUAL_UINT8(IDA_GUD_DIKEY_YOK, C.gud.dikey);
}

/* A4-K89: tanımsız durum değeri iç tutarlılık denetimine düşer. */
static void test_tutarlilik(void)
{
    hazirla();
    TEST_ASSERT_TRUE(ida_gud_tutarli(&C.gud));
    C.gud.dikey = 9u;
    TEST_ASSERT_FALSE(ida_gud_tutarli(&C.gud));
    C.gud.dikey = 0u;
    C.gud.yatay = 3u;
    TEST_ASSERT_FALSE(ida_gud_tutarli(&C.gud));
    C.gud.yatay = 0u;
    TEST_ASSERT_TRUE(ida_gud_tutarli(&C.gud));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_317_araliklar_ve_sira);
    RUN_TEST(test_guided_kipinde_kabul_ve_saklama);
    RUN_TEST(test_kip_islendigi_andaki_kip);
    RUN_TEST(test_istek_derinlik_ve_yon);
    RUN_TEST(test_istek_yatay_ve_hiz_kanallari);
    RUN_TEST(test_istek_uygulanma_kosullari);
    RUN_TEST(test_hedef_silinmesi);
    RUN_TEST(test_tutarlilik);
    return UNITY_END();
}
