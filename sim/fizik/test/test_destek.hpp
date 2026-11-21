// sim/fizik testlerinin ortak düzeni: küçük denetim makroları ve yardımcılar.
//
// Her test `int test_<ad>(const TestBaglami&)` işlevidir; 0 geçti, 0 dışı kaldı demektir.
// Toleranslar her testin başında adlandırılmış sabit olarak ve gerekçesiyle yazılıdır.
#pragma once

#include "ida/fizik/dunya.hpp"

#include <array>
#include <cmath>
#include <cstdio>
#include <string>
#include <utility>
#include <vector>

namespace ida::fizik::test {

struct TestBaglami {
    std::string ida0_toml_yolu;
};

// Denetim: koşul sağlanmazsa iletiyi yazar ve hata sayacını artırır (test sürer).
struct Denetci {
    int hata = 0;
    void dogrula(bool kosul, const char* dosya, int satir, const std::string& ileti)
    {
        if (!kosul) {
            ++hata;
            std::printf("  KALDI %s:%d: %s\n", dosya, satir, ileti.c_str());
        }
    }
    // |gercek − beklenen| ≤ mutlak + bagil · |beklenen|
    void yakin(double gercek, double beklenen, double bagil, double mutlak, const char* dosya,
               int satir, const char* ad)
    {
        const double fark = std::abs(gercek - beklenen);
        const bool tamam = std::isfinite(gercek) && fark <= mutlak + bagil * std::abs(beklenen);
        if (!tamam) {
            ++hata;
            std::printf("  KALDI %s:%d: %s: gerçek %.17g, beklenen %.17g, fark %.3g "
                        "(bağıl tol %.3g, mutlak tol %.3g)\n",
                        dosya, satir, ad, gercek, beklenen, fark, bagil, mutlak);
        }
    }
};

#define DOGRULA(d, kosul, ileti) (d).dogrula((kosul), __FILE__, __LINE__, (ileti))
#define YAKIN(d, gercek, beklenen, bagil, mutlak, ad) \
    (d).yakin((gercek), (beklenen), (bagil), (mutlak), __FILE__, __LINE__, (ad))

// Test parametre seçenekleri: İDA-0 üzerine.
struct TestAyari {
    bool notr_yuzerlik = false;     // hacim = m/ρ (boş araç)
    bool karesel_sonum_yok = false; // D_NL = 0
    bool sonum_yok = false;         // D_L = D_NL = 0, yüzey sönümü 0
    bool esc_hemen_hazir = true;    // baslatma_suresi_ns = 0
};

inline AracParametreleri test_araci(const TestAyari& t, double rho = 1000.0)
{
    AracParametreleri a = ida0_arac();
    if (t.notr_yuzerlik) {
        a.kutle.hacim = a.kutle.kutle / rho;
    }
    if (t.karesel_sonum_yok || t.sonum_yok) {
        a.hidrodinamik.karesel_sonum = {0, 0, 0, 0, 0, 0};
    }
    if (t.sonum_yok) {
        a.hidrodinamik.dogrusal_sonum = {0, 0, 0, 0, 0, 0};
        a.hidrodinamik.yuzey_sonum_orani = {0, 0, 0};
    }
    if (t.esc_hemen_hazir) {
        a.esc.baslatma_suresi_ns = 0;
    }
    return a;
}

inline BaslangicDurumu baslangic(double z, std::vector<std::string> yuklu = {})
{
    BaslangicDurumu b;
    b.konum = {0.0, 0.0, z};
    b.yuklu_tupler = std::move(yuklu);
    return b;
}

// Dünya kurar; başarısızsa iletiyi yazar.
inline bool kur(Dunya& d, const AracParametreleri& a, const OrtamParametreleri& o,
                const BaslangicDurumu& b, const SayisalAyarlar& s = {})
{
    const KurulumSonucu r = d.kur(a, o, b, s);
    if (!r.tamam) {
        std::printf("  kurulum başarısız: %s\n", r.hata.c_str());
    }
    return r.tamam;
}

inline bool kos(Dunya& d, std::uint64_t adim_sayisi)
{
    for (std::uint64_t i = 0; i < adim_sayisi; ++i) {
        if (d.adim() != AdimDurumu::tamam) {
            std::printf("  adım %llu başarısız\n", static_cast<unsigned long long>(d.adim_no()));
            return false;
        }
    }
    return true;
}

inline std::uint64_t adim_sayisi(double saniye, std::int64_t adim_ns = 1'250'000)
{
    return static_cast<std::uint64_t>(std::llround(saniye * 1e9 / static_cast<double>(adim_ns)));
}

// Gövde çerçevesinden Euler (roll, pitch, yaw), yalnız test gözlemi için.
inline Vek3 euler(const Kuaterniyon& q)
{
    const double w = q[0], x = q[1], y = q[2], z = q[3];
    const double roll = std::atan2(2.0 * (w * x + y * z), 1.0 - 2.0 * (x * x + y * y));
    const double s = std::fmax(-1.0, std::fmin(1.0, 2.0 * (w * y - z * x)));
    const double pitch = std::asin(s);
    const double yaw = std::atan2(2.0 * (w * z + x * y), 1.0 - 2.0 * (y * y + z * z));
    return {roll, pitch, yaw};
}

inline Kuaterniyon eksen_aci(int eksen, double aci)
{
    Kuaterniyon q{std::cos(aci / 2.0), 0.0, 0.0, 0.0};
    q[static_cast<std::size_t>(1 + eksen)] = std::sin(aci / 2.0);
    return q;
}

// Açık çevrim deneme profili (kanal sırası, µs), 180 s periyotla yinelenir: 2 s nötr (ESC
// başlatma), dalış, ileri
// gidiş, dönüş, nötr (yüzeye çıkış). Uzun koşu, belirlenimcilik ve süre testleri kullanır.
inline std::array<double, kIticiSayisi> profil_pwm(double t)
{
    std::array<double, kIticiSayisi> p{};
    p.fill(1500.0);
    const double f = std::fmod(t, 180.0);
    if (f < 2.0) {
        // nötr: ESC başlatma (nötr bekleme)
    } else if (f < 20.0) {
        for (int i = 4; i < 8; ++i) p[static_cast<std::size_t>(i)] = 1620.0;
    } else if (f < 60.0) {
        for (int i = 0; i < 4; ++i) p[static_cast<std::size_t>(i)] = 1600.0;
        for (int i = 4; i < 8; ++i) p[static_cast<std::size_t>(i)] = 1560.0;
    } else if (f < 120.0) {
        p[0] = 1400.0;
        p[2] = 1400.0;
        p[1] = 1600.0;
        p[3] = 1600.0;
        for (int i = 4; i < 8; ++i) p[static_cast<std::size_t>(i)] = 1540.0;
    }
    return p;
}

// Bir istenen gövde kuvvet-momenti için itici başına itki (en küçük norm; B⁺) ve onu veren
// PWM'ler (kanal sırası). İtki modelinin tersini iç modelle (ikiye bölme) bulur. Ölü bant
// nedeniyle elde edilemeyen (0 < |F| < F_en_küçük) bir itki varsa false döner.
bool tahsis_pwm(const AracParametreleri& a, const Vek6& tau, std::array<double, kIticiSayisi>& pwm,
                std::array<double, kIticiSayisi>* itki = nullptr);

// Durağan itki isteğinden PWM (tek itici); elde edilemiyorsa false.
bool itkiden_pwm(const AracParametreleri& a, double F, double& pwm);

}  // namespace ida::fizik::test
