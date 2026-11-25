// yazici.cpp — A6 kayıt yazıcısı: üretici yolu (kilitsiz) ve dosyaya yazan tek iş parçacığı.
//
// İş parçacığı kuralları:
//   Üretici yolu (`Uretici::ayir_`, `yayinla_`, `kodlanamadi_`, `suzuldu_`): kilit, bellek ayırma
//   ve dosya erişimi YOKTUR; yalnız kendi halkasına ve atomik sayaçlara dokunur (A6-K06).
//   Yazıcı yolu (`YaziciIc::tur` ve altındakiler): tek iş parçacığı; MCAP sarmalayıcısına ve
//   dosyaya yalnız o dokunur (A6-K05).
//   Denetim yolu (`guncelle`, `kapanis_taslagi`): seyrek; `kilit` ile yazıcıya iş bırakır.
#include "ida/kayit/yazici.hpp"

#include "dusme.hpp"
#include "halka.hpp"
#include "mcap_yazim.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstring>
#include <limits>
#include <mutex>
#include <optional>
#include <system_error>
#include <thread>
#include <utility>

namespace ida::kayit {

namespace ic {

namespace {

// Halka kaydının başlığı: [u16 kanal][u16 0][u32 0][i64 log_time][i64 publish_time][u64 adim].
constexpr std::size_t kKayitBasligi = 32;
// Üreticinin bildirdiği düşme nedenleri (`yuk_kayit_eksik.neden`): 0 halka_tasmasi, 1 yazma_hatasi.
constexpr std::size_t kUreticiNedeni = 2;
constexpr std::uint8_t kNedenHalkaTasmasi = 0;
constexpr std::uint8_t kNedenYazmaHatasi = 1;
constexpr std::uint8_t kNedenDiskDolu = 2;
// MCAP Message kaydının yük dışındaki boyu: opcode + uzunluk + kanal + sequence + iki zaman.
constexpr std::uint64_t kIletiEkYuku = 9 + 2 + 4 + 8 + 8;
constexpr std::uint64_t kSiraSonu = 0xFFFFFFFFull;  // A6-K12: en büyük `sequence`
// Bir turda bir üreticiden alınan en çok ileti (öteki üreticiler ve süreli işler aç kalmasın).
constexpr std::size_t kTurButcesi = 4096;

std::string yol_metni(const std::filesystem::path& p)
{
    const auto u = p.u8string();
    return std::string(u.begin(), u.end());
}

void art(std::atomic<std::uint64_t>& a) noexcept
{
    // Tek yazarlı sayaç: kilitli artırma gerekmez.
    a.store(a.load(std::memory_order_relaxed) + 1u, std::memory_order_relaxed);
}

void yaz64(std::uint8_t* p, std::uint64_t v) noexcept
{
    std::memcpy(p, &v, 8);
}
std::uint64_t oku64(const std::uint8_t* p) noexcept
{
    std::uint64_t v = 0;
    std::memcpy(&v, p, 8);
    return v;
}

}  // namespace

// Üreticilerle yazıcının paylaştığı, açılışta sabitlenen durum.
struct Ortak {
    Kademe kademe = Kademe::olaylar;
    std::array<bool, kKanalSayisi> etkin{};
    std::array<bool, kKanalSayisi> paysiz{};
    std::function<std::int64_t()> saat;
    std::int64_t saat_sifiri = 0;
    std::atomic<bool> kabul{true};
};

struct UreticiIc {
    UreticiIc(std::string ad_, std::size_t halka_bayt, Ortak* o) : ad(std::move(ad_)), ortak(o), halka(halka_bayt)
    {
        pay = halka.kapasite() / 8;
    }

    std::string ad;
    Ortak* ortak;
    BaytHalkasi halka;
    std::size_t pay = 0;
    std::array<std::array<DusmeYuvasi, kUreticiNedeni>, kKanalSayisi> dusme;
    std::atomic<std::uint64_t> dusme_toplam{0};
    std::atomic<std::uint64_t> uretilen{0};
    std::atomic<std::uint64_t> kuyruklanan{0};
    std::atomic<std::uint64_t> dusen_halka{0};
    std::atomic<std::uint64_t> dusen_gecersiz{0};
    std::atomic<std::uint64_t> suzulen{0};
    std::atomic<std::uint64_t> kapali_atilan{0};
    // Yazıcı tarafı.
    std::array<std::array<std::uint64_t, kUreticiNedeni>, kKanalSayisi> bildirilen{};
    std::uint64_t gorulen_dusme_toplam = 0;

    void dusur(std::uint16_t kanal, std::uint8_t neden, std::int64_t sim_t_ns, std::uint64_t adim) noexcept
    {
        dusme[kanal][neden].dusur(sim_t_ns, adim);
        dusme_toplam.store(dusme_toplam.load(std::memory_order_relaxed) + 1u, std::memory_order_release);
    }
};

namespace {

class Cikti final : public McapCikti {
public:
    void bagla(Dosya* d) noexcept { d_ = d; }
    void yaz(const std::uint8_t* veri, std::uint64_t n) override
    {
        boyut_ += n;
        if (hata_ == Dosya::Sonuc::tamam) {
            hata_ = d_->yaz(veri, static_cast<std::size_t>(n));
        }
    }
    std::uint64_t boyut() const override { return boyut_; }
    void bosalt() override
    {
        if (hata_ == Dosya::Sonuc::tamam) {
            hata_ = d_->bosalt();
        }
    }
    bool saglam() const noexcept { return hata_ == Dosya::Sonuc::tamam; }
    Dosya::Sonuc hata() const noexcept { return hata_; }
    // Dosya `boy`'a kesildikten sonra: sayım oradan sürer, hata temizlenir.
    void sifirla(std::uint64_t boy) noexcept
    {
        boyut_ = boy;
        hata_ = Dosya::Sonuc::tamam;
    }

private:
    Dosya* d_ = nullptr;
    std::uint64_t boyut_ = 0;
    Dosya::Sonuc hata_ = Dosya::Sonuc::tamam;
};

// Dosyaya girmemiş iletilerin kanal başına dökümü: açık parçadakiler (yazma hatasında kaybolur) ve
// erken kapanışta halkalardan alınıp yazılamayanlar. `kayit/eksik`'e çevrilir.
//
// Aralık en küçük ve en büyük değerdir (ilk ve son görülen değil): döküm birden çok üreticinin
// iletilerini karıştırır ve okuyucu önek sınırını `ilk_adim`'den kurar (A6-K33); `ilk_adim` dökümdeki
// EN KÜÇÜK adım olmazsa önek eksik bir adımı kapsar.
struct KanalDokumu {
    std::uint64_t sayi = 0;
    std::int64_t en_kucuk_t = 0;
    std::int64_t en_buyuk_t = 0;
    std::uint64_t en_kucuk_adim = 0;
    std::uint64_t en_buyuk_adim = 0;

    void ekle(std::int64_t t, std::uint64_t adim) noexcept
    {
        if (sayi == 0) {
            en_kucuk_t = t;
            en_buyuk_t = t;
            en_kucuk_adim = adim;
            en_buyuk_adim = adim;
        } else {
            en_kucuk_t = std::min(en_kucuk_t, t);
            en_buyuk_t = std::max(en_buyuk_t, t);
            en_kucuk_adim = std::min(en_kucuk_adim, adim);
            en_buyuk_adim = std::max(en_buyuk_adim, adim);
        }
        ++sayi;
    }
    void birlestir(const KanalDokumu& o) noexcept
    {
        if (o.sayi == 0) {
            return;
        }
        if (sayi == 0) {
            *this = o;
            return;
        }
        en_kucuk_t = std::min(en_kucuk_t, o.en_kucuk_t);
        en_buyuk_t = std::max(en_buyuk_t, o.en_buyuk_t);
        en_kucuk_adim = std::min(en_kucuk_adim, o.en_kucuk_adim);
        en_buyuk_adim = std::max(en_buyuk_adim, o.en_buyuk_adim);
        sayi += o.sayi;
    }
};

AnahtarDeger ciftler(const UstVeri& u)
{
    AnahtarDeger a;
    a.reserve(u.size());
    for (const auto& [k, v] : u) {
        a.emplace_back(k, v);
    }
    return a;
}

}  // namespace

struct YaziciIc {
    // ---- açılışta sabitlenen ----
    Ayar ayar;
    std::filesystem::path yol;
    std::string kip;
    bool guvencesiz_baslatma = false;
    Ortak ortak;
    std::vector<std::unique_ptr<UreticiIc>> uretici_ic;
    std::vector<std::unique_ptr<Uretici>> ureticiler;
    std::array<std::uint16_t, kKanalSayisi> mcap_kanal{};  // 0: bu dosyada kanal yok
    std::uint16_t eksik_kanali = 0;                        // kanallar() içinde `kayit/eksik`
    KuyrukKanali kuyruk_kanali;
    std::uint64_t sira_yedegi = 0;

