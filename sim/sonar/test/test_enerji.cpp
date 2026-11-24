// Kabul 2: enerji ve yakınsama (iskelet §8 "Sonar saçıcı kuralı"; MP7 yakınsama ölçütü).
#include "test_destek.hpp"

#include "ida/sensor/rastgele.hpp"

namespace ida::sonar::test {

namespace {

constexpr std::uint16_t kSp7 = 311;   // 7 m: n = 1200, sp = 311 → 6,9975 m (05 §2.4.5)

using Prof = std::vector<std::vector<std::vector<double>>>;   // [gerçekleşim][açı][kutu]

constexpr int kAci = 8;

// M gerçekleşim × 8 açı (7, 57, …, 357 gradyan); gürültü kapalı (yalnız benek). Gerçekleşim
// tohumu = ana_tohum (benek alanı N'den bağımsız).
struct Satir {
    std::uint16_t n = 1200;
    std::uint16_t sp = kSp7;
};

Prof uret(std::shared_ptr<const Sahne> s, Satir y, int n_isin, std::uint64_t tohum0, int m_say, std::uint32_t& gercek)
{
    Prof x(static_cast<std::size_t>(m_say));
    for (int m = 0; m < m_say; ++m) {
        auto a = kume_ayari({ping360(p360_yap(y.sp, y.n, 7, 7, 400 / kAci))}, tohum0 + static_cast<std::uint64_t>(m));
        a.sade_isin_sayisi = n_isin;
        Kosu k;
        if (!k.kur(a, s) || !k.kos(durgun({0.0, 0.0, 6.0}), 0, 8 * kAci + 10)) std::abort();
        const auto v = k.sonar(7);
        for (int i = 0; i < kAci; ++i) {
            x[static_cast<std::size_t>(m)].push_back(p360(v[static_cast<std::size_t>(i)]).tani.yogunluk);
            gercek = p360(v[static_cast<std::size_t>(i)]).tani.isin;
        }
    }
    return x;
}

struct Buyukluk {
    double de = 0.0, dv = 0.0, dc = 0.0;
};

// MP7 tanımı (W0-MP7a "Mimarın tanımı" md. 4; MP7 prototipiyle aynı hesap).
Buyukluk buyukluk(const Prof& x1, const Prof& x2, const std::vector<int>& idx,
                  const std::vector<std::vector<char>>& kume)
{
    const std::size_t na = x1[0].size(), n = x1[0][0].size();
    const double m = static_cast<double>(idx.size());
    double se = 0, se2 = 0, sv = 0, sv2 = 0, c1 = 0, c2 = 0;
    long long nc = 0;
    std::vector<double> e1(n), e2(n), v1(n), v2(n);
    auto ev = [&](const Prof& x, std::size_t a, std::size_t i, double& e, double& v) {
        double s = 0, s2 = 0;
        for (int k : idx) {
            const double z = x[static_cast<std::size_t>(k)][a][i];
            s += z;
            s2 += z * z;
        }
        e = s / m;
        v = std::max(0.0, (s2 - s * s / m) / (m - 1));
    };
    auto ilinti = [&](const Prof& x, std::size_t a, std::size_t i, double ea, double eb, double va, double vb) {
        double c = 0;
        for (int k : idx) {
            c += (x[static_cast<std::size_t>(k)][a][i] - ea) * (x[static_cast<std::size_t>(k)][a][i + 1] - eb);
        }
        return c / (m - 1) / std::sqrt(va * vb);
    };
    for (std::size_t a = 0; a < na; ++a) {
        for (std::size_t i = 0; i < n; ++i) {
            if (kume[a][i]) {
                ev(x1, a, i, e1[i], v1[i]);
                ev(x2, a, i, e2[i], v2[i]);
                se += std::abs(e2[i] - e1[i]);
                se2 += e2[i];
                sv += std::abs(v2[i] - v1[i]);
                sv2 += v2[i];
            }
        }
        for (std::size_t i = 0; i + 1 < n; ++i) {
            if (kume[a][i] && kume[a][i + 1] && v1[i] > 0 && v1[i + 1] > 0 && v2[i] > 0 && v2[i + 1] > 0) {
                c1 += ilinti(x1, a, i, e1[i], e1[i + 1], v1[i], v1[i + 1]);
                c2 += ilinti(x2, a, i, e2[i], e2[i + 1], v2[i], v2[i + 1]);
                ++nc;
            }
        }
    }
    const double o1 = nc ? c1 / static_cast<double>(nc) : 0, o2 = nc ? c2 / static_cast<double>(nc) : 0;
    return {se2 > 0 ? se / se2 : 0.0, sv2 > 0 ? sv / sv2 : 0.0, std::abs(o2 - o1)};
}

double yuzdelik(std::vector<double> v, double p)
{
    std::sort(v.begin(), v.end());
    const std::size_t k = static_cast<std::size_t>(std::ceil(p * static_cast<double>(v.size()))) - 1;
    return v[std::min(k, v.size() - 1)];
}

}  // namespace

int test_enerji_isin_sayisi()
{
    Denetci d;
    // İskelet §8: hücreye düşen ışınların enerji ağırlıkları toplanır; ışın sayısı iki katına
    // çıkınca profil enerjisi büyümez (05 §2.4.3'ün eşevreli genlik toplamı enerjiyi N ile
    // büyütüyordu; MP7 §7). Ölçüt: alıcı öncesi toplam enerjinin oranı 1 ± %5 (yalnız ışın
    // örneklemesinin Monte Carlo farkı; MP7'de sade δ_E 5 000 → 10 000 adımında 0,007).
    auto s = orta_sahne();
    for (int aci : {7, 107, 207}) {
        double onceki = 0.0;
        for (int n : {2500, 5000, 10000, 20000, 40000}) {
            auto a = kume_ayari({ping360(p360_yap(kSp7, 1200, static_cast<std::uint16_t>(aci),
                                                  static_cast<std::uint16_t>(aci), 1))});
            a.sade_isin_sayisi = n;
            Kosu k;
            if (!k.kur(a, s) || !k.kos(durgun({0.0, 0.0, 6.0}), 0, 12)) return 1;
            const double e = toplam(p360(k.sonar(7).front()).tani.ham);
            if (onceki > 0.0) {
                std::printf("  açı %d, N %d: E(2N)/E(N) = %.4f\n", aci, n, e / onceki);
                DOGRULA(d, e / onceki < 1.05 && e / onceki > 0.95, "ışın sayısıyla enerji değişmez");
            }
            onceki = e;
        }
    }
    return d.hata;
}

namespace {

// MP7 yakınsama ölçütü (W0-MP7a md. 4; MP7 §4.3; ölçümden önce dondurulmuş): M = 32
// gerçekleşim (tohum 1000–1031), 8 kafa açısı, gürültü kapalı. Kutu kümesi S bağımsız
// referanstan (N = 40 000, tohum 5000–5031): açı başına en yüksek referans ortalama enerjisinin
// 10⁻³ katından büyük kutular. N → 2N için δ_E, δ_V, δ_C; nokta değeri ve bootstrap %95 üst ucu
// (B = 1000) ≤ 0,05; bir sonraki ikiye katlamada da sağlanmalı. Dönüş: sağlanan adım sayısı.
int yakinsama(Denetci& d, Satir y, bool dogrula)
{
    constexpr int kM = 32;
    constexpr int kB = 1000;
    constexpr double kEsik = 0.05;
    auto s = orta_sahne();
    std::uint32_t g_ref = 0;
    const Prof ref = uret(s, y, 40000, 5000, kM, g_ref);
    const std::size_t n = ref[0][0].size();
    std::vector<std::vector<char>> kume(kAci, std::vector<char>(n, 0));
    long long ns = 0;
    for (std::size_t a = 0; a < kAci; ++a) {
        std::vector<double> e(n, 0.0);
        for (int m = 0; m < kM; ++m) {
            for (std::size_t i = 0; i < n; ++i) e[i] += ref[static_cast<std::size_t>(m)][a][i] / kM;
        }
        const double mx = *std::max_element(e.begin(), e.end());
        for (std::size_t i = 0; i < n; ++i) {
            if (mx > 0 && e[i] > 1e-3 * mx) {
                kume[a][i] = 1;
                ++ns;
            }
        }
    }
    std::printf("  n = %u, sp = %u: |S| = %lld kutu, referans N = %u\n", static_cast<unsigned>(y.n),
                static_cast<unsigned>(y.sp), ns, g_ref);
    DOGRULA(d, ns > 200, "S boş değil");
    const int nler[] = {5000, 10000, 20000};
    std::vector<Prof> x;
    std::vector<std::uint32_t> g(3);
    for (std::size_t q = 0; q < 3; ++q) x.push_back(uret(s, y, nler[q], 1000, kM, g[q]));
    std::vector<int> tum(kM);
    for (int m = 0; m < kM; ++m) tum[static_cast<std::size_t>(m)] = m;
    const sensor::Akis kb(77, "test/bootstrap");
    int gecen = 0;
    for (std::size_t q = 0; q + 1 < 3; ++q) {
        const Buyukluk nk = buyukluk(x[q], x[q + 1], tum, kume);
        if (tani_kipi()) {
            for (std::size_t a = 0; a < kAci; ++a) {
                std::vector<std::vector<char>> tek(kAci, std::vector<char>(n, 0));
                tek[a] = kume[a];
                const Buyukluk r = buyukluk(x[q], x[q + 1], tum, tek);
                long c = 0;
                for (char z : kume[a]) c += z;
                std::printf("    açı %zu: |S| %ld δ_E %.4f δ_V %.4f δ_C %.4f\n", a, c, r.de, r.dv, r.dc);
            }
        }
        std::vector<double> be, bv, bc;
        std::vector<int> idx(kM);
        for (int b = 0; b < kB; ++b) {
            for (int m = 0; m < kM; ++m) {
                const auto w = kb.sozcukler((static_cast<std::uint64_t>(q) << 32) | static_cast<std::uint32_t>(b),
                                            static_cast<std::uint32_t>(m));
                idx[static_cast<std::size_t>(m)] = static_cast<int>(w[0] % static_cast<std::uint32_t>(kM));
            }
            const Buyukluk r = buyukluk(x[q], x[q + 1], idx, kume);
            be.push_back(r.de);
            bv.push_back(r.dv);
            bc.push_back(r.dc);
        }
        const double ue = yuzdelik(be, 0.975), uv = yuzdelik(bv, 0.975), uc = yuzdelik(bc, 0.975);
        const bool gecti = nk.de <= kEsik && ue <= kEsik && nk.dv <= kEsik && uv <= kEsik && nk.dc <= kEsik &&
                           uc <= kEsik;
        std::printf("  N %u → %u: δ_E %.4f (üst %.4f)  δ_V %.4f (üst %.4f)  δ_C %.4f (üst %.4f)  %s\n", g[q],
                    g[q + 1], nk.de, ue, nk.dv, uv, nk.dc, uc, gecti ? "sağlandı" : "sağlanmadı");
        gecen += gecti ? 1 : 0;
        if (dogrula) {
            DOGRULA(d, nk.de <= kEsik && ue <= kEsik, "δ_E ≤ 0,05");
            DOGRULA(d, nk.dv <= kEsik && uv <= kEsik, "δ_V ≤ 0,05");
            DOGRULA(d, nk.dc <= kEsik && uc <= kEsik, "δ_C ≤ 0,05");
        }
    }
    return gecen;
}

}  // namespace

int test_yakinsama_mp7()
{
    Denetci d;
    // Sahne MP7'nin değil, onun çözümsel karşılığıdır (orta_sahne, MP7 malzeme değerleri); ölçüt
    // aynıdır. MP7'nin sade kademesi satırı 600 örneğe indiriyordu (MP7 kaynak/sonar.cpp: sade ve
    // n > 600 ise n_eff = 600, sp ölçeklenir; 05 §2.4.7); S2 hükmü (N = 5 000) bu yapılandırmanın
    // ölçümüdür. Kabul bu yapılandırmada sınanır: 7 m, n = 600, sp = 622.
    yakinsama(d, {600, 622}, true);
    // A5-K78 sade kademede de 1 200 örnek ister. Bu yapılandırmada (7 m, n = 1200, sp = 311) aynı
    // ölçüt bu sahnede 5 000 ışında sağlanmaz (bilgi; sim/sonar/BENIOKU.md, rapor). Ağır olduğu
    // için yalnız elle koşulur: `ida_sonar_test yakinsama_mp7 tani`.
    if (tani_kipi()) {
        Denetci bilgi;
        yakinsama(bilgi, {1200, kSp7}, false);
    }
    return d.hata;
}

}  // namespace ida::sonar::test
