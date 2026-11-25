// A2 sunucusunun (`sim/a2`, `ida_a2`) `ida_sim`'e bağlanması (W5-bagla-a2): dünyanın yetkili
// durumunun A2 veri kaynağı arayüzüne (`ida::a2::Yayinci`) çevrilmesi.
//
// Sözleşme: docs/sozlesmeler/A2-gorsellestirici.md (A2-K01, K02, K05, K20–K24, K47, K65, K79, K80,
// K93, K96, §3.4, §6). Mimar kararları: W5-K31 (A2 yalnız `--a2-port` ile açılır), W5-K32 (gövde
// kimliği bu katmanda, süreç boyunca tekdüze artan sayaçtan), W5-K33 (A3 `sifirla` A2'ye eşlenmez).
//
// KABULLER (yanlışı derleme hatası vermez; her biri testle çapraz kontrol edilir):
//   Çerçeve  : fizik dünyası doğrusal hızı GÖVDE çerçevesinde tutar (`DurumAnlik::dogrusal_hiz_b`:
//              (u, v, w), O_b, yere göre). A2 `v_n` ister: O_b'nin NED hızı.
//                  v_n = R(q_nb) · v_b
//              `q_nb` gövdeden dünyaya (NED) dönüş, Hamilton, sıra (w, x, y, z); fizikteki
//              `DurumAnlik::yonelim` ile aynı kabul (parametreler.hpp), dönüştürülmeden taşınır.
//              `w_b` gövdenin dünyaya göre açısal hızı, GÖVDE çerçevesinde: `acisal_hiz_b` aynen.
//              `p_n` O_b'nin NED konumu (senaryo orijinine göre): `konum_n` aynen.
//   Birim    : m, m/s, rad/s; sim zamanı adım numarasıyla (t = adim_no · 1,25 ms).
//   Adım     : `DurumAnlik::adim_no` tamamlanan adım sayısıdır; `adim_no = b` durumu fizik sınırı
//              b'nin (t = b·h) durumudur. Sınır b'nin durumu, o sınırla başlayan fizik adımından
//              ÖNCE yayınlanır (A2-K21: `T` durumu `T` adımının başındaki olaylardan sonraki durum).
//
// İş parçacığı: `calistirma_basladi`, `adim_siniri`, `duraklat`, `surdur` yalnız dünya iş
// parçacığından çağrılır; engellemez, kilit almaz, bellek ayırmaz, G/Ç yapmaz (A2-K47; Yayinci'nin
// güvencesi). Yaptıkları tek ek iş çağrı süresinin ölçümüdür (tekdüze saat, iki okuma).
#pragma once

#include "ida/a2/sunucu.hpp"
#include "ida/fizik/dunya.hpp"
#include "ida/senaryo/senaryo.hpp"
#include "ida/sunucu/sunucu.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace ida::sunucu {

namespace a2b {

// v_n = R(q_nb) · v_b: gövde çerçevesindeki vektörün NED bileşenleri. `q_nb` birim kuaterniyon
// (w, x, y, z), gövdeden NED'e (Hamilton). Normalleştirme yapılmaz (fizik her adımda yapar).
fizik::Vek3 govdeden_ned(const fizik::Kuaterniyon& q_nb, const fizik::Vek3& v_b) noexcept;

// Fizik dünyasının bir adım sınırındaki durumundan A2 gövde durumu (çerçeve kabulleri dosya başında).
a2::GovdeAni govde_ani(std::uint32_t kimlik, const fizik::DurumAnlik& d) noexcept;

// W5-K32: süreç boyunca tekdüze artan gövde kimliği (1'den; yeniden kullanılmaz: A2-K23, K92).
std::uint32_t yeni_govde_kimligi() noexcept;

// Bit k = tüp k+1 dolu (A2 §3.3 `dolu_tupler`).
std::uint8_t dolu_tup_maskesi(const std::array<bool, 2>& dolu) noexcept;

// Çalıştırma tanımının senaryodan gelen, çalıştırma boyunca değişmeyen kısmı.
struct Tanim {
    a2::Kademe kademe = a2::Kademe::sade;            // kalite.gorunum (sade | tam)
    std::string manifest_yolu;                       // mutlak yol, UTF-8 (A2 §3.3)
    std::array<std::uint8_t, 32> manifest_sha256{};  // manifestin ham baytlarının özeti (A7-K05)
    std::uint16_t arac_varlik = 0;                   // A7 varliklar[] içinde arac.varlik'in indeksi
    // ortam_turu, statik varlık ve pozu, ışık ve su optiği (A5 §3.9 → A2 §3.4). `ortam_surumu`
    // ve dolgu alanlarını kitaplık yönetir.
    a2::Ortam ortam{};
    std::vector<a2::MalzemeOptikGirdisi> malzeme;    // kalibrasyon.malzeme.<kimlik>.optik (A7-K30)
};

enum class TanimSonucu : std::uint8_t {
    tamam,
    gorunum_kapali,   // kalite.gorunum = "kapali": A2 dinleyicisi açılmaz (A2-K02)
    hata,             // tanım kurulamadı (`hata` dolu): A2 açılmaz, koşu sürer
};

// Doğrulanmış senaryodan çalıştırma tanımı. A7 manifesti senaryonun `varliklar.kume`sinden
// (`assets/<kume>/manifest.json`), senaryoyu çözen `kaynak` ile yeniden okunur; baytların SHA-256'sı
// doğrulama raporundaki kaynak özetine eşit olmalıdır (çözümleyicinin doğruladığı manifestle aynı
// dosya; A5-K115). Manifestin mutlak yolu `depo_koku`ndan türetilir.
TanimSonucu tanim_kur(const senaryo::DogrulanmisSenaryo& cozumlenmis, const senaryo::DogrulamaRaporu& rapor,
                      const senaryo::DosyaKaynagi& kaynak, const std::string& depo_koku, Tanim& tanim,
                      std::string& hata);

}  // namespace a2b

