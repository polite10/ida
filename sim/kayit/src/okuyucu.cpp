// okuyucu.cpp — A6 kayıt okuyucusu: kendi MCAP kayıt ayrıştırıcısı, CRC doğrulaması (A6-K59),
// kurtarma okuması (A6-K60), üst veri birleştirmesi (A6-K46), sürüm kuralları (A6-K48, K49) ve
// `log_time`/`sequence` sıralı okuma (A6-K11).
//
// Biçim: https://mcap.dev/spec (A6 §3.2). Bütün tamsayılar küçük uçlu; kayıt = opcode (u8) + uzunluk
// (u64) + gövde; dize = u32 uzunluk + bayt; eşlem = u32 bayt uzunluğu + (dize, dize) çiftleri.
//
// Girdi güvenilmezdir (bozuk, yarım ya da kasıtlı dosya): her uzunluk dosya boyuyla ve ayardaki üst
// sınırlarla denetlenir; denetimden geçmeyen hiçbir uzunlukla bellek ayrılmaz.
#include "ida/kayit/okuyucu.hpp"

#include "crc32.hpp"
#include "ida/a3/kodlayici.hpp"
#include "ida/kayit/coz.hpp"

#include <zstd.h>

#include <algorithm>
#include <array>
#include <charconv>
#include <cstring>
#include <fstream>
#include <limits>
#include <map>
#include <optional>
#include <queue>
#include <set>
#include <utility>

namespace ida::kayit {

namespace ic {

namespace {

constexpr std::array<std::uint8_t, 8> kSihir = {0x89, 'M', 'C', 'A', 'P', 0x30, '\r', '\n'};
constexpr std::uint64_t kKayitBasligi = 9;  // opcode + uzunluk
constexpr std::uint64_t kFooterGovdesi = 20;
constexpr std::uint64_t kKucukKayitSiniri = std::uint64_t{16} << 20;  // Header, Schema, Channel
constexpr std::size_t kAkisBlogu = std::size_t{1} << 20;
constexpr std::size_t kUyariSiniri = 256;
// İki parça arasında (ya da ilk parçadan önce) bekleyen üst düzey kayıt sayısı için üst sınır. Yazıcı
// burada en çok açılış kaydını, ekleri ve birkaç güncellemeyi bırakır; sınırı aşan dosya bozuktur.
constexpr std::size_t kBekleyenSiniri = 4096;

enum : std::uint8_t {
    kOpHeader = 0x01,
    kOpFooter = 0x02,
    kOpSchema = 0x03,
    kOpChannel = 0x04,
    kOpMessage = 0x05,
    kOpChunk = 0x06,
    kOpAttachment = 0x09,
    kOpMetadata = 0x0C,
    kOpDataEnd = 0x0F,
};

// ---- bayt kaynağı ----
class Kaynak {
public:
    virtual ~Kaynak() = default;
    virtual std::uint64_t boyut() const = 0;
    // [konum, konum + n) aralığını okur; aralık dosyanın içinde değilse ya da okunamıyorsa false.
    virtual bool oku(std::uint64_t konum, std::uint8_t* hedef, std::size_t n) = 0;
};

class BellekKaynagi final : public Kaynak {
public:
    explicit BellekKaynagi(std::span<const std::uint8_t> v) : v_(v) {}
    std::uint64_t boyut() const override { return v_.size(); }
    bool oku(std::uint64_t konum, std::uint8_t* hedef, std::size_t n) override
    {
        if (konum > v_.size() || n > v_.size() - konum) {
            return false;
        }
        if (n > 0) {
            std::memcpy(hedef, v_.data() + konum, n);
        }
        return true;
    }

private:
    std::span<const std::uint8_t> v_;
};

class DosyaKaynagi final : public Kaynak {
public:
    bool ac(const std::filesystem::path& yol)
    {
        std::error_code ec;
        const auto b = std::filesystem::file_size(yol, ec);
        if (ec) {
            return false;
        }
        boyut_ = static_cast<std::uint64_t>(b);
        akis_.open(yol, std::ios::binary | std::ios::in);
        return akis_.is_open();
    }
    std::uint64_t boyut() const override { return boyut_; }
    bool oku(std::uint64_t konum, std::uint8_t* hedef, std::size_t n) override
    {
        if (konum > boyut_ || n > boyut_ - konum) {
            return false;
        }
        if (n == 0) {
            return true;
        }
        akis_.clear();
        akis_.seekg(static_cast<std::streamoff>(konum), std::ios::beg);
        akis_.read(reinterpret_cast<char*>(hedef), static_cast<std::streamsize>(n));
        return akis_.gcount() == static_cast<std::streamsize>(n);
    }

private:
    std::ifstream akis_;
    std::uint64_t boyut_ = 0;
};

// ---- alan okuma ----
using Alan = a3::Okuyucu;

bool dize_oku(Alan& r, std::string_view& s) noexcept
{
    const std::uint32_t n = r.u32();
    const std::uint8_t* p = r.baytlar(n);
    if (!r.tamam()) {
        return false;
    }
    s = std::string_view(reinterpret_cast<const char*>(p), p == nullptr ? 0u : n);
    return true;
}

bool eslem_oku(Alan& r, UstVeri& m)
{
    const std::uint32_t bayt = r.u32();
    const std::uint8_t* p = r.baytlar(bayt);
    if (!r.tamam()) {
        return false;
    }
    Alan ic(p, bayt);
    while (ic.kalan() > 0) {
        std::string_view k;
        std::string_view v;
        if (!dize_oku(ic, k) || !dize_oku(ic, v)) {
            return false;
        }
        m.insert_or_assign(std::string(k), std::string(v));
    }
    return true;
}

bool onluk_coz(std::string_view s, std::uint64_t& v) noexcept
{
    if (s.empty()) {
        return false;
    }
    const auto r = std::from_chars(s.data(), s.data() + s.size(), v, 10);
    return r.ec == std::errc() && r.ptr == s.data() + s.size();
}

// ---- dosyadan çözülen kayıtlar ----
struct SemaKaydi {
    std::uint16_t id = 0;
    std::string ad;
    std::string kodlama;
    std::string veri;
    bool operator==(const SemaKaydi&) const = default;
};

struct KanalKaydi {
    std::uint16_t id = 0;
    std::uint16_t sema_id = 0;
    std::string konu;
    std::string ileti_kodlamasi;
    UstVeri metadata;
    bool operator==(const KanalKaydi&) const = default;
};

struct UstVeriKaydi {
    std::string ad;
    UstVeri icerik;
};

struct EkKaydi {
    std::string ad;
    std::string ortam_turu;
    std::uint64_t kayit_konumu = 0;  // kayıt gövdesinin dosya konumu
    std::uint64_t kayit_boyu = 0;    // gövde boyu (CRC dahil)
    std::uint64_t veri_konumu = 0;
    std::uint64_t veri_boyu = 0;
};

struct HamIleti {
    std::int64_t log_time = 0;
    std::int64_t publish_time = 0;
    std::uint32_t sequence = 0;
    std::uint16_t kanal_id = 0;
    std::uint32_t konum = 0;  // parçanın sıkıştırmasız verisi içinde yükün başı
    std::uint32_t boy = 0;
};

// Bir parçanın açılmış içeriği.
struct ParcaIcerigi {
    std::vector<std::uint8_t> veri;  // sıkıştırmasız kayıtlar
    std::vector<SemaKaydi> semalar;
    std::vector<KanalKaydi> kanallar;
    std::vector<HamIleti> iletiler;
};

struct ParcaOzeti {
    std::uint64_t konum = 0;  // kayıt gövdesinin dosya konumu
    std::uint64_t boy = 0;    // gövde boyu
    std::uint64_t acik_boy = 0;
    std::int64_t en_kucuk_t = 0;  // okunur iletilerin en küçük `log_time`'ı
    std::uint64_t okunur_ileti = 0;
};

bool sema_coz(const std::uint8_t* p, std::size_t n, SemaKaydi& s)
{
    Alan r(p, n);
    s.id = r.u16();
    std::string_view ad;
    std::string_view kodlama;
    if (!dize_oku(r, ad) || !dize_oku(r, kodlama)) {
        return false;
    }
    const std::uint32_t veri_boyu = r.u32();
    const std::uint8_t* v = r.baytlar(veri_boyu);
    if (!r.tamam() || r.kalan() != 0 || s.id == 0) {
        return false;
    }
    s.ad = std::string(ad);
    s.kodlama = std::string(kodlama);
    s.veri.assign(reinterpret_cast<const char*>(v), v == nullptr ? 0u : veri_boyu);
    return true;
}

bool kanal_coz(const std::uint8_t* p, std::size_t n, KanalKaydi& k)
{
    Alan r(p, n);
    k.id = r.u16();
    k.sema_id = r.u16();
    std::string_view konu;
    std::string_view kodlama;
    if (!dize_oku(r, konu) || !dize_oku(r, kodlama) || !eslem_oku(r, k.metadata)) {
        return false;
    }
    if (!r.tamam() || r.kalan() != 0) {
        return false;
    }
    k.konu = std::string(konu);
    k.ileti_kodlamasi = std::string(kodlama);
    return true;
}

// Dosyadaki bir kanalın okuma durumu.
struct KanalDurumu {
    KanalKaydi kayit;
    const KanalTanimi* tanim = nullptr;
    std::uint16_t kanal_surumu = 0;
    bool okunur = false;
    std::uint64_t ileti_sayisi = 0;
    std::string uyari;
};

struct YiginOgesi {
    std::int64_t log_time = 0;
    std::uint32_t sequence = 0;
    std::size_t parca = 0;  // dosya sırası
    bool operator>(const YiginOgesi& o) const noexcept
    {
        if (log_time != o.log_time) {
            return log_time > o.log_time;
        }
        if (sequence != o.sequence) {
            return sequence > o.sequence;
        }
        return parca > o.parca;
    }
};

struct AcikParca {
    ParcaIcerigi icerik;
    std::size_t sira = 0;
};

}  // namespace

struct OkuyucuIc {
    OkumaAyari ayar;
    std::unique_ptr<Kaynak> kaynak;
    DosyaOzeti ozet;
    ZSTD_DCtx* zstd = nullptr;

