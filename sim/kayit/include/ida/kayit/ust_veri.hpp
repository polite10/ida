// ida/kayit/ust_veri.hpp — üst veri kayıtları, ekler ve tekrar güvencesi (A6 §3.5, §3.6, §4.6, §4.8).
//
// Açılış kaydı (`ida_kayit`) çağırandan `AcilisUstVerisi` yapısıyla alınır (üretilen; her anahtar
// bir metin). Kapanış kaydının (`ida_kayit_kapanis`) çalıştırmaya ait alanları `KapanisAlanlari`
// ile verilir; kayda ait olanları (ileti sayısı, eksiklik, tekrar güvencesi) yazıcı hesaplar.
#pragma once

#include "ida/kayit/katalog.hpp"
#include "ida/kayit/yukler.hpp"

#include <cstdint>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace ida::kayit {

// MCAP Metadata kaydının içeriği: anahtar → metin.
using UstVeri = std::map<std::string, std::string, std::less<>>;

// Dosyaya yazılacak ek (A6 §3.6). `a6_kanallar.json` verilmez; yazıcı gömülü kataloğunu ekler.
struct Ek {
    std::string ad;
    std::span<const std::uint8_t> veri;
};

// A6-K45.
enum class BitisDurumu : std::uint8_t {
    bitti,
    durduruldu,
    on_denetim_kaldi,
    hata,
    disk_dolu,
    yazma_hatasi,
    sinira_ulasti,
    sim_kapanisi,
};
std::string_view bitis_durumu_adi(BitisDurumu b) noexcept;
bool bitis_durumu_coz(std::string_view ad, BitisDurumu& b) noexcept;

// A6 §4.6. Sıra anlamlıdır (yok < izleme < fizik < girdi).
enum class Guvence : std::uint8_t { yok = 0, izleme = 1, fizik = 2, girdi = 3 };
std::string_view guvence_adi(Guvence g) noexcept;
bool guvence_coz(std::string_view ad, Guvence& g) noexcept;

// Kapanış kaydının çalıştırmaya ait alanları (A6 §3.5 `ida_kayit_kapanis`).
struct KapanisAlanlari {
    BitisDurumu bitis_durumu = BitisDurumu::hata;
    std::uint64_t son_adim_no = 0;
    std::int64_t son_sim_t_ns = 0;
    bool gecerli = false;
    bool gercek_zamani_kacirdi = false;
    std::string gecerlilik_nedenleri;  // virgülle ayrılmış; yoksa boş
    std::string sayaclar;              // `ad=deger` listesi
    // A6-K53: tekrar koşusunda ilk canlı müdahalenin adımı; varsa güvence `yok` olur.
    std::optional<std::uint64_t> canli_mudahale_adimi;
};

// Bir `kayit/eksik` kaydının içeriği (A6 §3.4 `yuk_kayit_eksik`); `kanal` katalogdaki kanaldır.
struct EksikKaydi {
    const KanalTanimi* kanal = nullptr;
    std::int64_t ilk_sim_t_ns = 0;
    std::int64_t son_sim_t_ns = 0;
    std::uint64_t ilk_adim = 0;
    std::uint64_t son_adim = 0;
    std::uint64_t kayip_sayisi = 0;
    std::uint8_t neden = 0;  // 0 halka_tasmasi, 1 yazma_hatasi, 2 disk_dolu
};

// A6-K33 girdisi.
struct GuvenceGirdisi {
    Kademe kademe = Kademe::olaylar;
    std::string_view kip;
    bool guvencesiz_baslatma = false;                 // A6-K38 (`guvencesiz_nedenler` dolu)
    bool canli_mudahale = false;                      // A6-K53
    // Düzeyin gerektirdiği kanallardaki ilk eksik adım (yoksa boş), düzey başına: [fizik, girdi].
    std::optional<std::uint64_t> ilk_eksik_fizik;
    std::optional<std::uint64_t> ilk_eksik_girdi;
    // Kaydın sonu: dosya erken kapandıysa (sınır, yazma hatası) son tam kaydedilen adım.
    std::optional<std::uint64_t> erken_son_adim;
    std::string_view erken_neden;                     // `sinira_ulasti`, `yazma_hatasi`, `disk_dolu`
};

struct GuvenceSonucu {
    Guvence duzey = Guvence::yok;
    Guvence beklenen = Guvence::yok;                // kademe ve kipin verebileceği en yüksek düzey
    std::optional<std::uint64_t> on_eki_son_adim;   // önek güvencesi (A6-K33)
    std::string nedenler;                           // virgülle ayrılmış; düzey beklenenden düşükse ya da önekse
};

// A6-K33: kademe ve kipin verdiği en yüksek düzey; `guvencesiz`/`canli_mudahale` → `yok`; düzeyin
// gerektirdiği kanalda eksik varsa önek; ilk eksik adım 0 ise bir alt düzeye inilir. `izleme`
// koşulsuzdur (Tablo 4.6): eksik kayıt onu daraltmaz.
GuvenceSonucu guvence_belirle(const GuvenceGirdisi& g);

// Eksik kaydın, `duzey`'in gerektirdiği bir kanala ait olup olmadığı (katalogdaki `tekrar_duzeyi`).
bool eksik_duzeyi_etkiler(const KanalTanimi& kanal, Guvence duzey) noexcept;

// Üst veri kaydını kataloğa göre denetler (`$defs/ust_veri_*`'ın karşılığı): bilinmeyen anahtar,
// eksik ya da boş zorunlu anahtar, biçim (onluk u32), `kip = sil` iken `sil_n_bg`. Kayıt adı
// `ida_kayit`, `ida_kayit_guncelleme` ya da `ida_kayit_kapanis` olmalıdır. Hata varsa false ve ileti.
bool ust_veri_denetle(std::string_view kayit, const UstVeri& u, std::string& hata);

// A3-K63: `YYYYMMDD-hhmmss-xxxxxx`.
bool run_id_gecerli(std::string_view run_id) noexcept;

// `bagimliliklar` üst verisine eklenecek kitaplık sürümleri: "mcap=<sürüm>,zstd=<sürüm>" (A6-K03).
std::string bagimlilik_surumleri();

}  // namespace ida::kayit
