// test_katalog.cpp — üretilen tabloların katalogla ve sözleşme belgesiyle eşleşmesi (A6-K07; A6 §3.3,
// §3.4), ortak yüklerde A3 kodlayıcısıyla bayt eşitliği (W5-K36) ve tablo güdümlü çözücünün üretilen
// kodlayıcılarla gidiş-dönüşü (A6-K08).
#include "test_destek.hpp"

#include "ida/kayit/coz.hpp"
#include "sinama_uretim.hpp"

#include <nlohmann/json.hpp>

#include <set>
#include <sstream>

namespace ida::kayit::test {

namespace {

using nlohmann::json;

json katalog_oku(Denetci& d)
{
    const std::vector<std::uint8_t> b = dosya_oku(IDA_A6_KATALOG_YOLU);
    DOGRULA(d, !b.empty(), "katalog dosyası okunamadı");
    return b.empty() ? json::object() : json::parse(b.begin(), b.end());
}

std::string_view tip_adi(a3::Tip t)
{
    switch (t) {
    case a3::Tip::u8:
        return "u8";
    case a3::Tip::u16:
        return "u16";
    case a3::Tip::u32:
        return "u32";
    case a3::Tip::u64:
        return "u64";
    case a3::Tip::i32:
        return "i32";
    case a3::Tip::i64:
        return "i64";
    case a3::Tip::f32:
        return "f32";
    case a3::Tip::f64:
        return "f64";
    case a3::Tip::metin:
        return "metin";
    case a3::Tip::bayt_dizisi:
        return "bayt_dizisi";
    case a3::Tip::dizi:
        return "dizi";
    }
    return "?";
}

int yok_kodu(const json& alan)
{
    if (!alan.contains("yok_degeri")) {
        return 0;
    }
    const std::string y = alan["yok_degeri"].get<std::string>();
    static const std::map<std::string, int> kodlar = {{"NaN", 1},  {"INT64_MIN", 2}, {"2^64-1", 3}, {"0xFFFFFFFF", 4},
                                                      {"255", 5},  {"-1", 6},        {"0", 7},      {"0xFFFF", 8}};
    const auto it = kodlar.find(y);
    return it == kodlar.end() ? -1 : it->second;
}

void alanlari_karsilastir(Denetci& d, const std::string& yol, const a3::AlanTanimi* alanlar, std::size_t n,
                          const json& beklenen, std::set<std::string>& tipler)
{
    DOGRULA(d, beklenen.is_array() && beklenen.size() == n, yol + ": alan sayısı");
    if (!beklenen.is_array() || beklenen.size() != n) {
        return;
    }
    for (std::size_t i = 0; i < n; ++i) {
        const a3::AlanTanimi& a = alanlar[i];
        const json& b = beklenen[i];
        const std::string ad = yol + "." + std::string(a.ad);
        DOGRULA(d, a.ad == b["ad"].get<std::string>(), ad + ": ad ve sıra");
        DOGRULA(d, tip_adi(a.tip) == b["tip"].get<std::string>(), ad + ": tip");
        tipler.insert(std::string(tip_adi(a.tip)));
        DOGRULA(d, a.adet == (b.contains("adet") ? b["adet"].get<int>() : 0), ad + ": adet");
        if (b.contains("adet")) {
            tipler.insert("adet");
        }
        DOGRULA(d, a.yok == yok_kodu(b), ad + ": yok değeri");
        if (b.contains("sayac")) {
            DOGRULA(d, tip_adi(a.sayac) == b["sayac"].get<std::string>(), ad + ": sayaç");
            DOGRULA(d, a.azami == b["azami"].get<std::uint32_t>(), ad + ": azami");
        }
        if (b.contains("degerler")) {
            DOGRULA(d, a.deger_sayisi == b["degerler"].size(), ad + ": değer sayısı");
            for (std::uint16_t k = 0; k < a.deger_sayisi; ++k) {
                const std::string kod = std::to_string(a.degerler[k].kod);
                DOGRULA(d, b["degerler"].contains(kod) && b["degerler"][kod].get<std::string>() == a.degerler[k].ad,
                        ad + ": değer " + kod);
            }
        } else {
            DOGRULA(d, a.deger_sayisi == 0, ad + ": değer tablosu yok");
        }
        if (a.tip == a3::Tip::dizi) {
            alanlari_karsilastir(d, ad + "[]", a.oge, a.oge_sayisi, b["oge"], tipler);
        }
    }
}

std::string deger_adi_bul(const a3::YukTanimi& y, std::string_view alan, std::uint32_t kod)
{
    for (std::uint16_t i = 0; i < y.alan_sayisi; ++i) {
        if (y.alanlar[i].ad != alan) {
            continue;
        }
        for (std::uint16_t k = 0; k < y.alanlar[i].deger_sayisi; ++k) {
            if (y.alanlar[i].degerler[k].kod == kod) {
                return std::string(y.alanlar[i].degerler[k].ad);
            }
        }
    }
    return {};
}

std::vector<std::string> hucreler(const std::string& satir)
{
    std::vector<std::string> v;
    std::size_t bas = satir.find('|');
    while (bas != std::string::npos) {
        const std::size_t son = satir.find('|', bas + 1);
        if (son == std::string::npos) {
            break;
        }
        std::string h = satir.substr(bas + 1, son - bas - 1);
        const std::size_t ilk = h.find_first_not_of(' ');
        const std::size_t sonu = h.find_last_not_of(' ');
        v.push_back(ilk == std::string::npos ? std::string() : h.substr(ilk, sonu - ilk + 1));
        bas = son;
    }
    return v;
}

std::string tirnaksiz(std::string s)
{
    if (s.size() >= 2 && s.front() == '`' && s.back() == '`') {
        s = s.substr(1, s.size() - 2);
    }
    return s;
}

// Belgedeki tip yazımı: `u8`, `u16[8]`, `metin(u16, en çok 64)`, `dizi(u8, en çok 8)`; iç alanlar
// `dizi[].alan` adıyla ayrı satırdadır.
void belge_alanlari(const json& alanlar, const std::string& on_ek, std::vector<std::pair<std::string, std::string>>& c)
{
    for (const json& a : alanlar) {
        const std::string t = a["tip"].get<std::string>();
        std::string yazim = t;
        if (t == "metin" || t == "bayt_dizisi" || t == "dizi") {
            yazim += "(" + a["sayac"].get<std::string>() + ", en çok " + std::to_string(a["azami"].get<std::uint32_t>()) + ")";
        } else if (a.contains("adet")) {
            yazim += "[" + std::to_string(a["adet"].get<int>()) + "]";
        }
        const std::string ad = on_ek + a["ad"].get<std::string>();
        c.emplace_back(ad, yazim);
        if (t == "dizi") {
            belge_alanlari(a["oge"], ad + "[].", c);
        }
    }
}

}  // namespace

int test_katalog_tablosu()
{
    Denetci d;
    const json k = katalog_oku(d);
    if (d.hata != 0) {
        return d.hata;
    }
    // Gömülü katalog dosyayla bayt bayt aynı (A6-K50: ek olarak yazılan budur).
    {
        const std::vector<std::uint8_t> dosya = dosya_oku(IDA_A6_KATALOG_YOLU);
        const auto g = katalog_baytlari();
        DOGRULA(d, dosya.size() == g.size() && std::equal(dosya.begin(), dosya.end(), g.begin()),
                "gömülü katalog dosyayla aynı");
    }
    const json& soz = k["x_ida_sozlesme"];
    DOGRULA(d, kA6Surumu == soz["surum"].get<std::string>(), "a6 sürümü");
    DOGRULA(d, kMcapProfili == soz["mcap"]["profil"].get<std::string>() && kMcapProfili == "ida", "profil");
    DOGRULA(d, kIletiKodlamasi == soz["mcap"]["ileti_kodlamasi"].get<std::string>() && kIletiKodlamasi == "ida-ikili-v1",
            "ileti kodlaması");
    DOGRULA(d, kSemaKodlamasi == soz["mcap"]["sema_kodlamasi"].get<std::string>() && kSemaKodlamasi == "ida-yuk-v1",
            "şema kodlaması");
    DOGRULA(d, kParcaHedefiBayt == soz["mcap"]["parca_hedefi_bayt"].get<std::uint64_t>() && kParcaHedefiBayt == 1048576u,
            "parça hedefi 1 MiB");
    DOGRULA(d, kParcaKapatmaNs == soz["mcap"]["parca_kapatma_duvar_ms"].get<std::int64_t>() * 1000000 &&
                   kParcaKapatmaNs == 1000000000,
            "parça kapatma 1 s");

    // Kanallar: `kamera/kare` dışında hepsi, katalog sırasıyla (W5-K37).
    std::vector<json> beklenen;
    for (const json& kn : k["x_ida_kanallar"]) {
        if (kn["ad"].get<std::string>() != "kamera/kare") {
            beklenen.push_back(kn);
        }
    }
    const auto tablo = kanallar();
    DOGRULA(d, tablo.size() == beklenen.size() && tablo.size() == kKanalSayisi && k["x_ida_kanallar"].size() == tablo.size() + 1,
            "kanal sayısı: " + std::to_string(tablo.size()));
    DOGRULA(d, kanal_bul("kamera/kare") == nullptr, "kamera/kare bu dilimde yok");
    std::set<std::string> tipler;
    std::set<std::string> sema_adlari;
    if (tablo.size() == beklenen.size()) {
        for (std::size_t i = 0; i < tablo.size(); ++i) {
            const KanalTanimi& t = tablo[i];
            const json& b = beklenen[i];
            const std::string ad = b["ad"].get<std::string>();
            DOGRULA(d, t.ad == ad && t.indeks == i, ad + ": ad ve sıra");
            DOGRULA(d, t.kanal_surumu == b["kanal_surumu"].get<int>(), ad + ": kanal_surumu");
            DOGRULA(d, t.yuk->ad == b["yuk"].get<std::string>(), ad + ": yük");
            DOGRULA(d, t.kademe_adi == b["kademe"].get<std::string>() && kademe_adi(t.kademe) == t.kademe_adi, ad + ": kademe");
            DOGRULA(d, t.zaman_anlami == b["zaman_anlami"].get<std::string>(), ad + ": zaman_anlami");
            DOGRULA(d, t.a3_ortak == b["a3_ortak"].get<bool>(), ad + ": a3_ortak");
            std::uint8_t bit = 0;
            for (const json& dz : b["tekrar_duzeyi"]) {
                const std::string s = dz.get<std::string>();
                bit = static_cast<std::uint8_t>(bit | (s == "izleme"   ? tekrar::izleme
                                                       : s == "fizik"  ? tekrar::fizik
                                                       : s == "girdi"  ? tekrar::girdi
                                                                        : tekrar::yeniden_kurma));
            }
            DOGRULA(d, t.tekrar_duzeyleri == bit, ad + ": tekrar düzeyleri");
            DOGRULA(d, (t.kosul == KanalKosulu::yok) == b["kosul"].is_null(), ad + ": koşul");
            // Şema kaydı (A6 §3.2, A6-K07): ad ve kanonik JSON.
            const SemaTanimi& s = semalar()[t.sema];
            const std::string yuk = b["yuk"].get<std::string>();
            DOGRULA(d, s.ad == "ida." + yuk + ".v" + std::to_string(b["kanal_surumu"].get<int>()), ad + ": şema adı");
            DOGRULA(d, s.yuk == t.yuk && s.kanal_surumu == t.kanal_surumu, ad + ": şema ↔ yük");
            // nlohmann: anahtarlar sıralı, boşluksuz, ASCII dışı ham UTF-8 (üretecin Python çıktısından
            // bağımsız ikinci bir gerçekleştirim).
            DOGRULA(d, s.veri == k["x_ida_yukler"][yuk].dump(), ad + ": şema `data`'sı kanonik JSON ile bayt bayt aynı");
            sema_adlari.insert(std::string(s.ad));
            alanlari_karsilastir(d, yuk, t.yuk->alanlar, t.yuk->alan_sayisi, k["x_ida_yukler"][yuk], tipler);
        }
    }
    DOGRULA(d, sema_adlari.size() == semalar().size(), "her (yük, kanal_surumu) için tek şema");
    DOGRULA(d, yukler().size() == k["x_ida_yukler"].size() - 1, "yük sayısı (yuk_kamera_kare hariç)");
    for (const char* t : {"u8", "u16", "u32", "u64", "i64", "f32", "f64", "metin", "bayt_dizisi", "dizi", "adet"}) {
        DOGRULA(d, tipler.count(t) == 1, std::string("katalogda kullanılan tel tipi: ") + t);
    }
    // Kanal kimlikleri tablo sırasıdır.
    DOGRULA(d, tablo[static_cast<std::size_t>(Kanal::kayit_eksik)].ad == "kayit/eksik", "Kanal::kayit_eksik");
    DOGRULA(d, tablo[static_cast<std::size_t>(Kanal::fizik_durum)].ad == "fizik/durum", "Kanal::fizik_durum");
    DOGRULA(d, tablo[static_cast<std::size_t>(Kanal::hil_bariyer)].ad == "hil/bariyer", "Kanal::hil_bariyer");
    DOGRULA(d, tablo[static_cast<std::size_t>(Kanal::sonar_gercek_ping360)].ad == "sonar_gercek/ping360", "son kanal");

    // Üst veri anahtarları ve ekler.
    const auto ust_veri_karsilastir = [&](const char* kayit, std::span<const UstVeriAnahtari> t) {
        for (const json& u : k["x_ida_ust_veri"]) {
            if (u["kayit"].get<std::string>() != kayit) {
                continue;
            }
            const json& a = u["anahtarlar"];
            const json& sema = k["$defs"][std::string("ust_veri_") + kayit];
            DOGRULA(d, a.size() == t.size(), std::string(kayit) + ": anahtar sayısı");
            for (std::size_t i = 0; i < t.size() && i < a.size(); ++i) {
                const std::string ad = a[i]["ad"].get<std::string>();
                DOGRULA(d, t[i].ad == ad, std::string(kayit) + "." + ad + ": ad ve sıra");
                DOGRULA(d, t[i].zorunlu == (a[i]["zorunlu"].get<std::string>() == "evet"), std::string(kayit) + "." + ad + ": zorunlu");
                bool sema_zorunlu = false;
                if (sema.contains("required")) {
                    for (const json& r : sema["required"]) {
                        sema_zorunlu = sema_zorunlu || r.get<std::string>() == ad;
                    }
                }
                DOGRULA(d, t[i].zorunlu == sema_zorunlu, std::string(kayit) + "." + ad + ": $defs required");
                DOGRULA(d, (t[i].bicim == UstVeriBicimi::onluk_u32) == sema["properties"][ad].contains("pattern"),
                        std::string(kayit) + "." + ad + ": biçim");
            }
            return;
        }
        DOGRULA(d, false, std::string(kayit) + ": katalogda yok");
    };
    ust_veri_karsilastir("ida_kayit", acilis_anahtarlari());
    ust_veri_karsilastir("ida_kayit_guncelleme", guncelleme_anahtarlari());
    ust_veri_karsilastir("ida_kayit_kapanis", kapanis_anahtarlari());
    DOGRULA(d, ekler().size() == k["x_ida_ekler"].size(), "ek sayısı");
    for (std::size_t i = 0; i < ekler().size() && i < k["x_ida_ekler"].size(); ++i) {
        const json& e = k["x_ida_ekler"][i];
        DOGRULA(d, ekler()[i].ad == e["ad"].get<std::string>() && ekler()[i].ortam_turu == e["media_type"].get<std::string>() &&
                       ekler()[i].zorunlu == (e["zorunlu"].get<std::string>() == "evet"),
                "ek: " + e["ad"].get<std::string>());
    }
    // `AcilisUstVerisi` alanları anahtar tablosuyla aynı sırada.
    {
        AcilisUstVerisi u;
        const auto cift = ust_veri_ciftleri(u);
        DOGRULA(d, cift.size() == acilis_anahtarlari().size(), "AcilisUstVerisi alan sayısı");
        for (std::size_t i = 0; i < cift.size() && i < acilis_anahtarlari().size(); ++i) {
            DOGRULA(d, cift[i].first == acilis_anahtarlari()[i].ad, "AcilisUstVerisi alan sırası");
            DOGRULA(d, ust_veri_alani(u, cift[i].first) != nullptr, "ust_veri_alani");
        }
        DOGRULA(d, ust_veri_alani(u, "yok_boyle_anahtar") == nullptr, "bilinmeyen anahtar");
    }
    // Kademe süzgeçlerinin dayandığı kodlar (yazici.hpp'deki sabitler).
    DOGRULA(d, deger_adi_bul(*kanal_bul("hil/bariyer")->yuk, "karar", 0) == "zamaninda", "hil/bariyer karar 0 = zamaninda");
    DOGRULA(d, deger_adi_bul(*kanal_bul("algilama/parti")->yuk, "nihai_durum", 0) == "OK", "algilama/parti nihai_durum 0 = OK");
    DOGRULA(d, deger_adi_bul(*kanal_bul("algilama/parti")->yuk, "ret_nedeni", 0) == "yok", "algilama/parti ret_nedeni 0 = yok");
    DOGRULA(d, deger_adi_bul(*kanal_bul("algilama/parti")->yuk, "gec", 0) == "hayir", "algilama/parti gec 0 = hayir");
    DOGRULA(d, deger_adi_bul(*kanal_bul("kayit/eksik")->yuk, "neden", 0) == "halka_tasmasi" &&
                   deger_adi_bul(*kanal_bul("kayit/eksik")->yuk, "neden", 1) == "yazma_hatasi" &&
                   deger_adi_bul(*kanal_bul("kayit/eksik")->yuk, "neden", 2) == "disk_dolu",
            "kayit/eksik neden kodları");
    // Kip listesi (A6 §3.5) ve çekirdeği PC'de çağıran kipler.
    for (const char* kip : {"sil", "hil_1x", "hil_bariyer", "tekrar_fizik", "tekrar_girdi_sil", "tekrar_yeniden_kurma"}) {
        DOGRULA(d, kip_gecerli(kip), std::string("kip: ") + kip);
    }
    DOGRULA(d, !kip_gecerli("") && !kip_gecerli("hil"), "geçersiz kip");
    DOGRULA(d, kip_cekirdek_pc("sil") && kip_cekirdek_pc("tekrar_girdi_sil") && kip_cekirdek_pc("tekrar_yeniden_kurma") &&
                   !kip_cekirdek_pc("hil_1x") && !kip_cekirdek_pc("hil_bariyer") && !kip_cekirdek_pc("tekrar_fizik"),
            "çekirdeği PC'de çağıran kipler");
    return d.hata;
}

// Sözleşme belgesindeki Tablo 3.3 ve §3.4 yük tabloları katalogla aynı mı (çelişkide belge geçerlidir;
// bu test çelişkiyi görünür kılar).
int test_belge_tablosu()
{
    Denetci d;
    const json k = katalog_oku(d);
    const std::vector<std::uint8_t> belge = dosya_oku(IDA_A6_BELGE_YOLU);
    DOGRULA(d, !belge.empty(), "sözleşme belgesi okunamadı");
    if (d.hata != 0) {
        return d.hata;
    }
    std::istringstream akis(std::string(belge.begin(), belge.end()));
    std::string satir;
    bool tablo33 = false;
    std::size_t kanal_satiri = 0;
    std::string bolum;
    std::map<std::string, std::vector<std::pair<std::string, std::string>>> yuk_tablolari;
    while (std::getline(akis, satir)) {
        if (!satir.empty() && satir.back() == '\r') {
            satir.pop_back();
        }
        if (satir.rfind("| Ad | Yük | Kademe |", 0) == 0) {
            tablo33 = true;
            continue;
        }
        if (tablo33) {
            if (satir.rfind("|---", 0) == 0) {
                continue;
            }
            if (satir.empty() || satir[0] != '|') {
                tablo33 = false;
            } else {
                const std::vector<std::string> h = hucreler(satir);
                DOGRULA(d, h.size() == 8 && kanal_satiri < k["x_ida_kanallar"].size(), "Tablo 3.3 satır biçimi");
                if (h.size() == 8 && kanal_satiri < k["x_ida_kanallar"].size()) {
                    const json& kn = k["x_ida_kanallar"][kanal_satiri];
                    const std::string ad = tirnaksiz(h[0]);
                    DOGRULA(d, ad == kn["ad"].get<std::string>(), "Tablo 3.3 ad ve sıra: " + ad);
                    DOGRULA(d, tirnaksiz(h[1]) == kn["yuk"].get<std::string>(), ad + ": yük");
                    DOGRULA(d, h[2].substr(0, h[2].find(' ')) == kn["kademe"].get<std::string>(), ad + ": kademe");
                    DOGRULA(d, h[3] == kn["kayit_hizi"].get<std::string>(), ad + ": ne zaman yazılır");
                    DOGRULA(d, h[4] == kn["zaman_anlami"].get<std::string>(), ad + ": log_time anlamı");
                    std::string tekrar_metni;
                    for (const json& t : kn["tekrar_duzeyi"]) {
                        tekrar_metni += (tekrar_metni.empty() ? "" : ", ") + t.get<std::string>();
                    }
                    DOGRULA(d, h[5] == tekrar_metni, ad + ": tekrar");
                    DOGRULA(d, (h[6] == "evet") == kn["a3_ortak"].get<bool>(), ad + ": A3 ortak");
                }
                ++kanal_satiri;
                continue;
            }
        }
        if (satir.rfind("#### `yuk_", 0) == 0) {
            bolum = satir.substr(6, satir.find('`', 6) - 6);
            yuk_tablolari[bolum];
            continue;
        }
        if (satir.rfind("## ", 0) == 0 || satir.rfind("### ", 0) == 0) {
            bolum.clear();
        }
        if (!bolum.empty() && satir.rfind("| `", 0) == 0) {
            const std::vector<std::string> h = hucreler(satir);
            if (h.size() >= 2) {
                yuk_tablolari[bolum].emplace_back(tirnaksiz(h[0]), h[1]);
            }
        }
    }
    DOGRULA(d, kanal_satiri == k["x_ida_kanallar"].size(), "Tablo 3.3 satır sayısı: " + std::to_string(kanal_satiri));
    // §3.4: A6'ya özgü yüklerin tabloları (ortak yüklerinki A3 §3.4'tedir).
    DOGRULA(d, yuk_tablolari.size() == 20, "§3.4 yük tablosu sayısı: " + std::to_string(yuk_tablolari.size()));
    for (const auto& [yuk, satirlar] : yuk_tablolari) {
        DOGRULA(d, k["x_ida_yukler"].contains(yuk), "§3.4 yükü katalogda: " + yuk);
        if (!k["x_ida_yukler"].contains(yuk)) {
            continue;
        }
        std::vector<std::pair<std::string, std::string>> beklenen;
        belge_alanlari(k["x_ida_yukler"][yuk], "", beklenen);
        DOGRULA(d, satirlar == beklenen, "§3.4 `" + yuk + "` alan adları, sırası ve tipleri katalogla aynı");
    }
    return d.hata;
}

int test_kodlayici_ortak()
{
    Denetci d;
    // W5-K36: A3 ile ortak her yük için A3 kodlayıcısı (ürünün kullandığı) ile A6 kataloğundan aynı
    // üretim düzeniyle üretilmiş kodlayıcı aynı değerlerde aynı baytları verir.
    const std::vector<std::string_view> ortaklar = ortak_yuk_adlari();
    DOGRULA(d, ortaklar.size() == 24, "ortak yük sayısı: " + std::to_string(ortaklar.size()));
    std::size_t toplam_bayt = 0;
    std::size_t vaka = 0;
    for (const std::string_view yuk : ortaklar) {
        for (std::uint64_t tohum = 1; tohum <= 300; ++tohum) {
            bool ortak = false;
            std::size_t bayt = 0;
            const bool esit = ortak_kodlayici_esit(yuk, tohum, ortak, bayt);
            DOGRULA(d, ortak && esit, std::string(yuk) + ": tohum " + std::to_string(tohum) + " baytlar farklı");
            toplam_bayt += bayt;
            ++vaka;
        }
    }
    {
        bool ortak = true;
        std::size_t bayt = 0;
        DOGRULA(d, !ortak_kodlayici_esit("yuk_hil_bariyer", 1, ortak, bayt) && !ortak, "A6'ya özgü yük ortak değil");
    }
    // Ortak kanalların yük tipi A3 yapısıdır (yeniden üretilmez).
    static_assert(std::is_same_v<KanalYuku<Kanal::fizik_durum>::tip, a3::yuk_fizik_durum>);
    static_assert(std::is_same_v<KanalYuku<Kanal::olay>::tip, a3::yuk_olay>);
    static_assert(std::is_same_v<KanalYuku<Kanal::sensor_temel_imu>::tip, a3::yuk_imu>);
    static_assert(std::is_same_v<KanalYuku<Kanal::hil_bariyer>::tip, yuk_hil_bariyer>);
    // Katalogda ortak işaretli her kanalın yükü ortak yük listesindedir.
    for (const KanalTanimi& k : kanallar()) {
        if (k.a3_ortak) {
            DOGRULA(d, std::find(ortaklar.begin(), ortaklar.end(), k.yuk->ad) != ortaklar.end(),
                    std::string(k.ad) + ": ortak kanalın yükü A3 yapısı");
        }
    }
    std::printf("  ortak kodlayıcı: %zu yük, %zu vaka, %zu bayt karşılaştırıldı\n", ortaklar.size(), vaka, toplam_bayt);
    return d.hata;
}

int test_coz_gidis_donus()
{
    Denetci d;
    // Her kanalın yükü: üretilen kodlayıcının baytları, tablo güdümlü (bağımsız) çözücüyle aynı
    // değerlere çözülür; değer dökümü yapının dökümüyle aynıdır.
    std::size_t vaka = 0;
    std::size_t bayt = 0;
    for (const KanalTanimi& k : kanallar()) {
        for (std::uint64_t tohum = 1; tohum <= 200; ++tohum) {
            const Ornek o = ornek_uret(static_cast<Kanal>(k.indeks), tohum * 7919u + k.indeks);
            DOGRULA(d, o.json != "KODLANAMADI", std::string(k.ad) + ": örnek kodlanamadı");
            std::string json;
            const CozmeSonucu s = json_dok(*k.yuk, o.bayt, json);
            DOGRULA(d, s.tamam && s.eksik_alan == 0 && s.fazla_bayt == 0, std::string(k.ad) + ": çözülemedi");
            DOGRULA(d, json == o.json, std::string(k.ad) + ": değerler farklı\n    kodlanan: " + o.json + "\n    çözülen : " + json);
            ++vaka;
            bayt += o.bayt.size();
            // Her kesik önek ya çözülemez ya da yalnız alan sınırında "eksik alan" olarak çözülür;
            // hiçbir durumda sınır dışına okunmaz (ASan altında anlamlı).
            if (tohum <= 3) {
                for (std::size_t n = 0; n < o.bayt.size(); ++n) {
                    const CozmeSonucu ks = json_dok(*k.yuk, std::span<const std::uint8_t>(o.bayt.data(), n), json);
                    DOGRULA(d, !ks.tamam || ks.eksik_alan > 0, std::string(k.ad) + ": kesik yük tam çözüldü");
                }
            }
        }
    }
    std::printf("  gidiş-dönüş: %zu kanal, %zu vaka, %zu bayt\n", kanallar().size(), vaka, bayt);
    return d.hata;
}

}  // namespace ida::kayit::test
