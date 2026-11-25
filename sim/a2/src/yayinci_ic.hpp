// yayinci_ic.hpp — Yayinci'nin iç yapısı: yayın halkası, tam durum kesiti ve kayıt biçimi.
//
// İki yol vardır ve ikisi de dünya iş parçacığını bekletmez:
//   1. Kesit (`Anlik`, üç yuvalı SonDeger): her yayında güncellenen TAM durum (çalıştırma tanımı,
//      ortam, dalga kümesi, malzeme, yaşayan gövdeler ve o adım sınırındaki durumları). Yeni bir
//      oturum yalnız bundan kurulur (A2-K65); dünya o sırada ilerlemese de okunabilir.
//   2. Halka (BaytHalkasi): sıralı kayıt akışı (olay, durum, küme değişimi, yeni çalıştırma,
//      kapanış). Bağlı oturuma yalnız bunlar iletilir.
//
// İkisini bağlayan değişmez: her kayıt bir sıra numarası taşır; dünya ÖNCE o kayıtları kapsayan
// kesiti yayına alır (`Anlik::sira` ≥ o kayıtların sırası), SONRA kayıtları halkaya yazar. G/Ç iş
// parçacığı oturum kurarken kesiti okur ve sırası `Anlik::sira`'dan büyük olmayan kayıtları atar.
// Kesit okunmadan önce halkadan çekilmiş (ve oturum olmadığı için atılmış) her kaydın sırası, o
// anda yayında olan kesitin sırasından büyük olamaz; öyleyse kesitten sonraki hiçbir doğum, ölüm
// ya da küme değişimi kaybolmaz ve hiçbiri iki kez uygulanmaz.
#pragma once

#include "halka.hpp"
#include "ida/a2/yayinci.hpp"
#include "son_deger.hpp"

#include <array>
#include <atomic>
#include <cstdint>

namespace ida::a2::ic {

// ---- Halka kayıtları ----
enum class KayitTuru : std::uint16_t {
    olay = 1,             // yük: SahneOlayi
    durum = 2,            // yük: Durum + govde_sayisi × GovdeDurumu (gonderim_duvar_ns = 0)
    dalga = 3,            // yük: DalgaKumesi (özet boş) + bilesen_sayisi × DalgaBileseni
    ortam = 4,            // yük: Ortam
    malzeme = 5,          // yük: MalzemeOptik + girdi_sayisi × MalzemeOptikGirdisi
    yeni_calistirma = 6,  // yük yok: oturum veda(yeni_calistirma) ile kapanır
    kapanis = 7,          // yük yok: veda(sim_kapaniyor)
};

inline constexpr std::uint16_t kKayitKorunan = 1u << 0;  // durum: seyreltilmez, atılmaz

struct KayitBasligi {      // 16 B
    std::uint64_t sira;
    std::uint16_t tur;     // KayitTuru
    std::uint16_t bayrak;
    std::uint32_t dolgu;
};
static_assert(sizeof(KayitBasligi) == 16);

// ---- Tam durum kesiti ----
struct Anlik {
    std::uint64_t sira = 0;            // bu kesitin kapsadığı son kayıt sırası
    bool tanimli = false;              // çalıştırma tanımı var mı (yoksa el sıkışma bekler)
    std::uint8_t calisma_durumu = static_cast<std::uint8_t>(CalismaDurumu::durdu);
    std::uint8_t sim_kipi = static_cast<std::uint8_t>(SimKipi::sil);
    std::uint8_t kademe_3b = static_cast<std::uint8_t>(Kademe::tam);
    std::uint32_t nesil = 1;
    std::uint64_t adim_no = 0;
    // Çalıştırma tanımı (yuva etiketi: tanim_surumu)
    std::uint64_t tanim_surumu = 0;
    std::uint8_t manifest_sha256[32] = {};
    char manifest_yolu[512] = {};
    // Kümeler (yuva etiketleri: ortam.ortam_surumu, dalga.surum, malzeme.surum)
    Ortam ortam{};
    DalgaKumesi dalga{};
    MalzemeOptik malzeme{};
    std::uint16_t govde_sayisi = 0;
    std::array<std::uint16_t, kAzamiGovde> varlik{};
    std::array<GovdeDurumu, kAzamiGovde> govde{};
    std::array<DalgaBileseni, kAzamiDalgaBileseni> bilesen{};
    std::array<MalzemeOptikGirdisi, kAzamiMalzemeOptik> girdi{};
};

inline constexpr std::size_t kBekleyenOlay = 192;  // bir adım sınırında tutulabilen olay

}  // namespace ida::a2::ic

