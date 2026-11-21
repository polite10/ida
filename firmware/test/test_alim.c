/*
 * test_alim.c — dairesel DMA alım muhasebesi (firmware/link/alim.h) birim testleri.
 *
 * DMA, yazılan toplam bayttan üretilen yazmaç görüntüsüyle (BNDT, FIFOL) modellenir: halka boyu
 * n'de RAM konumu p = toplam mod n iken BNDT = n − p, FIFOL = 0. Zaman µs'dir; `simdi` son
 * görüntünün zamanıdır (doğrulama görüntüsü de o anda alınır).
 */
#include "alim.h"
#include "unity.h"

#include <string.h>

void setUp(void);
void tearDown(void);

#define N 64u
#define HIZ 300000u /* bayt/s */

static ida_alim_t a;
static uint8_t halka[N];
static uint64_t yazilan; /* modelin toplamı */
static uint64_t simdi;

void setUp(void)
{
    TEST_ASSERT_EQUAL_INT(1, ida_alim_kur(&a, N, 8u, HIZ));
    memset(halka, 0, sizeof halka);
    yazilan = 0u;
    simdi = 0u;
    /* İlk görüntü: kanal açıldı, BNDT = n. */
    TEST_ASSERT_EQUAL_INT(1, ida_alim_goruntu(&a, N, 0u, N, 0u, IDA_ALIM_OLAY_YOKLAMA));
}

void tearDown(void) {}

static uint32_t bndt(uint64_t toplam)
{
    return N - (uint32_t)(toplam % N);
}

/* Modele k bayt yazar (değer = toplam konumun düşük baytı). */
static void yaz(uint32_t k)
{
    for (uint32_t i = 0u; i < k; i++) {
        halka[yazilan % N] = (uint8_t)(yazilan & 0xFFu);
        yazilan++;
    }
}

static int olay(uint64_t t_us, uint8_t tur)
{
    const uint32_t b = bndt(yazilan);
    simdi = t_us;
    return ida_alim_goruntu(&a, b, 0u, b, t_us, tur);
}

/* Bir blok okur ve doğrular; kopyanın değerlerini (toplam konuma göre) denetler. */
static ida_alim_durum_t oku_dogrula_k(size_t azami, size_t *n, uint64_t *t, uint8_t *kaba)
{
    uint8_t hedef[N];
    uint64_t atilan = 0u;
    const uint64_t bas = a.okunan;
    ida_alim_durum_t d = ida_alim_oku(&a, halka, hedef, azami, n, t, kaba, &atilan);
    if (d != IDA_ALIM_VERI) {
        return d;
    }
    const uint32_t b = bndt(yazilan);
    d = ida_alim_dogrula(&a, *n, b, 0u, b, simdi, &atilan);
    if (d == IDA_ALIM_VERI) {
        for (size_t i = 0u; i < *n; i++) {
            TEST_ASSERT_EQUAL_HEX8((uint8_t)((bas + i) & 0xFFu), hedef[i]);
        }
    }
    return d;
}

static ida_alim_durum_t oku_dogrula(size_t azami, size_t *n, uint64_t *t)
{
    uint8_t kaba = 0u;
    return oku_dogrula_k(azami, n, t, &kaba);
}

static void test_kurulum_reddi(void)
{
    ida_alim_t k;
    TEST_ASSERT_EQUAL_INT(0, ida_alim_kur(&k, 100u, 8u, HIZ)); /* 2'nin kuvveti değil */
    TEST_ASSERT_EQUAL_INT(0, ida_alim_kur(&k, 32u, 8u, HIZ));  /* < 64 */
    TEST_ASSERT_EQUAL_INT(0, ida_alim_kur(&k, 64u, 64u, HIZ)); /* fifo >= n */
    TEST_ASSERT_EQUAL_INT(0, ida_alim_kur(&k, 64u, 8u, 0u));
    TEST_ASSERT_EQUAL_INT(0, ida_alim_kur(NULL, 64u, 8u, HIZ));
}

