/*
 * A4 §3.11 — değerlendirme sırası ve birlikte oluşan koşulların öncelik tablosu
 * (init, step, tick, background); A4-K02, K08, K10, K14, K15, K17, K25, K31, K46, K72,
 * K103, K109, K113, K117, K122.
 *
 * Her çağrı düzenekte dört örnekte yürür; genel değişmezler (kabul_cekirdek.c) her
 * yazılabilir çıktıda ayrıca denetlenir. Durumun değişip değişmediği sonraki çağrıyla
 * gözlenir (sıra izi, örnek kabulü, komut yineleme penceresi, sayaç farkları).
 */
#include <string.h>

#include "kabul_cekirdek.h"
#include "unity.h"

static ida_in_t g_in;
static ida_in_t g_in2;
static ida_out_t g_o;
static ida_out_t g_desen;
static uint8_t g_kopya[1u << 20];

void setUp(void)
{
    kc_ortam_kur(KC_N_AZAMI);
}

void tearDown(void)
{
}

static float nan_f(void)
{
    uint32_t u = 0x7FC00000u;
    float f;
    memcpy(&f, &u, sizeof f);
    return f;
}

static float sonsuz_f(void)
{
    uint32_t u = 0x7F800000u;
    float f;
    memcpy(&f, &u, sizeof f);
    return f;
}

/* ===================================================================================
 * Init'in değerlendirme sırası: (1) c NULL, (2) in NULL, (3) sürüm, (4) now_w ya da
 * param_blob NULL iken param_len > 0, (5) blok geçersiz/yok, (6) OK.
 * =================================================================================== */

static void test_A4_3_11_init_1_c_null(void)
{
    ida_init_in_t ii;
    kc_init_girdi(&ii);
    TEST_ASSERT_EQUAL_INT(IDA_RC_BAD_INPUT, ida_core_init(NULL, &ii));
    TEST_ASSERT_EQUAL_INT(IDA_RC_BAD_INPUT, ida_core_init(NULL, NULL));
    ii.port_major = (uint16_t)(IDA_PORT_VERSION_MAJOR + 1u);
    ii.now_w.ns = -1;
    TEST_ASSERT_EQUAL_INT(IDA_RC_BAD_INPUT, ida_core_init(NULL, &ii));
}

static void test_A4_3_11_init_2_in_null_kurulmamis_isaretler(void)
{
    kc_hazirla(2);
    TEST_ASSERT_EQUAL_INT(IDA_RC_BAD_INPUT, kc_init(NULL));
    /* Örnek kurulmamış: sonraki step ve tick NOT_INIT, A4-K126 içeriği (düzenek). */
    kc_in_step(&g_in, 3u, KC_T0 + 3 * KC_P);
    TEST_ASSERT_EQUAL_INT(IDA_RC_NOT_INIT, kc_step(&g_in));
    kc_in_izgara(&g_in, KC_T0 + 4 * KC_P);
    TEST_ASSERT_EQUAL_INT(IDA_RC_NOT_INIT, kc_tick(&g_in));
}

static void test_A4_K02_init_3_surum_uyusmazligi(void)
{
    ida_init_in_t ii;
    for (int k = 0; k < 3; ++k) {
        kc_hazirla(1);
        kc_init_girdi(&ii);
        if (k == 0) {
            ii.port_major = (uint16_t)(IDA_PORT_VERSION_MAJOR + 1u);
        } else if (k == 1) {
            ii.port_minor = (uint16_t)(IDA_PORT_VERSION_MINOR + 1u);
        } else {
            ii.port_major = 0u;
        }
        TEST_ASSERT_EQUAL_INT(IDA_RC_VERSION, kc_init(&ii));
        kc_in_step(&g_in, 2u, KC_T0 + 2 * KC_P);
        TEST_ASSERT_EQUAL_INT_MESSAGE(IDA_RC_NOT_INIT, kc_step(&g_in), "VERSION sonrası örnek kurulmamış olmalı");
    }
}

static void test_A4_3_11_init_3_surum_now_w_ve_bloktan_once(void)
{
    static const uint8_t cop[40] = {1u, 2u, 3u};
    ida_init_in_t ii;
    kc_init_girdi(&ii);
    ii.port_major = (uint16_t)(IDA_PORT_VERSION_MAJOR + 1u);
    ii.now_w.ns = -1;
    TEST_ASSERT_EQUAL_INT_MESSAGE(IDA_RC_VERSION, kc_init(&ii), "sürüm (3), now_w (4)'ten önce");
    kc_init_girdi(&ii);
    ii.port_minor = (uint16_t)(IDA_PORT_VERSION_MINOR + 1u);
    ii.param_blob = NULL;
    ii.param_len = 5u;
    TEST_ASSERT_EQUAL_INT_MESSAGE(IDA_RC_VERSION, kc_init(&ii), "sürüm (3), param_len (4)'ten önce");
    kc_init_girdi(&ii);
    ii.port_major = (uint16_t)(IDA_PORT_VERSION_MAJOR + 1u);
    ii.param_blob = cop;
    ii.param_len = (uint32_t)sizeof cop;
    TEST_ASSERT_EQUAL_INT_MESSAGE(IDA_RC_VERSION, kc_init(&ii), "sürüm (3), geçersiz bloktan (5) önce");
}

static void test_A4_K08_init_4_now_w_araligi(void)
{
    static const int64_t gecersiz[3] = {-1, IDA_TIME_MAX_NS + 1, INT64_MIN};
    ida_init_in_t ii;
    for (int k = 0; k < 3; ++k) {
        kc_hazirla(1);
        kc_init_girdi(&ii);
        ii.now_w.ns = gecersiz[k];
        TEST_ASSERT_EQUAL_INT(IDA_RC_BAD_INPUT, kc_init(&ii));
        kc_in_izgara(&g_in, KC_T0 + 9 * KC_P);
        TEST_ASSERT_EQUAL_INT_MESSAGE(IDA_RC_NOT_INIT, kc_tick(&g_in), "aralık dışı now_w ile init kurmamalı");
    }
    /* Sınırlar geçerli: 0 ve 2^62. */
    kc_init_girdi(&ii);
    ii.now_w.ns = 0;
    TEST_ASSERT_EQUAL_INT(IDA_RC_PARAM_DEFAULTS, kc_init(&ii));
    kc_in_izgara(&g_in, KC_T0);
    TEST_ASSERT_EQUAL_INT(IDA_RC_OK, kc_tick(&g_in));
    kc_init_girdi(&ii);
    ii.now_w.ns = IDA_TIME_MAX_NS;
    TEST_ASSERT_EQUAL_INT(IDA_RC_PARAM_DEFAULTS, kc_init(&ii));
    kc_in_izgara(&g_in, IDA_TIME_MAX_NS);
    TEST_ASSERT_EQUAL_INT(IDA_RC_OK, kc_tick(&g_in));
}

static void test_A4_K101_init_4_blob_null_param_len(void)
{
    ida_init_in_t ii;
    kc_hazirla(1);
    kc_init_girdi(&ii);
    ii.param_blob = NULL;
    ii.param_len = 1u;
    TEST_ASSERT_EQUAL_INT(IDA_RC_BAD_INPUT, kc_init(&ii));
    kc_in_izgara(&g_in, KC_T0 + 5 * KC_P);
    TEST_ASSERT_EQUAL_INT(IDA_RC_NOT_INIT, kc_tick(&g_in));
}

static void test_A4_3_11_init_4_bloktan_once(void)
{
    static const uint8_t cop[40] = {9u, 9u, 9u};
    ida_init_in_t ii;
    kc_init_girdi(&ii);
    ii.now_w.ns = IDA_TIME_MAX_NS + 1;
    ii.param_blob = cop;
    ii.param_len = (uint32_t)sizeof cop;
    TEST_ASSERT_EQUAL_INT_MESSAGE(IDA_RC_BAD_INPUT, kc_init(&ii), "now_w (4), geçersiz bloktan (5) önce");
    kc_init_girdi(&ii);
    ii.now_w.ns = -5;
    ii.param_blob = cop;
    ii.param_len = 9000u;
    TEST_ASSERT_EQUAL_INT_MESSAGE(IDA_RC_BAD_INPUT, kc_init(&ii), "now_w (4), param_len > 8000'den (5) önce");
}

static void test_A4_K78_init_5_varsayilanlar_ornek_kurulur(void)
{
    static uint8_t cop[IDA_PARAM_BLOB_MAX + 8u];
    ida_init_in_t ii;
    memset(cop, 0x3C, sizeof cop);
    /* Blok yok. */
    kc_init_girdi(&ii);
    TEST_ASSERT_EQUAL_INT(IDA_RC_PARAM_DEFAULTS, kc_init(&ii));
    kc_in_izgara(&g_in, KC_T0);
    TEST_ASSERT_EQUAL_INT(IDA_RC_OK, kc_tick(&g_in));
    TEST_ASSERT_FALSE(kc_out()->status.param_ok);
    /* Geçersiz blok. */
    kc_init_girdi(&ii);
    ii.param_blob = cop;
    ii.param_len = 100u;
    TEST_ASSERT_EQUAL_INT(IDA_RC_PARAM_DEFAULTS, kc_init(&ii));
    kc_in_izgara(&g_in, KC_T0);
    TEST_ASSERT_EQUAL_INT(IDA_RC_OK, kc_tick(&g_in));
    TEST_ASSERT_FALSE(kc_out()->status.param_ok);
    /* param_len > 8000. */
    kc_init_girdi(&ii);
    ii.param_blob = cop;
    ii.param_len = IDA_PARAM_BLOB_MAX + 1u;
    TEST_ASSERT_EQUAL_INT(IDA_RC_PARAM_DEFAULTS, kc_init(&ii));
    kc_in_izgara(&g_in, KC_T0);
    TEST_ASSERT_EQUAL_INT(IDA_RC_OK, kc_tick(&g_in));
}

static void test_A4_3_11_init_6_ok(void)
{
    kc_init_gecerli();
    kc_in_izgara(&g_in, KC_T0);
    TEST_ASSERT_EQUAL_INT(IDA_RC_OK, kc_tick(&g_in));
    TEST_ASSERT_TRUE(kc_out()->status.param_ok);
}

/* A4-K77: sıfırlama nedeni, bayrakları ve ardışık arıza sayacı davranışı değiştirmez
   (yalnız RESET_INFO günlüğünün argümanları farklıdır). */
static void test_A4_K77_reset_cause_davranisi_degistirmez(void)
{
    static ida_out_t ilk;
    static ida_out_t simdi;
    ida_init_in_t ii;
    for (unsigned rcu = 0u; rcu <= IDA_RESET_SIL_REBOOT; ++rcu) {
        uint8_t rc = (uint8_t)rcu;
        kc_ortam_kur(2);
        kc_init_girdi(&ii);
        ii.reset_cause = rc;
        ii.reset_flags = 0xDEAD0000u + rc;
        ii.consecutive_faults = (uint8_t)(rc * 3u);
        TEST_ASSERT_EQUAL_INT(IDA_RC_PARAM_DEFAULTS, kc_init(&ii));
        kc_in_step(&g_in, 1u, KC_T0 + KC_P);
        TEST_ASSERT_EQUAL_INT(IDA_RC_OK, kc_step(&g_in));
        simdi = *kc_out();
        TEST_ASSERT_FALSE(simdi.armed);
        for (unsigned i = 0; i < simdi.n_logs; ++i) {
            if (simdi.logs[i].code == IDA_LOG_RESET_INFO) {
                simdi.logs[i].arg0 = 0;
                simdi.logs[i].arg1 = 0;
            }
        }
        if (rc == 0u) {
            ilk = simdi;
        } else {
            TEST_ASSERT_TRUE_MESSAGE(kc_out_esit(&ilk, &simdi), "reset_cause çıktıyı değiştirdi (A4-K77)");
        }
    }
}

