// Kabul 4 (enerji), 5 (adım yarılama), 6 (belirlenimcilik), 8 (uzun koşu), 9 (süre).
#include "test_destek.hpp"

#include <algorithm>
#include <chrono>
#include <cstring>
#include <vector>

namespace ida::fizik::test {

// Kabul 4: itki yokken toplam mekanik enerji artmaz.
int test_enerji(const TestBaglami&)
{
    Denetci d;
    const OrtamParametreleri o = havuz_ortami();

    // (a) Sönümsüz, tam batık (200 m), nötr yüzerlik, altı eksende ilk hız: enerji korunur
    // (Coriolis terimleri iş yapmaz). 600 s (480 000 adım). Tolerans: 04 §7.2'nin ölçütü,
    // bağıl sürüklenme < 1e-6 (kinetik enerjinin başlangıç değerine göre).
    {
        TestAyari ta;
        ta.sonum_yok = true;
        ta.notr_yuzerlik = true;
        const AracParametreleri a = test_araci(ta);
        BaslangicDurumu b = baslangic(200.0, {});
        b.hiz = {0.1, 0.05, -0.03, 0.2, -0.15, 0.1};
        Dunya dn;
        if (!kur(dn, a, o, b)) return 1;
        const double E0 = dn.mekanik_enerji();
        double kin0 = 0.0;
        {
            const auto& M = dn.turetilmis().kutle_matrisi;
            for (int i = 0; i < 6; ++i)
                for (int j = 0; j < 6; ++j)
                    kin0 += 0.5 * b.hiz[static_cast<std::size_t>(i)] * M[static_cast<std::size_t>(i)][static_cast<std::size_t>(j)] *
                            b.hiz[static_cast<std::size_t>(j)];
        }
        double en_buyuk = 0.0;
        const std::uint64_t n = adim_sayisi(600.0);
        for (std::uint64_t i = 0; i < n; ++i) {
            if (dn.adim() != AdimDurumu::tamam) return 1;
            if (i % 80 == 0 || i + 1 == n) {
                en_buyuk = std::max(en_buyuk, std::abs(dn.mekanik_enerji() - E0));
            }
        }
        std::printf("  (a) korunum: E0 = %.9f J, kinetik0 = %.6f J, en büyük |ΔE|/kinetik0 = %.3e, derinlik %.2f m\n",
                    E0, kin0, en_buyuk / kin0, dn.durum().konum_n[2]);
        DOGRULA(d, en_buyuk / kin0 < 1e-6, "sönümsüz enerji sürüklenmesi 1e-6'yı aştı");
        DOGRULA(d, dn.durum().konum_n[2] > 1.0, "araç yüzeye çıktı (deney geçersiz)");
    }

    // (b) Tam İDA-0 (sönümler, yüzey geçişi, pozitif yüzerlik): yüzeyin altına itilmiş,
    // yalpa 0,3 rad ve baş-kıç −0,2 rad eğik, hızlı bırakılan araç. Her adımda
    // E(t+h) ≤ E(t) + ε. ε = 1e-9 J (≈ 1e-9 bağıl): RK4'ün su hattı geçişindeki (kuvvetin türevi
    // süreksiz) yerel hatası ve yuvarlama için pay; fiziksel sönüm aynı adımda bundan
    // büyükken bu payın kullanılmaması beklenir.
    {
        const AracParametreleri a = ida0_arac();
        BaslangicDurumu b = baslangic(0.10, {"f1", "f2"});
        const Kuaterniyon qr = eksen_aci(0, 0.3), qp = eksen_aci(1, -0.2);
        b.yonelim = {qr[0] * qp[0], qr[1] * qp[0], qr[0] * qp[2], qr[1] * qp[2]};
        b.hiz = {0.2, -0.1, 0.05, 0.5, -0.3, 0.2};
        Dunya dn;
        if (!kur(dn, a, o, b)) return 1;
        const double eps = 1e-9;
        double E = dn.mekanik_enerji();
        const double E0 = E;
        double en_buyuk_artis = -HUGE_VAL;
        std::uint64_t ihlal = 0;
        const std::uint64_t n = adim_sayisi(60.0);
        for (std::uint64_t i = 0; i < n; ++i) {
            if (dn.adim() != AdimDurumu::tamam) return 1;
            const double E1 = dn.mekanik_enerji();
            en_buyuk_artis = std::max(en_buyuk_artis, E1 - E);
            if (E1 > E + eps) ++ihlal;
            E = E1;
        }
        std::printf("  (b) sönümlü: E0 = %.6f J, E(60 s) = %.6f J, adım başına en büyük artış %.3e J, ihlal %llu\n",
                    E0, E, en_buyuk_artis, static_cast<unsigned long long>(ihlal));
        DOGRULA(d, ihlal == 0, "sönümlü koşuda enerji arttı");
        DOGRULA(d, E < E0, "enerji azalmadı");
    }
    return d.hata;
}

namespace {

struct Ozet {
    std::array<double, 13> x{};
};

Ozet son_durum(std::int64_t adim_ns, double sure)
{
    TestAyari ta;
    ta.karesel_sonum_yok = true;  // |ν|ν'nin ν = 0'daki ikinci türev süreksizliği mertebeyi düşürür
    const AracParametreleri a = test_araci(ta);
    BaslangicDurumu b = baslangic(20.0, {"f1"});  // asimetrik kütle durumu: bağlaşım
    b.hiz = {0.3, -0.1, 0.05, 0.2, 0.1, -0.15};
    SayisalAyarlar s;
    s.adim_ns = adim_ns;
    Dunya dn;
    Ozet o;
    if (!kur(dn, a, havuz_ortami(), b, s)) return o;
    std::array<double, kIticiSayisi> p{1650.0, 1600.0, 1420.0, 1560.0, 1580.0, 1440.0, 1610.0, 1530.0};
    dn.itici_komutu_ver(0, p);
    kos(dn, adim_sayisi(sure, adim_ns));
    const auto& an = dn.durum();
    for (int i = 0; i < 3; ++i) {
        const auto u = static_cast<std::size_t>(i);
        o.x[u] = an.konum_n[u];
        o.x[7 + u] = an.dogrusal_hiz_b[u];
        o.x[10 + u] = an.acisal_hiz_b[u];
    }
    for (int i = 0; i < 4; ++i) o.x[3 + static_cast<std::size_t>(i)] = an.yonelim[static_cast<std::size_t>(i)];
    return o;
}

double fark(const Ozet& a, const Ozet& b)
{
    double e = 0.0;
    for (std::size_t i = 0; i < a.x.size(); ++i) e = std::max(e, std::abs(a.x[i] - b.x[i]));
    return e;
}

}  // namespace

// Kabul 5: h, h/2, h/4 ile aynı senaryoda hata beklenen mertebeyle (RK4: 4) küçülür.
// Düzgün alt sistem (04 §2.10): tam batık, ideal kaynak, ölü bant dışında sabit komut, karesel
// sönüm kapalı. Ölçüt: e₁/e₂ = |x_h − x_{h/2}| / |x_{h/2} − x_{h/4}| ≈ 2⁴ = 16.
int test_adim_yarilama(const TestBaglami&)
{
    Denetci d;
    // Tolerans: oran [14, 18] (gözlenen mertebe 3,81–4,17). Asimptotik bölgede oran 16'ya
    // yakınsar; 1,25 ms'de en hızlı kip (itici filtresi, |λ| ≈ 91 rad/s, h λ ≈ 0,11) yüksek
    // mertebe terimlerini yüzde birkaç düzeyinde tutar.
    const double sure = 2.0;
    const Ozet x1 = son_durum(1'250'000, sure);
    const Ozet x2 = son_durum(625'000, sure);
    const Ozet x4 = son_durum(312'500, sure);
    const double e1 = fark(x1, x2);
    const double e2 = fark(x2, x4);
    const double oran = e1 / e2;
    std::printf("  e(h, h/2) = %.3e, e(h/2, h/4) = %.3e, oran %.3f, gözlenen mertebe %.3f\n", e1, e2,
                oran, std::log2(oran));
    DOGRULA(d, e2 > 1e-13, "hata yuvarlama düzeyinde: deney ayırt edici değil");
    DOGRULA(d, oran > 14.0 && oran < 18.0, "yakınsama oranı 16'dan uzak");
    return d.hata;
}

namespace {

void anlik_ekle(std::vector<double>& v, const DurumAnlik& a)
{
    auto ekle = [&](const auto& dizi) { v.insert(v.end(), dizi.begin(), dizi.end()); };
    v.push_back(static_cast<double>(a.t_ns));
    ekle(a.konum_n);
    ekle(a.yonelim);
    ekle(a.dogrusal_hiz_b);
    ekle(a.acisal_hiz_b);
    ekle(a.dogrusal_ivme_b);
    ekle(a.acisal_ivme_b);
    ekle(a.ozgul_kuvvet_b);
    ekle(a.itki);
    ekle(a.itki_istegi);
    ekle(a.havalanma);
    ekle(a.itici_kuvvet_momenti);
    v.push_back(a.batik_oran);
    v.push_back(a.batik_hacim);
    v.push_back(a.yuzey_sonumu_agirligi);
}

std::vector<double> kosu_dizisi(double sure)
{
    std::vector<double> v;
    Dunya dn;
    if (!kur(dn, ida0_arac(), havuz_ortami(), baslangic(0.03, {"f1", "f2"}))) return v;
    const std::uint64_t n = adim_sayisi(sure);
    anlik_ekle(v, dn.durum());
    for (std::uint64_t i = 0; i < n; ++i) {
        const double t = static_cast<double>(dn.zaman_ns()) * 1e-9;
        dn.itici_komutu_ver(dn.adim_no(), profil_pwm(t));
        if (dn.adim() != AdimDurumu::tamam) return {};
        anlik_ekle(v, dn.durum());
    }
    return v;
}

}  // namespace

// Kabul 6: aynı girdiyle iki koşu bit düzeyinde aynı durum dizisini verir (aynı ikili, makine).
int test_belirlenimcilik(const TestBaglami&)
{
    Denetci d;
    const double sure = 200.0;  // profilin bir periyodu + yüzey geçişleri
    const std::vector<double> a = kosu_dizisi(sure);
    const std::vector<double> b = kosu_dizisi(sure);
    DOGRULA(d, !a.empty() && a.size() == b.size(), "koşu başarısız");
    const bool esit = !a.empty() && a.size() == b.size() &&
                      std::memcmp(a.data(), b.data(), a.size() * sizeof(double)) == 0;
    std::printf("  %zu değer karşılaştırıldı: %s\n", a.size(), esit ? "bit düzeyinde eşit" : "FARKLI");
    DOGRULA(d, esit, "iki koşu bit düzeyinde aynı değil");
    return d.hata;
}

// Kabul 8: 10 dakikalık simülasyonda kuaterniyon normu ve durum sonlu kalır.
int test_uzun_kosu(const TestBaglami&)
{
    Denetci d;
    // Tolerans: her adım sonunda normalleştirildiği için ||q| − 1| yalnız yuvarlamadır (≤ 1e-14).
    Dunya dn;
    if (!kur(dn, ida0_arac(), havuz_ortami(), baslangic(0.03, {"f1", "f2"}))) return 1;
    const std::uint64_t n = adim_sayisi(600.0);
    double en_buyuk_norm_sapmasi = 0.0, en_derin = 0.0, en_buyuk_hiz = 0.0;
    bool sonlu = true;
    for (std::uint64_t i = 0; i < n; ++i) {
        const double t = static_cast<double>(dn.zaman_ns()) * 1e-9;
        dn.itici_komutu_ver(dn.adim_no(), profil_pwm(t));
        if (dn.adim() != AdimDurumu::tamam) {
            DOGRULA(d, false, "adım başarısız");
            return 1;
        }
        const auto& a = dn.durum();
        const auto& q = a.yonelim;
        en_buyuk_norm_sapmasi = std::max(
            en_buyuk_norm_sapmasi, std::abs(std::sqrt(q[0] * q[0] + q[1] * q[1] + q[2] * q[2] + q[3] * q[3]) - 1.0));
        for (double x : a.konum_n) sonlu = sonlu && std::isfinite(x);
        for (double x : a.dogrusal_hiz_b) sonlu = sonlu && std::isfinite(x);
        for (double x : a.acisal_hiz_b) sonlu = sonlu && std::isfinite(x);
        for (double x : a.ozgul_kuvvet_b) sonlu = sonlu && std::isfinite(x);
        en_derin = std::max(en_derin, a.konum_n[2]);
        en_buyuk_hiz = std::max(en_buyuk_hiz, std::abs(a.dogrusal_hiz_b[0]));
    }
    std::printf("  600 s: en büyük ||q|−1| = %.3e, normalleştirme öncesi >1e-9 sapma sayısı %llu, "
                "en derin %.2f m, en büyük |u| %.3f m/s\n",
                en_buyuk_norm_sapmasi, static_cast<unsigned long long>(dn.kuaterniyon_sapma_sayaci()),
                en_derin, en_buyuk_hiz);
    DOGRULA(d, sonlu, "durum sonlu değil");
    DOGRULA(d, en_buyuk_norm_sapmasi <= 1e-14, "kuaterniyon normu saptı");
    return d.hata;
}

// Kabul 9 (ölçüm, eşik yok): tek adımın ortalama ve en kötü süresi.
int test_sure(const TestBaglami&)
{
    Dunya dn;
    if (!kur(dn, ida0_arac(), havuz_ortami(), baslangic(0.03, {"f1", "f2"}))) return 1;
    const std::uint64_t n = adim_sayisi(200.0);
    std::vector<double> us;
    us.reserve(static_cast<std::size_t>(n));
    for (std::uint64_t i = 0; i < n; ++i) {
        const double t = static_cast<double>(dn.zaman_ns()) * 1e-9;
        dn.itici_komutu_ver(dn.adim_no(), profil_pwm(t));
        const auto t0 = std::chrono::steady_clock::now();
        const AdimDurumu r = dn.adim();
        const auto t1 = std::chrono::steady_clock::now();
        if (r != AdimDurumu::tamam) return 1;
        us.push_back(std::chrono::duration<double, std::micro>(t1 - t0).count());
    }
    double toplam = 0.0;
    for (double x : us) toplam += x;
    std::vector<double> sirali = us;
    std::sort(sirali.begin(), sirali.end());
    const auto yuzdelik = [&](double p) {
        return sirali[static_cast<std::size_t>(p * static_cast<double>(sirali.size() - 1))];
    };
#ifdef NDEBUG
    const char* yapi = "eniyileştirilmiş (NDEBUG)";
#else
    const char* yapi = "hata ayıklama (NDEBUG yok)";
#endif
    std::printf("  ÖLÇÜM (%s, %zu adım, profil: dalış/ileri/dönüş/yüzeye çıkış): ortalama %.2f µs, "
                "p50 %.2f µs, p99 %.2f µs, p99,9 %.2f µs, en kötü %.2f µs\n",
                yapi, us.size(), toplam / static_cast<double>(us.size()), yuzdelik(0.5), yuzdelik(0.99),
                yuzdelik(0.999), sirali.back());
    return 0;
}

}  // namespace ida::fizik::test
