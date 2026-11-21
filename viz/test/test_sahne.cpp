// Ara değerleme ve operatör saati (A2-K30–K35).
#include <cmath>
#include <limits>

#include "deneme.hpp"
#include "ida/viz/sahne.hpp"

using namespace ida::viz;
namespace a2 = ida::a2;

namespace {

constexpr std::int64_t kMs = 1'000'000;

a2::GovdeDurumu durum(double n, double vn, std::uint32_t sureklilik, double sapma = 0.0) {
  a2::GovdeDurumu d{};
  d.govde_kimligi = 1;
  d.govde_turu = 1;
  d.bayraklar = a2::kGovdeGecerli;
  d.sureklilik_no = sureklilik;
  d.p_n[0] = n;
  d.v_n[0] = vn;
  const Q q = euler_zyx(sapma, 0.0, 0.0);
  d.q_nb[0] = q.w;
  d.q_nb[1] = q.x;
  d.q_nb[2] = q.y;
  d.q_nb[3] = q.z;
  return d;
}

void hermite_ve_slerp() {
  Sahne s;
  GovdeKaydi& g = s.ekle(1, 0, 1);
  // Sabit 2 m/s: Hermite doğrusal hareketi birebir verir.
  Sahne::ornek_ekle(g, 0, durum(0.0, 2.0, 0, 0.0));
  Sahne::ornek_ekle(g, 100 * kMs, durum(0.2, 2.0, 0, 1.0));
  Poz p;
  DENETLE(Sahne::poz(g, 25 * kMs, p));
  YAKIN(p.p.x, 0.05, 1e-12);
  // Yönelim: yarı yolda sapma 0,5 rad.
  DENETLE(Sahne::poz(g, 50 * kMs, p));
  const Q beklenen = euler_zyx(0.5, 0.0, 0.0);
  YAKIN(std::fabs(qnokta(p.q, beklenen)), 1.0, 1e-12);
  // Son durumun ötesinde ekstrapolasyon yok (A2-K30).
  DENETLE(Sahne::poz(g, 300 * kMs, p));
  YAKIN(p.p.x, 0.2, 1e-15);
}

void sureklilik_siniri() {
  // A2-K21/K32: T = 100 ms'de süreklilik değişti; sim T − h (eski no) ve T (yeni no) gönderir.
  const std::int64_t h = 1'250'000;
  const std::int64_t T = 100 * kMs;
  Sahne s;
  GovdeKaydi& g = s.ekle(1, 0, 1);
  Sahne::ornek_ekle(g, 0, durum(0.0, 1.0, 0));
  Sahne::ornek_ekle(g, T - h, durum(0.09875, 1.0, 0));
  Sahne::ornek_ekle(g, T, durum(5.0, -3.0, 1));  // olay sonrası: sıçrama
  Sahne::ornek_ekle(g, T + 16 * kMs, durum(4.952, -3.0, 1));
  Poz p;
  // t_op < T − h: eski parçanın içinde ara değer
  DENETLE(Sahne::poz(g, 50 * kMs, p));
  YAKIN(p.p.x, 0.05, 1e-9);
  // T − h ≤ t_op < T: eski parçanın son durumu değişmeden
  DENETLE(Sahne::poz(g, T - h, p));
  YAKIN(p.p.x, 0.09875, 1e-15);
  DENETLE(Sahne::poz(g, T - 1, p));
  YAKIN(p.p.x, 0.09875, 1e-15);
  // t_op = T: olay sonrası durum
  DENETLE(Sahne::poz(g, T, p));
  YAKIN(p.p.x, 5.0, 1e-15);
  // t_op > T: yeni parçanın içinde ara değer; eski parçaya doğru eğri yok
  DENETLE(Sahne::poz(g, T + 8 * kMs, p));
  DENETLE(p.p.x <= 5.0 && p.p.x >= 4.95);
}

void dogum_olum() {
  Sahne s;
  GovdeKaydi& g = s.ekle(1, 0, 2);
  g.t_dogum = 100 * kMs;
  g.t_olum = 300 * kMs;
  Sahne::ornek_ekle(g, 100 * kMs, durum(0.0, 0.0, 0));
  DENETLE(!Sahne::gorunur(g, 99 * kMs));   // A2-K33: t_op < T çizilmez
  DENETLE(Sahne::gorunur(g, 100 * kMs));
  DENETLE(Sahne::gorunur(g, 299 * kMs));
  DENETLE(!Sahne::gorunur(g, 300 * kMs));  // ölüm anında ve sonra çizilmez
  int silinen = 0;
  s.buda(299 * kMs, [&](std::uint32_t) { ++silinen; });
  DENETLE(silinen == 0);
  s.buda(300 * kMs, [&](std::uint32_t) { ++silinen; });
  DENETLE(silinen == 1 && s.govdeler().empty());
}

void gecmis_budama() {
  Sahne s;
  GovdeKaydi& g = s.ekle(1, 0, 1);
  for (int i = 0; i <= 120; ++i) Sahne::ornek_ekle(g, i * 16 * kMs, durum(i * 0.01, 0.0, 0));
  s.buda(120 * 16 * kMs, [](std::uint32_t) {});
  // A2-K34: t_op'tan en az 500 ms geriye kadar örnek kalır.
  DENETLE(!g.gecmis.empty());
  DENETLE(g.gecmis.front().t_ns <= 120 * 16 * kMs - 500 * kMs);
  DENETLE(g.gecmis.size() < 121);
}

// A2-K31 aynen: her karede t_op = t_son − D_op (sol #5'in karşı örnekleri, kesin beklenti).
void operator_saati() {
  {  // D_op = 0: sol'un örneği (ilk durum 1 s, 10 ms sonra 1,1 s → t_op = 1,1 s; eskisi 1,0118 s)
    OperatorSaati saat(0);
    std::int64_t t_op = 0;
    DENETLE(!saat.ilerle(0, t_op));
    saat.durum_alindi(1'000'000'000, 0);
    DENETLE(saat.ilerle(0, t_op));
    DENETLE(t_op == 1'000'000'000);
    saat.durum_alindi(1'100'000'000, 10'000'000);
    DENETLE(saat.ilerle(10'000'000, t_op));
    DENETLE(t_op == 1'100'000'000);
  }
  // 60 Hz durum; duvar hızı sırasıyla 1×, 2× (hızlandırılmış SIL), 0,5× ve yine 1×;
  // çizim duvar saatinde 8,33 ms'de bir. Her karede t_op = t_son − D_op.
  const std::int64_t d_op = 33'333'000;
  OperatorSaati saat(d_op);
  std::int64_t t_op = 0;
  std::int64_t t_son = 0;
  std::int64_t duvar = 1'000'000'000;
  bool kesin = true, asmadi = true, azalmadi = true;
  std::int64_t onceki = std::numeric_limits<std::int64_t>::min();
  const double hizlar[] = {1.0, 2.0, 0.5, 1.0};
  std::int64_t k = 0;
  for (double hiz : hizlar) {
    const auto aralik = static_cast<std::int64_t>(16'666'667.0 / hiz);  // duvar saatinde durum aralığı
    for (int i = 0; i < 300; ++i, ++k) {
      t_son = k * 16'666'667LL;
      saat.durum_alindi(t_son, duvar);
      for (std::int64_t d = 0; d < aralik; d += 8'333'333) {
        DENETLE(saat.ilerle(duvar + d, t_op));
        if (t_op != t_son - d_op) kesin = false;
        if (t_op > t_son) asmadi = false;
        if (t_op < onceki) azalmadi = false;
        onceki = t_op;
      }
      duvar += aralik;
    }
  }
  DENETLE(kesin);     // A2-K31
  DENETLE(asmadi);    // A2-K30
  DENETLE(azalmadi);  // A2-K35
  // Veri kesilirse son durumda bekler (t_op değişmez, ekstrapolasyon yok), bekleme sayılır.
  const std::uint32_t bekleme0 = saat.bekleme_sayaci();
  for (int i = 0; i < 100; ++i) {
    DENETLE(saat.ilerle(duvar + i * 8'333'333LL, t_op));
    DENETLE(t_op == t_son - d_op);
  }
  DENETLE(saat.bekleme_sayaci() > bekleme0);
  // Sıfırlamadan sonra azalabilir (A2-K35 istisnası).
  saat.sifirla();
  saat.durum_alindi(0, duvar);
  DENETLE(saat.ilerle(duvar, t_op));
  DENETLE(t_op == -d_op);
}

}  // namespace

int main() {
  hermite_ve_slerp();
  sureklilik_siniri();
  dogum_olum();
  gecmis_budama();
  operator_saati();
  DENEME_SONUC("viz.sahne");
}
