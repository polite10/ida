// ida/a2/yayinci.hpp — veri kaynağı tarafı: dünya iş parçacığının A2'ye veri verdiği tek yol.
//
// Sözleşme: docs/sozlesmeler/A2-gorsellestirici.md §4.3 (A2-K20…K29), §4.7 (K65), §4.9 (K80, K96).
//
// Bu sınıfın işlevleri yalnız tek bir iş parçacığından (dünya) çağrılır. Hiçbiri engellemez, kilit
// almaz, bellek ayırmaz ve sistem çağrısı yapmaz (A2-K17, K26, K47): veri sabit boylu yapılara
// yazılır; görselleştiricinin var olup olmaması, yavaşlığı ya da donması bu işlevlerin yaptığı işi
// değiştirmez. Yer kalmazsa kayıt atılır ve sayılır.
//
// KABULLER (çağıranın uyması gerekenler; hiçbiri derleme hatası vermez):
//   Birim     : konum m, hız m/s, açısal hız rad/s; sim zamanı adım numarasıyla verilir
//               (t_ns = adim_no · 1 250 000 ns; iskelet §4).
//   Çerçeve   : p_n ve v_n NED (senaryo orijinine göre; v_n DÜNYA çerçevesindedir, gövde
//               çerçevesindeki hız verilmez); w_b gövdenin dünyaya göre açısal hızı, GÖVDE
//               çerçevesinde; q_nb gövdeden dünyaya dönüş, Hamilton, sıra (w, x, y, z).
//   Adım sırası (A2-K21): `T` anındaki olay, `T` ile başlayan fizik adımının başında uygulanmıştır.
//               Dünya, `T` sınırının olaylarını (govde_dogdu, govde_oldu, kutle_durumu,
//               sureklilik_kir, isabet) `adim_siniri(T, …)` çağrısından ÖNCE verir; `adim_siniri`
//               olaydan SONRAKİ durumu taşır. `adim_siniri` her fizik adımı sınırında bir kez
//               çağrılır (60 Hz takvimi ve takvim dışı iletileri bu sınıf seçer).
//   Kimlik    : gövde kimliği bir A2 oturumu boyunca yeniden kullanılmaz (A2-K23); sıfırlamadan
//               sonra doğan gövdeler YENİ kimlik alır. Bu sınıf kimlikleri çevirmez.
//   Süreklilik: süreklilik numarasını bu sınıf tutar (doğumda 0; bir adımda kaç neden olursa olsun
//               tam 1 artar: A2-K21). Dünya yalnız nedeni bildirir (kutle_durumu, sureklilik_kir).
#pragma once

#include "ida/a2_wire.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <string_view>

