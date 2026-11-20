/*
 * A4 §4.15 — işlevlerin işaretçi önkoşulları, dönüşleri ve hata halinde çıktı (A4-K101):
 * her işlevin her NULL satırı; "yazılmaz" denen yerlerde hedef belleğin değişmediği.
 * Ayrıca A4-K85 (param_get örneği değiştirmez), A4-K40 (ping360 NULL olabilir).
 */
#include <string.h>

#include "kabul_cekirdek.h"
#include "unity.h"

static ida_in_t g_in;
static ida_out_t g_o;
static ida_out_t g_desen;
static uint8_t g_kopya[1u << 20];
static ida_ping360_t g_ping;

void setUp(void)
{
    kc_ortam_kur(KC_N_AZAMI);
}

void tearDown(void)
{
}

/* ---- ida_core_info ---------------------------------------------------------------- */

static void test_A4_4_15_info_null_ve_her_alan(void)
{
    ida_core_info_t a;
    ida_core_info_t b;
    ida_core_info(NULL); /* hiçbir şey yapmaz; çökmez */
    memset(&a, 0x00, sizeof a);
    memset(&b, 0xA7, sizeof b);
    ida_core_info(&a);
    ida_core_info(&b);
    TEST_ASSERT_EQUAL_UINT16(IDA_PORT_VERSION_MAJOR, a.port_major);
    TEST_ASSERT_EQUAL_UINT16(IDA_PORT_VERSION_MINOR, a.port_minor);
    TEST_ASSERT_TRUE(a.core_size > 0u);
    TEST_ASSERT_TRUE(a.core_align > 0u && (a.core_align & (a.core_align - 1u)) == 0u);
    /* Her alan yazıldı: iki farklı desenden aynı değerler (A4-K98). */
    TEST_ASSERT_EQUAL_UINT16(a.port_major, b.port_major);
    TEST_ASSERT_EQUAL_UINT16(a.port_minor, b.port_minor);
    TEST_ASSERT_EQUAL_UINT32(a.core_size, b.core_size);
    TEST_ASSERT_EQUAL_UINT32(a.core_align, b.core_align);
    TEST_ASSERT_TRUE(a.param_schema_hash == b.param_schema_hash);
    TEST_ASSERT_EQUAL_UINT32(a.core_version, b.core_version);
}

/* ---- ida_core_init ---------------------------------------------------------------- */

static void test_A4_4_15_init_null_satirlari(void)
{
    ida_init_in_t ii;
    kc_init_girdi(&ii);
    /* c NULL: hiçbir yere yazmaz, BAD_INPUT (girdi değişmez). */
    ida_init_in_t kopya = ii;
    TEST_ASSERT_EQUAL_INT(IDA_RC_BAD_INPUT, ida_core_init(NULL, &ii));
    TEST_ASSERT_EQUAL_MEMORY(&kopya, &ii, sizeof ii);
    /* in NULL: örneği kurulmamış işaretler. */
    kc_hazirla(1);
    TEST_ASSERT_EQUAL_INT(IDA_RC_BAD_INPUT, kc_init(NULL));
    kc_in_izgara(&g_in, KC_T0 + KC_P);
    TEST_ASSERT_EQUAL_INT(IDA_RC_NOT_INIT, kc_tick(&g_in));
    /* param_blob NULL iken param_len > 0. */
    kc_hazirla(1);
    kc_init_girdi(&ii);
    ii.param_len = 24u;
    TEST_ASSERT_EQUAL_INT(IDA_RC_BAD_INPUT, kc_init(&ii));
    kc_in_izgara(&g_in, KC_T0 + KC_P);
    TEST_ASSERT_EQUAL_INT(IDA_RC_NOT_INIT, kc_tick(&g_in));
}

/* ---- ida_core_step / ida_core_tick ------------------------------------------------ */

static void out_degismedi_mi(ida_rc_t rc, ida_rc_t beklenen, const char *ad)
{
    TEST_ASSERT_EQUAL_INT_MESSAGE(beklenen, rc, ad);
    TEST_ASSERT_EQUAL_MEMORY_MESSAGE(&g_desen, &g_o, sizeof g_o, ad);
}

