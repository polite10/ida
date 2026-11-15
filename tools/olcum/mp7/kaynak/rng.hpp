// Sayaç tabanlı RNG: Philox4x32-10 (Salmon vd. 2011, Random123).
// 05 §2.6: anahtar = özet(tohum, alt sistem, sensör, akış); sayaç çekilişin yeri.
#pragma once
#include <array>
#include <cmath>
#include <complex>
#include <cstdint>

namespace mp7 {

using Ctr = std::array<uint32_t, 4>;
using Key = std::array<uint32_t, 2>;

inline Ctr philox(Ctr c, Key k) {
  const uint32_t M0 = 0xD2511F53u, M1 = 0xCD9E8D57u;
  const uint32_t W0 = 0x9E3779B9u, W1 = 0xBB67AE85u;
  for (int r = 0; r < 10; ++r) {
    const uint64_t p0 = uint64_t(M0) * c[0];
    const uint64_t p1 = uint64_t(M1) * c[2];
    const uint32_t hi0 = uint32_t(p0 >> 32), lo0 = uint32_t(p0);
    const uint32_t hi1 = uint32_t(p1 >> 32), lo1 = uint32_t(p1);
    c = {hi1 ^ c[1] ^ k[0], lo1, hi0 ^ c[3] ^ k[1], lo0};
    k[0] += W0;
    k[1] += W1;
  }
  return c;
}

inline uint64_t splitmix64(uint64_t x) {
  x += 0x9E3779B97F4A7C15ull;
  x = (x ^ (x >> 30)) * 0xBF58476D1CE4E5B9ull;
  x = (x ^ (x >> 27)) * 0x94D049BB133111EBull;
  return x ^ (x >> 31);
}

// Akış kimlikleri (05 §2.6 adları)
enum class Akis : uint32_t { olcum = 1, sonar_isin = 5, sonar_hucre = 6, sahne = 100, bootstrap = 101 };

inline Key anahtar(uint64_t tohum, uint32_t alt_sistem, uint32_t sensor, Akis akis) {
  uint64_t h = splitmix64(tohum);
  h = splitmix64(h ^ alt_sistem);
  h = splitmix64(h ^ (uint64_t(sensor) << 8));
  h = splitmix64(h ^ (uint64_t(akis) << 16));
  return {uint32_t(h), uint32_t(h >> 32)};
}

// (0,1) açık aralıkta düzgün
inline double duzgun(uint32_t x) { return (double(x) + 0.5) * (1.0 / 4294967296.0); }

// CN(0,1): E|g|^2 = 1 (Box-Muller; aynı ikili ve platformda tekrar)
inline std::complex<double> karmasik_normal(uint32_t a, uint32_t b) {
  const double r = std::sqrt(-std::log(duzgun(a)));
  const double t = 6.283185307179586 * duzgun(b);
  return {r * std::cos(t), r * std::sin(t)};
}

inline double ustel(uint32_t a) { return -std::log(duzgun(a)); }

}  // namespace mp7
