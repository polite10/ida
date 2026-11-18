/*
 * W0-MP4a yer tutucu: 15 hata durumlu EKF, sabit gecikmeli füzyon ufku ve çıkış öngörücüsü (02 §2.3).
 * Hesap hacmi ve bellek erişim düzeni gerçekçi tutulur; sayısal doğruluk aranmaz.
 * Yoğun 15×15 işlemler bilerek yapısal seyreklik kullanmadan yazıldı (en pahalı makul yorum), yalnız
 * G P Gᵀ sıfırlaması 02 §2.2'nin "≈ 540" bütçesindeki gibi yapılı (yalnız δθ satır/sütunları).
 */
#include "yt.h"
#include "yt_mat.h"
#include "yt_ic.h"

#include <math.h>
#include <string.h>

#define G_N        9.80665f
#define S_ACC      0.05f
#define S_GYRO     0.005f
#define S_BG       1.0e-4f
#define S_BA       1.0e-3f
#define K_CIKIS    0.01f      /* T / τ_o = 2,5 ms / 0,25 s */
#define RHO_G      (1000.0f * G_N)
#define P_ATM      101325.0f
#define P_ALT_SINIR 1.0e-9f

static const float L_P[3]   = { 0.10f, 0.00f, 0.05f };
static const float L_DVL[3] = { -0.20f, 0.00f, 0.10f };
static const float L_A[3]   = { 0.00f, 0.00f, -0.30f };
static const float M_N[3]   = { 0.20f, 0.01f, 0.45f };
static float R_DVL[3][3] = { { 0.70710678f, -0.70710678f, 0.0f },
                                   { 0.70710678f, 0.70710678f, 0.0f },
                                   { 0.0f, 0.0f, 1.0f } };
static const float KAPI[5] = { 0.0f, 6.63f, 9.21f, 11.34f, 13.28f };

void yt_ekf_baslat(yt_ekf_t *e)
{
    memset(&e->xh, 0, sizeof e->xh);
    e->xh.q[0] = 1.0f;
    e->xo = e->xh;
    for (int i = 0; i < YT_N; i++) {
        for (int j = 0; j < YT_N; j++) { e->P[i][j] = 0.0f; }
        e->dx[i] = 0.0f;
    }
    for (int i = 0; i < 3; i++) {
        e->P[i][i] = 1.0f;
        e->P[3 + i][3 + i] = 0.1f;
        e->P[6 + i][6 + i] = 0.01f;
        e->P[9 + i][9 + i] = 1.0e-4f;
        e->P[12 + i][12 + i] = 1.0e-2f;
    }
    e->dt_kov = 0.0f;
    e->imu_son = 0u; e->imu_n = 0u;
    e->cikis_son = 0u; e->cikis_n = 0u;
    for (int k = 0; k < YT_S_SAYI; k++) { e->sf[k].bas = 0u; e->sf[k].n = 0u; }
    e->t_h = 0;
}

/* ---------- tamponlar ---------- */
void yt_ekf_olcum_ekle(yt_ekf_t *e, int sensor, const yt_olcum_t *o)
{
    yt_sfifo_t *f = &e->sf[sensor];
    if (f->n == YT_SENSOR_FIFO) { f->bas = (f->bas + 1u) % YT_SENSOR_FIFO; f->n--; f->tasma++; }
    f->k[(f->bas + f->n) % YT_SENSOR_FIFO] = *o;
    f->n++;
}

static const yt_imu_t *imu_geri(const yt_ekf_t *e, uint32_t geri)
{
    return &e->imu[(e->imu_son + YT_IMU_FIFO - geri) % YT_IMU_FIFO];
}

/* ---------- nominal yayılım ---------- */
static void nom_yay(yt_nom_t *x, const yt_imu_t *m, const float bg[3], const float ba[3], float dt)
{
    float w[3], a[3], R[3][3], an[3], dq[4], th[3];
    for (int i = 0; i < 3; i++) { w[i] = m->w[i] - bg[i]; a[i] = m->a[i] - ba[i]; }
    yt_q_rot(x->q, R);
    yt_m3v(R, a, an);
    an[2] += G_N;
    for (int i = 0; i < 3; i++) {
        x->p[i] += x->v[i] * dt + 0.5f * an[i] * dt * dt;
        x->v[i] += an[i] * dt;
        th[i] = w[i] * dt;
    }
    yt_q_exp(th, dq);
    yt_q_carp(x->q, dq, x->q);
    yt_q_norm(x->q);
}

