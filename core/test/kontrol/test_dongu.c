/*
 * test_dongu.c — kapalı döngü: basamak yanıtı ve sarma (W3-kontrol kabul md. 3, 4).
 *
 * Bitki: testin içinde yazılmış küçük bir model (sim/ kullanılmaz). Dinamik doğrusaldır
 * (sabit kütle + eklenmiş kütle, doğrusal sönüm, küçük açıda doğrusal geri getirme, sabit
 * net yüzerlik); yönelim kinematiği ve çerçeve dönüşümleri tamdır. İticiler: kontrolün
 * çıktısı (normalleştirilmiş ESC komutu) ölü bantlı ileri itki modelinden ve birinci derece
 * gecikmeden geçer, B ile genelleştirilmiş kuvvete çevrilir. Değerler İDA-0'ın A5 tanımına
 * yakın seçildi ama bilinçli olarak kontrolün parametrelerinden farklıdır (azami itki, net
 * yüzerlik): integral ve sağlamlık sınansın diye. Bu bir doğrulanmış araç modeli değildir.
 *
 * Kabul sınırları (testin ölçütü; ölçüm değil): derinlik 1 → 4 m basamağında aşma ≤ 0,12 m,
 * %2 bandına (0,06 m) oturma ≤ 20 s, son 5 s ortalama |hata| ≤ 5 mm; yön 0 → 90° basamağında
 * (0,3 N·m sabit bozucu moment) aşma ≤ 5°, 2°'lik banda oturma ≤ 8 s, son 5 s ortalama
 * |hata| ≤ 0,2°. Sarma: araç 30 s sıkışık tutulduktan (çıkış sınırda) sonra bırakıldığında
 * 3 m ± 0,1 m bandına dönüş ≤ 8 s, aşma ≤ 0,2 m; integral her an sınırı içinde. Sarma
 * sınırları koşullu integrali ayırt eder: yalnız kırpmalı integralle (koşullu dondurma
 * kapalı) aynı senaryoda aşma 0,38 m çıktı (geliştirme sırasında elle denendi).
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
void setUp(void) {}
void tearDown(void) {}

#define DT_SIM     0.001    /* s */
#define ALT_ADIM   5u       /* kontrol her 5 benzetim adımında: 200 Hz */
#define DT_SIM_NS  1000000LL

/* ---- Bitki ------------------------------------------------------------------------------ */

typedef struct {
    double pos[3];   /* NED, m */
    double eul[3];   /* φ, θ, ψ (rad) */
    double v[3];     /* gövde hızları u, v, w */
    double w[3];     /* gövde açısal hızları p, q, r */
    double f[8];     /* iticilerin anlık itkisi (gecikmeli), N */
    double bozucu_ned[3];  /* dış kuvvet, NED, N */
    double bozucu_mom[3];  /* dış moment, gövde, N·m */
    bool sikisik;          /* true: araç hareketsiz tutulur */
    double b[6][8];
    double sekil[10001];   /* ileri itki şekli, monoton kırpılmış ve normalleştirilmiş */
} bitki_t;

static bitki_t B_;
static ida_prm_kume_t P;
static ida_kon_t K;

static const double KUTLE[3] = {19.86, 20.62, 32.18};   /* m + eklenmiş kütle (İDA-0 A5) */
static const double SONUM[3] = {30.0, 35.0, 45.0};      /* doğrusallaştırılmış, N·s/m */
static const double ATALET[3] = {0.449, 0.365, 0.592};  /* I + eklenmiş, kg·m² */
static const double SONUM_D[3] = {0.6, 0.6, 0.5};       /* N·m·s/rad */
static const double GERI_GETIRME = 1.34;                 /* W·BG, N·m/rad (yalpa, baş-kıç) */
static const double NET_YUZERLIK = 3.0;                  /* B − W, N (kontrolün FF'i 1,82) */
static const double FMAX_ILERI = 47.0;                   /* kontrolün 49,94'ünden farklı */
static const double FMAX_GERI = 37.0;
static const double TAU_ITICI = 0.03;                    /* s, birinci derece gecikme */