    // ---- yalnız yazıcı iş parçacığı ----
    std::unique_ptr<Dosya> dosya;
    Cikti cikti;
    McapYazim mcap;
    std::uint64_t sonraki_sira = 1;      // sıradaki `sequence` (kSiraSonu + 1: tükendi)
    std::uint64_t saglam_sira = 1;       // dosyaya girmiş son parçadan sonraki `sequence`
    std::uint64_t saglam_boyut = 0;      // dosyanın son tutarlı boyu (son parçanın sonu)
    std::uint32_t saglam_crc = 0;        // veri bölümü CRC'sinin `saglam_boyut`'taki ara durumu (W5-K39)
    std::uint64_t acik_bayt = 0;         // açık parçanın sıkıştırmasız boyu (ileti, şema ve kanal kayıtları)
    std::array<std::uint32_t, kKanalSayisi> kanal_kayit_bayt{};  // kanal kaydının boyu
    std::array<std::uint32_t, kKanalSayisi> sema_kayit_bayt{};   // kanalın şema kaydının boyu
    std::array<std::uint16_t, kKanalSayisi> kanal_semasi{};      // semalar() indeksi
    std::array<bool, kKanalSayisi> kanal_yazildi{};              // kanal kaydı dosyaya girdi mi
    std::vector<bool> sema_yazildi;                              // şema kaydı dosyaya girdi mi
    std::int64_t acik_duvar_ns = 0;      // açık parçaya ilk iletinin yazıldığı an
    std::uint64_t acik_ileti = 0;
    std::uint64_t acik_uretici_iletisi = 0;
    std::uint64_t acik_eksik_kaydi = 0;
    std::uint64_t acik_bildirilen = 0;
    std::array<KanalDokumu, kKanalSayisi> acik{};    // açık parçadaki üretici iletileri
    // Erken kapanışta (sınır, yazma hatası, disk dolu) halkalardan alınıp dosyaya yazılamayan iletiler.
    std::array<KanalDokumu, kKanalSayisi> kalan{};
    // `bosalt`'ın halkadan okuyup henüz bırakmadığı iletinin üreticisi (yoksa nullptr). İç içe bir
    // hata kapanışı halkaları toplamadan önce o iletiyi bırakır (ileti açık parçanın dökümündedir).
    UreticiIc* eldeki = nullptr;
    // Erken kapanışta dosyaya giremeyen iletilerin (bütün üreticiler, bütün kanallar) en küçük adımı.
    std::optional<std::uint64_t> kayip_en_kucuk_adim;
    std::vector<EksikKaydi> acik_eksikler;           // açık parçadaki `kayit/eksik` kayıtları
    std::optional<std::uint64_t> acik_en_buyuk_adim;
    std::optional<std::uint64_t> en_buyuk_adim;  // dosyaya girmiş üretici iletilerinin en büyük adımı
    // Eksik izleme (kapanış kaydı için): dosyaya yazılan her `kayit/eksik`.
    bool eksik_var = false;
    std::optional<std::uint64_t> ilk_eksik_izleme;
    std::optional<std::uint64_t> ilk_eksik_fizik;
    std::optional<std::uint64_t> ilk_eksik_girdi;
    bool kapanis_yapildi = true;  // dosya açılana dek: kapatılacak bir şey yok
    KapanisSonucu sonuc;

    // ---- paylaşılan ----
    std::atomic<YaziciDurumu> durum{YaziciDurumu::acik};
    std::atomic<std::uint64_t> s_alinan{0};
    std::atomic<std::uint64_t> s_yazilan_ileti{0};
    std::atomic<std::uint64_t> s_yazilan_uretici{0};
    std::atomic<std::uint64_t> s_eksik_kaydi{0};
    std::atomic<std::uint64_t> s_bildirilen{0};
    std::atomic<std::uint64_t> s_yazimda_kaybolan{0};
    std::atomic<std::uint64_t> s_kapali_atilan{0};
    std::atomic<std::uint64_t> s_bildirilemeyen{0};
    std::atomic<std::uint64_t> s_kapanan_parca{0};
    std::atomic<std::uint64_t> s_dosya_bayt{0};
    std::mutex kilit;  // taslak, bekleyen güncellemeler, bilinen üst veri değerleri
    KapanisAlanlari taslak;
    std::vector<std::pair<std::string, std::string>> bekleyen_guncelleme;
    UstVeri bilinen;   // açılış kaydı + kabul edilen güncellemeler (A6-K46 birleştirmesi)
    std::thread is;
    std::atomic<bool> dur{false};

    Uretici* uretici_olustur(UreticiIc* u) { return new Uretici(u); }

    std::int64_t duvar_ns() const noexcept { return ortak.saat() - ortak.saat_sifiri; }

    YaziciSayaclari sayaclar() const noexcept
    {
        YaziciSayaclari s;
        s.kuyruktan_alinan = s_alinan.load(std::memory_order_acquire);
        s.yazilan_ileti = s_yazilan_ileti.load(std::memory_order_acquire);
        s.yazilan_uretici_iletisi = s_yazilan_uretici.load(std::memory_order_acquire);
        s.eksik_kaydi = s_eksik_kaydi.load(std::memory_order_acquire);
        s.bildirilen_dusen = s_bildirilen.load(std::memory_order_acquire);
        s.yazimda_kaybolan = s_yazimda_kaybolan.load(std::memory_order_acquire);
        s.kapali_iken_atilan = s_kapali_atilan.load(std::memory_order_acquire);
        s.bildirilemeyen_dusen = s_bildirilemeyen.load(std::memory_order_acquire);
        s.kapanan_parca = s_kapanan_parca.load(std::memory_order_acquire);
        s.dosya_bayt = s_dosya_bayt.load(std::memory_order_acquire);
        return s;
    }

    bool acik_mi() const noexcept { return durum.load(std::memory_order_acquire) == YaziciDurumu::acik; }

    // ---- parça ----
    void acik_sifirla() noexcept
    {
        acik_bayt = 0;
        acik_ileti = 0;
        acik_uretici_iletisi = 0;
        acik_eksik_kaydi = 0;
        acik_bildirilen = 0;
        acik_en_buyuk_adim.reset();
        acik_eksikler.clear();
        for (auto& a : acik) {
            a.sayi = 0;
        }
    }

    // Dosyanın o anki sonu tutarlı bir noktadır (yazılan her bayt işletim sistemine teslim edildi):
    // hata kapanışı buraya keser ve veri bölümü CRC'sini buradaki ara durumdan sürdürür.
    void saglam_noktasi()
    {
        saglam_boyut = cikti.boyut();
        saglam_crc = mcap.veri_crc_ara();
    }

    // Açık parçayı dosyaya yazar ve işletim sistemine teslim eder. Hata varsa false (dosya hata
    // kapanışına girmiştir).
    bool parca_kapat()
    {
        if (acik_ileti == 0) {
            return true;
        }
        mcap.parca_kapat();
        // Kitaplık parça kaydından sonra çıktıyı boşaltır, ardından ileti indekslerini yazar; onlar da
        // işletim sistemine teslim edilir: `saglam_boyut`'a kadarki her bayt dosyadadır.
        cikti.bosalt();
        if (!cikti.saglam()) {
            hata_kapanisi();
            return false;
        }
        saglam_noktasi();
        saglam_sira = sonraki_sira;
        s_yazilan_ileti.fetch_add(acik_ileti, std::memory_order_acq_rel);
        s_yazilan_uretici.fetch_add(acik_uretici_iletisi, std::memory_order_acq_rel);
        s_eksik_kaydi.fetch_add(acik_eksik_kaydi, std::memory_order_acq_rel);
        s_bildirilen.fetch_add(acik_bildirilen, std::memory_order_acq_rel);
        s_kapanan_parca.fetch_add(1, std::memory_order_acq_rel);
        s_dosya_bayt.store(saglam_boyut, std::memory_order_release);
        if (acik_en_buyuk_adim.has_value() && (!en_buyuk_adim.has_value() || *acik_en_buyuk_adim > *en_buyuk_adim)) {
            en_buyuk_adim = acik_en_buyuk_adim;
        }
        acik_sifirla();
        return true;
    }

