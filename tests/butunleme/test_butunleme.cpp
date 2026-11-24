// Bütünleme testleri (W4-kopru kabul 4–7): `ida_sim`'in kitaplığı (ida_sunucu) üzerinden Z1'in
// A1 mesajlarıyla koşusu.
//
//   z1_sil_ic       kabul 5: süreç içi karşı taraf (ida_link + gerçek çekirdek); W3-zincir
//                   sınırları (tests/zincir/test_zincir.cpp ile aynı değerler) ve aynı süreçteki
//                   Z1 ile farklar (yazdırılır); kabul 2: çekirdeğin reddettiği örnek 0
//   z1_belirlenim   kabul 6: aynı tohumla iki SIL koşusu aynı CSV
//   z1_sil_tcp      kabul 5: iki süreç (ida_sim kitaplığı + ida_sil programı, TCP)
//   acil_durdurma   kabul 4 (EMNİYET): acil durdurma etkinken fiziğe giden darbe genişliği nötr
//   hil_pty         kabul 7: HIL 1x, sözde uçbirim + kart rolünde karşı taraf, 60 s; gerçek
//                   zaman kaçırma sayacı ve TF periyot dağılımı (ölçüm; eşik yok)
//   sil_gecikmeli_tcp  W4-K14: ACTUATOR sonrası çıktılar gecikmeli teslim edilir; CSV aynı kalır
//   sil_iki_surec   W4-K14: iki süreçli (ida_sil) iki koşu ve süreç içi koşu aynı CSV
//   a4_girdi_esitligi  süreç içi karşı taraf ile ida_sil'in A4 çağrı girdileri bayt bayt aynı
//   hil_cikarma_takma  HIL 1x, sözde uçbirim; 5 s'de aygıt kalkar, 7 s'de takılır (A1-K23, H29)
#include "ida/kopru/tasima.hpp"
#include "ida/port.h"
#include "ida/sil/kosucu.hpp"
#include "ida_link/tel.h"
#include "ida/sunucu/sunucu.hpp"
#include "zincir.hpp"
#include "../../sim/kopru/test/zamanlama_olcutu.hpp"   // W4-K24: ortak zamanlama yardımcısı

#include <algorithm>
#include <atomic>
#include <chrono>
#include <deque>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <functional>
#include <memory>
#include <string>
#include <thread>
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

// tests/zincir/test_zincir.cpp'deki sınırlar (W3-zincir; Z1 varsayılanı t5 + t1 arızası).
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

// A1 üzerinden koşunun satırları W3-zincir'in değerlendirmesine çevrilir.
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

void z1_denetle(const char* ad, const ida::sunucu::Sonuc& s, const ida::zincir::Ozet& o)
{
    const std::string on = std::string("[") + ad + "] ";
    dogrula(s.tamam, on + "koşu tamamlandı " + s.hata);
    dogrula(s.arm_ack == static_cast<int>(IDA_ACK_ACCEPTED),
            on + "ARM kabul (ack " + std::to_string(s.arm_ack) + ", neden " + std::to_string(s.arm_neden) + ")");
    dogrula(s.kip_ack == static_cast<int>(IDA_ACK_ACCEPTED), on + "SET_MODE(GUIDED) kabul");
    dogrula(s.sp_ack == static_cast<int>(IDA_ACK_ACCEPTED), on + "GUIDED_SP 1 kabul");
    dogrula(s.sp2_ack == static_cast<int>(IDA_ACK_ACCEPTED), on + "GUIDED_SP 2 kabul");
    dogrula(s.disarm_ack == static_cast<int>(IDA_ACK_ACCEPTED), on + "DISARM kabul");
    dogrula(o.dalis.oturma_s >= 0.0 && o.dalis.oturma_s <= kDalisOturma,
            on + "dalış oturma " + sayi(o.dalis.oturma_s) + " s ≤ " + sayi(kDalisOturma));
    dogrula(o.dalis.asim_m <= kDalisAsim, on + "dalış aşma " + sayi(o.dalis.asim_m) + " m");
    dogrula(o.dalis.tutma_derinlik_m <= kTutmaDerinlik, on + "dalış tutma derinlik " + sayi(o.dalis.tutma_derinlik_m) + " m");
    dogrula(o.dalis.tutma_yon_rad <= kTutmaYon, on + "dalış tutma yön " + sayi(o.dalis.tutma_yon_rad) + " rad");
    dogrula(o.dalis.tutum_rad <= kTutumArizasiz, on + "dalış yalpa/baş-kıç " + sayi(o.dalis.tutum_rad) + " rad");
    dogrula(o.manevra.asim_m <= kManevraAsim, on + "manevra aşma " + sayi(o.manevra.asim_m) + " m");
    dogrula(o.manevra.yon_asim_rad <= kYonAsim, on + "manevra yön aşması " + sayi(o.manevra.yon_asim_rad) + " rad");
    dogrula(o.manevra.yon_oturma_s >= 0.0 && o.manevra.yon_oturma_s <= kManevraYonOturma,
            on + "manevra yön oturma " + sayi(o.manevra.yon_oturma_s) + " s");
    dogrula(o.manevra.tutma_derinlik_m <= kTutmaDerinlik,
            on + "manevra tutma derinlik " + sayi(o.manevra.tutma_derinlik_m) + " m");
    dogrula(o.manevra.tutma_yon_rad <= kTutmaYon, on + "manevra tutma yön " + sayi(o.manevra.tutma_yon_rad) + " rad");
    dogrula(o.manevra.tutum_rad <= kTutumArizali, on + "manevra yalpa/baş-kıç " + sayi(o.manevra.tutum_rad) + " rad");
    dogrula(o.manevra.tutum_oturma_s >= 0.0 && o.manevra.tutum_oturma_s <= kTutumToparlanma,
            on + "tutum banda dönüş " + sayi(o.manevra.tutum_oturma_s) + " s");
    // W4-K20: itici arızası fizikte mekanik kuvvete uygulanır; darbe genişliği değişmez. W3-zincir'in
    // "arıza darbe genişliğini değiştirdi" ölçütünün karşılığı: etkin her satırda arızalı kanalların
    // fizikteki çarpanı Z1 çarpanına eşit.
    std::uint32_t uygulandi = 0;
    for (const ida::sunucu::Satir& r : s.satirlar) {
        if (r.t_s < 43.2 + 0.002 || r.t_s >= 83.2) {
            continue;
        }
        bool hepsi = true;
        for (int ch : {4, 0}) {
            const auto c = static_cast<std::size_t>(ch);
            hepsi = hepsi && (std::abs(r.pwm_yur[c] - 1500) <= 25 || r.itki_carpani[c] == 0.4);
        }
        uygulandi += hepsi ? 1u : 0u;
    }
    const auto satir_sayisi = static_cast<std::uint32_t>(std::count_if(
        s.satirlar.begin(), s.satirlar.end(), [](const ida::sunucu::Satir& r) { return r.t_s >= 43.202 && r.t_s < 83.2; }));
    dogrula(o.ariza_etkin_adim >= kEtkinAdimAz && uygulandi == satir_sayisi,
            on + "arıza etkin (" + std::to_string(o.ariza_etkin_adim) + " adım; fizikte çarpan 0,4: " +
                std::to_string(uygulandi) + "/" + std::to_string(satir_sayisi) + " satır)");
    dogrula(!o.sayisal_cokme, on + "sayısal çökme yok");
    dogrula(o.en_yuksek_fs == 0, on + "beklenmeyen arıza eylemi yok (en yüksek fs_level " + std::to_string(o.en_yuksek_fs) + ")");
    dogrula(o.disarm_sonrasi_sifir, on + "DISARM sonrası itki sıfır ve darbe genişliği nötr");
    dogrula(s.t_disarm_ns > 0 && s.t_pwm_notr_ns == s.t_disarm_ns + 10'000'000,
            on + "fiziğe giden darbe genişliği DISARM partisinden tam L_act sonra nötr (+" +
                sayi(1e-6 * static_cast<double>(s.t_pwm_notr_ns - s.t_disarm_ns)) + " ms)");
}

