// test_protokol.cpp — kabul 3: protokol hataları ve el sıkışma (A2-U02, U03, U04'ün soketsiz kısmı).
// Süreler sanal saatle sınanır (gerçek 5 s / 120 s / 3 s beklenmez).
#include "test_destek.hpp"

#include <cstring>

namespace ida::a2::test {

namespace {

viz::Baytlar merhaba(std::uint32_t sira = 0, std::uint16_t ana = 1, std::uint16_t alt = 0)
{
    Merhaba m{};
    m.surum_ana = ana;
    m.surum_alt = alt;
    m.platform = 1;
    m.cizici = 1;
    m.pencere_cizilebilir = 1;
    m.surec_kimligi = 7;
    viz::metin_yaz(m.godot_surumu, sizeof m.godot_surumu, "4.7.2");
    viz::metin_yaz(m.uzanti_surumu, sizeof m.uzanti_surumu, "sinama");
    viz::metin_yaz(m.gpu_adi, sizeof m.gpu_adi, "yok");
    viz::Baytlar b;
    viz::kodla_merhaba(b, sira, m);
    return b;
}

viz::Baytlar hazir(std::uint32_t sira, std::uint32_t durum = 0, std::uint8_t ozet_tohumu = 0x5A)
{
    Hazir h{};
    const auto o = ornek_ozet(ozet_tohumu);
    std::memcpy(h.manifest_sha256, o.data(), 32);
    h.dalga_kumesi_surumu = 1;
    h.ortam_surumu = 1;
    h.durum = durum;
    viz::Baytlar b;
    viz::kodla_hazir(b, sira, h);
    return b;
}

viz::Baytlar nabiz(std::uint32_t sira)
{
    GoruntuleyiciNabzi n{};
    n.gonderim_duvar_ns = 1;
    n.cizim_hizi_mhz = 60'000;
    viz::Baytlar b;
    viz::kodla_goruntuleyici_nabzi(b, sira, n);
    return b;
}

// Koşan bir dünya + çekirdek; istenen aşamaya kadar ham iletilerle getirilir.
struct Ham {
    Duzenek z;
    sahte::SahteDunya dn;
    std::uint32_t sira = 0;   // sıradaki istemci sırası
    Ham() : dn(z.y, ornek_ayar())
    {
        dn.basla();
        dn.adimla(20);
        z.ham_baglan();
    }
    void merhaba_gonder() { z.ham_gonder(merhaba(sira++)); }
    void hazir_gonder() { z.ham_gonder(hazir(sira++)); }
    void akisa_getir()
    {
        merhaba_gonder();
        hazir_gonder();
    }
};

// Beklenen: son çerçeve veda(protokol_hatasi), oturum bitti, sayaç 1, sonraki baytlar yok sayılır.
void protokol_hatasi_bekle(Denetci& d, Ham& h, const char* ad)
{
    DOGRULA(d, h.z.k.son_veda(VedaNedeni::protokol_hatasi), std::string(ad) + ": veda(protokol_hatasi)");
    DOGRULA(d, !h.z.c.oturum_var(), std::string(ad) + ": oturum kapandı");
    const SunucuSayaclari s = h.z.c.sayaclar();
    DOGRULA(d, s.protokol_hatasi == 1 && s.son_kopma_nedeni == KopmaNedeni::protokol_hatasi && s.biten_oturum == 1,
            std::string(ad) + ": sayaçlar");
    DOGRULA(d, !h.z.k.cerceve_hatasi, std::string(ad) + ": sunucu çıktısı düzgün");
    // A2-K67: vedadan sonra gelenler yok sayılır; yeni çerçeve üretilmez.
    const std::size_t n = h.z.k.cerceveler.size();
    h.z.ham_gonder(nabiz(99));
    h.z.pompala();
    DOGRULA(d, h.z.k.cerceveler.size() == n && h.z.c.sayaclar().protokol_hatasi == 1, std::string(ad) + ": sonrası yok sayıldı");
    // Sunucu yeni bağlantıya hazır (A2-K64).
    DOGRULA(d, h.z.ham_baglan(), std::string(ad) + ": yeni bağlantı kabul edilir");
}

}  // namespace

int test_protokol_baslik_hatalari()
{
    Denetci d;
    {  // bozuk sihirli sayı (ilk ileti)
        Ham h;
        viz::Baytlar b = merhaba();
        b[1] ^= 0x40u;
        h.z.ham_gonder(b);
        protokol_hatasi_bekle(d, h, "bozuk sihirli sayı");
        DOGRULA(d, h.z.k.cerceveler.size() == 1 && h.z.k.cerceveler[0].b.sira == 0, "veda oturumun ilk iletisi: sıra 0");
    }
    {  // bilinmeyen tür
        Ham h;
        h.akisa_getir();
        viz::Baytlar b = nabiz(h.sira);
        b[4] = 0x77;
        b[5] = 0x77;
        h.z.ham_gonder(b);
        protokol_hatasi_bekle(d, h, "bilinmeyen tür");
    }
    {  // sim → görselleştirici türü ters yönde
        Ham h;
        h.akisa_getir();
        viz::Baytlar b;
        viz::kodla_sim_nabzi(b, h.sira, SimNabzi{});
        h.z.ham_gonder(b);
        protokol_hatasi_bekle(d, h, "ters yönde sim_nabzi");
    }
    {  // yanlış uzunluk: hata yük beklenmeden bulunur (yalnız başlık gönderilir)
        Ham h;
        h.akisa_getir();
        viz::Baytlar b = nabiz(h.sira);
        b[8] = 47;  // 48 yerine
        b.resize(16);
        h.z.ham_gonder(b);
        protokol_hatasi_bekle(d, h, "yanlış uzunluk");
    }
    {  // 1 MiB'den büyük yük
        Ham h;
        viz::Baytlar b = merhaba();
        b[8] = 0x01;
        b[9] = 0x00;
        b[10] = 0x10;
        b[11] = 0x00;  // 1 048 577
        b.resize(16);
        h.z.ham_gonder(b);
        protokol_hatasi_bekle(d, h, "yük 1 MiB'den büyük");
    }
    {  // sıfırdan farklı bayrak
        Ham h;
        h.akisa_getir();
        viz::Baytlar b = nabiz(h.sira);
        b[6] = 0x01;
        h.z.ham_gonder(b);
        protokol_hatasi_bekle(d, h, "bayraklar ≠ 0");
    }
    return d.hata;
}

int test_protokol_sira()
{
    Denetci d;
    {  // ilk iletinin sırası 0 değil
        Ham h;
        h.z.ham_gonder(merhaba(5));
        protokol_hatasi_bekle(d, h, "ilk sıra ≠ 0");
    }
    {  // yinelenen sıra
        Ham h;
        h.akisa_getir();
        h.z.ham_gonder(nabiz(h.sira));
        DOGRULA(d, h.z.c.oturum_var(), "geçerli nabız kabul edildi");
        h.z.ham_gonder(nabiz(h.sira));
        protokol_hatasi_bekle(d, h, "yinelenen sıra");
    }
    {  // azalan sıra
        Ham h;
        h.akisa_getir();
        h.z.ham_gonder(nabiz(h.sira++));
        h.z.ham_gonder(nabiz(h.sira++));
        h.z.ham_gonder(nabiz(1));
        protokol_hatasi_bekle(d, h, "azalan sıra");
    }
    {  // boşluklu sıra (görselleştirici → sim yönünde boşluk da hatadır: A2-K07)
        Ham h;
        h.akisa_getir();
        h.z.ham_gonder(nabiz(h.sira + 1));
        protokol_hatasi_bekle(d, h, "boşluklu sıra");
    }
    {  // geçerli uzun dizi: hata yok
        Ham h;
        h.akisa_getir();
        for (int i = 0; i < 1000; ++i) {
            h.z.ham_gonder(nabiz(h.sira++));
        }
        DOGRULA(d, h.z.c.oturum_var() && h.z.c.sayaclar().protokol_hatasi == 0 && h.z.c.sayaclar().alinan_nabiz == 1000,
                "1000 nabız, hata yok");
    }
    return d.hata;
}

int test_protokol_ileti_sirasi()
{
    Denetci d;
    {  // ilk ileti merhaba değil (A2-K10)
        Ham h;
        h.z.ham_gonder(nabiz(0));
        protokol_hatasi_bekle(d, h, "ilk ileti merhaba değil");
    }
    {  // ikinci merhaba
        Ham h;
        h.merhaba_gonder();
        h.z.ham_gonder(merhaba(h.sira));
        protokol_hatasi_bekle(d, h, "ikinci merhaba");
    }
    {  // ikinci hazir
        Ham h;
        h.akisa_getir();
        DOGRULA(d, h.z.c.asama() == ic::Asama::akis, "akışta");
        h.z.ham_gonder(hazir(h.sira));
        protokol_hatasi_bekle(d, h, "ikinci hazir");
    }
    {  // hazir.durum aralık dışı (§3.3: 0–7)
        Ham h;
        h.merhaba_gonder();
        h.z.ham_gonder(hazir(h.sira, 8));
        protokol_hatasi_bekle(d, h, "hazir.durum = 8");
    }
    {  // veda nedeni aralık dışı (§3.3: 0–5)
        Ham h;
        h.akisa_getir();
        viz::Baytlar b;
        viz::kodla_veda(b, h.sira, VedaNedeni::olagan);
        b[16] = 9;
        h.z.ham_gonder(b);
        protokol_hatasi_bekle(d, h, "veda nedeni 9");
    }
    // W5-K30: bu dilimde kamera isteği gönderilmez; istenmemiş yanıtlar protokol hatasıdır.
    {
        Ham h;
        h.akisa_getir();
        viz::Baytlar b;
        viz::kodla_kare_hazir(b, h.sira, KareHazir{});
        h.z.ham_gonder(b);
        protokol_hatasi_bekle(d, h, "kare_hazir");
    }
    {
        Ham h;
        h.akisa_getir();
        viz::Baytlar b;
        viz::kodla_kare_dustu(b, h.sira, KareDustu{});
        h.z.ham_gonder(b);
        protokol_hatasi_bekle(d, h, "kare_dustu");
    }
    {
        Ham h;
        h.akisa_getir();
        viz::Baytlar b;
        viz::kodla_halka_eslendi(b, h.sira, HalkaEslendi{});
        h.z.ham_gonder(b);
        protokol_hatasi_bekle(d, h, "halka_eslendi");
    }
    {  // olağan veda: hata değil
        Ham h;
        h.akisa_getir();
        const std::size_t n = h.z.k.cerceveler.size();
        viz::Baytlar b;
        viz::kodla_veda(b, h.sira, VedaNedeni::olagan);
        h.z.ham_gonder(b);
        const SunucuSayaclari s = h.z.c.sayaclar();
        DOGRULA(d, !h.z.c.oturum_var() && s.protokol_hatasi == 0 && s.son_kopma_nedeni == KopmaNedeni::karsi_veda &&
                       h.z.k.cerceveler.size() == n,
                "veda(olagan): oturum biter, yanıt gönderilmez, hata sayılmaz");
        DOGRULA(d, s.son_karsi_veda_nedeni == 0, "karşı tarafın veda nedeni sayaçlarda (0: olağan)");
        DOGRULA(d, h.z.ham_baglan(), "yeni bağlantı kabul edilir");
    }
    return d.hata;
}

// A2-K11, K14, §3.3: ret(surum | protokol | manifest | halka) ve oturum alt sürümü.
int test_el_sikisma_retleri()
{
    Denetci d;
    {  // ana sürüm farkı → ret(surum); öteki alanlara bakılmaz
        Ham h;
        viz::Baytlar b = merhaba(0, 2, 0);
        b[16 + 8] = 99;  // platform da geçersiz: sürüm önce değerlendirilir
        h.z.ham_gonder(b);
        DOGRULA(d, h.z.k.son_ret(RetNedeni::surum) && !h.z.c.oturum_var(), "ret(surum)");
        const SunucuSayaclari s = h.z.c.sayaclar();
        DOGRULA(d, s.son_kopma_nedeni == KopmaNedeni::surum_reddi && s.protokol_hatasi == 0, "sürüm reddi sayaçları");
        DOGRULA(d, h.z.k.cerceveler.size() == 1 && h.z.k.cerceveler[0].b.sira == 0, "ret tek ve ilk ileti");
    }
    {  // platform geçersiz → ret(protokol) (§3.3)
        Ham h;
        viz::Baytlar b = merhaba();
        b[16 + 8] = 3;
        h.z.ham_gonder(b);
        DOGRULA(d, h.z.k.son_ret(RetNedeni::protokol) && !h.z.c.oturum_var(), "platform 3 → ret(protokol)");
        DOGRULA(d, h.z.c.sayaclar().son_kopma_nedeni == KopmaNedeni::protokol_reddi && h.z.c.sayaclar().protokol_hatasi == 1,
                "protokol reddi sayaçları");
    }
    {  // çizici geçersiz
        Ham h;
        viz::Baytlar b = merhaba();
        b[16 + 9] = 2;
        h.z.ham_gonder(b);
        DOGRULA(d, h.z.k.son_ret(RetNedeni::protokol), "çizici 2 → ret(protokol)");
    }
    {  // NUL'suz metin (§3.3: ret(protokol))
        Ham h;
        viz::Baytlar b = merhaba();
        std::memset(b.data() + 16 + 80, 'g', 64);  // gpu_adi: NUL yok
        h.z.ham_gonder(b);
        DOGRULA(d, h.z.k.son_ret(RetNedeni::protokol), "NUL'suz gpu_adi → ret(protokol)");
    }
    {  // bilinmeyen yetenek biti yok sayılır; pencere_cizilebilir geçersizse 0 sayılır
        Ham h;
        viz::Baytlar b = merhaba();
        b[16 + 4] = 0xFE;
        b[16 + 10] = 7;
        h.z.ham_gonder(b);
        DOGRULA(d, h.z.c.asama() == ic::Asama::hazir_bekleniyor && h.z.k.say(IletiTuru::hosgeldin) == 1,
                "bilinmeyen yetenek bitleriyle el sıkışma sürer");
    }
    {  // oturum alt sürümü = en küçüğü (A2-K11): görselleştirici alt 7, sim alt 0 → 0
        Ham h;
        h.z.ham_gonder(merhaba(0, 1, 7));
        const Cerceve* c = h.z.k.ilk(IletiTuru::hosgeldin);
        DOGRULA(d, c != nullptr && hosgeldin_coz(*c).h.surum_alt == 0 && hosgeldin_coz(*c).h.surum_ana == 1,
                "oturum alt sürümü min(sim, görselleştirici)");
        DOGRULA(d, h.z.c.sayaclar().oturum_surum_alt == 0, "sayaçta oturum alt sürümü");
    }
    {  // yanlış manifest özeti → ret(manifest) (gerçek istemciyle; A2-K16: yeniden bağlanmaz)
        Duzenek z;
        sahte::SahteDunya dn(z.y, ornek_ayar());
        dn.basla();
        z.hazir_ozeti[5] ^= 0x01u;
        DOGRULA(d, z.baglan(), "bağlandı");
        z.pompala();
        DOGRULA(d, z.k.son_ret(RetNedeni::manifest), "yanlış özet → ret(manifest)");
        DOGRULA(d, !z.c.oturum_var() && z.c.sayaclar().son_kopma_nedeni == KopmaNedeni::manifest_reddi, "oturum kapandı");
        DOGRULA(d, !z.ist.yeniden_baglanabilir(), "istemci ret(manifest)'ten sonra kendiliğinden yeniden bağlanmaz");
        DOGRULA(d, z.k.say(IletiTuru::kamera_istegi) == 0, "hiç kamera_istegi yok");
    }
    {  // hazir.durum ≠ 0 → ret(manifest)
        Duzenek z;
        sahte::SahteDunya dn(z.y, ornek_ayar());
        dn.basla();
        z.hazir_durumu = HazirDurumu::varlik_eksik;
        DOGRULA(d, z.baglan(), "bağlandı");
        z.pompala();
        DOGRULA(d, z.k.son_ret(RetNedeni::manifest), "hazir.durum = 3 → ret(manifest)");
    }
    for (std::uint32_t kod = 1; kod <= 7; ++kod) {  // H04, H05: 1–5 → manifest, 6–7 → halka
        Ham h;
        h.merhaba_gonder();
        h.z.ham_gonder(hazir(h.sira, kod));
        DOGRULA(d, kod >= 6 ? h.z.k.son_ret(RetNedeni::halka) : h.z.k.son_ret(RetNedeni::manifest),
                "hazir.durum = " + std::to_string(kod));
        DOGRULA(d, !h.z.c.oturum_var(), "oturum kapandı");
    }
    {  // doğru özet ve durum 0 → akış; ret yok
        Ham h;
        h.akisa_getir();
        DOGRULA(d, h.z.c.asama() == ic::Asama::akis && h.z.k.say(IletiTuru::ret) == 0, "başarılı hazir");
    }
    {  // hazir, hosgeldin'den önce (çalıştırma tanımı yokken) → protokol hatası
        Duzenek z;
        z.ham_baglan();
        z.ham_gonder(merhaba(0));
        DOGRULA(d, z.c.asama() == ic::Asama::tam_durum_bekleniyor && z.k.cerceveler.empty(),
                "tanım yokken hosgeldin gönderilmez");
        z.ham_gonder(hazir(1));
        DOGRULA(d, z.k.son_veda(VedaNedeni::protokol_hatasi), "hosgeldin'den önce hazir → protokol hatası");
    }
    return d.hata;
}

// A2-K10, K15, K62 ve W5-K28: süreler (sanal saat).
int test_zaman_asimlari()
{
    Denetci d;
    {  // merhaba 5 s: ileti gönderilmeden kapatılır
        Ham h;
        h.z.ilerlet(4'900'000'000);
        DOGRULA(d, h.z.c.oturum_var(), "4,9 s: bağlantı açık");
        h.z.ilerlet(200'000'000);
        const SunucuSayaclari s = h.z.c.sayaclar();
        DOGRULA(d, !h.z.c.oturum_var() && s.son_kopma_nedeni == KopmaNedeni::merhaba_zaman_asimi, "5 s'de kapandı");
        DOGRULA(d, h.z.k.cerceveler.empty(), "merhaba zaman aşımında ileti gönderilmez (A2-K10)");
        DOGRULA(d, h.z.ham_baglan(), "yeni bağlantı kabul edilir");
    }
    {  // hazir 120 s: istemci nabız gönderirken (W5-K28) pencere dolunca ret(zaman_asimi)
        Duzenek z;
        sahte::SahteDunya dn(z.y, ornek_ayar());
        dn.basla();
        z.otomatik_hazir = false;
        DOGRULA(d, z.baglan(), "bağlandı");
        DOGRULA(d, z.c.asama() == ic::Asama::hazir_bekleniyor, "hazir bekleniyor");
        for (int i = 0; i < 1190; ++i) {  // 119 s; dünya da ilerler
            dn.adimla(80);
            z.ilerlet(100'000'000);
        }
        DOGRULA(d, z.c.oturum_var() && z.c.asama() == ic::Asama::hazir_bekleniyor, "119 s: oturum açık");
        const std::size_t nabiz_sayisi = z.k.say(IletiTuru::sim_nabzi);
        DOGRULA(d, nabiz_sayisi >= 118 && nabiz_sayisi <= 119, "W5-K28: hazir penceresinde sim_nabzi 1 s aralıkla: " +
                                                                 std::to_string(nabiz_sayisi));
        DOGRULA(d, z.c.sayaclar().alinan_nabiz >= 118, "W5-K28: istemci hazir'dan önce de nabız gönderdi");
        DOGRULA(d, z.k.say(IletiTuru::durum) > 7000, "A2-K12: akış hazir'dan önce başlar");
        DOGRULA(d, z.ist.asama() == viz::Asama::uygulama_bekleniyor && z.istemci_hatasiz(), "istemci beklemede, hatasız");
        z.ilerlet(1'200'000'000);
        DOGRULA(d, z.k.son_ret(RetNedeni::zaman_asimi), "120 s'de ret(zaman_asimi)");
        DOGRULA(d, !z.c.oturum_var() && z.c.sayaclar().son_kopma_nedeni == KopmaNedeni::hazir_zaman_asimi, "oturum kapandı");
        DOGRULA(d, z.ist.yeniden_baglanabilir(), "ret(zaman_asimi)'ndan sonra istemci yeniden bağlanabilir");
    }
    {  // W5-K28: hazir penceresinde de 3 s sessizlik kapatır
        Duzenek z;
        sahte::SahteDunya dn(z.y, ornek_ayar());
        dn.basla();
        z.otomatik_hazir = false;
        DOGRULA(d, z.baglan(), "bağlandı");
        z.ilerlet(2'900'000'000, 100'000'000, false);  // istemci susar
        DOGRULA(d, z.c.oturum_var(), "2,9 s: oturum açık");
        z.ilerlet(200'000'000, 100'000'000, false);
        DOGRULA(d, z.k.son_veda(VedaNedeni::nabiz_zaman_asimi), "hazir penceresinde 3 s sessizlik → veda(nabiz_zaman_asimi)");
        DOGRULA(d, z.c.sayaclar().son_kopma_nedeni == KopmaNedeni::nabiz_zaman_asimi, "kopma nedeni");
    }
    {  // çalıştırma tanımı yokken: hosgeldin gönderilmez; tanım gelince el sıkışma sürer
        Duzenek z;
        DOGRULA(d, z.baglan(), "bağlandı (tanım yok)");
        z.ilerlet(1'500'000'000);
        DOGRULA(d, z.c.asama() == ic::Asama::tam_durum_bekleniyor && z.k.cerceveler.empty(), "tanım yok: sunucu sessiz");
        DOGRULA(d, z.c.sayaclar().tam_durum_bekleme > 0, "bekleme sayıldı");
        sahte::SahteDunya dn(z.y, ornek_ayar());
        dn.basla();
        z.pompala();
        z.pompala();
        DOGRULA(d, z.c.asama() == ic::Asama::akis && z.ist.asama() == viz::Asama::akis, "tanım gelince el sıkışma tamamlandı");
        DOGRULA(d, z.istemci_hatasiz(), "istemci hatasız: " + z.istemci_son_nedeni());
    }
    return d.hata;
}

}  // namespace ida::a2::test
