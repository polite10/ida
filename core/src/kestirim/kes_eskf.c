/*
 * kes_eskf.c — hata durumlu EKF'nin matematiği (02 §2.3.1, §2.3.4, §2.3.6). Sahibi: W3 kestirim.
 *
 * Hata durumu x = (δp, δv, δθ, δb_g, δb_a), tutum hatası sağdan yerel: q = q̂ ⊗ Exp(δθ).
 * Sürekli zaman Jacobian'ı A'nın sıfır olmayan blokları:
 *   δṗ = δv;  δv̇ = −R[f]× δθ − R δb_a;  δθ̇ = −[ω]× δθ − δb_g;  δḃ = gürültü.
 * Φ = I + A Δt seyrek olduğundan Φ P Φᵀ yoğun çarpım yerine sütun ve satır işlemleriyle
 * kurulur (02 §6 "elle açılmış seyrek çarpım"; maliyet yoğun 15×15 çarpımın yaklaşık onda
 * biri; tahmin, kartta ölçülmedi).
 */
#include "kestirim/kes_ic.h"

#include "mat/ida_math.h"

#define N IDA_KES_N

float ida_kes_ns_s(int64_t ns)
{
    return (float)ns * 1.0e-9f;
}

ida_q_t ida_kes_q_euler(float roll, float pitch, float yaw)
{
    const float cr = ida_cos(0.5f * roll);
    const float sr = ida_sin(0.5f * roll);
    const float cp = ida_cos(0.5f * pitch);
    const float sp = ida_sin(0.5f * pitch);
    const float cy = ida_cos(0.5f * yaw);
    const float sy = ida_sin(0.5f * yaw);
    const ida_q_t q = {cr * cp * cy + sr * sp * sy, sr * cp * cy - cr * sp * sy,
                       cr * sp * cy + sr * cp * sy, cr * cp * sy - sr * sp * cy};
    return q;
}

ida_v3_t ida_kes_q_log(ida_q_t q)
{
    ida_q_t a = q;
    if (a.w < 0.0f) {
        a.w = -a.w;
        a.x = -a.x;
        a.y = -a.y;
        a.z = -a.z;
    }
    const ida_v3_t v = {a.x, a.y, a.z};
    const float n = ida_v3_norm(v);
    if (!(n > 1.0e-7f)) {
        return ida_v3_olcekle(v, 2.0f); /* küçük açı: Log ≈ 2 v */
    }
    const float aci = 2.0f * ida_atan2(n, a.w);
    return ida_v3_olcekle(v, aci / n);
}

void ida_kes_nominal_yay(ida_kes_nominal_t *x, ida_v3_t w, ida_v3_t f, float dt, float g)
{
    const ida_v3_t wc = ida_v3_cikar(w, x->bg);
    const ida_v3_t fc = ida_v3_cikar(f, x->ba);
    const ida_m3_t r = ida_q_dcm(x->q);
    ida_v3_t a = ida_m3_v3(r, fc);
    a.z += g;
    x->p = ida_v3_topla(x->p, ida_v3_topla(ida_v3_olcekle(x->v, dt), ida_v3_olcekle(a, 0.5f * dt * dt)));
    x->v = ida_v3_topla(x->v, ida_v3_olcekle(a, dt));
    ida_q_t dq;
    if (!ida_q_donme_vektorunden(ida_v3_olcekle(wc, dt), &dq)) {
        /* Sonlu olmayan ω: durum bozulur, sağlamlık denetimi yakalar. */
        x->q.w = wc.x * dt;
        return;
    }
    const ida_q_t q = ida_q_carp(x->q, dq);
    ida_q_t b;
    if (ida_q_birimle(q, &b)) {
        x->q = b;
    } else {
        x->q = q;
    }
}

/* Φ'nin bir vektöre uygulanması (yerinde). */
typedef struct {
    float dt;
    float avt[3][3]; /* −R[f]× */
    float avb[3][3]; /* −R     */
    float att[3][3]; /* −[ω]×  */
} phi_t;

