// SonarKumesi: kurulum doğrulaması, Ping1D ve Ping360 modelleri, zamanlama, arıza zinciri,
// teslim kuyruğu.
#include "ida/sonar/sonar_kumesi.hpp"

#include "demet.hpp"
#include "ida/sonar/akustik.hpp"
#include "vek.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <optional>
#include <string>

namespace ida::sonar {

namespace {

using ic::carp;
using ic::kPi;
using sensor::Akis;
using sensor::ArizaTuru;
using sensor::c2_kodla;
using sensor::duzgun_donusum;
using sensor::ZamanDeseniTuru;

constexpr double kDerece = kPi / 180.0;
constexpr std::int64_t kSonsuz = std::numeric_limits<std::int64_t>::max();
constexpr std::int64_t kAzamiSure = std::int64_t{1} << 53;    // sensör kümesiyle aynı sınır
constexpr std::int64_t kAzamiZaman = std::int64_t{1} << 62;   // IDA_TIME_MAX_NS (A4-K08)
constexpr std::size_t kKuyrukSiniri = 16;                     // sonar başına [T]
// Yük tamponu: en büyük satır (Ping360 1 200 örnek; Ping1D kutu_sayisi ≤ 200).
constexpr std::size_t kTamponBoyu = 1200;
// Sonar başına aynı anda havuz dışında bulunabilecek en çok kayıt: bekleyen 1 + kuyruk 16 +
// kullanıcının elindeki (boşaltılmış, henüz geri verilmemiş) 16, bir de pay.
constexpr std::size_t kSonarBasinaTampon = 2 * kKuyrukSiniri + 2;
constexpr double kHucreKenari = 0.01;                         // Δ_h, m (MP7 kuralı; A5 tam varsayılanı)

// ---------------------------------------------------------------- Ping360 modeli sabitleri
// Cihaz: 750 kHz, 2° × 25° (05 §2.4.5 [Ü]); deseni ayrılabilir sinc [T].
constexpr double kP360Yatay = 2.0 * kDerece;
constexpr double kP360Dusey = 25.0 * kDerece;
// Alıcı zinciri eşlemesi (05 §2.4.4 md. 6; A5 §8 md. 4: model sabitler, sürümüyle A6'ya yazılır).
// Değerler [T]: İDA-0 havuzunda (beton, μ = 0,02) TVG'li taban ve duvar dönüşleri 8 bitin
// ortasına düşecek biçimde seçildi; cihaz eğrisi bilinmiyor (10 §2.10).
constexpr double kP360Lmin = -95.0;    // dB
constexpr double kP360Lmaks = -15.0;   // dB
constexpr double kP360Kazanc[3] = {-6.0, 0.0, 6.0};   // gain 0/1/2, dB [T]
constexpr double kYakinAlanM = 1.0;    // yakın alan bandı (05 §2.4.4 md. 2: 0,75–1 m)
constexpr std::uint16_t kP360DarbeAzamiUs = 1000;   // ping-protocol Ping360 `transducer`: 1–1000 µs
constexpr int kP360BaslikBayti = 24;   // ping-protocol device_data: başlık 8 + alan 14 + sağlama 2 [T]

// ---------------------------------------------------------------- Ping1D modeli sabitleri
constexpr double kP1dFrekans = 115e3;  // Hz (05 §2.4.6 [Ü])
constexpr double kP1dGenislik = 25.0 * kDerece;
constexpr double kP1dGurultu = 1e-9;   // normalize gürültü tabanı Ĩ_n [T]
constexpr double kP1dSnrAsgariDb = 10.0;   // algılama için tepe SNR'si [K]
constexpr double kP1dEsikDb = -3.0;        // ilk kutu: yumuşatılmış TVG'li tepenin −3 dB'si [S]
constexpr std::size_t kP1dYumusatma = 2;   // ± kutu (5 kutuluk ortalama) [S]
constexpr double kP1dGuvenAltDb = 10.0;    // güven 0 → 100 eşlemesi [K]
constexpr double kP1dGuvenUstDb = 40.0;
constexpr double kP1dLmin = -95.0;     // profil u8 eşlemesi (TVG 20 log r) [T]: havuz tabanı tepesi ≈ 200
constexpr double kP1dLmaks = -25.0;
constexpr double kP1dAsgariMenzil = 0.5;   // otomatik pencere alt sınırı (m) [T]
constexpr double kP1dAzamiMenzil = 100.0;  // 05 §2.4.6

constexpr std::uint16_t kModelSurumu = 1;

std::int64_t ust_sinir(std::int64_t t, std::int64_t adim)
{
    return ((t + adim - 1) / adim) * adim;
}

std::int64_t ns(double s) { return static_cast<std::int64_t>(std::llround(s * 1e9)); }

bool sure_ok(std::int64_t v, bool sifir_olabilir)
{
    return (sifir_olabilir ? v >= 0 : v > 0) && v <= kAzamiSure;
}

bool ad_gecerli(const std::string& ad)
{
    if (ad.empty() || ad.size() > 32 || ad[0] < 'a' || ad[0] > 'z') return false;
    if (ad.size() >= 2 && ad[0] == 'x' && ad[1] == '_') return false;
    return std::all_of(ad.begin(), ad.end(), [](char c) {
        return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_';
    });
}

std::string ping360_denetle(const Ping360Yapilandirmasi& y)
{
    if (y.n_samples < 200 || y.n_samples > 1200) return "n_samples 200–1200 olmalı (A1 §3.4, A5-K78)";
    if (y.sample_period < 80 || y.sample_period > 40000) return "sample_period 80–40000 olmalı (05 §2.4.5)";
    if (y.transmit_frequency_khz < 500 || y.transmit_frequency_khz > 1000) {
        return "transmit_frequency_khz 500–1000 olmalı (05 §2.4.5)";
    }
    if (y.transmit_duration_us > kP360DarbeAzamiUs) {
        return "transmit_duration_us 0 (otomatik) ya da 1–1000 µs olmalı (ping-protocol Ping360 transducer)";
    }
    if (y.gain > 2) return "gain 0–2 olmalı";
    if (y.baslangic_grad > 399 || y.bitis_grad > 399) return "açılar 0–399 gradyan olmalı";
    if (y.adim_grad < 1 || y.adim_grad > 200) return "adim_grad 1–200 olmalı";
    if (!sure_ok(y.bekleme_ns, true)) return "bekleme_ns ≥ 0 olmalı (A5-K48)";
    return {};
}

std::string ping1d_denetle(const Ping1dYapilandirmasi& y)
{
    if (!std::isfinite(y.scan_start) || y.scan_start < 0.0) return "scan_start ≥ 0 olmalı";
    if (!std::isfinite(y.scan_length) || y.scan_length <= 0.0 ||
        y.scan_start + y.scan_length > kP1dAzamiMenzil) {
        return "scan_length > 0 ve scan_start + scan_length ≤ 100 m olmalı (05 §2.4.6)";
    }
    return {};
}

int grad_farki(int a, int b)
{
    const int d = std::abs(a - b) % 400;
    return std::min(d, 400 - d);
}

fizik::Mat3 rz(double a)
{
    const double c = std::cos(a), s = std::sin(a);
    return {{{c, -s, 0.0}, {s, c, 0.0}, {0.0, 0.0, 1.0}}};
}

// Ping1D kafa çerçevesi: kafa +x = sensör +z (ışın ekseni, A5-K89), kafa +y = sensör +y.
const fizik::Mat3 kP1dKafa{{{0.0, 0.0, -1.0}, {0.0, 1.0, 0.0}, {1.0, 0.0, 0.0}}};

}  // namespace

// ================================================================ iç yapı
struct SonarKumesi::Ic {
    struct Bekleyen {
        std::int64_t t_hazir = 0;
        SonarOlcumu kayit;
    };
    struct SonarDurumu {
        SonarAyari a;
        bool ping1d = false;
        Akis isin_akisi;
        Akis benek_akisi;     // sonar_hucre: sade kademede kutu başına benek (05 §2.4.3)
        Akis olcum_akisi;     // gürültü tabanı ve yakın alan bandı
        double alfa = 0.0;    // dB/m
        std::uint64_t ornek = 0;          // 64 bitlik ping/örnek sayacı (A5-K110 n)
        std::int64_t sonraki_tx = -1;     // -1: ilk adımda; adım sınırı
        // Cihaz zamanlaması (sol #6): kayma = 1 + ppm · 10⁻⁶. `t_ideal` son gönderimin cihazdaki
        // (yuvarlanmamış) anı; `t_ideal_sonraki` sonraki gönderiminki; adım sınırına en yakın
        // sınırla bağlanır. Gönderim önceki kaydın üretiminden önce olamazsa ideal an o sınıra
        // yeniden demirlenir.
        double kayma = 1.0;
        std::int64_t t_ideal = -1;
        std::int64_t t_ideal_sonraki = 0;
        std::int64_t son_sure = 0;        // son pingin gönderimden fiziksel bitişe süresi
        int aci_tx = 0;                   // son gönderimin açısı (kafa bir sonraki harekete dek burada)
        std::optional<Bekleyen> bekleyen;
        // Ping360
        Ping360Yapilandirmasi p360{};
        std::optional<Ping360Yapilandirmasi> p360_yeni;
        int aci = 0;
        int sektor_p = 0;
        int yon = 1;
        // Ping1D
        Ping1dYapilandirmasi p1d{};
        std::optional<Ping1dYapilandirmasi> p1d_yeni;
        // arıza zinciri
        bool son_var = false;
        SonarOlcumu son_cikti{};
        std::int64_t son_teslim = std::numeric_limits<std::int64_t>::min();
    };
    struct ArizaKaydi {
        ArizaTutamaci tutamac = 0;
        SensorArizasi t{};
        std::size_t sonar = 0;
        std::int64_t t_b = 0;
        std::int64_t t_bitis = kSonsuz;
        Akis akis;
        Akis gecis;
        std::vector<std::int64_t> ge_anlar;
        bool donma_bakildi = false;
        bool donma_var = false;
        SonarOlcumu donma{};
    };

