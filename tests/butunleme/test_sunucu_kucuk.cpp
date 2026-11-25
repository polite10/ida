// W4-sunucu-kucuk bütünleme testleri (docs/gorevler/W4-sunucu-kucuk.md).
//
//   hil_sinyal_kapanisi     (POSIX) kapsam 1: HIL 1x, sözde uçbirim (sahte kart): koşu sırasında kapanış isteği
//                           (sinyal işleyicisinin kurduğu bayrak) → telde SESSION_CLOSE(SHUTDOWN) görülür,
//                           yerel daraltma, `sinyal_kesti`; tetikten bitişe süre ölçülür (bu Mac)
//   senaryo_hil_dosyalari   (POSIX) kapsam 4: scenarios/z1-hil.toml ve gorev-havuz-hil.toml kartsız HIL'de
//                           (sözde uçbirim) başlar; HIL eşleri SIL dosyalarından yalnız [meta] ad/aciklama
//                           ve [calistirma] kip/sil_azami_hiz bakımından ayrılır; çelişen --kip hâlâ kullanım hatası
//   kabul_son_dilim_sinyali (her platform) kapsam 1, inceleme #2: bağlantı beklemesinin (tek, son dilim) sırasında
//                           gelen kapanış isteği "bağlanmadı" hatası değil sinyal sınıflamasıyla döner
//   sonar_acik_su_dosyasi   (her platform) kapsam 6: scenarios/sonar-acik-su.toml --sonar SIL'de koşar;
//                           iki koşunun CSV'si ve sonar CSV'si bayt bayt aynı
#include "ida/senaryo/senaryo.hpp"
#include "ida/sunucu/sunucu.hpp"
#include "ida_link/tel.h"
#include "../../sim/kopru/src/mavlink_ic.hpp"   // yalnız mesaj yapıları (A1-K101)
#include "../../sim/kopru/test/zamanlama_olcutu.hpp"   // W4-K24, K29

#include <chrono>
#include <filesystem>
#include <cstdio>
#include <string>
#include <thread>
#include <vector>

#ifdef _WIN32
#include <process.h>
#else
#include <unistd.h>
#endif

#ifndef IDA_DEPO_KOKU
#define IDA_DEPO_KOKU "."
#endif

namespace {

int g_hata_sk = 0;

long surec_kimligi()
{
#ifdef _WIN32
    return static_cast<long>(_getpid());
#else
    return static_cast<long>(getpid());
#endif
}

void dogrula_sk(bool kosul, const std::string& ileti)
{
    std::printf("%s %s\n", kosul ? "GECTI " : "KALDI ", ileti.c_str());
    if (!kosul) {
        ++g_hata_sk;
    }
}

#ifndef _WIN32
// Akıştaki SESSION_CLOSE nedenleri (sim → kart yönü).
std::vector<std::uint8_t> kapanislar(const std::vector<std::uint8_t>& v)
{
    std::vector<std::uint8_t> r;
    ida_link_alici_t al{};
    (void)ida_link_alici_kur(&al, IDA_LINK_ROL_KART);
    std::size_t k = 0;
    while (k < v.size()) {
        std::size_t tuk = 0;
        ida_link_cerceve_t c{};
        const ida_link_rc_t rc = ida_link_al(&al, v.data() + k, v.size() - k, &tuk, &c);
        k += tuk;
        if (rc != IDA_LINK_CERCEVE) {
            break;
        }
        if (c.msgid == MAVLINK_MSG_ID_IDA_SESSION_CLOSE) {
            mavlink_ida_session_close_t s{};
            (void)ida_link_coz(&c, c.msgid, &s, sizeof s);
            r.push_back(s.reason);
        }
    }
    return r;
}

std::int64_t simdi_ns()
{
    return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch())
        .count();
}

