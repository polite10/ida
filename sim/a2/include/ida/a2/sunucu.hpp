// ida/a2/sunucu.hpp — sim tarafının A2 sunucusu (docs/sozlesmeler/A2-gorsellestirici.md; W5-a2-sunucu).
//
// Yalnız 127.0.0.1'de tek TCP dinleyicisi (A2-K01), tek görselleştirici oturumu (A2-K03). Kendi
// G/Ç iş parçacığı vardır; dünya veriyi `yayinci()` ile verir (yayinci.hpp) ve hiçbir koşulda
// beklemez (A2-K17, K26, K47). Dinleyiciden önce sahiplik kilidi alınır (A2-K77); kilit ya da port
// alınamazsa sunucu "A2'siz" kalır (A2-K05, K79): nesne yaşar, yayın işlevleri hemen döner, durum
// `durum()` ile sorgulanır.
//
// Bu dilimde kamera hattı yoktur: her oturumda `kamera_sayisi = 0` bildirilir; `kamera_istegi`,
// `kamera_iptal`, `halka_yenile`, `govde_tablosu` ve `atlama` üretilmez (W5-K30).
#pragma once

#include "ida/a2/yayinci.hpp"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <string_view>

namespace ida::a2 {

struct SunucuAyari {
    // A2-K93: 1024 … 65 535; aralık dışı değerle sunucu başlamaz.
    std::uint16_t port = kVarsayilanPort;
    // Yalnız sınamalar: true ise `port` yok sayılır ve işletim sisteminin verdiği boş port kullanılır.
    bool sinama_bos_port = false;
    // Yalnız sınamalar: sahiplik kilidinin yolu. Boşsa sözleşmedeki sabit yer kullanılır
    // (kullanıcının geçici dizini / ida / a2.kilit; A2-K77, K93).
    std::string kilit_dosyasi;
    // Dünya → A2 yayın halkası (bayt).
    std::size_t yayin_halkasi_bayt = std::size_t{4} << 20u;
    // A2-K26: bağlantı başına gönderme kuyruğu (bayt). Sınamalar küçültebilir.
    std::size_t gonderme_kuyrugu_bayt = static_cast<std::size_t>(kGondermeKuyruguAzamiBayt);
    // Duvar saati (ns; A2 §2). Boşsa işletim sisteminin tekdüze saati; sınamalar saat enjekte eder.
    std::function<std::int64_t()> saat;
    // Sunucunun günlüğü (boşsa yazılmaz).
    std::function<void(std::string_view)> gunluk;
};

enum class A2Durumu : std::uint8_t {
    dinliyor = 0,
    kilit_baskasinda = 1,   // A2-K79: sahiplik kilidi başka bir sim sürecinde
    kilit_acilamadi = 2,    // kilit dosyası açılamadı ya da oluşturulamadı
    port_acilamadi = 3,     // A2-K05: dinleyici açılamadı (port kullanımda)
    ic_hata = 4,            // G/Ç iş parçacığı başlatılamadı
    durduruldu = 5,         // durdur() ya da kapaniyor() sonrası
};
const char* a2_durumu_adi(A2Durumu d) noexcept;

// Son oturumun bitiş nedeni (sayaç ve tanılama; tel değeri değildir).
enum class KopmaNedeni : std::uint8_t {
    yok = 0,
    karsi_kapatti,        // soket kapandı ya da hata verdi (çökme, öldürme)
    karsi_veda,           // görselleştirici veda gönderdi
    protokol_hatasi,      // veda(protokol_hatasi) (A2-K06, K07)
    protokol_reddi,       // ret(protokol): merhaba alanı geçersiz (§3.3)
    surum_reddi,          // ret(surum) (A2-K11)
    manifest_reddi,       // ret(manifest) (A2-K14)
    halka_reddi,          // ret(halka) (A2-K14)
    merhaba_zaman_asimi,  // A2-K10
    hazir_zaman_asimi,    // ret(zaman_asimi) (A2-K15)
    nabiz_zaman_asimi,    // veda(nabiz_zaman_asimi) (A2-K62)
    tampon_tasti,         // veda(tampon_tasti): gönderme kuyruğu (A2-K26)
    yayin_tasmasi,        // veda(tampon_tasti): dünya → A2 halkasında atılamayan kayıt kayboldu
    yeni_calistirma,      // veda(yeni_calistirma) (A2-K96)
    sim_kapaniyor,        // veda(sim_kapaniyor) (A2-K67)
};
const char* kopma_nedeni_adi(KopmaNedeni n) noexcept;

// A2 sayaçları (§5 son paragrafının kamera dışı kısmı). G/Ç iş parçacığında güncellenir; okuma
// yaklaşık anlıktır. A3/A6'ya taşınma biçimi sonraki dilimdedir.
struct SunucuSayaclari {
    std::uint64_t baglanti_sayisi = 0;       // kabul edilip oturum açılan bağlantı
    std::uint64_t mesgul_reddi = 0;          // ret(mesgul) (A2-K03)
    std::uint64_t biten_oturum = 0;
    KopmaNedeni son_kopma_nedeni = KopmaNedeni::yok;
    std::uint32_t son_karsi_veda_nedeni = 0xFFFFFFFFu;  // görselleştiricinin son veda nedeni (VedaNedeni); yoksa 0xFFFFFFFF
    std::uint64_t protokol_hatasi = 0;       // A2-K06, K07 (ret(protokol) dahil)
    std::uint64_t seyreltilen_durum = 0;     // A2-K25: SIL duvar saati sınırı
    std::uint64_t kuyrukta_atilan_durum = 0; // A2-K26: kuyruk dolu
    std::uint64_t kuyruk_tasmasi = 0;        // veda(tampon_tasti) sayısı (kuyruk)
    std::uint64_t yayin_tasmasi_kapanis = 0; // veda(tampon_tasti) sayısı (yayın halkası)
    std::uint64_t gonderilen_ileti = 0;
    std::uint64_t gonderilen_durum = 0;
    std::uint64_t gonderilen_olay = 0;
    std::uint64_t gonderilen_nabiz = 0;
    std::uint64_t alinan_ileti = 0;
    std::uint64_t alinan_nabiz = 0;
    std::uint64_t azami_kuyruk_bayt = 0;     // gönderme kuyruğunda görülen en büyük doluluk
    std::uint64_t tam_durum_bekleme = 0;     // merhaba geldi ama çalıştırma tanımı yoktu (tik sayısı)
    // Oturum bilgisi (tanılama)
    bool oturum_var = false;
    bool hazir_alindi = false;
    std::uint64_t oturum_kimligi = 0;
    std::uint16_t oturum_surum_alt = 0;
    std::int64_t son_uygulanan_t_ns = 0;     // son goruntuleyici_nabzi'ndan
    std::uint32_t cizim_hizi_mhz = 0;
    YayinSayaclari yayin;                    // dünya tarafı (yayın taşması)
};

class Sunucu {
public:
    struct Ic;
    // Kilidi almayı ve dinlemeyi dener, G/Ç iş parçacığını açar. Geçersiz ayarda (port aralığı)
    // nullptr döner ve `hata` dolar (A2-K93: "geçersiz değerle sim başlamaz"). Kilit ya da port
    // alınamazsa nesne yine döner; `durum()` nedeni söyler ve sunucu A2'siz çalışır.
    static std::unique_ptr<Sunucu> baslat(const SunucuAyari& ayar, std::string& hata);
    ~Sunucu();
    Sunucu(const Sunucu&) = delete;
    Sunucu& operator=(const Sunucu&) = delete;

    // Olağan kapanış: bağlı oturuma veda(sim_kapaniyor) (A2-K67), dinleyici kapanır, iş parçacığı
    // durur. Kilit nesne yok edilene kadar tutulur (A2-K77: süreç ömrü).
    void durdur();

    A2Durumu durum() const noexcept;
    // İnsan için açıklama (ör. "A2 sahiplik kilidi başka bir sim sürecinde").
    std::string durum_aciklamasi() const;
    std::uint16_t port() const noexcept;     // dinlenen port; A2'sizken 0
    Yayinci& yayinci() noexcept;
    SunucuSayaclari sayaclar() const;

private:
    explicit Sunucu(std::unique_ptr<Ic> ic);
    std::unique_ptr<Ic> ic_;
};

}  // namespace ida::a2
