// ida_z2 (W4-z2): Z1 senaryosunu `ida_sim` kitaplığıyla (ida_sunucu) koşar ve sonucu W3-zincir
// sınırlarıyla (tests/zincir/test_zincir.cpp ve tests/butunleme/test_butunleme.cpp ile aynı
// değerler) değerlendirir. Kipler:
//
//   ida_z2 hil --seri <aygıt> [--sure s] [--tohum n] [--acil s --acil-kalk s] [--csv yol] [--json yol]
//   ida_z2 sil [--sure s] [--tohum n] [--acil s --acil-kalk s] [--json yol]     (süreç içi karşı taraf)
//
// Çıktı: insan okunur özet (stdout) ve isteğe bağlı JSON (ölçüm raporu için). Sınır ya da kazanç
// değiştirmez; yalnız ölçer. İki ayrı hüküm verir: işlevsel ölçütler ve A1-K79 zamanlama
// geçerliliği (geçersizlik bitleri 0x0). Çıkış kodu: 0 ikisi de geçti, 3 işlevsel kaldı, 4 işlevsel
// geçti ama zamanlama geçersiz, 5 ikisi de kaldı, 1 kullanım hatası. CTest'e girmez; zamanlama
// hükmü yerelde ve kartta ölçüttür (W4-K24 yalnız paylaşılan CI koşucusu için). HIL koşusu kartı ve gerçek zamanı kullanır (ağır yük altında
// sayılar ön değerdir).
#include "ida/port.h"
#include "ida/sunucu/sunucu.hpp"
#include "zincir.hpp"

#include <algorithm>
#include <cerrno>
#include <cmath>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>
#include <vector>