/* ===================================================================================
 * Step ve tick: (a1) c ya da in NULL.
 * =================================================================================== */

static void a1_c_null_dene(ida_in_t *in, const char *ad)
{
    for (int f = 0; f < 2; ++f) {
        memset(&g_o, 0xC3, sizeof g_o);
        g_desen = g_o;
        ida_rc_t rc = f == 0 ? ida_core_step(NULL, in, &g_o) : ida_core_tick(NULL, in, &g_o);
        TEST_ASSERT_EQUAL_INT_MESSAGE(IDA_RC_BAD_INPUT, rc, ad);
        TEST_ASSERT_EQUAL_MEMORY_MESSAGE(&g_desen, &g_o, sizeof g_o, "(a1) c NULL: out'a yazıldı");
    }
}

static void test_A4_K113_a1_c_null_her_kosulla(void)
{
    kc_hazirla(3);
    kc_in_step(&g_in, kc_sonraki_seq(), kc_sonraki_t());
    a1_c_null_dene(&g_in, "c NULL, geçerli girdi");
    g_in.n_samples = 17u;
    a1_c_null_dene(&g_in, "c NULL × BAD_INPUT");
    kc_in_step(&g_in, kc_son_seq(), kc_sonraki_t());
    a1_c_null_dene(&g_in, "c NULL × DUP_SEQ");
    kc_in_step(&g_in, kc_son_seq() - 1u, kc_sonraki_t());
    a1_c_null_dene(&g_in, "c NULL × OLD_SEQ");
    kc_in_step(&g_in, kc_sonraki_seq(), kc_sonraki_t());
    g_in.imu.imu.gyro[0] = nan_f();
    a1_c_null_dene(&g_in, "c NULL × IMU_REJECTED");
    /* Durum değişmedi: aynı seq hâlâ yeni. */
    kc_in_step(&g_in, kc_sonraki_seq(), kc_sonraki_t());
    TEST_ASSERT_EQUAL_INT(IDA_RC_OK, kc_step(&g_in));
}

static void a1_in_null_dene(int i_ornek, const char *ad)
{
    size_t boy = kc_core_boyu();
    for (int f = 0; f < 2; ++f) {
        memcpy(g_kopya, kc_bellek(i_ornek), boy);
        memset(&g_o, 0x3C, sizeof g_o);
        g_desen = g_o;
        ida_rc_t rc = f == 0 ? ida_core_step(kc_core(i_ornek), NULL, &g_o)
                             : ida_core_tick(kc_core(i_ornek), NULL, &g_o);
        TEST_ASSERT_EQUAL_INT_MESSAGE(IDA_RC_BAD_INPUT, rc, ad);
        TEST_ASSERT_EQUAL_MEMORY_MESSAGE(&g_desen, &g_o, sizeof g_o, "(a1) in NULL: out'a yazıldı");
        TEST_ASSERT_EQUAL_MEMORY_MESSAGE(g_kopya, kc_bellek(i_ornek), boy, "(a1) in NULL: örnek durumu değişti");
        rc = f == 0 ? ida_core_step(kc_core(i_ornek), NULL, NULL) : ida_core_tick(kc_core(i_ornek), NULL, NULL);
        TEST_ASSERT_EQUAL_INT_MESSAGE(IDA_RC_BAD_INPUT, rc, ad);
        TEST_ASSERT_EQUAL_MEMORY_MESSAGE(g_kopya, kc_bellek(i_ornek), boy, "(a1) in, out NULL: örnek değişti");
    }
}

static void test_A4_K113_a1_in_null_hicbir_sey_yazilmaz(void)
{
    /* c/in NULL × NOT_INIT: kurulmamış örnekte de BAD_INPUT. */
    for (int i = 0; i < kc_ornek_sayisi(); ++i) {
        a1_in_null_dene(i, "in NULL × NOT_INIT");
    }
    kc_hazirla(3);
    for (int i = 0; i < kc_ornek_sayisi(); ++i) {
        a1_in_null_dene(i, "in NULL, kurulu örnek");
    }
    /* Sonraki çağrı: sıra izi değişmemiş (DUP), bad_input sayılmamış (düzenek farkı). */
    kc_in_step(&g_in, kc_son_seq(), kc_sonraki_t());
    TEST_ASSERT_EQUAL_INT(IDA_RC_DUP_SEQ, kc_step(&g_in));
    TEST_ASSERT_EQUAL_UINT32(1u, KC_FARK(dup_seq));
    TEST_ASSERT_EQUAL_UINT32(0u, KC_FARK(bad_input));
}

/* ===================================================================================
 * (a2) kurulmamış örnek.
 * =================================================================================== */

static void test_A4_K113_a2_kurulmamis_not_init(void)
{
    /* Hiç init edilmemiş bellek (dört örnek: 0x00, 0xFF, 0x5A, sözde rastgele). */
    kc_in_step(&g_in, 1u, KC_T0);
    TEST_ASSERT_EQUAL_INT(IDA_RC_NOT_INIT, kc_step(&g_in));
    kc_in_izgara(&g_in, KC_T0);
    TEST_ASSERT_EQUAL_INT(IDA_RC_NOT_INIT, kc_tick(&g_in));
    TEST_ASSERT_EQUAL_INT(IDA_RC_NOT_INIT, kc_cagir_out_null(KC_STEP, &g_in));
    TEST_ASSERT_EQUAL_INT(IDA_RC_NOT_INIT, kc_cagir_out_null(KC_TICK, &g_in));
}

static void test_A4_3_11_oncelik_NOT_INIT_her_kosuldan_once(void)
{
    ida_init_in_t ii;
    kc_hazirla(5);
    kc_init_girdi(&ii);
    ii.port_major = 99u;
    TEST_ASSERT_EQUAL_INT(IDA_RC_VERSION, kc_init(&ii));
    /* NOT_INIT × BAD_INPUT */
    kc_in_step(&g_in, 6u, KC_T0 + 6 * KC_P);
    g_in.n_samples = 17u;
    TEST_ASSERT_EQUAL_INT(IDA_RC_NOT_INIT, kc_step(&g_in));
    kc_in_tick(&g_in, 6u, KC_T0 + 6 * KC_P);
    g_in.imu_present = true;
    TEST_ASSERT_EQUAL_INT(IDA_RC_NOT_INIT, kc_tick(&g_in));
    /* NOT_INIT × DUP_SEQ ve OLD_SEQ (kurulumdan önceki son seq 5). */
    kc_in_step(&g_in, 5u, KC_T0 + 7 * KC_P);
    TEST_ASSERT_EQUAL_INT(IDA_RC_NOT_INIT, kc_step(&g_in));
    kc_in_tick(&g_in, 4u, KC_T0 + 7 * KC_P);
    TEST_ASSERT_EQUAL_INT(IDA_RC_NOT_INIT, kc_tick(&g_in));
    /* NOT_INIT × IMU_REJECTED */
    kc_in_step(&g_in, 8u, KC_T0 + 8 * KC_P);
    g_in.imu.hdr.flags = 0u;
    TEST_ASSERT_EQUAL_INT(IDA_RC_NOT_INIT, kc_step(&g_in));
    /* NOT_INIT × (b2) out NULL; mandal ve komutlarla (A4-K114: REJECTED / NOT_INIT). */
    kc_in_step(&g_in, 9u, KC_T0 + 9 * KC_P);
    kc_mandal_kur(&g_in, (uint8_t)IDA_NARROW_DEFINED, 100u);
    (void)kc_komut_ekle(&g_in, 200u, (uint8_t)IDA_CMD_HEARTBEAT);
    kc_komut_ekle(&g_in, 201u, (uint8_t)IDA_CMD_PARAM_GET)->u.param.id = 5u;
    TEST_ASSERT_EQUAL_INT(IDA_RC_NOT_INIT, kc_cagir_out_null(KC_STEP, &g_in));
    TEST_ASSERT_EQUAL_INT(IDA_RC_NOT_INIT, kc_step(&g_in));
    TEST_ASSERT_FALSE(kc_out()->health.narrowed);
}

/* ===================================================================================
 * (b3) SESSION_START: sıra izi ve örnek kabul durumu her çağrıda sıfırlanır.
 * =================================================================================== */

static void test_A4_K72_b3_session_start_ayni_seq_kabul(void)
{
    kc_hazirla(3);
    /* (b3) (d)'den önce: son kabul edilen seq ile gelen zaman çerçevesi kabul edilir. */
    kc_in_step(&g_in, 3u, kc_sonraki_t());
    g_in.plat.link_event = (uint8_t)IDA_LINKEV_SESSION_START;
    TEST_ASSERT_EQUAL_INT(IDA_RC_OK, kc_step(&g_in));
    TEST_ASSERT_EQUAL_UINT32(3u, kc_out()->seq);
}

static void test_A4_K15_b3_session_start_zaman_geri(void)
{
    kc_hazirla(4);
    int64_t now = kc_son_now();
    /* Yeni oturumda ölçüm zamanı sürekli sayılmaz: tf.t geri gider, seq de keyfî. */
    kc_in_step(&g_in, 1000u, KC_T0 + KC_P / 2);
    g_in.now_w.ns = now;
    g_in.tf.t_rx.ns = now;
    g_in.imu.hdr.t_rx.ns = now;
    g_in.plat.link_event = (uint8_t)IDA_LINKEV_SESSION_START;
    TEST_ASSERT_EQUAL_INT(IDA_RC_OK, kc_step(&g_in));
    /* Aynı oturumda seq artıp tf.t artmazsa BAD_INPUT (A4-K15). */
    kc_in_step(&g_in, 1001u, KC_T0 + KC_P / 2);
    g_in.now_w.ns = now;
    g_in.tf.t_rx.ns = now;
    g_in.imu.hdr.t_rx.ns = now;
    g_in.imu.hdr.t_meas.ns = KC_T0 + KC_P / 2;
    TEST_ASSERT_EQUAL_INT(IDA_RC_BAD_INPUT, kc_step(&g_in));
}

static void test_A4_K57_b3_reddedilen_cagrida_son_gecerli_sifir(void)
{
    kc_hazirla(3);
    kc_in_step(&g_in, 50u, kc_sonraki_t());
    g_in.plat.link_event = (uint8_t)IDA_LINKEV_SESSION_START;
    g_in.n_samples = 17u;
    TEST_ASSERT_EQUAL_INT(IDA_RC_BAD_INPUT, kc_step(&g_in));
    TEST_ASSERT_EQUAL_UINT32(0u, kc_out()->seq);
    TEST_ASSERT_TRUE(kc_out()->t_ref.ns == 0);
    /* İz sıfırlandı: eski seq yeniden kabul edilir (geri alınmaz). */
    kc_in_step(&g_in, 3u, kc_sonraki_t());
    TEST_ASSERT_EQUAL_INT(IDA_RC_OK, kc_step(&g_in));
}

static void test_A4_K72_b3_out_null_cagrisinda_iz_sifirlanir(void)
{
    kc_hazirla(3);
    kc_in_step(&g_in, 77u, kc_sonraki_t());
    g_in.plat.link_event = (uint8_t)IDA_LINKEV_SESSION_START;
    TEST_ASSERT_EQUAL_INT(IDA_RC_BAD_INPUT, kc_cagir_out_null(KC_STEP, &g_in));
    /* §7 K113 (7): sonraki çağrıda eski tf.seq DUP_SEQ vermez; IMU kabul durumu da sıfır. */
    kc_in_step(&g_in, 3u, kc_sonraki_t());
    g_in.imu.hdr.seq = 3u;
    TEST_ASSERT_EQUAL_INT_MESSAGE(IDA_RC_OK, kc_step(&g_in),
                                  "out NULL çağrısındaki SESSION_START izi sıfırlamalı (3.11 (b3))");
    TEST_ASSERT_EQUAL_UINT32(3u, kc_out()->seq);
}

