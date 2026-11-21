// sim/senaryo test koşucusu: `ida_senaryo_test <ad> <depo kökü>`.
#include "test_destek.hpp"

#include <cstring>

namespace ida::senaryo::test {
int test_sha256_vektorleri(const TestBaglami&);
int test_sayi_dizilimi(const TestBaglami&);
int test_k96_vektorleri(const TestBaglami&);
int test_altin_dosya(const TestBaglami&);
int test_ek_e_suruklenme(const TestBaglami&);
int test_belge_tutarliligi(const TestBaglami&);
int test_sema_sozcukleri(const TestBaglami&);
int test_bozuk_dosyalar(const TestBaglami&);
int test_anlamsal_kurallar(const TestBaglami&);
int test_kosullu_varsayilanlar(const TestBaglami&);
int test_a7_atiflari(const TestBaglami&);
int test_ozet_degismezligi(const TestBaglami&);
int test_yeniden_yukleme(const TestBaglami&);
int test_rapor_bicimi(const TestBaglami&);
int test_dayaniklilik(const TestBaglami&);
int test_cozumleme_suresi(const TestBaglami&);
int test_fizik_yukleyici(const TestBaglami&);
int test_ek_a_vektorleri(const TestBaglami&);
int test_fizik_kurallari(const TestBaglami&);
int test_fizik_karsilastirma(const TestBaglami&);
}  // namespace ida::senaryo::test

int main(int argc, char** argv)
{
    using namespace ida::senaryo::test;
    struct Kayit {
        const char* ad;
        int (*islev)(const TestBaglami&);
    };
    const Kayit testler[] = {
        {"sha256_vektorleri", test_sha256_vektorleri},
        {"sayi_dizilimi", test_sayi_dizilimi},
        {"k96_vektorleri", test_k96_vektorleri},
        {"altin_dosya", test_altin_dosya},
        {"ek_e_suruklenme", test_ek_e_suruklenme},
        {"belge_tutarliligi", test_belge_tutarliligi},
        {"sema_sozcukleri", test_sema_sozcukleri},
        {"bozuk_dosyalar", test_bozuk_dosyalar},
        {"anlamsal_kurallar", test_anlamsal_kurallar},
        {"kosullu_varsayilanlar", test_kosullu_varsayilanlar},
        {"a7_atiflari", test_a7_atiflari},
        {"ozet_degismezligi", test_ozet_degismezligi},
        {"yeniden_yukleme", test_yeniden_yukleme},
        {"rapor_bicimi", test_rapor_bicimi},
        {"dayaniklilik", test_dayaniklilik},
        {"cozumleme_suresi", test_cozumleme_suresi},
        {"fizik_yukleyici", test_fizik_yukleyici},
        {"ek_a_vektorleri", test_ek_a_vektorleri},
        {"fizik_kurallari", test_fizik_kurallari},
        {"fizik_karsilastirma", test_fizik_karsilastirma},
    };
    if (argc < 3) {
        std::printf("kullanım: %s <test> <depo kökü>\n", argv[0]);
        return 2;
    }
    TestBaglami b;
    b.kok = argv[2];
    for (const auto& t : testler) {
        if (std::strcmp(t.ad, argv[1]) == 0) {
            std::printf("[%s]\n", t.ad);
            const int r = t.islev(b);
            std::printf("[%s] %s\n", t.ad, r == 0 ? "GEÇTİ" : "KALDI");
            return r == 0 ? 0 : 1;
        }
    }
    std::printf("bilinmeyen test: %s\n", argv[1]);
    return 2;
}