static void step_tick_null_satirlari(bool kurulu)
{
    size_t boy = kc_core_boyu();
    for (int f = 0; f < 2; ++f) {
        kc_in_step(&g_in, kc_sonraki_seq(), kc_sonraki_t());
        if (f == 1) {
            g_in.imu_present = false;
        }
        memcpy(g_kopya, kc_bellek(0), boy);
        memset(&g_o, 0x6B, sizeof g_o);
        g_desen = g_o;
        out_degismedi_mi(f == 0 ? ida_core_step(NULL, &g_in, &g_o) : ida_core_tick(NULL, &g_in, &g_o),
                         IDA_RC_BAD_INPUT, "c NULL");
        out_degismedi_mi(f == 0 ? ida_core_step(kc_core(0), NULL, &g_o) : ida_core_tick(kc_core(0), NULL, &g_o),
                         IDA_RC_BAD_INPUT, "in NULL");
        out_degismedi_mi(f == 0 ? ida_core_step(NULL, NULL, &g_o) : ida_core_tick(NULL, NULL, &g_o),
                         IDA_RC_BAD_INPUT, "c ve in NULL");
        TEST_ASSERT_EQUAL_INT(IDA_RC_BAD_INPUT, f == 0 ? ida_core_step(NULL, NULL, NULL) : ida_core_tick(NULL, NULL, NULL));
        TEST_ASSERT_EQUAL_INT(IDA_RC_BAD_INPUT, f == 0 ? ida_core_step(NULL, &g_in, NULL) : ida_core_tick(NULL, &g_in, NULL));
        TEST_ASSERT_EQUAL_MEMORY_MESSAGE(g_kopya, kc_bellek(0), boy, "c/in NULL çağrısı örneği değiştirdi");
        /* out NULL: kuruluysa BAD_INPUT ((b2)), değilse NOT_INIT ((a2)) — düzenek üzerinden. */
        ida_rc_t rc = kc_cagir_out_null(f == 0 ? KC_STEP : KC_TICK, &g_in);
        TEST_ASSERT_EQUAL_INT(kurulu ? IDA_RC_BAD_INPUT : IDA_RC_NOT_INIT, rc);
    }
}

static void test_A4_4_15_step_tick_null_kurulmamis(void)
{
    step_tick_null_satirlari(false);
}

static void test_A4_4_15_step_tick_null_kurulu(void)
{
    kc_hazirla(2);
    step_tick_null_satirlari(true);
    /* Durum değişmedi: sıradaki olağan adım kabul. */
    TEST_ASSERT_EQUAL_INT(IDA_RC_OK, kc_adim());
}

static void test_A4_K40_ping360_null_olabilir(void)
{
    kc_hazirla(2);
    kc_in_step(&g_in, kc_sonraki_seq(), kc_sonraki_t());
    g_in.ping360 = NULL;
    TEST_ASSERT_EQUAL_INT(IDA_RC_OK, kc_step(&g_in));
    /* Geçersiz satır işaretçisi verilir: satır reddedilir, çağrı sürer. */
    memset(&g_ping, 0, sizeof g_ping);
    g_ping.hdr = g_in.imu.hdr;
    g_ping.hdr.type = (uint8_t)IDA_SENS_PING360;
    g_ping.hdr.window_ns = 0u;
    g_ping.n_samples = 0u; /* 1–1200 dışında */
    kc_in_step(&g_in, kc_sonraki_seq(), kc_sonraki_t());
    g_ping.hdr.t_meas = g_in.tf.t;
    g_ping.hdr.t_rx = g_in.now_w;
    g_in.ping360 = &g_ping;
    TEST_ASSERT_EQUAL_INT(IDA_RC_OK, kc_step(&g_in));
    TEST_ASSERT_EQUAL_UINT32(1u, KC_FARK(ping360_rejected));
}

/* ---- ida_core_background ---------------------------------------------------------- */

static void test_A4_4_15_background_c_null(void)
{
    ida_wtime_t w;
    w.ns = KC_T0;
    TEST_ASSERT_EQUAL_INT(IDA_BG_BAD_INPUT, ida_core_background(NULL, w));
}

/* ---- Parametre işlevleri ---------------------------------------------------------- */

