// sol incelemesinin karşı örnekleri (docs/gorevler/inceleme/W4-sonar-sol.md, #1–#8).
#include "test_destek.hpp"

#include "demet.hpp"
#include "ida/sonar/akustik.hpp"

namespace ida::sonar::test {

namespace {

std::shared_ptr<CozumselSahne> acik_su(double derinlik)
{
    auto s = std::make_shared<CozumselSahne>();
    acik_su_ekle(*s, derinlik, {"taban", 0.02, GelisYasasi::cos1, 0.6, 0.7});
    return s;
}

bool ham_sifir(const std::vector<double>& v)
{
    return std::all_of(v.begin(), v.end(), [](double x) { return x == 0.0; });
}

std::vector<SonarOlcumu> kos(const SonarKumesiAyari& a, std::shared_ptr<const Sahne> s, const Yorunge& y,
                             std::uint64_t adim)
{
    Kosu k;
    if (!k.kur(a, std::move(s)) || !k.kos(y, 0, adim)) std::abort();
    return k.kayit;
}

}  // namespace

// #1: dönüştürücü havadayken (su yüzeyinin üstünde; NED z ≤ 0) geometrik eko yok: Ping1D güven 0
// ve VALID = 0, Ping360 yalnız gürültü tabanı (05 §4).
int test_sol1_havada()
{
    Denetci d;
    auto s = acik_su(10.0);
    // Ping1D (aşağı bakar) suyun 0,36 m üstünde: düz su yüzeyi saçıcı sayılmamalı.
    {
        Ping1dYapilandirmasi w;
        w.otomatik = false;
        auto a = kume_ayari({ping1d(w)});
        a.sade_isin_sayisi = 500;
        a.sonarlar[0].montaj.konum = {0.0, 0.0, 0.14};
        const auto v = kos(a, s, durgun({0.0, 0.0, -0.5}), 200);
        DOGRULA(d, !v.empty(), "Ping1D kayıtları");
        for (const auto& o : v) {
            DOGRULA(d, (o.hdr.flags & sensor::kBayrakGecerli) == 0 && p1d(o).confidence == 0,
                    "havada Ping1D: VALID 0, güven 0");
            DOGRULA(d, ham_sifir(p1d(o).tani.ham), "havada Ping1D: geometrik eko yok");
        }
    }
    // Ping360 havada: gürültü tabanı açıkken yalnız gürültü; ham enerji sıfır.
    {
        auto p = ping360(p360_yap(311));
        std::get<Ping360Ayar>(p.ayar).gurultu_tabani = 1e-12;
        auto a = kume_ayari({p});
        a.sade_isin_sayisi = 500;
        const auto v = kos(a, s, durgun({0.0, 0.0, -0.3}), 40);
        DOGRULA(d, !v.empty(), "Ping360 kayıtları");
        for (const auto& o : v) {
            DOGRULA(d, ham_sifir(p360(o).tani.ham), "havada Ping360: ham enerji sıfır");
            DOGRULA(d, !ham_sifir(p360(o).tani.yogunluk), "havada Ping360: gürültü tabanı var");
        }
    }
    // Eğimli montaj: gövde suda (z = 0,1), sonar burunda 0,5 m ileride; 30° baş yukarı → sonar
    // z = 0,1 − 0,5 sin 30° = −0,15 (havada). Aynı konum eğimsizken suda ve eko üretir.
    {
        auto p = ping360(p360_yap(311, 1200, 0, 0, 1));
        p.montaj.konum = {0.5, 0.0, 0.0};
        auto a = kume_ayari({p});
        a.sade_isin_sayisi = 500;
        const auto egik = kos(a, s, durgun({0.0, 0.0, 0.1}, euler_q(0.0, 30.0 * kDerece, 0.0)), 40);
        for (const auto& o : egik) DOGRULA(d, ham_sifir(p360(o).tani.ham), "eğik montaj havada: eko yok");
        const auto duz = kos(a, s, durgun({0.0, 0.0, 0.1}), 40);
        bool var = false;
        for (const auto& o : duz) var = var || !ham_sifir(p360(o).tani.ham);
        DOGRULA(d, var, "eğimsiz montaj suda: eko var");
    }
    // Tam yüzey sınırı: dönüştürücü z = 0 havada sayılır (batmış olmalı); z = 0,001 suda.
    {
        Ping1dYapilandirmasi w;
        w.otomatik = false;
        auto a = kume_ayari({ping1d(w)});
        a.sade_isin_sayisi = 500;
        const auto sinir = kos(a, acik_su(3.0), durgun({0.0, 0.0, 0.0}), 100);
        for (const auto& o : sinir) DOGRULA(d, ham_sifir(p1d(o).tani.ham), "z = 0: havada");
        const auto ici = kos(a, acik_su(3.0), durgun({0.0, 0.0, 0.001}), 100);
        for (const auto& o : ici) {
            DOGRULA(d, !ham_sifir(p1d(o).tani.ham) && (o.hdr.flags & sensor::kBayrakGecerli) != 0,
                    "z = 0,001: suda, taban görülür");
        }
    }
    return d.hata;
}

// #2: geçerli bir Ping1D penceresi otomatik menzilde [0, 100] m dışına taşmaz.
int test_sol2_ping1d_pencere()
{
    Denetci d;
    Ping1dYapilandirmasi w;
    w.scan_start = 99.8;
    w.scan_length = 0.1;
    w.otomatik = true;
    auto a = kume_ayari({ping1d(w)});
    a.sade_isin_sayisi = 200;
    const auto v = kos(a, acik_su(300.0), durgun({0.0, 0.0, 1.0}), 1600);
    DOGRULA(d, v.size() > 5, "kayıtlar");
    for (const auto& o : v) {
        const auto& p = p1d(o);
        DOGRULA(d, p.scan_length > 0.0f && p.scan_start + p.scan_length <= 100.0f + 1e-4f,
                "pencere ≤ 100 m: " + std::to_string(p.scan_start) + " + " + std::to_string(p.scan_length));
    }
    return d.hata;
}

// #3: yeniden yapılandırmada frekans değişirse soğurma yeni frekanstan; doğrudan o frekansla
// kurulmuş kümeyle aynı satırlar.
int test_sol3_soguma_frekans()
{
    Denetci d;
    auto sahne = havuz_sahnesi();
    auto y750 = p360_yap(2222, 1200, 0, 0, 200);
    auto y1000 = y750;
    y1000.transmit_frequency_khz = 1000;
    // Yeniden yapılandırma kafayı yeni başlangıca götürür: 200'den başlatılırsa açı dizisi
    // doğrudan kurulan kümeninkiyle (0, 200, 0, …) aynı kalır.
    auto y1000b = y1000;
    y1000b.baslangic_grad = y1000b.bitis_grad = 200;
    auto a = kume_ayari({ping360(y750)});
    a.sade_isin_sayisi = 500;
    Kosu k;
    k.adim_oncesi = [&](std::uint64_t n, SonarKumesi& km) {
        if (n == 10) DOGRULA(d, km.ping360_yapilandir("p360", y1000b), "yapılandırma");
    };
    if (!k.kur(a, sahne) || !k.kos(durgun({5.0, 0.0, 1.0}), 0, 600)) return 1;
    auto b = kume_ayari({ping360(y1000)});
    b.sade_isin_sayisi = 500;
    Kosu dk;
    if (!dk.kur(b, sahne) || !dk.kos(durgun({5.0, 0.0, 1.0}), 0, 600)) return 1;
    const auto va = k.sonar(7), vb = dk.sonar(7);
    int karsilastirilan = 0;
    for (std::size_t i = 1; i < va.size() && i < vb.size(); ++i) {
        const auto& x = p360(va[i]);
        const auto& z = p360(vb[i]);
        if (x.tx_freq_khz != 1000 || x.angle_grad != z.angle_grad || va[i].hdr.t_tx_ns != vb[i].hdr.t_tx_ns) continue;
        ++karsilastirilan;
        DOGRULA(d, x.tani.ham == z.tani.ham && x.data == z.data, "yeniden yapılandırma = doğrudan kurulum");
    }
    DOGRULA(d, karsilastirilan >= 2, "karşılaştırılan satır var");
    // Açık sabit soğurma korunur.
    auto c = kume_ayari({ping360(y750)});
    std::get<Ping360Ayar>(c.sonarlar[0].ayar).sogurma = 0.05;
    c.sade_isin_sayisi = 500;
    auto e = c;
    std::get<Ping360Yapilandirmasi>(e.sonarlar[0].yapilandirma) = y1000;
    Kosu kc;
    kc.adim_oncesi = [&](std::uint64_t n, SonarKumesi& km) {
        if (n == 10) km.ping360_yapilandir("p360", y1000b);
    };
    Kosu ke;
    if (!kc.kur(c, sahne) || !kc.kos(durgun({5.0, 0.0, 1.0}), 0, 600)) return 1;
    if (!ke.kur(e, sahne) || !ke.kos(durgun({5.0, 0.0, 1.0}), 0, 600)) return 1;
    const auto vc = kc.sonar(7), ve = ke.sonar(7);
    int sabit_sayisi = 0;
    for (std::size_t i = 1; i < vc.size() && i < ve.size(); ++i) {
        if (p360(vc[i]).angle_grad != p360(ve[i]).angle_grad || vc[i].hdr.t_tx_ns != ve[i].hdr.t_tx_ns) continue;
        ++sabit_sayisi;
        DOGRULA(d, p360(vc[i]).tani.ham == p360(ve[i]).tani.ham, "sabit soğurma korunur");
    }
    DOGRULA(d, sabit_sayisi >= 2, "sabit soğurmada karşılaştırma var");
    return d.hata;
}

// #4: süren ping sırasında başlangıç açısı değişirse kafa son gönderim açısından yeni başlangıca
// döner: 0 gradyanda ping, adım 200; başlangıç 200'e alınır; t_grad = 1 ms → 200 ms motor.
int test_sol4_motor()
{
    Denetci d;
    auto p = ping360(p360_yap(311, 1200, 0, 0, 200));
    std::get<Ping360Ayar>(p.ayar).t_grad_ns = 1'000'000;
    auto a = kume_ayari({p});
    a.sade_isin_sayisi = 200;
    Kosu k;
    k.adim_oncesi = [&](std::uint64_t n, SonarKumesi& km) {
        if (n == 2) DOGRULA(d, km.ping360_yapilandir("p360", p360_yap(311, 1200, 200, 200, 200)), "yapılandırma");
    };
    if (!k.kur(a, havuz_sahnesi()) || !k.kos(durgun({5.0, 0.0, 1.0}), 0, 400)) return 1;
    const auto v = k.sonar(7);
    DOGRULA(d, v.size() >= 2, "iki satır");
    if (v.size() < 2) return d.hata;
    DOGRULA(d, p360(v[0]).angle_grad == 0 && p360(v[1]).angle_grad == 200, "açılar 0, 200");
    const std::int64_t beklenen = 1200LL * 311 * 25 + 200'000'000;   // dinleme + 200 gradyan motor
    DOGRULA(d, std::abs(v[1].hdr.t_tx_ns - beklenen) <= kAdim / 2,
            "ikinci ping dinleme + 200 ms motor sonra: " + std::to_string(v[1].hdr.t_tx_ns));
    return d.hata;
}

// #5: pencerenin hemen dışındaki eko, darbe çekirdeğinin taşan kısmıyla son kutulara düşer.
int test_sol5_pencere_kenari()
{
    Denetci d;
    // n = 200, sp = 311: pencere ucu c · 200 · 7,775 µs / 2 ≈ 1,1525 m; darbe 4 kutu (≈ 2,3 cm).
    // Dik duvar pencere ucunun 1 cm ötesinde.
    auto a = kume_ayari({ping360(p360_yap(311, 200, 0, 0, 1))});
    std::get<Ping360Ayar>(a.sonarlar[0].ayar).sogurma = 0.0;
    SonarKumesi km;
    km.kur(a, havuz_sahnesi());
    const double uc = 0.5 * km.ses_hizi() * 200.0 * 311.0 * 25e-9;
    auto s = std::make_shared<CozumselSahne>();
    const std::uint16_t m = s->malzeme_ekle({"duvar", 0.02, GelisYasasi::lambert_cos2, 0.5, 0.5});
    Govde g;
    g.kimlik = 1;
    g.sekil.tur = SekilTuru::duzlem;
    g.konum = {uc + 0.01, 0.0, 0.0};
    g.yonelim = {std::cos(ic::kPi / 4), 0.0, std::sin(ic::kPi / 4), 0.0};
    g.malzemeler = {m};
    s->govde_ekle(g);
    const auto v = kos(a, s, durgun({0.0, 0.0, 1.0}), 10);
    DOGRULA(d, !v.empty(), "satır");
    if (v.empty()) return d.hata;
    const auto& h = p360(v.front()).tani.ham;
    DOGRULA(d, h.back() > 0.0, "son kutuya çekirdek taşması düşer");
    DOGRULA(d, h[h.size() - 6] == 0.0, "çekirdek desteğinin ötesi boş");
    return d.hata;
}

// #7: açık darbe süresi cihaz aralığında (ping-protocol Ping360 `transducer`: 1–1000 µs; 0 otomatik).
int test_sol7_darbe_suresi()
{
    Denetci d;
    auto y = p360_yap(311);
    for (std::uint16_t tx : {std::uint16_t{0}, std::uint16_t{1}, std::uint16_t{1000}}) {
        y.transmit_duration_us = tx;
        SonarKumesi k;
        DOGRULA(d, k.kur(kume_ayari({ping360(y)}), havuz_sahnesi()).tamam, "geçerli darbe " + std::to_string(tx));
    }
    for (std::uint16_t tx : {std::uint16_t{1001}, std::uint16_t{65535}}) {
        y.transmit_duration_us = tx;
        SonarKumesi k;
        DOGRULA(d, !k.kur(kume_ayari({ping360(y)}), havuz_sahnesi()).tamam, "geçersiz darbe " + std::to_string(tx));
    }
    // Geçersiz yeniden yapılandırma reddedilir, önceki sürer.
    y.transmit_duration_us = 50;
    auto a = kume_ayari({ping360(y)});
    a.sade_isin_sayisi = 200;
    Kosu k;
    k.adim_oncesi = [&](std::uint64_t n, SonarKumesi& km) {
        if (n == 3) {
            auto kotu = y;
            kotu.transmit_duration_us = 2000;
            DOGRULA(d, !km.ping360_yapilandir("p360", kotu), "geçersiz yeniden yapılandırma reddedilir");
        }
    };
    if (!k.kur(a, havuz_sahnesi()) || !k.kos(durgun({5.0, 0.0, 1.0}), 0, 60)) return 1;
    for (const auto& o : k.sonar(7)) DOGRULA(d, p360(o).tx_duration_us == 50, "önceki darbe süresi sürer");
    return d.hata;
}

// #5, birim: incelemenin karşı örneği. n = 1200, sp = 311, darbe 31 µs (yarı genişlik 16 ince
// örnek), birim enerjili eko 9 331,94375 µs'de (ince konum 4800,5; dizi 4800 örnek): son kutuya
// katkı Σ h² / 4 = 790 / 289 / 4 = 0,68339100346. Alt sınırın komşusu: pencere başından bir ince
// örnek önceki eko ilk kutuya aynı katkıyı verir (simetrik).
int test_sol5_cekirdek_birim()
{
    Denetci d;
    const double t_s = 311.0 * 25e-9;
    const double dt = t_s / 4.0;
    ic::BiriktirmeTamponu tampon;
    std::vector<double> ham;
    ic::biriktir({{9331.94375e-6, 1.0}}, 0.0, t_s, 1200, 31e-6, tampon, ham);
    YAKIN(d, ham[1199], 790.0 / 289.0 / 4.0, 1e-9, "üst sınır taşması (0,68339100346)");
    YAKIN(d, ham[1199], 0.68339100346, 1e-9, "inceleme değeri");
    const double tau0 = 1e-3;
    ic::biriktir({{tau0 - dt, 1.0}}, tau0, t_s, 1200, 31e-6, tampon, ham);
    YAKIN(d, ham[0], 790.0 / 289.0 / 4.0, 1e-9, "alt sınır komşusu");
    // Çekirdek desteğinin tamamen dışındaki eko katkı vermez.
    ic::biriktir({{tau0 - 20.0 * dt, 1.0}}, tau0, t_s, 1200, 31e-6, tampon, ham);
    DOGRULA(d, ham_sifir(ham), "destek dışı eko yok");
    // Pencere içindeki ekoda enerji korunur: Σ kutu = Σ h² / 4 (çekirdek desteği içeride).
    ic::biriktir({{tau0 + 300.0 * t_s, 1.0}}, tau0, t_s, 1200, 31e-6, tampon, ham);
    double toplam_h2 = 0.0;
    for (int m = -16; m <= 16; ++m) {
        const double h = 1.0 - std::abs(m) / 17.0;
        toplam_h2 += h * h;
    }
    YAKIN(d, toplam(ham), toplam_h2 / 4.0, 1e-12, "enerji korunumu");
    return d.hata;
}

// #6: saat kayması. Ping1D (aralık 50 ms) +2000 ppm: ideal gönderimler k · 50,1 ms; her gönderim en
// yakın adım sınırında (|fark| ≤ adım/2), birikim yok; 60 s'de kaymasız koşudan 3 ping eksik.
int test_sol6_saat_kaymasi()
{
    Denetci d;
    Ping1dYapilandirmasi w;
    w.scan_length = 4.0;
    w.otomatik = false;
    auto a = kume_ayari({ping1d(w)});
    a.sade_isin_sayisi = 100;
    auto b = a;
    a.sonarlar[0].saat_kaymasi_ppm = 2000.0;
    const std::uint64_t adim = 48000;   // 60 s
    const auto va = kos(a, havuz_sahnesi(), durgun({5.0, 0.0, 1.0}), adim);
    const auto vb = kos(b, havuz_sahnesi(), durgun({5.0, 0.0, 1.0}), adim);
    std::printf("  ping sayısı: +2000 ppm %zu, 0 ppm %zu\n", va.size(), vb.size());
    DOGRULA(d, vb.size() == 1200 && va.size() == 1198, "60 s'de ping sayısı (1200 / 1198)");
    std::int64_t en_buyuk = 0;
    for (const auto& o : va) {
        const std::int64_t ideal = static_cast<std::int64_t>(o.hdr.seq) * 50'100'000;
        en_buyuk = std::max(en_buyuk, std::abs(o.hdr.t_tx_ns - ideal));
        DOGRULA(d, o.hdr.t_tx_ns % kAdim == 0, "gönderim adım sınırında");
    }
    std::printf("  ideal anlardan en büyük sapma %lld ns\n", static_cast<long long>(en_buyuk));
    DOGRULA(d, en_buyuk <= kAdim / 2, "birikimsiz: sapma ≤ adım/2");
    if (va.size() > 1000 && vb.size() > 1000) {
        YAKIN(d, static_cast<double>(va[1000].hdr.t_tx_ns - vb[1000].hdr.t_tx_ns), 1000.0 * 100'000.0,
              static_cast<double>(kAdim), "1000. pingte faz farkı 100 ms");
    }
    return d.hata;
}

// #8: ses hızı formülünün geçerlilik alanı dışı reddedilmez, uyarıyla bildirilir.
int test_sol8_gecerlilik_alani()
{
    Denetci d;
    DOGRULA(d, ortam_ses_hizi(40.0, 0.050, std::nullopt).dislama, "Mackenzie 40 °C / 50 ‰ dışında");
    DOGRULA(d, ortam_ses_hizi(1.0, 0.035, std::nullopt).dislama, "Mackenzie 1 °C dışında");
    DOGRULA(d, !ortam_ses_hizi(20.0, 0.035, std::nullopt).dislama, "Mackenzie 20 °C / 35 ‰ içinde");
    DOGRULA(d, ortam_ses_hizi(-1.0, 0.0, std::nullopt).dislama, "Chen–Millero −1 °C dışında");
    DOGRULA(d, !ortam_ses_hizi(20.0, 0.0, std::nullopt).dislama, "Chen–Millero 20 °C içinde");
    DOGRULA(d, !ortam_ses_hizi(-1.0, 0.0, 1450.0).dislama, "sabit c: uyarı yok");
    auto a = ida0_sonar_kumesi();
    a.ortam.su.sicaklik_c = 35.0;
    a.ortam.su.tuzluluk = 0.035;
    {
        SonarKumesi k;
        const KurulumSonucu r = k.kur(a, havuz_sahnesi());
        DOGRULA(d, r.tamam && r.uyarilar.size() == 1 && r.uyarilar[0].find("dışdeğerleme") != std::string::npos,
                "sonar kurulumu uyarıyla tamam");
    }
    a.ortam.su.sicaklik_c = 20.0;
    {
        SonarKumesi k;
        const KurulumSonucu r = k.kur(a, havuz_sahnesi());
        DOGRULA(d, r.tamam && r.uyarilar.empty(), "alan içinde uyarı yok");
    }
    // Sensör kümesi (W4-K1 yolu) aynı uyarıyı verir.
    sensor::SensorKumesiAyari s = sensor::ida0_sensor_kumesi();
    s.ortam.su.ses_hizi.reset();
    s.ortam.su.sicaklik_c = 35.0;
    s.ortam.su.tuzluluk = 0.035;
    {
        sensor::SensorKumesi k;
        const sensor::KurulumSonucu r = k.kur(s);
        bool var = false;
        for (const auto& u : r.uyarilar) var = var || u.find("dışdeğerleme") != std::string::npos;
        DOGRULA(d, r.tamam && var, "sensör kümesi uyarıyla tamam");
    }
    return d.hata;
}

}  // namespace ida::sonar::test
