/*
 * test_crc.c — ust_crc32'nin (A1-K68 save_crc) tablolu hesabı (W4-param-hiz): tanım ve sonuç
 * değişmedi; protokol modülünün tablosu çekirdeğinkiyle (core/src/param/crc32.c) aynı.
 *
 * Referans: değişiklikten önceki bit bit ust_crc32 gövdesi bu dosyada aynen tutulur. Çekirdeğin
 * ida_crc32'si yalnız bu testte, iki tablonun eşitliği için bağlanır (protokol modülü çekirdeğin
 * iç başlıklarına bağlanmaz). Rastgele girdiler sabit tohumludur.
 */
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "param/param.h"
#include "unity.h"
#include "ust_ic.h"

void setUp(void);
void tearDown(void);
void setUp(void) {}
void tearDown(void) {}

/* bc83bca'daki ust_crc32 gövdesi (bit bit). */
static uint32_t ref_crc32(const uint8_t *p, size_t n)
{
    uint32_t c = 0xFFFFFFFFu;
    for (size_t i = 0; i < n; ++i) {
        c ^= p[i];
        for (unsigned k = 0; k < 8u; ++k) {
            c = ((c & 1u) != 0u) ? ((c >> 1) ^ 0xEDB88320u) : (c >> 1);
        }
    }
    return c ^ 0xFFFFFFFFu;
}

#define SEKIZ_KB 8192u
static uint8_t g_veri[SEKIZ_KB];

static uint32_t xorshift(uint32_t *s)
{
    uint32_t x = *s;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    *s = x;
    return x;
}

static void test_bilinen_vektorler(void)
{
    TEST_ASSERT_EQUAL_HEX32(0xCBF43926u, ust_crc32((const uint8_t *)"123456789", 9u));
    TEST_ASSERT_EQUAL_HEX32(0x00000000u, ust_crc32(g_veri, 0u));
    const uint8_t sifir = 0x00u;
    const uint8_t ff = 0xFFu;
    const uint8_t a = (uint8_t)'a';
    TEST_ASSERT_EQUAL_HEX32(0xD202EF8Du, ust_crc32(&sifir, 1u));
    TEST_ASSERT_EQUAL_HEX32(0xFF000000u, ust_crc32(&ff, 1u));
    TEST_ASSERT_EQUAL_HEX32(0xE8B7BE43u, ust_crc32(&a, 1u));
    /* 8 KB: değerler bağımsız bir uygulamayla (Python zlib.crc32) üretildi. */
    (void)memset(g_veri, 0, sizeof g_veri);
    TEST_ASSERT_EQUAL_HEX32(0xD8F49994u, ust_crc32(g_veri, SEKIZ_KB));
    for (uint32_t i = 0u; i < SEKIZ_KB; i++) {
        g_veri[i] = (uint8_t)((i * 7u) + 3u);
    }
    TEST_ASSERT_EQUAL_HEX32(0xB65EF7BFu, ust_crc32(g_veri, SEKIZ_KB));
    TEST_ASSERT_EQUAL_HEX32(ref_crc32(g_veri, SEKIZ_KB), ust_crc32(g_veri, SEKIZ_KB));
}

/* 256 tek baytlık girdi tablonun her satırını bir kez kullanır: protokolün tablosu bit bit
   tanımla ve çekirdeğin tablosuyla satır satır aynı. */
static void test_tablo_cekirdekle_ayni(void)
{
    for (uint32_t b = 0u; b < 256u; b++) {
        const uint8_t x = (uint8_t)b;
        TEST_ASSERT_EQUAL_HEX32(ref_crc32(&x, 1u), ust_crc32(&x, 1u));
        TEST_ASSERT_EQUAL_HEX32(ida_crc32(&x, 1u), ust_crc32(&x, 1u));
    }
}

static void test_rastgele_10000(void)
{
    uint32_t s = 0x5A7EC3C1u;
    for (uint32_t i = 0u; i < SEKIZ_KB; i++) {
        g_veri[i] = (uint8_t)xorshift(&s);
    }
    for (uint32_t n = 0u; n < 10000u; n++) {
        const uint32_t bas = xorshift(&s) % 64u;
        const uint32_t boy = xorshift(&s) % (SEKIZ_KB - 64u + 1u);
        const uint32_t u = ((n & 1u) != 0u) ? boy : (boy % 97u);
        g_veri[bas] = (uint8_t)(g_veri[bas] ^ (uint8_t)n);
        const uint32_t beklenen = ref_crc32(&g_veri[bas], u);
        TEST_ASSERT_EQUAL_HEX32(beklenen, ust_crc32(&g_veri[bas], u));
        TEST_ASSERT_EQUAL_HEX32(beklenen, ida_crc32(&g_veri[bas], u));
    }
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_bilinen_vektorler);
    RUN_TEST(test_tablo_cekirdekle_ayni);
    RUN_TEST(test_rastgele_10000);
    return UNITY_END();
}