    // ---- tarama durumu ----
    std::map<std::uint16_t, SemaKaydi> semalar;
    std::map<std::uint16_t, KanalDurumu> kanallar;
    std::vector<ParcaOzeti> parcalar;
    std::vector<EkKaydi> ekler;
    std::set<std::string, std::less<>> ek_adlari;
    std::vector<bool> yerel_kanal;  // parça içinde tanımlanan kanal kimlikleri (parca_coz'un çalışma alanı)
    bool dur = false;               // tarama ilk bozuk parçada durdu
    bool ust_veri_cozulemedi = false;
    bool kapanis_goruldu = false;
    UstVeri kapanis;
    std::vector<std::uint8_t> ham;  // parça gövdesi için yeniden kullanılan tampon

    // Parçalar arasında bekleyen üst düzey kayıtlar (yalnız geçerli bir parçayla ya da dosyanın
    // düzgün sonuyla kesinleşir: A6-K60 "kurtarma sonu").
    struct Bekleyen {
        enum class Tur : std::uint8_t { sema, kanal, ust_veri, ek } tur = Tur::sema;
        SemaKaydi sema;
        KanalKaydi kanal;
        UstVeriKaydi ust_veri;
        EkKaydi ek;
        std::uint64_t konum = 0;
    };
    std::vector<Bekleyen> bekleyen;

    // ---- sıralı okuma ----
    std::vector<bool> secili;            // kanallar() indeksine göre; boş: hepsi
    std::vector<std::size_t> zaman_sirasi;
    std::size_t sonraki_parca = 0;
    std::map<std::size_t, AcikParca> acik;
    std::uint64_t acik_bayt = 0;
    std::priority_queue<YiginOgesi, std::vector<YiginOgesi>, std::greater<>> yigin;
    std::optional<std::size_t> biten_parca;
    std::string okuma_hatasi;

    ~OkuyucuIc()
    {
        if (zstd != nullptr) {
            ZSTD_freeDCtx(zstd);
        }
    }

    // Uyarılar sınırlıdır: bozuk bir dosya kayıt başına bir uyarıyla belleği büyütemez.
    void uyar(std::string ileti)
    {
        if (ozet.uyarilar.size() < kUyariSiniri) {
            ozet.uyarilar.push_back(std::move(ileti));
        } else if (ozet.uyarilar.size() == kUyariSiniri) {
            ozet.uyarilar.emplace_back("uyarı sınırına ulaşıldı; sonraki uyarılar yazılmadı");
        }
    }

    void bozuk(std::string aciklama, std::uint64_t konum)
    {
        if (!ozet.bozuk) {
            ozet.bozuk = true;
            ozet.bozulma = std::move(aciklama);
            ozet.bozulma_konumu = konum;
        }
    }

    // ---- kayıtların kesinleşmesi ----
    void kanal_degerlendir(KanalDurumu& d)
    {
        const KanalKaydi& k = d.kayit;
        d.tanim = kanal_bul(k.konu);
        d.okunur = false;
        if (d.tanim == nullptr) {
            d.uyari = "bilinmeyen kanal (atlanır; A6-K48)";
            return;
        }
        const auto surum = k.metadata.find("kanal_surumu");
        std::uint64_t v = 0;
        if (surum == k.metadata.end() || !onluk_coz(surum->second, v) || v == 0 || v > 0xFFFFu) {
            d.uyari = "kanal üst verisinde geçerli `kanal_surumu` yok";
            return;
        }
        d.kanal_surumu = static_cast<std::uint16_t>(v);
        if (k.ileti_kodlamasi != kIletiKodlamasi) {
            d.uyari = "ileti kodlaması `" + std::string(kIletiKodlamasi) + "` değil";
            return;
        }
        const auto s = semalar.find(k.sema_id);
        if (s == semalar.end() || s->second.kodlama != kSemaKodlamasi) {
            d.uyari = "şema kaydı yok ya da kodlaması `" + std::string(kSemaKodlamasi) + "` değil";
            return;
        }
        if (d.kanal_surumu == d.tanim->kanal_surumu) {
            // A6-K49: aynı `kanal_surumu`'nda şema `data`'sı bilinen tanımla aynı olmalı.
            const SemaTanimi& bilinen = semalar_tablosu()[d.tanim->sema];
            if (s->second.ad != bilinen.ad || s->second.veri != bilinen.veri) {
                d.uyari = "şema `data`'sı bilinen tanımla aynı değil (A6-K49); kanal okunmaz";
                return;
            }
        }
        d.okunur = true;
    }

