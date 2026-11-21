// Dayanıklılık (rastgele bozulmuş 10 000 dosya, sabit tohum; uç girdiler) ve çözümleme süresi.
#include "test_destek.hpp"

#include <algorithm>
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
    tek(std::string("x = \"") + std::string(2'000'000, 'a') + "\n", "kapanmamış uzun dizgi");
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