static double d_abs(double x)
{
    return (x < 0.0) ? -x : x;
}

static double d_sin(double x)
{
    return (double)ida_sin((float)x);
}

static double d_cos(double x)
{
    return (double)ida_cos((float)x);
}

/* R (gövde → NED), ZYX Euler açılarından (sabit olmayan toplu ilkleyici yok: MSVC C4204). */
static void dcm(const double eul[3], double R[3][3])
{
    const double sf = d_sin(eul[0]);
    const double cf = d_cos(eul[0]);
    const double st = d_sin(eul[1]);
    const double ct = d_cos(eul[1]);
    const double ss = d_sin(eul[2]);
    const double cs = d_cos(eul[2]);
    R[0][0] = ct * cs;
    R[0][1] = (sf * st * cs) - (cf * ss);
    R[0][2] = (cf * st * cs) + (sf * ss);
    R[1][0] = ct * ss;
    R[1][1] = (sf * st * ss) + (cf * cs);
    R[1][2] = (cf * st * ss) - (sf * cs);
    R[2][0] = -st;
    R[2][1] = sf * ct;
    R[2][2] = cf * ct;
}

typedef union {
    uint32_t u;
    float f;
} bit32_t;

static float prm_oku(enum ida_prm_indis i)
{
    bit32_t b;
    b.u = P.ham[i];
    return b.f;
}

static void bitki_kur(void)
{
    (void)memset(&B_, 0, sizeof(B_));
    /* B, varsayılan geometriden (testin kendi hesabı). */
    for (uint32_t i = 0u; i < 8u; i++) {
        double r[3];
        double e[3];
        for (uint32_t j = 0u; j < 3u; j++) {
            r[j] = (double)prm_oku((enum ida_prm_indis)((uint32_t)IDA_PRM_kon_t1_rx + (6u * i) + j));
            e[j] = (double)prm_oku((enum ida_prm_indis)((uint32_t)IDA_PRM_kon_t1_ex + (6u * i) + j));
        }
        B_.b[0][i] = e[0];
        B_.b[1][i] = e[1];
        B_.b[2][i] = e[2];
        B_.b[3][i] = (r[1] * e[2]) - (r[2] * e[1]);
        B_.b[4][i] = (r[2] * e[0]) - (r[0] * e[2]);
        B_.b[5][i] = (r[0] * e[1]) - (r[1] * e[0]);
    }
    /* Şekil: ida0 polinomu, 10001 noktada önek en büyüğü / genel en büyük. */
    const double c[5] = {8.9, 176.0, -404.1, 389.9, -140.3};
    double enb = 0.0;
    double onek = 0.0;
    for (uint32_t j = 0u; j <= 10000u; j++) {
        const double u = (double)j / 10000.0;
        const double u2 = u * u;
        const double fv = u * (c[0] + (u2 * (c[1] + (u2 * (c[2] + (u2 * (c[3] + (u2 * c[4]))))))));
        if (fv > onek) {
            onek = fv;
        }
        B_.sekil[j] = onek;
        if (fv > enb) {
            enb = fv;
        }
    }
    for (uint32_t j = 0u; j <= 10000u; j++) {
        B_.sekil[j] /= enb;
    }
}

/* ESC: 1500 + 400u µs, ölü bant ±25 µs; PWM 1 µs nicemli (02 §2.5; 04 §2.7.1). */
static double itki(float u)
{
    const double pwm = 1500.0 + (double)(int32_t)((400.0 * (double)u) + ((u >= 0.0f) ? 0.5 : -0.5));
    const double un = (pwm - 1500.0) / 400.0;
    if (d_abs(pwm - 1500.0) <= 25.0) {
        return 0.0;
    }
    const double a = d_abs(un);
    uint32_t j = (uint32_t)(a * 10000.0);
    if (j > 10000u) {
        j = 10000u;
    }
    const double s = B_.sekil[j];
    return (un > 0.0) ? (s * FMAX_ILERI) : (-s * FMAX_GERI);
}