namespace ida::a2 {

inline constexpr std::int64_t kAdimNs = 1'250'000;  // fizik adımı (iskelet §4)

// Bir gövdenin kimliği ve değişmeyen/seyrek değişen özellikleri (doğumda ve çalıştırma tanımında).
struct GovdeKaydi {
    std::uint32_t kimlik = 0;
    std::uint16_t varlik_indeksi = 0;   // A7 manifestindeki varliklar[] indeksi
    GovdeTuru tur = GovdeTuru::arac;
    std::uint8_t dolu_tupler = 0;       // bit k = tüp k+1 dolu; araç dışındaki gövdelerde 0
};

// Bir gövdenin adım sınırındaki kinematik durumu (birim ve çerçeveler dosya başında).
struct GovdeAni {
    std::uint32_t kimlik = 0;
    bool gecerli = true;                // false: görselleştirici gövdeyi son geçerli pozunda çizer (A2-K27)
    std::array<double, 3> p_n{};        // m, O_b'nin NED konumu
    std::array<double, 4> q_nb{1.0, 0.0, 0.0, 0.0};  // w x y z; gövdeden dünyaya
    std::array<double, 3> v_n{};        // m/s, O_b'nin NED hızı
    std::array<double, 3> w_b{};        // rad/s, gövde çerçevesinde
};

// Dalga kümesi (§3.4). Sürüm numarasını ve özetini bu kitaplık verir.
struct DalgaGirdisi {
    bool derin_su = true;               // true: derinlik_m yok sayılır (tel üzerinde geçersizse 1.0 yazılır)
    std::uint64_t tohum = 0;
    double g = 9.80665;                 // m/s²
    double rho = 1025.0;                // kg/m³
    double derinlik_m = 0.0;            // m; yalnız derin_su = false iken (> 0)
    std::span<const DalgaBileseni> bilesenler;  // ≤ 256
};

// Bir çalıştırmanın tam başlangıç durumu (A2-K65, K96).
struct CalistirmaTanimi {
    SimKipi sim_kipi = SimKipi::sil;
    Kademe kademe_3b = Kademe::tam;     // sade ya da tam (kapalıda A2 açılmaz: A2-K02)
    std::string_view manifest_yolu;     // mutlak yol, UTF-8, boş olamaz, < 512 bayt
    std::array<std::uint8_t, 32> manifest_sha256{};  // A7-K05
    std::uint64_t adim_no = 0;          // başlangıç adım sınırı
    CalismaDurumu calisma_durumu = CalismaDurumu::calisiyor;
    DalgaGirdisi dalga;
    Ortam ortam{};                      // `ortam_surumu` ve dolgu alanları yok sayılır
    std::span<const MalzemeOptikGirdisi> malzeme;  // ≤ 256; boş olabilir
    // Başlangıçta yaşayan gövdeler ve `adim_no` sınırındaki durumları (aynı kimlikler, aynı sayı).
    std::span<const GovdeKaydi> govdeler;
    std::span<const GovdeAni> anlar;
};

// Yayın sayaçları (her iş parçacığından okunabilir; değerler yaklaşık anlıktır).
struct YayinSayaclari {
    std::uint64_t yazilan_kayit = 0;     // yayın halkasına yazılan kayıt
    std::uint64_t dusen_durum = 0;       // yayın taşması: halkada yer olmadığı için atılan takvimli durum
    std::uint64_t kayip_korunan = 0;     // yayın taşması: atılamayan bir kayıt (olay, takvim dışı durum,
                                         // küme) yazılamadı; etkilenen oturum veda(tampon_tasti) ile kapanır
    std::uint64_t olay_tasmasi = 0;      // bir adım sınırında sabit olay tamponuna sığmayan olay
    std::uint64_t gecersiz_cagri = 0;    // sırası ya da girdisi geçersiz çağrı (uygulanmadı)
    std::uint64_t eksik_govde = 0;       // adim_siniri'nde durumu verilmeyen yaşayan gövde (geçersiz işaretlendi)
};

class Yayinci {
public:
    struct Ic;
    // `halka_bayt`: dünya → A2 yayın halkası (2'nin kuvvetine yuvarlanır; en az 256 KiB).
    // `etkin = false`: A2'siz durum; bütün işlevler hemen döner (hiçbir şey yazılmaz).
    explicit Yayinci(std::size_t halka_bayt = std::size_t{4} << 20u, bool etkin = true);
    ~Yayinci();
    Yayinci(const Yayinci&) = delete;
    Yayinci& operator=(const Yayinci&) = delete;

    // Bütün işlevler: true = uygulandı; false = uygulanmadı (sayaç: gecersiz_cagri ya da ilgili taşma).

    // Yeni çalıştırma (A2-K80, K96). Süreçteki ilk çağrı nesil 1'i kullanır; sonraki her çağrı nesli
    // 1 artırır, bağlı oturumu veda(yeni_calistirma) ile kapatır ve yeni oturumlar bu tanımla kurulur.
    // İlk çağrıdan önce el sıkışma tamamlanmaz (tam durum yoktur).
    bool yeni_calistirma(const CalistirmaTanimi& t) noexcept;
    // Çalıştırma bitti (fizik ilerlemiyor): son sınırın durumu `calisma_durumu = durdu` ile bir kez
    // daha gider; sim_nabzi "durdu" taşır. Yeni adım sınırı ancak yeni_calistirma'dan sonra verilir.
    bool calistirma_durdu() noexcept;

