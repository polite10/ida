// W5-a3-senaryo — senaryo ve ön denetim uçları ile dünyanın aynı süreçte yeniden kurulması: bütünleme
// testleri (docs/gorevler/W5-a3-senaryo.md kabul 1–3, 5; W5-K47…K50). Karşı uç gerçek HTTP istemcisidir
// (tests/butunleme/a3_istemci); sim `ida::sunucu::kos` ile ayrı bir iş parçacığında koşar ve her dünyanın
// sonucu `Ayar::kosu_sonu_kancasi` ile alınır. Eşzamanlama sabit uykuyla değil olayla yapılır (dünya bitişi
// koşul değişkeniyle, durum geçişi `GET /durum` yoklamasıyla); her bekleyişin asılma sınırı vardır (ölçüt
// değil).
//
//   senaryo_uclar          uçların her biri: liste, dosya, doğrula, çözümlenmiş, ön denetim; doğrulama
//                          durumu ve komut numarasını değiştirmez; geçersiz yükleme durumu değiştirmez
//   senaryo_ad_denetimi    geçersiz adlar (`..`, mutlak yol, kalıp dışı, simgesel bağ, dizin) hepsi 400;
//                          kalıp dışı adda dosya sistemine gidilmez (sim.sunucu.senaryo_hizmeti_ad birim testi)
//   senaryo_yukle_belirlenim  W5-K47: yüklenen senaryonun koşusu komut satırından verilenle aynı CSV ve
//                          aynı A1 akışı (ad ve metin ile); aynı süreçte sıfırla → başlat ve yeniden yükle ile
//                          yapılan sonraki koşular taze süreçteki ilk koşuyla aynı
//   senaryo_sifirla        sıfırla her durumdan (koşarken, duraklatılmış, bitti, durduruldu) `yuklendi`;
//                          yeni run_id; emniyet girdileri başlangıç değerine; durum_rev yalnız artar
//   senaryo_geri_kurma     yeni senaryo bu dünyada kurulamazsa önceki senaryo geri kurulur, istek reddedilir
//   senaryo_kayit          kayıt açıkken çalıştırma başına bir dosya (A6-K01); ön denetim eki ile
//                          `GET /on_denetim` aynı belge (W5-K49)
//   senaryo_a2             A2 sunucusu süreç boyunca bir kez başlar; her yeni dünya A2 nesli artırır (W5-K49)
//   senaryo_sifirla_hil        POSIX: kartsız HIL (sözde uçbirim) dünyası sıfırlanır ve yeniden başlar
//   senaryo_surekli_kapanis  `--a3-surekli` olmadan çalıştırma bitince süreç biter (W5-K48); sinyal ile kapanış
#include "a3_istemci.hpp"
#include "ida/a3/senaryo.hpp"
#include "ida/kayit/okuyucu.hpp"
#include "ida/senaryo/senaryo.hpp"
#include "ida/sunucu/sunucu.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iterator>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
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

namespace fs = std::filesystem;
namespace sunucu = ida::sunucu;
using nlohmann::json;

int g_hata = 0;

void dogrula(bool kosul, const std::string& ileti)
{
    std::printf("%s %s\n", kosul ? "GECTI " : "KALDI ", ileti.c_str());
    if (!kosul) {
        ++g_hata;
    }
}

