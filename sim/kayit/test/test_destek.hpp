// sim/kayit testlerinin ortak düzeni: her test `int test_<ad>()`, 0 geçti (sim/a3 ve sim/a2 ile aynı).
#pragma once

#include "ida/kayit/okuyucu.hpp"
#include "ida/kayit/yazici.hpp"

#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace ida::kayit::test {

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

std::uint64_t surec_kimligi();

// İşletim sisteminin geçici dizininde, süreç kimliğiyle adlandırılmış dizin; yıkıcıda silinir.
class GeciciDizin {
public:
    explicit GeciciDizin(std::string_view ad);
    ~GeciciDizin();
    GeciciDizin(const GeciciDizin&) = delete;
    GeciciDizin& operator=(const GeciciDizin&) = delete;
    const std::filesystem::path& yol() const noexcept { return yol_; }

private:
    std::filesystem::path yol_;
};

// Enjekte edilen monoton saat (ns). Kopyaları aynı zamanı paylaşır.
class SahteSaat {
public:
    SahteSaat() : t_(std::make_shared<std::atomic<std::int64_t>>(1'000'000'000)) {}
    std::function<std::int64_t()> islev() const
    {
        auto t = t_;
        return [t] { return t->load(std::memory_order_acquire); };
    }
    void ilerlet(std::int64_t ns) { t_->fetch_add(ns, std::memory_order_acq_rel); }

private:
    std::shared_ptr<std::atomic<std::int64_t>> t_;
};

inline constexpr std::string_view kOrnekRunId = "20261125-101500-a1b2c3";

// Bütün zorunlu anahtarları dolu bir açılış üst verisi (değerler sınama içindir).
AcilisUstVerisi ornek_ust_veri(std::string_view run_id = kOrnekRunId, std::string_view kip = "sil");

// Zorunlu eklerin hepsi (içerik sınama içindir); `a6_kanallar.json`'ı yazıcı ekler.
struct OrnekEkler {
    std::vector<std::pair<std::string, std::vector<std::uint8_t>>> icerik;
    std::vector<Ek> ekler() const;
};
OrnekEkler ornek_ekler();

KapanisAlanlari ornek_kapanis();

// Elle sürülen (iş parçacıksız) yazıcı ayarı: tek üretici "dunya".
Ayar elle_ayar(const GeciciDizin& d, Kademe kademe, const SahteSaat& saat, std::size_t halka_bayt = std::size_t{4} << 20);

// Yazıcı düzeneği: geçici dizin, sahte saat, örnek üst veri ve ekler; elle sürülür (`y->isle()`).
struct Duzenek {
    explicit Duzenek(std::string_view ad, Kademe kademe = Kademe::tam, std::string_view kip = "sil",
                     const std::function<void(Ayar&)>& ayarla = {});
    Uretici& u() { return y->uretici(0); }
    KapanisSonucu kapat() { return y->kapat(ornek_kapanis()); }
    std::unique_ptr<Okuyucu> oku();  // dosyayı A6 okuyucusuyla açar; açılamazsa nullptr ve `acma`

    GeciciDizin dizin;
    SahteSaat saat;
    OrnekEkler ekler;
    Ayar ayar;
    std::string hata;
    AcmaHatasi acma = AcmaHatasi::yok;
    std::unique_ptr<Yazici> y;
};

// Fizik durumu: 4 gövde, 8 itici (A6 §6'daki hesap: 647 bayt).
struct FizikDurumu {
    std::array<a3::yuk_fizik_durum_govdeler, 4> govdeler{};
    std::array<a3::yuk_fizik_durum_iticiler, 8> iticiler{};
    a3::yuk_fizik_durum y;
    FizikDurumu()
    {
        y.govdeler = govdeler;
        y.iticiler = iticiler;
    }
    // Yavaş değişen, fiziksel görünümlü değerler (sıkıştırma ölçümü rastgele gürültüden anlamlı olsun).
    void guncelle(std::uint64_t adim)
    {
        const double t = static_cast<double>(adim) * 0.005;
        y.adim_no = adim * 4u;
        for (std::size_t g = 0; g < govdeler.size(); ++g) {
            auto& b = govdeler[g];
            b.govde_id = static_cast<std::uint16_t>(g);
            const double f = 0.1 + 0.03 * static_cast<double>(g);
            b.p_n = {10.0 * std::sin(f * t), 10.0 * std::cos(f * t), 1.5 + 0.2 * std::sin(0.5 * t)};
            b.q_nb = {std::cos(0.05 * t), 0.0, 0.0, std::sin(0.05 * t)};
            b.nu = {1.0 + 0.1 * std::sin(t), 0.01 * std::cos(t), 0.0, 0.0, 0.0, 0.05};
            b.a_n = {0.1 * std::cos(t), -0.01 * std::sin(t), 0.0};
        }
        for (std::size_t i = 0; i < iticiler.size(); ++i) {
            iticiler[i].u = static_cast<float>(0.3 * std::sin(t + static_cast<double>(i)));
            iticiler[i].itki_n = static_cast<float>(12.0 * std::sin(t + static_cast<double>(i)));
            iticiler[i].akim_a = static_cast<float>(2.0 + 1.5 * std::fabs(std::sin(t + static_cast<double>(i))));
        }
        y.bara_gerilimi_v = static_cast<float>(15.8 - 0.0001 * t);
        y.paket_akimi_a = static_cast<float>(14.0 + std::sin(t));
        y.soc = static_cast<float>(0.9 - 0.00001 * t);
    }
};

std::vector<std::uint8_t> dosya_oku(const std::filesystem::path& yol);
bool dosya_yaz(const std::filesystem::path& yol, const std::vector<std::uint8_t>& veri);

struct OkunanIleti {
    std::string kanal;
    std::int64_t log_time = 0;
    std::int64_t publish_time = 0;
    std::uint32_t sequence = 0;
    std::vector<std::uint8_t> veri;
};
// Okuyucunun verdiği bütün iletiler, verdiği sırayla.
std::vector<OkunanIleti> hepsini_oku(Okuyucu& o);

// ---- testin kendi MCAP kayıt taraması (yalnız konum bulmak için) ----
struct KayitKonumu {
    std::uint8_t op = 0;
    std::uint64_t konum = 0;  // kaydın başı (opcode)
    std::uint64_t govde = 0;  // gövdenin başı
    std::uint64_t boy = 0;    // gövde boyu
};
std::vector<KayitKonumu> kayitlari_tara(const std::vector<std::uint8_t>& dosya);
// Data End kaydındaki veri bölümü CRC'sini dosyanın o anki içeriğinden yeniden hesaplar ve yazar
// (W5-K39: CRC, ilk bayttan Data End kaydının başına kadar). Bir denetimi veri bölümü CRC'sinden
// BAĞIMSIZ sınamak için: değişiklik yapılır, CRC yenilenir, öteki denetimin yakaladığı gösterilir.
// Data End bulunamazsa false.
bool veri_crc_yenile(std::vector<std::uint8_t>& dosya);
// Dosyadaki Data End kaydının konumu (opcode); yoksa 0.
std::uint64_t veri_sonu_konumu(const std::vector<std::uint8_t>& dosya);

// ---- elle MCAP kurucu: okuyucu testleri için kitaplıktan ve yazıcıdan bağımsız yazım ----
class ElleMcap {
public:
    using Eslem = std::map<std::string, std::string>;
    ElleMcap& sihir();
    ElleMcap& baslik(std::string_view profil, std::string_view kitaplik);
    ElleMcap& sema(std::uint16_t id, std::string_view ad, std::string_view kodlama, std::string_view veri);
    ElleMcap& kanal(std::uint16_t id, std::uint16_t sema_id, std::string_view konu, std::string_view kodlama,
                    const Eslem& metadata);
    ElleMcap& ileti(std::uint16_t kanal_id, std::uint32_t sira, std::uint64_t log_time, std::uint64_t publish_time,
                    const std::vector<std::uint8_t>& veri);
    // `kayitlar`: başka bir ElleMcap'in baytları (şema, kanal, ileti). Sıkıştırmasız; CRC doğru hesaplanır.
    ElleMcap& parca(const std::vector<std::uint8_t>& kayitlar);
    ElleMcap& ust_veri(std::string_view ad, const Eslem& icerik);
    ElleMcap& ek(std::string_view ad, std::string_view ortam_turu, const std::vector<std::uint8_t>& veri);
    ElleMcap& veri_sonu(std::uint32_t crc = 0);
    // Data End, o ana kadarki bütün baytların CRC-32'siyle (W5-K39'daki yazıcının yazdığı gibi).
    ElleMcap& veri_sonu_crcli();
    ElleMcap& footer();
    ElleMcap& ham(const std::vector<std::uint8_t>& baytlar);
    const std::vector<std::uint8_t>& al() const noexcept { return b_; }

private:
    void u8(std::uint8_t v);
    void u16(std::uint16_t v);
    void u32(std::uint32_t v);
    void u64(std::uint64_t v);
    void dize(std::string_view s);
    void kayit_basi(std::uint8_t op, std::uint64_t boy);
    std::vector<std::uint8_t> b_;
};

// ---- ayrı süreç ----
// Programı çalıştırır, bitmesini bekler; çıkış kodu (başlatılamazsa -1).
int surec_calistir(const std::vector<std::string>& argumanlar);
// Programı başlatır (beklemeden). Tutamak 0 ise başlatılamadı.
std::uint64_t surec_baslat(const std::vector<std::string>& argumanlar);
// Süreci hemen öldürür (POSIX SIGKILL, Windows TerminateProcess) ve kaynaklarını bırakır.
void surec_oldur(std::uint64_t tutamak);

// ---- ayırma sayacı (ayirma_sayaci.cpp; yalnız ida_kayit_test) ----
bool ayirma_sayimi_var() noexcept;         // false: sanitizer'lı derleme (sayım yapılmaz)
void ayirma_sayimini_baslat() noexcept;    // çağıran iş parçacığı için
std::uint64_t ayirma_sayimini_bitir() noexcept;

}  // namespace ida::kayit::test
