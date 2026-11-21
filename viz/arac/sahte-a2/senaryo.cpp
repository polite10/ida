#include "senaryo.hpp"

#include <algorithm>
#include <cmath>

#include "ida/viz/dalga.hpp"

namespace ida::sahte {

namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr double kDerece = kPi / 180.0;

double smoothstep(double u) {
  u = std::clamp(u, 0.0, 1.0);
  return u * u * (3.0 - 2.0 * u);
}

double saniye(std::int64_t t_ns) { return static_cast<double>(t_ns) * 1e-9; }

// Zaman çizelgesinin sabitleri.
constexpr double kPsi0 = 20.0 * kDerece;    // başlangıç sapması
constexpr double kYuzeyD = 0.11;            // yüzeyde O_b'nin ortalama derinliği (m): köpüğün üstü su üstünde
constexpr double kDalisD = 3.0;
constexpr double kTurR = 4.0;
constexpr double kTurV = 0.6;
constexpr double kTA = 10.0, kTB = 24.0;
const double kTC = kTB + 2.0 * kPi * kTurR / kTurV;  // ≈ 65,9 s
const double kTD = kTC + 30.0;
const V3 kHedef{14.0, 9.0, 4.0};
constexpr double kHedefSapma = 100.0 * kDerece;
constexpr double kDuruMesafe = 1.8;
constexpr double kDuruD = 3.9;

V3 yatay(double n, double e) { return {n, e, 0.0}; }

}  // namespace

Senaryo::Senaryo(SenaryoAyarlari a) : a_(std::move(a)) {
  // Dalga kümesi: 8 bileşen, derin su dağılımı ω² = g k; H_s ≈ 0,4 m (hesap: 4·√(Σa²/2)).
  // Görsel amaçlı varsayım; tohum yalnız kayıt içindir (§3.4).
  struct B {
    double lambda, a, beta_derece, phi;
  };
  const B bilesenler[] = {
      {42.0, 0.10, 30.0, 0.3},  {26.0, 0.07, 52.0, 1.9}, {15.0, 0.05, 12.0, 4.1}, {9.0, 0.035, 71.0, 2.6},
      {6.0, 0.020, -8.0, 5.3}, {4.0, 0.012, 41.0, 0.8}, {2.5, 0.006, 96.0, 3.7}, {1.6, 0.004, 22.0, 1.2},
  };
  dalga_.d.surum = 1;
  dalga_.d.derin_su = 1;
  dalga_.d.tohum = 20261121;
  dalga_.d.g = 9.80665;
  dalga_.d.rho = 1025.0;
  dalga_.d.derinlik_m = 50.0;
  for (const B& b : bilesenler) {
    a2::DalgaBileseni d{};
    d.k = 2.0 * kPi / b.lambda;
    d.omega = std::sqrt(dalga_.d.g * d.k);
    d.a = b.a;
    d.beta = b.beta_derece * kDerece;
    d.phi = b.phi;
    dalga_.bilesenler.push_back(d);
  }
  dalga_ozeti_hesapla(dalga_);

  // Ortam: açık su; statik varlık deniz tabanı (REF 10 m derinlikte). Su optiği kıyı suyu
  // görünümü için seçilmiş varsayımdır (A5'te kalibrasyonu yok).
  ortam_.ortam_surumu = 1;
  ortam_.ortam_turu = static_cast<std::uint8_t>(a2::OrtamTuru::acik_su);
  ortam_.statik_varlik_indeksi = kVarlikTaban;
  ortam_.statik_p_n[0] = 0.0;
  ortam_.statik_p_n[1] = 0.0;
  ortam_.statik_p_n[2] = 10.0;
  ortam_.statik_q_nb[0] = 1.0;
  const double yukseklik = 52.0 * kDerece, yon = 135.0 * kDerece;
  ortam_.gunes_yonu_n[0] = std::cos(yukseklik) * std::cos(yon);
  ortam_.gunes_yonu_n[1] = std::cos(yukseklik) * std::sin(yon);
  ortam_.gunes_yonu_n[2] = -std::sin(yukseklik);
  ortam_.gunes_isinimi_w_m2 = 900.0;
  const double ortam_isigi[3] = {0.32, 0.42, 0.56};
  const double beta_d[3] = {0.42, 0.085, 0.07};
  const double beta_b[3] = {0.30, 0.080, 0.065};
  const double b_sonsuz[3] = {0.008, 0.10, 0.16};
  for (int i = 0; i < 3; ++i) {
    ortam_.ortam_isigi_rgb_dogrusal[i] = ortam_isigi[i];
    ortam_.beta_d_m[i] = beta_d[i];
    ortam_.beta_b_m[i] = beta_b[i];
    ortam_.b_sonsuz_rgb_dogrusal[i] = b_sonsuz[i];
  }
  ortam_.bulaniklik_olcegi = 0.0;
  ortam_.su_optigi_surumu = 1;
}

