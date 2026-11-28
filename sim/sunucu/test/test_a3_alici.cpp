// W4-K27 (2) (EMNİYET): A3 komut alıcısının iki aktarım yolu (kısıtlayan yuvalar, genel halka)
// arasındaki tutarlı kesim. G/Ç ile dünya arasındaki iş parçacığı sıralaması `cek_kancasi` ile
// belirlenimci kurulur: G/Ç istekleri `cek`in evreleri arasında verir.
//
//   a3_kesim   sol'un sırası (dünya yuvayı boş görür → G/Ç "bas"ı yuvaya, "bırak"ı halkaya koyar →
//              dünya halkayı okur) artık "bas"tan önce "bırak" teslim ettirmez; bekleyen kısıtlayan
//              sorgusu (değişmezin denetimi) yuvada kalan kısıtlayanı gösterir
//   a3_cekis_sirasi  W5-K64 (EMNİYET): çalıştırma denetimi yolları (`durdur`, `sifirla`) yuvalardan sonra
//              okunmaz: yuvalar okunurken araya giren "kısıtlayan (n), sifirla (n+1)" çiftinde sifirla
//              kısıtlayandan önce çekilmez (kısıtlayan aynı çekişte ya da daha önce görünür)
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
    // (3) Değişmezin denetimi: yuvası atlanan (çekişte alınmayan) kısıtlayan bekliyorken
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

// W4-duraklat: Tablo 4.7a ve 4.7b'nin üç komutu girişte (eşzamanlı denetim). Her durum ve iki kip
// için: duraklat yalnız kosuyor'dan, surdur ve adimla yalnız duraklatildi'dan geçerlidir; HIL'de
// adimla `kipte_gecersiz`, duraklat `gecersizligi_onayla` olmadan `onay_gerekli`; adimla'nın
// periyot_sayisi 1–10 000; bayat durum revizyonu `durum_rev_uyusmuyor` (her şeyden önce). Geçerli
// istekler çalıştırma denetimi yolundan kuyruğa girer, ötekiler girmez.
int test_a3_duraklatma_tablo()
{
    using ida::a3::CalistirmaDurumu;
    using ida::a3::KomutTuru;
    const std::string run = "20261124-000000-abcdef";
    int hata0 = g_hata_alici;
    int toplam = 0;
    int dogru = 0;
    std::size_t beklenen_kuyruk = 0;
    std::size_t kuyruk = 0;
    for (const bool hil : {false, true}) {
        for (int d = 0; d <= 8; ++d) {
            ida::sunucu::A3Alici al(hil);
            std::vector<ida::sunucu::A3Giris> kisit;
            std::vector<ida::sunucu::A3Giris> genel;
            kisit.reserve(16);
            genel.reserve(512);
            ida::sunucu::AnlikPod p;
            p.durum = static_cast<std::uint8_t>(d);
            p.durum_rev = 11;
            p.run_var = d >= 2 && d <= 7;
            for (std::size_t i = 0; i < run.size(); ++i) {
                p.run_id[i] = run[i];
            }
            al.anlik_yaz(p);
            const auto durum = static_cast<CalistirmaDurumu>(d);
            for (const KomutTuru t : {KomutTuru::calistirma_duraklat, KomutTuru::calistirma_surdur, KomutTuru::calistirma_adimla}) {
                const bool tablo = t == KomutTuru::calistirma_duraklat ? durum == CalistirmaDurumu::kosuyor
                                                                        : durum == CalistirmaDurumu::duraklatildi;
                dogrula_alici(ida::sunucu::a3b::gecis_gecerli(durum, t) == tablo,
                              "Tablo 4.7a: durum " + std::to_string(d) + ", tür " + std::to_string(static_cast<int>(t)));
                for (int varyant = 0; varyant < 4; ++varyant) {
                    // 0: geçerli biçim (onaylı, n = 1); 1: bayat revizyon; 2: onaysız; 3: n = 0
                    Komut k;
                    k.komut_no = static_cast<std::uint32_t>(++toplam);
                    k.tur = t;
                    if (p.run_var) {
                        k.beklenen_run_id = run;
                    }
                    k.beklenen_durum_rev = varyant == 1 ? 3u : 11u;
                    k.gecersizligi_onayla = varyant != 2;
                    k.periyot_sayisi = varyant == 3 ? 0u : 1u;
                    std::string beklenen;
                    if (varyant == 1) {
                        beklenen = "durum_rev_uyusmuyor";
                    } else if (!tablo) {
                        beklenen = "gecersiz_durum";
                    } else if (hil && t == KomutTuru::calistirma_adimla) {
                        beklenen = "kipte_gecersiz";
                    } else if (hil && t == KomutTuru::calistirma_duraklat && varyant == 2) {
                        beklenen = "onay_gerekli";
                    } else if (t == KomutTuru::calistirma_adimla && varyant == 3) {
                        beklenen = "gecersiz_deger";
                    }
                    const ida::a3::AlimSonucu r = al.al(k);
                    const bool tamam = beklenen.empty() ? r.tur == ida::a3::AlimSonucu::Tur::kuyrukta
                                                        : r.tur == ida::a3::AlimSonucu::Tur::ret && r.hata_kodu == beklenen;
                    dogru += tamam ? 1 : 0;
                    beklenen_kuyruk += beklenen.empty() ? 1u : 0u;
                    if (!tamam) {
                        std::printf("  hil=%d durum=%d tür=%d varyant=%d: beklenen '%s', alınan %d '%s'\n", hil ? 1 : 0, d,
                                    static_cast<int>(t), varyant, beklenen.c_str(), static_cast<int>(r.tur), r.hata_kodu.c_str());
                    }
                }
            }
            al.cek(kisit, genel);
            kuyruk += genel.size() + kisit.size();
        }
    }
    dogrula_alici(dogru == toplam && toplam == 2 * 9 * 3 * 4,
                  "girişte beklenen sonuç (" + std::to_string(dogru) + "/" + std::to_string(toplam) + ")");
    dogrula_alici(kuyruk == beklenen_kuyruk && beklenen_kuyruk > 0u,
                  "yalnız geçerli istekler kuyruğa girdi (" + std::to_string(kuyruk) + "/" + std::to_string(beklenen_kuyruk) + ")");
    return g_hata_alici - hata0;
}