/* ---------- kovaryans yayılımı: P⁻ = Φ P Φᵀ + Q_d (yoğun) ---------- */
static void kov_yay(yt_ekf_t *e, const yt_imu_t *m, float dt)
{
    float Phi[YT_N][YT_N], T[YT_N][YT_N];
    float R[3][3], f[3], w[3], Sf[3][3], Sw[3][3], RSf[3][3];
    for (int i = 0; i < 3; i++) { f[i] = m->a[i] - e->xh.ba[i]; w[i] = m->w[i] - e->xh.bg[i]; }
    yt_q_rot(e->xh.q, R);
    yt_kosegen_skew(f, Sf);
    yt_kosegen_skew(w, Sw);
    yt_m3m3(R, Sf, RSf);
    for (int i = 0; i < YT_N; i++) {
        for (int j = 0; j < YT_N; j++) { Phi[i][j] = (i == j) ? 1.0f : 0.0f; }
    }
    for (int i = 0; i < 3; i++) {
        Phi[i][3 + i] = dt;
        Phi[6 + i][9 + i] = -dt;
        for (int j = 0; j < 3; j++) {
            Phi[3 + i][6 + j] = -RSf[i][j] * dt;
            Phi[3 + i][12 + j] = -R[i][j] * dt;
            Phi[6 + i][6 + j] -= Sw[i][j] * dt;
        }
    }
    /* T = Φ P */
    for (int i = 0; i < YT_N; i++) {
        for (int j = 0; j < YT_N; j++) {
            float s = 0.0f;
            for (int k = 0; k < YT_N; k++) { s += Phi[i][k] * e->P[k][j]; }
            T[i][j] = s;
        }
    }
    /* P = T Φᵀ */
    for (int i = 0; i < YT_N; i++) {
        for (int j = 0; j < YT_N; j++) {
            float s = 0.0f;
            for (int k = 0; k < YT_N; k++) { s += T[i][k] * Phi[j][k]; }
            e->P[i][j] = s;
        }
    }
    /* Q_d = L Q_c Lᵀ Δt: hız bloğu R diag(σa²) Rᵀ */
    const float qa = S_ACC * S_ACC * dt;
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 3; j++) {
            float s = 0.0f;
            for (int k = 0; k < 3; k++) { s += R[i][k] * qa * R[j][k]; }
            e->P[3 + i][3 + j] += s;
        }
        e->P[6 + i][6 + i] += S_GYRO * S_GYRO * dt;
        e->P[9 + i][9 + i] += S_BG * S_BG * dt;
        e->P[12 + i][12 + i] += S_BA * S_BA * dt;
    }
    e->kov_say++;
}

/* ---------- sıralı skaler güncelleme (Joseph formunun açılmış hali, 02 §2.3.4) ---------- */
static void skaler_guncelle(yt_ekf_t *e, const float h[YT_N], float y, float r)
{
    float u[YT_N], k[YT_N];
    for (int i = 0; i < YT_N; i++) {
        float s = 0.0f;
        for (int j = 0; j < YT_N; j++) { s += e->P[i][j] * h[j]; }
        u[i] = s;
    }
    float s = r;
    float hdx = 0.0f;
    for (int i = 0; i < YT_N; i++) { s += h[i] * u[i]; hdx += h[i] * e->dx[i]; }
    const float is = 1.0f / s;
    for (int i = 0; i < YT_N; i++) { k[i] = u[i] * is; }
    const float inn = y - hdx;
    for (int i = 0; i < YT_N; i++) { e->dx[i] += k[i] * inn; }
    for (int i = 0; i < YT_N; i++) {
        const float ski = s * k[i];
        for (int j = 0; j < YT_N; j++) {
            e->P[i][j] = e->P[i][j] - k[i] * u[j] - u[i] * k[j] + ski * k[j];
        }
    }
    e->skaler_say++;
}

