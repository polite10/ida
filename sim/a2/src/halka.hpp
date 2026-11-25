// halka.hpp — tek üretici–tek tüketici bayt halkası (dünya → A2 G/Ç iş parçacığı).
//
// Üretici (dünya iş parçacığı) beklemez, kilit almaz, bellek ayırmaz: yer yoksa `ayir` nullptr
// döner ve kayıt düşer. Kayıt düzeni: 8 bayta hizalı [u32 boy][u32 0][yük…]; halkanın sonunda
// kayıt sığmazsa "sarma" işareti (boy = 0xFFFFFFFF) yazılır ve kayıt başa konur.
//
// `pay`: `ayir` yalnız kayıttan sonra en az `pay` bayt boş kalacaksa yer verir. Atılabilir kayıtlar
// (takvimli durum) payla, atılamayanlar (olay, takvim dışı durum, küme) paysız yazılır; böylece
// durum trafiği olaylara ayrılan yeri tüketemez.
#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>

namespace ida::a2::ic {

class BaytHalkasi {
public:
    // kapasite: 2'nin kuvvetine yuvarlanır; en az 4096.
    explicit BaytHalkasi(std::size_t kapasite)
    {
        std::size_t k = 4096;
        while (k < kapasite) {
            k <<= 1u;
        }
        kap_ = k;
        tampon_ = std::make_unique<std::uint8_t[]>(k);
    }

    std::size_t kapasite() const noexcept { return kap_; }

    // ---- Üretici ----
    std::uint8_t* ayir(std::size_t yuk, std::size_t pay) noexcept
    {
        const std::size_t kayit = hizala_(8 + yuk);
        if (kayit > kap_ / 2) {
            return nullptr;
        }
        const std::uint64_t bas = bas_.load(std::memory_order_relaxed);
        const std::uint64_t son = son_.load(std::memory_order_acquire);
        const std::size_t konum = static_cast<std::size_t>(bas & (kap_ - 1));
        const std::size_t sona_kalan = kap_ - konum;
        std::size_t gereken = kayit;
        if (sona_kalan < kayit) {
            gereken += sona_kalan;  // sarma işareti + baştan
        }
        const std::size_t bos = kap_ - static_cast<std::size_t>(bas - son);
        if (bos < gereken || bos - gereken < pay) {
            return nullptr;
        }
        std::size_t yaz = konum;
        if (sona_kalan < kayit) {
            yaz32_(konum, 0xFFFFFFFFu);
            bekleyen_ek_ = sona_kalan;
            yaz = 0;
        } else {
            bekleyen_ek_ = 0;
        }
        yaz32_(yaz, static_cast<std::uint32_t>(yuk));
        yaz32_(yaz + 4, 0);
        bekleyen_kayit_ = kayit;
        return tampon_.get() + yaz + 8;
    }

    void yayinla() noexcept
    {
        const std::uint64_t bas = bas_.load(std::memory_order_relaxed);
        bas_.store(bas + bekleyen_ek_ + bekleyen_kayit_, std::memory_order_release);
    }

    // ---- Tüketici ----
    // Sıradaki kaydın yükü ve boyu; kayıt yoksa nullptr. Ardından `birak` çağrılır.
    const std::uint8_t* oku(std::size_t& yuk) noexcept
    {
        std::uint64_t son = son_.load(std::memory_order_relaxed);
        const std::uint64_t bas = bas_.load(std::memory_order_acquire);
        if (son == bas) {
            return nullptr;
        }
        std::size_t konum = static_cast<std::size_t>(son & (kap_ - 1));
        std::uint32_t boy = oku32_(konum);
        if (boy == 0xFFFFFFFFu) {
            son += kap_ - konum;
            son_.store(son, std::memory_order_release);
            if (son == bas) {
                return nullptr;
            }
            konum = 0;
            boy = oku32_(0);
        }
        yuk = boy;
        okunan_kayit_ = hizala_(8 + static_cast<std::size_t>(boy));
        return tampon_.get() + konum + 8;
    }

    void birak() noexcept
    {
        const std::uint64_t son = son_.load(std::memory_order_relaxed);
        son_.store(son + okunan_kayit_, std::memory_order_release);
    }

private:
    static std::size_t hizala_(std::size_t n) noexcept { return (n + 7u) & ~static_cast<std::size_t>(7u); }
    void yaz32_(std::size_t konum, std::uint32_t v) noexcept { std::memcpy(tampon_.get() + konum, &v, 4); }
    std::uint32_t oku32_(std::size_t konum) const noexcept
    {
        std::uint32_t v = 0;
        std::memcpy(&v, tampon_.get() + konum, 4);
        return v;
    }

    std::size_t kap_ = 0;
    std::unique_ptr<std::uint8_t[]> tampon_;
    std::atomic<std::uint64_t> bas_{0};  // üretici yazar
    std::size_t bekleyen_kayit_ = 0;     // üretici yerel
    std::size_t bekleyen_ek_ = 0;        // üretici yerel
    std::atomic<std::uint64_t> son_{0};  // tüketici yazar
    std::size_t okunan_kayit_ = 0;       // tüketici yerel
};

}  // namespace ida::a2::ic
