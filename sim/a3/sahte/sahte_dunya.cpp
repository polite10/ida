// sahte_dunya.cpp — bkz. sahte_dunya.hpp.
#include "sahte_dunya.hpp"

#include "ida/a3/zaman.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <limits>
#include <optional>
#include <random>

namespace ida::a3::sahte {

namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr std::int64_t kAdimNs = 1'250'000;  // iskelet §4: fizik adımı
constexpr std::uint64_t kYok64 = std::numeric_limits<std::uint64_t>::max();

std::string metin_json(std::string_view s)
{
    // Sahte olaylar yalnız ASCII ad ve kod taşır.
    std::string r = "\"";
    for (const char c : s) {
        if (c == '"' || c == '\\') {
            r.push_back('\\');
        }
        r.push_back(c);
    }
    r.push_back('"');
    return r;
}

const char* durum_adi(std::uint8_t d)
{
    static constexpr const char* k[] = {"bos", "yuklendi", "hazirlaniyor", "kosuyor", "duraklatildi",
                                         "bitti", "durduruldu", "hata", "izleme"};
    return d < 9 ? k[d] : "bos";
}

bool canli(std::uint8_t d) { return d == 2 || d == 3 || d == 4; }

}  // namespace

struct SahteDunya::Durum {
    std::uint8_t durum = 1;  // yuklendi: sahte senaryo yüklü
    ZamanKipi kip = ZamanKipi::sil;
    std::optional<std::string> run_id;
    std::uint64_t durum_rev = 1;
    std::uint64_t adim_no = 0;
    std::int64_t t_ns = 0;
    double carpan = 1.0;
    std::uint32_t adimla_kalan = 0;  // fizik adımı
    std::uint32_t emniyet_rev = 1;
    bool doydu = false;
    std::uint8_t acil = 0;                   // serbest
    std::array<std::uint8_t, 2> fis{0, 0};   // çıkarılmış (kısıtlayan başlangıç)
    std::array<std::uint8_t, 2> pim{1, 1};   // takılı (kısıtlayan başlangıç)
    std::optional<std::uint64_t> son_degisim;
    std::uint8_t son_kaynak = 0;
    std::deque<Komut> genel;
    std::deque<Komut> kisitlayan;
    bool donduruldu = false;
    std::vector<UygulamaIzi> iz;
    std::vector<Komut> alinan;
    std::uint32_t son_komut_no = 0;
    std::uint64_t dongu = 0;
    std::uint32_t seq = 0;
    std::uint16_t ping_aci = 0;
    std::mt19937 rng{20261123u};
};

SahteDunya::SahteDunya(Yayinci& yayinci, SahteAyar ayar) : y_(yayinci), ayar_(ayar), d_(std::make_unique<Durum>())
{
    d_->kip = ayar.kip;
    d_->emniyet_rev = ayar.emniyet_rev_baslangic;
}

SahteDunya::~SahteDunya() { durdur(); }

void SahteDunya::baslat()
{
    if (!ayar_.is_parcacigi || is_.joinable()) {
        return;
    }
    dur_.store(false);
    is_ = std::thread([this] {
        auto sonraki = std::chrono::steady_clock::now();
        while (!dur_.load(std::memory_order_relaxed)) {
            adim();
            sonraki += std::chrono::milliseconds(5);
            const auto simdi = std::chrono::steady_clock::now();
            if (sonraki < simdi - std::chrono::milliseconds(50)) {
                sonraki = simdi;
            }
            hassas_bekle(sonraki);  // Windows: sleep_until sistem tikine (≈ 15,6 ms) yuvarlanır
        }
    });
}

void SahteDunya::durdur()
{
    dur_.store(true);
    if (is_.joinable()) {
        is_.join();
    }
}