std::string dosya_oku(const fs::path& p)
{
    std::ifstream f(p, std::ios::binary);
    return std::string((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
}

void dosya_yaz(const fs::path& p, const std::string& s)
{
    std::ofstream f(p, std::ios::binary);
    f.write(s.data(), static_cast<std::streamsize>(s.size()));
}

// Geçici depo kökü: `<kök>/scenarios` (senaryo ve araç dosyaları) ve `<kök>/assets/ornek/manifest.json`.
struct GeciciKok {
    fs::path yol;
    explicit GeciciKok(const std::string& ad)
    {
#ifdef _WIN32
        const int pid = _getpid();
#else
        const int pid = static_cast<int>(::getpid());
#endif
        yol = fs::temp_directory_path() / ("ida_a3_senaryo_" + ad + "_" + std::to_string(pid));
        std::error_code ec;
        fs::remove_all(yol, ec);
        fs::create_directories(yol / "scenarios");
        fs::create_directories(yol / "assets" / "ornek");
        const fs::path depo = IDA_DEPO_KOKU;
        for (const char* d : {"z1.toml", "ornek-havuz.toml", "z1-hil.toml", "ida0.toml"}) {
            fs::copy_file(depo / "scenarios" / d, yol / "scenarios" / d);
        }
        fs::copy_file(depo / "assets" / "ornek" / "manifest.json", yol / "assets" / "ornek" / "manifest.json");
    }
    ~GeciciKok()
    {
        std::error_code ec;
        fs::remove_all(yol, ec);
    }
    GeciciKok(const GeciciKok&) = delete;
    GeciciKok& operator=(const GeciciKok&) = delete;
    std::string dizge() const { return yol.generic_string(); }
};

struct Fnv {
    std::uint64_t h = 1469598103934665603ull;
    std::uint64_t n = 0;
    void ekle(const std::uint8_t* p, std::size_t k)
    {
        for (std::size_t i = 0; i < k; ++i) {
            h = (h ^ p[i]) * 1099511628211ull;
        }
        n += k;
    }
    bool operator==(const Fnv& o) const { return h == o.h && n == o.n; }
};

// Bir dünyanın çıktısı.
struct Cikti {
    std::string csv;
    Fnv a1_sim, a1_karsi;
    bool tamam = false;
    std::string hata, calistirma_sonu, kayit_yol, kayit_bitis, a2_durum;
    std::int64_t yeniden_kurma_ns = 0;
    std::uint64_t dunya_sayisi = 0;
    std::size_t satir_sayisi = 0;
    std::string senaryo_yolu, davranis_ozeti;
};

// `ida::sunucu::kos`u ayrı iş parçacığında koşturur; HTTP istemcisi ve dünya çıktıları.
class Oturum {
public:
    explicit Oturum(sunucu::Ayar a) : a_(std::move(a))
    {
        a_.a3_acik = true;
        a_.a3_port = 0;
        a_.gunluk = std::getenv("IDA_TEST_GUNLUK") != nullptr ? 1 : 0;   // hata ayıklama
        a_.csv_uret = true;
        a_.a3_kapanis_bekleme_s = 0.0;
        a_.a3_hazir = [this](std::uint16_t p, const std::string& j) {
            std::lock_guard<std::mutex> kl(m_);
            port_ = p;
            jeton_ = j;
            cv_.notify_all();
        };
        a_.a1_bayt_kancasi = [this](bool sim_giden, const std::uint8_t* p, std::size_t n) {
            (sim_giden ? a1_sim_ : a1_karsi_).ekle(p, n);
        };
        a_.kosu_sonu_kancasi = [this](const sunucu::Sonuc& s) {
            Cikti c;
            c.csv = s.csv;
            c.a1_sim = a1_sim_;
            c.a1_karsi = a1_karsi_;
            a1_sim_ = Fnv{};
            a1_karsi_ = Fnv{};
            c.tamam = s.tamam;
            c.hata = s.hata;
            c.calistirma_sonu = s.calistirma_sonu;
            c.kayit_yol = s.kayit.yol;
            c.kayit_bitis = s.kayit.bitis_durumu;
            c.a2_durum = s.a2.durum;
            c.yeniden_kurma_ns = s.yeniden_kurma_ns;
            c.dunya_sayisi = s.dunya_sayisi;
            c.satir_sayisi = s.satirlar.size();
            c.senaryo_yolu = s.senaryo_yolu;
            c.davranis_ozeti = s.davranis_ozeti;
            std::lock_guard<std::mutex> kl(m_);
            dunyalar_.push_back(std::move(c));
            cv_.notify_all();
        };
    }
    ~Oturum() { kapat(); }
    Oturum(const Oturum&) = delete;
    Oturum& operator=(const Oturum&) = delete;

    // Koşuyu başlatır ve A3 sunucusu dinleyene dek bekler; istemci kaydı yapılır.
    bool baslat(int bekle_ms = 60000)
    {
        sunucu::kapanis_sifirla();
        is_ = std::thread([this] {
            sonuc_ = sunucu::kos(a_);
            bitti_ = true;
            std::lock_guard<std::mutex> kl(m_);
            cv_.notify_all();
        });
        {
            std::unique_lock<std::mutex> kl(m_);
            if (!cv_.wait_for(kl, std::chrono::milliseconds(bekle_ms), [this] { return port_ != 0 || bitti_.load(); })) {
                return false;
            }
        }
        if (port_ == 0) {
            return false;
        }
        const auto y = ida::test::a3::http_istek(port_, jeton_, "POST", "/api/v1/istemciler", "{\"ad\":\"a3_senaryo\"}");
        const auto k = y.govde.find("\"istemci_id\":");
        if (k == std::string::npos) {
            return false;
        }
        istemci_ = "Ida-Istemci: " + std::to_string(std::atoi(y.govde.c_str() + k + 13)) + "\r\n";
        return true;
    }

    // Kapanış isteği (sinyal gibi) ve bitişi bekler.
    void kapat()
    {
        if (is_.joinable()) {
            sunucu::kapanis_iste();
            is_.join();
            sunucu::kapanis_sifirla();
        }
    }
    bool bitti() const { return bitti_.load(); }
    const sunucu::Sonuc& sonuc() const { return sonuc_; }

    ida::test::a3::HttpSonuc get(const std::string& yol, int bekle_ms = 15000)
    {
        return ida::test::a3::http_istek(port_, jeton_, "GET", "/api/v1" + yol, {}, {}, bekle_ms);
    }
    // Gövde `{` ile başlar; `istek_kimligi` eklenir. Durum değiştiren uçlar `Ida-Istemci` ister.
    ida::test::a3::HttpSonuc post(const std::string& yol, const std::string& govde, int bekle_ms = 30000)
    {
        char kimlik[32];
        std::snprintf(kimlik, sizeof kimlik, "%024x", static_cast<unsigned>(++istek_));
        const std::string g = "{\"istek_kimligi\":\"" + std::string(kimlik) + "\"," + govde.substr(1);
        return ida::test::a3::http_istek(port_, jeton_, "POST", "/api/v1" + yol, g, istemci_, bekle_ms);
    }
    // `istek_kimligi` eklenmeden (okuma uçları: doğrula).
    ida::test::a3::HttpSonuc post_ham(const std::string& yol, const std::string& govde)
    {
        return ida::test::a3::http_istek(port_, jeton_, "POST", "/api/v1" + yol, govde, istemci_, 30000);
    }

    json durum()
    {
        const auto y = get("/durum");
        const json j = json::parse(y.govde, nullptr, false);
        return j.is_object() ? j : json::object();
    }
    std::string durum_adi() { return durum().value("durum", std::string("?")); }

    // `/durum` verilen koşulu sağlayana dek yoklanır.
    bool durum_bekle(const std::function<bool(const json&)>& kosul, int bekle_ms = 120000)
    {
        const auto son = std::chrono::steady_clock::now() + std::chrono::milliseconds(bekle_ms);
        while (std::chrono::steady_clock::now() < son) {
            if (kosul(durum())) {
                return true;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        return false;
    }
    bool durum_bekle(const std::string& ad, int bekle_ms = 120000)
    {
        return durum_bekle([&](const json& d) { return d.value("durum", std::string()) == ad; }, bekle_ms);
    }

    // Çalıştırma denetimi (güncel run_id ve durum_rev ile).
    ida::test::a3::HttpSonuc kontrol(const std::string& ad, const std::string& ek = {})
    {
        const json d = durum();
        const std::string run = d.contains("run_id") && d["run_id"].is_string() ? "\"" + d["run_id"].get<std::string>() + "\"" : "null";
        std::string g = "{\"beklenen_run_id\":" + run + ",\"beklenen_durum_rev\":" + std::to_string(d.value("durum_rev", 0));
        g += ek.empty() ? "}" : "," + ek + "}";
        return post("/calistirma/" + ad, g);
    }
    ida::test::a3::HttpSonuc yukle(const json& girdi)
    {
        const json d = durum();
        json g = girdi;
        g["beklenen_run_id"] = d.contains("run_id") ? d["run_id"] : json(nullptr);
        g["beklenen_durum_rev"] = d.value("durum_rev", 0);
        return post("/senaryo/yukle", g.dump());
    }

    // n. dünyanın (1'den) çıktısı gelene dek bekler.
    bool dunya_bekle(std::size_t n, int bekle_ms = 180000)
    {
        std::unique_lock<std::mutex> kl(m_);
        return cv_.wait_for(kl, std::chrono::milliseconds(bekle_ms), [&] { return dunyalar_.size() >= n; });
    }
    Cikti dunya(std::size_t i)
    {
        std::lock_guard<std::mutex> kl(m_);
        return i < dunyalar_.size() ? dunyalar_[i] : Cikti{};
    }
    std::size_t dunya_sayisi()
    {
        std::lock_guard<std::mutex> kl(m_);
        return dunyalar_.size();
    }
    std::uint16_t port() const { return port_; }
    const std::string& jeton() const { return jeton_; }

private:
    sunucu::Ayar a_;
    std::thread is_;
    std::mutex m_;
    std::condition_variable cv_;
    std::uint16_t port_ = 0;
    std::string jeton_, istemci_;
    int istek_ = 0;
    std::atomic<bool> bitti_{false};
    sunucu::Sonuc sonuc_;
    Fnv a1_sim_, a1_karsi_;
    std::vector<Cikti> dunyalar_;
};

sunucu::Ayar dosya_ayari(const GeciciKok& k, const std::string& senaryo)
{
    sunucu::Ayar a;
    a.senaryo_dosyasi = "scenarios/" + senaryo;
    a.depo_koku = k.dizge();
    a.a3_bekle = true;
    a.a3_surekli = true;
    return a;
}

// Referans: aynı senaryo komut satırından (A3 yok), süreç içi taze koşu.
Cikti referans_kosu(const GeciciKok& k, const std::string& senaryo)
{
    Cikti c;
    sunucu::Ayar a;
    a.senaryo_dosyasi = "scenarios/" + senaryo;
    a.depo_koku = k.dizge();
    a.gunluk = 0;
    a.csv_uret = true;
    a.a1_bayt_kancasi = [&c](bool sim_giden, const std::uint8_t* p, std::size_t n) { (sim_giden ? c.a1_sim : c.a1_karsi).ekle(p, n); };
    const sunucu::Sonuc s = sunucu::kos(a);
    c.csv = s.csv;
    c.tamam = s.tamam;
    c.hata = s.hata;
    c.davranis_ozeti = s.davranis_ozeti;
    return c;
}

bool ayni_cikti(const Cikti& a, const Cikti& b)
{
    return a.csv == b.csv && !a.csv.empty() && a.a1_sim == b.a1_sim && a.a1_karsi == b.a1_karsi && a.a1_sim.n > 0u;
}

std::string cikti_ozeti(const Cikti& c)
{
    return "csv " + std::to_string(c.csv.size()) + " B, a1 " + std::to_string(c.a1_sim.n) + "/" + std::to_string(c.a1_karsi.n) + " B";
}

// ------------------------------------------------------------------------------------------------ yardımcılar

std::string sha_govde(const std::string& s) { return ida::senaryo::sha256_hex(s); }

// Bir sonraki tamamlanan koşunun (boş olmayan CSV) çıktısını bekler; `gorulen` ilerler.
bool kosu_bekle(Oturum& o, std::size_t& gorulen, Cikti& c, int ms = 180000)
{
    const auto son = std::chrono::steady_clock::now() + std::chrono::milliseconds(ms);
    for (;;) {
        const auto kalan = std::chrono::duration_cast<std::chrono::milliseconds>(son - std::chrono::steady_clock::now()).count();
        if (kalan <= 0 || !o.dunya_bekle(gorulen + 1u, static_cast<int>(kalan))) {
            return false;
        }
        c = o.dunya(gorulen);
        ++gorulen;
        if (!c.csv.empty()) {
            return true;
        }
    }
}

// ------------------------------------------------------------------------------------------------ uçlar

int test_uclar()
{
    GeciciKok k("uclar");
    // Listede görünmemesi gerekenler: kalıp dışı adlar ve dizin.
    dosya_yaz(k.yol / "scenarios" / "X.TOML", "x");
    dosya_yaz(k.yol / "scenarios" / "a.txt", "x");
    dosya_yaz(k.yol / "scenarios" / "bos ad.toml", "x");
    fs::create_directories(k.yol / "scenarios" / "dizin.toml");
    Oturum o(dosya_ayari(k, "z1.toml"));
    if (!o.baslat()) {
        dogrula(false, "sim başladı ve A3 dinliyor");
        return g_hata;
    }
    // ---- GET /senaryolar ----
    const auto liste = o.get("/senaryolar");
    const json lj = json::parse(liste.govde, nullptr, false);
    std::vector<std::string> adlar;
    if (lj.is_object() && lj.contains("senaryolar")) {
        for (const json& s : lj["senaryolar"]) {
            adlar.push_back(s.value("ad", ""));
        }
    }
    const std::vector<std::string> beklenen = {"ida0.toml", "ornek-havuz.toml", "z1-hil.toml", "z1.toml"};
    dogrula(liste.durum == 200 && adlar == beklenen, "GET /senaryolar: yalnız kalıba uyan düzgün dosyalar, ada göre sıralı");
    bool alanlar = !adlar.empty();
    for (const json& s : lj.value("senaryolar", json::array())) {
        alanlar = alanlar && s.contains("boyut_bayt") && s.contains("degisim_utc") &&
                  s["boyut_bayt"].get<std::uint64_t>() == fs::file_size(k.yol / "scenarios" / s["ad"].get<std::string>()) &&
                  s["degisim_utc"].get<std::string>().size() == 20u;
    }
    dogrula(alanlar, "liste: boyut gerçek dosya boyu, değişim zamanı ISO 8601 UTC");

    // ---- GET /senaryolar/{ad} ----
    const std::string z1 = dosya_oku(k.yol / "scenarios" / "z1.toml");
    const auto d = o.get("/senaryolar/z1.toml");
    const json dj = json::parse(d.govde, nullptr, false);
    dogrula(d.durum == 200 && dj.value("ad", "") == "z1.toml" && dj.value("icerik", std::string("?")) == z1,
            "GET /senaryolar/{ad}: içerik dosyayla bayt bayt aynı");
    const auto yok = o.get("/senaryolar/yok.toml");
    dogrula(yok.durum == 404 && yok.kod() == "bulunamadi", "GET /senaryolar/{ad}: olmayan dosya 404 bulunamadi");

    // ---- POST /senaryo/dogrula: dosyayı, durumu ve komut numarasını değiştirmez (A3-K76) ----
    const json once = o.durum();
    const std::string ozet0 = once.value("senaryo_ozeti", std::string());
    const auto dg = o.post_ham("/senaryo/dogrula", json{{"ad", "z1.toml"}}.dump());
    const json dgj = json::parse(dg.govde, nullptr, false);
    dogrula(dg.durum == 200 && dgj.value("gecerli", false) && dgj.value("senaryo_ozeti", "") == ozet0 && dgj.contains("rapor") &&
                dgj["rapor"].is_object() && dgj["rapor"].value("gecerli", false),
            "POST /senaryo/dogrula {ad}: geçerli, özet yüklü senaryonunkiyle aynı, rapor A5 raporu");
    const auto di = o.post_ham("/senaryo/dogrula", json{{"icerik", z1}}.dump());
    const json dij = json::parse(di.govde, nullptr, false);
    dogrula(di.durum == 200 && dij.value("gecerli", false) && dij.value("senaryo_ozeti", "") == ozet0,
            "POST /senaryo/dogrula {icerik}: aynı metin aynı özet");
    const auto dk = o.post_ham("/senaryo/dogrula", json{{"icerik", "bu = [ toml degil"}}.dump());
    const json dkj = json::parse(dk.govde, nullptr, false);
    bool hata_bulgusu = false;
    for (const json& b : dkj.value("rapor", json::object()).value("bulgular", json::array())) {
        hata_bulgusu = hata_bulgusu || b.value("onem", "") == "hata";
    }
    dogrula(dk.durum == 200 && !dkj.value("gecerli", true) && dkj.contains("senaryo_ozeti") && dkj["senaryo_ozeti"].is_null() &&
                hata_bulgusu,
            "POST /senaryo/dogrula: geçersiz metin gecerli=false, özet null, raporda hata bulgusu");
    dogrula(o.post_ham("/senaryo/dogrula", json{{"ad", "../x.toml"}}.dump()).durum == 400, "doğrula: kalıp dışı ad 400");
    dogrula(o.post_ham("/senaryo/dogrula", json{{"ad", "z1.toml"}, {"icerik", z1}}.dump()).durum == 400, "doğrula: ad ve icerik birlikte 400");
    dogrula(o.post_ham("/senaryo/dogrula", "{}").durum == 400, "doğrula: ad da icerik de yok 400");
    dogrula(o.post_ham("/senaryo/dogrula", json{{"ad", "yok.toml"}}.dump()).durum == 404, "doğrula: olmayan dosya 404");
    const json sonra = o.durum();
    dogrula(sonra.value("durum_rev", 0) == once.value("durum_rev", -1) && sonra.value("son_komut_no", 0) == once.value("son_komut_no", -1) &&
                sonra.value("durum", "") == "yuklendi" && sonra.value("senaryo_ozeti", "") == ozet0 && o.dunya_sayisi() == 0u,
            "doğrulamalar durum, durum_rev ve komut numarasını değiştirmedi");

    // ---- GET /senaryo/cozumlenmis (A3-K80) ----
    const auto cz = o.get("/senaryo/cozumlenmis");
    dogrula(cz.durum == 200 && "sha256:" + sha_govde(cz.govde) == ozet0 && cz.baslik("Ida-Davranis-Ozeti") == ozet0 &&
                cz.baslik("Content-Type").rfind("application/json", 0) == 0,
            "GET /senaryo/cozumlenmis: baytların SHA-256'sı davranış özeti, özet başlıkta, application/json (A5-K02, K17)");

    // ---- GET /on_denetim: koşudan önce yok ----
    dogrula(o.get("/on_denetim").durum == 404 && o.get("/on_denetim/0123456789abcdef").durum == 404, "ön denetim raporu çalıştırmadan önce yok (404)");

    // ---- geçersiz yükleme durumu değiştirmez (A3-K77) ----
    const auto gec = o.yukle(json{{"icerik", "bu = [ toml degil"}});
    const json gecj = json::parse(gec.govde, nullptr, false);
    dogrula(gec.durum == 422 && gec.kod() == "senaryo_gecersiz" && gecj["hata"]["ayrinti"]["rapor"].is_object(),
            "yükle: geçersiz senaryo 422 senaryo_gecersiz, ayrıntıda A5 raporu");
    const json s2 = o.durum();
    dogrula(s2.value("durum", "") == "yuklendi" && s2.value("durum_rev", 0) == once.value("durum_rev", -1) &&
                s2.value("senaryo_ozeti", "") == ozet0 && o.dunya_sayisi() == 0u,
            "geçersiz yükleme: durum, durum_rev ve yüklü senaryo değişmedi, yeni dünya kurulmadı");
    json yanlis_rev = {{"ad", "ornek-havuz.toml"}, {"beklenen_run_id", nullptr}, {"beklenen_durum_rev", once.value("durum_rev", 0) + 7}};
    const auto yr = o.post("/senaryo/yukle", yanlis_rev.dump());
    dogrula(yr.durum == 409 && yr.kod() == "durum_rev_uyusmuyor" && o.dunya_sayisi() == 0u && o.durum().value("senaryo_ozeti", "") == ozet0,
            "yükle: bayat durum_rev 409 durum_rev_uyusmuyor, durum değişmedi");
    dogrula(o.yukle(json{{"ad", "../x.toml"}}).durum == 400, "yükle: kalıp dışı ad 400");
    dogrula(o.yukle(json{{"ad", "yok.toml"}}).durum == 404, "yükle: olmayan dosya 404");

    // ---- geçerli yükleme ----
    const auto oh = o.post_ham("/senaryo/dogrula", json{{"ad", "ornek-havuz.toml"}}.dump());
    const std::string ozet1 = json::parse(oh.govde, nullptr, false).value("senaryo_ozeti", "");
    const std::uint64_t rev0 = o.durum().value("durum_rev", 0u);
    const auto y = o.yukle(json{{"ad", "ornek-havuz.toml"}});
    const json yj = json::parse(y.govde, nullptr, false);
    dogrula(y.durum == 200 && yj.value("sonuc", "") == "kabul" && yj.value("senaryo_ozeti", "") == ozet1 && yj.contains("rapor") &&
                yj.value("komut_no", 0) >= 1,
            "yükle: 200, komut_no, senaryo_ozeti doğrulamadakiyle aynı, A5 raporu");
    const json s3 = o.durum();
    dogrula(s3.value("durum", "") == "yuklendi" && s3.value("senaryo_ozeti", "") == ozet1 && s3.value("durum_rev", 0u) == rev0 + 1u && s3["run_id"].is_null(),
            "yükle yanıtı geldiğinde durum yuklendi ve yeni senaryo, durum_rev tam bir arttı (A3-K122)");
    dogrula(o.dunya_sayisi() == 1u, "eski dünya sonlandı, yenisi kuruldu");
    const auto cz2 = o.get("/senaryo/cozumlenmis");
    dogrula(cz2.durum == 200 && "sha256:" + sha_govde(cz2.govde) == ozet1, "yüklemeden sonra çözümlenmiş senaryo yenisi");
    const auto kn = o.get("/komutlar/" + std::to_string(yj.value("komut_no", 0)));
    dogrula(kn.durum == 200 && kn.govde.find("uygulandi") != std::string::npos, "yükleme komutunun sonucu uygulandi (GET /komutlar)");
    // Yinelenen istek (A3-K53): aynı istek_kimligi ve gövde aynı yanıt + yineleme:true.
    // ---- ön denetim: koşu başlayınca rapor ----
    std::size_t gorulen = o.dunya_sayisi();
    const auto b = o.kontrol("baslat");
    dogrula(b.durum == 202, "baslat kabul (202)");
    Cikti c;
    dogrula(kosu_bekle(o, gorulen, c) && c.tamam && c.calistirma_sonu == "bitti", "yüklenen senaryo koştu ve bitti");
    const auto od = o.get("/on_denetim");
    const json odj = json::parse(od.govde, nullptr, false);
    const std::string kimlik = odj.value("rapor_kimligi", "");
    dogrula(od.durum == 200 && kimlik.size() == 16u && odj.value("sonuc", "") == "gecti" && odj.value("kip", "") == "sil", "GET /on_denetim: son rapor (16 karakterli kimlik, sonuç gecti, kip sil)");
    const auto od2 = o.get("/on_denetim/" + kimlik);
    dogrula(od2.durum == 200 && od2.govde == od.govde, "GET /on_denetim/{kimlik}: aynı rapor");
    std::map<std::string, std::string> durum_de;
    for (const json& m : odj.value("maddeler", json::array())) {
        durum_de[m.value("kod", "")] = m.value("durum", "");
    }
    bool atlandi_dogru = durum_de.count("senaryo_gecerli") == 1u && durum_de["senaryo_gecerli"] == "gecti";
    for (const char* kod : {"surum_uyumu", "kontrol_hizi", "a1_bant_payi", "a7_manifest", "ping360_zamanlama", "gorsellestirici",
                            "algilama_bileseni", "kayit_disk_alani"}) {
        atlandi_dogru = atlandi_dogru && durum_de.count(kod) == 1u && durum_de[kod] == "atlandi";   // W5-K50: yapılmayan denetim geçti yazmaz
    }
    dogrula(atlandi_dogru, "ön denetim raporu: yalnız yapılan denetim gecti (senaryo_gecerli); öteki maddeler atlandi (kayıt kapalıyken disk dahil; W5-K50)");
    dogrula(o.get("/on_denetim/zzzzzzzzzzzzzzzz").durum == 400 && o.get("/on_denetim/0123456789abcdef").durum == 404 && o.get("/on_denetim/abc").durum == 400,
            "ön denetim: kalıp dışı kimlik 400, bilinmeyen kimlik 404");
    return g_hata;
}

// ------------------------------------------------------------------------------------------------ ad denetimi

int test_ad_denetimi()
{
    GeciciKok k("ad");
    dosya_yaz(k.yol / "dis.toml", dosya_oku(k.yol / "scenarios" / "z1.toml"));   // senaryo dizininin DIŞINDA
    fs::create_directories(k.yol / "scenarios" / "dizin.toml");
    bool bag_var = false;
    {
        std::error_code ec;
        fs::create_symlink(fs::path("..") / "dis.toml", k.yol / "scenarios" / "bag.toml", ec);
        bag_var = !ec;
        if (!bag_var) {
            std::printf("  ATLANDI: sembolik bağ bu ortamda oluşturulamadı (%s); bag.toml denemeleri koşturulmadı\n", ec.message().c_str());
        }
    }
    Oturum o(dosya_ayari(k, "z1.toml"));
    if (!o.baslat()) {
        dogrula(false, "sim başladı ve A3 dinliyor");
        return g_hata;
    }
    const json once = o.durum();
    std::vector<std::string> kotu = {"..%2Fdis.toml", "%2e%2e%2Fdis.toml", "%2Fetc%2Fpasswd", "..", ".toml", "Z1.toml", "a.txt",
                                     "a%00.toml", std::string(65, 'a') + ".toml", "a/b.toml", "../dis.toml", "./z1.toml",
                                     "z1.toml/", "z1.toml%2F", "dizin.toml", "z1.TOML", "z%201.toml", "..%5Cdis.toml", "C%3A%5Cx.toml"};
    if (bag_var) {
        kotu.push_back("bag.toml");
    }
    int ret400 = 0;
    std::string kacan;
    for (const std::string& ad : kotu) {
        const auto y = o.get("/senaryolar/" + ad);
        const bool tamam = y.durum == 400 && y.kod() == "gecersiz_istek" && y.govde.find("icerik") == std::string::npos;
        if (tamam) {
            ++ret400;
        } else {
            kacan += " " + ad + "→" + std::to_string(y.durum);
        }
    }
    dogrula(ret400 == static_cast<int>(kotu.size()), "GET /senaryolar/{ad}: " + std::to_string(kotu.size()) + " geçersiz ad hepsi 400 gecersiz_istek, içerik sızmadı" + kacan);
    // Gövdedeki adlar (doğrula ve yükle): şema kalıbı ya da dosya durumu.
    std::vector<std::string> govde_kotu = {"../dis.toml", "/etc/passwd", "dis.toml/", "..", "a\\b.toml", "dizin.toml", "Z1.toml", ""};
    if (bag_var) {
        govde_kotu.push_back("bag.toml");
    }
    int dogrula400 = 0, yukle400 = 0;
    for (const std::string& ad : govde_kotu) {
        dogrula400 += o.post_ham("/senaryo/dogrula", json{{"ad", ad}}.dump()).durum == 400 ? 1 : 0;
        yukle400 += o.yukle(json{{"ad", ad}}).durum == 400 ? 1 : 0;
    }
    dogrula(dogrula400 == static_cast<int>(govde_kotu.size()), "POST /senaryo/dogrula: geçersiz ya da dizin dışına çıkan her ad 400");
    dogrula(yukle400 == static_cast<int>(govde_kotu.size()), "POST /senaryo/yukle: geçersiz ya da dizin dışına çıkan her ad 400");
    // Liste simgesel bağı ve dizini göstermez.
    const json lj = json::parse(o.get("/senaryolar").govde, nullptr, false);
    bool bag_listede = false;
    for (const json& s : lj.value("senaryolar", json::array())) {
        bag_listede = bag_listede || s.value("ad", "") == "bag.toml" || s.value("ad", "") == "dizin.toml";
    }
    dogrula(!bag_listede, "liste: simgesel bağ ve dizin listelenmez");
    const json sonra = o.durum();
    dogrula(sonra.value("durum_rev", 0) == once.value("durum_rev", -1) && sonra.value("durum", "") == "yuklendi" && o.dunya_sayisi() == 0u,
            "reddedilen isteklerden sonra durum ve durum_rev değişmedi, dünya yeniden kurulmadı");
    // Dizin dışındaki dosya yok sayılmadı: kalıba uyan ve düzgün bir dosya olarak dizine kopyalanırsa açılır.
    fs::copy_file(k.yol / "dis.toml", k.yol / "scenarios" / "ic.toml");
    dogrula(o.get("/senaryolar/ic.toml").durum == 200, "kontrol: dizin içindeki düzgün dosya açılır");
    return g_hata;
}

// ------------------------------------------------------------------------------------------------ belirlenim (W5-K47)

int test_yukle_belirlenim()
{
    GeciciKok k("belirlenim");
    const Cikti ref = referans_kosu(k, "z1.toml");
    const Cikti ref2 = referans_kosu(k, "ornek-havuz.toml");
    dogrula(ref.tamam && ref2.tamam && !ref.csv.empty() && ref.a1_sim.n > 0u && ref.davranis_ozeti != ref2.davranis_ozeti,
            "referans koşular (A3 yok, taze) tamam: " + cikti_ozeti(ref) + " ; " + cikti_ozeti(ref2));
    const std::string z1 = dosya_oku(k.yol / "scenarios" / "z1.toml");

    Oturum o(dosya_ayari(k, "z1.toml"));
    if (!o.baslat()) {
        dogrula(false, "sim başladı ve A3 dinliyor");
        return g_hata;
    }
    std::size_t gorulen = 0;
    Cikti c;
    // (1) A3 açık, komut satırı senaryosu, ilk koşu: A3'süz taze koşuyla aynı.
    dogrula(o.kontrol("baslat").durum == 202 && kosu_bekle(o, gorulen, c) && ayni_cikti(ref, c), "ilk koşu (A3 açık) = taze A3'süz koşu: " + cikti_ozeti(c));
    // (2) Aynı süreçte sıfırla → başlat: taze süreçteki ilk koşuyla aynı.
    dogrula(o.kontrol("sifirla").durum == 202 && o.durum_bekle("yuklendi"), "sıfırla: bitti → yuklendi");
    dogrula(o.kontrol("baslat").durum == 202 && kosu_bekle(o, gorulen, c) && ayni_cikti(ref, c),
            "sıfırla → başlat: aynı süreçteki ikinci koşu = taze süreçteki ilk koşu (CSV ve A1 akışı bayt bayt): " + cikti_ozeti(c));
    // (3) Başka bir senaryo yükle (bitti durumundan), koş: o senaryonun komut satırı koşusuyla aynı.
    dogrula(o.yukle(json{{"ad", "ornek-havuz.toml"}}).durum == 200, "yükle (bitti durumundan): ornek-havuz.toml");
    dogrula(o.kontrol("baslat").durum == 202 && kosu_bekle(o, gorulen, c) && ayni_cikti(ref2, c),
            "yüklenen ornek-havuz.toml koşusu = komut satırından verilenin koşusu: " + cikti_ozeti(c));
    // (4) Yüklenen senaryo ad ile, sonra metin ile (yuklendi durumundan): ilk senaryonun koşusu.
    dogrula(o.yukle(json{{"ad", "z1.toml"}}).durum == 200, "yükle (bitti durumundan): z1.toml (ad)");
    dogrula(o.kontrol("baslat").durum == 202 && kosu_bekle(o, gorulen, c) && ayni_cikti(ref, c),
            "yüklenen z1.toml (ad) koşusu = komut satırından verilen: " + cikti_ozeti(c));
    dogrula(o.kontrol("sifirla").durum == 202 && o.durum_bekle("yuklendi"), "sıfırla");
    dogrula(o.yukle(json{{"icerik", z1}}).durum == 200, "yükle (yuklendi durumundan): z1.toml (metin)");
    dogrula(o.kontrol("baslat").durum == 202 && kosu_bekle(o, gorulen, c) && ayni_cikti(ref, c),
            "yüklenen z1.toml (metin) koşusu = komut satırından verilen: " + cikti_ozeti(c));
    // (5) Koşuyu yarıda sıfırlamak sonraki koşuyu bozmaz.
    dogrula(o.kontrol("sifirla").durum == 202 && o.durum_bekle("yuklendi"), "sıfırla (bitti → yuklendi)");
    dogrula(o.kontrol("baslat").durum == 202 && o.durum_bekle("kosuyor", 60000) && o.kontrol("hiz", "\"carpan\":1").durum == 202,
            "yeni koşu başladı ve gerçek zamana kısıldı");
    dogrula(o.kontrol("sifirla").durum == 202 && o.durum_bekle("yuklendi"), "koşarken sıfırla: yuklendi");
    Cikti kismi;
    const bool kismi_var = kosu_bekle(o, gorulen, kismi);
    const std::size_t onek = kismi.csv.size() / 10u * 9u;   // son satırlar kart yanıtını beklerken kesilir: ilk %90 karşılaştırılır
    dogrula(kismi_var && kismi.csv.size() < ref.csv.size() && kismi.calistirma_sonu == "yuklendi" && kismi.csv.compare(0, onek, ref.csv, 0, onek) == 0,
            "yarıda sıfırlanan koşunun çıktısı tam koşunun öneki (" + std::to_string(kismi.csv.size()) + " / " + std::to_string(ref.csv.size()) + " B)");
    dogrula(o.kontrol("baslat").durum == 202 && kosu_bekle(o, gorulen, c) && ayni_cikti(ref, c),
            "yarıda sıfırlanan koşudan sonraki tam koşu = taze koşu: " + cikti_ozeti(c));
    // Ölçüm (bu makine; eşik yok): isteğin dünya tarafından kabulünden yeni dünyanın `yuklendi` yayınına süre
    // (eski dünya durmuş, yenisi başlamamıştır) ve `senaryo/yukle` HTTP gidiş-dönüşü (A5 doğrulaması dahil).
    for (std::size_t i = 0; i < o.dunya_sayisi(); ++i) {
        const Cikti x = o.dunya(i);
        std::printf("  ÖLÇÜM: dünya %zu: kabulden yuklendi'ye %.2f ms (sıfırla/yükle ile kurulduysa; 0: ilk dünya)\n", i + 1u,
                    1e-6 * static_cast<double>(x.yeniden_kurma_ns));
    }
    for (int i = 0; i < 3; ++i) {
        const auto t0 = std::chrono::steady_clock::now();
        const auto yy = o.yukle(json{{"ad", i % 2 == 0 ? "ornek-havuz.toml" : "z1.toml"}});
        const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
        std::printf("  ÖLÇÜM: senaryo/yukle HTTP gidiş-dönüşü (yuklendi durumundan; doğrulama + kurulum) %.1f ms (durum %d)\n", ms, yy.durum);
    }
    // Dünya sayacı ve yeniden kurma süresi bildirimi.
    const std::size_t n = o.dunya_sayisi();
    bool sayac = n >= 6u;
    std::uint64_t onceki = 0;
    for (std::size_t i = 0; i < n; ++i) {
        const Cikti x = o.dunya(i);
        sayac = sayac && x.dunya_sayisi > onceki;
        onceki = x.dunya_sayisi;
    }
    dogrula(sayac, "her dünya bir öncekinden sonra kuruldu (dünya sayısı artıyor: " + std::to_string(n) + ")");
    return g_hata;
}

// ------------------------------------------------------------------------------------------------ sıfırla

int test_sifirla()
{
    GeciciKok k("sifirla");
    Oturum o(dosya_ayari(k, "z1.toml"));
    if (!o.baslat()) {
        dogrula(false, "sim başladı ve A3 dinliyor");
        return g_hata;
    }
    json bas = o.durum();
    const json emn0 = json::parse(o.get("/emniyet").govde, nullptr, false);
    const std::uint64_t rev_ilk = bas.value("durum_rev", 0u);
    // (a) yuklendi: yalnız durum_rev artar (A3-K67), dünya yeniden kurulmaz.
    dogrula(o.kontrol("sifirla").durum == 202, "yuklendi: sıfırla kabul");
    dogrula(o.durum_bekle([&](const json& d) { return d.value("durum_rev", 0u) == rev_ilk + 1u; }) && o.dunya_sayisi() == 0u,
            "yuklendi'de sıfırla: durum_rev bir arttı, dünya yeniden kurulmadı");
    std::vector<std::string> run_idler;
    std::uint64_t onceki_rev = o.durum().value("durum_rev", 0u);
    auto rev_artti = [&]() {
        const std::uint64_t r = o.durum().value("durum_rev", 0u);
        const bool ok = r > onceki_rev;
        onceki_rev = r;
        return ok;
    };
    auto yeni_kosu_baslat = [&]() {
        const auto y = o.kontrol("baslat");
        if (y.durum != 202 || !o.durum_bekle([](const json& d) { return d.value("durum", "") == "kosuyor"; }, 60000)) {
            return false;
        }
        const json d = o.durum();
        run_idler.push_back(d["run_id"].is_string() ? d["run_id"].get<std::string>() : std::string());
        return true;
    };
    // (b) koşarken sıfırla: acil durdurma basılıyken de yeni dünya başlangıç emniyet durumuyla başlar.
    dogrula(yeni_kosu_baslat() && o.kontrol("hiz", "\"carpan\":1").durum == 202, "koşu başladı (gerçek zaman)");
    {
        const json d = o.durum();
        const auto e = o.post("/emniyet/degistir", "{\"beklenen_run_id\":\"" + d["run_id"].get<std::string>() + "\",\"girdi\":\"acil_durdurma\",\"hedef_durum\":\"basili\"}");
        const bool basili = o.durum_bekle([&](const json&) {
            const json em = json::parse(o.get("/emniyet").govde, nullptr, false);
            return em.contains("yetkili") && em["yetkili"].value("acil_durdurma", "") == "basili";
        }, 30000);
        dogrula(e.durum == 202 && basili, "koşarken acil durdurma basıldı (yetkili durum basili)");
    }
    dogrula(o.kontrol("sifirla").durum == 202 && o.durum_bekle("yuklendi") && o.durum()["run_id"].is_null() && rev_artti(),
            "koşarken sıfırla: yuklendi, run_id null (A3-K124), durum_rev arttı");
    {
        const json em = json::parse(o.get("/emniyet").govde, nullptr, false);
        dogrula(em.contains("yetkili") && em["yetkili"] == emn0["yetkili"] && em.value("emniyet_rev", 0u) == 1u,
                "yeni dünyanın emniyet girdileri senaryonun başlangıç değerinde, emniyet_rev 1 (acil durdurma önceki dünyadan devralınmadı): " + em.dump() +
                    " / başlangıç " + emn0.dump());
    }
    // (c) duraklatılmış.
    dogrula(yeni_kosu_baslat() && o.kontrol("hiz", "\"carpan\":1").durum == 202 && o.kontrol("duraklat").durum == 202 && o.durum_bekle("duraklatildi"),
            "koşu duraklatıldı");
    {
        // Önceki koşuda basılan acil durdurma yeni koşuda yok: köprünün yetkili durumu başlangıç değerinde.
        const json em = json::parse(o.get("/emniyet").govde, nullptr, false);
        dogrula(em.contains("yetkili") && em["yetkili"].value("acil_durdurma", "") == "serbest" && em["yetkili"] == emn0["yetkili"],
                "sıfırlanan dünyanın çalışan koşusunda acil durdurma serbest, öteki girdiler başlangıç değerinde: " + em.dump());
    }
    dogrula(o.kontrol("sifirla").durum == 202 && o.durum_bekle("yuklendi") && rev_artti(), "duraklatılmışken sıfırla: yuklendi");
    // (d) durduruldu.
    dogrula(yeni_kosu_baslat() && o.kontrol("hiz", "\"carpan\":1").durum == 202 && o.kontrol("durdur").durum == 202 && o.durum_bekle("durduruldu"),
            "koşu durduruldu");
    dogrula(o.kontrol("sifirla").durum == 202 && o.durum_bekle("yuklendi") && rev_artti(), "durduruldu'dan sıfırla: yuklendi");
    // (e) bitti.
    dogrula(yeni_kosu_baslat() && o.durum_bekle("bitti"), "koşu bitti");
    dogrula(o.kontrol("sifirla").durum == 202 && o.durum_bekle("yuklendi") && rev_artti(), "bitti'den sıfırla: yuklendi");
    bool farkli = run_idler.size() == 4u;
    for (std::size_t i = 0; i < run_idler.size(); ++i) {
        for (std::size_t j = i + 1u; j < run_idler.size(); ++j) {
            farkli = farkli && !run_idler[i].empty() && run_idler[i] != run_idler[j];
        }
    }
    dogrula(farkli, "her koşu yeni run_id aldı");
    // Eski çalıştırmanın run_id'siyle komut reddedilir (A3-K54, K124): yuklendi'de yalnız null kabul.
    const json d = o.durum();
    json eski = {{"beklenen_run_id", run_idler[0]}, {"beklenen_durum_rev", d.value("durum_rev", 0)}};
    const auto er = o.post("/calistirma/baslat", eski.dump());
    dogrula(er.durum == 409 && er.kod() == "beklenen_durum_uyusmuyor", "eski run_id ile baslat 409 beklenen_durum_uyusmuyor");
    // Sıfırlamadan sonra A3 sayaçları sıfır (A3-K75).
    const json sayaclar = d.value("sayaclar", json::object());
    dogrula(sayaclar.value("parti_gonderilen", 1) == 0 && sayaclar.value("parti_kacirildi", 1) == 0, "sıfırlamadan sonra sayaçlar sıfır (A3-K75)");
    return g_hata;
}

// ------------------------------------------------------------------------------------------------ geri kurma

int test_geri_kurma()
{
    GeciciKok k("geri");
    const Cikti ref = referans_kosu(k, "z1.toml");
    Oturum o(dosya_ayari(k, "z1.toml"));   // SIL, `--seri` yok
    if (!o.baslat()) {
        dogrula(false, "sim başladı ve A3 dinliyor");
        return g_hata;
    }
    const json once = o.durum();
    // z1-hil.toml A5 doğrulamasından geçer ama bu süreç SIL ve seri aygıt verilmemiş: yeni dünya kurulamaz.
    const auto y = o.yukle(json{{"ad", "z1-hil.toml"}});
    dogrula(y.durum == 422 && y.kod() == "gecersiz_deger" && y.govde.find("hil") != std::string::npos,
            "kurulamayan yeni dünya: yükle 422 gecersiz_deger, neden bildirildi (" + y.govde.substr(0, 160) + ")");
    dogrula(o.durum_bekle([&](const json& d) {
        return d.value("durum", "") == "yuklendi" && d.value("senaryo_ozeti", "") == once.value("senaryo_ozeti", "?");
    }), "önceki senaryo geri kuruldu: durum yuklendi, özet eskisi");
    const json sonra = o.durum();
    dogrula(sonra.value("durum_rev", 0u) > once.value("durum_rev", 0u), "durum_rev yalnız arttı (" + std::to_string(sonra.value("durum_rev", 0u)) + ")");
    const auto kn = o.get("/komutlar/" + std::to_string(y.komut_no()));
    dogrula(kn.durum == 200 && kn.govde.find("reddedildi") != std::string::npos, "yükleme komutunun sonucu reddedildi");
    std::size_t gorulen = o.dunya_sayisi();
    Cikti c;
    dogrula(o.kontrol("baslat").durum == 202 && kosu_bekle(o, gorulen, c) && ayni_cikti(ref, c),
            "geri kurulan dünya koşar ve taze koşuyla aynı çıktıyı verir: " + cikti_ozeti(c));
    return g_hata;
}

// ------------------------------------------------------------------------------------------------ kayıt

fs::path tek_mcap_adlari(const fs::path& dizin, std::vector<std::string>& adlar)
{
    std::error_code ec;
    for (const auto& e : fs::directory_iterator(dizin, ec)) {
        if (e.path().extension() == ".mcap") {
            adlar.push_back(e.path().stem().string());
        }
    }
    std::sort(adlar.begin(), adlar.end());
    return dizin;
}

int test_kayit()
{
    GeciciKok k("kayit");
    fs::create_directories(k.yol / "protocol");
    const fs::path depo = IDA_DEPO_KOKU;
    fs::copy_file(depo / "protocol" / "komutlar.json", k.yol / "protocol" / "komutlar.json");
    fs::copy_file(depo / "protocol" / "ida.xml", k.yol / "protocol" / "ida.xml");
    const fs::path kdizin = k.yol / "kayitlar";
    sunucu::Ayar a = dosya_ayari(k, "z1.toml");
    a.kayit_acik = true;
    a.kayit_dizini = kdizin.generic_string();
    Oturum o(a);
    if (!o.baslat()) {
        dogrula(false, "sim başladı ve A3 dinliyor");
        return g_hata;
    }
    std::size_t gorulen = 0;
    Cikti c1, c2;
    // Koşu 1: sonuna kadar.
    dogrula(o.kontrol("baslat").durum == 202 && kosu_bekle(o, gorulen, c1) && c1.tamam, "koşu 1 bitti");
    const json od1 = json::parse(o.get("/on_denetim").govde, nullptr, false);
    // Sıfırla; koşu 2: yarıda sıfırlanır.
    dogrula(o.kontrol("sifirla").durum == 202 && o.durum_bekle("yuklendi"), "sıfırla");
    dogrula(o.kontrol("baslat").durum == 202 && o.durum_bekle("kosuyor") && o.kontrol("hiz", "\"carpan\":1").durum == 202, "koşu 2 başladı (gerçek zaman)");
    const json od2 = json::parse(o.get("/on_denetim").govde, nullptr, false);
    dogrula(o.kontrol("sifirla").durum == 202 && o.durum_bekle("yuklendi"), "koşu 2 yarıda sıfırlandı");
    Cikti kismi;
    dogrula(kosu_bekle(o, gorulen, kismi) && kismi.calistirma_sonu == "yuklendi" && kismi.kayit_bitis == "durduruldu",
            "yarıda sıfırlanan koşunun kaydı durduruldu ile kapandı (A6-K45; " + kismi.kayit_bitis + ")");
    // Koşu 3: sonuna kadar.
    dogrula(o.kontrol("baslat").durum == 202 && kosu_bekle(o, gorulen, c2) && c2.tamam, "koşu 3 bitti");
    const json od3 = json::parse(o.get("/on_denetim").govde, nullptr, false);
    o.kapat();
    std::vector<std::string> adlar;
    tek_mcap_adlari(kdizin, adlar);
    std::string adlar_metni;
    for (const std::string& x : adlar) {
        adlar_metni += " " + x;
    }
    dogrula(adlar.size() == 3u && adlar[0] != adlar[1] && adlar[1] != adlar[2], "çalıştırma başına bir dosya: 3 koşu, 3 farklı run_id'li dosya (A6-K01):" + adlar_metni);
    // Her dosyanın ön denetim eki, o koşu sırasında GET /on_denetim'in verdiği belgeyle aynı (W5-K49).
    int eslesen = 0;
    int kapali = 0;
    for (const std::string& ad : adlar) {
        ida::kayit::AcmaHatasi h{};
        auto r = ida::kayit::Okuyucu::ac(kdizin / (ad + ".mcap"), h);
        if (!r) {
            continue;
        }
        std::vector<std::uint8_t> bayt;
        if (r->ek_oku("on_denetim_raporu.json", bayt)) {
            const json ek = json::parse(std::string(bayt.begin(), bayt.end()), nullptr, false);
            for (const json& od : {od1, od2, od3}) {
                if (ek == od && od.is_object() && !od.empty()) {
                    ++eslesen;
                }
            }
        }
        kapali += r->ozet().saglam() ? 1 : 0;
    }
    dogrula(eslesen == 3, "kayıt ekindeki ön denetim raporu = GET /on_denetim (3/3)");
    dogrula(kapali == 3, "üç dosya da sağlam kapandı (yarıda sıfırlanan dahil)");
    dogrula(od1.value("rapor_kimligi", "a") != od2.value("rapor_kimligi", "a") && od2.value("rapor_kimligi", "a") != od3.value("rapor_kimligi", "a"),
            "her koşunun ön denetim raporu ayrı kimlikli: " + od1.value("rapor_kimligi", "?") + " " + od2.value("rapor_kimligi", "?") + " " +
                od3.value("rapor_kimligi", "?"));
    bool disk = false;
    for (const json& m : od1.value("maddeler", json::array())) {
        disk = disk || (m.value("kod", "") == "kayit_disk_alani" && m.value("durum", "") == "gecti" && m.contains("olculen") && m.contains("sinir"));
    }
    dogrula(disk, "kayıt açıkken kayit_disk_alani gerçekten ölçüldü (gecti, ölçülen ve sınır yazılı)");
    return g_hata;
}

// ------------------------------------------------------------------------------------------------ HIL (kartsız)

int test_hil_pty()
{
#ifdef _WIN32
    std::printf("  ATLANDI: sözde uçbirim yalnız POSIX'te\n");
    return 0;
#else
    GeciciKok k("hil");
    sunucu::Ayar a = dosya_ayari(k, "z1-hil.toml");
    a.karsi = sunucu::Karsi::pty;
    a.karsi_verildi = true;
    Oturum o(a);
    if (!o.baslat()) {
        dogrula(false, "sim başladı ve A3 dinliyor");
        return g_hata;
    }
    const json once = o.durum();
    dogrula(once.value("kip", "") == "hil_1x" && once.value("durum", "") == "yuklendi", "HIL dünyası yuklendi (kartsız: sözde uçbirim ve süreç içi karşı taraf)");
    // Koşu 1: başlat → koşuyor → sıfırla.
    dogrula(o.kontrol("baslat").durum == 202 && o.durum_bekle("kosuyor", 120000), "HIL koşusu başladı (A1 el sıkışması, hazırlık oturumu)");
    const json k1 = o.durum();
    dogrula(o.kontrol("sifirla").durum == 202 && o.durum_bekle("yuklendi", 60000) && o.durum()["run_id"].is_null(),
            "HIL koşarken sıfırla: yuklendi");
    // Koşu 2: aynı dünya tekrar kurulur ve başlar (yeni oturum).
    dogrula(o.kontrol("baslat").durum == 202 && o.durum_bekle("kosuyor", 120000), "sıfırlanan HIL dünyası yeniden başladı");
    const json k2 = o.durum();
    dogrula(k1["run_id"].is_string() && k2["run_id"].is_string() && k1["run_id"] != k2["run_id"] && k1["a1_run_id"] != k2["a1_run_id"],
            "yeni run_id ve yeni A1 run_id (A1-K126)");
    dogrula(o.kontrol("durdur").durum == 202 && o.durum_bekle("durduruldu", 60000), "HIL durduruldu");
    dogrula(o.kontrol("sifirla").durum == 202 && o.durum_bekle("yuklendi", 60000), "durduruldu'dan sıfırla: yuklendi");
    o.kapat();
    return g_hata;
#endif
}

// ------------------------------------------------------------------------------------------------ A2

int test_a2()
{
    GeciciKok k("a2");
    sunucu::Ayar a = dosya_ayari(k, "z1.toml");
    a.a2_acik = true;
    a.a2_sinama_bos_port = true;
    a.a2_kilit_dosyasi = (k.yol / "a2.kilit").generic_string();
    const std::uint64_t once = sunucu::a2_baslatma_sayisi();
    Oturum o(a);
    if (!o.baslat()) {
        dogrula(false, "sim başladı ve A3 dinliyor");
        return g_hata;
    }
    std::size_t gorulen = 0;
    Cikti c;
    dogrula(o.kontrol("baslat").durum == 202 && kosu_bekle(o, gorulen, c) && c.a2_durum == "dinliyor", "koşu 1: A2 dinliyor");
    dogrula(o.kontrol("sifirla").durum == 202 && o.durum_bekle("yuklendi"), "sıfırla");
    dogrula(o.kontrol("baslat").durum == 202 && kosu_bekle(o, gorulen, c) && c.a2_durum == "dinliyor", "koşu 2 (yeni dünya): A2 yine dinliyor");
    dogrula(o.yukle(json{{"ad", "ornek-havuz.toml"}}).durum == 200 && o.kontrol("baslat").durum == 202 && kosu_bekle(o, gorulen, c) && c.a2_durum == "dinliyor",
            "koşu 3 (yüklenen senaryo): A2 yine dinliyor");
    o.kapat();
    dogrula(sunucu::a2_baslatma_sayisi() - once == 1u, "A2 sunucusu süreç boyunca bir kez başlatıldı (üç dünya; sahiplik kilidi tek)");
    dogrula(o.sonuc().a2.calistirma_tanimlandi && o.sonuc().a2.sayac.biten_oturum == 0u, "son dünyada A2 çalıştırma tanımı verildi");
    return g_hata;
}

// ------------------------------------------------------------------------------------------------ süreklilik

int test_surekli_kapanis()
{
    GeciciKok k("surekli");
    // (1) `--a3-surekli` yok: çalıştırma bitince süreç biter (W5-K48), A3 sunucusu kapanır.
    {
        sunucu::Ayar a = dosya_ayari(k, "ornek-havuz.toml");
        a.a3_surekli = false;
        Oturum o(a);
        if (!o.baslat()) {
            dogrula(false, "sim başladı ve A3 dinliyor");
            return g_hata;
        }
        dogrula(o.kontrol("baslat").durum == 202, "baslat");
        dogrula(o.dunya_bekle(1) && o.dunya(0).calistirma_sonu == "bitti", "çalıştırma bitti");
        const auto son = std::chrono::steady_clock::now() + std::chrono::seconds(60);
        while (!o.bitti() && std::chrono::steady_clock::now() < son) {
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
        dogrula(o.bitti() && o.sonuc().tamam && o.sonuc().calistirma_sonu == "bitti" && o.sonuc().dunya_sayisi == 1u,
                "süreç kendiliğinden bitti (sürekli kip yok): tek dünya");
    }
    // (2) `--a3-surekli`: çalıştırma bitince süreç sürer; sinyal ile düzgün kapanır ve sonuç son koşunun sonucudur.
    {
        Oturum o(dosya_ayari(k, "ornek-havuz.toml"));
        if (!o.baslat()) {
            dogrula(false, "sim başladı ve A3 dinliyor");
            return g_hata;
        }
        dogrula(o.kontrol("baslat").durum == 202 && o.durum_bekle("bitti") && !o.bitti(), "bitti durumunda süreç sürüyor");
        // Süreklilik beklemesinde geçersiz istekler reddedilir.
        const auto b = o.kontrol("baslat");
        dogrula(b.durum == 409 && b.kod() == "gecersiz_durum", "bitti'de baslat 409 gecersiz_durum");
        o.kapat();
        dogrula(o.bitti() && o.sonuc().tamam && !o.sonuc().sinyal_kesti && o.sonuc().calistirma_sonu == "bitti" && o.sonuc().dunya_sayisi == 1u,
                "sinyal: düzgün kapanış, son koşunun sonucu bitti");
    }
    // (3) Başlamadan sinyal.
    {
        Oturum o(dosya_ayari(k, "ornek-havuz.toml"));
        if (!o.baslat()) {
            dogrula(false, "sim başladı ve A3 dinliyor");
            return g_hata;
        }
        o.kapat();
        dogrula(o.bitti() && o.sonuc().sinyal_kesti && o.sonuc().calistirma_sonu == "yuklendi", "baslat beklenirken sinyal: yuklendi'de kapandı");
    }
    return g_hata;
}

}  // namespace

int main(int argc, char** argv)
{
    const std::string ad = argc > 1 ? argv[1] : "";
    std::printf("== %s ==\n", ad.c_str());
    int h = 0;
    if (ad == "senaryo_uclar") {
        h = test_uclar();
    } else if (ad == "senaryo_ad_denetimi") {
        h = test_ad_denetimi();
    } else if (ad == "senaryo_yukle_belirlenim") {
        h = test_yukle_belirlenim();
    } else if (ad == "senaryo_sifirla") {
        h = test_sifirla();
    } else if (ad == "senaryo_geri_kurma") {
        h = test_geri_kurma();
    } else if (ad == "senaryo_kayit") {
        h = test_kayit();
    } else if (ad == "senaryo_a2") {
        h = test_a2();
    } else if (ad == "senaryo_sifirla_hil") {
        h = test_hil_pty();
    } else if (ad == "senaryo_surekli_kapanis") {
        h = test_surekli_kapanis();
    } else {
        std::printf("bilinmeyen test: %s\n", ad.c_str());
        return 2;
    }
    std::printf("%s %s: %d kalan\n", ad.c_str(), h == 0 ? "GECTI" : "KALDI", h);
    return h == 0 ? 0 : 1;
}