int test_hil_sinyal_kapanisi()
{
    ida::sunucu::kapanis_sifirla();
    ida::sunucu::Ayar a;
    a.gunluk = 0;
    a.kip = ida::sunucu::Kip::hil;
    a.karsi = ida::sunucu::Karsi::pty;
    a.sure_s = 20.0;
    std::vector<std::uint8_t> giden;
    a.a1_bayt_kancasi = [&giden](bool sim_giden, const std::uint8_t* p, std::size_t n) {
        if (sim_giden) {
            giden.insert(giden.end(), p, p + n);
        }
    };
    std::int64_t tetik = 0;
    a.sinir_kancasi = [&tetik](const ida::sunucu::Ayar::SinirGozlemi& g) {
        if (g.b == 3200u && tetik == 0) {   // 4 s: Z1'de itki yürürlükte
            tetik = simdi_ns();
            ida::sunucu::kapanis_iste();
        }
    };
    const ida::sunucu::Sonuc s = ida::sunucu::kos(a);
    const std::int64_t bitis = simdi_ns();
    ida::sunucu::kapanis_sifirla();
    const std::vector<std::uint8_t> kp = kapanislar(giden);
    const double sure = 1e-9 * static_cast<double>(bitis - tetik);
    std::printf("  HIL sinyal: tamam %d, sinyal_kesti %d, hata '%s', telde SESSION_CLOSE %zu (son neden %d), daraltma %d, "
                "kapanış yazımı eksik %llu, TF %llu\n",
                s.tamam ? 1 : 0, s.sinyal_kesti ? 1 : 0, s.hata.c_str(), kp.size(), kp.empty() ? -1 : kp.back(),
                s.kapanis_daraltildi ? 1 : 0, static_cast<unsigned long long>(s.kapanis_yazim_eksik),
                static_cast<unsigned long long>(s.zaman.tf_sayisi));
    dogrula_sk(!s.tamam && s.sinyal_kesti && s.hata == "sinyalle kesildi", "HIL: koşu sinyalle kesildi (hata kapanışı)");
    dogrula_sk(!kp.empty() && kp.back() == static_cast<std::uint8_t>(IDAW_CLOSE_REASON_SHUTDOWN) && s.kapanis_yazim_eksik == 0u,
               "telde SESSION_CLOSE(SHUTDOWN) görüldü ve tamamen yazıldı");
    dogrula_sk(s.kapanis_daraltildi && !s.kapanis_izin_acik && !s.kapanis_itki_yururlukte,
               "kapanışta yerel daraltma uygulandı");
    dogrula_sk(!s.satirlar.empty() && s.satirlar.back().t_s < 10.0, "koşu 20 s'ye gitmedi");
    std::printf("  ÖLÇÜM (bu Mac, HIL 1x, sahte kart): kapanış isteğinden kos() dönüşüne %.3f s\n", sure);
    dogrula_sk(ida::test::zamanlama(sure < 5.0, "hil_sinyal_kapanisi.sure", "tetikten bitişe " + std::to_string(sure) + " s (< 5 s)"),
               "kapanış 5 s'den kısa sürdü (zamanlama)");
    return 0;
}

// İki dosya satırlarının karşılaştırması: yorum satırlarını ve yalnız şu anahtarları atlar: `[meta]`
// tablosunda `ad` ve `aciklama` (eşin ayrı bir adı olmalı); `[calistirma]` tablosunda `kip` ve
// `sil_azami_hiz`. Başka bir tablodaki aynı adlı anahtarın farkı yakalanır.
std::vector<std::string> govde_satirlari(const std::string& m)
{
    std::vector<std::string> r;
    std::string tablo;
    std::size_t i = 0;
    while (i < m.size()) {
        std::size_t j = m.find('\n', i);
        if (j == std::string::npos) {
            j = m.size();
        }
        const std::string l = m.substr(i, j - i);
        i = j + 1u;
        if (!l.empty() && l[0] == '[') {
            tablo = l.substr(0, l.find(']') + 1u);
        }
        const bool meta = tablo == "[meta]" && (l.rfind("ad = ", 0) == 0 || l.rfind("aciklama = ", 0) == 0);
        const bool kip = tablo == "[calistirma]" && (l.rfind("kip = \"sil\"", 0) == 0 ||
                                                     l.rfind("kip = \"hil_1x\"", 0) == 0 || l.rfind("sil_azami_hiz", 0) == 0);
        if (l.empty() || l[0] == '#' || meta || kip) {
            continue;
        }
        r.push_back(l);
    }
    return r;
}

