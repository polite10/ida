/*
 * A1 §3.11 kabul testleri (sayaç adları, genişlikleri ve taşma davranışı; K02–K08'in sayaçları):
 *   "... kartın sayaçları 32 bittir, açılıştan (SIL'de init'ten) beri birikir ve 2³²'de sarar."
 *   Tablo 3.11b: rx_bytes, rx_frames, rx_crc_err, rx_unknown_msgid, rx_iflag, rx_bad_len,
 *   rx_wrong_sys, rx_mav1, rx_overflow, rx_resync_bytes, rx_seq_gap.
 * Öğeler:
 *   (a) her sayaç 32 bit, işaretsiz (derleme zamanı)
 *   (b) kurulumda (init) bütün sayaçlar 0
 *   (c) kart rolünde her sayaç 2³²'de sarar (UINT32_MAX + 1 = 0); doyma yok
 *   (d) rx_bytes verilen her baytı sayar; rx_frames her kabulü
 */
#include <limits.h>
#include <stdio.h>
#include <string.h>

#include "kabul_ortak.h"
#include "unity.h"

void setUp(void) {}
void tearDown(void) {}

static uint8_t akis[4096];
static ida_link_alici_t alici;
static kabul_sonuc_t sonuc;

/* (a): genişlik derleme zamanında; işaretsizlik test_A1_3_11_a_isaretsiz'de. */
#define KABUL_32_BIT(ad) \
    _Static_assert(sizeof(((ida_link_sayaclar_t *)0)->ad) == 4, #ad " 32 bit değil")
KABUL_32_BIT(rx_bytes);
KABUL_32_BIT(rx_frames);
KABUL_32_BIT(rx_crc_err);
KABUL_32_BIT(rx_unknown_msgid);
KABUL_32_BIT(rx_iflag);
KABUL_32_BIT(rx_bad_len);
KABUL_32_BIT(rx_wrong_sys);
KABUL_32_BIT(rx_mav1);
KABUL_32_BIT(rx_overflow);
KABUL_32_BIT(rx_resync_bytes);
KABUL_32_BIT(rx_seq_gap);

static void test_A1_3_11_a_isaretsiz(void)
{
    ida_link_sayaclar_t s;
    memset(&s, 0, sizeof s);
#define KABUL_ISARETSIZ(ad) \
    s.ad--;                 \
    TEST_ASSERT_TRUE_MESSAGE(s.ad > 0u, #ad " işaretsiz değil")
    KABUL_ISARETSIZ(rx_bytes);
    KABUL_ISARETSIZ(rx_frames);
    KABUL_ISARETSIZ(rx_crc_err);
    KABUL_ISARETSIZ(rx_unknown_msgid);
    KABUL_ISARETSIZ(rx_iflag);
    KABUL_ISARETSIZ(rx_bad_len);
    KABUL_ISARETSIZ(rx_wrong_sys);
    KABUL_ISARETSIZ(rx_mav1);
    KABUL_ISARETSIZ(rx_overflow);
    KABUL_ISARETSIZ(rx_resync_bytes);
    KABUL_ISARETSIZ(rx_seq_gap);
#undef KABUL_ISARETSIZ
}

static void test_A1_3_11_b_kurulumda_sifir(void)
{
    static const ida_link_rol_t ROLLER[2] = {IDA_LINK_ROL_KART, IDA_LINK_ROL_SIM};
    for (size_t i = 0; i < 2; ++i) {
        memset(&alici, 0xA7, sizeof alici);
        TEST_ASSERT_EQUAL_INT(IDA_LINK_TAMAM, ida_link_alici_kur(&alici, ROLLER[i]));
        ida_link_sayaclar_t sifir;
        memset(&sifir, 0, sizeof sifir);
        kabul_sayac_esit(&sifir, &alici.sayac, "kurulum");
    }
}

/* (c): her sayaç UINT32_MAX'a ayarlanır, o sayacı tam bir artıran akış verilir: 0 olur. */
static void test_A1_3_11_c_kart_sayaclari_sarar(void)
{
    kabul_rng_t r;
    kabul_rng_kur(&r, 0x4B333131ull);
    kabul_parcalayici_t p = {KABUL_PARCA_TUMU, 0u, {0u}};
    const kabul_mesaj_t *imu = kabul_mesaj_bul(46020u);
    size_t n;

    /* rx_frames ve rx_seq_gap: 0, 2 seq'li iki geçerli çerçeve (bir tek boşluk). */
    TEST_ASSERT_EQUAL_INT(IDA_LINK_TAMAM, ida_link_alici_kur(&alici, IDA_LINK_ROL_KART));
    alici.sayac.rx_frames = UINT32_MAX - 1u;
    alici.sayac.rx_seq_gap = UINT32_MAX;
    n = kabul_temiz_kur(akis, &r, 46020u, 0u, 250u, 25u, 10u, 1, NULL);
    n += kabul_temiz_kur(&akis[n], &r, 46020u, 2u, 250u, 25u, 10u, 1, NULL);
    kabul_sonuc_sifirla(&sonuc);
    kabul_besle(&alici, akis, n, &p, &sonuc);
    TEST_ASSERT_EQUAL_UINT32(2u, (uint32_t)sonuc.sayi);
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(0u, alici.sayac.rx_frames, "rx_frames sarmadı");
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(0u, alici.sayac.rx_seq_gap, "rx_seq_gap sarmadı");

    /* rx_bytes: UINT32_MAX - 2 + 5 bayt = 2. */
    TEST_ASSERT_EQUAL_INT(IDA_LINK_TAMAM, ida_link_alici_kur(&alici, IDA_LINK_ROL_KART));
    alici.sayac.rx_bytes = UINT32_MAX - 2u;
    alici.sayac.rx_resync_bytes = UINT32_MAX - 4u;
    for (size_t i = 0; i < 5; ++i) {
        akis[i] = (uint8_t)(0x10u + i);
    }
    kabul_besle(&alici, akis, 5u, &p, &sonuc);
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(2u, alici.sayac.rx_bytes, "rx_bytes sarmadı");
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(0u, alici.sayac.rx_resync_bytes, "rx_resync_bytes sarmadı");

    /* rx_crc_err. */
    TEST_ASSERT_EQUAL_INT(IDA_LINK_TAMAM, ida_link_alici_kur(&alici, IDA_LINK_ROL_KART));
    alici.sayac.rx_crc_err = UINT32_MAX;
    n = kabul_temiz_kur(akis, &r, 46020u, 0u, 250u, 25u, imu->yuk_azami, 1, NULL);
    akis[n - 1u] = (uint8_t)(akis[n - 1u] == 0x01u ? 0x02u : 0x01u);
    kabul_besle(&alici, akis, n, &p, &sonuc);
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(0u, alici.sayac.rx_crc_err, "rx_crc_err sarmadı");

    /* rx_wrong_sys. */
    TEST_ASSERT_EQUAL_INT(IDA_LINK_TAMAM, ida_link_alici_kur(&alici, IDA_LINK_ROL_KART));
    alici.sayac.rx_wrong_sys = UINT32_MAX;
    n = kabul_temiz_kur(akis, &r, 46020u, 0u, 1u, 1u, 5u, 1, NULL);
    kabul_besle(&alici, akis, n, &p, &sonuc);
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(0u, alici.sayac.rx_wrong_sys, "rx_wrong_sys sarmadı");

    /* rx_unknown_msgid (yönü yanlış kimlik; K06 ve Tablo 3.11b aynı sayacı verir). */
    TEST_ASSERT_EQUAL_INT(IDA_LINK_TAMAM, ida_link_alici_kur(&alici, IDA_LINK_ROL_KART));
    alici.sayac.rx_unknown_msgid = UINT32_MAX;
    n = kabul_temiz_kur(akis, &r, 46063u, 0u, 250u, 25u, 5u, 1, NULL);
    kabul_besle(&alici, akis, n, &p, &sonuc);
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(0u, alici.sayac.rx_unknown_msgid, "rx_unknown_msgid sarmadı");

    /* rx_iflag ve rx_bad_len. */
    TEST_ASSERT_EQUAL_INT(IDA_LINK_TAMAM, ida_link_alici_kur(&alici, IDA_LINK_ROL_KART));
    alici.sayac.rx_iflag = UINT32_MAX;
    alici.sayac.rx_bad_len = UINT32_MAX;
    n = kabul_temiz_ham(akis, &r, 10u, 0x01u, 0u, 0u, 250u, 25u, 46020u, imu->crc_extra, 0u);
    n += kabul_temiz_ham(&akis[n], &r, 0u, 0u, 0u, 1u, 250u, 25u, 46020u, imu->crc_extra, 0u);
    kabul_besle(&alici, akis, n, &p, &sonuc);
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(0u, alici.sayac.rx_iflag, "rx_iflag sarmadı");
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(0u, alici.sayac.rx_bad_len, "rx_bad_len sarmadı");

    /* rx_mav1: tek 0xFE ve ardından temiz MAVLink 1 HEARTBEAT gövdesi. */
    TEST_ASSERT_EQUAL_INT(IDA_LINK_TAMAM, ida_link_alici_kur(&alici, IDA_LINK_ROL_KART));
    alici.sayac.rx_mav1 = UINT32_MAX;
    {
        uint8_t m1[17] = {0xFEu, 9u, 0u, 250u, 25u, 0u, 1u, 2u, 3u, 4u, 5u, 6u, 7u, 8u, 9u, 0u, 0u};
        uint16_t crc = kabul_crc(&m1[1], 14u, 0xFFFFu);
        crc = kabul_crc_bayt(crc, 50u);
        m1[15] = (uint8_t)(crc & 0xFFu);
        m1[16] = (uint8_t)(crc >> 8);
        TEST_ASSERT_TRUE(m1[15] != 0xFDu && m1[15] != 0xFEu && m1[16] != 0xFDu && m1[16] != 0xFEu);
        kabul_besle(&alici, m1, sizeof m1, &p, &sonuc);
    }
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(0u, alici.sayac.rx_mav1, "rx_mav1 sarmadı");
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_A1_3_11_a_isaretsiz);
    RUN_TEST(test_A1_3_11_b_kurulumda_sifir);
    RUN_TEST(test_A1_3_11_c_kart_sayaclari_sarar);
    return UNITY_END();
}
