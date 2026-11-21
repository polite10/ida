#include "hidrostatik.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace ida::fizik::ic {

namespace {

V birim(const V& a) { return a / std::sqrt(nokta(a, a)); }

// Üçgenin yönünü dışbükey bir cisim için iç noktadan dışa bakacak biçimde düzeltir.
std::array<int, 3> disa_yonlendir(const std::vector<V>& k, std::array<int, 3> u, const V& ic_nokta)
{
    const V& a = k[static_cast<std::size_t>(u[0])];
    const V& b = k[static_cast<std::size_t>(u[1])];
    const V& c = k[static_cast<std::size_t>(u[2])];
    const V n = capraz(b - a, c - a);
    const V m = (a + b + c) / 3.0 - ic_nokta;
    if (nokta(n, m) < 0.0) {
        std::swap(u[1], u[2]);
    }
    return u;
}

double dortyuzlu_hacmi(const V& p0, const V& a, const V& b, const V& c)
{
    return nokta(a - p0, capraz(b - p0, c - p0)) / 6.0;
}

}  // namespace

void ag_ozelliklerini_hesapla(KapaliAg& ag)
{
    // Diverjans teoremi; referans noktası ilk köşe (yuvarlamayı küçük tutar).
    const V p0 = ag.koseler.empty() ? V{} : ag.koseler.front();
    double v = 0.0;
    V mom{};
    for (const auto& u : ag.ucgenler) {
        const V& a = ag.koseler[static_cast<std::size_t>(u[0])];
        const V& b = ag.koseler[static_cast<std::size_t>(u[1])];
        const V& c = ag.koseler[static_cast<std::size_t>(u[2])];
        const double dv = dortyuzlu_hacmi(p0, a, b, c);
        v += dv;
        mom += (dv / 4.0) * (p0 + a + b + c);
    }
    ag.hacim = v;
    ag.merkez = (v != 0.0) ? mom / v : V{};
    ag.sinir_merkezi = ag.merkez;
    double r2 = 0.0;
    for (const auto& k : ag.koseler) {
        const V f = k - ag.sinir_merkezi;
        r2 = std::max(r2, nokta(f, f));
    }
    ag.sinir_yaricapi = std::sqrt(r2);
}

KapaliAg kutu_agi(const V& merkez, const V& boyut)
{
    KapaliAg ag;
    const V y = boyut / 2.0;
    for (int i = 0; i < 8; ++i) {
        const double sx = (i & 1) ? 1.0 : -1.0;
        const double sy = (i & 2) ? 1.0 : -1.0;
        const double sz = (i & 4) ? 1.0 : -1.0;
        ag.koseler.push_back(merkez + V{sx * y.x, sy * y.y, sz * y.z});
    }
    // Altı yüz, her biri dört köşe (bit dizinleri); ikişer üçgen.
    const int yuzler[6][4] = {
        {0, 2, 6, 4},  // x−
        {1, 3, 7, 5},  // x+
        {0, 1, 5, 4},  // y−
        {2, 3, 7, 6},  // y+
        {0, 1, 3, 2},  // z−
        {4, 5, 7, 6},  // z+
    };
    for (const auto& f : yuzler) {
        ag.ucgenler.push_back(disa_yonlendir(ag.koseler, {f[0], f[1], f[2]}, merkez));
        ag.ucgenler.push_back(disa_yonlendir(ag.koseler, {f[0], f[2], f[3]}, merkez));
    }
    ag_ozelliklerini_hesapla(ag);
    return ag;
}