static void test_olay_siniri_ve_damga(void)
{
    size_t n = 0u;
    uint64_t t = 0u;
    uint8_t kaba = 1u;
    TEST_ASSERT_EQUAL_INT(IDA_ALIM_BOS, oku_dogrula(N, &n, &t)); /* ilk görüntü bayt kapsamaz */
    yaz(10u);
    TEST_ASSERT_EQUAL_INT(1, olay(100u, IDA_ALIM_OLAY_ESLESME));
    yaz(15u);
    TEST_ASSERT_EQUAL_INT(1, olay(200u, IDA_ALIM_OLAY_BOSTA));
    TEST_ASSERT_EQUAL_INT(IDA_ALIM_VERI, oku_dogrula_k(N, &n, &t, &kaba));
    TEST_ASSERT_EQUAL_UINT(10u, n);
    TEST_ASSERT_EQUAL_UINT64(100u, t);
    TEST_ASSERT_EQUAL_UINT8(0u, kaba);
    TEST_ASSERT_EQUAL_INT(IDA_ALIM_VERI, oku_dogrula_k(N, &n, &t, &kaba));
    TEST_ASSERT_EQUAL_UINT(15u, n);
    TEST_ASSERT_EQUAL_UINT64(200u, t);
    TEST_ASSERT_EQUAL_UINT8(0u, kaba);
    TEST_ASSERT_EQUAL_INT(IDA_ALIM_BOS, oku_dogrula(N, &n, &t));
    TEST_ASSERT_EQUAL_UINT32(1u, a.olay_sayisi[0]);
    TEST_ASSERT_EQUAL_UINT32(1u, a.olay_sayisi[1]);
}

static void test_azami_parcalar_ayni_damga(void)
{
    size_t n = 0u;
    uint64_t t = 0u;
    yaz(30u);
    TEST_ASSERT_EQUAL_INT(1, olay(50u, IDA_ALIM_OLAY_BOSTA));
    size_t toplam = 0u;
    for (int i = 0; i < 4; i++) {
        TEST_ASSERT_EQUAL_INT(IDA_ALIM_VERI, oku_dogrula(8u, &n, &t));
        TEST_ASSERT_EQUAL_UINT64(50u, t);
        toplam += n;
    }
    TEST_ASSERT_EQUAL_UINT(30u, toplam);
    TEST_ASSERT_EQUAL_INT(IDA_ALIM_BOS, oku_dogrula(8u, &n, &t));
}

static void test_halka_sarar(void)
{
    size_t n = 0u;
    uint64_t t = 0u;
    uint64_t okunan = 0u;
    for (uint32_t i = 1u; i <= 20u; i++) {
        yaz(23u);
        TEST_ASSERT_EQUAL_INT(1, olay(i * 100u, IDA_ALIM_OLAY_ESLESME));
        while (oku_dogrula(N, &n, &t) == IDA_ALIM_VERI) {
            okunan += n;
        }
    }
    TEST_ASSERT_EQUAL_UINT64(20u * 23u, okunan);
    TEST_ASSERT_EQUAL_UINT64(yazilan, a.toplam);
    TEST_ASSERT_EQUAL_UINT32(0u, a.tasma);
}

static void test_kucuk_geri_gidis_ilerleme_sayilmaz(void)
{
    yaz(30u);
    TEST_ASSERT_EQUAL_INT(1, olay(10u, IDA_ALIM_OLAY_ESLESME));
    /* BNDT'nin FIFOL'den önce güncellendiği ara durumun tersi: konum 1 geri görünür. */
    const uint32_t b = bndt(yazilan) + 1u;
    TEST_ASSERT_EQUAL_INT(1, ida_alim_goruntu(&a, b, 0u, b, 20u, IDA_ALIM_OLAY_BOSTA));
    TEST_ASSERT_EQUAL_UINT32(1u, a.geri);
    TEST_ASSERT_EQUAL_UINT64(30u, a.toplam);
    TEST_ASSERT_EQUAL_UINT32(0u, a.belirsiz);
}

