// ida/kayit/okuyucu.hpp — A6 kayıt okuyucusu (W5-kayit dilim 1: `izleme` düzeyi için sıralı okuma).
//
// Okuyucu MCAP kitaplığının okuyucusunu KULLANMAZ: dosyayı baştan sona kendi ayrıştırıcısıyla
// tarar, her parçanın sıkıştırmasız verisinin ve her ekin CRC-32'sini kendisi hesaplar (A6-K59) ve
// kapanmamış dosyayı A6'nın kurtarma okumasıyla okur (A6-K60).
//
// Açılışta dosyanın tamamı bir kez taranır (her parça açılır ve doğrulanır); sonuç `DosyaOzeti`'dir:
//   - `kapanmis`: Data End, Footer ve sondaki sihirli baytlar yerinde.
//   - `bozuk`: CRC'si tutmayan, açılamayan ya da çözülemeyen bir parça ya da ek var; A6-K46'yı
//     çiğneyen üst veri var. Bozuk dosyada yalnız ilk bozuk parçadan öncesi okunur ve tekrar
//     güvencesi verilmez.
//   - kapanmamış dosyada yalnız son tam ve CRC'si tutan parçaya kadarki veri okunur; güvence `izleme`.
// Sessiz "sağlam" yoktur: `saglam()` yalnız dosya kapanmış VE bozuk değilse doğrudur.
//
// İletiler `log_time`, eşitlikte `sequence` sırasıyla verilir (A6-K11); dosya sırası farklı olabilir.
// Bellek: açılışta aynı anda bir parça; sıralı okumada zaman aralıkları örtüşen parçalar (üst sınır
// `OkumaAyari::azami_acik_bayt`).
#pragma once

#include "ida/kayit/katalog.hpp"
#include "ida/kayit/ust_veri.hpp"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace ida::kayit {

namespace ic {
struct OkuyucuIc;
}

struct OkumaAyari {
    // Tek parçanın sıkıştırmasız boyu için üst sınır; aşan parça çözülemeyen parça sayılır.
    std::uint64_t azami_parca_bayt = std::uint64_t{64} << 20;
    // Sıralı okumada aynı anda açık tutulan parçaların toplam sıkıştırmasız boyu için üst sınır.
    std::uint64_t azami_acik_bayt = std::uint64_t{256} << 20;
    // Tek üst veri kaydının boyu için üst sınır.
    std::uint64_t azami_ust_veri_bayt = std::uint64_t{16} << 20;
};

// Dosyanın neden okunamadığı (A6 §5: `kayit_okunamadi`).
enum class AcmaHatasi : std::uint8_t {
    yok,
    dosya_acilamadi,
    sihirli_bayt_yok,     // başta sihirli baytlar yok
    baslik_okunamadi,     // Header yok ya da çözülemiyor
    profil_farkli,        // Header `profile` "ida" değil
    buyuk_surum_farkli,   // `a6_surumu` büyük sürümü okuyucununkinden farklı (A6-K48)
};
std::string_view acma_hatasi_adi(AcmaHatasi h) noexcept;

struct KanalOzeti {
    std::string konu;                      // dosyadaki `topic`
    const KanalTanimi* tanim = nullptr;    // katalogdaki kanal; bilinmeyen kanalda nullptr (atlanır)
    std::uint16_t kanal_surumu = 0;        // dosyadaki (kanal `metadata`'sından)
    bool okunur = false;                   // iletileri verilir mi (A6-K48, K49)
    std::uint64_t ileti_sayisi = 0;
    std::string uyari;                     // okunmuyorsa neden
};

struct EkOzeti {
    std::string ad;
    std::string ortam_turu;
    std::uint64_t boyut = 0;
};

struct DosyaOzeti {
    bool kapanmis = false;
    bool bozuk = false;
    std::string bozulma;                   // bozuksa ilk bozulmanın açıklaması
    std::uint64_t bozulma_konumu = 0;      // dosya konumu (bayt)
    std::uint64_t dosya_bayt = 0;
    std::uint64_t kullanilan_bayt = 0;     // okumanın bittiği konum (kurtarma sonu ya da ilk bozuk parça)
    std::string kitaplik;                  // Header `library`

