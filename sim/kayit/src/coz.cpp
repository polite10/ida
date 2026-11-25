// coz.cpp — tablo güdümlü yük çözücü.
#include "ida/kayit/coz.hpp"

#include "ida/a3/kodlayici.hpp"
#include "ida/kayit/katalog.hpp"

#include <cmath>
#include <limits>

namespace ida::kayit {

namespace {

using a3::AlanTanimi;
using a3::Okuyucu;
using a3::Tip;

bool skaler_mi(Tip t) noexcept
{
    return t != Tip::metin && t != Tip::bayt_dizisi && t != Tip::dizi;
}

struct Skaler {
    Tip tip = Tip::u8;
    std::uint64_t u = 0;  // işaretsiz tipler
    std::int64_t i = 0;   // işaretli tipler
    float f32 = 0;
    double f64 = 0;
};

Skaler skaler_oku(Okuyucu& r, Tip t) noexcept
{
    Skaler s;
    s.tip = t;
    switch (t) {
    case Tip::u8:
        s.u = r.u8();
        break;
    case Tip::u16:
        s.u = r.u16();
        break;
    case Tip::u32:
        s.u = r.u32();
        break;
    case Tip::u64:
        s.u = r.u64();
        break;
    case Tip::i32:
        s.i = r.i32();
        break;
    case Tip::i64:
        s.i = r.i64();
        break;
    case Tip::f32:
        s.f32 = r.f32();
        break;
    case Tip::f64:
        s.f64 = r.f64();
        break;
    case Tip::metin:
    case Tip::bayt_dizisi:
    case Tip::dizi:
        break;
    }
    return s;
}

// Katalogdaki `yok_degeri` (AlanTanimi::yok kodu) bu değere eşit mi.
bool yok_mu(const Skaler& s, std::uint8_t kod) noexcept
{
    const bool isaretli = s.tip == Tip::i32 || s.tip == Tip::i64;
    switch (kod) {
    case 1:  // NaN
        return s.tip == Tip::f32 ? std::isnan(s.f32) : (s.tip == Tip::f64 && std::isnan(s.f64));
    case 2:  // INT64_MIN
        return s.tip == Tip::i64 && s.i == std::numeric_limits<std::int64_t>::min();
    case 3:  // 2^64-1
        return s.tip == Tip::u64 && s.u == std::numeric_limits<std::uint64_t>::max();
    case 4:  // 0xFFFFFFFF
        return s.tip == Tip::u32 && s.u == 0xFFFFFFFFull;
    case 5:  // 255
        return isaretli ? s.i == 255 : s.u == 255u;
    case 6:  // -1
        return isaretli && s.i == -1;
    case 7:  // 0
        return isaretli ? s.i == 0 : s.u == 0u;
    case kYokKodu0xFFFF:
        return s.tip == Tip::u16 && s.u == 0xFFFFull;
    default:
        return false;
    }
}

void skaler_dok(std::string& c, const Skaler& s, std::uint8_t yok)
{
    if (s.tip == Tip::f32) {
        if (yok == 1) {
            a3::dok_kayan_yok(c, s.f32);
        } else {
            a3::dok_kayan(c, s.f32);
        }
        return;
    }
    if (s.tip == Tip::f64) {
        if (yok == 1) {
            a3::dok_kayan_yok(c, s.f64);
        } else {
            a3::dok_kayan(c, s.f64);
        }
        return;
    }
    if (yok != 0 && yok_mu(s, yok)) {
        c += "null";
        return;
    }
    switch (s.tip) {
    case Tip::u64:
        a3::dok_64(c, s.u);
        break;
    case Tip::i64:
        a3::dok_64(c, s.i);
        break;
    case Tip::i32:
        a3::dok_tam(c, s.i);
        break;
    default:
        a3::dok_tam(c, s.u);
        break;
    }
}

std::uint64_t sayac_oku(Okuyucu& r, Tip sayac) noexcept
{
    switch (sayac) {
    case Tip::u8:
        return r.u8();
    case Tip::u16:
        return r.u16();
    case Tip::u32:
        return r.u32();
    default:
        return std::numeric_limits<std::uint64_t>::max();  // tablo hatası: sınır denetimi reddeder
    }
}

struct Gezgin {
    Okuyucu r;
    std::string* cikti;            // nullptr: döküm yok
    const AlanZiyaretcisi* ziyaretci;  // nullptr: ziyaret yok
    bool bozuk = false;

