/*
 * test_param.c — parametre tablosu, şema özeti, kodlanmış blok ve parametre komutları
 * (A4-K39 aralığı, K78–K85, K100, K112; 3.10; 4.15 parametre satırları).
 */
#include <string.h>

#include "destek.h"
#include "param/ozet.h"
#include "unity.h"
#include "zaman/zaman.h"

void setUp(void);
void tearDown(void);
void setUp(void) {}
void tearDown(void) {}

static ida_core_t C;
static ida_in_t IN;
static ida_out_t OUT;
static uint8_t BLOK[IDA_PARAM_BLOB_MAX];

static void ozet_hex(const char *metin, const uint8_t beklenen[32])
{
    ida_sha256_t s;
    ida_sha256_basla(&s);
    ida_sha256_ekle(&s, (const uint8_t *)metin, (uint32_t)strlen(metin));
    uint8_t o[32];
    ida_sha256_bitir(&s, o);
    TEST_ASSERT_EQUAL_HEX8_ARRAY(beklenen, o, 32);
}

static void test_sha256_ve_crc(void)
{
    static const uint8_t abc[32] = {
        0xba, 0x78, 0x16, 0xbf, 0x8f, 0x01, 0xcf, 0xea, 0x41, 0x41, 0x40, 0xde, 0x5d, 0xae, 0x22, 0x23,
        0xb0, 0x03, 0x61, 0xa3, 0x96, 0x17, 0x7a, 0x9c, 0xb4, 0x10, 0xff, 0x61, 0xf2, 0x00, 0x15, 0xad};
    static const uint8_t bos[32] = {
        0xe3, 0xb0, 0xc4, 0x42, 0x98, 0xfc, 0x1c, 0x14, 0x9a, 0xfb, 0xf4, 0xc8, 0x99, 0x6f, 0xb9, 0x24,
        0x27, 0xae, 0x41, 0xe4, 0x64, 0x9b, 0x93, 0x4c, 0xa4, 0x95, 0x99, 0x1b, 0x78, 0x52, 0xb8, 0x55};
    static const uint8_t uzun[32] = {/* "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq" */
        0x24, 0x8d, 0x6a, 0x61, 0xd2, 0x06, 0x38, 0xb8, 0xe5, 0xc0, 0x26, 0x93, 0x0c, 0x3e, 0x60, 0x39,
        0xa3, 0x3c, 0xe4, 0x59, 0x64, 0xff, 0x21, 0x67, 0xf6, 0xec, 0xed, 0xd4, 0x19, 0xdb, 0x06, 0xc1};
    ozet_hex("abc", abc);
    ozet_hex("", bos);
    ozet_hex("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq", uzun);
    TEST_ASSERT_EQUAL_HEX32(0xCBF43926u, ida_crc32((const uint8_t *)"123456789", 9u));
}

/* Ondalık yazım (kartta printf ailesi bellek ayırdığı için kullanılmaz). */
static void ondalik_ekle(ida_sha256_t *s, uint32_t x)
{
    uint8_t r[10] = {0u};
    uint32_t n = 0u;
    do {
        r[n] = (uint8_t)((uint8_t)'0' + (uint8_t)(x % 10u));
        n++;
        x /= 10u;
    } while (x != 0u);
    while (n > 0u) {
        n--;
        ida_sha256_ekle(s, &r[n], 1u);
    }
}

/* Şema özeti, port işlevlerinden bağımsız kurulan kanonik metnin özetidir (3.10):
   "<id>\t<ad>\t<type>\n", kimlik artan sırada. */
static void test_sema_ozeti_kanonik_metin(void)
{
    ida_sha256_t s;
    ida_sha256_basla(&s);
    const uint8_t sekme = (uint8_t)'\t';
    const uint8_t satir = (uint8_t)'\n';
    const uint16_t n = ida_core_param_count();
    for (uint16_t i = 0u; i < n; i++) {
        ida_param_desc_t d;
        TEST_ASSERT_EQUAL(IDA_RC_OK, ida_core_param_desc(i, &d));
        ondalik_ekle(&s, d.id);
        ida_sha256_ekle(&s, &sekme, 1u);
        ida_sha256_ekle(&s, (const uint8_t *)d.name, (uint32_t)strlen(d.name));
        ida_sha256_ekle(&s, &sekme, 1u);
        ondalik_ekle(&s, d.type);
        ida_sha256_ekle(&s, &satir, 1u);
    }
    uint8_t o[32];
    ida_sha256_bitir(&s, o);
    uint64_t beklenen = 0u;
    for (uint32_t i = 0u; i < 8u; i++) {
        beklenen |= (uint64_t)o[i] << (8u * i);
    }
    ida_core_info_t info;
    ida_core_info(&info);
    TEST_ASSERT_TRUE(beklenen == info.param_schema_hash);
}

