// Belirlenimci, tohumlu ölçüm sahnesi: engebeli taban (yükseklik alanı),
// ayakta cisimler (silindir, kutu, küre), bir duvar, su yüzeyi düzlemi
// ve hareketli bir hedef (ayrı alt sahne).
#pragma once
#include <embree4/rtcore.h>

#include <cstdint>
#include <string>
#include <vector>

namespace mp7 {

struct V3 {
  double x = 0, y = 0, z = 0;
};
inline V3 operator+(V3 a, V3 b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
inline V3 operator-(V3 a, V3 b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
inline V3 operator*(V3 a, double s) { return {a.x * s, a.y * s, a.z * s}; }
inline double dot(V3 a, V3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
inline V3 cross(V3 a, V3 b) { return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x}; }
double norm(V3 a);
V3 normalize(V3 a);

// Akustik malzeme (05 §2.4.1). Değerler [K] yer tutucudur; yükü değil yalnız genliği etkiler.
struct Malzeme {
  double mu;       // Lambert geri saçılım katsayısı (mu cos^2)
  double Rs;       // speküler basınç yansıma katsayısı
  double gamma_p;  // pürüzlülük çarpanı (sabit)
};

struct Govde {
  std::string ad;
  std::vector<float> v;      // x,y,z üçlüleri (nesne uzayı)
  std::vector<uint32_t> ix;  // üçgen indisleri
  int malzeme = 0;
  uint32_t govde_id = 0;
  size_t ucgen() const { return ix.size() / 3; }
};

struct Sahne {
  RTCDevice dev = nullptr;
  std::vector<Malzeme> malzemeler;
  std::vector<Govde> statik;  // geomID == indis
  Govde hedef;                // hareketli alt sahne
  RTCScene statik_sahne = nullptr;
  RTCScene hedef_sahne = nullptr;
  size_t toplam_ucgen = 0;
  double kurulum_ms = 0;  // statik + hedef BVH kurulumu (tek iş parçacığı)

  // ad: "1e4", "1e5", "1e6"
  void kur(RTCDevice d, const std::string& ad, uint64_t sahne_tohumu);
  void birak();
};

}  // namespace mp7
