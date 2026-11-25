// dusme.hpp — üreticinin düşme sayaçları (A6-K06, K30).
//
// Her (üretici, kanal, neden) için bir yuva. Üretici (tek iş parçacığı) düşen her iletide yuvayı
// kilitsiz günceller; kayıt yazıcısı yuvayı sıra numarasıyla (seqlock) tutarlı okur ve `kayit/eksik`
// kaydına çevirir. Üretici hiçbir koşulda beklemez; yazıcı tutarsız okuma görürse yeniden dener.
//
// Sayım kümülatiftir ve hiç sıfırlanmaz: yazıcı son bildirdiği değerle farkı bildirir, bu yüzden
// hiçbir düşme sayım dışı kalamaz. Aralık (`ilk`/`son`): üretici, yazıcının onayladığı
// (`onay` = bildirilen kümülatif sayı) noktadan sonraki ilk düşmede yeni bir aralık başlatır.
// Yazıcının okuması ile onayı arasına düşen bir ileti eski aralığa katılır; o durumda bir sonraki
// kaydın `ilk` değeri gerçeğinden erken olabilir (aralık hiçbir zaman gerçeğinden dar olmaz).
#pragma once

#include <atomic>
#include <cstdint>

namespace ida::kayit::ic {

// Üretici yolu kilitsizdir: atomikler kilide dönüşmemelidir (A6-K06).
static_assert(std::atomic<std::uint64_t>::is_always_lock_free && std::atomic<std::int64_t>::is_always_lock_free &&
              std::atomic<std::uint32_t>::is_always_lock_free && std::atomic<bool>::is_always_lock_free);

struct DusmeKesiti {
    std::uint64_t sayi = 0;  // kümülatif
    std::int64_t ilk_t = 0;
    std::int64_t son_t = 0;
    std::uint64_t ilk_adim = 0;
    std::uint64_t son_adim = 0;
};

class DusmeYuvasi {
public:
    // ---- Üretici ----
    void dusur(std::int64_t sim_t_ns, std::uint64_t adim) noexcept
    {
        const std::uint64_t sayi = sayi_.load(std::memory_order_relaxed);
        const bool yeni_aralik = sayi == 0 || onay_.load(std::memory_order_seq_cst) == sayi;
        const std::uint32_t s = sira_.load(std::memory_order_relaxed);
        sira_.store(s + 1u, std::memory_order_seq_cst);  // tek: yazım sürüyor
        if (yeni_aralik) {
            ilk_t_.store(sim_t_ns, std::memory_order_seq_cst);
            ilk_adim_.store(adim, std::memory_order_seq_cst);
        }
        son_t_.store(sim_t_ns, std::memory_order_seq_cst);
        son_adim_.store(adim, std::memory_order_seq_cst);
        sayi_.store(sayi + 1u, std::memory_order_seq_cst);
        sira_.store(s + 2u, std::memory_order_seq_cst);  // çift: tutarlı
    }

    // ---- Yazıcı ----
    // Tutarlı kesit; üretici o anda yazıyorsa false (sonra yeniden denenir).
    bool oku(DusmeKesiti& k) const noexcept
    {
        for (int deneme = 0; deneme < 64; ++deneme) {
            const std::uint32_t s1 = sira_.load(std::memory_order_seq_cst);
            if ((s1 & 1u) != 0) {
                continue;
            }
            k.sayi = sayi_.load(std::memory_order_seq_cst);
            k.ilk_t = ilk_t_.load(std::memory_order_seq_cst);
            k.son_t = son_t_.load(std::memory_order_seq_cst);
            k.ilk_adim = ilk_adim_.load(std::memory_order_seq_cst);
            k.son_adim = son_adim_.load(std::memory_order_seq_cst);
            if (sira_.load(std::memory_order_seq_cst) == s1) {
                return true;
            }
        }
        return false;
    }
    // Yazıcının `sayi`'ya kadarki düşmeleri bildirdiğini üreticiye duyurur.
    void onayla(std::uint64_t sayi) noexcept { onay_.store(sayi, std::memory_order_seq_cst); }
    // Hızlı yoklama: kümülatif sayı (tutarlılık gerekmez).
    std::uint64_t sayi() const noexcept { return sayi_.load(std::memory_order_acquire); }

private:
    std::atomic<std::uint32_t> sira_{0};
    std::atomic<std::uint64_t> sayi_{0};
    std::atomic<std::int64_t> ilk_t_{0};
    std::atomic<std::int64_t> son_t_{0};
    std::atomic<std::uint64_t> ilk_adim_{0};
    std::atomic<std::uint64_t> son_adim_{0};
    std::atomic<std::uint64_t> onay_{0};
};

}  // namespace ida::kayit::ic
