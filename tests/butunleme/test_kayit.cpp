// W5-bagla-kayit — A6 kayıt kitaplığının `ida_sim`'e bağlanması: bütünleme testleri
// (docs/gorevler/W5-bagla-kayit.md kabul 1–4; sim/sunucu/BENIOKU.md "A6 kayıt").
//
// Eşzamanlama süreye değil olaya göre yapılır (sabit uyku yok): SIL koşuları kitaplıkta eşzamanlı
// koşar ve biter; öldürme sınamasında hedef süreç, dosyada kapanmış bir parça görününceye kadar
// yoklanır (her bekleyişin asılma sınırı vardır, ölçüt değildir). Hıza bağlı eşik yoktur; HIL
// sayaç karşılaştırması ortak zamanlama yardımcısından geçer (W4-K24, K29). Geçici dizin adları
// benzersizdir; sabit port ve gerçek kilit dosyası yoktur.
//
//   kayit_varsayilan   kabul 1: `--kayit-dizini` yokken dosya açılmaz, özet boş, dizin oluşmaz;
//                      yerleşik senaryoyla kullanım hatası; var olan dosya ezilmez ve çalıştırma
//                      başlamaz (A6-K01)
//   kayit_belirlenim   kabul 2 (SIL): kapalı ve üç kademede bayt bayt aynı CSV ve iki yönde aynı A1 akışı
//   kayit_icerik       kabul 3: `saglam` açılır; `fizik/durum` pozu CSV satırıyla bit düzeyinde aynı;
//                      ileti sayıları; Tablo 3.3 süzgeci; üretilen/eksik kanal tablosu; güvence (W5-K41)
//   kayit_a3_acik      A3 açık: aynı yapı iki hedefe (W5-K42); olay, çalıştırma durumu ve komut sonucu
//   kayit_kapanis      kabul 4: olağan, hata, sinyal; (POSIX) `kill -9` sonrası kurtarma okuması
//   kayit_hil_sayac    kabul 2 (POSIX, kartsız HIL 1x): kaçırılan parti ve bütçe aşımı sayaçları farklı değil
#include "ida/a3/komut.hpp"
#include "ida/kayit/coz.hpp"
#include "ida/kayit/okuyucu.hpp"
#include "ida/senaryo/senaryo.hpp"
#include "ida/sunucu/sunucu.hpp"
#include "../../sim/kopru/test/zamanlama_olcutu.hpp"   // W4-K24, W4-K29

#include <nlohmann/json.hpp>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iterator>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <thread>
#include <vector>

#ifndef _WIN32
#include <csignal>
#include <fcntl.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

#ifndef IDA_DEPO_KOKU
#define IDA_DEPO_KOKU "."
#endif

