// http.cpp — bkz. http.hpp.
#include "http.hpp"

#include <cstring>

namespace ida::a3::http {

namespace {

bool tchar(char c) noexcept
{
    if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')) {
        return true;
    }
    return std::strchr("!#$%&'*+-.^_`|~", c) != nullptr && c != '\0';
}

char kucuk(char c) noexcept { return (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c; }

std::string_view kirp(std::string_view s) noexcept
{
    while (!s.empty() && (s.front() == ' ' || s.front() == '\t')) {
        s.remove_prefix(1);
    }
    while (!s.empty() && (s.back() == ' ' || s.back() == '\t')) {
        s.remove_suffix(1);
    }
    return s;
}

}  // namespace

const std::string* Istek::baslik(std::string_view ad) const noexcept
{
    for (const auto& [k, v] : basliklar) {
        if (k == ad) {
            return &v;
        }
    }
    return nullptr;
}

std::size_t Istek::baslik_sayisi(std::string_view ad) const noexcept
{
    std::size_t n = 0;
    for (const auto& [k, v] : basliklar) {
        (void)v;
        if (k == ad) {
            ++n;
        }
    }
    return n;
}

Sonuc baslik_ayristir(std::string_view tampon, Istek& istek, std::size_t& blok_boyu, std::string& neden)
{
    const std::size_t ara = tampon.size() < kAzamiBaslikBlogu + 4 ? tampon.size() : kAzamiBaslikBlogu + 4;
    const std::size_t son = tampon.substr(0, ara).find("\r\n\r\n");
    if (son == std::string_view::npos) {
        if (tampon.size() > kAzamiBaslikBlogu) {
            neden = "başlık bloğu sınırı aşıldı";
            return Sonuc::hata;
        }
        return Sonuc::eksik;
    }
    blok_boyu = son + 4;
    istek = Istek{};
    std::string_view blok = tampon.substr(0, son + 2);  // son satırın CRLF'i dahil
    // İstek satırı
    std::size_t satir_sonu = blok.find("\r\n");
    std::string_view satir = blok.substr(0, satir_sonu);
    blok.remove_prefix(satir_sonu + 2);
    const std::size_t b1 = satir.find(' ');
    const std::size_t b2 = b1 == std::string_view::npos ? b1 : satir.find(' ', b1 + 1);
    if (b1 == std::string_view::npos || b2 == std::string_view::npos || satir.find(' ', b2 + 1) != std::string_view::npos) {
        neden = "istek satırı bozuk";
        return Sonuc::hata;
    }
    const std::string_view yontem = satir.substr(0, b1);
    const std::string_view hedef = satir.substr(b1 + 1, b2 - b1 - 1);
    const std::string_view surum = satir.substr(b2 + 1);
    if (yontem.empty() || yontem.size() > 16) {
        neden = "yöntem bozuk";
        return Sonuc::hata;
    }
    for (const char c : yontem) {
        if (!tchar(c)) {
            neden = "yöntem bozuk";
            return Sonuc::hata;
        }
    }
    if (hedef.empty() || hedef.front() != '/' || hedef.size() > 8192) {
        neden = "istek hedefi origin-form değil";
        return Sonuc::hata;
    }
    for (const char c : hedef) {
        const auto u = static_cast<unsigned char>(c);
        if (u <= 0x20u || u >= 0x7Fu || c == '#') {
            neden = "istek hedefinde geçersiz karakter";
            return Sonuc::hata;
        }
    }
    if (surum == "HTTP/1.1") {
        istek.kucuk_surum = 1;
    } else if (surum == "HTTP/1.0") {
        istek.kucuk_surum = 0;
    } else {
        neden = "HTTP sürümü desteklenmiyor";
        return Sonuc::hata;
    }
    istek.yontem.assign(yontem);
    istek.hedef.assign(hedef);
    const std::size_t soru = hedef.find('?');
    istek.yol.assign(hedef.substr(0, soru));
    if (soru != std::string_view::npos) {
        istek.sorgu.assign(hedef.substr(soru + 1));
    }
    // Başlık alanları
    while (!blok.empty()) {
        satir_sonu = blok.find("\r\n");
        satir = blok.substr(0, satir_sonu);
        blok.remove_prefix(satir_sonu + 2);
        if (istek.basliklar.size() >= kAzamiBaslikSayisi) {
            neden = "başlık sayısı sınırı aşıldı";
            return Sonuc::hata;
        }
        const std::size_t iki = satir.find(':');
        if (iki == std::string_view::npos || iki == 0) {
            neden = "başlık satırı bozuk";
            return Sonuc::hata;
        }
        const std::string_view ad = satir.substr(0, iki);
        for (const char c : ad) {
            if (!tchar(c)) {
                neden = "başlık adı bozuk";  // satır katlama ve ':' öncesi boşluk dahil (RFC 9112 §5)
                return Sonuc::hata;
            }
        }
        const std::string_view deger = kirp(satir.substr(iki + 1));
        for (const char c : deger) {
            const auto u = static_cast<unsigned char>(c);
            if ((u < 0x20u && c != '\t') || u == 0x7Fu) {
                neden = "başlık değerinde denetim karakteri";
                return Sonuc::hata;
            }
        }
        std::string k(ad);
        for (auto& c : k) {
            c = kucuk(c);
        }
        istek.basliklar.emplace_back(std::move(k), std::string(deger));
    }
    return Sonuc::tamam;
}

bool govde_bilgisi(const Istek& istek, GovdeBilgisi& g, std::string& neden)
{
    g = GovdeBilgisi{};
    const std::size_t te = istek.baslik_sayisi("transfer-encoding");
    const std::size_t cl = istek.baslik_sayisi("content-length");
    if (te > 0 && cl > 0) {
        neden = "Content-Length ile Transfer-Encoding birlikte";
        return false;
    }
    if (te > 1) {
        neden = "birden çok Transfer-Encoding";
        return false;
    }
    if (te == 1) {
        if (istek.kucuk_surum == 0 || !token_esit(kirp(*istek.baslik("transfer-encoding")), "chunked")) {
            neden = "desteklenmeyen Transfer-Encoding";
            return false;
        }
        g.tur = GovdeBilgisi::Tur::parcali;
        return true;
    }
    if (cl == 0) {
        return true;
    }
    // Birden çok Content-Length: hepsi aynı ve geçerliyse kabul (RFC 9112 §6.3 md.5); değilse hata.
    std::uint64_t deger = 0;
    bool ilk = true;
    for (const auto& [k, v] : istek.basliklar) {
        if (k != "content-length") {
            continue;
        }
        for (const auto& parca : liste(v)) {
            if (parca.empty() || parca.size() > 19) {
                neden = "Content-Length bozuk";
                return false;
            }
            std::uint64_t n = 0;
            for (const char c : parca) {
                if (c < '0' || c > '9') {
                    neden = "Content-Length bozuk";
                    return false;
                }
                n = n * 10u + static_cast<std::uint64_t>(c - '0');
            }
            if (!ilk && n != deger) {
                neden = "çelişkili Content-Length";
                return false;
            }
            deger = n;
            ilk = false;
        }
    }
    if (ilk) {
        neden = "Content-Length bozuk";
        return false;
    }
    g.tur = GovdeBilgisi::Tur::uzunluk;
    g.uzunluk = deger;
    return true;
}

std::size_t ParcaliCozucu::besle(std::string_view girdi, std::string& govde)
{
    std::size_t i = 0;
    while (i < girdi.size() && asama_ != Asama::bitti && asama_ != Asama::hata && !asildi_) {
        switch (asama_) {
        case Asama::boy:
        case Asama::trailer: {
            const char c = girdi[i++];
            if (c != '\n') {
                if (satir_.size() > 64) {  // en kısa biçimdeki boy satırı (15 onaltılık + CR) bunun çok altında
                    asama_ = Asama::hata;
                    break;
                }
                satir_.push_back(c);
                break;
            }
            if (satir_.empty() || satir_.back() != '\r') {
                asama_ = Asama::hata;
                break;
            }
            satir_.pop_back();
            if (asama_ == Asama::trailer) {
                // Sol #8, en kısıtlayan okuma: trailer alanı kabul edilmez (panel göndermez); son
                // parçadan sonra yalnız boş satır gelir, aksi bozuk çerçeveleme (400 + kapanış).
                asama_ = satir_.empty() ? Asama::bitti : Asama::hata;
                satir_.clear();
                break;
            }
            // chunk-size: yalnız onaltılık rakamlar. Sol #8, en kısıtlayan okuma: chunk uzantısı
            // (';') ve boşluk kabul edilmez.
            const std::string_view s = satir_;
            if (s.empty() || s.size() > 15) {
                asama_ = Asama::hata;
                break;
            }
            std::uint64_t n = 0;
            for (const char h : s) {
                std::uint64_t v = 0;
                if (h >= '0' && h <= '9') {
                    v = static_cast<std::uint64_t>(h - '0');
                } else if (h >= 'a' && h <= 'f') {
                    v = static_cast<std::uint64_t>(h - 'a' + 10);
                } else if (h >= 'A' && h <= 'F') {
                    v = static_cast<std::uint64_t>(h - 'A' + 10);
                } else {
                    asama_ = Asama::hata;
                    break;
                }
                n = (n << 4u) | v;
            }
            satir_.clear();
            if (asama_ == Asama::hata) {
                break;
            }
            kalan_ = n;
            asama_ = n == 0 ? Asama::trailer : Asama::veri;
            break;
        }
        case Asama::veri: {
            const std::size_t mevcut = girdi.size() - i;
            const std::size_t al = kalan_ < mevcut ? static_cast<std::size_t>(kalan_) : mevcut;
            if (govde.size() + al > sinir_) {
                asildi_ = true;
                break;
            }
            govde.append(girdi.substr(i, al));
            i += al;
            kalan_ -= al;
            if (kalan_ == 0) {
                asama_ = Asama::veri_crlf;
            }
            break;
        }
        case Asama::veri_crlf: {
            const char c = girdi[i++];
            satir_.push_back(c);
            if (satir_.size() == 2) {
                if (satir_ != "\r\n") {
                    asama_ = Asama::hata;
                } else {
                    asama_ = Asama::boy;
                }
                satir_.clear();
            }
            break;
        }
        case Asama::bitti:
        case Asama::hata:
            break;
        }
    }
    return i;
}

std::string durum_metni(int durum)
{
    switch (durum) {
    case 101: return "Switching Protocols";
    case 200: return "OK";
    case 202: return "Accepted";
    case 400: return "Bad Request";
    case 403: return "Forbidden";
    case 404: return "Not Found";
    case 405: return "Method Not Allowed";
    case 409: return "Conflict";
    case 410: return "Gone";
    case 413: return "Content Too Large";
    case 415: return "Unsupported Media Type";
    case 422: return "Unprocessable Content";
    case 426: return "Upgrade Required";
    case 500: return "Internal Server Error";
    case 503: return "Service Unavailable";
    default: return "Status";
    }
}

std::string yanit(int durum, std::string_view icerik_turu, std::string_view govde, bool kapat,
                  const std::vector<std::pair<std::string, std::string>>& ek, bool govdesiz)
{
    std::string s;
    s.reserve(256 + govde.size());
    s += "HTTP/1.1 ";
    s += std::to_string(durum);
    s += ' ';
    s += durum_metni(durum);
    s += "\r\n";
    if (!icerik_turu.empty()) {
        s += "Content-Type: ";
        s += icerik_turu;
        s += "\r\n";
    }
    s += "Content-Length: ";
    s += std::to_string(govde.size());
    s += "\r\nCache-Control: no-store\r\nX-Content-Type-Options: nosniff\r\n";
    for (const auto& [k, v] : ek) {
        s += k;
        s += ": ";
        s += v;
        s += "\r\n";
    }
    if (kapat) {
        s += "Connection: close\r\n";
    }
    s += "\r\n";
    if (!govdesiz) {
        s += govde;
    }
    return s;
}

std::vector<std::string> liste(std::string_view deger)
{
    std::vector<std::string> r;
    std::size_t bas = 0;
    for (;;) {
        const std::size_t v = deger.find(',', bas);
        const std::string_view parca = kirp(deger.substr(bas, v == std::string_view::npos ? std::string_view::npos : v - bas));
        if (!parca.empty()) {
            r.emplace_back(parca);
        }
        if (v == std::string_view::npos) {
            break;
        }
        bas = v + 1;
    }
    return r;
}

bool token_esit(std::string_view a, std::string_view b) noexcept
{
    if (a.size() != b.size()) {
        return false;
    }
    for (std::size_t i = 0; i < a.size(); ++i) {
        if (kucuk(a[i]) != kucuk(b[i])) {
            return false;
        }
    }
    return true;
}

}  // namespace ida::a3::http

