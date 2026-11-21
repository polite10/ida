// tests/kabul/kopru — gerçek sensör kümesiyle emniyet geri okuması (A1-K56; W4-K4, W4-K5).
//
// Zincir: Kopru (açık başlık) + ida::sensor::SensorKumesi (İDA-0 ayarı, ida0_sensor_kumesi) +
// sahte kart. Her sınırda kopru.hpp'deki sıra: sinir_isle(b) → dis_durum_ver(dis_durum()) →
// kume.adim → bosalt(t) → olcum_ekle → (kontrol sınırında) parti_kapat. Araç durağandır
// (fizik yürütülmez; DurumAnlik sabit, yalnız zaman ilerler): yalnız emniyet geri okuması
// denetlenir.
//
// Vakalar: aynı pencerede (4m+1 … 4m+4) kısıtlama (4m+1) ve gevşetme (4m+3); fiş için gevşeten
// yön fişin takılması, pim için pimin çıkarılmasıdır.
#include <set>
#include <utility>

#include "geri_okuma.hpp"
#include "ida/sensor/sensor_kumesi.hpp"
#include "ortak.hpp"

namespace kabul {
namespace {

struct GercekSensor {
    ida::sensor::SensorKumesi kume;
    ida::fizik::DurumAnlik da{};

    bool kur() {
        da.ozgul_kuvvet_b = {0.0, 0.0, -9.80665};
        da.konum_n = {0.0, 0.0, 0.5};
        da.batik_oran = 1.0;
        return kume.kur(ida::sensor::ida0_sensor_kumesi()).tamam;
    }

    void bagla(Duzenek& d) {
        d.olcum_uretici = [this, &d](std::uint64_t b, std::int64_t t) {
            kume.dis_durum_ver(d.k.dis_durum());
            da.t_ns = t;
            da.adim_no = b;
            kume.adim(da, b);
            std::vector<ida::sensor::Olcum> v;
            kume.bosalt(t, v);
            for (const auto& o : v) {
                d.k.olcum_ekle(o);
            }
        };
    }
};

std::vector<const Parti*> oturum_partileri(const Duzenek& d) {
    std::vector<const Parti*> v;
    for (const auto& p : d.kart.partiler) {
        if (p.tf.sess == d.kart.sess()) {
            v.push_back(&p);
        }
    }
    return v;
}

// kisit/gevset: sinir_sonrasi'nda çağrılır (sonraki sınırda uygulanır).
// alan: yetkili durumda alanın kısıtlı olup olmadığı.
void pencere_geri_okuma(const char* ad, const std::function<void(Duzenek&)>& kisit,
                        const std::function<void(Duzenek&)>& gevset,
                        const std::function<bool(const ida::sensor::EmniyetDurumu&)>& alan,
                        const std::function<bool(const mavlink_ida_safety_t&)>& kisitli) {
    Duzenek d;
    GercekSensor g;
    KONTROL(g.kur(), ad << ": sensör kümesi kurulur");
    g.bagla(d);
    KONTROL(hazirla(d), ad << ": kurulum");
    d.ilerle(40);
    while (d.b() % 4u != 1u) {
        d.adim();
    }
    const std::uint64_t B1 = d.b() + 4u;  // 4m+1
    const std::uint64_t B2 = B1 + 2u;     // 4m+3
    d.sinir_sonrasi = [&](std::uint64_t b) {
        if (b + 1u == B1) {
            kisit(d);
        } else if (b + 1u == B2) {
            gevset(d);
        }
    };
    const std::int64_t gecikme = anahtar_gecikmesi_ns();
    d.ilerle_kadar(static_cast<std::int64_t>(B2) * kH + gecikme + 8 * kP);
    d.sinir_sonrasi = nullptr;
    KONTROL(alan(d.emniyet_b(B1)) && alan(d.emniyet_b(B1 + 1u)) && !alan(d.emniyet_b(B2)),
            ad << ": düzenek: yetkili durumda kısıtlama B1'de, gevşetme B2'de");
    std::set<std::pair<bool, std::uint32_t>> gecmis;
    for (std::uint64_t b = 0; b < d.b(); ++b) {
        if (d.islendi(b)) {
            gecmis.emplace(alan(d.emniyet_b(b)), d.emniyet_b(b).emniyet_revizyonu);
        }
    }
    GeriOkumaBeklentisi be;
    be.ad = ad;
    be.kisitli = kisitli;
    be.t_kisit = static_cast<std::int64_t>(B1) * kH;
    be.t_gevset = static_cast<std::int64_t>(B2) * kH;
    be.gecikme = gecikme;
    be.gecmiste = [&gecmis](bool k, std::uint32_t rev) { return gecmis.count({k, rev}) != 0u; };
    geri_okuma_denetle(oturum_partileri(d), be);
}

VAKA("A1-K56.geri_okuma_fis_takilmasi") {
    pencere_geri_okuma(
        "fiş 0", [](Duzenek& d) { d.emniyet({}, false, {}, {}, {}); },
        [](Duzenek& d) { d.emniyet({}, true, {}, {}, {}); },
        [](const ida::sensor::EmniyetDurumu& e) { return !e.fis[0]; },
        [](const mavlink_ida_safety_t& s) { return (s.tube[0] & w::kTupFis) == 0u; });
}

VAKA("A1-K56.geri_okuma_pim_cikarilmasi") {
    pencere_geri_okuma(
        "pim 1", [](Duzenek& d) { d.emniyet({}, {}, {}, {}, true); },
        [](Duzenek& d) { d.emniyet({}, {}, {}, {}, false); },
        [](const ida::sensor::EmniyetDurumu& e) { return e.pim[1]; },
        [](const mavlink_ida_safety_t& s) { return (s.tube[1] & w::kTupPimCikik) == 0u; });
}

}  // namespace
}  // namespace kabul
