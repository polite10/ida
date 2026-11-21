// Fizik yükleyici (dilim 2): çözümlenmiş senaryo → ida::fizik kurulum yapıları; A5-K22
// (kütle matrisinin pozitif tanımlılığı), A5-K23 (kütle durumları; fizik çekirdeğinin
// türetimi) ve A5-K34 (temas doğruluk ölçütü) denetimleri.
//
// §4.3'ün öteki kuralları (A5-K21, K24–K33, K104, K105) dosya düzeyinde `kurallar.cpp`'de
// denetlenir; A5-K20 ve A5-K89 çerçeve kabulleridir (denetlenecek değer taşımaz).
#include "ida/senaryo/fizik_yukleyici.hpp"

#include "ic.hpp"

#include <Eigen/Dense>

#include <cmath>
#include <string_view>

namespace ida::senaryo {

namespace {

class Okuyucu {
public:
    explicit Okuyucu(const Deger& k) : k_(k) {}
    std::string hata;

    const Deger* bul(std::string_view yol)
    {
        const Deger* d = k_.yol_bul(yol);
        if (d == nullptr && hata.empty()) {
            hata = "çözümlenmiş senaryoda yok: " + std::string(yol);
        }
        return d;
    }
    double sayi(std::string_view yol)
    {
        const Deger* d = bul(yol);
        if (d != nullptr && d->sayisal_mi()) {
            return d->cift();
        }
        tip_hatasi(yol, d);
        return 0.0;
    }
    std::int64_t tam(std::string_view yol)
    {
        const Deger* d = bul(yol);
        if (d != nullptr && d->tur == Tur::tamsayi) {
            return d->tam;
        }
        tip_hatasi(yol, d);
        return 0;
    }
    bool mantik(std::string_view yol)
    {
        const Deger* d = bul(yol);
        if (d != nullptr && d->mantiksal_mi()) {
            return d->mantik;
        }
        tip_hatasi(yol, d);
        return false;
    }
    std::string dizgi(std::string_view yol)
    {
        const Deger* d = bul(yol);
        if (d != nullptr && d->dizgi_mi()) {
            return d->dizgi;
        }
        tip_hatasi(yol, d);
        return {};
    }
    std::vector<double> sayilar(std::string_view yol)
    {
        std::vector<double> o;
        const Deger* d = bul(yol);
        if (d == nullptr || !d->dizi_mi()) {
            tip_hatasi(yol, d);
            return o;
        }
        for (const auto& e : d->dizi) {
            if (!e.sayisal_mi()) {
                tip_hatasi(yol, d);
                return {};
            }
            o.push_back(e.cift());
        }
        return o;
    }
    template <std::size_t N>
    std::array<double, N> vek(std::string_view yol)
    {
        std::array<double, N> o{};
        const auto v = sayilar(yol);
        if (v.size() != N) {
            tip_hatasi(yol, nullptr);
            return o;
        }
        for (std::size_t i = 0; i < N; ++i) {
            o[i] = v[i];
        }
        return o;
    }
    fizik::Mat3 mat3(std::string_view yol)
    {
        fizik::Mat3 o{};
        const Deger* d = bul(yol);
        if (d == nullptr || !d->dizi_mi() || d->dizi.size() != 3) {
            tip_hatasi(yol, d);
            return o;
        }
        for (std::size_t i = 0; i < 3; ++i) {
            const Deger& s = d->dizi[i];
            if (!s.dizi_mi() || s.dizi.size() != 3) {
                tip_hatasi(yol, d);
                return o;
            }
            for (std::size_t j = 0; j < 3; ++j) {
                o[i][j] = s.dizi[j].cift();
            }
        }
        return o;
    }
    bool var(std::string_view yol) const { return k_.yol_bul(yol) != nullptr; }

private:
    const Deger& k_;
    void tip_hatasi(std::string_view yol, const Deger* d)
    {
        if (hata.empty() && d != nullptr) {
            hata = "beklenmeyen tip: " + std::string(yol);
        } else if (hata.empty()) {
            hata = "çözümlenmiş senaryoda yok: " + std::string(yol);
        }
    }
};

fizik::GaussMarkov gauss_markov(Okuyucu& o, const std::string& taban)
{
    fizik::GaussMarkov g;
    g.etkin = o.mantik(taban + ".etkin");
    if (o.var(taban + ".tau_ns")) g.tau_ns = o.tam(taban + ".tau_ns");
    if (o.var(taban + ".sigma")) g.sigma = o.sayi(taban + ".sigma");
    if (o.var(taban + ".sinir")) g.sinir = o.sayi(taban + ".sinir");
    return g;
}

using M6 = Eigen::Matrix<double, 6, 6>;

Eigen::Matrix3d capraz_matrisi(const fizik::Vek3& r)
{
    Eigen::Matrix3d S;
    S << 0.0, -r[2], r[1], r[2], 0.0, -r[0], -r[1], r[0], 0.0;
    return S;
}

// M_RB + M_A (04 §2.3); M_A köşegen, sapma çarpanıyla (yalnız fizik tarafı, A5-K26).
M6 kutle_matrisi(const FizikKurulumu& k, const fizik::KutleDurumu& kd)
{
    M6 M = M6::Zero();
    M.topLeftCorner<3, 3>() = kd.kutle * Eigen::Matrix3d::Identity();
    M.topRightCorner<3, 3>() = -kd.kutle * capraz_matrisi(kd.r_g);
    M.bottomLeftCorner<3, 3>() = kd.kutle * capraz_matrisi(kd.r_g);
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) {
            M(3 + i, 3 + j) = kd.atalet[static_cast<std::size_t>(i)][static_cast<std::size_t>(j)];
        }
    }
    for (int i = 0; i < 6; ++i) {
        const auto u = static_cast<std::size_t>(i);
        M(i, i) += k.arac.hidrodinamik.eklenmis_kutle[u] * k.arac.hidrodinamik.sapma.eklenmis_kutle[u];
    }
    return M;
}

