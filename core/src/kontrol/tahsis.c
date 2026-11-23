/*
 * tahsis.c — itki tahsisi ve itki eğrisinin tersi (W3 kontrol). Açıklama tahsis.h'tadır.
 */
#include "kontrol/tahsis.h"

#include "mat/ida_math.h"

_Static_assert((uint32_t)IDA_PRM_kon_t8_ez - (uint32_t)IDA_PRM_kon_t1_rx == 47u,
               "itici geometrisi parametreleri ardışık (8 × 6)");
_Static_assert((uint32_t)IDA_PRM_kon_sekil_c4 - (uint32_t)IDA_PRM_kon_sekil_c0 == 4u,
               "şekil katsayıları ardışık");

#define SEKIL_IZGARA   1000u   /* A5-K104: u_j = j/1000                                   */
#define YON_TOLERANS   1.0e-3f /* |‖ε‖ − 1| sınırı (A5-K32'nin uygulamadaki toleransı)    */
#define DF_ESIK        1.0e-12f

/* Sabit olmayan iki boyutlu diziyi const parametreye açık dönüşümle verir: ISO C17'de
   T (*)[N] → const T (*)[N] örtük dönüşümü uyumsuz sayılır (GCC -pedantic-errors). */
#define SABIT_B(x) ((const float(*)[IDA_TAH_ITICI])(x))
#define SABIT_N(x) ((const float(*)[IDA_TAH_ITICI])(x))

typedef union {
    uint32_t u;
    float f;
} bit32_t;

static float ham_f32(const ida_prm_kume_t *k, uint32_t indis)
{
    bit32_t b;
    b.u = k->ham[indis];
    return b.f;
}

static float geo(const ida_prm_kume_t *k, uint32_t itici, uint32_t alan)
{
    return ham_f32(k, (uint32_t)IDA_PRM_kon_t1_rx + (6u * itici) + alan);
}

float ida_tah_polinom(const float c[5], float u)
{
    const float u2 = u * u;
    float p = c[4];
    p = (p * u2) + c[3];
    p = (p * u2) + c[2];
    p = (p * u2) + c[1];
    p = (p * u2) + c[0];
    return p * u;
}

static void sekil_katsayilari(const ida_prm_kume_t *k, float c[5])
{
    for (uint32_t j = 0u; j < 5u; j++) {
        c[j] = ham_f32(k, (uint32_t)IDA_PRM_kon_sekil_c0 + j);
    }
}

/* A5-K104: N = max_j F(j/1000); N sonlu ve pozitif, j ≥ 1 için F(u_j) > 0. Geçersizse 0. */
static float sekil_boleni(const float c[5])
{
    float n = 0.0f;
    for (uint32_t j = 1u; j <= SEKIL_IZGARA; j++) {
        const float f = ida_tah_polinom(c, (float)j / (float)SEKIL_IZGARA);
        if (!ida_sonlu(f) || !(f > 0.0f)) {
            return 0.0f;
        }
        if (f > n) {
            n = f;
        }
    }
    return n;
}

/* B'nin sütunları; ε birim değilse false. yatay: |ε_z| < 0,5 olan iticiler. */
static bool matris_kur(const ida_prm_kume_t *k, float b[IDA_TAH_EKSEN][IDA_TAH_ITICI],
                       uint8_t *yatay)
{
    *yatay = 0u;
    for (uint32_t i = 0u; i < IDA_TAH_ITICI; i++) {
        const float rx = geo(k, i, 0u);
        const float ry = geo(k, i, 1u);
        const float rz = geo(k, i, 2u);
        const float ex = geo(k, i, 3u);
        const float ey = geo(k, i, 4u);
        const float ez = geo(k, i, 5u);
        const float n = ida_karekok((ex * ex) + (ey * ey) + (ez * ez));
        if (!ida_sonlu(n) || !(n >= (1.0f - YON_TOLERANS)) || !(n <= (1.0f + YON_TOLERANS))) {
            return false;
        }
        b[IDA_EKS_X][i] = ex;
        b[IDA_EKS_Y][i] = ey;
        b[IDA_EKS_Z][i] = ez;
        b[IDA_EKS_K][i] = (ry * ez) - (rz * ey);
        b[IDA_EKS_M][i] = (rz * ex) - (rx * ez);
        b[IDA_EKS_N][i] = (rx * ey) - (ry * ex);
        if ((ez < 0.5f) && (ez > -0.5f)) {
            *yatay = (uint8_t)(*yatay | (uint8_t)(1u << i));
        }
    }
    return true;
}

