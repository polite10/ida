// sim/sonar test koşucusu: `ida_sonar_test <ad>`.
#include "test_destek.hpp"

#include <cstring>

namespace ida::sonar::test {

int test_ses_hizi();
int test_sogurma();
int test_menzil_yasasi();
int test_malzeme();
int test_ping1d_duz_taban();
int test_ping360_kure_duvar();
int test_ping360_yonelim_montaj();
int test_enerji_isin_sayisi();
int test_yakinsama_mp7();
int test_zaman_ping360();
int test_zaman_ping1d();
int test_zaman_hareket();
int test_belirlenimcilik();
int test_sonar_bagimsizligi();
int test_ariza_etkileri();
int test_ariza_desenleri();
int test_kurulum_hatalari();
int test_w4k1_sensor_kumesi();
int test_sure();

}  // namespace ida::sonar::test

int main(int argc, char** argv)
{
    using namespace ida::sonar::test;
    struct Kayit {
        const char* ad;
        int (*islev)();
    };
    const Kayit testler[] = {
        {"ses_hizi", test_ses_hizi},
        {"sogurma", test_sogurma},
        {"menzil_yasasi", test_menzil_yasasi},
        {"malzeme", test_malzeme},
        {"ping1d_duz_taban", test_ping1d_duz_taban},
        {"ping360_kure_duvar", test_ping360_kure_duvar},
        {"ping360_yonelim_montaj", test_ping360_yonelim_montaj},
        {"enerji_isin_sayisi", test_enerji_isin_sayisi},
        {"yakinsama_mp7", test_yakinsama_mp7},
        {"zaman_ping360", test_zaman_ping360},
        {"zaman_ping1d", test_zaman_ping1d},
        {"zaman_hareket", test_zaman_hareket},
        {"belirlenimcilik", test_belirlenimcilik},
        {"sonar_bagimsizligi", test_sonar_bagimsizligi},
        {"ariza_etkileri", test_ariza_etkileri},
        {"ariza_desenleri", test_ariza_desenleri},
        {"kurulum_hatalari", test_kurulum_hatalari},
        {"w4k1_sensor_kumesi", test_w4k1_sensor_kumesi},
        {"sure", test_sure},
    };
    if (argc < 2) {
        std::printf("kullanım: %s <test>\n", argv[0]);
        return 2;
    }
    tani_kipi() = argc >= 3 && std::strcmp(argv[2], "tani") == 0;
    for (const auto& t : testler) {
        if (std::strcmp(t.ad, argv[1]) == 0) {
            std::printf("[%s]\n", t.ad);
            const int r = t.islev();
            std::printf("[%s] %s\n", t.ad, r == 0 ? "GEÇTİ" : "KALDI");
            return r == 0 ? 0 : 1;
        }
    }
    std::printf("bilinmeyen test: %s\n", argv[1]);
    return 2;
}
