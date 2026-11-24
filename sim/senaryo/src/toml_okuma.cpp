// Aşama 1, okuma: TOML 1.0 (toml++), A5-K01, K96, K97 ve tamsayı sınırı (A5-K10).
//
// Güvenlik sınırı kitaplığın kendisidir: toml++ depo içi vcpkg overlay port'uyla gelir
// (vcpkg-overlay/tomlplusplus: 3.4.0 + üst akımda birleşmiş PR #242, #301, #305, #321). Bu
// yamalar bozuk girdinin TOML_UNREACHABLE / TOML_ASSERT_ASSUME yollarına (tanımsız davranış)
// ulaşmasını ayrıştırma hatasına çevirir; hata A5-K01 olur. Aşağıdaki ön tarama bir güvenlik
// sınırı değildir (W4-mac-ci kararı): yalnız (1) UTF-8 geçerliliğini satırıyla bildirir,
// (2) iç içe yapı derinliğine erken ve okunaklı bir üst sınır koyar (kitaplık ayrıca noktalı
// anahtarları 1024, iç içe değerleri 256 ile sınırlar) ve (3) değer konumundaki kayan noktalı
// yazımları bulur.
//
// toml++ Clang'de kayan noktalı değeri akışla okur ve alt normal, taşan, alt taşan yazımları
// ayrıştırma hatası sayar (A5-K96 vektörleri). Bu yazımlar aynı uzunlukta geçerli bir yer
// tutucuyla değiştirilip tek geçişte ayrıştırılır; bütün kayan noktalı değerler özgün metinden
// `ondalik_cevir` ile çevrilir. Yer tutucunun her konumu bir kayan noktalı düğüme denk
// gelmelidir; gelmezse dosya A5-K01 ile reddedilir (ön tarama yanılsa da yanlış değer üretilmez).
#include "ic.hpp"

#include <toml++/toml.hpp>

#include <algorithm>
#include <cmath>
#include <limits>
#include <set>
#include <string>
#include <vector>

namespace ida::senaryo::ic {

namespace {

constexpr std::int64_t kTamsayiSiniri = 9007199254740991;  // 2^53 − 1 (A5-K10)

// İlk geçersiz UTF-8 baytının satırı; geçerliyse 0.
std::int64_t utf8_hata_satiri(std::string_view s)
{
    std::int64_t satir = 1;
    std::size_t i = 0;
    while (i < s.size()) {
        const auto c = static_cast<unsigned char>(s[i]);
        if (c == '\n') {
            ++satir;
        }
        if (c < 0x80u) {
            ++i;
            continue;
        }
        std::size_t n = 0;
        char32_t cp = 0;
        char32_t alt = 0;
        if (c >= 0xC2u && c <= 0xDFu) {
            n = 2;
            cp = c & 0x1Fu;
            alt = 0x80u;
        } else if ((c & 0xF0u) == 0xE0u) {
            n = 3;
            cp = c & 0x0Fu;
            alt = 0x800u;
        } else if (c >= 0xF0u && c <= 0xF4u) {
            n = 4;
            cp = c & 0x07u;
            alt = 0x10000u;
        } else {
            return satir;
        }
        if (i + n > s.size()) {
            return satir;
        }
        for (std::size_t j = 1; j < n; ++j) {
            const auto d = static_cast<unsigned char>(s[i + j]);
            if ((d & 0xC0u) != 0x80u) {
                return satir;
            }
            cp = (cp << 6) | (d & 0x3Fu);
        }
        if (cp < alt || cp > 0x10FFFFu || (cp >= 0xD800u && cp <= 0xDFFFu)) {
            return satir;
        }
        i += n;
    }
    return 0;
}

// ------------------------------------------------------------------ derinlik ön taraması
// Geçerli TOML için ortaya çıkacak ağacın derinliğinin üst sınırını hesaplar (başlık ve
// anahtar parçaları + dizi ve satır içi tablo düzeyleri). Geçersiz metinde sonuç anlamsız
// olabilir ama toml++ o metni zaten reddeder.
class DerinlikTarayici {
public:
    explicit DerinlikTarayici(std::string_view s) : s_(s) {}

