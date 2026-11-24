// Senaryo planı (W4-sim-senaryo): `ida_sim`'in yürüttüğü kurulum ve zaman çizelgesi. İki kaynak
// aynı biçime çevrilir: A5 senaryo dosyası (`plan_kur`; yalnız `DogrulanmisSenaryo`) ve
// yerleşik senaryolar (`yerlesik_plan`; z1, bos). Yürütme sim/sunucu'dadır (Calistirici).
//
// Bu dilimde yürütülen olay sınıfları (A5 Tablo 3.10-A): `emniyet_girdisi` (kalıcı atama),
// `gcs_komutu` (ARM, DISARM, SET_MODE), `sensor` (sensör kümesinin gövdeli türleri; durumlu
// üçü sensör kümesinde reddedilir), `aktuator/itki_carpani`. Tetik: `zaman_ns` ya da
// `kosul.buyukluk = derinlik`. Diğer sınıflar, `esc_sinyal_kaybi`, `kosul.buyukluk =
// gorev_ogesi`, bağlantı emülatörü profilleri (L1, L2) ve `hil_bariyer` başlamadan açık hatayla
// reddedilir.
#pragma once

#include "ida/kopru/kopru.hpp"
#include "ida/senaryo/fizik_yukleyici.hpp"
#include "ida/senaryo/senaryo.hpp"
#include "ida/sensor/ariza.hpp"
#include "ida/sensor/ayarlar.hpp"
#include "ida/sunucu/sunucu.hpp"

#include <array>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace ida::sunucu {

enum class Karsilastirma : std::uint8_t { buyuk, buyuk_esit, kucuk, kucuk_esit, esit };

// A5-K63: `derinlik` yer gerçeği O_b derinliği (m); her fizik adımı sonunda değerlendirilir.
struct Kosul {
    Karsilastirma k = Karsilastirma::buyuk;
    double deger = 0.0;
    bool sagliyor(double derinlik) const;
};

enum class PlanOlayTuru : std::uint8_t { emniyet, gcs, guided, sensor, itici };
enum class EmniyetGirdisi : std::uint8_t { acil_durdurma, fis, pim };

struct PlanOlayi {
    std::string kimlik;                    // olaylar[i].kimlik (yerleşikte üretilmiş ad)
    PlanOlayTuru tur = PlanOlayTuru::gcs;
    std::optional<std::int64_t> zaman_ns;  // ya zaman ya koşul (A5-K61)
    std::optional<Kosul> kosul;
    std::int64_t sure_ns = 0;              // geçici arızada; 0 süresiz
    // emniyet_girdisi
    EmniyetGirdisi girdi = EmniyetGirdisi::acil_durdurma;
    int tup = -1;                          // fis/pim: 0 = f1, 1 = f2
    bool deger = false;                    // true: basılı / takılı
    // gcs_komutu
    std::uint8_t komut = 0;                // IDAW_CMD_TYPE
    std::uint16_t arg = 0;
    // yerleşik GUIDED_SP (A5'te yok; yalnız yerleşik z1)
    kopru::GuidedHedef guided{};
    int guided_no = 0;
    // sensor
    sensor::SensorArizasi ariza{};
    // aktuator/itki_carpani
    std::uint8_t kanal_maskesi = 0;        // bit i: t(i+1)
    double carpan = 1.0;
};

// Kısıtlayan emniyet ataması mı (acil durdurma basılı, fiş çıkık, pim takılı; A5-K62, K63).
bool kisitlayan(const PlanOlayi& o);
// Emniyet alanının A5 anahtar yolu (ör. "emniyet_girdileri.pim.f1").
std::string emniyet_alani(const PlanOlayi& o);

struct Plan {
    bool dosyadan = false;
    Kip kip = Kip::sil;
    std::int64_t sure_ns = 0;
    std::uint64_t tohum = 1;
    // Yalnız dosyada (yerleşikte koddaki İDA-0 kurulumu).
    std::optional<senaryo::FizikKurulumu> fizik;
    std::optional<sensor::SensorKumesiAyari> sensor;
    std::vector<std::pair<std::string, double>> parametreler;   // otopilot.parametreler (ad, değer)
    // Başlangıç emniyet durumu (A5-K86, K87).
    bool acil_durdurma = false;
    std::array<bool, 2> fis{false, false};
    std::array<bool, 2> pim{true, true};
    std::array<bool, 2> dolu{true, true};
    // Görev (A5-K55–K57).
    bool gorev_var = false;
    std::int64_t gorev_yukleme_ns = 0;
    std::vector<kopru::GorevOgesi> gorev;
    // Zaman çizelgesi; dizideki sıra dosyadaki sıradır (A5-K62).
    std::vector<PlanOlayi> olaylar;
    std::string davranis_ozeti;
    std::map<std::string, std::string> kaynak_ozetleri;
    std::vector<std::string> uyarilar;      // yürütme uyarıları (ör. SIL hız çarpanı uygulanmaz)
};

// Dosyadan plan. false: bu dilimde yürütülemeyen bir öğe var (hata: olay kimliği ve neden).
bool plan_kur(const senaryo::DogrulanmisSenaryo& dz, const senaryo::DogrulamaRaporu& rapor, Plan& p,
              std::string& hata);

// Yerleşik senaryo (z1, bos) ve komut satırı atamaları (--acil, --acil-kalk).
Plan yerlesik_plan(const Ayar& a);

}  // namespace ida::sunucu
