// Aşama 2 anlamsal kuralları: A5 §4.3–§4.13'ün yükleme anında denetlenebilen kuralları ve
// A7 atıfları (A5-K18, K19, K36). Kurallar çözümlenmiş senaryo üzerinde koşar (koşullu
// varsayılanlar yazılmış, profil değerleri dolu); A5-K102 çözümlemeden önce koşar.
//
// Kural başına yapı tabloları (olay alanları, şiddet alanları, görev öğesi aralıkları) bu
// dosyanın başındadır; kurallar onları yorumlar.
#include "ic.hpp"

#include <Eigen/Dense>

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <map>
#include <set>

namespace ida::senaryo::ic {

namespace {

constexpr double kPi = 3.141592653589793;
constexpr std::int64_t kFizikAdimiNs = 1'250'000;  // iskelet §4 (A5-K77: A5'ten ayarlanmaz)

Yol yp(std::string_view noktali)
{
    Yol y;
    std::size_t bas = 0;
    for (;;) {
        const std::size_t i = noktali.find('.', bas);
        y = y.ekle(std::string(
            noktali.substr(bas, i == std::string_view::npos ? std::string_view::npos : i - bas)));
        if (i == std::string_view::npos) {
            return y;
        }
        bas = i + 1;
    }
}

bool dizgi_esit(const Deger* d, std::string_view s)
{
    return d != nullptr && d->dizgi_mi() && d->dizgi == s;
}

bool dogru_mu(const Deger* d)
{
    return d != nullptr && d->mantiksal_mi() && d->mantik;
}

std::vector<double> sayilar(const Deger* d)
{
    std::vector<double> o;
    if (d != nullptr && d->dizi_mi()) {
        for (const auto& e : d->dizi) {
            o.push_back(e.sayisal_mi() ? e.cift() : std::numeric_limits<double>::quiet_NaN());
        }
    }
    return o;
}

// Bileşen sırasıyla kareler toplamı (A5-K32).
double kare_toplami(const std::vector<double>& v)
{
    double t = 0.0;
    for (const double x : v) {
        t += x * x;
    }
    return t;
}

struct Baglam {
    const Deger& s;
    Bulgucu& b;
};

// Etkin ve anahtarlı-koleksiyon girdisi olan sensörler (ad, tablo).
std::vector<std::pair<std::string, const Deger*>> sensorler(const Deger& s, bool yalniz_etkin)
{
    std::vector<std::pair<std::string, const Deger*>> o;
    const Deger* c = s.yol_bul("arac.sensorler");
    if (c == nullptr || !c->tablo_mu()) {
        return o;
    }
    for (const auto& [ad, sn] : c->tablo) {
        if (deneysel_mi(ad) || !sn.tablo_mu()) {
            continue;
        }
        if (yalniz_etkin && !dogru_mu(sn.bul("etkin"))) {
            continue;
        }
        o.emplace_back(ad, &sn);
    }
    return o;
}

Yol sensor_yolu(const std::string& ad)
{
    return Yol().ekle("arac").ekle("sensorler").ekle(ad);
}

// ------------------------------------------------------------------ §4.3 araç
void birim_denetle(Baglam& c, const Deger* d, const Yol& yol, bool kamera)
{
    if (d == nullptr || !d->dizi_mi()) {
        return;
    }
    const std::vector<double> v = sayilar(d);
    const double t = kare_toplami(v);
    if (kamera) {
        // A5-K32 kamera istisnası: A2 §3.6 montaj_q_bs ölçütü.
        if (!(std::abs(t - 1.0) <= 1e-5)) {
            c.b.hata("A5-K32", d->konum, yol,
                     "kamera montaj kuaterniyonu |w²+x²+y²+z² − 1| ≤ 1e-5 ölçütünü sağlamıyor");
        }
    } else if (!(std::abs(std::sqrt(t) - 1.0) <= 1e-6)) {
        c.b.hata("A5-K32", d->konum, yol, "birim vektör/kuaterniyon normu 1'den 1e-6'dan çok sapıyor");
    }
}

void arac_kurallari(Baglam& c)
{
    const Deger& s = c.s;
    // A5-K21
    if (const Deger* at = s.yol_bul("arac.kutle.atalet"); at != nullptr && at->dizi_mi() && at->dizi.size() == 3) {
        Eigen::Matrix3d I;
        for (int i = 0; i < 3; ++i) {
            const auto satir = sayilar(&at->dizi[static_cast<std::size_t>(i)]);
            for (int j = 0; j < 3; ++j) {
                I(i, j) = satir.size() == 3 ? satir[static_cast<std::size_t>(j)] : 0.0;
            }
        }
        const Yol y = yp("arac.kutle.atalet");
        if ((I - I.transpose()).cwiseAbs().maxCoeff() > 1e-12) {
            c.b.hata("A5-K21", at->konum, y, "atalet matrisi simetrik değil (|I_ij − I_ji| > 1e-12)");
        } else {
            const Eigen::SelfAdjointEigenSolver<Eigen::Matrix3d> es(I);
            const Eigen::Vector3d l = es.eigenvalues();
            if (!(l(0) > 0.0)) {
                c.b.hata("A5-K21", at->konum, y, "atalet matrisi pozitif tanımlı değil");
            } else if (!(l(0) + l(1) >= l(2) && l(0) + l(2) >= l(1) && l(1) + l(2) >= l(0))) {
                c.b.hata("A5-K21", at->konum, y, "asal momentler üçgen eşitsizliğini sağlamıyor");
            }
        }
    }
    // A5-K24
    if (const Deger* tk = s.yol_bul("arac.kutle.trim_kutlesi"); tk != nullptr && tk->sayisal_mi() &&
                                                                 tk->cift() > 0.0 &&
                                                                 s.yol_bul("arac.kutle.trim_konumu") == nullptr) {
        c.b.hata("A5-K24", Konum{tk->konum.dosya, 0}, yp("arac.kutle.trim_konumu"),
                 "trim_kutlesi > 0 iken trim_konumu zorunlu");
    }
    // A5-K25, K32 (itici yönü)
    if (const Deger* it = s.yol_bul("arac.iticiler"); it != nullptr && it->tablo_mu()) {
        Eigen::Matrix<double, 6, 8> B = Eigen::Matrix<double, 6, 8>::Zero();
        std::map<std::int64_t, std::string> kanallar;
        int sutun = 0;
        for (const auto& [ad, t] : it->tablo) {
            if (deneysel_mi(ad) || !t.tablo_mu() || sutun >= 8) {
                continue;
            }
            const Yol y = yp("arac.iticiler").ekle(ad);
            if (const Deger* k = t.bul("kanal"); k != nullptr && k->tur == Tur::tamsayi) {
                if (!kanallar.emplace(k->tam, ad).second) {
                    c.b.hata("A5-K25", k->konum, y.ekle("kanal"),
                             "kanal değeri tekil olmalı (" + kanallar[k->tam] + " ile aynı)");
                }
            }
            birim_denetle(c, t.bul("yon"), y.ekle("yon"), false);
            const auto r = sayilar(t.bul("konum"));
            const auto e = sayilar(t.bul("yon"));
            if (r.size() == 3 && e.size() == 3) {
                const Eigen::Vector3d rv(r[0], r[1], r[2]);
                const Eigen::Vector3d ev(e[0], e[1], e[2]);
                B.col(sutun).head<3>() = ev;
                B.col(sutun).tail<3>() = rv.cross(ev);
            }
            ++sutun;
        }
        if (B.allFinite()) {
            Eigen::FullPivLU<Eigen::Matrix<double, 6, 8>> lu(B);
            if (lu.rank() != 6) {
                c.b.hata("A5-K25", it->konum, yp("arac.iticiler"),
                         "itici matrisi B = [ε; r × ε]'nin rankı 6 değil");
            }
        }
    }
    // A5-K26
    {
        const Deger* ot = s.yol_bul("arac.hidrodinamik.belirsizlik.oteleme");
        const Deger* dn = s.yol_bul("arac.hidrodinamik.belirsizlik.donme");
        if (ot != nullptr && dn != nullptr && ot->sayisal_mi() && dn->sayisal_mi()) {
            for (const char* ad : {"eklenmis_kutle", "dogrusal_sonum", "karesel_sonum"}) {
                const std::string yol = std::string("arac.hidrodinamik.sapma.") + ad;
                const Deger* d = s.yol_bul(yol);
                const auto v = sayilar(d);
                for (std::size_t i = 0; i < v.size(); ++i) {
                    const double bant = i < 3 ? ot->cift() : dn->cift();
                    if (!(v[i] >= 1.0 - bant && v[i] <= 1.0 + bant)) {
                        c.b.hata("A5-K26", d->dizi[i].konum, yp(yol).ekle(i),
                                 "sapma çarpanı belirsizlik bandının dışında");
                    }
                }
            }
        }
    }
    // A5-K28
    if (dizgi_esit(s.yol_bul("arac.batarya.kip"), "ideal")) {
        const Deger* v = s.yol_bul("arac.batarya.v_sabit");
        if (v == nullptr) {
            c.b.hata("A5-K28", Konum{s.yol_bul("arac.batarya")->konum.dosya, 0},
                     yp("arac.batarya.v_sabit"), "ideal batarya kipinde v_sabit zorunlu");
        } else if (v->sayisal_mi() && (v->cift() < 12.0 || v->cift() > 20.0)) {
            c.b.uyari("A5-K28", v->konum, yp("arac.batarya.v_sabit"),
                      "v_sabit 12–20 V dışında: itici modeli dışdeğerleme yapar (04 §2.7.4)");
        }
    }
    // A5-K30
    {
        const char* gruplar[] = {"gerilim_noktalari", "azami_itki_ileri", "azami_itki_geri",
                                 "tam_gaz_akimi"};
        const Deger* g0 = s.yol_bul("arac.itici_modeli.gerilim_noktalari");
        const auto gn = sayilar(g0);
        for (const char* ad : gruplar) {
            const std::string yol = std::string("arac.itici_modeli.") + ad;
            const Deger* d = s.yol_bul(yol);
            if (d != nullptr && sayilar(d).size() != gn.size()) {
                c.b.hata("A5-K30", d->konum, yp(yol),
                         std::string(ad) + " gerilim_noktalari ile aynı uzunlukta olmalı");
            }
        }
        for (std::size_t i = 1; i < gn.size(); ++i) {
            if (!(gn[i] > gn[i - 1])) {
                c.b.hata("A5-K30", g0->konum, yp("arac.itici_modeli.gerilim_noktalari"),
                         "gerilim_noktalari kesin artan olmalı");
                break;
            }
        }
        const Deger* h = s.yol_bul("arac.itici_modeli.havalanma_h_bolu_r");
        const Deger* be = s.yol_bul("arac.itici_modeli.havalanma_beta");
        const auto hv = sayilar(h);
        if (be != nullptr && sayilar(be).size() != hv.size()) {
            c.b.hata("A5-K30", be->konum, yp("arac.itici_modeli.havalanma_beta"),
                     "havalanma_beta havalanma_h_bolu_r ile aynı uzunlukta olmalı");
        }
        for (std::size_t i = 1; i < hv.size(); ++i) {
            if (!(hv[i] > hv[i - 1])) {
                c.b.hata("A5-K30", h->konum, yp("arac.itici_modeli.havalanma_h_bolu_r"),
                         "havalanma_h_bolu_r kesin artan olmalı");
                break;
            }
        }
    }
    // A5-K104
    if (const Deger* p = s.yol_bul("arac.itici_modeli.sekil_polinomu"); p != nullptr) {
        const auto cs = sayilar(p);
        double N = -std::numeric_limits<double>::infinity();
        bool pozitif = true;
        for (int j = 0; j <= 1000; ++j) {
            const double u = static_cast<double>(j) / 1000.0;
            double F = 0.0;
            double us = u;
            for (const double k : cs) {
                F += k * us;
                us *= u * u;
            }
            N = std::max(N, F);
            if (j >= 1 && !(F > 0.0)) {
                pozitif = false;
            }
        }
        if (!pozitif || !std::isfinite(N) || !(N > 0.0)) {
            c.b.hata("A5-K104", p->konum, yp("arac.itici_modeli.sekil_polinomu"),
                     "şekil polinomu (0, 1] aralığında pozitif ve normalleştirme böleni sonlu ve "
                     "pozitif olmalı");
        }
    }
    // A5-K105
    {
        const Deger* pd = s.yol_bul("arac.itici_modeli.transfer_pay");
        const Deger* qd = s.yol_bul("arac.itici_modeli.transfer_payda");
        const auto pay = sayilar(pd);
        const auto payda = sayilar(qd);
        if (!pay.empty() && payda.size() >= 2) {
            const Yol y = yp("arac.itici_modeli.transfer_payda");
            if (payda.front() == 0.0 || payda.back() == 0.0) {
                c.b.hata("A5-K105", qd->konum, y, "paydanın ilk ve son katsayısı sıfır olamaz");
            } else {
                if (!(pay.size() < payda.size())) {
                    c.b.hata("A5-K105", pd->konum, yp("arac.itici_modeli.transfer_pay"),
                             "transfer fonksiyonu kesin uygun olmalı (pay paydadan kısa)");
                }
                if (!(std::abs(pay.back() / payda.back() - 1.0) <= 1e-12)) {
                    c.b.hata("A5-K105", pd->konum, yp("arac.itici_modeli.transfer_pay"),
                             "DC kazanç pay[son]/payda[son] 1 olmalı (±1e-12)");
                }
                // Routh–Hurwitz: ilk sütunun bütün elemanları baş katsayıyla aynı işaretli
                // ve sıfırdan farklı.
                const std::size_t n = payda.size();
                std::vector<std::vector<double>> R(n, std::vector<double>((n + 1) / 2 + 1, 0.0));
                for (std::size_t i = 0; i < n; ++i) {
                    R[i % 2][i / 2] = payda[i];
                }
                bool kararli = true;
                for (std::size_t r = 2; r < n && kararli; ++r) {
                    const double a = R[r - 1][0];
                    if (a == 0.0) {
                        kararli = false;
                        break;
                    }
                    for (std::size_t k = 0; k + 1 < R[r].size(); ++k) {
                        R[r][k] = (a * R[r - 2][k + 1] - R[r - 2][0] * R[r - 1][k + 1]) / a;
                    }
                }
                const double isaret = payda.front() > 0.0 ? 1.0 : -1.0;
                for (std::size_t r = 0; r < n && kararli; ++r) {
                    if (!(R[r][0] * isaret > 0.0) || !std::isfinite(R[r][0])) {
                        kararli = false;
                    }
                }
                if (!kararli) {
                    c.b.hata("A5-K105", qd->konum, y,
                             "payda Routh–Hurwitz ölçütüne göre kararlı değil");
                }
            }
        }
    }
    // A5-K31
    {
        const Deger* n = s.yol_bul("arac.esc.pwm_notr_us");
        const Deger* o = s.yol_bul("arac.esc.olu_bant_us");
        const Deger* mn = s.yol_bul("arac.esc.pwm_min_us");
        const Deger* mx = s.yol_bul("arac.esc.pwm_max_us");
        if (n && o && mn && mx && n->sayisal_mi() && o->sayisal_mi() && mn->sayisal_mi() &&
            mx->sayisal_mi()) {
            if (!(mn->cift() < n->cift() - o->cift() && n->cift() + o->cift() < mx->cift())) {
                c.b.hata("A5-K31", n->konum, yp("arac.esc"),
                         "pwm_min < notr − olu_bant ve notr + olu_bant < pwm_max olmalı");
            }
        }
    }
    // A5-K32 (öteki birim büyüklükler)
    for (const char* f : {"f1", "f2"}) {
        const std::string yol = std::string("arac.firlaticilar.") + f + ".tup_ekseni";
        birim_denetle(c, s.yol_bul(yol), yp(yol), false);
    }
    birim_denetle(c, s.yol_bul("baslangic.yonelim"), yp("baslangic.yonelim"), false);
    birim_denetle(c, s.yol_bul("ortam.sahne_poz.yonelim"), yp("ortam.sahne_poz.yonelim"), false);
    birim_denetle(c, s.yol_bul("ortam.isik.gunes_yonu_n"), yp("ortam.isik.gunes_yonu_n"), false);
    if (const Deger* h = s.bul("hedefler"); h != nullptr && h->tablo_mu()) {
        for (const auto& [ad, hd] : h->tablo) {
            if (!deneysel_mi(ad)) {
                birim_denetle(c, hd.bul("yonelim"), Yol().ekle("hedefler").ekle(ad).ekle("yonelim"), false);
            }
        }
    }
    // A5-K32 sensör montajları, A5-K33 tekillik
    std::map<std::int64_t, std::string> kimlikler;
    std::map<std::pair<std::string, std::int64_t>, std::string> tur_inst;
    for (const auto& [ad, sn] : sensorler(s, false)) {
        const Yol y = sensor_yolu(ad);
        const bool kamera = dizgi_esit(sn->bul("tur"), "kamera");
        if (const Deger* m = sn->bul("montaj"); m != nullptr) {
            birim_denetle(c, m->bul("yonelim"), y.ekle("montaj").ekle("yonelim"), kamera);
        }
        if (kamera) {
            if (const Deger* nm = sn->yol_bul("ayar.nominal_montaj"); nm != nullptr) {
                birim_denetle(c, nm->bul("yonelim"),
                              y.ekle("ayar").ekle("nominal_montaj").ekle("yonelim"), true);
            }
        }
        const Deger* k = sn->bul("kimlik");
        if (k != nullptr && k->tur == Tur::tamsayi && !kimlikler.emplace(k->tam, ad).second) {
            c.b.hata("A5-K33", k->konum, y.ekle("kimlik"),
                     "sensör kimliği tekil olmalı (" + kimlikler[k->tam] + " ile aynı)");
        }
        const Deger* t = sn->bul("tur");
        const Deger* in = sn->bul("inst");
        if (t != nullptr && in != nullptr && t->dizgi_mi() && in->tur == Tur::tamsayi &&
            !tur_inst.emplace(std::make_pair(t->dizgi, in->tam), ad).second) {
            c.b.hata("A5-K33", in->konum, y.ekle("inst"),
                     "(tur, inst) çifti tekil olmalı (" + tur_inst[{t->dizgi, in->tam}] + " ile aynı)");
        }
    }
}

// ------------------------------------------------------------------ §4.4 ortam
void ortam_kurallari(Baglam& c)
{
    const Deger& s = c.s;
    if (const Deger* sp = s.yol_bul("ortam.dalga.spektrum"); sp != nullptr && !dizgi_esit(sp, "yok")) {
        for (const char* k : {"hs", "tepe_periyodu_ns", "yon"}) {
            if (s.yol_bul(std::string("ortam.dalga.") + k) == nullptr) {
                c.b.hata("A5-K37", Konum{sp->konum.dosya, 0}, yp(std::string("ortam.dalga.") + k),
                         "dalga spektrumu varken zorunlu: " + std::string(k));
            }
        }
    }
    for (const char* a : {"akinti", "ruzgar"}) {
        const std::string taban = std::string("ortam.") + a + ".gauss_markov";
        const Deger* gm = s.yol_bul(taban);
        if (gm != nullptr && dogru_mu(gm->bul("etkin"))) {
            for (const char* k : {"tau_ns", "sigma", "sinir"}) {
                if (gm->bul(k) == nullptr) {
                    c.b.hata("A5-K38", Konum{gm->konum.dosya, 0}, yp(taban).ekle(k),
                             "Gauss–Markov bileşeni etkinken zorunlu: " + std::string(k));
                }
            }
        }
    }
    const auto r = sayilar(s.yol_bul("ortam.ruzgar.hiz_ned"));
    if (r.size() == 3 && r[2] != 0.0) {
        c.b.hata("A5-K39", s.yol_bul("ortam.ruzgar.hiz_ned")->konum, yp("ortam.ruzgar.hiz_ned"),
                 "rüzgâr hızının d bileşeni 0 olmalı");
    }
}

// ------------------------------------------------------------------ §4.5 sensörler
void sensor_kurallari(Baglam& c)
{
    const Deger& s = c.s;
    bool mag = false;
    bool orijin_gerekir = false;
    std::string orijin_nedeni;
    for (const auto& [ad, sn] : sensorler(s, false)) {
        const Yol y = sensor_yolu(ad);
        const bool etkin = dogru_mu(sn->bul("etkin"));
        const Deger* tur = sn->bul("tur");
        if (tur == nullptr || !tur->dizgi_mi()) {
            continue;
        }
        const std::string& t = tur->dizgi;
        const Deger* ayar = sn->bul("ayar");
        if (t == "gnss") {
            const Deger* h = ayar ? ayar->bul("yakinsama_baslangic_hatasi") : nullptr;
            if (h != nullptr && h->sayisal_mi() && h->cift() > 0.0 && ayar->bul("yakinsama_tau_ns") == nullptr) {
                c.b.hata("A5-K40", Konum{h->konum.dosya, 0}, y.ekle("ayar").ekle("yakinsama_tau_ns"),
                         "yakinsama_baslangic_hatasi > 0 iken yakinsama_tau_ns zorunlu");
            }
            if (etkin) {
                orijin_gerekir = true;
                orijin_nedeni = ad;
            }
        }
        if (t == "basinc") {
            const Deger* hz = sn->bul("hiz_hz");
            const Deger* p = ayar ? ayar->bul("donusum_penceresi_ns") : nullptr;
            const Deger* o = ayar ? ayar->bul("okuma_suresi_ns") : nullptr;
            if (hz && p && o && hz->sayisal_mi() && p->sayisal_mi() && o->sayisal_mi()) {
                const double sinir = 1e9 / (2.0 * p->cift() + o->cift());
                if (hz->cift() > sinir) {
                    c.b.hata("A5-K42", hz->konum, y.ekle("hiz_hz"),
                             "basınç örnekleme hızı 1/(2·dönüşüm penceresi + okuma süresi) = " +
                                 sayi_dizilimi(sinir) + " Hz'i aşıyor");
                }
            }
        }
        if (t == "manyetometre" && etkin) {
            mag = true;
            orijin_gerekir = true;
            orijin_nedeni = ad;
        }
        if (t == "imu") {
            if (etkin && ayar != nullptr && dogru_mu(ayar->bul("dunya_donusu_etkin"))) {
                orijin_gerekir = true;
                orijin_nedeni = ad;
            }
            if (ayar != nullptr && dogru_mu(ayar->bul("suzgec_etkin")) && ayar->bul("suzgec_tau_ns") == nullptr) {
                c.b.hata("A5-K46", Konum{ayar->konum.dosya, 0}, y.ekle("ayar").ekle("suzgec_tau_ns"),
                         "suzgec_etkin = true iken suzgec_tau_ns zorunlu");
            }
        }
    }
    const Konum ok{s.bul("ortam") ? s.bul("ortam")->konum.dosya : 0, 0};
    if (mag && s.yol_bul("ortam.manyetik_alan") == nullptr) {
        c.b.hata("A5-K44", ok, yp("ortam.manyetik_alan"),
                 "etkin bir manyetometre varken ortam.manyetik_alan zorunlu");
    }
    if (orijin_gerekir && s.yol_bul("ortam.orijin") == nullptr) {
        c.b.hata("A5-K45", ok, yp("ortam.orijin"),
                 "ortam.orijin zorunlu (etkin GNSS, manyetometre ya da dünya dönüşü açık IMU: " +
                     orijin_nedeni + ")");
    }
}

// ------------------------------------------------------------------ §4.6 otopilot
void otopilot_kurallari(Baglam& c)
{
    const Deger* p = c.s.yol_bul("otopilot.parametreler");
    if (p == nullptr || !p->dizi_mi()) {
        return;
    }
    std::set<std::int64_t> gorulen;
    for (std::size_t i = 0; i < p->dizi.size(); ++i) {
        const Deger& e = p->dizi[i];
        const Yol y = yp("otopilot.parametreler").ekle(i);
        const Deger* k = e.bul("kimlik");
        const Deger* t = e.bul("tip");
        const Deger* d = e.bul("deger");
        if (k != nullptr && k->tur == Tur::tamsayi && !gorulen.insert(k->tam).second) {
            c.b.hata("A5-K52", k->konum, y.ekle("kimlik"), "parametre kimliği dizide tekil olmalı");
        }
        if (t == nullptr || d == nullptr || !t->dizgi_mi() || !d->sayisal_mi()) {
            continue;
        }
        const double v = d->cift();
        bool tamam = true;
        if (t->dizgi == "i32" || t->dizgi == "u32") {
            const double alt = t->dizgi == "i32" ? -2147483648.0 : 0.0;
            const double ust = t->dizgi == "i32" ? 2147483647.0 : 4294967295.0;
            tamam = std::floor(v) == v && v >= alt && v <= ust;
        } else if (t->dizgi == "f32") {
            // binary32'ye en yakına çevrildiğinde sonlu: |v| < 2^128 − 2^103.
            tamam = std::abs(v) < 0x1.ffffffp127;
        }
        if (!tamam) {
            c.b.hata("A5-K52", d->konum, y.ekle("deger"),
                     "değer `" + t->dizgi + "` tipinin aralığında değil");
        }
    }
}

// ------------------------------------------------------------------ §4.7 görev
struct OgeTanimi {
    const char* ad;
    std::array<const char*, 3> cerceveler;  // nullptr: yok
    double derinlik_alt;
    bool derinlik_alt_dahil;
    double derinlik_ust;
    std::array<std::array<double, 2>, 4> p;  // [alt, ust]; alt > ust ise "tam 0" (aşağı bak)
    std::array<bool, 4> p_alt_dahil;
    std::array<bool, 4> p_sifir;
    std::array<bool, 4> p_tamsayi;
};

// A1 Tablo 3.6a (A1-K122).
const OgeTanimi kOgeler[] = {
    {"WAYPOINT", {"LOCAL", "GLOBAL", nullptr}, 0.0, true, 100.0,
     {{{0.1, 50.0}, {0.05, 3.0}, {0, 0}, {0, 0}}}, {true, true, true, true},
     {false, false, true, true}, {false, false, false, false}},
    {"DIVE_TO", {"NONE", nullptr, nullptr}, 0.0, false, 100.0,
     {{{0.05, 1.0}, {0.05, 5.0}, {0, 0}, {0, 0}}}, {true, true, true, true},
     {false, false, true, true}, {false, false, false, false}},
    {"SURFACE", {"NONE", nullptr, nullptr}, 0.0, true, 0.0,
     {{{0.05, 1.0}, {0, 0}, {0, 0}, {0, 0}}}, {true, true, true, true},
     {false, true, true, true}, {false, false, false, false}},
    {"HOLD", {"NONE", "LOCAL", "GLOBAL"}, 0.0, true, 100.0,
     {{{0.0, 3600.0}, {0.1, 50.0}, {0, 0}, {0, 0}}}, {false, true, true, true},
     {false, false, true, true}, {false, false, false, false}},
    {"SCAN_LINES", {"LOCAL", "GLOBAL", nullptr}, 0.0, true, 100.0,
     {{{1.0, 500.0}, {0.5, 100.0}, {1.0, 100.0}, {-kPi, kPi}}}, {true, true, true, false},
     {false, false, false, false}, {false, false, true, false}},
    {"SCAN_POINT", {"LOCAL", "GLOBAL", nullptr}, 0.0, true, 100.0,
     {{{0.0, 3600.0}, {0.1, 50.0}, {0, 0}, {0, 0}}}, {false, true, true, true},
     {false, false, true, true}, {false, false, false, false}},
    {"WAIT_GNSS", {"NONE", nullptr, nullptr}, 0.0, true, 0.0,
     {{{0.0, 3600.0}, {1.0, 4.0}, {0, 0}, {0, 0}}}, {false, true, true, true},
     {false, false, true, true}, {false, true, false, false}},
};

void gorev_kurallari(Baglam& c)
{
    const Deger* o = c.s.yol_bul("gorev.ogeler");
    if (o == nullptr || !o->dizi_mi()) {
        return;
    }
    for (std::size_t i = 0; i < o->dizi.size(); ++i) {
        const Deger& e = o->dizi[i];
        const Yol y = yp("gorev.ogeler").ekle(i);
        const Deger* tur = e.bul("item_type");
        const Deger* fr = e.bul("frame");
        if (tur == nullptr || fr == nullptr || !tur->dizgi_mi() || !fr->dizgi_mi()) {
            continue;
        }
        const OgeTanimi* t = nullptr;
        for (const auto& x : kOgeler) {
            if (tur->dizgi == x.ad) {
                t = &x;
            }
        }
        if (t == nullptr) {
            continue;  // şema reddeder
        }
        auto hata = [&](const Deger* d, const Yol& yy, const std::string& ileti) {
            c.b.hata("A5-K55", d != nullptr ? d->konum : e.konum, yy,
                     tur->dizgi + ": " + ileti + " (A1 Tablo 3.6a, A1-K122)");
        };
        bool cerceve_tamam = false;
        for (const char* f : t->cerceveler) {
            cerceve_tamam = cerceve_tamam || (f != nullptr && fr->dizgi == f);
        }
        if (!cerceve_tamam) {
            hata(fr, y.ekle("frame"), "bu tür için izinli olmayan frame: " + fr->dizgi);
        }
        const auto sayi = [&](const char* ad) {
            const Deger* d = e.bul(ad);
            return d != nullptr && d->sayisal_mi() ? d->cift() : 0.0;
        };
        const bool global = fr->dizgi == "GLOBAL";
        const bool local = fr->dizgi == "LOCAL";
        for (const char* ad : {"lat_e7", "lon_e7"}) {
            if (!global && sayi(ad) != 0.0) {
                hata(e.bul(ad), y.ekle(ad), std::string(ad) + " bu frame'de 0 olmalı");
            }
        }
        for (const char* ad : {"north_m", "east_m"}) {
            const double v = sayi(ad);
            if (!local && v != 0.0) {
                hata(e.bul(ad), y.ekle(ad), std::string(ad) + " bu frame'de 0 olmalı");
            } else if (local && !(std::abs(v) <= 10000.0)) {
                hata(e.bul(ad), y.ekle(ad), std::string(ad) + " mutlak değeri ≤ 10 000 m olmalı");
            }
        }
        const double d = sayi("depth");
        const bool d_alt = t->derinlik_alt_dahil ? d >= t->derinlik_alt : d > t->derinlik_alt;
        if (!d_alt || !(d <= t->derinlik_ust)) {
            hata(e.bul("depth"), y.ekle("depth"), "depth türün aralığı dışında");
        }
        const auto p = sayilar(e.bul("p"));
        for (std::size_t k = 0; k < 4 && k < p.size(); ++k) {
            const Yol pk = y.ekle("p").ekle(k);
            const double v = p[k];
            const Deger* pd = &e.bul("p")->dizi[k];
            if (t->p_sifir[k]) {
                if (v != 0.0) {
                    hata(pd, pk, "p[" + std::to_string(k) + "] tam 0 olmalı");
                }
                continue;
            }
            const double alt = t->p[k][0];
            const double ust = t->p[k][1];
            const bool alt_tamam = t->p_alt_dahil[k] ? v >= alt : v > alt;
            if (!std::isfinite(v) || !alt_tamam || !(v <= ust)) {
                hata(pd, pk, "p[" + std::to_string(k) + "] türün aralığı dışında");
            } else if (t->p_tamsayi[k] && std::floor(v) != v) {
                hata(pd, pk, "p[" + std::to_string(k) + "] tamsayı değerli olmalı");
            }
        }
    }
}

// ------------------------------------------------------------------ §4.7 olaylar
struct SinifTanimi {
    const char* sinif;
    std::vector<std::string_view> alanlar;  // ortak alanlar dışında izinli olanlar
};

const SinifTanimi kSiniflar[] = {
    {"sensor", {"tur", "hedef", "sure_ns", "siddet", "kanal_maskesi", "sira_boz", "zaman_deseni"}},
    {"aktuator", {"tur", "hedef", "sure_ns", "siddet"}},
    {"guc", {"tur", "sure_ns", "siddet"}},
    {"baglanti", {"tur", "yon", "sure_ns", "profil"}},
    {"protokol", {"tur", "yon", "duzlem", "mesaj_turu", "siddet", "sure_ns"}},
    {"sistem", {"tur", "problar"}},
    {"emniyet_girdisi", {"girdi", "deger", "hedef"}},
    {"gcs_komutu", {"komut", "kip"}},
};

struct SiddetTanimi {
    const char* tur;
    std::vector<std::string_view> zorunlu;
    std::vector<std::string_view> istege_bagli;
};

// Tablo 3.10-A (A5-K68).
const SiddetTanimi kSiddetler[] = {
    {"ofset", {"ofset"}, {}},
    {"olcek", {"olcek"}, {}},
    {"suruklenme_dogrusal", {"oran"}, {}},
    {"suruklenme_ustel", {"son_deger", "tau_ns"}, {}},
    {"gurultu_artisi", {"k"}, {}},
    {"sicrama", {"genlik", "p"}, {}},
    {"donma", {}, {}},
    {"donma_zaman_damgasi", {}, {}},
    {"yanlis_gecerlilik", {}, {}},
    {"kendini_bildiren", {}, {}},
    {"kayip", {}, {}},
    {"gec_teslim", {"gecikme_ns"}, {"titresim_ns"}},
    {"motor_takilmasi", {}, {}},
    {"dvl_isin_kaybi", {}, {}},
    {"guc_cevrimi", {}, {}},
    {"itki_carpani", {"carpan"}, {}},
    {"esc_sinyal_kaybi", {}, {}},
    {"ic_direnc_artisi", {"carpan"}, {}},
    {"bara_kopmasi", {}, {}},
    {"kesinti", {}, {}},
    {"profil_degisimi", {}, {}},
    {"dusurme", {"p"}, {}},
    {"cogaltma", {"p"}, {}},
    {"yer_degistirme", {"p"}, {}},
    {"bayt_bozma", {"p"}, {}},
    {"kesme", {"p"}, {}},
    {"kart_sifirlama", {}, {}},
    {"su_girisi", {}, {}},
};

const std::set<std::string_view> kGurultuArtisiTurleri = {"imu", "manyetometre", "basinc",
                                                          "gnss", "dvl", "batarya_olcer"};

std::string_view yasak_alan_kurali(std::string_view alan)
{
    if (alan == "siddet") return "A5-K68";
    if (alan == "zaman_deseni" || alan == "sira_boz" || alan == "kanal_maskesi") return "A5-K67";
    if (alan == "kip") return "A5-K72";
    if (alan == "sure_ns") return "A5-K109";
    return "A5-K65";
}

// Emniyet ataması: (alan, kısıtlayan mı); alan "acil_durdurma", "fis.f1" ...
std::optional<std::pair<std::string, bool>> emniyet_atamasi(const Deger& e)
{
    const Deger* g = e.bul("girdi");
    const Deger* d = e.bul("deger");
    if (g == nullptr || d == nullptr || !g->dizgi_mi() || !d->mantiksal_mi()) {
        return std::nullopt;
    }
    std::string alan = g->dizgi;
    if (const Deger* h = e.bul("hedef"); h != nullptr && h->dizgi_mi()) {
        alan += "." + h->dizgi;
    }
    // Kısıtlayan: acil durdurma basılı, fiş çıkık, pim takılı (A5-K62).
    bool kisitlayan = false;
    if (g->dizgi == "acil_durdurma") {
        kisitlayan = d->mantik;
    } else if (g->dizgi == "fis") {
        kisitlayan = !d->mantik;
    } else {
        kisitlayan = d->mantik;
    }
    return std::make_pair(alan, kisitlayan);
}

std::int64_t sinir_ns(std::int64_t zaman_ns)
{
    // `t ≥ zaman_ns` olan ilk fizik adım sınırı (A5-K62).
    return ((zaman_ns + kFizikAdimiNs - 1) / kFizikAdimiNs) * kFizikAdimiNs;
}

void olay_kurallari(Baglam& c)
{
    const Deger& s = c.s;
    const Deger* ol = s.bul("olaylar");
    if (ol == nullptr || !ol->dizi_mi()) {
        return;
    }
    std::map<std::string, std::string> sensor_turu;
    for (const auto& [ad, sn] : sensorler(s, false)) {
        if (const Deger* t = sn->bul("tur"); t != nullptr && t->dizgi_mi()) {
            sensor_turu[ad] = t->dizgi;
        }
    }
    std::optional<std::int64_t> prob_sayisi;
    for (const auto& [ad, sn] : sensorler(s, false)) {
        if (dizgi_esit(sn->bul("tur"), "sizinti")) {
            if (const Deger* p = sn->yol_bul("ayar.prob_sayisi"); p != nullptr && p->tur == Tur::tamsayi) {
                prob_sayisi = prob_sayisi ? std::min(*prob_sayisi, p->tam) : p->tam;
            }
        }
    }
    std::set<std::string> kimlikler;
    // A5-K62: sınır → (alan, kısıtlayan, olay)
    std::map<std::int64_t, std::vector<std::tuple<std::string, bool, std::size_t>>> emniyet;
    std::map<std::int64_t, std::vector<std::pair<std::string, std::size_t>>> profil_atamalari;

    for (std::size_t i = 0; i < ol->dizi.size(); ++i) {
        const Deger& e = ol->dizi[i];
        if (!e.tablo_mu()) {
            continue;
        }
        const Yol y = Yol().ekle("olaylar").ekle(i);
        auto hata = [&](std::string_view kural, const Deger* d, const Yol& yy, std::string ileti) {
            c.b.hata(kural, d != nullptr ? d->konum : e.konum, yy, std::move(ileti));
        };
        // A5-K60
        if (const Deger* k = e.bul("kimlik"); k != nullptr && k->dizgi_mi()) {
            if (!kimlikler.insert(k->dizgi).second) {
                hata("A5-K60", k, y.ekle("kimlik"), "olay kimliği tekil olmalı: " + k->dizgi);
            }
            if (k->dizgi.size() >= 2 && k->dizgi[0] == 'E' &&
                k->dizgi.find_first_not_of("0123456789", 1) == std::string::npos) {
                hata("A5-K60", k, y.ekle("kimlik"), "`E<n>` kimlikleri elle enjeksiyona ayrılmıştır");
            }
        }
        const Deger* sd = e.bul("sinif");
        if (sd == nullptr || !sd->dizgi_mi()) {
            continue;
        }
        const std::string& sinif = sd->dizgi;
        const Deger* td = e.bul("tur");
        const std::string tur = td != nullptr && td->dizgi_mi() ? td->dizgi : std::string();
        // izinli alanlar
        const SinifTanimi* st = nullptr;
        for (const auto& x : kSiniflar) {
            if (sinif == x.sinif) {
                st = &x;
            }
        }
        if (st == nullptr) {
            continue;
        }
        for (const auto& [k, v] : e.tablo) {
            if (deneysel_mi(k) || k == "kimlik" || k == "sinif" || k == "zaman_ns" || k == "kosul") {
                continue;
            }
            if (std::find(st->alanlar.begin(), st->alanlar.end(), k) == st->alanlar.end()) {
                hata(yasak_alan_kurali(k), &v, y.ekle(k),
                     "`" + k + "` alanı `" + sinif + "` sınıfında bulunamaz");
            }
        }
        // hedef (A5-K65)
        const Deger* hedef = e.bul("hedef");
        if (sinif == "sensor") {
            if (hedef == nullptr || !hedef->dizgi_mi() || sensor_turu.count(hedef->dizgi) == 0) {
                hata("A5-K65", hedef, y.ekle("hedef"),
                     "sensör arızasının hedefi arac.sensorler'de bir sensör olmalı");
            } else {
                const std::string& htur = sensor_turu[hedef->dizgi];
                if (tur == "motor_takilmasi" && htur != "ping360") {
                    hata("A5-K65", hedef, y.ekle("hedef"), "motor_takilmasi yalnız ping360 hedefinde geçerli");
                }
                if (tur == "dvl_isin_kaybi" && htur != "dvl") {
                    hata("A5-K65", hedef, y.ekle("hedef"), "dvl_isin_kaybi yalnız dvl hedefinde geçerli");
                }
                if (tur == "gurultu_artisi" && kGurultuArtisiTurleri.count(htur) == 0) {
                    hata("A5-K65", hedef, y.ekle("hedef"),
                         "gurultu_artisi yalnız Tablo 3.10-A'daki sensör türlerinde geçerli");
                }
            }
        } else if (sinif == "aktuator") {
            static const std::set<std::string> iticiler = {"t1", "t2", "t3", "t4", "t5",
                                                           "t6", "t7", "t8", "tumu"};
            if (hedef == nullptr || !hedef->dizgi_mi() || iticiler.count(hedef->dizgi) == 0) {
                hata("A5-K65", hedef, y.ekle("hedef"), "aktüatör arızasının hedefi t1…t8 ya da tumu olmalı");
            }
        } else if (sinif == "emniyet_girdisi") {
            const Deger* g = e.bul("girdi");
            const bool tuplu = g != nullptr && g->dizgi_mi() && (g->dizgi == "fis" || g->dizgi == "pim");
            if (tuplu && (hedef == nullptr || !(dizgi_esit(hedef, "f1") || dizgi_esit(hedef, "f2")))) {
                hata("A5-K65", hedef, y.ekle("hedef"), "fis ve pim atamasının hedefi f1 ya da f2 olmalı");
            }
            if (!tuplu && hedef != nullptr) {
                hata("A5-K65", hedef, y.ekle("hedef"), "acil_durdurma atamasında hedef bulunamaz");
            }
        }
        if (sinif == "baglanti" || sinif == "protokol") {
            if (e.bul("yon") == nullptr) {
                hata("A5-K65", nullptr, y.ekle("yon"), "`" + sinif + "` olayında yon zorunlu");
            }
        }
        if (sinif == "protokol" && e.bul("duzlem") == nullptr) {
            hata("A5-K65", nullptr, y.ekle("duzlem"), "protokol olayında duzlem zorunlu");
        }
        if (sinif == "baglanti") {
            const Deger* p = e.bul("profil");
            if (tur == "profil_degisimi") {
                if (p == nullptr || !p->dizgi_mi() || deneysel_mi(p->dizgi) ||
                    s.yol_bul("baglanti.profiller") == nullptr ||
                    s.yol_bul("baglanti.profiller")->bul(p->dizgi) == nullptr) {
                    hata("A5-K65", p, y.ekle("profil"), "profil_degisimi mevcut bir bağlantı profili ister");
                }
            } else if (p != nullptr) {
                hata("A5-K65", p, y.ekle("profil"), "profil yalnız profil_degisimi olayında bulunur");
            }
        }
        if (sinif == "sistem") {
            const Deger* p = e.bul("problar");
            if (tur == "su_girisi") {
                if (p == nullptr) {
                    hata("A5-K65", nullptr, y.ekle("problar"), "su_girisi olayında problar zorunlu");
                } else if (!prob_sayisi) {
                    hata("A5-K65", p, y.ekle("problar"), "su_girisi için sızıntı sensörü yok");
                } else {
                    for (const auto& x : p->dizi) {
                        if (x.tur == Tur::tamsayi && x.tam >= *prob_sayisi) {
                            hata("A5-K65", &x, y.ekle("problar"),
                                 "prob numarası prob sayısından (" + std::to_string(*prob_sayisi) +
                                     ") küçük olmalı");
                        }
                    }
                }
            } else if (p != nullptr) {
                hata("A5-K65", p, y.ekle("problar"), "problar yalnız su_girisi olayında bulunur");
            }
        }
        // şiddet (A5-K68)
        if (!tur.empty()) {
            const SiddetTanimi* sdt = nullptr;
            for (const auto& x : kSiddetler) {
                if (tur == x.tur) {
                    sdt = &x;
                }
            }
            const Deger* sid = e.bul("siddet");
            if (sdt != nullptr) {
                for (const auto z : sdt->zorunlu) {
                    if (sid == nullptr || sid->bul(z) == nullptr) {
                        hata("A5-K68", nullptr, y.ekle("siddet").ekle(std::string(z)),
                             "`" + tur + "` için zorunlu şiddet alanı eksik: " + std::string(z));
                    }
                }
                if (sid != nullptr && sid->tablo_mu()) {
                    for (const auto& [k, v] : sid->tablo) {
                        if (deneysel_mi(k)) {
                            continue;
                        }
                        const bool izinli =
                            std::find(sdt->zorunlu.begin(), sdt->zorunlu.end(), k) != sdt->zorunlu.end() ||
                            std::find(sdt->istege_bagli.begin(), sdt->istege_bagli.end(), k) !=
                                sdt->istege_bagli.end();
                        if (!izinli) {
                            hata("A5-K68", &v, y.ekle("siddet").ekle(k),
                                 "`" + tur + "` bu şiddet alanını taşıyamaz: " + k);
                        }
                    }
                    if (const Deger* cp = sid->bul("carpan"); cp != nullptr && cp->sayisal_mi()) {
                        if (tur == "itki_carpani" && !(cp->cift() >= 0.0 && cp->cift() <= 1.0)) {
                            hata("A5-K68", cp, y.ekle("siddet").ekle("carpan"), "itki_carpani çarpanı [0, 1] olmalı");
                        }
                        if (tur == "ic_direnc_artisi" && !(cp->cift() >= 1.0)) {
                            hata("A5-K68", cp, y.ekle("siddet").ekle("carpan"), "ic_direnc_artisi çarpanı ≥ 1 olmalı");
                        }
                    }
                    // A5-K66
                    if (tur == "gec_teslim") {
                        const Deger* g = sid->bul("gecikme_ns");
                        const Deger* j = sid->bul("titresim_ns");
                        if (g != nullptr && j != nullptr && g->sayisal_mi() && j->sayisal_mi() &&
                            !(j->cift() <= g->cift())) {
                            hata("A5-K66", j, y.ekle("siddet").ekle("titresim_ns"), "titresim_ns ≤ gecikme_ns olmalı");
                        }
                    }
                }
            }
            if (tur == "guc_cevrimi") {
                const Deger* su = e.bul("sure_ns");
                if (su == nullptr || !su->sayisal_mi() || !(su->cift() > 0.0)) {
                    hata("A5-K68", su, y.ekle("sure_ns"), "guc_cevrimi sure_ns > 0 ister");
                }
            }
        }
        // A5-K67
        if (sinif == "sensor") {
            if (const Deger* zd = e.bul("zaman_deseni"); zd != nullptr && zd->tablo_mu()) {
                const Deger* zt = zd->bul("tur");
                if (dizgi_esit(zt, "bernoulli") && zd->bul("p") == nullptr) {
                    hata("A5-K67", nullptr, y.ekle("zaman_deseni").ekle("p"), "bernoulli deseni p ister");
                }
                if (dizgi_esit(zt, "gilbert_elliott")) {
                    for (const char* k : {"t_iyi_ns", "t_kotu_ns"}) {
                        if (zd->bul(k) == nullptr) {
                            hata("A5-K67", nullptr, y.ekle("zaman_deseni").ekle(k),
                                 std::string("gilbert_elliott deseni ") + k + " ister");
                        }
                    }
                }
            }
            // A5-K69
            if (const Deger* km = e.bul("kanal_maskesi"); km != nullptr && km->tur == Tur::tamsayi) {
                if (tur == "dvl_isin_kaybi") {
                    if (!(km->tam >= 1 && km->tam <= 15)) {
                        hata("A5-K69", km, y.ekle("kanal_maskesi"), "dvl_isin_kaybi maskesi 1…15 olmalı");
                    }
                } else if (km->tam != 4294967295LL) {
                    hata("A5-K69", km, y.ekle("kanal_maskesi"),
                         "bu sürümde kanal_maskesi yalnız 4294967295 (bütün kanallar) olabilir");
                }
            }
        }
        // A5-K72
        if (sinif == "gcs_komutu") {
            const bool set_mode = dizgi_esit(e.bul("komut"), "SET_MODE");
            if (set_mode && e.bul("kip") == nullptr) {
                hata("A5-K72", nullptr, y.ekle("kip"), "SET_MODE komutunda kip zorunlu");
            }
            if (!set_mode && e.bul("kip") != nullptr) {
                hata("A5-K72", e.bul("kip"), y.ekle("kip"), "kip yalnız SET_MODE komutunda bulunur");
            }
        }
        // A5-K62, K71
        if (sinif == "emniyet_girdisi") {
            if (const auto a = emniyet_atamasi(e)) {
                if (!a->second) {
                    c.b.uyari("A5-K71", e.konum, y,
                              "emniyeti azaltan zaman çizelgesi ataması: " + a->first);
                }
                if (const Deger* z = e.bul("zaman_ns"); z != nullptr && z->tur == Tur::tamsayi) {
                    emniyet[sinir_ns(z->tam)].emplace_back(a->first, a->second, i);
                }
            }
        }
        if (sinif == "baglanti" && tur == "profil_degisimi") {
            const Deger* z = e.bul("zaman_ns");
            const Deger* p = e.bul("profil");
            if (z != nullptr && z->tur == Tur::tamsayi && p != nullptr && p->dizgi_mi()) {
                profil_atamalari[sinir_ns(z->tam)].emplace_back(p->dizgi, i);
            }
        }
    }
    for (const auto& [t, grup] : emniyet) {
        const bool kisit = std::any_of(grup.begin(), grup.end(), [](const auto& x) { return std::get<1>(x); });
        const bool gevset = std::any_of(grup.begin(), grup.end(), [](const auto& x) { return !std::get<1>(x); });
        if (kisit && gevset) {
            for (const auto& x : grup) {
                if (!std::get<1>(x)) {
                    const std::size_t i = std::get<2>(x);
                    c.b.hata("A5-K62", ol->dizi[i].konum, Yol().ekle("olaylar").ekle(i),
                             "aynı fizik sınırında (" + std::to_string(t) +
                                 " ns) kısıtlayan ve gevşeten zamanlı emniyet ataması");
                }
            }
        }
    }
    for (const auto& [t, grup] : profil_atamalari) {
        for (std::size_t k = 1; k < grup.size(); ++k) {
            if (grup[k].first != grup[0].first) {
                const std::size_t i = grup[k].second;
                c.b.hata("A5-K62", ol->dizi[i].konum, Yol().ekle("olaylar").ekle(i),
                         "aynı fizik sınırında (" + std::to_string(t) +
                             " ns) bağlantı profiline farklı değer atayan zamanlı atamalar");
            }
        }
    }
}

// ------------------------------------------------------------------ §4.8 bağlantı ve kip
void baglanti_kurallari(Baglam& c)
{
    const Deger& s = c.s;
    const Deger* b = s.bul("baglanti");
    if (b == nullptr || !b->tablo_mu()) {
        return;
    }
    const Deger* pr = b->bul("profiller");
    const Deger* ep = b->bul("etkin_profil");
    if (ep != nullptr && ep->dizgi_mi() &&
        (pr == nullptr || deneysel_mi(ep->dizgi) || pr->bul(ep->dizgi) == nullptr)) {
        c.b.hata("A5-K74", ep->konum, yp("baglanti.etkin_profil"),
                 "etkin_profil profiller içinde bulunmalı: " + ep->dizgi);
    }
    if (pr == nullptr || !pr->tablo_mu()) {
        return;
    }
    for (const auto& [ad, p] : pr->tablo) {
        if (deneysel_mi(ad) || !p.tablo_mu()) {
            continue;
        }
        const Yol y = yp("baglanti.profiller").ekle(ad);
        const Deger* kademe = p.bul("kademe");
        const bool stream = dizgi_esit(p.bul("tasima"), "stream");
        if (stream && p.bul("stream") == nullptr) {
            c.b.hata("A5-K74", Konum{p.konum.dosya, 0}, y.ekle("stream"), "stream taşımada stream tablosu zorunlu");
        }
        for (const char* yon : {"yukari", "asagi"}) {
            const Deger* d = p.bul(yon);
            if (d == nullptr || !d->tablo_mu()) {
                continue;
            }
            const Yol yy = y.ekle(yon);
            const Deger* km = d->bul("kayip_modeli");
            if (dizgi_esit(km, "bernoulli") && d->bul("kayip_p") == nullptr) {
                c.b.hata("A5-K74", Konum{d->konum.dosya, 0}, yy.ekle("kayip_p"), "bernoulli kayıp modeli kayip_p ister");
            }
            if (dizgi_esit(km, "gilbert_elliott")) {
                for (const char* k : {"ge_lambda_gb", "ge_lambda_bg", "ge_k", "ge_h"}) {
                    if (d->bul(k) == nullptr) {
                        c.b.hata("A5-K74", Konum{d->konum.dosya, 0}, yy.ekle(k),
                                 std::string("gilbert_elliott kayıp modeli ") + k + " ister");
                    }
                }
            }
            if (stream) {
                for (const char* k : {"bayt_bozma_orani", "hava_paketi"}) {
                    if (const Deger* x = d->bul(k); x != nullptr) {
                        c.b.hata("A5-K74", x->konum, yy.ekle(k), std::string("stream taşımada ") + k + " bulunamaz");
                    }
                }
            }
            if (dizgi_esit(kademe, "L0")) {
                for (const auto& [k, v] : d->tablo) {
                    if (deneysel_mi(k)) {
                        continue;
                    }
                    const bool varsayilan = (k == "gecikme_taban_ns" && v.sayisal_mi() && v.cift() == 0.0) ||
                                            (k == "titresim_ns" && v.sayisal_mi() && v.cift() == 0.0) ||
                                            (k == "kayip_modeli" && dizgi_esit(&v, "yok"));
                    if (!varsayilan) {
                        c.b.hata("A5-K74", v.konum, yy.ekle(k),
                                 "L0 kademesinde yön tablosunda yalnız varsayılan değerler bulunabilir");
                    }
                }
            }
            if (dizgi_esit(kademe, "L1") && dizgi_esit(km, "gilbert_elliott")) {
                c.b.hata("A5-K74", km->konum, yy.ekle("kayip_modeli"), "L1 kademesinde gilbert_elliott yasak");
            }
            if (dizgi_esit(kademe, "L2") && dizgi_esit(km, "bernoulli")) {
                c.b.hata("A5-K74", km->konum, yy.ekle("kayip_modeli"), "L2 kademesinde bernoulli yasak");
            }
        }
    }
}

void calistirma_kurallari(Baglam& c)
{
    const Deger& s = c.s;
    const Deger* kip = s.yol_bul("calistirma.kip");
    if (kip != nullptr && kip->dizgi_mi() && kip->dizgi != "sil") {
        for (const char* k : {"sil_hiz_carpani", "sil_azami_hiz"}) {
            if (const Deger* x = s.yol_bul(std::string("calistirma.") + k); x != nullptr) {
                c.b.hata("A5-K76", x->konum, yp(std::string("calistirma.") + k),
                         std::string("HIL kiplerinde ") + k + " verilemez");
            }
        }
    }
}

// ------------------------------------------------------------------ §4.9 kalite, §4.13 kamera
void kalite_kurallari(Baglam& c)
{
    const Deger& s = c.s;
    const Deger* sonar = s.yol_bul("kalite.sonar");
    const bool hil = !dizgi_esit(s.yol_bul("calistirma.kip"), "sil");
    if (dizgi_esit(sonar, "tam")) {
        const Deger* n = s.yol_bul("kalite.sonar_tam.isin_sayisi");
        if (n == nullptr) {
            c.b.hata("A5-K79", Konum{sonar->konum.dosya, 0}, yp("kalite.sonar_tam.isin_sayisi"),
                     "tam sonar kademesinde isin_sayisi zorunlu");
        } else if (n->tur == Tur::tamsayi) {
            const bool olculmus = n->tam == 20000 || n->tam == 40000 || n->tam == 80000 || n->tam == 160000;
            const Yol y = yp("kalite.sonar_tam.isin_sayisi");
            if (hil && !olculmus) {
                c.b.hata("A5-K79", n->konum, y,
                         "kartlı kipte ışın sayısı yalnız ölçülmüş kümeden seçilir: 20000, 40000, 80000, 160000");
            } else if (hil) {
                c.b.uyari("A5-K79", n->konum, y, "kartlı kipte tam sonar kademesi (ölçülmüş ışın sayısı)");
            } else if (!olculmus) {
                c.b.uyari("A5-K79", n->konum, y, "ölçülmemiş ışın sayısı (MP7 kümesinde değil)");
            }
        }
    }
    const Deger* gorunum = s.yol_bul("kalite.gorunum");
    const Deger* kamera = s.yol_bul("kalite.kamera");
    const auto etkin_kameralar = [&] {
        std::vector<std::pair<std::string, const Deger*>> o;
        for (const auto& x : sensorler(s, true)) {
            if (dizgi_esit(x.second->bul("tur"), "kamera")) {
                o.push_back(x);
            }
        }
        return o;
    }();
    if (dizgi_esit(gorunum, "kapali") && kamera != nullptr && !dizgi_esit(kamera, "kapali")) {
        c.b.hata("A5-K80", kamera->konum, yp("kalite.kamera"), "3B görünüm kapalıyken kamera kapalı olmalı");
    }
    if (dizgi_esit(kamera, "kapali")) {
        for (const auto& [ad, sn] : etkin_kameralar) {
            c.b.hata("A5-K80", sn->konum, sensor_yolu(ad).ekle("etkin"),
                     "kamera kademesi kapalıyken etkin kamera sensörü bulunamaz");
        }
    } else if (kamera != nullptr && etkin_kameralar.empty()) {
        c.b.hata("A5-K80", kamera->konum, yp("kalite.kamera"),
                 "kamera kademesi açıkken tam olarak bir etkin kamera sensörü bulunmalı");
    }
    if (dogru_mu(s.yol_bul("kalite.kamera_gerekli")) &&
        (dizgi_esit(kamera, "kapali") || etkin_kameralar.empty())) {
        c.b.hata("A5-K80", s.yol_bul("kalite.kamera_gerekli")->konum, yp("kalite.kamera_gerekli"),
                 "kamera_gerekli = true iken kamera kademesi açık ve bir etkin kamera olmalı");
    }
    // A5-K114
    if (etkin_kameralar.size() > 1) {
        for (std::size_t i = 1; i < etkin_kameralar.size(); ++i) {
            c.b.hata("A5-K114", etkin_kameralar[i].second->konum, sensor_yolu(etkin_kameralar[i].first),
                     "en çok bir kamera sensörü etkin olabilir");
        }
    }
    for (const auto& [ad, sn] : sensorler(s, false)) {
        if (!dizgi_esit(sn->bul("tur"), "kamera")) {
            continue;
        }
        const Yol y = sensor_yolu(ad);
        const bool etkin = dogru_mu(sn->bul("etkin"));
        const Deger* ayar = sn->bul("ayar");
        if (etkin) {
            if (const Deger* in = sn->bul("inst"); in != nullptr && in->tur == Tur::tamsayi && in->tam != 0) {
                c.b.hata("A5-K114", in->konum, y.ekle("inst"), "etkin kameranın inst değeri 0 olmalı");
            }
            for (const char* k : {"ic_tam", "ic_sade", "pozlama_ns", "yakin_m", "uzak_m"}) {
                if (ayar == nullptr || ayar->bul(k) == nullptr) {
                    c.b.hata("A5-K114", Konum{sn->konum.dosya, 0}, y.ekle("ayar").ekle(k),
                             std::string("etkin kamerada zorunlu: ") + k);
                }
            }
        }
        if (ayar == nullptr) {
            continue;
        }
        // A2 §3.6 aralıkları (tek kaynak).
        const struct {
            const char* ad;
            double g;
            double h;
        } kademeler[] = {{"ic_tam", 640.0, 480.0}, {"ic_sade", 320.0, 240.0}};
        for (const auto& k : kademeler) {
            const Deger* ic = ayar->bul(k.ad);
            if (ic == nullptr) {
                continue;
            }
            const Deger* cx = ic->bul("cx");
            const Deger* cy = ic->bul("cy");
            if (cx != nullptr && cx->sayisal_mi() && !(cx->cift() >= -0.5 && cx->cift() <= k.g - 0.5)) {
                c.b.hata("A5-K114", cx->konum, y.ekle("ayar").ekle(k.ad).ekle("cx"),
                         "cx −0,5 ≤ cx ≤ genişlik − 0,5 olmalı (A2 §3.6)");
            }
            if (cy != nullptr && cy->sayisal_mi() && !(cy->cift() >= -0.5 && cy->cift() <= k.h - 0.5)) {
                c.b.hata("A5-K114", cy->konum, y.ekle("ayar").ekle(k.ad).ekle("cy"),
                         "cy −0,5 ≤ cy ≤ yükseklik − 0,5 olmalı (A2 §3.6)");
            }
        }
        if (dizgi_esit(ayar->bul("bozulma_modeli"), "yok")) {
            const auto bz = sayilar(ayar->bul("bozulma"));
            if (std::any_of(bz.begin(), bz.end(), [](double x) { return x != 0.0; })) {
                c.b.hata("A5-K114", ayar->bul("bozulma")->konum, y.ekle("ayar").ekle("bozulma"),
                         "bozulma modeli yokken katsayılar 0 olmalı");
            }
        }
        if (const Deger* pz = ayar->bul("pozlama_ns"); pz != nullptr && pz->tur == Tur::tamsayi &&
                                                       (pz->tam <= 0 || pz->tam % 2 != 0 || pz->tam > 1000000000)) {
            c.b.hata("A5-K114", pz->konum, y.ekle("ayar").ekle("pozlama_ns"),
                     "pozlama_ns > 0, çift ve ≤ 1 000 000 000 olmalı (A2 §3.6)");
        }
        const Deger* yk = ayar->bul("yakin_m");
        const Deger* uz = ayar->bul("uzak_m");
        if (yk != nullptr && uz != nullptr && yk->sayisal_mi() && uz->sayisal_mi() &&
            !(yk->cift() > 0.0 && yk->cift() < uz->cift())) {
            c.b.hata("A5-K114", uz->konum, y.ekle("ayar").ekle("uzak_m"), "0 < yakin_m < uzak_m olmalı");
        }
        const auto konum_denetle = [&](const Deger* m, const Yol& my) {
            const auto v = sayilar(m != nullptr ? m->bul("konum") : nullptr);
            if (std::any_of(v.begin(), v.end(), [](double x) { return !(std::abs(x) <= 10.0); })) {
                c.b.hata("A5-K114", m->bul("konum")->konum, my.ekle("konum"),
                         "kamera montaj konumunun her bileşeni |·| ≤ 10 m olmalı (A2 §3.6)");
            }
        };
        konum_denetle(sn->bul("montaj"), y.ekle("montaj"));
        konum_denetle(ayar->bul("nominal_montaj"), y.ekle("ayar").ekle("nominal_montaj"));
    }
}

// ------------------------------------------------------------------ §4.11 başlangıç, K71
void baslangic_kurallari(Baglam& c)
{
    const Deger& s = c.s;
    if (const Deger* yt = s.yol_bul("baslangic.yuklu_tupler"); yt != nullptr && yt->dizi_mi()) {
        std::set<std::string> g;
        for (std::size_t i = 0; i < yt->dizi.size(); ++i) {
            if (yt->dizi[i].dizgi_mi() && !g.insert(yt->dizi[i].dizgi).second) {
                c.b.hata("A5-K86", yt->dizi[i].konum, yp("baslangic.yuklu_tupler").ekle(i), "yinelenen tüp");
            }
        }
    }
    for (const char* f : {"f1", "f2"}) {
        const Deger* fis = s.yol_bul(std::string("emniyet_girdileri.fis.") + f);
        const Deger* pim = s.yol_bul(std::string("emniyet_girdileri.pim.") + f);
        if (dogru_mu(fis)) {
            c.b.uyari("A5-K71", fis->konum, yp(std::string("emniyet_girdileri.fis.") + f),
                      "emniyeti azaltan başlangıç durumu: fiş takılı");
        }
        if (pim != nullptr && pim->mantiksal_mi() && !pim->mantik) {
            c.b.uyari("A5-K71", pim->konum, yp(std::string("emniyet_girdileri.pim.") + f),
                      "emniyeti azaltan başlangıç durumu: pim çıkık");
        }
    }
}

// ------------------------------------------------------------------ A7 atıfları
void a7_kurallari(Baglam& c, const Deger& m)
{
    const Deger& s = c.s;
    std::map<std::string, const Deger*> varliklar;
    if (const Deger* v = m.bul("varliklar"); v != nullptr && v->dizi_mi()) {
        for (const auto& x : v->dizi) {
            if (const Deger* k = x.bul("kimlik"); k != nullptr && k->dizgi_mi()) {
                varliklar[k->dizgi] = &x;
            }
        }
    }
    std::set<std::string> malzemeler;
    if (const Deger* v = m.bul("malzemeler"); v != nullptr && v->dizi_mi()) {
        for (const auto& x : v->dizi) {
            if (const Deger* k = x.bul("kimlik"); k != nullptr && k->dizgi_mi()) {
                malzemeler.insert(k->dizgi);
            }
        }
    }
    auto atif = [&](const Deger* d, const Yol& y, std::string_view tur) -> const Deger* {
        if (d == nullptr || !d->dizgi_mi()) {
            return nullptr;
        }
        const auto it = varliklar.find(d->dizgi);
        if (it == varliklar.end()) {
            c.b.hata("A5-K19", d->konum, y, "A7 kümesinde böyle bir varlık yok: " + d->dizgi);
            return nullptr;
        }
        if (!dizgi_esit(it->second->bul("tur"), tur)) {
            c.b.hata("A5-K19", d->konum, y,
                     "A7 varlığı `" + std::string(tur) + "` türünde değil: " + d->dizgi);
            return nullptr;
        }
        return it->second;
    };
    atif(s.yol_bul("arac.varlik"), yp("arac.varlik"), "arac");
    atif(s.yol_bul("arac.torpido.varlik"), yp("arac.torpido.varlik"), "torpido");
    if (const Deger* h = s.bul("hedefler"); h != nullptr && h->tablo_mu()) {
        for (const auto& [ad, hd] : h->tablo) {
            if (!deneysel_mi(ad)) {
                atif(hd.bul("varlik"), Yol().ekle("hedefler").ekle(ad).ekle("varlik"), "hedef");
            }
        }
    }
    const Deger* statik = atif(s.yol_bul("ortam.sahne"), yp("ortam.sahne"), "statik");
    // A5-K18
    if (const Deger* km = s.yol_bul("kalibrasyon.malzeme"); km != nullptr && km->tablo_mu()) {
        for (const auto& [ad, v] : km->tablo) {
            if (!deneysel_mi(ad) && malzemeler.count(ad) == 0) {
                c.b.hata("A5-K18", v.konum, yp("kalibrasyon.malzeme").ekle(ad),
                         "A7 manifestinde böyle bir malzeme yok: " + ad);
            }
        }
    }
    // A5-K36
    if (statik != nullptr) {
        const Deger* t = statik->bul("taban_derinligi_m");
        const Deger* der = s.yol_bul("ortam.derinlik");
        if (t == nullptr || !t->sayisal_mi()) {
            c.b.uyari("A5-K36", s.yol_bul("ortam.sahne")->konum, yp("ortam.sahne"),
                      "statik varlıkta taban_derinligi_m yok; ortam.derinlik denetlenemedi");
        } else if (der != nullptr && der->sayisal_mi()) {
            const auto q = sayilar(s.yol_bul("ortam.sahne_poz.yonelim"));
            const auto p = sayilar(s.yol_bul("ortam.sahne_poz.konum"));
            if (q.size() == 4 && (q[1] != 0.0 || q[2] != 0.0)) {
                c.b.hata("A5-K36", s.yol_bul("ortam.sahne_poz.yonelim")->konum, yp("ortam.sahne_poz.yonelim"),
                         "taban beyanı varken statik poz yalnız düşey eksen etrafında dönebilir (x = y = 0)");
            } else if (p.size() == 3) {
                const double taban = p[2] + t->cift();
                if (!(std::abs(taban - der->cift()) <= 0.001)) {
                    c.b.hata("A5-K36", der->konum, yp("ortam.derinlik"),
                             "pozlu taban derinliği (" + sayi_dizilimi(taban) +
                                 " m) ortam.derinlik'ten 1 mm'den çok farklı");
                }
            }
        }
    }
}

}  // namespace

void desteklenmeyenler(const Deger& s, Bulgucu& b)
{
    const char* ds = "bu sürümde desteklenmiyor";
    for (const char* yol : {"arac.batarya.sigorta_i2t", "arac.batarya.hucre", "arac.anten_konumu"}) {
        if (const Deger* d = s.yol_bul(yol); d != nullptr) {
            b.hata("A5-K102", d->konum, yp(yol), std::string(ds) + ": " + yol);
        }
    }
    if (const Deger* k = s.yol_bul("arac.batarya.kip"); dizgi_esit(k, "b0") || dizgi_esit(k, "b1")) {
        b.hata("A5-K102", k->konum, yp("arac.batarya.kip"), std::string(ds) + ": batarya kipi " + k->dizgi);
    }
    if (const Deger* p = s.yol_bul("baglanti.profiller"); p != nullptr && p->tablo_mu()) {
        for (const auto& [ad, pr] : p->tablo) {
            if (const Deger* t = pr.bul("telsiz"); t != nullptr && !deneysel_mi(ad)) {
                b.hata("A5-K102", t->konum, yp("baglanti.profiller").ekle(ad).ekle("telsiz"),
                       std::string(ds) + ": telsiz marj modeli");
            }
        }
    }
    for (const auto& [ad, sn] : sensorler(s, false)) {
        const Deger* ayar = sn->bul("ayar");
        if (ayar == nullptr) {
            continue;
        }
        std::vector<const char*> yasak;
        if (dizgi_esit(sn->bul("tur"), "ping1d")) {
            yasak = {"algilama_esigi"};
        } else if (dizgi_esit(sn->bul("tur"), "ping360")) {
            yasak = {"kazanc_ofsetleri", "l_min", "l_maks"};
        }
        for (const char* k : yasak) {
            if (const Deger* d = ayar->bul(k); d != nullptr) {
                b.hata("A5-K102", d->konum, sensor_yolu(ad).ekle("ayar").ekle(k), std::string(ds) + ": " + k);
            }
        }
    }
    static const std::set<std::string> kDesteklenmeyenTurler = {
        "nan", "aralik_disi", "kismi", "tutukluk", "dolanma", "ani_cokme", "dusuk_batarya",
        "gecikme_sicramasi", "cop_bayt"};
    if (const Deger* ol = s.bul("olaylar"); ol != nullptr && ol->dizi_mi()) {
        for (std::size_t i = 0; i < ol->dizi.size(); ++i) {
            const Deger* t = ol->dizi[i].bul("tur");
            if (t != nullptr && t->dizgi_mi() && kDesteklenmeyenTurler.count(t->dizgi) != 0) {
                b.hata("A5-K102", t->konum, Yol().ekle("olaylar").ekle(i).ekle("tur"),
                       std::string(ds) + ": olay türü " + t->dizgi);
            }
        }
    }
}

void anlamsal_kurallar(const Deger& s, const Deger* manifest, Bulgucu& b)
{
    Baglam c{s, b};
    arac_kurallari(c);
    ortam_kurallari(c);
    sensor_kurallari(c);
    otopilot_kurallari(c);
    gorev_kurallari(c);
    olay_kurallari(c);
    baglanti_kurallari(c);
    calistirma_kurallari(c);
    kalite_kurallari(c);
    baslangic_kurallari(c);
    if (manifest != nullptr) {
        a7_kurallari(c, *manifest);
    }
}

}  // namespace ida::senaryo::ic