void Senaryo::sifirla() {
  ++nesil_;
  arac_kimligi_ = hedef_kimligi_ + 1;
  hedef_kimligi_ = arac_kimligi_ + 1;
}

std::vector<a2::GovdeBilgisi> Senaryo::govde_bilgileri() const {
  a2::GovdeBilgisi arac{};
  arac.govde_kimligi = arac_kimligi_;
  arac.sureklilik_no = 0;
  arac.varlik_indeksi = kVarlikArac;
  arac.govde_turu = static_cast<std::uint8_t>(a2::GovdeTuru::arac);
  arac.dolu_tupler = 0x3;  // iki tüp dolu
  a2::GovdeBilgisi hedef{};
  hedef.govde_kimligi = hedef_kimligi_;
  hedef.sureklilik_no = 0;
  hedef.varlik_indeksi = kVarlikHedef;
  hedef.govde_turu = static_cast<std::uint8_t>(a2::GovdeTuru::hedef);
  hedef.dolu_tupler = 0;
  return {arac, hedef};
}

KinematikPoz Senaryo::hedef_pozu() const { return {kHedef, euler_zyx(kHedefSapma, 0.0, 0.0)}; }

KinematikPoz Senaryo::arac_pozu(std::int64_t t_ns) const {
  const double t = saniye(t_ns);
  const V3 p0{0.0, 0.0, 0.0};
  const V3 ileri0{std::cos(kPsi0), std::sin(kPsi0), 0.0};
  const V3 sancak0{-std::sin(kPsi0), std::cos(kPsi0), 0.0};
  const V3 p1 = topla(p0, olcekle(ileri0, 1.2));  // dalış sonu

  V3 p;
  double sapma = kPsi0, yunuslama = 0.0, yalpa = 0.0;
  double yuzey_agirligi = 0.0;  // 1: dalgayı izler
  double d_hedef = kYuzeyD;

  if (t < kTA) {
    p = topla(p0, yatay(0.03 * std::sin(0.4 * t), 0.02 * std::sin(0.31 * t)));
    sapma = kPsi0 + 0.03 * std::sin(0.2 * t);
    yuzey_agirligi = 1.0;
  } else if (t < kTB) {
    const double u = (t - kTA) / (kTB - kTA);
    const double s = smoothstep(u);
    const double salinim = 1.0 - smoothstep(u * 3.0);
    p = topla(topla(p0, olcekle(ileri0, 1.2 * s)),
              olcekle(yatay(0.03 * std::sin(0.4 * t), 0.02 * std::sin(0.31 * t)), salinim));
    sapma = kPsi0 + 0.03 * std::sin(0.2 * t) * salinim;
    yuzey_agirligi = salinim;
    d_hedef = kYuzeyD + (kDalisD - kYuzeyD) * s;
    yunuslama = -4.0 * kDerece * std::sin(kPi * u);
  } else if (t < kTC) {
    const double u = (t - kTB) / (kTC - kTB);
    const double alfa = 2.0 * kPi * smoothstep(u);
    const V3 merkez = topla(p1, olcekle(sancak0, kTurR));
    const double ps = kPsi0 + alfa;
    p = cikar(merkez, olcekle(V3{-std::sin(ps), std::cos(ps), 0.0}, kTurR));
    sapma = ps;
    d_hedef = kDalisD + 0.05 * std::sin(2.0 * alfa);
  } else {
    const double psi_a = std::atan2(kHedef.y - p1.y, kHedef.x - p1.x);
    const V3 yon_a{std::cos(psi_a), std::sin(psi_a), 0.0};
    const V3 duru = cikar(V3{kHedef.x, kHedef.y, 0.0}, olcekle(yon_a, kDuruMesafe));
    if (t < kTD) {
      const double u = (t - kTC) / (kTD - kTC);
      const double s = smoothstep(u);
      const double L = uzunluk(cikar(duru, p1));
      const V3 m0 = olcekle(ileri0, 0.6 * L);
      const V3 m1 = olcekle(yon_a, 0.6 * L);
      p = hermite(p1, m0, duru, m1, 1.0, s);
      // Sapma: yolun teğeti (dH/ds), sürekli olsun diye ilk turdan kalan 2π eklenir.
      const double s2 = s * s;
      const double d00 = 6.0 * s2 - 6.0 * s, d10 = 3.0 * s2 - 4.0 * s + 1.0;
      const double d01 = -6.0 * s2 + 6.0 * s, d11 = 3.0 * s2 - 2.0 * s;
      const V3 teget = topla(topla(olcekle(p1, d00), olcekle(m0, d10)), topla(olcekle(duru, d01), olcekle(m1, d11)));
      double ps = std::atan2(teget.y, teget.x);
      while (ps < kPsi0 + 2.0 * kPi - kPi) ps += 2.0 * kPi;
      while (ps > kPsi0 + 2.0 * kPi + kPi) ps -= 2.0 * kPi;
      sapma = ps;
      d_hedef = kDalisD + (kDuruD - kDalisD) * s;
    } else {
      // Konum tutma salınımı 3 s'de yumuşakça başlar (yörünge sürekli kalsın).
      const double k = smoothstep((t - kTD) / 3.0);
      p = topla(duru, olcekle(yatay(0.02 * std::sin(0.7 * t), 0.015 * std::sin(0.53 * t)), k));
      sapma = psi_a + 2.0 * kPi + k * 0.01 * std::sin(0.5 * t);
      d_hedef = kDuruD + k * 0.015 * std::sin(0.9 * t);
    }
  }

  // Yüzeyde: O_b dalga yüzeyini izler; yalpa ve yunuslama yüzey eğiminin bir kısmı.
  double d = d_hedef;
  if (yuzey_agirligi > 0.0) {
    const double zeta = dalga_yuksekligi(dalga_.bilesenler, p.x, p.y, t_ns);
    double dn = 0.0, de = 0.0;
    dalga_egimi(dalga_.bilesenler, p.x, p.y, t_ns, dn, de);
    const double sx = dn * std::cos(sapma) + de * std::sin(sapma);    // gövde x boyunca
    const double sy = -dn * std::sin(sapma) + de * std::cos(sapma);   // gövde y boyunca
    d = yuzey_agirligi * (kYuzeyD - zeta) + (1.0 - yuzey_agirligi) * d_hedef;
    // Yüzey önde yükselirse burun kalkar (yunuslama +); sancakta yükselirse sancak kalkar (yalpa −).
    yunuslama += yuzey_agirligi * 0.6 * std::atan(sx);
    yalpa += yuzey_agirligi * -0.6 * std::atan(sy);
  }
  p.z = d;
  return {p, euler_zyx(sapma, yunuslama, yalpa)};
}

