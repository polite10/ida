// W4-duraklat — çalıştırmanın duraklatılması, sürdürülmesi ve adımlanması: bütünleme testleri
// (docs/gorevler/W4-duraklat.md kabul 1–5). Emniyet testleri önce nötr dışı bir komutun yürürlükte
// olduğunu doğrular. Duraklatılmış sınırın her turu `a3_sinir_kancasi`'nı (b, tur) ile çağırır; istekler
// belirli turlarda verilir (SIL belirlenimcidir: turun içeriği duvar saatine bağlı değildir).
//
//   duraklat_sil_esitlik      kabul 4 ve tek yol: müdahalesiz duraklat/adımla/sürdür koşusunun CSV ve
//                             iki yöndeki A1 akışı duraklatmasız koşuyla bayt bayt aynı (sim zamanı donar,
//                             sınır bir kez işlenir); müdahaleli iki koşu birbiriyle aynı
//   duraklat_senaryo_operator kabul 2 (sol-1 #1): senaryo b'de bırakır, duraklatmada operatör basar:
//                             basılı kalır, senaryonun bırakması yeniden girmez
//   duraklat_ayni_tur_cikis   kabul 2 (sol-2 #1): aynı turda acil bas + sürdür/adımla: fizik eski girdiyle
//                             tek adım koşmaz (SIL ve HIL); Tablo 4.7b'nin HIL kuralları
//   duraklat_kisitlayan_ardisik kabul 2 (sol-2 #2): duraklatmada DISARM → DISARM → acil bas gerçek
//                             kitaplık yolundan (HTTP): kilitlenme ve gecikme yok (SIL ve HIL)
//   duraklat_hil_oturum_sonu  kabul 2 (sol-2 #3): HIL duraklatmasında kartın SESSION_CLOSE'u: revizyon
//                             hemen artar, aynı turdaki ve sonraki eski revizyonlu gevşetme reddedilir
//   duraklat_hil_esz          kabul 2 (sol-2 #3): HIL duraklatmasında kartın eşzamansız güvenli çıktıları
//                             duran turlarda uygulanır (SAFETY_CLOSE: sim.kopru.duran_kapanis_esz)
//   duraklat_hil_ilk_tf       kabul 2 (sol-2 #4): ilk TF'den önce uzun HIL duraklatması: dünya iş
//                             parçacığının canlı belleği sabit
//   duraklat_bas_birak        kabul 2 (W4-K3): duraklatmada bas → bırak → sürdür: eski komut geri gelmez
//   duraklat_hil_tel          kabul 3: HIL duraklatmasında telde yalnız IDA_KEEPALIVE; biriken komutlar
//                             sürdürmedeki ilk sınırda; depo doluyken kısıtlayan ve sürdür alınır
//   duraklat_ayirma           kabul 5 (W4-K27 (5)): duraklatma, adımlama ve duraklatılmışken verilen
//                             isteklerle dünya tarafındaki A3 hizmetinde bellek ayırma 0
//   duraklat_taban_ozet       kabul 4: duraklatma hiç kullanılmayan koşuların özetleri (bu iş öncesiyle
//                             elle karşılaştırılır; ölçüt değil, kayıt)
#include "a3_istemci.hpp"
#include "ida/a3/komut.hpp"
#include "ida/sunucu/sunucu.hpp"
#include "ida_link/tel.h"
#include "../../sim/kopru/src/mavlink_ic.hpp"   // yalnız mesaj yapıları ve sabitleri (A1-K101)
#include "../../sim/kopru/test/zamanlama_olcutu.hpp"   // W4-K24, W4-K29

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

// test_a3.cpp'nin ayırma kancaları (aynı sınama programı).
void a3_ayirma_sifirla();
std::uint64_t a3_ayirma_sayisi();
void dunya_isaretle();
long long dunya_canli_bayt();

