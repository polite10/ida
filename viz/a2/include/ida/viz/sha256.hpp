// SHA-256 (FIPS 180-4). A2'de iki yerde gerekir: dalga kümesi özeti (A2 §3.4, H17) ve
// manifest özeti (A7-K05, A2-K13). Dış bağımlılık eklememek için burada yazıldı.
#ifndef IDA_VIZ_SHA256_HPP
#define IDA_VIZ_SHA256_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

namespace ida::viz {

using Ozet = std::array<std::uint8_t, 32>;

class Sha256 {
 public:
  Sha256();
  void ekle(const void* veri, std::size_t n);
  Ozet bitir();

 private:
  void blok_isle(const std::uint8_t* blok);

  std::uint32_t h_[8];
  std::uint8_t tampon_[64];
  std::size_t tampon_n_ = 0;
  std::uint64_t toplam_bayt_ = 0;
};

Ozet sha256(const void* veri, std::size_t n);

// 64 haneli küçük harf onaltılık yazım (A7-K05).
std::string onaltilik(const Ozet& ozet);

}  // namespace ida::viz

#endif