/* Vektör ölçüm: S, kapı (bütün olarak), R ile beyazlatma, sıralı skalerler. Döner: yapılan skaler sayısı. */
static uint32_t vektor_fuzyon(yt_ekf_t *e, int m, float H[4][YT_N], const float y[4], float Rm[4][4],
                              uint32_t kalan)
{
    float HP[4][YT_N], S[4][4], L[4][4], w[4], yw[4], Hw[4][YT_N];
    for (int a = 0; a < m; a++) {
        for (int j = 0; j < YT_N; j++) {
            float s = 0.0f;
            for (int k = 0; k < YT_N; k++) { s += H[a][k] * e->P[k][j]; }
            HP[a][j] = s;
        }
    }
    for (int a = 0; a < 4; a++) {
        for (int b = 0; b < 4; b++) { S[a][b] = 0.0f; }
    }
    for (int a = 0; a < m; a++) {
        for (int b = 0; b < m; b++) {
            float s = Rm[a][b];
            for (int k = 0; k < YT_N; k++) { s += HP[a][k] * H[b][k]; }
            S[a][b] = s;
        }
    }
    /* Kapı: yᵀ S⁻¹ y < γ_m. Yer tutucuda sonuç sayılır, güncelleme her durumda yapılır (üst sınır). */
    if (yt_chol(m, S, L)) {
        yt_ileri_coz(m, L, y, w);
        float nis = 0.0f;
        for (int a = 0; a < m; a++) { nis += w[a] * w[a]; }
        if (nis < KAPI[m]) { e->kapi_gecti++; } else { e->kapi_red++; }
    } else {
        e->kapi_red++;
    }
    /* Beyazlatma: R = L Lᵀ, y' = L⁻¹ y, H' = L⁻¹ H */
    if (!yt_chol(m, Rm, L)) { return 0u; }
    yt_ileri_coz(m, L, y, yw);
    for (int j = 0; j < YT_N; j++) {
        float col[4], out[4];
        for (int a = 0; a < m; a++) { col[a] = H[a][j]; }
        for (int a = m; a < 4; a++) { col[a] = 0.0f; }
        yt_ileri_coz(m, L, col, out);
        for (int a = 0; a < m; a++) { Hw[a][j] = out[a]; }
    }
    uint32_t yapilan = 0u;
    for (int a = 0; a < m; a++) {
        if (kalan != 0u && yapilan >= kalan) { break; }
        skaler_guncelle(e, Hw[a], yw[a], 1.0f);
        yapilan++;
    }
    return yapilan;
}

static void h_temizle(float H[4][YT_N], float Rm[4][4])
{
    for (int a = 0; a < 4; a++) {
        for (int j = 0; j < YT_N; j++) { H[a][j] = 0.0f; }
        for (int b = 0; b < 4; b++) { Rm[a][b] = 0.0f; }
    }
}

