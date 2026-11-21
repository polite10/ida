// Kabul 7 (bütünleme): sensör kümesinin ürettiği IMU, manyetometre ve basınç ölçümleri A4
// örneklerine çevrilip gerçek çekirdeğin kestirimine verilir (durgun ve yavaş dalış); kestirim
// yönelim ve derinliği sınır içinde bulmalıdır.
//
// Çevirme bu dosyadadır (köprü değildir): her kontrol sınırında t_teslim ≤ t_n olan kayıtlar
// partiye girer; alım zamanı t_rx = çağrının duvar saati (SIL: duvar = sim + 1 s).
// Parametre bloğu yalnız çekirdeğin genel arayüzüyle kurulur (A4 §3.10): varsayılanlar, yalnız
// YER_G = 9,82 (senaryo yerçekimi) ve BARO_KAL_D = başlangıç derinliği değiştirilir.
//
// Sınırlar ilk koşudan önce yazılmıştır: yalpa ve yunuslama 0,03 rad, yön 0,06 rad, derinlik
// 0,06 m (değerlendirme 5 s'den sonra). Gerekçe: ivmeölçer açılış sapması (A5 profili 3σ
// 0,196 m/s²) hizalamada ≈ 0,007 rad (1σ) eğim hatası verir; sınır bunun ≈ 4 katıdır.
#include "ida/port.h"
#include "ida/sensor/sensor_kumesi.hpp"
#include "matematik.hpp"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <cstddef>
#include <vector>

