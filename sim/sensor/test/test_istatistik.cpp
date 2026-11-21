// Kabul 2: gürültü açıkken uzun örneklemde ortalama, standart sapma ve (IMU) Allan sapması.
//
// Toleranslar örneklem büyüklüğünden: standart sapmanın göreli hatası ≈ 1/√(2N), Allan
// sapmasınınki ≈ 1/√(2(K−1)) (K küme sayısı); eşikler bunların ≥ 3 katıdır (sabit tohum).
#include "test_destek.hpp"

namespace ida::sensor::test {

namespace {

struct Ozet {
    double ort = 0.0, ss = 0.0;
};

Ozet ozet(const std::vector<double>& x)
{
    Ozet o;
    for (double v : x) o.ort += v;
    o.ort /= static_cast<double>(x.size());
    for (double v : x) o.ss += (v - o.ort) * (v - o.ort);
    o.ss = std::sqrt(o.ss / static_cast<double>(x.size() - 1));
    return o;
}

// Örtüşmesiz Allan sapması; m örneklik kümeler.
double allan(const std::vector<double>& x, std::size_t m)
{
    const std::size_t k = x.size() / m;
    std::vector<double> ort(k, 0.0);
    for (std::size_t i = 0; i < k; ++i) {
        for (std::size_t j = 0; j < m; ++j) ort[i] += x[i * m + j];
        ort[i] /= static_cast<double>(m);
    }
    double s = 0.0;
    for (std::size_t i = 0; i + 1 < k; ++i) s += (ort[i + 1] - ort[i]) * (ort[i + 1] - ort[i]);
    return std::sqrt(s / (2.0 * static_cast<double>(k - 1)));
}

double ozilinti(const std::vector<double>& x, std::size_t gecikme)
{
    const Ozet o = ozet(x);
    double s = 0.0;
    for (std::size_t i = 0; i + gecikme < x.size(); ++i) s += (x[i] - o.ort) * (x[i + gecikme] - o.ort);
    return s / static_cast<double>(x.size() - gecikme) / (o.ss * o.ss);
}

}  // namespace

int test_istatistik_imu()
{
    Denetci d;
    const double sure = 300.0;
    const std::uint64_t adim = static_cast<std::uint64_t>(sure / 1.25e-3);
    {
        // Yalnız beyaz gürültü (yoğunluklar profil değerinde).
        SensorKumesiAyari a = ideal_ayar();
        yalniz(a, {SensorTuru::imu});
        auto& p = ayar_govdesi<ImuAyar>(a, SensorTuru::imu);
        p.bit = 24;
        p.jiroskop_yogunluk = 4.887e-5;
        p.ivme_yogunluk = {0.0006374, 0.0006374, 0.0006865};
        Kosu k;
        if (!k.kur(a)) return 1;
        if (!k.kos(durgun({0, 0, 1}, {1, 0, 0, 0}), 0, adim)) return 1;
        std::vector<double> gx, az;
        for (const auto& o : k.tur(SensorTuru::imu)) {
            gx.push_back(static_cast<double>(yuk<ImuOlcumu>(o).gyro[0]));
            az.push_back(static_cast<double>(yuk<ImuOlcumu>(o).accel[2]) + kG);
        }
        const double n = static_cast<double>(gx.size());
        const double sg = 4.887e-5 / std::sqrt(2.0 * 0.005);
        const double sa = 0.0006865 / std::sqrt(2.0 * 0.005);
        const Ozet og = ozet(gx), oa = ozet(az);
        std::printf("  jiroskop x: ort %.3g, ss %.6g (beklenen %.6g); ivme z: ss %.6g (beklenen %.6g)\n",
                    og.ort, og.ss, sg, oa.ss, sa);
        YAKIN(d, og.ort, 0.0, 4.0 * sg / std::sqrt(n), "jiroskop ortalaması");
        YAKIN(d, og.ss, sg, 0.03 * sg, "jiroskop ayrık σ = ASD/√(2Δt)");
        YAKIN(d, oa.ss, sa, 0.03 * sa, "ivme z ayrık σ");
        // Allan sapması: σ_A(τ) = N/√τ, N = ASD/√2 (05 §2.3, beyaz gürültü eğimi −1/2).
        const double nk = 4.887e-5 / std::sqrt(2.0);
        for (double tau : {0.1, 1.0}) {
            const double ad = allan(gx, static_cast<std::size_t>(tau / 0.005 + 0.5));
            const double bek = nk / std::sqrt(tau);
            std::printf("  Allan τ = %.1f s: %.6g (beklenen %.6g)\n", tau, ad, bek);
            YAKIN(d, ad, bek, (tau < 0.5 ? 0.06 : 0.15) * bek, "Allan sapması");
        }
    }
    {
        // Yalnız Gauss–Markov sapması (σ = 1e-3 rad/s, τ = 2 s): durağan σ ve özilinti.
        SensorKumesiAyari a = ideal_ayar();
        yalniz(a, {SensorTuru::imu});
        auto& p = ayar_govdesi<ImuAyar>(a, SensorTuru::imu);
        p.bit = 24;
        p.jiroskop_gm_sigma = 1e-3;
        p.jiroskop_gm_tau_ns = 2'000'000'000;
        Kosu k;
        if (!k.kur(a)) return 1;
        if (!k.kos(durgun({0, 0, 1}, {1, 0, 0, 0}), 0, adim)) return 1;
        std::vector<double> gx;
        for (const auto& o : k.tur(SensorTuru::imu)) gx.push_back(static_cast<double>(yuk<ImuOlcumu>(o).gyro[0]));
        const Ozet o = ozet(gx);
        const double r = ozilinti(gx, 400);
        std::printf("  GM: ss %.6g (beklenen 1e-3), özilinti(2 s) %.3f (beklenen %.3f)\n", o.ss, r,
                    std::exp(-1.0));
        YAKIN(d, o.ss, 1e-3, 0.25e-3, "GM durağan σ");
        YAKIN(d, r, std::exp(-1.0), 0.15, "GM özilintisi τ'da");
    }
    return d.hata;
}

int test_istatistik_digerleri()
{
    Denetci d;
    const double sure = 300.0;
    const std::uint64_t adim = static_cast<std::uint64_t>(sure / 1.25e-3);
    SensorKumesiAyari a = ideal_ayar();
    ayar_govdesi<ManyetometreAyar>(a, SensorTuru::manyetometre).gurultu_sigma = 2e-8;
    ayar_govdesi<BasincAyar>(a, SensorTuru::basinc).gurultu_sigma = 20.0;
    ayar_govdesi<GnssAyar>(a, SensorTuru::gnss).hiz_sigma = 0.05;
    ayar_govdesi<GnssAyar>(a, SensorTuru::gnss).cep = 2.0;
    auto& dv = ayar_govdesi<DvlAyar>(a, SensorTuru::dvl);
    dv.sigma0 = 0.001;
    dv.k_r = 0.0002;
    Kosu k;
    if (!k.kur(a)) return 1;
    // Yüzeyde (anten kuru, DVL ve basınç ağzı suda), düz.
    const Kuaterniyon q{1, 0, 0, 0};
    if (!k.kos(durgun({0, 0, 0.05}, q), 0, adim)) return 1;

    std::vector<double> mx, pb, vn, dx;
    for (const auto& o : k.tur(SensorTuru::manyetometre)) mx.push_back(static_cast<double>(yuk<MagOlcumu>(o).field[0]) - 2.56e-5);
    for (const auto& o : k.tur(SensorTuru::basinc)) pb.push_back(static_cast<double>(yuk<BaroOlcumu>(o).p_abs));
    const auto gn = k.tur(SensorTuru::gnss);
    for (const auto& o : gn) vn.push_back(static_cast<double>(yuk<GnssOlcumu>(o).vel_ned[0]));
    double cov_xx = 0.0;
    for (const auto& o : k.tur(SensorTuru::dvl)) {
        const auto& v = yuk<DvlOlcumu>(o);
        if ((v.dvl_flags & kDvlHizGecerli) == 0) continue;
        dx.push_back(static_cast<double>(v.vel[0]));
        cov_xx = static_cast<double>(v.cov[0]);
    }
    const Ozet om = ozet(mx), op = ozet(pb), og = ozet(vn), od = ozet(dx);
    const double p_bek = 101325.0 + 1000.0 * kG * 0.02;
    std::printf("  mag ss %.4g; basınç ort %.3f ss %.3f; GNSS hız ss %.4g; DVL vx ss %.4g, √cov %.4g\n",
                om.ss, op.ort, op.ss, og.ss, od.ss, std::sqrt(cov_xx));
    YAKIN(d, om.ort, 0.0, 4.0 * 2e-8 / std::sqrt(static_cast<double>(mx.size())), "mag ortalaması");
    YAKIN(d, om.ss, 2e-8, 0.05 * 2e-8, "mag σ");
    YAKIN(d, op.ort, p_bek, 4.0 * 20.0 / std::sqrt(static_cast<double>(pb.size())), "basınç ortalaması");
    YAKIN(d, op.ss, 20.0, 0.06 * 20.0, "basınç σ");
    YAKIN(d, og.ss, 0.05, 0.1 * 0.05, "GNSS hız σ");
    // DVL: eşit σ'lı simetrik dört ışında σ_x = 1,848 σ_b (05 §2.3.5); düz tabanda r = h / cos α.
    const double r = (3.0 - 0.19) / std::cos(0.39269908169872414);
    const double sb = std::sqrt(1e-6 + (2e-4 * r) * (2e-4 * r));
    YAKIN(d, std::sqrt(cov_xx), 1.848 * sb, 0.002 * 1.848 * sb, "DVL kovaryansı 1,848 σ_b");
    YAKIN(d, od.ss, 1.848 * sb, 0.1 * 1.848 * sb, "DVL hız σ");
    // eph: σ_eff = CEP/√(2 ln 2) · √(N_ref / n_sv); yüzeyde 20 uydu.
    const auto& g = yuk<GnssOlcumu>(gn.back());
    const double eph = 2.0 / std::sqrt(2.0 * std::log(2.0)) * std::sqrt(12.0 / 20.0);
    DOGRULA(d, g.n_sv == 20, "yüzeyde n_sv 20 değil");
    YAKIN(d, g.eph, eph, 1e-5, "eph");
    YAKIN(d, g.epv, 1.5 * eph, 1e-5, "epv");
    return d.hata;
}

}  // namespace ida::sensor::test