int test_senaryo_hil_dosyalari()
{
    ida::senaryo::DiskKaynagi d(IDA_DEPO_KOKU);
    struct Cift {
        const char* sil;
        const char* hil;
    };
    for (const Cift& c : {Cift{"scenarios/z1.toml", "scenarios/z1-hil.toml"},
                          Cift{"scenarios/gorev-havuz.toml", "scenarios/gorev-havuz-hil.toml"}}) {
        const std::string m_sil = d.oku(c.sil).value_or("");
        const std::string m_hil = d.oku(c.hil).value_or("");
        dogrula_sk(!m_sil.empty() && !m_hil.empty() && govde_satirlari(m_sil) == govde_satirlari(m_hil),
                   std::string(c.hil) + ": SIL eşinden yalnız kip ve kipe bağlı alan (sil_azami_hiz) farklı");
        bool yasak = false;   // yorum satırları hariç
        for (std::size_t i = 0; i < m_hil.size();) {
            std::size_t j = m_hil.find('\n', i);
            j = j == std::string::npos ? m_hil.size() : j;
            const std::string l = m_hil.substr(i, j - i);
            yasak = yasak || (!l.empty() && l[0] != '#' && l.find("sil_") != std::string::npos);
            i = j + 1u;
        }
        dogrula_sk(m_hil.find("kip = \"hil_1x\"") != std::string::npos && !yasak,
                   std::string(c.hil) + ": kip hil_1x ve HIL'de yasak alan yok (A5-K76)");
        ida::sunucu::kapanis_sifirla();
        ida::sunucu::Ayar a;
        a.gunluk = 0;
        a.senaryo_dosyasi = c.hil;
        a.depo_koku = IDA_DEPO_KOKU;
        a.karsi = ida::sunucu::Karsi::pty;
        a.karsi_verildi = true;
        a.sinir_kancasi = [](const ida::sunucu::Ayar::SinirGozlemi& g) {
            if (g.b == 1600u) {   // 2 s'den sonra kes: başladığını göstermek yeter
                ida::sunucu::kapanis_iste();
            }
        };
        const ida::sunucu::Sonuc s = ida::sunucu::kos(a);
        ida::sunucu::kapanis_sifirla();
        std::printf("  %s: hata_turu %d, kip %s, TF %llu, doğrulama bulgusu %zu, görev '%s', hata '%s'\n", c.hil,
                    static_cast<int>(s.hata_turu), s.kip == ida::sunucu::Kip::hil ? "hil" : "sil",
                    static_cast<unsigned long long>(s.zaman.tf_sayisi), s.dogrulama_bulgulari.size(), s.gorev_durumu.c_str(),
                    s.hata.c_str());
        bool hata_bulgusu = false;
        for (const auto& b : s.dogrulama_bulgulari) {
            hata_bulgusu = hata_bulgusu || b.onem == "hata";
        }
        dogrula_sk(s.hata_turu == ida::sunucu::HataTuru::yok && !hata_bulgusu && s.kip == ida::sunucu::Kip::hil &&
                       !s.davranis_ozeti.empty(),
                   std::string(c.hil) + ": senaryo geçerli, kip hil; kartsız HIL'de kuruldu");
        dogrula_sk(s.sinyal_kesti && s.zaman.tf_sayisi > 100u && s.sayac.oturum_kurulan >= 1u,
                   std::string(c.hil) + ": koşu başladı (TF > 100; oturum kuruldu) ve 2 s sonra kesildi");
        // Komut satırındaki --kip dosyayla aynıysa kabul, çelişiyorsa kullanım hatası (dosya değişmez).
        ida::sunucu::Ayar c2 = a;
        c2.kip_verildi = true;
        c2.kip = ida::sunucu::Kip::sil;
        c2.sinir_kancasi = nullptr;
        const ida::sunucu::Sonuc sc = ida::sunucu::kos(c2);
        dogrula_sk(sc.hata_turu == ida::sunucu::HataTuru::kullanim,
                   std::string(c.hil) + ": çelişen --kip sil kullanım hatası (" + sc.hata + ")");
    }
    ida::sunucu::Ayar z;
    z.gunluk = 0;
    z.senaryo_dosyasi = "scenarios/z1.toml";
    z.depo_koku = IDA_DEPO_KOKU;
    z.kip_verildi = true;
    z.kip = ida::sunucu::Kip::hil;
    z.karsi = ida::sunucu::Karsi::pty;
    z.karsi_verildi = true;
    const ida::sunucu::Sonuc zs = ida::sunucu::kos(z);
    dogrula_sk(zs.hata_turu == ida::sunucu::HataTuru::kullanim, "scenarios/z1.toml + --kip hil: kullanım hatası (dosya SIL kalır; A5 komut satırı geçersiz kılmasına izin vermez)");
    return 0;
}
#endif

