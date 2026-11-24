// A1 taşıması (W4-kopru): bayt akışı soyutlaması ve üç gerçekleme.
//
//   bellek_cifti   aynı süreçte iki uç (sınama ve süreç içi karşı taraf); kayıpsız, FIFO
//   TcpDinleyici   SIL: yalnız 127.0.0.1'de dinler, tek bağlantı kabul eder, TCP_NODELAY (A1-K97)
//   seri_ac        HIL: ST-LINK sanal seri portu, 3 000 000 baud 8N1, akış denetimi yok, ham kip
//                  (A1-K01); macOS'ta hız IOSSIOSPEED ile (W0-MP3'ün kanıtlanmış ayarı), Windows'ta
//                  DCB ile
//
// Taşıma çerçeve bilmez; A1-K10 (tam çerçeveyi kesintisiz yazma) çağıranın, `yaz`'ın bütün
// baytları yazmasıyla sağlanır. Bir uç aynı anda tek iş parçacığından yazılır ve tek iş
// parçacığından okunur.
#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>

namespace ida::kopru {

enum class OkumaSonucu {
    tamam,         // okunan > 0
    zaman_asimi,   // bekleme süresinde bayt gelmedi
    kapandi,       // karşı uç kapandı ya da aygıt kalktı
};

class Tasima {
public:
    virtual ~Tasima() = default;
    // Engellemeden yazabildiği kadarını yazar (sol #7): dönüş yazılan bayt (0: şu an ilerleme
    // yok), −1: kapandı ya da hata. Kısmi yazımın konumunu çağıran tutar.
    virtual long long yaz_biraz(const std::uint8_t* p, std::size_t n) = 0;
    // n baytın tamamını en çok sure_us içinde yazar (kısmi yazımları birleştirir). İlerlemesiz
    // yazımda önce iş parçacığı sırası bırakılır (ardışık 64 kez), sonra kısa uykuyla beklenir:
    // uykunun çözünürlüğü platforma bağlıdır (Windows'ta varsayılan ~15,6 ms), ilerleme sürerken
    // hız ona bağlı kalmaz. false: kapandı, hata ya da süre doldu (bağlantı sorunu). yazilan_cikti
    // verilirse başarısızlıkta da yazılmış önekin boyu yazılır (çağıran o konumdan sürdürebilir).
    bool yaz(const std::uint8_t* p, std::size_t n, std::int64_t sure_us = 2'000'000,
             std::size_t* yazilan_cikti = nullptr);
    // En az 1, en çok n bayt okur; en çok bekle_us mikrosaniye bekler (0: beklemez).
    virtual OkumaSonucu oku(std::uint8_t* p, std::size_t n, std::int64_t bekle_us,
                            std::size_t& okunan) = 0;
    // İşletim sisteminin alım tamponunu boşaltır (A1-K12; yalnız seri portta anlamlıdır).
    virtual void giris_bosalt() = 0;
    virtual bool acik() const = 0;
    virtual void kapat() = 0;
    // Düzgün kapanış (W4-K25 (5)): yazma yönü kapatılır (gönderilmiş baytlar ve ardından FIN
    // gider), karşı taraf kapatana ya da en çok sure_us geçene dek gelen baytlar okunup atılır,
    // sonra kapat(). Okunmamış gelen veriyle kapatma bağlantıyı RST ile keser (RFC 1122
    // §4.2.2.13); karşı tarafın yazımı hata alır ve son çerçeveyi okumadan kapanabilir. Atılan
    // baytlar işlenmez. TCP dışındaki taşımalarda (seri port, bellek) kapat() ile aynıdır.
    virtual void kapat_duzgun(std::int64_t /*sure_us*/) { kapat(); }
    virtual std::string ad() const = 0;
    // Tanı: son başarısız işlemin işletim sistemi hata kodu (POSIX errno, Winsock
    // WSAGetLastError); hata yoksa ya da taşıma bildirmiyorsa 0.
    virtual int son_hata_kodu() const { return 0; }
};

// Aynı süreçteki iki uç: birinin yazdığını öteki okur. İki uç farklı iş parçacıklarından
// kullanılabilir. Bir uç kapanınca öteki okumada bekleyen baytları bitirince `kapandi` alır.
std::pair<std::unique_ptr<Tasima>, std::unique_ptr<Tasima>> bellek_cifti();

// SIL TCP sunucusu (A1-K97): yalnız 127.0.0.1. port = 0 ise işletim sistemi boş port seçer.
class TcpDinleyici {
public:
    virtual ~TcpDinleyici() = default;
    virtual std::uint16_t port() const = 0;
    // Bir bağlantı kabul eder (TCP_NODELAY); bekle_us içinde gelmezse nullptr.
    virtual std::unique_ptr<Tasima> kabul(std::int64_t bekle_us, std::string& hata) = 0;
};

std::unique_ptr<TcpDinleyici> tcp_dinle(std::uint16_t port, std::string& hata);

// İstemci (sınama ve süreç içi karşı taraf): 127.0.0.1:port'a bağlanır (TCP_NODELAY).
std::unique_ptr<Tasima> tcp_baglan(std::uint16_t port, std::string& hata);

struct SeriAyar {
    std::uint32_t baud = 3'000'000;   // A1 §3.1
    // false: hız kurulamazsa yine aç (yalnız sözde uçbirimle sınama; hata metni uyarı olur).
    bool hiz_zorunlu = true;
};

// HIL seri portu (A1-K01). macOS/POSIX: aygıt yolu (/dev/cu.usbmodem…); Windows: COMn.
std::unique_ptr<Tasima> seri_ac(const std::string& yol, const SeriAyar& ayar, std::string& hata);

// Sözde uçbirim çifti (yalnız POSIX; HIL yolunun kartsız sınaması): `ana` ucu karşı tarafa,
// `yol` seri_ac'a verilir. Windows'ta desteklenmez (ana = nullptr, hata metni).
struct SozdeUcbirim {
    std::unique_ptr<Tasima> ana;
    std::string yol;
};
SozdeUcbirim sozde_ucbirim_ac(std::string& hata);

}  // namespace ida::kopru
