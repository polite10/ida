// Kurulum hataları (A5-K41–K49 ve bu kümenin koşulları) ve adım başına süre ölçümü (kabul 8).
#include "test_destek.hpp"

#include <chrono>

namespace ida::sensor::test {

int test_kurulum_hatalari()
{
    Denetci d;
    auto red = [&](const SensorKumesiAyari& a, const char* ad) {
        SensorKumesi k;
        const KurulumSonucu s = k.kur(a);
        DOGRULA(d, !s.tamam && !k.kuruldu(), std::string("reddedilmedi: ") + ad);
        if (!s.tamam) std::printf("  %s → %s\n", ad, s.hata.c_str());
    };
    {
        SensorKumesi k;
        const KurulumSonucu s = k.kur(ida0_sensor_kumesi());
        DOGRULA(d, s.tamam && s.uyarilar.size() == 3, "İDA-0 kümesi kurulmadı ya da kapsam dışı uyarısı eksik");
        DOGRULA(d, k.adim(durum(5, {}, {}, {}, {1, 0, 0, 0}, {}), 4) == AdimSonucu::tutarsiz, "tutarsız adım");
        DOGRULA(d, k.adim(durum(4, {}, {}, {}, {1, 0, 0, 0}, {}), 4) == AdimSonucu::tamam, "ilk adım");
        DOGRULA(d, k.adim(durum(6, {}, {}, {}, {1, 0, 0, 0}, {}), 6) == AdimSonucu::sira_disi, "atlanan adım");
        SensorKumesi bos;
        DOGRULA(d, bos.adim(durum(0, {}, {}, {}, {1, 0, 0, 0}, {}), 0) == AdimSonucu::kurulmadi, "kurulmamış");
    }
    SensorKumesiAyari a = ida0_sensor_kumesi();
    sensor_ayari(a, SensorTuru::basinc).hiz_hz = 26.0;
    red(a, "basınç 26 Hz (A5-K42)");
    a = ida0_sensor_kumesi();
    sensor_ayari(a, SensorTuru::basinc).hiz_hz = 25.9;
    {
        SensorKumesi k;
        DOGRULA(d, k.kur(a).tamam, "basınç 25,9 Hz reddedildi (A5-K42)");
    }
    a = ida0_sensor_kumesi();
    sensor_ayari(a, SensorTuru::imu).hiz_hz = 100.0;
    red(a, "imu hiz_hz (A5-K41)");
    a = ida0_sensor_kumesi();
    sensor_ayari(a, SensorTuru::dvl).hiz_hz = 5.0;
    red(a, "dvl hiz_hz (A5-K41)");
    a = ida0_sensor_kumesi();
    a.ortam.manyetik_alan.reset();
    red(a, "manyetik alan yok (A5-K44)");
    a = ida0_sensor_kumesi();
    a.ortam.orijin.reset();
    red(a, "orijin yok (A5-K45)");
    a = ida0_sensor_kumesi();
    a.ortam.orijin.reset();
    sensor_ayari(a, SensorTuru::gnss).etkin = false;
    ayar_govdesi<ImuAyar>(a, SensorTuru::imu).dunya_donusu_etkin = false;
    {
        SensorKumesi k;
        DOGRULA(d, k.kur(a).tamam, "orijinsiz, GNSS kapalı ve dünya dönüşü kapalıyken reddedildi (A5-K45)");
    }
    a = ida0_sensor_kumesi();
    ayar_govdesi<ImuAyar>(a, SensorTuru::imu).suzgec_etkin = true;
    red(a, "süzgeç τ yok (A5-K46)");
    a = ida0_sensor_kumesi();
    sensor_ayari(a, SensorTuru::sizinti).inst = 4;
    red(a, "inst 4 (A4-K30)");
    a = ida0_sensor_kumesi();
    sensor_ayari(a, SensorTuru::sizinti).kimlik = 1;
    red(a, "yinelenen kimlik");
    a = ida0_sensor_kumesi();
    sensor_ayari(a, SensorTuru::imu).montaj.yonelim = {1.0, 0.01, 0.0, 0.0};
    red(a, "birim olmayan kuaterniyon (A5-K32)");
    a = ida0_sensor_kumesi();
    a.ortam.su.ses_hizi.reset();
    {
        SensorKumesi k;
        DOGRULA(d, k.kur(a).tamam, "ses hızı yokken DVL kurulur: formülden (W4-K1)");
    }
    a = ida0_sensor_kumesi();
    ayar_govdesi<DvlAyar>(a, SensorTuru::dvl).su_takibi = true;
    red(a, "su takibi (bu dilimde yok)");
    a = ida0_sensor_kumesi();
    sensor_ayari(a, SensorTuru::imu).ayar = BasincAyar{};
    red(a, "tür ile ayar uyuşmuyor");
    a = ida0_sensor_kumesi();
    a.kontrol_periyodu_ns = 4'000'000;
    red(a, "kontrol periyodu adımın katı değil");
    a = ida0_sensor_kumesi();
    sensor_ayari(a, SensorTuru::imu).ad = "x_imu";
    red(a, "x_ önekli ad (A5-K07)");
    return d.hata;
}

// Bir fizik adımı başına sensör maliyeti (ölçüm; eşik yok). İDA-0 kümesi, yavaş dalış ve
// dönüş, iticiler çalışıyor. Sonuç makineye bağlıdır.
int test_sure()
{
    Kosu k;
    if (!k.kur(ida0_sensor_kumesi())) return 1;
    const std::uint64_t n = 80'000;   // 100 s sim
    std::vector<fizik::DurumAnlik> durumlar;
    durumlar.reserve(n);
    for (std::uint64_t i = 0; i < n; ++i) {
        const double t = sn(i);
        fizik::DurumAnlik s = durum(i, {0, 0, 0.1 + 0.01 * t}, {0, 0, 0.01}, {}, euler_q(0.02, -0.01, 0.05 * t),
                                    {0, 0, 0.05});
        s.itki_istegi = {3, 3, 3, 3, 8, 8, 8, 8};
        durumlar.push_back(s);
    }
    std::vector<Olcum> cikti;
    cikti.reserve(64);
    const auto bas = std::chrono::steady_clock::now();
    for (std::uint64_t i = 0; i < n; ++i) {
        k.kume.adim(durumlar[i], i);
        if (i % 4 == 0) {
            cikti.clear();
            k.kume.bosalt(static_cast<std::int64_t>(i) * kAdim, cikti);
        }
    }
    const auto son = std::chrono::steady_clock::now();
    const double ns = static_cast<double>(std::chrono::duration_cast<std::chrono::nanoseconds>(son - bas).count());
    std::printf("  %llu adım, toplam %.3f ms, adım başına %.0f ns (%llu kayıt üretildi)\n",
                static_cast<unsigned long long>(n), ns * 1e-6, ns / static_cast<double>(n),
                static_cast<unsigned long long>(k.kume.sayaclar().uretilen));
    return 0;
}

}  // namespace ida::sensor::test
