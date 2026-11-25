// test_senaryo.cpp — senaryo ve ön denetim uçları (W5-a3-senaryo; A3 §3.6, §3.8, §4.9; A3-K76…K80). Soketsiz
// çekirdek + sahte dünya + bellekteki sahte `SenaryoHizmeti`. Yanıt gövdeleri dondurulmuş katalogdaki
// `$defs` şemalarına karşı doğrulanır. Kalıp dışı adların hizmete hiç ulaşmadığı (dosya sistemine
// dokunulmadığı) hizmetin çağrı sayaçlarıyla gösterilir.
#include "test_destek.hpp"

namespace ida::a3::test {

namespace {

struct Istemci {
    Ortam& o;
    std::uint32_t id;
    int n = 0;
    std::string kimlik() { return "kimlik-" + std::to_string(id) + "-" + std::to_string(++n); }
    HttpYanit post(const std::string& yol, const std::string& govde)
    {
        return o.http(istek("POST", yol, {"Content-Type: application/json", "Ida-Istemci: " + std::to_string(id)}, govde));
    }
    HttpYanit post_ham(const std::string& yol, const std::string& govde)
    {
        return o.http(istek("POST", yol, {"Content-Type: application/json"}, govde));
    }
};

constexpr const char* kOzet = "sha256:aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";

struct SahteHizmet final : SenaryoHizmeti {
    std::map<std::string, std::string> dosyalar{{"z1.toml", "[meta]\nad = \"z1\"\n"}, {"ornek.toml", "x = 1\n"}};
    int liste = 0, oku_n = 0, dogrula_n = 0, hazirla_n = 0, birak_n = 0, bekle_n = 0, coz_n = 0, od_n = 0;
    std::string son_birakilan;
    bool cozumlenmis_var = true;
    YuklemeSonucu bekle_sonucu{YuklemeSonucu::Tur::uygulandi, {}, {}};
    std::map<std::string, std::string> raporlar;

