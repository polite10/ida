// test_dayaniklilik.cpp — dayanıklılık (W4-a3 kabul 6): bozuk HTTP, aşırı uzun başlık/gövde, yarım
// ve maskesiz WebSocket çerçeveleri, parçalı ve 1 MiB üstü iletiler, çok sayıda bağlantı. Sabit
// tohumlu rastgele girdi; çökme, sınırsız bellek ya da kilitlenme olmamalı.
#include "sema.hpp"
#include "test_destek.hpp"

#include <random>
#include <thread>

namespace ida::a3::test {

namespace {

std::string rastgele_bayt(std::mt19937_64& g, std::size_t n)
{
    std::string s(n, '\0');
    for (auto& c : s) {
        c = static_cast<char>(g());
    }
    return s;
}

// Geçerli bir girdiyi bozar: bayt çevirme, kesme, ekleme, yineleme, uzun kuyruk.
std::string boz(std::mt19937_64& g, std::string s)
{
    const int islem = static_cast<int>(g() % 8);
    switch (islem) {
    case 0:  // birkaç bayt çevir
        for (int i = 0, n = 1 + static_cast<int>(g() % 4); i < n && !s.empty(); ++i) {
            s[g() % s.size()] = static_cast<char>(g());
        }
        break;
    case 1:  // kes
        s.resize(s.empty() ? 0 : g() % s.size());
        break;
    case 2: {  // rastgele ekle
        const std::size_t k = s.empty() ? 0 : g() % s.size();
        s.insert(k, rastgele_bayt(g, 1 + g() % 64));
        break;
    }
    case 3: {  // bir parçayı yinele
        if (!s.empty()) {
            const std::size_t a = g() % s.size();
            const std::size_t n = 1 + g() % std::min<std::size_t>(s.size() - a, 200);
            s.insert(a, s.substr(a, n));
        }
        break;
    }
    case 4:  // uzun başlık
        s.insert(s.find("\r\n") == std::string::npos ? 0 : s.find("\r\n") + 2, "X-Uzun: " + std::string(g() % 40000, 'u') + "\r\n");
        break;
    case 5:  // rastgele kuyruk
        s += rastgele_bayt(g, g() % 300);
        break;
    case 6:  // CRLF'leri LF yap
        for (std::size_t k = s.find("\r\n"); k != std::string::npos; k = s.find("\r\n", k)) {
            s.erase(k, 1);
        }
        break;
    default:  // olduğu gibi
        break;
    }
    return s;
}

// Rastgele bir WebSocket istemci çerçevesi (geçerli ya da bozuk).
std::string ws_rastgele(std::mt19937_64& g)
{
    const int tur = static_cast<int>(g() % 10);
    switch (tur) {
    case 0: return ws_istemci_cercevesi(1, R"({"tur":"abone_ol","istek_no":1,"kanallar":[{"kanal":"fizik/durum","hz":10}]})");
    case 1: return ws_istemci_cercevesi(1, rastgele_bayt(g, g() % 100));  // geçersiz UTF-8 olası
    case 2: return ws_istemci_cercevesi(static_cast<std::uint8_t>(g() % 16), rastgele_bayt(g, g() % 200), (g() & 1u) != 0);
    case 3: return ws_istemci_cercevesi(1, "x", true, false);  // maskesiz
    case 4: {  // 1 MiB üstü bildirilen uzunluk, yük yok
        std::string c = ws_istemci_cercevesi(1, std::string(70000, 'a'));
        return c.substr(0, 14);
    }
    case 5: return ws_istemci_cercevesi(1, "{\"tur\":", false) + ws_istemci_cercevesi(0, "\"abone_ol\"}");
    case 6: return ws_istemci_cercevesi(9, rastgele_bayt(g, g() % 125));
    case 7: return ws_istemci_cercevesi(1, std::string(70000, 'b'));  // 64 KiB üstü
    case 8: return ws_istemci_cercevesi(1, "{\"tur\":\"abonelik_birak\",\"istek_no\":2,\"kanallar\":[\"olay\"]}").substr(0, 5);  // yarım
    default: return rastgele_bayt(g, g() % 64);
    }
}

}  // namespace

int test_rastgele_cekirdek()
{
    Denetci d;
    Ortam o;
    const std::uint32_t ist = o.istemci_al();
    const std::string ib = "Ida-Istemci: " + std::to_string(ist);
    const std::vector<std::string> taban = {
        istek("GET", "/api/v1/surum"),
        istek("GET", "/api/v1/durum"),
        istek("GET", "/api/v1/olaylar?sonra=0&en_cok=10"),
        istek("GET", "/api/v1/komutlar/1"),
        istek("POST", "/api/v1/istemciler", {"Content-Type: application/json"}, R"({"ad":"x"})"),
        istek("POST", "/api/v1/calistirma/baslat", {"Content-Type: application/json", ib},
              R"({"istek_kimligi":"rastgele-01","beklenen_run_id":null,"beklenen_durum_rev":1})"),
        istek("POST", "/api/v1/emniyet/degistir", {"Content-Type: application/json", ib},
              R"({"istek_kimligi":"rastgele-02","beklenen_run_id":null,"girdi":"acil_durdurma","hedef_durum":"basili"})"),
        istek("POST", "/api/v1/emniyet/onay_jetonu", {"Content-Type: application/json", ib},
              R"({"beklenen_run_id":null,"girdi":"emniyet_pimi","tup":"f1","hedef_durum":"cikarilmis"})"),
        istek("POST", "/api/v1/yer_istasyonu/komut", {"Content-Type: application/json", ib},
              R"({"istek_kimligi":"rastgele-03","beklenen_run_id":null,"komut":"fc_disarm","parametreler":{}})"),
        "POST /api/v1/calistirma/durdur HTTP/1.1\r\n" + host() + "\r\n" + yetki() + "\r\nContent-Type: application/json\r\n" + ib +
            "\r\nTransfer-Encoding: chunked\r\n\r\n4\r\n{\"a\"\r\n3\r\n:1}\r\n0\r\n\r\n",
        istek("PUT", "/api/v1/yayin", {"Content-Type: application/json", ib}, R"({"istek_kimligi":"rastgele-04","yayin_tavani_hz":10})"),
        istek("GET", "/"),
        istek("OPTIONS", "/api/v1/surum"),
        istek("GET", "/api/v1/surum", {"Origin: http://evil"}),
        ws_el_sikisma(),
    };
    std::mt19937_64 g(0x5EED'A3u);
    const int kVaka = 100000;
    std::size_t en_buyuk_cikis = 0;
    std::size_t ws_vaka = 0;
    std::size_t yanit = 0;
    const auto bas = std::chrono::steady_clock::now();
    for (int v = 0; v < kVaka; ++v) {
        const BagId id = o.ac();
        if (id == 0) {
            DOGRULA(d, false, "bağlantı açılamadı");
            break;
        }
        std::string girdi = boz(g, taban[g() % taban.size()]);
        if (g() % 4 == 0) {
            girdi += boz(g, taban[g() % taban.size()]);  // ardışık (pipelining)
        }
        const bool ws_istegi = girdi.rfind("GET /ws", 0) == 0;
        // Rastgele parça boylarıyla besle
        std::size_t k = 0;
        while (k < girdi.size()) {
            const std::size_t n = 1 + g() % std::max<std::size_t>(1, girdi.size() / 3 + 1);
            o.gonder(id, std::string_view(girdi).substr(k, n));
            k += n;
            if (g() % 3 == 0) {
                o.dongu();
            }
        }
        o.dongu();
        std::string c = o.al(id);
        if (ws_istegi && c.rfind("HTTP/1.1 101", 0) == 0) {
            ++ws_vaka;
            for (int i = 0, n = 1 + static_cast<int>(g() % 6); i < n; ++i) {
                o.gonder(id, ws_rastgele(g));
                o.dongu();
            }
            c += o.al(id);
        }
        en_buyuk_cikis = std::max(en_buyuk_cikis, c.size());
        yanit += c.rfind("HTTP/1.1 ", 0) == 0 ? 1u : 0u;
        o.cekirdek.kapandi(id);
        if (v % 500 == 499) {
            o.dunya.adim();
            o.ilerle(std::chrono::milliseconds(20));
        }
    }
    const double sure = std::chrono::duration<double>(std::chrono::steady_clock::now() - bas).count();
    std::printf("ÖLÇÜM: %d çekirdek vakası %.1f s; WebSocket'e yükselen %zu; HTTP yanıtı %zu; en büyük çıkış %zu B\n", kVaka, sure,
                ws_vaka, yanit, en_buyuk_cikis);
    DOGRULA(d, o.cekirdek.baglantilar().empty(), "bütün bağlantılar kapandı");
    DOGRULA(d, en_buyuk_cikis < (std::size_t{8} << 20u), "çıkış sınırlı");
    DOGRULA(d, ws_vaka > 1000 && yanit > 10000, "vakaların önemli bölümü ayrıştırıcıların içine ulaştı");
    // Sunucu hâlâ doğru yanıt verir
    const HttpYanit y = o.http(istek("GET", "/api/v1/surum"));
    DOGRULA(d, y.durum == 200, "vakalardan sonra sunucu yanıt veriyor");
    return d.hata;
}

int test_rastgele_ayristirici()
{
    Denetci d;
    std::mt19937_64 g(0xA3'0F'F0u);
    const int kVaka = 100000;
    std::size_t tamam = 0;
    for (int v = 0; v < kVaka; ++v) {
        const std::string s = rastgele_bayt(g, g() % 600);
        http::Istek r;
        std::size_t blok = 0;
        std::string n;
        if (http::baslik_ayristir("GET / HTTP/1.1\r\n" + s + "\r\n\r\n", r, blok, n) == http::Sonuc::tamam) {
            ++tamam;
            http::GovdeBilgisi gb;
            http::govde_bilgisi(r, gb, n);
        }
        ws::Cerceve c;
        std::uint16_t kod = 0;
        std::size_t t = 0;
        ws::oku(reinterpret_cast<const std::uint8_t*>(s.data()), s.size(), t, c, kod, 65536);
        json::Deger j;
        json::coz(s, j);
        json::coz("{\"a\":[" + s + "]}", j);
        http::ParcaliCozucu p(1000);
        std::string govde;
        p.besle(s, govde);
        DOGRULA(d, govde.size() <= 1000, "chunked sınırı");
    }
    // JSON şeması: rastgele değerler bütün istek şemalarına karşı
    const auto kb = katalog_baytlari();
    json::Deger katalog;
    json::CozumSiniri sn;
    sn.azami_deger = 2'000'000;
    json::coz(std::string_view(reinterpret_cast<const char*>(kb.data()), kb.size()), katalog, nullptr, sn);
    Sema sema(katalog);
    const char* parcalar[] = {"{", "}", "[", "]", ",", ":", "\"istek_kimligi\"", "\"abcdefgh12\"", "null", "true", "1", "1.5", "-3",
                              "\"girdi\"", "\"acil_durdurma\"", "\"tup\"", "\"f1\"", "\"beklenen_run_id\"", "\"hedef_durum\"",
                              "\"basili\"", "1e400", "\"\\ud800\""};
    std::size_t gecerli_json = 0;
    for (int v = 0; v < 20000; ++v) {
        std::string s;
        for (int i = 0, n = static_cast<int>(g() % 30); i < n; ++i) {
            s += parcalar[g() % std::size(parcalar)];
        }
        json::Deger j;
        if (json::coz(s, j)) {
            ++gecerli_json;
            for (const char* t : {"istek_emniyet_degistir", "istek_calistirma", "ws_abone_ol", "istek_yer_istasyonu_komut"}) {
                sema.dogrula(*sema.tanim(t), j);
            }
        }
    }
    std::printf("ÖLÇÜM: %d ayrıştırıcı vakası (başlığı geçen %zu); şema: geçerli JSON %zu\n", kVaka, tamam, gecerli_json);
    return d.hata;
}

int test_cok_baglanti()
{
    Denetci d;
    CanliSunucu s;
    if (!s.sunucu) {
        return 1;
    }
    const std::uint16_t p = s.sunucu->port();
    // 1) 200 bağlantı yarım başlık bırakır: sunucu sınırlı sayıda tutar, kilitlenmez
    std::vector<SoketIstemci> yarim;
    for (int i = 0; i < 200; ++i) {
        yarim.emplace_back(p);
        yarim.back().yaz("GET /api/v1/surum HTTP/1.1\r\nHo");
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    const SunucuSayaclari say = s.sunucu->sayaclar();
    DOGRULA(d, say.http_baglanti <= 64, "HTTP bağlantı sınırı: " + std::to_string(say.http_baglanti));
    yarim.clear();
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    {
        SoketIstemci c(p);
        c.yaz("GET /api/v1/surum HTTP/1.1\r\n" + s.host() + "\r\n" + s.yetki() + "\r\n\r\n");
        HttpYanit y;
        DOGRULA(d, c.yanit_bekle(y) && y.durum == 200, "yarım bağlantılar kapandıktan sonra hizmet sürer");
    }
    // 2) 64 boştaki keep-alive bağlantısı varken yeni istek: en eskisi kapatılıp yer açılır
    std::vector<SoketIstemci> bosta;
    for (int i = 0; i < 64; ++i) {
        bosta.emplace_back(p);
        bosta.back().yaz("GET /api/v1/surum HTTP/1.1\r\n" + s.host() + "\r\n" + s.yetki() + "\r\n\r\n");
        HttpYanit y;
        bosta.back().yanit_bekle(y);
    }
    {
        SoketIstemci c(p);
        c.yaz("GET /api/v1/surum HTTP/1.1\r\n" + s.host() + "\r\n" + s.yetki() + "\r\n\r\n");
        HttpYanit y;
        DOGRULA(d, c.yanit_bekle(y) && y.durum == 200, "bağlantı sınırında yeni istek hizmet alır");
    }
    bosta.clear();
    // 3) WebSocket sınırı: 64'ten fazlası 503
    std::vector<SoketIstemci> wsler;
    int acik = 0;
    int mesgul = 0;
    for (int i = 0; i < 70; ++i) {
        wsler.emplace_back(p);
        wsler.back().yaz("GET /ws HTTP/1.1\r\n" + s.host() +
                      "\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Version: 13\r\n"
                      "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\nSec-WebSocket-Protocol: ida-a3.v1, ida-erisim." +
                      s.sunucu->jeton() + "\r\n\r\n");
        HttpYanit y;
        if (wsler.back().yanit_bekle(y)) {
            acik += y.durum == 101 ? 1 : 0;
            mesgul += y.durum == 503 ? 1 : 0;
        }
    }
    DOGRULA(d, acik == 64 && mesgul == 6, "WebSocket sınırı 64: " + std::to_string(acik) + "/" + std::to_string(mesgul));
    // 4) 1 MiB üstü WebSocket iletisi: 1009, sunucu yükü okumaz
    {
        SoketIstemci& c = wsler.front();
        c.tampon.clear();
        c.yaz(ws_istemci_cercevesi(1, std::string((std::size_t{2} << 20u), 'a')).substr(0, 14));
        bool kod1009 = false;
        WsCerceve f;
        while (c.ws_bekle(f, 2000)) {
            if (f.opkod == 8 && f.yuk.size() >= 2 && static_cast<unsigned char>(f.yuk[0]) == 0x03 &&
                static_cast<unsigned char>(f.yuk[1]) == 0xF1) {
                kod1009 = true;
                break;
            }
        }
        DOGRULA(d, kod1009, "2 MiB bildirilen ileti 1009");
    }
    wsler.clear();
    // 5) Büyük HTTP gövdesi: 413
    {
        SoketIstemci c(p);
        c.yaz("POST /api/v1/istemciler HTTP/1.1\r\n" + s.host() + "\r\n" + s.yetki() +
              "\r\nContent-Type: application/json\r\nContent-Length: 5000000\r\n\r\n");
        HttpYanit y;
        DOGRULA(d, c.yanit_bekle(y) && y.durum == 413, "büyük gövde 413");
    }
    return d.hata;
}

}  // namespace ida::a3::test
