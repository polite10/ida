// W4-sonar-sahne: A7 varlık okuyucusu (a7_varlik.hpp).
//
//   a7_varlik_havuz    assets/havuz kümesinin dört varlığı yüklenir (gerçek dosyalar; A7-K06 özetleri);
//                      havuzun akustik ağı = beş çarpışma kaynak ağı (A7-K27); iç yüzler beyan edilen
//                      ölçülerde; uygunluk örneği (assets/ornek, ikili dosyası yok) EKSIK_VARLIK
//   a7_varlik_olumsuz  bellekte üretilen kutu varlıklarıyla A7-U03/U05/U07/U09/U10/U18'in okuyucuya
//                      düşen satırları: her bozukluk beklenen A7 koduyla reddedilir; akustik[] doluyken
//                      yalnız SON_* ağları kullanılır
#include "glb_yazici.hpp"
#include "ic.hpp"
#include "ida/senaryo/a7_varlik.hpp"
#include "test_destek.hpp"

#include <cmath>
#include <functional>

namespace ida::senaryo::test {

namespace {

std::optional<A7Varlik> yukle(const BellekKaynagi& k, const std::string& kume, const std::string& kimlik, A7Hata& h)
{
    const auto m = k.oku("assets/" + kume + "/manifest.json");
    ic::JsonOkuma j = ic::json_oku(m.value_or(""), 0);
    if (!j.kok) {
        h.kod = "AYRISTIRMA";
        return std::nullopt;
    }
    return a7_varlik_yukle(*j.kok, kume, kimlik, k, h);
}

// Bellekteki deneme kümesi: tek varlık "deneme" (varsayılan tür hedef), kutu görsel + kutu çarpışma.
struct Deneme {
    GlbAyar glb;
    std::string tur = "hedef";
    std::string roller;
    std::string atama = R"([{"gltf_malzeme":0,"malzeme":"mz"}])";
    std::string kutu = R"({"en_kucuk":[-0.5,-0.25,-0.25],"en_buyuk":[0.5,0.25,0.25]})";
    std::string ek;
    std::string eksenler = R"({"gltf":"sag_el_y_yukari_z_on","gltf_govde":[[0,0,1],[-1,0,0],[0,-1,0]],)"
                           R"("gltf_godot_yerel":[[-1,0,0],[0,1,0],[0,0,-1]]})";
    std::function<void(std::string&)> bayt_degistir;   // özet hesaplandıktan sonra (A7-K06)
    bool dosya_yok = false;
    std::string bayt_ek;                                // manifestteki bayt alanına eklenen

    Deneme()
    {
        glb.dugumler = {{"kutu", true, {-0.5, -0.25, -0.25}, {0.5, 0.25, 0.25}},
                        {"COL_kutu", true, {-0.5, -0.25, -0.25}, {0.5, 0.25, 0.25}}};
        roller = R"("gorsel":[{"dugum":1,"ad":"kutu","kosul":"her_zaman"}],)"
                 R"("carpisma":[{"dugum":2,"ad":"COL_kutu","ayristirma":"yok"}])";
    }

    BellekKaynagi kaynak() const
    {
        std::string b = glb_yaz(glb);
        const std::string ozet = sha256_hex(b);
        const std::string boy = std::to_string(b.size()) + bayt_ek;
        if (bayt_degistir) bayt_degistir(b);
        BellekKaynagi k;
        k.dosyalar["assets/deneme/manifest.json"] =
            R"({"sema_surumu":"1.0","kume_kimligi":"deneme","kume_surumu":1,)"
            R"("birimler":{"uzunluk":"m","aci":"rad","kutle":"kg","zaman":"s"},"eksenler":)" +
            eksenler + R"(,"uretim":{"disa_aktarici":{"arac":"elle","surum":"0"}},)"
                       R"("malzemeler":[{"kimlik":"mz","optik":{"model":"gltf_metalik_puruzluluk"},)"
                       R"("akustik":{"geri_sacilim_mu":0.05,"gelis_yasasi":"lambert_cos2","ayna_yansima_rs":0.9,)"
                       R"("puruzluluk_gamma_p":0.3,"kaynak":"tahmin"}}],)"
                       R"("varliklar":[{"kimlik":"deneme","tur":")" +
            tur + R"(","dosya":"deneme.glb","sha256":")" + ozet + R"(","bayt":)" + boy + ek +
            R"(,"referans":{"dugum":0,"ad":"REF"},"sinir_kutusu_govde_m":)" + kutu + R"(,"malzeme_atamasi":)" + atama +
            "," + roller + "}]}";
        if (!dosya_yok) k.dosyalar["assets/deneme/deneme.glb"] = b;
        return k;
    }
};

