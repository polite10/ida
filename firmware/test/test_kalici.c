/*
 * test_kalici.c — açılış sayacı ve parametre A/B blokları (firmware/app/kalici.h) birim
 * testleri.
 *
 * Flash, RAM'de 32 KB'lık bir modeldir (sol bulgu 7):
 *   - quad-word başına yazılmışlık bilgisi: silinmemiş (programlanmış) quad-word'e yazım hata
 *     verir; tamamen 0xFF yazılan quad-word de "yazılmış"tır;
 *   - BUSY: her işlem durum()'da birkaç kez "sürüyor" döner;
 *   - kesinti enjeksiyonu: seçilen işlem yarıda kalır (yarım program: quad-word'ün yarısı yeni,
 *     yazılmış ve ECC'si bozuk; yarım silme: sayfanın yarısı silinmiş, kalanı eski veri ve ECC'si
 *     bozuk), model "güç kesildi" durumuna geçer; test aynı bellekle yeniden açılışı taklit eder;
 *   - ECC: bozuk quad-word okunduğunda çift hata (yutulan NMI) bildirilir (ecc_bozuk()).
 * Bu testler mantığın kanıtıdır; donanımdaki kesinti davranışının kanıtı değildir.
 */
#include "kalici.h"
#include "unity.h"

#include <string.h>

void setUp(void);
void tearDown(void);

#define BELLEK_BAYT (IDA_KAL_SAYFA_N * IDA_KAL_SAYFA_BAYT)
#define QW_N (BELLEK_BAYT / IDA_KAL_QW)
#define SAYFA_QW (IDA_KAL_SAYFA_BAYT / IDA_KAL_QW)

static uint8_t bellek[BELLEK_BAYT];
static uint8_t yazildi[QW_N]; /* 1: programlandı (veri 0xFF olsa da) */
static uint8_t ecc[QW_N];     /* 1: okunduğunda çift ECC hatası */
static int yazma_hatasi;      /* 1: sonraki yaz16 başlatılamaz */
static uint32_t silme, yazma;
static uint32_t islem_no;     /* başlatılan işlem sayısı */
static int64_t kes_islem;     /* bu numaralı işlem yarıda kesilir; -1: yok */
static int olu;               /* güç kesildi: işlem yok */
static int mesgul_kalan;
static int bekleyen;          /* 0 yok, 1 silme, 2 yazma */
static uint32_t bek_sayfa, bek_ofs;
static uint8_t bek_q[IDA_KAL_QW];
static int busy_n;            /* her işlemin durum()'da "sürüyor" döndüğü yoklama sayısı */
static ida_kalici_t k;

static bool sinirda(uint32_t ofs, uint32_t n)
{
    const bool ic = ofs <= BELLEK_BAYT && n <= BELLEK_BAYT - ofs;
    TEST_ASSERT_TRUE_MESSAGE(ic, "flash modeli: sınır dışı erişim");
    return ic;
}

static const uint8_t *m_taban(void *ctx)
{
    (void)ctx;
    return bellek;
}

static void sayfa_sil(uint32_t s, uint32_t qw_bas, uint32_t qw_son)
{
    for (uint32_t i = qw_bas; i < qw_son && i < SAYFA_QW; i++) {
        const uint32_t q = s * SAYFA_QW + i;
        if (q < QW_N) {
            memset(&bellek[q * IDA_KAL_QW], 0xFF, IDA_KAL_QW);
            yazildi[q] = 0u;
            ecc[q] = 0u;
        }
    }
}

/* Kesilen işlemin yarım etkisi. */
static void yarim_etki(void)
{
    if (bekleyen == 1) {
        /* Yarım silme: ilk yarı silinmiş; kalan eski veri ECC'si bozuk. */
        sayfa_sil(bek_sayfa, 0u, SAYFA_QW / 2u);
        for (uint32_t i = SAYFA_QW / 2u; i < SAYFA_QW; i++) {
            const uint32_t q = bek_sayfa * SAYFA_QW + i;
            if (q < QW_N) {
                ecc[q] = 1u;
            }
        }
    } else if (bekleyen == 2) {
        /* Yarım program: ilk 8 bayt yeni, kalanı 0xFF; programlanmış ve ECC'si bozuk. */
        const uint32_t q = bek_ofs / IDA_KAL_QW;
        if (q < QW_N && sinirda(bek_ofs, IDA_KAL_QW) && bek_ofs <= BELLEK_BAYT - IDA_KAL_QW) {
            memcpy(&bellek[bek_ofs], bek_q, IDA_KAL_QW / 2u);
            memset(&bellek[bek_ofs + IDA_KAL_QW / 2u], 0xFF, IDA_KAL_QW / 2u);
            yazildi[q] = 1u;
            ecc[q] = 1u;
        }
    }
    bekleyen = 0;
    olu = 1;
}

