// Sonar yükleyici (W4-bagla-sonar): çözümlenmiş senaryo → ida::sonar::SonarKumesiAyari ve sahne.
#include "ida/senaryo/sonar_yukleyici.hpp"

#include "ic.hpp"
#include "ida/senaryo/a7_varlik.hpp"

#include <cmath>
#include <map>

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

// A7 manifesti (A5-K115): senaryonun `varliklar.kume`sinden yeniden okunur; baytların SHA-256'sı
// doğrulama raporundaki kaynak özetine KOŞULSUZ eşit olmalıdır (W4-bagla-sonar sol #3: doğrulanan
// manifest ile sonarın okuduğu aynı), verilmişse `varliklar.manifest_ozeti`'ne eşittir ve beyan
// edilmişse `varliklar.kume_surumu` manifestteki değerle aynı kalır.
bool manifest_oku(const Deger& c, const DosyaKaynagi& kaynak, const DogrulamaRaporu& rapor, Deger& manifest,
                  std::string& kume, std::string& hata)
{
    const Deger* k = c.yol_bul("varliklar.kume");
    if (k == nullptr || !k->dizgi_mi()) {
        hata = "varliklar.kume yok";
        return false;
    }
    kume = k->dizgi;
    const std::string yol = "assets/" + kume + "/manifest.json";
    const std::optional<std::string> baytlar = kaynak.oku(yol);
    if (!baytlar) {
        hata = "A7 manifesti okunamadı: " + yol;
        return false;
    }
    // A5-K115: senaryonun verdiği özet, sonarın okuduğu manifestle aynı olmalı (A3 ön denetimi
    // `a7_manifest`: sim, sonar ve görselleştiricide aynı manifest).
    const std::string ozet = sha256_hex(*baytlar);
    if (const Deger* oz = c.yol_bul("varliklar.manifest_ozeti"); oz != nullptr && oz->dizgi_mi()) {
        if (ozet != oz->dizgi) {
            hata = "A7 manifestinin özeti varliklar.manifest_ozeti ile uyuşmuyor (A5-K115): " + yol;
            return false;
        }
    }
    // Sol #3: özet beyan edilmemiş olsa da yeniden okunan baytlar doğrulamada okunanlarla aynı olmalı.
    const auto ro = rapor.kaynak_ozetleri.find(yol);
    if (ro == rapor.kaynak_ozetleri.end()) {
        hata = "A7 manifesti doğrulama raporunun kaynak özetlerinde yok (A5-K115): " + yol;
        return false;
    }
    if (ro->second != ozet) {
        hata = "A7 manifesti doğrulamadan sonra değişti: yeniden okunan baytların özeti doğrulama raporundakiyle "
               "uyuşmuyor (A5-K115): " + yol;
        return false;
    }
    ic::JsonOkuma j = ic::json_oku(*baytlar, 0);
    if (!j.kok) {
        hata = "A7 manifesti ayrıştırılamadı: " + j.hata;
        return false;
    }
    if (const Deger* ks = c.yol_bul("varliklar.kume_surumu"); ks != nullptr) {
        const Deger* ms = j.kok->bul("kume_surumu");
        if (ks->tur != Tur::tamsayi || ms == nullptr || ms->tur != Tur::tamsayi || ks->tam != ms->tam) {
            hata = "A7 manifestinin kume_surumu senaryonun beyanıyla aynı değil (A5-K115): " + yol;
            return false;
        }
    }
    manifest = std::move(*j.kok);
    return true;
}

