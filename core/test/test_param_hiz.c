/*
 * test_param_hiz.c — W4-param-hiz: tablolu CRC ve şema özeti önbelleğinin, değişiklikten önceki
 * bit bit uygulamayla bayt bayt aynı sonucu verdiği (A4 §3.10; biçim, CRC tanımı ve özet
 * değişmez).
 *
 * Referans: değişiklikten önceki ida_crc32 (bit bit), ida_prm_sema_ozeti'nin gövdesi ve
 * ida_prm_blok_kodla (her kodlamada özeti tablodan yeniden hesaplayan) bu dosyada çekirdeğin
 * işlevlerini çağırmadan tutulur (yalnız ozet.c'nin SHA-256'sı ortaktır; o bilinen vektörlerle
 * test_param.c'de sınanır). Güncel şemanın özeti ayrıca eski çekirdeğin ikilisiyle ve sol
 * incelemesinin bağımsız hesabıyla bulunan altın değere sabitlenir. Rastgele durumlar sabit
 * tohumludur.
 */
#include <string.h>

#include "destek.h"
#include "param/ozet.h"
#include "unity.h"

void setUp(void);
void tearDown(void);
void setUp(void) {}
void tearDown(void) {}

/* ---- Referans: değişiklikten önceki uygulama ------------------------------------------- */

static uint32_t ref_crc32(const uint8_t *veri, uint32_t uzunluk)
{
    uint32_t crc = 0xFFFFFFFFu;
    for (uint32_t i = 0u; i < uzunluk; i++) {
        crc ^= (uint32_t)veri[i];
        for (uint32_t b = 0u; b < 8u; b++) {
            const uint32_t maske = 0u - (crc & 1u);
            crc = (crc >> 1) ^ (0xEDB88320u & maske);
        }
    }
    return crc ^ 0xFFFFFFFFu;
}

static void ref_yaz16(uint8_t *b, uint32_t x)
{
    b[0] = (uint8_t)x;
    b[1] = (uint8_t)(x >> 8);
}

static void ref_yaz32(uint8_t *b, uint32_t x)
{
    b[0] = (uint8_t)x;
    b[1] = (uint8_t)(x >> 8);
    b[2] = (uint8_t)(x >> 16);
    b[3] = (uint8_t)(x >> 24);
}

/* bc83bca'daki ozet_ondalik ve ida_prm_sema_ozeti gövdesi (tablo değişken; metin boyu sayılır). */
static void ref_ondalik(ida_sha256_t *s, uint32_t x, uint32_t *boy)
{
    uint8_t rakam[10] = {0u};
    uint32_t n = 0u;
    uint32_t kalan = x;
    do {
        rakam[n] = (uint8_t)('0' + (uint8_t)(kalan % 10u));
        n++;
        kalan /= 10u;
    } while (kalan != 0u);
    while (n > 0u) {
        n--;
        ida_sha256_ekle(s, &rakam[n], 1u);
        (*boy)++;
    }
}

static uint64_t ref_sema_ozeti(const ida_param_desc_t *tablo, uint16_t sayi, uint32_t *metin_boyu)
{
    ida_sha256_t s;
    ida_sha256_basla(&s);
    uint32_t boy = 0u;
    const uint8_t sekme = (uint8_t)'\t';
    const uint8_t satir = (uint8_t)'\n';
    for (uint16_t i = 0u; i < sayi; i++) {
        const ida_param_desc_t *d = &tablo[i];
        ref_ondalik(&s, d->id, &boy);
        ida_sha256_ekle(&s, &sekme, 1u);
        for (uint32_t j = 0u; (j < IDA_PARAM_NAME_MAX) && (d->name[j] != '\0'); j++) {
            const uint8_t ch = (uint8_t)d->name[j];
            ida_sha256_ekle(&s, &ch, 1u);
            boy++;
        }
        ida_sha256_ekle(&s, &sekme, 1u);
        ref_ondalik(&s, d->type, &boy);
        ida_sha256_ekle(&s, &satir, 1u);
        boy += 3u; /* iki sekme ve satır sonu */
    }
    uint8_t ozet[32];
    ida_sha256_bitir(&s, ozet);
    uint64_t sonuc = 0u;
    for (uint32_t i = 0u; i < 8u; i++) {
        sonuc |= (uint64_t)ozet[i] << (8u * i);
    }
    *metin_boyu = boy;
    return sonuc;
}

/* bc83bca'daki ida_prm_blok_kodla gövdesi (tablo ve ham dizi değişken). */
static uint32_t ref_blok_kodla_tablo(const ida_param_desc_t *tablo, uint16_t sayi,
                                     const uint32_t *ham, uint8_t *blok)
{
    const uint32_t yuk = IDA_PBLOB_REC_SIZE * (uint32_t)sayi;
    ref_yaz32(&blok[0], IDA_PBLOB_MAGIC);
    ref_yaz16(&blok[4], IDA_PBLOB_FMT_VER);
    ref_yaz16(&blok[6], 0u);
    uint32_t metin = 0u;
    const uint64_t ozet = ref_sema_ozeti(tablo, sayi, &metin); /* her kodlamada tablodan */
    ref_yaz32(&blok[8], (uint32_t)ozet);
    ref_yaz32(&blok[12], (uint32_t)(ozet >> 32));
    ref_yaz32(&blok[16], yuk);
    for (uint32_t r = 0u; r < (uint32_t)sayi; r++) {
        uint8_t *kay = &blok[IDA_PBLOB_HDR_SIZE + (r * IDA_PBLOB_REC_SIZE)];
        ref_yaz16(kay, tablo[r].id);
        kay[2] = tablo[r].type;
        kay[3] = 0u;
        ref_yaz32(&kay[4], ham[r]);
    }
    ref_yaz32(&blok[20], ref_crc32(&blok[IDA_PBLOB_HDR_SIZE], yuk));
    return IDA_PBLOB_HDR_SIZE + yuk;
}

