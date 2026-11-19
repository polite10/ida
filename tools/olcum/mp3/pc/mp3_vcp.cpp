// W0-MP3a ölçüm prototipi: ST-LINK VCP bağlantı ölçüm aracı (atılabilir; yalnız macOS gerçeklendi).
//
//   mp3_vcp kimlik <port>
//       Denetim hızında (115200) kartın KIMLIK metnini basar.
//   mp3_vcp kos <port> --mod akis|echo --baud B --yuk N --sure S --kayit DOSYA
//               [--oran O (≤ 0.80)] [--hiz ISTEK_S] [--en-az-sn S] [--kampanya] [--kosu ONALTILIK]
//               [--zaman-asimi-ms 1000] [--azami-bekleyen 64]
//       Bir koşu. Akış oranı %80'i aşamaz (sabit kampanya sınırı); %80 altı "deneysel" profildir. --kampanya:
//       dondurulmuş kampanya profili (mp3-akis-v1 / mp3-echo-v1) zorunlu, değilse GECMEDI. Kayıt dosyası var olmamalı.
//   mp3_vcp tara <port> --dizin D [--baudlar 921600,2000000,3000000,6000000,12000000] [--akis-sn 10] [--echo-sn 5]
//       Ön tarama: her aday için kısa akış (256 B, %80) ve kısa echo (64 B, 200 istek/s); profil zorunlu.
//   mp3_vcp analiz <kayıt> [--en-az-sn S] [--kampanya]
// Çıkış kodu (kos/analiz): 0 GECTI, 1 GECMEDI, 2 GECERSIZ, 3 BASLAMADI, 4 kullanım/ortam hatası.
// Çıkış kodu (tara): 0 bütün koşular GECTI, 1 tarama tamamlandı ama en az bir aday GECMEDI/BASLAMADI,
//                    2 en az bir koşu GECERSIZ, 4 kullanım/ortam hatası.
#include "kayit.hpp"
#include "mp3_cerceve.h"
#include "oturum.hpp"
#include "secenek.hpp"
#include "seri.hpp"

#include <csignal>
#include <cstdio>
#include <iostream>
#include <map>
#include <memory>
#include <random>
#include <set>
#include <sstream>
#include <string>
#include <sys/stat.h>
#include <vector>

using namespace mp3;

namespace {

int kullanim(const std::string &neden = "")
{
    if (!neden.empty()) std::cerr << "hata: " << neden << "\n";
    std::cerr << "kullanım: mp3_vcp kimlik <port> | kos <port> --mod akis|echo --baud B --yuk N --sure S --kayit DOSYA "
                 "[--oran O<=0.80] [--hiz H] [--en-az-sn S] [--kampanya] [--kosu HEX] | tara <port> --dizin D "
                 "[--baudlar ...] [--akis-sn S] [--echo-sn S] | analiz <kayıt> [--en-az-sn S] [--kampanya]\n";
    return 4;
}

// "--anahtar değer" çiftleri; bayraklar (değersiz) ayrı küme. Bilinmeyen seçenek ya da yinelenen anahtar hata.
bool secenekler(int argc, char **argv, int bas, const std::set<std::string> &degerli, const std::set<std::string> &bayrak,
                std::map<std::string, std::string> &m, std::string &hata)
{
    for (int i = bas; i < argc; ++i) {
        const std::string k = argv[i];
        if (bayrak.count(k)) {
            if (!m.emplace(k, "1").second) { hata = "yinelenen " + k; return false; }
            continue;
        }
        if (!degerli.count(k)) { hata = "bilinmeyen seçenek " + k; return false; }
        if (i + 1 >= argc) { hata = k + " değer bekliyor"; return false; }
        if (!m.emplace(k, argv[++i]).second) { hata = "yinelenen " + k; return false; }
    }
    return true;
}

uint32_t rastgele_kosu()
{
    std::random_device rd;
    uint32_t v = 0;
    while (v == 0) v = rd();
    return v;
}

void bas(const AnalizSonuc &s)
{
    std::cout << s.rapor;
    for (const auto &n : s.nedenler) std::cout << "  neden: " << n << "\n";
    std::cout << "SONUC: " << hukum_ad(s.hukum) << " (profil " << (s.profil.empty() ? "?" : s.profil) << ")" << std::endl;
}

std::unique_ptr<Port> ac(const std::string &yol)
{
    std::string h;
    Port *p = posix_ac(yol, h);
    if (!p) std::cerr << "port açılamadı: " << yol << ": " << h << "\n";
    return std::unique_ptr<Port>(p);
}

}  // namespace

