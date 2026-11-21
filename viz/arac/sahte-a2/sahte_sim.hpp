// Sahte A2 sunucusu: soketten bağımsız. Sim zamanını çağıran ilerletir (gerçek zamanlı
// ya da testte hızlı); A2 sunucu tarafının görselleştiriciyi ilgilendiren kısmını uygular:
// el sıkışma (A2-K10–K15), 60 Hz durum takvimi (A2-K20), sim nabzı ve zaman aşımı
// (A2-K62), sıfırlama olayı ve yeni gövdelerin doğumu (A2-K29, K80, K92).
// Kamera, halka, kayıt, A3 ve seyreltme yoktur.
#ifndef IDA_SAHTE_A2_SAHTE_SIM_HPP
#define IDA_SAHTE_A2_SAHTE_SIM_HPP

#include <cstdint>
#include <string>
#include <vector>

#include "ida/viz/kodek.hpp"
#include "senaryo.hpp"

namespace ida::sahte {

class SahteSim {
 public:
  enum class Asama : std::uint8_t { yok, merhaba_bekleniyor, akis, kapaniyor };

  explicit SahteSim(SenaryoAyarlari a);

  // ---- bağlantı ----
  void baglanti_acildi(std::int64_t duvar_ns, std::uint64_t oturum_kimligi);
  void baglanti_kapandi();
  void veri_geldi(const Bayt* veri, std::size_t n, std::int64_t duvar_ns);
  void zaman(std::int64_t duvar_ns);  // nabız ve zaman aşımları
  void kapaniyor();                   // veda(sim_kapaniyor)
  Baytlar giden_al();
  bool kapatilmali() const { return kapatilmali_; }
  void kapatildi() { kapatilmali_ = false; asama_ = Asama::yok; }
  Asama asama() const { return asama_; }

  // ---- simülasyon ----
  // Sim'i sahte kaynağın başlangıcından bu yana geçen toplam sim süresine (`t_toplam_ns`,
  // sıfırlamalardan bağımsız, artan) kadar ilerletir; takvimdeki her durum iletisini
  // (bağlı oturum akıştaysa) gönderir. Sıfırlama zamanı gelince sıfırlar: çalıştırmanın
  // sim zamanı 0'dan yeniden başlar.
  void sim_ilerle(std::int64_t t_toplam_ns);
  std::int64_t t_ns() const { return t_ns_; }
  std::uint64_t adim_no() const { return adim_no_; }
  const Senaryo& senaryo() const { return senaryo_; }

  // ---- gözlem (testler ve günlük) ----
  struct Sayaclar {
    std::uint64_t gonderilen_ileti = 0;
    std::uint64_t durum = 0;
    std::uint64_t sifirlama = 0;
    std::uint64_t hazir = 0;
    std::uint32_t son_hazir_durumu = 0xFFFFFFFFu;
    bool son_hazir_ozet_tuttu = false;
    std::uint64_t gor_nabzi = 0;
    std::uint64_t protokol_hatasi = 0;
    std::uint64_t veda_alinan = 0;
  };
  const Sayaclar& sayaclar() const { return sayac_; }
  const std::string& son_olay() const { return son_olay_; }

 private:
  void parca_isle(const Bayt* veri, std::size_t n, std::int64_t duvar_ns);
  void gonder_kurulum();
  void durum_gonder(std::int64_t t, std::uint64_t adim, bool takvim_disi);
  void hata(KodHata h, const char* baglam);
  void kapat(const std::string& neden);
  std::uint32_t sira() { return sira_++; }
  static std::uint64_t takvim_adimi(std::uint64_t k);  // A2-K20

  Senaryo senaryo_;
  Asama asama_ = Asama::yok;
  CerceveOkuyucu okuyucu_{Yon::gorsel_sim};
  Baytlar giden_;
  std::uint32_t sira_ = 0;
  bool ilk_alim_ = true;
  std::uint32_t son_alinan_sira_ = 0;
  bool kapatilmali_ = false;
  std::uint64_t oturum_kimligi_ = 0;
  std::int64_t baglanti_duvar_ = 0;
  std::int64_t son_alim_duvar_ = 0;
  std::int64_t son_nabiz_duvar_ = 0;
  bool hazir_alindi_ = false;
  std::int64_t duvar_ = 1;  // son bilinen duvar saati (durum.gonderim_duvar_ns)

  std::int64_t t_ns_ = 0;
  std::uint64_t adim_no_ = 0;
  std::uint64_t takvim_k_ = 0;  // sıradaki takvim iletisi
  std::int64_t kayma_ns_ = 0;   // önceki çalıştırmaların toplam süresi (sıfırlamada artar)
  Sayaclar sayac_;
  std::string son_olay_;
};

}  // namespace ida::sahte

#endif