static void bitki_adim(const float thr[8])
{
    for (uint32_t i = 0u; i < 8u; i++) {
        const double hedef = itki(thr[i]);
        B_.f[i] += (hedef - B_.f[i]) * (DT_SIM / TAU_ITICI);
    }
    double tau[6] = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0};
    for (uint32_t r = 0u; r < 6u; r++) {
        for (uint32_t i = 0u; i < 8u; i++) {
            tau[r] += B_.b[r][i] * B_.f[i];
        }
    }
    const double sf = d_sin(B_.eul[0]);
    const double cf = d_cos(B_.eul[0]);
    const double st = d_sin(B_.eul[1]);
    const double ct = d_cos(B_.eul[1]);
    double R[3][3];
    dcm(B_.eul, R);
    /* NED dış kuvvet: net yüzerlik yukarı (−z) + bozucu. Gövdeye Rᵀ. */
    double fn[3];
    fn[0] = B_.bozucu_ned[0];
    fn[1] = B_.bozucu_ned[1];
    fn[2] = B_.bozucu_ned[2] - NET_YUZERLIK;
    for (uint32_t j = 0u; j < 3u; j++) {
        const double fb = (R[0][j] * fn[0]) + (R[1][j] * fn[1]) + (R[2][j] * fn[2]);
        tau[j] += fb;
    }
    tau[3] += -GERI_GETIRME * B_.eul[0] + B_.bozucu_mom[0];
    tau[4] += -GERI_GETIRME * B_.eul[1] + B_.bozucu_mom[1];
    tau[5] += B_.bozucu_mom[2];

    if (B_.sikisik) {
        for (uint32_t j = 0u; j < 3u; j++) {
            B_.v[j] = 0.0;
            B_.w[j] = 0.0;
        }
        return;
    }
    for (uint32_t j = 0u; j < 3u; j++) {
        B_.v[j] += ((tau[j] - (SONUM[j] * B_.v[j])) / KUTLE[j]) * DT_SIM;
        B_.w[j] += ((tau[3u + j] - (SONUM_D[j] * B_.w[j])) / ATALET[j]) * DT_SIM;
    }
    for (uint32_t j = 0u; j < 3u; j++) {
        B_.pos[j] += ((R[j][0] * B_.v[0]) + (R[j][1] * B_.v[1]) + (R[j][2] * B_.v[2])) * DT_SIM;
    }
    const double p = B_.w[0];
    const double q = B_.w[1];
    const double r = B_.w[2];
    B_.eul[0] += (p + (((q * sf) + (r * cf)) * (st / ct))) * DT_SIM;
    B_.eul[1] += ((q * cf) - (r * sf)) * DT_SIM;
    B_.eul[2] += (((q * sf) + (r * cf)) / ct) * DT_SIM;
    if (B_.eul[2] > 3.14159265358979) {
        B_.eul[2] -= 6.28318530717959;
    } else if (B_.eul[2] <= -3.14159265358979) {
        B_.eul[2] += 6.28318530717959;
    }
}

/* Bitkinin durumundan kusursuz kestirim (A4-K87 alanları). */
static void nav_doldur(ida_nav_out_t *n, int64_t t_ns)
{
    (void)memset(n, 0, sizeof(*n));
    n->t.ns = t_ns;
    const double hf = 0.5 * B_.eul[0];
    const double ht = 0.5 * B_.eul[1];
    const double hs = 0.5 * B_.eul[2];
    const double cf = d_cos(hf);
    const double sf = d_sin(hf);
    const double ct = d_cos(ht);
    const double st = d_sin(ht);
    const double cs = d_cos(hs);
    const double ss = d_sin(hs);
    n->q_nb[0] = (float)((cf * ct * cs) + (sf * st * ss));
    n->q_nb[1] = (float)((sf * ct * cs) - (cf * st * ss));
    n->q_nb[2] = (float)((cf * st * cs) + (sf * ct * ss));
    n->q_nb[3] = (float)((cf * ct * ss) - (sf * st * cs));
    double R[3][3];
    dcm(B_.eul, R);
    for (uint32_t j = 0u; j < 3u; j++) {
        n->p_ned[j] = (float)B_.pos[j];
        n->v_ned[j] = (float)((R[j][0] * B_.v[0]) + (R[j][1] * B_.v[1]) + (R[j][2] * B_.v[2]));
        n->omega_b[j] = (float)B_.w[j];
    }
    n->depth = (float)B_.pos[2];
    n->altitude = -1.0f;
    n->est_level = (uint8_t)IDA_EST_OK;
}

