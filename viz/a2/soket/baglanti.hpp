// İstemci bağlantısı: TCP soketi + A2 istemci oturumu + yeniden bağlanma (A2-K63:
// bağlanamazsa ya da koparsa 1 s aralıkla yeniden dener; A2-K16: ret(surum/manifest)
// sonrası denemez). Her çizim karesinde `adim` çağrılır; hiçbir çağrı bloklamaz.
#ifndef IDA_VIZ_BAGLANTI_HPP
#define IDA_VIZ_BAGLANTI_HPP

#include <cstdint>
#include <string>

#include "ida/viz/oturum.hpp"
#include "soket.hpp"

namespace ida::viz {

class IstemciBaglantisi {
 public:
  enum class Durum : std::uint8_t { bekliyor, baglaniyor, bagli, durdu };

  IstemciBaglantisi(IstemciOturumu::Ayarlar ayarlar, std::uint16_t port);

  void adim(std::int64_t simdi_ns, std::uint32_t cizim_hizi_mhz, bool pencere_cizilebilir);
  void olagan_kapat();  // veda(olagan) gönderip soketi kapatır

  IstemciOturumu& oturum() { return oturum_; }
  const IstemciOturumu& oturum() const { return oturum_; }
  Durum durum() const { return durum_; }
  const std::string& son_hata() const { return son_hata_; }
  std::uint32_t baglanti_sayisi() const { return baglanti_sayisi_; }
  std::uint16_t port() const { return port_; }
  std::uint64_t okunan_bayt() const { return okunan_bayt_; }

  // Bir `adim` çağrısında soketten okunacak en çok bayt (sol #1).
  static constexpr std::size_t kAdimOkumaButcesi = 1u << 20;
  static constexpr std::size_t kParcaBoyutu = 64u * 1024u;

 private:
  void gonder();
  void kopar(const std::string& neden, std::int64_t simdi_ns);

  IstemciOturumu oturum_;
  std::uint16_t port_;
  TcpSoket soket_;
  Durum durum_ = Durum::bekliyor;
  std::int64_t sonraki_deneme_ns_ = 0;
  Baytlar giden_;
  Baytlar gelen_;
  std::string son_hata_;
  std::uint32_t baglanti_sayisi_ = 0;
  std::uint64_t okunan_bayt_ = 0;
};

}  // namespace ida::viz

#endif
