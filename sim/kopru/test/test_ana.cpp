// sim.kopru.* testlerinin girişi: `ida_kopru_test <ad>`.
#include <cstdio>
#include <cstring>

int test_d7_vektorleri();
int test_el_sikisma();
int test_on_bin_parti();
int test_kopma_yeniden();
int test_yanlis_dialect();
int test_yuva_zamanlama();
int test_eszamansiz_guvenli();
int test_olay_izin_yok();
int test_olay_yinelenen();
int test_olay_suresi_gecmis();
int test_olay_fire_kabul();
int test_emniyet_ornegi();
int test_sil_kisit_erken();
int test_tcp_kapat_duzgun_sinirli();
int test_canlilik_olay_yanit_ret();
int test_canlilik_eksik_kume();
int test_canlilik_ret_yineleme();
int test_canlilik_bekleyen_yineleme();
int test_canlilik_dogrulanan_kume();
int test_canlilik_aninda();
int test_ret_eden_olay_canlilik();
int test_gec_kapanis_tamamlamaz();
int test_emniyet_gecmis_tahliyesi();
int test_sayim_ihlali_sonradan_fire();
int test_sayim_ihlali_kucuk_sayi();
int test_sayim_ihlali_yanittan_once();
int test_tam_yuvaya_yineleme();
int test_sayiyi_asan_kapanis();
int test_hata_donusu_canlilik();
int test_eksik_kume_suresi();
int test_bekleyen_kume_siniri();
int test_emniyet_rev_ileri();
int test_reddedilen_yanit_gecmis();
int test_eksik_kume_tutarsiz();
int test_eksik_kume_gelmez();
int test_eksik_kume_tutarli();
int test_tutarsiz_yanit_canlilik();
int test_yazma_sinirli();
int test_aygit_kalkti();
int test_sim_gecikmesi();
int test_parti_butcesi();
int test_daraltici_kovasi();
int test_batch_seq_sarmasi();
int test_sil_tampon_sonraki_act();
int test_sil_tampon_kapanis();
int test_sil_tampon_siniri();
int test_sira_tanisi();
int test_emniyet_capraz_alan();
int test_emniyet_izin_acik();
int test_yinelenen_yanit_olay();
int test_toplu_tutarlilik();
int test_yeni_acilis_out_seq();
int test_gecersiz_cerceve_etkisiz();
int test_eszamansiz_ilk_parti();
int test_emniyet_rev_durum();
int test_bellek_tasima();
int test_tcp_tasima();
int test_pty_tasima();
int test_seri_cikis_bosalt();
int test_operator_emniyet();
int test_operator_ve_senaryo();
int test_operator_artimli();
int test_esz_sinirli();
int test_esz_ilk_parti_oncesi();