void ret_bekle(Denetci& d, const Deneme& x, const std::string& kod, const std::string& ne)
{
    A7Hata h;
    const auto v = yukle(x.kaynak(), "deneme", "deneme", h);
    DOGRULA(d, !v && h.kod == kod, ne + ": beklenen " + kod + ", bulunan " + (v ? std::string("kabul") : h.metin()));
    if (!v && h.kod == kod) {
        std::printf("  %s → %s\n", ne.c_str(), h.metin().c_str());
    }
}

void json_degis(Deneme& x, const std::string& eski, const std::string& yeni)
{
    x.glb.json_degistir = [eski, yeni](std::string& j) {
        const std::size_t i = j.find(eski);
        if (i != std::string::npos) j.replace(i, eski.size(), yeni);
    };
}

}  // namespace

int test_a7_varlik_havuz(const TestBaglami& tb)
{
    Denetci d;
    DiskKaynagi disk(tb.kok);
    BellekKaynagi k;
    for (const char* f : {"manifest.json", "ida0.glb", "torpido_ida0.glb", "hedef_ornek.glb", "havuz_ornek.glb"}) {
        const auto b = disk.oku(std::string("assets/havuz/") + f);
        DOGRULA(d, b.has_value(), std::string("assets/havuz/") + f);
        if (b) k.dosyalar[std::string("assets/havuz/") + f] = *b;
    }
    for (const char* v : {"ida0", "torpido_ida0", "hedef_ornek", "havuz_ornek"}) {
        A7Hata h;
        const auto x = yukle(k, "havuz", v, h);
        DOGRULA(d, x.has_value(), std::string(v) + " yüklenir: " + h.metin());
        if (x) {
            std::printf("  %s: tür %s, akustik %zu üçgen (çarpışma kaynak %zu), malzeme %zu\n", v, x->tur.c_str(),
                        x->akustik_ucgen, x->carpisma_kaynak_ucgen, x->malzemeler.size());
        }
    }
    A7Hata h;
    const auto havuz = yukle(k, "havuz", "havuz_ornek", h);
    if (havuz) {
        DOGRULA(d, havuz->akustik_carpismadan && havuz->akustik.size() == 5u && havuz->akustik_ucgen == 60u &&
                       havuz->carpisma_kaynak_ucgen == havuz->akustik_ucgen,
                "havuz: akustik = beş çarpışma kaynak ağı (A7-K27), 60 üçgen");
        DOGRULA(d, havuz->malzemeler.size() == 1u && havuz->malzemeler[0] == "havuz_beton", "havuz: malzeme havuz_beton");
        // İç yüzler: taban z = 3, duvarlar x = ±12,5 ve y = ±6,25 (gövde çerçevesi).
        bool taban = false, kuzey = false, guney = false, dogu = false, bati = false;
        for (const A7Ag& a : havuz->akustik) {
            for (const auto& p : a.koseler) {
                taban = taban || (a.ad == "COL_taban" && std::abs(p[2] - 3.0) < 1e-6);
                kuzey = kuzey || (a.ad == "COL_duvar_kuzey" && std::abs(p[0] - 12.5) < 1e-6);
                guney = guney || (a.ad == "COL_duvar_guney" && std::abs(p[0] + 12.5) < 1e-6);
                dogu = dogu || (a.ad == "COL_duvar_dogu" && std::abs(p[1] - 6.25) < 1e-6);
                bati = bati || (a.ad == "COL_duvar_bati" && std::abs(p[1] + 6.25) < 1e-6);
            }
        }
        DOGRULA(d, taban && kuzey && guney && dogu && bati, "havuz iç yüzleri beyan edilen ölçülerde");
    }
    // Uygunluk örneği: ikili dosyaları yok (A7 §3.7) → EKSIK_VARLIK (A7-K51: çalıştırma başlamaz).
    {
        BellekKaynagi o;
        o.dosyalar["assets/ornek/manifest.json"] = disk.oku("assets/ornek/manifest.json").value_or("");
        for (const char* v : {"havuz_ornek", "hedef_ornek"}) {
            A7Hata e;
            const auto x = yukle(o, "ornek", v, e);
            DOGRULA(d, !x && e.kod == "EKSIK_VARLIK", std::string("ornek/") + v + ": " + e.metin());
        }
        // Dosya bulunsa da yer tutucu özet tutmaz (A7-K06).
        o.dosyalar["assets/ornek/havuz_ornek.glb"] = k.dosyalar["assets/havuz/havuz_ornek.glb"];
        A7Hata e;
        const auto x = yukle(o, "ornek", "havuz_ornek", e);
        DOGRULA(d, !x && e.kod == "OZET_UYUSMAZ", "ornek/havuz_ornek yer tutucu özet: " + e.metin());
    }
    return d.hata;
}