static void test_uzun_sure_belirsiz(void)
{
    size_t n = 0u;
    uint64_t t = 0u;
    yaz(5u);
    TEST_ASSERT_EQUAL_INT(1, olay(10u, IDA_ALIM_OLAY_ESLESME));
    TEST_ASSERT_EQUAL_INT(IDA_ALIM_VERI, oku_dogrula(N, &n, &t));
    yaz(3u);
    /* 1 s sonra: o sürede n'den çok bayt gelebilirdi; tur sayısı bilinemez. */
    TEST_ASSERT_EQUAL_INT(1, olay(10u + 1000000u, IDA_ALIM_OLAY_BOSTA));
    TEST_ASSERT_EQUAL_UINT32(1u, a.belirsiz);
    uint8_t hedef[N];
    uint8_t kaba = 0u;
    uint64_t atilan = 0u;
    TEST_ASSERT_EQUAL_INT(IDA_ALIM_TASMA,
                          ida_alim_oku(&a, halka, hedef, N, &n, &t, &kaba, &atilan));
    TEST_ASSERT_EQUAL_UINT64(3u, atilan);
    TEST_ASSERT_EQUAL_UINT32(1u, a.tasma);
    TEST_ASSERT_EQUAL_INT(IDA_ALIM_BOS, oku_dogrula(N, &n, &t));
}

static void test_ucte_dort_ilerleme_belirsiz(void)
{
    yaz(50u); /* 50 >= 3n/4 = 48, aradaki görüntüler yok */
    TEST_ASSERT_EQUAL_INT(1, olay(20u, IDA_ALIM_OLAY_TAM));
    TEST_ASSERT_EQUAL_UINT32(1u, a.belirsiz);
}

static void test_okunmamis_uzerine_yazma(void)
{
    size_t n = 0u;
    uint64_t t = 0u;
    yaz(20u);
    TEST_ASSERT_EQUAL_INT(1, olay(100u, IDA_ALIM_OLAY_ESLESME));
    TEST_ASSERT_EQUAL_INT(IDA_ALIM_VERI, oku_dogrula(N, &n, &t));
    yaz(40u);
    TEST_ASSERT_EQUAL_INT(1, olay(200u, IDA_ALIM_OLAY_ESLESME));
    yaz(40u);
    TEST_ASSERT_EQUAL_INT(1, olay(300u, IDA_ALIM_OLAY_ESLESME));
    yaz(40u);
    TEST_ASSERT_EQUAL_INT(1, olay(400u, IDA_ALIM_OLAY_ESLESME));
    TEST_ASSERT_EQUAL_UINT32(0u, a.belirsiz);
    /* Bekleyen: konum 60'ın olayı (40 bayt); ama yazılan (140) − okunan (20) = 120 > n:
       kopya sürerken üzerine yazılmış sayılır. */
    uint8_t hedef[N];
    uint8_t kaba = 0u;
    uint64_t atilan = 0u;
    TEST_ASSERT_EQUAL_INT(IDA_ALIM_VERI, ida_alim_oku(&a, halka, hedef, N, &n, &t, &kaba, &atilan));
    const uint32_t b = bndt(yazilan);
    TEST_ASSERT_EQUAL_INT(IDA_ALIM_TASMA, ida_alim_dogrula(&a, n, b, 0u, b, simdi, &atilan));
    TEST_ASSERT_EQUAL_UINT64(120u, atilan);
    TEST_ASSERT_EQUAL_UINT64(yazilan, a.okunan);
    /* Atılan konuma kadar olan olaylar sessizce tüketilir. */
    TEST_ASSERT_EQUAL_INT(IDA_ALIM_BOS, oku_dogrula(N, &n, &t));
}

static void test_olay_konumu_n_den_buyuk(void)
{
    size_t n = 0u;
    uint64_t t = 0u;
    for (uint32_t i = 1u; i <= 4u; i++) {
        yaz(20u);
        TEST_ASSERT_EQUAL_INT(1, olay(i * 100u, IDA_ALIM_OLAY_ESLESME));
    }
    /* İlk üç olayı (okunmadan) atla: son olaya (80) kadar 80 > n bayt bekler. */
    a.son = a.son + 3u;
    uint8_t hedef[N];
    uint8_t kaba = 0u;
    uint64_t atilan = 0u;
    TEST_ASSERT_EQUAL_INT(IDA_ALIM_TASMA,
                          ida_alim_oku(&a, halka, hedef, N, &n, &t, &kaba, &atilan));
    TEST_ASSERT_EQUAL_UINT64(80u, atilan);
}

