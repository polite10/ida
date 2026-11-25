// test_yayinci.cpp — veri kaynağı arayüzünün kuralları: süreklilik numarası (A2-K21), geçersiz
// çağrılar, sabit boylu yapıların taşması, küme değişimleri, A2'siz yayıncı.
#include "takvim.hpp"
#include "test_destek.hpp"
#include "zamanlama_olcutu.hpp"

#include <atomic>
#include <chrono>
#include <cmath>
#include <cstring>
#include <limits>
#include <random>
#include <set>
#include <thread>

namespace ida::a2::test {

namespace {

const GovdeDurumu* bul(const viz::DurumIleti& d, std::uint32_t kimlik)
{
    for (const GovdeDurumu& g : d.govdeler) {
        if (g.govde_kimligi == kimlik) {
            return &g;
        }
    }
    return nullptr;
}

}  // namespace

int test_yayinci_sureklilik()
{
    Denetci d;
    Duzenek z;
    sahte::SahteDunya dn(z.y, ornek_ayar());
    dn.basla();
    DOGRULA(d, z.baglan(), "bağlandı");
    const std::uint32_t arac = dn.arac_kimligi();
    std::array<GovdeAni, kAzamiGovde> an{};
    const auto adim = [&](std::uint64_t n) {
        const std::size_t m = dn.dogru_anlar(n, an);
        return z.y.adim_siniri(n, std::span<const GovdeAni>(an.data(), m));
    };
    DOGRULA(d, adim(1), "adım 1");
    // Aynı adımda üç neden: kütle + darbe + yine kütle → numara tam 1 artar (A2-K21).
    DOGRULA(d, z.y.kutle_durumu(2, arac, 0x02) && z.y.sureklilik_kir(2, arac) && z.y.kutle_durumu(2, arac, 0x00),
            "aynı sınırda üç süreklilik nedeni");
    DOGRULA(d, adim(2), "adım 2");
    DOGRULA(d, z.y.sureklilik_kir(3, arac), "sonraki sınırda darbe");
    DOGRULA(d, adim(3), "adım 3");
    DOGRULA(d, adim(4), "adım 4 (olaysız)");
    z.pompala();
    std::vector<std::pair<std::uint64_t, std::uint32_t>> gorulen;  // (adım, aracın süreklilik numarası)
    std::vector<std::uint32_t> olay_surek;
    for (const Cerceve& c : z.k.cerceveler) {
        if (c.tur() == IletiTuru::durum) {
            const viz::DurumIleti m = durum_coz(c);
            const GovdeDurumu* g = bul(m, arac);
            gorulen.emplace_back(m.d.adim_no, g != nullptr ? g->sureklilik_no : 999u);
        } else if (c.tur() == IletiTuru::sahne_olayi) {
            olay_surek.push_back(olay_coz(c).sureklilik_no);
        }
    }
    // 0 (ilk), 1 (T − h), 2 (T), [2 yinelenmez], 3 (T); 4 olaysız ve takvimde değil: gitmez.
    const std::vector<std::pair<std::uint64_t, std::uint32_t>> beklenen = {{0, 0}, {1, 0}, {2, 1}, {3, 2}};
    DOGRULA(d, gorulen == beklenen, "durum dizisi: T − h eski numarayla, T tam 1 artmış; yineleme yok");
    DOGRULA(d, olay_surek.size() == 2 && olay_surek[0] == 1 && olay_surek[1] == 1,
            "iki kutle_durumu olayı da aynı (tam 1 artmış) numarayı taşır");
    const Cerceve* son = z.k.son(IletiTuru::durum);
    DOGRULA(d, son != nullptr && bul(durum_coz(*son), arac)->dolu_tupler == 0, "son tüp durumu");
    DOGRULA(d, z.istemci_hatasiz(), "istemci hatasız: " + z.istemci_son_nedeni());
    return d.hata;
}

int test_yayinci_gecersiz_cagri()
{
    Denetci d;
    Duzenek z;
    sahte::SahteDunya dn(z.y, ornek_ayar());
    const auto gecersiz = [&] { return z.y.sayaclar().gecersiz_cagri; };
    // Çalıştırma tanımı yokken her şey reddedilir.
    std::array<GovdeAni, 1> bir{};
    DOGRULA(d, !z.y.adim_siniri(1, bir) && !z.y.govde_dogdu(1, GovdeKaydi{9, 0, GovdeTuru::hedef, 0}) && !z.y.duraklat() &&
                   !z.y.sifirla(0) && !z.y.dalga_kumesi(dn.dalga()) && !z.y.calistirma_durdu(),
            "tanım yokken çağrılar reddedilir");
    DOGRULA(d, gecersiz() == 6, "altı geçersiz çağrı sayıldı");
    // Geçersiz tanımlar: hiçbiri uygulanmaz.
    {
        CalistirmaTanimi t;
        t.manifest_yolu = "";
        t.dalga = dn.dalga();
        t.ortam = dn.ortam();
        DOGRULA(d, !z.y.yeni_calistirma(t), "boş manifest yolu reddedilir");
        t.manifest_yolu = "/x";
        t.ortam.statik_q_nb[0] = 0.5;  // birim değil
        DOGRULA(d, !z.y.yeni_calistirma(t), "birim olmayan kuaterniyonlu ortam reddedilir");
        t.ortam = dn.ortam();
        t.ortam.gunes_yonu_n[0] = std::numeric_limits<double>::quiet_NaN();
        DOGRULA(d, !z.y.yeni_calistirma(t), "NaN güneş yönü reddedilir");
        t.ortam = dn.ortam();
        std::array<DalgaBileseni, 1> kotu = {DalgaBileseni{0.1, -1.0, 0.5, 0.0, 0.0}};  // omega ≤ 0
        t.dalga.bilesenler = kotu;
        DOGRULA(d, !z.y.yeni_calistirma(t), "omega ≤ 0 reddedilir");
        t.dalga = dn.dalga();
        t.kademe_3b = Kademe::kapali;
        DOGRULA(d, !z.y.yeni_calistirma(t), "3B kademesi kapalıyken tanım reddedilir (A2-K02)");
        t.kademe_3b = Kademe::tam;
        std::array<GovdeKaydi, 2> g = {GovdeKaydi{1, 0, GovdeTuru::arac, 3}, GovdeKaydi{1, 1, GovdeTuru::hedef, 0}};
        std::array<GovdeAni, 2> an{};
        an[0].kimlik = 1;
        an[1].kimlik = 1;
        t.govdeler = g;
        t.anlar = an;
        DOGRULA(d, !z.y.yeni_calistirma(t), "yinelenen gövde kimliği reddedilir");
        DOGRULA(d, z.y.calisma_nesli() == 1, "reddedilen tanım nesli değiştirmedi");
    }
    DOGRULA(d, z.baglan(), "bağlandı (tanım yok: bekler)");
    DOGRULA(d, z.c.asama() == ic::Asama::tam_durum_bekleniyor, "geçersiz tanımlardan sonra hâlâ tanım yok");
    DOGRULA(d, dn.basla(), "geçerli tanım");
    z.pompala();
    z.pompala();
    DOGRULA(d, z.c.asama() == ic::Asama::akis, "el sıkışma tamamlandı");

    const std::uint64_t g0 = gecersiz();
    dn.adimla(10);
    std::array<GovdeAni, kAzamiGovde> an{};
    std::size_t m = dn.dogru_anlar(5, an);
    DOGRULA(d, !z.y.adim_siniri(5, std::span<const GovdeAni>(an.data(), m)), "gerileyen adım reddedilir");
    DOGRULA(d, !z.y.adim_siniri(10, std::span<const GovdeAni>(an.data(), m)), "yinelenen adım reddedilir");
    DOGRULA(d, !z.y.govde_dogdu(10, GovdeKaydi{77, 1, GovdeTuru::hedef, 0}), "geçmiş sınırın olayı reddedilir");
    DOGRULA(d, !z.y.govde_dogdu(11, GovdeKaydi{dn.arac_kimligi(), 1, GovdeTuru::hedef, 0}), "yaşayan kimlikle doğum reddedilir");
    DOGRULA(d, !z.y.govde_dogdu(11, GovdeKaydi{78, 1, GovdeTuru::torpido, 0}, 3), "kaynak_tup > 2 reddedilir");
    DOGRULA(d, !z.y.govde_oldu(11, 4242, OlumNedeni::sure), "yaşamayan gövdenin ölümü reddedilir");
    DOGRULA(d, !z.y.govde_oldu(11, dn.arac_kimligi(), OlumNedeni::yok), "nedensiz ölüm reddedilir");
    DOGRULA(d, !z.y.kutle_durumu(11, dn.hedef_kimligi(0), 1), "araç olmayan gövdede kütle durumu reddedilir");
    DOGRULA(d, !z.y.sureklilik_kir(11, 4242), "yaşamayan gövdede süreklilik reddedilir");
    DOGRULA(d, !z.y.isabet(11, 1, 2, {std::numeric_limits<double>::infinity(), 0, 0}, {1, 0, 0}, {0, 0, 0}),
            "sonlu olmayan isabet reddedilir");
    DOGRULA(d, !z.y.surdur(), "duraklatılmamışken sürdür reddedilir");
    Ortam kotu = dn.ortam();
    kotu.ortam_turu = 9;
    DOGRULA(d, !z.y.ortam(kotu), "geçersiz ortam reddedilir");
    DOGRULA(d, gecersiz() == g0 + 12, "on iki geçersiz çağrı sayıldı: " + std::to_string(gecersiz() - g0));
    // Reddedilen çağrılar akışı bozmadı.
    dn.adimla(30);
    z.ilerlet(40'000'000);
    DOGRULA(d, z.istemci_hatasiz() && z.c.oturum_var() && !z.k.cerceve_hatasi, "akış sürüyor: " + z.istemci_son_nedeni());
    DOGRULA(d, z.k.say(IletiTuru::sahne_olayi) == 0, "reddedilen olaylar gönderilmedi");

    // Durumu verilmeyen gövde: geçersiz işaretli gider (A2-K27), sayılır.
    m = dn.dogru_anlar(41, an);
    DOGRULA(d, m == 2 && z.y.adim_siniri(53, std::span<const GovdeAni>(an.data(), 1)), "bir gövdenin durumu eksik");
    z.pompala();
    const Cerceve* son = z.k.son(IletiTuru::durum);
    DOGRULA(d, son != nullptr && durum_coz(*son).govdeler.size() == 2 && durum_coz(*son).govdeler[1].bayraklar == 0 &&
                   durum_coz(*son).govdeler[0].bayraklar == kGovdeGecerli,
            "eksik gövde listede, geçerli biti 0");
    DOGRULA(d, z.y.sayaclar().eksik_govde == 1 && z.ist.sayaclar().gecersiz_govde == 1, "eksik gövde iki tarafta sayıldı");
    DOGRULA(d, z.istemci_hatasiz(), "istemci hatasız");

    // 64 gövde sınırı: 65'inci doğum reddedilir.
    std::uint32_t kimlik = 20'000;
    int kabul = 0;
    for (int i = 0; i < 70; ++i) {
        if (z.y.govde_dogdu(54, GovdeKaydi{kimlik++, 1, GovdeTuru::hedef, 0})) {
            ++kabul;
        }
    }
    DOGRULA(d, kabul == 62, "tablo 64 gövdeyle sınırlı (2 + 62): " + std::to_string(kabul));
    return d.hata;
}

// Sabit boylu yapıların taşması: sayılır; dünya beklemez; oturum ya etkilenmez (takvimli durum)
// ya da veda(tampon_tasti) ile yeniden eşitlenir (atılamayan kayıt).
int test_yayinci_tasma()
{
    Denetci d;
    {  // Halka taşması, takvimli durum: yalnız durum atılır; olaylar ayrılmış payla geçer.
        Duzenek z(static_cast<std::size_t>(kGondermeKuyruguAzamiBayt), std::size_t{256} << 10u);  // 256 KiB halka
        sahte::SahteAyar a = ornek_ayar();
        a.hedef_sayisi = 30;
        sahte::SahteDunya dn(z.y, a);
        dn.basla();
        DOGRULA(d, z.baglan(), "bağlandı");
        // G/Ç tarafı tik atmadan 20 000 adım: ≈ 1500 takvimli durum × 3,7 KiB ≈ 5,6 MiB > 192 KiB.
        std::array<GovdeAni, kAzamiGovde> an{};
        std::uint32_t olay = 0;
        for (std::uint64_t n = 1; n <= 20'000; ++n) {
            if (n % 500 == 0) {
                ++olay;
                DOGRULA(d, z.y.isabet(n, 1, 2, {static_cast<double>(olay), 0, 0}, {1, 0, 0}, {0, 0, 0}), "olay kabul edildi");
            }
            const std::size_t m = dn.dogru_anlar(n, an);
            z.y.adim_siniri(n, std::span<const GovdeAni>(an.data(), m));
        }
        const YayinSayaclari y = z.y.sayaclar();
        DOGRULA(d, y.dusen_durum > 1000, "takvimli durumlar halkaya sığmadı ve sayıldı: " + std::to_string(y.dusen_durum));
        DOGRULA(d, y.kayip_korunan == 0, "olaylar ayrılmış payla yazıldı (kayıp yok)");
        z.ilerlet(10'000'000);
        DOGRULA(d, z.c.oturum_var() && z.istemci_hatasiz(), "oturum sürüyor: " + z.istemci_son_nedeni());
        std::uint32_t gelen = 0;
        bool sirali = true;
        for (const Cerceve& c : z.k.cerceveler) {
            if (c.tur() == IletiTuru::sahne_olayi) {
                ++gelen;
                sirali = sirali && olay_coz(c).nokta_n[0] == static_cast<double>(gelen);
            }
        }
        DOGRULA(d, gelen == olay && sirali && olay == 40, "bütün olaylar sırayla ulaştı");
        DOGRULA(d, z.ist.sayaclar().sira_boslugu == y.dusen_durum, "atılan her durum sıra boşluğu olarak göründü");
    }
    {  // Bir sınırda sabit olay tamponundan fazla olay: oturum veda(tampon_tasti) ile yeniden eşitlenir.
        Duzenek z;
        sahte::SahteDunya dn(z.y, ornek_ayar());
        dn.basla();
        DOGRULA(d, z.baglan(), "bağlandı");
        for (int i = 0; i < 250; ++i) {
            z.y.isabet(1, 1, 2, {0, 0, 0}, {1, 0, 0}, {0, 0, 0});
        }
        dn.adimla(1);
        DOGRULA(d, z.y.sayaclar().olay_tasmasi == 250 - ic::kBekleyenOlay, "taşan olaylar sayıldı");
        z.pompala();
        DOGRULA(d, z.k.son_veda(VedaNedeni::tampon_tasti), "veda(tampon_tasti)");
        const SunucuSayaclari s = z.c.sayaclar();
        DOGRULA(d, s.son_kopma_nedeni == KopmaNedeni::yayin_tasmasi && s.yayin_tasmasi_kapanis == 1, "kopma nedeni: yayın taşması");
        dn.adimla(5);
        DOGRULA(d, z.baglan() && z.ist.asama() == viz::Asama::akis, "yeniden bağlanan oturum tam durumu aldı");
        dn.adimla(40);
        z.ilerlet(50'000'000);
        DOGRULA(d, z.c.oturum_var() && z.istemci_hatasiz(), "yeni oturum etkilenmedi (eski kayıp yeni oturumu kapatmaz)");
    }
    {  // Halka tamamen dolu: atılamayan kayıt da kaybolur → oturum yeniden eşitlenir; dünya beklemez.
        Duzenek z(static_cast<std::size_t>(kGondermeKuyruguAzamiBayt), std::size_t{256} << 10u);
        sahte::SahteDunya dn(z.y, ornek_ayar());
        dn.basla();
        DOGRULA(d, z.baglan(), "bağlandı");
        for (std::uint64_t n = 1; n <= 4000; ++n) {
            z.y.isabet(n, 1, 2, {0, 0, 0}, {1, 0, 0}, {0, 0, 0});  // 136 B'lik korunan kayıtlar: 256 KiB dolar
            dn.adimla(1);
        }
        DOGRULA(d, z.y.sayaclar().kayip_korunan > 0, "korunan kayıt kaybı sayıldı");
        z.pompala();
        DOGRULA(d, z.k.son_veda(VedaNedeni::tampon_tasti) && z.c.sayaclar().son_kopma_nedeni == KopmaNedeni::yayin_tasmasi,
                "veda(tampon_tasti): yayın taşması");
        DOGRULA(d, z.baglan(), "yeniden bağlandı");
        DOGRULA(d, z.ist.hosgeldin().h.adim_no == 4000, "yeni oturum güncel kesitten kuruldu");
        dn.adimla(40);
        z.ilerlet(50'000'000);
        DOGRULA(d, z.c.oturum_var() && z.istemci_hatasiz(), "yeni oturum akıyor: " + z.istemci_son_nedeni());
    }
    // Oturum kurulur kurulmaz (el sıkışma iletileri henüz gönderilmeden) taşma: atılan iletilerin
    // sırası yeniden kullanılır; istemcinin gördüğü sıra 0'dan başlar ve boşluksuzdur (A2-K07).
    for (int kismi = 0; kismi < 2; ++kismi) {
        Duzenek z;
        sahte::SahteDunya dn(z.y, ornek_ayar());
        dn.basla();
        DOGRULA(d, z.ham_baglan(), "bağlandı");
        Merhaba m{};
        m.surum_ana = 1;
        m.platform = 1;
        m.cizici = 1;
        viz::Baytlar mb;
        viz::kodla_merhaba(mb, 0, m);
        z.c.veri_geldi(mb.data(), mb.size(), z.simdi);  // el sıkışma iletileri kuyrukta, gönderilmedi
        DOGRULA(d, z.c.asama() == ic::Asama::hazir_bekleniyor && z.c.kuyruk().bayt() > 600, "el sıkışma kuyrukta");
        if (kismi == 1) {
            const auto on = z.c.kuyruk().on();
            z.k.besle(on.data(), 10);   // hosgeldin'in ilk 10 baytı sokete yazıldı
            z.c.kuyruk().tuket(10);
        }
        for (int i = 0; i < 250; ++i) {
            z.y.isabet(1, 1, 2, {0, 0, 0}, {1, 0, 0}, {0, 0, 0});
        }
        dn.adimla(1);
        z.c.tik(z.simdi);
        DOGRULA(d, !z.c.oturum_var() && z.c.kapanis_var(), "oturum yayın taşmasıyla bitti");
        const ic::Baytlar kalan = z.c.kapanis_al();
        z.k.besle(kalan.data(), kalan.size());
        DOGRULA(d, !z.k.cerceve_hatasi && z.k.son_veda(VedaNedeni::tampon_tasti), "son ileti veda(tampon_tasti)");
        if (kismi == 0) {
            DOGRULA(d, z.k.cerceveler.size() == 1 && z.k.cerceveler[0].b.sira == 0,
                    "hiçbir ileti gönderilmemişti: veda oturumun ilk iletisi, sıra 0");
        } else {
            DOGRULA(d, z.k.cerceveler.size() == 2 && z.k.cerceveler[0].tur() == IletiTuru::hosgeldin &&
                           z.k.cerceveler[0].b.sira == 0 && z.k.cerceveler[1].b.sira == 1,
                    "gönderimi başlamış hosgeldin tamamlandı, veda hemen ardındaki sırayı aldı");
        }
        // Gerçek istemci bu akışı protokol hatası saymaz.
        viz::IstemciOturumu ist{viz::IstemciOturumu::Ayarlar{}};
        ist.baglandi(z.simdi, true);
        (void)ist.giden_al();
        for (const Cerceve& c : z.k.cerceveler) {
            viz::Baytlar b;
            viz::ileti_yaz(b, c.tur(), c.b.sira, c.yuk);
            ist.veri_geldi(b.data(), b.size(), z.simdi);
        }
        DOGRULA(d, ist.asama() == viz::Asama::kapali && ist.son_kapanis_nedeni().find("protokol") == std::string::npos &&
                       ist.yeniden_baglanabilir(),
                "istemci vedayı olağan işledi: " + ist.son_kapanis_nedeni());
    }
    return d.hata;
}

// Küme değişimleri (dalga, ortam, malzeme) koşu sırasında: sürümler artar, durumla tutarlı sırada gider.
int test_yayinci_kume_degisimi()
{
    Denetci d;
    Duzenek z;
    sahte::SahteDunya dn(z.y, ornek_ayar());
    dn.basla();
    DOGRULA(d, z.baglan(), "bağlandı");
    dn.adimla(20);
    z.ilerlet(25'000'000);
    const std::uint32_t dalga0 = z.ist.dalga().d.surum;
    const std::uint32_t ortam0 = z.ist.ortam().ortam_surumu;
    const std::uint32_t malzeme0 = z.ist.malzeme().m.surum;

    std::array<DalgaBileseni, 2> bl = {DalgaBileseni{0.3, 2.0, 0.4, 0.1, 0.2}, DalgaBileseni{0.1, 3.0, 0.9, 1.1, 0.0}};
    DalgaGirdisi dg = dn.dalga();
    dg.bilesenler = bl;
    dg.derin_su = false;
    dg.derinlik_m = 7.5;
    Ortam o = dn.ortam();
    o.gunes_isinimi_w_m2 = 321.0;
    std::array<MalzemeOptikGirdisi, 1> mg{};
    mg[0].malzeme_indeksi = 4;
    mg[0].maske = 0x2;
    mg[0].metaliklik = 0.9;
    mg[0].puruzluluk = 0.33;  // maskede değil: tel üzerinde 0
    DOGRULA(d, z.y.dalga_kumesi(dg) && z.y.ortam(o) && z.y.malzeme_optik(mg), "üç küme değişti");
    z.pompala();
    DOGRULA(d, z.ist.dalga().d.surum == dalga0, "çalışırken değişim bir sonraki adım sınırında yayınlanır");
    dn.adimla(30);  // 21…50
    z.ilerlet(40'000'000);
    DOGRULA(d, z.ist.dalga().d.surum == dalga0 + 1 && z.ist.dalga().bilesenler.size() == 2 && z.ist.dalga().d.derin_su == 0 &&
                   z.ist.dalga().d.derinlik_m == 7.5 && z.ist.dalga().bilesenler[1].omega == 3.0,
            "yeni dalga kümesi istemcide (sürüm +1)");
    DOGRULA(d, z.ist.ortam().ortam_surumu == ortam0 + 1 && z.ist.ortam().gunes_isinimi_w_m2 == 321.0, "yeni ortam (sürüm +1)");
    DOGRULA(d, z.ist.malzeme().m.surum == malzeme0 + 1 && z.ist.malzeme().girdiler.size() == 1 &&
                   z.ist.malzeme().girdiler[0].metaliklik == 0.9 && z.ist.malzeme().girdiler[0].puruzluluk == 0.0,
            "yeni malzeme optiği (maskede olmayan alan 0)");
    DOGRULA(d, !z.ist.tutarsiz() && z.ist.sayaclar().tutarsiz_durum == 0, "durumların dalga sürümü hep güncel kümeyle aynı (A2-K28)");
    DOGRULA(d, z.istemci_hatasiz(), "istemci hatasız: " + z.istemci_son_nedeni());
    int degisim = 0;
    for (const viz::Olay& e : z.olaylar) {
        if (e.tur == viz::OlayTuru::dalga_degisti || e.tur == viz::OlayTuru::ortam_degisti || e.tur == viz::OlayTuru::malzeme_degisti) {
            ++degisim;
        }
    }
    DOGRULA(d, degisim == 3, "istemci üç değişim olayı üretti");
    // Duraklatılmışken değişim hemen yayınlanır.
    DOGRULA(d, z.y.duraklat(), "duraklatıldı");
    o.gunes_isinimi_w_m2 = 111.0;
    DOGRULA(d, z.y.ortam(o), "duraklatılmışken ortam değişti");
    z.pompala();
    DOGRULA(d, z.ist.ortam().ortam_surumu == ortam0 + 2 && z.ist.ortam().gunes_isinimi_w_m2 == 111.0, "hemen ulaştı");
    // Yeni bağlanan oturum güncel kümeleri alır ve değişim kayıtlarını yeniden almaz.
    z.istemciyi_kopar();
    DOGRULA(d, z.baglan(), "yeniden bağlandı");
    DOGRULA(d, z.ist.dalga().d.surum == dalga0 + 1 && z.ist.ortam().ortam_surumu == ortam0 + 2 &&
                   z.ist.malzeme().girdiler.size() == 1,
            "yeni oturum güncel kümelerle kuruldu");
    DOGRULA(d, z.istemci_hatasiz(), "istemci hatasız");
    return d.hata;
}

// A2'siz yayıncı: bütün işlevler hemen döner, hiçbir şey yazılmaz ve sayılmaz.
int test_yayinci_a2siz()
{
    Denetci d;
    Yayinci y(4096, false);
    sahte::SahteDunya dn(y, ornek_ayar());
    DOGRULA(d, !dn.basla(), "A2'siz: yeni_calistirma false");
    dn.adimla(1000);
    DOGRULA(d, !y.duraklat() && !y.surdur() && !y.sifirla(0) && !y.kapaniyor() && !y.calistirma_durdu() &&
                   !y.govde_dogdu(1, GovdeKaydi{}) && !y.isabet(1, 1, 2, {0, 0, 0}, {1, 0, 0}, {0, 0, 0}),
            "A2'siz: bütün işlevler false");
    const YayinSayaclari s = y.sayaclar();
    DOGRULA(d, s.yazilan_kayit == 0 && s.gecersiz_cagri == 0 && s.dusen_durum == 0 && s.kayip_korunan == 0, "hiçbir şey sayılmadı");
    return d.hata;
}

}  // namespace ida::a2::test