    // En büyük derinlik ve ilk aşıldığı satır.
    std::size_t azami = 0;
    std::int64_t azami_satir = 1;
    // Değer konumundaki dizgi olmayan belirteçler [baş, son) (sayı kurtarma için).
    std::vector<std::pair<std::size_t, std::size_t>> skalerler;

    void tara()
    {
        std::size_t taban = 0;
        while (i_ < s_.size()) {
            bosluk_ve_yorum_atla(true);
            if (i_ >= s_.size()) {
                break;
            }
            const char c = s_[i_];
            if (c == '[') {
                ++i_;
                bool dizi_tablo = false;
                if (i_ < s_.size() && s_[i_] == '[') {
                    ++i_;
                    dizi_tablo = true;
                }
                taban = anahtar_oku() + (dizi_tablo ? 1u : 0u);
                kaydet(taban);
                satir_sonuna_git();
            } else if (anahtar_basi(c)) {
                const std::size_t d = taban + anahtar_oku();
                kaydet(d);
                bosluk_atla();
                if (i_ < s_.size() && s_[i_] == '=') {
                    ++i_;
                    deger_tara(d);
                }
                satir_sonuna_git();
            } else {
                ++i_;
            }
        }
    }

private:
    std::string_view s_;
    std::size_t i_ = 0;
    std::int64_t satir_ = 1;

    void kaydet(std::size_t d)
    {
        if (d > azami) {
            azami = d;
            azami_satir = satir_;
        }
    }
    static bool ciplak(char c)
    {
        return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ||
               c == '_' || c == '-';
    }
    static bool anahtar_basi(char c) { return ciplak(c) || c == '"' || c == '\''; }
    void bosluk_atla()
    {
        while (i_ < s_.size() && (s_[i_] == ' ' || s_[i_] == '\t')) {
            ++i_;
        }
    }
    void bosluk_ve_yorum_atla(bool satir_sonlari)
    {
        while (i_ < s_.size()) {
            const char c = s_[i_];
            if (c == ' ' || c == '\t' || c == '\r') {
                ++i_;
            } else if (c == '\n' && satir_sonlari) {
                ++satir_;
                ++i_;
            } else if (c == '#') {
                while (i_ < s_.size() && s_[i_] != '\n') {
                    ++i_;
                }
            } else {
                break;
            }
        }
    }
    void satir_sonuna_git()
    {
        while (i_ < s_.size() && s_[i_] != '\n') {
            if (s_[i_] == '#') {
                // Yorum satır sonunda biter; içindeki tırnaklar dizgi başlatmaz (TOML 1.0).
                while (i_ < s_.size() && s_[i_] != '\n') {
                    ++i_;
                }
                return;
            }
            if (s_[i_] == '"' || s_[i_] == '\'') {
                dizgi_atla();
            } else {
                ++i_;
            }
        }
    }
    // Dizgi (dört biçim); i_ açılış tırnağında.
    void dizgi_atla()
    {
        const char q = s_[i_];
        const bool cok_satirli = s_.substr(i_, 3) == std::string_view(q == '"' ? "\"\"\"" : "'''");
        if (cok_satirli) {
            i_ += 3;
            while (i_ < s_.size()) {
                if (s_[i_] == '\n') {
                    ++satir_;
                }
                if (q == '"' && s_[i_] == '\\') {
                    i_ += 2;
                    continue;
                }
                if (s_.substr(i_, 3) == std::string_view(q == '"' ? "\"\"\"" : "'''")) {
                    i_ += 3;
                    while (i_ < s_.size() && s_[i_] == q) {
                        ++i_;
                    }
                    return;
                }
                ++i_;
            }
            return;
        }
        ++i_;
        while (i_ < s_.size() && s_[i_] != '\n') {
            if (q == '"' && s_[i_] == '\\') {
                i_ += 2;
                continue;
            }
            if (s_[i_] == q) {
                ++i_;
                return;
            }
            ++i_;
        }
    }
    // Noktalı anahtar; parça sayısını döndürür (her nokta bir parça sayılır).
    std::size_t anahtar_oku()
    {
        std::size_t parca = 0;
        for (;;) {
            bosluk_atla();
            if (i_ >= s_.size()) {
                break;
            }
            const char c = s_[i_];
            if (c == '"' || c == '\'') {
                dizgi_atla();
                ++parca;
            } else if (ciplak(c)) {
                while (i_ < s_.size() && ciplak(s_[i_])) {
                    ++i_;
                }
                ++parca;
            } else if (c == '.') {
                ++i_;
                ++parca;  // boş parça da sayılır (temkinli üst sınır)
                continue;
            } else {
                break;
            }
            bosluk_atla();
            if (i_ < s_.size() && s_[i_] == '.') {
                ++i_;
                continue;
            }
            break;
        }
        return parca == 0 ? 1 : parca;
    }

