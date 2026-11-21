/*
 * test_gudum_gecis.c — saklı hedefin ve kontrol referansının daraltma ve emniyet geçişlerinde
 * geçersizleşmesi; gerçek çekirdek üzerinden, itki isteğine kadar (sol W3-zincir bulguları 1
 * ve 2; EMNİYET).
 *
 * Duyargalar 1 m derinlikte durgun ve yatay bir aracı söyler (yüzey eşiğinin altında; sol'un
 * örneği: bulunulan 1 m, hedef ve eski referans 2 m); GUIDED_SP derinlik 2 m'dir. Hedef uygulanırsa dikey iticiler (kanal 4–7, pozitif = aşağı) aşağı iter;
 * uygulanmazsa araç bulunduğu derinliği tutar ve dikey itki isteği küçüktür. Ölçüt itki
 * isteğidir (out.thr_cmd), istek alanları değil.
 *
 * Bulgu 1: reddedilen (DUP_SEQ) bir çağrıdaki DISARM, SESSION_END ya da daraltıcı kip (b)'de
 * uygulanır ama o çağrıda modüllerin adımı çalışmaz. Sonraki çağrıda yeniden ARM ya da
 * SET_MODE(GUIDED) kabul edilince eski hedef yeni GUIDED_SP gelmeden uygulanmamalı.
 * Bulgu 2: HOLD'a girişte, hedef iptalinde ve derinleşme yasağı başladığında kontrolün eski
 * derinlik referansı ve integrali aşağı itkiyi sürdürmemeli.
 */
#include <string.h>

#include "destek.h"
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
static uint32_t g_sq[16];
static uint32_t g_id;
static bool g_hb;     /* 1 Hz HEARTBEAT */
static bool g_leak;   /* 10 Hz kuru sızıntı örneği */


#define P_NS (5 * DS_MS)
#define DIKEY_ESIK 0.10f /* tutmada beklenen dikey itki isteğinin üstü (ölü bant 0,065) */

static ida_sample_t *ornek(uint8_t tur, uint32_t pencere)
{
    ida_sample_t *s = &IN.samples[IN.n_samples++];
    (void)memset(s, 0, sizeof(*s));
    s->hdr.t_meas.ns = g_t;
    s->hdr.t_rx.ns = g_t;
    s->hdr.seq = ++g_sq[tur];
    s->hdr.window_ns = pencere;
    s->hdr.flags = (uint16_t)IDA_HF_VALID;
    s->hdr.type = tur;
    return s;
}

static void girdi_kur(void)
{
    g_seq++;
    g_t += P_NS;
    ds_girdi(&IN, g_t, g_seq, true);
    IN.imu.imu.accel[2] = -9.80665f;
    if ((g_seq % 2u) == 0u) {
        ida_sample_t *s = ornek((uint8_t)IDA_SENS_MAG, 0u);
        s->u.mag.field[0] = 25.6e-6f;
        s->u.mag.field[1] = 2.7e-6f;
        s->u.mag.field[2] = 40.3e-6f;
    }
    if ((g_seq % 8u) == 0u) {
        ida_sample_t *s = ornek((uint8_t)IDA_SENS_BARO, 18750000u);
        s->u.baro.p_abs = 111132.0f; /* sabit; BARO_KAL_D = 1 m ile kalibre edilir */
        s->u.baro.temp_c = 20.0f;
    }
    if ((g_seq % 20u) == 0u) {
        ida_sample_t *s = ornek((uint8_t)IDA_SENS_BATT, 100000000u);
        s->u.batt.voltage = 15.6f;
        s->u.batt.current = 1.0f;
    }
    if (g_leak && ((g_seq % 20u) == 10u)) {
        (void)ornek((uint8_t)IDA_SENS_LEAK, 0u);
    }
    if (g_hb && ((g_seq % 200u) == 100u)) {
        ida_cmd_t *k = &IN.cmds[IN.n_cmds++];
        (void)memset(k, 0, sizeof(*k));
        k->cmd_id = ++g_id;
        k->type = (uint8_t)IDA_CMD_HEARTBEAT;
    }
}

static ida_rc_t cagir(void)
{
    const ida_rc_t rc = ida_core_step(&C, &IN, &OUT);
    ds_cikti_degismezleri(rc, &IN, &OUT);
    return rc;
}

static void adim(void)
{
    girdi_kur();
    TEST_ASSERT_EQUAL(IDA_RC_OK, cagir());
}

static void adimlar(uint32_t n)
{
    for (uint32_t i = 0u; i < n; i++) {
        adim();
    }
}

