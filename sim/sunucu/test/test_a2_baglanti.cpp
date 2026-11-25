// A2 bağlama katmanının birim sınamaları (W5-bagla-a2; sim.sunucu.a2_*).
//
//   a2_hiz_donusumu     kabul 3 (a): v_n = R(q_nb) · v_b; bilinen yönelimlerle (eksenle hizalı dört
//                       yönelim ve eksenle hizalı olmayan iki yönelim). Beklenen değer bağımsız iki
//                       yoldan hesaplanır: (1) Euler açılarından açık yazılmış dönüş matrisi (Fossen
//                       ZYX), (2) kuaterniyon çarpımı q ⊗ (0, v) ⊗ q*. `govde_ani` alanları.
//   a2_fizik_tutarlilik kabul 3 (b), fizik çekirdeğiyle: itki altında hareket eden araçta ardışık iki
//                       adım sınırının konum farkı ile v_n · h arasındaki fark bir adımın ivme
//                       teriminden küçük; yönelim artışı ile w_b · h arasındaki fark açısal ivme
//                       teriminden küçük. Dönüşümün yanlış çerçevesi (v_b'nin ya da dünya
//                       çerçevesindeki ω'nın aynen taşınması) bu sınırları katlarca aşar (sınanır).
//   a2_tanim            çalıştırma tanımı: depodaki senaryodan (kademe, A7 manifest yolu ve özeti,
//                       varlık indeksleri, ortam), görünüm kapalı, tam kademe, optik malzeme geçersiz
//                       kılması, değişmiş manifest.
//   a2_kimlik           W5-K32: gövde kimliği süreç boyunca tekdüze artar.
#include "a2_baglanti.hpp"
#include "ida/fizik/dunya.hpp"
#include "ida/senaryo/senaryo.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <memory>
#include <string>

#ifndef IDA_DEPO_KOKU
#define IDA_DEPO_KOKU "."
#endif

namespace {

using ida::fizik::Kuaterniyon;
using ida::fizik::Vek3;
namespace a2b = ida::sunucu::a2b;

int g_hata_a2 = 0;

void dogrula_a2(bool kosul, const std::string& ileti)
{
    std::printf("%s %s\n", kosul ? "GECTI " : "KALDI ", ileti.c_str());
    if (!kosul) {
        ++g_hata_a2;
    }
}

double norm(const Vek3& v) { return std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]); }
Vek3 fark(const Vek3& a, const Vek3& b) { return {a[0] - b[0], a[1] - b[1], a[2] - b[2]}; }

// ZYX Euler açılarından (yalpa φ, baş-kıç θ, yön ψ) gövdeden NED'e kuaterniyon (Hamilton; w, x, y, z).
Kuaterniyon euler_q(double phi, double theta, double psi)
{
    const double cf = std::cos(phi / 2.0), sf = std::sin(phi / 2.0);
    const double ct = std::cos(theta / 2.0), st = std::sin(theta / 2.0);
    const double cp = std::cos(psi / 2.0), sp = std::sin(psi / 2.0);
    return {cp * ct * cf + sp * st * sf, cp * ct * sf - sp * st * cf, cp * st * cf + sp * ct * sf,
            sp * ct * cf - cp * st * sf};
}

// Bağımsız yol 1: Euler açılarından açık yazılmış dönüş (Fossen 2011, denklem 2.18; gövdeden NED'e).
Vek3 euler_donus(double phi, double theta, double psi, const Vek3& v)
{
    const double cf = std::cos(phi), sf = std::sin(phi);
    const double ct = std::cos(theta), st = std::sin(theta);
    const double cp = std::cos(psi), sp = std::sin(psi);
    return {cp * ct * v[0] + (-sp * cf + cp * st * sf) * v[1] + (sp * sf + cp * cf * st) * v[2],
            sp * ct * v[0] + (cp * cf + sf * st * sp) * v[1] + (-cp * sf + st * sp * cf) * v[2],
            -st * v[0] + ct * sf * v[1] + ct * cf * v[2]};
}

