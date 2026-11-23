// ida/a3/kaynak.hpp — veri kaynağı tarafı: dünya iş parçacığının A3'e veri verdiği tek yol.
//
// A3-K38: dünya, A3'e veriyi yalnız tek üretici–tek tüketici halkalarına yazarak verir. Bu
// sınıfın işlevleri yalnız tek bir iş parçacığından (dünya) çağrılır; engellemez, kilit almaz ve
// bellek ayırmaz. Yer yoksa kayıt atılır, sayaç artar ve işlev false döner; atılan dizi kaydından
// sonraki ilk kaydın çerçevesinde `oncesinde_dusme` biti kurulur (A3-K28), atılan olay sunucuda
// `olay_kaybi` olayıyla bildirilir (A3-K44).
#pragma once

#include "ida/a3/yukler.hpp"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string_view>

namespace ida::a3 {

// Asenkron bir komutun dünyadaki nihai sonucu (A3-K57). Sunucu bunu `komut_sonucu` olayına
// çevirir ve `GET /api/v1/komutlar/{komut_no}` kaydını günceller. Sonuç aynen taşınır.
struct KomutSonucuGirdisi {
    std::uint32_t komut_no = 0;
    bool uygulandi = false;
    bool uygulama_adimi_var = false;
    std::uint64_t uygulama_adimi = 0;
    std::int64_t t_ns = 0;               // sonucun ait olduğu adım sınırının sim zamanı
    std::uint64_t adim_no = 0;           // olayın adim_no alanı
    std::string_view hata_kodu;          // reddedildiyse A3 §3.9 kodu (ör. kisitlama_hakim)
    std::string_view sonuc_ayrinti_json; // boş ya da `$defs/sinirli_nesne`'ye uyan JSON nesnesi
};

class Yayinci {
public:
    struct Ic;
    explicit Yayinci(std::size_t telemetri_bayt, std::size_t olay_bayt);
    ~Yayinci();
    Yayinci(const Yayinci&) = delete;
    Yayinci& operator=(const Yayinci&) = delete;

    // Durum ve dizi sınıfı kanalların yayını. `t_ns` kanalın zaman anlamına göredir (A3-K26).
    template <Kanal K>
    bool yayinla(std::int64_t t_ns, const typename KanalYuku<K>::tip& y) noexcept
    {
        static_assert(!KanalYuku<K>::olay, "olay kanalı Yayinci::olay ile yayınlanır");
        using T = typename KanalYuku<K>::tip;
        return yaz_(KanalYuku<K>::indeks, KanalYuku<K>::anahtar(y), t_ns, boyut(y),
                    [](const void* p, std::uint8_t* h, std::size_t k, std::size_t& n) noexcept {
                        return kodla(*static_cast<const T*>(p), h, k, n);
                    },
                    &y);
    }

    // `olay` kanalı (A3 §3.5): `sira` sunucuda verilir (A3-K27, K44). `ayrinti` olay türünün
    // `$defs/olay_<ad>` şemasına uyan, A3-K121 politikasıyla yazılmış JSON nesnesidir.
    bool olay(std::int64_t t_ns, const yuk_olay& o) noexcept;

    bool komut_sonucu(const KomutSonucuGirdisi& s) noexcept;

    // `sayac/gercek_zaman.a3_halka_tasma` (A3-K38) için: iki halkada düşen kayıt sayısı.
    std::uint64_t halka_tasma() const noexcept;
    std::uint64_t kodlama_hatasi() const noexcept;

    Ic& ic() noexcept { return *ic_; }

private:
    using KodlaIslevi = bool (*)(const void*, std::uint8_t*, std::size_t, std::size_t&) noexcept;
    bool yaz_(std::uint16_t kanal_indeks, std::uint64_t anahtar, std::int64_t t_ns, std::size_t boy,
              KodlaIslevi kodlayici, const void* yuk) noexcept;

    std::unique_ptr<Ic> ic_;
};

}  // namespace ida::a3
