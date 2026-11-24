// A5 JSON şemasının (scenarios/sema/a5.schema.json) yorumlayıcısı.
//
// Şema derleme zamanında ikiliye gömülür ve çalışma anında yorumlanır (veri güdümlü; kural
// başına elle yazılmış denetim yok). İlk kullanımda JSON şema ağacı bir kez düğüm yapılarına
// derlenir (anahtar sözcükler, sınırlar, derlenmiş düzenli ifadeler, çözülmüş `$ref`'ler);
// doğrulama bu düğümler üzerinde koşar. Kullanılan JSON Schema 2020-12 alt kümesi: type,
// properties, patternProperties, additionalProperties, required, items, minItems, maxItems,
// minProperties, maxProperties, propertyNames, enum, const, minimum, maximum,
// exclusiveMinimum, exclusiveMaximum, pattern, maxLength, $ref, allOf, anyOf, oneOf, not,
// if/then/else. Şemada başka bir anahtar sözcük bulunursa testi (sim.senaryo.sema_sozcukleri)
// düşer.
//
// Bulgular A5 kural numarasına `kEslemeler` tablosuyla eşlenir; tabloda olmayan durumlar
// anahtar sözcüğe göre: required → K05, additionalProperties → K06, type → K09,
// propertyNames → K08, öteki her sınır → K10 (A5 §5).
//
// Sözleşmeden gelen sapmalar: (1) min/maxProperties `x_` anahtarlarını saymaz (A5-K07);
// (2) TOML kipinde `integer` yalnız TOML tamsayısını kabul eder (A5-K09); (3) `yonelim`
// dizilerinin eleman sınırları (±1,0000001) uygulanmaz, birimlik A5-K32 ile denetlenir
// (şema belgeden dar; belge geçerlidir).
#include "ic.hpp"

#include <nlohmann/json.hpp>

#include <cmath>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <regex>

