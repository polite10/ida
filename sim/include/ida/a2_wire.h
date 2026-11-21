// A2 — Sim sunucusu <-> görselleştirici: tel ve paylaşımlı bellek yapıları.
//
// Sözleşme: docs/sozlesmeler/A2-gorsellestirici.md (sürüm 1.0-aday).
// Bu dosya sözleşmenin parçasıdır; belgeyle çelişemez. Çelişki görülürse belge
// geçerlidir ve bu dosya düzeltilir.
//
// İçerik yalnız tip, sabit ve derleme zamanı denetimidir; işlev gövdesi yoktur.
// Bayt düzeni küçük sonlu (little-endian) ve doğal hizalıdır; her yapıda dolgu
// alanları açıkça yazılmıştır, derleyicinin kendiliğinden dolgu eklemediği
// static_assert ile denetlenir. Tarafların bu yapıları bellekten doğrudan
// kopyalaması zorunlu değildir; zorunlu olan, sözleşmedeki bayt düzenidir.
//
// Metin alanları: sabit uzunluklu, UTF-8, NUL ile biten; NUL'dan sonraki bütün
// baytlar sıfırdır.
//
// Paylaşımlı bellekte atomik alan yoktur. Bir yuvaya aynı anda yazan ve okuyan
// bulunmaması, sahipliği yalnız sim'in tuttuğu yuva protokolüyle sağlanır
// (sözleşme A2-K52–K61); bellek görünürlüğü varsayımı A2-K53'tedir.

#ifndef IDA_A2_WIRE_H
#define IDA_A2_WIRE_H

#include <bit>
#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace ida::a2 {

static_assert(std::endian::native == std::endian::little,
              "A2 yalnız küçük sonlu ana makinelerde tanımlıdır (macOS arm64, Windows x64)");

// ---------------------------------------------------------------------------
// Sürüm ve genel sabitler
// ---------------------------------------------------------------------------

inline constexpr std::uint16_t kSurumAna = 1;     // A2 ana sürüm (major)
inline constexpr std::uint16_t kSurumAlt = 0;     // A2 alt sürüm (minor)

inline constexpr std::uint32_t kSihirliSayi = 0x32414449u;   // "IDA2" (bayt sırası 49 44 41 32)
inline constexpr std::uint16_t kVarsayilanPort = 47210;      // mimari seçimi; IANA'da atanmamış aralık
inline constexpr std::uint32_t kBaslikBoyutu = 16;
inline constexpr std::uint32_t kAzamiYuk = 1u << 20;          // 1 MiB; mimari seçimi

inline constexpr std::uint32_t kAzamiGovde = 64;              // mimari seçimi
inline constexpr std::uint32_t kAzamiDalgaBileseni = 256;     // mimari seçimi
inline constexpr std::uint32_t kAzamiKamera = 1;              // iskelet §10 md.25: tek kamera
inline constexpr std::uint32_t kAzamiMalzemeOptik = 256;      // mimari seçimi

inline constexpr std::uint16_t kKameraAzamiGenislik = 640;    // iskelet §10 md.25
inline constexpr std::uint16_t kKameraAzamiYukseklik = 480;   // iskelet §10 md.25
inline constexpr std::uint16_t kKameraAsgariBoyut = 16;       // mimari seçimi
inline constexpr std::uint32_t kKameraAzamiHizHz = 30;        // iskelet §10 md.25
inline constexpr std::int64_t kKameraAzamiPozlamaNs = 1'000'000'000;  // A2 §3.6 (U13; A8 ile aynı)
inline constexpr double kKameraAzamiMontajKonumuM = 10.0;    // A2 §3.6 (U13); mimari seçimi (tahmin)
inline constexpr double kKameraKuaterniyonToleransi = 1e-5;   // A2 §3.6 (U13); binary64'te w,x,y,z sırasıyla

// S (sensör) çerçevesinden optik çerçeveye sabit dönüşüm (A2 §3.6, U13): v_S = R_SO · v_O.
// S: x ileri (optik eksen), y sağ, z aşağı. Optik: x sağ, y aşağı, z ileri. det = +1.
inline constexpr double kR_SO[3][3] = {{0.0, 0.0, 1.0}, {1.0, 0.0, 0.0}, {0.0, 1.0, 0.0}};

// Kamera kademe değerleri (sözleşme §6). Tam: üst sınır. Sade: tahmin; Windows ölçümüyle (MP8) sabitlenecek.
inline constexpr std::uint16_t kTamGenislik = 640;
inline constexpr std::uint16_t kTamYukseklik = 480;
inline constexpr std::uint32_t kTamHizHz = 30;
inline constexpr std::uint16_t kSadeGenislik = 320;
inline constexpr std::uint16_t kSadeYukseklik = 240;
inline constexpr std::uint32_t kSadeHizHz = 15;

