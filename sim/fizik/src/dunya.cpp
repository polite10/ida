// Dunya: tek araçlı 6-SD fizik (04 §2.3, §2.4 dalgasız, §2.7.1–§2.7.3, §2.10).
//
// Hareket denklemi (gövde çerçevesi, O_b'ye göre; 04 §2.3):
//   (M_RB + M_A) ν̇ = τ_itici + τ_hidrostatik + τ_W − C_RB(ν) ν − C_A(ν_r) ν_r − D(ν_r, s) ν_r
//                    + M_A ν̇_w,     ν_r = ν − ν_w,   ν_w = [Rᵀ v_akıntı ; 0]
//   D(ν_r, s) = s (D_L + D_NL(ν_r)) + w_y D_yüzey
// Kinematik: ṗ_n = R(q) ν₁:₃, q̇ = ½ q ⊗ [0, ω]. Sayısal çözüm klasik RK4; aktüatör komutu ve
// bara gerilimi adım boyunca sabit (04 §2.10); adım sonunda kuaterniyon normalleştirilir.
//
// Kurulumdaki doğrusal cebir Eigen ile, adım içi sıcak yol düz tiplerle (vek.hpp) yazılır.
#include "ida/fizik/dunya.hpp"

#include "hidrostatik.hpp"
#include "itici.hpp"
#include "matematik.hpp"
#include "vek.hpp"

#include <Eigen/Cholesky>
#include <Eigen/Eigenvalues>
#include <Eigen/LU>

#include <algorithm>
#include <cmath>
#include <utility>
#include <vector>

namespace ida::fizik {

using ic::V;

namespace {

// Durum vektörü yerleşimi.
constexpr int kP = 0;        // p_n (3)
constexpr int kQ = 3;        // q_nb (4)
constexpr int kNu = 7;       // ν (6)
constexpr int kN = 13;
using X = std::array<double, kN>;   // gövde durumu (RK4 ile ilerler)
// İtici filtre durumları: itici başına kAzamiFiltreDerecesi; RK4'ün dışında, adım başına tam
// ayrıklaştırmayla (matris üsteli) ilerler.
using XF = std::array<double, kIticiSayisi * ic::kAzamiFiltreDerecesi>;
using Y = std::array<double, kIticiSayisi>;   // filtre çıkışları (havalanmadan önce), itici sırası

// Yer tutucu yüzerlik ağı (04 §2.6, mimari varsayımı): tek kapalı kutu, taban 0,40 m (boy, x)
// × 0,48 m (genişlik, y), yüksekliği hacimden, merkezi r_b'de. Gerçek BUOY_* ağları A7 ile gelir.
constexpr double kKutuBoy = 0.40;
constexpr double kKutuGenislik = 0.48;
constexpr int kSferoidBoylam = 32;
constexpr int kSferoidEnlem = 16;

std::string hata(const char* kural, const std::string& ileti)
{
    return std::string(kural) + ": " + ileti;
}

bool birim_mi(const Vek3& v)
{
    return std::abs(std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]) - 1.0) <= 1e-6;
}

bool birim_mi(const Kuaterniyon& q)
{
    return std::abs(std::sqrt(q[0] * q[0] + q[1] * q[1] + q[2] * q[2] + q[3] * q[3]) - 1.0) <= 1e-6;
}

bool sonlu(double deger) { return std::isfinite(deger); }

template <std::size_t N>
bool hepsi_sonlu(const std::array<double, N>& a)
{
    return std::all_of(a.begin(), a.end(), sonlu);
}

V vv(const Vek3& a) { return {a[0], a[1], a[2]}; }
V birim(const Vek3& a)
{
    const double n = std::sqrt(a[0] * a[0] + a[1] * a[1] + a[2] * a[2]);
    return {a[0] / n, a[1] / n, a[2] / n};
}
Vek3 dz(const V& a) { return {a.x, a.y, a.z}; }
V parca(const X& x, int i) { return {x[static_cast<std::size_t>(i)], x[static_cast<std::size_t>(i + 1)], x[static_cast<std::size_t>(i + 2)]}; }
void yaz(X& x, int i, const V& v)
{
    x[static_cast<std::size_t>(i)] = v.x;
    x[static_cast<std::size_t>(i + 1)] = v.y;
    x[static_cast<std::size_t>(i + 2)] = v.z;
}
ic::M donus(const X& x)
{
    return ic::donus(x[kQ], x[kQ + 1], x[kQ + 2], x[kQ + 3]);
}

bool anlik_sonlu(const DurumAnlik& a)
{
    return hepsi_sonlu(a.konum_n) && hepsi_sonlu(a.yonelim) && hepsi_sonlu(a.dogrusal_hiz_b) &&
           hepsi_sonlu(a.acisal_hiz_b) && hepsi_sonlu(a.dogrusal_ivme_b) && hepsi_sonlu(a.acisal_ivme_b) &&
           hepsi_sonlu(a.ozgul_kuvvet_b) && hepsi_sonlu(a.itki) && hepsi_sonlu(a.itki_istegi) &&
           hepsi_sonlu(a.havalanma) && hepsi_sonlu(a.itici_kuvvet_momenti) && std::isfinite(a.batik_oran) &&
           std::isfinite(a.batik_hacim) && std::isfinite(a.yuzey_sonumu_agirligi);
}

// C¹ geçiş: x ≤ 0 → 1, x ≥ 1 → 0.
double yumusak_dusus(double t)
{
    if (t <= 0.0) {
        return 1.0;
    }
    if (t >= 1.0) {
        return 0.0;
    }
    return 1.0 - t * t * (3.0 - 2.0 * t);
}

}  // namespace

struct Dunya::Ic {
    struct Girdi {
        std::array<double, kIticiSayisi> f_ss{};
    };
    struct Cikti {
        std::array<double, kIticiSayisi> itki{};
        std::array<double, kIticiSayisi> beta{};
        std::array<double, 6> tau_itici{};
        std::array<double, 6> nu_dot{};
        double batik_hacim = 0.0;
        double s = 0.0;
        double w_yuzey = 0.0;
    };