static void test_tablo_ve_port_islevleri(void)
{
    const uint16_t n = ida_core_param_count();
    TEST_ASSERT_EQUAL_UINT16(9u, n);
    ida_param_desc_t d;
    ida_param_desc_t onceki = {0};
    for (uint16_t i = 0u; i < n; i++) {
        TEST_ASSERT_EQUAL(IDA_RC_OK, ida_core_param_desc(i, &d));
        if (i > 0u) {
            TEST_ASSERT_TRUE(d.id > onceki.id); /* kesin artan */
        }
        uint16_t id = 0u;
        TEST_ASSERT_EQUAL(IDA_RC_OK, ida_core_param_find(d.name, &id));
        TEST_ASSERT_EQUAL_UINT16(d.id, id);
        onceki = d;
    }
    /* Hata halinde çıktı yazılmaz (4.15). */
    (void)memset(&d, 0x5A, sizeof(d));
    ida_param_desc_t kopya = d;
    TEST_ASSERT_EQUAL(IDA_RC_NOT_FOUND, ida_core_param_desc(n, &d));
    TEST_ASSERT_EQUAL_MEMORY(&kopya, &d, sizeof(d));
    TEST_ASSERT_EQUAL(IDA_RC_BAD_INPUT, ida_core_param_desc(0u, NULL));
    uint16_t id = 777u;
    TEST_ASSERT_EQUAL(IDA_RC_BAD_INPUT, ida_core_param_find(NULL, &id));
    TEST_ASSERT_EQUAL(IDA_RC_BAD_INPUT, ida_core_param_find("T_imu_stale", NULL));
    TEST_ASSERT_EQUAL(IDA_RC_NOT_FOUND, ida_core_param_find("T_imu_stal", &id));
    TEST_ASSERT_EQUAL(IDA_RC_NOT_FOUND, ida_core_param_find("T_imu_stale_ABCD", &id)); /* 16 */
    TEST_ASSERT_EQUAL(IDA_RC_NOT_FOUND, ida_core_param_find("FC_APPLY_BY_NS_AB", &id)); /* 17 */
    TEST_ASSERT_EQUAL(IDA_RC_NOT_FOUND, ida_core_param_find("", &id));
    TEST_ASSERT_EQUAL_UINT16(777u, id);
    TEST_ASSERT_EQUAL(IDA_RC_OK, ida_core_param_find("T_lag_set", &id));
    TEST_ASSERT_EQUAL_UINT16(0x0103u, id);

    ida_param_value_t v;
    (void)memset(&v, 0x5A, sizeof(v));
    const ida_param_value_t vk = v;
    TEST_ASSERT_EQUAL(IDA_RC_BAD_INPUT, ida_core_param_get(NULL, 0x0101u, &v));
    TEST_ASSERT_EQUAL(IDA_RC_BAD_INPUT, ida_core_param_get(&C, 0x0101u, NULL));
    (void)memset(&C, 0, sizeof(C));
    TEST_ASSERT_EQUAL(IDA_RC_NOT_INIT, ida_core_param_get(&C, 0x0101u, &v));
    TEST_ASSERT_EQUAL(IDA_RC_OK, ds_kur(&C));
    TEST_ASSERT_EQUAL(IDA_RC_NOT_FOUND, ida_core_param_get(&C, 0x0999u, &v));
    TEST_ASSERT_EQUAL_MEMORY(&vk, &v, sizeof(v));
    TEST_ASSERT_EQUAL(IDA_RC_OK, ida_core_param_get(&C, 0x0101u, &v));
    TEST_ASSERT_EQUAL_UINT8(IDA_PT_U32, v.type);
    TEST_ASSERT_EQUAL_UINT32(20u, v.v.u32); /* T_imu_stale, ms */
    TEST_ASSERT_EQUAL(IDA_RC_OK, ida_core_param_get(&C, 0x0106u, &v));
    TEST_ASSERT_EQUAL_FLOAT(0.1f, v.v.f32);
}


/* ---- A4 §3.9 parametre tablosu: ad, birim, tip, varsayılan, aralık birebir ---------------- */

typedef struct {
    const char *ad;
    uint8_t tip;
    uint32_t en_az;
    uint32_t en_cok;
    uint32_t varsayilan;
} tablo_satiri_t;

static void tablo_satiri(const tablo_satiri_t *t)
{
    uint16_t id = 0u;
    TEST_ASSERT_EQUAL(IDA_RC_OK, ida_core_param_find(t->ad, &id));
    ida_param_desc_t d;
    uint16_t indis = 0u;
    TEST_ASSERT_TRUE(ida_prm_indis(id, &indis));
    TEST_ASSERT_EQUAL(IDA_RC_OK, ida_core_param_desc(indis, &d));
    TEST_ASSERT_EQUAL_STRING(t->ad, d.name);
    TEST_ASSERT_EQUAL_UINT8(t->tip, d.type);
    TEST_ASSERT_EQUAL_UINT32(t->en_az, d.min.v.u32);
    TEST_ASSERT_EQUAL_UINT32(t->en_cok, d.max.v.u32);
    TEST_ASSERT_EQUAL_UINT32(t->varsayilan, d.def.v.u32);
}

static void test_39_parametre_tablosu(void)
{
    static const tablo_satiri_t SATIRLAR[] = {
        {"T_lag_set", (uint8_t)IDA_PT_U32, 10u, 50u, 20u},
        {"T_lag_clear", (uint8_t)IDA_PT_U32, 1u, 50u, 10u},
        {"N_lag_clear", (uint8_t)IDA_PT_U32, 1u, 200u, 20u},
        {"T_imu_stale", (uint8_t)IDA_PT_U32, 5u, 30u, 20u},
        {"T_det_stale", (uint8_t)IDA_PT_U32, 100u, 10000u, 2000u},
    };
    for (uint32_t i = 0u; i < (uint32_t)(sizeof(SATIRLAR) / sizeof(SATIRLAR[0])); i++) {
        tablo_satiri(&SATIRLAR[i]);
    }
    /* T_imu_gap_tol: oran, F32, 0,01–1, varsayılan 0,1 */
    uint16_t id = 0u;
    uint16_t indis = 0u;
    TEST_ASSERT_EQUAL(IDA_RC_OK, ida_core_param_find("T_imu_gap_tol", &id));
    TEST_ASSERT_TRUE(ida_prm_indis(id, &indis));
    ida_param_desc_t d;
    TEST_ASSERT_EQUAL(IDA_RC_OK, ida_core_param_desc(indis, &d));
    TEST_ASSERT_EQUAL_UINT8(IDA_PT_F32, d.type);
    TEST_ASSERT_EQUAL_FLOAT(0.01f, d.min.v.f32);
    TEST_ASSERT_EQUAL_FLOAT(1.0f, d.max.v.f32);
    TEST_ASSERT_EQUAL_FLOAT(0.1f, d.def.v.f32);
    /* port.h'nin ns sabitleri değişmez; ms × 1 000 000 ile tutarlı (3.9). */
    TEST_ASSERT_TRUE(ida_ms_ns(5u) == (int64_t)IDA_IMU_STALE_MIN_NS);
    TEST_ASSERT_TRUE(ida_ms_ns(30u) == (int64_t)IDA_IMU_STALE_MAX_NS);
    TEST_ASSERT_TRUE(ida_ms_ns(20u) == (int64_t)IDA_IMU_STALE_DEFAULT_NS);
    TEST_ASSERT_TRUE(ida_ms_ns(2000u) == (int64_t)IDA_DET_STALE_DEFAULT_NS);
    TEST_ASSERT_TRUE(ida_ms_ns(10000u) == 10000000000LL); /* int64_t'de taşmaz */
}