KapaliAg sferoid_agi(const V& merkez, const V& eksen, double a, double b, int n_boylam, int n_enlem)
{
    KapaliAg ag;
    const V e1 = birim(eksen);
    // e1'e dik iki birim vektör.
    const V yardimci = (std::abs(e1.x) < 0.9) ? V{1.0, 0.0, 0.0} : V{0.0, 1.0, 0.0};
    const V e2 = birim(capraz(e1, yardimci));
    const V e3 = capraz(e1, e2);
    const double pi = std::numbers::pi;

    ag.koseler.push_back(merkez + a * e1);  // 0: ön kutup
    for (int i = 1; i < n_enlem; ++i) {
        const double th = pi * static_cast<double>(i) / static_cast<double>(n_enlem);
        for (int j = 0; j < n_boylam; ++j) {
            const double ph = 2.0 * pi * static_cast<double>(j) / static_cast<double>(n_boylam);
            ag.koseler.push_back(merkez + (a * std::cos(th)) * e1 +
                                 (b * std::sin(th)) * (std::cos(ph) * e2 + std::sin(ph) * e3));
        }
    }
    const int arka = static_cast<int>(ag.koseler.size());
    ag.koseler.push_back(merkez - a * e1);  // arka kutup
    auto halka = [&](int ii, int jj) { return 1 + (ii - 1) * n_boylam + (jj % n_boylam); };
    for (int j = 0; j < n_boylam; ++j) {
        ag.ucgenler.push_back(disa_yonlendir(ag.koseler, {0, halka(1, j), halka(1, j + 1)}, merkez));
        ag.ucgenler.push_back(disa_yonlendir(
            ag.koseler, {arka, halka(n_enlem - 1, j + 1), halka(n_enlem - 1, j)}, merkez));
    }
    for (int i = 1; i < n_enlem - 1; ++i) {
        for (int j = 0; j < n_boylam; ++j) {
            const int p = halka(i, j), q = halka(i, j + 1), r = halka(i + 1, j + 1), s = halka(i + 1, j);
            ag.ucgenler.push_back(disa_yonlendir(ag.koseler, {p, q, r}, merkez));
            ag.ucgenler.push_back(disa_yonlendir(ag.koseler, {p, r, s}, merkez));
        }
    }
    ag_ozelliklerini_hesapla(ag);
    // Analitik hacme ölçekle (merkez etrafında tek düze).
    const double hedef = 4.0 / 3.0 * pi * a * b * b;
    const double k = std::cbrt(hedef / ag.hacim);
    for (auto& v : ag.koseler) {
        v = merkez + k * (v - merkez);
    }
    ag_ozelliklerini_hesapla(ag);
    return ag;
}

BatikSonuc batik_hesapla(const std::vector<KapaliAg>& aglar, const V& p_n, const M& R,
                         double kesinlik_derinligi, std::vector<V>& tampon)
{
    BatikSonuc s;
    s.en_ust_derinlik = HUGE_VAL;
    // Düzlem üzerindeki referans: O_b'nin yüzeydeki izdüşümü (yuvarlamayı küçük tutar).
    const V p0{p_n.x, p_n.y, 0.0};

    for (const auto& ag : aglar) {
        const V c_n = p_n + R * ag.sinir_merkezi;
        const double alt = c_n.z - ag.sinir_yaricapi;  // en üst noktanın derinliğinin alt sınırı
        const double ust = c_n.z + ag.sinir_yaricapi;

        if (alt >= 0.0 && alt >= kesinlik_derinligi) {
            s.hacim += ag.hacim;
            s.moment += ag.hacim * (p_n + R * ag.merkez);
            s.en_ust_derinlik = std::min(s.en_ust_derinlik, alt);
            continue;
        }
        if (ust <= 0.0) {
            s.en_ust_derinlik = std::min(s.en_ust_derinlik, alt);
            continue;
        }

        tampon.resize(ag.koseler.size());
        double en_az = HUGE_VAL;
        double en_cok = -HUGE_VAL;
        for (std::size_t i = 0; i < ag.koseler.size(); ++i) {
            tampon[i] = p_n + R * ag.koseler[i];
            en_az = std::min(en_az, tampon[i].z);
            en_cok = std::max(en_cok, tampon[i].z);
        }
        s.en_ust_derinlik = std::min(s.en_ust_derinlik, en_az);
        if (en_az >= 0.0) {
            s.hacim += ag.hacim;
            s.moment += ag.hacim * (p_n + R * ag.merkez);
            continue;
        }
        if (en_cok <= 0.0) {
            continue;
        }
        // Kırpma: batık yarı uzay z ≥ 0.
        for (const auto& u : ag.ucgenler) {
            V cokgen[4];
            int n = 0;
            for (std::size_t e = 0; e < 3; ++e) {
                const V& P = tampon[static_cast<std::size_t>(u[e])];
                const V& Q = tampon[static_cast<std::size_t>(u[(e + 1) % 3])];
                const bool p_ic = P.z >= 0.0;
                const bool q_ic = Q.z >= 0.0;
                if (p_ic) {
                    cokgen[n++] = P;
                }
                if (p_ic != q_ic) {
                    const double t = P.z / (P.z - Q.z);
                    V X = P + t * (Q - P);
                    X.z = 0.0;  // tam düzlemde: kapak dörtyüzlüleri tam sıfır
                    cokgen[n++] = X;
                }
            }
            for (int k = 1; k + 1 < n; ++k) {
                const double dv = dortyuzlu_hacmi(p0, cokgen[0], cokgen[k], cokgen[k + 1]);
                s.hacim += dv;
                s.moment += (dv / 4.0) * (p0 + cokgen[0] + cokgen[k] + cokgen[k + 1]);
            }
        }
    }
    return s;
}

}  // namespace ida::fizik::ic
