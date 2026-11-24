// Sayısal dönüşümler: ondalık yazım → binary64 (A5-K96) ve binary64 → ECMAScript dizilimi
// (RFC 8785 §3.2.2.3; A5-K17).
//
// Ondalıktan ikiliye: toml++ Clang'de kayan noktalıyı akış (istringstream) ile okur ve alt
// normal, taşan ve alt taşan yazımları ayrıştırma hatası sayar; bu yüzden bütün kayan
// noktalı değerler kaynak metinden burada yeniden çevrilir. MSVC'de `std::from_chars`
// (doğru yuvarlamalı), macOS'te `strtod_l` C yerel ayarıyla (libc'nin gdtoa tabanlı doğru
// yuvarlamalı çevirisi; `from_chars(double)` macOS 26'dan önce yoktur) kullanılır.
#include "ic.hpp"

#include <charconv>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <string>
#include <system_error>

#if defined(__APPLE__)
#include <xlocale.h>
#endif

namespace ida::senaryo {

namespace ic {

namespace {
bool rakam(char c) { return c >= '0' && c <= '9'; }

// [+-]? rakam+ ('.' rakam+)? ([eE] [+-]? rakam+)?  (alt çizgiler önceden çıkarılmış)
bool bicim_gecerli(std::string_view s)
{
    std::size_t i = 0;
    if (i < s.size() && (s[i] == '+' || s[i] == '-')) {
        ++i;
    }
    const std::size_t b0 = i;
    while (i < s.size() && rakam(s[i])) {
        ++i;
    }
    if (i == b0) {
        return false;
    }
    if (i < s.size() && s[i] == '.') {
        ++i;
        const std::size_t b1 = i;
        while (i < s.size() && rakam(s[i])) {
            ++i;
        }
        if (i == b1) {
            return false;
        }
    }
    if (i < s.size() && (s[i] == 'e' || s[i] == 'E')) {
        ++i;
        if (i < s.size() && (s[i] == '+' || s[i] == '-')) {
            ++i;
        }
        const std::size_t b2 = i;
        while (i < s.size() && rakam(s[i])) {
            ++i;
        }
        if (i == b2) {
            return false;
        }
    }
    return i == s.size();
}

#if defined(_MSC_VER)
// Anlamlı ilk basamağın ondalık üssü (değer ≈ 0.d × 10^ust); taşma/alt taşma ayrımı için.
long long etkin_us(std::string_view s)
{
    long long us = 0;
    std::size_t i = 0;
    if (i < s.size() && (s[i] == '+' || s[i] == '-')) {
        ++i;
    }
    long long tamsayi_basamak = 0;
    long long ilk = -1;  // ilk sıfırdan farklı basamağın konumu (tamsayı kısmına göre)
    long long konum = 0;
    bool nokta = false;
    for (; i < s.size() && s[i] != 'e' && s[i] != 'E'; ++i) {
        if (s[i] == '.') {
            nokta = true;
            continue;
        }
        if (!nokta) {
            ++tamsayi_basamak;
        }
        if (s[i] != '0' && ilk < 0) {
            ilk = konum;
        }
        ++konum;
    }
    if (i < s.size()) {
        const std::string_view e = s.substr(i + 1);
        long long v = 0;
        bool eksi = false;
        std::size_t j = 0;
        if (j < e.size() && (e[j] == '+' || e[j] == '-')) {
            eksi = e[j] == '-';
            ++j;
        }
        for (; j < e.size(); ++j) {
            if (v < 1'000'000'000'000LL) {
                v = v * 10 + (e[j] - '0');
            }
        }
        us = eksi ? -v : v;
    }
    return us + tamsayi_basamak - ilk;
}
#endif
}  // namespace

bool sifir_olmayan_basamak_var(std::string_view s)
{
    for (const char c : s) {
        if (c == 'e' || c == 'E') {
            break;
        }
        if (c >= '1' && c <= '9') {
            return true;
        }
    }
    return false;
}

std::optional<double> ondalik_cevir(std::string_view metin)
{
    if (!bicim_gecerli(metin)) {
        return std::nullopt;
    }
    const bool eksi = metin[0] == '-';
#if defined(_MSC_VER)
    std::string_view s = metin;
    if (s[0] == '+') {
        s.remove_prefix(1);
    }
    double v = 0.0;
    const auto r = std::from_chars(s.data(), s.data() + s.size(), v, std::chars_format::general);
    if (r.ec == std::errc::result_out_of_range) {
        const bool tasma = sifir_olmayan_basamak_var(s) && etkin_us(s) > 0;
        v = tasma ? std::numeric_limits<double>::infinity() : 0.0;
        return eksi ? -v : v;
    }
    if (r.ec != std::errc() || r.ptr != s.data() + s.size()) {
        return std::nullopt;
    }
    return v;
#else
    const std::string kopya(metin);
    char* son = nullptr;
#if defined(__APPLE__)
    static const locale_t c_yerel = newlocale(LC_ALL_MASK, "C", static_cast<locale_t>(nullptr));
    const double v = strtod_l(kopya.c_str(), &son, c_yerel);
#else
    const double v = std::strtod(kopya.c_str(), &son);
#endif
    if (son != kopya.c_str() + kopya.size()) {
        return std::nullopt;
    }
    (void)eksi;
    return v;
#endif
}

}  // namespace ic

std::string sayi_dizilimi(double v)
{
    if (v == 0.0) {
        return "0";  // −0 dahil (RFC 8785 §3.2.2.3)
    }
    char tampon[64];
    const auto r = std::to_chars(tampon, tampon + sizeof tampon, v, std::chars_format::scientific);
    const std::string_view bilimsel(tampon, static_cast<std::size_t>(r.ptr - tampon));
    // Biçim: [-]d[.ddd]e(+|-)XX
    std::size_t i = 0;
    std::string isaret;
    if (bilimsel[0] == '-') {
        isaret = "-";
        i = 1;
    }
    const std::size_t e = bilimsel.find('e');
    std::string basamaklar;
    for (std::size_t j = i; j < e; ++j) {
        if (bilimsel[j] != '.') {
            basamaklar += bilimsel[j];
        }
    }
    const int us = std::atoi(std::string(bilimsel.substr(e + 1)).c_str());
    const int k = static_cast<int>(basamaklar.size());
    const int n = us + 1;  // değer = 0.basamaklar × 10^n
    std::string o = isaret;
    if (k <= n && n <= 21) {
        o += basamaklar;
        o.append(static_cast<std::size_t>(n - k), '0');
    } else if (0 < n && n <= 21) {
        o += basamaklar.substr(0, static_cast<std::size_t>(n));
        o += '.';
        o += basamaklar.substr(static_cast<std::size_t>(n));
    } else if (-6 < n && n <= 0) {
        o += "0.";
        o.append(static_cast<std::size_t>(-n), '0');
        o += basamaklar;
    } else {
        o += basamaklar[0];
        if (k > 1) {
            o += '.';
            o += basamaklar.substr(1);
        }
        o += 'e';
        o += (n - 1 >= 0) ? '+' : '-';
        o += std::to_string(n - 1 >= 0 ? n - 1 : -(n - 1));
    }
    return o;
}

}  // namespace ida::senaryo
