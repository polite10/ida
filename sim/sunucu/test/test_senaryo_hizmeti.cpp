// W5-a3-senaryo: senaryo hizmetinin dizin ve ad denetimi (A3-K78). Hizmet var olmayan bir depo köküne
// bağlanır: kalıp dışı bir ad dosya sistemine hiç gitmeden `gecersiz` döner, kalıba uyan olmayan ad ise
// dosya sistemine gidip `yok` döner (iki durumun ayrımı, ilkinin dosya sistemine dokunmadığını gösterir).
#include "senaryo_hizmeti.hpp"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>

int test_senaryo_hizmeti_ad()
{
    int hata = 0;
    auto dogrula = [&hata](bool k, const std::string& m) {
        std::printf("%s %s\n", k ? "GECTI " : "KALDI ", m.c_str());
        if (!k) {
            ++hata;
        }
    };
    using ida::a3::DosyaDurumu;
    ida::sunucu::SenaryoHizmeti::Ayar a;
    a.depo_koku = "/var/yok/ida_olmayan_kok";
    a.dizin = "scenarios";
    ida::sunucu::SenaryoHizmeti h(a);
    std::string ic, il;
    int dogru = 0;
    const char* kotu[] = {"../x.toml", "/etc/passwd", "..", "a/b.toml", "Z1.toml", "a.txt", "a\\b.toml", ".toml", "z1.toml/", "%2e%2e%2fx.toml"};
    for (const char* ad : kotu) {
        dogru += h.oku(ad, ic, il) == DosyaDurumu::gecersiz ? 1 : 0;
    }
    dogrula(dogru == 10, "kalıp dışı adlar dosya sistemine gitmeden gecersiz");
    dogrula(h.oku("z1.toml", ic, il) == DosyaDurumu::yok, "kalıba uyan olmayan ad: dosya sistemine gidildi, yok");
    dogrula(h.listele().empty(), "olmayan dizin: boş liste");
    namespace fs = std::filesystem;
    dogrula(ida::sunucu::SenaryoHizmeti::dizin_denetle("scenarios").empty() && ida::sunucu::SenaryoHizmeti::dizin_denetle("a/b/").empty() &&
                !ida::sunucu::SenaryoHizmeti::dizin_denetle("../x").empty() && !ida::sunucu::SenaryoHizmeti::dizin_denetle("/abs").empty() &&
                !ida::sunucu::SenaryoHizmeti::dizin_denetle("a/../b").empty() && !ida::sunucu::SenaryoHizmeti::dizin_denetle("").empty() &&
                !ida::sunucu::SenaryoHizmeti::dizin_denetle("C:/x").empty() && !ida::sunucu::SenaryoHizmeti::dizin_denetle("a\\b").empty() &&
                !ida::sunucu::SenaryoHizmeti::dizin_denetle("a//b").empty(),
            "--senaryo-dizini denetimi: göreli ve temiz yollar geçer; mutlak, .., boş parça, sürücü ve ters eğik çizgi reddedilir");
    return hata;
}