static void test_A4_K72_b3_ornek_kabul_durumu_sifirlanir(void)
{
    kc_hazirla(2);
    kc_in_step(&g_in, 3u, KC_T0 + 3 * KC_P);
    (void)kc_ornek_ekle(&g_in, (uint8_t)IDA_SENS_MAG, 0u, 500u, KC_T0 + 3 * KC_P);
    TEST_ASSERT_EQUAL_INT(IDA_RC_OK, kc_step(&g_in));
    /* SESSION_START taşıyan, (c)'de reddedilen çağrı. */
    kc_in_step(&g_in, 4u, KC_T0 + 4 * KC_P);
    g_in.plat.link_event = (uint8_t)IDA_LINKEV_SESSION_START;
    g_in.n_cmds = 9u;
    TEST_ASSERT_EQUAL_INT(IDA_RC_BAD_INPUT, kc_step(&g_in));
    /* Aynı IMU seq'i ve aynı MAG seq'i/ölçüm zamanı yeni oturumda ilk örnek olarak kabul. */
    kc_in_step(&g_in, 3u, KC_T0 + 5 * KC_P);
    g_in.imu.hdr.seq = 3u;
    (void)kc_ornek_ekle(&g_in, (uint8_t)IDA_SENS_MAG, 0u, 500u, KC_T0 + 3 * KC_P);
    TEST_ASSERT_EQUAL_INT(IDA_RC_OK, kc_step(&g_in));
    TEST_ASSERT_EQUAL_UINT32(0u, KC_FARK(sample_rejected));
    TEST_ASSERT_EQUAL_UINT32(0u, KC_FARK(sample_future));
}

/* ===================================================================================
 * (b2) out NULL.
 * =================================================================================== */

static void test_A4_3_11_b2_baska_hicbir_sey_yapilmaz(void)
{
    kc_hazirla(2);
    kc_in_step(&g_in, 3u, KC_T0 + 3 * KC_P);
    (void)kc_ornek_ekle(&g_in, (uint8_t)IDA_SENS_MAG, 0u, 70u, KC_T0 + 3 * KC_P);
    (void)kc_komut_ekle(&g_in, 900u, (uint8_t)IDA_CMD_HEARTBEAT);
    TEST_ASSERT_EQUAL_INT(IDA_RC_BAD_INPUT, kc_cagir_out_null(KC_STEP, &g_in));
    /* Zaman çerçevesi, IMU, örnek ve komut işlenmemiş; bad_input sayılmamış. */
    kc_in_step(&g_in, 3u, KC_T0 + 3 * KC_P);
    (void)kc_ornek_ekle(&g_in, (uint8_t)IDA_SENS_MAG, 0u, 70u, KC_T0 + 3 * KC_P);
    (void)kc_komut_ekle(&g_in, 900u, (uint8_t)IDA_CMD_HEARTBEAT);
    TEST_ASSERT_EQUAL_INT(IDA_RC_OK, kc_step(&g_in));
    TEST_ASSERT_EQUAL_UINT32(0u, KC_FARK(bad_input));
    TEST_ASSERT_EQUAL_UINT32(0u, KC_FARK(sample_rejected));
    TEST_ASSERT_TRUE_MESSAGE(kc_out()->results[0].ack != IDA_ACK_DUPLICATE,
                             "out NULL çağrısının komutu yineleme penceresine girmiş");
}

static void test_A4_3_11_b2_once_d_ve_e(void)
{
    kc_hazirla(3);
    /* (b2) × DUP_SEQ */
    kc_in_step(&g_in, kc_son_seq(), kc_sonraki_t());
    TEST_ASSERT_EQUAL_INT(IDA_RC_BAD_INPUT, kc_cagir_out_null(KC_STEP, &g_in));
    /* (b2) × OLD_SEQ */
    kc_in_tick(&g_in, kc_son_seq() - 2u, kc_sonraki_t());
    TEST_ASSERT_EQUAL_INT(IDA_RC_BAD_INPUT, kc_cagir_out_null(KC_TICK, &g_in));
    /* (b2) × IMU_REJECTED */
    kc_in_step(&g_in, kc_sonraki_seq(), kc_sonraki_t());
    g_in.imu.hdr.flags = 0u;
    TEST_ASSERT_EQUAL_INT(IDA_RC_BAD_INPUT, kc_cagir_out_null(KC_STEP, &g_in));
    /* (b2) × (c) BAD_INPUT */
    kc_in_step(&g_in, kc_sonraki_seq(), kc_sonraki_t());
    g_in.tf.t.ns = -1;
    TEST_ASSERT_EQUAL_INT(IDA_RC_BAD_INPUT, kc_cagir_out_null(KC_STEP, &g_in));
    /* Hiçbiri sayılmadı; IMU reddedilmedi (sonraki yazılabilir çağrının farkları). */
    TEST_ASSERT_EQUAL_INT(IDA_RC_OK, kc_adim());
    TEST_ASSERT_EQUAL_UINT32(0u, KC_FARK(dup_seq));
    TEST_ASSERT_EQUAL_UINT32(0u, KC_FARK(old_seq));
    TEST_ASSERT_EQUAL_UINT32(0u, KC_FARK(imu_rejected));
    TEST_ASSERT_EQUAL_UINT32(0u, KC_FARK(bad_input));
}

/* 3.11 (b2): (b) ve (b3)'ün günlükleri (SESSION) bekletilir ve ilk yazılabilir çağrıda
   bir kez verilir. */
static void test_A4_3_11_b2_oturum_gunlukleri_bekletilir(void)
{
    for (int k = 0; k < 2; ++k) {
        uint8_t olay = (uint8_t)(k == 0 ? IDA_LINKEV_SESSION_END : IDA_LINKEV_SESSION_START);
        kc_hazirla(2);
        kc_in_izgara(&g_in, kc_sonraki_t());
        g_in.plat.link_event = olay;
        TEST_ASSERT_EQUAL_INT(IDA_RC_BAD_INPUT, kc_cagir_out_null(KC_TICK, &g_in));
        kc_in_izgara(&g_in, kc_sonraki_t());
        TEST_ASSERT_EQUAL_INT(IDA_RC_OK, kc_tick(&g_in));
        const ida_log_t *l = kc_gunluk_bul(kc_out(), (uint16_t)IDA_LOG_SESSION);
        TEST_ASSERT_NOT_NULL_MESSAGE(l, k == 0 ? "bekletilen SESSION(END) günlüğü yok" : "bekletilen SESSION(START) günlüğü yok");
        TEST_ASSERT_EQUAL_INT32((int32_t)olay, l->arg0);
        kc_in_izgara(&g_in, kc_sonraki_t());
        TEST_ASSERT_EQUAL_INT(IDA_RC_OK, kc_tick(&g_in));
        TEST_ASSERT_EQUAL_INT(0, kc_gunluk_say(kc_out(), (uint16_t)IDA_LOG_SESSION));
    }
}

static void test_A4_K10_b2_now_izi_guncellenir(void)
{
    kc_hazirla(2);
    /* out NULL çağrısı ileri now_w taşır: iz güncellenir. */
    kc_in_step(&g_in, 3u, KC_T0 + 100 * KC_MS);
    TEST_ASSERT_EQUAL_INT(IDA_RC_BAD_INPUT, kc_cagir_out_null(KC_STEP, &g_in));
    /* Sonraki çağrının now_w'si izden küçük: geri adım +1 ve WALL_BACKSTEP (düzenek). */
    kc_in_step(&g_in, 3u, KC_T0 + 3 * KC_P);
    TEST_ASSERT_EQUAL_INT(IDA_RC_OK, kc_step(&g_in));
    TEST_ASSERT_EQUAL_UINT32(1u, KC_FARK(wall_backstep));
    const ida_log_t *l = kc_gunluk_bul(kc_out(), (uint16_t)IDA_LOG_WALL_BACKSTEP);
    TEST_ASSERT_NOT_NULL(l);
    TEST_ASSERT_EQUAL_INT32((int32_t)((100 * KC_MS - 3 * KC_P) / KC_US), l->arg0);
    /* out NULL çağrısının kendi geri adımı sayılır ve günlüğü bekletilir. */
    kc_in_step(&g_in, 4u, KC_T0 + 50 * KC_MS);
    TEST_ASSERT_EQUAL_INT(IDA_RC_BAD_INPUT, kc_cagir_out_null(KC_STEP, &g_in));
    kc_in_step(&g_in, 4u, KC_T0 + 100 * KC_MS);
    TEST_ASSERT_EQUAL_INT(IDA_RC_OK, kc_step(&g_in));
    TEST_ASSERT_EQUAL_UINT32(1u, KC_FARK(wall_backstep));
    TEST_ASSERT_EQUAL_INT(1, kc_gunluk_say(kc_out(), (uint16_t)IDA_LOG_WALL_BACKSTEP));
}

static void test_A4_K109_b2_bg_sayimi_karsilastirmadan_sifirlanir(void)
{
    kc_hazirla(2);
    TEST_ASSERT_TRUE(kc_bg(kc_son_now()) != IDA_BG_BAD_INPUT);
    TEST_ASSERT_TRUE(kc_bg(kc_son_now()) != IDA_BG_BAD_INPUT);
    kc_in_step(&g_in, kc_sonraki_seq(), kc_sonraki_t());
    kc_bg_slices_elle(77u); /* yanlış sayı: karşılaştırılmamalı */
    TEST_ASSERT_EQUAL_INT(IDA_RC_BAD_INPUT, kc_cagir_out_null(KC_STEP, &g_in));
    TEST_ASSERT_TRUE(kc_bg(kc_son_now()) != IDA_BG_BAD_INPUT);
    /* Platformun sayımı da out NULL çağrısından sonra yeniden başlar: 1. */
    TEST_ASSERT_EQUAL_INT(IDA_RC_OK, kc_adim());
    TEST_ASSERT_EQUAL_UINT32(0u, KC_FARK(bg_slice_mismatch));
}

static void test_A4_K117_b2_bg_bad_input_bayragi_tasinir(void)
{
    kc_hazirla(2);
    TEST_ASSERT_EQUAL_INT(IDA_BG_BAD_INPUT, kc_bg(-1));
    kc_in_step(&g_in, kc_sonraki_seq(), kc_sonraki_t());
    TEST_ASSERT_EQUAL_INT(IDA_RC_BAD_INPUT, kc_cagir_out_null(KC_STEP, &g_in));
    kc_in_izgara(&g_in, kc_sonraki_t());
    TEST_ASSERT_EQUAL_INT(IDA_RC_BAD_INPUT, kc_cagir_out_null(KC_TICK, &g_in));
    TEST_ASSERT_EQUAL_INT(IDA_RC_OK, kc_adim());
    TEST_ASSERT_TRUE(kc_out()->health.bg_bad_input);
    TEST_ASSERT_EQUAL_UINT32(1u, KC_FARK(bg_bad_input));
    TEST_ASSERT_EQUAL_INT(IDA_RC_OK, kc_adim());
    TEST_ASSERT_FALSE(kc_out()->health.bg_bad_input);
}

/* ===================================================================================
 * (c) yapısal doğrulama.
 * =================================================================================== */

