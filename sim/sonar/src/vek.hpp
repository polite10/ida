// sim/sonar iç vektör yardımcıları (double). Kabuller iskelet §3.1: kuaterniyon (w, x, y, z),
// Hamilton, gövdeden dünyaya (NED); `matris(q) · v` gövde vektörünü NED'e taşır.
#pragma once

#include "ida/fizik/parametreler.hpp"

#include <cmath>

namespace ida::sonar::ic {

using fizik::Kuaterniyon;
using fizik::Mat3;
using fizik::Vek3;

inline Vek3 topla(const Vek3& a, const Vek3& b) { return {a[0] + b[0], a[1] + b[1], a[2] + b[2]}; }
inline Vek3 cikar(const Vek3& a, const Vek3& b) { return {a[0] - b[0], a[1] - b[1], a[2] - b[2]}; }
inline Vek3 olcekle(const Vek3& a, double s) { return {a[0] * s, a[1] * s, a[2] * s}; }
inline double nokta(const Vek3& a, const Vek3& b) { return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]; }
inline double uzunluk(const Vek3& a) { return std::sqrt(nokta(a, a)); }
inline Vek3 capraz(const Vek3& a, const Vek3& b)
{
    return {a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]};
}

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

inline Vek3 carp_t(const Mat3& m, const Vek3& v)
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

inline Kuaterniyon normalle(const Kuaterniyon& q)
{
    const double n = std::sqrt(q[0] * q[0] + q[1] * q[1] + q[2] * q[2] + q[3] * q[3]);
    return {q[0] / n, q[1] / n, q[2] / n, q[3] / n};
}

inline bool sonlu(const Vek3& v)
{
    return std::isfinite(v[0]) && std::isfinite(v[1]) && std::isfinite(v[2]);
}

inline constexpr double kPi = 3.14159265358979323846;

}  // namespace ida::sonar::ic