    // ---- ileti ----
    // İletiyi açık parçaya yazar. `ureticiden` false ise ileti bir `kayit/eksik` kaydıdır.
    // Dönüş false: ileti dosyaya girmedi ve dosya kapandı (sınır ya da yazma hatası).
    bool ileti_yaz(std::uint16_t kanal, std::int64_t log_t, std::int64_t pub_t, std::uint64_t adim,
                   const std::uint8_t* veri, std::size_t n, bool ureticiden)
    {
        if (acik_ileti == 0) {
            acik_duvar_ns = duvar_ns();
        }
        // Sıra numarası tükenmişse (yedek hesabı gereği olmamalı) ileti yazılamaz; hata kapanışı
        // onu kayıp olarak bildirir.
        const bool sira_var = sonraki_sira <= kSiraSonu;
        const bool tamam = sira_var && mcap.ileti_yaz(mcap_kanal[kanal], static_cast<std::uint32_t>(sonraki_sira),
                                                      static_cast<std::uint64_t>(log_t),
                                                      static_cast<std::uint64_t>(std::max<std::int64_t>(pub_t, 0)),
                                                      veri, n);
        // İleti açık parçanın dökümüne girer: kalıcı olmazsa kayıp olarak bildirilir.
        ++acik_ileti;
        if (ureticiden) {
            acik[kanal].ekle(log_t, adim);
            ++acik_uretici_iletisi;
            if (!acik_en_buyuk_adim.has_value() || adim > *acik_en_buyuk_adim) {
                acik_en_buyuk_adim = adim;
            }
        }
        if (!tamam || !cikti.saglam()) {
            hata_kapanisi();
            return false;
        }
        ++sonraki_sira;
        // Kitaplık bir kanalın ilk iletisinden önce şema (ilk kullanımda) ve kanal kaydını da parçaya yazar.
        if (!kanal_yazildi[kanal]) {
            kanal_yazildi[kanal] = true;
            acik_bayt += kanal_kayit_bayt[kanal];
            if (!sema_yazildi[kanal_semasi[kanal]]) {
                sema_yazildi[kanal_semasi[kanal]] = true;
                acik_bayt += sema_kayit_bayt[kanal];
            }
        }
        acik_bayt += kIletiEkYuku + n;
        if (acik_bayt >= ayar.parca_hedefi_bayt) {
            return parca_kapat();
        }
        return true;
    }

    std::uint64_t sira_kalan() const noexcept { return kSiraSonu + 1u - sonraki_sira; }

    // ---- kayit/eksik ----
    void eksik_izle(const EksikKaydi& e) noexcept
    {
        eksik_var = true;
        const auto guncelle_ = [&](std::optional<std::uint64_t>& hedef) {
            if (!hedef.has_value() || e.ilk_adim < *hedef) {
                hedef = e.ilk_adim;
            }
        };
        if (eksik_duzeyi_etkiler(*e.kanal, Guvence::izleme)) {
            guncelle_(ilk_eksik_izleme);
        }
        if (eksik_duzeyi_etkiler(*e.kanal, Guvence::fizik)) {
            guncelle_(ilk_eksik_fizik);
        }
        if (eksik_duzeyi_etkiler(*e.kanal, Guvence::girdi)) {
            guncelle_(ilk_eksik_girdi);
        }
    }

    static std::size_t eksik_kodla(const EksikKaydi& e, std::array<std::uint8_t, 128>& tampon) noexcept
    {
        yuk_kayit_eksik y;
        y.kanal = e.kanal->ad;
        y.ilk_sim_t_ns = e.ilk_sim_t_ns;
        y.son_sim_t_ns = e.son_sim_t_ns;
        y.ilk_adim = e.ilk_adim;
        y.son_adim = e.son_adim;
        y.kayip_sayisi = e.kayip_sayisi;
        y.neden = e.neden;
        std::size_t yazilan = 0;
        if (!kodla(y, tampon.data(), tampon.size(), yazilan)) {
            return 0;
        }
        return yazilan;
    }

    // `kayit/eksik` kaydını dosyaya yazar (A6-K30). `log_time` son düşen iletinin sim zamanıdır
    // (negatifse 0; A6-K10).
    bool eksik_yaz(const EksikKaydi& e)
    {
        std::array<std::uint8_t, 128> tampon{};
        const std::size_t n = eksik_kodla(e, tampon);  // açılışta her kanal için sınandı: 0 olamaz
        const std::int64_t log_t = std::max<std::int64_t>(e.son_sim_t_ns, 0);
        ++acik_eksik_kaydi;
        acik_bildirilen += e.kayip_sayisi;
        acik_eksikler.push_back(e);
        eksik_izle(e);
        return ileti_yaz(eksik_kanali, log_t, duvar_ns(), e.son_adim, tampon.data(), n, false);
    }

    // Üreticilerin düşme sayaçlarındaki değişikliği `kayit/eksik` kayıtlarına çevirir. `topla`
    // verilirse dosyaya yazmaz, kayıtları oraya koyar (hata kapanışı).
    bool dusmeleri_isle(bool hepsi, std::vector<EksikKaydi>* topla)
    {
        bool is_yapildi = false;
        const auto katalog = kanallar();
        for (auto& up : uretici_ic) {
            UreticiIc& u = *up;
            const std::uint64_t toplam = u.dusme_toplam.load(std::memory_order_acquire);
            if (!hepsi && toplam == u.gorulen_dusme_toplam) {
                continue;
            }
            bool tutarli = true;
            for (std::size_t k = 0; k < kKanalSayisi; ++k) {
                for (std::size_t neden = 0; neden < kUreticiNedeni; ++neden) {
                    DusmeYuvasi& yuva = u.dusme[k][neden];
                    if (yuva.sayi() == u.bildirilen[k][neden]) {
                        continue;
                    }
                    DusmeKesiti kesit;
                    if (!yuva.oku(kesit)) {
                        tutarli = false;  // üretici o anda yazıyor: bir sonraki turda
                        continue;
                    }
                    const std::uint64_t fark = kesit.sayi - u.bildirilen[k][neden];
                    if (fark == 0) {
                        continue;
                    }
                    EksikKaydi e;
                    e.kanal = &katalog[k];
                    e.ilk_sim_t_ns = kesit.ilk_t;
                    e.son_sim_t_ns = kesit.son_t;
                    e.ilk_adim = kesit.ilk_adim;
                    e.son_adim = kesit.son_adim;
                    e.kayip_sayisi = fark;
                    e.neden = static_cast<std::uint8_t>(neden);
                    if (fark == 1) {
                        // Tek ileti: aralığın başı da sonu da o iletidir (dusme.hpp'deki yarışta kesin).
                        e.ilk_sim_t_ns = kesit.son_t;
                        e.ilk_adim = kesit.son_adim;
                    }
                    u.bildirilen[k][neden] = kesit.sayi;
                    yuva.onayla(kesit.sayi);
                    is_yapildi = true;
                    if (topla != nullptr) {
                        topla->push_back(e);
                    } else if (!eksik_yaz(e)) {
                        return true;  // dosya kapandı
                    }
                }
            }
            if (tutarli) {
                u.gorulen_dusme_toplam = toplam;
            }
        }
        return is_yapildi;
    }