AnlikDurum SahteDunya::anlik()
{
    std::lock_guard<std::mutex> k(kilit_);
    const Durum& d = *d_;
    AnlikDurum a;
    a.durum = static_cast<CalistirmaDurumu>(d.durum);
    a.kip = d.kip;
    a.run_id = d.run_id;
    a.sim_t_ns = d.t_ns;
    a.adim_no = d.adim_no;
    a.senaryo_ozeti = std::string("sha256:") + std::string(64, '0');
    a.durum_rev = d.durum_rev;
    a.emniyet_rev = d.emniyet_rev;
    a.emniyet_rev_doydu = d.doydu;
    a.acil_durdurma = d.acil;
    a.silahlandirma_fisi = d.fis;
    a.emniyet_pimi = d.pim;
    a.son_degisim_adimi = d.son_degisim;
    a.sayaclar = {{"kacirilan_adim", 0}, {"parti_kacirildi", 0}};
    return a;
}

namespace {

AlimSonucu ret(std::string kod, std::string ileti, std::string ayrinti = {})
{
    AlimSonucu a;
    a.tur = AlimSonucu::Tur::ret;
    a.hata_kodu = std::move(kod);
    a.ileti = std::move(ileti);
    a.ayrinti_json = std::move(ayrinti);
    return a;
}

// Tablo 4.7a: komutun bu durumda geçerli olup olmadığı.
bool gecis_gecerli(std::uint8_t d, KomutTuru t)
{
    switch (t) {
    case KomutTuru::calistirma_baslat: return d == 1;
    case KomutTuru::calistirma_duraklat: return d == 3;
    case KomutTuru::calistirma_surdur: return d == 4;
    case KomutTuru::calistirma_adimla: return d == 4;
    case KomutTuru::calistirma_hiz: return d == 3 || d == 4;
    case KomutTuru::calistirma_durdur: return d == 2 || d == 3 || d == 4;
    case KomutTuru::calistirma_sifirla: return d >= 1 && d <= 7;
    case KomutTuru::senaryo_yukle: return d == 1 || d == 5 || d == 6 || d == 7;   // Tablo 4.7a: `senaryo/yukle` sütunu
    default: return false;
    }
}

bool calistirma_mi(KomutTuru t)
{
    return t != KomutTuru::emniyet_degistir && t != KomutTuru::yer_istasyonu_komut;
}

}  // namespace

