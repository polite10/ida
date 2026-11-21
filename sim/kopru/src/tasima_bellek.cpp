// Bellek içi taşıma: aynı süreçte iki uç (tasima.hpp).
#include "ida/kopru/tasima.hpp"

#include <chrono>
#include <thread>
#include <condition_variable>
#include <deque>
#include <mutex>

namespace ida::kopru {

bool Tasima::yaz(const std::uint8_t* p, std::size_t n, std::int64_t sure_us)
{
    const auto son = std::chrono::steady_clock::now() + std::chrono::microseconds(sure_us);
    std::size_t yazilan = 0;
    while (yazilan < n) {
        const long long k = yaz_biraz(p + yazilan, n - yazilan);
        if (k < 0) {
            return false;
        }
        yazilan += static_cast<std::size_t>(k);
        if (yazilan < n) {
            if (std::chrono::steady_clock::now() >= son) {
                return false;
            }
            if (k == 0) {
                std::this_thread::sleep_for(std::chrono::microseconds(100));
            }
        }
    }
    return true;
}

namespace {

struct Ortak {
    std::mutex m;
    std::condition_variable cv;
    std::deque<std::uint8_t> yon[2];   // yon[i]: i ucunun okuyacağı baytlar
    bool kapali = false;
};

class BellekUcu final : public Tasima {
public:
    BellekUcu(std::shared_ptr<Ortak> o, int ben) : o_(std::move(o)), ben_(ben) {}
    ~BellekUcu() override { kapat(); }

    long long yaz_biraz(const std::uint8_t* p, std::size_t n) override
    {
        {
            const std::lock_guard<std::mutex> k(o_->m);
            if (o_->kapali) {
                return -1;
            }
            auto& q = o_->yon[1 - ben_];
            q.insert(q.end(), p, p + n);
        }
        o_->cv.notify_all();
        return static_cast<long long>(n);
    }

    OkumaSonucu oku(std::uint8_t* p, std::size_t n, std::int64_t bekle_us, std::size_t& okunan) override
    {
        okunan = 0;
        std::unique_lock<std::mutex> k(o_->m);
        auto& q = o_->yon[ben_];
        if (q.empty() && !o_->kapali && bekle_us > 0) {
            o_->cv.wait_for(k, std::chrono::microseconds(bekle_us),
                            [&] { return !q.empty() || o_->kapali; });
        }
        if (q.empty()) {
            return o_->kapali ? OkumaSonucu::kapandi : OkumaSonucu::zaman_asimi;
        }
        while (okunan < n && !q.empty()) {
            p[okunan++] = q.front();
            q.pop_front();
        }
        return OkumaSonucu::tamam;
    }

    void giris_bosalt() override {}
    bool acik() const override
    {
        const std::lock_guard<std::mutex> k(o_->m);
        return !o_->kapali;
    }
    void kapat() override
    {
        {
            const std::lock_guard<std::mutex> k(o_->m);
            o_->kapali = true;
        }
        o_->cv.notify_all();
    }
    std::string ad() const override { return ben_ == 0 ? "bellek:0" : "bellek:1"; }

private:
    std::shared_ptr<Ortak> o_;
    int ben_;
};

}  // namespace

std::pair<std::unique_ptr<Tasima>, std::unique_ptr<Tasima>> bellek_cifti()
{
    auto o = std::make_shared<Ortak>();
    return {std::make_unique<BellekUcu>(o, 0), std::make_unique<BellekUcu>(o, 1)};
}

}  // namespace ida::kopru
