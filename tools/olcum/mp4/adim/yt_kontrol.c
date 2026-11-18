/*
 * W0-MP4a yer tutucu: güdüm (LOS/ILOS, derinlik referans modeli; 02 §2.4), kademeli kontrol (6 eksen; 02 §2.5)
 * ve itki tahsisi (8 itici, öncelik gruplu doyma, itki eğrisi tersi; 02 §2.5). Kazanç ve geometri değerleri
 * yer tutucudur; araç parametresi değildir.
 */
#include "yt.h"
#include "yt_ic.h"
#include "yt_mat.h"

#include <math.h>

#define T_ADIM   0.0025f
#define BW_DALIS 1.82f     /* B − W, iki yüklü (02 §2.5) */

/* ---------------- güdüm ---------------- */
void yt_gudum_baslat(yt_gudum_t *g)
{
    static const float r[YT_ROTA_NOKTA][3] = {
        { 0.0f, 0.0f, 2.0f }, { 40.0f, 0.0f, 3.0f }, { 40.0f, 30.0f, 5.0f }, { 0.0f, 30.0f, 5.0f },
        { -20.0f, 10.0f, 4.0f }, { -20.0f, -20.0f, 6.0f }, { 20.0f, -25.0f, 3.0f }, { 10.0f, -5.0f, 2.0f } };
    for (int i = 0; i < YT_ROTA_NOKTA; i++) {
        for (int j = 0; j < 3; j++) { g->rota[i][j] = r[i][j]; }
    }
    g->rota_i = 0u;
    g->y_int = 0.0f;
    g->chi_d = 0.0f;
    g->d_ref[0] = g->d_ref[1] = g->d_ref[2] = 0.0f;
    g->irtifa_f = 5.0f;
    g->u_d = 0.5f;
    g->sp_kuat[0] = 1.0f; g->sp_kuat[1] = g->sp_kuat[2] = g->sp_kuat[3] = 0.0f;
    g->sp_derinlik = 0.0f;
    g->sp_konum[0] = g->sp_konum[1] = g->sp_konum[2] = 0.0f;
}

void yt_gudum_adim(yt_t *s)
{
    yt_gudum_t *g = &s->g;
    if (!s->ayar.her_adim_dis_dongu && (s->n_adim % 40u) != 0u) { return; }  /* 10 Hz (02 §2.2) */
    const float dt = s->ayar.her_adim_dis_dongu ? T_ADIM : 40.0f * T_ADIM;
    const float *p = s->e.xo.p;
    const uint32_t i0 = g->rota_i % YT_ROTA_NOKTA, i1 = (g->rota_i + 1u) % YT_ROTA_NOKTA;
    const float dx = g->rota[i1][0] - g->rota[i0][0], dy = g->rota[i1][1] - g->rota[i0][1];
    const float pi_p = atan2f(dy, dx);
    const float c = cosf(pi_p), sn = sinf(pi_p);
    const float ex = p[0] - g->rota[i0][0], ey = p[1] - g->rota[i0][1];
    const float boy = sqrtf(dx * dx + dy * dy);
    const float along = c * ex + sn * ey;
    const float cross = -sn * ex + c * ey;
    if (along > boy - 2.0f) { g->rota_i++; }
    const float delta = 5.0f, kappa = 0.3f;
    const float yk = cross + kappa * g->y_int;
    g->y_int += dt * delta * cross / (delta * delta + yk * yk);
    g->y_int = yt_sinirla(g->y_int, -20.0f, 20.0f);
    g->chi_d = yt_sar_pi(pi_p - atan2f(cross + kappa * g->y_int, delta));
    /* derinlik referans modeli (üçüncü mertebe, hız ve ivme sınırlı) */
    const float wn = 0.8f;
    const float hedef = g->rota[i1][2];
    float jerk = wn * wn * wn * (hedef - g->d_ref[0]) - 3.0f * wn * wn * g->d_ref[1] - 3.0f * wn * g->d_ref[2];
    jerk = yt_sinirla(jerk, -1.0f, 1.0f);
    g->d_ref[2] = yt_sinirla(g->d_ref[2] + dt * jerk, -0.2f, 0.2f);
    g->d_ref[1] = yt_sinirla(g->d_ref[1] + dt * g->d_ref[2], -0.3f, 0.3f);
    g->d_ref[0] += dt * g->d_ref[1];
    /* irtifa tutma: d_ref = d + (a − a_ref), filtreli (DVL irtifası) */
    g->irtifa_f += 0.1f * (s->in.dvl_irtifa - g->irtifa_f);
    const float irtifa_ref = 3.0f;
    const float d_irtifa = p[2] + (g->irtifa_f - irtifa_ref);
    g->sp_derinlik = fminf(g->d_ref[0], d_irtifa);
    yt_q_euler(0.0f, 0.0f, g->chi_d, g->sp_kuat);
    for (int j = 0; j < 3; j++) { g->sp_konum[j] = g->rota[i1][j]; }
}

