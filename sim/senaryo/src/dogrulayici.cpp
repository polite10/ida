// Doğrulama akışı (A5-K90–K95): aşama 1 (her dosya için sözdizimi, sayılar, tip, şema),
// aşama 2 (birleştirme, çözümleme, anlamsal kurallar, A7 atıfları, çözümlenmiş şema).
// Bir aşamada hata varsa sonraki aşama koşmaz; bir aşamanın bütün bulguları toplanır.
#include "ic.hpp"

#include <fstream>
#include <iterator>

namespace ida::senaryo {

using ic::Bulgucu;

// ------------------------------------------------------------------ dosya kaynakları
DiskKaynagi::DiskKaynagi(std::filesystem::path kok) : kok_(std::move(kok)) {}

std::optional<std::string> DiskKaynagi::oku(const std::string& yol) const
{
    std::error_code ec;
    // Yol UTF-8'dir (Windows'ta dar dizgi ANSI kod sayfası sayılır; u8string ile kurulur).
    const std::filesystem::path p = kok_ / std::filesystem::path(std::u8string(yol.begin(), yol.end()));
    if (!std::filesystem::is_regular_file(p, ec)) {
        return std::nullopt;
    }
    const auto boy = std::filesystem::file_size(p, ec);
    if (ec || boy > kAzamiDosyaBoyu + 1) {
        // Sınırı aşan dosya baştan okunmaz; boyu aşan bir parça döndürülür ve okuyucu reddeder.
        if (!ec) {
            return std::string(kAzamiDosyaBoyu + 1, ' ');
        }
        return std::nullopt;
    }
    std::ifstream f(p, std::ios::binary);
    if (!f) {
        return std::nullopt;
    }
    std::string s((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    return s;
}

std::optional<std::string> BellekKaynagi::oku(const std::string& yol) const
{
    const auto it = dosyalar.find(yol);
    if (it == dosyalar.end()) {
        return std::nullopt;
    }
    return it->second;
}

// ------------------------------------------------------------------ rapor
std::size_t DogrulamaRaporu::hata_sayisi() const
{
    std::size_t n = 0;
    for (const auto& b : bulgular) {
        n += b.onem == Onem::hata ? 1u : 0u;
    }
    return n;
}

std::size_t DogrulamaRaporu::uyari_sayisi() const
{
    return bulgular.size() - hata_sayisi();
}

std::string DogrulamaRaporu::json() const
{
    Deger r = Deger::yeni_tablo();
    r.tablo["gecerli"] = Deger::yeni_mantiksal(gecerli);
    r.tablo["davranis_ozeti"] =
        davranis_ozeti ? Deger::yeni_dizgi(*davranis_ozeti) : Deger{};
    Deger ko = Deger::yeni_tablo();
    for (const auto& [d, o] : kaynak_ozetleri) {
        ko.tablo[d] = Deger::yeni_dizgi(o);
    }
    r.tablo["kaynak_ozetleri"] = std::move(ko);
    Deger bl = Deger::yeni_dizi();
    for (const auto& b : bulgular) {
        Deger x = Deger::yeni_tablo();
        x.tablo["kural"] = Deger::yeni_dizgi(b.kural);
        x.tablo["onem"] = Deger::yeni_dizgi(b.onem == Onem::hata ? "hata" : "uyari");
        x.tablo["asama"] = Deger::yeni_tamsayi(b.asama);
        x.tablo["dosya"] = Deger::yeni_dizgi(b.dosya);
        x.tablo["yol"] = Deger::yeni_dizgi(b.yol);
        x.tablo["satir"] = b.satir ? Deger::yeni_tamsayi(*b.satir) : Deger{};
        x.tablo["ileti"] = Deger::yeni_dizgi(b.ileti);
        bl.dizi.push_back(std::move(x));
    }
    r.tablo["bulgular"] = std::move(bl);
    return kanonik_json(r);
}

namespace {

void deneysel_uyarilari(const Deger& d, Yol& yol, Bulgucu& b)
{
    if (d.tablo_mu()) {
        for (const auto& [k, v] : d.tablo) {
            yol.parcalar.push_back(YolParcasi{k});
            if (deneysel_mi(k)) {
                b.uyari("A5-K07", v.konum, yol, "deneysel `x_` alanı: kabul edildi, hiçbir model okumaz");
            } else if (v.tablo_mu() || v.dizi_mi()) {
                deneysel_uyarilari(v, yol, b);
            }
            yol.parcalar.pop_back();
        }
    } else if (d.dizi_mi()) {
        for (std::size_t i = 0; i < d.dizi.size(); ++i) {
            if (d.dizi[i].tablo_mu() || d.dizi[i].dizi_mi()) {
                yol.parcalar.push_back(YolParcasi{i});
                deneysel_uyarilari(d.dizi[i], yol, b);
                yol.parcalar.pop_back();
            }
        }
    }
}

// Aşama 1'in dosya başına denetimleri (sözdizimi toml_oku'da).
void asama1_dosya(const Deger& kok, Bulgucu& b)
{
    const Yol y0;
    {
        Yol y;
        deneysel_uyarilari(kok, y, b);
    }
    ic::sema_dogrula(kok, "#", y0, ic::SemaKipi::toml, b);
    const Deger* surum = kok.yol_bul("meta.sema_surumu");
    if (surum != nullptr && surum->dizgi_mi()) {
        const std::string& s = surum->dizgi;
        if (s.rfind("1.", 0) == 0 && s.size() > 2 && s.size() <= 5 &&
            s.find_first_not_of("0123456789", 2) == std::string::npos) {
            const int minor = std::stoi(s.substr(2));
            if (minor > kSemaMinor) {
                b.hata("A5-K04", surum->konum, y0.ekle("meta").ekle("sema_surumu"),
                       "dosyanın minor sürümü (" + s + ") okuyucununkinden (1." +
                           std::to_string(kSemaMinor) + ") büyük");
            }
        }
    }
    const Deger* tur = kok.yol_bul("meta.tur");
    if (tur != nullptr && tur->dizgi_mi() && tur->dizgi == "cozumlenmis_senaryo") {
        b.hata("A5-K02", tur->konum, y0.ekle("meta").ekle("tur"),
               "çözümlenmiş senaryo TOML olarak okunmaz; yalnız A6'dan kanonik JSON olarak yeniden "
               "yüklenir");
    }
}

bool goreli_yol_gecerli(const std::string& p)
{
    if (p.empty() || p[0] == '/' || p.find('\\') != std::string::npos ||
        p.find(':') != std::string::npos) {
        return false;
    }
    std::size_t bas = 0;
    for (;;) {
        const std::size_t i = p.find('/', bas);
        const std::string parca = p.substr(bas, i == std::string::npos ? std::string::npos : i - bas);
        if (parca == "..") {
            return false;
        }
        if (i == std::string::npos) {
            return true;
        }
        bas = i + 1;
    }
}

bool uzanti_toml(const std::string& p)
{
    return p.size() > 5 && p.compare(p.size() - 5, 5, ".toml") == 0;
}

// A5-K115: A7 kümesinin manifesti.
std::optional<Deger> manifest_oku(const DosyaKaynagi& kaynak, const Deger& s, Bulgucu& b,
                                  DogrulamaRaporu& r)
{
    const Yol yv = Yol().ekle("varliklar");
    const Deger* kume = s.yol_bul("varliklar.kume");
    if (kume == nullptr || !kume->dizgi_mi()) {
        return std::nullopt;
    }
    const std::string yol = "assets/" + kume->dizgi + "/manifest.json";
    const std::optional<std::string> bayt = kaynak.oku(yol);
    if (!bayt) {
        b.hata("A5-K115", kume->konum, yv.ekle("kume"), "A7 kümesinin manifesti yok: " + yol);
        return std::nullopt;
    }
    const std::string ozet = sha256_hex(*bayt);
    r.kaynak_ozetleri[yol] = ozet;
    b.dosyalar.push_back(yol);
    const int no = static_cast<int>(b.dosyalar.size()) - 1;
    ic::JsonOkuma j = ic::json_oku(*bayt, no);
    if (!j.kok || !j.kok->tablo_mu()) {
        b.hata("A5-K115", kume->konum, yv.ekle("kume"),
               "A7 manifesti ayrıştırılamadı (" + yol + "): " + j.hata);
        return std::nullopt;
    }
    const Deger* ks = s.yol_bul("varliklar.kume_surumu");
    const Deger* ms = j.kok->bul("kume_surumu");
    if (ks == nullptr) {
        b.uyari("A5-K115", s.yol_bul("varliklar")->konum, yv.ekle("kume_surumu"),
                "kume_surumu verilmedi; manifestteki değer A6'ya yazılır");
    } else if (ms == nullptr || !esit(*ks, *ms)) {
        b.hata("A5-K115", ks->konum, yv.ekle("kume_surumu"),
               "kume_surumu manifestteki değerle aynı değil");
    }
    const Deger* mo = s.yol_bul("varliklar.manifest_ozeti");
    if (mo == nullptr) {
        b.uyari("A5-K115", s.yol_bul("varliklar")->konum, yv.ekle("manifest_ozeti"),
                "manifest_ozeti verilmedi; hesaplanan özet A6'ya yazılır: " + ozet);
    } else if (!mo->dizgi_mi() || mo->dizgi != ozet) {
        b.hata("A5-K115", mo->konum, yv.ekle("manifest_ozeti"),
               "manifest özeti uyuşmuyor; hesaplanan " + ozet);
    }
    return std::move(j.kok);
}

}  // namespace

CozumSonucu senaryo_coz(const DosyaKaynagi& kaynak, const std::string& senaryo_yolu)
{
    CozumSonucu sonuc;
    DogrulamaRaporu& r = sonuc.rapor;
    Bulgucu b;
    b.dosyalar.push_back(senaryo_yolu);
    b.varsayilan_dosya = 0;
    b.asama = 1;
    const Yol y0;

    auto bitir = [&]() {
        r.bulgular = std::move(b.bulgular);
        bulgulari_sirala(r.bulgular);
        r.gecerli = r.hata_sayisi() == 0 && r.davranis_ozeti.has_value();
        return std::move(sonuc);
    };

    // ---------------------------------------------------------------- aşama 1
    const std::optional<std::string> s_bayt = kaynak.oku(senaryo_yolu);
    if (!s_bayt) {
        b.hata("A5-K01", Konum{0, 0}, y0, "senaryo dosyası okunamadı");
        return bitir();
    }
    r.kaynak_ozetleri[senaryo_yolu] = sha256_hex(*s_bayt);
    if (!uzanti_toml(senaryo_yolu)) {
        b.hata("A5-K01", Konum{0, 0}, y0, "dosya uzantısı `.toml` olmalı");
    }
    ic::TomlOkuma st = ic::toml_oku(*s_bayt, 0, b);
    if (!st.kok) {
        return bitir();
    }
    Deger& s = *st.kok;
    asama1_dosya(s, b);
    const Deger* tur = s.yol_bul("meta.tur");
    if (tur != nullptr && tur->dizgi_mi() && tur->dizgi == "arac") {
        b.hata("A5-K02", tur->konum, y0.ekle("meta").ekle("tur"),
               "girdi bir senaryo dosyası olmalı; araç dosyası bir senaryonun `arac.dosya` "
               "başvurusuyla okunur");
        return bitir();
    }

    // Araç dosyası (A5-K12)
    std::optional<Deger> arac;
    const Deger* dosya = s.yol_bul("arac.dosya");
    if (dosya != nullptr && dosya->dizgi_mi()) {
        const Yol yd = y0.ekle("arac").ekle("dosya");
        if (!goreli_yol_gecerli(dosya->dizgi)) {
            b.hata("A5-K12", dosya->konum, yd,
                   "araç dosyası yolu senaryonun dizinine göre göreli, `/` ayırıcılı ve `..` "
                   "bileşensiz olmalı");
        } else if (!uzanti_toml(dosya->dizgi)) {
            b.hata("A5-K12", dosya->konum, yd, "araç dosyasının uzantısı `.toml` olmalı");
        } else {
            const std::string ay = ic::yol_birlestir(ic::dizin_adi(senaryo_yolu), dosya->dizgi);
            const std::optional<std::string> a_bayt = kaynak.oku(ay);
            if (!a_bayt) {
                b.hata("A5-K12", dosya->konum, yd, "başvurulan araç dosyası yok: " + ay);
            } else {
                r.kaynak_ozetleri[ay] = sha256_hex(*a_bayt);
                b.dosyalar.push_back(ay);
                ic::TomlOkuma at = ic::toml_oku(*a_bayt, 1, b);
                if (at.kok) {
                    const Deger* atur = at.kok->yol_bul("meta.tur");
                    if (atur == nullptr || !atur->dizgi_mi() || atur->dizgi != "arac") {
                        b.hata("A5-K12", dosya->konum, yd,
                               "başvurulan dosyanın meta.tur değeri \"arac\" değil: " + ay);
                    } else {
                        asama1_dosya(*at.kok, b);
                        arac = std::move(at.kok);
                    }
                }
            }
        }
    }
    if (b.hata_var() || !arac) {
        return bitir();
    }

    // ---------------------------------------------------------------- aşama 2
    b.asama = 2;
    ic::birlestir(s, *arac, b);
    if (const Deger* a = s.bul("arac"); a != nullptr) {
        // Senaryonun geçersiz kılmalarıyla birleşmiş araç, araç dosyasının şemasına uymalı
        // (senaryo şeması sensör ayarlarını türe göre denetleyemez).
        Deger kopya = *a;
        kopya.tablo.erase("dosya");
        ic::sema_dogrula(kopya, "#/$defs/arac_tam", y0.ekle("arac"), ic::SemaKipi::toml, b);
    }
    ic::desteklenmeyenler(s, b);
    if (b.hata_var()) {
        return bitir();
    }
    const std::optional<Deger> manifest = manifest_oku(kaynak, s, b, r);
    ic::cozumle(s, b);
    ic::anlamsal_kurallar(s, manifest ? &*manifest : nullptr, b);
    if (!b.hata_var()) {
        ic::fizik_denetimleri(s, manifest ? &*manifest : nullptr, b);
    }
    ic::sema_dogrula(s, "#", y0, ic::SemaKipi::toml, b);
    if (b.hata_var()) {
        return bitir();
    }
    sonuc.kanonik = kanonik_json(s);
    r.davranis_ozeti = sha256_hex(sonuc.kanonik);
    sonuc.cozumlenmis = std::move(s);
    return bitir();
}

YenidenYukleme cozumlenmis_yukle(std::string_view baytlar, std::string_view davranis_ozeti)
{
    YenidenYukleme y;
    Bulgucu b;
    b.dosyalar.push_back("cozumlenmis_senaryo.json");
    b.asama = 1;
    const Yol y0;
    auto bitir = [&]() {
        y.bulgular = std::move(b.bulgular);
        bulgulari_sirala(y.bulgular);
        return std::move(y);
    };
    if (sha256_hex(baytlar) != davranis_ozeti) {
        b.hata("A5-K02", Konum{0, 0}, y0, "baytların SHA-256'sı kayıttaki davranış özetiyle eşit değil");
        return bitir();
    }
    ic::JsonOkuma j = ic::json_oku(baytlar, 0);
    if (!j.kok) {
        b.hata("A5-K02", Konum{0, 0}, y0, "çözümlenmiş senaryo ayrıştırılamadı: " + j.hata);
        return bitir();
    }
    if (kanonik_json(*j.kok) != baytlar) {
        b.hata("A5-K02", Konum{0, 0}, y0, "baytlar RFC 8785 kanonik biçiminde değil");
        return bitir();
    }
    const Deger* tur = j.kok->yol_bul("meta.tur");
    if (tur == nullptr || !tur->dizgi_mi() || tur->dizgi != "cozumlenmis_senaryo") {
        b.hata("A5-K02", Konum{0, 0}, y0.ekle("meta").ekle("tur"),
               "meta.tur \"cozumlenmis_senaryo\" olmalı");
        return bitir();
    }
    ic::sema_dogrula(*j.kok, "#", y0, ic::SemaKipi::json, b);
    if (b.hata_var()) {
        return bitir();
    }
    y.tamam = true;
    y.cozumlenmis = std::move(j.kok);
    return bitir();
}

}  // namespace ida::senaryo
