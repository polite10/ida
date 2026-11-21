/*
 * test_guvenli.c — güvenli çıktı ve belirlenimcilik (W3-kontrol kabul md. 5, 6; kapsam md. 3).
 *
 * EMNİYET: silahsız, kısıtlı ya da kestirimi yetersiz her adımda sekiz komutun bit deseni
 * tam 0x00000000'dır (−0.0f de kabul edilmez); rastgele girdi dizisiyle (sabit tohum) de.
 */
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "destek.h"
#include "kontrol/kontrol.h"
#include "mat/ida_math.h"
#include "param/param.h"
#include "unity.h"

void setUp(void);
void tearDown(void);

static ida_prm_kume_t P;
static ida_kon_t K1;
static ida_kon_t K2;
static ida_core_t C;
static ida_in_t IN;
static ida_out_t OUT;

void setUp(void)
{
    ida_prm_varsayilan(&P);
    ida_kon_kur(&K1);
    ida_kon_kur(&K2);
}
void tearDown(void) {}

typedef union {
    uint32_t u;
    float f;
} bit32_t;

static uint32_t bitler(float f)
{
    bit32_t b;
    b.f = f;
    return b.u;
}

static void prm_u32(enum ida_prm_indis i, uint32_t v)
{
    P.ham[i] = v;
}

static void etkin_girdi(ida_kon_girdi_t *g, int64_t t_ns)
{
    (void)memset(g, 0, sizeof(*g));
    g->istek.kip = IDA_KON_KIP_DALIS;
    g->istek.dikey = IDA_KON_DIKEY_DERINLIK;
    g->istek.tutum = IDA_KON_TUTUM_ACI;
    g->istek.yon_kanali = IDA_KON_YON_ACI;
    g->istek.yatay = IDA_KON_YATAY_HIZ;
    g->istek.derinlik = 3.0f;
    g->istek.yon = 0.5f;
    g->istek.hiz[0] = 0.3f;
    g->nav.t.ns = t_ns;
    g->nav.q_nb[0] = 1.0f;
    g->nav.depth = 0.5f;
    g->nav.est_level = (uint8_t)IDA_EST_OK;
    g->t_ns = t_ns;
    g->armed = true;
    g->kisit = false;
    g->kestirim_yeterli = true;
}

static bool hepsi_sifir(const ida_kon_t *k)
{
    float thr[IDA_PORT_N_THR];
    bool cokme = true;
    ida_kon_itki(k, thr, &cokme);
    bool s = true;
    for (uint32_t i = 0u; i < IDA_PORT_N_THR; i++) {
        s = s && (bitler(thr[i]) == 0u);
    }
    return s;
}

static bool sifirdan_farkli_var(const ida_kon_t *k)
{
    float thr[IDA_PORT_N_THR];
    bool cokme = true;
    ida_kon_itki(k, thr, &cokme);
    bool v = false;
    for (uint32_t i = 0u; i < IDA_PORT_N_THR; i++) {
        v = v || (thr[i] != 0.0f);
    }
    return v && !cokme;
}

static void etkin_calis(ida_kon_t *k, int64_t *t, uint32_t n)
{
    ida_kon_girdi_t g;
    for (uint32_t i = 0u; i < n; i++) {
        *t += 5 * DS_MS;
        etkin_girdi(&g, *t);
        ida_kon_hesapla(k, &P, &g);
    }
}

