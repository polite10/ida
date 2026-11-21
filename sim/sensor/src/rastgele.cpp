// A5 Ek D: SHA-256 (FIPS 180-4), Philox4x32-10 ve dönüşümler.
#include "ida/sensor/rastgele.hpp"

#include <cmath>
#include <vector>

namespace ida::sensor {

namespace {

constexpr std::array<std::uint32_t, 64> kSha256K = {
    0x428a2f98u, 0x71374491u, 0xb5c0fbcfu, 0xe9b5dba5u, 0x3956c25bu, 0x59f111f1u, 0x923f82a4u,
    0xab1c5ed5u, 0xd807aa98u, 0x12835b01u, 0x243185beu, 0x550c7dc3u, 0x72be5d74u, 0x80deb1feu,
    0x9bdc06a7u, 0xc19bf174u, 0xe49b69c1u, 0xefbe4786u, 0x0fc19dc6u, 0x240ca1ccu, 0x2de92c6fu,
    0x4a7484aau, 0x5cb0a9dcu, 0x76f988dau, 0x983e5152u, 0xa831c66du, 0xb00327c8u, 0xbf597fc7u,
    0xc6e00bf3u, 0xd5a79147u, 0x06ca6351u, 0x14292967u, 0x27b70a85u, 0x2e1b2138u, 0x4d2c6dfcu,
    0x53380d13u, 0x650a7354u, 0x766a0abbu, 0x81c2c92eu, 0x92722c85u, 0xa2bfe8a1u, 0xa81a664bu,
    0xc24b8b70u, 0xc76c51a3u, 0xd192e819u, 0xd6990624u, 0xf40e3585u, 0x106aa070u, 0x19a4c116u,
    0x1e376c08u, 0x2748774cu, 0x34b0bcb5u, 0x391c0cb3u, 0x4ed8aa4au, 0x5b9cca4fu, 0x682e6ff3u,
    0x748f82eeu, 0x78a5636fu, 0x84c87814u, 0x8cc70208u, 0x90befffau, 0xa4506cebu, 0xbef9a3f7u,
    0xc67178f2u};

constexpr std::uint32_t dondur_sag(std::uint32_t x, unsigned n)
{
    return (x >> n) | (x << (32u - n));
}

std::uint32_t le32(const std::uint8_t* b)
{
    return static_cast<std::uint32_t>(b[0]) | (static_cast<std::uint32_t>(b[1]) << 8) |
           (static_cast<std::uint32_t>(b[2]) << 16) | (static_cast<std::uint32_t>(b[3]) << 24);
}

}  // namespace

std::array<std::uint8_t, 32> sha256(const std::uint8_t* veri, std::size_t boy)
{
    std::array<std::uint32_t, 8> h = {0x6a09e667u, 0xbb67ae85u, 0x3c6ef372u, 0xa54ff53au,
                                      0x510e527fu, 0x9b05688cu, 0x1f83d9abu, 0x5be0cd19u};
    std::vector<std::uint8_t> m(veri, veri + boy);
    const std::uint64_t bit_boyu = static_cast<std::uint64_t>(boy) * 8u;
    m.push_back(0x80u);
    while ((m.size() % 64u) != 56u) {
        m.push_back(0u);
    }
    for (int i = 7; i >= 0; --i) {
        m.push_back(static_cast<std::uint8_t>((bit_boyu >> (8 * i)) & 0xFFu));
    }
    for (std::size_t blok = 0; blok < m.size(); blok += 64u) {
        std::array<std::uint32_t, 64> w{};
        for (std::size_t i = 0; i < 16u; ++i) {
            const std::uint8_t* b = &m[blok + 4u * i];
            w[i] = (static_cast<std::uint32_t>(b[0]) << 24) | (static_cast<std::uint32_t>(b[1]) << 16) |
                   (static_cast<std::uint32_t>(b[2]) << 8) | static_cast<std::uint32_t>(b[3]);
        }
        for (std::size_t i = 16; i < 64u; ++i) {
            const std::uint32_t s0 =
                dondur_sag(w[i - 15], 7) ^ dondur_sag(w[i - 15], 18) ^ (w[i - 15] >> 3);
            const std::uint32_t s1 =
                dondur_sag(w[i - 2], 17) ^ dondur_sag(w[i - 2], 19) ^ (w[i - 2] >> 10);
            w[i] = w[i - 16] + s0 + w[i - 7] + s1;
        }
        std::uint32_t a = h[0], b = h[1], c = h[2], d = h[3], e = h[4], f = h[5], g = h[6],
                      hh = h[7];
        for (std::size_t i = 0; i < 64u; ++i) {
            const std::uint32_t S1 = dondur_sag(e, 6) ^ dondur_sag(e, 11) ^ dondur_sag(e, 25);
            const std::uint32_t ch = (e & f) ^ (~e & g);
            const std::uint32_t t1 = hh + S1 + ch + kSha256K[i] + w[i];
            const std::uint32_t S0 = dondur_sag(a, 2) ^ dondur_sag(a, 13) ^ dondur_sag(a, 22);
            const std::uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
            const std::uint32_t t2 = S0 + maj;
            hh = g;
            g = f;
            f = e;
            e = d + t1;
            d = c;
            c = b;
            b = a;
            a = t1 + t2;
        }
        h[0] += a;
        h[1] += b;
        h[2] += c;
        h[3] += d;
        h[4] += e;
        h[5] += f;
        h[6] += g;
        h[7] += hh;
    }
    std::array<std::uint8_t, 32> ozet{};
    for (std::size_t i = 0; i < 8u; ++i) {
        ozet[4u * i] = static_cast<std::uint8_t>(h[i] >> 24);
        ozet[4u * i + 1u] = static_cast<std::uint8_t>((h[i] >> 16) & 0xFFu);
        ozet[4u * i + 2u] = static_cast<std::uint8_t>((h[i] >> 8) & 0xFFu);
        ozet[4u * i + 3u] = static_cast<std::uint8_t>(h[i] & 0xFFu);
    }
    return ozet;
}

std::array<std::uint32_t, 4> philox4x32_10(const std::array<std::uint32_t, 4>& sayac,
                                           const std::array<std::uint32_t, 2>& anahtar)
{
    constexpr std::uint64_t kM0 = 0xD2511F53u;
    constexpr std::uint64_t kM1 = 0xCD9E8D57u;
    constexpr std::uint32_t kW0 = 0x9E3779B9u;
    constexpr std::uint32_t kW1 = 0xBB67AE85u;
    std::array<std::uint32_t, 4> c = sayac;
    std::uint32_t k0 = anahtar[0];
    std::uint32_t k1 = anahtar[1];
    for (int tur = 0; tur < 10; ++tur) {
        const std::uint64_t p0 = kM0 * c[0];
        const std::uint64_t p1 = kM1 * c[2];
        const std::uint32_t hi0 = static_cast<std::uint32_t>(p0 >> 32);
        const std::uint32_t lo0 = static_cast<std::uint32_t>(p0 & 0xFFFFFFFFu);
        const std::uint32_t hi1 = static_cast<std::uint32_t>(p1 >> 32);
        const std::uint32_t lo1 = static_cast<std::uint32_t>(p1 & 0xFFFFFFFFu);
        c = {hi1 ^ c[1] ^ k0, lo1, hi0 ^ c[3] ^ k1, lo0};
        k0 += kW0;
        k1 += kW1;
    }
    return c;
}

double box_muller_z0(std::uint32_t wa, std::uint32_t wb)
{
    const double ua = duzgun_donusum(wa);
    const double ub = duzgun_donusum(wb);
    const double r = std::sqrt(-2.0 * std::log(ua));
    const double t = 6.283185307179586 * ub;
    return r * std::cos(t);
}

Akis::Akis(std::uint64_t ana_tohum, std::string_view alt_sistem_adi)
{
    std::vector<std::uint8_t> girdi;
    constexpr std::string_view kEtiket = "IDA-A5-RNG-1";
    girdi.insert(girdi.end(), kEtiket.begin(), kEtiket.end());
    girdi.push_back(0u);
    for (int i = 0; i < 8; ++i) {
        girdi.push_back(static_cast<std::uint8_t>((ana_tohum >> (8 * i)) & 0xFFu));
    }
    girdi.insert(girdi.end(), alt_sistem_adi.begin(), alt_sistem_adi.end());
    girdi.push_back(0u);
    const std::array<std::uint8_t, 32> k = sha256(girdi.data(), girdi.size());
    k_ = {le32(&k[0]), le32(&k[4])};
}

std::array<std::uint32_t, 4> Akis::sozcukler(std::uint64_t n, std::uint32_t c2) const
{
    const std::array<std::uint32_t, 4> sayac = {static_cast<std::uint32_t>(n & 0xFFFFFFFFu),
                                                static_cast<std::uint32_t>(n >> 32), c2, 0u};
    return philox4x32_10(sayac, k_);
}

double Akis::duzgun(std::uint64_t n, std::uint32_t c2) const
{
    return duzgun_donusum(sozcukler(n, c2)[0]);
}

double Akis::normal(std::uint64_t n, std::uint32_t c2) const
{
    const auto w = sozcukler(n, c2);
    return box_muller_z0(w[0], w[1]);
}

double Akis::ustel(std::uint64_t n, std::uint32_t c2) const
{
    return -std::log(duzgun(n, c2));
}

}  // namespace ida::sensor