    // ---- `T` sınırının olayları: adim_siniri(T, …) çağrısından önce ----
    // Doğan gövdenin ilk süreklilik numarası 0'dır. `kaynak_tup`: torpidoda 1 ya da 2, yoksa 0.
    bool govde_dogdu(std::uint64_t adim_no, const GovdeKaydi& g, std::uint8_t kaynak_tup = 0) noexcept;
    bool govde_oldu(std::uint64_t adim_no, std::uint32_t kimlik, OlumNedeni neden) noexcept;
    // Aracın dolu tüp kümesi değişti; süreklilik numarası (bu adımda henüz artmadıysa) 1 artar.
    bool kutle_durumu(std::uint64_t adim_no, std::uint32_t kimlik, std::uint8_t yeni_tupler) noexcept;
    // Darbe (ör. fırlatma geri tepmesi): durum ara değerlenemeyecek biçimde sıçradı (A2-K21).
    bool sureklilik_kir(std::uint64_t adim_no, std::uint32_t kimlik) noexcept;
    // Yalnız görselleştirme içindir; atış kararını etkilemez (§3.4).
    bool isabet(std::uint64_t adim_no, std::uint32_t torpido, std::uint32_t hedef, const std::array<double, 3>& nokta_n,
                const std::array<double, 3>& normal_n, const std::array<double, 3>& bagil_hiz_n) noexcept;

    // `adim_no` sınırında yaşayan BÜTÜN gövdelerin olay sonrası durumu (A2-K23). Adım numarası
    // kesin artar (sıfırlama ve yeni çalıştırma dışında); duraklatılmışken çağrılmaz. Durumu
    // verilmeyen yaşayan gövde son bilinen pozuyla ve "geçersiz" işaretiyle gönderilir (sayılır).
    // Bir sınırda sahne değiştiyse (doğum, ölüm, kütle durumu, darbe) `T − h` ve `T` zamanlı iki
    // durum, takvimde olmasalar da gider (A2-K21); `T − h` için bir önceki adımın sınırı da bu
    // işlevle verilmiş olmalıdır.
    bool adim_siniri(std::uint64_t adim_no, std::span<const GovdeAni> anlar) noexcept;

    // Küme değişimleri: çalışırken bir sonraki adim_siniri ile, değilken hemen yayınlanır.
    bool dalga_kumesi(const DalgaGirdisi& d) noexcept;
    bool ortam(const Ortam& o) noexcept;
    bool malzeme_optik(std::span<const MalzemeOptikGirdisi> girdiler) noexcept;

    // Duraklatma, son `adim_siniri` çağrısının sınırında başlar (A2-K24): önce o sınırın durumu
    // (takvimde değilse takvim dışı; az önce gittiyse yinelenmez), sonra `duraklat` olayı gider;
    // sürdürene kadar durum gitmez, `sim_nabzi` sürer. `surdur` `devam` olayını gönderir.
    bool duraklat() noexcept;
    bool surdur() noexcept;
    // Sıfırlama (A2-K29, K80): nesil 1 artar, bütün gövdeler ölmüş sayılır; sim zamanı
    // `yeni_adim_no`'dan sürer. Ardından dünya yaşayan gövdeler için govde_dogdu(yeni_adim_no, …)
    // (YENİ kimliklerle) ve adim_siniri(yeni_adim_no, …) çağırır; bu ilk sınır duraklatılmışken de
    // kabul edilir. Oturum kapanmaz.
    bool sifirla(std::uint64_t yeni_adim_no) noexcept;
    // Sim süreci olağan biçimde kapanıyor: bağlı oturum veda(sim_kapaniyor) alır (A2-K67).
    bool kapaniyor() noexcept;

    std::uint32_t calisma_nesli() const noexcept;
    YayinSayaclari sayaclar() const noexcept;

    Ic& ic() noexcept { return *ic_; }

private:
    std::unique_ptr<Ic> ic_;
};

}  // namespace ida::a2