    static std::span<const SemaTanimi> semalar_tablosu() noexcept { return ::ida::kayit::semalar(); }

    bool sema_kesinlestir(const SemaKaydi& s, std::uint64_t konum)
    {
        const auto it = semalar.find(s.id);
        if (it != semalar.end()) {
            if (!(it->second == s)) {
                bozuk("şema kimliği farklı içerikle yeniden tanımlandı", konum);
                return false;
            }
            return true;
        }
        semalar.emplace(s.id, s);
        return true;
    }

    bool kanal_kesinlestir(const KanalKaydi& k, std::uint64_t konum)
    {
        const auto it = kanallar.find(k.id);
        if (it != kanallar.end()) {
            if (!(it->second.kayit == k)) {
                bozuk("kanal kimliği farklı içerikle yeniden tanımlandı", konum);
                return false;
            }
            return true;
        }
        KanalDurumu d;
        d.kayit = k;
        kanal_degerlendir(d);
        if (!d.uyari.empty()) {
            uyar("kanal `" + k.konu + "`: " + d.uyari);
        }
        kanallar.emplace(k.id, std::move(d));
        return true;
    }

    void ust_veri_kesinlestir(const UstVeriKaydi& u, std::uint64_t konum)
    {
        if (u.ad == kUstVeriAcilis) {
            if (ozet.ust_veri_var) {
                bozuk("ikinci `ida_kayit` açılış kaydı (A6-K46)", konum);
                return;
            }
            ozet.ust_veri_var = true;
            for (const auto& [k, v] : u.icerik) {
                ozet.ust_veri.insert_or_assign(k, v);
            }
            return;
        }
        if (u.ad == kUstVeriGuncelleme) {
            const auto tablo = guncelleme_anahtarlari();
            for (const auto& [k, v] : u.icerik) {
                const bool listede =
                    std::any_of(tablo.begin(), tablo.end(), [&](const UstVeriAnahtari& a) { return a.ad == k; });
                if (!listede || v.empty()) {
                    uyar("`ida_kayit_guncelleme`: kapalı liste dışı ya da boş anahtar `" + k +
                                            "` yok sayıldı");
                    continue;
                }
                const auto it = ozet.ust_veri.find(k);
                if (it == ozet.ust_veri.end() || it->second.empty()) {
                    ozet.ust_veri.insert_or_assign(k, v);  // yalnız yok ya da boş olanı doldurur
                } else if (it->second != v) {
                    bozuk("güncelleme dolu anahtarı (`" + k + "`) farklı değerle değiştiriyor (A6-K46)", konum);
                    return;
                }
            }
            return;
        }
        if (u.ad == kUstVeriKapanis) {
            if (kapanis_goruldu) {
                uyar("ikinci `ida_kayit_kapanis` kaydı yok sayıldı");
                return;
            }
            kapanis_goruldu = true;
            kapanis = u.icerik;
        }
    }

    void ek_kesinlestir(const EkKaydi& e)
    {
        if (!ek_adlari.insert(e.ad).second) {
            uyar("ek `" + e.ad + "` ikinci kez yazılmış; ilki kullanılır");
            return;
        }
        ekler.push_back(e);
    }

    // Bekleyen kayıtları dosya sırasıyla uygular. Yapısal çelişkide false (tarama durur).
    bool bekleyenleri_kesinlestir()
    {
        bool tamam = true;
        for (const Bekleyen& b : bekleyen) {
            switch (b.tur) {
            case Bekleyen::Tur::sema:
                tamam = sema_kesinlestir(b.sema, b.konum) && tamam;
                break;
            case Bekleyen::Tur::kanal:
                tamam = kanal_kesinlestir(b.kanal, b.konum) && tamam;
                break;
            case Bekleyen::Tur::ust_veri:
                ust_veri_kesinlestir(b.ust_veri, b.konum);
                break;
            case Bekleyen::Tur::ek:
                ek_kesinlestir(b.ek);
                break;
            }
        }
        bekleyen.clear();
        return tamam;
    }

