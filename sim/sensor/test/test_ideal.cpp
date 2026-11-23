// Kabul 1: gürültü ve hata kapalıyken çıktı = analitik değer (durgun, sabit dönüş, sabit ivme,
// dalış; kimlik olmayan montaj).
//
// Toleranslar: float (binary32) yuvarlaması (|x|·6e-8) + kuantalama basamağının yarısı.
// İdeal ayarda IMU kuantalaması 24 bit seçilir (A5 `bit`; varsayılan 16 bitte dünya dönüşü
// 7·10⁻⁵ rad/s bir LSB'nin, 2,7·10⁻⁴ rad/s, altında kalır ve sınanamaz).
#include "test_destek.hpp"

namespace ida::sensor::test {

namespace {

constexpr double kOrtamP = 101325.0;
constexpr double kRho = 1000.0;
constexpr double kTaban = 3.0;
const Vek3 kBn{2.56e-5, 2.7e-6, 4.03e-5};
const Vek3 kLBaro{-0.19, 0.0, -0.03};
const Vek3 kLGnss{-0.05, 0.0, -0.22};
const Vek3 kLDvl{0.05, 0.0, 0.14};
constexpr double kOrijinEnlem = 0.7155849933176751;
constexpr double kOrijinBoylam = 0.5061454830783556;
constexpr double kIsinAcisi = 0.39269908169872414;

double ftol(double x) { return std::abs(x) * 1.2e-7 + 1e-12; }

SensorKumesiAyari ideal24()
{
    SensorKumesiAyari k = ideal_ayar();
    ayar_govdesi<ImuAyar>(k, SensorTuru::imu).bit = 24;
    return k;
}

double imu_lsb_g() { return 2.0 * 8.7266 / 16777216.0; }
double imu_lsb_a() { return 2.0 * 39.2266 / 16777216.0; }

void imu_denetle(Denetci& d, const Olcum& o, const Vek3& gyro, const Vek3& accel, const char* ad)
{
    const auto& y = yuk<ImuOlcumu>(o);
    for (std::size_t i = 0; i < 3; ++i) {
        YAKIN(d, y.gyro[i], gyro[i], imu_lsb_g() / 2 + ftol(gyro[i]), ad);
        YAKIN(d, y.accel[i], accel[i], imu_lsb_a() / 2 + ftol(accel[i]), ad);
    }
    DOGRULA(d, (o.hdr.flags & kBayrakGecerli) != 0 && (o.hdr.flags & kBayrakDoymus) == 0, "IMU bayrakları");
}

double baro_bekle(const fizik::DurumAnlik& s)
{
    const Vek3 m = ic::topla(s.konum_n, ic::carp(ic::matris(s.yonelim), kLBaro));
    return m[2] > 0.0 ? kOrtamP + kRho * kG * m[2] : kOrtamP;
}

// DVL düz taban geometrisi (bağımsız yazım): ışın menzilleri; irtifa dönüştürücüden yatay
// tabana dik mesafedir (A4 §3.3, W4-K8).
void dvl_geometri(const fizik::DurumAnlik& s, std::array<double, 4>& r, double& irtifa)
{
    const Mat3 rn = ic::matris(s.yonelim);
    const Vek3 p = ic::topla(s.konum_n, ic::carp(rn, kLDvl));
    irtifa = kTaban - p[2];
    for (std::size_t i = 0; i < 4; ++i) {
        const double psi = 0.7853981633974483 + 1.5707963267948966 * static_cast<double>(i);
        const Vek3 b{std::sin(kIsinAcisi) * std::cos(psi), std::sin(kIsinAcisi) * std::sin(psi),
                     std::cos(kIsinAcisi)};
        const Vek3 dn = ic::carp(rn, b);
        r[i] = (kTaban - p[2]) / dn[2];
    }
}

// Küçük ofsetler için yerel meridyen yarıçaplarıyla (bağımsız yöntem) WGS-84.
void lla_bekle(const Vek3& ned, double& lat_e7, double& lon_e7, double& alt_mm)
{
    const double a = 6378137.0, f = 1.0 / 298.257223563, e2 = f * (2 - f);
    const double s = std::sin(kOrijinEnlem);
    const double rn = a / std::sqrt(1 - e2 * s * s);
    const double rm = a * (1 - e2) / std::pow(1 - e2 * s * s, 1.5);
    const double lat = kOrijinEnlem + ned[0] / rm;
    const double lon = kOrijinBoylam + ned[1] / (rn * std::cos(kOrijinEnlem));
    lat_e7 = lat * 180.0 / 3.14159265358979323846 * 1e7;
    lon_e7 = lon * 180.0 / 3.14159265358979323846 * 1e7;
    alt_mm = -ned[2] * 1000.0;
}

void gnss_denetle(Denetci& d, const Olcum& o, const Vek3& ned, const Vek3& v_n)
{
    const auto& g = yuk<GnssOlcumu>(o);
    double lat = 0, lon = 0, alt = 0;
    lla_bekle(ned, lat, lon, alt);
    DOGRULA(d, g.fix_type == kFix3B && (o.hdr.flags & kBayrakGecerli) != 0, "GNSS fix 3D ve geçerli");
    YAKIN(d, g.lat_e7, lat, 1.0, "lat_e7");
    YAKIN(d, g.lon_e7, lon, 1.0, "lon_e7");
    YAKIN(d, g.alt_mm, alt, 1.0, "alt_mm");
    for (std::size_t i = 0; i < 3; ++i) YAKIN(d, g.vel_ned[i], v_n[i], ftol(v_n[i]) + 1e-9, "vel_ned");
}

}  // namespace

// Durgun, eğik araç (1 m derinlikte): IMU (−Rᵀg; dünya dönüşüyle Rᵀω_ie), manyetometre,
// basınç, DVL, batarya; yüzeyde GNSS.
int test_ideal_durgun()
{
    Denetci d;
    const Kuaterniyon q = euler_q(0.1, -0.05, 0.8);
    const Vek3 p{1.0, 2.0, 1.0};
    for (int dunya = 0; dunya < 2; ++dunya) {
        SensorKumesiAyari a = ideal24();
        ayar_govdesi<ImuAyar>(a, SensorTuru::imu).dunya_donusu_etkin = dunya == 1;
        Kosu k;
        if (!k.kur(a)) return 1;
        const Yorunge y = durgun(p, q);
        if (!k.kos(y, 0, 1600)) return 1;
        const Mat3 r = ic::matris(q);
        const Vek3 acc = ic::carp_t(r, {0.0, 0.0, -kG});
        Vek3 gyro{};
        if (dunya == 1) {
            const double om = 7.292115e-5;
            gyro = ic::carp_t(r, {om * std::cos(kOrijinEnlem), 0.0, -om * std::sin(kOrijinEnlem)});
        }
        const auto imu = k.tur(SensorTuru::imu);
        DOGRULA(d, imu.size() == 399, "IMU sayısı (2 s, 200 Hz, ilk pencere tam)");
        for (const auto& o : imu) {
            imu_denetle(d, o, gyro, acc, "IMU durgun");
            YAKIN(d, yuk<ImuOlcumu>(o).temp_c, 20.0, 1e-6, "IMU sıcaklık");
        }
        if (dunya == 1) continue;
        const Vek3 b_s = ic::carp_t(r, kBn);
        for (const auto& o : k.tur(SensorTuru::manyetometre)) {
            for (std::size_t i = 0; i < 3; ++i) YAKIN(d, yuk<MagOlcumu>(o).field[i], b_s[i], ftol(b_s[i]), "mag");
        }
        const double pb = baro_bekle(y(0));
        for (const auto& o : k.tur(SensorTuru::basinc)) {
            YAKIN(d, yuk<BaroOlcumu>(o).p_abs, pb, ftol(pb), "basınç");
            YAKIN(d, yuk<BaroOlcumu>(o).temp_c, 20.0, 1e-6, "basınç sıcaklığı");
        }
        std::array<double, 4> rr{};
        double irt = 0;
        dvl_geometri(y(0), rr, irt);
        const auto dvl = k.tur(SensorTuru::dvl);
        DOGRULA(d, !dvl.empty(), "DVL kaydı yok");
        for (const auto& o : dvl) {
            const auto& v = yuk<DvlOlcumu>(o);
            DOGRULA(d, (v.dvl_flags & kDvlHizGecerli) != 0 && v.beam_valid == 0x0F, "DVL kilitli değil");
            for (std::size_t i = 0; i < 3; ++i) YAKIN(d, v.vel[i], 0.0, 1e-9, "DVL hız");
            for (std::size_t i = 0; i < 4; ++i) YAKIN(d, v.beam_dist[i], rr[i], ftol(rr[i]), "DVL ışın menzili");
            YAKIN(d, v.altitude, irt, ftol(irt) + 1e-9, "DVL irtifa");
        }
        const double v_lsb = 3.3 / 4096.0 * 11.0, i_lsb = 3.3 / 4096.0 * 37.8788;
        for (const auto& o : k.tur(SensorTuru::batarya_olcer)) {
            YAKIN(d, yuk<BataryaOlcumu>(o).voltage, 15.6, v_lsb / 2 + 1e-5, "batarya gerilimi");
            YAKIN(d, yuk<BataryaOlcumu>(o).current, 20.0 / 15.6, i_lsb / 2 + 1e-5, "batarya akımı");
        }
        const auto gn = k.tur(SensorTuru::gnss);
        DOGRULA(d, !gn.empty(), "GNSS kaydı yok");
        for (const auto& o : gn) {
            DOGRULA(d, (o.hdr.flags & kBayrakGecerli) == 0 && yuk<GnssOlcumu>(o).fix_type == kFixYok,
                    "1 m derinlikte GNSS geçerli");
        }
    }
    // Yüzeyde GNSS: anten su üstünde.
    {
        Kosu k;
        if (!k.kur(ideal24())) return 1;
        const Kuaterniyon qs = euler_q(0.0, 0.0, 0.8);
        const Vek3 ps{3.0, -4.0, 0.05};
        if (!k.kos(durgun(ps, qs), 0, 1600)) return 1;
        const Vek3 anten = ic::topla(ps, ic::carp(ic::matris(qs), kLGnss));
        const auto gn = k.tur(SensorTuru::gnss);
        DOGRULA(d, gn.size() == 10, "GNSS sayısı (2 s, 5 Hz)");
        for (const auto& o : gn) gnss_denetle(d, o, anten, {});
    }
    return d.hata;
}

// Sabit yön dönüşü (r = 0,5 rad/s, düz araç): jiroskop, manyetometre, DVL (ω × l), GNSS hızı.
int test_ideal_donus()
{
    Denetci d;
    const double r = 0.5;
    const double z = 0.05;
    const Yorunge y = [&](std::uint64_t n) {
        return durum(n, {0.0, 0.0, z}, {}, {}, euler_q(0.0, 0.0, 0.2 + r * sn(n)), {0.0, 0.0, r});
    };
    Kosu k;
    if (!k.kur(ideal24())) return 1;
    if (!k.kos(y, 0, 2400)) return 1;
    for (const auto& o : k.tur(SensorTuru::imu)) {
        imu_denetle(d, o, {0.0, 0.0, r}, {0.0, 0.0, -kG}, "IMU dönüş");
    }
    for (const auto& o : k.tur(SensorTuru::manyetometre)) {
        const std::uint64_t n = static_cast<std::uint64_t>(o.hdr.t_meas_ns / kAdim);
        const Vek3 b = ic::carp_t(ic::matris(y(n).yonelim), kBn);
        for (std::size_t i = 0; i < 3; ++i) YAKIN(d, yuk<MagOlcumu>(o).field[i], b[i], ftol(b[i]), "mag dönüş");
    }
    const Vek3 v_dvl = ic::capraz({0.0, 0.0, r}, kLDvl);
    for (const auto& o : k.tur(SensorTuru::dvl)) {
        const auto& v = yuk<DvlOlcumu>(o);
        if ((v.dvl_flags & kDvlHizGecerli) == 0) continue;
        for (std::size_t i = 0; i < 3; ++i) YAKIN(d, v.vel[i], v_dvl[i], 0.5e-4 + 1e-7, "DVL dönüş");
    }
    for (const auto& o : k.tur(SensorTuru::gnss)) {
        const std::uint64_t n = static_cast<std::uint64_t>(o.hdr.t_meas_ns / kAdim);
        const Mat3 rn = ic::matris(y(n).yonelim);
        const Vek3 anten = ic::topla(y(n).konum_n, ic::carp(rn, kLGnss));
        const Vek3 v = ic::carp(rn, ic::capraz({0.0, 0.0, r}, kLGnss));
        gnss_denetle(d, o, anten, v);
    }
    return d.hata;
}

// Sabit ivme (a_n = (0,5; 0,2; 0)): IMU özgül kuvveti, DVL hızının t_meas ara değeri, GNSS konum
// ve hızı, basınç sabit.
int test_ideal_ivme()
{
    Denetci d;
    const Vek3 a_n{0.5, 0.2, 0.0};
    const Kuaterniyon q = euler_q(0.0, 0.0, 0.4);
    const Vek3 p0{0.0, 0.0, 0.05};
    auto konum = [&](double t) { return ic::topla(p0, ic::olcekle(a_n, 0.5 * t * t)); };
    const Yorunge y = [&](std::uint64_t n) {
        const double t = sn(n);
        return durum(n, konum(t), ic::olcekle(a_n, t), a_n, q, {});
    };
    Kosu k;
    if (!k.kur(ideal24())) return 1;
    if (!k.kos(y, 0, 2400)) return 1;
    const Mat3 r = ic::matris(q);
    const Vek3 f = ic::carp_t(r, ic::cikar(a_n, {0.0, 0.0, kG}));
    for (const auto& o : k.tur(SensorTuru::imu)) imu_denetle(d, o, {}, f, "IMU ivme");
    int n_dvl = 0;
    for (const auto& o : k.tur(SensorTuru::dvl)) {
        const auto& v = yuk<DvlOlcumu>(o);
        if ((v.dvl_flags & kDvlHizGecerli) == 0) continue;
        ++n_dvl;
        const Vek3 vb = ic::carp_t(r, ic::olcekle(a_n, static_cast<double>(o.hdr.t_meas_ns) * 1e-9));
        for (std::size_t i = 0; i < 3; ++i) YAKIN(d, v.vel[i], vb[i], 0.5e-4 + 1e-6, "DVL hızı t_meas'ta");
    }
    DOGRULA(d, n_dvl > 10, "DVL geçerli kayıt az");
    for (const auto& o : k.tur(SensorTuru::gnss)) {
        const double t = static_cast<double>(o.hdr.t_meas_ns) * 1e-9;
        gnss_denetle(d, o, ic::topla(konum(t), ic::carp(r, kLGnss)), ic::olcekle(a_n, t));
    }
    const double pb = baro_bekle(y(0));
    for (const auto& o : k.tur(SensorTuru::basinc)) YAKIN(d, yuk<BaroOlcumu>(o).p_abs, pb, ftol(pb), "basınç");
    return d.hata;
}

// Dalış (0,2 m/s): basınç pencere ortasındaki derinliğe eşit (doğrusal profilde yamuk ortalaması
// tamdır), IMU (0, 0, −g), DVL (0, 0, 0,2), GNSS geçersiz.
int test_ideal_dalis()
{
    Denetci d;
    const double w = 0.2;
    const Yorunge y = [&](std::uint64_t n) {
        return durum(n, {0.0, 0.0, 0.5 + w * sn(n)}, {0.0, 0.0, w}, {}, Kuaterniyon{1, 0, 0, 0}, {});
    };
    Kosu k;
    if (!k.kur(ideal24())) return 1;
    if (!k.kos(y, 0, 4000)) return 1;
    for (const auto& o : k.tur(SensorTuru::imu)) imu_denetle(d, o, {}, {0.0, 0.0, -kG}, "IMU dalış");
    const auto baro = k.tur(SensorTuru::basinc);
    DOGRULA(d, baro.size() > 100, "basınç kaydı az");
    for (const auto& o : baro) {
        const double tm = static_cast<double>(o.hdr.t_meas_ns - static_cast<std::int64_t>(o.hdr.window_ns) / 2) * 1e-9;
        const double pb = kOrtamP + kRho * kG * (0.5 + w * tm + kLBaro[2]);
        YAKIN(d, yuk<BaroOlcumu>(o).p_abs, pb, ftol(pb) + 1e-6, "basınç dalış");
    }
    for (const auto& o : k.tur(SensorTuru::dvl)) {
        const auto& v = yuk<DvlOlcumu>(o);
        if ((v.dvl_flags & kDvlHizGecerli) == 0) continue;
        YAKIN(d, v.vel[2], w, 0.5e-4 + 1e-7, "DVL düşey hız");
        const std::uint64_t n = static_cast<std::uint64_t>(o.hdr.t_tx_ns / kAdim);
        std::array<double, 4> rr{};
        double irt = 0;
        dvl_geometri(y(n), rr, irt);
        YAKIN(d, v.altitude, irt, ftol(irt) + 1e-9, "DVL irtifa (gönderim anında)");
    }
    return d.hata;
}

// Kimlik olmayan montaj (05 §7 IMU satırı): R_bs 30° döndürülmüş, l ≠ 0.
int test_ideal_montaj()
{
    Denetci d;
    const double aci = 30.0 * 3.14159265358979323846 / 180.0;
    const Vek3 eksen{1.0 / std::sqrt(3.0), -1.0 / std::sqrt(3.0), 1.0 / std::sqrt(3.0)};
    const Kuaterniyon q_bs{std::cos(aci / 2), eksen[0] * std::sin(aci / 2), eksen[1] * std::sin(aci / 2),
                           eksen[2] * std::sin(aci / 2)};
    const Mat3 rbs = ic::matris(q_bs);
    const Vek3 l{0.1, -0.05, 0.02};
    SensorKumesiAyari a = ideal24();
    sensor_ayari(a, SensorTuru::imu).montaj = Montaj{l, q_bs};
    sensor_ayari(a, SensorTuru::manyetometre).montaj = Montaj{l, q_bs};
    ayar_govdesi<ImuAyar>(a, SensorTuru::imu).dunya_donusu_etkin = true;
    const double om = 7.292115e-5;
    const Vek3 w_ie{om * std::cos(kOrijinEnlem), 0.0, -om * std::sin(kOrijinEnlem)};
    {
        // Durgun: ivme R_bsᵀ Rᵀ(−g_n), jiroskop R_bsᵀ Rᵀ ω_ie, alan R_bsᵀ Rᵀ B_n.
        const Kuaterniyon q = euler_q(0.2, 0.1, -1.0);
        const Mat3 r = ic::matris(q);
        Kosu k;
        if (!k.kur(a)) return 1;
        if (!k.kos(durgun({0, 0, 1.0}, q), 0, 800)) return 1;
        const Vek3 acc = ic::carp_t(rbs, ic::carp_t(r, {0.0, 0.0, -kG}));
        const Vek3 gyro = ic::carp_t(rbs, ic::carp_t(r, w_ie));
        for (const auto& o : k.tur(SensorTuru::imu)) imu_denetle(d, o, gyro, acc, "montaj durgun");
        const Vek3 b = ic::carp_t(rbs, ic::carp_t(r, kBn));
        for (const auto& o : k.tur(SensorTuru::manyetometre)) {
            for (std::size_t i = 0; i < 3; ++i) YAKIN(d, yuk<MagOlcumu>(o).field[i], b[i], ftol(b[i]), "montaj mag");
        }
    }
    {
        // Sabit dönüş (düz araç, gövde z ekseni): kaldıraç kolu R_bsᵀ ω × (ω × l).
        ayar_govdesi<ImuAyar>(a, SensorTuru::imu).dunya_donusu_etkin = false;
        const double r = 0.8;
        const Vek3 w{0.0, 0.0, r};
        Kosu k;
        if (!k.kur(a)) return 1;
        const Yorunge y = [&](std::uint64_t n) {
            return durum(n, {0, 0, 1.0}, {}, {}, euler_q(0, 0, r * sn(n)), w);
        };
        if (!k.kos(y, 0, 800)) return 1;
        const Vek3 f = ic::topla({0.0, 0.0, -kG}, ic::capraz(w, ic::capraz(w, l)));
        for (const auto& o : k.tur(SensorTuru::imu)) {
            imu_denetle(d, o, ic::carp_t(rbs, w), ic::carp_t(rbs, f), "montaj dönüş");
        }
    }
    return d.hata;
}

}  // namespace ida::sensor::test
