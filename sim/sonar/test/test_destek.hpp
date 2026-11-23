// sim/sonar testlerinin ortak düzeni (sim/sensor/test ile aynı biçim).
//
// Her test `int test_<ad>()`; 0 geçti. Toleranslar testin başında adlandırılmış sabit olarak ve
// gerekçesiyle yazılıdır. Yer gerçeği fizik koşturulmadan analitik olarak kurulur.
#pragma once

#include "ida/sonar/sonar_kumesi.hpp"
#include "vek.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <limits>
#include <memory>
#include <string>
#include <vector>

namespace ida::sonar::test {

struct Denetci {
    int hata = 0;
    void dogrula_v(const char* dosya, int satir, bool kosul, const std::string& ileti)
    {
        if (!kosul) {
            ++hata;
            if (hata <= 40) std::printf("  KALDI %s:%d: %s\n", dosya, satir, ileti.c_str());
        }
    }
    void yakin(double gercek, double beklenen, double mutlak, const char* dosya, int satir, const char* ad)
    {
        const double fark = std::abs(gercek - beklenen);
        if (!(std::isfinite(gercek) && fark <= mutlak)) {
            ++hata;
            if (hata <= 40) {
                std::printf("  KALDI %s:%d: %s: gerçek %.12g, beklenen %.12g, fark %.3g (tol %.3g)\n", dosya,
                            satir, ad, gercek, beklenen, fark, mutlak);
            }
        }
    }
};

#define DOGRULA(d, ...) (d).dogrula_v(__FILE__, __LINE__, __VA_ARGS__)
#define YAKIN(d, gercek, beklenen, mutlak, ad)                                                      \
    (d).yakin(static_cast<double>(gercek), static_cast<double>(beklenen), static_cast<double>(mutlak), \
              __FILE__, __LINE__, (ad))

// Elle koşuda ek tanı çıktısı (`ida_sonar_test <test> tani`); CTest kullanmaz.
inline bool& tani_kipi()
{
    static bool b = false;
    return b;
}

inline constexpr std::int64_t kAdim = 1'250'000;
inline constexpr double kDerece = ic::kPi / 180.0;

inline Kuaterniyon euler_q(double roll, double pitch, double yaw)
{
    const double cr = std::cos(roll / 2), sr = std::sin(roll / 2);
    const double cp = std::cos(pitch / 2), sp = std::sin(pitch / 2);
    const double cy = std::cos(yaw / 2), sy = std::sin(yaw / 2);
    return {cr * cp * cy + sr * sp * sy, sr * cp * cy - cr * sp * sy, cr * sp * cy + sr * cp * sy,
            cr * cp * sy - sr * sp * cy};
}

inline fizik::DurumAnlik durum(std::uint64_t adim, const Vek3& p, const Kuaterniyon& q, const Vek3& v_n = {})
{
    fizik::DurumAnlik d;
    d.adim_no = adim;
    d.t_ns = static_cast<std::int64_t>(adim) * kAdim;
    d.konum_n = p;
    d.yonelim = q;
    d.dogrusal_hiz_b = ic::carp_t(ic::matris(q), v_n);
    return d;
}

using Yorunge = std::function<fizik::DurumAnlik(std::uint64_t adim)>;

inline Yorunge durgun(const Vek3& p, const Kuaterniyon& q = {1.0, 0.0, 0.0, 0.0})
{
    return [p, q](std::uint64_t n) { return durum(n, p, q); };
}

// Ping360 yapılandırması: `n` örnek, `sp` örnek periyodu; tam tur, 1 gradyan adım.
inline Ping360Yapilandirmasi p360_yap(std::uint16_t sp, std::uint16_t n = 1200, std::uint16_t bas = 0,
                                      std::uint16_t bit = 0, std::uint16_t adim = 1)
{
    Ping360Yapilandirmasi y;
    y.n_samples = n;
    y.sample_period = sp;
    y.baslangic_grad = bas;
    y.bitis_grad = bit;
    y.adim_grad = adim;
    return y;
}

inline SonarAyari ping360(const Ping360Yapilandirmasi& y, const std::string& ad = "p360", std::uint16_t kimlik = 7)
{
    SonarAyari a;
    a.ad = ad;
    a.tur = SensorTuru::ping360;
    a.kimlik = kimlik;
    a.profil = "ping360";
    a.ayar = Ping360Ayar{};
    a.yapilandirma = y;
    return a;
}

inline SonarAyari ping1d(const Ping1dYapilandirmasi& y, const std::string& ad = "p1d", std::uint16_t kimlik = 6)
{
    SonarAyari a;
    a.ad = ad;
    a.tur = SensorTuru::ping1d;
    a.kimlik = kimlik;
    a.profil = "ping1d";
    a.ayar = Ping1dAyar{};
    a.yapilandirma = y;
    return a;
}

inline SonarKumesiAyari kume_ayari(std::vector<SonarAyari> s, std::uint64_t tohum = 1)
{
    SonarKumesiAyari k;
    k.sonarlar = std::move(s);
    k.ortam.su.sicaklik_c = 20.0;
    k.ana_tohum = tohum;
    k.tani = true;
    return k;
}

inline const Ping360Olcumu& p360(const SonarOlcumu& o) { return std::get<Ping360Olcumu>(o.yuk); }
inline const Ping1dOlcumu& p1d(const SonarOlcumu& o) { return std::get<Ping1dOlcumu>(o.yuk); }

// İDA-0 örnek havuzu: 25 × 12,5 × 3 m (scenarios/ornek-havuz.toml; A7 havuz_ornek).
inline std::shared_ptr<CozumselSahne> havuz_sahnesi(double derinlik = 3.0)
{
    auto s = std::make_shared<CozumselSahne>();
    HavuzTanimi h;
    h.derinlik = derinlik;
    havuz_ekle(*s, h);
    return s;
}

struct Kosu {
    SonarKumesi kume;
    std::vector<SonarOlcumu> kayit;
    std::function<void(std::uint64_t, SonarKumesi&)> adim_oncesi;

