// Kabul 6: arızalar (gövdeli türlerin etkisi; kalkınca temel davranışa dönüş; desenler; retler).
#include "test_destek.hpp"

#include "ida/sensor/rastgele.hpp"

namespace ida::sonar::test {

namespace {

using sensor::ArizaTuru;
using sensor::ZamanDeseniTuru;

constexpr std::uint64_t kAdimSayisi = 480;   // 0,6 s; 7 m Ping360 ≈ 60 satır

SonarKumesiAyari ayar()
{
    auto a = kume_ayari({ping360(p360_yap(311))});
    a.sade_isin_sayisi = 300;
    return a;
}

std::vector<SonarOlcumu> kos(const std::vector<SensorArizasi>& arizalar, SonarSayaclari* say = nullptr)
{
    Kosu k;
    if (!k.kur(ayar(), havuz_sahnesi())) std::abort();
    for (const auto& f : arizalar) {
        const ArmSonucu r = k.kume.arm(f);
        if (!r.tamam) {
            std::printf("  arm reddi: %s\n", r.hata.c_str());
            std::abort();
        }
    }
    if (!k.kos(durgun({5.0, 0.0, 1.0}), 0, kAdimSayisi)) std::abort();
    if (say != nullptr) *say = k.kume.sayaclar();
    return k.kayit;   // t_teslim sırası
}

SensorArizasi ariza(const char* kimlik, ArizaTuru tur, std::int64_t bas, std::int64_t sure)
{
    SensorArizasi f;
    f.kimlik = kimlik;
    f.hedef = "p360";
    f.tur = tur;
    f.zaman_ns = bas;
    f.sure_ns = sure;
    return f;
}

bool yuk_ayni(const SonarOlcumu& a, const SonarOlcumu& b)
{
    const auto& p = p360(a);
    const auto& q = p360(b);
    return p.data == q.data && p.angle_grad == q.angle_grad;
}

const SonarOlcumu* seq_bul(const std::vector<SonarOlcumu>& v, std::uint32_t s)
{
    for (const auto& o : v) {
        if (o.hdr.seq == s) return &o;
    }
    return nullptr;
}

constexpr std::int64_t kBas = 100'000'000;    // 100 ms
constexpr std::int64_t kSure = 150'000'000;   // 150 ms
bool pencerede(const SonarOlcumu& o) { return o.hdr.t_meas_ns >= kBas && o.hdr.t_meas_ns < kBas + kSure; }

}  // namespace

int test_ariza_etkileri()
{
    Denetci d;
    const auto temel = kos({});
    DOGRULA(d, temel.size() > 50, "temel satırlar");
    // Her arızada: pencere dışındaki kayıtlar temel koşuyla aynı (başlık ve yük); pencere
    // içindekiler türün etkisini taşır ve arıza maskesi biti kurulu.
    auto disari = [&](const std::vector<SonarOlcumu>& v, const char* ad) {
        for (const auto& o : v) {
            if (pencerede(o)) continue;
            const SonarOlcumu* t = seq_bul(temel, o.hdr.seq);
            DOGRULA(d, t != nullptr && yuk_ayni(o, *t) && o.hdr.t_meas_ns == t->hdr.t_meas_ns &&
                           o.hdr.flags == t->hdr.flags && o.hdr.t_teslim_ns == t->hdr.t_teslim_ns &&
                           o.hdr.ariza_maskesi == 0,
                    std::string(ad) + ": arıza dışında temel davranış");
        }
    };
    {   // kayıp: pencerede teslim yok, seq boşluğu kalır
        SonarSayaclari s;
        const auto v = kos({ariza("k", ArizaTuru::kayip, kBas, kSure)}, &s);
        std::size_t pencere = 0;
        for (const auto& o : temel) pencere += pencerede(o) ? 1u : 0u;
        DOGRULA(d, v.size() + pencere == temel.size() && s.kayip == pencere && pencere > 5, "kayıp sayısı");
        for (const auto& o : v) DOGRULA(d, !pencerede(o), "kayıp penceresinde kayıt yok");
        disari(v, "kayıp");
    }
    {   // donma: yük son arızasız çıktıyla tekrarlanır; t_meas ve seq ilerler
        const auto v = kos({ariza("d", ArizaTuru::donma, kBas, kSure)});
        const SonarOlcumu* son = nullptr;
        for (const auto& o : temel) {
            if (o.hdr.t_meas_ns < kBas) son = &o;
        }
        int n = 0;
        for (const auto& o : v) {
            if (!pencerede(o)) continue;
            ++n;
            const SonarOlcumu* t = seq_bul(temel, o.hdr.seq);
            DOGRULA(d, son != nullptr && yuk_ayni(o, *son), "donma: önceki yük");
            DOGRULA(d, t != nullptr && o.hdr.t_meas_ns == t->hdr.t_meas_ns, "donma: t_meas ilerler");
            DOGRULA(d, (o.hdr.ariza_maskesi & (1u << 6)) != 0, "donma maskesi");
        }
        DOGRULA(d, n > 5 && v.size() == temel.size(), "donma kayıtları");
        disari(v, "donma");
    }
    {   // donma_zaman_damgasi: t_meas da donar
        const auto v = kos({ariza("z", ArizaTuru::donma_zaman_damgasi, kBas, kSure)});
        const SonarOlcumu* son = nullptr;
        for (const auto& o : temel) {
            if (o.hdr.t_meas_ns < kBas) son = &o;
        }
        int n = 0;
        for (const auto& o : v) {
            const SonarOlcumu* t = seq_bul(temel, o.hdr.seq);
            if (t == nullptr || !pencerede(*t)) continue;
            ++n;
            DOGRULA(d, son != nullptr && o.hdr.t_meas_ns == son->hdr.t_meas_ns && yuk_ayni(o, *son),
                    "zaman damgası donar");
            DOGRULA(d, o.hdr.t_teslim_ns == t->hdr.t_teslim_ns, "teslim zamanı ilerler");
        }
        DOGRULA(d, n > 5, "donma_zaman_damgasi kayıtları");
    }
    {   // önceki çıktı yokken donma: arıza bitene dek kayıt yok, sayaç artar
        SonarSayaclari s;
        const auto v = kos({ariza("d0", ArizaTuru::donma, 0, kSure)}, &s);
        for (const auto& o : v) DOGRULA(d, o.hdr.t_meas_ns >= kSure, "önceki çıktı yok: kayıt yok");
        DOGRULA(d, s.donma_onceki_cikti_yok == 1, "donma_onceki_cikti_yok sayacı");
    }
    {   // yanlış geçerlilik ve kendini bildiren
        const auto v = kos({ariza("y", ArizaTuru::yanlis_gecerlilik, kBas, kSure),
                            ariza("b", ArizaTuru::kendini_bildiren, kBas, kSure)});
        for (const auto& o : v) {
            if (!pencerede(o)) continue;
            const SonarOlcumu* t = seq_bul(temel, o.hdr.seq);
            DOGRULA(d, t != nullptr && (o.hdr.flags & sensor::kBayrakGecerli) != (t->hdr.flags & sensor::kBayrakGecerli),
                    "VALID ters");
            DOGRULA(d, (o.hdr.flags & sensor::kBayrakSensorArizasi) != 0, "sensör arızası biti");
            DOGRULA(d, t != nullptr && yuk_ayni(o, *t), "yük değişmez");
        }
        disari(v, "geçerlilik");
    }
    {   // geç teslim: t_teslim + gecikme + titreşim; sıra korunur
        SensorArizasi f = ariza("g", ArizaTuru::gec_teslim, kBas, kSure);
        f.siddet.gecikme_ns = 30'000'000;
        f.siddet.titresim_ns = 10'000'000;
        const auto v = kos({f});
        const sensor::Akis ak(1, "ariza/g");
        std::int64_t onceki = 0;
        for (const auto& o : v) {
            const SonarOlcumu* t = seq_bul(temel, o.hdr.seq);
            if (t == nullptr) continue;
            if (pencerede(o)) {
                const double u = sensor::duzgun_donusum(ak.sozcukler(o.hdr.seq, sensor::c2_kodla(0, 2, 0))[0]);
                const std::int64_t j = static_cast<std::int64_t>(std::floor(u * 20'000'001.0)) - 10'000'000;
                const std::int64_t beklenen = t->hdr.t_teslim_ns + 30'000'000 + j;
                DOGRULA(d, o.hdr.t_teslim_ns == std::max(beklenen, onceki), "geç teslim (A5 Ek D.4 titreşimi)");
                DOGRULA(d, yuk_ayni(o, *t) && o.hdr.t_meas_ns == t->hdr.t_meas_ns, "yük ve t_meas aynı");
            }
            DOGRULA(d, o.hdr.t_teslim_ns >= onceki, "sıra korunur");
            onceki = o.hdr.t_teslim_ns;
        }
    }
    {   // disarm: sonraki adım sınırında kalkar
        Kosu k;
        if (!k.kur(ayar(), havuz_sahnesi())) return 1;
        const ArmSonucu r = k.kume.arm(ariza("x", ArizaTuru::kayip, 0, 0));
        DOGRULA(d, r.tamam && k.kume.active().size() == 1, "süresiz arıza etkin");
        k.adim_oncesi = [&](std::uint64_t n, SonarKumesi& km) {
            if (n == 80) DOGRULA(d, km.disarm(r.tutamac), "disarm");
        };
        if (!k.kos(durgun({5.0, 0.0, 1.0}), 0, kAdimSayisi)) return 1;
        DOGRULA(d, k.kume.active().empty(), "disarm sonrası etkin arıza yok");
        for (const auto& o : k.kayit) {
            DOGRULA(d, o.hdr.t_meas_ns >= 80 * kAdim, "disarm öncesi kayıt yok");
            const SonarOlcumu* t = seq_bul(temel, o.hdr.seq);
            DOGRULA(d, t != nullptr && yuk_ayni(o, *t), "disarm sonrası temel");
        }
    }
    return d.hata;
}

int test_ariza_desenleri()
{
    Denetci d;
    const auto temel = kos({});
    // Bernoulli: kayıt `u(w0) < p` ise kaybolur (A5 Ek D.4: ariza/<kimlik>, n = seq, c2 = 0xFF << 24).
    {
        SensorArizasi f = ariza("bern", ArizaTuru::kayip, 0, 0);
        f.zaman_deseni.tur = ZamanDeseniTuru::bernoulli;
        f.zaman_deseni.p = 0.4;
        const auto v = kos({f});
        const sensor::Akis ak(1, "ariza/bern");
        int kayip = 0;
        for (const auto& t : temel) {
            const bool beklenen = ak.duzgun(t.hdr.seq, sensor::c2_kodla(0xFF, 0, 0)) < 0.4;
            const bool var = seq_bul(v, t.hdr.seq) != nullptr;
            DOGRULA(d, var != beklenen, "Bernoulli çekilişi");
            kayip += beklenen ? 1 : 0;
        }
        DOGRULA(d, kayip > 5 && kayip < static_cast<int>(temel.size()) - 5, "Bernoulli karışık sonuç");
    }
    // Gilbert–Elliott: kayıt t_meas "kötü" kalıştaysa kaybolur (A5 Ek D.5).
    {
        SensorArizasi f = ariza("ge", ArizaTuru::kayip, 0, 0);
        f.zaman_deseni.tur = ZamanDeseniTuru::gilbert_elliott;
        f.zaman_deseni.t_iyi_ns = 60'000'000;
        f.zaman_deseni.t_kotu_ns = 40'000'000;
        const auto v = kos({f});
        const sensor::Akis g(1, "ariza/ge/gecis");
        std::vector<std::int64_t> anlar{0};
        while (anlar.back() <= static_cast<std::int64_t>(kAdimSayisi) * kAdim) {
            const std::size_t i = anlar.size() - 1;
            const double m = (i % 2 == 0) ? 60e6 : 40e6;
            const double u = sensor::duzgun_donusum(g.sozcukler(i, 0)[0]);
            anlar.push_back(anlar.back() + std::max<std::int64_t>(1, static_cast<std::int64_t>(std::ceil(-std::log(u) * m))));
        }
        int kotu = 0;
        for (const auto& t : temel) {
            const auto it = std::upper_bound(anlar.begin(), anlar.end(), t.hdr.t_meas_ns);
            const bool k_durum = ((it - anlar.begin()) - 1) % 2 == 1;
            DOGRULA(d, (seq_bul(v, t.hdr.seq) == nullptr) == k_durum, "GE kalışı");
            kotu += k_durum ? 1 : 0;
        }
        DOGRULA(d, kotu > 3, "GE kötü kalış var");
    }
    // Retler: gürültü artışı sonarda tanımsız (A5 Tablo 3.10-A, K65); sayısal arızalar kanal
    // numaralandırması olmadığı için ve durumlu arızalar bu dilimde yer tutar.
    {
        Kosu k;
        if (!k.kur(ayar(), havuz_sahnesi())) return 1;
        for (ArizaTuru t : {ArizaTuru::gurultu_artisi, ArizaTuru::ofset, ArizaTuru::olcek, ArizaTuru::sicrama,
                            ArizaTuru::suruklenme_dogrusal, ArizaTuru::motor_takilmasi, ArizaTuru::guc_cevrimi}) {
            SensorArizasi f = ariza("r", t, 0, 0);
            f.siddet.k = 2.0;
            DOGRULA(d, !k.kume.arm(f).tamam, "ret: tür " + std::to_string(static_cast<int>(t)));
        }
        SensorArizasi f = ariza("h", ArizaTuru::kayip, 0, 0);
        f.hedef = "yok";
        DOGRULA(d, !k.kume.arm(f).tamam, "bilinmeyen hedef");
        f = ariza("m", ArizaTuru::kayip, 0, 0);
        f.kanal_maskesi = 1;
        DOGRULA(d, !k.kume.arm(f).tamam, "kanal_maskesi (A5-K69)");
        DOGRULA(d, k.kume.arm(ariza("t", ArizaTuru::kayip, 0, 0)).tamam, "geçerli arm");
        DOGRULA(d, !k.kume.arm(ariza("t", ArizaTuru::kayip, 0, 0)).tamam, "yinelenen kimlik (A5-K60)");
        f = ariza("gt", ArizaTuru::gec_teslim, 0, 0);
        f.siddet.gecikme_ns = 1000;
        f.siddet.titresim_ns = 2000;
        DOGRULA(d, !k.kume.arm(f).tamam, "titreşim > gecikme");
    }
    return d.hata;
}

}  // namespace ida::sonar::test
