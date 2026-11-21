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
    TEST_ASSERT_TRUE_MESSAGE(n <= sizeof son, "taşıma: çerçeve boyu sınır dışı");
    if (n > sizeof son) {
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

/* MAVLink v2 biçiminde, sıra baytı seq olan sahte çerçeve. */
static void v2(uint8_t *b, size_t n, uint8_t seq)
{
    memset(b, 0x55, n);
    b[0] = 0xFDu;
    b[4] = seq;
}

/* Sol bulgu 1 (karşı örnek): kritik kuyruk dolunca çerçeve sessizce kaybolmaz; ret sayılır ve
   kalıcı kritik_hata kurulur (platform güvenli kapanışa geçer; A1-K110). */
static void test_kritik_ret_hata(void)
{
    uint8_t b[32];
    mesgul = true; /* taşıma durmuş */
    for (uint32_t i = 0u; i < IDA_GON_KRITIK_N; i++) {
        v2(b, sizeof b, (uint8_t)i);
        TEST_ASSERT_TRUE(ida_gon_ekle(&g, IDA_GON_SINIF_KRITIK, b, sizeof b, 0));
    }
    TEST_ASSERT_FALSE(ida_gon_kritik_hata(&g));
    v2(b, sizeof b, (uint8_t)IDA_GON_KRITIK_N);
    TEST_ASSERT_FALSE(ida_gon_ekle(&g, IDA_GON_SINIF_KRITIK, b, sizeof b, 0));
    TEST_ASSERT_EQUAL_UINT32(1u, g.kritik_ret);
    TEST_ASSERT_TRUE(ida_gon_kritik_hata(&g));
    /* Kalıcıdır: kuyruk boşalsa da kalkmaz. */
    mesgul = false;
    while (cikar(0) != 0u) {
    }
    TEST_ASSERT_TRUE(ida_gon_kritik_hata(&g));
}

/* Geçersiz çerçeve ayrı sayılır; kritik sınıfta geçersiz çerçeve de kritik hatadır. */
static void test_gecersiz_sayilir(void)
{
    uint8_t b[16];
    v2(b, sizeof b, 0u);
    TEST_ASSERT_FALSE(ida_gon_ekle(&g, IDA_GON_SINIF_ZORUNLU, b, 0, 0));
    TEST_ASSERT_FALSE(ida_gon_ekle(&g, 9u, b, sizeof b, 0));
    TEST_ASSERT_EQUAL_UINT32(2u, g.gecersiz_ret);
    TEST_ASSERT_FALSE(ida_gon_kritik_hata(&g));
    TEST_ASSERT_FALSE(ida_gon_ekle(&g, IDA_GON_SINIF_KRITIK, NULL, sizeof b, 0));
    TEST_ASSERT_TRUE(ida_gon_kritik_hata(&g));
}

/* Sıra sürekliliği: verilen (giriş), kuyruğa giren ve taşımaya verilen çerçevelerde ayrı ayrı;
   boşluk reddedilen ya da atılan çerçevenin sayacıyla açıklanır. */
static void test_sira_boslugu_sayacla_aciklanir(void)
{
    uint8_t b[32];
    uint8_t s = 250u; /* sarma da sınanır */
    mesgul = true;
    for (uint32_t i = 0u; i < IDA_GON_KRITIK_N; i++) {
        v2(b, sizeof b, s++);
        TEST_ASSERT_TRUE(ida_gon_ekle(&g, IDA_GON_SINIF_KRITIK, b, sizeof b, 0));
    }
    v2(b, sizeof b, s++); /* reddedilir */
    TEST_ASSERT_FALSE(ida_gon_ekle(&g, IDA_GON_SINIF_KRITIK, b, sizeof b, 0));
    v2(b, sizeof b, s++);
    TEST_ASSERT_TRUE(ida_gon_ekle(&g, IDA_GON_SINIF_ZORUNLU, b, sizeof b, 0));
    TEST_ASSERT_EQUAL_UINT32(0u, g.sira_giris.kesinti);  /* üst katman boşluk bırakmadı */
    TEST_ASSERT_EQUAL_UINT32(1u, g.sira_kuyruk.kesinti); /* ret: kritik_ret = 1 */
    TEST_ASSERT_EQUAL_UINT32(1u, g.sira_kuyruk.eksik);
    TEST_ASSERT_EQUAL_UINT32(g.kritik_ret + g.zorunlu_ret, g.sira_kuyruk.eksik);
    /* Üst katmanın atladığı numara giriş noktasında görünür. */
    s++;
    v2(b, sizeof b, s++);
    TEST_ASSERT_TRUE(ida_gon_ekle(&g, IDA_GON_SINIF_ZORUNLU, b, sizeof b, 0));
    TEST_ASSERT_EQUAL_UINT32(1u, g.sira_giris.kesinti);
    /* Taşımaya verilenler: kuyruktaki sırayla (öncelik sırası farklı olabilir: yalnız sayım). */
    mesgul = false;
    uint32_t verilen = 0u;
    while (cikar(0) != 0u) {
        verilen++;
    }
    TEST_ASSERT_EQUAL_UINT32(IDA_GON_KRITIK_N + 2u, verilen);
    TEST_ASSERT_EQUAL_UINT32(verilen, g.tx_frames);
}

/* (iii) atma ve temizleme taşıma noktasındaki boşluğu açıklar. */
static void test_atma_ve_temizleme_sayilir(void)
{
    uint8_t b[16];
    mesgul = true;
    for (uint32_t i = 0u; i < IDA_GON_KAYBED_N + 2u; i++) {
        v2(b, sizeof b, (uint8_t)i);
        TEST_ASSERT_TRUE(ida_gon_ekle(&g, IDA_GON_SINIF_KAYBED, b, sizeof b, 0));
    }
    TEST_ASSERT_EQUAL_UINT32(2u, g.tx_dropped);
    ida_gon_temizle(&g);
    TEST_ASSERT_EQUAL_UINT32(IDA_GON_KAYBED_N, g.temizlenen);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_kritik_ret_hata);
    RUN_TEST(test_gecersiz_sayilir);
    RUN_TEST(test_sira_boslugu_sayacla_aciklanir);
    RUN_TEST(test_atma_ve_temizleme_sayilir);
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