    // Parametreler
    AracParametreleri arac;
    OrtamParametreleri ortam;
    SayisalAyarlar sayisal;
    KutleDurumu kd;
    TuretilmisBuyuklukler tur;
    ic::IticiModeliIc model;

    // Önceden hesaplananlar
    double Mm[6][6] = {};      // M = M_RB + M_A
    double Minv[6][6] = {};    // M⁻¹ (kurulumda Cholesky ile)
    V ma_lin{}, ma_rot{};      // M_A köşegeni (sapma uygulanmış)
    double dl[6] = {};         // D_L köşegeni
    double dq[6] = {};         // D_NL katsayıları
    double ys[3] = {};         // yüzey sönümü katsayıları (heave, roll, pitch)
    V rg{};
    ic::M Io{};
    double m = 0.0;
    double rho = 0.0;
    double g = 0.0;
    double h = 0.0;            // adım, s
    double v_bara = 0.0;
    V akinti_n{};
    std::array<V, kIticiSayisi> r_i{};
    std::array<V, kIticiSayisi> e_i{};
    std::array<V, kIticiSayisi> rxe_i{};
    std::array<int, kIticiSayisi> kanal{};
    std::vector<ic::KapaliAg> aglar;
    double tam_hacim = 0.0;
    mutable std::vector<V> tampon;

    // Durum
    X x{};
    XF xf{};
    std::uint64_t adim = 0;
    // ESC başlatması, kanal sırası (A5 §3.3 baslatma_suresi_ns: nötr bekleme). notr_basi: komutun
    // kesintisiz nötr olduğu aralığın başı (ns); nötr değilse −1.
    std::array<bool, kIticiSayisi> esc_hazir{};
    std::array<std::int64_t, kIticiSayisi> notr_basi{};
    std::array<double, kIticiSayisi> pwm{};  // kanal sırası
    std::vector<std::pair<std::uint64_t, std::array<double, kIticiSayisi>>> bekleyen;
    std::array<double, kIticiSayisi> carpan{};  // itki çarpanı, kanal sırası (W4-K20)
    std::vector<std::pair<std::uint64_t, std::array<double, kIticiSayisi>>> bekleyen_carpan;
    DurumAnlik anlik;
    bool kuruldu = false;
    bool hatali = false;
    std::uint64_t q_sapma = 0;

    std::string kur(const AracParametreleri& a, const OrtamParametreleri& o,
                    const BaslangicDurumu& b, const SayisalAyarlar& s);
    void turev(const X& xs, const Y& y, X& d, Cikti* c) const;
    Y filtre_cikislari(const XF& f) const;
    void esc_guncelle();
    void geri_getirici(const V& p, const ic::M& R, V& f, V& mo, ic::BatikSonuc* bs) const;
    void yuzey_hesapla();
    Girdi girdi_kur() const;
    bool turetilmis_sonlu() const;
    void anlik_doldur(const Girdi& gi, const Cikti& c);
};

