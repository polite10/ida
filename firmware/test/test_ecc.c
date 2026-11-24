/*
 * test_ecc.c — flash ECC olaylarının işlenmesi (firmware/bsp/ecc.h; sol bulgu 2 ve 6).
 */
#include "ecc.h"
#include "unity.h"

void setUp(void);
void tearDown(void);

static ida_ecc_t e;

void setUp(void)
{
    ida_ecc_kur(&e);
}

void tearDown(void) {}

/* Kalıcı bölgedeki bir bayt adresinin ECCR değeri (Bank 2). */
static uint32_t nvm(uint32_t ofs, uint32_t bayrak)
{
    return bayrak | IDA_ECC_ECCIE | IDA_ECC_BK | ((IDA_ECC_NVM_OFS + ofs) & IDA_ECC_ADDR_MASK);
}

/* Kod bölgesi (Bank 1) adresi. */
static uint32_t kod(uint32_t ofs, uint32_t bayrak)
{
    return bayrak | IDA_ECC_ECCIE | (ofs & IDA_ECC_ADDR_MASK);
}

/* Sol bulgu 2: tek hata (ECCC) kaydedilir ve temizlenir; temizlik ECCD'ye dokunmaz, düzeltme
   kesmesini açık tutar. Önceki kodda ECCC yolu yoktu (bayrak kurulu kalıyordu). */
static void test_eccc_kaydedilir_ve_temizlenir(void)
{
    bool cift = true, bizim = true;
    const uint32_t t = ida_ecc_isle(&e, nvm(0x100u, IDA_ECC_ECCC), &cift, &bizim);
    TEST_ASSERT_EQUAL_HEX32(IDA_ECC_ECCIE | IDA_ECC_ECCC, t);
    TEST_ASSERT_FALSE(cift);
    TEST_ASSERT_FALSE(bizim);
    TEST_ASSERT_EQUAL_UINT32(1u, e.eccc_nvm);
    TEST_ASSERT_FALSE(ida_ecc_bozuk(&e, 0x100u, 16u)); /* düzeltilen hata okumayı bozmaz */
}

/* Tek hata ardından çift hata, kalıcı bölgede: ikisi de işlenir; çift hata yutulabilir ve
   okuma reddedilir. */
static void test_tek_sonra_cift_nvm(void)
{
    (void)ida_ecc_isle(&e, nvm(0x40u, IDA_ECC_ECCC), NULL, NULL);
    bool cift = false, bizim = false;
    const uint32_t t = ida_ecc_isle(&e, nvm(0x2010u, IDA_ECC_ECCD), &cift, &bizim);
    TEST_ASSERT_EQUAL_HEX32(IDA_ECC_ECCIE | IDA_ECC_ECCD, t);
    TEST_ASSERT_TRUE(cift);
    TEST_ASSERT_TRUE(bizim);
    TEST_ASSERT_EQUAL_UINT32(1u, e.eccd_nvm);
    TEST_ASSERT_TRUE(ida_ecc_bozuk(&e, 0x2010u, 16u));
    TEST_ASSERT_TRUE(ida_ecc_bozuk(&e, 0x2000u, 0x40u)); /* aralık içeriyor */
    TEST_ASSERT_FALSE(ida_ecc_bozuk(&e, 0x2020u, 16u));
    TEST_ASSERT_FALSE(ida_ecc_bozuk(&e, 0x0u, 0x2010u));
}

/* Aynı okumada iki bayrak birlikte kurulu. */
static void test_eccc_ve_eccd_birlikte(void)
{
    bool cift = false, bizim = false;
    const uint32_t t =
        ida_ecc_isle(&e, nvm(0x7FF0u, IDA_ECC_ECCC | IDA_ECC_ECCD), &cift, &bizim);
    TEST_ASSERT_EQUAL_HEX32(IDA_ECC_ECCIE | IDA_ECC_ECCC | IDA_ECC_ECCD, t);
    TEST_ASSERT_TRUE(bizim);
    TEST_ASSERT_TRUE(ida_ecc_bozuk(&e, 0x7FF0u, 16u));
}

/* Kod bölgesinde tek hata ardından çift hata: tek hata sayılır, çift hata yutulamaz (ölümcül). */
static void test_tek_sonra_cift_kod(void)
{
    (void)ida_ecc_isle(&e, kod(0x1234u, IDA_ECC_ECCC), NULL, NULL);
    TEST_ASSERT_EQUAL_UINT32(1u, e.eccc_diger);
    bool cift = false, bizim = true;
    (void)ida_ecc_isle(&e, kod(0x1240u, IDA_ECC_ECCD), &cift, &bizim);
    TEST_ASSERT_TRUE(cift);
    TEST_ASSERT_FALSE(bizim);
    TEST_ASSERT_EQUAL_UINT32(1u, e.eccd_diger);
    /* Bank 2'de ama bölge dışı, sistem flash'ı: yutulamaz. */
    (void)ida_ecc_isle(&e, IDA_ECC_ECCD | IDA_ECC_BK | (IDA_ECC_NVM_OFS - 16u), &cift, &bizim);
    TEST_ASSERT_FALSE(bizim);
    (void)ida_ecc_isle(&e, IDA_ECC_ECCD | IDA_ECC_SYSF | IDA_ECC_BK | IDA_ECC_NVM_OFS, &cift, &bizim);
    TEST_ASSERT_FALSE(bizim);
}

static void test_sayfa_silinince_isaret_kalkar(void)
{
    (void)ida_ecc_isle(&e, nvm(2u * IDA_ECC_SAYFA + 32u, IDA_ECC_ECCD), NULL, NULL);
    (void)ida_ecc_isle(&e, nvm(3u * IDA_ECC_SAYFA + 32u, IDA_ECC_ECCD), NULL, NULL);
    ida_ecc_sayfa_sil(&e, 2u);
    TEST_ASSERT_FALSE(ida_ecc_bozuk(&e, 2u * IDA_ECC_SAYFA, IDA_ECC_SAYFA));
    TEST_ASSERT_TRUE(ida_ecc_bozuk(&e, 3u * IDA_ECC_SAYFA, IDA_ECC_SAYFA));
    ida_ecc_sayfa_sil(&e, 9u); /* geçersiz sayfa: etkisiz */
}

static void test_sinir_disi_guvenli_yon(void)
{
    TEST_ASSERT_TRUE(ida_ecc_bozuk(&e, 0u, 0u));
    TEST_ASSERT_TRUE(ida_ecc_bozuk(&e, IDA_ECC_NVM_BAYT, 16u));
    TEST_ASSERT_TRUE(ida_ecc_bozuk(&e, IDA_ECC_NVM_BAYT - 8u, 16u));
    TEST_ASSERT_FALSE(ida_ecc_bozuk(&e, IDA_ECC_NVM_BAYT - 16u, 16u));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_eccc_kaydedilir_ve_temizlenir);
    RUN_TEST(test_tek_sonra_cift_nvm);
    RUN_TEST(test_eccc_ve_eccd_birlikte);
    RUN_TEST(test_tek_sonra_cift_kod);
    RUN_TEST(test_sayfa_silinince_isaret_kalkar);
    RUN_TEST(test_sinir_disi_guvenli_yon);
    return UNITY_END();
}
