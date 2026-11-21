#include "ida/fizik/kutle_durumu.hpp"

#include "matematik.hpp"

#include <numbers>

namespace ida::fizik {

using namespace ic;

KutleDurumu kutle_durumu_turet(const AracParametreleri& arac,
                               const std::array<bool, kFirlaticiSayisi>& yuklu)
{
    // (1) boş araç
    double m = arac.kutle.kutle;
    V3 mrg = m * v3(arac.kutle.r_g);           // Σ m r_g
    double hacim = arac.kutle.hacim;
    V3 vrb = hacim * v3(arac.kutle.r_b);       // Σ ∇ r_b
    M3 I = m3(arac.kutle.atalet);

    auto nokta_kutle = [](double mk, const V3& r) {
        return M3(mk * (r.squaredNorm() * M3::Identity() - r * r.transpose()));
    };

    // (2) trim kütlesi: hacimsiz nokta kütle (A5-K24)
    if (arac.kutle.trim_kutlesi > 0.0) {
        const double mt = arac.kutle.trim_kutlesi;
        const V3 rt = v3(arac.kutle.trim_konumu);
        m += mt;
        mrg += mt * rt;
        I += nokta_kutle(mt, rt);
    }

    // (3) dolu tüplerdeki torpidolar
    const double a = arac.torpido.boy / 2.0;
    const double b = arac.torpido.cap / 2.0;
    const double mtor = arac.torpido.kutle;
    const double vtor = 4.0 / 3.0 * std::numbers::pi * a * b * b;
    const double Ieks = 2.0 / 5.0 * mtor * b * b;               // eksen boyunca
    const double Idik = 1.0 / 5.0 * mtor * (a * a + b * b);     // eksene dik
    for (std::size_t k = 0; k < static_cast<std::size_t>(kFirlaticiSayisi); ++k) {
        if (!yuklu[k]) {
            continue;
        }
        const V3 rk = v3(arac.firlaticilar[k].tup_konumu);
        const V3 e = v3(arac.firlaticilar[k].tup_ekseni).normalized();
        // Kendi merkezine göre atalet: Idik I + (Ieks − Idik) e eᵀ
        const M3 Ic = Idik * M3::Identity() + (Ieks - Idik) * e * e.transpose();
        m += mtor;
        mrg += mtor * rk;
        I += Ic + nokta_kutle(mtor, rk);
        hacim += vtor;
        vrb += vtor * rk;
    }

    KutleDurumu s;
    s.yuklu = yuklu;
    s.kutle = m;
    s.r_g = dizi(V3(mrg / m));
    s.hacim = hacim;
    s.r_b = dizi(V3(vrb / hacim));
    s.atalet = dizi(I);
    return s;
}

}  // namespace ida::fizik
