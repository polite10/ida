// Sensör kümesinin kurulum girdileri.
//
// Alan adları A5 sözleşmesinin anahtarlarıyla bire bir aynıdır (`arac.sensorler.<ad>.*`,
// `ortam.*`, `rastgelelik.ana_tohum`; A5 §3.6, §3.8, §3.9). Üye başlatıcıları A5 Ek E'nin ürün
// profili değerleridir (çözümleyicinin verilmeyen anahtara yazdığı değer). TOML okuma yoktur;
// sonraki iş aynı yapıları çözümlenmiş senaryodan doldurur.
//
// A5'te olmayan iki girdi vardır ve öyle işaretlenmiştir: gövde içi sıcaklık modeli
// (05 §2.3 [T]) ve ses hızı (A5 `ortam.su.ses_hizi` verilmemişse `kur` formülden hesaplar:
// W4-K1, `ida/sonar/akustik.hpp`).
#pragma once

#include "ida/fizik/parametreler.hpp"
#include "ida/sensor/olcum.hpp"

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace ida::sensor {

using fizik::Kuaterniyon;
using fizik::Mat3;
using fizik::Vek3;

struct Montaj {                       // arac.sensorler.<ad>.montaj
    Vek3 konum{};                     // m, O_b'ye göre, gövde çerçevesi (`l`)
    Kuaterniyon yonelim{1.0, 0.0, 0.0, 0.0};  // R_bs: sensörden gövdeye
};

struct ImuAyar {                      // profil icm42688p
    double jiroskop_yogunluk = 4.887e-5;                 // rad/s/√Hz (tek taraflı ASD)
    Vek3 ivme_yogunluk{0.0006374, 0.0006374, 0.0006865}; // m/s²/√Hz
    double jiroskop_acilis_bias_siniri = 0.008727;       // rad/s (3σ)
    double ivme_acilis_bias_siniri = 0.1961;             // m/s² (3σ)
    double jiroskop_sicaklik_kats_siniri = 8.727e-5;     // rad/s/°C (3σ)
    double ivme_sicaklik_kats_siniri = 0.001471;         // m/s²/°C (3σ)
    double jiroskop_olcek_siniri = 0.005;
    double jiroskop_eksenler_arasi_siniri = 0.0125;
    double ivme_olcek_siniri = 0.005;
    double ivme_eksenler_arasi_siniri = 0.01;
    double jiroskop_dogrusalsizlik = 0.001;
    double ivme_dogrusalsizlik = 0.0;
    std::int64_t jiroskop_gm_tau_ns = 1'000'000'000'000;
    std::int64_t ivme_gm_tau_ns = 300'000'000'000;
    double jiroskop_gm_sigma = 4.848e-5;                 // rad/s
    double ivme_gm_sigma = 0.001;                        // m/s²
    double jiroskop_aralik = 8.7266;                     // rad/s (FS)
    double ivme_aralik = 39.2266;                        // m/s² (FS)
    int bit = 16;
    bool suzgec_etkin = false;
    std::int64_t suzgec_tau_ns = 0;                      // süzgeç etkinse zorunlu (A5-K46)
    bool dunya_donusu_etkin = true;
};

struct ManyetometreAyar {             // profil rm3100
    double gurultu_sigma = 2e-8;                         // T, örnek başına
    Mat3 yumusak_demir{{{1.0, 0.0, 0.0}, {0.0, 1.0, 0.0}, {0.0, 0.0, 1.0}}};
    Vek3 sert_demir{};                                   // T
    double sicaklik_kats = 0.0;                          // T/°C
    double itici_girisim_orani = 0.2;
    std::int64_t teslim_gecikmesi_ns = 1'000'000;
};

struct BasincAyar {                   // profil ms5837_30ba
    double gurultu_sigma = 20.0;                         // Pa, örnek başına
    double mutlak_dogruluk_siniri = 5000.0;              // Pa (3σ açılış ofseti)
    double sicaklik_kats = 0.0;                          // Pa/°C
    double kuantalama = 10.0;                            // Pa; 0: yok
    std::int64_t donusum_penceresi_ns = 18'750'000;      // T_D1 = T_D2
    std::int64_t okuma_suresi_ns = 1'000'000;
    double dinamik_basinc_kats = 0.0;                    // C_p
    double sicaklik_ofset_siniri = 4.0;                  // °C (3σ)
};

struct GnssAyar {                     // profil neo_m9n
    double cep = 2.0;                                    // m
    std::int64_t gm_tau_ns = 60'000'000'000;
    double gm_varyans_payi = 0.9;
    double dusey_carpan = 1.5;
    double hiz_sigma = 0.05;                             // m/s
    std::int64_t teslim_gecikmesi_ns = 120'000'000;
    int uydu_sayisi = 20;
    int n_sv_ref = 12;
    double w50 = 0.03;                                   // m
    double w_gecis = 0.005;                              // m
    std::int64_t t_kisa_ns = 10'000'000'000;
    std::int64_t t_efemeris_ns = 7'200'000'000'000;
    std::int64_t hizli_medyan_ns = 1'000'000'000;
    std::int64_t sicak_medyan_ns = 2'000'000'000;
    std::int64_t soguk_medyan_ns = 24'000'000'000;
    std::int64_t t_edinim_ns = 0;                        // KE
    double yakinsama_baslangic_hatasi = 0.0;             // m; KE
    std::int64_t yakinsama_tau_ns = 0;                   // hata > 0 ise zorunlu
    double rapor_carpani = 1.0;
};

enum class DvlBaglanti { seri, tcp };

struct DvlAyar {                      // profil a50
    double isin_acisi = 0.39269908169872414;             // rad
    double yari_guc_genisligi = 0.07679448708775051;     // rad (bu dilimde kullanılmaz)
    double azimut_ofseti = 0.7853981633974483;           // rad
    int alt_isin_sayisi = 7;                             // bu dilimde kullanılmaz (düz taban)
    double gelis_acisi_siniri = 1.0471975511965976;      // rad
    double sigma0 = 0.001;                               // m/s
    double k_r = 0.0002;                                 // 1/s
    double k_u = 0.001;
    double uzun_donem_olcek_siniri = 0.0101;             // (3σ)
    double kappa = 1.0;
    std::optional<double> c_ayar;                        // m/s; yoksa ortamın ses hızı (KE)
    std::int64_t islem_suresi_ns = 5'000'000;
    bool uc_isinla_cozum = false;
    bool su_takibi = false;                              // true bu dilimde desteklenmez
    DvlBaglanti baglanti = DvlBaglanti::seri;
    int seri_baud = 115200;
    int mod_histerezis_ping = 3;
    int kilit_arama_ping = 3;
    std::int64_t yakin_mod_deneme_ns = 10'000'000'000;
};

struct BataryaOlcerAyar {             // profil psm
    double gerilim_bolucu = 11.0;
    double akim_ofseti = 0.33;                           // V
    double akim_olcegi = 37.8788;                        // A/V
    double analog_gurultu_sigma = 0.002;                 // V, örnek başına
    int adc_bit = 12;
    double adc_referans = 3.3;                           // V
    double sicaklik_kats = 0.0;                          // A/°C
    std::int64_t teslim_gecikmesi_ns = 1'000'000;
};

struct SizintiAyar {                  // profil sos_leak
    int prob_sayisi = 2;
    std::int64_t algilama_gecikmesi_ns = 1'000'000'000;
    double sahte_pozitif_orani = 0.0;                    // örnek ve prob başına olasılık
};

struct FirlaticiGeriOkumaAyar {       // profil firlatici_io
    std::int64_t anahtar_gecikmesi_ns = 10'000'000;
    std::int64_t teslim_gecikmesi_ns = 1'000'000;
};

// ping1d, ping360, kamera: bu kümenin kapsamı dışındadır (sonar ve kamera işleri); kurulumda
// kabul edilir, kayıt üretilmez ve uyarı yazılır.
struct KapsamDisiAyar {};

using SensorAyarGovdesi =
    std::variant<ImuAyar, ManyetometreAyar, BasincAyar, GnssAyar, DvlAyar, BataryaOlcerAyar,
                 SizintiAyar, FirlaticiGeriOkumaAyar, KapsamDisiAyar>;

struct SensorAyari {                  // arac.sensorler.<ad>
    std::string ad;                   // A5 anahtarı; RNG alt sistem adı `sensor/<ad>/<akış>`
    SensorTuru tur = SensorTuru::imu;
    std::uint16_t kimlik = 0;
    std::uint8_t inst = 0;
    std::string profil;
    bool etkin = true;
    std::optional<double> hiz_hz;     // yoksa profil hızı; imu ve dvl'de verilemez (A5-K41)
    double saat_kaymasi_ppm = 0.0;    // imu'da 0 olmalı (A5-K41)
    Montaj montaj{};
    SensorAyarGovdesi ayar{};
};

struct OrtamOrijin {                  // ortam.orijin
    double enlem = 0.0;               // rad
    double boylam = 0.0;              // rad
    double yukseklik = 0.0;           // m, elipsoit
};

struct SensorOrtami {                 // ortam (05'in okuduğu alanlar)
    double yercekimi = 9.82;          // m/s²
    double atmosfer_basinci = 101325.0;
    double derinlik = 0.0;            // m; sabit su derinliği (düz taban, DVL)
    struct Su {
        double yogunluk = 1000.0;     // kg/m³
        double sicaklik_c = 20.0;     // °C
        double tuzluluk = 0.0;        // kg/kg (A5 ortam.su.tuzluluk); ses hızı formülü için
        std::optional<double> ses_hizi;  // m/s; yoksa formülden (W4-K1, ida/sonar/akustik.hpp)
    } su{};
    std::optional<OrtamOrijin> orijin;
    // Jeoit yüksekliği N (m): GNSS `alt_mm` = (h_elipsoit − N), ortalama deniz seviyesine göre
    // (A4 §3.3; W4-K7). Kabul: 0 m (sim dünyasında jeoit modeli yok; değer bu ayarın
    // değeridir, sessiz varsayım değil). A5'e anahtar eklenmesi küçük sürüm notudur.
    double jeoit_yuksekligi = 0.0;
    std::optional<Vek3> manyetik_alan;  // T, NED
    struct Akinti {
        Vek3 hiz_ned{};               // m/s (dinamik basınç için bağıl hız)
    } akinti{};
};

// Fizik güç modeli yokken batarya ölçerin ve manyetometre girişiminin akım kaynağı
// (A5 arac.batarya, arac.itici_modeli; 04 §2.7.4 akım yasası). Dizinler kanal sırasıdır.
struct GucParametreleri {
    double v_sabit = 0.0;             // V; ideal kaynak (bara gerilimi)
    double aviyonik_guc = 0.0;        // W
    std::vector<double> gerilim_noktalari;
    std::vector<double> azami_itki_ileri;
    std::vector<double> azami_itki_geri;
    std::vector<double> tam_gaz_akimi;
    double akim_ussu = 1.5;
    std::array<Vek3, fizik::kIticiSayisi> itici_konumu{};  // m, O_b'ye göre
    std::array<Vek3, fizik::kIticiSayisi> itici_yonu{};    // birim
};

// Fizik araç parametrelerinden güç girdisi (yalnız alan kopyası; ida_fizik'e bağlanmaz).
inline GucParametreleri guc_parametreleri(const fizik::AracParametreleri& a)
{
    GucParametreleri g;
    g.v_sabit = a.batarya.v_sabit;
    g.aviyonik_guc = a.batarya.aviyonik_guc;
    g.gerilim_noktalari = a.itici_modeli.gerilim_noktalari;
    g.azami_itki_ileri = a.itici_modeli.azami_itki_ileri;
    g.azami_itki_geri = a.itici_modeli.azami_itki_geri;
    g.tam_gaz_akimi = a.itici_modeli.tam_gaz_akimi;
    g.akim_ussu = a.itici_modeli.akim_ussu;
    for (const auto& t : a.iticiler) {
        if (t.kanal >= 0 && t.kanal < fizik::kIticiSayisi) {
            g.itici_konumu[static_cast<std::size_t>(t.kanal)] = t.konum;
            g.itici_yonu[static_cast<std::size_t>(t.kanal)] = t.yon;
        }
    }
    return g;
}

// Gövde içi sıcaklık (05 §2.3; [T]; A5'te anahtarı yoktur): C dT/dt = P_e − k (T − T_su).
struct IcSicaklikModeli {
    bool etkin = true;                // kapalıyken T = ortam.su.sicaklik_c
    double isil_kapasite = 2000.0;    // J/K [T]
    double isil_iletkenlik = 2.0;     // W/K [T]
    std::optional<double> guc;        // W; yoksa GucParametreleri::aviyonik_guc
};

struct SensorKumesiAyari {
    std::vector<SensorAyari> sensorler;
    SensorOrtami ortam{};
    GucParametreleri guc{};
    std::uint64_t ana_tohum = 0;               // rastgelelik.ana_tohum
    std::int64_t adim_ns = 1'250'000;          // fizik adımı (iskelet §4)
    std::int64_t kontrol_periyodu_ns = 5'000'000;  // IMU örnekleme aralığı (iskelet §4, A5-K41)
    IcSicaklikModeli ic_sicaklik{};
};

// İDA-0 araç dosyasının sensörleri (scenarios/ida0.toml; değerler A5 Ek E profilleri).
std::vector<SensorAyari> ida0_sensorleri();

// İDA-0 sensörleri + scenarios/ornek-havuz.toml ortamı (havuz 3 m, 20 °C, alan ve orijin)
// + İDA-0 güç girdisi (scenarios/ida0.toml). Ses hızı: 1482,3 m/s (saf su, 20 °C; tahmin,
// formül sonar işinde). `ana_tohum` = 1.
SensorKumesiAyari ida0_sensor_kumesi();

}  // namespace ida::sensor