static void test_A4_K08_c_now_w_araligi(void)
{
    static const int64_t gecersiz[2] = {-1, IDA_TIME_MAX_NS + 1};
    kc_hazirla(3);
    for (int k = 0; k < 2; ++k) {
        kc_in_step(&g_in, kc_sonraki_seq(), kc_sonraki_t());
        g_in.now_w.ns = gecersiz[k];
        TEST_ASSERT_EQUAL_INT(IDA_RC_BAD_INPUT, kc_step(&g_in));
        kc_in_izgara(&g_in, gecersiz[k]);
        TEST_ASSERT_EQUAL_INT(IDA_RC_BAD_INPUT, kc_tick(&g_in));
    }
    TEST_ASSERT_EQUAL_INT(IDA_RC_OK, kc_adim());
}

static void test_A4_K57_c_tf_t_araligi_ve_son_gecerli(void)
{
    static const int64_t gecersiz[2] = {-1, IDA_TIME_MAX_NS + 1};
    kc_hazirla(3);
    for (int k = 0; k < 2; ++k) {
        kc_in_step(&g_in, kc_sonraki_seq(), kc_sonraki_t());
        g_in.tf.t.ns = gecersiz[k];
        TEST_ASSERT_EQUAL_INT(IDA_RC_BAD_INPUT, kc_step(&g_in));
        TEST_ASSERT_EQUAL_UINT32(3u, kc_out()->seq);
        TEST_ASSERT_TRUE_MESSAGE(kc_out()->t_ref.ns == KC_T0 + 3 * KC_P, "girdideki geçersiz tf.t yazıldı (A4-K57)");
        kc_in_tick(&g_in, kc_sonraki_seq(), kc_sonraki_t());
        g_in.tf.t.ns = gecersiz[k];
        TEST_ASSERT_EQUAL_INT(IDA_RC_BAD_INPUT, kc_tick(&g_in));
    }
}

static void test_A4_K14_c_tf_t_rx(void)
{
    kc_hazirla(3);
    kc_in_step(&g_in, kc_sonraki_seq(), kc_sonraki_t());
    g_in.tf.t_rx.ns = -1;
    TEST_ASSERT_EQUAL_INT(IDA_RC_BAD_INPUT, kc_step(&g_in));
    kc_in_step(&g_in, kc_sonraki_seq(), kc_sonraki_t());
    g_in.tf.t_rx.ns = g_in.now_w.ns + 1;
    TEST_ASSERT_EQUAL_INT(IDA_RC_BAD_INPUT, kc_step(&g_in));
    kc_in_tick(&g_in, kc_sonraki_seq(), kc_sonraki_t());
    g_in.tf.t_rx.ns = g_in.now_w.ns + KC_MS;
    TEST_ASSERT_EQUAL_INT(IDA_RC_BAD_INPUT, kc_tick(&g_in));
}

static void test_A4_K10_c_t_rx_kullanilan_now_ile_karsilastirilir(void)
{
    kc_hazirla(3);
    int64_t t = kc_sonraki_t();
    /* Önce ileri bir now_w (ızgara tick'i), sonra geri adımlı step. */
    kc_in_izgara(&g_in, t + 20 * KC_MS);
    TEST_ASSERT_EQUAL_INT(IDA_RC_OK, kc_tick(&g_in));
    kc_in_step(&g_in, kc_sonraki_seq(), t);
    g_in.now_w.ns = t + 10 * KC_MS;  /* verilen: izden 10 ms geride */
    g_in.tf.t_rx.ns = t + 15 * KC_MS; /* verilenden büyük, kullanılandan küçük */
    g_in.imu.hdr.t_rx.ns = t;
    TEST_ASSERT_EQUAL_INT_MESSAGE(IDA_RC_OK, kc_step(&g_in), "tf.t_rx kullanılan now_w ile karşılaştırılmalı (A4-K10)");
    TEST_ASSERT_EQUAL_UINT32(1u, KC_FARK(wall_backstep));
}

static void test_A4_K15_c_seq_artti_t_artmadi(void)
{
    kc_hazirla(3);
    int64_t t = kc_son_t();
    kc_in_step(&g_in, kc_sonraki_seq(), t);
    g_in.now_w.ns = kc_son_now() + KC_P;
    TEST_ASSERT_EQUAL_INT(IDA_RC_BAD_INPUT, kc_step(&g_in));
    kc_in_tick(&g_in, kc_sonraki_seq(), t - KC_P);
    g_in.now_w.ns = kc_son_now() + KC_P;
    TEST_ASSERT_EQUAL_INT(IDA_RC_BAD_INPUT, kc_tick(&g_in));
    /* Atlanan seq (fark 2) ve artan tf.t kabul (A4-K25). */
    kc_in_step(&g_in, kc_son_seq() + 2u, kc_sonraki_t());
    TEST_ASSERT_EQUAL_INT(IDA_RC_OK, kc_step(&g_in));
}

static void test_A4_K17_c_imu_present(void)
{
    kc_hazirla(3);
    kc_in_step(&g_in, kc_sonraki_seq(), kc_sonraki_t());
    g_in.imu_present = false;
    TEST_ASSERT_EQUAL_INT(IDA_RC_BAD_INPUT, kc_step(&g_in));
    kc_in_tick(&g_in, kc_sonraki_seq(), kc_sonraki_t());
    g_in.imu_present = true;
    kc_imu_kur(&g_in.imu, kc_sonraki_seq(), g_in.tf.t.ns);
    TEST_ASSERT_EQUAL_INT(IDA_RC_BAD_INPUT, kc_tick(&g_in));
    kc_in_izgara(&g_in, kc_sonraki_t());
    g_in.imu_present = true;
    TEST_ASSERT_EQUAL_INT(IDA_RC_BAD_INPUT, kc_tick(&g_in));
}

static void test_A4_K31_c_kapasite_asimi(void)
{
    kc_hazirla(3);
    for (int k = 0; k < 5; ++k) {
        for (int f = 0; f < 2; ++f) {
            if (f == 0) {
                kc_in_step(&g_in, kc_sonraki_seq(), kc_sonraki_t());
            } else {
                kc_in_tick(&g_in, kc_sonraki_seq(), kc_sonraki_t());
            }
            if (k == 0) {
                g_in.n_samples = 17u;
            } else if (k == 1) {
                g_in.n_cmds = 9u;
            } else if (k == 2) {
                g_in.n_det = 9u;
            } else if (k == 3) {
                g_in.n_det_batch = 3u;
            } else {
                g_in.n_evres = 5u;
            }
            TEST_ASSERT_EQUAL_INT(IDA_RC_BAD_INPUT, kc_cagir(f == 0 ? KC_STEP : KC_TICK, &g_in));
        }
    }
    /* Sınır değerler (16, 8, 8, 2, 4) yapısal hata değildir; içerik tek tek reddedilir. */
    kc_in_step(&g_in, kc_sonraki_seq(), kc_sonraki_t());
    g_in.n_samples = 16u;
    g_in.n_det = 8u;
    g_in.n_det_batch = 2u;
    g_in.n_evres = 4u;
    for (uint32_t j = 0; j < 8u; ++j) {
        (void)kc_komut_ekle(&g_in, 300u + j, (uint8_t)IDA_CMD_HEARTBEAT);
    }
    TEST_ASSERT_EQUAL_INT(IDA_RC_OK, kc_step(&g_in));
    TEST_ASSERT_EQUAL_UINT8(8u, kc_out()->n_results);
}

static void test_A4_K46_c_mandal_alani(void)
{
    static const uint8_t tanimsiz[3] = {0x10u, 0x80u, 0xF0u};
    static const uint8_t gecersiz_kip[6] = {IDA_MODE_INIT, IDA_MODE_DISARMED, IDA_MODE_GUIDED, IDA_MODE_AUTO, 8u, 255u};
    kc_hazirla(3);
    for (int k = 0; k < 3; ++k) {
        kc_in_step(&g_in, kc_sonraki_seq(), kc_sonraki_t());
        g_in.narrow.mask = tanimsiz[k];
        TEST_ASSERT_EQUAL_INT(IDA_RC_BAD_INPUT, kc_step(&g_in));
        TEST_ASSERT_EQUAL_UINT8(0u, kc_out()->n_results);
        /* Tanımlı bit yine uygulanır ve ACCEPTED (düzenek: sonuç dizisi ve health.narrowed). */
        kc_in_step(&g_in, kc_sonraki_seq(), kc_sonraki_t());
        g_in.narrow.mask = (uint8_t)(tanimsiz[k] | IDA_NARROW_DISARM);
        g_in.narrow.cmd_id[0] = 41u;
        TEST_ASSERT_EQUAL_INT(IDA_RC_BAD_INPUT, kc_step(&g_in));
        TEST_ASSERT_TRUE(kc_out()->health.narrowed);
    }
    for (int k = 0; k < 6; ++k) {
        kc_in_step(&g_in, kc_sonraki_seq(), kc_sonraki_t());
        kc_mandal_kur(&g_in, (uint8_t)IDA_NARROW_MODE, 60u);
        g_in.narrow.mode = gecersiz_kip[k];
        TEST_ASSERT_EQUAL_INT(IDA_RC_BAD_INPUT, kc_step(&g_in));
        TEST_ASSERT_EQUAL_UINT8(IDA_ACK_ACCEPTED, kc_out()->results[0].ack);
    }
    for (unsigned kipu = IDA_MODE_MANUAL; kipu <= IDA_MODE_STATION_KEEP; ++kipu) {
        uint8_t kip = (uint8_t)kipu;
        kc_in_step(&g_in, kc_sonraki_seq(), kc_sonraki_t());
        kc_mandal_kur(&g_in, (uint8_t)IDA_NARROW_MODE, 70u);
        g_in.narrow.mode = kip;
        TEST_ASSERT_EQUAL_INT(IDA_RC_OK, kc_step(&g_in));
    }
}

static void test_A4_K46_c_cmds_set_mode_gecersiz_hedef(void)
{
    static const uint8_t gecersiz[4] = {0u, 1u, 8u, 200u};
    kc_hazirla(3);
    for (int k = 0; k < 4; ++k) {
        kc_in_step(&g_in, kc_sonraki_seq(), kc_sonraki_t());
        kc_komut_ekle(&g_in, 80u + (uint32_t)k, (uint8_t)IDA_CMD_SET_MODE)->u.set_mode.mode = gecersiz[k];
        TEST_ASSERT_EQUAL_INT(IDA_RC_BAD_INPUT, kc_step(&g_in));
        TEST_ASSERT_EQUAL_UINT8(IDA_ACK_ACCEPTED, kc_out()->results[0].ack);
    }
    for (unsigned kipu = IDA_MODE_MANUAL; kipu <= IDA_MODE_STATION_KEEP; ++kipu) {
        uint8_t kip = (uint8_t)kipu;
        kc_in_step(&g_in, kc_sonraki_seq(), kc_sonraki_t());
        kc_komut_ekle(&g_in, 90u + kip, (uint8_t)IDA_CMD_SET_MODE)->u.set_mode.mode = kip;
        TEST_ASSERT_EQUAL_INT(IDA_RC_OK, kc_step(&g_in));
    }
}

static void test_A4_3_11_c_link_alanlari(void)
{
    kc_hazirla(3);
    kc_in_step(&g_in, kc_sonraki_seq(), kc_sonraki_t());
    g_in.plat.link_event = 3u;
    TEST_ASSERT_EQUAL_INT(IDA_RC_BAD_INPUT, kc_step(&g_in));
    kc_in_izgara(&g_in, kc_sonraki_t());
    g_in.plat.link_state = 2u;
    TEST_ASSERT_EQUAL_INT(IDA_RC_BAD_INPUT, kc_tick(&g_in));
    kc_in_izgara(&g_in, kc_sonraki_t());
    g_in.plat.link_event = 255u;
    TEST_ASSERT_EQUAL_INT(IDA_RC_BAD_INPUT, kc_tick(&g_in));
}

