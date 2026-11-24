// ozet.hpp — SHA-1 (RFC 3174; yalnız WebSocket el sıkışması, RFC 6455 §4.2.2), SHA-256 (FIPS
// 180-4; katalog özeti A3-K16, yineleme anahtarının gövde özeti A3-K53, işlem özeti A3-K103),
// Base64 (RFC 4648 §4) ve onaltılık gösterim. SHA-256 şimdilik bu dosyadadır (W4-a3 "SHA-256
// gerekiyorsa şimdilik kendi dosyanda"; ortak yere toplama ayrı iş).
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace ida::a3::ozet {

std::array<std::uint8_t, 20> sha1(const std::uint8_t* veri, std::size_t n);
std::array<std::uint8_t, 32> sha256(const std::uint8_t* veri, std::size_t n);
inline std::array<std::uint8_t, 32> sha256(std::string_view s)
{
    return sha256(reinterpret_cast<const std::uint8_t*>(s.data()), s.size());
}

std::string base64(const std::uint8_t* veri, std::size_t n);
// Base64 çözüm; geçersiz girdi false.
bool base64_coz(std::string_view s, std::string& cikti);
std::string onaltilik(const std::uint8_t* veri, std::size_t n);

// RFC 6455 §4.2.2: Sec-WebSocket-Accept değeri.
std::string ws_kabul_anahtari(std::string_view istemci_anahtari);

}  // namespace ida::a3::ozet
