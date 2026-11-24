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
    // W4-K15: yanıt yuvaya yalnız kümesi tam ve topluca doğrulanınca girer.
    return y.act_var && !y.reddedildi;
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

Canlilik Kopru::Ic::yanit_al(const mavlink_ida_actuator_t& m)
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
        return Canlilik::yok;
    }
    // V3
    if (esz ? (m.batch_seq != 0u && !tf_gonderildi(m.batch_seq)) : !tf_gonderildi(m.batch_seq)) {
        ++say.bag_ret;
        return Canlilik::yok;
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
        return Canlilik::yok;
    }
    // V5 (A1-K105)
    if (out_seq_var && !yeni(m.out_seq, son_out_seq)) {
        ++say.yanit_ret_eski_uretim;
        return Canlilik::yok;
    }
    if ((f & F(IDAW_ACT_FLAGS_BATCH_INCOMPLETE)) != 0u) {
        ++say.batch_incomplete;
    }
    if ((f & F(IDAW_ACT_FLAGS_CALL_CONFLICT)) != 0u) {
        ++say.cagri_cakismasi;
        say.calistirma_gecersiz |= kGecCagriCakismasi;
    }
    if (esz) {
        // W4-K6: batch_seq = 0 taşıyan ve bu oturumda kartın henüz hiçbir partiyi kapattığı
        // görülmemişken (olağan yanıt alınmamışken) gelen eşzamansız çıktı A1-K79'a sayılmaz.
        // Parti kapandıktan sonra batch_seq = 0 iddia eden çıktı muaf değildir (Okumalar).
        // W4-K28 (3): muafiyet yalnız tutulan tek çıktının yanında saklanır (bellek sabit).
        const bool muaf = m.batch_seq == 0u && !ilk_yanit_alindi;
        out_seq_var = true;
        son_out_seq = m.out_seq;
        // W4-K27 (3): yalnız en yeni eşzamansız güvenli çıktı bekler (ötekini geçersiz kılar: daha
        // eski üretimin iptal ettiklerini de iptal eder); bellek sınırlı, sayaç tutulur.
        if (!esz_bekleyen.empty() && !yeni(m.out_seq, esz_bekleyen.back().out_seq)) {
            ++say.esz_birlesen;
            return Canlilik::aninda;
        }
        if (!esz_bekleyen.empty()) {
            ++say.esz_birlesen;
            esz_bekleyen.clear();
        }
        esz_bekleyen.push_back(m);
        esz_bekleyen_muaf = muaf;
        return Canlilik::aninda;   // ASYNC: beklemeden uygulanır
    }
    // A1-K72: yuva doluysa ya da uygulama anı geçtiyse reddedilir. Partinin ilk alınmış yanıtı
    // (bekleyen, kabul ya da ret) yuvayı tutar: ikincisi yinelenendir (sol #2).
    const auto yv = yuvalar.find(m.batch_seq);
    const PartiKumesi* pk = kume.bul(m.batch_seq);
    if ((yv != yuvalar.end() && yv->second.act_var) || (pk != nullptr && pk->act_var)) {
        ++say.yanit_ret_yinelenen;
        return Canlilik::yok;
    }
    if (yv == yuvalar.end() || yv->second.islendi || (sinir_var && son_sinir >= yv->second.adim)) {
        ++say.yanit_ret_gec;
        say.calistirma_gecersiz |= kGecGecYanit;
        return Canlilik::yok;
    }
    // W4-K15 (2) (EMNİYET): yanıt kümesi topluca doğrulanana dek bekler; kabul edilmeden
    // hiçbir geçmişe girmez; canlılık katkısı kümeye bağlıdır (ek (7)). Hata dönüşü beklemez
    // (4): kümesi hemen doğrulanır ve alım zamanı o anda işlenir.
    PartiKumesi& p = kume_al(m.batch_seq);
    p.act_var = true;
    p.act = m;
    p.act_duvar = duvar;
    const Canlilik katki = kume_canliligi(m.batch_seq);
    (void)kume_tamamla(m.batch_seq);
    return katki;
}

namespace {

bool hata_donusu(const mavlink_ida_actuator_t& a)
{
    return a.core_rc == IDA_RC_BAD_INPUT || a.core_rc == IDA_RC_NOT_INIT;
}

// Kümenin topluca tutarlılığı (A1-K118, Tablo 4.6b): genişletici olay varsa çağrı olağan step
// ve RC OK; FIRE varsa ayrıca ARMED = 1 ve IS_SAFE = 0.
bool kume_tutarli_mi(const PartiKumesi& p)
{
    if (p.acan == 0u) {
        return true;
    }
    const std::uint16_t f = p.act.act_flags;
    bool t = p.act.core_rc == IDA_RC_OK && (f & F(IDAW_ACT_FLAGS_SRC_TICK)) == 0u;
    if ((p.acan & 2u) != 0u) {
        t = t && (f & F(IDAW_ACT_FLAGS_ARMED)) != 0u && (f & F(IDAW_ACT_FLAGS_IS_SAFE)) == 0u;
    }
    return t;
}

}  // namespace