static void istek_dalis(ida_kon_istek_t *s, float derinlik, float yon)
{
    (void)memset(s, 0, sizeof(*s));
    s->kip = IDA_KON_KIP_DALIS;
    s->dikey = IDA_KON_DIKEY_DERINLIK;
    s->tutum = IDA_KON_TUTUM_ACI;
    s->yon_kanali = IDA_KON_YON_ACI;
    s->yatay = IDA_KON_YATAY_HIZ;
    s->derinlik = derinlik;
    s->yon = yon;
}

static int64_t g_t_ns;
static uint32_t g_n;
static float g_thr[8]; /* testin durumu (çekirdek değil): son kontrol çıktısı */

static void kur(double derinlik, double yon)
{
    ida_prm_varsayilan(&P);
    bitki_kur();
    ida_kon_kur(&K);
    for (uint32_t i = 0u; i < 8u; i++) {
        g_thr[i] = 0.0f;
    }
    B_.pos[2] = derinlik;
    B_.eul[2] = yon;
    g_t_ns = 1000000000LL;
    g_n = 0u;
}

/* Bir benzetim adımı; gerekiyorsa kontrol adımı. */
static void adim(const ida_kon_istek_t *s)
{
    float *thr = g_thr;
    if ((g_n % ALT_ADIM) == 0u) {
        ida_kon_girdi_t g;
        (void)memset(&g, 0, sizeof(g));
        g.istek = *s;
        nav_doldur(&g.nav, g_t_ns);
        g.t_ns = g_t_ns;
        g.armed = true;
        g.kisit = false;
        g.kestirim_yeterli = true;
        ida_kon_hesapla(&K, &P, &g);
        bool cokme = true;
        ida_kon_itki(&K, thr, &cokme);
        TEST_ASSERT_FALSE(cokme);
        for (uint32_t i = 0u; i < 8u; i++) {
            TEST_ASSERT_TRUE((thr[i] >= -1.0f) && (thr[i] <= 1.0f));
        }
        TEST_ASSERT_TRUE(d_abs((double)K.i_w) <= (double)prm_oku(IDA_PRM_kon_lim_fz) + 1e-3);
    }
    bitki_adim(thr);
    g_n++;
    g_t_ns += DT_SIM_NS;
}

/* Kabul 3a: derinlik basamağı 1 → 4 m. */
static void test_derinlik_basamagi(void)
{
    kur(1.0, 0.0);
    ida_kon_istek_t s;
    istek_dalis(&s, 4.0f, 0.0f);
    const double hedef = 4.0;
    double asma = 0.0;
    double son_disari = 0.0;
    double ort = 0.0;
    uint32_t ort_n = 0u;
    const uint32_t toplam = 60000u; /* 60 s */
    for (uint32_t n = 0u; n < toplam; n++) {
        adim(&s);
        const double t = (double)n * DT_SIM;
        const double e = B_.pos[2] - hedef;
        if (e > asma) {
            asma = e;
        }
        if (d_abs(e) > 0.06) {
            son_disari = t;
        }
        if (n >= (toplam - 5000u)) {
            ort += d_abs(e);
            ort_n++;
        }
        TEST_ASSERT_TRUE(d_abs(B_.eul[0]) < 0.05);
        TEST_ASSERT_TRUE(d_abs(B_.eul[1]) < 0.05);
    }
    ort /= (double)ort_n;
    TEST_ASSERT_TRUE_MESSAGE(asma <= 0.12, "derinlik aşması");
    TEST_ASSERT_TRUE_MESSAGE(son_disari <= 20.0, "derinlik oturma süresi");
    TEST_ASSERT_TRUE_MESSAGE(ort <= 0.005, "derinlik kalıcı hatası");
    TEST_ASSERT_TRUE(d_abs(B_.eul[2]) < 0.01);
}

