// İDA fizik çekirdeği (W3-fizik, dilim 1): araç ve ortam parametreleri.
//
// Alan adları A5 sözleşmesinin (docs/sozlesmeler/A5-senaryo.md §3.2–§3.5, §3.9, §3.13)
// anahtarlarıyla bire bir aynıdır; yapı iç içeliği anahtar yolunu izler
// (`arac.kutle.hacim` -> AracParametreleri::kutle.hacim). TOML okuyucu bu işte yoktur; sonraki
// iş (08, çözümleyici) aynı yapıları çözümlenmiş senaryodan doldurur. Kapalı değer kümeleri
// (`ortam.tur`, `ortam.dalga.spektrum` ...) enum olarak tutulur; adları A5 değerleriyle aynıdır.
//
// Birimler SI (iskelet §3.1; A5-K11): açı radyan, süre tamsayı nanosaniye (`_ns`), ESC darbe
// genişliği mikrosaniye (`_us`), sıcaklık santigrat (`_c`). Çerçeveler: dünya NED, gövde
// x ileri / y sancak / z aşağı; konumlar gövde referans noktası O_b'ye göre (A5-K20).
// Kuaterniyon (w, x, y, z), Hamilton, gövdeden dünyaya.
#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace ida::fizik {

using Vek3 = std::array<double, 3>;
using Vek6 = std::array<double, 6>;
using Mat3 = std::array<std::array<double, 3>, 3>;  // satır dizisi
using Kuaterniyon = std::array<double, 4>;          // (w, x, y, z)

inline constexpr int kIticiSayisi = 8;
inline constexpr int kFirlaticiSayisi = 2;

// ---------------------------------------------------------------- arac (A5 §3.2)
struct AracKutle {                 // arac.kutle
    double kutle = 0.0;            // kg; boş araç (trim hariç)
    double hacim = 0.0;            // m³; boş araç deplasmanı ∇
    Vek3 r_g{};                    // m; ağırlık merkezi
    Vek3 r_b{};                    // m; tam batık yüzerlik merkezi
    Mat3 atalet{};                 // kg·m²; O_b'ye göre (köşegen dışı: I_yz = −Σ m y z)
    double trim_kutlesi = 0.0;     // kg; hacimsiz nokta kütle (A5-K24)
    Vek3 trim_konumu{};            // m
};

struct HidrodinamikBelirsizlik {   // arac.hidrodinamik.belirsizlik
    double oteleme = 0.0;
    double donme = 0.0;
};

struct HidrodinamikSapma {         // arac.hidrodinamik.sapma (yalnız fizik tarafı; A5-K26)
    Vek6 eklenmis_kutle{1.0, 1.0, 1.0, 1.0, 1.0, 1.0};
    Vek6 dogrusal_sonum{1.0, 1.0, 1.0, 1.0, 1.0, 1.0};
    Vek6 karesel_sonum{1.0, 1.0, 1.0, 1.0, 1.0, 1.0};
};

struct AracHidrodinamik {          // arac.hidrodinamik; katsayılar pozitif büyüklük (isaret_kurali)
    std::string isaret_kurali = "pozitif_buyukluk";
    Vek6 eklenmis_kutle{};         // [X_u̇, Y_v̇, Z_ẇ, K_ṗ, M_q̇, N_ṙ]
    Vek6 dogrusal_sonum{};         // [X_u, Y_v, Z_w, K_p, M_q, N_r]
    Vek6 karesel_sonum{};          // [X_u|u|, …, N_r|r|]
    Vek3 yuzey_sonum_orani{};      // Otter ζ (heave, roll, pitch)
    double ruzgar_direnc_katsayisi = 1.0;  // dilim 1'de kullanılmaz (rüzgâr gövdesi sonraki dilim)
    HidrodinamikBelirsizlik belirsizlik{};
    HidrodinamikSapma sapma{};
};

// ---------------------------------------------------------------- arac (A5 §3.3)
struct Itici {                     // arac.iticiler.<tN>
    int kanal = 0;                 // 0–7; kartın gördüğü itici sırası (A5-K25)
    Vek3 konum{};                  // m; r_i
    Vek3 yon{};                    // birim; pozitif komutun itki yönü
    std::string rol;
};

struct IticiModeli {               // arac.itici_modeli
    std::string tur = "t200_gecici";
    std::array<double, 5> sekil_polinomu{};   // F(u) = Σ c_i u^(2i+1)
    std::vector<double> gerilim_noktalari;    // V
    std::vector<double> azami_itki_ileri;     // N
    std::vector<double> azami_itki_geri;      // N (pozitif büyüklük)
    std::vector<double> tam_gaz_akimi;        // A (dilim 1'de kullanılmaz: güç modeli yok)
    double akim_ussu = 1.5;                   // (dilim 1'de kullanılmaz)
    std::vector<double> transfer_pay;         // K(s) payı, yüksek dereceden düşüğe
    std::vector<double> transfer_payda;       // K(s) paydası
    double pervane_yaricapi = 0.0;            // m
    std::vector<double> havalanma_h_bolu_r;   // kesin artan
    std::vector<double> havalanma_beta;       // [0, 1]
};

struct Esc {                       // arac.esc
    int pwm_notr_us = 1500;
    int olu_bant_us = 25;
    int pwm_min_us = 1100;
    int pwm_max_us = 1900;
    std::int64_t baslatma_suresi_ns = 0;
};

// ---------------------------------------------------------------- arac (A5 §3.4)
enum class BataryaKipi { ideal };  // `b0`, `b1` 1.0'da desteklenmiyor (A5-K28)

