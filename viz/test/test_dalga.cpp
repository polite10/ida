// sol #11 (A2 §3.4: aynı bileşenler, aynı formül, kesmeden): gölgelendiricinin yükseklik
// toplamı, CPU'daki dalga_yuksekligi() ile aynı paket ve aynı formülden gelir.
//  1) Paketlenen değerlerle float toplam, double CPU toplamıyla aynı yüksekliği verir
//     (uzak noktalar dahil; fark yalnız float yuvarlaması).
//  2) Gölgelendiricideki toplam satırı genliği değiştirmez: paket genliği `c.z` doğrudan
//     çarpan olarak kullanılır; ağ aralığına bağlı bir çarpan (önceki `smoothstep` sönümü) yoktur.
#include <cmath>
#include <fstream>
#include <sstream>
#include <string>

#include "deneme.hpp"
#include "ida/viz/dalga.hpp"
#include "senaryo.hpp"

using namespace ida::viz;

namespace {

std::string dosya(const char* yol) {
  std::ifstream f(yol);
  std::stringstream s;
  s << f.rdbuf();
  return s.str();
}

}  // namespace

int main() {
  ida::sahte::Senaryo sen(ida::sahte::SenaryoAyarlari{});
  const auto& b = sen.dalga().bilesenler;
  DENETLE(b.size() == 8);
  for (std::int64_t t : {0LL, 1'234'567'890LL, 3'600'000'000'000LL}) {
    DalgaPaketi p;
    dalga_paketle(b, t, p);
    double en_buyuk = 0.0;
    for (int i = -20; i <= 20; ++i) {
      for (int j = -20; j <= 20; ++j) {
        // yakın ağ (0,12 m) ve uzak halka (yüzlerce m) noktaları
        const double x = (std::abs(i) < 10 ? 0.12 : 25.0) * i;
        const double z = (std::abs(j) < 10 ? 0.12 : 25.0) * j;
        const double cpu = dalga_yuksekligi(b, -z, x, t);  // NED: N = −z, E = x
        const double gpu = static_cast<double>(dalga_paketi_yuksekligi(p, static_cast<float>(x), static_cast<float>(z)));
        en_buyuk = std::fmax(en_buyuk, std::fabs(cpu - gpu));
      }
    }
    std::printf("t = %lld ns: |CPU − paket| en çok %.3e m\n", static_cast<long long>(t), en_buyuk);
    DENETLE(en_buyuk < 1e-3);  // float yuvarlaması; A2-U19 eşiği max(1 mm, %1 H_s)
  }
  const std::string g = dosya(IDA_VIZ_DALGA_GOLGELENDIRICI);
  DENETLE(!g.empty());
  DENETLE(g.find("smoothstep") == std::string::npos);
  DENETLE(g.find("float th = dalga_faz[i] - (c.x * p.x + c.y * p.y);") != std::string::npos);
  DENETLE(g.find("h += c.z * cos(th);") != std::string::npos);
  DENETLE(g.find("g += c.z * sin(th) * c.xy;") != std::string::npos);
  DENEME_SONUC("viz.dalga");
}
