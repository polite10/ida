// Çerçeve dönüşümleri ve küçük vektör/kuaterniyon matematiği (double).
//
// Sözleşme A2 §2 (uygunluk testleri için bağlayıcı) ve 07 §2.3:
//   C = [[0, 1, 0], [0, 0, −1], [−1, 0, 0]],  det C = +1
//   konum_G = C · p_n   (x_G = E, y_G = −D, z_G = −N)
//   taban_G = C · R(q_nb) · Cᵀ
// Aynı C gövde çerçevesinden Godot yereline dönüşümdür. Kuaterniyon (w, x, y, z) Hamilton,
// gövdeden dünyaya (iskelet §3.1). C bir dönüş olduğundan C R Cᵀ'nin kuaterniyonu
// (w, C·v)'dir: (w, y, −z, −x). Godot'nun (x, y, z, w) bileşen sırasına çeviri yalnız
// eklentidedir (07 §2.3); burada alanlar adlıdır.
#ifndef IDA_VIZ_CERCEVE_HPP
#define IDA_VIZ_CERCEVE_HPP

namespace ida::viz {

struct V3 {
  double x = 0.0, y = 0.0, z = 0.0;
};

struct Q {  // Hamilton, (w, x, y, z)
  double w = 1.0, x = 0.0, y = 0.0, z = 0.0;
};

struct M3 {
  double m[3][3] = {{1.0, 0.0, 0.0}, {0.0, 1.0, 0.0}, {0.0, 0.0, 1.0}};
};

inline constexpr double kC[3][3] = {{0.0, 1.0, 0.0}, {0.0, 0.0, -1.0}, {-1.0, 0.0, 0.0}};

V3 topla(const V3& a, const V3& b);
V3 cikar(const V3& a, const V3& b);
V3 olcekle(const V3& a, double s);
double nokta(const V3& a, const V3& b);
V3 capraz(const V3& a, const V3& b);
double uzunluk(const V3& a);

Q carp(const Q& a, const Q& b);  // Hamilton çarpımı
Q eslenik(const Q& q);
Q normalle(const Q& q);
double qnokta(const Q& a, const Q& b);
V3 dondur(const Q& q, const V3& v);  // R(q) · v
M3 matris(const Q& q);                // R(q)
M3 carp(const M3& a, const M3& b);
M3 devrik(const M3& a);
V3 carp(const M3& a, const V3& v);

// Eksen–açıdan kuaterniyon (eksen birim olmalı).
Q eksen_aci(const V3& eksen, double aci);
// NED'de ZYX Euler (sapma ψ, yunuslama θ, yalpa φ): q = q_z(ψ) q_y(θ) q_x(φ).
Q euler_zyx(double sapma, double yunuslama, double yalpa);

// NED → Godot (A2 §2).
V3 ned_godot_konum(const V3& p_n);
Q ned_godot_kuaterniyon(const Q& q_nb);
M3 ned_godot_taban(const Q& q_nb);  // C R(q) Cᵀ, doğrudan matris çarpımıyla (test referansı)
// Godot → NED (operatör göstergeleri ve testler için).
V3 godot_ned_konum(const V3& p_g);

// Ara değerleme (07 §2.4; A2-K32): konumda kübik Hermite, yönelimde kısa yoldan slerp.
V3 hermite(const V3& p0, const V3& v0, const V3& p1, const V3& v1, double dt, double s);
Q slerp_kisa(const Q& a, const Q& b, double s);

}  // namespace ida::viz

#endif