    bool kur(const SonarKumesiAyari& a, std::shared_ptr<const Sahne> s)
    {
        const KurulumSonucu r = kume.kur(a, std::move(s));
        if (!r.tamam) std::printf("  kurulum başarısız: %s\n", r.hata.c_str());
        return r.tamam;
    }
    bool kos(const Yorunge& y, std::uint64_t bas, std::uint64_t son)
    {
        for (std::uint64_t n = bas; n < son; ++n) {
            if (adim_oncesi) adim_oncesi(n, kume);
            if (kume.adim(y(n), n) != AdimSonucu::tamam) {
                std::printf("  adım %llu başarısız\n", static_cast<unsigned long long>(n));
                return false;
            }
            kume.bosalt(std::numeric_limits<std::int64_t>::max(), kayit);
        }
        return true;
    }
    std::vector<SonarOlcumu> sonar(std::uint16_t kimlik) const
    {
        std::vector<SonarOlcumu> v;
        for (const auto& o : kayit) {
            if (o.hdr.sensor_id == kimlik) v.push_back(o);
        }
        std::sort(v.begin(), v.end(), [](const SonarOlcumu& a, const SonarOlcumu& b) { return a.hdr.seq < b.hdr.seq; });
        return v;
    }
};

// Kutu i'nin nominal eğik menzili (A1-K52).
inline double kutu_menzili(double c, std::uint16_t sp, std::size_t i)
{
    return c * (2.0 * static_cast<double>(i) + 1.0) * static_cast<double>(sp) * 25e-9 / 4.0;
}

inline std::size_t en_buyuk(const std::vector<double>& v, std::size_t bas = 0)
{
    std::size_t k = bas;
    for (std::size_t i = bas; i < v.size(); ++i) {
        if (v[i] > v[k]) k = i;
    }
    return k;
}

inline double toplam(const std::vector<double>& v)
{
    double s = 0.0;
    for (double x : v) s += x;
    return s;
}

// MP7 "orta sahne"nin çözümsel karşılığı (düz taban; MP7'de engebeli) (MP7 §3: sonar tabandan ≈ 2 m yukarıda, duvar 6 m'de,
// yakın ve uzak cisimler). Sonar (0, 0, 6) NED; taban z = 8, su yüzeyi z = 0, duvar x = 6.
inline std::shared_ptr<CozumselSahne> orta_sahne()
{
    auto s = std::make_shared<CozumselSahne>();
    // Malzemeler MP7 sahnesinin yer tutucu değerleri (MP7 kaynak/sahne.cpp; hepsi μ cos²θ):
    // taban 10^-2,7, su yüzeyi 10^-4, duvar 10^-2, cisim 10^-1,5, hedef 10^-1.
    acik_su_ekle(*s, 8.0, {"taban", std::pow(10.0, -2.7), GelisYasasi::lambert_cos2, 0.3, 0.1},
                 {"su_yuzeyi", 1e-4, GelisYasasi::lambert_cos2, -1.0, 0.9});
    const std::uint16_t duvar = s->malzeme_ekle({"duvar", 1e-2, GelisYasasi::lambert_cos2, 0.5, 0.3});
    const std::uint16_t metal = s->malzeme_ekle({"cisim", std::pow(10.0, -1.5), GelisYasasi::lambert_cos2, 0.7, 0.5});
    const std::uint16_t plastik = metal;
    const std::uint16_t hedef = s->malzeme_ekle({"hedef", 0.1, GelisYasasi::lambert_cos2, 0.8, 0.5});
    Govde g;
    g.kimlik = 3;
    g.sekil.tur = SekilTuru::duzlem;
    g.konum = {6.0, 0.0, 0.0};
    g.yonelim = {std::cos(ic::kPi / 4), 0.0, std::sin(ic::kPi / 4), 0.0};
    g.malzemeler = {duvar};
    s->govde_ekle(g);
    auto kure = [&](std::uint32_t id, Vek3 p, double r, std::uint16_t m) {
        Govde k;
        k.kimlik = id;
        k.sekil.tur = SekilTuru::kure;
        k.sekil.yaricap = r;
        k.konum = p;
        k.malzemeler = {m};
        s->govde_ekle(k);
    };
    kure(10, {3.0, 1.0, 7.5}, 0.3, metal);
    kure(11, {-4.5, -2.0, 7.0}, 0.5, plastik);
    kure(12, {-2.0, 4.0, 6.0}, 0.25, metal);
    Govde b;
    b.kimlik = 13;
    b.sekil.tur = SekilTuru::kutu;
    b.sekil.yari_boyut = {0.4, 0.3, 0.4};
    b.konum = {2.0, -3.0, 7.6};
    b.yonelim = euler_q(0.0, 0.0, 0.5);
    b.malzemeler = {metal};
    s->govde_ekle(b);
    Govde c;
    c.kimlik = 14;
    c.sekil.tur = SekilTuru::silindir;
    c.sekil.yaricap = 0.3;
    c.sekil.yukseklik = 1.2;
    c.konum = {-3.0, -4.5, 7.4};
    c.malzemeler = {plastik};
    s->govde_ekle(c);
    Govde h;   // hareketli hedef küre (MP7: r = 0,6 m, sonarın 0,5 m altında, 1 m/s)
    h.kimlik = 15;
    h.sekil.tur = SekilTuru::kure;
    h.sekil.yaricap = 0.6;
    h.konum = {0.0, 4.0, 6.5};
    h.hiz_ned = {1.0, 0.0, 0.0};
    h.malzemeler = {hedef};
    s->govde_ekle(h);
    return s;
}

}  // namespace ida::sonar::test
