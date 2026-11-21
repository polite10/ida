/*
 * test_saglik.c — ana döngünün donanımsız kararları (firmware/app/saglik.h) birim testleri:
 * sol bulgu 3 (alım ilerleme gözetimi), 5 (200 Hz sayaç sarması), 6 (başlatma sonucu LED).
 */
#include "saglik.h"
#include "unity.h"

void setUp(void);
void tearDown(void);
void setUp(void) {}
void tearDown(void) {}

#define S_US 20000u

/* Bulgu 5: sarmanın iki yanı, kayıpsız. 0xFFFFFFFE (çift, tick) -> 0xFFFFFFFF -> 0 (çift, tick)
   -> 1: kayıp yok, iki tick. */
static void test_sol5_sarma_kayipsiz(void)
{
    ida_izgara_t z;
    ida_izgara_adim_t a;
    ida_izgara_kur(&z, 0xFFFFFFFDu);
    TEST_ASSERT_EQUAL_INT(1, ida_izgara_ilerle(&z, 0xFFFFFFFEu, &a));
    TEST_ASSERT_EQUAL_UINT32(1u, a.tick);
    TEST_ASSERT_EQUAL_UINT32(0u, a.kacan_10ms);
    TEST_ASSERT_EQUAL_INT(1, ida_izgara_ilerle(&z, 0xFFFFFFFFu, &a));
    TEST_ASSERT_EQUAL_UINT32(0u, a.tick);
    TEST_ASSERT_EQUAL_INT(1, ida_izgara_ilerle(&z, 0u, &a));
    TEST_ASSERT_EQUAL_UINT32(1u, a.tick);
    TEST_ASSERT_EQUAL_UINT32(0u, a.kacan_10ms);
    TEST_ASSERT_EQUAL_UINT32(0u, a.kacan_200hz);
    TEST_ASSERT_EQUAL_INT(1, ida_izgara_ilerle(&z, 1u, &a));
    TEST_ASSERT_EQUAL_UINT32(0u, a.tick);
    TEST_ASSERT_EQUAL_UINT32(0u, a.kacan_10ms);
}

/* Bulgu 5: sarmanın iki yanı, kayıplı. 0xFFFFFFFD -> 2: geçen anlar FFFFFFFE, FFFFFFFF, 0, 1,
   2; çift olanlar FFFFFFFE, 0, 2 (3 tick, 2 kayıp); 200 Hz'te 4 kayıp. */
static void test_sol5_sarma_kayipli(void)
{
    ida_izgara_t z;
    ida_izgara_adim_t a;
    ida_izgara_kur(&z, 0xFFFFFFFDu);
    TEST_ASSERT_EQUAL_INT(1, ida_izgara_ilerle(&z, 2u, &a));
    TEST_ASSERT_EQUAL_UINT32(4u, a.kacan_200hz);
    TEST_ASSERT_EQUAL_UINT32(3u, a.tick);
    TEST_ASSERT_EQUAL_UINT32(2u, a.kacan_10ms);
    /* Tek değerden başlayan sarma: 0xFFFFFFFF -> 0 tek tick, kayıp yok. */
    ida_izgara_kur(&z, 0xFFFFFFFFu);
    TEST_ASSERT_EQUAL_INT(1, ida_izgara_ilerle(&z, 0u, &a));
    TEST_ASSERT_EQUAL_UINT32(1u, a.tick);
    TEST_ASSERT_EQUAL_UINT32(0u, a.kacan_10ms);
}

static void test_izgara_olagan(void)
{
    ida_izgara_t z;
    ida_izgara_adim_t a;
    ida_izgara_kur(&z, 10u);
    TEST_ASSERT_EQUAL_INT(0, ida_izgara_ilerle(&z, 10u, &a));
    TEST_ASSERT_EQUAL_INT(1, ida_izgara_ilerle(&z, 11u, &a));
    TEST_ASSERT_EQUAL_UINT32(0u, a.tick);
    TEST_ASSERT_EQUAL_INT(1, ida_izgara_ilerle(&z, 12u, &a));
    TEST_ASSERT_EQUAL_UINT32(1u, a.tick);
    TEST_ASSERT_EQUAL_INT(1, ida_izgara_ilerle(&z, 17u, &a)); /* 13..17: çiftler 14, 16 */
    TEST_ASSERT_EQUAL_UINT32(4u, a.kacan_200hz);
    TEST_ASSERT_EQUAL_UINT32(2u, a.tick);
    TEST_ASSERT_EQUAL_UINT32(1u, a.kacan_10ms);
}

/* Bulgu 3: döngü ve çekirdek tick'i sürerken bekleyen alım tüketilmiyor: 20 ms sonra besleme
   yapılmamalı (yalnız RX koşulu tutmaz). */
