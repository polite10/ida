// Seyrüsefer ve sistem sensörlerinin ölçüm zinciri (05 §2.2, §2.3).
//
// Her model temel kaydı üretir (arıza zincirinden önce; 05 §2.1). Rastgele çekilişlerin
// yerleri (akış, n, kanal, bileşen) sim/sensor/BENIOKU.md'deki tablodadır.
#include "model.hpp"

#include <algorithm>
#include <deque>
#include <limits>
#include <optional>

namespace ida::sensor::ic {

namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr double kDunyaDonusu = 7.292115e-5;   // rad/s, WGS-84
constexpr double kSicaklikReferansi = 25.0;    // T₀, °C (05 §2.3 [T])
constexpr double kBasincAzami = 3.0e6;         // Pa; MS5837-30BA ölçüm aralığı 0–30 bar [Ü]
constexpr double kDvlAzamiHiz = 3.75;          // m/s [Ü, 05 §2.3.5]
constexpr double kDvlHizCozunurlugu = 1.0e-4;  // m/s [Ü]
constexpr double kDvlAzamiIrtifa = 50.0;       // m; mod 4'ün üst ucu [T]
constexpr double kDvlSatirBayt = 120.0;        // seri rapor satırı, bayt [T]
constexpr double kGnssEdinimSigmaLn = 0.3;     // ilk fix süresinin log-normal yayılımı [T]
constexpr double kGnssYukselmeAlt = 10.0 * kPi / 180.0;  // uydu yükselme maskesi [T]

double ns_s(std::int64_t ns) { return static_cast<double>(ns) * 1e-9; }

// Profil hızı (A5 Ek E) ve saat kayması ile periyot, ns.
double periyot_ns(const SensorAyari& a, double profil_hizi)
{
    const double f = a.hiz_hz.value_or(profil_hizi);
    return 1e9 / f * (1.0 + a.saat_kaymasi_ppm * 1e-6);
}

Mat3 montaj_matrisi(const SensorAyari& a) { return matris(normalle(a.montaj.yonelim)); }

double sicaklik(const Baglam& b) { return b.ic_sicaklik; }

// En yakın basamağa kuantalama; adım ≤ 0 ise kuantalama yok.
double kuantala(double x, double adim)
{
    if (!(adim > 0.0)) {
        return x;
    }
    return std::round(x / adim) * adim;
}

double tablo(const std::vector<double>& x, const std::vector<double>& y, double v)
{
    const std::size_t n = std::min(x.size(), y.size());
    if (n == 0) {
        return 0.0;
    }
    if (n == 1 || v >= x[n - 1]) {
        return y[n - 1];
    }
    std::size_t i = 0;
    while (i + 2 < n && v > x[i + 1]) {
        ++i;
    }
    const double t = (v - x[i]) / (x[i + 1] - x[i]);  // v < x[0] iken dışdeğerleme
    return y[i] + t * (y[i + 1] - y[i]);
}

}  // namespace

// ---------------------------------------------------------------- güç yardımcıları
double tam_gaz_akimi(const GucParametreleri& g, double gerilim)
{
    if (gerilim < 7.0) {
        return 0.0;
    }
    return std::max(0.0, tablo(g.gerilim_noktalari, g.tam_gaz_akimi, gerilim));
}

double itici_akimi(const GucParametreleri& g, double gerilim, double itki)
{
    if (gerilim < 7.0 || itki == 0.0) {
        return 0.0;
    }
    const double f_maks = itki > 0.0 ? tablo(g.gerilim_noktalari, g.azami_itki_ileri, gerilim)
                                     : tablo(g.gerilim_noktalari, g.azami_itki_geri, gerilim);
    if (!(f_maks > 0.0)) {
        return 0.0;
    }
    const double oran = std::min(1.0, std::abs(itki) / f_maks);
    return tam_gaz_akimi(g, gerilim) * std::pow(oran, g.akim_ussu);
}

double paket_akimi(const GucParametreleri& g, const fizik::DurumAnlik& d)
{
    const double v = g.v_sabit;
    double i = 0.0;
    for (std::size_t k = 0; k < static_cast<std::size_t>(fizik::kIticiSayisi); ++k) {
        i += itici_akimi(g, v, d.itki_istegi[k]);
    }
    if (v > 0.0) {
        i += g.aviyonik_guc / v;
    }
    return i;
}

// ---------------------------------------------------------------- model tabanı
Model::Model(const SensorAyari& a, std::uint64_t tohum)
    : a_(a),
      acilis_(tohum, "sensor/" + a.ad + "/acilis"),
      olcum_(tohum, "sensor/" + a.ad + "/olcum"),
      zamanlama_(tohum, "sensor/" + a.ad + "/zamanlama")
{
}

Model::~Model() = default;

TemelKayit Model::yeni_kayit(std::int64_t t_meas, std::uint32_t window, std::int64_t t_hazir,
                             std::int64_t t_teslim)
{
    TemelKayit r;
    r.hdr.sensor_id = a_.kimlik;
    r.hdr.tur = a_.tur;
    r.hdr.inst = a_.inst;
    r.ornek_no = ornek_++;
    r.hdr.seq = static_cast<std::uint32_t>(r.ornek_no & 0xFFFFFFFFu);
    r.hdr.t_meas_ns = t_meas;
    r.hdr.window_ns = window;
    r.hdr.t_hazir_ns = t_hazir;
    r.hdr.t_teslim_ns = t_teslim;
    r.hdr.flags = kBayrakGecerli;
    return r;
}

