/*
 * A4 §3.9, §3.10, §4.9 — parametre işlevleri, kodlanmış blok ve doğrulaması:
 * A4-K78 (varsayılanlara düşüş, PARAM_DEFAULTS, arm engeli), A4-K79, A4-K80 (her denetim
 * numarası; PARAM_LOAD arg1), A4-K85 (count/desc/find/get tutarlılığı).
 *
 * Bloklar bu dosyada, şemanın kendi tanımlayıcılarından (ida_core_param_desc) ve
 * ida_core_info'nun param_schema_hash'inden kurulur (A4 §3.10: SIL koşucusu özeti
 * kendisi hesaplamaz). CRC-32/ISO-HDLC düzenekte bağımsız yazıldı ve "123456789"
 * denetim değeriyle sınanır.
 */
#include <stdio.h>
#include <string.h>

#include "kabul_cekirdek.h"
#include "unity.h"

static ida_in_t g_in;
static kc_pkayit_t g_k[KC_PARAM_AZAMI];
static uint8_t g_blok[IDA_PARAM_BLOB_MAX + 64u];

void setUp(void)
{
    kc_ortam_kur(KC_N_AZAMI);
}

void tearDown(void)
{
}

static uint32_t f32_ham(float f)
{
    uint32_t u;
    memcpy(&u, &f, sizeof u);
    return u;
}

/* A4 §3.9 tablosu: sözleşmenin adlandırdığı altı parametrenin tipi, birimi, varsayılanı ve
   aralığı. Süreler ms'dir (U32); dönüşüm ns = ms × 10^6 (3.9). */
typedef struct {
    const char *ad;
    uint8_t type;
    uint32_t def;
    uint32_t min;
    uint32_t max;
    bool sure_ms; /* ms cinsinden süre parametresi */
} sozlesme_param_t;

static sozlesme_param_t sp_tanim(int k)
{
    sozlesme_param_t t;
    t.sure_ms = true;
    t.type = (uint8_t)IDA_PT_U32;
    switch (k) {
    case 0:
        t.ad = "T_lag_set";
        t.def = 20u;
        t.min = 10u;
        t.max = 50u;
        break;
    case 1:
        t.ad = "T_lag_clear";
        t.def = 10u;
        t.min = 1u;
        t.max = 50u;
        break;
    case 2:
        t.ad = "N_lag_clear";
        t.def = 20u;
        t.min = 1u;
        t.max = 200u;
        t.sure_ms = false;
        break;
    case 3:
        t.ad = "T_imu_gap_tol";
        t.type = (uint8_t)IDA_PT_F32;
        t.def = f32_ham(0.1f);
        t.min = f32_ham(0.01f);
        t.max = f32_ham(1.0f);
        t.sure_ms = false;
        break;
    case 4:
        t.ad = "T_imu_stale";
        t.def = 20u;
        t.min = 5u;
        t.max = 30u;
        break;
    default:
        t.ad = "T_det_stale";
        t.def = 2000u;
        t.min = 100u;
        t.max = 10000u;
        break;
    }
    return t;
}

/* Sözleşmenin imzasıyla (A4 §3.9) şemadaki yeri; yoksa -1. */
static int sp_kesin(int k)
{
    sozlesme_param_t t = sp_tanim(k);
    return kc_param_imza(t.type, t.def, t.min, t.max);
}

/* Blok denetimi testleri için parametrenin rolüyle yeri: önce sözleşmenin imzası; yoksa
   aynı süre ns cinsinden (× 10^6). Birim uyuşmazlığı ayrıca test_A4_3_9'da kırmızıdır;
   blok denetimlerinin sınaması ondan bağımsız kalsın diye. */
static int sp_indis(int k)
{
    sozlesme_param_t t = sp_tanim(k);
    int i = kc_param_imza(t.type, t.def, t.min, t.max);
    if (i < 0 && t.sure_ms) {
        i = kc_param_imza(t.type, t.def * 1000000u, t.min * 1000000u, t.max * 1000000u);
    }
    TEST_ASSERT_TRUE_MESSAGE(i >= 0, t.ad);
    return i;
}

enum { P_LAG_SET = 0, P_LAG_CLEAR = 1, P_N_LAG = 2, P_GAP_TOL = 3, P_IMU_STALE = 4, P_DET_STALE = 5 };

static uint32_t p_min(int k)
{
    return kc_param_ham(&kc_param_desc((uint16_t)sp_indis(k))->min);
}

static uint32_t p_max(int k)
{
    return kc_param_ham(&kc_param_desc((uint16_t)sp_indis(k))->max);
}

static uint32_t p_def(int k)
{
    return kc_param_ham(&kc_param_desc((uint16_t)sp_indis(k))->def);
}

/* Varsayılandan farklı aralık içi değer: (min + max) / 2 ya da min. */
static uint32_t p_ozel(int k)
{
    uint32_t v = p_min(k) / 2u + p_max(k) / 2u;
    return v == p_def(k) ? p_min(k) : v;
}