// ------------------------------------------------------------------------------- kurulum
std::string Dunya::Ic::kur(const AracParametreleri& a, const OrtamParametreleri& o,
                           const BaslangicDurumu& b, const SayisalAyarlar& s)
{
    using ic::m3;
    using ic::M3;
    using ic::M6;
    using ic::S;
    using ic::v3;
    using ic::V3;

    arac = a;
    ortam = o;
    sayisal = s;

    // --- sayısal
    if (s.adim_ns <= 0) {
        return hata("SayisalAyarlar", "adim_ns pozitif olmalı");
    }
    if (!(s.yuzey_sonumu_gecis_derinligi > 0.0) || !sonlu(s.yuzey_sonumu_gecis_derinligi)) {
        return hata("SayisalAyarlar", "yuzey_sonumu_gecis_derinligi pozitif ve sonlu olmalı");
    }

    // --- sonluluk: fiziğin kullandığı bütün sayısal parametreler (itici modelininkiler
    // IticiModeliIc::kur'da). Sonlu girdilerden taşma kurulum sonundaki denetimlerle yakalanır.
    {
        const auto& hdr = a.hidrodinamik;
        const std::pair<const char*, bool> denetim[] = {
            {"arac.kutle", sonlu(a.kutle.kutle) && sonlu(a.kutle.hacim) && hepsi_sonlu(a.kutle.r_g) &&
                               hepsi_sonlu(a.kutle.r_b) && hepsi_sonlu(a.kutle.atalet[0]) &&
                               hepsi_sonlu(a.kutle.atalet[1]) && hepsi_sonlu(a.kutle.atalet[2]) &&
                               sonlu(a.kutle.trim_kutlesi) && hepsi_sonlu(a.kutle.trim_konumu)},
            {"arac.hidrodinamik", hepsi_sonlu(hdr.eklenmis_kutle) && hepsi_sonlu(hdr.dogrusal_sonum) &&
                                      hepsi_sonlu(hdr.karesel_sonum) && hepsi_sonlu(hdr.yuzey_sonum_orani) &&
                                      sonlu(hdr.belirsizlik.oteleme) && sonlu(hdr.belirsizlik.donme) &&
                                      hepsi_sonlu(hdr.sapma.eklenmis_kutle) &&
                                      hepsi_sonlu(hdr.sapma.dogrusal_sonum) &&
                                      hepsi_sonlu(hdr.sapma.karesel_sonum)},
            {"arac.batarya.v_sabit", sonlu(a.batarya.v_sabit)},
            {"arac.torpido", sonlu(a.torpido.boy) && sonlu(a.torpido.cap) && sonlu(a.torpido.kutle)},
            {"arac.firlaticilar", hepsi_sonlu(a.firlaticilar[0].tup_konumu) &&
                                      hepsi_sonlu(a.firlaticilar[0].tup_ekseni) &&
                                      hepsi_sonlu(a.firlaticilar[1].tup_konumu) &&
                                      hepsi_sonlu(a.firlaticilar[1].tup_ekseni)},
            {"ortam", sonlu(o.yercekimi) && sonlu(o.su.yogunluk) && hepsi_sonlu(o.akinti.hiz_ned)},
            {"baslangic", hepsi_sonlu(b.konum) && hepsi_sonlu(b.yonelim) && hepsi_sonlu(b.hiz)},
        };
        for (const auto& [ad, tamam] : denetim) {
            if (!tamam) {
                return hata("A5 §3 (sonluluk)", std::string(ad) + " içinde sonlu olmayan değer var");
            }
        }
        for (const auto& t : a.iticiler) {
            if (!hepsi_sonlu(t.konum) || !hepsi_sonlu(t.yon)) {
                return hata("A5 §3 (sonluluk)", "arac.iticiler içinde sonlu olmayan değer var");
            }
        }
    }

    // --- arac.kutle (A5 §3.2, A5-K21)
    const auto& k = a.kutle;
    if (!(k.kutle > 0.0) || !(k.hacim > 0.0) || !sonlu(k.kutle) || !sonlu(k.hacim)) {
        return hata("A5 §3.2", "arac.kutle.kutle ve hacim pozitif ve sonlu olmalı");
    }
    if (!(k.trim_kutlesi >= 0.0)) {
        return hata("A5 §3.2", "trim_kutlesi ≥ 0 olmalı");
    }
    {
        const M3 I = m3(k.atalet);
        if (!I.allFinite() || (I - I.transpose()).cwiseAbs().maxCoeff() > 1e-12) {
            return hata("A5-K21", "atalet simetrik değil");
        }
        Eigen::SelfAdjointEigenSolver<M3> es(I);
        const V3 l = es.eigenvalues();
        if (!(l(0) > 0.0)) {
            return hata("A5-K21", "atalet pozitif tanımlı değil");
        }
        if (!(l(0) + l(1) >= l(2) && l(0) + l(2) >= l(1) && l(1) + l(2) >= l(0))) {
            return hata("A5-K21", "asal momentler üçgen eşitsizliğini sağlamıyor");
        }
    }

    // --- arac.hidrodinamik (A5 §3.2, A5-K26)
    const auto& hd = a.hidrodinamik;
    if (hd.isaret_kurali != "pozitif_buyukluk") {
        return hata("A5 §3.2", "isaret_kurali \"pozitif_buyukluk\" olmalı");
    }
    for (int i = 0; i < 6; ++i) {
        const auto u = static_cast<std::size_t>(i);
        if (!(hd.eklenmis_kutle[u] >= 0.0) || !(hd.dogrusal_sonum[u] >= 0.0) ||
            !(hd.karesel_sonum[u] >= 0.0)) {
            return hata("A5 §3.2", "eklenmiş kütle ve sönüm katsayıları ≥ 0 olmalı");
        }
        const double bant = (i < 3) ? hd.belirsizlik.oteleme : hd.belirsizlik.donme;
        for (double c : {hd.sapma.eklenmis_kutle[u], hd.sapma.dogrusal_sonum[u],
                         hd.sapma.karesel_sonum[u]}) {
            if (!(c >= 1.0 - bant && c <= 1.0 + bant)) {
                return hata("A5-K26", "sapma çarpanı belirsizlik bandının dışında");
            }
        }
    }
    for (int i = 0; i < 3; ++i) {
        if (!(hd.yuzey_sonum_orani[static_cast<std::size_t>(i)] >= 0.0)) {
            return hata("A5 §3.2", "yuzey_sonum_orani ≥ 0 olmalı");
        }
    }

    // --- iticiler (A5-K25, A5-K32)
    {
        std::array<bool, kIticiSayisi> gorulen{};
        Eigen::Matrix<double, 6, kIticiSayisi> B;
        for (int i = 0; i < kIticiSayisi; ++i) {
            const auto ui = static_cast<std::size_t>(i);
            const auto& t = a.iticiler[ui];
            if (t.kanal < 0 || t.kanal >= kIticiSayisi || gorulen[static_cast<std::size_t>(t.kanal)]) {
                return hata("A5-K25", "kanal değerleri 0–7 ve tekil olmalı");
            }
            gorulen[static_cast<std::size_t>(t.kanal)] = true;
            if (!birim_mi(t.yon) || !hepsi_sonlu(t.konum)) {
                return hata("A5-K32", "itici yönü birim vektör değil");
            }
            r_i[ui] = vv(t.konum);
            e_i[ui] = birim(t.yon);  // A5-K32: kabul edilen değer normalleştirilerek kullanılır
            rxe_i[ui] = ic::capraz(r_i[ui], e_i[ui]);
            kanal[ui] = t.kanal;
            B.col(i) << e_i[ui].x, e_i[ui].y, e_i[ui].z, rxe_i[ui].x, rxe_i[ui].y, rxe_i[ui].z;
        }
        Eigen::FullPivLU<Eigen::Matrix<double, 6, kIticiSayisi>> lu(B);
        if (lu.rank() != 6) {
            return hata("A5-K25", "itici matrisi B'nin rankı 6 değil");
        }
    }
    if (std::string e = model.kur(a.itici_modeli, a.esc); !e.empty()) {
        return e;
    }

    // --- batarya (A5-K28): dilim 1'de bara gerilimi v_sabit'tir
    if (a.batarya.kip != BataryaKipi::ideal) {
        return hata("A5-K28", "yalnız ideal batarya kipi desteklenir");
    }
    if (!(a.batarya.v_sabit > 0.0 && a.batarya.v_sabit <= 26.0)) {
        return hata("A5 §3.4", "v_sabit (0, 26] V aralığında olmalı");
    }
    v_bara = a.batarya.v_sabit;

    // --- torpido ve fırlatıcılar (kütle durumu için)
    if (!(a.torpido.boy > 0.0) || !(a.torpido.cap > 0.0) || !(a.torpido.kutle > 0.0) ||
        a.torpido.cap > a.torpido.boy) {
        return hata("A5 §3.5", "torpido boy, çap ve kütle pozitif olmalı (prolate: çap ≤ boy)");
    }
    for (const auto& f : a.firlaticilar) {
        if (!birim_mi(f.tup_ekseni) || !hepsi_sonlu(f.tup_konumu)) {
            return hata("A5-K32", "tup_ekseni birim vektör değil");
        }
    }

    // --- ortam (A5 §3.9); dilim 1'in desteklemedikleri reddedilir
    if (!(o.yercekimi > 0.0) || !sonlu(o.yercekimi)) {
        return hata("A5 §3.9", "yercekimi pozitif olmalı");
    }
    if (!(o.su.yogunluk >= 900.0 && o.su.yogunluk <= 1100.0)) {
        return hata("A5 §3.9", "su.yogunluk [900, 1100] aralığında olmalı");
    }
    if (o.dalga.spektrum != DalgaSpektrumu::yok) {
        return hata("W3-fizik dilim 1", "dalga gövdesi bu dilimde yok; yalnız spektrum = yok");
    }
    if (o.ruzgar.hiz_ned != Vek3{0.0, 0.0, 0.0} || o.ruzgar.gauss_markov.etkin) {
        return hata("W3-fizik dilim 1", "rüzgâr gövdesi bu dilimde yok; hız sıfır olmalı");
    }
    if (o.akinti.gauss_markov.etkin) {
        return hata("W3-fizik dilim 1", "akıntının Gauss–Markov bileşeni bu dilimde yok");
    }
    if (!hepsi_sonlu(o.akinti.hiz_ned)) {
        return hata("A5 §3.9", "akinti.hiz_ned sonlu olmalı");
    }
    rho = o.su.yogunluk;
    g = o.yercekimi;
    akinti_n = vv(o.akinti.hiz_ned);

    // --- başlangıç (A5 §3.13)
    if (!birim_mi(b.yonelim)) {
        return hata("A5-K32", "baslangic.yonelim birim kuaterniyon değil");
    }
    if (!hepsi_sonlu(b.hiz) || !hepsi_sonlu(b.konum)) {
        return hata("A5 §3.13", "baslangic.konum ve hiz sonlu olmalı");
    }
    std::array<bool, kFirlaticiSayisi> yuklu{};
    for (const auto& ad : b.yuklu_tupler) {
        int i = -1;
        if (ad == "f1") {
            i = 0;
        } else if (ad == "f2") {
            i = 1;
        }
        if (i < 0 || yuklu[static_cast<std::size_t>(i)]) {
            return hata("A5-K86", "yuklu_tupler yalnız tekrarsız \"f1\", \"f2\" içerir");
        }
        yuklu[static_cast<std::size_t>(i)] = true;
    }

    // --- kütle durumları (A5-K22: dört yük birleşiminin hepsinde M SPD)
    auto kutle_matrisi = [&](const KutleDurumu& kb) {
        M6 Mk = M6::Zero();
        Mk.topLeftCorner<3, 3>() = kb.kutle * M3::Identity();
        Mk.topRightCorner<3, 3>() = -kb.kutle * S(v3(kb.r_g));
        Mk.bottomLeftCorner<3, 3>() = kb.kutle * S(v3(kb.r_g));
        Mk.bottomRightCorner<3, 3>() = m3(kb.atalet);
        for (int i = 0; i < 6; ++i) {
            const auto u = static_cast<std::size_t>(i);
            Mk(i, i) += hd.eklenmis_kutle[u] * hd.sapma.eklenmis_kutle[u];
        }
        return Mk;
    };
    for (int bit = 0; bit < 4; ++bit) {
        const M6 Mk = kutle_matrisi(kutle_durumu_turet(a, {(bit & 1) != 0, (bit & 2) != 0}));
        Eigen::LLT<M6> l(Mk);
        if (l.info() != Eigen::Success || (Mk - Mk.transpose()).cwiseAbs().maxCoeff() > 1e-12) {
            return hata("A5-K22", "M_RB + M_A simetrik pozitif tanımlı değil");
        }
    }
    kd = kutle_durumu_turet(a, yuklu);
    m = kd.kutle;
    rg = vv(kd.r_g);
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) {
            Io.m[i][j] = kd.atalet[static_cast<std::size_t>(i)][static_cast<std::size_t>(j)];
        }
    }
    const M6 M = kutle_matrisi(kd);
    const M6 Mi = Eigen::LLT<M6>(M).solve(M6::Identity());
    for (int i = 0; i < 6; ++i) {
        const auto u = static_cast<std::size_t>(i);
        for (int j = 0; j < 6; ++j) {
            Mm[i][j] = M(i, j);
            Minv[i][j] = Mi(i, j);
        }
        dl[i] = hd.dogrusal_sonum[u] * hd.sapma.dogrusal_sonum[u];
        dq[i] = hd.karesel_sonum[u] * hd.sapma.karesel_sonum[u];
    }
    auto mak = [&](int i) {
        const auto u = static_cast<std::size_t>(i);
        return hd.eklenmis_kutle[u] * hd.sapma.eklenmis_kutle[u];
    };
    ma_lin = {mak(0), mak(1), mak(2)};
    ma_rot = {mak(3), mak(4), mak(5)};

    // --- yüzerlik ağları: boş araç kutusu + dolu tüplerdeki torpidolar
    aglar.clear();
    aglar.push_back(ic::kutu_agi(vv(k.r_b), V{kKutuBoy, kKutuGenislik, k.hacim / (kKutuBoy * kKutuGenislik)}));
    for (std::size_t i = 0; i < static_cast<std::size_t>(kFirlaticiSayisi); ++i) {
        if (yuklu[i]) {
            aglar.push_back(ic::sferoid_agi(vv(a.firlaticilar[i].tup_konumu), vv(a.firlaticilar[i].tup_ekseni),
                                            a.torpido.boy / 2.0, a.torpido.cap / 2.0, kSferoidBoylam,
                                            kSferoidEnlem));
        }
    }
    tam_hacim = 0.0;
    V mom{};
    std::size_t en_cok_kose = 0;
    for (const auto& ag : aglar) {
        tam_hacim += ag.hacim;
        mom += ag.hacim * ag.merkez;
        en_cok_kose = std::max(en_cok_kose, ag.koseler.size());
    }
    tampon.reserve(en_cok_kose);

    h = static_cast<double>(s.adim_ns) * 1e-9;

    tur = {};
    tur.kutle = kd;
    for (int i = 0; i < 6; ++i) {
        for (int j = 0; j < 6; ++j) {
            tur.kutle_matrisi[static_cast<std::size_t>(i)][static_cast<std::size_t>(j)] = Mm[i][j];
        }
    }
    tur.tam_hacim_ag = tam_hacim;
    tur.tam_yuzerlik_merkezi_ag = dz(mom / tam_hacim);
    yuzey_hesapla();

    // --- başlangıç durumu
    x.fill(0.0);
    yaz(x, kP, vv(b.konum));
    const double qn = std::sqrt(b.yonelim[0] * b.yonelim[0] + b.yonelim[1] * b.yonelim[1] +
                                b.yonelim[2] * b.yonelim[2] + b.yonelim[3] * b.yonelim[3]);
    for (int i = 0; i < 4; ++i) {
        x[static_cast<std::size_t>(kQ + i)] = b.yonelim[static_cast<std::size_t>(i)] / qn;
    }
    for (int i = 0; i < 6; ++i) {
        x[static_cast<std::size_t>(kNu + i)] = b.hiz[static_cast<std::size_t>(i)];
    }
    xf.fill(0.0);
    adim = 0;
    pwm.fill(static_cast<double>(a.esc.pwm_notr_us));
    bekleyen.clear();
    carpan.fill(1.0);
    bekleyen_carpan.clear();
    q_sapma = 0;
    hatali = false;
    // Açılışta komut nötrdür (t = 0'dan beri); başlatma süresi 0 ise ESC hemen hazırdır.
    notr_basi.fill(0);
    esc_hazir.fill(a.esc.baslatma_suresi_ns == 0);

    // İtici filtresinin adım içi tam ayrıklaştırması.
    if (std::string e = model.ayriklastir(h); !e.empty()) {
        return e;
    }

    // Kurulum çıktılarının sonluluğu: türetilmiş büyüklükler, ilk türev, anlık görüntü.
    if (!turetilmis_sonlu()) {
        return hata("Sayısal", "kurulumda türetilen büyüklükler sonlu değil (parametre taşması)");
    }
    esc_guncelle();
    const Girdi gi = girdi_kur();
    X turev0;
    Cikti cikti0;
    turev(x, filtre_cikislari(xf), turev0, &cikti0);
    if (!hepsi_sonlu(turev0)) {
        return hata("Sayısal", "başlangıç durumunda hareket denkleminin türevi sonlu değil (parametre taşması)");
    }
    anlik_doldur(gi, cikti0);
    if (!anlik_sonlu(anlik)) {
        return hata("Sayısal", "başlangıç anlık görüntüsü sonlu değil");
    }
    return {};
}