namespace {

// ================================================================ IMU (05 §2.3.1)
class ImuModeli final : public Model {
public:
    ImuModeli(const SensorAyari& a, const SensorKumesiAyari& k)
        : Model(a, k.ana_tohum), p_(std::get<ImuAyar>(a.ayar)), r_bs_(montaj_matrisi(a)),
          l_(a.montaj.konum), periyot_(k.kontrol_periyodu_ns), adim_ns_(k.adim_ns)
    {
        if (p_.dunya_donusu_etkin && k.ortam.orijin) {
            const double e = k.ortam.orijin->enlem;
            omega_ie_n_ = {kDunyaDonusu * std::cos(e), 0.0, -kDunyaDonusu * std::sin(e)};
        }
        const double dt = ns_s(periyot_);
        const double h = ns_s(adim_ns_);
        for (std::size_t i = 0; i < 6; ++i) {
            const bool jiro = i < 3;
            const double asd = jiro ? p_.jiroskop_yogunluk : p_.ivme_yogunluk[i - 3];
            beyaz_[i] = asd / std::sqrt(2.0 * dt);
            suzgec_gurultu_[i] = asd / std::sqrt(2.0 * h);
            fs_[i] = jiro ? p_.jiroskop_aralik : p_.ivme_aralik;
            lsb_[i] = 2.0 * fs_[i] / std::ldexp(1.0, p_.bit);
            const double sinir = jiro ? p_.jiroskop_acilis_bias_siniri : p_.ivme_acilis_bias_siniri;
            const double ts = jiro ? p_.jiroskop_sicaklik_kats_siniri : p_.ivme_sicaklik_kats_siniri;
            const double ks = jiro ? p_.jiroskop_dogrusalsizlik : p_.ivme_dogrusalsizlik;
            const auto kanal = static_cast<std::uint32_t>(i);
            b_on_[i] = sinir / 3.0 * acilis_.normal(0, c2_kodla(kanal, 0, 0));
            b_t_[i] = ts / 3.0 * acilis_.normal(0, c2_kodla(kanal, 1, 0));
            kappa_[i] = ks / 3.0 * acilis_.normal(0, c2_kodla(kanal, 5, 0));
            gm_sigma_[i] = jiro ? p_.jiroskop_gm_sigma : p_.ivme_gm_sigma;
            const double tau = ns_s(jiro ? p_.jiroskop_gm_tau_ns : p_.ivme_gm_tau_ns);
            gm_phi_[i] = std::exp(-dt / tau);
            gm_[i] = gm_sigma_[i] * acilis_.normal(0, c2_kodla(kanal, 6, 0));
        }
        // S·M (üçlü başına): köşegen 1 + s_i; köşegen dışı m_ij.
        for (std::size_t u = 0; u < 2; ++u) {
            const double s_sinir = u == 0 ? p_.jiroskop_olcek_siniri : p_.ivme_olcek_siniri;
            const double m_sinir =
                u == 0 ? p_.jiroskop_eksenler_arasi_siniri : p_.ivme_eksenler_arasi_siniri;
            Mat3 sm{};
            for (std::size_t r = 0; r < 3; ++r) {
                const auto kanal = static_cast<std::uint32_t>(3 * u + r);
                const double s = s_sinir / 3.0 * acilis_.normal(0, c2_kodla(kanal, 2, 0));
                std::uint32_t bilesen = 3;
                for (std::size_t c = 0; c < 3; ++c) {
                    if (c == r) {
                        sm[r][c] = 1.0 + s;
                    } else {
                        const double m = m_sinir / 3.0 * acilis_.normal(0, c2_kodla(kanal, bilesen, 0));
                        ++bilesen;
                        sm[r][c] = (1.0 + s) * m;
                    }
                }
            }
            sm_[u] = sm;
        }
        if (p_.suzgec_etkin) {
            alfa_ = 1.0 - std::exp(-h / ns_s(p_.suzgec_tau_ns));
            // Süzgeçli çıktının örnek başına nominal σ'sı (gürültü artışı arızası için; A5 Tablo
            // 3.10-A): adım gürültüsü σ_h'nin birinci derece süzgeçten (durağan AR(1),
            // a = 1 − α) geçip pencerede yamuk ortalanmış hâli:
            //   σ² = σ_h² · α²/(1 − a²) · Σ_i Σ_j w_i w_j a^|i−j|, w = (½, 1, …, 1, ½)/N.
            const std::size_t n = static_cast<std::size_t>(periyot_ / adim_ns_);
            const double ar = 1.0 - alfa_;
            double toplam = 0.0;
            for (std::size_t i = 0; i <= n; ++i) {
                const double wi = (i == 0 || i == n) ? 0.5 : 1.0;
                for (std::size_t j = 0; j <= n; ++j) {
                    const double wj = (j == 0 || j == n) ? 0.5 : 1.0;
                    const double fark = static_cast<double>(i > j ? i - j : j - i);
                    toplam += wi * wj * std::pow(ar, fark);
                }
            }
            const double carpan = std::sqrt(alfa_ * alfa_ / (1.0 - ar * ar) * toplam) / static_cast<double>(n);
            for (std::size_t i = 0; i < 6; ++i) {
                suzgec_nominal_[i] = suzgec_gurultu_[i] * carpan;
            }
        }
        deger_.assign(static_cast<std::size_t>(periyot_ / adim_ns_) + 2, {});
    }

    std::size_t halka_ihtiyaci(std::int64_t) const override { return 2; }

    void baslat(const Baglam& b) override
    {
        zam_.kur(static_cast<double>(periyot_), adim_ns_, b.t_ns + periyot_);
    }

    void adim(const Baglam& b, std::vector<TemelKayit>& cikti) override
    {
        const AdimKaydi* r = b.halka->al(b.adim_no);
        std::array<double, 6> x = ham(r->d);
        std::array<double, 6> xp{};
        const double dT = sicaklik(b) - kSicaklikReferansi;
        for (std::size_t u = 0; u < 2; ++u) {
            const Vek3 v = {x[3 * u], x[3 * u + 1], x[3 * u + 2]};
            const Vek3 smv = carp(sm_[u], v);
            for (std::size_t j = 0; j < 3; ++j) {
                const std::size_t i = 3 * u + j;
                xp[i] = smv[j] + b_on_[i] + gm_[i] + b_t_[i] * dT +
                        kappa_[i] * x[i] * std::abs(x[i]) / fs_[i];
            }
        }
        std::array<double, 6> deger = xp;
        if (p_.suzgec_etkin) {
            if (!y_hazir_) {
                y_ = xp;
                y_hazir_ = true;
            }
            for (std::size_t i = 0; i < 6; ++i) {
                const double nh = suzgec_gurultu_[i] *
                                  olcum_.normal(b.adim_no, c2_kodla(static_cast<std::uint32_t>(i), 2, 0));
                y_[i] += alfa_ * (xp[i] + nh - y_[i]);
            }
            deger = y_;
        }
        deger_[static_cast<std::size_t>(b.adim_no % deger_.size())] = deger;

        if (!zam_.tetik(b.adim_no)) {
            return;
        }
        // Pencere [t − P, t]: adım sınırlarındaki değerlerin yamuk ortalaması.
        const std::uint64_t n = static_cast<std::uint64_t>(periyot_ / adim_ns_);
        const std::uint64_t k0 = b.adim_no - n;
        std::array<double, 6> ort{};
        for (std::uint64_t k = k0; k <= b.adim_no; ++k) {
            const double w = (k == k0 || k == b.adim_no) ? 0.5 : 1.0;
            const auto& v = deger_[static_cast<std::size_t>(k % deger_.size())];
            for (std::size_t i = 0; i < 6; ++i) {
                ort[i] += w * v[i];
            }
        }
        TemelKayit kayit =
            yeni_kayit(b.t_ns, static_cast<std::uint32_t>(periyot_), b.t_ns, b.t_ns);
        const std::uint64_t ornek = kayit.ornek_no;
        bool doymus = false;
        for (std::size_t i = 0; i < 6; ++i) {
            double y = ort[i] / static_cast<double>(n);
            if (!p_.suzgec_etkin) {
                y += beyaz_[i] * olcum_.normal(ornek, c2_kodla(static_cast<std::uint32_t>(i), 0, 0));
            }
            if (std::abs(y) > fs_[i]) {
                y = std::copysign(fs_[i], y);
                doymus = true;
            }
            kayit.k[i] = kuantala(y, lsb_[i]);
            kayit.sigma_nom[i] = p_.suzgec_etkin ? suzgec_nominal_[i] : beyaz_[i];
        }
        kayit.k[6] = sicaklik(b);
        kayit.n_kanal = 7;
        if (doymus) {
            kayit.hdr.flags |= kBayrakDoymus;
        }
        cikti.push_back(kayit);
        // Gauss–Markov sapması bir sonraki pencere için (05 §2.3, ayrık ve adımdan bağımsız).
        for (std::size_t i = 0; i < 6; ++i) {
            const double xi = olcum_.normal(ornek, c2_kodla(static_cast<std::uint32_t>(i), 1, 0));
            gm_[i] = gm_phi_[i] * gm_[i] +
                     gm_sigma_[i] * std::sqrt(1.0 - gm_phi_[i] * gm_phi_[i]) * xi;
        }
    }

private:
    // Sensör çerçevesinde (ω_s, f_s), montaj noktasında (05 §2.3.1 yer gerçeği).
    std::array<double, 6> ham(const fizik::DurumAnlik& d) const
    {
        const Mat3 r_nb = matris(d.yonelim);
        const Vek3 w = d.acisal_hiz_b;
        const Vek3 w_ie_b = carp_t(r_nb, omega_ie_n_);
        const Vek3 ws = carp_t(r_bs_, topla(w, w_ie_b));
        const Vek3 f_b =
            topla(d.ozgul_kuvvet_b, topla(capraz(d.acisal_ivme_b, l_), capraz(w, capraz(w, l_))));
        const Vek3 fs = carp_t(r_bs_, f_b);
        return {ws[0], ws[1], ws[2], fs[0], fs[1], fs[2]};
    }