inline constexpr std::uint32_t kYuvaAsgari = 4;               // mimari seçimi
inline constexpr std::uint32_t kYuvaVarsayilan = 8;           // mimari seçimi (tahmin)
inline constexpr std::uint32_t kYuvaAzami = 32;               // mimari seçimi

inline constexpr std::uint32_t kSayfa = 4096;
inline constexpr std::uint32_t kHalkaBaslikBolgesi = 4096;    // halka başlığının kapladığı bölge
inline constexpr std::uint32_t kYuvaBaslikBoyutu = 256;

inline constexpr std::uint64_t kHalkaSihirliSayi = 0x4B4C414832414449ull;  // baytlar 49 44 41 32 48 41 4C 4B ("IDA2HALK")

// Zaman sabitleri (ns). Mimari seçimleri; dayanakları sözleşmede.
inline constexpr std::int64_t kMerhabaZamanAsimiNs = 5'000'000'000;     // MERHABA bekleme
inline constexpr std::int64_t kHazirZamanAsimiNs = 120'000'000'000;     // HAZIR bekleme
inline constexpr std::int64_t kNabizAraligiNs = 1'000'000'000;          // nabız gönderimi
inline constexpr std::int64_t kNabizZamanAsimiNs = 3'000'000'000;       // karşı taraf ölü sayılır
inline constexpr std::int64_t kYenidenBaglanmaAraligiNs = 1'000'000'000;
inline constexpr std::int64_t kBirakmaZamanAsimiNs = 1'000'000'000;     // aşılırsa halka emekliye ayrılır
inline constexpr std::uint32_t kTuketiciBasinaAzamiEmekliHalka = 2;    // A2-K89; tahmin
inline constexpr std::int64_t kOperatorGecikmesiNs = 33'333'000;        // D_op varsayılanı (A2-K93: 33 333 µs)
inline constexpr std::int64_t kOperatorGecmisNs = 500'000'000;          // görselleştiricinin tuttuğu geçmiş
inline constexpr std::uint32_t kDurumHizPay = 60;                       // DURUM takvimi 60/1 Hz
inline constexpr std::uint32_t kDurumHizPayda = 1;
inline constexpr std::uint32_t kSilAzamiDurumDuvarSaniye = 120;         // SIL seyreltme sınırı
inline constexpr std::uint64_t kGondermeKuyruguAzamiBayt = 8ull << 20;  // 8 MiB

inline constexpr double kKuaterniyonNormToleransi = 1e-6;

// ---------------------------------------------------------------------------
// Sayılamalar (tel üzerindeki genişlikleri yapı alanlarında belirtilir)
// ---------------------------------------------------------------------------

enum class IletiTuru : std::uint16_t {
  // görselleştirici -> sim
  merhaba = 0x0001,
  hazir = 0x0003,
  kare_hazir = 0x0101,
  kare_dustu = 0x0102,
  goruntuleyici_nabzi = 0x0103,
  halka_eslendi = 0x0104,
  // sim -> görselleştirici
  hosgeldin = 0x0002,
  ret = 0x0004,
  dalga_kumesi = 0x0201,
  ortam = 0x0202,
  malzeme_optik = 0x0203,
  durum = 0x0204,
  sahne_olayi = 0x0205,
  kamera_istegi = 0x0206,
  kamera_iptal = 0x0207,
  sim_nabzi = 0x0208,
  halka_yenile = 0x0209,             // yük: HalkaBilgisi
  govde_tablosu = 0x020A,            // yük: GovdeTablosu + govde_sayisi × GovdeBilgisi (A2-K90)
  // iki yön
  veda = 0x00FF,
};

enum class Platform : std::uint8_t { macos_arm64 = 1, windows_x64 = 2 };
enum class Cizici : std::uint8_t { forward_plus = 1 };
enum class SimKipi : std::uint8_t { sil = 1, hil_1x = 2, hil_bariyer = 3, tekrar_izleme = 4 };
enum class Kademe : std::uint8_t { kapali = 0, sade = 1, tam = 2 };
enum class CalismaDurumu : std::uint8_t { calisiyor = 1, duraklatildi = 2, durdu = 3 };
enum class GovdeTuru : std::uint8_t { arac = 1, torpido = 2, hedef = 3 };
enum class OrtamTuru : std::uint8_t { havuz = 1, acik_su = 2 };
enum class PikselBicimi : std::uint8_t { rgb8_srgb = 1 };
enum class CizimYolu : std::uint8_t { elle_force_draw = 1 };
enum class BozulmaModeli : std::uint32_t { yok = 0, brown_conrady_5 = 1 };

enum class SahneOlayiTuru : std::uint16_t {
  govde_dogdu = 1,
  govde_oldu = 2,
  kutle_durumu = 3,
  isabet = 4,
  sifirlama = 5,
  duraklat = 6,
  devam = 7,
  atlama = 8,
};

enum class OlumNedeni : std::uint8_t { yok = 0, isabet = 1, alan_disi = 2, sure = 3, sifirlama = 4 };