// Düz suda, düz yönelimde yüzer denge ve Otter yüzey sönümü katsayıları (04 §2.4).
void Dunya::Ic::yuzey_hesapla()
{
    const ic::M I3 = ic::donus(1.0, 0.0, 0.0, 0.0);
    auto hacim_z = [&](double z) {
        return ic::batik_hesapla(aglar, V{0.0, 0.0, z}, I3, HUGE_VAL, tampon).hacim;
    };
    const double hedef = m / rho;
    ys[0] = ys[1] = ys[2] = 0.0;
    tur.yuzeyde_denge_var = false;
    if (!(hedef < tam_hacim)) {
        return;  // yüzer denge yok (nötr ya da negatif yüzerlik)
    }
    // Sınırlar: ağların sınır kürelerinden.
    double lo = 0.0;   // her şey dışarıda
    double hi = 0.0;   // her şey batık
    for (const auto& ag : aglar) {
        lo = std::min(lo, -(ag.sinir_merkezi.z + ag.sinir_yaricapi) - 1.0);
        hi = std::max(hi, -(ag.sinir_merkezi.z - ag.sinir_yaricapi) + 1.0);
    }
    for (int i = 0; i < 200; ++i) {
        const double orta = 0.5 * (lo + hi);
        if (orta <= lo || orta >= hi) {
            break;
        }
        if (hacim_z(orta) < hedef) {
            lo = orta;
        } else {
            hi = orta;
        }
    }
    const double z0 = 0.5 * (lo + hi);
    tur.yuzeyde_denge_var = true;
    tur.denge_konumu_z = z0;

    // Sonlu farklarla geri getirici katılıklar (O_b etrafında; küçük açı).
    const double ez = 1e-6;
    const double awp = (hacim_z(z0 + ez) - hacim_z(z0 - ez)) / (2.0 * ez);
    tur.denge_su_hatti_alani = awp;
    const double c33 = rho * g * awp;
    auto moment = [&](int eksen, double aci) {
        const double sn = std::sin(aci / 2.0);
        const ic::M R = ic::donus(std::cos(aci / 2.0), eksen == 0 ? sn : 0.0, eksen == 1 ? sn : 0.0, 0.0);
        V f, mo;
        geri_getirici(V{0.0, 0.0, z0}, R, f, mo, nullptr);
        return eksen == 0 ? mo.x : mo.y;
    };
    const double ea = 1e-6;
    const double c44 = -(moment(0, ea) - moment(0, -ea)) / (2.0 * ea);
    const double c55 = -(moment(1, ea) - moment(1, -ea)) / (2.0 * ea);
    const double cii[3] = {c33, c44, c55};
    for (int i = 0; i < 3; ++i) {
        const double Mii = Mm[2 + i][2 + i];
        const double w = (cii[i] > 0.0) ? std::sqrt(cii[i] / Mii) : 0.0;
        const auto u = static_cast<std::size_t>(i);
        tur.yuzey_dogal_frekans[u] = w;
        ys[i] = 2.0 * arac.hidrodinamik.yuzey_sonum_orani[u] * w * Mii;
        tur.yuzey_sonum_katsayisi[u] = ys[i];
    }
}