/* ---------------- kontrol ---------------- */
void yt_kontrol_baslat(yt_kontrol_t *c)
{
    for (int i = 0; i < 3; i++) {
        c->w_sp[i] = 0.0f; c->i_w[i] = 0.0f; c->d_onceki[i] = 0.0f; c->d_filtre[i] = 0.0f;
    }
    c->vz_sp = 0.0f; c->i_vz = 0.0f;
    c->vxy_sp[0] = c->vxy_sp[1] = 0.0f; c->i_vxy[0] = c->i_vxy[1] = 0.0f;
    c->harman = 0.0f;
    for (int i = 0; i < YT_EKSEN; i++) { c->kalinti[i] = 0.0f; c->tau_req[i] = 0.0f; }
}

/* Doyma kalıntısına göre integral yönetimi (02 §2.5): doymayı artıran yönde dondur. */
static float integral(float i, float e, float ki, float kalinti, float sinir)
{
    const float di = ki * e * T_ADIM;
    if ((kalinti > 0.05f && di > 0.0f) || (kalinti < -0.05f && di < 0.0f)) { return i; }
    return yt_sinirla(i + di, -sinir, sinir);
}

void yt_kontrol_adim(yt_t *s)
{
    yt_kontrol_t *c = &s->c;
    const yt_nom_t *x = &s->e.xo;
    const float psi = atan2f(2.0f * (x->q[0] * x->q[3] + x->q[1] * x->q[2]),
                             1.0f - 2.0f * (x->q[2] * x->q[2] + x->q[3] * x->q[3]));
    const float cp = cosf(psi), sp = sinf(psi);

    /* dış döngüler, 100 Hz (02 §2.5) */
    if (s->ayar.her_adim_dis_dongu || (s->n_adim % 4u) == 0u) {
        float qi[4], qe[4];
        const float *qs = s->g.sp_kuat;
        qi[0] = qs[0]; qi[1] = -qs[1]; qi[2] = -qs[2]; qi[3] = -qs[3];
        yt_q_carp(qi, x->q, qe);
        const float sg = qe[0] < 0.0f ? -1.0f : 1.0f;
        for (int i = 0; i < 3; i++) { c->w_sp[i] = yt_sinirla(-2.0f * 1.5f * sg * qe[1 + i], -0.6f, 0.6f); }
        c->vz_sp = yt_sinirla(0.8f * (s->g.sp_derinlik - x->p[2]), -0.3f, 0.3f);
        const float en = s->g.sp_konum[0] - x->p[0], ee = s->g.sp_konum[1] - x->p[1];
        const float eh_x = cp * en + sp * ee, eh_y = -sp * en + cp * ee;
        c->vxy_sp[0] = yt_sinirla(0.4f * eh_x, -1.0f, 1.0f);
        c->vxy_sp[1] = yt_sinirla(0.4f * eh_y, -0.5f, 0.5f);
    }

    /* iç döngüler, her adım */
    float tau_d[YT_EKSEN], tau_y[YT_EKSEN];
    static const float KP_W[3] = { 8.0f, 8.0f, 6.0f }, KI_W[3] = { 1.0f, 1.0f, 0.8f }, KD_W[3] = { 0.5f, 0.5f, 0.4f };
    for (int i = 0; i < 3; i++) {
        const float e = c->w_sp[i] - s->e.w_son[i];
        const float d = (e - c->d_onceki[i]) / T_ADIM;
        c->d_onceki[i] = e;
        c->d_filtre[i] += 0.2f * (d - c->d_filtre[i]);
        c->i_w[i] = integral(c->i_w[i], e, KI_W[i], c->kalinti[3 + i], 2.0f);
        tau_d[3 + i] = KP_W[i] * e + c->i_w[i] + KD_W[i] * c->d_filtre[i];
    }
    /* dikey hız PI + yüzerlik ileri beslemesi (NED aşağı +(B − W)) */
    const float evz = c->vz_sp - x->v[2];
    c->i_vz = integral(c->i_vz, evz, 2.0f, c->kalinti[2], 10.0f);
    const float Fz = 30.0f * evz + c->i_vz + BW_DALIS;
    /* yatay hız PI + sönüm ileri beslemesi, yön çerçevesinde */
    const float vh_x = cp * x->v[0] + sp * x->v[1], vh_y = -sp * x->v[0] + cp * x->v[1];
    const float ex = c->vxy_sp[0] - vh_x, ey = c->vxy_sp[1] - vh_y;
    c->i_vxy[0] = integral(c->i_vxy[0], ex, 3.0f, c->kalinti[0], 15.0f);
    c->i_vxy[1] = integral(c->i_vxy[1], ey, 3.0f, c->kalinti[1], 15.0f);
    const float Fx = 25.0f * ex + c->i_vxy[0] + 33.0f * c->vxy_sp[0];
    const float Fy = 25.0f * ey + c->i_vxy[1] + 40.0f * c->vxy_sp[1];
    /* [X Y Z]ᵀ = Rᵀ R_ψ [F_x F_y F_z]ᵀ */
    float R[3][3], Rpsi[3][3], fy[3], fn[3], fb[3];
    yt_q_rot(x->q, R);
    Rpsi[0][0] = cp; Rpsi[0][1] = -sp; Rpsi[0][2] = 0.0f;
    Rpsi[1][0] = sp; Rpsi[1][1] = cp;  Rpsi[1][2] = 0.0f;
    Rpsi[2][0] = 0.0f; Rpsi[2][1] = 0.0f; Rpsi[2][2] = 1.0f;
    fy[0] = Fx; fy[1] = Fy; fy[2] = Fz;
    yt_m3v(Rpsi, fy, fn);
    yt_m3tv(R, fn, fb);
    for (int i = 0; i < 3; i++) { tau_d[i] = fb[i]; }
    /* yüzey rejimi: X, Y, N; Z, K, M hidrostatiğe bırakılır */
    for (int i = 0; i < YT_EKSEN; i++) { tau_y[i] = 0.0f; }
    tau_y[0] = fb[0]; tau_y[1] = fb[1]; tau_y[5] = tau_d[5];
    /* rejim kazanç karışımı (1 s) */
    c->harman = yt_sinirla(c->harman + T_ADIM, 0.0f, 1.0f);
    for (int i = 0; i < YT_EKSEN; i++) { c->tau_req[i] = c->harman * tau_d[i] + (1.0f - c->harman) * tau_y[i]; }
}