/* ---- Blok doğrulaması (A4-K80) ---------------------------------------------------------- */

static void yaz32(uint8_t *b, uint32_t x)
{
    b[0] = (uint8_t)x;
    b[1] = (uint8_t)(x >> 8);
    b[2] = (uint8_t)(x >> 16);
    b[3] = (uint8_t)(x >> 24);
}

static void crc_duzelt(uint8_t *b, uint32_t n)
{
    yaz32(&b[20], ida_crc32(&b[24], n - 24u));
}

static void init_beklenen(uint32_t n, ida_rc_t rc, int32_t denetim)
{
    ida_init_in_t ii;
    ds_init_girdisi(&ii);
    ii.param_blob = BLOK;
    ii.param_len = n;
    TEST_ASSERT_EQUAL(rc, ida_core_init(&C, &ii));
    ds_girdi(&IN, 2000 * DS_MS, 1u, true);
    (void)ida_core_step(&C, &IN, &OUT);
    TEST_ASSERT_EQUAL_UINT32(1u, ds_gunluk_say(&OUT, (uint16_t)IDA_LOG_PARAM_LOAD));
    TEST_ASSERT_EQUAL_INT32((int32_t)rc, OUT.logs[1].arg0);
    TEST_ASSERT_EQUAL_INT32(denetim, OUT.logs[1].arg1);
    TEST_ASSERT_EQUAL((rc == IDA_RC_OK), OUT.status.param_ok);
}

/* İndisi verilen kaydın değerini yazar (kayıtlar 24 + 8·i'de; değer +4). */
static void kayit_deger(uint32_t indis, uint32_t ham)
{
    yaz32(&BLOK[24u + (8u * indis) + 4u], ham);
}

static void test_blok_denetimleri(void)
{
    uint32_t n = ds_gecerli_blok(BLOK);
    init_beklenen(n, IDA_RC_OK, 0);
    /* (1) magic */
    n = ds_gecerli_blok(BLOK);
    BLOK[0] = (uint8_t)(BLOK[0] ^ 0xFFu);
    init_beklenen(n, IDA_RC_PARAM_DEFAULTS, 1);
    n = ds_gecerli_blok(BLOK);
    init_beklenen(10u, IDA_RC_PARAM_DEFAULTS, 1); /* başlıktan kısa */
    /* (2) şema özeti */
    n = ds_gecerli_blok(BLOK);
    BLOK[8] = (uint8_t)(BLOK[8] ^ 0x01u);
    init_beklenen(n, IDA_RC_PARAM_DEFAULTS, 2);
    /* (3) boy */
    n = ds_gecerli_blok(BLOK);
    init_beklenen(n - 8u, IDA_RC_PARAM_DEFAULTS, 3);
    /* (4) CRC */
    n = ds_gecerli_blok(BLOK);
    BLOK[30] = (uint8_t)(BLOK[30] ^ 0x01u);
    init_beklenen(n, IDA_RC_PARAM_DEFAULTS, 4);
    /* (5) sıra ve kayıt rsv */
    n = ds_gecerli_blok(BLOK);
    BLOK[24u + 8u + 3u] = 1u;
    crc_duzelt(BLOK, n);
    init_beklenen(n, IDA_RC_PARAM_DEFAULTS, 5);
    n = ds_gecerli_blok(BLOK);
    BLOK[24u + 8u] = BLOK[24u]; /* ikinci kaydın kimliği birinciye eşit */
    BLOK[24u + 8u + 1u] = BLOK[24u + 1u];
    crc_duzelt(BLOK, n);
    init_beklenen(n, IDA_RC_PARAM_DEFAULTS, 5);
    /* (6) tür */
    n = ds_gecerli_blok(BLOK);
    BLOK[24u + 2u] = (uint8_t)IDA_PT_F32;
    crc_duzelt(BLOK, n);
    init_beklenen(n, IDA_RC_PARAM_DEFAULTS, 6);
    /* (7) aralık: T_imu_stale 4 ms ve 31 ms (§7 A4-K39; 3.9 birimi ms) */
    n = ds_gecerli_blok(BLOK);
    kayit_deger(IDA_PRM_imu_stale_ms, 4u);
    crc_duzelt(BLOK, n);
    init_beklenen(n, IDA_RC_PARAM_DEFAULTS, 7);
    n = ds_gecerli_blok(BLOK);
    kayit_deger(IDA_PRM_imu_stale_ms, 31u);
    crc_duzelt(BLOK, n);
    init_beklenen(n, IDA_RC_PARAM_DEFAULTS, 7);
    n = ds_gecerli_blok(BLOK);
    kayit_deger(IDA_PRM_imu_stale_ms, 30u); /* sınır içi */
    crc_duzelt(BLOK, n);
    init_beklenen(n, IDA_RC_OK, 0);
    /* (8) grup: T_lag_clear (30 ms) > T_lag_set (20 ms) */
    n = ds_gecerli_blok(BLOK);
    kayit_deger(IDA_PRM_lag_clear_ms, 30u);
    crc_duzelt(BLOK, n);
    init_beklenen(n, IDA_RC_PARAM_DEFAULTS, 8);
    /* param_len > 8000: denetim 3 */
    n = ds_gecerli_blok(BLOK);
    init_beklenen(IDA_PARAM_BLOB_MAX + 1u, IDA_RC_PARAM_DEFAULTS, 3);
    /* 24 bayttan kısa blok: denetim 1 (A4-K80); param_blob NULL değilken param_len = 0: blok
       yok, numara 0 (3.15 PARAM_LOAD arg1). */
    init_beklenen(23u, IDA_RC_PARAM_DEFAULTS, 1);
    init_beklenen(0u, IDA_RC_PARAM_DEFAULTS, 0);
}

/* ---- Parametre komutları ---------------------------------------------------------------- */

static uint32_t g_seq;
static int64_t g_t;

static ida_rc_t cagir(void)
{
    g_seq++;
    g_t += 5 * DS_MS;
    IN.now_w.ns = g_t;
    IN.tf.seq = g_seq;
    IN.tf.t.ns = g_t;
    IN.tf.t_rx.ns = g_t;
    ds_imu(&IN.imu, g_t, g_seq);
    return ida_core_step(&C, &IN, &OUT);
}