AlimSonucu SahteDunya::al(const Komut& k)
{
    std::lock_guard<std::mutex> kl(kilit_);
    Durum& d = *d_;
    d.son_komut_no = std::max(d.son_komut_no, k.komut_no);
    // A3-K54, K124 (Tablo 4.7a′): beklenen_run_id
    const bool run_uyar = d.run_id.has_value() == k.beklenen_run_id.has_value() &&
                          (!d.run_id || *d.run_id == *k.beklenen_run_id);
    auto run_ret = [&] {
        std::string a = "{\"guncel_run_id\":" + (d.run_id ? metin_json(*d.run_id) : std::string("null")) +
                        ",\"guncel_durum\":" + metin_json(durum_adi(d.durum)) +
                        ",\"guncel_durum_rev\":" + std::to_string(d.durum_rev) + "}";
        return ret("beklenen_durum_uyusmuyor", "beklenen_run_id güncel değil", a);
    };
    if (calistirma_mi(k.tur)) {
        if (k.beklenen_durum_rev != d.durum_rev) {
            return ret("durum_rev_uyusmuyor", "beklenen_durum_rev güncel değil",
                       "{\"guncel_durum_rev\":" + std::to_string(d.durum_rev) + "}");
        }
        if (!run_uyar) {
            return run_ret();
        }
        if (!gecis_gecerli(d.durum, k.tur)) {
            return ret("gecersiz_durum", "komut bu çalıştırma durumunda geçersiz (Tablo 4.7a)");
        }
        const bool hil = d.kip == ZamanKipi::hil_1x || d.kip == ZamanKipi::hil_bariyer;
        if (hil && (k.tur == KomutTuru::calistirma_adimla || k.tur == KomutTuru::calistirma_hiz)) {
            return ret("kipte_gecersiz", "HIL kiplerinde geçersiz (Tablo 4.7b)");
        }
        if (hil && k.tur == KomutTuru::calistirma_duraklat && !k.gecersizligi_onayla) {
            return ret("onay_gerekli", "HIL'de duraklatma geçersizliği onay ister (Tablo 4.7b)");
        }
    } else {
        // W4-K19 (EMNİYET): kısıtlayan istekte beklenen_run_id denetlenmez; A3-K54 yalnız
        // gevşeten ve nötr isteklere uygulanır. Canlı çalıştırma şartı (A3-K91) sürer.
        if (!k.kisitlayan) {
            if (!k.beklenen_run_id || !d.run_id) {
                return ret("canli_calistirma_yok", "canlı çalıştırma yok");
            }
            if (!run_uyar) {
                return run_ret();
            }
        }
        if (!d.run_id || !canli(d.durum)) {
            return ret("canli_calistirma_yok", "canlı çalıştırma yok");
        }
        const bool revli = !k.kisitlayan && (k.tur == KomutTuru::emniyet_degistir || k.yi_sinif == KomutSinifi::genisletici);
        if (revli) {
            if (d.doydu) {
                return ret("emniyet_rev_doydu", "emniyet revizyonu doydu");
            }
            if (!k.beklenen_emniyet_rev || *k.beklenen_emniyet_rev != d.emniyet_rev) {
                return ret("emniyet_rev_uyusmuyor", "beklenen_emniyet_rev güncel değil",
                           "{\"guncel_emniyet_rev\":" + std::to_string(d.emniyet_rev) + "}");
            }
        }
    }
    auto& q = k.kisitlayan ? d.kisitlayan : d.genel;
    const std::size_t sinir = k.kisitlayan ? ayar_.kisitlayan_kuyruk : ayar_.genel_kuyruk;
    if (q.size() >= sinir) {
        AlimSonucu a;
        a.tur = AlimSonucu::Tur::dolu;
        return a;
    }
    q.push_back(k);
    d.alinan.push_back(k);  // yalnız kuyruğa alınanlar
    AlimSonucu a;
    a.tur = AlimSonucu::Tur::kuyrukta;
    return a;
}

void SahteDunya::dondur(bool v)
{
    std::lock_guard<std::mutex> k(kilit_);
    d_->donduruldu = v;
}

std::vector<UygulamaIzi> SahteDunya::iz()
{
    std::lock_guard<std::mutex> k(kilit_);
    return d_->iz;
}

std::vector<Komut> SahteDunya::alinanlar()
{
    std::lock_guard<std::mutex> k(kilit_);
    return d_->alinan;
}

template <typename F>
void SahteDunya::olc_(F&& f)
{
    const auto bas = std::chrono::steady_clock::now();
    f();
    const auto ns = static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - bas).count());
    yayin_sayisi_.fetch_add(1, std::memory_order_relaxed);
    toplam_ns_.fetch_add(ns, std::memory_order_relaxed);
    if (ns > 50000u) {
        uzun_.fetch_add(1, std::memory_order_relaxed);
    }
    std::uint64_t eski = en_kotu_ns_.load(std::memory_order_relaxed);
    while (ns > eski && !en_kotu_ns_.compare_exchange_weak(eski, ns, std::memory_order_relaxed)) {
    }
}

void SahteDunya::olay_(std::uint16_t tur, std::uint8_t onem, std::uint8_t kaynak, std::uint32_t komut_no,
                       std::uint32_t istemci, const std::string& ayrinti)
{
    yuk_olay o;
    o.tur = tur;
    o.onem = onem;
    o.kaynak = kaynak;
    o.adim_no = d_->adim_no;
    o.komut_no = komut_no;
    o.istemci_id = istemci;
    o.ayrinti = ayrinti;
    olc_([&] { y_.olay(d_->t_ns, o); });
}