/* g_k içinde şema indisi i olan parametrenin kaydı. */
static kc_pkayit_t *kayit(size_t n, int sema_i)
{
    uint16_t id = kc_param_desc((uint16_t)sema_i)->id;
    kc_pkayit_t *bulunan = NULL;
    for (size_t j = 0; j < n; ++j) {
        if (g_k[j].id == id) {
            bulunan = &g_k[j];
        }
    }
    TEST_ASSERT_NOT_NULL_MESSAGE(bulunan, "kayıt bulunamadı");
    return bulunan;
}

static void param_bekle(int sema_i, uint32_t ham)
{
    ida_param_value_t v;
    const ida_param_desc_t *d = kc_param_desc((uint16_t)sema_i);
    TEST_ASSERT_EQUAL_INT(IDA_RC_OK, ida_core_param_get(kc_core(0), d->id, &v));
    TEST_ASSERT_EQUAL_HEX32_MESSAGE(ham, kc_param_ham(&v), d->name);
}

/* Bloğu verir ve ilk yazılabilir çağrının PARAM_LOAD kaydını düzeneğe bekletir. */
static ida_rc_t blokla_init(const uint8_t *blob, uint32_t len, int32_t beklenen_arg1)
{
    ida_init_in_t ii;
    kc_init_girdi(&ii);
    ii.param_src = (uint8_t)IDA_PSRC_FLASH;
    ii.param_blob = blob;
    ii.param_len = len;
    ida_rc_t rc = kc_init(&ii);
    kc_param_load_beklenen(beklenen_arg1);
    kc_in_izgara(&g_in, KC_T0 + KC_P);
    TEST_ASSERT_EQUAL_INT(IDA_RC_OK, kc_tick(&g_in));
    return rc;
}

static void varsayilana_dustu(ida_rc_t rc, int32_t numara)
{
    char msj[64];
    (void)snprintf(msj, sizeof msj, "A4-K80 denetim %ld", (long)numara);
    TEST_ASSERT_EQUAL_INT_MESSAGE(IDA_RC_PARAM_DEFAULTS, rc, msj);
    TEST_ASSERT_FALSE_MESSAGE(kc_out()->status.param_ok, msj);
    const ida_log_t *l = kc_gunluk_bul(kc_out(), (uint16_t)IDA_LOG_PARAM_LOAD);
    TEST_ASSERT_NOT_NULL_MESSAGE(l, msj);
    TEST_ASSERT_EQUAL_INT32_MESSAGE(numara, l->arg1, msj);
    /* Derlenmiş varsayılanlar yüklü. */
    param_bekle(sp_indis(P_LAG_SET), p_def(P_LAG_SET));
    param_bekle(sp_indis(P_IMU_STALE), p_def(P_IMU_STALE));
}

static size_t gecerli_kayitlar(void)
{
    size_t n = kc_varsayilan_kayitlar(g_k, KC_PARAM_AZAMI);
    /* Varsayılandan farklı, aralık içi değerler: yüklenip yüklenmediği görülsün. */
    kayit(n, sp_indis(P_LAG_SET))->ham = p_ozel(P_LAG_SET);
    kayit(n, sp_indis(P_IMU_STALE))->ham = p_ozel(P_IMU_STALE);
    return n;
}

/* ---------------------------------------------------------------------------------- */

static void test_A4_3_10_crc32_denetim_degeri(void)
{
    static const uint8_t m[9] = {'1', '2', '3', '4', '5', '6', '7', '8', '9'};
    TEST_ASSERT_EQUAL_HEX32(0xCBF43926u, kc_crc32(m, sizeof m));
}

static void test_A4_K85_tablo_tutarliligi(void)
{
    uint16_t n = ida_core_param_count();
    TEST_ASSERT_TRUE(n >= 6u);
    TEST_ASSERT_EQUAL_UINT16(n, ida_core_param_count());
    for (unsigned iu = 0; iu < (unsigned)(n); ++iu) {
        uint16_t i = (uint16_t)iu;
        const ida_param_desc_t *d = kc_param_desc(i);
        const char *son = (const char *)memchr(d->name, 0, sizeof d->name);
        TEST_ASSERT_NOT_NULL_MESSAGE(son, "ad NUL ile bitmeli");
        size_t uz = (size_t)(son - d->name);
        TEST_ASSERT_TRUE_MESSAGE(uz >= 1u && uz <= IDA_PARAM_NAME_MAX, "ad 1–16 karakter"); 
        TEST_ASSERT_TRUE(d->type >= IDA_PT_I32 && d->type <= IDA_PT_F32);
        TEST_ASSERT_TRUE(d->cls == IDA_PCLS_A || d->cls == IDA_PCLS_B);
        uint32_t a = kc_param_ham(&d->min);
        uint32_t b = kc_param_ham(&d->max);
        uint32_t v = kc_param_ham(&d->def);
        if (d->type == IDA_PT_U32) {
            TEST_ASSERT_TRUE_MESSAGE(a <= v && v <= b, d->name);
        } else if (d->type == IDA_PT_I32) {
            TEST_ASSERT_TRUE_MESSAGE((int32_t)a <= (int32_t)v && (int32_t)v <= (int32_t)b, d->name);
        } else {
            float fa;
            float fb;
            float fv;
            memcpy(&fa, &a, sizeof fa);
            memcpy(&fb, &b, sizeof fb);
            memcpy(&fv, &v, sizeof fv);
            TEST_ASSERT_TRUE_MESSAGE(fa <= fv && fv <= fb, d->name);
        }
        for (unsigned ju = 0; ju < (unsigned)(i); ++ju) {
            uint16_t j = (uint16_t)ju;
            TEST_ASSERT_TRUE_MESSAGE(kc_param_desc(j)->id != d->id, "yinelenen id");
            TEST_ASSERT_TRUE_MESSAGE(strcmp(kc_param_desc(j)->name, d->name) != 0, "yinelenen ad");
        }
        uint16_t id = 0u;
        TEST_ASSERT_EQUAL_INT(IDA_RC_OK, ida_core_param_find(d->name, &id));
        TEST_ASSERT_EQUAL_UINT16(d->id, id);
        /* A4-K85 (B17): min/max/def.type = type, def.id = id. */
        TEST_ASSERT_EQUAL_UINT8_MESSAGE(d->type, d->min.type, d->name);
        TEST_ASSERT_EQUAL_UINT8_MESSAGE(d->type, d->max.type, d->name);
        TEST_ASSERT_EQUAL_UINT8_MESSAGE(d->type, d->def.type, d->name);
        TEST_ASSERT_EQUAL_UINT16_MESSAGE(d->id, d->def.id, d->name);
    }
}

