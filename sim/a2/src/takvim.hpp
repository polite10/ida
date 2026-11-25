// takvim.hpp — `durum` iletisinin 60 Hz takvimi (A2-K20).
//
// Fizik adımı 1 250 000 ns (800 Hz); takvim 60/1 Hz. k'inci iletinin ideal zamanı k / 60 s =
// k · 40/3 adımdır ve en yakın adım sınırına bağlanır. 40k/3'ün kesir kısmı yalnız 0, 1/3, 2/3
// olabildiği için eşitlik durumu yoktur:
//     adim(k) = ⌊(80k + 3) / 6⌋         (= round(40k / 3))
// Dizi 0, 13, 27, 40, 53, 67, 80, … : ardışık iletiler arası 13 ya da 14 adım; ideal ızgaradan
// sapma en çok 1/3 adım = 416 667 ns (≤ 625 000 ns). Bir adımın takvimde olup olmadığı yalnız adım
// numarasından hesaplanır (sayaç tutulmaz, kayma birikmez): adim mod 40 ∈ {0, 13, 27}.
#pragma once

#include "ida/a2/yayinci.hpp"

#include <cstdint>

namespace ida::a2::ic {

// 800 Hz / 60 Hz = 40 / 3 (aşağıdaki sabitler bu orandan gelir).
static_assert(1'000'000'000 % kAdimNs == 0 && 1'000'000'000 / kAdimNs == 800, "fizik adımı 800 Hz değil");
static_assert(kDurumHizPay == 60 && kDurumHizPayda == 1, "durum takvimi 60/1 Hz değil");

constexpr std::uint64_t takvim_adimi(std::uint64_t k) noexcept { return (80u * k + 3u) / 6u; }

constexpr bool takvimde(std::uint64_t adim) noexcept
{
    const std::uint64_t r = adim % 40u;
    return r == 0u || r == 13u || r == 27u;
}

static_assert(takvim_adimi(0) == 0 && takvim_adimi(1) == 13 && takvim_adimi(2) == 27 && takvim_adimi(3) == 40);
static_assert(takvimde(0) && takvimde(13) && takvimde(27) && takvimde(40) && !takvimde(14) && !takvimde(26));

}  // namespace ida::a2::ic