    SonarKumesiAyari ayar{};
    std::shared_ptr<const Sahne> sahne;
    bool kuruldu = false;
    double c = 1500.0;
    std::vector<SonarDurumu> sonarlar;
    std::vector<SonarOlcumu> kuyruk;
    SonarSayaclari say{};
    std::vector<ArizaKaydi> arizalar;
    ArizaTutamaci sonraki_tutamac = 1;
    bool ilk = true;
    std::uint64_t son_adim = 0;
    std::int64_t t_son = -1;
    ic::Demet demet;
    ic::DemetSonucu ds;
    // Sabit bellek (W4-K27 (5); W4-bagla-sonar sol #5): ping hesabının çalışma tamponları ve kayıt
    // yük tamponlarının havuzu `kur`da ayrılır.
    std::vector<double> yog, tvg, yum;
    std::vector<std::vector<std::uint8_t>> havuz;

    std::vector<std::uint8_t> tampon_al()
    {
        if (!havuz.empty()) {
            std::vector<std::uint8_t> v = std::move(havuz.back());
            havuz.pop_back();
            return v;
        }
        ++say.tampon_yetersiz;
        std::vector<std::uint8_t> v;
        v.reserve(kTamponBoyu);
        return v;
    }
    // Kaydın yük tamponunu havuza geri koyar (kayıt yüksüz kalır).
    void tampon_ver(SonarOlcumu& r)
    {
        std::vector<std::uint8_t>& v = std::holds_alternative<Ping360Olcumu>(r.yuk)
                                           ? std::get<Ping360Olcumu>(r.yuk).data
                                           : std::get<Ping1dOlcumu>(r.yuk).profil;
        if (v.capacity() >= kTamponBoyu && havuz.size() < havuz.capacity()) {
            v.clear();
            havuz.push_back(std::move(v));
        }
    }
    // Ayrılmış yük tamponlu boş kayıt (kurulumda ve arm'da; döngüde değil).
    static SonarOlcumu bos_kayit(bool ping1d)
    {
        SonarOlcumu r;
        if (ping1d) {
            r.yuk = Ping1dOlcumu{};
            std::get<Ping1dOlcumu>(r.yuk).profil.reserve(kTamponBoyu);
        } else {
            r.yuk = Ping360Olcumu{};
            std::get<Ping360Olcumu>(r.yuk).data.reserve(kTamponBoyu);
        }
        return r;
    }

    static bool once(const OlcumBasligi& a, const OlcumBasligi& b)
    {
        if (a.t_teslim_ns != b.t_teslim_ns) return a.t_teslim_ns < b.t_teslim_ns;
        if (a.sensor_id != b.sensor_id) return a.sensor_id < b.sensor_id;
        if (a.inst != b.inst) return a.inst < b.inst;
        return a.seq < b.seq;
    }

    // ------------------------------------------------------------ zamanlama
    std::int64_t olcekle(const SonarDurumu& s, std::int64_t sure) const
    {
        return static_cast<std::int64_t>(std::llround(static_cast<double>(sure) * s.kayma));
    }
    // En yakın adım sınırı, yarıda yukarı (sensör kümesinin örnekleme kuralı; 05 §2.2).
    std::int64_t en_yakin_sinir(std::int64_t t) const
    {
        const std::int64_t h = ayar.adim_ns;
        return ((t + h / 2) / h) * h;
    }
    // Dönüştürücü su içinde mi (sol #1; 05 §4): NED z, yüzeyin altında (kesin büyük).
    bool suda(const ic::DemetGirdisi& g) const
    {
        return g.konum[2] > sahne->su_yuzeyi_z(g.konum[0], g.konum[1], g.t_ns);
    }
    // Su içindeyse ışın izler; havadaysa geometrik eko yok (ham enerji 0).
    bool izle(const ic::DemetGirdisi& g)
    {
        if (suda(g)) {
            demet.izle(g, ds);
            say.isin += ds.isin;
            return true;
        }
        ds.ham.assign(static_cast<std::size_t>(std::max(1, g.n_kutu)), 0.0);
        ds.isin = 0;
        ds.isabet = 0;
        ++say.havada;
        return false;
    }
    double frekans_sogurmasi(double f_hz) const
    {
        const auto& su = ayar.ortam.su;
        return sogurma_db_m(f_hz, su.sicaklik_c, su.tuzluluk, 0.0, su.ph);
    }
    std::int64_t ping360_dinleme(const Ping360Yapilandirmasi& y) const
    {
        return static_cast<std::int64_t>(y.n_samples) * y.sample_period * 25;
    }
    std::int64_t ping360_iletim(const SonarDurumu& s, const Ping360Yapilandirmasi& y) const
    {
        const auto& p = std::get<Ping360Ayar>(s.a.ayar);
        if (!p.baud) return 0;
        const double bayt = static_cast<double>(kP360BaslikBayti + y.n_samples);
        return ns(bayt * 10.0 / static_cast<double>(*p.baud));
    }
    std::int64_t motor(const SonarDurumu& s, int fark) const
    {
        const auto& p = std::get<Ping360Ayar>(s.a.ayar);
        return fark == 0 ? 0 : p.t_m0_ns + fark * p.t_grad_ns;
    }
    // Sonraki açı (05 §2.4.5 OTOMATİK): tam turda sarar, sektörde uçlarda yön değiştirir.
    int sonraki_aci(SonarDurumu& s) const
    {
        const Ping360Yapilandirmasi& y = s.p360;
        if (y.baslangic_grad == y.bitis_grad) {
            return (s.aci + y.adim_grad) % 400;
        }
        const int l = (static_cast<int>(y.bitis_grad) - static_cast<int>(y.baslangic_grad) + 400) % 400;
        int p = s.sektor_p + s.yon * static_cast<int>(y.adim_grad);
        // Uca varınca orada durur ve yön değiştirir [T]: 0, 4, 8, 10, 6, 2, 0, 4, …
        if (p >= l) {
            p = l;
            s.yon = -1;
        } else if (p <= 0) {
            p = 0;
            s.yon = 1;
        }
        s.sektor_p = p;
        return (static_cast<int>(y.baslangic_grad) + p) % 400;
    }