namespace ida::senaryo {
namespace gomulu {
extern const unsigned char a5_sema[];
extern const std::size_t a5_sema_boyu;
}  // namespace gomulu

namespace ic {

using json = nlohmann::json;

std::string_view sema_metni()
{
    return {reinterpret_cast<const char*>(gomulu::a5_sema), gomulu::a5_sema_boyu};
}

namespace {

struct Esleme {
    std::string_view desen;   // Yol::desen_metni() üzerinde; '*' bir parça, '**' kalan her şey
    std::string_view sozcuk;  // '|' ile ayrılmış anahtar sözcükler
    std::string_view kural;
};

// Sözleşmenin daha özgül kural numarası verdiği yapısal durumlar.
constexpr Esleme kEslemeler[] = {
    {"meta.sema_surumu", "pattern", "A5-K03"},
    {"arac.dosya", "pattern|additionalProperties|type", "A5-K12"},
    {"olaylar[]", "oneOf", "A5-K61"},
    {"olaylar[]", "not", "A5-K109"},
    {"arac.sensorler.*", "not", "A5-K41"},
    {"arac.torpido.**", "const", "A5-K29"},
    {"arac.firlaticilar.**", "const", "A5-K29"},
    {"kalibrasyon.malzeme.*.**",
     "additionalProperties|enum|minimum|maximum|exclusiveMinimum|exclusiveMaximum|minItems|maxItems",
     "A5-K18"},
    {"calistirma", "not", "A5-K76"},
    {"kalite", "not", "A5-K98"},
};

std::vector<std::string_view> bol(std::string_view s, char ayrac)
{
    std::vector<std::string_view> o;
    std::size_t bas = 0;
    for (;;) {
        const std::size_t i = s.find(ayrac, bas);
        o.push_back(s.substr(bas, i == std::string_view::npos ? std::string_view::npos : i - bas));
        if (i == std::string_view::npos) {
            return o;
        }
        bas = i + 1;
    }
}

bool desen_uyar(std::string_view desen, std::string_view yol)
{
    const auto d = bol(desen, '.');
    const auto y = bol(yol, '.');
    for (std::size_t i = 0; i < d.size(); ++i) {
        if (d[i] == "**") {
            return y.size() > i;
        }
        if (i >= y.size()) {
            return false;
        }
        if (d[i] != "*" && d[i] != y[i]) {
            return false;
        }
    }
    return d.size() == y.size();
}

std::string kural_belirle(std::string_view sozcuk, const Yol& yol)
{
    const std::string y = yol.desen_metni();
    for (const auto& e : kEslemeler) {
        if (!desen_uyar(e.desen, y)) {
            continue;
        }
        for (const auto s : bol(e.sozcuk, '|')) {
            if (s == sozcuk) {
                return std::string(e.kural);
            }
        }
    }
    if (sozcuk == "required") return "A5-K05";
    if (sozcuk == "additionalProperties") return "A5-K06";
    if (sozcuk == "type") return "A5-K09";
    if (sozcuk == "propertyNames") return "A5-K08";
    return "A5-K10";
}

// JSON Schema `maxLength` kod noktası sayar.
std::size_t kod_noktasi_sayisi(std::string_view s)
{
    std::size_t n = 0;
    for (const char c : s) {
        if ((static_cast<unsigned char>(c) & 0xC0u) != 0x80u) {
            ++n;
        }
    }
    return n;
}

bool json_esit(const json& j, const Deger& d)
{
    if (j.is_number()) {
        return d.sayisal_mi() && j.get<double>() == d.cift();
    }
    if (j.is_string()) {
        return d.tur == Tur::dizgi && j.get_ref<const std::string&>() == d.dizgi;
    }
    if (j.is_boolean()) {
        return d.tur == Tur::mantiksal && j.get<bool>() == d.mantik;
    }
    if (j.is_null()) {
        return d.tur == Tur::bos;
    }
    if (j.is_array()) {
        if (d.tur != Tur::dizi || d.dizi.size() != j.size()) {
            return false;
        }
        for (std::size_t i = 0; i < j.size(); ++i) {
            if (!json_esit(j[i], d.dizi[i])) {
                return false;
            }
        }
        return true;
    }
    if (j.is_object()) {
        if (d.tur != Tur::tablo || d.tablo.size() != j.size()) {
            return false;
        }
        for (const auto& [k, v] : j.items()) {
            const Deger* w = d.bul(k);
            if (w == nullptr || !json_esit(v, *w)) {
                return false;
            }
        }
        return true;
    }
    return false;
}

std::string tur_adi(const Deger& d)
{
    switch (d.tur) {
    case Tur::bos: return "boş";
    case Tur::mantiksal: return "mantıksal";
    case Tur::tamsayi: return "tamsayı";
    case Tur::sayi: return "kayan noktalı sayı";
    case Tur::dizgi: return "dizgi";
    case Tur::dizi: return "dizi";
    case Tur::tablo: return "tablo";
    case Tur::gecersiz: return "geçersiz";
    }
    return "?";
}

// ------------------------------------------------------------------ derlenmiş şema
enum class Tip : std::uint8_t { yok, nesne, dizi, dizgi, mantiksal, sayi, tamsayi, bos };

// Düzenli ifade; `^x_` deseni (şemadaki en sık desen) önek denetimiyle hızlandırılır.
struct Desen {
    std::string metin;
    bool x_oneki = false;
    std::regex ifade;

    explicit Desen(std::string m) : metin(std::move(m))
    {
        x_oneki = metin == "^x_";
        if (!x_oneki) {
            ifade = std::regex(metin, std::regex::ECMAScript);
        }
    }
    bool uyar(const std::string& s) const
    {
        if (x_oneki) {
            return deneysel_mi(s);
        }
        // Şemadaki desenler kısa adlar içindir; çok uzun dizgi düzenli ifade motoruna
        // verilmez (MSVC std::regex özyinelemelidir) ve uymuyor sayılır.
        if (s.size() > 1024) {
            return false;
        }
        return std::regex_search(s, ifade);
    }
};

struct Dugum {
    Tip tip = Tip::yok;
    std::string tip_adi;
    const json* sabit = nullptr;
    const json* kume = nullptr;
    std::optional<double> en_az, en_cok, alt_haric, ust_haric;
    std::optional<std::size_t> en_az_eleman, en_cok_eleman, en_az_girdi, en_cok_girdi, en_uzun;
    std::unique_ptr<Desen> desen;
    std::vector<std::string> zorunlu;
    std::map<std::string, const Dugum*> ozellikler;
    std::vector<std::pair<std::unique_ptr<Desen>, const Dugum*>> desen_ozellikleri;
    bool ek_yasak = false;
    const Dugum* ek = nullptr;
    const Dugum* adlar = nullptr;
    const Dugum* elemanlar = nullptr;
    const Dugum* ref = nullptr;
    std::vector<const Dugum*> hepsi, biri, yalniz_biri;
    const Dugum* degil = nullptr;
    const Dugum* eger = nullptr;
    const Dugum* ise = nullptr;
    const Dugum* degilse = nullptr;
    bool her_zaman = true;   // boolean şema
    bool bool_sema = false;
};

class Derleyici {
public:
    explicit Derleyici(const json& kok) : kok_(kok) {}

