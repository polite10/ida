/*
 * test_kalici.c — açılış sayacı ve parametre A/B blokları (firmware/app/kalici.h) birim
 * testleri. Flash, RAM'de 32 KB'lık bir modeldir: silme 0xFF yazar; silinmemiş quad-word'e
 * yazım hata verir (STM32U5 gibi); kesinti, durum makinesinin yarıda bırakılıp aynı bellekle
 * yeniden açılmasıyla taklit edilir.
 */
#include "kalici.h"
#include "unity.h"

#include <string.h>

void setUp(void);
void tearDown(void);

#define BELLEK_BAYT (IDA_KAL_SAYFA_N * IDA_KAL_SAYFA_BAYT)
static uint8_t bellek[BELLEK_BAYT];
static int yazma_hatasi; /* 1: sonraki yaz16 hata verir */
static uint32_t silme, yazma;
static ida_kalici_t k;

static const uint8_t *m_taban(void *ctx)
{
    (void)ctx;
    return bellek;
}

/* Modelin bütün erişimleri bu denetimden geçer: [ofs, ofs + n) bellek içinde değilse test
   başarısızdır (modülün sınır dışı bir adres üretmesi hatadır; MSVC /analyze C6385/C6386). */
static bool sinirda(uint32_t ofs, uint32_t n)
{
    const bool ic = ofs <= BELLEK_BAYT && n <= BELLEK_BAYT - ofs;
    TEST_ASSERT_TRUE_MESSAGE(ic, "flash modeli: sınır dışı erişim");
    return ic;
}

static int m_sil(void *ctx, uint32_t s)
{
    (void)ctx;
    if (s >= IDA_KAL_SAYFA_N) {
        return -1;
    }
    const uint32_t ofs = s * IDA_KAL_SAYFA_BAYT;
    if (!sinirda(ofs, IDA_KAL_SAYFA_BAYT) || ofs > BELLEK_BAYT - IDA_KAL_SAYFA_BAYT) {
        return -1;
    }
    memset(&bellek[ofs], 0xFF, IDA_KAL_SAYFA_BAYT);
    silme++;
    return 0;
}

static int m_yaz16(void *ctx, uint32_t ofs, const uint8_t q[IDA_KAL_QW])
{
    (void)ctx;
    if (yazma_hatasi || (ofs % IDA_KAL_QW) != 0u) {
        return -1;
    }
    if (!sinirda(ofs, IDA_KAL_QW) || ofs > BELLEK_BAYT - IDA_KAL_QW) {
        return -1;
    }
    for (uint32_t i = 0u; i < IDA_KAL_QW; i++) {
        if (bellek[ofs + i] != 0xFFu) {
            return -1;
        }
    }
    memcpy(&bellek[ofs], q, IDA_KAL_QW);
    yazma++;
    return 0;
}

static int m_durum(void *ctx)
{
    (void)ctx;
    return 0;
}

static const ida_kal_flash_t F = {NULL, m_taban, m_sil, m_yaz16, m_durum};

void setUp(void)
{
    memset(bellek, 0xFF, sizeof bellek);
    yazma_hatasi = 0;
    silme = yazma = 0u;
}

void tearDown(void) {}

static void ac(void)
{
    ida_kal_acilis(&k, &F, NULL, NULL);
}

static void blob_kur(uint8_t *b, uint32_t n, uint8_t tohum)
{
    for (uint32_t i = 0u; i < n; i++) {
        b[i] = (uint8_t)(tohum + i * 7u);
    }
}

static void kaydet_bitir(const uint8_t *b, uint32_t n, uint32_t gen)
{
    ida_kal_kaydet(&k, b, n, gen);
    for (uint32_t i = 0u; i < 2000u && ida_kal_mesgul(&k); i++) {
        ida_kal_adim(&k);
    }
    TEST_ASSERT_FALSE(ida_kal_mesgul(&k));
}