    // ------------------------------------------------------------ alıcı zinciri yardımcıları
    // Benek (05 §2.4.3 sade: kutu başına ortalaması 1 olan üstel çarpan; anahtar (ping, kutu)) ve
    // gürültü tabanı (05 §2.4.4 md. 1–2). Çekiliş yerleri: n = ping sayacı, c2 = (0, bileşen, kutu).
    void benek_ve_gurultu(const SonarDurumu& s, std::uint64_t n, const std::vector<double>& ham,
                          double gurultu, double yakin, double tau0, double t_s,
                          std::vector<double>& cikti) const
    {
        cikti.resize(ham.size());
        for (std::size_t i = 0; i < ham.size(); ++i) {
            const std::uint32_t k = static_cast<std::uint32_t>(i);
            double x = ham[i] > 0.0 ? ham[i] * s.benek_akisi.ustel(n, c2_kodla(0, 0, k)) : 0.0;
            if (gurultu > 0.0) {
                x += gurultu * s.olcum_akisi.ustel(n, c2_kodla(0, 0, k));
            }
            const double r = 0.5 * c * (tau0 + (static_cast<double>(i) + 0.5) * t_s);
            if (yakin > 0.0 && r < kYakinAlanM) {
                x += yakin * s.olcum_akisi.ustel(n, c2_kodla(0, 1, k));
            }
            cikti[i] = x;
        }
    }

    // ------------------------------------------------------------ ping
    void dontur(const fizik::DurumAnlik& d, const SonarDurumu& s, const fizik::Mat3& kafa,
                ic::DemetGirdisi& g) const
    {
        const fizik::Mat3 r_nb = ic::matris(d.yonelim);
        const fizik::Mat3 r_bs = ic::matris(s.a.montaj.yonelim);
        g.konum = ic::topla(d.konum_n, carp(r_nb, s.a.montaj.konum));
        g.r_nh = carp(carp(r_nb, r_bs), kafa);
    }

    void ping360_at(SonarDurumu& s, const fizik::DurumAnlik& d, std::int64_t t)
    {
        if (s.p360_yeni) {
            s.p360 = *s.p360_yeni;
            s.p360_yeni.reset();
            s.aci = s.p360.baslangic_grad;
            s.sektor_p = 0;
            s.yon = 1;
            // Soğurma yeni frekanstan; A5'ten açık sabit verilmişse o korunur (sol #3).
            if (!std::get<Ping360Ayar>(s.a.ayar).sogurma) {
                s.alfa = frekans_sogurmasi(1e3 * static_cast<double>(s.p360.transmit_frequency_khz));
            }
        }
        s.aci_tx = s.aci;
        const Ping360Yapilandirmasi& y = s.p360;
        const auto& p = std::get<Ping360Ayar>(s.a.ayar);
        const double t_s = static_cast<double>(y.sample_period) * 25e-9;
        const std::uint16_t tx_us = ping360_darbe_us(y);
        ic::DemetGirdisi g;
        g.sahne = sahne.get();
        g.t_ns = t;
        dontur(d, s, rz(2.0 * kPi * static_cast<double>(s.aci) / 400.0), g);
        g.desen = ic::Desen::tam_genislikten(kP360Yatay, kP360Dusey);
        g.isin_sayisi = ayar.sade_isin_sayisi;
        g.ping_no = s.ornek;
        g.isin_akisi = &s.isin_akisi;
        g.c = c;
        g.alfa_db_m = s.alfa;
        g.hucre_kenari = kHucreKenari;
        g.tau0 = 0.0;
        g.t_s = t_s;
        g.n_kutu = y.n_samples;
        g.darbe_s = static_cast<double>(tx_us) * 1e-6;
        izle(g);

        benek_ve_gurultu(s, s.ornek, ds.ham, p.gurultu_tabani, p.yakin_alan_genligi, 0.0, t_s, yog);
        Ping360Olcumu o;
        o.angle_grad = static_cast<std::uint16_t>(s.aci);
        o.n_samples = y.n_samples;
        o.sample_period = y.sample_period;
        o.tx_duration_us = tx_us;
        o.tx_freq_khz = y.transmit_frequency_khz;
        o.gain = y.gain;
        o.ses_hizi_mps = static_cast<float>(c);
        o.data = tampon_al();
        o.data.assign(y.n_samples, 0);
        bool doymus = false;
        const double gk = kP360Kazanc[std::min<std::size_t>(y.gain, 2u)];
        for (std::size_t i = 0; i < yog.size(); ++i) {
            // 05 §2.4.4 md. 4–6: L = 10 log Ĩ; TVG(r) = a log r + 2 α r; kazanç; eşleme.
            const double r = 0.5 * c * (static_cast<double>(i) + 0.5) * t_s;
            const double L = yog[i] < 1e-15 ? -std::numeric_limits<double>::infinity()
                                             : 10.0 * std::log10(yog[i]) + p.tvg_a * std::log10(r) +
                                                   2.0 * s.alfa * r + gk;
            const double q = std::round((L - kP360Lmin) * 255.0 / (kP360Lmaks - kP360Lmin));
            doymus = doymus || q > 255.0;
            o.data[i] = static_cast<std::uint8_t>(std::clamp(q, 0.0, 255.0));
        }
        if (ayar.tani) {
            o.tani.ham = ds.ham;
            o.tani.yogunluk = yog;
            o.tani.isin = ds.isin;
            o.tani.isabet = ds.isabet;
        }
        const std::int64_t sure = olcekle(s, ping360_dinleme(y) + p.islem_suresi_ns + ping360_iletim(s, y));
        const std::int64_t bitis = t + sure;
        SonarOlcumu k;
        k.hdr = baslik(s, t, t, bitis);
        if (doymus) k.hdr.flags = static_cast<std::uint16_t>(k.hdr.flags | sensor::kBayrakDoymus);
        k.yuk = std::move(o);
        s.bekleyen = Bekleyen{std::max(ust_sinir(bitis, ayar.adim_ns), t + ayar.adim_ns), std::move(k)};
        s.son_sure = sure;
        s.aci = sonraki_aci(s);
        s.t_ideal_sonraki = s.t_ideal + sure + olcekle(s, y.bekleme_ns + motor(s, grad_farki(s.aci_tx, s.aci)));
        s.sonraki_tx = en_yakin_sinir(s.t_ideal_sonraki);
    }