    ImuAyar p_;
    Mat3 r_bs_;
    Vek3 l_;
    std::int64_t periyot_;
    std::int64_t adim_ns_;
    Vek3 omega_ie_n_{};
    std::array<Mat3, 2> sm_{};
    std::array<double, 6> b_on_{}, b_t_{}, kappa_{}, gm_{}, gm_sigma_{}, gm_phi_{};
    std::array<double, 6> beyaz_{}, suzgec_gurultu_{}, suzgec_nominal_{}, fs_{}, lsb_{};
    double alfa_ = 0.0;
    std::array<double, 6> y_{};
    bool y_hazir_ = false;
    std::vector<std::array<double, 6>> deger_;
    Zamanlayici zam_;
};

// ================================================================ manyetometre (05 §2.3.2)
class ManyetometreModeli final : public Model {
public:
    ManyetometreModeli(const SensorAyari& a, const SensorKumesiAyari& k)
        : Model(a, k.ana_tohum), p_(std::get<ManyetometreAyar>(a.ayar)), r_bs_(montaj_matrisi(a)),
          b_n_(k.ortam.manyetik_alan.value_or(Vek3{})), guc_(k.guc),
          periyot_(periyot_ns(a, 100.0)), adim_ns_(k.adim_ns)
    {
        // İtici girişimi [T]: itici k'nın tam gaz akımında alanı oran·‖B_n‖/N; yönü, itici
        // konumundaki itki ekseni doğrultusunda bir dipolün manyetometre konumundaki alanı
        // (gövdeye sabit; iki dönüş yönünde aynı, K⁺ = K⁻: ESC'nin DC akımı yönden bağımsız).
        const double i_tam = tam_gaz_akimi(guc_, guc_.v_sabit);
        const double buyukluk = p_.itici_girisim_orani * norm(b_n_) /
                                static_cast<double>(fizik::kIticiSayisi);
        for (std::size_t i = 0; i < static_cast<std::size_t>(fizik::kIticiSayisi); ++i) {
            Vek3 e = guc_.itici_yonu[i];
            const double ne = norm(e);
            if (!(ne > 0.0) || !(i_tam > 0.0)) {
                k_[i] = {};
                continue;
            }
            e = olcekle(e, 1.0 / ne);
            const Vek3 r = cikar(a.montaj.konum, guc_.itici_konumu[i]);
            Vek3 dir = e;
            const double nr = norm(r);
            if (nr > 1e-6) {
                const Vek3 rh = olcekle(r, 1.0 / nr);
                const Vek3 d = cikar(olcekle(rh, 3.0 * nokta(e, rh)), e);
                if (norm(d) > 1e-9) {
                    dir = olcekle(d, 1.0 / norm(d));
                }
            }
            k_[i] = olcekle(carp_t(r_bs_, dir), buyukluk / i_tam);
        }
    }

    std::size_t halka_ihtiyaci(std::int64_t) const override { return 2; }

    void baslat(const Baglam& b) override { zam_.kur(periyot_, adim_ns_, b.t_ns); }

    void adim(const Baglam& b, std::vector<TemelKayit>& cikti) override
    {
        if (!zam_.tetik(b.adim_no)) {
            return;
        }
        const fizik::DurumAnlik& d = b.halka->al(b.adim_no)->d;
        const Vek3 b_s = carp_t(r_bs_, carp_t(matris(d.yonelim), b_n_));
        Vek3 v = topla(carp(p_.yumusak_demir, b_s), p_.sert_demir);
        const double sic = p_.sicaklik_kats * (sicaklik(b) - kSicaklikReferansi);
        v = topla(v, {sic, sic, sic});
        for (std::size_t i = 0; i < static_cast<std::size_t>(fizik::kIticiSayisi); ++i) {
            const double akim = itici_akimi(guc_, guc_.v_sabit, d.itki_istegi[i]);
            v = topla(v, olcekle(k_[i], akim));
        }
        TemelKayit r = yeni_kayit(b.t_ns, 0, b.t_ns, b.t_ns + p_.teslim_gecikmesi_ns);
        for (std::size_t j = 0; j < 3; ++j) {
            r.k[j] = v[j] + p_.gurultu_sigma *
                                olcum_.normal(r.ornek_no, c2_kodla(static_cast<std::uint32_t>(j), 0, 0));
            r.sigma_nom[j] = p_.gurultu_sigma;
        }
        r.n_kanal = 3;
        cikti.push_back(r);
    }

private:
    ManyetometreAyar p_;
    Mat3 r_bs_;
    Vek3 b_n_;
    GucParametreleri guc_;
    double periyot_;
    std::int64_t adim_ns_;
    std::array<Vek3, fizik::kIticiSayisi> k_{};
    Zamanlayici zam_;
};

// ================================================================ basınç (05 §2.3.3)
class BasincModeli final : public Model {
public:
    BasincModeli(const SensorAyari& a, const SensorKumesiAyari& k)
        : Model(a, k.ana_tohum), p_(std::get<BasincAyar>(a.ayar)), l_(a.montaj.konum),
          ortam_(k.ortam), periyot_(periyot_ns(a, 25.0)), adim_ns_(k.adim_ns)
    {
        ofset_ = p_.mutlak_dogruluk_siniri / 3.0 * acilis_.normal(0, c2_kodla(0, 0, 0));
        sic_ofset_ = p_.sicaklik_ofset_siniri / 3.0 * acilis_.normal(0, c2_kodla(1, 0, 0));
    }

    std::size_t halka_ihtiyaci(std::int64_t h) const override
    {
        return static_cast<std::size_t>((2 * p_.donusum_penceresi_ns + p_.okuma_suresi_ns) / h) + 4;
    }

    void baslat(const Baglam& b) override { zam_.kur(periyot_, adim_ns_, b.t_ns); }

    void adim(const Baglam& b, std::vector<TemelKayit>& cikti) override
    {
        if (zam_.tetik(b.adim_no)) {
            bekleyen_.push_back(b.t_ns);   // D1 dönüşümünün başlangıcı
        }
        while (!bekleyen_.empty()) {
            const std::int64_t bas = bekleyen_.front();
            const std::int64_t t_meas = bas + p_.donusum_penceresi_ns;
            const std::int64_t t_fiz = t_meas + p_.donusum_penceresi_ns + p_.okuma_suresi_ns;
            if (ust_sinir(t_fiz, adim_ns_) > b.t_ns) {
                break;
            }
            bekleyen_.pop_front();
            std::array<double, 1> p{};
            const bool tamam = aralik_ortalamasi<1>(
                *b.halka, adim_ns_, bas, t_meas,
                [this](const AdimKaydi& r, std::uint64_t) {
                    return std::array<double, 1>{agiz_basinci(r.d)};
                },
                p);
            TemelKayit r = yeni_kayit(t_meas, static_cast<std::uint32_t>(p_.donusum_penceresi_ns),
                                      b.t_ns, t_fiz);
            double y = std::numeric_limits<double>::quiet_NaN();
            if (tamam) {
                y = p[0] + ofset_ + p_.sicaklik_kats * (sicaklik(b) - kSicaklikReferansi) +
                    p_.gurultu_sigma * olcum_.normal(r.ornek_no, c2_kodla(0, 0, 0));
                y = kuantala(y, p_.kuantalama);
                if (y > kBasincAzami || y < 0.0) {
                    y = std::clamp(y, 0.0, kBasincAzami);
                    r.hdr.flags |= kBayrakDoymus;
                }
            }
            r.k[0] = y;
            r.k[1] = sicaklik(b) + sic_ofset_;
            r.sigma_nom[0] = p_.gurultu_sigma;
            r.n_kanal = 2;
            cikti.push_back(r);
        }
    }

private:
    // Sensör ağzındaki mutlak basınç (05 §2.3.3; dalgasız su, 04 §2.4 hidrostatiği).
    double agiz_basinci(const fizik::DurumAnlik& d) const
    {
        const Mat3 r_nb = matris(d.yonelim);
        const Vek3 agiz = topla(d.konum_n, carp(r_nb, l_));
        if (!(agiz[2] > 0.0)) {
            return ortam_.atmosfer_basinci;   // ağız su üstünde
        }
        double p = ortam_.atmosfer_basinci + ortam_.su.yogunluk * ortam_.yercekimi * agiz[2];
        if (p_.dinamik_basinc_kats != 0.0) {
            const Vek3 v_n = carp(r_nb, topla(d.dogrusal_hiz_b, capraz(d.acisal_hiz_b, l_)));
            const Vek3 v_r = cikar(v_n, ortam_.akinti.hiz_ned);
            p += p_.dinamik_basinc_kats * 0.5 * ortam_.su.yogunluk * nokta(v_r, v_r);
        }
        return p;
    }

