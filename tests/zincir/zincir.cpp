// İDA kapalı çevrim düzeneği (W3-zincir, Z1). Açıklama zincir.hpp'tadır.
#include "zincir.hpp"

#include "ida/fizik/dunya.hpp"
#include "ida/port.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdarg>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <map>
#include <memory>
#include <new>
#include <string>
#include <vector>

namespace ida::zincir {
namespace {

using fizik::Kuaterniyon;
using fizik::Vek3;

constexpr std::int64_t kMs = 1'000'000;
constexpr std::int64_t kP = 5 * kMs;            // kontrol periyodu (iskelet §4)
constexpr int kAltAdim = 4;                     // 5 ms / 1,25 ms
constexpr int kLactPeriyot = 2;                 // L_act: iki kontrol periyodu
constexpr double kPi = 3.14159265358979323846;

// ---- Sensör sabitleri (TAHMİN; ürün profillerinden kaba değerler, ölçüm değil) ---------
// IMU (ICM-42688-P sınıfı): jiroskop ~4,9e-5 rad/s/√Hz, ivmeölçer ~6,9e-4 m/s²/√Hz; 5 ms
// pencere ortalamasında σ ≈ yoğunluk / √0,005.
constexpr double kSigmaJiro = 6.9e-4;           // rad/s
constexpr double kSigmaIvme = 9.7e-3;           // m/s²
constexpr std::array<double, 3> kSapmaJiro = {0.002, -0.0015, 0.001};  // rad/s
constexpr std::array<double, 3> kSapmaIvme = {0.03, -0.02, 0.04};      // m/s²
constexpr double kSigmaMag = 5.0e-8;            // T
constexpr double kSigmaBaro = 10.0;             // Pa (≈ 1 mm su)
constexpr std::int64_t kBaroPencere = 18'750'000;  // ns (15 fizik adımı)
// Montajlar: scenarios/ida0.toml (A5) [MV]; yönelimler birim.
constexpr std::array<double, 3> kLImu = {0.0, 0.0, -0.03};
constexpr std::array<double, 3> kLBaro = {-0.19, 0.0, -0.03};
// Yerel manyetik alan, NED (T): scenarios/ornek-havuz.toml `ortam.manyetik_alan` [MV].
constexpr std::array<double, 3> kBn = {25.6e-6, 2.7e-6, 40.3e-6};
// ESC (A5 arac.esc; A4-K54'ün üç parametresi): merkez 1500 µs, kazanç 400 µs/birim, sınırlar.
constexpr double kPwmNotr = 1500.0;
constexpr double kPwmKazanc = 400.0;
constexpr double kPwmMin = 1100.0;
constexpr double kPwmMax = 1900.0;

// ---- Belirlenimci gürültü (splitmix64 + Box–Muller) ------------------------------------
class Gurultu {
public:
    explicit Gurultu(std::uint64_t tohum) : s_(tohum) {}
    double normal()
    {
        if (yedek_var_) {
            yedek_var_ = false;
            return yedek_;
        }
        double u1 = birim();
        while (!(u1 > 0.0)) {
            u1 = birim();
        }
        const double u2 = birim();
        const double r = std::sqrt(-2.0 * std::log(u1));
        yedek_ = r * std::sin(2.0 * kPi * u2);
        yedek_var_ = true;
        return r * std::cos(2.0 * kPi * u2);
    }

private:
    std::uint64_t sonraki()
    {
        std::uint64_t z = (s_ += 0x9E3779B97F4A7C15ULL);
        z = (z ^ (z >> 30U)) * 0xBF58476D1CE4E5B9ULL;
        z = (z ^ (z >> 27U)) * 0x94D049BB133111EBULL;
        return z ^ (z >> 31U);
    }
    double birim() { return static_cast<double>(sonraki() >> 11U) * (1.0 / 9007199254740992.0); }