Eigen::Matrix<double, 6, 1> temas_vektoru(const Eigen::Vector3d& r, const Eigen::Vector3d& n)
{
    Eigen::Matrix<double, 6, 1> w;
    w.head<3>() = n;
    w.tail<3>() = r.cross(n);
    return w;
}

void arac_oku(Okuyucu& o, fizik::AracParametreleri& a)
{
    a.ad = o.dizgi("arac.ad");
    a.varlik = o.dizgi("arac.varlik");
    a.referans_noktasi = o.dizgi("arac.referans_noktasi");
    a.kutle.kutle = o.sayi("arac.kutle.kutle");
    a.kutle.hacim = o.sayi("arac.kutle.hacim");
    a.kutle.r_g = o.vek<3>("arac.kutle.r_g");
    a.kutle.r_b = o.vek<3>("arac.kutle.r_b");
    a.kutle.atalet = o.mat3("arac.kutle.atalet");
    a.kutle.trim_kutlesi = o.sayi("arac.kutle.trim_kutlesi");
    if (o.var("arac.kutle.trim_konumu")) {
        a.kutle.trim_konumu = o.vek<3>("arac.kutle.trim_konumu");
    }
    auto& h = a.hidrodinamik;
    h.isaret_kurali = o.dizgi("arac.hidrodinamik.isaret_kurali");
    h.eklenmis_kutle = o.vek<6>("arac.hidrodinamik.eklenmis_kutle");
    h.dogrusal_sonum = o.vek<6>("arac.hidrodinamik.dogrusal_sonum");
    h.karesel_sonum = o.vek<6>("arac.hidrodinamik.karesel_sonum");
    h.yuzey_sonum_orani = o.vek<3>("arac.hidrodinamik.yuzey_sonum_orani");
    h.ruzgar_direnc_katsayisi = o.sayi("arac.hidrodinamik.ruzgar_direnc_katsayisi");
    h.belirsizlik.oteleme = o.sayi("arac.hidrodinamik.belirsizlik.oteleme");
    h.belirsizlik.donme = o.sayi("arac.hidrodinamik.belirsizlik.donme");
    h.sapma.eklenmis_kutle = o.vek<6>("arac.hidrodinamik.sapma.eklenmis_kutle");
    h.sapma.dogrusal_sonum = o.vek<6>("arac.hidrodinamik.sapma.dogrusal_sonum");
    h.sapma.karesel_sonum = o.vek<6>("arac.hidrodinamik.sapma.karesel_sonum");
    for (std::size_t i = 0; i < fizik::kIticiSayisi; ++i) {
        const std::string t = "arac.iticiler.t" + std::to_string(i + 1);
        auto& it = a.iticiler[i];
        it.kanal = static_cast<int>(o.tam(t + ".kanal"));
        it.konum = o.vek<3>(t + ".konum");
        it.yon = o.vek<3>(t + ".yon");
        if (o.var(t + ".rol")) {
            it.rol = o.dizgi(t + ".rol");
        }
    }
    auto& m = a.itici_modeli;
    m.tur = o.dizgi("arac.itici_modeli.tur");
    m.sekil_polinomu = o.vek<5>("arac.itici_modeli.sekil_polinomu");
    m.gerilim_noktalari = o.sayilar("arac.itici_modeli.gerilim_noktalari");
    m.azami_itki_ileri = o.sayilar("arac.itici_modeli.azami_itki_ileri");
    m.azami_itki_geri = o.sayilar("arac.itici_modeli.azami_itki_geri");
    m.tam_gaz_akimi = o.sayilar("arac.itici_modeli.tam_gaz_akimi");
    m.akim_ussu = o.sayi("arac.itici_modeli.akim_ussu");
    m.transfer_pay = o.sayilar("arac.itici_modeli.transfer_pay");
    m.transfer_payda = o.sayilar("arac.itici_modeli.transfer_payda");
    m.pervane_yaricapi = o.sayi("arac.itici_modeli.pervane_yaricapi");
    m.havalanma_h_bolu_r = o.sayilar("arac.itici_modeli.havalanma_h_bolu_r");
    m.havalanma_beta = o.sayilar("arac.itici_modeli.havalanma_beta");
    a.esc.pwm_notr_us = static_cast<int>(o.tam("arac.esc.pwm_notr_us"));
    a.esc.olu_bant_us = static_cast<int>(o.tam("arac.esc.olu_bant_us"));
    a.esc.pwm_min_us = static_cast<int>(o.tam("arac.esc.pwm_min_us"));
    a.esc.pwm_max_us = static_cast<int>(o.tam("arac.esc.pwm_max_us"));
    a.esc.baslatma_suresi_ns = o.tam("arac.esc.baslatma_suresi_ns");
    if (o.dizgi("arac.batarya.kip") != "ideal" && o.hata.empty()) {
        o.hata = "yalnız ideal batarya kipi (A5-K28)";
    }
    a.batarya.kip = fizik::BataryaKipi::ideal;
    a.batarya.v_sabit = o.sayi("arac.batarya.v_sabit");
    a.batarya.seri = static_cast<int>(o.tam("arac.batarya.seri"));
    a.batarya.paralel = static_cast<int>(o.tam("arac.batarya.paralel"));
    a.batarya.kapasite = o.sayi("arac.batarya.kapasite");
    a.batarya.r_kablo = o.sayi("arac.batarya.r_kablo");
    a.batarya.aviyonik_guc = o.sayi("arac.batarya.aviyonik_guc");
    a.temas.yay_sabiti = o.sayi("arac.temas.yay_sabiti");
    a.temas.sonum_orani = o.sayi("arac.temas.sonum_orani");
    a.temas.surtunme_katsayisi = o.sayi("arac.temas.surtunme_katsayisi");
    auto& t = a.torpido;
    t.varlik = o.dizgi("arac.torpido.varlik");
    t.geometri = o.dizgi("arac.torpido.geometri");
    t.boy = o.sayi("arac.torpido.boy");
    t.cap = o.sayi("arac.torpido.cap");
    t.kutle = o.sayi("arac.torpido.kutle");
    t.r_g = o.vek<3>("arac.torpido.r_g");
    t.kanat_alani_duzlem = o.sayi("arac.torpido.kanat_alani_duzlem");
    t.kanat_en_boy_orani = o.sayi("arac.torpido.kanat_en_boy_orani");
    t.kanat_x = o.sayi("arac.torpido.kanat_x");
    t.surukleme_katsayisi = o.sayi("arac.torpido.surukleme_katsayisi");
    t.ayrilma_hizi = o.sayi("arac.torpido.ayrilma_hizi");
    for (std::size_t i = 0; i < fizik::kFirlaticiSayisi; ++i) {
        const std::string f = "arac.firlaticilar.f" + std::to_string(i + 1);
        a.firlaticilar[i].tup_konumu = o.vek<3>(f + ".tup_konumu");
        a.firlaticilar[i].tup_ekseni = o.vek<3>(f + ".tup_ekseni");
        a.firlaticilar[i].tup_su_kutlesi = o.sayi(f + ".tup_su_kutlesi");
    }
}

}  // namespace

