// Aşama 1, okuma: TOML 1.0 (toml++), A5-K01, K96, K97 ve tamsayı sınırı (A5-K10).
//
// Ayrıştırmadan önce iki ön denetim yapılır: UTF-8 geçerliliği (satırıyla) ve iç içe yapı
// derinliği. Derinlik denetimi bir dayanıklılık önlemidir: toml++ noktalı anahtarlarla kurulan
// çok derin tabloları özyinelemeli olarak yıkar ve yüz binlerce düzeyde yığın taşar (bu işte
// denendi); bu yüzden derinlik kaynak metinden toml++'tan önce sınanır.
//
// toml++ Clang'de kayan noktalı değeri akışla okur ve alt normal, taşan, alt taşan yazımları
// ayrıştırma hatası sayar (A5-K96 vektörleri). Böyle bir hatada yazım aynı uzunlukta geçerli
// bir yer tutucuyla değiştirilip yeniden ayrıştırılır; bütün kayan noktalı değerler sonunda
// özgün metinden `ondalik_cevir` ile çevrilir (sütunlar korunduğu için konum aynıdır).
#include "ic.hpp"

#include <toml++/toml.hpp>

#include <cmath>
#include <limits>
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
    std::int64_t gecersiz_baslik_satiri = 0;  // ilk anahtar karakteri geçersiz başlık

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
                // toml++ 3.4.0 başlıktaki ilk anahtar karakterini yalnız bir iddiayla
                // (TOML_ASSERT_ASSUME) denetler; geçersiz karakter hata ayıklamada süreci
                // durdurur, sürüm derlemesinde tanımsız davranıştır. Burada reddedilir.
                bosluk_atla();
                if (i_ < s_.size() && !anahtar_basi(s_[i_]) && s_[i_] != ']' &&
                    gecersiz_baslik_satiri == 0) {
                    gecersiz_baslik_satiri = satir_;
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
                    while (i_ < s_.size()) {
                        c = s_[i_];
                        if (c == ',' || c == ']' || c == '}' || c == '\n' || c == '#' ||
                            c == ' ' || c == '\t' || c == '\r') {
                            break;
                        }
                        ++i_;
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
        std::size_t i = bas_[satir - 1];
        for (std::uint32_t k = 1; k < sutun && i < s_.size(); ++k) {
            const auto c = static_cast<unsigned char>(s_[i]);
            i += (c < 0x80u) ? 1 : (c < 0xE0u) ? 2 : (c < 0xF0u) ? 3 : 4;
        }
        return i <= s_.size() ? i : std::string_view::npos;
    }

private:
    std::string_view s_;
    std::vector<std::size_t> bas_;
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
            kayan(n.as_floating_point()->get(), src.begin.line, src.begin.column, d, yol);
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

    void kayan(double toml_degeri, std::uint32_t satir, std::uint32_t sutun, Deger& d,
               const Yol& yol)
    {
        std::string yazim;
        const std::size_t bas = st_.bayt(satir, sutun);
        if (bas != std::string_view::npos) {
            for (std::size_t i = bas; i < ozgun_.size() && sayi_karakteri(ozgun_[i]); ++i) {
                if (ozgun_[i] != '_') {
                    yazim += ozgun_[i];
                }
            }
        }
        std::string_view govde = yazim;
        if (!govde.empty() && (govde[0] == '+' || govde[0] == '-')) {
            govde.remove_prefix(1);
        }
        if (govde == "inf" || govde == "nan") {
            d.tur = Tur::gecersiz;
            b_.hata("A5-K96", d.konum, yol, "`inf` ve `nan` kabul edilmez: " + yazim);
            return;
        }
        std::optional<double> v = ondalik_cevir(yazim);
        if (!v) {
            // Konum çözülemedi: toml++ değeri (sonlu ise) kullanılır.
            if (!std::isfinite(toml_degeri)) {
                d.tur = Tur::gecersiz;
                b_.hata("A5-K96", d.konum, yol, "sonlu olmayan sayı");
                return;
            }
            v = toml_degeri;
        }
        if (std::isinf(*v)) {
            d.tur = Tur::gecersiz;
            b_.hata("A5-K96", d.konum, yol, "sayısal taşma (binary64'te sonsuz): " + yazim);
            return;
        }
        if (*v == 0.0 && sifir_olmayan_basamak_var(yazim)) {
            d.tur = Tur::gecersiz;
            b_.hata("A5-K96", d.konum, yol, "alt taşma (sıfırdan farklı değer sıfıra yuvarlandı): " +
                                                yazim);
            return;
        }
        d.tur = Tur::sayi;
        d.sayi = (*v == 0.0) ? 0.0 : *v;  // −0 → +0
    }
};

constexpr std::string_view kKayanHatasi = "Error while parsing floating-point";

}  // namespace

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
    {
        DerinlikTarayici t(metin);
        t.tara();
        if (t.gecersiz_baslik_satiri != 0) {
            b.hata("A5-K01", Konum{dosya_no, t.gecersiz_baslik_satiri}, kok,
                   "TOML 1.0 sözdizimi hatası: tablo başlığı anahtar karakteriyle başlamıyor");
            return sonuc;
        }
        if (t.azami > kAzamiDerinlik) {
            b.hata("A5-K01", Konum{dosya_no, t.azami_satir}, kok,
                   "iç içe yapı derinliği uygulama sınırını aşıyor (" +
                       std::to_string(kAzamiDerinlik) + ")");
            return sonuc;
        }
    }

    const SatirTablosu st(metin);
    std::string calisma(metin);
    toml::table tablo;
    for (std::size_t deneme = 0;; ++deneme) {
        try {
            tablo = toml::parse(std::string_view(calisma));
            break;
        } catch (const toml::parse_error& e) {
            const auto& p = e.source().begin;
            const std::string_view aciklama = e.description();
            bool duzeltildi = false;
            if (aciklama.substr(0, kKayanHatasi.size()) == kKayanHatasi && deneme < metin.size()) {
                // Yazım hata konumundan hemen önce biter.
                const std::size_t son = st.bayt(p.line, p.column);
                if (son != std::string_view::npos && son > 0) {
                    std::size_t bas = son;
                    while (bas > 0 && sayi_karakteri(calisma[bas - 1])) {
                        --bas;
                    }
                    const std::size_t n = son - bas;
                    if (n >= 3) {
                        std::string yer_tutucu = "0.";
                        yer_tutucu.append(n - 2, '0');
                        if (calisma.compare(bas, n, yer_tutucu) != 0) {
                            calisma.replace(bas, n, yer_tutucu);
                            duzeltildi = true;
                        }
                    }
                }
            }
            if (!duzeltildi) {
                b.hata("A5-K01", Konum{dosya_no, static_cast<std::int64_t>(p.line)}, kok,
                       "TOML 1.0 sözdizimi hatası: " + std::string(aciklama));
                return sonuc;
            }
        }
    }

    Deger kokd;
    Donusturucu d(metin, st, dosya_no, b);
    Yol yol;
    d.donustur(tablo, kokd, yol, 0);
    kokd.konum = Konum{dosya_no, 0};
    sonuc.kok = std::move(kokd);
    return sonuc;
}

}  // namespace ida::senaryo::ic