    BasincAyar p_;
    Vek3 l_;
    SensorOrtami ortam_;
    double periyot_;
    std::int64_t adim_ns_;
    double ofset_ = 0.0;
    double sic_ofset_ = 0.0;
    std::deque<std::int64_t> bekleyen_;
    Zamanlayici zam_;
};

// ================================================================ GNSS (05 §2.3.4)
class GnssModeli final : public Model {
public:
    GnssModeli(const SensorAyari& a, const SensorKumesiAyari& k)
        : Model(a, k.ana_tohum), p_(std::get<GnssAyar>(a.ayar)), l_(a.montaj.konum),
          periyot_(periyot_ns(a, 5.0)), adim_ns_(k.adim_ns)
    {
        const std::size_t n = static_cast<std::size_t>(p_.uydu_sayisi);
        sin_e_.resize(n);
        const double s0 = std::sin(kGnssYukselmeAlt);
        for (std::size_t j = 0; j < n; ++j) {
            // Gökyüzünde düzgün: sin e ∈ [sin e_min, 1] düzgün.
            const double u = acilis_.duzgun(0, c2_kodla(static_cast<std::uint32_t>(16 + j), 0, 0));
            sin_e_[j] = s0 + (1.0 - s0) * u;
        }
        iz_.assign(n, false);
        kuru_bas_.assign(n, -1);
        sigma_ = p_.cep / std::sqrt(2.0 * std::log(2.0));
        for (std::size_t i = 0; i < 3; ++i) {
            gm_[i] = acilis_.normal(0, c2_kodla(static_cast<std::uint32_t>(i), 1, 0));
        }
    }

    std::size_t halka_ihtiyaci(std::int64_t) const override { return 2; }

    void baslat(const Baglam& b) override
    {
        zam_.kur(periyot_, adim_ns_, b.t_ns);
        const double w = su_kalinligi(b.halka->al(b.adim_no)->d);
        // Başlangıçta alıcı sıcaktır [T]: kuru uydular izlenir; hiç yoksa edinim kesintisi
        // başlangıçtan sayılır.
        std::size_t n = 0;
        for (std::size_t j = 0; j < iz_.size(); ++j) {
            iz_[j] = w / sin_e_[j] < p_.w50;
            n += iz_[j] ? 1u : 0u;
        }
        if (n == 0) {
            edinim_ = true;
            t_kayip_ = b.t_ns;
        }
        t_onceki_ = b.t_ns;
    }

    void adim(const Baglam& b, std::vector<TemelKayit>& cikti) override
    {
        if (!zam_.tetik(b.adim_no)) {
            return;
        }
        const fizik::DurumAnlik& d = b.halka->al(b.adim_no)->d;
        TemelKayit r = yeni_kayit(b.t_ns, 0, b.t_ns, b.t_ns + p_.teslim_gecikmesi_ns);
        const std::uint64_t ornek = r.ornek_no;
        izleme(b.t_ns, su_kalinligi(d), ornek);
        const std::size_t n_sv = edinim_ ? 0u : static_cast<std::size_t>(std::count(iz_.begin(), iz_.end(), true));

        // Gauss–Markov ve beyaz gürültü (birim varyanslı; σ_eff ile ölçeklenir).
        const double dt = ns_s(b.t_ns - t_onceki_);
        t_onceki_ = b.t_ns;
        const double phi = std::exp(-dt / ns_s(p_.gm_tau_ns));
        std::array<double, 3> hata{};
        const double sigma_eff =
            n_sv > 0 ? sigma_ * std::sqrt(static_cast<double>(p_.n_sv_ref) / static_cast<double>(n_sv))
                     : 0.0;
        for (std::size_t i = 0; i < 3; ++i) {
            const auto kanal = static_cast<std::uint32_t>(i);
            gm_[i] = phi * gm_[i] +
                     std::sqrt(1.0 - phi * phi) * olcum_.normal(ornek, c2_kodla(kanal, 1, 0));
            const double w = olcum_.normal(ornek, c2_kodla(kanal, 0, 0));
            const double carpan = i == 2 ? p_.dusey_carpan : 1.0;
            hata[i] = carpan * sigma_eff *
                      (std::sqrt(p_.gm_varyans_payi) * gm_[i] + std::sqrt(1.0 - p_.gm_varyans_payi) * w);
        }
        if (t_fix_ != kYok && p_.yakinsama_baslangic_hatasi > 0.0) {
            const double e = p_.yakinsama_baslangic_hatasi *
                             std::exp(-ns_s(b.t_ns - t_fix_) / ns_s(p_.yakinsama_tau_ns));
            hata[0] += e * std::cos(yak_yon_);
            hata[1] += e * std::sin(yak_yon_);
        }
        const Mat3 r_nb = matris(d.yonelim);
        const Vek3 anten = topla(d.konum_n, carp(r_nb, l_));
        const Vek3 v_n = carp(r_nb, topla(d.dogrusal_hiz_b, capraz(d.acisal_hiz_b, l_)));
        std::array<double, 3> vg{};
        for (std::size_t i = 0; i < 3; ++i) {
            vg[i] = p_.hiz_sigma * olcum_.normal(ornek, c2_kodla(static_cast<std::uint32_t>(3 + i), 0, 0));
        }
        r.n_sv = static_cast<std::uint8_t>(std::min<std::size_t>(n_sv, 255));
        r.fix_type = n_sv >= 4 ? kFix3B : (n_sv == 3 ? kFix2B : kFixYok);
        r.n_kanal = 6;
        if (r.fix_type == kFixYok) {
            r.gnss_cozum_yok = true;
            r.hdr.flags = static_cast<std::uint16_t>(r.hdr.flags & (0xFFFFu ^ kBayrakGecerli));
        } else {
            for (std::size_t i = 0; i < 3; ++i) {
                r.k[i] = anten[i] + hata[i];
                r.k[3 + i] = v_n[i] + vg[i];
                r.sigma_nom[i] = (i == 2 ? p_.dusey_carpan : 1.0) * sigma_eff;
                r.sigma_nom[3 + i] = p_.hiz_sigma;
            }
            r.eph = sigma_eff * p_.rapor_carpani;
            r.epv = p_.dusey_carpan * sigma_eff * p_.rapor_carpani;
        }
        cikti.push_back(r);
    }

private:
    static constexpr std::int64_t kYok = std::numeric_limits<std::int64_t>::min();

    // Anten üstündeki su kalınlığı (dalgasız su: ζ = 0).
    double su_kalinligi(const fizik::DurumAnlik& d) const
    {
        const Vek3 anten = topla(d.konum_n, carp(matris(d.yonelim), l_));
        return std::max(0.0, anten[2]);
    }

