// A3 sunucusunun `ida_sim`'e bağlanması (W4-bagla-a3) — bütünleme testleri.
//
//   a3_belirlenim     kabul 2: SIL'de aynı tohumla A3 açık ve bağlı istemciler (hızlı ve hiç
//                     okumayan yavaş) varken CSV ve A1 bayt akışı (iki yön) A3 kapalı koşuyla aynı;
//                     hızlı istemcinin fizik/durum değerleri CSV'yle aynı adımlarda eşleşir
//   a3_emniyet_yolu   kabul 4 (EMNİYET): panelden acil durdurma bas/bırak, senaryo zaman
//                     çizelgesindeki aynı atamalarla aynı sınırda: CSV ve A1 akışı bayt bayt aynı
//                     (aynı köprü yolu); bastıktan sonraki sınırdan itibaren itki nötr, bıraktıktan
//                     sonra L_act boyunca eski komut dönmez (W4-K3); yavaş istemci ve dolu komut
//                     kuyruğuyla, bayat run_id'li kısıtlayanla (W4-K19) aynı sonuç
//   a3_emniyet_ret    kabul 4 (EMNİYET): revizyonsuz/bayat revizyonlu gevşetme reddi; aynı sınırda
//                     senaryo ataması ile panel isteğinin karşılaşması (A1-K85): kısıtlayan önce
//   a3_calistirma     SIL'de hız CSV ve A1 akışını değiştirmez; duraklat/sürdür/adımla (W4-K28 (1))
//                     her durumda girişte reddedilir, durum ve çıktı değişmez; durdur çalıştırmayı bitirir
//   a3_hil_olcum      kabul 5 (ölçüm): HIL 1x 60 s, sözde uçbirim, A3 açık ve 50 Hz abonelikli istemci
//   a3_hil_duraklat_ret  W4-K28 (1): HIL'de duraklatma (onaylı dahil) reddedilir; geçersizlik kurulmaz
//   a3_hazirlik_durdur  W4-K27 (4): hazırlanırken durdur → yuklendi, başlamaz
//   a3_ayirma         W4-K27 (5): dünya tarafındaki A3 hizmetinde bellek ayırma yok (operator new kancası)
#include "a3_istemci.hpp"
#include "ida/a3/komut.hpp"
#include "ida_link/tel.h"
#include "ida/sunucu/sunucu.hpp"
#include "../../sim/kopru/test/zamanlama_olcutu.hpp"   // W4-K24

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <memory>
#include <mutex>
#include <new>
#include <optional>
#include <string>
#include <thread>
#include <vector>

// W4-K27 (5): dünya tarafındaki A3 hizmeti ve sonar hesabı/boşaltması (W4-bagla-sonar sol #5)
// sırasında yapılan bellek ayırmalarını sayan kanca (yalnız bu sınama programında; MSVC'de
// değiştirilmez).
namespace {
std::atomic<std::uint64_t> g_a3_ayirma{0};
std::atomic<std::uint64_t> g_sonar_ayirma{0};
}  // namespace
void a3_ayirma_sifirla() { g_a3_ayirma.store(0); }
std::uint64_t a3_ayirma_sayisi() { return g_a3_ayirma.load(); }
void sonar_ayirma_sifirla() { g_sonar_ayirma.store(0); }
std::uint64_t sonar_ayirma_sayisi() { return g_sonar_ayirma.load(); }
#if !defined(_MSC_VER)
void* operator new(std::size_t n)
{
    if (ida::sunucu::a3_ayirma_sayilir() > 0) {
        g_a3_ayirma.fetch_add(1, std::memory_order_relaxed);
    }
    if (ida::sunucu::sonar_ayirma_sayilir() > 0) {
        g_sonar_ayirma.fetch_add(1, std::memory_order_relaxed);
    }
    if (void* p = std::malloc(n == 0u ? 1u : n)) {
        return p;
    }
    throw std::bad_alloc();
}
void* operator new[](std::size_t n) { return ::operator new(n); }
void operator delete(void* p) noexcept { std::free(p); }
void operator delete[](void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }
void operator delete[](void* p, std::size_t) noexcept { std::free(p); }
#endif

namespace {

int g_hata_a3 = 0;

void dogrula_a3(bool kosul, const std::string& ileti)
{
    std::printf("%s %s\n", kosul ? "GECTI " : "KALDI ", ileti.c_str());
    if (!kosul) {
        ++g_hata_a3;
    }
}

using ida::a3::Komut;
using ida::a3::KomutTuru;

const std::vector<std::pair<std::string, int>> kPanelKanallari = {
    {"fizik/durum", 50}, {"kart/durum", 50}, {"atis/durum", 10}, {"emniyet/girdiler", 10}, {"ariza/etkin", 2},
    {"sensor/basinc", 50}, {"sensor/dvl", 50}, {"sensor/batarya", 50}, {"baglanti/durum", 10},
    {"kart/cekirdek_sayaclari", 2}};

struct Kosu {
    ida::sunucu::Sonuc s;
    std::vector<std::uint8_t> sim_giden, sim_gelen;
    std::vector<double> pwm_notr_degil;           // b'ye göre: fiziğe giden en büyük |pwm − 1500|
    std::vector<std::uint8_t> acil;               // b'ye göre acil durdurma
};

// A1 bayt akışını ve sınır gözlemini toplar.
void gozlem_kur(ida::sunucu::Ayar& a, Kosu& k)
{
    a.a1_bayt_kancasi = [&k](bool giden, const std::uint8_t* p, std::size_t n) {
        auto& v = giden ? k.sim_giden : k.sim_gelen;
        v.insert(v.end(), p, p + n);
    };
    a.sinir_kancasi = [&k](const ida::sunucu::Ayar::SinirGozlemi& g) {
        double m = 0.0;
        for (double p : g.pwm) {
            m = std::max(m, std::fabs(p - 1500.0));
        }
        if (k.pwm_notr_degil.size() <= g.b) {
            k.pwm_notr_degil.resize(g.b + 1u, -1.0);
            k.acil.resize(g.b + 1u, 0);
        }
        k.pwm_notr_degil[g.b] = m;
        k.acil[g.b] = g.acil_durdurma ? 1u : 0u;
    };
}

std::uint64_t kopru_lact() { return 8; }   // L_act / h (A1 §3.9)

bool notr_disi(const Kosu& k, std::uint64_t b)
{
    return b < k.pwm_notr_degil.size() && k.pwm_notr_degil[b] > 0.0;
}

std::size_t ilk_fark(const std::vector<std::uint8_t>& x, const std::vector<std::uint8_t>& y)
{
    std::size_t i = 0;
    while (i < x.size() && i < y.size() && x[i] == y[i]) {
        ++i;
    }
    return i;
}

void esitlik(const Kosu& a, const Kosu& b, const std::string& ad)
{
    dogrula_a3(a.s.tamam && b.s.tamam, ad + ": iki koşu tamam (" + a.s.hata + "|" + b.s.hata + ")");
    dogrula_a3(!a.s.csv.empty() && a.s.csv == b.s.csv, ad + ": CSV bayt bayt aynı (" + std::to_string(a.s.csv.size()) + " B)");
    dogrula_a3(!a.sim_giden.empty() && a.sim_giden == b.sim_giden,
               ad + ": sim → karşı taraf A1 akışı aynı (" + std::to_string(a.sim_giden.size()) + " B, ilk fark " +
                   std::to_string(ilk_fark(a.sim_giden, b.sim_giden)) + ")");
    dogrula_a3(!a.sim_gelen.empty() && a.sim_gelen == b.sim_gelen,
               ad + ": karşı taraf → sim A1 akışı aynı (" + std::to_string(a.sim_gelen.size()) + " B, ilk fark " +
                   std::to_string(ilk_fark(a.sim_gelen, b.sim_gelen)) + ")");
}

// A3 açık koşunun istemcileri: hızlı (sürekli okur) ve yavaş (abone olur, hiç okumaz).
struct Istemciler {
    std::atomic<bool> hazir{false};
    std::atomic<bool> dur{false};
    std::atomic<std::uint16_t> port{0};
    std::string jeton;
    std::mutex m;
    std::unique_ptr<ida::test::a3::Istemci> hizli, yavas;
    std::thread is;
    bool yavas_var = true;
    bool baglandi = false;

