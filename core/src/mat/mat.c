/*
 * mat.c — vektör, kuaterniyon ve küçük matris yardımcıları (W2).
 */
#include "mat/mat.h"

#include "mat/ida_math.h"

#define IDA_MAT_KUCUK 1e-12f

ida_v3_t ida_v3_topla(ida_v3_t lhs, ida_v3_t rhs)
{
    const ida_v3_t s = {lhs.x + rhs.x, lhs.y + rhs.y, lhs.z + rhs.z};
    return s;
}

ida_v3_t ida_v3_cikar(ida_v3_t lhs, ida_v3_t rhs)
{
    const ida_v3_t s = {lhs.x - rhs.x, lhs.y - rhs.y, lhs.z - rhs.z};
    return s;
}

ida_v3_t ida_v3_olcekle(ida_v3_t v, float k)
{
    const ida_v3_t s = {v.x * k, v.y * k, v.z * k};
    return s;
}

float ida_v3_ic(ida_v3_t lhs, ida_v3_t rhs)
{
    return (lhs.x * rhs.x) + (lhs.y * rhs.y) + (lhs.z * rhs.z);
}

ida_v3_t ida_v3_dis(ida_v3_t lhs, ida_v3_t rhs)
{
    const ida_v3_t s = {
        (lhs.y * rhs.z) - (lhs.z * rhs.y),
        (lhs.z * rhs.x) - (lhs.x * rhs.z),
        (lhs.x * rhs.y) - (lhs.y * rhs.x),
    };
    return s;
}

float ida_v3_norm(ida_v3_t v)
{
    return ida_karekok(ida_v3_ic(v, v));
}

bool ida_v3_birim(ida_v3_t v, ida_v3_t *sonuc)
{
    const float n = ida_v3_norm(v);
    if (!(n > IDA_MAT_KUCUK)) {
        const ida_v3_t sifir = {0.0f, 0.0f, 0.0f};
        *sonuc = sifir;
        return false;
    }
    *sonuc = ida_v3_olcekle(v, 1.0f / n);
    return true;
}

ida_q_t ida_q_birim_deger(void)
{
    const ida_q_t q = {1.0f, 0.0f, 0.0f, 0.0f};
    return q;
}

ida_q_t ida_q_carp(ida_q_t lhs, ida_q_t rhs)
{
    const ida_q_t q = {
        (lhs.w * rhs.w) - (lhs.x * rhs.x) - (lhs.y * rhs.y) - (lhs.z * rhs.z),
        (lhs.w * rhs.x) + (lhs.x * rhs.w) + (lhs.y * rhs.z) - (lhs.z * rhs.y),
        (lhs.w * rhs.y) - (lhs.x * rhs.z) + (lhs.y * rhs.w) + (lhs.z * rhs.x),
        (lhs.w * rhs.z) + (lhs.x * rhs.y) - (lhs.y * rhs.x) + (lhs.z * rhs.w),
    };
    return q;
}

ida_q_t ida_q_eslenik(ida_q_t q)
{
    const ida_q_t e = {q.w, -q.x, -q.y, -q.z};
    return e;
}

bool ida_q_birimle(ida_q_t q, ida_q_t *sonuc)
{
    const float n = ida_karekok((q.w * q.w) + (q.x * q.x) + (q.y * q.y) + (q.z * q.z));
    if (!(n > IDA_MAT_KUCUK)) {
        *sonuc = ida_q_birim_deger();
        return false;
    }
    const float k = 1.0f / n;
    const ida_q_t b = {q.w * k, q.x * k, q.y * k, q.z * k};
    *sonuc = b;
    return true;
}

ida_v3_t ida_q_dondur(ida_q_t q, ida_v3_t v)
{
    const ida_q_t p = {0.0f, v.x, v.y, v.z};
    const ida_q_t r = ida_q_carp(ida_q_carp(q, p), ida_q_eslenik(q));
    const ida_v3_t s = {r.x, r.y, r.z};
    return s;
}