void sayac_denetle(const char* ad, const ida::sunucu::Sonuc& s, bool ic)
{
    const std::string on = std::string("[") + ad + "] ";
    const ida::kopru::Sayaclar& c = s.sayac;
    dogrula(c.parti_kacirildi == 0u && c.yanit_ret_gec == 0u && c.yanit_ret_yinelenen == 0u &&
                c.yanit_ret_oturum == 0u && c.yanit_ret_eski_uretim == 0u,
            on + "kaçırılan ya da reddedilen yanıt yok");
    dogrula(c.alan_ret == 0u && c.bag_ret == 0u,
            on + "Tablo 4.6 retleri yok (" + std::to_string(c.alan_ret) + "/" + std::to_string(c.bag_ret) + ")");
    dogrula(c.rx_crc_err == 0u && c.rx_resync_bytes == 0u, on + "çerçeve hatası yok");
    dogrula(c.batch_incomplete == 0u, on + "eksik parti yok (A1-K30)");
    dogrula(c.pencere_tasma == 0u, on + "pencere bütçesi aşılmadı (kritik en büyük " +
                                     std::to_string(c.kritik_bayt_en_buyuk) + " B)");
    // Kabul 2 (Okumalar): sensör modelinin VALID = 0 işaretlediği örnekler (çözümsüz GNSS)
    // A4-K33 gereği reddedilir; kabul kapısının zaman ve sıra koşullarından reddedilen 0 olmalı.
    const ida::kopru::KartSayaclari& k = s.kart.sayac;
    dogrula(k.var && k.sample_future == 0u && k.imu_rejected == 0u &&
                static_cast<std::uint64_t>(k.sample_rejected) <= c.gecersiz_ornek,
            on + "çekirdek yalnız VALID = 0 örnekleri reddetti (IDA_CORE_COUNTERS: reddedilen " +
                std::to_string(k.sample_rejected) + " ≤ VALID = 0 gönderilen " + std::to_string(c.gecersiz_ornek) +
                "; gelecek damgalı " + std::to_string(k.sample_future) + ", IMU " + std::to_string(k.imu_rejected) + ")");
    if (ic) {
        dogrula(s.karsi_gecerli_red == 0u && s.karsi_sample_future == 0u &&
                    s.karsi_sample_rejected == c.gecersiz_ornek,
                on + "her çağrıda: VALID bayraklı reddedilen örnek 0; reddedilen = VALID = 0 gönderilen (" +
                    std::to_string(s.karsi_sample_rejected) + " = " + std::to_string(c.gecersiz_ornek) + ")");
    }
}

