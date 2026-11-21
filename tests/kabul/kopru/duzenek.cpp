#include "duzenek.hpp"

#include <array>
#include <utility>

namespace kabul {

namespace {

ida::kopru::KopruAyari kopru_ayari(ida::kopru::CalistirmaKipi kip,
                                   const ida::kopru::KopruAyari* ek,
                                   const SahteKart::Ayar& kart) {
    ida::kopru::KopruAyari a = (ek != nullptr) ? *ek : ida::kopru::KopruAyari{};
    a.kip = kip;
    a.beklenen_blok_crc = kart.blok_crc;
    return a;
}

SahteKart::Ayar kart_ayari_kur(ida::kopru::CalistirmaKipi kip, const ida::kopru::KopruAyari* ek,
                               const SahteKart::Ayar* kart_ayari) {
    SahteKart::Ayar a = (kart_ayari != nullptr) ? *kart_ayari : SahteKart::Ayar{};
    a.rol = (kip == ida::kopru::CalistirmaKipi::sil) ? w::kRolSil : w::kRolKart;
    if (ek != nullptr) {
        a.valid_for_ns = static_cast<std::uint32_t>(ek->valid_for_ns);
        a.esc_notr = ek->esc_notr_us;
    }
    return a;
}

}  // namespace

Duzenek::Duzenek(ida::kopru::CalistirmaKipi kip, const ida::kopru::KopruAyari* ek,
                 const SahteKart::Ayar* kart_ayari)
    : k(kopru_ayari(kip, ek, kart_ayari_kur(kip, ek, kart_ayari))),
      kart(kart_ayari_kur(kip, ek, kart_ayari)),
      kip_(kip) {
    auto cift = ida::kopru::bellek_cifti();
    sim_uc_ = std::move(cift.first);
    kart_uc_ = std::move(cift.second);
    kart.tf_geldi = [this](const GelenTf& tf, std::int64_t t) {
        ++parti_sayisi;
        if (!tek_seferlik_.empty()) {
            Yanitci f = std::move(tek_seferlik_.front());
            tek_seferlik_.pop_front();
            f(tf, t);
            return;
        }
        if (kalici_yanitci) {
            kalici_yanitci(tf, t);
            return;
        }
        if (varsayilan_acik) {
            varsayilan_yanit(tf, t);
        }
    };
}

void Duzenek::varsayilan_yanit(const GelenTf& tf, std::int64_t t) {
    kart.yanit_gonder(kart.yanit(tf, varsayilan_pwm, 0), t);
}

void Duzenek::pompa() {
    std::vector<std::uint8_t> v;
    std::array<std::uint8_t, 4096> tampon{};
    for (int tur = 0; tur < 64; ++tur) {
        bool is = false;
        v.clear();
        k.giden_al(v);
        if (!v.empty()) {
            sim_uc_->yaz(v.data(), v.size());
            is = true;
        }
        for (;;) {
            std::size_t okunan = 0;
            const auto s = kart_uc_->oku(tampon.data(), tampon.size(), 0, okunan);
            if (s != ida::kopru::OkumaSonucu::tamam || okunan == 0) {
                break;
            }
            kart.al(tampon.data(), okunan, t_simdi_);
        }
        for (auto it = kart.giden.begin(); it != kart.giden.end();) {
            if (it->t_birak <= t_simdi_) {
                kart_uc_->yaz(it->bayt.data(), it->bayt.size());
                it = kart.giden.erase(it);
                is = true;
            } else {
                ++it;
            }
        }
        for (;;) {
            std::size_t okunan = 0;
            const auto s = sim_uc_->oku(tampon.data(), tampon.size(), 0, okunan);
            if (s != ida::kopru::OkumaSonucu::tamam || okunan == 0) {
                break;
            }
            k.al(tampon.data(), okunan, duvar(t_simdi_));
            is = true;
        }
        if (!is) {
            break;
        }
    }
}

bool Duzenek::kur() {
    t_simdi_ = 0;
    std::int64_t d = 0;
    k.el_sikisma_baslat(d);
    for (int i = 0; i < 2000; ++i) {
        // El sıkışmada fizik yoktur; duvar ilerler, sim zamanı 0'da kalır.
        duvar0_ = d;
        pompa();
        if (k.durum() == ida::kopru::OturumDurumu::kosuyor) {
            duvar0_ = d + kMs;
            return true;
        }
        d += kMs;
        k.zamanla(d);
    }
    return false;
}

void Duzenek::adim() {
    const std::int64_t t = static_cast<std::int64_t>(b_) * kH;
    t_simdi_ = t;
    const std::int64_t dv = duvar(t);
    k.zamanla(dv);
    if (otomatik_yeniden && k.durum() == ida::kopru::OturumDurumu::kapali) {
        k.el_sikisma_baslat(dv);
    }
    pompa();
    const ida::kopru::SinirSonucu r = k.sinir_isle(b_, dv);
    if (sinirlar_.size() <= b_) {
        sinirlar_.resize(b_ + 1);
        emn_.resize(b_ + 1);
        durum_.resize(b_ + 1, ida::kopru::OturumDurumu::kapali);
        islendi_.resize(b_ + 1, false);
    }
    sinirlar_[b_] = r;
    emn_[b_] = k.dis_durum().emniyet;
    durum_[b_] = k.durum();
    islendi_[b_] = true;
    if (sinir_sonrasi) {
        sinir_sonrasi(b_);
    }
    pompa();
    if (emniyet_kaydi_uret) {
        emniyet_kaydi(t);
    }
    if (b_ % 4u == 0u) {
        ida::sensor::Olcum o;
        o.hdr.tur = ida::sensor::SensorTuru::imu;
        o.hdr.sensor_id = 1;
        o.hdr.inst = 0;
        o.hdr.seq = ++imu_seq_;
        o.hdr.t_meas_ns = t;
        o.hdr.window_ns = static_cast<std::uint32_t>(kP);
        o.hdr.t_hazir_ns = t;
        o.hdr.t_teslim_ns = t;
        o.hdr.flags = ida::sensor::kBayrakGecerli;
        ida::sensor::ImuOlcumu imu;
        imu.accel = {0.0F, 0.0F, -9.80665F};
        o.yuk = imu;
        k.olcum_ekle(o);
        const std::int64_t w_tn = (kip_ == ida::kopru::CalistirmaKipi::sil) ? -1 : dv;
        k.parti_kapat(t, dv, w_tn);
        pompa();
    }
    ++b_;
}

std::uint8_t Duzenek::tup_bitleri(const ida::sensor::EmniyetDurumu& e, int i) {
    const auto u = static_cast<std::size_t>(i);
    std::uint8_t v = 0;
    if (e.dolu[u]) {
        v = static_cast<std::uint8_t>(v | w::kTupDolu);
    }
    if (e.fis[u]) {
        v = static_cast<std::uint8_t>(v | w::kTupFis);
    }
    if (!e.pim[u]) {
        v = static_cast<std::uint8_t>(v | w::kTupPimCikik);
    }
    if (e.izin_acik[u]) {
        v = static_cast<std::uint8_t>(v | w::kTupIzin);
    }
    return v;
}

void Duzenek::emniyet_kaydi_ekle(std::int64_t t, const ida::sensor::EmniyetDurumu& e) {
    ida::sensor::Olcum o;
    o.hdr.tur = ida::sensor::SensorTuru::firlatici_geri_okuma;
    o.hdr.sensor_id = 90;
    o.hdr.inst = 0;
    o.hdr.seq = ++emniyet_seq_;
    o.hdr.t_meas_ns = t;
    o.hdr.window_ns = 0;
    o.hdr.t_hazir_ns = t;
    o.hdr.t_teslim_ns = t;
    o.hdr.flags = ida::sensor::kBayrakGecerli;
    ida::sensor::EmniyetOlcumu m;
    m.estop = static_cast<std::uint8_t>(e.acil_durdurma ? 1u : 0u);
    m.tube[0] = tup_bitleri(e, 0);
    m.tube[1] = tup_bitleri(e, 1);
    m.safety_rev = e.emniyet_revizyonu;
    o.yuk = m;
    k.olcum_ekle(o);
    ++emniyet_kaydi_sayisi;
}

// Sensör kümesinin emniyet geri okumasının (05 §2.3.8; A1-K56) taklidi: yetkili durum ya da
// revizyon değişince ve 100 ms'de bir, değişim anı damgalı bir kayıt.
void Duzenek::emniyet_kaydi(std::int64_t t) {
    const ida::sensor::EmniyetDurumu e = k.dis_durum().emniyet;
    std::array<std::uint8_t, 2> tb{tup_bitleri(e, 0), tup_bitleri(e, 1)};
    const bool degisti = !son_emniyet_var_ || e.acil_durdurma != son_estop_ ||
                         tb != son_tup_ || e.emniyet_revizyonu != son_rev_;
    if (!degisti && t - son_emniyet_t_ < 100 * kMs) {
        return;
    }
    emniyet_kaydi_ekle(t, e);
    son_emniyet_var_ = true;
    son_estop_ = e.acil_durdurma;
    son_tup_ = tb;
    son_rev_ = e.emniyet_revizyonu;
    son_emniyet_t_ = t;
}

void Duzenek::ilerle(int n) {
    for (int i = 0; i < n; ++i) {
        adim();
    }
}

void Duzenek::ilerle_kadar(std::int64_t t) {
    while (static_cast<std::int64_t>(b_) * kH <= t) {
        adim();
    }
}

bool Duzenek::yeniden_kur(int en_cok_sinir) {
    for (int i = 0; i < en_cok_sinir; ++i) {
        if (k.durum() == ida::kopru::OturumDurumu::kosuyor && kart.kosuyor()) {
            return true;
        }
        adim();
    }
    return k.durum() == ida::kopru::OturumDurumu::kosuyor && kart.kosuyor();
}

GelenTf Duzenek::sonraki_tf() {
    const std::size_t once = kart.partiler.size();
    for (int i = 0; i < 64 && kart.partiler.size() == once; ++i) {
        adim();
    }
    return kart.partiler.empty() ? GelenTf{} : kart.partiler.back().tf;
}

void Duzenek::emniyet(std::optional<bool> acil, std::optional<bool> fis0,
                      std::optional<bool> fis1, std::optional<bool> pim0,
                      std::optional<bool> pim1) {
    ida::kopru::EmniyetAtama a;
    a.acil_durdurma = acil;
    a.fis[0] = fis0;
    a.fis[1] = fis1;
    a.pim[0] = pim0;
    a.pim[1] = pim1;
    k.emniyet_ata(a);
}

void Duzenek::izinli_durum() {
    emniyet(false, true, true, false, false);
}

bool Duzenek::hepsi(std::uint64_t b, double deger) const {
    for (const double p : sinirlar_.at(b).pwm_us) {
        if (p != deger) {
            return false;
        }
    }
    return true;
}

}  // namespace kabul
