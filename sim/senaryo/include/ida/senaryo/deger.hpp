// İDA senaryo kitaplığı (W4-senaryo): ayrıştırılmış TOML/JSON değer ağacı.
//
// TOML ve JSON belgelerinin ortak bellek gösterimi. TOML tamsayısı ile kayan noktalı sayı
// ayrı tutulur (A5-K09); kanonik dizilimde ikisi de binary64 değeriyle yazılır (A5-K17).
// Her değer geldiği dosyayı ve satırı taşır (A5-K94); varsayılandan ya da türetimden gelen
// değerlerde satır yoktur.
#pragma once

#include <cstdint>
#include <map>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace ida::senaryo {

// Değerin kaynağı. `dosya` çözümleyicinin dosya tablosundaki sıradır (-1: varsayılan ya da
// türetim); `satir` 1 tabanlıdır (0: yok).
struct Konum {
    int dosya = -1;
    std::int64_t satir = 0;
};

enum class Tur : std::uint8_t {
    bos,        // JSON null
    mantiksal,
    tamsayi,    // TOML tamsayısı ya da JSON tamsayı belirteci (int64)
    sayi,       // binary64
    dizgi,      // UTF-8
    dizi,
    tablo,
    gecersiz,   // aşama 1'de reddedilmiş değer (A5-K96, K97); şema denetimi atlar
};

class Deger {
public:
    Tur tur = Tur::bos;
    bool mantik = false;
    std::int64_t tam = 0;
    double sayi = 0.0;
    std::string dizgi;
    std::vector<Deger> dizi;
    std::map<std::string, Deger> tablo;  // anahtar sırası bayt sırası; kanonik yazıcı yeniden sıralar
    Konum konum;

    static Deger yeni_tablo(Konum k = {});
    static Deger yeni_dizi(Konum k = {});
    static Deger yeni_dizgi(std::string s, Konum k = {});
    static Deger yeni_tamsayi(std::int64_t v, Konum k = {});
    static Deger yeni_sayi(double v, Konum k = {});
    static Deger yeni_mantiksal(bool v, Konum k = {});

    bool tablo_mu() const { return tur == Tur::tablo; }
    bool dizi_mi() const { return tur == Tur::dizi; }
    bool dizgi_mi() const { return tur == Tur::dizgi; }
    bool sayisal_mi() const { return tur == Tur::tamsayi || tur == Tur::sayi; }
    bool mantiksal_mi() const { return tur == Tur::mantiksal; }
    // Sayısal değerin binary64 karşılığı (tamsayı A5-K10 sınırında kayıpsızdır).
    double cift() const;

    // Tablo alanı; yoksa ya da tablo değilse nullptr.
    const Deger* bul(std::string_view anahtar) const;
    Deger* bul(std::string_view anahtar);
    // Noktalı yol ("a.b.c"; dizi indisi yok). Yoksa nullptr.
    const Deger* yol_bul(std::string_view noktali) const;
    Deger* yol_bul(std::string_view noktali);
};

// Değer eşitliği (JSON anlamı): sayılar değerce, tablolar anahtar anahtar; konum yok sayılır.
bool esit(const Deger& a, const Deger& b);

// Anahtar yolu (A5-K93): anahtar ya da dizi indisi dizisi.
struct YolParcasi {
    std::variant<std::string, std::size_t> p;
};

class Yol {
public:
    std::vector<YolParcasi> parcalar;
    Yol() = default;
    Yol ekle(std::string anahtar) const;
    Yol ekle(std::size_t indis) const;
    // A5-K93 sözdizimi: noktayla ayrılmış anahtarlar, `[i]` dizi elemanı, çıplak olamayan
    // anahtar TOML temel dizgisi olarak tırnaklı. Kök boş dizgidir.
    std::string metin() const;
    // Desen eşleşmesi için: anahtarlar aynen, indisler "[]" (ör. "olaylar[].siddet").
    std::string desen_metni() const;
};

// TOML çıplak anahtar mı (A-Za-z0-9_-, boş değil).
bool ciplak_anahtar_mi(std::string_view s);
// `x_` önekli deneysel anahtar mı (A5-K07).
inline bool deneysel_mi(std::string_view s) { return s.size() >= 2 && s[0] == 'x' && s[1] == '_'; }

}  // namespace ida::senaryo