/* Derlenmiş tablonun port işleviyle alınmış kopyası (referans çekirdeğin iç tablosunu okumaz). */
static ida_param_desc_t g_asil[IDA_PRM_SAYI];

static void asil_tablo(void)
{
    for (uint16_t i = 0u; i < (uint16_t)IDA_PRM_SAYI; i++) {
        TEST_ASSERT_EQUAL(IDA_RC_OK, ida_core_param_desc(i, &g_asil[i]));
    }
}

static uint32_t ref_blok_kodla(const ida_prm_kume_t *k, uint8_t *blok)
{
    asil_tablo();
    return ref_blok_kodla_tablo(g_asil, (uint16_t)IDA_PRM_SAYI, k->ham, blok);
}

/* ---- CRC ------------------------------------------------------------------------------- */

#define SEKIZ_KB 8192u
static uint8_t g_veri[SEKIZ_KB];

static void test_crc_bilinen_vektorler(void)
{
    TEST_ASSERT_EQUAL_HEX32(0xCBF43926u, ida_crc32((const uint8_t *)"123456789", 9u));
    TEST_ASSERT_EQUAL_HEX32(0x00000000u, ida_crc32(g_veri, 0u)); /* boş girdi */
    const uint8_t sifir = 0x00u;
    const uint8_t ff = 0xFFu;
    const uint8_t a = (uint8_t)'a';
    TEST_ASSERT_EQUAL_HEX32(0xD202EF8Du, ida_crc32(&sifir, 1u)); /* tek bayt */
    TEST_ASSERT_EQUAL_HEX32(0xFF000000u, ida_crc32(&ff, 1u));
    TEST_ASSERT_EQUAL_HEX32(0xE8B7BE43u, ida_crc32(&a, 1u));
    /* 8 KB: değerler bağımsız bir uygulamayla (Python zlib.crc32) üretildi. */
    (void)memset(g_veri, 0, sizeof g_veri);
    TEST_ASSERT_EQUAL_HEX32(0xD8F49994u, ida_crc32(g_veri, SEKIZ_KB));
    for (uint32_t i = 0u; i < SEKIZ_KB; i++) {
        g_veri[i] = (uint8_t)((i * 7u) + 3u);
    }
    TEST_ASSERT_EQUAL_HEX32(0xB65EF7BFu, ida_crc32(g_veri, SEKIZ_KB));
    TEST_ASSERT_EQUAL_HEX32(ref_crc32(g_veri, SEKIZ_KB), ida_crc32(g_veri, SEKIZ_KB));
}

/* 256 tek baytlık girdi tablonun her satırını bir kez kullanır: tablo bit bit tanımla aynı. */
static void test_crc_tablonun_her_satiri(void)
{
    for (uint32_t b = 0u; b < 256u; b++) {
        const uint8_t x = (uint8_t)b;
        TEST_ASSERT_EQUAL_HEX32(ref_crc32(&x, 1u), ida_crc32(&x, 1u));
    }
}

static void test_crc_rastgele_10000(void)
{
    ds_rastgele_t r = {.s = 0x1DA5EEDu};
    for (uint32_t i = 0u; i < SEKIZ_KB; i++) {
        g_veri[i] = (uint8_t)ds_rastgele(&r);
    }
    for (uint32_t n = 0u; n < 10000u; n++) {
        const uint32_t bas = ds_rastgele(&r) % 64u;          /* hizalanmamış başlangıç */
        const uint32_t boy = ds_rastgele(&r) % (SEKIZ_KB - 64u + 1u);
        const uint32_t kisa = boy % 97u;                       /* kısa girdiler de çok olsun */
        const uint32_t u = ((n & 1u) != 0u) ? boy : kisa;
        g_veri[bas] = (uint8_t)(g_veri[bas] ^ (uint8_t)n);     /* her denemede içerik değişir */
        TEST_ASSERT_EQUAL_HEX32(ref_crc32(&g_veri[bas], u), ida_crc32(&g_veri[bas], u));
    }
}

/* ---- Blok kodlaması: bayt eşitliği --------------------------------------------------------- */

static uint8_t g_yeni[IDA_PRM_BLOB_BOYU];
static uint8_t g_eski[IDA_PRM_BLOB_BOYU];
static ida_core_t C;

static void ayni_blok(const ida_prm_kume_t *k, uint64_t ozet)
{
    (void)memset(g_yeni, 0xA5, sizeof g_yeni);
    (void)memset(g_eski, 0x5A, sizeof g_eski);
    const uint32_t n_yeni = ida_prm_blok_kodla(k, ozet, g_yeni);
    const uint32_t n_eski = ref_blok_kodla(k, g_eski);
    TEST_ASSERT_EQUAL_UINT32(n_eski, n_yeni);
    TEST_ASSERT_EQUAL_UINT32(IDA_PRM_BLOB_BOYU, n_yeni);
    TEST_ASSERT_EQUAL_HEX8_ARRAY(g_eski, g_yeni, IDA_PRM_BLOB_BOYU);
}