static void test_A4_K122_c_oturum_sonu_alan_kisitlari_denetlenmez(void)
{
    kc_hazirla(3);
    kc_in_oturum_sonu(&g_in, kc_sonraki_t());
    (void)kc_komut_ekle(&g_in, 400u, (uint8_t)IDA_CMD_HEARTBEAT);
    (void)kc_ornek_ekle(&g_in, (uint8_t)IDA_SENS_MAG, 0u, 1u, kc_son_now());
    TEST_ASSERT_EQUAL_INT_MESSAGE(IDA_RC_OK, kc_tick(&g_in), "A4-K122 alan kısıtı ihlali (c)'de denetlenmez");
    /* Yeni oturum; kapasite aşımı ise oturum sonu çağrısında da A4-K31'dir. */
    kc_in_izgara(&g_in, kc_son_now() + KC_P);
    g_in.plat.link_event = (uint8_t)IDA_LINKEV_SESSION_START;
    TEST_ASSERT_EQUAL_INT(IDA_RC_OK, kc_tick(&g_in));
    kc_in_oturum_sonu(&g_in, kc_son_now() + KC_P);
    g_in.n_samples = 17u;
    TEST_ASSERT_EQUAL_INT(IDA_RC_BAD_INPUT, kc_tick(&g_in));
}

/* ===================================================================================
 * Öncelik tablosu: BAD_INPUT × {DUP, OLD, IMU_REJECTED}, DUP × IMU_REJECTED,
 * OLD × IMU_REJECTED. (DUP × OLD birlikte oluşmaz.)
 * =================================================================================== */

static void test_A4_3_11_oncelik_BAD_INPUT_x_DUP_OLD_IMU(void)
{
    kc_hazirla(4);
    kc_in_step(&g_in, kc_son_seq(), kc_sonraki_t());
    g_in.n_samples = 17u;
    TEST_ASSERT_EQUAL_INT(IDA_RC_BAD_INPUT, kc_step(&g_in));
    kc_in_tick(&g_in, kc_son_seq() - 1u, kc_sonraki_t());
    g_in.n_cmds = 9u;
    TEST_ASSERT_EQUAL_INT(IDA_RC_BAD_INPUT, kc_tick(&g_in));
    kc_in_step(&g_in, kc_sonraki_seq(), kc_sonraki_t());
    g_in.imu.imu.accel[1] = sonsuz_f();
    g_in.now_w.ns = -1;
    TEST_ASSERT_EQUAL_INT(IDA_RC_BAD_INPUT, kc_step(&g_in));
    TEST_ASSERT_FALSE(kc_out()->health.imu_rejected);
    kc_in_step(&g_in, kc_son_seq(), kc_sonraki_t());
    g_in.imu.hdr.flags = 0u;
    g_in.plat.link_state = 9u;
    TEST_ASSERT_EQUAL_INT(IDA_RC_BAD_INPUT, kc_step(&g_in));
}

static void test_A4_3_11_oncelik_DUP_OLD_x_IMU_REJECTED(void)
{
    kc_hazirla(4);
    kc_in_step(&g_in, kc_son_seq(), kc_sonraki_t());
    g_in.imu.hdr.flags = 0u;
    TEST_ASSERT_EQUAL_INT(IDA_RC_DUP_SEQ, kc_step(&g_in));
    kc_in_step(&g_in, kc_son_seq() - 1u, kc_sonraki_t());
    g_in.imu.imu.gyro[2] = nan_f();
    TEST_ASSERT_EQUAL_INT(IDA_RC_OLD_SEQ, kc_step(&g_in));
    /* IMU değerlendirilmedi: imu_rejected sayılmadı (düzenek), sonraki çağrıda aynı IMU kabul. */
    kc_in_step(&g_in, kc_sonraki_seq(), kc_sonraki_t());
    TEST_ASSERT_EQUAL_INT(IDA_RC_OK, kc_step(&g_in));
}

/* ===================================================================================
 * (d) sıra denetimi (A4-K25).
 * =================================================================================== */

static void test_A4_K25_d_dup_old_ve_pencere_siniri(void)
{
    kc_hazirla(5);
    uint32_t s = kc_son_seq();
    kc_in_step(&g_in, s, kc_sonraki_t());
    TEST_ASSERT_EQUAL_INT(IDA_RC_DUP_SEQ, kc_step(&g_in));
    kc_in_step(&g_in, s - 1u, kc_sonraki_t());
    TEST_ASSERT_EQUAL_INT(IDA_RC_OLD_SEQ, kc_step(&g_in));
    kc_in_step(&g_in, s - 0x7FFFFFFFu, kc_sonraki_t());
    TEST_ASSERT_EQUAL_INT(IDA_RC_OLD_SEQ, kc_step(&g_in));
    kc_in_step(&g_in, s + 0x80000000u, kc_sonraki_t());
    TEST_ASSERT_EQUAL_INT_MESSAGE(IDA_RC_OLD_SEQ, kc_step(&g_in), "fark 2^31 eskidir");
    kc_in_step(&g_in, s + 0x7FFFFFFFu, kc_sonraki_t());
    TEST_ASSERT_EQUAL_INT_MESSAGE(IDA_RC_OK, kc_step(&g_in), "fark 2^31 − 1 yenidir");
    TEST_ASSERT_EQUAL_UINT32(s + 0x7FFFFFFFu, kc_out()->seq);
}

static void test_A4_K25_d_ilk_zaman_cercevesi_ve_sarma(void)
{
    kc_init_bloksuz();
    kc_in_step(&g_in, 0xFFFFFFFEu, KC_T0 + KC_P);
    TEST_ASSERT_EQUAL_INT_MESSAGE(IDA_RC_OK, kc_step(&g_in), "ilk zaman çerçevesi her seq ile kabul");
    kc_in_step(&g_in, 0xFFFFFFFFu, KC_T0 + 2 * KC_P);
    TEST_ASSERT_EQUAL_INT(IDA_RC_OK, kc_step(&g_in));
    kc_in_step(&g_in, 0u, KC_T0 + 3 * KC_P);
    TEST_ASSERT_EQUAL_INT_MESSAGE(IDA_RC_OK, kc_step(&g_in), "0xFFFFFFFF -> 0 seri aritmetikle yeni");
    kc_in_tick(&g_in, 0xFFFFFFFFu, KC_T0 + 4 * KC_P);
    TEST_ASSERT_EQUAL_INT(IDA_RC_OLD_SEQ, kc_tick(&g_in));
}

static void test_A4_K25_d_tick(void)
{
    kc_hazirla(3);
    kc_in_tick(&g_in, kc_son_seq(), kc_sonraki_t());
    TEST_ASSERT_EQUAL_INT(IDA_RC_DUP_SEQ, kc_tick(&g_in));
    /* tf.present = 0: tf alanları okunmaz, DUP/OLD dönmez. */
    kc_in_izgara(&g_in, kc_sonraki_t());
    g_in.tf.seq = kc_son_seq();
    g_in.tf.t.ns = -7;
    g_in.tf.t_rx.ns = IDA_TIME_MAX_NS + 9;
    TEST_ASSERT_EQUAL_INT(IDA_RC_OK, kc_tick(&g_in));
    kc_in_tick(&g_in, kc_sonraki_seq(), kc_sonraki_t());
    TEST_ASSERT_EQUAL_INT(IDA_RC_OK, kc_tick(&g_in));
    kc_in_tick(&g_in, kc_son_seq(), kc_sonraki_t());
    TEST_ASSERT_EQUAL_INT_MESSAGE(IDA_RC_DUP_SEQ, kc_tick(&g_in), "zaman çerçeveli tick seq'i kabul edilmiş olmalı");
}

/* ===================================================================================
 * (e) olağan işleniş; IMU doğrulaması yalnız burada (A4-K103).
 * =================================================================================== */

typedef enum {
    IMU_VALID_YOK,
    IMU_NAN,
    IMU_SONSUZ,
    IMU_INST,
    IMU_TUR,
    IMU_PENCERE, /* window_ns = 0 (A4-K33 tür kuralı, A4-K103) */
    IMU_SEQ_YINELENEN,
    IMU_T_ARTMADI,
    IMU_T_GELECEK_TF,
    IMU_T_RX_GELECEK,
    IMU_T_ARALIK,
    IMU_VAKA_SAYISI
} imu_vaka_t;

static void test_A4_K103_e_imu_reddi_her_kosul(void)
{
    kc_hazirla(3);
    for (int v = 0; v < (int)IMU_VAKA_SAYISI; ++v) {
        kc_in_step(&g_in, kc_sonraki_seq(), kc_sonraki_t());
        (void)kc_ornek_ekle(&g_in, (uint8_t)IDA_SENS_MAG, 1u, 1000u + (uint32_t)v, g_in.tf.t.ns);
        ida_hdr_t *h = &g_in.imu.hdr;
        bool gelecek = false;
        switch ((imu_vaka_t)v) {
        case IMU_VALID_YOK:
            h->flags = (uint16_t)IDA_HF_SATURATED;
            break;
        case IMU_NAN:
            g_in.imu.imu.gyro[1] = nan_f();
            break;
        case IMU_SONSUZ:
            g_in.imu.imu.temp_c = sonsuz_f();
            break;
        case IMU_INST:
            h->inst = 4u;
            break;
        case IMU_PENCERE:
            h->window_ns = 0u;
            break;
        case IMU_TUR:
            h->type = (uint8_t)IDA_SENS_MAG;
            break;
        case IMU_SEQ_YINELENEN:
            h->seq = kc_son_seq();
            break;
        case IMU_T_ARTMADI:
            h->t_meas.ns = kc_son_t();
            break;
        case IMU_T_GELECEK_TF:
            h->t_meas.ns = g_in.tf.t.ns + 1;
            gelecek = true;
            break;
        case IMU_T_RX_GELECEK:
            h->t_rx.ns = g_in.now_w.ns + 1;
            gelecek = true;
            break;
        case IMU_T_ARALIK:
            h->t_meas.ns = -1;
            break;
        default:
            break;
        }
        TEST_ASSERT_EQUAL_INT_MESSAGE(IDA_RC_IMU_REJECTED, kc_step(&g_in), "IMU reddi");
        const ida_out_t *o = kc_out();
        TEST_ASSERT_TRUE(o->health.imu_rejected);
        TEST_ASSERT_FALSE(o->nav_valid);
        TEST_ASSERT_EQUAL_UINT32(g_in.tf.seq, o->seq);
        TEST_ASSERT_EQUAL_UINT32(1u, KC_FARK(imu_rejected));
        if (gelecek) {
            /* Gelecek damgalı IMU yalnız sample_future'a sayılır (3.8 sayaçların örtüşmesi). */
            TEST_ASSERT_EQUAL_UINT32(1u, KC_FARK(sample_future));
            TEST_ASSERT_EQUAL_UINT32(0u, KC_FARK(sample_rejected));
        } else {
            TEST_ASSERT_EQUAL_UINT32(1u, KC_FARK(sample_rejected));
            TEST_ASSERT_EQUAL_UINT32(0u, KC_FARK(sample_future));
        }
        /* Diğer örnekler kabul edilir (A4-K103): aynı MAG örneği yinelenirse reddedilir. */
        kc_in_step(&g_in2, kc_sonraki_seq(), kc_sonraki_t());
        (void)kc_ornek_ekle(&g_in2, (uint8_t)IDA_SENS_MAG, 1u, 1000u + (uint32_t)v, kc_son_t());
        TEST_ASSERT_EQUAL_INT(IDA_RC_OK, kc_step(&g_in2));
        TEST_ASSERT_EQUAL_UINT32_MESSAGE(1u, KC_FARK(sample_rejected), "IMU_REJECTED çağrısının MAG örneği kabul edilmemiş");
    }
}