/* Kabul 3b: yön basamağı 0 → 90°, sabit bozucu moment altında (integral sınanır). */
static void test_yon_basamagi(void)
{
    kur(2.0, 0.0);
    B_.bozucu_mom[2] = 0.3;
    ida_kon_istek_t s;
    istek_dalis(&s, 2.0f, 0.5f * IDA_PI_F);
    const double hedef = 0.5 * 3.14159265358979;
    double asma = 0.0;
    double son_disari = 0.0;
    double ort = 0.0;
    uint32_t ort_n = 0u;
    const uint32_t toplam = 40000u;
    const double bant = 2.0 * 3.14159265358979 / 180.0;
    for (uint32_t n = 0u; n < toplam; n++) {
        adim(&s);
        const double t = (double)n * DT_SIM;
        const double e = B_.eul[2] - hedef;
        if (e > asma) {
            asma = e;
        }
        if (d_abs(e) > bant) {
            son_disari = t;
        }
        if (n >= (toplam - 5000u)) {
            ort += d_abs(e);
            ort_n++;
        }
    }
    ort /= (double)ort_n;
    TEST_ASSERT_TRUE_MESSAGE(asma <= (5.0 * 3.14159265358979 / 180.0), "yön aşması");
    TEST_ASSERT_TRUE_MESSAGE(son_disari <= 8.0, "yön oturma süresi");
    TEST_ASSERT_TRUE_MESSAGE(ort <= (0.2 * 3.14159265358979 / 180.0), "yön kalıcı hatası");
    TEST_ASSERT_TRUE(d_abs(B_.pos[2] - 2.0) < 0.05);
}

/* Kabul 4: uzun süreli doymadan sonra toparlanma (sarma önlemi). */
static void test_sarma(void)
{
    kur(3.0, 0.0);
    ida_kon_istek_t s;
    istek_dalis(&s, 3.0f, 0.0f);
    for (uint32_t n = 0u; n < 10000u; n++) { /* 10 s: otur */
        adim(&s);
    }
    TEST_ASSERT_TRUE(d_abs(B_.pos[2] - 3.0) < 0.02);
    /* Araç 1 m'de sıkışık; istek 3 m: çıkış 30 s sınırda kalır. */
    B_.pos[2] = 1.0;
    B_.sikisik = true;
    for (uint32_t n = 0u; n < 30000u; n++) {
        adim(&s);
    }
    const float lim = prm_oku(IDA_PRM_kon_lim_fz);
    TEST_ASSERT_TRUE(K.i_w <= lim);
    B_.sikisik = false;
    double asma = 0.0;
    double son_disari = 0.0;
    for (uint32_t n = 0u; n < 40000u; n++) {
        adim(&s);
        const double t = (double)n * DT_SIM;
        const double e = B_.pos[2] - 3.0;
        if (e > asma) {
            asma = e;
        }
        if (d_abs(e) > 0.1) {
            son_disari = t;
        }
    }
    TEST_ASSERT_TRUE_MESSAGE(son_disari <= 8.0, "sarma: toparlanma süresi");
    TEST_ASSERT_TRUE_MESSAGE(asma <= 0.2, "sarma: aşma");
}

/* Yüzey rejimi: dikey iticiler tam 0, yön tutulur. */
static void test_yuzey_rejimi(void)
{
    kur(0.0, 0.0);
    ida_kon_istek_t s;
    istek_dalis(&s, 0.0f, 0.3f);
    s.kip = IDA_KON_KIP_YUZEY;
    B_.sikisik = false;
    for (uint32_t n = 0u; n < 20000u; n++) {
        adim(&s);
        if ((g_n % ALT_ADIM) == 1u) {
            for (uint32_t i = 4u; i < 8u; i++) {
                TEST_ASSERT_TRUE(K.thr[i] == 0.0f);
            }
        }
    }
    TEST_ASSERT_TRUE(d_abs(B_.eul[2] - 0.3) < 0.02);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_derinlik_basamagi);
    RUN_TEST(test_yon_basamagi);
    RUN_TEST(test_sarma);
    RUN_TEST(test_yuzey_rejimi);
    return UNITY_END();
}
