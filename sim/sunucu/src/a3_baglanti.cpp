// A3 komut alıcısı ve dönüşümler (a3_baglanti.hpp).
#include "a3_baglanti.hpp"

#include "ida/senaryo/senaryo.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <random>

namespace ida::sunucu {

namespace {

using a3::AlimSonucu;
using a3::AnlikDurum;
using a3::CalistirmaDurumu;
using a3::Komut;
using a3::KomutTuru;

AlimSonucu ret(std::string kod, std::string ileti, std::string ayrinti = {})
{
    AlimSonucu a;
    a.tur = AlimSonucu::Tur::ret;
    a.hata_kodu = std::move(kod);
    a.ileti = std::move(ileti);
    a.ayrinti_json = std::move(ayrinti);
    return a;
}

std::string json_metin(std::string_view s)
{
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

}  // namespace

namespace a3b {

bool canli(CalistirmaDurumu d)
{
    // A3-K91: canlı çalıştırma hazirlaniyor, kosuyor ve duraklatildi'dır (W4-duraklat).
    return d == CalistirmaDurumu::hazirlaniyor || d == CalistirmaDurumu::kosuyor || d == CalistirmaDurumu::duraklatildi;
}

const char* durum_adi(CalistirmaDurumu d)
{
    static constexpr const char* k[] = {"bos", "yuklendi", "hazirlaniyor", "kosuyor", "duraklatildi",
                                         "bitti", "durduruldu", "hata", "izleme"};
    const auto i = static_cast<std::size_t>(d);
    return i < 9u ? k[i] : "bos";
}

bool gecis_gecerli(CalistirmaDurumu d, KomutTuru t)
{
    using D = CalistirmaDurumu;
    switch (t) {
    case KomutTuru::calistirma_baslat: return d == D::yuklendi;
    // W4-duraklat (Tablo 4.7a): duraklat kosuyor'dan; surdur ve adimla duraklatildi'dan.
    case KomutTuru::calistirma_duraklat: return d == D::kosuyor;
    case KomutTuru::calistirma_surdur:
    case KomutTuru::calistirma_adimla: return d == D::duraklatildi;
    case KomutTuru::calistirma_hiz: return d == D::kosuyor || d == D::duraklatildi;
    case KomutTuru::calistirma_durdur: return d == D::hazirlaniyor || d == D::kosuyor || d == D::duraklatildi;
    case KomutTuru::calistirma_sifirla:
        return d == D::yuklendi || d == D::hazirlaniyor || d == D::kosuyor || d == D::duraklatildi || d == D::bitti ||
               d == D::durduruldu || d == D::hata;
    default: return false;
    }
}

bool calistirma_mi(KomutTuru t) { return t != KomutTuru::emniyet_degistir && t != KomutTuru::yer_istasyonu_komut; }

}  // namespace a3b

using a3b::calistirma_mi;
using a3b::canli;
using a3b::durum_adi;
using a3b::gecis_gecerli;

A3Alici::A3Alici(bool hil) : hil_(hil), halka_(kGenelKuyruk + 1u)
{
    AnlikPod p;
    p.kip = static_cast<std::uint8_t>(hil ? a3::ZamanKipi::hil_1x : a3::ZamanKipi::sil);
    tampon_.fill(p);
}

A3Alici::~A3Alici() = default;

// Üçlü tampon, dünya tarafı: kilit almaz, bellek ayırmaz (düz yapının kopyası).
void A3Alici::anlik_yaz(const AnlikPod& p)
{
    tampon_[arka_] = p;
    arka_ = orta_.exchange(arka_ | 4u, std::memory_order_acq_rel) & 3u;
}

AnlikDurum A3Alici::anlik()
{
    AnlikPod p;
    {
        std::lock_guard<std::mutex> k(okuyucu_m_);
        if ((orta_.load(std::memory_order_acquire) & 4u) != 0u) {
            on_ = orta_.exchange(on_, std::memory_order_acq_rel) & 3u;
        }
        p = tampon_[on_];
    }
    AnlikDurum a;
    a.durum = static_cast<CalistirmaDurumu>(p.durum);
    a.kip = static_cast<a3::ZamanKipi>(p.kip);
    if (p.run_var) {
        a.run_id = std::string(p.run_id.data());
    }
    a.sim_t_ns = p.sim_t_ns;
    a.adim_no = p.adim_no;
    a.gecerli = p.gecerli;
    a.gercek_zamani_kacirdi = p.gercek_zamani_kacirdi;
    a.gecerlilik_nedenleri = p.gecerlilik_nedenleri;
    // Kademeler: 3B görünüm ve kamera ida_sim'de yoktur (kapali); sonar `--sonar` ile sade
    // (W4-bagla-sonar); kayıt (A6) yok: en alt kademe (sözleşmenin değer kümesinde "yok" yok; rapor).
    a.kademe_gorunum = 0;
    a.kademe_kamera = 0;
    a.kademe_sonar = p.kademe_sonar;
    a.kademe_kayit = 0;
    if (p.ozet_var) {
        a.senaryo_ozeti = std::string(p.ozet.data());
    }
    a.a1_run_id = p.a1_run_id;
    a.durum_rev = p.durum_rev;
    a.emniyet_rev = p.emniyet_rev;
    a.emniyet_rev_doydu = p.emniyet_rev_doydu;
    a.acil_durdurma = p.acil;
    a.silahlandirma_fisi = p.fis;
    a.emniyet_pimi = p.pim;
    if (p.son_degisim_var) {
        a.son_degisim_adimi = p.son_degisim_adimi;
    }
    a.sayaclar = {{"parti_gonderilen", p.sayac[0]}, {"parti_kacirildi", p.sayac[1]}, {"oturum_kopma", p.sayac[2]},
                  {"sim_gecikmesi", p.sayac[3]}, {"a3_halka_tasma", p.sayac[4]}};
    return a;
}

int A3Alici::etki_yuvasi(const Komut& k)
{
    if (k.tur == KomutTuru::emniyet_degistir) {
        const int t = k.tup == 1 ? 1 : 0;
        switch (k.girdi) {
        case a3::EmniyetGirdisi::acil_durdurma: return 0;
        case a3::EmniyetGirdisi::silahlandirma_fisi: return 1 + t;
        case a3::EmniyetGirdisi::emniyet_pimi: return 3 + t;
        }
        return -1;
    }
    if (k.tur == KomutTuru::yer_istasyonu_komut) {
        if (k.yi_komut == "disarm") {
            return 5;
        }
        if (k.yi_komut == "set_mode_daraltici") {
            return 6;
        }
        if (k.yi_komut == "fc_disarm") {
            return 7;
        }
        if (k.yi_komut == "fc_abort") {
            return 8;
        }
    }
    return -1;
}

bool A3Alici::yola_koy(DenetimYolu& y, A3Giris&& g)
{
    const std::size_t s = y.son.load(std::memory_order_relaxed);
    const std::size_t n = (s + 1u) % y.g.size();
    if (n == y.bas.load(std::memory_order_acquire)) {
        return false;
    }
    y.g[s] = std::move(g);
    y.son.store(n, std::memory_order_release);
    return true;
}

void A3Alici::yoldan_al(DenetimYolu& y, std::vector<A3Giris>& cikti)
{
    const std::size_t son = y.son.load(std::memory_order_acquire);
    std::size_t b = y.bas.load(std::memory_order_relaxed);
    while (b != son) {
        cikti.push_back(std::move(y.g[b]));
        b = (b + 1u) % y.g.size();
    }
    y.bas.store(b, std::memory_order_release);
}

AlimSonucu A3Alici::kuyruga(const Komut& k, const a3b::YiKarsilik& yi)
{
    // uretici_m_ çağıranda (al) tutulur.
    if (calistirma_mi(k.tur)) {
        // EMNİYET (W4-K28 sol-3 #1): çalıştırma denetimi genel halkadan bağımsız yoldan.
        A3Giris g;
        g.sira = ++sira_;
        g.k = k;
        AlimSonucu a;
        a.tur = yola_koy(k.tur == KomutTuru::calistirma_durdur ? durdur_ : denetim_, std::move(g))
                    ? AlimSonucu::Tur::kuyrukta
                    : AlimSonucu::Tur::dolu;
        return a;
    }
    const int y = k.kisitlayan ? etki_yuvasi(k) : -1;
    if (y >= 0) {
        // EMNİYET (W4-K22 (a)): etki başına sabit boylu yuva; doluysa kitaplığın mandalı bekletir
        // (atılmaz). Genel halkanın doluluğu bu yolu etkilemez.
        Yuva& v = kisit_[static_cast<std::size_t>(y)];
        if (v.dolu.load(std::memory_order_acquire)) {
            AlimSonucu a;
            a.tur = AlimSonucu::Tur::dolu;
            return a;
        }
        v.g.sira = ++sira_;
        v.g.k = k;
        v.g.yi = yi;
        v.sira.store(v.g.sira, std::memory_order_relaxed);
        v.dolu.store(true, std::memory_order_release);
        AlimSonucu a;
        a.tur = AlimSonucu::Tur::kuyrukta;
        return a;
    }
    const std::size_t s = son_.load(std::memory_order_relaxed);
    const std::size_t n = (s + 1u) % halka_.size();
    if (n == bas_.load(std::memory_order_acquire)) {
        AlimSonucu a;
        a.tur = AlimSonucu::Tur::dolu;   // A3-K59 (kısıtlayan bu yola düşmez; düşerse kitaplık mandalı)
        return a;
    }
    halka_[s].sira = ++sira_;
    halka_[s].k = k;
    halka_[s].yi = yi;
    son_.store(n, std::memory_order_release);
    AlimSonucu a;
    a.tur = AlimSonucu::Tur::kuyrukta;
    return a;
}

void A3Alici::cek(std::vector<A3Giris>& kisitlayan, std::vector<A3Giris>& genel, std::uint16_t atla,
                  std::size_t genel_azami)
{
    // EMNİYET (W4-K27 (2)): önce kesim. Kesimdeki her genel isteğin yayınlanması (release),
    // ondan önce teslim edilmiş kısıtlayanların yuvaya yazılmasından sonradır; burada acquire ile
    // okunan kesimden sonra yuvalar onları gösterir.
    const std::size_t kesim = son_.load(std::memory_order_acquire);
    if (cek_kancasi) {
        cek_kancasi(1);
    }
    const std::size_t k0 = kisitlayan.size();
    for (std::size_t i = 0; i < kisit_.size(); ++i) {
        if (((atla >> i) & 1u) != 0u) {
            continue;
        }
        Yuva& v = kisit_[i];
        if (v.dolu.load(std::memory_order_acquire)) {
            kisitlayan.push_back(std::move(v.g));
            v.sira.store(0, std::memory_order_relaxed);
            v.dolu.store(false, std::memory_order_release);
        }
    }
    std::sort(kisitlayan.begin() + static_cast<std::ptrdiff_t>(k0), kisitlayan.end(),
              [](const A3Giris& x, const A3Giris& y) { return x.sira < y.sira; });
    if (cek_kancasi) {
        cek_kancasi(2);
    }
    std::size_t b = bas_.load(std::memory_order_relaxed);
    for (std::size_t n = 0; b != kesim && n < genel_azami; ++n) {
        genel.push_back(std::move(halka_[b]));
        b = (b + 1u) % halka_.size();
    }
    bas_.store(b, std::memory_order_release);
    // Çalıştırma denetimi: depo doluluğundan bağımsız, bütünüyle; önce durdur (W4-K28 sol-3 #1).
    yoldan_al(durdur_, genel);
    yoldan_al(denetim_, genel);
}

std::uint64_t A3Alici::bekleyen_kisit_en_kucuk(std::uint16_t maske) const
{
    std::uint64_t en = 0;
    for (std::size_t i = 0; i < kisit_.size(); ++i) {
        if (((maske >> i) & 1u) == 0u || !kisit_[i].dolu.load(std::memory_order_acquire)) {
            continue;
        }
        const std::uint64_t s = kisit_[i].sira.load(std::memory_order_relaxed);
        if (s != 0u && (en == 0u || s < en)) {
            en = s;
        }
    }
    return en;
}

std::size_t A3Alici::genel_dolu() const
{
    const std::size_t b = bas_.load(std::memory_order_acquire);
    const std::size_t s = son_.load(std::memory_order_acquire);
    return (s + halka_.size() - b) % halka_.size();
}

AlimSonucu A3Alici::al(const Komut& k)
{
    // EMNİYET (W4-K28 (3)): görüntünün okunması, denetim ve kuyruğa koyma tek kilit altında
    // (gecis_kilidi): dünyanın koşu dışı iç geçişi bu üçünün arasına giremez.
    std::lock_guard<std::mutex> kl(uretici_m_);
    const AnlikDurum a = anlik();
    const bool run_uyar = a.run_id.has_value() == k.beklenen_run_id.has_value() &&
                          (!a.run_id || *a.run_id == *k.beklenen_run_id);
    auto run_ret = [&] {
        std::string ay = "{\"guncel_run_id\":" + (a.run_id ? json_metin(*a.run_id) : std::string("null")) +
                         ",\"guncel_durum\":" + json_metin(durum_adi(a.durum)) +
                         ",\"guncel_durum_rev\":" + std::to_string(a.durum_rev) + "}";
        return ret("beklenen_durum_uyusmuyor", "beklenen_run_id güncel değil (A3-K54, K124)", ay);
    };
    if (calistirma_mi(k.tur)) {
        // A3-K123: eşzamanlı denetim; dünya uygulamada aynı atomik adımda yeniden denetler.
        if (k.beklenen_durum_rev != a.durum_rev) {
            return ret("durum_rev_uyusmuyor", "beklenen_durum_rev güncel değil (A3-K123)",
                       "{\"guncel_durum_rev\":" + std::to_string(a.durum_rev) + "}");
        }
        if (!run_uyar) {
            return run_ret();
        }
        if (!gecis_gecerli(a.durum, k.tur)) {
            return ret("gecersiz_durum", "komut bu çalıştırma durumunda geçersiz (Tablo 4.7a)");
        }
        if (hil_ && (k.tur == KomutTuru::calistirma_hiz || k.tur == KomutTuru::calistirma_adimla)) {
            return ret("kipte_gecersiz", "HIL kiplerinde geçersiz (Tablo 4.7b)");
        }
        if (hil_ && k.tur == KomutTuru::calistirma_duraklat && !k.gecersizligi_onayla) {
            // Tablo 4.7b: HIL'de duraklatma çalıştırmayı geçersiz kılar (A1-K41, K79); onay ister.
            return ret("onay_gerekli", "HIL'de duraklatma çalıştırmayı geçersiz kılar: gecersizligi_onayla gerekli (Tablo 4.7b)");
        }
        if (k.tur == KomutTuru::calistirma_adimla && (k.periyot_sayisi < 1u || k.periyot_sayisi > 10000u)) {
            return ret("gecersiz_deger", "periyot_sayisi 1–10 000 (A3-K65)");
        }
        if (k.tur == KomutTuru::calistirma_sifirla && a.durum != CalistirmaDurumu::yuklendi) {
            // Karşılığı olmayan istek (W4-bagla-a3 kapsam 3): ida_sim bu dilimde tek çalıştırma koşar;
            // çalıştırmayı başa saran sıfırlama yoktur.
            return ret("gecersiz_durum",
                       "ida_sim bu dilimde tek çalıştırma koşar: sıfırlama yalnız 'yuklendi' durumunda (A3-K67 bu dilimde yok)");
        }
        return kuyruga(k, a3b::YiKarsilik{});
    }
    // Emniyet ve yer istasyonu (A3-K54, K91, K118; EMNİYET W4-K19).
    if (!k.kisitlayan) {
        if (!k.beklenen_run_id) {
            return ret("canli_calistirma_yok", "canlı çalıştırma gerektiren uçta beklenen_run_id null (A3-K54)");
        }
        if (!run_uyar) {
            return run_ret();
        }
    }
    if (!a.run_id || !canli(a.durum)) {
        return ret("canli_calistirma_yok", "canlı çalıştırma yok (A3-K91)");
    }
    const bool revli = !k.kisitlayan && (k.tur == KomutTuru::emniyet_degistir || k.yi_sinif == a3::KomutSinifi::genisletici);
    if (revli) {
        if (a.emniyet_rev_doydu) {
            return ret("emniyet_rev_doydu", "emniyet revizyonu doydu (A3-K117)");
        }
        if (!k.beklenen_emniyet_rev || *k.beklenen_emniyet_rev != a.emniyet_rev) {
            return ret("emniyet_rev_uyusmuyor", "beklenen_emniyet_rev güncel değil (A3-K118)",
                       "{\"guncel_emniyet_rev\":" + std::to_string(a.emniyet_rev) + "}");
        }
    }
    a3b::YiKarsilik yi;
    if (k.tur == KomutTuru::yer_istasyonu_komut && !a3b::yi_karsilik(k.yi_komut, k.yi_parametreler_json, yi)) {
        // Kitaplık parametreleri katalog şemasıyla doğruladı; buraya düşmek bir dönüşüm hatasıdır.
        return ret("gecersiz_deger", "yer istasyonu komutunun A1 karşılığı çözülemedi");
    }
    return kuyruga(k, yi);
}

namespace a3b {

std::uint32_t gecerlilik_bitleri(std::uint32_t k, std::uint64_t beklenmeyen_sifirlama, std::uint64_t sensor_kuyruk_tasma)
{
    // kopru::Gecersizlik bit i → A3 Tablo 4.8 biti.
    static constexpr unsigned kEsle[11] = {0u, 1u, 3u, 4u, 5u, 6u, 7u, 8u, 9u, 11u, 2u};
    std::uint32_t r = 0;
    for (unsigned i = 0; i < 11u; ++i) {
        if (((k >> i) & 1u) != 0u) {
            r |= 1u << kEsle[i];
        }
    }
    if (beklenmeyen_sifirlama > 0u) {
        r |= 1u << 10u;
    }
    if (sensor_kuyruk_tasma > 0u) {
        r |= 1u << 18u;
    }
    return r;
}

std::uint32_t gecersiz_kilan(bool hil)
{
    return hil ? (0x0FFFu & ~(1u << 4u)) | (1u << 17u) | (1u << 18u) : (1u << 8u) | (1u << 9u) | (1u << 10u);
}

std::string_view neden_adi(unsigned bit)
{
    static constexpr std::string_view k[] = {"parti_kacirildi", "son_tarih_kacirildi", "sim_gecikmesi",
                                             "cagri_cakismasi", "bariyer_bekleme", "olay_gec",
                                             "esz_guvenli_uygulanan", "yanit_ret_gec", "pencere_tasma",
                                             "planlanmamis_oturum_sonu", "beklenmeyen_sifirlama", "hil_duraklatma"};
    if (bit < 12u) {
        return k[bit];
    }
    switch (bit) {
    case 16u: return "sonar_gec";
    case 17u: return "sonar_kuyruk_dolu";
    case 18u: return "sensor_kuyruk_tasma";
    case 19u: return "cagri_izi_eksik";
    default: return {};
    }
}

void JsonYaz::ekle_(std::string_view s)
{
    // Kapanış '}' için bir bayt ayrılır; sığmazsa yazıcı taşmış sayılır ve "{}" döner.
    if (tasti_ || s.size() + 1u > t_.size() - n_) {
        tasti_ = true;
        return;
    }
    for (const char c : s) {
        if (n_ < t_.size()) {
            t_[n_] = c;
            ++n_;
        }
    }
}

void JsonYaz::ad_(const char* ad)
{
    ekle_(ilk_ ? "{\"" : ",\"");
    ilk_ = false;
    ekle_(ad);
    ekle_("\":");
}

JsonYaz& JsonYaz::metin(const char* ad, std::string_view v)
{
    ad_(ad);
    ekle_("\"");
    for (const char c : v) {
        if (static_cast<unsigned char>(c) < 0x20u) {
            continue;   // A3-K121: denetim karakteri yazılmaz (yalnız iç metinler gelir)
        }
        if (c == '"' || c == '\\') {
            ekle_("\\");
        }
        ekle_(std::string_view(&c, 1));
    }
    ekle_("\"");
    return *this;
}

JsonYaz& JsonYaz::sayi(const char* ad, long long v)
{
    ad_(ad);
    char b[32];
    const int n = std::snprintf(b, sizeof b, "%lld", v);
    ekle_(std::string_view(b, n > 0 ? static_cast<std::size_t>(n) : 0u));
    return *this;
}

JsonYaz& JsonYaz::sayi_u(const char* ad, unsigned long long v)
{
    ad_(ad);
    char b[32];
    const int n = std::snprintf(b, sizeof b, "%llu", v);
    ekle_(std::string_view(b, n > 0 ? static_cast<std::size_t>(n) : 0u));
    return *this;
}

JsonYaz& JsonYaz::mantik(const char* ad, bool v)
{
    ad_(ad);
    ekle_(v ? "true" : "false");
    return *this;
}

JsonYaz& JsonYaz::ham(const char* ad, std::string_view json)
{
    ad_(ad);
    ekle_(json);
    return *this;
}

std::string_view JsonYaz::bitir()
{
    if (ilk_) {
        ekle_("{");
        ilk_ = false;
    }
    if (!tasti_ && n_ < t_.size()) {
        t_[n_] = '}';
        ++n_;
    } else {
        tasti_ = true;
    }
    return tasti_ ? std::string_view("{}") : std::string_view(t_.data(), n_);
}

bool yi_karsilik(const std::string& komut, const std::string& parametreler_json, YiKarsilik& k)
{
    k = YiKarsilik{};
    static constexpr const char* kAdlar[] = {"arm", "disarm", "set_mode_daraltici", "set_mode_genisletici", "manual",
                                             "guided_sp", "mission_upload", "mission_ctl", "param_get", "param_apply",
                                             "param_save", "failsafe_reset", "fc_arm", "fc_fire", "fc_disarm",
                                             "fc_abort", "fc_reset", "fc_test_enter", "fc_test_exit"};
    for (const char* ad : kAdlar) {
        if (komut == ad) {
            k.ad = ad;
        }
    }
    const nlohmann::json p = nlohmann::json::parse(parametreler_json, nullptr, false);
    if (p.is_discarded() || !p.is_object()) {
        return false;
    }
    auto basit = [&k](std::uint8_t tur, std::uint16_t arg) {
        k.tur = YiTur::basit;
        k.a1_tur = tur;
        k.arg = arg;
        return true;
    };
    auto fc = [&k, &p](std::uint8_t op, bool tup_var) {
        k.tur = YiTur::fc;
        k.a1_tur = op;
        k.arg = 255u;
        if (tup_var) {
            const auto it = p.find("tube");
            if (it == p.end() || !it->is_number_integer()) {
                return false;
            }
            const auto t = it->get<long long>();
            if (t < 0 || t > 1) {
                return false;
            }
            k.arg = static_cast<std::uint16_t>(t);
        }
        return true;
    };
    // protocol/komutlar.json (A1-K125): IDAW_CMD_TYPE, IDAW_MODE, IDAW_MCTL, IDAW_FC_OP değerleri.
    if (komut == "arm") {
        return basit(2u, 0u);
    }
    if (komut == "disarm") {
        return basit(3u, 0u);
    }
    if (komut == "failsafe_reset") {
        return basit(15u, 0u);
    }
    if (komut == "set_mode_daraltici" || komut == "set_mode_genisletici") {
        const auto it = p.find("mode");
        if (it == p.end() || !it->is_string()) {
            return false;
        }
        static const std::pair<const char*, std::uint16_t> kKip[] = {
            {"MANUAL", 2u}, {"STABILIZE", 3u}, {"DEPTH_HOLD", 4u}, {"STATION_KEEP", 5u}, {"GUIDED", 6u}, {"AUTO", 7u}};
        const std::string m = it->get<std::string>();
        for (const auto& [ad, v] : kKip) {
            if (m == ad) {
                const bool dar = v <= 5u;
                if (dar != (komut == "set_mode_daraltici")) {
                    return false;
                }
                return basit(4u, v);
            }
        }
        return false;
    }
    if (komut == "mission_ctl") {
        const auto it = p.find("action");
        if (it == p.end() || !it->is_string()) {
            return false;
        }
        static const std::pair<const char*, std::uint16_t> kEylem[] = {
            {"START", 1u}, {"PAUSE", 2u}, {"RESUME", 3u}, {"ABORT", 4u}};
        const std::string m = it->get<std::string>();
        for (const auto& [ad, v] : kEylem) {
            if (m == ad) {
                return basit(10u, v);
            }
        }
        return false;
    }
    if (komut == "param_get") {
        const auto it = p.find("param_id");
        if (it == p.end() || !it->is_number_integer()) {
            return false;
        }
        const auto v = it->get<long long>();
        if (v < 0 || v > 65535) {
            return false;
        }
        return basit(12u, static_cast<std::uint16_t>(v));
    }
    if (komut == "guided_sp") {
        const auto m = p.find("mask");
        if (m == p.end() || !m->is_array()) {
            return false;
        }
        static const std::pair<const char*, std::uint8_t> kMaske[] = {{"POS_NE", 1u}, {"DEPTH", 2u}, {"ALTITUDE", 4u},
                                                                       {"VEL_NE", 8u}, {"VEL_D", 16u}, {"YAW", 32u},
                                                                       {"YAW_RATE", 64u}};
        for (const auto& e : *m) {
            if (!e.is_string()) {
                return false;
            }
            bool bulundu = false;
            for (const auto& [ad, v] : kMaske) {
                if (e.get<std::string>() == ad) {
                    k.sp_mask = static_cast<std::uint8_t>(k.sp_mask | v);
                    bulundu = true;
                }
            }
            if (!bulundu) {
                return false;
            }
        }
        auto sayi = [&p](const char* ad, float& f) {
            const auto it = p.find(ad);
            if (it == p.end()) {
                return true;   // A4-K50: verilmeyen alan 0 yazılır
            }
            if (!it->is_number()) {
                return false;
            }
            f = static_cast<float>(it->get<double>());
            return true;
        };
        auto dizi = [&p](const char* ad, float* f, std::size_t n) {
            const auto it = p.find(ad);
            if (it == p.end()) {
                return true;
            }
            if (!it->is_array() || it->size() != n) {
                return false;
            }
            for (std::size_t i = 0; i < n; ++i) {
                if (!(*it)[i].is_number()) {
                    return false;
                }
                f[i] = static_cast<float>((*it)[i].get<double>());
            }
            return true;
        };
        if (!sayi("depth", k.depth) || !sayi("altitude", k.altitude) || !sayi("yaw", k.yaw) ||
            !sayi("yaw_rate", k.yaw_rate) || !dizi("pos_ne", k.pos_ne.data(), 2u) || !dizi("vel_ned", k.vel_ned.data(), 3u)) {
            return false;
        }
        k.tur = YiTur::guided;
        return true;
    }
    if (komut == "fc_arm") {
        return fc(1u, true);
    }
    if (komut == "fc_disarm") {
        return fc(2u, false);
    }
    if (komut == "fc_abort") {
        return fc(3u, false);
    }
    if (komut == "fc_fire") {
        // Atış kontrol sınırı: yalnız katalogdaki biçimiyle taşınan komut (A3-K100); köprüde
        // IDA_CMD_FC karşılığı vardır, aynen iletilir. Burada atış mantığı yoktur.
        return fc(5u, true);
    }
    if (komut == "fc_reset") {
        return fc(6u, true);
    }
    if (komut == "fc_test_enter") {
        return fc(7u, true);
    }
    if (komut == "fc_test_exit") {
        return fc(8u, true);
    }
    // manual (köprüde IDA_CMD_MANUAL yolu yok), mission_upload, param_apply, param_save (bileşik;
    // A1-K125 ağ geçidi yürütmesi bu dilimde yok): desteklenmiyor.
    k.tur = YiTur::desteklenmiyor;
    return true;
}

std::string_view ack_adi(std::uint8_t ack)
{
    switch (ack) {
    case 1u: return "ACCEPTED";
    case 2u: return "REJECTED";
    case 3u: return "IN_PROGRESS";
    case 4u: return "DUPLICATE";
    case 5u: return "UNSUPPORTED";
    default: return {};
    }
}

std::string run_id_uret()
{
    const auto t = std::chrono::system_clock::now();
    const auto gun = std::chrono::floor<std::chrono::days>(t);
    const std::chrono::year_month_day ymd{gun};
    const std::chrono::hh_mm_ss<std::chrono::seconds> hms{std::chrono::floor<std::chrono::seconds>(t - gun)};
    std::random_device rd;
    const unsigned r = static_cast<unsigned>(rd()) & 0xFFFFFFu;
    char b[40];
    std::snprintf(b, sizeof b, "%04d%02u%02u-%02d%02d%02d-%06x", static_cast<int>(ymd.year()),
                  static_cast<unsigned>(ymd.month()), static_cast<unsigned>(ymd.day()),
                  static_cast<int>(hms.hours().count()), static_cast<int>(hms.minutes().count()),
                  static_cast<int>(hms.seconds().count()), r);
    return std::string(b);
}

std::uint32_t a1_run_id(const std::string& run_id)
{
    const std::string h = senaryo::sha256_hex(run_id);
    std::uint32_t v = 0;
    for (std::size_t i = 0; i < 4u && 2u * i + 1u < h.size(); ++i) {
        const auto bayt = static_cast<std::uint32_t>(std::stoul(h.substr(2u * i, 2u), nullptr, 16));
        v |= bayt << (8u * static_cast<unsigned>(i));
    }
    return v == 0u ? 1u : v;
}

}  // namespace a3b

}  // namespace ida::sunucu