PartiKumesi& Kopru::Ic::kume_al(std::uint32_t seq)
{
    if (kume.bul(seq) == nullptr && kume.sira.size() >= kume.sinir) {
        const PartiKumesi* eski = kume.bul(kume.sira.front());
        if (eski != nullptr && eski->durum == KumeDurumu::bekliyor) {
            ++say.kume_bekleyen_tahliye;
        }
    }
    PartiKumesi& p = kume.al(seq);
    say.kume_en_cok = std::max<std::uint64_t>(say.kume_en_cok, kume.m.size());
    return p;
}

// W4-K15 (3): sayım ihlali. Olay(lar) REJ_SAFE alır, oturum protokol hatasıyla biter (A1-K23:
// oturum sonu iki tüpün iznini sonraki sınırda kapatır).
void Kopru::Ic::sayim_ihlali(const std::vector<std::uint64_t>& idler)
{
    ++say.olay_sayim_ihlali;
    for (std::uint64_t id : idler) {
        if (sakli.count(id) == 0u) {
            sonuc_kuyruga(id, static_cast<std::uint8_t>(IDA_EVR_REJ_SAFE), 0);
        }
    }
    mavlink_ida_session_close_t kapanis{};
    kapanis.sess = sess_;
    kapanis.reason = static_cast<std::uint8_t>(IDAW_CLOSE_REASON_PROTOCOL);
    giden.clear();
    cerceve(MAVLINK_MSG_ID_IDA_SESSION_CLOSE, &kapanis, sizeof kapanis);
    oturum_bitti(false, true);
}

// bekliyor → dogrulandi | reddedildi. Tam: yanıt var ve (hata dönüşü ya da sayılan olay sayısı
// ≥ n_events). Yanıt varken genişletici olay sayısı n_events'i aşarsa sayım ihlalidir.
bool Kopru::Ic::kume_tamamla(std::uint32_t seq)
{
    PartiKumesi* p = kume.bul(seq);
    if (p == nullptr || !p->act_var || p->durum != KumeDurumu::bekliyor) {
        return false;
    }
    const bool hata = hata_donusu(p->act);
    if (!hata && p->acan_id.size() > p->act.n_events) {
        p->durum = KumeDurumu::reddedildi;
        p->canlilik_bekleyen = -1;   // ek (7): reddedilen kümenin alımları atılır
        ++say.kume_reddedilen;
        const std::vector<std::uint64_t> idler = p->acan_id;
        sayim_ihlali(idler);
        return false;
    }
    if (!hata && p->olaylar.size() < p->act.n_events) {
        return false;   // küme eksik: bekler
    }
    if (!kume_tutarli_mi(*p)) {
        p->durum = KumeDurumu::reddedildi;
        p->canlilik_bekleyen = -1;   // ek (7): reddedilen kümenin alımları atılır
        ++say.kume_reddedilen;
        ++say.alan_ret;
        const auto yv = yuvalar.find(seq);
        if (yv != yuvalar.end() && !yv->second.islendi) {
            yv->second.reddedildi = true;
        }
        return false;
    }
    p->durum = KumeDurumu::dogrulandi;
    if (p->canlilik_bekleyen >= 0) {
        canlilik_isle(p->canlilik_bekleyen, true);   // ek (7): kümenin alımları şimdi işlenir
        p->canlilik_bekleyen = -1;
    }
    yanit_kabul(seq, *p);
    return true;
}

// Kümeye bağlı çerçevenin alım zamanı: bekleyen kümede biriktirilir (en son alım); doğrulanmış
// kümede alındığı anda işlenir; reddedilmiş kümede katkı yoktur.
Canlilik Kopru::Ic::kume_canliligi(std::uint32_t seq)
{
    PartiKumesi* p = kume.bul(seq);
    if (p == nullptr) {
        return Canlilik::yok;
    }
    switch (p->durum) {
    case KumeDurumu::bekliyor:
        p->canlilik_bekleyen = std::max(p->canlilik_bekleyen, duvar);
        return Canlilik::kumeye_bagli;
    case KumeDurumu::dogrulandi:
    case KumeDurumu::uygulandi:
        return Canlilik::aninda;
    case KumeDurumu::reddedildi:
        break;
    }
    return Canlilik::yok;
}