std::vector<a2::GovdeDurumu> Senaryo::govde_durumlari(std::int64_t t_ns) const {
  constexpr std::int64_t h = 1'000'000;  // 1 ms; sayısal türev
  const KinematikPoz a0 = arac_pozu(t_ns - h);
  const KinematikPoz a1 = arac_pozu(t_ns);
  const KinematikPoz a2p = arac_pozu(t_ns + h);

  a2::GovdeDurumu arac{};
  arac.govde_kimligi = arac_kimligi_;
  arac.sureklilik_no = 0;
  arac.govde_turu = static_cast<std::uint8_t>(a2::GovdeTuru::arac);
  arac.dolu_tupler = 0x3;
  arac.bayraklar = a2::kGovdeGecerli;
  arac.p_n[0] = a1.p.x;
  arac.p_n[1] = a1.p.y;
  arac.p_n[2] = a1.p.z;
  const Q q = normalle(a1.q);
  arac.q_nb[0] = q.w;
  arac.q_nb[1] = q.x;
  arac.q_nb[2] = q.y;
  arac.q_nb[3] = q.z;
  const double iki_h = 2.0 * static_cast<double>(h) * 1e-9;
  arac.v_n[0] = (a2p.p.x - a0.p.x) / iki_h;
  arac.v_n[1] = (a2p.p.y - a0.p.y) / iki_h;
  arac.v_n[2] = (a2p.p.z - a0.p.z) / iki_h;
  // Gövde çerçevesinde açısal hız: q1 = q0 ⊗ dq, ω_b ≈ 2·vec(dq) / Δt.
  Q dq = carp(eslenik(normalle(a0.q)), normalle(a2p.q));
  if (dq.w < 0.0) dq = {-dq.w, -dq.x, -dq.y, -dq.z};
  arac.w_b[0] = 2.0 * dq.x / iki_h;
  arac.w_b[1] = 2.0 * dq.y / iki_h;
  arac.w_b[2] = 2.0 * dq.z / iki_h;

  a2::GovdeDurumu hedef{};
  hedef.govde_kimligi = hedef_kimligi_;
  hedef.sureklilik_no = 0;
  hedef.govde_turu = static_cast<std::uint8_t>(a2::GovdeTuru::hedef);
  hedef.dolu_tupler = 0;
  hedef.bayraklar = a2::kGovdeGecerli;
  const KinematikPoz hp = hedef_pozu();
  hedef.p_n[0] = hp.p.x;
  hedef.p_n[1] = hp.p.y;
  hedef.p_n[2] = hp.p.z;
  hedef.q_nb[0] = hp.q.w;
  hedef.q_nb[1] = hp.q.x;
  hedef.q_nb[2] = hp.q.y;
  hedef.q_nb[3] = hp.q.z;
  return {arac, hedef};
}

}  // namespace ida::sahte
