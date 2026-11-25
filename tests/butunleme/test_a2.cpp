// W5-bagla-a2 — A2 sunucusunun `ida_sim`'e bağlanması: bütünleme testleri (docs/gorevler/W5-bagla-a2.md
// kabul 1–5). Karşı uç gerçek istemcidir: `viz/a2`'nin istemci oturumu ve kodeği (`ida_viz_a2`), gerçek
// soketle; sahte istemci düzeneği `sim/a2/test/test_destek.*`'ten alınır ("damla okuma" dahil). Testler
// sabit 47210 portunu ve gerçek `ida/a2.kilit` dosyasını kullanmaz: işletim sisteminin verdiği port,
// tekil geçici dizinde kilit.
//
// Eşzamanlama sabit uykuyla değil işaretle yapılır: dünya iş parçacığı sınama kancasında (yalnız SIL;
// sim zamanı donuktur) istemcinin beklenen duruma gelmesini bekler; her bekleyişin asılma sınırı vardır
// (ölçüt değil).
//
//   a2_varsayilan  kabul 1: `--a2-port` yokken A2 başlatılmaz (kilit ve dinleyici denenmez), sonuçta A2
//                  kapalı; yerleşik senaryoda `--a2-port` kullanım hatası; `kalite.gorunum = kapali` iken
//                  dinleyici açılmaz (A2-K02) ve çıktı A2'siz koşuyla aynı
//   a2_belirlenim  kabul 2 (A2-U10, SIL): aynı senaryo beş koşulda (A2 kapalı; açık, istemci yok; istemci
//                  bağlı ve okuyor; bağlı ve okumuyor; koşunun ortasında kapanıyor) bayt bayt aynı CSV ve
//                  iki yönde aynı A1 akışı
//   a2_hil_donuk   kabul 2 (POSIX, kartsız HIL 1x): A2 açık ve donuk istemciyle kaçırılan parti ve bütçe
//                  aşımı sayaçları A2 kapalı koşudan farklı değil (ortak zamanlama yardımcısı)
//   a2_uctan_uca   kabul 3: el sıkışma, tanım (manifest yolu ve özeti dosyadan yeniden hesaplanır), her
//                  `durum` iletisindeki araç pozu aynı sınırın dünya durumuyla ve CSV satırıyla bit
//                  düzeyinde aynı; v_n ve w_b hareketli koşuda dünyanın ardışık sınırlarıyla tutarlı
//   a2_duraklat    kabul 4: duraklat → `duraklat` olayı, `durum` yok, nabız sürer; adımla → her adım bir
//                  `durum`; sürdür → akış kaldığı sınırdan; CSV duraklatmasız A2'siz koşuyla aynı; A3'ün
//                  görünüm kademesi alanı (kapsam 6)
//   a2_3b_senaryo  `scenarios/gorev-havuz-3b.toml` (havuz kümesi) A2 ile başlar, hosgeldin manifest özeti tutar;
//                  `--sil-hiz` varken SIL hız alanı uyarısı komut satırını söyler
//   sil_hiz        `--sil-hiz` (kapsam 7'nin gerçek zamana yakın SIL'i): yalnız bekleme; CSV ve A1 akışı aynı
//   a2_kapanis     kabul 5: koşunun ortasında bağlanan istemci tutarlı tam durum alır; kapanış isteğinde
//                  istemci son durumu `durdu` ile ve veda(sim_kapaniyor) görür; port dolu ve kilit
//                  başkasında iken koşu A2'siz sürer ve sonuçta görünür; geçersiz portla koşu başlamaz
#include "test_destek.hpp"   // sim/a2/test: SoketIstemci, Kaydedici, SunucuDuzenegi, gecici_dizin

#include "ida/a3/komut.hpp"
#include "ida/senaryo/senaryo.hpp"
#include "ida/sunucu/sunucu.hpp"
#include "ida/viz/sha256.hpp"
#include "../../sim/kopru/test/zamanlama_olcutu.hpp"   // W4-K24, W4-K29

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iterator>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#ifndef IDA_DEPO_KOKU
#define IDA_DEPO_KOKU "."
#endif

namespace {

using namespace ida::a2;         // tel türleri
using namespace ida::a2::test;   // SoketIstemci, Kaydedici, …
namespace sunucu = ida::sunucu;
namespace senaryo = ida::senaryo;

int g_hata = 0;

void dogrula(bool kosul, const std::string& ileti)
{
    std::printf("%s %s\n", kosul ? "GECTI " : "KALDI ", ileti.c_str());
    if (!kosul) {
        ++g_hata;
    }
}

constexpr double kH = 1.25e-3;   // fizik adımı (s)

std::string sayi(std::uint64_t v) { return std::to_string(v); }

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

// Depodaki bir senaryodan bellekte türetilmiş senaryo (yol aynı kalır; araç dosyası ve A7 manifesti
// diskten). `duzenle` metni değiştirir.
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

// Senaryonun süresini değiştirir (`sure_ns = …` satırı).
std::function<void(std::string&)> sure(double saniye)
{
    return [saniye](std::string& m) {
        const std::size_t i = m.find("sure_ns = ");
        const std::size_t j = i == std::string::npos ? i : m.find('\n', i);
        if (i == std::string::npos || j == std::string::npos) {
            dogrula(false, "sınama hatası: sure_ns satırı yok");
            return;
        }
        m.replace(i, j - i, "sure_ns = " + std::to_string(static_cast<long long>(saniye * 1e9)));
    };
}

const char* const kGorev = "scenarios/gorev-havuz.toml";

// Bir `ida_sim` koşusu (kitaplık; ayrı iş parçacığında) ve dünya tarafının gözlemleri.
struct Kosu {
    sunucu::Ayar a;
    sunucu::Sonuc s;
    std::thread is;
    std::atomic<bool> bitti{false};
    std::atomic<int> port{-1};          // −1: A2 henüz başlamadı; 0: A2'siz
    std::atomic<bool> dinliyor{false};
    std::string dizin;                  // kilit dosyasının tekil geçici dizini
    // Dünyanın sınır b'deki yer gerçeği (sinir_kancasi; fizik adımı b'den önce). Yalnız koşu bitince okunur.
    std::vector<std::array<double, 3>> p;
    std::vector<std::array<double, 4>> q;
    Fnv giden, gelen;                   // A1 bayt akışı (sim → karşı taraf, karşı taraf → sim)
    std::function<void(std::uint64_t b)> sinirda;   // dünya iş parçacığında, sınır b'nin kancasında

    explicit Kosu(const std::string& dosya, std::shared_ptr<const senaryo::DosyaKaynagi> k = nullptr)
    {
        a.gunluk = 0;
        a.senaryo_dosyasi = dosya;
        a.depo_koku = IDA_DEPO_KOKU;
        a.senaryo_kaynagi = std::move(k);
        a.csv_uret = true;
    }
    Kosu(const Kosu&) = delete;
    Kosu& operator=(const Kosu&) = delete;
    ~Kosu()
    {
        bekle();
        if (!dizin.empty()) {
            std::error_code ec;
            std::filesystem::remove_all(dizin, ec);
        }
    }

    // A2 açık: işletim sisteminin verdiği port, tekil geçici dizinde kilit.
    void a2_ac()
    {
        dizin = gecici_dizin("bagla");
        a.a2_acik = true;
        a.a2_sinama_bos_port = true;
        a.a2_kilit_dosyasi = dizin + "/a2.kilit";
        a.a2_hazir = [this](std::uint16_t pt, bool d) {
            dinliyor.store(d);
            port.store(static_cast<int>(pt));
        };
    }

    void baslat()
    {
        a.sinir_kancasi = [this](const sunucu::Ayar::SinirGozlemi& g) {
            if (p.size() <= g.b) {
                p.resize(g.b + 1u);
                q.resize(g.b + 1u);
            }
            p[g.b] = g.konum;
            q[g.b] = g.yonelim;
            if (sinirda) {
                sinirda(g.b);
            }
        };
        a.a1_bayt_kancasi = [this](bool sim_giden, const std::uint8_t* b, std::size_t n) {
            (sim_giden ? giden : gelen).ekle(b, n);
        };
        is = std::thread([this] {
            s = sunucu::kos(a);
            bitti.store(true);
        });
    }

    void bekle()
    {
        if (is.joinable()) {
            is.join();
        }
    }

    Kosu& kos()
    {
        baslat();
        bekle();
        return *this;
    }

