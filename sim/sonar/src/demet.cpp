// Sade kademe ışın demeti ve biriktirme (demet.hpp).
#include "demet.hpp"

#include "vek.hpp"

#include <algorithm>
#include <cmath>

namespace ida::sonar::ic {

namespace {

constexpr double kSincSkala = 0.44295;    // sinc(0,44295) = 1/√2 (tek yönlü −3 dB)
constexpr double kAsgariCos = 0.05;        // sıyıran gelişte cos θ alt sınırı (05 §2.4.2 [T])
constexpr double kIsinTmin = 1e-6;        // m

std::uint64_t karistir(std::uint64_t x)
{
    x += 0x9E3779B97F4A7C15ull;
    x = (x ^ (x >> 30)) * 0xBF58476D1CE4E5B9ull;
    x = (x ^ (x >> 27)) * 0x94D049BB133111EBull;
    return x ^ (x >> 31);
}

std::uint64_t ozet(const HucreAnahtari& k)
{
    const std::uint64_t a = (static_cast<std::uint64_t>(k.govde) << 32) | k.yuzey;
    const std::uint64_t b = (static_cast<std::uint64_t>(static_cast<std::uint32_t>(k.i)) << 32) |
                            static_cast<std::uint32_t>(k.j);
    return karistir(a ^ karistir(b ^ (static_cast<std::uint64_t>(k.yol) << 56)));
}

bool esit(const HucreAnahtari& a, const HucreAnahtari& b)
{
    return a.govde == b.govde && a.yuzey == b.yuzey && a.i == b.i && a.j == b.j && a.yol == b.yol;
}

}  // namespace

double sinc(double x)
{
    if (std::abs(x) < 1e-9) {
        return 1.0;
    }
    const double px = kPi * x;
    return std::sin(px) / px;
}

Desen Desen::tam_genislikten(double yatay_3db_rad, double dusey_3db_rad)
{
    return {0.5 * yatay_3db_rad / kSincSkala, 0.5 * dusey_3db_rad / kSincSkala};
}

void Demet::izle(const DemetGirdisi& g, DemetSonucu& s)
{
    const int n = std::max(1, g.n_kutu);
    const std::size_t nf = static_cast<std::size_t>(4 * n);
    const double dt = g.t_s / 4.0;
    // Darbe/alıcı çekirdeği (05 §2.4.4): dikdörtgen zarf ⊗ alıcı süzgeci = üçgen, sıfır fazlı
    // (tepe gerçek gecikmede [T]; MP7 §3 md.4); sade kademede karesi.
    const int hw = std::max(0, static_cast<int>(std::lround(g.darbe_s / dt)));
    cekirdek_.assign(static_cast<std::size_t>(2 * hw + 1), 0.0);
    for (int m = -hw; m <= hw; ++m) {
        const double h = 1.0 - std::abs(static_cast<double>(m)) / static_cast<double>(hw + 1);
        cekirdek_[static_cast<std::size_t>(m + hw)] = h * h;
    }
    // Dinlenen en uzak menzil (çekirdek payıyla).
    const double tau_son = g.tau0 + static_cast<double>(n) * g.t_s + static_cast<double>(hw + 1) * dt;
    const double r_maks = 0.5 * g.c * tau_son;

    const Desen& p = g.desen;
    const double wh = p.teta0;
    const double wv = p.fi0;
    const int nh = std::max(1, static_cast<int>(std::lround(std::sqrt(static_cast<double>(g.isin_sayisi) * wh / wv))));
    const int nv = std::max(1, static_cast<int>(std::lround(static_cast<double>(g.isin_sayisi) / static_cast<double>(nh))));
    const double dth = 2.0 * wh / static_cast<double>(nh);
    const double dph = 2.0 * wv / static_cast<double>(nv);
    const int nr = nh * nv;
    const double ln10 = std::log(10.0);

    std::size_t kap = 1024;
    while (kap < 2u * static_cast<std::size_t>(nr)) {
        kap <<= 1;
    }
    tablo_.assign(kap, -1);
    hucreler_.clear();
    hucreler_.reserve(static_cast<std::size_t>(nr));

    std::uint32_t isabet = 0;
    Vurus v;
    for (int j = 0; j < nr; ++j) {
        const int jh = j % nh;
        const int jv = j / nh;
        // 05 §2.6: n = ping numarası, bileşen + çekiliş = ışın numarası (24 bit).
        const auto w = g.isin_akisi->sozcukler(g.ping_no, static_cast<std::uint32_t>(j) & 0x00FFFFFFu);
        const double th = -wh + (static_cast<double>(jh) + sensor::duzgun_donusum(w[0])) * dth;
        const double ph = -wv + (static_cast<double>(jv) + sensor::duzgun_donusum(w[1])) * dph;
        const double cph = std::cos(ph);
        const double dom = dth * dph * cph;
        const Vek3 dh{cph * std::cos(th), cph * std::sin(th), std::sin(ph)};
        const Vek3 d = carp(g.r_nh, dh);
        if (!g.sahne->kesis(g.konum, d, kIsinTmin, r_maks, g.t_ns, g.hucre_kenari, v)) {
            continue;
        }
        ++isabet;
        const AkustikMalzeme& m = g.sahne->malzeme(v.malzeme);
        const double r = v.t;
        const double cosi = std::max(-nokta(d, v.normal), kAsgariCos);
        const double sigma = m.geri_sacilim_mu *
                             (m.gelis_yasasi == GelisYasasi::lambert_cos2 ? cosi * cosi : cosi);
        const double b = sinc(th / p.teta0) * sinc(ph / p.fi0);
        const double b2 = b * b;
        // w_j = a_j², a_j = B² √(σ_bs dA) 10^(−2αr/20) / r², dA = r² dΩ / cos θ (05 §2.4.2).
        const double w_j = b2 * b2 * sigma * dom * std::exp(-2.0 * g.alfa_db_m * r / 10.0 * ln10) /
                           (r * r * cosi);
        const double tau = 2.0 * r / g.c;
        const HucreAnahtari k{v.govde, v.yuzey, v.hucre_i, v.hucre_j, 0};
        std::size_t h = static_cast<std::size_t>(ozet(k)) & (kap - 1);
        for (;;) {
            const std::int32_t ix = tablo_[h];
            if (ix < 0) {
                tablo_[h] = static_cast<std::int32_t>(hucreler_.size());
                hucreler_.push_back({k, w_j, w_j * tau});
                break;
            }
            Hucre& c = hucreler_[static_cast<std::size_t>(ix)];
            if (esit(c.k, k)) {
                c.sw += w_j;
                c.swt += w_j * tau;
                break;
            }
            h = (h + 1) & (kap - 1);
        }
    }

    // Hücre: genlik √(Σw), enerji Σw, gecikme enerji ağırlıklı ortalama (iskelet §8). Sade
    // kademede faz yoktur; hücreler eşevresiz (enerji) toplanır. Ekleme sırası = ışın sırası.
    ince_.assign(nf, 0.0);
    for (const Hucre& c : hucreler_) {
        if (!(c.sw > 0.0)) {
            continue;
        }
        const double tau = c.swt / c.sw;
        const double fpos = (tau - g.tau0) / dt - 0.5;
        const double fl = std::floor(fpos);
        if (fl < -1.0 || fl >= static_cast<double>(nf)) {
            continue;
        }
        const long long k0 = static_cast<long long>(fl);
        const double a = fpos - fl;
        if (k0 >= 0) {
            ince_[static_cast<std::size_t>(k0)] += (1.0 - a) * c.sw;
        }
        if (k0 + 1 < static_cast<long long>(nf)) {
            ince_[static_cast<std::size_t>(k0 + 1)] += a * c.sw;
        }
    }
    evr_.assign(nf, 0.0);
    for (std::size_t i = 0; i < nf; ++i) {
        const double x = ince_[i];
        if (x == 0.0) {
            continue;
        }
        const std::size_t lo = i >= static_cast<std::size_t>(hw) ? i - static_cast<std::size_t>(hw) : 0u;
        const std::size_t hi = std::min(nf - 1, i + static_cast<std::size_t>(hw));
        for (std::size_t t = lo; t <= hi; ++t) {
            evr_[t] += cekirdek_[t + static_cast<std::size_t>(hw) - i] * x;
        }
    }
    s.ham.assign(static_cast<std::size_t>(n), 0.0);
    for (std::size_t i = 0; i < static_cast<std::size_t>(n); ++i) {
        s.ham[i] = 0.25 * (evr_[4 * i] + evr_[4 * i + 1] + evr_[4 * i + 2] + evr_[4 * i + 3]);
    }
    s.isin = static_cast<std::uint32_t>(nr);
    s.isabet = isabet;
}

}  // namespace ida::sonar::ic