static void test_blok_varsayilan_ve_uc_degerler(void)
{
    TEST_ASSERT_EQUAL(IDA_RC_OK, ds_kur(&C));
    const uint64_t ozet = C.prm.sema_ozeti; /* çekirdeğin önbelleği */
    ida_prm_kume_t k;
    ida_prm_varsayilan(&k);
    ayni_blok(&k, ozet);
    /* Her parametre kendi tipinin en az ve en çok değerinde (derlenmiş tabloda U32 ve F32 var,
       I32 yok; I32 kayıtları test_blok_i32_duzenegi'nde sınanır). */
    ida_prm_kume_t en_az = {{0u}};
    ida_prm_kume_t en_cok = {{0u}};
    for (uint16_t i = 0u; i < (uint16_t)IDA_PRM_SAYI; i++) {
        ida_param_desc_t d = {0};
        TEST_ASSERT_EQUAL(IDA_RC_OK, ida_core_param_desc(i, &d));
        en_az.ham[i] = d.min.v.u32;
        en_cok.ham[i] = d.max.v.u32;
    }
    ayni_blok(&en_az, ozet);
    ayni_blok(&en_cok, ozet);
    /* Tipten bağımsız bit desenleri (derlenmiş tablonun U32 ve F32 kayıtlarına): 0, tüm birler,
       0x80000000 (-0.0), 0x7FFFFFFF, sessiz NaN, +sonsuz, en küçük alt normal. */
    static const uint32_t DESEN[] = {0x00000000u, 0xFFFFFFFFu, 0x80000000u, 0x7FFFFFFFu,
                                     0x7FC00000u, 0x7F800000u, 0x00000001u};
    for (uint32_t j = 0u; j < (uint32_t)(sizeof DESEN / sizeof DESEN[0]); j++) {
        for (uint32_t i = 0u; i < (uint32_t)IDA_PRM_SAYI; i++) {
            k.ham[i] = DESEN[j];
        }
        ayni_blok(&k, ozet);
    }
}

static void test_blok_rastgele_1000(void)
{
    TEST_ASSERT_EQUAL(IDA_RC_OK, ds_kur(&C));
    ds_rastgele_t r = {.s = 0xB10C5EEDu};
    ida_prm_kume_t k = {{0u}};
    for (uint32_t n = 0u; n < 1000u; n++) {
        for (uint32_t i = 0u; i < (uint32_t)IDA_PRM_SAYI; i++) {
            k.ham[i] = ds_rastgele(&r);
        }
        ayni_blok(&k, C.prm.sema_ozeti);
    }
}

/* Çekirdek yolu: PARAM_SET + PARAM_COMMIT + PARAM_SAVE'in bloğu referansla bayt bayt aynı. */
static ida_in_t IN;
static ida_out_t OUT;

static void test_param_save_cikti_blogu(void)
{
    TEST_ASSERT_EQUAL(IDA_RC_OK, ds_kur(&C));
    const int64_t t = 2005 * DS_MS;
    ds_girdi(&IN, t, 1u, true);
    IN.cmds[0].cmd_id = 1u;
    IN.cmds[0].type = (uint8_t)IDA_CMD_PARAM_SET;
    IN.cmds[0].u.param.id = 0x0101u; /* T_imu_stale, 5–30 ms */
    IN.cmds[0].u.param.type = (uint8_t)IDA_PT_U32;
    IN.cmds[0].u.param.v.u32 = 25u;
    IN.cmds[1].cmd_id = 2u;
    IN.cmds[1].type = (uint8_t)IDA_CMD_PARAM_COMMIT;
    IN.cmds[2].cmd_id = 3u;
    IN.cmds[2].type = (uint8_t)IDA_CMD_PARAM_SAVE;
    IN.n_cmds = 3u;
    TEST_ASSERT_EQUAL(IDA_RC_OK, ida_core_step(&C, &IN, &OUT));
    const ida_cmd_result_t *r = ds_sonuc(&OUT, 3u);
    TEST_ASSERT_NOT_NULL(r);
    TEST_ASSERT_EQUAL_UINT8(IDA_ACK_ACCEPTED, r->ack);
    TEST_ASSERT_NOT_NULL(r->save.blob);
    TEST_ASSERT_EQUAL_UINT32(IDA_PRM_BLOB_BOYU, r->save.len);
    TEST_ASSERT_EQUAL_UINT32(25u, ida_prm_u32(&C.prm, IDA_PRM_imu_stale_ms));
    const uint32_t n = ref_blok_kodla(&C.prm.yururluk, g_eski);
    TEST_ASSERT_EQUAL_UINT32(n, r->save.len);
    TEST_ASSERT_EQUAL_HEX8_ARRAY(g_eski, r->save.blob, IDA_PRM_BLOB_BOYU);
}

/* ---- Şema özeti: önbellek, değer ve tablodan türeme --------------------------------------- */

static ida_param_desc_t g_tablo[IDA_PRM_SAYI];

static void tabloyu_kopyala(void)
{
    for (uint16_t i = 0u; i < (uint16_t)IDA_PRM_SAYI; i++) {
        TEST_ASSERT_EQUAL(IDA_RC_OK, ida_core_param_desc(i, &g_tablo[i]));
    }
}