    void ping1d_at(SonarDurumu& s, const fizik::DurumAnlik& d, std::int64_t t)
    {
        if (s.p1d_yeni) {
            s.p1d = *s.p1d_yeni;
            s.p1d_yeni.reset();
        }
        const auto& p = std::get<Ping1dAyar>(s.a.ayar);
        const Ping1dYapilandirmasi w = s.p1d;
        const int nk = p.kutu_sayisi;
        const double dr = w.scan_length / static_cast<double>(nk);
        const double tau0 = 2.0 * w.scan_start / c;
        const double t_s = 2.0 * dr / c;
        ic::DemetGirdisi g;
        g.sahne = sahne.get();
        g.t_ns = t;
        dontur(d, s, kP1dKafa, g);
        g.desen = ic::Desen::tam_genislikten(kP1dGenislik, kP1dGenislik);
        g.isin_sayisi = ayar.sade_isin_sayisi;
        g.ping_no = s.ornek;
        g.isin_akisi = &s.isin_akisi;
        g.c = c;
        g.alfa_db_m = s.alfa;
        g.hucre_kenari = kHucreKenari;
        g.tau0 = tau0;
        g.t_s = t_s;
        g.n_kutu = nk;
        g.darbe_s = t_s;   // darbe ≈ bir kutu [T]
        const bool sudaki = izle(g);

        benek_ve_gurultu(s, s.ornek, ds.ham, kP1dGurultu, 0.0, tau0, t_s, yog);
        // Algılama [S] (cihaz algoritması belgelenmemiş; 05 §2.4.6): TVG'li (20 log r + 2αr)
        // profil 5 kutuluk ortalamayla yumuşatılır (benek); yumuşatılmış ham profilin tepe SNR'si
        // ≥ 10 dB ise yumuşatılmış TVG'li tepenin −3 dB'sini aşan ilk kutunun merkezi mesafedir.
        // Basamak biçimli bir dönüşte 5 kutuluk ortalama tam basamak kutusunda yarıyı aşar.
        tvg.resize(yog.size());
        for (std::size_t i = 0; i < yog.size(); ++i) {
            const double r = w.scan_start + (static_cast<double>(i) + 0.5) * dr;
            tvg[i] = yog[i] * r * r * std::pow(10.0, 2.0 * s.alfa * r / 10.0);
        }
        yum.resize(yog.size());
        double tepe = 0.0, snr_tepe = 0.0;
        for (std::size_t i = 0; i < yog.size(); ++i) {
            const std::size_t lo = i >= kP1dYumusatma ? i - kP1dYumusatma : 0u;
            const std::size_t hi = std::min(yog.size() - 1, i + kP1dYumusatma);
            double st = 0.0, sy = 0.0;
            for (std::size_t k = lo; k <= hi; ++k) {
                st += tvg[k];
                sy += yog[k];
            }
            const double nkat = static_cast<double>(2 * kP1dYumusatma + 1);
            yum[i] = st / nkat;
            tepe = std::max(tepe, yum[i]);
            snr_tepe = std::max(snr_tepe, sy / nkat / kP1dGurultu);
        }
        Ping1dOlcumu o;
        o.scan_start = static_cast<float>(w.scan_start);
        o.scan_length = static_cast<float>(w.scan_length);
        o.ping_number = static_cast<std::uint32_t>(s.ornek & 0xFFFFFFFFu);
        o.profil = tampon_al();
        o.profil.assign(static_cast<std::size_t>(nk), 0);
        for (std::size_t i = 0; i < yog.size(); ++i) {
            const double L = tvg[i] < 1e-15 ? -std::numeric_limits<double>::infinity()
                                            : 10.0 * std::log10(tvg[i]);
            const double q = std::round((L - kP1dLmin) * 255.0 / (kP1dLmaks - kP1dLmin));
            o.profil[i] = static_cast<std::uint8_t>(std::clamp(q, 0.0, 255.0));
        }
        const double snr_db = snr_tepe > 0.0 ? 10.0 * std::log10(snr_tepe) : -1e9;
        bool var = false;
        double mesafe = 0.0;
        if (sudaki && snr_db >= kP1dSnrAsgariDb && tepe > 0.0) {
            const double esik = tepe * std::pow(10.0, kP1dEsikDb / 10.0);
            for (std::size_t i = 0; i < yum.size(); ++i) {
                if (yum[i] >= esik) {
                    mesafe = w.scan_start + (static_cast<double>(i) + 0.5) * dr;
                    var = true;
                    break;
                }
            }
        }
        std::int64_t t_meas = t;
        if (var) {
            o.distance = static_cast<float>(mesafe);
            const std::int64_t ofs = ns(mesafe / c);   // yansıma anı t_tx + t_eko/2, t_eko = 2d/c
            o.tx_offset_ns = static_cast<std::uint32_t>(ofs);
            t_meas = t + ofs;
            const double g01 = (snr_db - kP1dGuvenAltDb) / (kP1dGuvenUstDb - kP1dGuvenAltDb);
            o.confidence = static_cast<std::uint8_t>(std::lround(100.0 * std::clamp(g01, 0.0, 1.0)));
        }
        if (ayar.tani) {
            o.tani.ham = ds.ham;
            o.tani.yogunluk = yog;
            o.tani.isin = ds.isin;
            o.tani.isabet = ds.isabet;
        }
        const std::int64_t dinleme = ns(2.0 * (w.scan_start + w.scan_length) / c);
        const std::int64_t sure = olcekle(s, dinleme + p.islem_suresi_ns);
        const std::int64_t bitis = t + sure;
        SonarOlcumu k;
        k.hdr = baslik(s, t_meas, t, bitis);
        if (!var) k.hdr.flags = 0;
        k.yuk = std::move(o);
        s.bekleyen = Bekleyen{std::max(ust_sinir(bitis, ayar.adim_ns), t + ayar.adim_ns), std::move(k)};
        s.son_sure = sure;
        s.t_ideal_sonraki = s.t_ideal + olcekle(s, std::max(p.ping_araligi_ns, dinleme + p.islem_suresi_ns));
        s.sonraki_tx = en_yakin_sinir(s.t_ideal_sonraki);
        // Otomatik menzil (05 §2.4.6 [T]): pencere son mesafenin `oto_menzil_carpani` katı;
        // eko yoksa pencere iki katına çıkar.
        if (s.p1d.otomatik && !s.p1d_yeni) {
            double l = var ? p.oto_menzil_carpani * mesafe : 2.0 * s.p1d.scan_length;
            // Kalan menzil (> 0: pencere doğrulanmıştır) asgari uzunluktan kısaysa asgari de
            // ona iner; her durumda lo ≤ hi (sol #2).
            const double hi = kP1dAzamiMenzil - s.p1d.scan_start;
            l = std::clamp(l, std::min(kP1dAsgariMenzil, hi), hi);
            s.p1d.scan_length = l;
        }
    }