namespace ida::a3::ws {

Okuma oku(const std::uint8_t* p, std::size_t n, std::size_t& tuketilen, Cerceve& c, std::uint16_t& kapanis_kodu,
          std::size_t azami_yuk)
{
    tuketilen = 0;
    if (n < 2) {
        return Okuma::eksik;
    }
    const bool fin = (p[0] & 0x80u) != 0;
    const std::uint8_t rsv = static_cast<std::uint8_t>(p[0] & 0x70u);
    const std::uint8_t op = static_cast<std::uint8_t>(p[0] & 0x0Fu);
    const bool maske = (p[1] & 0x80u) != 0;
    std::uint64_t uz = p[1] & 0x7Fu;
    std::size_t k = 2;
    if (rsv != 0) {
        kapanis_kodu = 1002;  // uzantı anlaşılmadı
        return Okuma::hata;
    }
    const bool denetim = (op & 0x08u) != 0;
    if (!(op == 0x0 || op == 0x1 || op == 0x2 || op == 0x8 || op == 0x9 || op == 0xA)) {
        kapanis_kodu = 1002;
        return Okuma::hata;
    }
    if (!maske) {
        kapanis_kodu = 1002;  // RFC 6455 §5.1: istemci çerçevesi maskeli olmalı
        return Okuma::hata;
    }
    if (denetim && (!fin || uz > 125)) {
        kapanis_kodu = 1002;
        return Okuma::hata;
    }
    if (uz == 126) {
        if (n < 4) {
            return Okuma::eksik;
        }
        uz = (static_cast<std::uint64_t>(p[2]) << 8u) | p[3];
        k = 4;
        if (uz < 126) {
            kapanis_kodu = 1002;  // RFC 6455 §5.2: en kısa gösterim zorunlu
            return Okuma::hata;
        }
    } else if (uz == 127) {
        if (n < 10) {
            return Okuma::eksik;
        }
        uz = 0;
        for (std::size_t i = 0; i < 8; ++i) {
            uz = (uz << 8u) | p[2 + i];
        }
        if ((uz >> 63u) != 0 || uz < 65536) {  // MSB 0 ve en kısa gösterim (RFC 6455 §5.2)
            kapanis_kodu = 1002;
            return Okuma::hata;
        }
        k = 10;
    }
    if (!denetim && uz > azami_yuk) {
        kapanis_kodu = 1009;  // A3-K47: büyük ileti
        return Okuma::hata;
    }
    if (n - k < 4) {
        return Okuma::eksik;
    }
    const std::uint8_t m[4] = {p[k], p[k + 1], p[k + 2], p[k + 3]};
    k += 4;
    if (n - k < uz) {
        return Okuma::eksik;
    }
    const auto boy = static_cast<std::size_t>(uz);
    c.fin = fin;
    c.opkod = op;
    c.yuk.resize(boy);
    for (std::size_t i = 0; i < boy; ++i) {
        c.yuk[i] = static_cast<char>(p[k + i] ^ m[i & 3u]);
    }
    tuketilen = k + boy;
    return Okuma::cerceve;
}

void yaz(Opkod op, const std::uint8_t* yuk, std::size_t n, std::string& cikti)
{
    cikti.push_back(static_cast<char>(0x80u | static_cast<unsigned>(op)));
    if (n < 126) {
        cikti.push_back(static_cast<char>(n));
    } else if (n <= 0xFFFFu) {
        cikti.push_back(static_cast<char>(126));
        cikti.push_back(static_cast<char>((n >> 8u) & 0xFFu));
        cikti.push_back(static_cast<char>(n & 0xFFu));
    } else {
        cikti.push_back(static_cast<char>(127));
        const auto u = static_cast<std::uint64_t>(n);
        for (int i = 7; i >= 0; --i) {
            cikti.push_back(static_cast<char>((u >> (8u * static_cast<unsigned>(i))) & 0xFFu));
        }
    }
    if (n > 0) {
        cikti.append(reinterpret_cast<const char*>(yuk), n);
    }
}

void kapanis_yaz(std::uint16_t kod, std::string_view neden, std::string& cikti)
{
    std::string y;
    y.push_back(static_cast<char>((kod >> 8u) & 0xFFu));
    y.push_back(static_cast<char>(kod & 0xFFu));
    y.append(neden.substr(0, 120));
    yaz(Opkod::kapat, reinterpret_cast<const std::uint8_t*>(y.data()), y.size(), cikti);
}

}  // namespace ida::a3::ws
