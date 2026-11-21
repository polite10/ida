#include "itici.hpp"

#include <Eigen/Dense>

#include <algorithm>
#include <cmath>

namespace ida::fizik::ic {

namespace {

std::string bicim(const char* kural, const char* ileti)
{
    return std::string(kural) + ": " + ileti;
}

bool kesin_artan(const std::vector<double>& v)
{
    for (std::size_t i = 1; i < v.size(); ++i) {
        if (!(v[i] > v[i - 1])) {
            return false;
        }
    }
    return true;
}

bool hepsi_sonlu(const std::vector<double>& v)
{
    return std::all_of(v.begin(), v.end(), [](double deger) { return std::isfinite(deger); });
}

// Bir dizinin elemanlarını 2'nin bir kuvvetiyle (tam, yuvarlamasız) bölerek en büyük mutlak
// değeri [0,5, 1) aralığına getirir. Sıfır dizide değişiklik yok.
void ikinin_kuvvetiyle_olcekle(std::vector<double>& v)
{
    double enb = 0.0;
    for (double x : v) {
        enb = std::max(enb, std::abs(x));
    }
    if (enb == 0.0 || !std::isfinite(enb)) {
        return;
    }
    int us = 0;
    std::frexp(enb, &us);
    for (auto& x : v) {
        x = std::ldexp(x, -us);
    }
}

enum class Routh { kararli, kararsiz, hesaplanamadi };

// Routh–Hurwitz: p[0] s^n + … + p[n]; bütün kökler sol yarı düzlemde mi. Katsayılar ve her
// tablo satırı 2'nin kuvvetleriyle ölçeklenir (pozitif ölçek ilk sütunun işaretlerini
// değiştirmez); böylece aynı polinomun büyük ya da küçük ortak ölçekli yazımları aynı sonucu
// verir. Ara değerlerden biri sonlu değilse sonuç "hesaplanamadı"dır.
Routh hurwitz_kararli(const std::vector<double>& p_ham)
{
    std::vector<double> p = p_ham;
    if (p.empty() || p[0] == 0.0) {
        return Routh::kararsiz;
    }
    for (double x : p) {
        if (!std::isfinite(x)) {
            return Routh::hesaplanamadi;
        }
    }
    if (p[0] < 0.0) {
        for (auto& x : p) {
            x = -x;
        }
    }
    ikinin_kuvvetiyle_olcekle(p);
    const std::size_t n = p.size() - 1;
    if (n == 0) {
        return Routh::kararli;
    }
    for (double x : p) {
        if (!(x > 0.0)) {
            return Routh::kararsiz;  // gerekli koşul
        }
    }
    // Routh tablosu.
    const std::size_t sutun = (n + 2) / 2;
    std::vector<std::vector<double>> t(n + 1, std::vector<double>(sutun + 1, 0.0));
    for (std::size_t j = 0; j < sutun; ++j) {
        if (2 * j < p.size()) {
            t[0][j] = p[2 * j];
        }
        if (2 * j + 1 < p.size()) {
            t[1][j] = p[2 * j + 1];
        }
    }
    ikinin_kuvvetiyle_olcekle(t[0]);
    ikinin_kuvvetiyle_olcekle(t[1]);
    for (std::size_t i = 2; i <= n; ++i) {
        if (!(t[i - 1][0] > 0.0)) {
            return Routh::kararsiz;
        }
        for (std::size_t j = 0; j < sutun; ++j) {
            t[i][j] = (t[i - 1][0] * t[i - 2][j + 1] - t[i - 2][0] * t[i - 1][j + 1]) / t[i - 1][0];
            if (!std::isfinite(t[i][j])) {
                return Routh::hesaplanamadi;
            }
        }
        ikinin_kuvvetiyle_olcekle(t[i]);
    }
    for (std::size_t i = 0; i <= n; ++i) {
        if (!(t[i][0] > 0.0)) {
            return Routh::kararsiz;
        }
    }
    return Routh::kararli;
}

using MatX = Eigen::Matrix<double, Eigen::Dynamic, Eigen::Dynamic, 0, kAzamiFiltreDerecesi + 1,
                           kAzamiFiltreDerecesi + 1>;

// Matris üsteli: ölçekleme ve kare alma + 30 terimli Taylor (ölçeklenmiş normu ≤ 0,5).
MatX matris_usteli(const MatX& Z)
{
    double norm = 0.0;
    for (int i = 0; i < Z.rows(); ++i) {
        norm = std::max(norm, Z.row(i).cwiseAbs().sum());
    }
    int s = 0;
    if (norm > 0.5) {
        s = static_cast<int>(std::ceil(std::log2(norm / 0.5)));
    }
    s = std::clamp(s, 0, 4000);
    const MatX A = Z * std::ldexp(1.0, -s);
    MatX E = MatX::Identity(Z.rows(), Z.cols());
    MatX terim = MatX::Identity(Z.rows(), Z.cols());
    for (int k = 1; k <= 30; ++k) {
        terim = (terim * A) / static_cast<double>(k);
        E += terim;
    }
    for (int i = 0; i < s; ++i) {
        E = E * E;
    }
    return E;
}

}  // namespace

std::string IticiModeliIc::kur(const IticiModeli& m, const Esc& esc)
{
    if (m.tur != "t200_gecici") {
        return bicim("A5-K102", "arac.itici_modeli.tur yalnız \"t200_gecici\" olabilir");
    }
    const std::size_t nv = m.gerilim_noktalari.size();
    if (nv < 2 || nv > 8 || m.azami_itki_ileri.size() != nv || m.azami_itki_geri.size() != nv ||
        m.tam_gaz_akimi.size() != nv) {
        return bicim("A5-K30", "gerilim noktaları ve azami itki dizileri 2–8 elemanlı ve aynı uzunlukta olmalı");
    }
    if (!kesin_artan(m.gerilim_noktalari) || !hepsi_sonlu(m.gerilim_noktalari) ||
        m.gerilim_noktalari.front() <= 0.0) {
        return bicim("A5-K30", "gerilim_noktalari pozitif ve kesin artan olmalı");
    }
    for (std::size_t i = 0; i < nv; ++i) {
        if (!(m.azami_itki_ileri[i] > 0.0) || !(m.azami_itki_geri[i] > 0.0) ||
            !std::isfinite(m.azami_itki_ileri[i]) || !std::isfinite(m.azami_itki_geri[i]) ||
            !(m.tam_gaz_akimi[i] > 0.0) || !std::isfinite(m.tam_gaz_akimi[i])) {
            return bicim("A5 §3.3", "azami itki ve tam gaz akımı değerleri pozitif ve sonlu olmalı");
        }
    }
    if (!(m.akim_ussu > 0.0) || !std::isfinite(m.akim_ussu)) {
        return bicim("A5 §3.3", "akim_ussu pozitif ve sonlu olmalı");
    }
    if (m.havalanma_h_bolu_r.size() < 2 || m.havalanma_h_bolu_r.size() > 32 ||
        m.havalanma_beta.size() != m.havalanma_h_bolu_r.size() || !kesin_artan(m.havalanma_h_bolu_r) ||
        !hepsi_sonlu(m.havalanma_h_bolu_r)) {
        return bicim("A5-K30", "havalanma tablosu 2–32 elemanlı, aynı uzunlukta ve h/R kesin artan olmalı");
    }
    for (double b : m.havalanma_beta) {
        if (!(b >= 0.0 && b <= 1.0)) {
            return bicim("A5 §3.3", "havalanma_beta [0, 1] aralığında olmalı");
        }
    }
    if (!(m.pervane_yaricapi > 0.0) || !std::isfinite(m.pervane_yaricapi)) {
        return bicim("A5 §3.3", "pervane_yaricapi pozitif ve sonlu olmalı");
    }
    if (!(esc.pwm_min_us < esc.pwm_notr_us - esc.olu_bant_us &&
          esc.pwm_notr_us + esc.olu_bant_us < esc.pwm_max_us) ||
        esc.olu_bant_us < 0) {
        return bicim("A5-K31", "pwm_min < notr − olu_bant ve notr + olu_bant < pwm_max olmalı");
    }
    if (esc.baslatma_suresi_ns < 0) {
        return bicim("A5 §3.3", "baslatma_suresi_ns ≥ 0 olmalı");
    }

    c_ = m.sekil_polinomu;
    // A5-K104: N = max_j F(j/1000); her j ≥ 1 için F > 0.
    double N = -HUGE_VAL;
    for (int j = 0; j <= 1000; ++j) {
        const double f = polinom(static_cast<double>(j) / 1000.0);
        if (!std::isfinite(f) || (j >= 1 && !(f > 0.0))) {
            return bicim("A5-K104", "sekil_polinomu (0, 1] aralığında pozitif olmalı");
        }
        N = std::max(N, f);
    }
    if (!(N > 0.0) || !std::isfinite(N)) {
        return bicim("A5-K104", "normalleştirme böleni sonlu ve pozitif olmalı");
    }
    N_ = N;
    // F'nin [0, 1]'deki yerel en büyükleri: F' işaret değişimi + ikiye bölme.
    tepeler_.clear();
    tepe_degeri_.clear();
    const int bolme = 4000;
    for (int j = 0; j < bolme; ++j) {
        double lo = static_cast<double>(j) / bolme;
        double hi = static_cast<double>(j + 1) / bolme;
        const double dlo = polinom_turevi(lo);
        const double dhi = polinom_turevi(hi);
        if (dlo > 0.0 && dhi <= 0.0) {
            for (int k = 0; k < 200 && hi - lo > 0.0; ++k) {
                const double orta = 0.5 * (lo + hi);
                if (orta <= lo || orta >= hi) {
                    break;
                }
                if (polinom_turevi(orta) > 0.0) {
                    lo = orta;
                } else {
                    hi = orta;
                }
            }
            tepeler_.push_back(lo);
            tepe_degeri_.push_back(polinom(lo));
        }
    }

    V_ = m.gerilim_noktalari;
    Fi_ = m.azami_itki_ileri;
    Fg_ = m.azami_itki_geri;
    hr_ = m.havalanma_h_bolu_r;
    beta_ = m.havalanma_beta;
    R_ = m.pervane_yaricapi;
    notr_ = static_cast<double>(esc.pwm_notr_us);
    olu_ = static_cast<double>(esc.olu_bant_us);
    min_ = static_cast<double>(esc.pwm_min_us);
    max_ = static_cast<double>(esc.pwm_max_us);

    // A5-K105: transfer fonksiyonu.
    const auto& pay = m.transfer_pay;
    const auto& payda = m.transfer_payda;
    if (payda.size() < 2 || payda.size() > 5 || pay.empty() || pay.size() > 4 ||
        pay.size() >= payda.size() || !hepsi_sonlu(pay) || !hepsi_sonlu(payda)) {
        return bicim("A5-K105", "transfer_pay 1–4, transfer_payda 2–5 eleman ve pay paydadan kısa olmalı");
    }
    if (payda.front() == 0.0 || payda.back() == 0.0) {
        return bicim("A5-K105", "paydanın ilk ve son katsayısı sıfır olamaz");
    }
    if (std::abs(pay.back() / payda.back() - 1.0) > 1e-12) {
        return bicim("A5-K105", "DC kazanç 1 olmalı (pay[son]/payda[son])");
    }
    switch (hurwitz_kararli(payda)) {
    case Routh::kararli:
        break;
    case Routh::kararsiz:
        return bicim("A5-K105", "payda Routh–Hurwitz ölçütüne göre kararlı olmalı");
    case Routh::hesaplanamadi:
        return bicim("A5-K105", "payda Routh–Hurwitz tablosu sayısal olarak hesaplanamadı (Routh)");
    }
    n_ = static_cast<int>(payda.size()) - 1;
    a_.fill(0.0);
    b_.fill(0.0);
    const double a0 = payda.front();
    for (int k = 1; k <= n_; ++k) {
        a_[static_cast<std::size_t>(k - 1)] = payda[static_cast<std::size_t>(k)] / a0;
    }
    const int np = static_cast<int>(pay.size());
    for (int i = 0; i < np; ++i) {
        // pay[np−1−i] s^i katsayısıdır.
        b_[static_cast<std::size_t>(i)] = pay[static_cast<std::size_t>(np - 1 - i)] / a0;
    }
    for (int i = 0; i < kAzamiFiltreDerecesi; ++i) {
        const auto u = static_cast<std::size_t>(i);
        if (!std::isfinite(a_[u]) || !std::isfinite(b_[u])) {
            return bicim("A5-K105", "transfer fonksiyonu katsayıları baş katsayıya bölününce sonlu değil");
        }
    }
    return {};
}

double IticiModeliIc::polinom(double u) const
{
    const double u2 = u * u;
    // c0 u + c1 u³ + … + c4 u⁹ (Horner, u²'de)
    double t = c_[4];
    t = t * u2 + c_[3];
    t = t * u2 + c_[2];
    t = t * u2 + c_[1];
    t = t * u2 + c_[0];
    return t * u;
}

double IticiModeliIc::polinom_turevi(double u) const
{
    const double u2 = u * u;
    double t = 9.0 * c_[4];
    t = t * u2 + 7.0 * c_[3];
    t = t * u2 + 5.0 * c_[2];
    t = t * u2 + 3.0 * c_[1];
    t = t * u2 + c_[0];
    return t;
}

double IticiModeliIc::pwm_den_u(double pwm_us) const
{
    const double d = pwm_us - notr_;
    if (std::abs(d) <= olu_) {
        return 0.0;
    }
    if (d > 0.0) {
        return std::min(d / (max_ - notr_), 1.0);
    }
    return std::max(d / (notr_ - min_), -1.0);
}

double IticiModeliIc::sekil(double u_mutlak) const
{
    const double u = std::clamp(u_mutlak, 0.0, 1.0);
    double f = polinom(u);
    for (std::size_t i = 0; i < tepeler_.size(); ++i) {
        if (tepeler_[i] <= u) {
            f = std::max(f, tepe_degeri_[i]);
        }
    }
    return f / N_;
}

double IticiModeliIc::azami_itki(double v_bara, bool ileri) const
{
    const std::vector<double>& F = ileri ? Fi_ : Fg_;
    if (!(v_bara >= 7.0)) {
        return 0.0;  // ESC çalışma alanı dışı (04 §2.7.4)
    }
    const std::size_t n = V_.size();
    if (v_bara >= V_[n - 1]) {
        return F[n - 1];  // 20–26 V: 20 V değerleri (04 §2.7.4)
    }
    std::size_t i = 0;  // [V_i, V_{i+1}] parçası; V < V_0 ise ilk parçanın yasası (dışdeğerleme)
    while (i + 2 < n && v_bara > V_[i + 1]) {
        ++i;
    }
    // Parça başına kuvvet yasası F = F_i (V/V_i)^p (04 §2.7.2; D04 Y12).
    const double p = std::log(F[i + 1] / F[i]) / std::log(V_[i + 1] / V_[i]);
    return F[i] * std::pow(v_bara / V_[i], p);
}

double IticiModeliIc::durgun_itki(double pwm_us, double v_bara) const
{
    const double u = pwm_den_u(pwm_us);
    if (u == 0.0) {
        return 0.0;
    }
    const bool ileri = u > 0.0;
    const double f = sekil(std::abs(u)) * azami_itki(v_bara, ileri);
    return ileri ? f : -f;
}

double IticiModeliIc::havalanma(double h) const
{
    const double x = h / R_;
    const std::size_t n = hr_.size();
    if (x <= hr_[0]) {
        return beta_[0];
    }
    if (x >= hr_[n - 1]) {
        return beta_[n - 1];
    }
    std::size_t i = 0;
    while (i + 1 < n && x > hr_[i + 1]) {
        ++i;
    }
    const double t = (x - hr_[i]) / (hr_[i + 1] - hr_[i]);
    return beta_[i] + t * (beta_[i + 1] - beta_[i]);
}

double IticiModeliIc::filtre_cikisi(const double* x) const
{
    double y = 0.0;
    for (int i = 0; i < n_; ++i) {
        y += b_[static_cast<std::size_t>(i)] * x[i];
    }
    return y;
}

bool IticiModeliIc::notr_mu(double pwm_us) const
{
    return std::abs(pwm_us - notr_) <= olu_;
}

std::string IticiModeliIc::ayriklastir(double h)
{
    // Genişletilmiş sistem Z = [[A, B], [0, 0]] τ; e^Z = [[Φ(τ), Γ(τ)], [0, 1]].
    const int n = n_;
    MatX Zb = MatX::Zero(n + 1, n + 1);
    for (int i = 0; i + 1 < n; ++i) {
        Zb(i, i + 1) = 1.0;
    }
    for (int k = 0; k < n; ++k) {
        Zb(n - 1, k) = -a_[static_cast<std::size_t>(n - 1 - k)];
    }
    Zb(n - 1, n) = 1.0;
    for (int yarim = 0; yarim < 2; ++yarim) {
        const double tau = yarim == 0 ? h / 2.0 : h;
        const MatX E = matris_usteli(Zb * tau);
        auto& fi = fi_[static_cast<std::size_t>(yarim)];
        auto& ga = ga_[static_cast<std::size_t>(yarim)];
        fi.fill(0.0);
        ga.fill(0.0);
        for (int i = 0; i < n; ++i) {
            for (int j = 0; j < n; ++j) {
                fi[static_cast<std::size_t>(i * kAzamiFiltreDerecesi + j)] = E(i, j);
            }
            ga[static_cast<std::size_t>(i)] = E(i, n);
        }
        if (!E.allFinite()) {
            return bicim("A5-K105", "itici filtresinin ayrık geçiş matrisi sonlu değil");
        }
    }
    return {};
}

void IticiModeliIc::filtre_ilerlet(const double* x, double girdi, bool yarim_adim, double* x_yeni) const
{
    const auto& fi = fi_[yarim_adim ? 0 : 1];
    const auto& ga = ga_[yarim_adim ? 0 : 1];
    for (int i = 0; i < n_; ++i) {
        double t = ga[static_cast<std::size_t>(i)] * girdi;
        for (int j = 0; j < n_; ++j) {
            t += fi[static_cast<std::size_t>(i * kAzamiFiltreDerecesi + j)] * x[j];
        }
        x_yeni[i] = t;
    }
}

}  // namespace ida::fizik::ic
