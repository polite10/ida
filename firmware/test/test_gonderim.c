/*
 * test_gonderim.c — üç sınıflı gönderim yöneticisi (firmware/app/gonderim.h) birim testleri
 * (A1-K10, A1-K110).
 */
#include "gonderim.h"
#include "unity.h"

#include <string.h>

void setUp(void);
void tearDown(void);

static ida_gonderim_t g;

/* Sahte taşıma: tek çerçeve yuvası; bos() yazılan çerçeve "çıkana" kadar false. */
static bool mesgul;
static uint8_t son[IDA_GON_CERCEVE_AZAMI];
static size_t son_n;
static uint32_t yazilan;
static bool yaz_reddet;

static bool t_bos(void *ctx)
{
    (void)ctx;
    return !mesgul;
}

static bool t_yaz(void *ctx, const uint8_t *b, size_t n)
{
    (void)ctx;
    if (yaz_reddet) {
        return false;
    }
    memcpy(son, b, n);
    son_n = n;
    mesgul = true;
    yazilan++;
    return true;
}

static const ida_gon_tasima_t T = {NULL, t_bos, t_yaz};

void setUp(void)
{
    ida_gon_kur(&g);
    mesgul = false;
    son_n = 0u;
    yazilan = 0u;
    yaz_reddet = false;
}

void tearDown(void) {}

static void cerceve(uint8_t etiket, size_t n, uint8_t *b)
{
    memset(b, etiket, n);
}

/* Pompa sürücüyü boşaltarak bir çerçeve çıkarır; sınıfını döndürür. */
static uint32_t cikar(uint64_t t)
{
    mesgul = false;
    return ida_gon_pompa(&g, &T, t);
}

static void test_oncelik_sirasi(void)
{
    uint8_t b[64];
    cerceve(3, 40, b);
    TEST_ASSERT_TRUE(ida_gon_ekle(&g, IDA_GON_SINIF_KAYBED, b, 40, 0));
    cerceve(2, 30, b);
    TEST_ASSERT_TRUE(ida_gon_ekle(&g, IDA_GON_SINIF_ZORUNLU, b, 30, 0));
    cerceve(1, 20, b);
    TEST_ASSERT_TRUE(ida_gon_ekle(&g, IDA_GON_SINIF_KRITIK, b, 20, 0));
    TEST_ASSERT_EQUAL_UINT32(1u, cikar(10));
    TEST_ASSERT_EQUAL_UINT8(1u, son[0]);
    TEST_ASSERT_EQUAL_UINT(20u, son_n);
    TEST_ASSERT_EQUAL_UINT32(2u, cikar(20));
    TEST_ASSERT_EQUAL_UINT32(3u, cikar(30));
    TEST_ASSERT_EQUAL_UINT32(0u, cikar(40));
    TEST_ASSERT_EQUAL_UINT32(3u, g.tx_frames);
}

/* Sürücüye aynı anda tek çerçeve: önceki çıkmadan (bos() false) yenisi verilmez. */
static void test_surucuye_tek_cerceve(void)
{
    uint8_t b[16];
    cerceve(1, 16, b);
    TEST_ASSERT_TRUE(ida_gon_ekle(&g, IDA_GON_SINIF_ZORUNLU, b, 16, 0));
    TEST_ASSERT_TRUE(ida_gon_ekle(&g, IDA_GON_SINIF_ZORUNLU, b, 16, 0));
    TEST_ASSERT_EQUAL_UINT32(2u, ida_gon_pompa(&g, &T, 0));
    TEST_ASSERT_EQUAL_UINT32(0u, ida_gon_pompa(&g, &T, 0)); /* sürücü meşgul */
    TEST_ASSERT_EQUAL_UINT32(1u, yazilan);
    TEST_ASSERT_EQUAL_UINT32(2u, cikar(0));
}

/* (ii) atılmaz: Q_ii dolunca false (oturum TX_OVERFLOW ile biter). */
static void test_zorunlu_tasma_false(void)
{
    uint8_t b[16];
    cerceve(2, 16, b);
    for (uint32_t i = 0u; i < IDA_GON_ZORUNLU_N; i++) {
        TEST_ASSERT_TRUE(ida_gon_ekle(&g, IDA_GON_SINIF_ZORUNLU, b, 16, 0));
    }
    TEST_ASSERT_FALSE(ida_gon_ekle(&g, IDA_GON_SINIF_ZORUNLU, b, 16, 0));
    TEST_ASSERT_EQUAL_UINT32(1u, g.zorunlu_ret);
    TEST_ASSERT_EQUAL_UINT32(IDA_GON_ZORUNLU_N, ida_gon_bekleyen(&g, IDA_GON_SINIF_ZORUNLU));
}

