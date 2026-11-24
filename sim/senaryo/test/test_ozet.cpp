// A5-K17 davranış özetinin değişmezliği, A5-K02 yeniden yükleme ve A5-K90–K95 rapor biçimi.
#include "ic.hpp"
#include "test_destek.hpp"

#include <nlohmann/json.hpp>

namespace ida::senaryo::test {

namespace {

struct Ozetler {
    bool gecerli = false;
    std::string davranis;
    std::string kaynak;
};

Ozetler ozetle(const BellekKaynagi& k)
{
    const CozumSonucu s = senaryo_coz(k, kSenaryo);
    Ozetler o;
    o.gecerli = s.rapor.gecerli;
    o.davranis = s.rapor.davranis_ozeti.value_or("");
    o.kaynak = s.rapor.kaynak_ozetleri.count(kSenaryo) ? s.rapor.kaynak_ozetleri.at(kSenaryo) : "";
    if (!o.gecerli) {
        std::printf("%s", bulgu_ozeti(s.rapor).c_str());
    }
    return o;
}

struct Donusum {
    const char* ad;
    const char* dosya;
    std::vector<std::pair<std::string, std::string>> degisiklikler;
    bool ozet_ayni;
};

}  // namespace

int test_ozet_degismezligi(const TestBaglami& tb)
{
    Denetci d;
    const BellekKaynagi temel = temel_kaynak(tb);
    const Ozetler o0 = ozetle(temel);
    DOGRULA(d, o0.gecerli, "örnek geçerli olmalı");
    const std::vector<Donusum> donusumler = {
        {"anahtar_sirasi", kSenaryo, {{"kip = \"sil\"\nsure_ns = 60_000_000_000      # 60 s", "sure_ns = 60_000_000_000\nkip = \"sil\""}}, true},
        {"yorum", kSenaryo, {{"[rastgelelik]", "# yeni bir yorum\n[rastgelelik]   # satır sonu yorumu"}}, true},
        {"bosluk", kSenaryo, {{"ana_tohum = 1", "ana_tohum    =    1   "}}, true},
        {"satir_ici_tablo", kSenaryo,
         {{"fis = { f1 = false, f2 = false }\npim = { f1 = true, f2 = true }", "fis.f1 = false\nfis.f2 = false\n[emniyet_girdileri.pim]\nf1 = true\nf2 = true"}},
         true},
        {"tamsayi_kayan", kSenaryo, {{"derinlik = 3.0 ", "derinlik = 3 "}}, true},
        {"bilimsel_yazim", kArac, {{"oteleme = 0.30", "oteleme = 3e-1"}}, true},
        {"eksi_sifir", kSenaryo, {{"konum = [4.0, 0.0, 1.5]", "konum = [4.0, -0.0, 1.5]"}}, true},
        {"alt_cizgi", kSenaryo, {{"sure_ns = 60_000_000_000", "sure_ns = 60000000000"}}, true},
        {"onaltilik", kSenaryo, {{"ana_tohum = 1", "ana_tohum = 0x1"}}, true},
        {"varsayilanlar_silindi", kSenaryo,
         {{"[emniyet_girdileri]           # açıkça yazıldı; değerler varsayılanla aynı (fiş çıkık, pim takılı)\nacil_durdurma = false\nfis = { f1 = false, f2 = false }\npim = { f1 = true, f2 = true }\n", ""},
          {"gorunum = \"sade\"\n", ""},
          {"kayit = \"telemetri\"\n", ""},
          {"konum = [0.0, 0.0, 0.0]\nyonelim = [1.0, 0.0, 0.0, 0.0]\nyuklu_tupler = [\"f1\", \"f2\"]", "yuklu_tupler = [\"f1\", \"f2\"]"},
          {"yukleme_zamani_ns = 0\n", ""}},
         true},
        {"varsayilan_acik_yazildi", kArac, {{"[arac.esc]", "[arac.hidrodinamik.sapma]\neklenmis_kutle = [1.0, 1.0, 1.0, 1.0, 1.0, 1.0]\n\n[arac.esc]"}}, true},
        {"olay_sirasi", kSenaryo, {}, false},  // aşağıda elle: iki olayın yeri değişir
        {"yaprak", kSenaryo, {{"ana_tohum = 1", "ana_tohum = 2"}}, false},
        {"yaprak_araç", kArac, {{"kutle = 13.5 ", "kutle = 13.6 "}}, false},
    };
    for (const auto& t : donusumler) {
        BellekKaynagi k = temel;
        bool uygulandi = true;
        for (const auto& [eski, yeni] : t.degisiklikler) {
            const auto y = degistir(k.dosyalar[t.dosya], eski, yeni);
            if (!y) {
                uygulandi = false;
                continue;
            }
            k.dosyalar[t.dosya] = *y;
        }
        if (std::string(t.ad) == "olay_sirasi") {
            // İki olayın yerini değiştir.
            std::string& m = k.dosyalar[kSenaryo];
            const std::size_t a = temel.dosyalar.at(kSenaryo).find("[[olaylar]]");
            const std::size_t b = temel.dosyalar.at(kSenaryo).find("[[olaylar]]", a + 1);
            const std::size_t c = temel.dosyalar.at(kSenaryo).find("[emniyet_girdileri]");
            const std::string& ozgun = temel.dosyalar.at(kSenaryo);
            m = ozgun.substr(0, a) + ozgun.substr(b, c - b) + ozgun.substr(a, b - a) + ozgun.substr(c);
        }
        DOGRULA(d, uygulandi, std::string(t.ad) + ": değişiklik uygulanamadı");
        const Ozetler o = ozetle(k);
        DOGRULA(d, o.gecerli, std::string(t.ad) + ": geçerli olmalı");
        if (t.ozet_ayni) {
            DOGRULA(d, o.davranis == o0.davranis, std::string(t.ad) + ": davranış özeti değişmemeli");
        } else {
            DOGRULA(d, o.davranis != o0.davranis, std::string(t.ad) + ": davranış özeti değişmeli");
        }
        DOGRULA(d, t.dosya != std::string(kSenaryo) || o.kaynak != o0.kaynak, std::string(t.ad) + ": kaynak özeti değişmeli");
    }
    // Tablo ↔ satır içi tablo, ek alan olmadan.
    {
        BellekKaynagi k = temel;
        auto y = degistir(k.dosyalar[kArac],
                          "[arac.hidrodinamik.belirsizlik]\noteleme = 0.30                # [T] 04 §2.5 önerisi\ndonme = 1.00                  # [T]",
                          "");
        y = y ? degistir(*y, "ruzgar_direnc_katsayisi = 1.0", "ruzgar_direnc_katsayisi = 1.0\nbelirsizlik = { oteleme = 0.30, donme = 1.00 }") : y;
        DOGRULA(d, y.has_value(), "satır içi tablo dönüşümü uygulanamadı");
        if (y) {
            k.dosyalar[kArac] = *y;
            const Ozetler o = ozetle(k);
            DOGRULA(d, o.gecerli && o.davranis == o0.davranis, "araçta tablo → satır içi tablo: özet aynı olmalı");
        }
    }
    return d.hata;
}

int test_yeniden_yukleme(const TestBaglami& tb)
{
    Denetci d;
    const std::string altin = dosya_oku(tb.kok + "/scenarios/ornek-havuz.cozumlenmis.json");
    const std::string ozet = sha256_hex(altin);
    {
        const YenidenYukleme y = cozumlenmis_yukle(altin, ozet);
        DOGRULA(d, y.tamam && y.cozumlenmis.has_value(), "altın dosya yeniden yüklenmeli");
        DOGRULA(d, y.cozumlenmis && kanonik_json(y.cozumlenmis->agac()) == altin, "yeniden yüklenen ağaç aynı baytları üretmeli");
        const CozumSonucu s = senaryo_coz(temel_kaynak(tb), kSenaryo);
        DOGRULA(d, y.cozumlenmis && s.cozumlenmis && esit(y.cozumlenmis->agac(), s.cozumlenmis->agac()), "çözümleyicinin ağacına eşit");
    }
    // Bir bayt değişince ret (her konumda değil: sabit adımlarla).
    for (std::size_t i = 0; i < altin.size(); i += 97) {
        std::string b = altin;
        b[i] = static_cast<char>(b[i] ^ 0x01);
        const YenidenYukleme y = cozumlenmis_yukle(b, ozet);
        DOGRULA(d, !y.tamam && !y.bulgular.empty() && y.bulgular[0].kural == "A5-K02", "değişen bayt reddedilmeli: " + std::to_string(i));
    }
    // Özeti tutan ama kanonik olmayan / şemaya uymayan baytlar.
    {
        const std::string bosluklu = altin + " ";
        const YenidenYukleme y = cozumlenmis_yukle(bosluklu, sha256_hex(bosluklu));
        DOGRULA(d, !y.tamam && y.bulgular[0].kural == "A5-K02", "kanonik olmayan baytlar reddedilmeli");
        const std::string eksik = "{\"meta\":{\"tur\":\"cozumlenmis_senaryo\"}}";
        const YenidenYukleme e = cozumlenmis_yukle(eksik, sha256_hex(eksik));
        DOGRULA(d, !e.tamam && bulgu_ozeti(DogrulamaRaporu{false, std::nullopt, {}, e.bulgular}).find("A5-K05") != std::string::npos,
                "şemaya uymayan çözümlenmiş senaryo reddedilmeli");
        const std::string tur = "{\"meta\":{\"tur\":\"senaryo\"}}";
        const YenidenYukleme t = cozumlenmis_yukle(tur, sha256_hex(tur));
        DOGRULA(d, !t.tamam && t.bulgular[0].kural == "A5-K02", "yanlış tür reddedilmeli");
        const std::string yinelenen = "{\"a\":1,\"a\":2}";
        const YenidenYukleme r = cozumlenmis_yukle(yinelenen, sha256_hex(yinelenen));
        DOGRULA(d, !r.tamam, "yinelenen anahtar reddedilmeli");
    }
    return d.hata;
}

int test_rapor_bicimi(const TestBaglami& tb)
{
    Denetci d;
    BellekKaynagi k = temel_kaynak(tb);
    // Aşama 1'de iki dosyada birden çok hata (A5-K91: ilk hatada durulmaz).
    k.dosyalar[kSenaryo] = *degistir(k.dosyalar[kSenaryo], "derinlik = 3.0 ", "derinlik = -3.0 ");
    k.dosyalar[kSenaryo] = *degistir(k.dosyalar[kSenaryo], "ana_tohum = 1", "ana_tohum = 1\n\"x_a b\" = 1");
    k.dosyalar[kSenaryo] = *degistir(k.dosyalar[kSenaryo], "kimlik = \"auto\"", "kimlik = \"auto\"\nsure_ns = 3");
    k.dosyalar[kArac] = *degistir(k.dosyalar[kArac], "kutle = 13.5 ", "kutle = -13.5 ");
    const CozumSonucu s = senaryo_coz(k, kSenaryo);
    const auto& r = s.rapor;
    DOGRULA(d, !r.gecerli && !r.davranis_ozeti, "geçersiz rapor özetsiz olmalı");
    DOGRULA(d, bulgu_bul(r, "A5-K10", "ortam.derinlik") && bulgu_bul(r, "A5-K10", "arac.kutle.kutle") &&
                   bulgu_bul(r, "A5-K109", "olaylar[1]"),
            "aşama 1'in bütün hataları toplanmalı\n" + bulgu_ozeti(r));
    DOGRULA(d, bulgu_bul(r, "A5-K07", "rastgelelik.\"x_a b\"") != nullptr, "çıplak olmayan anahtar tırnaklı yazılmalı (A5-K93)");
    // Sıralama: (dosya, satir, yol, kural)
    bool sirali = true;
    for (std::size_t i = 1; i < r.bulgular.size(); ++i) {
        const auto& a = r.bulgular[i - 1];
        const auto& b = r.bulgular[i];
        if (a.dosya != b.dosya) {
            sirali = sirali && a.dosya < b.dosya;
        } else if (a.satir.value_or(0) != b.satir.value_or(0)) {
            sirali = sirali && a.satir.value_or(0) < b.satir.value_or(0);
        }
    }
    DOGRULA(d, sirali, "bulgular (dosya, satir, yol, kural) sırasında olmalı");
    // JSON biçimi (§3.14)
    const nlohmann::json j = nlohmann::json::parse(r.json());
    DOGRULA(d, j.size() == 4 && j.contains("gecerli") && j["davranis_ozeti"].is_null() && j["kaynak_ozetleri"].size() == 2 &&
                   j["bulgular"].size() == r.bulgular.size(),
            "rapor alanları");
    for (const auto& b : j["bulgular"]) {
        const std::string kural = b["kural"].get<std::string>();
        DOGRULA(d, b.size() == 7 && kural.rfind("A5-K", 0) == 0 && kural.size() >= 6 && kural.size() <= 7 &&
                       (b["onem"] == "hata" || b["onem"] == "uyari") && (b["asama"] == 1 || b["asama"] == 2) &&
                       (b["satir"].is_null() || b["satir"].is_number_integer()),
                "bulgu alanları: " + b.dump());
    }
    // Aşama 2 bulgusunda satır ve dosya (birleşmiş araçtan gelen değer araç dosyasında).
    {
        BellekKaynagi k2 = temel_kaynak(tb);
        k2.dosyalar[kArac] = *degistir(k2.dosyalar[kArac], "pwm_min_us = 1100", "pwm_min_us = 1480");
        const CozumSonucu s2 = senaryo_coz(k2, kSenaryo);
        const Bulgu* b = bulgu_bul(s2.rapor, "A5-K31", "arac.esc");
        DOGRULA(d, b && b->asama == 2 && b->dosya == kArac && b->satir == satir_bul(k2.dosyalar[kArac], "pwm_notr_us"),
                "aşama 2 bulgusu araç dosyasının satırıyla\n" + bulgu_ozeti(s2.rapor));
        // Senaryonun geçersiz kıldığı değer senaryo dosyasının satırıyla.
        BellekKaynagi k3 = temel_kaynak(tb);
        k3.dosyalar[kSenaryo] += "\n[arac.esc]\npwm_min_us = 1480\n";
        const CozumSonucu s3 = senaryo_coz(k3, kSenaryo);
        const Bulgu* b3 = bulgu_bul(s3.rapor, "A5-K31", "arac.esc");
        DOGRULA(d, b3 && b3->dosya == kArac, "K31 araç tablosunda (pwm_notr_us konumu)\n" + bulgu_ozeti(s3.rapor));
    }
    return d.hata;
}

}  // namespace ida::senaryo::test
