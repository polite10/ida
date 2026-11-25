// sahte_dunya.cpp — bkz. sahte_dunya.hpp.
#include "sahte_dunya.hpp"

#include <algorithm>
#include <cmath>

namespace ida::a2::sahte {

namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr double kYaricap = 4.0;         // m
constexpr double kAcisalHiz = 0.15;      // rad/s (≈ 0,6 m/s)
constexpr double kDerinlik = 2.0;        // m (NED: aşağı pozitif)
constexpr double kGeriTepme = 0.05;      // m/s: fırlatmada aracın hız sıçraması (süreklilik sınırı)
constexpr double kTorpidoHizi = 8.0;     // m/s

double t_s(std::uint64_t adim_no) { return static_cast<double>(adim_no) * (static_cast<double>(kAdimNs) * 1e-9); }

// Yalnız sapma (z ekseni) dönüşü: q = (cos ψ/2, 0, 0, sin ψ/2).
std::array<double, 4> sapma_q(double psi) { return {std::cos(psi * 0.5), 0.0, 0.0, std::sin(psi * 0.5)}; }

}  // namespace

SahteDunya::SahteDunya(Yayinci& y, SahteAyar a) : y_(y), a_(std::move(a))
{
    if (a_.hedef_sayisi > 62) {
        a_.hedef_sayisi = 62;
    }
    bilesenler_.resize(a_.dalga_bileseni);
    for (std::size_t i = 0; i < bilesenler_.size(); ++i) {
        const double k = 0.3 + 0.2 * static_cast<double>(i);
        DalgaBileseni b{};
        b.a = 0.05 / (1.0 + static_cast<double>(i));
        b.k = k;
        b.omega = std::sqrt(9.80665 * k);  // derin su dağılım bağıntısı
        b.beta = 0.4 * static_cast<double>(i);
        b.phi = 0.7 * static_cast<double>(i) + 0.1;
        bilesenler_[i] = b;
    }
}

DalgaGirdisi SahteDunya::dalga() const
{
    DalgaGirdisi d;
    d.derin_su = true;
    d.tohum = 0x1DA2u;
    d.g = 9.80665;
    d.rho = 1025.0;
    d.derinlik_m = 0.0;
    d.bilesenler = std::span<const DalgaBileseni>(bilesenler_.data(), bilesenler_.size());
    return d;
}

Ortam SahteDunya::ortam() const
{
    Ortam o{};
    o.ortam_turu = static_cast<std::uint8_t>(OrtamTuru::acik_su);
    o.statik_varlik_indeksi = 2;
    o.statik_p_n[2] = 6.0;
    o.statik_q_nb[0] = 1.0;
    o.gunes_yonu_n[0] = 0.6;
    o.gunes_yonu_n[1] = 0.0;
    o.gunes_yonu_n[2] = -0.8;
    o.gunes_isinimi_w_m2 = 800.0;
    for (std::size_t i = 0; i < 3; ++i) {
        o.ortam_isigi_rgb_dogrusal[i] = 0.2;
        o.beta_d_m[i] = 0.1 + 0.05 * static_cast<double>(i);
        o.beta_b_m[i] = 0.02;
        o.b_sonsuz_rgb_dogrusal[i] = 0.3;
    }
    o.bulaniklik_olcegi = 0.5;
    o.su_optigi_surumu = 1;
    return o;
}

bool SahteDunya::torpido_yasiyor(std::uint64_t adim_no) const noexcept
{
    return a_.firlatma_adimi != 0 && adim_no >= a_.firlatma_adimi && (a_.isabet_adimi == 0 || adim_no < a_.isabet_adimi);
}

bool SahteDunya::hedef_yasiyor(std::uint32_t i, std::uint64_t adim_no) const noexcept
{
    return i < a_.hedef_sayisi && !(i == 0 && a_.isabet_adimi != 0 && adim_no >= a_.isabet_adimi);
}

GovdeAni SahteDunya::dogru_arac(std::uint64_t adim_no) const
{
    const double t = t_s(adim_no);
    const double aci = kAcisalHiz * t;
    GovdeAni g;
    g.kimlik = arac_kimligi();
    g.p_n = {kYaricap * std::cos(aci), kYaricap * std::sin(aci), kDerinlik};
    g.v_n = {-kYaricap * kAcisalHiz * std::sin(aci), kYaricap * kAcisalHiz * std::cos(aci), 0.0};
    g.q_nb = sapma_q(aci + kPi * 0.5);  // burun teğet yönde
    g.w_b = {0.0, 0.0, kAcisalHiz};
    if (a_.firlatma_adimi != 0 && adim_no >= a_.firlatma_adimi) {
        // Fırlatma sınırından itibaren: fırlatma anındaki burun yönünün tersine sabit hız eklenir
        // (konum sürekli, hız sıçrar).
        const double a0 = kAcisalHiz * t_s(a_.firlatma_adimi);
        const double bx = -std::sin(a0);
        const double by = std::cos(a0);
        const double dt = t - t_s(a_.firlatma_adimi);
        g.p_n[0] -= kGeriTepme * dt * bx;
        g.p_n[1] -= kGeriTepme * dt * by;
        g.v_n[0] -= kGeriTepme * bx;
        g.v_n[1] -= kGeriTepme * by;
    }
    return g;
}

GovdeAni SahteDunya::dogru_hedef(std::uint32_t i, std::uint64_t adim_no) const
{
    const double t = t_s(adim_no);
    GovdeAni g;
    g.kimlik = hedef_kimligi(i);
    g.p_n = {12.0 + 1.5 * static_cast<double>(i % 8u), -6.0 + 1.5 * static_cast<double>(i / 8u), 3.0};
    const double psi = 0.02 * t + 0.1 * static_cast<double>(i);
    g.q_nb = sapma_q(psi);
    g.w_b = {0.0, 0.0, 0.02};
    return g;
}