    // A2 dinleyicisinin portu belli olana kadar bekler (asılma sınırı 30 s).
    bool port_bekle()
    {
        return kosul_bekle([this] { return port.load() >= 0 || bitti.load(); }, 30000) && port.load() > 0;
    }
};

// Dünya iş parçacığında (sınama kancası) `kosul` sağlanana kadar bekler; asılma sınırı 30 s.
void dunyada_bekle(const std::function<bool()>& kosul)
{
    const auto son = std::chrono::steady_clock::now() + std::chrono::seconds(30);
    while (!kosul() && std::chrono::steady_clock::now() < son) {
        std::this_thread::sleep_for(std::chrono::microseconds(200));
    }
}

// hosgeldin'deki manifest dosyasını okur ve özetini hesaplar (görselleştiricinin yaptığı: A2-K13).
bool manifest_ozeti(const viz::HosgeldinIleti& h, viz::Ozet& ozet)
{
    const std::size_t n = ::strnlen(h.h.manifest_yolu, sizeof h.h.manifest_yolu);
    const std::u8string u(h.h.manifest_yolu, h.h.manifest_yolu + n);
    std::ifstream f(std::filesystem::path(u), std::ios::binary);
    if (!f) {
        return false;
    }
    const std::string bayt((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    ozet = viz::sha256(bayt.data(), bayt.size());
    return true;
}

// Gerçek istemci oturumunu el sıkışmadan akışa getirir: `hazir`, manifest dosyasından hesaplanan
// özetle gönderilir (sim'in bildirdiği özet yanlışsa sunucu `ret(manifest)` verir).
bool akisa_getir(SoketIstemci& c, int ms = 20000)
{
    c.otomatik_hazir = false;
    c.basla();
    if (!c.bekle([&] { return c.ist.asama() == viz::Asama::uygulama_bekleniyor || c.karsi_kapatti; }, ms) ||
        c.ist.asama() != viz::Asama::uygulama_bekleniyor) {
        return false;
    }
    viz::Ozet o{};
    if (!manifest_ozeti(c.ist.hosgeldin(), o)) {
        return false;
    }
    c.ist.sahne_sonucu(HazirDurumu::basarili, o);
    return c.bekle([&] { return c.ist.asama() == viz::Asama::akis || c.karsi_kapatti; }, ms) &&
           c.ist.asama() == viz::Asama::akis;
}

// Koşu bitene ve bağlantı kapanana kadar istemciyi pompalar (asılma sınırı).
void sonuna_kadar(SoketIstemci& c, Kosu& k, int ms = 120000)
{
    (void)c.bekle([&] { return k.bitti.load() && (c.karsi_kapatti || !c.oku); }, ms);
    if (c.oku) {
        (void)c.bekle([&] { return c.karsi_kapatti; }, 2000);
    }
}

std::vector<const Cerceve*> turunden(const Kaydedici& k, IletiTuru t)
{
    std::vector<const Cerceve*> v;
    for (const Cerceve& c : k.cerceveler) {
        if (c.tur() == t) {
            v.push_back(&c);
        }
    }
    return v;
}

bool bit_ayni(const double* a, const double* b, std::size_t n) { return std::memcmp(a, b, n * sizeof(double)) == 0; }

double norm3(const double* v) { return std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]); }

// sim/sunucu'nun CSV'sindeki yön (Euler ZYX) ile aynı formül.
double yon(const double* q) { return std::atan2(2.0 * (q[0] * q[3] + q[1] * q[2]), 1.0 - 2.0 * (q[2] * q[2] + q[3] * q[3])); }

bool istemci_protokol_hatasiz(const SoketIstemci& c)
{
    return !c.k.cerceve_hatasi && c.ist.son_kapanis_nedeni().find("protokol") == std::string::npos;
}

// ------------------------------------------------------------------------------------------------ kabul 1

int test_varsayilan()
{
    const std::uint64_t once = sunucu::a2_baslatma_sayisi();
    {
        sunucu::Ayar a;   // yerleşik z1
        a.gunluk = 0;
        a.sure_s = 3.0;
        const sunucu::Sonuc s = sunucu::kos(a);
        dogrula(s.tamam && !s.a2.istendi && s.a2.durum == "kapali" && s.a2.port == 0u && s.a2.sayac.baglanti_sayisi == 0u,
                "yerleşik z1, --a2-port yok: koşu tamam, A2 kapalı");
    }
    Kosu z1("scenarios/z1.toml", kaynak("scenarios/z1.toml", sure(6.0)));
    z1.kos();
    dogrula(z1.s.tamam && !z1.s.a2.istendi && z1.s.a2.durum == "kapali", "scenarios/z1.toml, --a2-port yok: A2 kapalı");
    Kosu gorev(kGorev, kaynak(kGorev, sure(20.0)));
    gorev.kos();
    dogrula(gorev.s.tamam && !gorev.s.a2.istendi && gorev.s.a2.durum == "kapali", "görev senaryosu, --a2-port yok: A2 kapalı");
    dogrula(sunucu::a2_baslatma_sayisi() == once,
            "--a2-port yokken A2 sunucusu hiç başlatılmadı (sahiplik kilidi alınmadı, port açılmadı): sayaç " + sayi(once));

    // Yerleşik senaryoda --a2-port: tanım yok → kullanım hatası; sunucu denenmez.
    {
        sunucu::Ayar a;
        a.gunluk = 0;
        a.sure_s = 1.0;
        a.a2_acik = true;
        a.a2_sinama_bos_port = true;
        const sunucu::Sonuc s = sunucu::kos(a);
        dogrula(!s.tamam && s.hata_turu == sunucu::HataTuru::kullanim && s.satirlar.empty(),
                "yerleşik senaryoda --a2-port kullanım hatası: " + s.hata);
        dogrula(sunucu::a2_baslatma_sayisi() == once, "kullanım hatasında A2 sunucusu denenmedi");
    }
    // kalite.gorunum = kapali: dinleyici açılmaz (A2-K02), kilit alınmaz; koşu A2'siz koşuyla aynı.
    {
        const auto duzen = [](std::string& m) {
            sure(20.0)(m);
            degistir(m, "kamera = \"kapali\"", "kamera = \"kapali\"\ngorunum = \"kapali\"");
        };
        Kosu ref(kGorev, kaynak(kGorev, duzen));
        ref.kos();
        Kosu k(kGorev, kaynak(kGorev, duzen));
        k.a2_ac();
        k.kos();
        dogrula(k.s.tamam && k.s.a2.istendi && k.s.a2.durum == "gorunum_kapali" && k.port.load() == -1,
                "kalite.gorunum = kapali ve --a2-port: dinleyici açılmadı (durum " + k.s.a2.durum + ")");
        dogrula(sunucu::a2_baslatma_sayisi() == once, "görünüm kapalıyken A2 sunucusu denenmedi (kilit alınmadı)");
        dogrula(!ref.s.csv.empty() && k.s.csv == ref.s.csv && k.giden == ref.giden && k.gelen == ref.gelen,
                "görünüm kapalı: CSV ve A1 akışı --a2-port'suz koşuyla bayt bayt aynı");
        std::error_code ec;
        dogrula(!std::filesystem::exists(k.a.a2_kilit_dosyasi, ec), "görünüm kapalıyken kilit dosyası oluşmadı");
    }
    return g_hata;
}

// `--sil-hiz` (viz/calistir.sh --sim için; A3-K66 ile aynı düzenek): yalnız bekleme ekler, çıktı değişmez.
int test_sil_hiz()
{
    Kosu ref(kGorev, kaynak(kGorev, sure(4.0)));
    ref.kos();
    Kosu k(kGorev, kaynak(kGorev, sure(4.0)));
    k.a.sil_hiz = 10.0;   // 4 s sim → en az 0,4 s duvar (alt sınır: bekleme hedefe kadar sürer)
    const auto t0 = std::chrono::steady_clock::now();
    k.kos();
    const double duvar = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    std::printf("  --sil-hiz 10: 4 s sim zamanı %.3f s duvar süresinde koştu (ölçüm)\n", duvar);
    dogrula(ref.s.tamam && k.s.tamam && !ref.s.csv.empty() && k.s.csv == ref.s.csv && k.giden == ref.giden && k.gelen == ref.gelen,
            "--sil-hiz: CSV ve A1 akışı hız sınırsız koşuyla bayt bayt aynı");
    dogrula(duvar >= 0.39, "--sil-hiz 10: koşu sim zamanının onda birinden kısa sürmedi (" + std::to_string(duvar) + " s ≥ 0,39 s)");
    {
        Kosu h("scenarios/z1-hil.toml", kaynak("scenarios/z1-hil.toml", sure(2.0)));
        h.a.karsi = sunucu::Karsi::pty;
        h.a.karsi_verildi = true;
        h.a.sil_hiz = 1.0;
        h.kos();
        dogrula(!h.s.tamam && h.s.hata_turu == sunucu::HataTuru::kullanim && h.s.satirlar.empty(), "--sil-hiz HIL kipinde kullanım hatası: " + h.s.hata);
    }
    return g_hata;
}

// ------------------------------------------------------------------------------------------------ kabul 2

void a2_ozeti_yaz(const char* ad, const Kosu& k)
{
    const sunucu::A2Ozeti& o = k.s.a2;
    const SunucuSayaclari& c = o.sayac;
    std::printf("  %-22s A2 %s; sınır %llu, çağrı ort %.3f µs, en çok %.1f µs (ölçüm, dünya iş parçacığı); bağlantı %llu, "
                "gönderilen durum %llu, seyreltilen %llu, kuyrukta atılan %llu, taşma %llu, son kopma %s, yayın düşen %llu, "
                "geçersiz çağrı %llu\n",
                ad, o.durum.c_str(), static_cast<unsigned long long>(o.adim_siniri),
                o.adim_siniri != 0u ? 1e-3 * static_cast<double>(o.cagri_toplam_ns) / static_cast<double>(o.adim_siniri) : 0.0,
                1e-3 * static_cast<double>(o.cagri_en_kotu_ns), static_cast<unsigned long long>(c.baglanti_sayisi),
                static_cast<unsigned long long>(c.gonderilen_durum), static_cast<unsigned long long>(c.seyreltilen_durum),
                static_cast<unsigned long long>(c.kuyrukta_atilan_durum),
                static_cast<unsigned long long>(c.kuyruk_tasmasi + c.yayin_tasmasi_kapanis),
                kopma_nedeni_adi(c.son_kopma_nedeni), static_cast<unsigned long long>(c.yayin.dusen_durum),
                static_cast<unsigned long long>(c.yayin.gecersiz_cagri + o.reddedilen_cagri));
}

// Koşu sınır 0'da (ilk fizik adımından önce) istemci akışa gelene kadar tutulur: istemci koşunun
// tamamında bağlıdır (SIL: sim zamanı donuk; sonuç değişmemelidir, sınanan da budur).
void sinir0da_tut(Kosu& k, std::atomic<bool>& hazir)
{
    k.sinirda = [&hazir](std::uint64_t b) {
        if (b == 0u) {
            dunyada_bekle([&hazir] { return hazir.load(); });
        }
    };
}

int test_belirlenim()
{
    Kosu ref(kGorev);
    ref.kos();
    dogrula(ref.s.tamam && ref.s.gorev_durumu == "tamamlandi" && ref.s.satirlar.size() == 48000u,
            "başvuru (A2 kapalı): görev senaryosu tamam, 48 000 satır (görev " + ref.s.gorev_durumu + ")");
    const std::uint64_t n_sinir = 192000u;
    const auto ayni = [&](const char* ad, const Kosu& k) {
        dogrula(k.s.tamam && k.s.csv == ref.s.csv && k.s.csv.size() > 1000u, std::string(ad) + ": CSV başvuruyla bayt bayt aynı (" +
                                                                                  sayi(k.s.csv.size()) + " bayt)");
        dogrula(k.giden == ref.giden && k.gelen == ref.gelen && k.giden.n > 0u,
                std::string(ad) + ": A1 akışı iki yönde aynı (" + sayi(k.giden.n) + " / " + sayi(k.gelen.n) + " bayt)");
        dogrula(k.s.davranis_ozeti == ref.s.davranis_ozeti && k.s.gorev_durumu == ref.s.gorev_durumu &&
                    k.s.sayac.parti_gonderilen == ref.s.sayac.parti_gonderilen,
                std::string(ad) + ": davranış özeti, görev durumu ve parti sayısı aynı");
        dogrula(k.s.a2.durum == "dinliyor" && k.s.a2.calistirma_tanimlandi && k.s.a2.adim_siniri == n_sinir &&
                    k.s.a2.reddedilen_cagri == 0u && k.s.a2.sayac.yayin.gecersiz_cagri == 0u,
                std::string(ad) + ": A2 dinledi, tanım verildi, sınır başına bir adim_siniri (" + sayi(k.s.a2.adim_siniri) +
                    "), reddedilen çağrı 0");
        a2_ozeti_yaz(ad, k);
    };

    // (1) A2 açık, istemci yok.
    {
        Kosu k(kGorev);
        k.a2_ac();
        k.kos();
        ayni("istemci yok", k);
        dogrula(k.s.a2.sayac.baglanti_sayisi == 0u, "istemci yok: bağlantı 0");
    }
    // (2) İstemci bağlı ve okuyor.
    {
        Kosu k(kGorev);
        k.a2_ac();
        std::atomic<bool> hazir{false};
        sinir0da_tut(k, hazir);
        k.baslat();
        const bool port = k.port_bekle();
        SoketIstemci c(static_cast<std::uint16_t>(port ? k.port.load() : 1));
        const bool akis = port && akisa_getir(c);
        hazir.store(true);
        sonuna_kadar(c, k);
        k.bekle();
        dogrula(akis && c.k.say(IletiTuru::durum) >= 20u && c.k.son_veda(VedaNedeni::sim_kapaniyor) && istemci_protokol_hatasiz(c),
                "okuyan istemci: akışa geldi, " + sayi(c.k.say(IletiTuru::durum)) + " durum aldı, veda(sim_kapaniyor), protokol hatası yok");
        ayni("istemci okuyor", k);
        dogrula(k.s.a2.sayac.baglanti_sayisi == 1u && k.s.a2.sayac.protokol_hatasi == 0u, "okuyan istemci: tek oturum, sunucuda protokol hatası 0");
    }
    // (3) İstemci bağlı ama okumuyor (donuk; 3 s kuralına takılmamak için damla okuma).
    {
        Kosu k(kGorev);
        k.a2_ac();
        std::atomic<bool> hazir{false};
        sinir0da_tut(k, hazir);
        k.baslat();
        const bool port = k.port_bekle();
        SoketIstemci c(static_cast<std::uint16_t>(port ? k.port.load() : 1));
        const bool akis = port && akisa_getir(c);
        c.oku = false;
        c.damla = true;
        const std::uint64_t okunan = c.k.bayt;
        hazir.store(true);
        (void)c.bekle([&] { return k.bitti.load(); }, 120000);
        k.bekle();
        dogrula(akis, "donuk istemci: akışa geldi, sonra okumayı bıraktı (okunan " + sayi(c.k.bayt - okunan) + " bayt damla)");
        ayni("istemci okumuyor", k);
    }
    // (4) İstemci koşunun ortasında (sınır 96 000, 120 s) haber vermeden kapanıyor.
    {
        Kosu k(kGorev);
        k.a2_ac();
        std::atomic<bool> hazir{false};
        std::atomic<bool> oldur{false};
        std::atomic<bool> oldu{false};
        k.sinirda = [&](std::uint64_t b) {
            if (b == 0u) {
                dunyada_bekle([&] { return hazir.load(); });
            } else if (b == 96000u) {
                oldur.store(true);
                dunyada_bekle([&] { return oldu.load(); });   // soket kapanışı koşunun ortasına düşsün
            }
        };
        k.baslat();
        const bool port = k.port_bekle();
        SoketIstemci c(static_cast<std::uint16_t>(port ? k.port.load() : 1));
        const bool akis = port && akisa_getir(c);
        hazir.store(true);
        (void)c.bekle([&] { return oldur.load() || k.bitti.load(); }, 120000);
        const std::size_t alinan = c.k.say(IletiTuru::durum);
        c.kapat();   // veda yok: çökme / öldürme
        oldu.store(true);
        k.bekle();
        dogrula(akis && alinan >= 10u, "ölen istemci: akışa geldi, ortada kapandı (" + sayi(alinan) + " durum aldıktan sonra)");
        ayni("istemci ortada kapandı", k);
        dogrula(k.s.a2.sayac.biten_oturum == 1u && k.s.a2.sayac.son_kopma_nedeni == KopmaNedeni::karsi_kapatti,
                std::string("ölen istemci: sunucu kopmayı gördü (") + kopma_nedeni_adi(k.s.a2.sayac.son_kopma_nedeni) + ")");
    }
    return g_hata;
}

// W5-bagla-a2: görselleştirilebilir senaryo `scenarios/gorev-havuz-3b.toml` SIL'de `--a2-port` (sınama
// portu) ile başlar ve istemcinin hosgeldin'indeki manifest yolu ve özeti `havuz` kümesinindir (özet
// dosyadan yeniden hesaplanır; sunucu `ret(manifest)` vermez, el sıkışma akışa varır). `--sil-hiz` verilince
// senaryonun SIL hız alanlarının uygulanmama uyarısı hızın komut satırından geldiğini söyler (W5-K38).
int test_3b_senaryo()
{
    const char* dosya = "scenarios/gorev-havuz-3b.toml";
    Kosu k(dosya);
    k.a2_ac();
    k.a.sil_hiz = 40.0;
    k.a.csv_uret = false;
    std::atomic<bool> hazir{false};
    sinir0da_tut(k, hazir);
    k.baslat();
    const bool port = k.port_bekle();
    dogrula(port && k.dinliyor.load(), "3B senaryosu: A2 dinliyor (port " + std::to_string(k.port.load()) + ")");
    SoketIstemci c(static_cast<std::uint16_t>(port ? k.port.load() : 1));
    const bool akis = port && akisa_getir(c);
    dogrula(akis, "3B senaryosu: gerçek istemci el sıkışmayı tamamladı (manifest özeti tuttu: ret(manifest) yok)");
    if (akis) {
        const viz::HosgeldinIleti& h = c.ist.hosgeldin();
        viz::Ozet dosya_ozeti{};
        const bool oku = manifest_ozeti(h, dosya_ozeti);
        const std::filesystem::path beklenen =
            (std::filesystem::absolute(std::filesystem::path(IDA_DEPO_KOKU)) / "assets" / "havuz" / "manifest.json").lexically_normal();
        const std::u8string by = beklenen.u8string();
        dogrula(oku && std::memcmp(dosya_ozeti.data(), h.h.manifest_sha256, 32) == 0 &&
                    std::string(h.h.manifest_yolu) == std::string(by.begin(), by.end()),
                std::string("3B senaryosu: hosgeldin manifest yolu assets/havuz ve özet dosyanın SHA-256'sı: ") + h.h.manifest_yolu);
    }
    hazir.store(true);
    sunucu::kapanis_iste();
    sonuna_kadar(c, k);
    k.bekle();
    sunucu::kapanis_sifirla();
    // Uyarı metni: senaryo `sil_azami_hiz = false` (uygulanmayan alan) ile kısa iki koşu; bayraksız metin eskisi gibi.
    const auto uyari_metni = [](double hiz) {
        Kosu u(kGorev, kaynak(kGorev, [](std::string& m) {
            sure(1.0)(m);
            degistir(m, "sil_azami_hiz = true", "sil_azami_hiz = false");
        }));
        u.a.sil_hiz = hiz;
        u.a.csv_uret = false;
        u.kos();
        std::string r;
        for (const std::string& w : u.s.senaryo_uyarilari) {
            if (w.rfind("calistirma.sil_hiz_carpani", 0) == 0) {
                r = w;
            }
        }
        return r;
    };
    dogrula(uyari_metni(0.0) == "calistirma.sil_hiz_carpani/sil_azami_hiz uygulanmaz: SIL olabildiğince hızlı koşar (sonuçları değiştirmez)",
            "bayraksız: SIL hız alanı uyarı metni değişmedi");
    const std::string m40 = uyari_metni(40.0);
    dogrula(m40.find("--sil-hiz ile belirlenir (40x") != std::string::npos && m40.find("olabildiğince hızlı") == std::string::npos,
            "--sil-hiz verilince SIL hız alanı uyarısı hızın komut satırından geldiğini söyler: " + m40);
    return g_hata;
}

#ifndef _WIN32
int test_hil_donuk()
{
    // Kartsız HIL 1x (sözde uçbirim, sahte kart), 12 s gerçek zaman. İki koşu: A2 kapalı, A2 açık + donuk istemci.
    const auto hil = [](bool a2, Kosu& k) {
        k.a.karsi = sunucu::Karsi::pty;
        k.a.karsi_verildi = true;
        k.a.csv_uret = false;
        if (a2) {
            k.a2_ac();
        }
    };
    const char* dosya = "scenarios/z1-hil.toml";
    Kosu ref(dosya, kaynak(dosya, sure(12.0)));
    hil(false, ref);
    ref.kos();
    Kosu k(dosya, kaynak(dosya, sure(12.0)));
    hil(true, k);
    k.baslat();
    const bool port = k.port_bekle();
    SoketIstemci c(static_cast<std::uint16_t>(port ? k.port.load() : 1));
    const bool akis = port && akisa_getir(c);
    c.oku = false;   // donuk: okumaz (damla okuma: istemcinin kendi 3 s kuralı oturumu kapatmasın)
    c.damla = true;
    (void)c.bekle([&] { return k.bitti.load(); }, 60000);
    k.bekle();

    const auto ozet = [](const char* ad, const sunucu::Sonuc& s) {
        std::printf("  %-10s tamam %d, TF %llu, kaçırılan parti %llu, son tarih %llu, sim gecikmesi %llu, gerçek zaman kaçırma %llu, "
                    "TF geç yazım en çok %.3f ms%s\n",
                    ad, s.tamam ? 1 : 0, static_cast<unsigned long long>(s.zaman.tf_sayisi),
                    static_cast<unsigned long long>(s.sayac.parti_kacirildi), static_cast<unsigned long long>(s.sayac.son_tarih_kacirildi),
                    static_cast<unsigned long long>(s.zaman.sim_gecikmesi), static_cast<unsigned long long>(s.zaman.gercek_zaman_kacirma),
                    1e-6 * static_cast<double>(s.zaman.gec_yazim_en_cok_ns), s.hata.empty() ? "" : (", hata: " + s.hata).c_str());
    };
    ozet("A2 kapalı", ref.s);
    ozet("A2 donuk", k.s);
    a2_ozeti_yaz("HIL, donuk istemci", k);
    // İşlevsel (her ortamda): iki koşu da tamamlandı, A2 dinledi, istemci bağlandı, her sınır yayınlandı.
    dogrula(ref.s.tamam && k.s.tamam && ref.s.zaman.tf_sayisi >= 2300u && k.s.zaman.tf_sayisi >= 2300u,
            "HIL: iki koşu da tamamlandı (TF " + sayi(ref.s.zaman.tf_sayisi) + " / " + sayi(k.s.zaman.tf_sayisi) + ")");
    dogrula(akis && k.s.a2.durum == "dinliyor" && k.s.a2.sayac.baglanti_sayisi >= 1u && k.s.a2.adim_siniri == 9600u &&
                k.s.a2.reddedilen_cagri == 0u,
            "HIL: donuk istemci bağlandı; A2 her sınırı aldı (" + sayi(k.s.a2.adim_siniri) + ")");
    // Zamanlama (yalnız yerel release'te ölçüt; W4-K24, K29): sayaçlar A2 kapalı koşudan farklı değil.
    const std::uint64_t pay = 2;   // iki koşu arasındaki gürültü payı (tahmin)
    const auto fark_yok = [&](std::uint64_t a2, std::uint64_t taban) { return a2 <= taban + pay; };
    dogrula(ida::test::zamanlama(fark_yok(k.s.sayac.parti_kacirildi, ref.s.sayac.parti_kacirildi), "a2_hil_donuk.parti_kacirildi",
                                 "A2 donuk " + sayi(k.s.sayac.parti_kacirildi) + ", A2 kapalı " + sayi(ref.s.sayac.parti_kacirildi) +
                                     " (≤ taban + " + sayi(pay) + ")"),
            "kaçırılan parti A2 kapalı koşudan farklı değil (zamanlama)");
    const std::uint64_t butce_a2 = k.s.zaman.sim_gecikmesi + k.s.sayac.son_tarih_kacirildi;
    const std::uint64_t butce_ref = ref.s.zaman.sim_gecikmesi + ref.s.sayac.son_tarih_kacirildi;
    dogrula(ida::test::zamanlama(fark_yok(butce_a2, butce_ref), "a2_hil_donuk.butce_asimi",
                                 "sim gecikmesi + son tarih: A2 donuk " + sayi(butce_a2) + ", A2 kapalı " + sayi(butce_ref) +
                                     " (≤ taban + " + sayi(pay) + ")"),
            "bütçe aşımı sayaçları A2 kapalı koşudan farklı değil (zamanlama)");
    dogrula(ida::test::zamanlama(k.s.a2.cagri_en_kotu_ns < 1'000'000, "a2_hil_donuk.cagri_en_kotu",
                                 "adim_siniri en uzun " + std::to_string(1e-3 * static_cast<double>(k.s.a2.cagri_en_kotu_ns)) +
                                     " µs (< 1000 µs; fizik adımı 1250 µs)"),
            "A2 yayın çağrısı bir fizik adımından kısa (zamanlama)");
    return g_hata;
}
#endif

// ------------------------------------------------------------------------------------------------ kabul 3

int test_uctan_uca()
{
    Kosu k(kGorev);
    k.a2_ac();
    // SIL seyreltmesi (A2-K25) duvar saatinin saniyesinde en çok 120 takvimli durum bırakır. Koşu hızı
    // 40x ile sınırlanır (240 s sim ≥ 6 s duvar; yalnız bekleme, çıktı değişmez): makinenin hızından
    // bağımsız olarak koşunun bütününe yayılmış yüzlerce durum iletisi gelir.
    k.a.sil_hiz = 40.0;
    std::atomic<bool> hazir{false};
    sinir0da_tut(k, hazir);
    k.baslat();
    const bool port = k.port_bekle();
    dogrula(port && k.dinliyor.load(), "A2 dinliyor (port " + std::to_string(k.port.load()) + ")");
    SoketIstemci c(static_cast<std::uint16_t>(port ? k.port.load() : 1));
    const bool akis = port && akisa_getir(c);
    dogrula(akis, "gerçek istemci oturumu el sıkışmayı tamamladı (hazir, dosyadan hesaplanan manifest özetiyle)");
    hazir.store(true);
    sonuna_kadar(c, k);
    k.bekle();
    dogrula(k.s.tamam && k.s.gorev_durumu == "tamamlandi", "koşu tamam, görev tamamlandı");
    if (!akis) {
        return g_hata;
    }

    // ---- hosgeldin ve tanım ----
    const viz::HosgeldinIleti& h = c.ist.hosgeldin();
    viz::Ozet dosya_ozeti{};
    const bool dosya = manifest_ozeti(h, dosya_ozeti);
    const std::filesystem::path beklenen_yol =
        (std::filesystem::absolute(std::filesystem::path(IDA_DEPO_KOKU)) / "assets" / "ornek" / "manifest.json").lexically_normal();
    const std::u8string by = beklenen_yol.u8string();
    dogrula(dosya && std::memcmp(dosya_ozeti.data(), h.h.manifest_sha256, 32) == 0 &&
                std::string(h.h.manifest_yolu) == std::string(by.begin(), by.end()),
            std::string("hosgeldin: manifest yolu senaryonun kümesi ve özet dosyanın SHA-256'sı: ") + h.h.manifest_yolu);
    dogrula(h.h.sim_kipi == static_cast<std::uint8_t>(SimKipi::sil) && h.h.kademe_3b == static_cast<std::uint8_t>(Kademe::sade) &&
                h.h.kamera_sayisi == 0u && h.h.kademe_kamera == 0u && h.h.adim_ns == 1'250'000 && h.h.adim_no == 0u && h.h.t_ns == 0 &&
                h.h.calisma_nesli == 1u,
            "hosgeldin: SIL, 3B sade, kamera yok, adım 1,25 ms, sınır 0, nesil 1");
    const bool tek_govde = h.govdeler.size() == 1u;
    dogrula(tek_govde && h.govdeler[0].govde_turu == static_cast<std::uint8_t>(GovdeTuru::arac) && h.govdeler[0].varlik_indeksi == 0u &&
                h.govdeler[0].dolu_tupler == 3u && h.govdeler[0].sureklilik_no == 0u && h.govdeler[0].govde_kimligi == k.s.a2.arac_kimligi &&
                k.s.a2.arac_kimligi >= 1u,
            "hosgeldin: tek gövde (araç, varlık 0, iki tüp dolu), kimlik bağlama katmanının sayacından (" + sayi(k.s.a2.arac_kimligi) + ")");
    const viz::DalgaIleti& dalga = c.ist.dalga();
    dogrula(dalga.bilesenler.empty() && dalga.d.bilesen_sayisi == 0u && dalga.d.g == 9.82 && dalga.d.rho == 1000.0 && dalga.d.derin_su == 0u &&
                dalga.d.derinlik_m == 3.0,
            "dalga kümesi: 0 bileşen (fizikte dalga yok), g 9,82, rho 1000, derinlik 3 m (fiziğin değerleri)");
    const Ortam& o = c.ist.ortam();
    dogrula(o.ortam_turu == static_cast<std::uint8_t>(OrtamTuru::havuz) && o.statik_varlik_indeksi == 3u && o.gunes_yonu_n[2] == -1.0 &&
                o.gunes_isinimi_w_m2 == 1000.0 && c.ist.malzeme().girdiler.empty(),
            "ortam: havuz, statik varlık havuz_ornek (3), ışık senaryodan; malzeme optiği 0 girdi");
    // W5-viz-gorsel: senaryonun su optiği ve ortam ışığı (gorev-havuz.toml) telden istemciye aynen ulaşır;
    // verilmeseler sim 0 yazardı (A5-K101 "kalibre edilmedi") ve görüntü siyah, ışıksız olurdu.
    dogrula(o.beta_d_m[0] == 0.30 && o.beta_d_m[1] == 0.055 && o.beta_d_m[2] == 0.04 && o.beta_b_m[0] == 0.11 &&
                o.beta_b_m[1] == 0.065 && o.beta_b_m[2] == 0.055 && o.b_sonsuz_rgb_dogrusal[0] == 0.02 &&
                o.b_sonsuz_rgb_dogrusal[1] == 0.20 && o.b_sonsuz_rgb_dogrusal[2] == 0.26 && o.ortam_isigi_rgb_dogrusal[0] == 0.30 &&
                o.ortam_isigi_rgb_dogrusal[1] == 0.36 && o.ortam_isigi_rgb_dogrusal[2] == 0.42,
            "ortam: senaryonun su optiği (β_d, β_b, B∞) ve ortam ışığı istemciye aynen ulaştı (sıfır değil)");

    // ---- durum akışı: aynı sınırın dünya durumu ve CSV satırıyla bit düzeyinde ----
    const std::vector<const Cerceve*> durumlar = turunden(c.k, IletiTuru::durum);
    std::uint64_t sayilan = 0, poz_farkli = 0, csv_farkli = 0, csv_bakilan = 0, bicim = 0, sira_disi = 0;
    std::uint64_t ihlal_v = 0, ihlal_w = 0, olculen = 0, hareketli = 0;
    double en_kotu_v = 0.0, en_kotu_w = 0.0, yanlis_v = 0.0, en_buyuk_hiz = 0.0;
    std::uint64_t onceki_adim = 0;
    bool ilk = true;
    for (const Cerceve* f : durumlar) {
        const viz::DurumIleti d = durum_coz(*f);
        const std::uint64_t b = d.d.adim_no;
        if (b == ~std::uint64_t{0} || d.govdeler.size() != 1u || b >= k.p.size() ||
            d.d.t_ns != static_cast<std::int64_t>(b) * 1'250'000 || (d.govdeler[0].bayraklar & kGovdeGecerli) == 0u ||
            d.govdeler[0].govde_kimligi != k.s.a2.arac_kimligi) {
            ++bicim;
            continue;
        }
        sira_disi += !ilk && b < onceki_adim ? 1u : 0u;
        onceki_adim = b;
        ilk = false;
        ++sayilan;
        const GovdeDurumu& g = d.govdeler[0];
        // Poz: dünyanın sınır b'deki yer gerçeği (sinir_kancasi, fizik adımı b'den önce).
        if (!bit_ayni(g.p_n, k.p[b].data(), 3) || !bit_ayni(g.q_nb, k.q[b].data(), 4)) {
            ++poz_farkli;
        }
        // CSV: kontrol sınırlarında (b = 4n, n ≥ 1) satır n − 1'in derinliği ve yönü.
        if (b != 0u && b % 4u == 0u && b / 4u - 1u < k.s.satirlar.size()) {
            const sunucu::Satir& r = k.s.satirlar[b / 4u - 1u];
            ++csv_bakilan;
            if (std::memcmp(&r.derinlik, &g.p_n[2], sizeof(double)) != 0 || std::fabs(r.yon - yon(g.q_nb)) > 1e-12 ||
                std::fabs(r.t_s - static_cast<double>(b) * kH) > 1e-9) {
                ++csv_farkli;
            }
        }
        // v_n ve w_b: dünyanın bir sonraki ve bir önceki sınırıyla (ivme terimi ikinci farktan).
        if (b >= 1u && b + 1u < k.p.size()) {
            const std::array<double, 3>& p0 = k.p[b];
            const std::array<double, 3>& p1 = k.p[b + 1u];
            const std::array<double, 3>& pm = k.p[b - 1u];
            const double dp[3] = {p1[0] - p0[0] - g.v_n[0] * kH, p1[1] - p0[1] - g.v_n[1] * kH, p1[2] - p0[2] - g.v_n[2] * kH};
            const double ikinci[3] = {p1[0] - 2.0 * p0[0] + pm[0], p1[1] - 2.0 * p0[1] + pm[1], p1[2] - 2.0 * p0[2] + pm[2]};
            const double sinir = norm3(ikinci) + 5e-8;
            const double f_v = norm3(dp);
            ihlal_v += f_v <= sinir ? 0u : 1u;
            en_kotu_v = std::max(en_kotu_v, f_v / sinir);
            const double hiz = norm3(g.v_n);
            en_buyuk_hiz = std::max(en_buyuk_hiz, hiz);
            hareketli += hiz > 0.1 ? 1u : 0u;
            // Dişi: yönü 20°'den çok dönmüş araçta hız NED'e çevrilmeden (gövde çerçevesinde) verilseydi
            // fark |v|·h ölçeğinde olurdu. v_b = Rᵀ v_n; yatay düzlemde yön ψ ile dönüş yeter.
            const double psi = yon(g.q_nb);
            if (hiz > 0.1 && std::fabs(std::sin(psi)) > 0.34) {
                const double vb[3] = {std::cos(psi) * g.v_n[0] + std::sin(psi) * g.v_n[1], -std::sin(psi) * g.v_n[0] + std::cos(psi) * g.v_n[1],
                                      g.v_n[2]};
                const double y[3] = {p1[0] - p0[0] - vb[0] * kH, p1[1] - p0[1] - vb[1] * kH, p1[2] - p0[2] - vb[2] * kH};
                yanlis_v = std::max(yanlis_v, norm3(y) / sinir);
            }
            // w_b: q(b)* ⊗ q(b + 1) ≈ (1, ½ w_b h) (gövde çerçevesinde artış).
            const std::array<double, 4>& a = k.q[b];
            const std::array<double, 4>& n = k.q[b + 1u];
            double dq[4] = {a[0] * n[0] + a[1] * n[1] + a[2] * n[2] + a[3] * n[3], a[0] * n[1] - a[1] * n[0] - a[2] * n[3] + a[3] * n[2],
                            a[0] * n[2] + a[1] * n[3] - a[2] * n[0] - a[3] * n[1], a[0] * n[3] - a[1] * n[2] + a[2] * n[1] - a[3] * n[0]};
            const double isaret = dq[0] < 0.0 ? -1.0 : 1.0;
            const double dw[3] = {2.0 * isaret * dq[1] - g.w_b[0] * kH, 2.0 * isaret * dq[2] - g.w_b[1] * kH, 2.0 * isaret * dq[3] - g.w_b[2] * kH};
            // Açısal ivme terimi: önceki sınırla aynı yoldan ikinci fark.
            const std::array<double, 4>& m = k.q[b - 1u];
            double dqm[4] = {m[0] * a[0] + m[1] * a[1] + m[2] * a[2] + m[3] * a[3], m[0] * a[1] - m[1] * a[0] - m[2] * a[3] + m[3] * a[2],
                             m[0] * a[2] + m[1] * a[3] - m[2] * a[0] - m[3] * a[1], m[0] * a[3] - m[1] * a[2] + m[2] * a[1] - m[3] * a[0]};
            const double isaret_m = dqm[0] < 0.0 ? -1.0 : 1.0;
            const double ikinci_w[3] = {2.0 * (isaret * dq[1] - isaret_m * dqm[1]), 2.0 * (isaret * dq[2] - isaret_m * dqm[2]),
                                        2.0 * (isaret * dq[3] - isaret_m * dqm[3])};
            const double sinir_w = norm3(ikinci_w) + 2e-8;
            const double f_w = norm3(dw);
            ihlal_w += f_w <= sinir_w ? 0u : 1u;
            en_kotu_w = std::max(en_kotu_w, f_w / sinir_w);
            ++olculen;
        }
    }
    std::printf("  %zu durum iletisi (SIL seyreltmesiyle; sunucuda seyreltilen %llu), %llu geçerli, %llu CSV satırıyla karşılaştırıldı\n",
                durumlar.size(), static_cast<unsigned long long>(k.s.a2.sayac.seyreltilen_durum), static_cast<unsigned long long>(sayilan),
                static_cast<unsigned long long>(csv_bakilan));
    std::printf("  v_n: %llu ileti ölçüldü, |Δp − v_n·h| / sınır en çok %.3f; en büyük |v_n| %.3f m/s, hareketli (> 0,1 m/s) %llu ileti; "
                "dönüşümsüz hız verilseydi oran en çok %.0f\n",
                static_cast<unsigned long long>(olculen), en_kotu_v, en_buyuk_hiz, static_cast<unsigned long long>(hareketli), yanlis_v);
    std::printf("  w_b: |Δθ − w_b·h| / sınır en çok %.3f\n", en_kotu_w);
    dogrula(durumlar.size() > 100u && bicim == 0u && sira_disi == 0u,
            "durum iletileri: " + sayi(durumlar.size()) + " ileti, hepsi tek geçerli gövde ve t_ns = adim_no · 1,25 ms, adım sırası artan");
    dogrula(poz_farkli == 0u && sayilan > 100u, "araç pozu (p_n, q_nb) aynı sınırın dünya durumuyla bit düzeyinde aynı (" + sayi(sayilan) + " ileti)");
    dogrula(csv_farkli == 0u && csv_bakilan > 20u,
            "kontrol sınırlarındaki iletilerde derinlik CSV satırıyla bit düzeyinde, yön 1e-12 rad içinde aynı (" + sayi(csv_bakilan) + " satır)");
    dogrula(ihlal_v == 0u && olculen > 100u,
            "v_n: |p(b+1) − p(b) − v_n·h| ≤ bir adımın ivme terimi (ikinci farktan) + 5e-8 m, her iletide");
    dogrula(hareketli > 50u && en_buyuk_hiz > 0.25 && yanlis_v > 20.0,
            "koşu hareketli ve dönüşüm sınanabilir: " + sayi(hareketli) + " iletide |v| > 0,1 m/s; gövde çerçevesindeki hız sınırı " +
                std::to_string(static_cast<long long>(yanlis_v)) + " kat aşardı");
    dogrula(ihlal_w == 0u, "w_b: |Δθ_b − w_b·h| ≤ bir adımın açısal ivme terimi + 2e-8 rad, her iletide");

    // ---- ilk ve son ileti, kapanış ----
    if (!durumlar.empty()) {
        const viz::DurumIleti ilk_d = durum_coz(*durumlar.front());
        const viz::DurumIleti son_d = durum_coz(*durumlar.back());
        dogrula(ilk_d.d.adim_no == 0u && ilk_d.d.calisma_durumu == static_cast<std::uint8_t>(CalismaDurumu::calisiyor),
                "oturumun ilk durumu sınır 0 (tanımdaki başlangıç durumu), çalışıyor");
        dogrula(son_d.d.adim_no == 192000u && son_d.d.calisma_durumu == static_cast<std::uint8_t>(CalismaDurumu::durdu),
                "son durum: son sınır (192 000), `durdu` (calistirma_durdu) — adım " + sayi(son_d.d.adim_no));
    }
    dogrula(c.k.son_veda(VedaNedeni::sim_kapaniyor) && c.karsi_kapatti, "kapanış: veda(sim_kapaniyor), bağlantı kapandı");
    const viz::OturumSayaclari& is = c.ist.sayaclar();
    dogrula(istemci_protokol_hatasiz(c) && is.gecersiz_govde == 0u && is.eksik_govde == 0u && is.tutarsiz_durum == 0u,
            "istemci: protokol hatası 0, geçersiz gövde 0, eksik gövde 0, tutarsız durum 0 (sıra boşluğu " + sayi(is.sira_boslugu) +
                ": SIL seyreltmesi)");
    const SunucuSayaclari& sc = k.s.a2.sayac;
    dogrula(sc.baglanti_sayisi == 1u && sc.protokol_hatasi == 0u && sc.son_kopma_nedeni == KopmaNedeni::sim_kapaniyor &&
                sc.kuyruk_tasmasi == 0u && sc.yayin.dusen_durum == 0u && sc.yayin.kayip_korunan == 0u && sc.yayin.gecersiz_cagri == 0u &&
                k.s.a2.reddedilen_cagri == 0u && k.s.a2.adim_siniri == 192000u,
            "sunucu: tek oturum, protokol hatası 0, kopma sim_kapaniyor, taşma 0, geçersiz çağrı 0, 192 000 adım sınırı");
    a2_ozeti_yaz("uçtan uca", k);
    return g_hata;
}

// ------------------------------------------------------------------------------------------------ kabul 4

std::uint32_t g_no = 9100;

ida::a3::Komut calistirma_komutu(const ida::a3::AnlikDurum& a, ida::a3::KomutTuru t)
{
    ida::a3::Komut k;
    k.komut_no = ++g_no;
    k.istemci_id = 5;
    k.tur = t;
    k.beklenen_run_id = a.run_id;
    k.beklenen_durum_rev = a.durum_rev;
    return k;
}

// Olay ve durum iletilerinin sırası (çerçeve dizisinde `bas`'tan itibaren).
struct Iz {
    char tur = '?';              // 'D' durum, 'P' duraklat, 'V' devam, 'N' sim_nabzi, 'X' başka
    std::uint64_t adim = 0;
    std::uint8_t cd = 0;         // durum: calisma_durumu
    bool takvim_disi = false;
};

std::vector<Iz> iz(const Kaydedici& k, std::size_t bas, std::size_t son)
{
    std::vector<Iz> v;
    for (std::size_t i = bas; i < son && i < k.cerceveler.size(); ++i) {
        const Cerceve& c = k.cerceveler[i];
        Iz z;
        if (c.tur() == IletiTuru::durum) {
            const viz::DurumIleti d = durum_coz(c);
            z.tur = 'D';
            z.adim = d.d.adim_no;
            z.cd = d.d.calisma_durumu;
            z.takvim_disi = (d.d.bayraklar & kDurumTakvimDisi) != 0u;
        } else if (c.tur() == IletiTuru::sahne_olayi) {
            const SahneOlayi o = olay_coz(c);
            z.tur = o.tur == static_cast<std::uint16_t>(SahneOlayiTuru::duraklat) ? 'P'
                    : o.tur == static_cast<std::uint16_t>(SahneOlayiTuru::devam)  ? 'V'
                                                                                   : 'X';
            z.adim = o.adim_no;
        } else if (c.tur() == IletiTuru::sim_nabzi) {
            SimNabzi n{};
            z.tur = viz::coz_sim_nabzi(c.yuk, n) == viz::KodHata::yok ? 'N' : 'X';
            z.adim = n.adim_no;
            z.cd = n.calisma_durumu;
        } else {
            z.tur = 'X';
        }
        v.push_back(z);
    }
    return v;
}

std::size_t olay_sayisi(const Kaydedici& k, SahneOlayiTuru t)
{
    std::size_t n = 0;
    for (const Cerceve& c : k.cerceveler) {
        if (c.tur() == IletiTuru::sahne_olayi && olay_coz(c).tur == static_cast<std::uint16_t>(t)) {
            ++n;
        }
    }
    return n;
}

int test_duraklat()
{
    const double sure_s = 30.0;
    const std::uint64_t kB = 9000u;   // duraklatma sınırı: 11,25 s (ARM 10 s, AUTO 10,5 s: araç hareketli)
    Kosu ref(kGorev, kaynak(kGorev, sure(sure_s)));
    ref.kos();   // duraklatmasız, A2'siz başvuru

    Kosu k(kGorev, kaynak(kGorev, sure(sure_s)));
    k.a2_ac();
    k.a.a3_acik = true;
    k.a.a3_port = 0;
    k.a.a3_kapanis_bekleme_s = 0.0;
    std::atomic<bool> hazir{false};
    sinir0da_tut(k, hazir);
    // İstek: 0 yok, 1 adımla (1 periyot), 2 sürdür. Dünya iş parçacığı duraklatılmış turlarda uygular.
    std::atomic<int> istek{0};
    std::atomic<int> a3_kademe{-1};   // A3 anlık görüntüsündeki 3B görünüm kademesi (koşarken)
    bool duraklat_verildi = false;
    k.a.a3_sinir_kancasi = [&](std::uint64_t b, ida::a3::KomutAlicisi& al) {
        const ida::a3::AnlikDurum a = al.anlik();
        if (b > 0u) {
            a3_kademe.store(a.kademe_gorunum);
        }
        if (b == kB && !duraklat_verildi && a.durum == ida::a3::CalistirmaDurumu::kosuyor) {
            duraklat_verildi = al.al(calistirma_komutu(a, ida::a3::KomutTuru::calistirma_duraklat)).tur ==
                               ida::a3::AlimSonucu::Tur::kuyrukta;
            return;
        }
        if (a.durum != ida::a3::CalistirmaDurumu::duraklatildi) {
            return;
        }
        const int i = istek.load();
        if (i == 1) {
            ida::a3::Komut kom = calistirma_komutu(a, ida::a3::KomutTuru::calistirma_adimla);
            kom.periyot_sayisi = 1;
            if (al.al(kom).tur == ida::a3::AlimSonucu::Tur::kuyrukta) {
                istek.store(0);
            }
        } else if (i == 2) {
            if (al.al(calistirma_komutu(a, ida::a3::KomutTuru::calistirma_surdur)).tur == ida::a3::AlimSonucu::Tur::kuyrukta) {
                istek.store(0);
            }
        }
    };
    k.baslat();
    const bool port = k.port_bekle();
    SoketIstemci c(static_cast<std::uint16_t>(port ? k.port.load() : 1));
    const bool akis = port && akisa_getir(c);
    dogrula(akis, "istemci akışta (A2 ve A3 açık)");
    hazir.store(true);

    // ---- Duraklat ----
    bool tamam = akis && c.bekle([&] { return olay_sayisi(c.k, SahneOlayiTuru::duraklat) >= 1u || k.bitti.load(); }, 30000) &&
                 olay_sayisi(c.k, SahneOlayiTuru::duraklat) == 1u;
    dogrula(tamam, "`duraklat` olayı geldi");
    std::uint64_t sinir = kB;
    if (tamam) {
        const std::vector<Iz> v = iz(c.k, 0, c.k.cerceveler.size());
        // Son durum olaydan hemen önce ve duraklatma sınırında; olaydan sonra durum yok.
        std::size_t p = v.size();
        for (std::size_t i = 0; i < v.size(); ++i) {
            if (v[i].tur == 'P') {
                p = i;
            }
        }
        std::size_t son_d = p;
        for (std::size_t i = 0; i < p; ++i) {
            if (v[i].tur == 'D') {
                son_d = i;
            }
        }
        bool sonra_durum = false;
        for (std::size_t i = p + 1u; i < v.size(); ++i) {
            sonra_durum = sonra_durum || v[i].tur == 'D';
        }
        dogrula(p < v.size() && v[p].adim == kB && son_d < p && v[son_d].adim == kB && !sonra_durum,
                "duraklatma sınırı " + sayi(kB) + ": o sınırın durumu olaydan önce geldi (adım " + sayi(son_d < p ? v[son_d].adim : 0u) +
                    "), olaydan sonra durum yok");
        // Nabız sürer, durum gelmez: 2,3 s gerçek zaman (1 s aralıklı sim_nabzi; A2-K24).
        const std::size_t d0 = c.k.say(IletiTuru::durum);
        const std::size_t n0 = c.k.say(IletiTuru::sim_nabzi);
        (void)c.bekle([&] { return c.k.say(IletiTuru::sim_nabzi) >= n0 + 2u || c.karsi_kapatti; }, 6000);
        const Cerceve* sn = c.k.son(IletiTuru::sim_nabzi);
        SimNabzi nb{};
        const bool nb_tamam = sn != nullptr && viz::coz_sim_nabzi(sn->yuk, nb) == viz::KodHata::yok;
        dogrula(c.k.say(IletiTuru::sim_nabzi) >= n0 + 2u && c.k.say(IletiTuru::durum) == d0 && c.ist.asama() == viz::Asama::akis &&
                    !c.karsi_kapatti,
                "duraklatılmışken nabız sürdü (+" + sayi(c.k.say(IletiTuru::sim_nabzi) - n0) + " sim_nabzi), durum gelmedi, oturum açık");
        dogrula(nb_tamam && nb.calisma_durumu == static_cast<std::uint8_t>(CalismaDurumu::duraklatildi) && nb.adim_no == kB,
                "sim_nabzi: duraklatıldı, sınır " + sayi(nb.adim_no));
        dogrula(c.ist.calisma_durumu() == static_cast<std::uint8_t>(CalismaDurumu::duraklatildi), "istemci oturumu duraklatıldı durumunda");
    }

    // ---- Adımla ×3: her adım (1 kontrol periyodu = 4 fizik adımı) bir durum ----
    for (int adim = 1; adim <= 3 && tamam; ++adim) {
        const std::size_t bas = c.k.cerceveler.size();
        istek.store(1);
        tamam = c.bekle([&] { return olay_sayisi(c.k, SahneOlayiTuru::duraklat) >= static_cast<std::size_t>(adim) + 1u || k.bitti.load(); },
                        30000) &&
                olay_sayisi(c.k, SahneOlayiTuru::duraklat) == static_cast<std::size_t>(adim) + 1u;
        const std::vector<Iz> v = iz(c.k, bas, c.k.cerceveler.size());
        // Beklenen sıra (nabızlar hariç): devam, en az bir durum (sonuncusu yeni duraklatma sınırında), duraklat.
        std::vector<Iz> w;
        for (const Iz& z : v) {
            if (z.tur != 'N') {
                w.push_back(z);
            }
        }
        const std::uint64_t yeni = sinir + 4u;
        bool sira = w.size() >= 3u && w.front().tur == 'V' && w.front().adim == sinir && w.back().tur == 'P' && w.back().adim == yeni;
        std::size_t durum_n = 0;
        std::uint64_t onceki = sinir;
        for (std::size_t i = 1; sira && i + 1u < w.size(); ++i) {
            sira = w[i].tur == 'D' && w[i].adim > onceki && w[i].adim <= yeni &&
                   w[i].cd == static_cast<std::uint8_t>(CalismaDurumu::calisiyor);
            onceki = w[i].adim;
            ++durum_n;
        }
        dogrula(tamam && sira && durum_n >= 1u && onceki == yeni,
                "adımla " + std::to_string(adim) + ": devam(" + sayi(sinir) + ") → " + sayi(durum_n) + " durum (sonuncusu sınır " + sayi(onceki) +
                    ") → duraklat(" + sayi(yeni) + ")");
        sinir = yeni;
    }

    // ---- Sürdür: akış kaldığı sınırdan ----
    if (tamam) {
        const std::size_t bas = c.k.cerceveler.size();
        const std::size_t d0 = c.k.say(IletiTuru::durum);
        istek.store(2);
        tamam = c.bekle([&] { return c.k.say(IletiTuru::durum) >= d0 + 3u || k.bitti.load(); }, 30000) && c.k.say(IletiTuru::durum) >= d0 + 3u;
        std::vector<Iz> w;
        for (const Iz& z : iz(c.k, bas, c.k.cerceveler.size())) {
            if (z.tur != 'N') {
                w.push_back(z);
            }
        }
        const bool sira = w.size() >= 3u && w[0].tur == 'V' && w[0].adim == sinir && w[1].tur == 'D' && w[1].adim > sinir &&
                          w[1].adim <= sinir + 14u && w[2].tur == 'D' && w[2].adim > w[1].adim && w[2].adim <= w[1].adim + 14u;
        dogrula(tamam && sira,
                "sürdür: devam(" + sayi(sinir) + ") → akış kaldığı sınırdan (ilk durum sınır " + sayi(w.size() > 1u ? w[1].adim : 0u) +
                    ", takvimin sonraki adımı ≤ 14 adım)");
    }
    if (!tamam) {
        // Asılı kalmasın: koşuyu kes.
        istek.store(2);
        sunucu::kapanis_iste();
    }
    sonuna_kadar(c, k);
    k.bekle();
    sunucu::kapanis_sifirla();

    dogrula(k.s.tamam && k.s.calistirma_sonu == "bitti", "koşu tamamlandı (çalıştırma " + k.s.calistirma_sonu + ")");
    dogrula(olay_sayisi(c.k, SahneOlayiTuru::duraklat) == 4u && olay_sayisi(c.k, SahneOlayiTuru::devam) == 4u,
            "dört duraklat ve dört devam olayı (duraklat, üç adımla, sürdür)");
    dogrula(c.k.son_veda(VedaNedeni::sim_kapaniyor) && istemci_protokol_hatasiz(c) && c.ist.sayaclar().tutarsiz_durum == 0u,
            "istemci: protokol hatası yok, veda(sim_kapaniyor)");
    const std::uint64_t n_sinir = static_cast<std::uint64_t>(sure_s / kH + 0.5);
    dogrula(k.s.a2.adim_siniri == n_sinir && k.s.a2.reddedilen_cagri == 0u && k.s.a2.sayac.yayin.gecersiz_cagri == 0u &&
                k.s.a2.sayac.protokol_hatasi == 0u,
            "A2: sınır başına bir adim_siniri (" + sayi(k.s.a2.adim_siniri) + " = " + sayi(n_sinir) +
                "; duran turlarda çağrı yok), reddedilen/geçersiz çağrı 0");
    dogrula(ref.s.tamam && !ref.s.csv.empty() && k.s.csv == ref.s.csv,
            "duraklatılıp adımlanan A2'li koşunun CSV'si duraklatmasız A2'siz koşuyla bayt bayt aynı");
    dogrula(a3_kademe.load() == static_cast<int>(Kademe::sade) && k.s.a2.kademe == static_cast<std::uint8_t>(Kademe::sade),
            "A3 anlık görüntüsünde 3B görünüm kademesi A2 açıkken senaryonunki (sade = 1): " + std::to_string(a3_kademe.load()));
    a2_ozeti_yaz("duraklat", k);
    return g_hata;
}

// ------------------------------------------------------------------------------------------------ kabul 5

int test_kapanis()
{
    sunucu::kapanis_sifirla();
    // ---- (a) koşunun ortasında bağlanma ve (b) kapanış isteği ----
    {
        const std::uint64_t kBagla = 40000u;   // 50 s: araç yolda
        const std::uint64_t kKes = 80000u;     // 100 s
        Kosu k(kGorev);
        k.a2_ac();
        std::atomic<bool> tutuldu{false}, birak{false};
        k.sinirda = [&](std::uint64_t b) {
            if (b == kBagla) {
                tutuldu.store(true);
                dunyada_bekle([&] { return birak.load(); });
            } else if (b == kKes) {
                sunucu::kapanis_iste();   // sinyal işleyicisinin kurduğu bayrak (W4-sunucu-kucuk)
            }
        };
        k.baslat();
        const bool port = k.port_bekle();
        const bool orta = port && kosul_bekle([&] { return tutuldu.load() || k.bitti.load(); }, 60000) && tutuldu.load();
        SoketIstemci c(static_cast<std::uint16_t>(port ? k.port.load() : 1));
        const bool akis = orta && akisa_getir(c);
        dogrula(akis, "koşunun ortasında (sınır " + sayi(kBagla) + " tutulurken) bağlanan istemci akışa geldi");
        // Tam durum: hosgeldin'in sınırı, gövde tablosu ve ilk durum aynı sınırdan (A2-K65).
        const viz::HosgeldinIleti h = c.ist.hosgeldin();
        const bool ilk_geldi = akis && c.bekle([&] { return c.k.say(IletiTuru::durum) >= 1u; }, 5000);
        const Cerceve* ilk = c.k.ilk(IletiTuru::durum);
        const viz::DurumIleti d0 = ilk != nullptr ? durum_coz(*ilk) : viz::DurumIleti{};
        birak.store(true);
        sonuna_kadar(c, k);
        k.bekle();
        sunucu::kapanis_sifirla();
        // Sınır kBagla'nın kancasında dünya henüz o sınırı yayınlamadı: son yayınlanan sınır kBagla − 1.
        dogrula(ilk_geldi && h.h.adim_no == kBagla - 1u && h.govdeler.size() == 1u && d0.d.adim_no == h.h.adim_no &&
                    d0.govdeler.size() == 1u && d0.govdeler[0].govde_kimligi == h.govdeler[0].govde_kimligi,
                "tam durum: hosgeldin sınır " + sayi(h.h.adim_no) + ", ilk durum aynı sınırdan ve aynı gövde kimliğiyle");
        const bool poz = ilk_geldi && d0.govdeler.size() == 1u && d0.d.adim_no < k.p.size() &&
                         bit_ayni(d0.govdeler[0].p_n, k.p[d0.d.adim_no].data(), 3) && bit_ayni(d0.govdeler[0].q_nb, k.q[d0.d.adim_no].data(), 4);
        dogrula(poz && norm3(d0.govdeler[0].p_n) > 1.0,
                "tam durum: ilk durumun pozu o sınırın dünya durumuyla bit düzeyinde aynı (araç başlangıçtan uzakta)");
        dogrula(c.ist.dalga().d.bilesen_sayisi == 0u && c.ist.ortam().statik_varlik_indeksi == 3u && h.h.calisma_nesli == 1u,
                "tam durum: dalga kümesi, ortam ve nesil yeniden gönderildi");
        // (b) kapanış isteği: hata kapanışı; istemci son durumu `durdu` ile ve veda(sim_kapaniyor) görür.
        dogrula(!k.s.tamam && k.s.sinyal_kesti && k.s.hata == "sinyalle kesildi" && k.s.kapanis_daraltildi,
                "kapanış isteği: koşu sinyal sınıflamasıyla ve ortak güvenli kapanışla bitti");
        const Cerceve* son = c.k.son(IletiTuru::durum);
        const viz::DurumIleti ds = son != nullptr ? durum_coz(*son) : viz::DurumIleti{};
        dogrula(c.karsi_kapatti && c.k.son_veda(VedaNedeni::sim_kapaniyor) && istemci_protokol_hatasiz(c),
                "kapanış isteği: istemci veda(sim_kapaniyor) gördü, protokol hatası yok");
        // İstek sınır kKes'in kancasında (turun sonuna yakın) verildi: o sınır yayınlanır, fizik adımı kKes
        // koşar, sınır kKes + 1'in başında bayrak görülür ve koşu biter (o sınır yayınlanmaz).
        dogrula(son != nullptr && ds.d.calisma_durumu == static_cast<std::uint8_t>(CalismaDurumu::durdu) && ds.d.adim_no == kKes &&
                    k.s.a2.adim_siniri == kKes,
                "kapanış isteği: son durum `durdu`, son yayınlanan sınır " + sayi(ds.d.adim_no) + " (sonraki sınır yayınlanmadı)");
        dogrula(k.s.a2.sayac.son_kopma_nedeni == KopmaNedeni::sim_kapaniyor && k.s.a2.sayac.baglanti_sayisi == 1u,
                "sunucu: kopma sim_kapaniyor");
    }

    // ---- A2'siz durumlar (kısa koşu) ----
    const auto kisa = sure(10.0);
    Kosu ref(kGorev, kaynak(kGorev, kisa));
    ref.kos();
    dogrula(ref.s.tamam && ref.s.satirlar.size() == 2000u, "başvuru (A2 kapalı, 10 s): tamam");
    {
        // Port dolu (A2-K05): başka bir dinleyici aynı portu tutuyor.
        (void)ag::baslat();
        ag::Tutamac dolu = ag::gecersiz();
        std::uint16_t pt = 0;
        std::string hata;
        const bool acildi = ag::dinle(0, dolu, pt, hata);
        Kosu k(kGorev, kaynak(kGorev, kisa));
        k.a2_ac();
        k.a.a2_sinama_bos_port = false;
        k.a.a2_port = pt;
        k.kos();
        if (acildi) {
            ag::kapat(dolu);
        }
        dogrula(acildi && pt >= 1024u, "sınama: port " + std::to_string(pt) + " başka dinleyicide");
        dogrula(k.s.tamam && k.s.a2.istendi && k.s.a2.durum == "port_acilamadi" && k.s.a2.port == 0u && k.port.load() == 0 &&
                    !k.dinliyor.load() && !k.s.a2.aciklama.empty(),
                "port dolu: koşu tamam, sonuçta A2 yok: " + k.s.a2.durum + " (" + k.s.a2.aciklama + ")");
        dogrula(k.s.csv == ref.s.csv && k.giden == ref.giden && k.gelen == ref.gelen, "port dolu: CSV ve A1 akışı A2'siz koşuyla aynı");
        dogrula(!k.s.a2.calistirma_tanimlandi && k.s.a2.adim_siniri == 0u, "port dolu: yayın çağrısı yapılmadı");
    }
    {
        // Kilit başkasında (A2-K79): aynı kilit dosyasını tutan başka bir A2 sunucusu.
        SunucuDuzenegi baska;
        Kosu k(kGorev, kaynak(kGorev, kisa));
        k.a2_ac();
        k.a.a2_kilit_dosyasi = baska.kilit;
        k.kos();
        dogrula(baska.s != nullptr && baska.s->durum() == A2Durumu::dinliyor, "sınama: kilidi başka bir A2 sunucusu tutuyor");
        dogrula(k.s.tamam && k.s.a2.durum == "kilit_baskasinda" && k.s.a2.port == 0u && !k.dinliyor.load(),
                "kilit başkasında: koşu tamam, sonuçta A2 yok: " + k.s.a2.durum + " (" + k.s.a2.aciklama + ")");
        dogrula(k.s.csv == ref.s.csv && k.giden == ref.giden && k.gelen == ref.gelen, "kilit başkasında: CSV ve A1 akışı A2'siz koşuyla aynı");
        dogrula(baska.s != nullptr && baska.s->sayaclar().baglanti_sayisi == 0u && baska.s->durum() == A2Durumu::dinliyor,
                "kilit başkasında: öteki sunucu etkilenmedi");
    }
    {
        // W5-bagla-a2 K-3: A2 nesnesi var ama dinlemiyorken A3 anlık görüntüsünde görünüm kademesi 0
        // (senaryonunki, sade = 1, yalnız A2 gerçekten dinliyorken yazılır).
        (void)ag::baslat();
        ag::Tutamac dolu = ag::gecersiz();
        std::uint16_t pt = 0;
        std::string hata;
        const bool acildi = ag::dinle(0, dolu, pt, hata);
        SunucuDuzenegi baska;
        for (int durum = 0; durum < 2; ++durum) {
            Kosu k(kGorev, kaynak(kGorev, sure(2.0)));
            k.a2_ac();
            if (durum == 0) {
                k.a.a2_sinama_bos_port = false;
                k.a.a2_port = pt;
            } else {
                k.a.a2_kilit_dosyasi = baska.kilit;
            }
            k.a.a3_acik = true;
            k.a.a3_port = 0;
            k.a.a3_kapanis_bekleme_s = 0.0;
            std::atomic<int> kademe{-1};
            k.a.a3_sinir_kancasi = [&](std::uint64_t b, ida::a3::KomutAlicisi& al) {
                if (b > 0u) {
                    kademe.store(al.anlik().kademe_gorunum);
                }
            };
            k.kos();
            const std::string etiket = durum == 0 ? "port dolu" : "kilit başkasında";
            dogrula(k.s.tamam && !k.dinliyor.load(), etiket + " (A3 açık): koşu tamam, A2 dinlemiyor: " + k.s.a2.durum);
            dogrula(kademe.load() == 0, etiket + ": A3 anlık görüntüsünde 3B görünüm kademesi 0 (A2 dinlemiyor): " + std::to_string(kademe.load()));
        }
        if (acildi) {
            ag::kapat(dolu);
        }
    }
    {
        // Aralık dışı port (A2-K93): koşu başlamaz.
        Kosu k(kGorev, kaynak(kGorev, kisa));
        k.a2_ac();
        k.a.a2_sinama_bos_port = false;
        k.a.a2_port = 80;
        k.kos();
        dogrula(!k.s.tamam && k.s.hata_turu == sunucu::HataTuru::kullanim && k.s.satirlar.empty(),
                "aralık dışı A2 portu (80): koşu başlamadı (kullanım hatası): " + k.s.hata);
    }
    return g_hata;
}

}  // namespace

