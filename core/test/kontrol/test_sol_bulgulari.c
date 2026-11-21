/*
 * test_sol_bulgulari.c — W3-kontrol sol incelemesinin karşı örnekleri
 * (docs/gorevler/inceleme/W3-kontrol-sol.md, bulgu 1–5; bulgu 6 uygulanmadı, gerekçe testi).
 *
 * Her test sol'un verdiği sayısal örneği ya da aynı sınıftan bir örneği sınar. Düzeltme öncesi
 * kodda kaldıkları, aynı dosyanın eski API'ye (ESKI_API tanımlı) derlenip eski çekirdek
 * arşiviyle bağlanmasıyla gösterildi (W3-kontrol düzeltme turu raporu); ürün derlemesinde
 * ESKI_API tanımlı değildir.
 */
#include <float.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "destek.h"
#include "kontrol/kontrol.h"
#include "kontrol/tahsis.h"
#include "mat/ida_math.h"
#include "param/param.h"
#include "unity.h"

void setUp(void);
void tearDown(void);

static ida_prm_kume_t P;
static ida_kon_t K;

void setUp(void)
{
    ida_prm_varsayilan(&P);
    ida_kon_kur(&K);
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

/* Statik pencereli, sıfırdan başlayan tahsis (eski ve yeni API). */
static void dagit(const ida_tah_t *t, uint8_t rejim, const float tau[6], float f[8], float ta[6])
{
#ifdef ESKI_API
    ida_tah_dagit(t, rejim, tau, f, ta);
#else
    float alt[8];
    float ust[8];
    ida_tah_sinirlar(t, alt, ust);
    for (uint32_t i = 0u; i < 8u; i++) {
        f[i] = 0.0f;
    }
    ida_tah_dagit(t, rejim, tau, alt, ust, f, ta);
#endif
}

/* Düz duruşlu, durgun kestirim; istek verilen. */
static void girdi(ida_kon_girdi_t *g, int64_t t_ns, bool armed)
{
    (void)memset(g, 0, sizeof(*g));
    g->nav.t.ns = t_ns;
    g->nav.q_nb[0] = 1.0f;
    g->nav.depth = 2.0f;
    g->nav.est_level = (uint8_t)IDA_EST_OK;
    g->t_ns = t_ns;
    g->armed = armed;
    g->kisit = false;
    g->kestirim_yeterli = true;
    g->istek.kip = IDA_KON_KIP_DALIS;
    g->istek.dikey = IDA_KON_DIKEY_HIZ;
    g->istek.tutum = IDA_KON_TUTUM_ACI;
    g->istek.yon_kanali = IDA_KON_YON_ACI;
    g->istek.yatay = IDA_KON_YATAY_HIZ;
}

/* İlk çağrı zamanı kurar (silahsız), ikincisi 5 ms sonra etkin adımdır. */
static void iki_adim(ida_kon_girdi_t *g)
{
    ida_kon_girdi_t ilk = *g;
    ilk.armed = false;
    ilk.t_ns = g->t_ns - (5 * DS_MS);
    ida_kon_hesapla(&K, &P, &ilk);
    ida_kon_hesapla(&K, &P, g);
}

static bool hepsi_sifir_ve_cokme(void)
{
    float thr[IDA_PORT_N_THR];
    bool cokme = false;
    ida_kon_itki(&K, thr, &cokme);
    bool s = cokme;
    for (uint32_t i = 0u; i < IDA_PORT_N_THR; i++) {
        bit32_t b;
        b.f = thr[i];
        s = s && (b.u == 0u);
    }
    return s;
}

/* Bulgu 1 (EMNİYET): sonlu ama uç girdiden doğan taşma kırpmayla gizlenmez; çıktı sıfır ve
   çökme bildirilir. Sol'un örneği v_ned[2] = −FLT_MAX (dikey hız isteği). */
static void test_b1_sonlu_girdiden_tasma(void)
{
    ida_kon_girdi_t g;
    for (uint32_t durum = 0u; durum < 4u; durum++) {
        ida_kon_kur(&K);
        girdi(&g, 1000 * DS_MS, true);
        switch (durum) {
        case 0u:
            g.nav.v_ned[2] = -FLT_MAX; /* sol: ham_w = +∞, eski kod 80 N'a kırpıp itki verdi */
            break;
        case 1u:
            g.nav.v_ned[0] = FLT_MAX;  /* yatay hız PI */
            break;
        case 2u:
            g.nav.omega_b[2] = -FLT_MAX; /* açısal hız PI */
            break;
        default:
            g.istek.dikey = IDA_KON_DIKEY_DERINLIK; /* derinlik dış döngüsü */
            g.istek.derinlik = 3.0f;
            g.nav.v_ned[2] = FLT_MAX;
            break;
        }
        iki_adim(&g);
        TEST_ASSERT_TRUE_MESSAGE(hepsi_sifir_ve_cokme(), "taşma: çıktı sıfır ve çökme olmalı");
    }
}

/* Bulgu 2: değişim hızı penceresi önceliği bozmaz. Önceki itkiler sıfır, 5 ms adım (pencere
   ±1,25 N): X = 40 N, M = +1 N·m isteğinde elde edilen M yalnız M istendiğindekiyle aynı ve
   pozitif (eski kod: −0,29948 / +0,6). Kabul ida_kon_hesapla çıktısına kadar. */
static float m_elde(float x)
{
    ida_kon_kur(&K);
    ida_kon_girdi_t g;
    girdi(&g, 1000 * DS_MS, true);
    g.istek.dikey = IDA_KON_DIKEY_KUVVET;
    g.istek.tutum = IDA_KON_TUTUM_MOMENT;
    g.istek.yon_kanali = IDA_KON_YON_MOMENT;
    g.istek.yatay = IDA_KON_YATAY_KUVVET;
    g.istek.kuvvet[IDA_EKS_X] = x;
    g.istek.kuvvet[IDA_EKS_M] = 1.0f;
    iki_adim(&g);
    float ta[6];
    ida_tah_uret(&K.tah, K.f, ta);
    TEST_ASSERT_EQUAL_UINT8(0u, K.cokme);
    return ta[IDA_EKS_M];
}

static void test_b2_degisim_hizi_onceligi(void)
{
    const float yalniz_m = m_elde(0.0f);
    const float x_ve_m = m_elde(40.0f);
    TEST_ASSERT_TRUE(yalniz_m > 0.5f);
    TEST_ASSERT_TRUE_MESSAGE(x_ve_m > 0.0f, "alt öncelikli X üst öncelikli M'nin işaretini çevirdi");
    TEST_ASSERT_FLOAT_WITHIN(1.0e-3f, yalniz_m, x_ve_m);
}

/* Bulgu 3: rank kaybında öncelik. T5 ve T7 kapalı: K = +4 isteğine alt öncelikli Z = +10
   eklemek elde edilen K'yı değiştirmez ve işaretini çevirmez (eski kod: +0,208 → −1,859). Aynı
   sınama ön dikeyler (T5, T6) kapalıyken M için. */
static void oncelik_maskeyle(uint8_t disi, uint32_t ust_eksen, float ust_deger)
{
    ida_tah_t t;
    TEST_ASSERT_EQUAL_UINT8(IDA_TAH_TAMAM, ida_tah_kur(&t, &P, disi));
    float f[8];
    float ta1[6];
    float ta2[6];
    float tau[6] = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
    tau[ust_eksen] = ust_deger;
    dagit(&t, IDA_TAH_DALIS, tau, f, ta1);
    tau[IDA_EKS_Z] = 10.0f;
    dagit(&t, IDA_TAH_DALIS, tau, f, ta2);
    TEST_ASSERT_TRUE_MESSAGE(ta1[ust_eksen] * ust_deger > 0.0f, "üst eksen işareti");
    TEST_ASSERT_TRUE_MESSAGE(ta2[ust_eksen] * ust_deger > 0.0f, "Z eklenince üst eksen işareti");
    TEST_ASSERT_FLOAT_WITHIN(1.0e-3f * (1.0f + mutlak(ust_deger)), ta1[ust_eksen], ta2[ust_eksen]);
    for (uint32_t i = 0u; i < 8u; i++) {
        if ((disi & (1u << i)) != 0u) {
            TEST_ASSERT_TRUE(f[i] == 0.0f);
        }
    }
}

static void test_b3_rank_kaybinda_oncelik(void)
{
    oncelik_maskeyle((uint8_t)((1u << 4) | (1u << 6)), IDA_EKS_K, 4.0f);
    oncelik_maskeyle((uint8_t)((1u << 4) | (1u << 6)), IDA_EKS_K, -4.0f);
    oncelik_maskeyle((uint8_t)((1u << 4) | (1u << 5)), IDA_EKS_M, 2.0f);
    oncelik_maskeyle((uint8_t)((1u << 4) | (1u << 5)), IDA_EKS_M, -2.0f);
}

/* Bulgu 4: karma açı/hız kanalları sıfırdan farklı yalpa altında. Sol'un örneği: φ = 0,5,
   θ = 0, istenen gövde p = q = 0 (TUTUM_HIZ), dış döngü ψ̇ = 0,35 (YON_ACI) → r ≈ 0,398823
   (eski kod 0,307154). İkinci vaka: TUTUM_ACI (φ tutulur), YON_HIZ r = 0,3 → q = r·tan φ. */
static void karma_kur(ida_kon_girdi_t *g)
{
    prm_f(&P, IDA_PRM_kon_itki_egim, 100000.0f); /* tek adımda tam tahsis */
    prm_f(&P, IDA_PRM_kon_yon_kp, 0.0f);        /* ψ̇ = yalnız referans ileri beslemesi */
    girdi(g, 1000 * DS_MS, true);
    g->nav.q_nb[0] = ida_cos(0.25f); /* φ = 0,5 */
    g->nav.q_nb[1] = ida_sin(0.25f);
    g->istek.dikey = IDA_KON_DIKEY_KUVVET; /* dikey kuvvet 0: yalnız açısal sınansın */
    g->istek.yatay = IDA_KON_YATAY_KUVVET;
}

static void test_b4_karma_kinematik(void)
{
    ida_kon_girdi_t g;
    karma_kur(&g);
    g.istek.tutum = IDA_KON_TUTUM_HIZ;
    g.istek.yon_kanali = IDA_KON_YON_ACI;
    g.istek.yon = 1.0f; /* referans 0,35 rad/s ile ilerler: ψ̇ = 0,35 */
    iki_adim(&g);
    TEST_ASSERT_EQUAL_UINT8(0u, K.cokme);
    float ta[6];
    ida_tah_uret(&K.tah, K.f, ta);
    /* İlk etkin adımda integraller 0, ölçülen ω 0: moment = Kp · ω_sp. */
    const float r_sp = ta[IDA_EKS_N] / 4.0f;  /* RATE_KP_R */
    TEST_ASSERT_FLOAT_WITHIN(2.0e-3f, 0.398823f, r_sp);
    TEST_ASSERT_FLOAT_WITHIN(2.0e-3f, 0.0f, ta[IDA_EKS_K] / 3.0f); /* p_sp = 0 */
    TEST_ASSERT_FLOAT_WITHIN(2.0e-3f, 0.0f, ta[IDA_EKS_M] / 3.0f); /* q_sp = 0 */

    ida_kon_kur(&K);
    karma_kur(&g);
    g.istek.tutum = IDA_KON_TUTUM_ACI;
    g.istek.yalpa = 0.5f; /* φ tutulur: φ̇ = 0, θ̇ = 0 */
    g.istek.yon_kanali = IDA_KON_YON_HIZ;
    g.istek.yon_hizi = 0.3f;
    iki_adim(&g);
    TEST_ASSERT_EQUAL_UINT8(0u, K.cokme);
    ida_tah_uret(&K.tah, K.f, ta);
    TEST_ASSERT_FLOAT_WITHIN(2.0e-3f, 0.3f, ta[IDA_EKS_N] / 4.0f);
    TEST_ASSERT_FLOAT_WITHIN(2.0e-3f, 0.3f * 0.5463025f, ta[IDA_EKS_M] / 3.0f); /* q = r tan φ */
    TEST_ASSERT_FLOAT_WITHIN(2.0e-3f, 0.0f, ta[IDA_EKS_K] / 3.0f);
}

/* Bulgu 5: koşul sayısı. Sol'un örneği: T5/T6 rx = 2, T7/T8 rx = 1,996 → κ₂(B Bᵀ) ≈ 6,24·10⁶,
   tam ranklı ama kötü koşullu; reddedilir (eski Cholesky oranı 0,0019 ile kabul ediyordu). */
static void dikey_x(ida_prm_kume_t *k, float on, float arka)
{
    prm_f(k, IDA_PRM_kon_t5_rx, on);
    prm_f(k, IDA_PRM_kon_t6_rx, on);
    prm_f(k, IDA_PRM_kon_t7_rx, arka);
    prm_f(k, IDA_PRM_kon_t8_rx, arka);
}

static void test_b5_kosul_sayisi(void)
{
    ida_prm_kume_t k = P;
    dikey_x(&k, 2.0f, 1.996f);
    TEST_ASSERT_EQUAL_UINT8(IDA_TAH_RANK, ida_tah_param_denetle(&k));
    TEST_ASSERT_EQUAL_UINT8(IDA_TAH_TAMAM, ida_tah_param_denetle(&P));
}

#ifndef ESKI_API
/* Bağımsız κ₂ (binary64 Jacobi) ile eşik çevresi: κ ≤ 0,9·sınır kabul, ≥ 1,1·sınır ret. */
static double kosul_cift(const ida_prm_kume_t *k)
{
    double b[6][8];
    for (uint32_t i = 0u; i < 8u; i++) {
        double r[3];
        double e[3];
        for (uint32_t j = 0u; j < 3u; j++) {
            bit32_t x;
            x.u = k->ham[(uint32_t)IDA_PRM_kon_t1_rx + (6u * i) + j];
            r[j] = (double)x.f;
            x.u = k->ham[(uint32_t)IDA_PRM_kon_t1_ex + (6u * i) + j];
            e[j] = (double)x.f;
        }
        b[0][i] = e[0];
        b[1][i] = e[1];
        b[2][i] = e[2];
        b[3][i] = (r[1] * e[2]) - (r[2] * e[1]);
        b[4][i] = (r[2] * e[0]) - (r[0] * e[2]);
        b[5][i] = (r[0] * e[1]) - (r[1] * e[0]);
    }
    double a[6][6];
    for (uint32_t p = 0u; p < 6u; p++) {
        for (uint32_t q = 0u; q < 6u; q++) {
            double s = 0.0;
            for (uint32_t i = 0u; i < 8u; i++) {
                s += b[p][i] * b[q][i];
            }
            a[p][q] = s;
        }
    }
    for (uint32_t tur = 0u; tur < 100u; tur++) {
        for (uint32_t p = 0u; p < 6u; p++) {
            for (uint32_t q = p + 1u; q < 6u; q++) {
                if (a[p][q] == 0.0) {
                    continue;
                }
                const double teta = (a[q][q] - a[p][p]) / (2.0 * a[p][q]);
                const double mt = (teta < 0.0) ? -teta : teta;
                double t = 1.0 / (mt + sqrt((teta * teta) + 1.0));
                if (teta < 0.0) {
                    t = -t;
                }
                const double c = 1.0 / sqrt((t * t) + 1.0);
                const double s = t * c;
                const double apq = a[p][q];
                a[p][p] -= t * apq;
                a[q][q] += t * apq;
                a[p][q] = 0.0;
                a[q][p] = 0.0;
                for (uint32_t r = 0u; r < 6u; r++) {
                    if ((r != p) && (r != q)) {
                        const double arp = a[r][p];
                        const double arq = a[r][q];
                        a[r][p] = (c * arp) - (s * arq);
                        a[p][r] = a[r][p];
                        a[r][q] = (s * arp) + (c * arq);
                        a[q][r] = a[r][q];
                    }
                }
            }
        }
    }
    double enk = a[0][0];
    double enb = a[0][0];
    for (uint32_t i = 1u; i < 6u; i++) {
        enk = (a[i][i] < enk) ? a[i][i] : enk;
        enb = (a[i][i] > enb) ? a[i][i] : enb;
    }
    return enb / enk;
}

static void test_b5_esik_cevresi(void)
{
    const double sinir = (double)IDA_TAH_KOSUL_EN_BUYUK;
    const float araliklar[9] = {0.004f, 0.02f, 0.05f, 0.08f, 0.1f, 0.12f, 0.15f, 0.2f, 0.3f};
    uint32_t kabul = 0u;
    uint32_t ret = 0u;
    for (uint32_t j = 0u; j < 9u; j++) {
        ida_prm_kume_t k = P;
        dikey_x(&k, 2.0f, 2.0f - araliklar[j]);
        const double kc = kosul_cift(&k);
        const float kf = ida_tah_kosul_sayisi(&k, IDA_TAH_DALIS);
        TEST_ASSERT_TRUE_MESSAGE(((double)kf > (0.98 * kc)) && ((double)kf < (1.02 * kc)),
                                 "κ tahmini bağımsız hesaptan %2'den fazla sapıyor");
        const uint8_t sonuc = ida_tah_param_denetle(&k);
        if (kc <= (0.9 * sinir)) {
            TEST_ASSERT_EQUAL_UINT8(IDA_TAH_TAMAM, sonuc);
            kabul++;
        } else if (kc >= (1.1 * sinir)) {
            TEST_ASSERT_EQUAL_UINT8(IDA_TAH_RANK, sonuc);
            ret++;
        }
    }
    TEST_ASSERT_TRUE(kabul >= 2u); /* eşiğin iki yanında da örnek var */
    TEST_ASSERT_TRUE(ret >= 2u);
    /* Varsayılan İDA-0 geometrisi eşiğin çok altında. */
    TEST_ASSERT_TRUE(ida_tah_kosul_sayisi(&P, IDA_TAH_DALIS) < (0.01f * IDA_TAH_KOSUL_EN_BUYUK));
    TEST_ASSERT_TRUE(ida_tah_kosul_sayisi(&P, IDA_TAH_YUZEY) < (0.01f * IDA_TAH_KOSUL_EN_BUYUK));
}
#endif

/* Bulgu 6 (uygulanmadı; gerekçe raporda): ölü bant bir sınır doyması değildir. Dikey istek
   itici başına ölü bant eşiğinin (≈0,513 N) altında kaldığında (komut 0) hata dışarı doğru
   itiyorsa integral donmaz, büyüyerek eşiği geçer. Kalıntı ölü banttan sonraki itkiden
   hesaplansaydı integral donar, eşiği yalnız P terimi geçebilirdi (yüzey yön tutmasında
   0,0555 rad kalıcı hata ölçüldü). */
static void test_b6_olu_bantta_integral_donmaz(void)
{
    prm_f(&P, IDA_PRM_kon_itki_egim, 100000.0f);
    prm_f(&P, IDA_PRM_kon_yuzerlik_ff, 1.2f);
    ida_kon_girdi_t g;
    girdi(&g, 1000 * DS_MS, true);
    g.istek.dikey_hiz = 0.0f;
    g.nav.v_ned[2] = -0.01f; /* yükseliyor: e_w = +0,01 */
    iki_adim(&g);
    TEST_ASSERT_EQUAL_UINT8(0u, K.cokme);
    for (uint32_t i = 4u; i < 8u; i++) {
        TEST_ASSERT_TRUE(K.thr[i] == 0.0f); /* istek ölü bantta: 4 × 0,45 N */
    }
    TEST_ASSERT_TRUE(K.i_w > 0.0f); /* integral büyüyor */
    /* Integral tek başına eşiği geçer: ölçülen hız sabitken birkaç adım sonra itki oluşur. */
    int64_t t = g.t_ns;
    for (uint32_t n = 0u; n < 400u; n++) {
        t += 5 * DS_MS;
        g.t_ns = t;
        g.nav.t.ns = t;
        ida_kon_hesapla(&K, &P, &g);
    }
    TEST_ASSERT_TRUE(K.thr[4] > 0.0f);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_b1_sonlu_girdiden_tasma);
    RUN_TEST(test_b2_degisim_hizi_onceligi);
    RUN_TEST(test_b3_rank_kaybinda_oncelik);
    RUN_TEST(test_b4_karma_kinematik);
    RUN_TEST(test_b5_kosul_sayisi);
    RUN_TEST(test_b6_olu_bantta_integral_donmaz);
#ifndef ESKI_API
    RUN_TEST(test_b5_esik_cevresi);
#endif
    return UNITY_END();
}
