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
};

class DiskKaynagi final : public DosyaKaynagi {
public:
    explicit DiskKaynagi(std::filesystem::path kok);
    std::optional<std::string> oku(const std::string& yol) const override;

private:
    std::filesystem::path kok_;
};

class BellekKaynagi final : public DosyaKaynagi {
public:
    std::map<std::string, std::string> dosyalar;
    std::optional<std::string> oku(const std::string& yol) const override;
};

struct CozumSonucu {
    DogrulamaRaporu rapor;
    std::optional<Deger> cozumlenmis;  // yalnız geçerliyse
    std::string kanonik;               // yalnız geçerliyse: RFC 8785 baytları
};

// Senaryoyu okur, doğrular ve çözümler. `senaryo_yolu` depo köküne göre göreli.
CozumSonucu senaryo_coz(const DosyaKaynagi& kaynak, const std::string& senaryo_yolu);

// A5-K02: A6'dan gelen çözümlenmiş senaryonun yeniden yüklenmesi. Baytların SHA-256'sı
// `davranis_ozeti`'ne eşit değilse, baytlar kanonik değilse ya da çözümlenmiş biçimin
// şemasına uymuyorsa reddedilir.
struct YenidenYukleme {
    bool tamam = false;
    std::vector<Bulgu> bulgular;
    std::optional<Deger> cozumlenmis;
};
YenidenYukleme cozumlenmis_yukle(std::string_view baytlar, std::string_view davranis_ozeti);

// RFC 8785 (JCS) dizilimi (A5-K17).
std::string kanonik_json(const Deger& d);
// SHA-256, 64 küçük onaltılık.
std::string sha256_hex(std::string_view baytlar);
// ECMAScript Number::toString (RFC 8785 §3.2.2.3) dizilimi; değer sonlu olmalı.
std::string sayi_dizilimi(double v);

}  // namespace ida::senaryo
