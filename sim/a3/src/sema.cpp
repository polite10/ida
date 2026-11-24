// sema.cpp — bkz. sema.hpp.
#include "sema.hpp"

#include <algorithm>
#include <cmath>
#include <functional>
#include <set>

namespace ida::a3 {

// ---- Desen --------------------------------------------------------------------------------------

struct Desen::Dugum {
    enum class Tur : std::uint8_t { sinif, grup, bas, son };
    Tur tur = Tur::sinif;
    // sinif
    bool olumsuz = false;
    std::vector<std::pair<std::uint32_t, std::uint32_t>> araliklar;
    // grup: seçenekler, her biri parça dizisi
    struct Parca;
    std::vector<std::vector<Parca>> secenekler;

    bool icerir(std::uint32_t kp) const noexcept
    {
        bool var = false;
        for (const auto& [a, b] : araliklar) {
            if (kp >= a && kp <= b) {
                var = true;
                break;
            }
        }
        return var != olumsuz;
    }
};

struct Desen::Dugum::Parca {
    Dugum d;
    std::uint32_t en_az = 1;
    std::uint32_t en_cok = 1;  // kSinirsiz = sınırsız
};

namespace {

constexpr std::uint32_t kSinirsiz = 0xFFFFFFFFu;
constexpr std::size_t kAdimButcesi = 2'000'000;
constexpr int kAzamiOzyineleme = 2000;

using Dugum = Desen::Dugum;
using Parca = Desen::Dugum::Parca;

class DesenCozucu {
public:
    explicit DesenCozucu(const std::vector<std::uint32_t>& k) : k_(k) {}

    bool calis(Dugum& kok, std::string& hata)
    {
        kok.tur = Dugum::Tur::grup;
        if (!secenekler_(kok.secenekler, 0)) {
            hata = hata_;
            return false;
        }
        if (i_ != k_.size()) {
            hata = "desende fazla ')'";
            return false;
        }
        return true;
    }

private:
    bool hata_ver_(const char* m)
    {
        hata_ = m;
        return false;
    }

    bool secenekler_(std::vector<std::vector<Parca>>& cikti, int derinlik)
    {
        if (derinlik > 32) {
            return hata_ver_("desen çok derin");
        }
        cikti.emplace_back();
        while (i_ < k_.size()) {
            const std::uint32_t c = k_[i_];
            if (c == ')') {
                return true;
            }
            if (c == '|') {
                ++i_;
                cikti.emplace_back();
                continue;
            }
            Parca p;
            if (!atom_(p.d, derinlik)) {
                return false;
            }
            if (!niceleyici_(p)) {
                return false;
            }
            cikti.back().push_back(std::move(p));
        }
        return true;
    }

    bool kacis_(std::uint32_t& kp, Dugum* sinif_icin)
    {
        // `\` okundu.
        if (i_ >= k_.size()) {
            return hata_ver_("desen sonunda '\\'");
        }
        const std::uint32_t c = k_[i_++];
        switch (c) {
        case 'u': {
            if (k_.size() - i_ < 4) {
                return hata_ver_("eksik \\u");
            }
            kp = 0;
            for (int n = 0; n < 4; ++n) {
                const std::uint32_t h = k_[i_++];
                std::uint32_t v = 0;
                if (h >= '0' && h <= '9') {
                    v = h - '0';
                } else if (h >= 'a' && h <= 'f') {
                    v = h - 'a' + 10;
                } else if (h >= 'A' && h <= 'F') {
                    v = h - 'A' + 10;
                } else {
                    return hata_ver_("geçersiz \\u");
                }
                kp = (kp << 4u) | v;
            }
            return true;
        }
        case 'n': kp = '\n'; return true;
        case 'r': kp = '\r'; return true;
        case 't': kp = '\t'; return true;
        case 'd':
        case 'w':
        case 's':
            if (sinif_icin == nullptr) {
                return hata_ver_("desteklenmeyen kaçış");
            }
            if (c == 'd') {
                sinif_icin->araliklar.emplace_back('0', '9');
            } else if (c == 'w') {
                sinif_icin->araliklar.emplace_back('a', 'z');
                sinif_icin->araliklar.emplace_back('A', 'Z');
                sinif_icin->araliklar.emplace_back('0', '9');
                sinif_icin->araliklar.emplace_back('_', '_');
            } else {
                for (const std::uint32_t b : {0x20u, 0x09u, 0x0Au, 0x0Bu, 0x0Cu, 0x0Du, 0xA0u, 0xFEFFu}) {
                    sinif_icin->araliklar.emplace_back(b, b);
                }
            }
            kp = kSinirsiz;
            return true;
        default:
            if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')) {
                return hata_ver_("desteklenmeyen kaçış");
            }
            kp = c;  // \. \- \\ \/ \^ \$ \[ \] \( \) \{ \} \* \+ \? \|
            return true;
        }
    }