/* A4-K80 (6) (B7): şemada olmayan param_id'li kayıt bloğu geçersiz kılar. */
static void test_A4_K80_denetim_6_semada_olmayan_kayit(void)
{
    size_t n = gecerli_kayitlar();
    uint16_t azami = 0u;
    for (size_t i = 0; i < n; ++i) {
        if (g_k[i].id > azami) {
            azami = g_k[i].id;
        }
    }
    TEST_ASSERT_TRUE(azami < UINT16_MAX);
    g_k[n].id = (uint16_t)(azami + 1u);
    g_k[n].type = (uint8_t)IDA_PT_U32;
    g_k[n].rsv = 0u;
    g_k[n].ham = 1u;
    size_t boy = kc_blok_kur(g_blok, sizeof g_blok, g_k, n + 1u, kc_sema_ozeti());
    varsayilana_dustu(blokla_init(g_blok, (uint32_t)boy, 6), 6);
}

/* A4 §3.9: sözleşmenin değerini verdiği altı parametre şemada tip, varsayılan ve aralıkla. */
static void test_A4_3_9_sozlesme_parametreleri(void)
{
    char eksik[200];
    size_t n = 0;
    eksik[0] = '\0';
    for (int k = 0; k < 6; ++k) {
        if (sp_kesin(k) < 0) {
            int y = snprintf(eksik + n, sizeof eksik - n, "%s ", sp_tanim(k).ad);
            if (y > 0 && (size_t)y < sizeof eksik - n) {
                n += (size_t)y;
            }
        }
    }
    char msj[300];
    (void)snprintf(msj, sizeof msj, "şemada A4 §3.9'daki tip, varsayılan ve aralıkla (süreler ms) bulunmayanlar: %s", eksik);
    TEST_ASSERT_TRUE_MESSAGE(n == 0u, msj);
}

static void test_A4_K80_gecerli_blok_yuklenir(void)
{
    size_t n = gecerli_kayitlar();
    size_t boy = kc_blok_kur(g_blok, sizeof g_blok, g_k, n, kc_sema_ozeti());
    ida_rc_t rc = blokla_init(g_blok, (uint32_t)boy, 0);
    TEST_ASSERT_EQUAL_INT(IDA_RC_OK, rc);
    TEST_ASSERT_TRUE(kc_out()->status.param_ok);
    const ida_log_t *l = kc_gunluk_bul(kc_out(), (uint16_t)IDA_LOG_PARAM_LOAD);
    TEST_ASSERT_NOT_NULL(l);
    TEST_ASSERT_EQUAL_INT32((int32_t)IDA_RC_OK, l->arg0);
    TEST_ASSERT_EQUAL_INT32(0, l->arg1);
    TEST_ASSERT_EQUAL_UINT8(IDA_SEV_INFO, l->severity);
    param_bekle(sp_indis(P_LAG_SET), p_ozel(P_LAG_SET));
    param_bekle(sp_indis(P_IMU_STALE), p_ozel(P_IMU_STALE));
    param_bekle(sp_indis(P_DET_STALE), p_def(P_DET_STALE));
}

static void test_A4_K80_blok_yok(void)
{
    static const uint8_t bir[1] = {0u};
    TEST_ASSERT_EQUAL_INT(IDA_RC_PARAM_DEFAULTS, blokla_init(NULL, 0u, 0));
    varsayilana_dustu(IDA_RC_PARAM_DEFAULTS, 0);
    /* NULL değilken param_len = 0: blok yok (numara 0). */
    TEST_ASSERT_EQUAL_INT(IDA_RC_PARAM_DEFAULTS, blokla_init(bir, 0u, 0));
    varsayilana_dustu(IDA_RC_PARAM_DEFAULTS, 0);
}

