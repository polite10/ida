// sol #1: soketten okuma yineleme başına bütçeyle sınırlı; alınan parça hemen çerçevelenip
// denetlenir. Karşı örnek: kaynak bozuk bir başlıkla başlayıp durmadan veri gönderir. Bir
// `adim` çağrısı bütçeden fazla bayt okumamalı ve bozuk başlık ilk parçada protokol hatası
// olarak yakalanıp bağlantı kapatılmalıdır (eskiden `oku()` veri geldikçe sınırsız biriktirdi).
#include <atomic>
#include <chrono>
#include <cstdio>
#include <thread>

#include "baglanti.hpp"
#include "deneme.hpp"
#include "ida/viz/saat.hpp"
#include "soket.hpp"

using namespace ida::viz;

int main() {
  soketleri_hazirla();
  const auto port = static_cast<std::uint16_t>(47800 + (duvar_ns() / 1000) % 150);
  TcpDinleyici dinleyici;
  std::string h;
  DENETLE(dinleyici.ac(port, h));
  IstemciBaglantisi ist(IstemciOturumu::Ayarlar{}, port);

  TcpSoket sunucu;
  for (int i = 0; i < 500 && sunucu.durum() != TcpSoket::Durum::bagli; ++i) {
    ist.adim(duvar_ns(), 0, true);
    dinleyici.kabul(sunucu);
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
  }
  for (int i = 0; i < 100 && ist.durum() != IstemciBaglantisi::Durum::bagli; ++i) {
    ist.adim(duvar_ns(), 0, true);
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
  }
  DENETLE(ist.durum() == IstemciBaglantisi::Durum::bagli);

  // Yazıcı: sıfırlarla (sihirli sayı yanlış) durmadan yazar.
  std::atomic<bool> dur{false};
  std::atomic<std::uint64_t> yazilan{0};
  std::thread yazici([&] {
    std::string hata;
    while (!dur.load() && sunucu.durum() == TcpSoket::Durum::bagli) {
      Baytlar parca(256 * 1024, 0);
      const std::size_t once = parca.size();
      if (!sunucu.yaz(parca, hata)) break;
      yazilan += once - parca.size();
    }
  });
  std::this_thread::sleep_for(std::chrono::milliseconds(300));

  const std::uint64_t once = ist.okunan_bayt();
  ist.adim(duvar_ns(), 0, true);
  const std::uint64_t okunan = ist.okunan_bayt() - once;
  std::printf("tek adımda okunan: %llu B (yazılan toplam %llu B)\n", static_cast<unsigned long long>(okunan),
              static_cast<unsigned long long>(yazilan.load()));
  DENETLE(okunan <= IstemciBaglantisi::kAdimOkumaButcesi);
  DENETLE(ist.durum() != IstemciBaglantisi::Durum::bagli);  // bozuk başlık: kapatıldı
  dur = true;
  yazici.join();
  DENEME_SONUC("viz.okuma_butcesi");
}
