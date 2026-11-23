// sim/sil test koşucusu: `ida_sil_test <ad> <geçici dizin> [ida_sil yolu]`.
#include <cstdio>
#include <cstring>
#include <system_error>

#include "test_destek.hpp"

namespace ida::sil::test {
int test_turetme_vektorleri(const Baglam&);
int test_crc32(const Baglam&);
int test_depo(const Baglam&);
int test_komut_satiri(const Baglam&);
int test_oturum(const Baglam&);
int test_geri_donus(const Baglam&);
int test_komut_yolu(const Baglam&);
int test_param_deposu(const Baglam&);
int test_pencere(const Baglam&);
int test_belirlenim(const Baglam&);
int test_program(const Baglam&);
int test_alim_damgasi(const Baglam&);
int test_tf_reddi(const Baglam&);
int test_reboot_tfsiz_kapanis(const Baglam&);
int test_core_ok_sifir(const Baglam&);
int test_durdur_gonderim(const Baglam&);
int test_depo_kalicilik(const Baglam&);
}  // namespace ida::sil::test

int main(int argc, char** argv)
{
    using namespace ida::sil::test;
    struct Kayit {
        const char* ad;
        int (*islev)(const Baglam&);
    };
    const Kayit testler[] = {
        {"turetme_vektorleri", test_turetme_vektorleri},
        {"crc32", test_crc32},
        {"depo", test_depo},
        {"komut_satiri", test_komut_satiri},
        {"oturum", test_oturum},
        {"geri_donus", test_geri_donus},
        {"komut_yolu", test_komut_yolu},
        {"param_deposu", test_param_deposu},
        {"pencere", test_pencere},
        {"belirlenim", test_belirlenim},
        {"program", test_program},
        {"alim_damgasi", test_alim_damgasi},
        {"tf_reddi", test_tf_reddi},
        {"reboot_tfsiz_kapanis", test_reboot_tfsiz_kapanis},
        {"core_ok_sifir", test_core_ok_sifir},
        {"durdur_gonderim", test_durdur_gonderim},
        {"depo_kalicilik", test_depo_kalicilik},
    };
    if (argc < 2) {
        std::fprintf(stderr, "kullanım: ida_sil_test <ad> [geçici dizin]\n");
        return 2;
    }
    for (const Kayit& k : testler) {
        if (std::strcmp(argv[1], k.ad) != 0) {
            continue;
        }
        Baglam b;
        b.ad = k.ad;
        const std::filesystem::path kok =
            argc >= 3 ? std::filesystem::path(argv[2]) : std::filesystem::temp_directory_path();
        b.gecici = kok / ("ida_sil_" + b.ad);
        b.program = argc >= 4 ? argv[3] : "";
        std::error_code ec;
        std::filesystem::remove_all(b.gecici, ec);
        std::filesystem::create_directories(b.gecici, ec);
        const int hata = k.islev(b);
        std::printf("%s: %s (%d hata)\n", k.ad, hata == 0 ? "GEÇTİ" : "KALDI", hata);
        return hata == 0 ? 0 : 1;
    }
    std::fprintf(stderr, "bilinmeyen test: %s\n", argv[1]);
    return 2;
}
