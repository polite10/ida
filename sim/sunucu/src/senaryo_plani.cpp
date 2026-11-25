// Senaryo planı (senaryo_plani.hpp).
#include "senaryo_plani.hpp"

#include "ida/port.h"
#include "ida/senaryo/sensor_yukleyici.hpp"

#include <cstdio>
#include <algorithm>
#include <cmath>

namespace ida::sunucu {
namespace {

using senaryo::Deger;
using senaryo::Tur;

// IDAW sabitleri (protocol/ida.xml; üretilmiş başlık bu çeviri birimine alınmaz).
constexpr std::uint8_t kCmdArm = 2, kCmdDisarm = 3, kCmdSetMode = 4;
constexpr std::uint16_t kModGuided = 6;
constexpr std::uint8_t kSpDepth = 2, kSpYaw = 32;

std::string dizgi(const Deger* d) { return d != nullptr && d->dizgi_mi() ? d->dizgi : std::string(); }

std::int64_t tam(const Deger* d, std::int64_t v0 = 0)
{
    if (d == nullptr || !d->sayisal_mi()) {
        return v0;
    }
    return d->tur == Tur::tamsayi ? d->tam : static_cast<std::int64_t>(std::llround(d->sayi));
}

double sayi(const Deger* d, double v0 = 0.0) { return d != nullptr && d->sayisal_mi() ? d->cift() : v0; }

bool mantik(const Deger* d, bool v0 = false) { return d != nullptr && d->mantiksal_mi() ? d->mantik : v0; }

int tup_no(const std::string& s) { return s == "f1" ? 0 : (s == "f2" ? 1 : -1); }

// A1 Tablo 3.6a ve IDAW_MODE (protocol/ida.xml).
std::optional<std::uint16_t> kip_kodu(const std::string& k)
{
    static const std::pair<const char*, std::uint16_t> t[] = {{"MANUAL", 2},       {"STABILIZE", 3}, {"DEPTH_HOLD", 4},
                                                              {"STATION_KEEP", 5}, {"GUIDED", 6},    {"AUTO", 7}};
    for (const auto& [ad, kod] : t) {
        if (k == ad) {
            return kod;
        }
    }
    return std::nullopt;
}

std::optional<std::uint8_t> oge_turu(const std::string& k)
{
    static const char* t[] = {"WAYPOINT", "DIVE_TO", "SURFACE", "HOLD", "SCAN_LINES", "SCAN_POINT", "WAIT_GNSS"};
    for (std::size_t i = 0; i < 7; ++i) {
        if (k == t[i]) {
            return static_cast<std::uint8_t>(i + 1u);
        }
    }
    return std::nullopt;
}

std::optional<std::uint8_t> cerceve_kodu(const std::string& k)
{
    if (k == "NONE") {
        return 0;
    }
    if (k == "LOCAL") {
        return 1;
    }
    if (k == "GLOBAL") {
        return 2;
    }
    return std::nullopt;
}

std::optional<sensor::ArizaTuru> ariza_turu(const std::string& k)
{
    static const std::pair<const char*, sensor::ArizaTuru> t[] = {
        {"ofset", sensor::ArizaTuru::ofset},
        {"olcek", sensor::ArizaTuru::olcek},
        {"suruklenme_dogrusal", sensor::ArizaTuru::suruklenme_dogrusal},
        {"suruklenme_ustel", sensor::ArizaTuru::suruklenme_ustel},
        {"gurultu_artisi", sensor::ArizaTuru::gurultu_artisi},
        {"sicrama", sensor::ArizaTuru::sicrama},
        {"donma", sensor::ArizaTuru::donma},
        {"donma_zaman_damgasi", sensor::ArizaTuru::donma_zaman_damgasi},
        {"yanlis_gecerlilik", sensor::ArizaTuru::yanlis_gecerlilik},
        {"kendini_bildiren", sensor::ArizaTuru::kendini_bildiren},
        {"kayip", sensor::ArizaTuru::kayip},
        {"gec_teslim", sensor::ArizaTuru::gec_teslim},
        {"motor_takilmasi", sensor::ArizaTuru::motor_takilmasi},
        {"dvl_isin_kaybi", sensor::ArizaTuru::dvl_isin_kaybi},
        {"guc_cevrimi", sensor::ArizaTuru::guc_cevrimi},
    };
    for (const auto& [ad, tr] : t) {
        if (k == ad) {
            return tr;
        }
    }
    return std::nullopt;
}

// otopilot.parametreler (A5-K52, K53): kimlik → çekirdeğin parametre adı (genel arayüz).
bool parametreleri_cevir(const Deger* dizi, std::vector<std::pair<std::string, double>>& cikti, std::string& hata)
{
    if (dizi == nullptr || !dizi->dizi_mi()) {
        return true;
    }
    const std::uint16_t n = ida_core_param_count();
    for (const Deger& e : dizi->dizi) {
        const auto kimlik = static_cast<std::uint16_t>(tam(e.bul("kimlik")));
        const std::string tip = dizgi(e.bul("tip"));
        const std::uint8_t tip_kodu = tip == "i32" ? IDA_PT_I32 : (tip == "u32" ? IDA_PT_U32 : IDA_PT_F32);
        bool bulundu = false;
        for (std::uint16_t i = 0; i < n; ++i) {
            ida_param_desc_t pd{};
            if (ida_core_param_desc(i, &pd) != IDA_RC_OK || pd.id != kimlik) {
                continue;
            }
            if (pd.type != tip_kodu) {
                hata = "otopilot.parametreler: kimlik " + std::to_string(kimlik) + " (" + pd.name + ") tipi " + tip +
                       " değil (A5-K52)";
                return false;
            }
            cikti.emplace_back(pd.name, sayi(e.bul("deger")));
            bulundu = true;
            break;
        }
        if (!bulundu) {
            hata = "otopilot.parametreler: kimlik " + std::to_string(kimlik) + " çekirdeğin kataloğunda yok (A5-K52)";
            return false;
        }
    }
    return true;
}

bool olay_cevir(const Deger& e, PlanOlayi& o, std::string& hata)
{
    o.kimlik = dizgi(e.bul("kimlik"));
    const std::string sinif = dizgi(e.bul("sinif"));
    const std::string tur = dizgi(e.bul("tur"));
    const std::string on = "olaylar[" + o.kimlik + "]: ";
    if (const Deger* z = e.bul("zaman_ns"); z != nullptr) {
        o.zaman_ns = tam(z);
    }
    if (const Deger* k = e.bul("kosul"); k != nullptr) {
        const std::string b = dizgi(k->bul("buyukluk"));
        if (b != "derinlik") {
            hata = on + "kosul.buyukluk = " + b + " bu dilimde yürütülemez (kartın etkin görev öğesi izlenmiyor)";
            return false;
        }
        Kosul c;
        const std::string kr = dizgi(k->bul("karsilastirma"));
        c.k = kr == ">"    ? Karsilastirma::buyuk
              : kr == ">=" ? Karsilastirma::buyuk_esit
              : kr == "<"  ? Karsilastirma::kucuk
              : kr == "<=" ? Karsilastirma::kucuk_esit
                           : Karsilastirma::esit;
        c.deger = sayi(k->bul("deger"));
        o.kosul = c;
    }
    o.sure_ns = tam(e.bul("sure_ns"));
    if (sinif == "emniyet_girdisi") {
        o.tur = PlanOlayTuru::emniyet;
        const std::string g = dizgi(e.bul("girdi"));
        o.girdi = g == "acil_durdurma" ? EmniyetGirdisi::acil_durdurma
                  : g == "fis"         ? EmniyetGirdisi::fis
                                       : EmniyetGirdisi::pim;
        o.tup = tup_no(dizgi(e.bul("hedef")));
        o.deger = mantik(e.bul("deger"));
        if (o.girdi != EmniyetGirdisi::acil_durdurma && o.tup < 0) {
            hata = on + "emniyet girdisinin hedefi f1 ya da f2 olmalı";
            return false;
        }
        return true;
    }
    if (sinif == "gcs_komutu") {
        o.tur = PlanOlayTuru::gcs;
        const std::string k = dizgi(e.bul("komut"));
        if (k == "ARM") {
            o.komut = kCmdArm;
        } else if (k == "DISARM") {
            o.komut = kCmdDisarm;
        } else if (k == "SET_MODE") {
            o.komut = kCmdSetMode;
            const auto kod = kip_kodu(dizgi(e.bul("kip")));
            if (!kod) {
                hata = on + "bilinmeyen kip";
                return false;
            }
            o.arg = *kod;
        } else {
            hata = on + "komut " + k + " bu dilimde yürütülemez";
            return false;
        }
        return true;
    }
    if (sinif == "sensor") {
        o.tur = PlanOlayTuru::sensor;
        const auto at = ariza_turu(tur);
        if (!at) {
            hata = on + "sensör arıza türü " + tur + " bu dilimde yürütülemez";
            return false;
        }
        sensor::SensorArizasi& a = o.ariza;
        a.kimlik = o.kimlik;
        a.hedef = dizgi(e.bul("hedef"));
        a.tur = *at;
        a.zaman_ns = o.zaman_ns.value_or(0);
        a.sure_ns = o.sure_ns;
        if (const Deger* s = e.bul("siddet"); s != nullptr) {
            a.siddet.ofset = sayi(s->bul("ofset"), 0.0);
            a.siddet.olcek = sayi(s->bul("olcek"), 1.0);
            a.siddet.oran = sayi(s->bul("oran"), 0.0);
            a.siddet.son_deger = sayi(s->bul("son_deger"), 0.0);
            a.siddet.tau_ns = tam(s->bul("tau_ns"), 0);
            a.siddet.k = sayi(s->bul("k"), 1.0);
            a.siddet.p = sayi(s->bul("p"), 0.0);
            a.siddet.genlik = sayi(s->bul("genlik"), 0.0);
            a.siddet.gecikme_ns = tam(s->bul("gecikme_ns"), 0);
            a.siddet.titresim_ns = tam(s->bul("titresim_ns"), 0);
        }
        a.kanal_maskesi = static_cast<std::uint32_t>(tam(e.bul("kanal_maskesi"), 0xFFFFFFFF));
        a.sira_boz = mantik(e.bul("sira_boz"));
        if (const Deger* z = e.bul("zaman_deseni"); z != nullptr) {
            const std::string zt = dizgi(z->bul("tur"));
            a.zaman_deseni.tur = zt == "bernoulli"         ? sensor::ZamanDeseniTuru::bernoulli
                                 : zt == "gilbert_elliott" ? sensor::ZamanDeseniTuru::gilbert_elliott
                                                           : sensor::ZamanDeseniTuru::surekli;
            a.zaman_deseni.p = sayi(z->bul("p"));
            a.zaman_deseni.t_iyi_ns = tam(z->bul("t_iyi_ns"));
            a.zaman_deseni.t_kotu_ns = tam(z->bul("t_kotu_ns"));
        }
        return true;
    }
    if (sinif == "aktuator") {
        if (tur != "itki_carpani") {
            hata = on + "aktüatör arıza türü " + tur + " bu dilimde yürütülemez";
            return false;
        }
        o.tur = PlanOlayTuru::itici;
        const std::string h = dizgi(e.bul("hedef"));
        if (h == "tumu") {
            o.kanal_maskesi = 0xFFu;
        } else if (h.size() == 2 && h[0] == 't' && h[1] >= '1' && h[1] <= '8') {
            o.kanal_maskesi = static_cast<std::uint8_t>(1u << static_cast<unsigned>(h[1] - '1'));
        } else {
            hata = on + "aktüatör hedefi t1…t8 ya da tumu olmalı";
            return false;
        }
        if (const Deger* s = e.bul("siddet"); s != nullptr) {
            o.carpan = sayi(s->bul("carpan"), 1.0);
        }
        return true;
    }
    hata = on + "sınıf " + sinif + (tur.empty() ? "" : "/" + tur) + " bu dilimde yürütülemez";
    return false;
}

}  // namespace

bool Kosul::sagliyor(double d) const
{
    switch (k) {
    case Karsilastirma::buyuk: return d > deger;
    case Karsilastirma::buyuk_esit: return d >= deger;
    case Karsilastirma::kucuk: return d < deger;
    case Karsilastirma::kucuk_esit: return d <= deger;
    case Karsilastirma::esit: return d == deger;
    }
    return false;
}

bool kisitlayan(const PlanOlayi& o)
{
    switch (o.girdi) {
    case EmniyetGirdisi::acil_durdurma: return o.deger;   // basılı
    case EmniyetGirdisi::fis: return !o.deger;            // çıkık
    case EmniyetGirdisi::pim: return o.deger;             // takılı
    }
    return true;
}

std::string emniyet_alani(const PlanOlayi& o)
{
    switch (o.girdi) {
    case EmniyetGirdisi::acil_durdurma: return "emniyet_girdileri.acil_durdurma";
    case EmniyetGirdisi::fis: return std::string("emniyet_girdileri.fis.") + (o.tup == 0 ? "f1" : "f2");
    case EmniyetGirdisi::pim: return std::string("emniyet_girdileri.pim.") + (o.tup == 0 ? "f1" : "f2");
    }
    return {};
}

bool plan_kur(const senaryo::DogrulanmisSenaryo& dz, const senaryo::DogrulamaRaporu& rapor, Plan& p,
              std::string& hata, double komut_sil_hizi)
{
    const Deger& k = dz.agac();
    p = Plan{};
    p.dosyadan = true;
    p.davranis_ozeti = dz.davranis_ozeti();
    p.kaynak_ozetleri = rapor.kaynak_ozetleri;

    // Çalıştırma ve rastgelelik (A5 §3.8).
    const std::string kip = dizgi(k.yol_bul("calistirma.kip"));
    if (kip == "sil") {
        p.kip = Kip::sil;
        if (!mantik(k.yol_bul("calistirma.sil_azami_hiz")) || sayi(k.yol_bul("calistirma.sil_hiz_carpani"), 1.0) != 1.0) {
            if (komut_sil_hizi > 0.0) {
                // W5-K38: --sil-hiz komut satırı geçersiz kılmasıdır; senaryo alanları yine de uygulanmaz.
                char hiz[32];
                std::snprintf(hiz, sizeof hiz, "%g", komut_sil_hizi);
                p.uyarilar.push_back("calistirma.sil_hiz_carpani/sil_azami_hiz uygulanmaz: hız komut satırındaki "
                                     "--sil-hiz ile belirlenir (" + std::string(hiz) + "x; sonuçları değiştirmez)");
            } else {
                p.uyarilar.push_back("calistirma.sil_hiz_carpani/sil_azami_hiz uygulanmaz: SIL olabildiğince hızlı koşar "
                                     "(sonuçları değiştirmez)");
            }
        }
    } else if (kip == "hil_1x") {
        p.kip = Kip::hil;
    } else {
        hata = "calistirma.kip = " + kip + " bu dilimde yürütülemez (bariyer kipi yok)";
        return false;
    }
    p.sure_ns = tam(k.yol_bul("calistirma.sure_ns"));
    p.tohum = static_cast<std::uint64_t>(tam(k.yol_bul("rastgelelik.ana_tohum")));

    // Bağlantı emülatörü (kapsam dışı): yalnız L0 (ideal) profil.
    {
        const std::string etkin = dizgi(k.yol_bul("baglanti.etkin_profil"));
        const Deger* prof = k.yol_bul("baglanti.profiller");
        const Deger* pr = prof != nullptr ? prof->bul(etkin) : nullptr;
        if (pr == nullptr || dizgi(pr->bul("kademe")) != "L0") {
            hata = "baglanti.etkin_profil " + etkin + " L0 değil: bağlantı emülatörü bu dilimde yok";
            return false;
        }
    }
    if (mantik(k.yol_bul("kalite.kamera_gerekli"))) {
        hata = "kalite.kamera_gerekli = true: kamera bu dilimde yok";
        return false;
    }
    if (const Deger* h = k.bul("hedefler"); h != nullptr && h->tablo_mu() && !h->tablo.empty()) {
        p.uyarilar.push_back("hedefler yürütülmez (hedef hareketi, sonar, kamera ve torpido bu dilimde yok)");
    }

    // Fizik ve sensörler (yalnız DogrulanmisSenaryo kabul eden yükleyiciler).
    std::string h;
    p.fizik = senaryo::fizik_kur(dz, &h);
    if (!p.fizik) {
        hata = "fizik_kur: " + h;
        return false;
    }
    p.sensor = senaryo::sensor_kur(dz, &h);
    if (!p.sensor) {
        hata = "sensor_kur: " + h;
        return false;
    }
    if (!parametreleri_cevir(k.yol_bul("otopilot.parametreler"), p.parametreler, hata)) {
        return false;
    }

    // Başlangıç emniyet durumu ve kütle durumu (A5-K86, K87).
    p.acil_durdurma = mantik(k.yol_bul("emniyet_girdileri.acil_durdurma"));
    p.fis = {mantik(k.yol_bul("emniyet_girdileri.fis.f1")), mantik(k.yol_bul("emniyet_girdileri.fis.f2"))};
    p.pim = {mantik(k.yol_bul("emniyet_girdileri.pim.f1"), true), mantik(k.yol_bul("emniyet_girdileri.pim.f2"), true)};
    p.dolu = {false, false};
    for (const std::string& t : p.fizik->baslangic.yuklu_tupler) {
        const int i = tup_no(t);
        if (i >= 0) {
            p.dolu[static_cast<std::size_t>(i)] = true;
        }
    }

    // Görev (A5-K55–K57).
    if (const Deger* g = k.bul("gorev"); g != nullptr && g->tablo_mu()) {
        p.gorev_var = true;
        p.gorev_yukleme_ns = tam(g->bul("yukleme_zamani_ns"));
        const Deger* og = g->bul("ogeler");
        std::uint16_t i = 0;
        for (const Deger& e : og != nullptr ? og->dizi : std::vector<Deger>{}) {
            kopru::GorevOgesi o;
            o.index = i++;
            const auto tr = oge_turu(dizgi(e.bul("item_type")));
            const auto fr = cerceve_kodu(dizgi(e.bul("frame")));
            if (!tr || !fr) {
                hata = "gorev.ogeler[" + std::to_string(o.index) + "]: tür ya da çerçeve";
                return false;
            }
            o.item_type = *tr;
            o.frame = *fr;
            if (const Deger* pp = e.bul("p"); pp != nullptr && pp->dizi_mi()) {
                for (std::size_t j = 0; j < 4 && j < pp->dizi.size(); ++j) {
                    o.p[j] = static_cast<float>(pp->dizi[j].cift());
                }
            }
            o.lat_e7 = static_cast<std::int32_t>(tam(e.bul("lat_e7")));
            o.lon_e7 = static_cast<std::int32_t>(tam(e.bul("lon_e7")));
            o.north_m = static_cast<float>(sayi(e.bul("north_m")));
            o.east_m = static_cast<float>(sayi(e.bul("east_m")));
            o.depth = static_cast<float>(sayi(e.bul("depth")));
            p.gorev.push_back(o);
        }
    }

    // Zaman çizelgesi (A5 §3.10); dosya sırası korunur.
    if (const Deger* ol = k.bul("olaylar"); ol != nullptr && ol->dizi_mi()) {
        for (const Deger& e : ol->dizi) {
            PlanOlayi o;
            if (!olay_cevir(e, o, hata)) {
                return false;
            }
            p.olaylar.push_back(std::move(o));
        }
    }
    return true;
}

Plan yerlesik_plan(const Ayar& a)
{
    Plan p;
    p.kip = a.kip;
    p.sure_ns = static_cast<std::int64_t>(std::llround(a.sure_s * 1e9));
    p.tohum = a.tohum;
    auto ns = [](double t) { return static_cast<std::int64_t>(std::llround(t * 1e9)); };
    auto emniyet = [&](const char* ad, double t, bool deger) {
        PlanOlayi o;
        o.kimlik = ad;
        o.tur = PlanOlayTuru::emniyet;
        o.girdi = EmniyetGirdisi::acil_durdurma;
        o.deger = deger;
        o.zaman_ns = ns(t);
        p.olaylar.push_back(o);
    };
    if (a.acil_durdurma_s >= 0.0) {
        emniyet("acil", a.acil_durdurma_s, true);
    }
    if (a.acil_kalkar_s >= 0.0) {
        emniyet("acil_kalk", a.acil_kalkar_s, false);
    }
    if (a.senaryo == "z1") {
        const Z1& z = a.z1;
        auto gcs = [&](const char* ad, double t, std::uint8_t komut, std::uint16_t arg) {
            PlanOlayi o;
            o.kimlik = ad;
            o.tur = PlanOlayTuru::gcs;
            o.komut = komut;
            o.arg = arg;
            o.zaman_ns = ns(t);
            p.olaylar.push_back(o);
        };
        auto guided = [&](const char* ad, double t, double derinlik, double yon, int no) {
            PlanOlayi o;
            o.kimlik = ad;
            o.tur = PlanOlayTuru::guided;
            o.guided.mask = static_cast<std::uint8_t>(kSpDepth | kSpYaw);
            o.guided.depth = static_cast<float>(derinlik);
            o.guided.yaw = static_cast<float>(yon);
            o.guided_no = no;
            o.zaman_ns = ns(t);
            p.olaylar.push_back(o);
        };
        gcs("arm", z.t_arm_s, kCmdArm, 0);
        gcs("kip", z.t_kip_s, kCmdSetMode, kModGuided);
        if (z.guided_sp) {
            guided("sp1", z.t_sp_s, z.hedef_derinlik_m, z.hedef_yon_rad, 1);
            guided("sp2", z.t_manevra_s, z.hedef2_derinlik_m, z.hedef2_yon_rad, 2);
        }
        gcs("disarm", z.t_disarm_s, kCmdDisarm, 0);
        if (!z.ariza_kanallari.empty()) {
            PlanOlayi o;
            o.kimlik = "itici_arizasi";
            o.tur = PlanOlayTuru::itici;
            for (int c : z.ariza_kanallari) {
                o.kanal_maskesi = static_cast<std::uint8_t>(o.kanal_maskesi | (1u << static_cast<unsigned>(c)));
            }
            o.carpan = z.ariza_carpani;
            o.zaman_ns = ns(z.t_manevra_s);
            p.olaylar.push_back(o);
        }
    }
    // Yerleşikte sıra zamandır (eşitlikte ekleme sırası); dosyada dosya sırası (A5-K62).
    std::stable_sort(p.olaylar.begin(), p.olaylar.end(),
                     [](const PlanOlayi& x, const PlanOlayi& y) { return *x.zaman_ns < *y.zaman_ns; });
    return p;
}

}  // namespace ida::sunucu
