#include "sonar.hpp"

#include <embree4/rtcore.h>

#include <algorithm>
#include <cmath>
#include <limits>

#include "rng.hpp"

namespace mp7 {

namespace {

constexpr double PI = 3.141592653589793;
constexpr double DEG = PI / 180.0;
constexpr double EPS = 1e-4;  // ikincil ışın kaydırması, m
const V3 T_POZ{0.0, 0.0, -6.0};
constexpr double EGIM = 5.0 * DEG;  // kafa 5° aşağı eğik
constexpr uint32_t AKIS_FAZ = 7;    // hücre fazı akışı (sonar_hucre'den ayrı)

double sinc(double x) {
  if (std::fabs(x) < 1e-9) return 1.0;
  return std::sin(PI * x) / (PI * x);
}

struct Kafa {
  double ca, sa, cb, sb;
  // R = Rz(alfa) * Ry(beta); kafa -> dünya
  V3 dunya(V3 h) const {
    const V3 r1{cb * h.x + sb * h.z, h.y, -sb * h.x + cb * h.z};
    return {ca * r1.x - sa * r1.y, sa * r1.x + ca * r1.y, r1.z};
  }
  V3 kafa(V3 w) const {
    const V3 r1{ca * w.x + sa * w.y, -sa * w.x + ca * w.y, w.z};
    return {cb * r1.x - sb * r1.z, r1.y, sb * r1.x + cb * r1.z};
  }
};

struct Vurus {
  bool var = false;
  double t = 0;
  V3 P, n;  // n gelen ışına bakar
  int malzeme = 0;
  uint32_t govde = 0, prim = 0;
  int32_t ci = 0, cj = 0;  // ortonormal yüzey koordinatlarında hücre indisi
};

struct Baglam {
  const Sahne* s;
  RTCScene ust;
  double dh;
  bool hucre;
};

Vurus kesis(const Baglam& b, V3 o, V3 d, double tnear, double tfar) {
  RTCRayHit rh;
  rh.ray.org_x = float(o.x);
  rh.ray.org_y = float(o.y);
  rh.ray.org_z = float(o.z);
  rh.ray.dir_x = float(d.x);
  rh.ray.dir_y = float(d.y);
  rh.ray.dir_z = float(d.z);
  rh.ray.tnear = float(tnear);
  rh.ray.tfar = float(tfar);
  rh.ray.time = 0.f;
  rh.ray.mask = 0xFFFFFFFFu;
  rh.ray.id = 0;
  rh.ray.flags = 0;
  rh.hit.geomID = RTC_INVALID_GEOMETRY_ID;
  rh.hit.instID[0] = RTC_INVALID_GEOMETRY_ID;
  rtcIntersect1(b.ust, &rh);
  Vurus v;
  if (rh.hit.geomID == RTC_INVALID_GEOMETRY_ID) return v;
  const Govde* g = (rh.hit.instID[0] == 1) ? &b.s->hedef : &b.s->statik[rh.hit.geomID];
  const uint32_t pr = rh.hit.primID;
  const uint32_t i0 = g->ix[3 * pr], i1 = g->ix[3 * pr + 1], i2 = g->ix[3 * pr + 2];
  const V3 v0{g->v[3 * i0], g->v[3 * i0 + 1], g->v[3 * i0 + 2]};
  const V3 v1{g->v[3 * i1], g->v[3 * i1 + 1], g->v[3 * i1 + 2]};
  const V3 v2{g->v[3 * i2], g->v[3 * i2 + 1], g->v[3 * i2 + 2]};
  const V3 e1 = v1 - v0, e2 = v2 - v0;
  const V3 nu = normalize(cross(e1, e2));
  V3 n = nu;
  if (dot(n, d) > 0) n = n * -1.0;
  v.var = true;
  v.t = rh.ray.tfar;
  v.P = o + d * v.t;
  v.n = n;
  v.malzeme = g->malzeme;
  v.govde = g->govde_id;
  v.prim = pr;
  if (b.hucre) {
    // Gövdeye sabit ortonormal yüzey koordinatları (u = e1 yönü, w = n x u); kenar Delta_h.
    // Hücre kimliği ışın sayısından bağımsızdır; üçgen sınırında hücre kırpılır
    // (alan, hücreye düşen ışınların enerji ağırlıklarıyla örtük olarak hesaplanır).
    const V3 ua = normalize(e1);
    const V3 wa = cross(nu, ua);
    const V3 rel = e1 * double(rh.hit.u) + e2 * double(rh.hit.v);
    v.ci = int32_t(std::floor(dot(rel, ua) / b.dh));
    v.cj = int32_t(std::floor(dot(rel, wa) / b.dh));
  }
  return v;
}

bool gorunur(const Baglam& b, V3 o, V3 hedef) {
  const V3 dv = hedef - o;
  const double L = norm(dv);
  const V3 d = dv * (1.0 / L);
  RTCRay r;
  r.org_x = float(o.x);
  r.org_y = float(o.y);
  r.org_z = float(o.z);
  r.dir_x = float(d.x);
  r.dir_y = float(d.y);
  r.dir_z = float(d.z);
  r.tnear = float(EPS);
  r.tfar = float(L - 2 * EPS);
  r.time = 0.f;
  r.mask = 0xFFFFFFFFu;
  r.id = 0;
  r.flags = 0;
  rtcOccluded1(b.ust, &r);
  return r.tfar >= 0.f;
}

uint64_t ozet4(const uint32_t k[4]) {
  return splitmix64(uint64_t(k[0]) | (uint64_t(k[1]) << 32)) ^
         splitmix64((uint64_t(k[2]) | (uint64_t(k[3]) << 32)) + 0x632BE59BD9B4E019ull);
}

}  // namespace

Sonar::Sonar(const Sahne& s, const SonarAyar& a) : s_(s), a_(a) {
  // Tek yönlü genlik deseni B = sinc(theta/th0) sinc(phi/ph0); -3 dB tek yönlü
  // (0,7071) sinc(0,4429)'da: 2° x 25° tam genişlik (05 §2.4.2 [T]).
  th0_ = (1.0 / 0.44295) * DEG;
  ph0_ = (12.5 / 0.44295) * DEG;
  // Tam: ilk yan lobun sonuna kadar (sinc ikinci sıfırı); sade: ana lob (ilk sıfır).
  const double kat = a_.tam ? 2.0 : 1.0;
  Wh_ = kat * th0_;
  Wv_ = kat * ph0_;
  nh_ = std::max(1, int(std::lround(std::sqrt(a_.N * Wh_ / Wv_))));
  nv_ = std::max(1, int(std::lround(double(a_.N) / nh_)));
  sonuc_.resize(size_t(nh_) * nv_);
}

void Sonar::ping(uint64_t ping_no, int grad, double t_s, Havuz& havuz, Profil& p) {
  // --- örnek sayısı ve zaman ekseni (05 §2.4.7) ---
  int n_eff = a_.n, sp_eff = a_.sp;
  if (!a_.tam && a_.n > 600) {
    n_eff = 600;
    sp_eff = std::clamp(int(std::lround(double(a_.sp) * a_.n / n_eff)), 80, 40000);
  }
  const double T_s = sp_eff * 25e-9;
  const double R = a_.c * n_eff * T_s / 2.0;  // gerçekleşen menzil
  const int nf = 4 * n_eff;
  const double dt = T_s / 4.0;
  const double ln10 = std::log(10.0);
  const double alfa = a_.alfa_dbm;

  // --- işe özel üst sahne: statik + hareketli hedef örnekleri (05 §2.4.1) ---
  const double om = 0.25;  // rad/s, 4 m yarıçapta 1 m/s
  const V3 hedef_t{4.0 * std::cos(om * t_s), 4.0 * std::sin(om * t_s), -6.5};
  RTCScene ust = rtcNewScene(s_.dev);
  rtcSetSceneBuildQuality(ust, RTC_BUILD_QUALITY_MEDIUM);
  {
    RTCGeometry g0 = rtcNewGeometry(s_.dev, RTC_GEOMETRY_TYPE_INSTANCE);
    rtcSetGeometryInstancedScene(g0, s_.statik_sahne);
    const float m0[12] = {1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0};
    rtcSetGeometryTransform(g0, 0, RTC_FORMAT_FLOAT3X4_COLUMN_MAJOR, m0);
    rtcCommitGeometry(g0);
    rtcAttachGeometryByID(ust, g0, 0);
    rtcReleaseGeometry(g0);
    RTCGeometry g1 = rtcNewGeometry(s_.dev, RTC_GEOMETRY_TYPE_INSTANCE);
    rtcSetGeometryInstancedScene(g1, s_.hedef_sahne);
    const float m1[12] = {1, 0, 0, 0, 1, 0, 0, 0, 1, float(hedef_t.x), float(hedef_t.y), float(hedef_t.z)};
    rtcSetGeometryTransform(g1, 0, RTC_FORMAT_FLOAT3X4_COLUMN_MAJOR, m1);
    rtcCommitGeometry(g1);
    rtcAttachGeometryByID(ust, g1, 1);
    rtcReleaseGeometry(g1);
  }
  rtcCommitScene(ust);

  Baglam b{&s_, ust, a_.hucre, a_.tam};
  const Key k_isin = anahtar(a_.tohum, 5, 0, Akis::sonar_isin);
  const double a_rad = grad * 0.9 * DEG;
  const Kafa kafa{std::cos(a_rad), std::sin(a_rad), std::cos(EGIM), std::sin(EGIM)};
  const double dth = 2 * Wh_ / nh_, dph = 2 * Wv_ / nv_;
  const double th0 = th0_, ph0 = ph0_, Wh = Wh_, Wv = Wv_;
  const int nh = nh_;
  const int D = a_.tam ? a_.D : 0;
  const V3 T = T_POZ;

  // Dünya doğrultusu için tek yönlü desen; koni dışı -> false. Koni içindeki
  // negatif yan lob genlikleri korunur (sol md.2).
  auto B_yon = [&](V3 dw, double& B) -> bool {
    const V3 h = kafa.kafa(dw);
    const double th = std::atan2(h.y, h.x);
    const double ph = std::asin(std::clamp(h.z, -1.0, 1.0));
    if (std::fabs(th) > Wh || std::fabs(ph) > Wv) return false;
    B = sinc(th / th0) * sinc(ph / ph0);
    return true;
  };
  auto sogurma = [&](double L) { return std::exp(-alfa * L / 20.0 * ln10); };  // genlik
  auto katki = [](Sonar::IsinSonuc& o, int tur, const Vurus& v, double a, double tau) {
    Sonar::Katki& k = o.k[o.adet++];
    k.tau = tau;
    k.a = a;
    k.w = a * a;  // w_j = sigma dA_j G_j
    k.anahtar[0] = (uint32_t(tur) << 24) | (v.govde & 0xFFFFFFu);
    k.anahtar[1] = v.prim;
    k.anahtar[2] = uint32_t(v.ci);
    k.anahtar[3] = uint32_t(v.cj);
  };

  // --- ışınlar (paralel; sonuç ışın numarasıyla) ---
  auto isle = [&](int j0, int j1) {
    for (int j = j0; j < j1; ++j) {
      IsinSonuc& out = sonuc_[size_t(j)];
      out.adet = 0;
      out.kesisim = 0;
      out.gorunurluk = 0;
      const int jh = j % nh, jv = j / nh;
      const Ctr r = philox({uint32_t(ping_no), uint32_t(ping_no >> 32), uint32_t(j), 0}, k_isin);
      const double th = -Wh + (jh + duzgun(r[0])) * dth;
      const double ph = -Wv + (jv + duzgun(r[1])) * dph;
      const double dOm = dth * dph * std::cos(ph);
      const V3 d = kafa.dunya({std::cos(ph) * std::cos(th), std::cos(ph) * std::sin(th), std::sin(ph)});
      const double Bg = sinc(th / th0) * sinc(ph / ph0);

      // Bacak 0: doğrudan  T -> S -> T
      const Vurus v1 = kesis(b, T, d, 0.0, R);
      out.kesisim++;
      if (!v1.var) continue;
      const Malzeme& m1 = s_.malzemeler[size_t(v1.malzeme)];
      const double r1 = v1.t;
      const double cos1 = std::max(-dot(d, v1.n), 0.05);
      const double dA1 = r1 * r1 * dOm / cos1;
      const double sac1 = std::sqrt(m1.mu * cos1 * cos1 * dA1);  // sqrt(sigma dA)
      katki(out, 0, v1, Bg * Bg * sac1 * sogurma(2 * r1) / (r1 * r1), 2 * r1 / a_.c);
      if (D < 1 || m1.Rs == 0.0) continue;

      // Bacak 1: ayna ışını M -> S  (M = v1)
      const V3 dm = normalize(d - v1.n * (2 * dot(d, v1.n)));
      const Vurus v2 = kesis(b, v1.P + v1.n * EPS, dm, EPS, R);
      out.kesisim++;
      if (!v2.var) continue;
      const Malzeme& m2 = s_.malzemeler[size_t(v2.malzeme)];
      const double Lg = r1 + v2.t;
      const double cos2 = std::max(-dot(dm, v2.n), 0.05);
      const double dA2 = Lg * Lg * dOm / cos2;
      const double sac2 = std::sqrt(m2.mu * cos2 * cos2 * dA2);
      const double ayna1 = m1.Rs * m1.gamma_p;
      const V3 S0 = v2.P + v2.n * EPS;

      // Gidişte sekme T->M->S->T (+ dönüşte sekme, genlik x2): dönüş S -> T görünürlüğü
      {
        const V3 dST = T - v2.P;
        const double Ld = norm(dST);
        const V3 eST = dST * (1.0 / Ld);
        double Br = 0;
        if (dot(eST, v2.n) > 0 && B_yon(eST * -1.0, Br)) {
          out.gorunurluk++;
          if (gorunur(b, S0, T))
            katki(out, 1, v2, 2.0 * Bg * Br * ayna1 * sac2 * sogurma(Lg + Ld) / (Lg * Ld), (Lg + Ld) / a_.c);
        }
      }
      if (D < 2) continue;

      // Çift sekme T->M->S->M'->T: M' = S'den T'nin M düzlemindeki görüntüsüne (görüntü kaynağı)
      {
        const double sT = dot(T - v1.P, v1.n);
        const double sS = dot(v2.P - v1.P, v1.n);
        if (sT <= 0 || sS <= 0) continue;
        const V3 Tg = T - v1.n * (2 * sT);
        const double ts = sS / (sS + sT);
        const V3 Mp = v2.P + (Tg - v2.P) * ts;
        const V3 dSM = Mp - v2.P;
        const double lSM = norm(dSM);
        const V3 eSM = dSM * (1.0 / lSM);
        if (dot(eSM, v2.n) <= 0) continue;
        const double tol = std::max(0.01, 1e-3 * lSM);
        const Vurus v3 = kesis(b, S0, eSM, EPS, lSM + tol);
        out.kesisim++;
        if (!v3.var || std::fabs(v3.t + EPS - lSM) > tol) continue;
        const Malzeme& m3 = s_.malzemeler[size_t(v3.malzeme)];
        if (m3.Rs == 0.0) continue;
        const V3 dMT = T - v3.P;
        const double lMT = norm(dMT);
        double Br = 0;
        if (!B_yon(dMT * (-1.0 / lMT), Br)) continue;
        out.gorunurluk++;
        if (!gorunur(b, v3.P + v3.n * EPS, T)) continue;
        const double Ld = norm(v3.P - v2.P) + lMT;
        katki(out, 2, v2, Bg * Br * ayna1 * (m3.Rs * m3.gamma_p) * sac2 * sogurma(Lg + Ld) / (Lg * Ld),
              (Lg + Ld) / a_.c);
      }
    }
  };
  const int Nr = nh_ * nv_;
  havuz.calistir(Nr, 64, isle);
  rtcReleaseScene(ust);

  // --- biriktirme: ışın numarası sırasıyla, kesirli gecikme, ince ızgara T_s/4 ---
  p.n_eff = n_eff;
  p.T_s = T_s;
  p.kesisim = p.gorunurluk = 0;
  const int Hw = int(std::lround(a_.tx_kat * 4));  // çekirdek yarı genişliği (ince örnek)
  cekirdek_.assign(size_t(2 * Hw + 1), 0.0);
  for (int m = -Hw; m <= Hw; ++m) cekirdek_[size_t(m + Hw)] = 1.0 - std::fabs(double(m)) / (Hw + 1);
  p.I.assign(size_t(n_eff), 0.0);
  p.u8.assign(size_t(n_eff), 0);
  const Key k_olc = anahtar(a_.tohum, 5, 0, Akis::olcum);
  auto ince_ekle = [&](double tau, auto deger, auto& dizi) {
    const double fpos = tau / dt - 0.5;
    const double fl = std::floor(fpos);
    const int k0 = int(fl);
    const double w = fpos - fl;
    if (k0 >= 0 && k0 < nf) dizi[size_t(k0)] += (1.0 - w) * deger;
    if (k0 + 1 >= 0 && k0 + 1 < nf) dizi[size_t(k0 + 1)] += w * deger;
  };

  size_t toplam = 0;
  for (int j = 0; j < Nr; ++j) {
    p.kesisim += sonuc_[size_t(j)].kesisim;
    p.gorunurluk += sonuc_[size_t(j)].gorunurluk;
    toplam += sonuc_[size_t(j)].adet;
  }

  if (a_.tam) {
    // Hücre içinde enerji toplanır: E_c = sum w_j, işaret s_c = sign(sum a_j), gecikme
    // enerji ağırlıklı ortalama. Hücreler arası eşevreli toplam (mimar tanımı md.1).
    size_t cap = 1024;
    while (cap < 2 * toplam) cap <<= 1;
    tablo_.assign(cap, -1);
    hucreler_.clear();
    hucreler_.reserve(toplam);
    for (int j = 0; j < Nr; ++j) {
      const IsinSonuc& o = sonuc_[size_t(j)];
      for (int q = 0; q < o.adet; ++q) {
        const Katki& k = o.k[q];
        size_t h = size_t(ozet4(k.anahtar)) & (cap - 1);
        for (;;) {
          const int32_t ix = tablo_[h];
          if (ix < 0) {
            tablo_[h] = int32_t(hucreler_.size());
            hucreler_.push_back({{k.anahtar[0], k.anahtar[1], k.anahtar[2], k.anahtar[3]}, k.w, k.a, k.tau * k.w});
            break;
          }
          Hucre& c = hucreler_[size_t(ix)];
          if (c.k[0] == k.anahtar[0] && c.k[1] == k.anahtar[1] && c.k[2] == k.anahtar[2] &&
              c.k[3] == k.anahtar[3]) {
            c.sw += k.w;
            c.sa += k.a;
            c.stw += k.tau * k.w;
            break;
          }
          h = (h + 1) & (cap - 1);
        }
      }
    }
    const Key k_hucre = anahtar(a_.tohum, 5, 0, Akis::sonar_hucre);
    const Key k_faz = anahtar(a_.tohum, 5, 0, Akis(AKIS_FAZ));
    ince_.assign(size_t(nf), {0, 0});
    for (const Hucre& c : hucreler_) {
      if (c.sw <= 0) continue;
      // g_c: yüzey hücresinin benek çarpanı (yol türünden bağımsız); phi_c: hücre + yol türü fazı
      const Ctr rg = philox({c.k[0] & 0xFFFFFFu, c.k[1], c.k[2], c.k[3]}, k_hucre);
      const Ctr rf = philox({c.k[0], c.k[1], c.k[2], c.k[3]}, k_faz);
      const std::complex<double> g = karmasik_normal(rg[0], rg[1]);
      const double s = c.sa < 0 ? -1.0 : 1.0;
      const std::complex<double> pc = s * std::sqrt(c.sw) * g * std::polar(1.0, 2 * PI * duzgun(rf[0]));
      ince_ekle(c.stw / c.sw, pc, ince_);
    }
    // darbe zarfı (dikdörtgen, tx_duration) ⊗ alıcı süzgeci (bant 1/tx_duration): üçgen çekirdek
    evr_.assign(size_t(nf), {0, 0});
    for (int i = 0; i < nf; ++i) {
      if (ince_[size_t(i)] == std::complex<double>(0, 0)) continue;
      const int lo = std::max(0, i - Hw), hi = std::min(nf - 1, i + Hw);
      for (int t = lo; t <= hi; ++t) evr_[size_t(t)] += cekirdek_[size_t(t - i + Hw)] * ince_[size_t(i)];
    }
    for (int i = 0; i < n_eff; ++i) {
      std::complex<double> s = (evr_[size_t(4 * i)] + evr_[size_t(4 * i + 1)] + evr_[size_t(4 * i + 2)] +
                                evr_[size_t(4 * i + 3)]) *
                               0.25;
      if (a_.gurultu) {
        const Ctr r = philox({uint32_t(ping_no), uint32_t(ping_no >> 32), uint32_t(i), 1}, k_olc);
        s += std::sqrt(a_.In) * karmasik_normal(r[0], r[1]);
        const double ri = a_.c * (i + 0.5) * T_s / 2;
        if (ri < a_.yakin_m) s += std::sqrt(a_.In_yakin) * karmasik_normal(r[2], r[3]);
      }
      p.I[size_t(i)] = std::norm(s);
    }
  } else {
    // Sade (eşevresiz): kutu enerjisi sum w_j (mimar tanımı md.2)
    ince_I_.assign(size_t(nf), 0.0);
    for (int j = 0; j < Nr; ++j) {
      const IsinSonuc& o = sonuc_[size_t(j)];
      for (int q = 0; q < o.adet; ++q) ince_ekle(o.k[q].tau, o.k[q].w, ince_I_);
    }
    evr_I_.assign(size_t(nf), 0.0);
    for (int i = 0; i < nf; ++i) {
      if (ince_I_[size_t(i)] == 0.0) continue;
      const int lo = std::max(0, i - Hw), hi = std::min(nf - 1, i + Hw);
      for (int t = lo; t <= hi; ++t) {
        const double h = cekirdek_[size_t(t - i + Hw)];
        evr_I_[size_t(t)] += h * h * ince_I_[size_t(i)];
      }
    }
    for (int i = 0; i < n_eff; ++i) {
      double s = 0.25 * (evr_I_[size_t(4 * i)] + evr_I_[size_t(4 * i + 1)] + evr_I_[size_t(4 * i + 2)] +
                         evr_I_[size_t(4 * i + 3)]);
      const Ctr r = philox({uint32_t(ping_no), uint32_t(ping_no >> 32), uint32_t(i), 2}, k_olc);
      if (a_.sade_benek) s *= ustel(r[0]);  // 05 §2.4.3: kutu başına ortalaması 1 olan üstel çarpan
      if (a_.gurultu) {
        s += a_.In * ustel(r[1]);
        const double ri = a_.c * (i + 0.5) * T_s / 2;
        if (ri < a_.yakin_m) s += a_.In_yakin * ustel(r[2]);
      }
      p.I[size_t(i)] = s;
    }
  }

  // --- alıcı zinciri: log, TVG + kazanç, eşleme ve kuantalama (05 §2.4.4) ---
  for (int i = 0; i < n_eff; ++i) {
    const double I = std::max(p.I[size_t(i)], 1e-15);
    const double ri = a_.c * (i + 0.5) * T_s / 2;
    const double L = 10 * std::log10(I) + 20 * std::log10(ri) + 2 * alfa * ri + a_.G;
    const double q = std::round((L - a_.Lmin) * 255.0 / (a_.Lmaks - a_.Lmin));
    p.u8[size_t(i)] = uint8_t(std::clamp(q, 0.0, 255.0));
  }
}

}  // namespace mp7