void fark_yazdir(const ida::zincir::Ozet& a1, const ida::zincir::Ozet& ic)
{
    std::printf("  Z1 farkı (A1 üzerinden − aynı süreçte):\n");
    auto satir = [](const char* ad, double x, double y) {
        std::printf("    %-28s %10.4f %10.4f %+10.4f\n", ad, x, y, x - y);
    };
    std::printf("    %-28s %10s %10s %10s\n", "ölçü", "A1", "süreç içi", "fark");
    satir("dalış oturma (s)", a1.dalis.oturma_s, ic.dalis.oturma_s);
    satir("dalış aşma (m)", a1.dalis.asim_m, ic.dalis.asim_m);
    satir("dalış tutma derinlik (m)", a1.dalis.tutma_derinlik_m, ic.dalis.tutma_derinlik_m);
    satir("dalış tutma yön (rad)", a1.dalis.tutma_yon_rad, ic.dalis.tutma_yon_rad);
    satir("dalış tutum (rad)", a1.dalis.tutum_rad, ic.dalis.tutum_rad);
    satir("manevra oturma (s)", a1.manevra.oturma_s, ic.manevra.oturma_s);
    satir("manevra aşma (m)", a1.manevra.asim_m, ic.manevra.asim_m);
    satir("manevra yön oturma (s)", a1.manevra.yon_oturma_s, ic.manevra.yon_oturma_s);
    satir("manevra yön aşma (rad)", a1.manevra.yon_asim_rad, ic.manevra.yon_asim_rad);
    satir("manevra tutma derinlik (m)", a1.manevra.tutma_derinlik_m, ic.manevra.tutma_derinlik_m);
    satir("manevra tutma yön (rad)", a1.manevra.tutma_yon_rad, ic.manevra.tutma_yon_rad);
    satir("manevra tutum (rad)", a1.manevra.tutum_rad, ic.manevra.tutum_rad);
    satir("tutum banda dönüş (s)", a1.manevra.tutum_oturma_s, ic.manevra.tutum_oturma_s);
    satir("kestirim derinlik hata (m)", a1.kestirim_derinlik_hata_m, ic.kestirim_derinlik_hata_m);
    satir("kestirim yön hata (rad)", a1.kestirim_yon_hata_rad, ic.kestirim_yon_hata_rad);
    satir("en kötü kestirim düzeyi", a1.en_yuksek_est, ic.en_yuksek_est);
}

int test_z1_sil_ic()
{
    ida::sunucu::Ayar a;
    a.gunluk = 0;
    const ida::sunucu::Sonuc s = ida::sunucu::kos(a);
    const ida::zincir::Ayar za = zincir_ayari(a);
    const ida::zincir::Ozet o = ida::zincir::ozetle(zincir_sonucu(s), za);
    z1_denetle("A1, süreç içi", s, o);
    sayac_denetle("A1, süreç içi", s, true);
    dogrula(s.sayac.calistirma_gecersiz == 0u, "[A1, süreç içi] çalıştırma zamanlama bakımından geçerli");
    dogrula(s.hazirlik_oturumu == 1u, "[A1, süreç içi] boş kart: bir hazırlık oturumu (A1-K19) ve REBOOT");
    ida::zincir::Ayar zi = za;
    zi.csv_uret = false;
    const ida::zincir::Sonuc ic = ida::zincir::kos(zi);
    fark_yazdir(o, ida::zincir::ozetle(ic, zi));
    return 0;
}

int test_z1_belirlenim()
{
    ida::sunucu::Ayar a;
    a.gunluk = 0;
    a.sure_s = 20.0;
    const ida::sunucu::Sonuc s1 = ida::sunucu::kos(a);
    const ida::sunucu::Sonuc s2 = ida::sunucu::kos(a);
    dogrula(s1.tamam && s2.tamam, "iki koşu tamamlandı");
    dogrula(!s1.csv.empty() && s1.csv == s2.csv,
            "aynı tohumla iki SIL koşusu aynı CSV (" + std::to_string(s1.csv.size()) + " bayt)");
    a.tohum = 2;
    const ida::sunucu::Sonuc s3 = ida::sunucu::kos(a);
    dogrula(s3.tamam && s3.csv != s1.csv, "farklı tohum farklı CSV (sensör gürültüsü)");
    return 0;
}

int test_z1_sil_tcp(const char* sil_yolu)
{
    ida::sunucu::Ayar a;
    a.gunluk = 1;
    a.karsi = ida::sunucu::Karsi::tcp;
    a.sil_yolu = sil_yolu;
    const ida::sunucu::Sonuc s = ida::sunucu::kos(a);
    const ida::zincir::Ayar za = zincir_ayari(a);
    const ida::zincir::Ozet o = ida::zincir::ozetle(zincir_sonucu(s), za);
    z1_denetle("A1, ida_sil", s, o);
    sayac_denetle("A1, ida_sil", s, false);
    std::printf("  yeniden bağlantı %llu, hazırlık %llu, oturum %llu, kopma %llu, çalıştırma geçersiz 0x%x\n",
                static_cast<unsigned long long>(s.yeniden_baglanti),
                static_cast<unsigned long long>(s.hazirlik_oturumu),
                static_cast<unsigned long long>(s.sayac.oturum_kurulan),
                static_cast<unsigned long long>(s.sayac.oturum_kopma), s.sayac.calistirma_gecersiz);
    // Süreç içi karşı tarafla aynı girdi: Z1 sonuçları aynı olmalı (aynı çekirdek, aynı akış).
    ida::sunucu::Ayar b;
    b.gunluk = 0;
    const ida::sunucu::Sonuc si = ida::sunucu::kos(b);
    const ida::zincir::Ozet oi = ida::zincir::ozetle(zincir_sonucu(si), za);
    std::printf("  ida_sil ile süreç içi karşı taraf farkı: dalış oturma %+.4f s, manevra tutma derinlik %+.4f m\n",
                o.dalis.oturma_s - oi.dalis.oturma_s, o.manevra.tutma_derinlik_m - oi.manevra.tutma_derinlik_m);
    return 0;
}

// sol #13: süreç içi karşı taraf ile ida_sil'in A4 çağrı girdileri aynıdır. Aynı Z1 akışı iki
// karşı tarafla koşulur; her çekirdek çağrısının tamamlanmış girdisi (çekirdek çağrısından
// hemen önce; alım damgaları ve platform alanları dahil) bayt bayt karşılaştırılır.
struct CagriIzi {
    int tur = 0;
    ida_in_t in{};
};

