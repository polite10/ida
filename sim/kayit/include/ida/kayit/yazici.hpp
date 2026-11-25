// ida/kayit/yazici.hpp — A6 kayıt yazıcısı (W5-kayit dilim 1).
//
// Bir çalıştırma bir dosyadır: `<dizin>/<run_id>.mcap` (A6 §3.1). Dosyaya tek bir iş parçacığı
// yazar (A6-K05). Üreticiler (dünya, HIL G/Ç, A3 sunucusu, …) veriyi üretici başına ayrı, sabit
// boylu tek üretici–tek tüketici halkayla verir (A6-K06):
//
//   `Uretici::yaz` ENGELLEMEZ: kilit almaz, bellek ayırmaz, dosyaya dokunmaz. Halka doluysa ileti
//   atılır; düşen iletinin kanalı, sim zamanı ve adımı üreticinin düşme sayaçlarında birikir ve
//   yazıcı bunları `kayit/eksik` kaydı olarak dosyaya yazar (A6-K30). Hiçbir ileti sessizce
//   kaybolmaz: her üretici için  uretilen = kuyruklanan + dusen_halka + dusen_gecersiz.
//
// Kullanım:
//   auto y = Yazici::ac(ayar, ust_veri, ekler, hata);     // var olan dosyayı ezmez (A6-K01)
//   Uretici& u = y->uretici(0);                           // her üretici tek iş parçacığından
//   u.yaz<Kanal::fizik_durum>(sim_t_ns, adim, yuk);
//   …                                                     // üreticiler durduktan sonra:
//   y->kapat(kapanis_alanlari);
#pragma once

#include "ida/kayit/katalog.hpp"
#include "ida/kayit/ust_veri.hpp"
#include "ida/kayit/yukler.hpp"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace ida::kayit {

namespace ic {
struct UreticiIc;
struct YaziciIc;
}  // namespace ic

enum class YazmaSonucu : std::uint8_t {
    yazildi,         // halkaya kondu
    suzuldu,         // kanal bu kademede/kipte yazılmıyor (A6-K16, K17); ileti üretilmiş sayılmaz
    dustu_halka,     // halka dolu (ya da ileti halkaya sığmayacak kadar büyük): `kayit/eksik`, halka_tasmasi
    dustu_gecersiz,  // negatif `log_time` (A6-K10) ya da kodlanamayan yük: `kayit/eksik`, yazma_hatasi
    kapali,          // dosya kapandı (kapanış, sınır, yazma hatası); çalıştırma kayıtsız sürer.
                     // Kapanıştan önce `yazildi` almış ama dosyaya girememiş iletiler `kayit/eksik`'tedir.
};

struct UreticiSayaclari {
    std::uint64_t uretilen = 0;        // süzgeçten geçip yazılmak istenen
    std::uint64_t kuyruklanan = 0;     // halkaya konan
    std::uint64_t dusen_halka = 0;
    std::uint64_t dusen_gecersiz = 0;
    std::uint64_t suzulen = 0;
    std::uint64_t kapali_atilan = 0;   // dosya kapalıyken gelen (üretilmiş sayılmaz)
};

// Kademeye bağlı içerik süzgeci (Tablo 3.3 "Ne zaman yazılır"). Genel durumda süzgeç yoktur.
template <Kanal K>
struct KademeSuzgeci {
    static bool yazilir(const typename KanalYuku<K>::tip&, Kademe) noexcept { return true; }
};
// A6-K17: `hil/bariyer`, `olaylar` kademesinde yalnız `karar != zamaninda` (0) olan partiler için.
template <>
struct KademeSuzgeci<Kanal::hil_bariyer> {
    static bool yazilir(const yuk_hil_bariyer& y, Kademe k) noexcept { return k != Kademe::olaylar || y.karar != 0; }
};
// Tablo 3.3, A6-K18, A8-K65: `algilama/parti`, `olaylar` kademesinde yalnız nihai durumu OK (0)
// olmayan, reddedilen ya da geç partiler için.
template <>
struct KademeSuzgeci<Kanal::algilama_parti> {
    static bool yazilir(const a3::yuk_algilama_parti& y, Kademe k) noexcept
    {
        return k != Kademe::olaylar || y.nihai_durum != 0 || y.ret_nedeni != 0 || y.gec != 0;
    }
};