    // Uydu izleme ve edinim durum makinesi (05 §2.3.4).
    void izleme(std::int64_t t, double w, std::uint64_t ornek)
    {
        const double s = p_.w_gecis;
        std::size_t gorunur = 0;
        for (std::size_t j = 0; j < iz_.size(); ++j) {
            gorunur += (w / sin_e_[j] < p_.w50 - s) ? 1u : 0u;
        }
        if (!edinim_) {
            std::size_t n = 0;
            for (std::size_t j = 0; j < iz_.size(); ++j) {
                const double wj = w / sin_e_[j];
                if (iz_[j]) {
                    if (wj > p_.w50 + s) {
                        iz_[j] = false;
                    } else if (std::abs(wj - p_.w50) <= s) {
                        const double u = olcum_.duzgun(ornek, c2_kodla(static_cast<std::uint32_t>(16 + j), 0, 0));
                        if (u < 1.0 / (1.0 + std::exp(-(wj - p_.w50) / s))) {
                            iz_[j] = false;
                        }
                    }
                    kuru_bas_[j] = -1;
                } else if (wj < p_.w50 - s) {
                    if (kuru_bas_[j] < 0) {
                        kuru_bas_[j] = t;
                    }
                    if (t - kuru_bas_[j] >= p_.t_edinim_ns) {
                        iz_[j] = true;
                        kuru_bas_[j] = -1;
                    }
                } else {
                    kuru_bas_[j] = -1;
                }
                n += iz_[j] ? 1u : 0u;
            }
            if (n == 0) {
                edinim_ = true;
                t_kayip_ = t;
                sayiyor_ = false;
            }
        }
        if (edinim_) {
            // İlk fix için en az dört kuru uydu [T]; suya girince edinim sayacı sıfırlanır.
            if (gorunur >= 4) {
                if (!sayiyor_) {
                    const std::int64_t kesinti = t - t_kayip_;
                    const std::int64_t medyan = kesinti < p_.t_kisa_ns
                                                    ? p_.hizli_medyan_ns
                                                    : (kesinti <= p_.t_efemeris_ns ? p_.sicak_medyan_ns
                                                                                   : p_.soguk_medyan_ns);
                    const double z = zamanlama_.normal(edinim_no_, c2_kodla(0, 0, 0));
                    const double sure = static_cast<double>(medyan) * std::exp(kGnssEdinimSigmaLn * z);
                    t_edinim_bitis_ = t + static_cast<std::int64_t>(std::llround(sure));
                    sayiyor_ = true;
                }
                if (t >= t_edinim_bitis_) {
                    edinim_ = false;
                    sayiyor_ = false;
                    for (std::size_t j = 0; j < iz_.size(); ++j) {
                        iz_[j] = w / sin_e_[j] < p_.w50 - s;
                        kuru_bas_[j] = -1;
                    }
                    t_fix_ = t;
                    yak_yon_ = 2.0 * kPi * zamanlama_.duzgun(edinim_no_, c2_kodla(0, 1, 0));
                    ++edinim_no_;
                }
            } else if (sayiyor_) {
                sayiyor_ = false;
                ++edinim_no_;
            }
        }
    }

    GnssAyar p_;
    Vek3 l_;
    double periyot_;
    std::int64_t adim_ns_;
    std::vector<double> sin_e_;
    std::vector<bool> iz_;
    std::vector<std::int64_t> kuru_bas_;
    bool edinim_ = false;
    bool sayiyor_ = false;
    std::int64_t t_kayip_ = 0;
    std::int64_t t_edinim_bitis_ = 0;
    std::uint64_t edinim_no_ = 0;
    std::int64_t t_fix_ = kYok;
    double yak_yon_ = 0.0;
    double sigma_ = 0.0;
    std::array<double, 3> gm_{};
    std::int64_t t_onceki_ = 0;
    Zamanlayici zam_;
};

// ================================================================ DVL (05 §2.3.5; sade model)
struct DvlModu {
    double h_min, h_max, f_min, f_max;
};
constexpr std::array<DvlModu, 5> kDvlModlari = {{
    {0.05, 0.6, 15.0, 15.0},
    {0.3, 3.0, 10.0, 10.0},
    {1.5, 14.0, 5.0, 6.0},
    {7.7, 36.0, 7.0, 8.0},
    {15.0, kDvlAzamiIrtifa, 2.0, 4.0},
}};
constexpr int kDvlIlkMod = 2;   // ilk aramanın başladığı mod [T]

class DvlModeli final : public Model {
public:
    DvlModeli(const SensorAyari& a, const SensorKumesiAyari& k)
        : Model(a, k.ana_tohum), p_(std::get<DvlAyar>(a.ayar)), r_bs_(montaj_matrisi(a)),
          l_(a.montaj.konum), taban_(k.ortam.derinlik), c_(k.ortam.su.ses_hizi.value_or(1500.0)),
          adim_ns_(k.adim_ns)
    {
        const double sa = std::sin(p_.isin_acisi);
        const double ca = std::cos(p_.isin_acisi);
        for (std::size_t i = 0; i < 4; ++i) {
            const double psi = p_.azimut_ofseti + 0.5 * kPi * static_cast<double>(i);
            isin_[i] = {sa * std::cos(psi), sa * std::sin(psi), ca};
        }
        const double c_ayar = p_.c_ayar.value_or(c_);
        olcek_ = std::pow(c_ayar / c_, p_.kappa) *
                 (1.0 + p_.uzun_donem_olcek_siniri / 3.0 * acilis_.normal(0, c2_kodla(0, 0, 0)));
        arama_sirasi_kur(kDvlIlkMod);
    }

    std::size_t halka_ihtiyaci(std::int64_t h) const override
    {
        const double r_maks = kDvlAzamiIrtifa / std::cos(p_.isin_acisi);
        const double s = 2.0 * r_maks / c_ + ns_s(p_.islem_suresi_ns);
        return static_cast<std::size_t>(s * 1e9 / static_cast<double>(h)) + 8;
    }

    void baslat(const Baglam& b) override
    {
        t_ideal_ = b.t_ns;
        t_tx_ = b.t_ns;
        son_yakin_ = b.t_ns;
    }

    void adim(const Baglam& b, std::vector<TemelKayit>& cikti) override
    {
        if (b.t_ns == t_tx_) {
            ping(b);
        }
        while (!bekleyen_.empty() && bekleyen_.front().t_hazir <= b.t_ns) {
            rapor(b, bekleyen_.front(), cikti);
            bekleyen_.pop_front();
        }
    }

private:
    struct Ping {
        std::int64_t t_tx = 0, t_meas = 0, t_rapor = 0, t_hazir = 0, t_teslim = 0;
        std::array<double, 4> menzil{};
        std::uint8_t gecerli = 0;     // bit i: ışın i geçerli
        double irtifa = -1.0;         // dönüştürücüden tabana dik mesafe (A4 §3.3, W4-K8)
        bool cozum = false;
        std::uint64_t seq = 0;    // model içi ping numarası (64 bit)
    };

    void arama_sirasi_kur(int m)
    {
        arama_sirasi_.clear();
        arama_sirasi_.push_back(m);
        for (int d = 1; d < 5; ++d) {
            if (m - d >= 0) arama_sirasi_.push_back(m - d);
            if (m + d <= 4) arama_sirasi_.push_back(m + d);
        }
        arama_idx_ = 0;
        arama_ = true;
    }

