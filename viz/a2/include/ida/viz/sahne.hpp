// Görselleştiricinin sahne durumu: gövde tablosu, gövde geçmişi, operatör saati ve ara
// değerleme. Godot'dan bağımsızdır; eklenti her karede buradan poz alır.
//
// Sözleşme A2 §4.3–§4.4: A2-K21 (süreklilik sınırı), K23 (doğum/ölüm), K27 (geçersiz gövde),
// K30 (ekstrapolasyon yok), K31 (t_op = t_son − D_op), K32 (Hermite + slerp, sınırda
// ara değerleme yok), K33 (görünürlük), K34 (≥ 500 ms geçmiş), K35 (t_op azalmaz).
#ifndef IDA_VIZ_SAHNE_HPP
#define IDA_VIZ_SAHNE_HPP

#include <cstdint>
#include <deque>
#include <limits>
#include <map>

#include "ida/a2_wire.h"
#include "ida/viz/cerceve.hpp"

namespace ida::viz {

inline constexpr std::int64_t kZamanEksiSonsuz = std::numeric_limits<std::int64_t>::min();
inline constexpr std::int64_t kZamanArtiSonsuz = std::numeric_limits<std::int64_t>::max();

struct Ornek {
  std::int64_t t_ns = 0;
  std::uint32_t sureklilik = 0;
  std::uint8_t dolu_tupler = 0;
  bool gecerli = true;  // false: geçersiz durum (A2-K27); poz son geçerli pozdur
  V3 p;  // NED, m
  Q q;   // gövdeden dünyaya
  V3 v;  // NED, m/s
};

struct GovdeKaydi {
  std::uint32_t kimlik = 0;
  std::uint16_t varlik_indeksi = 0;
  std::uint8_t tur = 0;
  std::uint8_t dolu_tupler = 0;
  std::uint32_t son_sureklilik = 0;
  std::int64_t t_dogum = kZamanEksiSonsuz;  // A2-K33: t_op ≥ t_dogum iken çizilir
  std::int64_t t_olum = kZamanArtiSonsuz;   // A2-K33: t_op ≥ t_olum iken çizilmez
  std::deque<Ornek> gecmis;                 // geçerli örnekler, t artan
  std::uint64_t gecersiz_sayisi = 0;        // A2-K27
};

struct Poz {
  V3 p;
  Q q;
  std::uint8_t dolu_tupler = 0;
};

class Sahne {
 public:
  void temizle() { govdeler_.clear(); }
  GovdeKaydi* bul(std::uint32_t kimlik);
  const GovdeKaydi* bul(std::uint32_t kimlik) const;
  GovdeKaydi& ekle(std::uint32_t kimlik, std::uint16_t varlik, std::uint8_t tur);
  void sil(std::uint32_t kimlik) { govdeler_.erase(kimlik); }
  const std::map<std::uint32_t, GovdeKaydi>& govdeler() const { return govdeler_; }
  std::map<std::uint32_t, GovdeKaydi>& govdeler() { return govdeler_; }

  // Geçerli bir gövde durumunu geçmişe ekler (t artan; aynı t gelirse sonuncunun yerine).
  static void ornek_ekle(GovdeKaydi& g, std::int64_t t_ns, const a2::GovdeDurumu& d);
  // Geçersiz bir durumu geçmişte temsil eder (A2-K27, sol #4): poz olarak son geçerli poz
  // tutulur ve geçersiz örneğin iki yanında ara değerleme yapılmaz. Önceki geçerli poz yoksa
  // eklenmez (gövdenin çizilecek pozu yoktur).
  static void gecersiz_ekle(GovdeKaydi& g, std::int64_t t_ns, std::uint32_t sureklilik, std::uint8_t dolu_tupler);

  // t_op'taki görünürlük (A2-K33) ve poz (A2-K32). false: çizilecek poz yok.
  static bool gorunur(const GovdeKaydi& g, std::int64_t t_op);
  static bool poz(const GovdeKaydi& g, std::int64_t t_op, Poz& out);

  // Geçmişi budar (A2-K34: t_op'tan en az 500 ms geriye kadar tutulur) ve ölüm zamanı
  // geçmiş gövdeleri siler. Silinen gövde kimlikleri `silinen`'e eklenir.
  template <class F>
  void buda(std::int64_t t_op, F&& silinen) {
    for (auto it = govdeler_.begin(); it != govdeler_.end();) {
      if (t_op >= it->second.t_olum) {
        silinen(it->first);
        it = govdeler_.erase(it);
        continue;
      }
      auto& gm = it->second.gecmis;
      while (gm.size() > 2 && gm[1].t_ns <= t_op - a2::kOperatorGecmisNs) gm.pop_front();
      ++it;
    }
  }

 private:
  std::map<std::uint32_t, GovdeKaydi> govdeler_;
};

// Operatör saati, A2-K31 aynen (sol #5): her karede t_op = t_son − D_op; t_son alınmış son
// durum.t_ns'dir. Ekstrapolasyon yok (A2-K30); t_son sıfırlama ve atlama dışında azalmadığı
// için t_op da azalmaz (A2-K35). Sonuç: operatör görünümünün zamanı durum gelişleriyle
// (60 Hz) adım adım ilerler; ara karelerde aynı t_op çizilir.
// Bekleme sayacı (A2-K30 "veri gecikirse son durumda bekler"): duraklatılmamışken son
// durumdan bu yana iki durum periyodundan uzun duvar süresi geçtiyse her kare 1 artar (Okuma).
class OperatorSaati {
 public:
  explicit OperatorSaati(std::int64_t d_op_ns = a2::kOperatorGecikmesiNs) : d_op_ns_(d_op_ns) {}

  void sifirla();  // yeni oturum, sıfırlama, atlama
  void durum_alindi(std::int64_t t_son, std::int64_t duvar_ns);
  // Bir çizim karesi için t_op'u döndürür. Henüz durum yoksa false.
  bool ilerle(std::int64_t duvar_ns, std::int64_t& t_op);
  void duraklat(bool d) { duraklatildi_ = d; }

  bool gecerli() const { return gecerli_; }
  std::int64_t t_op() const { return t_son_ - d_op_ns_; }
  std::int64_t t_son() const { return t_son_; }
  std::uint32_t bekleme_sayaci() const { return bekleme_; }
  std::int64_t d_op_ns() const { return d_op_ns_; }

  static constexpr std::int64_t kBeklemeEsigiNs = 2 * 16'666'667;  // iki durum periyodu (60 Hz)

 private:
  std::int64_t d_op_ns_;
  bool gecerli_ = false;
  bool duraklatildi_ = false;
  std::int64_t t_son_ = 0;
  std::int64_t duvar_son_durum_ = 0;
  std::uint32_t bekleme_ = 0;
};

}  // namespace ida::viz

#endif