    bool atom_(Dugum& d, int derinlik)
    {
        const std::uint32_t c = k_[i_];
        if (c == '^') {
            ++i_;
            d.tur = Dugum::Tur::bas;
            return true;
        }
        if (c == '$') {
            ++i_;
            d.tur = Dugum::Tur::son;
            return true;
        }
        if (c == '(') {
            ++i_;
            if (i_ < k_.size() && k_[i_] == '?') {
                if (i_ + 1 < k_.size() && k_[i_ + 1] == ':') {
                    i_ += 2;
                } else {
                    return hata_ver_("desteklenmeyen grup");
                }
            }
            d.tur = Dugum::Tur::grup;
            if (!secenekler_(d.secenekler, derinlik + 1)) {
                return false;
            }
            if (i_ >= k_.size() || k_[i_] != ')') {
                return hata_ver_("kapanmamış grup");
            }
            ++i_;
            return true;
        }
        d.tur = Dugum::Tur::sinif;
        if (c == '[') {
            ++i_;
            if (i_ < k_.size() && k_[i_] == '^') {
                d.olumsuz = true;
                ++i_;
            }
            bool ilk = true;
            for (;;) {
                if (i_ >= k_.size()) {
                    return hata_ver_("kapanmamış karakter sınıfı");
                }
                std::uint32_t a = k_[i_];
                if (a == ']' && !ilk) {
                    ++i_;
                    return true;
                }
                ilk = false;
                ++i_;
                if (a == '\\') {
                    if (!kacis_(a, &d)) {
                        return false;
                    }
                    if (a == kSinirsiz) {
                        continue;
                    }
                }
                std::uint32_t b = a;
                if (i_ + 1 < k_.size() && k_[i_] == '-' && k_[i_ + 1] != ']') {
                    ++i_;
                    b = k_[i_++];
                    if (b == '\\') {
                        if (!kacis_(b, nullptr)) {
                            return false;
                        }
                    }
                    if (b < a) {
                        return hata_ver_("ters karakter aralığı");
                    }
                }
                d.araliklar.emplace_back(a, b);
            }
        }
        ++i_;
        if (c == '.') {
            d.olumsuz = true;
            for (const std::uint32_t b : {0x0Au, 0x0Du, 0x2028u, 0x2029u}) {
                d.araliklar.emplace_back(b, b);
            }
            return true;
        }
        if (c == '*' || c == '+' || c == '?' || c == '{' || c == ')' || c == '|' || c == ']') {
            return hata_ver_("beklenmeyen desen karakteri");
        }
        std::uint32_t kp = c;
        if (c == '\\') {
            if (!kacis_(kp, &d)) {
                return false;
            }
            if (kp == kSinirsiz) {
                return true;
            }
        }
        d.araliklar.emplace_back(kp, kp);
        return true;
    }

    bool sayi_(std::uint32_t& v)
    {
        if (i_ >= k_.size() || k_[i_] < '0' || k_[i_] > '9') {
            return false;
        }
        v = 0;
        while (i_ < k_.size() && k_[i_] >= '0' && k_[i_] <= '9') {
            v = v * 10u + (k_[i_] - '0');
            if (v > 100000u) {
                return false;
            }
            ++i_;
        }
        return true;
    }

