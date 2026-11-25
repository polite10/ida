// sim/a3 testlerinin ortak düzeni (sim/sensor/test ile aynı biçim): her test `int test_<ad>()`,
// 0 geçti. Çekirdek soketsiz ve sanal saatle (Ortam), sunucu gerçek soketlerle (127.0.0.1, port 0;
// SoketIstemci) sürülür.
#pragma once

#include "cekirdek.hpp"
#include "http.hpp"
#include "ida/a3/katalog.hpp"
#include "ida/a3/sunucu.hpp"
#include "json.hpp"
#include "kodlayici.hpp"
#include "sahte_dunya.hpp"
#include "soket.hpp"

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <functional>
#include <map>
#include <mutex>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace ida::a3::test {

struct Denetci {
    int hata = 0;
    void dogrula_v(const char* dosya, int satir, bool kosul, const std::string& ileti)
    {
        if (!kosul) {
            ++hata;
            if (hata <= 40) {
                std::printf("  KALDI %s:%d: %s\n", dosya, satir, ileti.c_str());
            }
        }
    }
};

#define DOGRULA(d, ...) (d).dogrula_v(__FILE__, __LINE__, __VA_ARGS__)

inline constexpr std::string_view kJeton = "0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef";
inline constexpr std::uint16_t kPort = 47100;  // yalnız soketsiz çekirdekte Host/Origin için

// ---- HTTP ve WebSocket istemci tarafı yardımcıları ----

struct HttpYanit {
    int durum = 0;
    std::vector<std::pair<std::string, std::string>> basliklar;  // ad küçük harf
    std::string govde;
    const std::string* baslik(std::string_view ad) const
    {
        for (const auto& [k, v] : basliklar) {
            if (k == ad) {
                return &v;
            }
        }
        return nullptr;
    }
    std::string kod() const;  // yanit_ret ise hata.kod; değilse boş
    json::Deger json_() const
    {
        json::Deger d;
        json::coz(govde, d);
        return d;
    }
};

// `tampon`'un başındaki tek yanıtı ayırır (Content-Length; 101 gövdesiz). Eksikse false.
// `head`: yanıt bir HEAD isteğinindir; Content-Length olsa da gövde yoktur (RFC 9112 §6.3).
bool http_yanit_ayir(std::string& tampon, HttpYanit& y, bool head = false);
// Yanıt gövdesi dondurulmuş katalogdaki `$defs/<tanim>` şemasına uyuyor mu (şemada olmayan alan
// `additionalProperties: false` ile reddedilir).
bool semaya_uyar(std::string_view tanim, const std::string& govde, std::string* neden = nullptr);

std::string istek(std::string_view yontem, std::string_view hedef, const std::vector<std::string>& basliklar = {},
                  std::string_view govde = {}, bool varsayilan_basliklar = true);
std::string yetki();  // "Authorization: Bearer <kJeton>"
std::string host();   // "Host: 127.0.0.1:<kPort>"

std::string ws_istemci_cercevesi(std::uint8_t opkod, std::string_view yuk, bool fin = true, bool maske = true);

struct WsCerceve {
    std::uint8_t opkod = 0;
    bool fin = true;
    std::string yuk;
};
bool ws_sunucu_cercevesi(std::string& tampon, WsCerceve& c);

struct A3Kare {
    CerceveBasligi baslik;
    std::vector<std::uint8_t> yuk;
};
// İkili iletiyi A3 çerçevelerine ayırır; bozuksa false.
bool a3_ileti_ayir(std::string_view ileti, std::vector<A3Kare>& kareler);

std::string ws_el_sikisma(std::string_view jeton = kJeton, bool v1 = true, std::string_view origin = {});

// ---- Soketsiz çekirdek ortamı ----

struct Ortam {
    explicit Ortam(sahte::SahteAyar ayar = {}, SunucuAyari sa = {});
    ~Ortam();
    Ortam(const Ortam&) = delete;
    Ortam& operator=(const Ortam&) = delete;

    BagId ac();
    void gonder(BagId id, std::string_view veri);
    // Bağlantının bütün çıkışını alır (yazıldı sayar).
    std::string al(BagId id);
    // Sanal saati ilerletir ve çekirdek döngüsünü çalıştırır (tik adımıyla).
    void ilerle(std::chrono::milliseconds ms);
    void dongu();
    // Tek bir HTTP isteği gönderir ve yanıtını alır (yanıt gelmezse durum 0).
    HttpYanit http(BagId id, std::string_view ham);
    HttpYanit http(std::string_view ham);
    // İstemci oturumu alır.
    std::uint32_t istemci_al();
    // WebSocket açar: el sıkışma + merhaba; dönüş bağlantı kimliği, `merhaba` iletisi `merhaba`'ya.
    BagId ws_ac(std::string* merhaba = nullptr);
    // Bağlantının çıkışındaki WebSocket çerçevelerini ayrıştırır (biriktirerek).
    std::vector<WsCerceve> ws_al(BagId id);

    SunucuAyari sunucu_ayari;
    Yayinci yayinci;
    sahte::SahteDunya dunya;
    Cekirdek cekirdek;
    Zaman simdi;
    std::map<BagId, std::string> ws_tampon;
};

// ---- Gerçek soket istemcisi (127.0.0.1) ----

class SoketIstemci {
public:
    SoketIstemci() = default;
    explicit SoketIstemci(std::uint16_t port);
    ~SoketIstemci();
    SoketIstemci(SoketIstemci&& o) noexcept;
    SoketIstemci& operator=(SoketIstemci&& o) noexcept;
    SoketIstemci(const SoketIstemci&) = delete;
    SoketIstemci& operator=(const SoketIstemci&) = delete;

    bool gecerli() const { return t_ != ag::gecersiz(); }
    bool yaz(std::string_view s, int zaman_asimi_ms = 2000);
    // En çok `zaman_asimi_ms` bekleyerek gelenleri `tampon`'a ekler; bağlantı kapandıysa false.
    bool oku(int zaman_asimi_ms);
    // Bir HTTP yanıtı gelene dek okur.
    bool yanit_bekle(HttpYanit& y, int zaman_asimi_ms = 3000, bool head = false);
    bool ws_bekle(WsCerceve& c, int zaman_asimi_ms = 3000);
    void kapat();
    ag::Tutamac tutamac() const { return t_; }

    std::string tampon;
    bool kapandi = false;

private:
    ag::Tutamac t_ = ag::gecersiz();
};

// Sunucuyu sahte dünyayla 127.0.0.1:0'da başlatır.
struct CanliSunucu {
    explicit CanliSunucu(sahte::SahteAyar ayar = {}, SunucuAyari sa = {});
    ~CanliSunucu();
    std::unique_ptr<Sunucu> sunucu;
    std::unique_ptr<sahte::SahteDunya> dunya;
    struct Vekil;
    std::unique_ptr<Vekil> vekil;
    std::string hata;
    std::vector<std::string> gunluk;
    std::shared_ptr<std::mutex> gunluk_kilidi = std::make_shared<std::mutex>();
    std::string host() const;
    std::string yetki() const;
};

// W4-K24, K29: duvar saatine bağlı eşiklerin ölçüt olup olmadığı yalnız ortak yardımcıdan okunur
// (`sim/kopru/test/zamanlama_olcutu.hpp`; W4-K29: tek kopya). Bu dizinin testleri `ida::test::zamanlama_olcut_mu()`
// ve `ida::test::zamanlama_muafiyeti()` çağırır; burada ikinci bir kural yoktur.

}  // namespace ida::a3::test
