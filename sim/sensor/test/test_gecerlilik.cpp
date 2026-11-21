// Kabul 4: GNSS dalınca geçersiz, yüzeye çıkınca geçerli; DVL irtifa sınırları dışında geçersiz;
// doyma bayrakları. Ayrıca fırlatıcı/emniyet geri okuması ve sızıntı.
#include "test_destek.hpp"

namespace ida::sensor::test {

namespace {

double yumusak(double x)
{
    if (x <= 0.0) return 0.0;
    if (x >= 1.0) return 1.0;
    return x * x * x * (10.0 + x * (-15.0 + 6.0 * x));
}

}  // namespace

int test_gecerlilik_gnss()
{
    Denetci d;
    // 0–5 s yüzeyde; 5–33 s 0,02 m/s ile 0,05 → 0,61 m (anten yüzeyi yavaş geçer: uydular
    // yükselme açısına göre sırayla kaybolur); 40–41 s yumuşak yüzeye çıkış; 60 s'ye kadar yüzeyde.
    auto z = [](double t) {
        const double dalis = std::clamp(t - 5.0, 0.0, 28.0) * 0.02;
        return 0.05 + dalis - dalis * yumusak(t - 40.0);
    };
    const Yorunge y = [&](std::uint64_t n) {
        return durum(n, {0, 0, z(sn(n))}, {}, {}, Kuaterniyon{1, 0, 0, 0}, {});
    };
    Kosu k;
    if (!k.kur(ida0_sensor_kumesi())) return 1;
    if (!k.kos(y, 0, 48'000)) return 1;
    int yuzey = 0, dalis = 0, geri = 0, bant = 0;
    double eph_yuzey = 0.0, eph_en_buyuk = 0.0;
    double t_geri_ilk = -1.0;
    for (const auto& o : k.tur(SensorTuru::gnss)) {
        const double t = static_cast<double>(o.hdr.t_meas_ns) * 1e-9;
        const auto& g = yuk<GnssOlcumu>(o);
        const bool gecerli = (o.hdr.flags & kBayrakGecerli) != 0;
        if (t < 5.0) {
            yuzey += gecerli && g.fix_type == kFix3B ? 1 : 0;
            eph_yuzey = static_cast<double>(g.eph);
        } else if (t >= 20.0 && t < 40.0) {
            dalis += !gecerli && g.fix_type == kFixYok && g.n_sv == 0 ? 1 : 0;
        } else if (t >= 40.0 && gecerli && t_geri_ilk < 0.0) {
            t_geri_ilk = t;
        }
        if (t >= 50.0) geri += gecerli ? 1 : 0;
        if (t >= 5.0 && t < 20.0 && gecerli && g.n_sv < 20) {
            ++bant;
            eph_en_buyuk = std::max(eph_en_buyuk, static_cast<double>(g.eph));
        }
    }
    std::printf("  yüzey geçerli %d/25, dalışta geçersiz %d/100, ilk yeniden fix %.1f s, 50–60 s geçerli %d/50,"
                " azalmış uyduyla %d örnek (eph %.3f → en çok %.3f)\n",
                yuzey, dalis, t_geri_ilk, geri, bant, eph_yuzey, eph_en_buyuk);
    DOGRULA(d, yuzey == 25, "yüzeyde GNSS geçerli değil");
    DOGRULA(d, dalis == 100, "dalışta GNSS geçerli");
    DOGRULA(d, geri == 50, "yüzeye çıkınca GNSS geçerli olmadı");
    DOGRULA(d, t_geri_ilk > 40.5 && t_geri_ilk < 50.0, "yeniden fix anı beklenmedik");
    DOGRULA(d, bant > 0 && eph_en_buyuk > eph_yuzey, "fix kaybından önce eph büyümedi (05 §2.3.4)");
    return d.hata;
}

int test_gecerlilik_dvl()
{
    Denetci d;
    struct Vaka {
        const char* ad;
        double z;
        double taban;
        bool gecerli;
    };
    const Vaka vakalar[] = {
        {"olağan (irtifa ≈ 1,86 m)", 1.0, 3.0, true},
        {"dibe çok yakın (irtifa 0,03 m < 0,05)", 2.83, 3.0, false},
        {"çok derin taban (irtifa > 50 m)", 1.0, 100.0, false},
        {"dönüştürücü havada", -0.5, 3.0, false},
    };
    for (const auto& v : vakalar) {
        SensorKumesiAyari a = ideal_ayar();
        yalniz(a, {SensorTuru::dvl});
        a.ortam.derinlik = v.taban;
        Kosu k;
        if (!k.kur(a)) return 1;
        if (!k.kos(durgun({0, 0, v.z}, {1, 0, 0, 0}), 0, 4000)) return 1;
        const auto r = k.tur(SensorTuru::dvl);
        int uyan = 0;
        for (const auto& o : r) {
            const auto& x = yuk<DvlOlcumu>(o);
            const bool g = (x.dvl_flags & kDvlHizGecerli) != 0;
            if (v.gecerli) {
                uyan += g && x.beam_valid == 0x0F ? 1 : 0;
            } else {
                uyan += !g && x.beam_valid == 0 && x.altitude == -1.0f && x.beam_dist[0] == -1.0f &&
                                x.vel[0] == 0.0f
                            ? 1
                            : 0;
            }
        }
        std::printf("  %s: %d/%zu kayıt beklendiği gibi\n", v.ad, uyan, r.size());
        DOGRULA(d, !r.empty() && uyan == static_cast<int>(r.size()), std::string("DVL geçerlilik: ") + v.ad);
    }
    return d.hata;
}

int test_gecerlilik_doyma()
{
    Denetci d;
    SensorKumesiAyari a = ideal_ayar();
    yalniz(a, {SensorTuru::imu, SensorTuru::batarya_olcer});
    {
        // 10 rad/s dönüş (> 8,7266 rad/s) ve 50 m/s² ivme (> 39,2266 m/s²).
        Kosu k;
        if (!k.kur(a)) return 1;
        const Yorunge y = [](std::uint64_t n) {
            const double t = sn(n);
            return durum(n, {25.0 * t * t, 0, 1}, {50.0 * t, 0, 0}, {50.0, 0, 0}, euler_q(0, 0, 10.0 * t),
                         {0, 0, 10.0});
        };
        if (!k.kos(y, 0, 800)) return 1;
        for (const auto& o : k.tur(SensorTuru::imu)) {
            const auto& x = yuk<ImuOlcumu>(o);
            DOGRULA(d, (o.hdr.flags & kBayrakDoymus) != 0, "IMU doyma bayrağı yok");
            YAKIN(d, x.gyro[2], 8.7266, 3e-4, "jiroskop kırpması");
            DOGRULA(d, std::abs(x.accel[0]) <= 39.2266f + 1e-3f, "ivme kırpılmadı");
        }
    }
    {
        // Bütün iticiler tam itkide: paket akımı ≈ 188 A, ADC (3,3 V) doyar.
        Kosu k;
        if (!k.kur(a)) return 1;
        const Yorunge y = [](std::uint64_t n) {
            fizik::DurumAnlik s = durum(n, {0, 0, 1}, {}, {}, {1, 0, 0, 0}, {});
            s.itki_istegi.fill(80.0);
            return s;
        };
        if (!k.kos(y, 0, 800)) return 1;
        const auto b = k.tur(SensorTuru::batarya_olcer);
        DOGRULA(d, !b.empty(), "batarya kaydı yok");
        for (const auto& o : b) {
            DOGRULA(d, (o.hdr.flags & kBayrakDoymus) != 0, "batarya ADC doyma bayrağı yok");
            YAKIN(d, yuk<BataryaOlcumu>(o).current, (3.3 * 4095.0 / 4096.0 - 0.33) * 37.8788, 1e-3,
                  "akım ADC tavanında");
        }
    }
    {
        // Olağan durumda doyma yok.
        Kosu k;
        if (!k.kur(a)) return 1;
        if (!k.kos(durgun({0, 0, 1}, {1, 0, 0, 0}), 0, 800)) return 1;
        for (const auto& o : k.kayit) DOGRULA(d, (o.hdr.flags & kBayrakDoymus) == 0, "olağan durumda doyma");
    }
    return d.hata;
}

// Fırlatıcı ve emniyet geri okuması (05 §2.3.8; A1-K56): kısıtlama gecikmesiz, gevşetme
// anahtar gecikmesiyle (10 ms) algılanır; her değişimde ek örnek; izin ve revizyon t_meas anında.
int test_emniyet_geri_okuma()
{
    Denetci d;
    SensorKumesiAyari a = ideal_ayar();
    yalniz(a, {SensorTuru::firlatici_geri_okuma});
    Kosu k;
    if (!k.kur(a)) return 1;
    k.adim_oncesi = [](std::uint64_t n, SensorKumesi& s) {
        DisDurum dd;
        EmniyetDurumu& e = dd.emniyet;
        e.acil_durdurma = (n >= 100 && n < 200) || (n >= 700 && n < 703);
        e.fis[0] = n >= 300;
        e.pim[0] = n < 400;
        e.izin_acik[0] = n >= 500;
        e.emniyet_revizyonu = n >= 600 ? 2u : 1u;
        s.dis_durum_ver(dd);
    };
    if (!k.kos(durgun({0, 0, 1}, {1, 0, 0, 0}), 0, 1000)) return 1;
    const auto v = k.tur(SensorTuru::firlatici_geri_okuma);
    auto bul = [&](std::int64_t adim) -> const Olcum* {
        for (const auto& o : v) {
            if (o.hdr.t_meas_ns == adim * kAdim) return &o;
        }
        return nullptr;
    };
    auto e = [](const Olcum* o) { return std::get<EmniyetOlcumu>(o->yuk); };
    for (std::size_t i = 0; i < v.size(); ++i) {
        DOGRULA(d, v[i].hdr.seq == i, "seq her örnekte bir artmıyor");
        DOGRULA(d, v[i].hdr.t_teslim_ns - v[i].hdr.t_meas_ns == 1'000'000, "teslim gecikmesi 1 ms değil");
    }
    const Olcum* o = bul(0);
    DOGRULA(d, o && e(o).estop == 0 && e(o).tube[0] == kTupDolu && e(o).safety_rev == 1, "başlangıç durumu");
    o = bul(100);
    DOGRULA(d, o && e(o).estop == 1, "acil durdurma basılınca gecikmesiz örnek yok");
    o = bul(200);
    DOGRULA(d, o == nullptr, "gevşetme anahtar gecikmesinden önce örneklendi");
    o = bul(208);
    DOGRULA(d, o && e(o).estop == 0, "acil durdurma bırakılınca 10 ms sonra örnek yok");
    o = bul(308);
    DOGRULA(d, o && (e(o).tube[0] & kTupFisTakili) != 0, "fiş takılması 10 ms sonra algılanmadı");
    DOGRULA(d, bul(300) == nullptr || (e(bul(300)).tube[0] & kTupFisTakili) == 0, "fiş gecikmesiz algılandı");
    o = bul(408);
    DOGRULA(d, o && (e(o).tube[0] & kTupPimCikik) != 0, "pim çıkarılması 10 ms sonra algılanmadı");
    o = bul(500);
    DOGRULA(d, o && (e(o).tube[0] & kTupIzinAcik) != 0, "izin açılınca aynı anda örnek yok");
    o = bul(600);
    DOGRULA(d, o && e(o).safety_rev == 2, "revizyon değişiminde örnek yok");
    // Kısa basış (3 adım): basma ve bırakma ayrı örneklerde.
    o = bul(700);
    DOGRULA(d, o && e(o).estop == 1, "kısa basış algılanmadı");
    o = bul(711);
    DOGRULA(d, o && e(o).estop == 0, "kısa basıştan sonra bırakma algılanmadı");
    // Periyodik 10 Hz örnekler sürer.
    DOGRULA(d, bul(80) && bul(160) && bul(880), "periyodik örnek eksik");
    return d.hata;
}

int test_sizinti()
{
    Denetci d;
    SensorKumesiAyari a = ideal_ayar();
    yalniz(a, {SensorTuru::sizinti});
    Kosu k;
    if (!k.kur(a)) return 1;
    k.adim_oncesi = [](std::uint64_t n, SensorKumesi& s) {
        DisDurum dd;
        dd.islak_problar = n >= 800 ? 0x1 : 0x0;   // 1 s'de prob 0 ıslanır
        s.dis_durum_ver(dd);
    };
    if (!k.kos(durgun({0, 0, 1}, {1, 0, 0, 0}), 0, 3200)) return 1;
    for (const auto& o : k.tur(SensorTuru::sizinti)) {
        const std::uint8_t m = yuk<SizintiOlcumu>(o).mask;
        DOGRULA(d, m == (o.hdr.t_meas_ns >= 2'000'000'000 ? 1 : 0),
                "algılama gecikmesi (1 s) uygulanmadı, t_meas = " + std::to_string(o.hdr.t_meas_ns));
    }
    ayar_govdesi<SizintiAyar>(a, SensorTuru::sizinti).sahte_pozitif_orani = 0.5;
    Kosu k2;
    if (!k2.kur(a)) return 1;
    if (!k2.kos(durgun({0, 0, 1}, {1, 0, 0, 0}), 0, 80'000)) return 1;
    int bit = 0, toplam = 0;
    for (const auto& o : k2.tur(SensorTuru::sizinti)) {
        const std::uint8_t m = yuk<SizintiOlcumu>(o).mask;
        bit += (m & 1) + ((m >> 1) & 1);
        toplam += 2;
    }
    const double oran = static_cast<double>(bit) / static_cast<double>(toplam);
    std::printf("  sahte pozitif oranı %.3f (beklenen 0,5)\n", oran);
    YAKIN(d, oran, 0.5, 0.05, "sahte pozitif oranı");
    return d.hata;
}

}  // namespace ida::sensor::test
