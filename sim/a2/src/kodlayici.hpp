// kodlayici.hpp — sim tarafının A2 tel kodlaması (W5-K29: `a2_wire.h` yapılarından yazılır;
// `viz/` altındaki kodeğe bağlanılmaz).
//
// Bayt düzeni küçük sonludur ve `a2_wire.h`'deki yapılarla birebirdir (boyut ve ofsetler orada
// static_assert ile sabit; ana makinenin küçük sonlu olduğu da orada denetlenir). Yapılar sıfırla
// ilklendirilip alan alan doldurulduğu için dolgu ve ayrılmış alanlar sıfırdır (§3).
//
// Sim → görselleştirici: hosgeldin, ret, dalga_kumesi, ortam, malzeme_optik, durum, sahne_olayi,
// sim_nabzi, veda. Görselleştirici → sim: sabit boylu başlık + yük okuyucusu (A2-K06).
#pragma once

#include "ida/a2_wire.h"

#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

namespace ida::a2::ic {

using Baytlar = std::vector<std::uint8_t>;

// ---- Kodlayıcılar: tam iletiyi (başlık + yük) `c`'nin sonuna ekler ----
void kodla_hosgeldin(Baytlar& c, std::uint32_t sira, const Hosgeldin& h, std::span<const GovdeBilgisi> govdeler);
void kodla_ret(Baytlar& c, std::uint32_t sira, RetNedeni neden, std::string_view ayrinti);
void kodla_veda(Baytlar& c, std::uint32_t sira, VedaNedeni neden);
// `ozet_sha256` burada hesaplanır (§3.4: bileşen dizisinin baytları üzerinden, FIPS 180-4).
void kodla_dalga(Baytlar& c, std::uint32_t sira, const DalgaKumesi& d, std::span<const DalgaBileseni> bilesenler);
void kodla_ortam(Baytlar& c, std::uint32_t sira, const Ortam& o);
void kodla_malzeme(Baytlar& c, std::uint32_t sira, const MalzemeOptik& m, std::span<const MalzemeOptikGirdisi> girdiler);
void kodla_durum(Baytlar& c, std::uint32_t sira, const Durum& d, std::span<const GovdeDurumu> govdeler);
void kodla_sahne_olayi(Baytlar& c, std::uint32_t sira, const SahneOlayi& o);
void kodla_sim_nabzi(Baytlar& c, std::uint32_t sira, const SimNabzi& n);

// Metin alanı: UTF-8, NUL ile biter, NUL'dan sonraki baytlar sıfır. Sığmayan metin son bayt NUL
// kalacak biçimde, UTF-8 dizisi bölünmeden kesilir.
void metin_yaz(char* alan, std::size_t boyut, std::string_view s) noexcept;
bool metin_nul_var(const char* alan, std::size_t boyut) noexcept;

// ---- Görselleştirici → sim okuyucusu ----
enum class GelenHata : std::uint8_t {
    yok = 0,
    sihirli,     // sihirli sayı yanlış
    tur,         // tür bilinmiyor ya da bu yönde gelmez
    bayraklar,   // başlık bayrakları ≠ 0
    uzunluk,     // yuk_bayt türün beklenen boyutundan farklı ya da > 1 MiB
};
const char* gelen_hata_adi(GelenHata h) noexcept;

// Görselleştiriciden gelebilen türün beklenen yük boyutu; tür bu yönde gelmezse 0xFFFFFFFF.
std::uint32_t gelen_yuk_boyutu(std::uint16_t tur) noexcept;

// En büyük gelen ileti 16 + 144 bayttır (`merhaba`); tampon sabit boyludur. Başlık denetimleri
// (A2-K06) yük beklenmeden yapılır.
class GelenOkuyucu {
public:
    enum class Sonuc : std::uint8_t { eksik, ileti, hata };
    // `p`/`n`'den tüketir (ikisini ilerletir). `ileti`: `baslik()` ve `yuk()` geçerlidir (bir
    // sonraki çağrıya kadar). `hata`: `hata()` nedeni verir; okuyucu bundan sonra kullanılmaz.
    Sonuc besle(const std::uint8_t*& p, std::size_t& n) noexcept;
    const Baslik& baslik() const noexcept { return baslik_; }
    const std::uint8_t* yuk() const noexcept { return tampon_ + kBaslikBoyutu; }
    GelenHata hata() const noexcept { return hata_; }
    void temizle() noexcept;

private:
    static constexpr std::size_t kAzami = kBaslikBoyutu + sizeof(Merhaba);
    std::uint8_t tampon_[kAzami] = {};
    std::size_t dolu_ = 0;
    bool baslik_var_ = false;
    Baslik baslik_{};
    GelenHata hata_ = GelenHata::yok;
};

}  // namespace ida::a2::ic