    void ping(const Baglam& b)
    {
        const fizik::DurumAnlik& d = b.halka->al(b.adim_no)->d;
        const Mat3 r_nb = matris(d.yonelim);
        const Vek3 donusturucu = topla(d.konum_n, carp(r_nb, l_));
        const double ca = std::cos(p_.isin_acisi);
        std::array<bool, 4> isabet{};
        std::array<double, 4> menzil{};
        int n_isabet = 0;
        for (std::size_t i = 0; i < 4; ++i) {
            const Vek3 dn = carp(r_nb, carp(r_bs_, isin_[i]));
            if (!(donusturucu[2] > 0.0) || !(dn[2] > 0.0)) {
                continue;   // dönüştürücü havada ya da ışın dibe bakmıyor
            }
            const double r = (taban_ - donusturucu[2]) / dn[2];
            if (!(r > 0.0) || std::acos(std::min(1.0, dn[2])) >= p_.gelis_acisi_siniri ||
                r * ca > kDvlAzamiIrtifa) {
                continue;
            }
            isabet[i] = true;
            menzil[i] = r;
            ++n_isabet;
        }
        const bool yakin = yakin_deneme_;
        yakin_deneme_ = false;
        const int mod = arama_ ? arama_sirasi_[arama_idx_] : (yakin ? mod_ - 1 : mod_);
        const DvlModu& md = kDvlModlari[static_cast<std::size_t>(mod)];
        Ping p;
        p.t_tx = b.t_ns;
        p.menzil = menzil;
        int n_gecerli = 0;
        double r_top = 0.0;
        for (std::size_t i = 0; i < 4; ++i) {
            const double h = menzil[i] * ca;
            if (isabet[i] && h >= md.h_min && h <= md.h_max) {
                p.gecerli = static_cast<std::uint8_t>(p.gecerli | (1u << i));
                r_top += menzil[i];
                ++n_gecerli;
            }
        }
        p.cozum = n_gecerli >= (p_.uc_isinla_cozum ? 3 : 4);
        // İrtifa: düz, yatay tabana dik mesafe (A4 §3.3 "dibe dik mesafe"; W4-K8). 05 §2.3.5'in
        // ışın menzili · cos(ışın açısı) ortalaması eğik araçta bundan farklıdır; A4 geçerlidir.
        p.irtifa = n_isabet > 0 ? taban_ - donusturucu[2] : -1.0;
        mod_mantigi(b.t_ns, mod, yakin, p.cozum, p.irtifa);

        // Nedensel zaman zinciri (05 §2.3.5).
        p.seq = ornek_;
        const double r_ort = p.cozum ? r_top / static_cast<double>(n_gecerli) : md.h_max / ca;
        p.t_meas = p.cozum ? p.t_tx + std::llround(r_ort / c_ * 1e9) : p.t_tx;
        p.t_rapor = p.t_tx + std::llround(2.0 * r_ort / c_ * 1e9) + p_.islem_suresi_ns;
        double iletim = 0.0;
        if (p_.baglanti == DvlBaglanti::seri) {
            iletim = kDvlSatirBayt * 10.0 / static_cast<double>(p_.seri_baud) * 1e9;
        } else {
            // TCP: ortalama 4 ms, σ 2 ms, [0,5; 13] ms'ye kırpılmış normal [Ü gözlem; kırpma T].
            const double z = zamanlama_.normal(p.seq, c2_kodla(0, 1, 0));
            iletim = std::clamp(4.0e6 + 2.0e6 * z, 0.5e6, 13.0e6);
        }
        p.t_teslim = p.t_rapor + std::llround(iletim);
        p.t_hazir = ust_sinir(p.t_rapor, adim_ns_);
        ++ornek_;
        bekleyen_.push_back(p);

        // Sonraki ping: kullanılacak modun hız aralığından düzgün [T].
        const int sonraki_mod = arama_ ? arama_sirasi_[arama_idx_] : (yakin_deneme_ ? mod_ - 1 : mod_);
        const DvlModu& ms = kDvlModlari[static_cast<std::size_t>(sonraki_mod)];
        const double f = ms.f_min + (ms.f_max - ms.f_min) * zamanlama_.duzgun(p.seq, c2_kodla(0, 0, 0));
        t_ideal_ += std::llround(1e9 / f);
        t_tx_ = ((t_ideal_ + adim_ns_ / 2) / adim_ns_) * adim_ns_;
        if (t_tx_ <= b.t_ns) {
            t_tx_ = b.t_ns + adim_ns_;
        }
    }

    // Mod durum makinesi (05 §2.3.5): histerezis, kilit kaybında arama, yakın mod denemesi.
    void mod_mantigi(std::int64_t t, int mod, bool yakin, bool cozum, double irtifa)
    {
        if (arama_) {
            if (cozum) {
                mod_ = mod;
                arama_ = false;
                kilitsiz_ = 0;
                disari_ = 0;
                son_yakin_ = t;
            } else {
                arama_idx_ = (arama_idx_ + 1) % arama_sirasi_.size();
            }
        } else if (yakin) {
            son_yakin_ = t;
            if (cozum) {
                mod_ = mod;
                disari_ = 0;
            }
        } else {
            const DvlModu& md = kDvlModlari[static_cast<std::size_t>(mod_)];
            if (irtifa >= 0.0 && ((irtifa < md.h_min && mod_ > 0) || (irtifa > md.h_max && mod_ < 4))) {
                if (++disari_ >= p_.mod_histerezis_ping) {
                    mod_ += irtifa > md.h_max ? 1 : -1;
                    disari_ = 0;
                }
            } else {
                disari_ = 0;
            }
            if (!cozum) {
                if (++kilitsiz_ >= p_.kilit_arama_ping) {
                    arama_sirasi_kur(mod_);
                    kilitsiz_ = 0;
                    disari_ = 0;
                }
            } else {
                kilitsiz_ = 0;
            }
        }
        // Yakın mod denemesi yalnız irtifa o modun aralığındaysa yapılır [T].
        if (!arama_ && mod_ > 0 && t - son_yakin_ >= p_.yakin_mod_deneme_ns && irtifa >= 0.0 &&
            irtifa <= kDvlModlari[static_cast<std::size_t>(mod_ - 1)].h_max &&
            irtifa >= kDvlModlari[static_cast<std::size_t>(mod_ - 1)].h_min) {
            yakin_deneme_ = true;
        }
        if (t - son_yakin_ >= p_.yakin_mod_deneme_ns && !yakin_deneme_) {
            son_yakin_ = t;
        }
    }

