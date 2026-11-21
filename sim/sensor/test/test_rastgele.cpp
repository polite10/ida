// Kabul 5: rastgelelik (A5 Ek D) ve belirlenimcilik.
#include "test_destek.hpp"

#include "ida/sensor/rastgele.hpp"

#include <cstring>
#include <type_traits>

namespace ida::sensor::test {

namespace {

// Kaydın bütün alanlarının bayt dizilimi (bit düzeyinde karşılaştırma için).
std::vector<std::uint8_t> bayt(const Olcum& o)
{
    std::vector<std::uint8_t> v;
    auto ekle = [&v](const void* p, std::size_t n) {
        const auto* b = static_cast<const std::uint8_t*>(p);
        v.insert(v.end(), b, b + n);
    };
    const OlcumBasligi& h = o.hdr;
    ekle(&h.sensor_id, sizeof h.sensor_id);
    ekle(&h.seq, sizeof h.seq);
    ekle(&h.t_meas_ns, sizeof h.t_meas_ns);
    ekle(&h.window_ns, sizeof h.window_ns);
    ekle(&h.t_tx_ns, sizeof h.t_tx_ns);
    ekle(&h.t_hazir_ns, sizeof h.t_hazir_ns);
    ekle(&h.t_teslim_ns, sizeof h.t_teslim_ns);
    ekle(&h.flags, sizeof h.flags);
    std::visit(
        [&](const auto& y) {
            using T = std::decay_t<decltype(y)>;
            if constexpr (std::is_same_v<T, ImuOlcumu>) {
                ekle(y.gyro.data(), 12);
                ekle(y.accel.data(), 12);
                ekle(&y.temp_c, 4);
            } else if constexpr (std::is_same_v<T, MagOlcumu>) {
                ekle(y.field.data(), 12);
            } else if constexpr (std::is_same_v<T, BaroOlcumu>) {
                ekle(&y.p_abs, 4);
                ekle(&y.temp_c, 4);
            } else if constexpr (std::is_same_v<T, GnssOlcumu>) {
                ekle(&y.lat_e7, 4);
                ekle(&y.lon_e7, 4);
                ekle(&y.alt_mm, 4);
                ekle(y.vel_ned.data(), 12);
                ekle(&y.eph, 4);
                ekle(&y.epv, 4);
                ekle(&y.fix_type, 1);
                ekle(&y.n_sv, 1);
            } else if constexpr (std::is_same_v<T, DvlOlcumu>) {
                ekle(y.vel.data(), 12);
                ekle(y.cov.data(), 24);
                ekle(&y.fom, 4);
                ekle(y.beam_dist.data(), 16);
                ekle(&y.altitude, 4);
                ekle(&y.dvl_flags, 1);
                ekle(&y.beam_valid, 1);
            } else if constexpr (std::is_same_v<T, BataryaOlcumu>) {
                ekle(&y.voltage, 4);
                ekle(&y.current, 4);
                ekle(&y.temp_c, 4);
            } else if constexpr (std::is_same_v<T, SizintiOlcumu>) {
                ekle(&y.mask, 1);
            } else {
                ekle(&y.estop, 1);
                ekle(y.tube.data(), 2);
                ekle(&y.safety_rev, 4);
            }
        },
        o.yuk);
    return v;
}

// Yavaş dalış ve dönüş: yüzeyde 2 s, sonra 0,1 m/s dalış, 0,1 rad/s dönüş.
fizik::DurumAnlik hareketli(std::uint64_t n)
{
    const double t = sn(n);
    const double td = std::max(0.0, t - 2.0);
    const double z = 0.1 + 0.1 * td;
    const double w = t > 2.0 ? 0.1 : 0.0;
    fizik::DurumAnlik d =
        durum(n, {0.0, 0.0, z}, {0.0, 0.0, t > 2.0 ? 0.1 : 0.0}, {}, euler_q(0.0, 0.0, 0.3 + w * td),
              {0.0, 0.0, w});
    d.itki_istegi = {0.0, 0.0, 0.0, 0.0, 5.0, 5.0, 5.0, 5.0};
    return d;
}

std::vector<Olcum> kos_kayit(const SensorKumesiAyari& a, double sure)
{
    Kosu k;
    if (!k.kur(a)) return {};
    k.kos(hareketli, 0, static_cast<std::uint64_t>(sure / 1.25e-3));
    return k.kayit;
}

bool esit(const std::vector<Olcum>& a, const std::vector<Olcum>& b)
{
    if (a.size() != b.size()) return false;
    for (std::size_t i = 0; i < a.size(); ++i) {
        if (bayt(a[i]) != bayt(b[i])) return false;
    }
    return true;
}

std::vector<Olcum> suz(const std::vector<Olcum>& v, SensorTuru t)
{
    std::vector<Olcum> r;
    for (const auto& o : v) {
        if (o.hdr.tur == t) r.push_back(o);
    }
    return r;
}

}  // namespace

// A5 Ek D.2 (Random123), D.3 (anahtar ve sayaç), D.6 (GE kalış süresi formülü).
int test_philox_bilinen_yanit()
{
    Denetci d;
    using A4 = std::array<std::uint32_t, 4>;
    DOGRULA(d, philox4x32_10({0, 0, 0, 0}, {0, 0}) == A4{0x6627e8d5u, 0xe169c58du, 0xbc57ac4cu, 0x9b00dbd8u},
            "D.2 vektör 1");
    DOGRULA(d, philox4x32_10({0xffffffffu, 0xffffffffu, 0xffffffffu, 0xffffffffu}, {0xffffffffu, 0xffffffffu}) ==
                   A4{0x408f276du, 0x41c83b0eu, 0xa20bc7c6u, 0x6d5451fdu},
            "D.2 vektör 2");
    DOGRULA(d, philox4x32_10({0x243f6a88u, 0x85a308d3u, 0x13198a2eu, 0x03707344u}, {0xa4093822u, 0x299f31d0u}) ==
                   A4{0xd16cfe09u, 0x94fdccebu, 0x5001e420u, 0x24126ea1u},
            "D.2 vektör 3");
    const std::string abc = "abc";
    const auto ozet = sha256(reinterpret_cast<const std::uint8_t*>(abc.data()), abc.size());
    DOGRULA(d, ozet[0] == 0xba && ozet[1] == 0x78 && ozet[31] == 0xad, "SHA-256('abc')");

    const Akis dalga(1, "fizik/dalga");
    DOGRULA(d, dalga.anahtar()[0] == 0x64a90d57u && dalga.anahtar()[1] == 0x0a056cf8u, "D.3 anahtar dalga");
    DOGRULA(d, dalga.sozcukler(0, 0) == A4{0xa62dda3bu, 0xec8d7d6bu, 0x9fb8419au, 0x83ee6b35u}, "D.3 n=0");
    DOGRULA(d, dalga.sozcukler(1, 0) == A4{0x32db07b0u, 0x1952207cu, 0x160a5668u, 0x9d2d9ea3u}, "D.3 n=1");
    DOGRULA(d, dalga.sozcukler((std::uint64_t{1} << 32) + 5u, 0x03010007u) ==
                   A4{0x14bdf9e0u, 0x202cc9a5u, 0xdd5ee3bfu, 0xc181b39fu},
            "D.3 n=2^32+5");
    DOGRULA(d, c2_kodla(3, 1, 7) == 0x03010007u, "c2 düzeni");
    DOGRULA(d, dalga.duzgun(0, 0) == 0.6491371531737968, "D.3 u0 bit eşit");
    YAKIN(d, dalga.normal(0, 0), 0.8257344949355085, 1e-15, "D.3 z0 (bilgi)");
    const Akis imu(1, "sensor/imu0/olcum");
    DOGRULA(d, imu.anahtar()[0] == 0xc822880fu && imu.anahtar()[1] == 0x07865bffu, "D.3 anahtar imu0");
    DOGRULA(d, imu.sozcukler(0, 0) == A4{0x9f4e8d20u, 0x8607e250u, 0xc2b93430u, 0xadc365d1u}, "D.3 imu0 n=0");
    YAKIN(d, imu.normal(0, 0), -0.9633570170026116, 1e-15, "D.3 imu0 z0");
    const Akis akinti(0, "fizik/akinti");
    DOGRULA(d, akinti.sozcukler(0, 0) == A4{0xac35a29eu, 0x36a3f2d5u, 0x349cfeefu, 0xefb49abeu}, "D.3 akinti");

    // D.6: GE kalış süresi d_i = max(1, ⌈−log(u(w0)) · m_i⌉).
    const Akis ge(1, "baglanti/ornek/asagi/gecis");
    DOGRULA(d, ge.anahtar()[0] == 0xe3e9c071u && ge.anahtar()[1] == 0x5496e939u, "D.6 anahtar");
    const double m0 = 1e9 / 0.5, m1 = 1e9 / 5.0;
    const auto d0 = static_cast<std::int64_t>(std::ceil(-std::log(duzgun_donusum(ge.sozcukler(0, 0)[0])) * m0));
    const auto d1 = static_cast<std::int64_t>(std::ceil(-std::log(duzgun_donusum(ge.sozcukler(1, 0)[0])) * m1));
    DOGRULA(d, d0 == 1669964347, "D.6 d_0");
    DOGRULA(d, d1 == 34325160, "D.6 d_1");
    return d.hata;
}

// Aynı tohumla bit düzeyinde aynı; farklı tohumla farklı.
int test_belirlenimcilik()
{
    Denetci d;
    SensorKumesiAyari a = ida0_sensor_kumesi();
    const auto k1 = kos_kayit(a, 6.0);
    const auto k2 = kos_kayit(a, 6.0);
    DOGRULA(d, !k1.empty(), "kayıt yok");
    DOGRULA(d, esit(k1, k2), "aynı tohumla iki koşu bit düzeyinde aynı değil");
    a.ana_tohum = 2;
    const auto k3 = kos_kayit(a, 6.0);
    const auto i1 = suz(k1, SensorTuru::imu);
    const auto i3 = suz(k3, SensorTuru::imu);
    DOGRULA(d, i1.size() == i3.size() && !esit(i1, i3), "farklı tohum aynı IMU dizisini verdi");
    const auto m1 = suz(k1, SensorTuru::manyetometre);
    const auto m3 = suz(k3, SensorTuru::manyetometre);
    DOGRULA(d, !esit(m1, m3), "farklı tohum aynı manyetometre dizisini verdi");
    return d.hata;
}

// Bir sensörü kapatmak ötekilerin dizisini değiştirmez (A5-K84; akış sensör adıyla anahtarlı).
int test_sensor_bagimsizligi()
{
    Denetci d;
    SensorKumesiAyari a = ida0_sensor_kumesi();
    const auto tam = kos_kayit(a, 6.0);
    SensorKumesiAyari b = a;
    for (auto& s : b.sensorler) {
        if (s.tur == SensorTuru::gnss || s.tur == SensorTuru::dvl || s.tur == SensorTuru::manyetometre) {
            s.etkin = false;
        }
    }
    const auto eksik = kos_kayit(b, 6.0);
    for (SensorTuru t : {SensorTuru::imu, SensorTuru::basinc, SensorTuru::batarya_olcer,
                         SensorTuru::sizinti, SensorTuru::firlatici_geri_okuma}) {
        const auto x = suz(tam, t);
        const auto y = suz(eksik, t);
        DOGRULA(d, !x.empty() && esit(x, y),
                "sensör kapatınca başka sensörün dizisi değişti, tür " + std::to_string(static_cast<int>(t)));
    }
    DOGRULA(d, suz(eksik, SensorTuru::gnss).empty() && suz(eksik, SensorTuru::dvl).empty(),
            "kapalı sensör kayıt üretti (A5-K49)");
    // Sensör sırası da diziyi değiştirmez.
    SensorKumesiAyari c = a;
    std::reverse(c.sensorler.begin(), c.sensorler.end());
    const auto ters = kos_kayit(c, 6.0);
    DOGRULA(d, esit(suz(tam, SensorTuru::imu), suz(ters, SensorTuru::imu)) &&
                   esit(suz(tam, SensorTuru::dvl), suz(ters, SensorTuru::dvl)),
            "sensör sırası diziyi değiştirdi");
    return d.hata;
}

}  // namespace ida::sensor::test