    const Dugum* derle(const json& j)
    {
        if (const auto it = hazir_.find(&j); it != hazir_.end()) {
            return it->second;
        }
        auto yeni = std::make_unique<Dugum>();
        Dugum* d = yeni.get();
        hazir_[&j] = d;
        tumu_.push_back(std::move(yeni));
        if (j.is_boolean()) {
            d->bool_sema = true;
            d->her_zaman = j.get<bool>();
            return d;
        }
        if (!j.is_object()) {
            return d;
        }
        if (const auto r = j.find("$ref"); r != j.end()) {
            const std::string ref = r->get<std::string>();
            static const std::string kOnek = "#/$defs/";
            const json& hedef = ref.rfind(kOnek, 0) == 0 ? kok_.at("$defs").at(ref.substr(kOnek.size())) : kok_;
            d->ref = derle(hedef);
        }
        if (const auto t = j.find("type"); t != j.end()) {
            d->tip_adi = t->get<std::string>();
            static const std::map<std::string, Tip> kTipler = {
                {"object", Tip::nesne}, {"array", Tip::dizi},     {"string", Tip::dizgi},
                {"boolean", Tip::mantiksal}, {"number", Tip::sayi}, {"integer", Tip::tamsayi},
                {"null", Tip::bos}};
            d->tip = kTipler.at(d->tip_adi);
        }
        if (const auto c = j.find("const"); c != j.end()) d->sabit = &*c;
        if (const auto e = j.find("enum"); e != j.end()) d->kume = &*e;
        auto sayi = [&](const char* ad, std::optional<double>& o) {
            if (const auto it = j.find(ad); it != j.end()) o = it->get<double>();
        };
        sayi("minimum", d->en_az);
        sayi("maximum", d->en_cok);
        sayi("exclusiveMinimum", d->alt_haric);
        sayi("exclusiveMaximum", d->ust_haric);
        auto boy = [&](const char* ad, std::optional<std::size_t>& o) {
            if (const auto it = j.find(ad); it != j.end()) o = it->get<std::size_t>();
        };
        boy("minItems", d->en_az_eleman);
        boy("maxItems", d->en_cok_eleman);
        boy("minProperties", d->en_az_girdi);
        boy("maxProperties", d->en_cok_girdi);
        boy("maxLength", d->en_uzun);
        if (const auto p = j.find("pattern"); p != j.end()) d->desen = std::make_unique<Desen>(p->get<std::string>());
        if (const auto r = j.find("required"); r != j.end()) {
            for (const auto& x : *r) d->zorunlu.push_back(x.get<std::string>());
        }
        if (const auto p = j.find("properties"); p != j.end()) {
            for (const auto& [k, v] : p->items()) d->ozellikler[k] = derle(v);
        }
        if (const auto p = j.find("patternProperties"); p != j.end()) {
            for (const auto& [k, v] : p->items()) {
                d->desen_ozellikleri.emplace_back(std::make_unique<Desen>(k), derle(v));
            }
        }
        if (const auto a = j.find("additionalProperties"); a != j.end()) {
            if (a->is_boolean() && !a->get<bool>()) {
                d->ek_yasak = true;
            } else {
                d->ek = derle(*a);
            }
        }
        if (const auto a = j.find("propertyNames"); a != j.end()) d->adlar = derle(*a);
        if (const auto a = j.find("items"); a != j.end()) d->elemanlar = derle(*a);
        auto liste = [&](const char* ad, std::vector<const Dugum*>& o) {
            if (const auto it = j.find(ad); it != j.end()) {
                for (const auto& x : *it) o.push_back(derle(x));
            }
        };
        liste("allOf", d->hepsi);
        liste("anyOf", d->biri);
        liste("oneOf", d->yalniz_biri);
        if (const auto a = j.find("not"); a != j.end()) d->degil = derle(*a);
        if (const auto a = j.find("if"); a != j.end()) d->eger = derle(*a);
        if (const auto a = j.find("then"); a != j.end()) d->ise = derle(*a);
        if (const auto a = j.find("else"); a != j.end()) d->degilse = derle(*a);
        return d;
    }

private:
    const json& kok_;
    std::map<const json*, Dugum*> hazir_;
    std::vector<std::unique_ptr<Dugum>> tumu_;
};

struct DerlenmisSema {
    json kok;
    std::unique_ptr<Derleyici> derleyici;
    const Dugum* tumu = nullptr;
    std::map<std::string, const Dugum*> tanimlar;
};

const DerlenmisSema& derlenmis()
{
    static const DerlenmisSema s = [] {
        DerlenmisSema o;
        o.kok = json::parse(sema_metni());
        o.derleyici = std::make_unique<Derleyici>(o.kok);
        o.tumu = o.derleyici->derle(o.kok);
        for (const auto& [k, v] : o.kok.at("$defs").items()) {
            o.tanimlar[k] = o.derleyici->derle(v);
        }
        return o;
    }();
    return s;
}

// Doğrulama sırasında anahtar yolu: kopyalanmadan yığılır; Yol yalnız bulguda kurulur.
struct Iz {
    struct Adim {
        const std::string* ad;  // nullptr: dizi indisi
        std::size_t indis;
    };
    const Yol* kok = nullptr;
    std::vector<Adim> adimlar;

