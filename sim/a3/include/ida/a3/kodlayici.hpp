// ida/a3/kodlayici.hpp — A3 tel tiplerinin küçük uçlu yazıcısı ve okuyucusu (A3 §3.1). Üretilen
// kodlayıcılar (yukler.cpp) ve çerçeve başlığı bunları kullanır. Bellek ayırmaz.
// Kayıt kitaplığı (`sim/kayit`, A6-K08) iletilerini aynı yazıcıyla kodlar (W5-K36); bu yüzden
// başlık geneldir. `sim/a3/src/kodlayici.hpp` buraya yönlendirir.
#pragma once

#include <charconv>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <string_view>

namespace ida::a3 {

class Yazici {
public:
    Yazici(std::uint8_t* hedef, std::size_t kapasite) noexcept : p_(hedef), n_(kapasite) {}

    void u8(std::uint8_t v) noexcept { yaz_(v, 1); }
    void u16(std::uint16_t v) noexcept { yaz_(v, 2); }
    void u32(std::uint32_t v) noexcept { yaz_(v, 4); }
    void u64(std::uint64_t v) noexcept { yaz_(v, 8); }
    void i32(std::int32_t v) noexcept { yaz_(static_cast<std::uint32_t>(v), 4); }
    void i64(std::int64_t v) noexcept { yaz_(static_cast<std::uint64_t>(v), 8); }
    void f32(float v) noexcept
    {
        std::uint32_t b = 0;
        std::memcpy(&b, &v, sizeof b);
        yaz_(b, 4);
    }
    void f64(double v) noexcept
    {
        std::uint64_t b = 0;
        std::memcpy(&b, &v, sizeof b);
        yaz_(b, 8);
    }
    void baytlar(const std::uint8_t* v, std::size_t n) noexcept
    {
        if (!tamam_ || n > n_ - k_) {
            tamam_ = false;
            return;
        }
        if (n > 0) {
            std::memcpy(p_ + k_, v, n);
        }
        k_ += n;
    }
    bool tamam() const noexcept { return tamam_; }
    std::size_t konum() const noexcept { return k_; }

private:
    void yaz_(std::uint64_t v, std::size_t bayt) noexcept
    {
        if (!tamam_ || bayt > n_ - k_) {
            tamam_ = false;
            return;
        }
        for (std::size_t i = 0; i < bayt; ++i) {
            p_[k_ + i] = static_cast<std::uint8_t>((v >> (8u * i)) & 0xFFu);
        }
        k_ += bayt;
    }
    std::uint8_t* p_;
    std::size_t n_;
    std::size_t k_ = 0;
    bool tamam_ = true;
};

class Okuyucu {
public:
    Okuyucu(const std::uint8_t* kaynak, std::size_t uzunluk) noexcept : p_(kaynak), n_(uzunluk) {}

