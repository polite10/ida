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

namespace {

int cekirdek_yari_genisligi(double darbe_s, double dt)
{
    return std::max(0, static_cast<int>(std::lround(darbe_s / dt)));
}

}  // namespace

double dinleme_sonu(double tau0, double t_s, int n_kutu, double darbe_s)
{
    const double dt = t_s / 4.0;
    const int hw = cekirdek_yari_genisligi(darbe_s, dt);
    return tau0 + static_cast<double>(std::max(1, n_kutu)) * t_s + static_cast<double>(hw + 1) * dt;
}

void biriktir(const std::vector<Katki>& katkilar, double tau0, double t_s, int n_kutu, double darbe_s,
              BiriktirmeTamponu& tampon, std::vector<double>& ham)
{
    const int n = std::max(1, n_kutu);
    const std::size_t nf = static_cast<std::size_t>(4 * n);
    const double dt = t_s / 4.0;
    // Darbe/alıcı çekirdeği (05 §2.4.4): dikdörtgen zarf ⊗ alıcı süzgeci = üçgen, sıfır fazlı
    // (tepe gerçek gecikmede [T]; MP7 §3 md.4); sade kademede karesi.
    const int hw = cekirdek_yari_genisligi(darbe_s, dt);
    const std::size_t uhw = static_cast<std::size_t>(hw);
    tampon.cekirdek.assign(2 * uhw + 1, 0.0);
    for (int m = -hw; m <= hw; ++m) {
        const double h = 1.0 - std::abs(static_cast<double>(m)) / static_cast<double>(hw + 1);
        tampon.cekirdek[static_cast<std::size_t>(m + hw)] = h * h;
    }
    // Genişletilmiş ince dizi: [−hw, nf + hw); indis kayması hw.
    const std::size_t ne = nf + 2 * uhw;
    tampon.ince.assign(ne, 0.0);
    for (const Katki& k : katkilar) {
        if (!(k.enerji > 0.0)) {
            continue;
        }
        const double fpos = (k.tau - tau0) / dt - 0.5 + static_cast<double>(hw);
        const double fl = std::floor(fpos);
        if (fl < -1.0 || fl >= static_cast<double>(ne)) {
            continue;
        }
        const long long k0 = static_cast<long long>(fl);
        const double a = fpos - fl;
        if (k0 >= 0) {
            tampon.ince[static_cast<std::size_t>(k0)] += (1.0 - a) * k.enerji;
        }
        if (k0 + 1 < static_cast<long long>(ne)) {
            tampon.ince[static_cast<std::size_t>(k0 + 1)] += a * k.enerji;
        }
    }
    // Evrişim yalnız pencere içindeki ince örnekler için: [hw, hw + nf).
    tampon.evr.assign(nf, 0.0);
    for (std::size_t i = 0; i < ne; ++i) {
        const double x = tampon.ince[i];
        if (x == 0.0) {
            continue;
        }
        // Çıkış t (pencere içi indis), kaynak i: t + hw − i ∈ [0, 2hw].
        const std::size_t t_lo = i >= 2 * uhw ? i - 2 * uhw : 0u;
        const std::size_t t_hi = std::min(nf - 1, i);
        for (std::size_t t = t_lo; t <= t_hi; ++t) {
            tampon.evr[t] += tampon.cekirdek[t + 2 * uhw - i] * x;
        }
    }
    ham.assign(static_cast<std::size_t>(n), 0.0);
    for (std::size_t i = 0; i < static_cast<std::size_t>(n); ++i) {
        ham[i] = 0.25 * (tampon.evr[4 * i] + tampon.evr[4 * i + 1] + tampon.evr[4 * i + 2] + tampon.evr[4 * i + 3]);
    }
}

namespace {

// Işın ızgarası (nh × nv) ve hücre tablosunun boyu (`izle` ile `hazirla` aynı hesabı kullanır).
struct Izgara {
    int nh = 1, nv = 1;
    std::size_t kap = 1024;
};

Izgara izgara(const DemetGirdisi& g)
{
    Izgara z;
    const double wh = g.desen.teta0;
    const double wv = g.desen.fi0;
    z.nh = std::max(1, static_cast<int>(std::lround(std::sqrt(static_cast<double>(g.isin_sayisi) * wh / wv))));
    z.nv = std::max(1, static_cast<int>(std::lround(static_cast<double>(g.isin_sayisi) / static_cast<double>(z.nh))));
    while (z.kap < 2u * static_cast<std::size_t>(z.nh * z.nv)) {
        z.kap <<= 1;
    }
    return z;
}

}  // namespace

void Demet::hazirla(const DemetGirdisi& g, DemetSonucu& s)
{
    const Izgara z = izgara(g);
    const std::size_t nr = static_cast<std::size_t>(z.nh * z.nv);
    tablo_.reserve(z.kap);
    hucreler_.reserve(nr);
    katkilar_.reserve(nr);
    // biriktir(): çekirdek 2hw + 1, ince dizi 4n + 2hw, evrişim 4n, kutu n.
    const std::size_t n = static_cast<std::size_t>(std::max(1, g.n_kutu));
    const std::size_t hw = static_cast<std::size_t>(cekirdek_yari_genisligi(g.darbe_s, g.t_s / 4.0));
    tampon_.cekirdek.reserve(2 * hw + 1);
    tampon_.ince.reserve(4 * n + 2 * hw);
    tampon_.evr.reserve(4 * n);
    s.ham.reserve(n);
}

void Demet::izle(const DemetGirdisi& g, DemetSonucu& s)
{
    // Dinlenen en uzak menzil (çekirdek payıyla).
    const double r_maks = 0.5 * g.c * dinleme_sonu(g.tau0, g.t_s, g.n_kutu, g.darbe_s);

    const Desen& p = g.desen;
    const double wh = p.teta0;
    const double wv = p.fi0;
    const Izgara z = izgara(g);
    const int nh = z.nh;
    const int nv = z.nv;
    const double dth = 2.0 * wh / static_cast<double>(nh);
    const double dph = 2.0 * wv / static_cast<double>(nv);
    const int nr = nh * nv;
    const double ln10 = std::log(10.0);

    const std::size_t kap = z.kap;
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
    katkilar_.clear();
    for (const Hucre& c : hucreler_) {
        if (c.sw > 0.0) {
            katkilar_.push_back({c.swt / c.sw, c.sw});
        }
    }
    biriktir(katkilar_, g.tau0, g.t_s, g.n_kutu, g.darbe_s, tampon_, s.ham);
    s.isin = static_cast<std::uint32_t>(nr);
    s.isabet = isabet;
}

}  // namespace ida::sonar::ic