    Yol yap() const
    {
        Yol y = *kok;
        for (const auto& a : adimlar) {
            if (a.ad != nullptr) {
                y.parcalar.push_back(YolParcasi{*a.ad});
            } else {
                y.parcalar.push_back(YolParcasi{a.indis});
            }
        }
        return y;
    }
};

bool yonelim_dizisi(const Iz& iz)
{
    // Son iki parça: "yonelim" anahtarı ve bir dizi indisi (doğrulanan ağaçlarda `yonelim`
    // dizileri hep bir tablonun içindedir).
    const auto& a = iz.adimlar;
    return a.size() >= 2 && a.back().ad == nullptr && a[a.size() - 2].ad != nullptr &&
           *a[a.size() - 2].ad == "yonelim";
}

class Yorumlayici {
public:
    Yorumlayici(SemaKipi kip, Bulgucu* b) : kip_(kip), b_(b) {}

    bool dogrula(const Dugum& s, const Deger& v, Iz& yol)
    {
        if (v.tur == Tur::gecersiz) {
            return true;  // aşama 1'de zaten reddedildi
        }
        if (s.bool_sema) {
            return s.her_zaman;
        }
        bool tamam = true;
        if (s.ref != nullptr) {
            tamam = dogrula(*s.ref, v, yol);
            if (!tamam && sessiz()) return false;
        }
        if (s.tip != Tip::yok && !tip_uyar(s.tip, v)) {
            return bildir("type", v.konum, yol, "tip " + s.tip_adi + " olmalı (" + tur_adi(v) + " verildi)");
        }
        if (s.sabit != nullptr && !json_esit(*s.sabit, v)) {
            tamam = bildir("const", v.konum, yol, "değer sabit olmalı: " + s.sabit->dump());
            if (sessiz()) return false;
        }
        if (s.kume != nullptr) {
            bool bulundu = false;
            for (const auto& x : *s.kume) {
                bulundu = bulundu || json_esit(x, v);
            }
            if (!bulundu) {
                tamam = bildir("enum", v.konum, yol, "izinli değerlerden biri olmalı: " + s.kume->dump());
                if (sessiz()) return false;
            }
        }
        if (v.sayisal_mi() && !sayi_sinirlari(s, v, yol)) {
            tamam = false;
            if (sessiz()) return false;
        }
        if (v.tur == Tur::dizgi && !dizgi_sinirlari(s, v, yol)) {
            tamam = false;
            if (sessiz()) return false;
        }
        if (v.tur == Tur::dizi && !dizi(s, v, yol)) {
            tamam = false;
            if (sessiz()) return false;
        }
        if (v.tur == Tur::tablo && !tablo(s, v, yol)) {
            tamam = false;
            if (sessiz()) return false;
        }
        if (!bilesikler(s, v, yol)) {
            tamam = false;
        }
        return tamam;
    }

private:
    SemaKipi kip_;
    Bulgucu* b_;

    bool sessiz() const { return b_ == nullptr; }

    bool bildir(std::string_view sozcuk, Konum k, const Iz& yol, std::string ileti)
    {
        if (b_ != nullptr) {
            const Yol y = yol.yap();
            b_->hata(kural_belirle(sozcuk, y), k, y, std::move(ileti));
        }
        return false;
    }