    std::uint64_t s_;
    double yedek_ = 0.0;
    bool yedek_var_ = false;
};

// ---- Küçük vektör yardımcıları -------------------------------------------------------
using M3 = std::array<std::array<double, 3>, 3>;

M3 dcm(const Kuaterniyon& q)  // gövde → NED
{
    const double w = q[0], x = q[1], y = q[2], z = q[3];
    M3 r{};
    r[0] = {1.0 - 2.0 * (y * y + z * z), 2.0 * (x * y - w * z), 2.0 * (x * z + w * y)};
    r[1] = {2.0 * (x * y + w * z), 1.0 - 2.0 * (x * x + z * z), 2.0 * (y * z - w * x)};
    r[2] = {2.0 * (x * z - w * y), 2.0 * (y * z + w * x), 1.0 - 2.0 * (x * x + y * y)};
    return r;
}

Vek3 capraz(const Vek3& a, const Vek3& b)
{
    return {a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]};
}

Vek3 devrik_carp(const M3& r, const Vek3& v)  // Rᵀ v
{
    return {r[0][0] * v[0] + r[1][0] * v[1] + r[2][0] * v[2],
            r[0][1] * v[0] + r[1][1] * v[1] + r[2][1] * v[2],
            r[0][2] * v[0] + r[1][2] * v[1] + r[2][2] * v[2]};
}

struct Euler {
    double yalpa, bas_kic, yon;
};

Euler euler(const Kuaterniyon& q)
{
    const double w = q[0], x = q[1], y = q[2], z = q[3];
    const double sp = std::clamp(2.0 * (w * y - z * x), -1.0, 1.0);
    return {std::atan2(2.0 * (w * x + y * z), 1.0 - 2.0 * (x * x + y * y)), std::asin(sp),
            std::atan2(2.0 * (w * z + x * y), 1.0 - 2.0 * (y * y + z * z))};
}

Euler euler_f(const float q[4])
{
    const Kuaterniyon d = {static_cast<double>(q[0]), static_cast<double>(q[1]),
                           static_cast<double>(q[2]), static_cast<double>(q[3])};
    return euler(d);
}

double sar(double a)
{
    while (a > kPi) {
        a -= 2.0 * kPi;
    }
    while (a <= -kPi) {
        a += 2.0 * kPi;
    }
    return a;
}

#if defined(__GNUC__) || defined(__clang__)
__attribute__((format(printf, 2, 3)))
#endif
void ekle(std::string& s, const char* bicim, ...)
{
    char tampon[512];
    va_list ap;
    va_start(ap, bicim);
    const int n = std::vsnprintf(tampon, sizeof tampon, bicim, ap);
    va_end(ap);
    if (n > 0) {
        s.append(tampon, std::min(static_cast<std::size_t>(n), sizeof tampon - 1U));
    }
}

// ---- Çekirdek belleği (boyu ida_core_info'dan; hizalı) --------------------------------
struct CekirdekSilici {
    std::size_t hiza;
    void operator()(void* p) const { ::operator delete(p, std::align_val_t{hiza}); }
};
using CekirdekBellegi = std::unique_ptr<void, CekirdekSilici>;

CekirdekBellegi cekirdek_ayir()
{
    ida_core_info_t bilgi{};
    ida_core_info(&bilgi);
    const std::size_t hiza = std::max<std::size_t>(bilgi.core_align, alignof(std::max_align_t));
    void* p = ::operator new(bilgi.core_size, std::align_val_t{hiza});
    std::memset(p, 0, bilgi.core_size);
    return CekirdekBellegi(p, CekirdekSilici{hiza});
}

ida_core_t* cekirdek(const CekirdekBellegi& b)
{
    return static_cast<ida_core_t*>(b.get());
}

// ---- İtki arızası: şekil polinomunun tersiyle darbe genişliği ölçeklemesi ----------------
// Fizik arayüzünde kanal başına itki ölçeği yoktur (W3-zincir §3): düzenek o kanalın darbe
// genişliğini, durağan itki F_ss = şekil(|u|)·F_max (04 §2.7.2) çarpan kadar küçülecek biçimde
// nötre doğru ölçekler. Şekil, A5'in normalleştirilmiş polinomunun [0, u] üstündeki en
// büyüğüdür (fizikle aynı tanım); oran normalleştirme böleninden bağımsızdır.
class SekilTersi {
public:
    explicit SekilTersi(const std::array<double, 5>& c)
    {
        double enb = 0.0;
        for (int j = 0; j <= kN; ++j) {
            const double u = static_cast<double>(j) / kN;
            const double u2 = u * u;
            double t = c[4];
            t = t * u2 + c[3];
            t = t * u2 + c[2];
            t = t * u2 + c[1];
            t = t * u2 + c[0];
            enb = std::max(enb, t * u);
            tablo_[static_cast<std::size_t>(j)] = enb;
        }
    }
    double sekil(double u) const
    {
        const double x = std::clamp(u, 0.0, 1.0) * kN;
        const auto i = static_cast<std::size_t>(std::min(std::floor(x), static_cast<double>(kN - 1)));
        const double f = x - static_cast<double>(i);
        return tablo_[i] + f * (tablo_[i + 1U] - tablo_[i]);
    }
    // sekil(u') = hedef olan en küçük u'.
    double ters(double hedef) const
    {
        double lo = 0.0;
        double hi = 1.0;
        for (int k = 0; k < 60; ++k) {
            const double orta = 0.5 * (lo + hi);
            if (sekil(orta) < hedef) {
                lo = orta;
            } else {
                hi = orta;
            }
        }
        return hi;
    }

private:
    static constexpr int kN = 4000;
    std::array<double, kN + 1> tablo_{};
};

double pwm_olcekle(const SekilTersi& s, double pwm, double carpan)
{
    const double d = pwm - kPwmNotr;
    if (std::abs(d) <= 25.0) {
        return pwm;  // ölü bant: itki zaten 0
    }
    const double u = std::min(std::abs(d) / kPwmKazanc, 1.0);
    const double u2 = s.ters(carpan * s.sekil(u));
    const double d2 = std::round(u2 * kPwmKazanc);
    return (d > 0.0) ? kPwmNotr + d2 : kPwmNotr - d2;
}

// ---- Platform: thr_cmd → darbe genişliği (A4-K54, K59) ve sim'in yuvaları (A1-K72–K78) ---
// Çağrı k'nin çıktısı yuva k'ye konur ve uygulama anı a_k = t_k + L_act'te başlayan fizik
// adımının başında uygulanır (A1-K74): THR_VALID = 1 ise darbe genişlikleri yürürlükteki komut
// olur ve kullanım süresi a_k + valid_for_ns'te dolar; THR_VALID = 0 ise yürürlükteki komut
// değişmez. Kullanım süresi dolmuşsa ve yeni komut yoksa girdi nötrdür (A1-K77). SIL'de ızgara
// tick'i ve dolayısıyla eşzamansız güvenli çıktı (A1-K44, K78) yoktur: önceden kuyruğa alınmış
// olağan yanıtlar iptal edilmez, kendi yuvalarında uygulanır. EMNİYET: DISARM'ı taşıyan çağrının
// nötr çıktısı a_k'de yürürlüğe girer; o ana kadar önceki iki çağrının komutları uygulanır.
struct Yuva {
    bool thr_valid = false;
    bool guvenli = false;             // çıktı silahsız ya da is_safe (A4-K55)
    std::array<double, 8> pwm{};
    std::int64_t valid_for_ns = 0;
};

struct Platform {
    std::uint32_t duzeltilen = 0;     // A4-K59
    std::map<std::uint64_t, Yuva> yuvalar;   // anahtar: uygulama anının fizik adımı
    std::array<double, 8> yururluk{};        // yürürlükteki komut (µs; arızadan önce)
    bool yururluk_guvenli = true;            // yürürlükteki komutun kaynağı güvenli çıktı
    std::int64_t yururluk_son_ns = 0;        // kullanım süresinin sonu

    Platform() { yururluk.fill(kPwmNotr); }

