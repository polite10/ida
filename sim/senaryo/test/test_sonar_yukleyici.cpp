// W4-bagla-sonar, W4-sonar-sahne: sonar yükleyici (çözümlenmiş senaryo → sonar kümesi ayarı ve sahne).
// Yalnız sim/sonar depoda varken derlenir.
//
//   ida_senaryo_sonar_test sonar_yukleyici <depo kökü>
//   ida_senaryo_sonar_test sonar_sahne_havuz_duvari | sonar_sahne_hedef | sonar_sahne_k27 <depo kökü>
//                                                      (test_sonar_sahne.cpp; W4-sonar-sahne kabul 1–3)
//
// Senaryo: scenarios/sonar-havuz.toml ve A7 kümesi `havuz` (assets/havuz; gerçek dosyalar bellekte).
// Uygunluk örneği `ornek`i gösteren ornek-havuz.toml sonar açıkken A7 hatasıyla reddedilir.
#include "glb_yazici.hpp"
#include "ida/senaryo/sonar_yukleyici.hpp"
#include "ida/sonar/sonar_kumesi.hpp"
#include "test_destek.hpp"

#include <cstring>
#include <variant>

namespace ida::sonar::test {
int test_sonar_sahne_havuz_duvari(const std::string& kok);
int test_sonar_sahne_hedef(const std::string& kok);
int test_sonar_sahne_k27(const std::string& kok);
}  // namespace ida::sonar::test

