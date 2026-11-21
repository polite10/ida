/*
 * test_tahsis.c — itki tahsisi ve eğrinin tersi (W3-kontrol kabul md. 1, 2; kapsam md. 2).
 *
 * Beklenen desenler İDA-0 geometrisinden (scenarios/ida0.toml; varsayılan parametreler) elle
 * çıkarılmıştır: yatay iticiler 45°'lik vektörlü dörtlü, dikeyler dört köşede.
 */
#include <stdbool.h>
#include <stdint.h>

#include "destek.h"
#include "kontrol/tahsis.h"
#include "param/param.h"
#include "unity.h"

void setUp(void);
void tearDown(void);

static ida_prm_kume_t P;
static ida_tah_t T;

void setUp(void)
{
    ida_prm_varsayilan(&P);
    TEST_ASSERT_EQUAL_UINT8(IDA_TAH_TAMAM, ida_tah_kur(&T, &P, 0u));
}
void tearDown(void) {}

typedef union {
    uint32_t u;
    float f;
} bit32_t;

static void prm_f(ida_prm_kume_t *k, enum ida_prm_indis i, float v)
{
    bit32_t b;
    b.f = v;
    k->ham[i] = b.u;
}

static float mutlak(float x)
{
    return (x < 0.0f) ? -x : x;
}

#define YAKIN(bek, gercek, tol) TEST_ASSERT_FLOAT_WITHIN((tol), (bek), (gercek))

/* Statik pencerelerle tahsis (değişim hızı penceresi yok). */
static void dagit(const ida_tah_t *t, uint8_t rejim, const float tau[6], float f[8], float ta[6])
{
    float alt[8];
    float ust[8];
    ida_tah_sinirlar(t, alt, ust);
    for (uint32_t i = 0u; i < 8u; i++) {
        f[i] = 0.0f;
    }
    ida_tah_dagit(t, rejim, tau, alt, ust, f, ta);
}

static void tek_eksen(uint32_t eksen, float deger, float f[8], float tau_a[6])
{
    float tau[6] = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
    tau[eksen] = deger;
    dagit(&T, IDA_TAH_DALIS, tau, f, tau_a);
    for (uint32_t j = 0u; j < 6u; j++) {
        YAKIN(tau[j], tau_a[j], 1.0e-3f * (1.0f + mutlak(deger)));
    }
}