static void test_sema_ozeti_onbellegi(void)
{
    /* Önbellek, init'te tablodan hesaplanan değerdir ve ida_core_info'nunkiyle aynıdır. */
    (void)memset(&C, 0xC3, sizeof C);
    TEST_ASSERT_EQUAL(IDA_RC_OK, ds_kur(&C));
    ida_core_info_t info;
    ida_core_info(&info);
    TEST_ASSERT_TRUE(C.prm.sema_ozeti == ida_prm_sema_ozeti());
    TEST_ASSERT_TRUE(C.prm.sema_ozeti == info.param_schema_hash);
    TEST_ASSERT_EQUAL(IDA_RC_PARAM_DEFAULTS, ds_kur_bloksuz(&C));
    TEST_ASSERT_TRUE(C.prm.sema_ozeti == info.param_schema_hash);
    TEST_ASSERT_TRUE(ida_prm_tutarli(&C.prm));
    ida_param_t p;
    (void)memset(&p, 0, sizeof p);
    TEST_ASSERT_FALSE(ida_prm_tutarli(&p)); /* sıfır önbellek tümleyeniyle tutarsız */
    ida_prm_kur(&p);
    TEST_ASSERT_TRUE(p.sema_ozeti == info.param_schema_hash);
    TEST_ASSERT_TRUE(p.sema_ozeti_tumleyen == (UINT64_MAX ^ info.param_schema_hash));
    TEST_ASSERT_TRUE(ida_prm_tutarli(&p));
    /* Başka bir özetle yazılmış blok init'te (2) ile reddedilir: önbellek denetimde de kullanılır. */
    ida_prm_kume_t k;
    ida_prm_varsayilan(&k);
    const uint32_t n = ida_prm_blok_kodla(&k, info.param_schema_hash ^ UINT64_C(1), g_yeni);
    TEST_ASSERT_EQUAL_UINT8(2u, ida_prm_blok_coz(g_yeni, n, info.param_schema_hash, &k));
    TEST_ASSERT_EQUAL_UINT8(0u, ida_prm_blok_coz(g_yeni, n, info.param_schema_hash ^ UINT64_C(1),
                                                 &k));
}

static void test_sema_ozeti_tablodan_turer(void)
{
    const uint64_t asil = ida_prm_sema_ozeti();
    tabloyu_kopyala();
    TEST_ASSERT_TRUE(ida_prm_sema_ozeti_tablo(g_tablo, (uint16_t)IDA_PRM_SAYI) == asil);
    const uint16_t son = (uint16_t)((uint16_t)IDA_PRM_SAYI - 1u);
    /* Kanonik metne giren her alan özeti değiştirir: ad, kimlik, tip, satır sayısı. */
    g_tablo[son].name[0] = (char)(g_tablo[son].name[0] ^ 0x20);
    TEST_ASSERT_TRUE(ida_prm_sema_ozeti_tablo(g_tablo, (uint16_t)IDA_PRM_SAYI) != asil);
    tabloyu_kopyala();
    g_tablo[0].id = (uint16_t)(g_tablo[0].id + 1u);
    TEST_ASSERT_TRUE(ida_prm_sema_ozeti_tablo(g_tablo, (uint16_t)IDA_PRM_SAYI) != asil);
    tabloyu_kopyala();
    g_tablo[son].type = (g_tablo[son].type == (uint8_t)IDA_PT_F32) ? (uint8_t)IDA_PT_U32
                                                                     : (uint8_t)IDA_PT_F32;
    TEST_ASSERT_TRUE(ida_prm_sema_ozeti_tablo(g_tablo, (uint16_t)IDA_PRM_SAYI) != asil);
    tabloyu_kopyala();
    TEST_ASSERT_TRUE(ida_prm_sema_ozeti_tablo(g_tablo, son) != asil);
    /* Metne girmeyen alanlar (sınıf, en az, en çok, varsayılan) özeti değiştirmez. */
    g_tablo[son].cls = (uint8_t)(g_tablo[son].cls ^ 3u);
    g_tablo[son].min.v.u32 ^= 1u;
    g_tablo[son].max.v.u32 ^= 1u;
    g_tablo[son].def.v.u32 ^= 1u;
    TEST_ASSERT_TRUE(ida_prm_sema_ozeti_tablo(g_tablo, (uint16_t)IDA_PRM_SAYI) == asil);
    /* Değiştirilmiş tabloda yeni yardımcı eski gövdeyle aynı sonucu verir. */
    g_tablo[son].name[1] = 'Q';
    uint32_t metin = 0u;
    TEST_ASSERT_TRUE(ida_prm_sema_ozeti_tablo(g_tablo, (uint16_t)IDA_PRM_SAYI) ==
                     ref_sema_ozeti(g_tablo, (uint16_t)IDA_PRM_SAYI, &metin));
}

