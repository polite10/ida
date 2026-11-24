// Dayanıklılık (rastgele bozulmuş 10 000 dosya, sabit tohum; uç girdiler), Unicode belirteç
// üreteci (200 000 kısa girdi) ve çözümleme süresi.
#include "test_destek.hpp"

#include "ic.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <cstdio>

namespace ida::senaryo::test {

namespace {

// Platformdan bağımsız üreteç (splitmix64): std dağılımları platformlar arasında farklıdır.
class Uretec {
public:
    explicit Uretec(std::uint64_t t) : s_(t) {}
    std::uint64_t sonraki()
    {
        std::uint64_t z = (s_ += 0x9E3779B97F4A7C15ULL);
        z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
        z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
        return z ^ (z >> 31);
    }
    std::size_t asagi(std::size_t n) { return n == 0 ? 0 : static_cast<std::size_t>(sonraki() % n); }

private:
    std::uint64_t s_;
};

const char* const kParcalar[] = {
    "[[", "]]", "[", "]", "{", "}", "\"", "'''", "\"\"\"", "=", ".", ",", "\n", "#", "x_", "1e400",
    "1e-400", "nan", "inf", "-0.0", "4.9e-324", "1979-05-27T07:32:00Z", "07:32:00", "0x7FFFFFFFFFFFFFFF",
    "9007199254740993", "\xff", "\xc3", "\\u0000", "true", "[]", "{}", "a.b.c", "[olaylar]", "[[olaylar]]",
    "kimlik = \"E1\"", "sinif = \"sensor\"", "tur = \"nan\"", "hedef = \"imu0\"", "dosya = \"ida0.toml\"",
    "dosya = \"../x.toml\"", "etkin = false", "kume = \"yok\"", "sema_surumu = \"9.9\"", "\t", "\r\n"};

std::string boz(const std::string& m, Uretec& u)
{
    std::string o = m;
    const std::size_t n = 1 + u.asagi(4);
    for (std::size_t k = 0; k < n; ++k) {
        const std::size_t i = u.asagi(o.size() + 1);
        switch (u.asagi(6)) {
        case 0:  // bayt değiştir
            if (i < o.size()) {
                o[i] = static_cast<char>(u.asagi(256));
            }
            break;
        case 1:  // parça ekle
            o.insert(i, kParcalar[u.asagi(sizeof kParcalar / sizeof kParcalar[0])]);
            break;
        case 2: {  // aralık sil
            const std::size_t l = std::min<std::size_t>(o.size() - std::min(i, o.size()), u.asagi(64));
            o.erase(std::min(i, o.size()), l);
            break;
        }
        case 3: {  // bir satırı yinele
            const std::size_t b = o.rfind('\n', i == 0 ? 0 : i - 1);
            const std::size_t s = o.find('\n', i);
            const std::size_t bas = b == std::string::npos ? 0 : b + 1;
            const std::size_t son = s == std::string::npos ? o.size() : s;
            if (bas <= son) {
                o.insert(son, "\n" + o.substr(bas, son - bas));
            }
            break;
        }
        case 4:  // kes
            o.resize(i);
            break;
        default: {  // satırı başka bir yere taşı
            const std::size_t j = u.asagi(o.size() + 1);
            const std::size_t l = std::min<std::size_t>(o.size() - std::min(i, o.size()), u.asagi(80));
            const std::string parca = o.substr(std::min(i, o.size()), l);
            o.erase(std::min(i, o.size()), l);
            o.insert(std::min(j, o.size()), parca);
            break;
        }
        }
    }
    return o;
}

std::string k_ornek(const BellekKaynagi& k)
{
    return k.dosyalar.at(kSenaryo);
}

}  // namespace

int test_dayaniklilik(const TestBaglami& tb)
{
    Denetci d;
    const BellekKaynagi temel = temel_kaynak(tb);
    Uretec u(20261121ULL);
    constexpr int kDosyaSayisi = 10000;
    int gecerli = 0;
    const auto t0 = std::chrono::steady_clock::now();
    for (int i = 0; i < kDosyaSayisi; ++i) {
        BellekKaynagi k = temel;
        const char* hedef = (i % 4 == 3) ? kArac : (i % 8 == 5 ? kManifest : kSenaryo);
        k.dosyalar[hedef] = boz(k.dosyalar[hedef], u);
        const CozumSonucu s = senaryo_coz(k, kSenaryo);
        const auto& r = s.rapor;
        gecerli += r.gecerli ? 1 : 0;
        const bool tutarli = r.gecerli == r.davranis_ozeti.has_value() && r.gecerli == s.cozumlenmis.has_value() &&
                             (r.gecerli || r.hata_sayisi() > 0);
        DOGRULA(d, tutarli, "rapor tutarsız (dosya " + std::to_string(i) + ")");
        if (r.gecerli) {
            // Geçerli sayılan her çözüm yeniden yüklenebilir olmalı (A5-K02).
            const YenidenYukleme y = cozumlenmis_yukle(s.kanonik, *r.davranis_ozeti);
            DOGRULA(d, y.tamam, "geçerli çözüm yeniden yüklenemedi (dosya " + std::to_string(i) + ")\n" +
                                    bulgu_ozeti(DogrulamaRaporu{false, std::nullopt, {}, y.bulgular}));
        }
        for (const auto& b : r.bulgular) {
            DOGRULA(d, b.kural.rfind("A5-K", 0) == 0 && (b.asama == 1 || b.asama == 2), "bulgu biçimi: " + b.kural);
        }
        if (d.hata > 20) {
            break;
        }
    }
    const double sn = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    std::printf("  %d bozulmuş dosya, %d geçerli kaldı, toplam %.2f s (ölçüm, bu makine)\n", kDosyaSayisi, gecerli, sn);

    // Uç girdiler: sınır dışı boy ve derinlik çökme yerine A5-K01 ile reddedilir.
    const auto tek = [&](const std::string& metin, const char* ad) {
        BellekKaynagi k = temel;
        k.dosyalar[kSenaryo] = metin;
        const CozumSonucu s = senaryo_coz(k, kSenaryo);
        DOGRULA(d, !s.rapor.gecerli && bulgu_bul(s.rapor, "A5-K01", "") != nullptr, std::string(ad) + ": A5-K01 bekleniyordu\n" + bulgu_ozeti(s.rapor));
    };
    {
        std::string s = "[x_";
        for (int i = 0; i < 1'000'000; ++i) {
            s += ".a";
        }
        tek(s + "]\n", "derin başlık (1 000 000 düzey)");
    }
    {
        std::string s = "x_a = {";
        for (int i = 0; i < 200'000; ++i) {
            s += "a.";
        }
        tek(s + "b = 1}\n", "satır içi tabloda derin anahtar");
    }
    {
        std::string s = "x_a = ";
        s.append(100'000, '[');
        s.append(100'000, ']');
        tek(s + "\n", "derin dizi");
    }
    {
        std::string s = "x_a = {b = ";
        for (int i = 0; i < 50'000; ++i) {
            s += "{b = ";
        }
        tek(s + "1" + std::string(50'001, '}') + "\n", "derin satır içi tablo");
    }
    tek(std::string(kAzamiDosyaBoyu + 1, '#'), "dosya boyu sınırı");
    // toml++ 3.4.0'ın tanımsız davranış yolları (macOS CI UBSan; W4-mac-ci sol #5). Güvenlik
    // sınırı overlay'deki üst akım yamalarıdır (vcpkg-overlay/tomlplusplus): bu girdiler
    // toml++'ın olağan ayrıştırma hatasıyla A5-K01 olur. İlk CI tetikleyicisi bu testin 17.
    // bozuk dosyasıydı (tohum 20261121): yorumdaki `ğ`'nin bir sayının hemen arkasına kayması.
    {
        const std::string ozgun = k_ornek(temel);
        const auto t = degistir(ozgun, "2.7e-6, 40.3e-6]   # B_n (NED), T [MV]; WMM'den do", "2.7e-6");
        DOGRULA(d, t.has_value(), "tetikleyen girdi kurulamadı");
        if (t) {
            tek(*t, "sayının arkasında ASCII dışı karakter (CI tetikleyicisi)");
        }
    }
    tek("x_a=1.\"\xc5\x9f\"\n", "T1 (sol #5): sayı belirtecinde tırnak ve ş");
    tek("x_a = true\xc5\x9f\n", "mantıksal değerin arkasında ş");
    tek("\xcc\xaa", "#295: tek başına U+032A");
    tek("\xcf\xaa", "#295: tek başına U+03EA");
    tek("x_s = \"\"\"\\\xc4\x9f\"\"\"\n", "çok satırlı dizgide kaçıştan sonra ğ");
    tek("x_\xc3\xa7 = 1\n", "çıplak anahtarda ç");
    tek("x_a = [8.9, 176.0, }\n", "#321: dizide `}`");
    tek("x_a = [\n  # yorum\n}\n", "#321: boş dizide `}`");
    tek("x_a = [\xe2\x80\xa8]\n", "#321: dizide U+2028");
    tek("x_a = 1987-03-16T:\n", "#321: eksik saat");
    tek("x_a = 1987-03-16 1#\n", "#321/#313: tarihten sonra sayı ve yorum");
    tek("[[[x_a]]]\n", "#301: `[[[` başlığı");
    // A5-K01 TOML 1.0.0 ister; toml++ 3.5.x bunları koşulsuz kabul eder (TOML 1.1.0). Kitaplık
    // yükseltilirse bu satırlar düşer.
    tek("x_a = {x_b = 1,}\n", "TOML 1.1: satır içi tabloda sondaki virgül");
    tek("x_a = {\nx_b = 1\n}\n", "TOML 1.1: çok satırlı satır içi tablo");
    tek("x_a = \"\\e\"\n", "TOML 1.1: \\e kaçışı");
    tek("x_a = \"\\x41\"\n", "TOML 1.1: \\x kaçışı");
    tek("x_a = 07:32\n", "TOML 1.1: saniyesiz saat");
    {
        // Geçerli Unicode dizgiler kabul edilir: T2 (sol #5; #190 kalıbı: satır sonu kaçışından
        // sonra ASCII dışı karakter), dizgi ve yorumlarda ASCII dışı karakterler.
        BellekKaynagi k = temel;
        k.dosyalar[kSenaryo] = *degistir(k_ornek(temel), "ana_tohum = 1",
                                         "ana_tohum = 1  # yorum: ğüşıöç §\n"
                                         "x_s=\"\"\"\\\n\xc5\x9f\"\"\"\n"
                                         "x_p = \"\"\"\\\n\xc2\xa7\"\"\"\n"
                                         "x_t = \"ğüşıöç §\"\nx_l = 'ğ'\nx_m = '''\nğ\n'''\n"
                                         "x_b = \"\"\"\\\n   a ğ \u00a0 §\"\"\"\n");
        const CozumSonucu s = senaryo_coz(k, kSenaryo);
        const auto deger = [&](const char* y) {
            const Deger* v = s.cozumlenmis ? s.cozumlenmis->agac().yol_bul(y) : nullptr;
            return v != nullptr ? v->dizgi : std::string("<yok>");
        };
        DOGRULA(d, s.rapor.gecerli, "geçerli Unicode dizgiler kabul edilmeli\n" + bulgu_ozeti(s.rapor));
        DOGRULA(d, deger("rastgelelik.x_s") == "\xc5\x9f", "T2: değer `ş` olmalı: " + deger("rastgelelik.x_s"));
        DOGRULA(d, deger("rastgelelik.x_p") == "\xc2\xa7", "#190: değer `§` olmalı: " + deger("rastgelelik.x_p"));
        DOGRULA(d, deger("rastgelelik.x_b") == "a ğ \u00a0 §", "x_b değeri: " + deger("rastgelelik.x_b"));
    }
    // sol #2: yorumdaki tırnaklar ön taramayı atlatmaz (toml++ iddiası ve gizli derin başlık).
    tek("[x_a] # \"\"\"\n[!]\n# \"\"\"\n", "yorum arkasında geçersiz başlık");
    tek("[x_a] # '''\n[[\nx_b]]\n# '''\n", "yorum arkasında satır sonlu başlık");
    {
        std::string s = "[x_a] # \"\"\"\n[x_b";
        for (int i = 0; i < 1'000'000; ++i) {
            s += ".a";
        }
        tek(s + "]\n# \"\"\"\n", "yorum arkasına gizlenmiş derin başlık");
    }
    // sol #11: çok sayıda dönüşüm hatası ve alt normal değer tek geçişte (süre ölçülür).
    for (const char* yazim : {"1e400", "1e-400", "4.9e-324"}) {
        std::string s = k_ornek(temel);
        std::string dizi = "x_cok = [";
        for (int i = 0; i < 5000; ++i) {
            dizi += (i == 0 ? "" : ", ");
            dizi += yazim;
        }
        BellekKaynagi k = temel;
        k.dosyalar[kSenaryo] = *degistir(s, "ana_tohum = 1", "ana_tohum = 1\n" + dizi + "]");
        const auto a = std::chrono::steady_clock::now();
        const CozumSonucu r = senaryo_coz(k, kSenaryo);
        const double sure = std::chrono::duration<double>(std::chrono::steady_clock::now() - a).count();
        const bool beklenen = std::string(yazim) == "4.9e-324" ? r.rapor.gecerli : !r.rapor.gecerli;
        std::printf("  5 000 × %s: %s, %zu bulgu, %.3f s (ölçüm, bu makine)\n", yazim,
                    r.rapor.gecerli ? "geçerli" : "geçersiz", r.rapor.bulgular.size(), sure);
        DOGRULA(d, beklenen, std::string("çok sayıda ") + yazim + ": beklenen sonuç değil");
    }
    tek(std::string("x = \"") + std::string(2'000'000, 'a') + "\n", "kapanmamış uzun dizgi");
    return d.hata;
}

// W4-mac-ci sol #5: TOML sözdizimi belirteçleri ile toml++ 3.4.0'ın boşluk tablosunun tanımsız
// aralıklarındaki (U+00A1–U+0499, U+2F5E–U+3357, U+FB06–U+FEFE) kod noktalarının rastgele
// dizildiği kısa girdiler. Beklenen: çökme ya da tanımsız davranış yok (UBSan'lı derlemede
// sınanır); her girdi ya ayrıştırılır ya da yalnız aşama 1 kurallarıyla reddedilir.
int test_unicode_belirtecler(const TestBaglami&)
{
    Denetci d;
    Uretec u(0x5EED5EEDULL);
    const char* const belirtecler[] = {"\"", "'", "\"\"\"", "'''", "\\", "#", "=", ".", "[", "]", "{", "}",
                                       ",", "\n", "\r\n", " ", "x_a", "1", "1.5", "true"};
    const std::array<std::pair<char32_t, char32_t>, 3> araliklar = {
        {{0x00A1, 0x0499}, {0x2F5E, 0x3357}, {0xFB06, 0xFEFE}}};
    auto utf8 = [](char32_t c, std::string& o) {
        if (c < 0x800) {
            o += static_cast<char>(0xC0 | (c >> 6));
            o += static_cast<char>(0x80 | (c & 0x3F));
        } else {
            o += static_cast<char>(0xE0 | (c >> 12));
            o += static_cast<char>(0x80 | ((c >> 6) & 0x3F));
            o += static_cast<char>(0x80 | (c & 0x3F));
        }
    };
    constexpr int kGirdi = 200000;
    int ayristirilan = 0;
    const auto t0 = std::chrono::steady_clock::now();
    for (int i = 0; i < kGirdi; ++i) {
        std::string s;
        const std::size_t n = 1 + u.asagi(12);
        for (std::size_t k = 0; k < n; ++k) {
            if (u.asagi(3) == 0) {
                const auto& [alt, ust] = araliklar[u.asagi(araliklar.size())];
                char32_t c = static_cast<char32_t>(alt + u.asagi(static_cast<std::size_t>(ust - alt + 1)));
                if (c >= 0xD800 && c <= 0xDFFF) {
                    c = 0x00A7;
                }
                utf8(c, s);
            } else {
                s += belirtecler[u.asagi(sizeof belirtecler / sizeof belirtecler[0])];
            }
        }
        ic::Bulgucu b;
        b.dosyalar.push_back("u.toml");
        const ic::TomlOkuma t = ic::toml_oku(s, 0, b);
        ayristirilan += t.kok ? 1 : 0;
        for (const auto& x : b.bulgular) {
            const bool bilinen = x.kural == "A5-K01" || x.kural == "A5-K96" || x.kural == "A5-K97" || x.kural == "A5-K10";
            DOGRULA(d, bilinen && x.asama == 1, "beklenmeyen bulgu: " + x.kural);
        }
        DOGRULA(d, t.kok.has_value() || b.hata_var(), "ne ağaç ne hata (girdi " + std::to_string(i) + ")");
        if (d.hata > 20) {
            break;
        }
    }
    const double sn = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    std::printf("  %d kısa girdi, %d ayrıştırıldı, %.2f s (ölçüm, bu makine)\n", kGirdi, ayristirilan, sn);
    return d.hata;
}

int test_cozumleme_suresi(const TestBaglami& tb)
{
    const BellekKaynagi k = temel_kaynak(tb);
    constexpr int kTekrar = 50;
    std::vector<double> ms;
    for (int i = 0; i < kTekrar; ++i) {
        const auto t0 = std::chrono::steady_clock::now();
        const CozumSonucu s = senaryo_coz(k, kSenaryo);
        const auto t1 = std::chrono::steady_clock::now();
        if (!s.rapor.gecerli) {
            std::printf("  örnek geçersiz\n");
            return 1;
        }
        ms.push_back(std::chrono::duration<double, std::milli>(t1 - t0).count());
    }
    std::sort(ms.begin(), ms.end());
    std::printf("  ornek-havuz çözümleme süresi (%d tekrar; ölçüm, bu makine, eşik yok): en kısa %.3f ms, "
                "ortanca %.3f ms, en uzun %.3f ms\n",
                kTekrar, ms.front(), ms[ms.size() / 2], ms.back());
    return 0;
}

}  // namespace ida::senaryo::test