    // ---- parça ----
    // Parça kaydını okur, açar, CRC'sini doğrular (A6-K59) ve içindeki kayıtları ayrıştırır. Hata
    // varsa false ve açıklama; `p` o durumda kullanılmaz.
    bool parca_coz(std::uint64_t konum, std::uint64_t boy, ParcaIcerigi& p, std::string& hata)
    {
        // Sabit alanlar: zamanlar (16), açık boy (8), CRC (4), sıkıştırma dizesi (4 + n), kayıt boyu (8).
        std::array<std::uint8_t, 64> bas{};
        const std::size_t bas_n = static_cast<std::size_t>(std::min<std::uint64_t>(boy, bas.size()));
        if (!kaynak->oku(konum, bas.data(), bas_n)) {
            hata = "parça okunamadı";
            return false;
        }
        Alan r(bas.data(), bas_n);
        (void)r.u64();  // message_start_time: güvenilmez; iletilerden hesaplanır
        (void)r.u64();  // message_end_time
        const std::uint64_t acik_boy = r.u64();
        const std::uint32_t crc = r.u32();
        std::string_view sikistirma;
        if (!dize_oku(r, sikistirma)) {
            hata = "parça başlığı çözülemedi";
            return false;
        }
        const std::uint64_t kayit_boyu = r.u64();
        if (!r.tamam()) {
            hata = "parça başlığı çözülemedi";
            return false;
        }
        const std::uint64_t bas_boyu = r.konum();
        if (kayit_boyu != boy - bas_boyu) {
            hata = "parça kayıt uzunluğu tutarsız";
            return false;
        }
        if (acik_boy > ayar.azami_parca_bayt || acik_boy > 0xFFFFFFF0ull) {
            hata = "parçanın sıkıştırmasız boyu üst sınırı aşıyor";
            return false;
        }
        const bool zstd_mi = sikistirma == "zstd";
        if (!zstd_mi && !sikistirma.empty()) {
            hata = "desteklenmeyen parça sıkıştırması";
            return false;
        }
        const auto acik_n = static_cast<std::size_t>(acik_boy);
        if (!zstd_mi) {
            if (kayit_boyu != acik_boy) {
                hata = "sıkıştırmasız parçanın boyu tutarsız";
                return false;
            }
            p.veri.resize(acik_n);
            if (!kaynak->oku(konum + bas_boyu, p.veri.data(), acik_n)) {
                hata = "parça okunamadı";
                return false;
            }
        } else {
            if (kayit_boyu > ZSTD_compressBound(static_cast<std::size_t>(ayar.azami_parca_bayt)) + 1024u) {
                hata = "parçanın sıkıştırılmış boyu üst sınırı aşıyor";
                return false;
            }
            const auto sikisik_n = static_cast<std::size_t>(kayit_boyu);
            ham.resize(sikisik_n);
            if (!kaynak->oku(konum + bas_boyu, ham.data(), sikisik_n)) {
                hata = "parça okunamadı";
                return false;
            }
            if (zstd == nullptr) {
                zstd = ZSTD_createDCtx();
                if (zstd == nullptr) {
                    hata = "zstd bağlamı oluşturulamadı";
                    return false;
                }
            }
            p.veri.resize(acik_n);
            const std::size_t sonuc = ZSTD_decompressDCtx(zstd, p.veri.data(), acik_n, ham.data(), sikisik_n);
            if (ZSTD_isError(sonuc) != 0 || sonuc != acik_n) {
                hata = "parça açılamadı (zstd)";
                return false;
            }
        }
        // A6-K59: CRC'yi okuyucu kendisi hesaplar; 0 değeri de "doğrulama yok" sayılmaz.
        if (crc32(p.veri.data(), p.veri.size()) != crc) {
            hata = "parça CRC'si tutmuyor";
            return false;
        }
        // İçerideki kayıtlar: yalnız Schema, Channel ve Message; eksiksiz tüketilmelidir.
        p.semalar.clear();
        p.kanallar.clear();
        p.iletiler.clear();
        if (yerel_kanal.size() != 65536) {
            yerel_kanal.assign(65536, false);
        }
        // Çıkışta `yerel_kanal` temizlenir (yalnız bu parçada işaretlenenler).
        struct Temizle {
            std::vector<bool>& v;
            const std::vector<KanalKaydi>& k;
            ~Temizle()
            {
                for (const KanalKaydi& x : k) {
                    v[x.id] = false;
                }
            }
        } temizle{yerel_kanal, p.kanallar};
        std::size_t i = 0;
        while (i < acik_n) {
            if (acik_n - i < kKayitBasligi) {
                hata = "parça içinde yarım kayıt";
                return false;
            }
            Alan kb(p.veri.data() + i, static_cast<std::size_t>(kKayitBasligi));
            const std::uint8_t op = kb.u8();
            const std::uint64_t n64 = kb.u64();
            const std::size_t govde = i + static_cast<std::size_t>(kKayitBasligi);
            if (n64 > acik_n - govde) {
                hata = "parça içinde yarım kayıt";
                return false;
            }
            const auto n = static_cast<std::size_t>(n64);
            const std::uint8_t* g = p.veri.data() + govde;
            if (op == kOpSchema) {
                SemaKaydi s;
                if (!sema_coz(g, n, s)) {
                    hata = "parça içinde çözülemeyen şema kaydı";
                    return false;
                }
                p.semalar.push_back(std::move(s));
            } else if (op == kOpChannel) {
                KanalKaydi k;
                if (!kanal_coz(g, n, k)) {
                    hata = "parça içinde çözülemeyen kanal kaydı";
                    return false;
                }
                yerel_kanal[k.id] = true;
                p.kanallar.push_back(std::move(k));
            } else if (op == kOpMessage) {
                if (n < 22) {
                    hata = "parça içinde çözülemeyen ileti kaydı";
                    return false;
                }
                Alan m(g, 22);
                HamIleti h;
                h.kanal_id = m.u16();
                h.sequence = m.u32();
                const std::uint64_t log_t = m.u64();
                const std::uint64_t pub_t = m.u64();
                if (log_t > static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max()) ||
                    pub_t > static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max())) {
                    hata = "ileti zamanı aralık dışında";
                    return false;
                }
                h.log_time = static_cast<std::int64_t>(log_t);
                h.publish_time = static_cast<std::int64_t>(pub_t);
                h.konum = static_cast<std::uint32_t>(govde + 22u);
                h.boy = static_cast<std::uint32_t>(n - 22u);
                const bool bilinen = yerel_kanal[h.kanal_id] || kanallar.find(h.kanal_id) != kanallar.end();
                if (!bilinen) {
                    hata = "ileti tanımlanmamış bir kanal kimliği taşıyor";
                    return false;
                }
                p.iletiler.push_back(h);
            } else {
                hata = "parça içinde beklenmeyen kayıt türü";
                return false;
            }
            i = govde + n;
        }
        return true;
    }

    void eksik_coz(const KanalDurumu& d, std::span<const std::uint8_t> veri)
    {
        EksikKaydi e;
        std::string kanal_adi;
        const CozmeSonucu s = alanlari_gez(*d.tanim->yuk, veri, [&](const a3::AlanTanimi& a, const AlanDegeri& v) {
            if (a.ad == "kanal") {
                kanal_adi = std::string(v.metin);
            } else if (a.ad == "ilk_sim_t_ns") {
                e.ilk_sim_t_ns = v.isaretli;
            } else if (a.ad == "son_sim_t_ns") {
                e.son_sim_t_ns = v.isaretli;
            } else if (a.ad == "ilk_adim") {
                e.ilk_adim = v.isaretsiz;
            } else if (a.ad == "son_adim") {
                e.son_adim = v.isaretsiz;
            } else if (a.ad == "kayip_sayisi") {
                e.kayip_sayisi = v.isaretsiz;
            } else if (a.ad == "neden") {
                e.neden = static_cast<std::uint8_t>(v.isaretsiz);
            }
        });
        if (!s.tamam || s.eksik_alan != 0) {
            uyar("çözülemeyen `kayit/eksik` kaydı");
            // Hangi kanalın eksik olduğu bilinmiyor: her düzey için en kısıtlayan kabul (adım 0).
            ozet.ilk_eksik_fizik = 0;
            ozet.ilk_eksik_girdi = 0;
            return;
        }
        e.kanal = kanal_bul(kanal_adi);
        if (e.kanal == nullptr) {
            // Bu okuyucunun bilmediği kanal: hangi düzeyi etkilediği bilinmez; her düzey için sayılır.
            uyar("`kayit/eksik` bilinmeyen bir kanalı gösteriyor: " + kanal_adi);
            if (!ozet.ilk_eksik_fizik.has_value() || e.ilk_adim < *ozet.ilk_eksik_fizik) {
                ozet.ilk_eksik_fizik = e.ilk_adim;
            }
            if (!ozet.ilk_eksik_girdi.has_value() || e.ilk_adim < *ozet.ilk_eksik_girdi) {
                ozet.ilk_eksik_girdi = e.ilk_adim;
            }
            return;
        }
        ozet.eksikler.push_back(e);
        const auto guncelle = [&](std::optional<std::uint64_t>& hedef) {
            if (!hedef.has_value() || e.ilk_adim < *hedef) {
                hedef = e.ilk_adim;
            }
        };
        if (eksik_duzeyi_etkiler(*e.kanal, Guvence::fizik)) {
            guncelle(ozet.ilk_eksik_fizik);
        }
        if (eksik_duzeyi_etkiler(*e.kanal, Guvence::girdi)) {
            guncelle(ozet.ilk_eksik_girdi);
        }
    }