static void komut(uint32_t i, uint32_t id, uint8_t tur)
{
    IN.cmds[i].cmd_id = id;
    IN.cmds[i].type = tur;
    if (IN.n_cmds <= i) {
        IN.n_cmds = (uint8_t)(i + 1u);
    }
}

static void param_set(uint32_t i, uint32_t id, uint16_t pid, uint32_t ham)
{
    komut(i, id, (uint8_t)IDA_CMD_PARAM_SET);
    IN.cmds[i].u.param.id = pid;
    IN.cmds[i].u.param.type = (uint8_t)IDA_PT_U32;
    IN.cmds[i].u.param.v.u32 = ham;
}

static void hazirla(bool gecerli_blok)
{
    g_seq = 0u;
    g_t = 2000 * DS_MS;
    TEST_ASSERT_EQUAL(gecerli_blok ? IDA_RC_OK : IDA_RC_PARAM_DEFAULTS,
                      gecerli_blok ? ds_kur(&C) : ds_kur_bloksuz(&C));
    ds_girdi(&IN, g_t, 0u, true);
}

static const ida_cmd_result_t *sonuc_al(uint32_t id)
{
    const ida_cmd_result_t *r = ds_sonuc(&OUT, id);
    TEST_ASSERT_NOT_NULL(r);
    return r;
}

static void test_set_get_commit(void)
{
    hazirla(true);
    param_set(0u, 1u, 0x0999u, 5u);                      /* bilinmeyen */
    param_set(1u, 2u, 0x0101u, 4u);                      /* aralık dışı (T_imu_stale 5–30 ms) */
    param_set(2u, 3u, 0x0101u, 25u);                     /* geçerli */
    komut(3u, 4u, (uint8_t)IDA_CMD_PARAM_SET);           /* tür uyuşmuyor */
    IN.cmds[3].u.param.id = 0x0106u;
    IN.cmds[3].u.param.type = (uint8_t)IDA_PT_U32;
    komut(4u, 5u, (uint8_t)IDA_CMD_PARAM_GET);
    IN.cmds[4].u.param.id = 0x0101u;
    IN.cmds[4].u.param.type = 99u; /* PARAM_GET'te okunmaz (3.14) */
    TEST_ASSERT_EQUAL(IDA_RC_OK, cagir());
    ds_cikti_degismezleri(IDA_RC_OK, &IN, &OUT);
    TEST_ASSERT_EQUAL_UINT16(IDA_REASON_PARAM_INVALID, sonuc_al(1u)->reason);
    TEST_ASSERT_EQUAL_UINT16(IDA_REASON_PARAM_INVALID, sonuc_al(2u)->reason);
    /* Reddedilen PARAM_SET: id yankılanır, type ve v 0 (3.14; A4-K43). */
    TEST_ASSERT_EQUAL_UINT8(IDA_RES_PARAM_VALUE, sonuc_al(1u)->kind);
    TEST_ASSERT_EQUAL_UINT16(0x0999u, sonuc_al(1u)->value.id);
    TEST_ASSERT_EQUAL_UINT16(0x0101u, sonuc_al(2u)->value.id);
    TEST_ASSERT_EQUAL_UINT8(0u, sonuc_al(2u)->value.type);
    TEST_ASSERT_EQUAL_UINT32(0u, sonuc_al(2u)->value.v.u32);
    TEST_ASSERT_EQUAL_UINT8(IDA_ACK_ACCEPTED, sonuc_al(3u)->ack);
    TEST_ASSERT_EQUAL_UINT8(IDA_RES_PARAM_VALUE, sonuc_al(3u)->kind);
    TEST_ASSERT_EQUAL_UINT32(25u, sonuc_al(3u)->value.v.u32);
    TEST_ASSERT_EQUAL_UINT16(IDA_REASON_PARAM_INVALID, sonuc_al(4u)->reason);
    TEST_ASSERT_EQUAL_UINT8(0u, sonuc_al(4u)->value.type);
    TEST_ASSERT_EQUAL_UINT8(IDA_ACK_ACCEPTED, sonuc_al(5u)->ack);
    TEST_ASSERT_EQUAL_UINT32(20u, sonuc_al(5u)->value.v.u32); /* bekletilir */
    /* COMMIT: uygulanır (adım sınırında); sonraki GET yeni değeri verir. */
    IN.n_cmds = 0u;
    komut(0u, 6u, (uint8_t)IDA_CMD_PARAM_COMMIT);
    TEST_ASSERT_EQUAL(IDA_RC_OK, cagir());
    TEST_ASSERT_EQUAL_UINT8(IDA_ACK_ACCEPTED, sonuc_al(6u)->ack);
    TEST_ASSERT_EQUAL_UINT32(1u, ds_gunluk_say(&OUT, (uint16_t)IDA_LOG_PARAM_COMMIT));
    IN.n_cmds = 0u;
    komut(0u, 7u, (uint8_t)IDA_CMD_PARAM_GET);
    IN.cmds[0].u.param.id = 0x0101u;
    TEST_ASSERT_EQUAL(IDA_RC_OK, cagir());
    TEST_ASSERT_EQUAL_UINT32(25u, sonuc_al(7u)->value.v.u32);
    /* Reddedilen PARAM_GET: id yankılanır, type ve v 0. */
    IN.n_cmds = 0u;
    komut(0u, 70u, (uint8_t)IDA_CMD_PARAM_GET);
    IN.cmds[0].u.param.id = 0x0999u;
    TEST_ASSERT_EQUAL(IDA_RC_OK, cagir());
    TEST_ASSERT_EQUAL_UINT16(IDA_REASON_PARAM_INVALID, sonuc_al(70u)->reason);
    TEST_ASSERT_EQUAL_UINT16(0x0999u, sonuc_al(70u)->value.id);
    TEST_ASSERT_EQUAL_UINT8(0u, sonuc_al(70u)->value.type);
    TEST_ASSERT_EQUAL_UINT32(0u, sonuc_al(70u)->value.v.u32);
    /* Grup denetimi geçmeyen commit hepsini reddeder. */
    IN.n_cmds = 0u;
    param_set(0u, 8u, 0x0104u, 40u); /* T_lag_clear 40 ms > T_lag_set 20 ms */
    komut(1u, 9u, (uint8_t)IDA_CMD_PARAM_COMMIT);
    TEST_ASSERT_EQUAL(IDA_RC_OK, cagir());
    TEST_ASSERT_EQUAL_UINT16(IDA_REASON_PARAM_INVALID, sonuc_al(9u)->reason);
    ida_param_value_t v;
    TEST_ASSERT_EQUAL(IDA_RC_OK, ida_core_param_get(&C, 0x0104u, &v));
    TEST_ASSERT_EQUAL_UINT32(10u, v.v.u32);
}

