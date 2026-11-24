// Sonar yükleyici (W4-bagla-sonar): çözümlenmiş senaryo → ida::sonar::SonarKumesiAyari ve sahne.
#include "ida/senaryo/sonar_yukleyici.hpp"

#include "ic.hpp"

#include <cmath>

namespace ida::senaryo {

namespace {

const Deger* bul(const Deger* d, const char* k) { return d != nullptr ? d->bul(k) : nullptr; }

class Okur {
public:
    Okur(const Deger* a, std::string yol, std::string& hata) : a_(a), yol_(std::move(yol)), hata_(hata) {}
    void sayi(const char* k, double& o) const
    {
        if (const Deger* d = bul(a_, k); d != nullptr) {
            if (d->sayisal_mi()) o = d->cift();
            else tip(k);
        }
    }
    void sayi(const char* k, std::optional<double>& o) const
    {
        if (const Deger* d = bul(a_, k); d != nullptr) {
            if (d->sayisal_mi()) o = d->cift();
            else tip(k);
        }
    }
    void tam(const char* k, std::int64_t& o) const
    {
        if (const Deger* d = bul(a_, k); d != nullptr) {
            if (d->tur == Tur::tamsayi) o = d->tam;
            else tip(k);
        }
    }
    void tam(const char* k, int& o) const
    {
        std::int64_t v = o;
        tam(k, v);
        o = static_cast<int>(v);
    }
    void mantik(const char* k, bool& o) const
    {
        if (const Deger* d = bul(a_, k); d != nullptr) {
            if (d->mantiksal_mi()) o = d->mantik;
            else tip(k);
        }
    }
    template <std::size_t N>
    void vek(const char* k, std::array<double, N>& o) const
    {
        if (const Deger* d = bul(a_, k); d != nullptr) {
            if (!d->dizi_mi() || d->dizi.size() != N) {
                tip(k);
                return;
            }
            for (std::size_t i = 0; i < N; ++i) o[i] = d->dizi[i].cift();
        }
    }
    std::string dizgi(const char* k) const
    {
        const Deger* d = bul(a_, k);
        return d != nullptr && d->dizgi_mi() ? d->dizgi : std::string();
    }

private:
    const Deger* a_;
    std::string yol_;
    std::string& hata_;
    void tip(const char* k) const
    {
        if (hata_.empty()) hata_ = "beklenmeyen tip: " + yol_ + "." + k;
    }
};

// A7 §3.4 akustik alanları (manifestte ya da kalibrasyon.malzeme.<kimlik>.akustik'te).
void akustik_oku(const Deger* ak, sonar::AkustikMalzeme& m, const std::string& yol, std::string& hata)
{
    if (ak == nullptr) {
        return;
    }
    const Okur o(ak, yol, hata);
    o.sayi("geri_sacilim_mu", m.geri_sacilim_mu);
    o.sayi("ayna_yansima_rs", m.ayna_yansima_rs);
    o.sayi("puruzluluk_gamma_p", m.puruzluluk_gamma_p);
    const std::string y = o.dizgi("gelis_yasasi");
    if (y == "cos1") {
        m.gelis_yasasi = sonar::GelisYasasi::cos1;
    } else if (y == "lambert_cos2") {
        m.gelis_yasasi = sonar::GelisYasasi::lambert_cos2;
    } else if (!y.empty() && hata.empty()) {
        hata = yol + ".gelis_yasasi bilinmiyor: " + y;
    }
}

// Taban malzemesi: `ortam.sahne` statik varlığının ilk malzeme ataması (A7), kalibrasyonla.
bool taban_malzemesi(const Deger& c, const DosyaKaynagi& kaynak, sonar::AkustikMalzeme& m, std::string& hata)
{
    const Deger* kume = c.yol_bul("varliklar.kume");
    const Deger* sahne = c.yol_bul("ortam.sahne");
    if (kume == nullptr || !kume->dizgi_mi() || sahne == nullptr || !sahne->dizgi_mi()) {
        hata = "varliklar.kume ya da ortam.sahne yok";
        return false;
    }
    const std::string yol = "assets/" + kume->dizgi + "/manifest.json";
    const std::optional<std::string> baytlar = kaynak.oku(yol);
    if (!baytlar) {
        hata = "A7 manifesti okunamadı: " + yol;
        return false;
    }
    // A5-K115: senaryonun verdiği özet, sonarın okuduğu manifestle aynı olmalı (A3 ön denetimi
    // `a7_manifest`: sim, sonar ve görselleştiricide aynı manifest).
    if (const Deger* oz = c.yol_bul("varliklar.manifest_ozeti"); oz != nullptr && oz->dizgi_mi()) {
        if (sha256_hex(*baytlar) != oz->dizgi) {
            hata = "A7 manifestinin özeti varliklar.manifest_ozeti ile uyuşmuyor (A5-K115): " + yol;
            return false;
        }
    }
    const ic::JsonOkuma j = ic::json_oku(*baytlar, 0);
    if (!j.kok) {
        hata = "A7 manifesti ayrıştırılamadı: " + j.hata;
        return false;
    }
    const Deger* varliklar = j.kok->bul("varliklar");
    const Deger* statik = nullptr;
    if (varliklar != nullptr && varliklar->dizi_mi()) {
        for (const Deger& v : varliklar->dizi) {
            const Deger* k = v.bul("kimlik");
            const Deger* t = v.bul("tur");
            if (k != nullptr && k->dizgi_mi() && k->dizgi == sahne->dizgi && t != nullptr && t->dizgi_mi() &&
                t->dizgi == "statik") {
                statik = &v;
            }
        }
    }
    const Deger* atama = statik != nullptr ? statik->bul("malzeme_atamasi") : nullptr;
    if (atama == nullptr || !atama->dizi_mi() || atama->dizi.empty()) {
        hata = "A7 statik varlığının malzeme ataması yok: " + sahne->dizgi;
        return false;
    }
    const Deger* mk = atama->dizi.front().bul("malzeme");
    if (mk == nullptr || !mk->dizgi_mi()) {
        hata = "A7 malzeme ataması geçersiz: " + sahne->dizgi;
        return false;
    }
    m.kimlik = mk->dizgi;
    const Deger* malzemeler = j.kok->bul("malzemeler");
    bool bulundu = false;
    if (malzemeler != nullptr && malzemeler->dizi_mi()) {
        for (const Deger& x : malzemeler->dizi) {
            const Deger* k = x.bul("kimlik");
            if (k != nullptr && k->dizgi_mi() && k->dizgi == m.kimlik) {
                akustik_oku(x.bul("akustik"), m, yol + ": malzemeler." + m.kimlik + ".akustik", hata);
                bulundu = true;
            }
        }
    }
    if (!bulundu) {
        hata = "A7 manifestinde malzeme yok: " + m.kimlik;
        return false;
    }
    // A5-K13: kalibrasyon.malzeme.<kimlik>.akustik A7 değerini geçersiz kılar.
    if (const Deger* km = c.yol_bul("kalibrasyon.malzeme"); km != nullptr) {
        if (const Deger* x = km->bul(m.kimlik); x != nullptr) {
            akustik_oku(x->bul("akustik"), m, "kalibrasyon.malzeme." + m.kimlik + ".akustik", hata);
        }
    }
    return hata.empty();
}

}  // namespace

std::optional<SonarKurulumu> sonar_kur(const DogrulanmisSenaryo& dogrulanmis, const DosyaKaynagi& kaynak,
                                       std::string* hata_cikti)
{
    using namespace sonar;
    std::string hata;
    const Deger& c = dogrulanmis.agac();
    SonarKurulumu k;
    SonarKumesiAyari& a = k.ayar;
    // Kademe (A5 §3.12, K78): bu dilimde sonar modeli yalnız sade kademededir.
    {
        const Deger* kd = c.yol_bul("kalite.sonar");
        const std::string kademe = kd != nullptr && kd->dizgi_mi() ? kd->dizgi : "sade";
        if (kademe != "sade") {
            if (hata_cikti != nullptr) {
                *hata_cikti = "kalite.sonar = " + kademe + " bu dilimde yürütülemez (sonar modeli sade kademede)";
            }
            return std::nullopt;
        }
        a.kademe = SonarKademesi::sade;
    }
    if (const Deger* sl = c.yol_bul("arac.sensorler"); sl != nullptr && sl->tablo_mu()) {
        for (const auto& [ad, sn] : sl->tablo) {
            if (deneysel_mi(ad) || !sn.tablo_mu()) {
                continue;
            }
            const Deger* tur = sn.bul("tur");
            if (tur == nullptr || !tur->dizgi_mi() || (tur->dizgi != "ping1d" && tur->dizgi != "ping360")) {
                continue;
            }
            const bool p1 = tur->dizgi == "ping1d";
            SonarAyari s;
            s.ad = ad;
            s.tur = p1 ? SensorTuru::ping1d : SensorTuru::ping360;
            const std::string yol = "arac.sensorler." + ad;
            const Okur ust(&sn, yol, hata);
            std::int64_t v = 0;
            ust.tam("kimlik", v);
            s.kimlik = static_cast<std::uint16_t>(v);
            v = 0;
            ust.tam("inst", v);
            s.inst = static_cast<std::uint8_t>(v);
            s.profil = ust.dizgi("profil");
            ust.mantik("etkin", s.etkin);
            ust.sayi("saat_kaymasi_ppm", s.saat_kaymasi_ppm);
            const Okur montaj(sn.bul("montaj"), yol + ".montaj", hata);
            montaj.vek("konum", s.montaj.konum);
            montaj.vek("yonelim", s.montaj.yonelim);
            const Okur ay(sn.bul("ayar"), yol + ".ayar", hata);
            if (p1) {
                Ping1dAyar x;
                ay.tam("kutu_sayisi", x.kutu_sayisi);
                ay.tam("ping_araligi_ns", x.ping_araligi_ns);
                ay.sayi("oto_menzil_carpani", x.oto_menzil_carpani);
                ay.tam("islem_suresi_ns", x.islem_suresi_ns);
                ay.sayi("sogurma", x.sogurma);
                s.ayar = x;
                s.yapilandirma = Ping1dYapilandirmasi{};
            } else {
                Ping360Ayar x;
                const std::string b = ay.dizgi("baglanti");
                x.baglanti = b == "ethernet" ? Ping360Baglanti::ethernet
                             : b == "rs485"  ? Ping360Baglanti::rs485
                                             : Ping360Baglanti::usb;
                if (const Deger* bd = bul(sn.bul("ayar"), "baud"); bd != nullptr && bd->tur == Tur::tamsayi) {
                    x.baud = static_cast<int>(bd->tam);
                }
                ay.tam("t_m0_ns", x.t_m0_ns);
                ay.tam("t_grad_ns", x.t_grad_ns);
                ay.tam("islem_suresi_ns", x.islem_suresi_ns);
                ay.sayi("gurultu_tabani", x.gurultu_tabani);
                ay.sayi("yakin_alan_genligi", x.yakin_alan_genligi);
                ay.sayi("tvg_a", x.tvg_a);
                ay.sayi("sogurma", x.sogurma);
                s.ayar = x;
                s.yapilandirma = Ping360Yapilandirmasi{};
            }
            if (s.etkin) {
                ++k.etkin_sayisi;
            }
            a.sonarlar.push_back(std::move(s));
        }
    }
    const Okur ortam(c.bul("ortam"), "ortam", hata);
    ortam.sayi("yercekimi", a.ortam.yercekimi);
    const Okur su(c.yol_bul("ortam.su"), "ortam.su", hata);
    su.sayi("yogunluk", a.ortam.su.yogunluk);
    su.sayi("sicaklik_c", a.ortam.su.sicaklik_c);
    su.sayi("tuzluluk", a.ortam.su.tuzluluk);
    su.sayi("ph", a.ortam.su.ph);
    su.sayi("ses_hizi", a.ortam.su.ses_hizi);   // yoksa formül (W4-K1)
    if (const Deger* t = c.yol_bul("rastgelelik.ana_tohum"); t != nullptr && t->tur == Tur::tamsayi) {
        a.ana_tohum = static_cast<std::uint64_t>(t->tam);
    }
    // Sahne: su yüzeyi ve düz taban (`ortam.derinlik`).
    double derinlik = 0.0;
    ortam.sayi("derinlik", derinlik);
    AkustikMalzeme taban;
    if (hata.empty() && k.etkin_sayisi > 0) {
        (void)taban_malzemesi(c, kaynak, taban, hata);
    }
    if (!hata.empty()) {
        if (hata_cikti != nullptr) *hata_cikti = hata;
        return std::nullopt;
    }
    k.sahne = std::make_shared<CozumselSahne>();
    if (k.etkin_sayisi > 0) {
        if (!(derinlik > 0.0) || !std::isfinite(derinlik)) {
            if (hata_cikti != nullptr) *hata_cikti = "ortam.derinlik > 0 olmalı (sonar sahnesi)";
            return std::nullopt;
        }
        acik_su_ekle(*k.sahne, derinlik, taban);
        const Deger* h = c.bul("hedefler");
        const bool hedef_var = h != nullptr && h->tablo_mu() && !h->tablo.empty();
        const Deger* tur = c.yol_bul("ortam.tur");
        const bool havuz = tur != nullptr && tur->dizgi_mi() && tur->dizgi == "havuz";
        if (havuz || hedef_var) {
            k.uyarilar.push_back(std::string("sonar sahnesi: yalnız su yüzeyi ve düz taban (ortam.derinlik, malzeme ") +
                                 taban.kimlik + "); " + (havuz ? "havuz duvarları" : "") +
                                 (havuz && hedef_var ? " ve " : "") + (hedef_var ? "hedef gövdeleri" : "") +
                                 " A7 ağlarından kurulur, bu dilimde sahnede yok");
        }
    }
    return k;
}

}  // namespace ida::senaryo