void SahteDunya::sonuc_(const Komut& k, bool uygulandi, const std::string& kod, const std::string& ayrinti)
{
    KomutSonucuGirdisi s;
    s.komut_no = k.komut_no;
    s.uygulandi = uygulandi;
    s.uygulama_adimi_var = uygulandi;
    s.uygulama_adimi = d_->adim_no;
    s.t_ns = d_->t_ns;
    s.adim_no = d_->adim_no;
    s.hata_kodu = kod;
    s.sonuc_ayrinti_json = ayrinti;
    olc_([&] { y_.komut_sonucu(s); });
    UygulamaIzi i;
    i.komut_no = k.komut_no;
    i.tur = k.tur;
    i.kisitlayan = k.kisitlayan;
    i.uygulandi = uygulandi;
    i.hata_kodu = kod;
    i.emniyet_rev_sonra = d_->emniyet_rev;
    d_->iz.push_back(i);
}

void SahteDunya::gecis_(std::uint8_t yeni, const char* neden)
{
    Durum& d = *d_;
    const std::uint8_t eski = d.durum;
    d.durum = yeni;
    std::string a = "{\"onceki\":" + metin_json(durum_adi(eski)) + ",\"sonraki\":" + metin_json(durum_adi(yeni));
    if (neden != nullptr) {
        a += ",\"neden\":" + metin_json(neden);
    }
    a += "}";
    olay_(1, 0, 0, 0, 0, a);
}

