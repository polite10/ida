// W4-K27 (2) (EMNİYET): A3 komut alıcısının iki aktarım yolu (kısıtlayan yuvalar, genel halka)
// arasındaki tutarlı kesim. G/Ç ile dünya arasındaki iş parçacığı sıralaması `cek_kancasi` ile
// belirlenimci kurulur: G/Ç istekleri `cek`in evreleri arasında verir.
//
//   a3_kesim   sol'un sırası (dünya yuvayı boş görür → G/Ç "bas"ı yuvaya, "bırak"ı halkaya koyar →
//              dünya halkayı okur) artık "bas"tan önce "bırak" teslim ettirmez; bekleyen kısıtlayan
//              sorgusu (değişmezin denetimi) yuvada kalan kısıtlayanı gösterir
#include "a3_baglanti.hpp"

#include <cstdio>
#include <string>
#include <vector>

namespace {

int g_hata_alici = 0;

void dogrula_alici(bool kosul, const std::string& ileti)
{
    std::printf("%s %s\n", kosul ? "GECTI " : "KALDI ", ileti.c_str());
    if (!kosul) {
        ++g_hata_alici;
    }
}

using ida::a3::Komut;

Komut acil(std::uint32_t no, bool bas, std::uint32_t rev)
{
    Komut k;
    k.komut_no = no;
    k.tur = ida::a3::KomutTuru::emniyet_degistir;
    k.beklenen_run_id = "20261124-000000-abcdef";
    k.girdi = ida::a3::EmniyetGirdisi::acil_durdurma;
    k.hedef_durum = bas ? ida::a3::GirdiDurumu::basili : ida::a3::GirdiDurumu::serbest;
    k.kisitlayan = bas;
    if (!bas) {
        k.beklenen_emniyet_rev = rev;
    }
    return k;
}

Komut disarm(std::uint32_t no)
{
    Komut k;
    k.komut_no = no;
    k.tur = ida::a3::KomutTuru::yer_istasyonu_komut;
    k.beklenen_run_id = "20261124-000000-abcdef";
    k.yi_komut = "disarm";
    k.yi_sinif = ida::a3::KomutSinifi::daraltici;
    k.yi_parametreler_json = "{}";
    k.kisitlayan = true;
    return k;
}

bool icerir(const std::vector<ida::sunucu::A3Giris>& v, std::uint32_t no)
{
    for (const auto& g : v) {
        if (g.k.komut_no == no) {
            return true;
        }
    }
    return false;
}

}  // namespace

int test_a3_kesim()
{
    ida::sunucu::A3Alici al(false);
    ida::sunucu::AnlikPod p;
    p.durum = static_cast<std::uint8_t>(ida::a3::CalistirmaDurumu::kosuyor);
    p.run_var = true;
    const std::string run = "20261124-000000-abcdef";
    for (std::size_t i = 0; i < run.size(); ++i) {
        p.run_id[i] = run[i];
    }
    p.emniyet_rev = 5;
    p.acil = 1;
    al.anlik_yaz(p);
    std::vector<ida::sunucu::A3Giris> kisit;
    std::vector<ida::sunucu::A3Giris> genel;
    kisit.reserve(16);
    genel.reserve(512);
    // (1) Evre 1 (kesimden sonra, yuvalardan önce): G/Ç "bas" ve "bırak" verir.
    al.cek_kancasi = [&al](int evre) {
        if (evre == 1) {
            (void)al.al(acil(1, true, 0));
            (void)al.al(acil(2, false, 5));
        }
    };
    al.cek(kisit, genel);
    dogrula_alici(icerir(kisit, 1) && !icerir(genel, 2), "evre 1: bas alındı, bırak kesimin dışında");
    al.cek_kancasi = nullptr;
    kisit.clear();
    genel.clear();
    al.cek(kisit, genel);
    dogrula_alici(kisit.empty() && icerir(genel, 2), "sonraki çekişte bırak (bas zaten işlenmiş)");
    // (2) Sol'un sırası: dünya yuvaları boş görür (evre 2), G/Ç bas'ı yuvaya, bırak'ı halkaya koyar,
    // sonra dünya halkayı okur. Kesim önceden okunduğu için bırak bu çekişe girmez.
    kisit.clear();
    genel.clear();
    al.cek_kancasi = [&al](int evre) {
        if (evre == 2) {
            (void)al.al(acil(3, true, 0));
            (void)al.al(acil(4, false, 5));
        }
    };
    al.cek(kisit, genel);
    dogrula_alici(!icerir(genel, 4) && !icerir(kisit, 3), "evre 2: ne bas ne bırak bu çekişte (bırak bas'ı geçemez)");
    al.cek_kancasi = nullptr;
    kisit.clear();
    genel.clear();
    al.cek(kisit, genel);
    dogrula_alici(icerir(kisit, 3) && icerir(genel, 4) && kisit.front().sira < genel.front().sira,
                  "sonraki çekişte ikisi birlikte; kısıtlayanın sıra numarası küçük (önce işlenir)");
    // (3) Değişmezin denetimi: yuvası atlanan (duraklatmada yer istasyonu) kısıtlayan bekliyorken
    // ondan sonra teslim edilmiş gevşetme: bekleyen kısıtlayanın sırası gevşetmeninkinden küçük.
    kisit.clear();
    genel.clear();
    (void)al.al(disarm(5));
    (void)al.al(acil(6, false, 5));
    al.cek(kisit, genel, ida::sunucu::A3Alici::kYiYuvalari);
    const std::uint64_t bekleyen = al.bekleyen_kisit_en_kucuk(ida::sunucu::A3Alici::kButunYuvalar);
    dogrula_alici(kisit.empty() && icerir(genel, 6) && bekleyen != 0u && bekleyen < genel.front().sira,
                  "atlanan yuvadaki kısıtlayan görünür ve gevşetmeden önce teslim edilmiş");
    kisit.clear();
    genel.clear();
    al.cek(kisit, genel);
    dogrula_alici(icerir(kisit, 5) && al.bekleyen_kisit_en_kucuk(ida::sunucu::A3Alici::kButunYuvalar) == 0u,
                  "yuva boşalınca bekleyen yok");
    return g_hata_alici;
}
