// Çözümsel sonar sahnesi (sahne.hpp).
#include "ida/sonar/sahne.hpp"

#include "vek.hpp"

#include <algorithm>
#include <limits>

namespace ida::sonar {

namespace {

using ic::carp;
using ic::carp_t;
using ic::cikar;
using ic::nokta;
using ic::olcekle;
using ic::topla;

constexpr double kPayEps = 1e-12;

std::int32_t hucre(double x, double kenar)
{
    const double k = std::floor(x / kenar);
    const double s = static_cast<double>(std::numeric_limits<std::int32_t>::max() / 2);
    return static_cast<std::int32_t>(std::clamp(k, -s, s));
}

// Yerel çerçevede kesişim adayı.
struct Aday {
    double t = 0.0;
    Vek3 n{};              // yerel, dışa bakan normal
    std::uint32_t yuzey = 0;
    double u = 0.0;        // yerel yüzey koordinatları (m)
    double w = 0.0;
};

bool kure_kesis(double r, const Vek3& o, const Vek3& d, double t_min, double t_maks, Aday& a)
{
    const double b = nokta(o, d);
    const double c = nokta(o, o) - r * r;
    const double disk = b * b - c;
    if (disk < 0.0) {
        return false;
    }
    const double kok = std::sqrt(disk);
    double t = -b - kok;
    if (!(t > t_min)) {
        t = -b + kok;
    }
    if (!(t > t_min) || t > t_maks) {
        return false;
    }
    const Vek3 p = topla(o, olcekle(d, t));
    a.t = t;
    a.n = olcekle(p, 1.0 / r);
    a.yuzey = 0;
    // Gövdeye sabit küresel hücreler: kutup açısı bandı ve bant içinde yay uzunluğu.
    const double teta = std::acos(std::clamp(a.n[2], -1.0, 1.0));
    const double fi = std::atan2(a.n[1], a.n[0]) + ic::kPi;
    a.u = r * teta;
    a.w = fi * r;   // band çarpanı hücre indisinde uygulanır
    return true;
}

bool kutu_kesis(const Vek3& h, const Vek3& o, const Vek3& d, double t_min, double t_maks, Aday& a)
{
    double t_gir = -std::numeric_limits<double>::infinity();
    double t_cik = std::numeric_limits<double>::infinity();
    std::size_t eks_gir = 0, eks_cik = 0;
    for (std::size_t k = 0; k < 3; ++k) {
        if (std::abs(d[k]) < kPayEps) {
            if (o[k] < -h[k] || o[k] > h[k]) {
                return false;
            }
            continue;
        }
        double t1 = (-h[k] - o[k]) / d[k];
        double t2 = (h[k] - o[k]) / d[k];
        if (t1 > t2) {
            std::swap(t1, t2);
        }
        if (t1 > t_gir) {
            t_gir = t1;
            eks_gir = k;
        }
        if (t2 < t_cik) {
            t_cik = t2;
            eks_cik = k;
        }
    }
    if (t_gir > t_cik) {
        return false;
    }
    double t = 0.0;
    std::size_t eks = 0;
    bool arti = false;
    if (t_gir > t_min) {
        t = t_gir;
        eks = eks_gir;
        arti = d[eks] < 0.0;       // + yüzden girer
    } else if (t_cik > t_min) {
        t = t_cik;
        eks = eks_cik;
        arti = d[eks] > 0.0;       // + yüzden çıkar
    } else {
        return false;
    }
    if (t > t_maks) {
        return false;
    }
    const Vek3 p = topla(o, olcekle(d, t));
    a.t = t;
    a.n = {0.0, 0.0, 0.0};
    a.n[eks] = arti ? 1.0 : -1.0;
    a.yuzey = static_cast<std::uint32_t>(2 * eks + (arti ? 1u : 0u));
    a.u = p[(eks + 1) % 3];
    a.w = p[(eks + 2) % 3];
    return true;
}

bool silindir_kesis(double r, double yuk, const Vek3& o, const Vek3& d, double t_min, double t_maks,
                    Aday& a)
{
    bool var = false;
    const double hz = 0.5 * yuk;
    auto dene = [&](double t, std::uint32_t yuzey) {
        if (!(t > t_min) || t > t_maks || (var && t >= a.t)) {
            return;
        }
        const Vek3 p = topla(o, olcekle(d, t));
        if (yuzey == 0) {
            if (std::abs(p[2]) > hz) return;
            a.n = {p[0] / r, p[1] / r, 0.0};
            a.u = p[2];
            a.w = r * (std::atan2(p[1], p[0]) + ic::kPi);
        } else {
            if (p[0] * p[0] + p[1] * p[1] > r * r) return;
            a.n = {0.0, 0.0, yuzey == 1 ? -1.0 : 1.0};
            a.u = p[0];
            a.w = p[1];
        }
        a.t = t;
        a.yuzey = yuzey;
        var = true;
    };
    const double aa = d[0] * d[0] + d[1] * d[1];
    if (aa > kPayEps) {
        const double b = o[0] * d[0] + o[1] * d[1];
        const double c = o[0] * o[0] + o[1] * o[1] - r * r;
        const double disk = b * b - aa * c;
        if (disk >= 0.0) {
            const double kok = std::sqrt(disk);
            dene((-b - kok) / aa, 0);
            dene((-b + kok) / aa, 0);
        }
    }
    if (std::abs(d[2]) > kPayEps) {
        dene((-hz - o[2]) / d[2], 1);
        dene((hz - o[2]) / d[2], 2);
    }
    return var;
}

bool duzlem_kesis(const Vek3& o, const Vek3& d, double t_min, double t_maks, Aday& a)
{
    if (std::abs(d[2]) < kPayEps) {
        return false;
    }
    const double t = -o[2] / d[2];
    if (!(t > t_min) || t > t_maks) {
        return false;
    }
    a.t = t;
    a.n = {0.0, 0.0, 1.0};
    a.yuzey = 0;
    a.u = o[0] + t * d[0];
    a.w = o[1] + t * d[1];
    return true;
}

bool sekil_gecerli(const Sekil& s)
{
    switch (s.tur) {
    case SekilTuru::kure: return std::isfinite(s.yaricap) && s.yaricap > 0.0;
    case SekilTuru::kutu:
        return ic::sonlu(s.yari_boyut) && s.yari_boyut[0] > 0.0 && s.yari_boyut[1] > 0.0 &&
               s.yari_boyut[2] > 0.0;
    case SekilTuru::silindir:
        return std::isfinite(s.yaricap) && s.yaricap > 0.0 && std::isfinite(s.yukseklik) &&
               s.yukseklik > 0.0;
    case SekilTuru::duzlem: return true;
    }
    return false;
}

}  // namespace

AkustikMalzeme HavuzTanimi::su_yuzeyi_malzemesi()
{
    // Hava–su sınırı [T]: düz yüzeyin geri saçılımı çok küçüktür; μ yer tutucudur. R_s ≈ −1
    // (basınçta yumuşak sınır; 05 §2.4.4), tam kademede kullanılır.
    return {"su_yuzeyi", 1e-4, GelisYasasi::lambert_cos2, -1.0, 1.0};
}

std::uint16_t CozumselSahne::malzeme_ekle(const AkustikMalzeme& m)
{
    malzemeler_.push_back(m);
    return static_cast<std::uint16_t>(malzemeler_.size() - 1);
}

bool CozumselSahne::govde_ekle(const Govde& g)
{
    if (!sekil_gecerli(g.sekil) || g.malzemeler.empty() || !ic::sonlu(g.konum) ||
        !ic::sonlu(g.hiz_ned)) {
        return false;
    }
    for (std::uint16_t m : g.malzemeler) {
        if (m >= malzemeler_.size()) return false;
    }
    for (const Govde& b : govdeler_) {
        if (b.kimlik == g.kimlik) return false;
    }
    Govde n = g;
    n.yonelim = ic::normalle(g.yonelim);
    govdeler_.push_back(n);
    return true;
}

bool CozumselSahne::govde_pozu_kur(std::uint32_t kimlik, const Vek3& konum,
                                   const Kuaterniyon& yonelim, const Vek3& hiz_ned,
                                   std::int64_t t0_ns)
{
    for (Govde& g : govdeler_) {
        if (g.kimlik == kimlik) {
            g.konum = konum;
            g.yonelim = ic::normalle(yonelim);
            g.hiz_ned = hiz_ned;
            g.t0_ns = t0_ns;
            return true;
        }
    }
    return false;
}

bool CozumselSahne::kesis(const Vek3& o, const Vek3& d, double t_min, double t_maks,
                          std::int64_t t_ns, double hucre_kenari, Vurus& v) const
{
    bool var = false;
    double en_yakin = t_maks;
    for (const Govde& g : govdeler_) {
        const double dt = static_cast<double>(t_ns - g.t0_ns) * 1e-9;
        const Vek3 p = topla(g.konum, olcekle(g.hiz_ned, dt));
        const fizik::Mat3 r = ic::matris(g.yonelim);
        const Vek3 ol = carp_t(r, cikar(o, p));
        const Vek3 dl = carp_t(r, d);
        Aday a;
        bool k = false;
        switch (g.sekil.tur) {
        case SekilTuru::kure: k = kure_kesis(g.sekil.yaricap, ol, dl, t_min, en_yakin, a); break;
        case SekilTuru::kutu: k = kutu_kesis(g.sekil.yari_boyut, ol, dl, t_min, en_yakin, a); break;
        case SekilTuru::silindir:
            k = silindir_kesis(g.sekil.yaricap, g.sekil.yukseklik, ol, dl, t_min, en_yakin, a);
            break;
        case SekilTuru::duzlem: k = duzlem_kesis(ol, dl, t_min, en_yakin, a); break;
        }
        if (!k) {
            continue;
        }
        var = true;
        en_yakin = a.t;
        v.t = a.t;
        v.nokta = topla(o, olcekle(d, a.t));
        Vek3 n = carp(r, a.n);
        if (nokta(n, d) > 0.0) {
            n = olcekle(n, -1.0);
        }
        v.normal = n;
        v.govde = g.kimlik;
        v.yuzey = a.yuzey;
        v.hucre_i = hucre(a.u, hucre_kenari);
        if (g.sekil.tur == SekilTuru::kure) {
            // Bant merkezindeki çevre çarpanı: hücreler yaklaşık kare kalır.
            const double teta_c = (static_cast<double>(v.hucre_i) + 0.5) * hucre_kenari / g.sekil.yaricap;
            const double s = std::max(std::abs(std::sin(teta_c)), hucre_kenari / g.sekil.yaricap);
            v.hucre_j = hucre(a.w * s, hucre_kenari);
        } else {
            v.hucre_j = hucre(a.w, hucre_kenari);
        }
        const std::size_t mi = std::min<std::size_t>(a.yuzey, g.malzemeler.size() - 1);
        v.malzeme = g.malzemeler[mi];
    }
    return var;
}

void havuz_ekle(CozumselSahne& s, const HavuzTanimi& h)
{
    const std::uint16_t b = s.malzeme_ekle(h.beton);
    const std::uint16_t y = s.malzeme_ekle(h.yuzey);
    Govde g;
    g.kimlik = 1;
    g.sekil.tur = SekilTuru::kutu;
    g.sekil.yari_boyut = {0.5 * h.uzunluk, 0.5 * h.genislik, 0.5 * h.derinlik};
    g.konum = topla(h.orijin, {0.5 * h.uzunluk, 0.0, 0.5 * h.derinlik});
    g.malzemeler = {b, b, b, b, y, b};   // −z yüzü (z = 0) su yüzeyi, +z yüzü taban
    s.govde_ekle(g);
}

void acik_su_ekle(CozumselSahne& s, double derinlik, const AkustikMalzeme& taban,
                  const AkustikMalzeme& yuzey)
{
    const std::uint16_t b = s.malzeme_ekle(taban);
    const std::uint16_t y = s.malzeme_ekle(yuzey);
    Govde g;
    g.kimlik = 1;
    g.sekil.tur = SekilTuru::duzlem;
    g.konum = {0.0, 0.0, derinlik};
    g.malzemeler = {b};
    s.govde_ekle(g);
    g.kimlik = 2;
    g.konum = {0.0, 0.0, 0.0};
    g.malzemeler = {y};
    s.govde_ekle(g);
}

}  // namespace ida::sonar