void SahteDunya::uygula_()
{
    Durum& d = *d_;
    if (d.donduruldu) {
        return;
    }
    auto rev_artir = [&d] {
        if (d.emniyet_rev < 0xFFFFFFFFu) {
            ++d.emniyet_rev;
        }
        if (d.emniyet_rev == 0xFFFFFFFFu && !d.doydu) {
            d.doydu = true;
        }
    };
    auto emniyet_uygula = [&](const Komut& k) {
        std::uint8_t* hedef = nullptr;
        std::uint8_t deger = 0;
        const char* onceki = nullptr;
        const char* sonraki = nullptr;
        if (k.girdi == EmniyetGirdisi::acil_durdurma) {
            hedef = &d.acil;
            deger = k.hedef_durum == GirdiDurumu::basili ? 1 : 0;
            onceki = d.acil != 0 ? "basili" : "serbest";
            sonraki = deger != 0 ? "basili" : "serbest";
        } else {
            const auto t = static_cast<std::size_t>(k.tup < 0 ? 0 : k.tup);
            hedef = k.girdi == EmniyetGirdisi::silahlandirma_fisi ? &d.fis[t] : &d.pim[t];
            deger = k.hedef_durum == GirdiDurumu::takili ? 1 : 0;
            onceki = *hedef != 0 ? "takili" : "cikarilmis";
            sonraki = deger != 0 ? "takili" : "cikarilmis";
        }
        const bool degisti = *hedef != deger;
        *hedef = deger;
        rev_artir();  // A3-K117 (1) ve (6): durum değişmese de kısıtlama revizyonu artırır
        if (degisti) {
            d.son_degisim = d.adim_no;
            d.son_kaynak = 2;
            static constexpr const char* kGirdi[] = {"acil_durdurma", "silahlandirma_fisi", "emniyet_pimi"};
            std::string a = "{\"girdi\":" + metin_json(kGirdi[static_cast<int>(k.girdi)]);
            if (k.tup >= 0) {
                a += ",\"tup\":" + metin_json(k.tup == 0 ? "f1" : "f2");
            }
            a += ",\"onceki\":" + metin_json(onceki) + ",\"sonraki\":" + metin_json(sonraki) +
                 ",\"kaynak\":\"operator\",\"yon\":" + metin_json(k.kisitlayan ? "artiran" : "azaltan") +
                 ",\"uygulama_adimi\":" + std::to_string(d.adim_no) + ",\"emniyet_rev\":" + std::to_string(d.emniyet_rev) +
                 "}";
            olay_(7, 3, 0, k.komut_no, k.istemci_id, a);
        }
        sonuc_(k, true, {}, {});
    };
    auto yi_uygula = [&](const Komut& k, const char* asama) {
        sonuc_(k, true, {}, {});
        const std::string a = "{\"komut_no\":" + std::to_string(k.komut_no) + ",\"komut\":" + metin_json(k.yi_komut) +
                              ",\"emniyet_rev\":" + std::to_string(d.emniyet_rev) + ",\"durum\":" + metin_json(asama) + "}";
        olay_(11, 0, 1, k.komut_no, k.istemci_id, a);
    };
    const std::uint32_t rev0 = d.emniyet_rev;
    // A3-K95 adım 2: kısıtlayanlar (hepsi uygulanır)
    while (!d.kisitlayan.empty()) {
        const Komut k = d.kisitlayan.front();
        d.kisitlayan.pop_front();
        if (ayar_.kisitlayani_reddet) {
            sonuc_(k, false, "gecersiz_durum", {});  // yalnız sınama
            continue;
        }
        // W4-K19: kısıtlayan, beklenen_run_id uyuşmasa da uygulanır; yalnız canlı çalıştırma şartı.
        if (!d.run_id || !canli(d.durum)) {
            sonuc_(k, false, "canli_calistirma_yok", {});
            continue;
        }
        if (k.tur == KomutTuru::emniyet_degistir) {
            emniyet_uygula(k);
        } else {
            rev_artir();  // A3-K101: daraltıcı yer istasyonu komutu revizyonu artırır
            yi_uygula(k, "oturum_yok");
        }
    }
    const bool artti = d.emniyet_rev != rev0;
    // Diğerleri komut_no sırasıyla; A3-K95 adım 4: aynı sınırda revizyon arttıysa gevşetmeler reddedilir.
    while (!d.genel.empty()) {
        const Komut k = d.genel.front();
        d.genel.pop_front();
        const bool run_uyar = d.run_id.has_value() == k.beklenen_run_id.has_value() &&
                              (!d.run_id || *d.run_id == *k.beklenen_run_id);
        if (k.tur == KomutTuru::emniyet_degistir) {
            if (artti) {
                sonuc_(k, false, "kisitlama_hakim", "{\"guncel_emniyet_rev\":" + std::to_string(d.emniyet_rev) + "}");
            } else if (!run_uyar || !canli(d.durum)) {
                sonuc_(k, false, "beklenen_durum_uyusmuyor", {});
            } else if (d.doydu) {
                sonuc_(k, false, "emniyet_rev_doydu", {});
            } else if (!k.beklenen_emniyet_rev || *k.beklenen_emniyet_rev != d.emniyet_rev) {
                sonuc_(k, false, "emniyet_rev_uyusmuyor", "{\"guncel_emniyet_rev\":" + std::to_string(d.emniyet_rev) + "}");
            } else {
                emniyet_uygula(k);
            }
            continue;
        }
        if (k.tur == KomutTuru::yer_istasyonu_komut) {
            if (!run_uyar || !canli(d.durum)) {
                sonuc_(k, false, "beklenen_durum_uyusmuyor", {});
            } else if (k.yi_sinif == KomutSinifi::genisletici &&
                       (!k.beklenen_emniyet_rev || *k.beklenen_emniyet_rev != d.emniyet_rev)) {
                yi_uygula(k, "bayat_yetki");
            } else {
                yi_uygula(k, "oturum_yok");  // sahte dünyada kart yok
            }
            continue;
        }
        // Çalıştırma kontrolü: A3-K123 atomik yeniden denetim
        if (k.beklenen_durum_rev != d.durum_rev) {
            sonuc_(k, false, "durum_rev_uyusmuyor", "{\"guncel_durum_rev\":" + std::to_string(d.durum_rev) + "}");
            continue;
        }
        if (!run_uyar) {
            sonuc_(k, false, "beklenen_durum_uyusmuyor", {});
            continue;
        }
        if (!gecis_gecerli(d.durum, k.tur)) {
            sonuc_(k, false, "gecersiz_durum", {});
            continue;
        }
        ++d.durum_rev;  // A3-K122: uygulanan her çalıştırma kontrolü komutu
        switch (k.tur) {
        case KomutTuru::calistirma_baslat: {
            const auto t = std::chrono::system_clock::now();
            const auto gun = std::chrono::floor<std::chrono::days>(t);
            const std::chrono::year_month_day ymd{gun};
            const std::chrono::hh_mm_ss<std::chrono::seconds> hms{std::chrono::floor<std::chrono::seconds>(t - gun)};
            char b[32];
            std::snprintf(b, sizeof b, "%04d%02u%02u-%02d%02d%02d-%06x", static_cast<int>(ymd.year()),
                          static_cast<unsigned>(ymd.month()), static_cast<unsigned>(ymd.day()),
                          static_cast<int>(hms.hours().count()), static_cast<int>(hms.minutes().count()),
                          static_cast<int>(hms.seconds().count()), static_cast<unsigned>(d.rng() & 0xFFFFFFu));
            d.run_id = std::string(b);
            d.adim_no = 0;
            d.t_ns = 0;
            gecis_(2, nullptr);
            break;
        }
        case KomutTuru::calistirma_duraklat: gecis_(4, nullptr); break;
        case KomutTuru::calistirma_surdur: gecis_(3, nullptr); break;
        case KomutTuru::calistirma_adimla:
            d.adimla_kalan = k.periyot_sayisi * 4u;
            gecis_(3, "adimla");
            break;
        case KomutTuru::calistirma_hiz: d.carpan = k.carpan; break;
        case KomutTuru::calistirma_durdur:
            if (d.durum == 2) {
                d.run_id.reset();
                gecis_(1, nullptr);
            } else {
                gecis_(6, nullptr);
            }
            break;
        case KomutTuru::senaryo_yukle:   // sahte sim'de senaryo içeriği yok: durum yuklendi'ye döner (A3-K77)
        case KomutTuru::calistirma_sifirla:
            d.run_id.reset();
            d.adim_no = 0;
            d.t_ns = 0;
            d.adimla_kalan = 0;
            if (d.durum != 1) {
                gecis_(1, nullptr);
            }
            break;
        default: break;
        }
        sonuc_(k, true, {}, {});
    }
}