    void baslat(std::uint16_t p, const std::string& j)
    {
        port = p;
        jeton = j;
        is = std::thread([this] {
            hizli = std::make_unique<ida::test::a3::Istemci>();
            const bool h = hizli->baglan(port, jeton) && hizli->abone(kPanelKanallari);
            bool y = true;
            if (yavas_var) {
                yavas = std::make_unique<ida::test::a3::Istemci>();
                y = yavas->baglan(port, jeton) && yavas->abone(kPanelKanallari);
            }
            (void)hizli->oku(100);   // abonelik yanıtları
            {
                std::lock_guard<std::mutex> k(m);
                baglandi = h && y;
            }
            hazir = true;
            while (!dur && hizli->oku(20)) {
            }
            (void)hizli->oku(50);
        });
    }
    void bitir()
    {
        dur = true;
        if (is.joinable()) {
            is.join();
        }
    }
    // Dünya iş parçacığı: istemciler bağlanana dek (en çok 5 s) bekler. SIL kilitli adımdır: bu
    // bekleme bayt akışını değiştirmez.
    void bekle()
    {
        const auto son = std::chrono::steady_clock::now() + std::chrono::seconds(5);
        while (!hazir && std::chrono::steady_clock::now() < son) {
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
    }
};

Kosu kos(ida::sunucu::Ayar a, bool a3, Istemciler* ist,
         std::function<void(std::uint64_t, ida::a3::KomutAlicisi&)> kanca = {})
{
    Kosu k;
    gozlem_kur(a, k);
    a.csv_uret = true;
    if (a3) {
        a.a3_acik = true;
        a.a3_port = 0;
        a.a3_kapanis_bekleme_s = 0.1;
        if (ist != nullptr) {
            a.a3_hazir = [ist](std::uint16_t p, const std::string& j) { ist->baslat(p, j); };
        }
        a.a3_sinir_kancasi = [ist, kanca](std::uint64_t b, ida::a3::KomutAlicisi& al) {
            if (b == 0 && ist != nullptr) {
                ist->bekle();
            }
            if (kanca) {
                kanca(b, al);
            }
        };
    }
    k.s = ida::sunucu::kos(a);
    if (ist != nullptr) {
        ist->bitir();
    }
    return k;
}

ida::sunucu::Ayar z1_dosya()
{
    ida::sunucu::Ayar a;
    a.senaryo_dosyasi = "scenarios/z1.toml";
    a.depo_koku = IDA_DEPO_KOKU;
    a.gunluk = 0;
    return a;
}

ida::sunucu::Ayar z1_yerlesik(double sure)
{
    ida::sunucu::Ayar a;
    a.gunluk = 0;
    a.sure_s = sure;
    a.sure_verildi = true;
    return a;
}

// ---- Komut kurucuları (kitaplığın eşzamanlı denetiminden geçmiş biçimde) ----

std::uint32_t g_komut_no = 1000;

Komut emniyet(ida::a3::AnlikDurum a, ida::a3::EmniyetGirdisi g, int tup, ida::a3::GirdiDurumu hedef, bool kisitlayan)
{
    Komut k;
    k.komut_no = ++g_komut_no;
    k.istemci_id = 7;
    k.tur = KomutTuru::emniyet_degistir;
    k.beklenen_run_id = a.run_id;
    k.girdi = g;
    k.tup = static_cast<std::int8_t>(tup);
    k.hedef_durum = hedef;
    k.kisitlayan = kisitlayan;
    if (!kisitlayan) {
        k.beklenen_emniyet_rev = a.emniyet_rev;
    }
    return k;
}

Komut yi(ida::a3::AnlikDurum a, const std::string& ad, ida::a3::KomutSinifi sinif, const std::string& par)
{
    Komut k;
    k.komut_no = ++g_komut_no;
    k.istemci_id = 7;
    k.tur = KomutTuru::yer_istasyonu_komut;
    k.beklenen_run_id = a.run_id;
    k.yi_komut = ad;
    k.yi_sinif = sinif;
    k.yi_parametreler_json = par;
    k.kisitlayan = sinif == ida::a3::KomutSinifi::daraltici;
    if (sinif == ida::a3::KomutSinifi::genisletici) {
        k.beklenen_emniyet_rev = a.emniyet_rev;
    }
    return k;
}

Komut calistirma(ida::a3::AnlikDurum a, KomutTuru t)
{
    Komut k;
    k.komut_no = ++g_komut_no;
    k.istemci_id = 7;
    k.tur = t;
    k.beklenen_run_id = a.run_id;
    k.beklenen_durum_rev = a.durum_rev;
    return k;
}

const ida::sunucu::OlaySonucu* a3_sonucu(const ida::sunucu::Sonuc& s, std::uint32_t no)
{
    const std::string on = "a3/" + std::to_string(no) + "/";
    for (const auto& o : s.olay_sonuclari) {
        if (o.kaynak == "a3" && o.kimlik.rfind(on, 0) == 0) {
            return &o;
        }
    }
    return nullptr;
}

bool notr_araligi(const Kosu& k, std::uint64_t ilk, std::uint64_t son)
{
    for (std::uint64_t b = ilk; b < son && b < k.pwm_notr_degil.size(); ++b) {
        if (k.pwm_notr_degil[b] != 0.0) {
            std::printf("  sınır %llu: nötr dışı (%.1f µs)\n", static_cast<unsigned long long>(b), k.pwm_notr_degil[b]);
            return false;
        }
    }
    return son <= k.pwm_notr_degil.size();
}

// ---- Testler ----

int test_a3_belirlenim()
{
    const Kosu kapali = kos(z1_dosya(), false, nullptr);
    Istemciler ist;
    const Kosu acik = kos(z1_dosya(), true, &ist);
    esitlik(kapali, acik, "A3 açık, hızlı + yavaş istemci");
    dogrula_a3(ist.baglandi && ist.hizli != nullptr, "iki istemci bağlandı ve abone oldu");
    if (ist.hizli == nullptr) {
        return 1;
    }
    const auto& h = *ist.hizli;
    std::printf("  hızlı istemci: fizik/durum %llu, kart/durum %llu, sensor/basinc %llu, emniyet/girdiler %llu, olay %llu\n",
                static_cast<unsigned long long>(h.cerceve_sayisi.count(10) ? h.cerceve_sayisi.at(10) : 0),
                static_cast<unsigned long long>(h.cerceve_sayisi.count(11) ? h.cerceve_sayisi.at(11) : 0),
                static_cast<unsigned long long>(h.cerceve_sayisi.count(22) ? h.cerceve_sayisi.at(22) : 0),
                static_cast<unsigned long long>(h.cerceve_sayisi.count(14) ? h.cerceve_sayisi.at(14) : 0),
                static_cast<unsigned long long>(h.cerceve_sayisi.count(3) ? h.cerceve_sayisi.at(3) : 0));
    static constexpr std::uint16_t kKanallar[] = {1, 2, 3, 10, 11, 13, 14, 22, 24, 25, 52};
    for (const std::uint16_t kanal : kKanallar) {
        dogrula_a3(h.cerceve_sayisi.count(kanal) != 0u, "hızlı istemci kanal " + std::to_string(kanal) + " aldı");
    }
    // fizik/durum değerleri CSV'yle aynı adımlarda (A3-K26: t_ns = adim_no · h; derinlik %.6f).
    std::size_t eslesen = 0;
    std::size_t denenen = 0;
    for (const auto& f : h.fizik) {
        if (f.adim_no % 4u != 0u || f.adim_no == 0u) {
            continue;
        }
        ++denenen;
        const std::size_t satir = static_cast<std::size_t>(f.adim_no / 4u) - 1u;   // CSV satırları b = 4, 8, …
        if (satir < acik.s.satirlar.size() && std::fabs(acik.s.satirlar[satir].t_s - static_cast<double>(f.adim_no) * 1.25e-3) < 1e-9 &&
            std::fabs(acik.s.satirlar[satir].derinlik - f.derinlik) < 1e-12 &&
            f.t_ns == static_cast<std::int64_t>(f.adim_no) * 1'250'000) {
            ++eslesen;
        }
    }
    dogrula_a3(denenen >= 5u && eslesen == denenen,
               "fizik/durum derinliği CSV satırıyla aynı adımda aynı (" + std::to_string(eslesen) + "/" + std::to_string(denenen) + ")");
    const bool kosuyor = std::find(h.olay_turleri.begin(), h.olay_turleri.end(), std::uint16_t{1}) != h.olay_turleri.end();
    dogrula_a3(kosuyor, "calistirma_durumu_degisti olayı geldi");
    dogrula_a3(acik.s.calistirma_sonu == "bitti", "çalıştırma bitti (" + acik.s.calistirma_sonu + ")");
    dogrula_a3(acik.s.a3_halka_tasma == 0u, "dünya → A3 halkasında düşme yok");
    std::printf("  ÖLÇÜM (bu Mac, SIL olabildiğince hızlı, %s): yayın çağrısı %llu, tek çağrı en çok %.1f µs, sınır başına en çok %.1f µs\n",
                "2026-11-24", static_cast<unsigned long long>(acik.s.a3_yayin_cagrisi),
                1e-3 * static_cast<double>(acik.s.a3_cagri_en_kotu_ns), 1e-3 * static_cast<double>(acik.s.a3_sinir_en_kotu_ns));
    return 0;
}

int test_a3_emniyet_yolu()
{
    // (a) Senaryo: yerleşik z1, 20 s'de acil durdurma basılır, 25 s'de kalkar (zaman çizelgesi).
    ida::sunucu::Ayar sa = z1_yerlesik(30.0);
    sa.acil_durdurma_s = 20.0;
    sa.acil_kalkar_s = 25.0;
    sa.acil_verildi = sa.acil_kalk_verildi = true;
    const Kosu senaryo = kos(sa, false, nullptr);
    constexpr std::uint64_t kBas = 16000;   // 20 s
    constexpr std::uint64_t kKalk = 20000;  // 25 s
    std::vector<std::uint32_t> komutlar;
    // (b) Panel: aynı sınırlarda A3 komut alıcısından (kitaplığın denetiminden geçmiş biçimde).
    auto panel = [&komutlar](bool dolu_kuyruk) {
        return [&komutlar, dolu_kuyruk](std::uint64_t b, ida::a3::KomutAlicisi& al) {
            if (b == kBas) {
                const ida::a3::AnlikDurum a = al.anlik();
                if (dolu_kuyruk) {
                    // Komut kuyruğu doluyken (A3-K59): köprüye dokunmayan 256 komut (karşılığı olmayan
                    // `manual`), 257.si dolu; kısıtlayan ayrı yuvadan girer.
                    int kuyrukta = 0;
                    for (int i = 0; i < 256; ++i) {
                        kuyrukta += al.al(yi(a, "manual", ida::a3::KomutSinifi::notr, "{\"axes\":[0,0,0,0,0,0]}")).tur ==
                                            ida::a3::AlimSonucu::Tur::kuyrukta ? 1 : 0;
                    }
                    const auto d = al.al(yi(a, "manual", ida::a3::KomutSinifi::notr, "{\"axes\":[0,0,0,0,0,0]}"));
                    dogrula_a3(kuyrukta == 256 && d.tur == ida::a3::AlimSonucu::Tur::dolu, "genel kuyruk 256'da doldu");
                }
                Komut k = emniyet(a, ida::a3::EmniyetGirdisi::acil_durdurma, -1, ida::a3::GirdiDurumu::basili, true);
                if (dolu_kuyruk) {
                    k.beklenen_run_id = "20000101-000000-000000";   // W4-K19: bayat
                    k.run_id_uyusmazligi = true;
                }
                komutlar.push_back(k.komut_no);
                dogrula_a3(al.al(k).tur == ida::a3::AlimSonucu::Tur::kuyrukta, "acil durdurma bas kuyrukta");
            }
            if (b == kKalk) {
                const ida::a3::AnlikDurum a = al.anlik();
                const Komut k = emniyet(a, ida::a3::EmniyetGirdisi::acil_durdurma, -1, ida::a3::GirdiDurumu::serbest, false);
                komutlar.push_back(k.komut_no);
                dogrula_a3(al.al(k).tur == ida::a3::AlimSonucu::Tur::kuyrukta, "acil durdurma bırak (gevşeten, güncel revizyon) kuyrukta");
            }
        };
    };
    const Kosu p1 = kos(z1_yerlesik(30.0), true, nullptr, panel(false));
    esitlik(senaryo, p1, "panel ↔ senaryo, aynı sınır");
    Istemciler ist;
    ist.yavas_var = true;
    const Kosu p2 = kos(z1_yerlesik(30.0), true, &ist, panel(true));
    esitlik(senaryo, p2, "panel (yavaş istemci, dolu kuyruk, bayat run_id) ↔ senaryo");
    for (const Kosu* k : {&p1, &p2}) {
        dogrula_a3(notr_araligi(*k, kBas, kKalk + kopru_lact()), "bastıktan sonraki sınırdan bırakmadan L_act sonrasına dek itki nötr");
        dogrula_a3(k->acil.size() > kBas && k->acil[kBas] == 1u && k->acil[kBas - 1u] == 0u, "acil durdurma tam istenen sınırda");
    }
    dogrula_a3(notr_disi(p1, kBas - 1u) && notr_disi(p2, kBas - 1u), "ön koşul: basmadan önce nötr dışı komut yürürlükte");
    dogrula_a3(komutlar.size() == 4u, "dört panel komutu");
    if (komutlar.size() == 4u) {
        const auto* a = a3_sonucu(p1.s, komutlar[0]);
        const auto* b = a3_sonucu(p1.s, komutlar[1]);
        dogrula_a3(a != nullptr && a->sonuc == "uygulandi" && a->t_ns == 20'000'000'000, "bas: uygulandı, 20 s");
        dogrula_a3(b != nullptr && b->sonuc == "uygulandi" && b->t_ns == 25'000'000'000, "bırak: uygulandı, 25 s");
        const auto* c = a3_sonucu(p2.s, komutlar[2]);
        dogrula_a3(c != nullptr && c->sonuc == "uygulandi", "bayat run_id'li kısıtlayan uygulandı (W4-K19)");
    }
    std::size_t op = 0;
    for (const auto& e : p1.s.emniyet_degisimleri) {
        op += e.kaynak == "operator" ? 1u : 0u;
    }
    dogrula_a3(op == 2u, "iki değişim kaynağı operatör");
    return 0;
}

int test_a3_emniyet_ret()
{
    // Yerleşik z1: 5 s'de acil durdurma basılır, 6 s'de kalkar (senaryo).
    ida::sunucu::Ayar a = z1_yerlesik(8.0);
    a.acil_durdurma_s = 5.0;
    a.acil_kalkar_s = 6.0;
    a.acil_verildi = a.acil_kalk_verildi = true;
    std::uint32_t no_hakim = 0, no_bayat_run = 0, no_kisit_karsi = 0;
    const Kosu k = kos(a, true, nullptr, [&](std::uint64_t b, ida::a3::KomutAlicisi& al) {
        const ida::a3::AnlikDurum d = al.anlik();
        if (b == 4000) {
            // Senaryonun kısıtlaması (acil bas) ile aynı sınırda panelin gevşetmesi: kisitlama_hakim.
            const Komut g = emniyet(d, ida::a3::EmniyetGirdisi::silahlandirma_fisi, 0, ida::a3::GirdiDurumu::takili, false);
            no_hakim = g.komut_no;
            dogrula_a3(al.al(g).tur == ida::a3::AlimSonucu::Tur::kuyrukta, "gevşeten (güncel revizyon) kuyrukta");
        }
        if (b == 4400) {
            Komut g = emniyet(d, ida::a3::EmniyetGirdisi::acil_durdurma, -1, ida::a3::GirdiDurumu::serbest, false);
            g.beklenen_emniyet_rev = d.emniyet_rev - 1u;
            const auto r1 = al.al(g);
            dogrula_a3(r1.tur == ida::a3::AlimSonucu::Tur::ret && r1.hata_kodu == "emniyet_rev_uyusmuyor",
                       "bayat revizyonlu gevşetme reddi (A3-K118)");
            g.beklenen_emniyet_rev.reset();
            const auto r2 = al.al(g);
            dogrula_a3(r2.tur == ida::a3::AlimSonucu::Tur::ret && r2.hata_kodu == "emniyet_rev_uyusmuyor",
                       "revizyonsuz gevşetme reddi");
            g.beklenen_emniyet_rev = d.emniyet_rev;
            g.beklenen_run_id = "20000101-000000-000000";
            const auto r3 = al.al(g);
            dogrula_a3(r3.tur == ida::a3::AlimSonucu::Tur::ret && r3.hata_kodu == "beklenen_durum_uyusmuyor",
                       "bayat run_id'li gevşetme reddi (A3-K54)");
            Komut kk = emniyet(d, ida::a3::EmniyetGirdisi::emniyet_pimi, 1, ida::a3::GirdiDurumu::takili, true);
            kk.beklenen_run_id = "20000101-000000-000000";
            kk.run_id_uyusmazligi = true;
            no_bayat_run = kk.komut_no;
            dogrula_a3(al.al(kk).tur == ida::a3::AlimSonucu::Tur::kuyrukta, "bayat run_id'li kısıtlayan kuyrukta (W4-K19)");
        }
        if (b == 4800) {
            // Senaryonun gevşetmesi (acil kalk) ile aynı sınırda panelin kısıtlaması: senaryo reddedilir.
            const Komut kk = emniyet(d, ida::a3::EmniyetGirdisi::silahlandirma_fisi, 1, ida::a3::GirdiDurumu::cikarilmis, true);
            no_kisit_karsi = kk.komut_no;
            dogrula_a3(al.al(kk).tur == ida::a3::AlimSonucu::Tur::kuyrukta, "kısıtlayan kuyrukta");
        }
    });
    dogrula_a3(k.s.tamam, "koşu tamam " + k.s.hata);
    dogrula_a3(notr_disi(k, 3999), "ön koşul: senaryonun basmasından önce nötr dışı komut yürürlükte");
    const auto* h = a3_sonucu(k.s, no_hakim);
    dogrula_a3(h != nullptr && h->sonuc == "reddedildi" && h->neden == "kisitlama_hakim",
               "senaryonun kısıtlamasıyla aynı sınırdaki gevşetme: kisitlama_hakim (" + (h ? h->neden : std::string("yok")) + ")");
    const auto* r = a3_sonucu(k.s, no_bayat_run);
    dogrula_a3(r != nullptr && r->sonuc == "uygulandi" && r->t_ns == 5'500'000'000, "bayat run_id'li kısıtlayan 5,5 s'de uygulandı");
    const auto* c = a3_sonucu(k.s, no_kisit_karsi);
    dogrula_a3(c != nullptr && c->sonuc == "uygulandi", "panelin kısıtlaması uygulandı");
    dogrula_a3(k.s.gevsetme_retleri.size() == 1u && k.s.gevsetme_retleri[0].t_ns == 6'000'000'000,
               "senaryonun gevşetmesi panelin kısıtlamasıyla aynı sınırda reddedildi (A1-K85)");
    dogrula_a3(notr_araligi(k, 4000, 6400), "acil durdurma kalkmadı: itki nötr sürdü");
    return 0;
}

// W4-K28 (1): duraklatma bu dilimde kapalı. `duraklat`, `surdur`, `adimla` girişte `gecersiz_durum`
// ("bu sürümde desteklenmiyor") alır; durum, durum revizyonu ve çıktı (CSV, A1 akışı) değişmez.
bool duraklatma_reddi(const ida::a3::AlimSonucu& r)
{
    return r.tur == ida::a3::AlimSonucu::Tur::ret && r.hata_kodu == "gecersiz_durum" &&
           r.ileti.find("bu sürümde desteklenmiyor") != std::string::npos;
}

// Her durumda (güncel ve bayat durum revizyonuyla; HIL'de onaylı duraklatma dahil) üç isteği verir;
// hepsi reddedilmeli, anlık görüntünün durumu ve revizyonu değişmemeli.
bool duraklatma_dene(ida::a3::KomutAlicisi& al)
{
    const ida::a3::AnlikDurum d = al.anlik();
    bool hepsi = true;
    for (const KomutTuru t : {KomutTuru::calistirma_duraklat, KomutTuru::calistirma_surdur, KomutTuru::calistirma_adimla}) {
        for (int bayat = 0; bayat < 2; ++bayat) {
            Komut k = calistirma(d, t);
            k.gecersizligi_onayla = true;
            k.periyot_sayisi = 3;
            if (bayat != 0) {
                k.beklenen_durum_rev = d.durum_rev + 7u;
            }
            hepsi = hepsi && duraklatma_reddi(al.al(k));
        }
    }
    const ida::a3::AnlikDurum s = al.anlik();
    return hepsi && s.durum == d.durum && s.durum_rev == d.durum_rev;
}

int test_a3_calistirma()
{
    const Kosu taban = kos(z1_yerlesik(12.0), false, nullptr);
    // 2,5 s: duraklat/sürdür/adımla reddedilir; hız 20; 8 s'de hız 0. Görülen her durum kaydedilir.
    std::vector<std::uint32_t> nolar;
    int red_tamam = 0, red_deneme = 0;
    bool duraklatildi_goruldu = false;
    auto ver = [&nolar](ida::a3::KomutAlicisi& al, Komut k, const char* ne) {
        nolar.push_back(k.komut_no);
        dogrula_a3(al.al(k).tur == ida::a3::AlimSonucu::Tur::kuyrukta, std::string(ne) + " kuyrukta");
    };
    const Kosu k = kos(z1_yerlesik(12.0), true, nullptr, [&](std::uint64_t b, ida::a3::KomutAlicisi& al) {
        const ida::a3::AnlikDurum d = al.anlik();
        duraklatildi_goruldu = duraklatildi_goruldu || d.durum == ida::a3::CalistirmaDurumu::duraklatildi;
        if (b == 0 || b == 2000 || b == 2004 || b == 6400) {
            ++red_deneme;
            red_tamam += duraklatma_dene(al) ? 1 : 0;
        }
        if (b == 2000 && d.durum == ida::a3::CalistirmaDurumu::kosuyor) {
            Komut h = calistirma(d, KomutTuru::calistirma_hiz);
            h.carpan = 20.0;
            ver(al, h, "hız 20");
        } else if (b == 6400 && d.durum == ida::a3::CalistirmaDurumu::kosuyor) {
            Komut h = calistirma(d, KomutTuru::calistirma_hiz);
            h.carpan = 0.0;
            ver(al, h, "hız 0");
        }
    });
    esitlik(taban, k, "reddedilen duraklat/sürdür/adımla ve hız");
    dogrula_a3(red_deneme >= 4 && red_tamam == red_deneme,
               "W4-K28 (1): duraklat/sürdür/adımla her denemede gecersiz_durum ('bu sürümde desteklenmiyor'); durum ve "
               "durum_rev değişmedi (" + std::to_string(red_tamam) + "/" + std::to_string(red_deneme) + ")");
    dogrula_a3(!duraklatildi_goruldu, "W4-K28 (1): duraklatildi durumu hiç görülmedi");
    dogrula_a3(nolar.size() == 2u, "iki hız komutu");
    bool hepsi = nolar.size() == 2u;
    for (std::uint32_t no : nolar) {
        const auto* o = a3_sonucu(k.s, no);
        hepsi = hepsi && o != nullptr && o->sonuc == "uygulandi";
    }
    dogrula_a3(hepsi, "hız komutları uygulandı");
    std::size_t a3_kaydi = 0;
    for (const auto& o : k.s.olay_sonuclari) {
        a3_kaydi += o.kaynak == "a3" ? 1u : 0u;
    }
    dogrula_a3(a3_kaydi == 2u, "reddedilen duraklatma istekleri dünyaya ulaşmadı (A3 kaydı " + std::to_string(a3_kaydi) + ")");
    // Durdur: çalıştırma biter (A3 Tablo 4.7a: kosuyor → durduruldu).
    const Kosu d = kos(z1_yerlesik(12.0), true, nullptr, [](std::uint64_t b, ida::a3::KomutAlicisi& al) {
        if (b == 4000) {
            (void)al.al(calistirma(al.anlik(), KomutTuru::calistirma_durdur));
        }
    });
    dogrula_a3(d.s.tamam && d.s.calistirma_sonu == "durduruldu" && !d.s.satirlar.empty() &&
                   d.s.satirlar.back().t_s < 5.1,
               "durdur çalıştırmayı 5 s'de bitirdi (" + d.s.calistirma_sonu + ")");
    return 0;
}

int test_a3_hil_olcum()
{
    ida::sunucu::Ayar a;
    a.gunluk = 1;
    a.kip = ida::sunucu::Kip::hil;
    a.karsi = ida::sunucu::Karsi::pty;
    a.sure_s = 60.0;
    Istemciler ist;
    ist.yavas_var = false;
    // HIL'de istemci beklenmez (duvar saatine kilitli): bağlantı arka planda kurulur.
    a.a3_acik = true;
    a.a3_port = 0;
    a.csv_uret = false;
    a.a3_hazir = [&ist](std::uint16_t p, const std::string& j) { ist.baslat(p, j); };
    // W4-K28 (1): duraklatma bu dilimde kapalı (girişte gecersiz_durum); HIL'de hız kipte geçersiz.
    bool denendi = false;
    a.a3_sinir_kancasi = [&denendi](std::uint64_t b, ida::a3::KomutAlicisi& al) {
        if (b == 800 && !denendi) {
            denendi = true;
            const auto r = al.al(calistirma(al.anlik(), KomutTuru::calistirma_duraklat));
            dogrula_a3(duraklatma_reddi(r), "HIL'de duraklatma: gecersiz_durum (W4-K28 (1))");
            Komut h = calistirma(al.anlik(), KomutTuru::calistirma_hiz);
            h.carpan = 2.0;
            const auto r2 = al.al(h);
            dogrula_a3(r2.tur == ida::a3::AlimSonucu::Tur::ret && r2.hata_kodu == "kipte_gecersiz", "HIL'de hız: kipte_gecersiz");
        }
    };
    a3_ayirma_sifirla();
    const ida::sunucu::Sonuc s = ida::sunucu::kos(a);
    const std::uint64_t ayirma = a3_ayirma_sayisi();
    ist.bitir();
    dogrula_a3(s.tamam, "HIL 1x koşusu tamam " + s.hata);
#if !defined(_MSC_VER)
    dogrula_a3(ayirma == 0u, "W4-K27 (5): dünya tarafındaki A3 hizmetinde bellek ayırma yok (" + std::to_string(ayirma) + ")");
#else
    (void)ayirma;
#endif
    const ida::sunucu::ZamanOlcumu& z = s.zaman;
    dogrula_a3(ist.hizli != nullptr, "istemci oluşturuldu");
    if (ist.hizli == nullptr) {
        return 1;
    }
    const auto& h = *ist.hizli;
    const std::uint64_t fizik = h.cerceve_sayisi.count(10) ? h.cerceve_sayisi.at(10) : 0u;
    std::printf("  ÖLÇÜM (bu Mac, 2026-11-24, HIL 1x, sözde uçbirim, 60 s, A3 açık, bir istemci 50 Hz): TF %llu; "
                "gerçek zaman kaçırma %llu (sim gecikmesi %llu, kaçırılan parti %llu, son tarih %llu); "
                "periyot en çok %.3f ms, TF geç yazım en çok %.3f ms\n",
                static_cast<unsigned long long>(z.tf_sayisi), static_cast<unsigned long long>(z.gercek_zaman_kacirma),
                static_cast<unsigned long long>(z.sim_gecikmesi), static_cast<unsigned long long>(s.sayac.parti_kacirildi),
                static_cast<unsigned long long>(s.sayac.son_tarih_kacirildi), 1e-6 * static_cast<double>(z.periyot_en_cok_ns),
                1e-6 * static_cast<double>(z.gec_yazim_en_cok_ns));
    std::printf("  ÖLÇÜM: yayın çağrısı %llu; dünya iş parçacığında tek yayın çağrısının en uzun süresi %.1f µs, dünya "
                "tarafındaki bütün A3 hizmetinin (çekme, karar, hazırlık, yayın) sınır başına en uzun süresi %.1f µs; "
                "halka taşması %llu; istemcinin aldığı fizik/durum %llu, "
                "kart/durum %llu, olay %llu\n",
                static_cast<unsigned long long>(s.a3_yayin_cagrisi), 1e-3 * static_cast<double>(s.a3_cagri_en_kotu_ns),
                1e-3 * static_cast<double>(s.a3_sinir_en_kotu_ns), static_cast<unsigned long long>(s.a3_halka_tasma),
                static_cast<unsigned long long>(fizik),
                static_cast<unsigned long long>(h.cerceve_sayisi.count(11) ? h.cerceve_sayisi.at(11) : 0u),
                static_cast<unsigned long long>(h.cerceve_sayisi.count(3) ? h.cerceve_sayisi.at(3) : 0u));
    std::printf("  hil döngü %s", s.evre_ozeti.c_str());
    dogrula_a3(ist.baglandi, "istemci bağlandı");
    dogrula_a3(z.tf_sayisi >= 11'990u, "60 s'de ≈ 12 000 TF (" + std::to_string(z.tf_sayisi) + ")");
    dogrula_a3(s.a3_halka_tasma == 0u, "dünya → A3 halkasında düşme yok");
    dogrula_a3(s.calistirma_sonu == "bitti", "çalıştırma bitti");
    // ZAMANLAMA (W4-K24): yayın tiki duvar saatine bağlıdır (50 Hz · 60 s ≈ 3000).
    dogrula_a3(ida::test::zamanlama(fizik >= 2500u, "a3_hil_olcum.fizik_durum_cercevesi",
                                    std::to_string(fizik) + " (≥ 2500)"),
               "istemci fizik/durum'u ≈ 50 Hz aldı (zamanlama)");
    return 0;
}

// ---- W4-K27 düzeltme turu (sol incelemesi #1, #3, #4, #6, #7) ----

// Hazırlanırken durdurma (W4-K27 (4); sol #6): SIL TCP, koşucu bağlanmıyor (hazırlık bekliyor);
// panelden `durdur` → çalıştırma `yuklendi`'ye döner ve başlamaz. Hazırlık başarısızlığında da.
int test_a3_hazirlik_durdur()
{
    std::uint32_t no = 0;
    ida::sunucu::Ayar a = z1_yerlesik(5.0);
    a.karsi = ida::sunucu::Karsi::tcp;
    a.karsi_verildi = true;
    a.baglanti_bekleme_ns = 20'000'000'000;
    const auto bas = std::chrono::steady_clock::now();
    bool red = false;
    const Kosu k = kos(a, true, nullptr, [&no, &red](std::uint64_t b, ida::a3::KomutAlicisi& al) {
        const ida::a3::AnlikDurum d = al.anlik();
        if (b == 0 && d.durum == ida::a3::CalistirmaDurumu::hazirlaniyor && no == 0u) {
            red = duraklatma_dene(al);   // W4-K28 (1): hazırlıkta da reddedilir
            const Komut k2 = calistirma(d, KomutTuru::calistirma_durdur);
            no = k2.komut_no;
            dogrula_a3(al.al(k2).tur == ida::a3::AlimSonucu::Tur::kuyrukta, "hazırlanırken durdur kuyrukta");
        }
    });
    const double sure = std::chrono::duration<double>(std::chrono::steady_clock::now() - bas).count();
    dogrula_a3(k.s.tamam && k.s.calistirma_sonu == "yuklendi" && k.s.satirlar.empty(),
               "durdur: çalıştırma yuklendi'ye döndü ve başlamadı (" + k.s.calistirma_sonu + ")");
    const auto* o = a3_sonucu(k.s, no);
    dogrula_a3(o != nullptr && o->sonuc == "uygulandi", "durdur uygulandı");
    dogrula_a3(red, "hazırlıkta duraklat/sürdür/adımla gecersiz_durum (W4-K28 (1))");
    dogrula_a3(sure < 10.0, "hazırlık beklemesinde işlendi (" + std::to_string(sure) + " s; bağlantı bekleme 20 s)");
    // Hazırlık başarısız (koşucu yok, 0,5 s): yuklendi, başlamaz.
    ida::sunucu::Ayar f = z1_yerlesik(5.0);
    f.karsi = ida::sunucu::Karsi::tcp;
    f.karsi_verildi = true;
    f.baglanti_bekleme_ns = 500'000'000;
    const Kosu kf = kos(f, true, nullptr);
    dogrula_a3(!kf.s.tamam && kf.s.calistirma_sonu == "yuklendi" && kf.s.satirlar.empty(),
               "hazırlık başarısız: yuklendi, başlamadı (" + kf.s.calistirma_sonu + ")");
    return 0;
}

// W4-K28 (3) (EMNİYET; sol-2 #5): hazırlığın son el sıkışma pompası sırasında (`hazirlaniyor`
// revizyonuyla) kabul edilen `durdur`, hazırlığın tamamlanmasıyla (iç geçiş, revizyon artışı)
// bayatlamaz: durdurma kazanır, koşu başlamaz, fizik adımı 0 yürümez. Enjeksiyon A1 bayt kancasından
// (kartın el sıkışma yanıtının alındığı pompa), sınır kancasının verdiği alıcıyla yapılır.
int test_a3_hazirlik_son_durdur()
{
    for (const bool hil : {false, true}) {
        const std::string ad = hil ? "HIL" : "SIL";
        ida::sunucu::Ayar a = z1_yerlesik(5.0);
        if (hil) {
            a.kip = ida::sunucu::Kip::hil;
            a.karsi = ida::sunucu::Karsi::pty;
            a.karsi_verildi = true;
        }
        Kosu k;
        gozlem_kur(a, k);
        a.csv_uret = true;
        a.a3_acik = true;
        a.a3_port = 0;
        a.a3_kapanis_bekleme_s = 0.0;
        ida::a3::KomutAlicisi* alici = nullptr;
        std::uint32_t no = 0;
        bool enjekte = false;
        bool kabul_gitti = false;
        ida::a3::AlimSonucu::Tur tur = ida::a3::AlimSonucu::Tur::ret;
        ida_link_alici_t cozucu{};
        (void)ida_link_alici_kur(&cozucu, IDA_LINK_ROL_KART);   // sim → kart çerçeveleri
        a.a3_sinir_kancasi = [&alici](std::uint64_t, ida::a3::KomutAlicisi& al) { alici = &al; };
        auto bayt = a.a1_bayt_kancasi;
        a.a1_bayt_kancasi = [&, bayt](bool giden, const std::uint8_t* p, std::size_t n) {
            bayt(giden, p, n);
            if (giden) {
                // SESSION_ACCEPT yazıldı: kartın oturumu taşıyan ilk yanıtı el sıkışmayı bitirir;
                // onun alındığı pompa son el sıkışma pompasıdır.
                std::size_t kk = 0;
                while (kk < n) {
                    std::size_t tuk = 0;
                    ida_link_cerceve_t c{};
                    const ida_link_rc_t rc = ida_link_al(&cozucu, p + kk, n - kk, &tuk, &c);
                    kk += tuk;
                    if (rc == IDA_LINK_CERCEVE && c.msgid == 46002u) {   // IDA_SESSION_ACCEPT
                        kabul_gitti = true;
                    }
                    if (tuk == 0u) {
                        break;
                    }
                }
                return;
            }
            if (!kabul_gitti || enjekte || alici == nullptr) {
                return;
            }
            const ida::a3::AnlikDurum d = alici->anlik();
            if (d.durum == ida::a3::CalistirmaDurumu::hazirlaniyor) {
                enjekte = true;
                const Komut kd = calistirma(d, KomutTuru::calistirma_durdur);
                no = kd.komut_no;
                tur = alici->al(kd).tur;
            }
        };
        k.s = ida::sunucu::kos(a);
        dogrula_a3(enjekte && tur == ida::a3::AlimSonucu::Tur::kuyrukta,
                   ad + ": son el sıkışma pompasında durdur kabul edildi (kuyrukta)");
        const auto* o = a3_sonucu(k.s, no);
        dogrula_a3(o != nullptr && o->sonuc == "uygulandi",
                   ad + ": durdur uygulandı (" + (o != nullptr ? o->sonuc + " " + o->neden : std::string("sonuç yok")) + ")");
        dogrula_a3(k.s.tamam && k.s.calistirma_sonu == "yuklendi",
                   ad + ": çalıştırma yuklendi'de kaldı (" + k.s.calistirma_sonu + ")");
        dogrula_a3(k.s.satirlar.empty() && k.pwm_notr_degil.empty(),
                   ad + ": koşu başlamadı, fizik adımı 0 yürümedi (satır " + std::to_string(k.s.satirlar.size()) +
                       ", sınır gözlemi " + std::to_string(k.pwm_notr_degil.size()) + ")");
    }
    return 0;
}

// Sim → kart baytlarındaki çerçevelerin msgid'leri (kart rolündeki çözücüyle).
std::vector<std::uint32_t> sim_cerceveleri(const std::vector<std::uint8_t>& v)
{
    std::vector<std::uint32_t> r;
    ida_link_alici_t c{};
    (void)ida_link_alici_kur(&c, IDA_LINK_ROL_KART);
    std::size_t kk = 0;
    while (kk < v.size()) {
        std::size_t tuk = 0;
        ida_link_cerceve_t f{};
        const ida_link_rc_t rc = ida_link_al(&c, v.data() + kk, v.size() - kk, &tuk, &f);
        kk += tuk;
        if (rc == IDA_LINK_CERCEVE) {
            r.push_back(f.msgid);
        }
        if (tuk == 0u) {
            break;
        }
    }
    return r;
}

// Kısmi yazım veren sahte taşıma: `cimri` açıkken her yazımda en çok 3 bayt, ardından bir kez 0
// (ilerleme yok). Gerçekten yazılan baytlar ve düzgün kapanış kaydedilir.
class CimriTasima final : public ida::kopru::Tasima {
public:
    CimriTasima(std::unique_ptr<ida::kopru::Tasima> ic, std::vector<std::uint8_t>& yazilan, bool& cimri, bool& duzgun)
        : ic_(std::move(ic)), yazilan_(yazilan), cimri_(cimri), duzgun_(duzgun)
    {
    }
    long long yaz_biraz(const std::uint8_t* p, std::size_t n) override
    {
        if (cimri_) {
            sifir_ = !sifir_;
            if (!sifir_) {
                return 0;
            }
            n = std::min<std::size_t>(n, 3u);
        }
        const long long k = ic_->yaz_biraz(p, n);
        if (k > 0) {
            yazilan_.insert(yazilan_.end(), p, p + k);
        }
        return k;
    }
    ida::kopru::OkumaSonucu oku(std::uint8_t* p, std::size_t n, std::int64_t bekle_us, std::size_t& okunan) override
    {
        return ic_->oku(p, n, bekle_us, okunan);
    }
    void giris_bosalt() override { ic_->giris_bosalt(); }
    bool acik() const override { return ic_->acik(); }
    void kapat() override { ic_->kapat(); }
    void kapat_duzgun(std::int64_t sure_us) override
    {
        duzgun_ = true;
        ic_->kapat_duzgun(sure_us);
    }
    std::string ad() const override { return "cimri(" + ic_->ad() + ")"; }
    bool cikis_bosalt(std::int64_t sure_us) override { return ic_->cikis_bosalt(sure_us); }
    int son_hata_kodu() const override { return ic_->son_hata_kodu(); }

private:
    std::unique_ptr<ida::kopru::Tasima> ic_;
    std::vector<std::uint8_t>& yazilan_;
    bool& cimri_;
    bool& duzgun_;
    bool sifir_ = false;
};

// W4-K28 (3) (EMNİYET; sol-2 #6): hazırlık iptalinde SESSION_CLOSE, taşıma kısmi ya da 0 yazım
// döndürse de mutlak süre sınırıyla tamamen yazılır; ardından taşıma düzgün kapanır. Oturum
// kabulü (SESSION_ACCEPT) sıraya girince panelden `durdur` verilir ve taşıma cimrileşir.
int test_a3_hazirlik_kapanis()
{
    ida::sunucu::Ayar a = z1_yerlesik(5.0);
    Kosu k;
    gozlem_kur(a, k);
    a.a3_acik = true;
    a.a3_port = 0;
    a.a3_kapanis_bekleme_s = 0.0;
    std::vector<std::uint8_t> yazilan;
    bool cimri = false, duzgun = false;
    ida::a3::KomutAlicisi* alici = nullptr;
    std::uint32_t no = 0;
    a.tasima_sarmalayici = [&](std::unique_ptr<ida::kopru::Tasima> t) -> std::unique_ptr<ida::kopru::Tasima> {
        return std::make_unique<CimriTasima>(std::move(t), yazilan, cimri, duzgun);
    };
    a.a3_sinir_kancasi = [&alici](std::uint64_t, ida::a3::KomutAlicisi& al) { alici = &al; };
    auto bayt = a.a1_bayt_kancasi;
    a.a1_bayt_kancasi = [&, bayt](bool giden, const std::uint8_t* p, std::size_t n) {
        bayt(giden, p, n);
        if (!giden || cimri || alici == nullptr) {
            return;
        }
        const std::vector<std::uint32_t> m = sim_cerceveleri(std::vector<std::uint8_t>(p, p + n));
        if (std::find(m.begin(), m.end(), 46002u) != m.end()) {   // IDA_SESSION_ACCEPT
            const ida::a3::AnlikDurum d = alici->anlik();
            if (d.durum == ida::a3::CalistirmaDurumu::hazirlaniyor) {
                const Komut kd = calistirma(d, KomutTuru::calistirma_durdur);
                no = kd.komut_no;
                (void)alici->al(kd);
                cimri = true;
            }
        }
    };
    k.s = ida::sunucu::kos(a);
    const auto* o = a3_sonucu(k.s, no);
    dogrula_a3(cimri && o != nullptr && o->sonuc == "uygulandi", "oturum kabulünden sonra durdur uygulandı");
    dogrula_a3(k.s.tamam && k.s.calistirma_sonu == "yuklendi" && k.s.satirlar.empty(),
               "çalıştırma yuklendi'de kaldı, başlamadı (" + k.s.calistirma_sonu + ")");
    const std::vector<std::uint32_t> m = sim_cerceveleri(yazilan);
    const bool kapanis = !m.empty() && m.back() == 46004u;   // IDA_SESSION_CLOSE
    dogrula_a3(kapanis, "kart tarafı tam SESSION_CLOSE aldı (son çerçeve " + std::to_string(m.empty() ? 0u : m.back()) + ")");
    dogrula_a3(yazilan == k.sim_giden,
               "sıraya giren bütün baytlar yazıldı (" + std::to_string(yazilan.size()) + "/" + std::to_string(k.sim_giden.size()) + ")");
    dogrula_a3(duzgun && k.s.kapanis_yazim_eksik == 0u, "taşıma düzgün kapandı; süre sınırına takılmadı");
    return 0;
}

// Sürekli okunabilir (her okuma 4 KiB çöp döner), yazımı ilerlemeyen sahte taşıma; `sel` açılınca.
class SelTasima final : public ida::kopru::Tasima {
public:
    SelTasima(std::unique_ptr<ida::kopru::Tasima> ic, std::atomic<bool>& sel) : ic_(std::move(ic)), sel_(sel) {}
    long long yaz_biraz(const std::uint8_t* p, std::size_t n) override { return sel_ ? 0 : ic_->yaz_biraz(p, n); }
    ida::kopru::OkumaSonucu oku(std::uint8_t* p, std::size_t n, std::int64_t bekle_us, std::size_t& okunan) override
    {
        if (!sel_) {
            return ic_->oku(p, n, bekle_us, okunan);
        }
        okunan = std::min<std::size_t>(n, 4096u);
        std::memset(p, 0x55, okunan);
        return ida::kopru::OkumaSonucu::tamam;
    }
    void giris_bosalt() override { ic_->giris_bosalt(); }
    bool acik() const override { return ic_->acik(); }
    void kapat() override { ic_->kapat(); }
    void kapat_duzgun(std::int64_t sure_us) override { ic_->kapat_duzgun(sure_us); }
    std::string ad() const override { return "sel(" + ic_->ad() + ")"; }
    bool cikis_bosalt(std::int64_t sure_us) override { return ic_->cikis_bosalt(sure_us); }
    int son_hata_kodu() const override { return ic_->son_hata_kodu(); }

private:
    std::unique_ptr<ida::kopru::Tasima> ic_;
    std::atomic<bool>& sel_;
};

// W4-K28 sol-3 #2 (EMNİYET): kapanış yazımı ilerlemezken HIL taşıması sürekli veri döndürse de
// kapanış mutlak süre sınırında sonuçlanır (HIL pompa turu da sınırlı). İşlevsel ölçüt: kos()
// döndü ve eksik kapanış sayıldı; süre W4-K24 yardımcısıyla.
int test_a3_kapanis_sel()
{
    ida::sunucu::Ayar a;
    a.gunluk = 1;
    a.kip = ida::sunucu::Kip::hil;
    a.karsi = ida::sunucu::Karsi::pty;
    a.sure_s = 1.0;
    std::atomic<bool> sel{false};
    std::chrono::steady_clock::time_point bas{};
    a.tasima_sarmalayici = [&sel](std::unique_ptr<ida::kopru::Tasima> t) -> std::unique_ptr<ida::kopru::Tasima> {
        return std::make_unique<SelTasima>(std::move(t), sel);
    };
    a.sinir_kancasi = [&](const ida::sunucu::Ayar::SinirGozlemi& g) {
        if (g.b == 800u && !sel) {   // son sınır (1 s): kapanış yolu sel altında
            sel = true;
            bas = std::chrono::steady_clock::now();
        }
    };
    const ida::sunucu::Sonuc s = ida::sunucu::kos(a);
    const double sure = std::chrono::duration<double>(std::chrono::steady_clock::now() - bas).count();
    dogrula_a3(sel.load(), "ön koşul: kapanış sel altında (son sınırdan sonra)");
    dogrula_a3(s.kapanis_yazim_eksik >= 1u, "kapanış döndü; eksik kapanış sayıldı (" + std::to_string(s.kapanis_yazim_eksik) + ")");
    // Bütçe: 20 ms son okuma + 500 ms kapanış yazımı + 500 ms düzgün kapanış + pay.
    dogrula_a3(ida::test::zamanlama(sure < 1.5, "a3_kapanis_sel.donus", std::to_string(sure) + " s (< 1,5 s)"),
               "kapanış süre sınırı içinde döndü (zamanlama)");
    return 0;
}

// W4-K28 sol-3 #1 (EMNİYET): kabul edilmiş `durdur` hiçbir kuyruk durumu yüzünden bayatlamaz.
// (a) Hazırlıkta dünyanın genel deposu 512 nötr istekle dolu, halka da dolu: durdur yine alınır,
//     koşu başlamaz, fizik adımı 0 yürümez. (b) Koşarken dolu halkayla durdur: aynı sınırda durur.
// (c) Koşunun son sınırından sonra (fiziğin son adımından önce) kabul edilen durdur: `bitti`
//     iç geçişi onu bayatlatamaz, çalıştırma `durduruldu` biter.
int test_a3_durdur_dolu_depo()
{
    auto manual = [](ida::a3::KomutAlicisi& al, const ida::a3::AnlikDurum& d, int n) {
        int kuyrukta = 0;
        for (int i = 0; i < n; ++i) {
            kuyrukta += al.al(yi(d, "manual", ida::a3::KomutSinifi::notr, "{\"axes\":[0,0,0,0,0,0]}")).tur ==
                                ida::a3::AlimSonucu::Tur::kuyrukta
                            ? 1
                            : 0;
        }
        return kuyrukta;
    };
    {
        int dolum = 0, kuyrukta = 0;
        std::uint32_t no = 0;
        ida::a3::AlimSonucu::Tur tur = ida::a3::AlimSonucu::Tur::ret;
        const Kosu k = kos(z1_yerlesik(5.0), true, nullptr, [&](std::uint64_t, ida::a3::KomutAlicisi& al) {
            const ida::a3::AnlikDurum d = al.anlik();
            if (d.durum != ida::a3::CalistirmaDurumu::hazirlaniyor || no != 0u) {
                return;
            }
            if (dolum < 2) {
                kuyrukta += manual(al, d, 256);   // her yoklamada halka dünyaya (depoya) aktarılır
                ++dolum;
                return;
            }
            const Komut kd = calistirma(d, KomutTuru::calistirma_durdur);
            no = kd.komut_no;
            tur = al.al(kd).tur;
        });
        dogrula_a3(kuyrukta >= 512 && no != 0u && tur == ida::a3::AlimSonucu::Tur::kuyrukta,
                   "(a) ön koşul: hazırlıkta ≥ 512 nötr istek bekliyor (" + std::to_string(kuyrukta) +
                       "), durdur kabul edildi");
        const auto* o = a3_sonucu(k.s, no);
        dogrula_a3(o != nullptr && o->sonuc == "uygulandi",
                   "(a) dolu depoda durdur uygulandı (" + (o != nullptr ? o->sonuc + " " + o->neden : std::string("yok")) + ")");
        dogrula_a3(k.s.tamam && k.s.calistirma_sonu == "yuklendi" && k.s.satirlar.empty() && k.pwm_notr_degil.empty(),
                   "(a) koşu başlamadı, fizik adımı 0 yürümedi (" + k.s.calistirma_sonu + ")");
    }
    {
        std::uint32_t no = 0;
        int dolu = 0;
        const Kosu k = kos(z1_yerlesik(8.0), true, nullptr, [&](std::uint64_t b, ida::a3::KomutAlicisi& al) {
            const ida::a3::AnlikDurum d = al.anlik();
            if (b == 4000 && no == 0u && d.durum == ida::a3::CalistirmaDurumu::kosuyor) {
                (void)manual(al, d, 256);
                dolu = al.al(yi(d, "manual", ida::a3::KomutSinifi::notr, "{\"axes\":[0,0,0,0,0,0]}")).tur ==
                               ida::a3::AlimSonucu::Tur::dolu
                           ? 1
                           : 0;
                const Komut kd = calistirma(d, KomutTuru::calistirma_durdur);
                no = kd.komut_no;
                (void)al.al(kd);
            }
        });
        const auto* o = a3_sonucu(k.s, no);
        dogrula_a3(dolu == 1 && o != nullptr && o->sonuc == "uygulandi" && o->t_ns == 5'000'000'000,
                   "(b) halka doluyken durdur aynı sınırda (5 s) uygulandı");
        dogrula_a3(k.s.calistirma_sonu == "durduruldu" && !k.s.satirlar.empty() && k.s.satirlar.back().t_s <= 5.0 + 1e-9,
                   "(b) çalıştırma 5 s'de durdu (" + k.s.calistirma_sonu + ")");
    }
    {
        ida::sunucu::Ayar a = z1_yerlesik(2.0);
        ida::a3::KomutAlicisi* alici = nullptr;
        std::uint32_t no = 0;
        Kosu k;
        gozlem_kur(a, k);
        auto gozlem = a.sinir_kancasi;
        a.sinir_kancasi = [&, gozlem](const ida::sunucu::Ayar::SinirGozlemi& g) {
            gozlem(g);
            if (g.b == 1600u && alici != nullptr && no == 0u) {   // son sınır (2 s), fizik adımından önce
                const Komut kd = calistirma(alici->anlik(), KomutTuru::calistirma_durdur);
                no = kd.komut_no;
                (void)alici->al(kd);
            }
        };
        a.csv_uret = true;
        a.a3_acik = true;
        a.a3_port = 0;
        a.a3_kapanis_bekleme_s = 0.0;
        a.a3_sinir_kancasi = [&alici](std::uint64_t, ida::a3::KomutAlicisi& al) { alici = &al; };
        k.s = ida::sunucu::kos(a);
        const auto* o = a3_sonucu(k.s, no);
        dogrula_a3(no != 0u && o != nullptr && o->sonuc == "uygulandi",
                   "(c) son sınırda kabul edilen durdur uygulandı (" + (o != nullptr ? o->sonuc + " " + o->neden : std::string("yok")) + ")");
        dogrula_a3(k.s.calistirma_sonu == "durduruldu", "(c) bitti iç geçişi durdurmayı bayatlatmadı (" + k.s.calistirma_sonu + ")");
    }
    return 0;
}

// ---- W4-K28 E (EMNİYET; sol-2 #2 olağan işletimde): aynı etkiye iki ardışık daraltıcı istek ve
// ardından farklı etkide kısıtlayan (DISARM → DISARM → acil bas), gerçek kitaplık yolundan (HTTP →
// A3 çekirdeği → alıcı → dünya). İkinci DISARM kitaplığın mandalına düşer; acil bas onun arkasında
// beklememeli: dünya yuvaları boşalttığı ilk sınırda uygulanmalı. ----

struct A3Uc {
    std::uint16_t port = 0;
    std::string jeton;
    std::string istemci;   // "Ida-Istemci: n\r\n"
    int istek = 0;
    ida::test::a3::HttpSonuc post(const std::string& yol, const std::string& govde)
    {
        char kimlik[32];
        std::snprintf(kimlik, sizeof kimlik, "%024x", static_cast<unsigned>(++istek));   // istek_kimligi: 24 onaltılık
        const std::string g = "{\"istek_kimligi\":\"" + std::string(kimlik) + "\"," + govde.substr(1);
        return ida::test::a3::http_istek(port, jeton, "POST", "/api/v1" + yol, g, istemci);
    }
    bool kaydol()
    {
        const auto y = ida::test::a3::http_istek(port, jeton, "POST", "/api/v1/istemciler", "{\"ad\":\"ardisik\"}");
        const std::size_t k = y.govde.find("\"istemci_id\":");
        if (y.durum != 200 && y.durum != 201) {
            return false;
        }
        if (k == std::string::npos) {
            return false;
        }
        istemci = "Ida-Istemci: " + std::to_string(std::atoi(y.govde.c_str() + k + 13)) + "\r\n";
        return true;
    }
    // DISARM, DISARM, acil bas (kısıtlayanlar; run_id güncel ya da null).
    std::array<ida::test::a3::HttpSonuc, 3> uclu(const std::optional<std::string>& run)
    {
        const std::string r = run ? "\"" + *run + "\"" : std::string("null");
        std::array<ida::test::a3::HttpSonuc, 3> s;
        s[0] = post("/yer_istasyonu/komut", "{\"beklenen_run_id\":" + r + ",\"komut\":\"disarm\",\"parametreler\":{}}");
        s[1] = post("/yer_istasyonu/komut", "{\"beklenen_run_id\":" + r + ",\"komut\":\"disarm\",\"parametreler\":{}}");
        s[2] = post("/emniyet/degistir",
                    "{\"beklenen_run_id\":" + r + ",\"girdi\":\"acil_durdurma\",\"hedef_durum\":\"basili\"}");
        return s;
    }
};

int test_a3_kisitlayan_ardisik()
{
    // (1) SIL, koşarken (b = 4000, 5 s) ve (2) SIL hazırlıkta; (3) HIL hazırlıkta.
    struct Durum {
        const char* ad;
        bool hil;
        bool hazirlik;
    };
    for (const Durum& dz : {Durum{"SIL koşarken", false, false}, Durum{"SIL hazırlıkta", false, true},
                            Durum{"HIL hazırlıkta", true, true}}) {
        const std::string ad = dz.ad;
        ida::sunucu::Ayar a = z1_yerlesik(dz.hil ? 2.0 : 6.0);
        if (dz.hil) {
            a.kip = ida::sunucu::Kip::hil;
            a.karsi = ida::sunucu::Karsi::pty;
            a.karsi_verildi = true;
        }
        A3Uc uc;
        a.a3_hazir = [&uc](std::uint16_t p, const std::string& j) {
            uc.port = p;
            uc.jeton = j;
        };
        bool verildi = false, kayit = false, hiz = false;
        std::array<ida::test::a3::HttpSonuc, 3> y{};
        const std::uint64_t kB = dz.hazirlik ? 0u : 4000u;
        const Kosu k = kos(a, true, nullptr, [&](std::uint64_t b, ida::a3::KomutAlicisi& al) {
            if (!kayit) {
                kayit = uc.kaydol();
            }
            const ida::a3::AnlikDurum d = al.anlik();
            if (!dz.hil && !hiz && d.durum == ida::a3::CalistirmaDurumu::kosuyor) {
                // SIL'i duvar saatine bağla (hız 4): mandaldaki ikinci DISARM'ın G/Ç döngüsünde
                // yuvaya geçmesine koşu bitmeden zaman kalsın (yalnız bekleme; çıktı değişmez).
                hiz = true;
                Komut h = calistirma(d, KomutTuru::calistirma_hiz);
                h.carpan = 4.0;
                (void)al.al(h);
            }
            const bool zaman = dz.hazirlik ? d.durum == ida::a3::CalistirmaDurumu::hazirlaniyor
                                           : (b == kB && d.durum == ida::a3::CalistirmaDurumu::kosuyor);
            if (zaman && !verildi && kayit) {
                verildi = true;
                y = uc.uclu(d.run_id);
            }
        });
        dogrula_a3(k.s.tamam && verildi, ad + ": koşu tamam, istekler verildi " + k.s.hata);
        bool kuyrukta = true;
        for (const auto& r : y) {
            kuyrukta = kuyrukta && r.durum == 202 && r.komut_no() != 0u;
        }
        dogrula_a3(kuyrukta, ad + ": üç istek 202 kuyrukta (" + std::to_string(y[0].durum) + "," + std::to_string(y[1].durum) +
                                 "," + std::to_string(y[2].durum) + (kuyrukta ? std::string() : "; " + y[0].govde) + ")");
        const auto* d1 = a3_sonucu(k.s, y[0].komut_no());
        const auto* d2 = a3_sonucu(k.s, y[1].komut_no());
        const auto* ac = a3_sonucu(k.s, y[2].komut_no());
        const std::int64_t t = static_cast<std::int64_t>(kB) * 1'250'000;
        dogrula_a3(d1 != nullptr && d1->sonuc == "uygulandi" && d1->t_ns == t, ad + ": ilk DISARM ilk sınırda uygulandı");
        dogrula_a3(d2 != nullptr && d2->sonuc == "uygulandi",
                   ad + ": ikinci DISARM (mandaldan) uygulandı (" +
                       (d2 != nullptr ? d2->sonuc + " " + d2->neden + " " + std::to_string(d2->t_ns) : std::string("sonuç yok")) + ")");
        dogrula_a3(ac != nullptr && ac->sonuc == "uygulandi" && ac->t_ns == t,
                   ad + ": acil bas DISARM mandalının arkasında beklemedi, ilk sınırda uygulandı (" +
                       (ac != nullptr ? std::to_string(ac->t_ns) : std::string("sonuç yok")) + " ns; beklenen " +
                       std::to_string(t) + ")");
        dogrula_a3(k.acil.size() > kB && k.acil[kB] == 1u, ad + ": fiziğe giden ilk sınırda acil durdurma etkin");
    }
    // (4) `yuklendi` (--a3-bekle): canlı çalıştırma yok; üç istek beklemeden reddedilir (mandal yok).
    {
        ida::sunucu::Ayar a = z1_yerlesik(1.0);
        a.a3_bekle = true;
        A3Uc uc;
        a.a3_hazir = [&uc](std::uint16_t p, const std::string& j) {
            uc.port = p;
            uc.jeton = j;
        };
        std::array<ida::test::a3::HttpSonuc, 3> y{};
        bool verildi = false;
        const Kosu k = kos(a, true, nullptr, [&](std::uint64_t, ida::a3::KomutAlicisi& al) {
            const ida::a3::AnlikDurum d = al.anlik();
            if (d.durum == ida::a3::CalistirmaDurumu::yuklendi && !verildi && uc.kaydol()) {
                verildi = true;
                y = uc.uclu(std::nullopt);
                (void)al.al(calistirma(d, KomutTuru::calistirma_baslat));
            }
        });
        bool ret = verildi;
        for (const auto& r : y) {
            ret = ret && r.durum >= 400 && r.kod() == "canli_calistirma_yok";
        }
        dogrula_a3(ret, "yuklendi: üç istek beklemeden canli_calistirma_yok (" + y[0].kod() + "," + y[1].kod() + "," +
                            y[2].kod() + ")");
        dogrula_a3(k.s.tamam && k.s.calistirma_sonu == "bitti", "yuklendi: sonra baslat ile koşu bitti " + k.s.hata);
    }
    return 0;
}

// W4-K28 (1) HIL: duraklatma kapalı. Hazırlıkta ve koşarken (onaylı duraklatma dahil) duraklat/sürdür/
// adımla girişte reddedilir; durum `duraklatildi` olmaz; HIL duraklatma geçersizliği kurulmaz; TF'ler
// boşluksuz ve çalıştırma `bitti`.
int test_a3_hil_duraklat_ret()
{
    ida::sunucu::Ayar a;
    a.gunluk = 1;
    a.kip = ida::sunucu::Kip::hil;
    a.karsi = ida::sunucu::Karsi::pty;
    a.sure_s = 4.0;
    a.a3_acik = true;
    a.a3_port = 0;
    a.a3_kapanis_bekleme_s = 0.0;
    int deneme = 0, tamam = 0;
    bool hazirlikta = false, duraklatildi = false;
    a.a3_sinir_kancasi = [&](std::uint64_t b, ida::a3::KomutAlicisi& al) {
        const ida::a3::AnlikDurum d = al.anlik();
        duraklatildi = duraklatildi || d.durum == ida::a3::CalistirmaDurumu::duraklatildi;
        const bool hz = d.durum == ida::a3::CalistirmaDurumu::hazirlaniyor;
        if ((hz && !hazirlikta) || b == 800 || b == 2400) {
            hazirlikta = hazirlikta || hz;
            ++deneme;
            tamam += duraklatma_dene(al) ? 1 : 0;
        }
    };
    const ida::sunucu::Sonuc s = ida::sunucu::kos(a);
    dogrula_a3(s.tamam && s.calistirma_sonu == "bitti", "HIL koşusu tamam ve bitti " + s.hata);
    dogrula_a3(deneme >= 2 && tamam == deneme,
               "HIL: duraklat/sürdür/adımla (onaylı dahil) gecersiz_durum; durum değişmedi (" + std::to_string(tamam) + "/" +
                   std::to_string(deneme) + ")");
    dogrula_a3(!duraklatildi, "HIL: duraklatildi durumu hiç görülmedi");
    dogrula_a3((s.sayac.calistirma_gecersiz & ida::kopru::kGecDuraklatma) == 0u, "HIL duraklatma geçersizliği kurulmadı");
    dogrula_a3(s.zaman.tf_sayisi >= 790u, "TF'ler sürdü (" + std::to_string(s.zaman.tf_sayisi) + ")");
    return 0;
}

// W4-K27 (5): dünya tarafındaki A3 hizmeti bellek ayırmaz. Ayırmalar `operator new` kancasıyla, A3
// hizmeti kapsamında (köprü ve fizik çağrıları hariç) sayılır. Kapsam: bağlı istemci, emniyet bas/
// bırak, genişletici, nötr ve daraltıcı yer istasyonu komutları, hız ve senaryo arızası.
int test_a3_ayirma()
{
#if defined(_MSC_VER)
    std::printf("  ATLANDI: MSVC'de genel operator new değiştirilmez (yalnız POSIX derleyicilerinde sınanır)\n");
    return 0;
#else
    a3_ayirma_sifirla();
    Istemciler ist;
    ist.yavas_var = true;
    const Kosu k = kos(z1_yerlesik(12.0), true, &ist, [](std::uint64_t b, ida::a3::KomutAlicisi& al) {
        const ida::a3::AnlikDurum d = al.anlik();
        if (b == 4000) {
            (void)al.al(emniyet(d, ida::a3::EmniyetGirdisi::acil_durdurma, -1, ida::a3::GirdiDurumu::basili, true));
            (void)al.al(yi(d, "param_get", ida::a3::KomutSinifi::notr, "{\"param_id\":1}"));
        } else if (b == 4400) {
            (void)al.al(emniyet(d, ida::a3::EmniyetGirdisi::acil_durdurma, -1, ida::a3::GirdiDurumu::serbest, false));
            (void)al.al(yi(d, "arm", ida::a3::KomutSinifi::genisletici, "{}"));
        } else if (b == 6000) {
            (void)al.al(emniyet(d, ida::a3::EmniyetGirdisi::emniyet_pimi, 0, ida::a3::GirdiDurumu::takili, true));
            (void)al.al(yi(d, "disarm", ida::a3::KomutSinifi::daraltici, "{}"));
            Komut h = calistirma(d, KomutTuru::calistirma_hiz);
            h.carpan = 0.0;
            (void)al.al(h);
        }
    });
    const std::uint64_t n = a3_ayirma_sayisi();
    dogrula_a3(k.s.tamam && k.s.a3_komut >= 7u, "koşu tamam, komutlar işlendi (" + std::to_string(k.s.a3_komut) + ")");
    dogrula_a3(n == 0u, "dünya tarafındaki A3 hizmetinde bellek ayırma yok (" + std::to_string(n) + ")");
    std::printf("  ÖLÇÜM (bu Mac, 2026-11-24, SIL olabildiğince hızlı): dünya tarafındaki A3 hizmeti sınır/yoklama "
                "başına en çok %.1f µs; yayın çağrısı %llu, tek çağrı en çok %.1f µs\n",
                1e-3 * static_cast<double>(k.s.a3_sinir_en_kotu_ns), static_cast<unsigned long long>(k.s.a3_yayin_cagrisi),
                1e-3 * static_cast<double>(k.s.a3_cagri_en_kotu_ns));
    return 0;
#endif
}

}  // namespace

int a3_testi(const std::string& ad, int /*argc*/, char** /*argv*/)
{
    if (ad == "a3_belirlenim") {
        (void)test_a3_belirlenim();
    } else if (ad == "a3_emniyet_yolu") {
        (void)test_a3_emniyet_yolu();
    } else if (ad == "a3_emniyet_ret") {
        (void)test_a3_emniyet_ret();
    } else if (ad == "a3_calistirma") {
        (void)test_a3_calistirma();
    } else if (ad == "a3_hil_olcum") {
        (void)test_a3_hil_olcum();
    } else if (ad == "a3_kapanis_sel") {
        (void)test_a3_kapanis_sel();
    } else if (ad == "a3_durdur_dolu_depo") {
        (void)test_a3_durdur_dolu_depo();
    } else if (ad == "a3_kisitlayan_ardisik") {
        (void)test_a3_kisitlayan_ardisik();
    } else if (ad == "a3_hazirlik_kapanis") {
        (void)test_a3_hazirlik_kapanis();
    } else if (ad == "a3_hazirlik_son_durdur") {
        (void)test_a3_hazirlik_son_durdur();
    } else if (ad == "a3_hil_duraklat_ret") {
        (void)test_a3_hil_duraklat_ret();
    } else if (ad == "a3_hazirlik_durdur") {
        (void)test_a3_hazirlik_durdur();
    } else if (ad == "a3_ayirma") {
        (void)test_a3_ayirma();
    } else {
        return -1;   // bu dosyanın testi değil
    }
    return g_hata_a3;
}
