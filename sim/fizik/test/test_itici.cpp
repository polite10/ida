// İtici modeli, kabul 7 (itici geometrisi) ve kabul 3 (son hız).
#include "test_destek.hpp"

#include "itici.hpp"

#include <Eigen/Dense>

namespace ida::fizik::test {

namespace {

std::array<double, kIticiSayisi> notr(const AracParametreleri& a)
{
    std::array<double, kIticiSayisi> p{};
    p.fill(static_cast<double>(a.esc.pwm_notr_us));
    return p;
}

// Bağımsız referans: K(s)'in gözlenebilir kanonik biçimi, ince adımlı RK4 ile basamak yanıtı.
double referans_basamak(const IticiModeli& m, double t)
{
    const auto& pay = m.transfer_pay;
    const auto& payda = m.transfer_payda;
    const int n = static_cast<int>(payda.size()) - 1;
    std::vector<double> a(static_cast<std::size_t>(n + 1)), b(static_cast<std::size_t>(n + 1), 0.0);
    for (int i = 0; i <= n; ++i) a[static_cast<std::size_t>(i)] = payda[static_cast<std::size_t>(i)] / payda[0];
    // b[i]: s^(n−i) katsayısı (pay sağa yaslanır)
    const int np = static_cast<int>(pay.size());
    for (int i = 0; i < np; ++i) {
        b[static_cast<std::size_t>(n + 1 - np + i)] = pay[static_cast<std::size_t>(i)] / payda[0];
    }
    // Gözlenebilir kanonik: ẋ_i = x_{i+1} − a_{i+1} y + b_{i+1} u (i < n), y = x_1 (b_0 = 0)
    auto turev = [&](const std::vector<double>& x, std::vector<double>& dx) {
        const double y = x[0];
        for (int i = 0; i < n; ++i) {
            const auto u = static_cast<std::size_t>(i);
            dx[u] = (i + 1 < n ? x[u + 1] : 0.0) - a[u + 1] * y + b[u + 1] * 1.0;
        }
    };
    const int N = 200000;
    const double h = t / N;
    std::vector<double> x(static_cast<std::size_t>(n), 0.0), k1 = x, k2 = x, k3 = x, k4 = x, y = x;
    for (int s = 0; s < N; ++s) {
        turev(x, k1);
        for (int i = 0; i < n; ++i) y[static_cast<std::size_t>(i)] = x[static_cast<std::size_t>(i)] + h / 2 * k1[static_cast<std::size_t>(i)];
        turev(y, k2);
        for (int i = 0; i < n; ++i) y[static_cast<std::size_t>(i)] = x[static_cast<std::size_t>(i)] + h / 2 * k2[static_cast<std::size_t>(i)];
        turev(y, k3);
        for (int i = 0; i < n; ++i) y[static_cast<std::size_t>(i)] = x[static_cast<std::size_t>(i)] + h * k3[static_cast<std::size_t>(i)];
        turev(y, k4);
        for (int i = 0; i < n; ++i) {
            const auto u = static_cast<std::size_t>(i);
            x[u] += h / 6 * (k1[u] + 2 * k2[u] + 2 * k3[u] + k4[u]);
        }
    }
    return x[0];
}

}  // namespace

int test_itici_modeli(const TestBaglami&)
{
    Denetci d;
    const AracParametreleri a = ida0_arac();
    ic::IticiModeliIc m;
    const std::string e = m.kur(a.itici_modeli, a.esc);
    DOGRULA(d, e.empty(), "model kurulamadı: " + e);
    if (!e.empty()) return 1;
    const double V = a.batarya.v_sabit;

    // Azami itki: parça başına kuvvet yasası (04 §2.7.2); düğümlerde tablo değeri.
    // Tolerans 1e-12 bağıl: pow/log yuvarlaması.
    const auto& mi = a.itici_modeli;
    YAKIN(d, m.azami_itki(12.0, true), mi.azami_itki_ileri[0], 1e-12, 0.0, "F_maks ileri 12 V");
    YAKIN(d, m.azami_itki(16.0, true), mi.azami_itki_ileri[1], 1e-12, 0.0, "F_maks ileri 16 V");
    YAKIN(d, m.azami_itki(20.0, false), mi.azami_itki_geri[2], 1e-12, 0.0, "F_maks geri 20 V");
    YAKIN(d, m.azami_itki(24.0, true), mi.azami_itki_ileri[2], 0.0, 0.0, "F_maks 20–26 V: 20 V değeri");
    YAKIN(d, m.azami_itki(6.5, true), 0.0, 0.0, 0.0, "F_maks 7 V altı");
    const double p1 = std::log(51.4849 / 36.3827) / std::log(16.0 / 12.0);
    std::printf("  ileri üs 12→16 V: %.4f (04: 1,207)\n", p1);
    YAKIN(d, p1, 1.207, 0.0, 5e-4, "üs 12→16 V");
    YAKIN(d, m.azami_itki(V, true), 36.3827 * std::pow(V / 12.0, p1), 1e-12, 0.0, "F_maks ileri 15,6 V");
    YAKIN(d, m.azami_itki(10.0, true), 36.3827 * std::pow(10.0 / 12.0, p1), 1e-12, 0.0,
          "F_maks 7–12 V dışdeğerleme");

    // Şekil: N = max_j F(j/1000) (A5-K104); monoton kırpılmış; F_şekil(1) ≈ 1.
    auto F = [&](double u) {
        const auto& c = mi.sekil_polinomu;
        return c[0] * u + c[1] * std::pow(u, 3) + c[2] * std::pow(u, 5) + c[3] * std::pow(u, 7) +
               c[4] * std::pow(u, 9);
    };
    double N = 0.0;
    for (int j = 0; j <= 1000; ++j) N = std::max(N, F(j / 1000.0));
    YAKIN(d, m.sekil_boleni(), N, 1e-14, 0.0, "N");
    YAKIN(d, m.sekil(0.5), F(0.5) / N, 1e-14, 0.0, "F_şekil(0,5)");
    YAKIN(d, m.sekil(1.0), 1.0, 1e-6, 0.0, "F_şekil(1)");
    double onceki = 0.0;
    bool monoton = true;
    for (int j = 0; j <= 10000; ++j) {
        const double f = m.sekil(j / 10000.0);
        monoton = monoton && f >= onceki;
        onceki = f;
    }
    DOGRULA(d, monoton, "F_şekil monoton değil");
    DOGRULA(d, F(1.0) < F(0.972), "polinom u ≈ 0,972'den sonra azalmalı (kırpma sınanmıyor)");

    // PWM → u: ölü bant (1500 ± 25 dahil), doyma, ileri–geri asimetrisi.
    YAKIN(d, m.durgun_itki(1500.0, V), 0.0, 0.0, 0.0, "nötr");
    YAKIN(d, m.durgun_itki(1525.0, V), 0.0, 0.0, 0.0, "ölü bant üst kenarı");
    YAKIN(d, m.durgun_itki(1475.0, V), 0.0, 0.0, 0.0, "ölü bant alt kenarı");
    YAKIN(d, m.durgun_itki(1526.0, V), F(26.0 / 400.0) / N * m.azami_itki(V, true), 1e-12, 0.0,
          "ölü bant dışı 1526");
    YAKIN(d, m.durgun_itki(1900.0, V), m.sekil(1.0) * m.azami_itki(V, true), 1e-14, 0.0, "tam ileri");
    YAKIN(d, m.durgun_itki(2100.0, V), m.durgun_itki(1900.0, V), 0.0, 0.0, "doyma ileri");
    YAKIN(d, m.durgun_itki(1100.0, V), -m.sekil(1.0) * m.azami_itki(V, false), 1e-14, 0.0, "tam geri");
    YAKIN(d, m.durgun_itki(900.0, V), m.durgun_itki(1100.0, V), 0.0, 0.0, "doyma geri");
    DOGRULA(d, m.durgun_itki(1700.0, V) > -m.durgun_itki(1300.0, V), "ileri–geri asimetrisi yok");

    // Havalanma β(h/R): doğrusal tablo; yüzeyde 0, h/R ≥ 3,4'te 1.
    YAKIN(d, m.havalanma(-0.01), 0.0, 0.0, 0.0, "β su üstü");
    YAKIN(d, m.havalanma(0.05), 0.05 / 0.038 / 3.4, 1e-14, 0.0, "β(0,05 m)");
    YAKIN(d, m.havalanma(1.0), 1.0, 0.0, 0.0, "β derin");

    // Dinamik: tam batık, nötr yüzerlikte araç; t5'e (kanal 4) basamak. Gerçekleşen itki,
    // K(s)'in bağımsız (gözlenebilir kanonik, 200 000 adımlı RK4) basamak yanıtıyla uyuşur.
    // Tolerans 1e-9 bağıl: fizikte filtre adım başına tam ayrıklaştırılır (matris üsteli);
    // fark referansın kendi RK4 kesme hatası (200 000 adım; h|λ| ≈ 6e-6 düzeyi) ve yuvarlamadır.
    {
        TestAyari ta;
        ta.notr_yuzerlik = true;
        const AracParametreleri at = test_araci(ta);
        Dunya dn;
        if (!kur(dn, at, havuz_ortami(), baslangic(10.0, {}))) return 1;
        auto p = notr(at);
        p[4] = 1700.0;
        dn.itici_komutu_ver(0, p);
        const double fss = m.durgun_itki(1700.0, V);
        for (double t_hedef : {0.0125, 0.05, 0.2, 2.0}) {
            if (!kos(dn, adim_sayisi(t_hedef) - dn.adim_no())) return 1;
            const double beklenen = fss * referans_basamak(at.itici_modeli, t_hedef);
            std::printf("  t = %.4f s: F = %.9f N, referans %.9f N (F_ss %.6f)\n", t_hedef,
                        dn.durum().itki[4], beklenen, fss);
            YAKIN(d, dn.durum().itki[4], beklenen, 1e-9, 1e-12, "basamak yanıtı");
            YAKIN(d, dn.durum().itki_istegi[4], fss, 0.0, 0.0, "F_ss");
        }
    }

    // Yüzeyde havalanma: düz suda O_b 0,05 m derinlikte dikey iticiler h/R = 1,316 → β = 0,387;
    // yataylar (z = 0,085) derin. Kurulum anındaki anlık görüntüden.
    {
        Dunya dn;
        if (!kur(dn, ida0_arac(), havuz_ortami(), baslangic(0.05, {}))) return 1;
        YAKIN(d, dn.durum().havalanma[4], 0.05 / 0.038 / 3.4, 1e-12, 0.0, "β dikey, yüzey");
        YAKIN(d, dn.durum().havalanma[0], 1.0, 0.0, 0.0, "β yatay, yüzey");
    }
    return d.hata;
}

// Kabul 7: yalnız ileri / yan / dikey / dönüş komutları beklenen eksende net kuvvet-moment
// üretir; çapraz bileşenler tolerans altında.
int test_itici_geometrisi(const TestBaglami&)
{
    Denetci d;
    TestAyari ta;
    ta.notr_yuzerlik = true;
    const AracParametreleri a = test_araci(ta);
    ic::IticiModeliIc m;
    m.kur(a.itici_modeli, a.esc);
    const double V = a.batarya.v_sabit;
    const double k = 0.7071067811865475;

    // (a) Tek itici: τ = F [ε ; r × ε]. Beklenen r × ε elle (04 §2.6 tablosundan) yazıldı.
    const Vek6 sutun[kIticiSayisi] = {
        {k, -k, 0, 0.085 * k, 0.085 * k, -0.267 * k},
        {k, k, 0, -0.085 * k, 0.085 * k, 0.267 * k},
        {k, k, 0, -0.085 * k, 0.085 * k, -0.267 * k},
        {k, -k, 0, 0.085 * k, 0.085 * k, 0.267 * k},
        {0, 0, 1, 0.218, -0.120, 0},
        {0, 0, 1, -0.218, -0.120, 0},
        {0, 0, 1, 0.218, 0.120, 0},
        {0, 0, 1, -0.218, 0.120, 0},
    };
    // Tolerans: 1e-12 bağıl + 1e-12 mutlak (yalnız yuvarlama; elle yazılan 0,267 = 0,156 + 0,111
    // ondalıkta tam olmadığından mutlak pay).
    for (int i = 0; i < kIticiSayisi; ++i) {
        Dunya dn;
        if (!kur(dn, a, havuz_ortami(), baslangic(50.0, {}))) return 1;
        auto p = notr(a);
        p[static_cast<std::size_t>(i)] = (i % 2 == 0) ? 1700.0 : 1300.0;
        dn.itici_komutu_ver(0, p);
        if (!kos(dn, adim_sayisi(3.0))) return 1;
        const double Fi = dn.durum().itki[static_cast<std::size_t>(i)];
        YAKIN(d, Fi, m.durgun_itki(p[static_cast<std::size_t>(i)], V), 1e-6, 0.0, "oturmuş itki");
        for (int j = 0; j < 6; ++j) {
            const auto u = static_cast<std::size_t>(j);
            YAKIN(d, dn.durum().itici_kuvvet_momenti[u], Fi * sutun[i][u], 1e-12, 1e-12, "τ sütunu");
        }
    }

    // (b) Saf eksen komutları (en küçük normlu tahsis + itki eğrisinin tersi). Oturmuş τ_itici
    // istenen eksende; çapraz bileşenler |τ_j| ≤ 1e-6 · |τ_d|. Gerekçe: tahsis ve ters eğri
    // yuvarlama düzeyinde tamdır; 1e-6 filtrelerin 3 s'deki oturma kalıntısına (< 1e-9) ve PWM
    // ara değerlemesine geniş pay bırakır.
    const struct {
        const char* ad;
        int eksen;
        double deger;
    } eksenler[] = {{"ileri", 0, 40.0}, {"yan", 1, 25.0},  {"dikey", 2, 40.0},
                    {"yalpa", 3, 3.0},  {"baş-kıç", 4, 4.0}, {"dönüş", 5, 4.0}};
    for (const auto& ex : eksenler) {
        Vek6 tau{};
        tau[static_cast<std::size_t>(ex.eksen)] = ex.deger;
        std::array<double, kIticiSayisi> p{};
        if (!tahsis_pwm(a, tau, p)) {
            DOGRULA(d, false, std::string("tahsis başarısız: ") + ex.ad);
            continue;
        }
        Dunya dn;
        if (!kur(dn, a, havuz_ortami(), baslangic(50.0, {}))) return 1;
        dn.itici_komutu_ver(0, p);
        if (!kos(dn, adim_sayisi(3.0))) return 1;
        const auto& t = dn.durum().itici_kuvvet_momenti;
        std::printf("  %-8s τ = (%.3e %.3e %.3e %.3e %.3e %.3e)\n", ex.ad, t[0], t[1], t[2], t[3], t[4], t[5]);
        for (int j = 0; j < 6; ++j) {
            const auto u = static_cast<std::size_t>(j);
            YAKIN(d, t[u], tau[u], 1e-6, 1e-6 * ex.deger, ex.ad);
        }
    }

    // (c) Ham gruplar: dört dikey aynı PWM → saf F_z (simetri, tam); dört yatay aynı PWM → F_x
    // ile 04 §2.6'daki M = 0,085 X çapraz momenti (yataylar O_b'nin 0,085 m altında).
    {
        Dunya dn;
        if (!kur(dn, a, havuz_ortami(), baslangic(50.0, {}))) return 1;
        auto p = notr(a);
        for (int i = 4; i < 8; ++i) p[static_cast<std::size_t>(i)] = 1650.0;
        dn.itici_komutu_ver(0, p);
        if (!kos(dn, adim_sayisi(3.0))) return 1;
        const auto& t = dn.durum().itici_kuvvet_momenti;
        const double Fz = 4.0 * dn.durum().itki[4];
        YAKIN(d, t[2], Fz, 1e-12, 0.0, "dikey grup F_z");
        for (int j : {0, 1, 3, 4, 5}) YAKIN(d, t[static_cast<std::size_t>(j)], 0.0, 0.0, 1e-12 * Fz, "dikey grup çapraz");
    }
    {
        Dunya dn;
        if (!kur(dn, a, havuz_ortami(), baslangic(50.0, {}))) return 1;
        auto p = notr(a);
        for (int i = 0; i < 4; ++i) p[static_cast<std::size_t>(i)] = 1650.0;
        dn.itici_komutu_ver(0, p);
        if (!kos(dn, adim_sayisi(3.0))) return 1;
        const auto& t = dn.durum().itici_kuvvet_momenti;
        const double Fx = 4.0 * k * dn.durum().itki[0];
        YAKIN(d, t[0], Fx, 1e-12, 0.0, "yatay grup F_x");
        YAKIN(d, t[4], 0.085 * Fx, 1e-12, 0.0, "yatay grup M = 0,085 X");
        for (int j : {1, 2, 3, 5}) YAKIN(d, t[static_cast<std::size_t>(j)], 0.0, 0.0, 1e-12 * Fx, "yatay grup çapraz");
    }
    return d.hata;
}

// Kabul 3: sabit itkide ulaşılan hız sönüm katsayılarından hesaplanan değerle uyuşur:
// D_L,i v + D_NL,i v|v| = τ_i (tam batık, s = 1, nötr yüzerlik; tek eksen).
int test_son_hiz(const TestBaglami&)
{
    Denetci d;
    TestAyari ta;
    ta.notr_yuzerlik = true;
    const AracParametreleri a = test_araci(ta);
    // Tolerans: hız 1e-6 bağıl; çapraz hızlar 1e-8 (birim/s). Gerekçe: 30 s sonunda en yavaş
    // eksenin (yalnız karesel sönümlü yan öteleme, v(t) ≈ v_T − O(e^{−2 D_NL v_T t / M})) kalıntısı
    // ve tahsisin yuvarlama düzeyindeki çapraz bileşeni bunun çok altındadır.
    const double kBagil = 1e-6;
    const double kCapraz = 1e-8;
    const struct {
        const char* ad;
        int eksen;
        double tau;
    } durumlar[] = {{"ileri", 0, 40.0}, {"yan", 1, 25.0}, {"dikey", 2, 30.0}, {"dönüş", 5, 2.0}};
    for (const auto& s : durumlar) {
        Vek6 tau{};
        const auto e = static_cast<std::size_t>(s.eksen);
        tau[e] = s.tau;
        std::array<double, kIticiSayisi> p{};
        if (!tahsis_pwm(a, tau, p)) {
            DOGRULA(d, false, std::string("tahsis başarısız: ") + s.ad);
            continue;
        }
        Dunya dn;
        if (!kur(dn, a, havuz_ortami(), baslangic(50.0, {}))) return 1;
        dn.itici_komutu_ver(0, p);
        if (!kos(dn, adim_sayisi(30.0))) return 1;
        const double Dl = a.hidrodinamik.dogrusal_sonum[e];
        const double Dq = a.hidrodinamik.karesel_sonum[e];
        const double v = (-Dl + std::sqrt(Dl * Dl + 4.0 * Dq * s.tau)) / (2.0 * Dq);
        const auto& an = dn.durum();
        const double nu[6] = {an.dogrusal_hiz_b[0], an.dogrusal_hiz_b[1], an.dogrusal_hiz_b[2],
                              an.acisal_hiz_b[0],   an.acisal_hiz_b[1],   an.acisal_hiz_b[2]};
        std::printf("  %-6s τ = %.1f: son hız %.9f, analitik %.9f\n", s.ad, s.tau, nu[e], v);
        YAKIN(d, nu[e], v, kBagil, 0.0, s.ad);
        for (std::size_t j = 0; j < 6; ++j) {
            if (j != e) YAKIN(d, nu[j], 0.0, 0.0, kCapraz, "çapraz hız");
        }
    }
    // 04 §7.6 karşılaştırması (bilgi): 85 N ileri → 0,7294 m/s.
    const double v85 = (-13.7 + std::sqrt(13.7 * 13.7 + 4.0 * 141.0 * 85.0)) / (2.0 * 141.0);
    std::printf("  bilgi: 85 N ileri için analitik %.4f m/s (04 §7.6: 0,7294)\n", v85);
    return d.hata;
}

}  // namespace ida::fizik::test
