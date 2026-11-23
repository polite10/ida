// sim/sensor testlerinin ortak düzeni (sim/fizik/test ile aynı biçim).
//
// Her test `int test_<ad>()`; 0 geçti. Toleranslar testin başında adlandırılmış sabit olarak ve
// gerekçesiyle yazılıdır. Yer gerçeği fizik koşturulmadan analitik olarak kurulur (durgun, sabit
// dönüş, sabit ivme, dalış): `DurumAnlik` alanları tutarlı doldurulur.
#pragma once

#include "ida/sensor/sensor_kumesi.hpp"
#include "matematik.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <functional>
#include <string>
#include <vector>

namespace ida::sensor::test {

struct Denetci {
    int hata = 0;
    void dogrula(bool kosul, const char* dosya, int satir, const std::string& ileti)
    {
        if (!kosul) {
            ++hata;
            if (hata <= 40) {
                std::printf("  KALDI %s:%d: %s\n", dosya, satir, ileti.c_str());
            }
        }
    }
    void dogrula_v(const char* dosya, int satir, bool kosul, const std::string& ileti)
    {
        dogrula(kosul, dosya, satir, ileti);
    }
    void yakin(double gercek, double beklenen, double mutlak, const char* dosya, int satir,
               const char* ad)
    {
        const double fark = std::abs(gercek - beklenen);
        if (!(std::isfinite(gercek) && fark <= mutlak)) {
            ++hata;
            if (hata <= 40) {
                std::printf("  KALDI %s:%d: %s: gerçek %.12g, beklenen %.12g, fark %.3g (tol %.3g)\n",
                            dosya, satir, ad, gercek, beklenen, fark, mutlak);
            }
        }
    }
};

// Değişken argümanlı: koşuldaki küme parantezli virgüller makroyu bölmez.
#define DOGRULA(d, ...) (d).dogrula_v(__FILE__, __LINE__, __VA_ARGS__)
#define YAKIN(d, gercek, beklenen, mutlak, ad)                                          \
    (d).yakin(static_cast<double>(gercek), static_cast<double>(beklenen),               \
              static_cast<double>(mutlak), __FILE__, __LINE__, (ad))

inline constexpr std::int64_t kAdim = 1'250'000;
inline constexpr double kG = 9.82;

inline Kuaterniyon euler_q(double roll, double pitch, double yaw)
{
    const double cr = std::cos(roll / 2), sr = std::sin(roll / 2);
    const double cp = std::cos(pitch / 2), sp = std::sin(pitch / 2);
    const double cy = std::cos(yaw / 2), sy = std::sin(yaw / 2);
    return {cr * cp * cy + sr * sp * sy, sr * cp * cy - cr * sp * sy, cr * sp * cy + sr * cp * sy,
            cr * cp * sy - sr * sp * cy};
}

// Tutarlı anlık görüntü: NED konum, hız ve ivme; yönelim; gövde açısal hızı ve ivmesi.
inline fizik::DurumAnlik durum(std::uint64_t adim, const Vek3& p, const Vek3& v_n,
                               const Vek3& a_n, const Kuaterniyon& q, const Vek3& w_b,
                               const Vek3& alfa_b = {})
{
    fizik::DurumAnlik d;
    d.adim_no = adim;
    d.t_ns = static_cast<std::int64_t>(adim) * kAdim;
    d.konum_n = p;
    d.yonelim = q;
    const Mat3 r = ic::matris(q);
    d.dogrusal_hiz_b = ic::carp_t(r, v_n);
    d.acisal_hiz_b = w_b;
    d.acisal_ivme_b = alfa_b;
    const Vek3 a_b = ic::carp_t(r, a_n);
    d.dogrusal_ivme_b = ic::cikar(a_b, ic::capraz(w_b, d.dogrusal_hiz_b));
    d.ozgul_kuvvet_b = ic::carp_t(r, ic::cikar(a_n, {0.0, 0.0, kG}));
    return d;
}

using Yorunge = std::function<fizik::DurumAnlik(std::uint64_t adim)>;

inline Yorunge durgun(const Vek3& p, const Kuaterniyon& q)
{
    return [p, q](std::uint64_t n) { return durum(n, p, {}, {}, q, {}); };
}

inline double sn(std::uint64_t n) { return static_cast<double>(n) * 1.25e-3; }

// Sensör kümesini adım adım koşturur; bütün kayıtları toplar.
struct Kosu {
    SensorKumesi kume;
    std::vector<Olcum> kayit;
    std::function<void(std::uint64_t, SensorKumesi&)> adim_oncesi;

    bool kur(const SensorKumesiAyari& a)
    {
        const KurulumSonucu s = kume.kur(a);
        if (!s.tamam) {
            std::printf("  kurulum başarısız: %s\n", s.hata.c_str());
        }
        return s.tamam;
    }
    bool kos(const Yorunge& y, std::uint64_t bas, std::uint64_t son)
    {
        for (std::uint64_t n = bas; n < son; ++n) {
            if (adim_oncesi) {
                adim_oncesi(n, kume);
            }
            if (kume.adim(y(n), n) != AdimSonucu::tamam) {
                std::printf("  adım %llu başarısız\n", static_cast<unsigned long long>(n));
                return false;
            }
            kume.bosalt(std::numeric_limits<std::int64_t>::max(), kayit);
        }
        return true;
    }
    std::vector<Olcum> tur(SensorTuru t) const
    {
        std::vector<Olcum> v;
        for (const auto& o : kayit) {
            if (o.hdr.tur == t) v.push_back(o);
        }
        // Sensör içinde seq sırası (kuyruk t_teslim sırasıyla verir).
        std::sort(v.begin(), v.end(),
                  [](const Olcum& a, const Olcum& b) { return a.hdr.seq < b.hdr.seq; });
        return v;
    }
};

template <class T>
const T& yuk(const Olcum& o)
{
    return std::get<T>(o.yuk);
}

// Bütün hata kaynakları kapalı İDA-0 kümesi (ideal eşitlik testleri).
SensorKumesiAyari ideal_ayar();

// Belirli bir türün ayarını değiştirmek için.
template <class T>
T& ayar_govdesi(SensorKumesiAyari& k, SensorTuru t)
{
    for (auto& s : k.sensorler) {
        if (s.tur == t) return std::get<T>(s.ayar);
    }
    std::printf("  ayar bulunamadı\n");
    std::abort();
}

inline SensorAyari& sensor_ayari(SensorKumesiAyari& k, SensorTuru t)
{
    for (auto& s : k.sensorler) {
        if (s.tur == t) return s;
    }
    std::abort();
}

// Yalnız verilen türler etkin.
inline void yalniz(SensorKumesiAyari& k, std::initializer_list<SensorTuru> turler)
{
    for (auto& s : k.sensorler) {
        bool var = false;
        for (SensorTuru t : turler) var = var || s.tur == t;
        s.etkin = var;
    }
}

}  // namespace ida::sensor::test
