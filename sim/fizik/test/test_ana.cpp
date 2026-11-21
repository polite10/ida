// sim/fizik test koşucusu: `ida_fizik_test <ad> [ida0.toml yolu]`.
#include "test_destek.hpp"

#include "itici.hpp"
#include "matematik.hpp"

#include <cstring>

namespace ida::fizik::test {

int test_kutle_durumlari(const TestBaglami&);
int test_ida0_toml_esitligi(const TestBaglami&);
int test_hidrostatik_denge(const TestBaglami&);
int test_serbest_sonum(const TestBaglami&);
int test_son_hiz(const TestBaglami&);
int test_enerji(const TestBaglami&);
int test_adim_yarilama(const TestBaglami&);
int test_belirlenimcilik(const TestBaglami&);
int test_itici_geometrisi(const TestBaglami&);
int test_uzun_kosu(const TestBaglami&);
int test_sure(const TestBaglami&);
int test_itici_modeli(const TestBaglami&);
int test_komut_zamanlama(const TestBaglami&);
int test_akinti(const TestBaglami&);
int test_kurulum_hatalari(const TestBaglami&);
int test_dalis_ve_yuzeye_cikis(const TestBaglami&);
int test_itici_hizli_filtre(const TestBaglami&);
int test_esc_notr_disi_hazir_olmaz(const TestBaglami&);
int test_esc_notr_bekleme_sonrasi_hazir(const TestBaglami&);
int test_kurulum_sonluluk(const TestBaglami&);
int test_transfer_olcek(const TestBaglami&);

bool itkiden_pwm(const AracParametreleri& a, double F, double& pwm)
{
    ic::IticiModeliIc m;
    if (!m.kur(a.itici_modeli, a.esc).empty()) {
        return false;
    }
    const double notr = static_cast<double>(a.esc.pwm_notr_us);
    if (F == 0.0) {
        pwm = notr;
        return true;
    }
    const bool ileri = F > 0.0;
    const double hedef = std::abs(F) / m.azami_itki(a.batarya.v_sabit, ileri);
    if (!(hedef <= m.sekil(1.0))) {
        return false;
    }
    double lo = 0.0, hi = 1.0;
    for (int i = 0; i < 200; ++i) {
        const double orta = 0.5 * (lo + hi);
        if (orta <= lo || orta >= hi) {
            break;
        }
        if (m.sekil(orta) < hedef) {
            lo = orta;
        } else {
            hi = orta;
        }
    }
    const double u = hi;
    const double aralik = ileri ? static_cast<double>(a.esc.pwm_max_us - a.esc.pwm_notr_us)
                                : static_cast<double>(a.esc.pwm_notr_us - a.esc.pwm_min_us);
    if (u * aralik <= static_cast<double>(a.esc.olu_bant_us)) {
        return false;  // ölü bantta: elde edilemez
    }
    pwm = ileri ? notr + u * aralik : notr - u * aralik;
    return true;
}

bool tahsis_pwm(const AracParametreleri& a, const Vek6& tau, std::array<double, kIticiSayisi>& pwm,
                std::array<double, kIticiSayisi>* itki)
{
    Eigen::Matrix<double, 6, kIticiSayisi> B;
    for (int i = 0; i < kIticiSayisi; ++i) {
        const auto& t = a.iticiler[static_cast<std::size_t>(i)];
        const ic::V3 r = ic::v3(t.konum);
        const ic::V3 e = ic::v3(t.yon).normalized();
        B.col(i) << e, r.cross(e);
    }
    ic::V6 tv;
    for (int i = 0; i < 6; ++i) {
        tv(i) = tau[static_cast<std::size_t>(i)];
    }
    const Eigen::Matrix<double, 6, 6> BBt = B * B.transpose();
    Eigen::Matrix<double, kIticiSayisi, 1> f = B.transpose() * BBt.ldlt().solve(tv);
    // Simetriden sıfır olması gereken bileşenler yuvarlama düzeyinde kalır (≈ 1e-16 N): sıfırla.
    const double olcek = f.cwiseAbs().maxCoeff();
    for (int i = 0; i < kIticiSayisi; ++i) {
        if (std::abs(f(i)) <= 1e-12 * olcek) f(i) = 0.0;
    }
    for (int i = 0; i < kIticiSayisi; ++i) {
        const auto& t = a.iticiler[static_cast<std::size_t>(i)];
        double p = 0.0;
        if (!itkiden_pwm(a, f(i), p)) {
            std::printf("  tahsis: itici %d için itki %.6g N elde edilemiyor\n", i, f(i));
            return false;
        }
        pwm[static_cast<std::size_t>(t.kanal)] = p;
        if (itki != nullptr) {
            (*itki)[static_cast<std::size_t>(t.kanal)] = f(i);
        }
    }
    return true;
}

}  // namespace ida::fizik::test

int main(int argc, char** argv)
{
    using namespace ida::fizik::test;
    struct Kayit {
        const char* ad;
        int (*islev)(const TestBaglami&);
    };
    const Kayit testler[] = {
        {"kutle_durumlari", test_kutle_durumlari},
        {"ida0_toml_esitligi", test_ida0_toml_esitligi},
        {"hidrostatik_denge", test_hidrostatik_denge},
        {"serbest_sonum", test_serbest_sonum},
        {"son_hiz", test_son_hiz},
        {"enerji", test_enerji},
        {"adim_yarilama", test_adim_yarilama},
        {"belirlenimcilik", test_belirlenimcilik},
        {"itici_geometrisi", test_itici_geometrisi},
        {"uzun_kosu", test_uzun_kosu},
        {"sure", test_sure},
        {"itici_modeli", test_itici_modeli},
        {"komut_zamanlama", test_komut_zamanlama},
        {"akinti", test_akinti},
        {"kurulum_hatalari", test_kurulum_hatalari},
        {"dalis_ve_yuzeye_cikis", test_dalis_ve_yuzeye_cikis},
        {"itici_hizli_filtre", test_itici_hizli_filtre},
        {"esc_notr_disi_hazir_olmaz", test_esc_notr_disi_hazir_olmaz},
        {"esc_notr_bekleme_sonrasi_hazir", test_esc_notr_bekleme_sonrasi_hazir},
        {"kurulum_sonluluk", test_kurulum_sonluluk},
        {"transfer_olcek", test_transfer_olcek},
    };
    if (argc < 2) {
        std::printf("kullanım: %s <test> [ida0.toml]\n", argv[0]);
        return 2;
    }
    TestBaglami b;
    if (argc >= 3) {
        b.ida0_toml_yolu = argv[2];
    }
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