    // ---- kapanış kaydı ----
    UstVeri kapanis_kaydi(const KapanisAlanlari& a, BitisDurumu bitis, std::uint64_t ileti_sayisi,
                          std::string_view erken_neden) const
    {
        GuvenceGirdisi g;
        g.kademe = ortak.kademe;
        g.kip = kip;
        g.guvencesiz_baslatma = guvencesiz_baslatma;
        g.canli_mudahale = a.canli_mudahale_adimi.has_value();
        g.ilk_eksik_fizik = ilk_eksik_fizik;
        g.ilk_eksik_girdi = ilk_eksik_girdi;
        g.erken_neden = erken_neden;
        g.tavan = a.guvence_tavani;
        g.tavan_nedeni = a.guvence_tavan_nedeni;
        if (!erken_neden.empty() && en_buyuk_adim.has_value()) {
            // Kayıt erken bitti. Önek hiçbir eksik adımı kapsayamaz (A6-K12, K33): dosyadaki en büyük
            // adım yarım kalmış olabilir (ondan önceki adım tam kaydedilmiştir) ve kapanışta dosyaya
            // giremeyen iletilerin (bütün üreticilerin halkalarında kalanlar ve açık parçada
            // kaybolanlar; kanal ayrımı yapılmadan) en küçük adımı eksiktir.
            std::uint64_t ilk_kapsanmayan = *en_buyuk_adim;
            if (kayip_en_kucuk_adim.has_value()) {
                ilk_kapsanmayan = std::min(ilk_kapsanmayan, *kayip_en_kucuk_adim);
            }
            if (ilk_kapsanmayan > 0) {
                g.erken_son_adim = ilk_kapsanmayan - 1u;
            }
        }
        const GuvenceSonucu gs = guvence_belirle(g);

        UstVeri k;
        k["bitis_durumu"] = std::string(bitis_durumu_adi(bitis));
        k["son_adim_no"] = std::to_string(a.son_adim_no);
        k["son_sim_t_ns"] = std::to_string(a.son_sim_t_ns);
        k["gecerli"] = a.gecerli ? "true" : "false";
        k["gercek_zamani_kacirdi"] = a.gercek_zamani_kacirdi ? "true" : "false";
        k["gecerlilik_nedenleri"] = a.gecerlilik_nedenleri;
        k["sayaclar"] = a.sayaclar;
        k["kayit_eksik"] = eksik_var ? "true" : "false";
        if (eksik_var) {
            // `kayit_eksik` ise tekrar düzeyi kanallarındaki ilk eksik adım: kaydın kademe ve
            // kipinin verebileceği düzeyin gerektirdiği kanallar.
            const std::optional<std::uint64_t>& ilk = gs.beklenen == Guvence::girdi   ? ilk_eksik_girdi
                                                      : gs.beklenen == Guvence::fizik ? ilk_eksik_fizik
                                                                                      : ilk_eksik_izleme;
            if (ilk.has_value()) {
                k["ilk_eksik_adim"] = std::to_string(*ilk);
            }
        }
        k["tekrar_guvencesi"] = std::string(guvence_adi(gs.duzey));
        if (gs.on_eki_son_adim.has_value()) {
            k["guvence_on_eki_son_adim"] = std::to_string(*gs.on_eki_son_adim);
        }
        if (!gs.nedenler.empty()) {
            k["guvence_nedenleri"] = gs.nedenler;
        }
        if (a.canli_mudahale_adimi.has_value()) {
            k["canli_mudahale_adimi"] = std::to_string(*a.canli_mudahale_adimi);
        }
        k["ileti_sayisi"] = std::to_string(ileti_sayisi);
        return k;
    }

    void kapandi(YaziciDurumu d, bool dosya_kapandi, BitisDurumu bitis, UstVeri kayit, std::string hata)
    {
        ortak.kabul.store(false, std::memory_order_release);
        if (dosya) {
            dosya->kapat();
        }
        sonuc.dosya_kapandi = dosya_kapandi;
        sonuc.bitis_durumu = bitis;
        sonuc.kapanis_kaydi = std::move(kayit);
        sonuc.hata = std::move(hata);
        kapanis_yapildi = true;
        durum.store(d, std::memory_order_release);
    }

    KapanisAlanlari taslak_al()
    {
        const std::lock_guard<std::mutex> kl(kilit);
        return taslak;
    }

    void kayip_adimi(std::uint64_t adim) noexcept
    {
        if (!kayip_en_kucuk_adim.has_value() || adim < *kayip_en_kucuk_adim) {
            kayip_en_kucuk_adim = adim;
        }
    }

    // Erken kapanış (sınır, yazma hatası, disk dolu): üreticilerin halkalarında bekleyen iletiler artık
    // dosyaya yazılamaz. BÜTÜN halkalar boşaltılır ve iletiler kanal başına `kalan` dökümüne alınır;
    // kapanış kaydından ÖNCE `kayit/eksik`'e çevrilirler (A6-K12, K31, K33: hiçbir ileti sessizce
    // kaybolmaz; önek onların adımını kapsamaz). Çağrıdan önce `kabul` kesilmiş olmalıdır.
    void kalanlari_topla()
    {
        if (eldeki != nullptr) {
            // `bosalt` bir iletiyi yazarken hata kapanışına girildi: o ileti açık parçanın dökümünde
            // sayılmıştır; burada yalnız halkadan bırakılır (iki kez sayılmaz).
            eldeki->halka.birak();
            eldeki = nullptr;
        }
        for (auto& up : uretici_ic) {
            UreticiIc& u = *up;
            for (;;) {
                std::size_t n = 0;
                const std::uint8_t* p = u.halka.oku(n);
                if (p == nullptr) {
                    break;
                }
                std::uint16_t kanal = 0;
                std::memcpy(&kanal, p, 2);
                if (n >= kKayitBasligi && kanal < kKanalSayisi && mcap_kanal[kanal] != 0) {
                    const std::uint64_t adim = oku64(p + 24);
                    kalan[kanal].ekle(static_cast<std::int64_t>(oku64(p + 8)), adim);
                    kayip_adimi(adim);
                }
                // Geçersiz başlık olamaz (üretici yalnız bu dosyada kayıtlı kanala yazar); olursa da sayılır.
                s_yazimda_kaybolan.fetch_add(1, std::memory_order_acq_rel);
                u.halka.birak();
                s_alinan.fetch_add(1, std::memory_order_acq_rel);
            }
        }
    }

    static EksikKaydi dokum_kaydi(const KanalTanimi& kanal, const KanalDokumu& dk, std::uint8_t neden) noexcept
    {
        EksikKaydi e;
        e.kanal = &kanal;
        e.ilk_sim_t_ns = dk.en_kucuk_t;
        e.son_sim_t_ns = dk.en_buyuk_t;
        e.ilk_adim = dk.en_kucuk_adim;
        e.son_adim = dk.en_buyuk_adim;
        e.kayip_sayisi = dk.sayi;
        e.neden = neden;
        return e;
    }

    // Sınır kapanışı: `kalan` dökümünü `kayit/eksik` kayıtları olarak dosyaya yazar. `yuk_kayit_eksik.neden`
    // kapalı listesinde (A6 §3.4: halka_tasmasi, yazma_hatasi, disk_dolu) sınır için ayrı bir değer
    // yoktur; "yazılamadı" anlamındaki `yazma_hatasi` kullanılır (A6-K10'daki kullanımla aynı).
    // Dönüş false: dosya hata kapanışına girdi (kalan kayıtlar orada bildirildi).
    bool kalanlari_yaz()
    {
        const auto katalog = kanallar();
        for (std::size_t k = 0; k < kKanalSayisi; ++k) {
            if (kalan[k].sayi == 0) {
                continue;
            }
            const EksikKaydi e = dokum_kaydi(katalog[k], kalan[k], kNedenYazmaHatasi);
            // Kayıt açık parçanın `kayit/eksik` listesine girer; yazılamazsa hata kapanışı onu oradan
            // yeniden yazar. Döküm önce sıfırlanır: aynı kayıp iki kez bildirilmez.
            kalan[k] = KanalDokumu{};
            if (!eksik_yaz(e)) {
                return false;
            }
        }
        return true;
    }

