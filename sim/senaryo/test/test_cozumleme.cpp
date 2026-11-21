// Anlamsal kurallar (§4.3–§4.13), koşullu varsayılanlar (A5-K98) ve A7 atıfları
// (A5-K18, K19, K36, K115).
#include "test_destek.hpp"

#include <initializer_list>

namespace ida::senaryo::test {

namespace {

struct Degisiklik {
    const char* dosya;
    std::string eski;  // boşsa sona ekle
    std::string yeni;
};

struct Kosu {
    CozumSonucu s;
    bool uygulandi = true;
};

Kosu kos(const TestBaglami& tb, std::initializer_list<Degisiklik> ds)
{
    BellekKaynagi k = temel_kaynak(tb);
    Kosu o;
    for (const auto& d : ds) {
        std::string& m = k.dosyalar[d.dosya];
        if (d.eski.empty()) {
            m += d.yeni;
        } else if (const auto y = degistir(m, d.eski, d.yeni)) {
            m = *y;
        } else {
            std::printf("  değiştirilecek metin tekil değil: %s\n", d.eski.c_str());
            o.uygulandi = false;
        }
    }
    o.s = senaryo_coz(k, kSenaryo);
    return o;
}

bool var(const Kosu& k, const std::string& kural, const std::string& yol, Onem onem = Onem::hata)
{
    for (const auto& b : k.s.rapor.bulgular) {
        if (b.kural == kural && b.yol == yol && b.onem == onem) {
            return true;
        }
    }
    return false;
}

const Deger* cz(const Kosu& k, std::string_view yol)
{
    return k.s.cozumlenmis ? k.s.cozumlenmis->agac().yol_bul(yol) : nullptr;
}

double sayi(const Kosu& k, std::string_view yol)
{
    const Deger* d = cz(k, yol);
    return d != nullptr && d->sayisal_mi() ? d->cift() : -12345.0;
}

#define HATA_BEKLE(d, k, kural, yol, ad)                                                            \
    DOGRULA(d, (k).uygulandi && !(k).s.rapor.gecerli && var((k), (kural), (yol)),                  \
            std::string(ad) + ": " + (kural) + " [" + (yol) + "] bekleniyordu\n" + bulgu_ozeti((k).s.rapor))
#define GECERLI_BEKLE(d, k, ad) \
    DOGRULA(d, (k).uygulandi && (k).s.rapor.gecerli, std::string(ad) + ": geçerli bekleniyordu\n" + bulgu_ozeti((k).s.rapor))

const char* S = kSenaryo;
const char* A = kArac;

std::string olay(const std::string& govde)
{
    return "\n[[olaylar]]\n" + govde + "\n";
}

}  // namespace

int test_anlamsal_kurallar(const TestBaglami& tb)
{
    Denetci d;
    // ---------------------------------------------------------------- §4.3 araç
    HATA_BEKLE(d, kos(tb, {{A, "[0.0, 0.23, 0.0]", "[0.001, 0.23, 0.0]"}}), "A5-K21", "arac.kutle.atalet", "asimetrik atalet");
    HATA_BEKLE(d, kos(tb, {{A, "[0.26, 0.0, 0.0]", "[-0.26, 0.0, 0.0]"}}), "A5-K21", "arac.kutle.atalet", "pozitif tanımlı değil");
    HATA_BEKLE(d, kos(tb, {{A, "[0.0, 0.0, 0.37]", "[0.0, 0.0, 0.60]"}}), "A5-K21", "arac.kutle.atalet", "üçgen eşitsizliği");
    HATA_BEKLE(d, kos(tb, {{A, "trim_kutlesi = 0.0 ", "trim_kutlesi = 0.5 "}}), "A5-K24", "arac.kutle.trim_konumu", "trim konumu");
    GECERLI_BEKLE(d, kos(tb, {{A, "trim_kutlesi = 0.0 ", "trim_kutlesi = 0.5\ntrim_konumu = [0.1, 0.0, 0.05] "}}), "trimli");
    HATA_BEKLE(d, kos(tb, {{A, "kanal = 7", "kanal = 6"}}), "A5-K25", "arac.iticiler.t8.kanal", "yinelenen kanal");
    HATA_BEKLE(d, kos(tb, {{A, "yon = [0.7071067811865475, -0.7071067811865475, 0.0]\nrol = \"yatay, on sancak\"", "yon = [0.0, 0.0, 1.0]\nrol = \"yatay, on sancak\""},
                           {A, "yon = [0.7071067811865475, 0.7071067811865475, 0.0]\nrol = \"yatay, on iskele\"", "yon = [0.0, 0.0, 1.0]\nrol = \"yatay, on iskele\""},
                           {A, "yon = [0.7071067811865475, 0.7071067811865475, 0.0]\nrol = \"yatay, arka sancak\"", "yon = [0.0, 0.0, 1.0]\nrol = \"yatay, arka sancak\""},
                           {A, "yon = [0.7071067811865475, -0.7071067811865475, 0.0]\nrol = \"yatay, arka iskele\"", "yon = [0.0, 0.0, 1.0]\nrol = \"yatay, arka iskele\""}}),
               "A5-K25", "arac.iticiler", "B rankı");
    HATA_BEKLE(d, kos(tb, {{S, "", "\n[arac.hidrodinamik.sapma]\neklenmis_kutle = [1.31, 1.0, 1.0, 1.0, 1.0, 1.0]\n"}}), "A5-K26", "arac.hidrodinamik.sapma.eklenmis_kutle[0]", "sapma");
    GECERLI_BEKLE(d, kos(tb, {{S, "", "\n[arac.hidrodinamik.sapma]\neklenmis_kutle = [1.3, 1.0, 1.0, 2.0, 0.0, 1.0]\n"}}), "sapma sınırda");
    {
        const Kosu k = kos(tb, {{A, "v_sabit = 15.6 ", "v_sabit = 21.0 "}});
        DOGRULA(d, k.s.rapor.gecerli && var(k, "A5-K28", "arac.batarya.v_sabit", Onem::uyari), "v_sabit dışdeğerleme uyarısı");
    }
    HATA_BEKLE(d, kos(tb, {{A, "kip = \"ideal\"", "kip = \"ideal\"\nx_v = 1"}, {A, "v_sabit = 15.6 ", "x_v_sabit = 15.6 "}}), "A5-K28", "arac.batarya.v_sabit", "v_sabit yok");
    HATA_BEKLE(d, kos(tb, {{A, "tam_gaz_akimi = [17.0, 24.0, 32.0]", "tam_gaz_akimi = [17.0, 24.0]"}}), "A5-K30", "arac.itici_modeli.tam_gaz_akimi", "uzunluk");
    HATA_BEKLE(d, kos(tb, {{A, "gerilim_noktalari = [12.0, 16.0, 20.0]", "gerilim_noktalari = [12.0, 16.0, 16.0]"}}), "A5-K30", "arac.itici_modeli.gerilim_noktalari", "kesin artan");
    HATA_BEKLE(d, kos(tb, {{A, "sekil_polinomu = [8.9, 176.0, -404.1, 389.9, -140.3]", "sekil_polinomu = [0.0, 0.0, 0.0, 0.0, 0.0]"}}), "A5-K104", "arac.itici_modeli.sekil_polinomu", "sıfır polinom");
    HATA_BEKLE(d, kos(tb, {{A, "transfer_payda = [1.0, 89.0, 9258.0, 108700.0]", "transfer_payda = [0.0, 89.0, 9258.0, 108700.0]"}}), "A5-K105", "arac.itici_modeli.transfer_payda", "sıfır önde gelen");
    HATA_BEKLE(d, kos(tb, {{A, "transfer_payda = [1.0, 89.0, 9258.0, 108700.0]", "transfer_payda = [1.0, -89.0, 9258.0, 108700.0]"}}), "A5-K105", "arac.itici_modeli.transfer_payda", "kararsız");
    HATA_BEKLE(d, kos(tb, {{A, "transfer_pay = [6136.0, 108700.0]", "transfer_pay = [6136.0, 108701.0]"}}), "A5-K105", "arac.itici_modeli.transfer_pay", "DC kazanç");
    HATA_BEKLE(d, kos(tb, {{A, "pwm_min_us = 1100", "pwm_min_us = 1475"}}), "A5-K31", "arac.esc", "pwm");
    // A5-K32: norm toleransının iki yanı; D6 kamera istisnası
    GECERLI_BEKLE(d, kos(tb, {{S, "yonelim = [1.0, 0.0, 0.0, 0.0]\nyuklu", "yonelim = [1.0000009, 0.0, 0.0, 0.0]\nyuklu"}}), "norm 9e-7");
    HATA_BEKLE(d, kos(tb, {{S, "yonelim = [1.0, 0.0, 0.0, 0.0]\nyuklu", "yonelim = [1.0000011, 0.0, 0.0, 0.0]\nyuklu"}}), "A5-K32", "baslangic.yonelim", "norm 1.1e-6");
    HATA_BEKLE(d, kos(tb, {{S, "yonelim = [1.0, 0.0, 0.0, 0.0]\nyuklu", "yonelim = [0.7071096096080154, 0.0, 0.0, 0.7071096096080154]\nyuklu"}}), "A5-K32", "baslangic.yonelim", "D6 başlangıçta");
    GECERLI_BEKLE(d, kos(tb, {{A, "\nmontaj = { konum = [0.21, 0.0, -0.02], yonelim = [1.0, 0.0, 0.0, 0.0] }", "\nmontaj = { konum = [0.21, 0.0, -0.02], yonelim = [0.7071096096080154, 0.0, 0.0, 0.7071096096080154] }"}}), "D6 kamera montajında");
    HATA_BEKLE(d, kos(tb, {{A, "montaj = { konum = [0.0, 0.0, -0.03], yonelim = [1.0, 0.0, 0.0, 0.0] }        # [MV]\n\n[arac.sensorler.mag0]", "montaj = { konum = [0.0, 0.0, -0.03], yonelim = [0.7071096096080154, 0.0, 0.0, 0.7071096096080154] }\n\n[arac.sensorler.mag0]"}}), "A5-K32", "arac.sensorler.imu0.montaj.yonelim", "D6 IMU montajında");
    HATA_BEKLE(d, kos(tb, {{A, "nominal_montaj = { konum = [0.21, 0.0, -0.02], yonelim = [1.0, 0.0, 0.0, 0.0] }", "nominal_montaj = { konum = [0.21, 0.0, -0.02], yonelim = [1.00001, 0.0, 0.0, 0.0] }"}}), "A5-K32", "arac.sensorler.kamera0.ayar.nominal_montaj.yonelim", "nominal montaj 2e-5");
    HATA_BEKLE(d, kos(tb, {{A, "kimlik = 11", "kimlik = 10"}}), "A5-K33", "arac.sensorler.sizinti0.kimlik", "kimlik tekil");
    HATA_BEKLE(d, kos(tb, {{A, "tur = \"gnss\"\nkimlik = 4\ninst = 0\nprofil = \"neo_m9n\"", "tur = \"basinc\"\nkimlik = 4\ninst = 0\nprofil = \"ms5837_30ba\""}}), "A5-K33", "arac.sensorler.gnss0.inst", "(tur, inst) tekil");
    // ---------------------------------------------------------------- §4.4 ortam
    HATA_BEKLE(d, kos(tb, {{S, "", "\n[ortam.dalga]\nspektrum = \"jonswap\"\nhs = 0.2\n"}}), "A5-K37", "ortam.dalga.tepe_periyodu_ns", "dalga");
    HATA_BEKLE(d, kos(tb, {{S, "", "\n[ortam.akinti.gauss_markov]\netkin = true\ntau_ns = 10\n"}}), "A5-K38", "ortam.akinti.gauss_markov.sigma", "Gauss–Markov");
    HATA_BEKLE(d, kos(tb, {{S, "", "\n[ortam.ruzgar]\nhiz_ned = [1.0, 0.0, 0.1]\n"}}), "A5-K39", "ortam.ruzgar.hiz_ned", "rüzgâr d");
    // ---------------------------------------------------------------- §4.5 sensörler
    HATA_BEKLE(d, kos(tb, {{S, "[arac.sensorler.gnss0]\netkin = false", "[arac.sensorler.gnss0]\netkin = false\nayar = { yakinsama_baslangic_hatasi = 1.0 }"}}), "A5-K40", "arac.sensorler.gnss0.ayar.yakinsama_tau_ns", "yakınsama");
    GECERLI_BEKLE(d, kos(tb, {{A, "hiz_hz = 25.0 ", "hiz_hz = 25.9 "}}), "basınç 25,9 Hz");
    HATA_BEKLE(d, kos(tb, {{A, "hiz_hz = 25.0 ", "hiz_hz = 26.0 "}}), "A5-K42", "arac.sensorler.baro0.hiz_hz", "basınç 26,0 Hz");
    HATA_BEKLE(d, kos(tb, {{S, "manyetik_alan = [25.6e-6, 2.7e-6, 40.3e-6]", "x_manyetik_alan = 0"}}), "A5-K44", "ortam.manyetik_alan", "alan yok");
    HATA_BEKLE(d, kos(tb, {{S, "[ortam.orijin]", "[ortam.x_orijin]"}}), "A5-K45", "ortam.orijin", "orijin yok");
    GECERLI_BEKLE(d, kos(tb, {{S, "[ortam.orijin]", "[ortam.x_orijin]"},
                               {S, "", "\n[arac.sensorler.mag0]\netkin = false\n[arac.sensorler.imu0.ayar]\ndunya_donusu_etkin = false\n"}}),
                  "orijinsiz, dünya dönüşü kapalı");
    HATA_BEKLE(d, kos(tb, {{S, "[ortam.orijin]", "[ortam.x_orijin]"}, {S, "", "\n[arac.sensorler.mag0]\netkin = false\n"}}), "A5-K45", "ortam.orijin", "orijinsiz, dünya dönüşü açık");
    HATA_BEKLE(d, kos(tb, {{S, "", "\n[arac.sensorler.imu0.ayar]\nsuzgec_etkin = true\n"}}), "A5-K46", "arac.sensorler.imu0.ayar.suzgec_tau_ns", "süzgeç");
    // ---------------------------------------------------------------- §4.6 otopilot
    HATA_BEKLE(d, kos(tb, {{S, "", "\n[otopilot]\nparametreler = [{kimlik = 1281, tip = \"f32\", deger = 1.0}, {kimlik = 1281, tip = \"f32\", deger = 2.0}]\n"}}), "A5-K52", "otopilot.parametreler[1].kimlik", "kimlik tekil");
    HATA_BEKLE(d, kos(tb, {{S, "", "\n[otopilot]\nparametreler = [{kimlik = 257, tip = \"u32\", deger = 20.5}]\n"}}), "A5-K52", "otopilot.parametreler[0].deger", "u32 kesirli");
    HATA_BEKLE(d, kos(tb, {{S, "", "\n[otopilot]\nparametreler = [{kimlik = 258, tip = \"u32\", deger = 450000001}]\n"}}), "A5-K52", "otopilot.parametreler[0].deger", "u32 katalog aralığı");
    HATA_BEKLE(d, kos(tb, {{S, "", "\n[otopilot]\nparametreler = [{kimlik = 1281, tip = \"f32\", deger = 3.5e38}]\n"}}), "A5-K52", "otopilot.parametreler[0].deger", "f32 sonsuz");
    {
        const Kosu k = kos(tb, {{S, "", "\n[otopilot]\nparametreler = [{kimlik = 257, tip = \"u32\", deger = 30}, {kimlik = 1281, tip = \"f32\", deger = -2.0}, {kimlik = 258, tip = \"u32\", deger = 60000000.0}]\n"}});
        GECERLI_BEKLE(d, k, "otopilot sınırları");
        const Deger* p = cz(k, "otopilot.parametreler");
        DOGRULA(d, p != nullptr && p->dizi.size() == 3, "otopilot çiftleri aynen yazılmalı");
    }
    // ---------------------------------------------------------------- §4.7 görev
    HATA_BEKLE(d, kos(tb, {{S, "p = [0.25, 0.0, 0.0, 0.0]     # çıkış hızı 0,25 m/s\nlat_e7 = 0\nlon_e7 = 0\nnorth_m = 0.0\neast_m = 0.0\ndepth = 0.0",
                            "p = [0.25, 0.0, 0.0, 0.0]\nlat_e7 = 0\nlon_e7 = 0\nnorth_m = 0.0\neast_m = 0.0\ndepth = 1.0"}}),
               "A5-K55", "gorev.ogeler[1].depth", "SURFACE derinliği");
    HATA_BEKLE(d, kos(tb, {{S, "p = [0.25, 0.0, 0.0, 0.0]", "p = [0.25, 0.1, 0.0, 0.0]"}}), "A5-K55", "gorev.ogeler[1].p[1]", "0 yazan p");
    HATA_BEKLE(d, kos(tb, {{S, "p = [0.3, 0.3, 0.0, 0.0]", "p = [0.05, 0.3, 0.0, 0.0]"}}), "A5-K55", "gorev.ogeler[0].p[0]", "kabul yarıçapı");
    HATA_BEKLE(d, kos(tb, {{S, "east_m = 0.0\ndepth = 0.0\n\n[[gorev.ogeler]]", "east_m = 0.0\ndepth = 0.0\nx_a = 0\n\n[[gorev.ogeler]]"},
                           {S, "north_m = 2.0", "north_m = 10000.5"}}),
               "A5-K55", "gorev.ogeler[0].north_m", "LOCAL sınırı");
    HATA_BEKLE(d, kos(tb, {{S, "", olay("kimlik = \"E12\"\nsinif = \"gcs_komutu\"\nzaman_ns = 0\nkomut = \"DISARM\"")}}), "A5-K60", "olaylar[2].kimlik", "E<n>");
    HATA_BEKLE(d, kos(tb, {{S, "", olay("kimlik = \"arm\"\nsinif = \"gcs_komutu\"\nzaman_ns = 0\nkomut = \"DISARM\"")}}), "A5-K60", "olaylar[2].kimlik", "yinelenen kimlik");
    // A5-K62: aynı fizik sınırı (1 ns → 1,25 ms) ve farklı sınırlar
    HATA_BEKLE(d, kos(tb, {{S, "", olay("kimlik = \"a\"\nsinif = \"emniyet_girdisi\"\nzaman_ns = 1\ngirdi = \"fis\"\nhedef = \"f1\"\ndeger = false") +
                                       olay("kimlik = \"b\"\nsinif = \"emniyet_girdisi\"\nzaman_ns = 1_250_000\ngirdi = \"acil_durdurma\"\ndeger = false")}}),
               "A5-K62", "olaylar[3]", "aynı sınır");
    {
        const Kosu k = kos(tb, {{S, "", olay("kimlik = \"a\"\nsinif = \"emniyet_girdisi\"\nzaman_ns = 1\ngirdi = \"fis\"\nhedef = \"f1\"\ndeger = false") +
                                            olay("kimlik = \"b\"\nsinif = \"emniyet_girdisi\"\nzaman_ns = 1_250_001\ngirdi = \"acil_durdurma\"\ndeger = false")}});
        GECERLI_BEKLE(d, k, "farklı sınır");
        DOGRULA(d, var(k, "A5-K71", "olaylar[3]", Onem::uyari), "emniyeti azaltan atama uyarısı (A5-K71)");
    }
    HATA_BEKLE(d, kos(tb, {{S, "", "\n[baglanti]\netkin_profil = \"ideal\"\n[baglanti.profiller.ideal]\nkademe = \"L0\"\n[baglanti.profiller.yavas]\nkademe = \"L1\"\n" +
                                       olay("kimlik = \"a\"\nsinif = \"baglanti\"\nzaman_ns = 0\ntur = \"profil_degisimi\"\nyon = \"iki_yon\"\nprofil = \"ideal\"") +
                                       olay("kimlik = \"b\"\nsinif = \"baglanti\"\nzaman_ns = 0\ntur = \"profil_degisimi\"\nyon = \"iki_yon\"\nprofil = \"yavas\"")}}),
               "A5-K62", "olaylar[3]", "profil çelişkisi");
    HATA_BEKLE(d, kos(tb, {{S, "", olay("kimlik = \"s\"\nsinif = \"sensor\"\nzaman_ns = 0\ntur = \"ofset\"\nhedef = \"yok0\"\nsiddet = {ofset = 1.0}")}}), "A5-K65", "olaylar[2].hedef", "sensör yok");
    HATA_BEKLE(d, kos(tb, {{S, "", olay("kimlik = \"s\"\nsinif = \"sensor\"\nzaman_ns = 0\ntur = \"dvl_isin_kaybi\"\nhedef = \"imu0\"\nkanal_maskesi = 3")}}), "A5-K65", "olaylar[2].hedef", "dvl_isin_kaybi imu'da");
    HATA_BEKLE(d, kos(tb, {{S, "", olay("kimlik = \"s\"\nsinif = \"sensor\"\nzaman_ns = 0\ntur = \"gurultu_artisi\"\nhedef = \"ping1d0\"\nsiddet = {k = 2.0}")}}), "A5-K65", "olaylar[2].hedef", "gürültü artışı ping1d'de");
    HATA_BEKLE(d, kos(tb, {{S, "", olay("kimlik = \"a\"\nsinif = \"aktuator\"\nzaman_ns = 0\ntur = \"itki_carpani\"\nhedef = \"t9\"\nsiddet = {carpan = 0.5}")}}), "A5-K65", "olaylar[2].hedef", "itici yok");
    HATA_BEKLE(d, kos(tb, {{S, "", olay("kimlik = \"p\"\nsinif = \"protokol\"\nzaman_ns = 0\ntur = \"dusurme\"\nyon = \"yukari\"\nsiddet = {p = 0.5}")}}), "A5-K65", "olaylar[2].duzlem", "düzlem");
    HATA_BEKLE(d, kos(tb, {{S, "", olay("kimlik = \"w\"\nsinif = \"sistem\"\nzaman_ns = 0\ntur = \"su_girisi\"\nproblar = [2]")}}), "A5-K65", "olaylar[2].problar", "prob sayısı");
    HATA_BEKLE(d, kos(tb, {{S, "", olay("kimlik = \"s\"\nsinif = \"sensor\"\nzaman_ns = 0\ntur = \"gec_teslim\"\nhedef = \"baro0\"\nsiddet = {gecikme_ns = 10, titresim_ns = 11}")}}), "A5-K66", "olaylar[2].siddet.titresim_ns", "titreşim");
    HATA_BEKLE(d, kos(tb, {{S, "", olay("kimlik = \"s\"\nsinif = \"sensor\"\nzaman_ns = 0\ntur = \"kayip\"\nhedef = \"baro0\"\nzaman_deseni = {tur = \"bernoulli\"}")}}), "A5-K67", "olaylar[2].zaman_deseni.p", "bernoulli p");
    HATA_BEKLE(d, kos(tb, {{S, "", olay("kimlik = \"a\"\nsinif = \"aktuator\"\nzaman_ns = 0\ntur = \"esc_sinyal_kaybi\"\nhedef = \"t1\"\nsira_boz = true")}}), "A5-K67", "olaylar[2].sira_boz", "sıra bozma aktüatörde");
    HATA_BEKLE(d, kos(tb, {{S, "", olay("kimlik = \"s\"\nsinif = \"sensor\"\nzaman_ns = 0\ntur = \"ofset\"\nhedef = \"baro0\"")}}), "A5-K68", "olaylar[2].siddet.ofset", "şiddet eksik");
    HATA_BEKLE(d, kos(tb, {{S, "", olay("kimlik = \"s\"\nsinif = \"sensor\"\nzaman_ns = 0\ntur = \"ofset\"\nhedef = \"baro0\"\nsiddet = {ofset = 1.0, olcek = 2.0}")}}), "A5-K68", "olaylar[2].siddet.olcek", "fazla şiddet");
    HATA_BEKLE(d, kos(tb, {{S, "", olay("kimlik = \"a\"\nsinif = \"aktuator\"\nzaman_ns = 0\ntur = \"itki_carpani\"\nhedef = \"t1\"\nsiddet = {carpan = 1.5}")}}), "A5-K68", "olaylar[2].siddet.carpan", "itki çarpanı");
    HATA_BEKLE(d, kos(tb, {{S, "", olay("kimlik = \"s\"\nsinif = \"sensor\"\nzaman_ns = 0\ntur = \"kayip\"\nhedef = \"baro0\"\nkanal_maskesi = 3")}}), "A5-K69", "olaylar[2].kanal_maskesi", "maske");
    HATA_BEKLE(d, kos(tb, {{S, "", olay("kimlik = \"s\"\nsinif = \"sensor\"\nzaman_ns = 0\ntur = \"dvl_isin_kaybi\"\nhedef = \"dvl0\"")}}), "A5-K69", "olaylar[2].kanal_maskesi", "DVL maskesi zorunlu");
    GECERLI_BEKLE(d, kos(tb, {{S, "", olay("kimlik = \"s\"\nsinif = \"sensor\"\nzaman_ns = 0\ntur = \"dvl_isin_kaybi\"\nhedef = \"dvl0\"\nkanal_maskesi = 15")}}), "DVL maskesi 15");
    HATA_BEKLE(d, kos(tb, {{S, "komut = \"SET_MODE\"\nkip = \"AUTO\"", "komut = \"SET_MODE\""}}), "A5-K72", "olaylar[1].kip", "SET_MODE kipsiz");
    HATA_BEKLE(d, kos(tb, {{S, "komut = \"ARM\"", "komut = \"ARM\"\nkip = \"AUTO\""}}), "A5-K72", "olaylar[0].kip", "ARM'da kip");
    // ---------------------------------------------------------------- §4.8
    HATA_BEKLE(d, kos(tb, {{S, "", "\n[baglanti]\netkin_profil = \"yok\"\n[baglanti.profiller.ideal]\nkademe = \"L0\"\n"}}), "A5-K74", "baglanti.etkin_profil", "etkin profil");
    HATA_BEKLE(d, kos(tb, {{S, "", "\n[baglanti]\netkin_profil = \"p\"\n[baglanti.profiller.p]\nkademe = \"L1\"\nyukari = {kayip_modeli = \"bernoulli\"}\n"}}), "A5-K74", "baglanti.profiller.p.yukari.kayip_p", "bernoulli");
    HATA_BEKLE(d, kos(tb, {{S, "", "\n[baglanti]\netkin_profil = \"p\"\n[baglanti.profiller.p]\nkademe = \"L0\"\nasagi = {gecikme_taban_ns = 5}\n"}}), "A5-K74", "baglanti.profiller.p.asagi.gecikme_taban_ns", "L0 varsayılan dışı");
    HATA_BEKLE(d, kos(tb, {{S, "", "\n[baglanti]\netkin_profil = \"p\"\n[baglanti.profiller.p]\nkademe = \"L1\"\ntasima = \"stream\"\n"}}), "A5-K74", "baglanti.profiller.p.stream", "stream tablosu");
    HATA_BEKLE(d, kos(tb, {{S, "kip = \"sil\"", "kip = \"hil_1x\"\nsil_hiz_carpani = 2.0"}}), "A5-K76", "calistirma.sil_hiz_carpani", "HIL'de SIL alanı");
    // ---------------------------------------------------------------- §4.9, §4.13
    HATA_BEKLE(d, kos(tb, {{S, "kip = \"sil\"", "kip = \"hil_bariyer\""}, {S, "sonar = \"sade\"", "sonar = \"tam\"\nsonar_tam = {isin_sayisi = 30000}"}}), "A5-K79", "kalite.sonar_tam.isin_sayisi", "kartlı 30 000");
    {
        const Kosu k = kos(tb, {{S, "kip = \"sil\"", "kip = \"hil_bariyer\""}, {S, "sonar = \"sade\"", "sonar = \"tam\"\nsonar_tam = {isin_sayisi = 40000}"}});
        GECERLI_BEKLE(d, k, "kartlı 40 000");
        DOGRULA(d, var(k, "A5-K79", "kalite.sonar_tam.isin_sayisi", Onem::uyari), "kartlı tam sonar uyarısı");
        const Deger* kan = cz(k, "meta.olcum_kanitlari");
        DOGRULA(d, kan != nullptr && kan->dizi.size() == 1 && kan->dizi[0].bul("yol")->dizgi == "kalite.sonar_tam.isin_sayisi",
                "40 000 için ölçüm kanıtı");
    }
    {
        const Kosu k = kos(tb, {{S, "sonar = \"sade\"", "sonar = \"tam\"\nsonar_tam = {isin_sayisi = 30000}"}});
        GECERLI_BEKLE(d, k, "SIL 30 000");
        DOGRULA(d, var(k, "A5-K79", "kalite.sonar_tam.isin_sayisi", Onem::uyari), "ölçülmemiş ışın uyarısı");
        DOGRULA(d, cz(k, "meta.olcum_kanitlari") != nullptr && cz(k, "meta.olcum_kanitlari")->dizi.empty(), "ölçülmemiş değere kanıt yazılmaz");
    }
    HATA_BEKLE(d, kos(tb, {{S, "gorunum = \"sade\"\nkamera = \"kapali\"", "gorunum = \"kapali\"\nkamera = \"sade\""}}), "A5-K80", "kalite.kamera", "görünüm kapalı");
    HATA_BEKLE(d, kos(tb, {{S, "[arac.sensorler.kamera0]\netkin = false", "[arac.sensorler.kamera0]\netkin = true"}}), "A5-K80", "arac.sensorler.kamera0.etkin", "kamera kapalı, sensör etkin");
    HATA_BEKLE(d, kos(tb, {{S, "kayit = \"telemetri\"", "kayit = \"telemetri\"\nkamera_gerekli = true"}}), "A5-K80", "kalite.kamera_gerekli", "kamera gerekli");
    const Degisiklik kamera_ac[] = {{S, "[arac.sensorler.kamera0]\netkin = false", "[arac.sensorler.kamera0]\netkin = true"},
                                    {S, "kamera = \"kapali\"\nsonar", "kamera = \"sade\"\nsonar"}};
    GECERLI_BEKLE(d, kos(tb, {kamera_ac[0], kamera_ac[1]}), "kamera açık");
    HATA_BEKLE(d, kos(tb, {kamera_ac[0], kamera_ac[1], {A, "pozlama_ns = 8_000_000", "pozlama_ns = 8_000_001"}}), "A5-K114", "arac.sensorler.kamera0.ayar.pozlama_ns", "tek pozlama");
    GECERLI_BEKLE(d, kos(tb, {kamera_ac[0], kamera_ac[1], {A, "cx = 319.5", "cx = 639.5"}}), "cx = g − 0,5");
    HATA_BEKLE(d, kos(tb, {kamera_ac[0], kamera_ac[1], {A, "cx = 319.5", "cx = 639.6"}}), "A5-K114", "arac.sensorler.kamera0.ayar.ic_tam.cx", "cx = g − 0,4");
    HATA_BEKLE(d, kos(tb, {kamera_ac[0], kamera_ac[1], {A, "kalibrasyon_surumu = 1 ", "kalibrasyon_surumu = 65536 "}}), "A5-K10", "arac.sensorler.kamera0.ayar.kalibrasyon_surumu", "kalibrasyon sürümü 65 536");
    HATA_BEKLE(d, kos(tb, {kamera_ac[0], kamera_ac[1], {A, "kalibrasyon_surumu = 1 ", "kalibrasyon_surumu = 0 "}}), "A5-K10", "arac.sensorler.kamera0.ayar.kalibrasyon_surumu", "kalibrasyon sürümü 0");
    HATA_BEKLE(d, kos(tb, {kamera_ac[0], kamera_ac[1], {A, "pozlama_ns = 8_000_000", "x_p = 1"}}), "A5-K114", "arac.sensorler.kamera0.ayar.pozlama_ns", "pozlama eksik");
    HATA_BEKLE(d, kos(tb, {kamera_ac[0], kamera_ac[1], {A, "tur = \"kamera\"\nkimlik = 8\ninst = 0", "tur = \"kamera\"\nkimlik = 8\ninst = 1"}}), "A5-K114", "arac.sensorler.kamera0.inst", "kamera inst");
    HATA_BEKLE(d, kos(tb, {kamera_ac[0], kamera_ac[1], {A, "bozulma = [0.0, 0.0, 0.0, 0.0, 0.0]", "bozulma = [0.1, 0.0, 0.0, 0.0, 0.0]"}}), "A5-K114", "arac.sensorler.kamera0.ayar.bozulma", "bozulmasız katsayı");
    HATA_BEKLE(d, kos(tb, {kamera_ac[0], kamera_ac[1], {A, "uzak_m = 40.0", "uzak_m = 0.04"}}), "A5-K114", "arac.sensorler.kamera0.ayar.uzak_m", "kırpma");
    HATA_BEKLE(d, kos(tb, {{S, "yuklu_tupler = [\"f1\", \"f2\"]", "yuklu_tupler = [\"f1\", \"f1\"]"}}), "A5-K86", "baslangic.yuklu_tupler[1]", "yinelenen tüp");
    {
        const Kosu k = kos(tb, {{S, "pim = { f1 = true, f2 = true }", "pim = { f1 = false, f2 = true }"}});
        DOGRULA(d, k.s.rapor.gecerli && var(k, "A5-K71", "emniyet_girdileri.pim.f1", Onem::uyari), "emniyeti azaltan başlangıç (A5-K71)");
    }
    // A5-K116: 61 hedef kabul (62 bozuk dosya takımında)
    {
        std::string h;
        for (int i = 2; i < 62; ++i) {
            h += "\n[hedefler.h" + std::to_string(i) + "]\nkimlik = " + std::to_string(i) +
                 "\nvarlik = \"hedef_ornek\"\nkonum = [4.0, 0.0, 1.5]\nyonelim = [1.0, 0.0, 0.0, 0.0]\n";
        }
        GECERLI_BEKLE(d, kos(tb, {{S, "", h}}), "61 hedef");
    }
    return d.hata;
}

int test_kosullu_varsayilanlar(const TestBaglami& tb)
{
    Denetci d;
    const Kosu altin = kos(tb, {});
    GECERLI_BEKLE(d, altin, "örnek");
    // su.yogunluk, su.tuzluluk: havuz / açık su / verilmiş
    DOGRULA(d, sayi(altin, "ortam.su.yogunluk") == 1000.0 && sayi(altin, "ortam.su.tuzluluk") == 0.0, "havuz su");
    {
        const Kosu k = kos(tb, {{S, "tur = \"havuz\"", "tur = \"acik_su\""}});
        GECERLI_BEKLE(d, k, "açık su");
        DOGRULA(d, sayi(k, "ortam.su.yogunluk") == 1025.0 && sayi(k, "ortam.su.tuzluluk") == 0.035, "açık su değerleri");
        const Kosu v = kos(tb, {{S, "sicaklik_c = 20.0", "sicaklik_c = 20.0\nyogunluk = 1010.0\ntuzluluk = 0.01"}});
        DOGRULA(d, sayi(v, "ortam.su.yogunluk") == 1010.0 && sayi(v, "ortam.su.tuzluluk") == 0.01, "verilmiş su değerleri");
    }
    // dalga.gamma
    {
        const Kosu j = kos(tb, {{S, "", "\n[ortam.dalga]\nspektrum = \"jonswap\"\nhs = 0.2\ntepe_periyodu_ns = 4_000_000_000\nyon = 0.0\n"}});
        GECERLI_BEKLE(d, j, "jonswap");
        DOGRULA(d, sayi(j, "ortam.dalga.gamma") == 3.3, "jonswap gamma 3,3");
        const Kosu p = kos(tb, {{S, "", "\n[ortam.dalga]\nspektrum = \"pierson_moskowitz\"\nhs = 0.2\ntepe_periyodu_ns = 4_000_000_000\nyon = 0.0\n"}});
        DOGRULA(d, p.s.rapor.gecerli && cz(p, "ortam.dalga.gamma") == nullptr, "pierson_moskowitz'te gamma yazılmaz");
        DOGRULA(d, cz(altin, "ortam.dalga.gamma") == nullptr, "dalgasızda gamma yok");
    }
    // sil_hiz_carpani, sil_azami_hiz
    DOGRULA(d, sayi(altin, "calistirma.sil_hiz_carpani") == 1.0 && cz(altin, "calistirma.sil_azami_hiz")->mantik == false, "SIL varsayılanları");
    {
        const Kosu h = kos(tb, {{S, "kip = \"sil\"", "kip = \"hil_1x\""}});
        GECERLI_BEKLE(d, h, "HIL");
        DOGRULA(d, cz(h, "calistirma.sil_hiz_carpani") == nullptr && cz(h, "calistirma.sil_azami_hiz") == nullptr, "HIL'de SIL alanları yazılmaz");
    }
    // sonar_tam
    {
        const Kosu t = kos(tb, {{S, "sonar = \"sade\"", "sonar = \"tam\"\nsonar_tam = {isin_sayisi = 20000}"}});
        GECERLI_BEKLE(d, t, "tam sonar");
        DOGRULA(d, sayi(t, "kalite.sonar_tam.azami_sekme") == 2.0 && sayi(t, "kalite.sonar_tam.hucre_kenari") == 0.01 &&
                       sayi(t, "kalite.sonar_tam.yuzey_yamasi_araligi") == 0.05,
                "tam sonar varsayılanları");
        const Kosu s = kos(tb, {{S, "sonar = \"sade\"", "sonar = \"sade\"\nsonar_tam = {isin_sayisi = 20000}"}});
        GECERLI_BEKLE(d, s, "sade sonar, tablo verilmiş");
        DOGRULA(d, cz(s, "kalite.sonar_tam") == nullptr, "sade kademede sonar_tam çıkarılır");
    }
    // hiz_hz: hızı olan türde profil hızı; hızsız türde yazılmaz
    {
        const Kosu k = kos(tb, {{A, "hiz_hz = 100.0 ", "x_h = 1 "}});
        GECERLI_BEKLE(d, k, "manyetometre hızsız");
        DOGRULA(d, sayi(k, "arac.sensorler.mag0.hiz_hz") == 100.0, "profil hızı yazılır");
        DOGRULA(d, cz(altin, "arac.sensorler.imu0.hiz_hz") == nullptr && cz(altin, "arac.sensorler.dvl0.hiz_hz") == nullptr, "IMU ve DVL'de hız yok");
        const Kosu v = kos(tb, {{A, "hiz_hz = 100.0 ", "hiz_hz = 50.0 "}});
        DOGRULA(d, sayi(v, "arac.sensorler.mag0.hiz_hz") == 50.0, "verilmiş hız korunur");
    }
    // saat_kaymasi_ppm
    DOGRULA(d, sayi(altin, "arac.sensorler.mag0.saat_kaymasi_ppm") == 0.0 && cz(altin, "arac.sensorler.imu0.saat_kaymasi_ppm") == nullptr, "saat kayması");
    {
        const Kosu v = kos(tb, {{S, "", "\n[arac.sensorler.mag0]\nsaat_kaymasi_ppm = 12.5\n"}});
        DOGRULA(d, sayi(v, "arac.sensorler.mag0.saat_kaymasi_ppm") == 12.5, "verilmiş saat kayması");
    }
    // ayar.<k>
    {
        DOGRULA(d, sayi(altin, "arac.sensorler.baro0.ayar.gurultu_sigma") == 20.0, "profil ayarı");
        const Kosu v = kos(tb, {{S, "", "\n[arac.sensorler.baro0.ayar]\ngurultu_sigma = 30.0\n"}});
        GECERLI_BEKLE(d, v, "ayar geçersiz kılma");
        DOGRULA(d, sayi(v, "arac.sensorler.baro0.ayar.gurultu_sigma") == 30.0 && sayi(v, "arac.sensorler.baro0.ayar.kuantalama") == 10.0,
                "verilmiş ayar korunur, ötekiler profilden");
        DOGRULA(d, cz(altin, "arac.sensorler.imu0.ayar.suzgec_tau_ns") == nullptr && cz(altin, "arac.sensorler.dvl0.ayar.c_ayar") == nullptr,
                "yazılmaz işaretliler yazılmaz");
    }
    // nominal_montaj
    {
        const Kosu k = kos(tb, {{A, "nominal_montaj = { konum = [0.21, 0.0, -0.02], yonelim = [1.0, 0.0, 0.0, 0.0] }", "x_nm = 1"},
                                {A, "\nmontaj = { konum = [0.21, 0.0, -0.02], yonelim = [1.0, 0.0, 0.0, 0.0] }", "\nmontaj = { konum = [0.2, 0.0, -0.02], yonelim = [1.0, 0.0, 0.0, 0.0] }"}});
        GECERLI_BEKLE(d, k, "nominal montajsız kamera");
        const Deger* nm = cz(k, "arac.sensorler.kamera0.ayar.nominal_montaj");
        const Deger* m = cz(k, "arac.sensorler.kamera0.montaj");
        DOGRULA(d, nm != nullptr && m != nullptr && esit(*nm, *m), "nominal montaj fizik montajının kopyası");
        const Kosu v = kos(tb, {{A, "\nmontaj = { konum = [0.21, 0.0, -0.02], yonelim = [1.0, 0.0, 0.0, 0.0] }", "\nmontaj = { konum = [0.2, 0.0, -0.02], yonelim = [1.0, 0.0, 0.0, 0.0] }"}});
        DOGRULA(d, sayi(v, "arac.sensorler.kamera0.ayar.nominal_montaj.konum") == -12345.0 &&
                       cz(v, "arac.sensorler.kamera0.ayar.nominal_montaj.konum")->dizi[0].cift() == 0.21,
                "verilmiş nominal montaj korunur");
        DOGRULA(d, cz(altin, "arac.sensorler.imu0.ayar.nominal_montaj") == nullptr, "kamera dışında kopya yok");
    }
    // ortam.su_optigi.*, ortam.isik.*
    {
        const Deger* ke = cz(altin, "meta.kalibre_edilmedi");
        bool listede = false;
        for (const auto& x : ke->dizi) {
            listede = listede || x.dizgi == "ortam.isik.gunes_isinimi_w_m2";
        }
        DOGRULA(d, listede && sayi(altin, "ortam.isik.gunes_isinimi_w_m2") == 1000.0, "KE ışık değeri ve listesi");
        const Kosu v = kos(tb, {{S, "", "\n[ortam.isik]\ngunes_isinimi_w_m2 = 500.0\n"}});
        bool listede2 = false;
        for (const auto& x : cz(v, "meta.kalibre_edilmedi")->dizi) {
            listede2 = listede2 || x.dizgi == "ortam.isik.gunes_isinimi_w_m2";
        }
        DOGRULA(d, !listede2 && sayi(v, "ortam.isik.gunes_isinimi_w_m2") == 500.0, "verilmiş ışık listede değil");
    }
    // olaylar: sure_ns, zaman_deseni.tur, sira_boz, kanal_maskesi
    {
        const Kosu k = kos(tb, {{S, "", olay("kimlik = \"s\"\nsinif = \"sensor\"\nzaman_ns = 0\ntur = \"ofset\"\nhedef = \"baro0\"\nsiddet = {ofset = 1.0}") +
                                           olay("kimlik = \"a\"\nsinif = \"aktuator\"\nzaman_ns = 0\ntur = \"itki_carpani\"\nhedef = \"t1\"\nsiddet = {carpan = 0.5}") +
                                           olay("kimlik = \"s2\"\nsinif = \"sensor\"\nzaman_ns = 0\ntur = \"ofset\"\nhedef = \"baro0\"\nsiddet = {ofset = 1.0}\nsure_ns = 5\nsira_boz = true\nzaman_deseni = {tur = \"bernoulli\", p = 0.5}")}});
        GECERLI_BEKLE(d, k, "olaylar");
        DOGRULA(d, sayi(k, "olaylar") == -12345.0, "olaylar dizi");
        const Deger* o = cz(k, "olaylar");
        const Deger& s = o->dizi[2];
        const Deger& a = o->dizi[3];
        const Deger& s2 = o->dizi[4];
        DOGRULA(d, s.bul("sure_ns")->tam == 0 && s.yol_bul("zaman_deseni.tur")->dizgi == "surekli" &&
                       s.bul("sira_boz")->mantik == false && s.bul("kanal_maskesi")->tam == 4294967295LL,
                "sensör arızası varsayılanları");
        DOGRULA(d, a.bul("sure_ns")->tam == 0 && a.bul("zaman_deseni") == nullptr && a.bul("sira_boz") == nullptr && a.bul("kanal_maskesi") == nullptr,
                "aktüatörde sensör alanları yazılmaz");
        DOGRULA(d, s2.bul("sure_ns")->tam == 5 && s2.bul("sira_boz")->mantik && s2.yol_bul("zaman_deseni.tur")->dizgi == "bernoulli",
                "verilmiş olay alanları korunur");
        DOGRULA(d, o->dizi[0].bul("sure_ns") == nullptr, "anlık komutta sure_ns yazılmaz");
    }
    return d.hata;
}

int test_a7_atiflari(const TestBaglami& tb)
{
    Denetci d;
    HATA_BEKLE(d, kos(tb, {{A, "varlik = \"ida0\"", "varlik = \"yok\""}}), "A5-K19", "arac.varlik", "olmayan araç varlığı");
    HATA_BEKLE(d, kos(tb, {{S, "varlik = \"hedef_ornek\"", "varlik = \"ida0\""}}), "A5-K19", "hedefler.h1.varlik", "yanlış türde varlık");
    HATA_BEKLE(d, kos(tb, {{S, "sahne = \"havuz_ornek\"", "sahne = \"hedef_ornek\""}}), "A5-K19", "ortam.sahne", "statik olmayan sahne");
    HATA_BEKLE(d, kos(tb, {{A, "varlik = \"torpido_ida0\"", "varlik = \"ida0\""}}), "A5-K19", "arac.torpido.varlik", "torpido türü");
    // A5-K18
    {
        const Kosu k = kos(tb, {{S, "", "\n[kalibrasyon.malzeme.havuz_beton.akustik]\ngeri_sacilim_mu = 0.03\n"}});
        GECERLI_BEKLE(d, k, "malzeme geçersiz kılma");
        DOGRULA(d, sayi(k, "kalibrasyon.malzeme.havuz_beton.akustik.geri_sacilim_mu") == 0.03, "geçersiz kılma çözümlenmişte");
    }
    HATA_BEKLE(d, kos(tb, {{S, "", "\n[kalibrasyon.malzeme.mermer.akustik]\ngeri_sacilim_mu = 0.03\n"}}), "A5-K18", "kalibrasyon.malzeme.mermer", "bilinmeyen malzeme");
    HATA_BEKLE(d, kos(tb, {{S, "", "\n[kalibrasyon.malzeme.havuz_beton.akustik]\nrenk = 0.03\n"}}), "A5-K18", "kalibrasyon.malzeme.havuz_beton.akustik.renk", "A7 dışı alan");
    HATA_BEKLE(d, kos(tb, {{S, "", "\n[kalibrasyon.malzeme.havuz_beton.akustik]\ngelis_yasasi = \"lambert_cos\"\n"}}), "A5-K18", "kalibrasyon.malzeme.havuz_beton.akustik.gelis_yasasi", "lambert_cos");
    HATA_BEKLE(d, kos(tb, {{S, "", "\n[kalibrasyon.malzeme.havuz_beton.akustik]\ngeri_sacilim_mu = 0.0\n"}}), "A5-K18", "kalibrasyon.malzeme.havuz_beton.akustik.geri_sacilim_mu", "geniş aralık");
    // A5-K36
    GECERLI_BEKLE(d, kos(tb, {{S, "derinlik = 3.0 ", "derinlik = 3.0005 "}}), "0,5 mm fark");
    HATA_BEKLE(d, kos(tb, {{S, "derinlik = 3.0 ", "derinlik = 3.002 "}}), "A5-K36", "ortam.derinlik", "2 mm fark");
    HATA_BEKLE(d, kos(tb, {{S, "", "\n[ortam.sahne_poz]\nkonum = [0.0, 0.0, 1.0]\n"}}), "A5-K36", "ortam.derinlik", "pozlu taban 4 m, derinlik 3");
    GECERLI_BEKLE(d, kos(tb, {{S, "derinlik = 3.0 ", "derinlik = 4.0 "}, {S, "", "\n[ortam.sahne_poz]\nkonum = [0.0, 0.0, 1.0]\n"}}), "pozlu taban 4 m, derinlik 4");
    HATA_BEKLE(d, kos(tb, {{S, "", "\n[ortam.sahne_poz]\nyonelim = [0.7071067811865476, 0.7071067811865476, 0.0, 0.0]\n"}}), "A5-K36", "ortam.sahne_poz.yonelim", "yatay olmayan poz");
    GECERLI_BEKLE(d, kos(tb, {{S, "", "\n[ortam.sahne_poz]\nyonelim = [0.7071067811865476, 0.0, 0.0, 0.7071067811865476]\n"}}), "düşey eksen dönüşü");
    {
        BellekKaynagi k = temel_kaynak(tb);
        k.dosyalar[kManifest] = *degistir(k.dosyalar[kManifest], "\"taban_derinligi_m\": 3.0,", "");
        k.dosyalar[kSenaryo] = *degistir(k.dosyalar[kSenaryo], "manifest_ozeti = ", "x_manifest_ozeti = ");
        const CozumSonucu r = senaryo_coz(k, kSenaryo);
        DOGRULA(d, r.rapor.gecerli && bulgu_bul(r.rapor, "A5-K36", "ortam.sahne") != nullptr &&
                       bulgu_bul(r.rapor, "A5-K36", "ortam.sahne")->onem == Onem::uyari,
                "taban alanı yoksa uyarı\n" + bulgu_ozeti(r.rapor));
    }
    // A5-K115
    HATA_BEKLE(d, kos(tb, {{S, "kume_surumu = 1 ", "kume_surumu = 2 "}}), "A5-K115", "varliklar.kume_surumu", "yanlış küme sürümü");
    HATA_BEKLE(d, kos(tb, {{S, "29063c2d7f89a80e", "39063c2d7f89a80e"}}), "A5-K115", "varliklar.manifest_ozeti", "yanlış özet");
    {
        const Kosu k = kos(tb, {{S, "kume_surumu = 1 ", "x_ks = 1 "}, {S, "manifest_ozeti = ", "x_mo = "}});
        GECERLI_BEKLE(d, k, "sürüm ve özet verilmemiş");
        DOGRULA(d, var(k, "A5-K115", "varliklar.kume_surumu", Onem::uyari) && var(k, "A5-K115", "varliklar.manifest_ozeti", Onem::uyari),
                "verilmemişse iki uyarı");
    }
    HATA_BEKLE(d, kos(tb, {{S, "kume = \"ornek\"", "kume = \"acik_su\""}}), "A5-K115", "varliklar.kume", "başka küme (temel kaynakta manifesti yok)");
    return d.hata;
}

}  // namespace ida::senaryo::test