/* ---- Simetrik özdeğer ayrışımı (Jacobi) ve sözde ters -------------------------------------- */

#define JACOBI_TUR 24u

/* n×n simetrik a (n ≤ 6) yerinde köşegenleştirilir: a[k][k] özdeğerler, v'nin sütunları
   özvektörler. Döngü sayısı sabit üst sınırlıdır (özyineleme ve bellek ayırma yok). */
static void jacobi(float a[IDA_TAH_EKSEN][IDA_TAH_EKSEN], uint32_t n,
                   float v[IDA_TAH_EKSEN][IDA_TAH_EKSEN])
{
    for (uint32_t i = 0u; i < n; i++) {
        for (uint32_t j = 0u; j < n; j++) {
            v[i][j] = (i == j) ? 1.0f : 0.0f;
        }
    }
    for (uint32_t tur = 0u; tur < JACOBI_TUR; tur++) {
        float kosegen_disi = 0.0f;
        float toplam = 0.0f;
        for (uint32_t i = 0u; i < n; i++) {
            for (uint32_t j = 0u; j < n; j++) {
                toplam += a[i][j] * a[i][j];
                if (i != j) {
                    kosegen_disi += a[i][j] * a[i][j];
                }
            }
        }
        if (!(kosegen_disi > (1.0e-14f * toplam))) {
            return;
        }
        for (uint32_t p = 0u; p < n; p++) {
            for (uint32_t q = p + 1u; q < n; q++) {
                const float apq = a[p][q];
                if (apq == 0.0f) {
                    continue;
                }
                const float teta = (a[q][q] - a[p][p]) / (2.0f * apq);
                const float mt = (teta < 0.0f) ? -teta : teta;
                float t = 0.0f;
                if (mt > 1.0e15f) {
                    t = 0.5f / teta;
                } else {
                    t = 1.0f / (mt + ida_karekok((teta * teta) + 1.0f));
                    if (teta < 0.0f) {
                        t = -t;
                    }
                }
                const float c = 1.0f / ida_karekok((t * t) + 1.0f);
                const float s = t * c;
                a[p][p] -= t * apq;
                a[q][q] += t * apq;
                a[p][q] = 0.0f;
                a[q][p] = 0.0f;
                for (uint32_t r = 0u; r < n; r++) {
                    if ((r != p) && (r != q)) {
                        const float arp = a[r][p];
                        const float arq = a[r][q];
                        a[r][p] = (c * arp) - (s * arq);
                        a[p][r] = a[r][p];
                        a[r][q] = (s * arp) + (c * arq);
                        a[q][r] = a[r][q];
                    }
                    const float vrp = v[r][p];
                    const float vrq = v[r][q];
                    v[r][p] = (c * vrp) - (s * vrq);
                    v[r][q] = (s * vrp) + (c * vrq);
                }
            }
        }
    }
}

/* Simetrik a'nın (n ≤ 6) kesilmiş sözde tersi: özdeğer ≤ esik olan yönler 0 (*tam = false).
   Sonuç sonlu değilse false. a bozulur. */
static bool simetrik_ters(float a[IDA_TAH_EKSEN][IDA_TAH_EKSEN], uint32_t n, float esik,
                          float ters[IDA_TAH_EKSEN][IDA_TAH_EKSEN], bool *tam)
{
    float v[IDA_TAH_EKSEN][IDA_TAH_EKSEN];
    jacobi(a, n, v);
    float d[IDA_TAH_EKSEN] = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
    *tam = true;
    for (uint32_t k = 0u; k < n; k++) {
        if (a[k][k] > esik) {
            d[k] = 1.0f / a[k][k];
        } else {
            *tam = false;
        }
    }
    bool sonlu = true;
    for (uint32_t i = 0u; i < n; i++) {
        for (uint32_t j = 0u; j < n; j++) {
            float s = 0.0f;
            for (uint32_t k = 0u; k < n; k++) {
                s += v[i][k] * d[k] * v[j][k];
            }
            ters[i][j] = s;
            sonlu = sonlu && ida_sonlu(s);
        }
    }
    return sonlu;
}

/* ---- Öncelik grupları ve izdüşümlü tersler ------------------------------------------------- */