namespace {

namespace sunucu = ida::sunucu;
namespace senaryo = ida::senaryo;
namespace kayit = ida::kayit;
namespace fs = std::filesystem;

int g_hata = 0;

void dogrula(bool kosul, const std::string& ileti)
{
    std::printf("%s %s\n", kosul ? "GECTI " : "KALDI ", ileti.c_str());
    if (!kosul) {
        ++g_hata;
    }
}

std::string sayi(std::uint64_t v) { return std::to_string(v); }

constexpr std::int64_t kH = 1'250'000;   // fizik adımı (ns)

// FNV-1a (A1 bayt akışlarının karşılaştırması).
struct Fnv {
    std::uint64_t h = 14695981039346656037ULL;
    std::uint64_t n = 0;
    void ekle(const std::uint8_t* p, std::size_t m)
    {
        for (std::size_t i = 0; i < m; ++i) {
            h ^= p[i];
            h *= 1099511628211ULL;
        }
        n += m;
    }
    bool operator==(const Fnv& o) const { return h == o.h && n == o.n; }
};

// Tekil geçici dizin: sayaç + süreç + saat; bitince silinir.
struct Gecici {
    fs::path yol;
    explicit Gecici(const char* ad)
    {
        static std::atomic<unsigned> sayac{0};
        const auto t = std::chrono::steady_clock::now().time_since_epoch().count();
#ifdef _WIN32
        const unsigned long long pid = 0;
#else
        const auto pid = static_cast<unsigned long long>(::getpid());
#endif
        yol = fs::temp_directory_path() / ("ida_kayit_" + std::string(ad) + "_" + std::to_string(pid) + "_" +
                                           std::to_string(sayac.fetch_add(1)) + "_" + std::to_string(t));
    }
    Gecici(const Gecici&) = delete;
    Gecici& operator=(const Gecici&) = delete;
    ~Gecici()
    {
        std::error_code ec;
        fs::remove_all(yol, ec);
    }
    std::string dizge() const { return yol.string(); }
};

// Bir dizindeki tek `.mcap` dosyası (yoksa boş yol).
fs::path tek_dosya(const fs::path& dizin)
{
    std::error_code ec;
    fs::path bulunan;
    std::size_t n = 0;
    for (const auto& e : fs::directory_iterator(dizin, ec)) {
        if (e.path().extension() == ".mcap") {
            bulunan = e.path();
            ++n;
        }
    }
    return n == 1u ? bulunan : fs::path();
}

std::shared_ptr<senaryo::BellekKaynagi> kaynak(const std::string& dosya, const std::function<void(std::string&)>& duzenle = {})
{
    const senaryo::DiskKaynagi d(IDA_DEPO_KOKU);
    auto b = std::make_shared<senaryo::BellekKaynagi>();
    for (const char* y : {"scenarios/ida0.toml", "assets/ornek/manifest.json"}) {
        b->dosyalar[y] = d.oku(y).value_or("");
    }
    std::string metin = d.oku(dosya).value_or("");
    if (duzenle) {
        duzenle(metin);
    }
    b->dosyalar[dosya] = std::move(metin);
    return b;
}

void degistir(std::string& s, const std::string& a, const std::string& b)
{
    const std::size_t i = s.find(a);
    if (i == std::string::npos) {
        dogrula(false, "sınama hatası: metinde yok: " + a);
        return;
    }
    s.replace(i, a.size(), b);
}

// Senaryonun süresi (`sure_ns = …` satırı) ve kayıt kademesi (`[kalite]` başlığının altına `kayit = …`).
std::function<void(std::string&)> duzen(double saniye, const std::string& kademe = {})
{
    return [saniye, kademe](std::string& m) {
        const std::size_t i = m.find("sure_ns = ");
        const std::size_t j = i == std::string::npos ? i : m.find('\n', i);
        if (i == std::string::npos || j == std::string::npos) {
            dogrula(false, "sınama hatası: sure_ns satırı yok");
            return;
        }
        m.replace(i, j - i, "sure_ns = " + std::to_string(static_cast<long long>(saniye * 1e9)));
        if (!kademe.empty()) {
            degistir(m, "[kalite]\n", "[kalite]\nkayit = \"" + kademe + "\"\n");
        }
    };
}

constexpr const char* kZ1 = "scenarios/z1.toml";

struct Kosu {
    sunucu::Ayar a;
    sunucu::Sonuc s;
    Fnv giden, gelen;
};

Kosu kos_et(sunucu::Ayar a)
{
    Kosu k;
    a.gunluk = 0;
    a.depo_koku = IDA_DEPO_KOKU;
    a.csv_uret = true;
    a.a1_bayt_kancasi = [&k](bool sim_giden, const std::uint8_t* p, std::size_t n) { (sim_giden ? k.giden : k.gelen).ekle(p, n); };
    k.a = a;
    k.s = sunucu::kos(a);
    return k;
}

sunucu::Ayar z1_ayari(double sure_s, const std::string& kademe = {})
{
    sunucu::Ayar a;
    a.senaryo_dosyasi = kZ1;
    a.senaryo_kaynagi = kaynak(kZ1, duzen(sure_s, kademe));
    return a;
}

// Yükü A3'ün JSON dökümüne çevirir (64 bit tamsayılar onluk dize; kayan nokta en kısa gidiş-dönüş).
nlohmann::json yuk_json(const kayit::KanalTanimi& k, std::span<const std::uint8_t> veri)
{
    std::string s;
    const kayit::CozmeSonucu c = kayit::json_dok(*k.yuk, veri, s);
    return c.tamam ? nlohmann::json::parse(s, nullptr, false) : nlohmann::json();
}

std::uint64_t bitler(double v)
{
    std::uint64_t u = 0;
    std::memcpy(&u, &v, sizeof u);
    return u;
}

struct Okunan {
    std::unique_ptr<kayit::Okuyucu> o;
    std::map<std::string, std::uint64_t> sayilar;      // kanal → ileti sayısı
    std::map<std::string, std::vector<nlohmann::json>> yukler;   // yalnız istenen kanallar
};

Okunan oku(const fs::path& yol, const std::set<std::string>& yuk_kanallari = {})
{
    Okunan r;
    kayit::AcmaHatasi h{};
    r.o = kayit::Okuyucu::ac(yol, h);
    if (!r.o) {
        return r;
    }
    r.o->basa_sar();
    kayit::Ileti i;
    while (r.o->sonraki(i)) {
        const std::string ad(i.kanal->ad);
        ++r.sayilar[ad];
        if (yuk_kanallari.count(ad) != 0u) {
            r.yukler[ad].push_back(yuk_json(*i.kanal, i.veri));
        }
    }
    return r;
}

std::set<std::string> kume(const std::map<std::string, std::uint64_t>& m)
{
    std::set<std::string> s;
    for (const auto& [k, v] : m) {
        if (v > 0u) {
            s.insert(k);
        }
    }
    return s;
}

std::string birlestir(const std::set<std::string>& s)
{
    std::string r;
    for (const std::string& x : s) {
        r += (r.empty() ? "" : " ") + x;
    }
    return r;
}

// Dünya durumundan A3 kapalıyken üretilen kanallar (kademe olaylar / telemetri / tam).
const std::set<std::string> kOlaylar = {"senaryo/oge_uygulama", "kart/cekirdek_sayaclari", "sayac/gercek_zaman", "emniyet/girdiler",
                                         "atis/durum"};
const std::set<std::string> kTelemetri = {"senaryo/oge_uygulama", "kart/cekirdek_sayaclari", "sayac/gercek_zaman", "emniyet/girdiler",
                                           "atis/durum", "fizik/durum", "kart/durum", "sensor/basinc", "sensor/dvl", "sensor/batarya"};

// ------------------------------------------------------------------------------------------------ kabul 1

int test_varsayilan()
{
    {
        // `--kayit-dizini` yok (kayit_acik false; dizin verilse bile): dosya, dizin ve özet yok (W5-K40).
        Gecici g("varsayilan");
        sunucu::Ayar a = z1_ayari(2.0);
        a.kayit_dizini = (g.yol / "olmayacak").string();
        const Kosu k = kos_et(a);
        dogrula(k.s.tamam && !k.s.kayit.istendi && !k.s.kayit.acildi && k.s.kayit.uretilen == 0u && k.s.kayit.yol.empty() &&
                    k.s.kayit.uretilen_kanallar.empty() && k.s.kayit.eksik_kanallar.empty(),
                "kayıt kapalı: özet boş");
        dogrula(!fs::exists(g.yol / "olmayacak"), "kayıt kapalı: dizin oluşturulmadı");
    }
    {
        // Yerleşik senaryo: çözümlenmiş senaryo ve A7 manifesti yok → kullanım hatası; dosya oluşmaz.
        Gecici g("yerlesik");
        sunucu::Ayar a;
        a.sure_s = 1.0;
        a.sure_verildi = true;
        a.kayit_acik = true;
        a.kayit_dizini = g.dizge();
        const Kosu k = kos_et(a);
        dogrula(!k.s.tamam && k.s.hata_turu == sunucu::HataTuru::kullanim && k.s.hata.find("--kayit-dizini yalnız senaryo dosyasıyla") != std::string::npos &&
                    !fs::exists(g.yol),
                "yerleşik senaryo + --kayit-dizini: kullanım hatası, dizin yok (" + k.s.hata + ")");
    }
    {
        // A6-K01: aynı adlı dosya varsa ezilmez ve çalıştırma başlamaz.
        Gecici g("var");
        std::error_code ec;
        fs::create_directories(g.yol, ec);
        const std::string ad = "20261125-000000-abcdef";
        const fs::path dosya = g.yol / (ad + ".mcap");
        {
            std::ofstream f(dosya, std::ios::binary);
            f << "BU-DOSYA-EZILMEMELI";
        }
        sunucu::Ayar a = z1_ayari(2.0);
        a.kayit_acik = true;
        a.kayit_dizini = g.dizge();
        a.kayit_run_id = ad;
        const Kosu k = kos_et(a);
        std::ifstream f(dosya, std::ios::binary);
        const std::string icerik((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
        dogrula(icerik == "BU-DOSYA-EZILMEMELI", "var olan dosya ezilmedi");
        dogrula(!k.s.tamam && k.s.hata.find("kayıt açılamadı") != std::string::npos && !k.s.kayit.acildi &&
                    k.s.sayac.parti_gonderilen == 0u && k.s.satirlar.empty() && k.s.csv.empty(),
                "var olan dosya: çalıştırma başlamadı (parti " + sayi(k.s.sayac.parti_gonderilen) + "): " + k.s.hata);
    }
    return g_hata;
}

// ------------------------------------------------------------------------------------------------ kabul 2

int test_belirlenim()
{
    for (const char* kademe : {"olaylar", "telemetri", "tam"}) {
        const std::string ad = std::string("kademe ") + kademe;
        Gecici g("belirlenim");
        // Başvuru: aynı senaryo metni (kalite.kayit dahil), kayıt kapalı.
        const Kosu ref = kos_et(z1_ayari(10.0, kademe));
        sunucu::Ayar a = z1_ayari(10.0, kademe);
        a.kayit_acik = true;
        a.kayit_dizini = g.dizge();
        const Kosu k = kos_et(a);
        dogrula(ref.s.tamam && k.s.tamam, ad + ": iki koşu da tamamlandı (" + k.s.hata + ")");
        dogrula(!ref.s.csv.empty() && k.s.csv == ref.s.csv, ad + ": CSV bayt bayt aynı");
        dogrula(ref.giden == k.giden && ref.gelen == k.gelen && k.giden.n > 0u,
                ad + ": A1 akışı iki yönde aynı (" + sayi(k.giden.n) + " / " + sayi(k.gelen.n) + " B)");
        dogrula(ref.s.sayac.parti_gonderilen == k.s.sayac.parti_gonderilen && ref.s.sayac.parti_kacirildi == k.s.sayac.parti_kacirildi,
                ad + ": parti sayaçları aynı");
        dogrula(k.s.kayit.acildi && k.s.kayit.dosya_kapandi && k.s.kayit.kademe == kademe, ad + ": kayıt açıldı ve kapandı");
    }
    return g_hata;
}

// ------------------------------------------------------------------------------------------------ kabul 3

int test_icerik()
{
    const double sure_s = 10.0;
    const std::uint64_t n_sinir = static_cast<std::uint64_t>(sure_s * 1e9) / static_cast<std::uint64_t>(kH);   // 8000
    for (const char* kademe : {"olaylar", "telemetri", "tam"}) {
        const std::string ad = std::string("kademe ") + kademe;
        Gecici g("icerik");
        sunucu::Ayar a = z1_ayari(sure_s, kademe);
        a.kayit_acik = true;
        a.kayit_dizini = g.dizge();
        const Kosu k = kos_et(a);
        const fs::path dosya = tek_dosya(g.yol);
        dogrula(k.s.tamam && !dosya.empty(), ad + ": koşu tamam, tek dosya (" + k.s.hata + ")");
        if (dosya.empty()) {
            continue;
        }
        Okunan r = oku(dosya, {"fizik/durum", "senaryo/oge_uygulama"});
        dogrula(r.o && r.o->ozet().saglam() && r.o->ozet().ust_veri_gecerli, ad + ": `Okuyucu` ile saglam açıldı, üst veri geçerli");
        if (!r.o) {
            continue;
        }
        const kayit::DosyaOzeti& oz = r.o->ozet();
        dogrula(fs::path(dosya).stem().string() == k.s.kayit.run_id && oz.ust_veri.at("run_id") == k.s.kayit.run_id,
                ad + ": dosya adı = run_id = üst veri");
        dogrula(oz.kapanis.has_value() && oz.kapanis->at("bitis_durumu") == "bitti" &&
                    oz.kapanis->at("son_adim_no") == sayi(n_sinir + 1u) && oz.kapanis->at("ileti_sayisi") == sayi(oz.ileti_sayisi),
                ad + ": kapanış kaydı (bitti, son_adim_no " + (oz.kapanis ? oz.kapanis->at("son_adim_no") : "?") + ")");
        dogrula(oz.eksikler.empty() && k.s.kayit.dusen_halka == 0u && k.s.kayit.kuyruklanan == k.s.kayit.uretilen,
                ad + ": ileti düşmedi (üretilen " + sayi(k.s.kayit.uretilen) + ")");

        // Kademe süzgeci (Tablo 3.3) ve kanal tablosu.
        const std::set<std::string> beklenen = std::string(kademe) == "olaylar" ? kOlaylar : kTelemetri;
        const std::set<std::string> var = kume(r.sayilar);
        dogrula(var == beklenen, ad + ": dosyadaki kanallar Tablo 3.3 süzgecine uyar [" + birlestir(var) + "]");
        std::set<std::string> uretilen(k.s.kayit.uretilen_kanallar.begin(), k.s.kayit.uretilen_kanallar.end());
        dogrula(uretilen == beklenen, ad + ": üretilen kanal tablosu sabit [" + birlestir(uretilen) + "]");
        // Eksik kanal tablosu (kademe ve kipin gerektirdiği, bu bağlamada üretilmeyenler; A3 kapalı, sonar kapalı).
        std::set<std::string> eksik(k.s.kayit.eksik_kanallar.begin(), k.s.kayit.eksik_kanallar.end());
        std::set<std::string> eksik_beklenen = {"olay", "a3/komut", "a3/gecersiz_istek", "a3/komut_sonucu", "hil/oturum", "hil/bariyer",
                                                "hil/dusen", "kart/baglanti_sayaclari", "kamera/halka_sayaclari", "kamera/kare_dusme",
                                                "algilama/sayaclar", "algilama/parti", "hil/kart_acilis", "calistirma/durum", "ariza/etkin"};
        if (std::string(kademe) != "olaylar") {
            for (const char* e : {"fizik/adim_sonucu", "a1/kart_sim", "hil/zaman_esleme", "hil/tf_yazim", "hil/enjeksiyon",
                                  "yer_istasyonu/emulator", "yer_istasyonu/durum", "algilama/kayit", "sensor/imu", "sensor_temel/imu",
                                  "sensor_gercek/imu", "sensor/manyetometre", "sensor_temel/manyetometre", "sensor_gercek/manyetometre",
                                  "sensor_temel/basinc", "sensor_gercek/basinc", "sensor/gnss", "sensor_temel/gnss", "sensor_gercek/gnss",
                                  "sensor_temel/dvl", "sensor_gercek/dvl", "sensor_temel/batarya", "sensor_gercek/batarya", "sensor/sizinti",
                                  "sensor_temel/sizinti", "sensor_gercek/sizinti", "sensor/firlatici", "sensor_temel/firlatici",
                                  "sensor_gercek/firlatici"}) {
                eksik_beklenen.insert(e);
            }
        }
        if (std::string(kademe) == "tam") {
            for (const char* e : {"a1/sim_kart", "cekirdek/baslatma", "cekirdek/cagri", "sonar/ping360", "sonar/ping1d", "sonar_gercek/ping360"}) {
                eksik_beklenen.insert(e);
            }
        }
        dogrula(eksik == eksik_beklenen, ad + ": eksik kanal tablosu sabit (" + sayi(eksik.size()) + " kanal; fazla/eksik: " +
                                             [&] {
                                                 std::string d;
                                                 for (const std::string& x : eksik) {
                                                     if (eksik_beklenen.count(x) == 0u) {
                                                         d += "+" + x + " ";
                                                     }
                                                 }
                                                 for (const std::string& x : eksik_beklenen) {
                                                     if (eksik.count(x) == 0u) {
                                                         d += "-" + x + " ";
                                                     }
                                                 }
                                                 return d;
                                             }() + ")");

        // Güvence (W5-K41): üretilmeyen kanal varken dosya `fizik` ya da `girdi` iddia edemez.
        dogrula(oz.guvence == kayit::Guvence::izleme && k.s.kayit.guvence == "izleme" &&
                    oz.kapanis->at("tekrar_guvencesi") == "izleme",
                ad + ": güvence izleme (kapanış kaydı " + oz.kapanis->at("tekrar_guvencesi") + ")");
        const bool neden_var = oz.kapanis->find("guvence_nedenleri") != oz.kapanis->end() &&
                               oz.kapanis->at("guvence_nedenleri").rfind("kanal_uretilmiyor:", 0) == 0u;
        dogrula(neden_var == (std::string(kademe) != "olaylar"),
                ad + ": güvence nedeni yalnız izlemenin altındaki düzeyde yazılı (guvence_nedenleri " +
                    (oz.kapanis->find("guvence_nedenleri") != oz.kapanis->end() ? oz.kapanis->at("guvence_nedenleri") : "-") + ")");
        dogrula(!oz.kapanis->count("guvence_on_eki_son_adim"), ad + ": önek yok");

        if (std::string(kademe) == "olaylar") {
            dogrula(r.sayilar["senaryo/oge_uygulama"] == 5u, ad + ": senaryo öğeleri (" + sayi(r.sayilar["senaryo/oge_uygulama"]) + ")");
            continue;
        }
        // `fizik/durum`: her kontrol periyodu sınırı (200 Hz), poz CSV satırıyla bit düzeyinde aynı.
        const auto& fiz = r.yukler["fizik/durum"];
        dogrula(fiz.size() == n_sinir / 4u + 1u, ad + ": fizik/durum sayısı = kontrol sınırı sayısı (" + sayi(fiz.size()) + ")");
        std::uint64_t eslesen = 0, farkli = 0, adim_ters = 0, onceki = 0;
        bool ilk = true;
        for (const nlohmann::json& j : fiz) {
            const std::uint64_t b = std::stoull(j.at("adim_no").get<std::string>());
            if (!ilk && b <= onceki) {
                ++adim_ters;
            }
            ilk = false;
            onceki = b;
            if (b == 0u || b % 4u != 0u || b / 4u > k.s.satirlar.size()) {
                continue;
            }
            const sunucu::Satir& satir = k.s.satirlar[static_cast<std::size_t>(b / 4u - 1u)];
            const double z = j.at("govdeler")[0].at("p_n")[2].get<double>();
            (bitler(z) == bitler(satir.derinlik) ? eslesen : farkli) += 1u;
        }
        dogrula(farkli == 0u && eslesen == n_sinir / 4u && adim_ters == 0u,
                ad + ": fizik/durum pozu CSV satırıyla bit düzeyinde aynı (" + sayi(eslesen) + " satır, fark " + sayi(farkli) + ")");
        if (std::string(kademe) == "tam") {
            // A3 açık değil: yerleşik sonar kapalı → sonar kanalı yok (üretilmeyen listede).
            dogrula(eksik.count("sonar/ping1d") == 1u && r.sayilar.count("sonar/ping1d") == 0u, ad + ": sonar kapalıyken sonar kanalı yok");
        }
    }
    {
        // Sonar açık + tam kademe (sonar-havuz senaryosu, A7 kümesi `havuz`): sonar kanalları dosyada ve
        // teslim edilen kayıtlarla aynı sayıda (A3 kapalıyken de kayda).
        Gecici g("sonar");
        const senaryo::DiskKaynagi d(IDA_DEPO_KOKU);
        auto b = std::make_shared<senaryo::BellekKaynagi>();
        for (const char* y : {"scenarios/ida0.toml", "assets/havuz/manifest.json", "assets/havuz/ida0.glb", "assets/havuz/torpido_ida0.glb",
                              "assets/havuz/hedef_ornek.glb", "assets/havuz/havuz_ornek.glb"}) {
            b->dosyalar[y] = d.oku(y).value_or("");
        }
        std::string metin = d.oku("scenarios/sonar-havuz.toml").value_or("");
        degistir(metin, "sure_ns = 30_000_000_000", "sure_ns = 5_000_000_000");
        degistir(metin, "[kalite]\n", "[kalite]\nkayit = \"tam\"\n");
        b->dosyalar["scenarios/sonar-havuz.toml"] = metin;
        sunucu::Ayar a;
        a.senaryo_dosyasi = "scenarios/sonar-havuz.toml";
        a.senaryo_kaynagi = b;
        a.sonar_acik = true;
        a.kayit_acik = true;
        a.kayit_dizini = g.dizge();
        const Kosu k = kos_et(a);
        const fs::path dosya = tek_dosya(g.yol);
        Okunan r = dosya.empty() ? Okunan{} : oku(dosya);
        dogrula(k.s.tamam && r.o && r.o->ozet().saglam(), "sonar açık: koşu tamam, dosya sağlam (" + k.s.hata + ")");
        dogrula(r.sayilar["sonar/ping1d"] > 0u && r.sayilar["sonar/ping360"] > 0u && r.o->ozet().eksikler.empty() &&
                    r.sayilar["sonar/ping1d"] + r.sayilar["sonar/ping360"] == k.s.sonar_ping1d + k.s.sonar_ping360,
                "sonar açık: sonar kayıtları dosyada (ping1d " + sayi(r.sayilar["sonar/ping1d"]) + ", ping360 " +
                    sayi(r.sayilar["sonar/ping360"]) + "; teslim " + sayi(k.s.sonar_ping1d + k.s.sonar_ping360) + ")");
        const std::set<std::string> eksik(k.s.kayit.eksik_kanallar.begin(), k.s.kayit.eksik_kanallar.end());
        dogrula(eksik.count("sonar/ping1d") == 0u && eksik.count("sonar/ping360") == 0u && eksik.count("sonar_gercek/ping360") == 1u,
                "sonar açık: sonar/ping1d ve ping360 eksik tablosunda değil, sonar_gercek/ping360 var");
    }
    return g_hata;
}

// ------------------------------------------------------------------------------------------------ A3 açık

int test_a3_acik()
{
    const double sure_s = 5.0;
    std::uint32_t komut_no = 7700;
    const auto kosu = [&](bool kayitli, const std::string& dizin, std::atomic<int>& verildi) {
        sunucu::Ayar a = z1_ayari(sure_s, "telemetri");
        a.a3_acik = true;
        a.a3_port = 0;
        a.a3_kapanis_bekleme_s = 0.0;
        a.kayit_acik = kayitli;
        a.kayit_dizini = dizin;
        // Sınır 400'de kısıtlayan bir A3 komutu: sanal acil durdurma bas (A3-K96); sonucu `a3/komut_sonucu`.
        a.a3_sinir_kancasi = [&](std::uint64_t b, ida::a3::KomutAlicisi& al) {
            if (b != 400u || verildi.load() != 0) {
                return;
            }
            const ida::a3::AnlikDurum an = al.anlik();
            ida::a3::Komut k;
            k.komut_no = ++komut_no;
            k.istemci_id = 7;
            k.tur = ida::a3::KomutTuru::emniyet_degistir;
            k.beklenen_run_id = an.run_id;
            k.girdi = ida::a3::EmniyetGirdisi::acil_durdurma;
            k.tup = -1;
            k.hedef_durum = ida::a3::GirdiDurumu::basili;
            k.kisitlayan = true;
            verildi.store(al.al(k).tur == ida::a3::AlimSonucu::Tur::kuyrukta ? 1 : 2);
        };
        return kos_et(a);
    };
    std::atomic<int> v0{0}, v1{0};
    const Kosu ref = kosu(false, {}, v0);
    Gecici g("a3");
    const Kosu k = kosu(true, g.dizge(), v1);
    dogrula(ref.s.tamam && k.s.tamam && v0.load() == 1 && v1.load() == 1, "A3 açık: iki koşu tamam, komut kuyruğa alındı (" + k.s.hata + ")");
    dogrula(!ref.s.csv.empty() && ref.s.csv == k.s.csv && ref.giden == k.giden && ref.gelen == k.gelen,
            "A3 açık: kayıt açık ve kapalı bayt bayt aynı CSV ve A1 akışı");
    const fs::path dosya = tek_dosya(g.yol);
    Okunan r = dosya.empty() ? Okunan{} : oku(dosya, {"a3/komut_sonucu", "emniyet/girdiler", "calistirma/durum"});
    dogrula(r.o && r.o->ozet().saglam() && r.o->ozet().ust_veri_gecerli, "A3 açık: dosya sağlam");
    if (!r.o) {
        return g_hata;
    }
    dogrula(r.sayilar["olay"] > 0u && r.sayilar["calistirma/durum"] > 0u && r.sayilar["a3/komut_sonucu"] >= 1u,
            "A3 açık: olay (" + sayi(r.sayilar["olay"]) + "), çalıştırma durumu (" + sayi(r.sayilar["calistirma/durum"]) +
                ") ve komut sonucu (" + sayi(r.sayilar["a3/komut_sonucu"]) + ") dosyada");
    bool sonuc_uygulandi = false;
    for (const nlohmann::json& j : r.yukler["a3/komut_sonucu"]) {
        if (j.at("komut_no").get<std::uint32_t>() == komut_no && j.at("sonuc").get<int>() == 0 &&
            j.at("uygulama_adimi").is_string() && j.at("uygulama_adimi").get<std::string>() != "18446744073709551615") {
            sonuc_uygulandi = true;
        }
    }
    dogrula(sonuc_uygulandi, "A3 açık: komutun sonucu uygulandı, uygulama adımıyla yazılı");
    bool acil_goruldu = false;
    for (const nlohmann::json& j : r.yukler["emniyet/girdiler"]) {
        acil_goruldu = acil_goruldu || j.at("acil_durdurma").get<int>() == 1;
    }
    dogrula(acil_goruldu, "A3 açık: emniyet/girdiler değişimi (acil durdurma basıldı) dosyada");
    bool durum_kosuyor = false;
    for (const nlohmann::json& j : r.yukler["calistirma/durum"]) {
        durum_kosuyor = durum_kosuyor || j.at("durum").get<int>() == static_cast<int>(ida::a3::CalistirmaDurumu::kosuyor);
    }
    dogrula(durum_kosuyor, "A3 açık: çalıştırma durumu `kosuyor` dosyada");
    const std::set<std::string> eksik(k.s.kayit.eksik_kanallar.begin(), k.s.kayit.eksik_kanallar.end());
    dogrula(eksik.count("olay") == 0u && eksik.count("calistirma/durum") == 0u && eksik.count("a3/komut_sonucu") == 0u &&
                eksik.count("a3/komut") == 1u && eksik.count("ariza/etkin") == 0u,
            "A3 açık: eksik kanal tablosunda A3 kanalları yok, `a3/komut` var");
    dogrula(r.o->ozet().guvence == kayit::Guvence::izleme, "A3 açık: güvence izleme (a3/komut ve fizik/adim_sonucu üretilmiyor)");
    return g_hata;
}

// ------------------------------------------------------------------------------------------------ kabul 4

#ifndef _WIN32
// `ida_sim` sürecini başlatır (çıktıları atılır).
pid_t surec_baslat(const std::string& ida_sim, const std::vector<std::string>& arg)
{
    std::vector<std::string> v;
    v.push_back(ida_sim);
    v.insert(v.end(), arg.begin(), arg.end());
    const pid_t p = ::fork();
    if (p == 0) {
        const int dn = ::open("/dev/null", O_WRONLY);
        if (dn >= 0) {
            ::dup2(dn, 1);
            ::dup2(dn, 2);
        }
        std::vector<char*> c;
        for (std::string& s : v) {
            c.push_back(s.data());
        }
        c.push_back(nullptr);
        ::execv(c[0], c.data());
        ::_exit(127);
    }
    return p;
}

int test_kill9(const std::string& ida_sim)
{
    Gecici g("kill9");
    const pid_t p = surec_baslat(ida_sim, {"--senaryo", kZ1, "--kok", IDA_DEPO_KOKU, "--sil-hiz", "1", "--gunluk", "0", "--kayit-dizini", g.dizge()});
    dogrula(p > 0, "süreç başlatıldı");
    if (p <= 0) {
        return g_hata;
    }
    // Olay: dosyada en az bir kapanmış parça ve ileti görününceye kadar yokla (asılma sınırı 120 s; ölçüt değil).
    const auto son = std::chrono::steady_clock::now() + std::chrono::seconds(120);
    bool hazir = false;
    bool bitti = false;
    while (std::chrono::steady_clock::now() < son) {
        int durum = 0;
        if (::waitpid(p, &durum, WNOHANG) == p) {
            bitti = true;
            break;
        }
        const fs::path dosya = tek_dosya(g.yol);
        if (!dosya.empty()) {
            kayit::AcmaHatasi h{};
            const std::unique_ptr<kayit::Okuyucu> o = kayit::Okuyucu::ac(dosya, h);
            if (o && o->ozet().parca_sayisi >= 1u && o->ozet().okunur_ileti_sayisi > 0u) {
                hazir = true;
                break;
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(20));   // yoklama aralığı (bekleme süresi ölçüt değil)
    }
    dogrula(hazir && !bitti, "dosyada kapanmış parça görüldü ve süreç hâlâ koşuyor");
    ::kill(p, SIGKILL);
    int durum = 0;
    (void)::waitpid(p, &durum, 0);
    dogrula(WIFSIGNALED(durum) && WTERMSIG(durum) == SIGKILL, "süreç SIGKILL ile öldü");
    const fs::path dosya = tek_dosya(g.yol);
    Okunan r = dosya.empty() ? Okunan{} : oku(dosya, {"fizik/durum"});
    dogrula(r.o != nullptr, "kurtarma okuması: dosya açıldı");
    if (!r.o) {
        return g_hata;
    }
    const kayit::DosyaOzeti& oz = r.o->ozet();
    dogrula(!oz.kapanmis && !oz.bozuk && oz.parca_sayisi >= 1u && oz.okunur_ileti_sayisi > 0u && !oz.kapanis.has_value(),
            "kapanmamış, bozuk değil; kapanmış parçalar okunur (parça " + sayi(oz.parca_sayisi) + ", ileti " + sayi(oz.okunur_ileti_sayisi) + ")");
    dogrula(oz.ust_veri_var && oz.ust_veri_gecerli && oz.guvence == kayit::Guvence::izleme, "açılış üst verisi geçerli; güvence izleme");
    // Okunan fizik/durum adımları artan ve 4'ün katları (kapanmış parçalar bütün iletileri taşır).
    bool artan = !r.yukler["fizik/durum"].empty();
    std::uint64_t onceki = 0;
    bool ilk = true;
    for (const nlohmann::json& j : r.yukler["fizik/durum"]) {
        const std::uint64_t b = std::stoull(j.at("adim_no").get<std::string>());
        artan = artan && b % 4u == 0u && (ilk || b == onceki + 4u);
        ilk = false;
        onceki = b;
    }
    dogrula(artan, "kurtarılan fizik/durum adımları boşluksuz artan (" + sayi(r.yukler["fizik/durum"].size()) + " ileti)");
    return g_hata;
}
#endif

int test_kapanis(const std::string& ida_sim)
{
    // ---- olağan bitiş: kayit_icerik'te ayrıntılı; burada bitis_durumu ve son adım ----
    {
        Gecici g("olagan");
        sunucu::Ayar a = z1_ayari(4.0, "telemetri");
        a.kayit_acik = true;
        a.kayit_dizini = g.dizge();
        const Kosu k = kos_et(a);
        Okunan r = oku(tek_dosya(g.yol));
        dogrula(k.s.tamam && r.o && r.o->ozet().saglam() && r.o->ozet().kapanis->at("bitis_durumu") == "bitti" &&
                    r.o->ozet().kapanis->at("son_adim_no") == "3201" && k.s.kayit.bitis_durumu == "bitti",
                "olağan bitiş: bitis_durumu bitti, son_adim_no 3201");
    }
    // ---- hata: karşı taraf kurulamıyor (seri aygıt yok) → hata kapanışı, dosya kapalı ----
    {
        Gecici g("hata");
        const char* dosya = "scenarios/z1-hil.toml";
        sunucu::Ayar a;
        a.senaryo_dosyasi = dosya;
        a.senaryo_kaynagi = kaynak(dosya, duzen(2.0, "telemetri"));
        a.karsi = sunucu::Karsi::seri;
        a.karsi_verildi = true;
        a.seri = (g.yol / "yok_aygit").string();
        a.kayit_acik = true;
        a.kayit_dizini = g.dizge();
        const Kosu k = kos_et(a);
        Okunan r = oku(tek_dosya(g.yol));
        dogrula(!k.s.tamam && !k.s.hata.empty() && r.o && r.o->ozet().saglam() && r.o->ozet().kapanis->at("bitis_durumu") == "hata" &&
                    k.s.kayit.dosya_kapandi && k.s.kayit.bitis_durumu == "hata",
                "hata kapanışı: dosya kapalı, bitis_durumu hata (" + k.s.hata + ")");
    }
    // ---- sinyal (programdan kapanış isteği): tek kapanış yolu, bitis_durumu sim_kapanisi ----
    {
        Gecici g("sinyal");
        sunucu::Ayar a = z1_ayari(10.0, "telemetri");
        a.kayit_acik = true;
        a.kayit_dizini = g.dizge();
        a.sinir_kancasi = [](const sunucu::Ayar::SinirGozlemi& go) {
            if (go.b == 2000u) {
                sunucu::kapanis_iste();
            }
        };
        const Kosu k = kos_et(a);
        sunucu::kapanis_sifirla();
        Okunan r = oku(tek_dosya(g.yol), {"fizik/durum"});
        std::uint64_t son = 0;
        for (const nlohmann::json& j : r.yukler["fizik/durum"]) {
            son = std::max<std::uint64_t>(son, std::stoull(j.at("adim_no").get<std::string>()));
        }
        const std::uint64_t son_adim = r.o ? std::stoull(r.o->ozet().kapanis->at("son_adim_no")) : 0u;
        dogrula(k.s.sinyal_kesti && r.o && r.o->ozet().saglam() && r.o->ozet().kapanis->at("bitis_durumu") == "sim_kapanisi" &&
                    son >= 1996u && son <= son_adim && son_adim < 8000u,
                "sinyal: dosya kapandı, bitis_durumu sim_kapanisi, son_adim_no " + sayi(son_adim) + " (son fizik/durum " + sayi(son) + ")");
    }
#ifndef _WIN32
    (void)test_kill9(ida_sim);
#else
    (void)ida_sim;
#endif
    return g_hata;
}

// ------------------------------------------------------------------------------------------------ HIL sayaçları

#ifndef _WIN32
int test_hil_sayac()
{
    const char* dosya = "scenarios/z1-hil.toml";
    Gecici g("hil");
    const auto hil = [&](bool kayitli) {
        sunucu::Ayar a;
        a.senaryo_dosyasi = dosya;
        a.senaryo_kaynagi = kaynak(dosya, duzen(12.0, "telemetri"));
        a.karsi = sunucu::Karsi::pty;
        a.karsi_verildi = true;
        a.kayit_acik = kayitli;
        a.kayit_dizini = g.dizge();
        a.csv_uret = false;
        a.gunluk = 0;
        a.depo_koku = IDA_DEPO_KOKU;
        return sunucu::kos(a);
    };
    const sunucu::Sonuc ref = hil(false);
    const sunucu::Sonuc k = hil(true);
    std::printf("  HIL kayıtsız: TF %llu, kaçırılan parti %llu, sim gecikmesi %llu, son tarih %llu\n",
                static_cast<unsigned long long>(ref.zaman.tf_sayisi), static_cast<unsigned long long>(ref.sayac.parti_kacirildi),
                static_cast<unsigned long long>(ref.zaman.sim_gecikmesi), static_cast<unsigned long long>(ref.sayac.son_tarih_kacirildi));
    std::printf("  HIL kayıtlı:  TF %llu, kaçırılan parti %llu, sim gecikmesi %llu, son tarih %llu; üretici çağrısı %llu, ort. %.3f µs, "
                "en çok %.1f µs\n",
                static_cast<unsigned long long>(k.zaman.tf_sayisi), static_cast<unsigned long long>(k.sayac.parti_kacirildi),
                static_cast<unsigned long long>(k.zaman.sim_gecikmesi), static_cast<unsigned long long>(k.sayac.son_tarih_kacirildi),
                static_cast<unsigned long long>(k.kayit.cagri_adet),
                k.kayit.cagri_adet != 0u ? 1e-3 * static_cast<double>(k.kayit.cagri_toplam_ns) / static_cast<double>(k.kayit.cagri_adet) : 0.0,
                1e-3 * static_cast<double>(k.kayit.cagri_en_kotu_ns));
    dogrula(ref.tamam && k.tamam && ref.zaman.tf_sayisi >= 2300u && k.zaman.tf_sayisi >= 2300u && k.kayit.acildi && k.kayit.dosya_kapandi,
            "HIL: iki koşu da tamamlandı, kayıt açıldı ve kapandı (" + k.hata + ")");
    const std::uint64_t pay = 2;   // iki koşu arasındaki gürültü payı (tahmin)
    const auto fark_yok = [&](std::uint64_t kayitli, std::uint64_t taban) { return kayitli <= taban + pay; };
    dogrula(ida::test::zamanlama(fark_yok(k.sayac.parti_kacirildi, ref.sayac.parti_kacirildi), "kayit_hil.parti_kacirildi",
                                 "kayıtlı " + sayi(k.sayac.parti_kacirildi) + ", kayıtsız " + sayi(ref.sayac.parti_kacirildi) +
                                     " (≤ taban + " + sayi(pay) + ")"),
            "kaçırılan parti kayıtsız koşudan farklı değil (zamanlama)");
    const std::uint64_t butce_k = k.zaman.sim_gecikmesi + k.sayac.son_tarih_kacirildi;
    const std::uint64_t butce_r = ref.zaman.sim_gecikmesi + ref.sayac.son_tarih_kacirildi;
    dogrula(ida::test::zamanlama(fark_yok(butce_k, butce_r), "kayit_hil.butce_asimi",
                                 "sim gecikmesi + son tarih: kayıtlı " + sayi(butce_k) + ", kayıtsız " + sayi(butce_r) +
                                     " (≤ taban + " + sayi(pay) + ")"),
            "bütçe aşımı sayaçları kayıtsız koşudan farklı değil (zamanlama)");
    dogrula(ida::test::zamanlama(k.kayit.cagri_en_kotu_ns < 1'000'000, "kayit_hil.cagri_en_kotu",
                                 "üretici çağrısı en uzun " + std::to_string(1e-3 * static_cast<double>(k.kayit.cagri_en_kotu_ns)) +
                                     " µs (< 1000 µs; fizik adımı 1250 µs)"),
            "kayıt üretici çağrısı bir fizik adımından kısa (zamanlama)");
    return g_hata;
}
#endif

// ------------------------------------------------------------------------------------------------ kabul 5 (ölçüm)

// Ölçüm (bu makine; eşik yok, yalnız yazdırılır): 60 s'lik SIL koşusunun duvar süresi kayıtlı ve kayıtsız
// (3'er tekrar, en küçük ve ortanca), dünya iş parçacığındaki üretici çağrısı süreleri ve `telemetri`
// kaydının dosya boyutu. İşlevsel denetim: dosya sağlam.
int test_olcum()
{
    const double sure_s = 60.0;
    const auto sure_olc = [&](bool kayitli, const std::string& dizin, sunucu::Sonuc& son) {
        sunucu::Ayar a = z1_ayari(sure_s, "telemetri");
        a.kayit_acik = kayitli;
        a.kayit_dizini = dizin;
        a.gunluk = 0;
        a.depo_koku = IDA_DEPO_KOKU;
        a.csv_uret = true;
        const auto t0 = std::chrono::steady_clock::now();
        son = sunucu::kos(a);
        return std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    };
    std::vector<double> kayitsiz, kayitli;
    Gecici g("olcum");
    sunucu::Sonuc son;
    for (int i = 0; i < 3; ++i) {
        kayitsiz.push_back(sure_olc(false, {}, son));
        kayitli.push_back(sure_olc(true, (g.yol / std::to_string(i)).string(), son));
    }
    std::sort(kayitsiz.begin(), kayitsiz.end());
    std::sort(kayitli.begin(), kayitli.end());
    const kayit::DosyaOzeti* oz = nullptr;
    Okunan r = oku(tek_dosya(g.yol / "2"));
    if (r.o) {
        oz = &r.o->ozet();
    }
    std::printf("  60 s SIL koşusu, duvar (s; ölçüm, bu makine; en küçük / ortanca): kayıtsız %.3f / %.3f, kayıtlı %.3f / %.3f\n",
                kayitsiz.front(), kayitsiz[1], kayitli.front(), kayitli[1]);
    std::printf("  dünya iş parçacığında üretici çağrısı: %llu çağrı, ortalama %.3f µs, en çok %.1f µs, toplam %.3f ms; "
                "`kapanis_taslagi` en çok %.1f µs (ölçüm)\n",
                static_cast<unsigned long long>(son.kayit.cagri_adet),
                son.kayit.cagri_adet != 0u ? 1e-3 * static_cast<double>(son.kayit.cagri_toplam_ns) / static_cast<double>(son.kayit.cagri_adet) : 0.0,
                1e-3 * static_cast<double>(son.kayit.cagri_en_kotu_ns), 1e-6 * static_cast<double>(son.kayit.cagri_toplam_ns),
                1e-3 * static_cast<double>(son.kayit.taslak_en_kotu_ns));
    std::printf("  telemetri kaydı, 60 s: %llu B dosya, %llu ileti, %llu parça (ölçüm)\n",
                static_cast<unsigned long long>(son.kayit.dosya_bayt), static_cast<unsigned long long>(son.kayit.yazilan_ileti),
                static_cast<unsigned long long>(son.kayit.kapanan_parca));
    dogrula(oz != nullptr && oz->saglam() && oz->ileti_sayisi == son.kayit.yazilan_ileti && son.kayit.dosya_bayt == oz->dosya_bayt,
            "ölçüm koşusunun dosyası sağlam; ileti sayısı ve boyut özetle aynı");
    return g_hata;
}

}  // namespace

int main(int argc, char** argv)
{
    const std::string ad = argc > 1 ? argv[1] : "";
    const std::string ida_sim = argc > 2 ? argv[2] : "";
    std::printf("== %s ==\n", ad.c_str());
    int h = 0;
    if (ad == "kayit_varsayilan") {
        h = test_varsayilan();
    } else if (ad == "kayit_belirlenim") {
        h = test_belirlenim();
    } else if (ad == "kayit_icerik") {
        h = test_icerik();
    } else if (ad == "kayit_a3_acik") {
        h = test_a3_acik();
    } else if (ad == "kayit_olcum") {
        h = test_olcum();
    } else if (ad == "kayit_kapanis") {
        h = test_kapanis(ida_sim);
#ifndef _WIN32
    } else if (ad == "kayit_hil_sayac") {
        h = test_hil_sayac();
#endif
    } else {
        std::fprintf(stderr, "bilinmeyen test: %s\n", ad.c_str());
        return 2;
    }
    std::printf("%s: %d başarısız\n", ad.c_str(), h);
    return h == 0 ? 0 : 1;
}