    // Kart tarafı: çıktının darbe genişlikleri (A4-K54; A4-K59 düzeltmesi).
    Yuva cevir(const ida_out_t& o)
    {
        Yuva y;
        y.thr_valid = o.thr_valid;
        y.guvenli = !o.armed || o.is_safe;
        y.valid_for_ns = static_cast<std::int64_t>(o.valid_for_ns);
        bool gecerli = true;
        for (float v : o.thr_cmd) {
            gecerli = gecerli && std::isfinite(v) && (v >= -1.0F) && (v <= 1.0F);
        }
        if (o.thr_valid && !gecerli) {
            ++duzeltilen;
            y.guvenli = true;
        }
        for (std::size_t i = 0; i < 8; ++i) {
            const double u = (o.thr_valid && gecerli) ? static_cast<double>(o.thr_cmd[i]) : 0.0;
            y.pwm[i] = std::round(std::clamp(kPwmNotr + kPwmKazanc * u, kPwmMin, kPwmMax));
        }
        return y;
    }

    // Sim tarafı: a = adim_no'da başlayan fizik adımının başı (zaman t_ns).
    void adim_basi(std::uint64_t adim_no, std::int64_t t_ns)
    {
        const auto it = yuvalar.find(adim_no);
        if (it != yuvalar.end()) {
            if (it->second.thr_valid) {
                yururluk = it->second.pwm;
                yururluk_guvenli = it->second.guvenli;
                yururluk_son_ns = t_ns + it->second.valid_for_ns;
            }
            yuvalar.erase(it);
        }
        if (t_ns >= yururluk_son_ns) {
            yururluk.fill(kPwmNotr);  // A1-K77
            yururluk_guvenli = true;
        }
    }

    bool kuyrukta_notr_disi() const
    {
        for (const auto& [adim, y] : yuvalar) {
            (void)adim;
            for (double p : y.pwm) {
                if (y.thr_valid && p != kPwmNotr) {
                    return true;
                }
            }
        }
        return false;
    }
};

// ---- Parametre bloğu: SIL koşucusunun ürettiği blok (A4-K78) ---------------------------
// Bloğun kendisi çekirdeğin PARAM_SAVE çıktısından alınır (yalnız port işlevleriyle): geçici
// bir örnek varsayılanlarla kurulur, senaryonun yapılandırma değerleri PARAM_SET + PARAM_COMMIT
// + PARAM_SAVE ile tek çağrıda verilir, çıkan blok asıl örneğin init'ine konur.
struct ParamAyari {
    std::string ad;
    float deger;
};

bool blok_uret(const std::vector<ParamAyari>& ayarlar, std::vector<std::uint8_t>& blok,
               std::uint32_t& gen, std::string& hata)
{
    CekirdekBellegi bel = cekirdek_ayir();
    ida_core_t* c = cekirdek(bel);
    ida_init_in_t ii{};
    ii.port_major = static_cast<std::uint16_t>(IDA_PORT_VERSION_MAJOR);
    ii.port_minor = static_cast<std::uint16_t>(IDA_PORT_VERSION_MINOR);
    ii.now_w.ns = 0;
    ii.boot_count = 1;
    ii.boot_count_ok = true;
    ii.reset_cause = static_cast<std::uint8_t>(IDA_RESET_SIL_START);
    ii.param_src = static_cast<std::uint8_t>(IDA_PSRC_NONE);
    if (ida_core_init(c, &ii) != IDA_RC_PARAM_DEFAULTS) {
        hata = "geçici örneğin init'i PARAM_DEFAULTS dönmedi";
        return false;
    }
    static ida_in_t in;
    static ida_out_t out;
    // Komutlar çağrı başına en çok IDA_PORT_MAX_CMDS; PARAM_SET'ler bekletilir (A4-K81), son
    // çağrı PARAM_COMMIT ve PARAM_SAVE taşır.
    std::vector<ida_cmd_t> komutlar;
    std::uint32_t id = 1;
    for (const ParamAyari& p : ayarlar) {
        std::uint16_t pid = 0;
        if (ida_core_param_find(p.ad.c_str(), &pid) != IDA_RC_OK) {
            hata = "parametre bulunamadı: " + p.ad;
            return false;
        }
        ida_cmd_t k{};
        k.cmd_id = id++;
        k.type = static_cast<std::uint8_t>(IDA_CMD_PARAM_SET);
        k.u.param.id = pid;
        k.u.param.type = static_cast<std::uint8_t>(IDA_PT_F32);
        k.u.param.v.f32 = p.deger;
        komutlar.push_back(k);
    }
    ida_cmd_t kc{};
    kc.cmd_id = id++;
    kc.type = static_cast<std::uint8_t>(IDA_CMD_PARAM_COMMIT);
    komutlar.push_back(kc);
    ida_cmd_t ks{};
    ks.cmd_id = id;
    ks.type = static_cast<std::uint8_t>(IDA_CMD_PARAM_SAVE);
    komutlar.push_back(ks);
    const std::uint32_t save_id = id;
    std::size_t sira = 0;
    for (std::uint32_t seq = 1; sira < komutlar.size(); ++seq) {
        const std::int64_t t = static_cast<std::int64_t>(seq) * kP;
        std::memset(&in, 0, sizeof in);
        in.now_w.ns = t;
        in.tf.present = true;
        in.tf.seq = seq;
        in.tf.t.ns = t;
        in.tf.t_rx.ns = t;
        in.imu_present = true;
        in.imu.hdr.t_meas.ns = t;
        in.imu.hdr.t_rx.ns = t;
        in.imu.hdr.seq = seq;
        in.imu.hdr.window_ns = static_cast<std::uint32_t>(kP);
        in.imu.hdr.flags = static_cast<std::uint16_t>(IDA_HF_VALID);
        in.imu.hdr.type = static_cast<std::uint8_t>(IDA_SENS_IMU);
        in.imu.imu.accel[2] = -9.82F;
        in.plat.link_state = static_cast<std::uint8_t>(IDA_LINK_RUNNING);
        in.plat.link_event = static_cast<std::uint8_t>((seq == 1U) ? IDA_LINKEV_SESSION_START : IDA_LINKEV_NONE);
        // Son iki komut (COMMIT, SAVE) aynı çağrıda kalır.
        while ((sira < komutlar.size()) && (in.n_cmds < IDA_PORT_MAX_CMDS)) {
            const bool son_ikili = (sira + 2U == komutlar.size());
            if (son_ikili && (in.n_cmds + 2U > IDA_PORT_MAX_CMDS)) {
                break;
            }
            in.cmds[in.n_cmds++] = komutlar[sira++];
        }
        if (ida_core_step(c, &in, &out) != IDA_RC_OK) {
            hata = "geçici örneğin step'i OK dönmedi";
            return false;
        }
        for (std::uint8_t i = 0; i < out.n_results; ++i) {
            const ida_cmd_result_t& r = out.results[i];
            if (r.ack != static_cast<std::uint8_t>(IDA_ACK_ACCEPTED)) {
                hata = "parametre komutu reddedildi (cmd_id " + std::to_string(r.cmd_id) + ", neden " +
                       std::to_string(r.reason) + ")";
                return false;
            }
            if ((r.cmd_id == save_id) && (r.save.blob != nullptr)) {
                blok.assign(r.save.blob, r.save.blob + r.save.len);
                gen = r.save.gen;
            }
        }
    }
    if (blok.empty()) {
        hata = "PARAM_SAVE bloğu üretilmedi";
        return false;
    }
    return true;
}

// ---- Senaryo komutları -------------------------------------------------------------------
constexpr std::uint32_t kIdArm = 1001;
constexpr std::uint32_t kIdKip = 1002;
constexpr std::uint32_t kIdSp = 1003;
constexpr std::uint32_t kIdSp2 = 1005;
constexpr std::uint32_t kIdDisarm = 1004;
constexpr std::uint32_t kIdHbTaban = 100000;

bool an(std::int64_t t_ns, double t_s)
{
    // t_s anını içeren kontrol periyodunun çağrısı: (t − P, t].
    const auto hedef = static_cast<std::int64_t>(std::llround(t_s * 1e9));
    return (t_ns >= hedef) && (t_ns - kP < hedef);
}

void sonuc_ara(const ida_out_t& o, std::uint32_t id, int& ack, int& neden)
{
    for (std::uint8_t i = 0; i < o.n_results; ++i) {
        if (o.results[i].cmd_id == id) {
            ack = o.results[i].ack;
            neden = o.results[i].reason;
        }
    }
}

}  // namespace

