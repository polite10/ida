#include "sahne.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <stdexcept>

#include "rng.hpp"

namespace mp7 {

double norm(V3 a) { return std::sqrt(dot(a, a)); }
V3 normalize(V3 a) {
  const double n = norm(a);
  return n > 0 ? a * (1.0 / n) : a;
}

namespace {

constexpr double PI = 3.141592653589793;

struct SahneRng {
  Key k;
  uint32_t n = 0;
  double u() { return duzgun(philox({n++, 0, 0, 0}, k)[0]); }
  double ara(double a, double b) { return a + (b - a) * u(); }
};

// Taban yüksekliği: tohumlu sinüs toplamı (ortalama -8 m).
struct Taban {
  double A[6], kx[6], ky[6], ph[6];
  explicit Taban(SahneRng& r) {
    for (int i = 0; i < 6; ++i) {
      const double lam = r.ara(3.0, 30.0);
      const double yon = r.ara(0, 2 * PI);
      A[i] = r.ara(0.05, 0.3);
      kx[i] = 2 * PI / lam * std::cos(yon);
      ky[i] = 2 * PI / lam * std::sin(yon);
      ph[i] = r.ara(0, 2 * PI);
    }
  }
  double h(double x, double y) const {
    double z = -8.0;
    for (int i = 0; i < 6; ++i) z += A[i] * std::sin(kx[i] * x + ky[i] * y + ph[i]);
    return z;
  }
};

void nokta(Govde& g, double x, double y, double z) {
  g.v.push_back(float(x));
  g.v.push_back(float(y));
  g.v.push_back(float(z));
}
uint32_t nk(const Govde& g) { return uint32_t(g.v.size() / 3); }
void ucg(Govde& g, uint32_t a, uint32_t b, uint32_t c) {
  g.ix.push_back(a);
  g.ix.push_back(b);
  g.ix.push_back(c);
}

Govde taban_agi(const Taban& t, int G, double L) {
  Govde g;
  g.ad = "taban";
  for (int j = 0; j < G; ++j)
    for (int i = 0; i < G; ++i) {
      const double x = -L + 2 * L * i / (G - 1), y = -L + 2 * L * j / (G - 1);
      nokta(g, x, y, t.h(x, y));
    }
  for (int j = 0; j + 1 < G; ++j)
    for (int i = 0; i + 1 < G; ++i) {
      const uint32_t a = uint32_t(j * G + i), b = a + 1, c = a + uint32_t(G), d = c + 1;
      ucg(g, a, b, d);
      ucg(g, a, d, c);
    }
  return g;
}

Govde kutu(const std::string& ad, double cx, double cy, double z0, double sx, double sy, double sz, double yaw) {
  Govde g;
  g.ad = ad;
  const double c = std::cos(yaw), s = std::sin(yaw);
  for (int k = 0; k < 8; ++k) {
    const double lx = ((k & 1) ? 0.5 : -0.5) * sx, ly = ((k & 2) ? 0.5 : -0.5) * sy, lz = (k & 4) ? sz : 0.0;
    nokta(g, cx + c * lx - s * ly, cy + s * lx + c * ly, z0 + lz);
  }
  const uint32_t f[6][4] = {{0, 1, 3, 2}, {4, 6, 7, 5}, {0, 4, 5, 1}, {2, 3, 7, 6}, {0, 2, 6, 4}, {1, 5, 7, 3}};
  for (auto& q : f) {
    ucg(g, q[0], q[1], q[2]);
    ucg(g, q[0], q[2], q[3]);
  }
  return g;
}

Govde silindir(const std::string& ad, double cx, double cy, double z0, double r, double h, int seg) {
  Govde g;
  g.ad = ad;
  for (int i = 0; i < seg; ++i) {
    const double a = 2 * PI * i / seg;
    nokta(g, cx + r * std::cos(a), cy + r * std::sin(a), z0);
    nokta(g, cx + r * std::cos(a), cy + r * std::sin(a), z0 + h);
  }
  const uint32_t alt = nk(g);
  nokta(g, cx, cy, z0);
  const uint32_t ust = nk(g);
  nokta(g, cx, cy, z0 + h);
  for (int i = 0; i < seg; ++i) {
    const uint32_t a0 = uint32_t(2 * i), a1 = a0 + 1, b0 = uint32_t(2 * ((i + 1) % seg)), b1 = b0 + 1;
    ucg(g, a0, b0, b1);
    ucg(g, a0, b1, a1);
    ucg(g, alt, b0, a0);
    ucg(g, ust, a1, b1);
  }
  return g;
}

Govde kure(const std::string& ad, double cx, double cy, double cz, double r, int yigin, int dilim) {
  Govde g;
  g.ad = ad;
  const uint32_t kuzey = nk(g);
  nokta(g, cx, cy, cz + r);
  for (int s = 1; s < yigin; ++s) {
    const double t = PI * s / yigin;
    for (int d = 0; d < dilim; ++d) {
      const double p = 2 * PI * d / dilim;
      nokta(g, cx + r * std::sin(t) * std::cos(p), cy + r * std::sin(t) * std::sin(p), cz + r * std::cos(t));
    }
  }
  const uint32_t guney = nk(g);
  nokta(g, cx, cy, cz - r);
  auto ix = [&](int s, int d) { return uint32_t(1 + (s - 1) * dilim + (d % dilim)); };
  for (int d = 0; d < dilim; ++d) {
    ucg(g, kuzey, ix(1, d), ix(1, d + 1));
    ucg(g, guney, ix(yigin - 1, d + 1), ix(yigin - 1, d));
  }
  for (int s = 1; s + 1 < yigin; ++s)
    for (int d = 0; d < dilim; ++d) {
      ucg(g, ix(s, d), ix(s + 1, d), ix(s + 1, d + 1));
      ucg(g, ix(s, d), ix(s + 1, d + 1), ix(s, d + 1));
    }
  return g;
}

RTCScene ag_sahnesi(RTCDevice dev, const std::vector<const Govde*>& gl) {
  RTCScene s = rtcNewScene(dev);
  rtcSetSceneBuildQuality(s, RTC_BUILD_QUALITY_MEDIUM);
  rtcSetSceneFlags(s, RTC_SCENE_FLAG_NONE);
  for (unsigned i = 0; i < gl.size(); ++i) {
    const Govde& g = *gl[i];
    RTCGeometry geo = rtcNewGeometry(dev, RTC_GEOMETRY_TYPE_TRIANGLE);
    auto* vb = static_cast<float*>(
        rtcSetNewGeometryBuffer(geo, RTC_BUFFER_TYPE_VERTEX, 0, RTC_FORMAT_FLOAT3, 3 * sizeof(float), g.v.size() / 3));
    auto* ib = static_cast<uint32_t*>(
        rtcSetNewGeometryBuffer(geo, RTC_BUFFER_TYPE_INDEX, 0, RTC_FORMAT_UINT3, 3 * sizeof(uint32_t), g.ucgen()));
    std::copy(g.v.begin(), g.v.end(), vb);
    std::copy(g.ix.begin(), g.ix.end(), ib);
    rtcSetGeometryBuildQuality(geo, RTC_BUILD_QUALITY_MEDIUM);
    rtcCommitGeometry(geo);
    const unsigned id = rtcAttachGeometry(s, geo);
    if (id != i) throw std::runtime_error("geomID sırası beklenmedik");
    rtcReleaseGeometry(geo);
  }
  rtcCommitScene(s);
  return s;
}

}  // namespace

void Sahne::kur(RTCDevice d, const std::string& ad, uint64_t sahne_tohumu) {
  dev = d;
  size_t hedef_toplam;
  if (ad == "1e4")
    hedef_toplam = 10000;
  else if (ad == "1e5")
    hedef_toplam = 100000;
  else if (ad == "1e6")
    hedef_toplam = 1000000;
  else
    throw std::runtime_error("sahne adı 1e4|1e5|1e6");

  // 0 taban, 1 su yüzeyi, 2 duvar, 3 cisim, 4 hedef  ([K] yer tutucu değerler)
  malzemeler = {{std::pow(10.0, -2.7), 0.3, 0.1},
                {std::pow(10.0, -4.0), -1.0, 0.9},
                {std::pow(10.0, -2.0), 0.5, 0.3},
                {std::pow(10.0, -1.5), 0.7, 0.5},
                {std::pow(10.0, -1.0), 0.8, 0.5}};

  SahneRng r{anahtar(sahne_tohumu, 0, 0, Akis::sahne)};
  Taban taban(r);

  std::vector<Govde> cisimler;
  // Yarısı kısa menzilde (2-6,5 m), yarısı uzakta (8-45 m).
  auto konum = [&](int i, double& x, double& y) {
    const double rho = (i % 2 == 0) ? r.ara(2.0, 6.5) : r.ara(8.0, 45.0);
    const double psi = r.ara(0, 2 * PI);
    x = rho * std::cos(psi);
    y = rho * std::sin(psi);
  };
  int sayac = 0;
  for (int i = 0; i < 4; ++i) {
    double x, y;
    konum(sayac++, x, y);
    cisimler.push_back(silindir("silindir" + std::to_string(i), x, y, taban.h(x, y) - 0.1, r.ara(0.2, 0.6),
                                r.ara(1.0, 3.0), 32));
  }
  for (int i = 0; i < 4; ++i) {
    double x, y;
    konum(sayac++, x, y);
    cisimler.push_back(kutu("kutu" + std::to_string(i), x, y, taban.h(x, y) - 0.1, r.ara(0.5, 2.0), r.ara(0.5, 2.0),
                            r.ara(0.5, 2.0), r.ara(0, PI)));
  }
  for (int i = 0; i < 3; ++i) {
    double x, y;
    konum(sayac++, x, y);
    const double rr = r.ara(0.4, 1.2);
    cisimler.push_back(kure("kure" + std::to_string(i), x, y, taban.h(x, y) + rr * 0.8, rr, 12, 24));
  }
  Govde yuzey;
  yuzey.ad = "su_yuzeyi";
  nokta(yuzey, -60, -60, 0);
  nokta(yuzey, 60, -60, 0);
  nokta(yuzey, 60, 60, 0);
  nokta(yuzey, -60, 60, 0);
  ucg(yuzey, 0, 1, 2);
  ucg(yuzey, 0, 2, 3);
  Govde duvar = kutu("duvar", 6.15, 0.0, -9.0, 0.3, 30.0, 8.0, 0.0);

  hedef = kure("hedef", 0, 0, 0, 0.6, 12, 24);
  hedef.malzeme = 4;
  hedef.govde_id = 1000;

  size_t diger = yuzey.ucgen() + duvar.ucgen() + hedef.ucgen();
  for (auto& c : cisimler) diger += c.ucgen();
  const double kalan = double(hedef_toplam) - double(diger);
  const int G = int(std::lround(std::sqrt(kalan / 2.0))) + 1;
  Govde tab = taban_agi(taban, G, 55.0);

  statik.clear();
  tab.malzeme = 0;
  statik.push_back(std::move(tab));
  yuzey.malzeme = 1;
  statik.push_back(std::move(yuzey));
  duvar.malzeme = 2;
  statik.push_back(std::move(duvar));
  for (auto& c : cisimler) {
    c.malzeme = 3;
    statik.push_back(std::move(c));
  }
  toplam_ucgen = hedef.ucgen();
  for (unsigned i = 0; i < statik.size(); ++i) {
    statik[i].govde_id = i;
    toplam_ucgen += statik[i].ucgen();
  }

  const auto t0 = std::chrono::steady_clock::now();
  std::vector<const Govde*> gl;
  for (auto& g : statik) gl.push_back(&g);
  statik_sahne = ag_sahnesi(dev, gl);
  hedef_sahne = ag_sahnesi(dev, {&hedef});
  const auto t1 = std::chrono::steady_clock::now();
  kurulum_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
}

void Sahne::birak() {
  if (statik_sahne) rtcReleaseScene(statik_sahne);
  if (hedef_sahne) rtcReleaseScene(hedef_sahne);
  statik_sahne = hedef_sahne = nullptr;
}

}  // namespace mp7
