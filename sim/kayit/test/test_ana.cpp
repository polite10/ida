// test_ana.cpp — sim/kayit testlerinin giriş noktası: `ida_kayit_test <test_adı> [bağımsız değişken]`
// (CTest: sim.kayit.<ad>).
#include <cstdio>
#include <cstring>
#include <string>

namespace ida::kayit::test {
int test_crc32();
int test_halka();
int test_dusme_yuvasi();
int test_guvence();
int test_ust_veri_denetimi();
int test_coz_surum_kurali();
int test_katalog_tablosu();
int test_belge_tablosu();
int test_kodlayici_ortak();
int test_coz_gidis_donus();
int test_bicim();
int test_gidis_donus();
int test_zaman_alanlari();
int test_sira_karisik();
int test_kademe();
int test_kademe_suzgeci();
int test_uretici_beklemez();
int test_uretici_ayirmaz();
int test_eksik_sayim();
int test_es_zamanli_ureticiler();
int test_kurtarma_kesik();
int test_parca_suresi();
int test_bozulma_parca();
int test_bozulma_ek_ust_veri();
int test_ust_veri_birlestirme();
int test_rastgele_bozulma();
int test_dosya_ezilmez();
int test_negatif_zaman();
int test_sira_siniri();
int test_kademe_degismez();
int test_kapanis();
int test_hata_kapanisi();
int test_erken_kapanis_cok_uretici();
int test_kapat_sirasinda_hata();
int test_guncelleme_teslim_hatasi();
int test_sira_yedegi();
int test_guvence_kisitlari();
int test_guncelleme();
int test_surum_kurallari();
int test_parca_hedefi();
int test_olcum_telemetri();
int test_cokme_kurtarma(const char* yazan_program);
int test_dok_programi(const char* dok_programi);
}  // namespace ida::kayit::test

namespace {

struct Kayit {
    const char* ad;
    int (*islev)();
};

}  // namespace

int main(int argc, char** argv)
{
    using namespace ida::kayit::test;
    static const Kayit kTestler[] = {
        {"crc32", test_crc32},
        {"halka", test_halka},
        {"dusme_yuvasi", test_dusme_yuvasi},
        {"guvence", test_guvence},
        {"ust_veri_denetimi", test_ust_veri_denetimi},
        {"coz_surum_kurali", test_coz_surum_kurali},
        {"katalog_tablosu", test_katalog_tablosu},
        {"belge_tablosu", test_belge_tablosu},
        {"kodlayici_ortak", test_kodlayici_ortak},
        {"coz_gidis_donus", test_coz_gidis_donus},
        {"bicim", test_bicim},
        {"gidis_donus", test_gidis_donus},
        {"zaman_alanlari", test_zaman_alanlari},
        {"sira_karisik", test_sira_karisik},
        {"kademe", test_kademe},
        {"kademe_suzgeci", test_kademe_suzgeci},
        {"uretici_beklemez", test_uretici_beklemez},
        {"uretici_ayirmaz", test_uretici_ayirmaz},
        {"eksik_sayim", test_eksik_sayim},
        {"es_zamanli_ureticiler", test_es_zamanli_ureticiler},
        {"kurtarma_kesik", test_kurtarma_kesik},
        {"parca_suresi", test_parca_suresi},
        {"bozulma_parca", test_bozulma_parca},
        {"bozulma_ek_ust_veri", test_bozulma_ek_ust_veri},
        {"ust_veri_birlestirme", test_ust_veri_birlestirme},
        {"rastgele_bozulma", test_rastgele_bozulma},
        {"dosya_ezilmez", test_dosya_ezilmez},
        {"negatif_zaman", test_negatif_zaman},
        {"sira_siniri", test_sira_siniri},
        {"kademe_degismez", test_kademe_degismez},
        {"kapanis", test_kapanis},
        {"hata_kapanisi", test_hata_kapanisi},
        {"erken_kapanis_cok_uretici", test_erken_kapanis_cok_uretici},
        {"kapat_sirasinda_hata", test_kapat_sirasinda_hata},
        {"guncelleme_teslim_hatasi", test_guncelleme_teslim_hatasi},
        {"sira_yedegi", test_sira_yedegi},
        {"guvence_kisitlari", test_guvence_kisitlari},
        {"guncelleme", test_guncelleme},
        {"surum_kurallari", test_surum_kurallari},
        {"parca_hedefi", test_parca_hedefi},
        {"olcum_telemetri", test_olcum_telemetri},
    };
    if (argc < 2) {
        std::printf("kullanım: ida_kayit_test <test_adı>\n");
        return 2;
    }
    const std::string ad = argv[1];
    int sonuc = -1;
    if (ad == "cokme_kurtarma" || ad == "dok_programi") {
        if (argc < 3) {
            std::printf("%s: yardımcı programın yolu gerekir\n", ad.c_str());
            return 2;
        }
        sonuc = ad == "cokme_kurtarma" ? test_cokme_kurtarma(argv[2]) : test_dok_programi(argv[2]);
    } else {
        for (const Kayit& k : kTestler) {
            if (ad == k.ad) {
                sonuc = k.islev();
                break;
            }
        }
        if (sonuc == -1) {
            std::printf("bilinmeyen test: %s\n", ad.c_str());
            return 2;
        }
    }
    std::printf("%s: %s\n", ad.c_str(), sonuc == 0 ? "GEÇTİ" : "KALDI");
    return sonuc == 0 ? 0 : 1;
}