void girdi_kaydet(std::vector<CagriIzi>& v, int tur, const ida_in_t& in)
{
    CagriIzi c;
    c.tur = tur;
    std::memcpy(&c.in, &in, sizeof in);   // take girdiyi sıfırlayarak doldurur (dolgu baytları 0)
    c.in.ping360 = nullptr;               // işaretçi: içeriği karşılaştırmaya girmez (sonar yok)
    v.push_back(c);
}

std::string girdi_farki(const ida_in_t& x, const ida_in_t& y)
{
    std::string f;
    auto alan = [&](const char* ad, const void* a, const void* b, std::size_t n) {
        if (std::memcmp(a, b, n) != 0) {
            f += std::string(f.empty() ? "" : ", ") + ad;
        }
    };
    alan("now_w", &x.now_w, &y.now_w, sizeof x.now_w);
    alan("tf", &x.tf, &y.tf, sizeof x.tf);
    alan("imu", &x.imu, &y.imu, sizeof x.imu);
    alan("samples", x.samples, y.samples, sizeof x.samples);
    alan("narrow", &x.narrow, &y.narrow, sizeof x.narrow);
    alan("cmds", x.cmds, y.cmds, sizeof x.cmds);
    alan("evres", x.evres, y.evres, sizeof x.evres);
    alan("plat", &x.plat, &y.plat, sizeof x.plat);
    if (x.bg_slices != y.bg_slices || x.n_samples != y.n_samples || x.n_cmds != y.n_cmds || x.n_evres != y.n_evres ||
        x.imu_present != y.imu_present) {
        f += std::string(f.empty() ? "" : ", ") + "sayılar";
    }
    return f.empty() ? "diğer" : f;
}

int test_a4_girdi_esitligi()
{
    ida::sunucu::Ayar a;
    a.gunluk = 0;
    a.sure_s = 8.0;   // ARM, kip ve hedef komutları dahil
    std::vector<CagriIzi> ic, sil;
    a.ic_cagri_kancasi = [&](int tur, const ida_in_t& in) { girdi_kaydet(ic, tur, in); };
    const ida::sunucu::Sonuc s1 = ida::sunucu::kos(a);
    dogrula(s1.tamam, "süreç içi koşu " + s1.hata);

    ida::sunucu::Ayar b = a;
    b.ic_cagri_kancasi = nullptr;
    b.karsi = ida::sunucu::Karsi::tcp;
    std::unique_ptr<ida::sil::Kosucu> kosucu;
    std::thread is;
    b.tcp_hazir = [&](std::uint16_t port, std::uint64_t tohum) {
        ida::sil::Ayarlar ka;
        ka.a1_port = port;
        ka.a1_tohum = tohum;
        ka.gunluk = 0;
        ida::sil::Kancalar kn;
        kn.cagri_icinde = [&](int tur, const ida_in_t& in) { girdi_kaydet(sil, tur, in); };
        kosucu = std::make_unique<ida::sil::Kosucu>(ka, kn);
        is = std::thread([&] { (void)kosucu->calistir(); });
    };
    const ida::sunucu::Sonuc s2 = ida::sunucu::kos(b);
    if (kosucu) {
        kosucu->durdur();
    }
    if (is.joinable()) {
        is.join();
    }
    dogrula(s2.tamam, "ida_sil (süreç içinde, TCP) koşu " + s2.hata);
    dogrula(!ic.empty() && ic.size() == sil.size(),
            "çağrı sayısı eşit (süreç içi " + std::to_string(ic.size()) + ", ida_sil " + std::to_string(sil.size()) + ")");
    std::size_t farkli = 0;
    std::string ilk;
    for (std::size_t i = 0; i < std::min(ic.size(), sil.size()); ++i) {
        if (ic[i].tur != sil[i].tur || std::memcmp(&ic[i].in, &sil[i].in, sizeof(ida_in_t)) != 0) {
            if (farkli == 0u) {
                ilk = "çağrı " + std::to_string(i) + " (tür " + std::to_string(ic[i].tur) + "/" +
                      std::to_string(sil[i].tur) + "): " + girdi_farki(ic[i].in, sil[i].in);
            }
            ++farkli;
        }
    }
    dogrula(farkli == 0u, "A4 çağrı girdileri bayt bayt aynı (" + std::to_string(farkli) + " farklı" +
                              (ilk.empty() ? "" : "; ilk: " + ilk) + ")");
    dogrula(s1.csv == s2.csv, "iki karşı tarafla CSV aynı");
    return 0;
}

// W4-K14 (sol #5): kart → sim akışını çerçevelerine ayıran ve IDA_ACTUATOR'dan sonraki
// çerçeveleri gecikme düzeniyle bekleten aktarıcı. ida_sim (TCP) ile ida_sil (aynı süreçte)
// arasına girer; sim → kart yönü olduğu gibi geçer. Sıra korunur (bırakma anları monoton).
class Aktarici {
public:
    // gecikme_us(k): k. ACTUATOR'dan sonraki çerçevelerin bekletilmesi (µs).
    Aktarici(std::uint16_t sim_port, std::function<std::int64_t(std::uint64_t)> gecikme_us)
        : gecikme_(std::move(gecikme_us))
    {
        std::string hata;
        din_ = ida::kopru::tcp_dinle(0, hata);
        sim_port_ = sim_port;
        (void)ida_link_alici_kur(&alici_, IDA_LINK_ROL_SIM);
        if (din_) {
            is_ = std::thread([this] { dongu(); });
        }
    }
    ~Aktarici()
    {
        dur_ = true;
        if (is_.joinable()) {
            is_.join();
        }
    }
    std::uint16_t port() const { return din_ ? din_->port() : 0; }
    std::uint64_t ayrilan() const { return ayrilan_.load(); }

private:
    using Saat = std::chrono::steady_clock;
    struct Parca {
        std::vector<std::uint8_t> bayt;
        Saat::time_point birak;
    };

