// test_birim.cpp — birim testleri: JSON, desen, şema, özetler, HTTP ve WebSocket ayrıştırıcıları,
// halka, katalog tablosu ve kodlayıcılar.
#include "halka.hpp"
#include "ozet.hpp"
#include "sema.hpp"
#include "test_destek.hpp"

#include <atomic>
#include <cstring>
#include <fstream>
#include <random>
#include <sstream>
#include <thread>

namespace ida::a3::test {

namespace {

std::string sayi(double v)
{
    std::string s;
    json::sayi_yaz(v, s);
    return s;
}

std::string dosya_oku(const char* yol)
{
    std::ifstream f(yol, std::ios::binary);
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

std::string hex(const std::uint8_t* p, std::size_t n) { return ozet::onaltilik(p, n); }

}  // namespace

int test_json()
{
    Denetci d;
    json::Deger v;
    DOGRULA(d, json::coz(R"({"a":[1,2.5,-3e2,true,false,null,"x\u00e7\ud83d\ude00"],"b":{}})", v), "geçerli JSON");
    DOGRULA(d, v.bul("a") != nullptr && v.bul("a")->dizi.size() == 7, "dizi öğeleri");
    DOGRULA(d, v.bul("a")->dizi[6].metin == "x\xc3\xa7\xf0\x9f\x98\x80", "\\u kaçışları UTF-8'e");
    DOGRULA(d, v.bul("a")->dizi[0].tamsayi_mi() && !v.bul("a")->dizi[1].tamsayi_mi(), "tamsayı ayrımı");
    for (const char* bozuk : {R"({"a":1,"a":2})", "[1,]", "{\"a\":1,}", "01", "1.", "-", "\"\\ud800\"", "\"\\udc00x\"",
                              "\"a\x01\"", "\"\xff\"", "\"\xc0\x80\"", "[1] x", "", "nul", "{\"a\" 1}", "1e999"}) {
        json::Deger x;
        DOGRULA(d, !json::coz(bozuk, x), std::string("reddedilmeli: ") + bozuk);
    }
    std::string derin(65, '[');
    derin += std::string(65, ']');
    DOGRULA(d, !json::coz(derin, v), "derinlik sınırı");
    std::string cok = "[";
    for (int i = 0; i < 70000; ++i) {
        cok += i == 0 ? "0" : ",0";
    }
    cok += "]";
    DOGRULA(d, !json::coz(cok, v), "değer sayısı sınırı");
    // ECMAScript Number dizilimi (RFC 8785 §3.2.2.3 örnekleri)
    DOGRULA(d, sayi(0.0) == "0" && sayi(-0.0) == "0", "sıfır");
    DOGRULA(d, sayi(1.0) == "1" && sayi(100.0) == "100" && sayi(-1.5) == "-1.5", "tamsayı ve ondalık");
    DOGRULA(d, sayi(1e21) == "1e+21" && sayi(1e20) == "100000000000000000000", "büyük üs: " + sayi(1e21));
    DOGRULA(d, sayi(1e-7) == "1e-7" && sayi(0.000001) == "0.000001", "küçük üs: " + sayi(1e-7));
    DOGRULA(d, sayi(123e-20) == "1.23e-18", "1.23e-18: " + sayi(123e-20));
    DOGRULA(d, sayi(3.4028234663852886e+38) == "3.4028234663852886e+38", "f32 azami");
    DOGRULA(d, sayi(0.1) == "0.1" && sayi(4.35) == "4.35", "en kısa gösterim");
    // Kanonik yazım: anahtarlar UTF-16 sırasıyla (U+1F600 vekil çifti U+E000'dan önce)
    json::coz(R"({"b":1,"a":{"z":[1,{"y":2,"x":3}]},"\ue000":0,"\ud83d\ude00":0})", v);
    std::string k;
    json::kanonik_yaz(v, k);
    DOGRULA(d, k == "{\"a\":{\"z\":[1,{\"x\":3,\"y\":2}]},\"b\":1,\"\xf0\x9f\x98\x80\":0,\"\xee\x80\x80\":0}", "kanonik: " + k);
    // Yazım: \u kaçışı yok (A3-K121), yalnız " ve \ kaçışlı
    std::string m;
    json::metin_yaz("a\"b\\c\xc3\xa7", m);
    DOGRULA(d, m == "\"a\\\"b\\\\c\xc3\xa7\"", "metin yazımı: " + m);
    DOGRULA(d, json::utf8_gecerli("\xe2\x82\xac") && !json::utf8_gecerli("\xed\xa0\x80") && !json::utf8_gecerli("\xe2\x82"),
            "UTF-8 denetimi");
    return d.hata;
}

int test_desen()
{
    Denetci d;
    struct Vaka {
        const char* desen;
        const char* metin;
        bool beklenen;
    };
    const Vaka vakalar[] = {
        {"^[0-9a-f]{32}$", "0123456789abcdef0123456789abcdef", true},
        {"^[0-9a-f]{32}$", "0123456789abcdef0123456789abcde", false},
        {"^[0-9a-f]{32}$", "0123456789ABCDEF0123456789abcdef", false},
        {"^E([1-9]|1[0-9])$", "E1", true},
        {"^E([1-9]|1[0-9])$", "E19", true},
        {"^E([1-9]|1[0-9])$", "E20", false},
        {"^E([1-9]|1[0-9])$", "E0", false},
        {"^[^\\u0000-\\u001f]*$", "düz metin", true},
        {"^[^\\u0000-\\u001f]*$", "satır\nsonu", false},
        {"^[0-9]+\\.[0-9]+$", "1.0", true},
        {"^[0-9]+\\.[0-9]+$", "10", false},
        {"^[0-9]{8}-[0-9]{6}-[0-9a-f]{6}$", "20261123-101500-a1b2c3", true},
        {"^[a-z0-9_-]{1,64}\\.toml$", "ornek-havuz.toml", true},
        {"^[a-z0-9_-]{1,64}\\.toml$", "../x.toml", false},
        {"^[A-Za-z][A-Za-z0-9_]{0,31}$", "a", true},
        {"^sha256:[0-9a-f]{64}$", "sha256:0000000000000000000000000000000000000000000000000000000000000000", true},
        {"ab", "xxabyy", true},  // çapasız: her yerde
    };
    for (const auto& v : vakalar) {
        std::string h;
        auto p = Desen::derle(v.desen, &h);
        DOGRULA(d, p != nullptr, std::string("derlenmeli: ") + v.desen + " " + h);
        if (p) {
            DOGRULA(d, p->ara(v.metin) == v.beklenen, std::string(v.desen) + " ~ " + v.metin);
        }
    }
    for (const char* bozuk : {"[a-", "(a", "a)", "a{2,1}", "*a", "a**", "(?=a)", "\\p{L}", "a+?"}) {
        DOGRULA(d, Desen::derle(bozuk, nullptr) == nullptr, std::string("derlenmemeli: ") + bozuk);
    }
    // Uzun girdi: özyinelemesiz ve doğrusal (1 MiB)
    auto p = Desen::derle("^[^\\u0000-\\u001f]*$", nullptr);
    const std::string uzun(1u << 20u, 'a');
    const auto bas = std::chrono::steady_clock::now();
    DOGRULA(d, p->ara(uzun), "uzun girdi eşleşmeli");
    const auto sure = std::chrono::steady_clock::now() - bas;
    DOGRULA(d, sure < std::chrono::seconds(2), "uzun girdi hızlı olmalı");
    return d.hata;
}

int test_sema()
{
    Denetci d;
    const auto kb = katalog_baytlari();
    json::Deger katalog;
    json::CozumSiniri s;
    s.azami_deger = 2'000'000;
    DOGRULA(d, json::coz(std::string_view(reinterpret_cast<const char*>(kb.data()), kb.size()), katalog, nullptr, s), "katalog");
    Sema sema(katalog);
    std::string h;
    for (const auto& [ad, t] : katalog.bul("$defs")->nesne) {
        DOGRULA(d, sema.desteklenmeyen_bul(t, h), "$defs/" + ad + ": " + h);
    }
    auto dogrula = [&](const char* tanim, const char* metin) {
        json::Deger v;
        if (!json::coz(metin, v)) {
            return false;
        }
        return sema.dogrula(*sema.tanim(tanim), v);
    };
    DOGRULA(d, dogrula("istek_emniyet_degistir",
                       R"({"istek_kimligi":"abcdefgh","beklenen_run_id":null,"girdi":"acil_durdurma","hedef_durum":"basili"})"),
            "acil durdurma tüpsüz geçerli");
    DOGRULA(d, !dogrula("istek_emniyet_degistir",
                        R"({"istek_kimligi":"abcdefgh","beklenen_run_id":null,"girdi":"acil_durdurma","tup":"f1","hedef_durum":"basili"})"),
            "acil durdurma tüplü geçersiz (A3-K89)");
    DOGRULA(d, !dogrula("istek_emniyet_degistir",
                        R"({"istek_kimligi":"abcdefgh","beklenen_run_id":null,"girdi":"emniyet_pimi","hedef_durum":"takili"})"),
            "pim tüpsüz geçersiz (A3-K89)");
    DOGRULA(d, !dogrula("istek_emniyet_degistir",
                        R"({"istek_kimligi":"abc","beklenen_run_id":null,"girdi":"acil_durdurma","hedef_durum":"basili"})"),
            "kısa istek_kimligi geçersiz");
    DOGRULA(d, !dogrula("istek_emniyet_degistir",
                        R"({"istek_kimligi":"abcdefgh","beklenen_run_id":null,"girdi":"acil_durdurma","hedef_durum":"basili","fazla":1})"),
            "ek alan geçersiz");
    DOGRULA(d, dogrula("istek_calistirma", R"({"istek_kimligi":"abcdefgh","beklenen_run_id":"20261123-101500-a1b2c3","beklenen_durum_rev":3})"),
            "çalıştırma");
    DOGRULA(d, !dogrula("istek_calistirma", R"({"istek_kimligi":"abcdefgh","beklenen_run_id":"x","beklenen_durum_rev":3})"),
            "run_id deseni");
    DOGRULA(d, !dogrula("istek_calistirma", R"({"istek_kimligi":"abcdefgh","beklenen_run_id":null,"beklenen_durum_rev":0})"),
            "durum_rev ≥ 1");
    DOGRULA(d, dogrula("istek_calistirma", R"({"istek_kimligi":"abcdefgh","beklenen_run_id":null,"beklenen_durum_rev":1.0})"),
            "1.0 tamsayıdır");
    DOGRULA(d, dogrula("sinirli_nesne", R"({"a":1,"b":"x","c":1.5,"d":null})"), "sınırlı nesne");
    DOGRULA(d, !dogrula("sinirli_nesne", R"({"A":1})"), "sınırlı nesne anahtar deseni");
    // A1 komut kataloğu şemaları
    const auto mb = komut_katalogu_baytlari();
    json::Deger komutlar;
    DOGRULA(d, json::coz(std::string_view(reinterpret_cast<const char*>(mb.data()), mb.size()), komutlar, nullptr, s), "komutlar");
    Sema ks(komutlar);
    const json::Deger* fc_fire = nullptr;
    for (const auto& k : komutlar.bul("komutlar")->dizi) {
        DOGRULA(d, ks.desteklenmeyen_bul(*k.bul("parametreler"), h), k.bul("ad")->metin + ": " + h);
        if (k.bul("ad")->metin == "fc_fire") {
            fc_fire = k.bul("parametreler");
        }
    }
    DOGRULA(d, fc_fire != nullptr, "fc_fire katalogda");
    if (fc_fire != nullptr) {
        json::Deger a;
        json::coz(R"({"tube":1})", a);
        DOGRULA(d, ks.dogrula(*fc_fire, a), "fc_fire tube 1");
        json::coz(R"({"tube":2})", a);
        DOGRULA(d, !ks.dogrula(*fc_fire, a), "fc_fire tube 2 geçersiz");
        json::coz(R"({"tube":0,"hedef":1})", a);
        DOGRULA(d, !ks.dogrula(*fc_fire, a), "fc_fire ek alan geçersiz");
    }
    // Desteklenmeyen anahtar sözcük sessizce yok sayılmaz (kapalı hata)
    json::Deger bilinmeyen;
    json::coz(R"({"type":"object","if":{"required":["a"]}})", bilinmeyen);
    DOGRULA(d, !sema.desteklenmeyen_bul(bilinmeyen, h), "bilinmeyen anahtar sözcük reddedilmeli");
    return d.hata;
}

int test_ozet()
{
    Denetci d;
    auto sha = [](std::string_view s) {
        const auto o = ozet::sha256(s);
        return hex(o.data(), o.size());
    };
    DOGRULA(d, sha("") == "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855", "SHA-256 boş");
    DOGRULA(d, sha("abc") == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad", "SHA-256 abc");
    DOGRULA(d, sha("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq") ==
                   "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1",
            "SHA-256 iki blok");
    DOGRULA(d, sha(std::string(1000000, 'a')) == "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0",
            "SHA-256 milyon a");
    const std::string abc = "abc";
    const auto s1 = ozet::sha1(reinterpret_cast<const std::uint8_t*>(abc.data()), abc.size());
    DOGRULA(d, hex(s1.data(), s1.size()) == "a9993e364706816aba3e25717850c26c9cd0d89d", "SHA-1 abc");
    DOGRULA(d, ozet::ws_kabul_anahtari("dGhlIHNhbXBsZSBub25jZQ==") == "s3pPLMBiTxaQ9kYGzzhZRbK+xOo=",
            "RFC 6455 §1.3 örneği");
    for (std::size_t n = 0; n < 40; ++n) {
        std::string ham;
        for (std::size_t i = 0; i < n; ++i) {
            ham.push_back(static_cast<char>(i * 37 + n));
        }
        std::string c;
        DOGRULA(d, ozet::base64_coz(ozet::base64(reinterpret_cast<const std::uint8_t*>(ham.data()), ham.size()), c) && c == ham,
                "base64 gidiş-dönüş");
    }
    std::string c;
    DOGRULA(d, !ozet::base64_coz("abc", c) && !ozet::base64_coz("ab=c", c) && !ozet::base64_coz("a!==", c), "bozuk base64");
    // A3-K16: gömülü katalog, depodaki dosyanın kendisi (bayt bayt)
    const std::string dosya = dosya_oku(IDA_A3_KATALOG_YOLU);
    const auto kb = katalog_baytlari();
    DOGRULA(d, dosya.size() == kb.size() && std::memcmp(dosya.data(), kb.data(), kb.size()) == 0, "katalog bayt bayt gömülü");
    const std::string komut = dosya_oku(IDA_A3_KOMUTLAR_YOLU);
    const auto mb = komut_katalogu_baytlari();
    DOGRULA(d, komut.size() == mb.size() && std::memcmp(komut.data(), mb.data(), mb.size()) == 0,
            "komutlar.json bayt bayt gömülü (A3-K100)");
    return d.hata;
}

int test_http_ayristirma()
{
    Denetci d;
    http::Istek r;
    std::size_t blok = 0;
    std::string n;
    const std::string iyi = "POST /api/v1/x?y=1 HTTP/1.1\r\nHost: a\r\nContent-Length: 3\r\nX-Bos:\r\n\r\nabc";
    DOGRULA(d, http::baslik_ayristir(iyi, r, blok, n) == http::Sonuc::tamam, "geçerli istek: " + n);
    DOGRULA(d, r.yontem == "POST" && r.yol == "/api/v1/x" && r.sorgu == "y=1" && blok == iyi.size() - 3, "alanlar");
    DOGRULA(d, r.baslik("host") != nullptr && *r.baslik("host") == "a" && r.baslik("x-bos") != nullptr, "başlıklar küçük harf");
    DOGRULA(d, http::baslik_ayristir("GET / HTTP/1.1\r\nHost: a\r\n", r, blok, n) == http::Sonuc::eksik, "eksik");
    for (const char* bozuk : {"GET / HTTP/1.1\r\nHost : a\r\n\r\n", "GET / HTTP/1.1\r\n katlama\r\n\r\n",
                              "GET http://x/ HTTP/1.1\r\n\r\n", "GET / HTTP/2.0\r\n\r\n", "G@T / HTTP/1.1\r\n\r\n",
                              "GET  / HTTP/1.1\r\n\r\n", "GET / HTTP/1.1\r\nA: b\x01\r\n\r\n", "GET /\x7f HTTP/1.1\r\n\r\n",
                              "GET / HTTP/1.1\r\n:a\r\n\r\n", "GET / HTTP/1.1\nHost: a\r\n\r\n"}) {
        DOGRULA(d, http::baslik_ayristir(bozuk, r, blok, n) == http::Sonuc::hata, std::string("bozuk: ") + bozuk);
    }
    const std::string buyuk = "GET / HTTP/1.1\r\nA: " + std::string(20000, 'x');
    DOGRULA(d, http::baslik_ayristir(buyuk, r, blok, n) == http::Sonuc::hata, "başlık bloğu sınırı");
    http::GovdeBilgisi g;
    http::baslik_ayristir("POST / HTTP/1.1\r\nContent-Length: 5\r\nTransfer-Encoding: chunked\r\n\r\n", r, blok, n);
    DOGRULA(d, !http::govde_bilgisi(r, g, n), "CL + TE reddedilir");
    http::baslik_ayristir("POST / HTTP/1.1\r\nContent-Length: 5\r\nContent-Length: 6\r\n\r\n", r, blok, n);
    DOGRULA(d, !http::govde_bilgisi(r, g, n), "çelişkili CL");
    http::baslik_ayristir("POST / HTTP/1.1\r\nContent-Length: 5\r\nContent-Length: 5\r\n\r\n", r, blok, n);
    DOGRULA(d, http::govde_bilgisi(r, g, n) && g.uzunluk == 5, "aynı CL iki kez");
    http::baslik_ayristir("POST / HTTP/1.1\r\nContent-Length: -1\r\n\r\n", r, blok, n);
    DOGRULA(d, !http::govde_bilgisi(r, g, n), "negatif CL");
    http::baslik_ayristir("POST / HTTP/1.1\r\nTransfer-Encoding: gzip\r\n\r\n", r, blok, n);
    DOGRULA(d, !http::govde_bilgisi(r, g, n), "desteklenmeyen TE");
    // chunked
    http::ParcaliCozucu p(100);
    std::string govde;
    const std::string parcali = "4\r\nWiki\r\n5\r\npedia\r\n0\r\n\r\nSONRAKI";
    const std::size_t t = p.besle(parcali, govde);
    DOGRULA(d, p.bitti() && govde == "Wikipedia" && parcali.substr(t) == "SONRAKI", "chunked çözüm");
    // Sol #8, en kısıtlayan okuma: trailer ve chunk uzantısı reddedilir.
    for (const char* bozuk : {"0\r\nGET /x HTTP/1.1\r\n\r\n", "0\r\nX-Trailer: y\r\n\r\n", "4;ext=1\r\nWiki\r\n0\r\n\r\n",
                              "4 \r\nWiki\r\n0\r\n\r\n", " 4\r\nWiki\r\n0\r\n\r\n", "4\r\nWikiXX0\r\n\r\n"}) {
        http::ParcaliCozucu pb(100);
        std::string gb;
        pb.besle(bozuk, gb);
        DOGRULA(d, pb.hata(), std::string("bozuk chunked: ") + bozuk);
    }
    http::ParcaliCozucu q(5);
    std::string g2;
    q.besle("a\r\n0123456789\r\n0\r\n\r\n", g2);
    DOGRULA(d, q.asildi() && g2.size() <= 5, "chunked sınırı");
    http::ParcaliCozucu e(100);
    e.besle("zz\r\n", g2);
    DOGRULA(d, e.hata(), "bozuk chunk boyu");
    // Artımlı besleme: bayt bayt
    http::ParcaliCozucu bb(100);
    std::string g3;
    for (const char c : std::string("3\r\nabc\r\n0\r\n\r\n")) {
        bb.besle(std::string_view(&c, 1), g3);
    }
    DOGRULA(d, bb.bitti() && g3 == "abc", "bayt bayt chunked");
    return d.hata;
}

int test_ws_cerceve()
{
    Denetci d;
    auto oku = [](const std::string& s, ws::Cerceve& c, std::uint16_t& kod, std::size_t pay = 65536) {
        std::size_t t = 0;
        return ws::oku(reinterpret_cast<const std::uint8_t*>(s.data()), s.size(), t, c, kod, pay);
    };
    ws::Cerceve c;
    std::uint16_t kod = 0;
    DOGRULA(d, oku(ws_istemci_cercevesi(1, "merhaba"), c, kod) == ws::Okuma::cerceve && c.yuk == "merhaba" && c.opkod == 1,
            "maskeli metin");
    DOGRULA(d, oku(ws_istemci_cercevesi(1, "merhaba", true, false), c, kod) == ws::Okuma::hata && kod == 1002,
            "maskesiz istemci çerçevesi 1002");
    std::string rsv = ws_istemci_cercevesi(1, "x");
    rsv[0] = static_cast<char>(rsv[0] | 0x40);
    DOGRULA(d, oku(rsv, c, kod) == ws::Okuma::hata && kod == 1002, "RSV 1002");
    DOGRULA(d, oku(ws_istemci_cercevesi(9, std::string(126, 'p')), c, kod) == ws::Okuma::hata && kod == 1002,
            "uzun denetim çerçevesi 1002");
    DOGRULA(d, oku(ws_istemci_cercevesi(9, "p", false), c, kod) == ws::Okuma::hata && kod == 1002, "parçalı ping 1002");
    DOGRULA(d, oku(ws_istemci_cercevesi(3, "x"), c, kod) == ws::Okuma::hata && kod == 1002, "tanımsız opkod");
    const std::string buyuk = ws_istemci_cercevesi(1, std::string(70000, 'a'));
    DOGRULA(d, oku(buyuk.substr(0, 14), c, kod) == ws::Okuma::hata && kod == 1009, "büyük ileti başlıktan 1009");
    DOGRULA(d, oku(buyuk.substr(0, 100), c, kod, 100000) == ws::Okuma::eksik, "yarım çerçeve eksik");
    std::string uz64 = ws_istemci_cercevesi(1, std::string(70000, 'a'));
    uz64[2] = '\x80';  // 64 bit uzunluğun en anlamlı biti
    DOGRULA(d, oku(uz64, c, kod, ~std::size_t{0}) == ws::Okuma::hata && kod == 1002, "64 bit uzunluk MSB");
    std::string s;
    ws::yaz(ws::Opkod::ikili, nullptr, 0, s);
    DOGRULA(d, s.size() == 2 && static_cast<unsigned char>(s[0]) == 0x82u && s[1] == 0, "boş ikili");
    for (const std::size_t n : {std::size_t{125}, std::size_t{126}, std::size_t{65535}, std::size_t{65536}}) {
        std::string y(n, 'z');
        std::string w;
        ws::yaz(ws::Opkod::metin, reinterpret_cast<const std::uint8_t*>(y.data()), y.size(), w);
        WsCerceve wc;
        DOGRULA(d, ws_sunucu_cercevesi(w, wc) && wc.yuk.size() == n && w.empty(), "uzunluk kodlaması " + std::to_string(n));
    }
    return d.hata;
}

int test_halka()
{
    Denetci d;
    BaytHalkasi h(4096);
    std::size_t n = 0;
    DOGRULA(d, h.oku(n) == nullptr, "boş halka");
    int yazilan = 0;
    while (std::uint8_t* p = h.ayir(100)) {
        std::memset(p, yazilan & 0xFF, 100);
        h.yayinla();
        ++yazilan;
    }
    DOGRULA(d, yazilan == 36, "kapasite: " + std::to_string(yazilan));  // 4096 / 112 = 36,6
    int okunan = 0;
    while (const std::uint8_t* p = h.oku(n)) {
        DOGRULA(d, n == 100 && p[0] == (okunan & 0xFF), "kayıt içeriği");
        h.birak();
        ++okunan;
    }
    DOGRULA(d, okunan == yazilan, "hepsi okundu");
    DOGRULA(d, h.ayir(3000) == nullptr, "yarı kapasiteden büyük kayıt reddedilir");
    // Eşzamanlı üretici–tüketici: sarma dahil, sıra ve içerik korunur
    BaytHalkasi e(1u << 14u);
    std::atomic<bool> bitti{false};
    const int kAdet = 200000;
    std::thread uretici([&] {
        int i = 0;
        while (i < kAdet) {
            const std::size_t boy = 8 + static_cast<std::size_t>(i % 300);
            std::uint8_t* p = e.ayir(boy);
            if (p == nullptr) {
                std::this_thread::yield();
                continue;
            }
            std::memcpy(p, &i, sizeof i);
            std::memset(p + sizeof i, i & 0x7F, boy - sizeof i);
            e.yayinla();
            ++i;
        }
        bitti = true;
    });
    int beklenen = 0;
    bool tutarli = true;
    while (beklenen < kAdet) {
        const std::uint8_t* p = e.oku(n);
        if (p == nullptr) {
            std::this_thread::yield();
            continue;
        }
        int v = 0;
        std::memcpy(&v, p, sizeof v);
        if (v != beklenen || n != 8 + static_cast<std::size_t>(v % 300) || p[n - 1] != (v & 0x7F)) {
            tutarli = false;
        }
        e.birak();
        ++beklenen;
    }
    uretici.join();
    DOGRULA(d, tutarli && bitti, "eşzamanlı halka sırası ve içeriği");
    return d.hata;
}

namespace {

// Katalogdaki bir alan listesini üretilen tabloyla karşılaştırır.
void alanlari_karsilastir(Denetci& d, const json::Deger& liste, const AlanTanimi* alanlar, std::uint16_t n,
                          const std::string& yol)
{
    DOGRULA(d, liste.dizi.size() == static_cast<std::size_t>(n), yol + ": alan sayısı");
    for (std::size_t i = 0; i < liste.dizi.size() && i < n; ++i) {
        const json::Deger& a = liste.dizi[i];
        const AlanTanimi& t = alanlar[i];
        const std::string ay = yol + "." + a.bul("ad")->metin;
        DOGRULA(d, t.ad == a.bul("ad")->metin, ay + ": ad/sıra");
        static constexpr const char* kTip[] = {"u8", "u16", "u32", "u64", "i32", "i64", "f32", "f64", "metin", "bayt_dizisi", "dizi"};
        DOGRULA(d, a.bul("tip")->metin == kTip[static_cast<int>(t.tip)], ay + ": tip");
        const json::Deger* adet = a.bul("adet");
        DOGRULA(d, (adet == nullptr ? 0 : static_cast<int>(adet->sayi)) == t.adet, ay + ": adet");
        const json::Deger* azami = a.bul("azami");
        DOGRULA(d, (azami == nullptr ? 0u : static_cast<std::uint32_t>(azami->sayi)) == t.azami, ay + ": azami");
        if (const json::Deger* sayac = a.bul("sayac")) {
            DOGRULA(d, sayac->metin == kTip[static_cast<int>(t.sayac)], ay + ": sayaç");
        }
        const json::Deger* yd = a.bul("yok_degeri");
        int yok_kodu = 0;
        if (yd != nullptr) {
            const std::string& y = yd->metin;
            yok_kodu = y == "NaN" ? 1 : y == "INT64_MIN" ? 2 : y == "2^64-1" ? 3 : y == "0xFFFFFFFF" ? 4 : y == "255" ? 5
                     : y == "-1" ? 6 : y == "0" ? 7 : -1;
        }
        DOGRULA(d, yok_kodu == t.yok, ay + ": yok değeri");
        const json::Deger* degerler = a.bul("degerler");
        DOGRULA(d, (degerler == nullptr ? std::size_t{0} : degerler->nesne.size()) == static_cast<std::size_t>(t.deger_sayisi), ay + ": değer sayısı");
        if (degerler != nullptr) {
            for (std::uint16_t k = 0; k < t.deger_sayisi; ++k) {
                const json::Deger* ad = degerler->bul(std::to_string(t.degerler[k].kod));
                DOGRULA(d, ad != nullptr && ad->metin == t.degerler[k].ad, ay + ": değer adı");
            }
        }
        if (const json::Deger* oge = a.bul("oge")) {
            alanlari_karsilastir(d, *oge, t.oge, t.oge_sayisi, ay);
        }
    }
}

}  // namespace

// Üretilen kodlayıcı tablosunun katalogla eşleşmesi (W4-a3 kapsam 3).
int test_katalog_tablosu()
{
    Denetci d;
    const std::string metin = dosya_oku(IDA_A3_KATALOG_YOLU);
    json::Deger k;
    json::CozumSiniri s;
    s.azami_deger = 2'000'000;
    DOGRULA(d, json::coz(metin, k, nullptr, s), "katalog çözülür");
    const json::Deger* yk = k.bul("x_ida_yukler");
    DOGRULA(d, yk != nullptr && yk->nesne.size() == yukler().size(), "yük sayısı");
    if (yk != nullptr) {
        std::size_t i = 0;
        for (const auto& [ad, liste] : yk->nesne) {
            const YukTanimi* y = i < yukler().size() ? &yukler()[i] : nullptr;
            DOGRULA(d, y != nullptr && y->ad == ad, "yük sırası: " + ad);
            if (y != nullptr) {
                alanlari_karsilastir(d, liste, y->alanlar, y->alan_sayisi, ad);
            }
            ++i;
        }
    }
    const json::Deger* kn = k.bul("x_ida_kanallar");
    DOGRULA(d, kn != nullptr && kn->dizi.size() == kanallar().size(), "kanal sayısı");
    if (kn != nullptr) {
        for (std::size_t i = 0; i < kn->dizi.size() && i < kanallar().size(); ++i) {
            const json::Deger& j = kn->dizi[i];
            const KanalTanimi& t = kanallar()[i];
            const std::string a = j.bul("ad")->metin;
            DOGRULA(d, t.ad == a && t.indeks == i, a + ": ad/sıra");
            DOGRULA(d, t.kanal_id == static_cast<std::uint16_t>(j.bul("kanal_id")->sayi), a + ": kanal_id");
            DOGRULA(d, t.kanal_surumu == static_cast<std::uint16_t>(j.bul("kanal_surumu")->sayi), a + ": kanal_surumu");
            DOGRULA(d, t.yuk->ad == j.bul("yuk")->metin, a + ": yük");
            static constexpr const char* kSinif[] = {"durum", "dizi", "olay"};
            DOGRULA(d, j.bul("sinif")->metin == kSinif[static_cast<int>(t.sinif)], a + ": sınıf");
            const json::Deger* az = j.bul("azami_hz");
            DOGRULA(d, (az->tur == json::Tur::bos ? 0 : static_cast<int>(az->sayi)) == t.azami_hz, a + ": azami_hz");
            const json::Deger* va = j.bul("varsayilan_hz");
            DOGRULA(d, (va->tur == json::Tur::bos ? 0 : static_cast<int>(va->sayi)) == t.varsayilan_hz, a + ": varsayilan_hz");
            const json::Deger* ku = j.bul("kuyruk_siniri");
            DOGRULA(d, (ku->tur == json::Tur::bos ? 0u : static_cast<std::uint32_t>(ku->sayi)) == t.kuyruk_siniri, a + ": kuyruk");
            DOGRULA(d, j.bul("zorunlu_abonelik")->mantik == t.zorunlu, a + ": zorunlu");
            DOGRULA(d, j.bul("yer_gercegi")->mantik == t.yer_gercegi, a + ": yer_gercegi");
            DOGRULA(d, j.bul("anahtar")->dizi.size() == static_cast<std::size_t>(t.anahtar_sayisi), a + ": anahtar");
            DOGRULA(d, j.bul("zaman_anlami")->metin == t.zaman_anlami, a + ": zaman anlamı");
        }
    }
    const json::Deger* uc = k.bul("x_ida_uclar");
    DOGRULA(d, uc != nullptr && uc->dizi.size() == uclar().size(), "uç sayısı");
    const json::Deger* hk = k.bul("x_ida_hata_kodlari");
    DOGRULA(d, hk != nullptr && hk->dizi.size() == hata_kodlari().size(), "hata kodu sayısı");
    for (const auto& h : hk->dizi) {
        DOGRULA(d, hata_http(h.bul("kod")->metin) == static_cast<std::uint16_t>(h.bul("http")->sayi), "hata kodu " + h.bul("kod")->metin);
    }
    const json::Deger* ot = k.bul("x_ida_olay_turleri");
    DOGRULA(d, ot != nullptr && ot->dizi.size() == olay_turleri().size(), "olay türü sayısı");
    return d.hata;
}

namespace {

// Tabloya göre (üretilen yapılardan bağımsız) çözen ve üretilen json_dok ile aynı biçimde döken çözücü.
bool genel_coz(Okuyucu& r, const AlanTanimi* alanlar, std::uint16_t n, std::string& c)
{
    c.push_back('{');
    for (std::uint16_t i = 0; i < n; ++i) {
        const AlanTanimi& a = alanlar[i];
        if (i > 0) {
            c.push_back(',');
        }
        c += "\"" + std::string(a.ad) + "\":";
        // Katalogdaki yok değeri (tablodaki kodla): ham bit deseni karşılaştırılır.
        auto yok = [&](std::uint64_t ham) {
            switch (a.yok) {
            case 2: return ham == 0x8000000000000000ull;
            case 3: return ham == ~std::uint64_t{0};
            case 4: return ham == 0xFFFFFFFFull;
            case 5: return ham == 255u;
            case 6: return ham == ~std::uint64_t{0};
            case 7: return ham == 0u;
            default: return false;
            }
        };
        auto skaler = [&](std::string& o) {
            auto tam = [&](std::uint64_t ham, const std::string& yazi) { o += yok(ham) ? std::string("null") : yazi; };
            switch (a.tip) {
            case Tip::u8: { const auto v = r.u8(); tam(v, std::to_string(v)); break; }
            case Tip::u16: { const auto v = r.u16(); tam(v, std::to_string(v)); break; }
            case Tip::u32: { const auto v = r.u32(); tam(v, std::to_string(v)); break; }
            case Tip::i32: { const auto v = r.i32(); tam(static_cast<std::uint64_t>(static_cast<std::int64_t>(v)), std::to_string(v)); break; }
            case Tip::u64: { const auto v = r.u64(); tam(v, "\"" + std::to_string(v) + "\""); break; }
            case Tip::i64: { const auto v = r.i64(); tam(static_cast<std::uint64_t>(v), "\"" + std::to_string(v) + "\""); break; }
            case Tip::f32: { const float v = r.f32(); if (a.yok == 1 && std::isnan(v)) { o += "null"; } else { dok_kayan(o, v); } break; }
            case Tip::f64: { const double v = r.f64(); if (a.yok == 1 && std::isnan(v)) { o += "null"; } else { dok_kayan(o, v); } break; }
            default: break;
            }
        };
        switch (a.tip) {
        case Tip::metin: {
            const std::uint64_t uz = a.sayac == Tip::u8 ? r.u8() : a.sayac == Tip::u16 ? r.u16() : r.u32();
            const std::uint8_t* p = r.baytlar(uz);
            if (p == nullptr) {
                return false;
            }
            dok_metin(c, std::string_view(reinterpret_cast<const char*>(p), static_cast<std::size_t>(uz)));
            break;
        }
        case Tip::bayt_dizisi: {
            const std::uint64_t uz = a.sayac == Tip::u8 ? r.u8() : a.sayac == Tip::u16 ? r.u16() : r.u32();
            const std::uint8_t* p = r.baytlar(uz);
            if (p == nullptr) {
                return false;
            }
            c.push_back('[');
            for (std::uint64_t k = 0; k < uz; ++k) {
                if (k > 0) {
                    c.push_back(',');
                }
                c += std::to_string(p[k]);
            }
            c.push_back(']');
            break;
        }
        case Tip::dizi: {
            const std::uint64_t uz = a.sayac == Tip::u8 ? r.u8() : a.sayac == Tip::u16 ? r.u16() : r.u32();
            if (!r.tamam() || uz > a.azami) {
                return false;
            }
            c.push_back('[');
            for (std::uint64_t k = 0; k < uz; ++k) {
                if (k > 0) {
                    c.push_back(',');
                }
                if (!genel_coz(r, a.oge, a.oge_sayisi, c)) {
                    return false;
                }
            }
            c.push_back(']');
            break;
        }
        default:
            if (a.adet > 0) {
                c.push_back('[');
                for (std::uint16_t k = 0; k < a.adet; ++k) {
                    if (k > 0) {
                        c.push_back(',');
                    }
                    skaler(c);
                }
                c.push_back(']');
            } else {
                skaler(c);
            }
        }
    }
    c.push_back('}');
    return r.tamam();
}

// Rastgele ama geçerli (sonlu kayan nokta, ASCII metin, azami içinde) yük baytları.
void rastgele_yuk(std::mt19937_64& g, const AlanTanimi* alanlar, std::uint16_t n, Yazici& w, int derinlik = 0)
{
    for (std::uint16_t i = 0; i < n; ++i) {
        const AlanTanimi& a = alanlar[i];
        const int tekrar = a.adet > 0 ? a.adet : 1;
        auto sayac = [&](std::uint32_t v) {
            if (a.sayac == Tip::u8) {
                w.u8(static_cast<std::uint8_t>(v));
            } else if (a.sayac == Tip::u16) {
                w.u16(static_cast<std::uint16_t>(v));
            } else {
                w.u32(v);
            }
        };
        switch (a.tip) {
        case Tip::metin: {
            const auto uz = static_cast<std::uint32_t>(g() % (std::min<std::uint32_t>(a.azami, 40) + 1));
            sayac(uz);
            for (std::uint32_t k = 0; k < uz; ++k) {
                const std::uint8_t ch = static_cast<std::uint8_t>('a' + g() % 26);
                w.baytlar(&ch, 1);
            }
            break;
        }
        case Tip::bayt_dizisi: {
            const auto uz = static_cast<std::uint32_t>(g() % (std::min<std::uint32_t>(a.azami, 64) + 1));
            sayac(uz);
            for (std::uint32_t k = 0; k < uz; ++k) {
                const auto b = static_cast<std::uint8_t>(g());
                w.baytlar(&b, 1);
            }
            break;
        }
        case Tip::dizi: {
            const auto uz = static_cast<std::uint32_t>(g() % (std::min<std::uint32_t>(a.azami, derinlik == 0 ? 4 : 2) + 1));
            sayac(uz);
            for (std::uint32_t k = 0; k < uz; ++k) {
                rastgele_yuk(g, a.oge, a.oge_sayisi, w, derinlik + 1);
            }
            break;
        }
        default:
            for (int k = 0; k < tekrar; ++k) {
                switch (a.tip) {
                case Tip::u8: w.u8(static_cast<std::uint8_t>(g())); break;
                case Tip::u16: w.u16(static_cast<std::uint16_t>(g())); break;
                case Tip::u32: w.u32(static_cast<std::uint32_t>(g())); break;
                case Tip::u64: w.u64(g()); break;
                case Tip::i32: w.i32(static_cast<std::int32_t>(static_cast<std::uint32_t>(g()))); break;
                case Tip::i64: w.i64(static_cast<std::int64_t>(g())); break;
                case Tip::f32: w.f32(static_cast<float>(static_cast<double>(static_cast<std::int32_t>(static_cast<std::uint32_t>(g()))) / 1024.0)); break;
                case Tip::f64: w.f64(static_cast<double>(static_cast<std::int64_t>(g())) / 1048576.0); break;
                default: break;
                }
            }
        }
    }
}

template <typename Y>
void gidis_donus(Denetci& d, std::mt19937_64& g, std::string_view yuk_adi)
{
    const YukTanimi* t = yuk_bul(yuk_adi);
    for (int vaka = 0; vaka < 200; ++vaka) {
        std::vector<std::uint8_t> b(70000);
        Yazici w(b.data(), b.size());
        rastgele_yuk(g, t->alanlar, t->alan_sayisi, w);
        b.resize(w.konum());
        Y y;
        if (!coz(b.data(), b.size(), y)) {
            DOGRULA(d, false, std::string(yuk_adi) + ": üretilen çözücü reddetti");
            return;
        }
        Okuyucu r(b.data(), b.size());
        std::string genel;
        const bool ok = genel_coz(r, t->alanlar, t->alan_sayisi, genel) && r.kalan() == 0;
        DOGRULA(d, ok && genel == json_dok(y), std::string(yuk_adi) + ": tablo çözümü ile üretilen döküm farklı");
        std::vector<std::uint8_t> geri(b.size() + 16);
        std::size_t n = 0;
        DOGRULA(d, boyut(y) == b.size() && kodla(y, geri.data(), geri.size(), n) && n == b.size() &&
                       std::memcmp(geri.data(), b.data(), n) == 0,
                std::string(yuk_adi) + ": yeniden kodlama aynı baytları vermedi");
    }
}

template <typename Y>
void dizili_denet(Denetci& d, const Y& y, std::string_view yuk_adi)
{
    const YukTanimi* t = yuk_bul(yuk_adi);
    std::vector<std::uint8_t> b(boyut(y));
    std::size_t n = 0;
    DOGRULA(d, kodla(y, b.data(), b.size(), n) && n == b.size(), std::string(yuk_adi) + ": kodlama");
    Okuyucu r(b.data(), b.size());
    std::string genel;
    DOGRULA(d, genel_coz(r, t->alanlar, t->alan_sayisi, genel) && r.kalan() == 0 && genel == json_dok(y),
            std::string(yuk_adi) + ": tablo çözümü ile döküm farklı\n    " + genel + "\n    " + json_dok(y));
}

}  // namespace

std::vector<std::pair<std::string, std::string>> uyum_ornekleri();

int test_kodlayici()
{
    Denetci d;
    std::mt19937_64 g(0xA3C0DE);
    gidis_donus<yuk_calistirma_durum>(d, g, "yuk_calistirma_durum");
    gidis_donus<yuk_olay>(d, g, "yuk_olay");
    gidis_donus<yuk_kart_durum>(d, g, "yuk_kart_durum");
    gidis_donus<yuk_emniyet_girdiler>(d, g, "yuk_emniyet_girdiler");
    gidis_donus<yuk_basinc>(d, g, "yuk_basinc");
    gidis_donus<yuk_dvl>(d, g, "yuk_dvl");
    gidis_donus<yuk_batarya>(d, g, "yuk_batarya");
    gidis_donus<yuk_ping360>(d, g, "yuk_ping360");
    gidis_donus<yuk_ping1d>(d, g, "yuk_ping1d");
    gidis_donus<yuk_kart_cekirdek_sayaclari>(d, g, "yuk_kart_cekirdek_sayaclari");
    gidis_donus<yuk_imu>(d, g, "yuk_imu");
    gidis_donus<yuk_firlatici>(d, g, "yuk_firlatici");
    gidis_donus<yuk_kart_baglanti_sayaclari>(d, g, "yuk_kart_baglanti_sayaclari");
    // Dizi içeren yükler: ayırt edici elle değerler
    std::array<yuk_fizik_durum_govdeler, 2> govde{};
    govde[0] = {7, 1, 2, {1.5, -2.25, 3.125}, {0.5, 0.25, -0.125, 0.0625}, {1, 2, 3, 4, 5, 6}, {-1, -2, -3}};
    govde[1] = {9, 2, 255, {10, 20, 30}, {1, 0, 0, 0}, {0, 0, 0, 0, 0, 0}, {0, 0, 0}};
    std::array<yuk_fizik_durum_iticiler, 3> itici{{{0.5f, 20.0f, 3.0f}, {-0.25f, -10.0f, 1.5f}, {1.0f, 40.0f, 6.0f}}};
    yuk_fizik_durum f;
    f.adim_no = 123456789012345ull;
    f.govdeler = govde;
    f.iticiler = itici;
    f.bara_gerilimi_v = 15.5f;
    f.paket_akimi_a = 2.75f;
    f.soc = 0.625f;
    dizili_denet(d, f, "yuk_fizik_durum");
    std::array<yuk_sayac_gercek_zaman_mesaj_retleri, 2> mr{{{11, 12, 13}, {21, 22, 23}}};
    yuk_sayac_gercek_zaman s;
    s.parti_gonderilen = 1;
    s.olay_sonuc = {1, 2, 3, 4, 5, 6};
    s.mesaj_retleri = mr;
    s.a3_halka_tasma = 99;
    s.kayit_dusen = 100;
    dizili_denet(d, s, "yuk_sayac_gercek_zaman");
    std::array<yuk_atis_durum_tupler, 2> tup{{{0, 1, 0, -1, 2}, {1, 0, 1, 5000, 3}}};
    yuk_atis_durum a;
    a.fc_durumu = 2;
    a.engel_maskesi = 0x30;
    a.kart_son_olay_kimligi = 77;
    a.tupler = tup;
    dizili_denet(d, a, "yuk_atis_durum");
    std::array<yuk_ariza_etkin_arizalar, 1> ar{};
    ar[0].fault_id = "E3";
    ar[0].sinif = 1;
    ar[0].hedef = "itici_3";
    ar[0].tur = "zayiflama";
    ar[0].kaynak = 1;
    ar[0].baslangic_adimi = 400;
    yuk_ariza_etkin ae;
    ae.arizalar = ar;
    dizili_denet(d, ae, "yuk_ariza_etkin");
    std::array<yuk_baglanti_durum_emulator, 1> em{{{1, 4096, 3, 0}}};
    yuk_baglanti_durum bd;
    bd.a1_oturum = 3;
    bd.sess = 9;
    bd.emulator = em;
    bd.bu_baglanti_ezilen_durum = 5;
    bd.bu_baglanti_dusen_dizi = 6;
    bd.bu_baglanti_olay_kuyrugu = 7;
    dizili_denet(d, bd, "yuk_baglanti_durum");
    // Sınır: azami'yi aşan metin ve dizi kodlanmaz
    yuk_olay o;
    const std::string uzun(8193, 'x');
    o.ayrinti = uzun;
    std::vector<std::uint8_t> b(boyut(o));
    std::size_t n = 0;
    DOGRULA(d, !kodla(o, b.data(), b.size(), n), "8192 baytı aşan olay ayrıntısı kodlanmaz");
    std::vector<yuk_fizik_durum_govdeler> cok(65);
    f.govdeler = cok;
    b.resize(boyut(f));
    DOGRULA(d, !kodla(f, b.data(), b.size(), n), "64'ten çok gövde kodlanmaz");
    DOGRULA(d, uyum_ornekleri().size() == 15, "uyum örnekleri: yayınlanan 15 kanal");
    return d.hata;
}

}  // namespace ida::a3::test
