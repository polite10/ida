// test_takvim.cpp — kabul 4: 60 Hz takvim (A2-K20) ve SIL seyreltmesi (A2-K25).
#include "takvim.hpp"
#include "test_destek.hpp"

#include <algorithm>

namespace ida::a2::test {

// Bir saatlik adım dizisi: takvimli iletiler arası 13 ya da 14 adım, ideal 60 Hz ızgarasından
// sapma ≤ 0,625 ms, kayma yok (k'inci ileti her zaman takvimin k'inci adımında).
int test_takvim_bir_saat()
{
    Denetci d;
    Duzenek z;
    sahte::SahteAyar a = ornek_ayar();
    a.sim_kipi = SimKipi::hil_1x;  // seyreltme yok
    sahte::SahteDunya dn(z.y, a);
    DOGRULA(d, dn.basla(), "çalıştırma başladı");
    z.k.sakla = false;
    std::uint64_t k = 0;
    std::uint64_t onceki = 0;
    std::uint64_t durum_sayisi = 0;
    std::int64_t azami_sapma3 = 0;  // 3 ile çarpılmış ns (tamsayı karşılaştırma)
    int hata = 0;
    z.k.gozlemci = [&](const Cerceve& c) {
        if (c.tur() != IletiTuru::durum) {
            return;
        }
        const viz::DurumIleti m = durum_coz(c);
        ++durum_sayisi;
        if (m.d.adim_no != ic::takvim_adimi(k) || m.d.bayraklar != 0) {
            ++hata;  // kayma ya da takvim dışı ileti
        }
        if (k > 0 && m.d.adim_no - onceki != 13 && m.d.adim_no - onceki != 14) {
            ++hata;
        }
        std::int64_t sapma3 = 3 * m.d.t_ns - static_cast<std::int64_t>(k) * 50'000'000;  // 3·(t − k/60 s)
        if (sapma3 < 0) {
            sapma3 = -sapma3;
        }
        azami_sapma3 = std::max(azami_sapma3, sapma3);
        onceki = m.d.adim_no;
        ++k;
    };
    DOGRULA(d, z.baglan(), "bağlandı");
    const std::uint64_t kSaat = 3600ull * 800ull;  // 2 880 000 adım
    for (std::uint64_t i = 0; i < kSaat / 400; ++i) {
        dn.adimla(400);
        z.ilerlet(500'000'000, 500'000'000);
        if (i % 16 == 0) {
            // İstemcinin gövde geçmişi sınırlı kalsın (uygulama her karede budar).
            z.ist.sahne().buda(z.ist.saat().t_son(), [](std::uint32_t) {});
        }
    }
    DOGRULA(d, hata == 0, "takvim: 13/14 adım aralığı, kayma yok, takvim dışı ileti yok; ihlal " + std::to_string(hata));
    DOGRULA(d, durum_sayisi == 216'001, "bir saatte 216 001 durum (adım 0 dahil): " + std::to_string(durum_sayisi));
    DOGRULA(d, onceki == kSaat, "son ileti tam bir saatte (kayma yok): adım " + std::to_string(onceki));
    DOGRULA(d, azami_sapma3 <= 3 * 625'000, "ideal ızgaradan sapma ≤ 0,625 ms");
    std::printf("  ölçüm: 216 001 ileti; en büyük sapma %.1f ns (sınır 625 000 ns)\n", static_cast<double>(azami_sapma3) / 3.0);
    const SunucuSayaclari s = z.c.sayaclar();
    DOGRULA(d, s.seyreltilen_durum == 0 && s.kuyrukta_atilan_durum == 0 && s.yayin.dusen_durum == 0 && s.protokol_hatasi == 0,
            "hiçbir durum atılmadı");
    DOGRULA(d, z.ist.sayaclar().sira_boslugu == 0 && z.istemci_hatasiz(), "istemci boşluk ve hata görmedi");
    DOGRULA(d, s.gonderilen_nabiz >= 3598 && s.gonderilen_nabiz <= 3600, "bir saatte ≈ 3600 sim_nabzi: " +
                                                                           std::to_string(s.gonderilen_nabiz));
    return d.hata;
}

// SIL: takvimli durum duvar saatinin her saniyesinde en çok 120; takvim dışı iletiler seyreltilmez.
int test_seyreltme_sil()
{
    Denetci d;
    {  // 10x hız: adım başına duvar saati 0,125 ms → 600 takvimli durum/s üretilir
        Duzenek z;
        sahte::SahteAyar a = ornek_ayar();
        a.sim_kipi = SimKipi::sil;
        a.firlatma_adimi = 40'001;   // seyreltme sürerken bir süreklilik sınırı (T − h = 40 000 takvimde)
        a.isabet_adimi = 52'345;     // ve takvimde olmayan bir sınır
        sahte::SahteDunya dn(z.y, a);
        DOGRULA(d, dn.basla(), "çalıştırma başladı");
        std::vector<std::int64_t> takvimli;  // gönderim anları (duvar saati)
        std::vector<std::uint64_t> takvim_disi_adimlari;
        std::uint64_t olay = 0;
        std::uint64_t sinir_takvimde = 0;
        z.k.sakla = false;
        z.k.gozlemci = [&](const Cerceve& c) {
            if (c.tur() == IletiTuru::sahne_olayi) {
                ++olay;
            }
            if (c.tur() != IletiTuru::durum) {
                return;
            }
            const viz::DurumIleti m = durum_coz(c);
            if ((m.d.bayraklar & kDurumTakvimDisi) != 0) {
                takvim_disi_adimlari.push_back(m.d.adim_no);
            } else if (m.d.adim_no == 40'000) {
                ++sinir_takvimde;  // süreklilik sınırının T − h iletisi (takvim adımına denk gelir; seyreltilmez)
            } else {
                takvimli.push_back(m.d.gonderim_duvar_ns);
            }
        };
        DOGRULA(d, z.baglan(), "bağlandı");
        const int kTur = 4000;  // 4000 × 5 ms = 20 s duvar saati; 4000 × 40 = 160 000 adım
        for (int i = 0; i < kTur; ++i) {
            dn.adimla(40);
            z.ilerlet(5'000'000, 5'000'000);
        }
        // Her 1 s'lik (yarı açık) pencere: [t, t + 1 s) içinde en çok 120 takvimli durum.
        std::size_t azami = 0;
        std::size_t j = 0;
        for (std::size_t i = 0; i < takvimli.size(); ++i) {
            while (j < takvimli.size() && takvimli[j] < takvimli[i] + 1'000'000'000) {
                ++j;
            }
            azami = std::max(azami, j - i);
        }
        DOGRULA(d, azami <= 120, "herhangi bir 1 s'lik pencerede ≤ 120 takvimli durum; görülen " + std::to_string(azami));
        DOGRULA(d, azami >= 100, "sınır boşa daraltılmadı (≥ 100/s geçti): " + std::to_string(azami));
        const SunucuSayaclari s = z.c.sayaclar();
        const std::uint64_t uretilen = 160'000 * 3 / 40;  // adım 1…160 000 arasındaki takvim adımı
        DOGRULA(d, s.seyreltilen_durum + takvimli.size() >= uretilen - 2 && s.seyreltilen_durum + takvimli.size() <= uretilen + 2,
                "üretilen = gönderilen + seyreltilen: " + std::to_string(takvimli.size()) + " + " +
                    std::to_string(s.seyreltilen_durum));
        DOGRULA(d, s.seyreltilen_durum > 9000, "seyreltme sayıldı: " + std::to_string(s.seyreltilen_durum));
        DOGRULA(d, z.ist.sayaclar().sira_boslugu == s.seyreltilen_durum,
                "A2-K07: her seyreltilen durum istemcide sıra boşluğu olarak göründü: " +
                    std::to_string(z.ist.sayaclar().sira_boslugu));
        // Takvim dışı iletiler seyreltilmedi: 40 001 ve (52 344, 52 345) sınırları.
        const auto var = [&](std::uint64_t adim) {
            return std::find(takvim_disi_adimlari.begin(), takvim_disi_adimlari.end(), adim) != takvim_disi_adimlari.end();
        };
        DOGRULA(d, var(40'001) && var(52'344) && var(52'345), "takvim dışı sınır iletileri seyreltilmedi");
        DOGRULA(d, sinir_takvimde == 1, "takvim adımına denk gelen T − h iletisi tam bir kez gitti (seyreltilmedi, yinelenmedi)");
        DOGRULA(d, olay == 5, "olaylar eksiksiz: " + std::to_string(olay));
        DOGRULA(d, z.istemci_hatasiz() && !z.k.cerceve_hatasi, "istemci hatasız: " + z.istemci_son_nedeni());
        const viz::GovdeKaydi* tor = z.ist.sahne().bul(dn.torpido_kimligi());
        DOGRULA(d, tor != nullptr && tor->t_dogum == 40'001 * kAdimNs, "torpidonun doğumu istemcide");
        std::printf("  ölçüm (sanal saat): 10x SIL, 20 s: gönderilen takvimli %zu, seyreltilen %llu, 1 s penceresinde en çok %zu\n",
                    takvimli.size(), static_cast<unsigned long long>(s.seyreltilen_durum), azami);
    }
    {  // 1x SIL, dünya yığınlar hâlinde ilerler (300 ms'de 240 adım = 18 takvimli durum): seyreltme yok
        Duzenek z;
        sahte::SahteAyar a = ornek_ayar();
        a.sim_kipi = SimKipi::sil;
        sahte::SahteDunya dn(z.y, a);
        dn.basla();
        DOGRULA(d, z.baglan(), "bağlandı");
        for (int i = 0; i < 100; ++i) {
            dn.adimla(240);
            z.ilerlet(300'000'000, 100'000'000);
        }
        DOGRULA(d, z.c.sayaclar().seyreltilen_durum == 0 && z.ist.sayaclar().sira_boslugu == 0,
                "1x SIL'de seyreltme yok: " + std::to_string(z.c.sayaclar().seyreltilen_durum));
        DOGRULA(d, z.k.say(IletiTuru::durum) == 1801, "30 s'de 1801 durum: " + std::to_string(z.k.say(IletiTuru::durum)));
    }
    {  // HIL kipinde aynı hızda seyreltme uygulanmaz (A2-K25 yalnız SIL)
        Duzenek z;
        sahte::SahteAyar a = ornek_ayar();
        a.sim_kipi = SimKipi::hil_1x;
        sahte::SahteDunya dn(z.y, a);
        dn.basla();
        DOGRULA(d, z.baglan(), "bağlandı");
        for (int i = 0; i < 400; ++i) {
            dn.adimla(40);
            z.ilerlet(5'000'000, 5'000'000);
        }
        DOGRULA(d, z.c.sayaclar().seyreltilen_durum == 0 && z.k.say(IletiTuru::durum) == 1201, "HIL'de seyreltme yok");
    }
    return d.hata;
}

// Takvim adım numarasına bağlıdır: duraklatma takvimi kaydırmaz, sıfırlama onu yeniden başlatır.
int test_takvim_duraklat_sifirla()
{
    Denetci d;
    Duzenek z;
    sahte::SahteDunya dn(z.y, ornek_ayar());
    dn.basla();
    DOGRULA(d, z.baglan(), "bağlandı");
    dn.adimla(20);  // 13 takvimde; 20 değil
    z.ilerlet(25'000'000, 25'000'000);
    DOGRULA(d, z.y.duraklat(), "duraklatıldı");
    z.ilerlet(2'000'000'000);
    DOGRULA(d, z.y.surdur(), "sürdürüldü");
    dn.adimla(30);  // 21…50: takvim 27, 40
    z.ilerlet(40'000'000, 40'000'000);
    dn.sifirla();
    dn.adimla(30);  // 1…30: takvim 13, 27
    z.ilerlet(40'000'000, 40'000'000);
    std::vector<std::pair<std::uint64_t, int>> gorulen;  // (adım, bayrak)
    for (const Cerceve& c : z.k.cerceveler) {
        if (c.tur() == IletiTuru::durum) {
            const viz::DurumIleti m = durum_coz(c);
            gorulen.emplace_back(m.d.adim_no, m.d.bayraklar);
        }
    }
    const std::vector<std::pair<std::uint64_t, int>> beklenen = {
        {0, 0}, {13, 0}, {20, 1} /* duraklatma sınırı, takvim dışı */, {27, 0}, {40, 0},
        {0, 0} /* sıfırlamadan sonraki ilk durum */, {13, 0}, {27, 0}};
    DOGRULA(d, gorulen == beklenen, "durum adımları ve bayrakları beklenen dizide");
    if (gorulen != beklenen) {
        for (const auto& [adim, b] : gorulen) {
            std::printf("    görülen: adım %llu bayrak %d\n", static_cast<unsigned long long>(adim), b);
        }
    }
    DOGRULA(d, z.istemci_hatasiz(), "istemci hatasız: " + z.istemci_son_nedeni());
    return d.hata;
}

}  // namespace ida::a2::test
