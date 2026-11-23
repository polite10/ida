/*
 * test_acilis.c — sıfırlama nedeni, ardışık arıza sayacı ve hata kaydı
 * (firmware/bsp/acilis_kaydi.h) birim testleri.
 */
#include "acilis_kaydi.h"
#include "ida/port.h"
#include "unity.h"

#include <string.h>

void setUp(void);
void tearDown(void);
void setUp(void) {}
void tearDown(void) {}

static void test_crc32_bilinen_deger(void)
{
    /* CRC-32/ISO-HDLC "123456789" denetim değeri 0xCBF43926. */
    TEST_ASSERT_EQUAL_HEX32(0xCBF43926u, ida_fw_crc32("123456789", 9u));
    TEST_ASSERT_EQUAL_HEX32(0x00000000u, ida_fw_crc32("", 0u));
}

static void test_neden_eslemesi(void)
{
    TEST_ASSERT_EQUAL_UINT8(IDA_RESET_WATCHDOG, ida_sifirlama_nedeni(IDA_CSR_IWDGRSTF | IDA_CSR_PINRSTF, 0));
    TEST_ASSERT_EQUAL_UINT8(IDA_RESET_WATCHDOG, ida_sifirlama_nedeni(IDA_CSR_WWDGRSTF, 1));
    TEST_ASSERT_EQUAL_UINT8(IDA_RESET_FAULT, ida_sifirlama_nedeni(IDA_CSR_SFTRSTF | IDA_CSR_PINRSTF, 1));
    TEST_ASSERT_EQUAL_UINT8(IDA_RESET_SOFTWARE, ida_sifirlama_nedeni(IDA_CSR_SFTRSTF | IDA_CSR_PINRSTF, 0));
    TEST_ASSERT_EQUAL_UINT8(IDA_RESET_POWER_ON, ida_sifirlama_nedeni(IDA_CSR_BORRSTF | IDA_CSR_PINRSTF, 0));
    TEST_ASSERT_EQUAL_UINT8(IDA_RESET_PIN, ida_sifirlama_nedeni(IDA_CSR_PINRSTF, 0));
    /* Bekleyen arıza işareti yalnız yazılım sıfırlamasında FAULT'a çevirir. */
    TEST_ASSERT_EQUAL_UINT8(IDA_RESET_PIN, ida_sifirlama_nedeni(IDA_CSR_PINRSTF, 1));
    TEST_ASSERT_EQUAL_UINT8(IDA_RESET_UNKNOWN, ida_sifirlama_nedeni(IDA_CSR_OBLRSTF, 0));
    TEST_ASSERT_EQUAL_UINT8(IDA_RESET_UNKNOWN, ida_sifirlama_nedeni(IDA_CSR_LPWRRSTF, 0));
    TEST_ASSERT_EQUAL_UINT8(IDA_RESET_UNKNOWN, ida_sifirlama_nedeni(0u, 0));
}

static void test_gecersiz_kayit_kurulur(void)
{
    ida_acilis_kaydi_t k;
    memset(&k, 0xA5, sizeof k);
    ida_acilis_sonuc_t s;
    ida_kayit_acilis(&k, IDA_CSR_BORRSTF | IDA_CSR_PINRSTF | 0x0000FFFFu, &s);
    TEST_ASSERT_EQUAL_UINT8(0u, s.kayit_gecerliydi);
    TEST_ASSERT_EQUAL_UINT8(IDA_RESET_POWER_ON, s.reset_cause);
    TEST_ASSERT_EQUAL_HEX32(IDA_CSR_BORRSTF | IDA_CSR_PINRSTF, s.reset_flags); /* yalnız bayraklar */
    TEST_ASSERT_EQUAL_UINT8(0u, s.ardisik_ariza);
    TEST_ASSERT_TRUE(ida_kayit_gecerli(&k));
    TEST_ASSERT_EQUAL_UINT32(0u, k.ariza_var);
}