static void phi_uygula(float x[N], const phi_t *ph)
{
    float yeni[9];
    for (uint32_t i = 0u; i < 3u; i++) {
        float sv = 0.0f;
        float st = 0.0f;
        for (uint32_t j = 0u; j < 3u; j++) {
            sv += ph->avt[i][j] * x[IDA_KES_ITH + j] + ph->avb[i][j] * x[IDA_KES_IBA + j];
            st += ph->att[i][j] * x[IDA_KES_ITH + j];
        }
        st -= x[IDA_KES_IBG + i];
        yeni[i] = x[IDA_KES_IP + i] + ph->dt * x[IDA_KES_IV + i];
        yeni[3u + i] = x[IDA_KES_IV + i] + ph->dt * sv;
        yeni[6u + i] = x[IDA_KES_ITH + i] + ph->dt * st;
    }
    for (uint32_t i = 0u; i < 9u; i++) {
        x[i] = yeni[i];
    }
}

void ida_kes_P_yay(float P[IDA_KES_N][IDA_KES_N], const ida_kes_nominal_t *x, ida_v3_t w,
                   ida_v3_t f, float dt, const ida_kes_ayar_t *a)
{
    const ida_v3_t wc = ida_v3_cikar(w, x->bg);
    const ida_v3_t fc = ida_v3_cikar(f, x->ba);
    const ida_m3_t r = ida_q_dcm(x->q);
    const ida_m3_t rf = ida_m3_carp(r, ida_m3_capraz(fc));
    const ida_m3_t wx = ida_m3_capraz(wc);
    phi_t ph;
    ph.dt = dt;
    for (uint32_t i = 0u; i < 3u; i++) {
        for (uint32_t j = 0u; j < 3u; j++) {
            ph.avt[i][j] = -rf.m[i][j];
            ph.avb[i][j] = -r.m[i][j];
            ph.att[i][j] = -wx.m[i][j];
        }
    }
    /* M = Φ P: her sütuna Φ. */
    float s[N];
    for (uint32_t j = 0u; j < N; j++) {
        for (uint32_t i = 0u; i < N; i++) {
            s[i] = P[i][j];
        }
        phi_uygula(s, &ph);
        for (uint32_t i = 0u; i < N; i++) {
            P[i][j] = s[i];
        }
    }
    /* M Φᵀ: her satıra Φ. */
    for (uint32_t i = 0u; i < N; i++) {
        phi_uygula(P[i], &ph);
    }
    /* Q_d = L Q_c Lᵀ Δt (birinci mertebe; eksenler bağımsız, izotrop). */
    const float qv = a->q_acc * a->q_acc * dt;
    const float qt = a->q_gyro * a->q_gyro * dt;
    const float qbg = a->q_bg * a->q_bg * dt;
    const float qba = a->q_ba * a->q_ba * dt;
    for (uint32_t i = 0u; i < 3u; i++) {
        P[IDA_KES_IV + i][IDA_KES_IV + i] += qv;
        P[IDA_KES_ITH + i][IDA_KES_ITH + i] += qt;
        P[IDA_KES_IBG + i][IDA_KES_IBG + i] += qbg;
        P[IDA_KES_IBA + i][IDA_KES_IBA + i] += qba;
    }
    ida_kes_P_duzelt(P);
}

