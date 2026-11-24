// Altın dosya, Ek E sürüklenmesi, belge tutarlılığı, sayı dizilimi ve SHA-256 vektörleri.
#include "ic.hpp"
#include "test_destek.hpp"

#include <nlohmann/json.hpp>

#include <cmath>
#include <cstring>
#include <map>
#include <set>

namespace ida::senaryo::test {

namespace {

constexpr const char* kAltinOzet = "d1735f8b7eafabede9350c591bb4db7aa3a563b8ac4f45500200d33d4427cd92";

std::uint64_t bitler(double d)
{
    std::uint64_t b = 0;
    std::memcpy(&b, &d, sizeof b);
    return b;
}

std::string onaltilik(std::uint64_t b)
{
    char t[24];
    std::snprintf(t, sizeof t, "%016llx", static_cast<unsigned long long>(b));
    return t;
}

}  // namespace

int test_sha256_vektorleri(const TestBaglami&)
{
    Denetci d;
    // FIPS 180-4 / NIST örnekleri
    DOGRULA(d, sha256_hex("") == "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855", "boş");
    DOGRULA(d, sha256_hex("abc") == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad", "abc");
    DOGRULA(d, sha256_hex("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq") ==
                   "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1",
            "448 bit");
    DOGRULA(d, sha256_hex(std::string(1000000, 'a')) ==
                   "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0",
            "milyon a");
    // 55, 56, 63, 64 bayt sınırları (dolgu bir ya da iki blok)
    DOGRULA(d, sha256_hex(std::string(55, 'a')) == "9f4390f8d30c2dd92ec9f095b65e2b9ae9b0a925a5258e241c9f1e910f734318", "55");
    DOGRULA(d, sha256_hex(std::string(56, 'a')) == "b35439a4ac6f0948b6d6f9e3c6af0f5f590ce20f1bde7090ef7970686ec6738a", "56");
    DOGRULA(d, sha256_hex(std::string(64, 'a')) == "ffe054fe7ae0cb6dc65c3af9b61d5209f439851db43d0ba5997337df154668eb", "64");
    return d.hata;
}

int test_sayi_dizilimi(const TestBaglami&)
{
    Denetci d;
    const std::pair<double, const char*> v[] = {
        {0.1, "0.1"},
        {15.6, "15.6"},
        {-0.0, "0"},
        {0.0, "0"},
        {4.9e-324, "5e-324"},
        {1.7976931348623157e308, "1.7976931348623157e+308"},
        {9007199254740992.0, "9007199254740992"},
        {1e21, "1e+21"},
        {1e20, "100000000000000000000"},
        {123e18, "123000000000000000000"},
        {1e-7, "1e-7"},
        {1e-6, "0.000001"},
        {2e-8, "2e-8"},
        {2.56e-05, "0.0000256"},
        {4.887e-5, "0.00004887"},
        {-1.5, "-1.5"},
        {60000000000.0, "60000000000"},
        {0.39269908169872414, "0.39269908169872414"},
        {101325.0, "101325"},
        {1.25e-7, "1.25e-7"},
        {-3.14e300, "-3.14e+300"},
        {5e-7, "5e-7"},
        {123456789012345680000.0, "123456789012345680000"},
    };
    for (const auto& [x, beklenen] : v) {
        const std::string g = sayi_dizilimi(x);
        DOGRULA(d, g == beklenen, "sayi_dizilimi: " + g + " ≠ " + beklenen);
    }
    return d.hata;
}

int test_k96_vektorleri(const TestBaglami&)
{
    Denetci d;
    struct Vektor {
        const char* yazim;
        bool kabul;
        std::uint64_t bit;
        const char* jcs;
    };
    // A5-K96 tablosu (her satır).
    const Vektor v[] = {
        {"0.1", true, 0x3FB999999999999AULL, "0.1"},
        {"15.6", true, 0x402F333333333333ULL, "15.6"},
        {"-0.0", true, 0x0000000000000000ULL, "0"},
        {"4.9e-324", true, 0x0000000000000001ULL, "5e-324"},
        {"2.4703282292062328e-324", true, 0x0000000000000001ULL, "5e-324"},
        {"2.4703282292062327e-324", false, 0, nullptr},
        {"1e-400", false, 0, nullptr},
        {"1.7976931348623157e308", true, 0x7FEFFFFFFFFFFFFFULL, "1.7976931348623157e+308"},
        {"1.7976931348623159e308", false, 0, nullptr},
        {"9007199254740993.0", true, 0x4340000000000000ULL, "9007199254740992"},
        {"inf", false, 0, nullptr},
        {"nan", false, 0, nullptr},
        {"-inf", false, 0, nullptr},
        {"+nan", false, 0, nullptr},
        {"1e400", false, 0, nullptr},
        {"1_000.5", true, 0x408F440000000000ULL, "1000.5"},
    };
    for (const auto& x : v) {
        // Aynı satırda önce bir değer ve sonra bir dizi elemanı olarak (konum çevirisi).
        const std::string metin = std::string("a = 1\nx_k = ") + x.yazim + "\nx_d = [\"ğğ\", " + x.yazim + "]\n";
        ic::Bulgucu b;
        b.dosyalar.push_back("t.toml");
        const ic::TomlOkuma t = ic::toml_oku(metin, 0, b);
        DOGRULA(d, t.kok.has_value(), std::string("ayrıştırılamadı: ") + x.yazim);
        if (!t.kok) {
            continue;
        }
        const Deger* k = t.kok->bul("x_k");
        const Deger* e = &t.kok->bul("x_d")->dizi[1];
        if (x.kabul) {
            DOGRULA(d, !b.hata_var(), std::string("kabul edilmeliydi: ") + x.yazim);
            for (const Deger* z : {k, e}) {
                DOGRULA(d, z->tur == Tur::sayi && bitler(z->sayi) == x.bit,
                        std::string(x.yazim) + " bitleri " + onaltilik(bitler(z->sayi)));
                DOGRULA(d, sayi_dizilimi(z->sayi) == x.jcs, std::string(x.yazim) + " JCS " + sayi_dizilimi(z->sayi));
            }
        } else {
            std::size_t n = 0;
            for (const auto& bb : b.bulgular) {
                n += bb.kural == "A5-K96" && bb.satir.has_value() && (*bb.satir == 2 || *bb.satir == 3) ? 1u : 0u;
            }
            DOGRULA(d, n == 2 && b.hata_sayisi() == 2, std::string("A5-K96 ile iki kez reddedilmeliydi: ") + x.yazim);
            DOGRULA(d, k->tur == Tur::gecersiz && e->tur == Tur::gecersiz, std::string("geçersiz işaretli değil: ") + x.yazim);
        }
    }
    // A5-K09 ayrımı: tamsayı ve kayan noktalı tipler ayrı tutulur.
    {
        ic::Bulgucu b;
        const ic::TomlOkuma t = ic::toml_oku("a = 3\nb = 3.0\nc = 0x10\n", 0, b);
        DOGRULA(d, t.kok->bul("a")->tur == Tur::tamsayi && t.kok->bul("b")->tur == Tur::sayi &&
                       t.kok->bul("c")->tam == 16,
                "tamsayı/kayan ayrımı");
    }
    return d.hata;
}

int test_altin_dosya(const TestBaglami& tb)
{
    Denetci d;
    const BellekKaynagi k = temel_kaynak(tb);
    const CozumSonucu s = senaryo_coz(k, kSenaryo);
    DOGRULA(d, s.rapor.gecerli, "ornek-havuz geçerli olmalı:\n" + bulgu_ozeti(s.rapor));
    const std::string altin = dosya_oku(tb.kok + "/scenarios/ornek-havuz.cozumlenmis.json");
    DOGRULA(d, !altin.empty(), "altın dosya okunamadı");
    DOGRULA(d, sha256_hex(altin) == kAltinOzet, "altın dosyanın özeti A5-K17'deki değer değil");
    if (s.kanonik != altin) {
        std::size_t i = 0;
        while (i < s.kanonik.size() && i < altin.size() && s.kanonik[i] == altin[i]) {
            ++i;
        }
        const std::size_t bas = i > 120 ? i - 120 : 0;
        DOGRULA(d, false, "kanonik baytlar altın dosyadan farklı; ilk fark bayt " + std::to_string(i) +
                              "\n  üretilen: …" + s.kanonik.substr(bas, 240) + "\n  altın:    …" +
                              altin.substr(bas, 240));
    }
    DOGRULA(d, s.rapor.davranis_ozeti.value_or("") == kAltinOzet, "davranış özeti A5-K17'deki değer değil");
    // Kaynak özetleri ayrı: senaryo, araç ve manifest.
    DOGRULA(d, s.rapor.kaynak_ozetleri.size() == 3, "kaynak özetleri üç dosya içermeli");
    DOGRULA(d, s.rapor.kaynak_ozetleri.at(kSenaryo) == sha256_hex(k.dosyalar.at(kSenaryo)), "senaryo kaynak özeti");
    // Beklenen uyarılar: iki doğrulanmamış soğurma (A5-K108).
    std::size_t k108 = 0;
    for (const auto& b : s.rapor.bulgular) {
        k108 += b.kural == "A5-K108" ? 1u : 0u;
    }
    DOGRULA(d, k108 == 2, "iki A5-K108 uyarısı bekleniyordu:\n" + bulgu_ozeti(s.rapor));
    return d.hata;
}

namespace {
std::string ek_e_blogu(const std::string& sozlesme)
{
    const std::size_t i = sozlesme.find("## Ek E");
    if (i == std::string::npos) {
        return {};
    }
    const std::string ac = "```toml\n";
    const std::size_t j = sozlesme.find(ac, i);
    if (j == std::string::npos) {
        return {};
    }
    const std::size_t k = sozlesme.find("\n```", j + ac.size());
    if (k == std::string::npos) {
        return {};
    }
    return sozlesme.substr(j + ac.size(), k - j - ac.size()) + "\n";
}
}  // namespace

int test_ek_e_suruklenme(const TestBaglami& tb)
{
    Denetci d;
    const std::string sozlesme = dosya_oku(tb.kok + "/docs/sozlesmeler/A5-senaryo.md");
    const std::string blok = ek_e_blogu(sozlesme);
    const std::string veri = dosya_oku(tb.kok + "/sim/senaryo/veri/ek_e.toml");
    DOGRULA(d, !blok.empty(), "sözleşmede Ek E bloğu bulunamadı");
    DOGRULA(d, blok == veri, "sim/senaryo/veri/ek_e.toml sözleşmedeki Ek E bloğundan farklı");
    DOGRULA(d, std::string(ic::ek_e_metni()) == veri, "gömülü Ek E dosyadan farklı (yeniden derleyin)");
    DOGRULA(d, ic::ek_e().bul("surum") != nullptr && ic::ek_e().bul("surum")->dizgi == "1.0",
            "gömülü Ek E ayrıştırılamadı ya da sürümü 1.0 değil");
    return d.hata;
}

namespace {

using json = nlohmann::json;

const json* cozumle_ref(const json& kok, const json* n)
{
    while (n != nullptr && n->is_object() && n->contains("$ref")) {
        const std::string r = (*n)["$ref"].get<std::string>();
        n = &kok["$defs"][r.substr(std::string("#/$defs/").size())];
    }
    return n;
}

// Şemadaki her `default`'u (çözümlenmiş tanımdan erişilebilen) yol → değer olarak toplar.
void varsayilanlari_topla(const json& kok, const json& n0, const std::string& yol,
                          std::map<std::string, json>& o, int derinlik)
{
    if (derinlik > 12) {
        return;
    }
    const json* n = cozumle_ref(kok, &n0);
    if (n == nullptr || !n->is_object()) {
        return;
    }
    if (n->contains("default") && !yol.empty()) {
        o[yol] = (*n)["default"];
    }
    if (n->contains("properties")) {
        for (const auto& [k, v] : (*n)["properties"].items()) {
            varsayilanlari_topla(kok, v, yol.empty() ? k : yol + "." + k, o, derinlik + 1);
        }
    }
    if (n->contains("additionalProperties") && (*n)["additionalProperties"].is_object()) {
        varsayilanlari_topla(kok, (*n)["additionalProperties"], yol + ".*", o, derinlik + 1);
    }
    if (n->contains("items") && (*n)["items"].is_object()) {
        varsayilanlari_topla(kok, (*n)["items"], yol + "[]", o, derinlik + 1);
    }
}

bool json_deger_esit(const json& j, const Deger& v)
{
    if (j.is_number()) return v.sayisal_mi() && j.get<double>() == v.cift();
    if (j.is_string()) return v.dizgi_mi() && j.get<std::string>() == v.dizgi;
    if (j.is_boolean()) return v.mantiksal_mi() && j.get<bool>() == v.mantik;
    if (j.is_array()) {
        if (!v.dizi_mi() || v.dizi.size() != j.size()) return false;
        for (std::size_t i = 0; i < j.size(); ++i) {
            if (!json_deger_esit(j[i], v.dizi[i])) return false;
        }
        return true;
    }
    if (j.is_object()) {
        if (!v.tablo_mu() || v.tablo.size() != j.size()) return false;
        for (const auto& [k, x] : j.items()) {
            const Deger* w = v.bul(k);
            if (w == nullptr || !json_deger_esit(x, *w)) return false;
        }
        return true;
    }
    return false;
}

// A5 §3.6 profil tablosunun bir satırından (anahtar → TOML değişmezi) çiftleri çıkarır.
std::map<std::string, std::string> profil_satiri(const std::string& hucre)
{
    std::map<std::string, std::string> o;
    std::size_t i = 0;
    while ((i = hucre.find('`', i)) != std::string::npos) {
        const std::size_t j = hucre.find('`', i + 1);
        if (j == std::string::npos) {
            break;
        }
        const std::string anahtar = hucre.substr(i + 1, j - i - 1);
        std::size_t p = j + 1;
        while (p < hucre.size() && hucre[p] == ' ') {
            ++p;
        }
        if (p >= hucre.size() || hucre[p] != '(') {
            i = j + 1;
            continue;
        }
        int derin = 0;
        std::size_t son = p;
        for (; son < hucre.size(); ++son) {
            if (hucre[son] == '(') ++derin;
            if (hucre[son] == ')' && --derin == 0) break;
        }
        const std::string ic = hucre.substr(p + 1, son - p - 1);
        // Parantez içindeki son `…` değişmezi (TOML olarak ayrışıyorsa).
        std::string deger;
        std::size_t a = 0;
        while ((a = ic.find('`', a)) != std::string::npos) {
            const std::size_t b = ic.find('`', a + 1);
            if (b == std::string::npos) break;
            deger = ic.substr(a + 1, b - a - 1);
            a = b + 1;
        }
        if (!deger.empty()) {
            o[anahtar] = deger;
        }
        i = son;
    }
    return o;
}

}  // namespace

int test_belge_tutarliligi(const TestBaglami& tb)
{
    Denetci d;
    const json sema = json::parse(ic::sema_metni());
    const Deger& e = ic::ek_e();
    // (1) Ek E [kosulsuz] ↔ şemanın `default` değerleri (çözümlenmiş tanımda).
    std::map<std::string, json> varsayilanlar;
    varsayilanlari_topla(sema, sema["$defs"]["cozumlenmis_senaryo"], "", varsayilanlar, 0);
    const Deger* ks = e.bul("kosulsuz");
    DOGRULA(d, ks != nullptr, "Ek E [kosulsuz] yok");
    if (ks != nullptr) {
        for (const auto& [yol, v] : ks->tablo) {
            const auto it = varsayilanlar.find(yol);
            if (it == varsayilanlar.end()) {
                DOGRULA(d, false, "Ek E [kosulsuz] " + yol + ": şemada `default` yok");
            } else {
                DOGRULA(d, json_deger_esit(it->second, v),
                        "Ek E [kosulsuz] " + yol + " şema default'undan farklı: " + it->second.dump());
            }
        }
        for (const auto& [yol, v] : varsayilanlar) {
            DOGRULA(d, ks->bul(yol) != nullptr,
                    "şemanın `default`'u Ek E [kosulsuz]'da yok: " + yol + " = " + v.dump());
        }
    }
    // (2) Ek E [profil.*] ↔ A5 §3.6 ürün profilleri tablosu.
    const std::string sozlesme = dosya_oku(tb.kok + "/docs/sozlesmeler/A5-senaryo.md");
    const Deger* profiller = e.bul("profil");
    std::size_t satir_sayisi = 0;
    std::size_t bas = 0;
    while ((bas = sozlesme.find("\n| `", bas)) != std::string::npos) {
        const std::size_t son = sozlesme.find('\n', bas + 1);
        const std::string satir = sozlesme.substr(bas + 1, son - bas - 1);
        bas = son;
        // "| `tur` (`profil`; hız) | çiftler |"
        const std::size_t t1 = satir.find('`');
        const std::size_t t2 = satir.find('`', t1 + 1);
        const std::string tur = satir.substr(t1 + 1, t2 - t1 - 1);
        const Deger* p = profiller != nullptr ? profiller->bul(tur) : nullptr;
        if (p == nullptr || satir.find("` (`") != t2) {
            continue;
        }
        const std::size_t ayrac = satir.find(" | ", t2);
        if (ayrac == std::string::npos) {
            continue;
        }
        ++satir_sayisi;
        const std::string baslik = satir.substr(0, ayrac);
        const std::string hucre = satir.substr(ayrac + 3);
        // profil adı ve hız
        const std::size_t p1 = baslik.find("(`") + 2;
        const std::string profil = baslik.substr(p1, baslik.find('`', p1) - p1);
        DOGRULA(d, p->bul("profil") != nullptr && p->bul("profil")->dizgi == profil,
                tur + ": profil adı Ek E'den farklı");
        const std::size_t hz = baslik.find("; `");
        if (hz != std::string::npos) {
            const std::string h = baslik.substr(hz + 3, baslik.find('`', hz + 3) - hz - 3);
            const Deger* eh = p->bul("hiz_hz");
            DOGRULA(d, eh != nullptr && eh->cift() == std::stod(h), tur + ": hız Ek E'den farklı");
        } else {
            DOGRULA(d, p->bul("hiz_hz") == nullptr, tur + ": tabloda hız yok ama Ek E'de var");
        }
        const auto ciftler = profil_satiri(hucre);
        const Deger* ayar = p->bul("ayar");
        std::set<std::string> gorulen;
        for (const auto& [anahtar, yazim] : ciftler) {
            ic::Bulgucu b;
            const ic::TomlOkuma t = ic::toml_oku("v = " + yazim + "\n", -1, b);
            if (!t.kok || b.hata_var()) {
                continue;  // değişmez değil (ör. "montaj"); değer yazılmaz
            }
            gorulen.insert(anahtar);
            const Deger* ev = ayar != nullptr ? ayar->bul(anahtar) : nullptr;
            DOGRULA(d, ev != nullptr && esit(*ev, *t.kok->bul("v")),
                    tur + "." + anahtar + ": §3.6 değeri (" + yazim + ") Ek E'den farklı ya da Ek E'de yok");
        }
        if (ayar != nullptr) {
            for (const auto& kv : ayar->tablo) {
                DOGRULA(d, gorulen.count(kv.first) != 0, tur + "." + kv.first + ": Ek E'de var, §3.6 tablosunda yok");
            }
        }
    }
    DOGRULA(d, satir_sayisi == 11, "§3.6 profil tablosunda 11 satır bekleniyordu: " + std::to_string(satir_sayisi));
    return d.hata;
}

int test_sema_sozcukleri(const TestBaglami&)
{
    Denetci d;
    const json sema = json::parse(ic::sema_metni());
    static const std::set<std::string> taninan = {
        "type", "properties", "patternProperties", "additionalProperties", "required", "items",
        "minItems", "maxItems", "minProperties", "maxProperties", "propertyNames", "enum",
        "const", "minimum", "maximum", "exclusiveMinimum", "exclusiveMaximum", "pattern",
        "maxLength", "$ref", "allOf", "anyOf", "oneOf", "not", "if", "then", "else",
        // yorumlanmayan açıklama sözcükleri
        "description", "default", "title", "$schema", "$id", "$defs"};
    std::set<std::string> bilinmeyen;
    std::vector<std::pair<const json*, bool>> yigin = {{&sema, false}};
    while (!yigin.empty()) {
        const auto [n, ad_haritasi] = yigin.back();
        yigin.pop_back();
        if (n->is_object()) {
            for (const auto& [k, v] : n->items()) {
                if (!ad_haritasi && taninan.count(k) == 0) {
                    bilinmeyen.insert(k);
                }
                const bool harita = !ad_haritasi && (k == "properties" || k == "patternProperties" || k == "$defs");
                if (!ad_haritasi && (k == "default" || k == "const" || k == "enum")) {
                    continue;
                }
                yigin.emplace_back(&v, harita);
            }
        } else if (n->is_array()) {
            for (const auto& v : *n) {
                yigin.emplace_back(&v, false);
            }
        }
    }
    for (const auto& k : bilinmeyen) {
        DOGRULA(d, false, "şemada yorumlayıcının tanımadığı anahtar sözcük: " + k);
    }
    return d.hata;
}

}  // namespace ida::senaryo::test