struct Batarya {                   // arac.batarya
    BataryaKipi kip = BataryaKipi::ideal;
    double v_sabit = 0.0;          // V; dilim 1'de bara gerilimi bu değerdir (güç modeli yok)
    int seri = 1;
    int paralel = 1;
    double kapasite = 0.0;         // C
    double r_kablo = 0.0;          // Ω (dilim 1'de kullanılmaz)
    double aviyonik_guc = 0.0;     // W (dilim 1'de kullanılmaz)
};

struct Temas {                     // arac.temas (dilim 1'de kullanılmaz: çarpışma kapsam dışı)
    double yay_sabiti = 0.0;
    double sonum_orani = 0.0;
    double surtunme_katsayisi = 0.0;
};

// ---------------------------------------------------------------- arac (A5 §3.5)
struct Torpido {                   // arac.torpido; sayısal değerler sabittir (A5-K29)
    std::string varlik;
    std::string geometri = "prolate_sferoid";
    double boy = 0.0;              // m
    double cap = 0.0;              // m
    double kutle = 0.0;            // kg
    Vek3 r_g{};                    // m, torpido merkezine göre (bağlı durumda ihmal; A5-K23)
    double kanat_alani_duzlem = 0.0;
    double kanat_en_boy_orani = 0.0;
    double kanat_x = 0.0;
    double surukleme_katsayisi = 0.0;
    double ayrilma_hizi = 0.0;
};

struct Firlatici {                 // arac.firlaticilar.<f>
    Vek3 tup_konumu{};
    Vek3 tup_ekseni{};
    double tup_su_kutlesi = 0.0;
};

struct AracParametreleri {         // arac
    std::string ad;
    std::string varlik;
    std::string referans_noktasi;
    AracKutle kutle{};
    AracHidrodinamik hidrodinamik{};
    std::array<Itici, kIticiSayisi> iticiler{};            // t1…t8 sırasıyla
    IticiModeli itici_modeli{};
    Esc esc{};
    Batarya batarya{};
    Temas temas{};
    Torpido torpido{};
    std::array<Firlatici, kFirlaticiSayisi> firlaticilar{};  // f1, f2 sırasıyla
};

// ---------------------------------------------------------------- ortam (A5 §3.9)
enum class OrtamTuru { havuz, acik_su };
enum class DalgaSpektrumu { yok, pierson_moskowitz, jonswap };
enum class DalgaYayilimi { tek_yon, cos2s };

struct GaussMarkov {               // ortam.akinti.gauss_markov, ortam.ruzgar.gauss_markov
    bool etkin = false;
    std::int64_t tau_ns = 0;
    double sigma = 0.0;
    double sinir = 0.0;
};

struct Akinti {                    // ortam.akinti
    Vek3 hiz_ned{};                // m/s; düzgün akıntı
    GaussMarkov gauss_markov{};    // dilim 1'de etkin olamaz
};

struct Ruzgar {                    // ortam.ruzgar (gövdesi sonraki dilim)
    Vek3 hiz_ned{};
    GaussMarkov gauss_markov{};
};

struct Dalga {                     // ortam.dalga (gövdesi sonraki dilim; yalnız `yok` kabul edilir)
    DalgaSpektrumu spektrum = DalgaSpektrumu::yok;
    double hs = 0.0;
    std::int64_t tepe_periyodu_ns = 0;
    double gamma = 3.3;
    double yon = 0.0;
    DalgaYayilimi yayilim = DalgaYayilimi::cos2s;
    double yayilim_s = 10.0;
    int bilesen_sayisi = 64;
};

struct Su {                        // ortam.su
    double yogunluk = 1000.0;      // kg/m³
    double sicaklik_c = 20.0;      // °C (fizik kullanmaz)
    double tuzluluk = 0.0;
    double ph = 8.0;
};

struct OrtamParametreleri {        // ortam
    OrtamTuru tur = OrtamTuru::havuz;
    double derinlik = 0.0;         // m; sabit su derinliği (dilim 1'de taban yok: temas kapsam dışı)
    double yercekimi = 9.82;       // m/s²
    double atmosfer_basinci = 101325.0;
    double hava_yogunlugu = 1.225;
    Su su{};
    Dalga dalga{};
    Akinti akinti{};
    Ruzgar ruzgar{};
};

// ---------------------------------------------------------------- baslangic (A5 §3.13)
struct BaslangicDurumu {           // baslangic
    Vek3 konum{};                                   // O_b, NED
    Kuaterniyon yonelim{1.0, 0.0, 0.0, 0.0};        // gövdeden NED'e
    Vek6 hiz{};                                     // ν, gövde, O_b
    std::vector<std::string> yuklu_tupler{"f1", "f2"};
};

// ---------------------------------------------------------------- A5'te olmayan ayarlar
// Sayısal çözüm ve yer tutucu geometri ayarları. Fizik adımı A5'ten ayarlanmaz (A5-K77;
// iskelet §4: 1,25 ms); başka adım yalnız testte (adım yarılama) kullanılır.
struct SayisalAyarlar {
    std::int64_t adim_ns = 1'250'000;
    // Otter yüzey sönümünün tam batıkta sıfıra C¹ geçişi (04 §2.4 "arada C¹ geçiş"; değeri
    // bu işin seçimidir): gövdenin en üst noktası bu derinliğe inince ağırlık 0 olur.
    double yuzey_sonumu_gecis_derinligi = 0.02;  // m
};

// İDA-0 varsayılanları: scenarios/ida0.toml (A5 araç dosyası) ile aynı değerler.
AracParametreleri ida0_arac();
// Havuz ortamı: A5 Ek E varsayılanları (yercekimi 9,82; havuzda yoğunluk 1000, tuzluluk 0);
// zorunlu `derinlik` (3,0 m) ve `su.sicaklik_c` (20 °C) scenarios/ornek-havuz.toml'dan.
OrtamParametreleri havuz_ortami();

}  // namespace ida::fizik