Sonuc kos(const Ayar& ayar)
{
    Sonuc s;
    // ---- Fizik ----
    const fizik::AracParametreleri arac = fizik::ida0_arac();
    const fizik::OrtamParametreleri ortam = fizik::havuz_ortami();
    fizik::BaslangicDurumu bas;
    fizik::Dunya dn;
    fizik::KurulumSonucu kr = dn.kur(arac, ortam, bas);
    if (kr.tamam) {
        bas.konum[2] = dn.turetilmis().denge_konumu_z;  // düz suda yüzer denge
        kr = dn.kur(arac, ortam, bas);
    }
    if (!kr.tamam) {
        s.hata = "fizik kurulumu: " + kr.hata;
        return s;
    }
    s.denge_derinligi_m = bas.konum[2];
    const double g = ortam.yercekimi;
    const double rho = ortam.su.yogunluk;
    const double p_atm = ortam.atmosfer_basinci;
    const SekilTersi sekil(arac.itici_modeli.sekil_polinomu);

    // ---- Parametre bloğu ve çekirdek ----
    // Yapılandırma (senaryodan, A5): havuzun yerçekimi ve kalibrasyon anındaki O_b derinliği
    // (araç yüzeyde yüzer dengede). Kazançlar çekirdeğin varsayılanlarıdır.
    std::vector<ParamAyari> ayarlar = {
        {"YER_G", static_cast<float>(g)},
        {"BARO_KAL_D", static_cast<float>(s.denge_derinligi_m)},
    };
    for (const auto& p : ayar.parametreler) {
        ayarlar.push_back({p.first, p.second});
    }
    std::vector<std::uint8_t> blok;
    std::uint32_t gen = 0;
    if (!blok_uret(ayarlar, blok, gen, s.hata)) {
        return s;
    }
    CekirdekBellegi bel = cekirdek_ayir();
    ida_core_t* c = cekirdek(bel);
    ida_init_in_t ii{};
    ii.port_major = static_cast<std::uint16_t>(IDA_PORT_VERSION_MAJOR);
    ii.port_minor = static_cast<std::uint16_t>(IDA_PORT_VERSION_MINOR);
    ii.now_w.ns = 0;
    ii.boot_count = 1;
    ii.boot_count_ok = true;
    ii.reset_cause = static_cast<std::uint8_t>(IDA_RESET_SIL_START);
    ii.param_src = static_cast<std::uint8_t>(IDA_PSRC_SIL);
    ii.param_blob = blok.data();
    ii.param_len = static_cast<std::uint32_t>(blok.size());
    if (ida_core_init(c, &ii) != IDA_RC_OK) {
        s.hata = "çekirdek init OK dönmedi (blok reddedildi)";
        return s;
    }
    s.kurulum_tamam = true;

    // ---- Döngü ----
    static ida_in_t in;
    static ida_out_t out;
    Gurultu grt(ayar.tohum);
    Platform plat;
    std::deque<double> baro_halka;  // fizik adımı başına basınç (Pa), en çok 15
    std::uint32_t seq_imu = 0, seq_mag = 0, seq_baro = 0, seq_batt = 0, seq_leak = 0, seq_emn = 0;
    std::uint32_t hb_id = kIdHbTaban;
    bool onceki_armed = false;
    const auto ariza_t_ns = static_cast<std::int64_t>(std::llround(ayar.t_manevra_s * 1e9));
    const auto n_cagri = static_cast<std::int64_t>(std::llround(ayar.sure_s * 1e9)) / kP;
    if (ayar.csv_uret) {
        s.csv = "t_s,derinlik_m,derinlik_k_m,yon_rad,yon_k_rad,yalpa_rad,yalpa_k_rad,bas_kic_rad,bas_kic_k_rad";
        for (int i = 0; i < 8; ++i) {
            ekle(s.csv, ",thr%d", i);
        }
        for (int i = 0; i < 8; ++i) {
            ekle(s.csv, ",pwm_uret%d", i);
        }
        for (int i = 0; i < 8; ++i) {
            ekle(s.csv, ",pwm_yur%d", i);
        }
        for (int i = 0; i < 8; ++i) {
            ekle(s.csv, ",pwm_fiz%d", i);
        }
        s.csv += ",kip,armed,fs_level,fs_reasons,est_level,thr_valid,is_safe,rc\n";
    }
    s.satirlar.reserve(static_cast<std::size_t>(n_cagri));

    for (std::int64_t k = 1; k <= n_cagri; ++k) {
        const std::int64_t t = k * kP;
        // Fizik: (t − P, t]; IMU penceresi aynı aralığın ortalamasıdır.
        Vek3 f_top{}, w_top{};
        std::array<double, 8> pwm_yur{};
        std::array<double, 8> pwm_fiz{};
        for (int a = 0; a < kAltAdim; ++a) {
            // Fizik adımının başı: yuva uygulanır (A1-K74, K77), arıza fiziksel iticiye uygulanır.
            const std::uint64_t n = dn.adim_no();
            const std::int64_t t_n = dn.zaman_ns();
            plat.adim_basi(n, t_n);
            pwm_yur = plat.yururluk;
            pwm_fiz = pwm_yur;
            if (t_n >= ariza_t_ns) {
                for (int kanal : ayar.ariza_kanallari) {
                    const auto ch = static_cast<std::size_t>(kanal);
                    pwm_fiz[ch] = pwm_olcekle(sekil, pwm_fiz[ch], ayar.ariza_carpani);
                }
            }
            (void)dn.itici_komutu_ver(n, pwm_fiz);
            // Emniyet değişmezi (kabul md.4): güvenli çıktıdan gelen komut fiziğe nötr gider.
            if (plat.yururluk_guvenli) {
                for (double p : pwm_fiz) {
                    if (p != kPwmNotr) {
                        s.ihlaller.push_back("t=" + std::to_string(t_n) + ": güvenli çıktıdan gelen komut nötr değil");
                        break;
                    }
                }
            }
            // DISARM'ın fizik tarafındaki etkin anından sonra uygulanan her darbe genişliği nötr.
            if ((s.t_disarm_ns >= 0) && (t_n >= s.t_disarm_ns + kLactPeriyot * kP)) {
                for (double p : pwm_fiz) {
                    if (p != kPwmNotr) {
                        s.ihlaller.push_back("t=" + std::to_string(t_n) + ": DISARM etkin anından sonra nötr dışı");
                        break;
                    }
                }
            }
            if (dn.adim() != fizik::AdimDurumu::tamam) {
                ++s.sayisal_hata_fizik;
                s.hata = "fizik sayısal hata";
                return s;
            }
            const fizik::DurumAnlik& d = dn.durum();
            for (std::size_t i = 0; i < 8; ++i) {
                if (d.pwm_us[i] != pwm_fiz[i]) {
                    s.ihlaller.push_back("t=" + std::to_string(t_n) + ": fiziğin yürürlükteki komutu verilenden farklı");
                    break;
                }
            }
            if (s.t_disarm_ns >= 0) {
                bool notr = true;
                double en_buyuk_itki = 0.0;
                for (std::size_t i = 0; i < 8; ++i) {
                    notr = notr && (d.pwm_us[i] == kPwmNotr);
                    en_buyuk_itki = std::max(en_buyuk_itki, std::abs(d.itki[i]));
                }
                if (notr && (s.t_pwm_notr_ns < 0)) {
                    s.t_pwm_notr_ns = t_n;  // adımın başından beri nötr
                }
                if (!notr) {
                    s.t_pwm_notr_ns = -1;
                }
                if ((en_buyuk_itki < 0.05) && (s.t_itki_sifir_ns < 0)) {
                    s.t_itki_sifir_ns = d.t_ns;
                }
                if (en_buyuk_itki >= 0.05) {
                    s.t_itki_sifir_ns = -1;
                }
            }
            const M3 r = dcm(d.yonelim);
            // IMU montaj noktasında özgül kuvvet: f + ω̇ × l + ω × (ω × l).
            const Vek3 l = {kLImu[0], kLImu[1], kLImu[2]};
            const Vek3 c1 = capraz(d.acisal_ivme_b, l);
            const Vek3 c2 = capraz(d.acisal_hiz_b, capraz(d.acisal_hiz_b, l));
            for (std::size_t i = 0; i < 3; ++i) {
                f_top[i] += d.ozgul_kuvvet_b[i] + c1[i] + c2[i];
                w_top[i] += d.acisal_hiz_b[i];
            }
            // Basınç algılayıcısının derinliği; havada atmosfer basıncı.
            const double z_b = d.konum_n[2] + r[2][0] * kLBaro[0] + r[2][1] * kLBaro[1] + r[2][2] * kLBaro[2];
            baro_halka.push_back(p_atm + rho * g * std::max(z_b, 0.0));
            if (baro_halka.size() > static_cast<std::size_t>(kBaroPencere / dn.adim_ns())) {
                baro_halka.pop_front();
            }
        }
        const fizik::DurumAnlik& d = dn.durum();
        const M3 r = dcm(d.yonelim);

        std::memset(&in, 0, sizeof in);
        in.now_w.ns = t;
        in.tf.present = true;
        in.tf.seq = static_cast<std::uint32_t>(k);
        in.tf.t.ns = t;
        in.tf.t_rx.ns = t;
        in.imu_present = true;
        in.plat.link_state = static_cast<std::uint8_t>(IDA_LINK_RUNNING);
        in.plat.link_event = static_cast<std::uint8_t>((k == 1) ? IDA_LINKEV_SESSION_START : IDA_LINKEV_NONE);
        in.plat.param_store = static_cast<std::uint8_t>(IDA_PSTORE_IDLE);
        in.plat.param_gen = gen;
        // IMU (inst 0; sensör çerçevesi = gövde)
        ida_imu_sample_t& imu = in.imu;
        imu.hdr.t_meas.ns = t;
        imu.hdr.t_rx.ns = t;
        imu.hdr.seq = ++seq_imu;
        imu.hdr.window_ns = static_cast<std::uint32_t>(kP);
        imu.hdr.flags = static_cast<std::uint16_t>(IDA_HF_VALID);
        imu.hdr.type = static_cast<std::uint8_t>(IDA_SENS_IMU);
        for (std::size_t i = 0; i < 3; ++i) {
            imu.imu.gyro[i] = static_cast<float>(w_top[i] / kAltAdim + kSapmaJiro[i] + kSigmaJiro * grt.normal());
            imu.imu.accel[i] = static_cast<float>(f_top[i] / kAltAdim + kSapmaIvme[i] + kSigmaIvme * grt.normal());
        }
        imu.imu.temp_c = 20.0F;
        auto ornek = [&](std::uint8_t tur, std::uint32_t sq, std::uint32_t pencere) -> ida_sample_t& {
            ida_sample_t& o = in.samples[in.n_samples++];
            o.hdr.t_meas.ns = t;
            o.hdr.t_rx.ns = t;
            o.hdr.seq = sq;
            o.hdr.window_ns = pencere;
            o.hdr.flags = static_cast<std::uint16_t>(IDA_HF_VALID);
            o.hdr.type = tur;
            o.hdr.inst = 0;
            return o;
        };
        if (k % 2 == 0) {  // manyetometre 100 Hz
            const Vek3 bn = {kBn[0], kBn[1], kBn[2]};
            const Vek3 mb = devrik_carp(r, bn);
            ida_sample_t& o = ornek(static_cast<std::uint8_t>(IDA_SENS_MAG), ++seq_mag, 0);
            for (std::size_t i = 0; i < 3; ++i) {
                o.u.mag.field[i] = static_cast<float>(mb[i] + kSigmaMag * grt.normal());
            }
        }
        if (k % 8 == 0) {  // basınç 25 Hz, 18,75 ms pencere ortalaması
            double top = 0.0;
            for (double p : baro_halka) {
                top += p;
            }
            ida_sample_t& o = ornek(static_cast<std::uint8_t>(IDA_SENS_BARO), ++seq_baro, static_cast<std::uint32_t>(kBaroPencere));
            o.u.baro.p_abs = static_cast<float>(top / static_cast<double>(baro_halka.size()) + kSigmaBaro * grt.normal());
            o.u.baro.temp_c = 20.0F;
        }
        if (k % 20 == 0) {  // batarya 10 Hz (ideal kaynak, A5 v_sabit)
            ida_sample_t& o = ornek(static_cast<std::uint8_t>(IDA_SENS_BATT), ++seq_batt, static_cast<std::uint32_t>(100 * kMs));
            o.u.batt.voltage = static_cast<float>(arac.batarya.v_sabit);
            o.u.batt.current = 2.0F;
            o.u.batt.temp_c = 25.0F;
        }
        if (k % 20 == 10) {  // sızıntı 10 Hz, kuru
            ida_sample_t& o = ornek(static_cast<std::uint8_t>(IDA_SENS_LEAK), ++seq_leak, 0);
            o.u.leak.mask = 0;
        }
        if (k % 20 == 5) {  // emniyet örneği 10 Hz: acil durdurma yok, tüpler dolu, fiş ve pim emniyette
            ida_sample_t& o = ornek(static_cast<std::uint8_t>(IDA_SENS_SAFETY), ++seq_emn, 0);
            o.u.safety.estop = 0;
            o.u.safety.tube[0] = static_cast<std::uint8_t>(IDA_TUBE_LOADED);
            o.u.safety.tube[1] = static_cast<std::uint8_t>(IDA_TUBE_LOADED);
        }
        // Komutlar: yer istasyonu canlılığı 1 Hz ve senaryo.
        auto komut = [&](std::uint32_t id, std::uint8_t tur) -> ida_cmd_t& {
            ida_cmd_t& kk = in.cmds[in.n_cmds++];
            kk.cmd_id = id;
            kk.type = tur;
            return kk;
        };
        if ((k % 200 == 100) && ((ayar.t_hb_kes_s < 0.0) || (static_cast<double>(t) * 1e-9 < ayar.t_hb_kes_s))) {
            (void)komut(++hb_id, static_cast<std::uint8_t>(IDA_CMD_HEARTBEAT));
        }
        if (an(t, ayar.t_arm_s)) {
            (void)komut(kIdArm, static_cast<std::uint8_t>(IDA_CMD_ARM));
        }
        if (an(t, ayar.t_kip_s)) {
            komut(kIdKip, static_cast<std::uint8_t>(IDA_CMD_SET_MODE)).u.set_mode.mode = static_cast<std::uint8_t>(IDA_MODE_GUIDED);
        }
        if (an(t, ayar.t_sp_s)) {
            ida_cmd_t& kk = komut(kIdSp, static_cast<std::uint8_t>(IDA_CMD_GUIDED_SP));
            kk.u.guided.mask = static_cast<std::uint8_t>(IDA_SP_DEPTH | IDA_SP_YAW);
            kk.u.guided.depth = static_cast<float>(ayar.hedef_derinlik_m);
            kk.u.guided.yaw = static_cast<float>(ayar.hedef_yon_rad);
        }
        if (an(t, ayar.t_manevra_s)) {
            ida_cmd_t& kk = komut(kIdSp2, static_cast<std::uint8_t>(IDA_CMD_GUIDED_SP));
            kk.u.guided.mask = static_cast<std::uint8_t>(IDA_SP_DEPTH | IDA_SP_YAW);
            kk.u.guided.depth = static_cast<float>(ayar.hedef2_derinlik_m);
            kk.u.guided.yaw = static_cast<float>(ayar.hedef2_yon_rad);
        }
        const bool disarm_cagrisi = an(t, ayar.t_disarm_s);
        if (disarm_cagrisi) {
            in.narrow.mask = static_cast<std::uint8_t>(IDA_NARROW_DISARM);  // daraltıcı: yalnız mandalla (A4-K46)
            in.narrow.cmd_id[0] = kIdDisarm;
        }

        const ida_rc_t rc = ida_core_step(c, &in, &out);
        sonuc_ara(out, kIdArm, s.arm_ack, s.arm_neden);
        sonuc_ara(out, kIdKip, s.kip_ack, s.kip_neden);
        sonuc_ara(out, kIdSp, s.sp_ack, s.sp_neden);
        sonuc_ara(out, kIdSp2, s.sp2_ack, s.sp2_neden);
        int dn_ack = -1, dn_neden = -1;
        sonuc_ara(out, kIdDisarm, dn_ack, dn_neden);
        if (dn_ack >= 0) {
            s.disarm_ack = dn_ack;
        }

        // Emniyet değişmezleri (W3-zincir kabul md.4).
        int arm_bu = -1, arm_neden_bu = -1;
        sonuc_ara(out, kIdArm, arm_bu, arm_neden_bu);
        if (out.armed && !onceki_armed && (arm_bu != static_cast<int>(IDA_ACK_ACCEPTED))) {
            s.ihlaller.push_back("t=" + std::to_string(t) + ": armed kabul edilmiş ARM olmadan 1 oldu");
        }
        if (out.armed && !onceki_armed) {
            s.t_arm_kabul_s = static_cast<double>(t) * 1e-9;
        }
        onceki_armed = out.armed;

        const Yuva yuva = plat.cevir(out);
        if (disarm_cagrisi) {
            s.t_disarm_ns = t;
            s.disarm_kuyrukta_notr_disi = plat.kuyrukta_notr_disi();
        }
        // Emniyet değişmezi: güvenli çıktıda itki isteği tam 0 ve darbe genişliği nötr.
        if (yuva.guvenli) {
            for (std::size_t i = 0; i < 8; ++i) {
                if ((yuva.pwm[i] != kPwmNotr) || (out.thr_valid && out.thr_cmd[i] != 0.0F)) {
                    s.ihlaller.push_back("t=" + std::to_string(t) + ": silahsız/güvenli çıktıda nötr dışı komut");
                    break;
                }
            }
        }
        // Karşı örnek (yalnız test): platform hatası taklidi; DISARM çağrısının nötr çıktısı
        // kuyruğa konmaz, yürürlükteki nötr dışı komut kullanım süresi dolana dek sürer.
        if (!(disarm_cagrisi && ayar.karsi_ornek_disarm_yuvasi_atlanir)) {
            plat.yuvalar[static_cast<std::uint64_t>((k + kLactPeriyot) * kAltAdim)] = yuva;
        }

        // Kayıt.
        Satir sr;
        sr.t_s = static_cast<double>(t) * 1e-9;
        const Euler eg = euler(d.yonelim);
        sr.derinlik = d.konum_n[2];
        sr.yon = eg.yon;
        sr.yalpa = eg.yalpa;
        sr.bas_kic = eg.bas_kic;
        sr.tau_z = d.itici_kuvvet_momenti[2];
        sr.tau_n = d.itici_kuvvet_momenti[5];
        if (out.nav_valid) {
            const Euler ek = euler_f(out.nav.q_nb);
            sr.derinlik_k = static_cast<double>(out.nav.depth);
            sr.yon_k = ek.yon;
            sr.yalpa_k = ek.yalpa;
            sr.bas_kic_k = ek.bas_kic;
        }
        for (std::size_t i = 0; i < 8; ++i) {
            sr.thr[i] = out.thr_cmd[i];
            sr.pwm_uret[i] = static_cast<std::int32_t>(yuva.pwm[i]);
            sr.pwm_yur[i] = static_cast<std::int32_t>(pwm_yur[i]);
            sr.pwm_fiz[i] = static_cast<std::int32_t>(pwm_fiz[i]);
        }
        sr.kip = out.status.mode;
        sr.armed = out.armed ? 1U : 0U;
        sr.fs_level = out.fs_level;
        sr.fs_reasons = out.status.fs_reasons;
        sr.est_level = out.nav.est_level;
        sr.thr_valid = out.thr_valid ? 1U : 0U;
        sr.is_safe = out.is_safe ? 1U : 0U;
        sr.rc = static_cast<int>(rc);
        if (rc != IDA_RC_OK) {
            s.ihlaller.push_back("t=" + std::to_string(t) + ": çağrı OK dönmedi (rc " + std::to_string(sr.rc) + ")");
        }
        if (ayar.csv_uret) {
            ekle(s.csv, "%.3f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f", sr.t_s, sr.derinlik, sr.derinlik_k,
                 sr.yon, sr.yon_k, sr.yalpa, sr.yalpa_k, sr.bas_kic, sr.bas_kic_k);
            for (float v : sr.thr) {
                ekle(s.csv, ",%.6f", static_cast<double>(v));
            }
            for (std::int32_t v : sr.pwm_uret) {
                ekle(s.csv, ",%d", v);
            }
            for (std::int32_t v : sr.pwm_yur) {
                ekle(s.csv, ",%d", v);
            }
            for (std::int32_t v : sr.pwm_fiz) {
                ekle(s.csv, ",%d", v);
            }
            ekle(s.csv, ",%u,%u,%u,0x%x,%u,%u,%u,%d\n", static_cast<unsigned>(sr.kip),
                 static_cast<unsigned>(sr.armed), static_cast<unsigned>(sr.fs_level),
                 static_cast<unsigned>(sr.fs_reasons), static_cast<unsigned>(sr.est_level),
                 static_cast<unsigned>(sr.thr_valid), static_cast<unsigned>(sr.is_safe), sr.rc);
        }
        s.satirlar.push_back(sr);
    }
    s.cikti_duzeltildi = plat.duzeltilen;
    return s;
}

