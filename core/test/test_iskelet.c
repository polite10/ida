/*
 * test_iskelet.c — boş çekirdekte derleme ve test zincirini gösteren örnek birim testi (T1).
 *
 * PC'de CTest ile koşar; firmware kökünde aynı kaynak arm-none-eabi ile bağlanır.
 * Sözleşme değerlerini yeniden sınamaz (o iş sözleşme başlıkları denetimi ve uygunluk
 * testlerinindir); yalnız çekirdek kitaplığının bağlanıp çağrılabildiğini gösterir.
 */
#include "iskelet.h"

#include "ida/port.h"
#include "unity.h"

void setUp(void);
void tearDown(void);
void setUp(void) {}
void tearDown(void) {}

static void test_port_surumu_cekirdekten_okunur(void)
{
    const uint32_t beklenen =
        ((uint32_t)IDA_PORT_VERSION_MAJOR << 16) | (uint32_t)IDA_PORT_VERSION_MINOR;
    TEST_ASSERT_EQUAL_UINT32(beklenen, ida_core_iskelet_surumu());
}

static void test_algilama_kaydi_boyutu(void)
{
    TEST_ASSERT_EQUAL_size_t(80u, sizeof(ida_det_record_t));
    TEST_ASSERT_EQUAL_size_t(32u, sizeof(ida_det_batch_t));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_port_surumu_cekirdekten_okunur);
    RUN_TEST(test_algilama_kaydi_boyutu);
    return UNITY_END();
}
