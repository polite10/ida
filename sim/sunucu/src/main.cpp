// ida_sim — başsız sim sunucusu (W4-kopru). Kullanım: sim/sunucu/BENIOKU.md.
#include "ida/sunucu/sunucu.hpp"

#include <cerrno>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <algorithm>

namespace {

void kullanim()
{
    std::printf(
        "kullanım: ida_sim [seçenekler]\n"
        "  --kip sil|hil            zaman yöneticisi kipi (varsayılan sil)\n"
        "  --karsi ic|tcp|pty|seri  karşı taraf (sil: ic, tcp; hil: pty, seri; varsayılan ic / seri)\n"
        "  --a1-port <port>         tcp: dinlenecek port (0: boş port)\n"
        "  --sil <yol>              tcp: başlatılacak ida_sil programı\n"
        "  --sil-depo <yol>         tcp: koşucunun kalıcı depo dosyası\n"
        "  --seri <aygıt>           hil: seri aygıt (ör. /dev/cu.usbmodem…, COM5)\n"
        "  --tohum <sayı>           ana tohum (varsayılan 1)\n"
        "  --sure <s>               sim süresi, saniye (varsayılan 90)\n"
        "  --senaryo z1|bos|<dosya.toml>  yerleşik senaryo (varsayılan z1) ya da A5 senaryo dosyası\n"
        "                           (depo köküne göre göreli; kip, süre ve tohum dosyadan)\n"
        "  --kok <dizin>            depo kökü (senaryo dosyası için; varsayılan .)\n"
        "  --ariza <k1,k2,…|yok>    z1 itici arızası kanalları (varsayılan 4,0)\n"
        "  --acil <s> [--acil-kalk <s>]  sanal acil durdurma\n"
        "  --csv <yol>              kontrol adımı başına CSV\n"
        "  --gunluk <0..2>          stderr günlüğü (varsayılan 1)\n"
        "  --a3-port <port>         A3 sunucusu (web panelleri; yalnız 127.0.0.1; 0: boş port); açılış\n"
        "                           adresi standart çıktıya bir kez yazılır (A3-K05)\n"
        "  --a3-kok <dizin>         A3: panellerin derleme çıktısı (web/dist)\n"
        "  --a3-bekle               A3: çalıştırma panelden `baslat` ile başlar (varsayılan kendiliğinden)\n"
        "  --a2-port <port>         A2 sunucusu (görselleştirici; yalnız 127.0.0.1; 1024–65535, A2-K93); yalnız\n"
        "                           senaryo dosyasıyla; bayrak yoksa A2 açılmaz (kilit alınmaz, port açılmaz)\n"
        "  --sil-hiz <çarpan>       SIL hız çarpanının başlangıcı (1: gerçek zaman; 0: olabildiğince hızlı, varsayılan)\n"
        "  --sonar[=acik|kapali]    senaryo dosyasının sonarları (Ping1D, Ping360; sade kademe): karta A1'den,\n"
        "                           A3 açıksa sonar kanallarından (yalnız senaryo dosyasıyla); değersiz = acik;\n"
        "                           bayrak yoksa kapalı\n"
        "  --ping360-ornek <n>      sonar: Ping360 örnek sayısı 200–1200, menzil korunur (varsayılan 1200)\n"
        "  --ping360-bekleme-ms <ms>  sonar: Ping360 pingleri arası bekleme (satır hızının kısılması)\n"
        "  --sonar-csv <yol>        sonar: teslim edilen sonar kayıtlarının CSV'si\n"
        "çıkış kodu: 0 tamam, 1 komut satırı, 2 çalıştırma hatası, 3 geçersiz senaryo (doğrulama raporu\n"
        "standart hataya), 4 senaryo bu dilimde yürütülemez, 5 SIGINT/SIGTERM (Windows: Ctrl-C, Ctrl-Break,\n"
        "pencere kapatma) ile düzgün kapanış, 6 ikinci sinyal (hemen sonlandırma)\n");
}

// Sayısal seçeneklerin tek ayrıştırıcısı (sol #1 üçüncü dar denetimi): değerin tamamı sayı
// olmalı, `errno` dönüşümden önce sıfırlanır ve ERANGE (taşma ya da alt taşma; ör. −1e−999'un
// sessizce 0 olması) hatadır; sonuç sonlu ve [en_az, en_cok] içinde olmalıdır.
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

// Kapanış özet satırının A2 parçası (W5-bagla-a2 kapsam 6). `--a2-port` verilmediyse boş (satır
// değişmez). Sonu ", " ile biter (satırda `oturum` alanından önce yer alır).
//   A2 dinliyor (port P; …sayaçlar…)      sunucu açıldı
//   A2 yok: <neden> (<açıklama>)          A2'siz: kilit_baskasinda | kilit_acilamadi | port_acilamadi |
//                                         ic_hata | gorunum_kapali | tanim_yok
std::string a2_ozet_metni(const ida::sunucu::A2Ozeti& a)
{
    if (!a.istendi) {
        return {};
    }
    if (a.durum != "dinliyor") {
        return "A2 yok: " + a.durum + " (" + a.aciklama + "), ";
    }
    const ida::a2::SunucuSayaclari& c = a.sayac;
    char t[1024];
    std::snprintf(t, sizeof t,
                  "A2 dinliyor (port %u; bağlantı %llu, biten oturum %llu, son kopma %s, meşgul reddi %llu, protokol hatası %llu, "
                  "gönderilen durum %llu, olay %llu, seyreltilen %llu, kuyrukta atılan %llu, kuyruk taşması %llu, "
                  "yayın: sınır %llu, düşen durum %llu, kayıp %llu, olay taşması %llu, geçersiz çağrı %llu), ",
                  static_cast<unsigned>(a.port), static_cast<unsigned long long>(c.baglanti_sayisi),
                  static_cast<unsigned long long>(c.biten_oturum), ida::a2::kopma_nedeni_adi(c.son_kopma_nedeni),
                  static_cast<unsigned long long>(c.mesgul_reddi), static_cast<unsigned long long>(c.protokol_hatasi),
                  static_cast<unsigned long long>(c.gonderilen_durum), static_cast<unsigned long long>(c.gonderilen_olay),
                  static_cast<unsigned long long>(c.seyreltilen_durum), static_cast<unsigned long long>(c.kuyrukta_atilan_durum),
                  static_cast<unsigned long long>(c.kuyruk_tasmasi + c.yayin_tasmasi_kapanis),
                  static_cast<unsigned long long>(a.adim_siniri), static_cast<unsigned long long>(c.yayin.dusen_durum),
                  static_cast<unsigned long long>(c.yayin.kayip_korunan), static_cast<unsigned long long>(c.yayin.olay_tasmasi),
                  static_cast<unsigned long long>(c.yayin.gecersiz_cagri + a.reddedilen_cagri));
    return t;
}

}  // namespace

