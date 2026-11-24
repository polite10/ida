// Dilim 2: fizik yükleyici, Ek A test vektörleri (A5-K23, K24), A5-K22, K34 ve koddaki İDA-0
// varsayılanlarıyla karşılaştırma (madde 11).
#include "ida/fizik/dunya.hpp"
#include "ida/senaryo/fizik_yukleyici.hpp"
#include "karsilastirma.hpp"
#include "test_destek.hpp"

#include <cmath>

namespace ida::senaryo::test {

namespace {

std::optional<FizikKurulumu> ornekten(const TestBaglami& tb, const BellekKaynagi* k = nullptr)
{
    const BellekKaynagi temel = temel_kaynak(tb);
    const CozumSonucu s = senaryo_coz(k != nullptr ? *k : temel, kSenaryo);
    if (!s.cozumlenmis) {
        std::printf("%s", bulgu_ozeti(s.rapor).c_str());
        return std::nullopt;
    }
    std::string hata;
    auto f = fizik_kur(*s.cozumlenmis, &hata);
    if (!f) {
        std::printf("  fizik_kur: %s\n", hata.c_str());
    }
    return f;
}

bool yakin(double g, double b, double bagil = 1e-4, double mutlak = 1e-7)
{
    return std::abs(g - b) <= mutlak + bagil * std::abs(b);
}

}  // namespace

int test_fizik_yukleyici(const TestBaglami& tb)
{
    Denetci d;
    const auto f = ornekten(tb);
    DOGRULA(d, f.has_value(), "örnekten fizik yapıları kurulamadı");
    if (!f) {
        return d.hata;
    }
    DOGRULA(d, f->arac.kutle.kutle == 13.5 && f->arac.iticiler[7].kanal == 7 && f->arac.iticiler[0].rol == "yatay, on sancak",
            "araç alanları");
    DOGRULA(d, f->ortam.derinlik == 3.0 && f->ortam.su.yogunluk == 1000.0 && f->ortam.yercekimi == 9.82, "ortam alanları");
    DOGRULA(d, f->baslangic.yuklu_tupler.size() == 2 && f->baslangic.yonelim[0] == 1.0, "başlangıç alanları");
    // Fizik çekirdeği bu yapılarla kurulur.
    fizik::Dunya dunya;
    const fizik::KurulumSonucu r = dunya.kur(f->arac, f->ortam, f->baslangic);
    DOGRULA(d, r.tamam, "Dunya::kur başarısız: " + r.hata);
    // Yeniden yüklenen altın dosyadan aynı yapılar (A5-K02 → yükleyici).
    const std::string altin = dosya_oku(tb.kok + "/scenarios/ornek-havuz.cozumlenmis.json");
    const YenidenYukleme y = cozumlenmis_yukle(altin, sha256_hex(altin));
    DOGRULA(d, y.tamam, "altın dosya yeniden yüklenemedi");
    if (y.cozumlenmis) {
        const auto g = fizik_kur(*y.cozumlenmis);
        Karsilastirma k;
        if (g) {
            k.mat3("atalet", g->arac.kutle.atalet, f->arac.kutle.atalet);
            k.dizi("eklenmis_kutle", g->arac.hidrodinamik.eklenmis_kutle, f->arac.hidrodinamik.eklenmis_kutle);
            k.tam("baslatma_suresi_ns", g->arac.esc.baslatma_suresi_ns, f->arac.esc.baslatma_suresi_ns);
            k.sayi("derinlik", g->ortam.derinlik, f->ortam.derinlik);
        }
        DOGRULA(d, g.has_value() && k.farklar.empty(), "JSON'dan ve TOML'dan kurulan yapılar aynı olmalı");
    }
    return d.hata;
}

int test_ek_a_vektorleri(const TestBaglami& tb)
{
    Denetci d;
    struct Satir {
        const char* ad;
        std::array<bool, 2> yuklu;
        double m;
        std::array<double, 3> rg;
        double hacim;
        std::array<double, 3> rb;
        std::array<double, 3> I;
        double Ixz;
        double Iyz;
    };
    // A5 Ek A
    const Satir bos[] = {
        {"boş", {false, false}, 13.5, {0, 0, 0}, 0.0137, {0, 0, -0.01}, {0.26, 0.23, 0.37}, 0, 0},
        {"f1", {true, false}, 13.675, {0, 0.0012797, 0.0012797}, 0.0138676, {0, 0.0012082, -0.0086709},
         {0.263528, 0.232114, 0.372114}, 0, -0.00175},
        {"iki", {true, true}, 13.85, {0, 0, 0.0025271}, 0.0140351, {0, 0, -0.0073736}, {0.267056, 0.234228, 0.374228}, 0, 0},
    };
    const Satir trimli[] = {
        {"boş+trim", {false, false}, 14.0, {0.0035714, 0, 0.0017857}, 0.0137, {0, 0, -0.01}, {0.26125, 0.23625, 0.375}, -0.0025, 0},
        {"f1+trim", {true, false}, 14.175, {0.0035273, 0.0012346, 0.0029982}, 0.0138676, {0, 0.0012082, -0.0086709},
         {0.264778, 0.238364, 0.377114}, -0.0025, -0.00175},
        {"iki+trim", {true, true}, 14.35, {0.0034843, 0, 0.0041812}, 0.0140351, {0, 0, -0.0073736},
         {0.268306, 0.240478, 0.379228}, -0.0025, 0},
    };
    auto denetle = [&](const FizikKurulumu& k, const Satir& s) {
        const fizik::KutleDurumu kd = kutle_durumu(k, s.yuklu);
        bool tamam = yakin(kd.kutle, s.m) && yakin(kd.hacim, s.hacim);
        for (std::size_t i = 0; i < 3; ++i) {
            tamam = tamam && yakin(kd.r_g[i], s.rg[i]) && yakin(kd.r_b[i], s.rb[i]) && yakin(kd.atalet[i][i], s.I[i]);
        }
        tamam = tamam && yakin(kd.atalet[0][2], s.Ixz) && yakin(kd.atalet[1][2], s.Iyz);
        DOGRULA(d, tamam, std::string("Ek A satırı tutmuyor: ") + s.ad);
    };
    const auto f = ornekten(tb);
    DOGRULA(d, f.has_value(), "örnek kurulamadı");
    if (f) {
        for (const auto& s : bos) denetle(*f, s);
    }
    // Trimli vektörler: trim dosyaya yazılarak (yükleyici yolundan).
    BellekKaynagi k = temel_kaynak(tb);
    k.dosyalar[kArac] = *degistir(k.dosyalar[kArac], "trim_kutlesi = 0.0 ", "trim_kutlesi = 0.5\ntrim_konumu = [0.10, 0.0, 0.05] ");
    const auto t = ornekten(tb, &k);
    DOGRULA(d, t.has_value(), "trimli örnek kurulamadı");
    if (t) {
        for (const auto& s : trimli) denetle(*t, s);
    }
    return d.hata;
}

int test_fizik_kurallari(const TestBaglami& tb)
{
    Denetci d;
    {
        BellekKaynagi k = temel_kaynak(tb);
        k.dosyalar[kArac] = *degistir(k.dosyalar[kArac], "r_g = [0.0, 0.0, 0.0]", "r_g = [0.0, 0.0, 0.5]");
        const CozumSonucu s = senaryo_coz(k, kSenaryo);
        const Bulgu* b = bulgu_bul(s.rapor, "A5-K22", "arac.kutle");
        DOGRULA(d, !s.rapor.gecerli && b != nullptr && b->asama == 2, "A5-K22 bekleniyordu\n" + bulgu_ozeti(s.rapor));
    }
    {
        BellekKaynagi k = temel_kaynak(tb);
        k.dosyalar[kArac] = *degistir(k.dosyalar[kArac], "yay_sabiti = 3.0e4", "yay_sabiti = 3.0e6");
        const CozumSonucu s = senaryo_coz(k, kSenaryo);
        const Bulgu* b = bulgu_bul(s.rapor, "A5-K34", "arac.temas.yay_sabiti");
        DOGRULA(d, s.rapor.gecerli && b != nullptr && b->onem == Onem::uyari, "A5-K34 uyarısı bekleniyordu\n" + bulgu_ozeti(s.rapor));
    }
    // İDA-0 değerleriyle ölçüt (04 §2.9'un hesabıyla karşılaştırma için yazdırılır).
    const auto f = ornekten(tb);
    DOGRULA(d, f.has_value(), "örnek kurulamadı");
    if (f) {
        for (int bit = 0; bit < 4; ++bit) {
            const TemasOlcutu t = temas_olcutu(*f, {(bit & 1) != 0, (bit & 2) != 0}, {-0.23, -0.29, -0.19}, {0.23, 0.29, 0.19});
            std::printf("  kütle durumu %d: ω_c h merkez düşey %.4f, tekil en büyük %.4f, dört alt köşe %.4f\n", bit,
                        t.merkez_dusey, t.tekil_azami, t.dort_kose_azami);
            DOGRULA(d, t.tekil_azami <= 0.2 && t.dort_kose_azami <= 0.2, "İDA-0'da temas ölçütü sağlanmalı");
        }
        const TemasOlcutu t0 = temas_olcutu(*f, {false, false}, {-0.23, -0.29, -0.19}, {0.23, 0.29, 0.19});
        // 04 §2.9: boş araç, merkezden düşey temas m_eff = 32,18 kg → ω_c h = 0,038.
        DOGRULA(d, yakin(t0.merkez_dusey, std::sqrt(3.0e4 / 32.18) * 1.25e-3, 1e-3), "merkez düşey ölçüt 04 §2.9 ile aynı olmalı");
    }
    return d.hata;
}

int test_fizik_karsilastirma(const TestBaglami& tb)
{
    Denetci d;
    const auto f = ornekten(tb);
    DOGRULA(d, f.has_value(), "örnek kurulamadı");
    if (!f) {
        return d.hata;
    }
    const fizik::AracParametreleri k = fizik::ida0_arac();
    const fizik::AracParametreleri& a = f->arac;
    Karsilastirma c;
    c.dizgi("arac.ad", a.ad, k.ad);
    c.dizgi("arac.varlik", a.varlik, k.varlik);
    c.dizgi("arac.referans_noktasi", a.referans_noktasi, k.referans_noktasi);
    c.sayi("arac.kutle.kutle", a.kutle.kutle, k.kutle.kutle);
    c.sayi("arac.kutle.hacim", a.kutle.hacim, k.kutle.hacim);
    c.dizi("arac.kutle.r_g", a.kutle.r_g, k.kutle.r_g);
    c.dizi("arac.kutle.r_b", a.kutle.r_b, k.kutle.r_b);
    c.mat3("arac.kutle.atalet", a.kutle.atalet, k.kutle.atalet);
    c.sayi("arac.kutle.trim_kutlesi", a.kutle.trim_kutlesi, k.kutle.trim_kutlesi);
    c.dizi("arac.kutle.trim_konumu", a.kutle.trim_konumu, k.kutle.trim_konumu);
    const auto& h = a.hidrodinamik;
    const auto& hk = k.hidrodinamik;
    c.dizgi("arac.hidrodinamik.isaret_kurali", h.isaret_kurali, hk.isaret_kurali);
    c.dizi("arac.hidrodinamik.eklenmis_kutle", h.eklenmis_kutle, hk.eklenmis_kutle);
    c.dizi("arac.hidrodinamik.dogrusal_sonum", h.dogrusal_sonum, hk.dogrusal_sonum);
    c.dizi("arac.hidrodinamik.karesel_sonum", h.karesel_sonum, hk.karesel_sonum);
    c.dizi("arac.hidrodinamik.yuzey_sonum_orani", h.yuzey_sonum_orani, hk.yuzey_sonum_orani);
    c.sayi("arac.hidrodinamik.ruzgar_direnc_katsayisi", h.ruzgar_direnc_katsayisi, hk.ruzgar_direnc_katsayisi);
    c.sayi("arac.hidrodinamik.belirsizlik.oteleme", h.belirsizlik.oteleme, hk.belirsizlik.oteleme);
    c.sayi("arac.hidrodinamik.belirsizlik.donme", h.belirsizlik.donme, hk.belirsizlik.donme);
    c.dizi("arac.hidrodinamik.sapma.eklenmis_kutle", h.sapma.eklenmis_kutle, hk.sapma.eklenmis_kutle);
    c.dizi("arac.hidrodinamik.sapma.dogrusal_sonum", h.sapma.dogrusal_sonum, hk.sapma.dogrusal_sonum);
    c.dizi("arac.hidrodinamik.sapma.karesel_sonum", h.sapma.karesel_sonum, hk.sapma.karesel_sonum);
    for (std::size_t i = 0; i < fizik::kIticiSayisi; ++i) {
        const std::string t = "arac.iticiler.t" + std::to_string(i + 1);
        c.tam(t + ".kanal", a.iticiler[i].kanal, k.iticiler[i].kanal);
        c.dizi(t + ".konum", a.iticiler[i].konum, k.iticiler[i].konum);
        c.dizi(t + ".yon", a.iticiler[i].yon, k.iticiler[i].yon);
        c.dizgi(t + ".rol", a.iticiler[i].rol, k.iticiler[i].rol);
    }
    const auto& m = a.itici_modeli;
    const auto& mk = k.itici_modeli;
    c.dizgi("arac.itici_modeli.tur", m.tur, mk.tur);
    c.dizi("arac.itici_modeli.sekil_polinomu", m.sekil_polinomu, mk.sekil_polinomu);
    c.dizi("arac.itici_modeli.gerilim_noktalari", m.gerilim_noktalari, mk.gerilim_noktalari);
    c.dizi("arac.itici_modeli.azami_itki_ileri", m.azami_itki_ileri, mk.azami_itki_ileri);
    c.dizi("arac.itici_modeli.azami_itki_geri", m.azami_itki_geri, mk.azami_itki_geri);
    c.dizi("arac.itici_modeli.tam_gaz_akimi", m.tam_gaz_akimi, mk.tam_gaz_akimi);
    c.sayi("arac.itici_modeli.akim_ussu", m.akim_ussu, mk.akim_ussu);
    c.dizi("arac.itici_modeli.transfer_pay", m.transfer_pay, mk.transfer_pay);
    c.dizi("arac.itici_modeli.transfer_payda", m.transfer_payda, mk.transfer_payda);
    c.sayi("arac.itici_modeli.pervane_yaricapi", m.pervane_yaricapi, mk.pervane_yaricapi);
    c.dizi("arac.itici_modeli.havalanma_h_bolu_r", m.havalanma_h_bolu_r, mk.havalanma_h_bolu_r);
    c.dizi("arac.itici_modeli.havalanma_beta", m.havalanma_beta, mk.havalanma_beta);
    c.tam("arac.esc.pwm_notr_us", a.esc.pwm_notr_us, k.esc.pwm_notr_us);
    c.tam("arac.esc.olu_bant_us", a.esc.olu_bant_us, k.esc.olu_bant_us);
    c.tam("arac.esc.pwm_min_us", a.esc.pwm_min_us, k.esc.pwm_min_us);
    c.tam("arac.esc.pwm_max_us", a.esc.pwm_max_us, k.esc.pwm_max_us);
    c.tam("arac.esc.baslatma_suresi_ns", a.esc.baslatma_suresi_ns, k.esc.baslatma_suresi_ns);
    c.sayi("arac.batarya.v_sabit", a.batarya.v_sabit, k.batarya.v_sabit);
    c.tam("arac.batarya.seri", a.batarya.seri, k.batarya.seri);
    c.tam("arac.batarya.paralel", a.batarya.paralel, k.batarya.paralel);
    c.sayi("arac.batarya.kapasite", a.batarya.kapasite, k.batarya.kapasite);
    c.sayi("arac.batarya.r_kablo", a.batarya.r_kablo, k.batarya.r_kablo);
    c.sayi("arac.batarya.aviyonik_guc", a.batarya.aviyonik_guc, k.batarya.aviyonik_guc);
    c.sayi("arac.temas.yay_sabiti", a.temas.yay_sabiti, k.temas.yay_sabiti);
    c.sayi("arac.temas.sonum_orani", a.temas.sonum_orani, k.temas.sonum_orani);
    c.sayi("arac.temas.surtunme_katsayisi", a.temas.surtunme_katsayisi, k.temas.surtunme_katsayisi);
    const auto& t = a.torpido;
    const auto& tk = k.torpido;
    c.dizgi("arac.torpido.varlik", t.varlik, tk.varlik);
    c.dizgi("arac.torpido.geometri", t.geometri, tk.geometri);
    c.sayi("arac.torpido.boy", t.boy, tk.boy);
    c.sayi("arac.torpido.cap", t.cap, tk.cap);
    c.sayi("arac.torpido.kutle", t.kutle, tk.kutle);
    c.dizi("arac.torpido.r_g", t.r_g, tk.r_g);
    c.sayi("arac.torpido.kanat_alani_duzlem", t.kanat_alani_duzlem, tk.kanat_alani_duzlem);
    c.sayi("arac.torpido.kanat_en_boy_orani", t.kanat_en_boy_orani, tk.kanat_en_boy_orani);
    c.sayi("arac.torpido.kanat_x", t.kanat_x, tk.kanat_x);
    c.sayi("arac.torpido.surukleme_katsayisi", t.surukleme_katsayisi, tk.surukleme_katsayisi);
    c.sayi("arac.torpido.ayrilma_hizi", t.ayrilma_hizi, tk.ayrilma_hizi);
    for (std::size_t i = 0; i < fizik::kFirlaticiSayisi; ++i) {
        const std::string fy = "arac.firlaticilar.f" + std::to_string(i + 1);
        c.dizi(fy + ".tup_konumu", a.firlaticilar[i].tup_konumu, k.firlaticilar[i].tup_konumu);
        c.dizi(fy + ".tup_ekseni", a.firlaticilar[i].tup_ekseni, k.firlaticilar[i].tup_ekseni);
        c.sayi(fy + ".tup_su_kutlesi", a.firlaticilar[i].tup_su_kutlesi, k.firlaticilar[i].tup_su_kutlesi);
    }
    c.yazdir("fizik araç (scenarios/ida0.toml ↔ fizik::ida0_arac())");

    const fizik::OrtamParametreleri ok = fizik::havuz_ortami();
    const fizik::OrtamParametreleri& o = f->ortam;
    Karsilastirma e;
    e.tam("ortam.tur", static_cast<int>(o.tur), static_cast<int>(ok.tur));
    e.sayi("ortam.derinlik", o.derinlik, ok.derinlik);
    e.sayi("ortam.yercekimi", o.yercekimi, ok.yercekimi);
    e.sayi("ortam.atmosfer_basinci", o.atmosfer_basinci, ok.atmosfer_basinci);
    e.sayi("ortam.hava_yogunlugu", o.hava_yogunlugu, ok.hava_yogunlugu);
    e.sayi("ortam.su.yogunluk", o.su.yogunluk, ok.su.yogunluk);
    e.sayi("ortam.su.sicaklik_c", o.su.sicaklik_c, ok.su.sicaklik_c);
    e.sayi("ortam.su.tuzluluk", o.su.tuzluluk, ok.su.tuzluluk);
    e.sayi("ortam.su.ph", o.su.ph, ok.su.ph);
    e.tam("ortam.dalga.spektrum", static_cast<int>(o.dalga.spektrum), static_cast<int>(ok.dalga.spektrum));
    e.sayi("ortam.dalga.hs", o.dalga.hs, ok.dalga.hs);
    e.tam("ortam.dalga.tepe_periyodu_ns", o.dalga.tepe_periyodu_ns, ok.dalga.tepe_periyodu_ns);
    e.sayi("ortam.dalga.gamma", o.dalga.gamma, ok.dalga.gamma);
    e.sayi("ortam.dalga.yon", o.dalga.yon, ok.dalga.yon);
    e.tam("ortam.dalga.yayilim", static_cast<int>(o.dalga.yayilim), static_cast<int>(ok.dalga.yayilim));
    e.sayi("ortam.dalga.yayilim_s", o.dalga.yayilim_s, ok.dalga.yayilim_s);
    e.tam("ortam.dalga.bilesen_sayisi", o.dalga.bilesen_sayisi, ok.dalga.bilesen_sayisi);
    e.dizi("ortam.akinti.hiz_ned", o.akinti.hiz_ned, ok.akinti.hiz_ned);
    e.mantik("ortam.akinti.gauss_markov.etkin", o.akinti.gauss_markov.etkin, ok.akinti.gauss_markov.etkin);
    e.dizi("ortam.ruzgar.hiz_ned", o.ruzgar.hiz_ned, ok.ruzgar.hiz_ned);
    e.mantik("ortam.ruzgar.gauss_markov.etkin", o.ruzgar.gauss_markov.etkin, ok.ruzgar.gauss_markov.etkin);
    e.yazdir("fizik ortam (scenarios/ornek-havuz.toml ↔ fizik::havuz_ortami())");

    const fizik::BaslangicDurumu bk{};
    Karsilastirma b;
    b.dizi("baslangic.konum", f->baslangic.konum, bk.konum);
    b.dizi("baslangic.yonelim", f->baslangic.yonelim, bk.yonelim);
    b.dizi("baslangic.hiz", f->baslangic.hiz, bk.hiz);
    b.tam("baslangic.yuklu_tupler.uzunluk", f->baslangic.yuklu_tupler.size(), bk.yuklu_tupler.size());
    b.yazdir("fizik başlangıç (ornek-havuz ↔ fizik::BaslangicDurumu{})");
    std::printf("  (yukarıdaki farklar raporlanır, eşitlenmez; karar mimarındır)\n");
    return d.hata;
}

}  // namespace ida::senaryo::test