int main(int argc, char** argv)
{
    if (argc < 2) {
        std::printf("kullanım: ida_butunleme_a2_test <a2_varsayilan|a2_belirlenim|a2_hil_donuk|a2_uctan_uca|a2_duraklat|a2_kapanis|a2_3b_senaryo|sil_hiz>\n");
        return 2;
    }
    const std::string ad = argv[1];
    if (ad == "a2_varsayilan") {
        (void)test_varsayilan();
    } else if (ad == "a2_belirlenim") {
        (void)test_belirlenim();
#ifndef _WIN32
    } else if (ad == "a2_hil_donuk") {
        (void)test_hil_donuk();
#endif
    } else if (ad == "a2_uctan_uca") {
        (void)test_uctan_uca();
    } else if (ad == "a2_duraklat") {
        (void)test_duraklat();
    } else if (ad == "a2_kapanis") {
        (void)test_kapanis();
    } else if (ad == "a2_3b_senaryo") {
        (void)test_3b_senaryo();
    } else if (ad == "sil_hiz") {
        (void)test_sil_hiz();
    } else {
        std::printf("bilinmeyen test: %s\n", ad.c_str());
        return 2;
    }
    std::printf("%s %s: %d kalan\n", ad.c_str(), g_hata == 0 ? "GECTI" : "KALDI", g_hata);
    return g_hata == 0 ? 0 : 1;
}