/* Altın değer (sol W4-param-hiz bulgu 2): güncel şemanın özeti, kanonik metin boyu ve
   varsayılan blok boyu. Değerler değişiklikten önceki çekirdeğin (bc83bca) ikilisiyle ve sol
   incelemesinin bağımsız hesabıyla aynı bulundu (159 parametre; varsayılan bloğun yük CRC'si
   0xB011EAB1, varsayılanlar ayarlandıkça değiştiği için sabitlenmez). Parametre sayısı ayrıca
   sabitlenmez (W3-K6); ama şemadaki her değişiklik özeti de değiştirir.
   W5-atis-emniyet (2026-11-24): atış kontrolün üç emniyet zinciri parametresi eklendi
   (0x0601–0x0603; 162 parametre); değerler kanonik metnin SHA-256'sıyla çekirdekten bağımsız
   hesaplandı (aynı hesap eski şemada 0x34065414A77F8846 / 2426 B'yi verir).
   W5-atis-emniyet düzeltme turu (2026-11-24; W5-K13): T_assess ve T_tup_bos parametre oldu
   (0x0604, 0x0605; 164 parametre); değerler .def dosyalarının çekirdekten bağımsız
   ayrıştırılmasıyla kurulan kanonik metnin SHA-256'sından (aynı hesap 162 parametrede
   0x18BE798234EDD38D / 2477 B'yi verir). */
#define ALTIN_OZET UINT64_C(0xE0ED504FB6AEA22D)
#define ALTIN_METIN 2510u
#define ALTIN_BLOK 1336u
#define ALTIN_ILETI                                                                                \
    "Şema bilerek değiştiyse (parametre eklendi, adı, kimliği ya da tipi değişti) bu altın "       \
    "değerleri yeni şemanın bağımsız hesabıyla güncelleyin; bilerek değişmediyse özet hesabı "     \
    "ya da blok biçimi bozulmuştur."

static void test_sema_ozeti_altin_deger(void)
{
    asil_tablo();
    uint32_t metin = 0u;
    const uint64_t ref = ref_sema_ozeti(g_asil, (uint16_t)IDA_PRM_SAYI, &metin);
    TEST_ASSERT_TRUE_MESSAGE(ref == ALTIN_OZET, ALTIN_ILETI);
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(ALTIN_METIN, metin, ALTIN_ILETI);
    TEST_ASSERT_TRUE_MESSAGE(ida_prm_sema_ozeti() == ALTIN_OZET, ALTIN_ILETI);
    TEST_ASSERT_EQUAL(IDA_RC_OK, ds_kur(&C));
    TEST_ASSERT_TRUE_MESSAGE(C.prm.sema_ozeti == ALTIN_OZET, ALTIN_ILETI);
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(ALTIN_BLOK, ds_gecerli_blok(g_yeni), ALTIN_ILETI);
    const uint64_t baslik = (uint64_t)g_yeni[8] | ((uint64_t)g_yeni[9] << 8) |
                            ((uint64_t)g_yeni[10] << 16) | ((uint64_t)g_yeni[11] << 24) |
                            ((uint64_t)g_yeni[12] << 32) | ((uint64_t)g_yeni[13] << 40) |
                            ((uint64_t)g_yeni[14] << 48) | ((uint64_t)g_yeni[15] << 56);
    TEST_ASSERT_TRUE_MESSAGE(baslik == ALTIN_OZET, ALTIN_ILETI);
}

/* ---- I32 kayıtları (sol bulgu 3): ürün şemasını değiştirmeden, tablo parametreli kodlayıcıyla */

#define DUZ_SAYI 8u
static const ida_param_desc_t DUZENEK[DUZ_SAYI] = {
    {.id = 0x0901u, .type = (uint8_t)IDA_PT_I32, .cls = (uint8_t)IDA_PCLS_A, .name = "T_I32_A"},
    {.id = 0x0902u, .type = (uint8_t)IDA_PT_U32, .cls = (uint8_t)IDA_PCLS_A, .name = "T_U32_A"},
    {.id = 0x0903u, .type = (uint8_t)IDA_PT_F32, .cls = (uint8_t)IDA_PCLS_B, .name = "T_F32_A"},
    {.id = 0x0A10u, .type = (uint8_t)IDA_PT_I32, .cls = (uint8_t)IDA_PCLS_B, .name = "T_I32_B"},
    {.id = 0x1234u, .type = (uint8_t)IDA_PT_I32, .cls = (uint8_t)IDA_PCLS_A,
     .name = "T_I32_UZUN_AD_16"},
    {.id = 0x7FFFu, .type = (uint8_t)IDA_PT_F32, .cls = (uint8_t)IDA_PCLS_A, .name = "T_F32_B"},
    {.id = 0x8000u, .type = (uint8_t)IDA_PT_U32, .cls = (uint8_t)IDA_PCLS_A, .name = "T_U32_B"},
    {.id = 0xFFFFu, .type = (uint8_t)IDA_PT_I32, .cls = (uint8_t)IDA_PCLS_A, .name = "x"},
};
static uint8_t g_duz_yeni[IDA_PBLOB_HDR_SIZE + (IDA_PBLOB_REC_SIZE * DUZ_SAYI)];
static uint8_t g_duz_eski[IDA_PBLOB_HDR_SIZE + (IDA_PBLOB_REC_SIZE * DUZ_SAYI)];