static void test_bos_flash_sayac_artar(void)
{
    ac();
    TEST_ASSERT_EQUAL_UINT32(1u, k.boot_count);
    TEST_ASSERT_EQUAL_UINT8(1u, k.boot_count_ok);
    uint32_t len = 1u;
    TEST_ASSERT_NULL(ida_kal_blob(&k, &len));
    TEST_ASSERT_EQUAL_UINT32(0u, len);
    TEST_ASSERT_EQUAL_UINT8(IDA_KAL_IDLE, k.store);
    for (uint32_t i = 2u; i <= 5u; i++) {
        ac();
        TEST_ASSERT_EQUAL_UINT32(i, k.boot_count);
    }
}

static void test_sayac_sayfa_degisimi(void)
{
    for (uint32_t i = 1u; i <= 1200u; i++) {
        ac();
        TEST_ASSERT_EQUAL_UINT32(i, k.boot_count);
        TEST_ASSERT_EQUAL_UINT8(1u, k.boot_count_ok);
    }
    TEST_ASSERT_TRUE(silme >= 2u); /* 512 kayıtlık sayfalar iki kez değişti */
}

/* Yarım kalan kayıt (güç kesintisi) geçersiz okunur, yuvası yeniden kullanılmaz, sayaç geri
   gitmez. */
static void test_yarim_sayac_kaydi_atlanir(void)
{
    ac();
    ac();
    TEST_ASSERT_EQUAL_UINT32(2u, k.boot_count);
    /* 3. yuvaya yarım yazılmış bir kayıt (magic var, gerisi bozuk). */
    const uint8_t yarim[IDA_KAL_QW] = {0x42, 0x4F, 0x4F, 0x54, 0x03, 0, 0, 0, 0x12, 0x34, 0, 0, 0, 0, 0, 0};
    memcpy(&bellek[2u * IDA_KAL_QW], yarim, IDA_KAL_QW);
    ac();
    TEST_ASSERT_EQUAL_UINT32(3u, k.boot_count);
    TEST_ASSERT_EQUAL_UINT8(1u, k.boot_count_ok);
    ac();
    TEST_ASSERT_EQUAL_UINT32(4u, k.boot_count);
}

/* Fabrika verisi (0xFF olmayan, geçersiz) taşıyan sayfalar: sayfa silinir, sayaç 1'den başlar. */
static void test_cop_veri(void)
{
    memset(bellek, 0x5A, sizeof bellek);
    ac();
    TEST_ASSERT_EQUAL_UINT32(1u, k.boot_count);
    TEST_ASSERT_EQUAL_UINT8(1u, k.boot_count_ok);
    TEST_ASSERT_EQUAL_INT8(-1, k.gecerli_sayfa);
}

static void test_sayac_tukenir(void)
{
    uint8_t q[IDA_KAL_QW];
    const uint32_t n = 0xFFFFFFFFu;
    q[0] = 0x42;
    q[1] = 0x4F;
    q[2] = 0x4F;
    q[3] = 0x54;
    memcpy(q + 4, &n, 4);
    const uint32_t d = 0u;
    memcpy(q + 8, &d, 4);
    const uint32_t c = ida_kal_crc32(q, 12u);
    memcpy(q + 12, &c, 4);
    memcpy(&bellek[0], q, IDA_KAL_QW);
    ac();
    TEST_ASSERT_EQUAL_UINT8(0u, k.boot_count_ok);
    TEST_ASSERT_EQUAL_UINT32(0xFFFFFFFFu, k.boot_count);
}

static void test_yazma_hatasi_sayac_ayrilamaz(void)
{
    yazma_hatasi = 1;
    ac();
    TEST_ASSERT_EQUAL_UINT8(0u, k.boot_count_ok);
}

