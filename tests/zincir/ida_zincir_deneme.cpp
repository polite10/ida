// ida_zincir_deneme: Z1 kapalı çevrim senaryosunu koşturur, her kontrol adımını CSV yazar ve
// özeti standart çıktıya basar (W3-zincir §3).
//
// Kullanım:
//   ida_zincir_deneme [--cikti <dosya.csv>] [--tohum <sayı>] [--sure <s>] [--carpan <0–1>]
//                     [--ariza <kanal,kanal,...> | --arizasiz] [--param <AD>=<değer>]...
//
// --ariza: manevranın başında itkisi × çarpan olan kanallar (A5 itici sırası 0–7; varsayılan
// 4,0 = t5 dikey ve t1 yatay). --arizasiz: aynı manevra arızasız. --param: F32 parametresinin
// değeri SIL bloğunda varsayılanın yerine geçer (ayar denemesi).
//
// CSV sütunları: t_s, gerçek ve kestirilen derinlik (O_b, m), yön, yalpa, baş-kıç (rad), sekiz
// itki isteği (thr0–7, A4-K54); darbe genişlikleri (µs): bu çağrının çıktısından üretilip
// t + L_act için kuyruğa konan (pwm_uret0–7), (t − 1,25 ms, t] fizik adımında yürürlükte olan
// (pwm_yur0–7, arızadan önce) ve fiziğin uyguladığı (pwm_fiz0–7, arızadan sonra); kip, armed,
// fs_level, fs_reasons, est_level, thr_valid, is_safe, rc.
#include "zincir.hpp"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

namespace {

void evre_yaz(const char* ad, const ida::zincir::Evre& e)
{
    char t[512];
    std::snprintf(t, sizeof t,
                  "  %s: oturma %.3f s (yon %.3f s), asma %.4f m, yon asmasi %.4f rad, en buyuk "
                  "yalpa/bas-kic %.4f rad (banda donus %.3f s), tutma (son 20 s) derinlik %.4f m, "
                  "yon %.4f rad\n",
                  ad, e.oturma_s, e.yon_oturma_s, e.asim_m, e.yon_asim_rad, e.tutum_rad, e.tutum_oturma_s,
                  e.tutma_derinlik_m, e.tutma_yon_rad);
    std::cout << t;
}

}  // namespace

