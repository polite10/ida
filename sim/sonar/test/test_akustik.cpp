// Kabul 3: akustik büyüklükler (ses hızı, soğurma, menzille azalma, malzeme).
#include "test_destek.hpp"

#include "ida/sonar/akustik.hpp"

namespace ida::sonar::test {

namespace {

// Tek düzlem duvar: x = d (NED), normal −x; malzeme `m`.
std::shared_ptr<CozumselSahne> duvar(double d, const AkustikMalzeme& m)
{
    auto s = std::make_shared<CozumselSahne>();
    const std::uint16_t mi = s->malzeme_ekle(m);
    Govde g;
    g.kimlik = 10;
    g.sekil.tur = SekilTuru::duzlem;
    g.konum = {d, 0.0, 0.0};
    g.yonelim = {std::cos(ic::kPi / 4), 0.0, std::sin(ic::kPi / 4), 0.0};   // yerel z → NED x
    g.malzemeler = {mi};
    s->govde_ekle(g);
    return s;
}

// Tek pingin kaydı (t = 0'da gönderilir).
SonarOlcumu ilk_ping(const SonarKumesiAyari& a, std::shared_ptr<const Sahne> s, const Yorunge& y)
{
    Kosu k;
    if (!k.kur(a, std::move(s)) || !k.kos(y, 0, 200)) std::abort();
    for (const auto& o : k.kayit) {
        if (o.hdr.seq == 0) return o;
    }
    std::abort();
}

AkustikMalzeme beton(double mu = 0.02, GelisYasasi y = GelisYasasi::lambert_cos2)
{
    return {"beton", mu, y, 0.6, 0.7};
}

}  // namespace

int test_ses_hizi()
{
    Denetci d;
    // Yayımlanmış denetim değerleri (kaynaklar sim/sonar/BENIOKU.md):
    //  - UNESCO 1983 (Fofonoff ve Millard 1983, SVEL): S = 40, T68 = 40 °C, P = 1000 bar → 1731,995.
    //  - Mackenzie (MBARI Sndspd.txt): S = 40, T = 40 °C, D = 10 000 m → 1718,774.
    //  - Mackenzie (1981) makale denetim değeri: T = 25 °C, S = 35, D = 1000 m → 1550,744.
    // Tolerans: değerler üç ondalıkla yayımlanmıştır (yarım birim).
    constexpr double kTol = 0.0005;
    YAKIN(d, ses_hizi_unesco_t68(40.0, 40.0, 1000.0), 1731.995, kTol, "UNESCO 1983 denetim değeri");
    YAKIN(d, ses_hizi_mackenzie(40.0, 40.0, 10000.0), 1718.774, kTol, "Mackenzie MBARI denetim değeri");
    YAKIN(d, ses_hizi_mackenzie(25.0, 35.0, 1000.0), 1550.744, kTol, "Mackenzie 1981 denetim değeri");
    // ITS-90 → IPTS-68 dönüşümü.
    YAKIN(d, ses_hizi_chen_millero(20.0, 0.0, 0.0), ses_hizi_unesco_t68(20.004800000000003, 0.0, 0.0), 1e-9,
          "T68 = 1,00024 T90");
    // Saf su 20 °C, yüzey: UNESCO denklemi ≈ 1482,3 m/s (akıl denetimi; Del Grosso–Mader saf su
    // değeri 1482,34 m/s ile 0,1 m/s içinde beklenir; bu değer bu işte bağımsız doğrulanmadı).
    YAKIN(d, ses_hizi_chen_millero(20.0, 0.0, 0.0), 1482.34, 0.1, "saf su 20 °C");
    // Seçim (W4-K1): sabit > tatlı su (Chen–Millero) > deniz suyu (Mackenzie, S ≥ 25 ‰).
    const SesHizi a = ortam_ses_hizi(20.0, 0.0, std::nullopt);
    DOGRULA(d, a.kaynak == SesHiziKaynagi::chen_millero, "tatlı suda Chen–Millero");
    YAKIN(d, a.c, ses_hizi_chen_millero(20.0, 0.0, 0.0), 1e-12, "tatlı su yüzey değeri");
    const SesHizi b = ortam_ses_hizi(15.0, 0.035, std::nullopt, 10.0, 1025.0, 9.82);
    DOGRULA(d, b.kaynak == SesHiziKaynagi::mackenzie, "deniz suyunda Mackenzie");
    YAKIN(d, b.c, ses_hizi_mackenzie(15.0, 35.0, 10.0), 1e-12, "deniz suyu değeri");
    const SesHizi c = ortam_ses_hizi(20.0, 0.035, 1450.0);
    DOGRULA(d, c.kaynak == SesHiziKaynagi::sabit && c.c == 1450.0, "A5 sabit c geçersiz kılar");
    // Basınç: tatlı suda 10 m derinlik ≈ 0,98 bar → ≈ +0,16 m/s.
    const double fark = ortam_ses_hizi(20.0, 0.0, std::nullopt, 10.0, 1000.0, 9.82).c - a.c;
    DOGRULA(d, fark > 0.1 && fark < 0.25, "basınçla artış");
    return d.hata;
}

int test_sogurma()
{
    Denetci d;
    // 05 §2.4.2'nin aritmetiği (tatlı su, 20 °C, pH 8, yüzey): 750 kHz 0,1314 dB/m; 115 kHz
    // 0,00309 dB/m. Formülün kendisi bağımsız doğrulanmadı (05 §2.4.2; sim/sonar/BENIOKU.md).
    YAKIN(d, sogurma_db_m(750e3, 20.0, 0.0, 0.0, 8.0), 0.1314, 5e-5, "750 kHz tatlı su");
    YAKIN(d, sogurma_db_m(115e3, 20.0, 0.0, 0.0, 8.0), 0.00309, 5e-6, "115 kHz tatlı su");
    DOGRULA(d, sogurma_db_m(115e3, 20.0, 0.035, 0.0, 8.0) > sogurma_db_m(115e3, 20.0, 0.0, 0.0, 8.0),
            "deniz suyunda MgSO4 terimi soğurmayı artırır");
    DOGRULA(d, sogurma_db_m(750e3, 20.0, 0.0, 0.0, 7.0) == sogurma_db_m(750e3, 20.0, 0.0, 0.0, 8.5),
            "tatlı suda pH etkisiz (yalnız saf su terimi)");
    DOGRULA(d, sogurma_db_m(500e3, 20.0, 0.0, 0.0, 8.0) < sogurma_db_m(1000e3, 20.0, 0.0, 0.0, 8.0),
            "frekansla artar");
    return d.hata;
}

int test_menzil_yasasi()
{
    Denetci d;
    // Dik duvar ışın demetini doldurduğunda her ışının enerjisi w ∝ 1/r² · 10^(−2αr/10)
    // (05 §2.4.2: alan dA ∝ r²). Aynı ışın kümesiyle d → 2d'de toplam enerji tam ¼'üne iner
    // (soğurmasız); kalan fark yalnız kayan nokta yuvarlamasıdır.
    auto a = kume_ayari({ping360(p360_yap(311))});
    std::get<Ping360Ayar>(a.sonarlar[0].ayar).sogurma = 0.0;
    const Yorunge y = durgun({0.0, 0.0, 0.0});
    const double e2 = toplam(p360(ilk_ping(a, duvar(2.0, beton()), y)).tani.ham);
    const double e4 = toplam(p360(ilk_ping(a, duvar(4.0, beton()), y)).tani.ham);
    DOGRULA(d, e2 > 0.0, "duvar dönüşü var");
    YAKIN(d, e4 / e2, 0.25, 1e-9, "E(2d)/E(d) = 1/4 (soğurmasız)");
    // Soğurma: her ışın 10^(−2αr/10) ile çarpılır; r ∈ [d, d / cos φ_maks].
    constexpr double kAlfa = 0.1314;
    std::get<Ping360Ayar>(a.sonarlar[0].ayar).sogurma = kAlfa;
    const double e4a = toplam(p360(ilk_ping(a, duvar(4.0, beton()), y)).tani.ham);
    const double fi_maks = 12.5 / 0.44295 * kDerece;
    const double ust = std::pow(10.0, -2.0 * kAlfa * 4.0 / 10.0);
    const double alt = std::pow(10.0, -2.0 * kAlfa * 4.0 / std::cos(fi_maks) / 10.0);
    DOGRULA(d, e4a / e4 <= ust + 1e-12 && e4a / e4 >= alt - 1e-12, "soğurma çarpanı aralıkta");
    // Menzil profili: duvarın tepe kutusu d'de (kutu çözünürlüğü ve darbe yarım genişliği içinde).
    const auto o = p360(ilk_ping(a, duvar(4.0, beton()), y));
    const std::size_t k = en_buyuk(o.tani.ham);
    YAKIN(d, kutu_menzili(1482.34, 311, k), 4.0, 0.03, "duvar tepesi 4 m");
    return d.hata;
}

int test_malzeme()
{
    Denetci d;
    const Yorunge y = durgun({0.0, 0.0, 0.0});
    auto a = kume_ayari({ping360(p360_yap(311))});
    // Geri saçılım katsayısı μ enerjiyle doğrusal (σ_bs = μ f(θ)).
    const double e1 = toplam(p360(ilk_ping(a, duvar(3.0, beton(0.02)), y)).tani.ham);
    const double e2 = toplam(p360(ilk_ping(a, duvar(3.0, beton(0.04)), y)).tani.ham);
    YAKIN(d, e2 / e1, 2.0, 1e-9, "μ iki katı → enerji iki katı");
    // Geliş yasası: aynı geometride cos1 / lambert_cos2 = 1 / cos θ_geliş (ışın başına). Araç
    // duvar normaline 60° döndürülür; düşey açıklık θ_geliş'i büyütür: oran [1/cos 58°,
    // 1/(cos 28,2° · cos 62°)] aralığında.
    const Yorunge y60 = durgun({0.0, 0.0, 0.0}, euler_q(0.0, 0.0, 60.0 * kDerece));
    const double el = toplam(p360(ilk_ping(a, duvar(3.0, beton(0.02, GelisYasasi::lambert_cos2)), y60)).tani.ham);
    const double ec = toplam(p360(ilk_ping(a, duvar(3.0, beton(0.02, GelisYasasi::cos1)), y60)).tani.ham);
    const double oran = ec / el;
    std::printf("  cos1 / lambert_cos2 (60°) = %.4f\n", oran);
    DOGRULA(d, el > 0.0 && oran > 1.0 / std::cos(58.0 * kDerece) &&
                   oran < 1.0 / (std::cos(28.3 * kDerece) * std::cos(62.0 * kDerece)),
            "geliş yasası oranı 1/cos θ civarında");
    // Dik gelişte iki yasa yaklaşık aynı (θ ≈ 0; düşey açıklık farkı küçük).
    const double e0l = toplam(p360(ilk_ping(a, duvar(3.0, beton(0.02, GelisYasasi::lambert_cos2)), y)).tani.ham);
    const double e0c = toplam(p360(ilk_ping(a, duvar(3.0, beton(0.02, GelisYasasi::cos1)), y)).tani.ham);
    DOGRULA(d, e0c / e0l > 1.0 && e0c / e0l < 1.15, "dik gelişte iki yasa yakın");
    return d.hata;
}

}  // namespace ida::sonar::test
