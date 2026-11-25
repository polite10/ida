// kodlayici.cpp — bkz. kodlayici.hpp.
#include "kodlayici.hpp"

#include "sha256.hpp"

#include <cstring>
#include <type_traits>

namespace ida::a2::ic {

namespace {

template <class T>
void yapi_ekle(Baytlar& c, const T& v)
{
    static_assert(std::is_trivially_copyable_v<T>);
    const std::size_t n = c.size();
    c.resize(n + sizeof(T));
    std::memcpy(c.data() + n, &v, sizeof(T));
}

template <class T>
void dizi_ekle(Baytlar& c, std::span<const T> d)
{
    static_assert(std::is_trivially_copyable_v<T>);
    if (d.empty()) {
        return;
    }
    const std::size_t n = c.size();
    c.resize(n + d.size_bytes());
    std::memcpy(c.data() + n, d.data(), d.size_bytes());
}

void baslik_ekle(Baytlar& c, IletiTuru tur, std::uint32_t sira, std::size_t yuk_bayt)
{
    Baslik b{};
    b.sihirli = kSihirliSayi;
    b.tur = static_cast<std::uint16_t>(tur);
    b.bayraklar = 0;
    b.yuk_bayt = static_cast<std::uint32_t>(yuk_bayt);
    b.sira = sira;
    yapi_ekle(c, b);
}

}  // namespace

void metin_yaz(char* alan, std::size_t boyut, std::string_view s) noexcept
{
    std::memset(alan, 0, boyut);
    if (boyut == 0) {
        return;
    }
    std::size_t n = s.size() < boyut - 1 ? s.size() : boyut - 1;
    if (n < s.size()) {
        // UTF-8 dizisinin ortasında kesme: devam baytlarını (10xxxxxx) geri al.
        while (n > 0 && (static_cast<unsigned char>(s[n]) & 0xC0u) == 0x80u) {
            --n;
        }
    }
    // Gömülü NUL metni orada bitirir (NUL'dan sonraki baytlar sıfır kalmalıdır).
    for (std::size_t i = 0; i < n; ++i) {
        if (s[i] == '\0') {
            n = i;
            break;
        }
    }
    if (n > 0) {
        std::memcpy(alan, s.data(), n);
    }
}

bool metin_nul_var(const char* alan, std::size_t boyut) noexcept { return std::memchr(alan, 0, boyut) != nullptr; }

void kodla_hosgeldin(Baytlar& c, std::uint32_t sira, const Hosgeldin& h, std::span<const GovdeBilgisi> govdeler)
{
    Hosgeldin sabit = h;
    sabit.govde_sayisi = static_cast<std::uint16_t>(govdeler.size());
    sabit.kamera_sayisi = 0;  // bu dilimde kamera yok (W5-a2-sunucu)
    sabit.kademe_kamera = static_cast<std::uint8_t>(Kademe::kapali);  // kamera_sayisi = 0 ⇔ 0 (§3.3)
    sabit.dolgu0 = 0;
    baslik_ekle(c, IletiTuru::hosgeldin, sira, sizeof sabit + govdeler.size_bytes());
    yapi_ekle(c, sabit);
    dizi_ekle(c, govdeler);
}

void kodla_ret(Baytlar& c, std::uint32_t sira, RetNedeni neden, std::string_view ayrinti)
{
    Ret r{};
    r.neden = static_cast<std::uint32_t>(neden);
    metin_yaz(r.ayrinti, sizeof r.ayrinti, ayrinti);
    baslik_ekle(c, IletiTuru::ret, sira, sizeof r);
    yapi_ekle(c, r);
}

void kodla_veda(Baytlar& c, std::uint32_t sira, VedaNedeni neden)
{
    Veda v{};
    v.neden = static_cast<std::uint32_t>(neden);
    baslik_ekle(c, IletiTuru::veda, sira, sizeof v);
    yapi_ekle(c, v);
}

void kodla_dalga(Baytlar& c, std::uint32_t sira, const DalgaKumesi& d, std::span<const DalgaBileseni> bilesenler)
{
    DalgaKumesi sabit = d;
    sabit.bilesen_sayisi = static_cast<std::uint16_t>(bilesenler.size());
    sabit.dolgu0 = 0;
    const Ozet o = sha256(bilesenler.data(), bilesenler.size_bytes());
    std::memcpy(sabit.ozet_sha256, o.data(), sizeof sabit.ozet_sha256);
    baslik_ekle(c, IletiTuru::dalga_kumesi, sira, sizeof sabit + bilesenler.size_bytes());
    yapi_ekle(c, sabit);
    dizi_ekle(c, bilesenler);
}

void kodla_ortam(Baytlar& c, std::uint32_t sira, const Ortam& o)
{
    baslik_ekle(c, IletiTuru::ortam, sira, sizeof o);
    yapi_ekle(c, o);
}

void kodla_malzeme(Baytlar& c, std::uint32_t sira, const MalzemeOptik& m, std::span<const MalzemeOptikGirdisi> girdiler)
{
    MalzemeOptik sabit = m;
    sabit.girdi_sayisi = static_cast<std::uint16_t>(girdiler.size());
    sabit.dolgu0 = 0;
    baslik_ekle(c, IletiTuru::malzeme_optik, sira, sizeof sabit + girdiler.size_bytes());
    yapi_ekle(c, sabit);
    dizi_ekle(c, girdiler);
}

void kodla_durum(Baytlar& c, std::uint32_t sira, const Durum& d, std::span<const GovdeDurumu> govdeler)
{
    Durum sabit = d;
    sabit.govde_sayisi = static_cast<std::uint16_t>(govdeler.size());
    baslik_ekle(c, IletiTuru::durum, sira, sizeof sabit + govdeler.size_bytes());
    yapi_ekle(c, sabit);
    dizi_ekle(c, govdeler);
}

void kodla_sahne_olayi(Baytlar& c, std::uint32_t sira, const SahneOlayi& o)
{
    baslik_ekle(c, IletiTuru::sahne_olayi, sira, sizeof o);
    yapi_ekle(c, o);
}

void kodla_sim_nabzi(Baytlar& c, std::uint32_t sira, const SimNabzi& n)
{
    baslik_ekle(c, IletiTuru::sim_nabzi, sira, sizeof n);
    yapi_ekle(c, n);
}

const char* gelen_hata_adi(GelenHata h) noexcept
{
    switch (h) {
        case GelenHata::yok: return "yok";
        case GelenHata::sihirli: return "sihirli sayı yanlış";
        case GelenHata::tur: return "ileti türü bilinmiyor ya da bu yönde gelmez";
        case GelenHata::bayraklar: return "başlık bayrakları sıfır değil";
        case GelenHata::uzunluk: return "yük uzunluğu beklenenden farklı";
    }
    return "?";
}

std::uint32_t gelen_yuk_boyutu(std::uint16_t tur) noexcept
{
    switch (static_cast<IletiTuru>(tur)) {
        case IletiTuru::merhaba: return sizeof(Merhaba);
        case IletiTuru::hazir: return sizeof(Hazir);
        case IletiTuru::kare_hazir: return sizeof(KareHazir);
        case IletiTuru::kare_dustu: return sizeof(KareDustu);
        case IletiTuru::goruntuleyici_nabzi: return sizeof(GoruntuleyiciNabzi);
        case IletiTuru::halka_eslendi: return sizeof(HalkaEslendi);
        case IletiTuru::veda: return sizeof(Veda);
        // sim → görselleştirici türleri bu yönde gelmez
        case IletiTuru::hosgeldin:
        case IletiTuru::ret:
        case IletiTuru::dalga_kumesi:
        case IletiTuru::ortam:
        case IletiTuru::malzeme_optik:
        case IletiTuru::durum:
        case IletiTuru::sahne_olayi:
        case IletiTuru::kamera_istegi:
        case IletiTuru::kamera_iptal:
        case IletiTuru::sim_nabzi:
        case IletiTuru::halka_yenile:
        case IletiTuru::govde_tablosu:
            break;
    }
    return 0xFFFFFFFFu;
}

void GelenOkuyucu::temizle() noexcept
{
    dolu_ = 0;
    baslik_var_ = false;
    hata_ = GelenHata::yok;
}

GelenOkuyucu::Sonuc GelenOkuyucu::besle(const std::uint8_t*& p, std::size_t& n) noexcept
{
    if (hata_ != GelenHata::yok) {
        return Sonuc::hata;
    }
    if (!baslik_var_) {
        const std::size_t eksik = kBaslikBoyutu - dolu_;
        const std::size_t al = n < eksik ? n : eksik;
        if (al > 0) {
            std::memcpy(tampon_ + dolu_, p, al);
            dolu_ += al;
            p += al;
            n -= al;
        }
        if (dolu_ < kBaslikBoyutu) {
            return Sonuc::eksik;
        }
        std::memcpy(&baslik_, tampon_, sizeof baslik_);
        // A2-K06: başlık denetimleri yük beklenmeden yapılır.
        if (baslik_.sihirli != kSihirliSayi) {
            hata_ = GelenHata::sihirli;
            return Sonuc::hata;
        }
        const std::uint32_t beklenen = gelen_yuk_boyutu(baslik_.tur);
        if (beklenen == 0xFFFFFFFFu) {
            hata_ = GelenHata::tur;
            return Sonuc::hata;
        }
        if (baslik_.bayraklar != 0) {
            hata_ = GelenHata::bayraklar;
            return Sonuc::hata;
        }
        // Gelen bütün türler sabit boyludur; beklenen boyut 1 MiB sınırının çok altındadır.
        if (baslik_.yuk_bayt != beklenen) {
            hata_ = GelenHata::uzunluk;
            return Sonuc::hata;
        }
        baslik_var_ = true;
    }
    const std::size_t tam = kBaslikBoyutu + static_cast<std::size_t>(baslik_.yuk_bayt);
    if (tam > kAzami) {  // ulaşılmaz (beklenen boyutlar ≤ sizeof(Merhaba)); savunma
        hata_ = GelenHata::uzunluk;
        return Sonuc::hata;
    }
    const std::size_t eksik = tam - dolu_;
    const std::size_t al = n < eksik ? n : eksik;
    if (al > 0) {
        std::memcpy(tampon_ + dolu_, p, al);
        dolu_ += al;
        p += al;
        n -= al;
    }
    if (dolu_ < tam) {
        return Sonuc::eksik;
    }
    // İleti tamam: bir sonraki çağrı yeni başlıkla başlar (tampon bir sonraki beslemeye kadar durur).
    dolu_ = 0;
    baslik_var_ = false;
    return Sonuc::ileti;
}

}  // namespace ida::a2::ic
