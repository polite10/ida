// W4-sonar-sahne: üçgen ağlı sahne (ag_sahnesi.hpp).
//
//   ag_kaba_kuvvet     sınır kutusu ağacıyla kesişim = bütün üçgenlerin taranması (rastgele ağ, pozlu
//                      gövde, 20 000 ışın): isabet kararı aynı, uzaklık 1e-9 m içinde
//   ag_esitlik         aynı uzaklıktaki iki üçgende küçük sıralı üçgen seçilir (ekleme sırası ve ağaç
//                      yapısından bağımsız); gövdeler arasında önce eklenen kalır
//   ag_hareket         sabit_hiz ve sinus hareketi: ışın gövde çerçevesinde izlenir, ağaç yeniden
//                      kurulmaz; govde_pozu_kur
//   ag_hucre           iki üçgene bölünmüş duvar tek hücre ızgarası (aynı yüzey, köşegenin iki yanında
//                      aynı hücre); su yüzeyi hücreleri NED (x, y)
//   ag_cozumsel        aynı havuz çözümsel kutu ve üçgen ağı olarak: Ping360 duvar tepeleri ve Ping1D
//                      mesafeleri aynı kutuda
#include "ida/sonar/ag_sahnesi.hpp"
#include "test_destek.hpp"

#include <cstdint>
#include <tuple>

