// Kabul 1 (hidrostatik denge) ve kabul 2 (serbest sönüm).
#include "test_destek.hpp"

#include <vector>

namespace ida::fizik::test {

namespace {

// Yer tutucu yüzerlik kutusu (04 §2.6): taban 0,40 m (x) × 0,48 m (y).
constexpr double kKutuAlani = 0.40 * 0.48;

}  // namespace

// Kabul 1: araç durgun bırakılınca beklenen su hattında dengelenir; batık oran parametrelerden
// hesaplanan değerle (s = m / (ρ ∇)) uyuşur.
int test_hidrostatik_denge(const TestBaglami&)
{
    Denetci d;
    // Toleranslar. Simetrik durumlar (boş, iki yüklü): hareket yalnız dalıp çıkma–yalpa–baş-kıç
    // düzlemindedir ve 120 s sonunda geçici yanıtın kalıntısı (en yavaş kip ≈ 0,35 s zaman
    // sabitiyle sönümlenir) yuvarlama düzeyindedir; 1e-9 bağıl / 1e-8 m / 1e-8 hız birimi yalnız
    // kayan nokta birikimine pay bırakır. Bir yüklü (asimetrik) durum: bırakılınca yatarken yana
    // da kayar; İDA-0'da Y_v = 0 olduğundan yan hız yalnız karesel sönümle cebirsel azalır
    // (v ≲ M₂₂ / (Y_v|v| t) ≈ 20,6 / (217 · 120) ≈ 8e-4 m/s); bu yüzden hız toleransı 2e-3,
    // batık oran toleransı (bu kalıntı hareketin dikey bağlaşımı) 1e-6 bağıl.
    const double kOranTol = 1e-9;
    const double kKonumTol = 1e-8;
    const double kHizTol = 1e-8;
    const double kOranTolAsim = 1e-6;
    const double kHizTolAsim = 2e-3;
    const struct {
        const char* ad;
        std::vector<std::string> yuk;
        bool simetrik;
    } durumlar[] = {{"bos", {}, true}, {"f1", {"f1"}, false}, {"iki", {"f1", "f2"}, true}};

    for (const auto& s : durumlar) {
        const AracParametreleri a = ida0_arac();  // tam İDA-0 (karesel sönüm dahil)
        const OrtamParametreleri o = havuz_ortami();
        Dunya dn;
        if (!kur(dn, a, o, baslangic(0.03, s.yuk))) return 1;
        if (!kos(dn, adim_sayisi(120.0))) return 1;
        const KutleDurumu& k = dn.turetilmis().kutle;
        const DurumAnlik& an = dn.durum();
        const double s_beklenen = k.kutle / (o.su.yogunluk * k.hacim);
        const Vek3 e = euler(an.yonelim);
        std::printf("  %s: s = %.12f (beklenen %.12f), z = %.9f m, roll %.3e pitch %.3e rad\n", s.ad,
                    an.batik_oran, s_beklenen, an.konum_n[2], e[0], e[1]);
        const double oran_tol = s.simetrik ? kOranTol : kOranTolAsim;
        const double hiz_tol = s.simetrik ? kHizTol : kHizTolAsim;
        YAKIN(d, an.batik_oran, s_beklenen, oran_tol, 0.0, "batık oran");
        for (int i = 0; i < 3; ++i) {
            YAKIN(d, an.dogrusal_hiz_b[static_cast<std::size_t>(i)], 0.0, 0.0, hiz_tol, "doğrusal hız");
            YAKIN(d, an.acisal_hiz_b[static_cast<std::size_t>(i)], 0.0, 0.0, hiz_tol, "açısal hız");
        }
        DOGRULA(d, an.batik_oran < 1.0, "araç yüzmüyor (tam batık)");
        if (s.simetrik) {
            // Analitik su hattı: su üstündeki hacim ∇ − m/ρ yalnız kutudadır (torpidolar tam
            // batık); kutu üst yüzü gövdede r_b,z − H/2, H = ∇_boş / A.
            const double H = a.kutle.hacim / kKutuAlani;
            const double ust = a.kutle.r_b[2] - H / 2.0;
            const double v_hava = k.hacim - k.kutle / o.su.yogunluk;
            const double z_beklenen = -(v_hava / kKutuAlani) - ust;
            YAKIN(d, an.konum_n[2], z_beklenen, 0.0, kKonumTol, "denge derinliği");
            YAKIN(d, dn.turetilmis().denge_konumu_z, z_beklenen, 0.0, 1e-12, "kurulumdaki denge");
            YAKIN(d, e[0], 0.0, 0.0, kKonumTol, "roll");
            YAKIN(d, e[1], 0.0, 0.0, kKonumTol, "pitch");
        }
    }
    return d.hata;
}

namespace {

struct Ornek {
    double t;
    double x;
};

// Sıfır geçişleri (doğrusal ara değerleme) ve uç değerler (üç noktalı parabol).
struct Salinim {
    std::vector<double> sifirlar;
    std::vector<double> uclar;  // |x| değerleri (ilk eleman başlangıç değeri)
};

Salinim cozumle(const std::vector<Ornek>& o)
{
    Salinim s;
    s.uclar.push_back(std::abs(o.front().x));
    for (std::size_t i = 1; i < o.size(); ++i) {
        if ((o[i - 1].x > 0.0) != (o[i].x > 0.0)) {
            const double t = o[i - 1].t + (o[i].t - o[i - 1].t) * o[i - 1].x / (o[i - 1].x - o[i].x);
            s.sifirlar.push_back(t);
        }
        if (i + 1 < o.size() && i > 1) {
            const double a = o[i - 1].x, b = o[i].x, c = o[i + 1].x;
            if ((b - a) * (c - b) < 0.0) {
                // Parabol tepe değeri: b − (c − a)² / (8 (a − 2b + c))
                const double payda = a - 2.0 * b + c;
                s.uclar.push_back(std::abs(b - (c - a) * (c - a) / (8.0 * payda)));
            }
        }
    }
    return s;
}

// Doğrusal sönümlü salınıcı: M ẍ + c ẋ + k x = 0, x(0) = x0, ẋ(0) = 0.
// Ardışık sıfır geçişleri arası π/ω_d; ardışık uç değerlerin oranı exp(−ζ ω_n π / ω_d).
void salinim_denetle(Denetci& d, const char* ad, const std::vector<Ornek>& o, double M, double c,
                     double k, int uc_sayisi, double periyot_tol, double oran_tol)
{
    const double wn = std::sqrt(k / M);
    const double zeta = c / (2.0 * M * wn);
    const double wd = wn * std::sqrt(1.0 - zeta * zeta);
    const double yari = std::acos(-1.0) / wd;
    const double oran = std::exp(-zeta * wn * yari);
    const Salinim s = cozumle(o);
    std::printf("  %s: ω_n %.6f rad/s, ζ %.6f, T_d %.6f s, beklenen yarım periyot %.6f s, uç oranı %.6f\n",
                ad, wn, zeta, 2.0 * yari, yari, oran);
    DOGRULA(d, s.sifirlar.size() >= 2 && s.uclar.size() >= static_cast<std::size_t>(uc_sayisi + 1),
            std::string(ad) + ": yeterli salınım yok");
    if (s.sifirlar.size() >= 2) {
        const double olculen = s.sifirlar[1] - s.sifirlar[0];
        std::printf("    ölçülen yarım periyot %.6f s\n", olculen);
        YAKIN(d, olculen, yari, periyot_tol, 0.0, "yarım periyot");
    }
    for (int i = 1; i <= uc_sayisi && static_cast<std::size_t>(i) < s.uclar.size(); ++i) {
        const double olculen = s.uclar[static_cast<std::size_t>(i)] / s.uclar[static_cast<std::size_t>(i - 1)];
        std::printf("    uç oranı %d: %.6f\n", i, olculen);
        YAKIN(d, olculen, oran, oran_tol, 0.0, "uç oranı");
    }
}

}  // namespace

// Kabul 2: dalıp çıkma (yüzeyde), yalpa ve baş-kıç (tam batık) salınımlarının periyodu ve sönümü
// doğrusal modelin analitik değeriyle uyuşur. Karesel sönüm kapalı (doğrusal model).
int test_serbest_sonum(const TestBaglami&)
{
    Denetci d;
    // Toleranslar: yarım periyot %0,2, uç oranı %1. Gerekçe: küçük genlikli doğrusallaştırma
    // hatası (yalpa/baş-kıçta sin θ ≈ θ: θ₀²/16 ≈ 2,5e-5 ve 1,6e-4; dalıp çıkmada s_D(s) = s
    // çarpanının ±0,3 mm genlikteki ±%0,4 değişimi doğrusal sönüm payında ζ'yi ≈ %0,1 oynatır),
    // RK4 faz hatası (h ω ≤ 0,01: ihmal edilebilir) ve örneklemeden gelen ara değerleme hatası.
    const double kPeriyotTol = 2e-3;
    const double kOranTol = 1e-2;
    const OrtamParametreleri o = havuz_ortami();
    const double rho = o.su.yogunluk, g = o.yercekimi;

    // (a) Dalıp çıkma, yüzeyde, boş araç. k = ρ g A_wp; c = s Z_w + 2 ζ₃ ω₃ M₃₃.
    {
        TestAyari ta;
        ta.karesel_sonum_yok = true;
        const AracParametreleri a = test_araci(ta);
        Dunya dn;
        if (!kur(dn, a, o, baslangic(0.0, {}))) return 1;
        const double z0 = dn.turetilmis().denge_konumu_z;
        const double x0 = 3e-4;  // m; serbest borda ≈ 1 mm, kutu üstü suya girmez
        if (!kur(dn, a, o, baslangic(z0 + x0, {}))) return 1;
        const double M33 = a.kutle.kutle + a.hidrodinamik.eklenmis_kutle[2];
        const double k = rho * g * kKutuAlani;
        const double w3 = std::sqrt(k / M33);
        const double s_eq = a.kutle.kutle / (rho * a.kutle.hacim);
        const double c = s_eq * a.hidrodinamik.dogrusal_sonum[2] +
                         2.0 * a.hidrodinamik.yuzey_sonum_orani[0] * w3 * M33;
        YAKIN(d, dn.turetilmis().yuzey_dogal_frekans[0], w3, 1e-6, 0.0, "ω₃ (kurulum, ağdan)");
        std::printf("  yüzey ω₃ = %.6f rad/s (04 §7.5: 7,654)\n", w3);
        std::vector<Ornek> ornek{{0.0, x0}};
        for (std::uint64_t i = 0; i < adim_sayisi(3.0); ++i) {
            if (dn.adim() != AdimDurumu::tamam) return 1;
            ornek.push_back({static_cast<double>(dn.zaman_ns()) * 1e-9, dn.durum().konum_n[2] - z0});
            DOGRULA(d, dn.durum().yuzey_sonumu_agirligi == 1.0, "kutu üstü suya girdi");
        }
        salinim_denetle(d, "dalıp çıkma", ornek, M33, c, k, 2, kPeriyotTol, kOranTol);
    }

    // (b) Yalpa, tam batık (5 m), nötr yüzerlik, K_p = 0,1 (test değeri; İDA-0'da 0).
    // (c) Baş-kıç, aynı koşul, M_q = 0,8 (İDA-0 değeri). k = m g BG, BG = −r_b,z.
    for (int eksen = 0; eksen < 2; ++eksen) {
        TestAyari ta;
        ta.karesel_sonum_yok = true;
        ta.notr_yuzerlik = true;
        AracParametreleri a = test_araci(ta, rho);
        if (eksen == 0) a.hidrodinamik.dogrusal_sonum[3] = 0.1;
        const double aci0 = eksen == 0 ? 0.02 : 0.05;
        BaslangicDurumu b = baslangic(5.0, {});
        b.yonelim = eksen_aci(eksen, aci0);
        Dunya dn;
        if (!kur(dn, a, o, b)) return 1;
        const auto i3 = static_cast<std::size_t>(3 + eksen);
        const double M = a.kutle.atalet[static_cast<std::size_t>(eksen)][static_cast<std::size_t>(eksen)] +
                         a.hidrodinamik.eklenmis_kutle[i3];
        const double k = a.kutle.kutle * g * (-a.kutle.r_b[2]);
        const double c = a.hidrodinamik.dogrusal_sonum[i3];
        std::vector<Ornek> ornek{{0.0, aci0}};
        for (std::uint64_t i = 0; i < adim_sayisi(eksen == 0 ? 12.0 : 10.0); ++i) {
            if (dn.adim() != AdimDurumu::tamam) return 1;
            ornek.push_back({static_cast<double>(dn.zaman_ns()) * 1e-9,
                             euler(dn.durum().yonelim)[static_cast<std::size_t>(eksen)]});
        }
        salinim_denetle(d, eksen == 0 ? "yalpa" : "baş-kıç", ornek, M, c, k, 2, kPeriyotTol, kOranTol);
    }
    return d.hata;
}

}  // namespace ida::fizik::test