// A7 malzemesinin akustik alanları (A7 §3.4); `kalibrasyon.malzeme.<kimlik>.akustik` geçersiz kılar
// (A5-K13). Bilinmeyen malzemeye varsayılan verilmez (A7-K28; 05 §4).
bool akustik_malzeme(const Deger& c, const Deger& manifest, const std::string& kimlik, sonar::AkustikMalzeme& m,
                     std::string& hata)
{
    m = sonar::AkustikMalzeme{};
    m.kimlik = kimlik;
    bool bulundu = false;
    if (const Deger* ml = manifest.bul("malzemeler"); ml != nullptr && ml->dizi_mi()) {
        for (const Deger& x : ml->dizi) {
            const Deger* k = x.bul("kimlik");
            if (k != nullptr && k->dizgi_mi() && k->dizgi == kimlik) {
                akustik_oku(x.bul("akustik"), m, "A7 malzemeler." + kimlik + ".akustik", hata);
                bulundu = true;
            }
        }
    }
    if (!bulundu) {
        hata = "A7 manifestinde malzeme yok: " + kimlik;
        return false;
    }
    if (const Deger* km = c.yol_bul("kalibrasyon.malzeme"); km != nullptr) {
        if (const Deger* x = km->bul(kimlik); x != nullptr) {
            akustik_oku(x->bul("akustik"), m, "kalibrasyon.malzeme." + kimlik + ".akustik", hata);
        }
    }
    return hata.empty();
}