static void test_A4_4_15_param_desc(void)
{
    ida_param_desc_t d;
    ida_param_desc_t desen;
    uint16_t n = ida_core_param_count();
    TEST_ASSERT_TRUE(n > 0u);
    TEST_ASSERT_EQUAL_INT(IDA_RC_BAD_INPUT, ida_core_param_desc(0u, NULL));
    /* 4.15 denetim sırası (B5): d NULL ve index ≥ sayı birlikteyse BAD_INPUT. */
    TEST_ASSERT_EQUAL_INT(IDA_RC_BAD_INPUT, ida_core_param_desc(n, NULL));
    TEST_ASSERT_EQUAL_INT(IDA_RC_BAD_INPUT, ida_core_param_desc(UINT16_MAX, NULL));
    memset(&d, 0x5E, sizeof d);
    desen = d;
    TEST_ASSERT_EQUAL_INT(IDA_RC_NOT_FOUND, ida_core_param_desc(n, &d));
    TEST_ASSERT_EQUAL_MEMORY_MESSAGE(&desen, &d, sizeof d, "NOT_FOUND'da *d yazıldı");
    TEST_ASSERT_EQUAL_INT(IDA_RC_NOT_FOUND, ida_core_param_desc(UINT16_MAX, &d));
    TEST_ASSERT_EQUAL_MEMORY_MESSAGE(&desen, &d, sizeof d, "NOT_FOUND'da *d yazıldı");
    TEST_ASSERT_EQUAL_INT(IDA_RC_OK, ida_core_param_desc((uint16_t)(n - 1u), &d));
}

/* NUL'suz 17 baytlık ad: çekirdek en çok 17 bayt okur (ASan kökünde taşma yakalanır). */
static char g_ad17[17];

static void test_A4_4_15_param_find(void)
{
    uint16_t id;
    uint16_t n = kc_param_sayisi();
    char ad[40];
    TEST_ASSERT_EQUAL_INT(IDA_RC_BAD_INPUT, ida_core_param_find(NULL, &id));
    TEST_ASSERT_EQUAL_INT(IDA_RC_BAD_INPUT, ida_core_param_find(kc_param_desc(0u)->name, NULL));
    TEST_ASSERT_EQUAL_INT(IDA_RC_BAD_INPUT, ida_core_param_find(NULL, NULL));
    for (unsigned iu = 0; iu < (unsigned)(n); ++iu) {
        uint16_t i = (uint16_t)iu;
        const ida_param_desc_t *d = kc_param_desc(i);
        id = 0xBEEFu;
        TEST_ASSERT_EQUAL_INT_MESSAGE(IDA_RC_OK, ida_core_param_find(d->name, &id), d->name);
        TEST_ASSERT_EQUAL_UINT16(d->id, id);
        /* 16 karakterden uzun ad: NOT_FOUND, *id yazılmaz. */
        size_t uz = strlen(d->name);
        memset(ad, 0, sizeof ad);
        memcpy(ad, d->name, uz);
        for (size_t k = uz; k < 17u; ++k) {
            ad[k] = 'x';
        }
        id = 0xBEEFu;
        TEST_ASSERT_EQUAL_INT(IDA_RC_NOT_FOUND, ida_core_param_find(ad, &id));
        TEST_ASSERT_EQUAL_UINT16(0xBEEFu, id);
    }
    id = 0xBEEFu;
    TEST_ASSERT_EQUAL_INT(IDA_RC_NOT_FOUND, ida_core_param_find("", &id));
    TEST_ASSERT_EQUAL_INT(IDA_RC_NOT_FOUND, ida_core_param_find("yok_boyle_ad", &id));
    TEST_ASSERT_EQUAL_INT(IDA_RC_NOT_FOUND, ida_core_param_find("ABCDEFGHIJKLMNOP", &id)); /* 16, bilinmeyen */
    TEST_ASSERT_EQUAL_UINT16(0xBEEFu, id);
    memset(g_ad17, 'Q', sizeof g_ad17);
    TEST_ASSERT_EQUAL_INT(IDA_RC_NOT_FOUND, ida_core_param_find(g_ad17, &id));
    TEST_ASSERT_EQUAL_UINT16(0xBEEFu, id);
}

