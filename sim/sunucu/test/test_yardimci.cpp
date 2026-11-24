// Sunucu yardımcılarının sınamaları (sim.sunucu.*).
//
//   arguman_kacisi  sol #14: Windows komut satırı kodlaması Microsoft CRT ayrıştırma kurallarıyla
//                   geri çözülünce argümanlar aynen çıkar (platformdan bağımsız; başvuru
//                   ayrıştırıcısı belgedeki kurallardır); eski kodlamanın karşı örneği
//   arguman_surec   alt süreç başlatılır, argümanları dosyaya yazar; aynen geldikleri denetlenir
//                   (Windows'ta CreateProcess + CRT'nin gerçek ayrıştırması; POSIX'te posix_spawn)
#include "yardimci.hpp"

#include <chrono>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace {

int g_hata = 0;

void dogrula(bool kosul, const std::string& ileti)
{
    std::printf("%s %s\n", kosul ? "GECTI " : "KALDI ", ileti.c_str());
    if (!kosul) {
        ++g_hata;
    }
}

// Microsoft CRT kuralları (argv[1..]; "Parsing C command-line arguments"): boşluk/sekme ayırır;
// tırnak içi boşluk argümana girer; 2n ters bölü + tırnak → n ters bölü, tırnak kipi değişir;
// 2n + 1 ters bölü + tırnak → n ters bölü ve tırnak karakteri; tırnaktan önce gelmeyen ters
// bölüler olduğu gibi; tırnak içinde "" → tek tırnak karakteri (2008 sonrası CRT).
std::vector<std::string> crt_ayristir(const std::string& s)
{
    std::vector<std::string> v;
    std::size_t i = 0;
    const std::size_t n = s.size();
    while (i < n) {
        while (i < n && (s[i] == ' ' || s[i] == '\t')) {
            ++i;
        }
        if (i >= n) {
            break;
        }
        std::string a;
        bool tirnak = false;
        while (i < n && (tirnak || (s[i] != ' ' && s[i] != '\t'))) {
            if (s[i] == '\\') {
                std::size_t k = 0;
                while (i < n && s[i] == '\\') {
                    ++k;
                    ++i;
                }
                if (i < n && s[i] == '"') {
                    a.append(k / 2, '\\');
                    if (k % 2 == 1) {
                        a += '"';
                        ++i;
                    }
                } else {
                    a.append(k, '\\');
                }
                continue;
            }
            if (s[i] == '"') {
                if (tirnak && i + 1 < n && s[i + 1] == '"') {
                    a += '"';
                    i += 2;
                    continue;
                }
                tirnak = !tirnak;
                ++i;
                continue;
            }
            a += s[i++];
        }
        v.push_back(a);
    }
    return v;
}

// Program adı (argv[0]): tırnakla başlıyorsa sonraki tırnağa kadar, kaçış yok.
std::pair<std::string, std::string> program_ayir(const std::string& s)
{
    if (!s.empty() && s[0] == '"') {
        const std::size_t k = s.find('"', 1);
        return {s.substr(1, k - 1), k + 1 < s.size() ? s.substr(k + 1) : std::string()};
    }
    const std::size_t k = s.find_first_of(" \t");
    return {s.substr(0, k), k == std::string::npos ? std::string() : s.substr(k)};
}

const std::vector<std::string>& ornekler()
{
    static const std::vector<std::string> v = {
        "duz",
        "",
        "iki kelime",
        "C:\\yol\\dosya",
        "C:\\bosluklu yol\\",            // sondaki ters bölü kapanış tırnağını kaçırmamalı
        "C:\\bosluklu yol\\\\",
        "a\\",
        "x\"y",
        "\\\"",
        "a\\\\\"b c",
        "sekme\tiçinde",
        "\"",
        "\\\\sunucu\\pay alanı\\",
        "--depo",
    };
    return v;
}

