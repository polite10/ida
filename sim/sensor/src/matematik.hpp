// sim/sensor iç matematik yardımcıları (double; Eigen kullanılmaz).
//
// Kabuller iskelet §3.1: kuaterniyon (w, x, y, z), Hamilton, gövdeden dünyaya (NED) dönüş;
// R(q) v gövde vektörünü NED'e, R(q)ᵀ v NED vektörünü gövdeye taşır.
#pragma once

#include "ida/fizik/parametreler.hpp"

#include <array>
#include <cmath>

namespace ida::sensor::ic {

using fizik::Kuaterniyon;
using fizik::Mat3;
using fizik::Vek3;

inline Vek3 topla(const Vek3& a, const Vek3& b) { return {a[0] + b[0], a[1] + b[1], a[2] + b[2]}; }
inline Vek3 cikar(const Vek3& a, const Vek3& b) { return {a[0] - b[0], a[1] - b[1], a[2] - b[2]}; }
inline Vek3 olcekle(const Vek3& a, double s) { return {a[0] * s, a[1] * s, a[2] * s}; }
inline double nokta(const Vek3& a, const Vek3& b) { return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]; }
inline double norm(const Vek3& a) { return std::sqrt(nokta(a, a)); }
inline Vek3 capraz(const Vek3& a, const Vek3& b)
{
    return {a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]};
}

// Kuaterniyonun dönüş matrisi (satır dizisi).
inline Mat3 matris(const Kuaterniyon& q)
{
    const double w = q[0], x = q[1], y = q[2], z = q[3];
    Mat3 r{};
    r[0] = {1.0 - 2.0 * (y * y + z * z), 2.0 * (x * y - w * z), 2.0 * (x * z + w * y)};
    r[1] = {2.0 * (x * y + w * z), 1.0 - 2.0 * (x * x + z * z), 2.0 * (y * z - w * x)};
    r[2] = {2.0 * (x * z - w * y), 2.0 * (y * z + w * x), 1.0 - 2.0 * (x * x + y * y)};
    return r;
}

inline Vek3 carp(const Mat3& m, const Vek3& v)
{
    return {nokta(m[0], v), nokta(m[1], v), nokta(m[2], v)};
}

inline Vek3 carp_t(const Mat3& m, const Vek3& v)  // mᵀ v
{
    return {m[0][0] * v[0] + m[1][0] * v[1] + m[2][0] * v[2],
            m[0][1] * v[0] + m[1][1] * v[1] + m[2][1] * v[2],
            m[0][2] * v[0] + m[1][2] * v[1] + m[2][2] * v[2]};
}

inline Mat3 carp(const Mat3& a, const Mat3& b)
{
    Mat3 r{};
    for (std::size_t i = 0; i < 3; ++i) {
        for (std::size_t j = 0; j < 3; ++j) {
            r[i][j] = a[i][0] * b[0][j] + a[i][1] * b[1][j] + a[i][2] * b[2][j];
        }
    }
    return r;
}

inline Mat3 birim_matris()
{
    Mat3 r{};
    r[0][0] = r[1][1] = r[2][2] = 1.0;
    return r;
}

// 3×3 ters; tekilse false.
inline bool ters(const Mat3& m, Mat3& t)
{
    const double c00 = m[1][1] * m[2][2] - m[1][2] * m[2][1];
    const double c01 = m[1][2] * m[2][0] - m[1][0] * m[2][2];
    const double c02 = m[1][0] * m[2][1] - m[1][1] * m[2][0];
    const double det = m[0][0] * c00 + m[0][1] * c01 + m[0][2] * c02;
    if (!(std::abs(det) > 0.0) || !std::isfinite(det)) {
        return false;
    }
    const double id = 1.0 / det;
    t[0] = {c00 * id, (m[0][2] * m[2][1] - m[0][1] * m[2][2]) * id,
            (m[0][1] * m[1][2] - m[0][2] * m[1][1]) * id};
    t[1] = {c01 * id, (m[0][0] * m[2][2] - m[0][2] * m[2][0]) * id,
            (m[0][2] * m[1][0] - m[0][0] * m[1][2]) * id};
    t[2] = {c02 * id, (m[0][1] * m[2][0] - m[0][0] * m[2][1]) * id,
            (m[0][0] * m[1][1] - m[0][1] * m[1][0]) * id};
    return true;
}

inline Kuaterniyon normalle(const Kuaterniyon& q)
{
    const double n = std::sqrt(q[0] * q[0] + q[1] * q[1] + q[2] * q[2] + q[3] * q[3]);
    return {q[0] / n, q[1] / n, q[2] / n, q[3] / n};
}

inline bool sonlu(const Vek3& v)
{
    return std::isfinite(v[0]) && std::isfinite(v[1]) && std::isfinite(v[2]);
}

}  // namespace ida::sensor::ic