static void test_kararsiz_goruntu(void)
{
    TEST_ASSERT_EQUAL_INT(0, ida_alim_goruntu(&a, 10u, 0u, 11u, 5u, IDA_ALIM_OLAY_BOSTA));
    TEST_ASSERT_EQUAL_UINT32(1u, a.kararsiz);
    TEST_ASSERT_EQUAL_INT(0, ida_alim_goruntu(&a, N + 1u, 0u, N + 1u, 5u, IDA_ALIM_OLAY_BOSTA));
    TEST_ASSERT_EQUAL_UINT32(1u, a.gecersiz);
    /* Kararsız doğrulama görüntüsü: kopya taahhüt edilmez (BOS), okunan değişmez. */
    yaz(4u);
    TEST_ASSERT_EQUAL_INT(1, olay(10u, IDA_ALIM_OLAY_ESLESME));
    uint8_t hedef[N];
    size_t n = 0u;
    uint64_t t = 0u, atilan = 0u;
    uint8_t kaba = 0u;
    TEST_ASSERT_EQUAL_INT(IDA_ALIM_VERI, ida_alim_oku(&a, halka, hedef, N, &n, &t, &kaba, &atilan));
    TEST_ASSERT_EQUAL_INT(IDA_ALIM_BOS, ida_alim_dogrula(&a, n, 10u, 0u, 11u, 10u, &atilan));
    TEST_ASSERT_EQUAL_UINT64(0u, a.okunan);
}

static void test_olay_halkasi_dolu_birlesir(void)
{
    size_t n = 0u;
    uint64_t t = 0u;
    uint8_t kaba = 0u;
    for (uint32_t i = 1u; i <= IDA_ALIM_OLAY_N + 4u; i++) {
        yaz(i % 4u == 0u ? 1u : 0u);
        TEST_ASSERT_EQUAL_INT(1, olay(i * 10u, IDA_ALIM_OLAY_ESLESME));
    }
    /* 68 olay, 64 yuva: son 4 olay kaydedilemedi. */
    TEST_ASSERT_EQUAL_UINT32(4u, a.olay_birlesme);
    uint64_t okunan = 0u;
    uint8_t son_kaba = 0u;
    while (oku_dogrula_k(N, &n, &t, &kaba) == IDA_ALIM_VERI) {
        okunan += n;
        son_kaba = kaba;
    }
    kaba = son_kaba;
    TEST_ASSERT_EQUAL_UINT64(yazilan, okunan);
    /* A1-K48: yer olmadığı için kaydedilemeyen olaylardan önceki baytlar, kendilerinden önceki
       en son kayıtlı olayın damgasını alır (64. olay, t = 640) ve kaba işaretlenir; daha yeni
       zaman üretilmez. */
    TEST_ASSERT_EQUAL_UINT64(IDA_ALIM_OLAY_N * 10u, t);
    TEST_ASSERT_EQUAL_UINT8(1u, kaba);
}

/* ---- sol bulgu 1 ve 2'nin karşı örnekleri (N = 16384) ---- */
#define NB 16384u
static ida_alim_t ab;
static uint8_t halka_b[NB];
static uint64_t yazilan_b;

static void b_kur(void)
{
    TEST_ASSERT_EQUAL_INT(1, ida_alim_kur(&ab, NB, 255u, 331000u));
    yazilan_b = 0u;
    TEST_ASSERT_EQUAL_INT(1, ida_alim_goruntu(&ab, NB, 0u, NB, 0u, IDA_ALIM_OLAY_YOKLAMA));
}

static void b_yaz(uint32_t k)
{
    for (uint32_t i = 0u; i < k; i++) {
        halka_b[yazilan_b % NB] = (uint8_t)(yazilan_b & 0xFFu);
        yazilan_b++;
    }
}

static uint32_t b_bndt(void)
{
    return NB - (uint32_t)(yazilan_b % NB);
}

/* Bulgu 1, karşı örnek 1: konum 100 olayı okunmadan beklerken 16 387 bayt daha yazılır
   (gerçek toplam 16 487, fiziksel konum 103). Eski olayın 100 baytının hepsinin üzerine
   yazılmıştır: hiçbiri geçerli blok olarak teslim edilmemeli; taşma bildirilmeli. */
