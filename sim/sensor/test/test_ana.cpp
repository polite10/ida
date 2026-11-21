// sim/sensor test koşucusu: `ida_sensor_test <ad>`.
#include "test_destek.hpp"

#include <cstring>

namespace ida::sensor::test {

int test_philox_bilinen_yanit();
int test_ideal_durgun();
int test_ideal_donus();
int test_ideal_ivme();
int test_ideal_dalis();
int test_ideal_montaj();
int test_istatistik_imu();
int test_istatistik_digerleri();
int test_zaman_hiz_gecikme();
int test_zaman_dvl();
int test_gecerlilik_gnss();
int test_gecerlilik_dvl();
int test_gecerlilik_doyma();
int test_emniyet_geri_okuma();
int test_sizinti();
int test_belirlenimcilik();
int test_sensor_bagimsizligi();
int test_ariza_etkileri();
int test_ariza_desenleri();
int test_ariza_akis_bagimsizligi();
int test_kurulum_hatalari();
int test_sure();
int test_dvl_irtifa_egik();
int test_gnss_jeoit();
int test_ariza_donma_bilesik();
int test_ariza_temsil_araligi();
int test_ariza_suzgecli_imu();
int test_ornek_sayaci_64();
int test_emniyet_revizyon();
int test_kurulum_sureleri();

SensorKumesiAyari ideal_ayar()
{
    SensorKumesiAyari k = ida0_sensor_kumesi();
    k.ic_sicaklik.etkin = false;
    for (auto& s : k.sensorler) {
        if (auto* i = std::get_if<ImuAyar>(&s.ayar)) {
            i->jiroskop_yogunluk = 0.0;
            i->ivme_yogunluk = {0.0, 0.0, 0.0};
            i->jiroskop_acilis_bias_siniri = i->ivme_acilis_bias_siniri = 0.0;
            i->jiroskop_sicaklik_kats_siniri = i->ivme_sicaklik_kats_siniri = 0.0;
            i->jiroskop_olcek_siniri = i->jiroskop_eksenler_arasi_siniri = 0.0;
            i->ivme_olcek_siniri = i->ivme_eksenler_arasi_siniri = 0.0;
            i->jiroskop_dogrusalsizlik = i->ivme_dogrusalsizlik = 0.0;
            i->jiroskop_gm_sigma = i->ivme_gm_sigma = 0.0;
            i->dunya_donusu_etkin = false;
        } else if (auto* m = std::get_if<ManyetometreAyar>(&s.ayar)) {
            m->gurultu_sigma = 0.0;
            m->itici_girisim_orani = 0.0;
        } else if (auto* b = std::get_if<BasincAyar>(&s.ayar)) {
            b->gurultu_sigma = 0.0;
            b->mutlak_dogruluk_siniri = 0.0;
            b->kuantalama = 0.0;
            b->sicaklik_ofset_siniri = 0.0;
        } else if (auto* g = std::get_if<GnssAyar>(&s.ayar)) {
            g->cep = 0.0;
            g->hiz_sigma = 0.0;
        } else if (auto* v = std::get_if<DvlAyar>(&s.ayar)) {
            v->sigma0 = v->k_r = v->k_u = 0.0;
            v->uzun_donem_olcek_siniri = 0.0;
        } else if (auto* p = std::get_if<BataryaOlcerAyar>(&s.ayar)) {
            p->analog_gurultu_sigma = 0.0;
        }
    }
    return k;
}

}  // namespace ida::sensor::test

int main(int argc, char** argv)
{
    using namespace ida::sensor::test;
    struct Kayit {
        const char* ad;
        int (*islev)();
    };
    const Kayit testler[] = {
        {"philox_bilinen_yanit", test_philox_bilinen_yanit},
        {"ideal_durgun", test_ideal_durgun},
        {"ideal_donus", test_ideal_donus},
        {"ideal_ivme", test_ideal_ivme},
        {"ideal_dalis", test_ideal_dalis},
        {"ideal_montaj", test_ideal_montaj},
        {"istatistik_imu", test_istatistik_imu},
        {"istatistik_digerleri", test_istatistik_digerleri},
        {"zaman_hiz_gecikme", test_zaman_hiz_gecikme},
        {"zaman_dvl", test_zaman_dvl},
        {"gecerlilik_gnss", test_gecerlilik_gnss},
        {"gecerlilik_dvl", test_gecerlilik_dvl},
        {"gecerlilik_doyma", test_gecerlilik_doyma},
        {"emniyet_geri_okuma", test_emniyet_geri_okuma},
        {"sizinti", test_sizinti},
        {"belirlenimcilik", test_belirlenimcilik},
        {"sensor_bagimsizligi", test_sensor_bagimsizligi},
        {"ariza_etkileri", test_ariza_etkileri},
        {"ariza_desenleri", test_ariza_desenleri},
        {"ariza_akis_bagimsizligi", test_ariza_akis_bagimsizligi},
        {"kurulum_hatalari", test_kurulum_hatalari},
        {"sure", test_sure},
        {"dvl_irtifa_egik", test_dvl_irtifa_egik},
        {"gnss_jeoit", test_gnss_jeoit},
        {"ariza_donma_bilesik", test_ariza_donma_bilesik},
        {"ariza_temsil_araligi", test_ariza_temsil_araligi},
        {"ariza_suzgecli_imu", test_ariza_suzgecli_imu},
        {"ornek_sayaci_64", test_ornek_sayaci_64},
        {"emniyet_revizyon", test_emniyet_revizyon},
        {"kurulum_sureleri", test_kurulum_sureleri},
    };
    if (argc < 2) {
        std::printf("kullanım: %s <test>\n", argv[0]);
        return 2;
    }
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
