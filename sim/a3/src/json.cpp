// json.cpp — bkz. json.hpp.
#include "json.hpp"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <locale>
#include <sstream>

namespace ida::a3::json {

const Deger* Deger::bul(std::string_view anahtar) const noexcept
{
    if (tur != Tur::nesne) {
        return nullptr;
    }
    for (const auto& [k, v] : nesne) {
        if (k == anahtar) {
            return &v;
        }
    }
    return nullptr;
}

bool Deger::tamsayi_mi() const noexcept
{
    return tur == Tur::sayi && std::isfinite(sayi) && std::floor(sayi) == sayi;
}

namespace {

// Tek bir UTF-8 dizisini çözer; geçersizse false. `i` dizinin sonrasına ilerler.
bool utf8_cek(std::string_view s, std::size_t& i, std::uint32_t& kp) noexcept
{
    const auto b0 = static_cast<unsigned char>(s[i]);
    if (b0 < 0x80u) {
        kp = b0;
        ++i;
        return true;
    }
    std::size_t n = 0;
    std::uint32_t en_az = 0;
    if ((b0 & 0xE0u) == 0xC0u) {
        n = 1;
        kp = b0 & 0x1Fu;
        en_az = 0x80u;
    } else if ((b0 & 0xF0u) == 0xE0u) {
        n = 2;
        kp = b0 & 0x0Fu;
        en_az = 0x800u;
    } else if ((b0 & 0xF8u) == 0xF0u) {
        n = 3;
        kp = b0 & 0x07u;
        en_az = 0x10000u;
    } else {
        return false;
    }
    if (n > s.size() - i - 1) {
        return false;
    }
    for (std::size_t k = 1; k <= n; ++k) {
        const auto b = static_cast<unsigned char>(s[i + k]);
        if ((b & 0xC0u) != 0x80u) {
            return false;
        }
        kp = (kp << 6u) | (b & 0x3Fu);
    }
    if (kp < en_az || kp > 0x10FFFFu || (kp >= 0xD800u && kp <= 0xDFFFu)) {
        return false;
    }
    i += n + 1;
    return true;
}

void utf8_ekle(std::uint32_t kp, std::string& c)
{
    if (kp < 0x80u) {
        c.push_back(static_cast<char>(kp));
    } else if (kp < 0x800u) {
        c.push_back(static_cast<char>(0xC0u | (kp >> 6u)));
        c.push_back(static_cast<char>(0x80u | (kp & 0x3Fu)));
    } else if (kp < 0x10000u) {
        c.push_back(static_cast<char>(0xE0u | (kp >> 12u)));
        c.push_back(static_cast<char>(0x80u | ((kp >> 6u) & 0x3Fu)));
        c.push_back(static_cast<char>(0x80u | (kp & 0x3Fu)));
    } else {
        c.push_back(static_cast<char>(0xF0u | (kp >> 18u)));
        c.push_back(static_cast<char>(0x80u | ((kp >> 12u) & 0x3Fu)));
        c.push_back(static_cast<char>(0x80u | ((kp >> 6u) & 0x3Fu)));
        c.push_back(static_cast<char>(0x80u | (kp & 0x3Fu)));
    }
}

// Nesne başına üye sınırı: yinelenen anahtar denetimi karesel olduğundan sınırlıdır (A3 ve A1
// komut şemalarındaki nesneler bu sınırın çok altındadır).
constexpr std::size_t kAzamiUye = 1024;

class Cozucu {
public:
    Cozucu(std::string_view s, const CozumSiniri& sinir) : s_(s), sinir_(sinir) {}

    bool calis(Deger& d, std::string* hata)
    {
        bosluk_();
        if (!deger_(d, 0)) {
            if (hata != nullptr) {
                *hata = hata_ + " (konum " + std::to_string(i_) + ")";
            }
            return false;
        }
        bosluk_();
        if (i_ != s_.size()) {
            if (hata != nullptr) {
                *hata = "değerden sonra fazla karakter (konum " + std::to_string(i_) + ")";
            }
            return false;
        }
        return true;
    }

private:
    bool hata_ver_(const char* m)
    {
        hata_ = m;
        return false;
    }

    void bosluk_() noexcept
    {
        while (i_ < s_.size() && (s_[i_] == ' ' || s_[i_] == '\t' || s_[i_] == '\n' || s_[i_] == '\r')) {
            ++i_;
        }
    }

