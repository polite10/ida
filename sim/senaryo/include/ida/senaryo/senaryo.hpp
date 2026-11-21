// İDA senaryo kitaplığı (W4-senaryo, dilim 1): A5 senaryo ve araç dosyalarının çözümleyicisi
// ve doğrulayıcısı.
//
// Sözleşme: docs/sozlesmeler/A5-senaryo.md (1.0). Girdi bir senaryo TOML'udur; çözümleyici
// başvurduğu araç dosyasını ve A7 manifestini okur, üç aşamalı doğrulamanın ilk ikisini yapar
// (A5-K90; aşama 3 ön denetimi A1'in ve sim'indir), çözümlenmiş senaryoyu (A5-K16), kanonik
// baytlarını ve davranış özetini (A5-K17) ve doğrulama raporunu (A5-K92, §3.14) üretir.
// Senaryonun yürütülmesi bu kitaplığın işi değildir.
//
// Dosya yolları depo köküne göre göreli ve `/` ayırıcılıdır (A5 §3.14 `dosya`); dosyalar
// `DosyaKaynagi` üzerinden okunur (diskte ya da testte bellekte).
#pragma once

#include "ida/senaryo/deger.hpp"

#include <cstdint>
#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace ida::senaryo {

// Bu okuyucunun sözleşme sürümü (A5-K03, K04).
inline constexpr int kSemaMajor = 1;
inline constexpr int kSemaMinor = 0;

// Uygulama sınırları (dayanıklılık; sözleşme dışı, bu işin seçimi).
inline constexpr std::size_t kAzamiDosyaBoyu = 4u * 1024u * 1024u;  // bayt
inline constexpr std::size_t kAzamiDerinlik = 48;                    // iç içe tablo/dizi

enum class Onem : std::uint8_t { hata, uyari };

struct Bulgu {
    std::string kural;                  // "A5-K" + 2–3 basamak
    Onem onem = Onem::hata;
    int asama = 1;                      // 1, 2 (A5-K90)
    std::string dosya;                  // depo köküne göre göreli
    std::string yol;                    // A5-K93
    std::optional<std::int64_t> satir;  // A5-K94
    std::string ileti;
};

// A5-K92 sırası: (dosya, satir, yol, kural); satırı olmayan bulgu aynı dosyada önce gelir.
void bulgulari_sirala(std::vector<Bulgu>& b);

struct DogrulamaRaporu {
    bool gecerli = false;
    std::optional<std::string> davranis_ozeti;          // 64 küçük onaltılık
    std::map<std::string, std::string> kaynak_ozetleri; // dosya → SHA-256 (ham bayt)
    std::vector<Bulgu> bulgular;

    std::size_t hata_sayisi() const;
    std::size_t uyari_sayisi() const;
    // §3.14 JSON nesnesi (kanonik dizilim).
    std::string json() const;
};

class DosyaKaynagi {
public:
    virtual ~DosyaKaynagi() = default;
    // Depo köküne göre göreli yoldaki dosyanın baytları; yoksa ya da okunamıyorsa boş.
    virtual std::optional<std::string> oku(const std::string& yol) const = 0;
    // A5-K12: `yol`un gerçek konumu (simgesel bağlar çözülmüş) `taban_dizin`in gerçek
    // konumunun içinde mi. Bellekteki kaynakta bağ yoktur; metin düzeyindeki denetim yeter.
    virtual bool icinde_mi(const std::string& taban_dizin, const std::string& yol) const
    {
        (void)taban_dizin;
        (void)yol;
        return true;
    }
};

class DiskKaynagi final : public DosyaKaynagi {
public:
    explicit DiskKaynagi(std::filesystem::path kok);
    std::optional<std::string> oku(const std::string& yol) const override;
    bool icinde_mi(const std::string& taban_dizin, const std::string& yol) const override;

private:
    std::filesystem::path kok_;
};

class BellekKaynagi final : public DosyaKaynagi {
public:
    std::map<std::string, std::string> dosyalar;
    std::optional<std::string> oku(const std::string& yol) const override;
};

namespace ic {
struct Yapici;
}

// Doğrulayıcının geçerli saydığı çözümlenmiş senaryo. Yalnız `senaryo_coz` ve
// `cozumlenmis_yukle` üretebilir (kurucu kapalıdır); yükleyiciler (`fizik_kur`,
// `sensor_kur`) yalnız bu türü kabul eder, böylece doğrulanmamış bir ağaç onlara
// derleme zamanında verilemez.
class DogrulanmisSenaryo {
public:
    const Deger& agac() const { return agac_; }
    const std::string& kanonik() const { return kanonik_; }
    const std::string& davranis_ozeti() const { return ozet_; }

private:
    friend struct ic::Yapici;
    DogrulanmisSenaryo(Deger agac, std::string kanonik, std::string ozet)
        : agac_(std::move(agac)), kanonik_(std::move(kanonik)), ozet_(std::move(ozet))
    {
    }
    Deger agac_;
    std::string kanonik_;
    std::string ozet_;
};

struct CozumSonucu {
    DogrulamaRaporu rapor;
    std::optional<DogrulanmisSenaryo> cozumlenmis;  // yalnız geçerli senaryoda
    std::string kanonik;                            // yalnız geçerli senaryoda: RFC 8785 baytları
};

// Senaryoyu okur, doğrular ve çözümler. `senaryo_yolu` depo köküne göre göreli. Girdi
// senaryo olmalıdır (araç dosyası A5-K02 bulgusuyla reddedilir; onu `dosya_dogrula` sınar).
CozumSonucu senaryo_coz(const DosyaKaynagi& kaynak, const std::string& yol);

// Dosyayı türüne göre doğrular: `meta.tur = "senaryo"` ise `senaryo_coz`; `"arac"` ise araç
// dosyasını tek başına doğrular (aşama 1 ve araca ait aşama 2 kuralları; ortam ve senaryo
// varsayılanı uydurulmaz). Araç dosyası için çözümlenmiş senaryo ve davranış özeti yoktur.
CozumSonucu dosya_dogrula(const DosyaKaynagi& kaynak, const std::string& yol);

// A5-K02: A6'dan gelen çözümlenmiş senaryonun yeniden yüklenmesi. Baytların SHA-256'sı
// `davranis_ozeti`'ne eşit değilse, baytlar kanonik değilse, çözümlenmiş biçimin şemasına
// uymuyorsa ya da baytları değiştirmeden uygulanabilen sürüm ve anlamsal denetimlerden biri
// hata verirse reddedilir. `kaynak` verilirse A7 atıfları (A5-K18, K19, K36, K115) da sınanır.
struct YenidenYukleme {
    bool tamam = false;
    std::vector<Bulgu> bulgular;
    std::optional<DogrulanmisSenaryo> cozumlenmis;
};
YenidenYukleme cozumlenmis_yukle(std::string_view baytlar, std::string_view davranis_ozeti,
                                 const DosyaKaynagi* kaynak = nullptr);

// RFC 8785 (JCS) dizilimi (A5-K17).
std::string kanonik_json(const Deger& d);
// SHA-256, 64 küçük onaltılık.
std::string sha256_hex(std::string_view baytlar);
// ECMAScript Number::toString (RFC 8785 §3.2.2.3) dizilimi; değer sonlu olmalı.
std::string sayi_dizilimi(double v);

}  // namespace ida::senaryo