bool ida_kes_skaler(float P[IDA_KES_N][IDA_KES_N], const float h[IDA_KES_N], float y, float r,
                    float gamma, float dx[IDA_KES_N], float *nis)
{
    float u[N];
    float hdx = 0.0f;
    float s = r;
    for (uint32_t i = 0u; i < N; i++) {
        float t = 0.0f;
        for (uint32_t j = 0u; j < N; j++) {
            t += P[i][j] * h[j];
        }
        u[i] = t;
        s += h[i] * t;
        hdx += h[i] * dx[i];
    }
    if (!(s > 0.0f) || !ida_sonlu(s)) {
        return false;
    }
    const float yj = y - hdx;
    const float oran = (yj * yj) / s;
    *nis = oran;
    if ((gamma > 0.0f) && !(oran <= gamma)) {
        return false;
    }
    float kz[N];
    for (uint32_t i = 0u; i < N; i++) {
        kz[i] = u[i] / s;
        dx[i] += kz[i] * yj;
    }
    /* Joseph formunun açılmış hali (02 §2.3.4): P ← P − k uᵀ − u kᵀ + s k kᵀ; simetrik. */
    for (uint32_t i = 0u; i < N; i++) {
        for (uint32_t j = i; j < N; j++) {
            const float d = -(kz[i] * u[j]) - (u[i] * kz[j]) + (s * kz[i] * kz[j]);
            P[i][j] += d;
            if (j != i) {
                P[j][i] = P[i][j];
            }
        }
    }
    return true;
}

/* x_θ ← (I − ½[δθ]×) x_θ. */
static void g_uygula(float x[N], ida_v3_t dth)
{
    const ida_v3_t t = {x[IDA_KES_ITH], x[IDA_KES_ITH + 1u], x[IDA_KES_ITH + 2u]};
    const ida_v3_t c = ida_v3_dis(dth, t);
    x[IDA_KES_ITH] = t.x - 0.5f * c.x;
    x[IDA_KES_ITH + 1u] = t.y - 0.5f * c.y;
    x[IDA_KES_ITH + 2u] = t.z - 0.5f * c.z;
}

void ida_kes_enjekte(ida_kes_nominal_t *x, float P[IDA_KES_N][IDA_KES_N], const float dx[IDA_KES_N])
{
    const ida_v3_t dp = {dx[IDA_KES_IP], dx[IDA_KES_IP + 1u], dx[IDA_KES_IP + 2u]};
    const ida_v3_t dv = {dx[IDA_KES_IV], dx[IDA_KES_IV + 1u], dx[IDA_KES_IV + 2u]};
    const ida_v3_t dth = {dx[IDA_KES_ITH], dx[IDA_KES_ITH + 1u], dx[IDA_KES_ITH + 2u]};
    const ida_v3_t dbg = {dx[IDA_KES_IBG], dx[IDA_KES_IBG + 1u], dx[IDA_KES_IBG + 2u]};
    const ida_v3_t dba = {dx[IDA_KES_IBA], dx[IDA_KES_IBA + 1u], dx[IDA_KES_IBA + 2u]};
    x->p = ida_v3_topla(x->p, dp);
    x->v = ida_v3_topla(x->v, dv);
    x->bg = ida_v3_topla(x->bg, dbg);
    x->ba = ida_v3_topla(x->ba, dba);
    ida_q_t dq;
    if (ida_q_donme_vektorunden(dth, &dq)) {
        const ida_q_t q = ida_q_carp(x->q, dq);
        ida_q_t b;
        x->q = ida_q_birimle(q, &b) ? b : q;
    } else {
        x->q.w = dth.x; /* sonlu olmayan düzeltme: sağlamlık denetimi yakalar */
    }
    /* P ← G P Gᵀ, G sıfırlamadan önceki δθ ile (02 §2.3.4). */
    float s[N];
    for (uint32_t j = 0u; j < N; j++) {
        for (uint32_t i = 0u; i < N; i++) {
            s[i] = P[i][j];
        }
        g_uygula(s, dth);
        for (uint32_t i = 0u; i < N; i++) {
            P[i][j] = s[i];
        }
    }
    for (uint32_t i = 0u; i < N; i++) {
        g_uygula(P[i], dth);
    }
    ida_kes_P_duzelt(P);
}