/* Kabul 1: tek eksenli her istek beklenen deseni verir ve B f = τ (doyma yokken). */
static void test_tek_eksen_desenleri(void)
{
    float f[8];
    float ta[6];
    const float tol = 1.0e-3f;

    /* +X: dört yatay eşit ve pozitif; dikeyler M = 0,085 X'i dengeler (önde +, arkada −). */
    tek_eksen(IDA_EKS_X, 20.0f, f, ta);
    TEST_ASSERT_TRUE(f[0] > 1.0f);
    for (uint32_t i = 1u; i < 4u; i++) {
        YAKIN(f[0], f[i], tol);
    }
    YAKIN(f[4], f[5], tol);
    YAKIN(f[6], f[7], tol);
    YAKIN(f[4], -f[6], tol);
    TEST_ASSERT_TRUE(f[4] > 0.0f);

    /* +Y: t1 −, t2 +, t3 +, t4 − (eşit büyüklük); dikeyler K = −0,085 Y'yi dengeler. */
    tek_eksen(IDA_EKS_Y, 20.0f, f, ta);
    TEST_ASSERT_TRUE(f[0] < -1.0f);
    YAKIN(-f[0], f[1], tol);
    YAKIN(f[1], f[2], tol);
    YAKIN(f[0], f[3], tol);
    YAKIN(f[4], f[6], tol);
    YAKIN(f[5], f[7], tol);
    YAKIN(f[4], -f[5], tol);
    TEST_ASSERT_TRUE(f[4] > 0.0f); /* sancak dikeyleri +z: K > 0 ile −0,085 Y dengelenir */

    /* +Z: yalnız dikeyler, eşit ve pozitif. */
    tek_eksen(IDA_EKS_Z, 40.0f, f, ta);
    for (uint32_t i = 0u; i < 4u; i++) {
        YAKIN(0.0f, f[i], tol);
    }
    for (uint32_t i = 5u; i < 8u; i++) {
        YAKIN(f[4], f[i], tol);
    }
    YAKIN(10.0f, f[4], tol);

    /* −Z: aynı desen, ters yön. */
    tek_eksen(IDA_EKS_Z, -40.0f, f, ta);
    YAKIN(-10.0f, f[7], tol);

    /* +K: sancak dikeyleri (t5, t7) +, iskele (t6, t8) −; yataylar 0. */
    tek_eksen(IDA_EKS_K, 3.0f, f, ta);
    for (uint32_t i = 0u; i < 4u; i++) {
        YAKIN(0.0f, f[i], tol);
    }
    TEST_ASSERT_TRUE(f[4] > 0.5f);
    YAKIN(f[4], f[6], tol);
    YAKIN(-f[4], f[5], tol);
    YAKIN(f[5], f[7], tol);

    /* +M: ön dikeyler (t5, t6) −, arka (t7, t8) +; yataylar 0. */
    tek_eksen(IDA_EKS_M, 3.0f, f, ta);
    for (uint32_t i = 0u; i < 4u; i++) {
        YAKIN(0.0f, f[i], tol);
    }
    TEST_ASSERT_TRUE(f[4] < -0.5f);
    YAKIN(f[4], f[5], tol);
    YAKIN(-f[4], f[6], tol);
    YAKIN(f[6], f[7], tol);

    /* +N: t1 −, t2 +, t3 −, t4 + (eşit büyüklük); dikeyler 0. */
    tek_eksen(IDA_EKS_N, 3.0f, f, ta);
    TEST_ASSERT_TRUE(f[0] < -0.5f);
    YAKIN(-f[0], f[1], tol);
    YAKIN(f[0], f[2], tol);
    YAKIN(f[1], f[3], tol);
    for (uint32_t i = 4u; i < 8u; i++) {
        YAKIN(0.0f, f[i], tol);
    }
}

/* Karışık istek doyma yokken tam elde edilir; yüzey rejiminde dikeyler tam 0. */
static void test_karisik_ve_yuzey(void)
{
    float f[8];
    float ta[6];
    const float tau[6] = {12.0f, -7.0f, 15.0f, 1.0f, -0.8f, 2.0f};
    dagit(&T, IDA_TAH_DALIS, tau, f, ta);
    for (uint32_t j = 0u; j < 6u; j++) {
        YAKIN(tau[j], ta[j], 1.0e-3f);
    }
    dagit(&T, IDA_TAH_YUZEY, tau, f, ta);
    for (uint32_t i = 4u; i < 8u; i++) {
        TEST_ASSERT_TRUE(f[i] == 0.0f);
    }
    YAKIN(tau[IDA_EKS_X], ta[IDA_EKS_X], 1.0e-3f);
    YAKIN(tau[IDA_EKS_Y], ta[IDA_EKS_Y], 1.0e-3f);
    YAKIN(tau[IDA_EKS_N], ta[IDA_EKS_N], 1.0e-3f);
}

static bool sinirda(const float f[8])
{
    for (uint32_t i = 0u; i < 8u; i++) {
        if (!(f[i] <= T.f_ileri) || !(f[i] >= -T.f_geri)) {
            return false;
        }
    }
    return true;
}