int main(int argc, char **argv)
{
    std::signal(SIGPIPE, SIG_IGN);   // kopan hatta yazma EPIPE dönsün, süreç ölmesin
    mp3_hazirla();
    if (argc < 3) return kullanim();
    const std::string kom = argv[1];
    std::map<std::string, std::string> o;
    std::string hata;
    if (kom == "analiz") {
        if (!secenekler(argc, argv, 3, {"--en-az-sn"}, {"--kampanya"}, o, hata)) return kullanim(hata);
        AnalizBeklenti b;
        if (o.count("--en-az-sn")) {
            const auto v = ondalik(o["--en-az-sn"], 0.0, 1e6);
            if (!v) return kullanim("--en-az-sn");
            b.en_az_sn = *v;
        }
        b.kampanya = o.count("--kampanya") != 0;
        const AnalizSonuc s = analiz_dosya(argv[2], b);
        bas(s);
        return static_cast<int>(s.hukum);
    }
    if (kom != "kimlik" && kom != "kos" && kom != "tara") return kullanim("bilinmeyen komut " + kom);
    Ayar a;
    AnalizBeklenti b;
    uint32_t kosu = 0;
    std::vector<uint32_t> baudlar = {921600, 2000000, 3000000, 6000000, 12000000};
    double akis_sn = 10.0, echo_sn = 5.0;
    std::string dizin;
    if (kom == "kos") {
        if (!secenekler(argc, argv, 3,
                        {"--mod", "--baud", "--yuk", "--sure", "--kayit", "--oran", "--hiz", "--en-az-sn", "--kosu",
                         "--zaman-asimi-ms", "--azami-bekleyen"},
                        {"--kampanya"}, o, hata))
            return kullanim(hata);
        for (const char *z : {"--mod", "--baud", "--yuk", "--sure", "--kayit"})
            if (!o.count(z)) return kullanim(std::string(z) + " zorunlu");
        if (o["--mod"] != "akis" && o["--mod"] != "echo") return kullanim("--mod akis|echo");
        a.echo = o["--mod"] == "echo";
        const auto baud = tam_sayi(o["--baud"], 1200, 20000000);
        const auto yuk = tam_sayi(o["--yuk"], 0, MP3_AZAMI_YUK);
        const auto sure = ondalik(o["--sure"], 0.001, 86400.0);
        if (!baud) return kullanim("--baud 1200..20000000 tam sayı");
        if (!yuk) return kullanim("--yuk 0..256 tam sayı");
        if (!sure) return kullanim("--sure 0.001..86400 s");
        a.baud = static_cast<uint32_t>(*baud);
        a.yuk = static_cast<uint16_t>(*yuk);
        a.sure_s = *sure;
        a.kayit = o["--kayit"];
        if (o.count("--oran")) {
            const auto v = ondalik(o["--oran"], 1e-6, AKIS_ORAN_SINIR);
            if (!v) return kullanim("--oran (0, 0.80]: akış kampanya sınırı %80 sabittir");
            a.akis_oran = *v;
        }
        if (o.count("--hiz")) {
            const auto v = ondalik(o["--hiz"], 0.001, 100000.0);
            if (!v) return kullanim("--hiz");
            a.echo_hiz = *v;
        }
        if (o.count("--zaman-asimi-ms")) {
            const auto v = tam_sayi(o["--zaman-asimi-ms"], 1, 60000);
            if (!v) return kullanim("--zaman-asimi-ms");
            a.zaman_asimi_ms = static_cast<int>(*v);
        }
        if (o.count("--azami-bekleyen")) {
            const auto v = tam_sayi(o["--azami-bekleyen"], 1, 100000);
            if (!v) return kullanim("--azami-bekleyen");
            a.azami_bekleyen = static_cast<uint32_t>(*v);
        }
        if (o.count("--en-az-sn")) {
            const auto v = ondalik(o["--en-az-sn"], 0.0, 1e6);
            if (!v) return kullanim("--en-az-sn");
            b.en_az_sn = *v;
        }
        b.kampanya = o.count("--kampanya") != 0;
        if (o.count("--kosu")) {
            const auto v = tam_sayi(o["--kosu"], 1, 0xFFFFFFFFull, 16);
            if (!v) return kullanim("--kosu onaltılık 1..FFFFFFFF");
            kosu = static_cast<uint32_t>(*v);
        } else {
            kosu = rastgele_kosu();
        }
    } else if (kom == "tara") {
        if (!secenekler(argc, argv, 3, {"--dizin", "--baudlar", "--akis-sn", "--echo-sn"}, {}, o, hata)) return kullanim(hata);
        if (!o.count("--dizin")) return kullanim("--dizin zorunlu");
        dizin = o["--dizin"];
        struct stat st{};
        if (stat(dizin.c_str(), &st) != 0 || !S_ISDIR(st.st_mode)) return kullanim("dizin yok: " + dizin);
        if (o.count("--baudlar")) {
            baudlar.clear();
            std::stringstream ss(o["--baudlar"]);
            std::string t;
            while (std::getline(ss, t, ',')) {
                const auto v = tam_sayi(t, 1200, 20000000);
                if (!v) return kullanim("--baudlar: " + t);
                baudlar.push_back(static_cast<uint32_t>(*v));
            }
            if (baudlar.empty()) return kullanim("--baudlar boş");
        }
        if (o.count("--akis-sn")) {
            const auto v = ondalik(o["--akis-sn"], 0.001, 86400.0);
            if (!v) return kullanim("--akis-sn");
            akis_sn = *v;
        }
        if (o.count("--echo-sn")) {
            const auto v = ondalik(o["--echo-sn"], 0.001, 86400.0);
            if (!v) return kullanim("--echo-sn");
            echo_sn = *v;
        }
    } else if (argc != 3) {
        return kullanim("kimlik ek seçenek almaz");
    }

    auto port = ac(argv[2]);
    if (!port) return 4;
    if (kom == "kimlik") {
        HizRaporu h;
        if (!port->hiz(MP3_KONTROL_BAUD, h)) { std::cerr << "hız ayarlanamadı: " << h.aciklama << "\n"; return 4; }
        port->giris_at();
        std::map<std::string, std::string> k;
        std::string ham;
        if (!kimlik_sor(*port, 3000, k, ham)) { std::cerr << "KIMLIK yanıtı yok\n"; return 2; }
        std::cout << ham << std::endl;
        return 0;
    }
    if (kom == "kos") {
        std::cout << "koşu " << std::hex << kosu << std::dec << " mod=" << o["--mod"] << " baud=" << a.baud
                  << " yük=" << a.yuk << " süre=" << a.sure_s << " s kayıt=" << a.kayit << std::endl;
        const AnalizSonuc s = kos(*port, a, kosu, b, hata);
        if (!hata.empty()) std::cerr << hata << "\n";
        bas(s);
        return static_cast<int>(s.hukum);
    }
    std::vector<TaramaSatiri> tablo;
    const int rc = tara(*port, baudlar, akis_sn, echo_sn, dizin, rastgele_kosu, tablo, Ayar{});
    std::cout << "=== ön tarama özeti (tarama tamamlandı; çıkış kodu " << rc << ")" << std::endl;
    for (const auto &t : tablo)
        std::cout << t.baud << ": akis=" << hukum_ad(t.akis) << " echo=" << hukum_ad(t.echo) << std::endl;
    return rc;
}
