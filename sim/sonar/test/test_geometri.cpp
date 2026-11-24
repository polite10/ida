// Kabul 1: geometri (Ping1D düz taban; Ping360 küre ve duvar; yönelim ve montaj).
#include "test_destek.hpp"

namespace ida::sonar::test {

namespace {

AkustikMalzeme taban_malzemesi() { return {"havuz_beton", 0.02, GelisYasasi::cos1, 0.6, 0.7}; }

// Havuz (25 × 12,5 × 3 m) ve içinde kürenin hedefi (A7 hedef_boyali_metal, tahmin değerleri).
std::shared_ptr<CozumselSahne> havuz_ve_kure(const Vek3& merkez, double yaricap)
{
    auto s = havuz_sahnesi(3.0);
    const std::uint16_t m = s->malzeme_ekle({"hedef_boyali_metal", 0.05, GelisYasasi::lambert_cos2, 0.9, 0.3});
    Govde g;
    g.kimlik = 20;
    g.sekil.tur = SekilTuru::kure;
    g.sekil.yaricap = yaricap;
    g.konum = merkez;
    g.malzemeler = {m};
    s->govde_ekle(g);
    return s;
}

// Sektör taraması; açı başına [r0, r1] menzil aralığındaki ham enerji. Tepe açısı ve o açının
// en büyük kutusu döner.
struct Tepe {
    int aci = -1;
    std::size_t kutu = 0;
    double enerji = 0.0;
};

Tepe tara(const SonarKumesiAyari& a, std::shared_ptr<const Sahne> s, const Yorunge& y, double c_beklenen,
          std::uint16_t sp, double r0, double r1, std::uint64_t adim_sayisi)
{
    Kosu k;
    if (!k.kur(a, std::move(s)) || !k.kos(y, 0, adim_sayisi)) std::abort();
    Tepe t;
    for (const auto& o : k.kayit) {
        const auto& p = p360(o);
        double e = 0.0;
        std::size_t kmax = 0;
        double emax = -1.0;
        for (std::size_t i = 0; i < p.tani.ham.size(); ++i) {
            const double r = kutu_menzili(c_beklenen, sp, i);
            if (r < r0 || r > r1) continue;
            e += p.tani.ham[i];
            if (p.tani.ham[i] > emax) {
                emax = p.tani.ham[i];
                kmax = i;
            }
        }
        if (e > t.enerji) {
            t.enerji = e;
            t.aci = p.angle_grad;
            t.kutu = kmax;
        }
    }
    return t;
}

// 7 m menzil: n = 1200, sp = 311 (05 §2.4.5); bir ping ≈ 10 ms (8 adım).
constexpr std::uint16_t kSp7 = 311;

}  // namespace

int test_ping1d_duz_taban()
{
    Denetci d;
    // Düz taban (açık su; z = D) ve aşağı bakan Ping1D. Beklenen: mesafe irtifadır (ışın
    // konisi ana lobunun içinde dik doğrultu var: eğim ≤ 10° < 12,5° yarı genişlik).
    // Tolerans: 3 kutu (kutu = pencere / 200; algılama 5 kutuluk yumuşatma, darbe 1 kutu).
    // Ayrıca mesafe = c · tx_offset (yansıma anı t_tx + t/2, mesafe c · t / 2).
    const double derinlik = 20.0;
    for (double h : {1.0, 2.5, 6.0}) {
        for (double egim : {0.0, 5.0, 10.0}) {
            auto s = std::make_shared<CozumselSahne>();
            acik_su_ekle(*s, derinlik, taban_malzemesi());
            Ping1dYapilandirmasi w;
            w.scan_start = 0.0;
            w.scan_length = 2.0 * h + 1.0;
            w.otomatik = false;
            auto a = kume_ayari({ping1d(w)});
            Kosu k;
            if (!k.kur(a, s)) return 1;
            const Yorunge y = durgun({0.0, 0.0, derinlik - h}, euler_q(0.0, egim * kDerece, 0.0));
            if (!k.kos(y, 0, 400)) return 1;
            const auto v = k.sonar(6);
            DOGRULA(d, v.size() >= 5, "Ping1D kayıtları");
            const double kutu = w.scan_length / 200.0;
            const double c = k.kume.ses_hizi();
            for (const auto& o : v) {
                const auto& p = p1d(o);
                DOGRULA(d, (o.hdr.flags & sensor::kBayrakGecerli) != 0, "geçerli");
                YAKIN(d, p.distance, h, 3.0 * kutu, "Ping1D mesafe = irtifa");
                YAKIN(d, p.distance, c * static_cast<double>(p.tx_offset_ns) * 1e-9, 1e-5, "mesafe = c · t / 2");
                YAKIN(d, static_cast<double>(o.hdr.t_meas_ns - o.hdr.t_tx_ns), static_cast<double>(p.tx_offset_ns), 0.0,
                      "t_meas = t_tx + tx_offset (A1-K54)");
                DOGRULA(d, p.confidence > 0, "güven > 0");
            }
        }
    }
    // Otomatik menzil: pencere son mesafenin 1,5 katına çekilir (05 §2.4.6).
    {
        auto s = std::make_shared<CozumselSahne>();
        acik_su_ekle(*s, derinlik, taban_malzemesi());
        Ping1dYapilandirmasi w;
        w.scan_length = 5.0;
        auto a = kume_ayari({ping1d(w)});
        Kosu k;
        if (!k.kur(a, s) || !k.kos(durgun({0.0, 0.0, derinlik - 3.0}), 0, 800)) return 1;
        const auto v = k.sonar(6);
        const auto& son = p1d(v.back());
        YAKIN(d, son.scan_length, 1.5 * 3.0, 1.5 * 3.0 * 0.05, "otomatik pencere 1,5 · mesafe");
        YAKIN(d, son.distance, 3.0, 0.1, "otomatik pencerede mesafe");
    }
    // Eko yoksa (taban pencerenin dışında): VALID = 0, güven 0, t_meas = t_tx (05 §2.4.6).
    {
        auto s = std::make_shared<CozumselSahne>();
        acik_su_ekle(*s, derinlik, taban_malzemesi());
        Ping1dYapilandirmasi w;
        w.scan_length = 4.0;
        w.otomatik = false;
        auto a = kume_ayari({ping1d(w)});
        Kosu k;
        if (!k.kur(a, s) || !k.kos(durgun({0.0, 0.0, 5.0}), 0, 200)) return 1;
        for (const auto& o : k.sonar(6)) {
            DOGRULA(d, (o.hdr.flags & sensor::kBayrakGecerli) == 0, "eko yok: VALID = 0");
            DOGRULA(d, p1d(o).confidence == 0 && o.hdr.t_meas_ns == o.hdr.t_tx_ns, "eko yok: güven 0, t_meas = t_tx");
        }
    }
    return d.hata;
}

int test_ping360_kure_duvar()
{
    Denetci d;
    // Havuz (25 × 12,5 × 3 m), araç (5, 0, 1) m, yön kuzey. Küre (r = 0,3 m) aracın 50 gradyan
    // (45°) sağında 4 m'de: yüzeyi 3,7 m'de. Tolerans: açı ±2 gradyan (yatay −3 dB genişliği 2°
    // = 2,2 gradyan), menzil ±2 kutu (5,8 mm kutu; darbe 4 kutu, sıfır fazlı).
    const Vek3 arac{5.0, 0.0, 1.0};
    const double aci = 45.0 * kDerece;
    const Vek3 kure{arac[0] + 4.0 * std::cos(aci), arac[1] + 4.0 * std::sin(aci), 1.0};
    auto a = kume_ayari({ping360(p360_yap(kSp7, 1200, 30, 70, 1))});
    const double c = 1482.36;
    const Tepe t = tara(a, havuz_ve_kure(kure, 0.3), durgun(arac), c, kSp7, 3.0, 4.2, 8 * 45);
    std::printf("  küre: açı %d grad, tepe %.3f m\n", t.aci, kutu_menzili(c, kSp7, t.kutu));
    YAKIN(d, t.aci, 50, 2, "küre açısı");
    YAKIN(d, kutu_menzili(c, kSp7, t.kutu), 3.7, 0.02, "küre menzili");
    // Duvar: y = +6,25 m (sancak), 100 gradyanda dik; tepe 6,25 m.
    auto a2 = kume_ayari({ping360(p360_yap(kSp7, 1200, 100, 100, 1))});
    Kosu k;
    if (!k.kur(a2, havuz_sahnesi(3.0)) || !k.kos(durgun(arac), 0, 20)) return 1;
    const auto v = k.sonar(7);
    DOGRULA(d, !v.empty(), "duvar pingi");
    const auto& p = p360(v.front());
    DOGRULA(d, p.angle_grad == 100, "100 gradyan");
    const std::size_t kk = en_buyuk(p.tani.ham, 600);
    std::printf("  duvar: tepe %.3f m\n", kutu_menzili(c, kSp7, kk));
    YAKIN(d, kutu_menzili(c, kSp7, kk), 6.25, 0.02, "duvar menzili");
    // u8 eşlemesi: duvar tepesi boş sudan parlak.
    DOGRULA(d, p.data[kk] > p.data[300] + 50, "duvar u8 boş sudan parlak");
    return d.hata;
}

int test_ping360_yonelim_montaj()
{
    Denetci d;
    // Aynı küre; araç 30° (33,3 gradyan) sağa döner → küre 16,7 gradyanda; montaj −45°
    // (−50 gradyan) → küre 100 gradyanda; araç küreye doğru 1 m ilerlerse menzil 1 m azalır.
    const Vek3 arac{5.0, 0.0, 1.0};
    const double aci = 45.0 * kDerece;
    const Vek3 kure{arac[0] + 4.0 * std::cos(aci), arac[1] + 4.0 * std::sin(aci), 1.0};
    const double c = 1482.36;
    {
        auto a = kume_ayari({ping360(p360_yap(kSp7, 1200, 0, 40, 1))});
        const Tepe t = tara(a, havuz_ve_kure(kure, 0.3), durgun(arac, euler_q(0, 0, 30.0 * kDerece)), c, kSp7, 3.0,
                            4.2, 8 * 45);
        std::printf("  yönelim 30°: açı %d grad\n", t.aci);
        YAKIN(d, t.aci, 16.7, 2.0, "yönelimle kayma");
        YAKIN(d, kutu_menzili(c, kSp7, t.kutu), 3.7, 0.02, "menzil değişmez");
    }
    {
        auto s = ping360(p360_yap(kSp7, 1200, 80, 120, 1));
        s.montaj.yonelim = euler_q(0, 0, -45.0 * kDerece);
        auto a = kume_ayari({s});
        const Tepe t = tara(a, havuz_ve_kure(kure, 0.3), durgun(arac), c, kSp7, 3.0, 4.2, 8 * 45);
        std::printf("  montaj −45°: açı %d grad\n", t.aci);
        YAKIN(d, t.aci, 100, 2, "montajla kayma");
    }
    {
        auto a = kume_ayari({ping360(p360_yap(kSp7, 1200, 30, 70, 1))});
        const Vek3 ileri{arac[0] + std::cos(aci), arac[1] + std::sin(aci), 1.0};
        const Tepe t = tara(a, havuz_ve_kure(kure, 0.3), durgun(ileri), c, kSp7, 2.0, 3.2, 8 * 45);
        std::printf("  1 m ileri: açı %d grad, tepe %.3f m\n", t.aci, kutu_menzili(c, kSp7, t.kutu));
        YAKIN(d, t.aci, 50, 2, "ilerlemede açı");
        YAKIN(d, kutu_menzili(c, kSp7, t.kutu), 2.7, 0.02, "ilerlemede menzil");
    }
    {
        // Montaj konumu: sonar gövdede 0,5 m ileride (x) → küreye uzaklık 0,5 cos 45° azalır.
        auto s = ping360(p360_yap(kSp7, 1200, 30, 70, 1));
        s.montaj.konum = {0.5, 0.0, 0.0};
        auto a = kume_ayari({s});
        const Tepe t = tara(a, havuz_ve_kure(kure, 0.3), durgun(arac), c, kSp7, 2.8, 4.2, 8 * 45);
        const Vek3 rel{kure[0] - arac[0] - 0.5, kure[1] - arac[1], 0.0};
        const double beklenen = std::sqrt(rel[0] * rel[0] + rel[1] * rel[1]) - 0.3;
        std::printf("  montaj konumu: tepe %.3f m (beklenen %.3f)\n", kutu_menzili(c, kSp7, t.kutu), beklenen);
        YAKIN(d, kutu_menzili(c, kSp7, t.kutu), beklenen, 0.02, "montaj konumuyla menzil");
    }
    return d.hata;
}

}  // namespace ida::sonar::test
