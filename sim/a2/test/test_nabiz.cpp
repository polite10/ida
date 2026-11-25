// test_nabiz.cpp — kabul 6 ve 7: nabız, kopma, yeniden bağlanma (A2-U15, U14'ün kamera dışı kısmı;
// A2-K62, K64, K65), koşunun ortasında bağlanan oturumun tutarlılığı ve çalıştırma nesli
// (A2-U27'nin kamera dışı kısmı; A2-K80, K96, K29, K24).
#include "takvim.hpp"
#include "test_destek.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstring>
#include <random>
#include <set>
#include <thread>

namespace ida::a2::test {

namespace {

std::set<std::uint32_t> kimlikler(const viz::DurumIleti& m)
{
    std::set<std::uint32_t> s;
    for (const GovdeDurumu& g : m.govdeler) {
        s.insert(g.govde_kimligi);
    }
    return s;
}

std::set<std::uint32_t> kimlikler(const viz::HosgeldinIleti& h)
{
    std::set<std::uint32_t> s;
    for (const GovdeBilgisi& g : h.govdeler) {
        s.insert(g.govde_kimligi);
    }
    return s;
}

std::set<std::uint32_t> dogru_yasayanlar(const sahte::SahteDunya& dn, std::uint64_t adim)
{
    std::set<std::uint32_t> s{dn.arac_kimligi()};
    for (std::uint32_t i = 0; i < dn.ayar().hedef_sayisi; ++i) {
        if (dn.hedef_yasiyor(i, adim)) {
            s.insert(dn.hedef_kimligi(i));
        }
    }
    if (dn.torpido_yasiyor(adim)) {
        s.insert(dn.torpido_kimligi());
    }
    return s;
}

// Bir oturumun çerçevelerinde gövde kümesini izler: hosgeldin tablosundan başlar, doğum ve ölüm
// olaylarıyla güncellenir; her durum iletisinin gövde kümesi izlenen kümeyle AYNI olmalıdır
// (arada kaybolan ya da iki kez uygulanan doğum/ölüm bunu bozar).
struct KumeIzleyici {
    std::set<std::uint32_t> kume;
    bool hosgeldin_var = false;
    std::uint64_t durum = 0;
    std::uint64_t olay = 0;
    int hata = 0;
    std::string ilk_hata;
    std::int64_t son_t = -1;

