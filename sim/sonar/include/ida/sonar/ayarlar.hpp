// Sonar kümesinin kurulum girdileri.
//
// Alan adları A5 sözleşmesinin anahtarlarıyla aynıdır (`arac.sensorler.<ad>.*` ping1d ve
// ping360 profilleri, A5 §3.6; `ortam.su.*`, §3.9; `kalite.sonar`, §3.12;
// `rastgelelik.ana_tohum`). Üye başlatıcıları A5 Ek E'nin profil değerleridir.
//
// A5'te bulunmayan girdiler ayrıca işaretlidir:
//  - Ping360 tarama yapılandırması ve Ping1D tarama penceresi. A5-K48 menzil ve örnek sayısını
//    "kartın yapılandırması" sayar, A1 ise (AB-024) yapılandırmanın A5'te olduğunu yazar ve
//    yapılandırma komutu taşımaz: iki sözleşmede de yolu yoktur. Bu dilimde kurulumun başlangıç
//    yapılandırmasıdır ve `SonarKumesi::*_yapilandir` ile değiştirilebilir (05 §2.4.5 komut
//    tablosunun anlamıyla). Alan adları cihazın (Blue Robotics ping-protocol) adlarıdır.
//  - `sade_isin_sayisi`: A5-K78'e göre sade kademede ping başına 5 000 ışın, A5'ten
//    değiştirilmez; alan yalnız testlerin enerji ve yakınsama sınamaları içindir.
//  - `tani`: profile alıcı öncesi doğrusal büyüklükleri ekler (A6/test; karta gitmez).
#pragma once

#include "ida/sensor/ayarlar.hpp"
#include "ida/sensor/olcum.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace ida::sonar {

using sensor::Montaj;
using sensor::SensorTuru;

// ---------------------------------------------------------------- A5 profilleri
struct Ping1dAyar {                       // profil ping1d (A5 §3.6)
    int kutu_sayisi = 200;
    std::int64_t ping_araligi_ns = 50'000'000;
    double oto_menzil_carpani = 1.5;
    std::int64_t islem_suresi_ns = 0;     // KE
    std::optional<double> sogurma;        // dB/m; DY: yoksa 05 §2.4.2 formülü (A5-K108)
};

enum class Ping360Baglanti { usb, ethernet, rs485 };

struct Ping360Ayar {                      // profil ping360 (A5 §3.6)
    Ping360Baglanti baglanti = Ping360Baglanti::usb;
    std::optional<int> baud;              // KE: yoksa iletim süresi 0 (A5-K101)
    std::int64_t t_m0_ns = 0;             // KE
    std::int64_t t_grad_ns = 0;           // KE
    std::int64_t islem_suresi_ns = 0;     // KE
    double gurultu_tabani = 0.0;          // KE; normalize yoğunluk Ĩ_n (05 §2.4.4)
    double yakin_alan_genligi = 0.0;      // KE; yakın alan bandında ek normalize yoğunluk
    double tvg_a = 20.0;                  // dB
    std::optional<double> sogurma;        // dB/m; DY
};

// ---------------------------------------------------------------- cihaz yapılandırması
// (A5'te yok; üst açıklama.) Ping360 cihaz alanları; 05 §2.4.5 aralıkları.
struct Ping360Yapilandirmasi {
    std::uint16_t n_samples = 1200;          // 200–1200 (A1 §3.4, A5-K78)
    std::uint16_t sample_period = 2222;      // 25 ns birimi, 80–40 000 (05 §2.4.5 varsayılanı, 50 m)
    std::uint16_t transmit_duration_us = 0;  // 0: 4 · T_s, µs'ye yuvarlanmış [T] (MP7 §3 md.4) ve
                                             // 1–1000 µs'ye kırpılmış (ping360_darbe_us);
                                             // açık değer 1–1000 µs (ping-protocol `transducer`)
    std::uint16_t transmit_frequency_khz = 750;   // 500–1000
    std::uint8_t gain = 1;                   // 0 düşük, 1 olağan, 2 yüksek
    std::uint16_t baslangic_grad = 0;        // 0–399
    std::uint16_t bitis_grad = 0;            // başlangıca eşitse tam tur (399 → 0 sarar)
    std::uint16_t adim_grad = 1;             // num_steps, 1–200
    std::int64_t bekleme_ns = 0;             // pingler arası `delay`
};

// Ping1D tarama penceresi (cihaz `set_range` ve `set_mode_auto`).
struct Ping1dYapilandirmasi {
    double scan_start = 0.0;                 // m
    double scan_length = 5.0;                // m [T]; otomatikte ilk pencere
    bool otomatik = true;                    // otomatik menzil (05 §2.4.6)
};

struct SonarAyari {                          // arac.sensorler.<ad> (tur ping1d ya da ping360)
    std::string ad;                          // RNG alt sistemi `sensor/<ad>/<akış>`
    SensorTuru tur = SensorTuru::ping360;
    std::uint16_t kimlik = 0;
    std::uint8_t inst = 0;
    std::string profil;                      // "ping1d" ya da "ping360"
    bool etkin = true;
    // A5 `saat_kaymasi_ppm` (A5-K41 yalnız IMU'da yasaklar): cihaz zamanlamasının (ping aralığı,
    // dinleme, işlem, iletim, bekleme, motor) gerçek süresi (1 + ppm · 10⁻⁶) katıdır; ideal
    // gönderim anları birikimsiz hesaplanıp en yakın adım sınırına bağlanır (sensör kümesinin
    // kuralı; sol #6). Kutu zaman ekseni nominaldir (A1-K52).
    double saat_kaymasi_ppm = 0.0;
    Montaj montaj{};                         // A5-K89: Ping1D ışın ekseni sensör +z; Ping360
                                             // dönme ekseni +z, 0 gradyan +x
    std::variant<Ping1dAyar, Ping360Ayar> ayar{Ping360Ayar{}};
    std::variant<Ping1dYapilandirmasi, Ping360Yapilandirmasi> yapilandirma{Ping360Yapilandirmasi{}};
};

struct SonarOrtami {                         // ortam (05'in akustik alanları)
    double yercekimi = 9.82;
    struct Su {
        double yogunluk = 1000.0;            // kg/m³
        double sicaklik_c = 20.0;            // °C
        double tuzluluk = 0.0;               // kg/kg
        double ph = 8.0;
        std::optional<double> ses_hizi;      // m/s; yoksa formül (W4-K1)
    } su{};
};

enum class SonarKademesi { sade, tam };

struct SonarKumesiAyari {
    std::vector<SonarAyari> sonarlar;
    SonarOrtami ortam{};
    SonarKademesi kademe = SonarKademesi::sade;   // kalite.sonar; `tam` bu dilimde yok
    std::uint64_t ana_tohum = 0;
    std::int64_t adim_ns = 1'250'000;
    int sade_isin_sayisi = 5000;             // A5-K78; yalnız testler değiştirir
    bool tani = false;
};

// İDA-0'ın sonarları (scenarios/ida0.toml: ping1d0 kimlik 6, ping360_0 kimlik 7) ve
// scenarios/ornek-havuz.toml ortamı (20 °C, tatlı su). `ana_tohum` = 1.
SonarKumesiAyari ida0_sonar_kumesi();

}  // namespace ida::sonar