// Bir üreticinin yazım ucu. Tek iş parçacığından kullanılır; `Yazici` yaşadıkça geçerlidir.
class Uretici {
public:
    Uretici(const Uretici&) = delete;
    Uretici& operator=(const Uretici&) = delete;

    // İletiyi kodlar ve halkaya koyar. `sim_t_ns` kanalın `zaman_anlami`'na göre sim zamanıdır
    // (`log_time`; A6-K09); `adim` iletinin ait olduğu fizik adımıdır (yalnız eksik kayıt bildirimi
    // için). `publish_time` çağrı anındaki kayıt duvar saatidir.
    template <Kanal K>
    YazmaSonucu yaz(std::int64_t sim_t_ns, std::uint64_t adim, const typename KanalYuku<K>::tip& y) noexcept
    {
        return yaz_ic_<K>(sim_t_ns, adim, y, nullptr);
    }
    // Aynısı; `publish_time`'ı (kayıt duvar saati, ns) üretici verir, saat okunmaz.
    template <Kanal K>
    YazmaSonucu yaz(std::int64_t sim_t_ns, std::uint64_t adim, const typename KanalYuku<K>::tip& y,
                    std::int64_t duvar_ns) noexcept
    {
        return yaz_ic_<K>(sim_t_ns, adim, y, &duvar_ns);
    }

    // Kayıt duvar saati: kayıt açılışında 0 olan monoton saat, ns (A6-K13).
    std::int64_t duvar_ns() const noexcept;
    Kademe kademe() const noexcept;
    UreticiSayaclari sayaclar() const noexcept;
    std::string_view ad() const noexcept;

private:
    friend struct ic::YaziciIc;
    explicit Uretici(ic::UreticiIc* ic) noexcept : ic_(ic) {}

    template <Kanal K>
    YazmaSonucu yaz_ic_(std::int64_t sim_t_ns, std::uint64_t adim, const typename KanalYuku<K>::tip& y,
                        const std::int64_t* duvar_ns) noexcept
    {
        // `kayit/eksik`'in kaynağı kayıt yazıcısıdır (Tablo 3.3); üreticiler bu kanala yazamaz.
        static_assert(K != Kanal::kayit_eksik, "kayit/eksik kanalini yalniz kayit yazicisi yazar");
        const auto kanal = static_cast<std::uint16_t>(K);
        if (!etkin_(kanal) || !KademeSuzgeci<K>::yazilir(y, kademe())) {
            suzuldu_();
            return YazmaSonucu::suzuldu;
        }
        const std::size_t n = boyut(y);  // ortak yüklerde ida::a3::boyut (ADL; W5-K36)
        YazmaSonucu s = YazmaSonucu::yazildi;
        std::uint8_t* p = ayir_(kanal, sim_t_ns, adim, n, duvar_ns, s);
        if (p == nullptr) {
            return s;
        }
        std::size_t yazilan = 0;
        if (!kodla(y, p, n, yazilan) || yazilan != n) {
            kodlanamadi_(kanal, sim_t_ns, adim);
            return YazmaSonucu::dustu_gecersiz;
        }
        yayinla_();
        return YazmaSonucu::yazildi;
    }

    bool etkin_(std::uint16_t kanal) const noexcept;
    void suzuldu_() noexcept;
    std::uint8_t* ayir_(std::uint16_t kanal, std::int64_t sim_t_ns, std::uint64_t adim, std::size_t n,
                        const std::int64_t* duvar_ns, YazmaSonucu& s) noexcept;
    void kodlanamadi_(std::uint16_t kanal, std::int64_t sim_t_ns, std::uint64_t adim) noexcept;
    void yayinla_() noexcept;

    ic::UreticiIc* ic_;
};

