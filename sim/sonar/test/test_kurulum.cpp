// Kurulum hataları ve kabul 7 (W4-K1: sensör kümesinin ses hızı).
#include "test_destek.hpp"

#include "ida/sensor/sensor_kumesi.hpp"
#include "ida/sonar/akustik.hpp"

namespace ida::sonar::test {

int test_kurulum_hatalari()
{
    Denetci d;
    auto sahne = havuz_sahnesi();
    {
        SonarKumesi k;
        DOGRULA(d, k.kur(ida0_sonar_kumesi(), sahne).tamam, "İDA-0 sonar kümesi kurulur");
        DOGRULA(d, std::abs(k.ses_hizi() - ses_hizi_chen_millero(20.0, 0.0, 0.0)) < 1e-9, "formülden ses hızı");
    }
    auto red = [&](const SonarKumesiAyari& a, const char* ad, std::shared_ptr<const Sahne> s = nullptr) {
        SonarKumesi k;
        const KurulumSonucu r = k.kur(a, s ? s : sahne);
        DOGRULA(d, !r.tamam && !k.kuruldu(), std::string("reddedilmeli: ") + ad);
    };
    {
        SonarKumesi k;
        DOGRULA(d, !k.kur(ida0_sonar_kumesi(), nullptr).tamam, "sahnesiz ret");
    }
    auto a = ida0_sonar_kumesi();
    a.kademe = SonarKademesi::tam;
    red(a, "tam kademe (bu dilim)");
    a = ida0_sonar_kumesi();
    a.sonarlar[1].kimlik = 6;
    red(a, "yinelenen kimlik");
    a = ida0_sonar_kumesi();
    a.sonarlar[0].ad = "x_p";
    red(a, "x_ önekli ad");
    a = ida0_sonar_kumesi();
    a.sonarlar[0].inst = 4;
    red(a, "inst 4");
    a = ida0_sonar_kumesi();
    a.sonarlar[1].montaj.yonelim = {1.0, 0.1, 0.0, 0.0};
    red(a, "birim olmayan kuaterniyon (A5-K32)");
    a = ida0_sonar_kumesi();
    a.sonarlar[1].ayar = Ping1dAyar{};
    red(a, "tür ile ayar uyuşmuyor");
    a = ida0_sonar_kumesi();
    std::get<Ping360Yapilandirmasi>(a.sonarlar[1].yapilandirma).n_samples = 100;
    red(a, "n_samples < 200");
    a = ida0_sonar_kumesi();
    std::get<Ping360Yapilandirmasi>(a.sonarlar[1].yapilandirma).sample_period = 79;
    red(a, "sample_period < 80");
    a = ida0_sonar_kumesi();
    std::get<Ping360Ayar>(a.sonarlar[1].ayar).t_m0_ns = -1;
    red(a, "negatif süre (A5-K48)");
    a = ida0_sonar_kumesi();
    std::get<Ping1dAyar>(a.sonarlar[0].ayar).ping_araligi_ns = 0;
    red(a, "ping aralığı 0 (A5-K48)");
    a = ida0_sonar_kumesi();
    std::get<Ping1dYapilandirmasi>(a.sonarlar[0].yapilandirma).scan_length = 120.0;
    red(a, "Ping1D pencere > 100 m");
    a = ida0_sonar_kumesi();
    a.ortam.su.ses_hizi = 1200.0;
    red(a, "ses hızı aralık dışı (A5 §3.9)");
    a = ida0_sonar_kumesi();
    a.ortam.su.tuzluluk = 0.06;
    red(a, "tuzluluk aralık dışı");
    // Kapalı sonar doğrulanmaz ve üretmez (A5-K49).
    a = ida0_sonar_kumesi();
    a.sonarlar[1].etkin = false;
    std::get<Ping360Yapilandirmasi>(a.sonarlar[1].yapilandirma).n_samples = 100;
    {
        SonarKumesi k;
        DOGRULA(d, k.kur(a, sahne).tamam, "kapalı sonarın alanları denetlenmez");
    }
    // Adım sırası.
    {
        SonarKumesi k;
        k.kur(ida0_sonar_kumesi(), sahne);
        DOGRULA(d, k.adim(durum(5, {5, 0, 1}, {1, 0, 0, 0}), 5) == AdimSonucu::tamam, "ilk adım herhangi numara");
        DOGRULA(d, k.adim(durum(7, {5, 0, 1}, {1, 0, 0, 0}), 7) == AdimSonucu::sira_disi, "adım atlanamaz");
        DOGRULA(d, k.adim(durum(6, {5, 0, 1}, {1, 0, 0, 0}), 9) == AdimSonucu::tutarsiz, "durum ile numara tutarsız");
    }
    return d.hata;
}

int test_w4k1_sensor_kumesi()
{
    Denetci d;
    // W4-K1: `ornek-havuz` ses hızı vermez; DVL etkin sensör kümesi artık kurulur ve formülün
    // değerini kullanır. A5'ten sabit `c` verilince o kullanılır. Ses hızı DVL kaydından okunur:
    // kilitliyken t_meas − t_tx = r̄ / c (05 §2.3.5; r̄ geçerli ışın menzillerinin ortalaması).
    auto dvl_c = [&](const std::optional<double>& sabit, double& c) {
        sensor::SensorKumesiAyari a = sensor::ida0_sensor_kumesi();
        a.ortam.su.ses_hizi = sabit;
        a.ic_sicaklik.etkin = false;
        for (auto& s : a.sensorler) {
            s.etkin = s.tur == sensor::SensorTuru::dvl;
            if (auto* v = std::get_if<sensor::DvlAyar>(&s.ayar)) {
                v->sigma0 = v->k_r = v->k_u = 0.0;
                v->uzun_donem_olcek_siniri = 0.0;
            }
        }
        sensor::SensorKumesi k;
        const sensor::KurulumSonucu r = k.kur(a);
        DOGRULA(d, r.tamam, "DVL etkin, ses hızı verilmemiş sensör kümesi kurulur: " + r.hata);
        if (!r.tamam) return false;
        std::vector<sensor::Olcum> v;
        for (std::uint64_t n = 0; n < 4000; ++n) {
            k.adim(durum(n, {5.0, 0.0, 1.0}, {1.0, 0.0, 0.0, 0.0}), n);
            k.bosalt(std::numeric_limits<std::int64_t>::max(), v);
        }
        for (auto it = v.rbegin(); it != v.rend(); ++it) {
            const auto& y = std::get<sensor::DvlOlcumu>(it->yuk);
            if (y.beam_valid != 0x0F || it->hdr.t_meas_ns == it->hdr.t_tx_ns) continue;
            double r_ort = 0.0;
            for (float b : y.beam_dist) r_ort += static_cast<double>(b) / 4.0;
            c = r_ort / (static_cast<double>(it->hdr.t_meas_ns - it->hdr.t_tx_ns) * 1e-9);
            return true;
        }
        DOGRULA(d, false, "kilitli DVL kaydı yok");
        return false;
    };
    // Tolerans: t_meas ns'ye yuvarlanır (Δt ≈ 1,3 ms → 4·10⁻⁷ bağıl) ve menziller binary32
    // (6·10⁻⁸ bağıl): ≈ 0,001 m/s. Eski sabit 1482,3 ile formül (1482,36) arasındaki fark 0,06.
    constexpr double kTol = 0.003;
    double c = 0.0;
    if (dvl_c(std::nullopt, c)) {
        const double beklenen = ses_hizi_chen_millero(20.0, 0.0, 0.0);
        std::printf("  formül: DVL c %.4f, beklenen %.4f\n", c, beklenen);
        YAKIN(d, c, beklenen, kTol, "sensör kümesi formülü kullanır (W4-K1)");
    }
    if (dvl_c(1450.0, c)) {
        std::printf("  sabit: DVL c %.4f\n", c);
        YAKIN(d, c, 1450.0, kTol, "A5 sabit c kullanılır");
    }
    return d.hata;
}

}  // namespace ida::sonar::test