static void test_A4_K80_denetim_1_baslik(void)
{
    size_t n = gecerli_kayitlar();
    for (int v = 0; v < 4; ++v) {
        size_t boy = kc_blok_kur(g_blok, sizeof g_blok, g_k, n, kc_sema_ozeti());
        uint32_t len = (uint32_t)boy;
        if (v == 0) {
            g_blok[0] = (uint8_t)(g_blok[0] ^ 0x01u); /* magic */
        } else if (v == 1) {
            kc_le16(&g_blok[4], 2u); /* fmt_ver */
        } else if (v == 2) {
            kc_le16(&g_blok[6], 1u); /* rsv */
        } else {
            len = 23u; /* 24 bayttan kısa */
        }
        varsayilana_dustu(blokla_init(g_blok, len, 1), 1);
    }
}

static void test_A4_K80_denetim_2_sema_ozeti(void)
{
    size_t n = gecerli_kayitlar();
    size_t boy = kc_blok_kur(g_blok, sizeof g_blok, g_k, n, kc_sema_ozeti() ^ UINT64_C(1));
    varsayilana_dustu(blokla_init(g_blok, (uint32_t)boy, 2), 2);
}

static void test_A4_K80_denetim_3_boy(void)
{
    size_t n = gecerli_kayitlar();
    size_t boy = kc_blok_kur(g_blok, sizeof g_blok, g_k, n, kc_sema_ozeti());
    /* Blok boyu 24 + payload_len değil. */
    varsayilana_dustu(blokla_init(g_blok, (uint32_t)(boy + 8u), 3), 3);
    /* payload_len 8'in katı değil (ve boyla tutarlı). */
    boy = kc_blok_kur(g_blok, sizeof g_blok, g_k, n, kc_sema_ozeti());
    kc_le32(&g_blok[16], (uint32_t)(boy - 24u - 4u));
    kc_le32(&g_blok[20], kc_crc32(&g_blok[24], boy - 24u - 4u));
    varsayilana_dustu(blokla_init(g_blok, (uint32_t)(boy - 4u), 3), 3);
    /* param_len > 8000. */
    boy = kc_blok_kur(g_blok, sizeof g_blok, g_k, n, kc_sema_ozeti());
    varsayilana_dustu(blokla_init(g_blok, IDA_PARAM_BLOB_MAX + 1u, 3), 3);
}

static void test_A4_K80_denetim_4_crc(void)
{
    size_t n = gecerli_kayitlar();
    size_t boy = kc_blok_kur(g_blok, sizeof g_blok, g_k, n, kc_sema_ozeti());
    g_blok[20] = (uint8_t)(g_blok[20] ^ 0x80u);
    varsayilana_dustu(blokla_init(g_blok, (uint32_t)boy, 4), 4);
}

static void test_A4_K80_denetim_5_sira_ve_rsv(void)
{
    for (int v = 0; v < 3; ++v) {
        size_t n = gecerli_kayitlar();
        TEST_ASSERT_TRUE(n >= 2u);
        if (v == 0) {
            kc_pkayit_t t = g_k[0];
            g_k[0] = g_k[1];
            g_k[1] = t;
        } else if (v == 1) {
            g_k[1].id = g_k[0].id;
            g_k[1].type = g_k[0].type;
        } else {
            g_k[n - 1u].rsv = 1u;
        }
        size_t boy = kc_blok_kur(g_blok, sizeof g_blok, g_k, n, kc_sema_ozeti());
        varsayilana_dustu(blokla_init(g_blok, (uint32_t)boy, 5), 5);
    }
}

static void test_A4_K80_denetim_6_eksik_ve_tip(void)
{
    size_t n = gecerli_kayitlar();
    size_t boy = kc_blok_kur(g_blok, sizeof g_blok, g_k, n - 1u, kc_sema_ozeti());
    varsayilana_dustu(blokla_init(g_blok, (uint32_t)boy, 6), 6);
    n = gecerli_kayitlar();
    kc_pkayit_t *k = kayit(n, sp_indis(P_LAG_SET));
    k->type = (uint8_t)IDA_PT_I32;
    boy = kc_blok_kur(g_blok, sizeof g_blok, g_k, n, kc_sema_ozeti());
    varsayilana_dustu(blokla_init(g_blok, (uint32_t)boy, 6), 6);
}

static void test_A4_K80_denetim_7_aralik(void)
{
    uint32_t nan_ham = 0x7FC00000u;
    for (int v = 0; v < 6; ++v) {
        size_t n = gecerli_kayitlar();
        if (v == 0) {
            kayit(n, sp_indis(P_LAG_SET))->ham = p_max(P_LAG_SET) + 1u;
        } else if (v == 1) {
            /* §7 K39: T_imu_stale = 4 ve 31 ms (aralığın bir altı ve bir üstü) reddedilir */
            kayit(n, sp_indis(P_IMU_STALE))->ham = p_min(P_IMU_STALE) - 1u;
        } else if (v == 2) {
            kayit(n, sp_indis(P_IMU_STALE))->ham = p_max(P_IMU_STALE) + 1u;
        } else if (v == 3) {
            kayit(n, sp_indis(P_GAP_TOL))->ham = f32_ham(0.001f);
        } else if (v == 4) {
            kayit(n, sp_indis(P_GAP_TOL))->ham = nan_ham;
        } else {
            kayit(n, sp_indis(P_DET_STALE))->ham = p_max(P_DET_STALE) + 1u;
        }
        size_t boy = kc_blok_kur(g_blok, sizeof g_blok, g_k, n, kc_sema_ozeti());
        varsayilana_dustu(blokla_init(g_blok, (uint32_t)boy, 7), 7);
    }
}