    // Tarama sırasında bir parça: doğrular ve özetini kaydeder. Hata varsa false (dosya bozuk).
    bool parca_tara(std::uint64_t konum, std::uint64_t boy, ParcaIcerigi& p)
    {
        std::string hata;
        if (!parca_coz(konum, boy, p, hata)) {
            bozuk(hata, konum - kKayitBasligi);
            return false;
        }
        for (const SemaKaydi& s : p.semalar) {
            if (!sema_kesinlestir(s, konum)) {
                return false;
            }
        }
        for (const KanalKaydi& k : p.kanallar) {
            if (!kanal_kesinlestir(k, konum)) {
                return false;
            }
        }
        ParcaOzeti o;
        o.konum = konum;
        o.boy = boy;
        o.acik_boy = p.veri.size();
        for (const HamIleti& h : p.iletiler) {
            KanalDurumu& d = kanallar.find(h.kanal_id)->second;
            ++d.ileti_sayisi;
            ++ozet.ileti_sayisi;
            if (!d.okunur) {
                continue;
            }
            if (o.okunur_ileti == 0 || h.log_time < o.en_kucuk_t) {
                o.en_kucuk_t = h.log_time;
            }
            ++o.okunur_ileti;
            ++ozet.okunur_ileti_sayisi;
            if (!ozet.ilk_log_time.has_value() || h.log_time < *ozet.ilk_log_time) {
                ozet.ilk_log_time = h.log_time;
            }
            if (!ozet.son_log_time.has_value() || h.log_time > *ozet.son_log_time) {
                ozet.son_log_time = h.log_time;
            }
            if (d.tanim->ad == "kayit/eksik") {
                eksik_coz(d, std::span<const std::uint8_t>(p.veri.data() + h.konum, h.boy));
            }
        }
        parcalar.push_back(o);
        ++ozet.parca_sayisi;
        return true;
    }

    // ---- ek ----
    // Ek kaydını akışla okur: yapısını ve CRC'sini doğrular (A6-K59). `veri` verilirse içeriği oraya koyar.
    bool ek_coz(std::uint64_t konum, std::uint64_t boy, EkKaydi& e, std::vector<std::uint8_t>* veri, std::string& hata)
    {
        // Baş: log_time (8), create_time (8), ad, ortam türü, veri boyu (8). Ad ve tür kısa olmalıdır.
        std::array<std::uint8_t, 1024> bas{};
        const std::size_t bas_n = static_cast<std::size_t>(std::min<std::uint64_t>(boy, bas.size()));
        if (!kaynak->oku(konum, bas.data(), bas_n)) {
            hata = "ek okunamadı";
            return false;
        }
        Alan r(bas.data(), bas_n);
        (void)r.u64();
        (void)r.u64();
        std::string_view ad;
        std::string_view tur;
        if (!dize_oku(r, ad) || !dize_oku(r, tur)) {
            hata = "ek başlığı çözülemedi";
            return false;
        }
        const std::uint64_t veri_boyu = r.u64();
        if (!r.tamam()) {
            hata = "ek başlığı çözülemedi";
            return false;
        }
        const std::uint64_t bas_boyu = r.konum();
        if (boy < bas_boyu + 4u || veri_boyu != boy - bas_boyu - 4u) {
            hata = "ek kayıt uzunluğu tutarsız";
            return false;
        }
        e.ad = std::string(ad);
        e.ortam_turu = std::string(tur);
        e.kayit_konumu = konum;
        e.kayit_boyu = boy;
        e.veri_konumu = konum + bas_boyu;
        e.veri_boyu = veri_boyu;
        // CRC: kaydın CRC alanından önceki bütün baytları.
        std::uint32_t crc = crc32_baslat();
        std::vector<std::uint8_t> blok(static_cast<std::size_t>(std::min<std::uint64_t>(boy, kAkisBlogu)));
        std::uint64_t kalan = boy - 4u;
        std::uint64_t k = konum;
        if (veri != nullptr) {
            veri->clear();
            veri->reserve(static_cast<std::size_t>(veri_boyu));  // veri_boyu < boy ≤ dosya boyu
        }
        while (kalan > 0) {
            const auto n = static_cast<std::size_t>(std::min<std::uint64_t>(kalan, blok.size()));
            if (!kaynak->oku(k, blok.data(), n)) {
                hata = "ek okunamadı";
                return false;
            }
            crc = crc32_ekle(crc, blok.data(), n);
            if (veri != nullptr) {
                // Bloğun veri bölgesine düşen kısmı.
                const std::uint64_t bas_k = std::max<std::uint64_t>(k, e.veri_konumu);
                const std::uint64_t son_k = std::min<std::uint64_t>(k + n, e.veri_konumu + veri_boyu);
                if (son_k > bas_k) {
                    veri->insert(veri->end(), blok.data() + static_cast<std::size_t>(bas_k - k),
                                 blok.data() + static_cast<std::size_t>(son_k - k));
                }
            }
            k += n;
            kalan -= n;
        }
        std::array<std::uint8_t, 4> c{};
        if (!kaynak->oku(konum + boy - 4u, c.data(), c.size())) {
            hata = "ek okunamadı";
            return false;
        }
        Alan cr(c.data(), c.size());
        if (crc32_bitir(crc) != cr.u32()) {
            hata = "ek CRC'si tutmuyor";
            return false;
        }
        return true;
    }

    // ---- tarama ----
    // Dosya sonunda Footer kaydı ve sihirli baytlar yerinde mi (yapı olarak).
    bool kuyruk_kapali()
    {
        const std::uint64_t b = kaynak->boyut();
        const std::uint64_t gereken = kKayitBasligi + kFooterGovdesi + kSihir.size();
        if (b < kSihir.size() + gereken) {
            return false;
        }
        std::array<std::uint8_t, 37> k{};
        if (!kaynak->oku(b - gereken, k.data(), k.size())) {
            return false;
        }
        Alan r(k.data(), k.size());
        return r.u8() == kOpFooter && r.u64() == kFooterGovdesi &&
               std::memcmp(k.data() + kKayitBasligi + kFooterGovdesi, kSihir.data(), kSihir.size()) == 0;
    }

