// W4-bagla-sonar: sonar yükleyici (çözümlenmiş senaryo → sonar kümesi ayarı ve sahne).
// Yalnız sim/sonar depoda varken derlenir.
//
//   ida_senaryo_sonar_test sonar_yukleyici <depo kökü>
#include "ida/senaryo/sonar_yukleyici.hpp"
#include "ida/sonar/sonar_kumesi.hpp"
#include "test_destek.hpp"

#include <cstring>
#include <variant>

namespace ida::senaryo::test {

namespace {

std::optional<SonarKurulumu> kur(const BellekKaynagi& kaynak, std::string* hata,
                                 const DosyaKaynagi* sonar_kaynagi = nullptr)
{
    const CozumSonucu s = senaryo_coz(kaynak, kSenaryo);
    if (!s.cozumlenmis) {
        std::printf("%s", bulgu_ozeti(s.rapor).c_str());
        return std::nullopt;
    }
    return sonar_kur(*s.cozumlenmis, sonar_kaynagi != nullptr ? *sonar_kaynagi : kaynak, hata);
}

int test_sonar_yukleyici(const TestBaglami& tb)
{
    Denetci d;
    const BellekKaynagi temel = temel_kaynak(tb);
    std::string hata;
    const auto k = kur(temel, &hata);
    DOGRULA(d, k.has_value(), "ornek-havuz sonar kurulumu: " + hata);
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
    DOGRULA(d, k->ayar.kademe == sonar::SonarKademesi::sade && k->ayar.ana_tohum == 1u, "kademe sade, tohum 1");
    DOGRULA(d, k->ayar.ortam.su.sicaklik_c == 20.0 && k->ayar.ortam.su.tuzluluk == 0.0 && !k->ayar.ortam.su.ses_hizi,
            "ortam.su (ses hızı verilmemiş: W4-K1 formülü)");
    // Sahne: taban 3 m, A7 havuz_beton malzemesi; havuz duvarı ve hedef uyarısı.
    bool taban = false;
    for (const auto& g : k->sahne->govdeler()) {
        if (g.sekil.tur == sonar::SekilTuru::duzlem && g.konum[2] == 3.0) {
            const auto& m = k->sahne->malzeme(g.malzemeler.front());
            taban = m.kimlik == "havuz_beton" && m.geri_sacilim_mu == 0.02 && m.gelis_yasasi == sonar::GelisYasasi::cos1;
        }
    }
    DOGRULA(d, taban, "taban düzlemi ortam.derinlik'te, A7 malzemesiyle");
    DOGRULA(d, k->uyarilar.size() == 1u && k->uyarilar[0].find("havuz duvarları ve hedef gövdeleri") != std::string::npos,
            "duvar ve hedef uyarısı (sessiz değil)");
    {
        sonar::SonarKumesi s;
        const auto r = s.kur(k->ayar, k->sahne);
        DOGRULA(d, r.tamam, "sonar kümesi kurulur: " + r.hata);
        DOGRULA(d, std::abs(s.ses_hizi() - 1482.3) < 0.5, "ses hızı formülden ≈ 1482 m/s: " + std::to_string(s.ses_hizi()));
    }
    // Kalibrasyon geçersiz kılması (A5-K13).
    {
        BellekKaynagi b = temel;
        b.dosyalar[kSenaryo] = *degistir(b.dosyalar[kSenaryo], "[rastgelelik]",
                                         "[kalibrasyon.malzeme.havuz_beton.akustik]\ngeri_sacilim_mu = 0.05\n"
                                         "gelis_yasasi = \"lambert_cos2\"\n\n[rastgelelik]");
        const auto x = kur(b, &hata);
        bool ok = false;
        if (x) {
            for (const auto& g : x->sahne->govdeler()) {
                const auto& m = x->sahne->malzeme(g.malzemeler.front());
                ok = ok || (m.kimlik == "havuz_beton" && m.geri_sacilim_mu == 0.05 &&
                            m.gelis_yasasi == sonar::GelisYasasi::lambert_cos2);
            }
        }
        DOGRULA(d, ok, "kalibrasyon.malzeme A7 değerini geçersiz kılar: " + hata);
    }
    // Tam kademe: bu dilimde yürütülemez.
    {
        BellekKaynagi b = temel;
        b.dosyalar[kSenaryo] = *degistir(b.dosyalar[kSenaryo], "sonar = \"sade\"", "sonar = \"tam\"\nsonar_tam = { isin_sayisi = 20000 }");
        hata.clear();
        const auto x = kur(b, &hata);
        DOGRULA(d, !x && hata.find("kalite.sonar = tam") != std::string::npos, "tam kademe reddedilir: " + hata);
    }
    // Manifest özeti uyuşmazlığı (sonarın okuduğu manifest senaryonun beyanından farklı).
    {
        BellekKaynagi farkli = temel;
        farkli.dosyalar[kManifest] += "\n";
        hata.clear();
        const auto x = kur(temel, &hata, &farkli);
        DOGRULA(d, !x && hata.find("A5-K115") != std::string::npos, "manifest özeti uyuşmazlığı reddedilir: " + hata);
    }
    // Sonarlar kapalıysa sahne kurulmaz, manifest okunmaz.
    {
        BellekKaynagi b = temel;
        b.dosyalar[kSenaryo] = *degistir(b.dosyalar[kSenaryo], "[arac.sensorler.kamera0]",
                                         "[arac.sensorler.ping1d0]\netkin = false\n\n[arac.sensorler.ping360_0]\n"
                                         "etkin = false\n\n[arac.sensorler.kamera0]");
        BellekKaynagi bos;
        hata.clear();
        const CozumSonucu s = senaryo_coz(b, kSenaryo);
        const auto x = s.cozumlenmis ? sonar_kur(*s.cozumlenmis, bos, &hata) : std::nullopt;
        DOGRULA(d, x && x->etkin_sayisi == 0u && x->sahne->govdeler().empty(), "etkin sonar yok: sahne boş " + hata);
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
    } else {
        std::printf("bilinmeyen test: %s\n", argv[1]);
        return 2;
    }
    std::printf("[%s] %s\n", argv[1], r == 0 ? "GEÇTİ" : "KALDI");
    return r == 0 ? 0 : 1;
}
