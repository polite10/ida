// Senaryo ve ön denetim uçlarının sim tarafı veri kaynağı (senaryo_hizmeti.hpp).
#include "senaryo_hizmeti.hpp"

#include "senaryo_plani.hpp"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <ctime>
#include <fstream>
#include <iterator>
#include <system_error>

namespace ida::sunucu {

namespace {

using Saat = std::chrono::steady_clock;

constexpr std::size_t kAzamiListe = 1000;       // listelenen en çok dosya
constexpr std::size_t kAzamiAday = 16;          // hizmette tutulan aday (eskisi atılır)
constexpr std::size_t kAzamiSonuc = 16;
constexpr std::size_t kAzamiRapor = 64;         // A3-K62: son 64 ön denetim raporu

std::string utc_metni(std::chrono::system_clock::time_point t)
{
    const std::time_t s = std::chrono::system_clock::to_time_t(t);
    std::tm g{};
#ifdef _WIN32
    gmtime_s(&g, &s);
#else
    gmtime_r(&s, &g);
#endif
    char b[32];
    std::snprintf(b, sizeof b, "%04d-%02d-%02dT%02d:%02d:%02dZ", g.tm_year + 1900, g.tm_mon + 1, g.tm_mday, g.tm_hour,
                  g.tm_min, g.tm_sec);
    return b;
}

// Dosya saatinden UTC (std::chrono::clock_cast her standart kitaplıkta yok: iki saatin şimdiki farkı).
std::string dosya_zamani(const std::filesystem::file_time_type& ft)
{
    using namespace std::chrono;
    const auto sys = system_clock::now() + duration_cast<system_clock::duration>(ft - std::filesystem::file_time_type::clock::now());
    return utc_metni(sys);
}

bool yol_parcasi_gecerli(const std::string& p)
{
    if (p.empty() || p == "." || p == "..") {
        return false;
    }
    for (const char c : p) {
        const auto u = static_cast<unsigned char>(c);
        if (u < 0x20u || u == 0x7Fu || c == '\\' || c == ':' || c == '*' || c == '?' || c == '"' || c == '<' || c == '>' ||
            c == '|') {
            return false;
        }
    }
    return true;
}

std::string kirp_sonu(std::string s)
{
    while (!s.empty() && s.back() == '/') {
        s.pop_back();
    }
    return s;
}

}  // namespace

std::optional<std::string> KatmanliKaynak::oku(const std::string& yol) const
{
    const auto it = katman_.find(yol);
    if (it != katman_.end()) {
        return it->second;
    }
    return disk_.oku(yol);
}

std::string SenaryoHizmeti::dizin_denetle(const std::string& dizin)
{
    const std::string d = kirp_sonu(dizin);
    if (d.empty()) {
        return "senaryo dizini boş olamaz";
    }
    if (d.front() == '/' ) {
        return "senaryo dizini depo köküne göre göreli olmalı (mutlak yol olamaz): " + dizin;
    }
    std::size_t bas = 0;
    while (bas <= d.size()) {
        const std::size_t son = d.find('/', bas);
        const std::string parca = d.substr(bas, son == std::string::npos ? std::string::npos : son - bas);
        if (!yol_parcasi_gecerli(parca)) {
            return "senaryo dizini geçersiz (\"..\", \".\", boş parça, ters eğik çizgi, sürücü harfi ve özel karakter yok): " + dizin;
        }
        if (son == std::string::npos) {
            break;
        }
        bas = son + 1u;
    }
    return {};
}

SenaryoHizmeti::SenaryoHizmeti(Ayar ayar) : ayar_(std::move(ayar))
{
    ayar_.dizin = kirp_sonu(ayar_.dizin);
    dizin_ = std::filesystem::path(ayar_.depo_koku.empty() ? "." : ayar_.depo_koku) / std::filesystem::path(ayar_.dizin);
}

std::vector<a3::SenaryoDosyasi> SenaryoHizmeti::listele()
{
    std::vector<a3::SenaryoDosyasi> liste;
    std::error_code ec;
    std::filesystem::directory_iterator it(dizin_, ec);
    if (ec) {
        return liste;
    }
    for (const std::filesystem::directory_iterator son; it != son; it.increment(ec)) {
        if (ec) {
            break;
        }
        const std::string ad = it->path().filename().string();
        if (!a3::senaryo_adi_gecerli(ad)) {
            continue;
        }
        std::error_code e1;
        const std::filesystem::file_status st = std::filesystem::symlink_status(it->path(), e1);
        if (e1 || st.type() != std::filesystem::file_type::regular) {
            continue;   // simgesel bağlar ve dizinler listelenmez
        }
        a3::SenaryoDosyasi d;
        d.ad = ad;
        std::error_code e2;
        const auto boy = std::filesystem::file_size(it->path(), e2);
        d.boyut_bayt = e2 ? 0u : static_cast<std::uint64_t>(boy);
        std::error_code e3;
        const auto zaman = std::filesystem::last_write_time(it->path(), e3);
        if (!e3) {
            d.degisim_utc = dosya_zamani(zaman);
        }
        liste.push_back(std::move(d));
        if (liste.size() >= kAzamiListe) {
            break;
        }
    }
    std::sort(liste.begin(), liste.end(), [](const a3::SenaryoDosyasi& a, const a3::SenaryoDosyasi& b) { return a.ad < b.ad; });
    return liste;
}

a3::DosyaDurumu SenaryoHizmeti::dosya_oku_(const std::string& ad, std::string& icerik, std::string& ileti) const
{
    // A3-K78: kalıba uymayan ad dosya sistemine hiç gitmez.
    if (!a3::senaryo_adi_gecerli(ad)) {
        ileti = "senaryo adı ^[a-z0-9_-]{1,64}\\.toml$ kalıbına uymuyor (A3-K78)";
        return a3::DosyaDurumu::gecersiz;
    }
    const std::filesystem::path p = dizin_ / ad;
    std::error_code ec;
    const std::filesystem::file_status st = std::filesystem::symlink_status(p, ec);
    if (ec == std::errc::no_such_file_or_directory || ec == std::errc::not_a_directory ||
        (!ec && st.type() == std::filesystem::file_type::not_found)) {
        return a3::DosyaDurumu::yok;
    }
    if (ec) {
        ileti = "senaryo dosyası okunamadı: " + ec.message();
        return a3::DosyaDurumu::gecersiz;
    }
    if (st.type() != std::filesystem::file_type::regular) {
        // Simgesel bağ (Windows'ta kavşak ve yeniden çözümleme noktaları dahil), dizin ya da özel dosya:
        // senaryo dizininin dışına çıkabilecek hiçbir şey açılmaz.
        ileti = "senaryo dosyası düzgün bir dosya değil (simgesel bağ, dizin ya da özel dosya açılmaz; A3-K78)";
        return a3::DosyaDurumu::gecersiz;
    }
    const auto boy = std::filesystem::file_size(p, ec);
    if (ec) {
        ileti = "senaryo dosyası okunamadı: " + ec.message();
        return a3::DosyaDurumu::gecersiz;
    }
    if (boy > senaryo::kAzamiDosyaBoyu) {
        ileti = "senaryo dosyası 4 MiB'den büyük (A5-K01)";
        return a3::DosyaDurumu::gecersiz;
    }
    std::ifstream f(p, std::ios::binary);
    if (!f) {
        ileti = "senaryo dosyası açılamadı";
        return a3::DosyaDurumu::gecersiz;
    }
    icerik.assign((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    if (f.bad() || icerik.size() > senaryo::kAzamiDosyaBoyu) {
        ileti = "senaryo dosyası okunamadı";
        return a3::DosyaDurumu::gecersiz;
    }
    return a3::DosyaDurumu::tamam;
}

a3::DosyaDurumu SenaryoHizmeti::oku(std::string_view ad, std::string& icerik, std::string& ileti)
{
    return dosya_oku_(std::string(ad), icerik, ileti);
}

a3::DosyaDurumu SenaryoHizmeti::girdiyi_coz_(const a3::SenaryoGirdisi& g, std::string& ileti, std::string& yol,
                                             std::string& ad, std::shared_ptr<const senaryo::DosyaKaynagi>& kaynak,
                                             senaryo::CozumSonucu& cs)
{
    std::string icerik;
    if (g.adli) {
        const a3::DosyaDurumu d = dosya_oku_(g.ad, icerik, ileti);
        if (d != a3::DosyaDurumu::tamam) {
            return d;
        }
        ad = g.ad;
        yol = ayar_.dizin + "/" + g.ad;
    } else {
        if (g.icerik.size() > senaryo::kAzamiDosyaBoyu) {
            ileti = "senaryo metni 4 MiB'den büyük (A5-K01)";
            return a3::DosyaDurumu::gecersiz;
        }
        icerik = g.icerik;
        ad.clear();
        yol = ayar_.dizin + "/yuklenen.toml";
    }
    kaynak = std::make_shared<KatmanliKaynak>(std::filesystem::path(ayar_.depo_koku.empty() ? "." : ayar_.depo_koku),
                                              std::map<std::string, std::string>{{yol, std::move(icerik)}});
    cs = senaryo::senaryo_coz(*kaynak, yol);
    return a3::DosyaDurumu::tamam;
}

a3::SenaryoDogrulama SenaryoHizmeti::dogrula(const a3::SenaryoGirdisi& g)
{
    a3::SenaryoDogrulama d;
    std::string yol, ad;
    std::shared_ptr<const senaryo::DosyaKaynagi> kaynak;
    senaryo::CozumSonucu cs;
    d.dosya = girdiyi_coz_(g, d.ileti, yol, ad, kaynak, cs);
    if (d.dosya != a3::DosyaDurumu::tamam) {
        return d;
    }
    d.rapor_json = cs.rapor.json();
    d.gecerli = cs.rapor.gecerli && cs.cozumlenmis.has_value() && cs.rapor.davranis_ozeti.has_value();
    if (d.gecerli) {
        d.ozet = "sha256:" + *cs.rapor.davranis_ozeti;
    }
    return d;
}

a3::SenaryoAdayi SenaryoHizmeti::hazirla(const a3::SenaryoGirdisi& g)
{
    const Saat::time_point t0 = Saat::now();
    a3::SenaryoAdayi a;
    std::string yol, ad;
    std::shared_ptr<const senaryo::DosyaKaynagi> kaynak;
    senaryo::CozumSonucu cs;
    const a3::DosyaDurumu d = girdiyi_coz_(g, a.ileti, yol, ad, kaynak, cs);
    if (d == a3::DosyaDurumu::yok) {
        a.sonuc = a3::SenaryoAdayi::Sonuc::dosya_yok;
        return a;
    }
    if (d == a3::DosyaDurumu::gecersiz) {
        a.sonuc = a3::SenaryoAdayi::Sonuc::dosya_gecersiz;
        return a;
    }
    a.rapor_json = cs.rapor.json();
    if (!cs.rapor.gecerli || !cs.cozumlenmis || !cs.rapor.davranis_ozeti) {
        a.sonuc = a3::SenaryoAdayi::Sonuc::gecersiz;   // A3-K77: durum değişmez
        return a;
    }
    a.ozet = "sha256:" + *cs.rapor.davranis_ozeti;
    // Yürütülebilirlik: A5'in geçerli saydığı ama bu sim'in yürütemediği öğe, durumu bozmadan reddedilir
    // (dünya yıkıldıktan sonra yeni dünya kurulamazsa kalıcı zarar vermesin).
    Plan plan;
    std::string hata;
    if (!plan_kur(*cs.cozumlenmis, cs.rapor, plan, hata, ayar_.sil_hiz)) {
        a.sonuc = a3::SenaryoAdayi::Sonuc::yurutulemez;
        a.ileti = "senaryo bu sim'de yürütülemez: " + hata;
        return a;
    }
    auto goruntu = std::make_shared<SenaryoGoruntusu>();
    goruntu->ad = ad;
    goruntu->yol = yol;
    goruntu->ozet = a.ozet;
    goruntu->kaynak = kaynak;
    {
        std::lock_guard<std::mutex> kl(m_);
        goruntu->id = sonraki_aday_++;
        a.aday_id = goruntu->id;
        adaylar_[goruntu->id] = goruntu;
        while (adaylar_.size() > kAzamiAday) {
            adaylar_.erase(adaylar_.begin());
        }
        son_hazirlama_ns_ = std::chrono::duration_cast<std::chrono::nanoseconds>(Saat::now() - t0).count();
    }
    a.sonuc = a3::SenaryoAdayi::Sonuc::hazir;
    return a;
}

void SenaryoHizmeti::aday_birak(std::uint64_t aday_id)
{
    std::lock_guard<std::mutex> kl(m_);
    adaylar_.erase(aday_id);
    sonuclar_.erase(aday_id);
}

a3::YuklemeSonucu SenaryoHizmeti::yukleme_bekle(std::uint64_t aday_id, std::chrono::milliseconds sure)
{
    std::unique_lock<std::mutex> kl(m_);
    if (!cv_.wait_for(kl, sure, [&] { return sonuclar_.count(aday_id) != 0u || kapaniyor_; })) {
        return a3::YuklemeSonucu{};   // zaman_asimi
    }
    if (sonuclar_.count(aday_id) == 0u) {
        a3::YuklemeSonucu k;
        k.tur = a3::YuklemeSonucu::Tur::reddedildi;
        k.hata_kodu = "mesgul";
        k.ileti = "sunucu kapanıyor";
        return k;
    }
    a3::YuklemeSonucu s = sonuclar_[aday_id];
    sonuclar_.erase(aday_id);
    return s;
}

std::shared_ptr<const SenaryoGoruntusu> SenaryoHizmeti::aday_al(std::uint64_t aday_id) const
{
    std::lock_guard<std::mutex> kl(m_);
    const auto it = adaylar_.find(aday_id);
    return it == adaylar_.end() ? nullptr : it->second;
}

void SenaryoHizmeti::yukleme_tamam(std::uint64_t aday_id)
{
    std::lock_guard<std::mutex> kl(m_);
    a3::YuklemeSonucu s;
    s.tur = a3::YuklemeSonucu::Tur::uygulandi;
    sonuclar_[aday_id] = s;
    adaylar_.erase(aday_id);
    while (sonuclar_.size() > kAzamiSonuc) {
        sonuclar_.erase(sonuclar_.begin());
    }
    cv_.notify_all();
}

void SenaryoHizmeti::yukleme_reddedildi(std::uint64_t aday_id, std::string hata_kodu, std::string ileti)
{
    std::lock_guard<std::mutex> kl(m_);
    a3::YuklemeSonucu s;
    s.tur = a3::YuklemeSonucu::Tur::reddedildi;
    s.hata_kodu = std::move(hata_kodu);
    s.ileti = std::move(ileti);
    sonuclar_[aday_id] = std::move(s);
    adaylar_.erase(aday_id);
    while (sonuclar_.size() > kAzamiSonuc) {
        sonuclar_.erase(sonuclar_.begin());
    }
    cv_.notify_all();
}

void SenaryoHizmeti::kapaniyor()
{
    std::lock_guard<std::mutex> kl(m_);
    kapaniyor_ = true;
    cv_.notify_all();
}

void SenaryoHizmeti::yuklu_ayarla(std::string kanonik, std::string ozet)
{
    std::lock_guard<std::mutex> kl(m_);
    yuklu_kanonik_ = std::move(kanonik);
    yuklu_ozet_ = std::move(ozet);
}

bool SenaryoHizmeti::cozumlenmis(std::string& bayt, std::string& ozet)
{
    std::lock_guard<std::mutex> kl(m_);
    if (yuklu_kanonik_.empty()) {
        return false;
    }
    bayt = yuklu_kanonik_;
    ozet = yuklu_ozet_;
    return true;
}

void SenaryoHizmeti::on_denetim_ekle(const std::string& kimlik, const std::string& json)
{
    std::lock_guard<std::mutex> kl(m_);
    raporlar_.emplace_back(kimlik, json);
    while (raporlar_.size() > kAzamiRapor) {
        raporlar_.pop_front();
    }
}

bool SenaryoHizmeti::on_denetim(std::string_view kimlik, std::string& rapor_json)
{
    std::lock_guard<std::mutex> kl(m_);
    if (raporlar_.empty()) {
        return false;
    }
    if (kimlik.empty()) {
        rapor_json = raporlar_.back().second;
        return true;
    }
    for (auto it = raporlar_.rbegin(); it != raporlar_.rend(); ++it) {
        if (it->first == kimlik) {
            rapor_json = it->second;
            return true;
        }
    }
    return false;
}

std::int64_t SenaryoHizmeti::son_hazirlama_ns() const
{
    std::lock_guard<std::mutex> kl(m_);
    return son_hazirlama_ns_;
}

}  // namespace ida::sunucu
