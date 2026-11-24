// test_ana.cpp — sim/a3 testlerinin giriş noktası: `ida_a3_test <test_adı>` (CTest: sim.a3.<ad>).
//   ida_a3_test uyum_ornekleri <dosya>   web çözücü uyum testi için örnek çerçeveleri yazar
#include "soket.hpp"

#include <cstdio>
#include <cstring>
#include <map>
#include <string>

namespace ida::a3::test {
int test_json();
int test_desen();
int test_sema();
int test_ozet();
int test_http_ayristirma();
int test_ws_cerceve();
int test_halka();
int test_katalog_tablosu();
int test_kodlayici();
int test_dongu_disi_adres();
int test_erisim_http();
int test_erisim_ws();
int test_erisim_olayi_hiz_siniri();
int test_statik_simgesel_bag();
int test_merhaba_ve_zorunlu();
int test_abonelik_ve_ozet();
int test_seyreltme_hizi();
int test_olay_gecmisi();
int test_ws_kapanis_kodlari();
int test_canlilik();
int test_olay_kuyrugu_tasmasi();
int test_kapanis_1001();
int test_calistirma_akisi();
int test_yineleme();
int test_govde_sinirlari();
int test_emniyet_kisitlayan();
int test_emniyet_gevseten();
int test_sonuc_aynen();
int test_kuyruk_dolu_kisitlayan();
int test_yer_istasyonu();
int test_kisitlayan_bayat_run_id();
int test_kisitlayan_mandal_siniri();
int test_jeton_erken_ret();
int test_yavas_istemci();
int test_yavas_istemci_ret_duser();
int test_rastgele_cekirdek();
int test_rastgele_ayristirici();
int test_cok_baglanti();
int test_hz_aralik_disi();
int test_govde_okuma_butcesi();
int test_pipeline_baski();
int test_head_sonra_get();
int test_chunked_bozuk();
int test_ws_en_kisa_uzunluk();
int test_baslatma_hatasi();
int test_uyandirma_eslesmesi();
int test_olay_gecmisi_cok_bayt();
int test_baglanti_siniri_emniyet();
int test_baski_altinda_emniyet();
int test_baski_altinda_emniyet_soket();
int test_jeton_erken_ret_tablo();
int test_statik_harf_duyarli();
int test_baglanti_siniri_dogrulanmis();
int test_baglanti_siniri_503();
int test_baski_uzun_kisitlayan();
int test_baski_chunked_eof();
int test_baski_sira_tasmasi();
int test_jeton_okunamayan();
int test_statik_kok_tutulur();
int test_yer_acma_okuma_firsati();
int test_yer_acma_soket();
int test_cikis_tavani_dogrulanmamis();
int test_jeton_istemci_kapsami();
int uyum_ornekleri_yaz(const char* yol);
}  // namespace ida::a3::test