namespace {

// W3-zincir sınırları (Z1 varsayılanı t5 + t1 arızası).
constexpr double kDalisOturma = 20.0;
constexpr double kDalisAsim = 0.15;
constexpr double kManevraAsim = 0.15;
constexpr double kYonAsim = 0.10;
constexpr double kManevraYonOturma = 15.0;
constexpr double kTutumArizasiz = 0.05;
constexpr double kTutumArizali = 0.25;
constexpr double kTutumToparlanma = 15.0;
constexpr double kTutmaDerinlik = 0.05;
constexpr double kTutmaYon = 0.05;
constexpr unsigned kEtkinAdimAz = 200U;

struct Olcu {
    std::string ad;
    double deger;
    std::string kosul;
    bool gecti;
};

std::vector<Olcu> g_olcu;

void olcu(const std::string& ad, double deger, const std::string& kosul, bool gecti)
{
    g_olcu.push_back({ad, deger, kosul, gecti});
}

ida::zincir::Ayar zincir_ayari(const ida::sunucu::Ayar& a)
{
    ida::zincir::Ayar z;
    z.sure_s = a.sure_s;
    z.t_arm_s = a.z1.t_arm_s;
    z.t_kip_s = a.z1.t_kip_s;
    z.t_sp_s = a.z1.t_sp_s;
    z.t_manevra_s = a.z1.t_manevra_s;
    z.t_disarm_s = a.z1.t_disarm_s;
    z.ariza_kanallari = a.z1.ariza_kanallari;
    z.ariza_carpani = a.z1.ariza_carpani;
    return z;
}

ida::zincir::Sonuc zincir_sonucu(const ida::sunucu::Sonuc& s)
{
    ida::zincir::Sonuc z;
    z.kurulum_tamam = s.tamam;
    z.hata = s.hata;
    z.denge_derinligi_m = s.denge_derinligi_m;
    z.arm_ack = s.arm_ack;
    z.arm_neden = s.arm_neden;
    z.kip_ack = s.kip_ack;
    z.sp_ack = s.sp_ack;
    z.sp2_ack = s.sp2_ack;
    z.disarm_ack = s.disarm_ack;
    z.t_disarm_ns = s.t_disarm_ns;
    z.t_pwm_notr_ns = s.t_pwm_notr_ns;
    z.t_itki_sifir_ns = s.t_itki_sifir_ns;
    for (const ida::sunucu::Satir& r : s.satirlar) {
        ida::zincir::Satir y;
        y.t_s = r.t_s;
        y.derinlik = r.derinlik;
        y.derinlik_k = r.derinlik_k;
        y.yon = r.yon;
        y.yon_k = r.yon_k;
        y.yalpa = r.yalpa;
        y.yalpa_k = r.yalpa_k;
        y.bas_kic = r.bas_kic;
        y.bas_kic_k = r.bas_kic_k;
        for (std::size_t i = 0; i < 8; ++i) {
            y.pwm_uret[i] = r.pwm_uret[i];
            y.pwm_yur[i] = r.pwm_yur[i];
            y.pwm_fiz[i] = r.pwm_fiz[i];
            y.thr[i] = r.thr_valid != 0u ? static_cast<float>((r.pwm_uret[i] - 1500) / 400.0) : 0.0F;
        }
        y.tau_z = r.tau_z;
        y.tau_n = r.tau_n;
        y.kip = r.kip;
        y.armed = r.armed;
        y.fs_level = r.fs_level;
        y.est_level = r.est_level;
        y.fs_reasons = r.fs_reasons;
        y.thr_valid = r.thr_valid;
        y.is_safe = r.is_safe;
        y.rc = r.rc;
        z.satirlar.push_back(y);
    }
    return z;
}

void z1_olc(const ida::sunucu::Sonuc& s, const ida::zincir::Ozet& o, const ida::zincir::Ayar& za)
{
    const int kabul = static_cast<int>(IDA_ACK_ACCEPTED);
    olcu("ARM ack", s.arm_ack, "= 1", s.arm_ack == kabul);
    olcu("SET_MODE ack", s.kip_ack, "= 1", s.kip_ack == kabul);
    olcu("GUIDED_SP 1 ack", s.sp_ack, "= 1", s.sp_ack == kabul);
    olcu("GUIDED_SP 2 ack", s.sp2_ack, "= 1", s.sp2_ack == kabul);
    olcu("DISARM ack", s.disarm_ack, "= 1", s.disarm_ack == kabul);
    olcu("dalis oturma s", o.dalis.oturma_s, "0..20", o.dalis.oturma_s >= 0.0 && o.dalis.oturma_s <= kDalisOturma);
    olcu("dalis asma m", o.dalis.asim_m, "<= 0.15", o.dalis.asim_m <= kDalisAsim);
    olcu("dalis tutma derinlik m", o.dalis.tutma_derinlik_m, "<= 0.05", o.dalis.tutma_derinlik_m <= kTutmaDerinlik);
    olcu("dalis tutma yon rad", o.dalis.tutma_yon_rad, "<= 0.05", o.dalis.tutma_yon_rad <= kTutmaYon);
    olcu("dalis tutum rad", o.dalis.tutum_rad, "<= 0.05", o.dalis.tutum_rad <= kTutumArizasiz);
    olcu("manevra oturma s", o.manevra.oturma_s, "(bilgi)", true);
    olcu("manevra asma m", o.manevra.asim_m, "<= 0.15", o.manevra.asim_m <= kManevraAsim);
    olcu("manevra yon asma rad", o.manevra.yon_asim_rad, "<= 0.10", o.manevra.yon_asim_rad <= kYonAsim);
    olcu("manevra yon oturma s", o.manevra.yon_oturma_s, "0..15",
         o.manevra.yon_oturma_s >= 0.0 && o.manevra.yon_oturma_s <= kManevraYonOturma);
    olcu("manevra tutma derinlik m", o.manevra.tutma_derinlik_m, "<= 0.05", o.manevra.tutma_derinlik_m <= kTutmaDerinlik);
    olcu("manevra tutma yon rad", o.manevra.tutma_yon_rad, "<= 0.05", o.manevra.tutma_yon_rad <= kTutmaYon);
    olcu("manevra tutum rad", o.manevra.tutum_rad, "<= 0.25", o.manevra.tutum_rad <= kTutumArizali);
    olcu("tutum banda donus s", o.manevra.tutum_oturma_s, "0..15",
         o.manevra.tutum_oturma_s >= 0.0 && o.manevra.tutum_oturma_s <= kTutumToparlanma);
    // W4-K20: itici arızası fizikte mekanik kuvvete uygulanır; darbe genişliği değişmez. W3-zincir'in
    // "arıza darbe genişliğini değiştirdi" ölçütünün karşılığı (tests/butunleme ile aynı): arızanın
    // etkin olduğu her satırda arızalı kanalların fizikteki çarpanı Z1 çarpanına eşit.
    std::uint32_t uygulandi = 0;
    std::uint32_t satir_sayisi = 0;
    for (const ida::sunucu::Satir& r : s.satirlar) {
        if (r.t_s < za.t_manevra_s + 0.002 || r.t_s >= za.t_disarm_s) {
            continue;
        }
        ++satir_sayisi;
        bool hepsi = true;
        for (const int ch : za.ariza_kanallari) {
            const auto c = static_cast<std::size_t>(ch);
            hepsi = hepsi && (std::abs(r.pwm_yur[c] - 1500) <= 25 || r.itki_carpani[c] == za.ariza_carpani);
        }
        uygulandi += hepsi ? 1U : 0U;
    }
    olcu("ariza etkin adim", o.ariza_etkin_adim, ">= 200", o.ariza_etkin_adim >= kEtkinAdimAz);
    olcu("ariza fizikte uygulanan satir", uygulandi, "= etkin satir (" + std::to_string(satir_sayisi) + ")",
         satir_sayisi > 0 && uygulandi == satir_sayisi);
    olcu("sayisal cokme", o.sayisal_cokme ? 1.0 : 0.0, "= 0", !o.sayisal_cokme);
    olcu("en yuksek fs_level", o.en_yuksek_fs, "= 0", o.en_yuksek_fs == 0);
    olcu("DISARM sonrasi itki 0, pwm notr", o.disarm_sonrasi_sifir ? 1.0 : 0.0, "= 1", o.disarm_sonrasi_sifir);
    const double notr_ms = s.t_disarm_ns > 0 ? 1e-6 * static_cast<double>(s.t_pwm_notr_ns - s.t_disarm_ns) : -1.0;
    olcu("DISARM -> fizik notr ms", notr_ms, "= 10 (L_act)", s.t_disarm_ns > 0 && s.t_pwm_notr_ns == s.t_disarm_ns + 10'000'000);
    olcu("kestirim derinlik hata m", o.kestirim_derinlik_hata_m, "(bilgi)", true);
    olcu("kestirim yon hata rad", o.kestirim_yon_hata_rad, "(bilgi)", true);
    olcu("en kotu kestirim duzeyi", o.en_yuksek_est, "(bilgi)", true);
}

double yuzdelik(std::vector<std::int64_t> v, double q)
{
    if (v.empty()) {
        return 0.0;
    }
    std::sort(v.begin(), v.end());
    const auto i = static_cast<std::size_t>(
        std::min<double>(static_cast<double>(v.size() - 1), std::floor(q * static_cast<double>(v.size()))));
    return 1e-6 * static_cast<double>(v[i]);
}

// printf biçimli metni belleğe ekler (fopen yerine akış; MSVC C4996).
struct Metin {
    std::string s;
    void operator()(const char* bicim, ...)
    {
        char t[2048];
        va_list ap;
        va_start(ap, bicim);
        const int n = std::vsnprintf(t, sizeof t, bicim, ap);
        va_end(ap);
        if (n > 0) {
            s.append(t, static_cast<std::size_t>(std::min<int>(n, static_cast<int>(sizeof t) - 1)));
        }
    }
};

// Sayısal seçeneklerin ayrıştırıcısı (sim/sunucu/src/main.cpp ile aynı kural; W4-z2 sol #5):
// değerin tamamı sayı olmalı, ERANGE hatadır; sonuç sonlu ve [en_az, en_cok] içinde olmalıdır.
bool sayi_ayristir(const std::string& v, double en_az, double en_cok, double& x)
{
    if (v.empty()) {
        return false;
    }
    char* son = nullptr;
    errno = 0;
    x = std::strtod(v.c_str(), &son);
    return errno != ERANGE && son != nullptr && *son == '\0' && std::isfinite(x) && x >= en_az && x <= en_cok;
}

// Tamsayı seçenekleri: aynı ayrıştırıcıyla, ayrıca tam değerli olmalı.
bool tam_ayristir(const std::string& v, double en_az, double en_cok, double& x)
{
    return sayi_ayristir(v, en_az, en_cok, x) && x == std::floor(x);
}

int kullanim()
{
    std::fprintf(stderr,
                 "kullanım: ida_z2 hil --seri <aygıt> | sil  [--sure s] [--tohum n] [--acil s --acil-kalk s] "
                 "[--csv yol] [--json yol]\n"
                 "çıkış kodu: 0 işlevsel ve zamanlama geçti; 3 işlevsel kaldı; 4 işlevsel geçti, zamanlama geçersiz;\n"
                 "            5 ikisi de; 1 kullanım hatası\n");
    return 1;
}

int secenek_hatasi(const std::string& ileti)
{
    std::fprintf(stderr, "ida_z2: %s\n", ileti.c_str());
    return kullanim();
}

}  // namespace