enum class RetNedeni : std::uint32_t {
  surum = 1,
  manifest = 2,
  zaman_asimi = 3,
  mesgul = 4,
  protokol = 5,
  halka = 6,
};

enum class VedaNedeni : std::uint32_t {
  olagan = 0,
  protokol_hatasi = 1,
  tampon_tasti = 2,
  nabiz_zaman_asimi = 3,
  sim_kapaniyor = 4,
  yeni_calistirma = 5,               // A2-K96
};

enum class KareDusmeNedeni : std::uint8_t {
  daha_yeni_istek = 1,
  iptal = 2,
  geri_okuma_hatasi = 3,
  sahne_hazir_degil = 4,
  gecersiz_istek = 5,
  tutarsiz_surum = 6,
  halka_hatasi = 7,
};

// Sim'in A3 ve A6'ya bildirdiği kare düşme nedenleri (tel iletisi değil; sayaç ve kayıt kodu).
// Görselleştiricinin bildirdiği KareDusmeNedeni değerleri 32 + kod olarak bu kod uzayına eşlenir.
enum class SimKareDusmeNedeni : std::uint8_t {
  halka_dolu = 1,
  kamera_gec = 2,
  gorsellestirici_yok = 3,
  calisma_nesli_degisti = 4,
  halka_yenilendi = 5,               // emeklilik anında yanıtlanmamış istek
  halka_yenileniyor = 6,             // yeni halka henüz eşlenmedi
  halka_hatasi = 7,
  halka_emekli_hazir = 8,            // emeklilik anında hazır (S_d bekleyen) kare (A2-K86)
  gorsellestirici_nedeni_tabani = 32,
};

enum class KameraIptalNedeni : std::uint8_t {
  gec = 1,
  calisma_nesli_degisti = 2,          // sıfırlama ya da atlama
  oturum_sonu = 3,
  halka_yenilendi = 4,
};

// HAZIR.durum kodları (0 = başarılı). A7 hata kodlarıyla eşlenir; sözleşme §5.
enum class HazirDurumu : std::uint32_t {
  basarili = 0,
  manifest_okunamadi = 1,
  manifest_gecersiz = 2,
  varlik_eksik = 3,
  varlik_ozeti_uyusmaz = 4,
  varlik_kurali = 5,
  halka_eslenemedi = 6,
  halka_duzeni_uyusmaz = 7,
};

// Yetenek bitleri (MERHABA.yetenekler)
inline constexpr std::uint32_t kYetenekKamera = 1u << 0;   // force_draw + eşzamanlı geri okuma yolu var

// DURUM.bayraklar
inline constexpr std::uint8_t kDurumTakvimDisi = 1u << 0;  // süreklilik sınırı için takvim dışı gönderildi

// Gövde durumu bayrakları
inline constexpr std::uint16_t kGovdeGecerli = 1u << 0;

// Kare (yuva başlığı) bayrakları
inline constexpr std::uint32_t kKareBozulmaUygulandi = 1u << 0;
inline constexpr std::uint32_t kKareGurultuUygulandi = 1u << 1;
inline constexpr std::uint32_t kKareDalgaGorseli = 1u << 2;     // dalga yüzeyi çizildi (sade kademede 0)
inline constexpr std::uint32_t kKareSuAltiOptigi = 1u << 3;
inline constexpr std::uint32_t kKareKameraSuAltinda = 1u << 4;  // kamera merkezi yerel yüzeyin altında
inline constexpr std::uint32_t kKareYuzeyYakin = 1u << 5;       // |kamera merkezi - yerel yüzey| < 0,1 m
inline constexpr std::uint32_t kKarePencereCizilebilir = 1u << 6;
inline constexpr std::uint32_t kKareTutarsiz = 1u << 7;         // sürüm uyuşmazlığı; kare geçersiz

// ---------------------------------------------------------------------------
// İleti başlığı (16 B)
// ---------------------------------------------------------------------------

struct Baslik {
  std::uint32_t sihirli;     // kSihirliSayi
  std::uint16_t tur;         // IletiTuru
  std::uint16_t bayraklar;   // 0
  std::uint32_t yuk_bayt;    // başlıktan sonraki bayt sayısı
  std::uint32_t sira;        // yön başına 0'dan başlar, her iletide +1
};

// ---------------------------------------------------------------------------
// Görselleştirici -> sim
// ---------------------------------------------------------------------------

struct Merhaba {                     // 144 B
  std::uint16_t surum_ana;
  std::uint16_t surum_alt;
  std::uint32_t yetenekler;
  std::uint8_t platform;             // Platform
  std::uint8_t cizici;               // Cizici
  std::uint8_t pencere_cizilebilir;  // 0/1
  std::uint8_t dolgu0;
  std::uint32_t surec_kimligi;
  char godot_surumu[32];
  char uzanti_surumu[32];
  char gpu_adi[64];
};

