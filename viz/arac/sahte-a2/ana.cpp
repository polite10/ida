// ida_sahte_a2: A2 tel biçimiyle kinematik senaryo yayınlayan geliştirme aracı.
// Gerçek sim sunucusu DEĞİLDİR (fizik yok, kamera yok). Yalnız görselleştiriciyi
// geliştirmek ve sınamak içindir (W3-viz kapsam md. 5).
//
// Kullanım:
//   ida_sahte_a2 --manifest <yol> [--a2-port <ondalık>] [--kademe sade|tam]
//                [--sure <s>] [--sifirlama <s>] [--hiz <çarpan>] [--sessiz]
//
// --a2-port      A2 portu (A2-K93: 1024–65535, varsayılan 47210)
// --kademe       3B kademesi (hosgeldin.kademe_3b); varsayılan tam
// --sure         bu kadar duvar saniyesi sonra çık (0: sınırsız)
// --sifirlama    senaryonun sıfırlandığı sim saniyesi (0: hiç); varsayılan 121
// --hiz          sim zamanı / duvar saati (varsayılan 1)
#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iterator>
#include <random>
#include <string>
#include <thread>

#include "ida/viz/saat.hpp"
#include "ida/viz/sha256.hpp"
#include "sahte_sim.hpp"
#include "soket.hpp"

#if defined(_WIN32)
#include <process.h>
#else
#include <unistd.h>
#endif

namespace {

std::atomic<bool> g_dur{false};

void sinyal(int) { g_dur.store(true); }

bool tamsayi_oku(const char* s, long long alt, long long ust, long long& out) {
  if (!s || !*s) return false;
  char* son = nullptr;
  const long long v = std::strtoll(s, &son, 10);
  if (*son != '\0' || v < alt || v > ust) return false;
  out = v;
  return true;
}

bool ondalik_oku(const char* s, double alt, double ust, double& out) {
  if (!s || !*s) return false;
  char* son = nullptr;
  const double v = std::strtod(s, &son);
  if (*son != '\0' || !(v >= alt) || !(v <= ust)) return false;
  out = v;
  return true;
}

std::string mutlak_yol(const std::string& yol) {
#if defined(_WIN32)
  char tampon[4096];
  if (_fullpath(tampon, yol.c_str(), sizeof tampon)) return tampon;
  return yol;
#else
  char* r = realpath(yol.c_str(), nullptr);
  if (!r) return yol;
  std::string s(r);
  std::free(r);
  return s;
#endif
}

void kullanim() {
  std::fprintf(stderr,
               "kullanım: ida_sahte_a2 --manifest <yol> [--a2-port <1024-65535>] [--kademe sade|tam]\n"
               "                       [--sure <s>] [--sifirlama <s>] [--hiz <çarpan>] [--sessiz]\n");
}

}  // namespace