// Hamilton çarpımı.
Kuaterniyon carp(const Kuaterniyon& a, const Kuaterniyon& b)
{
    return {a[0] * b[0] - a[1] * b[1] - a[2] * b[2] - a[3] * b[3], a[0] * b[1] + a[1] * b[0] + a[2] * b[3] - a[3] * b[2],
            a[0] * b[2] - a[1] * b[3] + a[2] * b[0] + a[3] * b[1], a[0] * b[3] + a[1] * b[2] - a[2] * b[1] + a[3] * b[0]};
}
Kuaterniyon eslenik(const Kuaterniyon& q) { return {q[0], -q[1], -q[2], -q[3]}; }

// Bağımsız yol 2: v_n = q ⊗ (0, v_b) ⊗ q*.
Vek3 kuaterniyon_donus(const Kuaterniyon& q, const Vek3& v)
{
    const Kuaterniyon r = carp(carp(q, {0.0, v[0], v[1], v[2]}), eslenik(q));
    return {r[1], r[2], r[3]};
}

bool yakin(const Vek3& a, const Vek3& b, double tol) { return norm(fark(a, b)) <= tol; }

std::string v3(const Vek3& v)
{
    char t[96];
    std::snprintf(t, sizeof t, "(%.6f, %.6f, %.6f)", v[0], v[1], v[2]);
    return t;
}

}  // namespace