ida_q_t ida_q_donme_vektorunden(ida_v3_t rv)
{
    const float aci = ida_v3_norm(rv);
    if (!(aci > IDA_MAT_KUCUK)) {
        /* Küçük açı: q ≈ (1, rv/2), birimlenmiş. */
        const ida_q_t yaklasik = {1.0f, 0.5f * rv.x, 0.5f * rv.y, 0.5f * rv.z};
        ida_q_t b;
        (void)ida_q_birimle(yaklasik, &b);
        return b;
    }
    const float k = ida_sin(0.5f * aci) / aci;
    const ida_q_t q = {ida_cos(0.5f * aci), rv.x * k, rv.y * k, rv.z * k};
    return q;
}

ida_m3_t ida_q_dcm(ida_q_t q)
{
    const float ww = q.w * q.w;
    const float xx = q.x * q.x;
    const float yy = q.y * q.y;
    const float zz = q.z * q.z;
    const float xy = q.x * q.y;
    const float xz = q.x * q.z;
    const float yz = q.y * q.z;
    const float wx = q.w * q.x;
    const float wy = q.w * q.y;
    const float wz = q.w * q.z;
    ida_m3_t r;
    r.m[0][0] = ww + xx - yy - zz;
    r.m[0][1] = 2.0f * (xy - wz);
    r.m[0][2] = 2.0f * (xz + wy);
    r.m[1][0] = 2.0f * (xy + wz);
    r.m[1][1] = ww - xx + yy - zz;
    r.m[1][2] = 2.0f * (yz - wx);
    r.m[2][0] = 2.0f * (xz - wy);
    r.m[2][1] = 2.0f * (yz + wx);
    r.m[2][2] = ww - xx - yy + zz;
    return r;
}

ida_euler_t ida_q_euler(ida_q_t q)
{
    const ida_m3_t r = ida_q_dcm(q);
    ida_euler_t e;
    const ida_aralik_t birim = {-1.0f, 1.0f};
    e.pitch = -ida_asin(ida_kirp(r.m[2][0], birim));
    e.roll = ida_atan2(r.m[2][1], r.m[2][2]);
    e.yaw = ida_atan2(r.m[1][0], r.m[0][0]);
    return e;
}

ida_m3_t ida_m3_birim_deger(void)
{
    ida_m3_t r = {{{0.0f}}};
    r.m[0][0] = 1.0f;
    r.m[1][1] = 1.0f;
    r.m[2][2] = 1.0f;
    return r;
}

ida_m3_t ida_m3_carp(ida_m3_t lhs, ida_m3_t rhs)
{
    ida_m3_t r = {{{0.0f}}}; /* her eleman döngüde yazılır */
    for (uint32_t i = 0u; i < 3u; i++) {
        for (uint32_t j = 0u; j < 3u; j++) {
            r.m[i][j] = (lhs.m[i][0] * rhs.m[0][j]) + (lhs.m[i][1] * rhs.m[1][j]) +
                        (lhs.m[i][2] * rhs.m[2][j]);
        }
    }
    return r;
}

ida_v3_t ida_m3_v3(ida_m3_t m, ida_v3_t v)
{
    const ida_v3_t s = {
        (m.m[0][0] * v.x) + (m.m[0][1] * v.y) + (m.m[0][2] * v.z),
        (m.m[1][0] * v.x) + (m.m[1][1] * v.y) + (m.m[1][2] * v.z),
        (m.m[2][0] * v.x) + (m.m[2][1] * v.y) + (m.m[2][2] * v.z),
    };
    return s;
}

ida_m3_t ida_m3_devrik(ida_m3_t m)
{
    ida_m3_t r = {{{0.0f}}}; /* her eleman döngüde yazılır */
    for (uint32_t i = 0u; i < 3u; i++) {
        for (uint32_t j = 0u; j < 3u; j++) {
            r.m[i][j] = m.m[j][i];
        }
    }
    return r;
}

ida_m3_t ida_m3_capraz(ida_v3_t v)
{
    ida_m3_t r = {{{0.0f}}};
    r.m[0][1] = -v.z;
    r.m[0][2] = v.y;
    r.m[1][0] = v.z;
    r.m[1][2] = -v.x;
    r.m[2][0] = -v.y;
    r.m[2][1] = v.x;
    return r;
}