    // A6-K31: yazma hatası ya da disk dolu. Dosya son sağlam parçanın sonuna kesilir; kaybolan
    // iletiler `kayit/eksik` olarak, ardından kapanış kaydı ve dosya sonu elle yazılır (özet
    // bölümü olmadan). Bu da yazılamazsa dosya kapanmamış ama tutarlı kalır (kurtarma okuması).
    void hata_kapanisi()
    {
        const bool disk_dolu = cikti.hata() == Dosya::Sonuc::disk_dolu;
        const YaziciDurumu d = disk_dolu ? YaziciDurumu::disk_dolu : YaziciDurumu::yazma_hatasi;
        const BitisDurumu bitis = disk_dolu ? BitisDurumu::disk_dolu : BitisDurumu::yazma_hatasi;
        const std::string_view neden_adi = bitis_durumu_adi(bitis);
        ortak.kabul.store(false, std::memory_order_release);
        mcap.birak();
        // Kapanış kaydından önce bütün üreticilerin halkaları boşaltılır (önek ve `kayit/eksik` için).
        kalanlari_topla();

        // Bildirilecekler: (1) açık parçadaki `kayit/eksik` kayıtları (üretici sayaçları onaylandı;
        // aynen yeniden yazılır), (2) dosyaya giremeyen üretici iletileri, kanal başına: açık
        // parçadakiler ve halkalardan toplananlar, (3) henüz bildirilmemiş üretici düşmeleri.
        std::vector<EksikKaydi> eksikler = acik_eksikler;
        const auto katalog = kanallar();
        std::uint64_t kaybolan = 0;
        for (std::size_t k = 0; k < kKanalSayisi; ++k) {
            if (acik[k].sayi != 0) {
                kaybolan += acik[k].sayi;  // halkalardan toplananlar `kalanlari_topla`'da sayıldı
                kayip_adimi(acik[k].en_kucuk_adim);
            }
            KanalDokumu dk = acik[k];
            dk.birlestir(kalan[k]);
            kalan[k] = KanalDokumu{};
            if (dk.sayi == 0) {
                continue;
            }
            eksikler.push_back(dokum_kaydi(katalog[k], dk, disk_dolu ? kNedenDiskDolu : kNedenYazmaHatasi));
        }
        (void)dusmeleri_isle(true, &eksikler);
        s_yazimda_kaybolan.fetch_add(kaybolan, std::memory_order_acq_rel);
        acik_sifirla();

        std::string hata = "yazma hatası (" + std::string(neden_adi) + ")";
        if (dosya->kes(saglam_boyut) != Dosya::Sonuc::tamam) {
            kapandi(d, false, bitis, {}, hata + "; dosya kesilemedi");
            return;
        }
        cikti.sifirla(saglam_boyut);
        sonraki_sira = saglam_sira;

        std::vector<KuyrukIletisi> iletiler;
        std::uint64_t bildirilen = 0;
        for (const EksikKaydi& e : eksikler) {
            // Kayıt dosyaya yazılamasa da kapanış kaydı onu hesaba katar (`kayit_eksik`, önek).
            eksik_izle(e);
            std::array<std::uint8_t, 128> tampon{};
            const std::size_t n = eksik_kodla(e, tampon);
            if (n == 0 || sonraki_sira > kSiraSonu) {
                // Olamaz: kodlama açılışta her kanal için sınandı, sıra yedeği (`sira_yedegi`) kuyruğun
                // bütün kayıtlarını karşılar (sim.kayit.sira_yedegi). Olursa sessiz kalmaz.
                s_bildirilemeyen.fetch_add(e.kayip_sayisi, std::memory_order_acq_rel);
                continue;
            }
            KuyrukIletisi i;
            i.sira = static_cast<std::uint32_t>(sonraki_sira++);
            i.log_time = static_cast<std::uint64_t>(std::max<std::int64_t>(e.son_sim_t_ns, 0));
            i.publish_time = static_cast<std::uint64_t>(std::max<std::int64_t>(duvar_ns(), 0));
            i.veri.assign(tampon.begin(), tampon.begin() + static_cast<std::ptrdiff_t>(n));
            iletiler.push_back(std::move(i));
            bildirilen += e.kayip_sayisi;
        }
        const std::uint64_t ileti_sayisi = s_yazilan_ileti.load(std::memory_order_acquire) + iletiler.size();
        // Çalıştırmaya ait alanlar son taslaktan: `kapat(alanlar)` kendi alanlarını girişte taslağa yazar.
        UstVeri kayit = kapanis_kaydi(taslak_al(), bitis, ileti_sayisi, neden_adi);
        McapYazim::kuyruk_yaz(cikti, saglam_crc, kuyruk_kanali, iletiler, kUstVeriKapanis, ciftler(kayit));
        if (!cikti.saglam()) {
            // Kuyruk da yazılamadı: dosya son sağlam parçada biter (kapanmamış).
            (void)dosya->kes(saglam_boyut);
            kapandi(d, false, bitis, {}, hata + "; kapanış yazılamadı");
            return;
        }
        s_yazilan_ileti.fetch_add(iletiler.size(), std::memory_order_acq_rel);
        s_eksik_kaydi.fetch_add(iletiler.size(), std::memory_order_acq_rel);
        s_bildirilen.fetch_add(bildirilen, std::memory_order_acq_rel);
        s_dosya_bayt.store(cikti.boyut(), std::memory_order_release);
        kapandi(d, true, bitis, std::move(kayit), std::move(hata));
    }

    // Olağan ve sınır kapanışı: düşme bildirimleri, kapanış üst verisi, Data End, özet, Footer.
    void duzgun_kapanis(const KapanisAlanlari& a, BitisDurumu bitis, YaziciDurumu d, std::string_view erken_neden)
    {
        ortak.kabul.store(false, std::memory_order_release);
        if (!erken_neden.empty()) {
            // Sınır kapanışı: sıra numarası kalmadı; halkalarda bekleyen iletiler yazılamaz. Kapanış
            // kaydından önce toplanır ve `kayit/eksik`'e çevrilirler (olağan kapanışta halkaları
            // `kapat` daha önce dosyaya boşaltmıştır).
            kalanlari_topla();
        }
        (void)dusmeleri_isle(true, nullptr);
        if (!acik_mi() || kapanis_yapildi) {
            return;  // düşme kaydı yazılırken hata kapanışına girildi
        }
        if (!kalanlari_yaz() || !parca_kapat()) {
            return;
        }
        const std::uint64_t ileti_sayisi = s_yazilan_ileti.load(std::memory_order_acquire);
        UstVeri kayit = kapanis_kaydi(a, bitis, ileti_sayisi, erken_neden);
        const bool tamam = mcap.ust_veri_yaz(kUstVeriKapanis, ciftler(kayit));
        if (!tamam || !cikti.saglam()) {
            hata_kapanisi();
            return;
        }
        mcap.kapat();
        if (!cikti.saglam()) {
            // Özet yazılamadı: kapanış kaydından önceki sağlam noktaya dönülür ve elle kapatılır.
            hata_kapanisi();
            return;
        }
        s_dosya_bayt.store(cikti.boyut(), std::memory_order_release);
        kapandi(d, true, bitis, std::move(kayit), {});
    }

    // ---- tur ----
    void guncellemeleri_yaz()
    {
        std::vector<std::pair<std::string, std::string>> is_listesi;
        {
            const std::lock_guard<std::mutex> kl(kilit);
            is_listesi.swap(bekleyen_guncelleme);
        }
        for (auto& [anahtar, deger] : is_listesi) {
            if (!acik_mi() || !parca_kapat()) {
                return;
            }
            AnahtarDeger icerik;
            icerik.emplace_back(std::move(anahtar), std::move(deger));
            const bool tamam = mcap.ust_veri_yaz(kUstVeriGuncelleme, icerik);
            if (!tamam || !cikti.saglam()) {
                hata_kapanisi();
                return;
            }
            cikti.bosalt();
            if (!cikti.saglam()) {
                // Teslim edilemeyen kayıt sağlam nokta sayılmaz: `saglam_boyut` diskteki boyu aşarsa
                // hata kapanışı dosyayı kesemez ve kuyruk hiç yazılamaz.
                hata_kapanisi();
                return;
            }
            saglam_noktasi();
            s_dosya_bayt.store(saglam_boyut, std::memory_order_release);
        }
    }