static int islem_baslat(void)
{
    const uint32_t no = islem_no++;
    mesgul_kalan = busy_n;
    if (kes_islem >= 0 && (int64_t)no == kes_islem) {
        yarim_etki();
    }
    return 0;
}

static int m_sil(void *ctx, uint32_t s)
{
    (void)ctx;
    if (olu || bekleyen != 0 || s >= IDA_KAL_SAYFA_N) {
        return -1;
    }
    bekleyen = 1;
    bek_sayfa = s;
    silme++;
    return islem_baslat();
}

static int m_yaz16(void *ctx, uint32_t ofs, const uint8_t q[IDA_KAL_QW])
{
    (void)ctx;
    if (olu || bekleyen != 0 || yazma_hatasi || (ofs % IDA_KAL_QW) != 0u) {
        return -1;
    }
    if (!sinirda(ofs, IDA_KAL_QW) || ofs > BELLEK_BAYT - IDA_KAL_QW) {
        return -1;
    }
    bekleyen = 2;
    bek_ofs = ofs;
    memcpy(bek_q, q, IDA_KAL_QW);
    yazma++;
    return islem_baslat();
}

static int m_durum(void *ctx)
{
    (void)ctx;
    if (olu) {
        return -1;
    }
    if (bekleyen == 0) {
        return 0;
    }
    if (mesgul_kalan > 0) {
        mesgul_kalan--;
        return 1;
    }
    const int b = bekleyen;
    bekleyen = 0;
    if (b == 1) {
        sayfa_sil(bek_sayfa, 0u, SAYFA_QW);
        return 0;
    }
    const uint32_t q = bek_ofs / IDA_KAL_QW;
    if (q >= QW_N || yazildi[q] != 0u) {
        return -1; /* programlanmış quad-word'e yazım (PROGERR) */
    }
    memcpy(&bellek[bek_ofs], bek_q, IDA_KAL_QW);
    yazildi[q] = 1u;
    return 0;
}

static bool m_ecc(void *ctx, uint32_t ofs, uint32_t n)
{
    (void)ctx;
    if (!sinirda(ofs, n) || n == 0u) {
        return true;
    }
    for (uint32_t q = ofs / IDA_KAL_QW; q <= (ofs + n - 1u) / IDA_KAL_QW && q < QW_N; q++) {
        if (ecc[q] != 0u) {
            return true;
        }
    }
    return false;
}

static const ida_kal_flash_t F = {NULL, m_taban, m_sil, m_yaz16, m_durum, m_ecc};

void setUp(void)
{
    memset(bellek, 0xFF, sizeof bellek);
    memset(yazildi, 0, sizeof yazildi);
    memset(ecc, 0, sizeof ecc);
    yazma_hatasi = 0;
    silme = yazma = 0u;
    islem_no = 0u;
    kes_islem = -1;
    olu = 0;
    bekleyen = 0;
    mesgul_kalan = 0;
    busy_n = 2;
}

void tearDown(void) {}

/* "Kart yeniden açıldı": güç geri gelir, aynı bellekle açılış. */
static void ac(void)
{
    olu = 0;
    bekleyen = 0;
    kes_islem = -1;
    ida_kal_acilis(&k, &F, NULL, NULL);
}

static void blob_kur(uint8_t *b, uint32_t n, uint8_t tohum)
{
    for (uint32_t i = 0u; i < n; i++) {
        b[i] = (uint8_t)(tohum + i * 7u);
    }
}

static void adim_bitir(void)
{
    for (uint32_t i = 0u; i < 20000u && ida_kal_mesgul(&k); i++) {
        ida_kal_adim(&k);
    }
}

static void kaydet_bitir(const uint8_t *b, uint32_t n, uint32_t gen)
{
    ida_kal_kaydet(&k, b, n, gen);
    adim_bitir();
    TEST_ASSERT_FALSE(ida_kal_mesgul(&k));
}

