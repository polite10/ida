// Kabul 3: hız, gecikme, t_meas ve pencere; örnek numaraları kesintisiz; zamanlayıcı.
#include "test_destek.hpp"

#include "model.hpp"

namespace ida::sensor::test {

namespace {

struct Beklenen {
    SensorTuru tur;
    std::int64_t periyot;     // ns
    std::int64_t teslim;      // t_teslim − t_meas
    std::uint32_t pencere;    // window_ns
};

std::int64_t ust_kontrol(std::int64_t t) { return ((t + 4 * kAdim - 1) / (4 * kAdim)) * 4 * kAdim; }

}  // namespace

int test_zaman_hiz_gecikme()
{
    Denetci d;
    SensorKumesiAyari a = ida0_sensor_kumesi();
    Kosu k;
    if (!k.kur(a)) return 1;
    const std::uint64_t adim = 8000;   // 10 s
    // Parti sınırlarında boşaltma: kayıtlar t_teslim sırasıyla ve t_teslim ≤ t_n.
    const Yorunge y = durgun({0, 0, 0.05}, {1, 0, 0, 0});
    std::int64_t son_teslim = -1;
    for (std::uint64_t n = 0; n < adim; ++n) {
        if (k.kume.adim(y(n), n) != AdimSonucu::tamam) return 1;
        if (n % 4 == 0) {
            const std::int64_t tn = static_cast<std::int64_t>(n) * kAdim;
            std::vector<Olcum> v;
            k.kume.bosalt(tn, v);
            for (const auto& o : v) {
                DOGRULA(d, o.hdr.t_teslim_ns <= tn, "t_teslim > t_n olan kayıt boşaltıldı");
                DOGRULA(d, o.hdr.t_teslim_ns >= son_teslim, "boşaltma t_teslim sırasında değil");
                DOGRULA(d, o.hdr.t_hazir_ns <= tn, "t_hazır > t_n olan kayıt boşaltıldı");
                son_teslim = o.hdr.t_teslim_ns;
                k.kayit.push_back(o);
            }
        }
    }
    const Beklenen tablo[] = {
        {SensorTuru::imu, 5'000'000, 0, 5'000'000},
        {SensorTuru::manyetometre, 10'000'000, 1'000'000, 0},
        {SensorTuru::basinc, 40'000'000, 19'750'000, 18'750'000},
        {SensorTuru::gnss, 200'000'000, 120'000'000, 0},
        {SensorTuru::batarya_olcer, 100'000'000, 1'000'000, 100'000'000},
        {SensorTuru::sizinti, 100'000'000, 0, 0},
        {SensorTuru::firlatici_geri_okuma, 100'000'000, 1'000'000, 0},
    };
    for (const auto& b : tablo) {
        const auto v = k.tur(b.tur);
        const std::string ad = "tür " + std::to_string(static_cast<int>(b.tur));
        DOGRULA(d, v.size() > 10, ad + ": kayıt az");
        for (std::size_t i = 0; i < v.size(); ++i) {
            const auto& h = v[i].hdr;
            DOGRULA(d, h.seq == i, ad + ": seq kesintili");
            DOGRULA(d, h.window_ns == b.pencere, ad + ": window_ns");
            DOGRULA(d, h.t_teslim_ns - h.t_meas_ns == b.teslim, ad + ": t_teslim − t_meas");
            DOGRULA(d, h.t_hazir_ns % kAdim == 0, ad + ": t_hazır adım sınırında değil");
            DOGRULA(d, h.t_meas_ns <= h.t_hazir_ns && h.t_hazir_ns <= ust_kontrol(h.t_teslim_ns),
                    ad + ": nedensellik (t_meas ≤ t_hazır ≤ t_teslim'in üst kontrol sınırı)");
            if (i > 0) {
                DOGRULA(d, h.t_meas_ns - v[i - 1].hdr.t_meas_ns == b.periyot, ad + ": örnekleme aralığı");
            }
        }
    }
    // IMU: t_meas kontrol sınırında (5 ms'nin katı); basınç: t_meas = D1 başlangıcı + 18,75 ms.
    for (const auto& o : k.tur(SensorTuru::imu)) {
        DOGRULA(d, o.hdr.t_meas_ns % 5'000'000 == 0, "IMU t_meas kontrol sınırında değil");
    }
    for (const auto& o : k.tur(SensorTuru::basinc)) {
        DOGRULA(d, (o.hdr.t_meas_ns - 18'750'000) % 40'000'000 == 0, "basınç t_meas = D1 sonu değil");
    }

    // Rasyonel zamanlayıcı: 30 Hz → 26/27 adım; ideal anla fark ≤ h/2; 10⁶ örnekte sürüklenme yok.
    ic::Zamanlayici z;
    z.kur(1e9 / 30.0, kAdim, 0);
    std::uint64_t onceki = 0;
    std::uint64_t sayac = 0;
    bool aralik_tamam = true, fark_tamam = true;
    for (std::uint64_t n = 0; sayac < 1'000'000; ++n) {
        if (z.tetik(n)) {
            const std::int64_t t = static_cast<std::int64_t>(n) * kAdim;
            const double ideal = static_cast<double>(sayac) * 1e9 / 30.0;
            fark_tamam = fark_tamam && std::abs(static_cast<double>(t) - ideal) <= 625'000.0 + 1.0;
            if (sayac > 0) {
                const std::uint64_t ar = n - onceki;
                aralik_tamam = aralik_tamam && (ar == 26 || ar == 27);
            }
            onceki = n;
            ++sayac;
        }
    }
    DOGRULA(d, aralik_tamam, "30 Hz aralığı 26/27 adım değil");
    DOGRULA(d, fark_tamam, "en yakın sınır kuralı (≤ 0,625 ms) bozuldu ya da sürüklenme var");

    // Saat kayması: 100 ppm'de 100 s sonra 1 ms geride.
    {
        SensorKumesiAyari b = ida0_sensor_kumesi();
        yalniz(b, {SensorTuru::manyetometre});
        sensor_ayari(b, SensorTuru::manyetometre).saat_kaymasi_ppm = 100.0;
        Kosu kk;
        if (!kk.kur(b)) return 1;
        if (!kk.kos(y, 0, 80'001)) return 1;
        const auto v = kk.tur(SensorTuru::manyetometre);
        DOGRULA(d, v.size() == 9'999 || v.size() == 10'000, "100 ppm kaymada örnek sayısı");
        const auto& son = v[9'990];
        DOGRULA(d, std::llabs(son.hdr.t_meas_ns - static_cast<std::int64_t>(9'990.0 * 1.0001e7 + 0.5)) <= 625'000,
                "saat kayması uygulanmadı");
    }
    return d.hata;
}

// DVL nedensel zinciri (05 §2.3.5): t_tx < t_meas < t_hazır ≤ t_teslim; t_meas − t_tx = r̄/c;
// t_teslim − t_meas = r̄/c + T_işlem + T_iletim; ping aralığı moda göre.
int test_zaman_dvl()
{
    Denetci d;
    SensorKumesiAyari a = ideal_ayar();
    yalniz(a, {SensorTuru::dvl});
    Kosu k;
    if (!k.kur(a)) return 1;
    const Vek3 p{0, 0, 1.0};
    if (!k.kos(durgun(p, {1, 0, 0, 0}), 0, 16'000)) return 1;   // 20 s
    const double c = 1482.3;
    const double r = (3.0 - 1.14) / std::cos(0.39269908169872414);
    const auto tx_meas = std::llround(r / c * 1e9);
    const auto iletim = std::llround(120.0 * 10.0 / 115200.0 * 1e9);
    const auto v = k.tur(SensorTuru::dvl);
    DOGRULA(d, v.size() > 50, "DVL kaydı az");
    std::int64_t onceki_tx = -1;
    int mod2 = 0, mod1 = 0;
    for (std::size_t i = 0; i < v.size(); ++i) {
        const auto& h = v[i].hdr;
        DOGRULA(d, h.seq == i, "DVL seq kesintili");
        DOGRULA(d, h.t_tx_ns % kAdim == 0, "t_tx adım sınırında değil");
        DOGRULA(d, h.t_tx_ns < h.t_meas_ns && h.t_meas_ns < h.t_hazir_ns && h.t_hazir_ns <= ust_kontrol(h.t_teslim_ns),
                "DVL nedensellik");
        DOGRULA(d, h.t_meas_ns - h.t_tx_ns == tx_meas, "t_meas − t_tx = r̄/c");
        DOGRULA(d, h.t_teslim_ns - h.t_meas_ns == tx_meas + 5'000'000 + iletim,
                "t_teslim − t_meas = r̄/c + T_işlem + T_iletim");
        if (onceki_tx >= 0) {
            const std::int64_t ar = h.t_tx_ns - onceki_tx;
            if (ar >= 166'000'000 && ar <= 201'000'000) ++mod2;
            if (ar >= 99'000'000 && ar <= 101'000'000) ++mod1;
        }
        onceki_tx = h.t_tx_ns;
    }
    // İrtifa ≈ 1,86 m: mod 2'de (1,5–14 m, 5–6 Hz) kilitlenir, 10 s sonra yakın mod denemesiyle
    // mod 1'e (0,3–3 m, 10 Hz) geçer (05 §2.3.5).
    std::printf("  mod 2 aralığı %d ping, mod 1 aralığı %d ping\n", mod2, mod1);
    DOGRULA(d, mod2 > 40 && mod1 > 80, "ping aralıkları mod tablosuyla uyuşmuyor");
    return d.hata;
}

}  // namespace ida::sensor::test