    UstVeri ust_veri;                      // `ida_kayit` + güncellemeler (A6-K46 birleştirmesi)
    bool ust_veri_var = false;             // `ida_kayit` kaydı bulundu
    bool ust_veri_gecerli = false;         // `$defs/ust_veri_ida_kayit`'e uyuyor (zorunlu anahtarlar tam)
    std::optional<UstVeri> kapanis;        // `ida_kayit_kapanis`; kapanmamış dosyada kullanılmaz
    Kademe kademe = Kademe::olaylar;       // üst verideki `kayit_kademesi`; yoksa `olaylar`
    bool kademe_var = false;

    std::vector<KanalOzeti> kanallar;      // dosyadaki kanal kayıtları, kimlik sırasıyla
    std::vector<EkOzeti> ekler;
    std::uint64_t parca_sayisi = 0;        // okunan (doğrulanmış) parça
    std::uint64_t ileti_sayisi = 0;        // okunan parçalardaki ileti (okunmayan kanallar dahil)
    std::uint64_t okunur_ileti_sayisi = 0;
    std::optional<std::int64_t> ilk_log_time;
    std::optional<std::int64_t> son_log_time;

    std::vector<EksikKaydi> eksikler;      // dosyadaki `kayit/eksik` kayıtları, dosya sırasıyla
    // Önek güvencesinin sınırı (A6-K33): düzeyin gerektirdiği kanallardaki ilk eksik adım.
    std::optional<std::uint64_t> ilk_eksik_fizik;
    std::optional<std::uint64_t> ilk_eksik_girdi;

    // Dosyanın verdiği tekrar güvencesi (A6 §4.6, §5): bozuk, kapanmamış ya da üst verisi eksik
    // dosyada en çok `izleme`; yoksa kapanış kaydındaki `tekrar_guvencesi`.
    Guvence guvence = Guvence::izleme;
    std::optional<std::uint64_t> guvence_on_eki_son_adim;

    std::vector<std::string> uyarilar;

    bool saglam() const noexcept { return kapanmis && !bozuk; }
};

struct Ileti {
    const KanalTanimi* kanal = nullptr;
    std::uint16_t kanal_surumu = 0;        // dosyadaki
    std::int64_t log_time = 0;
    std::int64_t publish_time = 0;
    std::uint32_t sequence = 0;
    std::span<const std::uint8_t> veri;    // bir sonraki `sonraki()` çağrısına kadar geçerli
};

class Okuyucu {
public:
    // Dosyayı açar ve tarar. Okunamıyorsa nullptr ve `hata` (A6 §5 ilk satırlar; A6-K48).
    static std::unique_ptr<Okuyucu> ac(const std::filesystem::path& yol, AcmaHatasi& hata,
                                      const OkumaAyari& ayar = {});
    // Aynısı, bellekteki baytlardan (sınamalar). `veri` okuyucu yaşadıkça geçerli kalmalıdır.
    static std::unique_ptr<Okuyucu> ac_bellek(std::span<const std::uint8_t> veri, AcmaHatasi& hata,
                                             const OkumaAyari& ayar = {});
    ~Okuyucu();
    Okuyucu(const Okuyucu&) = delete;
    Okuyucu& operator=(const Okuyucu&) = delete;

    const DosyaOzeti& ozet() const noexcept;

    // Ekin içeriği (CRC'si açılışta doğrulandı; okurken yeniden doğrulanır). Yoksa ya da
    // okunamıyorsa false.
    bool ek_oku(std::string_view ad, std::vector<std::uint8_t>& veri) const;

    // Sıralı okuma: yalnız verilen kanallar (boşsa okunur bütün kanallar). Okumayı başa alır.
    void suz(std::span<const KanalTanimi* const> kanallar);
    void basa_sar();
    // Sıradaki iletiyi verir; bittiğinde ya da hata olduğunda false (`okuma_hatasi()` ayırır).
    bool sonraki(Ileti& i);
    // Sıralı okuma yarıda kesildiyse neden (dosya açılıştan sonra değişti, sıra penceresi sınırı).
    const std::string& okuma_hatasi() const noexcept;

private:
    Okuyucu();
    std::unique_ptr<ic::OkuyucuIc> ic_;
};

}  // namespace ida::kayit