    bool niceleyici_(Parca& p)
    {
        if (i_ >= k_.size()) {
            return true;
        }
        const std::uint32_t c = k_[i_];
        if (p.d.tur == Dugum::Tur::bas || p.d.tur == Dugum::Tur::son) {
            if (c == '*' || c == '+' || c == '?' || c == '{') {
                return hata_ver_("çapaya niceleyici");
            }
            return true;
        }
        if (c == '*') {
            p.en_az = 0;
            p.en_cok = kSinirsiz;
            ++i_;
        } else if (c == '+') {
            p.en_az = 1;
            p.en_cok = kSinirsiz;
            ++i_;
        } else if (c == '?') {
            p.en_az = 0;
            p.en_cok = 1;
            ++i_;
        } else if (c == '{') {
            ++i_;
            std::uint32_t a = 0;
            if (!sayi_(a)) {
                return hata_ver_("geçersiz {n,m}");
            }
            std::uint32_t b = a;
            if (i_ < k_.size() && k_[i_] == ',') {
                ++i_;
                if (i_ < k_.size() && k_[i_] == '}') {
                    b = kSinirsiz;
                } else if (!sayi_(b)) {
                    return hata_ver_("geçersiz {n,m}");
                }
            }
            if (i_ >= k_.size() || k_[i_] != '}' || b < a) {
                return hata_ver_("geçersiz {n,m}");
            }
            ++i_;
            p.en_az = a;
            p.en_cok = b;
        } else {
            return true;
        }
        if (i_ < k_.size() && k_[i_] == '?') {
            return hata_ver_("tembel niceleyici desteklenmez");
        }
        return true;
    }

    const std::vector<std::uint32_t>& k_;
    std::size_t i_ = 0;
    std::string hata_;
};

class Esleyici {
public:
    explicit Esleyici(const std::vector<std::uint32_t>& s) : s_(s) {}

    using Devam = std::function<bool(std::size_t)>;

    bool dizi(const std::vector<Parca>& seq, std::size_t i, std::size_t konum, const Devam& k, int derinlik)
    {
        if (++adim_ > kAdimButcesi || derinlik > kAzamiOzyineleme) {
            return false;
        }
        if (i == seq.size()) {
            return k(konum);
        }
        const Parca& p = seq[i];
        if (p.d.tur == Dugum::Tur::bas) {
            return konum == 0 && dizi(seq, i + 1, konum, k, derinlik + 1);
        }
        if (p.d.tur == Dugum::Tur::son) {
            return konum == s_.size() && dizi(seq, i + 1, konum, k, derinlik + 1);
        }
        if (p.d.tur == Dugum::Tur::sinif) {
            // Tek karakterlik atom: en uzun koşuyu bul, açgözlü geri dön (özyinelemesiz döngü).
            std::size_t r = 0;
            while (konum + r < s_.size() && (p.en_cok == kSinirsiz || r < p.en_cok) && p.d.icerir(s_[konum + r])) {
                ++r;
            }
            if (r < p.en_az) {
                return false;
            }
            for (std::size_t n = r + 1; n-- > p.en_az;) {
                if (++adim_ > kAdimButcesi) {
                    return false;
                }
                if (dizi(seq, i + 1, konum + n, k, derinlik + 1)) {
                    return true;
                }
            }
            return false;
        }
        return grup_tekrar(p, seq, i, konum, 0, k, derinlik + 1);
    }

