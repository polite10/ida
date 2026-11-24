// zincir.z1 — W3-zincir kabul md.3–5: Z1 kapalı çevrim koşusu (sabit tohum, belirlenimci).
//
// Senaryo (zincir.hpp): dalış 1,5 m / +90°, 40 s tutma; manevra 2,5 m / −90° ve aynı anda
// itici arızası (itki × 0,4), 40 s tutma; DISARM. Dört koşu: arızasız, dikey (t5), yatay (t1),
// ikisi birlikte (Z1'in varsayılanı). Arızalı manevra arızasız manevrayla karşılaştırılır.
// Ölçüt gerçek (fizik) durumdur, kestirim değil. Sınırlar aşağıda sabittir ve gerekçeleri
// yanlarındadır; arızalı koşular için sınır gevşetilmez: denetleyici (integral ve tahsis
// önceliği) toparlayamıyorsa test kalır. Varsayılan koşu iki kez yapılır; iki CSV bayt bayt
// aynı olmalıdır (aynı ikili, aynı makine).
#include "zincir.hpp"

#include "ida/port.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

namespace {

int g_hata = 0;

void dogrula(bool kosul, const std::string& ileti)
{
    std::printf("%s %s\n", kosul ? "GECTI " : "KALDI ", ileti.c_str());
    if (!kosul) {
        ++g_hata;
    }
}

std::string sayi(double v)
{
    char t[64];
    std::snprintf(t, sizeof t, "%.4f", v);
    return t;
}

// ---- Sınırlar ------------------------------------------------------------------------------
// Bantlar (Ozet): derinlik 0,10 m, yön 0,05 rad, tutum 0,02 rad.
// Dalış (1,5 m, +90°; arıza yok):
constexpr double kDalisOturma = 20.0;   // s: 1,5 m'ye REF_DEP_RATE 0,25 m/s ile ≈ 6 s + pay
constexpr double kDalisAsim = 0.15;     // m: hedefin %10'u
// Manevra (2,5 m, −90°): 1 m'lik derinlik adımı ≈ 4 s, 180°'lik dönüş REF_YAW_RATE 0,35 rad/s
// ile ≈ 9 s sürer.
constexpr double kManevraOturma = 15.0;     // s, derinlik
constexpr double kManevraYonOturma = 15.0;  // s, yön
constexpr double kManevraAsim = 0.15;       // m: 1 m adımın %15'i
constexpr double kYonAsim = 0.10;           // rad (≈ 5,7°)
// Arızalı manevrada arızasıza göre izin verilen gecikme: oturma en çok 5 s uzar.
constexpr double kArizaOturmaFarki = 5.0;
// Tutum: arızasız 0,05 rad; arızalı 0,25 rad (≈ 14°; itki kaybının yarattığı moment, integral
// ve tahsis toparlayana dek) ve 15 s içinde 0,02 rad bandına döner.
constexpr double kTutumArizasiz = 0.05;
constexpr double kTutumArizali = 0.25;
constexpr double kTutumToparlanma = 15.0;
// Tutma (her evrenin son 20 s'si; arızalı koşularda da: kalıcı hata integralle kapanmalı).
constexpr double kTutmaDerinlik = 0.05;  // m
constexpr double kTutmaYon = 0.05;       // rad
// Arızanın etkinliği: manevrada arızalı her kanalın isteği en az 1 s (200 adım) ölü bandın
// dışında ve arıza o adımların hepsinde darbe genişliğini değiştirmiş olmalı.
constexpr unsigned kEtkinAdimAz = 200U;

struct Kosu {
    const char* ad;
    std::vector<int> kanallar;
    ida::zincir::Sonuc s;
    ida::zincir::Ozet o;
};

void ortak_denetle(const Kosu& k)
{
    using namespace ida::zincir;
    const std::string on = std::string("[") + k.ad + "] ";
    const Sonuc& s = k.s;
    const Ozet& o = k.o;
    dogrula(s.kurulum_tamam && s.hata.empty(), on + "koşu tamamlandı " + s.hata);
    dogrula(s.arm_ack == static_cast<int>(IDA_ACK_ACCEPTED), on + "ARM kabul (ack " + std::to_string(s.arm_ack) +
                                                                  ", neden " + std::to_string(s.arm_neden) + ")");
    dogrula(s.kip_ack == static_cast<int>(IDA_ACK_ACCEPTED), on + "SET_MODE(GUIDED) kabul");
    dogrula(s.sp_ack == static_cast<int>(IDA_ACK_ACCEPTED), on + "GUIDED_SP 1 kabul");
    dogrula(s.sp2_ack == static_cast<int>(IDA_ACK_ACCEPTED), on + "GUIDED_SP 2 kabul");
    dogrula(s.disarm_ack == static_cast<int>(IDA_ACK_ACCEPTED), on + "DISARM kabul");
    // Dalış evresi (arızadan önce; bütün koşularda aynı).
    dogrula(o.dalis.oturma_s >= 0.0 && o.dalis.oturma_s <= kDalisOturma,
            on + "dalış oturma " + sayi(o.dalis.oturma_s) + " s ≤ " + sayi(kDalisOturma));
    dogrula(o.dalis.asim_m <= kDalisAsim, on + "dalış aşma " + sayi(o.dalis.asim_m) + " m ≤ " + sayi(kDalisAsim));
    dogrula(o.dalis.tutma_derinlik_m <= kTutmaDerinlik,
            on + "dalış tutma derinlik " + sayi(o.dalis.tutma_derinlik_m) + " m ≤ " + sayi(kTutmaDerinlik));
    dogrula(o.dalis.tutma_yon_rad <= kTutmaYon,
            on + "dalış tutma yön " + sayi(o.dalis.tutma_yon_rad) + " rad ≤ " + sayi(kTutmaYon));
    dogrula(o.dalis.tutum_rad <= kTutumArizasiz,
            on + "dalış yalpa/baş-kıç " + sayi(o.dalis.tutum_rad) + " rad ≤ " + sayi(kTutumArizasiz));
    // Manevra evresi: oturma ve tutma (arızalıda da).
    dogrula(o.manevra.asim_m <= kManevraAsim,
            on + "manevra aşma " + sayi(o.manevra.asim_m) + " m ≤ " + sayi(kManevraAsim));
    dogrula(o.manevra.yon_asim_rad <= kYonAsim,
            on + "manevra yön aşması " + sayi(o.manevra.yon_asim_rad) + " rad ≤ " + sayi(kYonAsim));
    dogrula(o.manevra.yon_oturma_s >= 0.0 && o.manevra.yon_oturma_s <= kManevraYonOturma,
            on + "manevra yön oturma " + sayi(o.manevra.yon_oturma_s) + " s ≤ " + sayi(kManevraYonOturma));
    dogrula(o.manevra.tutma_derinlik_m <= kTutmaDerinlik,
            on + "manevra tutma derinlik " + sayi(o.manevra.tutma_derinlik_m) + " m ≤ " + sayi(kTutmaDerinlik));
    dogrula(o.manevra.tutma_yon_rad <= kTutmaYon,
            on + "manevra tutma yön " + sayi(o.manevra.tutma_yon_rad) + " rad ≤ " + sayi(kTutmaYon));
    // Emniyet ve sağlık.
    dogrula(!o.sayisal_cokme, on + "sayısal çökme yok");
    dogrula(o.en_yuksek_fs == static_cast<int>(IDA_FS_NONE),
            on + "beklenmeyen arıza eylemi yok (en yüksek fs_level " + std::to_string(o.en_yuksek_fs) + ")");
    dogrula(o.disarm_sonrasi_sifir, on + "DISARM sonrası itki sıfır ve darbe genişliği nötr");
    dogrula(s.ihlaller.empty(), on + "emniyet değişmezleri: " + std::to_string(s.ihlaller.size()) + " ihlal");
    for (const std::string& h : s.ihlaller) {
        std::printf("       %s\n", h.c_str());
    }
    dogrula(s.cikti_duzeltildi == 0U, on + "platform hiçbir çıktıyı düzeltmedi (A4-K59)");
}

}  // namespace