/* Ufuk durumunda ölçüm modeli + füzyon. sinir: kalan skaler hakkı (0 = sınırsız). */
static uint32_t sensor_fuzyon(yt_ekf_t *e, int sensor, const yt_olcum_t *o, const yt_imu_t *m, uint32_t kalan)
{
    float H[4][YT_N], Rm[4][4], y[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
    float R[3][3], t1[3], S[3][3], RS[3][3];
    float w[3];
    for (int i = 0; i < 3; i++) { w[i] = m->w[i] - e->xh.bg[i]; }
    yt_q_rot(e->xh.q, R);
    h_temizle(H, Rm);
    int boy = 0;
    switch (sensor) {
    case YT_S_BASINC: {
        boy = 1;
        const float d = (o->z[0] - P_ATM) / RHO_G;
        yt_m3v(R, L_P, t1);
        y[0] = d - (e->xh.p[2] + t1[2]);
        yt_kosegen_skew(L_P, S);
        yt_m3m3(R, S, RS);
        H[0][2] = 1.0f;
        for (int j = 0; j < 3; j++) { H[0][6 + j] = -RS[2][j]; }
        Rm[0][0] = o->r[0][0];
        break;
    }
    case YT_S_DVL: {
        boy = 3;
        float vb[3], wl[3], hb[3], Rt[3][3], A[3][3], SL[3][3];
        yt_m3tv(R, e->xh.v, vb);
        yt_capraz(w, L_DVL, wl);
        for (int i = 0; i < 3; i++) { hb[i] = vb[i] + wl[i]; }
        yt_m3tv(R_DVL, hb, t1);
        for (int i = 0; i < 3; i++) { y[i] = o->z[i] - t1[i]; }
        for (int i = 0; i < 3; i++) {
            for (int j = 0; j < 3; j++) { Rt[i][j] = R[j][i]; }
        }
        float RdT[3][3];
        for (int i = 0; i < 3; i++) {
            for (int j = 0; j < 3; j++) { RdT[i][j] = R_DVL[j][i]; }
        }
        yt_m3m3(RdT, Rt, A);                 /* ∂/∂δv */
        yt_kosegen_skew(vb, S);
        yt_m3m3(RdT, S, RS);                 /* ∂/∂δθ */
        yt_kosegen_skew(L_DVL, SL);
        yt_m3m3(RdT, SL, SL);                /* ∂/∂δb_g */
        for (int i = 0; i < 3; i++) {
            for (int j = 0; j < 3; j++) {
                H[i][3 + j] = A[i][j];
                H[i][6 + j] = RS[i][j];
                H[i][9 + j] = SL[i][j];
                Rm[i][j] = o->r[i][j];
            }
        }
        break;
    }
    case YT_S_MANYETO: {
        boy = 3;
        yt_m3tv(R, M_N, t1);
        for (int i = 0; i < 3; i++) { y[i] = o->z[i] - t1[i]; }
        yt_kosegen_skew(t1, S);
        for (int i = 0; i < 3; i++) {
            for (int j = 0; j < 3; j++) { H[i][6 + j] = S[i][j]; }
            Rm[i][i] = o->r[i][i];
        }
        break;
    }
    default: { /* GNSS: yatay konum 2 + yatay hız 2 (02 §2.3.3) */
        boy = 4;
        float la[3], wl[3], Rwl[3], SA[3][3], SW[3][3];
        yt_m3v(R, L_A, la);
        yt_capraz(w, L_A, wl);
        yt_m3v(R, wl, Rwl);
        /* lat/lon farkı int64'te alınır, sonra float ölçek (02 §2.3.4) */
        const int64_t dlat = (int64_t)o->lat_e7 - (int64_t)YT_ORIJIN_LAT_E7;
        int64_t dlon = (int64_t)o->lon_e7 - (int64_t)YT_ORIJIN_LON_E7;
        if (dlon > 1800000000LL) { dlon -= 3600000000LL; }
        if (dlon < -1800000000LL) { dlon += 3600000000LL; }
        const float n_m = (float)dlat * YT_E7_METRE;
        const float e_m = (float)dlon * YT_E7_METRE * YT_COS_LAT0;
        y[0] = n_m - (e->xh.p[0] + la[0]);
        y[1] = e_m - (e->xh.p[1] + la[1]);
        y[2] = o->z[0] - (e->xh.v[0] + Rwl[0]);
        y[3] = o->z[1] - (e->xh.v[1] + Rwl[1]);
        yt_kosegen_skew(L_A, S);
        yt_m3m3(R, S, SA);
        yt_kosegen_skew(wl, S);
        yt_m3m3(R, S, SW);
        float RSL[3][3];
        yt_kosegen_skew(L_A, S);
        yt_m3m3(R, S, RSL);
        for (int i = 0; i < 2; i++) {
            H[i][i] = 1.0f;
            H[2 + i][3 + i] = 1.0f;
            for (int j = 0; j < 3; j++) {
                H[i][6 + j] = -SA[i][j];
                H[2 + i][6 + j] = -SW[i][j];
                H[2 + i][9 + j] = RSL[i][j];
            }
        }
        for (int i = 0; i < 4; i++) { Rm[i][i] = o->r[i][i]; }
        break;
    }
    }
    return vektor_fuzyon(e, boy, H, y, Rm, kalan);
}

/* Enjeksiyon ve sıfırlama: G = blokköşegen(I, I, I − ½[δθ]×, I, I), sıfırlamadan önceki δθ ile. */
static void enjekte_sifirla(yt_ekf_t *e)
{
    float dq[4], M[3][3], S[3][3];
    for (int i = 0; i < 3; i++) {
        e->xh.p[i] += e->dx[i];
        e->xh.v[i] += e->dx[3 + i];
        e->xh.bg[i] += e->dx[9 + i];
        e->xh.ba[i] += e->dx[12 + i];
    }
    yt_q_exp(&e->dx[6], dq);
    yt_q_carp(e->xh.q, dq, e->xh.q);
    yt_q_norm(e->xh.q);
    yt_kosegen_skew(&e->dx[6], S);
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 3; j++) { M[i][j] = ((i == j) ? 1.0f : 0.0f) - 0.5f * S[i][j]; }
    }
    for (int c = 0; c < YT_N; c++) {          /* satırlar: P[6..8][:] ← M P[6..8][:] */
        float t[3];
        for (int i = 0; i < 3; i++) {
            t[i] = M[i][0] * e->P[6][c] + M[i][1] * e->P[7][c] + M[i][2] * e->P[8][c];
        }
        e->P[6][c] = t[0]; e->P[7][c] = t[1]; e->P[8][c] = t[2];
    }
    for (int r = 0; r < YT_N; r++) {          /* sütunlar: P[:][6..8] ← P[:][6..8] Mᵀ */
        float t[3];
        for (int i = 0; i < 3; i++) {
            t[i] = e->P[r][6] * M[i][0] + e->P[r][7] * M[i][1] + e->P[r][8] * M[i][2];
        }
        e->P[r][6] = t[0]; e->P[r][7] = t[1]; e->P[r][8] = t[2];
    }
    for (int i = 0; i < YT_N; i++) { e->dx[i] = 0.0f; }
    /* simetrikleştirme ve varyans alt sınırı */
    for (int i = 0; i < YT_N; i++) {
        for (int j = i + 1; j < YT_N; j++) {
            const float a = 0.5f * (e->P[i][j] + e->P[j][i]);
            e->P[i][j] = a; e->P[j][i] = a;
        }
        if (e->P[i][i] < P_ALT_SINIR) { e->P[i][i] = P_ALT_SINIR; }
    }
    e->sifirlama_say++;
}