struct Hazir {                       // 48 B
  std::uint8_t manifest_sha256[32];  // görselleştiricinin hesapladığı
  std::uint32_t dalga_kumesi_surumu;
  std::uint32_t ortam_surumu;
  std::uint32_t halka_eslendi_maskesi;  // bit k: kamera k'nın halkası eşlendi
  std::uint32_t durum;                  // HazirDurumu
};

struct KareHazir {                   // 40 B
  std::uint64_t istek_kimligi;
  std::uint32_t halka_nesli;
  std::uint16_t yuva;
  std::uint8_t kamera_kimligi;
  std::uint8_t dolgu0;
  std::uint32_t kare_sirasi;
  std::uint32_t yazim_sayaci;        // yuva başlığına yazılan değer (tanılama)
  std::int64_t hazir_duvar_ns;
  std::uint32_t calisma_nesli;       // istekteki değer
  std::uint32_t dolgu1;
};

struct KareDustu {                   // 24 B
  std::uint64_t istek_kimligi;
  std::uint32_t halka_nesli;
  std::uint16_t yuva;
  std::uint8_t kamera_kimligi;
  std::uint8_t neden;                // KareDusmeNedeni
  std::uint32_t kare_sirasi;
  std::uint32_t calisma_nesli;       // istekteki değer
};

struct HalkaEslendi {                // 16 B; halka_yenile yanıtı
  std::uint8_t kamera_kimligi;
  std::uint8_t dolgu0[3];
  std::uint32_t halka_nesli;
  std::uint32_t durum;               // HazirDurumu: 0, 6 ya da 7
  std::uint32_t dolgu1;
};

struct GoruntuleyiciNabzi {          // 48 B
  std::int64_t gonderim_duvar_ns;
  std::int64_t son_uygulanan_t_ns;   // operatör görünümünün son çizdiği sim zamanı
  std::uint32_t cizim_hizi_mhz;      // saniyedeki çizim × 1000
  std::uint32_t elle_cizim_sayaci;
  std::uint32_t operator_bekleme_sayaci;
  std::uint32_t uretilen_kare;
  std::uint32_t dusen_kare;
  std::uint8_t pencere_cizilebilir;
  std::uint8_t dolgu0[3];
  std::uint64_t gpu_bellek_bayt;     // bilinmiyorsa 0
};

// ---------------------------------------------------------------------------
// Sim -> görselleştirici
// ---------------------------------------------------------------------------

struct GovdeBilgisi {                // 16 B (HOSGELDIN içinde)
  std::uint32_t govde_kimligi;
  std::uint32_t sureklilik_no;
  std::uint16_t varlik_indeksi;      // A7 manifest varliklar[] indeksi
  std::uint8_t govde_turu;           // GovdeTuru
  std::uint8_t dolu_tupler;          // bit k: tüp k+1 dolu (yalnız araç)
  std::uint32_t dolgu0;
};

struct KameraYapilandirmasi {        // 184 B
  std::uint8_t kamera_kimligi;
  std::uint8_t piksel_bicimi;        // PikselBicimi
  std::uint16_t genislik;
  std::uint16_t yukseklik;
  std::uint16_t dolgu0;
  std::uint32_t hiz_pay;             // hız = pay / payda Hz
  std::uint32_t hiz_payda;
  std::uint32_t yapilandirma_surumu;
  std::uint16_t kalibrasyon_surumu;  // 1…65535 (A8 calib_ver ile aynı genişlik; U13)
  std::uint16_t ayrilmis_kal;        // 0
  double fx, fy, cx, cy;             // piksel; OpenCV kabulü (sözleşme §3.6)
  std::uint32_t bozulma_modeli;      // BozulmaModeli
  std::uint32_t montaj_govde_kimligi;
  double bozulma[5];                 // k1 k2 p1 p2 k3
  double montaj_l_b[3];              // m, gövde çerçevesi, O_b'ye göre
  double montaj_q_bs[4];             // S çerçevesinden gövdeye dönüş (w x y z); fizik gerçeği montajı
  std::int64_t pozlama_ns;           // nominal pozlama süresi; > 0, çift, ≤ kKameraAzamiPozlamaNs
  double yakin_m, uzak_m;            // kırpma uzaklıkları
};

struct HalkaBilgisi {                // 96 B
  std::uint8_t kamera_kimligi;
  std::uint8_t dolgu0[3];
  std::uint32_t halka_nesli;
  std::uint32_t yuva_sayisi;
  std::uint32_t dolgu1;
  std::uint64_t yuva_boyutu;
  std::uint64_t toplam_boyut;
  char ad[64];                       // işletim sistemi nesne adı (sözleşme §4.8)
};

