#include "ida/viz/dalga.hpp"

#include <cmath>

namespace ida::viz {

namespace {
constexpr double kIkiPi = 6.283185307179586476925286766559;
}

double dalga_fazi(const a2::DalgaBileseni& b, std::int64_t t_ns) {
  // ω·t'yi tam saniye ve kesir olarak ayırıp her parçayı ayrı indirgemek uzun koşularda
  // (saatlerce) double'ın göreli hatasını faz üzerinde büyütmez.
  const std::int64_t saniye = t_ns / 1'000'000'000;
  const std::int64_t kalan = t_ns % 1'000'000'000;
  const double f1 = std::fmod(b.omega * static_cast<double>(saniye), kIkiPi);
  const double f2 = b.omega * (static_cast<double>(kalan) * 1e-9);
  double f = std::fmod(f1 + f2 + b.phi, kIkiPi);
  if (f < 0.0) f += kIkiPi;
  return f;
}

double dalga_yuksekligi(const std::vector<a2::DalgaBileseni>& bilesenler, double n, double e, std::int64_t t_ns) {
  double z = 0.0;
  for (const auto& b : bilesenler) {
    const double uzay = b.k * (n * std::cos(b.beta) + e * std::sin(b.beta));
    z += b.a * std::cos(dalga_fazi(b, t_ns) - uzay);
  }
  return z;
}

void dalga_egimi(const std::vector<a2::DalgaBileseni>& bilesenler, double n, double e, std::int64_t t_ns,
                 double& dzeta_dn, double& dzeta_de) {
  dzeta_dn = 0.0;
  dzeta_de = 0.0;
  for (const auto& b : bilesenler) {
    const double cb = std::cos(b.beta), sb = std::sin(b.beta);
    const double s = std::sin(dalga_fazi(b, t_ns) - b.k * (n * cb + e * sb));
    // d/dN cos(θ − k(N cb + E sb)) = sin(·) · k cb
    dzeta_dn += b.a * b.k * cb * s;
    dzeta_de += b.a * b.k * sb * s;
  }
}

void dalga_paketle(const std::vector<a2::DalgaBileseni>& bilesenler, std::int64_t t_ns, DalgaPaketi& out) {
  const std::size_t n = bilesenler.size();
  out.kx.resize(n);
  out.kz.resize(n);
  out.a.resize(n);
  out.k.resize(n);
  out.faz.resize(n);
  for (std::size_t i = 0; i < n; ++i) {
    const auto& b = bilesenler[i];
    out.kx[i] = static_cast<float>(b.k * std::sin(b.beta));
    out.kz[i] = static_cast<float>(-b.k * std::cos(b.beta));
    out.a[i] = static_cast<float>(b.a);
    out.k[i] = static_cast<float>(b.k);
    out.faz[i] = static_cast<float>(dalga_fazi(b, t_ns));
  }
}

float dalga_paketi_yuksekligi(const DalgaPaketi& p, float x_g, float z_g) {
  float h = 0.0f;
  for (std::size_t i = 0; i < p.a.size(); ++i) {
    const float th = p.faz[i] - (p.kx[i] * x_g + p.kz[i] * z_g);
    h += p.a[i] * std::cos(th);
  }
  return h;
}

}  // namespace ida::viz
