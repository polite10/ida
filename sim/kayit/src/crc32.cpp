// crc32.cpp — tablo güdümlü CRC-32 (8 baytlık dilimlerle).
#include "crc32.hpp"

#include <array>

namespace ida::kayit::ic {

namespace {

using Tablo = std::array<std::array<std::uint32_t, 256>, 8>;

constexpr Tablo tablo_uret() noexcept
{
    Tablo t{};
    for (std::uint32_t i = 0; i < 256; ++i) {
        std::uint32_t c = i;
        for (int k = 0; k < 8; ++k) {
            c = (c & 1u) != 0 ? (c >> 1u) ^ 0xEDB88320u : c >> 1u;
        }
        t[0][i] = c;
    }
    for (std::uint32_t i = 0; i < 256; ++i) {
        std::uint32_t c = t[0][i];
        for (std::size_t s = 1; s < 8; ++s) {
            c = t[0][c & 0xFFu] ^ (c >> 8u);
            t[s][i] = c;
        }
    }
    return t;
}

constexpr Tablo kTablo = tablo_uret();

}  // namespace

std::uint32_t crc32_ekle(std::uint32_t crc, const std::uint8_t* veri, std::size_t n) noexcept
{
    std::size_t i = 0;
    while (n - i >= 8) {
        const std::uint32_t a = crc ^ (static_cast<std::uint32_t>(veri[i]) | (static_cast<std::uint32_t>(veri[i + 1]) << 8u) |
                                       (static_cast<std::uint32_t>(veri[i + 2]) << 16u) |
                                       (static_cast<std::uint32_t>(veri[i + 3]) << 24u));
        crc = kTablo[7][a & 0xFFu] ^ kTablo[6][(a >> 8u) & 0xFFu] ^ kTablo[5][(a >> 16u) & 0xFFu] ^
              kTablo[4][(a >> 24u) & 0xFFu] ^ kTablo[3][veri[i + 4]] ^ kTablo[2][veri[i + 5]] ^ kTablo[1][veri[i + 6]] ^
              kTablo[0][veri[i + 7]];
        i += 8;
    }
    for (; i < n; ++i) {
        crc = kTablo[0][(crc ^ veri[i]) & 0xFFu] ^ (crc >> 8u);
    }
    return crc;
}

}  // namespace ida::kayit::ic