/* ---------------- tahsis ---------------- */
static float f_sekil(float u)
{
    /* Monoton kırpılmış biçim (yer tutucu): ölü bant dışında u|u| benzeri. */
    const float a = fabsf(u);
    const float f = a < 0.065f ? 0.0f : (0.3f * a + 0.7f * a * a);
    return u < 0.0f ? -f : f;
}

static int ters6(float A[6][6], float Ai[6][6])
{
    float M[6][12];
    for (int i = 0; i < 6; i++) {
        for (int j = 0; j < 6; j++) { M[i][j] = A[i][j]; M[i][6 + j] = (i == j) ? 1.0f : 0.0f; }
    }
    for (int c = 0; c < 6; c++) {
        int piv = c;
        for (int r = c + 1; r < 6; r++) { if (fabsf(M[r][c]) > fabsf(M[piv][c])) { piv = r; } }
        if (fabsf(M[piv][c]) < 1e-9f) { return 0; }
        if (piv != c) {
            for (int j = 0; j < 12; j++) { const float t = M[c][j]; M[c][j] = M[piv][j]; M[piv][j] = t; }
        }
        const float ip = 1.0f / M[c][c];
        for (int j = 0; j < 12; j++) { M[c][j] *= ip; }
        for (int r = 0; r < 6; r++) {
            if (r == c) { continue; }
            const float f = M[r][c];
            for (int j = 0; j < 12; j++) { M[r][j] -= f * M[c][j]; }
        }
    }
    for (int i = 0; i < 6; i++) {
        for (int j = 0; j < 6; j++) { Ai[i][j] = M[i][6 + j]; }
    }
    return 1;
}