static void test_A4_K80_denetim_8_iliski(void)
{
    /* T_lag_clear ≤ T_lag_set ilişkisi: ikisi de aralık içi, clear > set. */
    uint32_t set = p_def(P_LAG_SET);
    uint32_t clear = set + (p_max(P_LAG_CLEAR) - set) / 2u;
    TEST_ASSERT_TRUE(clear > set && clear <= p_max(P_LAG_CLEAR));
    size_t n = gecerli_kayitlar();
    kayit(n, sp_indis(P_LAG_SET))->ham = set;
    kayit(n, sp_indis(P_LAG_CLEAR))->ham = clear;
    size_t boy = kc_blok_kur(g_blok, sizeof g_blok, g_k, n, kc_sema_ozeti());
    varsayilana_dustu(blokla_init(g_blok, (uint32_t)boy, 8), 8);
    /* Eşitlik geçerlidir. */
    n = gecerli_kayitlar();
    kayit(n, sp_indis(P_LAG_SET))->ham = set;
    kayit(n, sp_indis(P_LAG_CLEAR))->ham = set;
    boy = kc_blok_kur(g_blok, sizeof g_blok, g_k, n, kc_sema_ozeti());
    TEST_ASSERT_EQUAL_INT(IDA_RC_OK, blokla_init(g_blok, (uint32_t)boy, 0));
}

/* İlk başarısız denetimin numarası: birden çok denetim bozulduğunda en küçük numara. */
static void test_A4_K80_ilk_basarisiz_denetim(void)
{
    size_t n = gecerli_kayitlar();
    kayit(n, sp_indis(P_LAG_SET))->ham = p_max(P_LAG_SET) + 1u; /* (7) */
    size_t boy = kc_blok_kur(g_blok, sizeof g_blok, g_k, n, kc_sema_ozeti());
    g_blok[21] = (uint8_t)(g_blok[21] ^ 0x01u); /* (4) */
    varsayilana_dustu(blokla_init(g_blok, (uint32_t)boy, 4), 4);
    n = gecerli_kayitlar();
    boy = kc_blok_kur(g_blok, sizeof g_blok, g_k, n, kc_sema_ozeti() + 5u); /* (2) */
    g_blok[21] = (uint8_t)(g_blok[21] ^ 0x01u); /* (4) */
    varsayilana_dustu(blokla_init(g_blok, (uint32_t)boy, 2), 2);
}

/* A4-K78: PARAM_DEFAULTS'ta ARM reddedilir (yer tutucuda UNSUPPORTED da olabilir; ACCEPTED
   olamaz); 3.17 ARM satırı sırası: EVENTS_OFF, PARAM_INVALID, PARAM_BUSY, NOT_ARMABLE. */
static void arm_dene(uint8_t beklenen_neden, uint8_t param_store)
{
    kc_in_step(&g_in, kc_sonraki_seq(), kc_sonraki_t());
    g_in.plat.param_store = param_store;
    (void)kc_komut_ekle(&g_in, 0xA000u + (uint32_t)kc_sonraki_seq(), (uint8_t)IDA_CMD_ARM);
    TEST_ASSERT_EQUAL_INT(IDA_RC_OK, kc_step(&g_in));
    const ida_cmd_result_t *r = &kc_out()->results[0];
    bool unsupported = r->ack == IDA_ACK_UNSUPPORTED && r->reason == IDA_REASON_NONE;
    bool red = r->ack == IDA_ACK_REJECTED && r->reason == beklenen_neden;
    char msj[96];
    (void)snprintf(msj, sizeof msj, "ARM %u/%u; beklenen UNSUPPORTED/NONE ya da REJECTED/%u", (unsigned)r->ack,
                   (unsigned)r->reason, (unsigned)beklenen_neden);
    TEST_ASSERT_TRUE_MESSAGE(unsupported || red, msj);
    TEST_ASSERT_FALSE(kc_out()->armed);
}

static void test_A4_K78_arm_engeli(void)
{
    ida_init_in_t ii;
    kc_init_bloksuz();
    TEST_ASSERT_EQUAL_INT(IDA_RC_OK, kc_adim());
    TEST_ASSERT_FALSE(kc_out()->status.param_ok);
    arm_dene((uint8_t)IDA_REASON_PARAM_INVALID, (uint8_t)IDA_PSTORE_IDLE);
    /* Olay kimliği yoksa EVENTS_OFF önce gelir (A4-K61). */
    kc_init_girdi(&ii);
    ii.boot_count_ok = false;
    TEST_ASSERT_EQUAL_INT(IDA_RC_PARAM_DEFAULTS, kc_init(&ii));
    arm_dene((uint8_t)IDA_REASON_EVENTS_OFF, (uint8_t)IDA_PSTORE_IDLE);
    kc_init_girdi(&ii);
    ii.boot_count = 0xFFFFFFFFu;
    TEST_ASSERT_EQUAL_INT(IDA_RC_PARAM_DEFAULTS, kc_init(&ii));
    arm_dene((uint8_t)IDA_REASON_EVENTS_OFF, (uint8_t)IDA_PSTORE_IDLE);
    /* Geçerli blokla engel yok; kayıt sürerken PARAM_BUSY (A4-K83). */
    kc_init_gecerli();
    TEST_ASSERT_EQUAL_INT(IDA_RC_OK, kc_adim());
    TEST_ASSERT_TRUE(kc_out()->status.param_ok);
    arm_dene((uint8_t)IDA_REASON_PARAM_BUSY, (uint8_t)IDA_PSTORE_BUSY);
    arm_dene((uint8_t)IDA_REASON_NOT_ARMABLE, (uint8_t)IDA_PSTORE_IDLE);
}