struct Hosgeldin {                   // 600 B; ardından: govde_sayisi × GovdeBilgisi,
                                     // kamera_sayisi × KameraYapilandirmasi, kamera_sayisi × HalkaBilgisi
  std::uint64_t oturum_kimligi;
  std::uint16_t surum_ana;
  std::uint16_t surum_alt;
  std::uint8_t sim_kipi;             // SimKipi
  std::uint8_t kademe_3b;            // Kademe (kapali gönderilmez)
  std::uint8_t kademe_kamera;        // Kademe
  std::uint8_t kamera_sayisi;
  std::int64_t t_ns;
  std::uint64_t adim_no;
  std::int64_t adim_ns;              // 1 250 000 (iskelet §4)
  std::uint32_t durum_hiz_pay;
  std::uint32_t durum_hiz_payda;
  std::uint8_t manifest_sha256[32];
  char manifest_yolu[512];           // mutlak yol, UTF-8
  std::uint16_t govde_sayisi;
  std::uint16_t dolgu0;
  std::uint32_t calisma_nesli;       // o anki çalıştırma nesli (A2-K80)
};

struct GovdeTablosu {                // 24 B; ardından govde_sayisi × GovdeBilgisi (A2-K90)
  std::int64_t t_ns;                 // atlamadan sonraki yeni sim zamanı
  std::uint64_t adim_no;
  std::uint32_t calisma_nesli;       // atlama olayındaki yeni nesil
  std::uint16_t govde_sayisi;        // yeni zamanda yaşayan bütün gövdeler
  std::uint16_t dolgu0;
};

struct Ret {                         // 128 B
  std::uint32_t neden;               // RetNedeni
  char ayrinti[124];
};

struct Veda {                        // 8 B
  std::uint32_t neden;               // VedaNedeni
  std::uint32_t dolgu0;
};

struct DalgaBileseni {               // 40 B
  double a;                          // m, genlik
  double omega;                      // rad/s
  double k;                          // rad/m
  double beta;                       // rad, kuzeyden doğuya yayılma yönü
  double phi;                        // rad
};

struct DalgaKumesi {                 // 72 B; ardından bilesen_sayisi × DalgaBileseni
  std::uint32_t surum;
  std::uint16_t bilesen_sayisi;
  std::uint8_t derin_su;             // 1: derinlik sonsuz kabul edilir, derinlik_m yok sayılır
  std::uint8_t dolgu0;
  std::uint64_t tohum;
  double g;                          // m/s^2
  double rho;                        // kg/m^3
  double derinlik_m;                 // h
  std::uint8_t ozet_sha256[32];      // bileşen dizisinin baytları üzerinden
};

struct Ortam {                       // 208 B
  std::uint32_t ortam_surumu;
  std::uint8_t ortam_turu;           // OrtamTuru
  std::uint8_t dolgu0;
  std::uint16_t statik_varlik_indeksi;   // 0xFFFF: yok
  double statik_p_n[3];              // m, NED
  double statik_q_nb[4];             // w x y z
  double gunes_yonu_n[3];            // birim vektör, sahneden güneşe, NED
  double gunes_isinimi_w_m2;
  double ortam_isigi_rgb_dogrusal[3];
  double beta_d_m[3];                // 1/m, kanal başına zayıflama (R, G, B)
  double beta_b_m[3];                // 1/m, geri saçılım
  double b_sonsuz_rgb_dogrusal[3];   // 0..1
  double bulaniklik_olcegi;          // >= 0
  std::uint32_t su_optigi_surumu;
  std::uint32_t dolgu1;
};

struct MalzemeOptikGirdisi {         // 56 B
  std::uint16_t malzeme_indeksi;     // A7 manifest malzemeler[] indeksi
  std::uint16_t maske;               // bit0 taban rengi, bit1 metaliklik, bit2 pürüzlülük
  std::uint32_t dolgu0;
  double taban_rengi_dogrusal[4];
  double metaliklik;
  double puruzluluk;
};

struct MalzemeOptik {                // 8 B; ardından girdi_sayisi × MalzemeOptikGirdisi
  std::uint32_t surum;
  std::uint16_t girdi_sayisi;
  std::uint16_t dolgu0;
};

struct GovdeDurumu {                 // 120 B
  std::uint32_t govde_kimligi;
  std::uint32_t sureklilik_no;
  std::uint8_t govde_turu;           // GovdeTuru
  std::uint8_t dolu_tupler;
  std::uint16_t bayraklar;           // kGovdeGecerli
  std::uint32_t dolgu0;
  double p_n[3];                     // m, O_b'nin NED konumu
  double q_nb[4];                    // w x y z; gövdeden dünyaya
  double v_n[3];                     // m/s, O_b'nin NED hızı
  double w_b[3];                     // rad/s, gövde çerçevesinde açısal hız
};

struct Durum {                       // 32 B; ardından govde_sayisi × GovdeDurumu
  std::int64_t t_ns;
  std::uint64_t adim_no;
  std::int64_t gonderim_duvar_ns;
  std::uint32_t dalga_kumesi_surumu;
  std::uint16_t govde_sayisi;
  std::uint8_t calisma_durumu;       // CalismaDurumu
  std::uint8_t bayraklar;            // kDurumTakvimDisi
};