/* LDLᵀ denetimi: pivot ≤ ε ise sayısal çökme (02 §2.3.6). */
static int ldl_denetle(const yt_ekf_t *e)
{
    float L[YT_N][YT_N], D[YT_N];
    for (int j = 0; j < YT_N; j++) {
        float d = e->P[j][j];
        for (int k = 0; k < j; k++) { d -= L[j][k] * L[j][k] * D[k]; }
        if (!(d > 1.0e-12f)) { return 0; }
        D[j] = d;
        const float id = 1.0f / d;
        L[j][j] = 1.0f;
        for (int i = j + 1; i < YT_N; i++) {
            float s = e->P[i][j];
            for (int k = 0; k < j; k++) { s -= L[i][k] * L[j][k] * D[k]; }
            L[i][j] = s * id;
        }
    }
    return 1;
}

/* Çıkış öngörücüsü: x_o yeni IMU ile yayılır, geçmişe yazılır; ufuk ilerlediyse hata düzeltmesi
   x_o(şimdi) ve tampondaki bütün kayıtlara uygulanır (02 §2.3.2; en pahalı yorum: kuaterniyon dahil). */
static void cikis_ongor(yt_ekf_t *e, const yt_imu_t *m, int ufuk_ilerledi)
{
    nom_yay(&e->xo, m, e->xh.bg, e->xh.ba, (float)YT_PERIYOT_NS * 1.0e-9f);
    e->cikis_son = (e->cikis_son + 1u) % YT_CIKIS_FIFO;
    e->cikis[e->cikis_son].t_ns = m->t_ns;
    e->cikis[e->cikis_son].x = e->xo;
    if (e->cikis_n < YT_CIKIS_FIFO) { e->cikis_n++; }
    if (!ufuk_ilerledi || e->cikis_n <= YT_UFUK_ADIM) { return; }
    const yt_cikis_kayit_t *k = &e->cikis[(e->cikis_son + YT_CIKIS_FIFO - YT_UFUK_ADIM) % YT_CIKIS_FIFO];
    float ep[3], ev[3], qi[4], qe[4], eth[3], dq[4];
    for (int i = 0; i < 3; i++) {
        ep[i] = K_CIKIS * (e->xh.p[i] - k->x.p[i]);
        ev[i] = K_CIKIS * (e->xh.v[i] - k->x.v[i]);
    }
    qi[0] = k->x.q[0]; qi[1] = -k->x.q[1]; qi[2] = -k->x.q[2]; qi[3] = -k->x.q[3];
    yt_q_carp(qi, e->xh.q, qe);
    yt_q_log(qe, eth);
    for (int i = 0; i < 3; i++) { eth[i] *= K_CIKIS; }
    yt_q_exp(eth, dq);
    for (uint32_t r = 0u; r < YT_CIKIS_FIFO; r++) {
        yt_nom_t *x = &e->cikis[r].x;
        for (int i = 0; i < 3; i++) { x->p[i] += ep[i]; x->v[i] += ev[i]; }
        yt_q_carp(x->q, dq, x->q);
    }
    for (int i = 0; i < 3; i++) { e->xo.p[i] += ep[i]; e->xo.v[i] += ev[i]; }
    yt_q_carp(e->xo.q, dq, e->xo.q);
    yt_q_norm(e->xo.q);
}