/* ---- Parametre komutları: A4-K81, K82, K83 (fark turu) ------------------------------ */

/* Bir step: verilen kayıt durumu ve kuşağıyla; komutlar çağıran tarafından eklenir. */
static void param_step_hazirla(uint8_t store, uint32_t gen)
{
    kc_in_step(&g_in, kc_sonraki_seq(), kc_sonraki_t());
    g_in.plat.param_store = store;
    g_in.plat.param_gen = gen;
}

static void kayit_kabul(const ida_cmd_result_t *r, uint32_t gen, const char *ad)
{
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(IDA_RES_PARAM_SAVE, r->kind, ad);
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(IDA_ACK_ACCEPTED, r->ack, ad);
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(gen, r->save.gen, ad);
}

static void reddedildi(const ida_cmd_result_t *r, uint16_t neden, const char *ad)
{
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(IDA_ACK_REJECTED, r->ack, ad);
    TEST_ASSERT_EQUAL_UINT16_MESSAGE(neden, r->reason, ad);
}

/* Son çıktıdaki PARAM_SAVE günlüklerinin sayısı; varsa ilkinin (gen, param_store)'u
   beklenenle aynı olmalı (3.15). */
static int kayit_gunlugu(uint32_t gen, uint8_t store)
{
    int n = kc_gunluk_say(kc_out(), (uint16_t)IDA_LOG_PARAM_SAVE);
    const ida_log_t *l = kc_gunluk_bul(kc_out(), (uint16_t)IDA_LOG_PARAM_SAVE);
    if (l != NULL) {
        TEST_ASSERT_EQUAL_INT32_MESSAGE((int32_t)gen, l->arg0, "PARAM_SAVE arg0 = gen");
        TEST_ASSERT_EQUAL_INT32_MESSAGE((int32_t)store, l->arg1, "PARAM_SAVE arg1 = param_store");
    }
    return n;
}

static void fail_step(uint32_t gen)
{
    param_step_hazirla((uint8_t)IDA_PSTORE_FAIL, gen);
    g_in.plat.param_fail = (uint8_t)IDA_PSFAIL_FLASH;
}

/* A4-K82, K83: FAIL de kuşak taşır. Bekleyen SAVE'in başarısız tamamlanması durum kodu
   geçişine bakmadan FAIL + param_gen = bekleyen gen eşleşmesiyle saptanır, günlük tam bir
   kez üretilir; FAIL bildirilen SAVE'in gen'i üretilmiş sayılır (taban yükselir). */
static void test_A4_K82_K83_fail_kusak_tasir(void)
{
    kc_init_bloksuz();
    param_step_hazirla((uint8_t)IDA_PSTORE_IDLE, 0u);
    (void)kc_komut_ekle(&g_in, 9001u, (uint8_t)IDA_CMD_PARAM_COMMIT);
    (void)kc_komut_ekle(&g_in, 9002u, (uint8_t)IDA_CMD_PARAM_SAVE);
    TEST_ASSERT_EQUAL_INT(IDA_RC_OK, kc_step(&g_in));
    TEST_ASSERT_EQUAL_UINT8(IDA_ACK_ACCEPTED, kc_out()->results[0].ack);
    kayit_kabul(&kc_out()->results[1], 1u, "ilk kayıt gen 1");
    /* FAIL/1: gen 1'in başarısız tamamlanması; günlük bir kez. */
    fail_step(1u);
    TEST_ASSERT_EQUAL_INT(IDA_RC_OK, kc_step(&g_in));
    TEST_ASSERT_EQUAL_INT_MESSAGE(1, kayit_gunlugu(1u, (uint8_t)IDA_PSTORE_FAIL), "FAIL/1 tamamlanması");
    TEST_ASSERT_FALSE(kc_out()->status.param_ok);
    /* Girdi FAIL/1 kalırken yeni SAVE: gen 2; FAIL/1 yeniden günlüklenmez. */
    fail_step(1u);
    (void)kc_komut_ekle(&g_in, 9003u, (uint8_t)IDA_CMD_PARAM_SAVE);
    TEST_ASSERT_EQUAL_INT(IDA_RC_OK, kc_step(&g_in));
    kayit_kabul(&kc_out()->results[0], 2u, "FAIL bildirilen kaydın gen'i yeniden kullanılmaz (A4-K82)");
    TEST_ASSERT_EQUAL_INT(0, kayit_gunlugu(1u, (uint8_t)IDA_PSTORE_FAIL));
    /* Girdide hâlâ FAIL/1: gen 2'nin beklemesi bitmez. */
    fail_step(1u);
    TEST_ASSERT_EQUAL_INT(IDA_RC_OK, kc_step(&g_in));
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, kayit_gunlugu(2u, (uint8_t)IDA_PSTORE_FAIL), "FAIL/1 gen 2'yi bitirmemeli");
    /* FAIL/2 (arada başka durum yok): ikinci tamamlanma saptanır, günlük bir kez. */
    fail_step(2u);
    TEST_ASSERT_EQUAL_INT(IDA_RC_OK, kc_step(&g_in));
    TEST_ASSERT_EQUAL_INT_MESSAGE(1, kayit_gunlugu(2u, (uint8_t)IDA_PSTORE_FAIL), "FAIL/2 tamamlanması");
    fail_step(2u);
    TEST_ASSERT_EQUAL_INT_MESSAGE(IDA_RC_OK, kc_step(&g_in), "FAIL/2 yinelenir");
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, kayit_gunlugu(2u, (uint8_t)IDA_PSTORE_FAIL), "günlük tam bir kez");
    /* Sonraki SAVE'in gen'i 3. */
    fail_step(2u);
    (void)kc_komut_ekle(&g_in, 9004u, (uint8_t)IDA_CMD_PARAM_SAVE);
    TEST_ASSERT_EQUAL_INT(IDA_RC_OK, kc_step(&g_in));
    kayit_kabul(&kc_out()->results[0], 3u, "gen 3");
}

