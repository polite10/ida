// yayinci_ic.hpp — Yayinci'nın iç durumu (sunucu tarafı tüketici bunu görür).
//
// Telemetri kaydı: [u16 kanal_indeks][u16 bayrak][u32 0][u64 anahtar][i64 t_ns][yük]
//   bayrak bit0: bu kanal-anahtar için önceki kayıt(lar) halkada düştü (A3-K28 oncesinde_dusme).
// Olay halkası kaydı: [u8 tür][...]
//   tür 0 (olay):        [u8 0][7 bayt 0][i64 t_ns][yuk_olay yükü]
//   tür 1 (komut sonucu): [u8 1][u8 uygulandi][u8 adim_var][u8 0][u32 komut_no][i64 t_ns]
//                        [u64 uygulama_adimi][u64 adim_no][u16 kod_boy][u16 0][u32 ayrinti_boy][kod][ayrinti]
#pragma once

#include "halka.hpp"
#include "ida/a3/kaynak.hpp"

#include <array>
#include <atomic>
#include <cstdint>

namespace ida::a3 {

inline constexpr std::size_t kTelemetriBaslik = 24;
inline constexpr std::size_t kOlayBaslik = 16;
inline constexpr std::size_t kSonucBaslik = 40;

struct Yayinci::Ic {
    Ic(std::size_t telemetri_bayt, std::size_t olay_bayt) : telemetri(telemetri_bayt), olaylar(olay_bayt) {}

    BaytHalkasi telemetri;
    BaytHalkasi olaylar;
    std::atomic<std::uint64_t> telemetri_dusen{0};
    std::atomic<std::uint64_t> olay_dusen{0};
    std::atomic<std::uint64_t> kodlama_hatasi{0};

    // Yalnız üretici: düşen kaydın kanal-anahtarı, sonraki başarılı kaydında bildirilir.
    struct Dusme {
        std::uint16_t kanal = 0xFFFFu;  // 0xFFFF: boş yuva
        std::uint64_t anahtar = 0;
    };
    std::array<Dusme, 64> dusmeler{};
    std::array<bool, 64> kanal_genel_dusme{};  // tablo dolunca kanal genelinde (fazla bildirim, kısıtlayan yön)
};

}  // namespace ida::a3
