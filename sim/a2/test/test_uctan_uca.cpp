// test_uctan_uca.cpp — kabul 2: gerçek istemciyle (viz/a2 oturumu) uçtan uca akış; gövde doğumu,
// ölümü ve süreklilik değişimi (A2-U05'in kamera gerektirmeyen kısmı).
#include "takvim.hpp"
#include "test_destek.hpp"

#include <cmath>
#include <cstring>

namespace ida::a2::test {

namespace {

// Tel üzerindeki gövde durumu, sahte kaynağın doğru değeriyle bit düzeyinde aynı mı.
bool ayni(const GovdeDurumu& g, const GovdeAni& a)
{
    return g.govde_kimligi == a.kimlik && std::memcmp(g.p_n, a.p_n.data(), sizeof g.p_n) == 0 &&
           std::memcmp(g.q_nb, a.q_nb.data(), sizeof g.q_nb) == 0 && std::memcmp(g.v_n, a.v_n.data(), sizeof g.v_n) == 0 &&
           std::memcmp(g.w_b, a.w_b.data(), sizeof g.w_b) == 0;
}

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

// merhaba → hosgeldin → dalga_kumesi → ortam → malzeme_optik → (durum) → hazir → akış (A2-K10–K14).
int test_uctan_uca_el_sikisma()
{
    Denetci d;
    Duzenek z;
    sahte::SahteAyar a = ornek_ayar();
    a.hedef_sayisi = 2;
    a.sim_kipi = SimKipi::hil_bariyer;
    a.kademe_3b = Kademe::sade;
    sahte::SahteDunya dn(z.y, a);
    DOGRULA(d, dn.basla(), "çalıştırma başladı");
    dn.adimla(57);  // oturum koşunun ortasında açılır
    DOGRULA(d, z.baglan(), "bağlandı");

    const auto& f = z.k.cerceveler;
    DOGRULA(d, f.size() >= 5, "en az beş çerçeve");
    if (f.size() >= 5) {
        DOGRULA(d, f[0].tur() == IletiTuru::hosgeldin && f[1].tur() == IletiTuru::dalga_kumesi &&
                       f[2].tur() == IletiTuru::ortam && f[3].tur() == IletiTuru::malzeme_optik &&
                       f[4].tur() == IletiTuru::durum,
                "A2-K12 sırası: hosgeldin, dalga_kumesi, ortam, malzeme_optik, sonra akış");
        DOGRULA(d, f[0].b.sira == 0 && f[4].b.sira == 4, "sıra 0'dan başlar");
    }
    DOGRULA(d, z.ist.asama() == viz::Asama::akis, "istemci akışta (hazir gönderdi)");
    DOGRULA(d, z.c.asama() == ic::Asama::akis, "sunucu hazir'ı kabul etti");
    const SunucuSayaclari s = z.c.sayaclar();
    DOGRULA(d, s.hazir_alindi && s.oturum_var && s.baglanti_sayisi == 1 && s.protokol_hatasi == 0, "sayaçlar");

    const viz::HosgeldinIleti& h = z.ist.hosgeldin();
    DOGRULA(d, h.h.oturum_kimligi != 0 && h.h.oturum_kimligi == s.oturum_kimligi, "oturum kimliği ≠ 0");
    DOGRULA(d, h.h.surum_ana == 1 && h.h.surum_alt == 0, "oturum sürümü");
    DOGRULA(d, h.h.sim_kipi == 3 && h.h.kademe_3b == 1 && h.h.kademe_kamera == 0 && h.h.kamera_sayisi == 0,
            "kip, kademe; kamera yok");
    DOGRULA(d, h.h.adim_no == 57 && h.h.t_ns == 57 * kAdimNs && h.h.adim_ns == kAdimNs, "hosgeldin zamanı = kesit sınırı");
    DOGRULA(d, h.h.durum_hiz_pay == 60 && h.h.durum_hiz_payda == 1 && h.h.calisma_nesli == 1, "hız ve nesil");
    DOGRULA(d, std::memcmp(h.h.manifest_sha256, a.manifest_ozeti.data(), 32) == 0, "manifest özeti");
    DOGRULA(d, viz::metin_oku(h.h.manifest_yolu, sizeof h.h.manifest_yolu) == a.manifest_yolu, "manifest yolu");
    DOGRULA(d, h.govdeler.size() == 3 && h.govdeler[0].govde_kimligi == dn.arac_kimligi() &&
                   h.govdeler[0].govde_turu == 1 && h.govdeler[0].dolu_tupler == 3 && h.govdeler[0].varlik_indeksi == 0 &&
                   h.govdeler[1].govde_turu == 3 && h.govdeler[1].varlik_indeksi == 1 && h.govdeler[2].dolu_tupler == 0,
            "gövde tablosu");
    // Dalga kümesi, ortam, malzeme: kaynağın verdiği değerler.
    DOGRULA(d, z.ist.dalga().d.surum >= 1 && z.ist.dalga().bilesenler.size() == dn.bilesenler().size(), "dalga kümesi");
    bool dalga_ayni = z.ist.dalga().bilesenler.size() == dn.bilesenler().size();
    for (std::size_t i = 0; dalga_ayni && i < dn.bilesenler().size(); ++i) {
        dalga_ayni = std::memcmp(&z.ist.dalga().bilesenler[i], &dn.bilesenler()[i], sizeof(DalgaBileseni)) == 0;
    }
    DOGRULA(d, dalga_ayni, "dalga bileşenleri bit düzeyinde aynı");
    DOGRULA(d, z.ist.dalga().d.derin_su == 1 && z.ist.dalga().d.derinlik_m > 0.0 && z.ist.dalga().d.g == 9.80665,
            "dalga başlığı (derin su; derinlik alanı sonlu ve pozitif)");
    Ortam beklenen = dn.ortam();
    beklenen.ortam_surumu = z.ist.ortam().ortam_surumu;
    DOGRULA(d, z.ist.ortam().ortam_surumu >= 1 && std::memcmp(&beklenen, &z.ist.ortam(), sizeof beklenen) == 0, "ortam");
    DOGRULA(d, z.ist.malzeme().m.surum >= 1 && z.ist.malzeme().girdiler.empty(), "malzeme_optik girdisiz de gönderilir");
    // İlk durum hosgeldin'in sınırındadır ve gövdeleri aynıdır.
    if (f.size() >= 5) {
        const viz::DurumIleti ilk = durum_coz(f[4]);
        DOGRULA(d, ilk.d.adim_no == 57 && ilk.d.bayraklar == kDurumTakvimDisi && ilk.govdeler.size() == 3,
                "ilk durum kesitin sınırında (57 takvimde değil: takvim dışı)");
        DOGRULA(d, ilk.d.gonderim_duvar_ns == z.simdi, "gonderim_duvar_ns kuyruğa konduğu an (A2-K08)");
        DOGRULA(d, ilk.govdeler.size() == 3 && ayni(ilk.govdeler[0], dn.dogru_arac(57)) &&
                       ayni(ilk.govdeler[2], dn.dogru_hedef(1, 57)),
                "ilk durumun değerleri kaynağınkilerle aynı");
    }
    DOGRULA(d, z.istemci_hatasiz(), "istemci protokol hatası bildirmedi: " + z.istemci_son_nedeni());
    return d.hata;
}

// 16 gövdeyle durum akışı: her iletinin her gövdesi kaynağın o adımdaki değeriyle bit düzeyinde aynı;
// istemcinin sahne durumu da öyle.
int test_uctan_uca_durum_akisi()
{
    Denetci d;
    Duzenek z;
    sahte::SahteAyar a = ornek_ayar();
    a.hedef_sayisi = 15;  // araçla birlikte 16 gövde (A2-U05)
    sahte::SahteDunya dn(z.y, a);
    DOGRULA(d, dn.basla(), "çalıştırma başladı");
    DOGRULA(d, z.baglan(), "bağlandı");
    for (int i = 0; i < 400; ++i) {
        dn.adimla(5);
        z.ilerlet(6'250'000, 6'250'000);
    }
    DOGRULA(d, !z.k.cerceve_hatasi && z.istemci_hatasiz(), "akış hatasız: " + z.istemci_son_nedeni());
    std::size_t durum_sayisi = 0;
    std::uint64_t k = 0;
    for (const Cerceve& c : z.k.cerceveler) {
        if (c.tur() != IletiTuru::durum) {
            continue;
        }
        const viz::DurumIleti m = durum_coz(c);
        DOGRULA(d, m.d.adim_no == ic::takvim_adimi(k), "durum adımı takvimin k'inci adımı");
        ++k;
        DOGRULA(d, m.d.bayraklar == 0 && m.d.calisma_durumu == 1 && m.govdeler.size() == 16, "takvimli, çalışıyor, 16 gövde");
        DOGRULA(d, m.d.t_ns == static_cast<std::int64_t>(m.d.adim_no) * kAdimNs, "t_ns = adim_no · adim_ns");
        DOGRULA(d, m.d.dalga_kumesi_surumu == z.ist.dalga().d.surum, "durumun dalga sürümü");
        bool hepsi = m.govdeler.size() == 16 && ayni(m.govdeler[0], dn.dogru_arac(m.d.adim_no));
        for (std::uint32_t i = 0; hepsi && i < 15; ++i) {
            hepsi = ayni(m.govdeler[i + 1], dn.dogru_hedef(i, m.d.adim_no)) && m.govdeler[i + 1].govde_turu == 3 &&
                    m.govdeler[i + 1].bayraklar == kGovdeGecerli;
        }
        DOGRULA(d, hepsi, "gövde durumları kaynağın değerleriyle aynı, adım " + std::to_string(m.d.adim_no));
        ++durum_sayisi;
    }
    DOGRULA(d, durum_sayisi == 151, "2000 adımda 151 durum (adım 0 dahil): " + std::to_string(durum_sayisi));
    // İstemcinin sahnesi: 16 gövde, son örnek son durumun zamanında ve değerinde.
    DOGRULA(d, z.ist.sahne().govdeler().size() == 16, "istemci sahnesinde 16 gövde");
    const std::uint64_t son_adim = ic::takvim_adimi(k - 1);
    const viz::GovdeKaydi* arac = z.ist.sahne().bul(dn.arac_kimligi());
    DOGRULA(d, arac != nullptr && !arac->gecmis.empty(), "araç sahnede");
    if (arac != nullptr && !arac->gecmis.empty()) {
        const viz::Ornek& o = arac->gecmis.back();
        const GovdeAni dg = dn.dogru_arac(son_adim);
        DOGRULA(d, o.t_ns == static_cast<std::int64_t>(son_adim) * kAdimNs && o.p.x == dg.p_n[0] && o.p.y == dg.p_n[1] &&
                       o.p.z == dg.p_n[2] && std::fabs(o.q.w - dg.q_nb[0]) < 1e-12 && std::fabs(o.q.z - dg.q_nb[3]) < 1e-12 &&
                       o.v.x == dg.v_n[0],
                "istemcinin son örneği kaynağın değeriyle aynı (kuaterniyon istemcide normallenir)");
        DOGRULA(d, arac->dolu_tupler == 3 && arac->son_sureklilik == 0, "araç tüpleri ve süreklilik");
    }
    DOGRULA(d, z.ist.sayaclar().gecersiz_govde == 0 && z.ist.sayaclar().eksik_govde == 0 && !z.ist.tutarsiz(),
            "istemci geçersiz/eksik gövde ya da tutarsız durum saymadı");
    return d.hata;
}

// Süreklilik sınırı (A2-K21–K23): T − h ve T iletileri, olayın T zamanlı durumdan önce gelmesi,
// doğan gövdenin ilk kez T durumunda yer alması, ölenin T durumunda olmaması.
int test_uctan_uca_sinir()
{
    Denetci d;
    // firlatma 100 (takvimde değil; 99 da değil), isabet 307 (takvimde; 306 değil).
    for (int durum = 0; durum < 2; ++durum) {
        Duzenek z;
        sahte::SahteAyar a = ornek_ayar();
        a.hedef_sayisi = 2;
        a.firlatma_adimi = durum == 0 ? 100 : 41;   // ikinci durumda T − h = 40 takvimdedir
        a.isabet_adimi = 307;
        sahte::SahteDunya dn(z.y, a);
        DOGRULA(d, dn.basla(), "çalıştırma başladı");
        DOGRULA(d, z.baglan(), "bağlandı");
        for (int i = 0; i < 70; ++i) {
            dn.adimla(5);
            z.ilerlet(6'250'000, 6'250'000);
        }
        DOGRULA(d, !z.k.cerceve_hatasi && z.istemci_hatasiz(), "akış hatasız: " + z.istemci_son_nedeni());
        const auto& f = z.k.cerceveler;
        const std::uint64_t T = a.firlatma_adimi;
        // ---- fırlatma: [durum(T − h)] → govde_dogdu → kutle_durumu → durum(T) ----
        std::size_t i_dogum = 0;
        for (std::size_t i = 0; i < f.size(); ++i) {
            if (f[i].tur() == IletiTuru::sahne_olayi && olay_coz(f[i]).tur == 1) {
                i_dogum = i;
                break;
            }
        }
        DOGRULA(d, i_dogum >= 1 && i_dogum + 2 < f.size(), "doğum olayı bulundu");
        if (i_dogum >= 1 && i_dogum + 2 < f.size()) {
            DOGRULA(d, f[i_dogum - 1].tur() == IletiTuru::durum, "doğumdan hemen önce T − h durumu");
            const viz::DurumIleti once = durum_coz(f[i_dogum - 1]);
            DOGRULA(d, once.d.adim_no == T - 1, "T − h zamanlı durum");
            DOGRULA(d, once.d.bayraklar == (ic::takvimde(T - 1) ? 0 : kDurumTakvimDisi),
                    "T − h: takvimde değilse takvim dışı bayrağı");
            const GovdeDurumu* arac0 = bul(once, dn.arac_kimligi());
            DOGRULA(d, arac0 != nullptr && arac0->sureklilik_no == 0 && arac0->dolu_tupler == 3 &&
                           ayni(*arac0, dn.dogru_arac(T - 1)),
                    "T − h: eski parçanın son durumu, eski numara, olay öncesi hız");
            DOGRULA(d, bul(once, dn.torpido_kimligi()) == nullptr, "T − h: torpido yok");
            // T − h yalnız bir kez gönderildi (takvimdeyse takvimli ileti yeterlidir).
            std::size_t ayni_adim = 0;
            for (const Cerceve& c : f) {
                if (c.tur() == IletiTuru::durum && durum_coz(c).d.adim_no == T - 1) {
                    ++ayni_adim;
                }
            }
            DOGRULA(d, ayni_adim == 1, "T − h durumu tam bir kez");

            const SahneOlayi dogum = olay_coz(f[i_dogum]);
            DOGRULA(d, dogum.adim_no == T && dogum.t_ns == static_cast<std::int64_t>(T) * kAdimNs &&
                           dogum.govde_kimligi == dn.torpido_kimligi() && dogum.govde_turu == 2 &&
                           dogum.varlik_indeksi == sahte::kVarlikTorpido && dogum.sureklilik_no == 0 &&
                           dogum.kaynak_tup == 1 && dogum.calisma_nesli == 1,
                    "govde_dogdu alanları");
            DOGRULA(d, f[i_dogum + 1].tur() == IletiTuru::sahne_olayi, "kutle_durumu olayı");
            const SahneOlayi kutle = olay_coz(f[i_dogum + 1]);
            DOGRULA(d, kutle.tur == 3 && kutle.adim_no == T && kutle.govde_kimligi == dn.arac_kimligi() &&
                           kutle.eski_tupler == 3 && kutle.yeni_tupler == 2 && kutle.sureklilik_no == 1,
                    "kutle_durumu alanları (süreklilik tam 1 arttı: kütle + darbe aynı adımda)");
            DOGRULA(d, f[i_dogum + 2].tur() == IletiTuru::durum, "olaylardan sonra T durumu (A2-K22)");
            const viz::DurumIleti sonra = durum_coz(f[i_dogum + 2]);
            DOGRULA(d, sonra.d.adim_no == T && sonra.d.bayraklar == (ic::takvimde(T) ? 0 : kDurumTakvimDisi),
                    "T zamanlı durum, takvim dışı");
            const GovdeDurumu* arac1 = bul(sonra, dn.arac_kimligi());
            DOGRULA(d, arac1 != nullptr && arac1->sureklilik_no == 1 && arac1->dolu_tupler == 2 &&
                           ayni(*arac1, dn.dogru_arac(T)),
                    "T: olay sonrası ilk durum, yeni numara, hız değişmiş");
            DOGRULA(d, arac0 != nullptr && arac1 != nullptr && arac0->v_n[0] != arac1->v_n[0], "aracın hızı sıçradı");
            const GovdeDurumu* tor = bul(sonra, dn.torpido_kimligi());
            DOGRULA(d, tor != nullptr && tor->sureklilik_no == 0 && tor->govde_turu == 2 && ayni(*tor, dn.dogru_torpido(T)),
                    "T'de doğan gövde ilk kez T durumunda (A2-K23)");
        }
        // ---- isabet (T2 = 307, takvimde): durum(306) → isabet → govde_oldu × 2 → durum(307) ----
        std::size_t i_isabet = 0;
        for (std::size_t i = 0; i < f.size(); ++i) {
            if (f[i].tur() == IletiTuru::sahne_olayi && olay_coz(f[i]).tur == 4) {
                i_isabet = i;
                break;
            }
        }
        DOGRULA(d, i_isabet >= 1 && i_isabet + 3 < f.size(), "isabet olayı bulundu");
        if (i_isabet >= 1 && i_isabet + 3 < f.size()) {
            const viz::DurumIleti once = durum_coz(f[i_isabet - 1]);
            DOGRULA(d, f[i_isabet - 1].tur() == IletiTuru::durum && once.d.adim_no == 306 && once.d.bayraklar == kDurumTakvimDisi &&
                           bul(once, dn.torpido_kimligi()) != nullptr && bul(once, dn.hedef_kimligi(0)) != nullptr,
                    "ölümden önce T − h durumu (ölecek gövdeler içinde)");
            const SahneOlayi isabet = olay_coz(f[i_isabet]);
            DOGRULA(d, isabet.adim_no == 307 && isabet.govde_kimligi == dn.torpido_kimligi() &&
                           isabet.diger_govde_kimligi == dn.hedef_kimligi(0) && isabet.normal_n[0] == 1.0,
                    "isabet alanları");
            const SahneOlayi o1 = olay_coz(f[i_isabet + 1]);
            const SahneOlayi o2 = olay_coz(f[i_isabet + 2]);
            DOGRULA(d, o1.tur == 2 && o1.govde_kimligi == dn.torpido_kimligi() && o1.neden == 1 && o2.tur == 2 &&
                           o2.govde_kimligi == dn.hedef_kimligi(0) && o2.adim_no == 307,
                    "govde_oldu olayları");
            const viz::DurumIleti sonra = durum_coz(f[i_isabet + 3]);
            DOGRULA(d, f[i_isabet + 3].tur() == IletiTuru::durum && sonra.d.adim_no == 307 && sonra.d.bayraklar == 0,
                    "T durumu (307 takvimde: bayrak yok)");
            DOGRULA(d, bul(sonra, dn.torpido_kimligi()) == nullptr && bul(sonra, dn.hedef_kimligi(0)) == nullptr &&
                           sonra.govdeler.size() == 2,
                    "T'de ölen gövde T durumunda yok (A2-K23)");
        }
        // İstemcinin gördüğü: doğum ve ölüm zamanları, süreklilik, tüpler.
        const viz::GovdeKaydi* tor = z.ist.sahne().bul(dn.torpido_kimligi());
        DOGRULA(d, tor != nullptr && tor->t_dogum == static_cast<std::int64_t>(T) * kAdimNs && tor->t_olum == 307 * kAdimNs,
                "istemci: torpidonun doğum ve ölüm zamanı");
        if (tor != nullptr) {
            DOGRULA(d, !tor->gecmis.empty() && tor->gecmis.front().t_ns == static_cast<std::int64_t>(T) * kAdimNs &&
                           tor->gecmis.back().t_ns == 306 * kAdimNs,
                    "istemci: torpidonun ilk örneği T'de, son örneği T2 − h'de");
        }
        const viz::GovdeKaydi* arac = z.ist.sahne().bul(dn.arac_kimligi());
        DOGRULA(d, arac != nullptr && arac->son_sureklilik == 1 && arac->dolu_tupler == 2, "istemci: araç süreklilik 1, tüpler 2");
        DOGRULA(d, z.c.sayaclar().gonderilen_olay == 5, "beş olay gönderildi");
    }
    return d.hata;
}

}  // namespace ida::a2::test
