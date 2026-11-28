// ida/a3/senaryo.hpp — senaryo ve ön denetim uçlarının veri kaynağı (W5-a3-senaryo; A3 §3.6, §3.8, §4.9).
//
// A3 kitaplığı A5 çözümleyicisini bilmez: senaryo dizinini, doğrulamayı, yüklenen senaryoyu ve ön denetim
// raporlarını sim sunucusu (`ida_sim`) verir. Sunucu isteği erişim, biçim (şema) ve ad kalıbı bakımından
// denetler; adı kalıba uymayan hiçbir istek buraya ulaşmaz (A3-K78). Hizmet yine de kendi tarafında
// aynı kalıbı denetlemelidir (derinlemesine savunma).
//
// İş parçacığı: bütün işlevler A3 G/Ç iş parçacığından çağrılır. Dünya iş parçacığı hizmetin içindeki
// durumu kendi işlevleriyle (sim tarafında) günceller; arayüz iki iş parçacığını da güvenli kılmalıdır.
// `yukleme_bekle` G/Ç iş parçacığını verilen süreye dek bekletir (senaryo/yukle senkron bir uçtur,
// A3 §3.6: yanıt, durum `yuklendi` olduktan sonra gelir).
#pragma once

#include <chrono>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace ida::a3 {

// A3-K78: `^[a-z0-9_-]{1,64}\.toml$`. Dizin ayırıcı, nokta nokta, mutlak yol ve büyük harf kalıp dışıdır.
inline bool senaryo_adi_gecerli(std::string_view ad) noexcept
{
    constexpr std::string_view kSon = ".toml";
    if (ad.size() < kSon.size() + 1u || ad.size() > kSon.size() + 64u || ad.substr(ad.size() - kSon.size()) != kSon) {
        return false;
    }
    for (const char c : ad.substr(0, ad.size() - kSon.size())) {
        const bool ok = (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' || c == '-';
        if (!ok) {
            return false;
        }
    }
    return true;
}

// `GET /senaryolar` öğesi (`senaryo_listesi`).
struct SenaryoDosyasi {
    std::string ad;
    std::uint64_t boyut_bayt = 0;
    std::string degisim_utc;   // ISO 8601 UTC ("2026-11-25T12:00:00Z"); boş: bilinmiyor
};

// `ad` (senaryo dizinindeki dosya) ya da `icerik` (TOML metni); ikisinden biri (istek şemasının oneOf'u).
struct SenaryoGirdisi {
    bool adli = true;
    std::string ad;
    std::string icerik;
};

enum class DosyaDurumu : std::uint8_t {
    tamam,
    yok,        // dosya yok (404 `bulunamadi`)
    gecersiz,   // ad kalıba uymuyor, dosya düzgün bir dosya değil (simgesel bağ vb.) ya da okunamadı (400 `gecersiz_istek`)
};

struct SenaryoDogrulama {
    DosyaDurumu dosya = DosyaDurumu::tamam;
    bool gecerli = false;
    std::string ozet;         // "sha256:<64 hex>"; geçersizse boş
    std::string rapor_json;   // A5 doğrulama raporu, değiştirilmeden (JSON nesnesi)
    std::string ileti;        // dosya durumu tamam değilse neden
};

// `senaryo/yukle` ön hazırlığı: A5 doğrulaması ve çalıştırılabilirlik denetimi. `hazir` ise dünyaya
// iletilecek komut `aday_id`'yi taşır.
struct SenaryoAdayi {
    enum class Sonuc : std::uint8_t {
        hazir,
        dosya_yok,       // 404 `bulunamadi`
        dosya_gecersiz,  // 400 `gecersiz_istek`
        gecersiz,        // A5 doğrulaması geçmedi: 422 `senaryo_gecersiz`, ayrıntı = rapor
        yurutulemez,     // doğrulandı ama bu sim'de yürütülemiyor: 422 `gecersiz_deger`
    };
    Sonuc sonuc = Sonuc::gecersiz;
    std::uint64_t aday_id = 0;
    std::string ozet;
    std::string rapor_json;
    std::string ileti;
};

struct YuklemeSonucu {
    enum class Tur : std::uint8_t {
        uygulandi,
        reddedildi,    // dünya reddetti (ya da yeni dünya kurulamadı): hata_kodu A3 §3.9 kodu
        zaman_asimi,   // süre doldu: sonuç bilinmiyor
    };
    Tur tur = Tur::zaman_asimi;
    std::string hata_kodu;
    std::string ileti;
};

class SenaryoHizmeti {
public:
    virtual ~SenaryoHizmeti() = default;

    // `GET /senaryolar`: yalnız yapılandırılmış senaryo dizini; ad sırasıyla.
    virtual std::vector<SenaryoDosyasi> listele() = 0;
    // `GET /senaryolar/{ad}`.
    virtual DosyaDurumu oku(std::string_view ad, std::string& icerik, std::string& ileti) = 0;
    // `POST /senaryo/dogrula` (A3-K76): dosyayı ve çalıştırma durumunu değiştirmez.
    virtual SenaryoDogrulama dogrula(const SenaryoGirdisi& g) = 0;
    // `POST /senaryo/yukle` ön hazırlığı (A3-K77). Başarılıysa aday hizmette saklanır; çağıran ya
    // komutu dünyaya iletip `yukleme_bekle` ile sonucu bekler ya da `aday_birak` der.
    virtual SenaryoAdayi hazirla(const SenaryoGirdisi& g) = 0;
    virtual void aday_birak(std::uint64_t aday_id) = 0;
    // Dünya adayı uygulayıp durum `yuklendi` olana (ya da reddedene) dek bekler.
    virtual YuklemeSonucu yukleme_bekle(std::uint64_t aday_id, std::chrono::milliseconds sure) = 0;
    // `GET /senaryo/cozumlenmis` (A3-K80): yüklü çözümlenmiş senaryonun kanonik baytları ve davranış
    // özeti ("sha256:<64 hex>"). Yüklü senaryo yoksa false.
    virtual bool cozumlenmis(std::string& bayt, std::string& ozet) = 0;
    // `GET /on_denetim` (kimlik boş: en sonuncusu) ve `GET /on_denetim/{rapor_kimligi}` (A3-K62): rapor
    // JSON'u (`on_denetim_raporu`); yoksa false.
    virtual bool on_denetim(std::string_view kimlik, std::string& rapor_json) = 0;
};

}  // namespace ida::a3
