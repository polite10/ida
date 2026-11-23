// ozet.cpp — bkz. ozet.hpp.
#include "ozet.hpp"

#include <cstring>

namespace ida::a3::ozet {

namespace {

constexpr std::uint32_t sol(std::uint32_t x, unsigned n) noexcept { return (x << n) | (x >> (32u - n)); }
constexpr std::uint32_t sag(std::uint32_t x, unsigned n) noexcept { return (x >> n) | (x << (32u - n)); }

// Ortak Merkle–Damgård dolgusu: 64 baytlık bloklar, büyük uçlu bit uzunluğu.
template <typename Blok>
void dolgulu_isle(const std::uint8_t* veri, std::size_t n, Blok&& blok)
{
    std::size_t i = 0;
    for (; i + 64 <= n; i += 64) {
        blok(veri + i);
    }
    std::uint8_t son[128] = {};
    const std::size_t kalan = n - i;
    if (kalan > 0) {
        std::memcpy(son, veri + i, kalan);
    }
    son[kalan] = 0x80u;
    const std::size_t toplam = kalan + 1 + 8 <= 64 ? 64 : 128;
    const std::uint64_t bit = static_cast<std::uint64_t>(n) * 8u;
    for (std::size_t k = 0; k < 8; ++k) {
        son[toplam - 1 - k] = static_cast<std::uint8_t>((bit >> (8u * k)) & 0xFFu);
    }
    blok(son);
    if (toplam == 128) {
        blok(son + 64);
    }
}

std::uint32_t be32(const std::uint8_t* p) noexcept
{
    return (static_cast<std::uint32_t>(p[0]) << 24u) | (static_cast<std::uint32_t>(p[1]) << 16u) |
           (static_cast<std::uint32_t>(p[2]) << 8u) | static_cast<std::uint32_t>(p[3]);
}

void be32_yaz(std::uint32_t v, std::uint8_t* p) noexcept
{
    p[0] = static_cast<std::uint8_t>(v >> 24u);
    p[1] = static_cast<std::uint8_t>((v >> 16u) & 0xFFu);
    p[2] = static_cast<std::uint8_t>((v >> 8u) & 0xFFu);
    p[3] = static_cast<std::uint8_t>(v & 0xFFu);
}

constexpr std::uint32_t kK256[64] = {
    0x428a2f98u, 0x71374491u, 0xb5c0fbcfu, 0xe9b5dba5u, 0x3956c25bu, 0x59f111f1u, 0x923f82a4u, 0xab1c5ed5u,
    0xd807aa98u, 0x12835b01u, 0x243185beu, 0x550c7dc3u, 0x72be5d74u, 0x80deb1feu, 0x9bdc06a7u, 0xc19bf174u,
    0xe49b69c1u, 0xefbe4786u, 0x0fc19dc6u, 0x240ca1ccu, 0x2de92c6fu, 0x4a7484aau, 0x5cb0a9dcu, 0x76f988dau,
    0x983e5152u, 0xa831c66du, 0xb00327c8u, 0xbf597fc7u, 0xc6e00bf3u, 0xd5a79147u, 0x06ca6351u, 0x14292967u,
    0x27b70a85u, 0x2e1b2138u, 0x4d2c6dfcu, 0x53380d13u, 0x650a7354u, 0x766a0abbu, 0x81c2c92eu, 0x92722c85u,
    0xa2bfe8a1u, 0xa81a664bu, 0xc24b8b70u, 0xc76c51a3u, 0xd192e819u, 0xd6990624u, 0xf40e3585u, 0x106aa070u,
    0x19a4c116u, 0x1e376c08u, 0x2748774cu, 0x34b0bcb5u, 0x391c0cb3u, 0x4ed8aa4au, 0x5b9cca4fu, 0x682e6ff3u,
    0x748f82eeu, 0x78a5636fu, 0x84c87814u, 0x8cc70208u, 0x90befffau, 0xa4506cebu, 0xbef9a3f7u, 0xc67178f2u};

}  // namespace

std::array<std::uint8_t, 20> sha1(const std::uint8_t* veri, std::size_t n)
{
    std::uint32_t h[5] = {0x67452301u, 0xEFCDAB89u, 0x98BADCFEu, 0x10325476u, 0xC3D2E1F0u};
    dolgulu_isle(veri, n, [&h](const std::uint8_t* b) {
        std::uint32_t w[80];
        for (std::size_t t = 0; t < 16; ++t) {
            w[t] = be32(b + 4 * t);
        }
        for (std::size_t t = 16; t < 80; ++t) {
            w[t] = sol(w[t - 3] ^ w[t - 8] ^ w[t - 14] ^ w[t - 16], 1);
        }
        std::uint32_t a = h[0], bb = h[1], c = h[2], d = h[3], e = h[4];
        for (std::size_t t = 0; t < 80; ++t) {
            std::uint32_t f = 0;
            std::uint32_t k = 0;
            if (t < 20) {
                f = (bb & c) | (~bb & d);
                k = 0x5A827999u;
            } else if (t < 40) {
                f = bb ^ c ^ d;
                k = 0x6ED9EBA1u;
            } else if (t < 60) {
                f = (bb & c) | (bb & d) | (c & d);
                k = 0x8F1BBCDCu;
            } else {
                f = bb ^ c ^ d;
                k = 0xCA62C1D6u;
            }
            const std::uint32_t g = sol(a, 5) + f + e + k + w[t];
            e = d;
            d = c;
            c = sol(bb, 30);
            bb = a;
            a = g;
        }
        h[0] += a;
        h[1] += bb;
        h[2] += c;
        h[3] += d;
        h[4] += e;
    });
    std::array<std::uint8_t, 20> r{};
    for (std::size_t i = 0; i < 5; ++i) {
        be32_yaz(h[i], r.data() + 4 * i);
    }
    return r;
}

std::array<std::uint8_t, 32> sha256(const std::uint8_t* veri, std::size_t n)
{
    std::uint32_t h[8] = {0x6a09e667u, 0xbb67ae85u, 0x3c6ef372u, 0xa54ff53au,
                          0x510e527fu, 0x9b05688cu, 0x1f83d9abu, 0x5be0cd19u};
    dolgulu_isle(veri, n, [&h](const std::uint8_t* b) {
        std::uint32_t w[64];
        for (std::size_t t = 0; t < 16; ++t) {
            w[t] = be32(b + 4 * t);
        }
        for (std::size_t t = 16; t < 64; ++t) {
            const std::uint32_t s0 = sag(w[t - 15], 7) ^ sag(w[t - 15], 18) ^ (w[t - 15] >> 3u);
            const std::uint32_t s1 = sag(w[t - 2], 17) ^ sag(w[t - 2], 19) ^ (w[t - 2] >> 10u);
            w[t] = w[t - 16] + s0 + w[t - 7] + s1;
        }
        std::uint32_t a = h[0], bb = h[1], c = h[2], d = h[3], e = h[4], f = h[5], g = h[6], hh = h[7];
        for (std::size_t t = 0; t < 64; ++t) {
            const std::uint32_t S1 = sag(e, 6) ^ sag(e, 11) ^ sag(e, 25);
            const std::uint32_t ch = (e & f) ^ (~e & g);
            const std::uint32_t t1 = hh + S1 + ch + kK256[t] + w[t];
            const std::uint32_t S0 = sag(a, 2) ^ sag(a, 13) ^ sag(a, 22);
            const std::uint32_t maj = (a & bb) ^ (a & c) ^ (bb & c);
            const std::uint32_t t2 = S0 + maj;
            hh = g;
            g = f;
            f = e;
            e = d + t1;
            d = c;
            c = bb;
            bb = a;
            a = t1 + t2;
        }
        h[0] += a;
        h[1] += bb;
        h[2] += c;
        h[3] += d;
        h[4] += e;
        h[5] += f;
        h[6] += g;
        h[7] += hh;
    });
    std::array<std::uint8_t, 32> r{};
    for (std::size_t i = 0; i < 8; ++i) {
        be32_yaz(h[i], r.data() + 4 * i);
    }
    return r;
}

std::string base64(const std::uint8_t* veri, std::size_t n)
{
    static constexpr char kAbc[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string c;
    c.reserve((n + 2) / 3 * 4);
    std::size_t i = 0;
    for (; i + 3 <= n; i += 3) {
        const std::uint32_t v = (static_cast<std::uint32_t>(veri[i]) << 16u) |
                                (static_cast<std::uint32_t>(veri[i + 1]) << 8u) | veri[i + 2];
        c.push_back(kAbc[(v >> 18u) & 63u]);
        c.push_back(kAbc[(v >> 12u) & 63u]);
        c.push_back(kAbc[(v >> 6u) & 63u]);
        c.push_back(kAbc[v & 63u]);
    }
    if (n - i == 1) {
        const std::uint32_t v = static_cast<std::uint32_t>(veri[i]) << 16u;
        c.push_back(kAbc[(v >> 18u) & 63u]);
        c.push_back(kAbc[(v >> 12u) & 63u]);
        c += "==";
    } else if (n - i == 2) {
        const std::uint32_t v = (static_cast<std::uint32_t>(veri[i]) << 16u) | (static_cast<std::uint32_t>(veri[i + 1]) << 8u);
        c.push_back(kAbc[(v >> 18u) & 63u]);
        c.push_back(kAbc[(v >> 12u) & 63u]);
        c.push_back(kAbc[(v >> 6u) & 63u]);
        c.push_back('=');
    }
    return c;
}

bool base64_coz(std::string_view s, std::string& cikti)
{
    cikti.clear();
    if (s.size() % 4 != 0) {
        return false;
    }
    auto deger = [](char ch) -> int {
        if (ch >= 'A' && ch <= 'Z') return ch - 'A';
        if (ch >= 'a' && ch <= 'z') return ch - 'a' + 26;
        if (ch >= '0' && ch <= '9') return ch - '0' + 52;
        if (ch == '+') return 62;
        if (ch == '/') return 63;
        return -1;
    };
    for (std::size_t i = 0; i < s.size(); i += 4) {
        const bool son = i + 4 == s.size();
        int v[4] = {0, 0, 0, 0};
        int dolgu = 0;
        for (std::size_t k = 0; k < 4; ++k) {
            const char ch = s[i + k];
            if (ch == '=' && son && k >= 2) {
                ++dolgu;
                continue;
            }
            if (dolgu > 0) {
                return false;  // dolgudan sonra veri
            }
            v[k] = deger(ch);
            if (v[k] < 0) {
                return false;
            }
        }
        const auto u = static_cast<std::uint32_t>((v[0] << 18) | (v[1] << 12) | (v[2] << 6) | v[3]);
        cikti.push_back(static_cast<char>((u >> 16u) & 0xFFu));
        if (dolgu < 2) {
            cikti.push_back(static_cast<char>((u >> 8u) & 0xFFu));
        }
        if (dolgu < 1) {
            cikti.push_back(static_cast<char>(u & 0xFFu));
        }
    }
    return true;
}

std::string onaltilik(const std::uint8_t* veri, std::size_t n)
{
    static constexpr char kHex[] = "0123456789abcdef";
    std::string c;
    c.reserve(2 * n);
    for (std::size_t i = 0; i < n; ++i) {
        c.push_back(kHex[veri[i] >> 4u]);
        c.push_back(kHex[veri[i] & 0x0Fu]);
    }
    return c;
}

std::string ws_kabul_anahtari(std::string_view istemci_anahtari)
{
    std::string s(istemci_anahtari);
    s += "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
    const auto h = sha1(reinterpret_cast<const std::uint8_t*>(s.data()), s.size());
    return base64(h.data(), h.size());
}

}  // namespace ida::a3::ozet