    bool grup_tekrar(const Parca& p, const std::vector<Parca>& seq, std::size_t i, std::size_t konum,
                     std::uint32_t n, const Devam& k, int derinlik)
    {
        if (++adim_ > kAdimButcesi || derinlik > kAzamiOzyineleme) {
            return false;
        }
        if (p.en_cok == kSinirsiz || n < p.en_cok) {
            for (const auto& secenek : p.d.secenekler) {
                const Devam sonraki = [&](std::size_t yeni) {
                    if (yeni == konum && n >= p.en_az) {
                        return false;  // boş tekrar döngüsünü kır
                    }
                    return grup_tekrar(p, seq, i, yeni, n + 1, k, derinlik + 1);
                };
                if (dizi(secenek, 0, konum, sonraki, derinlik + 1)) {
                    return true;
                }
            }
        }
        if (n >= p.en_az) {
            return dizi(seq, i + 1, konum, k, derinlik + 1);
        }
        return false;
    }

private:
    const std::vector<std::uint32_t>& s_;
    std::size_t adim_ = 0;
};

}  // namespace

Desen::~Desen() = default;

std::unique_ptr<Desen> Desen::derle(std::string_view kaynak, std::string* hata)
{
    std::vector<std::uint32_t> k;
    if (!json::kod_noktalari(kaynak, k)) {
        if (hata != nullptr) {
            *hata = "desen UTF-8 değil";
        }
        return nullptr;
    }
    std::unique_ptr<Desen> d(new Desen());
    d->kok_ = std::make_unique<Dugum>();
    DesenCozucu c(k);
    std::string h;
    if (!c.calis(*d->kok_, h)) {
        if (hata != nullptr) {
            *hata = h;
        }
        return nullptr;
    }
    return d;
}

bool Desen::ara(std::string_view metin) const
{
    std::vector<std::uint32_t> s;
    if (!json::kod_noktalari(metin, s)) {
        return false;
    }
    Esleyici e(s);
    for (std::size_t bas = 0; bas <= s.size(); ++bas) {
        // Kök bir gruptur: seçeneklerden biri `bas`'tan başlayarak eşleşirse yeter (sonu serbest).
        for (const auto& secenek : kok_->secenekler) {
            if (e.dizi(secenek, 0, bas, [](std::size_t) { return true; }, 0)) {
                return true;
            }
        }
        // `^` ile başlayan desende ilerideki başlangıçlar eşleşemez; erken çık.
        bool hepsi_capali = !kok_->secenekler.empty();
        for (const auto& secenek : kok_->secenekler) {
            if (secenek.empty() || secenek.front().d.tur != Dugum::Tur::bas) {
                hepsi_capali = false;
            }
        }
        if (hepsi_capali) {
            break;
        }
    }
    return false;
}

// ---- Sema ---------------------------------------------------------------------------------------

namespace {

bool sayi_esit(double a, double b) { return a == b; }

const std::set<std::string_view>& bilinen_sozcukler()
{
    static const std::set<std::string_view> k = {
        "$ref", "$defs", "$schema", "$id", "$comment", "type", "enum", "const", "properties", "required",
        "additionalProperties", "propertyNames", "minProperties", "maxProperties", "items", "prefixItems",
        "minItems", "maxItems", "uniqueItems", "minimum", "maximum", "exclusiveMinimum", "exclusiveMaximum",
        "multipleOf", "minLength", "maxLength", "pattern", "allOf", "anyOf", "oneOf", "not", "description",
        "title", "default", "examples", "format", "deprecated", "readOnly", "writeOnly"};
    return k;
}

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

bool tip_uyar(std::string_view tip, const json::Deger& d)
{
    using json::Tur;
    if (tip == "object") return d.tur == Tur::nesne;
    if (tip == "array") return d.tur == Tur::dizi;
    if (tip == "string") return d.tur == Tur::metin;
    if (tip == "boolean") return d.tur == Tur::mantiksal;
    if (tip == "null") return d.tur == Tur::bos;
    if (tip == "number") return d.tur == Tur::sayi;
    if (tip == "integer") return d.tamsayi_mi();
    return false;
}

void yol_ekle(std::string& yol, std::string_view parca)
{
    yol.push_back('/');
    yol.append(parca);
}

}  // namespace

