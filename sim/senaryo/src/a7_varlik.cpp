// A7 varlık okuyucusu (a7_varlik.hpp; W4-sonar-sahne).
#include "ida/senaryo/a7_varlik.hpp"

#include "ic.hpp"

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstring>
#include <limits>
#include <map>
#include <set>
#include <utility>

namespace ida::senaryo {

std::string A7Hata::metin() const
{
    std::string s = "A7 " + kod + ": küme " + kume + ", varlık " + varlik;
    if (dugum >= 0) {
        s += ", düğüm " + std::to_string(dugum);
        if (!dugum_adi.empty()) {
            s += " (" + dugum_adi + ")";
        }
    }
    if (!ayrinti.empty()) {
        s += ": " + ayrinti;
    }
    return s;
}

namespace {

static_assert(std::endian::native == std::endian::little, "GLB okuyucusu küçük sonlu makine varsayar");

using V3 = std::array<double, 3>;

constexpr std::uint32_t kGlbSihir = 0x46546C67u;   // "glTF"
constexpr std::uint32_t kParcaJson = 0x4E4F534Au;
constexpr std::uint32_t kParcaBin = 0x004E4942u;
constexpr double kBirimTol = 1e-6;                  // A7-K13
constexpr double kKutuTol = 1e-3;                   // A7-K23 (mimari seçimi 1 mm)
constexpr double kTabanTol = 1e-3;                  // A7-K56 (mimari seçimi 1 mm)
constexpr double kDisbukeyTol = 1e-6;               // A7-K26

// Dönüşüm: p ↦ r · p + t (r satır öncelikli 3 × 3).
struct Don {
    std::array<double, 9> r{1, 0, 0, 0, 1, 0, 0, 0, 1};
    V3 t{0, 0, 0};
};

V3 uygula(const Don& d, const V3& p)
{
    return {d.r[0] * p[0] + d.r[1] * p[1] + d.r[2] * p[2] + d.t[0],
            d.r[3] * p[0] + d.r[4] * p[1] + d.r[5] * p[2] + d.t[1],
            d.r[6] * p[0] + d.r[7] * p[1] + d.r[8] * p[2] + d.t[2]};
}

// a ∘ b (önce b).
Don bilesik(const Don& a, const Don& b)
{
    Don c;
    for (std::size_t i = 0; i < 3; ++i) {
        for (std::size_t j = 0; j < 3; ++j) {
            c.r[3 * i + j] = a.r[3 * i] * b.r[j] + a.r[3 * i + 1] * b.r[3 + j] + a.r[3 * i + 2] * b.r[6 + j];
        }
    }
    const V3 bt = uygula({a.r, {0, 0, 0}}, b.t);
    c.t = {bt[0] + a.t[0], bt[1] + a.t[1], bt[2] + a.t[2]};
    return c;
}

V3 fark(const V3& a, const V3& b) { return {a[0] - b[0], a[1] - b[1], a[2] - b[2]}; }
V3 capraz(const V3& a, const V3& b)
{
    return {a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]};
}
double nokta(const V3& a, const V3& b) { return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]; }

std::uint32_t u32(std::string_view b, std::size_t i)
{
    std::uint32_t v = 0;
    std::memcpy(&v, b.data() + i, 4);
    return v;
}

// Hata yazıcı: ilk hata kalır.
class Ret {
public:
    Ret(A7Hata& h, std::string_view kume, std::string_view varlik) : h_(h)
    {
        h_.kume = std::string(kume);
        h_.varlik = std::string(varlik);
    }
    bool operator()(const char* kod, std::string ayrinti, int dugum = -1, std::string dugum_adi = {})
    {
        if (h_.kod.empty()) {
            h_.kod = kod;
            h_.dugum = dugum;
            h_.dugum_adi = std::move(dugum_adi);
            h_.ayrinti = std::move(ayrinti);
        }
        return false;
    }

private:
    A7Hata& h_;
};

// ----------------------------------------------------------------- JSON yardımcıları
bool tam_al(const Deger* d, std::int64_t& o)
{
    if (d == nullptr) return false;
    if (d->tur == Tur::tamsayi) {
        o = d->tam;
        return true;
    }
    if (d->tur == Tur::sayi && std::isfinite(d->sayi) && std::floor(d->sayi) == d->sayi &&
        std::abs(d->sayi) < 9.0e15) {
        o = static_cast<std::int64_t>(d->sayi);
        return true;
    }
    return false;
}

bool sayi_al(const Deger* d, double& o)
{
    if (d == nullptr || !d->sayisal_mi()) return false;
    o = d->cift();
    return std::isfinite(o);
}

std::string dizgi(const Deger* d) { return d != nullptr && d->dizgi_mi() ? d->dizgi : std::string(); }

const std::vector<Deger>* dizi(const Deger& kok, std::string_view ad)
{
    const Deger* d = kok.bul(ad);
    return d != nullptr && d->dizi_mi() ? &d->dizi : nullptr;
}

bool sayilar(const Deger* d, std::size_t n, double* o)
{
    if (d == nullptr || !d->dizi_mi() || d->dizi.size() != n) return false;
    for (std::size_t i = 0; i < n; ++i) {
        if (!sayi_al(&d->dizi[i], o[i])) return false;
    }
    return true;
}

bool bos_ya_da_yok(const Deger& kok, std::string_view ad)
{
    const Deger* d = kok.bul(ad);
    return d == nullptr || (d->dizi_mi() && d->dizi.empty()) || (d->tablo_mu() && d->tablo.empty());
}

// ----------------------------------------------------------------- GLB
struct Glb {
    Deger json;
    std::string_view bin;
};

