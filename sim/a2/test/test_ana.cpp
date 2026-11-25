// test_ana.cpp — sim/a2 testlerinin giriş noktası: `ida_a2_test <test_adı> [bağımsız değişken]`
// (CTest: sim.a2.<ad>).
#include <cstdio>
#include <cstring>
#include <string>

namespace ida::a2::test {
int test_sha256();
int test_takvim_birim();
int test_gelen_okuyucu();
int test_tel_kodek();
int test_tel_sira_ve_yuk();
int test_uctan_uca_el_sikisma();
int test_uctan_uca_durum_akisi();
int test_uctan_uca_sinir();
int test_protokol_baslik_hatalari();
int test_protokol_sira();
int test_protokol_ileti_sirasi();
int test_el_sikisma_retleri();
int test_zaman_asimlari();
int test_takvim_bir_saat();
int test_seyreltme_sil();
int test_takvim_duraklat_sifirla();
int test_nabiz();
int test_yeniden_baglanma();
int test_ortada_baglanma();
int test_ortada_baglanma_eszamanli();
int test_nesil_yeni_calistirma();
int test_nesil_sifirlama_duraklatma();
int test_yayinci_sureklilik();
int test_yayinci_gecersiz_cagri();
int test_yayinci_tasma();
int test_yayinci_kume_degisimi();
int test_yayinci_a2siz();
int test_son_deger();
int test_halka();
int test_kesit_halka_degismezi();
int test_yavas_istemci();
int test_yayin_suresi();
int test_donuk_istemci_soket();
int test_soket_uctan_uca();
int test_soket_kapaniyor();
int test_ikinci_baglanti();
int test_port_dolu();
int test_yalniz_dongu_arabirimi();
int test_kilit();
int test_kilit_surecler(const char* sahte_program);
int test_rastgele_girdi();
int test_rastgele_kaynak();
}  // namespace ida::a2::test

namespace {

struct Kayit {
    const char* ad;
    int (*islev)();
};

}  // namespace

int main(int argc, char** argv)
{
    using namespace ida::a2::test;
    static const Kayit kTestler[] = {
        {"sha256", test_sha256},
        {"takvim_birim", test_takvim_birim},
        {"gelen_okuyucu", test_gelen_okuyucu},
        {"tel_kodek", test_tel_kodek},
        {"tel_sira_ve_yuk", test_tel_sira_ve_yuk},
        {"uctan_uca_el_sikisma", test_uctan_uca_el_sikisma},
        {"uctan_uca_durum_akisi", test_uctan_uca_durum_akisi},
        {"uctan_uca_sinir", test_uctan_uca_sinir},
        {"protokol_baslik_hatalari", test_protokol_baslik_hatalari},
        {"protokol_sira", test_protokol_sira},
        {"protokol_ileti_sirasi", test_protokol_ileti_sirasi},
        {"el_sikisma_retleri", test_el_sikisma_retleri},
        {"zaman_asimlari", test_zaman_asimlari},
        {"takvim_bir_saat", test_takvim_bir_saat},
        {"seyreltme_sil", test_seyreltme_sil},
        {"takvim_duraklat_sifirla", test_takvim_duraklat_sifirla},
        {"nabiz", test_nabiz},
        {"yeniden_baglanma", test_yeniden_baglanma},
        {"ortada_baglanma", test_ortada_baglanma},
        {"ortada_baglanma_eszamanli", test_ortada_baglanma_eszamanli},
        {"nesil_yeni_calistirma", test_nesil_yeni_calistirma},
        {"nesil_sifirlama_duraklatma", test_nesil_sifirlama_duraklatma},
        {"yayinci_sureklilik", test_yayinci_sureklilik},
        {"yayinci_gecersiz_cagri", test_yayinci_gecersiz_cagri},
        {"yayinci_tasma", test_yayinci_tasma},
        {"yayinci_kume_degisimi", test_yayinci_kume_degisimi},
        {"yayinci_a2siz", test_yayinci_a2siz},
        {"son_deger", test_son_deger},
        {"halka", test_halka},
        {"kesit_halka_degismezi", test_kesit_halka_degismezi},
        {"yavas_istemci", test_yavas_istemci},
        {"yayin_suresi", test_yayin_suresi},
        {"donuk_istemci_soket", test_donuk_istemci_soket},
        {"soket_uctan_uca", test_soket_uctan_uca},
        {"soket_kapaniyor", test_soket_kapaniyor},
        {"ikinci_baglanti", test_ikinci_baglanti},
        {"port_dolu", test_port_dolu},
        {"yalniz_dongu_arabirimi", test_yalniz_dongu_arabirimi},
        {"kilit", test_kilit},
        {"rastgele_girdi", test_rastgele_girdi},
        {"rastgele_kaynak", test_rastgele_kaynak},
    };
    if (argc < 2) {
        std::printf("kullanım: ida_a2_test <test_adı> [bağımsız değişken]\n");
        for (const Kayit& k : kTestler) {
            std::printf("  %s\n", k.ad);
        }
        std::printf("  kilit_surecler <ida_a2_sahte yolu>\n");
        return 2;
    }
    const std::string ad = argv[1];
    int sonuc = -1;
    if (ad == "kilit_surecler") {
        sonuc = test_kilit_surecler(argc >= 3 ? argv[2] : nullptr);
    } else {
        for (const Kayit& k : kTestler) {
            if (ad == k.ad) {
                sonuc = k.islev();
                break;
            }
        }
    }
    if (sonuc < 0) {
        std::printf("bilinmeyen test: %s\n", ad.c_str());
        return 2;
    }
    std::printf("%s: %s (%d hata)\n", ad.c_str(), sonuc == 0 ? "GEÇTİ" : "KALDI", sonuc);
    return sonuc == 0 ? 0 : 1;
}
