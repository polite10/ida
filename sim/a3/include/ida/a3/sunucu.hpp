// ida/a3/sunucu.hpp — sim tarafının A3 sunucusu (docs/sozlesmeler/A3-web.md; W4-a3 dilim 1).
//
// Tek TCP portunda HTTP/1.1 ve WebSocket (A3-K01), yalnız döngü arabiriminde (A3-K02). Kendi G/Ç
// iş parçacığı vardır: soketleri engellemesiz yönetir, 50 Hz yayın tikini (A3-K33) duvar saatine
// göre işler. Dünya veriyi `yayinci()` ile verir (kaynak.hpp); komutlar `KomutAlicisi`'na gider
// (komut.hpp). Sunucu hiçbir koşulda dünyayı beklemez; yavaş bir istemci ötekilerini ve veri
// kaynağını bekletmez (A3-K38, K39).
#pragma once

#include "ida/a3/kaynak.hpp"
#include "ida/a3/komut.hpp"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <string_view>

namespace ida::a3 {

struct SunucuAyari {
    // A3-K02: yalnız "127.0.0.1" ya da "::1"; başka her değer başlatmayı reddeder.
    std::string adres = "127.0.0.1";
    // A3-K01: varsayılan 47100; 0 → işletim sistemi atar (port() ile okunur).
    std::uint16_t port = 47100;
    // Panellerin derleme çıktısı (web/dist); boşsa statik yollar 404 döner.
    std::string statik_kok;
    std::string sim_surumu = "ida-a3";
    // Dünya → A3 halkaları (bayt).
    std::size_t telemetri_halkasi = std::size_t{8} << 20u;
    std::size_t olay_halkasi = std::size_t{2} << 20u;
    // Bağlantı sınırları (bellek üst sınırı için; mimari seçimi, tahmin).
    std::size_t azami_ws = 64;
    std::size_t azami_http = 64;
    // Sunucunun kendi günlüğü (stderr yerine); erişim jetonu hiçbir zaman günlüğe yazılmaz.
    std::function<void(std::string_view)> gunluk;
};

// Ölçüm ve sınama için sunucu sayaçları (G/Ç iş parçacığında güncellenir; okuma yaklaşık anlıktır).
struct SunucuSayaclari {
    std::uint64_t ws_baglanti = 0;          // açık WebSocket bağlantısı
    std::uint64_t http_baglanti = 0;        // açık HTTP bağlantısı
    std::uint64_t erisim_reddi = 0;         // A3-K12
    std::uint64_t ezilen_durum = 0;         // bütün bağlantılar (A3-K40)
    std::uint64_t dusen_dizi = 0;           // bütün bağlantılar (A3-K41)
    std::uint64_t olay_tasmasi_kapanis = 0; // 4001 ile kapanan bağlantı (A3-K42)
    std::uint64_t olay_sirasi = 0;          // son olay sırası
    std::uint64_t komut_no = 0;             // son komut numarası
    std::uint64_t bekleyen_kisitlayan = 0;  // alıcı doluyken bekletilen kısıtlayan istek
    std::uint64_t azami_cikis_bayt = 0;     // bir bağlantıda görülen en büyük bekleyen çıkış
    std::uint64_t azami_bellek_tahmini = 0; // bağlantı kuyruklarının toplam bayt tavanı (gözlenen)
    std::uint64_t tik = 0;                  // yayın tiki sayısı
    std::uint64_t azami_asim_govde_bayti = 0;  // sınırı aşan gövdede başlıktan sonra okunan en çok bayt (A3-K51)
    std::uint64_t yer_acmak_icin_kapatilan = 0;  // HTTP bağlantı sınırında yer açmak için kapatılan
    std::uint64_t baskida_one_alinan = 0;        // çıkış baskısında öne alınıp işlenen kısıtlayan istek
};

class Sunucu {
public:
    struct Ic;
    // Dinlemeye başlar ve G/Ç iş parçacığını açar. Başarısızsa nullptr ve `hata` (A3-K02: döngü
    // arabirimi dışı adreste dinlemez). Erişim jetonu burada üretilir (A3-K04); sunucu kendisi
    // standart çıktıya yazmaz: çağıran `acilis_adresi()`'ni bir kez yazar.
    static std::unique_ptr<Sunucu> baslat(const SunucuAyari& ayar, KomutAlicisi& alici, std::string& hata);
    ~Sunucu();
    Sunucu(const Sunucu&) = delete;
    Sunucu& operator=(const Sunucu&) = delete;

    // Bütün bağlantıları 1001 ile kapatır (A3-K49) ve iş parçacığını durdurur.
    void durdur();

    std::uint16_t port() const noexcept;
    // A3-K05: http://127.0.0.1:<port>/#erisim=<jeton>
    std::string acilis_adresi() const;
    // Yalnız aynı süreçteki sınamalar ve programın açılış adresi için.
    const std::string& jeton() const noexcept;

    Yayinci& yayinci() noexcept;
    SunucuSayaclari sayaclar() const;

private:
    explicit Sunucu(std::unique_ptr<Ic> ic);
    std::unique_ptr<Ic> ic_;
};

}  // namespace ida::a3
