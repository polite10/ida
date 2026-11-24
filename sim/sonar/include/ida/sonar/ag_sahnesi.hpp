// Üçgen ağlı sonar sahnesi (W4-sonar-sahne): A7 varlıklarının ağlarından kurulan, `Sahne`
// arayüzünü uygulayan sahne (05 §2.4.1; A7-K27, K28).
//
// Gövdeler: su yüzeyi (z = 0 sonsuz düzlemi; A7'de su yüzeyi malzemesi yoktur, sonar modelinin
// yüzey malzemesi kullanılır) ve üçgen ağlı gövdeler (statik sahne varlığı, hedef varlıkları).
// Her ağlı gövdenin sınır kutusu ağacı (BVH; yüzey alanı sezgisiyle bölme, derinlik sınırından sonra
// ortanca) gövde çerçevesinde, `govde_ekle`'de BİR KEZ kurulur;
// hareketli gövdede ağaç yeniden kurulmaz, ışın gövde çerçevesine çevrilir (görev belgesi kapsam
// 2). `kesis` bellek ayırmaz (W4-K27 (5)) ve durum değiştirmez (aynı anda birden çok okuyucu
// güvenlidir; `govde_ekle` ve `govde_pozu_kur` okuyucularla eşzamanlı çağrılmaz).
//
// Belirlenimcilik: aynı uzaklıktaki iki üçgen kesişiminde (ortak kenar, köşe) sıra numarası küçük
// olan üçgen seçilir; sonuç ağacın yapısından bağımsızdır. Gövdeler arasında eşitlikte önce
// eklenen gövde kalır.
//
// Saçıcı hücreleri (iskelet §8 "Sonar saçıcı kuralı"): üçgenler gövde çerçevesinde eş düzlemli
// kümelere ayrılır (`Vurus::yuzey` = küme numarası); her kümenin düzleminde sabit iki eksen
// vardır ve hücre indisi bu eksenlerdeki gövdeye sabit koordinatlardan alınır. Böylece iki üçgene
// bölünmüş bir duvar tek bir hücre ızgarası taşır. Su yüzeyinde koordinatlar NED (x, y)'dir.
//
// Kabuller (iskelet §3.1): NED, metre; kuaterniyon (w, x, y, z), gövdeden NED'e.
#pragma once

#include "ida/sonar/sahne.hpp"

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace ida::sonar {

// Gövde çerçevesinde üçgen ağ; `malzeme[i]` i. üçgenin `Sahne::malzeme` indisi.
struct UcgenAgi {
    std::vector<Vek3> koseler;
    std::vector<std::array<std::uint32_t, 3>> ucgenler;
    std::vector<std::uint16_t> malzeme;
};

// Kinematik hareket (A5 §3.10 `hedefler.<ad>.hareket`'in belirlenimci türleri). t = t_ns · 10⁻⁹ s
// (sim zamanı); yönelim sabittir:
//   sabit       konum
//   sabit_hiz   konum + hiz_ned · t
//   sinus       konum + genlik_ned · sin(2π t / periyot + faz)
enum class HareketTuru : std::uint8_t { sabit, sabit_hiz, sinus };

struct Hareket {
    HareketTuru tur = HareketTuru::sabit;
    Vek3 hiz_ned{};
    Vek3 genlik_ned{};
    std::int64_t periyot_ns = 1;   // sinus: > 0
    double faz = 0.0;              // rad
};

struct AgGovdesi {
    std::uint32_t kimlik = 0;                  // sahne içinde tekil
    UcgenAgi ag;
    Vek3 konum{};                              // NED, gövde orijini (A7 REF)
    Kuaterniyon yonelim{1.0, 0.0, 0.0, 0.0};   // gövdeden NED'e
    Hareket hareket{};
};

class AgSahnesi final : public Sahne {
public:
    std::uint16_t malzeme_ekle(const AkustikMalzeme& m);
    // z = 0 su yüzeyi düzlemi (iki yüzü de kesilir). En çok bir kez; kimlik tekil olmalı.
    bool su_yuzeyi_ekle(std::uint32_t kimlik, std::uint16_t malzeme, std::string* hata = nullptr);
    // Ağlı gövde ekler ve ağacını kurar. Kimlik tekil değilse, ağ boşsa, indeks ya da malzeme
    // geçersizse, köşe ya da poz sonlu değilse false.
    bool govde_ekle(const AgGovdesi& g, std::string* hata = nullptr);
    // Gövdenin pozunu ve hareketini değiştirir (ağaç yeniden kurulmaz). Yoksa false.
    bool govde_pozu_kur(std::uint32_t kimlik, const Vek3& konum, const Kuaterniyon& yonelim, const Hareket& h);

    bool kesis(const Vek3& o, const Vek3& d, double t_min, double t_maks, std::int64_t t_ns, double hucre_kenari,
               Vurus& v) const override;
    std::size_t malzeme_sayisi() const override { return malzemeler_.size(); }
    const AkustikMalzeme& malzeme(std::uint16_t indis) const override { return malzemeler_.at(indis); }

    std::size_t govde_sayisi() const { return govdeler_.size(); }
    // Gövdenin sonarın kullandığı üçgen sayısı; yoksa 0.
    std::size_t ucgen_sayisi(std::uint32_t kimlik) const;
    std::size_t toplam_ucgen() const;
    // Gövdenin `t_ns` anındaki NED konumu (hareket kuralıyla). Yoksa false.
    bool govde_konumu(std::uint32_t kimlik, std::int64_t t_ns, Vek3& p) const;
    bool su_yuzeyi_var() const { return yuzey_var_; }

private:
    struct Ucgen {
        Vek3 v0{}, e1{}, e2{};
        Vek3 n{};                  // birim (gövde çerçevesi); sıfır alanlıda 0
        Vek3 u_ekseni{}, w_ekseni{};
        std::uint32_t sira = 0;    // ağdaki sıra (eşitlik kuralı)
        std::uint32_t yuzey = 0;   // eş düzlemli küme
        std::uint16_t malzeme = 0;
    };
    struct Dugum {
        Vek3 mn{}, mx{};
        std::uint32_t ilk = 0;     // yaprak: ilk üçgen
        std::uint32_t sayi = 0;    // yaprak: üçgen sayısı; 0 → iç düğüm (sol = bu + 1)
        std::uint32_t sag = 0;     // iç düğüm: sağ çocuk
        std::uint32_t eks = 0;     // iç düğüm: bölme ekseni (sol çocuk bu eksende küçük taraf)
    };
    struct Govde {
        std::uint32_t kimlik = 0;
        std::vector<Ucgen> ucgenler;
        std::vector<Dugum> dugumler;
        Vek3 konum{};
        Kuaterniyon yonelim{1.0, 0.0, 0.0, 0.0};
        fizik::Mat3 r{};
        Hareket hareket{};
    };

    std::uint32_t agac_kur(Govde& g, std::uint32_t bas, std::uint32_t son, std::uint32_t derinlik);
    Vek3 konum_(const Govde& g, std::int64_t t_ns) const;
    bool kimlik_bos(std::uint32_t kimlik) const;

    std::vector<AkustikMalzeme> malzemeler_;
    std::vector<Govde> govdeler_;
    bool yuzey_var_ = false;
    std::uint32_t yuzey_kimlik_ = 0;
    std::uint16_t yuzey_malzeme_ = 0;
};

}  // namespace ida::sonar