// Yazım hedefi. Varsayılanı yerel dosyadır; sınamalar yavaş ya da hata veren hedef verir.
class Dosya {
public:
    enum class Sonuc : std::uint8_t { tamam, disk_dolu, hata };
    virtual ~Dosya() = default;
    // Verinin tamamını yazar ya da hata döner (kısmi yazım hatadır).
    virtual Sonuc yaz(const std::uint8_t* veri, std::size_t n) = 0;
    // Yazılanı işletim sistemine teslim eder (süreç ölse de kalır; diske eşitleme değildir).
    virtual Sonuc bosalt() = 0;
    // Dosyayı `boy` bayta indirir; sonraki yazım oradan sürer.
    virtual Sonuc kes(std::uint64_t boy) = 0;
    virtual void kapat() = 0;
};
// Dosyayı yeni oluşturur; aynı adlı dosya varsa oluşturmaz ve nullptr döner (A6-K01).
using DosyaFabrikasi = std::function<std::unique_ptr<Dosya>(const std::filesystem::path& yol, std::string& hata)>;
std::unique_ptr<Dosya> yerel_dosya_olustur(const std::filesystem::path& yol, std::string& hata);

struct UreticiAyari {
    std::string ad;
    std::size_t halka_bayt = std::size_t{4} << 20;  // 2'nin kuvvetine yuvarlanır (tahmin; A6 §8 md.1)
};

struct Ayar {
    std::filesystem::path dizin = "kayitlar";  // A6 §3.1; yoksa oluşturulur
    Kademe kademe = Kademe::telemetri;
    std::vector<UreticiAyari> ureticiler;
    // true: yazıcı kendi iş parçacığını açar. false: çağıran `isle()` ile sürer (sınamalar ve
    // belirlenimci koşular); o zaman `isle`, `guncelle` ve `kapat` aynı iş parçacığından çağrılır.
    bool is_parcacigi = true;
    // Monoton saat, ns (herhangi bir başlangıç); boşsa `std::chrono::steady_clock`. Üretici ve
    // yazıcı iş parçacıklarından çağrılır; engellememelidir.
    std::function<std::int64_t()> saat;
    DosyaFabrikasi dosya_fabrikasi;  // boşsa `yerel_dosya_olustur`
    std::uint64_t parca_hedefi_bayt = kParcaHedefiBayt;  // A6-K03
    std::int64_t parca_kapatma_ns = kParcaKapatmaNs;     // A6-K04
    std::int64_t yoklama_araligi_ns = 2'000'000;         // iş parçacığının boşta uyku süresi (tahmin)
    // `sequence`'in ilk değeri (A6-K09: 1). Yalnız sınama: A6-K12 sınırını enjekte etmek için.
    std::uint32_t ilk_sira = 1;
};

enum class YaziciDurumu : std::uint8_t {
    acik,
    kapandi,        // `kapat` ile
    sinira_ulasti,  // A6-K12: dosya kapatıldı, çalıştırma kayıtsız sürüyor
    disk_dolu,      // A6-K31
    yazma_hatasi,   // A6-K31
};

struct YaziciSayaclari {
    std::uint64_t kuyruktan_alinan = 0;       // yazıcının halkalardan aldığı ileti (açık parçadakiler dahil)
    std::uint64_t yazilan_ileti = 0;          // dosyadaki ileti sayısı (`kayit/eksik` dahil; `ileti_sayisi`)
    std::uint64_t yazilan_uretici_iletisi = 0;  // üreticilerden gelip dosyaya giren
    std::uint64_t eksik_kaydi = 0;            // yazılan `kayit/eksik` kaydı sayısı
    std::uint64_t bildirilen_dusen = 0;       // `kayit/eksik` kayıtlarındaki `kayip_sayisi` toplamı
    // Kuyruktan alınmış ama dosyaya girememiş: yazma hatasında açık parçada kaybolanlar ve erken
    // kapanışta (sınır, yazma hatası, disk dolu) halkalarda kalıp yazılamayanlar. Hepsi kapanış
    // kaydından önce `kayit/eksik`'e çevrilir ve öneki daraltır (A6-K12, K31, K33).
    std::uint64_t yazimda_kaybolan = 0;
    // Dosya kapandıktan SONRA halkaya girmiş ve atılmış ileti (kabul kesilirken yazmakta olan
    // üretici). Dosyada bildirilemez; yalnız burada görünür. Üretici sonraki çağrılarda `kapali` alır.
    std::uint64_t kapali_iken_atilan = 0;
    // Hata kapanışında `kayit/eksik` kaydı yazılamadığı için dosyada bildirilemeyen kayıp ileti
    // (sıra yedeği bunu karşılar: 0 olmalıdır; 0 değilse dosyanın eksik bildirimi tam değildir).
    std::uint64_t bildirilemeyen_dusen = 0;
    std::uint64_t kapanan_parca = 0;
    std::uint64_t dosya_bayt = 0;
};