    bool tip_uyar(Tip t, const Deger& v) const
    {
        switch (t) {
        case Tip::nesne: return v.tur == Tur::tablo;
        case Tip::dizi: return v.tur == Tur::dizi;
        case Tip::dizgi: return v.tur == Tur::dizgi;
        case Tip::mantiksal: return v.tur == Tur::mantiksal;
        case Tip::sayi: return v.sayisal_mi();
        case Tip::bos: return v.tur == Tur::bos;
        case Tip::tamsayi:
            if (v.tur == Tur::tamsayi) return true;
            return kip_ == SemaKipi::json && v.tur == Tur::sayi && std::isfinite(v.sayi) &&
                   std::floor(v.sayi) == v.sayi;
        case Tip::yok: return true;
        }
        return false;
    }

    bool sayi_sinirlari(const Dugum& s, const Deger& v, const Iz& yol)
    {
        if (yonelim_dizisi(yol)) {
            return true;  // A5-K32 geçerlidir (dosya başındaki not)
        }
        bool tamam = true;
        const double x = v.cift();
        auto sinir = [&](const char* ad, const std::optional<double>& b, bool kosul, const char* sozel) {
            if (b && !kosul) {
                tamam = bildir(ad, v.konum, yol, std::string("değer ") + sozel + " " + sayi_dizilimi(*b) + " olmalı") && tamam;
            }
        };
        sinir("minimum", s.en_az, !s.en_az || x >= *s.en_az, "≥");
        sinir("maximum", s.en_cok, !s.en_cok || x <= *s.en_cok, "≤");
        sinir("exclusiveMinimum", s.alt_haric, !s.alt_haric || x > *s.alt_haric, ">");
        sinir("exclusiveMaximum", s.ust_haric, !s.ust_haric || x < *s.ust_haric, "<");
        return tamam;
    }

    bool dizgi_sinirlari(const Dugum& s, const Deger& v, const Iz& yol)
    {
        bool tamam = true;
        if (s.en_uzun && kod_noktasi_sayisi(v.dizgi) > *s.en_uzun) {
            tamam = bildir("maxLength", v.konum, yol, "dizgi en çok " + std::to_string(*s.en_uzun) + " karakter olmalı");
        }
        if (s.desen && !s.desen->uyar(v.dizgi)) {
            tamam = bildir("pattern", v.konum, yol, "dizgi desene uymuyor: " + s.desen->metin) && tamam;
        }
        return tamam;
    }

    bool dizi(const Dugum& s, const Deger& v, Iz& yol)
    {
        bool tamam = true;
        if (s.en_az_eleman && v.dizi.size() < *s.en_az_eleman) {
            tamam = bildir("minItems", v.konum, yol, "en az " + std::to_string(*s.en_az_eleman) + " eleman olmalı");
            if (sessiz()) return false;
        }
        if (s.en_cok_eleman && v.dizi.size() > *s.en_cok_eleman) {
            tamam = bildir("maxItems", v.konum, yol, "en çok " + std::to_string(*s.en_cok_eleman) + " eleman olmalı") && tamam;
            if (sessiz()) return false;
        }
        if (s.elemanlar != nullptr) {
            for (std::size_t i = 0; i < v.dizi.size(); ++i) {
                yol.adimlar.push_back({nullptr, i});
                const bool e = dogrula(*s.elemanlar, v.dizi[i], yol);
                yol.adimlar.pop_back();
                tamam = e && tamam;
                if (!tamam && sessiz()) return false;
            }
        }
        return tamam;
    }

    bool tablo(const Dugum& s, const Deger& v, Iz& yol)
    {
        bool tamam = true;
        if (s.en_az_girdi || s.en_cok_girdi) {
            std::size_t girdi = 0;
            for (const auto& kv : v.tablo) {
                girdi += deneysel_mi(kv.first) ? 0u : 1u;
            }
            if (s.en_az_girdi && girdi < *s.en_az_girdi) {
                tamam = bildir("minProperties", v.konum, yol, "en az " + std::to_string(*s.en_az_girdi) + " girdi olmalı");
                if (sessiz()) return false;
            }
            if (s.en_cok_girdi && girdi > *s.en_cok_girdi) {
                tamam = bildir("maxProperties", v.konum, yol, "en çok " + std::to_string(*s.en_cok_girdi) + " girdi olmalı") && tamam;
                if (sessiz()) return false;
            }
        }
        for (const auto& a : s.zorunlu) {
            if (v.bul(a) == nullptr) {
                yol.adimlar.push_back({&a, 0});
                tamam = bildir("required", Konum{v.konum.dosya, 0}, yol, "zorunlu anahtar eksik: " + a) && tamam;
                yol.adimlar.pop_back();
                if (sessiz()) return false;
            }
        }
        for (const auto& [k, alt] : v.tablo) {
            yol.adimlar.push_back({&k, 0});
            const bool e = ozellik(s, k, alt, yol);
            yol.adimlar.pop_back();
            tamam = e && tamam;
            if (!tamam && sessiz()) return false;
        }
        return tamam;
    }

