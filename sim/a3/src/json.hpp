// json.hpp — A3 sunucusunun JSON çözücüsü ve yazıcısı (RFC 8259; A3-K121 yazım politikası).
//
// Çözücü katıdır ve sınırlıdır: derinlik ve öğe sayısı üst sınırlı, dizeler geçerli UTF-8, yalnız
// eşlenmiş vekil çiftli `\u` kaçışları, yinelenen nesne anahtarı reddedilir (kısıtlayan okuma: aynı
// alanın iki değeri hangi değerin uygulanacağını belirsiz bırakır). Yazıcı boşluksuz yazar, ASCII
// dışı karakterleri ham UTF-8 bırakır, yalnız `"`, `\` ve denetim karakterlerini kaçışlar.
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace ida::a3::json {

enum class Tur : std::uint8_t { bos, mantiksal, sayi, metin, dizi, nesne };

class Deger {
public:
    Tur tur = Tur::bos;
    bool mantik = false;
    double sayi = 0.0;
    std::string metin;
    std::vector<Deger> dizi;
    std::vector<std::pair<std::string, Deger>> nesne;

    const Deger* bul(std::string_view anahtar) const noexcept;
    // Sonlu ve tam değerli sayı (JSON Schema "integer"; 1.0 da tamsayıdır).
    bool tamsayi_mi() const noexcept;
    bool nesne_mi() const noexcept { return tur == Tur::nesne; }
};

struct CozumSiniri {
    std::size_t azami_derinlik = 64;
    std::size_t azami_deger = 65536;  // toplam değer sayısı (bellek üst sınırı)
};

// `metin` bütünüyle tek bir JSON değeri olmalıdır (baştaki ve sondaki boşluk serbest).
bool coz(std::string_view metin, Deger& sonuc, std::string* hata = nullptr,
         const CozumSiniri& sinir = {});

bool utf8_gecerli(std::string_view s) noexcept;
// UTF-8 → kod noktaları; geçersizse false.
bool kod_noktalari(std::string_view s, std::vector<std::uint32_t>& cikti);

// Yazım (A3-K121): boşluksuz; kaçış yalnız `"`, `\` ve U+0000–U+001F için.
void yaz(const Deger& d, std::string& cikti);
// Kanonik yazım (A3-K103): nesne anahtarları UTF-16 kod birimi sırasıyla (RFC 8785 §3.2.3).
void kanonik_yaz(const Deger& d, std::string& cikti);
void metin_yaz(std::string_view s, std::string& cikti);
// ECMAScript Number dizilimi (RFC 8785 §3.2.2.3); sonlu olmayan değer `null` yazılır.
void sayi_yaz(double v, std::string& cikti);
void tamsayi_yaz(std::int64_t v, std::string& cikti);
void tamsayi_yaz(std::uint64_t v, std::string& cikti);

// Küçük bir nesne kurucusu: yanıtlar ve olay ayrıntıları için.
class Nesne {
public:
    Nesne& metin(std::string_view ad, std::string_view deger);
    Nesne& sayi(std::string_view ad, std::int64_t deger);
    Nesne& sayi_u(std::string_view ad, std::uint64_t deger);
    Nesne& ondalik(std::string_view ad, double deger);
    Nesne& mantik(std::string_view ad, bool deger);
    Nesne& bos(std::string_view ad);
    // `ham` geçerli bir JSON değeri olmalıdır (çağıran sorumludur).
    Nesne& ham(std::string_view ad, std::string_view ham);
    std::string bitir() const { return s_ + "}"; }

private:
    void anahtar_(std::string_view ad);
    std::string s_ = "{";
    bool ilk_ = true;
};

}  // namespace ida::a3::json