static void test_sol3_rx_tuketimi_durunca_beslenmez(void)
{
    ida_rx_gozetim_t g;
    ida_rx_gozetim_kur(&g, 1000u);
    uint64_t t = 1000u;
    /* 30 ms boyunca her 100 µs'de bir döngü turu; alım bekliyor, hiç tüketilmiyor; çekirdek
       tick'i 10 ms'de bir sürüyor. */
    uint64_t son_cagri = 1000u;
    uint32_t ret = 0u;
    for (uint32_t i = 0u; i < 300u; i++) {
        t += 100u;
        if (t - son_cagri >= 10000u) {
            son_cagri = t;
        }
        ida_rx_gozetim_guncelle(&g, true, false, t);
        ida_saglik_girdi_t s = {true, true, son_cagri,
                                ida_rx_gozetim_tamam(&g, true, t, S_US), false, t, S_US};
        ret = ida_saglik_degerlendir(&s);
    }
    TEST_ASSERT_EQUAL_HEX32(IDA_SAGLIK_RX, ret);
}

static void test_rx_gozetim_ilerleme_ve_yeni_bekleme(void)
{
    ida_rx_gozetim_t g;
    ida_rx_gozetim_kur(&g, 0u);
    /* Uzun süre boş: koşul tutar; bekleme 50 ms'de başlar, süre oradan sayılır. */
    ida_rx_gozetim_guncelle(&g, false, false, 40000u);
    TEST_ASSERT_TRUE(ida_rx_gozetim_tamam(&g, false, 40000u, S_US));
    ida_rx_gozetim_guncelle(&g, true, false, 50000u);
    TEST_ASSERT_TRUE(ida_rx_gozetim_tamam(&g, true, 69000u, S_US));
    TEST_ASSERT_FALSE(ida_rx_gozetim_tamam(&g, true, 70001u, S_US));
    /* Tüketim olunca yeniden başlar. */
    ida_rx_gozetim_guncelle(&g, true, true, 70001u);
    TEST_ASSERT_TRUE(ida_rx_gozetim_tamam(&g, true, 80000u, S_US));
}

/* Uzun boşluktan sonra yeni gelen bekleme, gözetim onu görmeden değerlendirilirse süre son
   tüketimden sayılmaz (yanlış ret olmaz). */
static void test_rx_gozetim_yeni_bekleme_hemen_ret_edilmez(void)
{
    ida_rx_gozetim_t g;
    ida_rx_gozetim_kur(&g, 0u);
    ida_rx_gozetim_guncelle(&g, false, true, 1000u);
    TEST_ASSERT_TRUE(ida_rx_gozetim_tamam(&g, true, 50000u, S_US));
    ida_rx_gozetim_guncelle(&g, true, false, 50000u);
    TEST_ASSERT_TRUE(ida_rx_gozetim_tamam(&g, true, 60000u, S_US));
    TEST_ASSERT_FALSE(ida_rx_gozetim_tamam(&g, true, 70001u, S_US));
}

static void test_saglik_kosullari(void)
{
    ida_saglik_girdi_t s = {true, true, 100000u, true, false, 110000u, S_US};
    TEST_ASSERT_EQUAL_HEX32(0u, ida_saglik_degerlendir(&s));
    s.core_ok = false;
    TEST_ASSERT_EQUAL_HEX32(IDA_SAGLIK_CORE_OK, ida_saglik_degerlendir(&s));
    s.core_ok = true;
    s.simdi_us = 130001u;
    TEST_ASSERT_EQUAL_HEX32(IDA_SAGLIK_CAGRI, ida_saglik_degerlendir(&s));
    s.cekirdek_normal = false; /* güvenli bekleme: çekirdek koşulları aranmaz */
    TEST_ASSERT_EQUAL_HEX32(0u, ida_saglik_degerlendir(&s));
    s.alim_durdu = true; /* bulgu 4: alım durmuş ve yeniden başlatılamamış */
    TEST_ASSERT_EQUAL_HEX32(IDA_SAGLIK_ALIM_DURDU, ida_saglik_degerlendir(&s));
}

/* Bulgu 6: bağlantı başlatılamadıysa (USART hazır olmadı) yeşil yanmaz, kırmızı yanar. */
static void test_sol6_baglanti_basarisiz_led(void)
{
    ida_led_t l = ida_led_durumu(true, false, true);
    TEST_ASSERT_FALSE(l.yesil);
    TEST_ASSERT_TRUE(l.kirmizi);
    l = ida_led_durumu(true, true, true);
    TEST_ASSERT_TRUE(l.yesil);
    TEST_ASSERT_FALSE(l.kirmizi);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_sol5_sarma_kayipsiz);
    RUN_TEST(test_sol5_sarma_kayipli);
    RUN_TEST(test_izgara_olagan);
    RUN_TEST(test_sol3_rx_tuketimi_durunca_beslenmez);
    RUN_TEST(test_rx_gozetim_ilerleme_ve_yeni_bekleme);
    RUN_TEST(test_rx_gozetim_yeni_bekleme_hemen_ret_edilmez);
    RUN_TEST(test_saglik_kosullari);
    RUN_TEST(test_sol6_baglanti_basarisiz_led);
    return UNITY_END();
}