static void test_A4_3_11_e_tick_imu_alanini_okumaz(void)
{
    kc_hazirla(3);
    kc_in_tick(&g_in, kc_sonraki_seq(), kc_sonraki_t());
    memset(&g_in.imu, 0xEE, sizeof g_in.imu);
    TEST_ASSERT_EQUAL_INT(IDA_RC_OK, kc_tick(&g_in));
    TEST_ASSERT_FALSE(kc_out()->health.imu_rejected);
}

/* ===================================================================================
 * Arka plan çağrısının değerlendirme sırası (A4-K117).
 * =================================================================================== */

static void test_A4_K117_bg_1_c_null(void)
{
    ida_wtime_t w;
    w.ns = KC_T0;
    TEST_ASSERT_EQUAL_INT(IDA_BG_BAD_INPUT, ida_core_background(NULL, w));
    w.ns = -1;
    TEST_ASSERT_EQUAL_INT(IDA_BG_BAD_INPUT, ida_core_background(NULL, w));
}

static void test_A4_K117_bg_2_kurulmamis_once_now_w(void)
{
    /* Düzenek kurulmamış örnekte belleğin değişmediğini de denetler. */
    TEST_ASSERT_EQUAL_INT(IDA_BG_IDLE, kc_bg(KC_T0));
    TEST_ASSERT_EQUAL_INT(IDA_BG_IDLE, kc_bg(-1));
    TEST_ASSERT_EQUAL_INT(IDA_BG_IDLE, kc_bg(IDA_TIME_MAX_NS + 1));
    ida_init_in_t ii;
    kc_hazirla(1);
    kc_init_girdi(&ii);
    ii.port_minor = 77u;
    TEST_ASSERT_EQUAL_INT(IDA_RC_VERSION, kc_init(&ii));
    TEST_ASSERT_EQUAL_INT(IDA_BG_IDLE, kc_bg(-1));
}

static void test_A4_K117_bg_3_now_w_araligi(void)
{
    kc_hazirla(2);
    TEST_ASSERT_EQUAL_INT(IDA_BG_BAD_INPUT, kc_bg(-1));
    TEST_ASSERT_EQUAL_INT(IDA_BG_BAD_INPUT, kc_bg(IDA_TIME_MAX_NS + 1));
    /* bg_slices bu çağrıları saymaz (düzenek 0 verir): uyuşmazlık yok. */
    TEST_ASSERT_EQUAL_INT(IDA_RC_OK, kc_adim());
    TEST_ASSERT_TRUE(kc_out()->health.bg_bad_input);
    TEST_ASSERT_EQUAL_UINT32(2u, KC_FARK(bg_bad_input));
    TEST_ASSERT_EQUAL_UINT32(0u, KC_FARK(bg_slice_mismatch));
    const ida_log_t *l = kc_gunluk_bul(kc_out(), (uint16_t)IDA_LOG_BG_BAD_INPUT);
    TEST_ASSERT_NOT_NULL(l);
    TEST_ASSERT_EQUAL_INT32(2, l->arg0);
    TEST_ASSERT_EQUAL_INT(IDA_RC_OK, kc_adim());
    TEST_ASSERT_FALSE(kc_out()->health.bg_bad_input);
    /* Platform geçersiz çağrıyı yanlışlıkla sayarsa uyuşmazlık sayılır. */
    TEST_ASSERT_EQUAL_INT(IDA_BG_BAD_INPUT, kc_bg(-1));
    kc_in_step(&g_in, kc_sonraki_seq(), kc_sonraki_t());
    kc_bg_slices_elle(1u);
    TEST_ASSERT_EQUAL_INT(IDA_RC_OK, kc_step(&g_in));
    TEST_ASSERT_EQUAL_UINT32(1u, KC_FARK(bg_slice_mismatch));
}

/* A4-K117 §7: geçersiz arka plan çağrısı iş durumunu değiştirmez — aynı dizinin bu
   çağrılarla ve onlarsız koşusu, tanılama alanları dışında aynı çıktıyı verir. */
#define KARS_ADIM 12
static ida_out_t g_kars[2][KARS_ADIM];
static ida_bg_rc_t g_kars_bg[2][KARS_ADIM];

static void kars_normallestir(ida_out_t *o)
{
    ida_out_t t = *o;
    t.status.cnt.bg_bad_input = 0u;
    t.status.cnt.log_dropped = 0u;
    t.health.bg_bad_input = false;
    unsigned k = 0;
    memset(t.logs, 0, sizeof t.logs);
    for (unsigned i = 0; i < o->n_logs; ++i) {
        if (o->logs[i].code != IDA_LOG_BG_BAD_INPUT) {
            t.logs[k++] = o->logs[i];
        }
    }
    t.n_logs = (uint8_t)k;
    *o = t;
}

static void test_A4_K117_bg_3_is_durumu_degismez(void)
{
    for (int kosu = 0; kosu < 2; ++kosu) {
        kc_ortam_kur(2);
        kc_hazirla(1);
        for (int a = 0; a < KARS_ADIM; ++a) {
            if (kosu == 0 && (a % 3) == 1) {
                TEST_ASSERT_EQUAL_INT(IDA_BG_BAD_INPUT, kc_bg(a % 2 == 0 ? -1 : IDA_TIME_MAX_NS + 1));
            }
            g_kars_bg[kosu][a] = kc_bg(kc_son_now() + KC_MS);
            TEST_ASSERT_EQUAL_INT(IDA_RC_OK, kc_adim());
            g_kars[kosu][a] = *kc_out();
            kars_normallestir(&g_kars[kosu][a]);
        }
    }
    for (int a = 0; a < KARS_ADIM; ++a) {
        TEST_ASSERT_EQUAL_INT(g_kars_bg[0][a], g_kars_bg[1][a]);
        TEST_ASSERT_TRUE_MESSAGE(kc_out_esit(&g_kars[0][a], &g_kars[1][a]),
                                 "geçersiz arka plan çağrısı çıktıyı değiştirdi (A4-K117)");
    }
}

static void test_A4_K109_bg_slices_karsilastirmasi(void)
{
    kc_hazirla(2);
    for (int k = 0; k < 3; ++k) {
        TEST_ASSERT_TRUE(kc_bg(kc_son_now()) != IDA_BG_BAD_INPUT);
    }
    TEST_ASSERT_EQUAL_INT(IDA_RC_OK, kc_adim()); /* bg_slices = 3 */
    TEST_ASSERT_EQUAL_UINT32(0u, KC_FARK(bg_slice_mismatch));
    TEST_ASSERT_TRUE(kc_bg(kc_son_now()) != IDA_BG_BAD_INPUT);
    TEST_ASSERT_TRUE(kc_bg(kc_son_now()) != IDA_BG_BAD_INPUT);
    kc_in_tick(&g_in, kc_sonraki_seq(), kc_sonraki_t());
    kc_bg_slices_elle(5u);
    TEST_ASSERT_EQUAL_INT(IDA_RC_OK, kc_tick(&g_in));
    TEST_ASSERT_EQUAL_UINT32(1u, KC_FARK(bg_slice_mismatch));
    /* Reddedilen (yazılabilir) çağrıda da karşılaştırılır. */
    kc_in_step(&g_in, kc_son_seq(), kc_sonraki_t());
    kc_bg_slices_elle(2u);
    TEST_ASSERT_EQUAL_INT(IDA_RC_DUP_SEQ, kc_step(&g_in));
    TEST_ASSERT_EQUAL_UINT32(1u, KC_FARK(bg_slice_mismatch));
}

/* ===================================================================================
 * A4 fark turunda kesinleşenler (eski belirsiz B1, B2, B3, B6, B8, B18, B19) ve
 * (b2)/(b3)'ün yeni maddeleri.
 * =================================================================================== */

/* A4-K10 (B1): geri adım izinin tabanı init'in now_w'sidir. */
static void test_A4_K10_init_now_w_geri_adim_tabani(void)
{
    ida_init_in_t ii;
    kc_init_girdi(&ii);
    ii.now_w.ns = KC_T0 + 50 * KC_MS;
    TEST_ASSERT_EQUAL_INT(IDA_RC_PARAM_DEFAULTS, kc_init(&ii));
    kc_in_step(&g_in, 1u, KC_T0 + 10 * KC_MS);
    TEST_ASSERT_EQUAL_INT(IDA_RC_OK, kc_step(&g_in));
    TEST_ASSERT_EQUAL_UINT32(1u, kc_out()->status.cnt.wall_backstep);
    const ida_log_t *l = kc_gunluk_bul(kc_out(), (uint16_t)IDA_LOG_WALL_BACKSTEP);
    TEST_ASSERT_NOT_NULL(l);
    TEST_ASSERT_EQUAL_INT32(40000, l->arg0);
    TEST_ASSERT_TRUE_MESSAGE(l->t_w.ns == KC_T0 + 50 * KC_MS, "t_w kullanılan now_w (init tabanı)");
}

/* A4-K10 (B6): aralık dışı now_w geri adım sayılmaz; iz, sayaç ve günlük değişmez. */
static void test_A4_K10_aralik_disi_now_w_geri_adim_sayilmaz(void)
{
    kc_hazirla(2); /* iz: T0 + 10 ms */
    kc_in_izgara(&g_in, -1);
    TEST_ASSERT_EQUAL_INT(IDA_RC_BAD_INPUT, kc_tick(&g_in));
    TEST_ASSERT_EQUAL_UINT32(0u, KC_FARK(wall_backstep));
    TEST_ASSERT_EQUAL_INT(0, kc_gunluk_say(kc_out(), (uint16_t)IDA_LOG_WALL_BACKSTEP));
    kc_in_izgara(&g_in, IDA_TIME_MAX_NS + 1);
    TEST_ASSERT_EQUAL_INT(IDA_RC_BAD_INPUT, kc_tick(&g_in));
    TEST_ASSERT_EQUAL_UINT32(0u, KC_FARK(wall_backstep));
    /* İz değişmedi: T0 + 12 ms geri adım değildir; T0 + 8 ms geri adımdır. */
    kc_in_izgara(&g_in, KC_T0 + 12 * KC_MS);
    TEST_ASSERT_EQUAL_INT(IDA_RC_OK, kc_tick(&g_in));
    TEST_ASSERT_EQUAL_UINT32(0u, KC_FARK(wall_backstep));
    kc_in_izgara(&g_in, KC_T0 + 8 * KC_MS);
    TEST_ASSERT_EQUAL_INT(IDA_RC_OK, kc_tick(&g_in));
    TEST_ASSERT_EQUAL_UINT32(1u, KC_FARK(wall_backstep));
}

/* 3.8 t_w (B2): (b2) çağrısında bekletilen kaydın t_w'si o çağrının kullanılan now_w'si. */
static void test_A4_3_8_bekletilen_gunlugun_t_w(void)
{
    kc_hazirla(2);
    kc_in_izgara(&g_in, KC_T0 + 40 * KC_MS);
    TEST_ASSERT_EQUAL_INT(IDA_RC_OK, kc_tick(&g_in));
    kc_in_izgara(&g_in, KC_T0 + 30 * KC_MS);
    TEST_ASSERT_EQUAL_INT(IDA_RC_BAD_INPUT, kc_cagir_out_null(KC_TICK, &g_in));
    kc_in_izgara(&g_in, KC_T0 + 60 * KC_MS);
    TEST_ASSERT_EQUAL_INT(IDA_RC_OK, kc_tick(&g_in));
    const ida_log_t *l = kc_gunluk_bul(kc_out(), (uint16_t)IDA_LOG_WALL_BACKSTEP);
    TEST_ASSERT_NOT_NULL(l);
    TEST_ASSERT_TRUE_MESSAGE(l->t_w.ns == KC_T0 + 40 * KC_MS, "bekletilen kaydın t_w'si (b2) çağrısının kullanılan now_w'si");
}