    AcmaHatasi tara()
    {
        const std::uint64_t boyut = kaynak->boyut();
        ozet.dosya_bayt = boyut;
        std::array<std::uint8_t, 8> sihir{};
        if (boyut < sihir.size() || !kaynak->oku(0, sihir.data(), sihir.size()) || sihir != kSihir) {
            return AcmaHatasi::sihirli_bayt_yok;
        }
        std::uint64_t konum = sihir.size();

        // Header.
        std::vector<std::uint8_t> kucuk;
        {
            std::array<std::uint8_t, 9> kb{};
            if (boyut - konum < kKayitBasligi || !kaynak->oku(konum, kb.data(), kb.size())) {
                return AcmaHatasi::baslik_okunamadi;
            }
            Alan r(kb.data(), kb.size());
            const std::uint8_t op = r.u8();
            const std::uint64_t n = r.u64();
            if (op != kOpHeader || n > boyut - konum - kKayitBasligi || n > kKucukKayitSiniri) {
                return AcmaHatasi::baslik_okunamadi;
            }
            kucuk.resize(static_cast<std::size_t>(n));
            if (!kaynak->oku(konum + kKayitBasligi, kucuk.data(), kucuk.size())) {
                return AcmaHatasi::baslik_okunamadi;
            }
            Alan h(kucuk.data(), kucuk.size());
            std::string_view profil;
            std::string_view kitaplik;
            if (!dize_oku(h, profil) || !dize_oku(h, kitaplik) || h.kalan() != 0) {
                return AcmaHatasi::baslik_okunamadi;
            }
            if (profil != kMcapProfili) {
                return AcmaHatasi::profil_farkli;
            }
            ozet.kitaplik = std::string(kitaplik);
            konum += kKayitBasligi + n;
        }

        ParcaIcerigi parca;
        bool veri_sonu = false;        // Data End görüldü
        std::uint64_t veri_sonu_konumu = 0;
        std::uint32_t veri_crc = 0;
        bool footer = false;
        std::uint64_t son_parca_sonu = 0;   // son geçerli parçanın bittiği konum; 0: parça yok
        std::uint64_t son_tam_kayit = konum;

        while (!dur && konum < boyut) {
            std::array<std::uint8_t, 9> kb{};
            if (boyut - konum < kKayitBasligi || !kaynak->oku(konum, kb.data(), kb.size())) {
                break;  // yarım kayıt başlığı: dosya kapanmamış
            }
            Alan r(kb.data(), kb.size());
            const std::uint8_t op = r.u8();
            const std::uint64_t n = r.u64();
            const std::uint64_t govde = konum + kKayitBasligi;
            if (n > boyut - govde) {
                break;  // gövdesi dosya sonunu aşan kayıt (A6-K60)
            }
            const std::uint64_t sonraki = govde + n;
            const bool kucuk_kayit = op == kOpSchema || op == kOpChannel || op == kOpMetadata || op == kOpDataEnd ||
                                     op == kOpFooter;
            if (kucuk_kayit) {
                const std::uint64_t sinir = op == kOpMetadata ? ayar.azami_ust_veri_bayt : kKucukKayitSiniri;
                if (n > sinir) {
                    if (op == kOpMetadata) {
                        uyar("üst veri kaydı üst sınırdan büyük; yok sayıldı");
                        ust_veri_cozulemedi = true;
                        konum = sonraki;
                        son_tam_kayit = konum;
                        continue;
                    }
                    bozuk("kayıt üst sınırdan büyük", konum);
                    dur = true;
                    break;
                }
                kucuk.resize(static_cast<std::size_t>(n));
                if (!kaynak->oku(govde, kucuk.data(), kucuk.size())) {
                    break;
                }
            }
            if (op == 0x00) {
                bozuk("geçersiz kayıt türü (0)", konum);
                dur = true;
            } else if (op == kOpHeader) {
                bozuk("ikinci Header kaydı", konum);
                dur = true;
            } else if (op == kOpFooter) {
                if (!veri_sonu) {
                    bozuk("Data End'den önce Footer", konum);
                    dur = true;
                } else if (n != kFooterGovdesi) {
                    bozuk("Footer boyu geçersiz", konum);
                    dur = true;
                } else {
                    footer = true;
                    konum = sonraki;
                    break;
                }
            } else if (op == kOpDataEnd) {
                if (veri_sonu || n != 4) {
                    bozuk("Data End kaydı geçersiz ya da yinelenmiş", konum);
                    dur = true;
                } else {
                    Alan d(kucuk.data(), kucuk.size());
                    veri_crc = d.u32();
                    veri_sonu = true;
                    veri_sonu_konumu = konum;
                }
            } else if (veri_sonu) {
                // Özet bölümü: kopyalar ve indeksler; okuyucu bunlara dayanmaz.
                if (op == kOpMessage || op == kOpChunk) {
                    bozuk("Data End'den sonra veri kaydı", konum);
                    dur = true;
                }
            } else if (op == kOpSchema) {
                Bekleyen b;
                b.tur = Bekleyen::Tur::sema;
                b.konum = konum;
                if (!sema_coz(kucuk.data(), kucuk.size(), b.sema)) {
                    bozuk("çözülemeyen şema kaydı", konum);
                    dur = true;
                } else {
                    bekleyen.push_back(std::move(b));
                }
            } else if (op == kOpChannel) {
                Bekleyen b;
                b.tur = Bekleyen::Tur::kanal;
                b.konum = konum;
                if (!kanal_coz(kucuk.data(), kucuk.size(), b.kanal)) {
                    bozuk("çözülemeyen kanal kaydı", konum);
                    dur = true;
                } else {
                    bekleyen.push_back(std::move(b));
                }
            } else if (op == kOpMetadata) {
                Bekleyen b;
                b.tur = Bekleyen::Tur::ust_veri;
                b.konum = konum;
                Alan m(kucuk.data(), kucuk.size());
                std::string_view ad;
                if (!dize_oku(m, ad) || !eslem_oku(m, b.ust_veri.icerik) || m.kalan() != 0) {
                    uyar("çözülemeyen üst veri kaydı yok sayıldı");
                    ust_veri_cozulemedi = true;
                } else {
                    b.ust_veri.ad = std::string(ad);
                    bekleyen.push_back(std::move(b));
                }
            } else if (op == kOpAttachment) {
                Bekleyen b;
                b.tur = Bekleyen::Tur::ek;
                b.konum = konum;
                std::string hata;
                if (!ek_coz(govde, n, b.ek, nullptr, hata)) {
                    // A6-K59: CRC'si tutmayan ek kullanılmaz; dosya bozuk. Parçalar okunmayı sürdürür.
                    bozuk(hata, konum);
                } else {
                    bekleyen.push_back(std::move(b));
                }
            } else if (op == kOpMessage) {
                bozuk("parça dışında ileti kaydı (A6 dosyası parçalıdır)", konum);
                dur = true;
            } else if (op == kOpChunk) {
                if (!bekleyenleri_kesinlestir() || !parca_tara(govde, n, parca)) {
                    dur = true;
                } else {
                    son_parca_sonu = sonraki;
                }
            }
            // Öteki kayıt türleri (ileti indeksi, özet kayıtları, bilinmeyen) atlanır.
            if (!dur && bekleyen.size() > kBekleyenSiniri) {
                bozuk("parçalar arasında çok fazla üst düzey kayıt", konum);
                dur = true;
            }
            if (dur) {
                break;
            }
            konum = sonraki;
            son_tam_kayit = konum;
        }

        // ---- sonuç ----
        if (ozet.bozuk && dur) {
            // İlk bozuk parçadan (ya da yapısal hatadan) öncesi kullanılır.
            (void)bekleyenleri_kesinlestir();
            ozet.kapanmis = kuyruk_kapali();
            ozet.kullanilan_bayt = ozet.bozulma_konumu;
        } else {
            bool sihir_tamam = false;
            if (footer && boyut - konum == kSihir.size()) {
                std::array<std::uint8_t, 8> son{};
                sihir_tamam = kaynak->oku(konum, son.data(), son.size()) && son == kSihir;
            }
            ozet.kapanmis = veri_sonu && footer && sihir_tamam;
            if (ozet.kapanmis) {
                (void)bekleyenleri_kesinlestir();
                ozet.kullanilan_bayt = boyut;
                if (veri_crc != 0 && !veri_crc_dogrula(veri_sonu_konumu, veri_crc)) {
                    bozuk("veri bölümü CRC'si tutmuyor", veri_sonu_konumu);
                }
            } else if (son_parca_sonu != 0) {
                // A6-K60 kurtarma sonu: son tam ve CRC'si tutan parçanın sonu; sonrası kullanılmaz.
                bekleyen.clear();
                ozet.kullanilan_bayt = son_parca_sonu;
            } else {
                // Hiç parça yok: tam okunan kayıtlar (açılış üst verisi, ekler) kullanılır.
                (void)bekleyenleri_kesinlestir();
                ozet.kullanilan_bayt = son_tam_kayit;
            }
        }
        return ozeti_tamamla();
    }

