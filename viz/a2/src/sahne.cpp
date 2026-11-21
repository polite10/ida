#include "ida/viz/sahne.hpp"

#include <algorithm>
#include <cmath>

namespace ida::viz {

GovdeKaydi* Sahne::bul(std::uint32_t kimlik) {
  auto it = govdeler_.find(kimlik);
  return it == govdeler_.end() ? nullptr : &it->second;
}

const GovdeKaydi* Sahne::bul(std::uint32_t kimlik) const {
  auto it = govdeler_.find(kimlik);
  return it == govdeler_.end() ? nullptr : &it->second;
}

GovdeKaydi& Sahne::ekle(std::uint32_t kimlik, std::uint16_t varlik, std::uint8_t tur) {
  GovdeKaydi& g = govdeler_[kimlik];
  g = GovdeKaydi{};
  g.kimlik = kimlik;
  g.varlik_indeksi = varlik;
  g.tur = tur;
  return g;
}

void Sahne::ornek_ekle(GovdeKaydi& g, std::int64_t t_ns, const a2::GovdeDurumu& d) {
  Ornek o;
  o.t_ns = t_ns;
  o.sureklilik = d.sureklilik_no;
  o.dolu_tupler = d.dolu_tupler;
  o.p = {d.p_n[0], d.p_n[1], d.p_n[2]};
  o.q = normalle({d.q_nb[0], d.q_nb[1], d.q_nb[2], d.q_nb[3]});
  o.v = {d.v_n[0], d.v_n[1], d.v_n[2]};
  if (!g.gecmis.empty() && g.gecmis.back().t_ns >= t_ns) {
    // Aynı zaman yinelenirse (ör. takvimli ve takvim dışı ileti aynı adımda) sonuncu geçerli.
    while (!g.gecmis.empty() && g.gecmis.back().t_ns >= t_ns) g.gecmis.pop_back();
  }
  g.gecmis.push_back(o);
}

void Sahne::gecersiz_ekle(GovdeKaydi& g, std::int64_t t_ns, std::uint32_t sureklilik, std::uint8_t dolu_tupler) {
  while (!g.gecmis.empty() && g.gecmis.back().t_ns >= t_ns) g.gecmis.pop_back();
  if (g.gecmis.empty()) return;
  Ornek o = g.gecmis.back();  // son geçerli poz (geçersiz örnekler de onu taşır)
  o.t_ns = t_ns;
  o.sureklilik = sureklilik;
  o.dolu_tupler = dolu_tupler;
  o.gecerli = false;
  o.v = {};
  g.gecmis.push_back(o);
}

bool Sahne::gorunur(const GovdeKaydi& g, std::int64_t t_op) {
  return t_op >= g.t_dogum && t_op < g.t_olum && !g.gecmis.empty();
}

bool Sahne::poz(const GovdeKaydi& g, std::int64_t t_op, Poz& out) {
  const auto& gm = g.gecmis;
  if (gm.empty()) return false;
  if (t_op <= gm.front().t_ns) {
    out = {gm.front().p, gm.front().q, gm.front().dolu_tupler};
    return true;
  }
  if (t_op >= gm.back().t_ns) {  // A2-K30: son durumun ötesine ekstrapolasyon yok
    out = {gm.back().p, gm.back().q, gm.back().dolu_tupler};
    return true;
  }
  // t0 ≤ t_op < t1 olan ardışık çift.
  auto it = std::upper_bound(gm.begin(), gm.end(), t_op,
                             [](std::int64_t t, const Ornek& o) { return t < o.t_ns; });
  const Ornek& s1 = *it;
  const Ornek& s0 = *(it - 1);
  if (s0.sureklilik != s1.sureklilik || !s0.gecerli || !s1.gecerli) {
    // A2-K32: olayın iki yanı arasında ara değerleme yok; [T − h, T) aralığında eski
    // parçanın son durumu değiştirilmeden çizilir (s0 burada o durumdur).
    // A2-K27: geçersiz örneğin iki yanında da son geçerli poz tutulur (s0'ın pozu).
    out = {s0.p, s0.q, s0.dolu_tupler};
    return true;
  }
  const double dt = static_cast<double>(s1.t_ns - s0.t_ns) * 1e-9;
  const double s = static_cast<double>(t_op - s0.t_ns) / static_cast<double>(s1.t_ns - s0.t_ns);
  out.p = hermite(s0.p, s0.v, s1.p, s1.v, dt, s);
  out.q = slerp_kisa(s0.q, s1.q, s);
  out.dolu_tupler = s0.dolu_tupler;
  return true;
}

// ---------------------------------------------------------------------------

void OperatorSaati::sifirla() {
  gecerli_ = false;
  t_son_ = 0;
  duraklatildi_ = false;
}

void OperatorSaati::durum_alindi(std::int64_t t_son, std::int64_t duvar_ns) {
  // A2-K35: sıfırlama/atlama dışında t_son (dolayısıyla t_op) azalmaz.
  t_son_ = gecerli_ ? std::max(t_son_, t_son) : t_son;
  gecerli_ = true;
  duvar_son_durum_ = duvar_ns;
}

bool OperatorSaati::ilerle(std::int64_t duvar_ns, std::int64_t& t_op) {
  if (!gecerli_) return false;
  if (!duraklatildi_ && duvar_ns - duvar_son_durum_ > kBeklemeEsigiNs) ++bekleme_;
  t_op = t_son_ - d_op_ns_;  // A2-K31
  return true;
}

}  // namespace ida::viz
