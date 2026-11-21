/* test_istatistik.c — süre sayaçları ve histogram (firmware/app/istatistik.h) birim testleri. */
#include "istatistik.h"
#include "unity.h"

void setUp(void);
void tearDown(void);
void setUp(void) {}
void tearDown(void) {}

#define US(x) ((uint32_t)(x) * IDA_IST_CEVRIM_US)

static void test_kutu_sinirlari(void)
{
    TEST_ASSERT_EQUAL_UINT32(0u, ida_ist_kutu(0u));
    TEST_ASSERT_EQUAL_UINT32(0u, ida_ist_kutu(US(10) - 1u));
    TEST_ASSERT_EQUAL_UINT32(1u, ida_ist_kutu(US(10)));
    TEST_ASSERT_EQUAL_UINT32(4u, ida_ist_kutu(US(70)));
    TEST_ASSERT_EQUAL_UINT32(11u, ida_ist_kutu(US(999)));
    TEST_ASSERT_EQUAL_UINT32(13u, ida_ist_kutu(US(3499)));
    TEST_ASSERT_EQUAL_UINT32(14u, ida_ist_kutu(US(3500)));
    TEST_ASSERT_EQUAL_UINT32(15u, ida_ist_kutu(US(5000)));
    TEST_ASSERT_EQUAL_UINT32(15u, ida_ist_kutu(UINT32_MAX));
}

static void test_ekle(void)
{
    ida_ist_t s;
    ida_ist_sifirla(&s);
    ida_ist_ekle(&s, US(70));
    ida_ist_ekle(&s, US(5));
    ida_ist_ekle(&s, US(600));
    TEST_ASSERT_EQUAL_UINT32(3u, s.sayi);
    TEST_ASSERT_EQUAL_UINT32(US(600), s.en_kotu_cevrim);
    TEST_ASSERT_EQUAL_UINT32(US(5), s.en_iyi_cevrim);
    TEST_ASSERT_EQUAL_UINT32(US(600), s.son_cevrim);
    TEST_ASSERT_EQUAL_UINT64((uint64_t)US(675), s.toplam_cevrim);
    TEST_ASSERT_EQUAL_UINT32(1u, s.kutu[0]);
    TEST_ASSERT_EQUAL_UINT32(1u, s.kutu[4]);
    TEST_ASSERT_EQUAL_UINT32(1u, s.kutu[10]);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_kutu_sinirlari);
    RUN_TEST(test_ekle);
    return UNITY_END();
}