static void duzenek_ayni(const uint32_t ham[DUZ_SAYI])
{
    (void)memset(g_duz_yeni, 0xA5, sizeof g_duz_yeni);
    (void)memset(g_duz_eski, 0x5A, sizeof g_duz_eski);
    const uint64_t ozet = ida_prm_sema_ozeti_tablo(DUZENEK, (uint16_t)DUZ_SAYI);
    const uint32_t n_yeni =
        ida_prm_blok_kodla_tablo(DUZENEK, (uint16_t)DUZ_SAYI, ham, ozet, g_duz_yeni);
    const uint32_t n_eski = ref_blok_kodla_tablo(DUZENEK, (uint16_t)DUZ_SAYI, ham, g_duz_eski);
    TEST_ASSERT_EQUAL_UINT32(n_eski, n_yeni);
    TEST_ASSERT_EQUAL_UINT32((uint32_t)sizeof g_duz_yeni, n_yeni);
    TEST_ASSERT_EQUAL_HEX8_ARRAY(g_duz_eski, g_duz_yeni, sizeof g_duz_yeni);
    /* I32 kaydı: tip baytı 1, değer ikiye tümleyen küçük uçlu. */
    TEST_ASSERT_EQUAL_UINT8(IDA_PT_I32, g_duz_yeni[IDA_PBLOB_HDR_SIZE + 2u]);
    const uint32_t d0 = (uint32_t)g_duz_yeni[28] | ((uint32_t)g_duz_yeni[29] << 8) |
                        ((uint32_t)g_duz_yeni[30] << 16) | ((uint32_t)g_duz_yeni[31] << 24);
    TEST_ASSERT_EQUAL_HEX32(ham[0], d0);
}

static void test_blok_i32_duzenegi(void)
{
    /* I32 uç değerleri: INT32_MIN, INT32_MAX, -1, 0, 1; diğer tiplerde de uçlar. */
    static const uint32_t UC[] = {0x80000000u, 0x7FFFFFFFu, 0xFFFFFFFFu, 0x00000000u,
                                  0x00000001u, 0x7FC00000u, 0xFF800000u, 0x00800000u};
    uint32_t ham[DUZ_SAYI] = {0u};
    for (uint32_t j = 0u; j < (uint32_t)(sizeof UC / sizeof UC[0]); j++) {
        for (uint32_t i = 0u; i < DUZ_SAYI; i++) {
            ham[i] = UC[(i + j) % (uint32_t)(sizeof UC / sizeof UC[0])];
        }
        duzenek_ayni(ham);
    }
    /* Rastgele 1 000 durum (sabit tohum). */
    ds_rastgele_t r = {.s = 0x132EEDu};
    for (uint32_t n = 0u; n < 1000u; n++) {
        for (uint32_t i = 0u; i < DUZ_SAYI; i++) {
            ham[i] = ds_rastgele(&r);
        }
        duzenek_ayni(ham);
    }
    /* Derlenmiş tabloda tablo parametreli kodlayıcı ida_prm_blok_kodla ile aynı. */
    ida_prm_kume_t k;
    ida_prm_varsayilan(&k);
    asil_tablo();
    const uint64_t ozet = ida_prm_sema_ozeti();
    TEST_ASSERT_EQUAL_UINT32(IDA_PRM_BLOB_BOYU, ida_prm_blok_kodla(&k, ozet, g_yeni));
    TEST_ASSERT_EQUAL_UINT32(IDA_PRM_BLOB_BOYU, ida_prm_blok_kodla_tablo(g_asil,
                                                (uint16_t)IDA_PRM_SAYI, k.ham, ozet, g_eski));
    TEST_ASSERT_EQUAL_HEX8_ARRAY(g_eski, g_yeni, IDA_PRM_BLOB_BOYU);
}

/* ---- Özet önbelleğinin bütünlüğü (sol bulgu 1; A4 §3.10, K89) ---------------------------- */

static uint32_t g_seq;
static int64_t g_t;
static uint32_t g_cmd;
static uint8_t g_kayitli[IDA_PRM_BLOB_BOYU];

static void adim_hazirla(void)
{
    g_seq++;
    g_t += 5 * DS_MS;
    ds_girdi(&IN, g_t, g_seq, true);
}

static void komut_ekle(uint8_t tur)
{
    const uint32_t i = IN.n_cmds;
    if (i >= (uint32_t)IDA_PORT_MAX_CMDS) {
        return;
    }
    g_cmd++;
    IN.cmds[i].cmd_id = g_cmd;
    IN.cmds[i].type = tur;
    IN.n_cmds = (uint8_t)(i + 1u);
}

/* Yalnız PARAM_SAVE içeren bir adım; sonucunu döner. */
static const ida_cmd_result_t *save_adimi(void)
{
    adim_hazirla();
    komut_ekle((uint8_t)IDA_CMD_PARAM_SAVE);
    (void)ida_core_step(&C, &IN, &OUT);
    const ida_cmd_result_t *r = ds_sonuc(&OUT, g_cmd);
    TEST_ASSERT_NOT_NULL(r);
    return r;
}

static const ida_log_t *gunluk_bul(uint16_t kod)
{
    for (uint32_t i = 0u; i < (uint32_t)OUT.n_logs; i++) {
        if (OUT.logs[i].code == kod) {
            return &OUT.logs[i];
        }
    }
    return NULL;
}

