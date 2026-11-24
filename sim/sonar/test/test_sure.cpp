// Kabul 8: süre (ölçüm; eşik yok). Sonuç makineye ve derleme türüne bağlıdır; rapordaki sayılar
// host-release ile alınır.
#include "test_destek.hpp"

#include <chrono>

namespace ida::sonar::test {

namespace {

struct Istatistik {
    double medyan = 0.0, p99 = 0.0, en_buyuk = 0.0;
};

// p99 en yakın sıra yöntemiyle (MP7 §4.2: 200 pingte 198. değer).
Istatistik ozet(std::vector<double> v)
{
    std::sort(v.begin(), v.end());
    auto sira = [&](double p) {
        const std::size_t k = static_cast<std::size_t>(std::ceil(p * static_cast<double>(v.size())));
        return v[std::min(std::max<std::size_t>(k, 1) - 1, v.size() - 1)];
    };
    return {sira(0.5), sira(0.99), v.back()};
}

double ms_olc(const std::chrono::steady_clock::time_point& a, const std::chrono::steady_clock::time_point& b)
{
    return static_cast<double>(std::chrono::duration_cast<std::chrono::nanoseconds>(b - a).count()) * 1e-6;
}

}  // namespace

int test_sure()
{
    // Ping başına süre: MP7'nin sade satırına karşılık (5 000 ışın, 200 ping, 10 ısınma, kafa
    // açısı 2k gradyan), orta sahnenin çözümsel karşılığında. Ping süresi, pingin atıldığı
    // `adim` çağrısının süresidir (ışın izleme, biriktirme, benek, alıcı zinciri; önceki satırın
    // teslimi ve arıza zinciri dahil).
    auto sahne = orta_sahne();
    for (std::uint16_t sp : {std::uint16_t{311}, std::uint16_t{2222}}) {
        auto a = kume_ayari({ping360(p360_yap(sp, 1200, 0, 0, 2))});
        a.tani = false;
        SonarKumesi k;
        if (!k.kur(a, sahne).tamam) return 1;
        std::vector<double> sure;
        std::vector<SonarOlcumu> cop;
        std::uint64_t n = 0;
        while (sure.size() < 200) {
            const fizik::DurumAnlik d = durum(n, {0.0, 0.0, 6.0}, {1.0, 0.0, 0.0, 0.0});
            const std::uint64_t once = k.sayaclar().ping;
            const auto t0 = std::chrono::steady_clock::now();
            k.adim(d, n);
            const auto t1 = std::chrono::steady_clock::now();
            if (k.sayaclar().ping != once && once >= 10) sure.push_back(ms_olc(t0, t1));
            cop.clear();
            k.bosalt(std::numeric_limits<std::int64_t>::max(), cop);
            ++n;
        }
        const Istatistik s = ozet(sure);
        std::printf("  Ping360 %s (sp %u), 5000 ışın: ping başına medyan %.3f ms, p99 %.3f ms, en büyük %.3f ms\n",
                    sp == 311 ? "7 m" : "50 m", static_cast<unsigned>(sp), s.medyan, s.p99, s.en_buyuk);
    }
    // Fizik adımı başına maliyet: İDA-0 sonar kümesi (Ping1D otomatik menzil + Ping360 7 m tam tur),
    // örnek havuzda, 20 s sim; araç yavaş döner.
    {
        auto a = ida0_sonar_kumesi();
        std::get<Ping360Yapilandirmasi>(a.sonarlar[1].yapilandirma) = p360_yap(311);
        SonarKumesi k;
        if (!k.kur(a, havuz_sahnesi()).tamam) return 1;
        const std::uint64_t adimlar = 16000;
        std::vector<double> sure;
        sure.reserve(adimlar);
        std::vector<SonarOlcumu> cop;
        double toplam_ms = 0.0;
        for (std::uint64_t n = 0; n < adimlar; ++n) {
            const double t = static_cast<double>(n) * 1.25e-3;
            const fizik::DurumAnlik d = durum(n, {8.0, 0.0, 1.5}, euler_q(0.0, 0.0, 0.05 * t));
            const auto t0 = std::chrono::steady_clock::now();
            k.adim(d, n);
            const auto t1 = std::chrono::steady_clock::now();
            sure.push_back(ms_olc(t0, t1));
            toplam_ms += sure.back();
            cop.clear();
            k.bosalt(std::numeric_limits<std::int64_t>::max(), cop);
        }
        const Istatistik s = ozet(sure);
        std::printf("  İDA-0 sonar kümesi: %llu adım, adım başına ortalama %.4f ms, medyan %.4f ms, p99 %.3f ms, "
                    "en büyük %.3f ms; %llu ping\n",
                    static_cast<unsigned long long>(adimlar), toplam_ms / static_cast<double>(adimlar), s.medyan,
                    s.p99, s.en_buyuk, static_cast<unsigned long long>(k.sayaclar().ping));
    }
    return 0;
}

}  // namespace ida::sonar::test