// Toplu doğrulamadan geçen yanıt kabul edilmiş geçmişe girer. Uygulama anı geçmişse (küme geç
// tamamlandı) yuvaya ve yanıt geçmişine girmez; yalnız olayların dayanağı olur.
void Kopru::Ic::yanit_kabul(std::uint32_t seq, const PartiKumesi& p)
{
    const mavlink_ida_actuator_t& m = p.act;
    const auto yv = yuvalar.find(seq);
    if (yv == yuvalar.end() || yv->second.islendi || (sinir_var && son_sinir >= yv->second.adim)) {
        return;
    }
    if (!out_seq_var || yeni(m.out_seq, son_out_seq)) {
        out_seq_var = true;
        son_out_seq = m.out_seq;
    }
    gecen_yanit.koy(seq, m);
    if (const TfKaydi* tw = tf_kayit.bul(seq)) {
        yanit_sure.koy(seq, p.act_duvar - tw->duvar);
        yanit_sureleri.push_back(p.act_duvar - tw->duvar);
    }
    // Birleştirilen eski partiler (A1-K40, K75).
    for (std::uint32_t k = 1; k <= m.n_merged; ++k) {
        const std::uint32_t sq = seq - k;
        const auto it = yuvalar.find(sq);
        if (it != yuvalar.end() && it->second.islendi && !it->second.birlesik && !it->second.act_var) {
            ++say.parti_kacirildi_birlesik;
        }
        birlesik_seq.insert(sq);
    }
    Yuva& y = yv->second;
    y.act_var = true;
    ilk_yanit_alindi = true;
    y.act = m;
    yuva_tamamlandi_mi(seq);
}

// ---------------------------------------------------------------- Olay (IDA_EVENT)

Canlilik Kopru::Ic::olay_al(const mavlink_ida_event_t& m)
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
        return Canlilik::yok;
    }
    // V3
    const bool kapanis_sifir = m.kind == IDA_EVT_SAFETY_CLOSE && m.batch_seq == 0u && m.t_decide_ns == 0;
    if (!kapanis_sifir && (!tf_gonderildi(m.batch_seq) || tf_zamani(m.batch_seq) != m.t_decide_ns)) {
        ++say.bag_ret;
        return Canlilik::yok;
    }
    // V4 ve Tablo 4.6b
    const bool acan = m.kind == IDA_EVT_SAFETY_OPEN || m.kind == IDA_EVT_FIRE;
    if (acan && (m.safety_rev == 0u || m.call_kind != IDAW_CALL_KIND_STEP)) {
        ++say.alan_ret;
        return Canlilik::yok;
    }
    // A1-K81: yineleme. Canlılık (W4-K15 ek (7)): uygulanmış olayın yinelemesi yeniler;
    // reddedilmiş olayınki yenilemez. Kararı bekleyen olayın yinelemesi: SAFETY_CLOSE beklemeden
    // uygulanır (yeniler); genişletici olay kümesine bağlıdır.
    const auto sk = sakli.find(m.event_id);
    if (sk != sakli.end()) {
        sakli_talep.push_back(m.event_id);
        return sk->second.sonuc == IDA_EVR_APPLIED ? Canlilik::aninda : Canlilik::yok;
    }
    if (karari_bekleyen.count(m.event_id) != 0u) {
        if (!acan || kapanis_sifir || m.call_kind == IDAW_CALL_KIND_TICK_GRID) {
            return Canlilik::aninda;
        }
        return kume_canliligi(m.batch_seq);
    }
    // W4-K15: partinin olay kümesi (geliş zamanından bağımsız; ızgara olayları ve batch_seq = 0
    // kapanışları partiye sayılmaz, A1-K73). Karar verilmiş kümeye yeni kimlikli genişletici
    // olay sayım ihlalidir (3); SAFETY_CLOSE kümeye eklenir ama hiçbir zaman ihlal değildir.
    PartiKumesi* pk = nullptr;
    if (!kapanis_sifir && m.call_kind != IDAW_CALL_KIND_TICK_GRID) {
        pk = &kume_al(m.batch_seq);
        if (acan && pk->durum != KumeDurumu::bekliyor) {
            sayim_ihlali({m.event_id});
            return Canlilik::yok;
        }
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
    } else {
        o.adim = sinir_var ? son_sinir + 1u : 0u;
        o.gec = true;
        ++say.olay_gec;
        say.calistirma_gecersiz |= kGecGecOlay;
    }
    olay_bekleyen.push_back(o);
    // W4-K15 ek (6): ACTUATOR(n)'den sonra alınan yeni kimlikli SAFETY_CLOSE(n) uygulanır ama
    // kümenin sayımına girmez (sonraki hata dönüşünün olayı olabilir; A1-K73). Küme yalnız
    // ACTUATOR'dan önce alınan olaylarla ya da sayılmış genişletici olayların yinelemeleriyle
    // tamamlanır.
    if (pk != nullptr && (acan || !pk->act_var)) {
        pk->olaylar.insert(m.event_id);
        if (acan) {
            pk->acan_id.push_back(m.event_id);
            pk->acan = static_cast<std::uint8_t>(pk->acan | (m.kind == IDA_EVT_FIRE ? 2u : 1u));
        }
    }
    // Canlılık (W4-K15 ek (7)): SAFETY_CLOSE beklemeden uygulanır, alındığı anda yeniler;
    // genişletici olayın alım zamanı kümesine bağlıdır (doğrulanınca işlenir, retle atılır).
    const Canlilik katki = acan && pk != nullptr ? kume_canliligi(m.batch_seq) : Canlilik::aninda;
    if (pk != nullptr) {
        (void)kume_tamamla(m.batch_seq);   // bekliyorsa: tamamlanır, reddedilir ya da ihlal
    }
    if (durum != OturumDurumu::kosuyor && durum != OturumDurumu::hazir) {
        return Canlilik::yok;   // sayım ihlali oturumu bitirdi
    }
    return katki;
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
    rev_girdi[emn.emniyet_revizyonu] = RevDurum{girdi_kodu(), izin_kodu()};
    while (rev_girdi.size() > 256u) {
        rev_girdi.erase(rev_girdi.begin());
    }
}