namespace ic {

std::optional<FizikKurulumu> fizik_kur_agac(const Deger& c, std::string* hata)
{
    Okuyucu o(c);
    FizikKurulumu k;
    arac_oku(o, k.arac);

    auto& e = k.ortam;
    e.tur = o.dizgi("ortam.tur") == "acik_su" ? fizik::OrtamTuru::acik_su : fizik::OrtamTuru::havuz;
    e.derinlik = o.sayi("ortam.derinlik");
    e.yercekimi = o.sayi("ortam.yercekimi");
    e.atmosfer_basinci = o.sayi("ortam.atmosfer_basinci");
    e.hava_yogunlugu = o.sayi("ortam.hava_yogunlugu");
    e.su.yogunluk = o.sayi("ortam.su.yogunluk");
    e.su.sicaklik_c = o.sayi("ortam.su.sicaklik_c");
    e.su.tuzluluk = o.sayi("ortam.su.tuzluluk");
    e.su.ph = o.sayi("ortam.su.ph");
    const std::string sp = o.dizgi("ortam.dalga.spektrum");
    e.dalga.spektrum = sp == "jonswap"             ? fizik::DalgaSpektrumu::jonswap
                       : sp == "pierson_moskowitz" ? fizik::DalgaSpektrumu::pierson_moskowitz
                                                   : fizik::DalgaSpektrumu::yok;
    if (o.var("ortam.dalga.hs")) e.dalga.hs = o.sayi("ortam.dalga.hs");
    if (o.var("ortam.dalga.tepe_periyodu_ns")) e.dalga.tepe_periyodu_ns = o.tam("ortam.dalga.tepe_periyodu_ns");
    if (o.var("ortam.dalga.gamma")) e.dalga.gamma = o.sayi("ortam.dalga.gamma");
    if (o.var("ortam.dalga.yon")) e.dalga.yon = o.sayi("ortam.dalga.yon");
    e.dalga.yayilim = o.dizgi("ortam.dalga.yayilim") == "tek_yon" ? fizik::DalgaYayilimi::tek_yon
                                                                  : fizik::DalgaYayilimi::cos2s;
    e.dalga.yayilim_s = o.sayi("ortam.dalga.yayilim_s");
    e.dalga.bilesen_sayisi = static_cast<int>(o.tam("ortam.dalga.bilesen_sayisi"));
    e.akinti.hiz_ned = o.vek<3>("ortam.akinti.hiz_ned");
    e.akinti.gauss_markov = gauss_markov(o, "ortam.akinti.gauss_markov");
    e.ruzgar.hiz_ned = o.vek<3>("ortam.ruzgar.hiz_ned");
    e.ruzgar.gauss_markov = gauss_markov(o, "ortam.ruzgar.gauss_markov");

    auto& b = k.baslangic;
    b.konum = o.vek<3>("baslangic.konum");
    b.yonelim = o.vek<4>("baslangic.yonelim");
    b.hiz = o.vek<6>("baslangic.hiz");
    b.yuklu_tupler.clear();
    if (const Deger* yt = o.bul("baslangic.yuklu_tupler"); yt != nullptr && yt->dizi_mi()) {
        for (const auto& x : yt->dizi) {
            b.yuklu_tupler.push_back(x.dizgi);
        }
    }
    if (!o.hata.empty()) {
        if (hata != nullptr) {
            *hata = o.hata;
        }
        return std::nullopt;
    }
    return k;
}

}  // namespace ic

