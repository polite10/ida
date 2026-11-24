// test_komut.cpp — komut yolu (A3-K50…K60), çalıştırma durumu (A3-K61…K67) ve EMNİYET işaretli
// sanal emniyet girdileri ile yer istasyonu komutlarının taşınması (A3-K89…K104, K117…K119;
// W4-a3 kabul 5). Soketsiz çekirdek + sahte dünya (elle adım).
#include "test_destek.hpp"

namespace ida::a3::test {

namespace {

struct Istemci {
    Ortam& o;
    std::uint32_t id;
    int n = 0;
    std::string kimlik() { return "kimlik-" + std::to_string(id) + "-" + std::to_string(++n); }
    std::vector<std::string> basliklar() const
    {
        return {"Content-Type: application/json", "Ida-Istemci: " + std::to_string(id)};
    }
    HttpYanit post(const std::string& yol, const std::string& govde)
    {
        return o.http(istek("POST", yol, basliklar(), govde));
    }
};

json::Deger durum(Ortam& o) { return o.http(istek("GET", "/api/v1/durum")).json_(); }

std::string rid(const json::Deger& d)
{
    const json::Deger* r = d.bul("run_id");
    return r->tur == json::Tur::bos ? std::string("null") : "\"" + r->metin + "\"";
}

std::uint64_t durum_rev(Ortam& o) { return static_cast<std::uint64_t>(durum(o).bul("durum_rev")->sayi); }
std::uint32_t emniyet_rev(Ortam& o) { return static_cast<std::uint32_t>(durum(o).bul("emniyet_rev")->sayi); }

std::uint32_t komut_no(const HttpYanit& y)
{
    const json::Deger j = y.json_();  // bul() işaretçisi bu nesneye bağlı
    const json::Deger* n = j.bul("komut_no");
    return n != nullptr && n->tur == json::Tur::sayi ? static_cast<std::uint32_t>(n->sayi) : 0;
}

// Çalıştırmayı başlatır ve kosuyor'a getirir; run_id'yi döndürür.
std::string baslat(Istemci& k)
{
    const json::Deger d = durum(k.o);
    const HttpYanit y = k.post("/api/v1/calistirma/baslat", "{\"istek_kimligi\":\"" + k.kimlik() + "\",\"beklenen_run_id\":" + rid(d) +
                                                                ",\"beklenen_durum_rev\":" + std::to_string(static_cast<std::uint64_t>(d.bul("durum_rev")->sayi)) + "}");
    k.o.dunya.adim();
    k.o.dunya.adim();
    k.o.dongu();
    (void)y;
    return durum(k.o).bul("run_id")->metin;
}

// Bu komutun `komut_sonucu` olayının ayrıntısı (yoksa boş nesne).
json::Deger sonuc(Ortam& o, std::uint32_t no)
{
    o.dongu();
    const json::Deger j = o.http(istek("GET", "/api/v1/olaylar?sonra=0")).json_();
    for (const auto& e : j.bul("olaylar")->dizi) {
        if (e.bul("tur")->metin == "komut_sonucu" && e.bul("ayrinti")->bul("komut_no")->sayi == no) {
            return *e.bul("ayrinti");
        }
    }
    return json::Deger{};
}

std::string sonuc_metni(Ortam& o, std::uint32_t no)
{
    const json::Deger s = sonuc(o, no);
    if (!s.nesne_mi()) {
        return "yok";
    }
    std::string r = s.bul("sonuc")->metin;
    if (const json::Deger* h = s.bul("hata_kodu")) {
        r += "/" + h->metin;
    }
    return r;
}

std::string emniyet(Istemci& k, const std::string& girdi, const std::string& tup, const std::string& hedef,
                    const std::string& run, const std::string& ek = {})
{
    std::string g = "{\"istek_kimligi\":\"" + k.kimlik() + "\",\"beklenen_run_id\":\"" + run + "\",\"girdi\":\"" + girdi + "\"";
    if (!tup.empty()) {
        g += ",\"tup\":\"" + tup + "\"";
    }
    g += ",\"hedef_durum\":\"" + hedef + "\"" + ek + "}";
    return g;
}

std::string jeton_al(Istemci& k, const std::string& girdi, const std::string& tup, const std::string& hedef,
                     const std::string& run, HttpYanit* yanit = nullptr)
{
    std::string g = "{\"beklenen_run_id\":\"" + run + "\",\"girdi\":\"" + girdi + "\"";
    if (!tup.empty()) {
        g += ",\"tup\":\"" + tup + "\"";
    }
    g += ",\"hedef_durum\":\"" + hedef + "\"}";
    const HttpYanit y = k.post("/api/v1/emniyet/onay_jetonu", g);
    if (yanit != nullptr) {
        *yanit = y;
    }
    const json::Deger j = y.json_();
    const json::Deger* t = j.bul("onay_jetonu");
    return t != nullptr ? t->metin : std::string();
}

}  // namespace

int test_calistirma_akisi()
{
    Denetci d;
    Ortam o;
    Istemci k{o, o.istemci_al()};
    DOGRULA(d, k.id != 0, "istemci kaydı");
    const json::Deger d0 = durum(o);
    DOGRULA(d, d0.bul("durum")->metin == "yuklendi" && d0.bul("run_id")->tur == json::Tur::bos && d0.bul("durum_rev")->sayi == 1,
            "başlangıç durumu");
    HttpYanit y = k.post("/api/v1/calistirma/baslat", "{\"istek_kimligi\":\"" + k.kimlik() + "\",\"beklenen_run_id\":null,\"beklenen_durum_rev\":1}");
    const std::uint32_t no = komut_no(y);
    DOGRULA(d, y.durum == 202 && y.json_().bul("sonuc")->metin == "kuyrukta" && no >= 1, "baslat 202 kuyrukta (A3-K56)");
    HttpYanit kmt = o.http(istek("GET", "/api/v1/komutlar/" + std::to_string(no)));
    DOGRULA(d, kmt.json_().bul("durum")->metin == "kuyrukta", "komut kuyrukta");
    o.dunya.adim();
    o.dunya.adim();
    DOGRULA(d, sonuc_metni(o, no) == "uygulandi", "komut_sonucu uygulandi (A3-K57)");
    kmt = o.http(istek("GET", "/api/v1/komutlar/" + std::to_string(no)));
    DOGRULA(d, kmt.json_().bul("durum")->metin == "uygulandi" && kmt.json_().bul("uygulama_adimi") != nullptr, "GET /komutlar uygulandi");
    const json::Deger d1 = durum(o);
    DOGRULA(d, d1.bul("durum")->metin == "kosuyor" && d1.bul("run_id")->tur == json::Tur::metin, "kosuyor ve run_id");
    DOGRULA(d, d1.bul("durum_rev")->sayi == 3, "durum_rev: komut + iç geçiş (A3-K122)");
    const std::string run = d1.bul("run_id")->metin;
    // Bayat durum_rev: eşzamanlı 409 (A3-K123)
    y = k.post("/api/v1/calistirma/duraklat", "{\"istek_kimligi\":\"" + k.kimlik() + "\",\"beklenen_run_id\":\"" + run + "\",\"beklenen_durum_rev\":2}");
    DOGRULA(d, y.durum == 409 && y.kod() == "durum_rev_uyusmuyor" && komut_no(y) > no &&
                   y.json_().bul("hata")->bul("ayrinti")->bul("guncel_durum_rev")->sayi == 3,
            "bayat durum_rev 409 + güncel");
    // Yanlış run_id (A3-K54)
    y = k.post("/api/v1/calistirma/duraklat", "{\"istek_kimligi\":\"" + k.kimlik() + "\",\"beklenen_run_id\":null,\"beklenen_durum_rev\":3}");
    DOGRULA(d, y.durum == 409 && y.kod() == "beklenen_durum_uyusmuyor", "null run_id 409");
    // Geçersiz durum (Tablo 4.7a)
    y = k.post("/api/v1/calistirma/surdur", "{\"istek_kimligi\":\"" + k.kimlik() + "\",\"beklenen_run_id\":\"" + run + "\",\"beklenen_durum_rev\":3}");
    DOGRULA(d, y.durum == 409 && y.kod() == "gecersiz_durum", "kosuyor'da surdur 409");
    // hiz aralığı (A3-K66)
    y = k.post("/api/v1/calistirma/hiz", "{\"istek_kimligi\":\"" + k.kimlik() + "\",\"beklenen_run_id\":\"" + run + "\",\"beklenen_durum_rev\":3,\"carpan\":0.005}");
    DOGRULA(d, y.durum == 422 && y.kod() == "gecersiz_deger", "carpan 0,005 → 422");
    // Aynı durum_rev ile iki sifirla: yalnız biri uygulanır (A3-K123)
    Istemci k2{o, o.istemci_al()};
    const std::string g = "\",\"beklenen_run_id\":\"" + run + "\",\"beklenen_durum_rev\":3}";
    const HttpYanit a = k.post("/api/v1/calistirma/sifirla", "{\"istek_kimligi\":\"" + k.kimlik() + g);
    const HttpYanit b = k2.post("/api/v1/calistirma/sifirla", "{\"istek_kimligi\":\"" + k2.kimlik() + g);
    DOGRULA(d, a.durum == 202 && b.durum == 202, "ikisi de eşzamanlı denetimden geçer");
    o.dunya.adim();
    const std::string sa = sonuc_metni(o, komut_no(a));
    const std::string sb = sonuc_metni(o, komut_no(b));
    DOGRULA(d, sa == "uygulandi" && sb == "reddedildi/durum_rev_uyusmuyor", "yalnız ilki uygulanır: " + sa + " " + sb);
    const json::Deger d2 = durum(o);
    DOGRULA(d, d2.bul("durum")->metin == "yuklendi" && d2.bul("run_id")->tur == json::Tur::bos, "sifirla → yuklendi, run_id null");
    // yuklendi'de null run_id ile sifirla kabul (A3-K124)
    y = k.post("/api/v1/calistirma/sifirla", "{\"istek_kimligi\":\"" + k.kimlik() + "\",\"beklenen_run_id\":null,\"beklenen_durum_rev\":" +
                                                 std::to_string(durum_rev(o)) + "}");
    DOGRULA(d, y.durum == 202, "yuklendi'de null ile sifirla");
    // Başlık ve gövde hataları komut numarası alır (A3-K52)
    y = o.http(istek("POST", "/api/v1/calistirma/durdur", {"Content-Type: application/json"}, "{}"));
    DOGRULA(d, y.durum == 400 && y.kod() == "gecersiz_istek" && komut_no(y) > 0, "Ida-Istemci yok 400 + komut_no");
    y = o.http(istek("POST", "/api/v1/calistirma/durdur", {"Content-Type: application/json", "Ida-Istemci: 999"}, "{}"));
    DOGRULA(d, y.durum == 400 && komut_no(y) > 0, "kayıtsız istemci 400");
    y = o.http(istek("POST", "/api/v1/calistirma/durdur", {"Content-Type: text/plain", "Ida-Istemci: " + std::to_string(k.id)}, "{}"));
    DOGRULA(d, y.durum == 415 && y.kod() == "icerik_turu", "415");
    y = k.post("/api/v1/calistirma/durdur", "{bozuk");
    DOGRULA(d, y.durum == 400, "bozuk JSON 400");
    y = k.post("/api/v1/calistirma/durdur", "{\"istek_kimligi\":\"" + std::string(65, 'a') + "\",\"beklenen_run_id\":null,\"beklenen_durum_rev\":1}");
    DOGRULA(d, y.durum == 400, "65 karakterlik istek_kimligi şema dışı");
    const std::uint32_t son = komut_no(y);
    kmt = o.http(istek("GET", "/api/v1/komutlar/" + std::to_string(son)));
    DOGRULA(d, kmt.json_().bul("durum")->metin == "reddedildi" && kmt.json_().bul("hata")->bul("kod")->metin == "gecersiz_istek",
            "reddedilen komut kaydı");
    DOGRULA(d, o.http(istek("GET", "/api/v1/komutlar/999999")).durum == 404, "bilinmeyen komut 404 (A3-K60)");
    // Bu dilimde uygulanmayan durum değiştiren uç: komut numarası + 404
    y = k.post("/api/v1/senaryo/yukle", "{}");
    DOGRULA(d, y.durum == 404 && komut_no(y) > 0, "uygulanmayan uç 404 + komut_no");
    return d.hata;
}

int test_yineleme()
{
    Denetci d;
    Ortam o;
    Istemci k{o, o.istemci_al()};
    const std::string govde = R"({"istek_kimligi":"tekrar-0001","beklenen_run_id":null,"beklenen_durum_rev":1})";
    const HttpYanit a = k.post("/api/v1/calistirma/baslat", govde);
    const HttpYanit b = k.post("/api/v1/calistirma/baslat", govde);
    DOGRULA(d, a.durum == 202 && b.durum == 202 && komut_no(a) == komut_no(b), "yineleme aynı komut_no (A3-K53)");
    DOGRULA(d, b.json_().bul("yineleme") != nullptr && b.json_().bul("yineleme")->mantik, "yineleme: true");
    DOGRULA(d, o.dunya.alinanlar().size() == 1, "yineleme yeniden uygulanmaz");
    const HttpYanit c = k.post("/api/v1/calistirma/baslat", R"({"istek_kimligi":"tekrar-0001","beklenen_run_id":null,"beklenen_durum_rev":2})");
    DOGRULA(d, c.durum == 409 && c.kod() == "kimlik_cakismasi", "aynı kimlik başka gövde 409");
    const HttpYanit e = k.post("/api/v1/calistirma/durdur", govde);
    DOGRULA(d, e.durum == 409 && e.kod() == "kimlik_cakismasi", "aynı kimlik başka yol 409");
    Istemci k2{o, o.istemci_al()};
    const HttpYanit f = k2.post("/api/v1/calistirma/baslat", govde);
    DOGRULA(d, f.durum == 202 && komut_no(f) != komut_no(a) && f.json_().bul("yineleme") == nullptr,
            "başka istemcinin aynı kimliği çakışmaz");
    // Ret yanıtı da saklanır
    const HttpYanit r1 = k.post("/api/v1/calistirma/surdur", R"({"istek_kimligi":"tekrar-0002","beklenen_run_id":null,"beklenen_durum_rev":1})");
    const HttpYanit r2 = k.post("/api/v1/calistirma/surdur", R"({"istek_kimligi":"tekrar-0002","beklenen_run_id":null,"beklenen_durum_rev":1})");
    DOGRULA(d, r1.durum == r2.durum && r1.durum >= 400 && komut_no(r1) == komut_no(r2) && r2.json_().bul("yineleme")->mantik,
            "ret de yinelenir");
    // 10 dakika sonra kayıt silinir
    o.ilerle(std::chrono::milliseconds(601000));
    const HttpYanit g = k.post("/api/v1/calistirma/surdur", R"({"istek_kimligi":"tekrar-0002","beklenen_run_id":null,"beklenen_durum_rev":1})");
    DOGRULA(d, komut_no(g) != komut_no(r1) && g.json_().bul("yineleme") == nullptr, "süre dolunca yeni istek");
    // İstemci oturumu kapanınca
    DOGRULA(d, o.http(istek("DELETE", "/api/v1/istemciler/" + std::to_string(k2.id))).durum == 200, "istemci silindi");
    DOGRULA(d, o.http(istek("DELETE", "/api/v1/istemciler/" + std::to_string(k2.id))).durum == 404, "ikinci silme 404");
    const HttpYanit h = k2.post("/api/v1/calistirma/baslat", govde);
    DOGRULA(d, h.durum == 400, "silinmiş istemci 400");
    return d.hata;
}

int test_govde_sinirlari()
{
    Denetci d;
    Ortam o;
    Istemci k{o, o.istemci_al()};
    // Content-Length sınırı aşıyor: en çok 4096 bayt okunur, 413, bağlantı kapanır (A3-K51)
    const BagId id = o.ac();
    std::string bas = istek("POST", "/api/v1/calistirma/durdur", k.basliklar(), {});
    bas.replace(bas.find("Content-Length: 0"), 17, "Content-Length: 2097152");
    o.gonder(id, bas + std::string(10000, ' '));
    o.dongu();
    std::string t = o.al(id);
    HttpYanit y;
    DOGRULA(d, http_yanit_ayir(t, y) && y.durum == 413 && y.kod() == "govde_cok_buyuk" && komut_no(y) > 0, "413 + komut_no");
    DOGRULA(d, o.cekirdek.okunan_govde_bayti(id) <= 4096, "en çok 4096 bayt okundu: " + std::to_string(o.cekirdek.okunan_govde_bayti(id)));
    DOGRULA(d, !o.cekirdek.okumali(id) && o.cekirdek.kapatilmali(id), "bağlantı kapanır, okunmaz");
    // chunked: sınıra kadar okunur
    const BagId id2 = o.ac();
    std::string p = "POST /api/v1/calistirma/durdur HTTP/1.1\r\n" + host() + "\r\n" + yetki() +
                    "\r\nContent-Type: application/json\r\nIda-Istemci: " + std::to_string(k.id) + "\r\nTransfer-Encoding: chunked\r\n\r\n";
    for (int i = 0; i < 20; ++i) {
        p += "10000\r\n" + std::string(65536, ' ') + "\r\n";
    }
    o.gonder(id2, p);
    o.dongu();
    t = o.al(id2);
    DOGRULA(d, http_yanit_ayir(t, y) && y.durum == 413, "chunked 1 MiB üstü 413");
    DOGRULA(d, o.cekirdek.okunan_govde_bayti(id2) <= (std::size_t{1} << 20u) + 4096, "chunked sınıra kadar");
    // Tam 1 MiB gövde (boşlukla doldurulmuş geçerli JSON) kabul
    std::string govde = "{\"istek_kimligi\":\"" + k.kimlik() + "\",\"beklenen_run_id\":null,\"beklenen_durum_rev\":1}";
    govde += std::string((std::size_t{1} << 20u) - govde.size(), ' ');
    y = k.post("/api/v1/calistirma/baslat", govde);
    DOGRULA(d, y.durum == 202, "1 MiB gövde kabul: " + std::to_string(y.durum));
    return d.hata;
}

// EMNİYET — A3-K92, K119: kısıtlayan tek adımlıdır; onay, jeton ve revizyon istemez, bayat revizyon
// ya da geçersiz jeton yüzünden reddedilmez.
int test_emniyet_kisitlayan()
{
    Denetci d;
    Ortam o;
    Istemci k{o, o.istemci_al()};
    const std::string run = baslat(k);
    const std::uint32_t rev0 = emniyet_rev(o);
    HttpYanit a = k.post("/api/v1/emniyet/degistir", emniyet(k, "acil_durdurma", "", "basili", run));
    HttpYanit b = k.post("/api/v1/emniyet/degistir",
                         emniyet(k, "silahlandirma_fisi", "f2", "cikarilmis", run, ",\"beklenen_emniyet_rev\":1,\"onay\":false"));
    HttpYanit c = k.post("/api/v1/emniyet/degistir",
                         emniyet(k, "emniyet_pimi", "f1", "takili", run, ",\"onay_jetonu\":\"00000000000000000000000000000000\""));
    DOGRULA(d, a.durum == 202 && b.durum == 202 && c.durum == 202, "üç kısıtlayan kabul (bayat revizyon, geçersiz jeton yok sayılır)");
    o.dunya.adim();
    DOGRULA(d, sonuc_metni(o, komut_no(a)) == "uygulandi" && sonuc_metni(o, komut_no(b)) == "uygulandi" &&
                   sonuc_metni(o, komut_no(c)) == "uygulandi",
            "hepsi uygulandı");
    DOGRULA(d, emniyet_rev(o) == rev0 + 3, "her kısıtlama revizyonu bir artırır (durum değişmese de; A3-K92, K117)");
    const json::Deger e = o.http(istek("GET", "/api/v1/emniyet")).json_();
    DOGRULA(d, e.bul("yetkili")->bul("acil_durdurma")->metin == "basili", "acil durdurma basılı");
    const auto alinan = o.dunya.alinanlar();
    bool kisitlayan_isaretli = !alinan.empty();
    for (const auto& x : alinan) {
        if (x.tur == KomutTuru::emniyet_degistir && !x.kisitlayan) {
            kisitlayan_isaretli = false;
        }
        if (x.tur == KomutTuru::emniyet_degistir && x.beklenen_emniyet_rev.has_value()) {
            kisitlayan_isaretli = false;  // A3-K119: revizyon taşınmaz
        }
    }
    DOGRULA(d, kisitlayan_isaretli, "alıcıya kısıtlayan olarak, revizyonsuz iletildi");
    // Biçim: tüp kuralı (A3-K89)
    HttpYanit x = k.post("/api/v1/emniyet/degistir", emniyet(k, "acil_durdurma", "f1", "basili", run));
    DOGRULA(d, x.durum == 400, "acil durdurmada tup 400");
    x = k.post("/api/v1/emniyet/degistir", emniyet(k, "emniyet_pimi", "", "takili", run));
    DOGRULA(d, x.durum == 400, "pimde tupsuz 400");
    x = k.post("/api/v1/emniyet/degistir", emniyet(k, "acil_durdurma", "", "takili", run));
    DOGRULA(d, x.durum == 422 && x.kod() == "gecersiz_deger", "acil durdurmaya takili 422");
    return d.hata;
}

// EMNİYET — A3-K93, K94, K118: gevşeten iki adımlıdır, revizyonsuz kabul edilmez.
int test_emniyet_gevseten()
{
    Denetci d;
    Ortam o;
    Istemci k{o, o.istemci_al()};
    // Çalıştırma yokken jeton yok
    HttpYanit jy;
    jeton_al(k, "emniyet_pimi", "f1", "cikarilmis", "20261123-000000-000000", &jy);
    DOGRULA(d, jy.durum == 409 && jy.kod() == "canli_calistirma_yok", "çalıştırma yokken jeton 409");
    const std::string run = baslat(k);
    std::uint32_t rev = emniyet_rev(o);
    const std::string rv = ",\"beklenen_emniyet_rev\":" + std::to_string(rev);
    HttpYanit y = k.post("/api/v1/emniyet/degistir", emniyet(k, "emniyet_pimi", "f1", "cikarilmis", run));
    DOGRULA(d, y.durum == 409 && y.kod() == "onay_gerekli", "onaysız gevşetme 409 onay_gerekli");
    y = k.post("/api/v1/emniyet/degistir", emniyet(k, "emniyet_pimi", "f1", "cikarilmis", run, ",\"onay\":true" + rv));
    DOGRULA(d, y.durum == 409 && y.kod() == "onay_gerekli", "jetonsuz 409 onay_gerekli");
    std::string t = jeton_al(k, "emniyet_pimi", "f1", "cikarilmis", run, &jy);
    DOGRULA(d, t.size() == 32 && jy.json_().bul("gecerlilik_ms")->sayi == 10000 && jy.json_().bul("emniyet_rev")->sayi == rev,
            "jeton (A3-K93)");
    y = k.post("/api/v1/emniyet/degistir", emniyet(k, "emniyet_pimi", "f1", "cikarilmis", run, ",\"onay\":true,\"onay_jetonu\":\"" + t + "\""));
    DOGRULA(d, y.durum == 400 && y.kod() == "gecersiz_istek", "revizyonsuz gevşetme 400 (A3-K93)");
    y = k.post("/api/v1/emniyet/degistir", emniyet(k, "emniyet_pimi", "f1", "cikarilmis", run, ",\"onay\":true,\"onay_jetonu\":\"" + t + "\"" + rv));
    DOGRULA(d, y.durum == 409 && y.kod() == "onay_jetonu_gecersiz", "jeton ilk kullanımda (retle) tüketildi");
    t = jeton_al(k, "emniyet_pimi", "f1", "cikarilmis", run);
    y = k.post("/api/v1/emniyet/degistir",
               emniyet(k, "emniyet_pimi", "f1", "cikarilmis", run, ",\"onay\":true,\"onay_jetonu\":\"" + t + "\",\"beklenen_emniyet_rev\":" + std::to_string(rev + 5)));
    DOGRULA(d, y.durum == 409 && y.kod() == "emniyet_rev_uyusmuyor" &&
                   y.json_().bul("hata")->bul("ayrinti")->bul("guncel_emniyet_rev")->sayi == rev,
            "bayat revizyon 409 + güncel (A3-K118)");
    t = jeton_al(k, "emniyet_pimi", "f1", "cikarilmis", run);
    y = k.post("/api/v1/emniyet/degistir", emniyet(k, "emniyet_pimi", "f1", "cikarilmis", run, ",\"onay\":true,\"onay_jetonu\":\"" + t + "\"" + rv));
    DOGRULA(d, y.durum == 202, "geçerli gevşetme 202");
    o.dunya.adim();
    DOGRULA(d, sonuc_metni(o, komut_no(y)) == "uygulandi", "gevşetme uygulandı");
    DOGRULA(d, emniyet_rev(o) == rev + 1, "gevşetme revizyonu artırır");
    y = k.post("/api/v1/emniyet/degistir", emniyet(k, "emniyet_pimi", "f1", "cikarilmis", run, ",\"onay\":true,\"onay_jetonu\":\"" + t + "\"" + rv));
    DOGRULA(d, y.durum == 409 && y.kod() == "onay_jetonu_gecersiz", "jeton ikinci kez kullanılamaz");
    rev = emniyet_rev(o);
    const std::string rv2 = ",\"beklenen_emniyet_rev\":" + std::to_string(rev);
    // Jeton bağları (A3-K94): başka tüp, başka istemci, süre, revizyon değişimi
    t = jeton_al(k, "emniyet_pimi", "f1", "cikarilmis", run);
    y = k.post("/api/v1/emniyet/degistir", emniyet(k, "emniyet_pimi", "f2", "cikarilmis", run, ",\"onay\":true,\"onay_jetonu\":\"" + t + "\"" + rv2));
    DOGRULA(d, y.kod() == "onay_jetonu_gecersiz", "f1 jetonu f2'de geçersiz");
    Istemci k2{o, o.istemci_al()};
    t = jeton_al(k, "emniyet_pimi", "f2", "cikarilmis", run);
    y = k2.post("/api/v1/emniyet/degistir", emniyet(k2, "emniyet_pimi", "f2", "cikarilmis", run, ",\"onay\":true,\"onay_jetonu\":\"" + t + "\"" + rv2));
    DOGRULA(d, y.kod() == "onay_jetonu_gecersiz", "başka istemcinin jetonu geçersiz");
    t = jeton_al(k, "emniyet_pimi", "f2", "cikarilmis", run);
    o.ilerle(std::chrono::milliseconds(10100));
    y = k.post("/api/v1/emniyet/degistir", emniyet(k, "emniyet_pimi", "f2", "cikarilmis", run, ",\"onay\":true,\"onay_jetonu\":\"" + t + "\"" + rv2));
    DOGRULA(d, y.kod() == "onay_jetonu_gecersiz", "süresi geçmiş jeton (10 s)");
    // Ardışık sınır sırası (A3 §7 K89–K99 satırı): jeton rev=r; "pimi tak" uygulanır (r+1); eski çıkarma reddedilir.
    t = jeton_al(k, "emniyet_pimi", "f2", "cikarilmis", run);
    y = k.post("/api/v1/emniyet/degistir", emniyet(k, "emniyet_pimi", "f2", "takili", run));
    DOGRULA(d, y.durum == 202, "pimi tak (zaten takılı)");
    o.dunya.adim();
    y = k.post("/api/v1/emniyet/degistir", emniyet(k, "emniyet_pimi", "f2", "cikarilmis", run, ",\"onay\":true,\"onay_jetonu\":\"" + t + "\"" + rv2));
    DOGRULA(d, y.durum == 409 && (y.kod() == "onay_jetonu_gecersiz" || y.kod() == "emniyet_rev_uyusmuyor"),
            "kısıtlamadan sonra eski gevşetme reddedilir: " + y.kod());
    const json::Deger e = o.http(istek("GET", "/api/v1/emniyet")).json_();
    DOGRULA(d, e.bul("yetkili")->bul("emniyet_pimi")->bul("f2")->metin == "takili", "pim takılı kaldı");
    // Kısıtlayan için jeton istenmez
    HttpYanit j2;
    jeton_al(k, "acil_durdurma", "", "basili", run, &j2);
    DOGRULA(d, j2.durum == 422, "kısıtlayan değişikliğe jeton 422");
    return d.hata;
}

// EMNİYET — A3-K95: aynı sınırda kısıtlama ve gevşetme; sonuç alıcıdan aynen döner.
int test_sonuc_aynen()
{
    Denetci d;
    Ortam o;
    Istemci k{o, o.istemci_al()};
    const std::string run = baslat(k);
    const std::uint32_t rev = emniyet_rev(o);
    const std::string t = jeton_al(k, "emniyet_pimi", "f1", "cikarilmis", run);
    // Önce gevşeten, sonra kısıtlayan gelir; ikisi aynı adım sınırına düşer.
    const HttpYanit g = k.post("/api/v1/emniyet/degistir",
                               emniyet(k, "emniyet_pimi", "f1", "cikarilmis", run,
                                       ",\"onay\":true,\"onay_jetonu\":\"" + t + "\",\"beklenen_emniyet_rev\":" + std::to_string(rev)));
    const HttpYanit kz = k.post("/api/v1/emniyet/degistir", emniyet(k, "emniyet_pimi", "f1", "takili", run));
    DOGRULA(d, g.durum == 202 && kz.durum == 202, "ikisi de kuyrukta");
    o.dunya.adim();
    DOGRULA(d, sonuc_metni(o, komut_no(kz)) == "uygulandi", "kısıtlama uygulandı");
    const json::Deger s = sonuc(o, komut_no(g));
    DOGRULA(d, s.nesne_mi() && s.bul("sonuc")->metin == "reddedildi" && s.bul("hata_kodu")->metin == "kisitlama_hakim" &&
                   s.bul("sonuc_ayrinti")->bul("guncel_emniyet_rev")->sayi == rev + 1,
            "gevşetme kisitlama_hakim, ayrıntı aynen");
    const json::Deger km = o.http(istek("GET", "/api/v1/komutlar/" + std::to_string(komut_no(g)))).json_();
    DOGRULA(d, km.bul("durum")->metin == "reddedildi" && km.bul("hata")->bul("kod")->metin == "kisitlama_hakim",
            "GET /komutlar aynı sonuç");
    const json::Deger e = o.http(istek("GET", "/api/v1/emniyet")).json_();
    DOGRULA(d, e.bul("yetkili")->bul("emniyet_pimi")->bul("f1")->metin == "takili", "pim takılı");
    const auto iz = o.dunya.iz();
    std::size_t ki = 0;
    std::size_t gi = 0;
    for (std::size_t i = 0; i < iz.size(); ++i) {
        if (iz[i].komut_no == komut_no(kz)) ki = i;
        if (iz[i].komut_no == komut_no(g)) gi = i;
    }
    DOGRULA(d, ki < gi, "kısıtlayan, önce gelen gevşetenin arkasında beklemedi");
    return d.hata;
}

// EMNİYET — A3-K59: dünya kuyruğu doluyken kısıtlayan istek reddedilmez ve alıcıya ulaşır.
int test_kuyruk_dolu_kisitlayan()
{
    Denetci d;
    sahte::SahteAyar sa;
    sa.genel_kuyruk = 3;
    sa.kisitlayan_kuyruk = 2;
    Ortam o(sa);
    Istemci k{o, o.istemci_al()};
    const std::string run = baslat(k);
    o.dunya.dondur(true);
    const std::uint64_t dr = durum_rev(o);
    std::vector<std::uint32_t> genel;
    for (int i = 0; i < 3; ++i) {
        const HttpYanit y = k.post("/api/v1/calistirma/hiz", "{\"istek_kimligi\":\"" + k.kimlik() + "\",\"beklenen_run_id\":\"" + run +
                                                              "\",\"beklenen_durum_rev\":" + std::to_string(dr) + ",\"carpan\":1}");
        DOGRULA(d, y.durum == 202, "genel kuyruk dolana dek kabul");
        genel.push_back(komut_no(y));
    }
    HttpYanit y = k.post("/api/v1/calistirma/hiz", "{\"istek_kimligi\":\"" + k.kimlik() + "\",\"beklenen_run_id\":\"" + run +
                                                    "\",\"beklenen_durum_rev\":" + std::to_string(dr) + ",\"carpan\":1}");
    DOGRULA(d, y.durum == 503 && y.kod() == "mesgul", "dolu kuyrukta genel komut 503");
    std::vector<std::uint32_t> kisitlayan;
    for (const char* tup : {"f1", "f2", "f1", "f2", "f1"}) {
        const HttpYanit z = k.post("/api/v1/emniyet/degistir", emniyet(k, "silahlandirma_fisi", tup, "cikarilmis", run));
        DOGRULA(d, z.durum == 202, "kısıtlayan dolu kuyrukta kabul (ayrılmış yuva ve bekletme)");
        kisitlayan.push_back(komut_no(z));
    }
    // W4-K22 (a): alıcı doluyken etki başına mandal: f1 ve f2 fiş mandalları dolu; 5. istek (f1)
    // 3.'nün yerini aldı (3. birleşti).
    DOGRULA(d, o.cekirdek.sayaclar().bekleyen_kisitlayan == 2, "iki mandal dolu (fiş f1, f2)");
    // Bekleyen kısıtlayan varken gevşeten/genel komut alıcıya gitmez
    const std::string t = jeton_al(k, "silahlandirma_fisi", "f1", "takili", run);
    y = k.post("/api/v1/emniyet/degistir", emniyet(k, "silahlandirma_fisi", "f1", "takili", run,
                                                  ",\"onay\":true,\"onay_jetonu\":\"" + t + "\",\"beklenen_emniyet_rev\":" + std::to_string(emniyet_rev(o))));
    DOGRULA(d, y.durum == 503, "bekleyen kısıtlayan varken gevşeten 503");
    o.dunya.dondur(false);
    for (int i = 0; i < 4; ++i) {
        o.dunya.adim();
        o.dongu();
    }
    DOGRULA(d, o.cekirdek.sayaclar().bekleyen_kisitlayan == 0, "bekleyenler iletildi");
    std::size_t uygulanan = 0;
    for (const auto no : kisitlayan) {
        uygulanan += sonuc_metni(o, no) == "uygulandi" ? 1u : 0u;
    }
    DOGRULA(d, uygulanan == 5, "beş kısıtlayanın hepsi uygulandı: " + std::to_string(uygulanan));
    // Alıcıya komut_no sırasıyla ulaştı
    std::vector<std::uint32_t> sira;
    for (const auto& x : o.dunya.alinanlar()) {
        if (x.tur == KomutTuru::emniyet_degistir && x.kisitlayan) {
            sira.push_back(x.komut_no);
        }
    }
    const std::vector<std::uint32_t> beklenen_sira = {kisitlayan[0], kisitlayan[1], kisitlayan[3], kisitlayan[4]};
    DOGRULA(d, sira == beklenen_sira, "kısıtlayanlar komut_no sırasıyla iletildi; birleşen (3.) iletilmedi");
    // Yineleme kaydı sınırı kısıtlayanı reddetmez (A3-K53) — ayrı ortamda sınır doldurulur
    return d.hata;
}

// EMNİYET — W4-K19: kısıtlayan istek bayat ya da eksik `beklenen_run_id` ile de alıcıya ulaşır ve
// uygulanır; yanıt güncel run_id'yi taşır; uyuşmazlık olayla kaydedilir. Gevşetende A3-K54 aynen.
int test_kisitlayan_bayat_run_id()
{
    Denetci d;
    const std::string bayat = "20000101-000000-000000";
    Ortam o;
    Istemci k{o, o.istemci_al()};
    const std::string run = baslat(k);
    auto guncel = [](const HttpYanit& y) {
        const json::Deger j = y.json_();
        const json::Deger* g = j.bul("guncel_run_id");
        return g == nullptr ? std::string("<yok>") : g->tur == json::Tur::bos ? std::string("null") : g->metin;
    };
    auto uyusmazlik_olaylari = [&]() {
        std::vector<json::Deger> r;
        o.dongu();
        const json::Deger j = o.http(istek("GET", "/api/v1/olaylar?sonra=0")).json_();
        for (const auto& e : j.bul("olaylar")->dizi) {
            if (e.bul("tur")->metin == "uyari" && e.bul("ayrinti")->bul("kod")->metin == "kisitlayan_run_id_uyusmazligi") {
                r.push_back(e);
            }
        }
        return r;
    };
    // 1) Bayat run_id ile acil durdurma
    const std::string kimlik = k.kimlik();
    HttpYanit a = k.post("/api/v1/emniyet/degistir", "{\"istek_kimligi\":\"" + kimlik + "\",\"beklenen_run_id\":\"" + bayat +
                                                        "\",\"girdi\":\"acil_durdurma\",\"hedef_durum\":\"basili\"}");
    DOGRULA(d, a.durum == 202 && guncel(a) == run, "bayat run_id'li acil durdurma 202 + guncel_run_id: " + std::to_string(a.durum));
    o.dunya.adim();
    DOGRULA(d, sonuc_metni(o, komut_no(a)) == "uygulandi", "bayat run_id'li acil durdurma uygulandı");
    DOGRULA(d, o.http(istek("GET", "/api/v1/emniyet")).json_().bul("yetkili")->bul("acil_durdurma")->metin == "basili", "basılı");
    auto ol = uyusmazlik_olaylari();
    DOGRULA(d, ol.size() == 1 && ol[0].bul("komut_no")->sayi == komut_no(a) && ol[0].bul("kaynak")->metin == "a3",
            "uyuşmazlık olayı (komut numarasıyla)");
    if (ol.size() == 1) {
        const std::string ileti = ol[0].bul("ayrinti")->bul("ileti")->metin;
        DOGRULA(d, ileti.find(bayat) != std::string::npos && ileti.find(run) != std::string::npos, "olayda iki run_id");
        DOGRULA(d, ileti.find(kimlik) == std::string::npos && ileti.find(kJeton) == std::string::npos, "olayda gövde ve jeton yok");
    }
    // 2) Alan hiç yok; 3) null
    HttpYanit b = k.post("/api/v1/emniyet/degistir", "{\"istek_kimligi\":\"" + k.kimlik() +
                                                        "\",\"girdi\":\"emniyet_pimi\",\"tup\":\"f1\",\"hedef_durum\":\"takili\"}");
    HttpYanit c = k.post("/api/v1/emniyet/degistir", "{\"istek_kimligi\":\"" + k.kimlik() +
                                                        "\",\"beklenen_run_id\":null,\"girdi\":\"silahlandirma_fisi\",\"tup\":\"f2\",\"hedef_durum\":\"cikarilmis\"}");
    DOGRULA(d, b.durum == 202 && guncel(b) == run, "run_id alanı olmayan kısıtlayan 202");
    DOGRULA(d, c.durum == 202 && guncel(c) == run, "null run_id'li kısıtlayan 202");
    // 4) Daraltıcı yer istasyonu komutu
    HttpYanit e = k.post("/api/v1/yer_istasyonu/komut", "{\"istek_kimligi\":\"" + k.kimlik() + "\",\"beklenen_run_id\":\"" + bayat +
                                                           "\",\"komut\":\"fc_disarm\",\"parametreler\":{}}");
    DOGRULA(d, e.durum == 202 && guncel(e) == run, "bayat run_id'li daraltıcı 202");
    // 5) Uyuşan run_id: olay yok, alan yok
    HttpYanit f = k.post("/api/v1/emniyet/degistir", emniyet(k, "acil_durdurma", "", "basili", run));
    DOGRULA(d, f.durum == 202 && guncel(f) == "<yok>", "uyuşan run_id'de guncel_run_id yok");
    o.dunya.adim();
    for (const auto* y : {&b, &c, &e, &f}) {
        DOGRULA(d, sonuc_metni(o, komut_no(*y)) == "uygulandi", "kısıtlayan uygulandı: " + std::to_string(komut_no(*y)));
    }
    DOGRULA(d, uyusmazlik_olaylari().size() == 4, "dört uyuşmazlık olayı");
    bool isaret = false;
    for (const auto& x : o.dunya.alinanlar()) {
        if (x.komut_no == komut_no(a)) {
            isaret = x.run_id_uyusmazligi && x.kisitlayan && x.beklenen_run_id == bayat;
        }
    }
    DOGRULA(d, isaret, "alıcıya uyuşmazlık işaretiyle, özgün beklenen_run_id ile iletildi");
    // 6) Gevşeten: A3-K54 aynen
    const std::uint32_t rev = emniyet_rev(o);
    const std::string t = jeton_al(k, "emniyet_pimi", "f1", "cikarilmis", run);
    HttpYanit g = k.post("/api/v1/emniyet/degistir",
                         emniyet(k, "emniyet_pimi", "f1", "cikarilmis", bayat,
                                 ",\"onay\":true,\"onay_jetonu\":\"" + t + "\",\"beklenen_emniyet_rev\":" + std::to_string(rev)));
    DOGRULA(d, g.durum == 409 && g.kod() == "beklenen_durum_uyusmuyor", "bayat run_id'li gevşetme 409: " + g.kod());
    HttpYanit h = k.post("/api/v1/emniyet/degistir", "{\"istek_kimligi\":\"" + k.kimlik() +
                                                        "\",\"girdi\":\"emniyet_pimi\",\"tup\":\"f1\",\"hedef_durum\":\"cikarilmis\"}");
    DOGRULA(d, h.durum == 400 && h.kod() == "gecersiz_istek", "run_id alanı olmayan gevşetme 400");
    HttpYanit i = k.post("/api/v1/yer_istasyonu/komut", "{\"istek_kimligi\":\"" + k.kimlik() + "\",\"komut\":\"fc_fire\",\"parametreler\":{\"tube\":0}}");
    DOGRULA(d, i.durum == 400, "run_id alanı olmayan genişletici 400");
    // 7) Kuyruk doluyken bayat run_id'li kısıtlayan (bekletilir, sonra uygulanır)
    sahte::SahteAyar sa;
    sa.genel_kuyruk = 1;
    sa.kisitlayan_kuyruk = 1;
    Ortam o2(sa);
    Istemci k2{o2, o2.istemci_al()};
    const std::string run2 = baslat(k2);
    o2.dunya.dondur(true);
    std::vector<std::uint32_t> nolar;
    for (int n = 0; n < 3; ++n) {
        const HttpYanit z = k2.post("/api/v1/emniyet/degistir", "{\"istek_kimligi\":\"" + k2.kimlik() + "\",\"beklenen_run_id\":\"" + bayat +
                                                                  "\",\"girdi\":\"acil_durdurma\",\"hedef_durum\":\"basili\"}");
        DOGRULA(d, z.durum == 202 && guncel(z) == run2, "dolu kuyrukta bayat run_id'li kısıtlayan 202");
        nolar.push_back(komut_no(z));
    }
    DOGRULA(d, o2.cekirdek.sayaclar().bekleyen_kisitlayan == 1, "acil durdurma mandalı dolu (3. istek 2.'yle birleşti)");
    o2.dunya.dondur(false);
    for (int n = 0; n < 4; ++n) {
        o2.dunya.adim();
        o2.dongu();
    }
    for (const auto no : nolar) {
        DOGRULA(d, sonuc_metni(o2, no) == "uygulandi", "dolu kuyruktan sonra uygulandı: " + std::to_string(no));
    }
    return d.hata;
}

// EMNİYET — W4-K22 (a): kısıtlayan istek hiçbir sınır yüzünden reddedilmez. Eski 65 536'lık bekleme
// sınırını aşan sayıda istek; son istek farklı bir kısıtlayıcı etki taşır ve alıcıya ulaşır.
int test_kisitlayan_mandal_siniri()
{
    Denetci d;
    sahte::SahteAyar sa;
    sa.genel_kuyruk = 1;
    sa.kisitlayan_kuyruk = 1;
    Ortam o(sa);
    Istemci k{o, o.istemci_al()};
    const std::string run = baslat(k);
    o.dunya.dondur(true);
    const int kAdet = 70000;
    int ret_sayisi = 0;
    std::uint32_t ilk = 0;
    for (int i = 0; i < kAdet; ++i) {
        const HttpYanit y = k.post("/api/v1/emniyet/degistir", emniyet(k, "acil_durdurma", "", "basili", run));
        if (y.durum != 202) {
            ++ret_sayisi;
        }
        if (i == 0) {
            ilk = komut_no(y);
        }
    }
    const HttpYanit son = k.post("/api/v1/emniyet/degistir", emniyet(k, "emniyet_pimi", "f2", "takili", run));
    const HttpYanit fis = k.post("/api/v1/emniyet/degistir", emniyet(k, "silahlandirma_fisi", "f1", "cikarilmis", run));
    DOGRULA(d, ret_sayisi == 0 && son.durum == 202 && fis.durum == 202, "hiçbiri reddedilmedi: " + std::to_string(ret_sayisi));
    DOGRULA(d, o.cekirdek.sayaclar().bekleyen_kisitlayan == 3, "mandal sayısı sabit (acil, pim f2, fiş f1)");
    o.dunya.dondur(false);
    for (int n = 0; n < 6; ++n) {
        o.dunya.adim();
        o.dongu();
    }
    DOGRULA(d, sonuc_metni(o, komut_no(son)) == "uygulandi" && sonuc_metni(o, komut_no(fis)) == "uygulandi",
            "son istekteki farklı etkiler alıcıya ulaştı ve uygulandı");
    const json::Deger e = o.http(istek("GET", "/api/v1/emniyet")).json_();
    DOGRULA(d, e.bul("yetkili")->bul("acil_durdurma")->metin == "basili" &&
                   e.bul("yetkili")->bul("emniyet_pimi")->bul("f2")->metin == "takili" &&
                   e.bul("yetkili")->bul("silahlandirma_fisi")->bul("f1")->metin == "cikarilmis",
            "yetkili durum kısıtlı");
    std::size_t iletilen = 0;
    for (const auto& x : o.dunya.alinanlar()) {
        iletilen += x.kisitlayan ? 1u : 0u;
    }
    DOGRULA(d, iletilen <= 5, "birleşen istekler alıcıya tek tek gitmedi: " + std::to_string(iletilen));
    (void)ilk;
    return d.hata;
}

// EMNİYET — A3-K94: onay jetonu, erken ret yolları dahil ilk sunuluşta tüketilir; kısıtlayan
// istekte jeton yok sayılır (tüketilmez).
int test_jeton_erken_ret()
{
    Denetci d;
    Ortam o;
    Istemci k{o, o.istemci_al()};
    const std::string run = baslat(k);
    const std::string rv = ",\"beklenen_emniyet_rev\":" + std::to_string(emniyet_rev(o));
    // 1) Şema hatası (fazla alan) ile reddedilen gevşetme jetonu tüketir
    std::string t = jeton_al(k, "emniyet_pimi", "f1", "cikarilmis", run);
    HttpYanit y = k.post("/api/v1/emniyet/degistir",
                         emniyet(k, "emniyet_pimi", "f1", "cikarilmis", run, ",\"onay\":true,\"onay_jetonu\":\"" + t + "\"" + rv + ",\"fazla\":1"));
    DOGRULA(d, y.durum == 400, "şema hatası 400");
    y = k.post("/api/v1/emniyet/degistir", emniyet(k, "emniyet_pimi", "f1", "cikarilmis", run, ",\"onay\":true,\"onay_jetonu\":\"" + t + "\"" + rv));
    DOGRULA(d, y.durum == 409 && y.kod() == "onay_jetonu_gecersiz", "şema retten sonra aynı jeton geçersiz: " + y.kod());
    // 2) kimlik_cakismasi ile reddedilen istek jetonu tüketir
    const std::string t1 = jeton_al(k, "emniyet_pimi", "f1", "cikarilmis", run);
    const std::string govde1 = "{\"istek_kimligi\":\"cakisma-0001\",\"beklenen_run_id\":\"" + run +
                               "\",\"girdi\":\"emniyet_pimi\",\"tup\":\"f1\",\"hedef_durum\":\"cikarilmis\",\"onay\":true,\"onay_jetonu\":\"" + t1 + "\"" + rv + "}";
    y = k.post("/api/v1/emniyet/degistir", govde1);
    DOGRULA(d, y.durum == 202, "ilk istek kabul");
    const std::string t2 = jeton_al(k, "emniyet_pimi", "f2", "cikarilmis", run);
    const std::string govde2 = "{\"istek_kimligi\":\"cakisma-0001\",\"beklenen_run_id\":\"" + run +
                               "\",\"girdi\":\"emniyet_pimi\",\"tup\":\"f2\",\"hedef_durum\":\"cikarilmis\",\"onay\":true,\"onay_jetonu\":\"" + t2 + "\"" + rv + "}";
    y = k.post("/api/v1/emniyet/degistir", govde2);
    DOGRULA(d, y.durum == 409 && y.kod() == "kimlik_cakismasi", "kimlik çakışması");
    y = k.post("/api/v1/emniyet/degistir", emniyet(k, "emniyet_pimi", "f2", "cikarilmis", run, ",\"onay\":true,\"onay_jetonu\":\"" + t2 + "\"" + rv));
    DOGRULA(d, y.durum == 409 && y.kod() == "onay_jetonu_gecersiz", "kimlik çakışmasından sonra jeton geçersiz: " + y.kod());
    // 3) Yer istasyonu: şema hatasıyla reddedilen onaylı komut jetonu tüketir
    const HttpYanit jy = k.post("/api/v1/yer_istasyonu/onay_jetonu", "{\"beklenen_run_id\":\"" + run + "\",\"komut\":\"fc_fire\",\"parametreler\":{\"tube\":0}}");
    const json::Deger jj = jy.json_();
    const std::string tyi = jj.bul("onay_jetonu") != nullptr ? jj.bul("onay_jetonu")->metin : std::string();
    y = k.post("/api/v1/yer_istasyonu/komut", "{\"istek_kimligi\":\"" + k.kimlik() + "\",\"beklenen_run_id\":\"" + run +
                                                  "\",\"komut\":\"fc_fire\",\"parametreler\":{\"tube\":0},\"onay\":\"evet\",\"onay_jetonu\":\"" + tyi + "\"" + rv + "}");
    DOGRULA(d, y.durum == 400, "yer istasyonu şema hatası 400");
    y = k.post("/api/v1/yer_istasyonu/komut", "{\"istek_kimligi\":\"" + k.kimlik() + "\",\"beklenen_run_id\":\"" + run +
                                                  "\",\"komut\":\"fc_fire\",\"parametreler\":{\"tube\":0},\"onay\":true,\"onay_jetonu\":\"" + tyi + "\"" + rv + "}");
    DOGRULA(d, y.durum == 409 && y.kod() == "onay_jetonu_gecersiz", "yer istasyonu jetonu tüketildi: " + y.kod());
    // 4) Kısıtlayan istekte jeton yok sayılır, tüketilmez
    const std::string t3 = jeton_al(k, "emniyet_pimi", "f1", "cikarilmis", run);
    y = k.post("/api/v1/emniyet/degistir", emniyet(k, "emniyet_pimi", "f1", "takili", run, ",\"onay_jetonu\":\"" + t3 + "\""));
    DOGRULA(d, y.durum == 202, "kısıtlayan kabul");
    y = k.post("/api/v1/emniyet/degistir", emniyet(k, "emniyet_pimi", "f1", "cikarilmis", run, ",\"onay\":true,\"onay_jetonu\":\"" + t3 + "\"" + rv));
    DOGRULA(d, y.durum == 202, "kısıtlayanda sunulan jeton tüketilmedi: " + y.kod());
    return d.hata;
}

int test_yer_istasyonu()
{
    Denetci d;
    Ortam o;
    Istemci k{o, o.istemci_al()};
    const HttpYanit kat = o.http(istek("GET", "/api/v1/yer_istasyonu/komutlar"));
    const auto mb = komut_katalogu_baytlari();
    DOGRULA(d, kat.durum == 200 && kat.govde == std::string(reinterpret_cast<const char*>(mb.data()), mb.size()),
            "komut kataloğu bayt bayt (A3-K100)");
    const std::string run = baslat(k);
    auto komut = [&](const std::string& ad, const std::string& par, const std::string& ek = {}) {
        return k.post("/api/v1/yer_istasyonu/komut", "{\"istek_kimligi\":\"" + k.kimlik() + "\",\"beklenen_run_id\":\"" + run +
                                                         "\",\"komut\":\"" + ad + "\",\"parametreler\":" + par + ek + "}");
    };
    HttpYanit y = komut("ates_et", "{}");
    DOGRULA(d, y.durum == 422 && y.kod() == "komut_listede_yok", "katalog dışı 422");
    y = komut("fc_fire", R"({"tube":2})");
    DOGRULA(d, y.durum == 422 && y.kod() == "gecersiz_deger", "şema dışı parametre 422");
    y = komut("fc_disarm", "{}");
    DOGRULA(d, y.durum == 202, "daraltıcı tek adım");
    y = komut("fc_fire", R"({"tube":0})");
    DOGRULA(d, y.durum == 409 && y.kod() == "onay_gerekli", "genişletici jetonsuz 409");
    const std::uint32_t rev = emniyet_rev(o);
    auto jeton = [&](const std::string& ad, const std::string& par) {
        const HttpYanit j = k.post("/api/v1/yer_istasyonu/onay_jetonu",
                                   "{\"beklenen_run_id\":\"" + run + "\",\"komut\":\"" + ad + "\",\"parametreler\":" + par + "}");
        const json::Deger jj = j.json_();
        const json::Deger* t = jj.bul("onay_jetonu");
        return t != nullptr ? t->metin : std::string();
    };
    std::string t = jeton("fc_fire", R"({"tube":0})");
    DOGRULA(d, t.size() == 32, "yer istasyonu jetonu");
    y = komut("fc_fire", R"({"tube":1})", ",\"onay\":true,\"onay_jetonu\":\"" + t + "\",\"beklenen_emniyet_rev\":" + std::to_string(rev));
    DOGRULA(d, y.durum == 409 && y.kod() == "onay_jetonu_gecersiz", "başka parametreye jeton geçersiz (A3-K103)");
    t = jeton("fc_fire", R"({"tube":0})");
    y = komut("fc_fire", R"({"tube":0})", ",\"onay\":true,\"onay_jetonu\":\"" + t + "\"");
    DOGRULA(d, y.durum == 400, "genişletici revizyonsuz 400");
    t = jeton("fc_fire", R"({"tube":0})");
    y = komut("fc_fire", R"({ "tube" : 0 })", ",\"onay\":true,\"onay_jetonu\":\"" + t + "\",\"beklenen_emniyet_rev\":" + std::to_string(rev));
    DOGRULA(d, y.durum == 202, "genişletici geçerli 202 (kanonik parametre özeti)");
    o.dunya.adim();
    DOGRULA(d, sonuc_metni(o, komut_no(y)) == "uygulandi", "iletildi");
    bool tasindi = false;
    for (const auto& x : o.dunya.alinanlar()) {
        if (x.tur == KomutTuru::yer_istasyonu_komut && x.yi_komut == "fc_fire") {
            tasindi = x.yi_parametreler_json == R"({"tube":0})" && x.yi_sinif == KomutSinifi::genisletici && !x.kisitlayan &&
                      x.beklenen_emniyet_rev == rev;
        }
    }
    DOGRULA(d, tasindi, "FC_FIRE katalogdaki biçimiyle taşındı");
    HttpYanit tj = k.post("/api/v1/yer_istasyonu/onay_jetonu", "{\"beklenen_run_id\":\"" + run + "\",\"komut\":\"fc_disarm\",\"parametreler\":{}}");
    DOGRULA(d, tj.durum == 422, "onaysız komuta jeton 422");
    return d.hata;
}

}  // namespace ida::a3::test
