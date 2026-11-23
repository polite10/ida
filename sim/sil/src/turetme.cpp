// turetme.cpp — SHA-256, Philox4x32-10 ve A5 anahtar türetimi (turetme.hpp).
#include "turetme.hpp"

#include <vector>

namespace ida::sil::turetme {

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

constexpr std::uint32_t dondur(std::uint32_t x, unsigned r)
{
    return (x >> r) | (x << (32u - r));
}

void blok_isle(std::array<std::uint32_t, 8>& h, const std::uint8_t* p)
{
    std::array<std::uint32_t, 64> w{};
    for (std::size_t i = 0; i < 16u; ++i) {
        w[i] = (static_cast<std::uint32_t>(p[4u * i]) << 24) |
               (static_cast<std::uint32_t>(p[4u * i + 1u]) << 16) |
               (static_cast<std::uint32_t>(p[4u * i + 2u]) << 8) |
               static_cast<std::uint32_t>(p[4u * i + 3u]);
    }
    for (std::size_t i = 16u; i < 64u; ++i) {
        const std::uint32_t s0 = dondur(w[i - 15u], 7u) ^ dondur(w[i - 15u], 18u) ^ (w[i - 15u] >> 3);
        const std::uint32_t s1 = dondur(w[i - 2u], 17u) ^ dondur(w[i - 2u], 19u) ^ (w[i - 2u] >> 10);
        w[i] = w[i - 16u] + s0 + w[i - 7u] + s1;
    }
    std::uint32_t a = h[0], b = h[1], c = h[2], d = h[3];
    std::uint32_t e = h[4], f = h[5], g = h[6], hh = h[7];
    for (std::size_t i = 0; i < 64u; ++i) {
        const std::uint32_t S1 = dondur(e, 6u) ^ dondur(e, 11u) ^ dondur(e, 25u);
        const std::uint32_t ch = (e & f) ^ (~e & g);
        const std::uint32_t t1 = hh + S1 + ch + kK[i] + w[i];
        const std::uint32_t S0 = dondur(a, 2u) ^ dondur(a, 13u) ^ dondur(a, 22u);
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

std::uint32_t le32(const std::uint8_t* p)
{
    return static_cast<std::uint32_t>(p[0]) | (static_cast<std::uint32_t>(p[1]) << 8) |
           (static_cast<std::uint32_t>(p[2]) << 16) | (static_cast<std::uint32_t>(p[3]) << 24);
}

}  // namespace

std::array<std::uint8_t, 32> sha256(const std::uint8_t* veri, std::size_t n)
{
    std::array<std::uint32_t, 8> h = {0x6a09e667u, 0xbb67ae85u, 0x3c6ef372u, 0xa54ff53au,
                                      0x510e527fu, 0x9b05688cu, 0x1f83d9abu, 0x5be0cd19u};
    std::size_t i = 0;
    for (; i + 64u <= n; i += 64u) {
        blok_isle(h, veri + i);
    }
    std::array<std::uint8_t, 128> son{};
    const std::size_t kalan = n - i;
    for (std::size_t j = 0; j < kalan; ++j) {
        son[j] = veri[i + j];
    }
    son[kalan] = 0x80u;
    const std::size_t toplam = (kalan + 1u + 8u <= 64u) ? 64u : 128u;
    const std::uint64_t bit = static_cast<std::uint64_t>(n) * 8u;
    for (std::size_t j = 0; j < 8u; ++j) {
        son[toplam - 1u - j] = static_cast<std::uint8_t>((bit >> (8u * j)) & 0xFFu);
    }
    blok_isle(h, son.data());
    if (toplam == 128u) {
        blok_isle(h, son.data() + 64);
    }
    std::array<std::uint8_t, 32> ozet{};
    for (std::size_t j = 0; j < 8u; ++j) {
        ozet[4u * j] = static_cast<std::uint8_t>(h[j] >> 24);
        ozet[4u * j + 1u] = static_cast<std::uint8_t>((h[j] >> 16) & 0xFFu);
        ozet[4u * j + 2u] = static_cast<std::uint8_t>((h[j] >> 8) & 0xFFu);
        ozet[4u * j + 3u] = static_cast<std::uint8_t>(h[j] & 0xFFu);
    }
    return ozet;
}

std::array<std::uint32_t, 4> philox(std::array<std::uint32_t, 4> c, std::array<std::uint32_t, 2> k)
{
    constexpr std::uint64_t kM0 = 0xD2511F53u;
    constexpr std::uint64_t kM1 = 0xCD9E8D57u;
    constexpr std::uint32_t kW0 = 0x9E3779B9u;
    constexpr std::uint32_t kW1 = 0xBB67AE85u;
    for (int tur = 0; tur < 10; ++tur) {
        const std::uint64_t p0 = kM0 * c[0];
        const std::uint64_t p1 = kM1 * c[2];
        const std::array<std::uint32_t, 4> y = {
            static_cast<std::uint32_t>(p1 >> 32) ^ c[1] ^ k[0],
            static_cast<std::uint32_t>(p1 & 0xFFFFFFFFu),
            static_cast<std::uint32_t>(p0 >> 32) ^ c[3] ^ k[1],
            static_cast<std::uint32_t>(p0 & 0xFFFFFFFFu)};
        c = y;
        k[0] += kW0;
        k[1] += kW1;
    }
    return c;
}

std::array<std::uint32_t, 2> anahtar(std::uint64_t tohum, std::string_view ad)
{
    static constexpr char kOnek[] = "IDA-A5-RNG-1";
    std::vector<std::uint8_t> g;
    g.reserve(sizeof kOnek + 8u + ad.size() + 1u);
    for (std::size_t i = 0; i + 1u < sizeof kOnek; ++i) {
        g.push_back(static_cast<std::uint8_t>(kOnek[i]));
    }
    g.push_back(0u);
    for (unsigned j = 0; j < 8u; ++j) {
        g.push_back(static_cast<std::uint8_t>((tohum >> (8u * j)) & 0xFFu));
    }
    for (const char ch : ad) {
        g.push_back(static_cast<std::uint8_t>(ch));
    }
    g.push_back(0u);
    const auto ozet = sha256(g.data(), g.size());
    return {le32(ozet.data()), le32(ozet.data() + 4)};
}

std::array<std::uint32_t, 4> cek(std::uint64_t tohum, std::string_view ad, std::uint64_t n,
                                 std::uint32_t c2)
{
    const std::array<std::uint32_t, 4> sayac = {static_cast<std::uint32_t>(n & 0xFFFFFFFFu),
                                                static_cast<std::uint32_t>(n >> 32), c2, 0u};
    return philox(sayac, anahtar(tohum, ad));
}

std::uint32_t u32_sifirsiz(std::uint64_t tohum, std::string_view ad)
{
    const auto k = anahtar(tohum, ad);
    for (std::uint64_t n = 0;; ++n) {
        const std::array<std::uint32_t, 4> sayac = {static_cast<std::uint32_t>(n & 0xFFFFFFFFu),
                                                    static_cast<std::uint32_t>(n >> 32), 0u, 0u};
        const std::uint32_t w0 = philox(sayac, k)[0];
        if (w0 != 0u) {
            return w0;
        }
    }
}

std::uint64_t u64(std::uint64_t tohum, std::string_view ad)
{
    const auto w = cek(tohum, ad, 0u);
    return static_cast<std::uint64_t>(w[0]) | (static_cast<std::uint64_t>(w[1]) << 32);
}

std::string nonce_board_etiketi(std::uint32_t boot_count, std::uint32_t deneme)
{
    return "ida/a1/nonce_board/" + std::to_string(boot_count) + "/" + std::to_string(deneme);
}

}  // namespace ida::sil::turetme