#define GRUP_YOK 255u
#define KESME_GORELI 1.0e-5f /* grup Gram matrisinde kesme eşiği: iz(A_g M A_gᵀ) × bu */

static const uint8_t GRUP_DALIS[4][2] = {
    {IDA_EKS_K, IDA_EKS_M}, {IDA_EKS_Z, GRUP_YOK}, {IDA_EKS_N, GRUP_YOK}, {IDA_EKS_X, IDA_EKS_Y}};
static const uint8_t GRUP_YUZEY[2][2] = {{IDA_EKS_N, GRUP_YOK}, {IDA_EKS_X, IDA_EKS_Y}};

static uint32_t grup_boyu(const uint8_t g[2])
{
    return (g[1] == GRUP_YOK) ? 1u : 2u;
}

/* ns ← ns − (ns a)(ns a)ᵀ / (aᵀ ns a): a satırının sıfır uzayına izdüşüm (Greville). a ns
   altında bağımlıysa (aᵀ ns a ≤ eşik) değişmez. */
static void satir_cikar(float ns[IDA_TAH_ITICI][IDA_TAH_ITICI], const float a[IDA_TAH_ITICI],
                        uint8_t maske)
{
    float na[IDA_TAH_ITICI];
    float d = 0.0f;
    float olcek = 0.0f;
    for (uint32_t i = 0u; i < IDA_TAH_ITICI; i++) {
        float s = 0.0f;
        for (uint32_t j = 0u; j < IDA_TAH_ITICI; j++) {
            s += ns[i][j] * a[j];
        }
        na[i] = s;
        d += a[i] * s;
        if ((maske & (1u << i)) != 0u) {
            olcek += a[i] * a[i];
        }
    }
    if (!(d > (KESME_GORELI * olcek))) {
        return;
    }
    for (uint32_t i = 0u; i < IDA_TAH_ITICI; i++) {
        for (uint32_t j = 0u; j < IDA_TAH_ITICI; j++) {
            ns[i][j] -= (na[i] * na[j]) / d;
        }
    }
}

static void maske_izdusum(float ns[IDA_TAH_ITICI][IDA_TAH_ITICI], uint8_t maske)
{
    for (uint32_t i = 0u; i < IDA_TAH_ITICI; i++) {
        for (uint32_t j = 0u; j < IDA_TAH_ITICI; j++) {
            ns[i][j] = ((i == j) && ((maske & (1u << i)) != 0u)) ? 1.0f : 0.0f;
        }
    }
}

/* Grubun ns altındaki tersi: pg = ns A_gᵀ (A_g ns A_gᵀ)⁺ (8×m). *tam: bütün yönler üretilebilir;
   olcek ≤ 0 ise grubun yetkisi yoktur (pg = 0). */
static bool grup_tersi(const float b[IDA_TAH_EKSEN][IDA_TAH_ITICI], uint8_t maske,
                       const uint8_t grup[2], const float ns[IDA_TAH_ITICI][IDA_TAH_ITICI],
                       float pg[IDA_TAH_ITICI][2], float na[IDA_TAH_ITICI][2], bool *tam)
{
    const uint32_t m = grup_boyu(grup);
    float olcek = 0.0f;
    for (uint32_t c = 0u; c < m; c++) {
        for (uint32_t i = 0u; i < IDA_TAH_ITICI; i++) {
            float s = 0.0f;
            for (uint32_t j = 0u; j < IDA_TAH_ITICI; j++) {
                s += ns[i][j] * b[grup[c]][j];
            }
            na[i][c] = s;
            pg[i][c] = 0.0f;
            if ((maske & (1u << i)) != 0u) {
                olcek += b[grup[c]][i] * b[grup[c]][i];
            }
        }
    }
    *tam = false;
    if (!(olcek > 0.0f)) {
        return true;
    }
    float gm[IDA_TAH_EKSEN][IDA_TAH_EKSEN];
    for (uint32_t r = 0u; r < m; r++) {
        for (uint32_t c = 0u; c < m; c++) {
            float s = 0.0f;
            for (uint32_t i = 0u; i < IDA_TAH_ITICI; i++) {
                s += b[grup[r]][i] * na[i][c];
            }
            gm[r][c] = s;
        }
    }
    float gt[IDA_TAH_EKSEN][IDA_TAH_EKSEN];
    const bool sonlu = simetrik_ters(gm, m, KESME_GORELI * olcek, gt, tam);
    for (uint32_t i = 0u; i < IDA_TAH_ITICI; i++) {
        for (uint32_t c = 0u; c < m; c++) {
            float s = 0.0f;
            for (uint32_t d = 0u; d < m; d++) {
                s += na[i][d] * gt[d][c];
            }
            pg[i][c] = s;
        }
    }
    return sonlu;
}

