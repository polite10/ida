// test_soket.cpp — gerçek soketlerle: taşıma (A2-K01, K03, K05, K93), olağan kapanış (A2-K67) ve
// sahiplik kilidi (A2-K77, K79; kabul 8).
#include "kilit.hpp"
#include "test_destek.hpp"

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>

namespace ida::a2::test {

// Uçtan uca, gerçek TCP üzerinden: el sıkışma, akış, olağan veda, yeniden bağlanma.
int test_soket_uctan_uca()
{
    Denetci d;
    SunucuDuzenegi sd;
    DOGRULA(d, sd.s != nullptr && sd.s->durum() == A2Durumu::dinliyor && sd.s->port() != 0, "sunucu dinliyor: " + sd.hata);
    if (sd.s == nullptr) {
        return d.hata + 1;
    }
    sahte::SahteAyar a = ornek_ayar();
    a.hedef_sayisi = 2;
    a.firlatma_adimi = 200;
    sahte::SahteDunya dn(sd.s->yayinci(), a);
    dn.basla();
    std::uint64_t k1 = 0;
    {
        SoketIstemci c(sd.s->port());
        DOGRULA(d, c.t != ag::gecersiz(), "bağlandı");
        c.basla();
        DOGRULA(d, c.bekle([&] { return c.ist.asama() == viz::Asama::akis; }), "istemci akışta");
        k1 = c.ist.oturum_kimligi();
        for (int i = 0; i < 40; ++i) {
            dn.adimla(10);
            c.bekle([] { return false; }, 2);
        }
        DOGRULA(d, c.bekle([&] { return c.ist.sahne().bul(dn.torpido_kimligi()) != nullptr; }), "torpido istemci sahnesinde");
        DOGRULA(d, c.bekle([&] { return c.k.say(IletiTuru::durum) >= 30 && c.k.say(IletiTuru::sahne_olayi) == 2; }),
                "durum ve olaylar soketten geldi");
        DOGRULA(d, !c.k.cerceve_hatasi && c.ist.sayaclar().sira_boslugu == 0, "akış hatasız ve boşluksuz");
        const SunucuSayaclari s = sd.s->sayaclar();
        DOGRULA(d, s.oturum_var && s.hazir_alindi && s.oturum_kimligi == k1 && s.baglanti_sayisi == 1, "sunucu sayaçları");
        // Olağan kapanış (A2-K67): istemci veda(olagan) gönderir.
        c.ist.olagan_kapat();
        c.pompala();
        DOGRULA(d, kosul_bekle([&] { return !sd.s->sayaclar().oturum_var; }), "sunucu oturumu kapattı");
        DOGRULA(d, sd.s->sayaclar().son_kopma_nedeni == KopmaNedeni::karsi_veda, "kopma nedeni: karşı veda");
    }
    dn.adimla(100);  // oturum yokken dünya ilerler (A2-K68)
    {
        SoketIstemci c(sd.s->port());
        c.basla();
        DOGRULA(d, c.bekle([&] { return c.ist.asama() == viz::Asama::akis; }), "yeniden bağlandı");
        DOGRULA(d, c.ist.oturum_kimligi() != k1 && c.ist.oturum_kimligi() != 0, "yeni oturum kimliği");
        DOGRULA(d, c.ist.hosgeldin().h.adim_no == 500 && c.ist.hosgeldin().govdeler.size() == 4, "tam durum: güncel adım, torpido dahil");
        // Sunucu durdurulur: veda(sim_kapaniyor) (A2-K67).
        sd.s->durdur();
        DOGRULA(d, c.bekle([&] { return c.karsi_kapatti; }), "sunucu bağlantıyı kapattı");
        DOGRULA(d, c.k.son_veda(VedaNedeni::sim_kapaniyor), "son ileti veda(sim_kapaniyor)");
        DOGRULA(d, sd.s->durum() == A2Durumu::durduruldu, "durum: durduruldu");
        DOGRULA(d, sd.s->sayaclar().son_kopma_nedeni == KopmaNedeni::sim_kapaniyor, "kopma nedeni");
    }
    // Durdurulmuş sunucuda yayın işlevleri çalışmayı sürdürür (dünya beklemez).
    dn.adimla(5000);
    DOGRULA(d, sd.s->yayinci().sayaclar().gecersiz_cagri == 0, "durdurulduktan sonra yayın çağrıları sorunsuz");
    return d.hata;
}

// Yayinci::kapaniyor(): bağlı oturum veda(sim_kapaniyor) alır, dinleyici kapanır.
int test_soket_kapaniyor()
{
    Denetci d;
    SunucuDuzenegi sd;
    if (sd.s == nullptr) {
        return 1;
    }
    sahte::SahteDunya dn(sd.s->yayinci(), ornek_ayar());
    dn.basla();
    const std::uint16_t port = sd.s->port();
    SoketIstemci c(port);
    c.basla();
    DOGRULA(d, c.bekle([&] { return c.ist.asama() == viz::Asama::akis; }), "istemci akışta");
    DOGRULA(d, sd.s->yayinci().kapaniyor(), "kapaniyor()");
    DOGRULA(d, c.bekle([&] { return c.karsi_kapatti; }), "bağlantı kapandı");
    DOGRULA(d, c.k.son_veda(VedaNedeni::sim_kapaniyor), "veda(sim_kapaniyor)");
    DOGRULA(d, kosul_bekle([&] { return sd.s->durum() == A2Durumu::durduruldu; }), "dinleyici kapandı");
    // Yeni bağlantı kurulamaz ya da hemen kapanır; oturum açılmaz.
    SoketIstemci c2(port);
    if (c2.t != ag::gecersiz()) {
        c2.basla();
        c2.bekle([&] { return c2.karsi_kapatti; }, 300);
        DOGRULA(d, c2.k.say(IletiTuru::hosgeldin) == 0, "kapanıştan sonra hosgeldin gönderilmez");
    }
    DOGRULA(d, sd.s->sayaclar().baglanti_sayisi == 1, "yeni oturum açılmadı");
    return d.hata;
}

// A2-K03 (A2-U04): ikinci bağlantıya ret(mesgul); ilk oturum etkilenmez.
int test_ikinci_baglanti()
{
    Denetci d;
    SunucuDuzenegi sd;
    if (sd.s == nullptr) {
        return 1;
    }
    sahte::SahteDunya dn(sd.s->yayinci(), ornek_ayar());
    dn.basla();
    SoketIstemci c1(sd.s->port());
    c1.basla();
    DOGRULA(d, c1.bekle([&] { return c1.ist.asama() == viz::Asama::akis; }), "ilk istemci akışta");
    const std::uint64_t kimlik = c1.ist.oturum_kimligi();

    for (int i = 0; i < 3; ++i) {
        SoketIstemci c2(sd.s->port());
        DOGRULA(d, c2.t != ag::gecersiz(), "ikinci bağlantı TCP düzeyinde kuruldu");
        c2.basla();
        DOGRULA(d, c2.bekle([&] { return c2.karsi_kapatti; }), "ikinci bağlantı kapatıldı");
        DOGRULA(d, c2.k.cerceveler.size() == 1 && c2.k.son_ret(RetNedeni::mesgul) && c2.k.cerceveler[0].b.sira == 0,
                "ikinci bağlantı tek ileti aldı: ret(mesgul)");
        DOGRULA(d, c2.ist.asama() == viz::Asama::kapali && c2.ist.yeniden_baglanabilir(), "istemci reddi işledi (A2-K63 ile yeniden dener)");
        // İlk oturum bu sırada akmaya devam eder.
        const std::size_t once = c1.k.say(IletiTuru::durum);
        dn.adimla(80);
        DOGRULA(d, c1.bekle([&] { return c1.k.say(IletiTuru::durum) >= once + 5; }), "ilk oturum akıyor");
    }
    const SunucuSayaclari s = sd.s->sayaclar();
    DOGRULA(d, s.mesgul_reddi == 3 && s.baglanti_sayisi == 1 && s.oturum_var && s.oturum_kimligi == kimlik && s.biten_oturum == 0,
            "sayaçlar: üç ret(mesgul), tek oturum");
    DOGRULA(d, c1.ist.asama() == viz::Asama::akis && !c1.k.cerceve_hatasi && c1.ist.sayaclar().sira_boslugu == 0,
            "ilk oturum etkilenmedi");
    return d.hata;
}

// A2-K05 (A2-U04), A2-K93: port doluyken sunucu A2'siz durumda; yayın işlevleri çalışır.
int test_port_dolu()
{
    Denetci d;
    // Portu başka bir dinleyici tutar.
    ag::Tutamac engel = ag::gecersiz();
    std::uint16_t port = 0;
    std::string hata;
    DOGRULA(d, ag::dinle(0, engel, port, hata) && port >= 1024, "engel dinleyici açıldı: " + hata);
    const std::string dizin = gecici_dizin("port");
    {
        SunucuAyari a;
        a.port = port;
        a.kilit_dosyasi = dizin + "/a2.kilit";
        std::unique_ptr<Sunucu> s = Sunucu::baslat(a, hata);
        DOGRULA(d, s != nullptr, "sunucu nesnesi döndü (A2'siz): " + hata);
        if (s != nullptr) {
            DOGRULA(d, s->durum() == A2Durumu::port_acilamadi && s->port() == 0, "durum: port açılamadı");
            DOGRULA(d, !s->durum_aciklamasi().empty(), "neden sorgulanabilir: " + s->durum_aciklamasi());
            // Yayın işlevleri çalışmayı sürdürür: hemen döner, hiçbir şey beklemez.
            sahte::SahteDunya dn(s->yayinci(), ornek_ayar());
            DOGRULA(d, !dn.basla(), "A2'sizken yayın çağrısı false döner");
            dn.adimla(100'000);
            const SunucuSayaclari sy = s->sayaclar();
            DOGRULA(d, sy.baglanti_sayisi == 0 && sy.yayin.yazilan_kayit == 0, "A2'sizken hiçbir şey yazılmadı");
            s->durdur();
        }
    }
    {  // Aynı port boşalınca sunucu dinler.
        ag::kapat(engel);
        SunucuAyari a;
        a.port = port;
        a.kilit_dosyasi = dizin + "/a2.kilit";
        std::unique_ptr<Sunucu> s = Sunucu::baslat(a, hata);
        DOGRULA(d, s != nullptr && s->durum() == A2Durumu::dinliyor && s->port() == port, "port boşalınca dinler");
    }
    {  // A2-K93: aralık dışı port → sunucu başlamaz ve nedenini bildirir.
        for (std::uint16_t p : {std::uint16_t{0}, std::uint16_t{80}, std::uint16_t{1023}}) {
            SunucuAyari a;
            a.port = p;
            a.kilit_dosyasi = dizin + "/a2.kilit";
            std::unique_ptr<Sunucu> s = Sunucu::baslat(a, hata);
            DOGRULA(d, s == nullptr && !hata.empty(), "port " + std::to_string(p) + " reddedildi: " + hata);
        }
    }
    std::error_code ec;
    std::filesystem::remove_all(dizin, ec);
    return d.hata;
}

// Yalnız 127.0.0.1 (A2-K01): dinleyici döngü arabirimine bağlıdır.
int test_yalniz_dongu_arabirimi()
{
    Denetci d;
    ag::Tutamac t = ag::gecersiz();
    std::uint16_t port = 0;
    std::string hata;
    DOGRULA(d, ag::dinle(0, t, port, hata), "dinleyici açıldı");
    // Aynı portta ikinci dinleyici açılamaz (A2-K05'in koşulu).
    ag::Tutamac t2 = ag::gecersiz();
    std::uint16_t p2 = 0;
    DOGRULA(d, !ag::dinle(port, t2, p2, hata) && t2 == ag::gecersiz(), "aynı portta ikinci dinleyici açılamaz");
    const ag::Tutamac c = ag::baglan(port);
    DOGRULA(d, c != ag::gecersiz(), "127.0.0.1'den bağlanılır");
    ag::kapat(c);
    ag::kapat(t);
    return d.hata;
}

// ---- Sahiplik kilidi (A2-K77, K79) ----

int test_kilit()
{
    Denetci d;
    const std::string dizin = gecici_dizin("kilit");
    const std::string yol = dizin + "/ida/a2.kilit";  // alt dizin yoksa oluşturulur
    {
        ic::SahiplikKilidi a;
        ic::SahiplikKilidi b;
        std::string hata;
        DOGRULA(d, a.al(yol, hata) == ic::KilitSonucu::alindi && a.tutuluyor(), "ilk kilit alındı: " + hata);
        DOGRULA(d, b.al(yol, hata) == ic::KilitSonucu::baskasinda && !b.tutuluyor(), "ikinci kilit beklemeden reddedildi");
        a.birak();
        DOGRULA(d, b.al(yol, hata) == ic::KilitSonucu::alindi, "bırakılınca alınır");
    }
    {  // İki sunucu aynı kilit dosyasıyla (aynı süreçte): yalnız biri dinler.
        SunucuAyari a;
        a.sinama_bos_port = true;
        a.kilit_dosyasi = yol;
        std::string hata;
        std::unique_ptr<Sunucu> s1 = Sunucu::baslat(a, hata);
        std::unique_ptr<Sunucu> s2 = Sunucu::baslat(a, hata);
        DOGRULA(d, s1 != nullptr && s1->durum() == A2Durumu::dinliyor, "ilk sunucu dinliyor");
        DOGRULA(d, s2 != nullptr && s2->durum() == A2Durumu::kilit_baskasinda && s2->port() == 0,
                "ikinci sunucu A2'siz: kilit başkasında (A2-K79)");
        if (s2 != nullptr) {
            DOGRULA(d, s2->durum_aciklamasi().find("kilidi") != std::string::npos, "neden sorgulanabilir: " + s2->durum_aciklamasi());
            sahte::SahteDunya dn(s2->yayinci(), ornek_ayar());
            dn.basla();
            dn.adimla(10'000);  // yayın işlevleri çalışmayı sürdürür
        }
        // Kilit porttan bağımsızdır: durdurulan sunucu kilidi nesne yaşadıkça tutar (A2-K77).
        if (s1 != nullptr) {
            s1->durdur();
            std::unique_ptr<Sunucu> s3 = Sunucu::baslat(a, hata);
            DOGRULA(d, s3 != nullptr && s3->durum() == A2Durumu::kilit_baskasinda, "durdurulmuş sahip kilidi tutmaya devam eder");
        }
        s1.reset();
        std::unique_ptr<Sunucu> s4 = Sunucu::baslat(a, hata);
        DOGRULA(d, s4 != nullptr && s4->durum() == A2Durumu::dinliyor, "sahip yok olunca yenisi alır");
    }
    {  // Varsayılan yol sözleşmedeki addadır (dosyaya dokunulmaz).
        const std::filesystem::path v = ic::SahiplikKilidi::varsayilan_yol();
        DOGRULA(d, v.filename() == "a2.kilit" && v.parent_path().filename() == "ida", "varsayılan yol …/ida/a2.kilit");
    }
    std::error_code ec;
    std::filesystem::remove_all(dizin, ec);
    return d.hata;
}

namespace {

#if defined(_WIN32)
FILE* boru_ac(const std::string& komut) { return _popen(komut.c_str(), "r"); }
int boru_kapat(FILE* f) { return _pclose(f); }
#else
FILE* boru_ac(const std::string& komut) { return popen(komut.c_str(), "r"); }
int boru_kapat(FILE* f) { return pclose(f); }
#endif

// Alt süreç: ida_a2_sahte; ilk satırı "A2_DURUM <ad> port=<n>".
struct AltSurec {
    FILE* f = nullptr;
    std::string durum;
    bool baslat(const std::string& program, const std::string& secenekler)
    {
        std::string komut = "\"" + program + "\" " + secenekler;
#if defined(_WIN32)
        komut = "\"" + komut + "\"";  // cmd /c: bütün komut bir çift tırnak daha ister
#endif
        f = boru_ac(komut);
        if (f == nullptr) {
            return false;
        }
        char satir[256] = {};
        if (std::fgets(satir, sizeof satir, f) == nullptr) {
            return false;
        }
        // "A2_DURUM <ad> port=<n>": ikinci sözcük durum adıdır.
        const std::string s(satir);
        const std::string on_ek = "A2_DURUM ";
        if (s.compare(0, on_ek.size(), on_ek) != 0) {
            return false;
        }
        const std::size_t son = s.find_first_of(" \r\n", on_ek.size());
        durum = s.substr(on_ek.size(), son == std::string::npos ? std::string::npos : son - on_ek.size());
        return !durum.empty();
    }
    void bitir()
    {
        if (f != nullptr) {
            char atik[256];
            while (std::fgets(atik, sizeof atik, f) != nullptr) {
            }
            boru_kapat(f);
            f = nullptr;
        }
    }
};

}  // namespace

// Kabul 8 (A2-U21'in halkasız kısmı): ayrı süreçler. Sahibi yaşarken ikinci süreç (ve bu süreç)
// kilidi alamaz ve A2'siz çalışır; sahibi kilidi BIRAKMADAN sonlanınca (_Exit) yenisi alır.
int test_kilit_surecler(const char* sahte_program)
{
    Denetci d;
    if (sahte_program == nullptr) {
        std::printf("  ida_a2_sahte yolu verilmedi\n");
        return 1;
    }
    const std::string dizin = gecici_dizin("surec");
    const std::string yol = dizin + "/a2.kilit";
    const std::string dur = dizin + "/dur";
    const std::string ortak = "--bos-port --kilit \"" + yol + "\" --firlat 0 ";

    AltSurec sahip;
    DOGRULA(d, sahip.baslat(sahte_program, ortak + "--sure 60 --dur-dosyasi \"" + dur + "\""), "sahip süreç başladı");
    DOGRULA(d, sahip.durum == "dinliyor", "sahip süreç kilidi aldı ve dinliyor: " + sahip.durum);

    // İkinci süreç: kilidi alamaz, A2'siz çalışır ve olağan biçimde biter.
    AltSurec ikinci;
    DOGRULA(d, ikinci.baslat(sahte_program, ortak + "--sure 0.2"), "ikinci süreç başladı");
    DOGRULA(d, ikinci.durum == "kilit_baskasinda", "ikinci süreç A2'siz: " + ikinci.durum);
    ikinci.bitir();

    // Bu süreç de alamaz.
    SunucuAyari a;
    a.sinama_bos_port = true;
    a.kilit_dosyasi = yol;
    std::string hata;
    {
        std::unique_ptr<Sunucu> s = Sunucu::baslat(a, hata);
        DOGRULA(d, s != nullptr && s->durum() == A2Durumu::kilit_baskasinda, "bu süreç kilidi alamadı (sahip yaşıyor)");
    }

    // Sahip, kilidi bırakmadan sonlanır.
    {
        std::ofstream f(dur);
        f << "dur\n";
    }
    sahip.bitir();
    // İşletim sistemi kilidi süreç sonlanınca bırakır (Windows'ta "bırakma süresi sistem
    // kaynaklarına bağlıdır": kısa süre yeniden denenir).
    std::unique_ptr<Sunucu> yeni;
    const bool alindi = kosul_bekle(
        [&] {
            yeni = Sunucu::baslat(a, hata);
            return yeni != nullptr && yeni->durum() == A2Durumu::dinliyor;
        },
        5000);
    DOGRULA(d, alindi, "sahip sonlanınca yeni süreç kilidi aldı ve dinliyor");
    yeni.reset();
    std::error_code ec;
    std::filesystem::remove_all(dizin, ec);
    return d.hata;
}

}  // namespace ida::a2::test