    bool bosalt(UreticiIc& u, std::size_t butce)
    {
        bool is_yapildi = false;
        for (std::size_t i = 0; i < butce; ++i) {
            std::size_t n = 0;
            const std::uint8_t* p = u.halka.oku(n);
            if (p == nullptr) {
                break;
            }
            is_yapildi = true;
            if (!acik_mi()) {
                s_kapali_atilan.fetch_add(1, std::memory_order_acq_rel);
                u.halka.birak();
                continue;
            }
            if (sira_kalan() <= sira_yedegi) {
                // Bu ileti halkada bırakılır: sınır kapanışı onu ve bütün halkalarda kalanları toplar,
                // `kayit/eksik`'e çevirir ve dosyayı kapatır.
                sinir_kapanisi();
                continue;
            }
            std::uint16_t kanal = 0;
            std::memcpy(&kanal, p, 2);
            const auto log_t = static_cast<std::int64_t>(oku64(p + 8));
            const auto pub_t = static_cast<std::int64_t>(oku64(p + 16));
            const std::uint64_t adim = oku64(p + 24);
            if (n >= kKayitBasligi && kanal < kKanalSayisi && mcap_kanal[kanal] != 0) {
                // Yazılamazsa dosya kapanmıştır; ileti hata kapanışında kayıp olarak bildirilmiştir
                // ve halkadan orada bırakılmıştır (`eldeki`).
                eldeki = &u;
                (void)ileti_yaz(kanal, log_t, pub_t, adim, p + kKayitBasligi, n - kKayitBasligi, true);
                if (eldeki != nullptr) {
                    u.halka.birak();
                    eldeki = nullptr;
                }
            } else {
                // Olamaz: üretici yalnız bu dosyada kayıtlı kanala yazar. Olursa sessiz kalmaz, sayılır.
                s_yazimda_kaybolan.fetch_add(1, std::memory_order_acq_rel);
                u.halka.birak();
            }
            s_alinan.fetch_add(1, std::memory_order_acq_rel);
        }
        return is_yapildi;
    }

    // A6-K12: sıra numaraları tükeniyor. Kalan pay (`sira_yedegi`) kapanıştaki düşme bildirimlerine
    // ayrılıdır; dosya `sinira_ulasti` ile kapatılır ve çalıştırma kayıtsız sürer.
    void sinir_kapanisi()
    {
        duzgun_kapanis(taslak_al(), BitisDurumu::sinira_ulasti, YaziciDurumu::sinira_ulasti, "sinira_ulasti");
    }

    bool tur()
    {
        bool is_yapildi = false;
        if (acik_mi() && sira_kalan() <= sira_yedegi) {
            sinir_kapanisi();
        }
        if (acik_mi()) {
            guncellemeleri_yaz();
        }
        for (auto& u : uretici_ic) {
            is_yapildi = bosalt(*u, kTurButcesi) || is_yapildi;
        }
        if (acik_mi()) {
            is_yapildi = dusmeleri_isle(false, nullptr) || is_yapildi;
        }
        // A6-K04: açık parça en geç `parca_kapatma_ns` duvar saatinde kapatılır.
        if (acik_mi() && acik_ileti > 0 && duvar_ns() - acik_duvar_ns >= ayar.parca_kapatma_ns) {
            (void)parca_kapat();
            is_yapildi = true;
        }
        return is_yapildi;
    }

    void is_dongusu()
    {
        while (!dur.load(std::memory_order_acquire)) {
            if (!tur()) {
                std::this_thread::sleep_for(std::chrono::nanoseconds(ayar.yoklama_araligi_ns));
            }
        }
    }

    void is_parcacigini_durdur()
    {
        if (is.joinable()) {
            dur.store(true, std::memory_order_release);
            is.join();
        }
    }

    void halkalari_bosalt()
    {
        for (;;) {
            bool is_var = false;
            for (auto& u : uretici_ic) {
                is_var = bosalt(*u, kTurButcesi) || is_var;
            }
            if (!is_var) {
                break;
            }
        }
    }

