/*
 * test_param.c — parametre tablosu, şema özeti, kodlanmış blok ve parametre komutları
 * (A4-K39 aralığı, K78–K85, K100, K112; 3.10; 4.15 parametre satırları).
 */
#include <string.h>

#include "destek.h"
#include "param/ozet.h"
#include "unity.h"

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
    uint8_t r[10];
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
    TEST_ASSERT_EQUAL(IDA_RC_BAD_INPUT, ida_core_param_find("IMU_STALE_NS", NULL));
    TEST_ASSERT_EQUAL(IDA_RC_NOT_FOUND, ida_core_param_find("IMU_STALE", &id));
    TEST_ASSERT_EQUAL(IDA_RC_NOT_FOUND, ida_core_param_find("IMU_STALE_NS_ABCD", &id)); /* 17 */
    TEST_ASSERT_EQUAL(IDA_RC_NOT_FOUND, ida_core_param_find("", &id));
    TEST_ASSERT_EQUAL_UINT16(777u, id);
    TEST_ASSERT_EQUAL(IDA_RC_OK, ida_core_param_find("IMU_LAG_SET_NS", &id)); /* 14 karakter */
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
    TEST_ASSERT_EQUAL_UINT32(IDA_IMU_STALE_DEFAULT_NS, v.v.u32);
    TEST_ASSERT_EQUAL(IDA_RC_OK, ida_core_param_get(&C, 0x0106u, &v));
    TEST_ASSERT_EQUAL_FLOAT(0.1f, v.v.f32);
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
    BLOK[0] ^= 0xFFu;
    init_beklenen(n, IDA_RC_PARAM_DEFAULTS, 1);
    n = ds_gecerli_blok(BLOK);
    init_beklenen(10u, IDA_RC_PARAM_DEFAULTS, 1); /* başlıktan kısa */
    /* (2) şema özeti */
    n = ds_gecerli_blok(BLOK);
    BLOK[8] ^= 0x01u;
    init_beklenen(n, IDA_RC_PARAM_DEFAULTS, 2);
    /* (3) boy */
    n = ds_gecerli_blok(BLOK);
    init_beklenen(n - 8u, IDA_RC_PARAM_DEFAULTS, 3);
    /* (4) CRC */
    n = ds_gecerli_blok(BLOK);
    BLOK[30] ^= 0x01u;
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
    /* (7) aralık: T_imu_stale 4 ms ve 31 ms (§7 A4-K39) */
    n = ds_gecerli_blok(BLOK);
    kayit_deger(IDA_PRM_imu_stale_ns, 4000000u);
    crc_duzelt(BLOK, n);
    init_beklenen(n, IDA_RC_PARAM_DEFAULTS, 7);
    n = ds_gecerli_blok(BLOK);
    kayit_deger(IDA_PRM_imu_stale_ns, 31000000u);
    crc_duzelt(BLOK, n);
    init_beklenen(n, IDA_RC_PARAM_DEFAULTS, 7);
    n = ds_gecerli_blok(BLOK);
    kayit_deger(IDA_PRM_imu_stale_ns, 30000000u); /* sınır içi */
    crc_duzelt(BLOK, n);
    init_beklenen(n, IDA_RC_OK, 0);
    /* (8) grup: IMU_LAG_CLR_NS > IMU_LAG_SET_NS */
    n = ds_gecerli_blok(BLOK);
    kayit_deger(IDA_PRM_imu_lag_clr_ns, 30000000u);
    crc_duzelt(BLOK, n);
    init_beklenen(n, IDA_RC_PARAM_DEFAULTS, 8);
    /* param_len > 8000: denetim 3 */
    n = ds_gecerli_blok(BLOK);
    init_beklenen(IDA_PARAM_BLOB_MAX + 1u, IDA_RC_PARAM_DEFAULTS, 3);
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

static const ida_cmd_result_t *s(uint32_t id)
{
    const ida_cmd_result_t *r = ds_sonuc(&OUT, id);
    TEST_ASSERT_NOT_NULL(r);
    return r;
}

