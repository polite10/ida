// A3 sunucusunun `ida_sim`'e bağlanması (W4-bagla-a3): komut alıcısı ve dünya ↔ A3 aktarımı.
//
// İş parçacıkları (04 §2.12; A3-K38):
//   A3 G/Ç iş parçacığı (kitaplık)  `anlik()` ve `al()` çağırır: eşzamanlı denetimler (A3-K54,
//                                   K91, K118, K123, Tablo 4.7a/4.7b) son anlık görüntüye göre;
//                                   yer istasyonu komutunun A1 karşılığı da burada çözülür (JSON
//                                   ayrıştırma dünya iş parçacığına girmez: W4-K27 (5)).
//   Dünya iş parçacığı (Calistirici) `cek()` ile istekleri yalnız fizik sınırında (ve hazırlık ile
//                                   `yuklendi` beklemesinde) alır, kararını orada yeniden verir.
//
// Aktarım (EMNİYET; W4-K22, W4-K27 (2), (5)):
//   - Genel istekler: 256'lık sınırlı halka (A3-K59); üreticiler kısa bir kilitle yazar, tüketici
//     (dünya) kilit almaz. Doluyken `dolu` (kitaplık 503 `mesgul`).
//   - Kısıtlayan istekler (acil durdurma bas; tüp başına fiş çıkar, pim tak; dört daraltıcı yer
//     istasyonu komutu): etki başına taşmayan tek yuva. Doluyken `dolu` döner, kitaplığın etki
//     başına mandalı bekletir.
//   - Tutarlı kesim (W4-K27 (2)): `cek` önce genel halkanın o anki ucunu okur, sonra kısıtlayan
//     yuvaları boşaltır, sonra halkayı kesime kadar alır. Her istek G/Ç'de tek bir artan sıra
//     numarası alır; kesimdeki bir genel istekten önce teslim edilmiş her kısıtlayan aynı `cek`te
//     (ya da daha önce) alınır ya da yuvasında bekler (`bekleyen_kisit_en_kucuk`).
//   - Anlık görüntü: kilitsiz üçlü tampon (dünya yazar, kilit almaz, bellek ayırmaz; G/Ç okur).
#pragma once

#include "ida/a3/komut.hpp"

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

