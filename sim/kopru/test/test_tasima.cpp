// Taşıma sınamaları: bellek, TCP (A1-K97) ve sözde uçbirimle seri port yolu (A1-K01).
#include "test_destek.hpp"

#include <chrono>
#include <thread>

#if !defined(_WIN32)
#include <cstdlib>
#include <fcntl.h>
#include <unistd.h>
#endif

using namespace ida::kopru;
using namespace ida::kopru::test;

namespace {

// a'dan b'ye n bayt (belirli desen) gönderir; b tamamını aynı sırayla almalı.
bool aktar(Tasima& a, Tasima& b, std::size_t n)
{
    std::vector<std::uint8_t> v(n);
    for (std::size_t i = 0; i < n; ++i) {
        v[i] = static_cast<std::uint8_t>((i * 131u + 7u) & 0xFFu);
    }
    std::thread yazici([&] { (void)a.yaz(v.data(), v.size()); });
    std::vector<std::uint8_t> al;
    std::uint8_t t[4096];
    int bos = 0;
    while (al.size() < n && bos < 200) {
        std::size_t k = 0;
        const OkumaSonucu r = b.oku(t, sizeof t, 10'000, k);
        if (r == OkumaSonucu::tamam) {
            al.insert(al.end(), t, t + k);
            bos = 0;
        } else if (r == OkumaSonucu::kapandi) {
            break;
        } else {
            ++bos;
        }
    }
    yazici.join();
    return al == v;
}

}  // namespace

int test_bellek_tasima()
{
    Denetci d;
    auto [a, b] = bellek_cifti();
    DOGRULA(d, aktar(*a, *b, 100'000), "bellek a → b");
    DOGRULA(d, aktar(*b, *a, 100'000), "bellek b → a");
    a->kapat();
    std::uint8_t t[8];
    std::size_t n = 0;
    DOGRULA(d, b->oku(t, sizeof t, 0, n) == OkumaSonucu::kapandi, "kapanış görülür");
    return d.hata;
}

int test_tcp_tasima()
{
    Denetci d;
    std::string hata;
    auto din = tcp_dinle(0, hata);
    DOGRULA(d, din != nullptr, "dinleyici: " + hata);
    if (!din) {
        return d.hata;
    }
    std::unique_ptr<Tasima> istemci;
    std::thread baglan([&] {
        std::string h2;
        istemci = tcp_baglan(din->port(), h2);
    });
    auto sunucu = din->kabul(2'000'000, hata);
    baglan.join();
    DOGRULA(d, sunucu != nullptr && istemci != nullptr, "bağlantı kuruldu");
    if (!sunucu || !istemci) {
        return d.hata;
    }
    DOGRULA(d, aktar(*sunucu, *istemci, 1'000'000), "TCP sunucu → istemci 1 MB");
    DOGRULA(d, aktar(*istemci, *sunucu, 1'000'000), "TCP istemci → sunucu 1 MB");
    istemci->kapat();
    std::uint8_t t[8];
    std::size_t n = 0;
    OkumaSonucu r = OkumaSonucu::zaman_asimi;
    for (int i = 0; i < 100 && r == OkumaSonucu::zaman_asimi; ++i) {
        r = sunucu->oku(t, sizeof t, 10'000, n);
    }
    DOGRULA(d, r == OkumaSonucu::kapandi, "karşı ucun kapanışı görülür (A1-K99)");
    return d.hata;
}

int test_pty_tasima()
{
    Denetci d;
#if defined(_WIN32)
    std::printf("  (Windows: sözde uçbirim yok; atlandı)\n");
#else
    const int ana = posix_openpt(O_RDWR | O_NOCTTY);
    DOGRULA(d, ana >= 0 && grantpt(ana) == 0 && unlockpt(ana) == 0, "posix_openpt");
    if (ana < 0) {
        return d.hata;
    }
    const char* ad = ptsname(ana);
    std::string hata;
    SeriAyar sa;
    sa.hiz_zorunlu = false;   // sözde uçbirim 3 000 000 baud'u kabul etmeyebilir
    auto seri = seri_ac(ad != nullptr ? ad : "", sa, hata);
    DOGRULA(d, seri != nullptr, "seri_ac: " + hata);
    if (!seri) {
        ::close(ana);
        return d.hata;
    }
    if (!hata.empty()) {
        std::printf("  not: %s\n", hata.c_str());
    }
    // Ana ucu Tasima olarak sarmak için seri_ac'ın POSIX yolunu yeniden kullanamayız; ham fd ile.
    struct AnaUc final : Tasima {
        int fd;
        explicit AnaUc(int f) : fd(f) {}
        bool yaz(const std::uint8_t* p, std::size_t n) override
        {
            std::size_t y = 0;
            while (y < n) {
                const ssize_t k = ::write(fd, p + y, n - y);
                if (k > 0) {
                    y += static_cast<std::size_t>(k);
                } else {
                    std::this_thread::sleep_for(std::chrono::microseconds(100));
                }
            }
            return true;
        }
        OkumaSonucu oku(std::uint8_t* p, std::size_t n, std::int64_t bekle_us, std::size_t& okunan) override
        {
            okunan = 0;
            const auto son = std::chrono::steady_clock::now() + std::chrono::microseconds(bekle_us);
            do {
                const ssize_t k = ::read(fd, p, n);
                if (k > 0) {
                    okunan = static_cast<std::size_t>(k);
                    return OkumaSonucu::tamam;
                }
                std::this_thread::sleep_for(std::chrono::microseconds(100));
            } while (std::chrono::steady_clock::now() < son);
            return OkumaSonucu::zaman_asimi;
        }
        void giris_bosalt() override {}
        bool acik() const override { return true; }
        void kapat() override {}
        std::string ad() const override { return "pty-ana"; }
    };
    (void)fcntl(ana, F_SETFL, fcntl(ana, F_GETFL) | O_NONBLOCK);
    AnaUc au(ana);
    DOGRULA(d, aktar(*seri, au, 50'000), "seri → pty ana");
    DOGRULA(d, aktar(au, *seri, 50'000), "pty ana → seri (ham kip: bayt çevirisi yok)");
    seri->kapat();
    ::close(ana);
#endif
    return d.hata;
}
