// sol incelemesinin (docs/gorevler/inceleme/W3-fizik-sol.md) dört kritik bulgusunun karşı
// örnekleri.
#include "test_destek.hpp"

#include "itici.hpp"

#include <limits>

namespace ida::fizik::test {

namespace {

std::array<double, kIticiSayisi> notr8()
{
    std::array<double, kIticiSayisi> p{};
    p.fill(1500.0);
    return p;
}

}  // namespace

// Bulgu 1: sürekli zamanda kararlı ama hızlı bir K(s) (τ = 0,1 ms; 1,25 ms adımda h λ = −12,5)
// kabul edilir; itki, birinci dereceden filtrenin analitik basamak yanıtına uymalıdır:
// F(t) = F_ss (1 − e^{−t/τ}). Yalnız RK4'le ilerletilen filtrede büyütme çarpanı ≈ 758 olur.
int test_itici_hizli_filtre(const TestBaglami&)
{
    Denetci d;
    // Tolerans 1e-12 bağıl: tam ayrıklaştırmada (matris üsteli) yalnız yuvarlama kalır.
    const double kBagil = 1e-12;
    for (double tau : {1e-4, 0.05}) {
        TestAyari ta;
        ta.notr_yuzerlik = true;
        AracParametreleri a = test_araci(ta);
        a.itici_modeli.transfer_pay = {1.0};
        a.itici_modeli.transfer_payda = {tau, 1.0};
        Dunya dn;
        if (!kur(dn, a, havuz_ortami(), baslangic(10.0, {}))) return 1;
        ic::IticiModeliIc m;
        m.kur(a.itici_modeli, a.esc);
        const double fss = m.durgun_itki(1700.0, a.batarya.v_sabit);
        auto p = notr8();
        p[4] = 1700.0;
        dn.itici_komutu_ver(0, p);
        for (int n = 1; n <= 40; ++n) {
            if (dn.adim() != AdimDurumu::tamam) return 1;
            const double t = static_cast<double>(dn.zaman_ns()) * 1e-9;
            const double beklenen = fss * (1.0 - std::exp(-t / tau));
            if (n <= 2 || n == 40) {
                std::printf("  τ = %g s, t = %.5f s: F = %.12g N, analitik %.12g N\n", tau, t,
                            dn.durum().itki[4], beklenen);
            }
            YAKIN(d, dn.durum().itki[4], beklenen, kBagil, 1e-12, "hızlı filtre basamak yanıtı");
        }
    }
    return d.hata;
}

// Bulgu 2a: ESC yalnız nötr komut (A5 §3.3: pwm_notr_us ± olu_bant_us) kesintisiz
// baslatma_suresi_ns boyunca tutulunca hazır olur; baştan sürekli nötr dışı komutla hiç
// hazır olmaz.
int test_esc_notr_disi_hazir_olmaz(const TestBaglami&)
{
    Denetci d;
    const AracParametreleri a = ida0_arac();  // baslatma_suresi_ns = 2 s
    Dunya dn;
    if (!kur(dn, a, havuz_ortami(), baslangic(5.0, {}))) return 1;
    auto p = notr8();
    p[4] = 1700.0;
    dn.itici_komutu_ver(0, p);
    for (std::uint64_t i = 0; i < adim_sayisi(10.0); ++i) {
        if (dn.adim() != AdimDurumu::tamam) return 1;
        if (dn.durum().itki_istegi[4] != 0.0 || dn.durum().itki[4] != 0.0) {
            DOGRULA(d, false, "nötr beklenmeden ESC itki verdi (t = " +
                                  std::to_string(static_cast<double>(dn.zaman_ns()) * 1e-9) + " s)");
            break;
        }
    }
    std::printf("  10 s sürekli 1700 µs: F_ss = %.6f N, F = %.6f N\n", dn.durum().itki_istegi[4],
                dn.durum().itki[4]);
    return d.hata;
}

// Bulgu 2b: yeterli nötr beklemeden sonra hazır olur; kesintide süre baştan sayılır; ölü
// bant içindeki komut nötr sayılır; kanallar bağımsızdır. Sınırlar adım düzeyinde sınanır
// (1,25 ms adım, başlatma 2 s = 1600 adım).
int test_esc_notr_bekleme_sonrasi_hazir(const TestBaglami&)
{
    Denetci d;
    const AracParametreleri a = ida0_arac();
    const std::uint64_t B = adim_sayisi(2.0);    // 1600
    const std::uint64_t k1 = adim_sayisi(1.0);   // 800: kanal 4 ve 6'da tek adımlık kesinti
    const std::uint64_t h4 = k1 + 1 + B;         // 2401: kanal 4'ün hazır olduğu ilk adım
    // Komut planı (nötr dışı komut 1700 µs):
    //   kanal 0: adım B'den (2,0 s) itibaren 1700      -> B'den itibaren itki
    //   kanal 1: adım B − 1'den itibaren 1700           -> hiç itki yok (1,99875 s nötr yetmez)
    //   kanal 4: k1'de 1700, sonra 1525 µs (ölü bant kenarı, nötr), h4'ten itibaren 1700
    //            -> h4'ten itibaren itki
    //   kanal 6: aynı, ama h4 − 1'den itibaren 1700     -> hiç itki yok
    //   kanal 5: 2,5 s'den itibaren 1700                -> 2,5 s'den itibaren itki
    auto plan = [&](std::uint64_t k) {
        auto p = notr8();
        if (k >= B) p[0] = 1700.0;
        if (k + 1 >= B) p[1] = 1700.0;
        p[4] = (k == k1 || k >= h4) ? 1700.0 : (k > k1 ? 1525.0 : 1500.0);
        p[6] = (k == k1 || k + 1 >= h4) ? 1700.0 : (k > k1 ? 1525.0 : 1500.0);
        if (k >= adim_sayisi(2.5)) p[5] = 1700.0;
        return p;
    };
    Dunya dn;
    if (!kur(dn, a, havuz_ortami(), baslangic(20.0, {}))) return 1;
    for (std::uint64_t i = 0; i < adim_sayisi(4.0); ++i) {
        const std::uint64_t k = dn.adim_no();
        dn.itici_komutu_ver(k, plan(k));
        if (dn.adim() != AdimDurumu::tamam) return 1;
        const auto& an = dn.durum();
        const std::string adim = ", adım " + std::to_string(k);
        DOGRULA(d, (an.itki_istegi[0] != 0.0) == (k >= B), "kanal 0" + adim);
        DOGRULA(d, an.itki_istegi[1] == 0.0, "kanal 1 (nötr 1 adım eksik) itki verdi" + adim);
        DOGRULA(d, (an.itki_istegi[4] != 0.0) == (k >= h4), "kanal 4" + adim);
        DOGRULA(d, an.itki_istegi[6] == 0.0, "kanal 6 (nötr 1 adım eksik) itki verdi" + adim);
        DOGRULA(d, (an.itki_istegi[5] != 0.0) == (k >= adim_sayisi(2.5)), "kanal 5" + adim);
        if (d.hata > 10) break;
    }
    std::printf("  4 s: F_ss = (%.4f, %.4f, %.4f, %.4f, %.4f) N (kanal 0, 1, 4, 5, 6)\n",
                dn.durum().itki_istegi[0], dn.durum().itki_istegi[1], dn.durum().itki_istegi[4],
                dn.durum().itki_istegi[5], dn.durum().itki_istegi[6]);
    return d.hata;
}

// Bulgu 3: sonlu olmayan parametreler ve sonlu girdilerden taşma kurulumda reddedilir;
// kurulum hiçbir zaman sonlu olmayan bir anlık görüntüyle başarı döndürmez.
int test_kurulum_sonluluk(const TestBaglami&)
{
    Denetci d;
    const double inf = std::numeric_limits<double>::infinity();
    const double nan = std::numeric_limits<double>::quiet_NaN();
    struct Durum {
        const char* ad;
        AracParametreleri a;
        BaslangicDurumu b;
    };
    std::vector<Durum> durumlar;
    auto ekle = [&](const char* ad, auto degistir) {
        Durum s{ad, ida0_arac(), baslangic(1.0, {"f1", "f2"})};
        degistir(s);
        durumlar.push_back(s);
    };
    ekle("dogrusal_sonum[0] = +inf", [&](Durum& s) { s.a.hidrodinamik.dogrusal_sonum[0] = inf; });
    ekle("karesel_sonum[2] = +inf", [&](Durum& s) { s.a.hidrodinamik.karesel_sonum[2] = inf; });
    ekle("eklenmis_kutle[1] = +inf", [&](Durum& s) { s.a.hidrodinamik.eklenmis_kutle[1] = inf; });
    ekle("r_g.x = NaN", [&](Durum& s) { s.a.kutle.r_g[0] = nan; });
    ekle("r_b.z = NaN", [&](Durum& s) { s.a.kutle.r_b[2] = nan; });
    ekle("trim_konumu = inf", [&](Durum& s) {
        s.a.kutle.trim_kutlesi = 0.5;
        s.a.kutle.trim_konumu = {inf, 0.0, 0.0};
    });
    ekle("yuzey_sonum_orani = +inf", [&](Durum& s) { s.a.hidrodinamik.yuzey_sonum_orani[0] = inf; });
    ekle("azami_itki_ileri = +inf", [&](Durum& s) { s.a.itici_modeli.azami_itki_ileri[1] = inf; });
    ekle("havalanma_beta = NaN", [&](Durum& s) { s.a.itici_modeli.havalanma_beta[1] = nan; });
    ekle("pervane_yaricapi = +inf", [&](Durum& s) { s.a.itici_modeli.pervane_yaricapi = inf; });
    ekle("torpido.kutle = +inf", [&](Durum& s) { s.a.torpido.kutle = inf; });
    // Sonlu girdilerden taşma
    ekle("taşma: D_L 1e308 · u 10", [&](Durum& s) {
        s.a.hidrodinamik.dogrusal_sonum[0] = 1e308;
        s.b.hiz = {10.0, 0, 0, 0, 0, 0};
    });
    ekle("taşma: D_NL 1e300 · u|u| 1e10", [&](Durum& s) {
        s.a.hidrodinamik.karesel_sonum[1] = 1e300;
        s.b.hiz = {0, 1e10, 0, 0, 0, 0};
    });
    ekle("taşma: kütle 1e308 · g", [&](Durum& s) { s.a.kutle.kutle = 1e308; });
    for (const auto& s : durumlar) {
        Dunya dn;
        const KurulumSonucu r = dn.kur(s.a, havuz_ortami(), s.b);
        std::printf("  %-30s -> %s\n", s.ad, r.tamam ? "(kabul edildi)" : r.hata.c_str());
        DOGRULA(d, !r.tamam, std::string("kabul edildi: ") + s.ad);
        if (r.tamam) {
            const auto& an = dn.durum();
            bool sonlu = std::isfinite(an.batik_oran);
            for (double x : an.dogrusal_ivme_b) sonlu = sonlu && std::isfinite(x);
            for (double x : an.ozgul_kuvvet_b) sonlu = sonlu && std::isfinite(x);
            DOGRULA(d, sonlu, std::string("kabul edilen kurulumun anlık görüntüsü sonlu değil: ") + s.ad);
        }
    }
    return d.hata;
}

// Bulgu 4: aynı transfer fonksiyonunun büyük (1e155) ve küçük (1e-200) ortak ölçekli yazımları
// ölçeksiz yazımla aynı sonucu verir (kabul ve itki dizisi); kararsız payda ölçekten bağımsız
// reddedilir.
int test_transfer_olcek(const TestBaglami&)
{
    Denetci d;
    auto kosu = [&](double olcek, std::vector<double>& itki) {
        TestAyari ta;
        ta.notr_yuzerlik = true;
        AracParametreleri a = test_araci(ta);
        for (auto& c : a.itici_modeli.transfer_pay) c *= olcek;
        for (auto& c : a.itici_modeli.transfer_payda) c *= olcek;
        Dunya dn;
        const KurulumSonucu r = dn.kur(a, havuz_ortami(), baslangic(10.0, {}));
        std::printf("  ölçek %g: %s\n", olcek, r.tamam ? "kabul" : r.hata.c_str());
        if (!r.tamam) return false;
        auto p = notr8();
        p[4] = 1700.0;
        dn.itici_komutu_ver(0, p);
        for (int i = 0; i < 400; ++i) {
            dn.adim();
            itki.push_back(dn.durum().itki[4]);
        }
        return true;
    };
    std::vector<double> t1, tb, tk;
    const bool k1 = kosu(1.0, t1);
    const bool kb = kosu(1e155, tb);
    const bool kk = kosu(1e-200, tk);
    DOGRULA(d, k1 && kb && kk, "eşdeğer transfer fonksiyonu reddedildi");
    if (k1 && kb && kk) {
        // Tolerans 1e-12 bağıl: katsayılar baş katsayıya bölünürken yalnız yuvarlama farkı.
        for (std::size_t i = 0; i < t1.size(); ++i) {
            YAKIN(d, tb[i], t1[i], 1e-12, 1e-12, "1e155 ölçekli itki");
            YAKIN(d, tk[i], t1[i], 1e-12, 1e-12, "1e-200 ölçekli itki");
        }
    }
    for (double olcek : {1.0, 1e155, 1e-200}) {
        AracParametreleri a = ida0_arac();
        a.itici_modeli.transfer_pay = {1.0 * olcek, 10.0 * olcek};
        a.itici_modeli.transfer_payda = {1.0 * olcek, 1.0 * olcek, 1.0 * olcek, 10.0 * olcek};  // a1 a2 < a3
        Dunya dn;
        const KurulumSonucu r = dn.kur(a, havuz_ortami(), baslangic(1.0, {}));
        std::printf("  kararsız, ölçek %g: %s\n", olcek, r.tamam ? "(kabul edildi)" : r.hata.c_str());
        DOGRULA(d, !r.tamam && r.hata.find("Routh") != std::string::npos, "kararsız payda reddedilmedi");
    }
    return d.hata;
}

}  // namespace ida::fizik::test