int test_a7_varlik_olumsuz(const TestBaglami&)
{
    Denetci d;
    {
        Deneme x;
        A7Hata h;
        const auto v = yukle(x.kaynak(), "deneme", "deneme", h);
        DOGRULA(d, v.has_value() && v->akustik_ucgen == 12u && v->carpisma_kaynak_ucgen == 12u && v->akustik_carpismadan,
                "geçerli deneme varlığı yüklenir: " + h.metin());
    }
    // A7-K06
    {
        Deneme x;
        x.bayt_degistir = [](std::string& b) { b[b.size() - 1] = static_cast<char>(b[b.size() - 1] ^ 1); };
        ret_bekle(d, x, "OZET_UYUSMAZ", "tek bayt değişen .glb");
        Deneme y;
        y.bayt_ek = "0";
        ret_bekle(d, y, "OZET_UYUSMAZ", "boyut uyuşmazlığı");
        Deneme z;
        z.dosya_yok = true;
        ret_bekle(d, z, "EKSIK_VARLIK", "dosya yok");
    }
    // A7-K10–K12 (A7-U03)
    {
        Deneme x;
        x.bayt_degistir = [](std::string& b) { b[0] = 'X'; };
        // Özet bozuk dosyanınki olsun diye kaynak yeniden kurulur.
        BellekKaynagi k = x.kaynak();
        std::string& m = k.dosyalar["assets/deneme/manifest.json"];
        const std::string& b = k.dosyalar["assets/deneme/deneme.glb"];
        const std::size_t i = m.find("\"sha256\":\"") + 10u;
        m.replace(i, 64u, sha256_hex(b));
        A7Hata h;
        const auto v = yukle(k, "deneme", "deneme", h);
        DOGRULA(d, !v && h.kod == "GLTF", "yanlış sihirli sayı: " + h.metin());
        BellekKaynagi k2 = Deneme().kaynak();
        std::string& b2 = k2.dosyalar["assets/deneme/deneme.glb"];
        b2[4] = 1;   // sürüm 1
        std::string& m2 = k2.dosyalar["assets/deneme/manifest.json"];
        m2.replace(m2.find("\"sha256\":\"") + 10u, 64u, sha256_hex(b2));
        const auto v2 = yukle(k2, "deneme", "deneme", h);
        DOGRULA(d, !v2 && h.kod == "GLTF", "GLB sürüm 1: " + h.metin());
    }
    {
        Deneme x;
        json_degis(x, "\"buffers\":[{", "\"buffers\":[{\"uri\":\"dis.bin\",");
        ret_bekle(d, x, "GLTF", "dış URI");
        json_degis(x, "\"scene\":0,", "\"scene\":0,\"extensionsRequired\":[\"KHR_draco_mesh_compression\"],");
        ret_bekle(d, x, "UZANTI", "extensionsRequired dolu");
        json_degis(x, "\"scene\":0,", "\"scene\":0,\"extensionsUsed\":[\"EXT_ozel\"],");
        ret_bekle(d, x, "UZANTI", "izinsiz uzantı");
        json_degis(x, "\"scene\":0,", "\"scene\":0,\"extensionsUsed\":[\"KHR_lights_punctual\"],");
        A7Hata h;
        DOGRULA(d, yukle(x.kaynak(), "deneme", "deneme", h).has_value(), "izinli uzantı kabul: " + h.metin());
        json_degis(x, "\"mode\":4", "\"mode\":1");
        ret_bekle(d, x, "GLTF", "mode = 1");
        json_degis(x, "\"componentType\":5126,", "\"componentType\":5126,\"sparse\":{},");
        ret_bekle(d, x, "GLTF", "seyrek erişimci");
        json_degis(x, "\"scene\":0,", "\"scene\":0,\"animations\":[{}],");
        ret_bekle(d, x, "GLTF", "animasyon");
        json_degis(x, "\"scene\":0,", "\"scene\":0,\"skins\":[{}],");
        ret_bekle(d, x, "GLTF", "deri");
        json_degis(x, "\"componentType\":5126,", "\"componentType\":5123,");
        ret_bekle(d, x, "GLTF", "POSITION float32 değil");
        json_degis(x, "\"attributes\":{\"POSITION\":0}", "\"attributes\":{\"POSITION\":0},\"targets\":[{\"POSITION\":0}]");
        ret_bekle(d, x, "GLTF", "biçim hedefi");
    }
    // A7-K13 (A7-U03 ölçek, yansıtıcı matris; A7-U07 santimetre)
    {
        Deneme x;
        json_degis(x, "{\"name\":\"kutu\",\"mesh\"", "{\"name\":\"kutu\",\"scale\":[0.01,0.01,0.01],\"mesh\"");
        ret_bekle(d, x, "BIRIM", "ölçek 0,01");
        json_degis(x, "{\"name\":\"kutu\",\"mesh\"",
                   "{\"name\":\"kutu\",\"matrix\":[-1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1],\"mesh\"");
        ret_bekle(d, x, "BIRIM", "yansıtıcı matris");
        json_degis(x, "\"scene\":0,", "\"scene\":1,");
        ret_bekle(d, x, "BIRIM", "scene = 1");
        // Santimetreyle dışa aktarılmış varlık: köşeler 100 kat, manifest metre (A7-K23).
        Deneme y;
        for (auto& n : y.glb.dugumler) {
            for (int k = 0; k < 3; ++k) {
                n.mn[static_cast<std::size_t>(k)] *= 100.0;
                n.mx[static_cast<std::size_t>(k)] *= 100.0;
            }
        }
        ret_bekle(d, y, "BIRIM", "santimetre (A7-U07)");
        Deneme z;
        z.kutu = R"({"en_kucuk":[-0.502,-0.25,-0.25],"en_buyuk":[0.5,0.25,0.25]})";
        ret_bekle(d, z, "BIRIM", "sınır kutusu 2 mm farklı");
        Deneme t;
        t.kutu = R"({"en_kucuk":[-0.5005,-0.25,-0.25],"en_buyuk":[0.5,0.25,0.25]})";
        A7Hata h;
        DOGRULA(d, yukle(t.kaynak(), "deneme", "deneme", h).has_value(), "sınır kutusu 0,5 mm farklı: kabul " + h.metin());
    }
    // A7-K14
    {
        Deneme x;
        x.eksenler = R"({"gltf":"sag_el_y_yukari_z_on","gltf_govde":[[1,0,0],[0,1,0],[0,0,1]],)"
                     R"("gltf_godot_yerel":[[-1,0,0],[0,1,0],[0,0,-1]]})";
        ret_bekle(d, x, "EKSEN", "eksen kabulü");
    }
    // A7-K19–K21 (A7-U05)
    {
        Deneme x;
        x.roller = R"("gorsel":[{"dugum":1,"ad":"govde","kosul":"her_zaman"}],)"
                   R"("carpisma":[{"dugum":2,"ad":"COL_kutu","ayristirma":"yok"}])";
        ret_bekle(d, x, "DUGUM", "ad–indeks uyuşmazlığı");
        Deneme y;
        y.glb.dugumler.push_back({"fazla", true, {-0.1, -0.1, -0.1}, {0.1, 0.1, 0.1}});
        ret_bekle(d, y, "DUGUM", "listelenmemiş ağlı düğüm");
        Deneme z;
        z.roller = R"("gorsel":[{"dugum":1,"ad":"kutu","kosul":"her_zaman"}],)"
                   R"("carpisma":[{"dugum":2,"ad":"COL_kutu","ayristirma":"yok"},{"dugum":2,"ad":"COL_kutu","ayristirma":"yok"}])";
        ret_bekle(d, z, "DUGUM", "iki kez listelenen düğüm");
        Deneme w;
        w.glb.dugumler[0].ad = "COL_kutu2";
        w.roller = R"("gorsel":[{"dugum":1,"ad":"COL_kutu2","kosul":"her_zaman"}],)"
                   R"("carpisma":[{"dugum":2,"ad":"COL_kutu","ayristirma":"yok"}])";
        ret_bekle(d, w, "DUGUM", "görselde ayrılmış önek");
        Deneme r;
        json_degis(r, "{\"name\":\"REF\"}", "{\"name\":\"REF\",\"rotation\":[0,0.7071067811865476,0,0.7071067811865476]}");
        ret_bekle(d, r, "DUGUM", "REF dönüşü birim değil");
        Deneme e;
        json_degis(e, "\"nodes\":[0,1,2]", "\"nodes\":[0,1]");
        ret_bekle(d, e, "ERISILEMEZ", "scenes[0]'dan erişilemeyen rol düğümü");
        Deneme c;
        json_degis(c, "\"nodes\":[0,1,2]}],\"nodes\":[{\"name\":\"REF\"}", "\"nodes\":[0,1]}],\"nodes\":[{\"name\":\"REF\",\"children\":[2]}");
        A7Hata h;
        DOGRULA(d, yukle(c.kaynak(), "deneme", "deneme", h).has_value(), "REF'in çocuğu olan rol düğümü erişilebilir: " + h.metin());
        json_degis(c, "\"nodes\":[0,1,2]}],\"nodes\":[{\"name\":\"REF\"},{\"name\":\"kutu\",\"mesh\":0}",
                   "\"nodes\":[0,1]}],\"nodes\":[{\"name\":\"REF\",\"children\":[2]},{\"name\":\"kutu\",\"mesh\":0,\"children\":[2]}");
        ret_bekle(d, c, "ERISILEMEZ", "iki ebeveynli düğüm");
    }
    // A7-K24
    {
        Deneme x;
        x.roller = R"("gorsel":[{"dugum":1,"ad":"kutu","kosul":"tup_1_dolu"}],)"
                   R"("carpisma":[{"dugum":2,"ad":"COL_kutu","ayristirma":"yok"}])";
        ret_bekle(d, x, "TUR", "koşullu düğüm hedefte");
    }
    // A7-K26 (A7-U08, U09)
    {
        Deneme x;
        x.glb.dugumler[1].delik = true;
        ret_bekle(d, x, "KAPALI_DEGIL", "delikli çarpışma ağı");
        Deneme y;
        y.glb.dugumler[1].ters = true;
        ret_bekle(d, y, "KAPALI_DEGIL", "içe dönük çarpışma ağı");
        Deneme z;
        z.glb.dugumler[1].ikiz = true;
        z.glb.dugumler[1].kaydir = {0.0, 0.0, -0.6};
        z.kutu = R"({"en_kucuk":[-0.5,-0.25,-0.85],"en_buyuk":[0.5,0.25,0.25]})";
        ret_bekle(d, z, "DISBUKEY_DEGIL", "dışbükey olmayan yok ağı");
        z.roller = R"("gorsel":[{"dugum":1,"ad":"kutu","kosul":"her_zaman"}],)"
                   R"("carpisma":[{"dugum":2,"ad":"COL_kutu","ayristirma":"coacd"}])";
        A7Hata h;
        const auto v = yukle(z.kaynak(), "deneme", "deneme", h);
        DOGRULA(d, v.has_value() && v->akustik_ucgen == 24u, "coacd kaynağı dışbükey olmak zorunda değil; sonar kaynağı kullanır: " + h.metin());
    }
    // A7-K28 (A7-U10)
    {
        Deneme x;
        x.glb.malzeme_sayisi = 2;
        x.glb.dugumler[1].malzeme = 1;
        ret_bekle(d, x, "MALZEME", "atanmamış glTF malzemesi");
        Deneme y;
        y.atama = R"([{"gltf_malzeme":0,"malzeme":"yok_boyle"}])";
        ret_bekle(d, y, "MALZEME", "tabloda olmayan kimlik");
        Deneme z;
        json_degis(z, ",\"material\":0,\"mode\":4}]}]", ",\"mode\":4}]}]");
        ret_bekle(d, z, "MALZEME", "malzemesiz temel öğe");
    }
    // A7-K56 (A7-U18): statik, REF'ten 3,0 m'de taban.
    {
        Deneme x;
        x.tur = "statik";
        x.glb.dugumler = {{"taban", true, {-5.0, -5.0, 3.0}, {5.0, 5.0, 3.2}}, {"COL_taban", true, {-5.0, -5.0, 3.0}, {5.0, 5.0, 3.2}}};
        x.roller = R"("gorsel":[{"dugum":1,"ad":"taban","kosul":"her_zaman"}],)"
                   R"("carpisma":[{"dugum":2,"ad":"COL_taban","ayristirma":"yok"}])";
        x.kutu = R"({"en_kucuk":[-5,-5,3],"en_buyuk":[5,5,3.2]})";
        x.ek = R"(,"taban_derinligi_m":3.0)";
        A7Hata h;
        DOGRULA(d, yukle(x.kaynak(), "deneme", "deneme", h).has_value(), "taban 3,0 m: kabul " + h.metin());
        x.ek = R"(,"taban_derinligi_m":3.002)";
        ret_bekle(d, x, "TABAN", "taban 3,002 m beyanı");
        x.ek = R"(,"taban_derinligi_m":3.0)";
        x.glb.dugumler[1].mn = {6.0, 6.0, 3.0};
        x.glb.dugumler[1].mx = {7.0, 7.0, 3.2};
        x.glb.dugumler[0].mx = {7.0, 7.0, 3.2};
        x.kutu = R"({"en_kucuk":[-5,-5,3],"en_buyuk":[7,7,3.2]})";
        ret_bekle(d, x, "TABAN", "REF'in altında taban yok");
        Deneme y;
        y.ek = R"(,"taban_derinligi_m":3.0)";
        ret_bekle(d, y, "SEMA", "hedefte taban_derinligi_m");
    }
    // A7-K27: akustik[] doluyken yalnız SON_* (A7-U09'un sonar yarısı, iki yönü).
    {
        Deneme x;
        x.glb.dugumler.push_back({"COL_ust", true, {-0.5, -0.25, -0.75}, {0.5, 0.25, -0.25}});
        x.glb.dugumler.push_back({"SON_kutu", true, {-0.5, -0.25, -0.25}, {0.5, 0.25, 0.25}});
        x.kutu = R"({"en_kucuk":[-0.5,-0.25,-0.75],"en_buyuk":[0.5,0.25,0.25]})";
        x.roller = R"("gorsel":[{"dugum":1,"ad":"kutu","kosul":"her_zaman"}],)"
                   R"("carpisma":[{"dugum":2,"ad":"COL_kutu","ayristirma":"yok"},{"dugum":3,"ad":"COL_ust","ayristirma":"yok"}],)"
                   R"("akustik":[{"dugum":4,"ad":"SON_kutu"}])";
        A7Hata h;
        const auto v = yukle(x.kaynak(), "deneme", "deneme", h);
        DOGRULA(d, v && !v->akustik_carpismadan && v->akustik.size() == 1u && v->akustik[0].ad == "SON_kutu" &&
                       v->akustik_ucgen == 12u && v->carpisma_kaynak_ucgen == 24u,
                "akustik[] dolu: yalnız SON_kutu (12 üçgen; çarpışma 24) " + h.metin());
        x.roller = R"("gorsel":[{"dugum":1,"ad":"kutu","kosul":"her_zaman"},{"dugum":4,"ad":"kutu2","kosul":"her_zaman"}],)"
                   R"("carpisma":[{"dugum":2,"ad":"COL_kutu","ayristirma":"yok"},{"dugum":3,"ad":"COL_ust","ayristirma":"yok"}])";
        x.glb.dugumler[3].ad = "kutu2";
        const auto w = yukle(x.kaynak(), "deneme", "deneme", h);
        DOGRULA(d, w && w->akustik_carpismadan && w->akustik_ucgen == w->carpisma_kaynak_ucgen && w->akustik_ucgen == 24u,
                "akustik[] boş: çarpışma kaynak ağlarının hepsi (24 üçgen) " + h.metin());
    }
    return d.hata;
}

}  // namespace ida::senaryo::test
