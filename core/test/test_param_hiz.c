/*
 * test_param_hiz.c — W4-param-hiz: tablolu CRC ve şema özeti önbelleğinin, değişiklikten önceki
 * bit bit uygulamayla bayt bayt aynı sonucu verdiği (A4 §3.10; biçim, CRC tanımı ve özet
 * değişmez).
 *
 * Referans: değişiklikten önceki ida_crc32 (bit bit) ve ida_prm_blok_kodla (her kodlamada özeti
 * tablodan yeniden hesaplayan) bu dosyada aynen tutulur. Rastgele durumlar sabit tohumludur.
 */
#include <string.h>

#include "destek.h"
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

static uint32_t ref_blok_kodla(const ida_prm_kume_t *k, uint8_t *blok)
{
    const uint32_t yuk = IDA_PBLOB_REC_SIZE * (uint32_t)IDA_PRM_SAYI;
    ref_yaz32(&blok[0], IDA_PBLOB_MAGIC);
    ref_yaz16(&blok[4], IDA_PBLOB_FMT_VER);
    ref_yaz16(&blok[6], 0u);
    const uint64_t ozet = ida_prm_sema_ozeti(); /* her kodlamada tablodan */
    ref_yaz32(&blok[8], (uint32_t)ozet);
    ref_yaz32(&blok[12], (uint32_t)(ozet >> 32));
    ref_yaz32(&blok[16], yuk);
    for (uint32_t r = 0u; r < (uint32_t)IDA_PRM_SAYI; r++) {
        ida_param_desc_t d = {0};
        TEST_ASSERT_EQUAL(IDA_RC_OK, ida_core_param_desc((uint16_t)r, &d));
        uint8_t *kay = &blok[IDA_PBLOB_HDR_SIZE + (r * IDA_PBLOB_REC_SIZE)];
        ref_yaz16(kay, d.id);
        kay[2] = d.type;
        kay[3] = 0u;
        ref_yaz32(&kay[4], k->ham[r]);
    }
    ref_yaz32(&blok[20], ref_crc32(&blok[IDA_PBLOB_HDR_SIZE], yuk));
    return IDA_PBLOB_HDR_SIZE + yuk;
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
    /* Her parametre kendi tipinin en az ve en çok değerinde (I32 ikiye tümleyen, F32 bitleri). */
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
    /* Tipten bağımsız bit desenleri: 0, tüm birler, INT32_MIN / -0.0, INT32_MAX, sessiz NaN,
       +sonsuz, en küçük alt normal. */
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
    ida_param_t p;
    (void)memset(&p, 0, sizeof p);
    ida_prm_kur(&p);
    TEST_ASSERT_TRUE(p.sema_ozeti == info.param_schema_hash);
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
    return UNITY_END();
}