namespace ida::sonar::test {

namespace {

// Belirlenimci sözde rastgele (64 bit LCG; testin kendi akışı).
struct Lcg {
    std::uint64_t s;
    double u()
    {
        s = s * 6364136223846793005ull + 1442695040888963407ull;
        return static_cast<double>(s >> 11) * (1.0 / 9007199254740992.0);
    }
    double ara(double a, double b) { return a + (b - a) * u(); }
};

AkustikMalzeme mz(const char* ad, double mu = 0.02) { return {ad, mu, GelisYasasi::lambert_cos2, 0.5, 0.5}; }

// Kutunun 12 üçgeni (dışa dönük), gövde çerçevesinde.
void kutu_ekle(UcgenAgi& a, const Vek3& mn, const Vek3& mx, std::uint16_t m)
{
    const auto b = static_cast<std::uint32_t>(a.koseler.size());
    for (int x = 0; x < 2; ++x) {
        for (int y = 0; y < 2; ++y) {
            for (int z = 0; z < 2; ++z) a.koseler.push_back({x ? mx[0] : mn[0], y ? mx[1] : mn[1], z ? mx[2] : mn[2]});
        }
    }
    static const std::uint32_t yuz[12][3] = {{0, 1, 3}, {0, 3, 2}, {4, 6, 7}, {4, 7, 5}, {0, 4, 5}, {0, 5, 1},
                                            {2, 3, 7}, {2, 7, 6}, {0, 2, 6}, {0, 6, 4}, {1, 5, 7}, {1, 7, 3}};
    for (const auto& t : yuz) {
        a.ucgenler.push_back({b + t[0], b + t[1], b + t[2]});
        a.malzeme.push_back(m);
    }
}

// Kaba kuvvet: dünya çerçevesindeki üçgenlerle Möller–Trumbore (ağacın denetimi için ayrı yazım).
bool kaba(const std::vector<std::array<Vek3, 3>>& u, const Vek3& o, const Vek3& d, double t_maks, double& t_en)
{
    bool var = false;
    t_en = t_maks;
    for (const auto& tri : u) {
        const Vek3 e1 = ic::cikar(tri[1], tri[0]);
        const Vek3 e2 = ic::cikar(tri[2], tri[0]);
        const Vek3 p = ic::capraz(d, e2);
        const double det = ic::nokta(e1, p);
        if (det == 0.0) continue;
        const Vek3 s = ic::cikar(o, tri[0]);
        const double a = ic::nokta(s, p) / det;
        if (a < 0.0 || a > 1.0) continue;
        const Vek3 q = ic::capraz(s, e1);
        const double b = ic::nokta(d, q) / det;
        if (b < 0.0 || a + b > 1.0) continue;
        const double t = ic::nokta(e2, q) / det;
        if (t > 1e-9 && t <= t_en) {
            t_en = t;
            var = true;
        }
    }
    return var;
}

}  // namespace

int test_ag_kaba_kuvvet()
{
    Denetci d;
    Lcg r{12345};
    AgSahnesi s;
    const std::uint16_t m = s.malzeme_ekle(mz("m"));
    AgGovdesi g;
    g.kimlik = 7;
    for (int i = 0; i < 3000; ++i) {
        const Vek3 c{r.ara(-5, 5), r.ara(-5, 5), r.ara(-5, 5)};
        const auto b = static_cast<std::uint32_t>(g.ag.koseler.size());
        for (int k = 0; k < 3; ++k) g.ag.koseler.push_back({c[0] + r.ara(-0.4, 0.4), c[1] + r.ara(-0.4, 0.4), c[2] + r.ara(-0.4, 0.4)});
        g.ag.ucgenler.push_back({b, b + 1, b + 2});
        g.ag.malzeme.push_back(m);
    }
    g.konum = {3.0, -2.0, 4.0};
    g.yonelim = euler_q(0.3, -0.2, 1.1);
    std::string hata;
    DOGRULA(d, s.govde_ekle(g, &hata), "gövde eklenir: " + hata);
    DOGRULA(d, s.ucgen_sayisi(7) == 3000u && s.toplam_ucgen() == 3000u, "üçgen sayısı");
    const fizik::Mat3 R = ic::matris(ic::normalle(g.yonelim));
    std::vector<std::array<Vek3, 3>> dunya;
    for (const auto& u : g.ag.ucgenler) {
        std::array<Vek3, 3> t;
        for (std::size_t k = 0; k < 3; ++k) t[k] = ic::topla(g.konum, ic::carp(R, g.ag.koseler[u[k]]));
        dunya.push_back(t);
    }
    std::size_t isabet = 0, uyusmaz = 0;
    double en_kotu = 0.0;
    for (int i = 0; i < 20000; ++i) {
        const Vek3 o{r.ara(-12, 18), r.ara(-15, 11), r.ara(-8, 16)};
        Vek3 dir{r.ara(-1, 1), r.ara(-1, 1), r.ara(-1, 1)};
        const double l = ic::uzunluk(dir);
        if (l < 1e-3) continue;
        dir = ic::olcekle(dir, 1.0 / l);
        Vurus v;
        const bool a = s.kesis(o, dir, 1e-9, 60.0, 0, 0.01, v);
        double t = 0.0;
        const bool b = kaba(dunya, o, dir, 60.0, t);
        if (a != b) {
            ++uyusmaz;
            continue;
        }
        if (a) {
            ++isabet;
            en_kotu = std::max(en_kotu, std::abs(v.t - t));
            DOGRULA(d, v.govde == 7u && v.malzeme == m, "gövde ve malzeme");
            DOGRULA(d, ic::nokta(v.normal, dir) <= 0.0, "normal ışına bakar");
        }
    }
    std::printf("  %zu isabet, uyuşmayan %zu, en büyük uzaklık farkı %.3g m\n", isabet, uyusmaz, en_kotu);
    DOGRULA(d, isabet > 1000u && uyusmaz == 0u && en_kotu <= 1e-9, "ağaç = kaba kuvvet");
    return d.hata;
}

int test_ag_esitlik()
{
    Denetci d;
    // Aynı yerde iki üçgen (farklı malzeme): küçük sıralı üçgen kazanır; ağaç yapısı değişse de
    // (çok sayıda dolgu üçgeni ile) sonuç aynı.
    for (int dolgu : {0, 200}) {
        for (bool ters : {false, true}) {
            AgSahnesi s;
            const std::uint16_t ma = s.malzeme_ekle(mz("a"));
            const std::uint16_t mb = s.malzeme_ekle(mz("b"));
            AgGovdesi g;
            g.kimlik = 3;
            Lcg r{7};
            for (int i = 0; i < dolgu; ++i) {
                const Vek3 c{r.ara(5, 9), r.ara(-3, 3), r.ara(-3, 3)};
                const auto b = static_cast<std::uint32_t>(g.ag.koseler.size());
                for (int k = 0; k < 3; ++k) g.ag.koseler.push_back({c[0], c[1] + r.ara(-0.2, 0.2), c[2] + r.ara(-0.2, 0.2)});
                g.ag.ucgenler.push_back({b, b + 1, b + 2});
                g.ag.malzeme.push_back(ma);
            }
            const auto b = static_cast<std::uint32_t>(g.ag.koseler.size());
            g.ag.koseler.push_back({2.0, -1.0, -1.0});
            g.ag.koseler.push_back({2.0, 1.0, -1.0});
            g.ag.koseler.push_back({2.0, 0.0, 1.0});
            g.ag.ucgenler.push_back({b, b + 1, b + 2});
            g.ag.malzeme.push_back(ters ? mb : ma);
            g.ag.ucgenler.push_back({b, b + 2, b + 1});
            g.ag.malzeme.push_back(ters ? ma : mb);
            DOGRULA(d, s.govde_ekle(g), "gövde");
            Vurus v;
            DOGRULA(d, s.kesis({0, 0, 0}, {1, 0, 0}, 1e-9, 10.0, 0, 0.01, v) && v.t == 2.0, "isabet 2 m");
            DOGRULA(d, v.malzeme == (ters ? mb : ma), "küçük sıralı üçgenin malzemesi (dolgu " + std::to_string(dolgu) + ")");
        }
    }
    // Gövdeler arasında: aynı düzlemde iki gövde; önce eklenen kalır.
    {
        AgSahnesi s;
        const std::uint16_t m = s.malzeme_ekle(mz("a"));
        for (std::uint32_t k : {11u, 10u}) {
            AgGovdesi g;
            g.kimlik = k;
            kutu_ekle(g.ag, {2.0, -1.0, -1.0}, {3.0, 1.0, 1.0}, m);
            DOGRULA(d, s.govde_ekle(g), "gövde");
        }
        Vurus v;
        DOGRULA(d, s.kesis({0, 0, 0}, {1, 0, 0}, 1e-9, 10.0, 0, 0.01, v) && v.govde == 11u, "önce eklenen gövde kalır");
        AgGovdesi g;
        g.kimlik = 11;
        kutu_ekle(g.ag, {0, 0, 0}, {1, 1, 1}, m);
        DOGRULA(d, !s.govde_ekle(g), "yinelenen kimlik reddedilir");
    }
    return d.hata;
}

int test_ag_hareket()
{
    Denetci d;
    AgSahnesi s;
    const std::uint16_t m = s.malzeme_ekle(mz("hedef", 0.05));
    AgGovdesi g;
    g.kimlik = 1001;
    kutu_ekle(g.ag, {-0.5, -0.25, -0.25}, {0.5, 0.25, 0.25}, m);
    g.konum = {4.0, 0.0, 1.5};
    g.hareket.tur = HareketTuru::sabit_hiz;
    g.hareket.hiz_ned = {0.5, 0.0, 0.0};
    DOGRULA(d, s.govde_ekle(g), "hedef");
    const Vek3 o{0.0, 0.0, 1.5};
    for (std::int64_t t_ns : {0LL, 2'000'000'000LL, 4'000'000'000LL}) {
        Vurus v;
        const double beklenen = 4.0 + 0.5 * static_cast<double>(t_ns) * 1e-9 - 0.5;
        DOGRULA(d, s.kesis(o, {1, 0, 0}, 1e-9, 50.0, t_ns, 0.01, v), "isabet");
        YAKIN(d, v.t, beklenen, 1e-12, "sabit hızla menzil");
        Vek3 p{};
        DOGRULA(d, s.govde_konumu(1001, t_ns, p) && std::abs(p[0] - (beklenen + 0.5)) < 1e-12, "govde_konumu");
    }
    // Yönelim: 90° yaw → kutunun dar yüzü (0,25) ışına döner.
    DOGRULA(d, s.govde_pozu_kur(1001, {4.0, 0.0, 1.5}, euler_q(0.0, 0.0, ic::kPi / 2), Hareket{}), "poz");
    {
        Vurus v;
        DOGRULA(d, s.kesis(o, {1, 0, 0}, 1e-9, 50.0, 7'000'000'000LL, 0.01, v), "isabet");
        YAKIN(d, v.t, 3.75, 1e-12, "dönük kutu, sabit");
    }
    // Sinüs: konum + A sin(2π t / P + φ).
    Hareket h;
    h.tur = HareketTuru::sinus;
    h.genlik_ned = {1.0, 0.0, 0.0};
    h.periyot_ns = 8'000'000'000;
    h.faz = 0.0;
    DOGRULA(d, s.govde_pozu_kur(1001, {4.0, 0.0, 1.5}, {1, 0, 0, 0}, h), "sinüs");
    for (std::int64_t t_ns : {0LL, 2'000'000'000LL, 6'000'000'000LL, 10'000'000'000LL}) {
        Vurus v;
        const double x = 4.0 + std::sin(2.0 * ic::kPi * static_cast<double>(t_ns % h.periyot_ns) / 8e9);
        DOGRULA(d, s.kesis(o, {1, 0, 0}, 1e-9, 50.0, t_ns, 0.01, v), "isabet");
        YAKIN(d, v.t, x - 0.5, 1e-9, "sinüsle menzil");
    }
    h.periyot_ns = 0;
    DOGRULA(d, !s.govde_pozu_kur(1001, {4.0, 0.0, 1.5}, {1, 0, 0, 0}, h), "periyot 0 reddedilir");
    return d.hata;
}

int test_ag_hucre()
{
    Denetci d;
    AgSahnesi s;
    const std::uint16_t m = s.malzeme_ekle(mz("duvar"));
    const std::uint16_t y = s.malzeme_ekle(HavuzTanimi::su_yuzeyi_malzemesi());
    DOGRULA(d, s.su_yuzeyi_ekle(1, y), "su yüzeyi");
    DOGRULA(d, !s.su_yuzeyi_ekle(9, y), "ikinci su yüzeyi reddedilir");
    AgGovdesi g;
    g.kimlik = 2;
    // x = 6 düzleminde 4 × 4 m kare, köşegenle iki üçgen.
    g.ag.koseler = {{6, -2, -1}, {6, 2, -1}, {6, 2, 3}, {6, -2, 3}};
    g.ag.ucgenler = {{0, 1, 2}, {0, 2, 3}};
    g.ag.malzeme = {m, m};
    DOGRULA(d, s.govde_ekle(g), "duvar");
    // Köşegene (y = z − 1... köşegen (−2,−1)–(2,3): z = y + 1) çok yakın iki nokta.
    auto vur = [&](double yy, double zz, Vurus& v) {
        const Vek3 hedef{6.0, yy, zz};
        Vek3 dir = ic::cikar(hedef, Vek3{0.0, 0.0, 1.0});
        dir = ic::olcekle(dir, 1.0 / ic::uzunluk(dir));
        return s.kesis({0.0, 0.0, 1.0}, dir, 1e-9, 20.0, 0, 0.01, v);
    };
    Vurus a, b;
    DOGRULA(d, vur(0.5051, 1.5049, a) && vur(0.5049, 1.5051, b), "köşegenin iki yanı");
    DOGRULA(d, a.govde == 2u && b.govde == 2u && a.yuzey == b.yuzey, "aynı eş düzlemli küme");
    DOGRULA(d, a.hucre_i == b.hucre_i && a.hucre_j == b.hucre_j, "köşegenin iki yanında aynı hücre");
    Vurus c;
    DOGRULA(d, vur(0.52, 1.5, c) && (c.hucre_i != a.hucre_i || c.hucre_j != a.hucre_j), "2 cm ötede başka hücre");
    // Su yüzeyi: yukarı bakan ışın z = 0'da, hücre NED (x, y).
    Vurus w;
    DOGRULA(d, s.kesis({1.234, -0.567, 1.0}, {0, 0, -1}, 1e-9, 20.0, 0, 0.01, w) && w.govde == 1u && w.t == 1.0 &&
                   w.malzeme == y && w.hucre_i == 123 && w.hucre_j == -57 && w.normal[2] == 1.0,
            "su yüzeyi isabeti ve hücresi");
    return d.hata;
}

int test_ag_cozumsel()
{
    Denetci d;
    // Havuz: x ∈ [0, 25], y ∈ [−6,25; 6,25], taban z = 3 (HavuzTanimi). Ağ: aynı iç yüzler, 0,2 m
    // kalın kutulardan (A7 havuz_ornek düzeni). Araç (5, 0, 1); Ping360 100 gradyanda sancak duvarı
    // (6,25 m), 0 gradyanda kuzey duvarı (20 m; 25 m menzil) ve Ping1D tabana (2 m).
    const AkustikMalzeme beton{"havuz_beton", 0.02, GelisYasasi::cos1, 0.6, 0.7};
    auto ag = std::make_shared<AgSahnesi>();
    {
        const std::uint16_t y = ag->malzeme_ekle(HavuzTanimi::su_yuzeyi_malzemesi());
        const std::uint16_t b = ag->malzeme_ekle(beton);
        (void)ag->su_yuzeyi_ekle(1, y);
        AgGovdesi g;
        g.kimlik = 2;
        const double t = 0.2;
        kutu_ekle(g.ag, {0.0, -6.25, 3.0}, {25.0, 6.25, 3.0 + t}, b);
        kutu_ekle(g.ag, {25.0, -6.25 - t, -0.5}, {25.0 + t, 6.25 + t, 3.0 + t}, b);
        kutu_ekle(g.ag, {-t, -6.25 - t, -0.5}, {0.0, 6.25 + t, 3.0 + t}, b);
        kutu_ekle(g.ag, {0.0, 6.25, -0.5}, {25.0, 6.25 + t, 3.0 + t}, b);
        kutu_ekle(g.ag, {0.0, -6.25 - t, -0.5}, {25.0, -6.25, 3.0 + t}, b);
        DOGRULA(d, ag->govde_ekle(g), "ağ havuzu");
    }
    auto coz = havuz_sahnesi(3.0);
    const Vek3 arac{5.0, 0.0, 1.0};
    const double c = 1482.36;
    for (const auto& [aci, sp, beklenen] : {std::tuple<std::uint16_t, std::uint16_t, double>{std::uint16_t{100}, std::uint16_t{311}, 6.25},
                                            std::tuple<std::uint16_t, std::uint16_t, double>{std::uint16_t{0}, std::uint16_t{1111}, 20.0}}) {
        std::size_t tepe[2] = {0, 0};
        int i = 0;
        for (std::shared_ptr<const Sahne> s : {std::shared_ptr<const Sahne>(coz), std::shared_ptr<const Sahne>(ag)}) {
            auto a = kume_ayari({ping360(p360_yap(sp, 1200, aci, aci, 1))});
            Kosu k;
            if (!k.kur(a, s) || !k.kos(durgun(arac), 0, 60)) return 1;
            const auto v = k.sonar(7);
            DOGRULA(d, !v.empty(), "ping");
            if (v.empty()) return d.hata;
            tepe[i++] = en_buyuk(p360(v.front()).tani.ham, 600);
        }
        std::printf("  %u grad: çözümsel tepe %.3f m, ağ tepe %.3f m (beklenen %.2f)\n", aci, kutu_menzili(c, sp, tepe[0]),
                    kutu_menzili(c, sp, tepe[1]), beklenen);
        DOGRULA(d, tepe[0] == tepe[1], "çözümsel ve ağ sahnesinde aynı tepe kutusu");
        YAKIN(d, kutu_menzili(c, sp, tepe[1]), beklenen, 0.02 * beklenen / 6.25, "ağ duvar menzili");
    }
    {
        double mesafe[2] = {0, 0};
        int i = 0;
        for (std::shared_ptr<const Sahne> s : {std::shared_ptr<const Sahne>(coz), std::shared_ptr<const Sahne>(ag)}) {
            Ping1dYapilandirmasi w;
            w.scan_length = 5.0;
            w.otomatik = false;
            Kosu k;
            if (!k.kur(kume_ayari({ping1d(w)}), s) || !k.kos(durgun(arac), 0, 200)) return 1;
            const auto v = k.sonar(6);
            DOGRULA(d, v.size() >= 3u, "Ping1D");
            if (v.empty()) return d.hata;
            mesafe[i++] = static_cast<double>(p1d(v.back()).distance);
        }
        std::printf("  Ping1D: çözümsel %.4f m, ağ %.4f m (beklenen 2)\n", mesafe[0], mesafe[1]);
        YAKIN(d, mesafe[1], 2.0, 3.0 * 5.0 / 200.0, "ağ tabanında Ping1D mesafesi");
        YAKIN(d, mesafe[1], mesafe[0], 2.0 * 5.0 / 200.0, "çözümsel ile aynı (2 kutu)");
    }
    return d.hata;
}

}  // namespace ida::sonar::test
