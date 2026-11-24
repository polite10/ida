// test_akis.cpp — WebSocket akışı: merhaba, zorunlu kanallar, abonelik, seyreltme, olay geçmişi,
// kapanış kodları, canlılık, olay kuyruğu taşması (A3-K17…K49). Soketsiz çekirdek, sanal saat.
#include "test_destek.hpp"

namespace ida::a3::test {

namespace {

std::string abone(int no, const std::string& kanallar)
{
    return ws_istemci_cercevesi(1, "{\"tur\":\"abone_ol\",\"istek_no\":" + std::to_string(no) + ",\"kanallar\":[" + kanallar + "]}");
}

std::vector<A3Kare> kareler(const std::vector<WsCerceve>& cerceveler)
{
    std::vector<A3Kare> r;
    for (const auto& c : cerceveler) {
        if (c.opkod == 2) {
            a3_ileti_ayir(c.yuk, r);
        }
    }
    return r;
}

std::vector<json::Deger> metinler(const std::vector<WsCerceve>& cerceveler)
{
    std::vector<json::Deger> r;
    for (const auto& c : cerceveler) {
        if (c.opkod == 1) {
            json::Deger d;
            json::coz(c.yuk, d);
            r.push_back(std::move(d));
        }
    }
    return r;
}

std::size_t say(const std::vector<A3Kare>& k, std::uint16_t kanal_id)
{
    std::size_t n = 0;
    for (const auto& x : k) {
        if (x.baslik.kanal_id == kanal_id) {
            ++n;
        }
    }
    return n;
}

std::optional<std::uint16_t> kapanis(const std::vector<WsCerceve>& c)
{
    for (const auto& x : c) {
        if (x.opkod == 8 && x.yuk.size() >= 2) {
            return static_cast<std::uint16_t>((static_cast<unsigned char>(x.yuk[0]) << 8u) | static_cast<unsigned char>(x.yuk[1]));
        }
    }
    return std::nullopt;
}

void fizik_yayinla(Ortam& o, std::uint64_t adim)
{
    yuk_fizik_durum f;
    f.adim_no = adim;
    o.yayinci.yayinla<Kanal::fizik_durum>(static_cast<std::int64_t>(adim) * 1250000, f);
}

}  // namespace

int test_merhaba_ve_zorunlu()
{
    Denetci d;
    Ortam o;
    for (int i = 0; i < 20; ++i) {
        o.dunya.adim();  // 20. döngüde calistirma/durum, sayac/gercek_zaman yayınlanır
    }
    o.dongu();
    std::string merhaba;
    const BagId id = o.ws_ac(&merhaba);
    DOGRULA(d, id != 0, "el sıkışma");
    json::Deger m;
    DOGRULA(d, json::coz(merhaba, m) && m.bul("tur")->metin == "merhaba", "ilk ileti merhaba (A3-K17)");
    DOGRULA(d, m.bul("a3_surumu")->metin == "1.0" && m.bul("katalog_ozeti")->metin.rfind("sha256:", 0) == 0 &&
                   m.bul("yayin_tavani_hz")->sayi == 50 && m.bul("run_id")->tur == json::Tur::bos,
            "merhaba alanları");
    const auto c = o.ws_al(id);
    const auto k = kareler(c);
    DOGRULA(d, say(k, 1) == 1 && say(k, 2) == 1, "zorunlu durum kanallarının özeti aboneliksiz (A3-K29, K35)");
    for (const auto& x : k) {
        DOGRULA(d, (x.baslik.bayraklar & bayrak::ozet) != 0, "özet bayrağı");
    }
    // Olay kanalı aboneliksiz gelir
    yuk_olay ol;
    ol.tur = 22;
    ol.ayrinti = R"({"kod":"x","ileti":"y"})";
    o.yayinci.olay(1, ol);
    o.ilerle(std::chrono::milliseconds(25));
    DOGRULA(d, say(kareler(o.ws_al(id)), 3) == 1, "olay aboneliksiz gelir");
    // Zorunlu kanal bırakılamaz
    o.gonder(id, ws_istemci_cercevesi(1, R"({"tur":"abonelik_birak","istek_no":5,"kanallar":["calistirma/durum","olay","yok/kanal"]})"));
    const auto mt = metinler(o.ws_al(id));
    DOGRULA(d, mt.size() == 1 && mt[0].bul("tur")->metin == "abonelik_yaniti" && mt[0].bul("istek_no")->sayi == 5,
            "abonelik_yaniti aynı istek_no (A3-K31)");
    if (mt.size() == 1) {
        const auto& l = mt[0].bul("kanallar")->dizi;
        DOGRULA(d, l.size() == 3 && l[0].bul("neden")->metin == "zorunlu_kanal" && l[1].bul("neden")->metin == "zorunlu_kanal" &&
                       l[2].bul("neden")->metin == "bilinmeyen_kanal",
                "zorunlu_kanal ve bilinmeyen_kanal retleri");
    }
    return d.hata;
}

int test_abonelik_ve_ozet()
{
    Denetci d;
    Ortam o;
    fizik_yayinla(o, 1);
    yuk_kart_durum kd;
    o.yayinci.yayinla<Kanal::kart_durum>(5, kd);
    o.dongu();
    const BagId id = o.ws_ac();
    o.ws_al(id);
    o.gonder(id, abone(7, R"({"kanal":"fizik/durum","hz":10},{"kanal":"sensor/imu","hz":7},{"kanal":"sonar/ping360","hz":7},{"kanal":"yok"})"));
    const auto c = o.ws_al(id);
    const auto mt = metinler(c);
    DOGRULA(d, mt.size() == 1, "tek abonelik_yaniti");
    if (mt.size() == 1) {
        const auto& l = mt[0].bul("kanallar")->dizi;
        DOGRULA(d, l.size() == 4, "her kanal için yanıt");
        DOGRULA(d, l[0].bul("sonuc")->metin == "kabul" && l[0].bul("etkin_hz")->sayi == 10 && l[0].bul("kanal_id")->sayi == 10,
                "fizik/durum kabul 10 Hz (A3-K32)");
        DOGRULA(d, l[1].bul("sonuc")->metin == "ret" && l[1].bul("neden")->metin == "gecersiz_hz", "geçersiz hz ret (A3-K30)");
        DOGRULA(d, l[2].bul("sonuc")->metin == "kabul" && l[2].bul("etkin_hz")->tur == json::Tur::bos,
                "dizi sınıfında hz yok sayılır");
        DOGRULA(d, l[3].bul("neden")->metin == "bilinmeyen_kanal", "bilinmeyen kanal");
    }
    // Yanıttan hemen sonra fizik/durum özeti (A3-K35), yer gerçeği bayrağıyla (A3-K28)
    bool yanit_once = false;
    bool ozet = false;
    for (const auto& x : c) {
        if (x.opkod == 1) {
            yanit_once = true;
        } else if (x.opkod == 2 && yanit_once) {
            std::vector<A3Kare> k;
            a3_ileti_ayir(x.yuk, k);
            for (const auto& kr : k) {
                if (kr.baslik.kanal_id == 10 && (kr.baslik.bayraklar & bayrak::ozet) != 0 &&
                    (kr.baslik.bayraklar & bayrak::yer_gercegi) != 0) {
                    ozet = true;
                }
            }
        }
    }
    DOGRULA(d, ozet, "abonelik özeti, yanıttan sonra, özet + yer_gercegi bayraklı");
    // Abone olunmayan kanal gönderilmez (A3-K36)
    o.yayinci.yayinla<Kanal::kart_durum>(6, kd);
    fizik_yayinla(o, 2);
    o.ilerle(std::chrono::milliseconds(200));
    const auto k = kareler(o.ws_al(id));
    DOGRULA(d, say(k, 11) == 0, "abone olunmayan kart/durum gelmez");
    DOGRULA(d, say(k, 10) == 1, "yeni fizik/durum bir kez");
    o.ilerle(std::chrono::milliseconds(300));
    DOGRULA(d, say(kareler(o.ws_al(id)), 10) == 0, "yeni değer yoksa gönderim yok (A3-K33)");
    // Bırakma
    o.gonder(id, ws_istemci_cercevesi(1, R"({"tur":"abonelik_birak","istek_no":8,"kanallar":["fizik/durum"]})"));
    fizik_yayinla(o, 3);
    o.ilerle(std::chrono::milliseconds(200));
    DOGRULA(d, say(kareler(o.ws_al(id)), 10) == 0, "bırakılan kanal gelmez");
    return d.hata;
}

int test_seyreltme_hizi()
{
    Denetci d;
    for (const int hz : {50, 25, 10, 5, 2, 1}) {
        Ortam o;
        const BagId id = o.ws_ac();
        o.gonder(id, abone(1, "{\"kanal\":\"fizik/durum\",\"hz\":" + std::to_string(hz) + "}"));
        o.ws_al(id);
        std::size_t n = 0;
        // 2 s boyunca her 5 ms'de yeni değer (200 Hz üretim)
        for (int i = 0; i < 400; ++i) {
            fizik_yayinla(o, static_cast<std::uint64_t>(i + 10));
            o.ilerle(std::chrono::milliseconds(5));
            n += say(kareler(o.ws_al(id)), 10);
        }
        const auto beklenen = static_cast<std::size_t>(2 * hz);
        DOGRULA(d, n + 1 >= beklenen && n <= beklenen + 1,
                std::to_string(hz) + " Hz: " + std::to_string(n) + " çerçeve (beklenen " + std::to_string(beklenen) + " ±1)");
    }
    // Yayın tavanı (A3-K37): 25 Hz
    Ortam o;
    const std::uint32_t ist = o.istemci_al();
    const HttpYanit y = o.http(istek("PUT", "/api/v1/yayin",
                                     {"Content-Type: application/json", "Ida-Istemci: " + std::to_string(ist)},
                                     R"({"istek_kimligi":"tavan-0001","yayin_tavani_hz":25})"));
    DOGRULA(d, y.durum == 200 && y.json_().bul("komut_no") != nullptr, "PUT /yayin 200");
    const BagId id = o.ws_ac();
    o.gonder(id, abone(1, R"({"kanal":"fizik/durum","hz":50})"));
    const auto mt = metinler(o.ws_al(id));
    DOGRULA(d, !mt.empty() && mt[0].bul("kanallar")->dizi[0].bul("etkin_hz")->sayi == 25, "etkin hz tavanla 25");
    std::size_t n = 0;
    for (int i = 0; i < 400; ++i) {
        fizik_yayinla(o, static_cast<std::uint64_t>(i + 10));
        o.ilerle(std::chrono::milliseconds(5));
        n += say(kareler(o.ws_al(id)), 10);
    }
    DOGRULA(d, n >= 49 && n <= 51, "tavan 25 Hz: " + std::to_string(n));
    const HttpYanit durum = o.http(istek("GET", "/api/v1/durum"));
    DOGRULA(d, durum.json_().bul("yayin_tavani_hz")->sayi == 25, "GET /durum tavan");
    return d.hata;
}

int test_olay_gecmisi()
{
    Denetci d;
    Ortam o;
    yuk_olay ol;
    ol.tur = 22;
    ol.onem = 1;
    ol.ayrinti = R"({"kod":"gecmis","ileti":"x"})";
    for (int i = 0; i < 1500; ++i) {
        o.yayinci.olay(i, ol);
        if (i % 500 == 499) {
            o.dongu();
        }
    }
    o.dongu();
    HttpYanit y = o.http(istek("GET", "/api/v1/olaylar?sonra=0&en_cok=1000"));
    json::Deger j = y.json_();
    DOGRULA(d, y.durum == 200 && j.bul("olaylar")->dizi.size() == 1000 && !j.bul("tamam")->mantik, "ilk sayfa 1000");
    DOGRULA(d, j.bul("olaylar")->dizi[0].bul("sira")->sayi == 1 && j.bul("olaylar")->dizi[999].bul("sira")->sayi == 1000,
            "sıra 1'den boşluksuz (A3-K27)");
    DOGRULA(d, j.bul("olaylar")->dizi[0].bul("tur")->metin == "uyari" && j.bul("olaylar")->dizi[0].bul("onem")->metin == "uyari" &&
                   j.bul("olaylar")->dizi[0].bul("ayrinti")->bul("kod")->metin == "gecmis",
            "JSON karşılığı (A3-K45)");
    y = o.http(istek("GET", "/api/v1/olaylar?sonra=1000&en_cok=1000"));
    j = y.json_();
    DOGRULA(d, j.bul("olaylar")->dizi.size() == 500 && j.bul("tamam")->mantik, "ikinci sayfa 500, tamam");
    y = o.http(istek("GET", "/api/v1/olaylar?sonra=1500"));
    DOGRULA(d, y.json_().bul("olaylar")->dizi.empty() && y.json_().bul("tamam")->mantik, "yeni olay yok");
    DOGRULA(d, o.http(istek("GET", "/api/v1/olaylar?sonra=0&en_cok=0")).durum == 422, "en_cok 0 → 422");
    DOGRULA(d, o.http(istek("GET", "/api/v1/olaylar?sonra=0&en_cok=1001")).durum == 422, "en_cok 1001 → 422");
    DOGRULA(d, o.http(istek("GET", "/api/v1/olaylar?en_cok=10")).durum == 400, "sonra yok → 400");
    DOGRULA(d, o.http(istek("GET", "/api/v1/olaylar?sonra=x")).durum == 400, "sonra bozuk → 400");
    // Geçmiş penceresi: 65 536'dan eskisi 410 (A3-K45)
    for (int i = 0; i < 66000; ++i) {
        o.yayinci.olay(i, ol);
        if (i % 1000 == 999) {
            o.dongu();
        }
    }
    o.dongu();
    y = o.http(istek("GET", "/api/v1/olaylar?sonra=0"));
    DOGRULA(d, y.durum == 410 && y.kod() == "gecmis_yok", "pencere dışı 410");
    // Halka taşması: olay_kaybi (A3-K44), sıra boşluksuz. Büyük ayrıntı: halka az kayıtla dolar.
    Ortam o2;
    const BagId id = o2.ws_ac();
    o2.ws_al(id);
    int dusen = 0;
    const std::string buyuk = std::string("{\"kod\":\"dolgu\",\"ileti\":\"") + std::string(7000, 'x') + "\"}";
    ol.ayrinti = buyuk;
    for (int i = 0; i < 100000 && dusen < 10; ++i) {
        if (!o2.yayinci.olay(i, ol)) {
            ++dusen;
        }
    }
    o2.ilerle(std::chrono::milliseconds(40));
    std::vector<A3Kare> k;
    for (int t = 0; t < 200; ++t) {
        const auto x = kareler(o2.ws_al(id));
        k.insert(k.end(), x.begin(), x.end());
        o2.ilerle(std::chrono::milliseconds(20));
    }
    bool bosluksuz = true;
    bool kayip = false;
    for (std::size_t i = 0; i < k.size(); ++i) {
        if (k[i].baslik.sira != i + 1) {
            bosluksuz = false;
        }
        yuk_olay e;
        if (coz(k[i].yuk.data(), k[i].yuk.size(), e) && e.tur == 18 && e.ayrinti.find("\"kayip_sayisi\":10") != std::string_view::npos) {
            kayip = true;
        }
    }
    DOGRULA(d, dusen == 10 && kayip, "olay_kaybi kayip_sayisi 10");
    DOGRULA(d, bosluksuz && !k.empty(), "WebSocket'te olay sırası boşluksuz");
    return d.hata;
}

int test_ws_kapanis_kodlari()
{
    Denetci d;
    struct Vaka {
        const char* ad;
        std::string veri;
        std::uint16_t kod;
    };
    const std::vector<Vaka> vakalar = {
        {"ikili ileti 1003 (A3-K47)", ws_istemci_cercevesi(2, "ab"), 1003},
        {"geçersiz UTF-8 1007", ws_istemci_cercevesi(1, "\xff\xfe"), 1007},
        {"64 KiB üstü 1009", ws_istemci_cercevesi(1, std::string(65537, 'a')), 1009},
        {"parçalı toplamı 64 KiB üstü 1009",
         ws_istemci_cercevesi(1, std::string(40000, 'a'), false) + ws_istemci_cercevesi(0, std::string(40000, 'a')), 1009},
        {"maskesiz 1002", ws_istemci_cercevesi(1, "{}", true, false), 1002},
        {"başsız devam 1002", ws_istemci_cercevesi(0, "x"), 1002},
        {"parça arası yeni ileti 1002", ws_istemci_cercevesi(1, "a", false) + ws_istemci_cercevesi(1, "b"), 1002},
        {"istemci kapanışı yankılanır", ws_istemci_cercevesi(8, std::string("\x03\xe8", 2)), 1000},
        {"geçersiz kapanış kodu 1002", ws_istemci_cercevesi(8, std::string("\x03\xed", 2)), 1002},
    };
    for (const auto& v : vakalar) {
        Ortam o;
        const BagId id = o.ws_ac();
        o.ws_al(id);
        o.gonder(id, v.veri);
        const auto k = kapanis(o.ws_al(id));
        DOGRULA(d, k && *k == v.kod, std::string(v.ad) + ": " + (k ? std::to_string(*k) : std::string("kapanış yok")));
        DOGRULA(d, o.cekirdek.kapatilmali(id), std::string(v.ad) + ": bağlantı kapanır");
    }
    // Açık kalanlar: hata iletisi (A3-K48), parçalı ileti birleştirme, ping/pong
    Ortam o;
    const BagId id = o.ws_ac();
    o.ws_al(id);
    o.gonder(id, ws_istemci_cercevesi(1, "{bozuk"));
    o.gonder(id, ws_istemci_cercevesi(1, R"({"tur":"selam","istek_no":3})"));
    o.gonder(id, ws_istemci_cercevesi(1, R"({"tur":"abone_ol","istek_no":4,"kanallar":[{"kanal":"x","fazla":1}]})"));
    o.gonder(id, ws_istemci_cercevesi(1, R"({"tur":"abone_ol","istek_no":5,"kanallar":[{"kanal":"fizik/durum","hz":2.5}]})"));
    o.gonder(id, ws_istemci_cercevesi(1, R"({"tur":"abone_ol","is)", false) + ws_istemci_cercevesi(0, R"(tek_no":6,"kanallar":[{"kan)", false) +
                     ws_istemci_cercevesi(0, R"(al":"atis/durum"}]})"));
    o.gonder(id, ws_istemci_cercevesi(9, "nabiz"));
    const auto c = o.ws_al(id);
    const auto mt = metinler(c);
    DOGRULA(d, mt.size() == 5, "beş metin yanıtı: " + std::to_string(mt.size()));
    if (mt.size() == 5) {
        DOGRULA(d, mt[0].bul("tur")->metin == "hata" && mt[0].bul("kod")->metin == "gecersiz_ileti" &&
                       mt[0].bul("istek_no")->tur == json::Tur::bos,
                "bozuk JSON → hata");
        DOGRULA(d, mt[1].bul("kod")->metin == "bilinmeyen_tur" && mt[1].bul("istek_no")->sayi == 3, "bilinmeyen tür");
        DOGRULA(d, mt[2].bul("kod")->metin == "gecersiz_ileti" && mt[2].bul("istek_no")->sayi == 4, "şema dışı alan");
        DOGRULA(d, mt[3].bul("kod")->metin == "gecersiz_ileti", "tamsayı olmayan hz şema dışı");
        DOGRULA(d, mt[4].bul("tur")->metin == "abonelik_yaniti" && mt[4].bul("istek_no")->sayi == 6, "parçalı ileti birleşir");
    }
    bool pong = false;
    for (const auto& x : c) {
        if (x.opkod == 10 && x.yuk == "nabiz") {
            pong = true;
        }
    }
    DOGRULA(d, pong, "ping → aynı yüklü pong");
    DOGRULA(d, !o.cekirdek.kapatilmali(id), "bağlantı açık (A3-K48)");
    return d.hata;
}