/* 3.8 t_w (B3): RESET_INFO ve PARAM_LOAD'da init'in now_w'si. */
static void test_A4_3_8_init_gunluklerinin_t_w(void)
{
    kc_init_bloksuz();
    kc_in_step(&g_in, 1u, KC_T0 + 5 * KC_MS);
    TEST_ASSERT_EQUAL_INT(IDA_RC_OK, kc_step(&g_in));
    const ida_log_t *r = kc_gunluk_bul(kc_out(), (uint16_t)IDA_LOG_RESET_INFO);
    const ida_log_t *p = kc_gunluk_bul(kc_out(), (uint16_t)IDA_LOG_PARAM_LOAD);
    TEST_ASSERT_NOT_NULL(r);
    TEST_ASSERT_NOT_NULL(p);
    TEST_ASSERT_TRUE(r->t_w.ns == KC_T0);
    TEST_ASSERT_TRUE(p->t_w.ns == KC_T0);
}

/* A4-K33 (B8 ve tür kuralı): IMU, basınç ve batarya window_ns > 0; diğerleri 0. */
static void test_A4_K33_window_ns_tur_kurali(void)
{
    kc_hazirla(2);
    int64_t t = kc_sonraki_t();
    kc_in_step(&g_in, kc_sonraki_seq(), t);
    kc_ornek_ekle(&g_in, (uint8_t)IDA_SENS_MAG, 1u, 1u, t)->hdr.window_ns = (uint32_t)KC_P;
    kc_ornek_ekle(&g_in, (uint8_t)IDA_SENS_BARO, 1u, 1u, t)->hdr.window_ns = 0u;
    ida_sample_t *b = kc_ornek_ekle(&g_in, (uint8_t)IDA_SENS_BATT, 1u, 1u, t);
    b->hdr.window_ns = 0u;
    b->u.batt.voltage = 16.0f;
    TEST_ASSERT_EQUAL_INT(IDA_RC_OK, kc_step(&g_in));
    TEST_ASSERT_EQUAL_UINT32(3u, KC_FARK(sample_rejected));
    /* Kurala uyan aynı örnekler kabul (ilk örnek muafiyetiyle). */
    t = kc_sonraki_t();
    kc_in_step(&g_in, kc_sonraki_seq(), t);
    (void)kc_ornek_ekle(&g_in, (uint8_t)IDA_SENS_MAG, 1u, 1u, t);
    (void)kc_ornek_ekle(&g_in, (uint8_t)IDA_SENS_BARO, 1u, 1u, t);
    b = kc_ornek_ekle(&g_in, (uint8_t)IDA_SENS_BATT, 1u, 1u, t);
    b->hdr.window_ns = (uint32_t)(100 * KC_MS);
    b->u.batt.voltage = 16.0f;
    TEST_ASSERT_EQUAL_INT(IDA_RC_OK, kc_step(&g_in));
    TEST_ASSERT_EQUAL_UINT32(0u, KC_FARK(sample_rejected));
}

/* A4-K122 (B19): oturum sonu çağrısındaki örnekler ve komutlar olağan kurallarla işlenir. */
static void test_A4_K122_oturum_sonu_cagrisinin_ornek_ve_komutlari(void)
{
    kc_hazirla(2);
    int64_t t = kc_son_now();
    kc_in_oturum_sonu(&g_in, t + KC_P);
    (void)kc_ornek_ekle(&g_in, (uint8_t)IDA_SENS_MAG, 1u, 5u, t);
    (void)kc_komut_ekle(&g_in, 7770u, (uint8_t)IDA_CMD_HEARTBEAT);
    TEST_ASSERT_EQUAL_INT(IDA_RC_OK, kc_tick(&g_in));
    TEST_ASSERT_EQUAL_UINT8(1u, kc_out()->n_results);
    kc_in_step(&g_in, kc_sonraki_seq(), kc_sonraki_t());
    (void)kc_ornek_ekle(&g_in, (uint8_t)IDA_SENS_MAG, 1u, 5u, t);
    (void)kc_komut_ekle(&g_in, 7770u, (uint8_t)IDA_CMD_HEARTBEAT);
    TEST_ASSERT_EQUAL_INT(IDA_RC_OK, kc_step(&g_in));
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(1u, KC_FARK(sample_rejected), "oturum sonu çağrısının örneği kabul edilmiş olmalı");
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(IDA_ACK_DUPLICATE, kc_out()->results[0].ack,
                                    "oturum sonu çağrısının komutu pencereye girmiş olmalı");
}

/* 3.8 step_miss, 3.11 (b2): init'ten sonraki ilk step/tick out NULL ise taban orada kurulur. */
static void test_A4_3_8_step_miss_tabani_out_null_cagrisinda(void)
{
    kc_init_bloksuz();
    kc_in_step(&g_in, 1u, KC_T0 + KC_P);
    g_in.plat.prev_overrun = true;
    g_in.plat.cnt_batch_skipped = 5u;
    g_in.plat.cnt_batch_merged = 3u;
    TEST_ASSERT_EQUAL_INT(IDA_RC_BAD_INPUT, kc_cagir_out_null(KC_STEP, &g_in));
    kc_in_step(&g_in, 1u, KC_T0 + 2 * KC_P);
    g_in.plat.cnt_batch_skipped = 7u;
    g_in.plat.cnt_batch_merged = 3u;
    TEST_ASSERT_EQUAL_INT(IDA_RC_OK, kc_step(&g_in));
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(2u, kc_out()->status.cnt.step_miss,
                                     "taban out NULL çağrısında kurulmalı (skipped 5 -> 7)");
    /* (b2): out NULL çağrısında görülen prev_overrun step_miss'e sayılmaz; atlanan parti
       artışı (7 -> 8) sayılır. */
    kc_in_step(&g_in, 2u, KC_T0 + 3 * KC_P);
    g_in.plat.prev_overrun = true;
    g_in.plat.cnt_batch_skipped = 7u;
    g_in.plat.cnt_batch_merged = 3u;
    TEST_ASSERT_EQUAL_INT(IDA_RC_BAD_INPUT, kc_cagir_out_null(KC_STEP, &g_in));
    kc_in_step(&g_in, 2u, KC_T0 + 4 * KC_P);
    g_in.plat.cnt_batch_skipped = 8u;
    g_in.plat.cnt_batch_merged = 3u;
    TEST_ASSERT_EQUAL_INT(IDA_RC_OK, kc_step(&g_in));
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(3u, kc_out()->status.cnt.step_miss,
                                     "out NULL çağrısının prev_overrun'u sayılmamalı (3.11 (b2))");
}

/* A4-K10, 3.8 t_w (eski belirsiz R2): aralık dışı now_w taşıyan çağrıda kullanılan now_w
   geri adım izinin değeridir; o çağrının günlüklerinin t_w'si budur. */
static void test_A4_K10_aralik_disi_now_w_cagrisinin_t_w(void)
{
    kc_hazirla(2); /* iz: T0 + 10 ms */
    kc_in_izgara(&g_in, -1);
    g_in.plat.link_event = (uint8_t)IDA_LINKEV_SESSION_START;
    TEST_ASSERT_EQUAL_INT(IDA_RC_BAD_INPUT, kc_tick(&g_in));
    const ida_log_t *l = kc_gunluk_bul(kc_out(), (uint16_t)IDA_LOG_SESSION);
    TEST_ASSERT_NOT_NULL(l);
    TEST_ASSERT_TRUE_MESSAGE(l->t_w.ns == KC_T0 + 2 * KC_P, "t_w = geri adım izinin değeri (T0 + 10 ms)");
    /* out NULL çağrısında bekletilen kayıt için de aynı değer. */
    kc_in_izgara(&g_in, IDA_TIME_MAX_NS + 1);
    g_in.plat.link_event = (uint8_t)IDA_LINKEV_SESSION_START;
    TEST_ASSERT_EQUAL_INT(IDA_RC_BAD_INPUT, kc_cagir_out_null(KC_TICK, &g_in));
    kc_in_izgara(&g_in, KC_T0 + 40 * KC_MS);
    TEST_ASSERT_EQUAL_INT(IDA_RC_OK, kc_tick(&g_in));
    l = kc_gunluk_bul(kc_out(), (uint16_t)IDA_LOG_SESSION);
    TEST_ASSERT_NOT_NULL(l);
    TEST_ASSERT_TRUE_MESSAGE(l->t_w.ns == KC_T0 + 2 * KC_P, "bekletilen kaydın t_w'si (b2) çağrısının iz değeri");
}

/* A4-K39 (B18): init durumu için IMU_STALE = 1 günlüğü yok; her bayat -> taze geçişinde 0;
   taze -> bayat geçişinde 1; IMU_STALE engel biti ilk çağrıdan kurulu. */
static void test_A4_K39_imu_stale_gunlukleri(void)
{
    kc_init_bloksuz();
    kc_in_izgara(&g_in, KC_T0 + KC_P);
    TEST_ASSERT_EQUAL_INT(IDA_RC_OK, kc_tick(&g_in));
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, kc_gunluk_say(kc_out(), (uint16_t)IDA_LOG_IMU_STALE), "init durumu için günlük yok");
    TEST_ASSERT_TRUE((kc_out()->fc.inhibit_mask & IDA_FCI_IMU_STALE) != 0u);
    int64_t t = KC_T0 + 2 * KC_P;
    kc_in_step(&g_in, 1u, t);
    TEST_ASSERT_EQUAL_INT(IDA_RC_OK, kc_step(&g_in));
    const ida_log_t *l = kc_gunluk_bul(kc_out(), (uint16_t)IDA_LOG_IMU_STALE);
    TEST_ASSERT_NOT_NULL_MESSAGE(l, "ilk IMU kabulünde IMU_STALE = 0");
    TEST_ASSERT_EQUAL_INT32(0, l->arg0);
    /* T_imu_stale = 20 ms (varsayılan): t + 20 ms'de taze, t + 25 ms'de bayat. */
    for (int64_t k = 1; k <= 4; ++k) {
        kc_in_izgara(&g_in, t + k * KC_P);
        TEST_ASSERT_EQUAL_INT(IDA_RC_OK, kc_tick(&g_in));
        TEST_ASSERT_EQUAL_INT(0, kc_gunluk_say(kc_out(), (uint16_t)IDA_LOG_IMU_STALE));
    }
    kc_in_izgara(&g_in, t + 5 * KC_P);
    TEST_ASSERT_EQUAL_INT(IDA_RC_OK, kc_tick(&g_in));
    l = kc_gunluk_bul(kc_out(), (uint16_t)IDA_LOG_IMU_STALE);
    TEST_ASSERT_NOT_NULL_MESSAGE(l, "taze -> bayat geçişinde IMU_STALE = 1");
    TEST_ASSERT_EQUAL_INT32(1, l->arg0);
    kc_in_step(&g_in, 2u, t + 6 * KC_P);
    TEST_ASSERT_EQUAL_INT(IDA_RC_OK, kc_step(&g_in));
    l = kc_gunluk_bul(kc_out(), (uint16_t)IDA_LOG_IMU_STALE);
    TEST_ASSERT_NOT_NULL_MESSAGE(l, "bayat -> taze geçişinde IMU_STALE = 0");
    TEST_ASSERT_EQUAL_INT32(0, l->arg0);
}

