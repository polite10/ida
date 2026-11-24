// W4-sonar-sahne kabul 1–3: A7 ağlarından kurulan sonar sahnesi (gerçek `havuz` kümesi ve
// scenarios/sonar-havuz.toml; sahne `senaryo::sonar_kur`'dan, sonarlar sınamanın yapılandırmasıyla).
//
//   sonar_sahne_havuz_duvari  kabul 1: duvara dik bakan Ping360 (iki duvar) ve Ping1D menzili
//                             geometrik uzaklıkta (sim.sonar.ping360_kure_duvar ve ping1d_duz_taban'ın
//                             belgelenmiş sınırları: ±2 kutu, ±3 kutu); eğik açıda modelin kuralı
//   sonar_sahne_hedef         kabul 2: bilinen pozdaki hedef (hedef_ornek) Ping360'ta beklenen
//                             kerterizde ve menzilde; sabit hızla uzaklaşan hedefin dönüşü izler
//   sonar_sahne_k27           kabul 3 (A7-U09'un sonar yarısı): akustik[] boşken sonarın kullandığı
//                             üçgen sayısı çarpışma kaynak ağlarınınkine eşit (GLB'den bağımsız sayımla);
//                             akustik[] doluyken yalnız SON_* ağları
#include "glb_yazici.hpp"
#include "ida/senaryo/sonar_yukleyici.hpp"
#include "../../sonar/test/test_destek.hpp"   // sim/sonar/test (Kosu, ping360, ...)

#include <nlohmann/json.hpp>

#include <cstring>
#include <fstream>
#include <iterator>
#include <map>