static void test_ardisik_ariza_sayaci(void)
{
    ida_acilis_kaydi_t k;
    ida_kayit_kur(&k);
    ida_acilis_sonuc_t s;
    /* hata işleyicisinin yazdığı gibi: bekleyen arıza, yazılım sıfırlaması */
    k.ariza_bekliyor = 1u;
    k.ariza_var = 1u;
    k.pc = 0x08001234u;
    ida_kayit_muhurle(&k);
    ida_kayit_acilis(&k, IDA_CSR_SFTRSTF | IDA_CSR_PINRSTF, &s);
    TEST_ASSERT_EQUAL_UINT8(IDA_RESET_FAULT, s.reset_cause);
    TEST_ASSERT_EQUAL_UINT8(1u, s.onceki_ariza);
    TEST_ASSERT_EQUAL_UINT8(1u, s.ardisik_ariza);
    TEST_ASSERT_EQUAL_UINT32(0u, k.ariza_bekliyor);
    TEST_ASSERT_EQUAL_HEX32(0x08001234u, k.pc); /* hata alanları korunur */
    TEST_ASSERT_TRUE(ida_kayit_gecerli(&k));
    ida_kayit_acilis(&k, IDA_CSR_IWDGRSTF | IDA_CSR_PINRSTF, &s);
    TEST_ASSERT_EQUAL_UINT8(IDA_RESET_WATCHDOG, s.reset_cause);
    TEST_ASSERT_EQUAL_UINT8(2u, s.ardisik_ariza);
    TEST_ASSERT_EQUAL_UINT8(0u, s.onceki_ariza);
    ida_kayit_acilis(&k, 0u, &s); /* UNKNOWN: değişmez */
    TEST_ASSERT_EQUAL_UINT8(2u, s.ardisik_ariza);
    ida_kayit_acilis(&k, IDA_CSR_SFTRSTF, &s); /* planlı/hata ayıklayıcı: sıfırlar */
    TEST_ASSERT_EQUAL_UINT8(IDA_RESET_SOFTWARE, s.reset_cause);
    TEST_ASSERT_EQUAL_UINT8(0u, s.ardisik_ariza);
    ida_kayit_acilis(&k, IDA_CSR_IWDGRSTF, &s);
    ida_kayit_acilis(&k, IDA_CSR_PINRSTF, &s);
    TEST_ASSERT_EQUAL_UINT8(0u, s.ardisik_ariza);
}

static void test_ardisik_ariza_doyar(void)
{
    ida_acilis_kaydi_t k;
    ida_kayit_kur(&k);
    ida_acilis_sonuc_t s;
    for (int i = 0; i < 300; i++) {
        ida_kayit_acilis(&k, IDA_CSR_IWDGRSTF, &s);
    }
    TEST_ASSERT_EQUAL_UINT8(255u, s.ardisik_ariza);
}

static void test_bozulan_kayit_reddedilir(void)
{
    ida_acilis_kaydi_t k;
    ida_kayit_kur(&k);
    k.ardisik_ariza = 7u; /* mühürlenmeden değişti */
    TEST_ASSERT_FALSE(ida_kayit_gecerli(&k));
    ida_acilis_sonuc_t s;
    ida_kayit_acilis(&k, IDA_CSR_IWDGRSTF, &s);
    TEST_ASSERT_EQUAL_UINT8(0u, s.kayit_gecerliydi);
    TEST_ASSERT_EQUAL_UINT8(1u, s.ardisik_ariza); /* sıfırdan kuruldu, sonra +1 */
}

/* Sol genel bulgu 8: kararlı çalışmadan sonra ardışık arıza sayacı temizlenir; kayıt CRC'siyle
   tutarlı kalır, hata alanları korunur; geçersiz kayıt kurulur. */
static void test_ardisik_temizlenir(void)
{
    ida_acilis_kaydi_t k;
    ida_kayit_kur(&k);
    k.ardisik_ariza = 2u;
    k.ariza_var = 1u;
    k.pc = 0x08001234u;
    ida_kayit_muhurle(&k);
    ida_kayit_ardisik_temizle(&k);
    TEST_ASSERT_EQUAL_UINT32(0u, k.ardisik_ariza);
    TEST_ASSERT_EQUAL_INT(1, ida_kayit_gecerli(&k));
    TEST_ASSERT_EQUAL_UINT32(1u, k.ariza_var);
    TEST_ASSERT_EQUAL_HEX32(0x08001234u, k.pc);
    /* Sonraki açılış (IWDG) bir'den başlar. */
    ida_acilis_sonuc_t s;
    ida_kayit_acilis(&k, IDA_CSR_IWDGRSTF, &s);
    TEST_ASSERT_EQUAL_UINT8(1u, s.ardisik_ariza);
    memset(&k, 0x5A, sizeof k); /* bozuk */
    ida_kayit_ardisik_temizle(&k);
    TEST_ASSERT_EQUAL_INT(1, ida_kayit_gecerli(&k));
    TEST_ASSERT_EQUAL_UINT32(0u, k.ardisik_ariza);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_crc32_bilinen_deger);
    RUN_TEST(test_neden_eslemesi);
    RUN_TEST(test_gecersiz_kayit_kurulur);
    RUN_TEST(test_ardisik_ariza_sayaci);
    RUN_TEST(test_ardisik_ariza_doyar);
    RUN_TEST(test_bozulan_kayit_reddedilir);
    RUN_TEST(test_ardisik_temizlenir);
    return UNITY_END();
}