static void test_set_get_commit(void)
{
    hazirla(true);
    param_set(0u, 1u, 0x0999u, 5u);                      /* bilinmeyen */
    param_set(1u, 2u, 0x0101u, 4000000u);                /* aralık dışı */
    param_set(2u, 3u, 0x0101u, 25000000u);               /* geçerli */
    komut(3u, 4u, (uint8_t)IDA_CMD_PARAM_SET);           /* tür uyuşmuyor */
    IN.cmds[3].u.param.id = 0x0106u;
    IN.cmds[3].u.param.type = (uint8_t)IDA_PT_U32;
    komut(4u, 5u, (uint8_t)IDA_CMD_PARAM_GET);
    IN.cmds[4].u.param.id = 0x0101u;
    IN.cmds[4].u.param.type = 99u; /* PARAM_GET'te okunmaz (3.14) */
    TEST_ASSERT_EQUAL(IDA_RC_OK, cagir());
    ds_cikti_degismezleri(IDA_RC_OK, &IN, &OUT);
    TEST_ASSERT_EQUAL_UINT16(IDA_REASON_PARAM_INVALID, s(1u)->reason);
    TEST_ASSERT_EQUAL_UINT16(IDA_REASON_PARAM_INVALID, s(2u)->reason);
    TEST_ASSERT_EQUAL_UINT8(IDA_ACK_ACCEPTED, s(3u)->ack);
    TEST_ASSERT_EQUAL_UINT8(IDA_RES_PARAM_VALUE, s(3u)->kind);
    TEST_ASSERT_EQUAL_UINT32(25000000u, s(3u)->value.v.u32);
    TEST_ASSERT_EQUAL_UINT16(IDA_REASON_PARAM_INVALID, s(4u)->reason);
    TEST_ASSERT_EQUAL_UINT8(IDA_ACK_ACCEPTED, s(5u)->ack);
    TEST_ASSERT_EQUAL_UINT32(IDA_IMU_STALE_DEFAULT_NS, s(5u)->value.v.u32); /* bekletilir */
    /* COMMIT: uygulanır (adım sınırında); sonraki GET yeni değeri verir. */
    IN.n_cmds = 0u;
    komut(0u, 6u, (uint8_t)IDA_CMD_PARAM_COMMIT);
    TEST_ASSERT_EQUAL(IDA_RC_OK, cagir());
    TEST_ASSERT_EQUAL_UINT8(IDA_ACK_ACCEPTED, s(6u)->ack);
    TEST_ASSERT_EQUAL_UINT32(1u, ds_gunluk_say(&OUT, (uint16_t)IDA_LOG_PARAM_COMMIT));
    IN.n_cmds = 0u;
    komut(0u, 7u, (uint8_t)IDA_CMD_PARAM_GET);
    IN.cmds[0].u.param.id = 0x0101u;
    TEST_ASSERT_EQUAL(IDA_RC_OK, cagir());
    TEST_ASSERT_EQUAL_UINT32(25000000u, s(7u)->value.v.u32);
    /* Grup denetimi geçmeyen commit hepsini reddeder. */
    IN.n_cmds = 0u;
    param_set(0u, 8u, 0x0104u, 40000000u); /* CLR 40 ms > SET 20 ms */
    komut(1u, 9u, (uint8_t)IDA_CMD_PARAM_COMMIT);
    TEST_ASSERT_EQUAL(IDA_RC_OK, cagir());
    TEST_ASSERT_EQUAL_UINT16(IDA_REASON_PARAM_INVALID, s(9u)->reason);
    ida_param_value_t v;
    TEST_ASSERT_EQUAL(IDA_RC_OK, ida_core_param_get(&C, 0x0104u, &v));
    TEST_ASSERT_EQUAL_UINT32(10000000u, v.v.u32);
}

static void test_b_sinifi_ve_arm(void)
{
    hazirla(true);
    C.emn.armed = true; /* test kancası: silahlı */
    param_set(0u, 1u, 0x0101u, 25000000u);
    TEST_ASSERT_EQUAL(IDA_RC_OK, cagir());
    TEST_ASSERT_EQUAL_UINT16(IDA_REASON_ARMED, s(1u)->reason);
    /* silahsızken bekletilir; ARM ile aynı çağrıdaki commit reddedilir ve bekleyenler atılır */
    C.emn.armed = false;
    IN.n_cmds = 0u;
    param_set(0u, 2u, 0x0101u, 25000000u);
    TEST_ASSERT_EQUAL(IDA_RC_OK, cagir());
    IN.n_cmds = 0u;
    komut(0u, 3u, (uint8_t)IDA_CMD_PARAM_COMMIT);
    komut(1u, 4u, (uint8_t)IDA_CMD_ARM);
    TEST_ASSERT_EQUAL(IDA_RC_OK, cagir());
    TEST_ASSERT_EQUAL_UINT16(IDA_REASON_ARMED, s(3u)->reason);
    /* ARM önce, COMMIT sonra: aynı sonuç (sırasından bağımsız) */
    IN.n_cmds = 0u;
    param_set(0u, 5u, 0x0101u, 25000000u);
    TEST_ASSERT_EQUAL(IDA_RC_OK, cagir());
    IN.n_cmds = 0u;
    komut(0u, 6u, (uint8_t)IDA_CMD_ARM);
    komut(1u, 7u, (uint8_t)IDA_CMD_PARAM_COMMIT);
    TEST_ASSERT_EQUAL(IDA_RC_OK, cagir());
    TEST_ASSERT_EQUAL_UINT16(IDA_REASON_ARMED, s(7u)->reason);
    /* bekleyenler atıldı: boş commit etkisiz kabul, değer değişmedi */
    IN.n_cmds = 0u;
    komut(0u, 8u, (uint8_t)IDA_CMD_PARAM_COMMIT);
    TEST_ASSERT_EQUAL(IDA_RC_OK, cagir());
    TEST_ASSERT_EQUAL_UINT8(IDA_ACK_ACCEPTED, s(8u)->ack);
    TEST_ASSERT_EQUAL_UINT32(IDA_IMU_STALE_DEFAULT_NS, ida_prm_u32(&C.prm, IDA_PRM_imu_stale_ns));
}

