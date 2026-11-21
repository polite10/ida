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
int test_bellek_tasima();
int test_tcp_tasima();
int test_pty_tasima();

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
    {"bellek_tasima", test_bellek_tasima},
    {"tcp_tasima", test_tcp_tasima},
    {"pty_tasima", test_pty_tasima},
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