/* Doğrudan modele geçerli bir sayaç kaydı yazar (yazılmış işaretli). */
static void sayac_kaydi_koy(uint32_t ofs, uint32_t n)
{
    uint8_t q[IDA_KAL_QW] = {0};
    q[0] = 0x42;
    q[1] = 0x4F;
    q[2] = 0x4F;
    q[3] = 0x54;
    memcpy(&q[4], &n, 4);
    const uint32_t d = 0xFFFFFFFFu ^ n;
    memcpy(&q[8], &d, 4);
    const uint32_t c = ida_kal_crc32(q, 12u);
    memcpy(&q[12], &c, 4);
    if (sinirda(ofs, IDA_KAL_QW) && ofs <= BELLEK_BAYT - IDA_KAL_QW) {
        memcpy(&bellek[ofs], q, IDA_KAL_QW);
        yazildi[ofs / IDA_KAL_QW] = 1u;
    }
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
    yazildi[2] = 1u;
    ac();
    TEST_ASSERT_EQUAL_UINT32(3u, k.boot_count);
    TEST_ASSERT_EQUAL_UINT8(1u, k.boot_count_ok);
    ac();
    TEST_ASSERT_EQUAL_UINT32(4u, k.boot_count);
}

/* Sol bulgu 7: tamamen 0xFF yazılmış (programlanmış) yuva silinmiş görünür ama yazılamaz;
   sayaç bir sonraki yuvaya geçer. */
static void test_ff_yazilmis_yuva(void)
{
    ac();
    yazildi[1] = 1u; /* 2. yuva: veri 0xFF ama programlanmış */
    ac();
    TEST_ASSERT_EQUAL_UINT32(2u, k.boot_count);
    TEST_ASSERT_EQUAL_UINT8(1u, k.boot_count_ok);
    ac();
    TEST_ASSERT_EQUAL_UINT32(3u, k.boot_count);
}

/* Fabrika verisi (0xFF olmayan, geçersiz) taşıyan sayfalar: sayfa silinir, sayaç 1'den başlar. */
static void test_cop_veri(void)
{
    memset(bellek, 0x5A, sizeof bellek);
    memset(yazildi, 1, sizeof yazildi);
    ac();
    TEST_ASSERT_EQUAL_UINT32(1u, k.boot_count);
    TEST_ASSERT_EQUAL_UINT8(1u, k.boot_count_ok);
    TEST_ASSERT_EQUAL_INT8(-1, k.gecerli_sayfa);
}

