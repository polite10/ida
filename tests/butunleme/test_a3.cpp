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
//   a3_calistirma     SIL'de duraklat/adımla/hız/sürdür CSV ve A1 akışını değiştirmez; duraklatılmışken
//                     emniyet isteği beklemez; durdur çalıştırmayı bitirir
//   a3_hil_olcum      kabul 5 (ölçüm): HIL 1x 60 s, sözde uçbirim, A3 açık ve 50 Hz abonelikli istemci
#include "a3_istemci.hpp"
#include "ida/a3/komut.hpp"
#include "ida/sunucu/sunucu.hpp"
#include "../../sim/kopru/test/zamanlama_olcutu.hpp"   // W4-K24

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

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
    dogrula_a3(denenen > 10u && eslesen == denenen,
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

int test_a3_calistirma()
{
    const Kosu taban = kos(z1_yerlesik(12.0), false, nullptr);
    // Duraklat (2,5 s) → bekle → hız 20 → adımla 3 periyot → duraklat'a döner → sürdür; 8 s'de hız 0.
    int yoklama = 0;
    std::vector<std::uint32_t> nolar;
    auto ver = [&nolar](ida::a3::KomutAlicisi& al, Komut k, const char* ne) {
        nolar.push_back(k.komut_no);
        dogrula_a3(al.al(k).tur == ida::a3::AlimSonucu::Tur::kuyrukta, std::string(ne) + " kuyrukta");
    };
    const Kosu k = kos(z1_yerlesik(12.0), true, nullptr, [&](std::uint64_t b, ida::a3::KomutAlicisi& al) {
        const ida::a3::AnlikDurum d = al.anlik();
        if (b == 2000 && d.durum == ida::a3::CalistirmaDurumu::kosuyor && yoklama == 0) {
            ver(al, calistirma(d, KomutTuru::calistirma_duraklat), "duraklat");
            yoklama = 1;
        } else if (b == 2000 && d.durum == ida::a3::CalistirmaDurumu::duraklatildi) {
            ++yoklama;
            if (yoklama == 20) {
                Komut h = calistirma(d, KomutTuru::calistirma_hiz);
                h.carpan = 20.0;
                ver(al, h, "hız 20");
            } else if (yoklama == 40) {
                Komut s = calistirma(d, KomutTuru::calistirma_adimla);
                s.periyot_sayisi = 3;
                ver(al, s, "adımla 3");
            }
        } else if (b == 2012 && d.durum == ida::a3::CalistirmaDurumu::duraklatildi && yoklama < 1000) {
            yoklama = 1000;
            ver(al, calistirma(d, KomutTuru::calistirma_surdur), "sürdür");
        } else if (b == 6400 && d.durum == ida::a3::CalistirmaDurumu::kosuyor) {
            Komut h = calistirma(d, KomutTuru::calistirma_hiz);
            h.carpan = 0.0;
            ver(al, h, "hız 0");
        }
    });
    esitlik(taban, k, "duraklat/hız/adımla/sürdür");
    dogrula_a3(nolar.size() == 5u, "beş çalıştırma komutu");
    bool hepsi = nolar.size() == 5u;
    for (std::uint32_t no : nolar) {
        const auto* o = a3_sonucu(k.s, no);
        hepsi = hepsi && o != nullptr && o->sonuc == "uygulandi";
    }
    dogrula_a3(hepsi, "komutlar uygulandı");
    // Duraklatılmışken emniyet isteği beklemez: aynı sınırda (sim zamanı ilerlemeden) uygulanır.
    int y2 = 0;
    std::uint32_t no_acil = 0;
    const Kosu e = kos(z1_yerlesik(4.0), true, nullptr, [&](std::uint64_t b, ida::a3::KomutAlicisi& al) {
        const ida::a3::AnlikDurum d = al.anlik();
        if (b == 2000 && d.durum == ida::a3::CalistirmaDurumu::kosuyor && y2 == 0) {
            (void)al.al(calistirma(d, KomutTuru::calistirma_duraklat));
            y2 = 1;
        } else if (b == 2000 && d.durum == ida::a3::CalistirmaDurumu::duraklatildi) {
            ++y2;
            if (y2 == 10) {
                const Komut a = emniyet(d, ida::a3::EmniyetGirdisi::acil_durdurma, -1, ida::a3::GirdiDurumu::basili, true);
                no_acil = a.komut_no;
                (void)al.al(a);
            } else if (y2 == 30) {
                dogrula_a3(d.acil_durdurma == 1u && d.adim_no == 2000u, "duraklatılmışken acil durdurma uygulandı (sim zamanı 2,5 s)");
                (void)al.al(calistirma(d, KomutTuru::calistirma_surdur));
            }
        }
    });
    const auto* o = a3_sonucu(e.s, no_acil);
    dogrula_a3(o != nullptr && o->sonuc == "uygulandi" && o->t_ns == 2'500'000'000, "acil durdurmanın uygulama adımı duraklatılan sınır");
    dogrula_a3(e.acil.size() > 2000u && e.acil[2000] == 1u && notr_araligi(e, 2000, 3000), "o sınırdan itibaren itki nötr");
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
    // HIL'de duraklatma onay ister (Tablo 4.7b); onaysız istek eşzamanlı denetimde reddedilir.
    bool denendi = false;
    a.a3_sinir_kancasi = [&denendi](std::uint64_t b, ida::a3::KomutAlicisi& al) {
        if (b == 800 && !denendi) {
            denendi = true;
            const auto r = al.al(calistirma(al.anlik(), KomutTuru::calistirma_duraklat));
            dogrula_a3(r.tur == ida::a3::AlimSonucu::Tur::ret && r.hata_kodu == "onay_gerekli", "HIL'de onaysız duraklatma: onay_gerekli");
            Komut h = calistirma(al.anlik(), KomutTuru::calistirma_hiz);
            h.carpan = 2.0;
            const auto r2 = al.al(h);
            dogrula_a3(r2.tur == ida::a3::AlimSonucu::Tur::ret && r2.hata_kodu == "kipte_gecersiz", "HIL'de hız: kipte_gecersiz");
        }
    };
    const ida::sunucu::Sonuc s = ida::sunucu::kos(a);
    ist.bitir();
    dogrula_a3(s.tamam, "HIL 1x koşusu tamam " + s.hata);
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
    std::printf("  ÖLÇÜM: yayın çağrısı %llu; dünya iş parçacığında tek çağrının en uzun süresi %.1f µs, bir sınırdaki "
                "çağrıların en uzun toplamı %.1f µs; halka taşması %llu; istemcinin aldığı fizik/durum %llu, "
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

// HIL'de duraklatma (A1-K41, Tablo 4.7b): onaylı istekle; TF ve üyeler durur, canlılık sürer,
// çalıştırma geçersiz (hil_duraklatma); sürdürünce TF'ler boşluksuz sürer (A1-K29). Duraklatılmışken
// acil durdurma beklemez.
int test_a3_hil_duraklat()
{
    ida::sunucu::Ayar a;
    a.gunluk = 1;
    a.kip = ida::sunucu::Kip::hil;
    a.karsi = ida::sunucu::Karsi::pty;
    a.sure_s = 8.0;
    int yoklama = 0;
    std::uint32_t no_acil = 0;
    Kosu k;
    gozlem_kur(a, k);
    a.a3_acik = true;
    a.a3_port = 0;
    a.a3_kapanis_bekleme_s = 0.0;
    a.a3_sinir_kancasi = [&](std::uint64_t b, ida::a3::KomutAlicisi& al) {
        const ida::a3::AnlikDurum d = al.anlik();
        if (b == 1600 && d.durum == ida::a3::CalistirmaDurumu::kosuyor && yoklama == 0) {
            Komut p = calistirma(d, KomutTuru::calistirma_duraklat);
            p.gecersizligi_onayla = true;
            dogrula_a3(al.al(p).tur == ida::a3::AlimSonucu::Tur::kuyrukta, "onaylı duraklatma kuyrukta");
            yoklama = 1;
        } else if (b == 1600 && d.durum == ida::a3::CalistirmaDurumu::duraklatildi) {
            ++yoklama;
            if (yoklama == 300) {
                const Komut e = emniyet(d, ida::a3::EmniyetGirdisi::acil_durdurma, -1, ida::a3::GirdiDurumu::basili, true);
                no_acil = e.komut_no;
                (void)al.al(e);
            } else if (yoklama == 700) {
                (void)al.al(calistirma(d, KomutTuru::calistirma_surdur));
            }
        }
    };
    k.s = ida::sunucu::kos(a);
    const ida::sunucu::Sonuc& s = k.s;
    dogrula_a3(s.tamam && s.calistirma_sonu == "bitti", "HIL koşusu tamam ve bitti " + s.hata);
    dogrula_a3(yoklama >= 700, "duraklatıldı ve ≈ 0,7 s sonra sürdürüldü (" + std::to_string(yoklama) + " yoklama)");
    dogrula_a3((s.sayac.calistirma_gecersiz & ida::kopru::kGecDuraklatma) != 0u, "çalıştırma geçersiz: HIL duraklatma (A1-K79)");
    dogrula_a3(s.sayac.oturum_kopma == 0u, "oturum duraklatmada sürdü (canlılık; A1-K41)");
    dogrula_a3(s.zaman.tf_sayisi >= 1598u, "TF'ler boşluksuz sürdü (" + std::to_string(s.zaman.tf_sayisi) + ")");
    const auto* o = a3_sonucu(s, no_acil);
    dogrula_a3(o != nullptr && o->sonuc == "uygulandi" && o->t_ns == 2'000'000'000, "duraklatılmışken acil durdurma 2 s sınırında uygulandı");
    dogrula_a3(notr_araligi(k, 1600, 6400), "acil durdurmadan sonra itki nötr");
    // ZAMANLAMA (W4-K24): W(t) sürdürmede kaydırılır; sonrasında sim gecikmesi beklenmez.
    dogrula_a3(ida::test::zamanlama(s.zaman.sim_gecikmesi == 0u, "a3_hil_duraklat.sim_gecikmesi",
                                    std::to_string(s.zaman.sim_gecikmesi) + " (0)"),
               "sürdürmeden sonra sim gecikmesi yok (zamanlama)");
    return 0;
}

}  // namespace

int a3_testi(const std::string& ad, int /*argc*/, char** /*argv*/)
{
    if (ad == "a3_hil_duraklat") {
        (void)test_a3_hil_duraklat();
        return g_hata_a3;
    }
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
    } else {
        return -1;   // bu dosyanın testi değil
    }
    return g_hata_a3;
}
