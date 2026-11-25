// test_yavas.cpp — kabul 5: yavaş, donuk ve ölü görselleştirici (A2-U17, A2-U10'un kitaplık düzeyi;
// A2-K17, K26, K47). Asıl ölçüt: dünya iş parçacığının yayın çağrısı hiçbir koşulda beklemez.
//
// Zamanlama eşikleri yalnız ortak yardımcıdan geçer (W4-K24, W4-K29): paylaşılan CI'da, debug ve
// sanitizer'lı derlemede ölçülür ve yazdırılır, sonucu belirlemez. İşlevsel denetimler (olayların
// kaybolmaması, sayaçlar, veda(tampon_tasti), bellek tavanı) her ortamda geçerlidir.
#include "test_destek.hpp"
#include "zamanlama_olcutu.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <thread>

namespace ida::a2::test {

// Yavaş okuyan istemci (soketsiz, belirlenimci): durum seyreltilir ve sayılır, olaylar kaybolmaz,
// kuyruk taşınca veda(tampon_tasti).
int test_yavas_istemci()
{
    Denetci d;
    const std::size_t kSinir = std::size_t{256} << 10u;
    Duzenek z(kSinir);
    sahte::SahteAyar a = ornek_ayar();
    a.hedef_sayisi = 15;  // durum iletisi ≈ 1968 B
    sahte::SahteDunya dn(z.y, a);
    dn.basla();
    DOGRULA(d, z.baglan(), "bağlandı");
    // Tur başına 13 adım (≈ 1 durum, ≈ 2 KB) üretilir; istemci tur başına 600 bayt okur.
    z.istemci_okuma_siniri = 600;
    std::array<GovdeAni, kAzamiGovde> an{};
    std::uint32_t olay = 0;
    std::uint64_t adim = 0;
    const auto tur = [&](int adim_sayisi) {
        for (int i = 0; i < adim_sayisi; ++i) {
            ++adim;
            if (adim % 40 == 5) {
                ++olay;
                z.y.isabet(adim, 1, 2, {static_cast<double>(olay), 0, 0}, {1, 0, 0}, {0, 0, 0});
            }
            const std::size_t m = dn.dogru_anlar(adim, an);
            z.y.adim_siniri(adim, std::span<const GovdeAni>(an.data(), m));
        }
        z.ilerlet(16'250'000, 16'250'000);
    };
    for (int i = 0; i < 6000; ++i) {
        tur(13);
    }
    SunucuSayaclari s = z.c.sayaclar();
    DOGRULA(d, z.c.oturum_var(), "oturum sürüyor (olaylar kuyruğa sığıyor)");
    DOGRULA(d, s.kuyrukta_atilan_durum > 1000, "A2-K26: takvimli durumlar atıldı ve sayıldı: " +
                                                 std::to_string(s.kuyrukta_atilan_durum));
    DOGRULA(d, s.azami_kuyruk_bayt <= kSinir, "kuyruk sınırı aşılmadı: " + std::to_string(s.azami_kuyruk_bayt));
    DOGRULA(d, s.kuyruk_tasmasi == 0 && s.protokol_hatasi == 0, "taşma ve hata yok");
    // İstemcinin o ana kadar aldığı olaylar eksiksiz ve sıralı.
    const auto olaylari_denetle = [&](const char* ad) {
        std::uint32_t gelen = 0;
        bool sirali = true;
        for (const Cerceve& c : z.k.cerceveler) {
            if (c.tur() == IletiTuru::sahne_olayi) {
                ++gelen;
                sirali = sirali && olay_coz(c).nokta_n[0] == static_cast<double>(gelen);
            }
        }
        DOGRULA(d, sirali, std::string(ad) + ": olaylar sırayla ve boşluksuz");
        return gelen;
    };
    olaylari_denetle("yavaş okuma");
    DOGRULA(d, z.istemci_hatasiz() && !z.k.cerceve_hatasi, "istemci hatasız: " + z.istemci_son_nedeni());
    // İstemci hızlanınca: kuyruktaki bütün olaylar ulaşır (hiçbiri atılmamıştır).
    DOGRULA(d, olay == 1950, "1950 olay üretildi");
    z.istemci_okuma_siniri = 0;
    tur(13);
    tur(13);  // boşluklardan sonra en az bir ileti daha gelsin (istemci boşluğu sonraki iletide sayar)
    z.ilerlet(50'000'000, 10'000'000);
    const std::uint32_t gelen = olaylari_denetle("hızlandıktan sonra");
    DOGRULA(d, gelen == olay, "bütün olaylar ulaştı: " + std::to_string(gelen) + "/" + std::to_string(olay));
    s = z.c.sayaclar();
    DOGRULA(d, z.ist.sayaclar().sira_boslugu == s.kuyrukta_atilan_durum,
            "atılan her durum istemcide sıra boşluğu: " + std::to_string(z.ist.sayaclar().sira_boslugu));
    DOGRULA(d, z.istemci_hatasiz(), "istemci hatasız");

    // İstemci tamamen durur; yalnız olay üretilir: kuyruk dolar → veda(tampon_tasti).
    z.istemci_okuma_siniri = 1;
    int kalan_tur = 0;
    for (; kalan_tur < 4000 && z.c.oturum_var(); ++kalan_tur) {
        for (int i = 0; i < 40; ++i) {
            ++adim;
            z.y.isabet(adim, 1, 2, {0, 0, 0}, {1, 0, 0}, {0, 0, 0});
            const std::size_t m = dn.dogru_anlar(adim, an);
            z.y.adim_siniri(adim, std::span<const GovdeAni>(an.data(), m));
        }
        z.ilerlet(1'000'000, 1'000'000);
    }
    s = z.c.sayaclar();
    DOGRULA(d, !z.c.oturum_var() && s.kuyruk_tasmasi == 1 && s.son_kopma_nedeni == KopmaNedeni::tampon_tasti,
            "kuyruk taşınca oturum kapandı (tur " + std::to_string(kalan_tur) + ")");
    DOGRULA(d, s.azami_kuyruk_bayt <= kSinir, "bellek tavanı: kuyruk hiçbir an sınırı aşmadı");
    z.istemci_okuma_siniri = 0;
    z.pompala();
    z.pompala();
    DOGRULA(d, z.k.son_veda(VedaNedeni::tampon_tasti), "son ileti veda(tampon_tasti)");
    DOGRULA(d, !z.k.cerceve_hatasi, "kapanışta çerçeve bütünlüğü korundu (kısmen yazılmış ileti atılmadı)");
    DOGRULA(d, z.ist.son_kapanis_nedeni().find("veda") != std::string::npos && z.ist.yeniden_baglanabilir(),
            "istemci vedayı aldı ve yeniden bağlanabilir: " + z.ist.son_kapanis_nedeni());
    // Dünya bu sırada hiçbir çağrıda reddedilmedi/beklemedi; yeni oturum tam durumla kurulur.
    DOGRULA(d, z.y.sayaclar().gecersiz_cagri == 0 && z.y.sayaclar().kayip_korunan == 0, "yayıncı etkilenmedi");
    DOGRULA(d, z.baglan() && z.ist.asama() == viz::Asama::akis, "yeniden bağlandı");
    return d.hata;
}

namespace {

struct Olcum {
    double p50_ns = 0, p99_ns = 0, azami_ns = 0;
    std::uint64_t sayi = 0;
};

Olcum ozetle(std::vector<std::int64_t>& v)
{
    Olcum o;
    if (v.empty()) {
        return o;
    }
    std::sort(v.begin(), v.end());
    o.sayi = v.size();
    o.p50_ns = static_cast<double>(v[v.size() / 2]);
    o.p99_ns = static_cast<double>(v[v.size() * 99 / 100]);
    o.azami_ns = static_cast<double>(v.back());
    return o;
}

enum class Kosul { saglikli, hic_baglanmayan, okumayan, ortada_olen };

// Bir koşuyu yürütür: dünya iş parçacığı (bu iş parçacığı) `adim_sayisi` adım atar ve her yayın
// çağrısının süresini ölçer; istemci ayrı iş parçacığındadır.
Olcum kos(Denetci& d, Kosul k, const char* ad, SunucuSayaclari& son)
{
    SunucuDuzenegi sd;
    DOGRULA(d, sd.s != nullptr && sd.s->durum() == A2Durumu::dinliyor, std::string(ad) + ": sunucu dinliyor");
    Olcum bos;
    if (sd.s == nullptr) {
        return bos;
    }
    Yayinci& y = sd.s->yayinci();
    sahte::SahteAyar a = ornek_ayar();
    a.hedef_sayisi = 15;
    sahte::SahteDunya dn(y, a);
    dn.basla();

    std::atomic<bool> dur{false};
    std::atomic<bool> oldur{false};
    std::atomic<bool> akista{false};
    std::thread istemci;
    if (k != Kosul::hic_baglanmayan) {
        istemci = std::thread([&] {
            SoketIstemci c(sd.s->port());
            c.saat = [&sd] { return sd.saat(); };
            c.basla();
            while (!dur.load(std::memory_order_relaxed)) {
                if (oldur.load(std::memory_order_relaxed)) {
                    c.kapat();  // süreç öldürülmüş gibi: veda yok, soket kapanır
                    return;
                }
                if (c.ist.asama() == viz::Asama::akis) {
                    akista.store(true, std::memory_order_relaxed);
                    if (k == Kosul::okumayan) {
                        c.oku = false;  // donuk istemci: soketi açık, yalnız damla damla okur
                        c.damla = true;
                    }
                }
                if (!c.pompala()) {
                    gercek_bekle_ms(1);
                }
            }
        });
        DOGRULA(d, kosul_bekle([&] { return akista.load(); }, 5000), std::string(ad) + ": istemci akışta");
    }

    // Dünya: 16 gövde, 12 000 adım, her 100 adımda bir olay; her 16 adımda 1 ms uyur (≈ 16x hız)
    // ki G/Ç iş parçacığı gerçek bir koşudaki gibi araya girsin. Ölçülen yalnız yayın çağrılarıdır.
    const int kAdim = 12'000;
    std::vector<std::int64_t> sure;
    sure.reserve(static_cast<std::size_t>(kAdim) + 256);
    std::array<GovdeAni, kAzamiGovde> an{};
    using Saat = std::chrono::steady_clock;
    for (int n = 1; n <= kAdim; ++n) {
        const std::uint64_t adim = static_cast<std::uint64_t>(n);
        const std::size_t m = dn.dogru_anlar(adim, an);
        if (k == Kosul::ortada_olen && n == kAdim / 3) {
            oldur.store(true);
        }
        const Saat::time_point t0 = Saat::now();
        if (n % 100 == 0) {
            y.isabet(adim, 1, 2, {0, 0, 0}, {1, 0, 0}, {0, 0, 0});
        }
        y.adim_siniri(adim, std::span<const GovdeAni>(an.data(), m));
        const Saat::time_point t1 = Saat::now();
        sure.push_back(std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count());
        if (n % 64 == 0) {
            gercek_bekle_ms(1);
        }
    }
    gercek_bekle_ms(50);  // G/Ç iş parçacığı son kayıtları işlesin (sayaçlar için)
    son = sd.s->sayaclar();
    dur.store(true);
    if (istemci.joinable()) {
        istemci.join();
    }
    const Olcum o = ozetle(sure);
    std::printf("  ölçüm (bu Mac) [%-16s] yayın çağrısı: p50 %.0f ns, p99 %.0f ns, en kötü %.0f ns (%llu çağrı)\n", ad, o.p50_ns,
                o.p99_ns, o.azami_ns, static_cast<unsigned long long>(o.sayi));
    DOGRULA(d, son.yayin.gecersiz_cagri == 0, std::string(ad) + ": yayın çağrıları kabul edildi");
    return o;
}

}  // namespace

// Yayın çağrısının süresi: okumayan, hiç bağlanmayan ve ortada ölen istemciyle, bağlı ve sağlıklı
// istemcili koşudan ayırt edilemez.
int test_yayin_suresi()
{
    Denetci d;
    SunucuSayaclari s_saglikli, s_yok, s_okumayan, s_olen;
    const Olcum saglikli = kos(d, Kosul::saglikli, "sağlıklı", s_saglikli);
    const Olcum yok = kos(d, Kosul::hic_baglanmayan, "hiç bağlanmayan", s_yok);
    const Olcum okumayan = kos(d, Kosul::okumayan, "okumayan", s_okumayan);
    const Olcum olen = kos(d, Kosul::ortada_olen, "ortada ölen", s_olen);

    // İşlevsel (her ortamda): sağlıklı koşuda durum ve olaylar gitti; ötekilerde sunucu doğru durumda.
    DOGRULA(d, s_saglikli.gonderilen_durum >= 800 && s_saglikli.gonderilen_olay == 120 && s_saglikli.oturum_var,
            "sağlıklı: durum ve olaylar gönderildi: " + std::to_string(s_saglikli.gonderilen_durum));
    DOGRULA(d, s_yok.baglanti_sayisi == 0 && s_yok.gonderilen_ileti == 0, "hiç bağlanmayan: hiçbir şey gönderilmedi");
    DOGRULA(d, s_okumayan.oturum_var && s_okumayan.azami_kuyruk_bayt > 65'536,
            "okumayan: oturum açık, kuyruk birikti: " + std::to_string(s_okumayan.azami_kuyruk_bayt));
    DOGRULA(d, !s_olen.oturum_var && s_olen.son_kopma_nedeni == KopmaNedeni::karsi_kapatti && s_olen.biten_oturum == 1,
            "ortada ölen: oturum bitti (A2-K64), sunucu yeni bağlantıya hazır");

    // Zamanlama (ortak yardımcı; W4-K24, W4-K29). "Ayırt edilemez": bozuk koşulların p99'u sağlıklı
    // koşunun p99'unun 4 katını ya da +5 µs'yi aşmaz; en kötü çağrı 50 ms'nin altındadır (okumayan bir
    // istemciye engelleyen gönderim sınırsız beklerdi; 50 ms yalnız işletim sisteminin iş parçacığını
    // kesmesine pay bırakır: aynı makinede başka derlemeler koşabilir).
    const double p99_siniri = std::max(4.0 * saglikli.p99_ns, saglikli.p99_ns + 5'000.0);
    const auto zaman = [&](const Olcum& o, const char* ad) {
        char olcum[200];
        std::snprintf(olcum, sizeof olcum, "p99 %.0f ns (sınır %.0f ns), en kötü %.0f ns (sınır 50 ms)", o.p99_ns, p99_siniri,
                      o.azami_ns);
        DOGRULA(d, ida::test::zamanlama(o.p99_ns <= p99_siniri && o.azami_ns < 50e6, std::string("yayın süresi: ") + ad, olcum),
                std::string("yayın süresi sağlıklı koşudan ayırt edilemez: ") + ad);
    };
    zaman(yok, "hiç bağlanmayan istemci");
    zaman(okumayan, "okumayan istemci");
    zaman(olen, "ortada ölen istemci");
    char olcum[160];
    std::snprintf(olcum, sizeof olcum, "p99 %.0f ns, en kötü %.0f ns", saglikli.p99_ns, saglikli.azami_ns);
    DOGRULA(d, ida::test::zamanlama(saglikli.p99_ns < 100'000.0 && saglikli.azami_ns < 50e6, "yayın süresi: sağlıklı istemci", olcum),
            "sağlıklı koşuda yayın çağrısı kısa");
    return d.hata;
}

// Okumayan gerçek istemci, gerçek 8 MiB sınır: kuyruk dolar, durum atılır, veda(tampon_tasti);
// dünya bu sırada hiçbir çağrıda beklemez.
int test_donuk_istemci_soket()
{
    Denetci d;
    SunucuDuzenegi sd;
    DOGRULA(d, sd.s != nullptr && sd.s->durum() == A2Durumu::dinliyor, "sunucu dinliyor");
    if (sd.s == nullptr) {
        return d.hata + 1;
    }
    Yayinci& y = sd.s->yayinci();
    sahte::SahteAyar a = ornek_ayar();
    a.hedef_sayisi = 62;  // 63 gövde: durum ≈ 7,6 KiB
    sahte::SahteDunya dn(y, a);
    dn.basla();
    SoketIstemci c(sd.s->port());
    c.saat = [&sd] { return sd.saat(); };
    c.basla();
    DOGRULA(d, c.bekle([&] { return c.ist.asama() == viz::Asama::akis && sd.s->sayaclar().hazir_alindi; }), "istemci akışta");
    c.oku = false;  // istemci donar (soket açık); nabız sürer, yalnız damla damla okur (3 s kuralı)
    c.damla = true;

    // Atılamayan büyük iletiler (malzeme_optik: 256 girdi ≈ 14,3 KiB) ve takvimli durumlar.
    std::array<MalzemeOptikGirdisi, kAzamiMalzemeOptik> girdi{};
    for (std::size_t i = 0; i < girdi.size(); ++i) {
        girdi[i].malzeme_indeksi = static_cast<std::uint16_t>(i);
        girdi[i].maske = 0x4;
        girdi[i].puruzluluk = 0.5;
    }
    std::array<GovdeAni, kAzamiGovde> an{};
    using Saat = std::chrono::steady_clock;
    std::int64_t azami_cagri_ns = 0;
    std::uint64_t adim = 0;
    const Saat::time_point bitis = Saat::now() + std::chrono::seconds(20);
    while (sd.s->sayaclar().kuyruk_tasmasi == 0 && Saat::now() < bitis) {
        (void)c.pompala();  // nabız ve damla okuma: yavaş makinede sunucunun 3 s kuralı oturumu öldürmesin
        for (int i = 0; i < 13; ++i) {
            ++adim;
            const std::size_t m = dn.dogru_anlar(adim, an);
            const Saat::time_point t0 = Saat::now();
            if (i == 0) {
                y.malzeme_optik(girdi);
            }
            y.adim_siniri(adim, std::span<const GovdeAni>(an.data(), m));
            azami_cagri_ns = std::max<std::int64_t>(
                azami_cagri_ns, std::chrono::duration_cast<std::chrono::nanoseconds>(Saat::now() - t0).count());
        }
        // G/Ç iş parçacığı yetişsin (yayın halkası taşmasın): sınanan şey gönderme kuyruğudur.
        if (adim % 130 == 0) {
            gercek_bekle_ms(2);
        }
    }
    kosul_bekle([&] { return !sd.s->sayaclar().oturum_var; }, 2000);  // sayaçlar G/Ç iş parçacığında güncellenir
    const SunucuSayaclari s = sd.s->sayaclar();
    DOGRULA(d, s.kuyruk_tasmasi == 1 && s.son_kopma_nedeni == KopmaNedeni::tampon_tasti, "veda(tampon_tasti): kuyruk doldu");
    DOGRULA(d, s.kuyrukta_atilan_durum > 0, "önce takvimli durumlar atıldı: " + std::to_string(s.kuyrukta_atilan_durum));
    DOGRULA(d, s.azami_kuyruk_bayt <= kGondermeKuyruguAzamiBayt, "bellek tavanı: kuyruk ≤ 8 MiB: " + std::to_string(s.azami_kuyruk_bayt));
    DOGRULA(d, s.azami_kuyruk_bayt > kGondermeKuyruguAzamiBayt / 2, "kuyruk gerçekten sınıra yaklaştı");
    DOGRULA(d, s.yayin.kayip_korunan == 0 && s.yayin_tasmasi_kapanis == 0, "neden gönderme kuyruğu (yayın halkası değil)");
    DOGRULA(d, !s.oturum_var, "oturum kapandı");
    char olcum[120];
    std::snprintf(olcum, sizeof olcum, "en kötü yayın çağrısı %.1f µs (sınır 50 ms)", static_cast<double>(azami_cagri_ns) / 1e3);
    DOGRULA(d, ida::test::zamanlama(azami_cagri_ns < 50'000'000, "donuk istemciyle yayın çağrısı", olcum),
            "donuk istemci dünyayı bekletmedi");
    std::printf("  ölçüm (bu Mac): kuyruk en çok %llu bayt; atılan durum %llu; %llu adım\n",
                static_cast<unsigned long long>(s.azami_kuyruk_bayt), static_cast<unsigned long long>(s.kuyrukta_atilan_durum),
                static_cast<unsigned long long>(adim));
    // İstemci yeniden okumaya başlar: akış düzgün çerçevelerle biter, son ileti veda(tampon_tasti).
    c.oku = true;
    DOGRULA(d, c.bekle([&] { return c.karsi_kapatti; }, 10000), "sunucu bağlantıyı kapattı");
    DOGRULA(d, !c.k.cerceve_hatasi, "çerçeve bütünlüğü korundu");
    DOGRULA(d, c.k.son_veda(VedaNedeni::tampon_tasti), "son ileti veda(tampon_tasti)");
    // Sunucu yeni bağlantıya hazır.
    SoketIstemci c2(sd.s->port());
    c2.saat = [&sd] { return sd.saat(); };
    c2.basla();
    DOGRULA(d, c2.bekle([&] { return c2.ist.asama() == viz::Asama::akis; }), "yeni istemci akışta");
    return d.hata;
}

}  // namespace ida::a2::test