bool glb_ayir(std::string_view b, Glb& g, Ret& ret)
{
    if (b.size() < 20u) return ret("GLTF", "GLB başlığı ve JSON parçası için dosya kısa (" + std::to_string(b.size()) + " B)");
    if (u32(b, 0) != kGlbSihir) return ret("GLTF", "GLB sihirli sayısı yok (beklenen 0x46546C67)");
    if (u32(b, 4) != 2u) return ret("GLTF", "GLB sürümü " + std::to_string(u32(b, 4)) + " (beklenen 2)");
    if (u32(b, 8) != b.size()) {
        return ret("GLTF", "GLB başlığındaki uzunluk " + std::to_string(u32(b, 8)) + ", dosya " + std::to_string(b.size()) + " B");
    }
    std::size_t off = 12;
    int parca = 0;
    while (off < b.size()) {
        if (b.size() - off < 8u) return ret("GLTF", "GLB parça başlığı kesik");
        const std::uint32_t boy = u32(b, off);
        const std::uint32_t tur = u32(b, off + 4);
        if (boy % 4u != 0u || boy > b.size() - off - 8u) return ret("GLTF", "GLB parça uzunluğu geçersiz");
        const std::string_view veri = b.substr(off + 8, boy);
        if (parca == 0) {
            if (tur != kParcaJson) return ret("GLTF", "ilk GLB parçası JSON değil");
            ic::JsonOkuma j = ic::json_oku(veri, 0);
            if (!j.kok || !j.kok->tablo_mu()) return ret("GLTF", "glTF JSON ayrıştırılamadı: " + j.hata);
            g.json = std::move(*j.kok);
        } else if (parca == 1) {
            if (tur != kParcaBin) return ret("GLTF", "ikinci GLB parçası BIN değil");
            g.bin = veri;
        } else {
            return ret("GLTF", "GLB'de JSON ve BIN dışında parça var");
        }
        ++parca;
        off += 8u + boy;
    }
    if (parca == 0) return ret("GLTF", "GLB'de JSON parçası yok");
    return true;
}

// A7-K10–K13 dosya düzeyi kuralları.
bool gltf_kurallari(const Glb& g, Ret& ret)
{
    const Deger& j = g.json;
    if (dizgi(j.yol_bul("asset.version")) != "2.0") return ret("GLTF", "asset.version 2.0 değil");
    // A7-K11
    if (!bos_ya_da_yok(j, "extensionsRequired")) return ret("UZANTI", "extensionsRequired dolu");
    if (const Deger* u = j.bul("extensionsUsed"); u != nullptr) {
        if (!u->dizi_mi()) return ret("UZANTI", "extensionsUsed dizi değil");
        for (const Deger& x : u->dizi) {
            const std::string s = dizgi(&x);
            if (s != "KHR_lights_punctual" && s != "KHR_materials_emissive_strength" && s != "KHR_texture_transform") {
                return ret("UZANTI", "izinsiz uzantı: " + s);
            }
        }
    }
    // A7-K10: dış URI yok; tek tampon GLB'nin BIN parçasıdır.
    {
        const auto* bs = dizi(j, "buffers");
        if (bs == nullptr || bs->size() != 1u) return ret("GLTF", "buffers tam bir eleman olmalı (GLB'nin BIN parçası)");
        if ((*bs)[0].bul("uri") != nullptr) return ret("GLTF", "tampon 0 dış URI kullanıyor");
        std::int64_t n = 0;
        if (!tam_al((*bs)[0].bul("byteLength"), n) || n < 0 || static_cast<std::uint64_t>(n) > g.bin.size()) {
            return ret("GLTF", "tampon 0 byteLength BIN parçasıyla uyuşmuyor");
        }
    }
    if (const auto* im = dizi(j, "images")) {
        for (std::size_t i = 0; i < im->size(); ++i) {
            if ((*im)[i].bul("uri") != nullptr) return ret("GLTF", "görüntü " + std::to_string(i) + " dış URI kullanıyor");
        }
    }
    // A7-K12
    for (const char* ad : {"skins", "animations", "cameras"}) {
        if (!bos_ya_da_yok(j, ad)) return ret("GLTF", std::string(ad) + " bulunmamalı");
    }
    if (const auto* ac = dizi(j, "accessors")) {
        for (std::size_t i = 0; i < ac->size(); ++i) {
            if ((*ac)[i].bul("sparse") != nullptr) return ret("GLTF", "seyrek erişimci " + std::to_string(i));
        }
    }
    const auto* erisimciler = dizi(j, "accessors");
    if (const auto* ms = dizi(j, "meshes")) {
        for (std::size_t i = 0; i < ms->size(); ++i) {
            const auto* ps = dizi((*ms)[i], "primitives");
            if (ps == nullptr || ps->empty()) return ret("GLTF", "ağ " + std::to_string(i) + " temel öğesiz");
            for (const Deger& p : *ps) {
                std::int64_t mod = 4;
                if (const Deger* m = p.bul("mode"); m != nullptr && !tam_al(m, mod)) mod = -1;
                if (mod != 4) return ret("GLTF", "ağ " + std::to_string(i) + ": mode " + std::to_string(mod) + " (beklenen 4)");
                if (!bos_ya_da_yok(p, "targets")) return ret("GLTF", "ağ " + std::to_string(i) + ": biçim hedefi");
                std::int64_t a = -1;
                if (!tam_al(p.yol_bul("attributes.POSITION"), a) || erisimciler == nullptr || a < 0 ||
                    static_cast<std::size_t>(a) >= erisimciler->size()) {
                    return ret("GLTF", "ağ " + std::to_string(i) + ": POSITION yok");
                }
                const Deger& e = (*erisimciler)[static_cast<std::size_t>(a)];
                std::int64_t ct = 0;
                if (!tam_al(e.bul("componentType"), ct) || ct != 5126 || dizgi(e.bul("type")) != "VEC3") {
                    return ret("GLTF", "ağ " + std::to_string(i) + ": POSITION float32 VEC3 değil");
                }
            }
        }
    }
    // A7-K13 (sahne sayısı da bu kuraldadır; sözleşme koduyla BIRIM).
    const auto* sc = dizi(j, "scenes");
    std::int64_t sahne = -1;
    if (sc == nullptr || sc->size() != 1u || !tam_al(j.bul("scene"), sahne) || sahne != 0) {
        return ret("BIRIM", "tam bir sahne ve scene = 0 olmalı");
    }
    if (const auto* ns = dizi(j, "nodes")) {
        for (std::size_t i = 0; i < ns->size(); ++i) {
            const Deger& n = (*ns)[i];
            const std::string ad = dizgi(n.bul("name"));
            if (n.bul("skin") != nullptr || n.bul("camera") != nullptr) {
                return ret("GLTF", "düğümde deri ya da kamera", static_cast<int>(i), ad);
            }
            if (const Deger* s = n.bul("scale")) {
                double v[3];
                if (!sayilar(s, 3, v)) return ret("GLTF", "scale geçersiz", static_cast<int>(i), ad);
                for (double x : v) {
                    if (std::abs(x - 1.0) > kBirimTol) {
                        return ret("BIRIM", "ölçek " + std::to_string(x) + " (beklenen 1)", static_cast<int>(i), ad);
                    }
                }
            }
            if (const Deger* m = n.bul("matrix")) {
                if (n.bul("translation") != nullptr || n.bul("rotation") != nullptr || n.bul("scale") != nullptr) {
                    return ret("GLTF", "matrix ile TRS birlikte", static_cast<int>(i), ad);
                }
                double v[16];
                if (!sayilar(m, 16, v)) return ret("GLTF", "matrix geçersiz", static_cast<int>(i), ad);
                // Sütun öncelikli; R(i, j) = v[4 j + i].
                auto R = [&](int a, int b) { return v[4 * b + a]; };
                bool ok = std::abs(v[3]) <= kBirimTol && std::abs(v[7]) <= kBirimTol && std::abs(v[11]) <= kBirimTol &&
                          std::abs(v[15] - 1.0) <= kBirimTol;
                for (int a = 0; a < 3 && ok; ++a) {
                    for (int b = 0; b < 3 && ok; ++b) {
                        const double d = R(0, a) * R(0, b) + R(1, a) * R(1, b) + R(2, a) * R(2, b);
                        ok = std::abs(d - (a == b ? 1.0 : 0.0)) <= kBirimTol;
                    }
                }
                const double det = R(0, 0) * (R(1, 1) * R(2, 2) - R(1, 2) * R(2, 1)) -
                                   R(0, 1) * (R(1, 0) * R(2, 2) - R(1, 2) * R(2, 0)) +
                                   R(0, 2) * (R(1, 0) * R(2, 1) - R(1, 1) * R(2, 0));
                if (!ok || std::abs(det - 1.0) > kBirimTol) {
                    return ret("BIRIM", "matrix birim ölçekli dönüş ve öteleme değil", static_cast<int>(i), ad);
                }
            }
        }
    }
    return true;
}

