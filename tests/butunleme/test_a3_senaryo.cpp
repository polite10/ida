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
// PR #57 düzeltme turu (docs/gorevler/inceleme/W5-a3-senaryo-inceleme.md; W5-K55…K59; EMNİYET):
//   senaryo_emniyet_rev    W5-K55: `emniyet_rev` sıfırlamada geri dönmez, oturum sonu artışından sonraki değerle
//                          sürer; eski koşunun onay jetonu ve eski run_id'li gevşetme reddedilir (A3-K57, K94)
//   senaryo_emniyet_rev_doyma  W5-K55: doyma (0xFFFFFFFF) sıfırlamayla temizlenmez; gevşetme reddedilmeyi sürdürür
//   senaryo_sinir_yarisi   W5-K56: `sifirla` ile aynı sınırda çekilen emniyet istekleri sonuçsuz kalmaz
//   senaryo_canli_ayristirma  W5-K57: yükleme durum/revizyon denetimini ayrıştırmadan önce yapar; canlı
//                          çalıştırmada doğrula ve yükle ayrıştırma yapmadan 409 gecersiz_durum
//   senaryo_emniyet_karesi W5-K58, K59: yeni dünya `yuklendi`'de emniyet karesini yayınlar; geçiş penceresinde
//                          özet null ve `baslat` reddedilir; sıfırlamada A1 oturumu RUN_END ile kapanır
// PR #57 üçüncü tur (dar denetim; W5-K64…K67; EMNİYET):
//   senaryo_cekis_yarisi   W5-K64: alıcının çekişi sırasında kabul edilen "acil durdurma (n), sifirla (n+1)" çiftinde
//                          acil durdurma eski dünyada uygulanır, sonucu sifirla'nın sonucundan önce kayda geçer
//   senaryo_sinir_yarisi   (C) W5-K65: hazırlıkta da sifirla'dan önce kabul edilen kısıtlayan eski dünyada uygulanır
#include "a3_istemci.hpp"
#include "ida/a3/komut.hpp"
#include "ida/a3/senaryo.hpp"
#include "ida/kayit/okuyucu.hpp"
#include "ida/senaryo/senaryo.hpp"
#include "ida/sunucu/sunucu.hpp"
#include "ida_link/tel.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <array>
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

// ASan (ve TSan) derlemesi mi? (yaklaşık 4–5 kat yavaş: aşağıdaki W5-K67 kısaltması)
bool sanitizer_derlemesi()
{
#if defined(__SANITIZE_ADDRESS__) || defined(__SANITIZE_THREAD__)
    return true;
#elif defined(__has_feature)
#if __has_feature(address_sanitizer) || __has_feature(thread_sanitizer)
    return true;
#else
    return false;
#endif
#else
    return false;
#endif
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
    std::vector<sunucu::OlaySonucu> olaylar;              // A3 komutlarının nihai sonuçları dahil (kaynak "a3")
    std::vector<sunucu::EmniyetDegisimi> emniyet;         // yetkili emniyet durumunun değişimleri
    std::uint64_t son_emniyet_rev = 0;                    // dünyanın köprüsünün son revizyonu (oturum sonu dahil)
    std::vector<std::uint8_t> kapanis;                    // sim'in bu dünyada gönderdiği IDA_SESSION_CLOSE nedenleri
};

