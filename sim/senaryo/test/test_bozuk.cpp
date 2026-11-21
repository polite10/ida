// Bozuk dosya takımı (A5 §7: K01–K11, K96, K97, K102 ve §5 satırları; tablo güdümlü).
//
// Her satır örnek senaryodan ya da İDA-0'dan tek bir metin değişikliğiyle bir dosya üretir
// ve beklenen bulguyu (kural, aşama, dosya, yol, satır) arar. Beklenen satır, değiştirilmiş
// dosyada `isaret`'in bulunduğu satırdır; `isaret` boşsa satır `null` beklenir.
#include "test_destek.hpp"

#include <string>
#include <vector>

namespace ida::senaryo::test {

namespace {

struct BozukSatir {
    const char* ad;
    const char* dosya;  // kSenaryo ya da kArac
    std::string eski;   // boşsa `yeni` dosyanın sonuna eklenir
    std::string yeni;
    const char* kural;
    int asama;
    const char* yol;
    std::string isaret;
    Onem onem = Onem::hata;
};

std::string cok_hedef(int n)
{
    std::string o;
    for (int i = 2; i < 2 + n; ++i) {
        o += "\n[hedefler.h" + std::to_string(i) + "]\nkimlik = " + std::to_string(i) +
             "\nvarlik = \"hedef_ornek\"\nkonum = [4.0, 0.0, 1.5]\nyonelim = [1.0, 0.0, 0.0, 0.0]\n";
    }
    return o;
}

std::string derin_anahtar()
{
    std::string o = "x_d";
    for (int i = 0; i < 60; ++i) {
        o += ".a";
    }
    return o + " = 1";
}

const std::string kTelsiz =
    "\n[baglanti]\netkin_profil = \"p\"\n[baglanti.profiller.p]\nkademe = \"L0\"\n"
    "[baglanti.profiller.p.telsiz]\nfrekans = 1.0\np_tx_dbm = 1.0\ng_tx_db = 0.0\ng_rx_db = 0.0\n"
    "s_rx_dbm = -90.0\nl_uyumsuz_db = 0.0\nm50_db = 0.0\nw_db = 1.0\nyeniden_edinim_ns = 0\n"
    "yer_istasyonu_konumu = [0.0, 0.0, 0.0]\nsu_iletkenligi = 0.0\nsu_bagil_gecirgenligi = 1.0\n";

std::vector<BozukSatir> tablo()
{
    const char* S = kSenaryo;
    const char* A = kArac;
    return {
        // ---------------------------------------------------------------- A5-K01
        {"sozdizimi", S, "ana_tohum = 1", "ana_tohum = 1\nbozuk = ", "A5-K01", 1, "", "bozuk = "},
        {"utf8", S, "ad = \"ornek-havuz\"", "ad = \"ornek\xff\"", "A5-K01", 1, "", "ad = \"ornek\xff"},
        {"utf8_araç", A, "ad = \"IDA-0\"\nvarlik", "ad = \"IDA\xc3\x28\"\nvarlik", "A5-K01", 1, "", "IDA\xc3\x28"},
        {"yinelenen_anahtar", S, "ana_tohum = 1", "ana_tohum = 1\nana_tohum = 2", "A5-K01", 1, "", "ana_tohum = 2"},
        {"yinelenen_tablo", S, "[rastgelelik]", "[kalite]\nx_a = 1\n[rastgelelik]", "A5-K01", 1, "", "[kalite]\ngorunum"},
        {"derinlik_siniri", S, "", "\n" + derin_anahtar() + "\n", "A5-K01", 1, "", "x_d.a.a"},
        // ---------------------------------------------------------------- A5-K02, K03, K04
        {"tur_cozumlenmis", S, "tur = \"senaryo\"", "tur = \"cozumlenmis_senaryo\"", "A5-K02", 1, "meta.tur", "tur = \"cozumlenmis_senaryo\""},
        {"major_2", S, "sema_surumu = \"1.0\"", "sema_surumu = \"2.0\"", "A5-K03", 1, "meta.sema_surumu", "sema_surumu"},
        {"major_2_arac", A, "sema_surumu = \"1.0\"", "sema_surumu = \"2.1\"", "A5-K03", 1, "meta.sema_surumu", "sema_surumu"},
        {"minor_buyuk", S, "sema_surumu = \"1.0\"", "sema_surumu = \"1.1\"", "A5-K04", 1, "meta.sema_surumu", "sema_surumu"},
        // ---------------------------------------------------------------- A5-K05
        {"zorunlu_eksik", S, "kip = \"sil\"\n", "", "A5-K05", 1, "calistirma.kip", ""},
        {"zorunlu_eksik_arac", A, "kutle = 13.5 ", "x_kutle = 13.5 ", "A5-K05", 1, "arac.kutle.kutle", ""},
        {"zorunlu_varliklar", S, "[varliklar]\nkume = \"ornek\"", "[x_varliklar]\nkume = \"ornek\"", "A5-K05", 1, "varliklar", ""},
        // ---------------------------------------------------------------- A5-K06
        {"bakim_girdisi", S, "acil_durdurma = false", "acil_durdurma = false\nbakim = true", "A5-K06", 1, "emniyet_girdileri.bakim", "bakim"},
        {"otopilot_nominal", S, "", "\n[otopilot]\nnominal = true\n", "A5-K06", 1, "otopilot.nominal", "nominal"},
        {"kontrol_hizi", S, "kip = \"sil\"", "kip = \"sil\"\nkontrol_hizi_hz = 400.0", "A5-K06", 1, "calistirma.kontrol_hizi_hz", "kontrol_hizi_hz"},
        {"hil_esigi", S, "kip = \"sil\"", "kip = \"sil\"\nhil_gecersizlik_esigi = 2", "A5-K06", 1, "calistirma.hil_gecersizlik_esigi", "hil_gecersizlik"},
        {"pwm_kaybi", A, "baslatma_suresi_ns = 2_000_000_000", "baslatma_suresi_ns = 2_000_000_000\npwm_kaybi = \"notr\"", "A5-K06", 1, "arac.esc.pwm_kaybi", "pwm_kaybi"},
        {"ic_basinc_sensoru", A, "[arac.sensorler.batarya0]\ntur = \"batarya_olcer\"", "[arac.sensorler.batarya0]\ntur = \"ic_basinc\"", "A5-K10", 1, "arac.sensorler.batarya0.tur", "ic_basinc"},
        {"torpido_itki", A, "ayrilma_hizi = 2.0 ", "itki = 1.0\nayrilma_hizi = 2.0 ", "A5-K06", 1, "arac.torpido.itki", "itki = 1.0"},
        {"ust_veri_girdide", S, "ad = \"ornek-havuz\"", "ad = \"ornek-havuz\"\nkalibre_edilmedi = []", "A5-K06", 1, "meta.kalibre_edilmedi", "kalibre_edilmedi"},
        // ---------------------------------------------------------------- A5-K07, K08
        {"deneysel_alan", S, "ana_tohum = 1", "ana_tohum = 1\nx_not = \"deneme\"", "A5-K07", 1, "rastgelelik.x_not", "x_not", Onem::uyari},
        {"deneysel_koleksiyon", S, "[hedefler.h1]", "[hedefler.x_not]\nbilgi = 1\n[hedefler.h1]", "A5-K07", 1, "hedefler.x_not", "[hedefler.x_not]", Onem::uyari},
        {"kullanici_adi", S, "[hedefler.h1]", "[hedefler.H1]", "A5-K08", 1, "hedefler.H1", "[hedefler.H1]"},
        // ---------------------------------------------------------------- A5-K09, K11
        {"tamsayida_kayan", S, "kimlik = 1\nsinif", "kimlik = 1.0\nsinif", "A5-K09", 1, "hedefler.h1.kimlik", "kimlik = 1.0"},
        {"birimli_deger", S, "derinlik = 3.0 ", "derinlik = \"3.0 m\" ", "A5-K09", 1, "ortam.derinlik", "derinlik = \"3.0 m\""},
        {"mantiksalda_tamsayi", S, "acil_durdurma = false", "acil_durdurma = 0", "A5-K09", 1, "emniyet_girdileri.acil_durdurma", "acil_durdurma = 0"},
        // ---------------------------------------------------------------- A5-K10
        {"aralik", S, "derinlik = 3.0 ", "derinlik = -3.0 ", "A5-K10", 1, "ortam.derinlik", "derinlik = -3.0"},
        {"deger_kumesi", S, "kip = \"sil\"", "kip = \"hil\"", "A5-K10", 1, "calistirma.kip", "kip = \"hil\""},
        {"tamsayi_53_bit", S, "ana_tohum = 1", "ana_tohum = 9007199254740992", "A5-K10", 1, "rastgelelik.ana_tohum", "ana_tohum"},
        {"desen", S, "kume = \"ornek\"", "kume = \"Ornek\"", "A5-K10", 1, "varliklar.kume", "kume = \"Ornek\""},
        {"eleman_sayisi", S, "p = [0.3, 0.3, 0.0, 0.0]", "p = [0.3, 0.3, 0.0, 0.0, 0.0]", "A5-K10", 1, "gorev.ogeler[0].p", "p = [0.3, 0.3, 0.0, 0.0, 0.0]"},
        {"dalga_257", S, "", "\n[ortam.dalga]\nbilesen_sayisi = 257\n", "A5-K10", 1, "ortam.dalga.bilesen_sayisi", "bilesen_sayisi"},
        {"hedef_62", S, "", cok_hedef(61), "A5-K10", 1, "hedefler", "[hedefler.h1]"},
        {"torpido_sabit", A, "boy = 0.20 ", "boy = 0.21 ", "A5-K29", 1, "arac.torpido.boy", "boy = 0.21"},
        // ---------------------------------------------------------------- A5-K96, K97
        {"tasma", S, "sicaklik_c = 20.0", "sicaklik_c = 1e400", "A5-K96", 1, "ortam.su.sicaklik_c", "sicaklik_c"},
        {"alt_tasma", S, "sicaklik_c = 20.0", "sicaklik_c = 1e-400", "A5-K96", 1, "ortam.su.sicaklik_c", "sicaklik_c"},
        {"nan", S, "derinlik = 3.0 ", "derinlik = nan ", "A5-K96", 1, "ortam.derinlik", "derinlik = nan"},
        {"inf_dizide", S, "konum = [4.0, 0.0, 1.5]", "konum = [4.0, -inf, 1.5]", "A5-K96", 1, "hedefler.h1.konum[1]", "-inf"},
        {"tarih_deneysel", S, "ana_tohum = 1", "ana_tohum = 1\nx_tarih = 1979-05-27", "A5-K97", 1, "rastgelelik.x_tarih", "x_tarih"},
        {"saat", S, "derinlik = 3.0 ", "derinlik = 07:32:00 ", "A5-K97", 1, "ortam.derinlik", "07:32:00"},
        {"tarih_saat_arac", A, "rol = \"yatay, on sancak\"", "rol = 1979-05-27T07:32:00Z", "A5-K97", 1, "arac.iticiler.t1.rol", "1979-05-27T07"},
        // ---------------------------------------------------------------- A5-K12
        {"ust_dizin", S, "dosya = \"ida0.toml\"", "dosya = \"../scenarios/ida0.toml\"", "A5-K12", 1, "arac.dosya", "dosya = "},
        {"mutlak_yol", S, "dosya = \"ida0.toml\"", "dosya = \"/ida0.toml\"", "A5-K12", 1, "arac.dosya", "dosya = "},
        {"arac_yok", S, "dosya = \"ida0.toml\"", "dosya = \"yok.toml\"", "A5-K12", 1, "arac.dosya", "dosya = "},
        {"arac_turu_yanlis", S, "dosya = \"ida0.toml\"", "dosya = \"ornek-havuz.toml\"", "A5-K12", 1, "arac.dosya", "dosya = "},
        {"arac_baska_dosya", A, "[arac]\n", "[arac]\ndosya = \"x.toml\"\n", "A5-K12", 1, "arac.dosya", "dosya = "},
        // ---------------------------------------------------------------- A5-K102 (aşama 2)
        {"sigorta", A, "asgari_guvenli_gerilim = 12.0", "asgari_guvenli_gerilim = 12.0\nsigorta_i2t = 100.0", "A5-K102", 2, "arac.batarya.sigorta_i2t", "sigorta_i2t"},
        {"batarya_b0", A, "kip = \"ideal\"", "kip = \"b0\"", "A5-K102", 2, "arac.batarya.kip", "kip = \"b0\""},
        {"anten", A, "[arac]\n", "[arac]\nanten_konumu = [0.0, 0.0, 0.0]\n", "A5-K102", 2, "arac.anten_konumu", "anten_konumu"},
        {"telsiz", S, "", kTelsiz, "A5-K102", 2, "baglanti.profiller.p.telsiz", "[baglanti.profiller.p.telsiz]"},
        {"ping1d_esik", S, "", "\n[arac.sensorler.ping1d0.ayar]\nalgilama_esigi = 1.0\n", "A5-K102", 2, "arac.sensorler.ping1d0.ayar.algilama_esigi", "algilama_esigi"},
        {"ping360_l_min", S, "", "\n[arac.sensorler.ping360_0.ayar]\nl_min = 1.0\n", "A5-K102", 2, "arac.sensorler.ping360_0.ayar.l_min", "l_min"},
        {"olay_turu", S, "", "\n[[olaylar]]\nkimlik = \"n\"\nsinif = \"sensor\"\nzaman_ns = 0\ntur = \"nan\"\nhedef = \"imu0\"\n", "A5-K102", 2, "olaylar[2].tur", "tur = \"nan\""},
        {"olay_turu_guc", S, "", "\n[[olaylar]]\nkimlik = \"g\"\nsinif = \"guc\"\nzaman_ns = 0\ntur = \"dusuk_batarya\"\n", "A5-K102", 2, "olaylar[2].tur", "dusuk_batarya"},
        // ---------------------------------------------------------------- §5 öteki satırlar
        {"yeni_sensor", S, "", "\n[arac.sensorler.yeni0]\netkin = false\n", "A5-K15", 2, "arac.sensorler.yeni0", "[arac.sensorler.yeni0]"},
        {"zaman_ve_kosul", S, "komut = \"ARM\"", "komut = \"ARM\"\nkosul = { buyukluk = \"derinlik\", karsilastirma = \">\", deger = 1.0 }", "A5-K61", 1, "olaylar[0]", "[[olaylar]]\nkimlik = \"arm\""},
        {"emniyette_sure", S, "", "\n[[olaylar]]\nkimlik = \"e\"\nsinif = \"emniyet_girdisi\"\nzaman_ns = 0\ngirdi = \"acil_durdurma\"\ndeger = true\nsure_ns = 5\n", "A5-K109", 1, "olaylar[2]", "[[olaylar]]\nkimlik = \"e\""},
        {"imu_hizi", A, "profil = \"icm42688p\" ", "profil = \"icm42688p\"\nhiz_hz = 200.0 ", "A5-K41", 1, "arac.sensorler.imu0", "[arac.sensorler.imu0]"},
        {"gorev_ogesi", S, "item_type = \"SURFACE\"\nframe = \"NONE\"", "item_type = \"SURFACE\"\nframe = \"LOCAL\"", "A5-K55", 2, "gorev.ogeler[1].frame", "frame = \"LOCAL\"\np = [0.25"},
        {"kamera_kademesi", S, "kamera = \"kapali\"\nsonar", "kamera = \"sade\"\nsonar", "A5-K80", 2, "kalite.kamera", "kamera = \"sade\""},
        {"otopilot_cifti", S, "", "\n[[otopilot.parametreler]]\nkimlik = 1\ntip = \"i32\"\nderinlik = 3\n", "A5-K06", 1, "otopilot.parametreler[0].derinlik", "derinlik = 3\n"},
        {"otopilot_tipi", S, "", "\n[[otopilot.parametreler]]\nkimlik = 1\ntip = \"u32\"\ndeger = -1\n", "A5-K52", 2, "otopilot.parametreler[0].deger", "deger = -1"},
        {"cakisan_emniyet", S, "", "\n[[olaylar]]\nkimlik = \"bas\"\nsinif = \"emniyet_girdisi\"\nzaman_ns = 1\ngirdi = \"acil_durdurma\"\ndeger = true\n"
                                   "\n[[olaylar]]\nkimlik = \"pim\"\nsinif = \"emniyet_girdisi\"\nzaman_ns = 1_250_000\ngirdi = \"pim\"\nhedef = \"f1\"\ndeger = false\n",
         "A5-K62", 2, "olaylar[3]", "[[olaylar]]\nkimlik = \"pim\""},
        {"varlik_yok", S, "varlik = \"hedef_ornek\"", "varlik = \"hedef_yok\"", "A5-K19", 2, "hedefler.h1.varlik", "hedef_yok"},
        {"kume_yok", S, "kume = \"ornek\"", "kume = \"yok\"", "A5-K115", 2, "varliklar.kume", "kume = \"yok\""},
        {"havuz_derinligi", S, "derinlik = 3.0 ", "derinlik = 3.002 ", "A5-K36", 2, "ortam.derinlik", "derinlik = 3.002"},
    };
}

}  // namespace

int test_bozuk_dosyalar(const TestBaglami& tb)
{
    Denetci d;
    const BellekKaynagi temel = temel_kaynak(tb);
    std::size_t n = 0;
    for (const auto& s : tablo()) {
        BellekKaynagi k = temel;
        std::string& metin = k.dosyalar[s.dosya];
        if (s.eski.empty()) {
            metin += s.yeni;
        } else {
            const auto yeni = degistir(metin, s.eski, s.yeni);
            DOGRULA(d, yeni.has_value(), std::string(s.ad) + ": değiştirilecek metin tekil değil");
            if (!yeni) {
                continue;
            }
            metin = *yeni;
        }
        ++n;
        const CozumSonucu r = senaryo_coz(k, kSenaryo);
        const std::int64_t satir = s.isaret.empty() ? 0 : satir_bul(metin, s.isaret);
        DOGRULA(d, s.isaret.empty() || satir > 0, std::string(s.ad) + ": işaret bulunamadı");
        bool bulundu = false;
        for (const auto& b : r.rapor.bulgular) {
            if (b.kural == s.kural && b.asama == s.asama && b.yol == s.yol && b.dosya == s.dosya &&
                b.onem == s.onem && (s.isaret.empty() ? !b.satir.has_value() : b.satir == satir)) {
                bulundu = true;
            }
        }
        DOGRULA(d, bulundu,
                std::string(s.ad) + ": beklenen bulgu yok: " + s.kural + " aşama " + std::to_string(s.asama) +
                    " " + s.dosya + ":" + (s.isaret.empty() ? "null" : std::to_string(satir)) + " [" + s.yol +
                    "]\n" + bulgu_ozeti(r.rapor));
        DOGRULA(d, r.rapor.gecerli == (s.onem == Onem::uyari), std::string(s.ad) + ": geçerlilik beklenenden farklı");
        DOGRULA(d, r.rapor.gecerli == r.rapor.davranis_ozeti.has_value(), std::string(s.ad) + ": özet/geçerlilik tutarsız");
    }
    // Uzantı (A5-K01): `.toml` olmayan dosya.
    {
        BellekKaynagi k = temel;
        k.dosyalar["scenarios/ornek-havuz.txt"] = k.dosyalar[kSenaryo];
        const CozumSonucu r = senaryo_coz(k, "scenarios/ornek-havuz.txt");
        const Bulgu* b = bulgu_bul(r.rapor, "A5-K01", "");
        DOGRULA(d, b != nullptr && !b->satir && b->asama == 1 && !r.rapor.gecerli, "uzantı: A5-K01 bekleniyordu");
        ++n;
    }
    // Aşama sırası (A5-K90): aşama 1'de hata varsa aşama 2 bulgusu yoktur.
    {
        BellekKaynagi k = temel;
        k.dosyalar[kSenaryo] = *degistir(*degistir(k.dosyalar[kSenaryo], "kume = \"ornek\"", "kume = \"yok\""),
                                         "derinlik = 3.0 ", "derinlik = -3.0 ");
        const CozumSonucu r = senaryo_coz(k, kSenaryo);
        bool asama2 = false;
        for (const auto& b : r.rapor.bulgular) {
            asama2 = asama2 || b.asama == 2;
        }
        DOGRULA(d, !asama2 && bulgu_bul(r.rapor, "A5-K10", "ortam.derinlik") != nullptr,
                "aşama 1 hatasında aşama 2 koşmamalı");
    }
    std::printf("  %zu bozuk dosya\n", n);
    return d.hata;
}

}  // namespace ida::senaryo::test