int main(int argc, char** argv)
{
    if (argc < 2) {
        return kullanim();
    }
    ida::sunucu::Ayar a;
    a.gunluk = 1;
    const std::string kip = argv[1];
    if (kip == "hil") {
        a.kip = ida::sunucu::Kip::hil;
        a.karsi = ida::sunucu::Karsi::seri;
    } else if (kip == "sil") {
        a.kip = ida::sunucu::Kip::sil;
        a.karsi = ida::sunucu::Karsi::ic;
    } else {
        return kullanim();
    }
    std::string json_yolu;
    // Seçenek ayrıştırması (W4-z2 sol #5; ida_sim önyüzündeki kurallar): her seçenek bir kez;
    // sayılar tam ve aralıkta; açıkça verilmiş geçersiz değer reddedilir.
    std::vector<std::string> verilen;
    for (int i = 2; i < argc; ++i) {
        const std::string s = argv[i];
        if (i + 1 >= argc) {
            return secenek_hatasi(s + " bir değer ister");
        }
        if (std::find(verilen.begin(), verilen.end(), s) != verilen.end()) {
            return secenek_hatasi(s + " bir kez verilebilir");
        }
        verilen.push_back(s);
        const std::string v = argv[++i];
        double x = 0.0;
        if (s == "--seri") {
            if (a.kip != ida::sunucu::Kip::hil || v.empty()) {
                return secenek_hatasi("--seri yalnız hil kipinde ve boş olmayan bir aygıtla verilir");
            }
            a.seri = v;
        } else if (s == "--sure") {
            if (!sayi_ayristir(v, 0.0, 1e7, x) || !(x > 0.0)) {
                return secenek_hatasi("--sure pozitif bir süre (s) ister: " + v);
            }
            a.sure_s = x;
            a.sure_verildi = true;
        } else if (s == "--tohum") {
            if (!tam_ayristir(v, 0.0, 9007199254740991.0, x)) {   // A5-K82: 0 … 2⁵³−1
                return secenek_hatasi("--tohum 0 … 2^53−1 bir tamsayı ister: " + v);
            }
            a.tohum = static_cast<std::uint64_t>(x);
            a.tohum_verildi = true;
        } else if (s == "--acil" || s == "--acil-kalk") {
            if (!sayi_ayristir(v, 0.0, 1e7, x)) {
                return secenek_hatasi(s + " sonlu ve ≥ 0 bir süre (s) ister: " + v);
            }
            if (s == "--acil") {
                a.acil_durdurma_s = x;
                a.acil_verildi = true;
            } else {
                a.acil_kalkar_s = x;
                a.acil_kalk_verildi = true;
            }
        } else if (s == "--csv") {
            if (v.empty()) {
                return secenek_hatasi("--csv bir yol ister");
            }
            a.csv_yolu = v;
        } else if (s == "--json") {
            if (v.empty()) {
                return secenek_hatasi("--json bir yol ister");
            }
            json_yolu = v;
        } else {
            return secenek_hatasi("bilinmeyen seçenek " + s);
        }
    }
    if (a.kip == ida::sunucu::Kip::hil && a.seri.empty()) {
        return secenek_hatasi("hil kipi --seri <aygıt> ister");
    }
    // Acil durdurma denemesi (W4-z2 sol #1): istenen olay koşu süresine sığmalı ve basılmadan önce
    // etkin itki bulunmalı (Z1'de ARM + GUIDED 3,2 s'de; olay ondan sonra); kalkış basıştan sonra.
    if (a.acil_kalk_verildi && !a.acil_verildi) {
        return secenek_hatasi("--acil-kalk yalnız --acil ile verilir");
    }
    if (a.acil_verildi) {
        if (!(a.acil_durdurma_s > a.z1.t_sp_s + 1.0)) {
            return secenek_hatasi("--acil, etkin itkinin başladığı andan (GUIDED_SP + 1 s) sonra olmalı");
        }
        if (!(a.acil_durdurma_s + 1.0 < a.sure_s)) {
            return secenek_hatasi("--acil koşu süresine sığmıyor (en geç süre − 1 s)");
        }
        if (a.acil_kalk_verildi && !(a.acil_kalkar_s > a.acil_durdurma_s && a.acil_kalkar_s + 1.0 < a.sure_s)) {
            return secenek_hatasi("--acil-kalk, --acil'den sonra ve koşu süresinden en az 1 s önce olmalı");
        }
    }
    a.csv_uret = true;
    const ida::sunucu::Sonuc s = ida::sunucu::kos(a);
    const ida::kopru::Sayaclar& c = s.sayac;
    const bool tam_z1 = a.sure_s >= a.z1.t_disarm_s + 2.0 && !a.acil_verildi;
    olcu("kosu tamam", s.tamam ? 1.0 : 0.0, "= 1 " + s.hata, s.tamam);
    if (tam_z1) {
        const ida::zincir::Ayar za = zincir_ayari(a);
        z1_olc(s, ida::zincir::ozetle(zincir_sonucu(s), za), za);
    } else if (!a.acil_verildi) {
        // Ne Z1 ne acil durdurma denemesi: hiçbir işlevsel ölçüt uygulanmadan "GECTI" verilmez.
        olcu("Z1 olcutleri uygulandi", 0.0, "sure >= DISARM + 2 s", false);
    }
    if (a.acil_verildi) {
        std::size_t istek_notr_disi = 0;
        std::size_t fiz_notr_disi = 0;
        std::size_t thr_valid_satir = 0;
        std::size_t satir = 0;
        std::size_t once_etkin_itki = 0;   // basılmadan önceki etkin itki satırı (sol #1)
        double t_ilk_estop = -1.0;
        double t_son_estop = -1.0;
        std::size_t kalkis_sonrasi_satir = 0;
        for (const ida::sunucu::Satir& r : s.satirlar) {
            if (r.estop == 0u) {
                if (r.t_s >= a.z1.t_sp_s && r.t_s < a.acil_durdurma_s) {
                    bool etkin = false;
                    for (std::size_t i = 0; i < 8; ++i) {
                        etkin = etkin || std::abs(r.pwm_fiz[i] - 1500) > 25;
                    }
                    once_etkin_itki += etkin ? 1u : 0u;
                }
                if (a.acil_kalk_verildi && r.t_s >= a.acil_kalkar_s) {
                    ++kalkis_sonrasi_satir;
                }
                continue;
            }
            t_ilk_estop = t_ilk_estop < 0.0 ? r.t_s : t_ilk_estop;
            t_son_estop = r.t_s;
            ++satir;
            thr_valid_satir += r.thr_valid != 0u ? 1u : 0u;
            for (std::size_t i = 0; i < 8; ++i) {
                istek_notr_disi += r.pwm_yur[i] != 1500 ? 1u : 0u;
                fiz_notr_disi += r.pwm_fiz[i] != 1500 ? 1u : 0u;
            }
        }
        // Olayın gerçekleştiği (sol #1): boş bir deneme "ihlal 0" ile geçmez.
        olcu("acil: basilmadan once etkin itki satiri", static_cast<double>(once_etkin_itki), "> 0",
             once_etkin_itki > 0u);
        olcu("acil: etkin fizik adimi", static_cast<double>(s.estop_adim), "> 0", s.estop_adim > 0u);
        olcu("acil: etkin kontrol satiri", static_cast<double>(satir), "> 0", satir > 0u);
        olcu("acil: ilk etkin satir - basma s", t_ilk_estop >= 0.0 ? t_ilk_estop - a.acil_durdurma_s : -1.0,
             "0..0.1", t_ilk_estop >= a.acil_durdurma_s && t_ilk_estop <= a.acil_durdurma_s + 0.1);
        if (a.acil_kalk_verildi) {
            olcu("acil: son etkin satir - kalkis s", t_son_estop >= 0.0 ? t_son_estop - a.acil_kalkar_s : -1.0,
                 "< 0.1", t_son_estop >= 0.0 && t_son_estop < a.acil_kalkar_s + 0.1);
            olcu("acil: kalkistan sonra etkin olmayan satir", static_cast<double>(kalkis_sonrasi_satir), "> 0",
                 kalkis_sonrasi_satir > 0u);
        }
        olcu("acil: ihlal (notr disi fizik adimi)", static_cast<double>(s.ihlal_estop), "= 0", s.ihlal_estop == 0u);
        olcu("acil: fizige giden notr disi (satir x kanal)", static_cast<double>(fiz_notr_disi), "= 0", fiz_notr_disi == 0u);
        olcu("acil: kopru yururlukteki notr disi istek (satir x kanal)", static_cast<double>(istek_notr_disi),
             "(bilgi; W3-K1 acik nokta)", true);
        olcu("acil: thr_valid satiri", static_cast<double>(thr_valid_satir), "(bilgi)", true);
        // Kalkıştan sonra fiziğe giden itkinin geri geldiği ilk satır (W4-K3: kalkıştan sonra
        // gönderilen partinin yanıtıyla) ve kartın atış kontrol durumu akışı (gözlem).
        double t_geri = -1.0;
        for (const ida::sunucu::Satir& r : s.satirlar) {
            if (a.acil_kalk_verildi && r.t_s >= a.acil_kalkar_s && r.estop == 0u) {
                bool notr_disi = false;
                for (std::size_t i = 0; i < 8; ++i) {
                    notr_disi = notr_disi || r.pwm_fiz[i] != 1500;
                }
                if (notr_disi) {
                    t_geri = r.t_s;
                    break;
                }
            }
        }
        olcu("acil: kalkistan itkinin geri gelisine ms", t_geri >= 0.0 ? 1e3 * (t_geri - a.acil_kalkar_s) : -1.0,
             "(bilgi; W4-K3)", true);
        std::printf("fc akisi (t_s estop fc_state fc_inhibit):");
        int onceki_estop = -1;
        int onceki_state = -1;
        long long onceki_mask = -1;
        for (const ida::sunucu::Satir& r : s.satirlar) {
            if (static_cast<int>(r.estop) != onceki_estop || static_cast<int>(r.fc_state) != onceki_state ||
                static_cast<long long>(r.fc_inhibit) != onceki_mask) {
                std::printf(" [%.3f %u %u 0x%05X]", r.t_s, static_cast<unsigned>(r.estop),
                            static_cast<unsigned>(r.fc_state), static_cast<unsigned>(r.fc_inhibit));
                onceki_estop = static_cast<int>(r.estop);
                onceki_state = static_cast<int>(r.fc_state);
                onceki_mask = static_cast<long long>(r.fc_inhibit);
            }
        }
        std::printf("\n");
    }
    std::printf("== ida_z2 %s (sure %.1f s, tohum %llu)\n", kip.c_str(), a.sure_s,
                static_cast<unsigned long long>(a.tohum));
    int kalan = 0;
    for (const Olcu& m : g_olcu) {
        std::printf("%s %-46s %12.4f  %s\n", m.gecti ? "GECTI" : "KALDI", m.ad.c_str(), m.deger, m.kosul.c_str());
        kalan += m.gecti ? 0 : 1;
    }
    const ida::sunucu::ZamanOlcumu& z = s.zaman;
    // TF periyot histogramı (ms).
    const double sinir[] = {2.0, 3.0, 4.0, 4.5, 4.9, 5.1, 5.5, 6.0, 7.0, 10.0, 20.0};
    std::vector<std::uint64_t> kutu(sizeof sinir / sizeof sinir[0] + 1u, 0u);
    for (const std::int64_t p : z.periyotlar_ns) {
        const double ms = 1e-6 * static_cast<double>(p);
        std::size_t k = 0;
        while (k < sizeof sinir / sizeof sinir[0] && ms >= sinir[k]) {
            ++k;
        }
        ++kutu[k];
    }
    std::printf("sayaclar: parti %llu kacirilan %llu (birlesik %llu) gec_yanit %llu yinelenen %llu oturum_ret %llu "
                "eski_uretim %llu esz_uygulanan %llu esz_iptal %llu cagri_cakismasi %llu son_tarih %llu sim_gecikmesi %llu "
                "komut_suresi_doldu %llu alan/bag %llu/%llu crc %llu resync %llu seq_gap %llu batch_incomplete %llu "
                "pencere_tasma %llu kritik_bayt %llu oturum %llu kopma %llu beklenmeyen_sifirlama %llu hazirlik %llu "
                "gecersiz 0x%x\n",
                static_cast<unsigned long long>(c.parti_gonderilen), static_cast<unsigned long long>(c.parti_kacirildi),
                static_cast<unsigned long long>(c.parti_kacirildi_birlesik),
                static_cast<unsigned long long>(c.yanit_ret_gec), static_cast<unsigned long long>(c.yanit_ret_yinelenen),
                static_cast<unsigned long long>(c.yanit_ret_oturum),
                static_cast<unsigned long long>(c.yanit_ret_eski_uretim),
                static_cast<unsigned long long>(c.esz_guvenli_uygulanan),
                static_cast<unsigned long long>(c.yanit_iptal_esz), static_cast<unsigned long long>(c.cagri_cakismasi),
                static_cast<unsigned long long>(c.son_tarih_kacirildi), static_cast<unsigned long long>(c.sim_gecikmesi),
                static_cast<unsigned long long>(c.komut_suresi_doldu), static_cast<unsigned long long>(c.alan_ret),
                static_cast<unsigned long long>(c.bag_ret), static_cast<unsigned long long>(c.rx_crc_err),
                static_cast<unsigned long long>(c.rx_resync_bytes), static_cast<unsigned long long>(c.rx_seq_gap),
                static_cast<unsigned long long>(c.batch_incomplete), static_cast<unsigned long long>(c.pencere_tasma),
                static_cast<unsigned long long>(c.kritik_bayt_en_buyuk),
                static_cast<unsigned long long>(c.oturum_kurulan), static_cast<unsigned long long>(c.oturum_kopma),
                static_cast<unsigned long long>(c.beklenmeyen_sifirlama),
                static_cast<unsigned long long>(s.hazirlik_oturumu), c.calistirma_gecersiz);
    const ida::kopru::KartSayaclari& k = s.kart.sayac;
    std::printf("kart sayaclari (IDA_CORE_COUNTERS): var %d sample_rejected %u future %u conflict %u imu_rejected %u "
                "imu_gap %u bad_input %u cmd_rejected %u step_miss %u; gecersiz_ornek_gonderilen %llu\n",
                k.var ? 1 : 0, k.sample_rejected, k.sample_future, k.sample_conflict, k.imu_rejected, k.imu_gap,
                k.bad_input, k.cmd_rejected, k.step_miss, static_cast<unsigned long long>(c.gecersiz_ornek));
    if (a.kip == ida::sunucu::Kip::hil) {
        std::printf("hil zaman: TF %llu; periyot ms en az %.3f p1 %.3f p50 %.3f p99 %.3f p99.9 %.3f en cok %.3f ort %.4f; "
                    "TF gec yazim en cok %.3f ms; sim gecikmesi %llu; gercek zaman kacirma %llu\n",
                    static_cast<unsigned long long>(z.tf_sayisi), yuzdelik(z.periyotlar_ns, 0.0),
                    yuzdelik(z.periyotlar_ns, 0.01), yuzdelik(z.periyotlar_ns, 0.5), yuzdelik(z.periyotlar_ns, 0.99),
                    yuzdelik(z.periyotlar_ns, 0.999), 1e-6 * static_cast<double>(z.periyot_en_cok_ns),
                    1e-6 * z.periyot_ort_ns, 1e-6 * static_cast<double>(z.gec_yazim_en_cok_ns),
                    static_cast<unsigned long long>(z.sim_gecikmesi),
                    static_cast<unsigned long long>(z.gercek_zaman_kacirma));
        std::printf("hil periyot histogrami (ms):");
        double alt = 0.0;
        for (std::size_t i = 0; i < kutu.size(); ++i) {
            if (kutu[i] != 0u) {
                if (i < sizeof sinir / sizeof sinir[0]) {
                    std::printf(" [%.1f,%.1f) %llu;", alt, sinir[i], static_cast<unsigned long long>(kutu[i]));
                } else {
                    std::printf(" [%.1f,inf) %llu;", alt, static_cast<unsigned long long>(kutu[i]));
                }
            }
            alt = i < sizeof sinir / sizeof sinir[0] ? sinir[i] : alt;
        }
        std::printf("\n");
    }
    std::vector<std::int64_t> ys;
    for (const ida::sunucu::Satir& r : s.satirlar) {
        if (r.yanit_suresi_ns >= 0) {
            ys.push_back(r.yanit_suresi_ns);
        }
    }
    std::size_t ys_10ms = 0;
    for (const std::int64_t v : ys) {
        ys_10ms += v >= 10'000'000 ? 1u : 0u;
    }
    std::printf("yanit suresi (TF yazimi -> yanit alimi, satirlardan): n %zu; ms p0 %.3f p50 %.3f p99 %.3f p99.9 %.3f "
                "en cok %.3f; >= 10 ms %zu; ozet (ida_sim): n %llu p50 %.3f p99 %.3f en cok %.3f\n",
                ys.size(), yuzdelik(ys, 0.0), yuzdelik(ys, 0.5), yuzdelik(ys, 0.99), yuzdelik(ys, 0.999),
                ys.empty() ? 0.0 : 1e-6 * static_cast<double>(*std::max_element(ys.begin(), ys.end())), ys_10ms,
                static_cast<unsigned long long>(z.yanit_sayisi), 1e-6 * static_cast<double>(z.yanit_p50_ns),
                1e-6 * static_cast<double>(z.yanit_p99_ns), 1e-6 * static_cast<double>(z.yanit_en_cok_ns));
    std::printf("ek sayaclar: eszamansiz_ilk_parti_oncesi %llu estop_iptal %llu emniyet_rev_eslesmedi %llu "
                "emniyet_rev_duzeltme %llu; rx_sira_disi %llu rx_kayip %llu; "
                "oturum ici crc %llu atlanan %llu seq_gap %llu; fc son: var %d state %u inhibit 0x%05X\n",
                static_cast<unsigned long long>(c.eszamansiz_ilk_parti_oncesi),
                static_cast<unsigned long long>(c.estop_iptal),
                static_cast<unsigned long long>(c.emniyet_rev_eslesmedi),
                static_cast<unsigned long long>(c.emniyet_rev_duzeltme),
                static_cast<unsigned long long>(c.rx_sira_disi), static_cast<unsigned long long>(c.rx_kayip),
                static_cast<unsigned long long>(z.rx_crc_kosu), static_cast<unsigned long long>(z.rx_resync_kosu),
                static_cast<unsigned long long>(z.rx_seq_gap_kosu), s.kart.fc_var ? 1 : 0,
                static_cast<unsigned>(s.kart.fc_state), static_cast<unsigned>(s.kart.fc_inhibit_mask));
    std::printf("oturum ici sira boslugu zamanlari (s):");
    for (const std::int64_t t : z.sira_boslugu_t_ns) {
        std::printf(" %.3f", 1e-9 * static_cast<double>(t));
    }
    std::printf("\n");
    if (!s.evre_ozeti.empty()) {
        std::printf("evre ozeti: %s\n", s.evre_ozeti.c_str());
    }
    // İki ayrı hüküm (W4-z2 sol #2): işlevsel ölçütler ve A1-K79 zamanlama geçerliliği. Zamanlama
    // kabulü yalnız geçersizlik bitleri 0x0 iken verilir. W4-K24: paylaşılan CI koşucusunda
    // zamanlama hükmü ölçüt değildir; bu program CTest'e girmez, yerelde ve kartta ölçüttür.
    const bool islevsel = kalan == 0;
    const bool zamanlama = c.calistirma_gecersiz == 0u;
    std::printf("ida_z2 islevsel: %s (%d kalan)\n", islevsel ? "GECTI" : "KALDI", kalan);
    std::printf("ida_z2 zamanlama (A1-K79): %s (gecersizlik 0x%x)\n", zamanlama ? "GECERLI" : "GECERSIZ",
                c.calistirma_gecersiz);
    std::printf("ida_z2 %s\n", islevsel && zamanlama ? "GECTI" : "KALDI");
    if (!json_yolu.empty()) {
        Metin f;
        {
            f("{\"kip\": \"%s\", \"tohum\": %llu, \"sure_s\": %.3f, \"kalan\": %d, \"islevsel\": %s, "
              "\"zamanlama_gecerli\": %s, \"olcu\": {",
              kip.c_str(), static_cast<unsigned long long>(a.tohum), a.sure_s, kalan, islevsel ? "true" : "false",
              zamanlama ? "true" : "false");
            for (std::size_t i = 0; i < g_olcu.size(); ++i) {
                f("%s\"%s\": [%.6f, %s]", i == 0 ? "" : ", ", g_olcu[i].ad.c_str(), g_olcu[i].deger,
                             g_olcu[i].gecti ? "true" : "false");
            }
            f(
                         "}, \"sayac\": {\"parti\": %llu, \"kacirilan\": %llu, \"birlesik\": %llu, \"gec_yanit\": %llu, "
                         "\"esz_uygulanan\": %llu, \"esz_iptal\": %llu, \"cagri_cakismasi\": %llu, \"son_tarih\": %llu, "
                         "\"sim_gecikmesi\": %llu, \"alan_ret\": %llu, \"bag_ret\": %llu, \"crc\": %llu, "
                         "\"kritik_bayt\": %llu, \"hazirlik\": %llu, \"oturum\": %llu, \"kopma\": %llu, \"gecersiz\": %u}, "
                         "\"zaman\": {\"tf\": %llu, \"p0\": %.4f, \"p1\": %.4f, \"p50\": %.4f, \"p99\": %.4f, "
                         "\"p999\": %.4f, \"en_cok\": %.4f, \"gec_yazim_en_cok\": %.4f, \"gercek_zaman_kacirma\": %llu, "
                         "\"kutu\": [",
                         static_cast<unsigned long long>(c.parti_gonderilen),
                         static_cast<unsigned long long>(c.parti_kacirildi),
                         static_cast<unsigned long long>(c.parti_kacirildi_birlesik),
                         static_cast<unsigned long long>(c.yanit_ret_gec),
                         static_cast<unsigned long long>(c.esz_guvenli_uygulanan),
                         static_cast<unsigned long long>(c.yanit_iptal_esz),
                         static_cast<unsigned long long>(c.cagri_cakismasi),
                         static_cast<unsigned long long>(c.son_tarih_kacirildi),
                         static_cast<unsigned long long>(c.sim_gecikmesi), static_cast<unsigned long long>(c.alan_ret),
                         static_cast<unsigned long long>(c.bag_ret), static_cast<unsigned long long>(c.rx_crc_err),
                         static_cast<unsigned long long>(c.kritik_bayt_en_buyuk),
                         static_cast<unsigned long long>(s.hazirlik_oturumu),
                         static_cast<unsigned long long>(c.oturum_kurulan),
                         static_cast<unsigned long long>(c.oturum_kopma), c.calistirma_gecersiz,
                         static_cast<unsigned long long>(z.tf_sayisi), yuzdelik(z.periyotlar_ns, 0.0),
                         yuzdelik(z.periyotlar_ns, 0.01), yuzdelik(z.periyotlar_ns, 0.5),
                         yuzdelik(z.periyotlar_ns, 0.99), yuzdelik(z.periyotlar_ns, 0.999),
                         1e-6 * static_cast<double>(z.periyot_en_cok_ns),
                         1e-6 * static_cast<double>(z.gec_yazim_en_cok_ns),
                         static_cast<unsigned long long>(z.gercek_zaman_kacirma));
            for (std::size_t i = 0; i < kutu.size(); ++i) {
                f("%s%llu", i == 0 ? "" : ", ", static_cast<unsigned long long>(kutu[i]));
            }
            f("], \"yanit_n\": %zu, \"yanit_p50\": %.4f, \"yanit_p99\": %.4f, \"yanit_p999\": %.4f, "
              "\"yanit_en_cok\": %.4f, \"yanit_10ms_ustu\": %zu, \"ilk_parti_oncesi_esz\": %llu, "
              "\"estop_iptal\": %llu, \"crc_kosu\": %llu, \"atlanan_kosu\": %llu, \"seq_gap_kosu\": %llu}}\n",
              ys.size(), yuzdelik(ys, 0.5), yuzdelik(ys, 0.99), yuzdelik(ys, 0.999),
              ys.empty() ? 0.0 : 1e-6 * static_cast<double>(*std::max_element(ys.begin(), ys.end())), ys_10ms,
              static_cast<unsigned long long>(c.eszamansiz_ilk_parti_oncesi),
              static_cast<unsigned long long>(c.estop_iptal), static_cast<unsigned long long>(z.rx_crc_kosu),
              static_cast<unsigned long long>(z.rx_resync_kosu), static_cast<unsigned long long>(z.rx_seq_gap_kosu));
            std::ofstream(json_yolu, std::ios::binary) << f.s;
        }
    }
    if (!islevsel) {
        return zamanlama ? 3 : 5;
    }
    return zamanlama ? 0 : 4;
}
