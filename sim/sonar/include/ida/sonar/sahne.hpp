// Sonar sahnesi: ışın–sahne kesişiminin soyut arayüzü ve bu dilimin çözümsel sahnesi
// (05 §2.4.1; A7 §3.4).
//
// Sonar modeli sahneyi yalnız `Sahne` arayüzünden görür. Bu dilimde `CozumselSahne` vardır:
// havuz kutusu (taban, dört duvar, su yüzeyi), sonsuz düzlemler (düz deniz tabanı, su yüzeyi)
// ve basit gövdeler (küre, kutu, silindir; sabit hızla hareket edebilir ya da pozu dışarıdan
// güncellenebilir). A7 glTF ağlarından kurulan sahne (BVH) sonraki dilimde aynı arayüzü
// uygular: kesişim, yüzey normali, gövdeye sabit hücre indisleri ve malzeme.
//
// Kabuller (iskelet §3.1): NED, metre; kuaterniyon (w, x, y, z), gövdeden NED'e.
#pragma once

#include "ida/fizik/parametreler.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace ida::sonar {

using fizik::Kuaterniyon;
using fizik::Vek3;

enum class GelisYasasi : std::uint8_t {
    lambert_cos2,   // σ_bs = μ cos²θ (A7 `lambert_cos2`)
    cos1,           // σ_bs = μ cos θ (A7 `cos1`)
};

// A7 §3.4 `akustik.*` alanları (A5 `kalibrasyon.malzeme.<kimlik>.akustik.*` aynı adlarla
// geçersiz kılar). Sade kademe yalnız `geri_sacilim_mu` ve `gelis_yasasi`'nı kullanır.
struct AkustikMalzeme {
    std::string kimlik;                         // A7 malzeme kimliği
    double geri_sacilim_mu = 0.01;              // 0 < μ ≤ 1
    GelisYasasi gelis_yasasi = GelisYasasi::lambert_cos2;
    double ayna_yansima_rs = 0.0;               // −1 ≤ R_s ≤ 1 (tam kademe)
    double puruzluluk_gamma_p = 1.0;            // 0 ≤ γ_p ≤ 1 (tam kademe)
};

// Bir ışının ilk kesişimi.
struct Vurus {
    double t = 0.0;                 // m, ışın parametresi (birim doğrultu)
    Vek3 nokta{};                   // NED
    Vek3 normal{};                  // NED, birim; ışının geldiği tarafa bakar
    std::uint32_t govde = 0;        // gövde kimliği (sahne içinde tekil)
    std::uint32_t yuzey = 0;        // gövde içinde yüzey (yüz / üçgen) numarası
    std::int32_t hucre_i = 0;       // gövdeye sabit yüzey koordinatlarında hücre indisi
    std::int32_t hucre_j = 0;
    std::uint16_t malzeme = 0;      // `Sahne::malzeme` indisi
};

class Sahne {
public:
    virtual ~Sahne() = default;
    // `t_ns` anının gövde pozlarıyla, `o`'dan `d` (birim) doğrultusundaki ışının
    // (t_min, t_maks] aralığındaki ilk kesişimi. Hücre indisleri kenarı `hucre_kenari` olan,
    // gövdeye sabit ızgaradadır (ışın sayısından bağımsız; iskelet §8 "Sonar saçıcı kuralı").
    virtual bool kesis(const Vek3& o, const Vek3& d, double t_min, double t_maks,
                       std::int64_t t_ns, double hucre_kenari, Vurus& v) const = 0;
    virtual std::size_t malzeme_sayisi() const = 0;
    virtual const AkustikMalzeme& malzeme(std::uint16_t indis) const = 0;
};

// ---------------------------------------------------------------- çözümsel sahne
enum class SekilTuru : std::uint8_t { kure, kutu, silindir, duzlem };

struct Sekil {
    SekilTuru tur = SekilTuru::kure;
    double yaricap = 0.5;               // küre, silindir (m)
    Vek3 yari_boyut{0.5, 0.5, 0.5};     // kutu: yerel eksenlerde yarı kenarlar (m)
    double yukseklik = 1.0;             // silindir: yerel z ekseni boyunca (m), merkezde
    // düzlem: yerel z = 0 düzlemi (normal yerel +z); iki yüzü de kesilir.
};

struct Govde {
    std::uint32_t kimlik = 0;           // tekil
    Sekil sekil{};
    // Yüzey başına malzeme: kutu 6 yüz (−x, +x, −y, +y, −z, +z), silindir 3 (yan, −z, +z),
    // küre ve düzlem 1. Eksikse son eleman tekrarlanır.
    std::vector<std::uint16_t> malzemeler{0};
    Vek3 konum{};                       // NED, t0_ns anında, yerel orijin
    Kuaterniyon yonelim{1.0, 0.0, 0.0, 0.0};   // yerelden NED'e
    Vek3 hiz_ned{};                     // m/s; konum(t) = konum + hiz · (t − t0)
    std::int64_t t0_ns = 0;
};

class CozumselSahne final : public Sahne {
public:
    // Malzeme ekler, indisini döndürür.
    std::uint16_t malzeme_ekle(const AkustikMalzeme& m);
    // Gövde ekler; kimlik tekil değilse ya da şekil geçersizse false.
    bool govde_ekle(const Govde& g);
    // Gövdenin hareketini günceller (dışarıdan sürülen hedef ya da torpido). Yoksa false.
    bool govde_pozu_kur(std::uint32_t kimlik, const Vek3& konum, const Kuaterniyon& yonelim,
                        const Vek3& hiz_ned, std::int64_t t0_ns);

    bool kesis(const Vek3& o, const Vek3& d, double t_min, double t_maks, std::int64_t t_ns,
               double hucre_kenari, Vurus& v) const override;
    std::size_t malzeme_sayisi() const override { return malzemeler_.size(); }
    const AkustikMalzeme& malzeme(std::uint16_t indis) const override { return malzemeler_.at(indis); }
    const std::vector<Govde>& govdeler() const { return govdeler_; }

private:
    std::vector<AkustikMalzeme> malzemeler_;
    std::vector<Govde> govdeler_;
};

// Kapalı havuz: x ∈ [0, uzunluk], y ∈ [−genislik/2, genislik/2] (NED), su yüzeyi z = 0,
// taban z = derinlik. Gövde kimlikleri 1 (havuz kutusu). Yüzey malzemesi `yuzey`, taban ve
// duvarlar `beton`. Kutunun +z yüzü taban, −z yüzü su yüzeyidir.
struct HavuzTanimi {
    double uzunluk = 25.0;
    double genislik = 12.5;
    double derinlik = 3.0;
    Vek3 orijin{0.0, 0.0, 0.0};          // kutunun (x = 0, y = 0, z = 0) noktasının NED konumu
    AkustikMalzeme beton{"havuz_beton", 0.02, GelisYasasi::cos1, 0.6, 0.7};   // A7 örneği (tahmin)
    AkustikMalzeme yuzey = su_yuzeyi_malzemesi();
    static AkustikMalzeme su_yuzeyi_malzemesi();
};
void havuz_ekle(CozumselSahne& s, const HavuzTanimi& h);

// Açık su: düz taban z = derinlik (gövde 1) ve su yüzeyi z = 0 (gövde 2).
void acik_su_ekle(CozumselSahne& s, double derinlik, const AkustikMalzeme& taban,
                  const AkustikMalzeme& yuzey = HavuzTanimi::su_yuzeyi_malzemesi());

}  // namespace ida::sonar
