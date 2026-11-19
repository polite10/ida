// W0-MP3a ölçüm prototipi: komut satırı sayılarının katı çözümü (atılabilir).
// Tam metin sayı olmalı (boşluk, işaret, ek karakter yok), sınırlar GENİŞ türde denetlenir, sonra daraltılır.
#pragma once

#include <cerrno>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <optional>
#include <string>

namespace mp3 {

inline std::optional<uint64_t> tam_sayi(const std::string &t, uint64_t en_az, uint64_t en_cok, int taban = 10)
{
    if (t.empty() || t.size() > 20) return std::nullopt;
    for (char c : t) {
        const bool rakam = (c >= '0' && c <= '9') || (taban == 16 && ((c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F')));
        if (!rakam) return std::nullopt;
    }
    errno = 0;
    char *son = nullptr;
    const unsigned long long v = std::strtoull(t.c_str(), &son, taban);
    if (errno != 0 || son == nullptr || *son != '\0') return std::nullopt;
    if (v < en_az || v > en_cok) return std::nullopt;
    return static_cast<uint64_t>(v);
}

inline std::optional<double> ondalik(const std::string &t, double en_az, double en_cok)
{
    if (t.empty() || t.size() > 32) return std::nullopt;
    for (char c : t)
        if (!((c >= '0' && c <= '9') || c == '.')) return std::nullopt;
    errno = 0;
    char *son = nullptr;
    const double v = std::strtod(t.c_str(), &son);
    if (errno != 0 || son == nullptr || *son != '\0' || !std::isfinite(v)) return std::nullopt;
    if (v < en_az || v > en_cok) return std::nullopt;
    return v;
}

}  // namespace mp3
