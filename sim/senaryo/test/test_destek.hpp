// sim/senaryo testlerinin ortak düzeni.
//
// Her test `int test_<ad>(const TestBaglami&)` işlevidir; 0 geçti demektir. Testler depo
// köküne (argv[2]) göre `scenarios/`, `assets/` ve `docs/sozlesmeler/` dosyalarını okur;
// bozuk dosyalar bellekte, örnek dosyalardan tek bir metin değişikliğiyle üretilir.
#pragma once

#include "ida/senaryo/senaryo.hpp"

#include <cstdio>
#include <fstream>
#include <iterator>
#include <optional>
#include <string>
#include <vector>

namespace ida::senaryo::test {

struct TestBaglami {
    std::string kok;  // depo kökü
};

struct Denetci {
    int hata = 0;
    void dogrula(bool kosul, const char* dosya, int satir, const std::string& ileti)
    {
        if (!kosul) {
            ++hata;
            std::printf("  KALDI %s:%d: %s\n", dosya, satir, ileti.c_str());
        }
    }
};

#define DOGRULA(d, kosul, ileti) (d).dogrula((kosul), __FILE__, __LINE__, (ileti))

inline std::string dosya_oku(const std::string& yol)
{
    std::ifstream f(yol, std::ios::binary);
    return std::string((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
}

inline const char* kSenaryo = "scenarios/ornek-havuz.toml";
inline const char* kArac = "scenarios/ida0.toml";
inline const char* kManifest = "assets/ornek/manifest.json";

// Örnek senaryo, İDA-0 ve A7 uygunluk örneği bellekte.
inline BellekKaynagi temel_kaynak(const TestBaglami& b)
{
    BellekKaynagi k;
    for (const char* y : {kSenaryo, kArac, kManifest}) {
        k.dosyalar[y] = dosya_oku(b.kok + "/" + y);
    }
    return k;
}

// `eski` metinde tam bir kez geçmelidir; değilse boş döner (test kalır).
inline std::optional<std::string> degistir(const std::string& metin, const std::string& eski,
                                           const std::string& yeni)
{
    const std::size_t i = metin.find(eski);
    if (i == std::string::npos || metin.find(eski, i + 1) != std::string::npos) {
        return std::nullopt;
    }
    std::string o = metin;
    o.replace(i, eski.size(), yeni);
    return o;
}

// `isaret`'in geçtiği ilk satır (1 tabanlı); yoksa 0.
inline std::int64_t satir_bul(const std::string& metin, const std::string& isaret)
{
    const std::size_t i = metin.find(isaret);
    if (i == std::string::npos) {
        return 0;
    }
    std::int64_t s = 1;
    for (std::size_t j = 0; j < i; ++j) {
        s += metin[j] == '\n' ? 1 : 0;
    }
    return s;
}

inline const Bulgu* bulgu_bul(const DogrulamaRaporu& r, const std::string& kural,
                              const std::string& yol)
{
    for (const auto& b : r.bulgular) {
        if (b.kural == kural && b.yol == yol) {
            return &b;
        }
    }
    return nullptr;
}

inline std::string bulgu_ozeti(const DogrulamaRaporu& r)
{
    std::string o;
    for (const auto& b : r.bulgular) {
        o += "    " + b.kural + (b.onem == Onem::hata ? " hata" : " uyari") + " a" +
             std::to_string(b.asama) + " " + b.dosya + ":" +
             (b.satir ? std::to_string(*b.satir) : std::string("null")) + " [" + b.yol + "] " +
             b.ileti + "\n";
    }
    return o;
}

}  // namespace ida::senaryo::test
