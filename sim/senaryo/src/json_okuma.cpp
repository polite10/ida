// JSON okuma (nlohmann-json SAX arayüzü): A7 manifesti ve çözümlenmiş senaryonun yeniden
// yüklenmesi (A5-K02). Kayan noktalı sayılar ayrıştırıcının özgün yazımından `ondalik_cevir`
// ile çevrilir (platformdan bağımsız doğru yuvarlama). Yinelenen anahtar, derinlik sınırının
// aşılması ve 64 bite sığmayan tamsayı ret nedenidir. Ayrıştırıcı özyinelemesizdir; kurucu
// açık bir yığın kullanır.
#include "ic.hpp"

#include <nlohmann/json.hpp>

#include <cmath>
#include <limits>
#include <vector>

namespace ida::senaryo::ic {

namespace {

constexpr std::int64_t kTamsayiSiniri = 9007199254740991;  // 2^53 − 1

class Kurucu {
public:
    using number_integer_t = std::int64_t;
    using number_unsigned_t = std::uint64_t;
    using number_float_t = double;
    using string_t = std::string;
    using binary_t = nlohmann::json::binary_t;

    explicit Kurucu(int dosya_no) : dosya_(dosya_no) {}

    std::optional<Deger> kok;
    std::string hata;

    bool null() { return deger(Deger{}); }
    bool boolean(bool v) { return deger(Deger::yeni_mantiksal(v)); }
    // JSON'da (RFC 8785 çıktısında) tamsayı biçimli belirteç bir binary64 sayısının
    // dizilimidir: |v| > 2^53 − 1 ise kayıpsız TOML tamsayısı değil, binary64 sayı olarak
    // tutulur (ör. 1e19 → "10000000000000000000"). Tamsayı tipli alanların sınırı şemada.
    bool number_integer(number_integer_t v)
    {
        if (v > kTamsayiSiniri || v < -kTamsayiSiniri) {
            return deger(Deger::yeni_sayi(static_cast<double>(v)));
        }
        return deger(Deger::yeni_tamsayi(v));
    }
    bool number_unsigned(number_unsigned_t v)
    {
        if (v > static_cast<std::uint64_t>(kTamsayiSiniri)) {
            return deger(Deger::yeni_sayi(static_cast<double>(v)));
        }
        return deger(Deger::yeni_tamsayi(static_cast<std::int64_t>(v)));
    }
    bool number_float(number_float_t, const string_t& yazim)
    {
        const std::optional<double> v = ondalik_cevir(yazim);
        if (!v || !std::isfinite(*v) || (*v == 0.0 && sifir_olmayan_basamak_var(yazim))) {
            hata = "sayı binary64'e çevrilemiyor: " + yazim;
            return false;
        }
        return deger(Deger::yeni_sayi(*v == 0.0 ? 0.0 : *v));
    }
    bool string(string_t& s) { return deger(Deger::yeni_dizgi(s)); }
    bool binary(binary_t&)
    {
        hata = "ikili değer";
        return false;
    }
    bool start_object(std::size_t)
    {
        return ac(Deger::yeni_tablo());
    }
    bool key(string_t& k)
    {
        if (yigin_.back()->bul(k) != nullptr) {
            hata = "yinelenen anahtar: " + k;
            return false;
        }
        anahtar_ = k;
        return true;
    }
    bool end_object()
    {
        yigin_.pop_back();
        return true;
    }
    bool start_array(std::size_t)
    {
        return ac(Deger::yeni_dizi());
    }
    bool end_array()
    {
        yigin_.pop_back();
        return true;
    }
    bool parse_error(std::size_t konum, const std::string&, const nlohmann::json::exception& e)
    {
        hata = std::string("JSON sözdizimi hatası (bayt ") + std::to_string(konum) + "): " + e.what();
        return false;
    }

private:
    int dosya_;
    std::vector<Deger*> yigin_;
    std::string anahtar_;

    Deger* yerlestir(Deger d)
    {
        d.konum = Konum{dosya_, 0};
        if (yigin_.empty()) {
            kok = std::move(d);
            return &*kok;
        }
        Deger* ust = yigin_.back();
        if (ust->tur == Tur::tablo) {
            Deger& yer = ust->tablo[anahtar_];
            yer = std::move(d);
            return &yer;
        }
        ust->dizi.push_back(std::move(d));
        return &ust->dizi.back();
    }
    bool deger(Deger d)
    {
        yerlestir(std::move(d));
        return true;
    }
    bool ac(Deger d)
    {
        if (yigin_.size() >= kAzamiDerinlik) {
            hata = "iç içe yapı derinliği uygulama sınırını aşıyor";
            return false;
        }
        yigin_.push_back(yerlestir(std::move(d)));
        return true;
    }
};

}  // namespace

JsonOkuma json_oku(std::string_view metin, int dosya_no)
{
    JsonOkuma sonuc;
    if (metin.size() > kAzamiDosyaBoyu) {
        sonuc.hata = "dosya boyu uygulama sınırını aşıyor";
        return sonuc;
    }
    Kurucu k(dosya_no);
    bool tamam = false;
    try {
        tamam = nlohmann::json::sax_parse(metin.begin(), metin.end(), &k,
                                          nlohmann::json::input_format_t::json, true, false);
    } catch (const std::exception& e) {
        k.hata = e.what();
        tamam = false;
    }
    if (!tamam || !k.kok) {
        sonuc.hata = k.hata.empty() ? "JSON ayrıştırılamadı" : k.hata;
        return sonuc;
    }
    sonuc.kok = std::move(k.kok);
    return sonuc;
}

}  // namespace ida::senaryo::ic