int main(int argc, char** argv)
{
    ida::zincir::Ayar ayar;
    std::string cikti;
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        const bool deger_var = i + 1 < argc;
        if (a == "--cikti" && deger_var) {
            cikti = argv[++i];
        } else if (a == "--tohum" && deger_var) {
            ayar.tohum = std::strtoull(argv[++i], nullptr, 0);
        } else if (a == "--sure" && deger_var) {
            ayar.sure_s = std::atof(argv[++i]);
        } else if (a == "--carpan" && deger_var) {
            ayar.ariza_carpani = std::atof(argv[++i]);
        } else if (a == "--arizasiz") {
            ayar.ariza_kanallari.clear();
        } else if (a == "--ariza" && deger_var) {
            ayar.ariza_kanallari.clear();
            std::istringstream is(argv[++i]);
            std::string parca;
            while (std::getline(is, parca, ',')) {
                const int k = std::atoi(parca.c_str());
                if (k < 0 || k > 7) {
                    std::cerr << "kanal 0–7 olmalı\n";
                    return 2;
                }
                ayar.ariza_kanallari.push_back(k);
            }
        } else if (a == "--param" && deger_var) {
            const std::string pv = argv[++i];
            const auto esit = pv.find('=');
            if (esit == std::string::npos) {
                std::cerr << "--param AD=deger bekleniyor\n";
                return 2;
            }
            ayar.parametreler.emplace_back(pv.substr(0, esit),
                                           static_cast<float>(std::atof(pv.substr(esit + 1).c_str())));
        } else {
            std::cerr << "kullanım: " << argv[0]
                      << " [--cikti dosya.csv] [--tohum n] [--sure s] [--carpan 0–1] [--ariza k,k | --arizasiz]"
                         " [--param AD=deger]...\n";
            return 2;
        }
    }
    const auto bas = std::chrono::steady_clock::now();
    const ida::zincir::Sonuc s = ida::zincir::kos(ayar);
    const auto son = std::chrono::steady_clock::now();
    if (!s.kurulum_tamam || !s.hata.empty()) {
        std::cerr << "koşu başarısız: " << s.hata << "\n";
        return 1;
    }
    if (!cikti.empty()) {
        std::ofstream f(cikti, std::ios::binary);
        if (!f) {
            std::cerr << "çıktı açılamadı: " << cikti << "\n";
            return 1;
        }
        f << s.csv;
    }
    const ida::zincir::Ozet o = ida::zincir::ozetle(s, ayar);
    const double duvar = std::chrono::duration<double>(son - bas).count();
    std::string kanallar;
    for (int k : ayar.ariza_kanallari) {
        kanallar += (kanallar.empty() ? "" : ",") + std::to_string(k);
    }
    char t[2048];
    std::snprintf(t, sizeof t,
                  "Z1 ozeti (derinlik bandi %.2f m, yon bandi %.2f rad; ariza kanallari [%s] x %.2f)\n"
                  "  denge derinligi (baslangic, O_b): %.4f m\n"
                  "  ARM: ack %d neden %d, kabul t = %.3f s; SET_MODE: ack %d; GUIDED_SP 1: ack %d; "
                  "GUIDED_SP 2: ack %d; DISARM: ack %d\n",
                  o.bant_m, o.yon_bant_rad, kanallar.c_str(), ayar.ariza_carpani, s.denge_derinligi_m, s.arm_ack,
                  s.arm_neden, s.t_arm_kabul_s, s.kip_ack, s.sp_ack, s.sp2_ack, s.disarm_ack);
    std::cout << t;
    evre_yaz("dalis (1,5 m, +90)", o.dalis);
    evre_yaz("manevra (2,5 m, -90)", o.manevra);
    std::snprintf(t, sizeof t,
                  "  ariza etkin adim (manevrada istek olu bant disinda, kanal basina en az): %u; "
                  "arizanin degistirdigi: %u\n"
                  "  en yuksek fs_level %d, en kotu est_level %d, sayisal cokme %d\n"
                  "  kestirim hatasi (ARM-DISARM): derinlik %.4f m, yon %.4f rad\n"
                  "  DISARM sonrasi itki sifir: %d; emniyet ihlali: %zu; A4-K59 duzeltme: %u\n"
                  "  duvar saati: %.3f s (%zu kontrol adimi)\n",
                  static_cast<unsigned>(o.ariza_etkin_adim), static_cast<unsigned>(o.ariza_degistirdi_adim),
                  o.en_yuksek_fs, o.en_yuksek_est,
                  o.sayisal_cokme ? 1 : 0, o.kestirim_derinlik_hata_m, o.kestirim_yon_hata_rad,
                  o.disarm_sonrasi_sifir ? 1 : 0, s.ihlaller.size(), static_cast<unsigned>(s.cikti_duzeltildi), duvar,
                  s.satirlar.size());
    std::cout << t;
    std::snprintf(t, sizeof t,
                  "  DISARM: cagri t = %.3f s, kuyrukta notr disi komut %d; fizige giden PWM notr: +%.2f ms; "
                  "en buyuk itki < 0,05 N: +%.2f ms\n",
                  1e-9 * static_cast<double>(s.t_disarm_ns), s.disarm_kuyrukta_notr_disi ? 1 : 0,
                  1e-6 * static_cast<double>(s.t_pwm_notr_ns - s.t_disarm_ns),
                  1e-6 * static_cast<double>(s.t_itki_sifir_ns - s.t_disarm_ns));
    std::cout << t;
    for (const std::string& h : s.ihlaller) {
        std::cout << "  ihlal: " << h << "\n";
    }
    return 0;
}