    KapanisSonucu kapat(const KapanisAlanlari& a)
    {
        is_parcacigini_durdur();
        if (!kapanis_yapildi) {
            // Verilen alanlar HER kapanış yolunda kullanılır: bu çağrı sırasında sınıra ya da yazma
            // hatasına varılırsa kapanış kaydı eski taslağı değil bunları taşır (A6-K53: canlı
            // müdahale adımı kaybolursa güvence yanlış yazılır).
            const std::lock_guard<std::mutex> kl(kilit);
            taslak = a;
        }
        // Üreticiler durmuş olmalıdır. Yeni ileti kabulü burada kesilir: kurala uymayan bir üretici
        // kapanışı sonsuza dek süremez; halkalarda bekleyenler yazılır.
        ortak.kabul.store(false, std::memory_order_release);
        halkalari_bosalt();
        if (acik_mi()) {
            guncellemeleri_yaz();
        }
        if (acik_mi()) {
            duzgun_kapanis(a, a.bitis_durumu, YaziciDurumu::kapandi, {});
        }
        // Dosya kapandıktan SONRA halkaya giren ileti (kabul kesilirken yazmakta olan bir üretici)
        // dosyaya yazılamaz: sayılarak atılır (`kapali_iken_atilan`).
        halkalari_bosalt();
        sonuc.sayaclar = sayaclar();
        return sonuc;
    }
};

}  // namespace ic

// ================= Üretici =================

bool Uretici::etkin_(std::uint16_t kanal) const noexcept
{
    return ic_->ortak->etkin[kanal];
}

void Uretici::suzuldu_() noexcept
{
    ic::art(ic_->suzulen);
}

std::int64_t Uretici::duvar_ns() const noexcept
{
    return ic_->ortak->saat() - ic_->ortak->saat_sifiri;
}

Kademe Uretici::kademe() const noexcept
{
    return ic_->ortak->kademe;
}

std::string_view Uretici::ad() const noexcept
{
    return ic_->ad;
}

UreticiSayaclari Uretici::sayaclar() const noexcept
{
    UreticiSayaclari s;
    s.uretilen = ic_->uretilen.load(std::memory_order_acquire);
    s.kuyruklanan = ic_->kuyruklanan.load(std::memory_order_acquire);
    s.dusen_halka = ic_->dusen_halka.load(std::memory_order_acquire);
    s.dusen_gecersiz = ic_->dusen_gecersiz.load(std::memory_order_acquire);
    s.suzulen = ic_->suzulen.load(std::memory_order_acquire);
    s.kapali_atilan = ic_->kapali_atilan.load(std::memory_order_acquire);
    return s;
}

std::uint8_t* Uretici::ayir_(std::uint16_t kanal, std::int64_t sim_t_ns, std::uint64_t adim, std::size_t n,
                             const std::int64_t* duvar_ns, YazmaSonucu& s) noexcept
{
    ic::UreticiIc& u = *ic_;
    if (!u.ortak->kabul.load(std::memory_order_acquire)) {
        ic::art(u.kapali_atilan);
        s = YazmaSonucu::kapali;
        return nullptr;
    }
    ic::art(u.uretilen);
    if (sim_t_ns < 0) {
        // A6-K10: `log_time`'ı negatif olacak ileti yazılmaz; `kayit/eksik`'te yazma_hatasi.
        u.dusur(kanal, ic::kNedenYazmaHatasi, sim_t_ns, adim);
        ic::art(u.dusen_gecersiz);
        s = YazmaSonucu::dustu_gecersiz;
        return nullptr;
    }
    std::uint8_t* p = u.halka.ayir(ic::kKayitBasligi + n, u.ortak->paysiz[kanal] ? 0 : u.pay);
    if (p == nullptr) {
        u.dusur(kanal, ic::kNedenHalkaTasmasi, sim_t_ns, adim);
        ic::art(u.dusen_halka);
        s = YazmaSonucu::dustu_halka;
        return nullptr;
    }
    const std::int64_t duvar = duvar_ns != nullptr ? *duvar_ns : u.ortak->saat() - u.ortak->saat_sifiri;
    std::memcpy(p, &kanal, 2);
    std::memset(p + 2, 0, 6);
    ic::yaz64(p + 8, static_cast<std::uint64_t>(sim_t_ns));
    ic::yaz64(p + 16, static_cast<std::uint64_t>(duvar));
    ic::yaz64(p + 24, adim);
    return p + ic::kKayitBasligi;
}

void Uretici::kodlanamadi_(std::uint16_t kanal, std::int64_t sim_t_ns, std::uint64_t adim) noexcept
{
    // Ayrılan yer yayınlanmaz; bir sonraki `ayir` onu yeniden kullanır.
    ic_->dusur(kanal, ic::kNedenYazmaHatasi, sim_t_ns, adim);
    ic::art(ic_->dusen_gecersiz);
}

void Uretici::yayinla_() noexcept
{
    ic_->halka.yayinla();
    ic::art(ic_->kuyruklanan);
}

// ================= Yazıcı =================

Yazici::Yazici() : ic_(std::make_unique<ic::YaziciIc>()) {}

Yazici::~Yazici()
{
    if (!ic_) {
        return;
    }
    ic_->is_parcacigini_durdur();
    if (!ic_->kapanis_yapildi) {
        KapanisAlanlari a = ic_->taslak_al();
        a.bitis_durumu = BitisDurumu::hata;
        (void)ic_->kapat(a);
    }
}

std::unique_ptr<Yazici> Yazici::ac(const Ayar& ayar, const AcilisUstVerisi& ust_veri, std::span<const Ek> ekler_,
                                   std::string& hata)
{
    if (ayar.ureticiler.empty()) {
        hata = "en az bir üretici gerekir";
        return nullptr;
    }
    if (ayar.parca_hedefi_bayt == 0 || ayar.parca_kapatma_ns <= 0 || ayar.yoklama_araligi_ns <= 0 || ayar.ilk_sira == 0) {
        hata = "geçersiz ayar";
        return nullptr;
    }

    // ---- üst veri (A6 §3.5) ----
    AcilisUstVerisi uv = ust_veri;
    if (uv.a6_surumu.empty()) {
        uv.a6_surumu = std::string(kA6Surumu);
    } else if (uv.a6_surumu != kA6Surumu) {
        hata = "a6_surumu bu yazıcının sürümü değil";
        return nullptr;
    }
    if (uv.kayit_kademesi.empty()) {
        uv.kayit_kademesi = std::string(kademe_adi(ayar.kademe));
    } else if (uv.kayit_kademesi != kademe_adi(ayar.kademe)) {
        hata = "kayit_kademesi ayardaki kademeyle aynı değil";
        return nullptr;
    }
    if (uv.kamera_kareleri.empty()) {
        uv.kamera_kareleri = "kapali";
    } else if (uv.kamera_kareleri != "kapali") {
        hata = "kamera kareleri bu sürümde kaydedilmez (kamera_kareleri = kapali olmalı)";  // W5-K37
        return nullptr;
    }
    if (uv.kayit_duvar_saati.empty()) {
        uv.kayit_duvar_saati = "monoton_ns_kayit_acilisindan";
    }
    if (uv.bagimliliklar.find("mcap=") == std::string::npos) {
        if (!uv.bagimliliklar.empty()) {
            uv.bagimliliklar.push_back(',');
        }
        uv.bagimliliklar += bagimlilik_surumleri();  // A6-K03
    }
    if (!run_id_gecerli(uv.run_id)) {
        hata = "run_id biçimi geçersiz (A3-K63)";
        return nullptr;
    }
    if (!kip_gecerli(uv.kip)) {
        hata = "kip geçersiz";
        return nullptr;
    }
    UstVeri acilis;
    {
        const auto tablo = acilis_anahtarlari();
        std::size_t i = 0;
        for (const auto& [anahtar, deger] : ust_veri_ciftleri(uv)) {
            // Değeri bilinmeyen koşullu anahtar yazılmaz (A6-K46); zorunlu anahtar her zaman yazılır.
            if (!deger.empty() || tablo[i].zorunlu) {
                acilis.emplace(std::string(anahtar), std::string(deger));
            }
            ++i;
        }
    }
    if (!ust_veri_denetle(kUstVeriAcilis, acilis, hata)) {
        return nullptr;
    }

    // ---- ekler (A6 §3.6) ----
    const auto ek_tablosu = ekler();
    for (std::size_t i = 0; i < ekler_.size(); ++i) {
        const Ek& e = ekler_[i];
        const bool bilinen = std::any_of(ek_tablosu.begin(), ek_tablosu.end(),
                                         [&](const EkTanimi& t) { return t.ad == e.ad; });
        if (!bilinen || e.ad == kKatalogEki) {
            hata = "bilinmeyen ya da yazıcının kendi eklediği ek: " + e.ad;
            return nullptr;
        }
        for (std::size_t j = 0; j < i; ++j) {
            if (ekler_[j].ad == e.ad) {
                hata = "ek iki kez verildi: " + e.ad;
                return nullptr;
            }
        }
    }
    for (const EkTanimi& t : ek_tablosu) {
        if (!t.zorunlu || t.ad == kKatalogEki) {
            continue;
        }
        if (std::none_of(ekler_.begin(), ekler_.end(), [&](const Ek& e) { return e.ad == t.ad; })) {
            hata = "zorunlu ek yok: " + std::string(t.ad);
            return nullptr;
        }
    }

    std::unique_ptr<Yazici> y(new Yazici());
    ic::YaziciIc& c = *y->ic_;
    c.ayar = ayar;
    c.kip = uv.kip;
    c.guvencesiz_baslatma = !uv.guvencesiz_nedenler.empty();
    c.bilinen = acilis;
    c.ortak.kademe = ayar.kademe;
    c.ortak.saat = ayar.saat ? ayar.saat : std::function<std::int64_t()>([] {
        return static_cast<std::int64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(
                                             std::chrono::steady_clock::now().time_since_epoch())
                                             .count());
    });
    c.sonraki_sira = ayar.ilk_sira;
    c.saglam_sira = ayar.ilk_sira;

    // ---- kanallar ----
    const auto katalog = kanallar();
    const auto sema_tablosu = semalar();
    std::size_t tanim_bayt = 0;  // dosyaya en çok bir kez girecek şema ve kanal kayıtlarının boyu (üst sınır)
    for (std::size_t k = 0; k < katalog.size(); ++k) {
        c.ortak.etkin[k] = kanal_etkin(katalog[k], ayar.kademe, c.kip);
        c.ortak.paysiz[k] = katalog[k].kademe == Kademe::olaylar;
        if (c.ortak.etkin[k]) {
            tanim_bayt += sema_tablosu[katalog[k].sema].veri.size() + sema_tablosu[katalog[k].sema].ad.size() +
                          katalog[k].ad.size() + 256;
        }
        if (katalog[k].ad == "kayit/eksik") {
            c.eksik_kanali = static_cast<std::uint16_t>(k);
        }
    }
    if (!c.ortak.etkin[c.eksik_kanali]) {
        hata = "katalogda `kayit/eksik` kanalı yok";
        return nullptr;
    }
    // `kayit/eksik` kaydı her kanal için kodlanabilmelidir (kanal adı yükün `kanal` alanına sığar):
    // düşme bildirimi kodlanamadığı için kaybolamaz.
    for (const KanalTanimi& k : katalog) {
        EksikKaydi e;
        e.kanal = &k;
        std::array<std::uint8_t, 128> tampon{};
        if (ic::YaziciIc::eksik_kodla(e, tampon) == 0) {
            hata = "`kayit/eksik` kaydı kodlanamıyor: " + std::string(k.ad);
            return nullptr;
        }
    }

    // ---- üreticiler ----
    std::size_t azami_yuk = 0;
    for (const UreticiAyari& ua : ayar.ureticiler) {
        c.uretici_ic.push_back(std::make_unique<ic::UreticiIc>(ua.ad, ua.halka_bayt, &c.ortak));
        c.ureticiler.emplace_back(c.uretici_olustur(c.uretici_ic.back().get()));
        azami_yuk = std::max(azami_yuk, c.uretici_ic.back()->halka.azami_yuk());
    }
    // A6-K12: kapanıştaki `kayit/eksik` kayıtlarına ayrılan sıra numaraları. Üretici iletisi yalnız
    // kalan sıra bu yedekten BÜYÜKKEN yazılır; ondan sonra en çok şunlar yazılır: son turdaki düşme
    // bildirimleri (üretici × kanal × neden), kapanıştaki düşme bildirimleri (aynı sayı) ve halkalarda
    // kalıp yazılamayan iletilerin bildirimi (kanal başına bir kayıt; pay üretici × kanal). Hata
    // kapanışının kuyruğu bu kayıtların numaralarını son sağlam parçadan yeniden verir ve fazladan en
    // çok kanal başına bir kayıt ister (açık parçada kaybolan her kanalın en az bir iletisi vardı).
    const std::uint64_t uretici_sayisi = ayar.ureticiler.size();
    c.sira_yedegi = 2u * uretici_sayisi * kKanalSayisi * ic::kUreticiNedeni + uretici_sayisi * kKanalSayisi + 16u;

