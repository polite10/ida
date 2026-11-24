// sol incelemesinin (docs/gorevler/inceleme/W4-senaryo-sol.md) karşı örnekleri: her bulgu
// için düzeltmeden önce düşen, düzeltmeden sonra geçen test.
#include "ic.hpp"
#include "test_destek.hpp"

#include "ida/senaryo/fizik_yukleyici.hpp"

#include <chrono>
#include <type_traits>
#include <cmath>
#include <filesystem>
#include <fstream>

namespace ida::senaryo::test {

namespace {

bool var(const std::vector<Bulgu>& b, const std::string& kural, const std::string& yol = "*",
         Onem onem = Onem::hata)
{
    for (const auto& x : b) {
        if (x.kural == kural && x.onem == onem && (yol == "*" || x.yol == yol)) {
            return true;
        }
    }
    return false;
}

std::string ozet(const std::vector<Bulgu>& b)
{
    return bulgu_ozeti(DogrulamaRaporu{false, std::nullopt, {}, b});
}

// Altın çözümlenmiş senaryonun ağacını `degistir` ile değiştirip kanonik baytlarını ve
// özetini üreterek yeniden yükler (geçerli SHA-256, değiştirilmiş içerik).
template <class F>
YenidenYukleme sahte_yukle(const TestBaglami& tb, F degistir_islevi)
{
    const std::string altin = dosya_oku(tb.kok + "/scenarios/ornek-havuz.cozumlenmis.json");
    ic::JsonOkuma j = ic::json_oku(altin, 0);
    degistir_islevi(*j.kok);
    const std::string b = kanonik_json(*j.kok);
    return cozumlenmis_yukle(b, sha256_hex(b));
}

Deger emniyet_olayi(const std::string& kimlik, const std::string& girdi, const char* hedef, bool deger)
{
    Deger o = Deger::yeni_tablo();
    o.tablo["kimlik"] = Deger::yeni_dizgi(kimlik);
    o.tablo["sinif"] = Deger::yeni_dizgi("emniyet_girdisi");
    o.tablo["zaman_ns"] = Deger::yeni_tamsayi(1'250'000);
    o.tablo["girdi"] = Deger::yeni_dizgi(girdi);
    if (hedef != nullptr) {
        o.tablo["hedef"] = Deger::yeni_dizgi(hedef);
    }
    o.tablo["deger"] = Deger::yeni_mantiksal(deger);
    return o;
}

}  // namespace

// #1 EMNİYET: yeniden yükleme anlamsal ve sürüm denetimlerini atlamaz.
int test_sol_1_yeniden_yukleme_anlamsal(const TestBaglami& tb)
{
    Denetci d;
    // TOML yolu: aynı sınırda kısıtlayan + gevşeten zamanlı emniyet ataması → K62.
    BellekKaynagi k = temel_kaynak(tb);
    k.dosyalar[kSenaryo] +=
        "\n[[olaylar]]\nkimlik = \"bas\"\nsinif = \"emniyet_girdisi\"\nzaman_ns = 1_250_000\ngirdi = \"acil_durdurma\"\ndeger = true\n"
        "\n[[olaylar]]\nkimlik = \"cek\"\nsinif = \"emniyet_girdisi\"\nzaman_ns = 1_250_000\ngirdi = \"pim\"\nhedef = \"f1\"\ndeger = false\n";
    const CozumSonucu t = senaryo_coz(k, kSenaryo);
    DOGRULA(d, !t.rapor.gecerli && var(t.rapor.bulgular, "A5-K62"), "TOML: K62 ile ret bekleniyordu");
    // JSON yolu: aynı olaylar çözümlenmiş senaryoya eklenip geçerli özetle yüklenir → aynı ret.
    const YenidenYukleme j = sahte_yukle(tb, [](Deger& s) {
        s.tablo["olaylar"].dizi.push_back(emniyet_olayi("bas", "acil_durdurma", nullptr, true));
        s.tablo["olaylar"].dizi.push_back(emniyet_olayi("cek", "pim", "f1", false));
    });
    DOGRULA(d, !j.tamam && var(j.bulgular, "A5-K62"), "JSON: K62 ile ret bekleniyordu\n" + ozet(j.bulgular));
    const YenidenYukleme q = sahte_yukle(tb, [](Deger& s) {
        s.yol_bul("baslangic.yonelim")->dizi[0] = Deger::yeni_sayi(1.1);
    });
    DOGRULA(d, !q.tamam && var(q.bulgular, "A5-K32", "baslangic.yonelim"), "JSON: K32 bekleniyordu\n" + ozet(q.bulgular));
    const YenidenYukleme u = sahte_yukle(tb, [](Deger& s) {
        s.yol_bul("arac.batarya")->tablo["sigorta_i2t"] = Deger::yeni_sayi(10.0);
    });
    DOGRULA(d, !u.tamam && var(u.bulgular, "A5-K102", "arac.batarya.sigorta_i2t"), "JSON: K102 bekleniyordu\n" + ozet(u.bulgular));
    const YenidenYukleme m = sahte_yukle(tb, [](Deger& s) {
        s.yol_bul("meta")->tablo["sema_surumu"] = Deger::yeni_dizgi("1.1");
    });
    DOGRULA(d, !m.tamam && var(m.bulgular, "A5-K04", "meta.sema_surumu"), "JSON: K04 bekleniyordu\n" + ozet(m.bulgular));
    return d.hata;
}

// #1 (tür düzeyi): yükleyiciler yalnız doğrulanmış senaryo alır; ham ağaç derlenmez.
static_assert(!std::is_constructible_v<DogrulanmisSenaryo, Deger>,
              "DogrulanmisSenaryo yalnız doğrulama akışında üretilir");
static_assert(!std::is_constructible_v<DogrulanmisSenaryo, Deger, std::string, std::string>,
              "DogrulanmisSenaryo kurucusu kapalıdır");
template <class T>
concept FizikKurulabilir = requires(const T& t) { fizik_kur(t); };
static_assert(!FizikKurulabilir<Deger>, "fizik_kur ham ağaç almaz");
static_assert(FizikKurulabilir<DogrulanmisSenaryo>, "fizik_kur doğrulanmış senaryo alır");

int test_sol_1_tur_zorlamasi(const TestBaglami& tb)
{
    Denetci d;
    // Geçersiz senaryo yükleyiciye ulaşacak bir değer üretmez.
    BellekKaynagi k = temel_kaynak(tb);
    k.dosyalar[kArac] = *degistir(k.dosyalar[kArac], "pwm_min_us = 1100", "pwm_min_us = 1475");
    const CozumSonucu s = senaryo_coz(k, kSenaryo);
    DOGRULA(d, !s.rapor.gecerli && !s.cozumlenmis.has_value(), "geçersiz senaryoda doğrulanmış ağaç olmamalı");
    const YenidenYukleme y = sahte_yukle(tb, [](Deger& a) {
        a.tablo["olaylar"].dizi.push_back(emniyet_olayi("bas", "acil_durdurma", nullptr, true));
        a.tablo["olaylar"].dizi.push_back(emniyet_olayi("cek", "pim", "f1", false));
    });
    DOGRULA(d, !y.cozumlenmis.has_value(), "reddedilen yeniden yükleme doğrulanmış ağaç vermemeli");
    return d.hata;
}

// #2 başlık sonundaki yorum ön taramayı atlatmaz.
int test_sol_2_baslik_yorumu(const TestBaglami&)
{
    Denetci d;
    {
        ic::Bulgucu b;
        const auto t = ic::toml_oku("[x_a] # \"\"\"\n[!]\n# \"\"\"\n", 0, b);
        DOGRULA(d, !t.kok && var(b.bulgular, "A5-K01"), "geçersiz başlık K01 ile reddedilmeli");
    }
    {
        std::string s = "[x_a] # '''\n[x_b";
        for (int i = 0; i < 200'000; ++i) {
            s += ".a";
        }
        s += "]\n# '''\n";
        ic::Bulgucu b;
        const auto t = ic::toml_oku(s, 0, b);
        DOGRULA(d, !t.kok && var(b.bulgular, "A5-K01"), "yorum arkasına gizlenmiş derin başlık K01 ile reddedilmeli");
    }
    return d.hata;
}

// #3 sözdizimi hatası sayıya dönüşmez.
int test_sol_3_bozuk_sayi(const TestBaglami&)
{
    Denetci d;
    for (const char* y : {"0.0e+", "0.0e-", "1__0.5", "1_.5", "1._5", "0.0e+_1", "00.5", "1.e5", "+.5"}) {
        ic::Bulgucu b;
        const auto t = ic::toml_oku(std::string("x_a = ") + y + "\n", 0, b);
        const bool sayi = t.kok && t.kok->bul("x_a") != nullptr && t.kok->bul("x_a")->tur == Tur::sayi;
        DOGRULA(d, !sayi && var(b.bulgular, "A5-K01"), std::string("K01 bekleniyordu: ") + y + "\n" + ozet(b.bulgular));
    }
    return d.hata;
}

// #4 büyük tamsayı belirteci (JCS 1e19) yeniden yüklenir.
int test_sol_4_buyuk_tamsayi(const TestBaglami& tb)
{
    Denetci d;
    BellekKaynagi k = temel_kaynak(tb);
    k.dosyalar[kSenaryo] = *degistir(k.dosyalar[kSenaryo], "ana_tohum = 1", "ana_tohum = 1\nx_buyuk = 1e19\nx_eksi = -1e19\nx_orta = 1e18");
    const CozumSonucu s = senaryo_coz(k, kSenaryo);
    DOGRULA(d, s.rapor.gecerli, "çözüm geçerli olmalı");
    DOGRULA(d, s.kanonik.find("\"x_buyuk\":10000000000000000000") != std::string::npos, "JCS yazımı");
    const YenidenYukleme y = cozumlenmis_yukle(s.kanonik, s.rapor.davranis_ozeti.value_or(""));
    DOGRULA(d, y.tamam, "kendi çıktısı yeniden yüklenmeli\n" + ozet(y.bulgular));
    return d.hata;
}

// #5 hedeflerin yükleme kuralları.
int test_sol_5_hedefler(const TestBaglami& tb)
{
    Denetci d;
    struct Durum {
        const char* hareket;
        const char* eksik;
    };
    const Durum durumlar[] = {
        {"tur = \"sabit_hiz\"", "hiz_ned"},
        {"tur = \"rastgele_yuruyus\", azami_hiz = 1.0", "yuruyus_yogunlugu"},
        {"tur = \"rastgele_yuruyus\", yuruyus_yogunlugu = 0.1", "azami_hiz"},
        {"tur = \"sinus\", periyot_ns = 10, faz = 0.0", "genlik_ned"},
        {"tur = \"sinus\", genlik_ned = [1.0, 0.0, 0.0], faz = 0.0", "periyot_ns"},
        {"tur = \"sinus\", genlik_ned = [1.0, 0.0, 0.0], periyot_ns = 10", "faz"},
    };
    for (const auto& du : durumlar) {
        BellekKaynagi k = temel_kaynak(tb);
        k.dosyalar[kSenaryo] = *degistir(k.dosyalar[kSenaryo], "yonelim = [1.0, 0.0, 0.0, 0.0]\n\n[gorev]",
                                         std::string("yonelim = [1.0, 0.0, 0.0, 0.0]\nhareket = { ") + du.hareket + " }\n\n[gorev]");
        const CozumSonucu s = senaryo_coz(k, kSenaryo);
        DOGRULA(d, !s.rapor.gecerli && var(s.rapor.bulgular, "A5-K05", std::string("hedefler.h1.hareket.") + du.eksik),
                std::string("eksik hareket alanı reddedilmeli: ") + du.eksik + "\n" + bulgu_ozeti(s.rapor));
    }
    {
        BellekKaynagi k = temel_kaynak(tb);
        k.dosyalar[kSenaryo] += "\n[hedefler.h2]\nkimlik = 1\nvarlik = \"hedef_ornek\"\nkonum = [5.0, 0.0, 1.5]\nyonelim = [1.0, 0.0, 0.0, 0.0]\n";
        const CozumSonucu s = senaryo_coz(k, kSenaryo);
        DOGRULA(d, !s.rapor.gecerli && var(s.rapor.bulgular, "A5-K10", "hedefler.h2.kimlik"),
                "yinelenen hedef kimliği reddedilmeli\n" + bulgu_ozeti(s.rapor));
    }
    {
        BellekKaynagi k = temel_kaynak(tb);
        k.dosyalar[kSenaryo] = *degistir(k.dosyalar[kSenaryo], "yonelim = [1.0, 0.0, 0.0, 0.0]\n\n[gorev]",
                                         "yonelim = [1.0, 0.0, 0.0, 0.0]\nhareket = { tur = \"sinus\", genlik_ned = [1.0, 0.0, 0.0], periyot_ns = 10, faz = 0.0 }\n\n[gorev]");
        DOGRULA(d, senaryo_coz(k, kSenaryo).rapor.gecerli, "eksiksiz sinüs hareketi geçerli");
    }
    return d.hata;
}

// #6 K71: bırakılmış acil durdurma başlangıcı ayrı uyarı.
int test_sol_6_acil_durdurma_uyarisi(const TestBaglami& tb)
{
    Denetci d;
    const BellekKaynagi k = temel_kaynak(tb);
    const CozumSonucu s = senaryo_coz(k, kSenaryo);
    const Bulgu* b = bulgu_bul(s.rapor, "A5-K71", "emniyet_girdileri.acil_durdurma");
    DOGRULA(d, s.rapor.gecerli && b != nullptr && b->onem == Onem::uyari &&
                   b->satir == satir_bul(k.dosyalar.at(kSenaryo), "acil_durdurma = false"),
            "açıkça yazılmış bırakılmış acil durdurma: satırlı K71 uyarısı\n" + bulgu_ozeti(s.rapor));
    BellekKaynagi v = k;
    v.dosyalar[kSenaryo] = *degistir(v.dosyalar[kSenaryo], "acil_durdurma = false\n", "");
    const CozumSonucu s2 = senaryo_coz(v, kSenaryo);
    const Bulgu* b2 = bulgu_bul(s2.rapor, "A5-K71", "emniyet_girdileri.acil_durdurma");
    DOGRULA(d, s2.rapor.gecerli && b2 != nullptr && !b2->satir.has_value(), "varsayılandan gelen: satırı null K71 uyarısı");
    return d.hata;
}

// #7 K52: otopilot parametreleri çekirdeğin parametre kataloğuyla.
int test_sol_7_parametre_katalogu(const TestBaglami& tb)
{
    Denetci d;
    auto kos = [&](const std::string& parametre) {
        BellekKaynagi k = temel_kaynak(tb);
        k.dosyalar[kSenaryo] += "\n[otopilot]\nparametreler = [" + parametre + "]\n";
        return senaryo_coz(k, kSenaryo);
    };
    const CozumSonucu tur = kos("{kimlik = 1281, tip = \"u32\", deger = 1}");
    DOGRULA(d, !tur.rapor.gecerli && var(tur.rapor.bulgular, "A5-K52"), "T1_RX (F32) u32 olarak reddedilmeli");
    const CozumSonucu aralik = kos("{kimlik = 257, tip = \"u32\", deger = 31}");
    DOGRULA(d, !aralik.rapor.gecerli && var(aralik.rapor.bulgular, "A5-K52"), "T_imu_stale 5–30 dışı reddedilmeli");
    const CozumSonucu bilinmeyen = kos("{kimlik = 9, tip = \"u32\", deger = 1}");
    DOGRULA(d, !bilinmeyen.rapor.gecerli && var(bilinmeyen.rapor.bulgular, "A5-K52"), "katalogda olmayan kimlik reddedilmeli");
    const CozumSonucu iyi = kos("{kimlik = 257, tip = \"u32\", deger = 30}, {kimlik = 1281, tip = \"f32\", deger = -2.0}");
    DOGRULA(d, iyi.rapor.gecerli, "katalogla uyumlu çiftler geçerli\n" + bulgu_ozeti(iyi.rapor));
    const CozumSonucu f32 = kos("{kimlik = 1281, tip = \"f32\", deger = 2.0000003}");
    DOGRULA(d, !f32.rapor.gecerli && var(f32.rapor.bulgular, "A5-K52"), "F32 aralık dışı (binary32: 2,00000024 > 2) reddedilmeli");
    return d.hata;
}

// #8 K12: gerçek yol çözümü ve NUL.
int test_sol_8_yol_siniri(const TestBaglami& tb)
{
    Denetci d;
    {
        BellekKaynagi k = temel_kaynak(tb);
        k.dosyalar[kSenaryo] = *degistir(k.dosyalar[kSenaryo], "dosya = \"ida0.toml\"", "dosya = \"ida0.toml\\u0000.toml\"");
        const CozumSonucu s = senaryo_coz(k, kSenaryo);
        DOGRULA(d, !s.rapor.gecerli && var(s.rapor.bulgular, "A5-K12", "arac.dosya"), "NUL içeren yol reddedilmeli\n" + bulgu_ozeti(s.rapor));
    }
#if !defined(_WIN32)
    namespace fs = std::filesystem;
    const fs::path kok = fs::temp_directory_path() / ("ida_senaryo_k12_" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    fs::create_directories(kok / "scenarios");
    fs::create_directories(kok / "disari");
    fs::create_directories(kok / "assets" / "ornek");
    auto yaz = [](const fs::path& p, const std::string& m) {
        std::ofstream f(p, std::ios::binary);
        f << m;
    };
    yaz(kok / "disari" / "ida0.toml", dosya_oku(tb.kok + "/scenarios/ida0.toml"));
    yaz(kok / "scenarios" / "ornek-havuz.toml", dosya_oku(tb.kok + "/scenarios/ornek-havuz.toml"));
    yaz(kok / "assets" / "ornek" / "manifest.json", dosya_oku(tb.kok + "/assets/ornek/manifest.json"));
    std::error_code ec;
    fs::create_symlink(kok / "disari" / "ida0.toml", kok / "scenarios" / "ida0.toml", ec);
    DOGRULA(d, !ec, "simgesel bağ kurulamadı");
    const DiskKaynagi disk(kok);
    const CozumSonucu s = senaryo_coz(disk, "scenarios/ornek-havuz.toml");
    DOGRULA(d, !s.rapor.gecerli && var(s.rapor.bulgular, "A5-K12", "arac.dosya"),
            "senaryo dizini dışına çıkan simgesel bağ reddedilmeli\n" + bulgu_ozeti(s.rapor));
    // Dizin içinde kalan bağ kabul edilir.
    fs::remove(kok / "scenarios" / "ida0.toml");
    fs::create_directories(kok / "scenarios" / "araclar");
    yaz(kok / "scenarios" / "araclar" / "ida0.toml", dosya_oku(tb.kok + "/scenarios/ida0.toml"));
    fs::create_symlink(kok / "scenarios" / "araclar" / "ida0.toml", kok / "scenarios" / "ida0.toml", ec);
    const CozumSonucu s2 = senaryo_coz(disk, "scenarios/ornek-havuz.toml");
    DOGRULA(d, s2.rapor.gecerli, "senaryo dizini içindeki bağ kabul edilmeli\n" + bulgu_ozeti(s2.rapor));
    fs::remove_all(kok, ec);
#endif
    return d.hata;
}

// #9 Routh hesabı ölçekten bağımsız.
int test_sol_9_routh_olcegi(const TestBaglami& tb)
{
    Denetci d;
    for (const char* olcek : {"1.0", "1e200", "1e-200"}) {
        const std::string c = olcek;
        BellekKaynagi k = temel_kaynak(tb);
        k.dosyalar[kArac] = *degistir(k.dosyalar[kArac], "transfer_pay = [6136.0, 108700.0]", "transfer_pay = [" + c + "]");
        k.dosyalar[kArac] = *degistir(k.dosyalar[kArac], "transfer_payda = [1.0, 89.0, 9258.0, 108700.0]",
                                      "transfer_payda = [" + c + ", 2.0 * " + c + ", " + c + "]");
        // TOML'da çarpım yok: değerler önceden hesaplanır.
        const std::string iki = c == "1.0" ? "2.0" : (c == "1e200" ? "2e200" : "2e-200");
        k.dosyalar[kArac] = *degistir(k.dosyalar[kArac], "2.0 * " + c, iki);
        const CozumSonucu s = senaryo_coz(k, kSenaryo);
        DOGRULA(d, !var(s.rapor.bulgular, "A5-K105"), "ölçek " + c + ": kararlı sistem K105 almamalı\n" + bulgu_ozeti(s.rapor));
    }
    return d.hata;
}

// #10 K91: aynı aşamanın bağımsız bulguları toplanır.
int test_sol_10_bulgu_toplama(const TestBaglami& tb)
{
    Denetci d;
    BellekKaynagi k = temel_kaynak(tb);
    k.dosyalar[kArac] = *degistir(k.dosyalar[kArac], "asgari_guvenli_gerilim = 12.0", "asgari_guvenli_gerilim = 12.0\nsigorta_i2t = 100.0");
    k.dosyalar[kArac] = *degistir(k.dosyalar[kArac], "pwm_min_us = 1100", "pwm_min_us = 1475");
    k.dosyalar[kArac] = *degistir(k.dosyalar[kArac], "r_g = [0.0, 0.0, 0.0]", "r_g = [0.0, 0.0, 0.5]");
    k.dosyalar[kSenaryo] += "\n[arac.sensorler.baro0.ayar]\nbilinmeyen = 1\n";
    const CozumSonucu s = senaryo_coz(k, kSenaryo);
    DOGRULA(d, var(s.rapor.bulgular, "A5-K102", "arac.batarya.sigorta_i2t") && var(s.rapor.bulgular, "A5-K31", "arac.esc") &&
                   var(s.rapor.bulgular, "A5-K22", "arac.kutle") && var(s.rapor.bulgular, "A5-K06", "arac.sensorler.baro0.ayar.bilinmeyen"),
            "bağımsız aşama 2 bulgularının hepsi raporlanmalı\n" + bulgu_ozeti(s.rapor));
    return d.hata;
}

// #11 sayı kurtarma tek geçişte.
int test_sol_11_tek_gecis(const TestBaglami& tb)
{
    Denetci d;
    BellekKaynagi k = temel_kaynak(tb);
    std::string dizi = "x_alt_normaller = [";
    constexpr int kSayi = 20000;
    for (int i = 0; i < kSayi; ++i) {
        dizi += i == 0 ? "4.9e-324" : ", 4.9e-324";
    }
    dizi += "]\n";
    k.dosyalar[kSenaryo] = *degistir(k.dosyalar[kSenaryo], "ana_tohum = 1", "ana_tohum = 1\n" + dizi);
    const auto t0 = std::chrono::steady_clock::now();
    const CozumSonucu s = senaryo_coz(k, kSenaryo);
    const double sn = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    std::printf("  %d alt normal sayı içeren senaryonun çözümlenmesi: %.3f s (ölçüm, bu makine)\n", kSayi, sn);
    DOGRULA(d, s.rapor.gecerli, "alt normal değerler kabul edilmeli\n" + bulgu_ozeti(s.rapor));
    const Deger* x = s.cozumlenmis ? s.cozumlenmis->agac().yol_bul("rastgelelik.x_alt_normaller") : nullptr;
    DOGRULA(d, x != nullptr && x->dizi.size() == kSayi && x->dizi.back().sayi == 4.9e-324, "değerler doğru çevrilmeli");
    DOGRULA(d, sn < 30.0, "süre doğrusal olmalı (30 s güvenlik sınırı)");
    return d.hata;
}

// #12 araç dosyası bağımsız doğrulanır; senaryo çözümleme yine senaryo ister.
int test_sol_12_arac_dosyasi(const TestBaglami& tb)
{
    Denetci d;
    const BellekKaynagi k = temel_kaynak(tb);
    const CozumSonucu a = dosya_dogrula(k, kArac);
    DOGRULA(d, a.rapor.gecerli && !a.rapor.davranis_ozeti && !a.cozumlenmis, "geçerli araç dosyası geçerli raporlanmalı\n" + bulgu_ozeti(a.rapor));
    DOGRULA(d, a.rapor.kaynak_ozetleri.size() == 1, "yalnız araç dosyası okunmalı (ortam uydurulmaz)");
    BellekKaynagi bozuk = k;
    bozuk.dosyalar[kArac] = *degistir(bozuk.dosyalar[kArac], "pwm_min_us = 1100", "pwm_min_us = 1475");
    const CozumSonucu b = dosya_dogrula(bozuk, kArac);
    DOGRULA(d, !b.rapor.gecerli && var(b.rapor.bulgular, "A5-K31", "arac.esc"), "araç kuralı araç dosyasında da denetlenmeli");
    const CozumSonucu s = senaryo_coz(k, kArac);
    DOGRULA(d, !s.rapor.gecerli && var(s.rapor.bulgular, "A5-K02"), "senaryo_coz senaryo ister");
    const CozumSonucu o = dosya_dogrula(k, kSenaryo);
    DOGRULA(d, o.rapor.gecerli && o.rapor.davranis_ozeti.has_value(), "dosya_dogrula senaryoyu çözümler");
    return d.hata;
}

}  // namespace ida::senaryo::test