    bool sabit_(std::string_view k)
    {
        if (s_.substr(i_, k.size()) != k) {
            return hata_ver_("geçersiz değer");
        }
        i_ += k.size();
        return true;
    }

    bool deger_(Deger& d, std::size_t derinlik)
    {
        if (++sayac_ > sinir_.azami_deger) {
            return hata_ver_("değer sayısı sınırı aşıldı");
        }
        if (derinlik >= sinir_.azami_derinlik) {
            return hata_ver_("derinlik sınırı aşıldı");
        }
        if (i_ >= s_.size()) {
            return hata_ver_("beklenmeyen son");
        }
        const char c = s_[i_];
        switch (c) {
        case '{':
            return nesne_(d, derinlik);
        case '[':
            return dizi_(d, derinlik);
        case '"':
            d.tur = Tur::metin;
            return metin_(d.metin);
        case 't':
            d.tur = Tur::mantiksal;
            d.mantik = true;
            return sabit_("true");
        case 'f':
            d.tur = Tur::mantiksal;
            d.mantik = false;
            return sabit_("false");
        case 'n':
            d.tur = Tur::bos;
            return sabit_("null");
        default:
            if (c == '-' || (c >= '0' && c <= '9')) {
                return sayi_(d);
            }
            return hata_ver_("geçersiz değer");
        }
    }

    bool nesne_(Deger& d, std::size_t derinlik)
    {
        d.tur = Tur::nesne;
        ++i_;
        bosluk_();
        if (i_ < s_.size() && s_[i_] == '}') {
            ++i_;
            return true;
        }
        for (;;) {
            bosluk_();
            if (i_ >= s_.size() || s_[i_] != '"') {
                return hata_ver_("nesne anahtarı bekleniyordu");
            }
            std::string anahtar;
            if (!metin_(anahtar)) {
                return false;
            }
            if (d.nesne.size() >= kAzamiUye) {
                return hata_ver_("nesne üye sınırı aşıldı");
            }
            for (const auto& [k, v] : d.nesne) {
                (void)v;
                if (k == anahtar) {
                    return hata_ver_("yinelenen nesne anahtarı");
                }
            }
            bosluk_();
            if (i_ >= s_.size() || s_[i_] != ':') {
                return hata_ver_("':' bekleniyordu");
            }
            ++i_;
            bosluk_();
            d.nesne.emplace_back(std::move(anahtar), Deger{});
            if (!deger_(d.nesne.back().second, derinlik + 1)) {
                return false;
            }
            bosluk_();
            if (i_ >= s_.size()) {
                return hata_ver_("beklenmeyen son");
            }
            if (s_[i_] == ',') {
                ++i_;
                continue;
            }
            if (s_[i_] == '}') {
                ++i_;
                return true;
            }
            return hata_ver_("',' ya da '}' bekleniyordu");
        }
    }

    bool dizi_(Deger& d, std::size_t derinlik)
    {
        d.tur = Tur::dizi;
        ++i_;
        bosluk_();
        if (i_ < s_.size() && s_[i_] == ']') {
            ++i_;
            return true;
        }
        for (;;) {
            bosluk_();
            d.dizi.emplace_back();
            if (!deger_(d.dizi.back(), derinlik + 1)) {
                return false;
            }
            bosluk_();
            if (i_ >= s_.size()) {
                return hata_ver_("beklenmeyen son");
            }
            if (s_[i_] == ',') {
                ++i_;
                continue;
            }
            if (s_[i_] == ']') {
                ++i_;
                return true;
            }
            return hata_ver_("',' ya da ']' bekleniyordu");
        }
    }

    bool hex4_(std::uint32_t& v)
    {
        if (s_.size() - i_ < 4) {
            return hata_ver_("eksik \\u kaçışı");
        }
        v = 0;
        for (std::size_t k = 0; k < 4; ++k) {
            const char c = s_[i_ + k];
            std::uint32_t h = 0;
            if (c >= '0' && c <= '9') {
                h = static_cast<std::uint32_t>(c - '0');
            } else if (c >= 'a' && c <= 'f') {
                h = static_cast<std::uint32_t>(c - 'a' + 10);
            } else if (c >= 'A' && c <= 'F') {
                h = static_cast<std::uint32_t>(c - 'A' + 10);
            } else {
                return hata_ver_("geçersiz \\u kaçışı");
            }
            v = (v << 4u) | h;
        }
        i_ += 4;
        return true;
    }