/* p (8×6): her eksenin sütununa kendi grubunun tersi; ns: bütün grupların sıfır uzayı
   izdüşümü (yalnız maskeli iticiler). Grup için önce "temiz" ters denenir: öteki bütün
   eksenlerin sıfır uzayında (yan etki yok; tam rankta en küçük norm tersinin sütunları). Bu
   uzayda grup tam üretilemiyorsa (rank kaybı) yalnız üst öncelikli grupların sıfır uzayı
   kullanılır: üst eksenler korunur, alt eksenlerdeki yan etki alt gruplarca düzeltilir ya da
   bırakılır. */
static bool grup_tersleri(float b[IDA_TAH_EKSEN][IDA_TAH_ITICI], uint8_t maske,
                          const uint8_t (*gruplar)[2], uint32_t ng,
                          float p[IDA_TAH_ITICI][IDA_TAH_EKSEN],
                          float ns[IDA_TAH_ITICI][IDA_TAH_ITICI])
{
    for (uint32_t i = 0u; i < IDA_TAH_ITICI; i++) {
        for (uint32_t j = 0u; j < IDA_TAH_EKSEN; j++) {
            p[i][j] = 0.0f;
        }
    }
    maske_izdusum(ns, maske);
    bool sonlu = true;
    float pg[IDA_TAH_ITICI][2];
    float na[IDA_TAH_ITICI][2];
    for (uint32_t g = 0u; g < ng; g++) {
        const uint32_t m = grup_boyu(gruplar[g]);
        /* Temiz: öteki grupların bütün eksenlerinin sıfır uzayı. */
        float no[IDA_TAH_ITICI][IDA_TAH_ITICI];
        maske_izdusum(no, maske);
        for (uint32_t h = 0u; h < ng; h++) {
            if (h == g) {
                continue;
            }
            for (uint32_t c = 0u; c < grup_boyu(gruplar[h]); c++) {
                satir_cikar(no, b[gruplar[h][c]], maske);
            }
        }
        bool tam = false;
        sonlu = sonlu && grup_tersi(SABIT_B(b), maske, gruplar[g], SABIT_N(no), pg, na, &tam);
        if (!tam) {
            sonlu = sonlu && grup_tersi(SABIT_B(b), maske, gruplar[g], SABIT_N(ns), pg, na, &tam);
        }
        for (uint32_t i = 0u; i < IDA_TAH_ITICI; i++) {
            for (uint32_t c = 0u; c < m; c++) {
                p[i][gruplar[g][c]] = pg[i][c];
            }
        }
        /* Üst öncelikli sıfır uzayı: bu grubun satırları çıkarılır. */
        for (uint32_t c = 0u; c < m; c++) {
            satir_cikar(ns, b[gruplar[g][c]], maske);
        }
    }
    for (uint32_t i = 0u; i < IDA_TAH_ITICI; i++) {
        for (uint32_t j = 0u; j < IDA_TAH_ITICI; j++) {
            sonlu = sonlu && ida_sonlu(ns[i][j]);
        }
    }
    return sonlu;
}

/* κ₂(Bs M Bsᵀ): seçili eksenler ve iticiler. Tekilse −1. */
static float kosul(const float b[IDA_TAH_EKSEN][IDA_TAH_ITICI], const uint8_t *eks, uint32_t n,
                   uint8_t maske)
{
    float a[IDA_TAH_EKSEN][IDA_TAH_EKSEN];
    for (uint32_t r = 0u; r < n; r++) {
        for (uint32_t c = 0u; c < n; c++) {
            float s = 0.0f;
            for (uint32_t i = 0u; i < IDA_TAH_ITICI; i++) {
                if ((maske & (1u << i)) != 0u) {
                    s += b[eks[r]][i] * b[eks[c]][i];
                }
            }
            a[r][c] = s;
        }
    }
    float v[IDA_TAH_EKSEN][IDA_TAH_EKSEN];
    jacobi(a, n, v);
    float enk = a[0][0];
    float enb = a[0][0];
    for (uint32_t k = 1u; k < n; k++) {
        enk = (a[k][k] < enk) ? a[k][k] : enk;
        enb = (a[k][k] > enb) ? a[k][k] : enb;
    }
    if (!ida_sonlu(enk) || !ida_sonlu(enb) || !(enk > 0.0f)) {
        return -1.0f;
    }
    const float kappa = enb / enk;
    return ida_sonlu(kappa) ? kappa : -1.0f;
}

