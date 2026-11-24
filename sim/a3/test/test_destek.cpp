// test_destek.cpp — bkz. test_destek.hpp.
#include "test_destek.hpp"

#include "sema.hpp"

#include <atomic>
#include <cstdlib>
#include <thread>

namespace ida::a3::test {

std::string HttpYanit::kod() const
{
    json::Deger d;
    if (!json::coz(govde, d)) {
        return {};
    }
    const json::Deger* h = d.bul("hata");
    const json::Deger* k = h != nullptr ? h->bul("kod") : nullptr;
    return k != nullptr ? k->metin : std::string();
}

bool semaya_uyar(std::string_view tanim, const std::string& govde, std::string* neden)
{
    static const json::Deger katalog = [] {
        const auto kb = katalog_baytlari();
        json::CozumSiniri s;
        s.azami_deger = 2'000'000;
        json::Deger k;
        json::coz(std::string_view(reinterpret_cast<const char*>(kb.data()), kb.size()), k, nullptr, s);
        return k;
    }();
    static const Sema sema(katalog);
    json::Deger v;
    if (!json::coz(govde, v)) {
        if (neden != nullptr) {
            *neden = "JSON çözülemedi";
        }
        return false;
    }
    const json::Deger* t = sema.tanim(tanim);
    return t != nullptr && sema.dogrula(*t, v, neden);
}

bool http_yanit_ayir(std::string& tampon, HttpYanit& y, bool head)
{
    const std::size_t son = tampon.find("\r\n\r\n");
    if (son == std::string::npos) {
        return false;
    }
    y = HttpYanit{};
    std::string_view bas(tampon.data(), son);
    const std::size_t s1 = bas.find(' ');
    if (s1 == std::string_view::npos) {
        return false;
    }
    y.durum = std::atoi(std::string(bas.substr(s1 + 1, 3)).c_str());
    std::size_t k = bas.find("\r\n");
    while (k != std::string_view::npos) {
        const std::size_t sonraki = bas.find("\r\n", k + 2);
        const std::string_view satir = bas.substr(k + 2, sonraki == std::string_view::npos ? std::string_view::npos : sonraki - k - 2);
        const std::size_t iki = satir.find(':');
        if (iki != std::string_view::npos) {
            std::string ad(satir.substr(0, iki));
            for (auto& c : ad) {
                c = static_cast<char>((c >= 'A' && c <= 'Z') ? c - 'A' + 'a' : c);
            }
            std::string_view v = satir.substr(iki + 1);
            while (!v.empty() && v.front() == ' ') {
                v.remove_prefix(1);
            }
            y.basliklar.emplace_back(std::move(ad), std::string(v));
        }
        k = sonraki;
    }
    std::size_t uz = 0;
    if (const std::string* cl = y.baslik("content-length")) {
        uz = static_cast<std::size_t>(std::stoull(*cl));
    }
    if (y.durum == 101 || head) {
        uz = 0;
    }
    if (tampon.size() < son + 4 + uz) {
        return false;
    }
    y.govde = tampon.substr(son + 4, uz);
    tampon.erase(0, son + 4 + uz);
    return true;
}

std::string yetki() { return "Authorization: Bearer " + std::string(kJeton); }
std::string host() { return "Host: 127.0.0.1:" + std::to_string(kPort); }

std::string istek(std::string_view yontem, std::string_view hedef, const std::vector<std::string>& basliklar,
                  std::string_view govde, bool varsayilan_basliklar)
{
    std::string s;
    s += yontem;
    s += ' ';
    s += hedef;
    s += " HTTP/1.1\r\n";
    if (varsayilan_basliklar) {
        s += host() + "\r\n";
        s += yetki() + "\r\n";
    }
    for (const auto& b : basliklar) {
        s += b + "\r\n";
    }
    if (!govde.empty() || yontem == "POST" || yontem == "PUT") {
        s += "Content-Length: " + std::to_string(govde.size()) + "\r\n";
    }
    s += "\r\n";
    s += govde;
    return s;
}

std::string ws_istemci_cercevesi(std::uint8_t opkod, std::string_view yuk, bool fin, bool maske)
{
    std::string c;
    c.push_back(static_cast<char>((fin ? 0x80u : 0u) | opkod));
    const std::uint8_t m = maske ? 0x80u : 0u;
    if (yuk.size() < 126) {
        c.push_back(static_cast<char>(m | yuk.size()));
    } else if (yuk.size() <= 0xFFFF) {
        c.push_back(static_cast<char>(m | 126u));
        c.push_back(static_cast<char>((yuk.size() >> 8u) & 0xFFu));
        c.push_back(static_cast<char>(yuk.size() & 0xFFu));
    } else {
        c.push_back(static_cast<char>(m | 127u));
        for (int i = 7; i >= 0; --i) {
            c.push_back(static_cast<char>((static_cast<std::uint64_t>(yuk.size()) >> (8u * static_cast<unsigned>(i))) & 0xFFu));
        }
    }
    const std::uint8_t anahtar[4] = {0x12, 0x34, 0x56, 0x78};
    if (maske) {
        c.append(reinterpret_cast<const char*>(anahtar), 4);
    }
    for (std::size_t i = 0; i < yuk.size(); ++i) {
        c.push_back(maske ? static_cast<char>(static_cast<unsigned char>(yuk[i]) ^ anahtar[i & 3u]) : yuk[i]);
    }
    return c;
}

bool ws_sunucu_cercevesi(std::string& t, WsCerceve& c)
{
    if (t.size() < 2) {
        return false;
    }
    const auto b0 = static_cast<unsigned char>(t[0]);
    const auto b1 = static_cast<unsigned char>(t[1]);
    std::uint64_t uz = b1 & 0x7Fu;
    std::size_t k = 2;
    if (uz == 126) {
        if (t.size() < 4) {
            return false;
        }
        uz = (static_cast<std::uint64_t>(static_cast<unsigned char>(t[2])) << 8u) | static_cast<unsigned char>(t[3]);
        k = 4;
    } else if (uz == 127) {
        if (t.size() < 10) {
            return false;
        }
        uz = 0;
        for (std::size_t i = 0; i < 8; ++i) {
            uz = (uz << 8u) | static_cast<unsigned char>(t[2 + i]);
        }
        k = 10;
    }
    if (t.size() < k + uz) {
        return false;
    }
    c.fin = (b0 & 0x80u) != 0;
    c.opkod = static_cast<std::uint8_t>(b0 & 0x0Fu);
    c.yuk = t.substr(k, static_cast<std::size_t>(uz));
    t.erase(0, k + static_cast<std::size_t>(uz));
    return true;
}

bool a3_ileti_ayir(std::string_view ileti, std::vector<A3Kare>& kareler)
{
    std::size_t p = 0;
    while (p < ileti.size()) {
        if (ileti.size() - p < kBaslikBayt) {
            return false;
        }
        A3Kare k;
        baslik_oku(reinterpret_cast<const std::uint8_t*>(ileti.data()) + p, kBaslikBayt, k.baslik);
        p += kBaslikBayt;
        if (ileti.size() - p < k.baslik.yuk_uzunlugu) {
            return false;
        }
        const auto* y = reinterpret_cast<const std::uint8_t*>(ileti.data()) + p;
        k.yuk.assign(y, y + k.baslik.yuk_uzunlugu);
        p += k.baslik.yuk_uzunlugu;
        kareler.push_back(std::move(k));
    }
    return true;
}

std::string ws_el_sikisma(std::string_view jeton, bool v1, std::string_view origin)
{
    std::vector<std::string> b = {"Upgrade: websocket", "Connection: Upgrade", "Sec-WebSocket-Version: 13",
                                  "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ=="};
    std::string p = v1 ? "ida-a3.v1" : "baska";
    if (!jeton.empty()) {
        p += ", ida-erisim." + std::string(jeton);
    }
    b.push_back("Sec-WebSocket-Protocol: " + p);
    if (!origin.empty()) {
        b.push_back("Origin: " + std::string(origin));
    }
    std::string s = "GET /ws HTTP/1.1\r\n" + host() + "\r\n";
    for (const auto& x : b) {
        s += x + "\r\n";
    }
    return s + "\r\n";
}

// ---- Ortam ----

namespace {

SunucuAyari ortam_ayari(SunucuAyari sa)
{
    if (sa.telemetri_halkasi == (std::size_t{8} << 20u)) {
        sa.telemetri_halkasi = std::size_t{1} << 20u;
        sa.olay_halkasi = std::size_t{1} << 20u;
    }
    return sa;
}

sahte::SahteAyar elle(sahte::SahteAyar a)
{
    a.is_parcacigi = false;
    return a;
}

}  // namespace

Ortam::Ortam(sahte::SahteAyar ayar, SunucuAyari sa)
    : sunucu_ayari(ortam_ayari(sa)),
      yayinci(sunucu_ayari.telemetri_halkasi, sunucu_ayari.olay_halkasi),
      dunya(yayinci, elle(ayar)),
      cekirdek(sunucu_ayari, dunya, yayinci.ic(), std::string(kJeton), kPort),
      simdi(std::chrono::steady_clock::now())
{
    std::string h;
    if (!cekirdek.hazirla(h)) {
        std::printf("  Ortam: hazırlanamadı: %s\n", h.c_str());
    }
}

Ortam::~Ortam() = default;

BagId Ortam::ac()
{
    std::vector<BagId> k;
    return cekirdek.ac(simdi, k);
}

void Ortam::gonder(BagId id, std::string_view veri)
{
    cekirdek.gelen(id, reinterpret_cast<const std::uint8_t*>(veri.data()), veri.size(), simdi);
}

std::string Ortam::al(BagId id)
{
    std::string s(cekirdek.cikis(id));
    cekirdek.yazildi(id, s.size());
    return s;
}

void Ortam::dongu() { cekirdek.dongu(simdi); }

void Ortam::ilerle(std::chrono::milliseconds ms)
{
    const auto adim = std::chrono::milliseconds(5);
    for (auto g = std::chrono::milliseconds(0); g < ms; g += adim) {
        simdi += adim;
        cekirdek.dongu(simdi);
    }
}

HttpYanit Ortam::http(BagId id, std::string_view ham)
{
    gonder(id, ham);
    dongu();
    std::string t = al(id);
    HttpYanit y;
    http_yanit_ayir(t, y);
    return y;
}

HttpYanit Ortam::http(std::string_view ham)
{
    const BagId id = ac();
    HttpYanit y = http(id, ham);
    cekirdek.kapandi(id);
    return y;
}

std::uint32_t Ortam::istemci_al()
{
    const HttpYanit y = http(istek("POST", "/api/v1/istemciler", {"Content-Type: application/json"}, R"({"ad":"test"})"));
    const json::Deger d = y.json_();
    const json::Deger* id = d.bul("istemci_id");
    return id != nullptr ? static_cast<std::uint32_t>(id->sayi) : 0;
}

BagId Ortam::ws_ac(std::string* merhaba)
{
    const BagId id = ac();
    gonder(id, ws_el_sikisma());
    std::string t = al(id);
    HttpYanit y;
    if (!http_yanit_ayir(t, y) || y.durum != 101) {
        return 0;
    }
    ws_tampon[id] = t;
    WsCerceve c;
    if (ws_sunucu_cercevesi(ws_tampon[id], c) && merhaba != nullptr) {
        *merhaba = c.yuk;
    }
    return id;
}

std::vector<WsCerceve> Ortam::ws_al(BagId id)
{
    ws_tampon[id] += al(id);
    std::vector<WsCerceve> r;
    WsCerceve c;
    while (ws_sunucu_cercevesi(ws_tampon[id], c)) {
        r.push_back(c);
    }
    return r;
}

// ---- SoketIstemci ----

SoketIstemci::SoketIstemci(std::uint16_t port) : t_(ag::baglan("127.0.0.1", port)) {}

SoketIstemci::~SoketIstemci() { kapat(); }

SoketIstemci::SoketIstemci(SoketIstemci&& o) noexcept : tampon(std::move(o.tampon)), kapandi(o.kapandi), t_(o.t_)
{
    o.t_ = ag::gecersiz();
}

SoketIstemci& SoketIstemci::operator=(SoketIstemci&& o) noexcept
{
    if (this != &o) {
        kapat();
        t_ = o.t_;
        tampon = std::move(o.tampon);
        kapandi = o.kapandi;
        o.t_ = ag::gecersiz();
    }
    return *this;
}

void SoketIstemci::kapat()
{
    ag::kapat(t_);
    t_ = ag::gecersiz();
}

bool SoketIstemci::yaz(std::string_view s, int zaman_asimi_ms)
{
    const auto son = std::chrono::steady_clock::now() + std::chrono::milliseconds(zaman_asimi_ms);
    std::size_t k = 0;
    while (k < s.size()) {
        const long long n = ag::yaz(t_, reinterpret_cast<const std::uint8_t*>(s.data()) + k, s.size() - k);
        if (n > 0) {
            k += static_cast<std::size_t>(n);
            continue;
        }
        if (n == -2 || std::chrono::steady_clock::now() > son) {
            return false;
        }
        std::vector<ag::Yoklama> l(1);
        l[0].t = t_;
        l[0].oku_iste = false;
        l[0].yaz_iste = true;
        ag::yokla(l, 10);
    }
    return true;
}

bool SoketIstemci::oku(int zaman_asimi_ms)
{
    std::vector<ag::Yoklama> l(1);
    l[0].t = t_;
    ag::yokla(l, zaman_asimi_ms);
    static thread_local std::vector<std::uint8_t> b(65536);
    for (;;) {
        const long long n = ag::oku(t_, b.data(), b.size());
        if (n > 0) {
            tampon.append(reinterpret_cast<const char*>(b.data()), static_cast<std::size_t>(n));
            continue;
        }
        if (n == 0 || n == -2) {
            kapandi = true;
            return false;
        }
        return true;
    }
}

bool SoketIstemci::yanit_bekle(HttpYanit& y, int zaman_asimi_ms, bool head)
{
    const auto son = std::chrono::steady_clock::now() + std::chrono::milliseconds(zaman_asimi_ms);
    for (;;) {
        if (http_yanit_ayir(tampon, y, head)) {
            return true;
        }
        if (kapandi || std::chrono::steady_clock::now() > son) {
            return false;
        }
        oku(20);
    }
}

bool SoketIstemci::ws_bekle(WsCerceve& c, int zaman_asimi_ms)
{
    const auto son = std::chrono::steady_clock::now() + std::chrono::milliseconds(zaman_asimi_ms);
    for (;;) {
        if (ws_sunucu_cercevesi(tampon, c)) {
            return true;
        }
        if (kapandi || std::chrono::steady_clock::now() > son) {
            return false;
        }
        oku(20);
    }
}

// ---- CanliSunucu ----

struct CanliSunucu::Vekil final : KomutAlicisi {
    std::atomic<sahte::SahteDunya*> d{nullptr};
    AnlikDurum anlik() override
    {
        auto* p = d.load();
        return p != nullptr ? p->anlik() : AnlikDurum{};
    }
    AlimSonucu al(const Komut& k) override
    {
        auto* p = d.load();
        if (p == nullptr) {
            AlimSonucu a;
            a.tur = AlimSonucu::Tur::dolu;
            return a;
        }
        return p->al(k);
    }
};

CanliSunucu::CanliSunucu(sahte::SahteAyar ayar, SunucuAyari sa) : vekil(std::make_unique<Vekil>())
{
    sa.adres = "127.0.0.1";
    sa.port = 0;  // işletim sistemi atar
    auto kilit = gunluk_kilidi;
    auto* g = &gunluk;
    sa.gunluk = [kilit, g](std::string_view s) {
        std::lock_guard<std::mutex> k(*kilit);
        g->emplace_back(s);
    };
    sunucu = Sunucu::baslat(sa, *vekil, hata);
    if (!sunucu) {
        std::printf("  CanliSunucu: %s\n", hata.c_str());
        return;
    }
    dunya = std::make_unique<sahte::SahteDunya>(sunucu->yayinci(), ayar);
    vekil->d.store(dunya.get());
    dunya->baslat();
}

CanliSunucu::~CanliSunucu()
{
    if (sunucu) {
        sunucu->durdur();
    }
    vekil->d.store(nullptr);
    if (dunya) {
        dunya->durdur();
    }
}

std::string CanliSunucu::host() const { return "Host: 127.0.0.1:" + std::to_string(sunucu->port()); }

std::string CanliSunucu::yetki() const { return "Authorization: Bearer " + sunucu->jeton(); }

}  // namespace ida::a3::test
