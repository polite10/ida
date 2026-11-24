// İtici arızası: itici başına mekanik kuvvet çarpanı (A5 Tablo 3.10-A `aktuator/itki_carpani`;
// W4-K20; W4-sim-senaryo sol incelemesi #3).
#include "test_destek.hpp"

#include <cstring>

namespace ida::fizik::test {

namespace {

constexpr std::size_t kKanal = 0;   // sınanan kanal (t1)

// Derinde (β = 1), nötr yüzerlikte, ESC hemen hazır İDA-0; v_sabit = 15,6 V (İDA-0).
bool derin_dunya(Dunya& d)
{
    TestAyari t;
    t.notr_yuzerlik = true;
    return kur(d, test_araci(t), havuz_ortami(), baslangic(2.0));
}

std::array<double, kIticiSayisi> notr_ve(double pwm)
{
    std::array<double, kIticiSayisi> p{};
    p.fill(1500.0);
    p[kKanal] = pwm;
    return p;
}

std::array<double, kIticiSayisi> carpanlar(double c)
{
    std::array<double, kIticiSayisi> a{};
    a.fill(1.0);
    a[kKanal] = c;
    return a;
}

bool ayni_bitler(const DurumAnlik& a, const DurumAnlik& b)
{
    auto esit = [](const auto& x, const auto& y) { return std::memcmp(&x, &y, sizeof x) == 0; };
    return esit(a.konum_n, b.konum_n) && esit(a.yonelim, b.yonelim) && esit(a.dogrusal_hiz_b, b.dogrusal_hiz_b) &&
           esit(a.acisal_hiz_b, b.acisal_hiz_b) && esit(a.itki, b.itki) && esit(a.itki_istegi, b.itki_istegi) &&
           esit(a.itici_kuvvet_momenti, b.itici_kuvvet_momenti);
}

}  // namespace

// Çarpan 1 iken bit düzeyinde eski davranış (açık çevrim profili, 20 s).
int test_itki_carpani_birim(const TestBaglami&)
{
    Denetci dn;
    Dunya a, b;
    if (!kur(a, ida0_arac(), havuz_ortami(), baslangic(0.2)) || !kur(b, ida0_arac(), havuz_ortami(), baslangic(0.2))) {
        return 1;
    }
    std::array<double, kIticiSayisi> bir{};
    bir.fill(1.0);
    DOGRULA(dn, b.itki_carpani_ver(0, bir), "çarpan 1 verildi");
    bool ayni = true;
    for (std::uint64_t k = 0; k < adim_sayisi(20.0); ++k) {
        const auto p = profil_pwm(static_cast<double>(k) * 1.25e-3);
        (void)a.itici_komutu_ver(k, p);
        (void)b.itici_komutu_ver(k, p);
        if (k % 1000u == 0u) {
            (void)b.itki_carpani_ver(k, bir);
        }
        if (a.adim() != AdimDurumu::tamam || b.adim() != AdimDurumu::tamam) {
            return 1;
        }
        ayni = ayni && ayni_bitler(a.durum(), b.durum());
    }
    DOGRULA(dn, ayni, "çarpan 1: her adımda bit düzeyinde aynı durum");
    DOGRULA(dn, !b.itki_carpani_ver(b.adim_no(), carpanlar(1.5)) && !b.itki_carpani_ver(b.adim_no(), carpanlar(-0.1)) &&
                    !b.itki_carpani_ver(b.adim_no() - 1u, bir),
            "aralık dışı çarpan ve geçmiş adım reddedilir");
    return dn.hata;
}

// Başlangıç, bitiş, yön değiştirme ve elektrik tarafı (sol karşı örneği): 15,6 V, β = 1,
// yerleşmiş 1700 µs'den 1300 µs'ye geçiş ve aynı sınırda çarpan 0,4; geçişten 18,75 ms sonra
// itki ≈ −0,46898 N (arızasız ≈ −1,17245 N; sol hesabı). Çarpan süzgeçten sonra uygulandığından
// arızalı itki her adımda arızasızın tam 0,4 katıdır ve mutlak değerce hiçbir zaman büyük
// değildir; F_ss (elektrik tarafı) arızasızla bit düzeyinde aynıdır; çarpan kalkınca itki aynı
// adımda arızasıza döner.
int test_itki_carpani_gecis(const TestBaglami&)
{
    Denetci dn;
    Dunya ref, ar;
    if (!derin_dunya(ref) || !derin_dunya(ar)) {
        return 1;
    }
    const std::uint64_t n_yerles = adim_sayisi(2.0);
    for (std::uint64_t k = 0; k < n_yerles; ++k) {
        (void)ref.itici_komutu_ver(k, notr_ve(1700.0));
        (void)ar.itici_komutu_ver(k, notr_ve(1700.0));
        if (ref.adim() != AdimDurumu::tamam || ar.adim() != AdimDurumu::tamam) {
            return 1;
        }
    }
    DOGRULA(dn, ref.durum().havalanma[kKanal] == 1.0, "β = 1 (tamamen batık)");
    const std::uint64_t n0 = ref.adim_no();
    (void)ref.itici_komutu_ver(n0, notr_ve(1300.0));
    (void)ar.itici_komutu_ver(n0, notr_ve(1300.0));
    DOGRULA(dn, ar.itki_carpani_ver(n0, carpanlar(0.4)), "çarpan 0,4 verildi");
    const std::uint64_t n_bitis = n0 + 200u;   // 250 ms sonra çarpan kalkar
    DOGRULA(dn, ar.itki_carpani_ver(n_bitis, carpanlar(1.0)), "bitiş verildi");
    bool oran = true, buyuk_degil = true, elektrik = true, bitis = true;
    double itki_1875 = 0.0, ref_1875 = 0.0;
    for (std::uint64_t k = n0; k < n0 + 400u; ++k) {
        if (k > n0) {
            (void)ref.itici_komutu_ver(k, notr_ve(1300.0));
            (void)ar.itici_komutu_ver(k, notr_ve(1300.0));
        }
        if (ref.adim() != AdimDurumu::tamam || ar.adim() != AdimDurumu::tamam) {
            return 1;
        }
        const double fr = ref.durum().itki[kKanal];
        const double fa = ar.durum().itki[kKanal];
        elektrik = elektrik && ref.durum().itki_istegi[kKanal] == ar.durum().itki_istegi[kKanal];
        if (k < n_bitis) {
            oran = oran && fa == fr * 0.4;
            buyuk_degil = buyuk_degil && std::abs(fa) <= std::abs(fr);
        } else {
            bitis = bitis && fa == fr;
        }
        if (k + 1u == n0 + 15u) {   // 15 adım = 18,75 ms sonraki durum
            itki_1875 = fa;
            ref_1875 = fr;
        }
    }
    std::printf("  18,75 ms: arızalı %.5f N, arızasız %.5f N\n", itki_1875, ref_1875);
    YAKIN(dn, ref_1875, -1.17245, 0.0, 5e-5, "arızasız itki (sol hesabı)");
    YAKIN(dn, itki_1875, -0.46898, 0.0, 5e-5, "arızalı itki (sol hesabı)");
    DOGRULA(dn, oran, "arıza boyunca itki = 0,4 · arızasız (süzgeç ve β sonrası)");
    DOGRULA(dn, buyuk_degil, "yön değişiminde arızalı itki mutlak değerce arızasızdan büyük değil");
    DOGRULA(dn, elektrik, "elektrik tarafı (F_ss) arızasız komutu görür");
    DOGRULA(dn, bitis, "çarpan kalkınca itki aynı adımda arızasıza döner (süzgeç durumu arızasız)");
    return dn.hata;
}

}  // namespace ida::fizik::test