void Kopru::Ic::izin_kapat(int tup)
{
    emn.izin_acik[static_cast<std::size_t>(tup)] = false;
    izin_bitis[static_cast<std::size_t>(tup)] = 0;
}

void Kopru::Ic::sonuc_kuyruga(std::uint64_t event_id, std::uint8_t sonuc, std::int64_t t_apply, std::uint8_t kind,
                               std::uint8_t tube)
{
    // W4-bagla-a3 (A3-K98): kararın A3'e aktarılacak kopyası (yalnız gözlem; davranışı değiştirmez).
    // Sabit boylu halka (en çok 1024): dolarsa en eski karar atılır, sayaç artar.
    if (olay_karari_n == olay_kararlari.size()) {
        olay_karari_bas = (olay_karari_bas + 1u) % olay_kararlari.size();
        --olay_karari_n;
        ++olay_karari_atilan;
    }
    olay_kararlari[(olay_karari_bas + olay_karari_n) % olay_kararlari.size()] = OlayKarari{event_id, kind, tube, sonuc, t_apply};
    ++olay_karari_n;
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
void Kopru::Ic::olay_karar(const BekleyenOlay& o, std::int64_t t, std::uint64_t adim_no)
{
    const mavlink_ida_event_t& e = o.ev;
    auto karar = [&](std::uint8_t r, std::int64_t t_apply) { sonuc_kuyruga(e.event_id, r, t_apply, e.kind, e.tube); };
    // A1-K82
    if (static_cast<std::uint32_t>(e.event_id >> 32U) != kart.boot_count) {
        karar(static_cast<std::uint8_t>(IDA_EVR_REJ_SESSION), 0);
        return;
    }
    const bool kapanis_sifir = e.kind == IDA_EVT_SAFETY_CLOSE && e.batch_seq == 0u && e.t_decide_ns == 0;
    if (!kapanis_sifir) {
        // A1-K84: sim zamanında ve duvar saatinde son geçerlilik.
        bool suresi_gecti = t > e.t_decide_ns + static_cast<std::int64_t>(e.apply_by_ns);
        const TfKaydi* tk = tf_kayit.bul(e.batch_seq);
        if (tk != nullptr && duvar - tk->duvar > static_cast<std::int64_t>(e.apply_by_ns) + a.t_bekle_ns) {
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
    // İzin açan olaylar (W4-K15 (1), EMNİYET): genişletici etki yalnız partinin kümesi tam ve
    // topluca doğrulanmışsa uygulanır. Küme bekliyorsa olay bekler (son geçerlilik dolunca
    // yukarıda REJ_EXPIRED); küme reddedildiyse, tablodan atıldıysa ya da yanıt artık
    // gelemiyorsa (yuvanın uygulama anı geldi ya da geçti) REJ_SAFE.
    const PartiKumesi* pk = kume.bul(e.batch_seq);
    bool bekle = false;
    bool tutarli = false;
    if (pk != nullptr && pk->act_var) {
        bekle = pk->durum == KumeDurumu::bekliyor;
        tutarli = pk->durum == KumeDurumu::dogrulandi || pk->durum == KumeDurumu::uygulandi;
    } else if (pk != nullptr) {
        const auto yv = yuvalar.find(e.batch_seq);
        bekle = yv != yuvalar.end() && !yv->second.islendi && yv->second.adim > adim_no;
    }
    if (bekle) {
        BekleyenOlay b = o;
        b.adim = adim_no + 1u;
        if (!b.gec) {
            b.gec = true;   // kendi sınırından sonra uygulanırsa geç olaydır (A1-K83)
            ++say.olay_gec;
            say.calistirma_gecersiz |= kGecGecOlay;
        }
        olay_bekleyen.push_back(b);
        return;
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
        rev_girdi[emn.emniyet_revizyonu].izin_acilan =
            static_cast<std::uint8_t>(rev_girdi[emn.emniyet_revizyonu].izin_acilan | izin_kodu());   // W4-K12
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

// ---- Emniyet adımlarının ortak kodu: sinir_isle ve emniyet_artimli (W4-bagla-a3; W4-K27 (1)) ----

static bool gevseten_mi(const EmniyetAtama& at)
{
    return (at.acil_durdurma && !*at.acil_durdurma) || (at.fis[0] && *at.fis[0]) || (at.fis[1] && *at.fis[1]) ||
           (at.pim[0] && !*at.pim[0]) || (at.pim[1] && !*at.pim[1]);
}

// Atamanın kısıtlayan alanlarını uygular; dönüş: kısıtlayan alan vardı. `degisti`: yetkili durum değişti.
bool Kopru::Ic::kisitla(const EmniyetAtama& at, bool& degisti)
{
    bool var = false;
    if (at.acil_durdurma && *at.acil_durdurma) {
        degisti = degisti || !emn.acil_durdurma;
        emn.acil_durdurma = true;
        izin_kapat(0);
        izin_kapat(1);
        var = true;
    }
    for (std::size_t k = 0; k < 2; ++k) {
        if (at.fis[k] && !*at.fis[k]) {
            degisti = degisti || emn.fis[k];
            emn.fis[k] = false;
            izin_kapat(static_cast<int>(k));
            var = true;
        }
        if (at.pim[k] && *at.pim[k]) {
            degisti = degisti || !emn.pim[k];
            emn.pim[k] = true;
            izin_kapat(static_cast<int>(k));
            var = true;
        }
    }
    return var;
}

// Operatörün kısıtlayan isteği: istek başına bir revizyon; durum değişmese de artar (A3-K92,
// K117 (1)). EMNİYET: hiçbir koşulda reddedilmez. Dönüş: istek kısıtlayandı (sonuç `o`'da).
bool Kopru::Ic::operator_kisitla(const EmniyetAtama& at, OperatorSonucu& o)
{
    if (gevseten_mi(at)) {
        return false;
    }
    bool degisti = false;
    if (!kisitla(at, degisti)) {
        return false;
    }
    rev_artir();
    o = OperatorSonucu{at.operator_kimligi, AtamaSonucu::uygulandi, true, degisti, emn.emniyet_revizyonu};
    return true;
}

// Operatörün gevşeten isteği (A3-K95 (4), K117 (6), K118). `kisitlama_hakim`: bu sınırda revizyon
// 1–3. adımlarda arttı (artımlı işlemde yok: W4-K27 (1)); `rev_bas`: o durumda kodu ayırmak için.
OperatorSonucu Kopru::Ic::operator_gevset(const EmniyetAtama& at, bool kisitlama_hakim, std::uint32_t rev_bas)
{
    OperatorSonucu o{at.operator_kimligi, AtamaSonucu::uygulandi, false, false, 0};
    if (kisitlama_hakim) {
        o.sonuc = say.emniyet_rev_doydu && emn.emniyet_revizyonu == rev_bas ? AtamaSonucu::rev_doydu
                                                                              : AtamaSonucu::kisitlama_hakim;
    } else if (say.emniyet_rev_doydu) {
        o.sonuc = AtamaSonucu::rev_doydu;
    } else if (!at.beklenen_rev || *at.beklenen_rev != emn.emniyet_revizyonu) {
        o.sonuc = AtamaSonucu::rev_uyusmuyor;
    } else {
        if (at.acil_durdurma && !*at.acil_durdurma) {
            o.degisti = o.degisti || emn.acil_durdurma;
            emn.acil_durdurma = false;
        }
        for (std::size_t k = 0; k < 2; ++k) {
            if (at.fis[k] && *at.fis[k]) {
                o.degisti = o.degisti || !emn.fis[k];
                emn.fis[k] = true;
            }
            if (at.pim[k] && !*at.pim[k]) {
                o.degisti = o.degisti || emn.pim[k];
                emn.pim[k] = false;
            }
        }
        rev_artir();   // A3-K117 (6): uygulanan her gevşetme
    }
    o.rev_sonra = emn.emniyet_revizyonu;
    return o;
}

// W4-K3 (EMNİYET): acil durdurmanın etkinleştiği anda yürürlükteki komut ve bekleyen bütün
// yuvalar iptal edilir; kalkıştan sonra itki yalnız kalkış anından sonra gönderilen bir partinin
// (batch_seq ≥ eşik) yanıtıyla gelir. Eşik, acil durdurma sürerken ve kalktığı anda sonraki TF'nin
// batch_seq'idir (o andan sonraki parti kalkıştan sonra gönderilir).
void Kopru::Ic::estop_isle()
{
    if (emn.acil_durdurma) {
        if (!estop_onceki) {
            if (yururluk) {
                ++say.estop_iptal;
            }
            yururluk = false;
            for (auto& [sq, y] : yuvalar) {
                (void)sq;
                if (!y.islendi && !y.iptal) {
                    y.iptal = true;
                    ++say.estop_iptal;
                }
            }
        }
        estop_esik_var = true;
        estop_esik = sonraki_seq;
    } else if (estop_onceki) {
        estop_esik = sonraki_seq;
    }
    estop_onceki = emn.acil_durdurma;
}

std::size_t Kopru::emniyet_artimli(const EmniyetAtama* a, std::size_t n, OperatorSonucu* sonuc)
{
    Ic& i = *ic_;
    std::size_t k = 0;
    // A1-K85 sırası: önce kısıtlayanlar (hepsi), sonra gevşetenler (her biri taşıdığı revizyona göre).
    for (std::size_t j = 0; j < n; ++j) {
        OperatorSonucu o;
        if (a[j].operator_kimligi != 0u && i.operator_kisitla(a[j], o)) {
            sonuc[k++] = o;
        }
    }
    for (std::size_t j = 0; j < n; ++j) {
        if (a[j].operator_kimligi != 0u && gevseten_mi(a[j])) {
            sonuc[k++] = i.operator_gevset(a[j], false, i.emn.emniyet_revizyonu);
        }
    }
    i.estop_isle();
    return k;
}

void Kopru::emniyet_ata(const EmniyetAtama& a) { ic_->atama_bekleyen.push_back(a); }

bool Kopru::emniyet_baslangic(bool acil_durdurma, const std::array<bool, 2>& fis, const std::array<bool, 2>& pim,
                              const std::array<bool, 2>& dolu)
{
    Ic& i = *ic_;
    if (i.sinir_var || i.emn.emniyet_revizyonu != 1u || !i.atama_bekleyen.empty()) {
        return false;
    }
    i.emn.acil_durdurma = acil_durdurma;
    i.emn.fis = fis;
    i.emn.pim = pim;
    i.emn.dolu = dolu;
    i.rev_girdi.clear();
    i.rev_girdi[i.emn.emniyet_revizyonu] = RevDurum{i.girdi_kodu(), i.izin_kodu()};   // W4-K12
    return true;
}

sensor::DisDurum Kopru::dis_durum() const
{
    sensor::DisDurum d;
    d.emniyet = ic_->emn;
    return d;
}

bool Kopru::acil_durdurma() const { return ic_->emn.acil_durdurma; }
bool Kopru::tup_dolu(int tup) const { return ic_->emn.dolu[static_cast<std::size_t>(tup & 1)]; }
bool Kopru::izin_acik(int tup) const { return ic_->emn.izin_acik[static_cast<std::size_t>(tup & 1)]; }
bool Kopru::itki_yururlukte() const { return ic_->yururluk; }   // silinmesi bekleniyor olsa da

std::int64_t Kopru::izin_bitis_ns(int tup) const
{
    const auto k = static_cast<std::size_t>(tup & 1);
    return ic_->emn.izin_acik[k] ? ic_->izin_bitis[k] : -1;
}

std::uint32_t Kopru::emniyet_revizyonu() const { return ic_->emn.emniyet_revizyonu; }
bool Kopru::emniyet_rev_doydu() const { return ic_->say.emniyet_rev_doydu; }

void Kopru::olay_kararlari_al(std::vector<OlayKarari>& v)
{
    Ic& i = *ic_;
    for (; i.olay_karari_n > 0u; --i.olay_karari_n) {
        v.push_back(i.olay_kararlari[i.olay_karari_bas]);
        i.olay_karari_bas = (i.olay_karari_bas + 1u) % i.olay_kararlari.size();
    }
}

std::size_t Kopru::esz_bekleyen_sayisi() const { return ic_->esz_bekleyen.size(); }

std::uint64_t Kopru::olay_karari_atilan() const { return ic_->olay_karari_atilan; }

void Kopru::hil_duraklatma_bildir() { ic_->say.calistirma_gecersiz |= kGecDuraklatma; }

// ---------------------------------------------------------------- Fizik sınırı

// Yanıtın ya da eşzamansız güvenli çıktının uygulanması (A1-K74–K78): üretim sırası, A1-K78
// iptali, yürürlük. sinir_isle'nin (5). adımı ve SIL'in kısıtlayan erken uygulaması ortak yolu.
// `muaf` (W4-K6, W4-K28 (3)): eşzamansız çıktı ilk parti öncesinde alındı (A1-K79'a sayılmaz).
void Kopru::Ic::cikti_uygula(const mavlink_ida_actuator_t& m, bool esz, bool muaf, std::uint64_t adim_no, std::int64_t t,
                             SinirSonucu& s)
{
    if (uygulanan_var && !yeni(m.out_seq, son_uygulanan_out_seq)) {
        ++say.yanit_ret_eski_uretim;
        return;
    }
    const std::uint32_t onceki_out_seq = uygulanan_var ? son_uygulanan_out_seq : 0u;
    uygulanan_var = true;
    uygulanan_boot = kart.boot_count;
    son_uygulanan_out_seq = m.out_seq;
    if (esz) {
        ++say.esz_guvenli_uygulanan;
        if (muaf) {
            ++say.eszamansiz_ilk_parti_oncesi;   // W4-K6: A1-K79 sayımına girmez
        } else {
            say.calistirma_gecersiz |= kGecEszamansiz;
            if (say.eszamansiz_kayit_n < say.eszamansiz_kayit.size()) {
                Sayaclar::EszKaydi& e = say.eszamansiz_kayit[say.eszamansiz_kayit_n++];
                e.adim = adim_no;
                e.out_seq = m.out_seq;
                e.act_flags = m.act_flags;
                e.onceki_out_seq = onceki_out_seq;
                e.ilk_yanit_alindi = ilk_yanit_alindi;
            }
        }
        // Kendinden önce üretilmiş, henüz uygulanmamış olağan yanıtları iptal eder.
        for (auto& [sq, y] : yuvalar) {
            (void)sq;
            if (!y.islendi && y.act_var && yeni(m.out_seq, y.act.out_seq)) {
                y.iptal = true;
                ++say.yanit_iptal_esz;
            }
        }
    } else {
        s.yuva_uygulandi = true;
        if (PartiKumesi* pk = kume.bul(m.batch_seq)) {
            if (pk->durum == KumeDurumu::dogrulandi) {
                pk->durum = KumeDurumu::uygulandi;
            }
        }
    }
    if ((m.act_flags & F(IDAW_ACT_FLAGS_THR_VALID)) != 0u) {
        yururluk = true;
        for (std::size_t k = 0; k < 8; ++k) {
            pwm[k] = static_cast<double>(m.pwm_us[k]);
        }
        yururluk_bitis = t + static_cast<std::int64_t>(m.valid_for_ns);
    }
}

// Oturum sonunun yerel daraltması (A1-K23): iki tüpün izni kapanır (revizyon artar), yürürlükteki
// itki komutu silinir. sil_kisitlayan_uygula ve kapanis_daralt'ın ortak yolu. Dönüş: izinler
// kapandı (oturum sonu bekliyordu).
bool Kopru::Ic::oturum_sonu_daralt()
{
    bool degisti = false;
    if (oturum_sonu_bekliyor) {
        izin_kapat(0);
        izin_kapat(1);
        rev_artir();
        oturum_sonu_bekliyor = false;
        degisti = true;
    }
    if (yururluk_sil) {
        yururluk = false;
        yururluk_sil = false;
    }
    return degisti;
}

// Sol 4. denetim #3 (EMNİYET): kapanışta yerel daraltma; SIL ve HIL. Oturum sonunun bekleyen
// daraltması hemen uygulanır, izinler kapatılır ve yürürlükteki itki nötrlenir. Yeni kart çıktısı
// tüketmez, fizik ilerletmez, sınır işlemez.
KisitSonucu Kopru::kapanis_daralt()
{
    Ic& i = *ic_;
    KisitSonucu k;
    k.degisti = i.oturum_sonu_daralt();
    bool acik = false;
    for (int tup = 0; tup < 2; ++tup) {
        acik = acik || i.emn.izin_acik[static_cast<std::size_t>(tup)];
    }
    if (acik) {
        i.izin_kapat(0);
        i.izin_kapat(1);
        i.rev_artir();
        k.degisti = true;
    }
    if (i.yururluk) {
        i.yururluk = false;
        k.degisti = true;
    }
    k.notr = true;
    k.pwm_us.fill(static_cast<double>(i.a.esc_notr_us));
    return k;
}

// W4-K26 (1) (EMNİYET, yalnız SIL): kilitli adımda ACTUATOR(n)'den sonraki çerçevelerin alım
// anı, işlendikleri bekleme noktasıdır (yuva n + 1'in sınırı). Orada işlenen kısıtlayan
// çerçevelerin etkisi fizik o sınırdan ilerlemeden yürürlüğe girer: oturum sonu (A1-K23: iki
// tüpün izni kapanır, iticiler nötr) ve eşzamansız güvenli çıktı (A1-K78: kendinden önce
// üretilmiş, uygulanmamış yanıtları iptal eder; nötr). Gevşeten hiçbir etki burada uygulanmaz.
KisitSonucu Kopru::sil_kisitlayan_uygula(std::uint64_t adim_no)
{
    Ic& i = *ic_;
    KisitSonucu k;
    if (i.a.kip != CalistirmaKipi::sil) {
        return k;
    }
    const std::int64_t t = static_cast<std::int64_t>(adim_no) * kH;
    const bool yururluk_bas = i.yururluk;
    const std::array<double, 8> pwm_bas = i.pwm;
    k.degisti = i.oturum_sonu_daralt();
    if (!i.esz_bekleyen.empty()) {
        std::vector<mavlink_ida_actuator_t> e;
        e.swap(i.esz_bekleyen);
        const bool muaf = i.esz_bekleyen_muaf;   // tutulan tek çıktının muafiyeti (W4-K28 (3))
        i.esz_bekleyen_muaf = false;
        std::sort(e.begin(), e.end(), [](const mavlink_ida_actuator_t& x, const mavlink_ida_actuator_t& y) {
            return yeni(y.out_seq, x.out_seq);
        });
        SinirSonucu bos;
        for (const auto& m : e) {
            i.cikti_uygula(m, true, muaf, adim_no, t, bos);
        }
        ++i.say.sil_kisit_erken;
    }
    k.notr = !i.yururluk;
    for (std::size_t c = 0; c < 8; ++c) {
        k.pwm_us[c] = i.yururluk ? i.pwm[c] : static_cast<double>(i.a.esc_notr_us);
    }
    k.degisti = k.degisti || i.yururluk != yururluk_bas || (i.yururluk && i.pwm != pwm_bas);
    return k;
}

SinirSonucu Kopru::sinir_isle(std::uint64_t adim_no, std::int64_t duvar_ns)
{
    Ic& i = *ic_;
    i.duvar = duvar_ns;
    const std::int64_t t = static_cast<std::int64_t>(adim_no) * kH;
    SinirSonucu s;
    i.dar_hizmet();   // bekleyen daraltıcılar kova izin verince (A1-K61, K119)
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
    // Senaryonun kısıtlayan atamaları: tek işlem (A3-K117 (2)).
    for (const EmniyetAtama& at : i.atama_bekleyen) {
        if (at.operator_kimligi != 0u) {
            continue;
        }
        bool degisti = false;
        kisit = i.kisitla(at, degisti) || kisit;
        gevset = gevset || gevseten_mi(at);
    }
    if (kisit) {
        i.rev_artir();
    }
    // Operatörün kısıtlayan istekleri (W4-bagla-a3): istek başına bir işlem (A3-K92, K117 (1));
    // artımlı işlemle (`emniyet_artimli`) aynı kod.
    for (const EmniyetAtama& at : i.atama_bekleyen) {
        OperatorSonucu o;
        if (at.operator_kimligi != 0u && i.operator_kisitla(at, o)) {
            s.operator_sonuclari.push_back(o);
        }
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
        i.olay_karar(o, t, adim_no);
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
    s.rev_adim_basi = rev_bas;
    s.rev_adim3_sonu = i.emn.emniyet_revizyonu;
    const bool gevsetme_serbest = i.emn.emniyet_revizyonu == rev_bas && !doymus_bas && !i.say.emniyet_rev_doydu;
    if (gevset) {
        if (!gevsetme_serbest) {
            s.gevsetme_reddedildi = true;   // A5-K63: kaydı senaryo yürütücüsü yazar
            ++i.say.gevsetme_reddedildi;
        }
        if (gevsetme_serbest) {
            for (const EmniyetAtama& at : i.atama_bekleyen) {
                if (at.operator_kimligi != 0u) {
                    continue;
                }
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
    // Operatörün gevşeten istekleri (W4-bagla-a3; A1-K85 / A3-K95 (4)): senaryonun gevşetmelerinden
    // sonra, istek başına; her biri taşıdığı revizyona göre (A3-K118). EMNİYET: revizyon bu
    // sınırda 1–3. adımlarda arttıysa hepsi reddedilir ve yeniden denenmez.
    for (const EmniyetAtama& at : i.atama_bekleyen) {
        if (at.operator_kimligi != 0u && gevseten_mi(at)) {
            s.operator_sonuclari.push_back(i.operator_gevset(at, !gevsetme_serbest, rev_bas));
        }
    }
    i.atama_bekleyen.clear();

    // (5) Sürekli çıktılar (A1-K74–K78).
    if (i.yururluk_sil) {
        i.yururluk = false;
        i.yururluk_sil = false;
    }
    i.estop_isle();
    struct Aday {
        mavlink_ida_actuator_t m;
        bool esz;
        bool muaf = false;   // W4-K6: ilk parti öncesi eşzamansız çıktı
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
        // İşlenmiş yuvalar gönderim sırasıyla atılır (sarmada sayısal sıra kullanılmaz; sol #12);
        // bu sınırın yuvası olay doğrulaması için bir sınır daha kalır.
        while (!i.yuva_sira.empty() && i.yuva_sira.front() != seq) {
            const auto ey = i.yuvalar.find(i.yuva_sira.front());
            if (ey != i.yuvalar.end() && !ey->second.islendi) {
                break;
            }
            if (ey != i.yuvalar.end()) {
                i.yuvalar.erase(ey);
            }
            i.yuva_sira.pop_front();
        }
        while (!i.birlesik_seq.empty() && yeni(seq, *i.birlesik_seq.begin()) && seq - *i.birlesik_seq.begin() > 64u) {
            i.birlesik_seq.erase(i.birlesik_seq.begin());
        }
    }
    for (const auto& m : i.esz_bekleyen) {
        adaylar.push_back({m, true, i.esz_bekleyen_muaf});
    }
    i.esz_bekleyen.clear();
    i.esz_bekleyen_muaf = false;
    std::sort(adaylar.begin(), adaylar.end(),
              [](const Aday& x, const Aday& y) { return yeni(y.m.out_seq, x.m.out_seq); });
    for (const Aday& ad : adaylar) {
        i.cikti_uygula(ad.m, ad.esz, ad.muaf, adim_no, t, s);
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
