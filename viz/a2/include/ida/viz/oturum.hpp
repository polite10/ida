// A2 istemci oturumu (görselleştirici tarafı): soketten bağımsız durum makinesi.
//
// Girdi: bağlantı kuruldu / gelen baytlar / zaman / bağlantı koptu / uygulamanın sahne
// kurma sonucu. Çıktı: gönderilecek baytlar, uygulama olayları ve sahne durumu (gövdeler,
// geçmiş, dalga, ortam). Soket ve yeniden bağlanma zamanlaması `IstemciBaglantisi`'ndadır
// (viz/a2/soket); Godot'ya bağlama `viz/eklenti`'dedir.
//
// Sözleşme A2: §4.1 (K06, K07), §4.2 (K10–K16), §4.3 (K20–K29), §4.7 (K62–K67),
// §4.11 (K90–K92). Kamera hattı bu dilimde yoktur: `yetenekler` biti 0 gönderilir; kamera
// isteğine `kare_dustu(sahne_hazir_degil)`, halka yenilemeye `halka_eslendi(6)` ile
// yanıt verilir (raporda "Okumalar").
#ifndef IDA_VIZ_OTURUM_HPP
#define IDA_VIZ_OTURUM_HPP

#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include "ida/viz/kodek.hpp"
#include "ida/viz/sahne.hpp"
#include "ida/viz/sha256.hpp"

namespace ida::viz {

enum class Asama : std::uint8_t {
  kapali,                // bağlantı yok
  merhaba_gonderildi,    // hosgeldin bekleniyor
  kurulum,               // dalga_kumesi, ortam, malzeme_optik bekleniyor (A2-K12)
  uygulama_bekleniyor,   // sahne kuruluyor; hazir henüz gönderilmedi
  akis,                  // hazir gönderildi
};

const char* asama_adi(Asama a);

enum class OlayTuru : std::uint8_t {
  sahne_kur,       // hosgeldin + üç kurulum iletisi geldi: uygulama sahneyi kurmalı
  ortam_degisti,   // akış sırasında yeni ortam
  malzeme_degisti, // akış sırasında yeni malzeme optiği
  dalga_degisti,   // akış sırasında yeni dalga kümesi
  sifirlandi,      // sıfırlama ya da atlama: gövde geçmişi silindi
  kapandi,         // oturum bitti (neden metinde)
};

struct Olay {
  OlayTuru tur;
  std::string metin;
};

struct OturumSayaclari {
  std::uint64_t alinan_ileti = 0;
  std::uint64_t sira_boslugu = 0;        // A2-K07: durum seyreltmesi (hata değil)
  std::uint64_t gecersiz_govde = 0;      // A2-K27
  std::uint64_t eksik_govde = 0;         // yaşayan ama durumda olmayan gövde (bilgi)
  std::uint64_t kamera_istegi = 0;
  std::uint64_t dusen_kare = 0;
  std::uint64_t tutarsiz_durum = 0;      // A2-K28
};

class IstemciOturumu {
 public:
  struct Ayarlar {
    std::string godot_surumu = "?";
    std::string uzanti_surumu = "?";
    std::string gpu_adi = "";
    std::uint32_t surec_kimligi = 0;
    std::uint8_t platform = static_cast<std::uint8_t>(a2::Platform::macos_arm64);
    std::int64_t d_op_ns = a2::kOperatorGecikmesiNs;
  };

  explicit IstemciOturumu(Ayarlar ayarlar);

  // ---- bağlantı katmanından ----
  void baglandi(std::int64_t simdi_ns, bool pencere_cizilebilir);
  void veri_geldi(const Bayt* veri, std::size_t n, std::int64_t simdi_ns);
  // Nabız gönderimi ve karşı taraf zaman aşımı (A2-K62).
  void zaman(std::int64_t simdi_ns, std::uint32_t cizim_hizi_mhz, bool pencere_cizilebilir);
  void baglanti_koptu(const std::string& neden);
  // Uygulama kapanırken: veda(olagan) (A2-K67).
  void olagan_kapat();

  Baytlar giden_al();
  bool giden_var() const { return !giden_.empty(); }
  // Giden boşaltıldıktan sonra soket kapatılmalı mı?
  bool kapatilmali() const { return kapatilmali_; }
  void kapatildi() { kapatilmali_ = false; }
  // A2-K16: ret(surum) ve ret(manifest) sonrasında kendiliğinden yeniden bağlanılmaz.
  bool yeniden_baglanabilir() const { return yeniden_baglanabilir_; }