namespace {

int g_hata_d = 0;

void dogrula_d(bool kosul, const std::string& ileti)
{
    std::printf("%s %s\n", kosul ? "GECTI " : "KALDI ", ileti.c_str());
    if (!kosul) {
        ++g_hata_d;
    }
}

using ida::a3::AlimSonucu;
using ida::a3::CalistirmaDurumu;
using ida::a3::Komut;
using ida::a3::KomutTuru;

constexpr std::int64_t kH = 1'250'000;

std::uint64_t fnv(const std::uint8_t* p, std::size_t n)
{
    std::uint64_t h = 14695981039346656037ULL;
    for (std::size_t i = 0; i < n; ++i) {
        h ^= p[i];
        h *= 1099511628211ULL;
    }
    return h;
}

struct Kosu {
    ida::sunucu::Sonuc s;
    std::vector<std::uint8_t> giden, gelen;
    std::vector<double> sapma;          // b'ye göre fiziğe giden en büyük |pwm − 1500| (−1: gözlem yok)
    std::vector<std::uint8_t> acil;     // b'ye göre fiziğe giden adımda acil durdurma
    std::uint64_t gozlem_tekrari = 0;   // aynı b için birden çok gözlem (fizik adımı başına bir olmalı)
};

ida::sunucu::Ayar yerlesik(double sure)
{
    ida::sunucu::Ayar a;
    a.gunluk = 0;
    a.sure_s = sure;
    a.sure_verildi = true;
    a.csv_uret = true;
    return a;
}

ida::sunucu::Ayar hil_pty(double sure)
{
    ida::sunucu::Ayar a = yerlesik(sure);
    a.kip = ida::sunucu::Kip::hil;
    a.karsi = ida::sunucu::Karsi::pty;
    a.karsi_verildi = true;
    a.csv_uret = false;
    return a;
}

// A3 açık koşu; A1 baytları ve fizik adımı başına gözlem toplanır. `kanca` her turun başında.
// `bayt_topla` false: A1 baytları toplanmaz (dünya iş parçacığının belleğini ölçen sınama).
Kosu kos(ida::sunucu::Ayar a, bool a3, std::function<void(std::uint64_t, ida::a3::KomutAlicisi&)> kanca = {},
         bool bayt_topla = true)
{
    Kosu k;
    auto bayt = a.a1_bayt_kancasi;
    if (bayt_topla || bayt) {
        a.a1_bayt_kancasi = [&k, bayt, bayt_topla](bool giden, const std::uint8_t* p, std::size_t n) {
            if (bayt_topla) {
                auto& v = giden ? k.giden : k.gelen;
                v.insert(v.end(), p, p + n);
            }
            if (bayt) {
                bayt(giden, p, n);
            }
        };
    }
    a.sinir_kancasi = [&k](const ida::sunucu::Ayar::SinirGozlemi& g) {
        double m = 0.0;
        for (double p : g.pwm) {
            m = std::max(m, std::fabs(p - 1500.0));
        }
        if (k.sapma.size() <= g.b) {
            k.sapma.resize(g.b + 1u, -1.0);
            k.acil.resize(g.b + 1u, 0);
        }
        if (k.sapma[g.b] >= 0.0) {
            ++k.gozlem_tekrari;
        }
        k.sapma[g.b] = m;
        k.acil[g.b] = g.acil_durdurma ? 1u : 0u;
    };
    if (a3) {
        a.a3_acik = true;
        a.a3_port = 0;
        a.a3_kapanis_bekleme_s = 0.0;
        a.a3_sinir_kancasi = kanca;
    }
    k.s = ida::sunucu::kos(a);
    return k;
}

bool notr_disi(const Kosu& k, std::uint64_t b) { return b < k.sapma.size() && k.sapma[b] > 0.0; }

bool notr_araligi(const Kosu& k, std::uint64_t ilk, std::uint64_t son)
{
    for (std::uint64_t b = ilk; b < son; ++b) {
        if (b >= k.sapma.size() || k.sapma[b] != 0.0) {
            std::printf("  sınır %llu: nötr değil (%.1f µs)\n", static_cast<unsigned long long>(b),
                        b < k.sapma.size() ? k.sapma[b] : -1.0);
            return false;
        }
    }
    return true;
}

// Müdahalesiz kontrol koşusunda [ilk, son) aralığında en az bir nötr dışı adım var mı (inceleme #3:
// emniyet testinin "nötr" iddiası boş geçmesin diye aynı kurulumun müdahalesiz koşusu sınanır).
bool notr_disi_var(const Kosu& k, std::uint64_t ilk, std::uint64_t son)
{
    for (std::uint64_t b = ilk; b < son; ++b) {
        if (notr_disi(k, b)) {
            return true;
        }
    }
    return false;
}

bool acil_araligi(const Kosu& k, std::uint64_t ilk, std::uint64_t son)
{
    for (std::uint64_t b = ilk; b < son; ++b) {
        if (b >= k.acil.size() || k.acil[b] != 1u) {
            return false;
        }
    }
    return true;
}

// Aynı sınırın turlarını sayar (kanca her turun başında; hazırlıkta da b = 0 ile çağrılır).
struct Tur {
    std::uint64_t b = UINT64_MAX;
    int tur = 0;
    void gor(std::uint64_t yeni)
    {
        if (yeni == b) {
            ++tur;
        } else {
            b = yeni;
            tur = 0;
        }
    }
    bool bu(std::uint64_t bb, int tt) const { return b == bb && tur == tt; }
};

std::uint32_t g_no = 7000;

Komut calistirma(const ida::a3::AnlikDurum& a, KomutTuru t)
{
    Komut k;
    k.komut_no = ++g_no;
    k.istemci_id = 9;
    k.tur = t;
    k.beklenen_run_id = a.run_id;
    k.beklenen_durum_rev = a.durum_rev;
    return k;
}

Komut duraklat(const ida::a3::AnlikDurum& a, bool onay = false)
{
    Komut k = calistirma(a, KomutTuru::calistirma_duraklat);
    k.gecersizligi_onayla = onay;
    return k;
}

Komut adimla(const ida::a3::AnlikDurum& a, std::uint32_t n)
{
    Komut k = calistirma(a, KomutTuru::calistirma_adimla);
    k.periyot_sayisi = n;
    return k;
}

Komut emniyet(const ida::a3::AnlikDurum& a, ida::a3::EmniyetGirdisi g, int tup, ida::a3::GirdiDurumu hedef, bool kisitlayan)
{
    Komut k;
    k.komut_no = ++g_no;
    k.istemci_id = 9;
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

Komut acil_bas(const ida::a3::AnlikDurum& a)
{
    return emniyet(a, ida::a3::EmniyetGirdisi::acil_durdurma, -1, ida::a3::GirdiDurumu::basili, true);
}

Komut acil_birak(const ida::a3::AnlikDurum& a)
{
    return emniyet(a, ida::a3::EmniyetGirdisi::acil_durdurma, -1, ida::a3::GirdiDurumu::serbest, false);
}

Komut yi(const ida::a3::AnlikDurum& a, const std::string& ad, ida::a3::KomutSinifi sinif, const std::string& par)
{
    Komut k;
    k.komut_no = ++g_no;
    k.istemci_id = 9;
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

// Verilir ve kuyruğa alındığı doğrulanır; komut numarası döner.
std::uint32_t ver(ida::a3::KomutAlicisi& al, const Komut& k, const std::string& ne)
{
    const AlimSonucu r = al.al(k);
    dogrula_d(r.tur == AlimSonucu::Tur::kuyrukta, ne + " kuyrukta" + (r.tur == AlimSonucu::Tur::ret ? " (" + r.hata_kodu + ")" : ""));
    return k.komut_no;
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

bool uygulandi(const ida::sunucu::Sonuc& s, std::uint32_t no, std::int64_t t_ns)
{
    const auto* o = a3_sonucu(s, no);
    if (o == nullptr || o->sonuc != "uygulandi" || o->t_ns != t_ns) {
        std::printf("  komut %u: %s %s t=%lld (beklenen uygulandi t=%lld)\n", no, o ? o->sonuc.c_str() : "sonuç yok",
                    o ? o->neden.c_str() : "", o ? static_cast<long long>(o->t_ns) : -1LL, static_cast<long long>(t_ns));
        return false;
    }
    return true;
}

// sim → kart çerçevelerinin mesaj kimlikleri (ida_link, kart rolünde çözülür).
struct Cozucu {
    ida_link_alici_t a{};
    Cozucu() { (void)ida_link_alici_kur(&a, IDA_LINK_ROL_KART); }
    template <typename F>
    void besle(const std::uint8_t* p, std::size_t n, F&& f)
    {
        std::size_t k = 0;
        while (k < n) {
            std::size_t tuk = 0;
            ida_link_cerceve_t c{};
            const ida_link_rc_t rc = ida_link_al(&a, p + k, n - k, &tuk, &c);
            k += tuk;
            if (rc == IDA_LINK_CERCEVE) {
                f(c);
            }
            if (tuk == 0u) {
                break;
            }
        }
    }
};

// ---------------------------------------------------------------- kabul 4 ve tek yol

// Müdahalesiz duraklatma: kontrol sınırı olmayan (2001) ve olan (4000) sınırlarda; adımlama 3 ve 1
// periyot; sürdür. Sim zamanı duraklatmada donar, sınır bir kez işlenir: çıktı duraklatmasızla aynı.
std::function<void(std::uint64_t, ida::a3::KomutAlicisi&)> mudahalesiz(Tur& tr, std::vector<std::string>& hata,
                                                                      std::vector<std::uint32_t>& nolar)
{
    return [&tr, &hata, &nolar](std::uint64_t b, ida::a3::KomutAlicisi& al) {
        tr.gor(b);
        const ida::a3::AnlikDurum d = al.anlik();
        const bool durakli = d.durum == CalistirmaDurumu::duraklatildi;
        if (durakli && d.sim_t_ns != static_cast<std::int64_t>(b) * kH) {
            hata.push_back("duraklatılmışken sim zamanı sınırın zamanı değil (" + std::to_string(b) + ")");
        }
        auto komut = [&](const Komut& k) {
            const AlimSonucu r = al.al(k);
            if (r.tur != AlimSonucu::Tur::kuyrukta) {
                hata.push_back("komut kuyruğa girmedi: " + r.hata_kodu);
            }
            nolar.push_back(k.komut_no);
        };
        if (tr.bu(2001, 0)) {
            komut(duraklat(d));
        } else if (tr.bu(2001, 5)) {
            if (!durakli) {
                hata.push_back("2001: duraklatılmış değil");
            }
            komut(adimla(d, 3));
        } else if (tr.bu(2013, 3)) {
            if (!durakli) {
                hata.push_back("2013: adımlama sonunda duraklatılmış değil");
            }
            komut(adimla(d, 1));
        } else if (tr.bu(2017, 2)) {
            if (!durakli) {
                hata.push_back("2017: adımlama sonunda duraklatılmış değil");
            }
            komut(calistirma(d, KomutTuru::calistirma_surdur));
        } else if (tr.bu(4000, 0)) {
            komut(duraklat(d));
        } else if (tr.bu(4000, 4)) {
            komut(calistirma(d, KomutTuru::calistirma_surdur));
        }
    };
}

// Müdahaleli: duraklatılmışken acil bas/bırak, daraltıcı ve nötr yer istasyonu komutu, adımlama.
std::function<void(std::uint64_t, ida::a3::KomutAlicisi&)> mudahaleli(Tur& tr)
{
    return [&tr](std::uint64_t b, ida::a3::KomutAlicisi& al) {
        tr.gor(b);
        const ida::a3::AnlikDurum d = al.anlik();
        if (tr.bu(4001, 0)) {
            (void)al.al(duraklat(d));
        } else if (tr.bu(4001, 2)) {
            (void)al.al(acil_bas(d));
        } else if (tr.bu(4001, 3)) {
            (void)al.al(yi(d, "disarm", ida::a3::KomutSinifi::daraltici, "{}"));
            (void)al.al(yi(d, "param_get", ida::a3::KomutSinifi::notr, "{\"param_id\":1}"));
        } else if (tr.bu(4001, 5)) {
            (void)al.al(acil_birak(d));
        } else if (tr.bu(4001, 7)) {
            (void)al.al(adimla(d, 2));
        } else if (tr.bu(4009, 2)) {
            (void)al.al(calistirma(d, KomutTuru::calistirma_surdur));
        }
    };
}

void esitlik(const Kosu& a, const Kosu& b, const std::string& ad)
{
    dogrula_d(a.s.tamam && b.s.tamam, ad + ": iki koşu tamam (" + a.s.hata + "|" + b.s.hata + ")");
    dogrula_d(!a.s.csv.empty() && a.s.csv == b.s.csv, ad + ": CSV bayt bayt aynı (" + std::to_string(a.s.csv.size()) + " B)");
    dogrula_d(!a.giden.empty() && a.giden == b.giden, ad + ": sim → karşı taraf A1 akışı aynı (" + std::to_string(a.giden.size()) + " B)");
    dogrula_d(!a.gelen.empty() && a.gelen == b.gelen, ad + ": karşı taraf → sim A1 akışı aynı (" + std::to_string(a.gelen.size()) + " B)");
}

int test_sil_esitlik()
{
    const Kosu taban = kos(yerlesik(8.0), true);
    Tur t1;
    std::vector<std::string> h1;
    std::vector<std::uint32_t> n1;
    const Kosu d1 = kos(yerlesik(8.0), true, mudahalesiz(t1, h1, n1));
    esitlik(taban, d1, "müdahalesiz duraklat/adımla/sürdür ↔ duraklatmasız");
    dogrula_d(h1.empty(), "duraklatma turlarında sim zamanı sınırda dondu; adımlama sonunda yeniden duraklatıldı" +
                              (h1.empty() ? std::string() : " (" + h1.front() + ")"));
    bool hepsi = n1.size() == 6u;
    for (const std::uint32_t no : n1) {
        hepsi = hepsi && a3_sonucu(d1.s, no) != nullptr && a3_sonucu(d1.s, no)->sonuc == "uygulandi";
    }
    dogrula_d(hepsi, "altı çalıştırma denetimi uygulandı (" + std::to_string(n1.size()) + ")");
    dogrula_d(d1.gozlem_tekrari == 0u, "fizik adımı başına tek gözlem (duran turlar fizik girdisi bildirmez)");
    dogrula_d(d1.s.calistirma_sonu == "bitti" && (d1.s.sayac.calistirma_gecersiz & ida::kopru::kGecDuraklatma) == 0u,
              "SIL duraklatması çalıştırmayı geçersiz kılmaz (Tablo 4.8 bit 11 SIL'de —); bitti");
    Tur t2;
    Tur t3;
    const Kosu m1 = kos(yerlesik(8.0), true, mudahaleli(t2));
    const Kosu m2 = kos(yerlesik(8.0), true, mudahaleli(t3));
    esitlik(m1, m2, "müdahaleli duraklatma, iki koşu");
    dogrula_d(m1.s.csv != taban.s.csv, "müdahaleler çıktıyı değiştirdi (sınama anlamlı)");
    return 0;
}

// ---------------------------------------------------------------- kabul 2

// Sol-1 #1: yerleşik z1, senaryo 5 s'de basar, 6 s'de (sınır 4800) bırakır. Sınır 4800'de duraklat:
// senaryonun bırakması ilerleyen turda bir kez uygulanır; duraklatmada operatör basar; sürdürmeden
// sonra basılı kalır, itki nötr; senaryonun gevşetmesi yeniden girmez (gevşetme reddi yok).
int test_senaryo_operator()
{
    ida::sunucu::Ayar a = yerlesik(8.0);
    a.acil_durdurma_s = 5.0;
    a.acil_kalkar_s = 6.0;
    a.acil_verildi = a.acil_kalk_verildi = true;
    // `operator_basar` false: aynı kurulumun müdahalesiz kontrol koşusu (duraklat ve sürdür var,
    // operatörün basması yok).
    auto kos_bu = [&a](bool operator_basar, std::uint32_t& no_dur, std::uint32_t& no_bas, bool& birakti) {
        Tur tr;
        return kos(a, true, [&, operator_basar](std::uint64_t b, ida::a3::KomutAlicisi& al) {
            tr.gor(b);
            const ida::a3::AnlikDurum d = al.anlik();
            if (tr.bu(4800, 0)) {
                no_dur = ver(al, duraklat(d), "4800: duraklat");
            } else if (tr.bu(4800, 2)) {
                birakti = d.durum == CalistirmaDurumu::duraklatildi && d.acil_durdurma == 0u;
                if (operator_basar) {
                    no_bas = ver(al, acil_bas(d), "duraklatmada operatör acil bas");
                }
            } else if (tr.bu(4800, 4)) {
                (void)ver(al, calistirma(d, KomutTuru::calistirma_surdur), "sürdür");
            }
        });
    };
    std::uint32_t no_bas = 0, no_dur = 0, kno_dur = 0, kno_bas = 0;
    bool birakti = false, kbirakti = false;
    const Kosu kontrol = kos_bu(false, kno_dur, kno_bas, kbirakti);
    const Kosu k = kos_bu(true, no_dur, no_bas, birakti);
    dogrula_d(kontrol.s.tamam && k.s.tamam, "koşular tamam " + kontrol.s.hata + k.s.hata);
    dogrula_d(notr_disi(k, 3999), "ön koşul: senaryonun basmasından önce nötr dışı komut yürürlükte");
    dogrula_d(notr_disi_var(kontrol, 4800, 6400) && !acil_araligi(kontrol, 4800, 6400),
              "ön koşul: müdahalesiz kontrol koşusunda (operatör basmıyor) 4800–6400 aralığında itki nötr dışına döner");
    dogrula_d(birakti, "duraklatmada: senaryonun bırakması sınır 4800'ün ilerleyen turunda uygulanmış");
    dogrula_d(uygulandi(k.s, no_dur, 6'000'000'000) && uygulandi(k.s, no_bas, 6'000'000'000),
              "duraklat ve operatörün basması sınır 4800'de (6 s) uygulandı");
    dogrula_d(acil_araligi(k, 4800, 6400), "sürdürmeden sonra acil durdurma basılı kaldı (sınır 4800–6400)");
    dogrula_d(notr_araligi(k, 4800, 6400), "itki nötr");
    dogrula_d(k.s.gevsetme_retleri.empty(), "senaryonun gevşetmesi yeniden girmedi (gevşetme reddi yok: sınır bir kez işlendi)");
    std::size_t senaryo_birak = 0, operator_bas = 0;
    for (const auto& e : k.s.emniyet_degisimleri) {
        senaryo_birak += e.alan == "acil_durdurma" && e.onceki && !e.sonraki && e.kaynak == "senaryo" && e.t_ns == 6'000'000'000 ? 1u : 0u;
        operator_bas += e.alan == "acil_durdurma" && !e.onceki && e.sonraki && e.kaynak == "operator" && e.t_ns == 6'000'000'000 ? 1u : 0u;
    }
    dogrula_d(senaryo_birak == 1u && operator_bas == 1u, "değişimler: senaryo bıraktı, operatör bastı (ikisi 6 s)");
    return 0;
}

// Sol-2 #1: aynı duran turda acil bas + sürdür (ya da adımla). Fizik adımı b, o turun girdisiyle
// (nötr) koşar: eski (nötr dışı) girdiyle tek adım bile koşmaz.
int test_ayni_tur_cikis()
{
    for (const bool adimlama : {false, true}) {
        const std::string ad = adimlama ? "SIL acil + adımla" : "SIL acil + sürdür";
        // `basar` false: aynı kurulumun müdahalesiz kontrol koşusu (acil bas yok).
        auto kos_bu = [&](bool basar, std::uint32_t& no_bas) {
            Tur tr;
            return kos(yerlesik(7.0), true, [&, basar](std::uint64_t b, ida::a3::KomutAlicisi& al) {
                tr.gor(b);
                const ida::a3::AnlikDurum d = al.anlik();
                if (tr.bu(4001, 0)) {
                    (void)ver(al, duraklat(d), ad + ": duraklat");
                } else if (tr.bu(4001, 3)) {
                    if (basar) {
                        no_bas = ver(al, acil_bas(d), ad + ": acil bas");
                    }
                    (void)ver(al, adimlama ? adimla(d, 2) : calistirma(d, KomutTuru::calistirma_surdur), ad + ": çıkış");
                } else if (adimlama && tr.bu(4009, 2)) {
                    (void)ver(al, calistirma(d, KomutTuru::calistirma_surdur), ad + ": sürdür");
                }
            });
        };
        std::uint32_t no_bas = 0, kno_bas = 0;
        const Kosu kontrol = kos_bu(false, kno_bas);
        const Kosu k = kos_bu(true, no_bas);
        dogrula_d(kontrol.s.tamam && k.s.tamam, ad + ": koşular tamam " + kontrol.s.hata + k.s.hata);
        dogrula_d(notr_disi(k, 4000), ad + ": ön koşul: duraklatmadan önceki adımda nötr dışı komut yürürlükte");
        dogrula_d(notr_disi(kontrol, 4001), ad + ": ön koşul: müdahalesiz kontrol koşusunda çıkış adımı (4001) nötr dışı");
        dogrula_d(k.acil.size() > 4001u && k.acil[4001] == 1u && notr_araligi(k, 4001, 4041),
                  ad + ": çıkıştan sonraki ilk fizik adımı (4001) ve sonrası nötr, acil etkin");
        dogrula_d(uygulandi(k.s, no_bas, 4001 * kH), ad + ": acil bas sınır 4001'de uygulandı");
    }
    // HIL: aynı tur, sürdür (adımla HIL'de kipte geçersiz; duraklat onay ister).
    // Sözde uçbirim yalnız POSIX'te: Windows'ta HIL kolu (kontrol koşusu dahil) atlanır; SIL kolu koşar.
#if defined(_WIN32)
    std::printf("  atlandı: HIL acil + sürdür (sözde uçbirim Windows'ta yok)\n");
#else
    {
        const std::string ad = "HIL acil + sürdür";
        // `basar` false: aynı kurulumun müdahalesiz kontrol koşusu (onaylı duraklat ve sürdür var,
        // acil bas yok).
        auto kos_bu = [&](bool basar, std::uint32_t& no_bas, std::string& ret_onay, std::string& ret_adimla) {
            Tur tr;
            return kos(hil_pty(6.0), true, [&, basar](std::uint64_t b, ida::a3::KomutAlicisi& al) {
                tr.gor(b);
                const ida::a3::AnlikDurum d = al.anlik();
                if (tr.bu(4000, 0)) {
                    const AlimSonucu r = al.al(duraklat(d, false));
                    ret_onay = r.tur == AlimSonucu::Tur::ret ? r.hata_kodu : "kabul";
                    (void)ver(al, duraklat(d, true), ad + ": onaylı duraklat");
                } else if (tr.bu(4000, 3)) {
                    const AlimSonucu r = al.al(adimla(d, 1));
                    ret_adimla = r.tur == AlimSonucu::Tur::ret ? r.hata_kodu : "kabul";
                    if (basar) {
                        no_bas = ver(al, acil_bas(d), ad + ": acil bas");
                    }
                    (void)ver(al, calistirma(d, KomutTuru::calistirma_surdur), ad + ": sürdür");
                }
            });
        };
        std::uint32_t no_bas = 0, kno_bas = 0;
        std::string ret_onay, ret_adimla, kret_onay, kret_adimla;
        const Kosu kontrol = kos_bu(false, kno_bas, kret_onay, kret_adimla);
        const Kosu k = kos_bu(true, no_bas, ret_onay, ret_adimla);
        dogrula_d(kontrol.s.tamam && kontrol.s.calistirma_sonu == "bitti", ad + ": kontrol koşusu tamam ve bitti " + kontrol.s.hata);
        dogrula_d(notr_disi(kontrol, 4000), ad + ": ön koşul: müdahalesiz kontrol koşusunda çıkış adımı (4000) nötr dışı");
        dogrula_d(k.s.tamam && k.s.calistirma_sonu == "bitti", ad + ": koşu tamam ve bitti " + k.s.hata);
        dogrula_d(ret_onay == "onay_gerekli", ad + ": onaysız duraklat onay_gerekli (Tablo 4.7b): " + ret_onay);
        dogrula_d(ret_adimla == "kipte_gecersiz", ad + ": HIL'de adımla kipte_gecersiz (Tablo 4.7b): " + ret_adimla);
        dogrula_d(notr_disi(k, 3999), ad + ": ön koşul: duraklatmadan önceki adımda nötr dışı komut yürürlükte");
        dogrula_d(k.acil.size() > 4000u && k.acil[4000] == 1u && notr_araligi(k, 4000, 4040),
                  ad + ": çıkıştan sonraki ilk fizik adımı (4000) ve sonrası nötr, acil etkin");
        dogrula_d(uygulandi(k.s, no_bas, 4000 * kH), ad + ": acil bas sınır 4000'de uygulandı");
        dogrula_d((k.s.sayac.calistirma_gecersiz & ida::kopru::kGecDuraklatma) != 0u,
                  ad + ": HIL duraklatması çalıştırmayı geçersiz kıldı (A1-K41, K79)");
    }
#endif
    return 0;
}

// HTTP ucu (gerçek kitaplık yolu; test_a3.cpp'deki A3Uc ile aynı biçim).
struct A3Uc {
    std::uint16_t port = 0;
    std::string jeton;
    std::string istemci;
    int istek = 0;
    ida::test::a3::HttpSonuc post(const std::string& yol, const std::string& govde)
    {
        char kimlik[32];
        std::snprintf(kimlik, sizeof kimlik, "%024x", static_cast<unsigned>(++istek) + 0x100000u);
        const std::string g = "{\"istek_kimligi\":\"" + std::string(kimlik) + "\"," + govde.substr(1);
        return ida::test::a3::http_istek(port, jeton, "POST", "/api/v1" + yol, g, istemci);
    }
    bool kaydol()
    {
        const auto y = ida::test::a3::http_istek(port, jeton, "POST", "/api/v1/istemciler", "{\"ad\":\"duraklat\"}");
        const std::size_t k = y.govde.find("\"istemci_id\":");
        if ((y.durum != 200 && y.durum != 201) || k == std::string::npos) {
            return false;
        }
        istemci = "Ida-Istemci: " + std::to_string(std::atoi(y.govde.c_str() + k + 13)) + "\r\n";
        return true;
    }
};

// Sol-2 #2: duraklatmada DISARM → DISARM → acil bas, HTTP'den. İkinci DISARM kitaplığın mandalına
// düşebilir; acil bas onun arkasında beklemez: istekler döndükten sonraki ilk turlarda uygulanır;
// sürdür (genel kuyruktan bağımsız yol) `mesgul` almaz. İlk fizik adımı nötr.
int test_kisitlayan_ardisik()
{
    for (const bool hil : {false, true}) {
        const std::string ad = hil ? "HIL" : "SIL";
        ida::sunucu::Ayar a = hil ? hil_pty(6.0) : yerlesik(6.0);
        A3Uc uc;
        a.a3_hazir = [&uc](std::uint16_t p, const std::string& j) {
            uc.port = p;
            uc.jeton = j;
        };
        Tur tr;
        bool kayit = false;
        std::array<ida::test::a3::HttpSonuc, 3> y{};
        ida::test::a3::HttpSonuc sur{};
        int istek_turu = -1, acil_turu = -1;
        std::chrono::steady_clock::time_point t0{}, t1{};
        const Kosu k = kos(a, true, [&](std::uint64_t b, ida::a3::KomutAlicisi& al) {
            tr.gor(b);
            if (!kayit && uc.port != 0u) {
                kayit = uc.kaydol();
            }
            const ida::a3::AnlikDurum d = al.anlik();
            if (tr.bu(4000, 0)) {
                (void)ver(al, duraklat(d, hil), ad + ": duraklat");
            } else if (tr.bu(4000, 2) && kayit && d.durum == CalistirmaDurumu::duraklatildi) {
                const std::string r = "\"" + d.run_id.value_or("") + "\"";
                t0 = std::chrono::steady_clock::now();
                y[0] = uc.post("/yer_istasyonu/komut", "{\"beklenen_run_id\":" + r + ",\"komut\":\"disarm\",\"parametreler\":{}}");
                y[1] = uc.post("/yer_istasyonu/komut", "{\"beklenen_run_id\":" + r + ",\"komut\":\"disarm\",\"parametreler\":{}}");
                y[2] = uc.post("/emniyet/degistir", "{\"beklenen_run_id\":" + r + ",\"girdi\":\"acil_durdurma\",\"hedef_durum\":\"basili\"}");
                istek_turu = tr.tur;
            } else if (b == 4000 && istek_turu >= 0 && acil_turu < 0 && d.acil_durdurma == 1u) {
                acil_turu = tr.tur;
                t1 = std::chrono::steady_clock::now();
            } else if (b == 4000 && acil_turu >= 0 && tr.tur == acil_turu + 3) {
                sur = uc.post("/calistirma/surdur", "{\"beklenen_run_id\":\"" + d.run_id.value_or("") +
                                                        "\",\"beklenen_durum_rev\":" + std::to_string(d.durum_rev) + "}");
            }
        });
        dogrula_d(k.s.tamam && k.s.calistirma_sonu == "bitti", ad + ": koşu tamam ve bitti " + k.s.hata);
        dogrula_d(y[0].durum == 202 && y[1].durum == 202 && y[2].durum == 202,
                  ad + ": üç istek 202 kuyrukta (" + std::to_string(y[0].durum) + "," + std::to_string(y[1].durum) + "," +
                      std::to_string(y[2].durum) + ")");
        dogrula_d(sur.durum == 202, ad + ": sürdür 202 (mesgul değil): " + std::to_string(sur.durum) + " " + sur.kod());
        const std::int64_t t = 4000 * kH;
        dogrula_d(uygulandi(k.s, y[0].komut_no(), t) && uygulandi(k.s, y[1].komut_no(), t) && uygulandi(k.s, y[2].komut_no(), t),
                  ad + ": iki DISARM ve acil bas duraklatılmış sınırda (5 s) uygulandı");
        dogrula_d(acil_turu >= 0 && acil_turu <= istek_turu + 2,
                  ad + ": acil bas istekler döndükten sonra en geç ikinci turda yürürlükte (tur " + std::to_string(acil_turu) +
                      ", istek turu " + std::to_string(istek_turu) + ")");
        const double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
        dogrula_d(ida::test::zamanlama(acil_turu >= 0 && ms < 100.0, "duraklat_kisitlayan_ardisik.gecikme_" + ad,
                                       std::to_string(ms) + " ms (< 100 ms; üç HTTP isteği dahil)"),
                  ad + ": gecikme yok (zamanlama)");
        dogrula_d(k.acil.size() > 4000u && k.acil[4000] == 1u && k.sapma[4000] == 0.0, ad + ": fizik adımı 4000 nötr, acil etkin");
        dogrula_d(k.s.sayac.komut_birlestirildi >= 1u, ad + ": iki DISARM köprüde birleşti (A1-K61; düşmedi)");
    }
    return 0;
}

// Kart rolünde bir çerçeveyi sim'in alım akışına çerçeve sınırında ekleyen taşıma sarmalayıcısı.
class EnjekteTasima final : public ida::kopru::Tasima {
public:
    explicit EnjekteTasima(std::unique_ptr<ida::kopru::Tasima> ic) : ic_(std::move(ic)) {}
    long long yaz_biraz(const std::uint8_t* p, std::size_t n) override { return ic_->yaz_biraz(p, n); }
    ida::kopru::OkumaSonucu oku(std::uint8_t* p, std::size_t n, std::int64_t bekle_us, std::size_t& okunan) override
    {
        okunan = 0;
        if (hazir_.empty()) {
            std::uint8_t g[4096];
            std::size_t k = 0;
            const ida::kopru::OkumaSonucu r = ic_->oku(g, sizeof g, bekle_us, k);
            if (r == ida::kopru::OkumaSonucu::tamam) {
                ham_.insert(ham_.end(), g, g + k);
                ayir();
            }
            {
                std::lock_guard<std::mutex> kl(m_);
                hazir_.insert(hazir_.end(), ek_.begin(), ek_.end());   // tam çerçevelerden sonra
                ek_.clear();
            }
            if (hazir_.empty()) {
                return r == ida::kopru::OkumaSonucu::tamam ? ida::kopru::OkumaSonucu::zaman_asimi : r;
            }
        }
        okunan = std::min(n, hazir_.size());
        std::copy(hazir_.begin(), hazir_.begin() + static_cast<std::ptrdiff_t>(okunan), p);
        hazir_.erase(hazir_.begin(), hazir_.begin() + static_cast<std::ptrdiff_t>(okunan));
        return ida::kopru::OkumaSonucu::tamam;
    }
    void giris_bosalt() override { ic_->giris_bosalt(); }
    bool acik() const override { return ic_->acik(); }
    void kapat() override { ic_->kapat(); }
    void kapat_duzgun(std::int64_t s) override { ic_->kapat_duzgun(s); }
    bool cikis_bosalt(std::int64_t s) override { return ic_->cikis_bosalt(s); }
    std::string ad() const override { return "enjekte(" + ic_->ad() + ")"; }
    void enjekte(const std::uint8_t* p, std::size_t n)
    {
        std::lock_guard<std::mutex> kl(m_);
        ek_.insert(ek_.end(), p, p + n);
    }

private:
    // MAVLink 2 çerçevelerini ayırır (0xFD, uzunluk, imza bayrağı); eksik çerçeve ham_'da kalır.
    void ayir()
    {
        std::size_t i = 0;
        while (i < ham_.size()) {
            if (ham_[i] != 0xFDu) {
                hazir_.push_back(ham_[i]);
                ++i;
                continue;
            }
            if (ham_.size() - i < 3u) {
                break;
            }
            const std::size_t boy = static_cast<std::size_t>(ham_[i + 1]) + 12u + ((ham_[i + 2] & 1u) != 0u ? 13u : 0u);
            if (ham_.size() - i < boy) {
                break;
            }
            hazir_.insert(hazir_.end(), ham_.begin() + static_cast<std::ptrdiff_t>(i),
                          ham_.begin() + static_cast<std::ptrdiff_t>(i + boy));
            i += boy;
        }
        ham_.erase(ham_.begin(), ham_.begin() + static_cast<std::ptrdiff_t>(i));
    }
    std::unique_ptr<ida::kopru::Tasima> ic_;
    std::vector<std::uint8_t> ham_, hazir_, ek_;
    std::mutex m_;
};

// Sol-2 #3 (HIL): duraklatmada kart oturumu kapatır (IDA_SESSION_CLOSE, kart rolünde enjekte); aynı
// turda eski revizyonlu (o anki) gevşetme verilir. O tur: oturum sonu revizyonu artırır (A1-K85 (1)),
// gevşetme kisitlama_hakim ile reddedilir. Sonraki turda aynı eski revizyonla gelen gevşetme girişte
// reddedilir. Koşu sürdürülür ve biter.
int test_hil_oturum_sonu()
{
    ida::sunucu::Ayar a = hil_pty(6.0);
    EnjekteTasima* enj = nullptr;
    a.tasima_sarmalayici = [&enj](std::unique_ptr<ida::kopru::Tasima> t) {
        auto e = std::make_unique<EnjekteTasima>(std::move(t));
        enj = e.get();
        return std::unique_ptr<ida::kopru::Tasima>(std::move(e));
    };
    std::atomic<std::uint16_t> sess{0};
    Cozucu coz;
    a.a1_bayt_kancasi = [&](bool giden, const std::uint8_t* p, std::size_t n) {
        if (!giden) {
            return;
        }
        coz.besle(p, n, [&](const ida_link_cerceve_t& c) {
            if (c.msgid == MAVLINK_MSG_ID_IDA_SESSION_ACCEPT) {
                mavlink_ida_session_accept_t m{};
                if (ida_link_coz(&c, c.msgid, &m, sizeof m) == IDA_LINK_TAMAM && m.result == 0u) {
                    sess = m.sess;
                }
            }
        });
    };
    Tur tr;
    std::uint32_t rev0 = 0, rev_sonra = 0, no_gev = 0;
    std::string ikinci;
    const Kosu k = kos(a, true, [&](std::uint64_t b, ida::a3::KomutAlicisi& al) {
        tr.gor(b);
        const ida::a3::AnlikDurum d = al.anlik();
        if (tr.bu(4000, 0)) {
            (void)ver(al, duraklat(d, true), "HIL duraklat (onaylı)");
        } else if (tr.bu(4000, 3) && enj != nullptr) {
            rev0 = d.emniyet_rev;
            mavlink_ida_session_close_t m{};
            m.sess = sess.load();
            m.reason = static_cast<std::uint8_t>(IDAW_CLOSE_REASON_SHUTDOWN);
            ida_link_gonderici_t g{};
            (void)ida_link_gonderici_kur(&g, IDA_LINK_ROL_KART);
            std::uint8_t cer[IDA_LINK_CERCEVE_AZAMI];
            std::size_t n = 0;
            if (ida_link_gonder(&g, MAVLINK_MSG_ID_IDA_SESSION_CLOSE, &m, sizeof m, cer, sizeof cer, &n) == IDA_LINK_TAMAM) {
                enj->enjekte(cer, n);
            }
            no_gev = ver(al, emniyet(d, ida::a3::EmniyetGirdisi::emniyet_pimi, 1, ida::a3::GirdiDurumu::cikarilmis, false),
                         "aynı turda gevşetme (o anki revizyon)");
        } else if (tr.bu(4000, 6)) {
            rev_sonra = d.emniyet_rev;
            Komut g = emniyet(d, ida::a3::EmniyetGirdisi::emniyet_pimi, 1, ida::a3::GirdiDurumu::cikarilmis, false);
            g.beklenen_emniyet_rev = rev0;
            const AlimSonucu r = al.al(g);
            ikinci = r.tur == AlimSonucu::Tur::ret ? r.hata_kodu : "kabul";
        } else if (tr.bu(4000, 40)) {
            (void)ver(al, calistirma(d, KomutTuru::calistirma_surdur), "sürdür");
        }
    });
    dogrula_d(k.s.tamam && k.s.calistirma_sonu == "bitti", "HIL koşusu tamam ve bitti " + k.s.hata);
    dogrula_d(sess.load() != 0u && rev0 != 0u, "ön koşul: oturum kuruldu, revizyon okundu");
    dogrula_d(rev_sonra >= rev0 + 1u, "kartın kapanışı duran turda revizyonu artırdı (" + std::to_string(rev0) + " → " +
                                          std::to_string(rev_sonra) + ")");
    const auto* o = a3_sonucu(k.s, no_gev);
    dogrula_d(o != nullptr && o->sonuc == "reddedildi" && o->neden == "kisitlama_hakim" && o->t_ns == 4000 * kH,
              "aynı turdaki gevşetme kisitlama_hakim ile reddedildi (" + (o ? o->sonuc + " " + o->neden : std::string("yok")) + ")");
    dogrula_d(ikinci == "emniyet_rev_uyusmuyor", "sonraki turda eski revizyonlu gevşetme reddedildi: " + ikinci);
    dogrula_d(k.s.sayac.oturum_kopma >= 1u || k.s.sayac.oturum_kurulan >= 2u, "oturum bitti ve yeniden kuruldu");
    return 0;
}

// Sol-2 #3 (HIL; sunucu düzeyi): duraklatmada (0,5 s) kartın eşzamansız güvenli çıktıları duran
// turlarda uygulanır; fizik adımı nötr girdiyle sürer. (SAFETY_CLOSE: köprü testi duran_kapanis_esz;
// Z1'de izin açılmaz.)
int test_hil_esz()
{
    Tur tr;
    std::chrono::steady_clock::time_point t0{};
    bool surdu = false;
    const Kosu k = kos(hil_pty(6.0), true, [&](std::uint64_t b, ida::a3::KomutAlicisi& al) {
        tr.gor(b);
        const ida::a3::AnlikDurum d = al.anlik();
        if (tr.bu(4000, 0)) {
            (void)ver(al, duraklat(d, true), "HIL duraklat (onaylı)");
            t0 = std::chrono::steady_clock::now();
        } else if (b == 4000 && !surdu && std::chrono::steady_clock::now() - t0 > std::chrono::milliseconds(500)) {
            surdu = true;
            (void)ver(al, calistirma(d, KomutTuru::calistirma_surdur), "sürdür");
        }
    });
    const ida::kopru::Sayaclar& c = k.s.sayac;
    std::printf("  eşzamansız güvenli: uygulanan %llu, birleşen %llu, iptal %llu\n",
                static_cast<unsigned long long>(c.esz_guvenli_uygulanan), static_cast<unsigned long long>(c.esz_birlesen),
                static_cast<unsigned long long>(c.yanit_iptal_esz));
    dogrula_d(k.s.tamam && k.s.calistirma_sonu == "bitti", "HIL koşusu tamam ve bitti " + k.s.hata);
    dogrula_d(notr_disi(k, 3999), "ön koşul: duraklatmadan önce nötr dışı komut yürürlükte");
    dogrula_d(c.esz_guvenli_uygulanan >= 1u, "duraklatmada kartın eşzamansız güvenli çıktısı uygulandı (A1-K78)");
    dogrula_d(k.sapma.size() > 4000u && k.sapma[4000] == 0.0, "sürdürmeden sonraki ilk fizik adımı nötr girdiyle");
    return 0;
}

// Sol-2 #4 (HIL): ilk TF'den önce (sınır 0) 3 s duraklatma. Kart 10 ms ızgarasında batch_seq = 0
// eşzamansız güvenli çıktılar üretir; dünya iş parçacığının canlı belleği ısınmadan (0,5 s) sonra
// büyümez. Sürdürmeden sonra koşu biter. Eşzamansız çıktı muafiyeti (W4-K6) yalnız duraklatma
// penceresinde denetlenir (inceleme #1): sürdürmeden sonra kartın batch_seq ≠ 0 taşıyan eşzamansız
// çıktısı Tablo 4.8 bit 6'yı meşru olarak kurabilir.
int test_hil_ilk_tf()
{
    Tur tr;
    std::chrono::steady_clock::time_point t0{};
    bool duraklatildi = false, isindi = false, surdu = false;
    long long b0 = 0, b1 = 0;
    std::uint64_t tf_once = 0;
    std::uint32_t gec_surdur = 0;   // sürdürme anındaki görüntünün Tablo 4.8 bitleri
    // Karşı taraf → sim eşzamansız çıktıları (ayırmasız çözücü; dünya iş parçacığında).
    std::atomic<bool> surdu_a{false};
    std::uint64_t esz_once = 0, esz_once_bagli = 0, esz_sifir = 0, esz_bagli = 0;
    ida_link_alici_t gelen_alici{};
    (void)ida_link_alici_kur(&gelen_alici, IDA_LINK_ROL_SIM);
    ida::sunucu::Ayar a = hil_pty(2.0);
    a.a1_bayt_kancasi = [&](bool giden, const std::uint8_t* p, std::size_t n) {
        if (giden) {
            return;
        }
        std::size_t i = 0;
        while (i < n) {
            std::size_t tuk = 0;
            ida_link_cerceve_t c{};
            const ida_link_rc_t rc = ida_link_al(&gelen_alici, p + i, n - i, &tuk, &c);
            i += tuk;
            if (rc == IDA_LINK_CERCEVE && c.msgid == MAVLINK_MSG_ID_IDA_ACTUATOR) {
                mavlink_ida_actuator_t m{};
                if (ida_link_coz(&c, c.msgid, &m, sizeof m) == IDA_LINK_TAMAM && (m.act_flags & IDAW_ACT_FLAGS_ASYNC) != 0u) {
                    const bool sifir = m.batch_seq == 0u;
                    (sifir ? esz_sifir : esz_bagli) += 1u;
                    if (!surdu_a.load()) {
                        ++esz_once;
                        esz_once_bagli += sifir ? 0u : 1u;
                    }
                }
            }
            if (tuk == 0u) {
                break;
            }
        }
    };
    const Kosu k = kos(a, true, [&](std::uint64_t b, ida::a3::KomutAlicisi& al) {
        dunya_isaretle();
        tr.gor(b);
        const ida::a3::AnlikDurum d = al.anlik();
        if (!duraklatildi && b == 0 && d.durum == CalistirmaDurumu::kosuyor) {
            duraklatildi = true;
            (void)ver(al, duraklat(d, true), "sınır 0'da duraklat (ilk TF öncesi)");
            t0 = std::chrono::steady_clock::now();
            tf_once = static_cast<std::uint64_t>(d.sayaclar.empty() ? 0 : d.sayaclar.front().second);
            return;
        }
        if (!duraklatildi || surdu || b != 0) {
            return;
        }
        const auto gecen = std::chrono::steady_clock::now() - t0;
        if (!isindi && gecen > std::chrono::milliseconds(500)) {
            isindi = true;
            b0 = dunya_canli_bayt();
        } else if (gecen > std::chrono::milliseconds(3500)) {
            b1 = dunya_canli_bayt();
            surdu = true;
            gec_surdur = d.gecerlilik_nedenleri;
            surdu_a = true;
            (void)ver(al, calistirma(d, KomutTuru::calistirma_surdur), "sürdür");
        }
    }, false);
    const ida::kopru::Sayaclar& c = k.s.sayac;
    std::printf("  ilk TF öncesi duraklatma: kart çıktıları birleşen %llu, uygulanan %llu (ilk parti öncesi %llu); dünya "
                "canlı belleği %lld → %lld B; alınan eşzamansız: sürdürmeden önce %llu (bağlı %llu), toplam batch_seq 0 "
                "%llu, bağlı %llu\n",
                static_cast<unsigned long long>(c.esz_birlesen), static_cast<unsigned long long>(c.esz_guvenli_uygulanan),
                static_cast<unsigned long long>(c.eszamansiz_ilk_parti_oncesi), b0, b1, static_cast<unsigned long long>(esz_once),
                static_cast<unsigned long long>(esz_once_bagli), static_cast<unsigned long long>(esz_sifir),
                static_cast<unsigned long long>(esz_bagli));
    dogrula_d(k.s.tamam && k.s.calistirma_sonu == "bitti", "HIL koşusu tamam ve bitti " + k.s.hata);
    dogrula_d(surdu && tf_once == 0u, "ön koşul: sınır 0'da, hiç parti gönderilmeden duraklatıldı ve 3 s sonra sürdürüldü");
    dogrula_d(c.esz_birlesen + c.esz_guvenli_uygulanan >= 100u && esz_once >= 100u,
              "ön koşul: duraklatmada (sürdürmeden önce) kart çıktıları geldi (≥ 100)");
    // Duraklatma penceresi: sürdürmeden önce alınan bütün eşzamansız çıktılar ilk parti öncesidir
    // (batch_seq = 0) ve sürdürme anında bit 6 (eşzamansız güvenli çıktı) kurulmamıştır.
    dogrula_d(esz_once_bagli == 0u && (gec_surdur & (1u << 6)) == 0u,
              "duraklatma penceresinde alınan çıktılar ilk parti öncesi, muaf sayıldı (W4-K6; Tablo 4.8 bit 6 sürdürme "
              "anında kurulu değil; bitler 0x" + [&] { char b[16]; std::snprintf(b, sizeof b, "%x", gec_surdur); return std::string(b); }() + ")");
    // Muaf sayılan ≤ alınan batch_seq = 0 çıktılar; muaf olmayan uygulananlar yalnız bağlı (sürdürmeden
    // sonra gelen) çıktılardan.
    dogrula_d(c.eszamansiz_ilk_parti_oncesi >= 1u && c.eszamansiz_ilk_parti_oncesi <= esz_sifir &&
                  c.esz_guvenli_uygulanan - c.eszamansiz_ilk_parti_oncesi <= esz_bagli,
              "ilk parti öncesi çıktılar uygulandı ve muaf sayıldı; muaf olmayanlar yalnız sürdürmeden sonraki bağlı çıktılar");
#if !defined(_MSC_VER)
    dogrula_d(b1 - b0 < 64 * 1024, "dünya iş parçacığının canlı belleği sabit (0,5 s → 3,5 s artış " + std::to_string(b1 - b0) + " B)");
#endif
    dogrula_d(k.s.zaman.tf_sayisi >= 300u, "sürdürmeden sonra TF'ler aktı (" + std::to_string(k.s.zaman.tf_sayisi) + ")");
    return 0;
}

// W4-K3: duraklatmada bas → bırak (güncel revizyonla) → sürdür. Bırakma uygulandı ama basmadan önce
// gönderilmiş partilerin yanıtları (yuvaları ileride) geri gelmez: sürdürmeden sonraki ilk TF'nin
// (4004) yanıtı uygulanana dek (4012) itki nötr.
int test_bas_birak()
{
    // `mudahale` false: aynı kurulumun müdahalesiz kontrol koşusu (duraklat ve sürdür var; bas ve
    // bırak yok).
    auto kos_bu = [](bool mudahale, std::uint32_t& no_bas, std::uint32_t& no_bir) {
        Tur tr;
        return kos(yerlesik(7.0), true, [&, mudahale](std::uint64_t b, ida::a3::KomutAlicisi& al) {
            tr.gor(b);
            const ida::a3::AnlikDurum d = al.anlik();
            if (tr.bu(4001, 0)) {
                (void)ver(al, duraklat(d), "duraklat");
            } else if (mudahale && tr.bu(4001, 2)) {
                no_bas = ver(al, acil_bas(d), "bas");
            } else if (mudahale && tr.bu(4001, 4)) {
                no_bir = ver(al, acil_birak(d), "bırak (güncel revizyon)");
            } else if (tr.bu(4001, 6)) {
                (void)ver(al, calistirma(d, KomutTuru::calistirma_surdur), "sürdür");
            }
        });
    };
    std::uint32_t no_bas = 0, no_bir = 0, kno_bas = 0, kno_bir = 0;
    const Kosu kontrol = kos_bu(false, kno_bas, kno_bir);
    const Kosu k = kos_bu(true, no_bas, no_bir);
    dogrula_d(kontrol.s.tamam && k.s.tamam, "koşular tamam " + kontrol.s.hata + k.s.hata);
    dogrula_d(notr_disi(k, 4000), "ön koşul: duraklatmadan önce nötr dışı komut yürürlükte");
    dogrula_d(notr_disi(kontrol, 4001), "ön koşul: müdahalesiz kontrol koşusunda çıkış adımı (4001) nötr dışı");
    dogrula_d(uygulandi(k.s, no_bas, 4001 * kH) && uygulandi(k.s, no_bir, 4001 * kH), "bas ve bırak duraklatmada uygulandı");
    dogrula_d(k.acil.size() > 4001u && k.acil[4001] == 0u, "sürdürmede acil serbest");
    dogrula_d(notr_araligi(k, 4001, 4012), "eski komut geri gelmedi: 4001–4011 nötr (W4-K3)");
    return 0;
}

// Kabul 3 (HIL): duraklatma boyunca sim → kart telinde yalnız IDA_KEEPALIVE. Duraklatılmışken verilen
// daraltıcı (DISARM) ve nötr (param_get) komutları sürdürmeden sonra; DISARM ilk sınırda (ilk TF'den
// önce). Depo (a3_genel_ + genel halka) dolunca nötr istek `mesgul` (dolu) alır; acil bas, FC_DISARM ve
// sürdür yine kabul edilir.
int test_hil_tel()
{
    ida::sunucu::Ayar a = hil_pty(6.0);
    std::atomic<int> faz{0};   // 0 duraklatmadan önce, 1 duraklatma turları, 2 sürdürmeden sonra
    struct Cerceve {
        int faz;
        std::uint32_t msgid;
        std::uint8_t tip;   // CMD_SIMPLE ise type
    };
    std::vector<Cerceve> cerceveler;
    Cozucu coz;
    a.a1_bayt_kancasi = [&](bool giden, const std::uint8_t* p, std::size_t n) {
        if (!giden) {
            return;
        }
        coz.besle(p, n, [&](const ida_link_cerceve_t& c) {
            std::uint8_t tip = 0;
            if (c.msgid == MAVLINK_MSG_ID_IDA_CMD_SIMPLE) {
                mavlink_ida_cmd_simple_t m{};
                if (ida_link_coz(&c, c.msgid, &m, sizeof m) == IDA_LINK_TAMAM) {
                    tip = m.type;
                }
            }
            cerceveler.push_back({faz.load(), c.msgid, tip});
        });
    };
    Tur tr;
    std::chrono::steady_clock::time_point t0{};
    int dolu = 0, kuyrukta_notr = 0;
    bool tam_dolu = false;
    std::uint32_t no_dis = 0, no_pg = 0, no_acil = 0, no_fcd = 0;
    bool surdu = false, dolu_iken_alindi = false;
    const Kosu k = kos(a, true, [&](std::uint64_t b, ida::a3::KomutAlicisi& al) {
        tr.gor(b);
        const ida::a3::AnlikDurum d = al.anlik();
        if (b > 4000u) {
            faz = 2;
            return;
        }
        if (tr.bu(4000, 0)) {
            (void)ver(al, duraklat(d, true), "HIL duraklat (onaylı)");
            t0 = std::chrono::steady_clock::now();
            return;
        }
        if (b != 4000u || surdu) {
            return;
        }
        faz = 1;
        if (tr.tur == 2) {
            no_dis = ver(al, yi(d, "disarm", ida::a3::KomutSinifi::daraltici, "{}"), "duraklatmada DISARM");
            no_pg = ver(al, yi(d, "param_get", ida::a3::KomutSinifi::notr, "{\"param_id\":1}"), "duraklatmada param_get");
        } else if (tr.tur >= 3 && !tam_dolu) {
            // Depoyu doldur: nötr, köprüye dokunmayan `manual` (desteklenmiyor) istekleri. Her turda
            // halka (256) dolana dek; bir turun ilk isteği de dolu alınca depo tamamen doludur
            // (köprüye bu duraklatmada verilen 256 + a3_genel_ 512 + halka 256).
            for (int i = 0; i < 300; ++i) {
                const AlimSonucu r = al.al(yi(d, "manual", ida::a3::KomutSinifi::notr, "{\"axes\":[0,0,0,0,0,0]}"));
                if (r.tur == AlimSonucu::Tur::kuyrukta) {
                    ++kuyrukta_notr;
                    continue;
                }
                dolu += r.tur == AlimSonucu::Tur::dolu ? 1 : 0;
                tam_dolu = i == 0 && r.tur == AlimSonucu::Tur::dolu;
                break;
            }
            if (tam_dolu) {
                no_acil = ver(al, acil_bas(d), "depo doluyken acil bas");
                no_fcd = ver(al, yi(d, "fc_disarm", ida::a3::KomutSinifi::daraltici, "{}"), "depo doluyken FC_DISARM");
                dolu_iken_alindi = true;
            }
        } else if (tam_dolu && std::chrono::steady_clock::now() - t0 > std::chrono::milliseconds(400)) {
            surdu = true;
            (void)ver(al, calistirma(d, KomutTuru::calistirma_surdur), "depo doluyken sürdür");
        }
    });
    dogrula_d(k.s.tamam && k.s.calistirma_sonu == "bitti", "HIL koşusu tamam ve bitti " + k.s.hata);
    std::size_t f1 = 0, f1_disi = 0;
    bool dis_tf_once = false, tf_goruldu = false, pg_sonra = false;
    for (const Cerceve& c : cerceveler) {
        if (c.faz == 1) {
            ++f1;
            f1_disi += c.msgid != MAVLINK_MSG_ID_IDA_KEEPALIVE ? 1u : 0u;
        }
        if (c.faz == 2) {
            if (c.msgid == MAVLINK_MSG_ID_IDA_TIME_FRAME) {
                tf_goruldu = true;
            }
            if (c.msgid == MAVLINK_MSG_ID_IDA_CMD_SIMPLE && c.tip == IDAW_CMD_TYPE_DISARM && !tf_goruldu) {
                dis_tf_once = true;
            }
            if (c.msgid == MAVLINK_MSG_ID_IDA_CMD_SIMPLE && c.tip == IDAW_CMD_TYPE_PARAM_GET) {
                pg_sonra = true;
            }
        }
    }
    dogrula_d(f1 >= 2u && f1_disi == 0u, "duraklatma boyunca telde yalnız IDA_KEEPALIVE (" + std::to_string(f1) + " çerçeve, " +
                                             std::to_string(f1_disi) + " başka)");
    dogrula_d(dis_tf_once, "biriken DISARM sürdürmedeki ilk sınırda, ilk TF'den önce gönderildi");
    dogrula_d(pg_sonra, "biriken param_get sürdürmeden sonra gönderildi");
    dogrula_d(uygulandi(k.s, no_dis, 4000 * kH) && uygulandi(k.s, no_pg, 4000 * kH),
              "A3-K58: duraklatılmışken verilen komutlar beklemeden uygulandı (uygulama adımı 4000)");
    // Sınır: bu duraklatmada köprüye 256 (param_get dahil) + a3_genel_ 512 + genel halka 256.
    dogrula_d(tam_dolu && kuyrukta_notr + 1 == 256 + 512 + 256 && dolu_iken_alindi,
              "depo doldu: nötr istek dolu (mesgul) aldı (" + std::to_string(kuyrukta_notr) + " + param_get kuyrukta = 256 + 512 + "
              "256); kısıtlayanlar ve sürdür alındı");
    dogrula_d(uygulandi(k.s, no_acil, 4000 * kH) && uygulandi(k.s, no_fcd, 4000 * kH),
              "depo doluyken acil bas ve FC_DISARM duraklatmada uygulandı");
    return 0;
}

// Kabul 5 (W4-K27 (5)): duraklatma, adımlama, sürdürme ve duraklatılmışken verilen istekler (emniyet,
// daraltıcı, nötr, genişletici yer istasyonu komutu, hız) dünya tarafındaki A3 hizmetinde bellek
// ayırmaz (operator new kancası; köprü ve fizik çağrıları hariç).
int test_ayirma()
{
    Tur tr;
    a3_ayirma_sifirla();
    const Kosu k = kos(yerlesik(5.0), true, [&](std::uint64_t b, ida::a3::KomutAlicisi& al) {
        tr.gor(b);
        const ida::a3::AnlikDurum d = al.anlik();
        if (tr.bu(2000, 0)) {
            (void)al.al(duraklat(d));
        } else if (tr.bu(2000, 2)) {
            (void)al.al(acil_bas(d));
            (void)al.al(yi(d, "disarm", ida::a3::KomutSinifi::daraltici, "{}"));
            (void)al.al(yi(d, "param_get", ida::a3::KomutSinifi::notr, "{\"param_id\":1}"));
        } else if (tr.bu(2000, 4)) {
            (void)al.al(acil_birak(d));
            (void)al.al(yi(d, "arm", ida::a3::KomutSinifi::genisletici, "{}"));
            Komut h = calistirma(d, KomutTuru::calistirma_hiz);
            h.carpan = 0.0;
            (void)al.al(h);
        } else if (tr.bu(2000, 6)) {
            (void)al.al(adimla(d, 4));
        } else if (tr.bu(2016, 2)) {
            (void)al.al(calistirma(d, KomutTuru::calistirma_surdur));
        }
    });
    const std::uint64_t ayirma = a3_ayirma_sayisi();
    dogrula_d(k.s.tamam && k.s.calistirma_sonu == "bitti", "koşu tamam ve bitti " + k.s.hata);
#if !defined(_MSC_VER)
    dogrula_d(ayirma == 0u, "dünya tarafındaki A3 hizmetinde bellek ayırma yok (" + std::to_string(ayirma) + ")");
#else
    (void)ayirma;
#endif
    std::printf("  ÖLÇÜM (bu Mac, SIL): dünya tarafındaki A3 hizmetinin sınır/tur başına en uzun süresi %.1f µs\n",
                1e-3 * static_cast<double>(k.s.a3_sinir_en_kotu_ns));
    return 0;
}

// ---------------------------------------------------------------- kabul 4 (bu iş öncesiyle)

void ozet_yaz(const char* ad, const Kosu& k)
{
    std::printf("OZET %-14s tamam=%d csv=%zu/%016llx giden=%zu/%016llx gelen=%zu/%016llx estop_adim=%llu ihlal=%llu\n", ad,
                k.s.tamam ? 1 : 0, k.s.csv.size(),
                static_cast<unsigned long long>(fnv(reinterpret_cast<const std::uint8_t*>(k.s.csv.data()), k.s.csv.size())),
                k.giden.size(), static_cast<unsigned long long>(fnv(k.giden.data(), k.giden.size())), k.gelen.size(),
                static_cast<unsigned long long>(fnv(k.gelen.data(), k.gelen.size())),
                static_cast<unsigned long long>(k.s.estop_adim), static_cast<unsigned long long>(k.s.ihlal_estop));
}

// Duraklatma hiç kullanılmayan koşular. Özetler bu iş öncesindeki derlemenin özetleriyle elle
// karşılaştırılır (platforma bağlı kayan nokta: özet sabitleri teste gömülmez).
int test_taban_ozet()
{
    ozet_yaz("a3_kapali", kos(yerlesik(12.0), false));
    ozet_yaz("a3_acik", kos(yerlesik(12.0), true));
    ozet_yaz("a3_panel", kos(yerlesik(12.0), true, [](std::uint64_t b, ida::a3::KomutAlicisi& al) {
        const ida::a3::AnlikDurum a = al.anlik();
        if (b == 4000) {
            (void)al.al(acil_bas(a));
        }
        if (b == 4800) {
            (void)al.al(acil_birak(a));
        }
        if (b == 6000) {
            (void)al.al(yi(a, "disarm", ida::a3::KomutSinifi::daraltici, "{}"));
        }
        if (b == 6400) {
            (void)al.al(yi(a, "manual", ida::a3::KomutSinifi::notr, "{\"axes\":[0,0,0,0,0,0]}"));
        }
    }));
    ida::sunucu::Ayar s = yerlesik(12.0);
    s.acil_durdurma_s = 5.0;
    s.acil_kalkar_s = 6.0;
    s.acil_verildi = s.acil_kalk_verildi = true;
    ozet_yaz("senaryo_acil", kos(s, true));
    return 0;
}

}  // namespace

int duraklat_testi(const std::string& ad, int /*argc*/, char** /*argv*/)
{
    if (ad == "duraklat_taban_ozet") {
        (void)test_taban_ozet();
    } else if (ad == "duraklat_sil_esitlik") {
        (void)test_sil_esitlik();
    } else if (ad == "duraklat_senaryo_operator") {
        (void)test_senaryo_operator();
    } else if (ad == "duraklat_ayni_tur_cikis") {
        (void)test_ayni_tur_cikis();
    } else if (ad == "duraklat_kisitlayan_ardisik") {
        (void)test_kisitlayan_ardisik();
    } else if (ad == "duraklat_hil_oturum_sonu") {
        (void)test_hil_oturum_sonu();
    } else if (ad == "duraklat_hil_esz") {
        (void)test_hil_esz();
    } else if (ad == "duraklat_hil_ilk_tf") {
        (void)test_hil_ilk_tf();
    } else if (ad == "duraklat_bas_birak") {
        (void)test_bas_birak();
    } else if (ad == "duraklat_hil_tel") {
        (void)test_hil_tel();
    } else if (ad == "duraklat_ayirma") {
        (void)test_ayirma();
    } else {
        return -1;   // bu dosyanın testi değil
    }
    return g_hata_d;
}