/* (iii) dolunca en eskisi atılır ve sayılır. */
static void test_kaybedilebilir_en_eski_atilir(void)
{
    uint8_t b[16];
    for (uint32_t i = 0u; i < IDA_GON_KAYBED_N + 3u; i++) {
        cerceve((uint8_t)i, 16, b);
        TEST_ASSERT_TRUE(ida_gon_ekle(&g, IDA_GON_SINIF_KAYBED, b, 16, 0));
    }
    TEST_ASSERT_EQUAL_UINT32(3u, g.tx_dropped);
    TEST_ASSERT_EQUAL_UINT32(3u, cikar(0));
    TEST_ASSERT_EQUAL_UINT8(3u, son[0]); /* 0, 1, 2 atıldı */
}

/* (iii) periyot başına R_iii bayt; periyot sınırında yenilenir. */
static void test_kaybedilebilir_butce(void)
{
    uint8_t b[200];
    cerceve(3, 200, b);
    for (int i = 0; i < 4; i++) {
        TEST_ASSERT_TRUE(ida_gon_ekle(&g, IDA_GON_SINIF_KAYBED, b, 200, 0));
    }
    TEST_ASSERT_EQUAL_UINT32(3u, cikar(0));
    TEST_ASSERT_EQUAL_UINT32(3u, cikar(0));
    TEST_ASSERT_EQUAL_UINT32(0u, cikar(0)); /* 400 + 200 > 500 */
    ida_gon_periyot(&g, 5000);
    TEST_ASSERT_EQUAL_UINT32(3u, cikar(5000));
    /* Bütçe (ii)'yi ve (i)'i etkilemez. */
    TEST_ASSERT_TRUE(ida_gon_ekle(&g, IDA_GON_SINIF_ZORUNLU, b, 200, 0));
    TEST_ASSERT_EQUAL_UINT32(2u, cikar(5000));
}

static void test_temizle_ve_gecersiz(void)
{
    uint8_t b[16];
    cerceve(1, 16, b);
    TEST_ASSERT_TRUE(ida_gon_ekle(&g, IDA_GON_SINIF_KRITIK, b, 16, 0));
    TEST_ASSERT_TRUE(ida_gon_ekle(&g, IDA_GON_SINIF_ZORUNLU, b, 16, 0));
    ida_gon_temizle(&g);
    TEST_ASSERT_EQUAL_UINT32(0u, cikar(0));
    TEST_ASSERT_FALSE(ida_gon_ekle(&g, 0u, b, 16, 0));
    TEST_ASSERT_FALSE(ida_gon_ekle(&g, 4u, b, 16, 0));
    TEST_ASSERT_FALSE(ida_gon_ekle(&g, IDA_GON_SINIF_KRITIK, b, 0, 0));
    TEST_ASSERT_FALSE(ida_gon_ekle(&g, IDA_GON_SINIF_KRITIK, b, IDA_GON_CERCEVE_AZAMI + 1u, 0));
}

/* Kritik çerçeve bir periyottan uzun bekledi: tx_crit_stall ve tx_stalled. */
static void test_kritik_bekleme(void)
{
    uint8_t b[16];
    cerceve(1, 16, b);
    TEST_ASSERT_TRUE(ida_gon_ekle(&g, IDA_GON_SINIF_KRITIK, b, 16, 1000));
    mesgul = true; /* taşıma boşalmıyor */
    ida_gon_periyot(&g, 3000);
    TEST_ASSERT_FALSE(g.tx_stalled);
    ida_gon_periyot(&g, 7000);
    TEST_ASSERT_TRUE(g.tx_stalled);
    TEST_ASSERT_EQUAL_UINT32(1u, g.tx_crit_stall);
    TEST_ASSERT_EQUAL_UINT32(1u, cikar(9000));
    TEST_ASSERT_EQUAL_UINT32(8000u, g.kritik_bekleme_en_kotu_us);
    ida_gon_periyot(&g, 12000);
    TEST_ASSERT_FALSE(g.tx_stalled);
}

static void test_surucu_reddi(void)
{
    uint8_t b[16];
    cerceve(1, 16, b);
    TEST_ASSERT_TRUE(ida_gon_ekle(&g, IDA_GON_SINIF_KRITIK, b, 16, 0));
    yaz_reddet = true;
    TEST_ASSERT_EQUAL_UINT32(0u, cikar(0));
    TEST_ASSERT_EQUAL_UINT32(1u, g.yazma_ret);
    yaz_reddet = false;
    TEST_ASSERT_EQUAL_UINT32(1u, cikar(0)); /* çerçeve kaybolmadı */
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_oncelik_sirasi);
    RUN_TEST(test_surucuye_tek_cerceve);
    RUN_TEST(test_zorunlu_tasma_false);
    RUN_TEST(test_kaybedilebilir_en_eski_atilir);
    RUN_TEST(test_kaybedilebilir_butce);
    RUN_TEST(test_temizle_ve_gecersiz);
    RUN_TEST(test_kritik_bekleme);
    RUN_TEST(test_surucu_reddi);
    return UNITY_END();
}