int test_sonar_acik_su_dosyasi()
{
    ida::sunucu::kapanis_sifirla();
    std::string csv[2], sonar_csv[2];
    for (int i = 0; i < 2; ++i) {
        ida::sunucu::Ayar a;
        a.gunluk = 0;
        a.senaryo_dosyasi = "scenarios/sonar-acik-su.toml";
        a.depo_koku = IDA_DEPO_KOKU;
        a.sonar_acik = true;
        a.sonar_verildi = true;
        a.csv_uret = true;
        // Sonar CSV'si yol verilince üretilir (ve dosyaya yazılır); dosya geçici dizine yazılıp silinir.
        // Benzersiz yol (süreç kimliği + koşu sırası): paralel koşular ve eski dosyalar çakışmaz.
        const std::filesystem::path gecici = std::filesystem::temp_directory_path() /
            ("ida_sonar_acik_su_" + std::to_string(surec_kimligi()) + "_" + std::to_string(i) + ".csv");
        a.sonar_csv_yolu = gecici.string();
        const ida::sunucu::Sonuc s = ida::sunucu::kos(a);
        std::error_code ec;
        std::filesystem::remove(gecici, ec);
        csv[i] = s.csv;
        sonar_csv[i] = s.sonar_csv;
        if (i == 0) {
            dogrula_sk(s.tamam && s.sonar_acik && s.hata_turu == ida::sunucu::HataTuru::yok,
                       "sonar-acik-su.toml --sonar SIL'de koştu " + s.hata);
            dogrula_sk(s.sonar_ping1d > 0u && s.sonar_ping360 > 0u,
                       "Ping1D (" + std::to_string(s.sonar_ping1d) + ") ve Ping360 (" + std::to_string(s.sonar_ping360) + ") kaydı üretildi");
            dogrula_sk(s.kip == ida::sunucu::Kip::sil && s.senaryo_etkin_sonar == 2u, "SIL kipi; senaryoda 2 etkin sonar");
        }
    }
    dogrula_sk(!csv[0].empty() && csv[0] == csv[1], "iki koşunun CSV'si bayt bayt aynı (" + std::to_string(csv[0].size()) + " B)");
    dogrula_sk(!sonar_csv[0].empty() && sonar_csv[0] == sonar_csv[1],
               "iki koşunun sonar CSV'si bayt bayt aynı (" + std::to_string(sonar_csv[0].size()) + " B)");
    return 0;
}

// Koşucusuz TCP SIL; bağlantı payı tek dilimdir (100 ms) ve kapanış isteği o dilimin ortasında gelir
// (kabul `select` ile bekler, bayrak onu uyandırmaz: dilim zaman aşımıyla biter). Düzeltmeden önce
// koşu "SIL koşucusu bağlanmadı" hatasıyla ve `sinyal_kesti = false` ile bitiyordu (çıkış 2, özet
// `kapanış: hata`). İstek, dinleyici hazır olunca (`tcp_hazir`; kurulum süresi hesaba girmez) 30 ms
// sonra gelir. Aşırı yüklü bir koşucuda istek dilimden sonra kalırsa koşu meşru olarak "bağlanmadı"
// ile biter (karar, istekten önce verilmiştir); bu durum yeniden denenir. Düzeltme geri alınırsa
// istek her denemede dilimin içine düşer ve hiçbir deneme sinyal sınıflamasına varmaz: test düşer.
int test_kabul_son_dilim_sinyali()
{
    constexpr int kDenemeAzami = 20;
    ida::sunucu::Sonuc s;
    int deneme = 0;
    for (; deneme < kDenemeAzami; ++deneme) {
        ida::sunucu::kapanis_sifirla();
        ida::sunucu::Ayar a;
        a.gunluk = 0;
        a.kip = ida::sunucu::Kip::sil;
        a.karsi = ida::sunucu::Karsi::tcp;
        a.a1_port = 0;
        a.sure_s = 5.0;
        a.baglanti_bekleme_ns = 100'000'000;
        std::thread t;
        a.tcp_hazir = [&t](std::uint16_t, std::uint64_t) {
            t = std::thread([] {
                std::this_thread::sleep_for(std::chrono::milliseconds(30));
                ida::sunucu::kapanis_iste();
            });
        };
        s = ida::sunucu::kos(a);
        if (t.joinable()) {
            t.join();
        }
        ida::sunucu::kapanis_sifirla();
        if (s.sinyal_kesti) {
            break;
        }
    }
    std::printf("  son dilim sinyali: %d. denemede; tamam %d, sinyal_kesti %d, hata '%s', oturum %llu\n", deneme + 1,
                s.tamam ? 1 : 0, s.sinyal_kesti ? 1 : 0, s.hata.c_str(), static_cast<unsigned long long>(s.sayac.oturum_kurulan));
    dogrula_sk(!s.tamam && s.sinyal_kesti && s.hata == "sinyalle kesildi",
               "bağlantı beklerken gelen sinyal 'bağlanmadı' değil sinyal olarak sınıflandı (" + s.hata + ")");
    dogrula_sk(s.sayac.oturum_kurulan == 0u, "oturum kurulmadı");
    return 0;
}

}  // namespace

int sunucu_kucuk_testi(const std::string& ad, int, char**)
{
    g_hata_sk = 0;
#ifndef _WIN32
    if (ad == "hil_sinyal_kapanisi") {
        (void)test_hil_sinyal_kapanisi();
        return g_hata_sk;
    }
    if (ad == "senaryo_hil_dosyalari") {
        (void)test_senaryo_hil_dosyalari();
        return g_hata_sk;
    }
#endif
    if (ad == "kabul_son_dilim_sinyali") {
        (void)test_kabul_son_dilim_sinyali();
        return g_hata_sk;
    }
    if (ad == "sonar_acik_su_dosyasi") {
        (void)test_sonar_acik_su_dosyasi();
        return g_hata_sk;
    }
    return -1;   // bu dosyanın testi değil
}