int test_canlilik()
{
    Denetci d;
    Ortam o;
    const BagId sessiz = o.ws_ac();
    const BagId canli = o.ws_ac();
    o.ws_al(sessiz);
    o.ws_al(canli);
    int ping = 0;
    for (int s = 0; s < 32; ++s) {
        o.ilerle(std::chrono::milliseconds(500));
        for (const auto& c : o.ws_al(sessiz)) {
            ping += c.opkod == 9 ? 1 : 0;
        }
        for (const auto& c : o.ws_al(canli)) {
            if (c.opkod == 9) {
                o.gonder(canli, ws_istemci_cercevesi(10, c.yuk));
            }
        }
    }
    DOGRULA(d, o.cekirdek.kapatilmali(sessiz), "15 s sessiz bağlantı kapandı");
    DOGRULA(d, ping >= 2 && ping <= 3, "5 s'de bir ping (A3-K46): " + std::to_string(ping));
    DOGRULA(d, !o.cekirdek.kapatilmali(canli), "pong veren bağlantı açık");
    // Kapanış kodu 4004
    Ortam o2;
    const BagId b = o2.ws_ac();
    o2.ws_al(b);
    o2.ilerle(std::chrono::milliseconds(15100));
    const auto k = kapanis(o2.ws_al(b));
    DOGRULA(d, k && *k == 4004, "4004 canlılık yok");
    return d.hata;
}