static const uint8_t EKS_DALIS[6] = {IDA_EKS_X, IDA_EKS_Y, IDA_EKS_Z,
                                     IDA_EKS_K, IDA_EKS_M, IDA_EKS_N};
static const uint8_t EKS_YUZEY[3] = {IDA_EKS_X, IDA_EKS_Y, IDA_EKS_N};

/* Tablodan ileri değer: F_şekil(u), u ∈ [0, 1]. */
static float tablo_ileri(const float *s, float u)
{
    if (!(u > 0.0f)) {
        return 0.0f;
    }
    if (u >= 1.0f) {
        return s[IDA_TAH_TABLO - 1u];
    }
    const float x = u * (float)(IDA_TAH_TABLO - 1u);
    uint32_t k = (uint32_t)x;
    if (k >= (IDA_TAH_TABLO - 1u)) {
        k = IDA_TAH_TABLO - 2u;
    }
    const float t = x - (float)k;
    return s[k] + (t * (s[k + 1u] - s[k]));
}

/* Tablonun tersi: F_şekil(u) = v'yi sağlayan en küçük u (v ∈ [0, 1]). */
static float tablo_ters(const float *s, float v)
{
    if (!(v > 0.0f)) {
        return 0.0f;
    }
    if (v >= s[IDA_TAH_TABLO - 1u]) {
        /* Tablonun düz kuyruğu: değere ilk ulaşılan nokta. */
        uint32_t k = IDA_TAH_TABLO - 1u;
        while ((k > 0u) && (s[k - 1u] >= v)) {
            k--;
        }
        return (float)k / (float)(IDA_TAH_TABLO - 1u);
    }
    uint32_t alt = 0u;                  /* s[alt] < v */
    uint32_t ust = IDA_TAH_TABLO - 1u;  /* s[ust] ≥ v */
    while ((ust - alt) > 1u) {
        const uint32_t orta = (alt + ust) / 2u;
        if (s[orta] >= v) {
            ust = orta;
        } else {
            alt = orta;
        }
    }
    const float ds = s[ust] - s[alt];
    float t = 1.0f;
    if (ds > 0.0f) {
        t = (v - s[alt]) / ds;
    }
    return ((float)alt + t) / (float)(IDA_TAH_TABLO - 1u);
}

uint8_t ida_tah_kur(ida_tah_t *t, const ida_prm_kume_t *k, uint8_t disi)
{
    for (uint32_t r = 0u; r < IDA_TAH_EKSEN; r++) {
        for (uint32_t i = 0u; i < IDA_TAH_ITICI; i++) {
            t->b[r][i] = 0.0f;
        }
    }
    t->disi = disi;
    if (!matris_kur(k, t->b, &t->yatay)) {
        return IDA_TAH_YON_BIRIM;
    }
    t->f_ileri = ham_f32(k, (uint32_t)IDA_PRM_kon_fmax_ileri);
    t->f_geri = ham_f32(k, (uint32_t)IDA_PRM_kon_fmax_geri);
    t->u_min = ham_f32(k, (uint32_t)IDA_PRM_kon_u_min);

    float c[5];
    sekil_katsayilari(k, c);
    const float n = sekil_boleni(c);
    if (!(n > 0.0f) || !ida_sonlu(n)) {
        return IDA_TAH_SEKIL;
    }
    t->sekil[0] = 0.0f;
    for (uint32_t j = 1u; j < IDA_TAH_TABLO; j++) {
        const float u = (float)j / (float)(IDA_TAH_TABLO - 1u);
        float v = ida_tah_polinom(c, u) / n;
        if (v < t->sekil[j - 1u]) {
            v = t->sekil[j - 1u]; /* monoton kırpma (A5-K104) */
        }
        if (v > 1.0f) {
            v = 1.0f;
        }
        t->sekil[j] = v;
    }
    t->sekil[IDA_TAH_TABLO - 1u] = 1.0f; /* max_{v ≤ 1} F(v) / N = 1 tanım gereği */
    t->s_min = tablo_ileri(t->sekil, t->u_min);

    /* W3-K14: ~disi int'e yükselir ve 0xFFu ile birleşince işaret değişir (Clang 21
       -Wsign-conversion); aynı değer işaretsiz XOR ile. */
    const uint8_t etkin = (uint8_t)(0xFFu ^ disi);
    if (!grup_tersleri(t->b, etkin, GRUP_DALIS, 4u, t->p_dalis, t->n_dalis)) {
        return IDA_TAH_SAYISAL;
    }
    if (!grup_tersleri(t->b, (uint8_t)(etkin & t->yatay), GRUP_YUZEY, 2u, t->p_yuzey,
                       t->n_yuzey)) {
        return IDA_TAH_SAYISAL;
    }
    return IDA_TAH_TAMAM;
}

