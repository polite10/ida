/*
 * test_arka_plan.c — ida_core_background'ın değerlendirme sırası ve sayımı (A4-K08, K10,
 * K93, K109, K117; 3.11 arka plan sırası, 4.15).
 */
#include <string.h>

#include "destek.h"
#include "unity.h"

void setUp(void);
void tearDown(void);
void setUp(void) {}
void tearDown(void) {}

static ida_core_t C;
static ida_core_t KOPYA;
static ida_in_t IN;
static ida_out_t OUT;

static void test_sira_ve_kurulmamis(void)
{
    const ida_wtime_t gecerli = {2000 * DS_MS};
    const ida_wtime_t gecersiz = {-1};
    TEST_ASSERT_EQUAL(IDA_BG_BAD_INPUT, ida_core_background(NULL, gecerli));
    /* Kurulmamış: IDLE, hiçbir şey yazılmaz, now_w denetlenmez. */
    (void)memset(&C, 0x3C, sizeof(C));
    KOPYA = C;
    TEST_ASSERT_EQUAL(IDA_BG_IDLE, ida_core_background(&C, gecersiz));
    TEST_ASSERT_EQUAL(IDA_BG_IDLE, ida_core_background(&C, gecerli));
    TEST_ASSERT_EQUAL_MEMORY(&KOPYA, &C, sizeof(C));
}

static void test_k117_gecersiz_now_ve_saglik(void)
{
    TEST_ASSERT_EQUAL(IDA_RC_OK, ds_kur(&C));
    ds_girdi(&IN, 2000 * DS_MS, 1u, true);
    TEST_ASSERT_EQUAL(IDA_RC_OK, ida_core_step(&C, &IN, &OUT));
    ida_wtime_t w = {-1};
    TEST_ASSERT_EQUAL(IDA_BG_BAD_INPUT, ida_core_background(&C, w));
    w.ns = IDA_TIME_MAX_NS + 1;
    TEST_ASSERT_EQUAL(IDA_BG_BAD_INPUT, ida_core_background(&C, w));
    w.ns = 2001 * DS_MS;
    TEST_ASSERT_EQUAL(IDA_BG_IDLE, ida_core_background(&C, w)); /* yer tutucu: iş yok */
    /* bg_slices yalnız IDLE/MORE dönenleri sayar: 1 → uyuşmazlık yok */
    ds_girdi(&IN, 2005 * DS_MS, 2u, true);
    IN.bg_slices = 1u;
    TEST_ASSERT_EQUAL(IDA_RC_OK, ida_core_step(&C, &IN, &OUT));
    TEST_ASSERT_EQUAL_UINT32(2u, OUT.status.cnt.bg_bad_input);
    TEST_ASSERT_TRUE(OUT.health.bg_bad_input);
    TEST_ASSERT_EQUAL_UINT32(0u, OUT.status.cnt.bg_slice_mismatch);
    TEST_ASSERT_EQUAL_UINT32(1u, ds_gunluk_say(&OUT, (uint16_t)IDA_LOG_BG_BAD_INPUT));
    TEST_ASSERT_EQUAL_INT32(2, OUT.logs[0].arg0);
    /* ondan sonraki çağrıda bayrak 0 */
    ds_girdi(&IN, 2010 * DS_MS, 3u, true);
    TEST_ASSERT_EQUAL(IDA_RC_OK, ida_core_step(&C, &IN, &OUT));
    TEST_ASSERT_FALSE(OUT.health.bg_bad_input);
}