// Hidrostatik + ağırlık (gövde çerçevesi, O_b'ye göre): kuvvet f, moment mo.
void Dunya::Ic::geri_getirici(const V& p, const ic::M& R, V& f, V& mo, ic::BatikSonuc* bs) const
{
    const ic::BatikSonuc b = ic::batik_hesapla(aglar, p, R, sayisal.yuzey_sonumu_gecis_derinligi, tampon);
    f = V{};
    mo = V{};
    if (b.hacim > 0.0) {
        const V c_n = b.moment / b.hacim;
        const V fb = ic::devrik_carp(R, V{0.0, 0.0, -rho * g * b.hacim});
        const V r = ic::devrik_carp(R, c_n - p);
        f += fb;
        mo += ic::capraz(r, fb);
    }
    const V fg = ic::devrik_carp(R, V{0.0, 0.0, m * g});
    f += fg;
    mo += ic::capraz(rg, fg);
    if (bs != nullptr) {
        *bs = b;
    }
}

void Dunya::Ic::turev(const X& xs, const Y& y, X& d, Cikti* c) const
{
    using ic::capraz;
    using ic::carp;
    const V p = parca(xs, kP);
    const V v = parca(xs, kNu);
    const V w = parca(xs, kNu + 3);
    const ic::M R = donus(xs);

    // Hidrostatik ve ağırlık
    ic::BatikSonuc bs;
    V f, mo;
    geri_getirici(p, R, f, mo, &bs);
    const double s = std::clamp(bs.hacim / tam_hacim, 0.0, 1.0);

    // İticiler: F_i = c_i · β(h_i/R) · K(s) F_ss,i (c_i: itki çarpanı, W4-K20; arızasızda 1)
    V f_it{}, m_it{};
    for (int i = 0; i < kIticiSayisi; ++i) {
        const auto u = static_cast<std::size_t>(i);
        const double derinlik = p.z + R.m[2][0] * r_i[u].x + R.m[2][1] * r_i[u].y + R.m[2][2] * r_i[u].z;
        const double beta = model.havalanma(derinlik);
        const double F = beta * y[u] * carpan[static_cast<std::size_t>(kanal[u])];
        f_it += F * e_i[u];
        m_it += F * rxe_i[u];
        if (c != nullptr) {
            c->itki[u] = F;
            c->beta[u] = beta;
        }
    }
    f += f_it;
    mo += m_it;

    // Akıntıya göre bağıl hız (düzgün, sabit akıntı; su açısal hızı yok)
    const V vc_b = ic::devrik_carp(R, akinti_n);
    const V vr = v - vc_b;

    // Katı cisim Coriolis ve merkezcil terimleri, C_RB(ν) ν (O_b'ye göre, r_g ≠ 0)
    const V crb_f = m * capraz(w, v) + m * capraz(w, capraz(w, rg));
    const V crb_m = m * capraz(rg, capraz(w, v)) + capraz(w, Io * w);

    // Eklenmiş kütle Coriolis terimleri, C_A(ν_r) ν_r (Kirchhoff biçimi, köşegen M_A)
    const V a1 = carp(ma_lin, vr);
    const V a2 = carp(ma_rot, w);
    const V ca_f = capraz(w, a1);
    const V ca_m = capraz(vr, a1) + capraz(w, a2);

    // Sönüm: s (D_L + D_NL) ν_r + w_y D_yüzey ν_r
    const double nur[6] = {vr.x, vr.y, vr.z, w.x, w.y, w.z};
    double dv[6];
    for (int i = 0; i < 6; ++i) {
        dv[i] = s * (dl[i] + dq[i] * std::abs(nur[i])) * nur[i];
    }
    double wy = 0.0;
    if (bs.hacim > 0.0) {
        wy = yumusak_dusus(bs.en_ust_derinlik / sayisal.yuzey_sonumu_gecis_derinligi);
    }
    dv[2] += wy * ys[0] * nur[2];
    dv[3] += wy * ys[1] * nur[3];
    dv[4] += wy * ys[2] * nur[4];

    // Su hızının gövde çerçevesindeki türevi (düzgün sabit akıntı): ν̇_w = [−ω × Rᵀ v_c ; 0]
    const V nwd = -capraz(w, vc_b);
    const V ma_nwd = carp(ma_lin, nwd);

    const double rhs[6] = {
        f.x - crb_f.x - ca_f.x - dv[0] + ma_nwd.x,
        f.y - crb_f.y - ca_f.y - dv[1] + ma_nwd.y,
        f.z - crb_f.z - ca_f.z - dv[2] + ma_nwd.z,
        mo.x - crb_m.x - ca_m.x - dv[3],
        mo.y - crb_m.y - ca_m.y - dv[4],
        mo.z - crb_m.z - ca_m.z - dv[5],
    };
    double nu_dot[6];
    for (int i = 0; i < 6; ++i) {
        double t = 0.0;
        for (int j = 0; j < 6; ++j) {
            t += Minv[i][j] * rhs[j];
        }
        nu_dot[i] = t;
    }

    yaz(d, kP, R * v);
    // q̇ = ½ q ⊗ [0, ω]
    const double qw = xs[kQ], qx = xs[kQ + 1], qy = xs[kQ + 2], qz = xs[kQ + 3];
    d[kQ] = 0.5 * (-qx * w.x - qy * w.y - qz * w.z);
    d[kQ + 1] = 0.5 * (qw * w.x + qy * w.z - qz * w.y);
    d[kQ + 2] = 0.5 * (qw * w.y - qx * w.z + qz * w.x);
    d[kQ + 3] = 0.5 * (qw * w.z + qx * w.y - qy * w.x);
    for (int i = 0; i < 6; ++i) {
        d[static_cast<std::size_t>(kNu + i)] = nu_dot[i];
    }

    if (c != nullptr) {
        c->tau_itici = {f_it.x, f_it.y, f_it.z, m_it.x, m_it.y, m_it.z};
        for (int i = 0; i < 6; ++i) {
            c->nu_dot[static_cast<std::size_t>(i)] = nu_dot[i];
        }
        c->batik_hacim = bs.hacim;
        c->s = s;
        c->w_yuzey = wy;
    }
}

