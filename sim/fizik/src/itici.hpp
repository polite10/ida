// İtici ve motor sürücü modeli (04 §2.7.1–§2.7.3; A5 §3.3): PWM → u → durağan itki isteği
// F_ss → birim DC kazançlı transfer fonksiyonu K(s) → havalanma çarpanı β(h/R).
#pragma once

#include "ida/fizik/parametreler.hpp"

#include <array>
#include <string>
#include <vector>

namespace ida::fizik::ic {

inline constexpr int kAzamiFiltreDerecesi = 4;  // A5-K105: payda en çok 5 katsayı

class IticiModeliIc {
public:
    // Doğrular ve önceden hesaplar; hata varsa boş olmayan ileti döner.
    std::string kur(const IticiModeli& m, const Esc& esc);

    // PWM (µs) → normalleştirilmiş komut u ∈ [−1, 1]; ölü bantta 0 (04 §2.7.1).
    double pwm_den_u(double pwm_us) const;
    // Monoton kırpılmış şekil F_şekil(|u|) ∈ [0, ~1] (A5-K104).
    double sekil(double u_mutlak) const;
    // Bara gerilimindeki azami itki (N), yön: true ileri; 7 V altında 0 (04 §2.7.4).
    double azami_itki(double v_bara, bool ileri) const;
    // Durağan itki isteği F_ss (N).
    double durgun_itki(double pwm_us, double v_bara) const;
    // Havalanma çarpanı β(h/R); h itici konumunun yüzeyden derinliği (m).
    double havalanma(double h) const;

    // Nötr komut mu (A5 §3.3: pwm_notr_us ± olu_bant_us, kenarlar dahil).
    bool notr_mu(double pwm_us) const;

    int filtre_derecesi() const { return n_; }
    // Kontrol edilebilir kanonik biçim: ẋ = A x + B F_ss, F = C x.
    double filtre_cikisi(const double* x) const;
    // Adım içi tam ayrıklaştırma (sıfırıncı derece tutulan F_ss ile): x(τ) = Φ(τ) x + Γ(τ) F_ss,
    // τ ∈ {h/2, h}. Φ, Γ kurulumda matris üsteliyle bir kez hesaplanır; filtre her kararlı K(s)
    // için adım büyüklüğünden bağımsız olarak kararlı ve tam ilerler. Sonlu değilse ileti döner.
    std::string ayriklastir(double h);
    void filtre_ilerlet(const double* x, double girdi, bool yarim_adim, double* x_yeni) const;

    // Normalleştirme böleni N (A5-K104) ve şekil eğrisinin tepe noktaları (test için).
    double sekil_boleni() const { return N_; }

private:
    double polinom(double u) const;
    double polinom_turevi(double u) const;

    std::array<double, 5> c_{};
    double N_ = 1.0;
    std::vector<double> tepeler_;      // F'nin [0, 1]'deki yerel en büyükleri (u)
    std::vector<double> tepe_degeri_;  // o noktalardaki F
    std::vector<double> V_, Fi_, Fg_;
    std::vector<double> hr_, beta_;
    double R_ = 1.0;
    double notr_ = 1500.0, olu_ = 25.0, min_ = 1100.0, max_ = 1900.0;
    int n_ = 0;
    std::array<double, kAzamiFiltreDerecesi> a_{};  // s^n + a1 s^(n−1) + … + an  -> a_[k] = a_{k+1}
    std::array<double, kAzamiFiltreDerecesi> b_{};  // b_[i]: s^i katsayısı
    // Φ ve Γ, satır düzeni; [0]: h/2, [1]: h.
    std::array<std::array<double, kAzamiFiltreDerecesi * kAzamiFiltreDerecesi>, 2> fi_{};
    std::array<std::array<double, kAzamiFiltreDerecesi>, 2> ga_{};
};

}  // namespace ida::fizik::ic