namespace ida::a2 {

struct Yayinci::Ic {
    Ic(std::size_t halka_bayt, bool etkin_);

    const bool etkin;
    ic::BaytHalkasi halka;
    ic::SonDeger<ic::Anlik> anlik;
    std::size_t pay = 0;  // atılabilir kayıtların halkada bırakması gereken boşluk

    // ---- G/Ç iş parçacığının okuduğu sayaçlar ----
    std::atomic<std::uint64_t> yazilan_kayit{0};
    std::atomic<std::uint64_t> dusen_durum{0};
    std::atomic<std::uint64_t> kayip_korunan{0};
    std::atomic<std::uint64_t> kayip_sira{0};   // kaybolan son korunan kaydın sırası
    std::atomic<std::uint64_t> olay_tasmasi{0};
    std::atomic<std::uint64_t> gecersiz_cagri{0};
    std::atomic<std::uint64_t> eksik_govde{0};
    std::atomic<std::uint32_t> nesil_a{1};

    // ---- Yalnız dünya iş parçacığı ----
    struct Govde {
        std::uint32_t kimlik = 0;
        std::uint32_t sureklilik = 0;
        std::uint16_t varlik = 0;
        std::uint8_t tur = 0;
        std::uint8_t tupler = 0;
        bool kirildi = false;  // bu adım sınırında süreklilik numarası arttı (ya da gövde doğdu)
    };
    std::array<Govde, kAzamiGovde> tablo{};
    std::uint16_t n = 0;

    bool calistirma_var = false;
    std::uint32_t nesil = 1;
    std::uint8_t calisma_durumu = static_cast<std::uint8_t>(CalismaDurumu::durdu);
    bool adim_var = false;          // `adim_no` sınırının durumu yayınlandı
    std::uint64_t adim_no = 0;      // son yayınlanan (ya da sıfırlamanın verdiği) adım sınırı
    std::uint64_t asgari_adim = 0;  // olay ve adim_siniri için kabul edilen en küçük adım
    bool onceki_gecerli = false;    // yayındaki kesit `adim_no` sınırının durumunu taşıyor

    // Asıl kopyalar (kesit yuvaları bunlardan, etiket farklıysa yenilenir)
    std::uint64_t tanim_surumu = 0;
    std::uint8_t sim_kipi = static_cast<std::uint8_t>(SimKipi::sil);
    std::uint8_t kademe_3b = static_cast<std::uint8_t>(Kademe::tam);
    std::uint8_t manifest_sha256[32] = {};
    char manifest_yolu[512] = {};
    Ortam ortam{};
    DalgaKumesi dalga{};
    MalzemeOptik malzeme{};
    std::array<DalgaBileseni, kAzamiDalgaBileseni> bilesen{};
    std::array<MalzemeOptikGirdisi, kAzamiMalzemeOptik> girdi{};
    bool dalga_bekliyor = false;    // değişim kaydı henüz halkaya yazılmadı
    bool ortam_bekliyor = false;
    bool malzeme_bekliyor = false;

    // Bu adım sınırının bekleyen olayları
    std::array<SahneOlayi, ic::kBekleyenOlay> bekleyen{};
    std::uint16_t bekleyen_n = 0;
    bool bekleyen_tasti = false;
    bool sinir = false;             // bu sınırda sahne değişti (A2-K21: T − h ve T iletileri gider)
    std::uint64_t bekleyen_azami_adim = 0;

    std::uint64_t sira = 1;         // sıradaki kayıt sırası

    // Yalnız sınamalar: bir yayının ara noktalarında (kesit yayına alındıktan ve her halka kaydından
    // sonra) dünya iş parçacığından çağrılır; sınama o noktada G/Ç tarafını çalıştırıp kesit–halka
    // değişmezini her ara dizilişte belirlenimci olarak dener. Üründe boştur (tek bir dal).
    void (*sinama_kancasi)(void*) = nullptr;
    void* sinama_baglami = nullptr;
};

}  // namespace ida::a2