Dunya::Ic::Girdi Dunya::Ic::girdi_kur() const
{
    Girdi gi;
    for (int i = 0; i < kIticiSayisi; ++i) {
        const auto u = static_cast<std::size_t>(i);
        const auto k = static_cast<std::size_t>(kanal[u]);
        gi.f_ss[u] = esc_hazir[k] ? model.durgun_itki(pwm[k], v_bara) : 0.0;
    }
    return gi;
}

// ESC başlatması (kanal başına; A5 §3.3 `baslatma_suresi_ns` nötr bekleme süresidir): komut
// [notr − olu_bant, notr + olu_bant] içinde kesintisiz `baslatma_suresi_ns` boyunca tutulunca
// ESC hazır olur ve öyle kalır (bara kesintisi dilim 1'de yok). Hazır olmadan gelen nötr dışı
// komut itki üretmez ve süreyi sıfırlar. Adım başında, o adımın komutu yürürlüğe girdikten
// sonra çağrılır: [notr_basi, t) aralığındaki adımların komutları nötr ise süre dolmuştur.
void Dunya::Ic::esc_guncelle()
{
    const std::int64_t t = static_cast<std::int64_t>(adim) * sayisal.adim_ns;
    for (std::size_t k = 0; k < static_cast<std::size_t>(kIticiSayisi); ++k) {
        if (esc_hazir[k]) {
            continue;
        }
        if (notr_basi[k] >= 0 && t - notr_basi[k] >= arac.esc.baslatma_suresi_ns) {
            esc_hazir[k] = true;
            continue;
        }
        if (!model.notr_mu(pwm[k])) {
            notr_basi[k] = -1;
        } else if (notr_basi[k] < 0) {
            notr_basi[k] = t;
        }
    }
}