/* Kabul 5: üç durumun her birinde (ve KAPALI/geçersiz istekte) sekiz komut tam sıfır. */
static void test_uc_durumda_tam_sifir(void)
{
    int64_t t = 1000 * DS_MS;
    ida_kon_girdi_t g;
    for (uint32_t durum = 0u; durum < 6u; durum++) {
        ida_kon_kur(&K1);
        etkin_calis(&K1, &t, 40u);
        TEST_ASSERT_TRUE(sifirdan_farkli_var(&K1)); /* önce gerçekten itki var */
        t += 5 * DS_MS;
        etkin_girdi(&g, t);
        switch (durum) {
        case 0u:
            g.armed = false;
            break;
        case 1u:
            g.kisit = true;
            break;
        case 2u:
            g.kestirim_yeterli = false;
            break;
        case 3u:
            g.istek.kip = IDA_KON_KIP_KAPALI;
            break;
        case 4u:
            g.istek.derinlik = ds_nan(); /* geçersiz istek */
            break;
        default:
            g.istek.yatay = 7u; /* tanımsız seçici */
            break;
        }
        ida_kon_hesapla(&K1, &P, &g);
        TEST_ASSERT_TRUE(hepsi_sifir(&K1));
        bool cokme = true;
        float thr[IDA_PORT_N_THR];
        ida_kon_itki(&K1, thr, &cokme);
        TEST_ASSERT_FALSE(cokme);
        TEST_ASSERT_TRUE(K1.i_w == 0.0f); /* döngü durumu sıfırlandı */
        /* Koşul sürdükçe sıfır kalır. */
        for (uint32_t i = 0u; i < 20u; i++) {
            t += 5 * DS_MS;
            g.t_ns = t;
            ida_kon_hesapla(&K1, &P, &g);
            TEST_ASSERT_TRUE(hepsi_sifir(&K1));
        }
    }
}

/* Sonlu olmayan kestirim: sayısal çökme, komut sıfır (A4-K55). */
static void test_sayisal_cokme(void)
{
    int64_t t = 1000 * DS_MS;
    etkin_calis(&K1, &t, 40u);
    ida_kon_girdi_t g;
    etkin_girdi(&g, t + (5 * DS_MS));
    g.nav.v_ned[2] = ds_sonsuz();
    ida_kon_hesapla(&K1, &P, &g);
    bool cokme = false;
    float thr[IDA_PORT_N_THR];
    ida_kon_itki(&K1, thr, &cokme);
    TEST_ASSERT_TRUE(cokme);
    TEST_ASSERT_TRUE(hepsi_sifir(&K1));
}

/* Devre dışı itici (parametre ya da görev maskesi): komutu tam sıfır, diğerleri çalışır. */
static void test_devre_disi_itici_sifir(void)
{
    int64_t t = 1000 * DS_MS;
    prm_u32(IDA_PRM_kon_itici_disi, 1u << 4);
    ida_kon_girdi_t g;
    for (uint32_t i = 0u; i < 40u; i++) {
        t += 5 * DS_MS;
        etkin_girdi(&g, t);
        g.itici_disi = (uint8_t)(1u << 1);
        ida_kon_hesapla(&K1, &P, &g);
        TEST_ASSERT_TRUE(bitler(K1.thr[4]) == 0u);
        TEST_ASSERT_TRUE(bitler(K1.thr[1]) == 0u);
    }
    TEST_ASSERT_TRUE(sifirdan_farkli_var(&K1));
}

/* Rastgele girdi (sabit tohum). */
static float rastgele_aralik(ds_rastgele_t *r, float a, float b)
{
    const float x = (float)(ds_rastgele(r) % 100001u) / 100000.0f;
    return a + ((b - a) * x);
}

