// son_deger.hpp — tek yazar–tek okur "son değer" deposu: üç yuva, yazar beklemez.
//
// Yazar (dünya iş parçacığı) her yayında, yayındaki yuva ve okurun tutturduğu yuva dışındaki bir
// yuvayı doldurur ve onu yayına alır; kilit yok, yeniden deneme yok, bellek ayırma yok. Okur (G/Ç
// iş parçacığı) yayındaki yuvayı "tutturur", okur ve bırakır; tutturduğu yuvaya yazar dokunmaz.
//
// Doğruluk (seq_cst sırası; Dekker deseni). Okur: i = aktif; igne = i; aktif hâlâ i ise i'yi okur.
// Yazar: a = aktif; p = igne; s ∉ {a, p} yuvasına yazar; aktif = s. Okur r'yi okurken yazarın r'ye
// yazması için r ≠ a (yazar r'den ayrılmış: aktif = a yazımı t_f) ve p ≠ r (igne okuması t_l,
// t_f < t_l) gerekir. Okurun doğrulaması (t_v) aktif'i r gördüyse t_v < t_f; igne = r yazımı
// t_p < t_v. Öyleyse t_p < t_l ve yazar igne'yi r görür: çelişki. Üç yuva, {a, p} dışında her zaman
// bir yuva kalmasını sağlar.
#pragma once

#include <atomic>
#include <memory>

namespace ida::a2::ic {

template <class T>
class SonDeger {
public:
    SonDeger() : yuva_(std::make_unique<T[]>(3)) {}

    // ---- Yazar ----
    // Doldurulacak yuva. İçeriği iki yayın önceki değerdir (bayat); yazar gerekeni yeniler.
    T& yazilacak() noexcept
    {
        const int a = aktif_.load(std::memory_order_relaxed);
        const int p = igne_.load(std::memory_order_seq_cst);
        int s = 0;
        while (s == a || s == p) {
            ++s;
        }
        yaz_ = s;
        return yuva_[static_cast<std::size_t>(s)];
    }
    // Yayındaki yuva (yazarın gözünden; yalnız okunur).
    const T& yayindaki() const noexcept
    {
        return yuva_[static_cast<std::size_t>(aktif_.load(std::memory_order_relaxed))];
    }
    void yayinla() noexcept { aktif_.store(yaz_, std::memory_order_seq_cst); }

    // ---- Okur ----
    const T& tuttur() noexcept
    {
        for (;;) {
            const int i = aktif_.load(std::memory_order_seq_cst);
            igne_.store(i, std::memory_order_seq_cst);
            if (aktif_.load(std::memory_order_seq_cst) == i) {
                return yuva_[static_cast<std::size_t>(i)];
            }
        }
    }
    void birak() noexcept { igne_.store(-1, std::memory_order_seq_cst); }

private:
    std::unique_ptr<T[]> yuva_;
    std::atomic<int> aktif_{0};
    std::atomic<int> igne_{-1};
    int yaz_ = 0;  // yazar yerel
};

}  // namespace ida::a2::ic
