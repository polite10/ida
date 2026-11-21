// SHA-256 (FIPS 180-4). Davranış özeti (A5-K17), kaynak özetleri ve A7 manifest özeti
// (A7-K05) için. (`sim/sensor` ve `sim/sil` de birer kopya taşır; ortak yere toplanması
// ayrı iştir.)
#include "ida/senaryo/senaryo.hpp"

#include <array>
#include <cstdint>

namespace ida::senaryo {

namespace {

constexpr std::array<std::uint32_t, 64> kK = {
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

constexpr std::uint32_t dondur(std::uint32_t x, unsigned n)
{
    return (x >> n) | (x << (32u - n));
}

void blok_isle(std::array<std::uint32_t, 8>& h, const unsigned char* p)
{
    std::array<std::uint32_t, 64> w{};
    for (std::size_t i = 0; i < 16; ++i) {
        w[i] = (static_cast<std::uint32_t>(p[4 * i]) << 24) |
               (static_cast<std::uint32_t>(p[4 * i + 1]) << 16) |
               (static_cast<std::uint32_t>(p[4 * i + 2]) << 8) |
               static_cast<std::uint32_t>(p[4 * i + 3]);
    }
    for (std::size_t i = 16; i < 64; ++i) {
        const std::uint32_t s0 = dondur(w[i - 15], 7) ^ dondur(w[i - 15], 18) ^ (w[i - 15] >> 3);
        const std::uint32_t s1 = dondur(w[i - 2], 17) ^ dondur(w[i - 2], 19) ^ (w[i - 2] >> 10);
        w[i] = w[i - 16] + s0 + w[i - 7] + s1;
    }
    std::uint32_t a = h[0], b = h[1], c = h[2], d = h[3], e = h[4], f = h[5], g = h[6], k = h[7];
    for (std::size_t i = 0; i < 64; ++i) {
        const std::uint32_t S1 = dondur(e, 6) ^ dondur(e, 11) ^ dondur(e, 25);
        const std::uint32_t ch = (e & f) ^ (~e & g);
        const std::uint32_t t1 = k + S1 + ch + kK[i] + w[i];
        const std::uint32_t S0 = dondur(a, 2) ^ dondur(a, 13) ^ dondur(a, 22);
        const std::uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
        const std::uint32_t t2 = S0 + maj;
        k = g;
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
    h[7] += k;
}

}  // namespace

std::string sha256_hex(std::string_view baytlar)
{
    std::array<std::uint32_t, 8> h = {0x6a09e667u, 0xbb67ae85u, 0x3c6ef372u, 0xa54ff53au,
                                      0x510e527fu, 0x9b05688cu, 0x1f83d9abu, 0x5be0cd19u};
    const auto* p = reinterpret_cast<const unsigned char*>(baytlar.data());
    const std::size_t n = baytlar.size();
    std::size_t i = 0;
    for (; i + 64 <= n; i += 64) {
        blok_isle(h, p + i);
    }
    std::array<unsigned char, 128> son{};
    const std::size_t kalan = n - i;
    for (std::size_t j = 0; j < kalan; ++j) {
        son[j] = p[i + j];
    }
    son[kalan] = 0x80u;
    const std::size_t toplam = (kalan + 1 + 8 <= 64) ? 64 : 128;
    const std::uint64_t bit = static_cast<std::uint64_t>(n) * 8u;
    for (std::size_t j = 0; j < 8; ++j) {
        son[toplam - 1 - j] = static_cast<unsigned char>((bit >> (8u * j)) & 0xFFu);
    }
    blok_isle(h, son.data());
    if (toplam == 128) {
        blok_isle(h, son.data() + 64);
    }
    static constexpr char kOnaltilik[] = "0123456789abcdef";
    std::string o;
    o.reserve(64);
    for (const std::uint32_t x : h) {
        for (int s = 28; s >= 0; s -= 4) {
            o += kOnaltilik[(x >> static_cast<unsigned>(s)) & 0xFu];
        }
    }
    return o;
}

}  // namespace ida::senaryo
