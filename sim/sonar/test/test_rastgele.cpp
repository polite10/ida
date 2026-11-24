// Kabul 5: rastgelelik ve belirlenimcilik.
#include "test_destek.hpp"

namespace ida::sonar::test {

namespace {

bool ayni(const SonarOlcumu& a, const SonarOlcumu& b)
{
    const auto& x = a.hdr;
    const auto& y = b.hdr;
    if (x.sensor_id != y.sensor_id || x.seq != y.seq || x.t_meas_ns != y.t_meas_ns || x.t_tx_ns != y.t_tx_ns ||
        x.t_hazir_ns != y.t_hazir_ns || x.t_teslim_ns != y.t_teslim_ns || x.flags != y.flags) {
        return false;
    }
    if (a.yuk.index() != b.yuk.index()) return false;
    if (const auto* p = std::get_if<Ping360Olcumu>(&a.yuk)) {
        const auto& q = std::get<Ping360Olcumu>(b.yuk);
        return p->data == q.data && p->angle_grad == q.angle_grad && p->tani.ham == q.tani.ham &&
               p->tani.yogunluk == q.tani.yogunluk;
    }
    const auto& p = std::get<Ping1dOlcumu>(a.yuk);
    const auto& q = std::get<Ping1dOlcumu>(b.yuk);
    return p.distance == q.distance && p.confidence == q.confidence && p.tx_offset_ns == q.tx_offset_ns &&
           p.profil == q.profil && p.tani.yogunluk == q.tani.yogunluk;
}

SonarKumesiAyari karisik(std::uint64_t tohum)
{
    auto s3 = ping360(p360_yap(311));
    std::get<Ping360Ayar>(s3.ayar).gurultu_tabani = 1e-12;
    std::get<Ping360Ayar>(s3.ayar).yakin_alan_genligi = 1e-9;
    Ping1dYapilandirmasi w;
    return kume_ayari({ping1d(w), s3}, tohum);
}

std::vector<SonarOlcumu> kos(const SonarKumesiAyari& a, std::uint64_t adim = 400)
{
    Kosu k;
    if (!k.kur(a, havuz_sahnesi()) || !k.kos(durgun({5.0, 1.0, 1.2}, euler_q(0.02, -0.01, 0.3)), 0, adim)) {
        std::abort();
    }
    return k.kayit;
}

}  // namespace

int test_belirlenimcilik()
{
    Denetci d;
    // Aynı tohumla iki koşu bit düzeyinde aynı (aynı ikili, aynı platform; 05 §2.6).
    const auto a = kos(karisik(7));
    const auto b = kos(karisik(7));
    DOGRULA(d, a.size() == b.size() && a.size() > 30, "aynı kayıt sayısı");
    bool hepsi = a.size() == b.size();
    for (std::size_t i = 0; hepsi && i < a.size(); ++i) hepsi = ayni(a[i], b[i]);
    DOGRULA(d, hepsi, "aynı tohum: bit düzeyinde aynı");
    // Farklı tohum: benek (ve ışın titreşimi) farklı; geometri aynı.
    const auto c = kos(karisik(8));
    int farkli = 0;
    for (std::size_t i = 0; i < std::min(a.size(), c.size()); ++i) {
        if (const auto* p = std::get_if<Ping360Olcumu>(&a[i].yuk)) {
            const auto* q = std::get_if<Ping360Olcumu>(&c[i].yuk);
            if (q != nullptr && p->data != q->data) ++farkli;
        }
    }
    DOGRULA(d, farkli > 20, "farklı tohum farklı benek");
    // Benek istatistiği: tek bir kutuda Ĩ / Ĩ_ham üstel (ortalama 1); çok sayıda kutuda ortalama
    // ≈ 1 ve varyans ≈ 1 (05 §2.4.3; Rayleigh genliği).
    // Gürültü tabanı kapalı koşu: Ĩ / Ĩ_ham tam olarak benek çarpanıdır.
    auto sessiz = karisik(7);
    std::get<Ping360Ayar>(sessiz.sonarlar[1].ayar).gurultu_tabani = 0.0;
    std::get<Ping360Ayar>(sessiz.sonarlar[1].ayar).yakin_alan_genligi = 0.0;
    const auto e = kos(sessiz);
    double s1 = 0, s2 = 0;
    long n = 0;
    for (const auto& o : e) {
        const auto* p = std::get_if<Ping360Olcumu>(&o.yuk);
        if (p == nullptr) continue;
        for (std::size_t i = 0; i < p->tani.ham.size(); ++i) {
            if (p->tani.ham[i] > 0.0) {
                const double x = p->tani.yogunluk[i] / p->tani.ham[i];
                s1 += x;
                s2 += x * x;
                ++n;
            }
        }
    }
    const double ort = s1 / static_cast<double>(n);
    const double var = s2 / static_cast<double>(n) - ort * ort;
    std::printf("  benek: %ld kutu, ortalama %.3f, varyans %.3f\n", n, ort, var);
    DOGRULA(d, n > 5000, "yeterli kutu");
    YAKIN(d, ort, 1.0, 0.05, "benek ortalaması 1");
    YAKIN(d, var, 1.0, 0.15, "benek varyansı 1 (üstel)");
    return d.hata;
}

int test_sonar_bagimsizligi()
{
    Denetci d;
    // Bir sonarın açılıp kapanması ötekilerin dizisini değiştirmez (akışlar sonar adıyla
    // anahtarlı; A5-K84).
    const auto tam = kos(karisik(3));
    auto a = karisik(3);
    a.sonarlar[0].etkin = false;
    const auto yalniz360 = kos(a);
    auto b = karisik(3);
    b.sonarlar.erase(b.sonarlar.begin());
    auto ek = ping360(p360_yap(500, 800, 100, 100, 3), "baska", 9);
    ek.inst = 1;
    b.sonarlar.push_back(ek);
    const auto ekli = kos(b);
    auto sec = [](const std::vector<SonarOlcumu>& v, std::uint16_t k) {
        std::vector<SonarOlcumu> r;
        for (const auto& o : v) {
            if (o.hdr.sensor_id == k) r.push_back(o);
        }
        return r;
    };
    const auto x = sec(tam, 7), y = sec(yalniz360, 7), z = sec(ekli, 7);
    DOGRULA(d, !x.empty() && x.size() == y.size() && x.size() == z.size(), "aynı satır sayısı");
    bool esit = x.size() == y.size() && x.size() == z.size();
    for (std::size_t i = 0; esit && i < x.size(); ++i) esit = ayni(x[i], y[i]) && ayni(x[i], z[i]);
    DOGRULA(d, esit, "Ping360 dizisi öteki sonarlardan bağımsız");
    DOGRULA(d, !sec(ekli, 9).empty() && sec(yalniz360, 6).empty(), "ekli sonar üretir, kapalı üretmez");
    return d.hata;
}

}  // namespace ida::sonar::test