// Düğümün yerel dönüşümü (ebeveyn çerçevesinde).
bool yerel(const Deger& n, Don& d)
{
    d = Don{};
    if (const Deger* m = n.bul("matrix")) {
        double v[16];
        if (!sayilar(m, 16, v)) return false;
        for (std::size_t a = 0; a < 3; ++a) {
            for (std::size_t b = 0; b < 3; ++b) d.r[3 * a + b] = v[4 * b + a];
        }
        d.t = {v[12], v[13], v[14]};
        return true;
    }
    if (const Deger* t = n.bul("translation")) {
        if (!sayilar(t, 3, d.t.data())) return false;
    }
    if (const Deger* q = n.bul("rotation")) {
        double v[4];   // x, y, z, w
        if (!sayilar(q, 4, v)) return false;
        const double nn = std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2] + v[3] * v[3]);
        if (std::abs(nn - 1.0) > 1e-5) return false;
        const double x = v[0] / nn, y = v[1] / nn, z = v[2] / nn, w = v[3] / nn;
        d.r = {1 - 2 * (y * y + z * z), 2 * (x * y - w * z),     2 * (x * z + w * y),
               2 * (x * y + w * z),     1 - 2 * (x * x + z * z), 2 * (y * z - w * x),
               2 * (x * z - w * y),     2 * (y * z + w * x),     1 - 2 * (x * x + y * y)};
    }
    return true;
}

// Erişimcinin baytları: (başlangıç, adım, eleman boyu) ve sayı; sınırlar denetlenir.
struct Gorunum {
    std::size_t bas = 0, adim = 0, sayi = 0;
};

bool erisimci(const Glb& g, std::int64_t ai, std::size_t eleman, std::size_t bilesen, Gorunum& o, std::string& neden)
{
    const auto* ac = dizi(g.json, "accessors");
    const auto* bv = dizi(g.json, "bufferViews");
    if (ac == nullptr || ai < 0 || static_cast<std::size_t>(ai) >= ac->size()) {
        neden = "erişimci yok";
        return false;
    }
    const Deger& e = (*ac)[static_cast<std::size_t>(ai)];
    std::int64_t vi = -1, sayi = 0, eo = 0;
    if (!tam_al(e.bul("bufferView"), vi) || bv == nullptr || vi < 0 || static_cast<std::size_t>(vi) >= bv->size()) {
        neden = "erişimcinin bufferView'i yok";
        return false;
    }
    if (!tam_al(e.bul("count"), sayi) || sayi < 1) {
        neden = "erişimci sayısı geçersiz";
        return false;
    }
    if (e.bul("byteOffset") != nullptr && (!tam_al(e.bul("byteOffset"), eo) || eo < 0 || eo % static_cast<std::int64_t>(bilesen) != 0)) {
        neden = "erişimci byteOffset geçersiz";
        return false;
    }
    const Deger& v = (*bv)[static_cast<std::size_t>(vi)];
    std::int64_t tampon = -1, vo = 0, vl = 0, adim = static_cast<std::int64_t>(eleman);
    if (!tam_al(v.bul("buffer"), tampon) || tampon != 0) {
        neden = "bufferView tampon 0'ı göstermiyor";
        return false;
    }
    if ((v.bul("byteOffset") != nullptr && (!tam_al(v.bul("byteOffset"), vo) || vo < 0)) ||
        !tam_al(v.bul("byteLength"), vl) || vl < 1) {
        neden = "bufferView sınırları geçersiz";
        return false;
    }
    if (v.bul("byteStride") != nullptr && (!tam_al(v.bul("byteStride"), adim) || adim < static_cast<std::int64_t>(eleman) || adim > 252 || adim % 4 != 0)) {
        neden = "byteStride geçersiz";
        return false;
    }
    // Sınırlar çarpmadan, bölmeyle sınanır (64 bit taşma yok); sayi >= 1 yukarıda sağlandı.
    std::uint64_t sinir = g.bin.size();
    {
        const auto* bs = dizi(g.json, "buffers");
        std::int64_t bl = -1;
        if (bs == nullptr || bs->size() != 1u || !tam_al((*bs)[0].bul("byteLength"), bl) || bl < 0) {
            neden = "buffers[0].byteLength geçersiz";
            return false;
        }
        sinir = std::min<std::uint64_t>(sinir, static_cast<std::uint64_t>(bl));
    }
    if (static_cast<std::uint64_t>(vo) > sinir || static_cast<std::uint64_t>(vl) > sinir - static_cast<std::uint64_t>(vo)) {
        neden = "bufferView BIN parçasının dışında";
        return false;
    }
    const std::uint64_t vlu = static_cast<std::uint64_t>(vl);
    const std::uint64_t eou = static_cast<std::uint64_t>(eo);
    if (eou > vlu || eleman > vlu - eou ||
        static_cast<std::uint64_t>(sayi - 1) > (vlu - eou - eleman) / static_cast<std::uint64_t>(adim)) {
        neden = "erişimci bufferView'in dışına taşıyor";
        return false;
    }
    o.bas = static_cast<std::size_t>(vo + eo);
    o.adim = static_cast<std::size_t>(adim);
    o.sayi = static_cast<std::size_t>(sayi);
    return true;
}