    struct Kap {
        bool tablo;          // satır içi tablo; değilse dizi
        std::size_t derinlik;
    };

    void deger_tara(std::size_t d0)
    {
        std::vector<Kap> yigin;
        bool anahtar_bekle = false;
        std::size_t d = d0;
        for (std::size_t koruma = 0; koruma < s_.size() * 4 + 16; ++koruma) {
            const bool dizide = !yigin.empty() && !yigin.back().tablo;
            bosluk_ve_yorum_atla(dizide);
            if (i_ >= s_.size()) {
                return;
            }
            char c = s_[i_];
            if (anahtar_bekle) {
                if (c == '}') {
                    ++i_;
                    yigin.pop_back();
                } else if (anahtar_basi(c)) {
                    d = yigin.back().derinlik + anahtar_oku();
                    kaydet(d);
                    bosluk_atla();
                    if (i_ < s_.size() && s_[i_] == '=') {
                        ++i_;
                    } else {
                        return;
                    }
                    anahtar_bekle = false;
                    continue;
                } else {
                    return;
                }
            } else {
                if (c == '"' || c == '\'') {
                    dizgi_atla();
                } else if (c == '[') {
                    ++i_;
                    const std::size_t yeni = (yigin.empty() ? d : yigin.back().derinlik) + 1;
                    yigin.push_back({false, yeni});
                    kaydet(yeni);
                    continue;
                } else if (c == '{') {
                    ++i_;
                    const std::size_t taban = (yigin.empty() || yigin.back().tablo)
                                                  ? d
                                                  : yigin.back().derinlik;
                    yigin.push_back({true, taban + 1});
                    kaydet(taban + 1);
                    anahtar_bekle = true;
                    continue;
                } else if (c == ']' && dizide) {
                    // boş dizi ya da sondaki virgülden sonra kapanış
                } else {
                    const std::size_t bas = i_;
                    while (i_ < s_.size()) {
                        c = s_[i_];
                        if (c == ',' || c == ']' || c == '}' || c == '\n' || c == '#' ||
                            c == ' ' || c == '\t' || c == '\r') {
                            break;
                        }
                        ++i_;
                    }
                    if (i_ > bas) {
                        skalerler.emplace_back(bas, i_);
                    }
                }
            }
            // değerden sonra
            for (;;) {
                const bool dizide2 = !yigin.empty() && !yigin.back().tablo;
                bosluk_ve_yorum_atla(dizide2);
                if (yigin.empty() || i_ >= s_.size()) {
                    return;
                }
                c = s_[i_];
                if (c == ',') {
                    ++i_;
                    anahtar_bekle = yigin.back().tablo;
                    if (anahtar_bekle) {
                        d = yigin.back().derinlik;
                    }
                    break;
                }
                if (c == ']' && !yigin.back().tablo) {
                    ++i_;
                    yigin.pop_back();
                    continue;
                }
                if (c == '}' && yigin.back().tablo) {
                    ++i_;
                    yigin.pop_back();
                    continue;
                }
                if (!yigin.back().tablo) {
                    break;  // dizide virgülsüz yeni değer (geçersiz ama taramayı sürdür)
                }
                return;
            }
        }
    }
};

// ------------------------------------------------------------------ satır/sütun → bayt
class SatirTablosu {
public:
    explicit SatirTablosu(std::string_view s) : s_(s)
    {
        bas_.push_back(0);
        for (std::size_t i = 0; i < s.size(); ++i) {
            if (s[i] == '\n') {
                bas_.push_back(i + 1);
            }
        }
        if (s.size() >= 3 && static_cast<unsigned char>(s[0]) == 0xEFu &&
            static_cast<unsigned char>(s[1]) == 0xBBu && static_cast<unsigned char>(s[2]) == 0xBFu) {
            bas_[0] = 3;  // BOM sütun sayılmaz
        }
    }
    // 1 tabanlı (satır, kod noktası sütunu) → bayt konumu; yoksa npos.
    std::size_t bayt(std::uint32_t satir, std::uint32_t sutun) const
    {
        if (satir == 0 || satir > bas_.size() || sutun == 0) {
            return std::string_view::npos;
        }
        // Düğümler satır içinde artan sütunla gezilir: aynı satırda son konumdan sürdürülür
        // (tek satırlık uzun dizilerde toplam iş doğrusal kalır; sol #11).
        std::size_t i = bas_[satir - 1];
        std::uint32_t k = 1;
        if (onbellek_satir_ == satir && onbellek_sutun_ <= sutun) {
            i = onbellek_bayt_;
            k = onbellek_sutun_;
        }
        for (; k < sutun && i < s_.size(); ++k) {
            const auto c = static_cast<unsigned char>(s_[i]);
            i += (c < 0x80u) ? 1 : (c < 0xE0u) ? 2 : (c < 0xF0u) ? 3 : 4;
        }
        onbellek_satir_ = satir;
        onbellek_sutun_ = k;
        onbellek_bayt_ = i;
        return i <= s_.size() ? i : std::string_view::npos;
    }

private:
    std::string_view s_;
    std::vector<std::size_t> bas_;
    mutable std::uint32_t onbellek_satir_ = 0;
    mutable std::uint32_t onbellek_sutun_ = 1;
    mutable std::size_t onbellek_bayt_ = 0;
};

bool sayi_karakteri(char c)
{
    return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
           c == '_' || c == '+' || c == '-' || c == '.';
}

// ------------------------------------------------------------------ dönüştürme
class Donusturucu {
public:
    Donusturucu(std::string_view ozgun, const SatirTablosu& st, int dosya_no, Bulgucu& b)
        : ozgun_(ozgun), st_(st), dosya_(dosya_no), b_(b)
    {
    }

