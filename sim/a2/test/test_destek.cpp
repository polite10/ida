// test_destek.cpp — bkz. test_destek.hpp.
#include "test_destek.hpp"

#include "saat.hpp"

#include <chrono>
#include <cstring>
#include <filesystem>
#include <random>
#include <thread>

namespace ida::a2::test {

std::array<std::uint8_t, 32> ornek_ozet(std::uint8_t tohum)
{
    std::array<std::uint8_t, 32> o{};
    for (std::size_t i = 0; i < o.size(); ++i) {
        o[i] = static_cast<std::uint8_t>(tohum + 7u * i);
    }
    return o;
}

sahte::SahteAyar ornek_ayar()
{
    sahte::SahteAyar a;
    a.manifest_ozeti = ornek_ozet();
    return a;
}

// ---------------------------------------------------------------------------------------------

void Kaydedici::besle(const std::uint8_t* p, std::size_t n)
{
    bayt += n;
    for (std::size_t i = 0; i < n && !cerceve_hatasi; i += viz::kEklemeParcasi) {
        const std::size_t m = n - i < viz::kEklemeParcasi ? n - i : viz::kEklemeParcasi;
        if (!okuyucu.ekle(p + i, m)) {
            cerceve_hatasi = true;
            return;
        }
        Cerceve c;
        viz::KodHata h = viz::KodHata::yok;
        while (okuyucu.sonraki(c.b, c.yuk, h)) {
            ++sayi;
            if (gozlemci) {
                gozlemci(c);
            }
            if (sakla) {
                cerceveler.push_back(c);
            }
        }
        if (h != viz::KodHata::yok) {
            cerceve_hatasi = true;
        }
    }
}

std::size_t Kaydedici::say(IletiTuru t) const
{
    std::size_t n = 0;
    for (const Cerceve& c : cerceveler) {
        if (c.tur() == t) {
            ++n;
        }
    }
    return n;
}

const Cerceve* Kaydedici::son(IletiTuru t) const
{
    for (std::size_t i = cerceveler.size(); i > 0; --i) {
        if (cerceveler[i - 1].tur() == t) {
            return &cerceveler[i - 1];
        }
    }
    return nullptr;
}

const Cerceve* Kaydedici::ilk(IletiTuru t) const
{
    for (const Cerceve& c : cerceveler) {
        if (c.tur() == t) {
            return &c;
        }
    }
    return nullptr;
}

bool Kaydedici::son_veda(VedaNedeni n) const
{
    if (cerceveler.empty() || cerceveler.back().tur() != IletiTuru::veda) {
        return false;
    }
    Veda v{};
    return viz::coz_veda(cerceveler.back().yuk, v) == viz::KodHata::yok && v.neden == static_cast<std::uint32_t>(n);
}

bool Kaydedici::son_ret(RetNedeni n) const
{
    if (cerceveler.empty() || cerceveler.back().tur() != IletiTuru::ret) {
        return false;
    }
    Ret r{};
    return viz::coz_ret(cerceveler.back().yuk, r) == viz::KodHata::yok && r.neden == static_cast<std::uint32_t>(n);
}

viz::DurumIleti durum_coz(const Cerceve& c)
{
    viz::DurumIleti d;
    if (viz::coz_durum(c.yuk, kAdimNs, d) != viz::KodHata::yok) {
        d.d.adim_no = ~std::uint64_t{0};  // çözülemedi işareti
    }
    return d;
}

SahneOlayi olay_coz(const Cerceve& c)
{
    SahneOlayi o{};
    if (viz::coz_sahne_olayi(c.yuk, kAdimNs, o) != viz::KodHata::yok) {
        o.tur = 0xFFFF;
    }
    return o;
}

viz::HosgeldinIleti hosgeldin_coz(const Cerceve& c)
{
    viz::HosgeldinIleti h;
    if (viz::coz_hosgeldin(c.yuk, h) != viz::KodHata::yok) {
        h.h.oturum_kimligi = 0;
    }
    return h;
}

// ---------------------------------------------------------------------------------------------

namespace {

constexpr std::size_t kSoketTamponu = 64u * 1024u;  // yavaş istemci benzetiminde "yolda" bayt tavanı

viz::IstemciOturumu::Ayarlar istemci_ayari()
{
    viz::IstemciOturumu::Ayarlar a;
    a.godot_surumu = "4.7.2";
    a.uzanti_surumu = "sinama";
    a.gpu_adi = "yok";
    a.surec_kimligi = 4242;
    return a;
}

}  // namespace

Duzenek::Duzenek(std::size_t kuyruk_siniri, std::size_t halka_bayt)
    : y(halka_bayt, true), c(y, kuyruk_siniri), ist(istemci_ayari())
{
    const std::array<std::uint8_t, 32> o = ornek_ozet();
    std::memcpy(hazir_ozeti.data(), o.data(), 32);
}

bool Duzenek::baglan()
{
    if (!c.baglanti_acildi(simdi, ++kimlik_sayaci)) {
        return false;
    }
    yolda.clear();
    sunucu_kapatti = false;
    ham_bagli = false;
    ist.baglandi(simdi, true);
    istemci_bagli = true;
    pompala();
    pompala();
    return true;
}

bool Duzenek::ham_baglan()
{
    if (!c.baglanti_acildi(simdi, ++kimlik_sayaci)) {
        return false;
    }
    yolda.clear();
    sunucu_kapatti = false;
    ham_bagli = true;
    return true;
}

void Duzenek::ham_gonder(const viz::Baytlar& b)
{
    c.veri_geldi(b.data(), b.size(), simdi);
    sunucudan_al_();
    yolda.clear();
}

void Duzenek::sunucudan_al_()
{
    ic::GondermeKuyrugu& q = c.kuyruk();
    while (!q.bos()) {
        const std::span<const std::uint8_t> on = q.on();
        std::size_t n = on.size();
        if (istemci_okuma_siniri > 0) {
            if (yolda.size() >= kSoketTamponu) {
                break;  // "soket tamponu" dolu: sunucu yazamaz
            }
            const std::size_t bos = kSoketTamponu - yolda.size();
            n = n < bos ? n : bos;
        }
        k.besle(on.data(), n);
        yolda.insert(yolda.end(), on.begin(), on.begin() + static_cast<std::ptrdiff_t>(n));
        q.tuket(n);
    }
    if (c.kapanis_var()) {
        const ic::Baytlar kalan = c.kapanis_al();
        k.besle(kalan.data(), kalan.size());
        yolda.insert(yolda.end(), kalan.begin(), kalan.end());
        sunucu_kapatti = true;
        ham_bagli = false;
    }
}

void Duzenek::istemciye_ver_()
{
    if (!istemci_bagli) {
        yolda.clear();
        return;
    }
    std::size_t n = yolda.size();
    if (istemci_okuma_siniri > 0 && n > istemci_okuma_siniri) {
        n = istemci_okuma_siniri;
    }
    if (n > 0) {
        ist.veri_geldi(yolda.data(), n, simdi);
        yolda.erase(yolda.begin(), yolda.begin() + static_cast<std::ptrdiff_t>(n));
    }
    if (sunucu_kapatti && yolda.empty()) {
        ist.baglanti_koptu("sunucu kapattı");
        ist.kapatildi();
        istemci_bagli = false;
    }
}

void Duzenek::pompala()
{
    for (int tur = 0; tur < 2; ++tur) {
        if (istemci_bagli) {
            const viz::Baytlar g = ist.giden_al();
            if (!g.empty() && c.oturum_var()) {
                c.veri_geldi(g.data(), g.size(), simdi);
            }
            if (ist.kapatilmali()) {  // istemci kapatmaya karar verdi (veda gönderdi ya da aldı)
                ist.kapatildi();
                istemci_bagli = false;
                c.baglanti_koptu();
            }
        }
        if (tur == 0) {
            c.tik(simdi);
        }
        sunucudan_al_();
        istemciye_ver_();
        for (viz::Olay& o : ist.olaylari_al()) {
            if (o.tur == viz::OlayTuru::sahne_kur && otomatik_hazir) {
                ist.sahne_sonucu(hazir_durumu, hazir_ozeti);
            }
            olaylar.push_back(std::move(o));
        }
    }
}

void Duzenek::ilerlet(std::int64_t ns, std::int64_t dilim_ns, bool istemci_zamani)
{
    std::int64_t kalan = ns;
    while (kalan > 0) {
        const std::int64_t d = kalan < dilim_ns ? kalan : dilim_ns;
        simdi += d;
        kalan -= d;
        if (istemci_bagli && istemci_zamani) {
            ist.zaman(simdi, 60'000, true);
        }
        pompala();
    }
}

void Duzenek::istemciyi_kopar()
{
    if (istemci_bagli) {
        ist.baglanti_koptu("sınama: istemci öldürüldü");
        ist.kapatildi();
        istemci_bagli = false;
    }
    c.baglanti_koptu();
    yolda.clear();
}

bool Duzenek::istemci_hatasiz() const
{
    for (const viz::Olay& o : olaylar) {
        if (o.tur == viz::OlayTuru::kapandi && o.metin.find("protokol hatası") != std::string::npos) {
            return false;
        }
    }
    return true;
}

// ---------------------------------------------------------------------------------------------

SoketIstemci::SoketIstemci(std::uint16_t port) : t(ag::baglan(port)), ist(istemci_ayari())
{
    const std::array<std::uint8_t, 32> o = ornek_ozet();
    std::memcpy(hazir_ozeti.data(), o.data(), 32);
    saat = [] { return ic::duvar_ns(); };
}

SoketIstemci::~SoketIstemci() { kapat(); }

void SoketIstemci::kapat()
{
    if (t != ag::gecersiz()) {
        ag::kapat(t);
        t = ag::gecersiz();
    }
}

void SoketIstemci::basla()
{
    ist.baglandi(saat(), true);
    bagli = true;
}

void SoketIstemci::ham_yaz(const std::uint8_t* p, std::size_t n)
{
    std::size_t yazilan = 0;
    const auto son = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (t != ag::gecersiz() && yazilan < n && std::chrono::steady_clock::now() < son) {
        const long long r = ag::yaz(t, p + yazilan, n - yazilan);
        if (r > 0) {
            yazilan += static_cast<std::size_t>(r);
        } else if (r == -2) {
            karsi_kapatti = true;
            return;
        } else {
            gercek_bekle_ms(1);
        }
    }
}

bool SoketIstemci::pompala()
{
    bool is = false;
    if (t == ag::gecersiz()) {
        return false;
    }
    if (oku) {
        static thread_local std::vector<std::uint8_t> tampon(65536);
        for (int i = 0; i < 64; ++i) {
            const long long r = ag::oku(t, tampon.data(), tampon.size());
            if (r > 0) {
                k.besle(tampon.data(), static_cast<std::size_t>(r));
                if (bagli) {
                    ist.veri_geldi(tampon.data(), static_cast<std::size_t>(r), saat());
                }
                is = true;
            } else {
                if (r == 0 || r == -2) {
                    karsi_kapatti = true;
                }
                break;
            }
        }
    }
    if (bagli) {
        ist.zaman(saat(), 60'000, true);
        for (const viz::Olay& o : ist.olaylari_al()) {
            if (o.tur == viz::OlayTuru::sahne_kur && otomatik_hazir) {
                ist.sahne_sonucu(HazirDurumu::basarili, hazir_ozeti);
            }
        }
        const viz::Baytlar g = ist.giden_al();
        if (!g.empty()) {
            ham_yaz(g.data(), g.size());
            is = true;
        }
        if (ist.kapatilmali()) {
            ist.kapatildi();
            bagli = false;
        }
    }
    return is;
}

bool SoketIstemci::bekle(const std::function<bool()>& kosul, int ms)
{
    const auto son = std::chrono::steady_clock::now() + std::chrono::milliseconds(ms);
    for (;;) {
        const bool is = pompala();
        if (kosul()) {
            return true;
        }
        if (std::chrono::steady_clock::now() >= son) {
            return false;
        }
        if (!is) {
            gercek_bekle_ms(1);
        }
    }
}

// ---------------------------------------------------------------------------------------------

std::string gecici_dizin(const char* ad)
{
    static std::atomic<unsigned> sayac{0};
    std::random_device rd;
    const std::filesystem::path p = std::filesystem::temp_directory_path() /
                                    ("ida-a2-test-" + std::string(ad) + "-" + std::to_string(rd()) + "-" +
                                     std::to_string(sayac.fetch_add(1)));
    std::error_code ec;
    std::filesystem::create_directories(p, ec);
    const std::u8string u = p.u8string();
    return std::string(u.begin(), u.end());
}

void gercek_bekle_ms(int ms) { std::this_thread::sleep_for(std::chrono::milliseconds(ms)); }

bool kosul_bekle(const std::function<bool()>& kosul, int ms)
{
    const auto son = std::chrono::steady_clock::now() + std::chrono::milliseconds(ms);
    while (!kosul()) {
        if (std::chrono::steady_clock::now() >= son) {
            return false;
        }
        gercek_bekle_ms(1);
    }
    return true;
}

SunucuDuzenegi::SunucuDuzenegi(std::function<void(SunucuAyari&)> ayarla)
{
    dizin = gecici_dizin("sunucu");
    kilit = dizin + "/a2.kilit";
    SunucuAyari a;
    a.sinama_bos_port = true;
    a.kilit_dosyasi = kilit;
    a.saat = [this] { return saat(); };
    if (ayarla) {
        ayarla(a);
    }
    s = Sunucu::baslat(a, hata);
}

SunucuDuzenegi::~SunucuDuzenegi()
{
    s.reset();
    std::error_code ec;
    std::filesystem::remove_all(dizin, ec);
}

std::int64_t SunucuDuzenegi::saat() const { return ic::duvar_ns() + saat_kaydirma.load(std::memory_order_relaxed); }

}  // namespace ida::a2::test