static void test_b_sinifi_ve_arm(void)
{
    hazirla(true);
    C.emn.armed = true; /* test kancası: silahlı */
    param_set(0u, 1u, 0x0101u, 25u);
    TEST_ASSERT_EQUAL(IDA_RC_OK, cagir());
    TEST_ASSERT_EQUAL_UINT16(IDA_REASON_ARMED, sonuc_al(1u)->reason);
    /* silahsızken bekletilir; ARM ile aynı çağrıdaki commit reddedilir ve bekleyenler atılır */
    C.emn.armed = false;
    IN.n_cmds = 0u;
    param_set(0u, 2u, 0x0101u, 25u);
    TEST_ASSERT_EQUAL(IDA_RC_OK, cagir());
    IN.n_cmds = 0u;
    komut(0u, 3u, (uint8_t)IDA_CMD_PARAM_COMMIT);
    komut(1u, 4u, (uint8_t)IDA_CMD_ARM);
    TEST_ASSERT_EQUAL(IDA_RC_OK, cagir());
    TEST_ASSERT_EQUAL_UINT16(IDA_REASON_ARMED, sonuc_al(3u)->reason);
    /* ARM önce, COMMIT sonra: aynı sonuç (sırasından bağımsız) */
    IN.n_cmds = 0u;
    param_set(0u, 5u, 0x0101u, 25u);
    TEST_ASSERT_EQUAL(IDA_RC_OK, cagir());
    IN.n_cmds = 0u;
    komut(0u, 6u, (uint8_t)IDA_CMD_ARM);
    komut(1u, 7u, (uint8_t)IDA_CMD_PARAM_COMMIT);
    TEST_ASSERT_EQUAL(IDA_RC_OK, cagir());
    TEST_ASSERT_EQUAL_UINT16(IDA_REASON_ARMED, sonuc_al(7u)->reason);
    /* bekleyenler atıldı: boş commit etkisiz kabul, değer değişmedi */
    IN.n_cmds = 0u;
    komut(0u, 8u, (uint8_t)IDA_CMD_PARAM_COMMIT);
    TEST_ASSERT_EQUAL(IDA_RC_OK, cagir());
    TEST_ASSERT_EQUAL_UINT8(IDA_ACK_ACCEPTED, sonuc_al(8u)->ack);
    TEST_ASSERT_EQUAL_UINT32(20u, ida_prm_u32(&C.prm, IDA_PRM_imu_stale_ms));
}

static void test_save_ve_k78(void)
{
    hazirla(false); /* varsayılanlar: param_ok = 0 */
    komut(0u, 1u, (uint8_t)IDA_CMD_PARAM_SAVE); /* commit yok */
    komut(1u, 2u, (uint8_t)IDA_CMD_ARM);
    TEST_ASSERT_EQUAL(IDA_RC_OK, cagir());
    TEST_ASSERT_EQUAL_UINT16(IDA_REASON_ARMED, sonuc_al(1u)->reason);      /* ARM ile aynı çağrı */
    TEST_ASSERT_EQUAL_UINT16(IDA_REASON_PARAM_INVALID, sonuc_al(2u)->reason); /* A4-K78 */
    IN.n_cmds = 0u;
    komut(0u, 3u, (uint8_t)IDA_CMD_PARAM_SAVE);
    TEST_ASSERT_EQUAL(IDA_RC_OK, cagir());
    TEST_ASSERT_EQUAL_UINT16(IDA_REASON_PARAM_INVALID, sonuc_al(3u)->reason);
    TEST_ASSERT_EQUAL_UINT8(IDA_RES_PARAM_SAVE, sonuc_al(3u)->kind);
    /* commit (boş, etkisiz) + iki SAVE: ilki kabul, ikincisi ONE_PER_CALL */
    IN.n_cmds = 0u;
    komut(0u, 4u, (uint8_t)IDA_CMD_PARAM_COMMIT);
    komut(1u, 5u, (uint8_t)IDA_CMD_PARAM_SAVE);
    komut(2u, 6u, (uint8_t)IDA_CMD_PARAM_SAVE);
    IN.plat.param_gen = 41u;
    TEST_ASSERT_EQUAL(IDA_RC_OK, cagir());
    ds_cikti_degismezleri(IDA_RC_OK, &IN, &OUT);
    const ida_cmd_result_t *r = sonuc_al(5u);
    TEST_ASSERT_EQUAL_UINT8(IDA_ACK_ACCEPTED, r->ack);
    TEST_ASSERT_EQUAL_UINT32(42u, r->save.gen);
    TEST_ASSERT_NOT_NULL(r->save.blob);
    TEST_ASSERT_EQUAL_UINT32(IDA_PRM_BLOB_BOYU, r->save.len);
    ida_prm_kume_t k;
    TEST_ASSERT_EQUAL_UINT8(0u, ida_prm_blok_coz(r->save.blob, r->save.len, &k)); /* A4-K100 */
    TEST_ASSERT_EQUAL_UINT16(IDA_REASON_ONE_PER_CALL, sonuc_al(6u)->reason);
    /* Kayıt sürerken (BUSY) PARAM_SAVE: PARAM_BUSY (A4-K82) */
    IN.n_cmds = 0u;
    IN.plat.param_store = (uint8_t)IDA_PSTORE_BUSY;
    komut(0u, 7u, (uint8_t)IDA_CMD_PARAM_SAVE);
    TEST_ASSERT_EQUAL(IDA_RC_OK, cagir());
    TEST_ASSERT_EQUAL_UINT16(IDA_REASON_PARAM_BUSY, sonuc_al(7u)->reason);
    /* Kayıt OK ve gen eşleşti: engel kalkar (A4-K78), PARAM_SAVE günlüğü */
    IN.n_cmds = 0u;
    IN.plat.param_store = (uint8_t)IDA_PSTORE_OK;
    IN.plat.param_gen = 42u;
    komut(0u, 8u, (uint8_t)IDA_CMD_ARM);
    TEST_ASSERT_EQUAL(IDA_RC_OK, cagir());
    TEST_ASSERT_TRUE(OUT.status.param_ok);
    TEST_ASSERT_EQUAL_UINT32(1u, ds_gunluk_say(&OUT, (uint16_t)IDA_LOG_PARAM_SAVE));
    TEST_ASSERT_EQUAL_UINT16(IDA_REASON_NOT_ARMABLE, sonuc_al(8u)->reason);
    /* BUSY'de ARM PARAM_BUSY (A4-K83) */
    IN.n_cmds = 0u;
    IN.plat.param_store = (uint8_t)IDA_PSTORE_BUSY;
    komut(0u, 9u, (uint8_t)IDA_CMD_ARM);
    TEST_ASSERT_EQUAL(IDA_RC_OK, cagir());
    TEST_ASSERT_EQUAL_UINT16(IDA_REASON_PARAM_BUSY, sonuc_al(9u)->reason);
}