struct A2BaglantiAyari {
    std::uint16_t port = a2::kVarsayilanPort;
    bool sinama_bos_port = false;        // yalnız sınama
    std::string kilit_dosyasi;           // yalnız sınama (boş: sözleşmedeki sabit yer)
    std::size_t gonderme_kuyrugu_bayt = 0;   // yalnız sınama; 0: A2-K26 (8 MiB)
    std::function<void(std::string_view)> gunluk;
};

// Süreç başına bir tane; içindeki `a2::Sunucu` (dinleyici, sahiplik kilidi, G/Ç iş parçacığı)
// nesne yaşadıkça tutulur.
class A2Baglanti {
public:
    // Kilidi almayı ve dinlemeyi dener. nullptr: port aralık dışı (A2-K93; `hata` dolu, süreç
    // başlamaz). Kilit ya da port alınamazsa nesne yine döner ve A2'siz çalışır (A2-K05, K79).
    static std::unique_ptr<A2Baglanti> baslat(const A2BaglantiAyari& ayar, a2b::Tanim tanim, std::string& hata);
    ~A2Baglanti();
    A2Baglanti(const A2Baglanti&) = delete;
    A2Baglanti& operator=(const A2Baglanti&) = delete;

    bool dinliyor() const noexcept { return etkin_; }
    std::uint16_t port() const noexcept;
    const char* durum_adi() const noexcept;      // başlangıçtaki durum (dinliyor | kilit_baskasinda | …)
    std::string aciklama() const;
    a2::Kademe kademe() const noexcept { return tanim_.kademe; }

    // ---- Dünya iş parçacığı ----
    // Yeni çalıştırma (A2-K80 (1), K96): dünya kurulduktan sonra, ilk fizik adımından önce. `d`
    // başlangıç sınırının durumu; araç tek yaşayan gövdedir (fizik dünyası tek araçlıdır).
    // `dolu` fiziğin kurulumdaki kütle durumudur (baslangic.yuklu_tupler).
    bool calistirma_basladi(Kip kip, const fizik::OrtamParametreleri& ortam, const fizik::DurumAnlik& d,
                            const std::array<bool, 2>& dolu) noexcept;
    // Fizik sınırı `d.adim_no`: yalnız ilerleyen turda, sınır başına bir kez. Aynı ya da eski sınır
    // yeniden verilirse çağrı yapılmaz (tanımdaki başlangıç sınırı dahil).
    void adim_siniri(const fizik::DurumAnlik& d) noexcept;
    void duraklat() noexcept;   // duraklatma, son verilen sınırda başlar (A2-K24)
    void surdur() noexcept;

    // W5-a3-senaryo (W5-K49): aynı süreçte yeni bir dünya (`sifirla`, `senaryo/yukle`). Önceki çalıştırmanın
    // sayaçları ve durumu sıfırlanır; sunucu (kilit, dinleyici, bağlı görselleştirici) kalır. Bir sonraki
    // `calistirma_basladi` A2 çalıştırma nesli artırır ve bağlı oturumu veda(yeni_calistirma) ile kapatır.
    void yeni_tanim(a2b::Tanim tanim);

    // ---- Kapanış (dünya döngüsü bittikten ve A1 kapanışı tamamlandıktan sonra) ----
    // Çalıştırma tanımlandıysa `calistirma_durdu()` (bir kez); `son` ise ayrıca `kapaniyor()` (bağlı
    // oturuma veda(sim_kapaniyor); A2-K67) ve `durdur()`. Durum ve sayaçlar `ozet`e yazılır. Yinelenirse
    // ikinci çağrı yalnız özeti yeniden yazar. `son = false`: sunucu ayakta kalır (sonraki dünya için).
    void kapat(A2Ozeti& ozet, bool son = true) noexcept;

private:
    A2Baglanti() = default;

    std::unique_ptr<a2::Sunucu> sv_;
    a2b::Tanim tanim_;
    bool etkin_ = false;              // başlangıçta dinliyordu (A2'siz değil)
    a2::A2Durumu ilk_durum_ = a2::A2Durumu::ic_hata;
    std::string ilk_aciklama_;
    std::uint16_t ilk_port_ = 0;
    bool tanimli_ = false;            // yeni_calistirma kabul edildi
    bool durdu_ = false;              // calistirma_durdu() verildi (bu çalıştırma için)
    bool kapandi_ = false;
    std::uint32_t arac_kimligi_ = 0;
    std::uint64_t son_adim_ = 0;      // son yayınlanan adım sınırı
    std::uint64_t adim_siniri_ = 0;   // adim_siniri çağrısı
    std::uint64_t reddedilen_ = 0;    // Yayinci'nin uygulamadığı çağrı (0 beklenir)
    std::int64_t cagri_toplam_ns_ = 0;
    std::int64_t cagri_en_kotu_ns_ = 0;
};

}  // namespace ida::sunucu
