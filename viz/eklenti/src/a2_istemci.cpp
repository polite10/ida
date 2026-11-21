#include "a2_istemci.hpp"

#include <godot_cpp/classes/display_server.hpp>
#include <godot_cpp/classes/engine.hpp>
#include <godot_cpp/classes/os.hpp>
#include <godot_cpp/classes/rendering_server.hpp>
#include <godot_cpp/classes/scene_tree.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/object.hpp>
#include <godot_cpp/variant/packed_float32_array.hpp>
#include <godot_cpp/variant/packed_string_array.hpp>
#include <godot_cpp/variant/packed_vector4_array.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

#include <cerrno>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <string>

#include "ida/viz/cerceve.hpp"
#include "ida/viz/dalga.hpp"
#include "ida/viz/saat.hpp"
#include "ida/viz/sha256.hpp"

using namespace godot;

namespace ida::viz {

namespace {

constexpr const char* kUzantiSurumu = "ida-viz w3-dilim1";

std::string std_metin(const String& s) { return std::string(s.utf8().get_data()); }

// Godot dönüşümü: konum_G = C p_n, taban_G = C R(q) Cᵀ; Godot Quaternion(x, y, z, w).
Transform3D godot_donusum(const V3& p_n, const Q& q_nb) {
  const V3 p = ned_godot_konum(p_n);
  const Q q = ned_godot_kuaterniyon(q_nb);
  const Quaternion gq(static_cast<real_t>(q.x), static_cast<real_t>(q.y), static_cast<real_t>(q.z),
                      static_cast<real_t>(q.w));
  return Transform3D(Basis(gq), Vector3(static_cast<real_t>(p.x), static_cast<real_t>(p.y), static_cast<real_t>(p.z)));
}

bool tamsayi(const String& s, long long alt, long long ust, long long& out) {
  const std::string t = std_metin(s);
  if (t.empty()) return false;
  char* son = nullptr;
  errno = 0;
  const long long v = std::strtoll(t.c_str(), &son, 10);
  if (errno != 0 || *son != '\0' || v < alt || v > ust) return false;
  out = v;
  return true;
}

// "--ad değer" ya da "--ad=değer" biçiminde seçenek (A2-K93 komut satırı biçimi).
bool secenek(const PackedStringArray& args, const char* ad, String& deger, bool& var) {
  const String a(ad);
  for (int64_t i = 0; i < args.size(); ++i) {
    const String x = args[i];
    if (x == a) {
      var = true;
      if (i + 1 >= args.size()) return false;
      deger = args[i + 1];
      return true;
    }
    if (x.begins_with(a + String("="))) {
      var = true;
      deger = x.substr(a.length() + 1);
      return true;
    }
  }
  return true;
}

std::string onaltilik_64(uint64_t v) {
  static const char kHane[] = "0123456789abcdef";
  char t[17];
  for (int i = 15; i >= 0; --i) {
    t[i] = kHane[v & 0xFu];
    v >>= 4;
  }
  t[16] = '\0';
  return t;
}

}  // namespace

IdaA2Istemci::IdaA2Istemci() { set_process_priority(-100); }

IdaA2Istemci::~IdaA2Istemci() = default;

void IdaA2Istemci::_bind_methods() {
  ClassDB::bind_method(D_METHOD("sahne_sonucu", "durum", "manifest_ozeti"), &IdaA2Istemci::sahne_sonucu);
  ClassDB::bind_method(D_METHOD("protokol_hatasi_bildir", "neden"), &IdaA2Istemci::protokol_hatasi_bildir);
  ClassDB::bind_method(D_METHOD("govde_kaydet", "kimlik", "dugum", "kosullu"), &IdaA2Istemci::govde_kaydet);
  ClassDB::bind_method(D_METHOD("su_malzemesi_ekle", "malzeme"), &IdaA2Istemci::su_malzemesi_ekle);
  ClassDB::bind_method(D_METHOD("kapat"), &IdaA2Istemci::kapat);
  ClassDB::bind_method(D_METHOD("t_op_ns"), &IdaA2Istemci::t_op_ns);
  ClassDB::bind_method(D_METHOD("t_op_s"), &IdaA2Istemci::t_op_s);
  ClassDB::bind_method(D_METHOD("su_yuksekligi", "x", "z"), &IdaA2Istemci::su_yuksekligi);
  ClassDB::bind_method(D_METHOD("govde_konumu", "kimlik"), &IdaA2Istemci::govde_konumu);
  ClassDB::bind_method(D_METHOD("govde_ned", "kimlik"), &IdaA2Istemci::govde_ned);
  ClassDB::bind_method(D_METHOD("arac_kimligi"), &IdaA2Istemci::arac_kimligi);
  ClassDB::bind_method(D_METHOD("kademe_3b"), &IdaA2Istemci::kademe_3b);
  ClassDB::bind_method(D_METHOD("sim_kipi"), &IdaA2Istemci::sim_kipi);
  ClassDB::bind_method(D_METHOD("calisma_durumu"), &IdaA2Istemci::calisma_durumu);
  ClassDB::bind_method(D_METHOD("bagli"), &IdaA2Istemci::bagli);
  ClassDB::bind_method(D_METHOD("tutarsiz"), &IdaA2Istemci::tutarsiz);
  ClassDB::bind_method(D_METHOD("durum_metni"), &IdaA2Istemci::durum_metni);
  ClassDB::bind_method(D_METHOD("sayaclar"), &IdaA2Istemci::sayaclar);
  ClassDB::bind_method(D_METHOD("ortam_sozlugu"), &IdaA2Istemci::ortam_sozlugu);
  ClassDB::bind_method(D_METHOD("port"), &IdaA2Istemci::port);
  ClassDB::bind_static_method("IdaA2Istemci", D_METHOD("sha256", "veri"), &IdaA2Istemci::sha256);
  ClassDB::bind_static_method("IdaA2Istemci", D_METHOD("sha256_hex", "veri"), &IdaA2Istemci::sha256_hex);
  ClassDB::bind_static_method("IdaA2Istemci", D_METHOD("ned_donusum", "p_n", "q_nb"), &IdaA2Istemci::ned_donusum);
  ClassDB::bind_static_method("IdaA2Istemci", D_METHOD("ned_vektor", "v_n"), &IdaA2Istemci::ned_vektor);

  ADD_SIGNAL(MethodInfo("sahne_kurulmali", PropertyInfo(Variant::DICTIONARY, "bilgi")));
  ADD_SIGNAL(MethodInfo("govde_olustur", PropertyInfo(Variant::INT, "kimlik"), PropertyInfo(Variant::INT, "varlik"),
                        PropertyInfo(Variant::INT, "tur")));
  ADD_SIGNAL(MethodInfo("govde_kaldir", PropertyInfo(Variant::INT, "kimlik")));
  ADD_SIGNAL(MethodInfo("ortam_degisti", PropertyInfo(Variant::DICTIONARY, "ortam")));
  ADD_SIGNAL(MethodInfo("sifirlandi", PropertyInfo(Variant::STRING, "neden")));
  ADD_SIGNAL(MethodInfo("malzeme_degisti", PropertyInfo(Variant::ARRAY, "girdiler")));
  ADD_SIGNAL(MethodInfo("baglanti_degisti", PropertyInfo(Variant::STRING, "durum"),
                        PropertyInfo(Variant::STRING, "ayrinti")));
}

void IdaA2Istemci::_ready() {
  if (Engine::get_singleton()->is_editor_hint()) return;
  // A2-K93: --a2-port (1024–65535), --a2-operator-gecikme-us (0–100 000). Geçersizse başlamaz.
  const PackedStringArray args = OS::get_singleton()->get_cmdline_user_args();
  String deger;
  bool var = false;
  long long v = 0;
  if (!secenek(args, "--a2-port", deger, var) || (var && !tamsayi(deger, 1024, 65535, v))) {
    UtilityFunctions::push_error(String::utf8("ida-viz: geçersiz --a2-port değeri: "), deger);
    get_tree()->quit(2);
    return;
  }
  if (var) port_ = static_cast<uint16_t>(v);
  var = false;
  if (!secenek(args, "--a2-operator-gecikme-us", deger, var) || (var && !tamsayi(deger, 0, 100000, v))) {
    UtilityFunctions::push_error(String::utf8("ida-viz: geçersiz --a2-operator-gecikme-us değeri: "), deger);
    get_tree()->quit(2);
    return;
  }
  if (var) d_op_ns_ = static_cast<int64_t>(v) * 1000;

  IstemciOturumu::Ayarlar a;
  const Dictionary surum = Engine::get_singleton()->get_version_info();
  a.godot_surumu = std_metin(String(surum.get("string", "?")));
  a.uzanti_surumu = kUzantiSurumu;
  if (RenderingServer::get_singleton()) a.gpu_adi = std_metin(RenderingServer::get_singleton()->get_video_adapter_name());
  a.surec_kimligi = static_cast<uint32_t>(OS::get_singleton()->get_process_id());
#if defined(_WIN32)
  a.platform = static_cast<uint8_t>(a2::Platform::windows_x64);
#else
  a.platform = static_cast<uint8_t>(a2::Platform::macos_arm64);
#endif
  a.d_op_ns = d_op_ns_;
  baglanti_ = std::make_unique<IstemciBaglantisi>(a, port_);
  baslatildi_ = true;
  UtilityFunctions::print("ida-viz: A2 istemcisi 127.0.0.1:", static_cast<int64_t>(port_), ", D_op ",
                          d_op_ns_ / 1000, String::utf8(" µs"));
}

void IdaA2Istemci::_notification(int what) {
  if (what == NOTIFICATION_WM_CLOSE_REQUEST || what == NOTIFICATION_PREDELETE) kapat();
}

void IdaA2Istemci::kapat() {
  if (baglanti_) baglanti_->olagan_kapat();  // A2-K67: veda(olagan)
}

Node3D* IdaA2Istemci::dugum(uint64_t id) const {
  if (id == 0) return nullptr;
  return Object::cast_to<Node3D>(ObjectDB::get_instance(id));
}

void IdaA2Istemci::tumunu_kaldir() {
  for (const auto& [kimlik, k] : kayitlar_) emit_signal("govde_kaldir", static_cast<int64_t>(kimlik));
  for (const auto& [kimlik, b] : istenen_) emit_signal("govde_kaldir", static_cast<int64_t>(kimlik));
  kayitlar_.clear();
  istenen_.clear();
}

void IdaA2Istemci::_process(double) {
  if (!baslatildi_ || !baglanti_) return;
  const int64_t simdi = duvar_ns();
  const double fps = Engine::get_singleton()->get_frames_per_second();
  const auto cizim_mhz = static_cast<uint32_t>(fps * 1000.0);
  baglanti_->adim(simdi, cizim_mhz, true);

  const int bd = static_cast<int>(baglanti_->durum());
  const bool akis = baglanti_->oturum().asama() == Asama::akis;
  const String metin = durum_metni();
  if (bd != son_baglanti_durumu_ || metin != son_durum_) {
    son_baglanti_durumu_ = bd;
    son_durum_ = metin;
    emit_signal("baglanti_degisti", metin, String::utf8(baglanti_->son_hata().c_str()));
  }

  olaylari_isle();
  if (!sahne_hazir_) return;

  // A2-K63: bağlantı yokken sahne donar (son çizilen pozlar kalır).
  if (baglanti_->durum() == IstemciBaglantisi::Durum::bagli && (akis || baglanti_->oturum().asama() ==
                                                                             Asama::uygulama_bekleniyor)) {
    int64_t t = 0;
    t_op_gecerli_ = baglanti_->oturum().saat().ilerle(simdi, t);
    if (t_op_gecerli_) {
      t_op_ = t;
      baglanti_->oturum().sahne().buda(t_op_, [this](uint32_t kimlik) {
        if (kayitlar_.erase(kimlik) > 0 || istenen_.erase(kimlik) > 0)
          emit_signal("govde_kaldir", static_cast<int64_t>(kimlik));
      });
    }
    govdeleri_esitle();
    if (t_op_gecerli_) {
      pozlari_uygula();
      dalgayi_uygula();
    }
  }
}

void IdaA2Istemci::olaylari_isle() {
  for (const Olay& o : baglanti_->oturum().olaylari_al()) {
    switch (o.tur) {
      case OlayTuru::sahne_kur: {
        // Yeni oturum: önceki oturumun gövdeleri gider (kimlikler oturuma özgü).
        tumunu_kaldir();
        sahne_hazir_ = false;
        t_op_gecerli_ = false;
        const auto& h = baglanti_->oturum().hosgeldin();
        Dictionary bilgi;
        bilgi["manifest_yolu"] = String::utf8(o.metin.c_str());
        bilgi["manifest_ozeti"] = String::utf8(onaltilik([&] {
                                                 Ozet z{};
                                                 std::memcpy(z.data(), h.h.manifest_sha256, 32);
                                                 return z;
                                               }()).c_str());
        bilgi["kademe_3b"] = static_cast<int64_t>(h.h.kademe_3b);
        bilgi["sim_kipi"] = static_cast<int64_t>(h.h.sim_kipi);
        bilgi["oturum_kimligi"] = String(onaltilik_64(h.h.oturum_kimligi).c_str());
        bilgi["calisma_nesli"] = static_cast<int64_t>(h.h.calisma_nesli);
        Array govdeler;
        for (const auto& g : h.govdeler) {
          Dictionary d;
          d["kimlik"] = static_cast<int64_t>(g.govde_kimligi);
          d["varlik"] = static_cast<int64_t>(g.varlik_indeksi);
          d["tur"] = static_cast<int64_t>(g.govde_turu);
          govdeler.push_back(d);
        }
        bilgi["govdeler"] = govdeler;
        bilgi["ortam"] = ortam_sozlugu();
        bilgi["malzeme_optik"] = malzeme_dizisi();  // A7-K30
        emit_signal("sahne_kurulmali", bilgi);
        break;
      }
      case OlayTuru::ortam_degisti:
        emit_signal("ortam_degisti", ortam_sozlugu());
        break;
      case OlayTuru::malzeme_degisti:
        emit_signal("malzeme_degisti", malzeme_dizisi());  // A7-K30 (sol #10)
        break;
      case OlayTuru::sifirlandi:
        t_op_gecerli_ = false;
        emit_signal("sifirlandi", String::utf8(o.metin.c_str()));
        break;
      case OlayTuru::kapandi:
        UtilityFunctions::print(String::utf8("ida-viz: oturum kapandı: "), String::utf8(o.metin.c_str()));
        break;
      default:
        break;
    }
  }
}

void IdaA2Istemci::sahne_sonucu(int64_t durum, const PackedByteArray& manifest_ozeti) {
  if (!baglanti_) return;
  Ozet o{};
  if (manifest_ozeti.size() == 32) std::memcpy(o.data(), manifest_ozeti.ptr(), 32);
  const auto d = static_cast<a2::HazirDurumu>(durum < 0 || durum > 7 ? 1 : durum);
  baglanti_->oturum().sahne_sonucu(d, o);
  sahne_hazir_ = d == a2::HazirDurumu::basarili;
}

void IdaA2Istemci::protokol_hatasi_bildir(const String& neden) {
  if (baglanti_) baglanti_->oturum().protokol_hatasi(std_metin(neden));
}

void IdaA2Istemci::govdeleri_esitle() {
  const Sahne& s = baglanti_->oturum().sahne();
  // Sahneden çıkmış (sıfırlama, atlama tablosu) gövdeler.
  std::vector<uint32_t> giden;
  for (const auto& [kimlik, k] : kayitlar_) {
    if (!s.bul(kimlik)) giden.push_back(kimlik);
  }
  for (const auto& [kimlik, b] : istenen_) {
    if (!s.bul(kimlik)) giden.push_back(kimlik);
  }
  for (uint32_t kimlik : giden) {
    kayitlar_.erase(kimlik);
    istenen_.erase(kimlik);
    emit_signal("govde_kaldir", static_cast<int64_t>(kimlik));
  }
  // Yeni gövdeler: betikten düğüm iste.
  for (const auto& [kimlik, g] : s.govdeler()) {
    if (kayitlar_.count(kimlik) || istenen_.count(kimlik)) continue;
    istenen_[kimlik] = true;
    emit_signal("govde_olustur", static_cast<int64_t>(kimlik), static_cast<int64_t>(g.varlik_indeksi),
                static_cast<int64_t>(g.tur));
  }
}

void IdaA2Istemci::govde_kaydet(int64_t kimlik, Node3D* d, const Dictionary& kosullu) {
  if (!d) return;
  const auto k = static_cast<uint32_t>(kimlik);
  Kayit kayit;
  kayit.dugum = d->get_instance_id();
  for (int tup = 1; tup <= 2; ++tup) {
    const Array dizi = kosullu.get(tup, Array());
    for (int64_t i = 0; i < dizi.size(); ++i) {
      Node3D* n = Object::cast_to<Node3D>(dizi[i]);
      if (n) (tup == 1 ? kayit.tup1 : kayit.tup2).push_back(n->get_instance_id());
    }
  }
  istenen_.erase(k);
  kayitlar_[k] = std::move(kayit);
  d->set_visible(false);  // ilk poz uygulanana kadar
}

void IdaA2Istemci::pozlari_uygula() {
  const Sahne& s = baglanti_->oturum().sahne();
  for (const auto& [kimlik, k] : kayitlar_) {
    Node3D* d = dugum(k.dugum);
    const GovdeKaydi* g = s.bul(kimlik);
    if (!d || !g) continue;
    Poz p;
    const bool gor = Sahne::gorunur(*g, t_op_) && Sahne::poz(*g, t_op_, p);  // A2-K33
    d->set_visible(gor);
    if (!gor) continue;
    d->set_transform(godot_donusum(p.p, p.q));
    // A7 §3.3: koşullu görsel düğüm yalnız tüp doluyken çizilir (A2 dolu_tupler).
    for (uint64_t id : k.tup1) {
      if (Node3D* n = dugum(id)) n->set_visible((p.dolu_tupler & 0x1u) != 0);
    }
    for (uint64_t id : k.tup2) {
      if (Node3D* n = dugum(id)) n->set_visible((p.dolu_tupler & 0x2u) != 0);
    }
  }
}

void IdaA2Istemci::su_malzemesi_ekle(const Ref<ShaderMaterial>& malzeme) {
  if (malzeme.is_valid()) su_malzemeleri_.push_back(malzeme->get_instance_id());
}

void IdaA2Istemci::dalgayi_uygula() {
  if (su_malzemeleri_.empty()) return;
  const auto& bilesenler = baglanti_->oturum().dalga().bilesenler;
  // Sade kademe düz su çizer (A2 §6); dalga kümesi yine alınır ama uygulanmaz.
  const bool tam = baglanti_->oturum().hosgeldin().h.kademe_3b == static_cast<uint8_t>(a2::Kademe::tam);
  const int64_t n = tam ? static_cast<int64_t>(bilesenler.size()) : 0;
  PackedVector4Array k;
  PackedFloat32Array faz;
  k.resize(256);
  faz.resize(256);
  // Paket, CPU yüksekliğiyle aynı koddan (viz/a2 dalga_paketle; host.viz.dalga sınar).
  DalgaPaketi p;
  dalga_paketle(bilesenler, t_op_, p);
  for (int64_t i = 0; i < n; ++i) {
    const auto j = static_cast<std::size_t>(i);
    k.set(i, Vector4(p.kx[j], p.kz[j], p.a[j], p.k[j]));
    faz.set(i, static_cast<double>(p.faz[j]));
  }
  std::vector<uint64_t> canli;
  for (uint64_t id : su_malzemeleri_) {
    ShaderMaterial* m = Object::cast_to<ShaderMaterial>(ObjectDB::get_instance(id));
    if (!m) continue;
    canli.push_back(id);
    m->set_shader_parameter("dalga_sayisi", n);
    m->set_shader_parameter("dalga_k", k);
    m->set_shader_parameter("dalga_faz", faz);
  }
  su_malzemeleri_.swap(canli);
}

// ---- sorgular ----

int64_t IdaA2Istemci::t_op_ns() const { return t_op_; }
double IdaA2Istemci::t_op_s() const { return static_cast<double>(t_op_) * 1e-9; }

double IdaA2Istemci::su_yuksekligi(double x_g, double z_g) const {
  if (!baglanti_) return 0.0;
  // Godot (x, z) → NED (N = −z, E = x).
  return dalga_yuksekligi(baglanti_->oturum().dalga().bilesenler, -z_g, x_g, t_op_);
}

Vector3 IdaA2Istemci::govde_konumu(int64_t kimlik) const {
  const PackedFloat64Array p = govde_ned(kimlik);
  if (p.size() != 3) return Vector3();
  const V3 g = ned_godot_konum({p[0], p[1], p[2]});
  return Vector3(static_cast<real_t>(g.x), static_cast<real_t>(g.y), static_cast<real_t>(g.z));
}

PackedFloat64Array IdaA2Istemci::govde_ned(int64_t kimlik) const {
  PackedFloat64Array out;
  if (!baglanti_) return out;
  const GovdeKaydi* g = baglanti_->oturum().sahne().bul(static_cast<uint32_t>(kimlik));
  Poz p;
  if (!g || !Sahne::poz(*g, t_op_, p)) return out;
  out.push_back(p.p.x);
  out.push_back(p.p.y);
  out.push_back(p.p.z);
  return out;
}

int64_t IdaA2Istemci::arac_kimligi() const {
  if (!baglanti_) return -1;
  for (const auto& [kimlik, g] : baglanti_->oturum().sahne().govdeler()) {
    if (g.tur == static_cast<uint8_t>(a2::GovdeTuru::arac) && Sahne::gorunur(g, t_op_)) return kimlik;
  }
  return -1;
}

int64_t IdaA2Istemci::kademe_3b() const {
  return baglanti_ ? baglanti_->oturum().hosgeldin().h.kademe_3b : 0;
}
int64_t IdaA2Istemci::sim_kipi() const { return baglanti_ ? baglanti_->oturum().hosgeldin().h.sim_kipi : 0; }
int64_t IdaA2Istemci::calisma_durumu() const { return baglanti_ ? baglanti_->oturum().calisma_durumu() : 0; }
bool IdaA2Istemci::bagli() const {
  return baglanti_ && baglanti_->durum() == IstemciBaglantisi::Durum::bagli &&
         baglanti_->oturum().asama() == Asama::akis;
}
bool IdaA2Istemci::tutarsiz() const { return baglanti_ && baglanti_->oturum().tutarsiz(); }

String IdaA2Istemci::durum_metni() const {
  if (!baglanti_) return String::utf8("başlatılmadı");
  switch (baglanti_->durum()) {
    case IstemciBaglantisi::Durum::bekliyor:
    case IstemciBaglantisi::Durum::baglaniyor:
      return String::utf8("bağlantı yok");
    case IstemciBaglantisi::Durum::durdu:
      return String::utf8("durdu (yeniden bağlanılmaz)");
    case IstemciBaglantisi::Durum::bagli:
      break;
  }
  switch (baglanti_->oturum().asama()) {
    case Asama::akis: return String::utf8("bağlı");
    case Asama::kapali: return String::utf8("bağlantı yok");
    default: return String::utf8("el sıkışma");
  }
}

Dictionary IdaA2Istemci::sayaclar() const {
  Dictionary d;
  if (!baglanti_) return d;
  const auto& s = baglanti_->oturum().sayaclar();
  d["alinan_ileti"] = static_cast<int64_t>(s.alinan_ileti);
  d["sira_boslugu"] = static_cast<int64_t>(s.sira_boslugu);
  d["gecersiz_govde"] = static_cast<int64_t>(s.gecersiz_govde);
  d["eksik_govde"] = static_cast<int64_t>(s.eksik_govde);
  d["dusen_kare"] = static_cast<int64_t>(s.dusen_kare);
  d["operator_bekleme"] = static_cast<int64_t>(baglanti_->oturum().saat().bekleme_sayaci());
  d["baglanti_sayisi"] = static_cast<int64_t>(baglanti_->baglanti_sayisi());
  d["calisma_nesli"] = static_cast<int64_t>(baglanti_->oturum().calisma_nesli());
  d["t_son_ns"] = baglanti_->oturum().saat().t_son();
  return d;
}

Array IdaA2Istemci::malzeme_dizisi() const {
  Array dizi;
  if (!baglanti_) return dizi;
  for (const auto& g : baglanti_->oturum().malzeme().girdiler) {
    Dictionary d;
    d["malzeme_indeksi"] = static_cast<int64_t>(g.malzeme_indeksi);
    d["maske"] = static_cast<int64_t>(g.maske);
    PackedFloat64Array renk;
    for (double x : g.taban_rengi_dogrusal) renk.push_back(x);
    d["taban_rengi"] = renk;
    d["metaliklik"] = g.metaliklik;
    d["puruzluluk"] = g.puruzluluk;
    dizi.push_back(d);
  }
  return dizi;
}

Dictionary IdaA2Istemci::ortam_sozlugu() const {
  Dictionary d;
  if (!baglanti_) return d;
  const a2::Ortam& o = baglanti_->oturum().ortam();
  auto dizi3 = [](const double (&v)[3]) {
    PackedFloat64Array a;
    for (double x : v) a.push_back(x);
    return a;
  };
  PackedFloat64Array q;
  for (double x : o.statik_q_nb) q.push_back(x);
  d["surum"] = static_cast<int64_t>(o.ortam_surumu);
  d["tur"] = static_cast<int64_t>(o.ortam_turu);
  d["statik_varlik"] = static_cast<int64_t>(o.statik_varlik_indeksi);
  d["statik_p_n"] = dizi3(o.statik_p_n);
  d["statik_q_nb"] = q;
  d["gunes_yonu_n"] = dizi3(o.gunes_yonu_n);
  d["gunes_isinimi"] = o.gunes_isinimi_w_m2;
  d["ortam_isigi"] = dizi3(o.ortam_isigi_rgb_dogrusal);
  d["beta_d"] = dizi3(o.beta_d_m);
  d["beta_b"] = dizi3(o.beta_b_m);
  d["b_sonsuz"] = dizi3(o.b_sonsuz_rgb_dogrusal);
  d["bulaniklik"] = o.bulaniklik_olcegi;
  return d;
}

// ---- durağan yardımcılar ----

PackedByteArray IdaA2Istemci::sha256(const PackedByteArray& veri) {
  const Ozet o = ida::viz::sha256(veri.ptr(), static_cast<std::size_t>(veri.size()));
  PackedByteArray out;
  out.resize(32);
  std::memcpy(out.ptrw(), o.data(), 32);
  return out;
}

String IdaA2Istemci::sha256_hex(const PackedByteArray& veri) {
  return String(onaltilik(ida::viz::sha256(veri.ptr(), static_cast<std::size_t>(veri.size()))).c_str());
}

Transform3D IdaA2Istemci::ned_donusum(const PackedFloat64Array& p_n, const PackedFloat64Array& q_nb) {
  if (p_n.size() != 3 || q_nb.size() != 4) return Transform3D();
  return godot_donusum({p_n[0], p_n[1], p_n[2]}, normalle({q_nb[0], q_nb[1], q_nb[2], q_nb[3]}));
}

Vector3 IdaA2Istemci::ned_vektor(const PackedFloat64Array& v) {
  if (v.size() != 3) return Vector3();
  const V3 g = ned_godot_konum({v[0], v[1], v[2]});
  return Vector3(static_cast<real_t>(g.x), static_cast<real_t>(g.y), static_cast<real_t>(g.z));
}

}  // namespace ida::viz