int test_arguman_kacisi()
{
    std::vector<std::string> argv = {"C:\\Program Files\\ida\\ida_sil.exe"};
    for (const std::string& a : ornekler()) {
        argv.push_back(a);
    }
    argv.push_back("--a1-port");
    argv.push_back("5760");
    std::string satir;
    dogrula(ida::sunucu::windows_komut_satiri(argv, satir), "kodlandı");
    const auto [prog, kalan] = program_ayir(satir);
    dogrula(prog == argv[0], "program adı aynen");
    const std::vector<std::string> geri = crt_ayristir(kalan);
    bool ayni = geri.size() + 1u == argv.size();
    for (std::size_t i = 0; ayni && i < geri.size(); ++i) {
        ayni = geri[i] == argv[i + 1];
    }
    dogrula(ayni, "CRT kurallarıyla geri çözülen argümanlar aynen (" + std::to_string(geri.size()) + " / " +
                      std::to_string(argv.size() - 1) + ")");
    // Belgedeki örnekler (biçim).
    std::string t;
    (void)ida::sunucu::windows_komut_satiri({"p", "a b\\"}, t);
    dogrula(t == "\"p\" \"a b\\\\\"", "kapanıştan önceki ters bölü çoğaltıldı: " + t);
    (void)ida::sunucu::windows_komut_satiri({"p", "x\"y"}, t);
    dogrula(t == "\"p\" \"x\\\"y\"", "tırnak kaçırıldı: " + t);
    (void)ida::sunucu::windows_komut_satiri({"p", ""}, t);
    dogrula(t == "\"p\" \"\"", "boş argüman korunur: " + t);
    dogrula(!ida::sunucu::windows_komut_satiri({"p\"q"}, t), "tırnaklı program adı reddedilir");

    // Karşı örnek: eski kodlama (her argümanı yalnız tırnağa almak) sondaki ters bölüde
    // kapanış tırnağını kaçırır ve sonraki argümanı yutar.
    std::string naif;
    for (const std::string& a : std::vector<std::string>{"C:\\bosluklu yol\\", "--a1-port", "5760"}) {
        naif += (naif.empty() ? "" : " ") + ('"' + a + '"');
    }
    const std::vector<std::string> naif_geri = crt_ayristir(naif);
    dogrula(naif_geri.size() != 3u, "eski kodlamanın karşı örneği: " + std::to_string(naif_geri.size()) +
                                        " argüman çözüldü (3 olmalıydı)");
    return 0;
}

// Alt süreç kipi: argümanları uzunluk önekli olarak dosyaya yazar.
int yanki(int argc, char** argv)
{
    std::ofstream f(argv[2], std::ios::binary);
    for (int i = 3; i < argc; ++i) {
        const std::string a = argv[i];
        f << a.size() << ':' << a;
    }
    return 0;
}

int test_arguman_surec(const char* kendi)
{
    const std::string dosya = std::string(kendi) + ".yanki.txt";
    std::remove(dosya.c_str());
    std::vector<std::string> argv = {kendi, "yanki", dosya};
    for (const std::string& a : ornekler()) {
        argv.push_back(a);
    }
    std::string hata;
    auto s = ida::sunucu::surec_baslat(argv, hata);
    dogrula(s != nullptr, "alt süreç başladı " + hata);
    if (!s) {
        return 0;
    }
    const auto son = std::chrono::steady_clock::now() + std::chrono::seconds(20);
    while (s->calisiyor() && std::chrono::steady_clock::now() < son) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    dogrula(!s->calisiyor(), "alt süreç bitti");
    std::ifstream f(dosya, std::ios::binary);
    std::stringstream ss;
    ss << f.rdbuf();
    std::string icerik = ss.str();
    std::string beklenen;
    for (std::size_t i = 3; i < argv.size(); ++i) {
        beklenen += std::to_string(argv[i].size()) + ':' + argv[i];
    }
    dogrula(icerik == beklenen, "alt süreç argümanları aynen aldı (" + std::to_string(icerik.size()) + " / " +
                                    std::to_string(beklenen.size()) + " bayt)");
    std::remove(dosya.c_str());
    return 0;
}

}  // namespace

int test_a3_kesim();   // test_a3_alici.cpp (W4-K27 (2))
int test_a3_duraklatma_kapali();   // test_a3_alici.cpp (W4-K28 (1))

int main(int argc, char** argv)
{
    if (argc >= 3 && std::string(argv[1]) == "yanki") {
        return yanki(argc, argv);
    }
    if (argc < 2) {
        std::printf("kullanım: ida_sunucu_test <arguman_kacisi|arguman_surec>\n");
        return 2;
    }
    const std::string ad = argv[1];
    if (ad == "arguman_kacisi") {
        (void)test_arguman_kacisi();
    } else if (ad == "arguman_surec") {
        (void)test_arguman_surec(argv[0]);
    } else if (ad == "a3_kesim") {
        g_hata += test_a3_kesim();
    } else if (ad == "a3_duraklatma_kapali") {
        g_hata += test_a3_duraklatma_kapali();
    } else {
        std::printf("bilinmeyen test: %s\n", ad.c_str());
        return 2;
    }
    std::printf("%s %s: %d kalan\n", ad.c_str(), g_hata == 0 ? "GECTI" : "KALDI", g_hata);
    return g_hata == 0 ? 0 : 1;
}