    void dongu()
    {
        std::string hata;
        auto sim = ida::kopru::tcp_baglan(sim_port_, hata);
        auto kart = din_->kabul(20'000'000, hata);
        if (!sim || !kart) {
            return;
        }
        std::vector<std::uint8_t> t(65536);
        std::vector<std::uint8_t> ham;
        std::deque<Parca> kuyruk;
        Saat::time_point son_birak = Saat::now();
        bool act_sonrasi = false;
        while (!dur_) {
            bool is_var = false;
            std::size_t n = 0;
            ida::kopru::OkumaSonucu r = sim->oku(t.data(), t.size(), 0, n);
            if (r == ida::kopru::OkumaSonucu::kapandi) {
                break;
            }
            if (r == ida::kopru::OkumaSonucu::tamam) {
                is_var = true;
                if (!kart->yaz(t.data(), n)) {
                    break;
                }
            }
            r = kart->oku(t.data(), t.size(), 0, n);
            if (r == ida::kopru::OkumaSonucu::kapandi) {
                break;
            }
            if (r == ida::kopru::OkumaSonucu::tamam) {
                is_var = true;
                std::size_t k = 0;
                while (k < n) {
                    std::size_t tuk = 0;
                    ida_link_cerceve_t c{};
                    const ida_link_rc_t rc = ida_link_al(&alici_, t.data() + k, n - k, &tuk, &c);
                    ham.insert(ham.end(), t.data() + k, t.data() + k + tuk);
                    k += tuk;
                    if (rc != IDA_LINK_CERCEVE) {
                        break;
                    }
                    Saat::time_point b = Saat::now();
                    if (act_sonrasi) {
                        b += std::chrono::microseconds(gecikme_(act_sayisi_));
                        act_sonrasi = false;
                        ++ayrilan_;
                    }
                    son_birak = std::max(son_birak, b);
                    kuyruk.push_back({std::move(ham), son_birak});
                    ham.clear();
                    if (c.msgid == 46060u) {   // MAVLINK_MSG_ID_IDA_ACTUATOR
                        act_sonrasi = true;
                        ++act_sayisi_;
                    }
                }
            }
            const auto simdi = Saat::now();
            while (!kuyruk.empty() && kuyruk.front().birak <= simdi) {
                if (!sim->yaz(kuyruk.front().bayt.data(), kuyruk.front().bayt.size())) {
                    return;
                }
                kuyruk.pop_front();
                is_var = true;
            }
            if (!is_var) {
                std::this_thread::sleep_for(std::chrono::microseconds(50));
            }
        }
    }

    std::function<std::int64_t(std::uint64_t)> gecikme_;
    std::unique_ptr<ida::kopru::TcpDinleyici> din_;
    std::uint16_t sim_port_ = 0;
    ida_link_alici_t alici_{};
    std::thread is_;
    std::atomic<bool> dur_{false};
    std::atomic<std::uint64_t> ayrilan_{0};
    std::uint64_t act_sayisi_ = 0;
};

ida::sunucu::Sonuc aktarici_ile_kos(ida::sunucu::Ayar b, std::function<std::int64_t(std::uint64_t)> gecikme,
                                    std::uint64_t& ayrilan)
{
    b.karsi = ida::sunucu::Karsi::tcp;
    std::unique_ptr<Aktarici> ak;
    std::unique_ptr<ida::sil::Kosucu> kosucu;
    std::thread is;
    b.tcp_hazir = [&](std::uint16_t port, std::uint64_t tohum) {
        ak = std::make_unique<Aktarici>(port, gecikme);
        ida::sil::Ayarlar ka;
        ka.a1_port = ak->port();
        ka.a1_tohum = tohum;
        ka.gunluk = 0;
        kosucu = std::make_unique<ida::sil::Kosucu>(ka);
        is = std::thread([&] { (void)kosucu->calistir(); });
    };
    ida::sunucu::Sonuc s = ida::sunucu::kos(b);
    if (kosucu) {
        kosucu->durdur();
    }
    if (is.joinable()) {
        is.join();
    }
    ayrilan = ak ? ak->ayrilan() : 0u;
    ak.reset();
    return s;
}

std::string ilk_fark(const std::string& x, const std::string& y)
{
    std::size_t i = 0;
    while (i < x.size() && i < y.size() && x[i] == y[i]) {
        ++i;
    }
    if (i == x.size() && i == y.size()) {
        return "";
    }
    const std::size_t bas = x.rfind('\n', i) == std::string::npos ? 0 : x.rfind('\n', i) + 1;
    return " (ilk fark: satır '" + x.substr(bas, std::min<std::size_t>(60, x.size() - bas)) + "…')";
}

