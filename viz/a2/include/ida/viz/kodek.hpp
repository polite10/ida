// A2 tel kodlaması: çerçeveleme, iletilerin kodlanması, çözülmesi ve alan geçerliliği.
//
// Sözleşme: docs/sozlesmeler/A2-gorsellestirici.md §3, §4.1 (A2-K06, A2-K07). Bayt düzeni
// sim/include/ida/a2_wire.h'dedir (dokunulmaz). Bu kitaplık sim sunucusu (sahte kaynak),
// görselleştirici eklentisi ve testler tarafından aynı kaynaktan kullanılır (07 §2.1).
//
// Yapılar bellekten bellek kopyasıyla yazılıp okunur: a2_wire.h düzeni static_assert ile
// sabitler, ana makinenin küçük sonlu olduğunu da denetler; sözleşme bunu açıkça serbest
// bırakır ("zorunlu olan, sözleşmedeki bayt düzenidir"). Bütün sözleşme geçerlilik
// denetimleri burada, alan alan yapılır.
#ifndef IDA_VIZ_KODEK_HPP
#define IDA_VIZ_KODEK_HPP

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

#include "ida/a2_wire.h"

namespace ida::viz {

namespace a2 = ida::a2;

using Bayt = std::uint8_t;
using Baytlar = std::vector<Bayt>;
using BaytGorunumu = std::span<const Bayt>;

// İletinin yönü (§3.2).
enum class Yon : std::uint8_t { sim_gorsel, gorsel_sim, iki_yon };

// Çözme hataları. `yok` dışındaki her değer A2-K06'ya göre protokol hatasıdır.
enum class KodHata : std::uint8_t {
  yok = 0,
  sihirli,         // sihirli sayı yanlış
  tur_bilinmiyor,  // ileti türü tabloda yok
  yon_yanlis,      // tür biliniyor ama alıcıya bu yönde gelmez
  bayraklar,       // başlık bayrakları ≠ 0
  yuk_buyuk,       // yuk_bayt > 1 MiB
  uzunluk,         // yuk_bayt türün beklenen boyutundan farklı
  metin,           // metin alanında NUL yok
  sinir,           // sayı alanı §3'teki sınırı aşıyor (gövde, bileşen, kamera, girdi sayısı)
  alan,            // alan değeri geçersiz (§3 tabloları: "geçersizse protokol hatası")
  ozet,            // dalga kümesi özeti tutmuyor (H17)
};

const char* hata_adi(KodHata h);

struct TurBilgisi {
  bool bilinen = false;
  Yon yon = Yon::sim_gorsel;
  std::uint32_t sabit_boyut = 0;  // sabit iletilerde tam boyut, değişkenlerde sabit kısım
  bool degisken = false;
  const char* ad = "?";
};

TurBilgisi tur_bilgisi(std::uint16_t tur);

// ---------------------------------------------------------------------------
// Yapı ↔ bayt
// ---------------------------------------------------------------------------

template <class T>
void yapi_ekle(Baytlar& out, const T& v) {
  static_assert(std::is_trivially_copyable_v<T>);
  const std::size_t n = out.size();
  out.resize(n + sizeof(T));
  std::memcpy(out.data() + n, &v, sizeof(T));
}

template <class T>
T yapi_oku(const Bayt* p) {
  static_assert(std::is_trivially_copyable_v<T>);
  T v;
  std::memcpy(&v, p, sizeof(T));
  return v;
}

// Metin alanı: UTF-8, NUL ile biter, NUL'dan sonraki baytlar sıfır (a2_wire.h başı).
// Sığmayan metin, son bayt NUL kalacak biçimde kesilir (UTF-8 dizisini bölmeden).
void metin_yaz(char* alan, std::size_t boyut, std::string_view s);
bool metin_gecerli(const char* alan, std::size_t boyut);
std::string metin_oku(const char* alan, std::size_t boyut);

// ---------------------------------------------------------------------------
// Sıra aritmetiği (iskelet §3.1)
// ---------------------------------------------------------------------------

// a, b'den yenidir ⇔ (a − b) mod 2³² ∈ [1, 2³¹ − 1].
constexpr bool sira_yeni(std::uint32_t a, std::uint32_t b) {
  const std::uint32_t f = a - b;
  return f >= 1u && f <= 0x7FFFFFFFu;
}

// ---------------------------------------------------------------------------
// Çerçeveleme
// ---------------------------------------------------------------------------

// Bayt akışından başlık + yük çerçevelerini ayırır. Başlık denetimleri (A2-K06): sihirli
// sayı, tür, yön, bayraklar, yük üst sınırı ve sabit iletilerde tam boyut. Değişken
// iletilerin boyut tutarlılığı çözücülerde denetlenir.
// Okuyucuda bekleyebilecek en çok bayt: bir tam çerçeve (16 B + 1 MiB) ve bir okuma parçası.
// Çağıranlar veriyi bu boyuttan küçük parçalarla verip her parçadan sonra çerçeveleri çeker
// (sol #1); sınır aşılırsa ekleme reddedilir ve bu protokol hatasıdır.
inline constexpr std::size_t kEklemeParcasi = 64u * 1024u;
inline constexpr std::size_t kAzamiBekleyenBayt = a2::kBaslikBoyutu + a2::kAzamiYuk + kEklemeParcasi;

class CerceveOkuyucu {
 public:
  explicit CerceveOkuyucu(Yon alici) : alici_(alici) {}
  // false: bekleyen bayt kAzamiBekleyenBayt'ı aşardı (eklenmedi).
  [[nodiscard]] bool ekle(const Bayt* veri, std::size_t n);
  // true: bir çerçeve çıktı. false: veri yetmiyor (hata == yok) ya da protokol hatası.
  bool sonraki(a2::Baslik& baslik, Baytlar& yuk, KodHata& hata);
  void temizle();
  std::size_t bekleyen() const { return tampon_.size() - bas_; }

