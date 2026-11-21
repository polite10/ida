// Dunya arayüzü: komut zamanlaması, akıntı, kurulum hataları, dalış ve yüzeye çıkış.
#include "test_destek.hpp"

#include "itici.hpp"

namespace ida::fizik::test {

namespace {

std::array<double, kIticiSayisi> notr_pwm()
{
    std::array<double, kIticiSayisi> p{};
    p.fill(1500.0);
    return p;
}

}  // namespace

// Komut, verildiği adım numarasının başında yürürlüğe girer; geçmiş adıma komut reddedilir;
// aynı adıma ikinci komut öncekinin yerine geçer; ESC başlatma süresince itki isteği sıfırdır.
int test_komut_zamanlama(const TestBaglami&)
{
    Denetci d;
    TestAyari ta;
    const AracParametreleri a = test_araci(ta);
    ic::IticiModeliIc m;
    m.kur(a.itici_modeli, a.esc);
    Dunya dn;
    DOGRULA(d, dn.adim() == AdimDurumu::kurulmadi, "kurulmamış dünya adım attı");
    DOGRULA(d, !dn.itici_komutu_ver(0, notr_pwm()), "kurulmamış dünya komut aldı");
    if (!kur(dn, a, havuz_ortami(), baslangic(5.0, {}))) return 1;

    auto p = notr_pwm();
    p[4] = 1650.0;
    auto p2 = notr_pwm();
    p2[4] = 1700.0;
    DOGRULA(d, dn.itici_komutu_ver(5, p), "adım 5 komutu reddedildi");
    DOGRULA(d, dn.itici_komutu_ver(5, p2), "adım 5 ikinci komutu reddedildi");
    auto nan = notr_pwm();
    nan[0] = std::nan("");
    DOGRULA(d, !dn.itici_komutu_ver(6, nan), "sonlu olmayan komut kabul edildi");
    for (int i = 0; i < 5; ++i) {
        dn.adim();
        DOGRULA(d, dn.durum().pwm_us[4] == 1500.0 && dn.durum().itki_istegi[4] == 0.0,
                "komut adımından önce uygulandı");
    }
    DOGRULA(d, !dn.itici_komutu_ver(4, p), "geçmiş adıma komut kabul edildi");
    dn.adim();  // adım 5
    DOGRULA(d, dn.durum().adim_no == 6u, "adım numarası");
    DOGRULA(d, dn.durum().pwm_us[4] == 1700.0, "adım 5'in son komutu yürürlükte değil");
    YAKIN(d, dn.durum().itki_istegi[4], m.durgun_itki(1700.0, a.batarya.v_sabit), 0.0, 0.0, "F_ss");
    DOGRULA(d, dn.durum().itki[4] > 0.0, "itki başlamadı");

    // ESC başlatma (İDA-0: 2 s nötr bekleme; A5 §3.3). Komut nötr tutulursa 2 s'de hazır olur;
    // hazır olmadan itki isteği 0. (Sürekli nötr dışı komut ve kesinti durumları:
    // sim.fizik.esc_notr_disi_hazir_olmaz, sim.fizik.esc_notr_bekleme_sonrasi_hazir.)
    Dunya de;
    if (!kur(de, ida0_arac(), havuz_ortami(), baslangic(5.0, {}))) return 1;
    DOGRULA(d, !de.durum().esc_hazir[4], "kurulumda ESC hazır");
    de.itici_komutu_ver(adim_sayisi(2.0), p2);
    kos(de, adim_sayisi(2.0) - 1);
    DOGRULA(d, !de.durum().esc_hazir[4] && de.durum().itki_istegi[4] == 0.0 && de.durum().itki[4] == 0.0,
            "ESC başlatma süresinde hazır ya da itki var");
    kos(de, 2);
    DOGRULA(d, de.durum().esc_hazir[4] && de.durum().itki_istegi[4] > 0.0, "ESC başlatma sonrası itki yok");
    return d.hata;
}

// Düzgün akıntıda araç hızı akıntıya göre bağıl alınır: oturmuş durumda yere göre hız akıntı
// hızına eşittir ve araç dönmez.
int test_akinti(const TestBaglami&)
{
    Denetci d;
    TestAyari ta;
    ta.notr_yuzerlik = true;
    AracParametreleri a = test_araci(ta);
    // İDA-0'da Y_v, K_p, N_r doğrusal sönümü 0; yalnız karesel sönümle yakınsama cebirseldir
    // (v ~ 1/t). Üstel yakınsama ve sıkı tolerans için test değerleri.
    a.hidrodinamik.dogrusal_sonum = {13.7, 10.0, 33.0, 0.5, 0.8, 0.5};
    OrtamParametreleri o = havuz_ortami();
    o.akinti.hiz_ned = {0.2, -0.1, 0.02};
    Dunya dn;
    if (!kur(dn, a, o, baslangic(20.0, {}))) return 1;
    if (!kos(dn, adim_sayisi(120.0))) return 1;
    const auto& an = dn.durum();
    // Yere göre hız, NED: R v
    const auto& q = an.yonelim;
    const double w = q[0], x = q[1], y = q[2], z = q[3];
    const double R[3][3] = {{1 - 2 * (y * y + z * z), 2 * (x * y - w * z), 2 * (x * z + w * y)},
                            {2 * (x * y + w * z), 1 - 2 * (x * x + z * z), 2 * (y * z - w * x)},
                            {2 * (x * z - w * y), 2 * (y * z + w * x), 1 - 2 * (x * x + y * y)}};
    // Tolerans 1e-7: en yavaş kip (yalpa/baş-kıç geri dönüşü, ζ küçük) 120 s'de bu düzeyin
    // altına sönümlenir.
    for (int i = 0; i < 3; ++i) {
        double vn = 0.0;
        for (int j = 0; j < 3; ++j) vn += R[i][j] * an.dogrusal_hiz_b[static_cast<std::size_t>(j)];
        std::printf("  v_n[%d] = %.10f (akıntı %.3f)\n", i, vn, o.akinti.hiz_ned[static_cast<std::size_t>(i)]);
        YAKIN(d, vn, o.akinti.hiz_ned[static_cast<std::size_t>(i)], 0.0, 1e-7, "yere göre hız");
        YAKIN(d, an.acisal_hiz_b[static_cast<std::size_t>(i)], 0.0, 0.0, 1e-7, "açısal hız");
    }
    return d.hata;
}

// Geçersiz parametreler A5 kural numarasıyla reddedilir.
int test_kurulum_hatalari(const TestBaglami&)
{
    Denetci d;
    const OrtamParametreleri o0 = havuz_ortami();
    const BaslangicDurumu b0 = baslangic(1.0, {"f1", "f2"});
    struct Durum {
        const char* beklenen;
        AracParametreleri a;
        OrtamParametreleri o;
        BaslangicDurumu b;
        SayisalAyarlar s;
    };
    std::vector<Durum> durumlar;
    auto ekle = [&](const char* kural, auto degistir) {
        Durum s{kural, ida0_arac(), o0, b0, {}};
        degistir(s);
        durumlar.push_back(s);
    };
    ekle("A5-K21", [](Durum& s) { s.a.kutle.atalet[0][1] = 0.01; });
    ekle("A5-K21", [](Durum& s) { s.a.kutle.atalet[0][0] = -0.26; });
    ekle("A5-K21", [](Durum& s) { s.a.kutle.atalet[2][2] = 1.0; });  // üçgen eşitsizliği
    ekle("A5-K25", [](Durum& s) { s.a.iticiler[1].kanal = 0; });
    ekle("A5-K25", [](Durum& s) {
        for (auto& t : s.a.iticiler) t.yon = {0.0, 0.0, 1.0};
    });
    ekle("A5-K32", [](Durum& s) { s.a.iticiler[0].yon = {1.0, 1.0, 0.0}; });
    ekle("A5-K104", [](Durum& s) { s.a.itici_modeli.sekil_polinomu = {-1.0, 0, 0, 0, 0}; });
    ekle("A5-K105", [](Durum& s) { s.a.itici_modeli.transfer_payda = {1.0, -89.0, 9258.0, 108700.0}; });
    ekle("A5-K105", [](Durum& s) { s.a.itici_modeli.transfer_pay = {6136.0, 100000.0}; });
    ekle("A5-K105", [](Durum& s) {  // katsayılar pozitif ama a1 a2 < a3: Routh–Hurwitz
        s.a.itici_modeli.transfer_pay = {1.0, 10.0};
        s.a.itici_modeli.transfer_payda = {1.0, 1.0, 1.0, 10.0};
    });
    ekle("A5-K30", [](Durum& s) { s.a.itici_modeli.gerilim_noktalari = {12.0, 12.0, 20.0}; });
    ekle("A5-K31", [](Durum& s) { s.a.esc.pwm_min_us = 1480; });
    ekle("A5-K26", [](Durum& s) { s.a.hidrodinamik.sapma.karesel_sonum[0] = 1.5; });
    ekle("A5 §3.4", [](Durum& s) { s.a.batarya.v_sabit = 30.0; });
    ekle("A5-K86", [](Durum& s) { s.b.yuklu_tupler = {"f1", "f1"}; });
    ekle("A5-K86", [](Durum& s) { s.b.yuklu_tupler = {"f3"}; });
    ekle("A5-K32", [](Durum& s) { s.b.yonelim = {1.0, 0.1, 0.0, 0.0}; });
    ekle("dilim 1", [](Durum& s) { s.o.dalga.spektrum = DalgaSpektrumu::jonswap; });
    ekle("dilim 1", [](Durum& s) { s.o.ruzgar.hiz_ned = {3.0, 0.0, 0.0}; });
    ekle("dilim 1", [](Durum& s) { s.o.akinti.gauss_markov.etkin = true; });
    ekle("A5 §3.9", [](Durum& s) { s.o.su.yogunluk = 800.0; });
    ekle("SayisalAyarlar", [](Durum& s) { s.s.adim_ns = 0; });
    for (const auto& s : durumlar) {
        Dunya dn;
        const KurulumSonucu r = dn.kur(s.a, s.o, s.b, s.s);
        std::printf("  %-14s -> %s\n", s.beklenen, r.tamam ? "(kabul edildi)" : r.hata.c_str());
        DOGRULA(d, !r.tamam && r.hata.find(s.beklenen) != std::string::npos,
                std::string("beklenen ret: ") + s.beklenen);
        DOGRULA(d, !dn.kuruldu() && dn.adim() == AdimDurumu::kurulmadi, "reddedilen dünya kurulu");
    }
    // Geçerli kurulum ve 1,0e-6 içinde birim olmayan ama kabul edilen yön normalleştirilir.
    {
        AracParametreleri a = ida0_arac();
        a.iticiler[4].yon = {0.0, 0.0, 1.0 + 5e-7};
        Dunya dn;
        DOGRULA(d, dn.kur(a, o0, b0).tamam, "A5-K32 toleransındaki yön reddedildi");
    }
    return d.hata;
}

// Z1'in fizik yarısı (açık çevrim): yüzeyde yüzen İDA-0 dikey itkiyle 3 m'nin altına dalar,
// itki kesilince pozitif yüzerlikle yüzeye çıkar ve yüzer dengeye döner.
int test_dalis_ve_yuzeye_cikis(const TestBaglami&)
{
    Denetci d;
    const AracParametreleri a = ida0_arac();  // ESC başlatma 2 s dahil
    const OrtamParametreleri o = havuz_ortami();
    Dunya dn;
    if (!kur(dn, a, o, baslangic(0.0, {"f1", "f2"}))) return 1;
    const double z_eq = dn.turetilmis().denge_konumu_z;
    if (!kur(dn, a, o, baslangic(z_eq, {"f1", "f2"}))) return 1;
    auto p = notr_pwm();
    for (int i = 4; i < 8; ++i) p[static_cast<std::size_t>(i)] = 1600.0;
    // 2 s nötr bekleme (ESC başlatma), sonra dikey itki.
    dn.itici_komutu_ver(adim_sayisi(2.0), p);
    double en_derin = 0.0;
    for (std::uint64_t i = 0; i < adim_sayisi(22.0); ++i) {
        if (dn.adim() != AdimDurumu::tamam) return 1;
        en_derin = std::max(en_derin, dn.durum().konum_n[2]);
    }
    std::printf("  2 s nötr + 20 s dikey itki: derinlik %.3f m, s = %.4f\n", dn.durum().konum_n[2], dn.durum().batik_oran);
    DOGRULA(d, en_derin > 3.0, "3 m'nin altına inemedi");
    DOGRULA(d, dn.durum().batik_oran == 1.0, "dalışta tam batık değil");
    dn.itici_komutu_ver(dn.adim_no(), notr_pwm());
    double t_yuzey = -1.0;
    for (std::uint64_t i = 0; i < adim_sayisi(400.0); ++i) {
        if (dn.adim() != AdimDurumu::tamam) return 1;
        if (t_yuzey < 0.0 && dn.durum().batik_oran < 1.0) t_yuzey = static_cast<double>(dn.zaman_ns()) * 1e-9;
    }
    const auto& an = dn.durum();
    const Vek3 e = euler(an.yonelim);
    const double s_beklenen = dn.turetilmis().kutle.kutle / (o.su.yogunluk * dn.turetilmis().kutle.hacim);
    std::printf("  yüzeye ilk çıkış t = %.1f s; 420 s'de z = %.6f m (denge %.6f), s = %.6f (beklenen %.6f), "
                "roll %.2e pitch %.2e\n",
                t_yuzey, an.konum_n[2], z_eq, an.batik_oran, s_beklenen, e[0], e[1]);
    // Tolerans: oturmuş yüzer denge (kabul 1 ile aynı fizik); 1e-6 kalıntı geçici yanıt payı.
    DOGRULA(d, t_yuzey > 0.0, "yüzeye çıkmadı");
    YAKIN(d, an.konum_n[2], z_eq, 0.0, 1e-6, "yüzer denge derinliği");
    YAKIN(d, an.batik_oran, s_beklenen, 1e-6, 0.0, "batık oran");
    return d.hata;
}

}  // namespace ida::fizik::test