static ida_cmd_t *komut_ekle(uint8_t tur)
{
    ida_cmd_t *k = &IN.cmds[IN.n_cmds++];
    (void)memset(k, 0, sizeof(*k));
    k->cmd_id = ++g_id;
    k->type = tur;
    return k;
}

static uint8_t komutla(uint8_t tur, uint8_t kip)
{
    girdi_kur();
    ida_cmd_t *k = komut_ekle(tur);
    if (tur == (uint8_t)IDA_CMD_SET_MODE) {
        k->u.set_mode.mode = kip;
    }
    const uint32_t id = k->cmd_id;
    TEST_ASSERT_EQUAL(IDA_RC_OK, cagir());
    return ds_sonuc(&OUT, id)->ack;
}

static void hedef_gonder(float derinlik)
{
    girdi_kur();
    ida_cmd_t *k = komut_ekle((uint8_t)IDA_CMD_GUIDED_SP);
    k->u.guided.mask = (uint8_t)(IDA_SP_DEPTH | IDA_SP_YAW);
    k->u.guided.depth = derinlik;
    k->u.guided.yaw = 0.0f;
    const uint32_t id = k->cmd_id;
    TEST_ASSERT_EQUAL(IDA_RC_OK, cagir());
    TEST_ASSERT_EQUAL_UINT8(IDA_ACK_ACCEPTED, ds_sonuc(&OUT, id)->ack);
}

static float dikey_itki(void)
{
    float m = 0.0f;
    for (uint32_t i = 4u; i < 8u; i++) {
        m = (OUT.thr_cmd[i] > m) ? OUT.thr_cmd[i] : m;
    }
    return m;
}

/* n adım boyunca en büyük aşağı itki isteği. */
static float en_buyuk_dikey(uint32_t n)
{
    float m = 0.0f;
    for (uint32_t i = 0u; i < n; i++) {
        adim();
        const float d = dikey_itki();
        m = (d > m) ? d : m;
    }
    return m;
}

/* Geçişten sonra aşağı itki: ilk adımdaki değerin üstüne çıkmaz (sıçrama yok; değişim hızı
   sınırıyla önceki çıktıdan iner) ve 0,5 s sonra tutma düzeyine (DIKEY_ESIK altı) iner;
   400 adım (2 s) izlenir. */
static void asagi_itki_yok(void)
{
    adim();
    const float ilk = dikey_itki();
    for (uint32_t i = 1u; i < 400u; i++) {
        adim();
        const float d = dikey_itki();
        TEST_ASSERT_TRUE_MESSAGE(d <= (ilk + 0.02f), "geçişten sonra aşağı itki arttı");
        if (i >= 100u) {
            TEST_ASSERT_TRUE_MESSAGE(d < DIKEY_ESIK, "0,5 s sonra aşağı itki sürüyor");
        }
    }
}

/* Hazır, silahlı, GUIDED ve 2 m hedefi uygulanıyor (aşağı itki). */
static void hedefe_gidiyor(void)
{
    TEST_ASSERT_EQUAL(IDA_RC_OK, ds_kur(&C));
    g_seq = 0u;
    g_t = 2000 * DS_MS;
    (void)memset(g_sq, 0, sizeof(g_sq));
    g_id = 0u;
    g_hb = true;
    g_leak = true;
    /* Beyaz kutu: araç 1 m derinlikte durgun; basınç kalibrasyonu bu derinlikte yapılır
       (BARO_KAL_D, init'ten sonra ilk hizalamada okunur). */
    const float bir = 1.0f;
    (void)memcpy(&C.prm.yururluk.ham[IDA_PRM_baro_kal_d], &bir, sizeof(bir));
    adimlar(600u); /* hizalama ve ölçüm ufku */
    TEST_ASSERT_FLOAT_WITHIN(0.05f, 1.0f, OUT.nav.depth);
    TEST_ASSERT_EQUAL_UINT8(IDA_ACK_ACCEPTED, komutla((uint8_t)IDA_CMD_ARM, 0u));
    TEST_ASSERT_EQUAL_UINT8(IDA_ACK_ACCEPTED, komutla((uint8_t)IDA_CMD_SET_MODE, (uint8_t)IDA_MODE_GUIDED));
    adimlar(400u);
    hedef_gonder(2.0f);
    /* Hedef uygulanıyor: aşağı. Duyargalar 1 m'de kalır; derinlik referansı 4 s'de 2 m'ye
       varır (sol'un örneği: bulunulan 1 m, eski referans 2 m). */
    TEST_ASSERT_TRUE(en_buyuk_dikey(800u) > 0.3f);
}

