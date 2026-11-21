// Kabul 6: arıza etkileri, kalkınca temel davranışa dönüş, zaman desenleri, arıza akışlarının
// bağımsız anahtarlanması; arm parametre denetimi.
#include "test_destek.hpp"

#include <map>

namespace ida::sensor::test {

namespace {

constexpr std::int64_t kS = 1'000'000'000;

fizik::DurumAnlik yavas(std::uint64_t n)
{
    const double t = sn(n);
    return durum(n, {0, 0, 0.5 + 0.05 * t}, {0, 0, 0.05}, {}, euler_q(0.05, 0.0, 0.1 * t), {0, 0, 0.1});
}

std::vector<Olcum> kos(const std::vector<SensorArizasi>& arizalar, double sure,
                       SensorKumesiAyari a = ida0_sensor_kumesi(), Sayaclar* say = nullptr)
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

SensorArizasi ariza(const char* kimlik, const char* hedef, ArizaTuru tur, std::int64_t bas, std::int64_t sure)
{
    SensorArizasi f;
    f.kimlik = kimlik;
    f.hedef = hedef;
    f.tur = tur;
    f.zaman_ns = bas;
    f.sure_ns = sure;
    return f;
}

bool icinde(const Olcum& o, std::int64_t bas, std::int64_t son)
{
    return o.hdr.t_meas_ns >= bas && o.hdr.t_meas_ns < son;
}

bool ayni_baro(const Olcum& a, const Olcum& b)
{
    const auto& x = yuk<BaroOlcumu>(a);
    const auto& y = yuk<BaroOlcumu>(b);
    return x.p_abs == y.p_abs && x.temp_c == y.temp_c && a.hdr.flags == b.hdr.flags &&
           a.hdr.t_meas_ns == b.hdr.t_meas_ns && a.hdr.t_teslim_ns == b.hdr.t_teslim_ns;
}

}  // namespace

int test_ariza_etkileri()
{
    Denetci d;
    const auto temel = kos({}, 4.0);
    const auto tb = seq_haritasi(temel, SensorTuru::basinc);
    const std::int64_t bas = 1 * kS, son = 2 * kS;

    // Ofset, ölçek, sürüklenmeler, sıçrama: pencere içinde formül, dışında temel kayıtla bit eşit.
    struct Sayisal {
        ArizaTuru tur;
        const char* ad;
    };
    for (const Sayisal s : {Sayisal{ArizaTuru::ofset, "ofset"}, Sayisal{ArizaTuru::olcek, "olcek"},
                            Sayisal{ArizaTuru::suruklenme_dogrusal, "suruklenme_dogrusal"},
                            Sayisal{ArizaTuru::suruklenme_ustel, "suruklenme_ustel"},
                            Sayisal{ArizaTuru::sicrama, "sicrama"}}) {
        SensorArizasi f = ariza("F1", "baro0", s.tur, bas, son - bas);
        f.siddet.ofset = 100.0;
        f.siddet.olcek = 2.0;
        f.siddet.oran = 10.0;
        f.siddet.son_deger = 50.0;
        f.siddet.tau_ns = kS / 2;
        f.siddet.p = 1.0;
        f.siddet.genlik = 5.0;
        const auto ar = seq_haritasi(kos({f}, 4.0), SensorTuru::basinc);
        DOGRULA(d, ar.size() == tb.size(), std::string(s.ad) + ": kayıt sayısı değişti");
        int pencere = 0;
        for (const auto& [seq, o] : ar) {
            const Olcum& t = tb.at(seq);
            if (!icinde(o, bas, son)) {
                DOGRULA(d, ayni_baro(o, t) && o.hdr.ariza_maskesi == 0,
                        std::string(s.ad) + ": pencere dışında temelden farklı");
                continue;
            }
            ++pencere;
            const double x = static_cast<double>(yuk<BaroOlcumu>(t).p_abs);
            const double dt = static_cast<double>(o.hdr.t_meas_ns - bas) * 1e-9;
            double bek = x;
            switch (s.tur) {
            case ArizaTuru::ofset: bek = x + 100.0; break;
            case ArizaTuru::olcek: bek = 2.0 * x; break;
            case ArizaTuru::suruklenme_dogrusal: bek = x + 10.0 * dt; break;
            case ArizaTuru::suruklenme_ustel: bek = x + 50.0 * (1.0 - std::exp(-dt / 0.5)); break;
            default: bek = x; break;
            }
            if (s.tur == ArizaTuru::sicrama) {
                YAKIN(d, std::abs(static_cast<double>(yuk<BaroOlcumu>(o).p_abs) - x), 5.0, 0.02, "sıçrama genliği");
            } else {
                YAKIN(d, yuk<BaroOlcumu>(o).p_abs, bek, 0.02, s.ad);
            }
            DOGRULA(d, o.hdr.ariza_maskesi == (1u << static_cast<unsigned>(s.tur)), "arıza maskesi");
        }
        DOGRULA(d, pencere == 25, std::string(s.ad) + ": pencere içi kayıt sayısı");
    }

    // Gürültü artışı (IMU, k = 3): fark varyansı σ_nom² (k² − 1).
    {
        SensorArizasi f = ariza("G1", "imu0", ArizaTuru::gurultu_artisi, bas, son - bas);
        f.siddet.k = 3.0;
        const auto ti = seq_haritasi(temel, SensorTuru::imu);
        const auto ai = seq_haritasi(kos({f}, 4.0), SensorTuru::imu);
        double s2 = 0.0;
        int n = 0;
        for (const auto& [seq, o] : ai) {
            const auto& x = yuk<ImuOlcumu>(ti.at(seq));
            const auto& y = yuk<ImuOlcumu>(o);
            if (!icinde(o, bas, son)) {
                DOGRULA(d, x.gyro == y.gyro && x.accel == y.accel, "gürültü artışı pencere dışında");
                continue;
            }
            const double e = static_cast<double>(y.gyro[0]) - static_cast<double>(x.gyro[0]);
            s2 += e * e;
            ++n;
        }
        const double sig = 4.887e-5 / std::sqrt(0.01);
        const double oran = std::sqrt(s2 / n) / (sig * std::sqrt(8.0));
        std::printf("  gürültü artışı: fark σ oranı %.3f (beklenen 1)\n", oran);
        YAKIN(d, oran, 1.0, 0.2, "gürültü artışı k²");
    }

    // Donma: yük arızadan önceki son çıktı; t_meas ve seq ilerler. Donma (zaman damgası): t_meas de donar.
    for (int damga = 0; damga < 2; ++damga) {
        const ArizaTuru tur = damga ? ArizaTuru::donma_zaman_damgasi : ArizaTuru::donma;
        const auto ar = seq_haritasi(kos({ariza("D1", "baro0", tur, bas, son - bas)}, 4.0), SensorTuru::basinc);
        const Olcum* once = nullptr;
        for (const auto& [seq, o] : tb) {
            if (o.hdr.t_meas_ns < bas) once = &o;
        }
        int n = 0;
        for (const auto& [seq, o] : ar) {
            const Olcum& t = tb.at(seq);
            if (t.hdr.t_meas_ns < bas || t.hdr.t_meas_ns >= son) {
                DOGRULA(d, ayni_baro(o, t), "donma dışında temelden farklı");
                continue;
            }
            ++n;
            DOGRULA(d, yuk<BaroOlcumu>(o).p_abs == yuk<BaroOlcumu>(*once).p_abs, "donma yükü önceki çıktı değil");
            DOGRULA(d, o.hdr.t_meas_ns == (damga ? once->hdr.t_meas_ns : t.hdr.t_meas_ns), "donma t_meas");
        }
        DOGRULA(d, n == 25 && ar.size() == tb.size(), "donma kayıt sayısı");
    }
    // Donma, önceki çıktı yokken: arıza bitene kadar kayıt yok, sayaç artar.
    {
        Sayaclar say;
        const auto ar = seq_haritasi(kos({ariza("D2", "baro0", ArizaTuru::donma, 0, son)}, 4.0,
                                         ida0_sensor_kumesi(), &say),
                                     SensorTuru::basinc);
        bool kayit_yok = true;
        for (const auto& [seq, o] : ar) kayit_yok = kayit_yok && o.hdr.t_meas_ns >= son;
        DOGRULA(d, kayit_yok && !ar.empty() && say.donma_onceki_cikti_yok == 1,
                "önceki çıktı yokken donma kuralı (A5 Tablo 3.10-A)");
    }
    // Yanlış geçerlilik, kendini bildiren, kayıp, geç teslim.
    {
        const auto yg = seq_haritasi(kos({ariza("V1", "baro0", ArizaTuru::yanlis_gecerlilik, bas, son - bas)}, 4.0),
                                     SensorTuru::basinc);
        const auto kb = seq_haritasi(kos({ariza("K1", "baro0", ArizaTuru::kendini_bildiren, bas, son - bas)}, 4.0),
                                     SensorTuru::basinc);
        for (const auto& [seq, o] : yg) {
            const bool ic = icinde(o, bas, son);
            DOGRULA(d, ((o.hdr.flags & kBayrakGecerli) == 0) == ic, "yanlış geçerlilik biti");
            DOGRULA(d, yuk<BaroOlcumu>(o).p_abs == yuk<BaroOlcumu>(tb.at(seq)).p_abs, "yanlış geçerlilik yükü değiştirdi");
        }
        for (const auto& [seq, o] : kb) {
            DOGRULA(d, ((o.hdr.flags & kBayrakSensorArizasi) != 0) == icinde(o, bas, son), "kendini bildiren biti");
        }
        Sayaclar say;
        const auto ky = seq_haritasi(kos({ariza("L1", "baro0", ArizaTuru::kayip, bas, son - bas)}, 4.0,
                                         ida0_sensor_kumesi(), &say),
                                     SensorTuru::basinc);
        DOGRULA(d, ky.size() + 25 == tb.size() && say.kayip == 25, "kayıp: 25 kayıt düşmeli");
        for (const auto& [seq, o] : ky) DOGRULA(d, !icinde(o, bas, son) && ayni_baro(o, tb.at(seq)), "kayıp dışı");

        SensorArizasi g = ariza("T1", "baro0", ArizaTuru::gec_teslim, bas, son - bas);
        g.siddet.gecikme_ns = 50'000'000;
        g.siddet.titresim_ns = 10'000'000;
        const auto gt = seq_haritasi(kos({g}, 4.0), SensorTuru::basinc);
        std::int64_t onceki = 0;
        for (const auto& [seq, o] : gt) {
            const std::int64_t fark = o.hdr.t_teslim_ns - tb.at(seq).hdr.t_teslim_ns;
            if (icinde(o, bas, son)) {
                DOGRULA(d, fark >= 40'000'000 && fark <= 60'000'000, "geç teslim gecikmesi");
            }
            DOGRULA(d, o.hdr.t_teslim_ns >= onceki, "geç teslimde sensör içi sıra bozuldu");
            DOGRULA(d, o.hdr.t_meas_ns == tb.at(seq).hdr.t_meas_ns, "geç teslim t_meas'ı değiştirdi");
            onceki = o.hdr.t_teslim_ns;
        }
    }
    // disarm ve active: süresiz arıza 1 s'de başlar, 2 s'de kaldırılır.
    {
        Kosu k;
        if (!k.kur(ida0_sensor_kumesi())) return 1;
        SensorArizasi f = ariza("S1", "mag0", ArizaTuru::ofset, bas, 0);
        f.siddet.ofset = 1e-3;
        const ArmSonucu a = k.kume.arm(f);
        DOGRULA(d, a.tamam && k.kume.active().size() == 1, "arm/active");
        k.kos(yavas, 0, 1600);
        DOGRULA(d, k.kume.disarm(a.tutamac), "disarm");
        k.kos(yavas, 1600, 3200);
        DOGRULA(d, k.kume.active().empty(), "disarm sonrası active boş değil");
        for (const auto& o : k.tur(SensorTuru::manyetometre)) {
            const bool ic = icinde(o, bas, 2 * kS);
            DOGRULA(d, (yuk<MagOlcumu>(o).field[0] > 5e-4f) == ic, "süresiz arıza ve disarm penceresi");
        }
    }
    // arm parametre denetimi.
    {
        Kosu k;
        if (!k.kur(ida0_sensor_kumesi())) return 1;
        auto red = [&](SensorArizasi f, const char* ad) {
            DOGRULA(d, !k.kume.arm(f).tamam, std::string("reddedilmedi: ") + ad);
        };
        red(ariza("R1", "yok0", ArizaTuru::ofset, 0, 0), "bilinmeyen hedef");
        red(ariza("R2", "ping360_0", ArizaTuru::motor_takilmasi, 0, 0), "kapsam dışı sensör");
        red(ariza("R3", "dvl0", ArizaTuru::dvl_isin_kaybi, 0, 0), "durumlu arıza (yer tutucu)");
        red(ariza("R4", "sizinti0", ArizaTuru::ofset, 0, 0), "kanalsız sensöre sayısal arıza");
        SensorArizasi f = ariza("R5", "imu0", ArizaTuru::gurultu_artisi, 0, 0);
        f.siddet.k = 0.5;
        red(f, "k < 1");
        f = ariza("R6", "baro0", ArizaTuru::gec_teslim, 0, 0);
        f.siddet.gecikme_ns = 5;
        f.siddet.titresim_ns = 10;
        red(f, "titreşim > gecikme (A5-K66)");
        f = ariza("R7", "baro0", ArizaTuru::kayip, 0, 0);
        f.kanal_maskesi = 1;
        red(f, "kanal maskesi (A5-K69)");
        f = ariza("R8", "baro0", ArizaTuru::kayip, 0, 0);
        f.zaman_deseni.tur = ZamanDeseniTuru::bernoulli;
        f.zaman_deseni.p = 1.5;
        red(f, "p > 1");
        DOGRULA(d, k.kume.arm(ariza("R9", "baro0", ArizaTuru::kayip, 0, 0)).tamam, "geçerli arıza reddedildi");
        red(ariza("R9", "baro0", ArizaTuru::kayip, 0, 0), "yinelenen kimlik (A5-K60)");
    }
    return d.hata;
}

// Zaman desenleri: Bernoulli(p) ve Gilbert–Elliott (A5 Ek D.4, D.5).
int test_ariza_desenleri()
{
    Denetci d;
    SensorKumesiAyari a = ida0_sensor_kumesi();
    yalniz(a, {SensorTuru::basinc});
    const std::int64_t sure = 200 * kS;
    {
        SensorArizasi f = ariza("B1", "baro0", ArizaTuru::kayip, 0, 0);
        f.zaman_deseni.tur = ZamanDeseniTuru::bernoulli;
        f.zaman_deseni.p = 0.3;
        const auto v = kos({f}, 200.0, a);
        const double kayip = 1.0 - static_cast<double>(v.size()) / 5000.0;
        std::printf("  Bernoulli kayıp oranı %.3f (beklenen 0,3)\n", kayip);
        YAKIN(d, kayip, 0.3, 0.03, "Bernoulli oranı");
        const auto v2 = kos({f}, 200.0, a);
        DOGRULA(d, v.size() == v2.size(), "Bernoulli deseni belirlenimci değil");
    }
    {
        SensorArizasi f = ariza("GE1", "baro0", ArizaTuru::kayip, 0, sure);
        f.zaman_deseni.tur = ZamanDeseniTuru::gilbert_elliott;
        f.zaman_deseni.t_iyi_ns = 2 * kS;
        f.zaman_deseni.t_kotu_ns = kS;
        const auto v = kos({f}, 200.0, a);
        const double kayip = 1.0 - static_cast<double>(v.size()) / 5000.0;
        std::printf("  Gilbert–Elliott kayıp oranı %.3f (beklenen ≈ 1/3)\n", kayip);
        YAKIN(d, kayip, 1.0 / 3.0, 0.1, "GE oranı");
        // Süreç t_b'de "iyi" başlar: ilk kayıt kaybolmaz; kayıplar ardışık öbekler halindedir.
        DOGRULA(d, !v.empty() && v.front().hdr.seq == 0, "GE süreci iyi durumda başlamadı");
        int obek = 0;
        for (std::size_t i = 1; i < v.size(); ++i) obek += v[i].hdr.seq != v[i - 1].hdr.seq + 1 ? 1 : 0;
        std::printf("  kayıp öbeği %d (≈ 200 s / 3 s)\n", obek);
        DOGRULA(d, obek > 40 && obek < 100, "GE öbek sayısı beklenmedik");
    }
    return d.hata;
}

// Arıza akışı `ariza/<kimlik>` ile anahtarlanır: ölçüm akışlarını ve başka arızaları etkilemez;
// aynı kimlik başka sensörler kapalıyken de aynı çekilişleri verir; farklı kimlik farklı.
int test_ariza_akis_bagimsizligi()
{
    Denetci d;
    SensorArizasi f = ariza("A", "baro0", ArizaTuru::gurultu_artisi, kS, kS);
    f.siddet.k = 4.0;
    SensorArizasi g = ariza("B", "imu0", ArizaTuru::gurultu_artisi, kS, kS);
    g.siddet.k = 2.0;
    const auto tam = kos({f, g}, 3.0);
    SensorKumesiAyari a = ida0_sensor_kumesi();
    yalniz(a, {SensorTuru::basinc});
    const auto yalniz_baro = kos({f}, 3.0, a);
    const auto pb1 = seq_haritasi(tam, SensorTuru::basinc);
    const auto pb2 = seq_haritasi(yalniz_baro, SensorTuru::basinc);
    bool esit = pb1.size() == pb2.size();
    for (const auto& [seq, o] : pb1) esit = esit && ayni_baro(o, pb2.at(seq));
    DOGRULA(d, esit, "aynı kimlikli arızanın çekilişleri başka sensör ve arızalarla değişti");
    // Temel ölçüm dizisi arızadan bağımsız: IMU arızası basıncı, basınç arızası IMU'yu etkilemez.
    const auto temel = kos({}, 3.0);
    const auto ti = seq_haritasi(temel, SensorTuru::imu);
    const auto bi = seq_haritasi(kos({f}, 3.0), SensorTuru::imu);
    bool imu_ayni = ti.size() == bi.size();
    for (const auto& [seq, o] : ti) {
        imu_ayni = imu_ayni && yuk<ImuOlcumu>(o).gyro == yuk<ImuOlcumu>(bi.at(seq)).gyro;
    }
    DOGRULA(d, imu_ayni, "basınç arızası IMU dizisini değiştirdi");
    SensorArizasi h = f;
    h.kimlik = "C";
    const auto pc = seq_haritasi(kos({h}, 3.0), SensorTuru::basinc);
    int farkli = 0;
    for (const auto& [seq, o] : pc) {
        farkli += icinde(o, kS, 2 * kS) && yuk<BaroOlcumu>(o).p_abs != yuk<BaroOlcumu>(pb1.at(seq)).p_abs ? 1 : 0;
    }
    DOGRULA(d, farkli > 20, "farklı arıza kimliği aynı çekilişleri verdi");
    // Ana tohum arıza akışını da anahtarlar (A5-K83): farklı tohumda farklı.
    SensorKumesiAyari b = ida0_sensor_kumesi();
    b.ana_tohum = 7;
    const auto t7 = seq_haritasi(kos({}, 3.0, b), SensorTuru::basinc);
    const auto f7 = seq_haritasi(kos({f}, 3.0, b), SensorTuru::basinc);
    const auto t1 = seq_haritasi(temel, SensorTuru::basinc);
    double fark1 = 0.0, fark7 = 0.0;
    for (const auto& [seq, o] : pb1) {
        if (!icinde(o, kS, 2 * kS)) continue;
        fark1 += static_cast<double>(yuk<BaroOlcumu>(o).p_abs) - static_cast<double>(yuk<BaroOlcumu>(t1.at(seq)).p_abs);
        fark7 += static_cast<double>(yuk<BaroOlcumu>(f7.at(seq)).p_abs) - static_cast<double>(yuk<BaroOlcumu>(t7.at(seq)).p_abs);
    }
    DOGRULA(d, std::abs(fark1 - fark7) > 1.0, "arıza akışı ana tohumdan bağımsız çıktı");
    return d.hata;
}

}  // namespace ida::sensor::test