 private:
  Yon alici_;
  Baytlar tampon_;
  std::size_t bas_ = 0;
};

// Tam ileti (başlık + yük) yazımı.
void ileti_yaz(Baytlar& out, a2::IletiTuru tur, std::uint32_t sira, BaytGorunumu yuk);

// ---------------------------------------------------------------------------
// Çözülmüş ileti tipleri (değişken uzunluklular)
// ---------------------------------------------------------------------------

struct HosgeldinIleti {
  a2::Hosgeldin h{};
  std::vector<a2::GovdeBilgisi> govdeler;
  std::vector<a2::KameraYapilandirmasi> kameralar;
  std::vector<a2::HalkaBilgisi> halkalar;
};

struct DalgaIleti {
  a2::DalgaKumesi d{};
  std::vector<a2::DalgaBileseni> bilesenler;
};

struct MalzemeIleti {
  a2::MalzemeOptik m{};
  std::vector<a2::MalzemeOptikGirdisi> girdiler;
};

struct DurumIleti {
  a2::Durum d{};
  std::vector<a2::GovdeDurumu> govdeler;
};

struct KameraIstegiIleti {
  a2::KameraIstegi k{};
  std::vector<a2::GovdeDurumu> govdeler;
};

struct GovdeTablosuIleti {
  a2::GovdeTablosu t{};
  std::vector<a2::GovdeBilgisi> govdeler;
};

// ---------------------------------------------------------------------------
// Çözücüler: yük baytlarından (başlık hariç). Boyut ve alan geçerliliği denetlenir.
// `adim_ns`, oturumun `hosgeldin.adim_ns`'idir (t_ns = adim_no · adim_ns denetimi için).
// ---------------------------------------------------------------------------

// sim → görselleştirici
KodHata coz_hosgeldin(BaytGorunumu yuk, HosgeldinIleti& out);
KodHata coz_dalga(BaytGorunumu yuk, DalgaIleti& out);
KodHata coz_ortam(BaytGorunumu yuk, a2::Ortam& out);
KodHata coz_malzeme(BaytGorunumu yuk, MalzemeIleti& out);
KodHata coz_durum(BaytGorunumu yuk, std::int64_t adim_ns, DurumIleti& out);
KodHata coz_sahne_olayi(BaytGorunumu yuk, std::int64_t adim_ns, a2::SahneOlayi& out);
KodHata coz_kamera_istegi(BaytGorunumu yuk, KameraIstegiIleti& out);
KodHata coz_kamera_iptal(BaytGorunumu yuk, a2::KameraIptal& out);
KodHata coz_sim_nabzi(BaytGorunumu yuk, a2::SimNabzi& out);
KodHata coz_halka_bilgisi(BaytGorunumu yuk, a2::HalkaBilgisi& out);
KodHata coz_govde_tablosu(BaytGorunumu yuk, std::int64_t adim_ns, GovdeTablosuIleti& out);
KodHata coz_ret(BaytGorunumu yuk, a2::Ret& out);
// iki yön
KodHata coz_veda(BaytGorunumu yuk, a2::Veda& out);
// görselleştirici → sim
KodHata coz_merhaba(BaytGorunumu yuk, a2::Merhaba& out);
KodHata coz_hazir(BaytGorunumu yuk, a2::Hazir& out);
KodHata coz_goruntuleyici_nabzi(BaytGorunumu yuk, a2::GoruntuleyiciNabzi& out);
KodHata coz_kare_hazir(BaytGorunumu yuk, a2::KareHazir& out);
KodHata coz_kare_dustu(BaytGorunumu yuk, a2::KareDustu& out);
KodHata coz_halka_eslendi(BaytGorunumu yuk, a2::HalkaEslendi& out);

// ---------------------------------------------------------------------------
// Kodlayıcılar: tam iletiyi (başlık + yük) `out`'un sonuna ekler.
// ---------------------------------------------------------------------------

void kodla_merhaba(Baytlar& out, std::uint32_t sira, const a2::Merhaba& m);
void kodla_hazir(Baytlar& out, std::uint32_t sira, const a2::Hazir& h);
void kodla_goruntuleyici_nabzi(Baytlar& out, std::uint32_t sira, const a2::GoruntuleyiciNabzi& n);
void kodla_kare_hazir(Baytlar& out, std::uint32_t sira, const a2::KareHazir& k);
void kodla_kare_dustu(Baytlar& out, std::uint32_t sira, const a2::KareDustu& k);
void kodla_halka_eslendi(Baytlar& out, std::uint32_t sira, const a2::HalkaEslendi& h);
void kodla_veda(Baytlar& out, std::uint32_t sira, a2::VedaNedeni neden);

void kodla_hosgeldin(Baytlar& out, std::uint32_t sira, const HosgeldinIleti& h);
void kodla_ret(Baytlar& out, std::uint32_t sira, a2::RetNedeni neden, std::string_view ayrinti);
void kodla_dalga(Baytlar& out, std::uint32_t sira, const DalgaIleti& d);
void kodla_ortam(Baytlar& out, std::uint32_t sira, const a2::Ortam& o);
void kodla_malzeme(Baytlar& out, std::uint32_t sira, const MalzemeIleti& m);
void kodla_durum(Baytlar& out, std::uint32_t sira, const DurumIleti& d);
void kodla_sahne_olayi(Baytlar& out, std::uint32_t sira, const a2::SahneOlayi& o);
void kodla_kamera_istegi(Baytlar& out, std::uint32_t sira, const KameraIstegiIleti& k);
void kodla_kamera_iptal(Baytlar& out, std::uint32_t sira, const a2::KameraIptal& k);
void kodla_sim_nabzi(Baytlar& out, std::uint32_t sira, const a2::SimNabzi& n);
void kodla_halka_yenile(Baytlar& out, std::uint32_t sira, const a2::HalkaBilgisi& h);
void kodla_govde_tablosu(Baytlar& out, std::uint32_t sira, const GovdeTablosuIleti& t);

// Dalga bileşen dizisinin özeti (§3.4: bilesen_sayisi × 40 bayt, tablodaki sırayla).
void dalga_ozeti_hesapla(DalgaIleti& d);

// ---------------------------------------------------------------------------
// Alan yardımcıları
// ---------------------------------------------------------------------------

// t_ns = adim_no · adim_ns (taşma güvenli).
bool zaman_adimla_tutarli(std::int64_t t_ns, std::uint64_t adim_no, std::int64_t adim_ns);

// A2-K27 / §3.4: bayrak bit 0, sonlu alanlar ve |‖q‖ − 1| ≤ 1e-6.
bool govde_durumu_gecerli(const a2::GovdeDurumu& g);

}  // namespace ida::viz

#endif