    bool metin_(std::string& cikti)
    {
        ++i_;  // açılış tırnağı
        for (;;) {
            if (i_ >= s_.size()) {
                return hata_ver_("kapanmamış dize");
            }
            const auto c = static_cast<unsigned char>(s_[i_]);
            if (c == '"') {
                ++i_;
                return true;
            }
            if (c < 0x20u) {
                return hata_ver_("dizede denetim karakteri");
            }
            if (c == '\\') {
                ++i_;
                if (i_ >= s_.size()) {
                    return hata_ver_("kapanmamış dize");
                }
                const char e = s_[i_++];
                switch (e) {
                case '"': cikti.push_back('"'); break;
                case '\\': cikti.push_back('\\'); break;
                case '/': cikti.push_back('/'); break;
                case 'b': cikti.push_back('\b'); break;
                case 'f': cikti.push_back('\f'); break;
                case 'n': cikti.push_back('\n'); break;
                case 'r': cikti.push_back('\r'); break;
                case 't': cikti.push_back('\t'); break;
                case 'u': {
                    std::uint32_t kp = 0;
                    if (!hex4_(kp)) {
                        return false;
                    }
                    if (kp >= 0xD800u && kp <= 0xDBFFu) {
                        if (s_.size() - i_ < 6 || s_[i_] != '\\' || s_[i_ + 1] != 'u') {
                            return hata_ver_("eşlenmemiş vekil");
                        }
                        i_ += 2;
                        std::uint32_t alt = 0;
                        if (!hex4_(alt)) {
                            return false;
                        }
                        if (alt < 0xDC00u || alt > 0xDFFFu) {
                            return hata_ver_("eşlenmemiş vekil");
                        }
                        kp = 0x10000u + ((kp - 0xD800u) << 10u) + (alt - 0xDC00u);
                    } else if (kp >= 0xDC00u && kp <= 0xDFFFu) {
                        return hata_ver_("eşlenmemiş vekil");
                    }
                    utf8_ekle(kp, cikti);
                    break;
                }
                default:
                    return hata_ver_("geçersiz kaçış");
                }
                continue;
            }
            const std::size_t bas = i_;
            std::uint32_t kp = 0;
            if (!utf8_cek(s_, i_, kp)) {
                return hata_ver_("geçersiz UTF-8");
            }
            cikti.append(s_.substr(bas, i_ - bas));
        }
    }

    bool sayi_(Deger& d)
    {
        const std::size_t bas = i_;
        auto rakam = [&](std::size_t k) { return k < s_.size() && s_[k] >= '0' && s_[k] <= '9'; };
        if (s_[i_] == '-') {
            ++i_;
        }
        if (!rakam(i_)) {
            return hata_ver_("geçersiz sayı");
        }
        if (s_[i_] == '0') {
            ++i_;
        } else {
            while (rakam(i_)) {
                ++i_;
            }
        }
        if (i_ < s_.size() && s_[i_] == '.') {
            ++i_;
            if (!rakam(i_)) {
                return hata_ver_("geçersiz sayı");
            }
            while (rakam(i_)) {
                ++i_;
            }
        }
        if (i_ < s_.size() && (s_[i_] == 'e' || s_[i_] == 'E')) {
            ++i_;
            if (i_ < s_.size() && (s_[i_] == '+' || s_[i_] == '-')) {
                ++i_;
            }
            if (!rakam(i_)) {
                return hata_ver_("geçersiz sayı");
            }
            while (rakam(i_)) {
                ++i_;
            }
        }
        // Dilbilgisi yukarıda denetlendi; çeviri yerelden bağımsız (klasik yerel).
        std::istringstream iss(std::string(s_.substr(bas, i_ - bas)));
        iss.imbue(std::locale::classic());
        double v = 0.0;
        iss >> v;
        if (iss.fail() || !std::isfinite(v)) {
            return hata_ver_("sayı aralık dışı");
        }
        d.tur = Tur::sayi;
        d.sayi = v;
        return true;
    }

