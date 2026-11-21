// sim/sensor iç yapısı: anlık görüntü halkası, zamanlayıcı, temel kayıt ve model tabanı.
#pragma once

#include "ida/sensor/rastgele.hpp"
#include "ida/sensor/sensor_kumesi.hpp"
#include "matematik.hpp"

#include <array>
#include <cmath>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace ida::sensor::ic {

// ---------------------------------------------------------------- anlık görüntü halkası
struct AdimKaydi {
    fizik::DurumAnlik d{};
    double ic_sicaklik = 0.0;   // gövde içi sıcaklık, °C
};

class Halka {
public:
    void kur(std::size_t kapasite)
    {
        kayit_.assign(kapasite, AdimKaydi{});
        dolu_ = 0;
    }
    void ekle(const fizik::DurumAnlik& d, double ic_sicaklik)
    {
        const std::size_t i = static_cast<std::size_t>(d.adim_no % kayit_.size());
        kayit_[i].d = d;
        kayit_[i].ic_sicaklik = ic_sicaklik;
        if (dolu_ == 0) {
            ilk_ = d.adim_no;
        }
        son_ = d.adim_no;
        dolu_ = dolu_ < kayit_.size() ? dolu_ + 1 : dolu_;
        if (son_ - ilk_ + 1 > kayit_.size()) {
            ilk_ = son_ + 1 - kayit_.size();
        }
    }
    // Adım `n`'nin kaydı; halkada yoksa nullptr.
    const AdimKaydi* al(std::uint64_t n) const
    {
        if (dolu_ == 0 || n < ilk_ || n > son_) {
            return nullptr;
        }
        return &kayit_[static_cast<std::size_t>(n % kayit_.size())];
    }
    std::uint64_t ilk() const { return ilk_; }
    std::uint64_t son() const { return son_; }

private:
    std::vector<AdimKaydi> kayit_;
    std::size_t dolu_ = 0;
    std::uint64_t ilk_ = 0;
    std::uint64_t son_ = 0;
};

// Adım sınırlarında örneklenmiş bir büyüklüğün parçalı doğrusal ara değerinin [t0, t1]
// aralığındaki ortalaması (t0 = t1 ise t0'daki ara değer). Adım sınırına denk gelen uçlarda bu,
// 05 §2.3.1'in yamuk ortalamasıdır. `f(const AdimKaydi&, std::uint64_t adim)` → array<double,N>.
// Gerekli adımlardan biri halkada yoksa false.
template <std::size_t N, class F>
bool aralik_ortalamasi(const Halka& h, std::int64_t adim_ns, std::int64_t t0, std::int64_t t1,
                       F&& f, std::array<double, N>& ort)
{
    ort.fill(0.0);
    if (t1 < t0 || t0 < 0) {
        return false;
    }
    const std::uint64_t k0 = static_cast<std::uint64_t>(t0 / adim_ns);
    const std::uint64_t k1 = static_cast<std::uint64_t>((t1 + adim_ns - 1) / adim_ns);
    std::array<double, N> onceki{};
    const AdimKaydi* r0 = h.al(k0);
    if (r0 == nullptr) {
        return false;
    }
    onceki = f(*r0, k0);
    if (t1 == t0) {
        if (k1 == k0) {
            ort = onceki;
            return true;
        }
        const AdimKaydi* r1 = h.al(k0 + 1);
        if (r1 == nullptr) {
            return false;
        }
        const std::array<double, N> sonra = f(*r1, k0 + 1);
        const double a = static_cast<double>(t0 - static_cast<std::int64_t>(k0) * adim_ns) /
                         static_cast<double>(adim_ns);
        for (std::size_t i = 0; i < N; ++i) {
            ort[i] = onceki[i] + a * (sonra[i] - onceki[i]);
        }
        return true;
    }
    const double hd = static_cast<double>(adim_ns);
    for (std::uint64_t k = k0; k < k1; ++k) {
        const AdimKaydi* r = h.al(k + 1);
        if (r == nullptr) {
            return false;
        }
        const std::array<double, N> sonraki = f(*r, k + 1);
        const std::int64_t ta = static_cast<std::int64_t>(k) * adim_ns;
        const std::int64_t a = t0 > ta ? t0 : ta;
        const std::int64_t b = t1 < ta + adim_ns ? t1 : ta + adim_ns;
        if (b > a) {
            const double fa = static_cast<double>(a - ta) / hd;
            const double fb = static_cast<double>(b - ta) / hd;
            const double w = static_cast<double>(b - a);
            for (std::size_t i = 0; i < N; ++i) {
                const double va = onceki[i] + fa * (sonraki[i] - onceki[i]);
                const double vb = onceki[i] + fb * (sonraki[i] - onceki[i]);
                ort[i] += 0.5 * w * (va + vb);
            }
        }
        onceki = sonraki;
    }
    const double boy = static_cast<double>(t1 - t0);
    for (std::size_t i = 0; i < N; ++i) {
        ort[i] /= boy;
    }
    return true;
}