struct SahneOlayi {                  // 120 B
  std::int64_t t_ns;
  std::uint64_t adim_no;
  std::uint16_t tur;                 // SahneOlayiTuru
  std::uint8_t neden;                // OlumNedeni (govde_oldu)
  std::uint8_t dolgu0;
  std::uint32_t govde_kimligi;
  std::uint32_t diger_govde_kimligi; // isabet: hedef
  std::uint32_t sureklilik_no;       // olaydan sonraki süreklilik numarası
  std::uint16_t varlik_indeksi;      // govde_dogdu
  std::uint8_t govde_turu;           // govde_dogdu
  std::uint8_t eski_tupler;          // kutle_durumu
  std::uint8_t yeni_tupler;          // kutle_durumu
  std::uint8_t kaynak_tup;           // govde_dogdu: 1.., 0 yok
  std::uint16_t dolgu1;
  double nokta_n[3];                 // isabet
  double normal_n[3];                // isabet
  double bagil_hiz_n[3];             // isabet
  std::uint32_t calisma_nesli;       // olaydan sonra geçerli çalıştırma nesli
  std::uint32_t dolgu2;
};

struct KameraIstegi {                // 96 B; ardından govde_sayisi × GovdeDurumu (t_cizim anı)
  std::uint64_t istek_kimligi;
  std::uint32_t kare_sirasi;
  std::uint8_t kamera_kimligi;
  std::uint8_t bayraklar;            // 0
  std::uint16_t yuva;
  std::uint32_t halka_nesli;
  std::uint32_t yapilandirma_surumu;
  std::int64_t t_cizim_ns;           // pozlama ortası; fizik adımı sınırı
  std::int64_t t_olcum_ns;           // pozlama sonu = t_cizim + pozlama/2
  std::int64_t pozlama_ns;           // > 0, çift, ≤ kKameraAzamiPozlamaNs
  std::int64_t t_teslim_ns;          // modellenmiş alım anı (05)
  std::uint64_t adim_no;             // t_cizim anının adım numarası
  std::uint64_t gurultu_tohumu;
  std::uint32_t dalga_kumesi_surumu;
  std::uint32_t ortam_surumu;
  std::uint32_t malzeme_optik_surumu;
  std::uint16_t govde_sayisi;
  std::uint16_t dolgu0;
  std::uint32_t calisma_nesli;       // A2-K80
  std::uint32_t dolgu1;
};

struct KameraIptal {                 // 16 B
  std::uint64_t istek_kimligi;
  std::uint8_t kamera_kimligi;
  std::uint8_t neden;                // KameraIptalNedeni
  std::uint8_t dolgu0[6];
};

struct SimNabzi {                    // 32 B
  std::int64_t t_ns;
  std::uint64_t adim_no;
  std::int64_t gonderim_duvar_ns;
  std::uint8_t calisma_durumu;
  std::uint8_t sim_kipi;
  std::uint8_t dolgu0[6];
};

// ---------------------------------------------------------------------------
// Kamera kare halkası (paylaşımlı bellek)
// ---------------------------------------------------------------------------

struct HalkaBasligi {                // 128 B kullanılır; bölge kHalkaBaslikBolgesi
  std::uint64_t sihirli;             // kHalkaSihirliSayi
  std::uint16_t duzen_ana;           // = kSurumAna
  std::uint16_t duzen_alt;           // = kSurumAlt
  std::uint32_t baslik_bolgesi;      // = kHalkaBaslikBolgesi
  std::uint64_t oturum_kimligi;
  std::uint32_t halka_nesli;
  std::uint32_t yuva_sayisi;
  std::uint64_t yuva_boyutu;         // kSayfa katı
  std::uint64_t yuvalar_ofseti;      // = kHalkaBaslikBolgesi
  std::uint64_t toplam_boyut;
  std::uint16_t genislik;
  std::uint16_t yukseklik;
  std::uint32_t satir_adimi;         // = genislik × 3
  std::uint8_t kamera_kimligi;
  std::uint8_t piksel_bicimi;
  std::uint16_t dolgu0;
  std::uint32_t yuva_baslik_boyutu;  // = kYuvaBaslikBoyutu
  std::uint8_t ayrilmis[56];
};