// Sonar sahnesi (W4-sonar-sahne): su yüzeyi + `ortam.sahne` statik varlığı + `hedefler`; ağlar A7
// varlıklarından (A7-K27: akustik[] doluysa SON_*, boşsa COL_* kaynak ağları), malzeme üçgen başına
// (A7-K28). Yüklenemeyen varlık A7'nin hata koduyla açık hatadır (A7-K51; sessiz atlama yok).
bool sahne_kur(const Deger& c, const DosyaKaynagi& kaynak, const DogrulamaRaporu& rapor, SonarKurulumu& k,
               std::string& hata)
{
    using namespace sonar;
    Deger manifest;
    std::string kume;
    if (!manifest_oku(c, kaynak, rapor, manifest, kume, hata)) {
        return false;
    }
    AgSahnesi& s = *k.sahne;
    if (!s.su_yuzeyi_ekle(kSuYuzeyiGovdesi, s.malzeme_ekle(HavuzTanimi::su_yuzeyi_malzemesi()), &hata)) {
        return false;
    }
    std::map<std::string, std::uint16_t> malzeme_indisi;   // A7 kimliği → sahne malzemesi
    std::map<std::string, A7Varlik> yuklu;                  // varlık kimliği → tek yükleme
    auto yukle = [&](const std::string& kimlik) -> const A7Varlik* {
        if (const auto it = yuklu.find(kimlik); it != yuklu.end()) {
            return &it->second;
        }
        A7Hata h;
        std::optional<A7Varlik> v = a7_varlik_yukle(manifest, kume, kimlik, kaynak, h);
        if (!v) {
            hata = h.metin();
            return nullptr;
        }
        return &yuklu.emplace(kimlik, std::move(*v)).first->second;
    };
    auto ag = [&](const A7Varlik& v, UcgenAgi& u) {
        for (const A7Ag& a : v.akustik) {
            const auto taban = static_cast<std::uint32_t>(u.koseler.size());
            u.koseler.insert(u.koseler.end(), a.koseler.begin(), a.koseler.end());
            for (std::size_t i = 0; i < a.ucgenler.size(); ++i) {
                u.ucgenler.push_back({taban + a.ucgenler[i][0], taban + a.ucgenler[i][1], taban + a.ucgenler[i][2]});
                const std::string& mk = v.malzemeler[a.ucgen_malzeme[i]];
                auto it = malzeme_indisi.find(mk);
                if (it == malzeme_indisi.end()) {
                    AkustikMalzeme m;
                    if (!akustik_malzeme(c, manifest, mk, m, hata)) {
                        return false;
                    }
                    it = malzeme_indisi.emplace(mk, s.malzeme_ekle(m)).first;
                }
                u.malzeme.push_back(it->second);
            }
        }
        return true;
    };
    // Statik sahne varlığı (`ortam.sahne`) `ortam.sahne_poz` pozunda (A5 §3.9; A2 `statik_p_n`).
    {
        const Deger* sa = c.yol_bul("ortam.sahne");
        if (sa == nullptr || !sa->dizgi_mi()) {
            hata = "ortam.sahne yok";
            return false;
        }
        const A7Varlik* v = yukle(sa->dizgi);
        if (v == nullptr) {
            return false;
        }
        AgGovdesi g;
        g.kimlik = kStatikGovde;
        const Okur poz(c.yol_bul("ortam.sahne_poz"), "ortam.sahne_poz", hata);
        poz.vek("konum", g.konum);
        poz.vek("yonelim", g.yonelim);
        if (!hata.empty() || !ag(*v, g.ag) || !s.govde_ekle(g, &hata)) {
            return false;
        }
    }
    // Hedefler (A5 §3.10): yalnız akustik yansıtıcı cisim. Kinematik hareketin belirlenimci türleri
    // (sabit, sabit_hiz, sinus) sahnede uygulanır; rastgele yürüyüş ve rastgele başlangıç yaw'ı A5
    // Ek D akışı ister ve bu dilimde yürütülmez (açık hata).
    if (const Deger* hl = c.bul("hedefler"); hl != nullptr && hl->tablo_mu()) {
        for (const auto& [ad, h] : hl->tablo) {
            const std::string yol = "hedefler." + ad;
            const Okur o(&h, yol, hata);
            const std::string varlik = o.dizgi("varlik");
            std::int64_t kimlik = -1;
            o.tam("kimlik", kimlik);
            bool rastgele = false;
            o.mantik("yonelim_rastgele", rastgele);
            if (!hata.empty()) {
                return false;
            }
            if (rastgele) {
                hata = yol + ".yonelim_rastgele = true: rastgele başlangıç yaw'ı (A5 Ek D.4) bu dilimde sonar sahnesinde "
                             "yürütülmez";
                return false;
            }
            if (kimlik < 0 || kimlik > 65535) {
                hata = yol + ".kimlik geçersiz";
                return false;
            }
            AgGovdesi g;
            g.kimlik = hedef_govde_kimligi(static_cast<std::uint16_t>(kimlik));
            o.vek("konum", g.konum);
            o.vek("yonelim", g.yonelim);
            const Okur hr(h.bul("hareket"), yol + ".hareket", hata);
            const std::string tur = hr.dizgi("tur");
            if (tur.empty() || tur == "sabit") {
                g.hareket.tur = HareketTuru::sabit;
            } else if (tur == "sabit_hiz") {
                g.hareket.tur = HareketTuru::sabit_hiz;
                hr.vek("hiz_ned", g.hareket.hiz_ned);
            } else if (tur == "sinus") {
                g.hareket.tur = HareketTuru::sinus;
                hr.vek("genlik_ned", g.hareket.genlik_ned);
                hr.tam("periyot_ns", g.hareket.periyot_ns);
                hr.sayi("faz", g.hareket.faz);
            } else {
                hata = yol + ".hareket.tur = " + tur + ": rastgele yürüyüş (A5 Ek D akışı) bu dilimde sonar sahnesinde "
                             "yürütülmez";
                return false;
            }
            if (!hata.empty()) {
                return false;
            }
            const A7Varlik* v = yukle(varlik);
            if (v == nullptr) {
                return false;
            }
            if (!ag(*v, g.ag) || !s.govde_ekle(g, &hata)) {
                return false;
            }
            k.hedef_govdeleri.emplace_back(ad, g.kimlik);
        }
    }
    return true;
}

}  // namespace

std::optional<SonarKurulumu> sonar_kur(const DogrulanmisSenaryo& dogrulanmis, const DosyaKaynagi& kaynak,
                                       const DogrulamaRaporu& rapor, std::string* hata_cikti)
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
    if (!hata.empty()) {
        if (hata_cikti != nullptr) *hata_cikti = hata;
        return std::nullopt;
    }
    // Sahne yalnız etkin sonar varken kurulur (manifest ve varlıklar o zaman okunur).
    k.sahne = std::make_shared<AgSahnesi>();
    if (k.etkin_sayisi > 0 && !sahne_kur(c, kaynak, rapor, k, hata)) {
        if (hata_cikti != nullptr) *hata_cikti = "sonar sahnesi kurulamaz: " + hata;
        return std::nullopt;
    }
    return k;
}

}  // namespace ida::senaryo