    // Bir alanı okur. `ust_duzey` ise ziyaretçiye bildirilir.
    void alan(const AlanTanimi& a, bool ust_duzey)
    {
        if (skaler_mi(a.tip)) {
            if (a.adet > 0) {
                if (cikti != nullptr) {
                    cikti->push_back('[');
                }
                for (std::uint16_t i = 0; i < a.adet; ++i) {
                    const Skaler s = skaler_oku(r, a.tip);
                    if (cikti != nullptr) {
                        if (i > 0) {
                            cikti->push_back(',');
                        }
                        skaler_dok(*cikti, s, a.yok);
                    }
                }
                if (cikti != nullptr) {
                    cikti->push_back(']');
                }
                return;
            }
            const Skaler s = skaler_oku(r, a.tip);
            if (!r.tamam()) {
                return;
            }
            if (cikti != nullptr) {
                skaler_dok(*cikti, s, a.yok);
            }
            if (ust_duzey && ziyaretci != nullptr) {
                AlanDegeri d;
                if (a.tip == Tip::f32 || a.tip == Tip::f64) {
                    d.tur = AlanDegeri::Tur::kayan;
                    d.kayan = a.tip == Tip::f32 ? static_cast<double>(s.f32) : s.f64;
                } else if (a.tip == Tip::i32 || a.tip == Tip::i64) {
                    d.tur = AlanDegeri::Tur::isaretli;
                    d.isaretli = s.i;
                } else {
                    d.tur = AlanDegeri::Tur::isaretsiz;
                    d.isaretsiz = s.u;
                }
                (*ziyaretci)(a, d);
            }
            return;
        }
        const std::uint64_t n = sayac_oku(r, a.sayac);
        if (!r.tamam() || n > a.azami) {
            bozuk = true;  // yapısal hata: sayaç katalogdaki üst sınırı aşıyor ya da okunamadı
            return;
        }
        if (a.tip == Tip::dizi) {
            if (cikti != nullptr) {
                cikti->push_back('[');
            }
            for (std::uint64_t i = 0; i < n && !bozuk && r.tamam(); ++i) {
                if (cikti != nullptr) {
                    if (i > 0) {
                        cikti->push_back(',');
                    }
                    cikti->push_back('{');
                }
                for (std::uint16_t k = 0; k < a.oge_sayisi && !bozuk && r.tamam(); ++k) {
                    if (cikti != nullptr) {
                        if (k > 0) {
                            cikti->push_back(',');
                        }
                        ad_yaz(a.oge[k]);
                    }
                    alan(a.oge[k], false);
                }
                if (cikti != nullptr) {
                    cikti->push_back('}');
                }
            }
            if (cikti != nullptr) {
                cikti->push_back(']');
            }
            return;
        }
        const std::uint8_t* p = r.baytlar(n);
        if (p == nullptr && n > 0) {
            return;  // r.tamam() false
        }
        const auto boy = static_cast<std::size_t>(n);
        if (a.tip == Tip::metin) {
            const std::string_view m(reinterpret_cast<const char*>(p), p == nullptr ? 0u : boy);
            if (cikti != nullptr) {
                a3::dok_metin(*cikti, m);
            }
            if (ust_duzey && ziyaretci != nullptr) {
                AlanDegeri d;
                d.tur = AlanDegeri::Tur::metin;
                d.metin = m;
                (*ziyaretci)(a, d);
            }
            return;
        }
        if (cikti != nullptr) {
            cikti->push_back('[');
            for (std::size_t i = 0; i < boy; ++i) {
                if (i > 0) {
                    cikti->push_back(',');
                }
                a3::dok_tam(*cikti, p[i]);
            }
            cikti->push_back(']');
        }
        if (ust_duzey && ziyaretci != nullptr) {
            AlanDegeri d;
            d.tur = AlanDegeri::Tur::baytlar;
            d.baytlar = std::span<const std::uint8_t>(p, p == nullptr ? 0u : boy);
            (*ziyaretci)(a, d);
        }
    }

    void ad_yaz(const AlanTanimi& a)
    {
        cikti->push_back('"');
        cikti->append(a.ad);
        cikti->append("\":");
    }
};

CozmeSonucu gez(const a3::YukTanimi& yuk, std::span<const std::uint8_t> veri, std::string* cikti,
                const AlanZiyaretcisi* z)
{
    CozmeSonucu s;
    Gezgin g{Okuyucu(veri.data(), veri.size()), cikti, z};
    if (cikti != nullptr) {
        cikti->clear();
        cikti->push_back('{');
    }
    for (std::uint16_t i = 0; i < yuk.alan_sayisi; ++i) {
        if (cikti != nullptr) {
            if (i > 0) {
                cikti->push_back(',');
            }
            g.ad_yaz(yuk.alanlar[i]);
        }
        if (g.r.kalan() == 0) {
            // A6-K48: küçük `kanal_surumu`: sondaki alanlar yok.
            ++s.eksik_alan;
            if (cikti != nullptr) {
                cikti->append("null");
            }
            continue;
        }
        g.alan(yuk.alanlar[i], true);
        if (g.bozuk || !g.r.tamam()) {
            return s;  // tamam = false
        }
    }
    if (cikti != nullptr) {
        cikti->push_back('}');
    }
    s.fazla_bayt = g.r.kalan();  // A6-K48: büyük `kanal_surumu`: sondaki fazla baytlar yok sayılır
    s.tamam = true;
    return s;
}

}  // namespace

CozmeSonucu json_dok(const a3::YukTanimi& yuk, std::span<const std::uint8_t> veri, std::string& cikti)
{
    return gez(yuk, veri, &cikti, nullptr);
}

CozmeSonucu alanlari_gez(const a3::YukTanimi& yuk, std::span<const std::uint8_t> veri, const AlanZiyaretcisi& z)
{
    return gez(yuk, veri, nullptr, &z);
}

}  // namespace ida::kayit