    std::string_view s_;
    CozumSiniri sinir_;
    std::size_t i_ = 0;
    std::size_t sayac_ = 0;
    std::string hata_;
};

// UTF-16 kod birimi sırasıyla karşılaştırma (RFC 8785 §3.2.3).
bool utf16_kucuk(std::string_view a, std::string_view b)
{
    std::size_t i = 0;
    std::size_t j = 0;
    std::uint32_t ka[2] = {0, 0};
    std::uint32_t kb[2] = {0, 0};
    std::size_t na = 0;
    std::size_t nb = 0;
    std::size_t ia = 0;
    std::size_t ib = 0;
    auto birimler = [](std::uint32_t kp, std::uint32_t (&u)[2]) -> std::size_t {
        if (kp < 0x10000u) {
            u[0] = kp;
            return 1;
        }
        const std::uint32_t v = kp - 0x10000u;
        u[0] = 0xD800u + (v >> 10u);
        u[1] = 0xDC00u + (v & 0x3FFu);
        return 2;
    };
    for (;;) {
        if (ia == na) {
            if (i >= a.size()) {
                na = 0;
            } else {
                std::uint32_t kp = 0;
                if (!utf8_cek(a, i, kp)) {
                    kp = static_cast<unsigned char>(a[i++]);
                }
                na = birimler(kp, ka);
            }
            ia = 0;
        }
        if (ib == nb) {
            if (j >= b.size()) {
                nb = 0;
            } else {
                std::uint32_t kp = 0;
                if (!utf8_cek(b, j, kp)) {
                    kp = static_cast<unsigned char>(b[j++]);
                }
                nb = birimler(kp, kb);
            }
            ib = 0;
        }
        if (na == 0 || nb == 0) {
            return na == 0 && nb != 0;
        }
        if (ka[ia] != kb[ib]) {
            return ka[ia] < kb[ib];
        }
        ++ia;
        ++ib;
    }
}

void yaz_ic(const Deger& d, std::string& c, bool kanonik)
{
    switch (d.tur) {
    case Tur::bos:
        c += "null";
        return;
    case Tur::mantiksal:
        c += d.mantik ? "true" : "false";
        return;
    case Tur::sayi:
        sayi_yaz(d.sayi, c);
        return;
    case Tur::metin:
        metin_yaz(d.metin, c);
        return;
    case Tur::dizi: {
        c.push_back('[');
        bool ilk = true;
        for (const auto& e : d.dizi) {
            if (!ilk) {
                c.push_back(',');
            }
            ilk = false;
            yaz_ic(e, c, kanonik);
        }
        c.push_back(']');
        return;
    }
    case Tur::nesne: {
        c.push_back('{');
        std::vector<const std::pair<std::string, Deger>*> sira;
        sira.reserve(d.nesne.size());
        for (const auto& p : d.nesne) {
            sira.push_back(&p);
        }
        if (kanonik) {
            std::sort(sira.begin(), sira.end(),
                      [](const auto* x, const auto* y) { return utf16_kucuk(x->first, y->first); });
        }
        bool ilk = true;
        for (const auto* p : sira) {
            if (!ilk) {
                c.push_back(',');
            }
            ilk = false;
            metin_yaz(p->first, c);
            c.push_back(':');
            yaz_ic(p->second, c, kanonik);
        }
        c.push_back('}');
        return;
    }
    }
}

}  // namespace

bool coz(std::string_view metin, Deger& sonuc, std::string* hata, const CozumSiniri& sinir)
{
    sonuc = Deger{};
    Cozucu c(metin, sinir);
    return c.calis(sonuc, hata);
}

bool utf8_gecerli(std::string_view s) noexcept
{
    std::size_t i = 0;
    while (i < s.size()) {
        std::uint32_t kp = 0;
        if (!utf8_cek(s, i, kp)) {
            return false;
        }
    }
    return true;
}

bool kod_noktalari(std::string_view s, std::vector<std::uint32_t>& cikti)
{
    cikti.clear();
    std::size_t i = 0;
    while (i < s.size()) {
        std::uint32_t kp = 0;
        if (!utf8_cek(s, i, kp)) {
            return false;
        }
        cikti.push_back(kp);
    }
    return true;
}

void yaz(const Deger& d, std::string& cikti) { yaz_ic(d, cikti, false); }

void kanonik_yaz(const Deger& d, std::string& cikti) { yaz_ic(d, cikti, true); }

void metin_yaz(std::string_view s, std::string& c)
{
    static constexpr char kHex[] = "0123456789abcdef";
    c.push_back('"');
    for (const char ch : s) {
        const auto u = static_cast<unsigned char>(ch);
        switch (ch) {
        case '"': c += "\\\""; break;
        case '\\': c += "\\\\"; break;
        case '\n': c += "\\n"; break;
        case '\r': c += "\\r"; break;
        case '\t': c += "\\t"; break;
        case '\b': c += "\\b"; break;
        case '\f': c += "\\f"; break;
        default:
            if (u < 0x20u) {
                c += "\\u00";
                c.push_back(kHex[u >> 4u]);
                c.push_back(kHex[u & 0x0Fu]);
            } else {
                c.push_back(ch);
            }
        }
    }
    c.push_back('"');
}

void sayi_yaz(double v, std::string& c)
{
    if (!std::isfinite(v)) {
        c += "null";
        return;
    }
    if (v == 0.0) {
        c.push_back('0');
        return;
    }
    char tampon[64];
    const auto r = std::to_chars(tampon, tampon + sizeof tampon, v, std::chars_format::scientific);
    if (r.ec != std::errc{}) {
        c += "null";
        return;
    }
    // "-d.ddde±XX" → işaret, rakamlar (s), ondalık üs (n = üs + 1; ECMA-262 Number::toString).
    std::string_view t(tampon, static_cast<std::size_t>(r.ptr - tampon));
    std::string rakamlar;
    bool eksi = false;
    std::size_t k = 0;
    if (!t.empty() && t[0] == '-') {
        eksi = true;
        k = 1;
    }
    int us = 0;
    for (; k < t.size(); ++k) {
        if (t[k] == 'e') {
            int isaret = 1;
            ++k;
            if (k < t.size() && (t[k] == '+' || t[k] == '-')) {
                isaret = t[k] == '-' ? -1 : 1;
                ++k;
            }
            int e = 0;
            for (; k < t.size(); ++k) {
                e = e * 10 + (t[k] - '0');
            }
            us = isaret * e;
            break;
        }
        if (t[k] != '.') {
            rakamlar.push_back(t[k]);
        }
    }
    while (rakamlar.size() > 1 && rakamlar.back() == '0') {
        rakamlar.pop_back();
    }
    const int kk = static_cast<int>(rakamlar.size());
    const int n = us + 1;
    if (eksi) {
        c.push_back('-');
    }
    if (kk <= n && n <= 21) {
        c += rakamlar;
        c.append(static_cast<std::size_t>(n - kk), '0');
    } else if (0 < n && n <= 21) {
        c.append(rakamlar, 0, static_cast<std::size_t>(n));
        c.push_back('.');
        c.append(rakamlar, static_cast<std::size_t>(n), std::string::npos);
    } else if (-6 < n && n <= 0) {
        c += "0.";
        c.append(static_cast<std::size_t>(-n), '0');
        c += rakamlar;
    } else {
        c.push_back(rakamlar[0]);
        if (kk > 1) {
            c.push_back('.');
            c.append(rakamlar, 1, std::string::npos);
        }
        c.push_back('e');
        c.push_back(n - 1 < 0 ? '-' : '+');
        c += std::to_string(n - 1 < 0 ? -(n - 1) : n - 1);
    }
}

void tamsayi_yaz(std::int64_t v, std::string& c) { c += std::to_string(v); }

void tamsayi_yaz(std::uint64_t v, std::string& c) { c += std::to_string(v); }

void Nesne::anahtar_(std::string_view ad)
{
    if (!ilk_) {
        s_.push_back(',');
    }
    ilk_ = false;
    metin_yaz(ad, s_);
    s_.push_back(':');
}

Nesne& Nesne::metin(std::string_view ad, std::string_view deger)
{
    anahtar_(ad);
    metin_yaz(deger, s_);
    return *this;
}

Nesne& Nesne::sayi(std::string_view ad, std::int64_t deger)
{
    anahtar_(ad);
    tamsayi_yaz(deger, s_);
    return *this;
}

Nesne& Nesne::sayi_u(std::string_view ad, std::uint64_t deger)
{
    anahtar_(ad);
    tamsayi_yaz(deger, s_);
    return *this;
}

Nesne& Nesne::ondalik(std::string_view ad, double deger)
{
    anahtar_(ad);
    sayi_yaz(deger, s_);
    return *this;
}

Nesne& Nesne::mantik(std::string_view ad, bool deger)
{
    anahtar_(ad);
    s_ += deger ? "true" : "false";
    return *this;
}

Nesne& Nesne::bos(std::string_view ad)
{
    anahtar_(ad);
    s_ += "null";
    return *this;
}

Nesne& Nesne::ham(std::string_view ad, std::string_view ham)
{
    anahtar_(ad);
    s_ += ham;
    return *this;
}

}  // namespace ida::a3::json