// Rol: REF, görsel, yüzerlik, çarpışma, akustik, isabet, bağlantı noktası.
enum class Rol : std::uint8_t { ref, gorsel, yuzerlik, carpisma, akustik, isabet, baglanti };

struct RolGirdisi {
    Rol rol;
    std::int64_t dugum = -1;
    std::string ad;
    std::string kosul;
};

bool onekli(const std::string& s, const char* o) { return s.rfind(o, 0) == 0; }

bool onek_uyar(Rol r, const std::string& ad)
{
    switch (r) {
    case Rol::ref: return ad == "REF";
    case Rol::gorsel:
        return !(onekli(ad, "REF") || onekli(ad, "BUOY_") || onekli(ad, "COL_") || onekli(ad, "SON_") ||
                 onekli(ad, "MNT_") || onekli(ad, "THR_") || onekli(ad, "TUBE_") || onekli(ad, "HIT_"));
    case Rol::yuzerlik: return onekli(ad, "BUOY_");
    case Rol::carpisma: return onekli(ad, "COL_");
    case Rol::akustik: return onekli(ad, "SON_");
    case Rol::isabet: return onekli(ad, "HIT_");
    case Rol::baglanti: return onekli(ad, "MNT_") || onekli(ad, "THR_") || onekli(ad, "TUBE_");
    }
    return false;
}

bool ag_tasir(Rol r) { return r != Rol::ref && r != Rol::baglanti; }

// Düğümün ağının temel öğeleri → gövde çerçevesinde köşe ve üçgenler; temel öğe başına glTF
// malzeme indeksi (`malzeme_iste` ise zorunlu; A7-K28).
bool ag_oku(const Glb& g, std::size_t dugum, const std::string& ad, const Don& govde, A7Ag& o,
            std::vector<std::int64_t>& ucgen_gltf_malzeme, Ret& ret)
{
    const Deger& n = g.json.bul("nodes")->dizi[dugum];
    std::int64_t mi = -1;
    const auto* ms = dizi(g.json, "meshes");
    if (!tam_al(n.bul("mesh"), mi) || ms == nullptr || mi < 0 || static_cast<std::size_t>(mi) >= ms->size()) {
        return ret("DUGUM", "düğümün geçerli bir ağı yok", static_cast<int>(dugum), ad);
    }
    const auto* mats = dizi(g.json, "materials");
    o.dugum = static_cast<int>(dugum);
    o.ad = ad;
    for (const Deger& p : *dizi((*ms)[static_cast<std::size_t>(mi)], "primitives")) {
        std::int64_t mat = -1;
        if (!tam_al(p.bul("material"), mat) || mats == nullptr || mat < 0 || static_cast<std::size_t>(mat) >= mats->size()) {
            return ret("MALZEME", "temel öğenin glTF malzemesi yok", static_cast<int>(dugum), ad);
        }
        std::int64_t ai = -1;
        (void)tam_al(p.yol_bul("attributes.POSITION"), ai);
        Gorunum kg;
        std::string neden;
        if (!erisimci(g, ai, 12u, 4u, kg, neden)) return ret("GLTF", "POSITION: " + neden, static_cast<int>(dugum), ad);
        const std::size_t taban = o.koseler.size();
        for (std::size_t k = 0; k < kg.sayi; ++k) {
            float f[3];
            std::memcpy(f, g.bin.data() + kg.bas + k * kg.adim, 12);
            if (!std::isfinite(f[0]) || !std::isfinite(f[1]) || !std::isfinite(f[2])) {
                return ret("GLTF", "sonlu olmayan köşe", static_cast<int>(dugum), ad);
            }
            o.koseler.push_back(uygula(govde, {static_cast<double>(f[0]), static_cast<double>(f[1]), static_cast<double>(f[2])}));
        }
        std::vector<std::uint32_t> idx;
        if (const Deger* ie = p.bul("indices")) {
            std::int64_t ii = -1;
            const auto* ac = dizi(g.json, "accessors");
            if (!tam_al(ie, ii) || ac == nullptr || ii < 0 || static_cast<std::size_t>(ii) >= ac->size()) {
                return ret("GLTF", "indices erişimcisi yok", static_cast<int>(dugum), ad);
            }
            const Deger& e = (*ac)[static_cast<std::size_t>(ii)];
            std::int64_t ct = 0;
            std::size_t boy = 0;
            if (tam_al(e.bul("componentType"), ct)) {
                boy = ct == 5121 ? 1u : ct == 5123 ? 2u : ct == 5125 ? 4u : 0u;
            }
            if (boy == 0u || dizgi(e.bul("type")) != "SCALAR") {
                return ret("GLTF", "indices u8/u16/u32 SCALAR değil", static_cast<int>(dugum), ad);
            }
            Gorunum ig;
            if (!erisimci(g, ii, boy, boy, ig, neden)) return ret("GLTF", "indices: " + neden, static_cast<int>(dugum), ad);
            idx.resize(ig.sayi);
            for (std::size_t k = 0; k < ig.sayi; ++k) {
                const char* q = g.bin.data() + ig.bas + k * ig.adim;
                std::uint32_t v = 0;
                if (boy == 1u) {
                    v = static_cast<std::uint8_t>(*q);
                } else if (boy == 2u) {
                    std::uint16_t s = 0;
                    std::memcpy(&s, q, 2);
                    v = s;
                } else {
                    std::memcpy(&v, q, 4);
                }
                if (v >= kg.sayi) return ret("GLTF", "indeks köşe sayısını aşıyor", static_cast<int>(dugum), ad);
                idx[k] = v;
            }
        } else {
            idx.resize(kg.sayi);
            for (std::size_t k = 0; k < kg.sayi; ++k) idx[k] = static_cast<std::uint32_t>(k);
        }
        if (idx.size() % 3u != 0u) return ret("GLTF", "üçgen listesi 3'ün katı değil", static_cast<int>(dugum), ad);
        for (std::size_t k = 0; k < idx.size(); k += 3) {
            o.ucgenler.push_back({static_cast<std::uint32_t>(taban + idx[k]), static_cast<std::uint32_t>(taban + idx[k + 1]),
                                  static_cast<std::uint32_t>(taban + idx[k + 2])});
            ucgen_gltf_malzeme.push_back(mat);
        }
    }
    return true;
}