static void test_parametre_kaydi_ve_acilis(void)
{
    static uint8_t b[300];
    blob_kur(b, sizeof b, 1u);
    ac();
    kaydet_bitir(b, sizeof b, 5u);
    TEST_ASSERT_EQUAL_UINT8(IDA_KAL_OK, k.store);
    TEST_ASSERT_EQUAL_UINT32(5u, k.store_gen);
    TEST_ASSERT_EQUAL_UINT32(5u, k.blok_gen);
    TEST_ASSERT_EQUAL_UINT32(ida_kal_crc32(b, sizeof b), k.blok_crc);
    ac(); /* "kart sıfırlandı" */
    uint32_t len = 0u;
    const uint8_t *p = ida_kal_blob(&k, &len);
    TEST_ASSERT_NOT_NULL(p);
    if (p == NULL) {
        return;
    }
    TEST_ASSERT_EQUAL_UINT32(sizeof b, len);
    TEST_ASSERT_EQUAL_MEMORY(b, p, sizeof b);
    TEST_ASSERT_EQUAL_UINT32(5u, k.store_gen); /* init'te kalıcı bloğun kuşağı (A1-K68) */
    TEST_ASSERT_EQUAL_UINT8(IDA_KAL_IDLE, k.store);
}

/* A/B dönüşümü: her kayıt en yeni bloğu taşımayan sayfaya; en yeni kuşak seçilir. */
static void test_ab_donusumu(void)
{
    static uint8_t b[64];
    ac();
    for (uint32_t gen = 1u; gen <= 5u; gen++) {
        blob_kur(b, sizeof b, (uint8_t)gen);
        const int8_t onceki = k.gecerli_sayfa;
        kaydet_bitir(b, sizeof b, gen);
        TEST_ASSERT_EQUAL_UINT8(IDA_KAL_OK, k.store);
        TEST_ASSERT_TRUE(k.gecerli_sayfa != onceki);
        ac();
        uint32_t len = 0u;
        const uint8_t *p = ida_kal_blob(&k, &len);
        TEST_ASSERT_NOT_NULL(p);
        if (p == NULL) {
            return;
        }
        TEST_ASSERT_EQUAL_UINT32(gen, k.blok_gen);
        TEST_ASSERT_EQUAL_MEMORY(b, p, sizeof b);
    }
}

/* Kayıt her adımda kesilir (güç kesintisi / IWDG): yeniden açılışta eski blok geçerlidir. */
static void test_kesinti_eski_blok_korunur(void)
{
    static uint8_t eski[200], yeni[200];
    blob_kur(eski, sizeof eski, 9u);
    blob_kur(yeni, sizeof yeni, 77u);
    for (uint32_t kes = 0u; kes < 20u; kes++) {
        setUp();
        ac();
        kaydet_bitir(eski, sizeof eski, 7u);
        ida_kal_kaydet(&k, yeni, sizeof yeni, 8u);
        for (uint32_t i = 0u; i < kes && ida_kal_mesgul(&k); i++) {
            ida_kal_adim(&k);
        }
        const bool bitti = !ida_kal_mesgul(&k);
        ac();
        uint32_t len = 0u;
        const uint8_t *p = ida_kal_blob(&k, &len);
        TEST_ASSERT_NOT_NULL(p);
        if (p == NULL) {
            return;
        }
        /* Ya eski ya yeni blok, ikisinden biri bütünüyle (tamamlanma kaydı yazıldıysa yeni). */
        if (bitti || k.blok_gen == 8u) {
            TEST_ASSERT_EQUAL_UINT32(8u, k.blok_gen);
            TEST_ASSERT_EQUAL_MEMORY(yeni, p, sizeof yeni);
        } else {
            TEST_ASSERT_EQUAL_UINT32(7u, k.blok_gen);
            TEST_ASSERT_EQUAL_MEMORY(eski, p, sizeof eski);
        }
    }
}

