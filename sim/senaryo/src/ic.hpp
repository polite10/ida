// sim/senaryo iç bildirimleri (genel API değildir).
#pragma once

#include "ida/senaryo/senaryo.hpp"

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace ida::senaryo::ic {

// ------------------------------------------------------------------ bulgu toplayıcı
// Dosya tablosu: Konum::dosya bu tablodaki sıradır. Konumu olmayan (varsayılan/türetim)
// değerlerin bulguları `varsayilan_dosya`'ya yazılır.
class Bulgucu {
public:
    std::vector<std::string> dosyalar;
    int varsayilan_dosya = 0;
    int asama = 1;
    std::vector<Bulgu> bulgular;

    void ekle(std::string_view kural, Onem onem, Konum k, const Yol& yol, std::string ileti);
    void hata(std::string_view kural, Konum k, const Yol& yol, std::string ileti)
    {
        ekle(kural, Onem::hata, k, yol, std::move(ileti));
    }
    void uyari(std::string_view kural, Konum k, const Yol& yol, std::string ileti)
    {
        ekle(kural, Onem::uyari, k, yol, std::move(ileti));
    }
    // Değerin konumuyla; değer yoksa (nullptr) `ust`'ün dosyası, satırsız.
    void hata_d(std::string_view kural, const Deger* d, Konum ust, const Yol& yol,
                std::string ileti);
    bool hata_var() const;
    std::size_t hata_sayisi() const;
};

// ------------------------------------------------------------------ sayılar
// TOML/JSON ondalık yazımını (alt çizgisiz, işaretli olabilir) binary64'e en yakına çevirir
// (eşitlikte çift; A5-K96). Ayrıştırılamazsa nullopt. Taşmada ±inf, alt taşmada ±0 döner.
std::optional<double> ondalik_cevir(std::string_view metin);
// Yazımın sıfırdan farklı bir anlamlı basamağı var mı (alt taşma ayrımı için).
bool sifir_olmayan_basamak_var(std::string_view metin);

// ------------------------------------------------------------------ TOML
struct TomlOkuma {
    std::optional<Deger> kok;  // aşama 1 sözdizimi geçtiyse
};
// TOML 1.0 belgesini okur. A5-K01 (UTF-8, sözdizimi, uygulama sınırları), A5-K96, A5-K97
// ve tamsayı sınırı (A5-K10) bulgularını `b`'ye yazar. Reddedilen değerler Tur::gecersiz olur.
TomlOkuma toml_oku(std::string_view metin, int dosya_no, Bulgucu& b);

// ------------------------------------------------------------------ JSON
struct JsonOkuma {
    std::optional<Deger> kok;
    std::string hata;  // boş değilse ayrıştırma hatası
};
// JSON'u okur (yinelenen anahtar, derinlik sınırı aşımı ve sonlu olmayan sayı hatadır).
JsonOkuma json_oku(std::string_view metin, int dosya_no);

// ------------------------------------------------------------------ şema
enum class SemaKipi : std::uint8_t {
    toml,  // tamsayı tipinde TOML tamsayısı zorunlu (A5-K09)
    json,  // JSON Schema anlamı: kesirsiz sayı tamsayıdır
};
// `scenarios/sema/a5.schema.json`'ın gömülü kopyasını yorumlar. `tanim` "#" ya da
// "#/$defs/<ad>". Bulgular A5 kural numarasına eşlenir (sema.cpp'deki eşleme tablosu).
void sema_dogrula(const Deger& d, std::string_view tanim, const Yol& kok_yolu, SemaKipi kip,
                  Bulgucu& b);
// Gömülü şema metni (belge tutarlılığı testleri için).
std::string_view sema_metni();

// ------------------------------------------------------------------ Ek E
// sim/senaryo/veri/ek_e.toml'un gömülü kopyası (A5 Ek E bloğu) ve ayrıştırılmış hâli.
std::string_view ek_e_metni();
const Deger& ek_e();

// ------------------------------------------------------------------ JCS
void kanonik_yaz(const Deger& d, std::string& cikti);
std::string dizgi_kanonik(std::string_view s);

// ------------------------------------------------------------------ çözümleme
struct CozumBilgisi {
    // Kullanıcının verdiği (adım 5'ten önce var olan) sensör ayar anahtarları: "<ad>.<k>".
    std::vector<std::string> verilen_ayarlar;
};
// A5-K13–K15: araç dosyasının `arac`/`otopilot`'u ile senaryonun birleştirilmesi.
// Senaryo kökü yerinde değişir. A5-K15 bulgularını yazar.
void birlestir(Deger& senaryo, const Deger& arac_dosyasi, Bulgucu& b);
// A5-K16 adım 2–7. Birleşmiş senaryoyu çözümlenmiş biçime getirir; A5-K108 uyarılarını yazar.
void cozumle(Deger& s, Bulgucu& b);

// ------------------------------------------------------------------ anlamsal kurallar
// Aşama 2, çözümlemeden önce: A5-K102 (desteklenmeyen anahtar, kip ve olay türleri).
void desteklenmeyenler(const Deger& birlesik, Bulgucu& b);
// Aşama 2, çözümlenmiş senaryo üzerinde §4.3–§4.13'ün yükleme anı kuralları.
// `manifest` A7 kümesinin ayrıştırılmış manifesti (yoksa nullptr; A5-K115 bulgusu ayrıca).
void anlamsal_kurallar(const Deger& s, const Deger* manifest, Bulgucu& b);

// Yol yardımcıları
std::string yol_birlestir(std::string_view dizin, std::string_view goreli);
std::string dizin_adi(std::string_view yol);

}  // namespace ida::senaryo::ic