// W4-K14: ACTUATOR'dan sonraki çıktılar farklı gecikmelerle geldiğinde CSV değişmez; süreç
// içi karşı tarafla da aynıdır.
int test_sil_gecikmeli_tcp()
{
    ida::sunucu::Ayar a;
    a.gunluk = 0;
    a.sure_s = 8.0;
    const ida::sunucu::Sonuc ic = ida::sunucu::kos(a);
    dogrula(ic.tamam, "süreç içi koşu " + ic.hata);
    std::uint64_t ay0 = 0, ay1 = 0, ay2 = 0;
    const ida::sunucu::Sonuc t0 = aktarici_ile_kos(a, [](std::uint64_t) { return std::int64_t{0}; }, ay0);
    const ida::sunucu::Sonuc t1 = aktarici_ile_kos(
        a, [](std::uint64_t k) { return static_cast<std::int64_t>((k * 7919u) % 4u) * 700; }, ay1);
    const ida::sunucu::Sonuc t2 = aktarici_ile_kos(
        a, [](std::uint64_t k) { return static_cast<std::int64_t>((k * 104729u) % 5u) * 450 + 100; }, ay2);
    dogrula(t0.tamam && t1.tamam && t2.tamam, "üç aktarıcılı koşu tamamlandı " + t0.hata + t1.hata + t2.hata);
    dogrula(ay1 > 1000u && ay2 > 1000u, "ACTUATOR sonrası çıktılar ayrı teslim edildi (" + std::to_string(ay1) + ", " +
                                            std::to_string(ay2) + " kez)");
    dogrula(t0.csv == ic.csv, "gecikmesiz TCP = süreç içi CSV" + ilk_fark(ic.csv, t0.csv));
    dogrula(t1.csv == ic.csv, "gecikme düzeni 1 (0–2,1 ms) = süreç içi CSV" + ilk_fark(ic.csv, t1.csv));
    dogrula(t2.csv == ic.csv, "gecikme düzeni 2 (0,1–1,9 ms) = süreç içi CSV" + ilk_fark(ic.csv, t2.csv));
    dogrula(t1.sayac.calistirma_gecersiz == 0u && t2.sayac.calistirma_gecersiz == 0u, "çalıştırmalar geçerli");
    return 0;
}

// W4-K14: iki süreçli (ida_sil programı) iki koşu bayt bayt aynı CSV; süreç içiyle de aynı.
int test_sil_iki_surec(const char* sil_yolu)
{
    ida::sunucu::Ayar a;
    a.gunluk = 0;
    a.sure_s = 20.0;
    const ida::sunucu::Sonuc ic = ida::sunucu::kos(a);
    ida::sunucu::Ayar b = a;
    b.karsi = ida::sunucu::Karsi::tcp;
    b.sil_yolu = sil_yolu;
    const ida::sunucu::Sonuc s1 = ida::sunucu::kos(b);
    const ida::sunucu::Sonuc s2 = ida::sunucu::kos(b);
    dogrula(ic.tamam && s1.tamam && s2.tamam, "koşular tamamlandı " + s1.hata + s2.hata);
    dogrula(!s1.csv.empty() && s1.csv == s2.csv,
            "iki süreçli iki koşu aynı CSV (" + std::to_string(s1.csv.size()) + " bayt)" + ilk_fark(s1.csv, s2.csv));
    dogrula(s1.csv == ic.csv, "iki süreçli = süreç içi CSV" + ilk_fark(ic.csv, s1.csv));
    return 0;
}

int test_acil_durdurma()
{
    ida::sunucu::Ayar a;
    a.gunluk = 0;
    a.sure_s = 25.0;
    a.acil_durdurma_s = 12.0;   // dalıştan sonra, tutma sırasında
    a.acil_kalkar_s = 18.0;
    const ida::sunucu::Sonuc s = ida::sunucu::kos(a);
    dogrula(s.tamam, "koşu tamamlandı " + s.hata);
    dogrula(s.arm_ack == static_cast<int>(IDA_ACK_ACCEPTED), "ARM kabul (acil durdurma ARM'ın ön koşulu değil; W3-K1)");
    dogrula(s.estop_adim == 4800u, "acil durdurma 6 s boyunca etkin (" + std::to_string(s.estop_adim) + " adım)");
    dogrula(s.ihlal_estop == 0u, "EMNİYET: acil durdurmada fiziğe giden darbe genişliği nötr, itki isteği 0 (" +
                                     std::to_string(s.ihlal_estop) + " ihlal)");
    // Acil durdurma sırasında kart itki istemeyi sürdürür (W3-K1 açık nokta); sim keser.
    std::size_t notr_disi_istek = 0;
    std::size_t fiz_notr_disi = 0;
    for (const ida::sunucu::Satir& r : s.satirlar) {
        if (r.estop == 0u) {
            continue;
        }
        for (std::size_t i = 0; i < 8; ++i) {
            notr_disi_istek += r.pwm_yur[i] != 1500 ? 1u : 0u;
            fiz_notr_disi += r.pwm_fiz[i] != 1500 ? 1u : 0u;
        }
    }
    dogrula(fiz_notr_disi == 0u, "acil durdurma satırlarında fiziğin uyguladığı darbe genişliği nötr");
    std::printf("  acil durdurmada köprünün yürürlükteki nötr dışı komut sayısı (satır × kanal): %zu\n", notr_disi_istek);
    dogrula(s.kart.sayac.var, "kart sayaçları alındı");
    return 0;
}

