// Üçgen ağlı sonar sahnesi (ag_sahnesi.hpp; W4-sonar-sahne).
#include "ida/sonar/ag_sahnesi.hpp"

#include "vek.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <limits>
#include <map>
#include <tuple>

namespace ida::sonar {

namespace {

using ic::capraz;
using ic::carp;
using ic::carp_t;
using ic::cikar;
using ic::nokta;
using ic::olcekle;
using ic::topla;

constexpr double kBaryEps = 1e-9;          // ortak kenarda sızıntı olmasın (barisentrik pay)
constexpr std::uint32_t kYaprak = 4;       // yaprak başına en çok üçgen
constexpr int kYiginDerinligi = 64;        // ortanca bölmeyle derinlik ≤ ⌈log₂ n⌉ + 1

std::int32_t hucre(double x, double kenar)
{
    const double k = std::floor(x / kenar);
    const double s = static_cast<double>(std::numeric_limits<std::int32_t>::max() / 2);
    return static_cast<std::int32_t>(std::clamp(k, -s, s));
}

Vek3 birim(const Vek3& a)
{
    const double l = ic::uzunluk(a);
    return l > 0.0 ? olcekle(a, 1.0 / l) : Vek3{0.0, 0.0, 0.0};
}

// Kutu–ışın (dilim yöntemi); [t_min, t_ust] ile kesişiyorsa true.
bool kutu_kesis(const Vek3& mn, const Vek3& mx, const Vek3& o, const Vek3& ters, const Vek3& d, double t_min,
                double t_ust)
{
    double t0 = t_min;
    double t1 = t_ust;
    for (std::size_t k = 0; k < 3; ++k) {
        if (d[k] == 0.0) {
            if (o[k] < mn[k] || o[k] > mx[k]) return false;
            continue;
        }
        double a = (mn[k] - o[k]) * ters[k];
        double b = (mx[k] - o[k]) * ters[k];
        if (a > b) std::swap(a, b);
        t0 = std::max(t0, a);
        t1 = std::min(t1, b);
        if (t0 > t1) return false;
    }
    return true;
}

}  // namespace

std::uint16_t AgSahnesi::malzeme_ekle(const AkustikMalzeme& m)
{
    malzemeler_.push_back(m);
    return static_cast<std::uint16_t>(malzemeler_.size() - 1);
}

bool AgSahnesi::kimlik_bos(std::uint32_t kimlik) const
{
    if (yuzey_var_ && yuzey_kimlik_ == kimlik) return false;
    for (const Govde& g : govdeler_) {
        if (g.kimlik == kimlik) return false;
    }
    return true;
}

bool AgSahnesi::su_yuzeyi_ekle(std::uint32_t kimlik, std::uint16_t malzeme, std::string* hata)
{
    if (yuzey_var_ || !kimlik_bos(kimlik) || malzeme >= malzemeler_.size()) {
        if (hata != nullptr) *hata = "su yüzeyi eklenemez (yinelenen, kimlik ya da malzeme geçersiz)";
        return false;
    }
    yuzey_var_ = true;
    yuzey_kimlik_ = kimlik;
    yuzey_malzeme_ = malzeme;
    return true;
}

bool AgSahnesi::govde_ekle(const AgGovdesi& g, std::string* hata)
{
    auto red = [&](const std::string& m) {
        if (hata != nullptr) *hata = "ağ gövdesi " + std::to_string(g.kimlik) + ": " + m;
        return false;
    };
    if (!kimlik_bos(g.kimlik)) return red("kimlik tekil değil");
    if (g.ag.ucgenler.empty()) return red("üçgen yok");
    if (g.ag.malzeme.size() != g.ag.ucgenler.size()) return red("üçgen başına malzeme yok");
    if (g.ag.ucgenler.size() >= std::numeric_limits<std::uint32_t>::max() / 2u) return red("üçgen sayısı çok büyük");
    if (!ic::sonlu(g.konum) || !std::isfinite(g.yonelim[0]) || !std::isfinite(g.yonelim[1]) ||
        !std::isfinite(g.yonelim[2]) || !std::isfinite(g.yonelim[3])) {
        return red("poz sonlu değil");
    }
    if (g.hareket.tur == HareketTuru::sinus && g.hareket.periyot_ns <= 0) return red("sinüs periyodu ≤ 0");
    if (!ic::sonlu(g.hareket.hiz_ned) || !ic::sonlu(g.hareket.genlik_ned) || !std::isfinite(g.hareket.faz)) {
        return red("hareket sonlu değil");
    }
    for (const Vek3& p : g.ag.koseler) {
        if (!ic::sonlu(p)) return red("köşe sonlu değil");
    }
    Govde n;
    n.kimlik = g.kimlik;
    n.ucgenler.reserve(g.ag.ucgenler.size());
    // Eş düzlemli kümeler: normal (10⁻⁶) ve düzlem uzaklığı (0,1 mm) nicemlenir; kümenin eksenleri
    // ilk üçgeninden.
    std::map<std::tuple<long long, long long, long long, long long>, std::uint32_t> kumeler;
    std::vector<std::pair<Vek3, Vek3>> eksenler;
    for (std::size_t i = 0; i < g.ag.ucgenler.size(); ++i) {
        const auto& u = g.ag.ucgenler[i];
        if (u[0] >= g.ag.koseler.size() || u[1] >= g.ag.koseler.size() || u[2] >= g.ag.koseler.size()) {
            return red("indeks köşe sayısını aşıyor");
        }
        if (g.ag.malzeme[i] >= malzemeler_.size()) return red("malzeme indisi geçersiz");
        Ucgen t;
        t.v0 = g.ag.koseler[u[0]];
        t.e1 = cikar(g.ag.koseler[u[1]], t.v0);
        t.e2 = cikar(g.ag.koseler[u[2]], t.v0);
        t.n = birim(capraz(t.e1, t.e2));
        t.sira = static_cast<std::uint32_t>(i);
        t.malzeme = g.ag.malzeme[i];
        const auto anahtar = std::make_tuple(std::llround(t.n[0] * 1e6), std::llround(t.n[1] * 1e6),
                                             std::llround(t.n[2] * 1e6), std::llround(nokta(t.n, t.v0) * 1e4));
        const auto [it, yeni] = kumeler.emplace(anahtar, static_cast<std::uint32_t>(eksenler.size()));
        if (yeni) {
            const Vek3 yard = std::abs(t.n[2]) < 0.9 ? Vek3{0.0, 0.0, 1.0} : Vek3{1.0, 0.0, 0.0};
            const Vek3 ue = birim(capraz(yard, t.n));
            eksenler.emplace_back(ue, capraz(t.n, ue));
        }
        t.yuzey = it->second;
        t.u_ekseni = eksenler[it->second].first;
        t.w_ekseni = eksenler[it->second].second;
        n.ucgenler.push_back(t);
    }
    n.dugumler.reserve(2u * (n.ucgenler.size() / kYaprak + 1u));
    (void)agac_kur(n, 0u, static_cast<std::uint32_t>(n.ucgenler.size()));
    govdeler_.push_back(std::move(n));
    if (!govde_pozu_kur(g.kimlik, g.konum, g.yonelim, g.hareket)) {
        govdeler_.pop_back();
        return red("poz kurulamadı");
    }
    return true;
}

std::uint32_t AgSahnesi::agac_kur(Govde& g, std::uint32_t bas, std::uint32_t son)
{
    const std::uint32_t idx = static_cast<std::uint32_t>(g.dugumler.size());
    g.dugumler.push_back({});
    Vek3 mn{std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity(),
            std::numeric_limits<double>::infinity()};
    Vek3 mx{-mn[0], -mn[1], -mn[2]};
    Vek3 cmn = mn, cmx = mx;
    for (std::uint32_t i = bas; i < son; ++i) {
        const Ucgen& t = g.ucgenler[i];
        const Vek3 p[3] = {t.v0, topla(t.v0, t.e1), topla(t.v0, t.e2)};
        for (std::size_t k = 0; k < 3; ++k) {
            double c = 0.0;
            for (const Vek3& q : p) {
                mn[k] = std::min(mn[k], q[k]);
                mx[k] = std::max(mx[k], q[k]);
                c += q[k];
            }
            cmn[k] = std::min(cmn[k], c / 3.0);
            cmx[k] = std::max(cmx[k], c / 3.0);
        }
    }
    // Barisentrik payla kabul edilen kenar kesişimleri kutudan dışarıda kalmasın.
    for (std::size_t k = 0; k < 3; ++k) {
        const double pay = 1e-9 * (std::abs(mn[k]) + std::abs(mx[k]) + 1.0);
        mn[k] -= pay;
        mx[k] += pay;
    }
    g.dugumler[idx].mn = mn;
    g.dugumler[idx].mx = mx;
    const std::uint32_t n = son - bas;
    std::size_t eks = 0;
    for (std::size_t k = 1; k < 3; ++k) {
        if (cmx[k] - cmn[k] > cmx[eks] - cmn[eks]) eks = k;
    }
    if (n <= kYaprak || !(cmx[eks] - cmn[eks] > 0.0)) {
        g.dugumler[idx].ilk = bas;
        g.dugumler[idx].sayi = n;
        return idx;
    }
    // Ortanca bölme; sıralama anahtarı (ağırlık merkezi, sıra) tam bir sıradır (belirlenimci yapı).
    auto merkez = [eks](const Ucgen& t) { return 3.0 * t.v0[eks] + t.e1[eks] + t.e2[eks]; };
    std::sort(g.ucgenler.begin() + bas, g.ucgenler.begin() + son, [&](const Ucgen& a, const Ucgen& b) {
        const double x = merkez(a), y = merkez(b);
        return x < y || (x == y && a.sira < b.sira);
    });
    const std::uint32_t orta = bas + n / 2u;
    (void)agac_kur(g, bas, orta);
    const std::uint32_t sag = agac_kur(g, orta, son);
    g.dugumler[idx].sag = sag;
    g.dugumler[idx].sayi = 0;
    return idx;
}

bool AgSahnesi::govde_pozu_kur(std::uint32_t kimlik, const Vek3& konum, const Kuaterniyon& yonelim, const Hareket& h)
{
    const double qn = std::sqrt(yonelim[0] * yonelim[0] + yonelim[1] * yonelim[1] + yonelim[2] * yonelim[2] +
                                yonelim[3] * yonelim[3]);
    if (!ic::sonlu(konum) || !(qn > 0.0) || !std::isfinite(qn) || (h.tur == HareketTuru::sinus && h.periyot_ns <= 0)) {
        return false;
    }
    for (Govde& g : govdeler_) {
        if (g.kimlik == kimlik) {
            g.konum = konum;
            g.yonelim = ic::normalle(yonelim);
            g.r = ic::matris(g.yonelim);
            g.hareket = h;
            return true;
        }
    }
    return false;
}

Vek3 AgSahnesi::konum_(const Govde& g, std::int64_t t_ns) const
{
    switch (g.hareket.tur) {
    case HareketTuru::sabit: return g.konum;
    case HareketTuru::sabit_hiz: return topla(g.konum, olcekle(g.hareket.hiz_ned, static_cast<double>(t_ns) * 1e-9));
    case HareketTuru::sinus: {
        // Evre tamsayı kalanla (uzun koşuda duyarlık kaybı yok).
        const std::int64_t p = g.hareket.periyot_ns;
        std::int64_t m = t_ns % p;
        if (m < 0) m += p;
        const double s = std::sin(2.0 * ic::kPi * static_cast<double>(m) / static_cast<double>(p) + g.hareket.faz);
        return topla(g.konum, olcekle(g.hareket.genlik_ned, s));
    }
    }
    return g.konum;
}

bool AgSahnesi::govde_konumu(std::uint32_t kimlik, std::int64_t t_ns, Vek3& p) const
{
    for (const Govde& g : govdeler_) {
        if (g.kimlik == kimlik) {
            p = konum_(g, t_ns);
            return true;
        }
    }
    return false;
}

std::size_t AgSahnesi::ucgen_sayisi(std::uint32_t kimlik) const
{
    for (const Govde& g : govdeler_) {
        if (g.kimlik == kimlik) return g.ucgenler.size();
    }
    return 0;
}

std::size_t AgSahnesi::toplam_ucgen() const
{
    std::size_t n = 0;
    for (const Govde& g : govdeler_) n += g.ucgenler.size();
    return n;
}

bool AgSahnesi::kesis(const Vek3& o, const Vek3& d, double t_min, double t_maks, std::int64_t t_ns, double hucre_kenari,
                      Vurus& v) const
{
    bool var = false;
    double en_yakin = t_maks;
    // Su yüzeyi (z = 0).
    if (yuzey_var_ && d[2] != 0.0) {
        const double t = -o[2] / d[2];
        if (t > t_min && t <= en_yakin) {
            var = true;
            en_yakin = t;
            v.t = t;
            v.nokta = topla(o, olcekle(d, t));
            v.normal = {0.0, 0.0, d[2] > 0.0 ? -1.0 : 1.0};
            v.govde = yuzey_kimlik_;
            v.yuzey = 0;
            v.hucre_i = hucre(v.nokta[0], hucre_kenari);
            v.hucre_j = hucre(v.nokta[1], hucre_kenari);
            v.malzeme = yuzey_malzeme_;
        }
    }
    for (const Govde& g : govdeler_) {
        const Vek3 p = konum_(g, t_ns);
        const Vek3 ol = carp_t(g.r, cikar(o, p));
        const Vek3 dl = carp_t(g.r, d);
        const Vek3 ters{1.0 / dl[0], 1.0 / dl[1], 1.0 / dl[2]};
        const Ucgen* en = nullptr;
        double t_en = en_yakin;
        std::uint32_t yigin[kYiginDerinligi];
        int ust = 0;
        yigin[ust++] = 0u;
        while (ust > 0) {
            const Dugum& dg = g.dugumler[yigin[--ust]];
            if (!kutu_kesis(dg.mn, dg.mx, ol, ters, dl, t_min, t_en)) continue;
            if (dg.sayi == 0u) {
                const std::uint32_t sol = static_cast<std::uint32_t>(&dg - g.dugumler.data()) + 1u;
                if (ust + 2 > kYiginDerinligi) std::abort();   // ortanca bölmeyle ulaşılamaz
                // Sol çocuk önce; seçilen üçgen ziyaret sırasından bağımsızdır (eşitlik kuralı).
                yigin[ust++] = dg.sag;
                yigin[ust++] = sol;
                continue;
            }
            for (std::uint32_t i = dg.ilk; i < dg.ilk + dg.sayi; ++i) {
                const Ucgen& u = g.ucgenler[i];
                const Vek3 pv = capraz(dl, u.e2);
                const double det = nokta(u.e1, pv);
                if (det == 0.0 || !std::isfinite(det)) continue;
                const double ters_det = 1.0 / det;
                const Vek3 s = cikar(ol, u.v0);
                const double bu = nokta(s, pv) * ters_det;
                if (bu < -kBaryEps || bu > 1.0 + kBaryEps) continue;
                const Vek3 q = capraz(s, u.e1);
                const double bv = nokta(dl, q) * ters_det;
                if (bv < -kBaryEps || bu + bv > 1.0 + kBaryEps) continue;
                const double t = nokta(u.e2, q) * ters_det;
                if (!(t > t_min) || t > t_en) continue;
                if (t == t_en) {
                    if (en == nullptr) {
                        if (var) continue;                 // önceki gövde (ya da yüzey) eşitlikte kalır
                    } else if (u.sira > en->sira) {
                        continue;                          // gövde içinde küçük sıra kalır
                    }
                }
                en = &u;
                t_en = t;
            }
        }
        if (en == nullptr) continue;
        var = true;
        en_yakin = t_en;
        v.t = t_en;
        v.nokta = topla(o, olcekle(d, t_en));
        Vek3 nn = carp(g.r, en->n);
        if (nokta(nn, d) > 0.0) nn = olcekle(nn, -1.0);
        v.normal = nn;
        v.govde = g.kimlik;
        v.yuzey = en->yuzey;
        const Vek3 pl = topla(ol, olcekle(dl, t_en));
        v.hucre_i = hucre(nokta(pl, en->u_ekseni), hucre_kenari);
        v.hucre_j = hucre(nokta(pl, en->w_ekseni), hucre_kenari);
        v.malzeme = en->malzeme;
    }
    return var;
}

}  // namespace ida::sonar