/* A4-K83, K78 (sol bulgu 2'nin karşı örneği): başarılı tamamlanma arada BUSY görülmeden
   aynı durum koduyla (OK/41 -> OK/42) gelirse de gen eşleşmesiyle saptanır: arm engeli
   kalkar, PARAM_SAVE günlüğü tam bir kez üretilir (eski belirsiz R1). */
static void test_A4_K83_ayni_durum_koduyla_tamamlanma(void)
{
    kc_init_bloksuz();
    param_step_hazirla((uint8_t)IDA_PSTORE_OK, 41u);
    (void)kc_komut_ekle(&g_in, 9011u, (uint8_t)IDA_CMD_PARAM_COMMIT);
    (void)kc_komut_ekle(&g_in, 9012u, (uint8_t)IDA_CMD_PARAM_SAVE);
    TEST_ASSERT_EQUAL_INT(IDA_RC_OK, kc_step(&g_in));
    kayit_kabul(&kc_out()->results[1], 42u, "gen = max(0, 41) + 1");
    TEST_ASSERT_FALSE(kc_out()->status.param_ok);
    for (int k = 0; k < 3; ++k) {
        param_step_hazirla((uint8_t)IDA_PSTORE_OK, 42u);
        TEST_ASSERT_EQUAL_INT(IDA_RC_OK, kc_step(&g_in));
        TEST_ASSERT_TRUE_MESSAGE(kc_out()->status.param_ok,
                                 "OK/42 görüldü: kayıt tamamlandı, param_ok = 1 (A4-K78, K83)");
        TEST_ASSERT_EQUAL_INT_MESSAGE(k == 0 ? 1 : 0, kayit_gunlugu(42u, (uint8_t)IDA_PSTORE_OK),
                                      "PARAM_SAVE günlüğü tamamlanmanın saptandığı çağrıda tam bir kez");
    }
}