// A7-K25'teki tanımla kapalı ve manifold, dışa yönelik; `disbukey` ise A7-K26 dışbükeylik.
bool kapali_disbukey(const A7Ag& a, bool disbukey, Ret& ret)
{
    std::map<V3, std::uint32_t> kaynak;
    std::vector<std::uint32_t> es(a.koseler.size());
    std::vector<V3> tekil;
    for (std::size_t i = 0; i < a.koseler.size(); ++i) {
        const auto [it, yeni] = kaynak.emplace(a.koseler[i], static_cast<std::uint32_t>(tekil.size()));
        if (yeni) tekil.push_back(a.koseler[i]);
        es[i] = it->second;
    }
    std::map<std::pair<std::uint32_t, std::uint32_t>, int> kenar;
    double hacim6 = 0.0;
    for (const auto& u : a.ucgenler) {
        const std::uint32_t x = es[u[0]], y = es[u[1]], z = es[u[2]];
        if (x == y || y == z || x == z) return ret("KAPALI_DEGIL", "dejenere üçgen", a.dugum, a.ad);
        ++kenar[{x, y}];
        ++kenar[{y, z}];
        ++kenar[{z, x}];
        hacim6 += nokta(tekil[x], capraz(tekil[y], tekil[z]));
    }
    for (const auto& [k, n] : kenar) {
        const auto t = kenar.find({k.second, k.first});
        if (n != 1 || t == kenar.end() || t->second != 1) {
            return ret("KAPALI_DEGIL", "kenar tam iki üçgence karşıt yönde paylaşılmıyor", a.dugum, a.ad);
        }
    }
    if (!(hacim6 > 0.0)) return ret("KAPALI_DEGIL", "işaretli hacim ≤ 0 (yönelim içe)", a.dugum, a.ad);
    if (disbukey) {
        for (const auto& u : a.ucgenler) {
            const V3& p0 = tekil[es[u[0]]];
            V3 nn = capraz(fark(tekil[es[u[1]]], p0), fark(tekil[es[u[2]]], p0));
            const double l = std::sqrt(nokta(nn, nn));
            if (!(l > 0.0)) return ret("KAPALI_DEGIL", "sıfır alanlı üçgen", a.dugum, a.ad);
            nn = {nn[0] / l, nn[1] / l, nn[2] / l};
            for (const V3& p : tekil) {
                const double s = nokta(nn, fark(p, p0));
                if (s > kDisbukeyTol) {
                    return ret("DISBUKEY_DEGIL", "köşe üçgen düzleminin " + std::to_string(s) + " m önünde", a.dugum, a.ad);
                }
            }
        }
    }
    return true;
}

// +z ışını (orijinden) ile üçgenlerin ilk kesişimi (Möller–Trumbore); yoksa sonsuz.
double z_isini(const A7Ag& a)
{
    double en = std::numeric_limits<double>::infinity();
    const V3 d{0.0, 0.0, 1.0};
    for (const auto& u : a.ucgenler) {
        const V3& p0 = a.koseler[u[0]];
        const V3 e1 = fark(a.koseler[u[1]], p0);
        const V3 e2 = fark(a.koseler[u[2]], p0);
        const V3 p = capraz(d, e2);
        const double det = nokta(e1, p);
        if (std::abs(det) < 1e-15) continue;
        const V3 s{-p0[0], -p0[1], -p0[2]};
        const double uu = nokta(s, p) / det;
        if (uu < -1e-12 || uu > 1.0 + 1e-12) continue;
        const V3 q = capraz(s, e1);
        const double vv = nokta(d, q) / det;
        if (vv < -1e-12 || uu + vv > 1.0 + 1e-12) continue;
        const double t = nokta(e2, q) / det;
        if (t >= 0.0 && t < en) en = t;
    }
    return en;
}

}  // namespace

