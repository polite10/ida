// ida_a3_sahte — A3 sunucusunu sahte veri kaynağıyla çalıştıran program (W4-a3 kapsam 9).
//
//   ida_a3_sahte [--port N] [--statik DIZIN] [--adres 127.0.0.1|::1] [--sure SANIYE]
//
// Açılış adresini (A3-K05) standart çıktıya bir kez yazar; SIGINT/SIGTERM ya da --sure dolunca
// bağlantıları 1001 ile kapatıp çıkar. Değerler fizik değildir (sahte_dunya.hpp).
#include "ida/a3/sunucu.hpp"
#include "sahte_dunya.hpp"

#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <thread>

namespace {

volatile std::sig_atomic_t g_dur = 0;

void sinyal(int) { g_dur = 1; }

int kullanim()
{
    std::fprintf(stderr, "kullanım: ida_a3_sahte [--port N] [--statik DIZIN] [--adres 127.0.0.1|::1] [--sure SANIYE]\n");
    return 2;
}

}  // namespace

int main(int argc, char** argv)
{
    ida::a3::SunucuAyari ayar;
    ayar.sim_surumu = "ida_a3_sahte 0.1 (W4-a3; sahte veri, fizik değil)";
    double sure = 0.0;
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        if (i + 1 >= argc) {
            return kullanim();
        }
        const std::string v = argv[++i];
        if (a == "--port") {
            char* son = nullptr;
            const long p = std::strtol(v.c_str(), &son, 10);
            if (son == v.c_str() || *son != '\0' || p < 0 || p > 65535) {
                return kullanim();
            }
            ayar.port = static_cast<std::uint16_t>(p);
        } else if (a == "--statik") {
            ayar.statik_kok = v;
        } else if (a == "--adres") {
            ayar.adres = v;
        } else if (a == "--sure") {
            sure = std::strtod(v.c_str(), nullptr);
        } else {
            return kullanim();
        }
    }
    ayar.gunluk = [](std::string_view s) { std::fprintf(stderr, "%.*s\n", static_cast<int>(s.size()), s.data()); };

    // Dünya ve sunucu: Yayinci sunucunundur; sahte dünya onu sunucu başladıktan sonra alır.
    struct Vekil final : ida::a3::KomutAlicisi {
        std::atomic<ida::a3::sahte::SahteDunya*> d{nullptr};
        ida::a3::AnlikDurum anlik() override
        {
            auto* p = d.load();
            return p != nullptr ? p->anlik() : ida::a3::AnlikDurum{};
        }
        ida::a3::AlimSonucu al(const ida::a3::Komut& k) override
        {
            auto* p = d.load();
            if (p == nullptr) {
                ida::a3::AlimSonucu r;
                r.tur = ida::a3::AlimSonucu::Tur::dolu;
                return r;
            }
            return p->al(k);
        }
    } vekil;
    std::string hata;
    auto sunucu = ida::a3::Sunucu::baslat(ayar, vekil, hata);
    if (!sunucu) {
        std::fprintf(stderr, "ida_a3_sahte: %s\n", hata.c_str());
        return 1;  // A3-K02: döngü arabirimi dışı adreste dinlenmez, hata koduyla çıkılır
    }
    ida::a3::sahte::SahteDunya dunya(sunucu->yayinci(), ida::a3::sahte::SahteAyar{});
    vekil.d.store(&dunya);
    dunya.baslat();
    // A3-K04, K05: jeton yalnız açılış adresinin parçası olarak bir kez yazılır.
    std::printf("%s\n", sunucu->acilis_adresi().c_str());
    std::fflush(stdout);

    std::signal(SIGINT, sinyal);
    std::signal(SIGTERM, sinyal);
    const auto bas = std::chrono::steady_clock::now();
    while (g_dur == 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        if (sure > 0.0 && std::chrono::duration<double>(std::chrono::steady_clock::now() - bas).count() >= sure) {
            break;
        }
    }
    sunucu->durdur();
    vekil.d.store(nullptr);
    dunya.durdur();
    return 0;
}