/* A4-K81, K82: aynı çağrıdaki her ARM sayılır (yinelenen ya da NARROWED alan dahil). */
static void test_A4_K81_K82_her_ARM_sayilir(void)
{
    kc_init_bloksuz();
    param_step_hazirla((uint8_t)IDA_PSTORE_IDLE, 0u);
    (void)kc_komut_ekle(&g_in, 9101u, (uint8_t)IDA_CMD_PARAM_COMMIT);
    TEST_ASSERT_EQUAL_INT(IDA_RC_OK, kc_step(&g_in));
    param_step_hazirla((uint8_t)IDA_PSTORE_IDLE, 0u);
    (void)kc_komut_ekle(&g_in, 9100u, (uint8_t)IDA_CMD_ARM);
    TEST_ASSERT_EQUAL_INT(IDA_RC_OK, kc_step(&g_in));
    /* NARROWED alan ARM. */
    param_step_hazirla((uint8_t)IDA_PSTORE_IDLE, 0u);
    kc_mandal_kur(&g_in, (uint8_t)IDA_NARROW_DISARM, 9190u);
    (void)kc_komut_ekle(&g_in, 9102u, (uint8_t)IDA_CMD_ARM);
    (void)kc_komut_ekle(&g_in, 9103u, (uint8_t)IDA_CMD_PARAM_SAVE);
    TEST_ASSERT_EQUAL_INT(IDA_RC_OK, kc_step(&g_in));
    TEST_ASSERT_EQUAL_UINT16(IDA_REASON_NARROWED, kc_out()->results[1].reason);
    reddedildi(&kc_out()->results[2], (uint16_t)IDA_REASON_ARMED, "NARROWED ARM ile aynı çağrıda SAVE");
    /* Yinelenen kimlikli ARM. */
    param_step_hazirla((uint8_t)IDA_PSTORE_IDLE, 0u);
    (void)kc_komut_ekle(&g_in, 9104u, (uint8_t)IDA_CMD_PARAM_SAVE);
    (void)kc_komut_ekle(&g_in, 9100u, (uint8_t)IDA_CMD_ARM);
    TEST_ASSERT_EQUAL_INT(IDA_RC_OK, kc_step(&g_in));
    TEST_ASSERT_EQUAL_UINT8(IDA_ACK_DUPLICATE, kc_out()->results[1].ack);
    reddedildi(&kc_out()->results[0], (uint16_t)IDA_REASON_ARMED, "yinelenen ARM ile aynı çağrıda SAVE");
    /* ARM'sız SAVE kabul edilir (karşı örnek). */
    param_step_hazirla((uint8_t)IDA_PSTORE_IDLE, 0u);
    (void)kc_komut_ekle(&g_in, 9105u, (uint8_t)IDA_CMD_PARAM_SAVE);
    TEST_ASSERT_EQUAL_INT(IDA_RC_OK, kc_step(&g_in));
    kayit_kabul(&kc_out()->results[0], 1u, "ARM'sız SAVE");
    /* B sınıfı bekleyen değer + aynı çağrıda ARM: COMMIT ARMED ve bekleyenler atılır. */
    int b_sinifi = -1;
    for (unsigned iu = 0; iu < kc_param_sayisi() && b_sinifi < 0; ++iu) {
        const ida_param_desc_t *d = kc_param_desc((uint16_t)iu);
        if (d->cls == IDA_PCLS_B && kc_param_ham(&d->max) != kc_param_ham(&d->def)) {
            b_sinifi = (int)iu;
        }
    }
    if (b_sinifi < 0) {
        TEST_MESSAGE("şemada max ≠ def olan B sınıfı parametre yok; COMMIT/ARM vakası koşulmadı");
    } else {
        const ida_param_desc_t *d = kc_param_desc((uint16_t)b_sinifi);
        param_step_hazirla((uint8_t)IDA_PSTORE_IDLE, 0u);
        ida_cmd_t *c = kc_komut_ekle(&g_in, 9106u, (uint8_t)IDA_CMD_PARAM_SET);
        c->u.param = d->max;
        c->u.param.id = d->id;
        c->u.param.type = d->type;
        TEST_ASSERT_EQUAL_INT(IDA_RC_OK, kc_step(&g_in));
        TEST_ASSERT_EQUAL_UINT8(IDA_ACK_ACCEPTED, kc_out()->results[0].ack);
        param_step_hazirla((uint8_t)IDA_PSTORE_IDLE, 0u);
        (void)kc_komut_ekle(&g_in, 9107u, (uint8_t)IDA_CMD_PARAM_COMMIT);
        (void)kc_komut_ekle(&g_in, 9108u, (uint8_t)IDA_CMD_ARM);
        TEST_ASSERT_EQUAL_INT(IDA_RC_OK, kc_step(&g_in));
        reddedildi(&kc_out()->results[0], (uint16_t)IDA_REASON_ARMED, "B sınıfı commit + ARM (A4-K81)");
        param_step_hazirla((uint8_t)IDA_PSTORE_IDLE, 0u);
        (void)kc_komut_ekle(&g_in, 9109u, (uint8_t)IDA_CMD_PARAM_COMMIT);
        TEST_ASSERT_EQUAL_INT(IDA_RC_OK, kc_step(&g_in));
        param_step_hazirla((uint8_t)IDA_PSTORE_IDLE, 0u);
        kc_komut_ekle(&g_in, 9110u, (uint8_t)IDA_CMD_PARAM_GET)->u.param.id = d->id;
        TEST_ASSERT_EQUAL_INT(IDA_RC_OK, kc_step(&g_in));
        TEST_ASSERT_EQUAL_HEX32_MESSAGE(kc_param_ham(&d->def), kc_param_ham(&kc_out()->results[0].value),
                                        "reddedilen commit'in bekleyen değerleri atılmalı (A4-K81)");
    }
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_A4_3_10_crc32_denetim_degeri);
    RUN_TEST(test_A4_K85_tablo_tutarliligi);
    RUN_TEST(test_A4_3_9_sozlesme_parametreleri);
    RUN_TEST(test_A4_K80_gecerli_blok_yuklenir);
    RUN_TEST(test_A4_K80_blok_yok);
    RUN_TEST(test_A4_K80_denetim_1_baslik);
    RUN_TEST(test_A4_K80_denetim_2_sema_ozeti);
    RUN_TEST(test_A4_K80_denetim_3_boy);
    RUN_TEST(test_A4_K80_denetim_4_crc);
    RUN_TEST(test_A4_K80_denetim_5_sira_ve_rsv);
    RUN_TEST(test_A4_K80_denetim_6_eksik_ve_tip);
    RUN_TEST(test_A4_K80_denetim_7_aralik);
    RUN_TEST(test_A4_K80_denetim_8_iliski);
    RUN_TEST(test_A4_K80_ilk_basarisiz_denetim);
    RUN_TEST(test_A4_K78_arm_engeli);
    RUN_TEST(test_A4_K80_denetim_6_semada_olmayan_kayit);
    RUN_TEST(test_A4_K82_K83_fail_kusak_tasir);
    RUN_TEST(test_A4_K83_ayni_durum_koduyla_tamamlanma);
    RUN_TEST(test_A4_K81_K82_her_ARM_sayilir);
    return UNITY_END();
}
