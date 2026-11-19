// W0-MP3a ölçüm prototipi: seri port soyutlaması (atılabilir).
// Yalnız POSIX/macOS gerçeklemesi var (seri_posix.cpp). Windows gerçeklemesi YAZILMADI; Windows'ta bu araç
// çalıştırılmadı (macOS dışında hız ayarı reddedilir). Masaüstü testleri aynı okuma/yazma kodunu socketpair
// üzerinde, kart mantığını koşan öykünücüyle kullanır (test/sahte_hat).
#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>

namespace mp3 {

enum class Io { Tamam, ZamanAsimi, Kopma };

const char *io_ad(Io s);

struct HizRaporu {
    uint32_t istenen = 0;
    bool tcsetattr_ok = false;
    bool ioctl_ok = false;          // macOS IOSSIOSPEED
    int hata_errno = 0;
    uint64_t geri_ispeed = 0;       // tcgetattr sonrası cfgetispeed (işletim sisteminin bildirdiği; fiziksel hız değil)
    uint64_t geri_ospeed = 0;
    std::string aciklama;
};

class Port {
public:
    virtual ~Port() = default;
    // Hızı ayarlar; işletim sistemi yapılandırma sonucunu doldurur. false: hız kabul edilmedi.
    virtual bool hiz(uint32_t baud, HizRaporu &r) = 0;
    // n baytın TAMAMINI yazar (kısmi yazımları birleştirir) ya da zaman aşımı/kopma döner; `yazilan` gerçek sayı.
    virtual Io yaz(const uint8_t *p, size_t n, int zaman_asimi_ms, size_t &yazilan) = 0;
    // En az 1, en çok n bayt okur; süre dolarsa ZamanAsimi (okunan = 0).
    virtual Io oku(uint8_t *p, size_t n, int zaman_asimi_ms, size_t &okunan) = 0;
    // Girişte bekleyen baytları atar.
    virtual void giris_at() = 0;
    virtual std::string ad() const = 0;
};

// POSIX seri port (8N1, ham kip, akış denetimi yok, O_NONBLOCK + poll). Açılamazsa nullptr.
Port *posix_ac(const std::string &yol, std::string &hata);

// Yalnız masaüstü testleri: var olan bir fd (ör. socketpair ucu) üzerinde aynı okuma/yazma kodu; hız ayarı
// termios/ioctl yerine kancaya iletilir (kanca false dönerse hız reddedilmiş sayılır). `yazma_kanca` verilirse
// ::write yerine o çağrılır (kısmi yazma ve gecikme enjeksiyonu). Nesne fd'yi kapatır.
using YazmaKanca = std::function<long(int fd, const uint8_t *p, size_t n)>;
Port *posix_fd_ac(int fd, const std::string &ad, std::function<bool(uint32_t)> hiz_kanca, YazmaKanca yazma_kanca = nullptr);

}  // namespace mp3