void ida_kes_P_duzelt(float P[IDA_KES_N][IDA_KES_N])
{
    /* Varyans alt sınırları (blok başına; tahmin): konum, hız, tutum, jiroskop ve ivme sapması. */
    static const float ALT[5] = {1.0e-8f, 1.0e-10f, 1.0e-12f, 1.0e-14f, 1.0e-12f};
    for (uint32_t i = 0u; i < N; i++) {
        for (uint32_t j = i + 1u; j < N; j++) {
            const float o = 0.5f * (P[i][j] + P[j][i]);
            P[i][j] = o;
            P[j][i] = o;
        }
        const float alt = ALT[i / 3u];
        if (P[i][i] < alt) { /* NaN karşılaştırması yanlıştır: NaN kalır, denetim yakalar */
            P[i][i] = alt;
        }
    }
}

bool ida_kes_saglam(const ida_kes_t *k)
{
    const ida_kes_nominal_t *x = &k->xh;
    const float d[16] = {x->p.x,  x->p.y,  x->p.z,  x->v.x,  x->v.y,  x->v.z,  x->q.w,  x->q.x,
                         x->q.y,  x->q.z,  x->bg.x, x->bg.y, x->bg.z, x->ba.x, x->ba.y, x->ba.z};
    for (uint32_t i = 0u; i < 16u; i++) {
        if (!ida_sonlu(d[i])) {
            return false;
        }
    }
    const float qn = x->q.w * x->q.w + x->q.x * x->q.x + x->q.y * x->q.y + x->q.z * x->q.z;
    if (!((qn > 0.999f) && (qn < 1.001f))) {
        return false;
    }
    /* Köşegenle ölçeklenmiş matrisin LDLᵀ ayrıştırması; alt üçgen paketli (120 eleman). */
    float olcek[N];
    for (uint32_t i = 0u; i < N; i++) {
        const float v = k->P[i][i];
        if (!(v > 0.0f) || !ida_sonlu(v)) {
            return false;
        }
        olcek[i] = 1.0f / ida_karekok(v);
    }
    float L[(N * (N + 1u)) / 2u];
    float D[N];
    for (uint32_t j = 0u; j < N; j++) {
        const uint32_t jj = (j * (j + 1u)) / 2u;
        float dj = 1.0f;
        for (uint32_t m = 0u; m < j; m++) {
            dj -= L[jj + m] * L[jj + m] * D[m];
        }
        /* Pivot ≤ ε: ε = 0 (ölçeklenmiş matriste; float yuvarlamasıyla yanlış alarm vermemek
           için en küçük güvenli değer). */
        if (!(dj > 0.0f) || !ida_sonlu(dj)) {
            return false;
        }
        D[j] = dj;
        for (uint32_t i = j + 1u; i < N; i++) {
            const uint32_t ii = (i * (i + 1u)) / 2u;
            float t = k->P[i][j] * olcek[i] * olcek[j];
            for (uint32_t m = 0u; m < j; m++) {
                t -= L[ii + m] * L[jj + m] * D[m];
            }
            L[ii + j] = t / dj;
        }
        L[jj + j] = 1.0f;
    }
    return true;
}

bool ida_kes_nis3(float S[3][3], const float y[3], float *nis)
{
    /* Cholesky: S = C Cᵀ. */
    float c00 = S[0][0];
    if (!(c00 > 0.0f)) {
        return false;
    }
    c00 = ida_karekok(c00);
    const float c10 = S[1][0] / c00;
    const float c20 = S[2][0] / c00;
    float c11 = S[1][1] - c10 * c10;
    if (!(c11 > 0.0f)) {
        return false;
    }
    c11 = ida_karekok(c11);
    const float c21 = (S[2][1] - c20 * c10) / c11;
    float c22 = S[2][2] - c20 * c20 - c21 * c21;
    if (!(c22 > 0.0f)) {
        return false;
    }
    c22 = ida_karekok(c22);
    /* z = C⁻¹ y; nis = |z|². */
    const float z0 = y[0] / c00;
    const float z1 = (y[1] - c10 * z0) / c11;
    const float z2 = (y[2] - c20 * z0 - c21 * z1) / c22;
    const float t = z0 * z0 + z1 * z1 + z2 * z2;
    if (!ida_sonlu(t)) {
        return false;
    }
    *nis = t;
    return true;
}
