// test_saglam.cpp — sol incelemesinin ayrıştırıcı ve sağlamlık bulgularının karşı örnekleri
// (docs/gorevler/inceleme/W4-a3-sol.md #4–#11, #13).
#include "test_destek.hpp"

#include <filesystem>
#include <fstream>
#include <thread>

namespace ida::a3 {
namespace sinama {
extern std::atomic<bool> is_parcacigi_hatasi;
}  // namespace sinama
}  // namespace ida::a3

namespace ida::a3::test {

namespace {

std::vector<json::Deger> metin_iletileri(Ortam& o, BagId id)
{
    std::vector<json::Deger> r;
    for (const auto& c : o.ws_al(id)) {
        if (c.opkod == 1) {
            json::Deger d;
            json::coz(c.yuk, d);
            r.push_back(std::move(d));
        }
    }
    return r;
}

}  // namespace

// #4: aralık dışı tam değerli hz (UB'siz) durum kanalında gecersiz_hz; dizi kanalında yok sayılır.
int test_hz_aralik_disi()
{
    Denetci d;
    Ortam o;
    const BagId id = o.ws_ac();
    o.ws_al(id);
    o.gonder(id, ws_istemci_cercevesi(1, R"({"tur":"abone_ol","istek_no":1,"kanallar":[{"kanal":"fizik/durum","hz":-1},)"
                                         R"({"kanal":"kart/durum","hz":4294967306},{"kanal":"sensor/dvl","hz":1e100},)"
                                         R"({"kanal":"sonar/ping360","hz":1e100},{"kanal":"sensor/basinc","hz":25}]})"));
    const auto m = metin_iletileri(o, id);
    DOGRULA(d, m.size() == 1, "tek yanıt");
    if (m.size() == 1) {
        const auto& l = m[0].bul("kanallar")->dizi;
        DOGRULA(d, l.size() == 5, "beş öğe");
        for (std::size_t i = 0; i < 3 && i < l.size(); ++i) {
            DOGRULA(d, l[i].bul("sonuc")->metin == "ret" && l[i].bul("neden")->metin == "gecersiz_hz", "aralık dışı hz ret");
        }
        if (l.size() == 5) {
            DOGRULA(d, l[3].bul("sonuc")->metin == "kabul", "dizi kanalında hz yok sayılır");
            DOGRULA(d, l[4].bul("sonuc")->metin == "kabul" && l[4].bul("etkin_hz")->sayi == 25, "geçerli hz");
        }
    }
    return d.hata;
}

// #5: A3-K51 okuma sınırı soket okumasında: başlıkla aynı aktarımda gelen aşırı gövdeden en çok 4096 bayt okunur.
int test_govde_okuma_butcesi()
{
    Denetci d;
    CanliSunucu s;
    if (!s.sunucu) {
        return 1;
    }
    SoketIstemci c(s.sunucu->port());
    const std::string bas = "POST /api/v1/istemciler HTTP/1.1\r\n" + s.host() + "\r\n" + s.yetki() +
                            "\r\nContent-Type: application/json\r\nContent-Length: 2097152\r\n\r\n";
    (void)c.yaz(bas + std::string(100000, ' '), 1000);  // sunucu kalanı okumaz: yazım zaman aşımına düşebilir
    // 413 yanıtı denetlenmez: sunucu okunmamış veriyle kapattığında TCP RST yanıtı istemciye
    // ulaşmadan silebilir. Ölçülen, sunucunun soketten gerçekten okuduğu gövde baytıdır.
    HttpYanit y;
    (void)c.yanit_bekle(y, 500);
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    const auto n = s.sunucu->sayaclar().azami_asim_govde_bayti;
    std::printf("ÖLÇÜM: sınırı aşan gövdede başlıktan sonra soketten okunan bayt: %llu\n", static_cast<unsigned long long>(n));
    DOGRULA(d, n <= 4096, "en çok 4096 bayt okundu: " + std::to_string(n));
    return d.hata;
}

// #6: çıkış baskısıyla işlenmeden kalan ikinci istek, baskı kalkınca yeni ağ baytı olmadan yanıtlanır.
int test_pipeline_baski()
{
    namespace fs = std::filesystem;
    Denetci d;
    const fs::path kok = fs::current_path() / "a3_pipeline_deneme";
    std::error_code ec;
    fs::remove_all(kok, ec);
    fs::create_directories(kok / "panel");
    {
        std::ofstream f(kok / "panel" / "buyuk.js", std::ios::binary);
        f << std::string(std::size_t{5} << 20u, 'b');  // okuma eşiğinden (4 MiB) büyük
    }
    SunucuAyari sa;
    sa.statik_kok = kok.string();
    Ortam o({}, sa);
    const BagId id = o.ac();
    o.gonder(id, istek("GET", "/panel/buyuk.js", {}, {}, true) + istek("GET", "/api/v1/surum"));
    o.dongu();
    std::string t;
    for (int i = 0; i < 4; ++i) {
        t += o.al(id);  // çıkış okunur (baskı kalkar); yeni girdi gönderilmez
        o.dongu();
    }
    t += o.al(id);
    HttpYanit a;
    HttpYanit b;
    DOGRULA(d, http_yanit_ayir(t, a) && a.durum == 200 && a.govde.size() == (std::size_t{5} << 20u), "büyük ilk yanıt");
    DOGRULA(d, http_yanit_ayir(t, b) && b.durum == 200 && b.govde.find("a3_surumu") != std::string::npos, "ikinci istek yanıtlandı");
    fs::remove_all(kok, ec);
    return d.hata;
}

// #7: HEAD yanıtı gövdesizdir; kalıcı bağlantıda ardından gelen GET doğru ayrıştırılır.
int test_head_sonra_get()
{
    Denetci d;
    Ortam o;
    const BagId id = o.ac();
    o.gonder(id, "HEAD / HTTP/1.1\r\n" + host() + "\r\n\r\n" + istek("GET", "/api/v1/surum") + "HEAD /api/v1/surum HTTP/1.1\r\n" +
                     host() + "\r\n\r\n" + istek("GET", "/api/v1/surum"));
    o.dongu();
    std::string t = o.al(id);
    HttpYanit y1;
    HttpYanit y2;
    HttpYanit y3;
    HttpYanit y4;
    DOGRULA(d, http_yanit_ayir(t, y1, true) && y1.durum == 405 && y1.govde.empty(), "HEAD / 405 gövdesiz");
    DOGRULA(d, http_yanit_ayir(t, y2) && y2.durum == 200 && y2.govde.find("a3_surumu") != std::string::npos, "ardından GET");
    DOGRULA(d, http_yanit_ayir(t, y3, true) && y3.durum == 403, "jetonsuz HEAD /api 403 gövdesiz");
    DOGRULA(d, http_yanit_ayir(t, y4) && y4.durum == 200 && t.empty(), "ardından GET; fazla bayt yok");
    return d.hata;
}

// #8: trailer ya da uzantılı chunked istek 400 ve bağlantı kapanışı.
int test_chunked_bozuk()
{
    Denetci d;
    Ortam o;
    const std::uint32_t ist = o.istemci_al();
    for (const char* govde : {"2\r\n{}\r\n0\r\nGET /x HTTP/1.1\r\n\r\n", "2;a=b\r\n{}\r\n0\r\n\r\n"}) {
        const BagId id = o.ac();
        o.gonder(id, "POST /api/v1/istemciler HTTP/1.1\r\n" + host() + "\r\n" + yetki() +
                         "\r\nContent-Type: application/json\r\nIda-Istemci: " + std::to_string(ist) +
                         "\r\nTransfer-Encoding: chunked\r\n\r\n" + govde);
        o.dongu();
        std::string t = o.al(id);
        HttpYanit y;
        DOGRULA(d, http_yanit_ayir(t, y) && y.durum == 400 && t.empty(), std::string("400, tek yanıt: ") + govde);
        DOGRULA(d, o.cekirdek.kapatilmali(id), "bağlantı kapanır");
    }
    return d.hata;
}

// #9: WebSocket uzunluğunun en kısa gösterimi (RFC 6455 §5.2).
int test_ws_en_kisa_uzunluk()
{
    Denetci d;
    const std::uint8_t maske[4] = {1, 2, 3, 4};
    auto cerceve = [&](int bicim) {
        std::string c;
        c.push_back('\x81');  // FIN + metin
        if (bicim == 126) {
            c.push_back('\xfe');  // maske + 126
            c.push_back('\0');
            c.push_back('\x02');
        } else {
            c.push_back('\xff');  // maske + 127
            c.append(7, '\0');
            c.push_back('\x02');
        }
        c.append(reinterpret_cast<const char*>(maske), 4);
        c.push_back(static_cast<char>('{' ^ 1));
        c.push_back(static_cast<char>('}' ^ 2));
        return c;
    };
    for (const int bicim : {126, 127}) {
        const std::string c = cerceve(bicim);
        ws::Cerceve w;
        std::uint16_t kod = 0;
        std::size_t t = 0;
        const auto s = ws::oku(reinterpret_cast<const std::uint8_t*>(c.data()), c.size(), t, w, kod, 65536);
        DOGRULA(d, s == ws::Okuma::hata && kod == 1002, "iki baytlık yük " + std::to_string(bicim) + " biçiminde 1002");
        Ortam o;
        const BagId id = o.ws_ac();
        o.ws_al(id);
        o.gonder(id, c);
        bool k1002 = false;
        for (const auto& x : o.ws_al(id)) {
            k1002 = k1002 || (x.opkod == 8 && x.yuk.size() >= 2 && static_cast<unsigned char>(x.yuk[1]) == 0xEA);
        }
        DOGRULA(d, k1002, "sunucu 1002 ile kapatır");
    }
    return d.hata;
}

// #10: G/Ç iş parçacığı başlatılamazsa süreç çökmez; hata döner ve kaynaklar bırakılır.
int test_baslatma_hatasi()
{
    Denetci d;
    sahte::SahteAyar sa;
    sa.is_parcacigi = false;
    Yayinci y(1u << 16u, 1u << 16u);
    sahte::SahteDunya dunya(y, sa);
    SunucuAyari a;
    a.port = 0;
    std::string hata;
    sinama::is_parcacigi_hatasi.store(true);
    auto s = Sunucu::baslat(a, dunya, hata);
    DOGRULA(d, s == nullptr && hata.find("başlatılamadı") != std::string::npos, "başlatma hatası döner: " + hata);
    hata.clear();
    auto s2 = Sunucu::baslat(a, dunya, hata);
    DOGRULA(d, s2 != nullptr, "ardından olağan başlatma: " + hata);
    return d.hata;
}

// #11: uyandırma çiftinin eş denetimi aile + IP + port, iki yönde.
int test_uyandirma_eslesmesi()
{
    Denetci d;
    ag::Tutamac l = ag::gecersiz();
    std::uint16_t port = 0;
    std::string h;
    DOGRULA(d, ag::dinle("127.0.0.1", 0, l, port, h), "dinleyici");
    const ag::Tutamac c1 = ag::baglan("127.0.0.1", port);
    const ag::Tutamac c2 = ag::baglan("127.0.0.1", port);
    std::vector<ag::Tutamac> kabul;
    for (int i = 0; i < 200 && kabul.size() < 2; ++i) {
        std::vector<ag::Yoklama> y(1);
        y[0].t = l;
        ag::yokla(y, 10);
        const ag::Tutamac t = ag::kabul(l);
        if (t != ag::gecersiz()) {
            kabul.push_back(t);
        }
    }
    DOGRULA(d, kabul.size() == 2, "iki bağlantı kabul edildi");
    if (kabul.size() == 2) {
        const bool a0 = ag::uclar_eslesir(c1, kabul[0]);
        const bool a1 = ag::uclar_eslesir(c1, kabul[1]);
        const bool b0 = ag::uclar_eslesir(c2, kabul[0]);
        const bool b1 = ag::uclar_eslesir(c2, kabul[1]);
        DOGRULA(d, (a0 != a1) && (b0 != b1) && (a0 != b0), "her istemci yalnız kendi karşı ucuyla eşleşir");
        DOGRULA(d, !ag::uclar_eslesir(kabul[0], kabul[1]), "iki kabul edilen uç eşleşmez");
    }
    for (const auto t : kabul) {
        ag::kapat(t);
    }
    ag::kapat(c1);
    ag::kapat(c2);
    ag::kapat(l);
    ag::Tutamac o1 = ag::gecersiz();
    ag::Tutamac o2 = ag::gecersiz();
    DOGRULA(d, ag::uyandirma_cifti(o1, o2), "uyandırma çifti");
    ag::kapat(o1);
    ag::kapat(o2);
    return d.hata;
}

// #13 / W4-K22 (b): geçmiş tek gösterimle; çok baytlı uzun olaylarla pencere 64 MiB'ın tamamını kullanır.
int test_olay_gecmisi_cok_bayt()
{
    Denetci d;
    Ortam o;
    std::string ileti;
    for (int i = 0; i < 1024; ++i) {
        ileti += "\xf0\x9f\x98\x80";  // U+1F600, 4 bayt
    }
    const std::string ayrinti = "{\"kod\":\"uzun\",\"ileti\":\"" + ileti + "\"}";
    yuk_olay ol;
    ol.tur = 22;
    ol.onem = 1;
    ol.ayrinti = ayrinti;
    const int kAdet = 12000;  // ≈ 4,1 kB × 12 000 ≈ 50 MB < 64 MiB (iki gösterimle ≈ 100 MB olurdu)
    for (int i = 0; i < kAdet; ++i) {
        o.yayinci.olay(i, ol);
        if (i % 100 == 99) {
            o.dongu();
        }
    }
    o.dongu();
    const HttpYanit y = o.http(istek("GET", "/api/v1/olaylar?sonra=0&en_cok=3"));
    const json::Deger j = y.json_();
    DOGRULA(d, y.durum == 200, "pencere içinde 410 yok: " + std::to_string(y.durum));
    if (y.durum == 200) {
        const auto& l = j.bul("olaylar")->dizi;
        DOGRULA(d, l.size() == 3 && l[0].bul("sira")->sayi == 1 && l[0].bul("ayrinti")->bul("ileti")->metin == ileti &&
                       l[0].bul("tur")->metin == "uyari" && l[0].bul("onem")->metin == "uyari",
                "istek anında üretilen JSON karşılığı");
    }
    const HttpYanit son = o.http(istek("GET", "/api/v1/olaylar?sonra=" + std::to_string(kAdet - 1)));
    DOGRULA(d, son.durum == 200 && son.json_().bul("olaylar")->dizi.size() == 1, "son olay");
    return d.hata;
}

}  // namespace ida::a3::test
