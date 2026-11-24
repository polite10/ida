// Sonar kümesinin ürettiği kayıtlar.
//
// Başlık sensör kümesininkiyle aynıdır (`sensor::OlcumBasligi`; A1 §3.4 ortak alanları, A3 ölçüm
// başlığı). Yükler A4 `ida_ping1d_t` ve `ida_ping360_t` ile aynı ad ve birimdedir; A1 köprüsü
// değerleri dönüşümsüz kopyalar (Ping360 satırını A1-K49'a göre parçalamak köprünün işidir).
// A3/A6'ya giden ek alanlar (`kademe`, `ses_hizi_mps`, Ping1D `profil`) karta gitmez.
//
// Zaman (iskelet §3.1; 05 §2.2; A1-K53, K54):
//  - Ping1D: `t_meas_ns` algılanan ekonun yansıma anı; gönderim anı `t_meas − tx_offset_ns`.
//    Eko yoksa `t_meas = t_tx`, güven 0, VALID = 0.
//  - Ping360: `t_meas_ns` darbenin gönderim anı; kutu `i`'nin nominal yansıma anı
//    `t_meas + (2i + 1) · sample_period · 25 ns / 4` (A1-K52).
//  - `t_tx_ns` ikisinde de gönderim anıdır (A3 başlığı; A1'de yok).
#pragma once

#include "ida/sensor/olcum.hpp"

#include <cstdint>
#include <variant>
#include <vector>

namespace ida::sonar {

using sensor::OlcumBasligi;

inline constexpr std::uint8_t kKademeSade = 1;   // A3 `kademe`
inline constexpr std::uint8_t kKademeTam = 2;

// Tanı (yalnız `SonarKumesiAyari::tani`): kutu başına doğrusal büyüklükler.
struct ProfilTanisi {
    std::vector<double> ham;          // alıcı öncesi kutu enerjisi Ĩ (benek ve gürültüden önce)
    std::vector<double> yogunluk;     // benek ve gürültüden sonra Ĩ (log öncesi)
    std::uint32_t isin = 0;           // gerçekleşen ışın sayısı
    std::uint32_t isabet = 0;         // sahneye çarpan ışın
};

struct Ping1dOlcumu {                 // A1 IDA_PING1D / A4 ida_ping1d_t
    float distance = 0.0f;            // m
    float scan_start = 0.0f;          // m
    float scan_length = 0.0f;         // m
    std::uint32_t ping_number = 0;
    std::uint32_t tx_offset_ns = 0;
    std::uint8_t confidence = 0;      // %, 0–100
    // A3 `yuk_ping1d` (karta gitmez)
    std::uint8_t kademe = kKademeSade;
    std::vector<std::uint8_t> profil; // kutu_sayisi örnek, [scan_start, scan_start + scan_length]
    ProfilTanisi tani;
};

struct Ping360Olcumu {                // A1 IDA_PING360_HEAD + DATA / A4 ida_ping360_t
    std::uint16_t angle_grad = 0;
    std::uint16_t n_samples = 0;
    std::uint16_t sample_period = 0;
    std::uint16_t tx_duration_us = 0;
    std::uint16_t tx_freq_khz = 0;
    std::uint8_t gain = 0;
    std::vector<std::uint8_t> data;   // n_samples bayt
    // A3 `yuk_ping360` (karta gitmez)
    std::uint8_t kademe = kKademeSade;
    float ses_hizi_mps = 0.0f;
    ProfilTanisi tani;
};

using SonarYuku = std::variant<Ping1dOlcumu, Ping360Olcumu>;

struct SonarOlcumu {
    OlcumBasligi hdr{};
    SonarYuku yuk{};
};

}  // namespace ida::sonar