// W5-K64 (EMNİYET; PR #57 dar denetimi, bulgu 1): çalıştırma denetimi yolları (`durdur_`, `denetim_`) kısıtlayan
// yuvalar okunurken değil, onlardan ÖNCE okunan uçlara kadar alınır. Yuvalar okunurken G/Ç iş parçacığı "acil
// durdurma (n), sifirla (n+1)" çiftini kabul ederse, `sifirla` bu çekişe giremez; acil durdurma ise ya aynı çekişte
// (evre 1: yuvalardan önce) ya sonraki çekişte `sifirla` ile birlikte görünür. Eski kod yolları okuyup `sifirla`yı
// çekiyor, acil durdurmayı yuvada bırakıyordu (eski dünyada hiç uygulanmıyordu).
namespace {

Komut denetim_komutu(std::uint32_t no, ida::a3::KomutTuru tur)
{
    Komut k;
    k.komut_no = no;
    k.tur = tur;
    k.beklenen_run_id = "20261124-000000-abcdef";
    k.beklenen_durum_rev = 7;
    return k;
}

// Dönüş: komutun hangi çekişte (1 ya da 2) göründüğü; 0: görünmedi.
int cekis_no(const std::vector<ida::sunucu::A3Giris>& k1, const std::vector<ida::sunucu::A3Giris>& g1,
             const std::vector<ida::sunucu::A3Giris>& k2, const std::vector<ida::sunucu::A3Giris>& g2, std::uint32_t no)
{
    if (icerir(k1, no) || icerir(g1, no)) {
        return 1;
    }
    return icerir(k2, no) || icerir(g2, no) ? 2 : 0;
}

}  // namespace

int test_a3_cekis_sirasi()
{
    const int hata0 = g_hata_alici;
    const std::string run = "20261124-000000-abcdef";
    for (const ida::a3::KomutTuru tur : {ida::a3::KomutTuru::calistirma_sifirla, ida::a3::KomutTuru::calistirma_durdur}) {
        const char* ad = tur == ida::a3::KomutTuru::calistirma_sifirla ? "sifirla" : "durdur";
        for (int evre = 1; evre <= 2; ++evre) {
            ida::sunucu::A3Alici al(false);
            ida::sunucu::AnlikPod p;
            p.durum = static_cast<std::uint8_t>(ida::a3::CalistirmaDurumu::kosuyor);
            p.run_var = true;
            for (std::size_t i = 0; i < run.size(); ++i) {
                p.run_id[i] = run[i];
            }
            p.durum_rev = 7;
            p.emniyet_rev = 5;
            al.anlik_yaz(p);
            bool verildi = false;
            ida::a3::AlimSonucu a_acil, a_den;
            al.cek_kancasi = [&](int e) {
                if (e == evre && !verildi) {
                    verildi = true;
                    a_acil = al.al(acil(10, true, 0));
                    a_den = al.al(denetim_komutu(11, tur));
                }
            };
            std::vector<ida::sunucu::A3Giris> k1, g1, k2, g2;
            k1.reserve(16);
            g1.reserve(512);
            k2.reserve(16);
            g2.reserve(512);
            al.cek(k1, g1);
            al.cek_kancasi = nullptr;
            al.cek(k2, g2);
            const std::string on = std::string("evre ") + std::to_string(evre) + ", acil durdurma (10) + " + ad + " (11): ";
            dogrula_alici(verildi && a_acil.tur == ida::a3::AlimSonucu::Tur::kuyrukta && a_den.tur == ida::a3::AlimSonucu::Tur::kuyrukta,
                          on + "iki istek de kuyruğa alındı");
            const int c_acil = cekis_no(k1, g1, k2, g2, 10);
            const int c_den = cekis_no(k1, g1, k2, g2, 11);
            dogrula_alici(c_acil != 0 && c_den != 0, on + "ikisi de iki çekişte alındı (kaybolmadı)");
            dogrula_alici(c_acil != 0 && c_den != 0 && c_acil <= c_den,
                          on + "acil durdurma " + ad + "'dan önce ya da aynı çekişte (acil: " + std::to_string(c_acil) + ". çekiş, " + ad +
                              ": " + std::to_string(c_den) + ". çekiş)");
            // Aynı çekişte görünüyorlarsa kısıtlayan `kisit`te, denetim komutu `genel`dedir (dünyanın sınır başı
            // işlemi `sifirla`yı kısıtlayana göre erteleyebilsin).
            if (c_acil == c_den) {
                const bool ayni1 = c_acil == 1;
                dogrula_alici(icerir(ayni1 ? k1 : k2, 10) && icerir(ayni1 ? g1 : g2, 11), on + "aynı çekişte: kısıtlayan `kisit`te, " + ad + " `genel`de");
            }
        }
    }
    return g_hata_alici - hata0;
}
