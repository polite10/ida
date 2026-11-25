// kuyruk.hpp — bağlantı başına gönderme kuyruğu (A2-K26): bayt sınırlı, ileti sınırlarını bilir.
//
// Kuyruk sınıra ulaşınca gönderilmeye başlanmamış takvimli `durum` iletileri atılabilir; gönderimi
// başlamış ileti (kısmen yazılmış baş) hiçbir zaman atılmaz, yoksa karşı tarafta çerçeve bozulur.
// Yalnız G/Ç iş parçacığında kullanılır.
#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <deque>
#include <span>
#include <vector>

namespace ida::a2::ic {

class GondermeKuyrugu {
public:
    explicit GondermeKuyrugu(std::size_t sinir) : sinir_(sinir) {}

    std::size_t sinir() const noexcept { return sinir_; }
    std::size_t bayt() const noexcept { return t_.size() - bas_; }
    bool bos() const noexcept { return bayt() == 0; }
    std::size_t atilabilir_sayisi() const noexcept { return atilabilir_; }

    // İletiyi ekler; sınırı aşacaksa eklemez ve false döner. `sira`: iletinin başlığındaki sıra
    // (atılan iletilerin sırasını bildirmek için saklanır). `sinirsiz`: kapanış iletisi (ret, veda)
    // için sınır denetlenmez.
    bool ekle(const std::uint8_t* p, std::size_t n, std::uint32_t sira, bool atilabilir, bool sinirsiz = false)
    {
        if (n == 0) {
            return true;
        }
        if (!sinirsiz && bayt() + n > sinir_) {
            return false;
        }
        if (bas_ > 0 && (bas_ == t_.size() || (bas_ >= 65536 && bas_ * 2 >= t_.size()))) {
            t_.erase(t_.begin(), t_.begin() + static_cast<std::ptrdiff_t>(bas_));  // gönderilmiş baştakileri at
            bas_ = 0;
        }
        const std::size_t gereken = t_.size() + n;
        if (t_.capacity() < gereken) {
            // Sığa sınırın biraz üstünde tutulur (vector'ün ikiye katlaması 2 × sınıra çıkarmasın).
            std::size_t yeni = t_.capacity() * 2;
            const std::size_t tavan = sinir_ + 65536;
            if (yeni > tavan) {
                yeni = tavan;
            }
            if (yeni < gereken) {
                yeni = gereken;
            }
            t_.reserve(yeni);
        }
        t_.insert(t_.end(), p, p + n);
        il_.push_back(Ileti{static_cast<std::uint32_t>(n), sira, atilabilir});
        if (atilabilir) {
            ++atilabilir_;
        }
        return true;
    }

    // Gönderilmeye başlanmamış atılabilir iletileri atar; atılan ileti sayısını döndürür.
    std::size_t atilabilirleri_at()
    {
        std::uint32_t ilk = 0;
        return at_(false, ilk);
    }
    // Gönderilmeye başlanmamış bütün iletileri atar; `ilk_sira` atılan ilk iletinin sırasıdır
    // (hiçbiri atılmadıysa değişmez).
    std::size_t hepsini_at(std::uint32_t& ilk_sira) { return at_(true, ilk_sira); }

    // Gönderilecek ardışık baytlar.
    std::span<const std::uint8_t> on() const noexcept { return {t_.data() + bas_, t_.size() - bas_}; }

    void tuket(std::size_t n) noexcept
    {
        bas_ += n;
        while (n > 0 && !il_.empty()) {
            const std::size_t kalan = il_.front().boy - bas_gonderilen_;
            if (n >= kalan) {
                n -= kalan;
                bas_gonderilen_ = 0;
                if (il_.front().atilabilir) {
                    --atilabilir_;
                }
                il_.pop_front();
            } else {
                bas_gonderilen_ += n;
                n = 0;
            }
        }
        if (bas_ == t_.size()) {
            t_.clear();
            bas_ = 0;
        }
    }

    // Kalan (gönderilmemiş) baytları verir ve kuyruğu boşaltır.
    std::vector<std::uint8_t> kalanlari_al()
    {
        std::vector<std::uint8_t> k(t_.begin() + static_cast<std::ptrdiff_t>(bas_), t_.end());
        temizle();
        return k;
    }

    void temizle() noexcept
    {
        t_.clear();
        il_.clear();
        bas_ = 0;
        bas_gonderilen_ = 0;
        atilabilir_ = 0;
    }

private:
    struct Ileti {
        std::uint32_t boy;
        std::uint32_t sira;
        bool atilabilir;
    };

    std::size_t at_(bool hepsi, std::uint32_t& ilk_sira)
    {
        std::size_t atilan = 0;
        std::size_t oku = bas_;
        std::size_t yaz = bas_;
        std::deque<Ileti> kalan;
        bool ilk = true;
        for (const Ileti& m : il_) {
            const bool kismi = ilk && bas_gonderilen_ > 0;
            const std::size_t boy = kismi ? m.boy - bas_gonderilen_ : m.boy;
            const bool tut = kismi || (!hepsi && !m.atilabilir);
            if (tut) {
                if (yaz != oku) {
                    std::memmove(t_.data() + yaz, t_.data() + oku, boy);
                }
                yaz += boy;
                kalan.push_back(m);
            } else {
                if (atilan == 0) {
                    ilk_sira = m.sira;
                }
                ++atilan;
                if (m.atilabilir) {
                    --atilabilir_;
                }
            }
            oku += boy;
            ilk = false;
        }
        t_.resize(yaz);
        il_.swap(kalan);
        if (bas_ == t_.size()) {
            t_.clear();
            bas_ = 0;
        }
        return atilan;
    }

    std::size_t sinir_;
    std::vector<std::uint8_t> t_;
    std::deque<Ileti> il_;
    std::size_t bas_ = 0;             // t_ içinde gönderilmemiş ilk bayt
    std::size_t bas_gonderilen_ = 0;  // baştaki iletinin gönderilmiş baytı
    std::size_t atilabilir_ = 0;
};

}  // namespace ida::a2::ic