static uint16_t bilinmeyen_id(void)
{
    uint16_t aday = 0xFFFFu;
    bool var = true;
    while (var) {
        var = false;
        for (unsigned iu = 0; iu < (unsigned)(kc_param_sayisi()); ++iu) {
        uint16_t i = (uint16_t)iu;
            if (kc_param_desc(i)->id == aday) {
                var = true;
            }
        }
        if (var) {
            aday = (uint16_t)(aday - 1u);
        }
    }
    return aday;
}

static void test_A4_4_15_param_get(void)
{
    ida_param_value_t v;
    ida_param_value_t desen;
    const ida_param_desc_t *d = kc_param_desc(0u);
    size_t boy = kc_core_boyu();
    memset(&v, 0x77, sizeof v);
    desen = v;
    /* Kurulmamış örnek: NOT_INIT, *v yazılmaz. */
    TEST_ASSERT_EQUAL_INT(IDA_RC_NOT_INIT, ida_core_param_get(kc_core(0), d->id, &v));
    TEST_ASSERT_EQUAL_MEMORY(&desen, &v, sizeof v);
    TEST_ASSERT_EQUAL_INT(IDA_RC_BAD_INPUT, ida_core_param_get(NULL, d->id, &v));
    TEST_ASSERT_EQUAL_MEMORY(&desen, &v, sizeof v);
    TEST_ASSERT_EQUAL_INT(IDA_RC_BAD_INPUT, ida_core_param_get(kc_core(0), d->id, NULL));
    /* 4.15 denetim sırası (B4): NULL → BAD_INPUT; kurulmamış → NOT_INIT; bilinmeyen id →
       NOT_FOUND; ilk tutan kazanır. */
    TEST_ASSERT_EQUAL_INT(IDA_RC_NOT_INIT, ida_core_param_get(kc_core(0), bilinmeyen_id(), &v));
    TEST_ASSERT_EQUAL_MEMORY(&desen, &v, sizeof v);
    TEST_ASSERT_EQUAL_INT(IDA_RC_BAD_INPUT, ida_core_param_get(kc_core(0), bilinmeyen_id(), NULL));
    TEST_ASSERT_EQUAL_INT(IDA_RC_BAD_INPUT, ida_core_param_get(NULL, bilinmeyen_id(), &v));
    kc_hazirla(1);
    memcpy(g_kopya, kc_bellek(0), boy);
    TEST_ASSERT_EQUAL_INT(IDA_RC_BAD_INPUT, ida_core_param_get(NULL, d->id, &v));
    TEST_ASSERT_EQUAL_INT(IDA_RC_BAD_INPUT, ida_core_param_get(kc_core(0), d->id, NULL));
    TEST_ASSERT_EQUAL_INT(IDA_RC_NOT_FOUND, ida_core_param_get(kc_core(0), bilinmeyen_id(), &v));
    TEST_ASSERT_EQUAL_MEMORY(&desen, &v, sizeof v);
    for (unsigned iu = 0; iu < (unsigned)(kc_param_sayisi()); ++iu) {
        uint16_t i = (uint16_t)iu;
        const ida_param_desc_t *di = kc_param_desc(i);
        TEST_ASSERT_EQUAL_INT(IDA_RC_OK, ida_core_param_get(kc_core(0), di->id, &v));
        TEST_ASSERT_EQUAL_UINT16(di->id, v.id);
        TEST_ASSERT_EQUAL_UINT8(di->type, v.type);
        TEST_ASSERT_EQUAL_HEX32_MESSAGE(kc_param_ham(&di->def), kc_param_ham(&v), "bloksuz init: varsayılan (A4-K78)");
    }
    /* A4-K85: param_get örneği değiştirmez. */
    TEST_ASSERT_EQUAL_MEMORY_MESSAGE(g_kopya, kc_bellek(0), boy, "param_get örnek belleğini değiştirdi");
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_A4_4_15_info_null_ve_her_alan);
    RUN_TEST(test_A4_4_15_init_null_satirlari);
    RUN_TEST(test_A4_4_15_step_tick_null_kurulmamis);
    RUN_TEST(test_A4_4_15_step_tick_null_kurulu);
    RUN_TEST(test_A4_K40_ping360_null_olabilir);
    RUN_TEST(test_A4_4_15_background_c_null);
    RUN_TEST(test_A4_4_15_param_desc);
    RUN_TEST(test_A4_4_15_param_find);
    RUN_TEST(test_A4_4_15_param_get);
    return UNITY_END();
}