/* Kabul 2: aşırı istekte çıktı sınır içinde ve [−1, 1]'de; öncelik sırası korunur. */
static void test_doyma_ve_oncelik(void)
{
    float f[8];
    float ta[6];
    const float asiri[6] = {1.0e4f, -1.0e4f, 1.0e4f, 1.0e3f, -1.0e3f, 1.0e3f};
    dagit(&T, IDA_TAH_DALIS, asiri, f, ta);
    TEST_ASSERT_TRUE(sinirda(f));
    for (uint32_t i = 0u; i < 8u; i++) {
        const float u = ida_tah_komut(&T, f[i]);
        TEST_ASSERT_TRUE((u >= -1.0f) && (u <= 1.0f));
    }

    /* Öncelik: [K, M] > [Z] > [N] > [X, Y]. Alt öncelikli grupların eklenmesi üst gruplarda
       elde edileni değiştirmez; istek toplamda karşılanamaz (kalıntı var). */
    const float tam[6] = {150.0f, 90.0f, 160.0f, 12.0f, -6.0f, 20.0f};
    float f_tam[8];
    float ta_tam[6];
    dagit(&T, IDA_TAH_DALIS, tam, f_tam, ta_tam);
    TEST_ASSERT_TRUE(sinirda(f_tam));
    float kalinti = 0.0f;
    for (uint32_t j = 0u; j < 6u; j++) {
        kalinti += mutlak(tam[j] - ta_tam[j]);
    }
    TEST_ASSERT_TRUE(kalinti > 1.0f);

    float g1[6] = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
    g1[IDA_EKS_K] = tam[IDA_EKS_K];
    g1[IDA_EKS_M] = tam[IDA_EKS_M];
    float g12[6] = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
    g12[IDA_EKS_Z] = tam[IDA_EKS_Z];
    g12[IDA_EKS_K] = tam[IDA_EKS_K];
    g12[IDA_EKS_M] = tam[IDA_EKS_M];
    float g123[6] = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
    g123[IDA_EKS_Z] = tam[IDA_EKS_Z];
    g123[IDA_EKS_K] = tam[IDA_EKS_K];
    g123[IDA_EKS_M] = tam[IDA_EKS_M];
    g123[IDA_EKS_N] = tam[IDA_EKS_N];
    float fg[8];
    float tag[6];
    dagit(&T, IDA_TAH_DALIS, g1, fg, tag);
    YAKIN(tag[IDA_EKS_K], ta_tam[IDA_EKS_K], 1.0e-3f);
    YAKIN(tag[IDA_EKS_M], ta_tam[IDA_EKS_M], 1.0e-3f);
    YAKIN(tam[IDA_EKS_K], ta_tam[IDA_EKS_K], 1.0e-3f); /* K, M tek başına karşılanabiliyor */
    YAKIN(tam[IDA_EKS_M], ta_tam[IDA_EKS_M], 1.0e-3f);
    dagit(&T, IDA_TAH_DALIS, g12, fg, tag);
    YAKIN(tag[IDA_EKS_Z], ta_tam[IDA_EKS_Z], 1.0e-2f);
    dagit(&T, IDA_TAH_DALIS, g123, fg, tag);
    YAKIN(tag[IDA_EKS_N], ta_tam[IDA_EKS_N], 1.0e-3f);

    /* Grup içinde yön korunur: elde edilen (X, Y) istenene paraleldir. */
    const float capraz = (ta_tam[IDA_EKS_X] * tam[IDA_EKS_Y]) - (ta_tam[IDA_EKS_Y] * tam[IDA_EKS_X]);
    YAKIN(0.0f, capraz, 1.0e-1f);

    /* Yatay aşırı istek dikey öncelikli eksenleri bozmaz. */
    const float yatay_asiri[6] = {1000.0f, 1000.0f, 0.0f, 5.0f, 5.0f, 0.0f};
    dagit(&T, IDA_TAH_DALIS, yatay_asiri, f, ta);
    YAKIN(5.0f, ta[IDA_EKS_K], 1.0e-3f);
    YAKIN(5.0f, ta[IDA_EKS_M], 1.0e-3f);
    YAKIN(0.0f, ta[IDA_EKS_Z], 1.0e-3f);
}

/* Rastgele aşırı istekler (sabit tohum): her zaman sınır içinde, sonlu. */
static void test_doyma_rastgele(void)
{
    ds_rastgele_t r = {0x1234567u};
    float f[8];
    float ta[6];
    for (uint32_t n = 0u; n < 5000u; n++) {
        float tau[6];
        for (uint32_t j = 0u; j < 6u; j++) {
            const float olcek = (j < 3u) ? 400.0f : 60.0f;
            tau[j] = ((float)(ds_rastgele(&r) % 20001u) / 10000.0f - 1.0f) * olcek;
        }
        const uint8_t rejim = (uint8_t)(((ds_rastgele(&r) & 1u) != 0u) ? IDA_TAH_DALIS : IDA_TAH_YUZEY);
        dagit(&T, rejim, tau, f, ta);
        TEST_ASSERT_TRUE(sinirda(f));
        for (uint32_t i = 0u; i < 8u; i++) {
            const float u = ida_tah_komut(&T, f[i]);
            TEST_ASSERT_TRUE((u >= -1.0f) && (u <= 1.0f));
        }
    }
}

