// Taşıma sınamaları: bellek, TCP (A1-K97) ve sözde uçbirimle seri port yolu (A1-K01).
#include "test_destek.hpp"
#include "zamanlama_olcutu.hpp"   // W4-K24

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdio>
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

// W4-K26 (3): düzgün kapanışın süre sınırı mutlak son tarihtir; karşı uç durmadan veri gönderse
// de (okuma hiç beklemeye düşmese de) kapat_duzgun sınırda döner. W4-K26 (2): FIN'den sonra
// yazma hatası okunmamış veriyi yok etmez.
int test_tcp_kapat_duzgun_sinirli()
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
    std::atomic<bool> dur{false};
    std::atomic<std::uint64_t> gonderilen{0};
    std::thread sel([&] {
        std::vector<std::uint8_t> v(65536, 0x5A);
        while (!dur.load()) {
            const long long k = istemci->yaz_biraz(v.data(), v.size());
            if (k < 0) {
                break;
            }
            gonderilen += static_cast<std::uint64_t>(k);
        }
    });
    // Sel başlasın: sunucunun alım tamponu dolu olsun.
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    const auto t0 = std::chrono::steady_clock::now();
    sunucu->kapat_duzgun(100'000);   // sınır 100 ms
    const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - t0).count();
    dur = true;
    sel.join();
    // İşlevsel ölçüt: kapanış döndü (asılmadı) ve uç kapandı.
    DOGRULA(d, !sunucu->acik(), "sürekli veri gönderen karşı uçla kapat_duzgun döndü ve uç kapandı (" +
                                    std::to_string(gonderilen.load()) + " B gönderildi)");
    // ZAMANLAMA (W4-K24): üst sınır duvar saatine bağlı.
    DOGRULA(d, ida::test::zamanlama(ms < 100 + 400, "kapat_duzgun.sinir", std::to_string(ms) + " ms (sınır 100 + pay 400)"),
            "kapat_duzgun süre sınırında döndü (zamanlama)");
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
        long long yaz_biraz(const std::uint8_t* p, std::size_t n) override
        {
            const ssize_t k = ::write(fd, p, n);
            return k > 0 ? static_cast<long long>(k) : 0;
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

// sol #7: yazım süresiz tutmaz. Okumayan bir TCP karşı ucuna 64 MiB yazmak sınırlı sürede
// (Winsock engellemesiz send: 1..n bayt ya da WSAEWOULDBLOCK; Microsoft "send function")
// başarısız döner (eski döngü: 1 s beklemelerle sonsuza dek); yaz_biraz hiçbir zaman beklemez;
// kısmi yazımlar konum korunarak birleşir.
int test_yazma_sinirli()
{
    Denetci d;
    std::string hata;
    auto din = tcp_dinle(0, hata);
    DOGRULA(d, din != nullptr, "dinleyici " + hata);
    if (!din) {
        return d.hata;
    }
    auto istemci = tcp_baglan(din->port(), hata);
    auto sunucu = din->kabul(1'000'000, hata);
    DOGRULA(d, istemci && sunucu, "bağlantı " + hata);
    if (!istemci || !sunucu) {
        return d.hata;
    }
    std::vector<std::uint8_t> v(64u << 20U, 0x5Au);
    const auto t0 = std::chrono::steady_clock::now();
    const bool ok = sunucu->yaz(v.data(), v.size(), 300'000);
    const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - t0).count();
    DOGRULA(d, !ok, "okumayan karşı uca yazım başarısız döndü");
    // ZAMANLAMA (W4-K24): üst sınır duvar saatine bağlı; başarısızlık işlevseldir (yukarıda).
    DOGRULA(d, ida::test::zamanlama(ms < 2000, "yazma_sinirli.tcp_sure", std::to_string(ms) + " ms (< 2000)"),
            "sınırlı sürede (zamanlama)");
    // Tampon doluyken yaz_biraz beklemeden 0 döner.
    const auto t1 = std::chrono::steady_clock::now();
    const long long k = sunucu->yaz_biraz(v.data(), 4096);
    const auto us = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - t1).count();
    DOGRULA(d, k == 0, "dolu tamponda yaz_biraz 0 (" + std::to_string(k) + ")");
    DOGRULA(d, ida::test::zamanlama(us < 50'000, "yazma_sinirli.yaz_biraz_beklemesiz", std::to_string(us) + " µs (< 50 000)"),
            "yaz_biraz beklemesiz (zamanlama)");

    // Platformdan bağımsız kısım (Windows CI'da düşen eski vaka): sahte uç, her çağrıda en çok
    // 3 bayt yazar; her ikinci çağrıda ilerlemez; `durgun_sonra` çağrıdan sonra hiç ilerlemez.
    struct Kisitli final : Tasima {
        std::vector<std::uint8_t> al;
        int sayac = 0;
        int sifir = 0;
        int durgun_sonra = -1;   // < 0: hep ilerler
        long long yaz_biraz(const std::uint8_t* p, std::size_t n) override
        {
            ++sayac;
            if ((durgun_sonra >= 0 && sayac > durgun_sonra) || sayac % 2 == 0) {
                ++sifir;
                return 0;
            }
            const std::size_t k = std::min<std::size_t>(n, 3);
            al.insert(al.end(), p, p + k);
            return static_cast<long long>(k);
        }
        OkumaSonucu oku(std::uint8_t*, std::size_t, std::int64_t, std::size_t& o) override
        {
            o = 0;
            return OkumaSonucu::zaman_asimi;
        }
        void giris_bosalt() override {}
        bool acik() const override { return true; }
        void kapat() override {}
        std::string ad() const override { return "kisitli"; }
    } ks;
    std::vector<std::uint8_t> w(1000);
    for (std::size_t j = 0; j < w.size(); ++j) {
        w[j] = static_cast<std::uint8_t>(j * 7u);
    }
    using Saat = std::chrono::steady_clock;
    auto ms_gecen = [](Saat::time_point t) {
        return std::chrono::duration_cast<std::chrono::milliseconds>(Saat::now() - t).count();
    };
    // (1) Ara sıra ilerlemeyen uç: kısmi yazımlar sırayla birleşir. Süresi, uykunun platform
    // çözünürlüğüne bağlı değildir (ilerlemesiz çağrıda önce sıra bırakılır).
    std::size_t yazilan = 0;
    auto t2 = Saat::now();
    const bool ok1 = ks.yaz(w.data(), w.size(), 2'000'000, &yazilan);
    std::printf("  tanı (1): sonuç %d, yazılan %zu/%zu, alınan %zu, çağrı %d (ilerlemesiz %d), %lld ms\n", ok1 ? 1 : 0,
                yazilan, w.size(), ks.al.size(), ks.sayac, ks.sifir, static_cast<long long>(ms_gecen(t2)));
    DOGRULA(d, ok1 && yazilan == w.size() && ks.al == w, "kısmi ve ilerlemesiz yazımlar birleşti");
    // (2) Durgunlaşan uç: süre sınırında false; yazılan önek bildirilir, veri bozulmaz ya da
    // yinelenmez; uç yeniden ilerleyince çağıran kaldığı konumdan sürdürür.
    Kisitli ks2;
    ks2.durgun_sonra = 20;   // 10 ilerlemeli çağrı: 30 bayt
    t2 = Saat::now();
    const bool ok2 = ks2.yaz(w.data(), w.size(), 200'000, &yazilan);
    const auto ms2 = ms_gecen(t2);
    std::printf("  tanı (2): sonuç %d, yazılan %zu, alınan %zu, çağrı %d (ilerlemesiz %d), %lld ms (sınır 200)\n",
                ok2 ? 1 : 0, yazilan, ks2.al.size(), ks2.sayac, ks2.sifir, static_cast<long long>(ms2));
    DOGRULA(d, !ok2 && ms2 >= 200, "durgun uca yazım süre sınırından önce dönmedi ve başarısız döndü");
    DOGRULA(d, ida::test::zamanlama(ms2 < 1500, "yazma_sinirli.durgun_sure", std::to_string(ms2) + " ms (< 1500)"),
            "durgun uca yazım süre sınırından kısa sürede döndü (zamanlama)");
    DOGRULA(d, yazilan == 30u && ks2.al.size() == 30u && std::equal(ks2.al.begin(), ks2.al.end(), w.begin()),
            "yazılan önek bildirildi; veri bozulmadı ve yinelenmedi");
    ks2.durgun_sonra = -1;
    std::size_t kalan_yazilan = 0;
    const bool ok3 = ks2.yaz(w.data() + yazilan, w.size() - yazilan, 2'000'000, &kalan_yazilan);
    DOGRULA(d, ok3 && ks2.al == w, "kaldığı konumdan sürdürüldü (" + std::to_string(ks2.al.size()) + " bayt)");
    return d.hata;
}

// sol #8: sözde uçbirimin ana ucu kapanınca (aygıt kalktı) seri uç okumada `kapandi` görür.
int test_aygit_kalkti()
{
    Denetci d;
#if defined(_WIN32)
    std::printf("  atlandı: sözde uçbirim Windows'ta yok\n");
#else
    std::string hata;
    SozdeUcbirim su = sozde_ucbirim_ac(hata);
    DOGRULA(d, su.ana != nullptr, "sözde uçbirim " + hata);
    if (!su.ana) {
        return d.hata;
    }
    SeriAyar sa;
    sa.hiz_zorunlu = false;
    auto seri = seri_ac(su.yol, sa, hata);
    DOGRULA(d, seri != nullptr, "seri_ac " + hata);
    if (!seri) {
        return d.hata;
    }
    std::uint8_t t[64];
    std::size_t n = 0;
    DOGRULA(d, seri->oku(t, sizeof t, 1000, n) == OkumaSonucu::zaman_asimi, "veri yokken zaman aşımı");
    const std::uint8_t m[3] = {1, 2, 3};
    DOGRULA(d, su.ana->yaz(m, 3), "ana uç yazdı");
    DOGRULA(d, seri->oku(t, sizeof t, 100'000, n) == OkumaSonucu::tamam && n == 3u, "veri okundu");
    su.ana.reset();   // ana uç kapanır: aygıt kalktı
    OkumaSonucu r = OkumaSonucu::zaman_asimi;
    for (int j = 0; j < 50 && r == OkumaSonucu::zaman_asimi; ++j) {
        r = seri->oku(t, sizeof t, 10'000, n);
    }
    DOGRULA(d, r == OkumaSonucu::kapandi, "aygıtın kalkması okumada görüldü");
    DOGRULA(d, !seri->acik(), "uç kapatıldı");
#endif
    return d.hata;
}

// W4-K28 sol-3 #3 (EMNİYET): seri portta uygulama tamponunun boşalması telin boşalması sayılmaz.
// cikis_bosalt, yaz_biraz'ın kabul ettiği baytlar karşı uca geçene dek bekler (sözde uçbirimde:
// ana uç okuyana dek); karşı uç okumuyorsa sınırlı sürede false döner. Denetim karşı ucun aldığı
// baytlarla yapılır (yalnız yaz_biraz dönüşleriyle değil).
int test_seri_cikis_bosalt()
{
    Denetci d;
#if defined(_WIN32)
    std::printf("  atlandı: sözde uçbirim Windows'ta yok\n");
#else
    std::string hata;
    SozdeUcbirim su = sozde_ucbirim_ac(hata);
    DOGRULA(d, su.ana != nullptr, "sözde uçbirim " + hata);
    if (!su.ana) {
        return d.hata;
    }
    SeriAyar sa;
    sa.hiz_zorunlu = false;
    auto seri = seri_ac(su.yol, sa, hata);
    DOGRULA(d, seri != nullptr, "seri_ac " + hata);
    if (!seri) {
        return d.hata;
    }
    std::vector<std::uint8_t> v(2000);
    for (std::size_t i = 0; i < v.size(); ++i) {
        v[i] = static_cast<std::uint8_t>((i * 37u + 11u) & 0xFFu);
    }
    auto hepsini_kabul_ettir = [&](const std::vector<std::uint8_t>& x) {
        std::size_t k = 0;
        for (int j = 0; j < 2000 && k < x.size(); ++j) {
            const long long y = seri->yaz_biraz(x.data() + k, x.size() - k);
            if (y < 0) {
                break;
            }
            k += static_cast<std::size_t>(y);
            if (y == 0) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        }
        return k;
    };
    // (a) Karşı uç okumuyor: kuyruk boşalmaz; sınırlı sürede false.
    const std::size_t k1 = hepsini_kabul_ettir(v);
    DOGRULA(d, k1 > 0u, "ön koşul: baytlar uygulama tamponundan çıktı (" + std::to_string(k1) + " B)");
    const auto t0 = std::chrono::steady_clock::now();
    const bool b1 = seri->cikis_bosalt(200'000);
    const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - t0).count();
    DOGRULA(d, !b1, "okumayan karşı uçta gönderim kuyruğu boşalmadı: false (eksik kapanış)");
    DOGRULA(d, ida::test::zamanlama(ms < 200 + 300, "cikis_bosalt.sinir", std::to_string(ms) + " ms (sınır 200 + pay 300)"),
            "cikis_bosalt süre sınırında döndü (zamanlama)");
    // Bekleyenleri karşı uçta al (temizle).
    std::vector<std::uint8_t> al;
    std::uint8_t t[4096];
    for (int j = 0; j < 100 && al.size() < k1; ++j) {
        std::size_t n = 0;
        if (su.ana->oku(t, sizeof t, 10'000, n) == OkumaSonucu::tamam) {
            al.insert(al.end(), t, t + n);
        }
    }
    DOGRULA(d, al.size() == k1 && std::equal(al.begin(), al.end(), v.begin()), "bekleyenler karşı uca bozulmadan geçti");
    // (b) Karşı uç geç ve yavaş okuyor: cikis_bosalt true döndüğünde karşı uç bütün baytları almış.
    std::atomic<std::size_t> alinan{0};
    std::atomic<bool> dur{false};
    std::vector<std::uint8_t> al2;
    std::thread okuyucu([&] {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        while (!dur.load()) {
            std::uint8_t u[64];
            std::size_t n = 0;
            if (su.ana->oku(u, sizeof u, 2'000, n) == OkumaSonucu::tamam) {
                al2.insert(al2.end(), u, u + n);
                alinan = al2.size();
            }
        }
    });
    const std::size_t k2 = hepsini_kabul_ettir(v);
    const bool b2 = seri->cikis_bosalt(2'000'000);
    const std::size_t o_an = alinan.load();
    for (int j = 0; j < 100 && alinan.load() < k2; ++j) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));   // son okumanın sayaca yazılması
    }
    dur = true;
    okuyucu.join();
    DOGRULA(d, b2 && k2 == v.size(), "okuyan karşı uçta kuyruk boşaldı: true (" + std::to_string(k2) + " B)");
    DOGRULA(d, al2 == v, "karşı uç bütün çerçeveyi aldı (" + std::to_string(al2.size()) + " B; dönüş anında sayılan " +
                             std::to_string(o_an) + ")");
    // Bellek taşımasının gönderim kuyruğu yok: true.
    auto [x, y] = bellek_cifti();
    DOGRULA(d, x->cikis_bosalt(0), "bellek taşıması: kuyruk yok (true)");
#endif
    return d.hata;
}