    bool ozellik(const Dugum& s, const std::string& k, const Deger& alt, Iz& yol)
    {
        bool tamam = true;
        if (s.adlar != nullptr) {
            Yorumlayici y(kip_, nullptr);
            if (!y.dogrula(*s.adlar, Deger::yeni_dizgi(k), yol)) {
                return bildir("propertyNames", alt.konum, yol, "ad desene uymuyor: " + k);
            }
        }
        bool eslesti = false;
        if (const auto p = s.ozellikler.find(k); p != s.ozellikler.end()) {
            eslesti = true;
            tamam = dogrula(*p->second, alt, yol);
            if (!tamam && sessiz()) return false;
        }
        for (const auto& [d, ds] : s.desen_ozellikleri) {
            if (d->uyar(k)) {
                eslesti = true;
                tamam = dogrula(*ds, alt, yol) && tamam;
                if (!tamam && sessiz()) return false;
            }
        }
        if (!eslesti) {
            if (s.ek_yasak) {
                return bildir("additionalProperties", alt.konum, yol, "bu sözleşmede tanımlanmamış anahtar: " + k);
            }
            if (s.ek != nullptr) {
                tamam = dogrula(*s.ek, alt, yol) && tamam;
            }
        }
        return tamam;
    }

    bool sessiz_dogrula(const Dugum& s, const Deger& v, Iz& yol) const
    {
        Yorumlayici y(kip_, nullptr);
        return y.dogrula(s, v, yol);
    }

    bool bilesikler(const Dugum& s, const Deger& v, Iz& yol)
    {
        bool tamam = true;
        for (const Dugum* alt : s.hepsi) {
            tamam = dogrula(*alt, v, yol) && tamam;
            if (!tamam && sessiz()) return false;
        }
        if (!s.biri.empty()) {
            bool biri = false;
            for (const Dugum* alt : s.biri) {
                biri = biri || sessiz_dogrula(*alt, v, yol);
            }
            if (!biri) {
                tamam = bildir("anyOf", v.konum, yol, "seçeneklerin hiçbirine uymuyor") && tamam;
                if (sessiz()) return false;
            }
        }
        if (!s.yalniz_biri.empty()) {
            std::size_t n = 0;
            for (const Dugum* alt : s.yalniz_biri) {
                n += sessiz_dogrula(*alt, v, yol) ? 1u : 0u;
            }
            if (n != 1) {
                tamam = bildir("oneOf", v.konum, yol,
                               "seçeneklerden tam olarak birine uymalı (" + std::to_string(n) + " seçeneğe uyuyor)") &&
                        tamam;
                if (sessiz()) return false;
            }
        }
        if (s.degil != nullptr && sessiz_dogrula(*s.degil, v, yol)) {
            tamam = bildir("not", v.konum, yol, "bu biçime izin verilmiyor (A5 şeması `not`)") && tamam;
            if (sessiz()) return false;
        }
        if (s.eger != nullptr) {
            if (sessiz_dogrula(*s.eger, v, yol)) {
                if (s.ise != nullptr) {
                    tamam = dogrula(*s.ise, v, yol) && tamam;
                }
            } else if (s.degilse != nullptr) {
                tamam = dogrula(*s.degilse, v, yol) && tamam;
            }
        }
        return tamam;
    }
};

std::mutex& derleme_kilidi()
{
    static std::mutex k;
    return k;
}

}  // namespace

void sema_dogrula(const Deger& d, std::string_view tanim, const Yol& kok_yolu, SemaKipi kip,
                  Bulgucu& b)
{
    const DerlenmisSema* s = nullptr;
    {
        const std::lock_guard<std::mutex> k(derleme_kilidi());
        s = &derlenmis();
    }
    const std::string t(tanim);
    const Dugum* dugum = (t == "#") ? s->tumu : s->tanimlar.at(t.substr(std::string("#/$defs/").size()));
    Yorumlayici y(kip, &b);
    Iz iz;
    iz.kok = &kok_yolu;
    iz.adimlar.reserve(32);
    (void)y.dogrula(*dugum, d, iz);
}

}  // namespace ic
}  // namespace ida::senaryo
