// ida/a3/komut.hpp — komut alıcısı tarafı: A3 sunucusunun komutları ve anlık durum okumasını
// sim'e ilettiği soyut arayüz (W4-a3 kapsam 6–8).
//
// Sunucu isteği erişim, biçim (şema), yineleme ve onay jetonu bakımından denetler, komut
// numarasını verir ve komutu bu arayüze iletir. Hangi komutun uygulanacağına sim karar verir
// (A1-K85, A3-K95): durum makinesi (Tablo 4.7a/4.7b), `beklenen_*` karşılaştırmaları, kısıtlayan
// ve gevşeten emniyet işlemlerinin adım sınırındaki sırası alıcının sorumluluğundadır.
//
// İş parçacığı: `anlik` ve `al` sunucunun G/Ç iş parçacığından çağrılır; dünya iş parçacığını
// bekletmemeli ve kısa sürmelidir (A3-K38: A3 dünyayı bekletmez). Komutların nihai sonucu dünya
// tarafından `Yayinci::komut_sonucu` ile verilir.
#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace ida::a3 {

enum class CalistirmaDurumu : std::uint8_t {
    bos = 0, yuklendi = 1, hazirlaniyor = 2, kosuyor = 3, duraklatildi = 4, bitti = 5, durduruldu = 6, hata = 7,
    izleme = 8
};

enum class ZamanKipi : std::uint8_t {
    yok = 0, sil = 1, hil_1x = 2, hil_bariyer = 3, tekrar_fizik = 4, tekrar_girdi_sil = 5, tekrar_yeniden_kurma = 6
};

// GET /api/v1/durum, GET /api/v1/emniyet, `merhaba` ve onay jetonu bağları için anlık görüntü.
struct AnlikDurum {
    CalistirmaDurumu durum = CalistirmaDurumu::bos;
    ZamanKipi kip = ZamanKipi::yok;
    std::optional<std::string> run_id;  // A3-K124: yoksa null
    std::int64_t sim_t_ns = 0;
    std::uint64_t adim_no = 0;
    bool gecerli = true;
    bool gercek_zamani_kacirdi = false;
    std::uint32_t gecerlilik_nedenleri = 0;     // Tablo 4.8 bitleri
    std::uint8_t kademe_gorunum = 1;            // 0 kapali, 1 sade, 2 tam
    std::uint8_t kademe_kamera = 0;
    std::uint8_t kademe_sonar = 1;
    std::uint8_t kademe_kayit = 1;              // 0 olaylar, 1 telemetri, 2 tam
    std::optional<std::string> senaryo_ozeti;   // "sha256:<64 hex>"
    std::uint32_t a1_run_id = 0;                // 0: null
    std::uint64_t durum_rev = 1;
    // Sanal emniyet girdileri (A3-K89; yetkili durum yalnız sim'dedir: A3-K90)
    std::uint32_t emniyet_rev = 1;
    bool emniyet_rev_doydu = false;
    std::uint8_t acil_durdurma = 0;             // 0 serbest, 1 basili
    std::array<std::uint8_t, 2> silahlandirma_fisi{0, 0};  // 0 cikarilmis, 1 takili; dizin 0 = f1
    std::array<std::uint8_t, 2> emniyet_pimi{1, 1};        // 0 cikarilmis, 1 takili
    std::optional<std::uint64_t> son_degisim_adimi;
    std::vector<std::pair<std::string, std::int64_t>> sayaclar;  // yanit_durum.sayaclar
};

enum class KomutTuru : std::uint8_t {
    calistirma_baslat,
    calistirma_duraklat,
    calistirma_surdur,
    calistirma_adimla,
    calistirma_hiz,
    calistirma_durdur,
    calistirma_sifirla,
    senaryo_yukle,       // A3-K77 (W5-a3-senaryo): çalıştırma denetimi gibi `beklenen_*` taşır
    emniyet_degistir,
    yer_istasyonu_komut,
};