    OlcumBasligi baslik(const SonarDurumu& s, std::int64_t t_meas, std::int64_t t_tx,
                        std::int64_t bitis) const
    {
        OlcumBasligi h;
        h.sensor_id = s.a.kimlik;
        h.tur = s.a.tur;
        h.inst = s.a.inst;
        h.seq = static_cast<std::uint32_t>(s.ornek & 0xFFFFFFFFu);
        h.t_meas_ns = t_meas;
        h.window_ns = 0;
        h.t_tx_ns = t_tx;
        h.t_teslim_ns = bitis;
        h.flags = sensor::kBayrakGecerli;
        h.model_surumu = kModelSurumu;
        return h;
    }

    // ------------------------------------------------------------ arıza zinciri
    bool ge_kotu(ArizaKaydi& f, std::int64_t t)
    {
        if (f.ge_anlar.empty()) f.ge_anlar.push_back(f.t_b);
        while (f.ge_anlar.back() <= t) {
            const std::size_t i = f.ge_anlar.size() - 1;
            const double m = static_cast<double>((i % 2 == 0) ? f.t.zaman_deseni.t_iyi_ns
                                                              : f.t.zaman_deseni.t_kotu_ns);
            const double u = duzgun_donusum(f.gecis.sozcukler(i, 0)[0]);
            const double sure = std::ceil(-std::log(u) * m);
            f.ge_anlar.push_back(f.ge_anlar.back() + std::max<std::int64_t>(1, static_cast<std::int64_t>(sure)));
        }
        const auto it = std::upper_bound(f.ge_anlar.begin(), f.ge_anlar.end(), t);
        const std::size_t i = static_cast<std::size_t>(it - f.ge_anlar.begin()) - 1;
        return (i % 2) == 1;
    }

    bool desen_etkin(ArizaKaydi& f, const SonarOlcumu& r, std::int64_t t_temel)
    {
        switch (f.t.zaman_deseni.tur) {
        case ZamanDeseniTuru::surekli: return true;
        case ZamanDeseniTuru::bernoulli:
            return f.akis.duzgun(r.hdr.seq, c2_kodla(0xFF, 0, 0)) < f.t.zaman_deseni.p;
        case ZamanDeseniTuru::gilbert_elliott: return ge_kotu(f, t_temel);
        }
        return false;
    }

    // Sensör kümesinin zinciriyle aynı sıra ve anlam (05 §2.7; A5 Tablo 3.10-A). false: teslim yok.
    bool ariza_zinciri(std::size_t si, SonarOlcumu& r)
    {
        SonarDurumu& s = sonarlar[si];
        const std::int64_t t_temel = r.hdr.t_meas_ns;
        bool teslim = true;
        bool sira_boz = false;
        auto uygula = [&](ArizaTuru tur, auto&& islem) {
            for (ArizaKaydi& f : arizalar) {
                if (f.sonar != si || f.t.tur != tur) continue;
                if (!(f.t_b <= t_temel && t_temel < f.t_bitis)) continue;
                if ((tur == ArizaTuru::donma || tur == ArizaTuru::donma_zaman_damgasi) && !f.donma_bakildi) {
                    f.donma_bakildi = true;
                    f.donma_var = s.son_var;
                    if (s.son_var) {
                        f.donma = s.son_cikti;
                    } else {
                        ++say.donma_onceki_cikti_yok;
                    }
                }
                if (!desen_etkin(f, r, t_temel)) continue;
                islem(f);
                r.hdr.ariza_maskesi |= 1u << static_cast<unsigned>(tur);
            }
        };
        auto dondur = [&](ArizaKaydi& f, bool damga) {
            if (!f.donma_var) {
                teslim = false;
                return;
            }
            const OlcumBasligi h = r.hdr;
            r = f.donma;
            r.hdr.seq = h.seq;
            r.hdr.t_hazir_ns = h.t_hazir_ns;
            r.hdr.t_teslim_ns = h.t_teslim_ns;
            r.hdr.ariza_maskesi = h.ariza_maskesi;
            if (!damga) {
                r.hdr.t_meas_ns = h.t_meas_ns;
                r.hdr.window_ns = h.window_ns;
                r.hdr.t_tx_ns = h.t_tx_ns;
            }
        };
        uygula(ArizaTuru::donma, [&](ArizaKaydi& f) { dondur(f, false); });
        uygula(ArizaTuru::donma_zaman_damgasi, [&](ArizaKaydi& f) { dondur(f, true); });
        uygula(ArizaTuru::yanlis_gecerlilik, [&](ArizaKaydi&) {
            r.hdr.flags = static_cast<std::uint16_t>(r.hdr.flags ^ sensor::kBayrakGecerli);
        });
        uygula(ArizaTuru::kendini_bildiren, [&](ArizaKaydi&) {
            r.hdr.flags = static_cast<std::uint16_t>(r.hdr.flags | sensor::kBayrakSensorArizasi);
        });
        if (teslim) {
            s.son_var = true;
            s.son_cikti = r;
        }
        uygula(ArizaTuru::kayip, [&](ArizaKaydi&) {
            if (teslim) ++say.kayip;
            teslim = false;
        });
        uygula(ArizaTuru::gec_teslim, [&](ArizaKaydi& f) {
            const std::int64_t j_s = f.t.siddet.titresim_ns;
            std::int64_t j = 0;
            if (j_s > 0) {
                const double u = duzgun_donusum(f.akis.sozcukler(r.hdr.seq, c2_kodla(0, 2, 0))[0]);
                j = static_cast<std::int64_t>(std::floor(u * static_cast<double>(2 * j_s + 1))) - j_s;
            }
            r.hdr.t_teslim_ns += f.t.siddet.gecikme_ns + j;
            sira_boz = sira_boz || f.t.sira_boz;
        });
        if (teslim) {
            if (!sira_boz) r.hdr.t_teslim_ns = std::max(r.hdr.t_teslim_ns, s.son_teslim);
            r.hdr.t_teslim_ns = std::max(r.hdr.t_teslim_ns, r.hdr.t_hazir_ns);   // W4-K9
            s.son_teslim = r.hdr.t_teslim_ns;
        }
        return teslim;
    }

