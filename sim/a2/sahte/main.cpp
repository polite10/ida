// ida_a2_sahte — A2 sunucusunu sahte veri kaynağıyla çalıştıran program.
//
// İki kullanım: (1) gerçek görselleştiriciyi (viz/) gerçek A2 sunucusuna karşı elle denemek;
// (2) sahiplik kilidi sınamasında ayrı süreç olmak (sim.a2.kilit_surecler). `ida_sim`'e bağlama
// sonraki dilimdedir (W5-bagla-a2); bu program fizik koşturmaz (sahte_dunya.hpp).
//
//   ida_a2_sahte [--port N | --bos-port] [--kilit DOSYA] [--manifest YOL] [--sure S] [--hiz X]
//                [--sil] [--hedef N] [--firlat S] [--dur-dosyasi YOL]
//
// İlk satırda durumunu yazar: "A2_DURUM <ad> port=<port>". `--dur-dosyasi` verilirse dosya
// görününce süreç kilidi bırakmadan (_Exit) sonlanır: kilidin süreç ölünce işletim sistemince
// bırakıldığı böyle sınanır (A2-K77).
#include "ida/a2/sunucu.hpp"
#include "sahte_dunya.hpp"
#include "sha256.hpp"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <thread>
#include <vector>

namespace {

bool dosya_ozeti(const std::string& yol, std::array<std::uint8_t, 32>& ozet)
{
    std::ifstream f(yol, std::ios::binary);
    if (!f) {
        return false;
    }
    ida::a2::ic::Sha256 s;
    std::vector<char> t(65536);
    for (;;) {
        f.read(t.data(), static_cast<std::streamsize>(t.size()));
        const std::streamsize n = f.gcount();
        if (n > 0) {
            s.ekle(t.data(), static_cast<std::size_t>(n));
        }
        if (!f) {
            break;
        }
    }
    ozet = s.bitir();
    return true;
}

}  // namespace

int main(int argc, char** argv)
{
    ida::a2::SunucuAyari ayar;
    ida::a2::sahte::SahteAyar dunya;
    double sure_s = -1.0;
    double hiz = 1.0;
    double firlat_s = 20.0;
    std::string dur_dosyasi;
    std::string manifest;
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        const auto deger = [&](const char* ad) -> const char* {
            if (i + 1 >= argc) {
                std::fprintf(stderr, "ida_a2_sahte: %s değer ister\n", ad);
                std::exit(2);
            }
            return argv[++i];
        };
        if (a == "--port") {
            const long p = std::strtol(deger("--port"), nullptr, 10);
            if (p < 1024 || p > 65535) {
                std::fprintf(stderr, "ida_a2_sahte: --a2-port 1024 … 65535 olmalı (A2-K93)\n");
                return 2;
            }
            ayar.port = static_cast<std::uint16_t>(p);
        } else if (a == "--bos-port") {
            ayar.sinama_bos_port = true;
        } else if (a == "--kilit") {
            ayar.kilit_dosyasi = deger("--kilit");
        } else if (a == "--manifest") {
            manifest = deger("--manifest");
        } else if (a == "--sure") {
            sure_s = std::strtod(deger("--sure"), nullptr);
        } else if (a == "--hiz") {
            hiz = std::strtod(deger("--hiz"), nullptr);
        } else if (a == "--sil") {
            dunya.sim_kipi = ida::a2::SimKipi::sil;
        } else if (a == "--hedef") {
            dunya.hedef_sayisi = static_cast<std::uint32_t>(std::strtoul(deger("--hedef"), nullptr, 10));
        } else if (a == "--firlat") {
            firlat_s = std::strtod(deger("--firlat"), nullptr);
        } else if (a == "--dur-dosyasi") {
            dur_dosyasi = deger("--dur-dosyasi");
        } else {
            std::fprintf(stderr, "ida_a2_sahte: bilinmeyen seçenek: %s\n", a.c_str());
            return 2;
        }
    }
    if (!manifest.empty()) {
        std::error_code ec;
        const std::filesystem::path mutlak = std::filesystem::absolute(manifest, ec);
        if (ec || !dosya_ozeti(manifest, dunya.manifest_ozeti)) {
            std::fprintf(stderr, "ida_a2_sahte: manifest okunamadı: %s\n", manifest.c_str());
            return 2;
        }
        const std::u8string u = mutlak.u8string();
        dunya.manifest_yolu.assign(u.begin(), u.end());
    }
    if (firlat_s > 0.0) {
        dunya.firlatma_adimi = static_cast<std::uint64_t>(firlat_s * 800.0);
        dunya.isabet_adimi = dunya.firlatma_adimi + 1600;  // 2 s sonra
    }

    std::string hata;
    const std::unique_ptr<ida::a2::Sunucu> sunucu = ida::a2::Sunucu::baslat(ayar, hata);
    if (!sunucu) {
        std::fprintf(stderr, "ida_a2_sahte: %s\n", hata.c_str());
        return 2;
    }
    std::printf("A2_DURUM %s port=%u\n", ida::a2::a2_durumu_adi(sunucu->durum()),
                static_cast<unsigned>(sunucu->port()));
    std::printf("%s\n", sunucu->durum_aciklamasi().c_str());
    std::fflush(stdout);

    ida::a2::sahte::SahteDunya d(sunucu->yayinci(), dunya);
    d.basla();

    using Saat = std::chrono::steady_clock;
    const Saat::time_point t0 = Saat::now();
    double borc = 0.0;  // atılacak adım (kesirli)
    Saat::time_point onceki = t0;
    for (;;) {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
        const Saat::time_point simdi = Saat::now();
        if (!dur_dosyasi.empty()) {
            std::error_code ec;
            if (std::filesystem::exists(dur_dosyasi, ec)) {
                std::_Exit(0);  // kilit açıkça bırakılmaz: işletim sistemi bırakır
            }
        }
        if (sure_s >= 0.0 && std::chrono::duration<double>(simdi - t0).count() >= sure_s) {
            break;
        }
        borc += std::chrono::duration<double>(simdi - onceki).count() * 800.0 * hiz;
        onceki = simdi;
        const auto n = static_cast<std::uint64_t>(borc);
        borc -= static_cast<double>(n);
        d.adimla(n < 100000u ? n : 100000u);
    }
    sunucu->yayinci().kapaniyor();
    sunucu->durdur();
    const ida::a2::SunucuSayaclari s = sunucu->sayaclar();
    std::printf("A2_SON baglanti=%llu gonderilen=%llu seyreltilen=%llu kopma=%s\n",
                static_cast<unsigned long long>(s.baglanti_sayisi), static_cast<unsigned long long>(s.gonderilen_ileti),
                static_cast<unsigned long long>(s.seyreltilen_durum), ida::a2::kopma_nedeni_adi(s.son_kopma_nedeni));
    return 0;
}