    std::vector<SenaryoDosyasi> listele() override
    {
        ++liste;
        std::vector<SenaryoDosyasi> v;
        for (const auto& [ad, ic] : dosyalar) {
            v.push_back({ad, ic.size(), "2026-11-25T12:00:00Z"});
        }
        v.push_back({"../kacak.toml", 1, ""});   // kalıp dışı: çekirdek listelemez
        v.push_back({"BUYUK.toml", 1, ""});
        return v;
    }
    DosyaDurumu oku(std::string_view ad, std::string& icerik, std::string& ileti) override
    {
        ++oku_n;
        if (ad == "bag.toml") {
            ileti = "simgesel bağ";
            return DosyaDurumu::gecersiz;
        }
        const auto it = dosyalar.find(std::string(ad));
        if (it == dosyalar.end()) {
            return DosyaDurumu::yok;
        }
        icerik = it->second;
        return DosyaDurumu::tamam;
    }
    SenaryoDogrulama dogrula(const SenaryoGirdisi& g) override
    {
        ++dogrula_n;
        SenaryoDogrulama d;
        std::string icerik = g.icerik;
        if (g.adli) {
            std::string ileti;
            d.dosya = oku(g.ad, icerik, ileti);
            --oku_n;
            d.ileti = ileti;
            if (d.dosya != DosyaDurumu::tamam) {
                return d;
            }
        }
        d.gecerli = icerik.find("BOZUK") == std::string::npos;
        d.rapor_json = d.gecerli ? "{\"gecerli\":true,\"bulgular\":[]}"
                                 : "{\"gecerli\":false,\"bulgular\":[{\"kural\":\"A5-K01\",\"onem\":\"hata\"}]}";
        if (d.gecerli) {
            d.ozet = kOzet;
        }
        return d;
    }
    SenaryoAdayi hazirla(const SenaryoGirdisi& g) override
    {
        ++hazirla_n;
        SenaryoAdayi a;
        std::string icerik = g.icerik;
        if (g.adli) {
            std::string ileti;
            const DosyaDurumu d = oku(g.ad, icerik, ileti);
            --oku_n;
            if (d == DosyaDurumu::yok) {
                a.sonuc = SenaryoAdayi::Sonuc::dosya_yok;
                return a;
            }
            if (d == DosyaDurumu::gecersiz) {
                a.sonuc = SenaryoAdayi::Sonuc::dosya_gecersiz;
                a.ileti = ileti;
                return a;
            }
        }
        if (icerik.find("BOZUK") != std::string::npos) {
            a.sonuc = SenaryoAdayi::Sonuc::gecersiz;
            a.rapor_json = "{\"gecerli\":false,\"bulgular\":[{\"kural\":\"A5-K01\",\"onem\":\"hata\"}]}";
            return a;
        }
        if (icerik.find("YURUTULEMEZ") != std::string::npos) {
            a.sonuc = SenaryoAdayi::Sonuc::yurutulemez;
            a.ileti = "senaryo bu sim'de yürütülemez: deneme";
            return a;
        }
        a.sonuc = SenaryoAdayi::Sonuc::hazir;
        a.aday_id = 7;
        a.ozet = kOzet;
        a.rapor_json = "{\"gecerli\":true,\"bulgular\":[]}";
        return a;
    }
    void aday_birak(std::uint64_t id) override
    {
        ++birak_n;
        son_birakilan = std::to_string(id);
    }
    YuklemeSonucu yukleme_bekle(std::uint64_t, std::chrono::milliseconds) override
    {
        ++bekle_n;
        return bekle_sonucu;
    }
    bool cozumlenmis(std::string& bayt, std::string& ozet) override
    {
        ++coz_n;
        if (!cozumlenmis_var) {
            return false;
        }
        bayt = "{\"a\":1}";
        ozet = kOzet;
        return true;
    }
    bool on_denetim(std::string_view kimlik, std::string& rapor) override
    {
        ++od_n;
        if (raporlar.empty()) {
            return false;
        }
        if (kimlik.empty()) {
            rapor = raporlar.rbegin()->second;
            return true;
        }
        const auto it = raporlar.find(std::string(kimlik));
        if (it == raporlar.end()) {
            return false;
        }
        rapor = it->second;
        return true;
    }
};

std::string yaz_ham_json(const std::string& anahtar, const std::string& deger)
{
    std::string r = "{\"" + anahtar + "\":";
    json::metin_yaz(deger, r);
    return r + "}";
}

}  // namespace

int test_senaryo_uclari()
{
    Denetci d;
    // ---- hizmet yok: bütün uçlar 404 ----
    {
        Ortam o;
        Istemci k{o, o.istemci_al()};
        DOGRULA(d, o.http(istek("GET", "/api/v1/senaryolar")).durum == 404, "hizmet yok: GET /senaryolar 404");
        DOGRULA(d, o.http(istek("GET", "/api/v1/senaryolar/z1.toml")).durum == 404, "hizmet yok: GET /senaryolar/{ad} 404");
        DOGRULA(d, k.post_ham("/api/v1/senaryo/dogrula", yaz_ham_json("ad", "z1.toml")).durum == 404, "hizmet yok: doğrula 404");
        DOGRULA(d, o.http(istek("GET", "/api/v1/senaryo/cozumlenmis")).durum == 404, "hizmet yok: çözümlenmiş 404");
        DOGRULA(d, o.http(istek("GET", "/api/v1/on_denetim")).durum == 404, "hizmet yok: ön denetim 404");
        const json::Deger g = o.http(istek("GET", "/api/v1/durum")).json_();
        const HttpYanit y = k.post("/api/v1/senaryo/yukle",
                                   "{\"istek_kimligi\":\"" + k.kimlik() + "\",\"beklenen_run_id\":null,\"beklenen_durum_rev\":" +
                                       std::to_string(static_cast<std::uint64_t>(g.bul("durum_rev")->sayi)) + ",\"ad\":\"z1.toml\"}");
        DOGRULA(d, y.durum == 404 && y.kod() == "bulunamadi", "hizmet yok: yükle 404 bulunamadi");
    }
    SahteHizmet h;
    SunucuAyari sa;
    sa.senaryo = &h;
    Ortam o(sahte::SahteAyar{}, sa);
    Istemci k{o, o.istemci_al()};
    std::string neden;

    // ---- GET /senaryolar ----
    {
        const HttpYanit y = o.http(istek("GET", "/api/v1/senaryolar"));
        const bool uyar = semaya_uyar("senaryo_listesi", y.govde, &neden);
        const json::Deger j = y.json_();
        std::vector<std::string> adlar;
        for (const auto& s : j.bul("senaryolar")->dizi) {
            adlar.push_back(s.bul("ad")->metin);
        }
        DOGRULA(d, y.durum == 200 && uyar && adlar == std::vector<std::string>{"ornek.toml", "z1.toml"},
                "GET /senaryolar: şemaya uyar, kalıp dışı adlar (hizmet verse bile) listelenmez " + neden);
    }
    // ---- GET /senaryolar/{ad} ----
    {
        const HttpYanit y = o.http(istek("GET", "/api/v1/senaryolar/z1.toml"));
        const bool uyar = semaya_uyar("yanit_senaryo_dosyasi", y.govde, &neden);
        const json::Deger j = y.json_();
        DOGRULA(d, y.durum == 200 && uyar && j.bul("icerik")->metin == h.dosyalar["z1.toml"], "GET /senaryolar/{ad}: içerik, şemaya uyar " + neden);
        DOGRULA(d, o.http(istek("GET", "/api/v1/senaryolar/yok.toml")).durum == 404, "olmayan dosya 404");
        const HttpYanit b = o.http(istek("GET", "/api/v1/senaryolar/bag.toml"));
        DOGRULA(d, b.durum == 400 && b.kod() == "gecersiz_istek", "hizmetin geçersiz saydığı dosya (simgesel bağ) 400");
        const int once = h.oku_n;
        int sayi400 = 0;
        const std::vector<std::string> kotu = {"..", "../z1.toml", "..%2Fz1.toml", "%2e%2e%2fz1.toml", "%2Fetc%2Fpasswd", "/etc/passwd", "a/b.toml",
                                               "Z1.toml", "z1.TOML", "z1.toml/", "z1.toml%2F", ".toml", "a%00.toml", "z%201.toml", "a.txt", "z1",
                                               std::string(65, 'a') + ".toml", "..%5Cz1.toml", "C%3A%5Cx.toml", "a\\b.toml"};
        std::string kacan;
        for (const std::string& ad : kotu) {
            const HttpYanit y2 = o.http(istek("GET", "/api/v1/senaryolar/" + ad));
            if (y2.durum == 400 && y2.kod() == "gecersiz_istek") {
                ++sayi400;
            } else {
                kacan += " " + ad + "→" + std::to_string(y2.durum);
            }
        }
        DOGRULA(d, sayi400 == static_cast<int>(kotu.size()), "geçersiz adların hepsi 400 gecersiz_istek (" + std::to_string(kotu.size()) + " ad)" + kacan);
        const int fark = h.oku_n - once;
        DOGRULA(d, fark == 0, "kalıp dışı adlar hizmete (dosya sistemine) hiç ulaşmadı (oku çağrısı +" + std::to_string(fark) + ")");
    }
    // ---- POST /senaryo/dogrula ----
    {
        const int once = h.dogrula_n;
        HttpYanit y = k.post_ham("/api/v1/senaryo/dogrula", yaz_ham_json("ad", "z1.toml"));
        DOGRULA(d, y.durum == 200 && semaya_uyar("yanit_senaryo_dogrula", y.govde, &neden) && y.json_().bul("gecerli")->mantik,
                "dogrula {ad}: geçerli, şemaya uyar " + neden);
        y = k.post_ham("/api/v1/senaryo/dogrula", yaz_ham_json("icerik", "BOZUK"));
        DOGRULA(d, y.durum == 200 && semaya_uyar("yanit_senaryo_dogrula", y.govde, &neden) && !y.json_().bul("gecerli")->mantik &&
                       y.json_().bul("senaryo_ozeti")->tur == json::Tur::bos,
                "dogrula {icerik}: geçersiz, özet null " + neden);
        DOGRULA(d, k.post_ham("/api/v1/senaryo/dogrula", "{\"ad\":\"../x.toml\"}").durum == 400, "dogrula: kalıp dışı ad 400 (şema)");
        DOGRULA(d, k.post_ham("/api/v1/senaryo/dogrula", "{\"ad\":\"z1.toml\",\"icerik\":\"x\"}").durum == 400, "dogrula: ad ve icerik birlikte 400");
        DOGRULA(d, k.post_ham("/api/v1/senaryo/dogrula", "{}").durum == 400, "dogrula: ikisi de yok 400");
        DOGRULA(d, k.post_ham("/api/v1/senaryo/dogrula", yaz_ham_json("ad", "yok.toml")).durum == 404, "dogrula: olmayan dosya 404");
        DOGRULA(d, h.dogrula_n - once == 3, "yalnız geçerli biçimli üç istek hizmete ulaştı (" + std::to_string(h.dogrula_n - once) + ")");
        // Doğrulama komut numarası almaz ve durum değiştirmez (A3-K76).
        const json::Deger g = o.http(istek("GET", "/api/v1/durum")).json_();
        DOGRULA(d, g.bul("son_komut_no")->sayi == 0 && g.bul("durum_rev")->sayi == 1, "doğrulama komut numarası ve durum_rev almadı");
    }
    // ---- GET /senaryo/cozumlenmis ----
    {
        const HttpYanit y = o.http(istek("GET", "/api/v1/senaryo/cozumlenmis"));
        const std::string* ozet = y.baslik("ida-davranis-ozeti");
        DOGRULA(d, y.durum == 200 && y.govde == "{\"a\":1}" && ozet != nullptr && *ozet == kOzet, "çözümlenmiş: baytlar aynen, özet başlıkta (A3-K80)");
        h.cozumlenmis_var = false;
        DOGRULA(d, o.http(istek("GET", "/api/v1/senaryo/cozumlenmis")).durum == 404, "yüklü senaryo yoksa 404");
        h.cozumlenmis_var = true;
    }
    // ---- GET /on_denetim ----
    {
        DOGRULA(d, o.http(istek("GET", "/api/v1/on_denetim")).durum == 404, "ön denetim: rapor yokken 404");
        const std::string rapor1 = "{\"rapor_surumu\":\"1.0\",\"rapor_kimligi\":\"0123456789abcdef\",\"sonuc\":\"gecti\",\"kip\":\"sil\",\"maddeler\":"
                                   "[{\"kod\":\"senaryo_gecerli\",\"durum\":\"gecti\",\"ileti\":\"tamam\"}]}";
        const std::string rapor2 = "{\"rapor_surumu\":\"1.0\",\"rapor_kimligi\":\"fedcba9876543210\",\"sonuc\":\"gecti\",\"kip\":\"sil\",\"maddeler\":[]}";
        h.raporlar["0123456789abcdef"] = rapor1;
        h.raporlar["fedcba9876543210"] = rapor2;
        const HttpYanit son = o.http(istek("GET", "/api/v1/on_denetim"));
        DOGRULA(d, son.durum == 200 && semaya_uyar("on_denetim_raporu", son.govde, &neden) && son.govde == rapor2, "GET /on_denetim: son rapor, şemaya uyar " + neden);
        const HttpYanit bir = o.http(istek("GET", "/api/v1/on_denetim/0123456789abcdef"));
        DOGRULA(d, bir.durum == 200 && bir.govde == rapor1, "GET /on_denetim/{kimlik}: o rapor");
        DOGRULA(d, o.http(istek("GET", "/api/v1/on_denetim/aaaaaaaaaaaaaaaa")).durum == 404, "bilinmeyen kimlik 404");
        const int once = h.od_n;
        int ret = 0;
        for (const char* kimlik : {"abc", "0123456789ABCDEF", "0123456789abcdeg", "0123456789abcdef0", ".."}) {
            ret += o.http(istek("GET", std::string("/api/v1/on_denetim/") + kimlik)).durum == 400 ? 1 : 0;
        }
        const int kacis = o.http(istek("GET", "/api/v1/on_denetim/%2e%2e")).durum;   // yol parçası kalıbına (harf, rakam, _-.) uymaz: uç yok
        DOGRULA(d, ret == 5 && kacis == 404 && h.od_n == once, "kalıp dışı kimlikler 400 (kaçışlı biçim: uç yok 404) ve hizmete ulaşmaz");
    }
    // ---- POST /senaryo/yukle ----
    {
        auto yukle = [&](const std::string& girdi, std::uint64_t rev) {
            const json::Deger g = o.http(istek("GET", "/api/v1/durum")).json_();
            return k.post("/api/v1/senaryo/yukle", "{\"istek_kimligi\":\"" + k.kimlik() + "\",\"beklenen_run_id\":null,\"beklenen_durum_rev\":" +
                                                       std::to_string(rev != 0 ? rev : static_cast<std::uint64_t>(g.bul("durum_rev")->sayi)) + "," + girdi + "}");
        };
        HttpYanit y = yukle("\"icerik\":\"BOZUK\"", 0);
        DOGRULA(d, y.durum == 422 && y.kod() == "senaryo_gecersiz" && y.json_().bul("hata")->bul("ayrinti")->bul("rapor") != nullptr && h.bekle_n == 0,
                "yükle: geçersiz senaryo 422 senaryo_gecersiz, ayrıntıda A5 raporu (A3-K77)");
        y = yukle("\"icerik\":\"YURUTULEMEZ\"", 0);
        DOGRULA(d, y.durum == 422 && y.kod() == "gecersiz_deger", "yükle: A5'in geçerli saydığı ama yürütülemeyen senaryo 422 gecersiz_deger");
        DOGRULA(d, yukle("\"ad\":\"yok.toml\"", 0).durum == 404, "yükle: olmayan dosya 404");
        DOGRULA(d, yukle("\"ad\":\"bag.toml\"", 0).durum == 400, "yükle: simgesel bağ 400");
        DOGRULA(d, yukle("\"ad\":\"../x.toml\"", 0).durum == 400, "yükle: kalıp dışı ad 400 (şema)");
        const int once_bekle = h.bekle_n;
        const int once_birak = h.birak_n;
        y = yukle("\"ad\":\"z1.toml\"", 99);
        DOGRULA(d, y.durum == 409 && y.kod() == "durum_rev_uyusmuyor" && h.bekle_n == once_bekle && h.birak_n == once_birak + 1 && h.son_birakilan == "7",
                "yükle: bayat durum_rev 409; aday hizmete bırakıldı (atıldı), dünya beklenmedi");
        // Başarı.
        y = yukle("\"ad\":\"z1.toml\"", 0);
        const json::Deger j = y.json_();
        DOGRULA(d, y.durum == 200 && semaya_uyar("yanit_senaryo_yukle", y.govde, &neden) && j.bul("senaryo_ozeti")->metin == kOzet &&
                       j.bul("komut_no")->sayi >= 1 && h.bekle_n == once_bekle + 1,
                "yükle: 200, yanıt şemaya uyar, dünya sonucu beklendi " + neden);
        // Dünya reddetti.
        h.bekle_sonucu = YuklemeSonucu{YuklemeSonucu::Tur::reddedildi, "gecersiz_deger", "yeni dünya kurulamadı"};
        y = yukle("\"ad\":\"z1.toml\"", 0);
        DOGRULA(d, y.durum == 422 && y.kod() == "gecersiz_deger", "yükle: yeni dünya kurulamadı → 422 gecersiz_deger");
        h.bekle_sonucu = YuklemeSonucu{YuklemeSonucu::Tur::zaman_asimi, {}, {}};
        y = yukle("\"ad\":\"z1.toml\"", 0);
        DOGRULA(d, y.durum == 503 && y.kod() == "mesgul", "yükle: süre doldu 503 mesgul (nihai sonuç olayla)");
    }
    return d.hata;
}

}  // namespace ida::a3::test