static void test_sayac_tukenir(void)
{
    sayac_kaydi_koy(0u, 0xFFFFFFFFu);
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

/* Sol bulgu 6: çift ECC hatası görülen sayaç kaydı, baytları geçerli görünse de reddedilir;
   yuvası da kullanılmaz. */
static void test_ecc_bozuk_sayac_kaydi_reddedilir(void)
{
    sayac_kaydi_koy(0u, 5u);
    sayac_kaydi_koy(16u, 100u); /* baytları geçerli ama ECC'si bozuk */
    ecc[1] = 1u;
    ac();
    TEST_ASSERT_EQUAL_UINT32(6u, k.boot_count);
    TEST_ASSERT_EQUAL_UINT8(1u, k.boot_count_ok);
    ac();
    TEST_ASSERT_EQUAL_UINT32(7u, k.boot_count);
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

/* Sol bulgu 6: ECC'si bozuk quad-word içeren tam (CRC'si tutan) blok seçilmez. */
static void test_ecc_bozuk_blok_secilmez(void)
{
    static uint8_t eski[200], yeni[200];
    blob_kur(eski, sizeof eski, 9u);
    blob_kur(yeni, sizeof yeni, 77u);
    ac();
    kaydet_bitir(eski, sizeof eski, 8u);
    kaydet_bitir(yeni, sizeof yeni, 9u);
    const int8_t s = k.gecerli_sayfa;
    TEST_ASSERT_TRUE(s >= 2 && s <= 3);
    if (s < 2 || s > 3) {
        return;
    }
    ecc[(uint32_t)s * SAYFA_QW + 3u] = 1u; /* blob içinde bir quad-word */
    ac();
    TEST_ASSERT_EQUAL_UINT32(8u, k.blok_gen);
    uint32_t len = 0u;
    const uint8_t *p = ida_kal_blob(&k, &len);
    TEST_ASSERT_NOT_NULL(p);
    if (p == NULL) {
        return;
    }
    TEST_ASSERT_EQUAL_MEMORY(eski, p, sizeof eski);
}

/* Kayıt her işlemde yarıda kesilir (yarım silme / yarım program; güç kesintisi): yeniden
   açılışta ya eski blok ya da bütünüyle yeni blok geçerlidir (sol bulgu 7). */
static void test_kesinti_her_islemde(void)
{
    static uint8_t eski[200], yeni[200];
    blob_kur(eski, sizeof eski, 9u);
    blob_kur(yeni, sizeof yeni, 77u);
    uint32_t yeni_secildi = 0u, eski_secildi = 0u;
    for (int64_t kes = 0; kes < 20; kes++) {
        setUp();
        ac();
        kaydet_bitir(eski, sizeof eski, 7u);
        const uint32_t bas = islem_no;
        kes_islem = (int64_t)bas + kes;
        ida_kal_kaydet(&k, yeni, sizeof yeni, 8u);
        adim_bitir();
        ac();
        uint32_t len = 0u;
        const uint8_t *p = ida_kal_blob(&k, &len);
        TEST_ASSERT_NOT_NULL(p);
        if (p == NULL) {
            return;
        }
        if (k.blok_gen == 8u) {
            yeni_secildi++;
            TEST_ASSERT_EQUAL_MEMORY(yeni, p, sizeof yeni);
        } else {
            eski_secildi++;
            TEST_ASSERT_EQUAL_UINT32(7u, k.blok_gen);
            TEST_ASSERT_EQUAL_MEMORY(eski, p, sizeof eski);
        }
    }
    TEST_ASSERT_TRUE(eski_secildi > 0u); /* kesinti tamamlanma yazımından önce */
    TEST_ASSERT_TRUE(yeni_secildi > 0u); /* kesinti işlemler bittikten sonra */
}

/* Tamamlanma yazımının kendisi yarıda kesilir: yarım tamamlanma kaydı bloğu geçerli kılmaz. */
static void test_kesinti_tamamlanma_yaziminda(void)
{
    static uint8_t eski[100], yeni[100];
    blob_kur(eski, sizeof eski, 1u);
    blob_kur(yeni, sizeof yeni, 2u);
    ac();
    kaydet_bitir(eski, sizeof eski, 3u);
    const uint32_t bas = islem_no;
    /* sil + (1 + 7) quad-word + tamamlanma: tamamlanma 10. işlem (0'dan 9). */
    kes_islem = (int64_t)bas + 1 + 1 + (int64_t)((sizeof yeni + 15u) / 16u);
    ida_kal_kaydet(&k, yeni, sizeof yeni, 4u);
    adim_bitir();
    ac();
    TEST_ASSERT_EQUAL_UINT32(3u, k.blok_gen);
}

/* Sayaç sayfa değişiminde (öteki sayfanın silinmesi ve ilk yazım) kesinti: sayaç geri gitmez. */
static void test_kesinti_sayac_sayfa_degisiminde(void)
{
    for (uint32_t i = 1u; i <= 512u; i++) {
        ac();
    }
    TEST_ASSERT_EQUAL_UINT32(512u, k.boot_count); /* sayfa 0 dolu */
    for (int64_t kes = 0; kes < 3; kes++) {
        const uint32_t once = k.boot_count;
        olu = 0;
        bekleyen = 0;
        kes_islem = (int64_t)islem_no + kes;
        ida_kal_acilis(&k, &F, NULL, NULL); /* kesilen açılış */
        ac();                               /* sonraki açılış */
        TEST_ASSERT_EQUAL_UINT8(1u, k.boot_count_ok);
        TEST_ASSERT_TRUE(k.boot_count > once);
    }
}

/* Yarım kalan quad-word içeren bozuk tamamlanma kaydı geçersizdir. */
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
    adim_bitir();
    TEST_ASSERT_FALSE(ida_kal_mesgul(&k));
    TEST_ASSERT_EQUAL_UINT8(IDA_KAL_FAIL, k.store);
    TEST_ASSERT_EQUAL_UINT8(IDA_KAL_FAIL_ABORTED, k.fail);
    TEST_ASSERT_EQUAL_UINT32(2u, k.store_gen);
    TEST_ASSERT_EQUAL_UINT32(1u, k.blok_gen);
    ac();
    TEST_ASSERT_EQUAL_UINT32(1u, k.blok_gen);
}

/* Sol bulgu 3: iptal tamamlanma yazımı başladıktan sonra (yazım sürerken ya da bittikten
   sonra, durum makinesi işlemeden) gelir. FAIL/ABORTED bildirilirse yeniden açılış eski bloğu
   seçmelidir (A4-K83: FAIL'de kalıcı blok değişmemiştir); OK bildirilirse yeni bloğu. */
static void iptal_tamamlanmada(int yazim_suruyor)
{
    static uint8_t eski[64], yeni[64];
    blob_kur(eski, sizeof eski, 1u);
    blob_kur(yeni, sizeof yeni, 2u);
    ac();
    kaydet_bitir(eski, sizeof eski, 1u);
    const uint32_t tamamlanma_no = islem_no + 1u + 1u + (uint32_t)((sizeof yeni + 15u) / 16u);
    ida_kal_kaydet(&k, yeni, sizeof yeni, 2u);
    /* Tamamlanma yazımı başlayana kadar ilerle. */
    for (uint32_t i = 0u; i < 20000u && islem_no <= tamamlanma_no && ida_kal_mesgul(&k); i++) {
        ida_kal_adim(&k);
    }
    TEST_ASSERT_TRUE(ida_kal_mesgul(&k));
    if (!yazim_suruyor) {
        while (m_durum(NULL) == 1) { /* tamamlanma yazımı donanımda biter */
        }
    }
    ida_kal_iptal(&k);
    adim_bitir();
    TEST_ASSERT_FALSE(ida_kal_mesgul(&k));
    const uint8_t store = k.store;
    const uint8_t fail = k.fail;
    const uint32_t ram_gen = k.blok_gen;
    ac();
    if (store == IDA_KAL_FAIL) {
        TEST_ASSERT_EQUAL_UINT8(IDA_KAL_FAIL_ABORTED, fail);
        TEST_ASSERT_EQUAL_UINT32_MESSAGE(1u, k.blok_gen, "FAIL bildirilen kayıt kalıcı bloğu değiştirmemeli");
        TEST_ASSERT_EQUAL_UINT32(1u, ram_gen);
    } else {
        TEST_ASSERT_EQUAL_UINT8(IDA_KAL_OK, store);
        TEST_ASSERT_EQUAL_UINT32(2u, k.blok_gen);
        TEST_ASSERT_EQUAL_UINT32(2u, ram_gen);
    }
}

static void test_iptal_tamamlanma_yazimi_surerken(void)
{
    iptal_tamamlanmada(1);
}

static void test_iptal_tamamlanma_yazimi_bittikten_sonra(void)
{
    iptal_tamamlanmada(0);
}

/* İptalin geri alma silmesi güç kesintisiyle yarıda kalır: yeniden açılış eski bloğu seçer. */
static void test_iptal_geri_alma_silmesi_kesilir(void)
{
    static uint8_t eski[64], yeni[64];
    blob_kur(eski, sizeof eski, 1u);
    blob_kur(yeni, sizeof yeni, 2u);
    ac();
    kaydet_bitir(eski, sizeof eski, 1u);
    const uint32_t tamamlanma_no = islem_no + 1u + 1u + (uint32_t)((sizeof yeni + 15u) / 16u);
    ida_kal_kaydet(&k, yeni, sizeof yeni, 2u);
    for (uint32_t i = 0u; i < 20000u && islem_no <= tamamlanma_no && ida_kal_mesgul(&k); i++) {
        ida_kal_adim(&k);
    }
    while (m_durum(NULL) == 1) {
    }
    ida_kal_iptal(&k);
    kes_islem = (int64_t)islem_no; /* sıradaki işlem: geri alma silmesi */
    adim_bitir();
    TEST_ASSERT_EQUAL_UINT8(IDA_KAL_FAIL, k.store);
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
    adim_bitir();
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
    RUN_TEST(test_ff_yazilmis_yuva);
    RUN_TEST(test_cop_veri);
    RUN_TEST(test_sayac_tukenir);
    RUN_TEST(test_yazma_hatasi_sayac_ayrilamaz);
    RUN_TEST(test_ecc_bozuk_sayac_kaydi_reddedilir);
    RUN_TEST(test_parametre_kaydi_ve_acilis);
    RUN_TEST(test_ab_donusumu);
    RUN_TEST(test_ecc_bozuk_blok_secilmez);
    RUN_TEST(test_kesinti_her_islemde);
    RUN_TEST(test_kesinti_tamamlanma_yaziminda);
    RUN_TEST(test_kesinti_sayac_sayfa_degisiminde);
    RUN_TEST(test_bozuk_tamamlanma);
    RUN_TEST(test_iptal);
    RUN_TEST(test_iptal_tamamlanma_yazimi_surerken);
    RUN_TEST(test_iptal_tamamlanma_yazimi_bittikten_sonra);
    RUN_TEST(test_iptal_geri_alma_silmesi_kesilir);
    RUN_TEST(test_cok_buyuk_ve_flash_hatasi);
    return UNITY_END();
}