Y Dunya::Ic::filtre_cikislari(const XF& f) const
{
    Y y{};
    for (std::size_t i = 0; i < y.size(); ++i) {
        y[i] = model.filtre_cikisi(f.data() + i * static_cast<std::size_t>(ic::kAzamiFiltreDerecesi));
    }
    return y;
}

bool Dunya::Ic::turetilmis_sonlu() const
{
    bool tamam = std::isfinite(m) && std::isfinite(tam_hacim) && tam_hacim > 0.0 && std::isfinite(h);
    for (int i = 0; i < 6; ++i) {
        for (int j = 0; j < 6; ++j) {
            tamam = tamam && std::isfinite(Mm[i][j]) && std::isfinite(Minv[i][j]);
        }
        tamam = tamam && std::isfinite(dl[i]) && std::isfinite(dq[i]);
    }
    for (int i = 0; i < 3; ++i) {
        const auto u = static_cast<std::size_t>(i);
        tamam = tamam && std::isfinite(ys[i]) && std::isfinite(tur.yuzey_dogal_frekans[u]) &&
                std::isfinite(tur.tam_yuzerlik_merkezi_ag[u]);
    }
    tamam = tamam && std::isfinite(tur.denge_konumu_z) && std::isfinite(tur.denge_su_hatti_alani);
    return tamam;
}

void Dunya::Ic::anlik_doldur(const Girdi& gi, const Cikti& c)
{
    DurumAnlik& a = anlik;
    a.adim_no = adim;
    a.t_ns = static_cast<std::int64_t>(adim) * sayisal.adim_ns;
    a.konum_n = dz(parca(x, kP));
    a.yonelim = {x[kQ], x[kQ + 1], x[kQ + 2], x[kQ + 3]};
    const V v = parca(x, kNu);
    const V w = parca(x, kNu + 3);
    const V vd{c.nu_dot[0], c.nu_dot[1], c.nu_dot[2]};
    a.dogrusal_hiz_b = dz(v);
    a.acisal_hiz_b = dz(w);
    a.dogrusal_ivme_b = dz(vd);
    a.acisal_ivme_b = {c.nu_dot[3], c.nu_dot[4], c.nu_dot[5]};
    const ic::M R = donus(x);
    a.ozgul_kuvvet_b = dz(vd + ic::capraz(w, v) - ic::devrik_carp(R, V{0.0, 0.0, g}));
    for (int i = 0; i < kIticiSayisi; ++i) {
        const auto u = static_cast<std::size_t>(i);
        const auto k = static_cast<std::size_t>(kanal[u]);
        a.itki[k] = c.itki[u];
        a.itki_istegi[k] = gi.f_ss[u];
        a.havalanma[k] = c.beta[u];
    }
    a.pwm_us = pwm;
    a.itici_kuvvet_momenti = c.tau_itici;
    a.batik_oran = c.s;
    a.batik_hacim = c.batik_hacim;
    a.yuzey_sonumu_agirligi = c.w_yuzey;
    a.esc_hazir = esc_hazir;
}

// ------------------------------------------------------------------------------- Dunya
Dunya::Dunya() : ic_(std::make_unique<Ic>()) {}
Dunya::~Dunya() = default;
Dunya::Dunya(Dunya&&) noexcept = default;
Dunya& Dunya::operator=(Dunya&&) noexcept = default;

KurulumSonucu Dunya::kur(const AracParametreleri& arac, const OrtamParametreleri& ortam,
                         const BaslangicDurumu& baslangic, const SayisalAyarlar& sayisal)
{
    ic_ = std::make_unique<Ic>();
    const std::string e = ic_->kur(arac, ortam, baslangic, sayisal);
    KurulumSonucu s;
    s.tamam = e.empty();
    s.hata = e;
    ic_->kuruldu = s.tamam;
    return s;
}