namespace {

struct Test {
    const char* ad;
    int (*f)();
};

const Test kTestler[] = {
    {"d7_vektorleri", test_d7_vektorleri},
    {"el_sikisma", test_el_sikisma},
    {"on_bin_parti", test_on_bin_parti},
    {"kopma_yeniden", test_kopma_yeniden},
    {"yanlis_dialect", test_yanlis_dialect},
    {"yuva_zamanlama", test_yuva_zamanlama},
    {"eszamansiz_guvenli", test_eszamansiz_guvenli},
    {"olay_izin_yok", test_olay_izin_yok},
    {"olay_yinelenen", test_olay_yinelenen},
    {"olay_suresi_gecmis", test_olay_suresi_gecmis},
    {"olay_fire_kabul", test_olay_fire_kabul},
    {"emniyet_ornegi", test_emniyet_ornegi},
    {"sil_kisit_erken", test_sil_kisit_erken},
    {"tcp_kapat_duzgun_sinirli", test_tcp_kapat_duzgun_sinirli},
    {"canlilik_olay_yanit_ret", test_canlilik_olay_yanit_ret},
    {"canlilik_eksik_kume", test_canlilik_eksik_kume},
    {"canlilik_ret_yineleme", test_canlilik_ret_yineleme},
    {"canlilik_bekleyen_yineleme", test_canlilik_bekleyen_yineleme},
    {"canlilik_dogrulanan_kume", test_canlilik_dogrulanan_kume},
    {"canlilik_aninda", test_canlilik_aninda},
    {"ret_eden_olay_canlilik", test_ret_eden_olay_canlilik},
    {"gec_kapanis_tamamlamaz", test_gec_kapanis_tamamlamaz},
    {"emniyet_gecmis_tahliyesi", test_emniyet_gecmis_tahliyesi},
    {"sayim_ihlali_sonradan_fire", test_sayim_ihlali_sonradan_fire},
    {"sayim_ihlali_kucuk_sayi", test_sayim_ihlali_kucuk_sayi},
    {"sayim_ihlali_yanittan_once", test_sayim_ihlali_yanittan_once},
    {"tam_yuvaya_yineleme", test_tam_yuvaya_yineleme},
    {"sayiyi_asan_kapanis", test_sayiyi_asan_kapanis},
    {"hata_donusu_canlilik", test_hata_donusu_canlilik},
    {"eksik_kume_suresi", test_eksik_kume_suresi},
    {"bekleyen_kume_siniri", test_bekleyen_kume_siniri},
    {"emniyet_rev_ileri", test_emniyet_rev_ileri},
    {"reddedilen_yanit_gecmis", test_reddedilen_yanit_gecmis},
    {"eksik_kume_tutarsiz", test_eksik_kume_tutarsiz},
    {"eksik_kume_gelmez", test_eksik_kume_gelmez},
    {"eksik_kume_tutarli", test_eksik_kume_tutarli},
    {"tutarsiz_yanit_canlilik", test_tutarsiz_yanit_canlilik},
    {"yazma_sinirli", test_yazma_sinirli},
    {"aygit_kalkti", test_aygit_kalkti},
    {"sim_gecikmesi", test_sim_gecikmesi},
    {"parti_butcesi", test_parti_butcesi},
    {"daraltici_kovasi", test_daraltici_kovasi},
    {"batch_seq_sarmasi", test_batch_seq_sarmasi},
    {"sil_tampon_sonraki_act", test_sil_tampon_sonraki_act},
    {"sil_tampon_kapanis", test_sil_tampon_kapanis},
    {"sil_tampon_siniri", test_sil_tampon_siniri},
    {"sira_tanisi", test_sira_tanisi},
    {"emniyet_capraz_alan", test_emniyet_capraz_alan},
    {"emniyet_izin_acik", test_emniyet_izin_acik},
    {"yinelenen_yanit_olay", test_yinelenen_yanit_olay},
    {"toplu_tutarlilik", test_toplu_tutarlilik},
    {"yeni_acilis_out_seq", test_yeni_acilis_out_seq},
    {"gecersiz_cerceve_etkisiz", test_gecersiz_cerceve_etkisiz},
    {"eszamansiz_ilk_parti", test_eszamansiz_ilk_parti},
    {"emniyet_rev_durum", test_emniyet_rev_durum},
    {"bellek_tasima", test_bellek_tasima},
    {"tcp_tasima", test_tcp_tasima},
    {"pty_tasima", test_pty_tasima},
    {"seri_cikis_bosalt", test_seri_cikis_bosalt},
    {"operator_emniyet", test_operator_emniyet},
    {"operator_ve_senaryo", test_operator_ve_senaryo},
    {"operator_artimli", test_operator_artimli},
    {"esz_sinirli", test_esz_sinirli},
    {"esz_ilk_parti_oncesi", test_esz_ilk_parti_oncesi},
};

}  // namespace

int main(int argc, char** argv)
{
    int hata = 0;
    int kosulan = 0;
    for (const Test& t : kTestler) {
        if (argc > 1 && std::strcmp(argv[1], t.ad) != 0) {
            continue;
        }
        ++kosulan;
        const int r = t.f();
        std::printf("%s %s\n", r == 0 ? "GECTI" : "KALDI", t.ad);
        hata += r != 0 ? 1 : 0;
    }
    if (kosulan == 0) {
        std::printf("bilinmeyen test: %s\n", argc > 1 ? argv[1] : "");
        return 2;
    }
    return hata == 0 ? 0 : 1;
}