namespace {

// t_bas ile t_son arasındaki evre: hedef (derinlik, yön), GUIDED_SP anı t_bas.
Evre evre_olc(const std::vector<Satir>& satirlar, double t_bas, double t_son, double hedef_d,
              double hedef_y, double onceki_d, double onceki_y, double bant, double yon_bant,
              double tutum_bant)
{
    Evre e;
    const double p = 1e-9 * static_cast<double>(kP);
    const double yon_d = (hedef_d >= onceki_d) ? 1.0 : -1.0;
    // Dönüş yönü: önceki yönden hedefe sarılmış fark (−π, π]; tam π'de pozitif yön.
    const double fark = sar(hedef_y - onceki_y);
    const double yon_y = (fark >= 0.0) ? 1.0 : -1.0;
    double son_disari = -1.0;
    double son_disari_y = -1.0;
    double son_disari_t = -1.0;
    for (const Satir& r : satirlar) {
        if (r.t_s < t_bas || r.t_s >= t_son) {
            continue;
        }
        const double ed = r.derinlik - hedef_d;
        const double ey = sar(r.yon - hedef_y);
        e.asim_m = std::max(e.asim_m, yon_d * ed);
        // Yön aşması yalnız hedefe varıldıktan sonra anlamlıdır (|hata| < π/2 iken).
        if (std::abs(ey) < 0.5 * kPi) {
            e.yon_asim_rad = std::max(e.yon_asim_rad, yon_y * ey);
        }
        e.tutum_rad = std::max({e.tutum_rad, std::abs(r.yalpa), std::abs(r.bas_kic)});
        if (std::abs(ed) > bant) {
            son_disari = r.t_s;
        }
        if (std::abs(ey) > yon_bant) {
            son_disari_y = r.t_s;
        }
        if (std::max(std::abs(r.yalpa), std::abs(r.bas_kic)) > tutum_bant) {
            son_disari_t = r.t_s;
        }
        if (r.t_s >= t_son - 20.0) {
            e.tutma_derinlik_m = std::max(e.tutma_derinlik_m, std::abs(ed));
            e.tutma_yon_rad = std::max(e.tutma_yon_rad, std::abs(ey));
        }
    }
    e.oturma_s = (son_disari < 0.0) ? 0.0 : (son_disari + p - t_bas);
    if (son_disari >= t_son - p) {
        e.oturma_s = -1.0;
    }
    e.yon_oturma_s = (son_disari_y < 0.0) ? 0.0 : (son_disari_y + p - t_bas);
    if (son_disari_y >= t_son - p) {
        e.yon_oturma_s = -1.0;
    }
    e.tutum_oturma_s = (son_disari_t < 0.0) ? 0.0 : (son_disari_t + p - t_bas);
    if (son_disari_t >= t_son - p) {
        e.tutum_oturma_s = -1.0;
    }
    return e;
}

}  // namespace

