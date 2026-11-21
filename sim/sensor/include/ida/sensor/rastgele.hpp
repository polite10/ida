// A5 Ek D rastgelelik sözleşmesi (sürüm 1): SHA-256 anahtar türetimi, Philox4x32-10 ve
// dönüşümler (A5-K83, K110, K111).
//
// Alt akış `Akis(ana_tohum, alt_sistem_adi)`: anahtar
//   K = SHA-256("IDA-A5-RNG-1" ‖ 0x00 ‖ LE64(ana_tohum) ‖ UTF8(alt_sistem_adi) ‖ 0x00),
//   k0 = LE32(K[0..3]), k1 = LE32(K[4..7]).
// Sayaç: c0 = n mod 2³², c1 = ⌊n / 2³²⌋, c2 = (kanal << 24) | (bilesen << 16) | cekilis, c3 = 0.
// Her çekilişin yeri (n, c2) sabittir; bir akışın çekiliş sayısı başka bir çekilişi kaydırmaz.
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace ida::sensor {

std::array<std::uint8_t, 32> sha256(const std::uint8_t* veri, std::size_t boy);

// Philox4x32-10 (A5 Ek D.1).
std::array<std::uint32_t, 4> philox4x32_10(const std::array<std::uint32_t, 4>& sayac,
                                           const std::array<std::uint32_t, 2>& anahtar);

// A5-K111 düzgün dönüşümü: (x + 0,5) · 2⁻³², (0, 1) açık aralık, binary64'te tam.
inline double duzgun_donusum(std::uint32_t x)
{
    return (static_cast<double>(x) + 0.5) * (1.0 / 4294967296.0);
}

inline constexpr std::uint32_t c2_kodla(std::uint32_t kanal, std::uint32_t bilesen,
                                        std::uint32_t cekilis)
{
    return ((kanal & 0xFFu) << 24) | ((bilesen & 0xFFu) << 16) | (cekilis & 0xFFFFu);
}

class Akis {
public:
    Akis() = default;
    Akis(std::uint64_t ana_tohum, std::string_view alt_sistem_adi);

    const std::array<std::uint32_t, 2>& anahtar() const { return k_; }
    std::array<std::uint32_t, 4> sozcukler(std::uint64_t n, std::uint32_t c2) const;
    // u(w0)
    double duzgun(std::uint64_t n, std::uint32_t c2) const;
    // Box–Muller z0 (w0, w1); A5-K111 işlem sırası.
    double normal(std::uint64_t n, std::uint32_t c2) const;
    // −log(u(w0))
    double ustel(std::uint64_t n, std::uint32_t c2) const;

private:
    std::array<std::uint32_t, 2> k_{};
};

// Box–Muller (A5-K111): r = sqrt(−2,0 · log(u_a)), t = 6,283185307179586 · u_b, z0 = r cos t.
double box_muller_z0(std::uint32_t wa, std::uint32_t wb);

}  // namespace ida::sensor
