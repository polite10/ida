// http.hpp — HTTP/1.1 istek başlığı ayrıştırıcısı (RFC 9112) ve yanıt yazıcısı; WebSocket
// çerçeve okuyucusu ve yazıcısı (RFC 6455 §5). Yalnız sunucu tarafı; sınırlı ve katı.
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace ida::a3::http {

inline constexpr std::size_t kAzamiBaslikBlogu = 16 * 1024;  // istek satırı + başlıklar
inline constexpr std::size_t kAzamiBaslikSayisi = 100;

struct Istek {
    std::string yontem;
    std::string hedef;  // istek hedefi (origin-form)
    std::string yol;    // hedefin sorgudan önceki kısmı
    std::string sorgu;  // '?' sonrası (yoksa boş)
    int kucuk_surum = 1;  // HTTP/1.<kucuk_surum>
    std::vector<std::pair<std::string, std::string>> basliklar;  // ad küçük harfli

    // Adla ilk başlık; yoksa nullptr.
    const std::string* baslik(std::string_view ad) const noexcept;
    std::size_t baslik_sayisi(std::string_view ad) const noexcept;
};

enum class Sonuc : std::uint8_t { eksik, tamam, hata };

// `tampon`'un başında tam bir başlık bloğu varsa ayrıştırır; `blok_boyu` başlık bloğunun (son
// CRLFCRLF dahil) boyu. Blok sınırı aşılmışsa ya da biçim bozuksa `hata`.
Sonuc baslik_ayristir(std::string_view tampon, Istek& istek, std::size_t& blok_boyu, std::string& neden);

// Gövde çerçevelemesi (RFC 9112 §6): hiç, Content-Length ya da chunked.
struct GovdeBilgisi {
    enum class Tur : std::uint8_t { yok, uzunluk, parcali } tur = Tur::yok;
    std::uint64_t uzunluk = 0;
};
// Çelişkili ya da desteklenmeyen çerçeveleme `hata` (ör. Content-Length + Transfer-Encoding).
bool govde_bilgisi(const Istek& istek, GovdeBilgisi& g, std::string& neden);

// chunked gövdenin artımlı çözücüsü. `besle` tüketilen bayt sayısını döndürür; `bitti` olunca
// gövde tamamdır. Çözülen bayt sayısı `sinir`'i aşarsa `asildi` kurulur ve çözüm durur.
class ParcaliCozucu {
public:
    explicit ParcaliCozucu(std::size_t sinir) : sinir_(sinir) {}
    std::size_t besle(std::string_view girdi, std::string& govde);
    bool bitti() const noexcept { return asama_ == Asama::bitti; }
    bool hata() const noexcept { return asama_ == Asama::hata; }
    bool asildi() const noexcept { return asildi_; }

private:
    enum class Asama : std::uint8_t { boy, veri, veri_crlf, trailer, bitti, hata };
    Asama asama_ = Asama::boy;
    std::size_t sinir_;
    std::uint64_t kalan_ = 0;
    std::string satir_;
    bool asildi_ = false;
};

std::string durum_metni(int durum);

// Yanıt başlığı + gövde. `Cache-Control: no-store` ve `X-Content-Type-Options: nosniff` her
// yanıttadır; `Access-Control-Allow-*` hiçbir yanıtta yoktur (A3-K10).
// `govdesiz`: HEAD isteğinin yanıtı — başlıklar (Content-Length dahil) yazılır, gövde yazılmaz
// (RFC 9110 §9.3.2, RFC 9112 §6.3).
std::string yanit(int durum, std::string_view icerik_turu, std::string_view govde, bool kapat,
                  const std::vector<std::pair<std::string, std::string>>& ek = {}, bool govdesiz = false);

// Başlık değerindeki virgülle ayrılmış öğeler (boşlukları kırpılmış).
std::vector<std::string> liste(std::string_view deger);
bool token_esit(std::string_view a, std::string_view b) noexcept;  // ASCII büyük/küçük harf duyarsız

}  // namespace ida::a3::http

namespace ida::a3::ws {

enum class Opkod : std::uint8_t { devam = 0x0, metin = 0x1, ikili = 0x2, kapat = 0x8, ping = 0x9, pong = 0xA };

struct Cerceve {
    bool fin = true;
    std::uint8_t opkod = 0;
    std::string yuk;
};

enum class Okuma : std::uint8_t { eksik, cerceve, hata };

// İstemci → sunucu çerçevesi (maskeli olmalı). `azami_yuk` bu çerçevenin taşıyabileceği en
// büyük yük (iletinin kalan payı); aşılırsa hata ve kod 1009. Diğer protokol hataları 1002.
Okuma oku(const std::uint8_t* p, std::size_t n, std::size_t& tuketilen, Cerceve& c, std::uint16_t& kapanis_kodu,
          std::size_t azami_yuk);

// Sunucu → istemci çerçevesi (maskesiz, FIN=1).
void yaz(Opkod op, const std::uint8_t* yuk, std::size_t n, std::string& cikti);
void kapanis_yaz(std::uint16_t kod, std::string_view neden, std::string& cikti);

}  // namespace ida::a3::ws