int main(int argc, char** argv)
{
    using namespace ida::a3::test;
    if (argc == 3 && std::strcmp(argv[1], "uyum_ornekleri") == 0) {
        return uyum_ornekleri_yaz(argv[2]);
    }
    const std::map<std::string, int (*)()> testler = {
        {"json", test_json},
        {"desen", test_desen},
        {"sema", test_sema},
        {"ozet", test_ozet},
        {"http_ayristirma", test_http_ayristirma},
        {"ws_cerceve", test_ws_cerceve},
        {"halka", test_halka},
        {"katalog_tablosu", test_katalog_tablosu},
        {"kodlayici", test_kodlayici},
        {"dongu_disi_adres", test_dongu_disi_adres},
        {"erisim_http", test_erisim_http},
        {"erisim_ws", test_erisim_ws},
        {"erisim_olayi_hiz_siniri", test_erisim_olayi_hiz_siniri},
        {"statik_simgesel_bag", test_statik_simgesel_bag},
        {"merhaba_ve_zorunlu", test_merhaba_ve_zorunlu},
        {"abonelik_ve_ozet", test_abonelik_ve_ozet},
        {"seyreltme_hizi", test_seyreltme_hizi},
        {"olay_gecmisi", test_olay_gecmisi},
        {"ws_kapanis_kodlari", test_ws_kapanis_kodlari},
        {"canlilik", test_canlilik},
        {"olay_kuyrugu_tasmasi", test_olay_kuyrugu_tasmasi},
        {"kapanis_1001", test_kapanis_1001},
        {"calistirma_akisi", test_calistirma_akisi},
        {"yineleme", test_yineleme},
        {"govde_sinirlari", test_govde_sinirlari},
        {"emniyet_kisitlayan", test_emniyet_kisitlayan},
        {"emniyet_gevseten", test_emniyet_gevseten},
        {"sonuc_aynen", test_sonuc_aynen},
        {"kuyruk_dolu_kisitlayan", test_kuyruk_dolu_kisitlayan},
        {"yer_istasyonu", test_yer_istasyonu},
        {"kisitlayan_bayat_run_id", test_kisitlayan_bayat_run_id},
        {"kisitlayan_mandal_siniri", test_kisitlayan_mandal_siniri},
        {"jeton_erken_ret", test_jeton_erken_ret},
        {"yavas_istemci", test_yavas_istemci},
        {"yavas_istemci_ret_duser", test_yavas_istemci_ret_duser},
        {"rastgele_cekirdek", test_rastgele_cekirdek},
        {"rastgele_ayristirici", test_rastgele_ayristirici},
        {"cok_baglanti", test_cok_baglanti},
        {"yer_acma_okuma_firsati", test_yer_acma_okuma_firsati},
        {"yer_acma_soket", test_yer_acma_soket},
        {"cikis_tavani_dogrulanmamis", test_cikis_tavani_dogrulanmamis},
        {"jeton_istemci_kapsami", test_jeton_istemci_kapsami},
        {"baglanti_siniri_dogrulanmis", test_baglanti_siniri_dogrulanmis},
        {"baglanti_siniri_503", test_baglanti_siniri_503},
        {"baski_uzun_kisitlayan", test_baski_uzun_kisitlayan},
        {"baski_chunked_eof", test_baski_chunked_eof},
        {"baski_sira_tasmasi", test_baski_sira_tasmasi},
        {"jeton_okunamayan", test_jeton_okunamayan},
        {"statik_kok_tutulur", test_statik_kok_tutulur},
        {"baglanti_siniri_emniyet", test_baglanti_siniri_emniyet},
        {"baski_altinda_emniyet", test_baski_altinda_emniyet},
        {"baski_altinda_emniyet_soket", test_baski_altinda_emniyet_soket},
        {"jeton_erken_ret_tablo", test_jeton_erken_ret_tablo},
        {"statik_harf_duyarli", test_statik_harf_duyarli},
        {"hz_aralik_disi", test_hz_aralik_disi},
        {"govde_okuma_butcesi", test_govde_okuma_butcesi},
        {"pipeline_baski", test_pipeline_baski},
        {"head_sonra_get", test_head_sonra_get},
        {"chunked_bozuk", test_chunked_bozuk},
        {"ws_en_kisa_uzunluk", test_ws_en_kisa_uzunluk},
        {"baslatma_hatasi", test_baslatma_hatasi},
        {"uyandirma_eslesmesi", test_uyandirma_eslesmesi},
        {"olay_gecmisi_cok_bayt", test_olay_gecmisi_cok_bayt},
    };
    if (argc != 2 || testler.count(argv[1]) == 0) {
        std::fprintf(stderr, "kullanım: ida_a3_test <test>\n");
        for (const auto& [ad, f] : testler) {
            (void)f;
            std::fprintf(stderr, "  %s\n", ad.c_str());
        }
        return 2;
    }
    ida::a3::ag::baslat();
    const int hata = testler.at(argv[1])();
    std::printf("%s: %s (%d hata)\n", argv[1], hata == 0 ? "GEÇTİ" : "KALDI", hata);
    return hata == 0 ? 0 : 1;
}
