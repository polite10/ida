// İç yardımcılar: kurulum zamanı doğrusal cebiri için Eigen sabit boyutlu tipler ve dönüşümler
// (adım içi sıcak yol vek.hpp'dedir).
#pragma once

#include "ida/fizik/parametreler.hpp"

#include <Eigen/Dense>

namespace ida::fizik::ic {

using V3 = Eigen::Vector3d;
using V6 = Eigen::Matrix<double, 6, 1>;
using M3 = Eigen::Matrix3d;
using M6 = Eigen::Matrix<double, 6, 6>;

inline V3 v3(const Vek3& a) { return V3(a[0], a[1], a[2]); }
inline Vek3 dizi(const V3& a) { return {a(0), a(1), a(2)}; }
inline M3 m3(const Mat3& a)
{
    M3 m;
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) {
            m(i, j) = a[static_cast<std::size_t>(i)][static_cast<std::size_t>(j)];
        }
    }
    return m;
}
inline Mat3 dizi(const M3& m)
{
    Mat3 a{};
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) {
            a[static_cast<std::size_t>(i)][static_cast<std::size_t>(j)] = m(i, j);
        }
    }
    return a;
}

// Çarpık simetrik matris: S(a) b = a × b.
inline M3 S(const V3& a)
{
    M3 m;
    m << 0.0, -a(2), a(1),
         a(2), 0.0, -a(0),
         -a(1), a(0), 0.0;
    return m;
}

}  // namespace ida::fizik::ic