static bool boyut_gecerli(ida_mat_boyut_t b)
{
    return (b.satir > 0u) && (b.sutun > 0u) && (b.satir <= IDA_MAT_EN_BUYUK) &&
           (b.sutun <= IDA_MAT_EN_BUYUK);
}

bool ida_mat_carp(ida_mat_oku_t lhs, ida_mat_oku_t rhs, const ida_mat_yaz_t *sonuc)
{
    if (!boyut_gecerli(lhs.boyut) || !boyut_gecerli(rhs.boyut) ||
        (lhs.boyut.sutun != rhs.boyut.satir) || (sonuc->boyut.satir != lhs.boyut.satir) ||
        (sonuc->boyut.sutun != rhs.boyut.sutun)) {
        return false;
    }
    const uint32_t m = lhs.boyut.satir;
    const uint32_t n = lhs.boyut.sutun;
    const uint32_t p = rhs.boyut.sutun;
    for (uint32_t i = 0u; i < m; i++) {
        for (uint32_t j = 0u; j < p; j++) {
            float t = 0.0f;
            for (uint32_t k = 0u; k < n; k++) {
                t += lhs.veri[(i * n) + k] * rhs.veri[(k * p) + j];
            }
            sonuc->veri[(i * p) + j] = t;
        }
    }
    return true;
}

bool ida_mat_topla(ida_mat_oku_t lhs, ida_mat_oku_t rhs, const ida_mat_yaz_t *sonuc)
{
    if (!boyut_gecerli(lhs.boyut) || (lhs.boyut.satir != rhs.boyut.satir) ||
        (lhs.boyut.sutun != rhs.boyut.sutun) || (sonuc->boyut.satir != lhs.boyut.satir) ||
        (sonuc->boyut.sutun != lhs.boyut.sutun)) {
        return false;
    }
    const uint32_t say = (uint32_t)lhs.boyut.satir * (uint32_t)lhs.boyut.sutun;
    for (uint32_t i = 0u; i < say; i++) {
        sonuc->veri[i] = lhs.veri[i] + rhs.veri[i];
    }
    return true;
}

bool ida_mat_devrik(ida_mat_oku_t a, const ida_mat_yaz_t *sonuc)
{
    if (!boyut_gecerli(a.boyut) || (sonuc->boyut.satir != a.boyut.sutun) ||
        (sonuc->boyut.sutun != a.boyut.satir)) {
        return false;
    }
    const uint32_t m = a.boyut.satir;
    const uint32_t n = a.boyut.sutun;
    for (uint32_t i = 0u; i < m; i++) {
        for (uint32_t j = 0u; j < n; j++) {
            sonuc->veri[(j * m) + i] = a.veri[(i * n) + j];
        }
    }
    return true;
}

bool ida_mat_apat(ida_mat_oku_t a, ida_mat_oku_t p, const ida_mat_yaz_t *ara,
                  const ida_mat_yaz_t *sonuc)
{
    if (!boyut_gecerli(a.boyut) || (p.boyut.satir != a.boyut.sutun) ||
        (p.boyut.sutun != a.boyut.sutun) || (sonuc->boyut.satir != a.boyut.satir) ||
        (sonuc->boyut.sutun != a.boyut.satir) || (ara->boyut.satir != a.boyut.satir) ||
        (ara->boyut.sutun != a.boyut.sutun)) {
        return false;
    }
    /* ara = A · P */
    const ida_mat_oku_t ap_lhs = a;
    if (!ida_mat_carp(ap_lhs, p, ara)) {
        return false;
    }
    /* sonuc = ara · Aᵀ: (ara)_ik · A_jk */
    const uint32_t m = a.boyut.satir;
    const uint32_t n = a.boyut.sutun;
    for (uint32_t i = 0u; i < m; i++) {
        for (uint32_t j = 0u; j < m; j++) {
            float t = 0.0f;
            for (uint32_t k = 0u; k < n; k++) {
                t += ara->veri[(i * n) + k] * a.veri[(j * n) + k];
            }
            sonuc->veri[(i * m) + j] = t;
        }
    }
    return true;
}
