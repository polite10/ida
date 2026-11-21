// Değer ağacı, anahtar yolu ve bulgu toplayıcı.
#include "ic.hpp"

#include <algorithm>
#include <cstdio>
#include <tuple>

namespace ida::senaryo {

Deger Deger::yeni_tablo(Konum k)
{
    Deger d;
    d.tur = Tur::tablo;
    d.konum = k;
    return d;
}
Deger Deger::yeni_dizi(Konum k)
{
    Deger d;
    d.tur = Tur::dizi;
    d.konum = k;
    return d;
}
Deger Deger::yeni_dizgi(std::string s, Konum k)
{
    Deger d;
    d.tur = Tur::dizgi;
    d.dizgi = std::move(s);
    d.konum = k;
    return d;
}
Deger Deger::yeni_tamsayi(std::int64_t v, Konum k)
{
    Deger d;
    d.tur = Tur::tamsayi;
    d.tam = v;
    d.konum = k;
    return d;
}
Deger Deger::yeni_sayi(double v, Konum k)
{
    Deger d;
    d.tur = Tur::sayi;
    d.sayi = v;
    d.konum = k;
    return d;
}
Deger Deger::yeni_mantiksal(bool v, Konum k)
{
    Deger d;
    d.tur = Tur::mantiksal;
    d.mantik = v;
    d.konum = k;
    return d;
}

double Deger::cift() const
{
    return tur == Tur::tamsayi ? static_cast<double>(tam) : sayi;
}

const Deger* Deger::bul(std::string_view anahtar) const
{
    if (tur != Tur::tablo) {
        return nullptr;
    }
    const auto it = tablo.find(std::string(anahtar));
    return it == tablo.end() ? nullptr : &it->second;
}

Deger* Deger::bul(std::string_view anahtar)
{
    return const_cast<Deger*>(static_cast<const Deger*>(this)->bul(anahtar));
}

const Deger* Deger::yol_bul(std::string_view noktali) const
{
    const Deger* d = this;
    std::size_t bas = 0;
    while (d != nullptr) {
        const std::size_t nokta = noktali.find('.', bas);
        const std::string_view parca =
            noktali.substr(bas, nokta == std::string_view::npos ? std::string_view::npos : nokta - bas);
        d = d->bul(parca);
        if (nokta == std::string_view::npos) {
            return d;
        }
        bas = nokta + 1;
    }
    return nullptr;
}

Deger* Deger::yol_bul(std::string_view noktali)
{
    return const_cast<Deger*>(static_cast<const Deger*>(this)->yol_bul(noktali));
}

bool esit(const Deger& a, const Deger& b)
{
    if (a.sayisal_mi() && b.sayisal_mi()) {
        if (a.tur == Tur::tamsayi && b.tur == Tur::tamsayi) {
            return a.tam == b.tam;
        }
        return a.cift() == b.cift();
    }
    if (a.tur != b.tur) {
        return false;
    }
    switch (a.tur) {
    case Tur::bos:
    case Tur::gecersiz:
        return true;
    case Tur::mantiksal:
        return a.mantik == b.mantik;
    case Tur::dizgi:
        return a.dizgi == b.dizgi;
    case Tur::dizi:
        if (a.dizi.size() != b.dizi.size()) {
            return false;
        }
        for (std::size_t i = 0; i < a.dizi.size(); ++i) {
            if (!esit(a.dizi[i], b.dizi[i])) {
                return false;
            }
        }
        return true;
    case Tur::tablo:
        if (a.tablo.size() != b.tablo.size()) {
            return false;
        }
        for (const auto& [k, v] : a.tablo) {
            const Deger* w = b.bul(k);
            if (w == nullptr || !esit(v, *w)) {
                return false;
            }
        }
        return true;
    case Tur::tamsayi:
    case Tur::sayi:
        break;
    }
    return false;
}

bool ciplak_anahtar_mi(std::string_view s)
{
    if (s.empty()) {
        return false;
    }
    return std::all_of(s.begin(), s.end(), [](char c) {
        return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ||
               c == '_' || c == '-';
    });
}

Yol Yol::ekle(std::string anahtar) const
{
    Yol y = *this;
    y.parcalar.push_back(YolParcasi{std::move(anahtar)});
    return y;
}

Yol Yol::ekle(std::size_t indis) const
{
    Yol y = *this;
    y.parcalar.push_back(YolParcasi{indis});
    return y;
}

namespace {
// TOML temel dizgisi (A5-K93: çıplak olamayan ad).
std::string toml_temel_dizgi(std::string_view s)
{
    std::string o = "\"";
    for (const char c : s) {
        const auto u = static_cast<unsigned char>(c);
        switch (c) {
        case '"': o += "\\\""; break;
        case '\\': o += "\\\\"; break;
        case '\b': o += "\\b"; break;
        case '\t': o += "\\t"; break;
        case '\n': o += "\\n"; break;
        case '\f': o += "\\f"; break;
        case '\r': o += "\\r"; break;
        default:
            if (u < 0x20u || u == 0x7Fu) {
                char tampon[8];
                std::snprintf(tampon, sizeof tampon, "\\u%04X", static_cast<unsigned>(u));
                o += tampon;
            } else {
                o += c;
            }
        }
    }
    o += '"';
    return o;
}
}  // namespace

std::string Yol::metin() const
{
    std::string o;
    for (const auto& p : parcalar) {
        if (const auto* k = std::get_if<std::string>(&p.p)) {
            if (!o.empty()) {
                o += '.';
            }
            o += ciplak_anahtar_mi(*k) ? *k : toml_temel_dizgi(*k);
        } else {
            o += '[' + std::to_string(std::get<std::size_t>(p.p)) + ']';
        }
    }
    return o;
}

std::string Yol::desen_metni() const
{
    std::string o;
    for (const auto& p : parcalar) {
        if (const auto* k = std::get_if<std::string>(&p.p)) {
            if (!o.empty()) {
                o += '.';
            }
            o += *k;
        } else {
            o += "[]";
        }
    }
    return o;
}

void bulgulari_sirala(std::vector<Bulgu>& b)
{
    std::stable_sort(b.begin(), b.end(), [](const Bulgu& x, const Bulgu& y) {
        const std::int64_t sx = x.satir.value_or(0);
        const std::int64_t sy = y.satir.value_or(0);
        return std::tie(x.dosya, sx, x.yol, x.kural, x.ileti) <
               std::tie(y.dosya, sy, y.yol, y.kural, y.ileti);
    });
}

namespace ic {

void Bulgucu::ekle(std::string_view kural, Onem onem, Konum k, const Yol& yol, std::string ileti)
{
    Bulgu x;
    x.kural = std::string(kural);
    x.onem = onem;
    x.asama = asama;
    const int no = (k.dosya >= 0 && static_cast<std::size_t>(k.dosya) < dosyalar.size())
                       ? k.dosya
                       : varsayilan_dosya;
    if (no >= 0 && static_cast<std::size_t>(no) < dosyalar.size()) {
        x.dosya = dosyalar[static_cast<std::size_t>(no)];
    }
    x.yol = yol.metin();
    if (k.dosya >= 0 && k.satir > 0) {
        x.satir = k.satir;
    }
    x.ileti = std::move(ileti);
    bulgular.push_back(std::move(x));
}

void Bulgucu::hata_d(std::string_view kural, const Deger* d, Konum ust, const Yol& yol,
                     std::string ileti)
{
    if (d != nullptr) {
        hata(kural, d->konum, yol, std::move(ileti));
    } else {
        hata(kural, Konum{ust.dosya, 0}, yol, std::move(ileti));
    }
}

bool Bulgucu::hata_var() const
{
    return hata_sayisi() > 0;
}

std::size_t Bulgucu::hata_sayisi() const
{
    return static_cast<std::size_t>(std::count_if(
        bulgular.begin(), bulgular.end(), [](const Bulgu& x) { return x.onem == Onem::hata; }));
}

std::string dizin_adi(std::string_view yol)
{
    const std::size_t i = yol.rfind('/');
    return i == std::string_view::npos ? std::string() : std::string(yol.substr(0, i));
}

std::string yol_birlestir(std::string_view dizin, std::string_view goreli)
{
    std::vector<std::string> parcalar;
    auto ekle = [&](std::string_view s) {
        std::size_t bas = 0;
        while (bas <= s.size()) {
            const std::size_t b = s.find('/', bas);
            const std::string_view p =
                s.substr(bas, b == std::string_view::npos ? std::string_view::npos : b - bas);
            if (p == "..") {
                if (!parcalar.empty()) {
                    parcalar.pop_back();
                }
            } else if (!p.empty() && p != ".") {
                parcalar.emplace_back(p);
            }
            if (b == std::string_view::npos) {
                break;
            }
            bas = b + 1;
        }
    };
    ekle(dizin);
    ekle(goreli);
    std::string o;
    for (const auto& p : parcalar) {
        if (!o.empty()) {
            o += '/';
        }
        o += p;
    }
    return o;
}

}  // namespace ic

}  // namespace ida::senaryo
