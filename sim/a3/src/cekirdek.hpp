// cekirdek.hpp — A3 sunucusunun soketten bağımsız çekirdeği. Bağlantılar bayt akışı olarak
// beslenir, çıkış baytları alınır; soket döngüsü (sunucu.cpp) bunu sarar. Sınamalar ve dayanıklılık
// sınaması çekirdeği soketsiz, aynı süreçte sürer. Yalnız tek iş parçacığından kullanılır.
#pragma once

#include "http.hpp"
#include "ida/a3/katalog.hpp"
#include "ida/a3/komut.hpp"
#include "ida/a3/sunucu.hpp"
#include "json.hpp"
#include "sema.hpp"
#include "yayinci_ic.hpp"

#include <array>
#include <chrono>
#include <cstdint>
#include <deque>
#include <map>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace ida::a3 {

using Zaman = std::chrono::steady_clock::time_point;
using BagId = std::uint64_t;

// Mimari seçimleri (A3 §8 md.1; tahmin) ve bu uygulamanın sınırları.
namespace sinir {
inline constexpr std::size_t govde = std::size_t{1} << 20u;          // A3-K51
inline constexpr std::size_t senaryo_govde = std::size_t{4} << 20u;  // A3-K51
inline constexpr std::size_t govde_onizleme = 4096;                  // A3-K51
inline constexpr std::size_t ws_ileti = 64 * 1024;                   // A3-K47 (istemciden)
inline constexpr std::size_t ikili_ileti = std::size_t{1} << 20u;    // A3-K20
inline constexpr std::size_t olay_kuyrugu = 4096;                    // A3-K42
inline constexpr std::size_t olay_gecmisi = 65536;                   // A3-K45
inline constexpr std::size_t olay_gecmisi_bayt = std::size_t{64} << 20u;  // bu uygulama: bellek tavanı
inline constexpr std::size_t komut_gecmisi = 10000;                  // A3-K60
inline constexpr std::size_t yineleme_kaydi = 10000;                 // A3-K53
inline constexpr std::size_t onay_jetonu = 1024;                     // bu uygulama
inline constexpr std::size_t istemci = 1024;                         // bu uygulama
inline constexpr std::size_t birlesen_kayit = 64;                    // W4-K22: mandal başına birleşen sonuç kaydı
inline constexpr std::size_t birlesik_sonuc = 1024;                  // W4-K22: bekleyen birleşik sonuç kaydı
inline constexpr std::size_t baski_esigi = 256 * 1024;               // A3-K39: bu kadar bekleyen çıkış varsa veri eklenmez
inline constexpr std::size_t okuma_esigi = std::size_t{4} << 20u;    // çıkış bu kadar birikirse girdi okunmaz
}  // namespace sinir

namespace sure {
inline constexpr auto yayin_tiki = std::chrono::milliseconds(20);     // A3-K33 (50 Hz)
inline constexpr auto ping = std::chrono::seconds(5);                 // A3-K46
inline constexpr auto canlilik = std::chrono::seconds(15);            // A3-K46
inline constexpr auto onay_jetonu = std::chrono::seconds(10);         // A3-K94
inline constexpr auto yineleme = std::chrono::minutes(10);            // A3-K53
inline constexpr auto baslik = std::chrono::seconds(10);              // bu uygulama
inline constexpr auto govde = std::chrono::seconds(30);               // bu uygulama
inline constexpr auto bosta = std::chrono::seconds(30);               // bu uygulama (HTTP keep-alive)
inline constexpr auto kapanis = std::chrono::seconds(5);              // bu uygulama: kapanırken çıkışı okumayan karşı taraf
inline constexpr auto erisim_olayi = std::chrono::seconds(1);         // A3-K12
}  // namespace sure

class Cekirdek {
public:
    Cekirdek(const SunucuAyari& ayar, KomutAlicisi& alici, Yayinci::Ic& yayin, std::string jeton, std::uint16_t port);
    ~Cekirdek();

    // Katalogları çözer ve şemaları denetler; başarısızsa sunucu başlamaz.
    bool hazirla(std::string& hata);

    // Yeni bağlantı; sınır dolu ve yer açılamıyorsa 0 (çağıran soketi kapatır). `kapatilacak`
    // yer açmak için kapatılan boştaki bağlantıların kimlikleri.
    BagId ac(Zaman simdi, std::vector<BagId>& kapatilacak);
    void gelen(BagId id, const std::uint8_t* veri, std::size_t n, Zaman simdi);
    void karsi_kapatti(BagId id, Zaman simdi);
    std::string_view cikis(BagId id) const;
    void yazildi(BagId id, std::size_t n);
    bool okumali(BagId id) const;
    bool kapatilmali(BagId id) const;
    void kapandi(BagId id);
    std::vector<BagId> baglantilar() const;

    // Halkaları boşaltır, zaman aşımlarını ve bekleyen kısıtlayanları işler, vakti geldiyse yayın tikini çalıştırır.
    void dongu(Zaman simdi);
    // Bir sonraki yayın tikine kalan süre (yoklama zaman aşımı için).
    std::chrono::milliseconds tike_kalan(Zaman simdi) const;
    // Dış tik: açıkken `dongu` yayın tikini kendi çalıştırmaz; tik `tik()` ile verilir (sunucu
    // bunu yüksek çözünürlüklü zamanlayıcılı tik iş parçacığından alır: A3-K33).
    void dis_tik(bool acik);
    void tik();
    // A3-K49: bütün WebSocket bağlantılarına 1001; yeni isteklere 503.
    void kapaniyor(Zaman simdi);

    SunucuSayaclari sayaclar() const;
    // Sınamalar: bağlantının okumadığı (ya da okunmamış) çıkış baytı ve gövde okuma sayacı.
    std::size_t okunan_govde_bayti(BagId id) const;
    // Soketten bir seferde okunacak en çok bayt (A3-K51; ayrıştırma aşamasına bağlı); 0: okuma.
    std::size_t okuma_butcesi(BagId id) const;

private:
    struct Ic;
    std::unique_ptr<Ic> ic_;
};

}  // namespace ida::a3