/* Yarım yazılmış quad-word (tamamlanma kaydı bozuk) geçersizdir. */
static void test_bozuk_tamamlanma(void)
{
    static uint8_t b[32];
    blob_kur(b, sizeof b, 3u);
    ac();
    kaydet_bitir(b, sizeof b, 3u);
    const int8_t s = k.gecerli_sayfa;
    TEST_ASSERT_TRUE(s >= 0 && (uint32_t)s < IDA_KAL_SAYFA_N);
    if (s < 0 || (uint32_t)s >= IDA_KAL_SAYFA_N) {
        return;
    }
    const uint32_t i = (uint32_t)s * IDA_KAL_SAYFA_BAYT + IDA_KAL_CMIT_OFS + 5u;
    if (!sinirda(i, 1u) || i >= BELLEK_BAYT) {
        return;
    }
    bellek[i] ^= 0x01u;
    ac();
    TEST_ASSERT_EQUAL_INT8(-1, k.gecerli_sayfa);
}

/* A4-K83: oturum sonunda iptal -> FAIL / ABORTED, kuşak o denemeninki; eski blok değişmez. */
static void test_iptal(void)
{
    static uint8_t eski[64], yeni[64];
    blob_kur(eski, sizeof eski, 1u);
    blob_kur(yeni, sizeof yeni, 2u);
    ac();
    kaydet_bitir(eski, sizeof eski, 1u);
    ida_kal_kaydet(&k, yeni, sizeof yeni, 2u);
    TEST_ASSERT_EQUAL_UINT8(IDA_KAL_BUSY, k.store);
    ida_kal_adim(&k);
    ida_kal_iptal(&k);
    ida_kal_adim(&k);
    TEST_ASSERT_FALSE(ida_kal_mesgul(&k));
    TEST_ASSERT_EQUAL_UINT8(IDA_KAL_FAIL, k.store);
    TEST_ASSERT_EQUAL_UINT8(IDA_KAL_FAIL_ABORTED, k.fail);
    TEST_ASSERT_EQUAL_UINT32(2u, k.store_gen);
    TEST_ASSERT_EQUAL_UINT32(1u, k.blok_gen);
    ac();
    TEST_ASSERT_EQUAL_UINT32(1u, k.blok_gen);
}

static void test_cok_buyuk_ve_flash_hatasi(void)
{
    static uint8_t b[IDA_KAL_BLOB_AZAMI + 1u];
    ac();
    ida_kal_kaydet(&k, b, sizeof b, 4u);
    TEST_ASSERT_EQUAL_UINT8(IDA_KAL_FAIL, k.store);
    TEST_ASSERT_EQUAL_UINT8(IDA_KAL_FAIL_TOO_BIG, k.fail);
    TEST_ASSERT_EQUAL_UINT32(4u, k.store_gen);
    ida_kal_kaydet(&k, b, 100u, 5u);
    yazma_hatasi = 1;
    for (uint32_t i = 0u; i < 100u && ida_kal_mesgul(&k); i++) {
        ida_kal_adim(&k);
    }
    TEST_ASSERT_EQUAL_UINT8(IDA_KAL_FAIL, k.store);
    TEST_ASSERT_EQUAL_UINT8(IDA_KAL_FAIL_FLASH, k.fail);
    TEST_ASSERT_EQUAL_UINT32(5u, k.store_gen);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_bos_flash_sayac_artar);
    RUN_TEST(test_sayac_sayfa_degisimi);
    RUN_TEST(test_yarim_sayac_kaydi_atlanir);
    RUN_TEST(test_cop_veri);
    RUN_TEST(test_sayac_tukenir);
    RUN_TEST(test_yazma_hatasi_sayac_ayrilamaz);
    RUN_TEST(test_parametre_kaydi_ve_acilis);
    RUN_TEST(test_ab_donusumu);
    RUN_TEST(test_kesinti_eski_blok_korunur);
    RUN_TEST(test_bozuk_tamamlanma);
    RUN_TEST(test_iptal);
    RUN_TEST(test_cok_buyuk_ve_flash_hatasi);
    return UNITY_END();
}