std::optional<FizikKurulumu> fizik_kur(const DogrulanmisSenaryo& s, std::string* hata)
{
    return ic::fizik_kur_agac(s.agac(), hata);
}

namespace {

// Kurulumdan bağımsız K22 denetimi (araç parametreleri yeter).
bool k22_denetle(const FizikKurulumu& k, const Deger& kok, ic::Bulgucu& b)
{
    const Deger* kutle = kok.yol_bul("arac.kutle");
    const Konum kk = kutle != nullptr ? kutle->konum : Konum{};
    for (int bit = 0; bit < 4; ++bit) {
        const std::array<bool, 2> yuklu{(bit & 1) != 0, (bit & 2) != 0};
        const M6 M = kutle_matrisi(k, fizik::kutle_durumu_turet(k.arac, {yuklu[0], yuklu[1]}));
        const Eigen::LLT<M6> llt(M);
        if (!M.allFinite() || llt.info() != Eigen::Success || (M - M.transpose()).cwiseAbs().maxCoeff() > 1e-12) {
            b.hata("A5-K22", kk, Yol().ekle("arac").ekle("kutle"),
                   "M_RB + M_A simetrik pozitif tanımlı değil (kütle durumu " + std::to_string(bit) + ")");
            return false;
        }
    }
    return true;
}

}  // namespace