static void ozet_bozulmasi(bool tumleyende, uint64_t bit)
{
    g_seq = 0u;
    g_t = 2000 * DS_MS;
    TEST_ASSERT_EQUAL(IDA_RC_OK, ds_kur(&C));
    /* Geçerli kayıt: T_imu_stale 25 ms ile commit ve SAVE. */
    adim_hazirla();
    komut_ekle((uint8_t)IDA_CMD_PARAM_SET);
    IN.cmds[0].u.param.id = 0x0101u;
    IN.cmds[0].u.param.type = (uint8_t)IDA_PT_U32;
    IN.cmds[0].u.param.v.u32 = 25u;
    komut_ekle((uint8_t)IDA_CMD_PARAM_COMMIT);
    komut_ekle((uint8_t)IDA_CMD_PARAM_SAVE);
    TEST_ASSERT_EQUAL(IDA_RC_OK, ida_core_step(&C, &IN, &OUT));
    const ida_cmd_result_t *r = ds_sonuc(&OUT, g_cmd);
    TEST_ASSERT_NOT_NULL(r);
    TEST_ASSERT_EQUAL_UINT8(IDA_ACK_ACCEPTED, r->ack);
    TEST_ASSERT_EQUAL_UINT32(IDA_PRM_BLOB_BOYU, r->save.len);
    TEST_ASSERT_TRUE(OUT.health.core_ok);
    (void)memcpy(g_kayitli, r->save.blob, IDA_PRM_BLOB_BOYU);
    const uint32_t ret_once = OUT.status.cnt.cmd_rejected;

    /* Tek bit bozma: SAVE blok üretmez, bütünlük hatası (tamamlayıcı kopya, 2) bildirilir. */
    if (tumleyende) {
        C.prm.sema_ozeti_tumleyen ^= bit;
    } else {
        C.prm.sema_ozeti ^= bit;
    }
    r = save_adimi();
    TEST_ASSERT_EQUAL_UINT8(IDA_ACK_REJECTED, r->ack);
    TEST_ASSERT_EQUAL_UINT16(IDA_REASON_PARAM_INVALID, r->reason);
    TEST_ASSERT_TRUE(r->save.blob == NULL);
    TEST_ASSERT_EQUAL_UINT32(0u, r->save.len);
    TEST_ASSERT_EQUAL_UINT32(0u, r->save.gen);
    TEST_ASSERT_EQUAL_UINT32(ret_once + 1u, OUT.status.cnt.cmd_rejected);
    TEST_ASSERT_FALSE(OUT.health.core_ok);
    TEST_ASSERT_EQUAL_UINT32(1u, ds_gunluk_say(&OUT, (uint16_t)IDA_LOG_CORE_INTEGRITY));
    const ida_log_t *g = gunluk_bul((uint16_t)IDA_LOG_CORE_INTEGRITY);
    TEST_ASSERT_EQUAL_INT32(2, (g != NULL) ? g->arg0 : -1); /* 2: tamamlayıcı kopya */
    /* Mandallı: sonraki SAVE de reddedilir, core_ok 0 kalır, günlük yinelenmez. */
    r = save_adimi();
    TEST_ASSERT_EQUAL_UINT8(IDA_ACK_REJECTED, r->ack);
    TEST_ASSERT_TRUE(r->save.blob == NULL);
    TEST_ASSERT_FALSE(OUT.health.core_ok);
    TEST_ASSERT_EQUAL_UINT32(0u, ds_gunluk_say(&OUT, (uint16_t)IDA_LOG_CORE_INTEGRITY));

    /* Yeniden init: bozulmadan önceki geçerli kayıt okunur, önbellek yeniden kurulur. */
    ida_init_in_t ii;
    ds_init_girdisi(&ii);
    ii.param_blob = g_kayitli;
    ii.param_len = IDA_PRM_BLOB_BOYU;
    TEST_ASSERT_EQUAL(IDA_RC_OK, ida_core_init(&C, &ii));
    TEST_ASSERT_TRUE(C.prm.param_ok);
    TEST_ASSERT_TRUE(ida_prm_tutarli(&C.prm));
    TEST_ASSERT_EQUAL_UINT32(25u, ida_prm_u32(&C.prm, IDA_PRM_imu_stale_ms));
    g_t = 2000 * DS_MS;
    r = save_adimi();
    TEST_ASSERT_EQUAL_UINT8(IDA_ACK_ACCEPTED, r->ack);
    TEST_ASSERT_TRUE(OUT.health.core_ok);
    TEST_ASSERT_EQUAL_HEX8_ARRAY(g_kayitli, r->save.blob, IDA_PRM_BLOB_BOYU);
}

static void test_ozet_bozulmasi_save_uretmez(void)
{
    ozet_bozulmasi(false, UINT64_C(1));
    ozet_bozulmasi(false, UINT64_C(1) << 63);
}

static void test_tumleyen_bozulmasi_save_uretmez(void)
{
    ozet_bozulmasi(true, UINT64_C(1));
    ozet_bozulmasi(true, UINT64_C(1) << 37);
}

/* sol-2 bulgu 1 (beyaz kutu): blok, önbellekten bir kez okunup doğrulanan değerle kodlanır.
   Tutarlı ama tablonunkinden farklı bir çift (X, ~X) yazılır: SAVE kabul edilir ve başlıkta
   tam olarak X bulunur (özet yeniden hesaplanmaz, doğrulanan yerel değer kodlayıcıya geçer).
   Denetimle kullanım arasına bozma enjekte eden bir kanca çekirdekte yoktur (M9: test kipi
   yok); tek okuma ida_prm_ozet_al'ın volatile okumasıyla yapısal olarak sağlanır. */
