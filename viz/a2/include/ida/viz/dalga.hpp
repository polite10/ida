// Dalga yüzeyi (A2 §3.4, faz kuralı bağlayıcı; U-17, AB-190):
//   ζ(x, y, t) = Σ a_i · cos(ω_i·t − k_i·(x·cos β_i + y·sin β_i) + φ_i),  x = N, y = E (m),
//   t = t_ns · 1e-9 s; ζ ortalama yüzeye (D = 0) göre yukarı pozitif.
// Görselleştirici aynı kümeden, aynı formülle ve kesme yapmadan çizer (07 §2.7). Zaman terimi
// (ω t + φ) mod 2π CPU'da double hesaplanır; gölgelendiriciye yalnız bu faz verilir.
#ifndef IDA_VIZ_DALGA_HPP
#define IDA_VIZ_DALGA_HPP

#include <cstdint>
#include <vector>

#include "ida/a2_wire.h"

namespace ida::viz {

// Bir bileşenin zaman fazı: (ω·t + φ) mod 2π, [0, 2π) aralığında; t tamsayı ns'den
// double hassasiyeti kaybetmeden (t'nin tam saniye kısmı ayrı çarpılır).
double dalga_fazi(const a2::DalgaBileseni& b, std::int64_t t_ns);

// ζ(N, E, t) metre, yukarı pozitif.
double dalga_yuksekligi(const std::vector<a2::DalgaBileseni>& bilesenler, double n, double e, std::int64_t t_ns);

// ζ'nin yatay türevleri (∂ζ/∂N, ∂ζ/∂E); sahte kaynağın yalpa/yunuslaması için.
void dalga_egimi(const std::vector<a2::DalgaBileseni>& bilesenler, double n, double e, std::int64_t t_ns,
                 double& dzeta_dn, double& dzeta_de);

// Gölgelendiriciye giden paket (viz/proje/golgelendiriciler/dalga.gdshaderinc ile aynı düzen):
// bileşen i için k_i = (k sin β, −k cos β, a, k) ve θ_i = dalga_fazi(t). Godot dünyasında
// ζ(x_G, z_G) = Σ a·cos(θ − (kx·x_G + kz·z_G)); x_G = E, z_G = −N. Eklenti bu işlevi kullanır.
struct DalgaPaketi {
  std::vector<float> kx, kz, a, k, faz;
};
void dalga_paketle(const std::vector<a2::DalgaBileseni>& bilesenler, std::int64_t t_ns, DalgaPaketi& out);

// Gölgelendiricinin toplamının CPU'daki float karşılığı (aynı formül, aynı sabitler; testler).
float dalga_paketi_yuksekligi(const DalgaPaketi& p, float x_g, float z_g);

}  // namespace ida::viz

#endif