GovdeAni SahteDunya::dogru_torpido(std::uint64_t adim_no) const
{
    const double a0 = kAcisalHiz * t_s(a_.firlatma_adimi);
    const double bx = -std::sin(a0);
    const double by = std::cos(a0);
    const double dt = t_s(adim_no) - t_s(a_.firlatma_adimi);
    GovdeAni g;
    g.kimlik = torpido_kimligi();
    g.p_n = {kYaricap * std::cos(a0) + (0.8 + kTorpidoHizi * dt) * bx,
             kYaricap * std::sin(a0) + (0.8 + kTorpidoHizi * dt) * by, kDerinlik + 0.1};
    g.v_n = {kTorpidoHizi * bx, kTorpidoHizi * by, 0.0};
    g.q_nb = sapma_q(a0 + kPi * 0.5);
    return g;
}

std::size_t SahteDunya::dogru_anlar(std::uint64_t adim_no, std::array<GovdeAni, kAzamiGovde>& out) const
{
    std::size_t n = 0;
    for (std::uint32_t kimlik : yasayan_) {
        if (kimlik == arac_kimligi()) {
            out[n++] = dogru_arac(adim_no);
        } else if (kimlik == torpido_kimligi()) {
            out[n++] = dogru_torpido(adim_no);
        } else {
            out[n++] = dogru_hedef(kimlik - hedef_kimligi(0), adim_no);
        }
    }
    return n;
}

bool SahteDunya::basla()
{
    adim_ = 0;
    yasayan_.clear();
    std::array<GovdeKaydi, kAzamiGovde> kayit{};
    std::array<GovdeAni, kAzamiGovde> an{};
    std::size_t n = 0;
    kayit[n] = GovdeKaydi{arac_kimligi(), kVarlikArac, GovdeTuru::arac, 0x03};
    an[n++] = dogru_arac(0);
    yasayan_.push_back(arac_kimligi());
    for (std::uint32_t i = 0; i < a_.hedef_sayisi; ++i) {
        kayit[n] = GovdeKaydi{hedef_kimligi(i), kVarlikHedef, GovdeTuru::hedef, 0};
        an[n++] = dogru_hedef(i, 0);
        yasayan_.push_back(hedef_kimligi(i));
    }
    CalistirmaTanimi t;
    t.sim_kipi = a_.sim_kipi;
    t.kademe_3b = a_.kademe_3b;
    t.manifest_yolu = a_.manifest_yolu;
    t.manifest_sha256 = a_.manifest_ozeti;
    t.adim_no = 0;
    t.dalga = dalga();
    t.ortam = ortam();
    t.govdeler = std::span<const GovdeKaydi>(kayit.data(), n);
    t.anlar = std::span<const GovdeAni>(an.data(), n);
    return y_.yeni_calistirma(t);
}

void SahteDunya::olaylar_(std::uint64_t adim_no)
{
    if (a_.firlatma_adimi != 0 && adim_no == a_.firlatma_adimi) {
        // Tüp 1 boşalır: torpido doğar, aracın kütle durumu değişir ve geri tepme darbesi alır.
        y_.govde_dogdu(adim_no, GovdeKaydi{torpido_kimligi(), kVarlikTorpido, GovdeTuru::torpido, 0}, 1);
        y_.kutle_durumu(adim_no, arac_kimligi(), 0x02);
        y_.sureklilik_kir(adim_no, arac_kimligi());
        yasayan_.push_back(torpido_kimligi());
    }
    if (a_.isabet_adimi != 0 && adim_no == a_.isabet_adimi) {
        const GovdeAni tp = dogru_torpido(adim_no);
        y_.isabet(adim_no, torpido_kimligi(), hedef_kimligi(0), tp.p_n, {1.0, 0.0, 0.0}, tp.v_n);
        y_.govde_oldu(adim_no, torpido_kimligi(), OlumNedeni::isabet);
        y_.govde_oldu(adim_no, hedef_kimligi(0), OlumNedeni::isabet);
        yasayan_.erase(std::remove(yasayan_.begin(), yasayan_.end(), torpido_kimligi()), yasayan_.end());
        yasayan_.erase(std::remove(yasayan_.begin(), yasayan_.end(), hedef_kimligi(0)), yasayan_.end());
    }
}

void SahteDunya::adimla(std::uint64_t n)
{
    std::array<GovdeAni, kAzamiGovde> an{};
    for (std::uint64_t i = 0; i < n; ++i) {
        ++adim_;
        olaylar_(adim_);
        const std::size_t m = dogru_anlar(adim_, an);
        y_.adim_siniri(adim_, std::span<const GovdeAni>(an.data(), m));
    }
}

void SahteDunya::sifirla()
{
    // A2-K23: kimlik oturum içinde yeniden kullanılmaz: sıfırlamadan sonra yeni kimlikler.
    a_.kimlik_tabani += 1000u;
    adim_ = 0;
    yasayan_.clear();
    y_.sifirla(0);
    y_.govde_dogdu(0, GovdeKaydi{arac_kimligi(), kVarlikArac, GovdeTuru::arac, 0x03});
    yasayan_.push_back(arac_kimligi());
    for (std::uint32_t i = 0; i < a_.hedef_sayisi; ++i) {
        y_.govde_dogdu(0, GovdeKaydi{hedef_kimligi(i), kVarlikHedef, GovdeTuru::hedef, 0});
        yasayan_.push_back(hedef_kimligi(i));
    }
    std::array<GovdeAni, kAzamiGovde> an{};
    const std::size_t m = dogru_anlar(0, an);
    y_.adim_siniri(0, std::span<const GovdeAni>(an.data(), m));
}

}  // namespace ida::a2::sahte