    void rapor(const Baglam& b, const Ping& p, std::vector<TemelKayit>& cikti)
    {
        TemelKayit r;
        r.hdr.sensor_id = a_.kimlik;
        r.hdr.tur = a_.tur;
        r.hdr.inst = a_.inst;
        r.ornek_no = p.seq;
        r.hdr.seq = static_cast<std::uint32_t>(p.seq & 0xFFFFFFFFu);
        r.hdr.t_meas_ns = p.t_meas;
        r.hdr.window_ns = 0;
        r.hdr.t_tx_ns = p.t_tx;
        r.hdr.t_hazir_ns = b.t_ns;
        r.hdr.t_teslim_ns = p.t_teslim;
        r.hdr.flags = kBayrakGecerli;
        r.n_kanal = 8;
        r.beam_valid = p.gecerli;
        r.dvl_flags = kDvlDipIzi;
        for (std::size_t i = 0; i < 4; ++i) {
            r.k[3 + i] = (p.gecerli & (1u << i)) != 0 ? p.menzil[i] : -1.0;
        }
        r.k[7] = -1.0;
        // Işın hızları: t_meas anındaki durumun ara değeri (05 §2.2).
        std::array<double, 6> vw{};
        const bool ara = aralik_ortalamasi<6>(
            *b.halka, adim_ns_, p.t_meas, p.t_meas,
            [](const AdimKaydi& k, std::uint64_t) {
                const auto& d = k.d;
                return std::array<double, 6>{d.dogrusal_hiz_b[0], d.dogrusal_hiz_b[1],
                                             d.dogrusal_hiz_b[2], d.acisal_hiz_b[0],
                                             d.acisal_hiz_b[1], d.acisal_hiz_b[2]};
            },
            vw);
        const Vek3 v_b = topla({vw[0], vw[1], vw[2]}, capraz({vw[3], vw[4], vw[5]}, l_));
        const Vek3 v_d = carp_t(r_bs_, v_b);
        std::array<double, 4> u{};
        std::array<double, 4> sig{};
        for (std::size_t i = 0; i < 4; ++i) {
            const double u0 = olcek_ * nokta(isin_[i], v_d);
            sig[i] = std::sqrt(p_.sigma0 * p_.sigma0 + (p_.k_r * p.menzil[i]) * (p_.k_r * p.menzil[i]) +
                               (p_.k_u * u0) * (p_.k_u * u0));
            u[i] = u0 + sig[i] * olcum_.normal(p.seq, c2_kodla(static_cast<std::uint32_t>(i), 0, 0));
        }
        if (p.cozum && ara) {
            // Ağırlıklı en küçük kareler: v̂ = (HᵀWH)⁻¹ HᵀW u, kovaryans (HᵀWH)⁻¹.
            // Işın σ'larından biri 0 ise (gürültüsüz ayar) ağırlıksız çözülür ve kovaryans yoktur.
            bool agirlikli = true;
            for (std::size_t i = 0; i < 4; ++i) {
                if ((p.gecerli & (1u << i)) != 0 && !(sig[i] > 0.0)) {
                    agirlikli = false;
                }
            }
            Mat3 hwh{};
            Vek3 hwu{};
            for (std::size_t i = 0; i < 4; ++i) {
                if ((p.gecerli & (1u << i)) == 0) {
                    continue;
                }
                const double w = agirlikli ? 1.0 / (sig[i] * sig[i]) : 1.0;
                for (std::size_t a = 0; a < 3; ++a) {
                    hwu[a] += w * isin_[i][a] * u[i];
                    for (std::size_t c = 0; c < 3; ++c) {
                        hwh[a][c] += w * isin_[i][a] * isin_[i][c];
                    }
                }
            }
            Mat3 kov{};
            if (ters(hwh, kov)) {
                const Vek3 v = carp(kov, hwu);
                bool doymus = false;
                for (std::size_t a = 0; a < 3; ++a) {
                    double x = kuantala(v[a], kDvlHizCozunurlugu);
                    if (std::abs(x) > kDvlAzamiHiz) {
                        x = std::copysign(kDvlAzamiHiz, x);
                        doymus = true;
                    }
                    r.k[a] = x;
                    r.sigma_nom[a] = agirlikli ? std::sqrt(kov[a][a]) : 0.0;
                }
                if (agirlikli) {
                    r.dvl_cov = {kov[0][0], kov[0][1], kov[0][2], kov[1][1], kov[1][2], kov[2][2]};
                    r.dvl_fom = std::sqrt(std::max({kov[0][0], kov[1][1], kov[2][2]}));
                    r.dvl_flags = static_cast<std::uint8_t>(r.dvl_flags | kDvlKovaryansVar);
                }
                r.k[7] = p.irtifa;
                r.dvl_flags = static_cast<std::uint8_t>(r.dvl_flags | kDvlHizGecerli);
                if (doymus) {
                    r.hdr.flags |= kBayrakDoymus;
                }
            }
        }
        cikti.push_back(r);
    }

    DvlAyar p_;
    Mat3 r_bs_;
    Vek3 l_;
    double taban_;
    double c_;
    std::int64_t adim_ns_;
    std::array<Vek3, 4> isin_{};
    double olcek_ = 1.0;
    int mod_ = kDvlIlkMod;
    bool arama_ = true;
    std::vector<int> arama_sirasi_;
    std::size_t arama_idx_ = 0;
    int disari_ = 0;
    int kilitsiz_ = 0;
    bool yakin_deneme_ = false;
    std::int64_t son_yakin_ = 0;
    std::int64_t t_ideal_ = 0;
    std::int64_t t_tx_ = 0;
    std::deque<Ping> bekleyen_;
};

// ================================================================ batarya (05 §2.3.6)
class BataryaModeli final : public Model {
public:
    BataryaModeli(const SensorAyari& a, const SensorKumesiAyari& k)
        : Model(a, k.ana_tohum), p_(std::get<BataryaOlcerAyar>(a.ayar)), guc_(k.guc),
          periyot_(periyot_ns(a, 10.0)), adim_ns_(k.adim_ns)
    {
        pencere_ = std::llround(1e9 / a.hiz_hz.value_or(10.0));
        lsb_ = p_.adc_referans / std::ldexp(1.0, p_.adc_bit);
        kod_azami_ = std::ldexp(1.0, p_.adc_bit) - 1.0;
    }

    std::size_t halka_ihtiyaci(std::int64_t h) const override
    {
        return static_cast<std::size_t>(pencere_ / h) + 4;
    }

    void baslat(const Baglam& b) override { zam_.kur(periyot_, adim_ns_, b.t_ns + pencere_); }

    void adim(const Baglam& b, std::vector<TemelKayit>& cikti) override
    {
        if (!zam_.tetik(b.adim_no)) {
            return;
        }
        std::array<double, 2> vi{};
        const bool tamam = aralik_ortalamasi<2>(
            *b.halka, adim_ns_, b.t_ns - pencere_, b.t_ns,
            [this](const AdimKaydi& k, std::uint64_t) {
                return std::array<double, 2>{guc_.v_sabit, paket_akimi(guc_, k.d)};
            },
            vi);
        TemelKayit r = yeni_kayit(b.t_ns, static_cast<std::uint32_t>(pencere_), b.t_ns,
                                  b.t_ns + p_.teslim_gecikmesi_ns);
        bool doymus = false;
        const double va = vi[0] / p_.gerilim_bolucu +
                          p_.analog_gurultu_sigma * olcum_.normal(r.ornek_no, c2_kodla(0, 0, 0));
        const double ia = p_.akim_ofseti + vi[1] / p_.akim_olcegi +
                          p_.analog_gurultu_sigma * olcum_.normal(r.ornek_no, c2_kodla(1, 0, 0));
        const double va_q = adc(va, doymus);
        const double ia_q = adc(ia, doymus);
        r.k[0] = tamam ? va_q * p_.gerilim_bolucu : std::numeric_limits<double>::quiet_NaN();
        r.k[1] = (ia_q - p_.akim_ofseti) * p_.akim_olcegi +
                 p_.sicaklik_kats * (sicaklik(b) - kSicaklikReferansi);
        r.k[2] = sicaklik(b);
        r.sigma_nom[0] = p_.analog_gurultu_sigma * p_.gerilim_bolucu;
        r.sigma_nom[1] = p_.analog_gurultu_sigma * p_.akim_olcegi;
        r.n_kanal = 3;
        if (doymus) {
            r.hdr.flags |= kBayrakDoymus;
        }
        cikti.push_back(r);
    }

private:
    double adc(double x, bool& doymus) const
    {
        double kod = std::round(x / lsb_);
        if (kod < 0.0) {
            kod = 0.0;
            doymus = true;
        } else if (kod > kod_azami_) {
            kod = kod_azami_;
            doymus = true;
        }
        return kod * lsb_;
    }

    BataryaOlcerAyar p_;
    GucParametreleri guc_;
    double periyot_;
    std::int64_t adim_ns_;
    std::int64_t pencere_ = 0;
    double lsb_ = 0.0;
    double kod_azami_ = 0.0;
    Zamanlayici zam_;
};

// ================================================================ sızıntı (05 §2.3.7)
class SizintiModeli final : public Model {
public:
    SizintiModeli(const SensorAyari& a, const SensorKumesiAyari& k)
        : Model(a, k.ana_tohum), p_(std::get<SizintiAyar>(a.ayar)),
          periyot_(periyot_ns(a, 10.0)), adim_ns_(k.adim_ns)
    {
        islak_bas_.assign(static_cast<std::size_t>(p_.prob_sayisi), -1);
    }

    std::size_t halka_ihtiyaci(std::int64_t) const override { return 2; }

    void baslat(const Baglam& b) override { zam_.kur(periyot_, adim_ns_, b.t_ns); }

