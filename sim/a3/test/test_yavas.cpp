// test_yavas.cpp — yavaş istemci ve seyreltme ölçümü (W4-a3 kabul 4; A3-K38…K41).
//
// Gerçek sunucu ve iş parçacıklı sahte dünya (5 ms döngü, ek Ping360 yükü). Bir istemci abone olur
// ve hiç okumaz; öteki 50 Hz tüketir. Ölçülenler: dünyanın yayın çağrısı süresi (istemcisiz ve
// yavaş istemci varken), okuyan istemcide fizik/durum çerçeveleri arasındaki en büyük boşluk,
// HTTP yanıt süresi, kısıtlayan komutun sonucunun gelme süresi, düşen çerçeve sayaçları ve
// bağlantı kuyruklarının gözlenen bellek tavanı. Sonuçlar "ÖLÇÜM:" satırlarıyla yazılır; bu
// makinedeki değerlerdir, eşik değildir. Doğrulanan yalnız davranıştır (bekletmeme, düşürme ve
// bayrak, sınırlı bellek).
#include "test_destek.hpp"

#include <thread>

namespace ida::a3::test {

namespace {

using Saat = std::chrono::steady_clock;

double ms(Saat::duration d) { return std::chrono::duration<double, std::milli>(d).count(); }

bool ws_baglan(SoketIstemci& c, const CanliSunucu& s)
{
    std::string r = "GET /ws HTTP/1.1\r\n" + s.host() +
                    "\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Version: 13\r\n"
                    "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==\r\nSec-WebSocket-Protocol: ida-a3.v1, ida-erisim." +
                    s.sunucu->jeton() + "\r\n\r\n";
    HttpYanit y;
    WsCerceve m;
    return c.yaz(r) && c.yanit_bekle(y) && y.durum == 101 && c.ws_bekle(m) && m.opkod == 1;
}

const char* kHepsi =
    R"([{"kanal":"fizik/durum","hz":50},{"kanal":"kart/durum","hz":50},{"kanal":"atis/durum"},{"kanal":"emniyet/girdiler"},)"
    R"({"kanal":"ariza/etkin"},{"kanal":"sensor/basinc","hz":50},{"kanal":"sensor/dvl","hz":50},{"kanal":"sensor/batarya","hz":50},)"
    R"({"kanal":"sonar/ping360"},{"kanal":"sonar/ping1d","hz":50},{"kanal":"baglanti/durum"},{"kanal":"kart/cekirdek_sayaclari"}])";

HttpYanit http(const CanliSunucu& s, const std::string& yontem, const std::string& yol, const std::vector<std::string>& ek,
               const std::string& govde, double* sure = nullptr)
{
    SoketIstemci c(s.sunucu->port());
    std::string r = yontem + " " + yol + " HTTP/1.1\r\n" + s.host() + "\r\n" + s.yetki() + "\r\n";
    for (const auto& b : ek) {
        r += b + "\r\n";
    }
    if (yontem != "GET") {
        r += "Content-Length: " + std::to_string(govde.size()) + "\r\n";
    }
    r += "\r\n" + govde;
    const auto bas = Saat::now();
    HttpYanit y;
    c.yaz(r);
    c.yanit_bekle(y);
    if (sure != nullptr) {
        *sure = ms(Saat::now() - bas);
    }
    return y;
}

}  // namespace

int test_yavas_istemci()
{
    Denetci d;
    sahte::SahteAyar sa;
    CanliSunucu s(sa);
    if (!s.sunucu) {
        return 1;
    }
    // Çalıştırmayı başlat (Ping360 satırları yalnız kosuyor'da)
    const HttpYanit ist = http(s, "POST", "/api/v1/istemciler", {"Content-Type: application/json"}, "{}");
    const std::string istemci = std::to_string(static_cast<std::uint32_t>(ist.json_().bul("istemci_id")->sayi));
    const std::vector<std::string> kb = {"Content-Type: application/json", "Ida-Istemci: " + istemci};
    http(s, "POST", "/api/v1/calistirma/baslat", kb, R"({"istek_kimligi":"yavas-0001","beklenen_run_id":null,"beklenen_durum_rev":1})");
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    s.dunya->ek_ping360(10);  // ~2 000 satır/s ek yük (her satır ~1,3 kB)

    // 1) İstemcisiz: dünyanın yayın çağrısı süresi
    s.dunya->olcum_sifirla();
    std::this_thread::sleep_for(std::chrono::seconds(3));
    const std::uint64_t bos_en_kotu = s.dunya->en_kotu_yayin_ns();
    const std::uint64_t bos_sayi = s.dunya->yayin_sayisi();
    const double bos_ort = bos_sayi > 0 ? static_cast<double>(s.dunya->toplam_yayin_ns()) / static_cast<double>(bos_sayi) : 0.0;
    const std::uint64_t bos_uzun = s.dunya->uzun_yayin();

    // 2) Yavaş (hiç okumayan) ve hızlı istemci
    SoketIstemci yavas(s.sunucu->port());
    ag::alim_tamponu(yavas.tutamac(), 4096);
    DOGRULA(d, ws_baglan(yavas, s), "yavaş istemci bağlandı");
    yavas.yaz(ws_istemci_cercevesi(1, std::string("{\"tur\":\"abone_ol\",\"istek_no\":1,\"kanallar\":") + kHepsi + "}"));
    SoketIstemci hizli(s.sunucu->port());
    DOGRULA(d, ws_baglan(hizli, s), "hızlı istemci bağlandı");
    hizli.yaz(ws_istemci_cercevesi(1, std::string("{\"tur\":\"abone_ol\",\"istek_no\":1,\"kanallar\":") + kHepsi + "}"));
    s.dunya->olcum_sifirla();
    const auto bas = Saat::now();
    Saat::time_point son_fizik{};
    double en_buyuk_bosluk = 0.0;
    std::size_t fizik = 0;
    std::size_t ping = 0;
    double en_kotu_http = 0.0;
    double kisitlayan_ms = -1.0;
    std::uint32_t kisitlayan_no = 0;
    Saat::time_point kisitlayan_bas{};
    bool kisitlayan_gonderildi = false;
    auto sonraki_http = bas + std::chrono::milliseconds(500);
    while (Saat::now() - bas < std::chrono::seconds(10)) {
        hizli.oku(5);
        WsCerceve c;
        while (ws_sunucu_cercevesi(hizli.tampon, c)) {
            if (c.opkod == 9) {
                hizli.yaz(ws_istemci_cercevesi(10, c.yuk));
            }
            if (c.opkod != 2) {
                continue;
            }
            std::vector<A3Kare> k;
            a3_ileti_ayir(c.yuk, k);
            const auto simdi = Saat::now();
            for (const auto& x : k) {
                if (x.baslik.kanal_id == 10) {
                    if (fizik > 0 && simdi - bas > std::chrono::seconds(1)) {
                        en_buyuk_bosluk = std::max(en_buyuk_bosluk, ms(simdi - son_fizik));
                    }
                    son_fizik = simdi;
                    ++fizik;
                } else if (x.baslik.kanal_id == 30) {
                    ++ping;
                } else if (x.baslik.kanal_id == 3 && kisitlayan_no != 0 && kisitlayan_ms < 0.0) {
                    yuk_olay o;
                    if (coz(x.yuk.data(), x.yuk.size(), o) && o.tur == 5 && o.komut_no == kisitlayan_no) {
                        kisitlayan_ms = ms(simdi - kisitlayan_bas);
                    }
                }
            }
        }
        if (Saat::now() >= sonraki_http) {
            double sure = 0.0;
            const HttpYanit y = http(s, "GET", "/api/v1/durum", {}, {}, &sure);
            DOGRULA(d, y.durum == 200, "yavaş istemci varken HTTP yanıtı");
            en_kotu_http = std::max(en_kotu_http, sure);
            sonraki_http += std::chrono::milliseconds(500);
            if (!kisitlayan_gonderildi && Saat::now() - bas > std::chrono::seconds(5)) {
                // EMNİYET: kısıtlayan komut yavaş istemci varken alıcıya ulaşır ve uygulanır.
                const std::string run = y.json_().bul("run_id")->metin;
                kisitlayan_bas = Saat::now();
                const HttpYanit z = http(s, "POST", "/api/v1/emniyet/degistir", kb,
                                         R"({"istek_kimligi":"yavas-0002","beklenen_run_id":")" + run +
                                             R"(","girdi":"acil_durdurma","hedef_durum":"basili"})");
                DOGRULA(d, z.durum == 202, "kısıtlayan 202");
                kisitlayan_no = static_cast<std::uint32_t>(z.json_().bul("komut_no")->sayi);
                kisitlayan_gonderildi = true;
            }
        }
    }
    const std::uint64_t yuk_en_kotu = s.dunya->en_kotu_yayin_ns();
    const std::uint64_t yuk_sayi = s.dunya->yayin_sayisi();
    const double yuk_ort = yuk_sayi > 0 ? static_cast<double>(s.dunya->toplam_yayin_ns()) / static_cast<double>(yuk_sayi) : 0.0;
    const std::uint64_t yuk_uzun = s.dunya->uzun_yayin();
    const SunucuSayaclari say = s.sunucu->sayaclar();
    // 3) Yavaş istemci okumaya başlar: düşen Ping360 çerçevelerinden sonraki çerçevede bayrak (A3-K41)
    bool bayrak_goruldu = false;
    const auto okuma_bas = Saat::now();
    while (Saat::now() - okuma_bas < std::chrono::seconds(2) && !bayrak_goruldu) {
        yavas.oku(10);
        WsCerceve c;
        while (ws_sunucu_cercevesi(yavas.tampon, c)) {
            if (c.opkod != 2) {
                continue;
            }
            std::vector<A3Kare> k;
            a3_ileti_ayir(c.yuk, k);
            for (const auto& x : k) {
                if (x.baslik.kanal_id == 30 && (x.baslik.bayraklar & bayrak::oncesinde_dusme) != 0) {
                    bayrak_goruldu = true;
                }
            }
        }
    }
    std::printf("ÖLÇÜM: dünya yayın çağrısı, istemcisiz 3 s: en kötü %.1f us, ortalama %.2f us, %llu çağrı, 50 us üstü %llu\n",
                static_cast<double>(bos_en_kotu) / 1000.0, bos_ort / 1000.0, static_cast<unsigned long long>(bos_sayi),
                static_cast<unsigned long long>(bos_uzun));
    std::printf("ÖLÇÜM: dünya yayın çağrısı, yavaş + hızlı istemci 10 s: en kötü %.1f us, ortalama %.2f us, %llu çağrı, 50 us üstü %llu\n",
                static_cast<double>(yuk_en_kotu) / 1000.0, yuk_ort / 1000.0, static_cast<unsigned long long>(yuk_sayi),
                static_cast<unsigned long long>(yuk_uzun));
    std::printf("ÖLÇÜM: hızlı istemci: fizik/durum %zu çerçeve (50 Hz beklenir), en büyük boşluk %.1f ms; ping360 %zu satır\n",
                fizik, en_buyuk_bosluk, ping);
    std::printf("ÖLÇÜM: HTTP GET /durum en kötü %.1f ms; kısıtlayan komutun sonucu %.1f ms sonra\n", en_kotu_http, kisitlayan_ms);
    std::printf("ÖLÇÜM: düşen dizi %llu, ezilen durum %llu, en büyük bekleyen çıkış %llu B, kuyruk+çıkış tavanı %llu B, halka taşması %llu\n",
                static_cast<unsigned long long>(say.dusen_dizi), static_cast<unsigned long long>(say.ezilen_durum),
                static_cast<unsigned long long>(say.azami_cikis_bayt), static_cast<unsigned long long>(say.azami_bellek_tahmini),
                static_cast<unsigned long long>(s.sunucu->yayinci().halka_tasma()));
    DOGRULA(d, fizik >= 400, "hızlı istemci yavaş olanı beklemeden aldı");
    DOGRULA(d, say.dusen_dizi > 0, "yavaş istemcinin dizi kuyruğu düşürdü (A3-K41)");
    DOGRULA(d, bayrak_goruldu, "düşmeden sonraki çerçevede oncesinde_dusme");
    DOGRULA(d, kisitlayan_ms >= 0.0 && kisitlayan_ms < 1000.0, "kısıtlayan komut yavaş istemci varken uygulandı");
    DOGRULA(d, say.azami_cikis_bayt <= sinir::baski_esigi + sinir::ikili_ileti + 65536, "bağlantı çıkışı sınırlı");
    DOGRULA(d, s.sunucu->yayinci().halka_tasma() == 0, "dünya → A3 halkası taşmadı");
    return d.hata;
}

}  // namespace ida::a3::test