namespace {

using namespace ida::sensor;

constexpr std::int64_t kAdimNs = 1'250'000;
constexpr std::int64_t kDuvarFarki = 1'000'000'000;
constexpr double kG = 9.82;
constexpr double kZ0 = 0.15;
constexpr double kZ1 = 1.5;
constexpr double kRoll = 0.02, kPitch = -0.01, kYaw = 0.7;

alignas(std::max_align_t) unsigned char g_cekirdek[65536];
ida_in_t g_in;
ida_out_t g_out;

double yumusak(double x, int turev)
{
    if (x <= 0.0 || x >= 1.0) return (turev == 0 && x >= 1.0) ? 1.0 : 0.0;
    if (turev == 0) return x * x * x * (10.0 + x * (-15.0 + 6.0 * x));
    if (turev == 1) return 30.0 * x * x * (1.0 - x) * (1.0 - x);
    return 60.0 * x * (1.0 - x) * (1.0 - 2.0 * x);
}

struct Senaryo {
    bool dalis = false;
    double derinlik(double t, int turev) const
    {
        if (!dalis) return turev == 0 ? kZ0 : 0.0;
        const double sure = 14.0;
        const double k = std::pow(1.0 / sure, turev);
        const double y = yumusak((t - 10.0) / sure, turev);
        return (turev == 0 ? kZ0 : 0.0) + (kZ1 - kZ0) * y * k;
    }
};

Kuaterniyon euler_q(double roll, double pitch, double yaw)
{
    const double cr = std::cos(roll / 2), sr = std::sin(roll / 2);
    const double cp = std::cos(pitch / 2), sp = std::sin(pitch / 2);
    const double cy = std::cos(yaw / 2), sy = std::sin(yaw / 2);
    return {cr * cp * cy + sr * sp * sy, sr * cp * cy - cr * sp * sy, cr * sp * cy + sr * cp * sy,
            cr * cp * sy - sr * sp * cy};
}

ida::fizik::DurumAnlik durum(const Senaryo& s, std::uint64_t n)
{
    const double t = static_cast<double>(n) * 1.25e-3;
    ida::fizik::DurumAnlik d;
    d.adim_no = n;
    d.t_ns = static_cast<std::int64_t>(n) * kAdimNs;
    d.konum_n = {0.0, 0.0, s.derinlik(t, 0)};
    d.yonelim = euler_q(kRoll, kPitch, kYaw);
    const Mat3 r = ida::sensor::ic::matris(d.yonelim);
    d.dogrusal_hiz_b = ida::sensor::ic::carp_t(r, {0.0, 0.0, s.derinlik(t, 1)});
    const Vek3 a_n{0.0, 0.0, s.derinlik(t, 2)};
    d.dogrusal_ivme_b = ida::sensor::ic::carp_t(r, a_n);
    d.ozgul_kuvvet_b = ida::sensor::ic::carp_t(r, {0.0, 0.0, a_n[2] - kG});
    return d;
}

std::uint32_t crc32(const std::uint8_t* p, std::size_t n)
{
    std::uint32_t c = 0xFFFFFFFFu;
    for (std::size_t i = 0; i < n; ++i) {
        c ^= p[i];
        for (int k = 0; k < 8; ++k) c = (c & 1u) ? (c >> 1) ^ 0xEDB88320u : c >> 1;
    }
    return c ^ 0xFFFFFFFFu;
}

void yaz32(std::vector<std::uint8_t>& b, std::size_t yer, std::uint32_t v)
{
    for (int i = 0; i < 4; ++i) b[yer + static_cast<std::size_t>(i)] = static_cast<std::uint8_t>((v >> (8 * i)) & 0xFFu);
}

// A4 §3.10 parametre bloğu: varsayılanlar + değişiklikler (genel arayüzle).
std::vector<std::uint8_t> parametre_blogu(float baro_kal_d)
{
    ida_core_info_t info;
    ida_core_info(&info);
    const std::uint16_t n = ida_core_param_count();
    std::vector<ida_param_value_t> d;
    for (std::uint16_t i = 0; i < n; ++i) {
        ida_param_desc_t pd;
        if (ida_core_param_desc(i, &pd) != IDA_RC_OK) continue;
        ida_param_value_t v = pd.def;
        v.id = pd.id;
        v.type = pd.type;
        if (std::strcmp(pd.name, "YER_G") == 0) v.v.f32 = static_cast<float>(kG);
        if (std::strcmp(pd.name, "BARO_KAL_D") == 0) v.v.f32 = baro_kal_d;
        d.push_back(v);
    }
    for (std::size_t i = 1; i < d.size(); ++i) {   // param_id'ye göre artan
        for (std::size_t j = i; j > 0 && d[j - 1].id > d[j].id; --j) std::swap(d[j - 1], d[j]);
    }
    const std::size_t yuk = 8 * d.size();
    std::vector<std::uint8_t> b(24 + yuk, 0);
    yaz32(b, 0, IDA_PBLOB_MAGIC);
    b[4] = 1;
    for (int i = 0; i < 8; ++i) b[8 + static_cast<std::size_t>(i)] = static_cast<std::uint8_t>((info.param_schema_hash >> (8 * i)) & 0xFFu);
    yaz32(b, 16, static_cast<std::uint32_t>(yuk));
    for (std::size_t i = 0; i < d.size(); ++i) {
        const std::size_t y = 24 + 8 * i;
        b[y] = static_cast<std::uint8_t>(d[i].id & 0xFFu);
        b[y + 1] = static_cast<std::uint8_t>(d[i].id >> 8);
        b[y + 2] = d[i].type;
        yaz32(b, y + 4, d[i].v.u32);
    }
    yaz32(b, 20, crc32(b.data() + 24, yuk));
    return b;
}

void basligi_doldur(ida_hdr_t& h, const OlcumBasligi& o, std::uint8_t tur, std::int64_t t_rx)
{
    h.t_meas.ns = o.t_meas_ns;
    h.t_rx.ns = t_rx;
    h.seq = o.seq;
    h.window_ns = o.window_ns;
    h.flags = o.flags;
    h.type = tur;
    h.inst = o.inst;
}

double sar(double a)
{
    const double pi = 3.14159265358979323846;
    while (a > pi) a -= 2 * pi;
    while (a <= -pi) a += 2 * pi;
    return a;
}

int kos(bool dalis)
{
    ida_core_info_t info;
    ida_core_info(&info);
    if (info.core_size > sizeof(g_cekirdek) || info.core_align > alignof(std::max_align_t)) {
        std::printf("  çekirdek tamponu yetmiyor: %u B, hizalama %u\n", info.core_size, info.core_align);
        return 1;
    }
    auto* c = reinterpret_cast<ida_core_t*>(g_cekirdek);
    const std::vector<std::uint8_t> blok = parametre_blogu(static_cast<float>(kZ0));
    ida_init_in_t ii;
    std::memset(&ii, 0, sizeof ii);
    ii.port_major = static_cast<std::uint16_t>(IDA_PORT_VERSION_MAJOR);
    ii.port_minor = static_cast<std::uint16_t>(IDA_PORT_VERSION_MINOR);
    ii.now_w.ns = kDuvarFarki;
    ii.boot_count = 1;
    ii.boot_count_ok = true;
    ii.reset_cause = static_cast<std::uint8_t>(IDA_RESET_SIL_START);
    ii.param_src = static_cast<std::uint8_t>(IDA_PSRC_SIL);
    ii.param_blob = blok.data();
    ii.param_len = static_cast<std::uint32_t>(blok.size());
    const ida_rc_t rc = ida_core_init(c, &ii);
    if (rc != IDA_RC_OK) {
        std::printf("  ida_core_init: %d\n", static_cast<int>(rc));
        return 1;
    }

    SensorKumesiAyari a = ida0_sensor_kumesi();
    for (auto& s : a.sensorler) {
        s.etkin = s.tur == SensorTuru::imu || s.tur == SensorTuru::manyetometre || s.tur == SensorTuru::basinc;
    }
    SensorKumesi kume;
    const KurulumSonucu ks = kume.kur(a);
    if (!ks.tamam) {
        std::printf("  kurulum: %s\n", ks.hata.c_str());
        return 1;
    }
    Senaryo sen;
    sen.dalis = dalis;
    const std::uint64_t son = 32'000;   // 40 s
    double e_tilt = 0, e_yaw = 0, e_d = 0, e_d_dalis = 0;
    int hata = 0;
    std::uint32_t parti = 0;
    std::vector<Olcum> kayit;
    for (std::uint64_t n = 0; n < son; ++n) {
        kume.adim(durum(sen, n), n);
        if (n == 0 || n % 4 != 0) continue;
        const std::int64_t tn = static_cast<std::int64_t>(n) * kAdimNs;
        const std::int64_t duvar = tn + kDuvarFarki;
        kayit.clear();
        kume.bosalt(tn, kayit);
        std::memset(&g_in, 0, sizeof g_in);
        g_in.now_w.ns = duvar;
        g_in.tf.present = true;
        g_in.tf.seq = ++parti;
        g_in.tf.t.ns = tn;
        g_in.tf.t_rx.ns = duvar;
        g_in.plat.link_state = static_cast<std::uint8_t>(IDA_LINK_RUNNING);
        for (const Olcum& o : kayit) {
            if (const auto* imu = std::get_if<ImuOlcumu>(&o.yuk)) {
                g_in.imu_present = true;
                basligi_doldur(g_in.imu.hdr, o.hdr, static_cast<std::uint8_t>(IDA_SENS_IMU), duvar);
                for (std::size_t i = 0; i < 3; ++i) {
                    g_in.imu.imu.gyro[i] = imu->gyro[i];
                    g_in.imu.imu.accel[i] = imu->accel[i];
                }
                g_in.imu.imu.temp_c = imu->temp_c;
            } else if (g_in.n_samples < IDA_PORT_MAX_SAMPLES) {
                ida_sample_t& s = g_in.samples[g_in.n_samples];
                if (const auto* m = std::get_if<MagOlcumu>(&o.yuk)) {
                    basligi_doldur(s.hdr, o.hdr, static_cast<std::uint8_t>(IDA_SENS_MAG), duvar);
                    for (std::size_t i = 0; i < 3; ++i) s.u.mag.field[i] = m->field[i];
                } else if (const auto* b = std::get_if<BaroOlcumu>(&o.yuk)) {
                    basligi_doldur(s.hdr, o.hdr, static_cast<std::uint8_t>(IDA_SENS_BARO), duvar);
                    s.u.baro.p_abs = b->p_abs;
                    s.u.baro.temp_c = b->temp_c;
                } else {
                    continue;
                }
                ++g_in.n_samples;
            }
        }
        const ida_rc_t r = g_in.imu_present ? ida_core_step(c, &g_in, &g_out) : ida_core_tick(c, &g_in, &g_out);
        if (r != IDA_RC_OK) {
            std::printf("  çağrı %u: dönüş %d\n", parti, static_cast<int>(r));
            return 1;
        }
        const double t = static_cast<double>(tn) * 1e-9;
        if (t < 5.0) continue;
        if (!g_out.nav_valid || g_out.nav.est_level == IDA_EST_INIT || g_out.nav.est_level == IDA_EST_FAULT) {
            if (hata++ < 5) std::printf("  t = %.3f: kestirim geçersiz (düzey %u)\n", t, g_out.nav.est_level);
            continue;
        }
        const auto& q = g_out.nav.q_nb;
        const double w = static_cast<double>(q[0]), x = static_cast<double>(q[1]), y = static_cast<double>(q[2]),
                     z = static_cast<double>(q[3]);
        const double roll = std::atan2(2 * (w * x + y * z), w * w - x * x - y * y + z * z);
        const double s2 = std::fmax(-1.0, std::fmin(1.0, 2 * (w * y - x * z)));
        const double pitch = std::asin(s2);
        const double yaw = std::atan2(2 * (x * y + w * z), w * w + x * x - y * y - z * z);
        const double tnav = static_cast<double>(g_out.nav.t.ns) * 1e-9;
        e_tilt = std::fmax(e_tilt, std::fmax(std::abs(roll - kRoll), std::abs(pitch - kPitch)));
        e_yaw = std::fmax(e_yaw, std::abs(sar(yaw - kYaw)));
        const double ed = std::abs(static_cast<double>(g_out.nav.depth) - sen.derinlik(tnav, 0));
        e_d = std::fmax(e_d, ed);
        if (t >= 10.0 && t < 24.0) {
            e_d_dalis = std::fmax(e_d_dalis, ed);
        }
    }
    std::printf("  en büyük hata: eğim %.4f rad, yön %.4f rad, derinlik %.4f m (10–24 s: %.4f m); son düzey %u,"
                " derinlik %.3f m\n",
                e_tilt, e_yaw, e_d, e_d_dalis, g_out.nav.est_level, static_cast<double>(g_out.nav.depth));
    if (hata > 0) return 1;
    return (e_tilt <= 0.03 && e_yaw <= 0.06 && e_d <= 0.06) ? 0 : 1;
}

}  // namespace

int main(int argc, char** argv)
{
    if (argc < 2) {
        std::printf("kullanım: %s durgun|dalis\n", argv[0]);
        return 2;
    }
    const bool dalis = std::strcmp(argv[1], "dalis") == 0;
    std::printf("[cekirdek_butunleme_%s]\n", argv[1]);
    const int r = kos(dalis);
    std::printf("[cekirdek_butunleme_%s] %s\n", argv[1], r == 0 ? "GEÇTİ" : "KALDI");
    return r;
}
