// A6 kayıt bağlama katmanının birim sınamaları (W5-bagla-kayit; sim.sunucu.kayit_*).
//
//   kayit_tavan   W5-K41: güvence tavanı yalnız üretilmeyen kanalda değil, `bilinmiyor` yazılmış zorunlu üst
//                 veri anahtarında da `izleme` koyar (sürüm kimliği bilinmeyen dosya `fizik` iddia edemez);
//                 neden biçimi `ust_veri_bilinmiyor:<anahtar>+<anahtar>`.
#include "kayit_baglanti.hpp"

#include <cstdio>
#include <string>
#include <vector>

namespace {

int g_hata_kb = 0;

void dogrula_kb(bool kosul, const std::string& ileti)
{
    std::printf("%s %s\n", kosul ? "GECTI " : "KALDI ", ileti.c_str());
    if (!kosul) {
        ++g_hata_kb;
    }
}

}  // namespace

int test_kayit_tavan()
{
    using ida::kayit::Guvence;
    using ida::kayit::KapanisAlanlari;
    using ida::sunucu::KayitBaglanti;
    {
        KapanisAlanlari a;
        KayitBaglanti::tavan_uygula({}, {}, a);
        dogrula_kb(!a.guvence_tavani.has_value() && a.guvence_tavan_nedeni.empty(), "eksik kanal ve bilinmeyen anahtar yok: tavan yok");
    }
    {
        KapanisAlanlari a;
        KayitBaglanti::tavan_uygula({}, {"sim_surumu", "rng_surumu"}, a);
        dogrula_kb(a.guvence_tavani == Guvence::izleme && a.guvence_tavan_nedeni == "ust_veri_bilinmiyor:sim_surumu+rng_surumu",
                   "yalnız bilinmeyen anahtar: tavan izleme, neden ust_veri_bilinmiyor:… (" + a.guvence_tavan_nedeni + ")");
    }
    {
        KapanisAlanlari a;
        KayitBaglanti::tavan_uygula({"a3/komut", "hil/bariyer"}, {}, a);
        dogrula_kb(a.guvence_tavani == Guvence::izleme && a.guvence_tavan_nedeni == "kanal_uretilmiyor:a3/komut+hil/bariyer",
                   "yalnız eksik kanal: öncekiyle aynı neden (" + a.guvence_tavan_nedeni + ")");
    }
    {
        KapanisAlanlari a;
        KayitBaglanti::tavan_uygula({"a3/komut"}, {"sim_surumu"}, a);
        dogrula_kb(a.guvence_tavani == Guvence::izleme && a.guvence_tavan_nedeni == "kanal_uretilmiyor:a3/komut,ust_veri_bilinmiyor:sim_surumu",
                   "ikisi birden: nedenler virgülle birleşir (" + a.guvence_tavan_nedeni + ")");
    }
    {
        ida::kayit::AcilisUstVerisi uv;   // boş alanlar `bilinmiyor` değildir
        dogrula_kb(KayitBaglanti::bilinmeyen_anahtarlar(uv).empty(), "boş üst veri: bilinmeyen anahtar yok");
        uv.sim_surumu = "bilinmiyor";
        uv.derleyici = "clang 17";
        uv.dalga_listesi_ozeti = "bilinmiyor";
        const std::vector<std::string> b = KayitBaglanti::bilinmeyen_anahtarlar(uv);
        dogrula_kb(b.size() == 2u && b[0] == "sim_surumu" && b[1] == "dalga_listesi_ozeti",
                   "`bilinmiyor` yazılan anahtarlar katalog sırasıyla bulunur, bilinen değer sayılmaz");
    }
    return g_hata_kb;
}