static void rastgele_girdi(ds_rastgele_t *r, ida_kon_girdi_t *g, int64_t *t)
{
    const uint32_t z = ds_rastgele(r) % 100u;
    if (z < 3u) {
        *t -= 2 * DS_MS; /* geri giden zaman */
    } else if (z < 6u) {
        *t += 200 * DS_MS; /* uzun boşluk */
    } else {
        *t += (int64_t)(ds_rastgele(r) % 20u) * DS_MS;
    }
    etkin_girdi(g, *t);
    g->armed = (ds_rastgele(r) % 10u) < 7u;
    g->kisit = (ds_rastgele(r) % 10u) < 2u;
    g->kestirim_yeterli = (ds_rastgele(r) % 10u) < 8u;
    g->itici_disi = ((ds_rastgele(r) % 20u) == 0u) ? (uint8_t)(ds_rastgele(r) & 0xFFu) : 0u;
    ida_kon_istek_t *s = &g->istek;
    s->kip = (uint8_t)(ds_rastgele(r) % 4u);
    s->dikey = (uint8_t)(ds_rastgele(r) % 4u);
    s->tutum = (uint8_t)(ds_rastgele(r) % 4u);
    s->yon_kanali = (uint8_t)(ds_rastgele(r) % 4u);
    s->yatay = (uint8_t)(ds_rastgele(r) % 3u);
    s->derinlik = rastgele_aralik(r, -1.0f, 50.0f);
    s->dikey_hiz = rastgele_aralik(r, -2.0f, 2.0f);
    s->yalpa = rastgele_aralik(r, -1.0f, 1.0f);
    s->bas_kic = rastgele_aralik(r, -1.0f, 1.0f);
    s->yalpa_hizi = rastgele_aralik(r, -2.0f, 2.0f);
    s->bas_kic_hizi = rastgele_aralik(r, -2.0f, 2.0f);
    s->yon = rastgele_aralik(r, -4.0f, 4.0f);
    s->yon_hizi = rastgele_aralik(r, -2.0f, 2.0f);
    s->hiz[0] = rastgele_aralik(r, -3.0f, 3.0f);
    s->hiz[1] = rastgele_aralik(r, -3.0f, 3.0f);
    for (uint32_t j = 0u; j < 6u; j++) {
        s->kuvvet[j] = rastgele_aralik(r, -500.0f, 500.0f);
    }
    ida_nav_out_t *n = &g->nav;
    n->q_nb[0] = rastgele_aralik(r, -1.0f, 1.0f);
    n->q_nb[1] = rastgele_aralik(r, -0.5f, 0.5f);
    n->q_nb[2] = rastgele_aralik(r, -0.5f, 0.5f);
    n->q_nb[3] = rastgele_aralik(r, -1.0f, 1.0f);
    n->depth = rastgele_aralik(r, -1.0f, 30.0f);
    for (uint32_t j = 0u; j < 3u; j++) {
        n->v_ned[j] = rastgele_aralik(r, -2.0f, 2.0f);
        n->omega_b[j] = rastgele_aralik(r, -2.0f, 2.0f);
    }
    const uint32_t bozuk = ds_rastgele(r) % 100u;
    if (bozuk == 0u) {
        s->yon = ds_nan();
    } else if (bozuk == 1u) {
        n->depth = ds_nan();
    } else if (bozuk == 2u) {
        n->q_nb[0] = 0.0f;
        n->q_nb[1] = 0.0f;
        n->q_nb[2] = 0.0f;
        n->q_nb[3] = 0.0f;
    }
}

static void test_rastgele_dizi(void)
{
    ds_rastgele_t r = {0xC0FFEEu};
    int64_t t = 1000 * DS_MS;
    ida_kon_girdi_t g;
    uint32_t etkin_sayisi = 0u;
    for (uint32_t n = 0u; n < 30000u; n++) {
        rastgele_girdi(&r, &g, &t);
        ida_kon_hesapla(&K1, &P, &g);
        ida_kon_hesapla(&K2, &P, &g); /* kabul 6: aynı girdi, aynı durum ve çıktı */
        TEST_ASSERT_EQUAL_MEMORY(&K1, &K2, sizeof(K1));
        TEST_ASSERT_TRUE(ida_kon_tutarli(&K1));
        float thr[IDA_PORT_N_THR];
        bool cokme = false;
        ida_kon_itki(&K1, thr, &cokme);
        for (uint32_t i = 0u; i < IDA_PORT_N_THR; i++) {
            TEST_ASSERT_TRUE(ida_sonlu(thr[i]));
            TEST_ASSERT_TRUE((thr[i] >= -1.0f) && (thr[i] <= 1.0f));
        }
        if (!g.armed || g.kisit || !g.kestirim_yeterli || cokme) {
            TEST_ASSERT_TRUE(hepsi_sifir(&K1));
        }
        if (sifirdan_farkli_var(&K1)) {
            etkin_sayisi++;
        }
    }
    TEST_ASSERT_TRUE(etkin_sayisi > 1000u); /* dizi gerçekten itki üreten adımlar içeriyor */
}