int main(int argc, char** argv)
{
    ida::sunucu::Ayar a;
    bool karsi_verildi = false;
    for (int i = 1; i < argc; ++i) {
        const std::string s = argv[i];
        auto deger = [&]() -> std::string {
            if (i + 1 >= argc) {
                std::fprintf(stderr, "ida_sim: %s değer ister\n", s.c_str());
                std::exit(1);
            }
            return argv[++i];
        };
        if (s == "--kip") {
            const std::string v = deger();
            a.kip_verildi = true;
            if (v == "sil") {
                a.kip = ida::sunucu::Kip::sil;
            } else if (v == "hil") {
                a.kip = ida::sunucu::Kip::hil;
            } else {
                std::fprintf(stderr, "ida_sim: bilinmeyen kip %s\n", v.c_str());
                return 1;
            }
        } else if (s == "--karsi") {
            const std::string v = deger();
            karsi_verildi = true;
            a.karsi_verildi = true;
            if (v == "ic") {
                a.karsi = ida::sunucu::Karsi::ic;
            } else if (v == "tcp") {
                a.karsi = ida::sunucu::Karsi::tcp;
            } else if (v == "pty") {
                a.karsi = ida::sunucu::Karsi::pty;
            } else if (v == "seri") {
                a.karsi = ida::sunucu::Karsi::seri;
            } else {
                std::fprintf(stderr, "ida_sim: bilinmeyen karşı taraf %s\n", v.c_str());
                return 1;
            }
        } else if (s == "--a1-port") {
            const std::string v = deger();
            double x = 0.0;
            if (!tam_ayristir(v, 0.0, 65535.0, x)) {
                std::fprintf(stderr, "ida_sim: --a1-port 0–65535 bir tamsayı ister: %s\n", v.c_str());
                return 1;
            }
            a.a1_port = static_cast<std::uint16_t>(x);
        } else if (s == "--sil") {
            a.sil_yolu = deger();
        } else if (s == "--sil-depo") {
            a.sil_depo = deger();
        } else if (s == "--seri") {
            a.seri = deger();
            if (!karsi_verildi) {
                a.karsi = ida::sunucu::Karsi::seri;
            }
        } else if (s == "--tohum") {
            const std::string v = deger();
            double x = 0.0;
            if (!tam_ayristir(v, 0.0, 9007199254740991.0, x)) {   // A5-K82: 0 … 2⁵³−1
                std::fprintf(stderr, "ida_sim: --tohum 0 … 2^53−1 bir tamsayı ister: %s\n", v.c_str());
                return 1;
            }
            a.tohum = static_cast<std::uint64_t>(x);
            a.tohum_verildi = true;
        } else if (s == "--sure") {
            const std::string v = deger();
            double x = 0.0;
            if (!sayi_ayristir(v, 0.0, 1e7, x) || !(x > 0.0)) {
                std::fprintf(stderr, "ida_sim: --sure pozitif bir süre (s) ister: %s\n", v.c_str());
                return 1;
            }
            a.sure_s = x;
            a.sure_verildi = true;
        } else if (s == "--kok") {
            a.depo_koku = deger();
        } else if (s == "--senaryo") {
            const std::string v = deger();
            if (v == "z1" || v == "bos") {
                a.senaryo = v;
            } else if (v.size() > 5 && v.compare(v.size() - 5, 5, ".toml") == 0) {
                a.senaryo_dosyasi = v;
            } else {
                std::fprintf(stderr, "ida_sim: bilinmeyen senaryo %s (z1, bos ya da .toml dosyası)\n", v.c_str());
                return 1;
            }
        } else if (s == "--ariza") {
            const std::string v = deger();
            a.ariza_verildi = true;
            a.z1.ariza_kanallari.clear();
            if (v != "yok") {
                std::size_t p = 0;
                while (p < v.size()) {
                    const std::size_t q = v.find(',', p);
                    double k = 0.0;
                    if (!tam_ayristir(v.substr(p, q == std::string::npos ? std::string::npos : q - p), 0.0, 7.0, k)) {
                        std::fprintf(stderr, "ida_sim: kanal 0–7 olmalı\n");
                        return 1;
                    }
                    a.z1.ariza_kanallari.push_back(static_cast<int>(k));
                    p = q == std::string::npos ? v.size() : q + 1;
                }
            }
        } else if (s == "--acil" || s == "--acil-kalk") {
            // EMNİYET (sol #1 dar denetimi): bir kez verilir; değer sonlu ve ≥ 0 bir sayı olmalı.
            bool& verildi = s == "--acil" ? a.acil_verildi : a.acil_kalk_verildi;
            if (verildi) {
                std::fprintf(stderr, "ida_sim: %s bir kez verilebilir\n", s.c_str());
                return 1;
            }
            verildi = true;
            const std::string v = deger();
            double x = 0.0;
            if (!sayi_ayristir(v, 0.0, 1e7, x)) {
                std::fprintf(stderr, "ida_sim: %s sonlu ve ≥ 0 bir süre (s) ister: %s\n", s.c_str(), v.c_str());
                return 1;
            }
            (s == "--acil" ? a.acil_durdurma_s : a.acil_kalkar_s) = x;
        } else if (s == "--csv") {
            a.csv_yolu = deger();
        } else if (s == "--gunluk") {
            const std::string v = deger();
            double x = 0.0;
            if (!tam_ayristir(v, 0.0, 2.0, x)) {
                std::fprintf(stderr, "ida_sim: --gunluk 0..2 ister: %s\n", v.c_str());
                return 1;
            }
            a.gunluk = static_cast<int>(x);
        } else if (s == "--a3-port") {
            const std::string v = deger();
            double x = 0.0;
            if (!tam_ayristir(v, 0.0, 65535.0, x)) {
                std::fprintf(stderr, "ida_sim: --a3-port 0–65535 bir tamsayı ister: %s\n", v.c_str());
                return 1;
            }
            a.a3_acik = true;
            a.a3_port = static_cast<std::uint16_t>(x);
        } else if (s == "--a2-port") {
            // A2-K93: 1024 … 65 535; aralık dışı değerle süreç başlamaz.
            const std::string v = deger();
            double x = 0.0;
            if (!tam_ayristir(v, 1024.0, 65535.0, x)) {
                std::fprintf(stderr, "ida_sim: --a2-port 1024–65535 bir tamsayı ister (A2-K93): %s\n", v.c_str());
                return 1;
            }
            a.a2_acik = true;
            a.a2_port_verildi = true;
            a.a2_port = static_cast<std::uint16_t>(x);
        } else if (s == "--a2-sinama-bos-port") {
            // Yalnız sınama: sabit port yerine işletim sisteminin verdiği boş port (port günlüğe yazılır).
            a.a2_acik = true;
            a.a2_sinama_bos_port = true;
        } else if (s == "--a2-sinama-kilit") {
            // Yalnız sınama: sahiplik kilidinin yolu (A2-K77'deki sabit yer yerine).
            a.a2_kilit_dosyasi = deger();
        } else if (s == "--sil-hiz") {
            const std::string v = deger();
            double x = 0.0;
            if (!sayi_ayristir(v, 0.0, 1000.0, x)) {
                std::fprintf(stderr, "ida_sim: --sil-hiz 0–1000 bir çarpan ister: %s\n", v.c_str());
                return 1;
            }
            a.sil_hiz = x;
        } else if (s == "--a3-kok") {
            a.a3_kok = deger();
        } else if (s == "--a3-bekle") {
            a.a3_bekle = true;
        } else if (s == "--sonar" || s.rfind("--sonar=", 0) == 0) {
            // W4-sunucu-kucuk kapsam 5: değersiz `--sonar` = `acik`; `--sonar=acik|kapali`. Varsayılan
            // (bayrak yok) kapalıdır.
            const std::string v = s == "--sonar" ? "acik" : s.substr(8);
            if (v == "acik") {
                a.sonar_acik = true;
            } else if (v == "kapali") {
                a.sonar_acik = false;
            } else {
                std::fprintf(stderr, "ida_sim: --sonar acik ya da kapali ister: %s\n", v.c_str());
                return 1;
            }
            a.sonar_verildi = true;
        } else if (s == "--ping360-ornek") {
            const std::string v = deger();
            double x = 0.0;
            if (!tam_ayristir(v, 200.0, 1200.0, x)) {
                std::fprintf(stderr, "ida_sim: --ping360-ornek 200–1200 bir tamsayı ister: %s\n", v.c_str());
                return 1;
            }
            a.ping360_ornek = static_cast<int>(x);
        } else if (s == "--ping360-bekleme-ms") {
            const std::string v = deger();
            double x = 0.0;
            if (!sayi_ayristir(v, 0.0, 10000.0, x)) {
                std::fprintf(stderr, "ida_sim: --ping360-bekleme-ms 0–10000 ister: %s\n", v.c_str());
                return 1;
            }
            a.ping360_bekleme_ms = x;
        } else if (s == "--sonar-csv") {
            a.sonar_csv_yolu = deger();
        } else if (s == "--yardim" || s == "-h") {
            kullanim();
            return 0;
        } else {
            std::fprintf(stderr, "ida_sim: bilinmeyen seçenek %s\n", s.c_str());
            kullanim();
            return 1;
        }
    }
    if (!a.senaryo_dosyasi.empty() && !a.seri.empty()) {
        a.karsi_verildi = true;   // --seri karşı tarafı seri yapar
    }
    // Senaryo dosyasında kip dosyadandır: kip ve karşı taraf denetimi çalıştırıcıda yapılır.
    if (a.senaryo_dosyasi.empty() && a.kip == ida::sunucu::Kip::hil && !karsi_verildi && a.seri.empty()) {
        std::fprintf(stderr, "ida_sim: hil kipi --seri <aygıt> ya da --karsi pty ister\n");
        return 1;
    }
    if (a.senaryo_dosyasi.empty() && a.kip == ida::sunucu::Kip::hil &&
        (a.karsi == ida::sunucu::Karsi::ic || a.karsi == ida::sunucu::Karsi::tcp)) {
        std::fprintf(stderr, "ida_sim: hil kipinde karşı taraf pty ya da seri olmalı\n");
        return 1;
    }
    if (a.senaryo_dosyasi.empty() && a.kip == ida::sunucu::Kip::sil &&
        (a.karsi == ida::sunucu::Karsi::pty || a.karsi == ida::sunucu::Karsi::seri)) {
        std::fprintf(stderr, "ida_sim: sil kipinde karşı taraf ic ya da tcp olmalı\n");
        return 1;
    }
    if (!a.a3_acik && (!a.a3_kok.empty() || a.a3_bekle)) {
        std::fprintf(stderr, "ida_sim: --a3-kok ve --a3-bekle --a3-port ister\n");
        return 1;
    }
    if (a.a2_port_verildi && a.a2_sinama_bos_port) {
        std::fprintf(stderr, "ida_sim: --a2-port ile --a2-sinama-bos-port birlikte verilemez\n");
        return 1;
    }
    if (!a.a2_acik && !a.a2_kilit_dosyasi.empty()) {
        std::fprintf(stderr, "ida_sim: --a2-sinama-kilit --a2-port ister\n");
        return 1;
    }
    if (a.a2_acik && a.senaryo_dosyasi.empty()) {
        // W5-bagla-a2: varlık kümesi, ortam ve görünüm kademesi senaryo dosyasındandır.
        std::fprintf(stderr, "ida_sim: --a2-port senaryo dosyası ister (--senaryo <dosya.toml>)\n");
        return 1;
    }
    a.csv_uret = !a.csv_yolu.empty();
    // A3-K04, K05: açılış adresi (erişim jetonuyla) standart çıktıya bir kez; günlüğe yazılmaz.
    a.a3_acilis = [](const std::string& adres) {
        std::printf("%s\n", adres.c_str());
        std::fflush(stdout);
    };
    // W4-sunucu-kucuk kapsam 1: SIGINT/SIGTERM (Windows: konsol denetim olayları) yalnız bayrak kurar;
    // koşu bayrağı sınırda görür ve var olan hata kapanışını işletir. Kurulamazsa kapanış hızlanmaz
    // (kart zaman aşımıyla güvenli duruma geçer); koşu sürer.
    if (!ida::sunucu::sinyal_kur()) {
        std::fprintf(stderr, "ida_sim: uyarı: sinyal işleyicileri kurulamadı; kesilirse kapanış çerçevesi gitmez\n");
    }
    const ida::sunucu::Sonuc s = ida::sunucu::kos(a);
    const ida::kopru::Sayaclar& c = s.sayac;
    using ida::sunucu::HataTuru;
    if (s.hata_turu == HataTuru::senaryo_gecersiz) {
        std::fprintf(stderr, "ida_sim: %s\n%s\n", s.hata.c_str(), s.dogrulama_raporu.c_str());
        return 3;
    }
    if (s.hata_turu == HataTuru::senaryo_yurutulemez || s.hata_turu == HataTuru::kullanim) {
        std::fprintf(stderr, "ida_sim: %s\n", s.hata.c_str());
        return s.hata_turu == HataTuru::kullanim ? 1 : 4;
    }
    // W4-sunucu-kucuk kapsam 2: her kapanışta (düzgün, hata, sinyal) bir özet satırı; var olan sayaçlar.
    // Yazılım günlük düzeyinden bağımsızdır (kapanışın tek satırlık kaydı). `düşen kayıt`: A3 yayın
    // halkasında taşan, SIL tamponunda oturum bitince işlenmeden atılan ve oturumla atılan Ping360
    // kaydı; köprüde "yeniden gönderim" sayacı yoktur, alanlar gerçekte saydıklarıyla adlandırılır: yinelenen
    // yanıt reddi (A1 yinelenen çerçeve) ve yeniden bağlanma (port yeniden açılması);
    // `sonar taşma`: kuyruk, teslim deposu ve tampon yetersizliği.
    // W5-bagla-a2 (kapsam 6): `--a2-port` verildiyse A2 durumu ve sayaçları aynı satıra, `oturum`
    // alanından önce eklenir; verilmediyse satır bu iş öncesiyle bayt bayt aynıdır (W5-K31).
    const std::string a2_ozeti = a2_ozet_metni(s.a2);
    std::fprintf(stderr,
                 "ida_sim: kapanış: %s; kaçırılan parti %llu, düşen kayıt (A3 halka %llu, SIL tampon %llu, Ping360 oturum %llu), "
                 "yinelenen yanıt reddi %llu, yeniden bağlanma %llu, sonar taşma (kuyruk %llu, teslim %llu, tampon %llu), "
                 "%soturum %llu, kapanış yazımı eksik %llu, son hata: %s\n",
                 s.sinyal_kesti ? "sinyal" : (s.tamam ? "düzgün" : "hata"),
                 static_cast<unsigned long long>(c.parti_kacirildi), static_cast<unsigned long long>(s.a3_halka_tasma),
                 static_cast<unsigned long long>(c.sil_tampon_atilan), static_cast<unsigned long long>(c.ping360_oturum_atilan),
                 static_cast<unsigned long long>(c.yanit_ret_yinelenen), static_cast<unsigned long long>(s.yeniden_baglanti),
                 static_cast<unsigned long long>(s.sonar_kuyruk_tasma), static_cast<unsigned long long>(s.sonar_teslim_tasma),
                 static_cast<unsigned long long>(s.sonar_tampon_yetersiz), a2_ozeti.c_str(),
                 static_cast<unsigned long long>(c.oturum_kurulan),
                 static_cast<unsigned long long>(s.kapanis_yazim_eksik), s.hata.empty() ? "yok" : s.hata.c_str());
    if (a.gunluk >= 2) {
        std::fprintf(stderr, "ida_sim: başlangıç O_b derinliği %.17g m\n", s.denge_derinligi_m);
    }
    if (a.gunluk >= 1 && !a.senaryo_dosyasi.empty()) {
        std::fprintf(stderr, "ida_sim: senaryo %s, davranış özeti %s\n", a.senaryo_dosyasi.c_str(),
                     s.davranis_ozeti.c_str());
        for (const auto& [dosya, ozet] : s.kaynak_ozetleri) {
            std::fprintf(stderr, "ida_sim:   kaynak %s %s\n", dosya.c_str(), ozet.c_str());
        }
        for (const ida::sunucu::DogrulamaBulgusu& b : s.dogrulama_bulgulari) {
            std::fprintf(stderr, "ida_sim:   doğrulama %s %s %s%s%s %s: %s\n", b.onem.c_str(), b.kural.c_str(),
                         b.dosya.c_str(), b.satir >= 0 ? ":" : "", b.satir >= 0 ? std::to_string(b.satir).c_str() : "",
                         b.yol.c_str(), b.ileti.c_str());
        }
        for (const std::string& u : s.senaryo_uyarilari) {
            std::fprintf(stderr, "ida_sim:   uyarı: %s\n", u.c_str());
        }
        std::size_t uyg = 0, red = 0, yok = 0;
        for (const auto& o : s.olay_sonuclari) {
            if (o.kaynak != "senaryo") {
                continue;   // A3 komutları ayrı satırda
            }
            uyg += o.sonuc == "uygulandi" ? 1u : 0u;
            red += o.sonuc == "reddedildi" ? 1u : 0u;
            yok += o.sonuc == "uygulanmadi" ? 1u : 0u;
        }
        std::fprintf(stderr,
                     "ida_sim:   olaylar: uygulandı %zu, reddedildi %zu, uygulanmadı %zu; atama çelişkisi %zu, "
                     "gevşetme reddi %zu; görev %s\n",
                     uyg, red, yok, s.atama_celiskileri.size(), s.gevsetme_retleri.size(), s.gorev_durumu.c_str());
    }
    if (a.gunluk >= 1) {
        std::fprintf(stderr,
                     "ida_sim: %s; parti %llu, kaçırılan %llu, geç yanıt %llu, yinelenen %llu, alan/bağ reddi %llu/%llu, "
                     "oturum %llu, kopma %llu, hazırlık %llu, çalıştırma geçersiz 0x%x; karttan seq: sıra dışı %llu, "
                     "kayıp %llu (çalıştırma oturumunda %llu/%llu)\n",
                     s.tamam ? "tamam" : ("HATA: " + s.hata).c_str(),
                     static_cast<unsigned long long>(c.parti_gonderilen),
                     static_cast<unsigned long long>(c.parti_kacirildi),
                     static_cast<unsigned long long>(c.yanit_ret_gec),
                     static_cast<unsigned long long>(c.yanit_ret_yinelenen),
                     static_cast<unsigned long long>(c.alan_ret), static_cast<unsigned long long>(c.bag_ret),
                     static_cast<unsigned long long>(c.oturum_kurulan), static_cast<unsigned long long>(c.oturum_kopma),
                     static_cast<unsigned long long>(s.hazirlik_oturumu), c.calistirma_gecersiz,
                     static_cast<unsigned long long>(c.rx_sira_disi), static_cast<unsigned long long>(c.rx_kayip),
                     static_cast<unsigned long long>(s.zaman.rx_sira_disi_kosu),
                     static_cast<unsigned long long>(s.zaman.rx_kayip_kosu));
        for (std::uint32_t j = 0; j < c.eszamansiz_kayit_n && j < c.eszamansiz_kayit.size(); ++j) {
            const auto& e = c.eszamansiz_kayit[j];
            std::fprintf(stderr,
                         "ida_sim: tanı: A1-K79'a sayılan eşzamansız güvenli çıktı: sınır %llu (%.4f s), out_seq %u, "
                         "act_flags 0x%x, önceki uygulanan out_seq %u, oturumda olağan yanıt alınmıştı %d\n",
                         static_cast<unsigned long long>(e.adim), 1.25e-3 * static_cast<double>(e.adim), e.out_seq,
                         static_cast<unsigned>(e.act_flags), e.onceki_out_seq, e.ilk_yanit_alindi ? 1 : 0);
        }
        if (a.a3_acik) {
            std::fprintf(stderr,
                         "ida_sim: A3: çalıştırma %s; dünyaya ulaşan komut %llu; yayın çağrısı %llu, tek çağrı en çok %.1f µs, "
                         "sınırdaki çağrılar en çok %.1f µs (ölçüm, duvar); halka taşması %llu\n",
                         s.calistirma_sonu.empty() ? "başlamadı" : s.calistirma_sonu.c_str(),
                         static_cast<unsigned long long>(s.a3_komut), static_cast<unsigned long long>(s.a3_yayin_cagrisi),
                         1e-3 * static_cast<double>(s.a3_cagri_en_kotu_ns), 1e-3 * static_cast<double>(s.a3_sinir_en_kotu_ns),
                         static_cast<unsigned long long>(s.a3_halka_tasma));
        }
        if (s.a2.istendi && s.a2.durum == "dinliyor") {
            std::fprintf(stderr,
                         "ida_sim: A2: çalıştırma tanımı %s (kademe %u, araç gövde kimliği %u); adım sınırı çağrısı %llu, ortalama "
                         "%.3f µs, en çok %.1f µs (dünya iş parçacığı, duvar; ölçüm); gönderilen ileti %llu, nabız %llu, en büyük "
                         "kuyruk %llu B\n",
                         s.a2.calistirma_tanimlandi ? "verildi" : "VERİLEMEDİ", static_cast<unsigned>(s.a2.kademe),
                         static_cast<unsigned>(s.a2.arac_kimligi), static_cast<unsigned long long>(s.a2.adim_siniri),
                         s.a2.adim_siniri != 0u
                             ? 1e-3 * static_cast<double>(s.a2.cagri_toplam_ns) / static_cast<double>(s.a2.adim_siniri)
                             : 0.0,
                         1e-3 * static_cast<double>(s.a2.cagri_en_kotu_ns),
                         static_cast<unsigned long long>(s.a2.sayac.gonderilen_ileti),
                         static_cast<unsigned long long>(s.a2.sayac.gonderilen_nabiz),
                         static_cast<unsigned long long>(s.a2.sayac.azami_kuyruk_bayt));
        }
        if (s.sonar_acik) {
            std::vector<std::int64_t> p = s.sonar_adim_ping_ns;
            std::sort(p.begin(), p.end());
            const auto yuzde = [&](double q) {
                return p.empty() ? 0.0 : 1e-3 * static_cast<double>(p[std::min(p.size() - 1u, static_cast<std::size_t>(q * static_cast<double>(p.size())))]);
            };
            std::fprintf(stderr,
                         "ida_sim: sonar: Ping1D %llu, Ping360 %llu kayıt (ping %llu, havada %llu, kayıp %llu); köprü: Ping360 "
                         "satır %llu, çerçeve %llu, seyreltme %llu, en büyük teslim gecikmesi %llu periyot, oturumla atılan %llu, "
                         "Ping1D üye %llu; parti en büyük %llu B / %llu üye; sonar adımı (dünya, duvar; ölçüm): en uzun %.1f µs, "
                         "ping sınırlarında p50/p99 %.1f/%.1f µs (sınırın bekleyişinden önce); sınır içinde boşaltma en uzun "
                         "%.1f µs; A_sensör %.1f B/parti (sınır 150); kuyruk taşması %llu, teslim deposu taşması %llu, "
                         "tampon yetersiz %llu\n",
                         static_cast<unsigned long long>(s.sonar_ping1d), static_cast<unsigned long long>(s.sonar_ping360),
                         static_cast<unsigned long long>(s.sonar_ping), static_cast<unsigned long long>(s.sonar_havada),
                         static_cast<unsigned long long>(s.sonar_kayip), static_cast<unsigned long long>(c.ping360_satir),
                         static_cast<unsigned long long>(c.ping360_cerceve), static_cast<unsigned long long>(c.ping360_seyreltildi),
                         static_cast<unsigned long long>(c.ping360_gecikme_en_buyuk),
                         static_cast<unsigned long long>(c.ping360_oturum_atilan), static_cast<unsigned long long>(c.ping1d_uye),
                         static_cast<unsigned long long>(c.parti_bayt_en_buyuk), static_cast<unsigned long long>(c.parti_uye_en_buyuk),
                         1e-3 * static_cast<double>(s.sonar_adim_en_uzun_ns), yuzde(0.5), yuzde(0.99),
                         1e-3 * static_cast<double>(s.sonar_bosaltma_en_uzun_ns), s.sonar_a_sensor_bayt,
                         static_cast<unsigned long long>(s.sonar_kuyruk_tasma),
                         static_cast<unsigned long long>(s.sonar_teslim_tasma),
                         static_cast<unsigned long long>(s.sonar_tampon_yetersiz));
        }
        if (a.senaryo == "z1" && a.senaryo_dosyasi.empty()) {
            std::fprintf(stderr, "ida_sim: z1 komutları: ARM %d (neden %d), SET_MODE %d, GUIDED_SP %d/%d, DISARM %d\n",
                         s.arm_ack, s.arm_neden, s.kip_ack, s.sp_ack, s.sp2_ack, s.disarm_ack);
        }
        if (s.kip == ida::sunucu::Kip::hil) {   // etkin kip (dosyadan seçilebilir; sol #5)
            std::fprintf(stderr,
                         "ida_sim: hil: TF %llu, periyot en az/ort/en çok %.3f/%.3f/%.3f ms, TF geç yazım en çok %.3f ms, "
                         "sim gecikmesi %llu, son tarih %llu, gerçek zaman kaçırma %llu\n",
                         static_cast<unsigned long long>(s.zaman.tf_sayisi), 1e-6 * static_cast<double>(s.zaman.periyot_en_az_ns),
                         1e-6 * s.zaman.periyot_ort_ns, 1e-6 * static_cast<double>(s.zaman.periyot_en_cok_ns),
                         1e-6 * static_cast<double>(s.zaman.gec_yazim_en_cok_ns),
                         static_cast<unsigned long long>(s.zaman.sim_gecikmesi),
                         static_cast<unsigned long long>(c.son_tarih_kacirildi),
                         static_cast<unsigned long long>(s.zaman.gercek_zaman_kacirma));
            std::fprintf(stderr,
                         "ida_sim: hil: yanıt süresi (TF yazımı → yanıt alımı) %llu yanıt, p50/p99/en çok %.3f/%.3f/%.3f ms; "
                         "çalıştırma oturumunda karttan gelen: CRC %llu, atlanan bayt %llu, sıra boşluğu %llu "
                         "(toplam %llu/%llu/%llu); eşzamansız güvenli çıktı %llu (ilk parti öncesi, A1-K79 dışı: %llu)\n",
                         static_cast<unsigned long long>(s.zaman.yanit_sayisi), 1e-6 * static_cast<double>(s.zaman.yanit_p50_ns),
                         1e-6 * static_cast<double>(s.zaman.yanit_p99_ns), 1e-6 * static_cast<double>(s.zaman.yanit_en_cok_ns),
                         static_cast<unsigned long long>(s.zaman.rx_crc_kosu),
                         static_cast<unsigned long long>(s.zaman.rx_resync_kosu),
                         static_cast<unsigned long long>(s.zaman.rx_seq_gap_kosu),
                         static_cast<unsigned long long>(c.rx_crc_err), static_cast<unsigned long long>(c.rx_resync_bytes),
                         static_cast<unsigned long long>(c.rx_seq_gap),
                         static_cast<unsigned long long>(c.esz_guvenli_uygulanan),
                         static_cast<unsigned long long>(c.eszamansiz_ilk_parti_oncesi));
            if (!s.zaman.sira_boslugu_t_ns.empty()) {
                std::fprintf(stderr, "ida_sim: hil: oturum içi ilk sıra boşlukları (sim zamanı, s):");
                for (std::int64_t t : s.zaman.sira_boslugu_t_ns) {
                    std::fprintf(stderr, " %.4f", 1e-9 * static_cast<double>(t));
                }
                std::fprintf(stderr, "\n");
            }
            std::fprintf(stderr, "ida_sim: hil döngü %s", s.evre_ozeti.c_str());
            if (s.kart.fc_var) {
                std::fprintf(stderr, "ida_sim: kart atış kontrol durumu (son IDA_FC_STATUS): state %u, inhibit_mask 0x%x\n",
                             static_cast<unsigned>(s.kart.fc_state), static_cast<unsigned>(s.kart.fc_inhibit_mask));
            }
        }
    }
    ida::sunucu::kapanis_tamamlandi();   // Windows pencere kapatma işleyicisi bunu bekler
    return s.sinyal_kesti ? ida::sunucu::kCikisSinyal : (s.tamam ? 0 : 2);
}