// `ida::sunucu::kos`u ayrı iş parçacığında koşturur; HTTP istemcisi ve dünya çıktıları.
class Oturum {
public:
    explicit Oturum(sunucu::Ayar a) : a_(std::move(a))
    {
        a_.a3_acik = true;
        a_.a3_port = 0;
        a_.gunluk = 0;
        a_.csv_uret = true;
        a_.a3_kapanis_bekleme_s = 0.0;
        a_.a3_hazir = [this](std::uint16_t p, const std::string& j) {
            std::lock_guard<std::mutex> kl(m_);
            port_ = p;
            jeton_ = j;
            cv_.notify_all();
        };
        (void)ida_link_alici_kur(&a1_cozucu_, IDA_LINK_ROL_KART);
        a_.a1_bayt_kancasi = [this](bool sim_giden, const std::uint8_t* p, std::size_t n) {
            (sim_giden ? a1_sim_ : a1_karsi_).ekle(p, n);
            if (sim_giden) {
                kapanis_ayikla(p, n);
            }
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
            c.olaylar = s.olay_sonuclari;
            c.emniyet = s.emniyet_degisimleri;
            c.son_emniyet_rev = s.sayac.emniyet_revizyonu;
            c.kapanis = std::move(kapanis_);
            kapanis_.clear();
            (void)ida_link_alici_kur(&a1_cozucu_, IDA_LINK_ROL_KART);   // sıradaki dünyanın akışı baştan
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
    // `yuklendi`, yeni dünyanın kuruluşu bitince sayılır: geçiş penceresinde durum `yuklendi` ama senaryo özeti
    // null'dır ve `baslat` reddedilir (W5-K58).
    bool durum_bekle(const std::string& ad, int bekle_ms = 120000)
    {
        return durum_bekle([&](const json& d) {
            return d.value("durum", std::string()) == ad &&
                   (ad != "yuklendi" || (d.contains("senaryo_ozeti") && d["senaryo_ozeti"].is_string()));
        }, bekle_ms);
    }

    // Çalıştırma denetimi (güncel run_id ve durum_rev ile). W5-K66: 202 dışı yanıtın HTTP kodu ve ret kodu günlüğe
    // yazılır (CI'da bir `dogrula` düştüğünde nedeni görünsün).
    ida::test::a3::HttpSonuc kontrol(const std::string& ad, const std::string& ek = {})
    {
        const json d = durum();
        const std::string run = d.contains("run_id") && d["run_id"].is_string() ? "\"" + d["run_id"].get<std::string>() + "\"" : "null";
        son_beklenen_rev_ = d.value("durum_rev", 0u);
        std::string g = "{\"beklenen_run_id\":" + run + ",\"beklenen_durum_rev\":" + std::to_string(d.value("durum_rev", 0));
        g += ek.empty() ? "}" : "," + ek + "}";
        const auto y = post("/calistirma/" + ad, g);
        if (y.durum != 202) {
            std::printf("  KONTROL %s: HTTP %d %s\n", ad.c_str(), y.durum, y.kod().c_str());
        }
        return y;
    }
    // W5-K66: `hiz` (gerçek zaman, çarpan 1) kuyruğa girer ve dünya onu uygularken `durum_rev` artar (A3-K122). Uygulanmadan
    // (revizyon artmadan) gönderilen sonraki denetim komutu durum_rev_uyusmuyor ile reddedilirdi: `hiz`ın gönderildiği
    // görüntüden sonra revizyonun arttığı görülene dek beklenir.
    bool hiz_gercek_zaman(int bekle_ms = 60000)
    {
        const auto y = kontrol("hiz", "\"carpan\":1");
        const std::uint64_t once = son_beklenen_rev_;
        return y.durum == 202 && durum_bekle([&](const json& d) { return d.value("durum_rev", 0u) > once; }, bekle_ms);
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
    // Sim → karşı taraf baytlarındaki IDA_SESSION_CLOSE (46004) çerçevelerinin nedeni (yük: sess u16, reason u8).
    void kapanis_ayikla(const std::uint8_t* p, std::size_t n)
    {
        std::size_t k = 0;
        while (k < n) {
            std::size_t tuk = 0;
            ida_link_cerceve_t f{};
            const ida_link_rc_t rc = ida_link_al(&a1_cozucu_, p + k, n - k, &tuk, &f);
            k += tuk;
            if (rc == IDA_LINK_CERCEVE && f.msgid == 46004u) {
                kapanis_.push_back(f.yuk[2]);
            }
            if (tuk == 0u) {
                break;
            }
        }
    }

    sunucu::Ayar a_;
    std::thread is_;
    std::mutex m_;
    std::condition_variable cv_;
    std::uint16_t port_ = 0;
    std::string jeton_, istemci_;
    std::atomic<int> istek_{0};   // dünya kancasından da istek gönderilir (geçiş penceresi sınaması)
    std::uint64_t son_beklenen_rev_ = 0;   // son `kontrol`ün gönderdiği beklenen_durum_rev (yalnız sınama iş parçacığı)
    std::atomic<bool> bitti_{false};
    sunucu::Sonuc sonuc_;
    Fnv a1_sim_, a1_karsi_;
    ida_link_alici_t a1_cozucu_{};
    std::vector<std::uint8_t> kapanis_;
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

// Referans: aynı senaryo komut satırından (A3 yok), süreç içi taze koşu. `ilk_emniyet_rev` (yalnız sınama; W5-K55):
// sürecin başlangıç emniyet revizyonu; varsayılan 1 (üretim).
Cikti referans_kosu(const GeciciKok& k, const std::string& senaryo, std::uint32_t ilk_emniyet_rev = 1u)
{
    Cikti c;
    sunucu::Ayar a;
    a.senaryo_dosyasi = "scenarios/" + senaryo;
    a.depo_koku = k.dizge();
    a.sinama_ilk_emniyet_rev = ilk_emniyet_rev;
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

// `GET /durum` yanıtındaki senaryo özeti; null ise (W5-K58: geçiş penceresi) boş.
std::string ozet_al(const json& d)
{
    return d.contains("senaryo_ozeti") && d["senaryo_ozeti"].is_string() ? d["senaryo_ozeti"].get<std::string>() : std::string();
}

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
                sonra.value("durum", "") == "yuklendi" && ozet_al(sonra) == ozet0 && o.dunya_sayisi() == 0u,
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
                ozet_al(s2) == ozet0 && o.dunya_sayisi() == 0u,
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
    dogrula(s3.value("durum", "") == "yuklendi" && ozet_al(s3) == ozet1 && s3.value("durum_rev", 0u) == rev0 + 1u && s3["run_id"].is_null(),
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

// W5-K67: ASan derlemesinde belirlenim testinin koşuları kısaltılır (ASan'sız derlemelerde değişmez). Aynı denetim,
// daha kısa koşu: karşılaştırmalar (CSV, A1 akışı, ad/metin/sıfırla/yarıda sıfırlama) ve olay çizelgesinin türleri
// aynı kalır; yalnız süre ve olayların anları küçülür (z1: 90 s → 20 s; arıza olayları 43,2 s → 10 s, disarm
// 83,2 s → 18 s; ornek-havuz: 60 s → 20 s). Bu testin ~14 koşusu CI'da Windows ASan'da 365 s, macOS ASan'da 225 s sürdü
// (2026-11-25 CI ölçümü; debug: 80 s ve 24 s) ve Windows ASan işinin 60 dakikalık sınırı aşmasına katkı verdi.
// Dönüş: her değişiklik yapıldı mı (senaryo dosyaları değişirse sessizce eski süreyle koşmasın).
bool asan_icin_kisalt(const GeciciKok& k)
{
    auto degistir = [](std::string& metin, const std::string& eski, const std::string& yeni, int beklenen) {
        int n = 0;
        for (std::size_t i = metin.find(eski); i != std::string::npos; i = metin.find(eski, i + yeni.size())) {
            metin.replace(i, eski.size(), yeni);
            ++n;
        }
        return n == beklenen;
    };
    std::string z1 = dosya_oku(k.yol / "scenarios" / "z1.toml");
    bool tamam = degistir(z1, "sure_ns = 90_000_000_000", "sure_ns = 20_000_000_000", 1);
    tamam = degistir(z1, "zaman_ns = 43_200_000_000", "zaman_ns = 10_000_000_000", 2) && tamam;
    tamam = degistir(z1, "zaman_ns = 83_200_000_000", "zaman_ns = 18_000_000_000", 1) && tamam;
    std::string havuz = dosya_oku(k.yol / "scenarios" / "ornek-havuz.toml");
    tamam = degistir(havuz, "sure_ns = 60_000_000_000", "sure_ns = 20_000_000_000", 1) && tamam;
    if (tamam) {
        dosya_yaz(k.yol / "scenarios" / "z1.toml", z1);
        dosya_yaz(k.yol / "scenarios" / "ornek-havuz.toml", havuz);
    }
    return tamam;
}

int test_yukle_belirlenim()
{
    GeciciKok k("belirlenim");
    if (sanitizer_derlemesi()) {
        dogrula(asan_icin_kisalt(k), "ASan derlemesi: z1 ve ornek-havuz koşuları kısaltıldı (aynı denetim, daha kısa koşu; W5-K67)");
    }
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
    // W5-K55 (A3-K117): `emniyet_rev` süreç boyunca yaşar ve A1'de `IDA_SAFETY.safety_rev` olarak tele çıkar (aynı
    // sayaç). Bu yüzden bir oturum kapatmış süreçteki sonraki dünyaların A1 akışı, revizyonu 1'den başlayan taze
    // sürecinkiyle aynı OLAMAZ. Belirlenim iki katı karşılaştırmayla sınanır (eşik yok, ikisi de bayt bayt):
    //   CSV       taze komut satırı koşusunun (revizyon 1) CSV'siyle aynı;
    //   A1 akışı  aynı senaryonun, dünyanın başlangıç revizyonuyla başlayan taze A3'süz koşusununkiyle aynı
    //             (yalnız sınama: Ayar::sinama_ilk_emniyet_rev). Revizyon 1 ise bu, eski karşılaştırmanın kendisidir.
    std::uint32_t ilk_rev = 0;   // sıradaki koşunun başlangıç revizyonu (`yuklendi`'de okunur)
    auto rev_oku = [&] { ilk_rev = o.durum().value("emniyet_rev", 0u); };
    auto ayni_kosu = [&](const std::string& senaryo, const Cikti& taze, const Cikti& kosu) {
        if (ilk_rev == 1u) {
            return ayni_cikti(taze, kosu);
        }
        const Cikti r = referans_kosu(k, senaryo, ilk_rev);
        return ilk_rev != 0u && r.tamam && r.csv == taze.csv && kosu.csv == taze.csv && ayni_cikti(r, kosu);
    };
    // (1) A3 açık, komut satırı senaryosu, ilk koşu: A3'süz taze koşuyla aynı.
    rev_oku();
    dogrula(ilk_rev == 1u && o.kontrol("baslat").durum == 202 && kosu_bekle(o, gorulen, c) && ayni_cikti(ref, c),
            "ilk koşu (A3 açık) = taze A3'süz koşu: " + cikti_ozeti(c));
    // (2) Aynı süreçte sıfırla → başlat: taze süreçteki koşuyla aynı (revizyon devralınır: yukarıdaki açıklama).
    dogrula(o.kontrol("sifirla").durum == 202 && o.durum_bekle("yuklendi"), "sıfırla: bitti → yuklendi");
    rev_oku();
    dogrula(ilk_rev > 1u, "ikinci dünya ilk koşunun oturum sonu artışından sonraki revizyonla başlıyor (" + std::to_string(ilk_rev) + ")");
    dogrula(o.kontrol("baslat").durum == 202 && kosu_bekle(o, gorulen, c) && ayni_kosu("z1.toml", ref, c),
            "sıfırla → başlat: aynı süreçteki ikinci koşu = taze süreçteki koşu (CSV taze koşuyla, A1 akışı aynı başlangıç revizyonlu taze koşuyla bayt bayt): " +
                cikti_ozeti(c));
    // (3) Başka bir senaryo yükle (bitti durumundan), koş: o senaryonun komut satırı koşusuyla aynı.
    dogrula(o.yukle(json{{"ad", "ornek-havuz.toml"}}).durum == 200, "yükle (bitti durumundan): ornek-havuz.toml");
    rev_oku();
    dogrula(o.kontrol("baslat").durum == 202 && kosu_bekle(o, gorulen, c) && ayni_kosu("ornek-havuz.toml", ref2, c),
            "yüklenen ornek-havuz.toml koşusu = komut satırından verilenin koşusu: " + cikti_ozeti(c));
    // (4) Yüklenen senaryo ad ile, sonra metin ile (yuklendi durumundan): ilk senaryonun koşusu.
    dogrula(o.yukle(json{{"ad", "z1.toml"}}).durum == 200, "yükle (bitti durumundan): z1.toml (ad)");
    rev_oku();
    dogrula(o.kontrol("baslat").durum == 202 && kosu_bekle(o, gorulen, c) && ayni_kosu("z1.toml", ref, c),
            "yüklenen z1.toml (ad) koşusu = komut satırından verilen: " + cikti_ozeti(c));
    dogrula(o.kontrol("sifirla").durum == 202 && o.durum_bekle("yuklendi"), "sıfırla");
    dogrula(o.yukle(json{{"icerik", z1}}).durum == 200, "yükle (yuklendi durumundan): z1.toml (metin)");
    rev_oku();
    dogrula(o.kontrol("baslat").durum == 202 && kosu_bekle(o, gorulen, c) && ayni_kosu("z1.toml", ref, c),
            "yüklenen z1.toml (metin) koşusu = komut satırından verilen: " + cikti_ozeti(c));
    // (5) Koşuyu yarıda sıfırlamak sonraki koşuyu bozmaz.
    dogrula(o.kontrol("sifirla").durum == 202 && o.durum_bekle("yuklendi"), "sıfırla (bitti → yuklendi)");
    dogrula(o.kontrol("baslat").durum == 202 && o.durum_bekle("kosuyor", 60000) && o.hiz_gercek_zaman(),
            "yeni koşu başladı ve gerçek zamana kısıldı (hiz uygulandı: durum_rev arttı)");
    dogrula(o.kontrol("sifirla").durum == 202 && o.durum_bekle("yuklendi"), "koşarken sıfırla: yuklendi");
    Cikti kismi;
    const bool kismi_var = kosu_bekle(o, gorulen, kismi);
    const std::size_t onek = kismi.csv.size() / 10u * 9u;   // son satırlar kart yanıtını beklerken kesilir: ilk %90 karşılaştırılır
    dogrula(kismi_var && kismi.csv.size() < ref.csv.size() && kismi.calistirma_sonu == "yuklendi" && kismi.csv.compare(0, onek, ref.csv, 0, onek) == 0,
            "yarıda sıfırlanan koşunun çıktısı tam koşunun öneki (" + std::to_string(kismi.csv.size()) + " / " + std::to_string(ref.csv.size()) + " B)");
    rev_oku();
    dogrula(o.kontrol("baslat").durum == 202 && kosu_bekle(o, gorulen, c) && ayni_kosu("z1.toml", ref, c),
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
    dogrula(yeni_kosu_baslat() && o.hiz_gercek_zaman(), "koşu başladı (gerçek zaman)");
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
        // W5-K55: girdiler başlangıç değerine döner, `emniyet_rev` dönmez (A3-K117: süreç boyunca yalnız artar);
        // kısıtlayan (+1) ve oturum sonu (+1) artışlarından sonraki değerle sürer. Ayrıntılı sınama: senaryo_emniyet_rev.
        dogrula(em.contains("yetkili") && em["yetkili"] == emn0["yetkili"] && em.value("emniyet_rev", 0u) >= emn0.value("emniyet_rev", 0u) + 2u &&
                    !em.value("emniyet_rev_doydu", true),
                "yeni dünyanın emniyet girdileri senaryonun başlangıç değerinde (acil durdurma önceki dünyadan devralınmadı); emniyet_rev geri dönmedi: " +
                    em.dump() + " / başlangıç " + emn0.dump());
    }
    // (c) duraklatılmış.
    dogrula(yeni_kosu_baslat() && o.hiz_gercek_zaman() && o.kontrol("duraklat").durum == 202 && o.durum_bekle("duraklatildi"),
            "koşu duraklatıldı");
    {
        // Önceki koşuda basılan acil durdurma yeni koşuda yok: köprünün yetkili durumu başlangıç değerinde.
        const json em = json::parse(o.get("/emniyet").govde, nullptr, false);
        dogrula(em.contains("yetkili") && em["yetkili"].value("acil_durdurma", "") == "serbest" && em["yetkili"] == emn0["yetkili"],
                "sıfırlanan dünyanın çalışan koşusunda acil durdurma serbest, öteki girdiler başlangıç değerinde: " + em.dump());
    }
    dogrula(o.kontrol("sifirla").durum == 202 && o.durum_bekle("yuklendi") && rev_artti(), "duraklatılmışken sıfırla: yuklendi");
    // (d) durduruldu.
    dogrula(yeni_kosu_baslat() && o.hiz_gercek_zaman() && o.kontrol("durdur").durum == 202 && o.durum_bekle("durduruldu"),
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
        return d.value("durum", "") == "yuklendi" && !ozet_al(d).empty() && ozet_al(d) == ozet_al(once);
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
    // `hiz` kuyruğa girer ve dünya onu uygularken `durum_rev` artar (A3-K122); uygulanmadan alınan bir görüntüyle
    // gönderilen `sifirla` durum_rev_uyusmuyor ile reddedilir. Yavaş koşuda (ASan) bu yarış görünür: koşunun
    // bitmesini beklemek yerine `hiz`ın uygulandığı olaya bağlanır.
    dogrula(o.kontrol("baslat").durum == 202 && o.durum_bekle("kosuyor"), "koşu 2 başladı");
    dogrula(o.hiz_gercek_zaman(), "koşu 2 gerçek zamana alındı (hiz uygulandı: durum_rev arttı)");
    const json od2 = json::parse(o.get("/on_denetim").govde, nullptr, false);
    // Koşu 90 sn gerçek zamanlıdır; sıfırlama isteği olay sırasıyla (kosuyor + hiz uygulandı) gider: tek deneme
    // (W5-K66; bayat görüntü yarışı yeniden denemeyle örtülmez), ret olursa HTTP kodu ve ret kodu iletidedir.
    const auto sifirla_y = o.kontrol("sifirla");
    dogrula(sifirla_y.durum == 202 && o.durum_bekle("yuklendi"),
            "koşu 2 yarıda sıfırlandı (HTTP " + std::to_string(sifirla_y.durum) + " " + sifirla_y.kod() + ")");
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

// ------------------------------------------------------------------------------------------------ PR #57 düzeltme turu
//
// Emniyet yardımcıları (gerçek HTTP yolu). Kısıtlayan yön: acil durdurma basılı, fiş çıkık, pim takılı.

json emniyet_al(Oturum& o)
{
    const json e = json::parse(o.get("/emniyet").govde, nullptr, false);
    return e.is_object() ? e : json::object();
}

std::uint32_t emniyet_rev_al(Oturum& o) { return emniyet_al(o).value("emniyet_rev", 0u); }

std::string acil_al(const json& e)
{
    return e.contains("yetkili") && e["yetkili"].is_object() ? e["yetkili"].value("acil_durdurma", std::string()) : std::string();
}

bool acil_bekle(Oturum& o, const std::string& deger, int ms = 30000)
{
    return o.durum_bekle([&](const json&) { return acil_al(emniyet_al(o)) == deger; }, ms);
}

std::string run_id_al(const json& d) { return d.contains("run_id") && d["run_id"].is_string() ? d["run_id"].get<std::string>() : std::string(); }

ida::test::a3::HttpSonuc acil_bas(Oturum& o, const std::string& run)
{
    return o.post("/emniyet/degistir", json{{"beklenen_run_id", run}, {"girdi", "acil_durdurma"}, {"hedef_durum", "basili"}}.dump());
}

// Gevşetme (acil durdurma → serbest) için onay jetonu (A3-K93).
struct JetonYaniti {
    int http = 0;
    std::string kod, jeton;
    std::uint32_t rev = 0;
};

JetonYaniti jeton_al(Oturum& o, const std::string& run)
{
    const auto y = o.post_ham("/emniyet/onay_jetonu", json{{"beklenen_run_id", run}, {"girdi", "acil_durdurma"}, {"hedef_durum", "serbest"}}.dump());
    JetonYaniti j;
    j.http = y.durum;
    j.kod = y.kod();
    const json g = json::parse(y.govde, nullptr, false);
    if (g.is_object() && g.contains("onay_jetonu") && g["onay_jetonu"].is_string()) {
        j.jeton = g["onay_jetonu"].get<std::string>();
        j.rev = g.value("emniyet_rev", 0u);
    }
    return j;
}

ida::test::a3::HttpSonuc acil_birak(Oturum& o, const std::string& run, const std::string& jeton, std::uint32_t rev)
{
    return o.post("/emniyet/degistir", json{{"beklenen_run_id", run}, {"girdi", "acil_durdurma"}, {"hedef_durum", "serbest"}, {"onay", true},
                                            {"onay_jetonu", jeton}, {"beklenen_emniyet_rev", rev}}.dump());
}

// Revizyon, sınamanın isteklerinden bağımsız olarak da artabilir: acil durdurma basılınca kart SAFETY_CLOSE olayı
// gönderir ve uygulanması revizyonu bir artırır (A3-K117 (3)); bu, gerçek zamanlı koşuda jetonun alınması ile
// isteğin işlenmesi arasına düşebilir. O durumda gevşetme reddedilir ve sim yeniden denemez (A3-K95 (4)); istemci
// yeni jetonla yeniden dener. Aşağıdaki iki yardımcı bunu yapar (en çok 10 deneme; bekleyiş olaya göre).

// Güncel jeton ve run_id ile gevşetme (acil durdurma → serbest). true: yetkili durum serbest oldu.
bool acil_serbest_birak(Oturum& o, const std::string& run, std::string& iz)
{
    for (int i = 0; i < 10; ++i) {
        const JetonYaniti j = jeton_al(o, run);
        if (j.http != 200) {
            iz += " jeton " + std::to_string(j.http) + " " + j.kod;
            return false;
        }
        const auto e = acil_birak(o, run, j.jeton, j.rev);
        iz += " [rev " + std::to_string(j.rev) + ": " + std::to_string(e.durum) + " " + e.kod() + "]";
        if (e.durum == 202) {
            // Nihai sonuç: ya serbest oldu ya da revizyon araya giren bir artışla değişti (istek reddedildi).
            bool serbest = false;
            const bool sonuclandi = o.durum_bekle([&](const json&) {
                const json em = emniyet_al(o);
                serbest = acil_al(em) == "serbest";
                return serbest || em.value("emniyet_rev", 0u) != j.rev;
            }, 30000);
            if (serbest || !sonuclandi) {
                return serbest;
            }
        } else if (e.kod() != "onay_jetonu_gecersiz" && e.kod() != "emniyet_rev_uyusmuyor") {
            return false;
        }
    }
    return false;
}

// Güncel (geçerli) jetonla ama `istek_run` run_id'siyle gevşetme isteğinin ret kodu. Jeton, araya giren bir
// revizyon artışıyla geçersizleştiyse yeni jetonla yeniden denenir.
std::string yanlis_run_gevsetme(Oturum& o, const std::string& guncel_run, const std::string& istek_run)
{
    std::string kod;
    for (int i = 0; i < 10; ++i) {
        const JetonYaniti j = jeton_al(o, guncel_run);
        if (j.http != 200) {
            return "jeton " + std::to_string(j.http) + " " + j.kod;
        }
        const auto e = acil_birak(o, istek_run, j.jeton, j.rev);
        kod = std::to_string(e.durum) + " " + e.kod();
        if (e.kod() != "onay_jetonu_gecersiz" && e.kod() != "emniyet_rev_uyusmuyor") {
            break;
        }
    }
    return kod;
}

// Yeni koşu: `baslat` → `kosuyor` → gerçek zaman (koşu sınama boyunca canlı kalsın). Dönüş: run_id (boş: başlamadı).
std::string kosu_baslat(Oturum& o)
{
    if (o.kontrol("baslat").durum != 202 || !o.durum_bekle("kosuyor", 60000) || !o.hiz_gercek_zaman()) {
        return {};
    }
    return run_id_al(o.durum());
}

const sunucu::OlaySonucu* a3_sonucu(const Cikti& c, std::uint32_t no)
{
    const std::string on = "a3/" + std::to_string(no) + "/";
    for (const sunucu::OlaySonucu& x : c.olaylar) {
        if (x.kimlik.compare(0, on.size(), on) == 0) {
            return &x;
        }
    }
    return nullptr;
}

// A3 komutunun nihai sonucunun dünyanın kaydındaki sırası (yoksa -1): sonuçlar kayda verildikleri sırayla durur.
int sonuc_sirasi(const Cikti& c, std::uint32_t no)
{
    const std::string on = "a3/" + std::to_string(no) + "/";
    for (std::size_t i = 0; i < c.olaylar.size(); ++i) {
        if (c.olaylar[i].kimlik.compare(0, on.size(), on) == 0) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

std::string sonuc_metni(const sunucu::OlaySonucu* x) { return x == nullptr ? std::string("sonuç yok") : x->sonuc + (x->neden.empty() ? "" : " " + x->neden); }

// ---- W5-K55: emniyet_rev süreç boyunca yaşar ----

int test_emniyet_rev()
{
    GeciciKok k("emnrev");
    Oturum o(dosya_ayari(k, "z1.toml"));
    if (!o.baslat()) {
        dogrula(false, "sim başladı ve A3 dinliyor");
        return g_hata;
    }
    const json emn0 = emniyet_al(o);
    const std::uint32_t rev0 = emn0.value("emniyet_rev", 0u);
    dogrula(rev0 == 1u && !emn0.value("emniyet_rev_doydu", true), "süreç başında emniyet_rev 1, doymamış (A3-K117; üretim varsayılanı)");
    // 1. koşu: acil durdurma basılır; gevşetme jetonu alınır (kullanılmaz).
    const std::string run1 = kosu_baslat(o);
    dogrula(!run1.empty(), "1. koşu başladı");
    dogrula(acil_bas(o, run1).durum == 202 && acil_bekle(o, "basili"), "1. koşuda acil durdurma basıldı");
    const JetonYaniti j1 = jeton_al(o, run1);
    dogrula(j1.http == 200 && j1.jeton.size() == 32u && j1.rev > rev0, "1. koşuda gevşetme jetonu alındı; kısıtlayan revizyonu artırmış (" + std::to_string(j1.rev) + ")");
    // Sıfırla: revizyon geri dönmez; eski dünyanın oturum sonu artışından sonraki değerle sürer.
    dogrula(o.kontrol("sifirla").durum == 202 && o.durum_bekle("yuklendi"), "koşarken sıfırla: yuklendi");
    const Cikti d1 = o.dunya(0);
    const json emn1 = emniyet_al(o);
    const std::uint32_t rev1 = emn1.value("emniyet_rev", 0u);
    dogrula(rev1 > j1.rev, "sıfırlamadan sonra emniyet_rev geri dönmedi, oturum sonu artışıyla büyüdü (A3-K67, K117): " + std::to_string(j1.rev) + " → " +
                               std::to_string(rev1));
    dogrula(d1.son_emniyet_rev != 0u && rev1 == d1.son_emniyet_rev,
            "yuklendi'de yayınlanan revizyon eski dünyanın köprüsünün son değeri (" + std::to_string(d1.son_emniyet_rev) + ")");
    dogrula(o.durum().value("emniyet_rev", 0u) == rev1, "GET /durum aynı revizyonu veriyor");
    dogrula(emn1.contains("yetkili") && emn1["yetkili"] == emn0["yetkili"], "yeni dünyanın emniyet girdileri senaryonun başlangıç değerinde (A3-K91)");
    // 2. koşu: köprü devralınan revizyonla kurulur.
    const std::string run2 = kosu_baslat(o);
    dogrula(!run2.empty() && run2 != run1, "2. koşu yeni run_id ile başladı");
    const std::uint32_t rev2 = emniyet_rev_al(o);
    dogrula(rev2 >= rev1, "2. koşunun revizyonu devralınan değerden başladı (" + std::to_string(rev2) + ")");
    dogrula(acil_bas(o, run2).durum == 202 && acil_bekle(o, "basili"), "2. koşuda acil durdurma basıldı");
    const std::uint32_t rev3 = emniyet_rev_al(o);
    dogrula(rev3 > rev2, "kısıtlayan revizyonu artırdı (" + std::to_string(rev3) + ")");
    // Eski koşunun jetonu (A3-K94: sifirla ve yeni çalıştırma jetonu geçersizleştirir).
    const auto e1 = acil_birak(o, run2, j1.jeton, emniyet_rev_al(o));
    dogrula(e1.durum == 409 && e1.kod() == "onay_jetonu_gecersiz", "eski koşunun onay jetonu yeni koşuda 409 onay_jetonu_gecersiz (" + e1.kod() + ")");
    // Eski koşunun run_id'siyle gevşetme: jeton güncel ve geçerli olsa da reddedilir (A3-K54, K57).
    const std::string e2 = yanlis_run_gevsetme(o, run2, run1);
    dogrula(e2 == "409 beklenen_durum_uyusmuyor", "eski run_id'li gevşetme (güncel jetonla) 409 beklenen_durum_uyusmuyor (" + e2 + ")");
    const JetonYaniti j3 = jeton_al(o, run1);
    dogrula(j3.http == 409 && j3.kod == "beklenen_durum_uyusmuyor", "eski run_id ile onay jetonu verilmez (" + j3.kod + ")");
    dogrula(acil_al(emniyet_al(o)) == "basili", "reddedilen gevşetmelerden sonra acil durdurma basılı");
    // Güncel jeton ve run_id ile gevşetme uygulanır (denetimler geçerli isteği engellemiyor).
    std::string iz;
    const bool serbest = acil_serbest_birak(o, run2, iz);
    dogrula(serbest, "güncel jeton ve run_id ile gevşetme uygulandı (denemeler:" + iz + "; " + emniyet_al(o).dump() + ")");
    // İkinci sıfırlama: yine yalnız artar.
    const std::uint32_t rev4 = emniyet_rev_al(o);
    dogrula(o.kontrol("sifirla").durum == 202 && o.durum_bekle("yuklendi"), "2. koşu sıfırlandı");
    const std::uint32_t rev5 = emniyet_rev_al(o);
    dogrula(rev5 > rev4 && rev5 == o.dunya(1).son_emniyet_rev, "ikinci sıfırlamada da revizyon yalnız arttı: " + std::to_string(rev4) + " → " + std::to_string(rev5));
    return g_hata;
}

int test_emniyet_rev_doyma()
{
    GeciciKok k("doyma");
    sunucu::Ayar a = dosya_ayari(k, "z1.toml");
    a.sinama_ilk_emniyet_rev = 0xFFFFFFFDu;   // yalnız sınama: doymaya iki artış kala
    Oturum o(a);
    if (!o.baslat()) {
        dogrula(false, "sim başladı ve A3 dinliyor");
        return g_hata;
    }
    const json emn0 = emniyet_al(o);
    dogrula(emn0.value("emniyet_rev", 0u) == 0xFFFFFFFDu && !emn0.value("emniyet_rev_doydu", true), "sınama başlangıç revizyonu 0xFFFFFFFD, doymamış");
    const std::string run1 = kosu_baslat(o);
    dogrula(!run1.empty() && acil_bas(o, run1).durum == 202 && acil_bekle(o, "basili"), "1. koşuda acil durdurma basıldı (revizyon +1)");
    // Sıfırla: oturum sonu artışı revizyonu 0xFFFFFFFF'e taşır; doyma yeni dünyaya geçer.
    dogrula(o.kontrol("sifirla").durum == 202 && o.durum_bekle("yuklendi"), "koşarken sıfırla: yuklendi");
    const json emn1 = emniyet_al(o);
    dogrula(emn1.value("emniyet_rev", 0u) == 0xFFFFFFFFu && emn1.value("emniyet_rev_doydu", false),
            "sıfırlamadan sonra revizyon 0xFFFFFFFF ve doymuş (geri dönmedi): " + emn1.dump());
    dogrula(emn1.contains("yetkili") && emn1["yetkili"] == emn0["yetkili"], "girdiler başlangıç değerinde");
    // 2. koşu: köprü doymuş kurulur; gevşetme reddedilir, kısıtlayan uygulanır.
    const std::string run2 = kosu_baslat(o);
    dogrula(!run2.empty(), "2. koşu başladı");
    const json emn2 = emniyet_al(o);
    dogrula(emn2.value("emniyet_rev", 0u) == 0xFFFFFFFFu && emn2.value("emniyet_rev_doydu", false), "2. koşuda köprü doymuş: " + emn2.dump());
    dogrula(acil_bas(o, run2).durum == 202 && acil_bekle(o, "basili"), "doymuşken kısıtlayan uygulanır (A3-K92)");
    const JetonYaniti j = jeton_al(o, run2);
    dogrula(j.http == 409 && j.kod == "emniyet_rev_doydu", "doymuşken gevşetme jetonu verilmez: 409 emniyet_rev_doydu (" + j.kod + ")");
    dogrula(acil_al(emniyet_al(o)) == "basili", "acil durdurma basılı kaldı");
    // İkinci sıfırlama doymayı temizlemez (süreç yeniden başlayana kadar; A3-K117).
    dogrula(o.kontrol("sifirla").durum == 202 && o.durum_bekle("yuklendi"), "2. koşu sıfırlandı");
    const json emn3 = emniyet_al(o);
    dogrula(emn3.value("emniyet_rev", 0u) == 0xFFFFFFFFu && emn3.value("emniyet_rev_doydu", false), "ikinci sıfırlamadan sonra da doymuş: " + emn3.dump());
    const std::string run3 = kosu_baslat(o);
    const JetonYaniti j3 = jeton_al(o, run3);
    dogrula(!run3.empty() && j3.http == 409 && j3.kod == "emniyet_rev_doydu", "3. koşuda da gevşetme reddediliyor (" + j3.kod + ")");
    return g_hata;
}

// ---- W5-K56: sifirla ile aynı sınırda çekilen istekler ----

ida::a3::Komut k_emniyet(const ida::a3::AnlikDurum& a, std::uint32_t no, ida::a3::EmniyetGirdisi g, int tup, ida::a3::GirdiDurumu hedef, bool kisitlayan)
{
    ida::a3::Komut c;
    c.komut_no = no;
    c.istemci_id = 7;
    c.tur = ida::a3::KomutTuru::emniyet_degistir;
    c.beklenen_run_id = a.run_id;
    c.girdi = g;
    c.tup = static_cast<std::int8_t>(tup);
    c.hedef_durum = hedef;
    c.kisitlayan = kisitlayan;
    if (!kisitlayan) {
        c.beklenen_emniyet_rev = a.emniyet_rev;
    }
    return c;
}

ida::a3::Komut k_sifirla(const ida::a3::AnlikDurum& a, std::uint32_t no)
{
    ida::a3::Komut c;
    c.komut_no = no;
    c.istemci_id = 7;
    c.tur = ida::a3::KomutTuru::calistirma_sifirla;
    c.beklenen_run_id = a.run_id;
    c.beklenen_durum_rev = a.durum_rev;
    return c;
}

int test_sinir_yarisi()
{
    using ida::a3::AlimSonucu;
    using ida::a3::EmniyetGirdisi;
    using ida::a3::GirdiDurumu;
    GeciciKok k("yaris");
    sunucu::Ayar a = dosya_ayari(k, "z1.toml");
    // Enjeksiyon dünya iş parçacığında, sınır başında, istekler çekilmeden hemen önce yapılır: hepsi aynı
    // çekişe (aynı sınıra) düşer. Sonuçlar sınama iş parçacığınca dünya bittikten sonra okunur.
    std::atomic<int> evre{0};
    std::atomic<int> yapilan{0};
    std::array<bool, 4> kuyrukta{};
    std::uint32_t rev_enj = 0;
    std::uint64_t sinir_enj = 0;
    a.a3_sinir_kancasi = [&](std::uint64_t b, ida::a3::KomutAlicisi& al) {
        const int e = evre.load();
        if (e == 0) {
            return;
        }
        const ida::a3::AnlikDurum d = al.anlik();
        auto ver = [&](std::size_t i, const ida::a3::Komut& c) { kuyrukta[i] = al.al(c).tur == AlimSonucu::Tur::kuyrukta; };
        if (e == 1 && d.durum == ida::a3::CalistirmaDurumu::kosuyor && b >= 400u) {
            // (A) kısıtlayan (9001) < gevşeten (9002) < sifirla (9003) < kısıtlayan (9004)
            kuyrukta = {};
            rev_enj = d.emniyet_rev;
            sinir_enj = b;
            ver(0, k_emniyet(d, 9001, EmniyetGirdisi::acil_durdurma, -1, GirdiDurumu::basili, true));
            ver(1, k_emniyet(d, 9002, EmniyetGirdisi::emniyet_pimi, 0, GirdiDurumu::cikarilmis, false));
            ver(2, k_sifirla(d, 9003));
            ver(3, k_emniyet(d, 9004, EmniyetGirdisi::emniyet_pimi, 1, GirdiDurumu::takili, true));
        } else if (e == 2 && d.durum == ida::a3::CalistirmaDurumu::kosuyor && b >= 400u) {
            // (B) sifirla (9011) < kısıtlayan (9012)
            kuyrukta = {};
            ver(0, k_sifirla(d, 9011));
            ver(1, k_emniyet(d, 9012, EmniyetGirdisi::acil_durdurma, -1, GirdiDurumu::basili, true));
        } else if (e == 3 && d.durum == ida::a3::CalistirmaDurumu::hazirlaniyor) {
            // (C) hazırlıkta: kısıtlayan (9020) < gevşeten (9021) < sifirla (9022)
            kuyrukta = {};
            rev_enj = d.emniyet_rev;
            ver(0, k_emniyet(d, 9020, EmniyetGirdisi::acil_durdurma, -1, GirdiDurumu::basili, true));
            ver(1, k_emniyet(d, 9021, EmniyetGirdisi::emniyet_pimi, 0, GirdiDurumu::cikarilmis, false));
            ver(2, k_sifirla(d, 9022));
        } else {
            return;
        }
        yapilan.store(e);
        evre.store(0);
    };
    Oturum o(a);
    if (!o.baslat()) {
        dogrula(false, "sim başladı ve A3 dinliyor");
        return g_hata;
    }
    const json emn0 = emniyet_al(o);
    auto ret_mi = [](const sunucu::OlaySonucu* x) { return x != nullptr && x->sonuc == "reddedildi" && x->neden == "canli_calistirma_yok"; };
    auto uygulandi_mi = [](const sunucu::OlaySonucu* x) { return x != nullptr && x->sonuc == "uygulandi"; };

    // ---- (A) koşarken: sıfırlamadan küçük numaralı kısıtlayan eski dünyada uygulanır; ötekiler reddedilir ----
    evre.store(1);
    dogrula(o.kontrol("baslat").durum == 202 && o.dunya_bekle(1, 120000), "(A) koşu başladı ve enjekte edilen sıfırlamayla bitti");
    {
        const Cikti d = o.dunya(0);
        dogrula(yapilan.load() == 1 && kuyrukta[0] && kuyrukta[1] && kuyrukta[2] && kuyrukta[3],
                "(A) dört istek aynı sınırda (sınır " + std::to_string(sinir_enj) + ") kuyruğa alındı");
        dogrula(uygulandi_mi(a3_sonucu(d, 9003)) && d.calistirma_sonu == "yuklendi", "(A) sifirla uygulandı (" + sonuc_metni(a3_sonucu(d, 9003)) + ")");
        dogrula(uygulandi_mi(a3_sonucu(d, 9001)),
                "(A) sıfırlamadan küçük numaralı kısıtlayan (acil durdurma) eski dünyada uygulandı (" + sonuc_metni(a3_sonucu(d, 9001)) + ")");
        bool degisim = false;
        for (const sunucu::EmniyetDegisimi& x : d.emniyet) {
            degisim = degisim || (x.alan == "acil_durdurma" && !x.onceki && x.sonraki && x.kaynak == "operator" && x.komut_no == 9001u &&
                                  x.emniyet_rev == rev_enj + 1u && x.t_ns == static_cast<std::int64_t>(sinir_enj) * 1'250'000);
        }
        dogrula(degisim, "(A) acil durdurma değişimi kayıtta: operatör, komut 9001, aynı sınırda, revizyon +1");
        dogrula(ret_mi(a3_sonucu(d, 9002)), "(A) gevşeten istek canli_calistirma_yok ile reddedildi (" + sonuc_metni(a3_sonucu(d, 9002)) + ")");
        dogrula(ret_mi(a3_sonucu(d, 9004)),
                "(A) sıfırlamadan büyük numaralı kısıtlayan canli_calistirma_yok ile reddedildi (" + sonuc_metni(a3_sonucu(d, 9004)) + ")");
        dogrula(d.son_emniyet_rev >= static_cast<std::uint64_t>(rev_enj) + 2u,
                "(A) revizyon: kısıtlayan + oturum sonu (" + std::to_string(rev_enj) + " → " + std::to_string(d.son_emniyet_rev) + ")");
        dogrula(o.durum_bekle("yuklendi"), "(A) yeni dünya yuklendi");
        const json e = emniyet_al(o);
        dogrula(e.contains("yetkili") && e["yetkili"] == emn0["yetkili"] && e.value("emniyet_rev", 0u) == d.son_emniyet_rev,
                "(A) yeni dünya başlangıç girdileriyle ve devralınan revizyonla: " + e.dump());
    }
    // ---- (B) koşarken: sıfırlamadan büyük numaralı kısıtlayan uygulanmaz, reddedilir ----
    yapilan.store(0);
    evre.store(2);
    dogrula(o.kontrol("baslat").durum == 202 && o.dunya_bekle(2, 120000), "(B) koşu başladı ve enjekte edilen sıfırlamayla bitti");
    {
        const Cikti d = o.dunya(1);
        dogrula(yapilan.load() == 2 && kuyrukta[0] && kuyrukta[1], "(B) iki istek aynı sınırda kuyruğa alındı");
        dogrula(uygulandi_mi(a3_sonucu(d, 9011)) && d.calistirma_sonu == "yuklendi", "(B) sifirla uygulandı (" + sonuc_metni(a3_sonucu(d, 9011)) + ")");
        dogrula(ret_mi(a3_sonucu(d, 9012)), "(B) sonradan gelen kısıtlayan canli_calistirma_yok ile reddedildi (" + sonuc_metni(a3_sonucu(d, 9012)) + ")");
        bool acil = false;
        for (const sunucu::EmniyetDegisimi& x : d.emniyet) {
            acil = acil || x.komut_no == 9012u;
        }
        dogrula(!acil, "(B) reddedilen kısıtlayan yetkili durumu değiştirmedi");
        dogrula(o.durum_bekle("yuklendi"), "(B) yeni dünya yuklendi");
    }
    // ---- (C) hazırlanırken (W5-K65): sıfırlamadan küçük numaralı kısıtlayan eski dünyada uygulanır, sonucu sifirla'nın
    //      sonucundan önce kayda geçer; biriken gevşeten istek reddedilir ----
    yapilan.store(0);
    evre.store(3);
    dogrula(o.kontrol("baslat").durum == 202 && o.dunya_bekle(3, 120000), "(C) koşu istendi ve dünya bitti");
    {
        const Cikti d = o.dunya(2);
        dogrula(yapilan.load() == 3 && kuyrukta[0] && kuyrukta[1] && kuyrukta[2], "(C) üç istek hazırlık sırasında kuyruğa alındı");
        dogrula(uygulandi_mi(a3_sonucu(d, 9022)) && d.calistirma_sonu == "yuklendi" && d.csv.empty(),
                "(C) hazırlıkta sifirla uygulandı, koşu başlamadı (" + sonuc_metni(a3_sonucu(d, 9022)) + ", " + d.calistirma_sonu + ")");
        dogrula(ret_mi(a3_sonucu(d, 9021)), "(C) hazırlıkta biriken gevşeten istek canli_calistirma_yok ile reddedildi (" + sonuc_metni(a3_sonucu(d, 9021)) + ")");
        dogrula(uygulandi_mi(a3_sonucu(d, 9020)),
                "(C) sıfırlamadan küçük numaralı kısıtlayan (acil durdurma) hazırlıkta da eski dünyada uygulandı (" + sonuc_metni(a3_sonucu(d, 9020)) + ")");
        bool degisim = false;
        for (const sunucu::EmniyetDegisimi& x : d.emniyet) {
            degisim = degisim || (x.alan == "acil_durdurma" && !x.onceki && x.sonraki && x.kaynak == "operator" && x.komut_no == 9020u &&
                                  x.emniyet_rev == rev_enj + 1u);
        }
        dogrula(degisim, "(C) acil durdurma değişimi kayıtta: operatör, komut 9020, revizyon +1 (" + std::to_string(rev_enj) + " → +1)");
        const int s20 = sonuc_sirasi(d, 9020);
        const int s22 = sonuc_sirasi(d, 9022);
        dogrula(s20 >= 0 && s22 >= 0 && s20 < s22, "(C) kısıtlayanın sonucu sifirla'nınkinden önce kayda geçti (sıra " + std::to_string(s20) + " < " + std::to_string(s22) + ")");
        dogrula(d.son_emniyet_rev >= static_cast<std::uint64_t>(rev_enj) + 1u,
                "(C) revizyon arttı (" + std::to_string(rev_enj) + " → " + std::to_string(d.son_emniyet_rev) + ")");
        dogrula(o.durum_bekle("yuklendi"), "(C) yeni dünya yuklendi");
        const json e = emniyet_al(o);
        dogrula(e.contains("yetkili") && e["yetkili"] == emn0["yetkili"] && e.value("emniyet_rev", 0u) == d.son_emniyet_rev,
                "(C) yeni dünya başlangıç girdileriyle ve devralınan revizyonla: " + e.dump());
    }
    // Kısıtlayan (9020) eski dünyada sonuçlandı: yeni dünya onu yuvada bulup reddetmez (önceki "seçilen okuma" kalktı).
    o.kapat();
    {
        const Cikti d = o.dunya(3);
        dogrula(a3_sonucu(d, 9020) == nullptr, "(C) hazırlıkta uygulanan kısıtlayan yeni dünyanın kaydında yok (" + sonuc_metni(a3_sonucu(d, 9020)) + ")");
    }
    return g_hata;
}

// ---- W5-K64: alıcının çekişi sırasında kabul edilen "kısıtlayan (n), sifirla (n+1)" çifti ----

int test_cekis_yarisi()
{
    using ida::a3::AlimSonucu;
    using ida::a3::EmniyetGirdisi;
    using ida::a3::GirdiDurumu;
    GeciciKok k("cekis");
    sunucu::Ayar a = dosya_ayari(k, "z1.toml");
    // Enjeksiyon dünya iş parçacığında, alıcının çekişinin ortasında (evre 2: kısıtlayan yuvalar okunduktan sonra, genel
    // halka ve denetim yolları okunmadan önce) yapılır; G/Ç iş parçacığının tam bu pencerede iki isteği kabul etmesini
    // belirlenimci kurar: acil durdurma (9101) yuvaya, sifirla (9102) denetim yoluna girer. Sonuçlar sınama iş
    // parçacığınca dünya bittikten sonra okunur.
    std::atomic<bool> tetik{false};
    std::atomic<bool> yapildi{false};
    std::array<bool, 2> kuyrukta{};
    std::uint32_t rev_enj = 0;
    a.a3_cek_kancasi = [&](int evre, ida::a3::KomutAlicisi& al) {
        if (evre != 2 || !tetik.load() || yapildi.load()) {
            return;
        }
        const ida::a3::AnlikDurum d = al.anlik();
        if (d.durum != ida::a3::CalistirmaDurumu::kosuyor || d.adim_no < 400u) {
            return;
        }
        rev_enj = d.emniyet_rev;
        kuyrukta[0] = al.al(k_emniyet(d, 9101, EmniyetGirdisi::acil_durdurma, -1, GirdiDurumu::basili, true)).tur == AlimSonucu::Tur::kuyrukta;
        kuyrukta[1] = al.al(k_sifirla(d, 9102)).tur == AlimSonucu::Tur::kuyrukta;
        yapildi.store(true);
    };
    Oturum o(a);
    if (!o.baslat()) {
        dogrula(false, "sim başladı ve A3 dinliyor");
        return g_hata;
    }
    const json emn0 = emniyet_al(o);
    auto uygulandi_mi = [](const sunucu::OlaySonucu* x) { return x != nullptr && x->sonuc == "uygulandi"; };
    tetik.store(true);
    dogrula(o.kontrol("baslat").durum == 202 && o.dunya_bekle(1, 120000), "koşu başladı ve enjekte edilen sıfırlamayla bitti");
    const Cikti d = o.dunya(0);
    dogrula(yapildi.load() && kuyrukta[0] && kuyrukta[1], "acil durdurma (9101) ve sifirla (9102) çekişin ortasında kuyruğa alındı");
    dogrula(uygulandi_mi(a3_sonucu(d, 9102)) && d.calistirma_sonu == "yuklendi", "sifirla uygulandı (" + sonuc_metni(a3_sonucu(d, 9102)) + ")");
    dogrula(uygulandi_mi(a3_sonucu(d, 9101)),
            "acil durdurma eski dünyada uygulandı (yuvada kalıp yeni dünyaya geçmedi; " + sonuc_metni(a3_sonucu(d, 9101)) + ")");
    bool degisim = false;
    for (const sunucu::EmniyetDegisimi& x : d.emniyet) {
        degisim = degisim || (x.alan == "acil_durdurma" && !x.onceki && x.sonraki && x.kaynak == "operator" && x.komut_no == 9101u &&
                              x.emniyet_rev == rev_enj + 1u);
    }
    dogrula(degisim, "acil durdurma değişimi kayıtta: operatör, komut 9101, revizyon +1 (" + std::to_string(rev_enj) + " → +1)");
    const int s1 = sonuc_sirasi(d, 9101);
    const int s2 = sonuc_sirasi(d, 9102);
    dogrula(s1 >= 0 && s2 >= 0 && s1 < s2, "acil durdurmanın sonucu sifirla'nınkinden önce kayda geçti (sıra " + std::to_string(s1) + " < " + std::to_string(s2) + ")");
    dogrula(d.son_emniyet_rev >= static_cast<std::uint64_t>(rev_enj) + 2u,
            "revizyon: kısıtlayan + oturum sonu (" + std::to_string(rev_enj) + " → " + std::to_string(d.son_emniyet_rev) + ")");
    dogrula(o.durum_bekle("yuklendi"), "yeni dünya yuklendi");
    const json e = emniyet_al(o);
    dogrula(e.contains("yetkili") && e["yetkili"] == emn0["yetkili"] && e.value("emniyet_rev", 0u) == d.son_emniyet_rev,
            "yeni dünya başlangıç girdileriyle ve devralınan revizyonla: " + e.dump());
    o.kapat();
    dogrula(a3_sonucu(o.dunya(1), 9101) == nullptr, "yeni dünyanın kaydında acil durdurma (9101) için ret yok (eski dünyada sonuçlandı)");
    return g_hata;
}

// ---- W5-K57: canlı çalıştırmada senaryo ayrıştırılmaz ----

int test_canli_ayristirma()
{
    GeciciKok k("canli");
    Oturum o(dosya_ayari(k, "z1.toml"));
    if (!o.baslat()) {
        dogrula(false, "sim başladı ve A3 dinliyor");
        return g_hata;
    }
    auto yukle_ham = [&](const json& girdi, const json& run, std::uint64_t rev) {
        json g = girdi;
        g["beklenen_run_id"] = run;
        g["beklenen_durum_rev"] = rev;
        return o.post("/senaryo/yukle", g.dump());
    };
    const std::string bozuk = "bozuk = [";
    // yuklendi: bayat revizyon ve yanlış run_id, dosyaya ve ayrıştırmaya bakılmadan reddedilir (denetim sırası:
    // olmayan dosya 404 ya da bozuk içerik 422 değil, 409).
    const std::uint64_t rev0 = o.durum().value("durum_rev", 0u);
    auto y = yukle_ham(json{{"ad", "yok.toml"}}, nullptr, rev0 + 7u);
    dogrula(y.durum == 409 && y.kod() == "durum_rev_uyusmuyor", "yuklendi: bayat durum_rev, olmayan dosya → 409 durum_rev_uyusmuyor (" +
                                                                    std::to_string(y.durum) + " " + y.kod() + ")");
    y = yukle_ham(json{{"icerik", bozuk}}, nullptr, rev0 + 7u);
    dogrula(y.durum == 409 && y.kod() == "durum_rev_uyusmuyor", "yuklendi: bayat durum_rev, bozuk içerik → 409 durum_rev_uyusmuyor (" +
                                                                    std::to_string(y.durum) + " " + y.kod() + ")");
    y = yukle_ham(json{{"ad", "yok.toml"}}, "20260101-000000-abcdef", rev0);
    dogrula(y.durum == 409 && y.kod() == "beklenen_durum_uyusmuyor", "yuklendi: yanlış run_id, olmayan dosya → 409 beklenen_durum_uyusmuyor (" +
                                                                         std::to_string(y.durum) + " " + y.kod() + ")");
    // Denetimden geçen istek olağan sonucu alır (ayrıştırma yapılır).
    y = yukle_ham(json{{"ad", "yok.toml"}}, nullptr, rev0);
    dogrula(y.durum == 404, "yuklendi: geçerli revizyon, olmayan dosya → 404 (" + std::to_string(y.durum) + ")");
    y = yukle_ham(json{{"icerik", bozuk}}, nullptr, rev0);
    dogrula(y.durum == 422 && y.kod() == "senaryo_gecersiz", "yuklendi: geçerli revizyon, bozuk içerik → 422 senaryo_gecersiz");
    dogrula(o.post_ham("/senaryo/dogrula", json{{"ad", "z1.toml"}}.dump()).durum == 200, "yuklendi: doğrula 200");

    // Canlı çalıştırma: kosuyor.
    const std::string run = kosu_baslat(o);
    dogrula(!run.empty(), "koşu başladı (gerçek zaman)");
    auto canli_denetle = [&](const std::string& durum) {
        const json d = o.durum();
        const std::uint64_t rev = d.value("durum_rev", 0u);
        auto g = o.post_ham("/senaryo/dogrula", json{{"ad", "z1.toml"}}.dump());
        dogrula(g.durum == 409 && g.kod() == "gecersiz_durum", durum + ": doğrula {ad} → 409 gecersiz_durum (" + std::to_string(g.durum) + " " + g.kod() + ")");
        g = o.post_ham("/senaryo/dogrula", json{{"icerik", bozuk}}.dump());
        dogrula(g.durum == 409 && g.kod() == "gecersiz_durum", durum + ": doğrula {icerik} → 409 gecersiz_durum (" + std::to_string(g.durum) + " " + g.kod() + ")");
        g = o.post_ham("/senaryo/dogrula", json{{"ad", "yok.toml"}}.dump());
        dogrula(g.durum == 409 && g.kod() == "gecersiz_durum", durum + ": doğrula, olmayan dosya → 409 (dosyaya bakılmadı; " + std::to_string(g.durum) + ")");
        g = yukle_ham(json{{"icerik", bozuk}}, run, rev);
        dogrula(g.durum == 409 && g.kod() == "gecersiz_durum", durum + ": yükle, bozuk içerik → 409 gecersiz_durum (" + std::to_string(g.durum) + " " + g.kod() + ")");
        g = yukle_ham(json{{"ad", "yok.toml"}}, run, rev);
        dogrula(g.durum == 409 && g.kod() == "gecersiz_durum", durum + ": yükle, olmayan dosya → 409 gecersiz_durum (" + std::to_string(g.durum) + " " + g.kod() + ")");
        g = yukle_ham(json{{"ad", "ornek-havuz.toml"}}, run, rev);
        dogrula(g.durum == 409 && g.kod() == "gecersiz_durum", durum + ": yükle, geçerli senaryo → 409 gecersiz_durum (Tablo 4.7a)");
        const json s = o.durum();
        dogrula(s.value("durum", "") == durum && s.value("durum_rev", 0u) == rev && run_id_al(s) == run, durum + ": reddedilen istekler durumu değiştirmedi");
    };
    canli_denetle("kosuyor");
    dogrula(o.kontrol("duraklat").durum == 202 && o.durum_bekle("duraklatildi"), "koşu duraklatıldı");
    canli_denetle("duraklatildi");
    // Çalıştırma bitince uçlar yeniden kullanılır.
    dogrula(o.kontrol("durdur").durum == 202 && o.durum_bekle("durduruldu"), "koşu durduruldu");
    const auto dg = o.post_ham("/senaryo/dogrula", json{{"ad", "z1.toml"}}.dump());
    dogrula(dg.durum == 200 && json::parse(dg.govde, nullptr, false).value("gecerli", false), "durduruldu: doğrula 200, geçerli");
    const auto yk = o.yukle(json{{"ad", "ornek-havuz.toml"}});
    dogrula(yk.durum == 200 && o.durum_bekle("yuklendi"), "durduruldu: yükle 200, yuklendi (" + std::to_string(yk.durum) + " " + yk.kod() + ")");
    return g_hata;
}

// ---- W5-K58, K59: yeni dünyanın emniyet karesi, geçiş penceresi, A1 kapanış nedeni ----

bool ws_bekle(ida::test::a3::Istemci& ws, const std::function<bool()>& kosul, int ms = 15000)
{
    const auto son = std::chrono::steady_clock::now() + std::chrono::milliseconds(ms);
    while (std::chrono::steady_clock::now() < son) {
        if (kosul()) {
            return true;
        }
        if (!ws.oku(50)) {
            return kosul();
        }
    }
    return kosul();
}

std::string kare_metni(const std::vector<ida::test::a3::EmniyetKaresi>& v)
{
    if (v.empty()) {
        return "kare yok";
    }
    const ida::test::a3::EmniyetKaresi& e = v.back();
    return "son kare: acil " + std::to_string(e.acil) + ", rev " + std::to_string(e.emniyet_rev) + ", " + std::to_string(v.size()) + " kare";
}

int test_emniyet_karesi()
{
    GeciciKok k("kare");
    sunucu::Ayar a = dosya_ayari(k, "z1.toml");
    // Geçiş penceresi: sürücü yeni dünyayı kurmadan hemen önce (eski dünya yıkıldı) dünya kancasını çağırır;
    // pencere o anda belirlenimli gözlenir. Komut alıcısı süreç boyunca yaşar (sınır kancasından alınır):
    // görüntüsü ve giriş denetimi doğrudan sorulur. Sıfırlama penceresinde (dünya 1) ayrıca gerçek HTTP ile
    // durum okunur ve `baslat` denenir; yükleme penceresinde HTTP kullanılmaz (G/Ç iş parçacığı o sırada
    // `senaryo/yukle` yanıtını bekler: inceleme bulgusu 5, bu turun dışında).
    struct Pencere {
        // Alıcıdan:
        bool alici_var = false, yuklendi = false, ozet_yok = false, run_yok = false;
        std::string baslat_ret;   // alıcının `baslat`'a verdiği ret kodu; boş: reddedilmedi
        // HTTP'den (yalnız sıfırlama penceresi):
        std::string durum;
        bool ozet_null = false, run_null = false;
        std::uint32_t emniyet_rev = 0;
        int baslat_http = 0;
        std::string baslat_kod;
    };
    std::vector<Pencere> pencereler;
    std::mutex pm;
    std::atomic<Oturum*> po{nullptr};
    std::atomic<ida::a3::KomutAlicisi*> alici{nullptr};
    a.a3_sinir_kancasi = [&](std::uint64_t, ida::a3::KomutAlicisi& al) { alici.store(&al); };
    a.dunya_kancasi = [&](std::uint64_t n) {
        Oturum* o = po.load();
        if (n == 0u || o == nullptr) {
            return;
        }
        Pencere p;
        if (ida::a3::KomutAlicisi* al = alici.load()) {
            const ida::a3::AnlikDurum d = al->anlik();
            p.alici_var = true;
            p.yuklendi = d.durum == ida::a3::CalistirmaDurumu::yuklendi;
            p.ozet_yok = !d.senaryo_ozeti.has_value();
            p.run_yok = !d.run_id.has_value();
            ida::a3::Komut c;
            c.komut_no = 9100u + static_cast<std::uint32_t>(n);
            c.istemci_id = 7;
            c.tur = ida::a3::KomutTuru::calistirma_baslat;
            c.beklenen_durum_rev = d.durum_rev;
            const ida::a3::AlimSonucu r = al->al(c);
            p.baslat_ret = r.tur == ida::a3::AlimSonucu::Tur::ret ? r.hata_kodu : std::string();
        }
        if (n == 1u) {
            const json d = o->durum();
            p.durum = d.value("durum", std::string());
            p.ozet_null = d.contains("senaryo_ozeti") && d["senaryo_ozeti"].is_null();
            p.run_null = d.contains("run_id") && d["run_id"].is_null();
            p.emniyet_rev = emniyet_al(*o).value("emniyet_rev", 0u);
            const auto y = o->post("/calistirma/baslat", json{{"beklenen_run_id", nullptr}, {"beklenen_durum_rev", d.value("durum_rev", 0u)}}.dump());
            p.baslat_http = y.durum;
            p.baslat_kod = y.kod();
        }
        std::lock_guard<std::mutex> kl(pm);
        pencereler.push_back(std::move(p));
    };
    Oturum o(a);
    po.store(&o);
    if (!o.baslat()) {
        dogrula(false, "sim başladı ve A3 dinliyor");
        return g_hata;
    }
    auto pencere = [&](std::size_t i) {
        std::lock_guard<std::mutex> kl(pm);
        return i < pencereler.size() ? pencereler[i] : Pencere{};
    };
    const json emn0 = emniyet_al(o);
    const std::uint8_t acil0 = acil_al(emn0) == "basili" ? std::uint8_t{1} : std::uint8_t{0};
    ida::test::a3::Istemci ws;
    dogrula(ws.baglan(o.port(), o.jeton()) && ws.abone({{"emniyet/girdiler", 10}}), "WebSocket abonesi: emniyet/girdiler");
    // (0) İlk dünya `yuklendi`: abone senaryonun başlangıç değerlerini görür.
    dogrula(ws_bekle(ws, [&] { return !ws.emniyet.empty(); }) && !ws.emniyet.empty() && ws.emniyet.back().acil == acil0 &&
                ws.emniyet.back().emniyet_rev == emn0.value("emniyet_rev", 0u),
            "yuklendi'de emniyet karesi: başlangıç değerleri, revizyon " + std::to_string(emn0.value("emniyet_rev", 0u)) + " (" + kare_metni(ws.emniyet) + ")");
    // (1) Koşu: acil durdurma basılır; abone basılı kareyi görür.
    const std::string run1 = kosu_baslat(o);
    dogrula(!run1.empty() && acil_bas(o, run1).durum == 202 && acil_bekle(o, "basili"), "koşuda acil durdurma basıldı");
    dogrula(ws_bekle(ws, [&] { return !ws.emniyet.empty() && ws.emniyet.back().acil == 1u; }), "abone basılı kareyi gördü (" + kare_metni(ws.emniyet) + ")");
    // (2) Sıfırla: yeni dünya `yuklendi`'de başlangıç değerleriyle kare yayınlar; eski abone bayat kareyle kalmaz.
    dogrula(o.kontrol("sifirla").durum == 202 && o.durum_bekle("yuklendi", 30000), "koşarken sıfırla: yuklendi");
    const json emn1 = emniyet_al(o);
    const std::uint32_t rev1 = emn1.value("emniyet_rev", 0u);
    dogrula(ws_bekle(ws, [&] { return !ws.emniyet.empty() && ws.emniyet.back().acil == acil0 && ws.emniyet.back().emniyet_rev == rev1; }),
            "eski abone sıfırlamadan sonra başlangıç değerli kareyi gördü (GET /emniyet ile aynı: rev " + std::to_string(rev1) + "; " +
                kare_metni(ws.emniyet) + ")");
    // (3) Yeni abone tam durumda aynı kareyi alır.
    {
        ida::test::a3::Istemci ws2;
        dogrula(ws2.baglan(o.port(), o.jeton()) && ws2.abone({{"emniyet/girdiler", 10}}), "ikinci abone bağlandı (sıfırlamadan sonra)");
        dogrula(ws_bekle(ws2, [&] { return !ws2.emniyet.empty(); }, 5000) && !ws2.emniyet.empty() && ws2.emniyet.back().acil == acil0 &&
                    ws2.emniyet.back().emniyet_rev == rev1,
                "yeni abone yuklendi'de başlangıç değerli kareyi aldı (" + kare_metni(ws2.emniyet) + ")");
        ws2.kapat();
    }
    // (4) Geçiş penceresi (sıfırlama): durum yuklendi, özet null, `baslat` reddedilir.
    {
        const Pencere p = pencere(0);
        dogrula(p.durum == "yuklendi" && p.run_null && p.ozet_null, "geçiş penceresi: durum yuklendi, run_id null, senaryo özeti null (" + p.durum + ")");
        dogrula(p.baslat_http == 409 && p.baslat_kod == "gecersiz_durum",
                "geçiş penceresinde baslat 409 gecersiz_durum (" + std::to_string(p.baslat_http) + " " + p.baslat_kod + ")");
        dogrula(p.alici_var && p.yuklendi && p.ozet_yok && p.run_yok && p.baslat_ret == "gecersiz_durum",
                "geçiş penceresi (alıcı görüntüsü): yuklendi, özet yok; giriş denetimi baslat'ı gecersiz_durum ile reddetti (" + p.baslat_ret + ")");
        dogrula(p.emniyet_rev == rev1, "geçiş penceresinde yayınlanan emniyet_rev yeni dünyanınkiyle aynı (" + std::to_string(p.emniyet_rev) + ")");
        const json d = o.durum();
        dogrula(d.value("durum", "") == "yuklendi" && d["run_id"].is_null() && o.dunya_sayisi() == 1u, "pencerede reddedilen baslat koşu başlatmadı");
    }
    // (5) Sıfırlamada A1 oturumu RUN_END (5) ile kapanır; olay nedeni planli_sifirlama (W5-K59; A1-K128).
    {
        const Cikti d1 = o.dunya(0);
        std::string nedenler;
        for (const std::uint8_t r : d1.kapanis) {
            nedenler += (nedenler.empty() ? "" : ",") + std::to_string(r);
        }
        dogrula(!d1.kapanis.empty() && d1.kapanis.back() == 5u && std::find(d1.kapanis.begin(), d1.kapanis.end(), std::uint8_t{1}) == d1.kapanis.end(),
                "sıfırlanan koşunun A1 oturumu IDA_SESSION_CLOSE(RUN_END) ile kapandı (nedenler: " + nedenler + ")");
        auto olay_var = [&](const char* neden) {
            const std::string ara = std::string("\"neden\":\"") + neden + "\"";
            for (std::size_t i = 0; i < ws.olaylar.size(); ++i) {
                if (ws.olay_turleri[i] == 9u && ws.olaylar[i].find(ara) != std::string::npos) {
                    return true;
                }
            }
            return false;
        };
        dogrula(ws_bekle(ws, [&] { return olay_var("planli_sifirlama"); }, 5000), "a1_oturum_degisti olayı: neden planli_sifirlama");
        dogrula(!olay_var("sim_kapanisi"), "süreç kapanmadı: sim_kapanisi nedeni yayınlanmadı");
    }
    // (6) Yükleme: geçiş penceresi aynı; yeni senaryonun karesi yayınlanır.
    {
        const std::size_t once = ws.emniyet.size();
        const auto y = o.yukle(json{{"ad", "ornek-havuz.toml"}});
        dogrula(y.durum == 200 && o.durum_bekle("yuklendi", 30000), "yuklendi'de yeni senaryo yüklendi (" + std::to_string(y.durum) + " " + y.kod() + ")");
        const Pencere p = pencere(1);
        dogrula(p.alici_var && p.yuklendi && p.ozet_yok && p.run_yok && p.baslat_ret == "gecersiz_durum",
                "yükleme penceresi (alıcı görüntüsü): yuklendi, özet yok; baslat gecersiz_durum ile reddedildi (" + p.baslat_ret + ")");
        const json d = o.durum();
        dogrula(d.value("durum", "") == "yuklendi" && d["run_id"].is_null() && !ozet_al(d).empty(), "yüklemeden sonra durum yuklendi, özet dolu, koşu yok");
        dogrula(ws_bekle(ws, [&] { return ws.emniyet.size() > once; }), "yüklenen dünya emniyet karesini yayınladı (" + kare_metni(ws.emniyet) + ")");
    }
    // (7) Süreç kapanışı (sinyal) canlı koşuda: IDA_SESSION_CLOSE(SHUTDOWN = 1).
    const std::string run2 = kosu_baslat(o);
    dogrula(!run2.empty(), "kapanış öncesi koşu başladı");
    const std::size_t n = o.dunya_sayisi();
    ws.kapat();
    o.kapat();
    {
        const Cikti son = o.dunya(n);
        dogrula(o.dunya_sayisi() == n + 1u && !son.kapanis.empty() && son.kapanis.back() == 1u,
                "süreç kapanışında A1 oturumu IDA_SESSION_CLOSE(SHUTDOWN) ile kapandı (son neden " +
                    std::to_string(son.kapanis.empty() ? 0u : son.kapanis.back()) + ")");
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
    } else if (ad == "senaryo_emniyet_rev") {
        h = test_emniyet_rev();
    } else if (ad == "senaryo_emniyet_rev_doyma") {
        h = test_emniyet_rev_doyma();
    } else if (ad == "senaryo_sinir_yarisi") {
        h = test_sinir_yarisi();
    } else if (ad == "senaryo_cekis_yarisi") {
        h = test_cekis_yarisi();
    } else if (ad == "senaryo_canli_ayristirma") {
        h = test_canli_ayristirma();
    } else if (ad == "senaryo_emniyet_karesi") {
        h = test_emniyet_karesi();
    } else {
        std::printf("bilinmeyen test: %s\n", ad.c_str());
        return 2;
    }
    std::printf("%s %s: %d kalan\n", ad.c_str(), h == 0 ? "GECTI" : "KALDI", h);
    return h == 0 ? 0 : 1;
}