fizik::KutleDurumu kutle_durumu(const FizikKurulumu& k, const std::array<bool, 2>& yuklu)
{
    return fizik::kutle_durumu_turet(k.arac, {yuklu[0], yuklu[1]});
}

TemasOlcutu temas_olcutu(const FizikKurulumu& k, const std::array<bool, 2>& yuklu,
                         const std::array<double, 3>& en_kucuk, const std::array<double, 3>& en_buyuk,
                         double adim_s)
{
    TemasOlcutu o;
    const M6 M = kutle_matrisi(k, kutle_durumu(k, yuklu));
    const M6 Mi = M.llt().solve(M6::Identity());
    const double kn = k.arac.temas.yay_sabiti;
    auto omega_h = [&](const Eigen::Matrix<double, 6, 1>& w) {
        const double m_eff = 1.0 / w.dot(Mi * w);
        return std::sqrt(kn / m_eff) * adim_s;
    };
    const Eigen::Vector3d ex(1.0, 0.0, 0.0), ey(0.0, 1.0, 0.0), ez(0.0, 0.0, 1.0);
    // Gövdede z aşağıdır: alt yüz en büyük z'dedir.
    const double alt = en_buyuk[2];
    o.merkez_dusey = omega_h(temas_vektoru(Eigen::Vector3d(0.5 * (en_kucuk[0] + en_buyuk[0]),
                                                           0.5 * (en_kucuk[1] + en_buyuk[1]), alt),
                                           ez));
    o.tekil_azami = o.merkez_dusey;
    M6 K = M6::Zero();
    for (int i = 0; i < 8; ++i) {
        const Eigen::Vector3d r((i & 1) ? en_buyuk[0] : en_kucuk[0], (i & 2) ? en_buyuk[1] : en_kucuk[1],
                                (i & 4) ? en_buyuk[2] : en_kucuk[2]);
        for (const Eigen::Vector3d& n : {ex, ey, ez}) {
            o.tekil_azami = std::max(o.tekil_azami, omega_h(temas_vektoru(r, n)));
        }
        if (i & 4) {
            const auto w = temas_vektoru(r, ez);
            K += kn * w * w.transpose();
        }
    }
    const Eigen::GeneralizedSelfAdjointEigenSolver<M6> es(K, M);
    if (es.info() == Eigen::Success) {
        o.dort_kose_azami = std::sqrt(std::max(0.0, es.eigenvalues().maxCoeff())) * adim_s;
    }
    return o;
}

