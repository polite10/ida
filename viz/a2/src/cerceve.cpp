#include "ida/viz/cerceve.hpp"

#include <cmath>

namespace ida::viz {

V3 topla(const V3& a, const V3& b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
V3 cikar(const V3& a, const V3& b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
V3 olcekle(const V3& a, double s) { return {a.x * s, a.y * s, a.z * s}; }
double nokta(const V3& a, const V3& b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
V3 capraz(const V3& a, const V3& b) {
  return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}
double uzunluk(const V3& a) { return std::sqrt(nokta(a, a)); }

Q carp(const Q& a, const Q& b) {
  return {a.w * b.w - a.x * b.x - a.y * b.y - a.z * b.z, a.w * b.x + a.x * b.w + a.y * b.z - a.z * b.y,
          a.w * b.y - a.x * b.z + a.y * b.w + a.z * b.x, a.w * b.z + a.x * b.y - a.y * b.x + a.z * b.w};
}

Q eslenik(const Q& q) { return {q.w, -q.x, -q.y, -q.z}; }

double qnokta(const Q& a, const Q& b) { return a.w * b.w + a.x * b.x + a.y * b.y + a.z * b.z; }

Q normalle(const Q& q) {
  const double n = std::sqrt(qnokta(q, q));
  if (!(n > 0.0)) return Q{};
  return {q.w / n, q.x / n, q.y / n, q.z / n};
}

V3 dondur(const Q& q, const V3& v) {
  const Q p{0.0, v.x, v.y, v.z};
  const Q r = carp(carp(q, p), eslenik(q));
  return {r.x, r.y, r.z};
}

M3 matris(const Q& q) {
  const double w = q.w, x = q.x, y = q.y, z = q.z;
  M3 r;
  r.m[0][0] = 1.0 - 2.0 * (y * y + z * z);
  r.m[0][1] = 2.0 * (x * y - w * z);
  r.m[0][2] = 2.0 * (x * z + w * y);
  r.m[1][0] = 2.0 * (x * y + w * z);
  r.m[1][1] = 1.0 - 2.0 * (x * x + z * z);
  r.m[1][2] = 2.0 * (y * z - w * x);
  r.m[2][0] = 2.0 * (x * z - w * y);
  r.m[2][1] = 2.0 * (y * z + w * x);
  r.m[2][2] = 1.0 - 2.0 * (x * x + y * y);
  return r;
}

M3 carp(const M3& a, const M3& b) {
  M3 r;
  for (int i = 0; i < 3; ++i) {
    for (int j = 0; j < 3; ++j) {
      r.m[i][j] = a.m[i][0] * b.m[0][j] + a.m[i][1] * b.m[1][j] + a.m[i][2] * b.m[2][j];
    }
  }
  return r;
}

M3 devrik(const M3& a) {
  M3 r;
  for (int i = 0; i < 3; ++i) {
    for (int j = 0; j < 3; ++j) r.m[i][j] = a.m[j][i];
  }
  return r;
}

V3 carp(const M3& a, const V3& v) {
  return {a.m[0][0] * v.x + a.m[0][1] * v.y + a.m[0][2] * v.z, a.m[1][0] * v.x + a.m[1][1] * v.y + a.m[1][2] * v.z,
          a.m[2][0] * v.x + a.m[2][1] * v.y + a.m[2][2] * v.z};
}

Q eksen_aci(const V3& eksen, double aci) {
  const double s = std::sin(0.5 * aci);
  return {std::cos(0.5 * aci), eksen.x * s, eksen.y * s, eksen.z * s};
}

Q euler_zyx(double sapma, double yunuslama, double yalpa) {
  const Q qz = eksen_aci({0.0, 0.0, 1.0}, sapma);
  const Q qy = eksen_aci({0.0, 1.0, 0.0}, yunuslama);
  const Q qx = eksen_aci({1.0, 0.0, 0.0}, yalpa);
  return carp(carp(qz, qy), qx);
}

V3 ned_godot_konum(const V3& p_n) { return {p_n.y, -p_n.z, -p_n.x}; }

Q ned_godot_kuaterniyon(const Q& q_nb) { return {q_nb.w, q_nb.y, -q_nb.z, -q_nb.x}; }

M3 ned_godot_taban(const Q& q_nb) {
  M3 c;
  for (int i = 0; i < 3; ++i) {
    for (int j = 0; j < 3; ++j) c.m[i][j] = kC[i][j];
  }
  return carp(carp(c, matris(q_nb)), devrik(c));
}

V3 godot_ned_konum(const V3& p_g) { return {-p_g.z, p_g.x, -p_g.y}; }

V3 hermite(const V3& p0, const V3& v0, const V3& p1, const V3& v1, double dt, double s) {
  const double s2 = s * s, s3 = s2 * s;
  const double h00 = 2.0 * s3 - 3.0 * s2 + 1.0;
  const double h10 = s3 - 2.0 * s2 + s;
  const double h01 = -2.0 * s3 + 3.0 * s2;
  const double h11 = s3 - s2;
  return {h00 * p0.x + h10 * dt * v0.x + h01 * p1.x + h11 * dt * v1.x,
          h00 * p0.y + h10 * dt * v0.y + h01 * p1.y + h11 * dt * v1.y,
          h00 * p0.z + h10 * dt * v0.z + h01 * p1.z + h11 * dt * v1.z};
}

Q slerp_kisa(const Q& a, const Q& b_in, double s) {
  Q b = b_in;
  double c = qnokta(a, b);
  if (c < 0.0) {  // kısa yol
    b = {-b.w, -b.x, -b.y, -b.z};
    c = -c;
  }
  if (c > 0.9995) {  // neredeyse aynı: doğrusal + normalleme
    return normalle({a.w + s * (b.w - a.w), a.x + s * (b.x - a.x), a.y + s * (b.y - a.y), a.z + s * (b.z - a.z)});
  }
  const double teta = std::acos(c);
  const double st = std::sin(teta);
  const double ka = std::sin((1.0 - s) * teta) / st;
  const double kb = std::sin(s * teta) / st;
  return {ka * a.w + kb * b.w, ka * a.x + kb * b.x, ka * a.y + kb * b.y, ka * a.z + kb * b.z};
}

}  // namespace ida::viz
