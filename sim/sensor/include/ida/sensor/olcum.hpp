// Sensör kümesinin ürettiği ölçüm kayıtları (05 §3 MeasurementRecord'un bu dilimdeki biçimi).
//
// Alanlar A1 §3.4 (Tablo 3.4b) ve A4 §3.2–§3.3 ile bire bir eşlenir: aynı ad, aynı birim,
// aynı tip (float = IEEE-754 binary32). A1 köprüsü değerleri dönüşümsüz kopyalar; yalnız
// `t_hazir_ns` ve `t_teslim_ns` köprünün parti atamasına girer ve karta gönderilmez (05 §2.2).
//
// Birimler SI (iskelet §3.1): rad/s, m/s², T, Pa, m, m/s, V, A, °C. Değerler sensör
// çerçevesindedir (montaj yönelimi uygulanmıştır); GNSS konum ve hızı WGS-84 / NED'dir.
#pragma once

#include <array>
#include <cstdint>
#include <limits>
#include <variant>

namespace ida::sensor {

// Değerler A4 `IDA_SENS_*` ile aynıdır (hdr.type). `kamera` A1/A4'te örnek türü değildir.
enum class SensorTuru : std::uint8_t {
    imu = 1,
    manyetometre = 2,
    basinc = 3,
    gnss = 4,
    dvl = 5,
    ping1d = 6,
    ping360 = 7,
    batarya_olcer = 8,
    sizinti = 9,
    firlatici_geri_okuma = 10,
    kamera = 32,
};

// hdr.flags bitleri (A4 IDA_HF_*, A1 IDAW_SAMPLE_FLAGS).
inline constexpr std::uint16_t kBayrakGecerli = 0x0001u;
inline constexpr std::uint16_t kBayrakDoymus = 0x0002u;
inline constexpr std::uint16_t kBayrakSensorArizasi = 0x0004u;

// GNSS fix türü (A4 IDA_GNSS_FIX_*).
inline constexpr std::uint8_t kFixYok = 0u;
inline constexpr std::uint8_t kFix2B = 1u;
inline constexpr std::uint8_t kFix3B = 2u;

// DVL bayrakları (A4 IDA_DVL_*).
inline constexpr std::uint8_t kDvlHizGecerli = 0x01u;
inline constexpr std::uint8_t kDvlKovaryansVar = 0x02u;
inline constexpr std::uint8_t kDvlDipIzi = 0x04u;

// Tüp bitleri (A4 IDA_TUBE_*).
inline constexpr std::uint8_t kTupDolu = 0x01u;
inline constexpr std::uint8_t kTupFisTakili = 0x02u;
inline constexpr std::uint8_t kTupPimCikik = 0x04u;
inline constexpr std::uint8_t kTupIzinAcik = 0x08u;

inline constexpr std::int64_t kTxYok = std::numeric_limits<std::int64_t>::min();

// Arıza maskesi bitleri (`OlcumBasligi::ariza_maskesi`; yalnız A3/A6, karta gitmez):
// bit = 1 << static_cast<int>(ArizaTuru) (ariza.hpp).

struct OlcumBasligi {
    std::uint16_t sensor_id = 0;     // A5 `kimlik`
    SensorTuru tur = SensorTuru::imu;
    std::uint8_t inst = 0;           // hdr.inst (0–3)
    std::uint32_t seq = 0;           // sensörün örnek sayacı; kayıp arıza boşluk bırakır
    std::int64_t t_meas_ns = 0;      // ölçüm zamanı (05 §2.2)
    std::uint32_t window_ns = 0;     // aralık ölçümünde aralık süresi; anlıkta 0
    std::int64_t t_tx_ns = kTxYok;   // akustik gönderim anı (DVL; bilgi, A1'de yok)
    std::int64_t t_hazir_ns = 0;     // kaydın sim içinde var olduğu adım sınırı
    std::int64_t t_teslim_ns = 0;    // modellenmiş alım anı (karta gitmez)
    std::uint16_t flags = 0;         // kBayrak*
    std::uint32_t ariza_maskesi = 0; // etkin arıza türleri (yalnız A3/A6)
    std::uint16_t model_surumu = 1;  // yalnız A6
};

struct ImuOlcumu {                   // A1 IDA_IMU / A4 ida_imu_t
    std::array<float, 3> gyro{};     // rad/s, aralık ortalaması
    std::array<float, 3> accel{};    // m/s², montaj noktasında özgül kuvvet
    float temp_c = 0.0f;
};

struct MagOlcumu {                   // A1 IDA_MAG / A4 ida_mag_t
    std::array<float, 3> field{};    // T
};

struct BaroOlcumu {                  // A1 IDA_BARO / A4 ida_baro_t
    float p_abs = 0.0f;              // Pa, mutlak
    float temp_c = 0.0f;
};

struct GnssOlcumu {                  // A1 IDA_GNSS / A4 ida_gnss_t
    std::int32_t lat_e7 = 0;
    std::int32_t lon_e7 = 0;
    std::int32_t alt_mm = 0;
    std::array<float, 3> vel_ned{};
    float eph = 0.0f;                // m, yatay 1σ (eksen başına)
    float epv = 0.0f;                // m, dikey 1σ
    std::uint8_t fix_type = kFixYok;
    std::uint8_t n_sv = 0;
};

struct DvlOlcumu {                   // A1 IDA_DVL / A4 ida_dvl_t
    std::array<float, 3> vel{};      // m/s, DVL çerçevesi, dibe göre
    std::array<float, 6> cov{};      // (m/s)², xx, xy, xz, yy, yz, zz
    float fom = 0.0f;                // m/s
    std::array<float, 4> beam_dist{-1.0f, -1.0f, -1.0f, -1.0f};
    float altitude = -1.0f;
    std::uint8_t dvl_flags = 0;
    std::uint8_t beam_valid = 0;
};

struct BataryaOlcumu {               // A1 IDA_BATT / A4 ida_batt_t
    float voltage = 0.0f;
    float current = 0.0f;            // A, pozitif = boşalma
    float temp_c = 0.0f;
};

struct SizintiOlcumu {               // A1 IDA_LEAK / A4 ida_leak_t
    std::uint8_t mask = 0;
};

struct EmniyetOlcumu {               // A1 IDA_SAFETY / A4 ida_safety_t (+ safety_rev, A1-K106)
    std::uint8_t estop = 0;
    std::array<std::uint8_t, 2> tube{};
    std::uint32_t safety_rev = 0;
};

using OlcumYuku = std::variant<ImuOlcumu, MagOlcumu, BaroOlcumu, GnssOlcumu, DvlOlcumu,
                               BataryaOlcumu, SizintiOlcumu, EmniyetOlcumu>;

struct Olcum {
    OlcumBasligi hdr{};
    OlcumYuku yuk{};
};

}  // namespace ida::sensor
