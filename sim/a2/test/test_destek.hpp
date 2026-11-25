// sim/a2 testlerinin ortak düzeni: her test `int test_<ad>()`, 0 geçti (sim/a3/test ile aynı biçim).
//
// Karşı uç gerçek istemcidir (W5-K29): `viz/a2`'nin kodeği (`ida::viz::coz_*`, `kodla_*`,
// `CerceveOkuyucu`) ve istemci oturumu (`ida::viz::IstemciOturumu`). İki bağımsız gerçekleştirimin
// birbirini sınaması uygunluk kanıtıdır.
//   Duzenek        soketsiz: Yayinci + Cekirdek + istemci oturumu, SANAL saat (5 s, 120 s, 3 s ve
//                  1 s süreleri gerçek zaman beklemeden sınanır).
//   SunucuDuzenegi gerçek Sunucu (127.0.0.1, işletim sisteminin verdiği port; kilit dosyası geçici
//                  dizinde) ve SoketIstemci.
#pragma once

#include "cekirdek.hpp"
#include "ida/a2/sunucu.hpp"
#include "ida/viz/kodek.hpp"
#include "ida/viz/oturum.hpp"
#include "sahte_dunya.hpp"
#include "soket.hpp"

#include <array>
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace ida::a2::test {

namespace viz = ida::viz;

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

// Sınamalarda "doğru" manifest özeti (içeriği önemsiz; sim ile istemci aynı değeri kullanır).
std::array<std::uint8_t, 32> ornek_ozet(std::uint8_t tohum = 0x5A);

// Sunucunun gönderdiği bir çerçeve (viz kodeğinin çerçeveleyicisiyle ayrılmış).
struct Cerceve {
    Baslik b{};
    std::vector<std::uint8_t> yuk;
    IletiTuru tur() const { return static_cast<IletiTuru>(b.tur); }
};

// Sunucu çıkışını çerçevelere ayırır ve saklar.
struct Kaydedici {
    viz::CerceveOkuyucu okuyucu{viz::Yon::sim_gorsel};
    std::vector<Cerceve> cerceveler;
    bool cerceve_hatasi = false;   // sunucu bozuk çerçeve üretti
    std::uint64_t bayt = 0;
    bool sakla = true;             // false: yalnız sayar (uzun koşularda bellek için)
    std::uint64_t sayi = 0;
    std::function<void(const Cerceve&)> gozlemci;

    void besle(const std::uint8_t* p, std::size_t n);
    std::size_t say(IletiTuru t) const;
    const Cerceve* son(IletiTuru t) const;
    const Cerceve* ilk(IletiTuru t) const;
    // Son çerçeve veda(neden) mi / ret(neden) mi.
    bool son_veda(VedaNedeni n) const;
    bool son_ret(RetNedeni n) const;
};

viz::DurumIleti durum_coz(const Cerceve& c);
SahneOlayi olay_coz(const Cerceve& c);
viz::HosgeldinIleti hosgeldin_coz(const Cerceve& c);

// ---- Soketsiz düzenek ----
struct Duzenek {
    explicit Duzenek(std::size_t kuyruk_siniri = static_cast<std::size_t>(kGondermeKuyruguAzamiBayt),
                     std::size_t halka_bayt = std::size_t{4} << 20u);

    Yayinci y;
    ic::Cekirdek c;
    viz::IstemciOturumu ist;
    Kaydedici k;
    std::int64_t simdi = 1'000'000'000;
    bool istemci_bagli = false;
    bool ham_bagli = false;          // çekirdekte oturum var ama istemci oturumu yok (ham bayt sınamaları)
    bool otomatik_hazir = true;
    viz::Ozet hazir_ozeti{};
    HazirDurumu hazir_durumu = HazirDurumu::basarili;
    std::uint64_t kimlik_sayaci = 0xA200000000000000ull;
    std::vector<viz::Olay> olaylar;
    std::size_t istemci_okuma_siniri = 0;   // 0: sınırsız; > 0: tur başına istemcinin okuduğu en çok bayt
    std::vector<std::uint8_t> yolda;         // sunucudan çıkmış, istemcinin henüz okumadığı baytlar
    bool sunucu_kapatti = false;

    // Gerçek istemciyi bağlar ve bir tur pompalar.
    bool baglan();
    // Yalnız çekirdekte oturum açar (ham bayt sınamaları).
    bool ham_baglan();
    void ham_gonder(const viz::Baytlar& b);
    // Bir tur: istemci → sunucu, sunucu tiki, sunucu → istemci, istemci olayları (otomatik hazir).
    void pompala();
    // Sanal saati `ns` ilerletir; her `dilim_ns`'de bir pompalar ve istemcinin zamanını işler.
    void ilerlet(std::int64_t ns, std::int64_t dilim_ns = 100'000'000, bool istemci_zamani = true);
    // İstemci çöktü: soket kapanır.
    void istemciyi_kopar();
    bool istemci_hatasiz() const;   // istemci hiçbir protokol hatası bildirmedi
    std::string istemci_son_nedeni() const { return ist.son_kapanis_nedeni(); }

private:
    void sunucudan_al_();
    void istemciye_ver_();
};

sahte::SahteAyar ornek_ayar();

// ---- Gerçek soketlerle ----
struct SoketIstemci {
    explicit SoketIstemci(std::uint16_t port);
    ~SoketIstemci();
    SoketIstemci(const SoketIstemci&) = delete;
    SoketIstemci& operator=(const SoketIstemci&) = delete;

    ag::Tutamac t;
    viz::IstemciOturumu ist;
    Kaydedici k;
    bool bagli = false;
    bool oku = true;                 // false: soketten okumaz (donuk/yavaş istemci)
    bool otomatik_hazir = true;
    viz::Ozet hazir_ozeti{};
    bool karsi_kapatti = false;
    std::function<std::int64_t()> saat;

    // Oturumu başlatır (merhaba). `oturum = false`: yalnız bağlanır (ham sınamalar).
    void basla();
    // Bir tur: oku, istemci oturumunu işlet, yaz. Dönüş: bir şey okundu ya da yazıldı.
    bool pompala();
    // `kosul` sağlanana kadar pompalar (en çok `ms` gerçek zaman). Dönüş: koşul sağlandı.
    bool bekle(const std::function<bool()>& kosul, int ms = 5000);
    void ham_yaz(const std::uint8_t* p, std::size_t n);
    void kapat();
};

struct SunucuDuzenegi {
    explicit SunucuDuzenegi(std::function<void(SunucuAyari&)> ayarla = nullptr);
    ~SunucuDuzenegi();

    std::string dizin;               // geçici dizin (kilit dosyası burada)
    std::string kilit;
    std::atomic<std::int64_t> saat_kaydirma{0};   // sunucunun saatine eklenen (ns)
    std::unique_ptr<Sunucu> s;
    std::string hata;
    std::int64_t saat() const;
};

// Tekil geçici dizin (kilit dosyaları için).
std::string gecici_dizin(const char* ad);
void gercek_bekle_ms(int ms);
// `kosul` sağlanana kadar bekler (1 ms aralıkla yoklar).
bool kosul_bekle(const std::function<bool()>& kosul, int ms = 5000);

}  // namespace ida::a2::test