namespace ida::sunucu {

// A3 ↔ A1 dönüşümleri ve olay ayrıntısı yazımı (bellek ayırmaz; sabit tampon).
namespace a3b {

// Tablo 4.7a: komutun bu durumda geçerli olup olmadığı (izleme ve tekrar kipleri bu dilimde yok).
// W4-duraklat: `duraklat` kosuyor'dan, `surdur` ve `adimla` duraklatildi'dan; kip kuralları (Tablo
// 4.7b: HIL'de adimla ve hiz `kipte_gecersiz`, duraklat `gecersizligi_onayla` ister) ayrıca.
bool gecis_gecerli(a3::CalistirmaDurumu d, a3::KomutTuru t);
bool canli(a3::CalistirmaDurumu d);   // hazirlaniyor, kosuyor, duraklatildi (A3-K91)
const char* durum_adi(a3::CalistirmaDurumu d);
bool calistirma_mi(a3::KomutTuru t);

// Tablo 4.8 bitleri (A3) kopru::Gecersizlik bitlerinden (A1-K79) ve modelin sayaçlarından.
std::uint32_t gecerlilik_bitleri(std::uint32_t kopru_bitleri, std::uint64_t beklenmeyen_sifirlama,
                                 std::uint64_t sensor_kuyruk_tasma);
// Kipte zamanlama bakımından geçersiz kılan ("G") bitler (Tablo 4.8).
std::uint32_t gecersiz_kilan(bool hil);
// Bit adı (Tablo 4.8); bilinmiyorsa boş.
std::string_view neden_adi(unsigned bit);

// Sınırlı JSON nesne yazıcısı: sabit tamponda, A3-K121 politikası (boşluksuz, yalnız ASCII ad ve
// değerler; çağıran ASCII verir).
class JsonYaz {
public:
    JsonYaz& metin(const char* ad, std::string_view v);
    JsonYaz& sayi(const char* ad, long long v);
    JsonYaz& sayi_u(const char* ad, unsigned long long v);
    JsonYaz& mantik(const char* ad, bool v);
    JsonYaz& ham(const char* ad, std::string_view json);   // önceden yazılmış JSON değeri
    std::string_view bitir();
    bool tasti() const { return tasti_; }

private:
    void ad_(const char* ad);
    void ekle_(std::string_view s);
    std::array<char, 1024> t_{};
    std::size_t n_ = 0;
    bool ilk_ = true;
    bool tasti_ = false;
};

// A3 tarafındaki kanonik yer istasyonu komutunun A1 karşılığı (protocol/komutlar.json).
enum class YiTur : std::uint8_t {
    basit,        // IDA_CMD_SIMPLE (tur, arg)
    guided,       // IDA_CMD_GUIDED
    fc,           // IDA_CMD_FC (op, tube)
    desteklenmiyor,   // köprüde karşılığı yok (manual) ya da bileşik (bu dilimde yürütülmez)
};
struct YiKarsilik {
    YiTur tur = YiTur::desteklenmiyor;
    const char* ad = "";        // kataloğun komut adı (durağan metin; dünya tarafında kopyalanmaz)
    std::uint8_t a1_tur = 0;    // IDAW_CMD_TYPE (basit) ya da IDAW_FC_OP (fc)
    std::uint16_t arg = 0;      // basit: arg; fc: tube (255: ikisi)
    std::uint8_t sp_mask = 0;   // guided
    std::array<float, 2> pos_ne{};
    float depth = 0.0F, altitude = 0.0F, yaw = 0.0F, yaw_rate = 0.0F;
    std::array<float, 3> vel_ned{};
};
// Kataloğun adı ve kanonik parametrelerinden A1 karşılığı; parametre çözülemezse false.
bool yi_karsilik(const std::string& komut, const std::string& parametreler_json, YiKarsilik& k);

// IDAW_ACK adı (önek olmadan); bilinmiyorsa boş.
std::string_view ack_adi(std::uint8_t ack);

// A3-K63: `YYYYMMDD-hhmmss-xxxxxx` (UTC başlangıç anı + 6 onaltılık rastgele karakter).
std::string run_id_uret();
// A1-K126 / A3-K63: kartlı kiplerde a1 run_id = run_id metninin SHA-256'sının ilk dört baytı
// (küçük uçlu; 0 ise 1).
std::uint32_t a1_run_id(const std::string& run_id);

}  // namespace a3b

// Dünya iş parçacığına aktarılan istek.
struct A3Giris {
    std::uint64_t sira = 0;        // G/Ç'nin verdiği teslim sırası (1'den, artan)
    a3::Komut k;
    a3b::YiKarsilik yi;            // yer istasyonu komutunun A1 karşılığı (G/Ç'de çözüldü)
};

// Çalıştırmanın anlık görüntüsü (dünya → G/Ç; bellek ayırmayan düz yapı).
struct AnlikPod {
    std::uint8_t durum = 1;        // a3::CalistirmaDurumu
    std::uint8_t kip = 1;          // a3::ZamanKipi
    bool run_var = false;
    std::array<char, 40> run_id{};
    std::int64_t sim_t_ns = 0;
    std::uint64_t adim_no = 0;
    bool gecerli = true;
    bool gercek_zamani_kacirdi = false;
    std::uint32_t gecerlilik_nedenleri = 0;
    bool ozet_var = false;
    std::array<char, 80> ozet{};
    std::uint32_t a1_run_id = 0;
    std::uint64_t durum_rev = 1;
    std::uint32_t emniyet_rev = 1;
    bool emniyet_rev_doydu = false;
    std::uint8_t acil = 0;
    std::array<std::uint8_t, 2> fis{0, 0};
    std::array<std::uint8_t, 2> pim{1, 1};
    bool son_degisim_var = false;
    std::uint64_t son_degisim_adimi = 0;
    bool kuruluyor = false;          // W5-K58: dünya yeniden kuruluyor (yeni dünya `yuklendi` olana dek)
    // parti_gonderilen, parti_kacirildi, oturum_kopma, sim_gecikmesi, a3_halka_tasma
    std::array<std::int64_t, 5> sayac{};
    std::uint8_t kademe_sonar = 0;   // W4-bagla-sonar: 0 kapali, 1 sade
    std::uint8_t kademe_gorunum = 0; // W5-bagla-a2: 0 kapali (A2 açılmadı), 1 sade, 2 tam
};

class A3Alici final : public a3::KomutAlicisi {
public:
    static constexpr std::size_t kGenelKuyruk = 256;   // A3-K59
    // Kısıtlayan etki yuvaları: 0 acil, 1–2 fiş f1/f2, 3–4 pim f1/f2, 5 disarm,
    // 6 set_mode_daraltici, 7 fc_disarm, 8 fc_abort.
    static constexpr std::size_t kKisitYuva = 9;
    static constexpr std::uint16_t kYiYuvalari = 0x1E0u;    // 5–8
    static constexpr std::uint16_t kButunYuvalar = 0x1FFu;
    // EMNİYET (W4-K28 sol-3 #1): çalıştırma denetimi istekleri genel halkadan ve dünyanın genel
    // deposundan bağımsız, sabit boylu iki yoldan geçer: `durdur` ve öteki denetimler (baslat,
    // hiz, sifirla). Her `cek` ikisini de bütünüyle alır (depo doluluğu sınırlamaz); `durdur`lar
    // önce eklenir (öteki denetimlere baskın). Yol doluysa `dolu` (kitaplık 503 `mesgul`):
    // kabul edilmiş bir istek hiçbir kuyruk durumu yüzünden alınmadan kalmaz.
    static constexpr std::size_t kDenetimYolu = 8;

    explicit A3Alici(bool hil);
    ~A3Alici() override;
    A3Alici(const A3Alici&) = delete;
    A3Alici& operator=(const A3Alici&) = delete;