    // ---- dosya ----
    std::error_code ec;
    std::filesystem::create_directories(ayar.dizin, ec);
    c.yol = ayar.dizin / (uv.run_id + ".mcap");
    c.dosya = ayar.dosya_fabrikasi ? ayar.dosya_fabrikasi(c.yol, hata) : yerel_dosya_olustur(c.yol, hata);
    if (!c.dosya) {
        if (hata.empty()) {
            hata = "dosya oluşturulamadı: " + ic::yol_metni(c.yol);
        }
        return nullptr;
    }
    c.cikti.bagla(c.dosya.get());
    c.ortak.saat_sifiri = c.ortak.saat();  // kayıt duvar saati: açılışta 0 (A6-K13)

    // Parça sınırını yazıcı yönetir (`parca_hedefi_bayt` aşılınca ve süre dolunca kapatır): kitaplığın
    // kendi eşiği hiçbir iletide dolmayacak kadar büyük verilir; böylece hangi iletinin hangi parçada
    // olduğu kesin bilinir (yazma hatasında kayıp dökümü).
    const std::uint64_t kitaplik_esigi = ayar.parca_hedefi_bayt + tanim_bayt + azami_yuk + ic::kIletiEkYuku + 4096u;
    c.mcap.ac(c.cikti, kMcapProfili, kitaplik_esigi);

    std::vector<std::uint16_t> sema_kimligi(sema_tablosu.size(), 0);
    c.sema_yazildi.assign(sema_tablosu.size(), false);
    for (std::size_t k = 0; k < katalog.size(); ++k) {
        if (!c.ortak.etkin[k]) {
            continue;
        }
        const SemaTanimi& s = sema_tablosu[katalog[k].sema];
        if (sema_kimligi[katalog[k].sema] == 0) {
            sema_kimligi[katalog[k].sema] = c.mcap.sema_ekle(s.ad, kSemaKodlamasi, s.veri);
        }
        ic::AnahtarDeger metadata;
        metadata.emplace_back("kanal_surumu", std::to_string(katalog[k].kanal_surumu));
        metadata.emplace_back("zaman_anlami", std::string(katalog[k].zaman_anlami));
        metadata.emplace_back("kademe", std::string(katalog[k].kademe_adi));
        c.mcap_kanal[k] = c.mcap.kanal_ekle(katalog[k].ad, kIletiKodlamasi, sema_kimligi[katalog[k].sema], metadata);
        // MCAP kayıt boyları (A6 §3.2): şema = 9 + kimlik + üç uzunluk önekli alan; kanal = 9 + iki
        // kimlik + iki dize + uzunluk önekli eşlem.
        std::size_t eslem = 0;
        for (const auto& [a, v] : metadata) {
            eslem += 4 + a.size() + 4 + v.size();
        }
        c.kanal_semasi[k] = katalog[k].sema;
        c.sema_kayit_bayt[k] = static_cast<std::uint32_t>(9 + 2 + 4 + s.ad.size() + 4 + kSemaKodlamasi.size() + 4 + s.veri.size());
        c.kanal_kayit_bayt[k] =
            static_cast<std::uint32_t>(9 + 2 + 2 + 4 + katalog[k].ad.size() + 4 + kIletiKodlamasi.size() + 4 + eslem);
        if (k == c.eksik_kanali) {
            c.kuyruk_kanali.sema_id = sema_kimligi[katalog[k].sema];
            c.kuyruk_kanali.kanal_id = c.mcap_kanal[k];
            c.kuyruk_kanali.sema_adi = std::string(s.ad);
            c.kuyruk_kanali.sema_kodlamasi = std::string(kSemaKodlamasi);
            c.kuyruk_kanali.sema_verisi = std::string(s.veri);
            c.kuyruk_kanali.konu = std::string(katalog[k].ad);
            c.kuyruk_kanali.ileti_kodlamasi = std::string(kIletiKodlamasi);
            c.kuyruk_kanali.metadata = metadata;
        }
    }

    // ---- açılış kaydı ve ekler: ilk iletiden önce (A6-K46) ----
    bool tamam = c.mcap.ust_veri_yaz(kUstVeriAcilis, ic::ciftler(acilis));
    for (const Ek& e : ekler_) {
        const auto t = std::find_if(ek_tablosu.begin(), ek_tablosu.end(), [&](const EkTanimi& x) { return x.ad == e.ad; });
        tamam = tamam && c.mcap.ek_yaz(e.ad, t->ortam_turu, 0, 0, e.veri.data(), e.veri.size());
    }
    {
        const auto t = std::find_if(ek_tablosu.begin(), ek_tablosu.end(),
                                    [&](const EkTanimi& x) { return x.ad == kKatalogEki; });
        const auto kb = katalog_baytlari();
        if (t == ek_tablosu.end()) {
            hata = "katalogda `a6_kanallar.json` eki yok";
            tamam = false;
        } else {
            tamam = tamam && c.mcap.ek_yaz(kKatalogEki, t->ortam_turu, 0, 0, kb.data(), kb.size());  // A6-K50
        }
    }
    c.cikti.bosalt();
    if (!tamam || !c.cikti.saglam()) {
        if (hata.empty()) {
            hata = "dosya başlığı yazılamadı: " + ic::yol_metni(c.yol);
        }
        c.mcap.birak();
        c.dosya->kapat();
        std::filesystem::remove(c.yol, ec);  // yarım dosya bırakılmaz; çalıştırma başlamadı
        return nullptr;
    }
    c.saglam_noktasi();
    c.s_dosya_bayt.store(c.saglam_boyut, std::memory_order_release);

    c.kapanis_yapildi = false;
    if (ayar.is_parcacigi) {
        c.is = std::thread([&c] { c.is_dongusu(); });
    }
    return y;
}

std::size_t Yazici::uretici_sayisi() const noexcept
{
    return ic_->ureticiler.size();
}

Uretici& Yazici::uretici(std::size_t i) noexcept
{
    return *ic_->ureticiler[i];
}

const std::filesystem::path& Yazici::yol() const noexcept
{
    return ic_->yol;
}

Kademe Yazici::kademe() const noexcept
{
    return ic_->ortak.kademe;
}

std::int64_t Yazici::duvar_ns() const noexcept
{
    return ic_->duvar_ns();
}

YaziciDurumu Yazici::durum() const noexcept
{
    return ic_->durum.load(std::memory_order_acquire);
}

YaziciSayaclari Yazici::sayaclar() const noexcept
{
    return ic_->sayaclar();
}

bool Yazici::guncelle(std::string_view anahtar, std::string_view deger, std::string& hata)
{
    UstVeri tek;
    tek.emplace(std::string(anahtar), std::string(deger));
    if (!ust_veri_denetle(kUstVeriGuncelleme, tek, hata)) {
        return false;
    }
    const std::lock_guard<std::mutex> kl(ic_->kilit);
    if (!ic_->acik_mi()) {
        hata = "dosya kapalı";
        return false;
    }
    const auto it = ic_->bilinen.find(anahtar);
    if (it != ic_->bilinen.end() && !it->second.empty()) {
        if (it->second == deger) {
            return true;  // aynı değer: etkisiz (A6-K46)
        }
        hata = "`" + std::string(anahtar) + "` dolu; farklı değerle değiştirilemez (A6-K46)";
        return false;
    }
    ic_->bilinen.insert_or_assign(std::string(anahtar), std::string(deger));
    ic_->bekleyen_guncelleme.emplace_back(std::string(anahtar), std::string(deger));
    return true;
}

void Yazici::kapanis_taslagi(const KapanisAlanlari& taslak)
{
    const std::lock_guard<std::mutex> kl(ic_->kilit);
    ic_->taslak = taslak;
}

bool Yazici::isle()
{
    if (ic_->ayar.is_parcacigi) {
        return false;  // dosyaya yalnız yazıcının iş parçacığı dokunur (A6-K05)
    }
    return ic_->tur();
}

KapanisSonucu Yazici::kapat(const KapanisAlanlari& alanlar)
{
    return ic_->kapat(alanlar);
}

}  // namespace ida::kayit