    bool veri_crc_dogrula(std::uint64_t son, std::uint32_t beklenen)
    {
        std::uint32_t crc = crc32_baslat();
        std::vector<std::uint8_t> blok(static_cast<std::size_t>(std::min<std::uint64_t>(std::max<std::uint64_t>(son, 1), kAkisBlogu)));
        std::uint64_t k = 0;
        while (k < son) {
            const auto n = static_cast<std::size_t>(std::min<std::uint64_t>(son - k, blok.size()));
            if (!kaynak->oku(k, blok.data(), n)) {
                return false;
            }
            crc = crc32_ekle(crc, blok.data(), n);
            k += n;
        }
        return crc32_bitir(crc) == beklenen;
    }

    AcmaHatasi ozeti_tamamla()
    {
        // A6-K48: farklı büyük sürüm okunmaz.
        const auto surum = ozet.ust_veri.find("a6_surumu");
        bool surum_gecerli = false;
        if (surum != ozet.ust_veri.end()) {
            const std::string_view s = surum->second;
            const std::size_t nokta = s.find('.');
            std::uint64_t buyuk = 0;
            std::uint64_t kucuk_surum = 0;
            if (nokta != std::string_view::npos && onluk_coz(s.substr(0, nokta), buyuk) &&
                onluk_coz(s.substr(nokta + 1), kucuk_surum)) {
                std::uint64_t bizim = 0;
                (void)onluk_coz(kA6Surumu.substr(0, kA6Surumu.find('.')), bizim);
                if (buyuk != bizim) {
                    return AcmaHatasi::buyuk_surum_farkli;
                }
                surum_gecerli = true;
            }
        }
        std::string hata;
        ozet.ust_veri_gecerli = ozet.ust_veri_var && !ust_veri_cozulemedi && surum_gecerli &&
                                ust_veri_denetle(kUstVeriAcilis, ozet.ust_veri, hata);
        if (!ozet.ust_veri_var) {
            uyar("`ida_kayit` açılış kaydı yok");
        } else if (!ozet.ust_veri_gecerli) {
            uyar("açılış üst verisi geçersiz: " + (hata.empty() ? std::string("sürüm ya da kayıt çözülemedi") : hata));
        }
        const auto kademe = ozet.ust_veri.find("kayit_kademesi");
        ozet.kademe_var = kademe != ozet.ust_veri.end() && kademe_coz(kademe->second, ozet.kademe);

        for (const auto& [id, d] : kanallar) {
            KanalOzeti k;
            k.konu = d.kayit.konu;
            k.tanim = d.tanim;
            k.kanal_surumu = d.kanal_surumu;
            k.okunur = d.okunur;
            k.ileti_sayisi = d.ileti_sayisi;
            k.uyari = d.uyari;
            ozet.kanallar.push_back(std::move(k));
        }
        for (const EkKaydi& e : ekler) {
            ozet.ekler.push_back(EkOzeti{e.ad, e.ortam_turu, e.veri_boyu});
        }

        // Güvence (A6 §4.6, §5).
        ozet.guvence = Guvence::izleme;
        ozet.guvence_on_eki_son_adim.reset();
        if (ozet.kapanmis && kapanis_goruldu) {
            ozet.kapanis = kapanis;
            // Uçtan uca denetim: kapanış kaydındaki `ileti_sayisi` dosyadaki iletilerle aynı olmalıdır.
            // Parça CRC'si tek tek parçaları korur; bir parçanın bütünüyle kaybolduğu (ör. bozulmuş
            // bir kayıt uzunluğunun onu yuttuğu) dosya bu sayıyla yakalanır.
            const auto n = kapanis.find("ileti_sayisi");
            std::uint64_t yazilan = 0;
            if (n != kapanis.end() && onluk_coz(n->second, yazilan) && yazilan != ozet.ileti_sayisi && !ozet.bozuk) {
                bozuk("kapanış kaydındaki `ileti_sayisi` (" + n->second + ") dosyadaki ileti sayısıyla (" +
                          std::to_string(ozet.ileti_sayisi) + ") uyuşmuyor",
                      ozet.kullanilan_bayt);
            }
        }
        if (ozet.saglam() && ozet.ust_veri_gecerli && ozet.kademe_var && ozet.kapanis.has_value()) {
            std::string khata;
            Guvence yazilan = Guvence::izleme;
            const auto g = ozet.kapanis->find("tekrar_guvencesi");
            if (!ust_veri_denetle(kUstVeriKapanis, *ozet.kapanis, khata) || g == ozet.kapanis->end() ||
                !guvence_coz(g->second, yazilan)) {
                uyar("kapanış üst verisi geçersiz: " + khata);
            } else {
                // Kapanış kaydındaki güvence, dosyanın kendi içeriğinden (kademe, kip, eksik kayıtları)
                // yeniden hesaplananla karşılaştırılır; ikisinden kısıtlayan olan geçerlidir.
                GuvenceGirdisi gg;
                gg.kademe = ozet.kademe;
                const auto kip = ozet.ust_veri.find("kip");
                gg.kip = kip != ozet.ust_veri.end() ? std::string_view(kip->second) : std::string_view();
                const auto gn = ozet.ust_veri.find("guvencesiz_nedenler");
                gg.guvencesiz_baslatma = gn != ozet.ust_veri.end() && !gn->second.empty();
                gg.canli_mudahale = ozet.kapanis->find("canli_mudahale_adimi") != ozet.kapanis->end();
                gg.ilk_eksik_fizik = ozet.ilk_eksik_fizik;
                gg.ilk_eksik_girdi = ozet.ilk_eksik_girdi;
                const GuvenceSonucu hesap = guvence_belirle(gg);
                ozet.guvence = std::min(yazilan, hesap.duzey);
                if (ozet.guvence != yazilan) {
                    uyar("kapanış kaydındaki tekrar güvencesi dosyanın içeriğiyle uyuşmuyor; "
                                            "kısıtlayan değer kullanıldı");
                }
                std::optional<std::uint64_t> on_ek;
                const auto oe = ozet.kapanis->find("guvence_on_eki_son_adim");
                std::uint64_t v = 0;
                if (oe != ozet.kapanis->end() && onluk_coz(oe->second, v)) {
                    on_ek = v;
                }
                if (ozet.guvence == hesap.duzey && hesap.on_eki_son_adim.has_value()) {
                    on_ek = on_ek.has_value() ? std::min(*on_ek, *hesap.on_eki_son_adim) : *hesap.on_eki_son_adim;
                }
                if (ozet.guvence > Guvence::izleme) {
                    ozet.guvence_on_eki_son_adim = on_ek;
                }
            }
        }
        // Sıralı okuma düzeni.
        zaman_sirasi.clear();
        for (std::size_t i = 0; i < parcalar.size(); ++i) {
            if (parcalar[i].okunur_ileti > 0) {
                zaman_sirasi.push_back(i);
            }
        }
        std::stable_sort(zaman_sirasi.begin(), zaman_sirasi.end(), [&](std::size_t a, std::size_t b) {
            return parcalar[a].en_kucuk_t < parcalar[b].en_kucuk_t;
        });
        return AcmaHatasi::yok;
    }