static void test_save_dogrulanan_yerel_ozetle_kodlar(void)
{
    g_seq = 0u;
    g_t = 2000 * DS_MS;
    TEST_ASSERT_EQUAL(IDA_RC_OK, ds_kur(&C));
    const uint64_t x = ALTIN_OZET ^ UINT64_C(0x5A5A00000000A5A5);
    C.prm.sema_ozeti = x;
    C.prm.sema_ozeti_tumleyen = UINT64_MAX ^ x;
    uint64_t okunan = 0u;
    TEST_ASSERT_TRUE(ida_prm_ozet_al(&C.prm, &okunan));
    TEST_ASSERT_TRUE(okunan == x);
    const ida_cmd_result_t *r = save_adimi();
    TEST_ASSERT_EQUAL_UINT8(IDA_ACK_ACCEPTED, r->ack);
    TEST_ASSERT_TRUE(OUT.health.core_ok);
    const uint8_t *b = r->save.blob;
    TEST_ASSERT_NOT_NULL(b);
    if (b != NULL) {
        const uint64_t baslik = (uint64_t)b[8] | ((uint64_t)b[9] << 8) | ((uint64_t)b[10] << 16) |
                                ((uint64_t)b[11] << 24) | ((uint64_t)b[12] << 32) |
                                ((uint64_t)b[13] << 40) | ((uint64_t)b[14] << 48) |
                                ((uint64_t)b[15] << 56);
        TEST_ASSERT_TRUE(baslik == x);
    }
}

/* sol-2 bulgu 2 (birim): çağrı sonundaki SAVE kararı önbelleği kendisi denetler. Çağrı başı
   denetiminden sonra bozulmuş önbellek (burada doğrudan kurulur) için ida_prm_cagri_sonu 2
   döner (çerçeve bunu aynı çağrının core_ok ve CORE_INTEGRITY çıktısına taşır), blok
   üretmez ve gen tüketmez; tutarlı önbellekte 0 döner ve blok üretir. */
static int32_t cagri_sonu_save(ida_out_t *out, ida_core_counters_t *cnt)
{
    ida_prm_cagri_basi(&C.prm);
    C.prm.save_bekliyor = true;
    C.prm.save_indis = 0u;
    (void)memset(out, 0, sizeof *out);
    out->results[0].cmd_id = 77u;
    out->results[0].kind = (uint8_t)IDA_RES_PARAM_SAVE;
    out->results[0].ack = (uint8_t)IDA_ACK_ACCEPTED; /* param_save'in geçici sonucu */
    const ida_prm_komut_baglam_t b = {.k = NULL, .plat_param_gen = 5u, .gun = &C.gun,
                                      .sonuc_indisi = 0u, .armed = false};
    return ida_prm_cagri_sonu(&C.prm, &b, out, cnt);
}

static void test_cagri_sonu_save_bozulmayi_bildirir(void)
{
    for (uint32_t tur = 0u; tur < 2u; tur++) {
        TEST_ASSERT_EQUAL(IDA_RC_OK, ds_kur(&C));
        ida_core_counters_t cnt;
        (void)memset(&cnt, 0, sizeof cnt);
        if (tur == 0u) {
            C.prm.sema_ozeti ^= UINT64_C(1);
        } else {
            C.prm.sema_ozeti_tumleyen ^= UINT64_C(1) << 40;
        }
        const uint32_t gen_once = C.prm.kayit_gen;
        TEST_ASSERT_EQUAL_INT32(2, cagri_sonu_save(&OUT, &cnt));
        TEST_ASSERT_EQUAL_UINT8(IDA_ACK_REJECTED, OUT.results[0].ack);
        TEST_ASSERT_EQUAL_UINT16(IDA_REASON_PARAM_INVALID, OUT.results[0].reason);
        TEST_ASSERT_TRUE(OUT.results[0].save.blob == NULL);
        TEST_ASSERT_EQUAL_UINT32(0u, OUT.results[0].save.gen);
        TEST_ASSERT_EQUAL_UINT32(gen_once, C.prm.kayit_gen); /* gen tüketilmedi */
        TEST_ASSERT_FALSE(C.prm.kayit_bekliyor);
        TEST_ASSERT_EQUAL_UINT32(1u, cnt.cmd_rejected);
    }
    TEST_ASSERT_EQUAL(IDA_RC_OK, ds_kur(&C));
    ida_core_counters_t cnt;
    (void)memset(&cnt, 0, sizeof cnt);
    TEST_ASSERT_EQUAL_INT32(0, cagri_sonu_save(&OUT, &cnt));
    TEST_ASSERT_EQUAL_UINT8(IDA_ACK_ACCEPTED, OUT.results[0].ack);
    TEST_ASSERT_TRUE(OUT.results[0].save.blob != NULL);
    TEST_ASSERT_EQUAL_UINT32(6u, OUT.results[0].save.gen);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_crc_bilinen_vektorler);
    RUN_TEST(test_crc_tablonun_her_satiri);
    RUN_TEST(test_crc_rastgele_10000);
    RUN_TEST(test_blok_varsayilan_ve_uc_degerler);
    RUN_TEST(test_blok_rastgele_1000);
    RUN_TEST(test_param_save_cikti_blogu);
    RUN_TEST(test_sema_ozeti_onbellegi);
    RUN_TEST(test_sema_ozeti_tablodan_turer);
    RUN_TEST(test_sema_ozeti_altin_deger);
    RUN_TEST(test_blok_i32_duzenegi);
    RUN_TEST(test_ozet_bozulmasi_save_uretmez);
    RUN_TEST(test_tumleyen_bozulmasi_save_uretmez);
    RUN_TEST(test_save_dogrulanan_yerel_ozetle_kodlar);
    RUN_TEST(test_cagri_sonu_save_bozulmayi_bildirir);
    return UNITY_END();
}
