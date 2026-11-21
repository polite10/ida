// Sıcak yol için en küçük 3B vektör ve matris tipleri.
//
// Adım başına beş kez değerlendirilen türev işlevi ve hidrostatik kırpma bu tiplerle yazılır.
// Gerekçe (ölçüm, bu Mac, Apple Clang 17): aynı kod Eigen ifade şablonlarıyla eniyileştirmesiz
// derlemede (host-debug) adım başına ≈ 175 µs, eniyileştirmeli derlemede ≈ 0,93 µs sürdü;
// CI'daki host-debug ve host-asan testleri eniyileştirmesiz koşar. Kurulumdaki doğrusal cebir
// (özdeğer, rank, Cholesky, ters) Eigen'de kalır.
#pragma once

#include <cmath>

namespace ida::fizik::ic {

struct V {
    double x = 0.0, y = 0.0, z = 0.0;
};

inline V operator+(const V& a, const V& b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
inline V operator-(const V& a, const V& b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
inline V operator-(const V& a) { return {-a.x, -a.y, -a.z}; }
inline V operator*(double k, const V& a) { return {k * a.x, k * a.y, k * a.z}; }
inline V operator/(const V& a, double k) { return {a.x / k, a.y / k, a.z / k}; }
inline V& operator+=(V& a, const V& b)
{
    a.x += b.x;
    a.y += b.y;
    a.z += b.z;
    return a;
}
inline double nokta(const V& a, const V& b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
inline V capraz(const V& a, const V& b)
{
    return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}
inline V carp(const V& a, const V& b) { return {a.x * b.x, a.y * b.y, a.z * b.z}; }  // eleman eleman

// 3×3 matris, satır düzeni.
struct M {
    double m[3][3] = {{0, 0, 0}, {0, 0, 0}, {0, 0, 0}};
};
inline V operator*(const M& a, const V& v)
{
    return {a.m[0][0] * v.x + a.m[0][1] * v.y + a.m[0][2] * v.z,
            a.m[1][0] * v.x + a.m[1][1] * v.y + a.m[1][2] * v.z,
            a.m[2][0] * v.x + a.m[2][1] * v.y + a.m[2][2] * v.z};
}
// Aᵀ v
inline V devrik_carp(const M& a, const V& v)
{
    return {a.m[0][0] * v.x + a.m[1][0] * v.y + a.m[2][0] * v.z,
            a.m[0][1] * v.x + a.m[1][1] * v.y + a.m[2][1] * v.z,
            a.m[0][2] * v.x + a.m[1][2] * v.y + a.m[2][2] * v.z};
}

// Birim olmayabilen kuaterniyondan (w, x, y, z) gövde→dünya dönüş matrisi; normalleştirerek.
inline M donus(double w0, double x0, double y0, double z0)
{
    const double n = std::sqrt(w0 * w0 + x0 * x0 + y0 * y0 + z0 * z0);
    const double w = w0 / n, x = x0 / n, y = y0 / n, z = z0 / n;
    M r;
    r.m[0][0] = 1.0 - 2.0 * (y * y + z * z);
    r.m[0][1] = 2.0 * (x * y - w * z);
    r.m[0][2] = 2.0 * (x * z + w * y);
    r.m[1][0] = 2.0 * (x * y + w * z);
    r.m[1][1] = 1.0 - 2.0 * (x * x + z * z);
    r.m[1][2] = 2.0 * (y * z - w * x);
    r.m[2][0] = 2.0 * (x * z - w * y);
    r.m[2][1] = 2.0 * (y * z + w * x);
    r.m[2][2] = 1.0 - 2.0 * (x * x + y * y);
    return r;
}

}  // namespace ida::fizik::ic