static void test_sol1_tur_kaybi_eski_olay_teslim_edilmez(void)
{
    b_kur();
    b_yaz(100u);
    uint32_t bb = b_bndt();
    TEST_ASSERT_EQUAL_INT(1, ida_alim_goruntu(&ab, bb, 0u, bb, 1000u, IDA_ALIM_OLAY_ESLESME));
    b_yaz(16387u);
    bb = b_bndt();
    TEST_ASSERT_EQUAL_INT(1, ida_alim_goruntu(&ab, bb, 0u, bb, 1000u + 55000u, IDA_ALIM_OLAY_BOSTA));
    static uint8_t hedef[NB];
    size_t n = 0u;
    uint64_t t = 0u, atilan = 0u;
    uint8_t kaba = 0u;
    ida_alim_durum_t d = ida_alim_oku(&ab, halka_b, hedef, sizeof hedef, &n, &t, &kaba, &atilan);
    if (d == IDA_ALIM_VERI) {
        d = ida_alim_dogrula(&ab, n, bb, 0u, bb, 1000u + 55000u, &atilan);
    }
    TEST_ASSERT_EQUAL_INT(IDA_ALIM_TASMA, d);
    /* Sonraki okuma eski olayları teslim etmez. */
    TEST_ASSERT_EQUAL_INT(IDA_ALIM_BOS,
                          ida_alim_oku(&ab, halka_b, hedef, sizeof hedef, &n, &t, &kaba, &atilan));
}

/* Bulgu 1, karşı örnek 2: önceki konum 100, uzun aralıkta gerçek ilerleme 16 383 bayt (yeni
   konum 99). Geri gidiş sayılmamalı; tur sayısı belirsizdir. */
static void test_sol1_uzun_aralikta_n_eksi_1_ilerleme(void)
{
    b_kur();
    b_yaz(100u);
    uint32_t bb = b_bndt();
    TEST_ASSERT_EQUAL_INT(1, ida_alim_goruntu(&ab, bb, 0u, bb, 1000u, IDA_ALIM_OLAY_ESLESME));
    b_yaz(16383u);
    bb = b_bndt();
    TEST_ASSERT_EQUAL_INT(1, ida_alim_goruntu(&ab, bb, 0u, bb, 1000u + 60000u, IDA_ALIM_OLAY_BOSTA));
    TEST_ASSERT_EQUAL_UINT32(0u, ab.geri);
    TEST_ASSERT_EQUAL_UINT32(1u, ab.belirsiz);
}

/* Bulgu 1: kopya doğrulaması da süre denetimi yapmalı. Olay okunurken (kesme görüntüsü
   gelmeden) DMA bir tur + 3 bayt ilerlemiştir; doğrulama taşmayı yakalamalı. */
static void test_sol1_anlik_kestirimde_tur_kaybi(void)
{
    b_kur();
    b_yaz(100u);
    uint32_t bb = b_bndt();
    TEST_ASSERT_EQUAL_INT(1, ida_alim_goruntu(&ab, bb, 0u, bb, 1000u, IDA_ALIM_OLAY_ESLESME));
    static uint8_t hedef[NB];
    size_t n = 0u;
    uint64_t t = 0u, atilan = 0u;
    uint8_t kaba = 0u;
    TEST_ASSERT_EQUAL_INT(IDA_ALIM_VERI,
                          ida_alim_oku(&ab, halka_b, hedef, sizeof hedef, &n, &t, &kaba, &atilan));
    b_yaz(NB + 3u);
    bb = b_bndt();
    TEST_ASSERT_EQUAL_INT(IDA_ALIM_TASMA,
                          ida_alim_dogrula(&ab, n, bb, 0u, bb, 1000u + 55000u, &atilan));
}

/* Bulgu 2: olay gelmeden ana bağlamın yoklamasıyla bulunan baytlar daha geç bir zaman almaz;
   önceki en son olayın damgasını alır ve kaba işaretlenir (A1-K48). */
