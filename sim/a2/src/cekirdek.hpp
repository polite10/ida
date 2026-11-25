// cekirdek.hpp — A2 sunucusunun soketten bağımsız çekirdeği: oturum durum makinesi, el sıkışma,
// yayın halkasının oturuma aktarılması, takvimli durumun seyreltilmesi, nabız ve zaman aşımları.
//
// Yalnız G/Ç iş parçacığında çağrılır (sayaç okuması dışında). Zamanı çağıran verir (duvar saati,
// ns): sınamalar saati enjekte eder; 5 s, 120 s, 3 s ve 1 s süreleri gerçek zaman beklemeden sınanır.
// Soket katmanı (sunucu.cpp): bağlantıyı kabul eder, okuduğunu `veri_geldi`'ye verir, `kuyruk()`'u
// yazar, `kapanis_var()` olunca `kapanis_al()` baytlarını yazıp soketi kapatır.
//
// Oturum aşamaları ve süreler (A2 §4.2, §4.7; W5-K28):
//   merhaba_bekleniyor    kabulden sonra 5 s içinde `merhaba` gelmezse bağlantı kapatılır (A2-K10).
//   tam_durum_bekleniyor  `merhaba` kabul edildi ama çalıştırma tanımı henüz yok: `hosgeldin`
//                         gönderilemez; tanım gelince el sıkışma sürer.
//   hazir_bekleniyor      `hosgeldin` → `dalga_kumesi` → `ortam` → `malzeme_optik` gönderildi, akış
//                         başladı (A2-K12); 120 s içinde `hazir` gelmezse `ret(zaman_asimi)` (A2-K15).
//   akis                  `hazir` kabul edildi.
// `merhaba`'nın alındığı andan itibaren 3 s boyunca hiçbir ileti gelmezse
// `veda(nabiz_zaman_asimi)` (A2-K62; W5-K28: `hazir` penceresinde de). `sim_nabzi` `hosgeldin`'den
// itibaren 1 s aralıkla gider.
#pragma once

#include "ida/a2/sunucu.hpp"
#include "kodlayici.hpp"
#include "kuyruk.hpp"
#include "yayinci_ic.hpp"

#include <atomic>
#include <cstddef>
#include <cstdint>

namespace ida::a2::ic {

enum class Asama : std::uint8_t { yok, merhaba_bekleniyor, tam_durum_bekleniyor, hazir_bekleniyor, akis };

// A2-K25 seyreltmesi: jeton kovası. Sürekli hız 100 ileti/s, kova 20 ileti: herhangi bir 1 s'lik
// duvar saati penceresinde en çok 20 + 100 = 120 takvimli durum geçer. 60 Hz'lik (1x) akış, dünya
// adımları 333 ms'ye kadar yığılsa da seyreltilmez.
inline constexpr std::int64_t kSeyreltmeBirimNs = 10'000'000;                 // ileti başına 10 ms
inline constexpr std::int64_t kSeyreltmeKovaNs = 20 * kSeyreltmeBirimNs;      // 20 ileti
static_assert(1'000'000'000 / kSeyreltmeBirimNs + kSeyreltmeKovaNs / kSeyreltmeBirimNs ==
              static_cast<std::int64_t>(kSilAzamiDurumDuvarSaniye));

// Kapanışta kuyrukta bundan çok gönderilmemiş bayt varsa (istemci okumuyor) son iletiden önceki
// iletiler atılır; son ileti (ret/veda) karşıya öyle ulaşır.
inline constexpr std::size_t kKapanisKalanSiniri = std::size_t{256} << 10u;

class Cekirdek {
public:
    Cekirdek(Yayinci& y, std::size_t kuyruk_siniri);

    // ---- Bağlantı katmanından ----
    // Yeni bağlantı. false: zaten oturum var (çağıran `mesgul_reddi` baytlarını yazıp kapatır).
    bool baglanti_acildi(std::int64_t simdi_ns, std::uint64_t oturum_kimligi);
    void veri_geldi(const std::uint8_t* p, std::size_t n, std::int64_t simdi_ns);
    // Soket kapandı ya da hata verdi (A2-K64): oturum biter, gönderilecek bir şey kalmaz.
    void baglanti_koptu();
    // Zaman aşımları, el sıkışmanın sürdürülmesi, yayın halkasının işlenmesi, nabız.
    void tik(std::int64_t simdi_ns);
    // Sunucu duruyor: bağlı oturuma veda(sim_kapaniyor) (A2-K67).
    void sim_kapaniyor();
    // A2-K03: ikinci bağlantıya gönderilecek ret(mesgul) (o bağlantının ilk iletisi, sıra 0).
    void mesgul_reddi(Baytlar& c);

    bool oturum_var() const noexcept { return asama_ != Asama::yok; }
    Asama asama() const noexcept { return asama_; }
    GondermeKuyrugu& kuyruk() noexcept { return kuyruk_; }
    // Oturum sunucu tarafından bitirildi: kalan baytlar yazılıp soket kapatılmalı.
    bool kapanis_var() const noexcept { return kapanis_; }
    Baytlar kapanis_al();
    // kapaniyor() kaydı işlendi ya da sim_kapaniyor() çağrıldı: yeni bağlantı kabul edilmez.
    bool kapandi() const noexcept { return kapandi_; }