int main(int argc, char** argv) {
  using namespace ida::viz;
  using namespace ida::sahte;

  long long port = a2::kVarsayilanPort;
  std::string manifest;
  std::uint8_t kademe = 2;
  double sure_s = 0.0, sifirlama_s = 121.0, hiz = 1.0;
  bool sessiz = false;
  for (int i = 1; i < argc; ++i) {
    const std::string a = argv[i];
    const char* d = (i + 1 < argc) ? argv[i + 1] : nullptr;
    auto gecersiz = [&](const char* ne) {
      std::fprintf(stderr, "ida_sahte_a2: geçersiz %s: %s\n", ne, d ? d : "(yok)");
      kullanim();
      return 2;
    };
    if (a == "--a2-port") {
      if (!tamsayi_oku(d, 1024, 65535, port)) return gecersiz("--a2-port");
      ++i;
    } else if (a == "--manifest") {
      if (!d) return gecersiz("--manifest");
      manifest = d;
      ++i;
    } else if (a == "--kademe") {
      if (!d || (std::strcmp(d, "sade") != 0 && std::strcmp(d, "tam") != 0)) return gecersiz("--kademe");
      kademe = std::strcmp(d, "sade") == 0 ? 1 : 2;
      ++i;
    } else if (a == "--sure") {
      if (!ondalik_oku(d, 0.0, 1e7, sure_s)) return gecersiz("--sure");
      ++i;
    } else if (a == "--sifirlama") {
      if (!ondalik_oku(d, 0.0, 1e7, sifirlama_s)) return gecersiz("--sifirlama");
      ++i;
    } else if (a == "--hiz") {
      if (!ondalik_oku(d, 0.01, 100.0, hiz)) return gecersiz("--hiz");
      ++i;
    } else if (a == "--sessiz") {
      sessiz = true;
    } else {
      std::fprintf(stderr, "ida_sahte_a2: bilinmeyen seçenek: %s\n", a.c_str());
      kullanim();
      return 2;
    }
  }
  if (manifest.empty()) {
    kullanim();
    return 2;
  }

  SenaryoAyarlari ayar;
  ayar.kademe_3b = kademe;
  ayar.manifest_yolu = mutlak_yol(manifest);
  {
    std::ifstream f(ayar.manifest_yolu, std::ios::binary);
    if (!f) {
      std::fprintf(stderr, "ida_sahte_a2: manifest okunamadı: %s\n", ayar.manifest_yolu.c_str());
      return 1;
    }
    const std::string icerik((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    ayar.manifest_ozeti = sha256(icerik.data(), icerik.size());  // A7-K05: ham baytlar
  }
  ayar.sifirlama_ns = static_cast<std::int64_t>(sifirlama_s * 1e9);

  std::signal(SIGINT, sinyal);
  std::signal(SIGTERM, sinyal);
  soketleri_hazirla();

  TcpDinleyici dinleyici;
  std::string hata;
  if (!dinleyici.ac(static_cast<std::uint16_t>(port), hata)) {
    std::fprintf(stderr, "ida_sahte_a2: 127.0.0.1:%lld dinlenemedi: %s\n", port, hata.c_str());
    return 1;
  }
  std::printf("ida_sahte_a2: SAHTE VERİ — kinematik senaryo, fizik değil. 127.0.0.1:%lld, kademe %s, manifest %s (%s)\n",
              port, kademe == 1 ? "sade" : "tam", ayar.manifest_yolu.c_str(),
              onaltilik(ayar.manifest_ozeti).c_str());
  std::fflush(stdout);

  SahteSim sim(ayar);
  TcpSoket istemci;
  std::mt19937_64 rastgele(static_cast<std::uint64_t>(duvar_ns()));
  const std::int64_t baslangic = duvar_ns();
  std::int64_t son_gunluk = baslangic;
  std::string son_olay;
  Baytlar gelen, giden;

  while (!g_dur.load()) {
    const std::int64_t simdi = duvar_ns();
    if (sure_s > 0.0 && static_cast<double>(simdi - baslangic) * 1e-9 >= sure_s) break;

    // Yeni bağlantılar: biri varken ikincisine ret(mesgul) (A2-K03).
    TcpSoket yeni;
    while (dinleyici.kabul(yeni)) {
      if (istemci.durum() == TcpSoket::Durum::bagli) {
        Baytlar r;
        kodla_ret(r, 0, a2::RetNedeni::mesgul, "başka bir görselleştirici bağlı");
        std::string h;
        yeni.yaz(r, h);
        yeni.kapat();
        if (!sessiz) std::printf("ida_sahte_a2: ikinci bağlantı reddedildi (mesgul)\n");
      } else {
        istemci = std::move(yeni);
        std::uint64_t oturum = 0;
        while (oturum == 0) oturum = rastgele();
        sim.baglanti_acildi(simdi, oturum);
        giden.clear();
        if (!sessiz) std::printf("ida_sahte_a2: görselleştirici bağlandı (oturum %016llx)\n",
                                 static_cast<unsigned long long>(oturum));
      }
    }

    sim.sim_ilerle(static_cast<std::int64_t>(static_cast<double>(simdi - baslangic) * hiz));

    if (istemci.durum() == TcpSoket::Durum::bagli) {
      gelen.clear();
      std::string h;
      const bool acik = istemci.oku(gelen, 256u * 1024u, h);  // döngü başına bütçe
      if (!gelen.empty()) sim.veri_geldi(gelen.data(), gelen.size(), simdi);
      sim.zaman(simdi);
      Baytlar yeni_giden = sim.giden_al();
      giden.insert(giden.end(), yeni_giden.begin(), yeni_giden.end());
      if (giden.size() > a2::kGondermeKuyruguAzamiBayt) {
        // A2-K26'nın sade biçimi: kuyruk taşarsa bağlantıyı kapat.
        Baytlar v;
        kodla_veda(v, 0, a2::VedaNedeni::tampon_tasti);
        giden.clear();
        istemci.kapat();
        sim.baglanti_kapandi();
        if (!sessiz) std::printf("ida_sahte_a2: gönderme kuyruğu taştı, bağlantı kapatıldı\n");
      } else {
        istemci.yaz(giden, h);
        if (sim.kapatilmali()) {
          istemci.yaz(giden, h);
          istemci.kapat();
          sim.kapatildi();
          giden.clear();
          if (!sessiz) std::printf("ida_sahte_a2: bağlantı kapatıldı: %s\n", sim.son_olay().c_str());
        } else if (!acik) {
          istemci.kapat();
          sim.baglanti_kapandi();
          giden.clear();
          if (!sessiz) std::printf("ida_sahte_a2: görselleştirici ayrıldı: %s\n", h.c_str());
        }
      }
    } else {
      (void)sim.giden_al();
    }

    if (!sessiz && sim.son_olay() != son_olay) {
      son_olay = sim.son_olay();
      std::printf("ida_sahte_a2: %s\n", son_olay.c_str());
    }
    if (!sessiz && simdi - son_gunluk >= 10'000'000'000) {
      son_gunluk = simdi;
      const auto& s = sim.sayaclar();
      std::printf("ida_sahte_a2: t=%.1f s, nesil %u, durum %llu, hazir %llu, nabız %llu, sıfırlama %llu\n",
                  static_cast<double>(sim.t_ns()) * 1e-9, sim.senaryo().nesil(),
                  static_cast<unsigned long long>(s.durum), static_cast<unsigned long long>(s.hazir),
                  static_cast<unsigned long long>(s.gor_nabzi), static_cast<unsigned long long>(s.sifirlama));
    }
    std::fflush(stdout);
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
  }

  // A2-K67: olağan kapanışta veda(sim_kapaniyor).
  if (istemci.durum() == TcpSoket::Durum::bagli) {
    sim.kapaniyor();
    Baytlar son = sim.giden_al();
    giden.insert(giden.end(), son.begin(), son.end());
    std::string h;
    istemci.yaz(giden, h);
    istemci.kapat();
  }
  std::printf("ida_sahte_a2: çıkış\n");
  return 0;
}