struct YuvaBasligi {                 // 256 B; pikseller yuva başından kYuvaBaslikBoyutu ofsetinde
  std::uint32_t yazim_sayaci;        // yalnız tanılama: yuvaya yapılan yazım sayısı, 0: hiç yazılmadı (A2-K53)
  std::uint32_t calisma_nesli;       // istekteki değer
  std::uint64_t istek_kimligi;
  std::uint32_t halka_nesli;
  std::uint32_t kare_sirasi;
  std::uint8_t kamera_kimligi;
  std::uint8_t piksel_bicimi;        // PikselBicimi
  std::uint8_t kademe;               // Kademe
  std::uint8_t cizim_yolu;           // CizimYolu
  std::uint16_t genislik;
  std::uint16_t yukseklik;
  std::uint32_t satir_adimi;
  std::uint32_t bayraklar;           // kKare*
  std::uint32_t yapilandirma_surumu;
  std::uint16_t kalibrasyon_surumu;  // 1…65535
  std::uint16_t ayrilmis_kal;        // 0
  std::uint32_t dalga_kumesi_surumu;
  std::uint32_t ortam_surumu;
  std::int64_t t_cizim_ns;
  std::int64_t t_olcum_ns;
  std::int64_t pozlama_ns;
  std::int64_t t_teslim_ns;
  std::int64_t istek_alindi_duvar_ns;
  std::int64_t cizim_basladi_duvar_ns;
  std::int64_t hazir_duvar_ns;
  std::uint64_t piksel_bayt;         // = satir_adimi × yukseklik
  std::uint8_t ayrilmis[136];        // sıfır
};

// ---------------------------------------------------------------------------
// Derleme zamanı denetimleri
// ---------------------------------------------------------------------------