int test_a2_hiz_donusumu()
{
    const double pi = 3.14159265358979323846;
    // ---- Eksenle hizalı yönelimler: sonuç elle yazılır ----
    {
        // Birim yönelim: gövde eksenleri NED eksenleriyle çakışık.
        const Vek3 v = a2b::govdeden_ned({1.0, 0.0, 0.0, 0.0}, {0.3, -0.2, 0.1});
        dogrula_a2(yakin(v, {0.3, -0.2, 0.1}, 0.0), "birim yönelim: v_n = v_b " + v3(v));
    }
    {
        // Yön +90° (pruva doğuya): ileri hız (u) doğu hızıdır; sancak hızı (v) güneye (−N) bakar.
        const Kuaterniyon q = euler_q(0.0, 0.0, pi / 2.0);
        const Vek3 a = a2b::govdeden_ned(q, {1.0, 0.0, 0.0});
        const Vek3 b = a2b::govdeden_ned(q, {0.0, 1.0, 0.0});
        const Vek3 c = a2b::govdeden_ned(q, {0.0, 0.0, 1.0});
        dogrula_a2(yakin(a, {0.0, 1.0, 0.0}, 1e-15) && yakin(b, {-1.0, 0.0, 0.0}, 1e-15) && yakin(c, {0.0, 0.0, 1.0}, 1e-15),
                   "yön +90°: u → doğu " + v3(a) + ", v → güney " + v3(b) + ", w → aşağı " + v3(c));
    }
    {
        // Baş-kıç +30° (pruva yukarı): ileri hızın aşağı bileşeni negatiftir (araç yükselir).
        const Kuaterniyon q = euler_q(0.0, pi / 6.0, 0.0);
        const Vek3 a = a2b::govdeden_ned(q, {1.0, 0.0, 0.0});
        dogrula_a2(yakin(a, {std::cos(pi / 6.0), 0.0, -std::sin(pi / 6.0)}, 1e-15),
                   "baş-kıç +30°: u → (cos 30°, 0, −sin 30°) " + v3(a));
    }
    {
        // Yalpa +90° (sancak aşağı): sancak hızı aşağı (+D), gövde aşağı hızı iskeleye (−E) bakar.
        const Kuaterniyon q = euler_q(pi / 2.0, 0.0, 0.0);
        const Vek3 b = a2b::govdeden_ned(q, {0.0, 1.0, 0.0});
        const Vek3 c = a2b::govdeden_ned(q, {0.0, 0.0, 1.0});
        dogrula_a2(yakin(b, {0.0, 0.0, 1.0}, 1e-15) && yakin(c, {0.0, -1.0, 0.0}, 1e-15),
                   "yalpa +90°: v → aşağı " + v3(b) + ", w → batı " + v3(c));
    }
    {
        // Yön 180°: ileri hız güneye, sancak hızı batıya.
        const Kuaterniyon q = euler_q(0.0, 0.0, pi);
        const Vek3 a = a2b::govdeden_ned(q, {0.5, 0.25, 0.0});
        dogrula_a2(yakin(a, {-0.5, -0.25, 0.0}, 1e-15), "yön 180°: (u, v) → (−N, −E) " + v3(a));
    }
    // ---- Eksenle hizalı olmayan yönelimler: iki bağımsız hesapla ----
    struct Ornek {
        double phi, theta, psi;
        Vek3 v;
    };
    for (const Ornek& o : {Ornek{1.1, -0.4, 0.7, {0.83, -0.21, 0.37}}, Ornek{-2.3, 0.9, -2.9, {-0.4, 1.7, 0.05}},
                           Ornek{0.02, 1.2, 3.0, {2.0, 0.0, -1.0}}}) {
        const Kuaterniyon q = euler_q(o.phi, o.theta, o.psi);
        const Vek3 v = a2b::govdeden_ned(q, o.v);
        const Vek3 e1 = euler_donus(o.phi, o.theta, o.psi, o.v);
        const Vek3 e2 = kuaterniyon_donus(q, o.v);
        char t[200];
        std::snprintf(t, sizeof t, "φ %.2f θ %.2f ψ %.2f: v_n %s; Euler matrisinden sapma %.2e, q⊗v⊗q* sapması %.2e, |v_n| − |v_b| %.2e",
                      o.phi, o.theta, o.psi, v3(v).c_str(), norm(fark(v, e1)), norm(fark(v, e2)), norm(v) - norm(o.v));
        dogrula_a2(yakin(v, e1, 1e-14) && yakin(v, e2, 1e-14) && std::fabs(norm(v) - norm(o.v)) < 1e-14, t);
        // Sınamanın dişi: bu yönelimlerde v_b'nin aynen taşınması ve ters dönüş (Rᵀ) belirgin biçimde yanlıştır.
        const Vek3 ters = kuaterniyon_donus(eslenik(q), o.v);
        dogrula_a2(norm(fark(e1, o.v)) > 0.1 && norm(fark(e1, ters)) > 0.1,
                   "  aynı yönelimde v_b ve Rᵀ·v_b doğru değerden uzak (dönüşümsüz ve ters dönüşüm ayırt edilir)");
    }
    // ---- govde_ani: alanların çerçeveleri ----
    {
        ida::fizik::DurumAnlik d;
        d.adim_no = 1234;
        d.konum_n = {1.5, -2.5, 0.75};
        d.yonelim = euler_q(0.3, -0.2, 2.0);
        d.dogrusal_hiz_b = {0.6, 0.1, -0.05};
        d.acisal_hiz_b = {0.01, -0.02, 0.3};
        const ida::a2::GovdeAni a = a2b::govde_ani(77, d);
        const Vek3 beklenen = euler_donus(0.3, -0.2, 2.0, d.dogrusal_hiz_b);
        dogrula_a2(a.kimlik == 77u && a.gecerli && a.p_n == d.konum_n && a.q_nb == d.yonelim && a.w_b == d.acisal_hiz_b,
                   "govde_ani: kimlik, p_n, q_nb ve w_b dünyadan bit düzeyinde aynen (w_b gövde çerçevesinde)");
        dogrula_a2(yakin(a.v_n, beklenen, 1e-14) && !yakin(a.v_n, d.dogrusal_hiz_b, 0.1),
                   "govde_ani: v_n = R(q_nb) · v_b " + v3(a.v_n) + " (v_b " + v3(d.dogrusal_hiz_b) + " değil)");
    }
    return g_hata_a2;
}