// ---------------------------------------------------------------------------------------------
// Eşzamanlılık yapı taşları ve kesit–halka değişmezi
// ---------------------------------------------------------------------------------------------

namespace ida::a2::test {

namespace {

std::set<std::uint32_t> kume(const viz::DurumIleti& m)
{
    std::set<std::uint32_t> s;
    for (const GovdeDurumu& g : m.govdeler) {
        s.insert(g.govde_kimligi);
    }
    return s;
}

// Bir yayının (flush) içindeki her ara noktada G/Ç tarafını çalıştırır: nokta `hedef`'e gelince
// yeni bir oturum kurulur. Sonra akış sürer ve tutarlılık denetlenir.
struct AraNokta {
    Duzenek* z = nullptr;
    int sayac = 0;
    int hedef = -1;
    bool kuruldu = false;
    static void kanca(void* p)
    {
        auto* a = static_cast<AraNokta*>(p);
        if (a->sayac++ == a->hedef) {
            a->z->c.tik(a->z->simdi);     // oturum yok: bekleyen kayıtlar çekilip atılır
            a->kuruldu = a->z->baglan();  // kesit okunur, oturum kurulur
        }
    }
};

}  // namespace

// Üç yuvalı son değer deposu: yazar sürekli yazarken okur hiçbir zaman yarım (yırtık) değer görmez
// ve değer geriye gitmez.
int test_son_deger()
{
    Denetci d;
    struct Deger {
        std::array<std::uint64_t, 512> a{};
    };
    ic::SonDeger<Deger> depo;
    std::atomic<bool> dur{false};
    std::atomic<std::uint64_t> yazilan{0};
    std::thread yazar([&] {
        std::uint64_t k = 0;
        while (!dur.load(std::memory_order_relaxed)) {
            ++k;
            Deger& y = depo.yazilacak();
            for (std::uint64_t& x : y.a) {
                x = k;  // yuva dolarken okur tuttursa yırtık değer görürdü
            }
            depo.yayinla();
            yazilan.store(k, std::memory_order_relaxed);
        }
    });
    std::uint64_t onceki = 0;
    std::uint64_t okuma = 0;
    std::uint64_t farkli = 0;
    int hata = 0;
    const auto son = std::chrono::steady_clock::now() + std::chrono::milliseconds(700);
    // Kapsama hedefe göre (W5-K35): en az 700 ms ve yarış hedefi dolana kadar; asılma süresi cömert.
    const auto tavan = std::chrono::steady_clock::now() + std::chrono::seconds(60);
    const auto hedefte = [&] { return okuma > 10'000 && farkli > 100 && yazilan.load() > 1000; };
    while ((std::chrono::steady_clock::now() < son || !hedefte()) && std::chrono::steady_clock::now() < tavan) {
        for (int i = 0; i < 2000; ++i) {
            const Deger& g = depo.tuttur();
            const std::uint64_t ilk = g.a[0];
            for (std::uint64_t x : g.a) {
                if (x != ilk) {
                    ++hata;
                    break;
                }
            }
            if (ilk < onceki) {
                ++hata;
            }
            farkli += ilk != onceki ? 1u : 0u;
            onceki = ilk;
            depo.birak();
            ++okuma;
        }
    }
    dur.store(true);
    yazar.join();
    DOGRULA(d, hata == 0, "yırtık ya da gerileyen değer görülmedi; ihlal " + std::to_string(hata));
    DOGRULA(d, ida::test::zamanlama(hedefte(), "son değer yarışı kapsaması",
                                    std::to_string(okuma) + " okuma, " + std::to_string(farkli) + " farklı, " + std::to_string(yazilan.load()) + " yazım"),
            "yazar ve okur gerçekten yarıştı");
    std::printf("  ölçüm (bu Mac): %llu okuma, %llu farklı değer, %llu yazım\n", static_cast<unsigned long long>(okuma),
                static_cast<unsigned long long>(farkli), static_cast<unsigned long long>(yazilan.load()));
    return d.hata;
}

// Tek üretici–tek tüketici halka: değişken boylu kayıtlar sırayla, bozulmadan ve yalnız yer varken
// geçer; atılabilir kayıtlar ayrılmış paya dokunmaz.
int test_halka()
{
    Denetci d;
    {  // tek iş parçacığı: pay ve sarma
        ic::BaytHalkasi h(4096);
        DOGRULA(d, h.kapasite() == 4096, "kapasite");
        DOGRULA(d, h.ayir(3000, 0) == nullptr, "kapasitenin yarısından büyük kayıt reddedilir");
        int yazilan = 0;
        while (h.ayir(100, 1024) != nullptr) {  // 108 → 112 bayt; 1024 pay kalana kadar
            h.yayinla();
            ++yazilan;
        }
        DOGRULA(d, yazilan == (4096 - 1024) / 112, "atılabilir kayıtlar payı bırakır: " + std::to_string(yazilan));
        int paysiz = 0;
        while (h.ayir(100, 0) != nullptr) {
            h.yayinla();
            ++paysiz;
        }
        DOGRULA(d, paysiz == 9, "atılamayan kayıtlar payı kullanır: " + std::to_string(paysiz));
        std::size_t n = 0;
        int okunan = 0;
        while (h.oku(n) != nullptr) {
            DOGRULA(d, n == 100, "kayıt boyu");
            h.birak();
            ++okunan;
        }
        DOGRULA(d, okunan == yazilan + paysiz, "yazılan her kayıt okundu");
    }
    {  // iki iş parçacığı
        ic::BaytHalkasi h(std::size_t{64} << 10u);
        std::atomic<bool> bitti{false};
        const std::uint32_t kKayit = 400'000;
        std::atomic<std::uint64_t> dusen{0};
        std::thread uretici([&] {
            std::mt19937 rng(7);
            for (std::uint32_t i = 0; i < kKayit;) {
                const std::size_t boy = 8 + rng() % 600u;
                std::uint8_t* p = h.ayir(boy, 0);
                if (p == nullptr) {
                    dusen.fetch_add(1, std::memory_order_relaxed);
                    std::this_thread::yield();
                    continue;  // yer açılınca aynı kayıt yeniden denenir (sınama her kaydı geçirir)
                }
                std::memcpy(p, &i, 4);
                const auto b = static_cast<std::uint8_t>(i * 31u + boy);
                std::memset(p + 4, b, boy - 4);
                h.yayinla();
                ++i;
            }
            bitti.store(true);
        });
        std::uint32_t beklenen = 0;
        int hata = 0;
        for (;;) {
            std::size_t n = 0;
            const std::uint8_t* p = h.oku(n);
            if (p == nullptr) {
                if (bitti.load() && h.oku(n) == nullptr) {
                    break;
                }
                continue;
            }
            std::uint32_t i = 0;
            std::memcpy(&i, p, 4);
            if (i != beklenen || n < 8) {
                ++hata;
            } else {
                const auto b = static_cast<std::uint8_t>(i * 31u + n);
                for (std::size_t k = 4; k < n; ++k) {
                    if (p[k] != b) {
                        ++hata;
                        break;
                    }
                }
            }
            beklenen = i + 1;
            h.birak();
        }
        uretici.join();
        DOGRULA(d, hata == 0 && beklenen == kKayit, "kayıtlar sırayla ve bozulmadan geçti; ihlal " + std::to_string(hata));
    }
    return d.hata;
}

// Kesit–halka değişmezi, belirlenimci: dünya bir yayının ortasındayken (kesit yayına alındıktan ya
// da herhangi bir kayıttan sonra) kurulan oturum hiçbir doğumu/ölümü kaçırmaz ve iki kez görmez.
int test_kesit_halka_degismezi()
{
    Denetci d;
    int oturum = 0;
    for (std::uint64_t olay_adimi : {100u, 307u}) {
        for (int nokta = 0; nokta < 12; ++nokta) {
            Duzenek z;
            sahte::SahteAyar a = ornek_ayar();
            a.hedef_sayisi = 2;
            a.firlatma_adimi = 100;
            a.isabet_adimi = 307;
            sahte::SahteDunya dn(z.y, a);
            dn.basla();
            dn.adimla(olay_adimi - 1);
            z.c.tik(z.simdi);
            std::set<std::uint32_t> izlenen;
            bool tablo_var = false;
            int hata = 0;
            std::string ilk_hata;
            z.k.gozlemci = [&](const Cerceve& c) {
                if (c.tur() == IletiTuru::hosgeldin) {
                    izlenen.clear();
                    for (const GovdeBilgisi& g : hosgeldin_coz(c).govdeler) {
                        izlenen.insert(g.govde_kimligi);
                    }
                    tablo_var = true;
                } else if (c.tur() == IletiTuru::sahne_olayi) {
                    const SahneOlayi o = olay_coz(c);
                    if (o.tur == 1 && !izlenen.insert(o.govde_kimligi).second && hata++ == 0) {
                        ilk_hata = "doğum iki kez";
                    }
                    if (o.tur == 2 && izlenen.erase(o.govde_kimligi) != 1 && hata++ == 0) {
                        ilk_hata = "yaşamayan gövdenin ölümü";
                    }
                } else if (c.tur() == IletiTuru::durum) {
                    const viz::DurumIleti m = durum_coz(c);
                    if (kume(m) != izlenen && hata++ == 0) {
                        ilk_hata = "durumun gövde kümesi tabloyla ve olaylarla tutmuyor, adım " + std::to_string(m.d.adim_no);
                    }
                }
            };
            AraNokta ara;
            ara.z = &z;
            ara.hedef = nokta;
            z.y.ic().sinama_kancasi = &AraNokta::kanca;
            z.y.ic().sinama_baglami = &ara;
            dn.adimla(1);  // olay sınırının yayını: kanca ara noktalarda çağrılır
            z.y.ic().sinama_kancasi = nullptr;
            const std::string ad = "sınır " + std::to_string(olay_adimi) + ", ara nokta " + std::to_string(nokta);
            if (!ara.kuruldu) {
                DOGRULA(d, nokta >= ara.sayac, ad + ": oturum kurulamadı");
                continue;  // bu yayında o kadar ara nokta yok
            }
            ++oturum;
            for (int i = 0; i < 10; ++i) {
                dn.adimla(4);
                z.ilerlet(5'000'000, 5'000'000);
            }
            DOGRULA(d, tablo_var && hata == 0, ad + ": " + ilk_hata);
            DOGRULA(d, z.istemci_hatasiz() && !z.k.cerceve_hatasi, ad + ": istemci hatasız: " + z.istemci_son_nedeni());
            std::set<std::uint32_t> dogru{dn.arac_kimligi(), dn.hedef_kimligi(1)};
            if (olay_adimi == 100) {
                dogru.insert(dn.hedef_kimligi(0));
                dogru.insert(dn.torpido_kimligi());
            }
            DOGRULA(d, izlenen == dogru, ad + ": son gövde kümesi doğru");
        }
    }
    DOGRULA(d, oturum >= 10, "en az on ara noktada oturum kuruldu: " + std::to_string(oturum));
    std::printf("  %d oturum, her biri bir yayının farklı ara noktasında kuruldu\n", oturum);
    return d.hata;
}

}  // namespace ida::a2::test
