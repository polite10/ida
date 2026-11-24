// sema.hpp — A3 ve A1 komut kataloğu şemalarının kullandığı JSON Schema (2020-12) alt kümesinin
// doğrulayıcısı ve `pattern` için küçük bir ECMAScript düzenli ifade eşleyicisi.
//
// Desteklenen anahtar sözcükler `Sema::desteklenmeyen_bul` ile denetlenir: şemada bilinmeyen bir
// anahtar sözcük varsa sunucu başlamaz (kapalı hata: bilinmeyen bir kısıt sessizce yok sayılmaz).
// Yalnız G/Ç iş parçacığından kullanılır (desen önbelleği eşzamanlı değildir).
#pragma once

#include "json.hpp"

#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace ida::a3 {

// ECMAScript düzenli ifadesinin şemalarda geçen alt kümesi: `^`, `$`, karakter sınıfları (aralık,
// olumsuzlama, `\uXXXX`), `.`, gruplar ve seçenek (`|`), niceleyiciler `* + ? {n} {n,} {n,m}`.
// Eşleme kod noktaları üzerindedir; adım bütçesi aşılırsa eşleşmez (kapalı hata).
class Desen {
public:
    static std::unique_ptr<Desen> derle(std::string_view kaynak, std::string* hata);
    bool ara(std::string_view metin) const;  // ECMAScript `test` anlamı (çapa yoksa her yerde)
    ~Desen();

    struct Dugum;

private:
    Desen() = default;
    std::unique_ptr<Dugum> kok_;
};

class Sema {
public:
    // `kok`, `$ref: "#/$defs/<ad>"` başvurularının çözüldüğü belgedir; Sema'dan uzun yaşamalıdır.
    explicit Sema(const json::Deger& kok) : kok_(kok) {}

    const json::Deger* tanim(std::string_view ad) const;

    // Şema ağacında desteklenmeyen anahtar sözcük ya da derlenemeyen desen varsa false ve `hata`.
    bool desteklenmeyen_bul(const json::Deger& sema, std::string& hata) const;

    bool dogrula(const json::Deger& sema, const json::Deger& deger, std::string* hata = nullptr) const;

private:
    bool dogrula_(const json::Deger& sema, const json::Deger& deger, std::string& yol, std::string* hata,
                  int derinlik) const;
    const Desen* desen_(const std::string& kaynak) const;

    const json::Deger& kok_;
    mutable std::map<std::string, std::unique_ptr<Desen>> desenler_;
};

// JSON değerlerinin şema anlamında eşitliği (sayılar değerle, nesneler sırasız).
bool json_esit(const json::Deger& a, const json::Deger& b);

}  // namespace ida::a3
