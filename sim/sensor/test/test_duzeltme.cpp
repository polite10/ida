// sol incelemesinin karşı örnekleri (docs/gorevler/inceleme/W4-sensor-sol.md) ve mimar
// kararları W4-K4, K5, K7, K8, K9.
#include "test_destek.hpp"

#include "model.hpp"

#include <cfloat>
#include <map>

namespace ida::sensor::test {

namespace {

constexpr std::int64_t kS = 1'000'000'000;

SensorArizasi ariza(const char* kimlik, const char* hedef, ArizaTuru tur, std::int64_t bas,
                    std::int64_t sure)
{
    SensorArizasi f;
    f.kimlik = kimlik;
    f.hedef = hedef;
    f.tur = tur;
    f.zaman_ns = bas;
    f.sure_ns = sure;
    return f;
}

fizik::DurumAnlik yavas(std::uint64_t n)
{
    const double t = sn(n);
    return durum(n, {0, 0, 0.5 + 0.05 * t}, {0, 0, 0.05}, {}, euler_q(0.05, 0.0, 0.1 * t),
                 {0, 0, 0.1});
}

std::vector<Olcum> kos(const std::vector<SensorArizasi>& arizalar, double sure,
                       const SensorKumesiAyari& a, Sayaclar* say = nullptr)
{
    Kosu k;
    if (!k.kur(a)) return {};
    for (const auto& f : arizalar) {
        const ArmSonucu s = k.kume.arm(f);
        if (!s.tamam) {
            std::printf("  arm reddedildi: %s\n", s.hata.c_str());
            return {};
        }
    }
    k.kos(yavas, 0, static_cast<std::uint64_t>(sure / 1.25e-3));
    if (say) *say = k.kume.sayaclar();
    return k.kayit;
}

std::map<std::uint32_t, Olcum> seq_haritasi(const std::vector<Olcum>& v, SensorTuru t)
{
    std::map<std::uint32_t, Olcum> m;
    for (const auto& o : v) {
        if (o.hdr.tur == t) m[o.hdr.seq] = o;
    }
    return m;
}

bool sonlu_yuk(const Olcum& o)
{
    bool s = true;
    std::visit(
        [&](const auto& y) {
            using T = std::decay_t<decltype(y)>;
            if constexpr (std::is_same_v<T, MagOlcumu>) {
                for (float f : y.field) s = s && std::isfinite(f);
            } else if constexpr (std::is_same_v<T, BaroOlcumu>) {
                s = std::isfinite(y.p_abs) && std::isfinite(y.temp_c);
            } else if constexpr (std::is_same_v<T, GnssOlcumu>) {
                s = std::isfinite(y.eph) && std::isfinite(y.epv) && std::abs(y.lat_e7) <= 900000000 &&
                    std::abs(y.lon_e7) <= 1800000000;
                for (float f : y.vel_ned) s = s && std::isfinite(f);
            }
        },
        o.yuk);
    return s;
}

}  // namespace

// #1, W4-K8: DVL `altitude` dibe dik mesafedir. Beklenen değer doğrudan geometriden:
// taban derinliği − dönüştürücünün NED derinliği (düz, yatay taban).
int test_dvl_irtifa_egik()
{
    Denetci d;
    struct Vaka {
        const char* ad;
        Kuaterniyon q;
        Montaj m;
    };
    const double s20 = std::sin(0.1745329), c20 = std::cos(0.1745329);
    const Vaka vakalar[] = {
        {"yalpa 30°", euler_q(0.5235988, 0.0, 0.3), Montaj{{0.05, 0.0, 0.14}, {1, 0, 0, 0}}},
        {"yunuslama 15°, yalpa −10°", euler_q(-0.1745329, 0.2617994, -0.7), Montaj{{0.05, 0.0, 0.14}, {1, 0, 0, 0}}},
        {"kimlik olmayan montaj (y ekseni 20°), l ≠ 0", euler_q(0.1, 0.05, 0.0),
         Montaj{{0.1, -0.08, 0.2}, {c20, 0.0, s20, 0.0}}},
    };
    for (const auto& v : vakalar) {
        SensorKumesiAyari a = ideal_ayar();
        yalniz(a, {SensorTuru::dvl});
        sensor_ayari(a, SensorTuru::dvl).montaj = v.m;
        Kosu k;
        if (!k.kur(a)) return 1;
        const Vek3 p{0.0, 0.0, 1.0};
        if (!k.kos(durgun(p, v.q), 0, 3200)) return 1;
        const Vek3 donusturucu = ic::topla(p, ic::carp(ic::matris(v.q), v.m.konum));
        const double bek = 3.0 - donusturucu[2];
        int n = 0;
        for (const auto& o : k.tur(SensorTuru::dvl)) {
            const auto& x = yuk<DvlOlcumu>(o);
            if ((x.dvl_flags & kDvlHizGecerli) == 0) continue;
            ++n;
            YAKIN(d, x.altitude, bek, 1e-5, v.ad);
        }
        std::printf("  %s: %d geçerli kayıt, beklenen irtifa %.6f m\n", v.ad, n, bek);
        DOGRULA(d, n > 5, std::string("DVL kilitlenmedi: ") + v.ad);
    }
    return d.hata;
}

// #2, W4-K7: alt_mm = (h − N) · 1000; N ortam ayarıdır (varsayılan 0).
int test_gnss_jeoit()
{
    Denetci d;
    std::map<std::uint32_t, std::int32_t> alt;
    for (double n_jeoit : {0.0, 30.0}) {
        SensorKumesiAyari a = ideal_ayar();
        yalniz(a, {SensorTuru::gnss});
        a.ortam.orijin->yukseklik = 100.0;
        a.ortam.jeoit_yuksekligi = n_jeoit;
        Kosu k;
        if (!k.kur(a)) return 1;
        if (!k.kos(durgun({0, 0, 0.05}, {1, 0, 0, 0}), 0, 1600)) return 1;
        for (const auto& o : k.tur(SensorTuru::gnss)) {
            const std::int32_t mm = yuk<GnssOlcumu>(o).alt_mm;
            if (n_jeoit == 0.0) {
                alt[o.hdr.seq] = mm;
                // Anten O_b'nin 0,22 m üstünde, O_b 0,05 m derinde: h = 100 + 0,17 m.
                YAKIN(d, mm, 100170.0, 1.0, "alt_mm, N = 0");
            } else {
                DOGRULA(d, alt.count(o.hdr.seq) == 1 && mm == alt[o.hdr.seq] - 30000,
                        "alt_mm = (h − N), N = 30 m");
            }
        }
    }
    return d.hata;
}

// #3: arıza pencereleri temel ölçüm zamanına göre; donmuş damga yalnız çıktıda.
int test_ariza_donma_bilesik()
{
    Denetci d;
    SensorKumesiAyari a = ida0_sensor_kumesi();
    yalniz(a, {SensorTuru::basinc});
    const auto temel = seq_haritasi(kos({}, 3.0, a), SensorTuru::basinc);
    const std::int64_t bas = kS, son = 2 * kS;
    auto pencerede = [&](std::uint32_t seq) {
        const std::int64_t t = temel.at(seq).hdr.t_meas_ns;
        return t >= bas && t < son;
    };
    {
        const auto v = seq_haritasi(kos({ariza("D", "baro0", ArizaTuru::donma_zaman_damgasi, bas, kS),
                                         ariza("L", "baro0", ArizaTuru::kayip, bas, kS)},
                                        3.0, a),
                                    SensorTuru::basinc);
        int sizan = 0;
        for (const auto& [seq, o] : v) sizan += pencerede(seq) ? 1 : 0;
        DOGRULA(d, sizan == 0 && v.size() + 25 == temel.size(),
                "donmuş damga + kayıp: pencere içi kayıtlar teslim edildi (" + std::to_string(sizan) + ")");
    }
    {
        const auto v = seq_haritasi(kos({ariza("D", "baro0", ArizaTuru::donma_zaman_damgasi, bas, kS),
                                         ariza("V", "baro0", ArizaTuru::yanlis_gecerlilik, bas, kS),
                                         ariza("K", "baro0", ArizaTuru::kendini_bildiren, bas, kS)},
                                        3.0, a),
                                    SensorTuru::basinc);
        int n = 0;
        for (const auto& [seq, o] : v) {
            if (!pencerede(seq)) continue;
            ++n;
            DOGRULA(d, (o.hdr.flags & kBayrakGecerli) == 0 && (o.hdr.flags & kBayrakSensorArizasi) != 0,
                    "donmuş damga + bayrak arızaları: bayraklar uygulanmadı");
            DOGRULA(d, o.hdr.t_meas_ns < bas, "damga donmadı");
        }
        DOGRULA(d, n == 25, "donma + bayrak: pencere içi kayıt sayısı");
    }
    {
        SensorArizasi g = ariza("T", "baro0", ArizaTuru::gec_teslim, bas, kS);
        g.siddet.gecikme_ns = 50'000'000;
        const auto v = seq_haritasi(kos({ariza("D", "baro0", ArizaTuru::donma_zaman_damgasi, bas, kS), g}, 3.0, a),
                                    SensorTuru::basinc);
        int n = 0;
        for (const auto& [seq, o] : v) {
            if (!pencerede(seq)) continue;
            n += o.hdr.t_teslim_ns >= temel.at(seq).hdr.t_teslim_ns + 50'000'000 ? 1 : 0;
        }
        DOGRULA(d, n == 25, "donmuş damga + geç teslim: gecikme uygulanmadı (" + std::to_string(n) + "/25)");
    }
    return d.hata;
}

// #4: arıza zinciri sonrası temsil edilemeyen değer karta gitmez (VALID = 0, son geçerli değer).
int test_ariza_temsil_araligi()
{
    Denetci d;
    SensorKumesiAyari a = ida0_sensor_kumesi();
    yalniz(a, {SensorTuru::manyetometre, SensorTuru::basinc, SensorTuru::gnss});
    SensorArizasi m = ariza("M", "mag0", ArizaTuru::ofset, kS, kS);
    m.siddet.ofset = 1e40;
    SensorArizasi b = ariza("B", "baro0", ArizaTuru::gurultu_artisi, kS, kS);
    b.siddet.k = 1e200;
    SensorArizasi g = ariza("G", "gnss0", ArizaTuru::ofset, 0, 0);
    g.siddet.ofset = 1e12;   // kuzeye 10⁹ km: enlem tanımsız
    Sayaclar say;
    Kosu k;
    if (!k.kur(a)) return 1;
    for (const auto& f : {m, b, g}) {
        const ArmSonucu s = k.kume.arm(f);
        if (!s.tamam) {
            std::printf("  arm reddedildi (%s): %s\n", f.kimlik.c_str(), s.hata.c_str());
            continue;
        }
    }
    if (!k.kos(durgun({0, 0, 0.05}, {1, 0, 0, 0}), 0, 2400)) return 1;
    say = k.kume.sayaclar();
    int pencere = 0;
    for (const auto& o : k.kayit) {
        DOGRULA(d, sonlu_yuk(o), "temsil edilemeyen değer karta gidiyor, tür " +
                                     std::to_string(static_cast<int>(o.hdr.tur)));
        const bool ic = o.hdr.t_meas_ns >= kS && o.hdr.t_meas_ns < 2 * kS;
        if ((o.hdr.tur == SensorTuru::manyetometre || o.hdr.tur == SensorTuru::basinc) && ic) {
            ++pencere;
            DOGRULA(d, (o.hdr.flags & kBayrakGecerli) == 0, "taşan değerde VALID düşmedi");
        }
        if (o.hdr.tur == SensorTuru::gnss) {
            DOGRULA(d, (o.hdr.flags & kBayrakGecerli) == 0, "tanımsız enlemde GNSS VALID düşmedi");
        }
    }
    DOGRULA(d, pencere == 125 && say.aralik_reddi > 0, "aralık reddi sayacı ya da pencere sayısı");
    return d.hata;
}

// #6: süzgeçli IMU'da gürültü artışı (k = 3) çıktı gürültüsünün σ'sını 3 katına çıkarır.
int test_ariza_suzgecli_imu()
{
    Denetci d;
    SensorKumesiAyari a = ideal_ayar();
    yalniz(a, {SensorTuru::imu});
    auto& p = ayar_govdesi<ImuAyar>(a, SensorTuru::imu);
    p.bit = 24;
    p.jiroskop_yogunluk = 4.887e-5;
    p.suzgec_etkin = true;
    p.suzgec_tau_ns = 3'000'000;
    double ss[2] = {0.0, 0.0};
    for (int i = 0; i < 2; ++i) {
        Kosu k;
        if (!k.kur(a)) return 1;
        if (i == 1) {
            SensorArizasi f = ariza("G", "imu0", ArizaTuru::gurultu_artisi, 0, 0);
            f.siddet.k = 3.0;
            if (!k.kume.arm(f).tamam) return 1;
        }
        if (!k.kos(durgun({0, 0, 1}, {1, 0, 0, 0}), 0, 80'000)) return 1;
        double s = 0.0;
        int n = 0;
        for (const auto& o : k.tur(SensorTuru::imu)) {
            const double x = static_cast<double>(yuk<ImuOlcumu>(o).gyro[0]);
            s += x * x;
            ++n;
        }
        ss[i] = std::sqrt(s / n);
    }
    std::printf("  süzgeçli IMU σ: k = 1 → %.4g, k = 3 → %.4g, oran %.3f\n", ss[0], ss[1], ss[1] / ss[0]);
    YAKIN(d, ss[1] / ss[0], 3.0, 0.15, "gürültü artışı oranı (k)");
    return d.hata;
}

// #7: model içi çekilişlerin örnek sayacı 64 bittir; teldeki seq modulo 2³².
int test_ornek_sayaci_64()
{
    Denetci d;
    SensorKumesiAyari a = ideal_ayar();
    yalniz(a, {SensorTuru::imu});
    auto& p = ayar_govdesi<ImuAyar>(a, SensorTuru::imu);
    p.bit = 24;
    p.jiroskop_yogunluk = 4.887e-5;
    const SensorAyari& s = sensor_ayari(a, SensorTuru::imu);
    std::string hata;
    auto m0 = ic::model_olustur(s, a, hata);
    auto m1 = ic::model_olustur(s, a, hata);
    m1->ornek_sayaci_kur(std::uint64_t{1} << 32);
    ic::Halka h;
    h.kur(16);
    DisDurum dis;
    std::vector<ic::TemelKayit> c0, c1;
    for (std::uint64_t n = 0; n <= 4; ++n) {
        h.ekle(durum(n, {0, 0, 1}, {}, {}, {1, 0, 0, 0}, {}), 20.0);
        ic::Baglam b;
        b.adim_no = n;
        b.t_ns = static_cast<std::int64_t>(n) * kAdim;
        b.adim_ns = kAdim;
        b.kontrol_periyodu_ns = 5'000'000;
        b.halka = &h;
        b.ortam = &a.ortam;
        b.guc = &a.guc;
        b.dis = &dis;
        b.ic_sicaklik = 20.0;
        if (n == 0) {
            m0->baslat(b);
            m1->baslat(b);
        }
        m0->adim(b, c0);
        m1->adim(b, c1);
    }
    DOGRULA(d, c0.size() == 1 && c1.size() == 1, "IMU örneği üretilmedi");
    if (c0.size() != 1 || c1.size() != 1) return d.hata;
    DOGRULA(d, c1[0].hdr.seq == 0, "teldeki seq 2³²'de sarmadı");
    const double sigma = 4.887e-5 / std::sqrt(0.01);
    const double bek = sigma * Akis(1, "sensor/imu0/olcum").normal(std::uint64_t{1} << 32, c2_kodla(0, 0, 0));
    YAKIN(d, c1[0].k[0], bek, 1.2e-6, "2³² numaralı örneğin çekilişi n = 2³² sayacından");
    DOGRULA(d, c0[0].k[0] != c1[0].k[0], "2³² ve 0 numaralı örnekler aynı çekilişi kullandı");
    return d.hata;
}

// Ek (W4-K4, K5; EMNİYET): geri okuma örneğinin revizyonu gösterdiği durumun revizyonudur.
int test_emniyet_revizyon()
{
    Denetci d;
    SensorKumesiAyari a = ideal_ayar();
    yalniz(a, {SensorTuru::firlatici_geri_okuma});
    Kosu k;
    if (!k.kur(a)) return 1;
    // Yetkili geçmiş (adım: durum, revizyon):
    //   0: estop basılı, fiş f1 takılı (rev 1); 236: estop bırakıldı (rev 2, gevşetme);
    //   400: estop basıldı (rev 3); 480: bırakıldı (rev 4); 484: f1 fişi çıktı (rev 5, kısıtlama).
    k.adim_oncesi = [](std::uint64_t n, SensorKumesi& s) {
        DisDurum dd;
        EmniyetDurumu& e = dd.emniyet;
        e.acil_durdurma = n < 236 || (n >= 400 && n < 480);
        e.fis[0] = n < 484;
        e.emniyet_revizyonu = n < 236 ? 1u : (n < 400 ? 2u : (n < 480 ? 3u : (n < 484 ? 4u : 5u)));
        s.dis_durum_ver(dd);
    };
    if (!k.kos(durgun({0, 0, 1}, {1, 0, 0, 0}), 0, 640)) return 1;
    std::map<std::int64_t, EmniyetOlcumu> v;
    for (const auto& o : k.tur(SensorTuru::firlatici_geri_okuma)) {
        v[o.hdr.t_meas_ns / kAdim] = std::get<EmniyetOlcumu>(o.yuk);
    }
    auto var = [&](std::int64_t n) { return v.count(n) == 1; };
    DOGRULA(d, var(240) && v[240].estop == 1 && v[240].safety_rev == 1,
            "bekleyen gevşetme sırasında (240) revizyon eski durumun revizyonu değil");
    DOGRULA(d, var(244) && v[244].estop == 0 && v[244].safety_rev == 2, "gevşetme algılanınca revizyon 2");
    DOGRULA(d, var(400) && v[400].estop == 1 && v[400].safety_rev == 3, "kısıtlama gecikmesiz, revizyon 3");
    // 484: fiş çıktı (kısıtlama, gecikmesiz) ama estop bırakılması (480) hâlâ bekliyor: karışık
    // durumda kısıtlayan okuma: bekleyen gevşetmeden önceki revizyon (3).
    DOGRULA(d, var(484) && v[484].estop == 1 && (v[484].tube[0] & kTupFisTakili) == 0 && v[484].safety_rev == 3,
            "karışık durumda revizyon kısıtlayan okumayla seçilmedi");
    DOGRULA(d, var(488) && v[488].estop == 0 && v[488].safety_rev == 5, "bütün değişimler algılanınca güncel revizyon");
    // Her örnek: revizyonu, durumun yetkili olduğu bir revizyondur ya da (karışık durumda) eskidir.
    for (const auto& [n, e] : v) {
        DOGRULA(d, !(e.estop == 0 && e.safety_rev == 1), "sim'de var olmamış çift (estop 0, rev 1)");
        DOGRULA(d, !(e.estop == 1 && e.safety_rev == 2), "sim'de var olmamış çift (estop 1, rev 2)");
    }
    return d.hata;
}

// #5 (EMNİYET): sürelerin kurulumda aralık denetimi.
int test_kurulum_sureleri()
{
    Denetci d;
    auto red = [&](const SensorKumesiAyari& a, const char* ad) {
        SensorKumesi k;
        const KurulumSonucu s = k.kur(a);
        DOGRULA(d, !s.tamam, std::string("reddedilmedi: ") + ad);
        if (!s.tamam) std::printf("  %s → %s\n", ad, s.hata.c_str());
    };
    SensorKumesiAyari a = ida0_sensor_kumesi();
    ayar_govdesi<FirlaticiGeriOkumaAyar>(a, SensorTuru::firlatici_geri_okuma).anahtar_gecikmesi_ns = -1;
    red(a, "negatif anahtar_gecikmesi_ns");
    a = ida0_sensor_kumesi();
    ayar_govdesi<FirlaticiGeriOkumaAyar>(a, SensorTuru::firlatici_geri_okuma).teslim_gecikmesi_ns = -1;
    red(a, "negatif teslim_gecikmesi_ns (geri okuma)");
    a = ida0_sensor_kumesi();
    ayar_govdesi<ManyetometreAyar>(a, SensorTuru::manyetometre).teslim_gecikmesi_ns = -5;
    red(a, "negatif teslim_gecikmesi_ns (manyetometre)");
    a = ida0_sensor_kumesi();
    ayar_govdesi<GnssAyar>(a, SensorTuru::gnss).teslim_gecikmesi_ns = -1;
    red(a, "negatif teslim_gecikmesi_ns (GNSS)");
    a = ida0_sensor_kumesi();
    ayar_govdesi<BataryaOlcerAyar>(a, SensorTuru::batarya_olcer).teslim_gecikmesi_ns = -1;
    red(a, "negatif teslim_gecikmesi_ns (batarya)");
    a = ida0_sensor_kumesi();
    ayar_govdesi<DvlAyar>(a, SensorTuru::dvl).islem_suresi_ns = -1;
    red(a, "negatif islem_suresi_ns (DVL)");
    a = ida0_sensor_kumesi();
    ayar_govdesi<SizintiAyar>(a, SensorTuru::sizinti).algilama_gecikmesi_ns = -1;
    red(a, "negatif algilama_gecikmesi_ns");
    a = ida0_sensor_kumesi();
    ayar_govdesi<GnssAyar>(a, SensorTuru::gnss).sicak_medyan_ns = 0;
    red(a, "sıfır medyan (A5-K48)");
    a = ida0_sensor_kumesi();
    ayar_govdesi<FirlaticiGeriOkumaAyar>(a, SensorTuru::firlatici_geri_okuma).anahtar_gecikmesi_ns =
        std::numeric_limits<std::int64_t>::max();
    red(a, "taşan anahtar_gecikmesi_ns");
    a = ida0_sensor_kumesi();
    sensor_ayari(a, SensorTuru::basinc).hiz_hz = 0.05;
    ayar_govdesi<BasincAyar>(a, SensorTuru::basinc).donusum_penceresi_ns = 5'000'000'000;
    red(a, "window_ns'e sığmayan dönüşüm penceresi");
    a = ida0_sensor_kumesi();
    a.kontrol_periyodu_ns = 5'000'000'000;
    red(a, "window_ns'e sığmayan kontrol periyodu");
    // arm: taşan süreler.
    {
        SensorKumesi k;
        if (!k.kur(ida0_sensor_kumesi()).tamam) return 1;
        SensorArizasi f = ariza("T", "baro0", ArizaTuru::gec_teslim, 0, 0);
        f.siddet.gecikme_ns = std::numeric_limits<std::int64_t>::max();
        DOGRULA(d, !k.arm(f).tamam, "taşan geç teslim gecikmesi kabul edildi");
        f = ariza("U", "baro0", ArizaTuru::kayip, std::numeric_limits<std::int64_t>::max(), 0);
        DOGRULA(d, !k.arm(f).tamam, "taşan başlangıç zamanı kabul edildi");
        f = ariza("V", "baro0", ArizaTuru::gurultu_artisi, 0, 0);
        f.siddet.k = 1e200;
        DOGRULA(d, k.arm(f).tamam, "k = 1e200 sözleşmeye göre geçerli (≥ 1); zincir sonrası denetlenir");
    }
    return d.hata;
}

}  // namespace ida::sensor::test