std::optional<A7Varlik> a7_varlik_yukle(const Deger& manifest, std::string_view kume, std::string_view kimlik,
                                        const DosyaKaynagi& kaynak, A7Hata& hata)
{
    hata = A7Hata{};
    Ret ret(hata, kume, kimlik);
    // A7-K14: eksen kabulü.
    {
        const Deger* e = manifest.bul("eksenler");
        double gg[9] = {}, gy[9] = {};
        auto matris = [](const Deger* d, double* o) {
            if (d == nullptr || !d->dizi_mi() || d->dizi.size() != 3u) return false;
            for (std::size_t k = 0; k < 3; ++k) {
                if (!sayilar(&d->dizi[k], 3, o + 3 * k)) return false;
            }
            return true;
        };
        bool ok = e != nullptr && dizgi(e->bul("gltf")) == "sag_el_y_yukari_z_on" && matris(e->bul("gltf_govde"), gg) &&
                  matris(e->bul("gltf_godot_yerel"), gy);
        const double bg[9] = {0, 0, 1, -1, 0, 0, 0, -1, 0};
        const double by[9] = {-1, 0, 0, 0, 1, 0, 0, 0, -1};
        for (int k = 0; ok && k < 9; ++k) ok = gg[k] == bg[k] && gy[k] == by[k];
        if (!ok) {
            ret("EKSEN", "manifestin eksenler alanı A7-K14'teki sabit dönüşümler değil");
            return std::nullopt;
        }
    }
    const Deger* girdi = nullptr;
    if (const auto* vs = dizi(manifest, "varliklar")) {
        for (const Deger& v : *vs) {
            if (dizgi(v.bul("kimlik")) == kimlik) girdi = &v;
        }
    }
    if (girdi == nullptr) {
        ret("SEMA", "manifestte böyle bir varlık yok");
        return std::nullopt;
    }
    A7Varlik out;
    out.kimlik = std::string(kimlik);
    out.tur = dizgi(girdi->bul("tur"));
    // A7-K03
    const std::string dosya = dizgi(girdi->bul("dosya"));
    {
        bool ok = dosya.size() > 4u && dosya.size() <= 52u && dosya.compare(dosya.size() - 4u, 4u, ".glb") == 0;
        for (std::size_t i = 0; ok && i + 4u < dosya.size(); ++i) {
            const char c = dosya[i];
            ok = (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_';
        }
        if (!ok) {
            ret("SEMA", "dosya adı geçersiz: " + dosya);
            return std::nullopt;
        }
    }
    // A7-K06: ayrıştırmadan önce özet ve boyut.
    const std::string yol = "assets/" + std::string(kume) + "/" + dosya;
    const std::optional<std::string> baytlar = kaynak.oku(yol);
    if (!baytlar) {
        ret("EKSIK_VARLIK", "dosya yok: " + yol);
        return std::nullopt;
    }
    {
        std::int64_t bayt = -1;
        const std::string ozet = sha256_hex(*baytlar);
        if (!tam_al(girdi->bul("bayt"), bayt) || bayt < 0 || static_cast<std::uint64_t>(bayt) != baytlar->size()) {
            ret("OZET_UYUSMAZ", yol + ": boyut " + std::to_string(baytlar->size()) + " B, manifestte " + std::to_string(bayt));
            return std::nullopt;
        }
        if (ozet != dizgi(girdi->bul("sha256"))) {
            ret("OZET_UYUSMAZ", yol + ": SHA-256 " + ozet + ", manifestte " + dizgi(girdi->bul("sha256")));
            return std::nullopt;
        }
    }
    Glb g;
    if (!glb_ayir(*baytlar, g, ret) || !gltf_kurallari(g, ret)) return std::nullopt;
    const Deger& j = g.json;
    const auto* nodes = dizi(j, "nodes");
    const std::size_t nn = nodes != nullptr ? nodes->size() : 0u;
    // A7-K19: ağaç yapısı ve erişilebilirlik.
    std::vector<std::int64_t> ebeveyn(nn, -1);
    for (std::size_t i = 0; i < nn; ++i) {
        if (const Deger* c = (*nodes)[i].bul("children")) {
            if (!c->dizi_mi()) {
                ret("GLTF", "children dizi değil", static_cast<int>(i));
                return std::nullopt;
            }
            for (const Deger& x : c->dizi) {
                std::int64_t k = -1;
                if (!tam_al(&x, k) || k < 0 || static_cast<std::size_t>(k) >= nn) {
                    ret("GLTF", "geçersiz çocuk indeksi", static_cast<int>(i));
                    return std::nullopt;
                }
                if (ebeveyn[static_cast<std::size_t>(k)] != -1 || static_cast<std::size_t>(k) == i) {
                    ret("ERISILEMEZ", "düğümün birden çok ebeveyni var ya da kendi çocuğu", static_cast<int>(k));
                    return std::nullopt;
                }
                ebeveyn[static_cast<std::size_t>(k)] = static_cast<std::int64_t>(i);
            }
        }
    }
    for (std::size_t i = 0; i < nn; ++i) {
        std::int64_t p = ebeveyn[i];
        for (std::size_t adim = 0; p >= 0; ++adim) {
            if (adim > nn) {
                ret("ERISILEMEZ", "düğüm hiyerarşisinde döngü", static_cast<int>(i));
                return std::nullopt;
            }
            p = ebeveyn[static_cast<std::size_t>(p)];
        }
    }
    std::vector<bool> kok(nn, false), erisilir(nn, false);
    const auto* kokler = dizi(j.bul("scenes")->dizi[0], "nodes");   // gltf_kurallari: tam bir sahne
    for (const Deger& x : kokler != nullptr ? *kokler : std::vector<Deger>{}) {
        std::int64_t k = -1;
        if (!tam_al(&x, k) || k < 0 || static_cast<std::size_t>(k) >= nn || ebeveyn[static_cast<std::size_t>(k)] != -1) {
            ret("ERISILEMEZ", "sahne kökü geçersiz ya da ebeveynli", static_cast<int>(k));
            return std::nullopt;
        }
        kok[static_cast<std::size_t>(k)] = true;
    }
    std::vector<Don> dunya(nn);
    for (std::size_t i = 0; i < nn; ++i) {
        // Kök zincirini yukarı doğru topla; erişilebilirlik kökte biter.
        std::vector<std::size_t> zincir{i};
        while (ebeveyn[zincir.back()] >= 0) zincir.push_back(static_cast<std::size_t>(ebeveyn[zincir.back()]));
        erisilir[i] = kok[zincir.back()];
        Don d;
        for (auto it = zincir.rbegin(); it != zincir.rend(); ++it) {
            Don y;
            if (!yerel((*nodes)[*it], y)) {
                ret("GLTF", "düğüm dönüşümü geçersiz", static_cast<int>(*it), dizgi((*nodes)[*it].bul("name")));
                return std::nullopt;
            }
            d = bilesik(d, y);
        }
        dunya[i] = d;
    }
    // A7-K20: adı REF olan düğüm tam bir tane; A7-K21: ağ taşıyan her düğümün mesh indeksi dizi içinde.
    {
        std::size_t ref_sayisi = 0;
        const auto* ms = dizi(j, "meshes");
        for (std::size_t i = 0; i < nn; ++i) {
            if (dizgi((*nodes)[i].bul("name")) == "REF") ++ref_sayisi;
            if (const Deger* m = (*nodes)[i].bul("mesh")) {
                std::int64_t mi = -1;
                if (!tam_al(m, mi) || ms == nullptr || mi < 0 || static_cast<std::size_t>(mi) >= ms->size()) {
                    ret("GLTF", "düğümün mesh indeksi meshes dizisinin dışında", static_cast<int>(i), dizgi((*nodes)[i].bul("name")));
                    return std::nullopt;
                }
            }
        }
        if (ref_sayisi != 1u) {
            ret("DUGUM", "adı REF olan düğüm sayısı " + std::to_string(ref_sayisi) + " (beklenen 1)");
            return std::nullopt;
        }
    }
    // Roller (A7-K20, K21).
    std::vector<RolGirdisi> roller;
    {
        const Deger* r = girdi->bul("referans");
        RolGirdisi x{Rol::ref, -1, dizgi(r != nullptr ? r->bul("ad") : nullptr), "her_zaman"};
        (void)tam_al(r != nullptr ? r->bul("dugum") : nullptr, x.dugum);
        roller.push_back(x);
    }
    const std::pair<const char*, Rol> listeler[] = {{"gorsel", Rol::gorsel},     {"yuzerlik", Rol::yuzerlik},
                                                     {"carpisma", Rol::carpisma}, {"akustik", Rol::akustik},
                                                     {"isabet_bolgeleri", Rol::isabet},
                                                     {"baglanti_noktalari", Rol::baglanti}};
    for (const auto& [ad, rol] : listeler) {
        if (const auto* l = dizi(*girdi, ad)) {
            for (const Deger& x : *l) {
                RolGirdisi y{rol, -1, dizgi(x.bul("ad")), dizgi(x.bul("kosul"))};
                if (!tam_al(x.bul("dugum"), y.dugum)) y.dugum = -1;
                if (y.kosul.empty()) y.kosul = "her_zaman";
                roller.push_back(y);
            }
        }
    }
    std::vector<int> rol_sayisi(nn, 0);
    for (const RolGirdisi& r : roller) {
        if (r.dugum < 0 || static_cast<std::size_t>(r.dugum) >= nn) {
            ret("DUGUM", "rol düğümü dosyada yok (" + r.ad + ")", static_cast<int>(r.dugum), r.ad);
            return std::nullopt;
        }
        const Deger& n = (*nodes)[static_cast<std::size_t>(r.dugum)];
        const std::string ad = dizgi(n.bul("name"));
        const int di = static_cast<int>(r.dugum);
        if (ad != r.ad) {
            ret("DUGUM", "düğüm adı " + ad + ", manifestte " + r.ad, di, ad);
            return std::nullopt;
        }
        if (!onek_uyar(r.rol, ad)) {
            ret("DUGUM", "ad önekiyle rol uyuşmuyor", di, ad);
            return std::nullopt;
        }
        if (++rol_sayisi[static_cast<std::size_t>(r.dugum)] > 1) {
            ret("DUGUM", "düğüm birden çok rolde", di, ad);
            return std::nullopt;
        }
        if ((n.bul("mesh") != nullptr) != ag_tasir(r.rol)) {
            ret("DUGUM", ag_tasir(r.rol) ? "rol düğümü ağ taşımıyor" : "referans/bağlantı düğümü ağ taşıyor", di, ad);
            return std::nullopt;
        }
        if (!erisilir[static_cast<std::size_t>(r.dugum)]) {
            ret("ERISILEMEZ", "rol düğümü scenes[0]'dan erişilemiyor", di, ad);
            return std::nullopt;
        }
        if (r.rol == Rol::ref) {
            Don y;
            (void)yerel(n, y);
            bool birim = kok[static_cast<std::size_t>(r.dugum)];
            for (int k = 0; birim && k < 9; ++k) birim = std::abs(y.r[static_cast<std::size_t>(k)] - (k % 4 == 0 ? 1.0 : 0.0)) <= kBirimTol;
            if (!birim) {
                ret("DUGUM", "REF sahne kökü değil ya da dönüşü birim değil", di, ad);
                return std::nullopt;
            }
        }
        // A7-K24: koşullu düğüm yalnız araçta.
        if (r.kosul != "her_zaman" && out.tur != "arac") {
            ret("TUR", "koşullu düğüm (" + r.kosul + ") yalnız arac türünde bulunur", di, ad);
            return std::nullopt;
        }
    }
    for (std::size_t i = 0; i < nn; ++i) {
        if ((*nodes)[i].bul("mesh") != nullptr) {
            if (rol_sayisi[i] == 0) {
                ret("DUGUM", "ağ taşıyan düğüm hiçbir rolde yok", static_cast<int>(i), dizgi((*nodes)[i].bul("name")));
                return std::nullopt;
            }
            if (!erisilir[i]) {
                ret("ERISILEMEZ", "ağ taşıyan düğüm scenes[0]'dan erişilemiyor", static_cast<int>(i), dizgi((*nodes)[i].bul("name")));
                return std::nullopt;
            }
        }
    }
    auto say = [&](Rol r, bool her_zaman) {
        std::size_t n = 0;
        for (const RolGirdisi& x : roller) n += (x.rol == r && (!her_zaman || x.kosul == "her_zaman")) ? 1u : 0u;
        return n;
    };
    if (say(Rol::gorsel, false) == 0u || say(Rol::carpisma, false) == 0u ||
        (out.tur == "arac" && say(Rol::yuzerlik, true) == 0u)) {
        ret("TUR", out.tur + " türünün gerektirdiği rol eksik");
        return std::nullopt;
    }
    // Gövde çerçevesi (A7-K14, K20): p_b = M (p_kök − t_REF).
    const V3 t_ref = dunya[static_cast<std::size_t>(roller.front().dugum)].t;
    Don m;
    m.r = {0, 0, 1, -1, 0, 0, 0, -1, 0};
    m.t = uygula({m.r, {0, 0, 0}}, {-t_ref[0], -t_ref[1], -t_ref[2]});
    // Malzeme ataması (A7-K28).
    std::map<std::int64_t, std::string> atama;
    {
        std::map<std::int64_t, int> kez;
        if (const auto* l = dizi(*girdi, "malzeme_atamasi")) {
            for (const Deger& x : *l) {
                std::int64_t gm = -1;
                if (!tam_al(x.bul("gltf_malzeme"), gm)) gm = -1;
                ++kez[gm];
                atama[gm] = dizgi(x.bul("malzeme"));
            }
        }
        for (const auto& [gm, k] : kez) {
            if (k != 1) {
                ret("MALZEME", "glTF malzemesi " + std::to_string(gm) + " atamada " + std::to_string(k) + " kez");
                return std::nullopt;
            }
        }
    }
    std::set<std::string> bilinen;
    if (const auto* l = dizi(manifest, "malzemeler")) {
        for (const Deger& x : *l) bilinen.insert(dizgi(x.bul("kimlik")));
    }
    // Ağlar: görsel ve çarpışma (sınır kutusu, A7-K23), akustik (A7-K27).
    V3 kmin{std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity(),
            std::numeric_limits<double>::infinity()};
    V3 kmax{-kmin[0], -kmin[1], -kmin[2]};
    std::vector<A7Ag> carpisma;
    std::vector<std::string> ayristirma;
    out.akustik_carpismadan = say(Rol::akustik, false) == 0u;
    std::map<std::string, std::uint16_t> malzeme_indisi;
    for (std::size_t ri = 0; ri < roller.size(); ++ri) {
        const RolGirdisi& r = roller[ri];
        if (r.rol != Rol::gorsel && r.rol != Rol::carpisma && r.rol != Rol::akustik) continue;
        const std::size_t di = static_cast<std::size_t>(r.dugum);
        A7Ag a;
        std::vector<std::int64_t> gm;
        if (!ag_oku(g, di, r.ad, bilesik(m, dunya[di]), a, gm, ret)) return std::nullopt;
        for (std::size_t k = 0; k < a.ucgenler.size(); ++k) {
            const auto it = atama.find(gm[k]);
            if (it == atama.end()) {
                ret("MALZEME", "glTF malzemesi " + std::to_string(gm[k]) + " atanmamış", static_cast<int>(di), r.ad);
                return std::nullopt;
            }
            if (bilinen.count(it->second) == 0u) {
                ret("MALZEME", "malzemeler[]'de olmayan kimlik: " + it->second, static_cast<int>(di), r.ad);
                return std::nullopt;
            }
        }
        if (r.rol != Rol::akustik) {
            for (const V3& p : a.koseler) {
                for (int k = 0; k < 3; ++k) {
                    kmin[static_cast<std::size_t>(k)] = std::min(kmin[static_cast<std::size_t>(k)], p[static_cast<std::size_t>(k)]);
                    kmax[static_cast<std::size_t>(k)] = std::max(kmax[static_cast<std::size_t>(k)], p[static_cast<std::size_t>(k)]);
                }
            }
        }
        const bool akustik = r.rol == Rol::akustik || (r.rol == Rol::carpisma && out.akustik_carpismadan);
        if (akustik) {
            for (std::size_t k = 0; k < a.ucgenler.size(); ++k) {
                const std::string& mk = atama.at(gm[k]);
                const auto [it, yeni] = malzeme_indisi.emplace(mk, static_cast<std::uint16_t>(out.malzemeler.size()));
                if (yeni) out.malzemeler.push_back(mk);
                a.ucgen_malzeme.push_back(it->second);
            }
        }
        if (r.rol == Rol::carpisma) {
            // Ayrıştırma türü manifestteki girdiden (aynı sıra: carpisma[] listesi).
            std::string ay;
            for (const Deger& x : *dizi(*girdi, "carpisma")) {   // rol listesi buradan okundu
                std::int64_t d = -1;
                if (tam_al(x.bul("dugum"), d) && d == r.dugum) ay = dizgi(x.bul("ayristirma"));
            }
            if (ay != "yok" && ay != "coacd") {
                ret("SEMA", "çarpışma girdisinde ayristirma yok ya da 'yok'/'coacd' değil", static_cast<int>(r.dugum), r.ad);
                return std::nullopt;
            }
            out.carpisma_kaynak_ucgen += a.ucgenler.size();
            if (!kapali_disbukey(a, ay == "yok", ret)) return std::nullopt;
            if (akustik) {
                out.akustik_ucgen += a.ucgenler.size();
                out.akustik.push_back(a);
            }
            carpisma.push_back(std::move(a));
        } else if (r.rol == Rol::akustik) {
            out.akustik_ucgen += a.ucgenler.size();
            out.akustik.push_back(std::move(a));
        }
    }
    // A7-K23
    {
        const Deger* sk = girdi->bul("sinir_kutusu_govde_m");
        double mn[3], mx[3];
        if (sk == nullptr || !sayilar(sk->bul("en_kucuk"), 3, mn) || !sayilar(sk->bul("en_buyuk"), 3, mx)) {
            ret("SEMA", "sinir_kutusu_govde_m geçersiz");
            return std::nullopt;
        }
        for (int k = 0; k < 3; ++k) {
            const double a = std::abs(kmin[static_cast<std::size_t>(k)] - mn[k]);
            const double b = std::abs(kmax[static_cast<std::size_t>(k)] - mx[k]);
            if (!(a <= kKutuTol + 1e-12) || !(b <= kKutuTol + 1e-12)) {
                ret("BIRIM", "sınır kutusu ekseni " + std::to_string(k) + ": ağlardan [" + std::to_string(kmin[static_cast<std::size_t>(k)]) +
                                 ", " + std::to_string(kmax[static_cast<std::size_t>(k)]) + "], manifestte [" + std::to_string(mn[k]) +
                                 ", " + std::to_string(mx[k]) + "]");
                return std::nullopt;
            }
        }
    }
    // A7-K56
    if (const Deger* td = girdi->bul("taban_derinligi_m")) {
        double beyan = 0.0;
        if (out.tur != "statik" || !sayi_al(td, beyan)) {
            ret("SEMA", "taban_derinligi_m yalnız statik varlıkta, sayı");
            return std::nullopt;
        }
        double en = std::numeric_limits<double>::infinity();
        for (const A7Ag& a : carpisma) en = std::min(en, z_isini(a));
        if (!std::isfinite(en) || std::abs(en - beyan) > kTabanTol) {
            ret("TABAN", "REF'ten +z ışını " + (std::isfinite(en) ? std::to_string(en) + " m'de" : std::string("hiçbir ağa çarpmadı")) +
                             ", beyan " + std::to_string(beyan) + " m");
            return std::nullopt;
        }
    }
    return out;
}

}  // namespace ida::senaryo