/* Kestirim adımı (02 §2.2 md.2). Gelen IMU ve ölçümler önceden tamponlara eklenmiş olmalı. */
void yt_ekf_adim(yt_t *s)
{
    yt_ekf_t *e = &s->e;
    const yt_ayar_t *ay = &s->ayar;
    const yt_imu_t *yeni = imu_geri(e, 0u);
    int ufuk_ilerledi = 0;

    if (e->imu_n > YT_UFUK_ADIM) {
        const yt_imu_t *hs = imu_geri(e, YT_UFUK_ADIM);  /* t_imu − D anındaki IMU örneği */
        const int64_t t_bas = e->t_h;
        const int64_t t_bit = hs->t_ns;
        int64_t t_cur = t_bas;
        uint32_t kalan = ay->skaler_sinir;
        yt_parca(s, YT_P_NOMINAL);
        for (;;) {
            int en = -1;
            int64_t t_en = INT64_MAX;
            for (int k = 0; k < YT_S_SAYI; k++) {
                yt_sfifo_t *f = &e->sf[k];
                while (f->n > 0u && f->k[f->bas].t_ns <= t_bas) {   /* ufka göre geç: füzyonlanmaz */
                    f->bas = (f->bas + 1u) % YT_SENSOR_FIFO; f->n--; e->gec_ornek++;
                }
                if (f->n > 0u && f->k[f->bas].t_ns <= t_bit && f->k[f->bas].t_ns < t_en) {
                    t_en = f->k[f->bas].t_ns; en = k;
                }
            }
            if (en < 0) { break; }
            const float dt1 = (float)(t_en - t_cur) * 1.0e-9f;
            if (dt1 > 0.0f) { nom_yay(&e->xh, hs, e->xh.bg, e->xh.ba, dt1); e->dt_kov += dt1; }
            t_cur = t_en;
            yt_parca(s, YT_P_NOMINAL);
            if (ay->fuzyon_oncesi_kov && e->dt_kov > 0.0f) {
                kov_yay(e, hs, e->dt_kov);
                e->dt_kov = 0.0f;
                yt_parca(s, YT_P_KOVARYANS);
            }
            /* aynı t_m'deki bütün ölçümler (sabit sıra: basınç, DVL, manyeto, GNSS) */
            for (int k = 0; k < YT_S_SAYI; k++) {
                yt_sfifo_t *f = &e->sf[k];
                if (f->n > 0u && f->k[f->bas].t_ns == t_en) {
                    const uint32_t hak = (ay->skaler_sinir == 0u) ? 0u : kalan;
                    if (ay->skaler_sinir == 0u || kalan > 0u) {
                        const uint32_t y = sensor_fuzyon(e, k, &f->k[f->bas], hs, hak);
                        if (ay->skaler_sinir != 0u) { kalan = (y >= kalan) ? 0u : kalan - y; }
                    }
                    f->bas = (f->bas + 1u) % YT_SENSOR_FIFO; f->n--;
                }
            }
            yt_parca(s, YT_P_FUZYON);
            enjekte_sifirla(e);
            yt_parca(s, YT_P_SIFIRLAMA);
        }
        const float dtk = (float)(t_bit - t_cur) * 1.0e-9f;
        if (dtk > 0.0f) { nom_yay(&e->xh, hs, e->xh.bg, e->xh.ba, dtk); e->dt_kov += dtk; }
        yt_parca(s, YT_P_NOMINAL);
        if (e->dt_kov > 0.0f && (ay->adim_sonu_kov || e->dt_kov >= 0.01f)) {
            kov_yay(e, hs, e->dt_kov);
            e->dt_kov = 0.0f;
            yt_parca(s, YT_P_KOVARYANS);
        }
        if (ay->her_adim_ldl || (s->n_adim % 4u) == 0u) {
            if (!ldl_denetle(e)) { e->ldl_hata++; }
            yt_parca(s, YT_P_SIFIRLAMA);
        }
        e->t_h = t_bit;
        ufuk_ilerledi = 1;
    } else if (e->imu_n == YT_UFUK_ADIM) {
        e->t_h = imu_geri(e, YT_UFUK_ADIM - 1u)->t_ns - YT_PERIYOT_NS;  /* ufuk başlangıcı */
    }
    for (int i = 0; i < 3; i++) { e->w_son[i] = yeni->w[i] - e->xh.bg[i]; }
    cikis_ongor(e, yeni, ufuk_ilerledi);
    yt_parca(s, YT_P_NOMINAL);
}

void yt_ekf_imu_ekle(yt_ekf_t *e, const yt_imu_t *m)
{
    e->imu_son = (e->imu_son + 1u) % YT_IMU_FIFO;
    e->imu[e->imu_son] = *m;
    if (e->imu_n < YT_IMU_FIFO) { e->imu_n++; }
}