/* 3.11 (b3) (ii): IMU taze idiyse SESSION_START taze -> bayat geçişidir: IMU_STALE = 1. */
static void test_A4_3_11_b3_imu_stale_gecisi(void)
{
    kc_hazirla(3);
    kc_in_izgara(&g_in, kc_sonraki_t());
    g_in.plat.link_event = (uint8_t)IDA_LINKEV_SESSION_START;
    TEST_ASSERT_EQUAL_INT(IDA_RC_OK, kc_tick(&g_in));
    const ida_log_t *l = kc_gunluk_bul(kc_out(), (uint16_t)IDA_LOG_IMU_STALE);
    TEST_ASSERT_NOT_NULL_MESSAGE(l, "SESSION_START'ta IMU_STALE = 1 günlüğü");
    TEST_ASSERT_EQUAL_INT32(1, l->arg0);
    TEST_ASSERT_TRUE((kc_out()->fc.inhibit_mask & IDA_FCI_IMU_STALE) != 0u);
    TEST_ASSERT_EQUAL_INT(IDA_RC_OK, kc_adim());
    l = kc_gunluk_bul(kc_out(), (uint16_t)IDA_LOG_IMU_STALE);
    TEST_ASSERT_NOT_NULL(l);
    TEST_ASSERT_EQUAL_INT32(0, l->arg0);
}

/* 3.11 (b3) (ii), A4-K37: SESSION_START IMU_LAG mandalını çözmez, çözülme sayacını sıfırlar.
   T_lag_set 20 ms, T_lag_clear 10 ms, N_lag_clear 20 (varsayılanlar). Bir çağrının kendi
   IMU'sunun lag hesabına girip girmediğinden bağımsız sayılar seçildi. */
static void lag_adimi(uint32_t seq, int64_t t, int64_t lag)
{
    kc_in_step(&g_in, seq, t);
    g_in.imu.hdr.t_meas.ns = t - lag;
    TEST_ASSERT_EQUAL_INT(IDA_RC_OK, kc_step(&g_in));
}

static void test_A4_3_11_b3_imu_lag_mandali_ve_cozulme_sayaci(void)
{
    kc_init_bloksuz();
    int64_t t = KC_T0 + 100 * KC_MS;
    uint32_t s = 1u;
    for (int k = 0; k < 3; ++k, ++s, t += KC_P) {
        lag_adimi(s, t, 25 * KC_MS);
    }
    TEST_ASSERT_TRUE_MESSAGE(kc_out()->health.imu_lag, "lag 25 ms: mandal kurulu (A4-K37)");
    for (int k = 0; k < 10; ++k, ++s, t += KC_P) {
        lag_adimi(s, t, 0);
    }
    TEST_ASSERT_TRUE(kc_out()->health.imu_lag);
    kc_in_izgara(&g_in, t);
    g_in.plat.link_event = (uint8_t)IDA_LINKEV_SESSION_START;
    TEST_ASSERT_EQUAL_INT(IDA_RC_OK, kc_tick(&g_in));
    TEST_ASSERT_TRUE_MESSAGE(kc_out()->health.imu_lag, "SESSION_START mandalı çözmez");
    t += KC_P;
    for (int k = 0; k < 19; ++k, ++s, t += KC_P) {
        lag_adimi(s, t, 0);
        TEST_ASSERT_TRUE_MESSAGE(kc_out()->health.imu_lag, "çözülme sayacı SESSION_START'ta sıfırlanmalı");
    }
    for (int k = 0; k < 2; ++k, ++s, t += KC_P) {
        lag_adimi(s, t, 0);
    }
    TEST_ASSERT_FALSE_MESSAGE(kc_out()->health.imu_lag, "N_lag_clear art arda çağrıdan sonra çözülür");
}

/* 3.11 (b3) kapalı liste: yineleme penceresi, sonuç bekleyen olaylar, step_miss tabanı,
   sayaçlar ve etkin emniyet değerleri sıfırlanmaz. */
static void test_A4_3_11_b3_sifirlanmayanlar(void)
{
    kc_hazirla(3);
    kc_in_step(&g_in, kc_sonraki_seq(), kc_sonraki_t());
    (void)kc_komut_ekle(&g_in, 4242u, (uint8_t)IDA_CMD_HEARTBEAT);
    TEST_ASSERT_EQUAL_INT(IDA_RC_OK, kc_step(&g_in));
    kc_in_oturum_sonu(&g_in, kc_sonraki_t());
    TEST_ASSERT_EQUAL_INT(IDA_RC_OK, kc_tick(&g_in));
    TEST_ASSERT_EQUAL_UINT8(1u, kc_out()->n_events);
    uint64_t id = kc_out()->events[0].event_id;
    uint32_t dup0 = kc_out()->status.cnt.dup_seq;
    kc_in_izgara(&g_in, kc_sonraki_t());
    g_in.plat.link_event = (uint8_t)IDA_LINKEV_SESSION_START;
    TEST_ASSERT_EQUAL_INT(IDA_RC_OK, kc_tick(&g_in));
    kc_in_step(&g_in, kc_sonraki_seq(), kc_sonraki_t());
    (void)kc_komut_ekle(&g_in, 4242u, (uint8_t)IDA_CMD_HEARTBEAT);
    g_in.n_evres = 1u;
    g_in.evres[0].event_id = id;
    g_in.evres[0].result = (uint8_t)IDA_EVR_APPLIED;
    g_in.evres[0].t_apply.ns = g_in.tf.t.ns;
    g_in.plat.cnt_batch_skipped = 3u;
    TEST_ASSERT_EQUAL_INT(IDA_RC_OK, kc_step(&g_in));
    const ida_out_t *o = kc_out();
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(IDA_ACK_DUPLICATE, o->results[0].ack, "yineleme penceresi sıfırlanmaz");
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(0u, KC_FARK(evres_ignored), "sonuç bekleyen olay sıfırlanmaz");
    TEST_ASSERT_EQUAL_UINT8(IDA_EVR_APPLIED, o->fc.last_event_result);
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(3u, KC_FARK(step_miss), "step_miss tabanı sıfırlanmaz");
    TEST_ASSERT_TRUE_MESSAGE(o->status.cnt.dup_seq == dup0, "sayaçlar sıfırlanmaz");
    TEST_ASSERT_TRUE((o->fc.inhibit_mask & IDA_FCI_ESTOP) != 0u);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_A4_3_11_init_1_c_null);
    RUN_TEST(test_A4_3_11_init_2_in_null_kurulmamis_isaretler);
    RUN_TEST(test_A4_K02_init_3_surum_uyusmazligi);
    RUN_TEST(test_A4_3_11_init_3_surum_now_w_ve_bloktan_once);
    RUN_TEST(test_A4_K08_init_4_now_w_araligi);
    RUN_TEST(test_A4_K101_init_4_blob_null_param_len);
    RUN_TEST(test_A4_3_11_init_4_bloktan_once);
    RUN_TEST(test_A4_K78_init_5_varsayilanlar_ornek_kurulur);
    RUN_TEST(test_A4_3_11_init_6_ok);
    RUN_TEST(test_A4_K77_reset_cause_davranisi_degistirmez);
    RUN_TEST(test_A4_K113_a1_c_null_her_kosulla);
    RUN_TEST(test_A4_K113_a1_in_null_hicbir_sey_yazilmaz);
    RUN_TEST(test_A4_K113_a2_kurulmamis_not_init);
    RUN_TEST(test_A4_3_11_oncelik_NOT_INIT_her_kosuldan_once);
    RUN_TEST(test_A4_K72_b3_session_start_ayni_seq_kabul);
    RUN_TEST(test_A4_K15_b3_session_start_zaman_geri);
    RUN_TEST(test_A4_K57_b3_reddedilen_cagrida_son_gecerli_sifir);
    RUN_TEST(test_A4_K72_b3_out_null_cagrisinda_iz_sifirlanir);
    RUN_TEST(test_A4_K72_b3_ornek_kabul_durumu_sifirlanir);
    RUN_TEST(test_A4_3_11_b2_baska_hicbir_sey_yapilmaz);
    RUN_TEST(test_A4_3_11_b2_once_d_ve_e);
    RUN_TEST(test_A4_3_11_b2_oturum_gunlukleri_bekletilir);
    RUN_TEST(test_A4_K10_b2_now_izi_guncellenir);
    RUN_TEST(test_A4_K109_b2_bg_sayimi_karsilastirmadan_sifirlanir);
    RUN_TEST(test_A4_K117_b2_bg_bad_input_bayragi_tasinir);
    RUN_TEST(test_A4_K08_c_now_w_araligi);
    RUN_TEST(test_A4_K57_c_tf_t_araligi_ve_son_gecerli);
    RUN_TEST(test_A4_K14_c_tf_t_rx);
    RUN_TEST(test_A4_K10_c_t_rx_kullanilan_now_ile_karsilastirilir);
    RUN_TEST(test_A4_K15_c_seq_artti_t_artmadi);
    RUN_TEST(test_A4_K17_c_imu_present);
    RUN_TEST(test_A4_K31_c_kapasite_asimi);
    RUN_TEST(test_A4_K46_c_mandal_alani);
    RUN_TEST(test_A4_K46_c_cmds_set_mode_gecersiz_hedef);
    RUN_TEST(test_A4_3_11_c_link_alanlari);
    RUN_TEST(test_A4_K122_c_oturum_sonu_alan_kisitlari_denetlenmez);
    RUN_TEST(test_A4_3_11_oncelik_BAD_INPUT_x_DUP_OLD_IMU);
    RUN_TEST(test_A4_3_11_oncelik_DUP_OLD_x_IMU_REJECTED);
    RUN_TEST(test_A4_K25_d_dup_old_ve_pencere_siniri);
    RUN_TEST(test_A4_K25_d_ilk_zaman_cercevesi_ve_sarma);
    RUN_TEST(test_A4_K25_d_tick);
    RUN_TEST(test_A4_K103_e_imu_reddi_her_kosul);
    RUN_TEST(test_A4_3_11_e_tick_imu_alanini_okumaz);
    RUN_TEST(test_A4_K117_bg_1_c_null);
    RUN_TEST(test_A4_K117_bg_2_kurulmamis_once_now_w);
    RUN_TEST(test_A4_K117_bg_3_now_w_araligi);
    RUN_TEST(test_A4_K117_bg_3_is_durumu_degismez);
    RUN_TEST(test_A4_K109_bg_slices_karsilastirmasi);
    RUN_TEST(test_A4_K10_init_now_w_geri_adim_tabani);
    RUN_TEST(test_A4_K10_aralik_disi_now_w_geri_adim_sayilmaz);
    RUN_TEST(test_A4_3_8_bekletilen_gunlugun_t_w);
    RUN_TEST(test_A4_3_8_init_gunluklerinin_t_w);
    RUN_TEST(test_A4_K33_window_ns_tur_kurali);
    RUN_TEST(test_A4_K122_oturum_sonu_cagrisinin_ornek_ve_komutlari);
    RUN_TEST(test_A4_3_8_step_miss_tabani_out_null_cagrisinda);
    RUN_TEST(test_A4_K10_aralik_disi_now_w_cagrisinin_t_w);
    RUN_TEST(test_A4_K39_imu_stale_gunlukleri);
    RUN_TEST(test_A4_3_11_b3_imu_stale_gecisi);
    RUN_TEST(test_A4_3_11_b3_imu_lag_mandali_ve_cozulme_sayaci);
    RUN_TEST(test_A4_3_11_b3_sifirlanmayanlar);
    return UNITY_END();
}
