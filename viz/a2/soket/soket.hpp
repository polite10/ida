// Bloklamayan yerel TCP soketi (A2-K01: yalnız 127.0.0.1; A2-K04: TCP_NODELAY).
// macOS'ta POSIX soketleri, Windows'ta Winsock (derleme: .github/workflows/viz-windows.yml).
#ifndef IDA_VIZ_SOKET_HPP
#define IDA_VIZ_SOKET_HPP

#include <cstddef>
#include <cstdint>
#include <string>

#include "ida/viz/kodek.hpp"

namespace ida::viz {

#if defined(_WIN32)
using SoketTutamagi = std::uintptr_t;
#else
using SoketTutamagi = int;
#endif

class TcpSoket {
 public:
  enum class Durum : std::uint8_t { kapali, baglaniyor, bagli };

  TcpSoket() = default;
  ~TcpSoket();
  TcpSoket(const TcpSoket&) = delete;
  TcpSoket& operator=(const TcpSoket&) = delete;
  TcpSoket(TcpSoket&& o) noexcept;
  TcpSoket& operator=(TcpSoket&& o) noexcept;

  // 127.0.0.1:port'a bloklamadan bağlanmaya başlar.
  bool baglan_baslat(std::uint16_t port, std::string& hata);
  // Bağlanma sürüyorsa tamamlandı mı diye bakar (bloklamaz).
  Durum durum_yokla(std::string& hata);
  // Okunabilen baytlardan en çok `azami` kadarını `out`'a ekler (bloklamaz). false:
  // bağlantı kapandı ya da hata (o ana kadar okunanlar yine `out`'tadır).
  bool oku(Baytlar& out, std::size_t azami, std::string& hata);
  // `tampon`'un başından yazabildiğini yazar ve tampondan siler. false: hata.
  bool yaz(Baytlar& tampon, std::string& hata);
  void kapat();
  Durum durum() const { return durum_; }

  // Dinleyicinin kabul ettiği tutamakla kurmak için.
  static TcpSoket kabul_edilen(SoketTutamagi t);

 private:
  SoketTutamagi t_ = gecersiz();
  Durum durum_ = Durum::kapali;
  static SoketTutamagi gecersiz();
};

class TcpDinleyici {
 public:
  TcpDinleyici() = default;
  ~TcpDinleyici();
  TcpDinleyici(const TcpDinleyici&) = delete;
  TcpDinleyici& operator=(const TcpDinleyici&) = delete;

  bool ac(std::uint16_t port, std::string& hata);
  // Bekleyen bir bağlantı varsa kabul eder (bloklamaz).
  bool kabul(TcpSoket& out);
  void kapat();

 private:
  SoketTutamagi t_ = static_cast<SoketTutamagi>(-1);
  bool acik_ = false;
};

// Soket katmanının süreç başında bir kez hazırlanması (Windows'ta WSAStartup).
bool soketleri_hazirla();

}  // namespace ida::viz

#endif