bool json_esit(const json::Deger& a, const json::Deger& b)
{
    using json::Tur;
    if (a.tur != b.tur) {
        return false;
    }
    switch (a.tur) {
    case Tur::bos: return true;
    case Tur::mantiksal: return a.mantik == b.mantik;
    case Tur::sayi: return sayi_esit(a.sayi, b.sayi);
    case Tur::metin: return a.metin == b.metin;
    case Tur::dizi:
        if (a.dizi.size() != b.dizi.size()) {
            return false;
        }
        for (std::size_t i = 0; i < a.dizi.size(); ++i) {
            if (!json_esit(a.dizi[i], b.dizi[i])) {
                return false;
            }
        }
        return true;
    case Tur::nesne:
        if (a.nesne.size() != b.nesne.size()) {
            return false;
        }
        for (const auto& [k, v] : a.nesne) {
            const json::Deger* w = b.bul(k);
            if (w == nullptr || !json_esit(v, *w)) {
                return false;
            }
        }
        return true;
    }
    return false;
}

const json::Deger* Sema::tanim(std::string_view ad) const
{
    const json::Deger* defs = kok_.bul("$defs");
    return defs == nullptr ? nullptr : defs->bul(ad);
}

const Desen* Sema::desen_(const std::string& kaynak) const
{
    auto it = desenler_.find(kaynak);
    if (it != desenler_.end()) {
        return it->second.get();
    }
    auto d = Desen::derle(kaynak, nullptr);
    const Desen* p = d.get();
    desenler_.emplace(kaynak, std::move(d));
    return p;
}

bool Sema::desteklenmeyen_bul(const json::Deger& sema, std::string& hata) const
{
    using json::Tur;
    if (sema.tur == Tur::mantiksal) {
        return true;
    }
    if (sema.tur != Tur::nesne) {
        hata = "şema nesne ya da mantıksal değil";
        return false;
    }
    for (const auto& [k, v] : sema.nesne) {
        if (bilinen_sozcukler().count(k) == 0) {
            hata = "desteklenmeyen anahtar sözcük: " + k;
            return false;
        }
        if (k == "properties" || k == "$defs") {
            if (v.tur != Tur::nesne) {
                hata = k + " nesne değil";
                return false;
            }
            for (const auto& [ad, alt] : v.nesne) {
                (void)ad;
                if (!desteklenmeyen_bul(alt, hata)) {
                    return false;
                }
            }
        } else if (k == "items" || k == "additionalProperties" || k == "propertyNames" || k == "not") {
            if (!desteklenmeyen_bul(v, hata)) {
                return false;
            }
        } else if (k == "allOf" || k == "anyOf" || k == "oneOf" || k == "prefixItems") {
            if (v.tur != Tur::dizi) {
                hata = k + " dizi değil";
                return false;
            }
            for (const auto& alt : v.dizi) {
                if (!desteklenmeyen_bul(alt, hata)) {
                    return false;
                }
            }
        } else if (k == "pattern") {
            if (v.tur != Tur::metin || desen_(v.metin) == nullptr) {
                hata = "derlenemeyen desen: " + v.metin;
                return false;
            }
        } else if (k == "$ref") {
            if (v.tur != Tur::metin || v.metin.rfind("#/$defs/", 0) != 0 ||
                tanim(std::string_view(v.metin).substr(8)) == nullptr) {
                hata = "çözülemeyen $ref: " + v.metin;
                return false;
            }
        }
    }
    return true;
}

bool Sema::dogrula(const json::Deger& sema, const json::Deger& deger, std::string* hata) const
{
    std::string yol;
    return dogrula_(sema, deger, yol, hata, 0);
}