void yt_tahsis_baslat(yt_tahsis_t *a)
{
    /* 4 yatay vektörlü (45°, z = 0,085 m) + 4 dikey itici; konumlar gövde referansına göre (yer tutucu). */
    static const float yon[YT_ITICI][3] = {
        { 0.7071f, -0.7071f, 0.0f }, { 0.7071f, 0.7071f, 0.0f }, { -0.7071f, -0.7071f, 0.0f }, { -0.7071f, 0.7071f, 0.0f },
        { 0.0f, 0.0f, 1.0f }, { 0.0f, 0.0f, 1.0f }, { 0.0f, 0.0f, 1.0f }, { 0.0f, 0.0f, 1.0f } };
    static const float kon[YT_ITICI][3] = {
        { 0.20f, 0.11f, 0.085f }, { 0.20f, -0.11f, 0.085f }, { -0.20f, 0.11f, 0.085f }, { -0.20f, -0.11f, 0.085f },
        { 0.12f, 0.22f, 0.0f }, { 0.12f, -0.22f, 0.0f }, { -0.12f, 0.22f, 0.0f }, { -0.12f, -0.22f, 0.0f } };
    for (int i = 0; i < YT_ITICI; i++) {
        float m[3];
        yt_capraz(kon[i], yon[i], m);
        for (int k = 0; k < 3; k++) { a->B[k][i] = yon[i][k]; a->B[3 + k][i] = m[k]; }
    }
    /* B⁺ = W⁻¹ Bᵀ (B W⁻¹ Bᵀ)⁻¹, W = diag(1/F_maks²) */
    float wi[YT_ITICI], BWB[6][6], BWBi[6][6];
    for (int i = 0; i < YT_ITICI; i++) { wi[i] = 50.0f * 50.0f; }
    for (int r = 0; r < 6; r++) {
        for (int c = 0; c < 6; c++) {
            float s = 0.0f;
            for (int k = 0; k < YT_ITICI; k++) { s += a->B[r][k] * wi[k] * a->B[c][k]; }
            BWB[r][c] = s;
        }
    }
    if (!ters6(BWB, BWBi)) {
        for (int r = 0; r < 6; r++) { for (int c = 0; c < 6; c++) { BWBi[r][c] = 0.0f; } }
    }
    for (int i = 0; i < YT_ITICI; i++) {
        for (int c = 0; c < 6; c++) {
            float s = 0.0f;
            for (int k = 0; k < 6; k++) { s += wi[i] * a->B[k][i] * BWBi[k][c]; }
            a->Bp[i][c] = s;
        }
    }
    for (int k = 0; k < YT_TABLO_N; k++) {
        const float u = -1.0f + 2.0f * (float)k / (float)(YT_TABLO_N - 1);
        a->tablo_u[k] = u;
        a->tablo_f[k] = f_sekil(u);
    }
    for (int i = 0; i < YT_ITICI; i++) { a->u[i] = 0.0f; a->pwm[i] = 1500u; }
    a->v_bat_f = 16.0f;
}

