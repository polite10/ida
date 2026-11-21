// IdaA2Istemci: Godot düğümü olarak A2 istemcisi (W3-viz kapsam md. 4).
//
// İnce tutulur (07 §2.1): soket, el sıkışma, durum çözümü, operatör saati, ara değerleme,
// pozların düğümlere uygulanması, dalga parametrelerinin gölgelendiricilere verilmesi.
// Sahne kurma (manifest, glTF), su, ışık, kamera ve göstergeler GDScript'tedir; ikisi
// sinyaller ve yöntem çağrılarıyla konuşur:
//   sahne_kurulmali(bilgi)       → betik sahneyi kurar, sahne_sonucu(durum, özet) çağırır
//   govde_olustur(kimlik, varlık, tür) → betik düğümü yaratır, govde_kaydet(...) çağırır
//   govde_kaldir(kimlik)         → betik düğümü siler
//   ortam_degisti(ortam), sifirlandi(neden), baglanti_degisti(durum, ayrıntı)
// NED → Godot dönüşümü ve kuaterniyon sırasının çevrilmesi yalnız burada yapılır (07 §2.3).
#ifndef IDA_VIZ_EKLENTI_A2_ISTEMCI_HPP
#define IDA_VIZ_EKLENTI_A2_ISTEMCI_HPP

#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/classes/shader_material.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>
#include <godot_cpp/variant/packed_float64_array.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/transform3d.hpp>
#include <godot_cpp/variant/typed_array.hpp>

#include <cstdint>
#include <map>
#include <memory>
#include <vector>

#include "baglanti.hpp"

namespace ida::viz {

class IdaA2Istemci : public godot::Node {
  GDCLASS(IdaA2Istemci, godot::Node)

 public:
  IdaA2Istemci();
  ~IdaA2Istemci() override;

  void _ready() override;
  void _process(double delta) override;
  void _notification(int what);

  // ---- betikten ----
  void sahne_sonucu(int64_t durum, const godot::PackedByteArray& manifest_ozeti);
  void protokol_hatasi_bildir(const godot::String& neden);
  void govde_kaydet(int64_t kimlik, godot::Node3D* dugum, const godot::Dictionary& kosullu);
  void su_malzemesi_ekle(const godot::Ref<godot::ShaderMaterial>& malzeme);
  void kapat();

  // ---- sorgular ----
  int64_t t_op_ns() const;
  double t_op_s() const;
  double su_yuksekligi(double x_g, double z_g) const;  // ζ(t_op), Godot dünya koordinatında
  godot::Vector3 govde_konumu(int64_t kimlik) const;   // çizilen poz, Godot dünyası
  godot::PackedFloat64Array govde_ned(int64_t kimlik) const;  // çizilen poz, NED (m)
  int64_t arac_kimligi() const;
  int64_t kademe_3b() const;
  int64_t sim_kipi() const;
  int64_t calisma_durumu() const;
  bool bagli() const;
  bool tutarsiz() const;
  godot::String durum_metni() const;
  godot::Dictionary sayaclar() const;
  godot::Dictionary ortam_sozlugu() const;
  godot::Array malzeme_dizisi() const;
  int64_t port() const { return port_; }

  // ---- yardımcılar (durağan) ----
  static godot::PackedByteArray sha256(const godot::PackedByteArray& veri);
  static godot::String sha256_hex(const godot::PackedByteArray& veri);
  static godot::Transform3D ned_donusum(const godot::PackedFloat64Array& p_n, const godot::PackedFloat64Array& q_nb);
  static godot::Vector3 ned_vektor(const godot::PackedFloat64Array& v_n);

 protected:
  static void _bind_methods();

 private:
  struct Kayit {
    uint64_t dugum = 0;                        // ObjectID
    std::vector<uint64_t> tup1, tup2;          // koşullu görsel düğümler
  };

  void olaylari_isle();
  void govdeleri_esitle();
  void pozlari_uygula();
  void dalgayi_uygula();
  godot::Node3D* dugum(uint64_t id) const;
  void tumunu_kaldir();

  std::unique_ptr<IstemciBaglantisi> baglanti_;
  uint16_t port_ = a2::kVarsayilanPort;
  int64_t d_op_ns_ = a2::kOperatorGecikmesiNs;
  bool sahne_hazir_ = false;
  bool baslatildi_ = false;
  int64_t t_op_ = 0;
  bool t_op_gecerli_ = false;
  std::map<uint32_t, Kayit> kayitlar_;
  std::map<uint32_t, bool> istenen_;  // govde_olustur gönderildi, kayıt bekleniyor
  std::vector<uint64_t> su_malzemeleri_;
  godot::String son_durum_;
  int son_baglanti_durumu_ = -1;
};

}  // namespace ida::viz

#endif