    void adim(const Baglam& b, std::vector<TemelKayit>& cikti) override
    {
        for (std::size_t i = 0; i < islak_bas_.size(); ++i) {
            const bool islak = ((b.dis->islak_problar >> i) & 1u) != 0;
            if (islak && islak_bas_[i] < 0) {
                islak_bas_[i] = b.t_ns;
            } else if (!islak) {
                islak_bas_[i] = -1;
            }
        }
        if (!zam_.tetik(b.adim_no)) {
            return;
        }
        TemelKayit r = yeni_kayit(b.t_ns, 0, b.t_ns, b.t_ns);
        for (std::size_t i = 0; i < islak_bas_.size(); ++i) {
            const bool algilandi =
                islak_bas_[i] >= 0 && b.t_ns - islak_bas_[i] >= p_.algilama_gecikmesi_ns;
            const bool sahte =
                olcum_.duzgun(r.ornek_no, c2_kodla(static_cast<std::uint32_t>(i), 0, 0)) <
                p_.sahte_pozitif_orani;
            if (algilandi || sahte) {
                r.mask = static_cast<std::uint8_t>(r.mask | (1u << i));
            }
        }
        cikti.push_back(r);
    }

private:
    SizintiAyar p_;
    double periyot_;
    std::int64_t adim_ns_;
    std::vector<std::int64_t> islak_bas_;
    Zamanlayici zam_;
};

// ================================================================ fırlatıcı ve emniyet (05 §2.3.8)
class EmniyetModeli final : public Model {
public:
    EmniyetModeli(const SensorAyari& a, const SensorKumesiAyari& k)
        : Model(a, k.ana_tohum), p_(std::get<FirlaticiGeriOkumaAyar>(a.ayar)),
          periyot_(periyot_ns(a, 10.0)), adim_ns_(k.adim_ns)
    {
    }

    std::size_t halka_ihtiyaci(std::int64_t) const override { return 2; }

    void baslat(const Baglam& b) override
    {
        zam_.kur(periyot_, adim_ns_, b.t_ns);
        const EmniyetDurumu& e = b.dis->emniyet;
        anahtar_ = {e.acil_durdurma, e.fis[0], e.fis[1], e.pim[0], e.pim[1], e.dolu[0], e.dolu[1]};
        bekleyen_.fill(-1);
        onceki_rev_ = e.emniyet_revizyonu;
        son_bitler_ = bitler(e);
    }

    void adim(const Baglam& b, std::vector<TemelKayit>& cikti) override
    {
        const EmniyetDurumu& e = b.dis->emniyet;
        const std::array<bool, 7> yetkili = {e.acil_durdurma, e.fis[0], e.fis[1], e.pim[0],
                                             e.pim[1],        e.dolu[0], e.dolu[1]};
        // Kısıtlayan değer: acil durdurma basılı, fiş çıkık, pim takılı, tüp boş.
        const std::array<bool, 7> kisitlayan = {true, false, false, true, true, false, false};
        for (std::size_t i = 0; i < 7; ++i) {
            if (yetkili[i] == anahtar_[i]) {
                bekleyen_[i] = -1;
            } else if (yetkili[i] == kisitlayan[i]) {
                anahtar_[i] = yetkili[i];   // kısıtlama gecikmesiz algılanır (EMNİYET okuması)
                bekleyen_[i] = -1;
            } else {
                if (bekleyen_[i] < 0) {
                    bekleyen_[i] = b.t_ns + p_.anahtar_gecikmesi_ns;
                    // Gevşetmeden önceki revizyon: algılanana dek örnek bunu taşır (W4-K4, K5).
                    bekleyen_rev_[i] = onceki_rev_;
                }
                if (b.t_ns >= bekleyen_[i]) {
                    anahtar_[i] = yetkili[i];
                    bekleyen_[i] = -1;
                }
            }
        }
        const std::array<std::uint32_t, 4> simdi = bitler(e);
        onceki_rev_ = e.emniyet_revizyonu;
        const bool degisti = simdi != son_bitler_;
        const bool periyodik = zam_.tetik(b.adim_no);
        if (!degisti && !periyodik) {
            return;
        }
        son_bitler_ = simdi;
        TemelKayit r = yeni_kayit(b.t_ns, 0, b.t_ns, b.t_ns + p_.teslim_gecikmesi_ns);
        r.estop = static_cast<std::uint8_t>(simdi[0]);
        r.tube = {static_cast<std::uint8_t>(simdi[1]), static_cast<std::uint8_t>(simdi[2])};
        r.safety_rev = simdi[3];
        cikti.push_back(r);
    }

private:
    // Algılanan anahtarlar, t anındaki izin (A1-K56) ve gösterilen durumun revizyonu (W4-K4, K5;
    // EMNİYET): bekleyen gevşetme yoksa güncel revizyon; varsa en erken bekleyen gevşetmeden
    // önceki revizyon. Bu, o gevşetme yokken yetkili olan revizyondur; arada gecikmesiz
    // algılanmış bir kısıtlama varsa (karışık durum) daha eski revizyon gönderilir: kart o
    // revizyonla izin açamaz (kısıtlayan okuma).
    std::array<std::uint32_t, 4> bitler(const EmniyetDurumu& e) const
    {
        std::array<std::uint32_t, 4> s{};
        s[0] = anahtar_[0] ? 1u : 0u;
        for (std::size_t t = 0; t < 2; ++t) {
            std::uint32_t v = 0;
            v |= anahtar_[5 + t] ? kTupDolu : 0u;
            v |= anahtar_[1 + t] ? kTupFisTakili : 0u;
            v |= anahtar_[3 + t] ? 0u : kTupPimCikik;
            v |= e.izin_acik[t] ? kTupIzinAcik : 0u;
            s[1 + t] = v;
        }
        s[3] = e.emniyet_revizyonu;
        std::int64_t en_erken = -1;
        for (std::size_t i = 0; i < bekleyen_.size(); ++i) {
            if (bekleyen_[i] >= 0 && (en_erken < 0 || bekleyen_[i] < en_erken)) {
                en_erken = bekleyen_[i];
                s[3] = bekleyen_rev_[i];
            }
        }
        return s;
    }

    FirlaticiGeriOkumaAyar p_;
    double periyot_;
    std::int64_t adim_ns_;
    std::array<bool, 7> anahtar_{};       // estop, fis0, fis1, pim0, pim1, dolu0, dolu1
    std::array<std::int64_t, 7> bekleyen_{};
    std::array<std::uint32_t, 7> bekleyen_rev_{};
    std::uint32_t onceki_rev_ = 0;        // önceki adım sınırındaki yetkili revizyon
    std::array<std::uint32_t, 4> son_bitler_{};
    Zamanlayici zam_;
};

}  // namespace

std::unique_ptr<Model> model_olustur(const SensorAyari& a, const SensorKumesiAyari& k,
                                     std::string& hata)
{
    hata.clear();
    switch (a.tur) {
    case SensorTuru::imu:
        return std::make_unique<ImuModeli>(a, k);
    case SensorTuru::manyetometre:
        return std::make_unique<ManyetometreModeli>(a, k);
    case SensorTuru::basinc:
        return std::make_unique<BasincModeli>(a, k);
    case SensorTuru::gnss:
        return std::make_unique<GnssModeli>(a, k);
    case SensorTuru::dvl:
        return std::make_unique<DvlModeli>(a, k);
    case SensorTuru::batarya_olcer:
        return std::make_unique<BataryaModeli>(a, k);
    case SensorTuru::sizinti:
        return std::make_unique<SizintiModeli>(a, k);
    case SensorTuru::firlatici_geri_okuma:
        return std::make_unique<EmniyetModeli>(a, k);
    case SensorTuru::ping1d:
    case SensorTuru::ping360:
    case SensorTuru::kamera:
        break;
    }
    hata = "sensör türü bu kümede yok";
    return nullptr;
}

}  // namespace ida::sensor::ic