static void test_sol2_yoklama_onceki_damgayi_verir(void)
{
    size_t n = 0u;
    uint64_t t = 0u;
    uint8_t kaba = 1u;
    yaz(10u);
    TEST_ASSERT_EQUAL_INT(1, olay(100u, IDA_ALIM_OLAY_ESLESME));
    yaz(5u);
    TEST_ASSERT_EQUAL_INT(1, olay(150u, IDA_ALIM_OLAY_YOKLAMA));
    TEST_ASSERT_EQUAL_INT(IDA_ALIM_VERI, oku_dogrula_k(N, &n, &t, &kaba));
    TEST_ASSERT_EQUAL_UINT64(100u, t);
    TEST_ASSERT_EQUAL_UINT8(0u, kaba);
    TEST_ASSERT_EQUAL_INT(IDA_ALIM_VERI, oku_dogrula_k(N, &n, &t, &kaba));
    TEST_ASSERT_EQUAL_UINT(5u, n);
    TEST_ASSERT_EQUAL_UINT64(100u, t);
    TEST_ASSERT_EQUAL_UINT8(1u, kaba);
}

/* Bulgu 4: DMA alım hatasından sonra denetimli yeniden başlatma. Bekleyen veri atılır
   (TASMA), toplam bir sonraki n katına yuvarlanır ve yeni kanal halkanın başından yazınca
   veri doğru konumdan okunur. */
static void test_sol4_yeniden_baslatma(void)
{
    size_t n = 0u;
    uint64_t t = 0u;
    yaz(10u);
    TEST_ASSERT_EQUAL_INT(1, olay(100u, IDA_ALIM_OLAY_ESLESME));
    yaz(7u); /* kanal durmadan önce yazıldı, olay yok */
    ida_alim_yeniden_baslat(&a, 200u);
    TEST_ASSERT_TRUE(ida_alim_bekleyen(&a));
    uint8_t hedef[N];
    uint8_t kaba = 0u;
    uint64_t atilan = 0u;
    TEST_ASSERT_EQUAL_INT(IDA_ALIM_TASMA,
                          ida_alim_oku(&a, halka, hedef, N, &n, &t, &kaba, &atilan));
    TEST_ASSERT_EQUAL_UINT64(N, atilan); /* 0..64: bekleyen 10 + kanal halkası yeniden */
    TEST_ASSERT_EQUAL_UINT64(N, a.okunan);
    /* Yeni kanal halkanın başından yazar: model toplamı da n katına atlar. */
    yazilan = N;
    yaz(6u);
    TEST_ASSERT_EQUAL_INT(1, olay(210u, IDA_ALIM_OLAY_BOSTA));
    TEST_ASSERT_EQUAL_INT(IDA_ALIM_VERI, oku_dogrula(N, &n, &t));
    TEST_ASSERT_EQUAL_UINT(6u, n);
    TEST_ASSERT_EQUAL_UINT32(1u, a.yeniden);
}

/* Rastgele akış: DMA sürekli yazar; kesme görüntüleri (bazıları geçici olarak bir bayt fazla
   gösterir: BNDT/FIFOL ara durumu), ana bağlamın okuma+doğrulaması ve yoklamalar karışık sırada
   gelir. Teslim edilen her bayt, yazıldığı konumdaki değerdir (değer = konumun düşük baytı) ve
   konumlar sırayla artar; atılan bayt olmaz (halka hiç taşmaz). */
static uint32_t rnd_durum = 12345u;
static uint32_t rnd(uint32_t ust)
{
    rnd_durum = rnd_durum * 1103515245u + 12345u;
    return (rnd_durum >> 16) % ust;
}