/* Kabul 6: ayrı koşular aynı bit dizisini üretir. */
static uint32_t kosu_ozeti(void)
{
    ida_kon_kur(&K1);
    ds_rastgele_t r = {0xBEEFu};
    int64_t t = 1000 * DS_MS;
    ida_kon_girdi_t g;
    uint32_t h = 2166136261u; /* FNV-1a */
    for (uint32_t n = 0u; n < 5000u; n++) {
        rastgele_girdi(&r, &g, &t);
        ida_kon_hesapla(&K1, &P, &g);
        float thr[IDA_PORT_N_THR];
        bool cokme = false;
        ida_kon_itki(&K1, thr, &cokme);
        for (uint32_t i = 0u; i < IDA_PORT_N_THR; i++) {
            const uint32_t b = bitler(thr[i]);
            for (uint32_t k = 0u; k < 4u; k++) {
                h ^= (b >> (8u * k)) & 0xFFu;
                h *= 16777619u;
            }
        }
        h ^= cokme ? 1u : 0u;
        h *= 16777619u;
    }
    return h;
}

static void test_ayri_kosular(void)
{
    const uint32_t a = kosu_ozeti();
    const uint32_t b = kosu_ozeti();
    TEST_ASSERT_EQUAL_HEX32(a, b);
}

/* Çekirdekle bütünleşik: silahlı yapılsa da kestirim INIT ve istek KAPALI iken thr_cmd sıfır. */
static void test_cekirdekte_adim(void)
{
    TEST_ASSERT_EQUAL(IDA_RC_OK, ds_kur(&C));
    C.emn.armed = true;      /* yer tutucu görevde armed'a giden yol yok; test kurar */
    C.emn.kisitli = 0u;
    for (uint32_t n = 0u; n < 50u; n++) {
        const int64_t t = (2000 + (5 * (int64_t)n)) * DS_MS;
        ds_girdi(&IN, t, n + 1u, true);
        const ida_rc_t rc = ida_core_step(&C, &IN, &OUT);
        TEST_ASSERT_EQUAL(IDA_RC_OK, rc);
        for (uint32_t i = 0u; i < IDA_PORT_N_THR; i++) {
            TEST_ASSERT_TRUE(bitler(OUT.thr_cmd[i]) == 0u);
        }
        TEST_ASSERT_TRUE(OUT.health.core_ok);
        TEST_ASSERT_TRUE(ida_kon_tutarli(&C.kon));
    }
}

/* W3-K1, W3-K4: acil durdurmanın etkin değeri itki kapısında yer almaz. Çekirdekten toplanan
   girdide acil durdurma etkinken kısıt yoktur; öteki koşullar tutuyorsa itki üretilir. */
static void test_acil_durdurma_itkiyi_kesmez(void)
{
    TEST_ASSERT_EQUAL(IDA_RC_OK, ds_kur(&C));
    C.emn.armed = true;
    C.emn.kisitli = (uint8_t)IDA_EMN_TUMU; /* acil durdurma dahil bütün alanlar kısıtlı */
    TEST_ASSERT_TRUE((C.emn.kisitli & (uint8_t)IDA_EMN_ESTOP) != 0u);
    ida_kon_girdi_t g;
    ida_kon_girdi_topla(&C, &g);
    TEST_ASSERT_TRUE(g.armed);
    TEST_ASSERT_FALSE(g.kisit);
    /* Yer tutucu kestirim INIT ve istek KAPALI verdiği için bu iki koşul elle sağlanır;
       acil durdurmadan gelen bir kısıt olsaydı kisit alanında görünürdü. */
    int64_t t = 1000 * DS_MS;
    for (uint32_t i = 0u; i < 40u; i++) {
        t += 5 * DS_MS;
        ida_kon_girdi_t e;
        etkin_girdi(&e, t);
        e.armed = g.armed;
        e.kisit = g.kisit;
        ida_kon_hesapla(&K1, &P, &e);
    }
    TEST_ASSERT_TRUE(sifirdan_farkli_var(&K1));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_uc_durumda_tam_sifir);
    RUN_TEST(test_sayisal_cokme);
    RUN_TEST(test_devre_disi_itici_sifir);
    RUN_TEST(test_rastgele_dizi);
    RUN_TEST(test_ayri_kosular);
    RUN_TEST(test_cekirdekte_adim);
    RUN_TEST(test_acil_durdurma_itkiyi_kesmez);
    return UNITY_END();
}
