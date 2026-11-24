// A3 sunucusunun `ida_sim`'e bağlanması (W4-bagla-a3): komut alıcısı ve dünya ↔ A3 aktarımı.
//
// İş parçacıkları (04 §2.12; A3-K38):
//   A3 G/Ç iş parçacığı (kitaplık)  `anlik()` ve `al()` çağırır: eşzamanlı denetimler (A3-K54,
//                                   K91, K118, K123, Tablo 4.7a/4.7b) bu sınıftaki son anlık
//                                   görüntüye göre yapılır; kabul edilen komut kuyruğa girer.
//   Dünya iş parçacığı (Calistirici) `cek()` ile komutları yalnız fizik sınırında alır (A3-K57),
//                                   kararını orada yeniden verir ve sonucu `Yayinci` ile yayınlar.
//
// Aktarım (W4-bagla-a3 kapsam 3; EMNİYET):
//   - Genel komutlar: 256 komutluk sınırlı halka (A3-K59). Üretici tarafı (G/Ç) kısa bir kilitle
//     yazar; tüketici (dünya) kilit almaz. Halka doluyken `dolu` döner (kitaplık 503 `mesgul`).
//   - Kısıtlayan istekler (acil durdurma bas; tüp başına fiş çıkar, pim tak; dört daraltıcı yer
//     istasyonu komutu): etki başına sabit boylu, taşmayan tek yuva. Yuva doluyken `dolu` döner ve
//     kitaplığın etki başına mandalı (W4-K22 (a)) bekletir; dünya her sınırda önce bu yuvaları
//     boşaltır. Kısıtlayan istek genel halkanın doluluğundan ve gevşeten isteklerden bağımsızdır:
//     gevşetenin arkasında beklemez ve hiçbir doluluk yüzünden atılmaz.
//   - Anlık görüntü: dünya `anlik_ver` ile verir (`try_lock`: G/Ç kopyalarken dünya beklemez,
//     görüntü bir sonraki çağrıda yenilenir).
#pragma once

#include "ida/a3/komut.hpp"

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

namespace ida::sunucu {

class A3Alici final : public a3::KomutAlicisi {
public:
    static constexpr std::size_t kGenelKuyruk = 256;   // A3-K59
    // Kısıtlayan etki yuvaları: acil, fiş f1/f2, pim f1/f2, disarm, set_mode_daraltici,
    // fc_disarm, fc_abort.
    static constexpr std::size_t kKisitYuva = 9;

    explicit A3Alici(bool hil);
    ~A3Alici() override;
    A3Alici(const A3Alici&) = delete;
    A3Alici& operator=(const A3Alici&) = delete;

    // ---- A3 G/Ç iş parçacığı ----
    a3::AnlikDurum anlik() override;
    a3::AlimSonucu al(const a3::Komut& k) override;

    // ---- Dünya iş parçacığı ----
    // Anlık görüntüyü verir; G/Ç o an kopyalıyorsa false (dünya beklemez; sonra yeniden denenir).
    bool anlik_ver(const a3::AnlikDurum& a);
    // Bekleyen komutları alır: önce kısıtlayan yuvalar (etki sırasıyla), sonra genel halka
    // (komut_no sırasıyla). Kilit almaz.
    void cek(std::vector<a3::Komut>& kisitlayan, std::vector<a3::Komut>& genel);
    // Sınama: kuyruktaki genel komut sayısı.
    std::size_t genel_dolu() const;

private:
    static int etki_yuvasi(const a3::Komut& k);
    a3::AlimSonucu kuyruga(const a3::Komut& k);

    bool hil_ = false;
    std::mutex anlik_m_;
    a3::AnlikDurum anlik_;
    struct Yuva {
        std::atomic<bool> dolu{false};
        a3::Komut k;
    };
    std::array<Yuva, kKisitYuva> kisit_;
    std::mutex uretici_m_;   // yalnız üreticiler arası (G/Ç ve sınama kancası)
    std::vector<a3::Komut> halka_;
    std::atomic<std::size_t> bas_{0};   // tüketici
    std::atomic<std::size_t> son_{0};   // üretici
};

// A3 ↔ A1 dönüşümleri ve olay ayrıntısı yazımı (bellek ayırmaz; sabit tampon).
namespace a3b {

// Tablo 4.7a: komutun bu durumda geçerli olup olmadığı (izleme ve tekrar kipleri bu dilimde yok).
bool gecis_gecerli(a3::CalistirmaDurumu d, a3::KomutTuru t);
bool canli(a3::CalistirmaDurumu d);   // hazirlaniyor, kosuyor, duraklatildi
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

}  // namespace ida::sunucu
