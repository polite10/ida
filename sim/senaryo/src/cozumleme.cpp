// Birleştirme ve çözümleme (A5-K13–K16, K98–K101, K108, K79 kanıtları).
//
// Varsayılanların tek kaynağı A5 Ek E'dir: blok `sim/senaryo/veri/ek_e.toml`'a çıkarılmış,
// ikiliye gömülmüştür ve burada yorumlanır (sim.senaryo.ek_e_suruklenme testi dosyanın
// sözleşmedeki blokla bayt bayt aynı olduğunu denetler). Uygulama sırası A5-K16'dır.
#include "ic.hpp"

#include <algorithm>
#include <set>

namespace ida::senaryo {
namespace gomulu {
extern const unsigned char ek_e[];
extern const std::size_t ek_e_boyu;
}  // namespace gomulu

namespace ic {

std::string_view ek_e_metni()
{
    return {reinterpret_cast<const char*>(gomulu::ek_e), gomulu::ek_e_boyu};
}

const Deger& ek_e()
{
    static const Deger e = [] {
        Bulgucu b;
        TomlOkuma t = toml_oku(ek_e_metni(), -1, b);
        if (!t.kok || b.hata_var()) {
            return Deger::yeni_tablo();  // gömülü veri bozuk: test yakalar
        }
        return std::move(*t.kok);
    }();
    return e;
}

namespace {

const std::vector<std::string_view> kKoleksiyonlar = {"iticiler", "firlaticilar", "sensorler"};

void ust_uste(Deger& alt, const Deger& ust)
{
    for (const auto& [k, v] : ust.tablo) {
        Deger* a = alt.bul(k);
        if (a != nullptr && a->tablo_mu() && v.tablo_mu()) {
            ust_uste(*a, v);
        } else {
            alt.tablo[k] = v;  // yaprak (diziler dahil) bütün olarak değişir (A5-K14)
        }
    }
}

std::vector<std::string> parcala(std::string_view s)
{
    std::vector<std::string> o;
    std::size_t bas = 0;
    for (;;) {
        const std::size_t i = s.find('.', bas);
        o.emplace_back(s.substr(bas, i == std::string_view::npos ? std::string_view::npos : i - bas));
        if (i == std::string_view::npos) {
            return o;
        }
        bas = i + 1;
    }
}

// Ek E yolunu (`*` anahtarlı koleksiyonun her girdisi, `ad[]` dizinin her elemanı) gezer ve
// son parçanın ebeveyni olan her tablo için `islev(ebeveyn, son_parca)` çağırır.
template <class F>
void ebeveynleri_gez(Deger& d, const std::vector<std::string>& p, std::size_t i, F&& islev)
{
    if (i + 1 == p.size()) {
        if (d.tablo_mu()) {
            islev(d, p[i]);
        }
        return;
    }
    const std::string& s = p[i];
    if (s == "*") {
        if (!d.tablo_mu()) {
            return;
        }
        for (auto& [k, v] : d.tablo) {
            if (!deneysel_mi(k)) {
                ebeveynleri_gez(v, p, i + 1, islev);
            }
        }
        return;
    }
    if (s.size() > 2 && s.compare(s.size() - 2, 2, "[]") == 0) {
        Deger* a = d.bul(s.substr(0, s.size() - 2));
        if (a == nullptr || !a->dizi_mi()) {
            return;
        }
        for (auto& e : a->dizi) {
            ebeveynleri_gez(e, p, i + 1, islev);
        }
        return;
    }
    if (Deger* alt = d.bul(s); alt != nullptr) {
        ebeveynleri_gez(*alt, p, i + 1, islev);
    }
}

bool kosul_sagla(const Deger& baglam, const Deger& kosul)
{
    const Deger* yol = kosul.bul("yol");
    if (yol == nullptr || !yol->dizgi_mi()) {
        return false;
    }
    const Deger* v = baglam.yol_bul(yol->dizgi);
    if (v == nullptr) {
        return false;
    }
    if (const Deger* d = kosul.bul("deger"); d != nullptr) {
        return esit(*v, *d);
    }
    if (const Deger* icinde = kosul.bul("icinde"); icinde != nullptr && icinde->dizi_mi()) {
        return std::any_of(icinde->dizi.begin(), icinde->dizi.end(),
                           [&](const Deger& x) { return esit(*v, x); });
    }
    return false;
}

void yaz_yoksa(Deger& ebeveyn, const std::string& anahtar, const Deger& deger)
{
    if (ebeveyn.bul(anahtar) == nullptr) {
        ebeveyn.tablo[anahtar] = deger;
    }
}

std::string sensor_ayar_yolu(const std::string& ad, const std::string& k)
{
    return Yol().ekle("arac").ekle("sensorler").ekle(ad).ekle("ayar").ekle(k).metin();
}

}  // namespace

void birlestir(Deger& s, const Deger& arac_dosyasi, Bulgucu& b)
{
    const Deger* v_arac = arac_dosyasi.bul("arac");
    Deger* s_arac = s.bul("arac");
    if (v_arac == nullptr || s_arac == nullptr) {
        return;
    }
    // A5-K15: senaryo anahtarlı koleksiyonlara girdi ekleyemez (`x_` girdi değildir).
    for (const auto koleksiyon : kKoleksiyonlar) {
        const Deger* sk = s_arac->bul(koleksiyon);
        if (sk == nullptr || !sk->tablo_mu()) {
            continue;
        }
        const Deger* vk = v_arac->bul(koleksiyon);
        for (const auto& [k, v] : sk->tablo) {
            if (deneysel_mi(k)) {
                continue;
            }
            if (vk == nullptr || vk->bul(k) == nullptr) {
                b.hata("A5-K15", v.konum,
                       Yol().ekle("arac").ekle(std::string(koleksiyon)).ekle(k),
                       "senaryo araç dosyasında olmayan bir girdi ekleyemez: " + k);
            }
        }
    }
    Deger birlesik = *v_arac;
    ust_uste(birlesik, *s_arac);
    birlesik.konum = s_arac->konum;
    s.tablo["arac"] = std::move(birlesik);

    // otopilot: varsayılan → araç dosyası → senaryo (A5-K13)
    const Deger* v_oto = arac_dosyasi.bul("otopilot");
    const Deger* s_oto = s.bul("otopilot");
    if (v_oto != nullptr) {
        Deger o = *v_oto;
        if (s_oto != nullptr) {
            ust_uste(o, *s_oto);
            o.konum = s_oto->konum;
        }
        s.tablo["otopilot"] = std::move(o);
    }
}

namespace {

bool arac_yolu(const std::string& y)
{
    return y.rfind("arac.", 0) == 0 || y.rfind("otopilot.", 0) == 0;
}

void cozumle_ic(Deger& s, Bulgucu& b, bool yalniz_arac)
{
    const Deger& e = ek_e();
    // (2) kaynak alanları (A5-K100)
    if (Deger* a = s.bul("arac"); a != nullptr) {
        a->tablo.erase("dosya");
    }
    // (3) eksik tablolar (A5-K99)
    if (const Deger* et = e.bul("eksik_tablolar"); et != nullptr && !yalniz_arac) {
        for (const auto& [k, v] : et->tablo) {
            yaz_yoksa(s, k, v);
        }
    }
    // (4) koşulsuz varsayılanlar; Ek E'deki sırayla (ebeveyn önce)
    if (const Deger* ks = e.bul("kosulsuz"); ks != nullptr) {
        std::vector<const std::pair<const std::string, Deger>*> sira;
        for (const auto& kv : ks->tablo) {
            sira.push_back(&kv);
        }
        std::stable_sort(sira.begin(), sira.end(), [](const auto* x, const auto* y) {
            return x->second.konum.satir < y->second.konum.satir;
        });
        for (const auto* kv : sira) {
            if (yalniz_arac && !arac_yolu(kv->first)) {
                continue;
            }
            ebeveynleri_gez(s, parcala(kv->first), 0,
                            [&](Deger& eb, const std::string& son) { yaz_yoksa(eb, son, kv->second); });
        }
    }

    // Kullanıcının verdiği sensör ayarları (A5-K101 "verilmemişse").
    std::set<std::string> verilen;
    Deger* sensorler = s.yol_bul("arac.sensorler");
    if (sensorler != nullptr && sensorler->tablo_mu()) {
        for (const auto& [ad, sn] : sensorler->tablo) {
            if (const Deger* ayar = sn.bul("ayar"); ayar != nullptr && ayar->tablo_mu()) {
                for (const auto& kv : ayar->tablo) {
                    verilen.insert(ad + '\x01' + kv.first);
                }
            }
        }
    }

    // (5a) koşullu varsayılanlar (A5-K98)
    if (const Deger* kl = e.bul("kosullu"); kl != nullptr && kl->dizi_mi() && !yalniz_arac) {
        for (const Deger& k : kl->dizi) {
            const Deger* yol = k.bul("yol");
            const Deger* kosul = k.bul("kosul");
            const Deger* deger = k.bul("deger");
            const Deger* sil = k.bul("sil");
            if (yol == nullptr || kosul == nullptr) {
                continue;
            }
            const bool siler = sil != nullptr && sil->mantiksal_mi() && sil->mantik;
            auto uygula = [&](Deger& baglam, const std::string& goreli) {
                if (!kosul_sagla(baglam, *kosul)) {
                    return;
                }
                ebeveynleri_gez(baglam, parcala(goreli), 0, [&](Deger& eb, const std::string& son) {
                    if (siler) {
                        eb.tablo.erase(son);
                    } else if (deger != nullptr) {
                        yaz_yoksa(eb, son, *deger);
                    }
                });
            };
            const std::string& y = yol->dizgi;
            const std::size_t d = y.find("[].");
            if (d == std::string::npos) {
                uygula(s, y);
            } else if (Deger* dizi = s.bul(y.substr(0, d)); dizi != nullptr && dizi->dizi_mi()) {
                for (Deger& el : dizi->dizi) {
                    if (el.tablo_mu()) {
                        uygula(el, y.substr(d + 3));
                    }
                }
            }
        }
    }

    std::vector<std::string> kalibre_edilmedi;
    std::vector<std::string> dogrulanmamis;

    // (5b) ürün profilleri, kopya, kalibre edilmeyen ve doğrulanmamış değerler
    if (sensorler != nullptr && sensorler->tablo_mu()) {
        const Deger* profiller = e.bul("profil");
        const Deger* ke = e.bul("kalibre_edilmedi");
        const Deger* dy = e.bul("dogrulanmamis");
        for (auto& [ad, sn] : sensorler->tablo) {
            if (deneysel_mi(ad) || !sn.tablo_mu()) {
                continue;
            }
            const Deger* tur = sn.bul("tur");
            if (tur == nullptr || !tur->dizgi_mi()) {
                continue;
            }
            const Deger* etkin = sn.bul("etkin");
            const bool etkin_mi = etkin == nullptr || (etkin->mantiksal_mi() && etkin->mantik);
            auto verildi = [&](const std::string& k) { return verilen.count(ad + '\x01' + k) != 0; };
            if (const Deger* p = profiller != nullptr ? profiller->bul(tur->dizgi) : nullptr) {
                if (const Deger* h = p->bul("hiz_hz"); h != nullptr) {
                    yaz_yoksa(sn, "hiz_hz", *h);
                }
                if (const Deger* sk = p->bul("saat_kaymasi_ppm"); sk != nullptr && sk->sayisal_mi()) {
                    yaz_yoksa(sn, "saat_kaymasi_ppm", *sk);
                }
                if (const Deger* pa = p->bul("ayar"); pa != nullptr && pa->tablo_mu()) {
                    if (sn.bul("ayar") == nullptr) {
                        sn.tablo["ayar"] = Deger::yeni_tablo(pa->konum);
                    }
                    Deger& ayar = sn.tablo["ayar"];
                    if (ayar.tablo_mu()) {
                        for (const auto& [k, v] : pa->tablo) {
                            yaz_yoksa(ayar, k, v);
                        }
                    }
                }
            }
            if (const Deger* k = ke != nullptr ? ke->bul(tur->dizgi) : nullptr) {
                Deger* ayar = sn.bul("ayar");
                if (const Deger* dg = k->bul("deger"); dg != nullptr && ayar != nullptr && ayar->tablo_mu()) {
                    for (const auto& [kk, v] : dg->tablo) {
                        yaz_yoksa(*ayar, kk, v);
                        if (etkin_mi && !verildi(kk)) {
                            kalibre_edilmedi.push_back(sensor_ayar_yolu(ad, kk));
                        }
                    }
                }
                if (const Deger* yz = k->bul("yazilmaz"); yz != nullptr && yz->dizi_mi()) {
                    for (const auto& x : yz->dizi) {
                        if (x.dizgi_mi() && etkin_mi && !verildi(x.dizgi)) {
                            kalibre_edilmedi.push_back(sensor_ayar_yolu(ad, x.dizgi));
                        }
                    }
                }
            }
            if (const Deger* d = dy != nullptr ? dy->bul(tur->dizgi) : nullptr; d != nullptr && d->dizi_mi()) {
                for (const auto& x : d->dizi) {
                    if (x.dizgi_mi() && etkin_mi && !verildi(x.dizgi)) {
                        dogrulanmamis.push_back(sensor_ayar_yolu(ad, x.dizgi));
                        b.uyari("A5-K108", Konum{sn.konum.dosya, 0}, Yol().ekle("arac").ekle("sensorler").ekle(ad).ekle("ayar").ekle(x.dizgi),
                                "soğurma verilmedi: 05 §2.2.1'in doğrulanmamış formülü kullanılacak (AB-089)");
                    }
                }
            }
        }
    }
    // [[kopya]]
    if (const Deger* kp = e.bul("kopya"); kp != nullptr && kp->dizi_mi()) {
        for (const Deger& k : kp->dizi) {
            const Deger* kol = k.bul("koleksiyon");
            const Deger* kosul = k.bul("kosul");
            const Deger* hedef = k.bul("hedef");
            const Deger* kaynak = k.bul("kaynak");
            if (kol == nullptr || kosul == nullptr || hedef == nullptr || kaynak == nullptr) {
                continue;
            }
            Deger* c = s.yol_bul(kol->dizgi);
            if (c == nullptr || !c->tablo_mu()) {
                continue;
            }
            for (auto& [ad, el] : c->tablo) {
                if (deneysel_mi(ad) || !el.tablo_mu() || !kosul_sagla(el, *kosul)) {
                    continue;
                }
                const Deger* kay = el.yol_bul(kaynak->dizgi);
                if (kay == nullptr || el.yol_bul(hedef->dizgi) != nullptr) {
                    continue;
                }
                const Deger kopya = *kay;
                ebeveynleri_gez(el, parcala(hedef->dizgi), 0,
                                [&](Deger& eb, const std::string& son) { yaz_yoksa(eb, son, kopya); });
            }
        }
    }
    // [kalibre_edilmedi_ortam]
    if (const Deger* ko = e.bul("kalibre_edilmedi_ortam"); ko != nullptr && !yalniz_arac) {
        for (const auto& [yol, v] : ko->tablo) {
            bool yazildi = false;
            ebeveynleri_gez(s, parcala(yol), 0, [&](Deger& eb, const std::string& son) {
                if (eb.bul(son) == nullptr) {
                    eb.tablo[son] = v;
                    yazildi = true;
                }
            });
            if (yazildi) {
                kalibre_edilmedi.push_back(yol);
            }
        }
    }
    if (yalniz_arac) {
        return;
    }
    // (7) ölçüm kanıtları (A5-K79) ve üst veri
    Deger kanitlar = Deger::yeni_dizi();
    if (const Deger* ok = e.bul("olcum_kanitlari"); ok != nullptr) {
        const Deger* sonar = s.yol_bul("kalite.sonar");
        const auto kanit = [&](const char* ad) {
            if (const Deger* k = ok->bul(ad); k != nullptr) {
                Deger x = Deger::yeni_tablo();
                x.tablo["yol"] = *k->bul("yol");
                x.tablo["kaynak"] = *k->bul("kaynak");
                kanitlar.dizi.push_back(std::move(x));
            }
        };
        if (sonar != nullptr && sonar->dizgi_mi() && sonar->dizgi == "sade") {
            kanit("sade");
        } else if (sonar != nullptr && sonar->dizgi_mi() && sonar->dizgi == "tam") {
            const Deger* n = s.yol_bul("kalite.sonar_tam.isin_sayisi");
            const Deger* olculmus = ok->bul("tam_olculmus");
            if (n != nullptr && olculmus != nullptr && olculmus->dizi_mi() &&
                std::any_of(olculmus->dizi.begin(), olculmus->dizi.end(),
                            [&](const Deger& x) { return esit(x, *n); })) {
                kanit("tam");
            }
        }
    }
    std::sort(kalibre_edilmedi.begin(), kalibre_edilmedi.end());
    std::sort(dogrulanmamis.begin(), dogrulanmamis.end());
    Deger* meta = s.bul("meta");
    if (meta != nullptr && meta->tablo_mu()) {
        meta->tablo["tur"] = Deger::yeni_dizgi("cozumlenmis_senaryo", meta->konum);
        Deger ke = Deger::yeni_dizi();
        for (auto& x : kalibre_edilmedi) {
            ke.dizi.push_back(Deger::yeni_dizgi(std::move(x)));
        }
        Deger dv = Deger::yeni_dizi();
        for (auto& x : dogrulanmamis) {
            dv.dizi.push_back(Deger::yeni_dizgi(std::move(x)));
        }
        meta->tablo["kalibre_edilmedi"] = std::move(ke);
        meta->tablo["dogrulanmamis_varsayimlar"] = std::move(dv);
        meta->tablo["olcum_kanitlari"] = std::move(kanitlar);
    }
}

}  // namespace

void cozumle(Deger& s, Bulgucu& b)
{
    cozumle_ic(s, b, false);
}

void cozumle_arac(Deger& t, Bulgucu& b)
{
    cozumle_ic(t, b, true);
}

}  // namespace ic
}  // namespace ida::senaryo