/* ---- A4-K81, K82: commit ve save çağrı sonunda, girdideki sıradan bağımsız --------------- */

static void get_komutu(uint32_t i, uint32_t id, uint16_t pid)
{
    komut(i, id, (uint8_t)IDA_CMD_PARAM_GET);
    IN.cmds[i].u.param.id = pid;
}

static uint32_t blok_degeri(const uint8_t *blok, uint32_t indis)
{
    const uint8_t *b = &blok[24u + (8u * indis) + 4u];
    return (uint32_t)b[0] | ((uint32_t)b[1] << 8) | ((uint32_t)b[2] << 16) | ((uint32_t)b[3] << 24);
}

static void test_k81_commit_sirasindan_bagimsiz(void)
{
    /* (a) SAVE, COMMIT, GET, sonra SET, GET (aynı çağrı): commit sonraki SET'i de kapsar;
       GET'ler commit öncesi değeri okur; SAVE commit sonrası kümeyi kaydeder. */
    hazirla(true);
    komut(0u, 1u, (uint8_t)IDA_CMD_PARAM_SAVE);
    komut(1u, 2u, (uint8_t)IDA_CMD_PARAM_COMMIT);
    get_komutu(2u, 3u, 0x0101u);
    param_set(3u, 4u, 0x0101u, 25u);
    get_komutu(4u, 5u, 0x0101u);
    IN.plat.param_gen = 3u;
    TEST_ASSERT_EQUAL(IDA_RC_OK, cagir());
    ds_cikti_degismezleri(IDA_RC_OK, &IN, &OUT);
    TEST_ASSERT_EQUAL_UINT8(IDA_ACK_ACCEPTED, sonuc_al(2u)->ack);
    TEST_ASSERT_EQUAL_UINT32(20u, sonuc_al(3u)->value.v.u32);
    TEST_ASSERT_EQUAL_UINT32(20u, sonuc_al(5u)->value.v.u32); /* commit öncesi uygulanmış */
    TEST_ASSERT_EQUAL_UINT32(25u, ida_prm_u32(&C.prm, IDA_PRM_imu_stale_ms));
    const ida_cmd_result_t *r = sonuc_al(1u);
    TEST_ASSERT_EQUAL_UINT8(IDA_ACK_ACCEPTED, r->ack);
    TEST_ASSERT_EQUAL_UINT32(4u, r->save.gen);
    TEST_ASSERT_EQUAL_UINT32(25u, blok_degeri(r->save.blob, IDA_PRM_imu_stale_ms));
    TEST_ASSERT_EQUAL_UINT32(1u, ds_gunluk_say(&OUT, (uint16_t)IDA_LOG_PARAM_COMMIT));
    /* (b) Aynı içerik ters sırayla (SET, COMMIT, SAVE) aynı sonucu verir. */
    hazirla(true);
    param_set(0u, 4u, 0x0101u, 25u);
    komut(1u, 2u, (uint8_t)IDA_CMD_PARAM_COMMIT);
    komut(2u, 1u, (uint8_t)IDA_CMD_PARAM_SAVE);
    IN.plat.param_gen = 3u;
    TEST_ASSERT_EQUAL(IDA_RC_OK, cagir());
    TEST_ASSERT_EQUAL_UINT8(IDA_ACK_ACCEPTED, sonuc_al(2u)->ack);
    TEST_ASSERT_EQUAL_UINT32(25u, ida_prm_u32(&C.prm, IDA_PRM_imu_stale_ms));
    TEST_ASSERT_EQUAL_UINT32(4u, sonuc_al(1u)->save.gen);
    TEST_ASSERT_EQUAL_UINT32(25u, blok_degeri(sonuc_al(1u)->save.blob, IDA_PRM_imu_stale_ms));
}