bool Sema::dogrula_(const json::Deger& sema, const json::Deger& d, std::string& yol, std::string* hata,
                    int derinlik) const
{
    using json::Tur;
    auto ret = [&](const std::string& neden) {
        if (hata != nullptr) {
            *hata = (yol.empty() ? std::string("/") : yol) + ": " + neden;
        }
        return false;
    };
    if (derinlik > 128) {
        return ret("şema derinliği aşıldı");
    }
    if (sema.tur == Tur::mantiksal) {
        return sema.mantik ? true : ret("bu alana izin verilmez");
    }
    if (sema.tur != Tur::nesne) {
        return ret("geçersiz şema");
    }
    for (const auto& [k, v] : sema.nesne) {
        if (k == "$ref") {
            const json::Deger* hedef = v.tur == Tur::metin && v.metin.rfind("#/$defs/", 0) == 0
                                           ? tanim(std::string_view(v.metin).substr(8))
                                           : nullptr;
            if (hedef == nullptr) {
                return ret("çözülemeyen $ref");
            }
            if (!dogrula_(*hedef, d, yol, hata, derinlik + 1)) {
                return false;
            }
        } else if (k == "type") {
            bool uydu = false;
            if (v.tur == Tur::metin) {
                uydu = tip_uyar(v.metin, d);
            } else if (v.tur == Tur::dizi) {
                for (const auto& t : v.dizi) {
                    if (t.tur == Tur::metin && tip_uyar(t.metin, d)) {
                        uydu = true;
                    }
                }
            }
            if (!uydu) {
                return ret("tip uymuyor");
            }
        } else if (k == "enum") {
            bool var = false;
            for (const auto& e : v.dizi) {
                if (json_esit(e, d)) {
                    var = true;
                    break;
                }
            }
            if (!var) {
                return ret("izinli değerlerden biri değil");
            }
        } else if (k == "const") {
            if (!json_esit(v, d)) {
                return ret("sabit değere eşit değil");
            }
        } else if (k == "properties" && d.tur == Tur::nesne) {
            for (const auto& [ad, alt] : v.nesne) {
                const json::Deger* a = d.bul(ad);
                if (a != nullptr) {
                    const std::size_t u = yol.size();
                    yol_ekle(yol, ad);
                    const bool ok = dogrula_(alt, *a, yol, hata, derinlik + 1);
                    yol.resize(u);
                    if (!ok) {
                        return false;
                    }
                }
            }
        } else if (k == "required" && d.tur == Tur::nesne) {
            for (const auto& r : v.dizi) {
                if (r.tur == Tur::metin && d.bul(r.metin) == nullptr) {
                    return ret("zorunlu alan yok: " + r.metin);
                }
            }
        } else if (k == "additionalProperties" && d.tur == Tur::nesne) {
            const json::Deger* props = sema.bul("properties");
            for (const auto& [ad, deger] : d.nesne) {
                if (props != nullptr && props->bul(ad) != nullptr) {
                    continue;
                }
                const std::size_t u = yol.size();
                yol_ekle(yol, ad);
                const bool ok = dogrula_(v, deger, yol, hata, derinlik + 1);
                yol.resize(u);
                if (!ok) {
                    return false;
                }
            }
        } else if (k == "propertyNames" && d.tur == Tur::nesne) {
            for (const auto& [ad, deger] : d.nesne) {
                (void)deger;
                json::Deger ad_degeri;
                ad_degeri.tur = Tur::metin;
                ad_degeri.metin = ad;
                if (!dogrula_(v, ad_degeri, yol, hata, derinlik + 1)) {
                    return false;
                }
            }
        } else if ((k == "maxProperties" || k == "minProperties") && d.tur == Tur::nesne) {
            const double n = static_cast<double>(d.nesne.size());
            if ((k == "maxProperties" && n > v.sayi) || (k == "minProperties" && n < v.sayi)) {
                return ret("üye sayısı sınır dışı");
            }
        } else if (k == "prefixItems" && d.tur == Tur::dizi) {
            for (std::size_t i = 0; i < v.dizi.size() && i < d.dizi.size(); ++i) {
                const std::size_t u = yol.size();
                yol_ekle(yol, std::to_string(i));
                const bool ok = dogrula_(v.dizi[i], d.dizi[i], yol, hata, derinlik + 1);
                yol.resize(u);
                if (!ok) {
                    return false;
                }
            }
        } else if (k == "items" && d.tur == Tur::dizi) {
            const json::Deger* on = sema.bul("prefixItems");
            const std::size_t bas = on != nullptr && on->tur == Tur::dizi ? on->dizi.size() : 0;
            for (std::size_t i = bas; i < d.dizi.size(); ++i) {
                const std::size_t u = yol.size();
                yol_ekle(yol, std::to_string(i));
                const bool ok = dogrula_(v, d.dizi[i], yol, hata, derinlik + 1);
                yol.resize(u);
                if (!ok) {
                    return false;
                }
            }
        } else if ((k == "minItems" || k == "maxItems") && d.tur == Tur::dizi) {
            const double n = static_cast<double>(d.dizi.size());
            if ((k == "maxItems" && n > v.sayi) || (k == "minItems" && n < v.sayi)) {
                return ret("öğe sayısı sınır dışı");
            }
        } else if (k == "uniqueItems" && d.tur == Tur::dizi && v.tur == Tur::mantiksal && v.mantik) {
            for (std::size_t i = 0; i < d.dizi.size(); ++i) {
                for (std::size_t j = i + 1; j < d.dizi.size(); ++j) {
                    if (json_esit(d.dizi[i], d.dizi[j])) {
                        return ret("yinelenen öğe");
                    }
                }
            }
        } else if (d.tur == Tur::sayi &&
                   (k == "minimum" || k == "maximum" || k == "exclusiveMinimum" || k == "exclusiveMaximum")) {
            const double x = d.sayi;
            const double s = v.sayi;
            const bool ok = k == "minimum"            ? x >= s
                            : k == "maximum"          ? x <= s
                            : k == "exclusiveMinimum" ? x > s
                                                      : x < s;
            if (!ok) {
                return ret("sayı aralık dışı");
            }
        } else if (k == "multipleOf" && d.tur == Tur::sayi) {
            const double q = d.sayi / v.sayi;
            if (!std::isfinite(q) || std::abs(q - std::round(q)) > 1e-9 * std::max(1.0, std::abs(q))) {
                return ret("katı değil");
            }
        } else if ((k == "minLength" || k == "maxLength") && d.tur == Tur::metin) {
            const double n = static_cast<double>(kod_noktasi_sayisi(d.metin));
            if ((k == "maxLength" && n > v.sayi) || (k == "minLength" && n < v.sayi)) {
                return ret("uzunluk sınır dışı");
            }
        } else if (k == "pattern" && d.tur == Tur::metin) {
            const json::Deger* azami = sema.bul("maxLength");
            if (azami != nullptr && static_cast<double>(kod_noktasi_sayisi(d.metin)) > azami->sayi) {
                return ret("uzunluk sınır dışı");
            }
            const Desen* p = v.tur == Tur::metin ? desen_(v.metin) : nullptr;
            if (p == nullptr || !p->ara(d.metin)) {
                return ret("desene uymuyor");
            }
        } else if (k == "allOf") {
            for (const auto& alt : v.dizi) {
                if (!dogrula_(alt, d, yol, hata, derinlik + 1)) {
                    return false;
                }
            }
        } else if (k == "anyOf") {
            bool biri = false;
            for (const auto& alt : v.dizi) {
                if (dogrula_(alt, d, yol, nullptr, derinlik + 1)) {
                    biri = true;
                    break;
                }
            }
            if (!biri) {
                return ret("anyOf seçeneklerinden hiçbirine uymuyor");
            }
        } else if (k == "oneOf") {
            int sayi = 0;
            for (const auto& alt : v.dizi) {
                if (dogrula_(alt, d, yol, nullptr, derinlik + 1)) {
                    ++sayi;
                }
            }
            if (sayi != 1) {
                return ret("oneOf seçeneklerinden tam birine uymuyor");
            }
        } else if (k == "not") {
            if (dogrula_(v, d, yol, nullptr, derinlik + 1)) {
                return ret("not şemasına uyuyor");
            }
        } else if (bilinen_sozcukler().count(k) == 0) {
            return ret("desteklenmeyen anahtar sözcük: " + k);
        }
    }
    return true;
}

}  // namespace ida::a3