static void test_rastgele_akis_sirali_teslim(void)
{
    b_kur();
    uint64_t t = 0u;
    uint64_t beklenen = 0u; /* sıradaki teslim edilecek konum */
    static uint8_t hedef[1024];
    for (uint32_t adim = 0u; adim < 200000u; adim++) {
        t += 1u + rnd(5u);
        const uint32_t secim = rnd(10u);
        if (secim < 4u) {
            b_yaz(rnd(60u));
        } else if (secim < 7u) {
            /* kesme görüntüsü; %10 olasılıkla bir bayt fazla (geçici) */
            const uint32_t fazla = rnd(10u) == 0u ? 1u : 0u;
            const uint32_t bb = NB - (uint32_t)((yazilan_b + fazla) % NB);
            (void)ida_alim_goruntu(&ab, bb, 0u, bb, t, IDA_ALIM_OLAY_ESLESME);
            if (fazla) {
                b_yaz(1u); /* yoldaki bayt hemen belleğe iner */
            }
        } else if (secim < 8u) {
            const uint32_t bb = b_bndt();
            (void)ida_alim_goruntu(&ab, bb, 0u, bb, t, IDA_ALIM_OLAY_YOKLAMA);
        } else {
            size_t n = 0u;
            uint64_t ts = 0u, atilan = 0u;
            uint8_t kaba = 0u;
            const uint64_t bas = ab.okunan;
            ida_alim_durum_t d = ida_alim_oku(&ab, halka_b, hedef, sizeof hedef, &n, &ts, &kaba,
                                              &atilan);
            TEST_ASSERT_NOT_EQUAL_INT(IDA_ALIM_TASMA, d);
            if (d == IDA_ALIM_VERI) {
                const uint32_t fazla = rnd(10u) == 0u ? 1u : 0u;
                const uint32_t bb = NB - (uint32_t)((yazilan_b + fazla) % NB);
                d = ida_alim_dogrula(&ab, n, bb, 0u, bb, t, &atilan);
                if (fazla) {
                    b_yaz(1u);
                }
                TEST_ASSERT_NOT_EQUAL_INT(IDA_ALIM_TASMA, d);
                if (d == IDA_ALIM_VERI) {
                    TEST_ASSERT_EQUAL_UINT64(beklenen, bas);
                    TEST_ASSERT_TRUE(bas + n <= yazilan_b + 1u);
                    for (size_t i = 0u; i < n; i++) {
                        TEST_ASSERT_EQUAL_HEX8((uint8_t)((bas + i) & 0xFFu), hedef[i]);
                    }
                    beklenen = bas + n;
                }
            }
        }
        /* halka taşmasın: okuyucu geride kaldıysa yazmayı durdur */
        uint32_t sinir = 0u;
        while (yazilan_b > ab.okunan && yazilan_b - ab.okunan > NB / 2u) {
            TEST_ASSERT_TRUE(++sinir < 1000u);
            size_t n = 0u;
            uint64_t ts = 0u, atilan = 0u;
            uint8_t kaba = 0u;
            const uint32_t bb = b_bndt();
            t += 1u;
            (void)ida_alim_goruntu(&ab, bb, 0u, bb, t, IDA_ALIM_OLAY_BOSTA);
            const uint64_t bas = ab.okunan;
            if (ida_alim_oku(&ab, halka_b, hedef, sizeof hedef, &n, &ts, &kaba, &atilan) ==
                    IDA_ALIM_VERI &&
                ida_alim_dogrula(&ab, n, bb, 0u, bb, t, &atilan) == IDA_ALIM_VERI) {
                TEST_ASSERT_EQUAL_UINT64(beklenen, bas);
                beklenen = bas + n;
            }
        }
    }
    TEST_ASSERT_EQUAL_UINT32(0u, ab.tasma);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_kurulum_reddi);
    RUN_TEST(test_olay_siniri_ve_damga);
    RUN_TEST(test_azami_parcalar_ayni_damga);
    RUN_TEST(test_halka_sarar);
    RUN_TEST(test_kucuk_geri_gidis_ilerleme_sayilmaz);
    RUN_TEST(test_uzun_sure_belirsiz);
    RUN_TEST(test_ucte_dort_ilerleme_belirsiz);
    RUN_TEST(test_okunmamis_uzerine_yazma);
    RUN_TEST(test_olay_konumu_n_den_buyuk);
    RUN_TEST(test_kararsiz_goruntu);
    RUN_TEST(test_olay_halkasi_dolu_birlesir);
    RUN_TEST(test_sol1_tur_kaybi_eski_olay_teslim_edilmez);
    RUN_TEST(test_sol1_uzun_aralikta_n_eksi_1_ilerleme);
    RUN_TEST(test_sol1_anlik_kestirimde_tur_kaybi);
    RUN_TEST(test_sol2_yoklama_onceki_damgayi_verir);
    RUN_TEST(test_sol4_yeniden_baslatma);
    RUN_TEST(test_rastgele_akis_sirali_teslim);
    return UNITY_END();
}