/* Kapsam 2: bir itici devre dışıyken yeniden tahsis; çıktısı tam 0, istek yine karşılanır. */
static void test_devre_disi_itici(void)
{
    ida_tah_t t5;
    TEST_ASSERT_EQUAL_UINT8(IDA_TAH_TAMAM, ida_tah_kur(&t5, &P, (uint8_t)(1u << 4)));
    float f[8];
    float ta[6];
    const float tau[6] = {0.0f, 0.0f, 30.0f, 1.0f, 1.0f, 0.0f};
    dagit(&t5, IDA_TAH_DALIS, tau, f, ta);
    TEST_ASSERT_TRUE(f[4] == 0.0f);
    for (uint32_t j = 0u; j < 6u; j++) {
        YAKIN(tau[j], ta[j], 1.0e-2f);
    }
    /* Bir yatay devre dışı: X, Y, N yine üretilir (kalan üç yatay + dikeyler). */
    ida_tah_t t1;
    TEST_ASSERT_EQUAL_UINT8(IDA_TAH_TAMAM, ida_tah_kur(&t1, &P, 1u));
    const float tau2[6] = {10.0f, 5.0f, 0.0f, 0.0f, 0.0f, 1.0f};
    dagit(&t1, IDA_TAH_DALIS, tau2, f, ta);
    TEST_ASSERT_TRUE(f[0] == 0.0f);
    for (uint32_t j = 0u; j < 6u; j++) {
        YAKIN(tau2[j], ta[j], 1.0e-2f);
    }
    /* İki aynı taraf dikeyi kapalı: K üretilemez; çıktı yine sınır içinde ve sonlu. */
    ida_tah_t t57;
    TEST_ASSERT_EQUAL_UINT8(IDA_TAH_TAMAM, ida_tah_kur(&t57, &P, (uint8_t)((1u << 4) | (1u << 6))));
    const float tau3[6] = {0.0f, 0.0f, 10.0f, 4.0f, 0.0f, 0.0f};
    dagit(&t57, IDA_TAH_DALIS, tau3, f, ta);
    TEST_ASSERT_TRUE(f[4] == 0.0f);
    TEST_ASSERT_TRUE(f[6] == 0.0f);
    for (uint32_t i = 0u; i < 8u; i++) {
        TEST_ASSERT_TRUE((f[i] <= t57.f_ileri) && (f[i] >= -t57.f_geri));
    }
}

/* Eğrinin tersi (A4-K54; A5-K104): ileri modelle tutarlı, ölü bant kuralı, işaret. */
static float sekil_ileri(float u)
{
    /* Bağımsız ileri model: 1001 noktada normalleştirme, monoton kırpma. */
    float c[5];
    for (uint32_t j = 0u; j < 5u; j++) {
        bit32_t b;
        b.u = P.ham[(uint32_t)IDA_PRM_kon_sekil_c0 + j];
        c[j] = b.f;
    }
    float n = 0.0f;
    float m = 0.0f;
    for (uint32_t j = 1u; j <= 1000u; j++) {
        const float v = (float)j / 1000.0f;
        const float fv = ida_tah_polinom(c, v);
        if (fv > n) {
            n = fv;
        }
        if ((v <= u) && (fv > m)) {
            m = fv;
        }
    }
    const float fu = ida_tah_polinom(c, u);
    if (fu > m) {
        m = fu;
    }
    return m / n;
}