Ozet ozetle(const Sonuc& s, const Ayar& a)
{
    Ozet o;
    o.dalis = evre_olc(s.satirlar, a.t_sp_s, a.t_manevra_s, a.hedef_derinlik_m, a.hedef_yon_rad,
                       s.denge_derinligi_m, 0.0, o.bant_m, o.yon_bant_rad, o.tutum_bant_rad);
    o.manevra = evre_olc(s.satirlar, a.t_manevra_s, a.t_disarm_s, a.hedef2_derinlik_m, a.hedef2_yon_rad,
                         a.hedef_derinlik_m, a.hedef_yon_rad, o.bant_m, o.yon_bant_rad, o.tutum_bant_rad);
    std::vector<std::uint32_t> etkin(a.ariza_kanallari.size(), 0U);
    std::vector<std::uint32_t> degisti(a.ariza_kanallari.size(), 0U);
    bool disarm_sonrasi_tamam = true;
    bool disarm_goruldu = false;
    for (const Satir& r : s.satirlar) {
        if (r.t_s >= a.t_arm_s && r.t_s < a.t_disarm_s) {
            o.en_yuksek_fs = std::max(o.en_yuksek_fs, static_cast<int>(r.fs_level));
            o.en_yuksek_est = std::max(o.en_yuksek_est, static_cast<int>(r.est_level));
            if (r.est_level == static_cast<std::uint8_t>(IDA_EST_FAULT) || (r.fs_reasons & IDA_FSR_NUMERIC) != 0U) {
                o.sayisal_cokme = true;
            }
            o.kestirim_derinlik_hata_m = std::max(o.kestirim_derinlik_hata_m, std::abs(r.derinlik_k - r.derinlik));
            o.kestirim_yon_hata_rad = std::max(o.kestirim_yon_hata_rad, std::abs(sar(r.yon_k - r.yon)));
        }
        // Satırın fizik adımı (t − h, t] arızanın başlangıcından sonra başlamış olmalı.
        if (r.t_s >= a.t_manevra_s + 0.002 && r.t_s < a.t_disarm_s) {
            for (std::size_t j = 0; j < a.ariza_kanallari.size(); ++j) {
                const auto ch = static_cast<std::size_t>(a.ariza_kanallari[j]);
                if (std::abs(r.pwm_yur[ch] - static_cast<std::int32_t>(kPwmNotr)) > 25) {
                    ++etkin[j];
                    if (r.pwm_fiz[ch] != r.pwm_yur[ch]) {
                        ++degisti[j];
                    }
                }
            }
        }
        // DISARM çağrısının kendisi ve sonrası: itki isteği 0, üretilen darbe genişliği nötr;
        // fiziğe giden darbe genişliği DISARM'ın uygulama anından (t + L_act) sonra başlayan
        // ilk satırdan itibaren nötr (satırın fizik adımları (t − P, t]).
        if (r.t_s >= a.t_disarm_s - 0.5 * 1e-9 * static_cast<double>(kP)) {
            disarm_goruldu = true;
            const bool fiz_kontrol =
                r.t_s >= a.t_disarm_s + 1e-9 * static_cast<double>((kLactPeriyot + 1) * kP) - 1e-6;
            for (std::size_t i = 0; i < 8; ++i) {
                if (r.thr[i] != 0.0F || r.pwm_uret[i] != static_cast<std::int32_t>(kPwmNotr) || r.armed != 0U ||
                    (fiz_kontrol && r.pwm_fiz[i] != static_cast<std::int32_t>(kPwmNotr))) {
                    disarm_sonrasi_tamam = false;
                }
            }
        }
    }
    o.ariza_etkin_adim = etkin.empty() ? 0U : *std::min_element(etkin.begin(), etkin.end());
    o.ariza_degistirdi_adim = degisti.empty() ? 0U : *std::min_element(degisti.begin(), degisti.end());
    o.disarm_sonrasi_sifir = disarm_goruldu && disarm_sonrasi_tamam;
    return o;
}

}  // namespace ida::zincir