namespace ida::sonar::test {

namespace {

constexpr const char* kSenaryoYolu = "scenarios/sonar-havuz.toml";

std::string oku(const std::string& yol)
{
    std::ifstream f(yol, std::ios::binary);
    return std::string((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
}

senaryo::BellekKaynagi havuz_kaynagi(const std::string& kok)
{
    senaryo::BellekKaynagi k;
    for (const char* f : {"scenarios/sonar-havuz.toml", "scenarios/ida0.toml", "assets/havuz/manifest.json",
                          "assets/havuz/ida0.glb", "assets/havuz/torpido_ida0.glb", "assets/havuz/hedef_ornek.glb",
                          "assets/havuz/havuz_ornek.glb"}) {
        k.dosyalar[f] = oku(kok + "/" + f);
    }
    return k;
}

std::optional<senaryo::SonarKurulumu> kur(const senaryo::BellekKaynagi& k, std::string& hata)
{
    const senaryo::CozumSonucu s = senaryo::senaryo_coz(k, kSenaryoYolu);
    if (!s.cozumlenmis) {
        hata = "senaryo geçersiz:";
        for (const auto& b : s.rapor.bulgular) {
            if (b.onem == senaryo::Onem::hata) hata += " " + b.kural + " [" + b.yol + "] " + b.ileti;
        }
        return std::nullopt;
    }
    return senaryo::sonar_kur(*s.cozumlenmis, k, s.rapor, &hata);
}

// Bir Ping360 kaydında [r0, r1] aralığındaki ham enerjinin en büyük kutusu ve toplamı.
struct Tepe {
    std::size_t kutu = 0;
    double enerji = 0.0;
    double tepe = 0.0;
};
Tepe tepe(const Ping360Olcumu& p, double c, std::uint16_t sp, double r0, double r1)
{
    Tepe t;
    double emax = -1.0;
    for (std::size_t i = 0; i < p.tani.ham.size(); ++i) {
        const double r = kutu_menzili(c, sp, i);
        if (r < r0 || r > r1) continue;
        t.enerji += p.tani.ham[i];
        if (p.tani.ham[i] > emax) {
            emax = p.tani.ham[i];
            t.kutu = i;
        }
    }
    t.tepe = emax;
    return t;
}

// GLB'de COL_* (ya da SON_*) düğümlerinin ağlarındaki üçgen sayısı (okuyucudan bağımsız sayım).
std::size_t glb_ucgen(const std::string& glb, const char* onek)
{
    std::uint32_t jl = 0;
    std::memcpy(&jl, glb.data() + 12, 4);
    const nlohmann::json j = nlohmann::json::parse(glb.substr(20, jl));
    std::size_t n = 0;
    for (const auto& d : j["nodes"]) {
        if (!d.contains("mesh") || d["name"].get<std::string>().rfind(onek, 0) != 0) continue;
        for (const auto& p : j["meshes"][d["mesh"].get<std::size_t>()]["primitives"]) {
            const auto& a = p.contains("indices") ? j["accessors"][p["indices"].get<std::size_t>()]
                                                  : j["accessors"][p["attributes"]["POSITION"].get<std::size_t>()];
            n += a["count"].get<std::size_t>() / 3u;
        }
    }
    return n;
}

}  // namespace

int test_sonar_sahne_havuz_duvari(const std::string& kok)
{
    Denetci d;
    std::string hata;
    const auto k = kur(havuz_kaynagi(kok), hata);
    DOGRULA(d, k.has_value(), "sonar-havuz sahnesi kurulur: " + hata);
    if (!k) return d.hata;
    const double c = 1482.36;   // 20 °C tatlı su (W4-K1 formülü; ses_hizi testinde)
    // Araç havuzun ortasından 3,25 m doğuda, 1,5 m derinde, kuzeye bakar: doğu duvarı (y = 6,25) 3,0 m,
    // batı duvarı (y = −6,25) 9,5 m. Havuz ortası = REF (0, 0), taban 3 m.
    const Vek3 arac{0.0, 3.25, 1.5};
    struct Dik {
        std::uint16_t aci, sp;
        double beklenen;
    };
    double e_dik = 0.0;
    for (const Dik& x : {Dik{100, 311, 3.0}, Dik{300, 500, 9.5}}) {
        Kosu ks;
        if (!ks.kur(kume_ayari({ping360(p360_yap(x.sp, 1200, x.aci, x.aci, 1))}), k->sahne) || !ks.kos(durgun(arac), 0, 60)) return 1;
        const auto v = ks.sonar(7);
        DOGRULA(d, !v.empty(), "Ping360 kaydı");
        if (v.empty()) return d.hata;
        const double kutu = kutu_menzili(c, x.sp, 1) - kutu_menzili(c, x.sp, 0);
        const Tepe t = tepe(p360(v.front()), c, x.sp, 1.5, 100.0);
        const double r = kutu_menzili(c, x.sp, t.kutu);
        std::printf("  Ping360 %u grad (dik): tepe %.4f m, beklenen %.2f m, fark %.4f m (tolerans 2 kutu = %.4f m)\n", x.aci, r,
                    x.beklenen, r - x.beklenen, 2.0 * kutu);
        YAKIN(d, r, x.beklenen, 2.0 * kutu, "dik duvar menzili");
        DOGRULA(d, p360(v.front()).data[t.kutu] > p360(v.front()).data[t.kutu / 2] + 50, "duvar u8 boş sudan parlak");
        if (x.aci == 100) e_dik = t.tepe;
    }
    // Eğik geliş (modelin kuralı, 05 §2.4.2): yalnız doğrudan yol; dönüş ışının duvarı kestiği eğik
    // menzilde, r = d / cos α (yatay −3 dB yarı genişliği 1° → menzil yayılımı d tan α · 1°). Geri
    // saçılım `cos1` (havuz_beton): σ ∝ cos θ; dik gelişten zayıf. Ayna yansıması sade kademede yok.
    for (const double alfa_der : {30.0, 45.0}) {
        const auto aci = static_cast<std::uint16_t>(std::lround(100.0 - alfa_der * 400.0 / 360.0));
        const double alfa = (100.0 - aci) * 0.9 * kDerece;   // gradyana yuvarlanmış açı
        const double beklenen = 3.0 / std::cos(alfa);
        Kosu ks;
        if (!ks.kur(kume_ayari({ping360(p360_yap(311, 1200, aci, aci, 1))}), k->sahne) || !ks.kos(durgun(arac), 0, 60)) return 1;
        const auto v = ks.sonar(7);
        if (v.empty()) return 1;
        const double kutu = kutu_menzili(c, 311, 1) - kutu_menzili(c, 311, 0);
        const Tepe t = tepe(p360(v.front()), c, 311, 1.5, 100.0);
        const double r = kutu_menzili(c, 311, t.kutu);
        const double tol = 2.0 * kutu + 3.0 * std::tan(alfa) * kDerece / std::cos(alfa);
        std::printf("  Ping360 %u grad (eğik %.1f°): tepe %.4f m, beklenen d/cos α = %.4f m (tolerans %.4f m); tepe enerjisi "
                    "dik gelişin %.3f katı\n",
                    aci, alfa / kDerece, r, beklenen, tol, t.tepe / e_dik);
        YAKIN(d, r, beklenen, tol, "eğik duvar menzili d / cos α");
        DOGRULA(d, t.tepe < e_dik, "eğik gelişte tepe dik gelişten zayıf (cos1 ve yayılım)");
    }
    // Ping1D: ışın ekseni gövde +y (sensör +z → gövde +y; roll −90°), duvara dik → 3,0 m. Eğik
    // geliş α ≤ 12,5° (−3 dB yarı genişliği) iken dik doğrultu ana lobun içinde: mesafe hâlâ d;
    // α = 20°'de dik doğrultu lobun dışında: algılama (−3 dB eşiği) d ile d / cos α arasında.
    for (const double alfa_der : {0.0, 10.0, 20.0}) {
        Ping1dYapilandirmasi w;
        w.scan_length = 5.0;
        w.otomatik = false;
        auto s = ping1d(w);
        s.montaj.yonelim = euler_q(-ic::kPi / 2, 0.0, 0.0);
        Kosu ks;
        if (!ks.kur(kume_ayari({s}), k->sahne) || !ks.kos(durgun(arac, euler_q(0, 0, alfa_der * kDerece)), 0, 300)) return 1;
        const auto v = ks.sonar(6);
        DOGRULA(d, v.size() >= 3u, "Ping1D kayıtları");
        if (v.empty()) return d.hata;
        const double kutu = w.scan_length / 200.0;
        double en_az = 1e9, en_cok = -1e9;
        for (const auto& o : v) {
            const double m = static_cast<double>(p1d(o).distance);
            DOGRULA(d, (o.hdr.flags & sensor::kBayrakGecerli) != 0, "Ping1D geçerli");
            en_az = std::min(en_az, m);
            en_cok = std::max(en_cok, m);
        }
        const double ust = 3.0 / std::cos(alfa_der * kDerece);
        std::printf("  Ping1D eğim %.0f°: mesafe %.4f–%.4f m (d = 3,0; d / cos α = %.4f; 3 kutu = %.4f m)\n", alfa_der, en_az,
                    en_cok, ust, 3.0 * kutu);
        if (alfa_der <= 12.5) {
            YAKIN(d, en_az, 3.0, 3.0 * kutu, "Ping1D: dik doğrultu lobda, mesafe d");
            YAKIN(d, en_cok, 3.0, 3.0 * kutu, "Ping1D: dik doğrultu lobda, mesafe d");
        } else {
            DOGRULA(d, en_az > 3.0 + kutu && en_cok <= ust + 3.0 * kutu,
                    "Ping1D: dik doğrultu lobun dışında, d < mesafe ≤ d / cos α");
        }
    }
    return d.hata;
}

int test_sonar_sahne_hedef(const std::string& kok)
{
    Denetci d;
    // Hedef (hedef_ornek: 1,0 × 0,5 × 0,5 m kutu) araçtan 50 gradyan (45°) yönünde 5 m'de, uzun ekseni
    // bu yöne dönük: yakın yüzü 4,5 m'de. A5 kinematiği: sabit_hiz, aynı yönde 0,25 m/s.
    senaryo::BellekKaynagi kb = havuz_kaynagi(kok);
    const double s45 = std::sqrt(0.5);
    char ek[512];
    std::snprintf(ek, sizeof ek,
                  "\n[hedefler.h1]\nkimlik = 1\nvarlik = \"hedef_ornek\"\nkonum = [%.17g, %.17g, 1.5]\n"
                  "yonelim = [0.92387953251128674, 0.0, 0.0, 0.38268343236508978]\n"
                  "hareket = { tur = \"sabit_hiz\", hiz_ned = [%.17g, %.17g, 0.0] }\n",
                  5.0 * s45, 5.0 * s45, 0.25 * s45, 0.25 * s45);
    kb.dosyalar[kSenaryoYolu] += ek;
    std::string hata;
    const auto k = kur(kb, hata);
    DOGRULA(d, k.has_value() && k->hedef_govdeleri.size() == 1u && k->hedef_govdeleri[0].first == "h1",
            "hedefli sahne kurulur: " + hata);
    if (!k) return d.hata;
    const std::uint32_t g = k->hedef_govdeleri[0].second;
    DOGRULA(d, g == senaryo::hedef_govde_kimligi(1) && k->sahne->ucgen_sayisi(g) == 12u, "hedef gövdesi (12 üçgen)");
    const double c = 1482.36;
    const Vek3 arac{0.0, 0.0, 1.5};
    // Sektör taraması 30–70 gradyan, t = 0'dan: her açıda [3,5; 6] m aralığının enerjisi.
    double hedef_tepe = 0.0;
    {
        Kosu ks;
        if (!ks.kur(kume_ayari({ping360(p360_yap(311, 1200, 30, 70, 1))}), k->sahne) || !ks.kos(durgun(arac), 0, 8 * 45)) return 1;
        int en_aci = -1;
        Tepe en{};
        std::int64_t t_en = 0;
        for (const auto& o : ks.sonar(7)) {
            const Tepe t = tepe(p360(o), c, 311, 3.5, 6.0);
            if (t.enerji > en.enerji) {
                en = t;
                en_aci = p360(o).angle_grad;
                t_en = o.hdr.t_meas_ns;
            }
        }
        const double r = kutu_menzili(c, 311, en.kutu);
        const double beklenen = 4.5 + 0.25 * static_cast<double>(t_en) * 1e-9;
        std::printf("  tarama: en güçlü dönüş %d grad, %.4f m (beklenen 50 grad, %.4f m; t = %.3f s)\n", en_aci, r, beklenen,
                    static_cast<double>(t_en) * 1e-9);
        hedef_tepe = en.tepe;
        YAKIN(d, en_aci, 50, 2, "hedef kerterizi (±2 gradyan)");
        YAKIN(d, r, beklenen, 0.02, "hedef menzili (±2 kutu)");
    }
    // 49–51 gradyan sektöründe 4 s (başlangıç = bitiş tam tur demektir; sektör 49–51 ileri geri): her
    // pingde tepe menzili 4,5 + 0,25 t (hedef hareketini izler; 0,9° yan açıda yüz 4,5 / cos 0,9° ≈ 4,5006 m).
    {
        Kosu ks;
        if (!ks.kur(kume_ayari({ping360(p360_yap(311, 1200, 49, 51, 1))}), k->sahne) || !ks.kos(durgun(arac), 0, 3200)) return 1;
        std::size_t n = 0, iz = 0;
        double en_kotu = 0.0, ilk = 0.0, son = 0.0;
        for (const auto& o : ks.sonar(7)) {
            const double t = static_cast<double>(o.hdr.t_meas_ns) * 1e-9;   // Ping360: gönderim anı
            const double beklenen = 4.5 + 0.25 * t;
            const Tepe tp = tepe(p360(o), c, 311, 3.5, 6.0);
            const double r = kutu_menzili(c, 311, tp.kutu);
            ++n;
            iz += std::abs(r - beklenen) <= 0.02 ? 1u : 0u;
            en_kotu = std::max(en_kotu, std::abs(r - beklenen));
            if (n == 1u) ilk = r;
            son = r;
        }
        std::printf("  izleme: %zu ping, ilk %.4f m, son %.4f m; en büyük fark %.4f m\n", n, ilk, son, en_kotu);
        DOGRULA(d, n > 100u && iz == n, "her pingde menzil 4,5 + 0,25 t (±2 kutu)");
        DOGRULA(d, son - ilk > 0.9, "hedef uzaklaştıkça dönüş ilerler");
    }
    // Hedefsiz sahnede aynı açı ve pencerede yalnız tabanın sığ açılı zayıf dönüşü kalır (ışın konisi
    // ana lobu düşeyde ±12,5°'den geniştir): tepe hedefinkinin %1'inden küçük.
    {
        std::string h2;
        const auto k2 = kur(havuz_kaynagi(kok), h2);
        if (!k2) return 1;
        Kosu ks;
        if (!ks.kur(kume_ayari({ping360(p360_yap(311, 1200, 50, 50, 1))}), k2->sahne) || !ks.kos(durgun(arac), 0, 40)) return 1;
        const auto v = ks.sonar(7);
        const Tepe tp = v.empty() ? Tepe{} : tepe(p360(v.front()), c, 311, 3.5, 6.0);
        std::printf("  hedefsiz: 50 grad, 3,5–6 m tepe %.3g (hedefli %.3g)\n", tp.tepe, hedef_tepe);
        DOGRULA(d, !v.empty() && p360(v.front()).angle_grad == 50 && tp.tepe < 0.01 * hedef_tepe,
                "hedefsiz sahnede 3,5–6 m'de hedef dönüşü yok");
    }
    // Rastgele kinematik bu dilimde açık hata.
    for (const char* x : {"hareket = { tur = \"rastgele_yuruyus\", yuruyus_yogunlugu = 0.1, azami_hiz = 0.5 }\n",
                          "yonelim_rastgele = true\n"}) {
        senaryo::BellekKaynagi kr = havuz_kaynagi(kok);
        kr.dosyalar[kSenaryoYolu] += std::string("\n[hedefler.h1]\nkimlik = 1\nvarlik = \"hedef_ornek\"\nkonum = [4.0, 0.0, 1.5]\n"
                                                 "yonelim = [1.0, 0.0, 0.0, 0.0]\n") + x;
        std::string h3;
        const auto k3 = kur(kr, h3);
        DOGRULA(d, !k3 && h3.find("hedefler.h1") != std::string::npos && h3.find("Ek D") != std::string::npos,
                std::string("rastgele hedef kinematiği reddedilir: ") + h3);
    }
    return d.hata;
}

int test_sonar_sahne_k27(const std::string& kok)
{
    Denetci d;
    senaryo::BellekKaynagi kb = havuz_kaynagi(kok);
    kb.dosyalar[kSenaryoYolu] +=
        "\n[hedefler.h1]\nkimlik = 4\nvarlik = \"hedef_ornek\"\nkonum = [4.0, 0.0, 1.5]\nyonelim = [1.0, 0.0, 0.0, 0.0]\n";
    std::string hata;
    const auto k = kur(kb, hata);
    DOGRULA(d, k.has_value(), "sahne: " + hata);
    if (!k) return d.hata;
    const std::size_t havuz_col = glb_ucgen(kb.dosyalar["assets/havuz/havuz_ornek.glb"], "COL_");
    const std::size_t hedef_col = glb_ucgen(kb.dosyalar["assets/havuz/hedef_ornek.glb"], "COL_");
    std::printf("  havuz: sonar %zu üçgen, COL_* kaynak %zu; hedef: sonar %zu, COL_* %zu\n",
                k->sahne->ucgen_sayisi(senaryo::kStatikGovde), havuz_col, k->sahne->ucgen_sayisi(senaryo::hedef_govde_kimligi(4)),
                hedef_col);
    DOGRULA(d, havuz_col == 60u && k->sahne->ucgen_sayisi(senaryo::kStatikGovde) == havuz_col,
            "havuz: sonarın üçgen sayısı = çarpışma kaynak ağları (A7-K27)");
    DOGRULA(d, hedef_col == 12u && k->sahne->ucgen_sayisi(senaryo::hedef_govde_kimligi(4)) == hedef_col,
            "hedef: sonarın üçgen sayısı = çarpışma kaynak ağı (A7-K27)");
    DOGRULA(d, k->sahne->govde_sayisi() == 2u && k->sahne->su_yuzeyi_var(), "gövdeler: havuz, hedef (+ su yüzeyi)");
    // akustik[] dolu: bellekteki bir statik varlık (taban + kaya çarpışma ağları, yalnız SON_taban akustik).
    for (const bool son : {false, true}) {
        senaryo::test::GlbAyar ga;
        ga.dugumler = {{"taban", true, {-20.0, -20.0, 3.0}, {20.0, 20.0, 3.5}},
                       {"COL_taban", true, {-20.0, -20.0, 3.0}, {20.0, 20.0, 3.5}},
                       {"COL_kaya", true, {1.0, 1.0, 2.0}, {2.0, 2.0, 3.0}},
                       {son ? "SON_taban" : "kaya", true, {-20.0, -20.0, 3.0}, {20.0, 20.0, 3.5}}};
        const std::string glb = senaryo::test::glb_yaz(ga);
        const std::string roller =
            son ? R"("gorsel":[{"dugum":1,"ad":"taban","kosul":"her_zaman"}],)"
                  R"("carpisma":[{"dugum":2,"ad":"COL_taban","ayristirma":"yok"},{"dugum":3,"ad":"COL_kaya","ayristirma":"yok"}],)"
                  R"("akustik":[{"dugum":4,"ad":"SON_taban"}])"
                : R"("gorsel":[{"dugum":1,"ad":"taban","kosul":"her_zaman"},{"dugum":4,"ad":"kaya","kosul":"her_zaman"}],)"
                  R"("carpisma":[{"dugum":2,"ad":"COL_taban","ayristirma":"yok"},{"dugum":3,"ad":"COL_kaya","ayristirma":"yok"}])";
        const std::string girdi = R"({"kimlik":"deniz_deneme","tur":"statik","dosya":"deniz_deneme.glb","sha256":")" +
                                  senaryo::sha256_hex(glb) + R"(","bayt":)" + std::to_string(glb.size()) +
                                  R"(,"taban_derinligi_m":3.0,"referans":{"dugum":0,"ad":"REF"},)"
                                  R"("sinir_kutusu_govde_m":{"en_kucuk":[-20,-20,2],"en_buyuk":[20,20,3.5]},)"
                                  R"("malzeme_atamasi":[{"gltf_malzeme":0,"malzeme":"havuz_beton"}],)" +
                                  roller + "},\n    ";
        senaryo::BellekKaynagi kd = havuz_kaynagi(kok);
        std::string& m = kd.dosyalar["assets/havuz/manifest.json"];
        const std::size_t i = m.find("\"varliklar\": [\n    ") + std::strlen("\"varliklar\": [\n    ");
        m.insert(i, girdi);
        kd.dosyalar["assets/havuz/deniz_deneme.glb"] = glb;
        std::string& s = kd.dosyalar[kSenaryoYolu];
        s.erase(s.find("manifest_ozeti = \""), s.find('\n', s.find("manifest_ozeti = \"")) + 1u - s.find("manifest_ozeti = \""));
        const std::size_t j = s.find("sahne = \"havuz_ornek\"");
        s.replace(j, std::strlen("sahne = \"havuz_ornek\""), "sahne = \"deniz_deneme\"");
        std::string h;
        const auto x = kur(kd, h);
        const std::size_t beklenen = son ? glb_ucgen(glb, "SON_") : glb_ucgen(glb, "COL_");
        DOGRULA(d, x.has_value() && x->sahne->ucgen_sayisi(senaryo::kStatikGovde) == beklenen && beklenen == (son ? 12u : 24u),
                std::string(son ? "akustik[] dolu: yalnız SON_* (12)" : "akustik[] boş: iki COL_* (24)") + " " + h);
    }
    return d.hata;
}

}  // namespace ida::sonar::test