float ida_tah_kosul_sayisi(const ida_prm_kume_t *k, uint8_t rejim)
{
    float b[IDA_TAH_EKSEN][IDA_TAH_ITICI];
    uint8_t yatay = 0u;
    if (!matris_kur(k, b, &yatay)) {
        return -1.0f;
    }
    if (rejim == IDA_TAH_YUZEY) {
        return kosul(SABIT_B(b), EKS_YUZEY, 3u, yatay);
    }
    return kosul(SABIT_B(b), EKS_DALIS, 6u, 0xFFu);
}

uint8_t ida_tah_param_denetle(const ida_prm_kume_t *k)
{
    float b[IDA_TAH_EKSEN][IDA_TAH_ITICI];
    uint8_t yatay = 0u;
    if (!matris_kur(k, b, &yatay)) {
        return IDA_TAH_YON_BIRIM;
    }
    const float kd = kosul(SABIT_B(b), EKS_DALIS, 6u, 0xFFu);
    if (!(kd > 0.0f) || !(kd <= IDA_TAH_KOSUL_EN_BUYUK)) {
        return IDA_TAH_RANK;
    }
    const float ky = kosul(SABIT_B(b), EKS_YUZEY, 3u, yatay);
    if (!(ky > 0.0f) || !(ky <= IDA_TAH_KOSUL_EN_BUYUK)) {
        return IDA_TAH_YUZEY_RANK;
    }
    float c[5];
    sekil_katsayilari(k, c);
    if (!(sekil_boleni(c) > 0.0f)) {
        return IDA_TAH_SEKIL;
    }
    return IDA_TAH_TAMAM;
}

void ida_tah_uret(const ida_tah_t *t, const float f[IDA_TAH_ITICI], float tau_a[IDA_TAH_EKSEN])
{
    for (uint32_t r = 0u; r < IDA_TAH_EKSEN; r++) {
        float s = 0.0f;
        for (uint32_t i = 0u; i < IDA_TAH_ITICI; i++) {
            s += t->b[r][i] * f[i];
        }
        tau_a[r] = s;
    }
}

void ida_tah_sinirlar(const ida_tah_t *t, float alt[IDA_TAH_ITICI], float ust[IDA_TAH_ITICI])
{
    for (uint32_t i = 0u; i < IDA_TAH_ITICI; i++) {
        const bool kapali = (t->disi & (1u << i)) != 0u;
        alt[i] = kapali ? 0.0f : -t->f_geri;
        ust[i] = kapali ? 0.0f : t->f_ileri;
    }
}

/* f + α df'nin [alt, ust]'te kaldığı en büyük α ∈ [0, 1]. */
static float adim_katsayisi(const float f[IDA_TAH_ITICI], const float df[IDA_TAH_ITICI],
                            const float alt[IDA_TAH_ITICI], const float ust[IDA_TAH_ITICI])
{
    float alfa = 1.0f;
    for (uint32_t i = 0u; i < IDA_TAH_ITICI; i++) {
        float sinir = 1.0f;
        if (df[i] > DF_ESIK) {
            sinir = (ust[i] - f[i]) / df[i];
        } else if (df[i] < -DF_ESIK) {
            sinir = (alt[i] - f[i]) / df[i];
        } else {
            continue;
        }
        if (!(sinir > 0.0f)) {
            sinir = 0.0f; /* NaN dahil */
        }
        if (sinir < alfa) {
            alfa = sinir;
        }
    }
    return alfa;
}