  // ---- uygulamadan ----
  // Sahne kurma sonucu (A2-K13): durum HazirDurumu, özet manifest dosyasının SHA-256'sı.
  void sahne_sonucu(a2::HazirDurumu durum, const Ozet& manifest_ozeti);
  // Uygulamanın bulduğu protokol hatası (ör. varlık indeksi manifest dışında, §3.3).
  void protokol_hatasi(const std::string& neden);

  std::vector<Olay> olaylari_al();

  // ---- sorgular ----
  Asama asama() const { return asama_; }
  const HosgeldinIleti& hosgeldin() const { return hosgeldin_; }
  const DalgaIleti& dalga() const { return dalga_; }
  const a2::Ortam& ortam() const { return ortam_; }
  const MalzemeIleti& malzeme() const { return malzeme_; }
  const Sahne& sahne() const { return sahne_; }
  Sahne& sahne() { return sahne_; }
  OperatorSaati& saat() { return saat_; }
  const OperatorSaati& saat() const { return saat_; }
  std::uint32_t calisma_nesli() const { return nesil_; }
  std::uint8_t calisma_durumu() const { return calisma_durumu_; }
  bool tutarsiz() const { return tutarsiz_; }
  const OturumSayaclari& sayaclar() const { return sayac_; }
  const std::string& son_kapanis_nedeni() const { return son_neden_; }
  std::uint64_t oturum_kimligi() const { return hosgeldin_.h.oturum_kimligi; }

 private:
  void parca_isle(const Bayt* veri, std::size_t n, std::int64_t simdi_ns);
  void isle(const a2::Baslik& b, const Baytlar& yuk, std::int64_t simdi_ns);
  void hata(KodHata h, const char* baglam);
  void kapat(const std::string& neden, bool yeniden);
  std::uint32_t sonraki_sira() { return giden_sira_++; }

  void durum_isle(const DurumIleti& d, std::int64_t simdi_ns);
  void olay_isle(const a2::SahneOlayi& o);
  void tablo_isle(const GovdeTablosuIleti& t);
  void kamera_istegi_isle(const KameraIstegiIleti& k);
  bool kimlik_esle(std::uint32_t kimlik, std::uint16_t varlik, std::uint8_t tur);

  Ayarlar ayar_;
  Asama asama_ = Asama::kapali;
  int kurulum_adimi_ = 0;  // 0 dalga, 1 ortam, 2 malzeme bekleniyor
  CerceveOkuyucu okuyucu_{Yon::sim_gorsel};
  Baytlar giden_;
  std::uint32_t giden_sira_ = 0;
  bool ilk_ileti_ = true;
  std::uint32_t son_sira_ = 0;
  std::int64_t son_alim_ns_ = 0;
  std::int64_t son_nabiz_ns_ = 0;
  bool kapatilmali_ = false;
  bool yeniden_baglanabilir_ = true;
  std::string son_neden_;
  std::vector<Olay> olaylar_;

  HosgeldinIleti hosgeldin_;
  DalgaIleti dalga_;
  a2::Ortam ortam_{};
  MalzemeIleti malzeme_;
  Sahne sahne_;
  OperatorSaati saat_;
  std::uint32_t nesil_ = 0;
  std::uint8_t calisma_durumu_ = 1;
  bool tutarsiz_ = false;
  bool durum_var_ = false;
  std::int64_t son_durum_t_ = 0;
  // Atlama bloğu (A2-K90, K91)
  bool atlama_bekleniyor_ = false;
  std::int64_t atlama_t_ = 0;
  std::uint32_t atlama_nesli_ = 0;
  bool tablo_sonrasi_ilk_durum_ = false;
  std::int64_t tablo_t_ = 0;
  // A2-K92: kimlik → (varlık indeksi, tür), oturum boyunca.
  std::map<std::uint32_t, std::pair<std::uint16_t, std::uint8_t>> kimlikler_;
  // Kamera isteği sıra denetimi (§3.5)
  bool istek_var_ = false;
  std::uint64_t son_istek_ = 0;
  std::uint32_t son_kare_sirasi_ = 0;
  OturumSayaclari sayac_;
};

}  // namespace ida::viz

#endif