    void donustur(const toml::node& n, Deger& d, Yol& yol, std::size_t derinlik)
    {
        const auto& src = n.source();
        d.konum = Konum{dosya_, static_cast<std::int64_t>(src.begin.line)};
        if (derinlik > kAzamiDerinlik) {
            d.tur = Tur::gecersiz;
            b_.hata("A5-K01", d.konum, yol, "iç içe yapı derinliği uygulama sınırını aşıyor");
            return;
        }
        switch (n.type()) {
        case toml::node_type::table: {
            d.tur = Tur::tablo;
            for (auto&& [k, v] : *n.as_table()) {
                std::string ad(k.str());
                Deger& alt = d.tablo[ad];
                yol.parcalar.push_back(YolParcasi{ad});
                donustur(v, alt, yol, derinlik + 1);
                yol.parcalar.pop_back();
            }
            return;
        }
        case toml::node_type::array: {
            d.tur = Tur::dizi;
            const auto& a = *n.as_array();
            d.dizi.resize(a.size());
            for (std::size_t i = 0; i < a.size(); ++i) {
                yol.parcalar.push_back(YolParcasi{i});
                donustur(a[i], d.dizi[i], yol, derinlik + 1);
                yol.parcalar.pop_back();
            }
            return;
        }
        case toml::node_type::string:
            d.tur = Tur::dizgi;
            d.dizgi = n.as_string()->get();
            return;
        case toml::node_type::boolean:
            d.tur = Tur::mantiksal;
            d.mantik = n.as_boolean()->get();
            return;
        case toml::node_type::integer: {
            const std::int64_t v = n.as_integer()->get();
            if (v > kTamsayiSiniri || v < -kTamsayiSiniri) {
                d.tur = Tur::gecersiz;
                b_.hata("A5-K10", d.konum, yol,
                        "tamsayı [−(2^53−1), 2^53−1] aralığının dışında: " + std::to_string(v));
                return;
            }
            d.tur = Tur::tamsayi;
            d.tam = v;
            return;
        }
        case toml::node_type::floating_point:
            kayan(src.begin.line, src.begin.column, d, yol);
            return;
        case toml::node_type::date:
        case toml::node_type::time:
        case toml::node_type::date_time:
            d.tur = Tur::gecersiz;
            b_.hata("A5-K97", d.konum, yol, "TOML tarih ve saat türleri kabul edilmez");
            return;
        case toml::node_type::none:
            break;
        }
        d.tur = Tur::gecersiz;
        b_.hata("A5-K01", d.konum, yol, "tanınmayan TOML değeri");
    }

private:
    std::string_view ozgun_;
    const SatirTablosu& st_;
    int dosya_;
    Bulgucu& b_;