    // Her iş parçacığından (atomik sayaçlardan derlenir).
    SunucuSayaclari sayaclar() const;

private:
    struct Sayac {
        std::atomic<std::uint64_t> baglanti_sayisi{0}, mesgul_reddi{0}, biten_oturum{0}, protokol_hatasi{0},
            seyreltilen_durum{0}, kuyrukta_atilan_durum{0}, kuyruk_tasmasi{0}, yayin_tasmasi_kapanis{0},
            gonderilen_ileti{0}, gonderilen_durum{0}, gonderilen_olay{0}, gonderilen_nabiz{0}, alinan_ileti{0},
            alinan_nabiz{0}, azami_kuyruk_bayt{0}, tam_durum_bekleme{0}, oturum_kimligi{0};
        std::atomic<std::int64_t> son_uygulanan_t_ns{0};
        std::atomic<std::uint32_t> cizim_hizi_mhz{0};
        std::atomic<std::uint32_t> son_karsi_veda{0xFFFFFFFFu};
        std::atomic<std::uint16_t> oturum_surum_alt{0};
        std::atomic<std::uint8_t> son_kopma{0};
        std::atomic<bool> oturum_var{false}, hazir_alindi{false};
    };

    void ileti_isle_(const Baslik& b, const std::uint8_t* yuk, std::int64_t simdi_ns);
    void merhaba_isle_(const std::uint8_t* yuk, std::int64_t simdi_ns);
    void hazir_isle_(const std::uint8_t* yuk);
    bool oturum_baslat_(std::int64_t simdi_ns);
    void halkayi_isle_(std::int64_t simdi_ns);
    void kayit_isle_(const KayitBasligi& kb, const std::uint8_t* yuk, std::size_t n, std::int64_t simdi_ns);
    bool jeton_al_(std::int64_t simdi_ns);
    // `is_`'teki tek iletiyi kuyruğa koyar. false: konmadı (atılabilir ileti düştü ya da oturum bitti).
    bool kuyruga_(bool atilabilir);
    void protokol_hatasi_();
    void ret_(RetNedeni neden, const char* ayrinti, KopmaNedeni kopma);
    void veda_(VedaNedeni neden, KopmaNedeni kopma);
    void tasma_(KopmaNedeni kopma);
    // Oturumu bitirir; son ileti (varsa) sınır denetlenmeden kuyruğa eklenir.
    enum class Son : std::uint8_t { yok, veda, ret };
    void bitir_(KopmaNedeni neden, bool hepsini_at, Son son, std::uint32_t kod, const char* ayrinti);
    bool akista_() const noexcept { return asama_ == Asama::hazir_bekleniyor || asama_ == Asama::akis; }

    Yayinci::Ic& y_;
    GondermeKuyrugu kuyruk_;
    Baytlar is_;               // tek iletilik kodlama tamponu
    Sayac s_;

    Asama asama_ = Asama::yok;
    bool kapanis_ = false;
    bool kapandi_ = false;
    std::uint64_t oturum_kimligi_ = 0;
    std::uint16_t surum_alt_ = 0;
    std::int64_t baglanti_ns_ = 0;
    std::int64_t hosgeldin_ns_ = 0;
    std::int64_t son_alim_ns_ = 0;
    std::int64_t son_nabiz_ns_ = 0;
    std::uint32_t sira_ = 0;           // gönderilecek sıradaki ileti sırası (A2-K07)
    bool ilk_alim_ = true;
    std::uint32_t son_alinan_sira_ = 0;
    GelenOkuyucu okuyucu_;
    std::uint8_t manifest_[32] = {};

    // Akış konumu (kesitten ve iletilen kayıtlardan)
    std::uint64_t anlik_sira_ = 0;     // oturumun kurulduğu kesitin sırası
    std::uint32_t nesil_ = 1;
    std::int64_t t_ns_ = 0;
    std::uint64_t adim_no_ = 0;
    std::uint8_t calisma_durumu_ = static_cast<std::uint8_t>(CalismaDurumu::durdu);
    std::uint8_t sim_kipi_ = static_cast<std::uint8_t>(SimKipi::sil);
    std::uint32_t dalga_surumu_ = 0;
    std::uint32_t ortam_surumu_ = 0;
    std::uint32_t malzeme_surumu_ = 0;
    // Son gönderilen durum (aynı sınırın korunan yinelemesini atlamak için)
    bool sd_var_ = false;
    bool sd_sonrasi_degisim_ = false;
    std::uint32_t sd_nesil_ = 0;
    std::uint64_t sd_adim_ = 0;
    std::uint8_t sd_cd_ = 0;
    // Seyreltme (A2-K25)
    std::int64_t kova_ns_ = 0;
    std::int64_t kova_guncelleme_ns_ = 0;
    std::uint64_t gorulen_dusen_ = 0;  // dünya tarafında halkaya sığmadığı için atılan durum (sıra boşluğu)
};

}  // namespace ida::a2::ic
