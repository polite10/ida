// sha256.hpp — SHA-256 (FIPS 180-4). A2'de sim tarafında tek kullanım: dalga kümesinin
// `ozet_sha256` alanı (§3.4: bileşen dizisinin baytları üzerinden). Dış bağımlılık eklenmez ve
// `viz/` ya da `sim/a3` hedeflerine bağlanılmaz (W5-K29); bu yüzden burada yazıldı.
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace ida::a2::ic {

using Ozet = std::array<std::uint8_t, 32>;

class Sha256 {
public:
    Sha256() noexcept;
    void ekle(const void* veri, std::size_t n) noexcept;
    Ozet bitir() noexcept;

private:
    void blok_(const std::uint8_t* b) noexcept;

    std::uint32_t h_[8];
    std::uint8_t tampon_[64];
    std::size_t tampon_n_ = 0;
    std::uint64_t toplam_ = 0;
};

Ozet sha256(const void* veri, std::size_t n) noexcept;

}  // namespace ida::a2::ic