namespace ida::senaryo::test {

namespace {

constexpr const char* kSonarSenaryo = "scenarios/sonar-havuz.toml";
constexpr const char* kHavuzManifest = "assets/havuz/manifest.json";

// `sonar_kaynagi`: sonarın manifesti ve varlıkları yeniden okuduğu kaynak (verilmezse çözümleyicininki;
// farklıysa iki okuma arasında değişen kaynağın benzetimi).
std::optional<SonarKurulumu> kur(const BellekKaynagi& kaynak, const char* yol, std::string* hata,
                                 const DosyaKaynagi* sonar_kaynagi = nullptr)
{
    const CozumSonucu s = senaryo_coz(kaynak, yol);
    if (!s.cozumlenmis) {
        std::printf("%s", bulgu_ozeti(s.rapor).c_str());
        if (hata != nullptr) *hata = "senaryo geçersiz";
        return std::nullopt;
    }
    return sonar_kur(*s.cozumlenmis, sonar_kaynagi != nullptr ? *sonar_kaynagi : kaynak, s.rapor, hata);
}

std::string degistir1(Denetci& d, const std::string& metin, const std::string& eski, const std::string& yeni)
{
    const std::optional<std::string> o = degistir(metin, eski, yeni);
    DOGRULA(d, o.has_value(), "sınama metninde tam bir kez: " + eski);
    return o.value_or(metin);
}

BellekKaynagi havuz_kaynagi(const TestBaglami& b)
{
    BellekKaynagi k;
    for (const char* y : {kSonarSenaryo, kArac, kHavuzManifest, "assets/havuz/ida0.glb", "assets/havuz/torpido_ida0.glb",
                          "assets/havuz/hedef_ornek.glb", "assets/havuz/havuz_ornek.glb"}) {
        k.dosyalar[y] = dosya_oku(b.kok + "/" + y);
    }
    return k;
}

std::string ozetsiz(Denetci& d, const std::string& m)
{
    const std::size_t i = m.find("manifest_ozeti = \"");
    DOGRULA(d, i != std::string::npos, "senaryoda manifest_ozeti");
    return i == std::string::npos ? m : m.substr(0, i) + m.substr(m.find('\n', i) + 1u);
}

const sonar::AkustikMalzeme* malzeme_bul(const sonar::AgSahnesi& s, const std::string& kimlik)
{
    for (std::size_t i = 0; i < s.malzeme_sayisi(); ++i) {
        if (s.malzeme(static_cast<std::uint16_t>(i)).kimlik == kimlik) return &s.malzeme(static_cast<std::uint16_t>(i));
    }
    return nullptr;
}

int test_sonar_yukleyici(const TestBaglami& tb)
{
    Denetci d;
    std::string hata;
    // Uygunluk örneği (ikili dosyası yok): A7'nin kendi nedeniyle açık hata (A7-K06/K51).
    {
        const auto x = kur(temel_kaynak(tb), kSenaryo, &hata);
        DOGRULA(d, !x && hata.find("EKSIK_VARLIK") != std::string::npos && hata.find("havuz_ornek") != std::string::npos,
                "ornek-havuz (yer tutucu küme) reddedilir: " + hata);
    }
    const BellekKaynagi temel = havuz_kaynagi(tb);
    hata.clear();
    const auto k = kur(temel, kSonarSenaryo, &hata);
    DOGRULA(d, k.has_value(), "sonar-havuz (havuz kümesi) sonar kurulumu: " + hata);
    if (!k) {
        return d.hata;
    }
    // ida0.toml'daki iki sonar; kod varsayılanı (sonar::ida0_sonar_kumesi) ile alan alan.
    const sonar::SonarKumesiAyari kod = sonar::ida0_sonar_kumesi();
    DOGRULA(d, k->ayar.sonarlar.size() == 2u && k->etkin_sayisi == 2u, "iki etkin sonar");
    for (const sonar::SonarAyari& b : kod.sonarlar) {
        const sonar::SonarAyari* a = nullptr;
        for (const auto& x : k->ayar.sonarlar) {
            a = x.ad == b.ad ? &x : a;
        }
        DOGRULA(d, a != nullptr, "sonar " + b.ad);
        if (a == nullptr) {
            continue;
        }
        DOGRULA(d, a->tur == b.tur && a->kimlik == b.kimlik && a->inst == b.inst && a->profil == b.profil && a->etkin,
                b.ad + ": tür, kimlik, inst, profil");
        DOGRULA(d, a->montaj.konum == b.montaj.konum && a->montaj.yonelim == b.montaj.yonelim, b.ad + ": montaj");
        if (const auto* p = std::get_if<sonar::Ping1dAyar>(&a->ayar)) {
            const auto& q = std::get<sonar::Ping1dAyar>(b.ayar);
            DOGRULA(d, p->kutu_sayisi == q.kutu_sayisi && p->ping_araligi_ns == q.ping_araligi_ns &&
                           p->oto_menzil_carpani == q.oto_menzil_carpani && p->islem_suresi_ns == q.islem_suresi_ns &&
                           !p->sogurma,
                    "ping1d profili A5 Ek E ile");
        } else if (const auto* r = std::get_if<sonar::Ping360Ayar>(&a->ayar)) {
            const auto& q = std::get<sonar::Ping360Ayar>(b.ayar);
            DOGRULA(d, r->baglanti == q.baglanti && !r->baud && r->t_m0_ns == q.t_m0_ns && r->t_grad_ns == q.t_grad_ns &&
                           r->gurultu_tabani == q.gurultu_tabani && r->tvg_a == q.tvg_a && !r->sogurma,
                    "ping360 profili A5 Ek E ile");
        }
    }
    DOGRULA(d, k->ayar.kademe == sonar::SonarKademesi::sade && k->ayar.ana_tohum == 7u, "kademe sade, tohum 7");
    DOGRULA(d, k->ayar.ortam.su.sicaklik_c == 20.0 && k->ayar.ortam.su.tuzluluk == 0.0 && !k->ayar.ortam.su.ses_hizi,
            "ortam.su (ses hızı verilmemiş: W4-K1 formülü)");
    // Sahne: su yüzeyi + havuz_ornek ağları (taban ve dört duvar, 60 üçgen), A7 havuz_beton malzemesi; hedef yok.
    {
        const sonar::AkustikMalzeme* m = malzeme_bul(*k->sahne, "havuz_beton");
        DOGRULA(d, k->sahne->su_yuzeyi_var() && k->sahne->govde_sayisi() == 1u &&
                       k->sahne->ucgen_sayisi(kStatikGovde) == 60u && k->hedef_govdeleri.empty(),
                "sahne: su yüzeyi + havuz (60 üçgen)");
        DOGRULA(d, m != nullptr && m->geri_sacilim_mu == 0.02 && m->gelis_yasasi == sonar::GelisYasasi::cos1,
                "havuz_beton A7 değerleriyle");
        // Taban 3 m'de (geometriden; A7-K56): aşağı bakan ışın.
        sonar::Vurus v;
        DOGRULA(d, k->sahne->kesis({0.0, 0.0, 1.0}, {0.0, 0.0, 1.0}, 1e-9, 10.0, 0, 0.01, v) && std::abs(v.t - 2.0) < 1e-6 &&
                       v.govde == kStatikGovde,
                "taban z = 3 m (ağdan)");
        DOGRULA(d, k->sahne->kesis({0.0, 0.0, 1.0}, {0.0, 1.0, 0.0}, 1e-9, 10.0, 0, 0.01, v) && std::abs(v.t - 6.25) < 1e-6,
                "doğu duvarı y = 6,25 m (ağdan)");
        DOGRULA(d, k->uyarilar.empty(), "uyarı yok");
    }
    {
        sonar::SonarKumesi s;
        const auto r = s.kur(k->ayar, k->sahne);
        DOGRULA(d, r.tamam, "sonar kümesi kurulur: " + r.hata);
        DOGRULA(d, std::abs(s.ses_hizi() - 1482.3) < 0.5, "ses hızı formülden ≈ 1482 m/s: " + std::to_string(s.ses_hizi()));
    }
    // Kalibrasyon geçersiz kılması (A5-K13).
    {
        BellekKaynagi b = temel;
        b.dosyalar[kSonarSenaryo] = degistir1(d, b.dosyalar[kSonarSenaryo], "[rastgelelik]",
                                              "[kalibrasyon.malzeme.havuz_beton.akustik]\ngeri_sacilim_mu = 0.05\n"
                                              "gelis_yasasi = \"lambert_cos2\"\n\n[rastgelelik]");
        hata.clear();
        const auto x = kur(b, kSonarSenaryo, &hata);
        const sonar::AkustikMalzeme* m = x ? malzeme_bul(*x->sahne, "havuz_beton") : nullptr;
        DOGRULA(d, m != nullptr && m->geri_sacilim_mu == 0.05 && m->gelis_yasasi == sonar::GelisYasasi::lambert_cos2,
                "kalibrasyon.malzeme A7 değerini geçersiz kılar: " + hata);
    }
    // Tam kademe: bu dilimde yürütülemez.
    {
        BellekKaynagi b = temel;
        b.dosyalar[kSonarSenaryo] = degistir1(d, b.dosyalar[kSonarSenaryo], "sonar = \"sade\"",
                                              "sonar = \"tam\"\nsonar_tam = { isin_sayisi = 20000 }");
        hata.clear();
        const auto x = kur(b, kSonarSenaryo, &hata);
        DOGRULA(d, !x && hata.find("kalite.sonar = tam") != std::string::npos, "tam kademe reddedilir: " + hata);
    }
    // Manifest özeti uyuşmazlığı (sonarın okuduğu manifest senaryonun beyanından farklı).
    {
        BellekKaynagi farkli = temel;
        farkli.dosyalar[kHavuzManifest] += "\n";
        hata.clear();
        const auto x = kur(temel, kSonarSenaryo, &hata, &farkli);
        DOGRULA(d, !x && hata.find("A5-K115") != std::string::npos, "manifest özeti uyuşmazlığı reddedilir: " + hata);
    }
    // Sol #3: senaryo manifest_ozeti beyan etmese de iki okuma arasında değişen manifest (kume_surumu
    // 1 → 2) doğrulama raporundaki kaynak özetiyle karşılaştırılıp reddedilir.
    {
        BellekKaynagi b = temel;
        b.dosyalar[kSonarSenaryo] = ozetsiz(d, b.dosyalar[kSonarSenaryo]);
        BellekKaynagi farkli = b;
        farkli.dosyalar[kHavuzManifest] =
            degistir1(d, farkli.dosyalar[kHavuzManifest], "\"kume_surumu\": 1,", "\"kume_surumu\": 2,");
        hata.clear();
        const auto x = kur(b, kSonarSenaryo, &hata, &farkli);
        DOGRULA(d, !x && hata.find("doğrulamadan sonra değişti") != std::string::npos && hata.find("A5-K115") != std::string::npos,
                "iki okuma arasında değişen manifest reddedilir (özet beyansız): " + hata);
    }
    // A7-K06: varlık dosyasının tek baytı değişmiş (manifest aynı) → OZET_UYUSMAZ; dosya yok → EKSIK_VARLIK.
    {
        BellekKaynagi b = temel;
        std::string& g = b.dosyalar["assets/havuz/havuz_ornek.glb"];
        g[g.size() / 2] = static_cast<char>(g[g.size() / 2] ^ 0x01);
        hata.clear();
        const auto x = kur(b, kSonarSenaryo, &hata);
        DOGRULA(d, !x && hata.find("OZET_UYUSMAZ") != std::string::npos && hata.find("havuz_ornek") != std::string::npos,
                "değişmiş havuz_ornek.glb reddedilir: " + hata);
        b.dosyalar.erase("assets/havuz/havuz_ornek.glb");
        hata.clear();
        const auto y = kur(b, kSonarSenaryo, &hata);
        DOGRULA(d, !y && hata.find("EKSIK_VARLIK") != std::string::npos, "eksik havuz_ornek.glb reddedilir: " + hata);
    }
    // Sonarlar kapalıysa sahne kurulmaz, manifest ve varlıklar okunmaz.
    {
        BellekKaynagi b = temel;
        b.dosyalar[kSonarSenaryo] = degistir1(d, b.dosyalar[kSonarSenaryo], "[arac.sensorler.kamera0]",
                                              "[arac.sensorler.ping1d0]\netkin = false\n\n[arac.sensorler.ping360_0]\n"
                                              "etkin = false\n\n[arac.sensorler.kamera0]");
        b.dosyalar[kSonarSenaryo] = ozetsiz(d, b.dosyalar[kSonarSenaryo]);
        BellekKaynagi bos;
        hata.clear();
        const CozumSonucu s = senaryo_coz(b, kSonarSenaryo);
        const auto x = s.cozumlenmis ? sonar_kur(*s.cozumlenmis, bos, s.rapor, &hata) : std::nullopt;
        DOGRULA(d, x && x->etkin_sayisi == 0u && x->sahne->govde_sayisi() == 0u && !x->sahne->su_yuzeyi_var(),
                "etkin sonar yok: sahne boş " + hata);
    }
    // Açık su: bellekte kutu tabanlı bir statik varlık (gerçek GLB baytları, özetli; taban 3 m).
    {
        GlbAyar ga;
        ga.dugumler = {{"taban", true, {-50.0, -50.0, 3.0}, {50.0, 50.0, 3.5}},
                       {"COL_taban", true, {-50.0, -50.0, 3.0}, {50.0, 50.0, 3.5}}};
        const std::string glb = glb_yaz(ga);
        BellekKaynagi b = temel;
        b.dosyalar["assets/havuz/deniz_ornek.glb"] = glb;
        b.dosyalar[kHavuzManifest] = degistir1(
            d, b.dosyalar[kHavuzManifest], "\"varliklar\": [\n    ",
            std::string("\"varliklar\": [\n    ") + R"({"kimlik":"deniz_ornek","tur":"statik","dosya":"deniz_ornek.glb",)" +
                R"("sha256":")" + sha256_hex(glb) + R"(","bayt":)" + std::to_string(glb.size()) +
                R"(,"taban_derinligi_m":3.0,"referans":{"dugum":0,"ad":"REF"},)"
                R"("sinir_kutusu_govde_m":{"en_kucuk":[-50,-50,3],"en_buyuk":[50,50,3.5]},)"
                R"("malzeme_atamasi":[{"gltf_malzeme":0,"malzeme":"havuz_beton"}],)"
                R"("gorsel":[{"dugum":1,"ad":"taban","kosul":"her_zaman"}],)"
                R"("carpisma":[{"dugum":2,"ad":"COL_taban","ayristirma":"yok"}]},)"
                "\n    ");
        std::string m = ozetsiz(d, b.dosyalar[kSonarSenaryo]);
        m = degistir1(d, m, "tur = \"havuz\"\nsahne = \"havuz_ornek\"", "tur = \"acik_su\"\nsahne = \"deniz_ornek\"");
        m = degistir1(d, m, "[ortam.su]\n", "[ortam.su]\nyogunluk = 1000.0\ntuzluluk = 0.0\n");
        b.dosyalar[kSonarSenaryo] = m;
        hata.clear();
        const auto x = kur(b, kSonarSenaryo, &hata);
        sonar::Vurus v;
        DOGRULA(d, x && x->sahne->ucgen_sayisi(kStatikGovde) == 12u &&
                       x->sahne->kesis({0.0, 0.0, 1.0}, {0.0, 0.0, 1.0}, 1e-9, 10.0, 0, 0.01, v) && std::abs(v.t - 2.0) < 1e-6,
                "açık su (deniz_ornek, bellekte) kurulur, taban 3 m: " + hata);
    }
    return d.hata;
}

}  // namespace

}  // namespace ida::senaryo::test

int main(int argc, char** argv)
{
    using namespace ida::senaryo::test;
    if (argc < 3) {
        std::printf("kullanım: %s <test> <depo kökü>\n", argv[0]);
        return 2;
    }
    TestBaglami b;
    b.kok = argv[2];
    int r = 2;
    if (std::strcmp(argv[1], "sonar_yukleyici") == 0) {
        r = test_sonar_yukleyici(b);
    } else if (std::strcmp(argv[1], "sonar_sahne_havuz_duvari") == 0) {
        r = ida::sonar::test::test_sonar_sahne_havuz_duvari(b.kok);
    } else if (std::strcmp(argv[1], "sonar_sahne_hedef") == 0) {
        r = ida::sonar::test::test_sonar_sahne_hedef(b.kok);
    } else if (std::strcmp(argv[1], "sonar_sahne_k27") == 0) {
        r = ida::sonar::test::test_sonar_sahne_k27(b.kok);
    } else {
        std::printf("bilinmeyen test: %s\n", argv[1]);
        return 2;
    }
    std::printf("[%s] %s\n", argv[1], r == 0 ? "GEÇTİ" : "KALDI");
    return r == 0 ? 0 : 1;
}