/* F_şekil tersi: monoton tablo üzerinde ikili arama + doğrusal ara değerleme (02 §2.5). */
static float sekil_ters(const yt_tahsis_t *a, float f)
{
    if (f <= a->tablo_f[0]) { return a->tablo_u[0]; }
    if (f >= a->tablo_f[YT_TABLO_N - 1]) { return a->tablo_u[YT_TABLO_N - 1]; }
    int lo = 0, hi = YT_TABLO_N - 1;
    while (hi - lo > 1) {
        const int m = (lo + hi) / 2;
        if (a->tablo_f[m] <= f) { lo = m; } else { hi = m; }
    }
    const float df = a->tablo_f[hi] - a->tablo_f[lo];
    const float t = df > 1e-9f ? (f - a->tablo_f[lo]) / df : 0.0f;
    return a->tablo_u[lo] + t * (a->tablo_u[hi] - a->tablo_u[lo]);
}

void yt_tahsis_adim(yt_t *s)
{
    yt_tahsis_t *a = &s->a;
    static const int GRUP[4][2] = { { 3, 4 }, { 2, -1 }, { 5, -1 }, { 0, 1 } }; /* [K,M] > [Z] > [N] > [X,Y] */
    a->v_bat_f += 0.01f * (s->in.v_bat - a->v_bat_f);
    const float vo = a->v_bat_f / 16.0f;
    const float f_ileri = 50.0f * vo * vo, f_geri = 40.0f * vo * vo;
    for (int i = 0; i < YT_ITICI; i++) { a->u[i] = 0.0f; }
    for (int g = 0; g < 4; g++) {
        float tg[YT_EKSEN] = { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f };
        for (int k = 0; k < 2; k++) {
            const int ax = GRUP[g][k];
            if (ax < 0) { continue; }
            float bu = 0.0f;
            for (int i = 0; i < YT_ITICI; i++) { bu += a->B[ax][i] * a->u[i]; }
            tg[ax] = s->c.tau_req[ax] - bu;
        }
        float du[YT_ITICI];
        for (int i = 0; i < YT_ITICI; i++) {
            float t = 0.0f;
            for (int c = 0; c < YT_EKSEN; c++) { t += a->Bp[i][c] * tg[c]; }
            du[i] = t;
        }
        float alfa = 1.0f;
        for (int i = 0; i < YT_ITICI; i++) {
            if (du[i] > 1e-9f) { alfa = fminf(alfa, (f_ileri - a->u[i]) / du[i]); }
            else if (du[i] < -1e-9f) { alfa = fminf(alfa, (-f_geri - a->u[i]) / du[i]); }
        }
        alfa = yt_sinirla(alfa, 0.0f, 1.0f);
        if (alfa < 1.0f) { a->doyma_say++; }
        for (int i = 0; i < YT_ITICI; i++) { a->u[i] += alfa * du[i]; }
    }
    for (int r = 0; r < YT_EKSEN; r++) {
        float t = 0.0f;
        for (int i = 0; i < YT_ITICI; i++) { t += a->B[r][i] * a->u[i]; }
        a->tau_a[r] = t;
        s->c.kalinti[r] = s->c.tau_req[r] - t;
    }
    /* İtki eğrisi tersi ve PWM: ölü bant 1500 ± 25 µs, en küçük |u| = 0,065 (02 §2.5) */
    const float f_min = 0.5f * f_sekil(0.065f);
    for (int i = 0; i < YT_ITICI; i++) {
        const float fmax = a->u[i] >= 0.0f ? f_ileri : f_geri;
        const float fn = a->u[i] / fmax;
        float uc = 0.0f;
        if (fabsf(fn) >= f_min) {
            uc = sekil_ters(a, fn);
            if (fabsf(uc) < 0.065f) { uc = uc < 0.0f ? -0.065f : 0.065f; }
        }
        float pw = 1500.0f + 400.0f * uc;
        pw = yt_sinirla(pw, 1100.0f, 1900.0f);
        a->pwm[i] = (uint16_t)(pw + 0.5f);
    }
}