int test_olay_kuyrugu_tasmasi()
{
    Denetci d;
    Ortam o;
    const BagId okumayan = o.ws_ac();
    const BagId okuyan = o.ws_ac();
    o.ws_al(okuyan);
    // `okumayan`ın çıkışı hiç alınmaz: bekleyen çıkış baskı eşiğini aşınca veri eklenmez (A3-K39)
    yuk_olay ol;
    ol.tur = 22;
    const std::string dolgu = std::string("{\"kod\":\"dolgu\",\"ileti\":\"") + std::string(900, 'x') + "\"}";
    ol.ayrinti = dolgu;
    std::size_t gelen = 0;
    for (int i = 0; i < 6000; ++i) {
        o.yayinci.olay(i, ol);
        if (i % 50 == 49) {
            o.ilerle(std::chrono::milliseconds(20));
            gelen += say(kareler(o.ws_al(okuyan)), 3);
        }
    }
    o.ilerle(std::chrono::milliseconds(40));
    gelen += say(kareler(o.ws_al(okuyan)), 3);
    DOGRULA(d, gelen == 6000, "okuyan bağlantı bütün olayları aldı: " + std::to_string(gelen));
    const auto k = kapanis(o.ws_al(okumayan));
    DOGRULA(d, k && *k == 4001, "okumayan bağlantı 4001 ile kapandı (A3-K42)");
    DOGRULA(d, o.cekirdek.sayaclar().olay_tasmasi_kapanis == 1, "sayaç");
    return d.hata;
}

int test_kapanis_1001()
{
    Denetci d;
    Ortam o;
    const BagId w = o.ws_ac();
    o.ws_al(w);
    const BagId h = o.ac();
    o.cekirdek.kapaniyor(o.simdi);
    const auto k = kapanis(o.ws_al(w));
    DOGRULA(d, k && *k == 1001, "1001 (A3-K49)");
    const BagId h2 = o.ac();
    DOGRULA(d, h2 == 0, "kapanırken yeni bağlantı kabul edilmez");
    (void)h;
    // Kapanacak bağlantının çıkışı hiç okunmazsa 5 s sonra atılır ve bağlantı kapatılır.
    Ortam o2;
    const BagId c = o2.ac();
    o2.gonder(c, istek("GET", "/api/v1/surum", {"Connection: close"}));
    o2.dongu();
    DOGRULA(d, !o2.cekirdek.kapatilmali(c), "yanıt bekliyor");
    o2.ilerle(std::chrono::milliseconds(5200));
    DOGRULA(d, o2.cekirdek.kapatilmali(c), "okunmayan kapanış çıkışı süre sonunda atıldı");
    return d.hata;
}

}  // namespace ida::a3::test
