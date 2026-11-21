// Çerçeve dönüşümü (W3-viz kabul md. 3; A2 §2, 07 §2.3): bilinen NED pozları Godot'da
// beklenen konum ve yönelimi verir. Godot düğümünün yerel "ileri"si −Z'dir; gövdenin x
// ekseni (ileri) Godot yerelinde C·(1,0,0) = (0,0,−1) olduğundan, bir gövde düğümünün
// dünya ileri yönü taban_G · (0,0,−1)'dir.
#include <cmath>

#include "deneme.hpp"
#include "ida/viz/cerceve.hpp"

using namespace ida::viz;

namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr double kTol = 1e-12;

// Godot kuaterniyonu (w, x, y, z adlı alanlarda) ile vektör döndürme: Godot'nun
// Quaternion·Vector3 işlemiyle aynı Hamilton tanımı.
V3 godot_dondur(const Q& qg, const V3& v) { return dondur(qg, v); }

void matris_esit(const M3& a, const M3& b) {
  for (int i = 0; i < 3; ++i)
    for (int j = 0; j < 3; ++j) YAKIN(a.m[i][j], b.m[i][j], kTol);
}

void vektor_esit(const V3& a, const V3& b, double tol = kTol) {
  YAKIN(a.x, b.x, tol);
  YAKIN(a.y, b.y, tol);
  YAKIN(a.z, b.z, tol);
}

void poz_denetle(const V3& p_n, const Q& q_nb, const V3& beklenen_konum, const V3& beklenen_ileri,
                 const V3& beklenen_yukari) {
  vektor_esit(ned_godot_konum(p_n), beklenen_konum);
  const Q qg = ned_godot_kuaterniyon(q_nb);
  const M3 taban = ned_godot_taban(q_nb);
  // Kuaterniyon yolu ile matris yolu aynı tabanı vermeli.
  matris_esit(matris(qg), taban);
  // Godot yerel ileri (0,0,−1) ve yukarı (0,1,0)'ın dünyadaki yönleri.
  vektor_esit(godot_dondur(qg, {0.0, 0.0, -1.0}), beklenen_ileri);
  vektor_esit(godot_dondur(qg, {0.0, 1.0, 0.0}), beklenen_yukari);
}

}  // namespace

int main() {
  // C'nin kendisi: det +1, dik.
  M3 c;
  for (int i = 0; i < 3; ++i)
    for (int j = 0; j < 3; ++j) c.m[i][j] = kC[i][j];
  const double det = c.m[0][0] * (c.m[1][1] * c.m[2][2] - c.m[1][2] * c.m[2][1]) -
                     c.m[0][1] * (c.m[1][0] * c.m[2][2] - c.m[1][2] * c.m[2][0]) +
                     c.m[0][2] * (c.m[1][0] * c.m[2][1] - c.m[1][1] * c.m[2][0]);
  YAKIN(det, 1.0, kTol);
  matris_esit(carp(c, devrik(c)), M3{});

  // 1) Kuzeye bakış, yüzeyde: Godot'da orijinde, ileri −Z (kuzey), yukarı +Y.
  poz_denetle({0.0, 0.0, 0.0}, Q{}, {0.0, 0.0, 0.0}, {0.0, 0.0, -1.0}, {0.0, 1.0, 0.0});

  // 2) 90° sancağa dönüş (sapma +90°, doğuya bakış): ileri +X (doğu).
  poz_denetle({0.0, 0.0, 0.0}, euler_zyx(kPi / 2.0, 0.0, 0.0), {0.0, 0.0, 0.0}, {1.0, 0.0, 0.0}, {0.0, 1.0, 0.0});

  // 3) 3 m derinlik: y_G = −3.
  poz_denetle({0.0, 0.0, 3.0}, Q{}, {0.0, -3.0, 0.0}, {0.0, 0.0, -1.0}, {0.0, 1.0, 0.0});

  // 4) 10° baş aşağı (yunuslama −10°): ileri kuzey ve aşağı (−Y), yukarı biraz kuzeye yatar.
  const double a = 10.0 * kPi / 180.0;
  poz_denetle({0.0, 0.0, 0.0}, euler_zyx(0.0, -a, 0.0), {0.0, 0.0, 0.0}, {0.0, -std::sin(a), -std::cos(a)},
              {0.0, std::cos(a), -std::sin(a)});

  // 5) Bileşik: N = 5, E = −2, D = 1,5; sapma 90°, baş aşağı 10° → ileri doğu ve aşağı.
  poz_denetle({5.0, -2.0, 1.5}, euler_zyx(kPi / 2.0, -a, 0.0), {-2.0, -1.5, -5.0},
              {std::cos(a), -std::sin(a), 0.0}, {std::sin(a), std::cos(a), 0.0});

  // 6) Yalpa +30° (sancak aşağı): gövdenin yukarısı (−z_b) sancağa yatar (sağa yatan
  //    uçağın kabini gibi). Kuzeye bakarken sancak = doğu = +X.
  const double y = 30.0 * kPi / 180.0;
  poz_denetle({0.0, 0.0, 0.0}, euler_zyx(0.0, 0.0, y), {0.0, 0.0, 0.0}, {0.0, 0.0, -1.0},
              {std::sin(y), std::cos(y), 0.0});

  // 7) Ters dönüşüm.
  vektor_esit(godot_ned_konum(ned_godot_konum({1.25, -3.5, 7.0})), {1.25, -3.5, 7.0});

  // 8) glTF → Godot yerel = C · M = diag(−1, 1, −1) (A7-K14).
  const double M[3][3] = {{0, 0, 1}, {-1, 0, 0}, {0, -1, 0}};
  M3 mm;
  for (int i = 0; i < 3; ++i)
    for (int j = 0; j < 3; ++j) mm.m[i][j] = M[i][j];
  const M3 cm = carp(c, mm);
  M3 beklenen;
  beklenen.m[0][0] = -1.0;
  beklenen.m[2][2] = -1.0;
  matris_esit(cm, beklenen);

  // 9) Rastgele yönelimlerde kuaterniyon çevirisi = C R Cᵀ.
  for (int i = 0; i < 50; ++i) {
    const Q q = euler_zyx(0.37 * i, 0.11 * i - 2.0, 0.23 * i + 1.0);
    matris_esit(matris(ned_godot_kuaterniyon(q)), ned_godot_taban(q));
    // Bir gövde noktasının dünyadaki Godot konumu iki yoldan aynı.
    const V3 p_b{0.21, -0.1, 0.05};
    const V3 p_n{3.0, 1.0, 2.0};
    const V3 yol1 = ned_godot_konum(topla(p_n, dondur(q, p_b)));
    const V3 yerel = {p_b.y, -p_b.z, -p_b.x};  // C · p_b
    const V3 yol2 = topla(ned_godot_konum(p_n), dondur(ned_godot_kuaterniyon(q), yerel));
    vektor_esit(yol1, yol2, 1e-12);
  }
  DENEME_SONUC("viz.cerceve");
}