    void kuyruga(SonarOlcumu&& o)
    {
        ++say.uretilen;
        std::size_t ayni = 0;
        for (const SonarOlcumu& q : kuyruk) ayni += (q.hdr.sensor_id == o.hdr.sensor_id) ? 1u : 0u;
        if (ayni >= kKuyrukSiniri) {
            const auto it = std::find_if(kuyruk.begin(), kuyruk.end(), [&](const SonarOlcumu& q) {
                return q.hdr.sensor_id == o.hdr.sensor_id;
            });
            tampon_ver(*it);
            kuyruk.erase(it);
            ++say.kuyruk_tasma;
        }
        const auto yer = std::upper_bound(kuyruk.begin(), kuyruk.end(), o, [](const SonarOlcumu& a, const SonarOlcumu& b) {
            return once(a.hdr, b.hdr);
        });
        kuyruk.insert(yer, std::move(o));
    }
};

// ================================================================ arayüz
std::uint16_t ping360_darbe_us(const Ping360Yapilandirmasi& y)
{
    if (y.transmit_duration_us != 0) return y.transmit_duration_us;
    const double t_s = static_cast<double>(y.sample_period) * 25e-9;
    const long us = std::lround(4.0 * t_s * 1e6);
    return static_cast<std::uint16_t>(std::clamp<long>(us, 1L, static_cast<long>(kP360DarbeAzamiUs)));
}

SonarKumesi::SonarKumesi() : ic_(std::make_unique<Ic>()) {}
SonarKumesi::~SonarKumesi() = default;
SonarKumesi::SonarKumesi(SonarKumesi&&) noexcept = default;
SonarKumesi& SonarKumesi::operator=(SonarKumesi&&) noexcept = default;

KurulumSonucu SonarKumesi::kur(const SonarKumesiAyari& ayar, std::shared_ptr<const Sahne> sahne)
{
    ic_ = std::make_unique<Ic>();
    KurulumSonucu s;
    if (!sahne) {
        s.hata = "sahne yok";
        return s;
    }
    if (ayar.kademe != SonarKademesi::sade) {
        s.hata = "kalite.sonar = tam bu dilimde desteklenmiyor (W4-sonar kapsamı: sade kademe)";
        return s;
    }
    if (ayar.adim_ns <= 0 || ayar.adim_ns > kAzamiSure) {
        s.hata = "adim_ns > 0 olmalı";
        return s;
    }
    if (ayar.sade_isin_sayisi < 1 || ayar.sade_isin_sayisi > 10'000'000) {
        s.hata = "ışın sayısı 1 … 10⁷ olmalı (A5 §3.12)";
        return s;
    }
    const auto& su = ayar.ortam.su;
    if (!std::isfinite(su.sicaklik_c) || su.sicaklik_c < -2.0 || su.sicaklik_c > 40.0 ||
        !std::isfinite(su.tuzluluk) || su.tuzluluk < 0.0 || su.tuzluluk > 0.05 || !std::isfinite(su.ph) ||
        su.ph < 6.0 || su.ph > 9.0 || !(su.yogunluk >= 900.0 && su.yogunluk <= 1100.0)) {
        s.hata = "ortam.su alanı A5 §3.9 aralığı dışında";
        return s;
    }
    if (su.ses_hizi && !(*su.ses_hizi >= 1300.0 && *su.ses_hizi <= 1700.0)) {
        s.hata = "ortam.su.ses_hizi [1300, 1700] m/s dışında (A5 §3.9)";
        return s;
    }
    if (ayar.sonarlar.size() > 64) {
        s.hata = "arac.sensorler: en çok 64 sensör (A5 §3.6)";
        return s;
    }
    const SesHizi sh = ortam_ses_hizi(su.sicaklik_c, su.tuzluluk, su.ses_hizi, 0.0, su.yogunluk,
                                      ayar.ortam.yercekimi);
    if (sh.dislama) {
        s.uyarilar.emplace_back(ses_hizi_uyarisi(sh));   // sol #8: reddedilmez, bildirilir
    }
    for (std::size_t i = 0; i < ayar.sonarlar.size(); ++i) {
        const SonarAyari& a = ayar.sonarlar[i];
        const std::string on = "arac.sensorler." + a.ad + ": ";
        if (!ad_gecerli(a.ad)) {
            s.hata = "arac.sensorler: geçersiz sensör adı '" + a.ad + "' (A5 §3.6)";
            return s;
        }
        for (std::size_t j = 0; j < i; ++j) {
            const SonarAyari& b = ayar.sonarlar[j];
            if (b.ad == a.ad || b.kimlik == a.kimlik || (b.tur == a.tur && b.inst == a.inst)) {
                s.hata = on + "ad, kimlik ya da (tur, inst) tekil değil (A5 §3.6)";
                return s;
            }
        }
        const bool p1 = a.tur == SensorTuru::ping1d;
        if (!p1 && a.tur != SensorTuru::ping360) {
            s.hata = on + "tur ping1d ya da ping360 olmalı";
            return s;
        }
        if (p1 != std::holds_alternative<Ping1dAyar>(a.ayar) ||
            p1 != std::holds_alternative<Ping1dYapilandirmasi>(a.yapilandirma) ||
            a.profil != (p1 ? "ping1d" : "ping360")) {
            s.hata = on + "tur, profil, ayar ve yapılandırma uyuşmuyor";
            return s;
        }
        if (!a.etkin) continue;
        if (!std::isfinite(a.saat_kaymasi_ppm) || std::abs(a.saat_kaymasi_ppm) >= 1e5) {
            s.hata = on + "saat_kaymasi_ppm geçersiz";
            return s;
        }
        if (a.inst > 3) {
            s.hata = on + "inst 0–3 olmalı (A4-K30, A1 §3.4)";
            return s;
        }
        const auto& q = a.montaj.yonelim;
        const double qn = std::sqrt(q[0] * q[0] + q[1] * q[1] + q[2] * q[2] + q[3] * q[3]);
        if (!(std::abs(qn - 1.0) <= 1e-6) || !ic::sonlu(a.montaj.konum)) {
            s.hata = on + "montaj.yonelim birim kuaterniyon değil ya da konum sonlu değil (A5-K32)";
            return s;
        }
        std::string h;
        if (p1) {
            const auto& p = std::get<Ping1dAyar>(a.ayar);
            if (p.kutu_sayisi < 1 || p.kutu_sayisi > 200) h = "kutu_sayisi 1–200 olmalı (A3 yuk_ping1d)";
            else if (!sure_ok(p.ping_araligi_ns, false) || !sure_ok(p.islem_suresi_ns, true))
                h = "ping_araligi_ns > 0, islem_suresi_ns ≥ 0 olmalı (A5-K48)";
            else if (!(p.oto_menzil_carpani > 0.0) || !std::isfinite(p.oto_menzil_carpani))
                h = "oto_menzil_carpani > 0 olmalı";
            else if (p.sogurma && !(*p.sogurma >= 0.0 && std::isfinite(*p.sogurma)))
                h = "sogurma ≥ 0 olmalı";
            else
                h = ping1d_denetle(std::get<Ping1dYapilandirmasi>(a.yapilandirma));
        } else {
            const auto& p = std::get<Ping360Ayar>(a.ayar);
            if (p.baud && *p.baud <= 0) h = "baud > 0 olmalı";
            else if (!sure_ok(p.t_m0_ns, true) || !sure_ok(p.t_grad_ns, true) || !sure_ok(p.islem_suresi_ns, true))
                h = "t_m0_ns, t_grad_ns, islem_suresi_ns ≥ 0 olmalı (A5-K48)";
            else if (!(p.gurultu_tabani >= 0.0) || !std::isfinite(p.gurultu_tabani) ||
                     !(p.yakin_alan_genligi >= 0.0) || !std::isfinite(p.yakin_alan_genligi) ||
                     !std::isfinite(p.tvg_a))
                h = "gurultu_tabani, yakin_alan_genligi ≥ 0 ve tvg_a sonlu olmalı";
            else if (p.sogurma && !(*p.sogurma >= 0.0 && std::isfinite(*p.sogurma)))
                h = "sogurma ≥ 0 olmalı";
            else
                h = ping360_denetle(std::get<Ping360Yapilandirmasi>(a.yapilandirma));
        }
        if (!h.empty()) {
            s.hata = on + h;
            return s;
        }
    }
    Ic& c = *ic_;
    c.ayar = ayar;
    c.sahne = std::move(sahne);
    c.c = sh.c;
    for (const SonarAyari& a : ayar.sonarlar) {
        if (!a.etkin) continue;
        Ic::SonarDurumu d;
        d.a = a;
        d.a.montaj.yonelim = ic::normalle(a.montaj.yonelim);
        d.ping1d = a.tur == SensorTuru::ping1d;
        d.kayma = 1.0 + a.saat_kaymasi_ppm * 1e-6;
        d.isin_akisi = Akis(ayar.ana_tohum, "sensor/" + a.ad + "/sonar_isin");
        d.benek_akisi = Akis(ayar.ana_tohum, "sensor/" + a.ad + "/sonar_hucre");
        d.olcum_akisi = Akis(ayar.ana_tohum, "sensor/" + a.ad + "/olcum");
        if (d.ping1d) {
            const auto& p = std::get<Ping1dAyar>(a.ayar);
            d.p1d = std::get<Ping1dYapilandirmasi>(a.yapilandirma);
            d.alfa = p.sogurma ? *p.sogurma : sogurma_db_m(kP1dFrekans, su.sicaklik_c, su.tuzluluk, 0.0, su.ph);
        } else {
            const auto& p = std::get<Ping360Ayar>(a.ayar);
            d.p360 = std::get<Ping360Yapilandirmasi>(a.yapilandirma);
            d.aci = d.p360.baslangic_grad;
            d.aci_tx = d.aci;
            d.alfa = p.sogurma ? *p.sogurma
                               : sogurma_db_m(1e3 * d.p360.transmit_frequency_khz, su.sicaklik_c, su.tuzluluk,
                                              0.0, su.ph);
        }
        d.son_cikti = Ic::bos_kayit(d.ping1d);   // arıza zinciri kopyası ayırmasız (W4-K27 (5))
        c.sonarlar.push_back(std::move(d));
    }
    // Sabit bellek (W4-K27 (5); W4-bagla-sonar sol #5): kuyruk, yük tamponu havuzu, ping hesabının
    // çalışma tamponları ve ışın demetinin tamponları kurulumda ayrılır.
    const std::size_t n_sonar = c.sonarlar.size();
    c.kuyruk.reserve(n_sonar * kKuyrukSiniri);
    c.havuz.reserve(n_sonar * kSonarBasinaTampon);
    for (std::size_t i = 0; i < n_sonar * kSonarBasinaTampon; ++i) {
        std::vector<std::uint8_t> v;
        v.reserve(kTamponBoyu);
        c.havuz.push_back(std::move(v));
    }
    c.yog.reserve(kTamponBoyu);
    c.tvg.reserve(kTamponBoyu);
    c.yum.reserve(kTamponBoyu);
    for (const Ic::SonarDurumu& d : c.sonarlar) {
        ic::DemetGirdisi g;
        g.isin_sayisi = ayar.sade_isin_sayisi;
        if (d.ping1d) {
            // Ping1D: darbe bir kutu (çekirdek boyu pencereden bağımsız), kutu sayısı sabit.
            g.desen = ic::Desen::tam_genislikten(kP1dGenislik, kP1dGenislik);
            g.n_kutu = std::get<Ping1dAyar>(d.a.ayar).kutu_sayisi;
            g.t_s = 1e-5;
            g.darbe_s = g.t_s;
        } else {
            const Ping360Yapilandirmasi& y = d.p360;
            g.desen = ic::Desen::tam_genislikten(kP360Yatay, kP360Dusey);
            g.n_kutu = y.n_samples;
            g.t_s = static_cast<double>(y.sample_period) * 25e-9;
            g.darbe_s = static_cast<double>(ping360_darbe_us(y)) * 1e-6;
        }
        c.demet.hazirla(g, c.ds);
    }
    c.kuruldu = true;
    s.tamam = true;
    return s;
}

AdimSonucu SonarKumesi::adim(const fizik::DurumAnlik& d, std::uint64_t adim_no)
{
    Ic& c = *ic_;
    if (!c.kuruldu) return AdimSonucu::kurulmadi;
    if (d.adim_no != adim_no || adim_no > static_cast<std::uint64_t>(kAzamiZaman / c.ayar.adim_ns) ||
        d.t_ns != static_cast<std::int64_t>(adim_no) * c.ayar.adim_ns) {
        return AdimSonucu::tutarsiz;
    }
    if (!c.ilk && adim_no != c.son_adim + 1) return AdimSonucu::sira_disi;
    c.ilk = false;
    c.son_adim = adim_no;
    c.t_son = d.t_ns;
    const std::int64_t t = d.t_ns;
    for (std::size_t si = 0; si < c.sonarlar.size(); ++si) {
        Ic::SonarDurumu& s = c.sonarlar[si];
        if (s.bekleyen && s.bekleyen->t_hazir <= t) {
            SonarOlcumu k = std::move(s.bekleyen->kayit);
            s.bekleyen.reset();
            k.hdr.t_hazir_ns = t;
            k.hdr.t_teslim_ns = std::max(k.hdr.t_teslim_ns, t);   // W4-K9
            if (c.ariza_zinciri(si, k)) {
                c.kuyruga(std::move(k));
            } else {
                c.tampon_ver(k);   // teslim edilmeyen kaydın tamponu havuza (W4-K27 (5))
            }
        }
        if (s.sonraki_tx < 0) s.sonraki_tx = t;
        if (!s.bekleyen && t >= s.sonraki_tx) {
            s.t_ideal = (s.t_ideal >= 0 && t == s.sonraki_tx) ? s.t_ideal_sonraki : t;
            if (s.ping1d) {
                c.ping1d_at(s, d, t);
            } else {
                c.ping360_at(s, d, t);
            }
            ++s.ornek;
            ++c.say.ping;
        }
    }
    return AdimSonucu::tamam;
}

std::size_t SonarKumesi::bekleyen() const { return ic_->kuyruk.size(); }

bool SonarKumesi::siradaki(std::int64_t t_kadar_ns, SonarOlcumu& cikti)
{
    auto& q = ic_->kuyruk;
    if (q.empty() || q.front().hdr.t_teslim_ns > t_kadar_ns) return false;
    cikti = std::move(q.front());
    q.erase(q.begin());
    return true;
}

void SonarKumesi::bosalt(std::int64_t t_kadar_ns, std::vector<SonarOlcumu>& cikti)
{
    SonarOlcumu o;
    while (siradaki(t_kadar_ns, o)) cikti.push_back(std::move(o));
}

std::size_t SonarKumesi::bosalt(std::int64_t t_kadar_ns, std::vector<SonarOlcumu>& cikti, std::size_t azami)
{
    Ic& c = *ic_;
    auto& q = c.kuyruk;
    std::size_t n = 0;
    std::size_t dusen = 0;
    while (n < q.size() && q[n].hdr.t_teslim_ns <= t_kadar_ns) {
        if (cikti.size() < azami) {
            cikti.push_back(std::move(q[n]));
        } else {
            c.tampon_ver(q[n]);
            ++dusen;
        }
        ++n;
    }
    // Baştaki n kayıt taşındı (yüksüz); kalanlar yüksüz yerlere kayar: ayırma ve bırakma yok.
    q.erase(q.begin(), q.begin() + static_cast<std::ptrdiff_t>(n));
    return dusen;
}

void SonarKumesi::geri_al(std::vector<SonarOlcumu>& v)
{
    for (SonarOlcumu& r : v) ic_->tampon_ver(r);
    v.clear();
}

std::size_t SonarKumesi::teslim_kapasitesi() const { return ic_->sonarlar.size() * kKuyrukSiniri; }

std::vector<std::int64_t> SonarKumesi::ping1d_en_kisa_aralik_ns() const
{
    const Ic& c = *ic_;
    std::vector<std::int64_t> v;
    for (const Ic::SonarDurumu& s : c.sonarlar) {
        if (!s.ping1d) continue;
        const auto& p = std::get<Ping1dAyar>(s.a.ayar);
        const Ping1dYapilandirmasi& w = s.p1d;
        double l = w.scan_length;
        if (w.otomatik) {
            // ping1d_at'ın otomatik menzili: pencere [min(0,5; kalan), kalan] aralığına kırpılır.
            const double hi = kP1dAzamiMenzil - w.scan_start;
            l = std::min(l, std::min(kP1dAsgariMenzil, hi));
        }
        const std::int64_t dinleme = ns(2.0 * (w.scan_start + l) / c.c);
        const std::int64_t aralik = c.olcekle(s, std::max(p.ping_araligi_ns, dinleme + p.islem_suresi_ns));
        v.push_back(std::max(c.ayar.adim_ns, aralik));
    }
    return v;
}

bool SonarKumesi::ping360_yapilandir(const std::string& ad, const Ping360Yapilandirmasi& y)
{
    Ic& c = *ic_;
    if (!c.kuruldu || !ping360_denetle(y).empty()) return false;
    for (auto& s : c.sonarlar) {
        if (s.a.ad != ad || s.ping1d) continue;
        s.p360_yeni = y;
        // Süren ping tamamlanır; kafa yeni başlangıca döner (05 §2.4.5).
        if (s.t_ideal >= 0) {
            const int fark = grad_farki(s.aci_tx, y.baslangic_grad);
            const std::int64_t t_min = c.ilk ? 0 : c.t_son + c.ayar.adim_ns;
            s.t_ideal_sonraki = s.t_ideal + s.son_sure + c.olcekle(s, y.bekleme_ns + c.motor(s, fark));
            s.sonraki_tx = std::max(c.en_yakin_sinir(s.t_ideal_sonraki), t_min);
        }
        return true;
    }
    return false;
}

bool SonarKumesi::ping1d_yapilandir(const std::string& ad, const Ping1dYapilandirmasi& y)
{
    Ic& c = *ic_;
    if (!c.kuruldu || !ping1d_denetle(y).empty()) return false;
    for (auto& s : c.sonarlar) {
        if (s.a.ad != ad || !s.ping1d) continue;
        s.p1d_yeni = y;
        return true;
    }
    return false;
}

ArmSonucu SonarKumesi::arm(const SensorArizasi& f)
{
    Ic& c = *ic_;
    ArmSonucu s;
    if (!c.kuruldu) {
        s.hata = "küme kurulmadı";
        return s;
    }
    std::size_t si = c.sonarlar.size();
    for (std::size_t i = 0; i < c.sonarlar.size(); ++i) {
        if (c.sonarlar[i].a.ad == f.hedef) si = i;
    }
    if (si == c.sonarlar.size()) {
        s.hata = "hedef etkin bir sonar değil (A5-K65)";
        return s;
    }
    switch (f.tur) {
    case ArizaTuru::gurultu_artisi:
        s.hata = "gurultu_artisi sonarda tanımlı değil (A5 Tablo 3.10-A, A5-K65)";
        return s;
    case ArizaTuru::ofset:
    case ArizaTuru::olcek:
    case ArizaTuru::suruklenme_dogrusal:
    case ArizaTuru::suruklenme_ustel:
    case ArizaTuru::sicrama:
        s.hata = "sayısal arıza: sonar kayıtlarının kanal numaralandırması tanımlı değil (A5 Ek D.4, 05 §2.7); yer tutucu";
        return s;
    case ArizaTuru::motor_takilmasi:
    case ArizaTuru::dvl_isin_kaybi:
    case ArizaTuru::guc_cevrimi:
        s.hata = "durumlu arıza bu dilimde desteklenmiyor (yer tutucu)";
        return s;
    default:
        break;
    }
    const sensor::ArizaSiddeti& z = f.siddet;
    bool gecerli = true;
    if (f.tur == ArizaTuru::gec_teslim) {
        gecerli = sure_ok(z.gecikme_ns, true) && z.titresim_ns >= 0 && z.titresim_ns <= z.gecikme_ns;
    }
    const sensor::ZamanDeseni& zd = f.zaman_deseni;
    if (zd.tur == ZamanDeseniTuru::bernoulli) gecerli = gecerli && zd.p >= 0.0 && zd.p <= 1.0;
    if (zd.tur == ZamanDeseniTuru::gilbert_elliott) {
        gecerli = gecerli && sure_ok(zd.t_iyi_ns, false) && sure_ok(zd.t_kotu_ns, false);
    }
    if (!gecerli || f.sure_ns < 0 || f.sure_ns > kAzamiZaman / 2 || f.zaman_ns < 0 || f.zaman_ns > kAzamiZaman) {
        s.hata = "arıza parametresi geçersiz (A5-K48, K66, K67, K68; 05 §2.7)";
        return s;
    }
    if (f.kanal_maskesi != 0xFFFFFFFFu) {
        s.hata = "kanal_maskesi bu sürümde yalnız 4294967295 (A5-K69)";
        return s;
    }
    if (f.kimlik.empty()) {
        s.hata = "arıza kimliği boş (A5-K60)";
        return s;
    }
    for (const auto& a : c.arizalar) {
        if (a.t.kimlik == f.kimlik) {
            s.hata = "arıza kimliği tekil değil (A5-K60)";
            return s;
        }
    }
    Ic::ArizaKaydi k;
    k.tutamac = c.sonraki_tutamac++;
    k.t = f;
    k.sonar = si;
    const std::int64_t h = c.ayar.adim_ns;
    const std::int64_t sonraki_sinir = c.ilk ? 0 : c.t_son + h;
    k.t_b = std::max(ust_sinir(f.zaman_ns, h), sonraki_sinir);
    k.t_bitis = f.sure_ns > 0 ? ust_sinir(k.t_b + f.sure_ns, h) : kSonsuz;
    k.akis = Akis(c.ayar.ana_tohum, "ariza/" + f.kimlik);
    k.gecis = Akis(c.ayar.ana_tohum, "ariza/" + f.kimlik + "/gecis");
    k.donma = Ic::bos_kayit(c.sonarlar[si].ping1d);   // donma kopyası adımda ayırmasız (W4-K27 (5))
    c.arizalar.push_back(std::move(k));
    s.tamam = true;
    s.tutamac = c.arizalar.back().tutamac;
    return s;
}

bool SonarKumesi::disarm(ArizaTutamaci t)
{
    Ic& c = *ic_;
    for (auto& a : c.arizalar) {
        if (a.tutamac == t && a.t_bitis > c.t_son) {
            const std::int64_t sonraki_sinir = c.ilk ? 0 : c.t_son + c.ayar.adim_ns;
            a.t_bitis = std::min(a.t_bitis, sonraki_sinir);
            return true;
        }
    }
    return false;
}

std::vector<EtkinAriza> SonarKumesi::active() const
{
    std::vector<EtkinAriza> v;
    for (const auto& a : ic_->arizalar) {
        if (a.t_bitis > ic_->t_son) v.push_back(EtkinAriza{a.tutamac, a.t, a.t_b, a.t_bitis});
    }
    return v;
}

bool SonarKumesi::kuruldu() const { return ic_->kuruldu; }
std::int64_t SonarKumesi::zaman_ns() const { return ic_->t_son; }
double SonarKumesi::ses_hizi() const { return ic_->c; }
SonarSayaclari SonarKumesi::sayaclar() const { return ic_->say; }

}  // namespace ida::sonar