// ---------------------------------------------------------------- periyodik zamanlayıcı
// İdeal anlar t_k = round(k · periyot) (k'dan doğrudan; birikimli sürüklenme yok), her olay en
// yakın adım sınırına bağlanır (yarıda yukarı; 05 §2.2, 04 §2.11).
class Zamanlayici {
public:
    void kur(double periyot_ns, std::int64_t adim_ns, std::int64_t t_alt_ns)
    {
        periyot_ = periyot_ns;
        adim_ns_ = adim_ns;
        k_ = static_cast<std::uint64_t>(std::ceil(static_cast<double>(t_alt_ns) / periyot_ns));
        while (k_ > 0 && ideal(k_ - 1) >= t_alt_ns) {
            --k_;
        }
        while (ideal(k_) < t_alt_ns) {
            ++k_;
        }
        sonraki_ = adim(k_);
    }
    // Olay bu adımdaysa true (ve sonraki olaya geçer). Aynı adıma düşen birden çok ideal an
    // tek olaydır.
    bool tetik(std::uint64_t adim_no)
    {
        if (sonraki_ != adim_no) {
            return false;
        }
        while (adim(k_) <= adim_no) {
            ++k_;
        }
        sonraki_ = adim(k_);
        return true;
    }
    std::int64_t ideal(std::uint64_t k) const
    {
        return static_cast<std::int64_t>(std::llround(static_cast<double>(k) * periyot_));
    }

private:
    std::uint64_t adim(std::uint64_t k) const
    {
        return static_cast<std::uint64_t>((ideal(k) + adim_ns_ / 2) / adim_ns_);
    }
    double periyot_ = 1.0;
    std::int64_t adim_ns_ = 1;
    std::uint64_t k_ = 0;
    std::uint64_t sonraki_ = 0;
};

// İlk adım sınırı ≥ t.
inline std::int64_t ust_sinir(std::int64_t t, std::int64_t adim_ns)
{
    const std::int64_t k = (t + adim_ns - 1) / adim_ns;
    return k * adim_ns;
}

// ---------------------------------------------------------------- temel kayıt
inline constexpr std::size_t kAzamiKanal = 8;

// Ölçüm zincirinin çıktısı (05 §2.1 "temel kayıt"): sayısal kanallar SI ve double; arıza
// zinciri bunun kopyası üzerinde çalışır, sonra A1/A4 biçimli `Olcum`'a çevrilir.
// Kanal numaraları (A5 Ek D.4 `kanal`; sim/sensor/BENIOKU.md):
//   IMU 0–2 gyro, 3–5 accel, 6 temp_c; MAG 0–2 field; BARO 0 p_abs, 1 temp_c;
//   GNSS 0–2 anten konumu N, E, D (orijine göre, m), 3–5 vel_ned; DVL 0–2 vel,
//   3–6 beam_dist, 7 altitude; BATT 0 voltage, 1 current, 2 temp_c; LEAK ve SAFETY kanalsız.
struct TemelKayit {
    OlcumBasligi hdr{};
    std::uint64_t ornek_no = 0;   // model içi örnek numarası (64 bit; RNG sayacı)
    std::array<double, kAzamiKanal> k{};
    std::array<double, kAzamiKanal> sigma_nom{};   // gürültü artışı için örnek başına σ
    std::uint8_t n_kanal = 0;
    // kanal dışı alanlar
    std::array<double, 6> dvl_cov{};
    double dvl_fom = 0.0;
    std::uint8_t dvl_flags = 0;
    std::uint8_t beam_valid = 0;
    std::uint8_t fix_type = 0;
    std::uint8_t n_sv = 0;
    bool gnss_cozum_yok = false;
    double eph = 0.0;
    double epv = 0.0;
    std::uint8_t mask = 0;
    std::uint8_t estop = 0;
    std::array<std::uint8_t, 2> tube{};
    std::uint32_t safety_rev = 0;
};

// ---------------------------------------------------------------- model tabanı
struct Baglam {
    std::uint64_t adim_no = 0;
    std::int64_t t_ns = 0;
    std::int64_t adim_ns = 0;
    std::int64_t kontrol_periyodu_ns = 0;
    const Halka* halka = nullptr;
    const SensorOrtami* ortam = nullptr;
    const GucParametreleri* guc = nullptr;
    const DisDurum* dis = nullptr;
    double ic_sicaklik = 0.0;
};

// 04 §2.7.4 akım yasası: I = I_maks(V) · (|F| / F_maks,yön(V))^p; 7 V altında 0; 20 V üstünde
// son değer; tablonun alt ucu altında doğrusal dışdeğerleme.
double itici_akimi(const GucParametreleri& g, double gerilim, double itki);
// Paket akımı: Σ itici akımı (itki isteğinden; fizik güç modeli yok, tahmin) + aviyonik P/V.
double paket_akimi(const GucParametreleri& g, const fizik::DurumAnlik& d);
// Bir iticinin tam gazdaki akımı (I_maks(V)).
double tam_gaz_akimi(const GucParametreleri& g, double gerilim);

class Model {
public:
    Model(const SensorAyari& a, std::uint64_t tohum);
    virtual ~Model();
    Model(const Model&) = delete;
    Model& operator=(const Model&) = delete;

    // İlk adımda bir kez (zamanlayıcıların alt sınırı t_ilk).
    virtual void baslat(const Baglam& b) = 0;
    virtual void adim(const Baglam& b, std::vector<TemelKayit>& cikti) = 0;
    // Gereken geçmiş (adım sayısı).
    virtual std::size_t halka_ihtiyaci(std::int64_t adim_ns) const = 0;

    const SensorAyari& ayar() const { return a_; }
    // Model içi örnek sayacını kurar (yalnız testler: 2³² sarımının sınanması).
    void ornek_sayaci_kur(std::uint64_t n) { ornek_ = n; }

protected:
    TemelKayit yeni_kayit(std::int64_t t_meas, std::uint32_t window, std::int64_t t_hazir,
                          std::int64_t t_teslim);

    SensorAyari a_;
    Akis acilis_;
    Akis olcum_;
    Akis zamanlama_;
    // Model içi örnek sayacı (A5-K110 `n`, 64 bit); teldeki seq bunun 2³² kalanıdır.
    std::uint64_t ornek_ = 0;
};

// Türüne göre modeli kurar; ayar hatası varsa nullptr ve `hata`.
std::unique_ptr<Model> model_olustur(const SensorAyari& a, const SensorKumesiAyari& k,
                                     std::string& hata);

}  // namespace ida::sensor::ic