static void test_k81_birden_cok_commit(void)
{
    /* Birinci commit'ten sonra gelen SET grubu bozar: iki commit de PARAM_INVALID alır, iki
       PARAM_COMMIT günlüğü üretilir, bekleyenler atılır, cmd_rejected iki artar. */
    hazirla(true);
    komut(0u, 1u, (uint8_t)IDA_CMD_PARAM_COMMIT);
    param_set(1u, 2u, 0x0104u, 40u); /* T_lag_clear 40 ms > T_lag_set 20 ms */
    param_set(2u, 3u, 0x0101u, 25u);
    komut(3u, 4u, (uint8_t)IDA_CMD_PARAM_COMMIT);
    TEST_ASSERT_EQUAL(IDA_RC_OK, cagir());
    ds_cikti_degismezleri(IDA_RC_OK, &IN, &OUT);
    TEST_ASSERT_EQUAL_UINT16(IDA_REASON_PARAM_INVALID, sonuc_al(1u)->reason);
    TEST_ASSERT_EQUAL_UINT16(IDA_REASON_PARAM_INVALID, sonuc_al(4u)->reason);
    TEST_ASSERT_EQUAL_UINT8(IDA_ACK_REJECTED, sonuc_al(1u)->ack);
    TEST_ASSERT_EQUAL_UINT32(2u, ds_gunluk_say(&OUT, (uint16_t)IDA_LOG_PARAM_COMMIT));
    TEST_ASSERT_EQUAL_UINT32(2u, OUT.status.cnt.cmd_rejected);
    TEST_ASSERT_EQUAL_UINT32(20u, ida_prm_u32(&C.prm, IDA_PRM_imu_stale_ms));
    TEST_ASSERT_EQUAL_UINT32(10u, ida_prm_u32(&C.prm, IDA_PRM_lag_clear_ms));
    /* Bekleyenler atıldı: sonraki iki boş commit etkisiz kabul. */
    IN.n_cmds = 0u;
    komut(0u, 5u, (uint8_t)IDA_CMD_PARAM_COMMIT);
    komut(1u, 6u, (uint8_t)IDA_CMD_PARAM_COMMIT);
    TEST_ASSERT_EQUAL(IDA_RC_OK, cagir());
    TEST_ASSERT_EQUAL_UINT8(IDA_ACK_ACCEPTED, sonuc_al(5u)->ack);
    TEST_ASSERT_EQUAL_UINT8(IDA_ACK_ACCEPTED, sonuc_al(6u)->ack);
    TEST_ASSERT_EQUAL_UINT32(2u, ds_gunluk_say(&OUT, (uint16_t)IDA_LOG_PARAM_COMMIT));
    TEST_ASSERT_EQUAL_UINT32(20u, ida_prm_u32(&C.prm, IDA_PRM_imu_stale_ms));
}

static void test_k78_bos_commit_ve_save_ayni_cagri(void)
{
    /* Bloksuz init (param_ok = 0). SAVE girdide commit'ten önce: aynı çağrının ACCEPTED boş
       commit'i SAVE için uygulanmış commit sayılır; kayıt OK olunca arm engeli kalkar. */
    hazirla(false);
    komut(0u, 1u, (uint8_t)IDA_CMD_PARAM_SAVE);
    komut(1u, 2u, (uint8_t)IDA_CMD_PARAM_COMMIT);
    IN.plat.param_gen = 0u;
    TEST_ASSERT_EQUAL(IDA_RC_OK, cagir());
    TEST_ASSERT_EQUAL_UINT8(IDA_ACK_ACCEPTED, sonuc_al(2u)->ack);
    TEST_ASSERT_EQUAL_UINT8(IDA_ACK_ACCEPTED, sonuc_al(1u)->ack);
    TEST_ASSERT_EQUAL_UINT32(1u, sonuc_al(1u)->save.gen);
    TEST_ASSERT_FALSE(OUT.status.param_ok);
    IN.n_cmds = 0u;
    IN.plat.param_store = (uint8_t)IDA_PSTORE_BUSY;
    TEST_ASSERT_EQUAL(IDA_RC_OK, cagir());
    IN.plat.param_store = (uint8_t)IDA_PSTORE_OK;
    IN.plat.param_gen = 1u;
    komut(0u, 3u, (uint8_t)IDA_CMD_ARM);
    TEST_ASSERT_EQUAL(IDA_RC_OK, cagir());
    TEST_ASSERT_TRUE(OUT.status.param_ok);
    TEST_ASSERT_EQUAL_UINT16(IDA_REASON_NOT_ARMABLE, sonuc_al(3u)->reason); /* PARAM_INVALID değil */
}

static void test_k82_gen_kurali_ve_doyma(void)
{
    hazirla(true);
    /* gen = max(son gen, param_gen) + 1 */
    komut(0u, 1u, (uint8_t)IDA_CMD_PARAM_SAVE);
    IN.plat.param_gen = 41u;
    TEST_ASSERT_EQUAL(IDA_RC_OK, cagir());
    TEST_ASSERT_EQUAL_UINT32(42u, sonuc_al(1u)->save.gen);
    IN.cmds[0].cmd_id = 2u;
    IN.plat.param_gen = 10u; /* platformunki daha küçük: çekirdeğin son gen'i taban */
    TEST_ASSERT_EQUAL(IDA_RC_OK, cagir());
    TEST_ASSERT_EQUAL_UINT32(43u, sonuc_al(2u)->save.gen);
    /* max(...) = 0xFFFFFFFE: gen 0xFFFFFFFF kabul */
    IN.cmds[0].cmd_id = 3u;
    IN.plat.param_gen = 0xFFFFFFFEu;
    TEST_ASSERT_EQUAL(IDA_RC_OK, cagir());
    TEST_ASSERT_EQUAL_UINT8(IDA_ACK_ACCEPTED, sonuc_al(3u)->ack);
    TEST_ASSERT_EQUAL_UINT32(0xFFFFFFFFu, sonuc_al(3u)->save.gen);
    /* max(...) = 0xFFFFFFFF (çekirdeğin son gen'i): OUT_OF_RANGE, blok yok, sarmaz */
    const uint32_t onceki_ret = OUT.status.cnt.cmd_rejected;
    IN.cmds[0].cmd_id = 4u;
    IN.plat.param_gen = 0u;
    TEST_ASSERT_EQUAL(IDA_RC_OK, cagir());
    ds_cikti_degismezleri(IDA_RC_OK, &IN, &OUT);
    TEST_ASSERT_EQUAL_UINT8(IDA_ACK_REJECTED, sonuc_al(4u)->ack);
    TEST_ASSERT_EQUAL_UINT16(IDA_REASON_OUT_OF_RANGE, sonuc_al(4u)->reason);
    TEST_ASSERT_TRUE(sonuc_al(4u)->save.blob == NULL);
    TEST_ASSERT_EQUAL_UINT32(0u, sonuc_al(4u)->save.gen);
    TEST_ASSERT_EQUAL_UINT32(onceki_ret + 1u, OUT.status.cnt.cmd_rejected);
    /* plat.param_gen = 0xFFFFFFFF de aynı */
    hazirla(true);
    komut(0u, 5u, (uint8_t)IDA_CMD_PARAM_SAVE);
    IN.plat.param_gen = 0xFFFFFFFFu;
    TEST_ASSERT_EQUAL(IDA_RC_OK, cagir());
    TEST_ASSERT_EQUAL_UINT16(IDA_REASON_OUT_OF_RANGE, sonuc_al(5u)->reason);
}

