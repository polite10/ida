// ida_z2 (W4-z2): Z1 senaryosunu `ida_sim` kitaplığıyla (ida_sunucu) koşar ve sonucu W3-zincir
// sınırlarıyla (tests/zincir/test_zincir.cpp ve tests/butunleme/test_butunleme.cpp ile aynı
// değerler) değerlendirir. Kipler:
//
//   ida_z2 hil --seri <aygıt> [--sure s] [--tohum n] [--acil s --acil-kalk s] [--csv yol] [--json yol]
//   ida_z2 sil [--sure s] [--tohum n] [--acil s --acil-kalk s] [--json yol]     (süreç içi karşı taraf)
//
// Çıktı: insan okunur özet (stdout) ve isteğe bağlı JSON (ölçüm raporu için). Sınır ya da kazanç
// değiştirmez; yalnız ölçer. HIL koşusu kartı ve gerçek zamanı kullanır (ağır yük altında
// sayılar ön değerdir).
#include "ida/port.h"
#include "ida/sunucu/sunucu.hpp"
#include "zincir.hpp"

#include <algorithm>
#include <cmath>
#include <cstdarg>
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

void z1_olc(const ida::sunucu::Sonuc& s, const ida::zincir::Ozet& o)
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
    olcu("ariza etkin adim", o.ariza_etkin_adim, ">= 200 ve = degistirdi",
         o.ariza_etkin_adim >= kEtkinAdimAz && o.ariza_degistirdi_adim == o.ariza_etkin_adim);
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

int kullanim()
{
    std::fprintf(stderr,
                 "kullanım: ida_z2 hil --seri <aygıt> | sil  [--sure s] [--tohum n] [--acil s --acil-kalk s] "
                 "[--csv yol] [--json yol]\n");
    return 1;
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
    for (int i = 2; i < argc; ++i) {
        const std::string s = argv[i];
        if (i + 1 >= argc) {
            return kullanim();
        }
        const std::string v = argv[++i];
        if (s == "--seri") {
            a.seri = v;
        } else if (s == "--sure") {
            a.sure_s = std::strtod(v.c_str(), nullptr);
        } else if (s == "--tohum") {
            a.tohum = std::strtoull(v.c_str(), nullptr, 10);
        } else if (s == "--acil") {
            a.acil_durdurma_s = std::strtod(v.c_str(), nullptr);
        } else if (s == "--acil-kalk") {
            a.acil_kalkar_s = std::strtod(v.c_str(), nullptr);
        } else if (s == "--csv") {
            a.csv_yolu = v;
        } else if (s == "--json") {
            json_yolu = v;
        } else {
            return kullanim();
        }
    }
    if (a.kip == ida::sunucu::Kip::hil && a.seri.empty()) {
        return kullanim();
    }
    a.csv_uret = true;
    const ida::sunucu::Sonuc s = ida::sunucu::kos(a);
    const ida::kopru::Sayaclar& c = s.sayac;
    const bool tam_z1 = a.sure_s >= a.z1.t_disarm_s + 2.0 && a.acil_durdurma_s < 0.0;
    olcu("kosu tamam", s.tamam ? 1.0 : 0.0, "= 1 " + s.hata, s.tamam);
    if (tam_z1) {
        const ida::zincir::Ayar za = zincir_ayari(a);
        z1_olc(s, ida::zincir::ozetle(zincir_sonucu(s), za));
    }
    if (a.acil_durdurma_s >= 0.0) {
        std::size_t istek_notr_disi = 0;
        std::size_t fiz_notr_disi = 0;
        std::size_t thr_valid_satir = 0;
        std::size_t satir = 0;
        for (const ida::sunucu::Satir& r : s.satirlar) {
            if (r.estop == 0u) {
                continue;
            }
            ++satir;
            thr_valid_satir += r.thr_valid != 0u ? 1u : 0u;
            for (std::size_t i = 0; i < 8; ++i) {
                istek_notr_disi += r.pwm_yur[i] != 1500 ? 1u : 0u;
                fiz_notr_disi += r.pwm_fiz[i] != 1500 ? 1u : 0u;
            }
        }
        olcu("acil: etkin fizik adimi", static_cast<double>(s.estop_adim), "(bilgi)", true);
        olcu("acil: ihlal (notr disi fizik adimi)", static_cast<double>(s.ihlal_estop), "= 0", s.ihlal_estop == 0u);
        olcu("acil: fizige giden notr disi (satir x kanal)", static_cast<double>(fiz_notr_disi), "= 0", fiz_notr_disi == 0u);
        olcu("acil: kopru yururlukteki notr disi istek (satir x kanal)", static_cast<double>(istek_notr_disi),
             "(bilgi; W3-K1 acik nokta)", true);
        olcu("acil: kontrol satiri", static_cast<double>(satir), "(bilgi)", true);
        olcu("acil: thr_valid satiri", static_cast<double>(thr_valid_satir), "(bilgi)", true);
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
    std::printf("ida_z2 %s: %d kalan\n", kalan == 0 ? "GECTI" : "KALDI", kalan);
    if (!json_yolu.empty()) {
        Metin f;
        {
            f("{\"kip\": \"%s\", \"tohum\": %llu, \"sure_s\": %.3f, \"kalan\": %d, \"olcu\": {", kip.c_str(),
                         static_cast<unsigned long long>(a.tohum), a.sure_s, kalan);
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
            f("]}}\n");
            std::ofstream(json_yolu, std::ios::binary) << f.s;
        }
    }
    return kalan == 0 ? 0 : 3;
}