int main()
{
    using namespace ida::zincir;
    std::vector<Kosu> kosular = {
        {"arızasız", {}, {}, {}},
        {"dikey t5", {4}, {}, {}},
        {"yatay t1", {0}, {}, {}},
        {"t5 + t1 (Z1)", {4, 0}, {}, {}},
    };
    for (Kosu& k : kosular) {
        Ayar a;
        a.ariza_kanallari = k.kanallar;
        k.s = kos(a);
        if (!k.s.kurulum_tamam) {
            dogrula(false, std::string("[") + k.ad + "] kurulum: " + k.s.hata);
            return 1;
        }
        k.o = ozetle(k.s, a);
        ortak_denetle(k);
    }
    const Ozet& temel = kosular[0].o;
    dogrula(temel.manevra.oturma_s >= 0.0 && temel.manevra.oturma_s <= kManevraOturma,
            "[arızasız] manevra oturma " + sayi(temel.manevra.oturma_s) + " s ≤ " + sayi(kManevraOturma));
    dogrula(temel.manevra.tutum_rad <= kTutumArizasiz,
            "[arızasız] manevra yalpa/baş-kıç " + sayi(temel.manevra.tutum_rad) + " rad ≤ " + sayi(kTutumArizasiz));
    for (std::size_t i = 1; i < kosular.size(); ++i) {
        const Kosu& k = kosular[i];
        const std::string on = std::string("[") + k.ad + "] ";
        const Evre& m = k.o.manevra;
        dogrula(k.o.ariza_etkin_adim >= kEtkinAdimAz,
                on + "arıza etkin: istek ölü bant dışında " + std::to_string(k.o.ariza_etkin_adim) + " adım ≥ " +
                    std::to_string(kEtkinAdimAz));
        dogrula(k.o.ariza_degistirdi_adim == k.o.ariza_etkin_adim,
                on + "arıza o adımların hepsinde darbe genişliğini değiştirdi (" +
                    std::to_string(k.o.ariza_degistirdi_adim) + ")");
        dogrula(m.oturma_s >= 0.0 && m.oturma_s <= temel.manevra.oturma_s + kArizaOturmaFarki,
                on + "manevra oturma " + sayi(m.oturma_s) + " s ≤ arızasız " + sayi(temel.manevra.oturma_s) +
                    " + " + sayi(kArizaOturmaFarki));
        dogrula(m.tutum_rad <= kTutumArizali,
                on + "manevra yalpa/baş-kıç " + sayi(m.tutum_rad) + " rad ≤ " + sayi(kTutumArizali));
        dogrula(m.tutum_oturma_s >= 0.0 && m.tutum_oturma_s <= kTutumToparlanma,
                on + "tutum banda dönüş " + sayi(m.tutum_oturma_s) + " s ≤ " + sayi(kTutumToparlanma));
    }
    // Dikey itici arızası tutumu gerçekten bozar (arızanın etkisinin gözlenebilirliği).
    dogrula(kosular[1].o.manevra.tutum_rad > 4.0 * temel.manevra.tutum_rad,
            "[dikey t5] arıza tutumu etkiledi: " + sayi(kosular[1].o.manevra.tutum_rad) + " rad > 4 × arızasız " +
                sayi(temel.manevra.tutum_rad));

    // DISARM (kabul md.4; A1-K74, K77; EMNİYET): DISARM'ı taşıyan çağrının (t) nötr çıktısı
    // uygulama anında (t + L_act) yürürlüğe girer; o ana kadar önceden kuyruğa alınmış iki
    // komut uygulanır (SIL'de eşzamansız güvenli çıktı yok, A1-K44, K78). Vaka, kuyrukta nötr
    // dışı komut varken sınanır; itkinin sönümü (ESC ve itici dinamiği) ölçülür.
    {
        const Sonuc& z1 = kosular[3].s;
        dogrula(z1.disarm_kuyrukta_notr_disi, "DISARM anında kuyrukta nötr dışı komut vardı");
        dogrula(z1.t_disarm_ns > 0 && z1.t_pwm_notr_ns == z1.t_disarm_ns + 10'000'000,
                "fiziğe giden darbe genişliği DISARM'dan tam L_act = 10 ms sonra nötr (+" +
                    sayi(1e-6 * static_cast<double>(z1.t_pwm_notr_ns - z1.t_disarm_ns)) + " ms)");
        // İtkinin 0,05 N altına inmesi: L_act + ESC/itici gecikmesi; sınır 250 ms (ölçülen değer
        // raporda; ölçüm, bu fizik modelinde).
        dogrula(z1.t_itki_sifir_ns > z1.t_pwm_notr_ns && z1.t_itki_sifir_ns <= z1.t_disarm_ns + 250'000'000,
                "en büyük itki DISARM'dan +" + sayi(1e-6 * static_cast<double>(z1.t_itki_sifir_ns - z1.t_disarm_ns)) +
                    " ms'de 0,05 N altında (≤ 250 ms)");
    }
    // Karşı örnek: platform DISARM'ın nötr çıktısını kuyruğa koymazsa çıktı tarafı (itki isteği
    // ve üretilen darbe genişliği) nötr görünür ama fizik eski komutla sürer; fizik tarafındaki
    // denetim bunu yakalamalı.
    {
        Ayar a;
        a.karsi_ornek_disarm_yuvasi_atlanir = true;
        a.csv_uret = false;
        const Sonuc ks = kos(a);
        const Ozet ko = ozetle(ks, a);
        bool cikti_tarafi_notr = true;
        for (const Satir& r : ks.satirlar) {
            if (r.t_s >= a.t_disarm_s - 0.0025) {
                for (std::size_t i = 0; i < 8; ++i) {
                    cikti_tarafi_notr = cikti_tarafi_notr && (r.thr[i] == 0.0F) && (r.pwm_uret[i] == 1500);
                }
            }
        }
        dogrula(cikti_tarafi_notr, "[karşı örnek] çıktı tarafı nötr (eski denetim bunu geçerdi)");
        dogrula(!ks.ihlaller.empty() && !ko.disarm_sonrasi_sifir,
                "[karşı örnek] fizik tarafındaki denetim nötr dışı uygulamayı yakaladı (" +
                    std::to_string(ks.ihlaller.size()) + " ihlal)");
    }

    // HOLD'a giriş (sol 2. tur notu): manevra başlarken yer istasyonu canlılığı kesilir; GCS_KAYIP_MS
    // (3 s) sonra, dönüş ve dalış sürerken HOLD başlar. Kontrol yön ve derinlik referansını
    // bulunulan durumdan yeniden kurar, öteleme integrallerini sıfırlar, açısal hız
    // integrallerini korur. Ölçüt fiziğin itici kuvvet ve momentidir.
    {
        Ayar a;
        a.ariza_kanallari.clear();
        a.t_hb_kes_s = a.t_manevra_s;
        a.csv_uret = false;
        const Sonuc hs = kos(a);
        double t_hold = -1.0;
        std::size_t i_hold = 0;
        for (std::size_t i = 0; i < hs.satirlar.size(); ++i) {
            if (hs.satirlar[i].fs_level >= static_cast<std::uint8_t>(IDA_FS_HOLD)) {
                t_hold = hs.satirlar[i].t_s;
                i_hold = i;
                break;
            }
        }
        dogrula(t_hold > a.t_manevra_s, "[HOLD] arıza eylemi HOLD başladı (t = " + sayi(t_hold) + " s)");
        if (t_hold > 0.0) {
            // Her 5 ms'de yön momentinin değişimi: geçişten önceki 1 s ve sonraki 2 s.
            double d_once = 0.0, d_sonra = 0.0, n_sonra_enb = 0.0, z_hold = hs.satirlar[i_hold].tau_z;
            double z_sonra_enb = -1.0e9;
            for (std::size_t i = i_hold - 200; i < i_hold + 400; ++i) {
                const double dn = std::abs(hs.satirlar[i].tau_n - hs.satirlar[i - 1].tau_n);
                if (i < i_hold) {
                    d_once = std::max(d_once, dn);
                } else {
                    d_sonra = std::max(d_sonra, dn);
                    n_sonra_enb = std::max(n_sonra_enb, std::abs(hs.satirlar[i].tau_n));
                    z_sonra_enb = std::max(z_sonra_enb, hs.satirlar[i].tau_z);
                }
            }
            const Satir& h0 = hs.satirlar[i_hold];
            const Satir& h5 = hs.satirlar[i_hold + 1000];  // HOLD'dan 5 s sonra
            const Satir& h4 = hs.satirlar[i_hold + 800];
            // Sınırlar: itici başına değişim hızı THR_SLEW = 250 N/s, 5 ms'de 1,25 N; dört yatay
            // iticinin N kolu 0,189 m → adım başına en çok 0,945 N·m değişim (sıçrama yok).
            dogrula(d_sonra <= 0.945, "[HOLD] yön momenti adım başına değişimi " + sayi(d_sonra) +
                                          " N·m ≤ 0,945 (önce " + sayi(d_once) + ")");
            dogrula(n_sonra_enb <= 10.0, "[HOLD] yön momenti " + sayi(n_sonra_enb) + " N·m ≤ LIM_MN 10");
            dogrula(z_sonra_enb <= z_hold + 0.5, "[HOLD] aşağı kuvvet artmadı: en büyük " + sayi(z_sonra_enb) +
                                                     " N ≤ girişteki " + sayi(z_hold) + " + 0,5");
            dogrula(std::abs(h5.derinlik - h0.derinlik) <= 0.10,
                    "[HOLD] derinlik girişteki değerde tutuldu (" + sayi(h0.derinlik) + " → " + sayi(h5.derinlik) + " m)");
            dogrula(std::abs(h5.yon - h4.yon) <= 0.01 && std::abs(h5.yon - h0.yon) <= 0.10,
                    "[HOLD] dönüş durdu ve yön girişteki değerde tutuldu (" + sayi(h0.yon) + " → " + sayi(h5.yon) +
                        " rad)");
        }
    }

    // Belirlenimcilik (kabul md.5): varsayılan senaryo ikinci kez.
    const Sonuc tekrar = kos(Ayar{});
    dogrula(tekrar.csv == kosular[3].s.csv && !tekrar.csv.empty(),
            "iki koşu bit düzeyinde aynı CSV (" + std::to_string(tekrar.csv.size()) + " bayt)");

    std::printf("%s: %d kalan\n", g_hata == 0 ? "zincir.z1 GECTI" : "zincir.z1 KALDI", g_hata);
    return g_hata == 0 ? 0 : 1;
}