    // Zaman kipi (Tablo 4.7b denetimi): ilk dünya kurulurken ve senaryo/yukle ya da sifirla ile yeni dünya
    // başka kipte kurulduğunda dünya iş parçacığından verilir (yeni dünya hiçbir istek işlenmeden önce).
    void kip_ayarla(bool hil);

    // ---- A3 G/Ç iş parçacığı ----
    a3::AnlikDurum anlik() override;
    a3::AlimSonucu al(const a3::Komut& k) override;
    std::optional<a3::AlimSonucu> on_denetim(const a3::Komut& k) override;   // W5-K57

    // ---- Dünya iş parçacığı (kilitsiz, bellek ayırmaz; vektörler çağıranın ayırdığı kapasitede) ----
    void anlik_yaz(const AnlikPod& p);
    // Tutarlı kesimle alır (W4-K27 (2)): kesim, kısıtlayan yuvalar (`atla` maskesindekiler hariç;
    // sıra numarasıyla), sonra genel halka kesime kadar (en çok `genel_azami` istek); ardından
    // çalıştırma denetimi yollarının tamamı `genel`e eklenir (önce durdur; en çok 2 · kDenetimYolu).
    void cek(std::vector<A3Giris>& kisitlayan, std::vector<A3Giris>& genel, std::uint16_t atla = 0,
             std::size_t genel_azami = kGenelKuyruk);
    // `maske`deki dolu kısıtlayan yuvalardaki isteklerin en küçük sıra numarası (yoksa 0).
    std::uint64_t bekleyen_kisit_en_kucuk(std::uint16_t maske) const;
    std::size_t genel_dolu() const;
    // Yalnız sınama: `cek`in evreleri arasında (1: kesimden sonra, 2: yuvalardan sonra) çağrılır.
    std::function<void(int evre)> cek_kancasi;
    // EMNİYET (W4-K28 (3)): `al` anlık görüntüyü okumaktan kuyruğa koymaya dek üretici kilidini
    // tutar. Dünya, koşu dışındaki bir iç geçişte (hazırlık tamamlandı → kosuyor) bu kilidi alıp son
    // bir çekiş yapar ve yeni görüntüyü yazar: eski görüntüyle denetlenmiş her istek ya bu çekişte
    // alınır ya da yeni görüntüyle denetlenir (iç geçiş kabul edilmiş isteği bayatlatamaz). Sınır
    // döngüsünde alınmaz (W4-K27 (5)); tek istisna SIL'de `adimla`'nın iç geçişidir (adımlama bitti →
    // duraklatildi; mimar kararı, W4-duraklat düzeltme turu): yalnız SIL'de, kısa süre ve sim zamanı
    // durmuşken alınır (HIL döngüsünde hiçbir zaman).
    std::unique_lock<std::mutex> gecis_kilidi() { return std::unique_lock<std::mutex>(uretici_m_); }

private:
    static int etki_yuvasi(const a3::Komut& k);
    a3::AlimSonucu kuyruga(const a3::Komut& k, const a3b::YiKarsilik& yi);   // uretici_m_ tutulurken
    // Çalıştırma denetimi komutunun giriş denetimi (`al` ve `on_denetim` ortak; uretici_m_ tutulurken).
    std::optional<a3::AlimSonucu> calistirma_denetle_(const a3::Komut& k, const a3::AnlikDurum& a) const;

    bool hil_ = false;
    // Üçlü tampon: dünya `arka_`'ya yazar ve `orta_` ile değiştirir; okuyucu `on_`'u okur.
    std::array<AnlikPod, 3> tampon_{};
    std::atomic<unsigned> orta_{1u};
    unsigned arka_ = 0;
    std::mutex okuyucu_m_;   // yalnız okuyucular arası (G/Ç ve sınama kancası); dünya almaz
    unsigned on_ = 2;
    struct Yuva {
        std::atomic<bool> dolu{false};
        std::atomic<std::uint64_t> sira{0};
        A3Giris g;
    };
    std::array<Yuva, kKisitYuva> kisit_;
    std::mutex uretici_m_;   // üreticiler arası (G/Ç ve sınama kancası) ve gecis_kilidi
    std::uint64_t sira_ = 0;
    std::vector<A3Giris> halka_;
    std::atomic<std::size_t> bas_{0};   // tüketici
    std::atomic<std::size_t> son_{0};   // üretici
    // Çalıştırma denetimi yolları (tek üretici kilit altında, tek tüketici kilitsiz).
    struct DenetimYolu {
        std::array<A3Giris, kDenetimYolu + 1u> g{};
        std::atomic<std::size_t> bas{0};
        std::atomic<std::size_t> son{0};
    };
    DenetimYolu durdur_, denetim_;
    static bool yola_koy(DenetimYolu& y, A3Giris&& g);
    static void yoldan_al(DenetimYolu& y, std::vector<A3Giris>& cikti);
};

}  // namespace ida::sunucu