int test_hil_pty()
{
    ida::sunucu::Ayar a;
    a.gunluk = 1;
    a.kip = ida::sunucu::Kip::hil;
    a.karsi = ida::sunucu::Karsi::pty;
    a.sure_s = 60.0;
    const ida::sunucu::Sonuc s = ida::sunucu::kos(a);
    dogrula(s.tamam, "HIL 1x koşusu tamamlandı " + s.hata);
    dogrula(s.arm_ack == static_cast<int>(IDA_ACK_ACCEPTED), "ARM kabul");
    const ida::sunucu::ZamanOlcumu& z = s.zaman;
    std::vector<std::int64_t> p = z.periyotlar_ns;
    std::sort(p.begin(), p.end());
    auto yuzde = [&](double q) {
        if (p.empty()) {
            return 0.0;
        }
        const auto i = static_cast<std::size_t>(std::min<double>(static_cast<double>(p.size() - 1),
                                                                 std::floor(q * static_cast<double>(p.size()))));
        return 1e-6 * static_cast<double>(p[i]);
    };
    std::printf("  ÖLÇÜM (bu Mac, HIL 1x, sözde uçbirim, 60 s): TF %llu; periyot ms: en az %.3f, p1 %.3f, p50 %.3f, "
                "p99 %.3f, p99,9 %.3f, en çok %.3f; TF geç yazım en çok %.3f ms\n",
                static_cast<unsigned long long>(z.tf_sayisi), yuzde(0.0), yuzde(0.01), yuzde(0.5), yuzde(0.99),
                yuzde(0.999), 1e-6 * static_cast<double>(z.periyot_en_cok_ns),
                1e-6 * static_cast<double>(z.gec_yazim_en_cok_ns));
    std::printf("  gerçek zaman kaçırma %llu (sim gecikmesi %llu, kaçırılan parti %llu, son tarih %llu); "
                "eşzamansız güvenli çıktı %llu; çağrı çakışması %llu; çalıştırma geçersiz 0x%x\n",
                static_cast<unsigned long long>(z.gercek_zaman_kacirma),
                static_cast<unsigned long long>(z.sim_gecikmesi),
                static_cast<unsigned long long>(s.sayac.parti_kacirildi),
                static_cast<unsigned long long>(s.sayac.son_tarih_kacirildi),
                static_cast<unsigned long long>(s.sayac.esz_guvenli_uygulanan),
                static_cast<unsigned long long>(s.sayac.cagri_cakismasi), s.sayac.calistirma_gecersiz);
    std::printf("  karttan seq: sıra dışı %llu, kayıp %llu (çalıştırma oturumunda)\n",
                static_cast<unsigned long long>(z.rx_sira_disi_kosu), static_cast<unsigned long long>(z.rx_kayip_kosu));
    for (std::uint32_t j = 0; j < s.sayac.eszamansiz_kayit_n && j < s.sayac.eszamansiz_kayit.size(); ++j) {
        const auto& e = s.sayac.eszamansiz_kayit[j];
        std::printf("  TANI: A1-K79'a sayılan eşzamansız güvenli çıktı: sınır %llu, out_seq %u, act_flags 0x%x, "
                    "önceki out_seq %u, olağan yanıt alınmıştı %d\n",
                    static_cast<unsigned long long>(e.adim), e.out_seq, static_cast<unsigned>(e.act_flags),
                    e.onceki_out_seq, e.ilk_yanit_alindi ? 1 : 0);
    }
    // ZAMANLAMA (W4-K24, K29): 60 s'deki TF sayısı duvar saatine bağlıdır; ölçüt değilse yalnız ölçüm.
    dogrula(ida::test::zamanlama(z.tf_sayisi >= 11'990u, "hil_pty.tf_sayisi", "60 s'de " + std::to_string(z.tf_sayisi) + " TF (≥ 11 990; ≈ 12 000)"),
            "60 s'de ≈ 12 000 TF (zamanlama)");
    dogrula(s.sayac.alan_ret == 0u && s.sayac.bag_ret == 0u, "Tablo 4.6 retleri yok");
    return 0;
}

// sol #8 (A1-K23, H29): HIL'de aygıt kalkar ve geri takılır. Fizik durmaz ve kesintide
// iticiler nötrdür; sim portu 1 s aralıkla yeniden açmayı dener; takıldıktan sonra yeni el
// sıkışma kurulur ve yanıtlar yeniden uygulanır; çalıştırma geçersiz işaretlenir.
int test_hil_cikarma_takma()
{
    ida::sunucu::Ayar a;
    a.gunluk = 1;
    a.kip = ida::sunucu::Kip::hil;
    a.karsi = ida::sunucu::Karsi::pty;
    a.sure_s = 12.0;
    a.pty_cikarma_s = 5.0;
    a.pty_takma_s = 7.0;
    const ida::sunucu::Sonuc s = ida::sunucu::kos(a);
    dogrula(s.tamam, "koşu tamamlandı " + s.hata);
    dogrula(s.baglanti_kopma == 1u, "bağlantı kaybı görüldü (" + std::to_string(s.baglanti_kopma) + ")");
    dogrula(s.port_acildi == 1u, "port yeniden açıldı (" + std::to_string(s.port_acildi) + ")");
    // ZAMANLAMA (W4-K24): deneme sayısı 1 s'lik duvar saati aralıklarına bağlı.
    dogrula(ida::test::zamanlama(s.port_acma_denemesi >= 2u, "hil_cikarma_takma.port_acma_denemesi",
                                 "1 s aralıkla deneme " + std::to_string(s.port_acma_denemesi) + " (≥ 2)"),
            "port 1 s aralıkla yeniden açılmaya çalışıldı (zamanlama)");
    dogrula((s.sayac.calistirma_gecersiz & ida::kopru::kGecPlanlanmamisBitis) != 0u, "çalıştırma geçersiz (A1-K79)");
    // Satırlar: fizik kesintisiz (her kontrol periyodunda bir satır).
    bool kesintisiz = !s.satirlar.empty();
    for (std::size_t i = 1; i < s.satirlar.size(); ++i) {
        kesintisiz = kesintisiz && std::fabs(s.satirlar[i].t_s - s.satirlar[i - 1].t_s - 0.005) < 1e-9;
    }
    dogrula(kesintisiz && s.satirlar.back().t_s > 11.9, "fizik durmadan sürdü (son satır " + sayi(s.satirlar.back().t_s) + " s)");
    std::uint16_t sess_once = 0, sess_sonra = 0;
    std::size_t kesinti = 0, notr_disi = 0, sonra_yanit = 0, sonra = 0;
    for (const auto& r : s.satirlar) {
        if (r.t_s > 4.0 && r.t_s < 5.0 && r.sess != 0u) {
            sess_once = r.sess;
        }
        if (r.t_s > 5.1 && r.t_s < 6.9) {
            ++kesinti;
            for (double f : r.itki_istegi) {
                if (f != 0.0) {
                    ++notr_disi;
                    break;
                }
            }
        }
        if (r.t_s > 9.0) {
            ++sonra;
            sonra_yanit += r.yanit_var != 0u ? 1u : 0u;
            if (r.sess != 0u) {
                sess_sonra = r.sess;
            }
        }
    }
    dogrula(kesinti > 300u && notr_disi == 0u, "kesintide iticiler nötr (" + std::to_string(notr_disi) + " nötr dışı satır)");
    dogrula(sess_once != 0u && sess_sonra != 0u && sess_sonra != sess_once,
            "yeni oturum (sess " + std::to_string(sess_once) + " → " + std::to_string(sess_sonra) + ")");
    dogrula(sonra > 0u && sonra_yanit > 0u, "takıldıktan sonra yanıtlar uygulanıyor (" + std::to_string(sonra_yanit) +
                                                "/" + std::to_string(sonra) + ")");
    // ZAMANLAMA (W4-K24): her yanıtın yuvasına gerçek zamanda yetişmesi.
    dogrula(ida::test::zamanlama(sonra_yanit == sonra, "hil_cikarma_takma.yanit_hepsi",
                                 std::to_string(sonra_yanit) + "/" + std::to_string(sonra) + " satırda yanıt"),
            "takıldıktan sonra her satırda yanıt (zamanlama)");
    return 0;
}

}  // namespace

