// Ping360 taramalı sonar modeli (05 §2.4), ölçüm için yeterli sadakatte.
// Yükü belirleyen adımlar gerçek: ışın örnekleme, Embree sorguları, çok yollu
// yayılım (≤ 2 speküler), saçıcı hücreleri, kesirli gecikmeli biriktirme,
// darbe/alıcı çekirdeği, gürültü/benek ve alıcı zinciri. Kalibrasyon ve arıza yok.
#pragma once
#include <complex>
#include <cstdint>
#include <vector>

#include "havuz.hpp"
#include "sahne.hpp"

namespace mp7 {

struct SonarAyar {
  int n = 1200;            // istenen örnek sayısı
  int sp = 311;            // sample_period (25 ns birimi)
  double c = 1500.0;       // ses hızı, m/s (A5'ten sabit c ile geçersiz kılma yolu)
  double f = 750e3;        // Hz
  double alfa_dbm = 0.1314;  // dB/m, 750 kHz tatlı su 20 °C (05 §2.4.2 aritmetiği; doğrulanmadı)
  int N = 5000;            // istenen ışın sayısı
  int D = 2;               // speküler etkileşim derecesi: 0, 1, 2
  bool tam = true;         // tam / sade kademe
  double tx_kat = 4.0;     // tx_duration = tx_kat * T_s  [T]
  bool gurultu = true;     // gürültü tabanı + yakın alan bandı
  bool sade_benek = true;  // sade: 05 §2.4.3 kutu başına üstel benek (mimar 2. tur: korunur)
  double hucre = 0.01;     // Delta_h, m
  uint64_t tohum = 1;      // sonar tohumu (ışın ve hücre akışları)
  double In = 1e-14;       // gürültü tabanı Ĩ_n [K]
  double In_yakin = 1e-10; // yakın alan bandı ek gürültü [K]
  double yakin_m = 1.0;    // yakın alan bandı sınırı (0,75-1 m)
  double Lmin = -140.0, Lmaks = -40.0, G = 0.0;  // eşleme [K]
};

struct Profil {
  int n_eff = 0;
  double T_s = 0;            // s
  std::vector<double> I;     // zarf Ĩ (log öncesi)
  std::vector<uint8_t> u8;   // kuantalanmış çıktı
  uint64_t kesisim = 0, gorunurluk = 0;  // sorgu sayıları
};

class Sonar {
 public:
  Sonar(const Sahne& s, const SonarAyar& a);
  // grad: kafa açısı (0..399, 0,9°); t_s: gönderim anı (hedef pozu için)
  void ping(uint64_t ping_no, int grad, double t_s, Havuz& havuz, Profil& p);
  int nh() const { return nh_; }
  int nv() const { return nv_; }
  int N_gercek() const { return nh_ * nv_; }

  // Bir yolun bir hücreye katkısı (mimar tanımı, düzeltme turu): enerji ağırlığı w,
  // işaretli genlik a (yan lob ve R_s işareti), gecikme tau, hücre anahtarı.
  struct Katki {
    double tau;
    double w;
    double a;
    uint32_t anahtar[4];  // (tür<<24 | gövde, üçgen, ci, cj)
  };
  struct IsinSonuc {
    Katki k[3];
    uint8_t adet;
    uint8_t kesisim;
    uint8_t gorunurluk;
  };

 private:
  const Sahne& s_;
  SonarAyar a_;
  int nh_ = 1, nv_ = 1;
  double Wh_, Wv_, th0_, ph0_;
  std::vector<IsinSonuc> sonuc_;
  std::vector<std::complex<double>> ince_, evr_;
  std::vector<double> ince_I_, evr_I_;
  std::vector<double> cekirdek_;
  // Hücre toplama tablosu (açık adresleme; ekleme sırası = ışın sırası)
  struct Hucre {
    uint32_t k[4];
    double sw, sa, stw;
  };
  std::vector<int32_t> tablo_;
  std::vector<Hucre> hucreler_;
};

}  // namespace mp7
