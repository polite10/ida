// Sahte A2 kaynağının kinematik senaryosu. Fizik DEĞİLDİR: pozlar zamanın kapalı
// işlevleridir; yalnız görselleştiriciyi gerçek sim sunucusu yokken geliştirmek içindir
// (W3-viz kapsam md. 5). Değerler (dalga, su optiği, güneş) görsel amaçlı seçilmiş
// varsayımlardır; ölçüm ya da kalibrasyon değildir.
//
// Zaman çizelgesi (çalıştırma başından, s):
//   0–10   yüzeyde yarı batık bekler, dalgayla iner kalkar
//   10–24  3 m'ye dalar
//   24–66  4 m yarıçaplı tam bir tur atar (≈0,6 m/s)
//   66–96  sabit hedefe yaklaşır, 3,9 m'ye iner
//   96–121 hedefin 1,8 m önünde durur
//   121    sıfırlama (yeni çalıştırma nesli, yeni gövde kimlikleri, zaman 0)
#ifndef IDA_SAHTE_A2_SENARYO_HPP
#define IDA_SAHTE_A2_SENARYO_HPP

#include <cstdint>
#include <string>
#include <vector>

#include "ida/viz/cerceve.hpp"
#include "ida/viz/kodek.hpp"
#include "ida/viz/sha256.hpp"

namespace ida::sahte {

using namespace ida::viz;

struct SenaryoAyarlari {
  std::uint8_t kademe_3b = 2;      // 1 sade, 2 tam
  std::string manifest_yolu;       // mutlak yol
  Ozet manifest_ozeti{};
  std::int64_t sifirlama_ns = 121'000'000'000;  // bu sürede sıfırlama; 0: hiç
};

// Varlık indeksleri: assets/acik_su/manifest.json'daki varliklar[] sırası.
inline constexpr std::uint16_t kVarlikArac = 0;
inline constexpr std::uint16_t kVarlikHedef = 1;
inline constexpr std::uint16_t kVarlikTaban = 2;

struct KinematikPoz {
  V3 p;    // NED
  Q q;     // gövdeden dünyaya
};

class Senaryo {
 public:
  explicit Senaryo(SenaryoAyarlari a);

  const SenaryoAyarlari& ayarlar() const { return a_; }
  const DalgaIleti& dalga() const { return dalga_; }
  const a2::Ortam& ortam() const { return ortam_; }

  // Şu anki çalıştırmanın yaşayan gövdeleri (hosgeldin ve govde_dogdu için).
  std::vector<a2::GovdeBilgisi> govde_bilgileri() const;
  // t_ns anındaki gövde durumları (hız ve açısal hız sayısal türevle).
  std::vector<a2::GovdeDurumu> govde_durumlari(std::int64_t t_ns) const;

  // Sıfırlama: yeni kimlikler (A2-K92: kimlik yeniden kullanılmaz), nesil +1.
  void sifirla();
  std::uint32_t nesil() const { return nesil_; }
  std::uint32_t arac_kimligi() const { return arac_kimligi_; }
  std::uint32_t hedef_kimligi() const { return hedef_kimligi_; }

  // Kinematik pozlar (testler ve ekran görüntüsü zamanlaması için açık).
  KinematikPoz arac_pozu(std::int64_t t_ns) const;
  KinematikPoz hedef_pozu() const;

 private:
  SenaryoAyarlari a_;
  DalgaIleti dalga_;
  a2::Ortam ortam_{};
  std::uint32_t nesil_ = 1;
  std::uint32_t arac_kimligi_ = 1;
  std::uint32_t hedef_kimligi_ = 2;
};

}  // namespace ida::sahte

#endif
