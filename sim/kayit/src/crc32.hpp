// crc32.hpp — CRC-32 (ISO 3309 / zlib çokterimlisi 0xEDB88320; MCAP spesifikasyonundaki CRC).
// A6 okuyucusu parça ve ek CRC'sini bu kendi gerçekleştirimiyle hesaplar; MCAP kitaplığının
// hesabına dayanmaz (A6-K59).
#pragma once

#include <cstddef>
#include <cstdint>

namespace ida::kayit::ic {

// Akış biçimi: crc = crc32_baslat(); crc = crc32_ekle(crc, p, n); …; sonuc = crc32_bitir(crc).
inline constexpr std::uint32_t crc32_baslat() noexcept { return 0xFFFFFFFFu; }
std::uint32_t crc32_ekle(std::uint32_t crc, const std::uint8_t* veri, std::size_t n) noexcept;
inline constexpr std::uint32_t crc32_bitir(std::uint32_t crc) noexcept { return crc ^ 0xFFFFFFFFu; }

inline std::uint32_t crc32(const std::uint8_t* veri, std::size_t n) noexcept
{
    return crc32_bitir(crc32_ekle(crc32_baslat(), veri, n));
}

}  // namespace ida::kayit::ic
