// Kopru: yanıtların ve olayların alınması (A1-K72, K73, K81–K83, K109, K118), fizik
// sınırındaki emniyet işlemleri ve sürekli çıktıların uygulanması (A1-K74–K78, K84–K89,
// K106; iskelet §3.2 "Fizik sınırındaki sıra").
#include "kopru_ic.hpp"

#include "ida/port.h"

#include <algorithm>

namespace ida::kopru {
namespace {

bool yeni(std::uint32_t a, std::uint32_t b)
{
    const std::uint32_t d = a - b;
    return d >= 1u && d <= 0x7FFFFFFFu;
}

constexpr std::uint16_t F(std::uint32_t b) { return static_cast<std::uint16_t>(b); }

}  // namespace

// ---------------------------------------------------------------- Yanıt (IDA_ACTUATOR)

bool Kopru::Ic::yuva_tamam(const Yuva& y) const
{
    if (!y.act_var || y.reddedildi) {
        return false;
    }
    const std::uint8_t rc = y.act.core_rc;
    if (rc == IDA_RC_OK || rc == IDA_RC_IMU_REJECTED) {
        return y.olaylar.size() >= y.act.n_events;
    }
    return true;
}

void Kopru::Ic::yuva_tamamlandi_mi(std::uint32_t seq)
{
    const auto it = yuvalar.find(seq);
    if (it == yuvalar.end()) {
        return;
    }
    Yuva& y = it->second;
    if (y.tamam_duvar < 0 && yuva_tamam(y)) {
        y.tamam_duvar = duvar;
        // A1-K108: yuvayı tamamlayan son çerçeve D_n'den sonra alındıysa.
        if (y.son_tarih >= 0 && duvar > y.son_tarih) {
            ++say.son_tarih_kacirildi;
            say.calistirma_gecersiz |= kGecSonTarih;
        }
    }
}

void Kopru::Ic::yanit_al(const mavlink_ida_actuator_t& m)
{
    const std::uint16_t f = m.act_flags;
    const bool thr = (f & F(IDAW_ACT_FLAGS_THR_VALID)) != 0u;
    const bool guvenli = (f & F(IDAW_ACT_FLAGS_IS_SAFE)) != 0u;
    const bool tick = (f & F(IDAW_ACT_FLAGS_SRC_TICK)) != 0u;
    const bool esz = (f & F(IDAW_ACT_FLAGS_ASYNC)) != 0u;
    // V2 (Tablo 4.6)
    bool v2 = (f & static_cast<std::uint16_t>(~0x03FFu)) == 0u && m.fs_level <= 5u && m.n_events <= 4u &&
              (m.core_rc == IDA_RC_OK || m.core_rc == IDA_RC_BAD_INPUT || m.core_rc == IDA_RC_NOT_INIT ||
               m.core_rc == IDA_RC_IMU_REJECTED);
    if (v2) {
        if (thr) {
            v2 = static_cast<std::int64_t>(m.valid_for_ns) == a.valid_for_ns;
            for (std::uint16_t p : m.pwm_us) {
                v2 = v2 && p >= a.esc_alt_us && p <= a.esc_ust_us;
            }
        } else {
            v2 = m.valid_for_ns == 0u;
            for (std::uint16_t p : m.pwm_us) {
                v2 = v2 && p == 0u;
            }
        }
    }
    if (!v2) {
        ++say.alan_ret;
        return;
    }
    // V3
    if (esz ? (m.batch_seq != 0u && !tf_gonderildi(m.batch_seq)) : !tf_gonderildi(m.batch_seq)) {
        ++say.bag_ret;
        return;
    }
    // V4 (Tablo 4.6a)
    bool v4 = true;
    if (esz && !(thr && guvenli && tick)) {
        v4 = false;
    }
    if (thr && guvenli) {
        for (std::uint16_t p : m.pwm_us) {
            v4 = v4 && p == a.esc_notr_us;
        }
    }
    switch (m.core_rc) {
    case IDA_RC_OK:
        if (!tick) {
            v4 = v4 && thr && !esz;
        } else {
            v4 = v4 && (!thr || guvenli) && (!esz || thr);
        }
        break;
    case IDA_RC_IMU_REJECTED:
        v4 = v4 && !tick && (!thr || guvenli) && !esz;
        break;
    case IDA_RC_BAD_INPUT:
        v4 = v4 && !thr && !esz;
        break;
    case IDA_RC_NOT_INIT:
        v4 = v4 && !thr && guvenli && !esz && m.n_events == 0u;
        break;
    default:
        v4 = false;
        break;
    }
    if (!v4) {
        ++say.alan_ret;
        return;
    }
    // V5 (A1-K105)
    if (out_seq_var && !yeni(m.out_seq, son_out_seq)) {
        ++say.yanit_ret_eski_uretim;
        return;
    }
    out_seq_var = true;
    son_out_seq = m.out_seq;

    if ((f & F(IDAW_ACT_FLAGS_BATCH_INCOMPLETE)) != 0u) {
        ++say.batch_incomplete;
    }
    if ((f & F(IDAW_ACT_FLAGS_CALL_CONFLICT)) != 0u) {
        ++say.cagri_cakismasi;
        say.calistirma_gecersiz |= kGecCagriCakismasi;
    }
    if (esz) {
        esz_bekleyen.push_back(m);
        return;
    }
    gecen_yanit[m.batch_seq] = m;
    const auto tw = tf_yazim.find(m.batch_seq);
    if (tw != tf_yazim.end()) {
        yanit_sure[m.batch_seq] = duvar - tw->second;
        yanit_sureleri.push_back(duvar - tw->second);
        while (yanit_sure.size() > 64u) {
            yanit_sure.erase(yanit_sure.begin());
        }
    }
    while (gecen_yanit.size() > 64u) {
        gecen_yanit.erase(gecen_yanit.begin());
    }
    // Birleştirilen eski partiler (A1-K40, K75).
    for (std::uint32_t k = 1; k <= m.n_merged; ++k) {
        const std::uint32_t s = m.batch_seq - k;
        const auto it = yuvalar.find(s);
        if (it != yuvalar.end() && it->second.islendi && !it->second.birlesik && !it->second.act_var) {
            ++say.parti_kacirildi_birlesik;
        }
        birlesik_seq.insert(s);
    }
    Yuva& y = yuvalar[m.batch_seq];
    if (y.act_var) {
        ++say.yanit_ret_yinelenen;   // A1-K72
        return;
    }
    if (y.islendi || (sinir_var && son_sinir >= y.adim)) {
        ++say.yanit_ret_gec;
        say.calistirma_gecersiz |= kGecGecYanit;
        return;
    }
    y.act_var = true;
    y.act = m;
    yuva_tamamlandi_mi(m.batch_seq);
}

// ---------------------------------------------------------------- Olay (IDA_EVENT)

void Kopru::Ic::olay_al(const mavlink_ida_event_t& m)
{
    // V2
    const std::int64_t ab = static_cast<std::int64_t>(m.apply_by_ns);
    const std::int64_t hd = static_cast<std::int64_t>(m.hold_ns);
    bool v2 = m.kind >= 1u && m.kind <= 3u && m.call_kind >= 1u && m.call_kind <= 3u && ab >= 16 * kMs &&
              ab <= 1000 * kMs;
    v2 = v2 && (m.tube <= 1u || (m.kind == IDA_EVT_SAFETY_CLOSE && m.tube == 255u));
    if (m.kind == IDA_EVT_SAFETY_OPEN) {
        v2 = v2 && hd >= 60 * kMs && hd <= 450 * kMs;
    } else {
        v2 = v2 && hd == 0;
    }
    if (!v2) {
        ++say.alan_ret;
        return;
    }
    // V3
    const bool kapanis_sifir = m.kind == IDA_EVT_SAFETY_CLOSE && m.batch_seq == 0u && m.t_decide_ns == 0;
    if (!kapanis_sifir && (!tf_gonderildi(m.batch_seq) || tf_zamani(m.batch_seq) != m.t_decide_ns)) {
        ++say.bag_ret;
        return;
    }
    // V4 ve Tablo 4.6b
    const bool acan = m.kind == IDA_EVT_SAFETY_OPEN || m.kind == IDA_EVT_FIRE;
    if (acan && (m.safety_rev == 0u || m.call_kind != IDAW_CALL_KIND_STEP)) {
        ++say.alan_ret;
        return;
    }
    // A1-K81: yineleme.
    if (sakli.count(m.event_id) != 0u) {
        sakli_talep.push_back(m.event_id);
        return;
    }
    if (karari_bekleyen.count(m.event_id) != 0u) {
        return;
    }
    karari_bekleyen.insert(m.event_id);
    ++say.olay_alinan;
    BekleyenOlay o;
    o.ev = m;
    // A1-K83: zamanında gelen olay yuvasıyla aynı sınırda; geç gelen ya da ızgara olayı
    // alımdan sonraki ilk sınırda.
    const auto it = kapanis_sifir ? yuvalar.end() : yuvalar.find(m.batch_seq);
    const bool zamaninda = it != yuvalar.end() && m.call_kind != IDAW_CALL_KIND_TICK_GRID &&
                           !(sinir_var && son_sinir >= it->second.adim);
    if (zamaninda) {
        o.adim = it->second.adim;
        it->second.olaylar.insert(m.event_id);
        yuva_tamamlandi_mi(m.batch_seq);
    } else {
        o.adim = sinir_var ? son_sinir + 1u : 0u;
        ++say.olay_gec;
        say.calistirma_gecersiz |= kGecGecOlay;
    }
    olay_bekleyen.push_back(o);
}

// ---------------------------------------------------------------- Emniyet

void Kopru::Ic::rev_artir()
{
    if (emn.emniyet_revizyonu == kRevAzami) {
        say.emniyet_rev_doydu = true;
        return;
    }
    ++emn.emniyet_revizyonu;
    if (emn.emniyet_revizyonu == kRevAzami) {
        say.emniyet_rev_doydu = true;
    }
    say.emniyet_revizyonu = emn.emniyet_revizyonu;
    // W4-K4: bu revizyonun girdi değeri (artış, işlemin durumu değiştirmesinden sonra yapılır).
    rev_girdi[emn.emniyet_revizyonu] = girdi_kodu();
    while (rev_girdi.size() > 256u) {
        rev_girdi.erase(rev_girdi.begin());
    }
}

void Kopru::Ic::izin_kapat(int tup)
{
    emn.izin_acik[static_cast<std::size_t>(tup)] = false;
    izin_bitis[static_cast<std::size_t>(tup)] = 0;
}

void Kopru::Ic::sonuc_kuyruga(std::uint64_t event_id, std::uint8_t sonuc, std::int64_t t_apply)
{
    karari_bekleyen.erase(event_id);
    sakli[event_id] = SakliSonuc{sonuc, t_apply};
    if (sonuc < say.olay_sonuc.size()) {
        ++say.olay_sonuc[sonuc];
    }
    mavlink_ida_event_result_t r{};
    r.event_id = event_id;
    r.result = sonuc;
    r.t_apply_ns = t_apply;
    sonuc_kuyrugu.push_back(r);
    say.sonuc_kuyrugu_en_buyuk = std::max<std::uint64_t>(say.sonuc_kuyrugu_en_buyuk, sonuc_kuyrugu.size());
}

// Tek bir olayın kararı (A1-K82, K84, K86–K88, K106, K118). t: uygulama sınırının sim zamanı.
void Kopru::Ic::olay_karar(const BekleyenOlay& o, std::int64_t t)
{
    const mavlink_ida_event_t& e = o.ev;
    auto karar = [&](std::uint8_t r, std::int64_t t_apply) { sonuc_kuyruga(e.event_id, r, t_apply); };
    // A1-K82
    if (static_cast<std::uint32_t>(e.event_id >> 32U) != kart.boot_count) {
        karar(static_cast<std::uint8_t>(IDA_EVR_REJ_SESSION), 0);
        return;
    }
    const bool kapanis_sifir = e.kind == IDA_EVT_SAFETY_CLOSE && e.batch_seq == 0u && e.t_decide_ns == 0;
    if (!kapanis_sifir) {
        // A1-K84: sim zamanında ve duvar saatinde son geçerlilik.
        bool suresi_gecti = t > e.t_decide_ns + static_cast<std::int64_t>(e.apply_by_ns);
        const auto it = tf_yazim.find(e.batch_seq);
        if (it != tf_yazim.end() && duvar - it->second > static_cast<std::int64_t>(e.apply_by_ns) + a.t_bekle_ns) {
            suresi_gecti = true;
        }
        if (suresi_gecti) {
            karar(static_cast<std::uint8_t>(IDA_EVR_REJ_EXPIRED), 0);
            return;
        }
    }
    if (e.kind == IDA_EVT_SAFETY_CLOSE) {
        // A1-K87: her durumda uygulanır.
        if (e.tube == 255u) {
            izin_kapat(0);
            izin_kapat(1);
        } else {
            izin_kapat(e.tube);
        }
        rev_artir();
        karar(static_cast<std::uint8_t>(IDA_EVR_APPLIED), t);
        return;
    }
    // İzin açan olaylar: A1-K118 tutarlılığı.
    const auto gy = gecen_yanit.find(e.batch_seq);
    bool tutarli = gy != gecen_yanit.end();
    if (tutarli) {
        const mavlink_ida_actuator_t& m = gy->second;
        const std::uint16_t f = m.act_flags;
        tutarli = m.core_rc == IDA_RC_OK && (f & F(IDAW_ACT_FLAGS_SRC_TICK)) == 0u;
        if (e.kind == IDA_EVT_FIRE) {
            tutarli = tutarli && (f & F(IDAW_ACT_FLAGS_ARMED)) != 0u && (f & F(IDAW_ACT_FLAGS_IS_SAFE)) == 0u;
        }
        if (!tutarli) {
            // Tutarsız yanıt da reddedilir (A1-K118).
            ++say.alan_ret;
            const auto yv = yuvalar.find(e.batch_seq);
            if (yv != yuvalar.end() && !yv->second.islendi) {
                yv->second.reddedildi = true;
            }
        }
    }
    const auto tup = static_cast<std::size_t>(e.tube);
    const bool rev_tamam = !say.emniyet_rev_doydu && e.safety_rev == emn.emniyet_revizyonu;
    if (!rev_tamam && tutarli) {
        ++say.olay_rev_ret;
    }
    const bool girdiler = !emn.acil_durdurma && emn.fis[tup] && !emn.pim[tup];
    if (e.kind == IDA_EVT_SAFETY_OPEN) {
        if (!tutarli || !rev_tamam || !girdiler) {
            karar(static_cast<std::uint8_t>(IDA_EVR_REJ_SAFE), 0);
            return;
        }
        emn.izin_acik[tup] = true;
        izin_bitis[tup] = t + static_cast<std::int64_t>(e.hold_ns);   // A1-K86
        karar(static_cast<std::uint8_t>(IDA_EVR_APPLIED), t);
        return;
    }
    // FIRE (A1-K88).
    if (!tutarli || !rev_tamam || !girdiler || !emn.izin_acik[tup]) {
        karar(static_cast<std::uint8_t>(IDA_EVR_REJ_SAFE), 0);
        return;
    }
    if (!emn.dolu[tup]) {
        karar(static_cast<std::uint8_t>(IDA_EVR_REJ_TUBE_EMPTY), 0);
        return;
    }
    // Uygulanan FIRE yalnız "tüp boşaldı" yetkili durumunu üretir (torpido fiziği bu işte yok).
    emn.dolu[tup] = false;
    karar(static_cast<std::uint8_t>(IDA_EVR_APPLIED), t);
}

void Kopru::emniyet_ata(const EmniyetAtama& a) { ic_->atama_bekleyen.push_back(a); }

sensor::DisDurum Kopru::dis_durum() const
{
    sensor::DisDurum d;
    d.emniyet = ic_->emn;
    return d;
}

bool Kopru::acil_durdurma() const { return ic_->emn.acil_durdurma; }
bool Kopru::tup_dolu(int tup) const { return ic_->emn.dolu[static_cast<std::size_t>(tup & 1)]; }
bool Kopru::izin_acik(int tup) const { return ic_->emn.izin_acik[static_cast<std::size_t>(tup & 1)]; }

// ---------------------------------------------------------------- Fizik sınırı

SinirSonucu Kopru::sinir_isle(std::uint64_t adim_no, std::int64_t duvar_ns)
{
    Ic& i = *ic_;
    i.duvar = duvar_ns;
    const std::int64_t t = static_cast<std::int64_t>(adim_no) * kH;
    SinirSonucu s;
    const std::uint32_t rev_bas = i.emn.emniyet_revizyonu;
    const bool doymus_bas = i.say.emniyet_rev_doydu;

    // (1) Oturum sonu ve süresi dolan izinler.
    if (i.oturum_sonu_bekliyor) {
        i.izin_kapat(0);
        i.izin_kapat(1);
        i.rev_artir();
        i.oturum_sonu_bekliyor = false;
    }
    bool doldu = false;
    for (int tup = 0; tup < 2; ++tup) {
        const auto k = static_cast<std::size_t>(tup);
        if (i.emn.izin_acik[k] && t >= i.izin_bitis[k]) {
            i.izin_kapat(tup);
            doldu = true;
        }
    }
    if (doldu) {
        i.rev_artir();
    }

    // (2) Kısıtlayan komutlar (komut başına) ve senaryonun kısıtlayan atamaları (tek işlem).
    for (; i.dar_komut_bekleyen > 0; --i.dar_komut_bekleyen) {
        i.rev_artir();
    }
    bool kisit = false;
    bool gevset = false;
    for (const EmniyetAtama& at : i.atama_bekleyen) {
        if (at.acil_durdurma && *at.acil_durdurma) {
            i.emn.acil_durdurma = true;
            i.izin_kapat(0);
            i.izin_kapat(1);
            kisit = true;
        }
        for (std::size_t k = 0; k < 2; ++k) {
            if (at.fis[k] && !*at.fis[k]) {
                i.emn.fis[k] = false;
                i.izin_kapat(static_cast<int>(k));
                kisit = true;
            }
            if (at.pim[k] && *at.pim[k]) {
                i.emn.pim[k] = true;
                i.izin_kapat(static_cast<int>(k));
                kisit = true;
            }
        }
        gevset = gevset || (at.acil_durdurma && !*at.acil_durdurma) || (at.fis[0] && *at.fis[0]) ||
                 (at.fis[1] && *at.fis[1]) || (at.pim[0] && !*at.pim[0]) || (at.pim[1] && !*at.pim[1]);
    }
    if (kisit) {
        i.rev_artir();
    }

    // (3) Kart olayları: SAFETY_CLOSE, SAFETY_OPEN, FIRE; her türde event_id sırasıyla.
    std::vector<BekleyenOlay> bu;
    for (auto it = i.olay_bekleyen.begin(); it != i.olay_bekleyen.end();) {
        if (it->adim <= adim_no) {
            bu.push_back(*it);
            it = i.olay_bekleyen.erase(it);
        } else {
            ++it;
        }
    }
    auto sira = [](std::uint8_t k) { return k == IDA_EVT_SAFETY_CLOSE ? 0 : (k == IDA_EVT_SAFETY_OPEN ? 1 : 2); };
    std::sort(bu.begin(), bu.end(), [&](const BekleyenOlay& x, const BekleyenOlay& y) {
        if (sira(x.ev.kind) != sira(y.ev.kind)) {
            return sira(x.ev.kind) < sira(y.ev.kind);
        }
        return x.ev.event_id < y.ev.event_id;
    });
    for (const BekleyenOlay& o : bu) {
        i.olay_karar(o, t);
    }
    if (i.sonuc_kuyrugu.size() > kQSonuc) {
        // A1-K111: E16'nın varsayımı bozuldu: protokol hatası, oturum sonu.
        mavlink_ida_session_close_t c{};
        c.sess = i.sess_;
        c.reason = static_cast<std::uint8_t>(IDAW_CLOSE_REASON_PROTOCOL);
        i.giden.clear();
        i.cerceve(MAVLINK_MSG_ID_IDA_SESSION_CLOSE, &c, sizeof c);
        i.oturum_bitti(false, true);
    }

    // (4) Gevşetmeler: revizyon bu sınırda arttıysa hepsi reddedilir ve yeniden denenmez.
    if (gevset) {
        if (i.emn.emniyet_revizyonu == rev_bas && !doymus_bas && !i.say.emniyet_rev_doydu) {
            for (const EmniyetAtama& at : i.atama_bekleyen) {
                if (at.acil_durdurma && !*at.acil_durdurma) {
                    i.emn.acil_durdurma = false;
                }
                for (std::size_t k = 0; k < 2; ++k) {
                    if (at.fis[k] && *at.fis[k]) {
                        i.emn.fis[k] = true;
                    }
                    if (at.pim[k] && !*at.pim[k]) {
                        i.emn.pim[k] = false;
                    }
                }
            }
            i.rev_artir();
        }
    }
    i.atama_bekleyen.clear();

    // (5) Sürekli çıktılar (A1-K74–K78).
    if (i.yururluk_sil) {
        i.yururluk = false;
        i.yururluk_sil = false;
    }
    // W4-K3 (EMNİYET): acil durdurmanın etkinleştiği sınırda yürürlükteki komut ve bekleyen bütün
    // yuvalar iptal edilir; kalkıştan sonra itki yalnız kalkış anından sonra gönderilen bir
    // partinin (batch_seq ≥ eşik) yanıtıyla gelir. Eşik, acil durdurma sürerken ve kalktığı
    // sınırda sonraki TF'nin batch_seq'idir (o sınırın partisi kalkıştan sonra gönderilir).
    if (i.emn.acil_durdurma) {
        if (!i.estop_onceki) {
            if (i.yururluk) {
                ++i.say.estop_iptal;
            }
            i.yururluk = false;
            for (auto& [sq, y] : i.yuvalar) {
                (void)sq;
                if (!y.islendi && !y.iptal) {
                    y.iptal = true;
                    ++i.say.estop_iptal;
                }
            }
        }
        i.estop_esik_var = true;
        i.estop_esik = i.sonraki_seq;
    } else if (i.estop_onceki) {
        i.estop_esik = i.sonraki_seq;
    }
    i.estop_onceki = i.emn.acil_durdurma;
    struct Aday {
        mavlink_ida_actuator_t m;
        bool esz;
    };
    std::vector<Aday> adaylar;
    const auto ay = i.adim_yuva.find(adim_no);
    if (ay != i.adim_yuva.end()) {
        const std::uint32_t seq = ay->second;
        i.adim_yuva.erase(ay);
        s.yuva_seq = seq;
        const auto yv = i.yuvalar.find(seq);
        if (yv != i.yuvalar.end()) {
            Yuva& y = yv->second;
            y.islendi = true;
            const bool esik_alti = i.estop_esik_var && seq != i.estop_esik && !yeni(seq, i.estop_esik);
            if (y.iptal) {
                // eşzamansız güvenli çıktı ya da acil durdurma iptal etti; kaçırılmış sayılmaz
            } else if (i.emn.acil_durdurma || esik_alti) {
                ++i.say.estop_iptal;   // W4-K3: acil durdurmada ya da öncesinde gönderilen parti
            } else if (i.yuva_tamam(y)) {
                adaylar.push_back({y.act, false});
            } else {
                ++i.say.parti_kacirildi;
                i.say.calistirma_gecersiz |= kGecKacirilanParti;
                s.yuva_kacirildi = true;
                if (y.act_var && !y.reddedildi) {
                    ++i.say.olay_eksik;
                }
                if (i.birlesik_seq.count(seq) != 0u) {
                    ++i.say.parti_kacirildi_birlesik;
                }
            }
        }
        // Uygulanmış eski yuvaları at (gecen_yanit A1-K118 için ayrıca tutulur).
        while (!i.yuvalar.empty() && i.yuvalar.begin()->second.islendi && i.yuvalar.begin()->first != seq) {
            i.yuvalar.erase(i.yuvalar.begin());
        }
        while (!i.birlesik_seq.empty() && yeni(seq, *i.birlesik_seq.begin()) && seq - *i.birlesik_seq.begin() > 64u) {
            i.birlesik_seq.erase(i.birlesik_seq.begin());
        }
    }
    for (const auto& m : i.esz_bekleyen) {
        adaylar.push_back({m, true});
    }
    i.esz_bekleyen.clear();
    std::sort(adaylar.begin(), adaylar.end(),
              [](const Aday& x, const Aday& y) { return yeni(y.m.out_seq, x.m.out_seq); });
    for (const Aday& ad : adaylar) {
        if (i.uygulanan_var && !yeni(ad.m.out_seq, i.son_uygulanan_out_seq)) {
            ++i.say.yanit_ret_eski_uretim;
            continue;
        }
        i.uygulanan_var = true;
        i.son_uygulanan_out_seq = ad.m.out_seq;
        if (ad.esz) {
            ++i.say.esz_guvenli_uygulanan;
            i.say.calistirma_gecersiz |= kGecEszamansiz;
            // Kendinden önce üretilmiş, henüz uygulanmamış olağan yanıtları iptal eder.
            for (auto& [sq, y] : i.yuvalar) {
                (void)sq;
                if (!y.islendi && y.act_var && yeni(ad.m.out_seq, y.act.out_seq)) {
                    y.iptal = true;
                    ++i.say.yanit_iptal_esz;
                }
            }
        } else {
            s.yuva_uygulandi = true;
        }
        if ((ad.m.act_flags & F(IDAW_ACT_FLAGS_THR_VALID)) != 0u) {
            i.yururluk = true;
            for (std::size_t k = 0; k < 8; ++k) {
                i.pwm[k] = static_cast<double>(ad.m.pwm_us[k]);
            }
            i.yururluk_bitis = t + static_cast<std::int64_t>(ad.m.valid_for_ns);
        }
    }
    if (i.yururluk && t >= i.yururluk_bitis) {
        i.yururluk = false;   // A1-K77
        ++i.say.komut_suresi_doldu;
    }
    i.sinir_var = true;
    i.son_sinir = adim_no;
    s.notr = !i.yururluk;
    for (std::size_t k = 0; k < 8; ++k) {
        s.pwm_us[k] = i.yururluk ? i.pwm[k] : static_cast<double>(i.a.esc_notr_us);
    }
    return s;
}

}  // namespace ida::kopru
