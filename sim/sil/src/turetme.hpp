// turetme.hpp — A5'in anahtar türetme işlevi ve Philox4x32-10 (A5-K83, K110, K118; Ek D.1,
// D.7). SIL koşucusu IDA_BOARD_HELLO.nonce'u buradan, --a1-seed tohumuyla ve
// `ida/a1/nonce_board/<boot_count>/<deneme>` etiketiyle belirlenimci olarak türetir (A1-K114).
//
// Yalnız tamsayı işlemleri: sonuçlar her platformda bit düzeyinde aynıdır (A5-K112).
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace ida::sil::turetme {

// SHA-256 (FIPS 180-4).
std::array<std::uint8_t, 32> sha256(const std::uint8_t* veri, std::size_t n);

// Philox4x32-10 (Random123; A5 Ek D.1): sayaç (c0 c1 c2 c3), anahtar (k0 k1).
std::array<std::uint32_t, 4> philox(std::array<std::uint32_t, 4> sayac,
                                    std::array<std::uint32_t, 2> anahtar);

// A5-K83: K = SHA-256("IDA-A5-RNG-1" ‖ 0x00 ‖ LE64(tohum) ‖ UTF8(ad) ‖ 0x00);
// k0 = LE32(K[0..3]), k1 = LE32(K[4..7]).
std::array<std::uint32_t, 2> anahtar(std::uint64_t tohum, std::string_view ad);

// A5-K110: n'inci çekiliş; c0 = n mod 2^32, c1 = n / 2^32, c2 verilen, c3 = 0.
std::array<std::uint32_t, 4> cek(std::uint64_t tohum, std::string_view ad, std::uint64_t n,
                                 std::uint32_t c2 = 0u);

// A5-K118: u32 değer w0; 0 çıkarsa sonraki n denenir (ilk sıfırdan farklı değer).
std::uint32_t u32_sifirsiz(std::uint64_t tohum, std::string_view ad);

// A5-K118: u64 değer w0 | (w1 << 32), n = 0 (ör. a1/board_tohum).
std::uint64_t u64(std::uint64_t tohum, std::string_view ad);

// A1-K114 etiketi: "ida/a1/nonce_board/<boot_count>/<deneme>" (başında sıfır olmayan onluk).
std::string nonce_board_etiketi(std::uint32_t boot_count, std::uint32_t deneme);

}  // namespace ida::sil::turetme