    void hata_yaz(const std::string& s)
    {
        if (hata++ == 0) {
            ilk_hata = s;
        }
    }
    void isle(const Cerceve& c)
    {
        if (c.tur() == IletiTuru::hosgeldin) {
            const viz::HosgeldinIleti h = hosgeldin_coz(c);
            kume = kimlikler(h);
            hosgeldin_var = true;
            son_t = h.h.t_ns;
        } else if (c.tur() == IletiTuru::sahne_olayi) {
            const SahneOlayi o = olay_coz(c);
            ++olay;
            if (o.tur == 1) {
                if (!kume.insert(o.govde_kimligi).second) {
                    hata_yaz("yaşayan gövde yeniden doğdu: " + std::to_string(o.govde_kimligi));
                }
            } else if (o.tur == 2) {
                if (kume.erase(o.govde_kimligi) != 1) {
                    hata_yaz("yaşamayan gövde öldü: " + std::to_string(o.govde_kimligi));
                }
            } else if (o.tur == 5) {
                kume.clear();
                son_t = -1;
            }
        } else if (c.tur() == IletiTuru::durum) {
            const viz::DurumIleti m = durum_coz(c);
            ++durum;
            if (kimlikler(m) != kume) {
                hata_yaz("durumun gövde kümesi izlenen kümeden farklı, adım " + std::to_string(m.d.adim_no));
            }
            if (m.d.t_ns < son_t) {
                hata_yaz("durum zamanı geriledi, adım " + std::to_string(m.d.adim_no));
            }
            son_t = m.d.t_ns;
        }
    }
};

}  // namespace

// A2-K62: sim_nabzi 1 s aralıkla; 3 s sessizlikte veda(nabiz_zaman_asimi); A2-K24: duraklatmada
// durum gitmez, nabız sürer.
int test_nabiz()
{
    Denetci d;
    {
        Duzenek z;
        sahte::SahteAyar a = ornek_ayar();
        a.sim_kipi = SimKipi::hil_bariyer;
        sahte::SahteDunya dn(z.y, a);
        dn.basla();
        DOGRULA(d, z.baglan(), "bağlandı");
        const std::int64_t t0 = z.simdi;
        for (int i = 0; i < 100; ++i) {  // 10 s
            dn.adimla(80);
            z.ilerlet(100'000'000);
        }
        std::vector<SimNabzi> nb;
        for (const Cerceve& c : z.k.cerceveler) {
            if (c.tur() == IletiTuru::sim_nabzi) {
                SimNabzi n{};
                DOGRULA(d, viz::coz_sim_nabzi(c.yuk, n) == viz::KodHata::yok, "sim_nabzi çözüldü");
                nb.push_back(n);
            }
        }
        DOGRULA(d, nb.size() == 10, "10 s'de 10 sim_nabzi: " + std::to_string(nb.size()));
        for (std::size_t i = 0; i < nb.size(); ++i) {
            DOGRULA(d, nb[i].gonderim_duvar_ns == t0 + static_cast<std::int64_t>(i + 1) * 1'000'000'000,
                    "nabız tam 1 s aralıkla (duvar saati)");
            DOGRULA(d, nb[i].sim_kipi == 3 && nb[i].calisma_durumu == 1, "nabız kip ve çalışma durumu");
            DOGRULA(d, nb[i].t_ns == static_cast<std::int64_t>(nb[i].adim_no) * kAdimNs && nb[i].adim_no > 0, "nabız sim zamanı");
        }
        DOGRULA(d, z.c.sayaclar().alinan_nabiz >= 9, "görselleştirici nabzı alındı");
        DOGRULA(d, z.c.sayaclar().cizim_hizi_mhz == 60'000, "nabız içeriği sayaçlarda");

        // Duraklatma: durum yok, nabız sürer; istemci 3 s kuralına takılmaz.
        const std::size_t durum_once = z.k.say(IletiTuru::durum);
        DOGRULA(d, z.y.duraklat(), "duraklatıldı");
        z.ilerlet(8'000'000'000);
        const std::size_t durum_sonra = z.k.say(IletiTuru::durum);
        DOGRULA(d, durum_sonra <= durum_once + 1, "duraklatmada yalnız sınırın durumu (en çok 1) gitti");
        DOGRULA(d, z.k.say(IletiTuru::sim_nabzi) == 18, "duraklatmada nabız 1 s aralıkla sürdü");
        SimNabzi son{};
        viz::coz_sim_nabzi(z.k.son(IletiTuru::sim_nabzi)->yuk, son);
        DOGRULA(d, son.calisma_durumu == 2 && son.adim_no == 8000, "nabız: duraklatıldı, adım donuk");
        DOGRULA(d, z.c.oturum_var() && z.ist.asama() == viz::Asama::akis && z.istemci_hatasiz(), "oturum sürüyor");
        DOGRULA(d, !z.y.adim_siniri(8001, {}), "duraklatılmışken adim_siniri reddedilir");
    }
    {  // 3 s sessizlik → veda(nabiz_zaman_asimi); sunucu yeni bağlantıya hazır (A2-U15, A2-K64)
        Duzenek z;
        sahte::SahteDunya dn(z.y, ornek_ayar());
        dn.basla();
        DOGRULA(d, z.baglan(), "bağlandı");
        const std::uint64_t ilk_kimlik = z.c.sayaclar().oturum_kimligi;
        z.ilerlet(2'900'000'000, 100'000'000, false);  // istemci donuk: nabız göndermez
        DOGRULA(d, z.c.oturum_var(), "2,9 s: oturum açık");
        z.ilerlet(200'000'000, 100'000'000, false);
        DOGRULA(d, z.k.son_veda(VedaNedeni::nabiz_zaman_asimi), "3 s sessizlik → veda(nabiz_zaman_asimi)");
        DOGRULA(d, !z.c.oturum_var() && z.c.sayaclar().son_kopma_nedeni == KopmaNedeni::nabiz_zaman_asimi, "oturum kapandı");
        DOGRULA(d, z.baglan(), "yeni bağlantı kabul edildi");
        DOGRULA(d, z.c.sayaclar().oturum_kimligi != ilk_kimlik && z.c.sayaclar().oturum_kimligi != 0, "yeni oturum kimliği");
        DOGRULA(d, z.ist.asama() == viz::Asama::akis, "yeni oturum akışta");
    }
    return d.hata;
}

// A2-K64, K65: istemci kapatılıp yeniden bağlanınca yeni oturum kimliği ve TAM durum.
int test_yeniden_baglanma()
{
    Denetci d;
    Duzenek z;
    sahte::SahteAyar a = ornek_ayar();
    a.hedef_sayisi = 3;
    a.firlatma_adimi = 150;
    a.isabet_adimi = 900;
    sahte::SahteDunya dn(z.y, a);
    dn.basla();
    DOGRULA(d, z.baglan(), "ilk oturum");
    const std::uint64_t k1 = z.ist.oturum_kimligi();
    for (int i = 0; i < 10; ++i) {
        dn.adimla(10);
        z.ilerlet(12'500'000, 12'500'000);
    }
    // İstemci öldürülür (soket kapanır); dünya beklemeden ilerler: fırlatma bu arada olur.
    z.istemciyi_kopar();
    DOGRULA(d, !z.c.oturum_var() && z.c.sayaclar().son_kopma_nedeni == KopmaNedeni::karsi_kapatti, "oturum bitti (A2-K64)");
    for (int i = 0; i < 20; ++i) {
        dn.adimla(10);
        z.pompala();  // oturum yokken de halka boşaltılır
    }
    DOGRULA(d, dn.adim() == 300, "dünya ilerledi");
    const std::size_t once = z.k.cerceveler.size();
    DOGRULA(d, z.baglan(), "ikinci oturum");
    const std::uint64_t k2 = z.ist.oturum_kimligi();
    DOGRULA(d, k2 != 0 && k2 != k1, "yeni oturum kimliği");
    const auto& f = z.k.cerceveler;
    DOGRULA(d, f.size() >= once + 5, "yeni oturumun çerçeveleri");
    if (f.size() >= once + 5) {
        DOGRULA(d, f[once].tur() == IletiTuru::hosgeldin && f[once].b.sira == 0 && f[once + 1].tur() == IletiTuru::dalga_kumesi &&
                       f[once + 2].tur() == IletiTuru::ortam && f[once + 3].tur() == IletiTuru::malzeme_optik &&
                       f[once + 4].tur() == IletiTuru::durum,
                "tam durum yeniden gönderildi; sıra 0'dan başladı");
        const viz::HosgeldinIleti h = hosgeldin_coz(f[once]);
        DOGRULA(d, h.h.adim_no == 300 && h.h.calisma_nesli == 1, "hosgeldin o anki adım sınırında, nesil değişmedi");
        DOGRULA(d, kimlikler(h) == dogru_yasayanlar(dn, 300) && h.govdeler.size() == 5, "tablo: o sınırda yaşayan gövdeler (torpido dahil)");
        for (const GovdeBilgisi& g : h.govdeler) {
            if (g.govde_kimligi == dn.arac_kimligi()) {
                DOGRULA(d, g.sureklilik_no == 1 && g.dolu_tupler == 2, "araç: fırlatma sonrası süreklilik 1, tüpler 2");
            }
            if (g.govde_kimligi == dn.torpido_kimligi()) {
                DOGRULA(d, g.sureklilik_no == 0 && g.govde_turu == 2 && g.varlik_indeksi == sahte::kVarlikTorpido, "torpido tabloda");
            }
        }
        const viz::DurumIleti ilk = durum_coz(f[once + 4]);
        DOGRULA(d, ilk.d.adim_no == 300 && kimlikler(ilk) == kimlikler(h), "ilk durum tabloyla aynı sınırda ve aynı gövdelerle");
    }
    // Fırlatmanın olayları yeni oturuma YİNELENMEZ (tabloya yansımıştır).
    std::size_t olay = 0;
    for (std::size_t i = once; i < f.size(); ++i) {
        if (f[i].tur() == IletiTuru::sahne_olayi) {
            ++olay;
        }
    }
    DOGRULA(d, olay == 0, "kesitten önceki olaylar yinelenmedi");
    for (int i = 0; i < 70; ++i) {
        dn.adimla(10);
        z.ilerlet(12'500'000, 12'500'000);
    }
    DOGRULA(d, z.istemci_hatasiz() && !z.k.cerceve_hatasi, "ikinci oturum hatasız: " + z.istemci_son_nedeni());
    const viz::GovdeKaydi* tor = z.ist.sahne().bul(dn.torpido_kimligi());
    DOGRULA(d, tor != nullptr && tor->t_olum == 900 * kAdimNs && tor->t_dogum == viz::kZamanEksiSonsuz,
            "tablodan gelen torpido sonradan öldü (ölüm olayı yeni oturuma ulaştı)");
    DOGRULA(d, z.c.sayaclar().baglanti_sayisi == 2 && z.c.sayaclar().biten_oturum == 1, "bağlantı sayaçları");
    return d.hata;
}

// Koşunun ortasında bağlanma, her adım sınırı için (A2-K65): hosgeldin tablosu ile ilk durum aynı
// sınırdandır; arada kaybolan ya da yinelenen doğum/ölüm yoktur. Halkada kesitten eski kayıtlar
// bekliyor olsa da (tik atılmamışsa) sonuç değişmez.
int test_ortada_baglanma()
{
    Denetci d;
    int oturum = 0;
    for (std::uint64_t c = 95; c <= 312; ++c) {
        for (int bayat_halka = 0; bayat_halka < 2; ++bayat_halka) {
            if (bayat_halka == 1 && c % 3 != 0) {
                continue;
            }
            Duzenek z;
            sahte::SahteAyar a = ornek_ayar();
            a.hedef_sayisi = 2;
            a.firlatma_adimi = 100;
            a.isabet_adimi = 307;
            sahte::SahteDunya dn(z.y, a);
            dn.basla();
            KumeIzleyici izleyici;
            z.k.gozlemci = [&](const Cerceve& cc) { izleyici.isle(cc); };
            if (bayat_halka == 0) {
                for (std::uint64_t i = 0; i < c; ++i) {
                    dn.adimla(1);
                    z.c.tik(z.simdi);  // oturum yok: kayıtlar atılır
                }
            } else {
                dn.adimla(c);  // halka hiç boşaltılmadı: kesitten eski kayıtlar bekliyor
            }
            const std::string ad = "bağlanma adımı " + std::to_string(c) + (bayat_halka ? " (bayat halka)" : "");
            DOGRULA(d, z.baglan(), ad + ": bağlandı");
            ++oturum;
            const auto& f = z.k.cerceveler;
            if (f.size() < 5 || f[0].tur() != IletiTuru::hosgeldin || f[4].tur() != IletiTuru::durum) {
                DOGRULA(d, false, ad + ": el sıkışma çerçeveleri eksik");
                continue;
            }
            const viz::HosgeldinIleti h = hosgeldin_coz(f[0]);
            const viz::DurumIleti ilk = durum_coz(f[4]);
            DOGRULA(d, h.h.adim_no == c && ilk.d.adim_no == c, ad + ": hosgeldin ve ilk durum aynı sınırda");
            DOGRULA(d, kimlikler(h) == dogru_yasayanlar(dn, c), ad + ": tablo o sınırda yaşayanlar");
            DOGRULA(d, kimlikler(ilk) == kimlikler(h), ad + ": ilk durumun gövdeleri tabloyla aynı");
            bool surek = h.govdeler.size() == ilk.govdeler.size();
            for (std::size_t i = 0; surek && i < h.govdeler.size(); ++i) {
                surek = h.govdeler[i].govde_kimligi == ilk.govdeler[i].govde_kimligi &&
                        h.govdeler[i].sureklilik_no == ilk.govdeler[i].sureklilik_no &&
                        h.govdeler[i].dolu_tupler == ilk.govdeler[i].dolu_tupler;
            }
            DOGRULA(d, surek, ad + ": süreklilik numaraları ve tüpler tabloyla ilk durumda aynı");
            // Koşu sürer: bütün olaylar ve durumlar tutarlı kalır.
            while (dn.adim() < 330) {
                dn.adimla(3);
                z.ilerlet(3'750'000, 3'750'000);
            }
            DOGRULA(d, izleyici.hata == 0, ad + ": " + izleyici.ilk_hata);
            DOGRULA(d, izleyici.kume == dogru_yasayanlar(dn, 330), ad + ": son gövde kümesi doğru");
            DOGRULA(d, z.istemci_hatasiz() && !z.k.cerceve_hatasi, ad + ": istemci hatasız: " + z.istemci_son_nedeni());
            DOGRULA(d, z.ist.sahne().bul(dn.torpido_kimligi()) == nullptr ||
                           z.ist.sahne().bul(dn.torpido_kimligi())->t_olum == 307 * kAdimNs,
                    ad + ": torpido ya hiç görülmedi ya da 307'de öldü");
            // Olay sayısı: bağlanma sınırından SONRAKİ olaylar (≤ c olanlar kesitte).
            const std::uint64_t beklenen_olay = (c < 100 ? 2u : 0u) + (c < 307 ? 3u : 0u);
            DOGRULA(d, izleyici.olay == beklenen_olay, ad + ": olay sayısı " + std::to_string(izleyici.olay));
        }
    }
    std::printf("  %d oturum, her biri farklı adım sınırında kuruldu\n", oturum);
    return d.hata;
}

// Aynı şey eşzamanlı: dünya iş parçacığı sürekli doğum/ölüm üretirken G/Ç tarafı (bu iş parçacığı)
// oturumları açıp kapatır. Kesit ile halka arasındaki değişmez gerçek yarış altında sınanır.
namespace {

void eszamanli_kos(Denetci& d, std::size_t halka_bayt, bool cogu_olagan)
{
    Duzenek z(static_cast<std::size_t>(kGondermeKuyruguAzamiBayt), halka_bayt);
    sahte::SahteAyar a = ornek_ayar();
    a.hedef_sayisi = 0;
    sahte::SahteDunya tanim(z.y, a);
    DOGRULA(d, tanim.basla(), "çalıştırma başladı");

    std::atomic<bool> dur{false};
    std::atomic<std::uint64_t> adim_a{0};
    std::thread dunya([&] {
        // Sabit tohumlu sahte dünya: araç her zaman yaşar; hedefler rastgele doğar ve ölür.
        std::mt19937 rng(0xA2A2u);
        std::vector<std::uint32_t> yasayan;  // araç dışındakiler
        std::uint32_t sonraki_kimlik = 1000;
        std::uint8_t tupler = 3;
        std::array<GovdeAni, kAzamiGovde> an{};
        std::uint64_t adim = 0;
        std::uint64_t adim0 = 0;
        auto hedef0 = std::chrono::steady_clock::now();
        while (!dur.load(std::memory_order_relaxed)) {
            ++adim;
            const std::uint32_t zar = rng() % 100u;
            if (zar < 12 && yasayan.size() < 12) {
                const std::uint32_t kimlik = sonraki_kimlik++;
                z.y.govde_dogdu(adim, GovdeKaydi{kimlik, sahte::kVarlikHedef, GovdeTuru::hedef, 0});
                yasayan.push_back(kimlik);
            } else if (zar < 22 && !yasayan.empty()) {
                const std::size_t i = rng() % yasayan.size();
                z.y.govde_oldu(adim, yasayan[i], OlumNedeni::sure);
                yasayan.erase(yasayan.begin() + static_cast<std::ptrdiff_t>(i));
            } else if (zar < 25) {
                tupler = static_cast<std::uint8_t>(tupler ^ 1u);
                z.y.kutle_durumu(adim, tanim.arac_kimligi(), tupler);
            }
            an[0] = tanim.dogru_arac(adim);
            for (std::size_t i = 0; i < yasayan.size(); ++i) {
                GovdeAni g;
                g.kimlik = yasayan[i];
                g.p_n = {static_cast<double>(yasayan[i]), static_cast<double>(adim), 1.0};
                an[i + 1] = g;
            }
            z.y.adim_siniri(adim, std::span<const GovdeAni>(an.data(), yasayan.size() + 1));
            adim_a.store(adim, std::memory_order_relaxed);
            // Dünya G/Ç tarafının izleyebileceği hızda koşar (gerçek zamanın çok üstünde); arada
            // hız sınırsızlaşır ki yayın taşması ve yeniden eşitleme yolu da sınansın.
            if (adim % 16 == 0) {
                const bool kosu = (adim / 2'000) % 8 == 7;  // her sekiz dilimden biri sınırsız hızda
                if (kosu) {
                    hedef0 = std::chrono::steady_clock::now();
                    adim0 = adim;
                } else {
                    // ≈ 20 000 adım/s (gerçek zamanın 25 katı; debug derlemesinde istemci tarafı yetişsin)
                    const auto hedef = hedef0 + std::chrono::microseconds((adim - adim0) * 50u);
                    while (std::chrono::steady_clock::now() < hedef && !dur.load(std::memory_order_relaxed)) {
                        std::this_thread::sleep_for(std::chrono::microseconds(100));
                    }
                }
            }
        }
    });

    KumeIzleyici izleyici;
    z.k.sakla = false;
    z.k.gozlemci = [&](const Cerceve& c) { izleyici.isle(c); };
    int oturum = 0;
    int tasma_ile_biten = 0;
    std::uint64_t toplam_durum = 0;
    std::uint64_t toplam_olay = 0;
    const auto son = std::chrono::steady_clock::now() + std::chrono::milliseconds(1500);
    while (std::chrono::steady_clock::now() < son || oturum < 50) {
        izleyici = KumeIzleyici{};
        if (!z.baglan()) {
            DOGRULA(d, false, "bağlanamadı");
            break;
        }
        ++oturum;
        // Oturum birkaç tur sürer; tur sayısı oturuma göre değişir.
        const int tur = 1 + oturum % 7;
        for (int i = 0; i < tur; ++i) {
            z.ilerlet(1'000'000, 1'000'000);
            if (oturum % 3 == 0) {
                std::this_thread::yield();
            }
            if (oturum % 16 == 0) {
                gercek_bekle_ms(1);  // uzun oturumlar: tur başına onlarca adım ve olay
            }
        }
        // Oturumu yalnız yayın taşması (dünya G/Ç'den hızlı yazdı) sunucu tarafından bitirebilir;
        // o da veda(tampon_tasti) ile olur ve istemci için protokol hatası değildir.
        const bool sunucu_bitirdi = !z.c.oturum_var();
        if (sunucu_bitirdi) {
            ++tasma_ile_biten;
            DOGRULA(d, z.c.sayaclar().son_kopma_nedeni == KopmaNedeni::yayin_tasmasi,
                    std::string("beklenmeyen kopma: ") + kopma_nedeni_adi(z.c.sayaclar().son_kopma_nedeni));
        } else {
            DOGRULA(d, izleyici.hosgeldin_var, "hosgeldin alındı");
        }
        DOGRULA(d, izleyici.hata == 0, "oturum " + std::to_string(oturum) + ": " + izleyici.ilk_hata);
        DOGRULA(d, z.istemci_hatasiz(), "istemci hatasız: " + z.istemci_son_nedeni());
        DOGRULA(d, !z.k.cerceve_hatasi, "sunucu çıktısı düzgün çerçeve");
        toplam_durum += izleyici.durum;
        toplam_olay += izleyici.olay;
        if (d.hata != 0) {
            break;
        }
        // Oturumların yarısı çökmeyle, yarısı sunucu açıkken istemcinin olağan vedasıyla biter.
        if (sunucu_bitirdi) {
            z.pompala();
        } else if (oturum % 2 == 0) {
            z.istemciyi_kopar();
        } else {
            z.ist.olagan_kapat();
            z.pompala();
        }
        DOGRULA(d, !z.c.oturum_var(), "oturum kapandı");
        z.olaylar.clear();
    }
    dur.store(true);
    dunya.join();
    const SunucuSayaclari s = z.c.sayaclar();
    std::printf("  ölçüm (bu Mac): %d oturum (%d'i yayın taşmasıyla bitti), %llu durum, %llu olay; dünya %llu adım attı; yayın taşması: durum %llu, korunan %llu\n",
                oturum, tasma_ile_biten, static_cast<unsigned long long>(toplam_durum), static_cast<unsigned long long>(toplam_olay),
                static_cast<unsigned long long>(adim_a.load()), static_cast<unsigned long long>(s.yayin.dusen_durum),
                static_cast<unsigned long long>(s.yayin.kayip_korunan));
    DOGRULA(d, oturum >= 50 && toplam_olay > 1000 && toplam_durum > static_cast<std::uint64_t>(oturum), "yeterli kapsama");
    if (cogu_olagan) {
        DOGRULA(d, tasma_ile_biten * 2 < oturum, "oturumların çoğu olağan sürdü (sınama yalnız taşma yolunu koşmadı)");
    }
    DOGRULA(d, s.protokol_hatasi == 0 && s.yayin.gecersiz_cagri == 0, "sunucu ve yayıncı hata saymadı");
}

}  // namespace

int test_ortada_baglanma_eszamanli()
{
    Denetci d;
    eszamanli_kos(d, std::size_t{8} << 20u, true);
    // Küçük halka: dünyanın sınırsız hızlı dilimlerinde yayın taşar; oturum veda(tampon_tasti) ile
    // yeniden eşitlenir ve sonraki oturum yine tutarlıdır.
    eszamanli_kos(d, std::size_t{256} << 10u, false);
    return d.hata;
}

// Çalıştırma nesli (A2-K80, K96): yeni çalıştırmada veda(yeni_calistirma) ve yeni oturumda nesil +1.
int test_nesil_yeni_calistirma()
{
    Denetci d;
    Duzenek z;
    sahte::SahteDunya dn(z.y, ornek_ayar());
    DOGRULA(d, z.y.calisma_nesli() == 1, "süreç başında nesil 1");
    dn.basla();
    DOGRULA(d, z.y.calisma_nesli() == 1, "ilk çalıştırma nesil 1 ile başlar (A2-K80)");
    DOGRULA(d, z.baglan(), "bağlandı");
    DOGRULA(d, z.ist.calisma_nesli() == 1, "hosgeldin.calisma_nesli = 1");
    dn.adimla(100);
    z.ilerlet(125'000'000);
    const std::uint64_t k1 = z.ist.oturum_kimligi();

    // Aynı süreçte yeni çalıştırma (farklı sahne: 3 hedef).
    sahte::SahteAyar a2 = ornek_ayar();
    a2.hedef_sayisi = 3;
    a2.kimlik_tabani = 5000;
    a2.manifest_yolu = "/sahte/a2/ikinci.json";
    sahte::SahteDunya dn2(z.y, a2);
    DOGRULA(d, dn2.basla(), "ikinci çalıştırma");
    DOGRULA(d, z.y.calisma_nesli() == 2, "yeni çalıştırmada nesil tam 1 arttı");
    z.pompala();
    DOGRULA(d, z.k.son_veda(VedaNedeni::yeni_calistirma), "veda(yeni_calistirma) (A2-K96 (3))");
    DOGRULA(d, !z.c.oturum_var() && z.c.sayaclar().son_kopma_nedeni == KopmaNedeni::yeni_calistirma, "oturum kapandı");
    DOGRULA(d, z.ist.yeniden_baglanabilir(), "istemci yeniden bağlanabilir (A2-K63)");
    // Yeni çalıştırmanın ilk adımından önce yeni bağlantı kabul edilir; fizik bağlantıyı beklemez.
    dn2.adimla(40);
    DOGRULA(d, z.baglan(), "yeni oturum");
    DOGRULA(d, z.ist.calisma_nesli() == 2 && z.ist.oturum_kimligi() != k1, "yeni oturum: nesil 2, yeni kimlik");
    DOGRULA(d, z.ist.hosgeldin().govdeler.size() == 4 && z.ist.hosgeldin().govdeler[0].govde_kimligi == dn2.arac_kimligi(),
            "yeni çalıştırmanın gövdeleri");
    DOGRULA(d, viz::metin_oku(z.ist.hosgeldin().h.manifest_yolu, 512) == "/sahte/a2/ikinci.json", "yeni çalıştırmanın manifesti");
    DOGRULA(d, z.ist.hosgeldin().h.adim_no == 40, "sim zamanı yeni çalıştırmanınki");
    dn2.adimla(100);
    z.ilerlet(125'000'000);
    DOGRULA(d, z.istemci_hatasiz() && z.c.oturum_var(), "yeni oturum akıyor: " + z.istemci_son_nedeni());

    // Üçüncü çalıştırma, oturum yokken: yine tam 1 artar.
    z.istemciyi_kopar();
    DOGRULA(d, dn.basla() && z.y.calisma_nesli() == 3, "üçüncü çalıştırma: nesil 3");
    DOGRULA(d, z.baglan() && z.ist.calisma_nesli() == 3, "oturum nesil 3 ile kuruldu");
    DOGRULA(d, z.c.sayaclar().baglanti_sayisi == 3, "üç oturum");
    return d.hata;
}

// Sıfırlama (A2-K29, K80): nesil +1, oturum SÜRER; duraklatma nesli değiştirmez (A2-K24).
int test_nesil_sifirlama_duraklatma()
{
    Denetci d;
    Duzenek z;
    sahte::SahteAyar a = ornek_ayar();
    a.hedef_sayisi = 2;
    a.firlatma_adimi = 60;
    sahte::SahteDunya dn(z.y, a);
    dn.basla();
    DOGRULA(d, z.baglan(), "bağlandı");
    const std::uint64_t kimlik = z.ist.oturum_kimligi();
    dn.adimla(100);
    z.ilerlet(125'000'000);
    const std::uint32_t eski_arac = dn.arac_kimligi();
    DOGRULA(d, z.ist.sahne().bul(dn.torpido_kimligi()) != nullptr, "torpido sahnede");

    // ---- duraklatma: nesil değişmez ----
    const std::size_t n0 = z.k.cerceveler.size();
    DOGRULA(d, z.y.duraklat() && z.y.calisma_nesli() == 1, "duraklatma nesli değiştirmedi");
    z.ilerlet(2'000'000'000);
    {
        const auto& f = z.k.cerceveler;
        DOGRULA(d, f.size() >= n0 + 2 && f[n0].tur() == IletiTuru::durum && f[n0 + 1].tur() == IletiTuru::sahne_olayi,
                "duraklatma sınırının durumu, sonra duraklat olayı");
        if (f.size() >= n0 + 2) {
            const viz::DurumIleti m = durum_coz(f[n0]);
            const SahneOlayi o = olay_coz(f[n0 + 1]);
            DOGRULA(d, m.d.adim_no == 100 && m.d.bayraklar == kDurumTakvimDisi && m.d.calisma_durumu == 1,
                    "sınır durumu: adım 100, takvim dışı");
            DOGRULA(d, o.tur == 6 && o.adim_no == 100 && o.calisma_nesli == 1, "duraklat olayı");
        }
        std::size_t durum = 0;
        for (std::size_t i = n0 + 2; i < f.size(); ++i) {
            if (f[i].tur() == IletiTuru::durum) {
                ++durum;
            }
        }
        DOGRULA(d, durum == 0, "duraklat olayından sonra durum yok (A2-K24)");
    }
    // Duraklatılmışken bağlanan ikinci istemci (ilk oturum kapatılır): ilk durum "duraklatıldı" taşır.
    z.istemciyi_kopar();
    const std::size_t n1 = z.k.cerceveler.size();
    DOGRULA(d, z.baglan(), "duraklatılmışken bağlandı");
    {
        const auto& f = z.k.cerceveler;
        DOGRULA(d, f.size() >= n1 + 5 && f[n1 + 4].tur() == IletiTuru::durum, "duraklatılmışken de tam durum ve ilk durum");
        if (f.size() >= n1 + 5) {
            const viz::DurumIleti m = durum_coz(f[n1 + 4]);
            DOGRULA(d, m.d.calisma_durumu == 2 && m.d.adim_no == 100 && m.govdeler.size() == 4, "ilk durum: duraklatıldı, 4 gövde");
        }
        DOGRULA(d, z.ist.calisma_nesli() == 1, "nesil hâlâ 1");
    }
    const std::uint64_t kimlik2 = z.ist.oturum_kimligi();
    DOGRULA(d, kimlik2 != kimlik, "yeni oturum kimliği");
    DOGRULA(d, z.y.surdur(), "sürdürüldü");
    dn.adimla(50);
    z.ilerlet(62'500'000);
    DOGRULA(d, z.y.calisma_nesli() == 1 && z.ist.calisma_nesli() == 1, "sürdürme nesli değiştirmedi");
    DOGRULA(d, z.k.son(IletiTuru::sahne_olayi) != nullptr && olay_coz(*z.k.son(IletiTuru::sahne_olayi)).tur == 7, "devam olayı");

    // ---- sıfırlama: nesil +1, oturum sürer ----
    const std::size_t n2 = z.k.cerceveler.size();
    dn.sifirla();
    DOGRULA(d, z.y.calisma_nesli() == 2, "sıfırlamada nesil tam 1 arttı");
    z.ilerlet(10'000'000, 10'000'000);
    DOGRULA(d, z.c.oturum_var() && z.ist.asama() == viz::Asama::akis && z.ist.oturum_kimligi() == kimlik2,
            "sıfırlama oturumu kapatmadı (A2-K85)");
    DOGRULA(d, z.k.say(IletiTuru::veda) == 0, "veda gönderilmedi");
    DOGRULA(d, z.ist.calisma_nesli() == 2, "istemci yeni nesli olaydan öğrendi");
    {
        const auto& f = z.k.cerceveler;
        DOGRULA(d, f.size() >= n2 + 5, "sıfırlama çerçeveleri");
        if (f.size() >= n2 + 5) {
            const SahneOlayi s = olay_coz(f[n2]);
            DOGRULA(d, f[n2].tur() == IletiTuru::sahne_olayi && s.tur == 5 && s.calisma_nesli == 2 && s.adim_no == 0 && s.t_ns == 0,
                    "sifirlama olayı: yeni nesil, yeni sim zamanı");
            bool dogumlar = true;
            for (std::size_t i = 1; i <= 3; ++i) {
                const SahneOlayi o = olay_coz(f[n2 + i]);
                dogumlar = dogumlar && f[n2 + i].tur() == IletiTuru::sahne_olayi && o.tur == 1 && o.calisma_nesli == 2 &&
                           o.adim_no == 0 && o.sureklilik_no == 0;
            }
            DOGRULA(d, dogumlar, "sıfırlamadan sonra yaşayan gövdeler için govde_dogdu (A2-K29)");
            const viz::DurumIleti m = durum_coz(f[n2 + 4]);
            DOGRULA(d, f[n2 + 4].tur() == IletiTuru::durum && m.d.adim_no == 0 && m.govdeler.size() == 3, "yeni zamanda ilk durum");
        }
    }
    DOGRULA(d, dn.arac_kimligi() != eski_arac, "sıfırlamadan sonra yeni kimlikler (A2-K23)");
    DOGRULA(d, z.ist.sahne().bul(eski_arac) == nullptr && z.ist.sahne().bul(dn.arac_kimligi()) != nullptr &&
                   z.ist.sahne().govdeler().size() == 3,
            "istemci eski gövdeleri sildi, yenilerini kurdu");
    dn.adimla(100);  // yeni zaman çizgisinde fırlatma yeniden olur (adım 60)
    z.ilerlet(125'000'000);
    DOGRULA(d, z.istemci_hatasiz() && !z.k.cerceve_hatasi, "sıfırlama sonrası akış hatasız: " + z.istemci_son_nedeni());
    DOGRULA(d, z.ist.sahne().bul(dn.torpido_kimligi()) != nullptr, "yeni zaman çizgisinin torpidosu sahnede");
    // Sıfırlamadan sonra bağlanan oturum yeni nesli hosgeldin'de görür.
    z.istemciyi_kopar();
    DOGRULA(d, z.baglan() && z.ist.calisma_nesli() == 2 && z.ist.hosgeldin().h.calisma_nesli == 2, "yeni oturumda nesil 2");

    // ---- durdurma: son durum ve nabız "durdu" taşır; nesil değişmez ----
    DOGRULA(d, z.y.calistirma_durdu() && z.y.calisma_nesli() == 2, "durdurma nesli değiştirmedi");
    z.ilerlet(1'100'000'000);
    const Cerceve* sd = z.k.son(IletiTuru::durum);
    DOGRULA(d, sd != nullptr && durum_coz(*sd).d.calisma_durumu == 3, "son durum: durdu");
    SimNabzi nb{};
    const Cerceve* sn = z.k.son(IletiTuru::sim_nabzi);
    DOGRULA(d, sn != nullptr && viz::coz_sim_nabzi(sn->yuk, nb) == viz::KodHata::yok && nb.calisma_durumu == 3, "nabız: durdu");
    DOGRULA(d, z.istemci_hatasiz(), "istemci hatasız: " + z.istemci_son_nedeni());
    return d.hata;
}

}  // namespace ida::a2::test