    std::uint8_t u8() noexcept { return static_cast<std::uint8_t>(oku_(1)); }
    std::uint16_t u16() noexcept { return static_cast<std::uint16_t>(oku_(2)); }
    std::uint32_t u32() noexcept { return static_cast<std::uint32_t>(oku_(4)); }
    std::uint64_t u64() noexcept { return oku_(8); }
    std::int32_t i32() noexcept { return static_cast<std::int32_t>(static_cast<std::uint32_t>(oku_(4))); }
    std::int64_t i64() noexcept { return static_cast<std::int64_t>(oku_(8)); }
    float f32() noexcept
    {
        const auto b = static_cast<std::uint32_t>(oku_(4));
        float v = 0;
        std::memcpy(&v, &b, sizeof v);
        return v;
    }
    double f64() noexcept
    {
        const std::uint64_t b = oku_(8);
        double v = 0;
        std::memcpy(&v, &b, sizeof v);
        return v;
    }
    // n bayt ilerler ve başlangıcını döndürür; yetmezse nullptr (ve tamam() false).
    const std::uint8_t* baytlar(std::uint64_t n) noexcept
    {
        if (!tamam_ || n > n_ - k_) {
            tamam_ = false;
            return nullptr;
        }
        const std::uint8_t* r = p_ + k_;
        k_ += static_cast<std::size_t>(n);
        return r;
    }
    bool tamam() const noexcept { return tamam_; }
    std::size_t kalan() const noexcept { return n_ - k_; }
    std::size_t konum() const noexcept { return k_; }

private:
    std::uint64_t oku_(std::size_t bayt) noexcept
    {
        if (!tamam_ || bayt > n_ - k_) {
            tamam_ = false;
            return 0;
        }
        std::uint64_t v = 0;
        for (std::size_t i = 0; i < bayt; ++i) {
            v |= static_cast<std::uint64_t>(p_[k_ + i]) << (8u * i);
        }
        k_ += bayt;
        return v;
    }
    const std::uint8_t* p_;
    std::size_t n_;
    std::size_t k_ = 0;
    bool tamam_ = true;
};

// A3 §3.2: 24 baytlık çerçeve başlığı.
struct CerceveBasligi {
    std::int64_t t_ns = 0;
    std::uint32_t sira = 0;
    std::uint32_t yuk_uzunlugu = 0;
    std::uint16_t kanal_id = 0;
    std::uint16_t kanal_surumu = 0;
    std::uint16_t bayraklar = 0;
};

inline constexpr std::size_t kBaslikBayt = 24;
inline constexpr std::uint32_t kYukAzamiBayt = 65536;  // A3-K21

namespace bayrak {
inline constexpr std::uint16_t kaynak_kayit = 1u << 0;
inline constexpr std::uint16_t ozet = 1u << 1;
inline constexpr std::uint16_t oncesinde_dusme = 1u << 2;
inline constexpr std::uint16_t yer_gercegi = 1u << 3;
}  // namespace bayrak

inline void baslik_yaz(const CerceveBasligi& b, std::uint8_t* hedef) noexcept
{
    Yazici w(hedef, kBaslikBayt);
    w.i64(b.t_ns);
    w.u32(b.sira);
    w.u32(b.yuk_uzunlugu);
    w.u16(b.kanal_id);
    w.u16(b.kanal_surumu);
    w.u16(b.bayraklar);
    w.u16(0);  // ayrilmis (A3-K21)
}

// ---- Sınama dökümü yardımcıları (üretilen json_dok işlevleri kullanır) ----
template <typename T>
void dok_tam(std::string& c, T v)
{
    c += std::to_string(v);
}
template <typename T>
void dok_64(std::string& c, T v)
{
    c.push_back('"');
    c += std::to_string(v);
    c.push_back('"');
}
inline void dok_kayan(std::string& c, double v)
{
    if (std::isnan(v)) {
        c += "\"NaN\"";
        return;
    }
    if (std::isinf(v)) {
        c += v > 0 ? "\"Infinity\"" : "\"-Infinity\"";
        return;
    }
    char t[64];
    const auto r = std::to_chars(t, t + sizeof t, v);
    c.append(t, r.ptr);
}
inline void dok_kayan(std::string& c, float v) { dok_kayan(c, static_cast<double>(v)); }
template <typename T>
void dok_kayan_yok(std::string& c, T v)
{
    if (std::isnan(v)) {
        c += "null";
    } else {
        dok_kayan(c, v);
    }
}
inline void dok_metin(std::string& c, std::string_view s)
{
    static constexpr char kHex[] = "0123456789abcdef";
    c.push_back('"');
    for (const char ch : s) {
        const auto u = static_cast<unsigned char>(ch);
        if (ch == '"' || ch == '\\') {
            c.push_back('\\');
            c.push_back(ch);
        } else if (u < 0x20u) {
            c += "\\u00";
            c.push_back(kHex[u >> 4u]);
            c.push_back(kHex[u & 0x0Fu]);
        } else {
            c.push_back(ch);
        }
    }
    c.push_back('"');
}

inline bool baslik_oku(const std::uint8_t* kaynak, std::size_t n, CerceveBasligi& b) noexcept
{
    Okuyucu r(kaynak, n);
    b.t_ns = r.i64();
    b.sira = r.u32();
    b.yuk_uzunlugu = r.u32();
    b.kanal_id = r.u16();
    b.kanal_surumu = r.u16();
    b.bayraklar = r.u16();
    (void)r.u16();
    return r.tamam();
}

}  // namespace ida::a3
