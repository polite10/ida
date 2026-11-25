// sinama_destek.hpp — üretilen sınama kodunun (sinama_uretim.cpp) kullandığı örnek değer üreticisi
// ve yük yapılarının gösterdiği belleği tutan depo. Değerler tohumdan belirlenir (belirlenimci).
#pragma once

#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace ida::kayit::test {

class Rastgele {
public:
    explicit Rastgele(std::uint64_t tohum) noexcept : s_(tohum + 0x9E3779B97F4A7C15ull) {}

    // splitmix64
    std::uint64_t ham() noexcept
    {
        std::uint64_t z = (s_ += 0x9E3779B97F4A7C15ull);
        z = (z ^ (z >> 30u)) * 0xBF58476D1CE4E5B9ull;
        z = (z ^ (z >> 27u)) * 0x94D049BB133111EBull;
        return z ^ (z >> 31u);
    }
    std::uint64_t alt(std::uint64_t n) noexcept { return n == 0 ? 0 : ham() % n; }

    // Tipin bütün aralığından; arada uç değerler.
    template <class T>
    T tam() noexcept
    {
        switch (alt(16)) {
        case 0:
            return std::numeric_limits<T>::max();
        case 1:
            return std::numeric_limits<T>::min();
        case 2:
            return static_cast<T>(0);
        default:
            return static_cast<T>(ham());
        }
    }
    // Arada "yok" değeri.
    template <class T>
    T tam_yoklu(T yok) noexcept
    {
        return alt(4) == 0 ? yok : tam<T>();
    }
    // Sonlu değerler; `yok_nan` ise arada NaN ("yok").
    template <class T>
    T kayan(bool yok_nan) noexcept
    {
        const std::uint64_t secim = alt(8);
        if (yok_nan && secim == 0) {
            return std::numeric_limits<T>::quiet_NaN();
        }
        if (secim == 1) {
            return static_cast<T>(0);
        }
        const auto pay = static_cast<std::int64_t>(alt(2000001)) - 1000000;
        const auto bolen = static_cast<T>(1u << alt(12));
        return static_cast<T>(pay) / bolen;
    }
    // Dizi öğe sayısı: 0…min(azami, 4).
    std::size_t adet(std::uint32_t azami) noexcept
    {
        return static_cast<std::size_t>(alt(std::uint64_t{azami < 4u ? azami : 4u} + 1u));
    }

private:
    std::uint64_t s_;
};

class Depo {
public:
    // En çok min(azami, 40) baytlık metin: ASCII ve arada çok baytlı UTF-8.
    std::string_view metin(Rastgele& r, std::uint32_t azami)
    {
        static constexpr std::string_view kParcalar[] = {"a",  "Z",  "0", "_",        "/",        " ",
                                                         "\"", "\\", ".", "\xc4\x9f", "\xc3\xbc", "\xc5\x9f"};
        const std::size_t sinir = azami < 40u ? azami : 40u;
        auto m = std::make_shared<std::string>();
        const std::size_t hedef = static_cast<std::size_t>(r.alt(sinir + 1u));
        while (m->size() < hedef) {
            const std::string_view p = kParcalar[r.alt(std::size(kParcalar))];
            if (m->size() + p.size() > hedef) {
                break;
            }
            m->append(p);
        }
        tutulan_.push_back(m);
        return *m;
    }
    // En çok min(azami, 64) bayt.
    std::span<const std::uint8_t> baytlar(Rastgele& r, std::uint32_t azami)
    {
        const std::size_t sinir = azami < 64u ? azami : 64u;
        auto v = std::make_shared<std::vector<std::uint8_t>>(static_cast<std::size_t>(r.alt(sinir + 1u)));
        for (auto& b : *v) {
            b = static_cast<std::uint8_t>(r.ham());
        }
        tutulan_.push_back(v);
        return *v;
    }
    template <class T>
    std::span<const T> tut(std::vector<T>&& v)
    {
        auto p = std::make_shared<std::vector<T>>(std::move(v));
        tutulan_.push_back(p);
        return *p;
    }

private:
    std::vector<std::shared_ptr<void>> tutulan_;
};

}  // namespace ida::kayit::test
