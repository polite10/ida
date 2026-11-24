// Ses hızı ve soğurma (akustik.hpp).
#include "ida/sonar/akustik.hpp"

#include <cmath>

namespace ida::sonar {

double ses_hizi_unesco_t68(double t68_c, double s_ppt, double p_bar)
{
    // Fofonoff ve Millard (1983), UNESCO Tech. Pap. Mar. Sci. 44, SVEL; katsayılar MBARI
    // SW_svel.txt ile karşılaştırıldı (sim/sonar/BENIOKU.md). Denetim değeri: S = 40, T68 = 40,
    // P = 1000 bar → 1731,995 m/s.
    const double t = t68_c;
    const double s = s_ppt;
    const double p = p_bar;
    const double t2 = t * t, t3 = t2 * t, t4 = t3 * t, t5 = t4 * t;

    const double c00 = 1402.388, c01 = 5.03711, c02 = -5.80852e-2, c03 = 3.3420e-4,
                 c04 = -1.47800e-6, c05 = 3.1464e-9;
    const double c10 = 0.153563, c11 = 6.8982e-4, c12 = -8.1788e-6, c13 = 1.3621e-7,
                 c14 = -6.1185e-10;
    const double c20 = 3.1260e-5, c21 = -1.7107e-6, c22 = 2.5974e-8, c23 = -2.5335e-10,
                 c24 = 1.0405e-12;
    const double c30 = -9.7729e-9, c31 = 3.8504e-10, c32 = -2.3643e-12;
    const double a00 = 1.389, a01 = -1.262e-2, a02 = 7.164e-5, a03 = 2.006e-6, a04 = -3.21e-8;
    const double a10 = 9.4742e-5, a11 = -1.2580e-5, a12 = -6.4885e-8, a13 = 1.0507e-8,
                 a14 = -2.0122e-10;
    const double a20 = -3.9064e-7, a21 = 9.1041e-9, a22 = -1.6002e-10, a23 = 7.988e-12;
    const double a30 = 1.100e-10, a31 = 6.649e-12, a32 = -3.389e-13;
    const double b00 = -1.922e-2, b01 = -4.42e-5, b10 = 7.3637e-5, b11 = 1.7945e-7;
    const double d00 = 1.727e-3, d10 = -7.9836e-6;

    const double cw = (c00 + c01 * t + c02 * t2 + c03 * t3 + c04 * t4 + c05 * t5) +
                      (c10 + c11 * t + c12 * t2 + c13 * t3 + c14 * t4) * p +
                      (c20 + c21 * t + c22 * t2 + c23 * t3 + c24 * t4) * p * p +
                      (c30 + c31 * t + c32 * t2) * p * p * p;
    const double a = (a00 + a01 * t + a02 * t2 + a03 * t3 + a04 * t4) +
                     (a10 + a11 * t + a12 * t2 + a13 * t3 + a14 * t4) * p +
                     (a20 + a21 * t + a22 * t2 + a23 * t3) * p * p +
                     (a30 + a31 * t + a32 * t2) * p * p * p;
    const double b = b00 + b01 * t + (b10 + b11 * t) * p;
    const double d = d00 + d10 * p;
    return cw + a * s + b * s * std::sqrt(s) + d * s * s;
}

double ses_hizi_chen_millero(double t90_c, double s_ppt, double p_bar)
{
    return ses_hizi_unesco_t68(1.00024 * t90_c, s_ppt, p_bar);
}

double ses_hizi_mackenzie(double t_c, double s_ppt, double d_m)
{
    // Mackenzie (1981), JASA 70(3) 807–812; katsayılar MBARI Sndspd.txt ile karşılaştırıldı.
    const double t = t_c;
    const double ds = s_ppt - 35.0;
    const double d = d_m;
    return 1448.96 + 4.591 * t - 5.304e-2 * t * t + 2.374e-4 * t * t * t + 1.340 * ds +
           1.630e-2 * d + 1.675e-7 * d * d - 1.025e-2 * t * ds - 7.139e-13 * t * d * d * d;
}

SesHizi ortam_ses_hizi(double sicaklik_c, double tuzluluk, const std::optional<double>& sabit,
                       double z_m, double yogunluk, double yercekimi)
{
    if (sabit) {
        return {*sabit, SesHiziKaynagi::sabit, false};
    }
    const double s_ppt = tuzluluk * 1000.0;
    const double z = z_m > 0.0 ? z_m : 0.0;
    auto icinde = [](double x, double lo, double hi) { return x >= lo && x <= hi; };
    if (s_ppt >= kDenizSuyuEsigiPpt) {
        const bool alan = icinde(sicaklik_c, 2.0, 30.0) && icinde(s_ppt, 25.0, 40.0) && icinde(z, 0.0, 8000.0);
        return {ses_hizi_mackenzie(sicaklik_c, s_ppt, z), SesHiziKaynagi::mackenzie, !alan};
    }
    const double p_bar = yogunluk * yercekimi * z * 1e-5;
    const bool alan = icinde(sicaklik_c, 0.0, 40.0) && icinde(s_ppt, 0.0, 40.0) && icinde(p_bar, 0.0, 1000.0);
    return {ses_hizi_chen_millero(sicaklik_c, s_ppt, p_bar), SesHiziKaynagi::chen_millero, !alan};
}

const char* ses_hizi_uyarisi(const SesHizi& s)
{
    if (!s.dislama) {
        return "";
    }
    return s.kaynak == SesHiziKaynagi::mackenzie
               ? "ortam.su: ses hızı Mackenzie (1981) geçerlilik alanı (2–30 °C, 25–40 ‰, 0–8000 m) dışında; "
                 "doğrulanmamış dışdeğerleme"
               : "ortam.su: ses hızı UNESCO/Chen–Millero geçerlilik alanı (0–40 °C, 0–40 ‰, 0–1000 bar) dışında; "
                 "doğrulanmamış dışdeğerleme";
}

double sogurma_db_m(double f_hz, double t_c, double tuzluluk, double z_m, double ph)
{
    // 05 §2.4.2: f kHz, T °C, S ‰, z km; sonuç dB/km. Doğrulanmadı (05 §2.4.2).
    const double f = f_hz * 1e-3;
    const double s = tuzluluk * 1000.0;
    const double z = z_m * 1e-3;
    const double f2k = f * f;
    double a = 0.0;
    if (s > 0.0) {
        const double f1 = 0.78 * std::sqrt(s / 35.0) * std::exp(t_c / 26.0);
        const double f2 = 42.0 * std::exp(t_c / 17.0);
        a += 0.106 * f1 * f2k / (f1 * f1 + f2k) * std::exp((ph - 8.0) / 0.56);
        a += 0.52 * (1.0 + t_c / 43.0) * (s / 35.0) * f2 * f2k / (f2 * f2 + f2k) * std::exp(-z / 6.0);
    }
    a += 0.00049 * f2k * std::exp(-(t_c / 27.0 + z / 17.0));
    return a / 1000.0;
}

}  // namespace ida::sonar