static void adim_uygula(float f[IDA_TAH_ITICI], const float df[IDA_TAH_ITICI], float alfa,
                        const float alt[IDA_TAH_ITICI], const float ust[IDA_TAH_ITICI])
{
    for (uint32_t i = 0u; i < IDA_TAH_ITICI; i++) {
        float v = f[i] + (alfa * df[i]);
        if (v > ust[i]) {
            v = ust[i];
        }
        if (!(v >= alt[i])) {
            v = alt[i]; /* NaN dahil */
        }
        f[i] = v;
    }
}

void ida_tah_dagit(const ida_tah_t *t, uint8_t rejim, const float tau[IDA_TAH_EKSEN],
                   const float alt[IDA_TAH_ITICI], const float ust[IDA_TAH_ITICI],
                   float f[IDA_TAH_ITICI], float tau_a[IDA_TAH_EKSEN])
{
    const bool yuzey = rejim == IDA_TAH_YUZEY;
    /* Etkin pencere: çağıranın penceresi ∩ statik sınır ∩ devre dışı ∩ rejim. Boş kesişim
       statik sınırın çağıran penceresine yakın ucuna daralır. */
    float lo[IDA_TAH_ITICI];
    float hi[IDA_TAH_ITICI];
    for (uint32_t i = 0u; i < IDA_TAH_ITICI; i++) {
        const bool kapali = ((t->disi & (1u << i)) != 0u) ||
                            (yuzey && ((t->yatay & (1u << i)) == 0u));
        float a = -t->f_geri;
        float u = t->f_ileri;
        if (alt[i] > a) {
            a = alt[i];
        }
        if (ust[i] < u) {
            u = ust[i];
        }
        if (!(a <= u)) { /* NaN dahil */
            const float uc = (alt[i] > t->f_ileri) ? t->f_ileri : -t->f_geri;
            a = uc;
            u = uc;
        }
        if (kapali) {
            a = 0.0f;
            u = 0.0f;
        }
        lo[i] = a;
        hi[i] = u;
        /* Başlangıç: girişteki f (çağıranın önceki tahsisi) pencereye kırpılır. */
        float v = f[i];
        if (!(v >= a)) { /* NaN dahil */
            v = a;
        }
        if (v > u) {
            v = u;
        }
        f[i] = v;
    }
    const float(*p)[IDA_TAH_EKSEN] = yuzey ? t->p_yuzey : t->p_dalis;
    const float(*ns)[IDA_TAH_ITICI] = yuzey ? t->n_yuzey : t->n_dalis;
    const uint8_t(*gruplar)[2] = yuzey ? GRUP_YUZEY : GRUP_DALIS;
    const uint32_t ng = yuzey ? 2u : 4u;
    float df[IDA_TAH_ITICI];
    for (uint32_t g = 0u; g < ng; g++) {
        ida_tah_uret(t, f, tau_a);
        const uint32_t m = grup_boyu(gruplar[g]);
        for (uint32_t i = 0u; i < IDA_TAH_ITICI; i++) {
            float s = 0.0f;
            for (uint32_t c = 0u; c < m; c++) {
                const uint8_t eks = gruplar[g][c];
                s += p[i][eks] * (tau[eks] - tau_a[eks]);
            }
            df[i] = s;
        }
        adim_uygula(f, df, adim_katsayisi(f, df, lo, hi), lo, hi);
    }
    /* Elde edileni değiştirmeyen sıfır uzayı bileşeni: f ← f − β N f (en küçük norma doğru). */
    for (uint32_t i = 0u; i < IDA_TAH_ITICI; i++) {
        float s = 0.0f;
        for (uint32_t j = 0u; j < IDA_TAH_ITICI; j++) {
            s += ns[i][j] * f[j];
        }
        df[i] = -s;
    }
    adim_uygula(f, df, adim_katsayisi(f, df, lo, hi), lo, hi);
    ida_tah_uret(t, f, tau_a);
}

float ida_tah_komut(const ida_tah_t *t, float itki)
{
    if (!ida_sonlu(itki)) {
        return 0.0f;
    }
    const bool geri = itki < 0.0f;
    float s = geri ? (-itki / t->f_geri) : (itki / t->f_ileri);
    if (!(s > 0.0f)) {
        return 0.0f;
    }
    if (s > 1.0f) {
        s = 1.0f;
    }
    float u = tablo_ters(t->sekil, s);
    if (u < t->u_min) {
        u = (s < (0.5f * t->s_min)) ? 0.0f : t->u_min;
    }
    if (u > 1.0f) {
        u = 1.0f;
    }
    return geri ? -u : u;
}