/* Yinelenen zaman çerçevesiyle (DUP_SEQ) reddedilen çağrıda daraltıcı girdi. */
static void reddedilen_cagri(uint8_t mandal, uint8_t kip, bool oturum_sonu)
{
    ds_girdi(&IN, g_t, g_seq, true); /* aynı tf.seq: DUP_SEQ */
    IN.narrow.mask = mandal;
    IN.narrow.mode = kip;
    if (mandal != 0u) {
        IN.narrow.cmd_id[((mandal & IDA_NARROW_DISARM) != 0u) ? 0u : 1u] = ++g_id;
    }
    if (oturum_sonu) {
        IN.plat.link_event = (uint8_t)IDA_LINKEV_SESSION_END;
    }
    TEST_ASSERT_EQUAL(IDA_RC_DUP_SEQ, cagir());
}

static void test_b1_reddedilen_disarm_sonra_arm(void)
{
    hedefe_gidiyor();
    reddedilen_cagri((uint8_t)IDA_NARROW_DISARM, 0u, false);
    TEST_ASSERT_FALSE(OUT.armed);
    TEST_ASSERT_EQUAL_UINT8(IDA_ACK_ACCEPTED, komutla((uint8_t)IDA_CMD_ARM, 0u));
    TEST_ASSERT_TRUE(OUT.armed);
    TEST_ASSERT_EQUAL_UINT8(IDA_MODE_GUIDED, OUT.status.mode);
    /* Yeni GUIDED_SP yok: eski 2 m hedefi uygulanmaz, araç bulunduğu derinliği tutar. */
    asagi_itki_yok();
    /* Yeni hedef gelince yine uygulanır. */
    hedef_gonder(2.0f);
    TEST_ASSERT_TRUE(en_buyuk_dikey(200u) > 0.3f);
}

/* sol 2. tur bulgu 1: tam sayaç turu. Beyaz kutu: daraltma kuşağı, hedefin saklandığı kuşağın
   bir eksiğine kurulur (2^32 − 1 daraltmanın, örneğin out NULL çağrılarda, güdüm çalışmadan
   geçtiği durum); reddedilen çağrıdaki bir DISARM daha kuşağı hedefin kuşağına eşitler. Eski
   hedef ARM'dan sonra yine uygulanmamalı: geçersizlik sayaç eşitliğine değil kalıcı mandala
   dayanır. */
static void test_b1_kusak_turu_eski_hedefi_canlandirmaz(void)
{
    hedefe_gidiyor();
    C.emn.kusak = C.gud.kusak - 1u;
    C.emn.kusak_tumleyen = 0xFFFFFFFFu ^ C.emn.kusak;
    reddedilen_cagri((uint8_t)IDA_NARROW_DISARM, 0u, false);
    TEST_ASSERT_EQUAL_UINT32(C.gud.kusak, C.emn.kusak); /* sayaç tam tur attı */
    TEST_ASSERT_FALSE(OUT.armed);
    TEST_ASSERT_EQUAL_UINT8(IDA_ACK_ACCEPTED, komutla((uint8_t)IDA_CMD_ARM, 0u));
    asagi_itki_yok();
}

/* Aynı çağrıda daraltmadan (DISARM mandalı, (b)) sonra işlenen GUIDED_SP geçerli yeni
   hedeftir: mandal temizlenir, yeni hedef saklanır ve ARM'dan sonra uygulanır. */
static void test_b1_daraltmadan_sonra_gelen_hedef_gecerli(void)
{
    hedefe_gidiyor();
    girdi_kur();
    IN.narrow.mask = (uint8_t)IDA_NARROW_DISARM;
    IN.narrow.cmd_id[0] = ++g_id;
    ida_cmd_t *k = komut_ekle((uint8_t)IDA_CMD_GUIDED_SP);
    k->u.guided.mask = (uint8_t)(IDA_SP_DEPTH | IDA_SP_YAW);
    k->u.guided.depth = 3.0f;
    k->u.guided.yaw = 0.0f;
    const uint32_t id = k->cmd_id;
    TEST_ASSERT_EQUAL(IDA_RC_OK, cagir());
    TEST_ASSERT_FALSE(OUT.armed);
    TEST_ASSERT_EQUAL_UINT8(IDA_ACK_ACCEPTED, ds_sonuc(&OUT, id)->ack);
    TEST_ASSERT_EQUAL_UINT8(0u, (uint8_t)(C.emn.hedef_iptal & IDA_EMN_IPTAL_GUD));
    TEST_ASSERT_EQUAL_FLOAT(3.0f, C.gud.derinlik);
    TEST_ASSERT_EQUAL_UINT8(IDA_ACK_ACCEPTED, komutla((uint8_t)IDA_CMD_ARM, 0u));
    TEST_ASSERT_TRUE(en_buyuk_dikey(400u) > 0.3f); /* yeni 3 m hedefi: aşağı */
}