static void test_egri_tersi(void)
{
    TEST_ASSERT_TRUE(ida_tah_komut(&T, 0.0f) == 0.0f);
    /* Tam itki: şekil 1'e ilk ulaştığı yerin yakınında, en çok 1. */
    const float u_tam = ida_tah_komut(&T, T.f_ileri);
    TEST_ASSERT_TRUE((u_tam > 0.95f) && (u_tam <= 1.0f));
    const float u_geri = ida_tah_komut(&T, -T.f_geri);
    TEST_ASSERT_TRUE((u_geri < -0.95f) && (u_geri >= -1.0f));
    TEST_ASSERT_TRUE(ida_tah_komut(&T, 10.0f * T.f_ileri) <= 1.0f);

    /* İleri modelle tutarlılık: u = ters(s) için F_şekil(u) ≈ s. */
    for (uint32_t j = 1u; j < 100u; j++) {
        const float s = (float)j / 100.0f;
        if (s < T.s_min) {
            continue;
        }
        const float u = ida_tah_komut(&T, s * T.f_ileri);
        YAKIN(s, sekil_ileri(u), 0.02f);
        const float ug = ida_tah_komut(&T, -s * T.f_geri);
        YAKIN(-u, ug, 1.0e-6f);
    }

    /* Ölü bant: F_şekil(u_min)'in yarısından küçük istek 0, büyüğü en az u_min. */
    const float s_esik = 0.5f * T.s_min;
    TEST_ASSERT_TRUE(ida_tah_komut(&T, 0.9f * s_esik * T.f_ileri) == 0.0f);
    YAKIN(T.u_min, ida_tah_komut(&T, 1.1f * s_esik * T.f_ileri), 1.0e-6f);
    YAKIN(-T.u_min, ida_tah_komut(&T, -1.1f * s_esik * T.f_geri), 1.0e-6f);
    YAKIN(0.065f, T.u_min, 1.0e-7f);
}

/* Parametre grubu denetimi (A4-K80 md. 8; A5-K25, K32, K104). */
static void test_param_denetimi(void)
{
    TEST_ASSERT_EQUAL_UINT8(IDA_TAH_TAMAM, ida_tah_param_denetle(&P));
    ida_prm_kume_t k = P;
    prm_f(&k, IDA_PRM_kon_t1_ex, 0.5f); /* ε birim değil */
    TEST_ASSERT_EQUAL_UINT8(IDA_TAH_YON_BIRIM, ida_tah_param_denetle(&k));
    k = P;
    /* Bütün dikeyler aynı noktada: K ve M üretilemez (rank < 6). */
    for (uint32_t i = 4u; i < 8u; i++) {
        prm_f(&k, (enum ida_prm_indis)((uint32_t)IDA_PRM_kon_t1_rx + (6u * i)), 0.0f);
        prm_f(&k, (enum ida_prm_indis)((uint32_t)IDA_PRM_kon_t1_ry + (6u * i)), 0.0f);
    }
    TEST_ASSERT_EQUAL_UINT8(IDA_TAH_RANK, ida_tah_param_denetle(&k));
    k = P;
    prm_f(&k, IDA_PRM_kon_sekil_c0, -100.0f); /* küçük u'da F ≤ 0 (A5-K104) */
    prm_f(&k, IDA_PRM_kon_sekil_c1, 0.0f);
    prm_f(&k, IDA_PRM_kon_sekil_c2, 0.0f);
    prm_f(&k, IDA_PRM_kon_sekil_c3, 0.0f);
    prm_f(&k, IDA_PRM_kon_sekil_c4, 0.0f);
    TEST_ASSERT_EQUAL_UINT8(IDA_TAH_SEKIL, ida_tah_param_denetle(&k));
    /* Varsayılan küme bütün çekirdek grup denetiminden geçer. */
    TEST_ASSERT_EQUAL_UINT8(0u, ida_prm_grup(&P));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_tek_eksen_desenleri);
    RUN_TEST(test_karisik_ve_yuzey);
    RUN_TEST(test_doyma_ve_oncelik);
    RUN_TEST(test_doyma_rastgele);
    RUN_TEST(test_devre_disi_itici);
    RUN_TEST(test_egri_tersi);
    RUN_TEST(test_param_denetimi);
    return UNITY_END();
}
