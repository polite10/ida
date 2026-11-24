// RFC 8785 (JSON Canonicalization Scheme) dizilimi (A5-K17).
//
// Nesne anahtarları UTF-16 kod birimlerine göre sıralanır; belirteçler arasında boşluk yok;
// dizgiler RFC 8785 §3.2.2.2 kaçışlarıyla (yalnız `"`, `\` ve U+0000–U+001F; ASCII dışı
// karakterler kaçışsız UTF-8); sayılar ECMAScript Number dizilimiyle (§3.2.2.3).
#include "ic.hpp"

#include <algorithm>
#include <cstdio>

namespace ida::senaryo {

namespace ic {

namespace {

// Geçerli UTF-8 dizgiyi UTF-16 kod birimlerine çevirir (sıralama için).
std::u16string utf16(std::string_view s)
{
    std::u16string o;
    std::size_t i = 0;
    while (i < s.size()) {
        const auto c = static_cast<unsigned char>(s[i]);
        char32_t cp = 0;
        std::size_t n = 1;
        if (c < 0x80u) {
            cp = c;
        } else if ((c & 0xE0u) == 0xC0u) {
            cp = c & 0x1Fu;
            n = 2;
        } else if ((c & 0xF0u) == 0xE0u) {
            cp = c & 0x0Fu;
            n = 3;
        } else {
            cp = c & 0x07u;
            n = 4;
        }
        for (std::size_t j = 1; j < n && i + j < s.size(); ++j) {
            cp = (cp << 6) | (static_cast<unsigned char>(s[i + j]) & 0x3Fu);
        }
        i += n;
        if (cp >= 0x10000u) {
            cp -= 0x10000u;
            o += static_cast<char16_t>(0xD800u + (cp >> 10));
            o += static_cast<char16_t>(0xDC00u + (cp & 0x3FFu));
        } else {
            o += static_cast<char16_t>(cp);
        }
    }
    return o;
}

}  // namespace

std::string dizgi_kanonik(std::string_view s)
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
            if (u < 0x20u) {
                char tampon[8];
                std::snprintf(tampon, sizeof tampon, "\\u%04x", static_cast<unsigned>(u));
                o += tampon;
            } else {
                o += c;
            }
        }
    }
    o += '"';
    return o;
}

void kanonik_yaz(const Deger& d, std::string& o)
{
    switch (d.tur) {
    case Tur::bos:
    case Tur::gecersiz:
        o += "null";
        return;
    case Tur::mantiksal:
        o += d.mantik ? "true" : "false";
        return;
    case Tur::tamsayi:
        o += sayi_dizilimi(static_cast<double>(d.tam));
        return;
    case Tur::sayi:
        o += sayi_dizilimi(d.sayi);
        return;
    case Tur::dizgi:
        o += dizgi_kanonik(d.dizgi);
        return;
    case Tur::dizi: {
        o += '[';
        bool ilk = true;
        for (const auto& e : d.dizi) {
            if (!ilk) {
                o += ',';
            }
            ilk = false;
            kanonik_yaz(e, o);
        }
        o += ']';
        return;
    }
    case Tur::tablo: {
        std::vector<std::pair<std::u16string, const std::pair<const std::string, Deger>*>> s;
        s.reserve(d.tablo.size());
        for (const auto& kv : d.tablo) {
            s.emplace_back(utf16(kv.first), &kv);
        }
        std::sort(s.begin(), s.end(),
                  [](const auto& a, const auto& b) { return a.first < b.first; });
        o += '{';
        bool ilk = true;
        for (const auto& e : s) {
            if (!ilk) {
                o += ',';
            }
            ilk = false;
            o += dizgi_kanonik(e.second->first);
            o += ':';
            kanonik_yaz(e.second->second, o);
        }
        o += '}';
        return;
    }
    }
}

}  // namespace ic

std::string kanonik_json(const Deger& d)
{
    std::string o;
    ic::kanonik_yaz(d, o);
    return o;
}

}  // namespace ida::senaryo