    // ---- sıralı okuma ----
    void basa_sar()
    {
        acik.clear();
        acik_bayt = 0;
        yigin = {};
        sonraki_parca = 0;
        biten_parca.reset();
        okuma_hatasi.clear();
    }

    bool kanal_secili(const KanalDurumu& d) const
    {
        return d.okunur && (secili.empty() || secili[d.tanim->indeks]);
    }

    bool parca_yukle(std::size_t indeks)
    {
        const ParcaOzeti& o = parcalar[indeks];
        if (acik_bayt + o.acik_boy > ayar.azami_acik_bayt && !acik.empty()) {
            okuma_hatasi = "sıra penceresi sınırı aşıldı (zaman aralıkları örtüşen parçalar çok büyük)";
            return false;
        }
        AcikParca a;
        std::string hata;
        if (!parca_coz(o.konum, o.boy, a.icerik, hata)) {
            okuma_hatasi = "dosya açılıştan sonra değişti: " + hata;
            return false;
        }
        auto& v = a.icerik.iletiler;
        v.erase(std::remove_if(v.begin(), v.end(),
                               [&](const HamIleti& h) {
                                   const auto it = kanallar.find(h.kanal_id);
                                   return it == kanallar.end() || !kanal_secili(it->second);
                               }),
                v.end());
        if (v.empty()) {
            return true;
        }
        std::stable_sort(v.begin(), v.end(), [](const HamIleti& x, const HamIleti& y) {
            return x.log_time != y.log_time ? x.log_time < y.log_time : x.sequence < y.sequence;
        });
        acik_bayt += a.icerik.veri.size();
        yigin.push(YiginOgesi{v.front().log_time, v.front().sequence, indeks});
        acik.emplace(indeks, std::move(a));
        return true;
    }

    bool sonraki(Ileti& i)
    {
        if (!okuma_hatasi.empty()) {
            return false;
        }
        if (biten_parca.has_value()) {
            // Önceki çağrıda verilen iletinin baytları artık gerekmez.
            const auto it = acik.find(*biten_parca);
            if (it != acik.end()) {
                acik_bayt -= it->second.icerik.veri.size();
                acik.erase(it);
            }
            biten_parca.reset();
        }
        // Sıradaki adayın zamanına eşit ya da ondan erken başlayan bütün parçalar açık olmalıdır.
        while (sonraki_parca < zaman_sirasi.size() &&
               (yigin.empty() || parcalar[zaman_sirasi[sonraki_parca]].en_kucuk_t <= yigin.top().log_time)) {
            if (!parca_yukle(zaman_sirasi[sonraki_parca])) {
                return false;
            }
            ++sonraki_parca;
        }
        if (yigin.empty()) {
            return false;
        }
        const YiginOgesi ust = yigin.top();
        yigin.pop();
        AcikParca& a = acik.find(ust.parca)->second;
        const HamIleti& h = a.icerik.iletiler[a.sira++];
        const KanalDurumu& d = kanallar.find(h.kanal_id)->second;
        i.kanal = d.tanim;
        i.kanal_surumu = d.kanal_surumu;
        i.log_time = h.log_time;
        i.publish_time = h.publish_time;
        i.sequence = h.sequence;
        i.veri = std::span<const std::uint8_t>(a.icerik.veri.data() + h.konum, h.boy);
        if (a.sira < a.icerik.iletiler.size()) {
            const HamIleti& s = a.icerik.iletiler[a.sira];
            yigin.push(YiginOgesi{s.log_time, s.sequence, ust.parca});
        } else {
            biten_parca = ust.parca;
        }
        return true;
    }
};

}  // namespace ic

std::string_view acma_hatasi_adi(AcmaHatasi h) noexcept
{
    switch (h) {
    case AcmaHatasi::yok:
        return "yok";
    case AcmaHatasi::dosya_acilamadi:
        return "dosya_acilamadi";
    case AcmaHatasi::sihirli_bayt_yok:
        return "sihirli_bayt_yok";
    case AcmaHatasi::baslik_okunamadi:
        return "baslik_okunamadi";
    case AcmaHatasi::profil_farkli:
        return "profil_farkli";
    case AcmaHatasi::buyuk_surum_farkli:
        return "buyuk_surum_farkli";
    }
    return {};
}

Okuyucu::Okuyucu() : ic_(std::make_unique<ic::OkuyucuIc>()) {}
Okuyucu::~Okuyucu() = default;

std::unique_ptr<Okuyucu> Okuyucu::ac(const std::filesystem::path& yol, AcmaHatasi& hata, const OkumaAyari& ayar)
{
    auto kaynak = std::make_unique<ic::DosyaKaynagi>();
    if (!kaynak->ac(yol)) {
        hata = AcmaHatasi::dosya_acilamadi;
        return nullptr;
    }
    std::unique_ptr<Okuyucu> o(new Okuyucu());
    o->ic_->ayar = ayar;
    o->ic_->kaynak = std::move(kaynak);
    hata = o->ic_->tara();
    if (hata != AcmaHatasi::yok) {
        return nullptr;
    }
    return o;
}

std::unique_ptr<Okuyucu> Okuyucu::ac_bellek(std::span<const std::uint8_t> veri, AcmaHatasi& hata, const OkumaAyari& ayar)
{
    std::unique_ptr<Okuyucu> o(new Okuyucu());
    o->ic_->ayar = ayar;
    o->ic_->kaynak = std::make_unique<ic::BellekKaynagi>(veri);
    hata = o->ic_->tara();
    if (hata != AcmaHatasi::yok) {
        return nullptr;
    }
    return o;
}

const DosyaOzeti& Okuyucu::ozet() const noexcept
{
    return ic_->ozet;
}

bool Okuyucu::ek_oku(std::string_view ad, std::vector<std::uint8_t>& veri) const
{
    for (const ic::EkKaydi& e : ic_->ekler) {
        if (e.ad == ad) {
            ic::EkKaydi yeniden;
            std::string hata;
            return ic_->ek_coz(e.kayit_konumu, e.kayit_boyu, yeniden, &veri, hata);
        }
    }
    return false;
}

void Okuyucu::suz(std::span<const KanalTanimi* const> secilenler)
{
    ic_->secili.clear();
    if (!secilenler.empty()) {
        ic_->secili.assign(kanallar().size(), false);
        for (const KanalTanimi* k : secilenler) {
            if (k != nullptr && k->indeks < ic_->secili.size()) {
                ic_->secili[k->indeks] = true;
            }
        }
    }
    ic_->basa_sar();
}

void Okuyucu::basa_sar()
{
    ic_->basa_sar();
}

bool Okuyucu::sonraki(Ileti& i)
{
    return ic_->sonraki(i);
}

const std::string& Okuyucu::okuma_hatasi() const noexcept
{
    return ic_->okuma_hatasi;
}

}  // namespace ida::kayit