static void test_b1_reddedilen_oturum_sonu_sonra_arm(void)
{
    hedefe_gidiyor();
    reddedilen_cagri(0u, 0u, true);
    TEST_ASSERT_FALSE(OUT.armed);
    TEST_ASSERT_EQUAL_UINT8(IDA_ACK_ACCEPTED, komutla((uint8_t)IDA_CMD_ARM, 0u));
    TEST_ASSERT_TRUE(OUT.armed);
    asagi_itki_yok();
}

static void test_b1_reddedilen_kip_daralmasi_sonra_guided(void)
{
    hedefe_gidiyor();
    reddedilen_cagri((uint8_t)IDA_NARROW_MODE, (uint8_t)IDA_MODE_DEPTH_HOLD, false);
    TEST_ASSERT_EQUAL_UINT8(IDA_MODE_DEPTH_HOLD, OUT.status.mode);
    TEST_ASSERT_TRUE(OUT.armed);
    /* SET_MODE(GUIDED) güdümün adımından önce işlenir: eski hedef yine geçersiz. */
    TEST_ASSERT_EQUAL_UINT8(IDA_ACK_ACCEPTED, komutla((uint8_t)IDA_CMD_SET_MODE, (uint8_t)IDA_MODE_GUIDED));
    asagi_itki_yok();
}

/* Bulgu 2: HOLD'a giriş (yer istasyonu kaybı; GUIDED sütununda GCS_KAYIP_MS sonra HOLD).
   Aşağı itki HOLD'un ilk adımından sonra sürmez. */
static void test_b2_hold_girisinde_asagi_itki_yok(void)
{
    hedefe_gidiyor();
    g_hb = false;
    uint32_t n = 0u;
    while ((OUT.fs_level < (uint8_t)IDA_FS_HOLD) && (n < 2000u)) {
        adim();
        n++;
    }
    TEST_ASSERT_EQUAL_UINT8(IDA_FS_HOLD, OUT.fs_level);
    asagi_itki_yok();
}

/* Bulgu 2: hedef iptali (kabul edilen çağrıda daraltıcı kip DEPTH_HOLD). */
static void test_b2_hedef_iptalinde_asagi_itki_yok(void)
{
    hedefe_gidiyor();
    girdi_kur();
    IN.narrow.mask = (uint8_t)IDA_NARROW_MODE;
    IN.narrow.mode = (uint8_t)IDA_MODE_DEPTH_HOLD;
    IN.narrow.cmd_id[1] = ++g_id;
    TEST_ASSERT_EQUAL(IDA_RC_OK, cagir());
    TEST_ASSERT_EQUAL_UINT8(IDA_MODE_DEPTH_HOLD, OUT.status.mode);
    asagi_itki_yok();
}

/* Bulgu 2: derinleşme yasağı (sızıntı algılayıcısı bayat): hedef bulunulan derinliğe kırpılır
   ve kontrolün eski referansı aşağı itkiyi sürdürmez. */
static void test_b2_derinlesme_yasaginda_asagi_itki_yok(void)
{
    hedefe_gidiyor();
    g_leak = false;
    uint32_t n = 0u;
    while (((OUT.status.fs_reasons & IDA_FSR_LEAK) == 0u) && (n < 2000u)) {
        adim();
        n++;
    }
    TEST_ASSERT_TRUE((OUT.status.fs_reasons & IDA_FSR_LEAK) != 0u);
    asagi_itki_yok();
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_b1_reddedilen_disarm_sonra_arm);
    RUN_TEST(test_b1_kusak_turu_eski_hedefi_canlandirmaz);
    RUN_TEST(test_b1_daraltmadan_sonra_gelen_hedef_gecerli);
    RUN_TEST(test_b1_reddedilen_oturum_sonu_sonra_arm);
    RUN_TEST(test_b1_reddedilen_kip_daralmasi_sonra_guided);
    RUN_TEST(test_b2_hold_girisinde_asagi_itki_yok);
    RUN_TEST(test_b2_hedef_iptalinde_asagi_itki_yok);
    RUN_TEST(test_b2_derinlesme_yasaginda_asagi_itki_yok);
    return UNITY_END();
}