/* sol W2-cekirdek bulgu 2: kaydın tamamlanması durum kodu değişmeden (aradaki BUSY
   görülmeden) beklenen gen ile eşleştirilir; günlük bir kez. */
static void test_k83_ayni_durum_koduyla_tamamlanma(void)
{
    hazirla(false); /* bloksuz: param_ok = 0 */
    IN.plat.param_store = (uint8_t)IDA_PSTORE_OK;
    IN.plat.param_gen = 41u;
    TEST_ASSERT_EQUAL(IDA_RC_OK, cagir());
    komut(0u, 1u, (uint8_t)IDA_CMD_PARAM_COMMIT);
    komut(1u, 2u, (uint8_t)IDA_CMD_PARAM_SAVE);
    TEST_ASSERT_EQUAL(IDA_RC_OK, cagir());
    TEST_ASSERT_EQUAL_UINT32(42u, sonuc_al(2u)->save.gen);
    TEST_ASSERT_FALSE(OUT.status.param_ok);
    /* platform kaydı iki çağrı arasında bitirdi: OK/42 (BUSY hiç görülmedi) */
    IN.n_cmds = 0u;
    IN.plat.param_gen = 42u;
    TEST_ASSERT_EQUAL(IDA_RC_OK, cagir());
    TEST_ASSERT_TRUE(OUT.status.param_ok);
    TEST_ASSERT_EQUAL_UINT32(1u, ds_gunluk_say(&OUT, (uint16_t)IDA_LOG_PARAM_SAVE));
    TEST_ASSERT_EQUAL_INT32(42, OUT.logs[0].arg0);
    TEST_ASSERT_EQUAL(IDA_RC_OK, cagir());
    TEST_ASSERT_EQUAL_UINT32(0u, ds_gunluk_say(&OUT, (uint16_t)IDA_LOG_PARAM_SAVE)); /* yineleme yok */
    /* FAIL de kuşak taşır: eski kuşağın FAIL'i beklemeyi bitirmez (yalnız geçiş günlüğü). */
    komut(0u, 3u, (uint8_t)IDA_CMD_PARAM_SAVE);
    TEST_ASSERT_EQUAL(IDA_RC_OK, cagir());
    TEST_ASSERT_EQUAL_UINT32(43u, sonuc_al(3u)->save.gen);
    IN.n_cmds = 0u;
    IN.plat.param_store = (uint8_t)IDA_PSTORE_FAIL; /* param_gen hâlâ 42 */
    TEST_ASSERT_EQUAL(IDA_RC_OK, cagir());
    TEST_ASSERT_EQUAL_UINT32(1u, ds_gunluk_say(&OUT, (uint16_t)IDA_LOG_PARAM_SAVE));
    TEST_ASSERT_TRUE(C.prm.kayit_bekliyor);
    /* FAIL/43: durum kodu değişmeden başarısız tamamlanma saptanır, günlük bir kez. */
    IN.plat.param_gen = 43u;
    TEST_ASSERT_EQUAL(IDA_RC_OK, cagir());
    TEST_ASSERT_EQUAL_UINT32(1u, ds_gunluk_say(&OUT, (uint16_t)IDA_LOG_PARAM_SAVE));
    TEST_ASSERT_EQUAL_INT32(43, OUT.logs[0].arg0);
    TEST_ASSERT_EQUAL_INT32((int32_t)IDA_PSTORE_FAIL, OUT.logs[0].arg1);
    TEST_ASSERT_FALSE(C.prm.kayit_bekliyor);
    TEST_ASSERT_EQUAL(IDA_RC_OK, cagir());
    TEST_ASSERT_EQUAL_UINT32(0u, ds_gunluk_say(&OUT, (uint16_t)IDA_LOG_PARAM_SAVE));
    /* Art arda ikinci FAIL: FAIL/43 → SAVE (44) → FAIL/43 sürerken bekleme bitmez → FAIL/44. */
    komut(0u, 4u, (uint8_t)IDA_CMD_PARAM_SAVE);
    TEST_ASSERT_EQUAL(IDA_RC_OK, cagir());
    TEST_ASSERT_EQUAL_UINT32(44u, sonuc_al(4u)->save.gen); /* FAIL'li 43 yeniden kullanılmaz */
    IN.n_cmds = 0u;
    TEST_ASSERT_EQUAL(IDA_RC_OK, cagir());
    TEST_ASSERT_EQUAL_UINT32(0u, ds_gunluk_say(&OUT, (uint16_t)IDA_LOG_PARAM_SAVE));
    TEST_ASSERT_TRUE(C.prm.kayit_bekliyor);
    IN.plat.param_gen = 44u;
    TEST_ASSERT_EQUAL(IDA_RC_OK, cagir());
    TEST_ASSERT_EQUAL_UINT32(1u, ds_gunluk_say(&OUT, (uint16_t)IDA_LOG_PARAM_SAVE));
    TEST_ASSERT_FALSE(C.prm.kayit_bekliyor);
    TEST_ASSERT_TRUE(OUT.status.param_ok); /* önceki OK/42 kaydı geçerli kalır */
    komut(0u, 5u, (uint8_t)IDA_CMD_PARAM_SAVE);
    IN.plat.param_gen = 10u;
    TEST_ASSERT_EQUAL(IDA_RC_OK, cagir());
    TEST_ASSERT_EQUAL_UINT32(45u, sonuc_al(5u)->save.gen); /* taban yükseldi */
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_sha256_ve_crc);
    RUN_TEST(test_sema_ozeti_kanonik_metin);
    RUN_TEST(test_tablo_ve_port_islevleri);
    RUN_TEST(test_blok_denetimleri);
    RUN_TEST(test_set_get_commit);
    RUN_TEST(test_b_sinifi_ve_arm);
    RUN_TEST(test_save_ve_k78);
    RUN_TEST(test_39_parametre_tablosu);
    RUN_TEST(test_k81_commit_sirasindan_bagimsiz);
    RUN_TEST(test_k81_birden_cok_commit);
    RUN_TEST(test_k78_bos_commit_ve_save_ayni_cagri);
    RUN_TEST(test_k82_gen_kurali_ve_doyma);
    RUN_TEST(test_k83_ayni_durum_koduyla_tamamlanma);
    return UNITY_END();
}