AdimDurumu Dunya::adim()
{
    Ic& d = *ic_;
    if (!d.kuruldu) {
        return AdimDurumu::kurulmadi;
    }
    if (d.hatali) {
        return AdimDurumu::sayisal_hata;
    }
    // 1. Aktüatör komutu: bu adıma verilmiş komut yürürlüğe girer; ESC başlatması güncellenir.
    while (!d.bekleyen.empty() && d.bekleyen.front().first <= d.adim) {
        d.pwm = d.bekleyen.front().second;
        d.bekleyen.erase(d.bekleyen.begin());
    }
    while (!d.bekleyen_carpan.empty() && d.bekleyen_carpan.front().first <= d.adim) {
        d.carpan = d.bekleyen_carpan.front().second;
        d.bekleyen_carpan.erase(d.bekleyen_carpan.begin());
    }
    d.esc_guncelle();
    const Ic::Girdi gi = d.girdi_kur();

    // 2. İtici filtreleri: F_ss adım boyunca sabit; t, t + h/2 ve t + h'deki durum tam
    // ayrıklaştırmayla (sıfırıncı derece tutma, matris üsteli) bulunur.
    XF f_yarim{}, f_son{};
    constexpr auto kD = static_cast<std::size_t>(ic::kAzamiFiltreDerecesi);
    for (std::size_t i = 0; i < static_cast<std::size_t>(kIticiSayisi); ++i) {
        d.model.filtre_ilerlet(d.xf.data() + i * kD, gi.f_ss[i], true, f_yarim.data() + i * kD);
        d.model.filtre_ilerlet(d.xf.data() + i * kD, gi.f_ss[i], false, f_son.data() + i * kD);
    }
    const Y y0 = d.filtre_cikislari(d.xf);
    const Y y_yarim = d.filtre_cikislari(f_yarim);
    const Y y_son = d.filtre_cikislari(f_son);

    // 3. Gövde: RK4; itici çıkışı aşama zamanlarındaki tam değeriyle.
    const double h = d.h;
    X k1, k2, k3, k4, ara;
    d.turev(d.x, y0, k1, nullptr);
    for (std::size_t i = 0; i < ara.size(); ++i) ara[i] = d.x[i] + (h / 2.0) * k1[i];
    d.turev(ara, y_yarim, k2, nullptr);
    for (std::size_t i = 0; i < ara.size(); ++i) ara[i] = d.x[i] + (h / 2.0) * k2[i];
    d.turev(ara, y_yarim, k3, nullptr);
    for (std::size_t i = 0; i < ara.size(); ++i) ara[i] = d.x[i] + h * k3[i];
    d.turev(ara, y_son, k4, nullptr);
    X yeni;
    for (std::size_t i = 0; i < yeni.size(); ++i) {
        yeni[i] = d.x[i] + (h / 6.0) * (k1[i] + 2.0 * k2[i] + 2.0 * k3[i] + k4[i]);
    }

    // 4. Kuaterniyon normalleştirme
    const double qn = std::sqrt(yeni[kQ] * yeni[kQ] + yeni[kQ + 1] * yeni[kQ + 1] +
                                yeni[kQ + 2] * yeni[kQ + 2] + yeni[kQ + 3] * yeni[kQ + 3]);
    if (std::abs(qn - 1.0) > 1e-9) {
        ++d.q_sapma;
    }
    for (int i = 0; i < 4; ++i) {
        yeni[static_cast<std::size_t>(kQ + i)] /= qn;
    }
    if (!hepsi_sonlu(yeni) || !hepsi_sonlu(f_son)) {
        d.hatali = true;
        return AdimDurumu::sayisal_hata;
    }

    // 5. Adım sonu çıktıları (ν̇, gerçekleşen itki, batık oran) yeni durumda
    X kson;
    Ic::Cikti c;
    d.turev(yeni, y_son, kson, &c);
    if (!hepsi_sonlu(kson)) {
        d.hatali = true;
        return AdimDurumu::sayisal_hata;
    }
    d.x = yeni;
    d.xf = f_son;
    ++d.adim;
    d.anlik_doldur(gi, c);
    if (!anlik_sonlu(d.anlik)) {
        d.hatali = true;
        return AdimDurumu::sayisal_hata;
    }
    return AdimDurumu::tamam;
}

bool Dunya::itici_komutu_ver(std::uint64_t adim_no, const std::array<double, kIticiSayisi>& pwm_us)
{
    Ic& d = *ic_;
    if (!d.kuruldu || adim_no < d.adim || !hepsi_sonlu(pwm_us)) {
        return false;
    }
    auto it = std::lower_bound(d.bekleyen.begin(), d.bekleyen.end(), adim_no,
                               [](const auto& e, std::uint64_t a) { return e.first < a; });
    if (it != d.bekleyen.end() && it->first == adim_no) {
        it->second = pwm_us;
    } else {
        d.bekleyen.insert(it, {adim_no, pwm_us});
    }
    return true;
}

bool Dunya::itki_carpani_ver(std::uint64_t adim_no, const std::array<double, kIticiSayisi>& carpan)
{
    Ic& d = *ic_;
    if (!d.kuruldu || adim_no < d.adim) {
        return false;
    }
    for (double c : carpan) {
        if (!(c >= 0.0 && c <= 1.0)) {
            return false;
        }
    }
    auto it = std::lower_bound(d.bekleyen_carpan.begin(), d.bekleyen_carpan.end(), adim_no,
                               [](const auto& e, std::uint64_t a) { return e.first < a; });
    if (it != d.bekleyen_carpan.end() && it->first == adim_no) {
        it->second = carpan;
    } else {
        d.bekleyen_carpan.insert(it, {adim_no, carpan});
    }
    return true;
}

bool Dunya::kuruldu() const { return ic_->kuruldu; }
std::int64_t Dunya::zaman_ns() const { return static_cast<std::int64_t>(ic_->adim) * ic_->sayisal.adim_ns; }
std::uint64_t Dunya::adim_no() const { return ic_->adim; }
std::int64_t Dunya::adim_ns() const { return ic_->sayisal.adim_ns; }
const DurumAnlik& Dunya::durum() const { return ic_->anlik; }
const TuretilmisBuyuklukler& Dunya::turetilmis() const { return ic_->tur; }
std::uint64_t Dunya::kuaterniyon_sapma_sayaci() const { return ic_->q_sapma; }

double Dunya::mekanik_enerji() const
{
    const Ic& d = *ic_;
    const V p = parca(d.x, kP);
    const ic::M R = donus(d.x);
    double kinetik = 0.0;
    for (int i = 0; i < 6; ++i) {
        for (int j = 0; j < 6; ++j) {
            kinetik += 0.5 * d.x[static_cast<std::size_t>(kNu + i)] * d.Mm[i][j] * d.x[static_cast<std::size_t>(kNu + j)];
        }
    }
    const ic::BatikSonuc b = ic::batik_hesapla(d.aglar, p, R, HUGE_VAL, d.tampon);
    const double zG = p.z + R.m[2][0] * d.rg.x + R.m[2][1] * d.rg.y + R.m[2][2] * d.rg.z;
    return kinetik - d.m * d.g * zG + d.rho * d.g * b.moment.z;
}

}  // namespace ida::fizik