enum class EmniyetGirdisi : std::uint8_t { acil_durdurma, silahlandirma_fisi, emniyet_pimi };
enum class GirdiDurumu : std::uint8_t { serbest, basili, takili, cikarilmis };
// A1 komut kataloğunun sınıfı (protocol/komutlar.json `sinif`).
enum class KomutSinifi : std::uint8_t { daraltici, genisletici, notr };

struct Komut {
    std::uint32_t komut_no = 0;
    std::uint32_t istemci_id = 0;
    KomutTuru tur = KomutTuru::calistirma_baslat;
    // A3-K54, K124: null → nullopt
    std::optional<std::string> beklenen_run_id;
    // A3-K123: çalıştırma kontrolü komutlarında zorunlu; diğerlerinde 0
    std::uint64_t beklenen_durum_rev = 0;
    // EMNİYET: emniyeti artıran sanal girdi değişikliği (A3-K92) ya da daraltıcı yer istasyonu
    // komutu (A3-K104). Sunucu bunu isteğin biçiminden çıkarır (hedef durum / katalog sınıfı);
    // kısıtlayan komut kuyruk doluluğu ya da revizyon yüzünden reddedilmez (A3-K59, K119).
    bool kisitlayan = false;
    // EMNİYET — W4-K19: kısıtlayan istekte `beklenen_run_id` güncel çalıştırmayla uyuşmuyor (ya da
    // istekte yok). Kısıtlayan istek yine de alıcıya iletilir ve uygulanır; A3-K54 yalnız
    // gevşetenlere uygulanır. Bilgi içindir; sunucu uyuşmazlığı olayla kaydetmiştir.
    bool run_id_uyusmazligi = false;

    // calistirma/baslat
    std::optional<std::string> kayit_kademesi;   // olaylar | telemetri | tam
    std::optional<std::string> kamera_kareleri;  // kapali | pencereli | surekli
    // calistirma/duraklat
    bool gecersizligi_onayla = false;
    // calistirma/adimla
    std::uint32_t periyot_sayisi = 0;
    // calistirma/hiz
    double carpan = 0.0;
    // senaryo/yukle: `SenaryoHizmeti::hazirla`nın verdiği aday (senaryo içeriği dünyaya taşınmaz).
    std::uint64_t senaryo_aday = 0;

    // emniyet/degistir (A3-K89…K95)
    EmniyetGirdisi girdi = EmniyetGirdisi::acil_durdurma;
    std::int8_t tup = -1;  // fiş ve pimde 0 (f1) ya da 1 (f2); acil durdurmada -1
    GirdiDurumu hedef_durum = GirdiDurumu::basili;
    // Gevşeten emniyet ve genişletici yer istasyonu komutunda zorunlu; kısıtlayanda taşınmaz
    // (A3-K119: varsa yok sayılır).
    std::optional<std::uint32_t> beklenen_emniyet_rev;

    // yer_istasyonu/komut (A3-K100…K105): katalogdaki biçimiyle taşınır.
    std::string yi_komut;
    KomutSinifi yi_sinif = KomutSinifi::notr;
    std::string yi_parametreler_json;  // kanonik JSON (anahtarlar sıralı, boşluksuz)
};

struct AlimSonucu {
    enum class Tur : std::uint8_t {
        kuyrukta,  // dünyanın kuyruğuna alındı; nihai sonuç komut_sonucu olayıyla gelir
        ret,       // eşzamanlı denetimde reddedildi: hata_kodu A3 §3.9 kodlarından
        dolu,      // dünyanın komut kuyruğu dolu (A3-K59)
    };
    Tur tur = Tur::ret;
    std::string hata_kodu;
    std::string ileti;
    std::string ayrinti_json;  // boş ya da JSON nesnesi (ör. {"guncel_durum_rev":7})
};

class KomutAlicisi {
public:
    virtual ~KomutAlicisi() = default;
    virtual AnlikDurum anlik() = 0;
    virtual AlimSonucu al(const Komut& k) = 0;
};

}  // namespace ida::a3