int test_a2_fizik_tutarlilik()
{
    using namespace ida::fizik;
    AracParametreleri arac = ida0_arac();
    const OrtamParametreleri ortam = havuz_ortami();
    BaslangicDurumu bas;
    bas.konum = {0.0, 0.0, 1.2};                 // batık (yüzey etkisinden uzak)
    bas.yonelim = euler_q(-0.2, 0.15, 0.9);      // eksenle hizalı olmayan başlangıç yönelimi
    Dunya dn;
    const KurulumSonucu kr = dn.kur(arac, ortam, bas);
    dogrula_a2(kr.tamam, "fizik kuruldu" + (kr.tamam ? std::string() : ": " + kr.hata));
    if (!kr.tamam) {
        return g_hata_a2;
    }
    const double h = 1e-9 * static_cast<double>(dn.adim_ns());
    // ESC başlatması için nötr, sonra bütün eksenlerde kuvvet ve moment üreten sabit bir komut.
    const auto notr_adim = static_cast<std::uint64_t>(arac.esc.baslatma_suresi_ns / dn.adim_ns()) + 40u;
    std::array<double, kIticiSayisi> notr{};
    notr.fill(static_cast<double>(arac.esc.pwm_notr_us));
    const std::array<double, kIticiSayisi> komut{1640.0, 1410.0, 1590.0, 1440.0, 1610.0, 1380.0, 1560.0, 1460.0};
    const std::uint64_t n = notr_adim + 6400u;   // 8 s itki

    Vek3 p_onceki{};
    bool onceki_var = false;
    double en_buyuk_v = 0.0, en_buyuk_w = 0.0;
    double en_kotu_oran_v = 0.0, en_kotu_oran_w = 0.0;       // fark / sınır
    double en_buyuk_fark_v = 0.0, en_buyuk_fark_w = 0.0;
    double yanlis_v_en_buyuk = 0.0, yanlis_w_en_buyuk = 0.0; // yanlış çerçevenin farkı / sınır
    std::uint64_t ihlal_v = 0, ihlal_w = 0, olculen = 0;
    for (std::uint64_t k = 0; k < n; ++k) {
        (void)dn.itici_komutu_ver(k, k < notr_adim ? notr : komut);
        const DurumAnlik d0 = dn.durum();
        const ida::a2::GovdeAni a = a2b::govde_ani(1, d0);
        if (dn.adim() != AdimDurumu::tamam) {
            dogrula_a2(false, "fizik adımı");
            return g_hata_a2;
        }
        const DurumAnlik& d1 = dn.durum();
        if (onceki_var && k >= notr_adim) {
            // Konum: p(t + h) − p(t) = v_n h + ½ a h² + …; ivme terimi ikinci farktan (a h²).
            const Vek3 dp = fark(d1.konum_n, d0.konum_n);
            const Vek3 ikinci = {d1.konum_n[0] - 2.0 * d0.konum_n[0] + p_onceki[0], d1.konum_n[1] - 2.0 * d0.konum_n[1] + p_onceki[1],
                                 d1.konum_n[2] - 2.0 * d0.konum_n[2] + p_onceki[2]};
            const double sinir_v = norm(ikinci) + 5e-8;
            const double f_v = norm(fark(dp, {a.v_n[0] * h, a.v_n[1] * h, a.v_n[2] * h}));
            ihlal_v += f_v <= sinir_v ? 0u : 1u;
            en_kotu_oran_v = std::max(en_kotu_oran_v, f_v / sinir_v);
            en_buyuk_fark_v = std::max(en_buyuk_fark_v, f_v);
            const double y_v = norm(fark(dp, {d0.dogrusal_hiz_b[0] * h, d0.dogrusal_hiz_b[1] * h, d0.dogrusal_hiz_b[2] * h}));
            yanlis_v_en_buyuk = std::max(yanlis_v_en_buyuk, y_v / sinir_v);
            // Yönelim: q(t)* ⊗ q(t + h) ≈ (1, ½ w_b h): artış GÖVDE çerçevesindedir.
            Kuaterniyon dq = carp(eslenik(d0.yonelim), d1.yonelim);
            if (dq[0] < 0.0) {
                dq = {-dq[0], -dq[1], -dq[2], -dq[3]};
            }
            const Vek3 aci = {2.0 * dq[1], 2.0 * dq[2], 2.0 * dq[3]};
            const double sinir_w = norm(d0.acisal_ivme_b) * h * h + 2e-8;
            const double f_w = norm(fark(aci, {a.w_b[0] * h, a.w_b[1] * h, a.w_b[2] * h}));
            ihlal_w += f_w <= sinir_w ? 0u : 1u;
            en_kotu_oran_w = std::max(en_kotu_oran_w, f_w / sinir_w);
            en_buyuk_fark_w = std::max(en_buyuk_fark_w, f_w);
            const Vek3 w_n = a2b::govdeden_ned(d0.yonelim, d0.acisal_hiz_b);   // yanlış: dünya çerçevesi
            const double y_w = norm(fark(aci, {w_n[0] * h, w_n[1] * h, w_n[2] * h}));
            yanlis_w_en_buyuk = std::max(yanlis_w_en_buyuk, y_w / sinir_w);
            en_buyuk_v = std::max(en_buyuk_v, norm(a.v_n));
            en_buyuk_w = std::max(en_buyuk_w, norm(a.w_b));
            ++olculen;
        }
        p_onceki = d0.konum_n;
        onceki_var = true;
    }
    std::printf("  %llu adım (h = %.5f s): en büyük |v| %.3f m/s, |ω| %.3f rad/s\n", static_cast<unsigned long long>(olculen), h,
                en_buyuk_v, en_buyuk_w);
    std::printf("  konum: en büyük |Δp − v_n·h| %.3e m; sınıra oranı en çok %.3f; v_b aynen taşınsaydı oran %.0f\n", en_buyuk_fark_v,
                en_kotu_oran_v, yanlis_v_en_buyuk);
    std::printf("  yönelim: en büyük |Δθ − w_b·h| %.3e rad; sınıra oranı en çok %.3f; ω dünya çerçevesinde verilseydi oran %.0f\n",
                en_buyuk_fark_w, en_kotu_oran_w, yanlis_w_en_buyuk);
    dogrula_a2(en_buyuk_v > 0.2 && en_buyuk_w > 0.2, "koşu hareketli (|v| > 0,2 m/s ve |ω| > 0,2 rad/s görüldü)");
    dogrula_a2(ihlal_v == 0u, "her adımda |p(t+h) − p(t) − v_n·h| ≤ bir adımın ivme terimi (|a|h², ikinci farktan) + 5e-8 m");
    dogrula_a2(ihlal_w == 0u, "her adımda |Δθ_b − w_b·h| ≤ |ω̇|h² + 2e-8 rad (w_b gövde çerçevesinde)");
    dogrula_a2(yanlis_v_en_buyuk > 20.0 && yanlis_w_en_buyuk > 20.0,
               "sınamanın dişi: yanlış çerçeve (v_b aynen; ω dünya çerçevesinde) sınırı en az 20 kat aşar");
    return g_hata_a2;
}

