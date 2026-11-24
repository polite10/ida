// test_erisim.cpp — EMNİYET sınıfı erişim denetimi testleri (A3-K02, K04…K12; W4-a3 kabul 1).
// Gerçek soketler: sunucu 127.0.0.1'de, port 0 (işletim sistemi atar).
#include "test_destek.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <thread>

namespace ida::a3::test {

namespace {

HttpYanit gonder(std::uint16_t port, const std::string& ham)
{
    SoketIstemci c(port);
    HttpYanit y;
    if (!c.gecerli() || !c.yaz(ham) || !c.yanit_bekle(y)) {
        y.durum = -1;
    }
    return y;
}

std::string get(const CanliSunucu& s, std::string_view yol, const std::vector<std::string>& ek, bool host_ekle = true)
{
    std::string r = "GET " + std::string(yol) + " HTTP/1.1\r\n";
    if (host_ekle) {
        r += s.host() + "\r\n";
    }
    for (const auto& b : ek) {
        r += b + "\r\n";
    }
    return r + "\r\n";
}

bool cors_yok(const HttpYanit& y)
{
    return std::none_of(y.basliklar.begin(), y.basliklar.end(),
                        [](const auto& b) { return b.first.rfind("access-control-", 0) == 0; });
}

}  // namespace

int test_dongu_disi_adres()
{
    Denetci d;
    sahte::SahteAyar sa;
    sa.is_parcacigi = false;
    for (const char* adres : {"0.0.0.0", "192.168.1.10", "localhost", "127.0.0.2", "::", ""}) {
        Yayinci y(1u << 16u, 1u << 16u);
        sahte::SahteDunya dunya(y, sa);
        SunucuAyari a;
        a.adres = adres;
        a.port = 0;
        std::string hata;
        auto s = Sunucu::baslat(a, dunya, hata);
        DOGRULA(d, s == nullptr && hata.find("A3-K02") != std::string::npos, std::string("dinlenmemeli: ") + adres);
    }
    return d.hata;
}

int test_erisim_http()
{
    Denetci d;
    CanliSunucu s;
    if (!s.sunucu) {
        return 1;
    }
    const std::uint16_t p = s.sunucu->port();
    const std::string jeton = s.sunucu->jeton();
    DOGRULA(d, jeton.size() == 64 && jeton.find_first_not_of("0123456789abcdef") == std::string::npos, "jeton 64 hex (A3-K04)");
    DOGRULA(d, s.sunucu->acilis_adresi() == "http://127.0.0.1:" + std::to_string(p) + "/#erisim=" + jeton, "açılış adresi (A3-K05)");
    // A3-K06: jeton
    HttpYanit y = gonder(p, get(s, "/api/v1/surum", {}));
    DOGRULA(d, y.durum == 403 && y.kod() == "erisim_reddedildi", "jetonsuz 403");
    y = gonder(p, get(s, "/api/v1/surum", {"Authorization: Bearer " + std::string(64, 'a')}));
    DOGRULA(d, y.durum == 403 && y.kod() == "erisim_reddedildi", "yanlış jeton 403");
    y = gonder(p, get(s, "/api/v1/surum", {"Authorization: Bearer " + jeton.substr(0, 63)}));
    DOGRULA(d, y.durum == 403, "kısa jeton 403");
    y = gonder(p, get(s, "/api/v1/surum", {"Authorization: Basic " + jeton}));
    DOGRULA(d, y.durum == 403, "Basic şeması 403");
    y = gonder(p, get(s, "/api/v1/surum", {s.yetki(), s.yetki()}));
    DOGRULA(d, y.durum == 403, "iki Authorization 403");
    y = gonder(p, get(s, "/api/v1/surum", {s.yetki()}));
    DOGRULA(d, y.durum == 200 && cors_yok(y), "jetonlu, Origin'siz araç 200 (A3-K09)");
    DOGRULA(d, y.govde.find(jeton) == std::string::npos, "yanıtta jeton yok");
    // A3-K08: Host
    const std::string ps = std::to_string(p);
    y = gonder(p, get(s, "/api/v1/surum", {"Host: evil.example:" + ps, s.yetki()}, false));
    DOGRULA(d, y.durum == 403, "yanlış Host 403");
    y = gonder(p, get(s, "/api/v1/surum", {"Host: 127.0.0.1:1", s.yetki()}, false));
    DOGRULA(d, y.durum == 403, "yanlış port Host 403");
    y = gonder(p, get(s, "/api/v1/surum", {s.yetki()}, false));
    DOGRULA(d, y.durum == 403, "Host'suz 403");
    y = gonder(p, get(s, "/api/v1/surum", {"Host: localhost:" + ps, s.yetki()}, false));
    DOGRULA(d, y.durum == 200, "localhost Host 200");
    y = gonder(p, get(s, "/api/v1/surum", {"Host: [::1]:" + ps, s.yetki()}, false));
    DOGRULA(d, y.durum == 200, "[::1] Host 200");
    y = gonder(p, get(s, "/", {"Host: evil.example:" + ps}, false));
    DOGRULA(d, y.durum == 403, "statik yolda da Host denetimi (A3-K11)");
    // A3-K09: Origin
    y = gonder(p, get(s, "/api/v1/surum", {s.yetki(), "Origin: http://evil.example"}));
    DOGRULA(d, y.durum == 403, "yanlış Origin 403");
    y = gonder(p, get(s, "/api/v1/surum", {s.yetki(), "Origin: null"}));
    DOGRULA(d, y.durum == 403, "null Origin 403");
    y = gonder(p, get(s, "/api/v1/surum", {s.yetki(), "Origin: http://127.0.0.1:" + ps}));
    DOGRULA(d, y.durum == 200, "izinli Origin 200");
    y = gonder(p, get(s, "/api/v1/surum", {s.yetki(), "Origin: https://127.0.0.1:" + ps}));
    DOGRULA(d, y.durum == 403, "https Origin 403");
    // A3-K10, K11
    y = gonder(p, "OPTIONS / HTTP/1.1\r\n" + s.host() + "\r\nOrigin: http://evil.example\r\nAccess-Control-Request-Method: POST\r\n\r\n");
    DOGRULA(d, y.durum == 403 && cors_yok(y), "yabancı ön uçuş 403, CORS başlığı yok");
    y = gonder(p, "OPTIONS / HTTP/1.1\r\n" + s.host() + "\r\n\r\n");
    DOGRULA(d, y.durum == 405 && cors_yok(y), "OPTIONS 405");
    y = gonder(p, "OPTIONS /api/v1/surum HTTP/1.1\r\n" + s.host() + "\r\n" + s.yetki() + "\r\n\r\n");
    DOGRULA(d, y.durum == 405, "OPTIONS /api 405");
    y = gonder(p, get(s, "/baska", {}));
    DOGRULA(d, y.durum == 404 && y.kod() == "bulunamadi", "bilinmeyen yol 404");
    y = gonder(p, get(s, "/api/v1/ates", {s.yetki()}));
    DOGRULA(d, y.durum == 404, "listede olmayan uç 404 (A3-K99)");
    y = gonder(p, "POST / HTTP/1.1\r\n" + s.host() + "\r\nContent-Length: 0\r\n\r\n");
    DOGRULA(d, y.durum == 405, "statik yolda POST 405");
    y = gonder(p, "POST /panel/durum.html HTTP/1.1\r\n" + s.host() + "\r\nContent-Length: 0\r\n\r\n");
    DOGRULA(d, y.durum == 405, "panel yolunda POST 405");
    y = gonder(p, "HEAD / HTTP/1.1\r\n" + s.host() + "\r\n\r\n");
    DOGRULA(d, y.durum == 405, "statik yolda HEAD 405 (yalnız GET)");
    y = gonder(p, "DELETE /api/v1/surum HTTP/1.1\r\n" + s.host() + "\r\n" + s.yetki() + "\r\n\r\n");
    DOGRULA(d, y.durum == 405 && y.baslik("allow") != nullptr, "yanlış yöntem 405 + Allow");
    // Jeton yol içinde geçse bile olaya yazılmaz (A3-K12)
    gonder(p, get(s, "/api/v1/" + jeton, {}));
    std::this_thread::sleep_for(std::chrono::milliseconds(1200));
    gonder(p, get(s, "/api/v1/" + jeton + "x", {}));
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    y = gonder(p, get(s, "/api/v1/olaylar?sonra=0&en_cok=1000", {s.yetki()}));
    DOGRULA(d, y.durum == 200 && y.govde.find("erisim_reddedildi") != std::string::npos, "erişim reddi olayları var");
    DOGRULA(d, y.govde.find(jeton) == std::string::npos, "olaylarda jeton yok");
    DOGRULA(d, y.govde.find("[gizli]") != std::string::npos, "yoldaki jeton gizlendi");
    {
        std::lock_guard<std::mutex> k(*s.gunluk_kilidi);
        for (const auto& g : s.gunluk) {
            DOGRULA(d, g.find(jeton) == std::string::npos, "günlükte jeton yok");
        }
    }
    DOGRULA(d, s.sunucu->sayaclar().erisim_reddi >= 15, "reddedilenler sayıldı");
    return d.hata;
}

int test_erisim_ws()
{
    Denetci d;
    CanliSunucu s;
    if (!s.sunucu) {
        return 1;
    }
    const std::uint16_t p = s.sunucu->port();
    const std::string jeton = s.sunucu->jeton();
    auto el_sikis = [&](const std::string& protokol, const std::string& ek = {}) {
        std::string r = "GET /ws HTTP/1.1\r\n" + s.host() +
                        "\r\nUpgrade: websocket\r\nConnection: keep-alive, Upgrade\r\nSec-WebSocket-Version: 13\r\n"
                        "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\n";
        if (!protokol.empty()) {
            r += "Sec-WebSocket-Protocol: " + protokol + "\r\n";
        }
        return r + ek + "\r\n";
    };
    HttpYanit y = gonder(p, el_sikis(""));
    DOGRULA(d, y.durum == 403, "alt protokolsüz 403");
    y = gonder(p, el_sikis("ida-erisim." + jeton));
    DOGRULA(d, y.durum == 403, "ida-a3.v1'siz 403");
    y = gonder(p, el_sikis("ida-a3.v1"));
    DOGRULA(d, y.durum == 403, "jetonsuz 403");
    y = gonder(p, el_sikis("ida-a3.v1, ida-erisim." + std::string(64, 'b')));
    DOGRULA(d, y.durum == 403, "yanlış jeton 403");
    y = gonder(p, el_sikis("ida-a3.v1, ida-erisim." + jeton + ", ida-erisim." + jeton));
    DOGRULA(d, y.durum == 403, "iki jeton 403");
    y = gonder(p, el_sikis("ida-a3.v1, ida-erisim." + jeton, "Origin: http://evil.example\r\n"));
    DOGRULA(d, y.durum == 403, "yabancı Origin'li el sıkışma 403 (RFC 6455 §10.2)");
    SoketIstemci c(p);
    DOGRULA(d, c.yaz(el_sikis("ida-a3.v1, ida-erisim." + jeton, "Origin: http://localhost:" + std::to_string(p) + "\r\n")),
            "el sıkışma gönderildi");
    DOGRULA(d, c.yanit_bekle(y) && y.durum == 101, "101");
    DOGRULA(d, y.baslik("sec-websocket-protocol") != nullptr && *y.baslik("sec-websocket-protocol") == "ida-a3.v1",
            "yalnız ida-a3.v1 seçildi (A3-K07)");
    DOGRULA(d, y.baslik("sec-websocket-accept") != nullptr && *y.baslik("sec-websocket-accept") == "s3pPLMBiTxaQ9kYGzzhZRbK+xOo=",
            "Sec-WebSocket-Accept");
    bool jeton_yok = true;
    for (const auto& [k, v] : y.basliklar) {
        if (v.find(jeton) != std::string::npos || k.find(jeton) != std::string::npos) {
            jeton_yok = false;
        }
    }
    DOGRULA(d, jeton_yok && cors_yok(y), "yanıtta jeton ve CORS yok");
    WsCerceve m;
    DOGRULA(d, c.ws_bekle(m) && m.opkod == 1 && m.yuk.find("\"tur\":\"merhaba\"") != std::string::npos,
            "ilk ileti merhaba (A3-K17)");
    DOGRULA(d, m.yuk.find(jeton) == std::string::npos, "merhabada jeton yok");
    y = gonder(p, "POST /ws HTTP/1.1\r\n" + s.host() + "\r\nContent-Length: 0\r\n\r\n");
    DOGRULA(d, y.durum == 405, "/ws POST 405");
    return d.hata;
}

// EMNİYET — A3-K11 (sol #1): statik kök altında simgesel bağ içeren hiçbir yol sunulmaz (kök içi
// bağ, kök dışı bağ, ara dizin bağı); düz dosyalar sunulur.
int test_statik_simgesel_bag()
{
    namespace fs = std::filesystem;
    Denetci d;
    const fs::path taban = fs::current_path() / "a3_statik_bag_deneme";
    std::error_code ec;
    fs::remove_all(taban, ec);
    const fs::path kok = taban / "kok";
    fs::create_directories(kok / "panel" / "gercek_alt");
    fs::create_directories(taban / "dis");
    auto yaz_dosya = [](const fs::path& p, const char* s) { std::ofstream(p, std::ios::binary) << s; };
    yaz_dosya(kok / "index.html", "kok");
    yaz_dosya(kok / "panel" / "a.html", "a");
    yaz_dosya(kok / "panel" / "gercek_alt" / "x.html", "x");
    yaz_dosya(taban / "dis" / "gizli.txt", "GIZLI-SIM-VERISI");
    bool bag_var = true;
    try {
        fs::create_symlink("a.html", kok / "panel" / "ic_bag.html");
        fs::create_symlink(taban / "dis" / "gizli.txt", kok / "panel" / "dis_dosya.txt");
        fs::create_directory_symlink(taban / "dis", kok / "panel" / "dis_dizin");
        fs::create_directory_symlink("gercek_alt", kok / "panel" / "alt");
    } catch (const fs::filesystem_error& h) {
        bag_var = false;  // Windows'ta simgesel bağ ayrıcalık ister
        std::printf("  simgesel bağ oluşturulamadı (%s); yalnız düz dosyalar sınanır\n", h.what());
    }
    SunucuAyari sa;
    sa.statik_kok = kok.string();
    Ortam o({}, sa);
    auto kod = [&](const std::string& yol) { return o.http(istek("GET", yol, {}, {}, true)); };
    HttpYanit y = kod("/");
    DOGRULA(d, y.durum == 200 && y.govde == "kok", "kök dizin dosyası 200");
    DOGRULA(d, kod("/panel/a.html").durum == 200, "düz dosya 200");
    DOGRULA(d, kod("/panel/gercek_alt/x.html").durum == 200, "alt dizindeki düz dosya 200");
    if (bag_var) {
        DOGRULA(d, kod("/panel/ic_bag.html").durum == 404, "kök içini gösteren bağ 404");
        y = kod("/panel/dis_dosya.txt");
        DOGRULA(d, y.durum == 404 && y.govde.find("GIZLI") == std::string::npos, "kök dışı dosya bağı 404");
        y = kod("/panel/dis_dizin/gizli.txt");
        DOGRULA(d, y.durum == 404 && y.govde.find("GIZLI") == std::string::npos, "kök dışı dizin bağı (ara dizin) 404");
        DOGRULA(d, kod("/panel/alt/x.html").durum == 404, "kök içi ara dizin bağı 404");
    }
    fs::remove_all(taban, ec);
    return d.hata;
}

// A3-K12: aynı neden için saniyede en çok bir olay; aradaki retler `adet`'te toplanır.
int test_erisim_olayi_hiz_siniri()
{
    Denetci d;
    Ortam o;
    for (int i = 0; i < 5; ++i) {
        o.http(istek("GET", "/api/v1/surum", {host()}, {}, false));
        o.ilerle(std::chrono::milliseconds(20));
    }
    auto olaylar = [&] {
        const HttpYanit y = o.http(istek("GET", "/api/v1/olaylar?sonra=0"));
        std::vector<std::uint64_t> adetler;
        const json::Deger j = y.json_();
        for (const auto& e : j.bul("olaylar")->dizi) {
            if (e.bul("tur")->metin == "erisim_reddedildi" && e.bul("ayrinti")->bul("neden")->metin == "jeton") {
                adetler.push_back(static_cast<std::uint64_t>(e.bul("ayrinti")->bul("adet")->sayi));
            }
        }
        return adetler;
    };
    auto a = olaylar();
    DOGRULA(d, a.size() == 1 && a[0] == 1, "ilk ret hemen bir olay");
    o.ilerle(std::chrono::milliseconds(1100));
    a = olaylar();
    DOGRULA(d, a.size() == 2 && a[1] == 4, "sonraki dört ret tek olayda adet 4");
    o.http(istek("GET", "/api/v1/surum", {"Host: x:1"}, {}, false));
    a = olaylar();
    DOGRULA(d, a.size() == 2, "başka neden ayrı sayılır");
    return d.hata;
}

}  // namespace ida::a3::test