void SahteDunya::adim()
{
    std::lock_guard<std::mutex> kl(kilit_);
    Durum& d = *d_;
    uygula_();
    // İç geçişler (Tablo 4.7a): ön denetim sahte sim'de her zaman geçer.
    if (d.durum == 2) {
        ++d.durum_rev;
        gecis_(3, "on_denetim_gecti");
    } else if (d.durum == 3) {
        const auto adim = static_cast<std::uint32_t>(d.carpan <= 0.0 ? 40.0 : std::max(1.0, std::round(4.0 * d.carpan)));
        d.adim_no += adim;
        d.t_ns = static_cast<std::int64_t>(d.adim_no) * kAdimNs;
        if (d.adimla_kalan > 0) {
            d.adimla_kalan = d.adimla_kalan > adim ? d.adimla_kalan - adim : 0;
            if (d.adimla_kalan == 0) {
                ++d.durum_rev;
                gecis_(4, "adimla_bitti");
            }
        }
    }
    ++d.dongu;
    dongu_sayaci_.fetch_add(1, std::memory_order_relaxed);
    if (ayar_.veri) {
        yayinla_();
    }
}

void SahteDunya::yayinla_()
{
    Durum& d = *d_;
    const std::uint64_t n = d.dongu;
    const std::int64_t t = d.t_ns;
    const double ts = static_cast<double>(t) * 1e-9;
    // Araç: 10 m yarıçaplı çember, 1,5 m derinlik, 0,5 m/s (değerler fizik değildir).
    const double aci = 0.05 * ts;
    const double pn = 10.0 * std::cos(aci);
    const double pe = 10.0 * std::sin(aci);
    const double derinlik = 1.5;
    const double yon = aci + kPi / 2.0;
    const double qw = std::cos(yon / 2.0);
    const double qz = std::sin(yon / 2.0);
    auto olcum = [&](auto& y, std::uint16_t sensor) {
        y.sensor_id = sensor;
        y.inst = 0;
        y.seq = ++d.seq;
        y.flags = 1;
        y.window_ns = 0;
        y.t_hazir_ns = t;
        y.t_teslim_ns = t;
        y.fault_mask = 0;
        y.model_version = 1;
    };
    const std::string run = d.run_id.value_or("");
    if (n % 20 == 0) {
        yuk_calistirma_durum c;
        c.durum = d.durum;
        c.kip = static_cast<std::uint8_t>(d.kip);
        c.run_id = run;
        c.adim_no = d.adim_no;
        c.hedef_carpan = static_cast<float>(d.carpan);
        c.gecerli = 1;
        c.kademe_gorunum = 1;
        c.kademe_sonar = 1;
        c.kademe_kayit = 1;
        c.a1_oturum = 255;
        c.son_komut_no = d.son_komut_no;
        c.durum_rev = d.durum_rev;
        olc_([&] { y_.yayinla<Kanal::calistirma_durum>(t, c); });
        yuk_sayac_gercek_zaman s;
        s.a3_halka_tasma = y_.halka_tasma();
        olc_([&] { y_.yayinla<Kanal::sayac_gercek_zaman>(t, s); });
        yuk_emniyet_girdiler e;
        e.acil_durdurma = d.acil;
        e.silahlandirma_fisi = {d.fis[0], d.fis[1]};
        e.emniyet_pimi = {d.pim[0], d.pim[1]};
        e.son_degisim_adimi = d.son_degisim.value_or(kYok64);
        e.son_degisim_kaynagi = d.son_kaynak;
        e.emniyet_rev = d.emniyet_rev;
        e.emniyet_rev_doydu = d.doydu ? 1 : 0;
        olc_([&] { y_.yayinla<Kanal::emniyet_girdiler>(t, e); });
        const std::array<yuk_atis_durum_tupler, 2> tupler{{{0, 1, 0, -1, 0}, {1, 1, 0, -1, 0}}};
        yuk_atis_durum a;
        a.tupler = tupler;
        olc_([&] { y_.yayinla<Kanal::atis_durum>(t, a); });
        yuk_baglanti_durum b;
        b.a1_oturum = 255;
        olc_([&] { y_.yayinla<Kanal::baglanti_durum>(t, b); });
        std::array<std::uint8_t, 100> profil{};
        for (std::size_t i = 0; i < profil.size(); ++i) {
            profil[i] = static_cast<std::uint8_t>(i > 45 && i < 55 ? 220 : 10 + (d.rng() % 20));
        }
        yuk_ping1d p;
        olcum(p, 8);
        p.t_tx_ns = t;
        p.mesafe_m = 1.5f;
        p.guven = 0.9f;
        p.kademe = 1;
        p.profil = profil;
        olc_([&] { y_.yayinla<Kanal::sonar_ping1d>(t, p); });
    }
    if (n % 100 == 0) {
        yuk_ariza_etkin a;
        olc_([&] { y_.yayinla<Kanal::ariza_etkin>(t, a); });
        yuk_kart_cekirdek_sayaclari k;
        k.saglik = 3;
        olc_([&] { y_.yayinla<Kanal::kart_cekirdek_sayaclari>(t, k); });
    }
    if (n % 4 == 0) {
        std::array<yuk_fizik_durum_govdeler, 2> govdeler{};
        govdeler[0].govde_id = 1;
        govdeler[0].tur = 0;
        govdeler[0].kutle_durumu = 0;
        govdeler[0].p_n = {pn, pe, derinlik};
        govdeler[0].q_nb = {qw, 0.0, 0.0, qz};
        govdeler[0].nu = {0.5, 0.0, 0.0, 0.0, 0.0, 0.05};
        govdeler[0].a_n = {-0.025 * std::cos(aci), -0.025 * std::sin(aci), 0.0};
        govdeler[1].govde_id = 2;
        govdeler[1].tur = 2;
        govdeler[1].kutle_durumu = 255;
        govdeler[1].p_n = {15.0, 5.0, 2.0};
        govdeler[1].q_nb = {1.0, 0.0, 0.0, 0.0};
        std::array<yuk_fizik_durum_iticiler, 6> iticiler{};
        for (std::size_t i = 0; i < iticiler.size(); ++i) {
            iticiler[i].u = static_cast<float>(0.2 + 0.05 * static_cast<double>(i));
            iticiler[i].itki_n = iticiler[i].u * 40.0f;
            iticiler[i].akim_a = iticiler[i].u * 5.0f;
        }
        yuk_fizik_durum f;
        f.adim_no = d.adim_no;
        f.govdeler = govdeler;
        f.iticiler = iticiler;
        f.bara_gerilimi_v = 16.0f;
        f.paket_akimi_a = 3.0f;
        f.soc = 0.9f;
        olc_([&] { y_.yayinla<Kanal::fizik_durum>(t, f); });
        yuk_kart_durum k;
        k.kart_t_ns = t;
        k.silahli = 0;
        k.saglik = 3;
        k.kestirim_gecerli = 1;
        k.p_n = {static_cast<float>(pn + 0.05), static_cast<float>(pe - 0.05), static_cast<float>(derinlik)};
        k.v_n = {static_cast<float>(-0.5 * std::sin(aci)), static_cast<float>(0.5 * std::cos(aci)), 0.0f};
        k.q_nb = {static_cast<float>(qw), 0.0f, 0.0f, static_cast<float>(qz)};
        k.derinlik_m = static_cast<float>(derinlik);
        k.batarya_soc = 0.9f;
        olc_([&] { y_.yayinla<Kanal::kart_durum>(t, k); });
        yuk_basinc bs;
        olcum(bs, 3);
        bs.basinc_pa = static_cast<float>(101325.0 + 1000.0 * 9.81 * derinlik);
        bs.sicaklik_c = 20.0f;
        olc_([&] { y_.yayinla<Kanal::sensor_basinc>(t, bs); });
        yuk_dvl dv;
        olcum(dv, 5);
        dv.t_tx_ns = t;
        dv.hiz_sensor_mps = {0.5f, 0.0f, 0.0f};
        dv.irtifa_m = static_cast<float>(3.0 - derinlik);
        dv.fom_mps = 0.01f;
        dv.isin_gecerli = 0x0F;
        olc_([&] { y_.yayinla<Kanal::sensor_dvl>(t, dv); });
        yuk_batarya bt;
        olcum(bt, 6);
        bt.gerilim_v = 16.0f;
        bt.akim_a = 3.0f;
        olc_([&] { y_.yayinla<Kanal::sensor_batarya>(t, bt); });
    }
    const int ek = ek_ping_.load(std::memory_order_relaxed);
    const int satir = (d.durum == 3 && n % 2 == 0 ? 1 : 0) + ek;
    for (int i = 0; i < satir; ++i) {
        std::array<std::uint8_t, 200> ornek{};
        for (std::size_t k = 0; k < ornek.size(); ++k) {
            ornek[k] = static_cast<std::uint8_t>((k > 120 && k < 130) ? 200 : (d.rng() % 30));
        }
        yuk_ping360 p;
        olcum(p, 7);
        p.t_tx_ns = t;
        d.ping_aci = static_cast<std::uint16_t>((d.ping_aci + 1u) % 400u);
        p.aci_grad = d.ping_aci;
        p.n_ornek = static_cast<std::uint16_t>(ornek.size());
        p.ornek_periyodu = 80;
        p.darbe_suresi_us = 80;
        p.frekans_khz = 750;
        p.kademe = 1;
        p.ses_hizi_mps = 1481.0f;
        p.ornekler = ornek;
        olc_([&] { y_.yayinla<Kanal::sonar_ping360>(t, p); });
    }
}

}  // namespace ida::a3::sahte