int test_a2_tanim()
{
    using namespace ida::senaryo;
    const DiskKaynagi disk(IDA_DEPO_KOKU);
    const std::string kok = IDA_DEPO_KOKU;

    // ---- Depodaki senaryo (diskten) ----
    {
        const CozumSonucu cs = senaryo_coz(disk, "scenarios/gorev-havuz.toml");
        dogrula_a2(cs.rapor.gecerli && cs.cozumlenmis.has_value(), "scenarios/gorev-havuz.toml çözüldü");
        if (!cs.cozumlenmis) {
            return g_hata_a2;
        }
        a2b::Tanim t;
        std::string hata;
        const a2b::TanimSonucu r = a2b::tanim_kur(*cs.cozumlenmis, cs.rapor, disk, kok, t, hata);
        dogrula_a2(r == a2b::TanimSonucu::tamam, "tanım kuruldu" + (hata.empty() ? std::string() : ": " + hata));
        dogrula_a2(t.kademe == ida::a2::Kademe::sade, "kalite.gorunum varsayılanı sade (A5-K78)");
        const std::filesystem::path beklenen =
            (std::filesystem::absolute(std::filesystem::path(kok)) / "assets" / "ornek" / "manifest.json").lexically_normal();
        const std::u8string u = beklenen.u8string();
        dogrula_a2(t.manifest_yolu == std::string(u.begin(), u.end()) && std::filesystem::path(t.manifest_yolu).is_absolute(),
                   "manifest yolu mutlak ve senaryonun kümesini gösteriyor: " + t.manifest_yolu);
        // Özet: dosyanın ham baytlarından bağımsız hesap ve senaryonun beyanı (A5-K115).
        const std::string bayt = disk.oku("assets/ornek/manifest.json").value_or("");
        const std::string hex = sha256_hex(bayt);
        std::string tanim_hex;
        for (std::uint8_t b : t.manifest_sha256) {
            char c[3];
            std::snprintf(c, sizeof c, "%02x", static_cast<unsigned>(b));
            tanim_hex += c;
        }
        dogrula_a2(tanim_hex == hex && hex == "29063c2d7f89a80e452dfbb66504e9470d5c928d0fcf7dd295497e2b22a22ca4",
                   "manifest özeti dosyanın SHA-256'sı ve senaryodaki manifest_ozeti: " + tanim_hex);
        dogrula_a2(t.arac_varlik == 0u && t.ortam.statik_varlik_indeksi == 3u,
                   "varlık indeksleri: araç ida0 → 0, sahne havuz_ornek → 3 (assets/ornek manifest sırası)");
        const ida::a2::Ortam& o = t.ortam;
        dogrula_a2(o.ortam_turu == static_cast<std::uint8_t>(ida::a2::OrtamTuru::havuz) && o.statik_p_n[0] == 0.0 &&
                       o.statik_p_n[1] == 0.0 && o.statik_p_n[2] == 0.0 && o.statik_q_nb[0] == 1.0 && o.statik_q_nb[1] == 0.0,
                   "ortam: havuz, statik varlık pozu birim (ortam.sahne_poz varsayılanı)");
        // Güneş yönü ve ışınımı senaryoda verilmez: A5-K101 kalibre edilmedi (KE) varsayılanı. Su optiği
        // sürümü ve bulanıklık ölçeği de varsayılandır.
        dogrula_a2(o.gunes_yonu_n[0] == 0.0 && o.gunes_yonu_n[1] == 0.0 && o.gunes_yonu_n[2] == -1.0 &&
                       o.gunes_isinimi_w_m2 == 1000.0 && o.su_optigi_surumu == 1u && o.bulaniklik_olcegi == 0.0,
                   "ortam: güneş yönü ve ışınımı KE varsayılanı (A5-K101: (0, 0, −1), 1000 W/m²), su optiği sürümü 1, bulanıklık ölçeği 0");
        // W5-viz-gorsel: senaryonun `[ortam.su_optigi]` ve `[ortam.isik]` değerleri A2 `Ortam`'a aynen ulaşır
        // (verilmeseler sim 0 yazardı: A5-K101 "kalibre edilmedi", berrak su ve ışıksız sahne).
        dogrula_a2(o.beta_d_m[0] == 0.30 && o.beta_d_m[1] == 0.055 && o.beta_d_m[2] == 0.04 && o.beta_b_m[0] == 0.11 &&
                       o.beta_b_m[1] == 0.065 && o.beta_b_m[2] == 0.055 && o.b_sonsuz_rgb_dogrusal[0] == 0.02 &&
                       o.b_sonsuz_rgb_dogrusal[1] == 0.20 && o.b_sonsuz_rgb_dogrusal[2] == 0.26 &&
                       o.ortam_isigi_rgb_dogrusal[0] == 0.30 && o.ortam_isigi_rgb_dogrusal[1] == 0.36 &&
                       o.ortam_isigi_rgb_dogrusal[2] == 0.42,
                   "ortam: senaryonun su optiği (β_d, β_b, B∞) ve ortam ışığı A2 Ortam'a aynen ulaştı (sıfır değil)");
        dogrula_a2(t.malzeme.empty(), "optik malzeme geçersiz kılması yok: 0 girdi");
    }

    // ---- Depodaki açık su görev senaryosu (W5-viz-gorsel): acik_su kümesi, tam kademe, eğik güneş, su optiği ----
    {
        const CozumSonucu cs = senaryo_coz(disk, "scenarios/gorev-acik-su-3b.toml");
        dogrula_a2(cs.rapor.gecerli && cs.cozumlenmis.has_value(), "scenarios/gorev-acik-su-3b.toml çözüldü");
        if (cs.cozumlenmis) {
            a2b::Tanim t;
            std::string hata;
            const a2b::TanimSonucu r = a2b::tanim_kur(*cs.cozumlenmis, cs.rapor, disk, kok, t, hata);
            dogrula_a2(r == a2b::TanimSonucu::tamam, "açık su: tanım kuruldu" + (hata.empty() ? std::string() : ": " + hata));
            dogrula_a2(t.kademe == ida::a2::Kademe::tam, "açık su: kalite.gorunum = tam → kademe tam");
            const std::filesystem::path beklenen =
                (std::filesystem::absolute(std::filesystem::path(kok)) / "assets" / "acik_su" / "manifest.json").lexically_normal();
            const std::u8string u = beklenen.u8string();
            dogrula_a2(t.manifest_yolu == std::string(u.begin(), u.end()), "açık su: manifest yolu assets/acik_su: " + t.manifest_yolu);
            std::string tanim_hex;
            for (std::uint8_t b : t.manifest_sha256) {
                char c[3];
                std::snprintf(c, sizeof c, "%02x", static_cast<unsigned>(b));
                tanim_hex += c;
            }
            const std::string dosya_hex = sha256_hex(disk.oku("assets/acik_su/manifest.json").value_or(""));
            dogrula_a2(!dosya_hex.empty() && tanim_hex == dosya_hex, "açık su: manifest özeti dosyanın SHA-256'sı: " + tanim_hex);
            const ida::a2::Ortam& o = t.ortam;
            dogrula_a2(o.ortam_turu == static_cast<std::uint8_t>(ida::a2::OrtamTuru::acik_su) && o.statik_varlik_indeksi == 2u &&
                           t.arac_varlik == 0u,
                       "açık su: ortam açık su, statik varlık deniz_tabani (2), araç ida0 (0)");
            dogrula_a2(o.gunes_yonu_n[2] == -0.8137334712067349 && o.gunes_yonu_n[0] == 0.4649905549752772 &&
                           o.beta_d_m[0] == 0.40 && o.beta_d_m[1] == 0.070 && o.beta_d_m[2] == 0.050 && o.beta_b_m[0] == 0.20 &&
                           o.b_sonsuz_rgb_dogrusal[2] == 0.16 && o.ortam_isigi_rgb_dogrusal[2] == 0.55,
                       "açık su: eğik güneş, su optiği ve ortam ışığı senaryodan A2 Ortam'a aynen ulaştı");
        }
    }

    // ---- Bellekte türetilmiş senaryolar (araç dosyası ve manifest diskten) ----
    const std::string taban = disk.oku("scenarios/gorev-havuz.toml").value_or("");
    const auto kaynak = [&](std::string metin) {
        auto b = std::make_shared<BellekKaynagi>();
        for (const char* y : {"scenarios/ida0.toml", "assets/ornek/manifest.json"}) {
            b->dosyalar[y] = disk.oku(y).value_or("");
        }
        b->dosyalar["scenarios/t.toml"] = std::move(metin);
        return b;
    };
    const auto degistir = [](std::string s, const std::string& a, const std::string& b) {
        const std::size_t i = s.find(a);
        if (i == std::string::npos) {
            dogrula_a2(false, "sınama hatası: metinde yok: " + a);
            return s;
        }
        s.replace(i, a.size(), b);
        return s;
    };
    {
        const auto k = kaynak(degistir(taban, "kamera = \"kapali\"", "kamera = \"kapali\"\ngorunum = \"kapali\""));
        const CozumSonucu cs = senaryo_coz(*k, "scenarios/t.toml");
        a2b::Tanim t;
        std::string hata;
        dogrula_a2(cs.cozumlenmis.has_value() &&
                       a2b::tanim_kur(*cs.cozumlenmis, cs.rapor, *k, kok, t, hata) == a2b::TanimSonucu::gorunum_kapali,
                   "kalite.gorunum = kapali → gorunum_kapali (A2-K02)");
    }
    {
        const auto k = kaynak(degistir(taban, "kamera = \"kapali\"", "kamera = \"kapali\"\ngorunum = \"tam\""));
        const CozumSonucu cs = senaryo_coz(*k, "scenarios/t.toml");
        a2b::Tanim t;
        std::string hata;
        dogrula_a2(cs.cozumlenmis.has_value() && a2b::tanim_kur(*cs.cozumlenmis, cs.rapor, *k, kok, t, hata) == a2b::TanimSonucu::tamam &&
                       t.kademe == ida::a2::Kademe::tam,
                   "kalite.gorunum = tam → kademe tam");
    }
    {
        // Optik geçersiz kılma (A7-K30): malzemeler[] indeksi ve maske.
        const std::string ek = "\n[kalibrasyon.malzeme.havuz_beton.optik]\ntaban_rengi_dogrusal = [0.1, 0.2, 0.3, 1.0]\npuruzluluk = 0.8\n"
                               "\n[kalibrasyon.malzeme.govde_plastik.optik]\nmetaliklik = 0.25\n";
        const auto k = kaynak(taban + ek);
        const CozumSonucu cs = senaryo_coz(*k, "scenarios/t.toml");
        a2b::Tanim t;
        std::string hata;
        const bool tamam = cs.cozumlenmis.has_value() &&
                           a2b::tanim_kur(*cs.cozumlenmis, cs.rapor, *k, kok, t, hata) == a2b::TanimSonucu::tamam;
        dogrula_a2(tamam && t.malzeme.size() == 2u, "iki optik malzeme geçersiz kılması" +
                                                        (cs.cozumlenmis ? std::string() : std::string(": senaryo reddedildi: ") + cs.rapor.json()) +
                                                        (hata.empty() ? std::string() : ": " + hata));
        if (tamam && t.malzeme.size() == 2u) {
            // Tablo anahtar sırası: govde_plastik (indeks 0), havuz_beton (indeks 2).
            const ida::a2::MalzemeOptikGirdisi& g = t.malzeme[0];
            const ida::a2::MalzemeOptikGirdisi& b = t.malzeme[1];
            dogrula_a2(g.malzeme_indeksi == 0u && g.maske == 0x2u && g.metaliklik == 0.25,
                       "govde_plastik: indeks 0, maske metaliklik, değer 0,25");
            dogrula_a2(b.malzeme_indeksi == 2u && b.maske == 0x5u && b.taban_rengi_dogrusal[0] == 0.1 &&
                           b.taban_rengi_dogrusal[2] == 0.3 && b.taban_rengi_dogrusal[3] == 1.0 && b.puruzluluk == 0.8,
                       "havuz_beton: indeks 2, maske taban rengi + pürüzlülük");
        }
    }
    {
        // Manifest doğrulamadan sonra değişti: tanım kurulmaz (A2'siz sürer), koşu etkilenmez.
        const auto k = kaynak(taban);
        const CozumSonucu cs = senaryo_coz(*k, "scenarios/t.toml");
        k->dosyalar["assets/ornek/manifest.json"] += " ";
        a2b::Tanim t;
        std::string hata;
        dogrula_a2(cs.cozumlenmis.has_value() && a2b::tanim_kur(*cs.cozumlenmis, cs.rapor, *k, kok, t, hata) == a2b::TanimSonucu::hata &&
                       !hata.empty(),
                   "manifest doğrulamadan sonra değiştiyse tanım kurulmaz: " + hata);
    }
    return g_hata_a2;
}

int test_a2_kimlik()
{
    std::uint32_t onceki = a2b::yeni_govde_kimligi();
    bool artan = onceki >= 1u;
    for (int i = 0; i < 1000; ++i) {
        const std::uint32_t k = a2b::yeni_govde_kimligi();
        artan = artan && k == onceki + 1u;
        onceki = k;
    }
    dogrula_a2(artan, "gövde kimliği 1'den başlar ve her çağrıda tam 1 artar (yeniden kullanılmaz; W5-K32)");
    dogrula_a2(a2b::dolu_tup_maskesi({true, true}) == 3u && a2b::dolu_tup_maskesi({true, false}) == 1u &&
                   a2b::dolu_tup_maskesi({false, true}) == 2u && a2b::dolu_tup_maskesi({false, false}) == 0u,
               "dolu tüp maskesi: bit k = tüp k+1 (A2 §3.3)");
    return g_hata_a2;
}