    void kayan(std::uint32_t satir, std::uint32_t sutun, Deger& d, const Yol& yol)
    {
        std::string ham;
        const std::size_t bas = st_.bayt(satir, sutun);
        if (bas != std::string_view::npos) {
            for (std::size_t i = bas; i < ozgun_.size() && sayi_karakteri(ozgun_[i]); ++i) {
                ham += ozgun_[i];
            }
            kayan_konumlari.insert(bas);
        }
        std::string_view govde = ham;
        if (!govde.empty() && (govde[0] == '+' || govde[0] == '-')) {
            govde.remove_prefix(1);
        }
        if (govde == "inf" || govde == "nan") {
            d.tur = Tur::gecersiz;
            b_.hata("A5-K96", d.konum, yol, "`inf` ve `nan` kabul edilmez: " + ham);
            return;
        }
        // Değer daima özgün yazımdan çevrilir; yazım TOML 1.0 kayan noktalı sayı gramerine
        // uymuyorsa ya da çevrilemiyorsa toml++'ın (ya da yer tutucunun) değerine dönülmez.
        const std::optional<double> v = toml_kayan_gramer(ham) ? ondalik_cevir(alt_cizgisiz(ham)) : std::nullopt;
        if (!v) {
            d.tur = Tur::gecersiz;
            b_.hata("A5-K01", d.konum, yol, "TOML 1.0 kayan noktalı sayı yazımı geçersiz: " + ham);
            return;
        }
        if (std::isinf(*v)) {
            d.tur = Tur::gecersiz;
            b_.hata("A5-K96", d.konum, yol, "sayısal taşma (binary64'te sonsuz): " + ham);
            return;
        }
        if (*v == 0.0 && sifir_olmayan_basamak_var(ham)) {
            d.tur = Tur::gecersiz;
            b_.hata("A5-K96", d.konum, yol, "alt taşma (sıfırdan farklı değer sıfıra yuvarlandı): " + ham);
            return;
        }
        d.tur = Tur::sayi;
        d.sayi = (*v == 0.0) ? 0.0 : *v;  // −0 → +0
    }

public:
    std::set<std::size_t> kayan_konumlari;  // dönüştürülen kayan noktalı değerlerin bayt konumu
};

}  // namespace

std::string alt_cizgisiz(std::string_view s)
{
    std::string o;
    for (const char c : s) {
        if (c != '_') {
            o += c;
        }
    }
    return o;
}

bool toml_kayan_gramer(std::string_view s)
{
    std::size_t i = 0;
    const std::size_t n = s.size();
    auto rakam = [&](std::size_t j) { return j < n && s[j] >= '0' && s[j] <= '9'; };
    // RAKAM *( RAKAM / "_" RAKAM ); en az bir rakam
    auto rakamlar = [&]() {
        if (!rakam(i)) {
            return false;
        }
        ++i;
        while (i < n) {
            if (rakam(i)) {
                ++i;
            } else if (s[i] == '_' && rakam(i + 1)) {
                i += 2;
            } else {
                break;
            }
        }
        return true;
    };
    if (i < n && (s[i] == '+' || s[i] == '-')) {
        ++i;
    }
    if (!rakam(i)) {
        return false;
    }
    if (s[i] == '0') {
        ++i;  // dec-int "0": ardından rakam ya da alt çizgi gelemez
        if (i < n && (rakam(i) || s[i] == '_')) {
            return false;
        }
    } else if (!rakamlar()) {
        return false;
    }
    bool kesir = false;
    bool us = false;
    if (i < n && s[i] == '.') {
        ++i;
        if (!rakamlar()) {
            return false;
        }
        kesir = true;
    }
    if (i < n && (s[i] == 'e' || s[i] == 'E')) {
        ++i;
        if (i < n && (s[i] == '+' || s[i] == '-')) {
            ++i;
        }
        if (!rakamlar()) {
            return false;
        }
        us = true;
    }
    return i == n && (kesir || us);
}

TomlOkuma toml_oku(std::string_view metin, int dosya_no, Bulgucu& b)
{
    TomlOkuma sonuc;
    const Yol kok;
    if (metin.size() > kAzamiDosyaBoyu) {
        b.hata("A5-K01", Konum{dosya_no, 1}, kok,
               "dosya boyu uygulama sınırını aşıyor (" + std::to_string(kAzamiDosyaBoyu) + " bayt)");
        return sonuc;
    }
    if (const std::int64_t s = utf8_hata_satiri(metin); s != 0) {
        b.hata("A5-K01", Konum{dosya_no, s}, kok, "geçerli UTF-8 değil");
        return sonuc;
    }
    // Tek geçişte: ön tarama derinliği ve değer konumundaki belirteçleri verir. toml++'ın
    // okuyamadığı (alt normal, taşan, alt taşan) dilbilgisel olarak geçerli kayan noktalı
    // yazımlar aynı uzunlukta bir yer tutucuyla topluca değiştirilir; değerleri dönüştürmede
    // özgün metinden çevrilir. Yer tutucunun her konumu bir kayan noktalı düğüme denk
    // gelmelidir; gelmezse (ön tarama ile ayrıştırıcı arasında uyuşmazlık) dosya reddedilir.
    std::string calisma(metin);
    std::vector<std::size_t> yer_tutucular;
    {
        DerinlikTarayici t(metin);
        t.tara();
        if (t.azami > kAzamiDerinlik) {
            b.hata("A5-K01", Konum{dosya_no, t.azami_satir}, kok,
                   "iç içe yapı derinliği uygulama sınırını aşıyor (" +
                       std::to_string(kAzamiDerinlik) + ")");
            return sonuc;
        }
        for (const auto& [bas, son] : t.skalerler) {
            const std::string_view y = metin.substr(bas, son - bas);
            if (y.size() < 3 || !toml_kayan_gramer(y)) {
                continue;
            }
            const std::optional<double> v = ondalik_cevir(alt_cizgisiz(y));
            const bool sorunlu = v && (std::isinf(*v) || (*v == 0.0 && sifir_olmayan_basamak_var(y)) ||
                                       (*v != 0.0 && std::abs(*v) < std::numeric_limits<double>::min()));
            if (sorunlu) {
                std::string yer = "0.";
                yer.append(y.size() - 2, '0');
                std::copy(yer.begin(), yer.end(), calisma.begin() + static_cast<std::ptrdiff_t>(bas));
                yer_tutucular.push_back(bas);
            }
        }
    }

    const SatirTablosu st(metin);
    toml::table tablo;
    try {
        tablo = toml::parse(std::string_view(calisma));
    } catch (const toml::parse_error& e) {
        b.hata("A5-K01", Konum{dosya_no, static_cast<std::int64_t>(e.source().begin.line)}, kok,
               "TOML 1.0 sözdizimi hatası: " + std::string(e.description()));
        return sonuc;
    }

    Deger kokd;
    Donusturucu d(metin, st, dosya_no, b);
    Yol yol;
    d.donustur(tablo, kokd, yol, 0);
    kokd.konum = Konum{dosya_no, 0};
    for (const std::size_t k : yer_tutucular) {
        if (d.kayan_konumlari.count(k) == 0) {
            b.hata("A5-K01", Konum{dosya_no, 0}, kok,
                   "sayı yazımı bir kayan noktalı değere eşlenemedi (ön tarama uyuşmazlığı)");
            return TomlOkuma{};
        }
    }
    sonuc.kok = std::move(kokd);
    return sonuc;
}

}  // namespace ida::senaryo::ic