static void test_k109_uyusmazlik(void)
{
    TEST_ASSERT_EQUAL(IDA_RC_OK, ds_kur(&C));
    const ida_wtime_t w = {1500 * DS_MS};
    TEST_ASSERT_EQUAL(IDA_BG_IDLE, ida_core_background(&C, w));
    TEST_ASSERT_EQUAL(IDA_BG_IDLE, ida_core_background(&C, w));
    ds_girdi(&IN, 2000 * DS_MS, 1u, true);
    IN.bg_slices = 3u; /* çekirdek 2 saydı */
    TEST_ASSERT_EQUAL(IDA_RC_OK, ida_core_step(&C, &IN, &OUT));
    TEST_ASSERT_EQUAL_UINT32(1u, OUT.status.cnt.bg_slice_mismatch);
    TEST_ASSERT_EQUAL_UINT32(1u, ds_gunluk_say(&OUT, (uint16_t)IDA_LOG_BG_SLICE_MISMATCH));
    /* sayım her step/tick'te sıfırlanır */
    ds_girdi(&IN, 2005 * DS_MS, 2u, true);
    IN.bg_slices = 0u;
    TEST_ASSERT_EQUAL(IDA_RC_OK, ida_core_step(&C, &IN, &OUT));
    TEST_ASSERT_EQUAL_UINT32(1u, OUT.status.cnt.bg_slice_mismatch);
    /* out NULL çağrısı da çağrı başına izleri sıfırlar (3.11 (b2)) */
    TEST_ASSERT_EQUAL(IDA_BG_IDLE, ida_core_background(&C, w));
    ds_girdi(&IN, 2010 * DS_MS, 3u, true);
    TEST_ASSERT_EQUAL(IDA_RC_BAD_INPUT, ida_core_step(&C, &IN, NULL));
    ds_girdi(&IN, 2015 * DS_MS, 4u, true);
    TEST_ASSERT_EQUAL(IDA_RC_OK, ida_core_step(&C, &IN, &OUT));
    TEST_ASSERT_EQUAL_UINT32(1u, OUT.status.cnt.bg_slice_mismatch);
}

static void test_k10_arka_plan_now_izi(void)
{
    TEST_ASSERT_EQUAL(IDA_RC_OK, ds_kur(&C)); /* init now = 1000 ms */
    const ida_wtime_t ileri = {3000 * DS_MS};
    TEST_ASSERT_EQUAL(IDA_BG_IDLE, ida_core_background(&C, ileri));
    ds_girdi(&IN, 2000 * DS_MS, 1u, true); /* arka plan çağrısının izine göre geri adım */
    IN.bg_slices = 1u;
    TEST_ASSERT_EQUAL(IDA_RC_OK, ida_core_step(&C, &IN, &OUT));
    TEST_ASSERT_EQUAL_UINT32(1u, OUT.status.cnt.wall_backstep);
}


/* A4-K117, 3.11 (b2): araya giren out NULL çağrısı bg_bad_input bayrağını silmez. */
static void test_k117_b2_bayragi_tasir(void)
{
    TEST_ASSERT_EQUAL(IDA_RC_OK, ds_kur(&C));
    ds_girdi(&IN, 2000 * DS_MS, 1u, true);
    TEST_ASSERT_EQUAL(IDA_RC_OK, ida_core_step(&C, &IN, &OUT));
    const ida_wtime_t kotu = {-1};
    TEST_ASSERT_EQUAL(IDA_BG_BAD_INPUT, ida_core_background(&C, kotu));
    ds_girdi(&IN, 2005 * DS_MS, 2u, true);
    TEST_ASSERT_EQUAL(IDA_RC_BAD_INPUT, ida_core_step(&C, &IN, NULL));
    ds_girdi(&IN, 2010 * DS_MS, 3u, true);
    TEST_ASSERT_EQUAL(IDA_RC_OK, ida_core_step(&C, &IN, &OUT));
    TEST_ASSERT_TRUE(OUT.health.bg_bad_input);
    TEST_ASSERT_EQUAL_UINT32(1u, OUT.status.cnt.bg_bad_input);
    TEST_ASSERT_EQUAL_UINT32(1u, ds_gunluk_say(&OUT, (uint16_t)IDA_LOG_BG_BAD_INPUT));
    ds_girdi(&IN, 2015 * DS_MS, 4u, true);
    TEST_ASSERT_EQUAL(IDA_RC_OK, ida_core_step(&C, &IN, &OUT));
    TEST_ASSERT_FALSE(OUT.health.bg_bad_input);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_sira_ve_kurulmamis);
    RUN_TEST(test_k117_gecersiz_now_ve_saglik);
    RUN_TEST(test_k109_uyusmazlik);
    RUN_TEST(test_k10_arka_plan_now_izi);
    RUN_TEST(test_k117_b2_bayragi_tasir);
    return UNITY_END();
}