struct KapanisSonucu {
    bool dosya_kapandi = false;    // Footer ve sondaki sihirli baytlar yazıldı
    BitisDurumu bitis_durumu = BitisDurumu::hata;
    UstVeri kapanis_kaydi;         // dosyaya yazılan `ida_kayit_kapanis`
    YaziciSayaclari sayaclar;
    std::string hata;              // dosya kapanamadıysa neden
};

class Yazici {
public:
    // Dosyayı açar: Header, `ida_kayit` üst verisi, ekler; ilk iletiden önce (A6-K46). `ust_veri`
    // içinde `a6_surumu`, `kayit_kademesi`, `kamera_kareleri` ve `kayit_duvar_saati` boşsa yazıcı
    // doldurur; `bagimliliklar`'a mcap ve zstd sürümlerini ekler. Hata: aynı adlı dosya var
    // (A6-K01), geçersiz üst veri, eksik ya da bilinmeyen ek. Başarısızsa nullptr ve `hata`.
    static std::unique_ptr<Yazici> ac(const Ayar& ayar, const AcilisUstVerisi& ust_veri, std::span<const Ek> ekler,
                                     std::string& hata);
    // `kapat` çağrılmadıysa dosya son taslakla ve `bitis_durumu = hata` ile kapatılır.
    ~Yazici();
    Yazici(const Yazici&) = delete;
    Yazici& operator=(const Yazici&) = delete;

    std::size_t uretici_sayisi() const noexcept;
    Uretici& uretici(std::size_t i) noexcept;

    const std::filesystem::path& yol() const noexcept;
    Kademe kademe() const noexcept;                 // çalıştırma boyunca değişmez (A6-K20)
    std::int64_t duvar_ns() const noexcept;         // kayıt duvar saati (A6-K13)
    YaziciDurumu durum() const noexcept;
    YaziciSayaclari sayaclar() const noexcept;

    // Açılışta boş bırakılan koşullu bir anahtarı sonradan yazar (`ida_kayit_guncelleme`; A6-K46).
    // Yalnız kapalı listedeki anahtarlar; dolu anahtarı farklı değerle değiştirme reddedilir.
    // Üretici yolu değildir (kilit alır).
    bool guncelle(std::string_view anahtar, std::string_view deger, std::string& hata);

    // Dosya erken kapanırsa (A6-K12 sınırı, A6-K31 yazma hatası) kapanış kaydında kullanılacak
    // alanlar. Seyrek çağrılır (ör. 1 Hz, `sayac/gercek_zaman` ile birlikte); üretici yolu değildir.
    // Güvenceyi değiştiren bir alan (`canli_mudahale_adimi`; A6-K53) öğrenildiği ANDA yeniden
    // çağrılmalıdır: erken kapanış eski taslağı kullanırsa dosya gerçekte olmayan bir güvence yazar.
    void kapanis_taslagi(const KapanisAlanlari& taslak);

    // `is_parcacigi = false` iken bir yazım turu: halkaları boşaltır, düşmeleri `kayit/eksik`'e
    // çevirir, süresi dolan parçayı kapatır. İş yapıldıysa true.
    bool isle();

    // Olağan kapanış (A6-K44): bekleyen iletileri ve düşme bildirimlerini yazar, `ida_kayit_kapanis`
    // üst verisini yazar ve dosyayı kapatır. Çağrılmadan önce üreticiler durmuş olmalıdır (son
    // `sayac/gercek_zaman` iletisi dahil her şey üretilmiş). Dosya daha önce erken kapandıysa
    // (sınır, yazma hatası) o kapanışın sonucunu döndürür. Bu çağrı SIRASINDA sınıra ya da yazma
    // hatasına varılırsa kapanış kaydının çalıştırmaya ait alanları yine `alanlar`'dan alınır
    // (`bitis_durumu` dışında: o, kapanışın nedenidir).
    KapanisSonucu kapat(const KapanisAlanlari& alanlar);

private:
    Yazici();
    std::unique_ptr<ic::YaziciIc> ic_;
};

}  // namespace ida::kayit