#define IDA_A2_DUZ(T, N)                                                    \
  static_assert(sizeof(T) == (N), #T " boyutu sözleşmeyle uyuşmuyor");      \
  static_assert(std::is_trivially_copyable_v<T>, #T " düz kopyalanabilir değil"); \
  static_assert(std::is_standard_layout_v<T>, #T " standart düzende değil")

IDA_A2_DUZ(Baslik, 16);
IDA_A2_DUZ(Merhaba, 144);
IDA_A2_DUZ(Hazir, 48);
IDA_A2_DUZ(KareHazir, 40);
IDA_A2_DUZ(KareDustu, 24);
IDA_A2_DUZ(HalkaEslendi, 16);
IDA_A2_DUZ(GoruntuleyiciNabzi, 48);
IDA_A2_DUZ(GovdeBilgisi, 16);
IDA_A2_DUZ(KameraYapilandirmasi, 184);
IDA_A2_DUZ(HalkaBilgisi, 96);
IDA_A2_DUZ(Hosgeldin, 600);
IDA_A2_DUZ(GovdeTablosu, 24);
IDA_A2_DUZ(Ret, 128);
IDA_A2_DUZ(Veda, 8);
IDA_A2_DUZ(DalgaBileseni, 40);
IDA_A2_DUZ(DalgaKumesi, 72);
IDA_A2_DUZ(Ortam, 208);
IDA_A2_DUZ(MalzemeOptikGirdisi, 56);
IDA_A2_DUZ(MalzemeOptik, 8);
IDA_A2_DUZ(GovdeDurumu, 120);
IDA_A2_DUZ(Durum, 32);
IDA_A2_DUZ(SahneOlayi, 120);
IDA_A2_DUZ(KameraIstegi, 96);
IDA_A2_DUZ(KameraIptal, 16);
IDA_A2_DUZ(SimNabzi, 32);
IDA_A2_DUZ(HalkaBasligi, 128);
IDA_A2_DUZ(YuvaBasligi, 256);

#undef IDA_A2_DUZ

// Seçilmiş alan ofsetleri (sözleşmedeki tablolarla birebir)
static_assert(offsetof(Baslik, tur) == 4);
static_assert(offsetof(Baslik, yuk_bayt) == 8);
static_assert(offsetof(Baslik, sira) == 12);
static_assert(offsetof(Merhaba, surec_kimligi) == 12);
static_assert(offsetof(Merhaba, godot_surumu) == 16);
static_assert(offsetof(Merhaba, gpu_adi) == 80);
static_assert(offsetof(Hosgeldin, t_ns) == 16);
static_assert(offsetof(Hosgeldin, manifest_sha256) == 48);
static_assert(offsetof(Hosgeldin, manifest_yolu) == 80);
static_assert(offsetof(Hosgeldin, govde_sayisi) == 592);
static_assert(offsetof(Hosgeldin, calisma_nesli) == 596);
static_assert(offsetof(GovdeTablosu, calisma_nesli) == 16);
static_assert(offsetof(GovdeTablosu, govde_sayisi) == 20);
static_assert(offsetof(KareHazir, yazim_sayaci) == 20);
static_assert(offsetof(KareHazir, hazir_duvar_ns) == 24);
static_assert(offsetof(KareHazir, calisma_nesli) == 32);
static_assert(offsetof(KareDustu, calisma_nesli) == 20);
static_assert(offsetof(KameraYapilandirmasi, kalibrasyon_surumu) == 20);
static_assert(offsetof(KameraYapilandirmasi, ayrilmis_kal) == 22);
static_assert(offsetof(KameraYapilandirmasi, fx) == 24);
static_assert(offsetof(KameraYapilandirmasi, bozulma) == 64);
static_assert(offsetof(KameraYapilandirmasi, montaj_l_b) == 104);
static_assert(offsetof(KameraYapilandirmasi, montaj_q_bs) == 128);
static_assert(offsetof(KameraYapilandirmasi, pozlama_ns) == 160);
static_assert(offsetof(KameraYapilandirmasi, uzak_m) == 176);
static_assert(offsetof(HalkaBilgisi, ad) == 32);
static_assert(offsetof(DalgaKumesi, g) == 16);
static_assert(offsetof(DalgaKumesi, ozet_sha256) == 40);
static_assert(offsetof(Ortam, statik_p_n) == 8);
static_assert(offsetof(Ortam, gunes_yonu_n) == 64);
static_assert(offsetof(Ortam, beta_d_m) == 120);
static_assert(offsetof(Ortam, su_optigi_surumu) == 200);
static_assert(offsetof(GovdeDurumu, p_n) == 16);
static_assert(offsetof(GovdeDurumu, q_nb) == 40);
static_assert(offsetof(GovdeDurumu, v_n) == 72);
static_assert(offsetof(GovdeDurumu, w_b) == 96);
static_assert(offsetof(SahneOlayi, govde_kimligi) == 20);
static_assert(offsetof(SahneOlayi, nokta_n) == 40);
static_assert(offsetof(SahneOlayi, calisma_nesli) == 112);
static_assert(offsetof(KameraIstegi, t_cizim_ns) == 24);
static_assert(offsetof(KameraIstegi, t_teslim_ns) == 48);
static_assert(offsetof(KameraIstegi, dalga_kumesi_surumu) == 72);
static_assert(offsetof(KameraIstegi, govde_sayisi) == 84);
static_assert(offsetof(KameraIstegi, calisma_nesli) == 88);
static_assert(offsetof(HalkaBasligi, yuva_boyutu) == 32);
static_assert(offsetof(HalkaBasligi, satir_adimi) == 60);
static_assert(offsetof(HalkaBasligi, yuva_baslik_boyutu) == 68);
static_assert(offsetof(YuvaBasligi, yazim_sayaci) == 0);
static_assert(offsetof(YuvaBasligi, calisma_nesli) == 4);
static_assert(offsetof(YuvaBasligi, istek_kimligi) == 8);
static_assert(offsetof(YuvaBasligi, satir_adimi) == 32);
static_assert(offsetof(YuvaBasligi, bayraklar) == 36);
static_assert(offsetof(YuvaBasligi, kalibrasyon_surumu) == 44);
static_assert(offsetof(YuvaBasligi, ayrilmis_kal) == 46);
static_assert(offsetof(YuvaBasligi, dalga_kumesi_surumu) == 48);
static_assert(offsetof(YuvaBasligi, t_cizim_ns) == 56);
static_assert(offsetof(YuvaBasligi, t_teslim_ns) == 80);
static_assert(offsetof(YuvaBasligi, hazir_duvar_ns) == 104);
static_assert(offsetof(YuvaBasligi, piksel_bayt) == 112);

static_assert(kHalkaBaslikBolgesi % kSayfa == 0);
static_assert(sizeof(HalkaBasligi) <= kHalkaBaslikBolgesi);

// Sabit boyutlu yüklerin boyutları (değişken uzunluklu iletilerde sabit kısım)
inline constexpr std::uint32_t kAzamiDurumYuku =
    sizeof(Durum) + kAzamiGovde * sizeof(GovdeDurumu);
inline constexpr std::uint32_t kAzamiKameraIstegiYuku =
    sizeof(KameraIstegi) + kAzamiGovde * sizeof(GovdeDurumu);
inline constexpr std::uint32_t kAzamiHosgeldinYuku =
    sizeof(Hosgeldin) + kAzamiGovde * sizeof(GovdeBilgisi) +
    kAzamiKamera * (sizeof(KameraYapilandirmasi) + sizeof(HalkaBilgisi));
inline constexpr std::uint32_t kAzamiDalgaYuku =
    sizeof(DalgaKumesi) + kAzamiDalgaBileseni * sizeof(DalgaBileseni);
inline constexpr std::uint32_t kAzamiMalzemeOptikYuku =
    sizeof(MalzemeOptik) + kAzamiMalzemeOptik * sizeof(MalzemeOptikGirdisi);
inline constexpr std::uint32_t kAzamiGovdeTablosuYuku =
    sizeof(GovdeTablosu) + kAzamiGovde * sizeof(GovdeBilgisi);
static_assert(kAzamiGovdeTablosuYuku <= kAzamiYuk);
static_assert(kAzamiDurumYuku <= kAzamiYuk);
static_assert(kAzamiKameraIstegiYuku <= kAzamiYuk);
static_assert(kAzamiHosgeldinYuku <= kAzamiYuk);
static_assert(kAzamiDalgaYuku <= kAzamiYuk);
static_assert(kAzamiMalzemeOptikYuku <= kAzamiYuk);

}  // namespace ida::a2

#endif  // IDA_A2_WIRE_H