static void test_save_ve_k78(void)
{
    hazirla(false); /* varsayılanlar: param_ok = 0 */
    komut(0u, 1u, (uint8_t)IDA_CMD_PARAM_SAVE); /* commit yok */
    komut(1u, 2u, (uint8_t)IDA_CMD_ARM);
    TEST_ASSERT_EQUAL(IDA_RC_OK, cagir());
    TEST_ASSERT_EQUAL_UINT16(IDA_REASON_ARMED, s(1u)->reason);      /* ARM ile aynı çağrı */
    TEST_ASSERT_EQUAL_UINT16(IDA_REASON_PARAM_INVALID, s(2u)->reason); /* A4-K78 */
    IN.n_cmds = 0u;
    komut(0u, 3u, (uint8_t)IDA_CMD_PARAM_SAVE);
    TEST_ASSERT_EQUAL(IDA_RC_OK, cagir());
    TEST_ASSERT_EQUAL_UINT16(IDA_REASON_PARAM_INVALID, s(3u)->reason);
    TEST_ASSERT_EQUAL_UINT8(IDA_RES_PARAM_SAVE, s(3u)->kind);
    /* commit (boş, etkisiz) + iki SAVE: ilki kabul, ikincisi ONE_PER_CALL */
    IN.n_cmds = 0u;
    komut(0u, 4u, (uint8_t)IDA_CMD_PARAM_COMMIT);
    komut(1u, 5u, (uint8_t)IDA_CMD_PARAM_SAVE);
    komut(2u, 6u, (uint8_t)IDA_CMD_PARAM_SAVE);
    IN.plat.param_gen = 41u;
    TEST_ASSERT_EQUAL(IDA_RC_OK, cagir());
    ds_cikti_degismezleri(IDA_RC_OK, &IN, &OUT);
    const ida_cmd_result_t *r = s(5u);
    TEST_ASSERT_EQUAL_UINT8(IDA_ACK_ACCEPTED, r->ack);
    TEST_ASSERT_EQUAL_UINT32(42u, r->save.gen);
    TEST_ASSERT_NOT_NULL(r->save.blob);
    TEST_ASSERT_EQUAL_UINT32(IDA_PRM_BLOB_BOYU, r->save.len);
    ida_prm_kume_t k;
    TEST_ASSERT_EQUAL_UINT8(0u, ida_prm_blok_coz(r->save.blob, r->save.len, &k)); /* A4-K100 */
    TEST_ASSERT_EQUAL_UINT16(IDA_REASON_ONE_PER_CALL, s(6u)->reason);
    /* Kayıt sürerken (BUSY) PARAM_SAVE: PARAM_BUSY (A4-K82) */
    IN.n_cmds = 0u;
    IN.plat.param_store = (uint8_t)IDA_PSTORE_BUSY;
    komut(0u, 7u, (uint8_t)IDA_CMD_PARAM_SAVE);
    TEST_ASSERT_EQUAL(IDA_RC_OK, cagir());
    TEST_ASSERT_EQUAL_UINT16(IDA_REASON_PARAM_BUSY, s(7u)->reason);
    /* Kayıt OK ve gen eşleşti: engel kalkar (A4-K78), PARAM_SAVE günlüğü */
    IN.n_cmds = 0u;
    IN.plat.param_store = (uint8_t)IDA_PSTORE_OK;
    IN.plat.param_gen = 42u;
    komut(0u, 8u, (uint8_t)IDA_CMD_ARM);
    TEST_ASSERT_EQUAL(IDA_RC_OK, cagir());
    TEST_ASSERT_TRUE(OUT.status.param_ok);
    TEST_ASSERT_EQUAL_UINT32(1u, ds_gunluk_say(&OUT, (uint16_t)IDA_LOG_PARAM_SAVE));
    TEST_ASSERT_EQUAL_UINT16(IDA_REASON_NOT_ARMABLE, s(8u)->reason);
    /* BUSY'de ARM PARAM_BUSY (A4-K83) */
    IN.n_cmds = 0u;
    IN.plat.param_store = (uint8_t)IDA_PSTORE_BUSY;
    komut(0u, 9u, (uint8_t)IDA_CMD_ARM);
    TEST_ASSERT_EQUAL(IDA_RC_OK, cagir());
    TEST_ASSERT_EQUAL_UINT16(IDA_REASON_PARAM_BUSY, s(9u)->reason);
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
    return UNITY_END();
}
