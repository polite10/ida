// Soketli yeniden bağlanma (W3-viz kabul md. 4'ün eklentisiz yarısı; A2-K63, K65, K67):
// sahte kaynak (aynı süreçte, gerçek TCP soketiyle) önce habersiz kapanır, sonra
// veda(sim_kapaniyor) ile kapanır; ikisinde de istemci bağlantı yokken 1 s aralıkla dener ve
// kaynak yeniden açılınca yeni oturumla tam el sıkışma yapar.
#include <chrono>
#include <cstdio>
#include <thread>

#include "baglanti.hpp"
#include "deneme.hpp"
#include "ida/viz/saat.hpp"
#include "sahte_sim.hpp"
#include "soket.hpp"

using namespace ida::viz;
using namespace ida::sahte;

namespace {

struct Kaynak {
  TcpDinleyici dinleyici;
  TcpSoket soket;
  SahteSim sim;
  Baytlar giden;
  std::int64_t baslangic;
  bool acik = false;

  explicit Kaynak(SenaryoAyarlari a) : sim(std::move(a)), baslangic(duvar_ns()) {}

  bool ac(std::uint16_t port) {
    std::string h;
    acik = dinleyici.ac(port, h);
    if (!acik) std::fprintf(stderr, "dinleyici açılamadı: %s\n", h.c_str());
    return acik;
  }
  void kapat_habersiz() {
    soket.kapat();
    dinleyici.kapat();
    sim.baglanti_kapandi();
    acik = false;
  }
  void kapat_vedayla() {
    sim.kapaniyor();
    Baytlar b = sim.giden_al();
    giden.insert(giden.end(), b.begin(), b.end());
    std::string h;
    soket.yaz(giden, h);
    kapat_habersiz();
  }
  void adim() {
    if (!acik) return;
    const std::int64_t simdi = duvar_ns();
    TcpSoket yeni;
    if (dinleyici.kabul(yeni)) {
      soket = std::move(yeni);
      sim.baglanti_acildi(simdi, static_cast<std::uint64_t>(simdi));
      giden.clear();
    }
    sim.sim_ilerle(simdi - baslangic);
    if (soket.durum() != TcpSoket::Durum::bagli) {
      (void)sim.giden_al();
      return;
    }
    Baytlar gelen;
    std::string h;
    const bool a = soket.oku(gelen, 256u * 1024u, h);
    if (!gelen.empty()) sim.veri_geldi(gelen.data(), gelen.size(), simdi);
    sim.zaman(simdi);
    Baytlar b = sim.giden_al();
    giden.insert(giden.end(), b.begin(), b.end());
    soket.yaz(giden, h);
    if (sim.kapatilmali()) {
      soket.kapat();
      sim.kapatildi();
    } else if (!a) {
      soket.kapat();
      sim.baglanti_kapandi();
    }
  }
};

struct Istemci {
  IstemciBaglantisi b;
  int sahne_kur = 0;
  std::uint64_t son_oturum = 0;
  explicit Istemci(std::uint16_t port) : b(IstemciOturumu::Ayarlar{}, port) {}
  void adim() {
    b.adim(duvar_ns(), 60'000, true);
    for (const Olay& o : b.oturum().olaylari_al()) {
      if (o.tur == OlayTuru::sahne_kur) {
        ++sahne_kur;
        son_oturum = b.oturum().oturum_kimligi();
        b.oturum().sahne_sonucu(a2::HazirDurumu::basarili, sha256("m", 1));
      }
    }
  }
};

template <class F>
void kos(double saniye, F&& f) {
  const auto son = std::chrono::steady_clock::now() + std::chrono::duration<double>(saniye);
  while (std::chrono::steady_clock::now() < son) {
    f();
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
  }
}

}  // namespace

int main() {
  soketleri_hazirla();
  const auto port = static_cast<std::uint16_t>(47300 + (duvar_ns() / 1000) % 500);
  SenaryoAyarlari a;
  a.manifest_yolu = "/yok/manifest.json";
  a.manifest_ozeti = sha256("m", 1);

  Istemci ist(port);
  {
    Kaynak k(a);
    DENETLE(k.ac(port));
    kos(1.5, [&] { k.adim(); ist.adim(); });
    DENETLE(ist.b.durum() == IstemciBaglantisi::Durum::bagli);
    DENETLE(ist.b.oturum().asama() == Asama::akis);
    DENETLE(ist.sahne_kur == 1);
    // 1) Kaynak habersiz kapanır (çökme benzeri).
    k.kapat_habersiz();
  }
  const std::uint64_t ilk_oturum = ist.son_oturum;
  kos(2.5, [&] { ist.adim(); });
  DENETLE(ist.b.durum() != IstemciBaglantisi::Durum::bagli);
  DENETLE(ist.b.durum() != IstemciBaglantisi::Durum::durdu);  // yeniden denemeye devam
  // Sahne donmuş kalır: gövdeler silinmedi (A2-K63).
  DENETLE(!ist.b.oturum().sahne().govdeler().empty());
  {
    Kaynak k(a);
    DENETLE(k.ac(port));
    kos(2.5, [&] { k.adim(); ist.adim(); });
    DENETLE(ist.b.durum() == IstemciBaglantisi::Durum::bagli);
    DENETLE(ist.b.oturum().asama() == Asama::akis);
    DENETLE(ist.sahne_kur == 2);
    DENETLE(ist.son_oturum != ilk_oturum);  // A2-K65: yeni oturum
    // 2) Kaynak veda(sim_kapaniyor) ile kapanır.
    k.kapat_vedayla();
  }
  kos(1.5, [&] { ist.adim(); });
  DENETLE(ist.b.durum() != IstemciBaglantisi::Durum::bagli);
  {
    Kaynak k(a);
    DENETLE(k.ac(port));
    kos(2.5, [&] { k.adim(); ist.adim(); });
    DENETLE(ist.b.durum() == IstemciBaglantisi::Durum::bagli);
    DENETLE(ist.sahne_kur == 3);
    DENETLE(ist.b.baglanti_sayisi() == 3);
  }
  DENEME_SONUC("viz.yeniden_baglanma");
}