int senaryo_testi(const std::string& ad, int argc, char** argv);   // test_senaryo.cpp
int sil_belirlenim_testi(const std::string& ad, int argc, char** argv);   // test_sil_belirlenim.cpp
int sil_alim_testi(const std::string& ad, int argc, char** argv);   // test_sil_alim.cpp
int a3_testi(const std::string& ad, int argc, char** argv);   // test_a3.cpp (W4-bagla-a3)
int sonar_testi(const std::string& ad, int argc, char** argv);   // test_sonar.cpp (W4-bagla-sonar)
int duraklat_testi(const std::string& ad, int argc, char** argv);   // test_duraklat.cpp (W4-duraklat)
int gorev_sil_testi(const std::string& ad, int argc, char** argv);   // test_gorev_sil.cpp (W5-gorev-yurutucu)

int main(int argc, char** argv)
{
    if (argc < 2) {
        std::printf("kullanım: ida_butunleme_test <z1_sil_ic|z1_belirlenim|z1_sil_tcp <ida_sil>|a4_girdi_esitligi|sil_gecikmeli_tcp|sil_iki_surec <ida_sil>|acil_durdurma|hil_pty|hil_cikarma_takma>\n");
        return 2;
    }
    const std::string ad = argv[1];
    if (const int r = senaryo_testi(ad, argc, argv); r >= 0) {
        std::printf("%s %s: %d kalan\n", ad.c_str(), r == 0 ? "GECTI" : "KALDI", r);
        return r == 0 ? 0 : 1;
    }
    if (const int r = sil_alim_testi(ad, argc, argv); r >= 0) {
        std::printf("%s %s: %d kalan\n", ad.c_str(), r == 0 ? "GECTI" : "KALDI", r);
        return r == 0 ? 0 : 1;
    }
    if (const int r = sil_belirlenim_testi(ad, argc, argv); r >= 0) {
        std::printf("%s %s: %d kalan\n", ad.c_str(), r == 0 ? "GECTI" : "KALDI", r);
        return r == 0 ? 0 : 1;
    }
    if (const int r = a3_testi(ad, argc, argv); r >= 0) {
        std::printf("%s %s: %d kalan\n", ad.c_str(), r == 0 ? "GECTI" : "KALDI", r);
        return r == 0 ? 0 : 1;
    }
    if (const int r = sonar_testi(ad, argc, argv); r >= 0) {
        std::printf("%s %s: %d kalan\n", ad.c_str(), r == 0 ? "GECTI" : "KALDI", r);
        return r == 0 ? 0 : 1;
    }
    if (const int r = duraklat_testi(ad, argc, argv); r >= 0) {
        std::printf("%s %s: %d kalan\n", ad.c_str(), r == 0 ? "GECTI" : "KALDI", r);
        return r == 0 ? 0 : 1;
    }
    if (const int r = gorev_sil_testi(ad, argc, argv); r >= 0) {
        std::printf("%s %s: %d kalan\n", ad.c_str(), r == 0 ? "GECTI" : "KALDI", r);
        return r == 0 ? 0 : 1;
    }
    if (ad == "z1_sil_ic") {
        (void)test_z1_sil_ic();
    } else if (ad == "z1_belirlenim") {
        (void)test_z1_belirlenim();
    } else if (ad == "z1_sil_tcp" && argc > 2) {
        (void)test_z1_sil_tcp(argv[2]);
    } else if (ad == "sil_gecikmeli_tcp") {
        (void)test_sil_gecikmeli_tcp();
    } else if (ad == "sil_iki_surec" && argc > 2) {
        (void)test_sil_iki_surec(argv[2]);
    } else if (ad == "a4_girdi_esitligi") {
        (void)test_a4_girdi_esitligi();
    } else if (ad == "acil_durdurma") {
        (void)test_acil_durdurma();
    } else if (ad == "hil_pty") {
        (void)test_hil_pty();
    } else if (ad == "hil_cikarma_takma") {
        (void)test_hil_cikarma_takma();
    } else {
        std::printf("bilinmeyen test: %s\n", ad.c_str());
        return 2;
    }
    std::printf("%s %s: %d kalan\n", ad.c_str(), g_hata == 0 ? "GECTI" : "KALDI", g_hata);
    return g_hata == 0 ? 0 : 1;
}