namespace ic {

void fizik_denetimleri(const Deger& s, const Deger* manifest, Bulgucu& b)
{
    std::string hata;
    const std::optional<FizikKurulumu> k = fizik_kur_agac(s, &hata);
    if (!k) {
        // Girdisi kullanılamayan bağımlı denetim: önceki bir hata bunu açıklar (A5-K91);
        // yoksa çözümleyici iç tutarsızlığıdır.
        if (!b.hata_var()) {
            b.hata("A5-K16", Konum{}, Yol(), "çözümlenmiş senaryo fizik yapılarına dönüştürülemedi: " + hata);
        }
        return;
    }
    // A5-K22: dört yük birleşiminin (boş, f1, f2, iki yüklü) hepsinde M_RB + M_A SPD.
    if (!k22_denetle(*k, s, b)) {
        return;
    }
    // A5-K34: çarpışma şekli A7 araç varlığının sınır kutusu.
    if (manifest == nullptr) {
        return;
    }
    const Deger* varlik = s.yol_bul("arac.varlik");
    const Deger* kutu = nullptr;
    if (const Deger* vl = manifest->bul("varliklar"); vl != nullptr && vl->dizi_mi() && varlik != nullptr) {
        for (const auto& x : vl->dizi) {
            const Deger* kim = x.bul("kimlik");
            if (kim != nullptr && kim->dizgi_mi() && kim->dizgi == varlik->dizgi) {
                kutu = x.bul("sinir_kutusu_govde_m");
            }
        }
    }
    if (kutu == nullptr) {
        return;
    }
    std::array<double, 3> en_kucuk{};
    std::array<double, 3> en_buyuk{};
    const Deger* ek = kutu->bul("en_kucuk");
    const Deger* eb = kutu->bul("en_buyuk");
    if (ek == nullptr || eb == nullptr || ek->dizi.size() != 3 || eb->dizi.size() != 3) {
        return;
    }
    for (std::size_t i = 0; i < 3; ++i) {
        en_kucuk[i] = ek->dizi[i].cift();
        en_buyuk[i] = eb->dizi[i].cift();
    }
    for (int bit = 0; bit < 4; ++bit) {
        const std::array<bool, 2> yuklu{(bit & 1) != 0, (bit & 2) != 0};
        const TemasOlcutu t = temas_olcutu(*k, yuklu, en_kucuk, en_buyuk);
        const double enb = std::max(t.tekil_azami, t.dort_kose_azami);
        if (!(enb <= 0.2)) {
            const Deger* ys = s.yol_bul("arac.temas.yay_sabiti");
            b.uyari("A5-K34", ys != nullptr ? ys->konum : Konum{}, Yol().ekle("arac").ekle("temas").ekle("yay_sabiti"),
                    "temas doğruluk ölçütü ω_c h ≤ 0,2 aşılıyor (kütle durumu " + std::to_string(bit) + ": " +
                        sayi_dizilimi(enb) + ")");
            return;
        }
    }
}

void fizik_arac_denetimleri(const Deger& t, Bulgucu& b)
{
    Okuyucu o(t);
    FizikKurulumu k;
    arac_oku(o, k.arac);
    if (!o.hata.empty()) {
        return;  // aşama 2'nin başka bulgusu girdiyi açıklar
    }
    (void)k22_denetle(k, t, b);
}

}  // namespace ic
}  // namespace ida::senaryo
