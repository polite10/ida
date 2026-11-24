/*
 * atis_emniyet.c — atış kontrolün emniyet girdileri ve olay sonuçları (W5-atis-emniyet):
 * (b) adımındaki daraltmaya tepki ve bütünlük onarımı, emniyet örneğinin bayatlığı (W5-K4 ve
 * eki), etkin değerin gevşetilmesi (A4-K116; core/TASARIM.md §1'deki "emniyet gevşetmesi"
 * dosyası emniyet/ yerine buradadır: dosya sahipliği atis/), geri okuma çelişkisi ve tekleme
 * (W5-K4; G-Ç53), olay sonuçlarına göre geçişler (W5-K2, K11).
 */
#include <string.h>

#include "atis/atis.h"
#include "atis/atis_ic.h"
#include "zaman/zaman.h"

#define TUP_TANIMLI (IDA_TUBE_LOADED | IDA_TUBE_PLUG_IN | IDA_TUBE_PIN_OUT | IDA_TUBE_PERMIT_OPEN)

int32_t ida_fc_gunluk_tup_bitleri(uint8_t kisitli)
{
    int32_t b = 0;
    if ((kisitli & IDA_EMN_FIS_T0) == 0u) {
        b |= 0x1; /* tüp 0 fiş takılı */
    }
    if ((kisitli & IDA_EMN_PIM_T0) == 0u) {
        b |= 0x2; /* tüp 0 pim çıkık */
    }
    if ((kisitli & IDA_EMN_FIS_T1) == 0u) {
        b |= 0x4;
    }
    if ((kisitli & IDA_EMN_PIM_T1) == 0u) {
        b |= 0x8;
    }
    return b;
}

static uint8_t kapat_biti(uint8_t tup)
{
    if (tup == 0u) {
        return (uint8_t)IDA_KAPAT_T0;
    }
    return (tup == 1u) ? (uint8_t)IDA_KAPAT_T1 : (uint8_t)IDA_KAPAT_ALL;
}

/* ---- Geri okuma izleri (G-Ç53; 03 §2.6 FIRING → ASSESS) ----------------------------------- */

/* Kapatma ve atış izlerinin bir inst = 0 geri okumasıyla değerlendirilmesi: örnek geldiğinde
   ve izi kuran sonuç örnekten sonra geldiğinde (son kabul edilen örnekle). */
static void izleri_degerlendir(ida_fc_t *fc, const uint8_t bitler[IDA_PORT_N_TUBE],
                               int64_t t_meas, int64_t now)
{
    for (uint8_t i = 0u; i < IDA_PORT_N_TUBE; i++) {
        ida_fc_tup_t *t = &fc->t[i];
        const uint8_t bit = bitler[i];
        /* G-Ç53 (1): son SAFETY_CLOSE APPLIED olduktan sonra, araya yeni SAFETY_OPEN girmeden,
           t_meas'i t_apply'dan büyük örnekte PERMIT_OPEN = 1 → FAULT_LATCHED (süren neden). */
        if (t->kapat_izleme && (t_meas > t->kapat_t_apply_ns)) {
            t->celiski = (bit & IDA_TUBE_PERMIT_OPEN) != 0u;
            if (t->celiski) {
                ida_fc_kilitle(fc, kapat_biti(i), now);
            }
        }
        /* G-Ç53 (2) tekleme ve 03 §2.6 FIRING → ASSESS: FIRE APPLIED olduktan sonra t_meas'i
           t_apply'dan büyük örnekte LOADED. */
        if (t->atis_izleme && (t_meas > t->atis_t_apply_ns)) {
            t->atis_izleme = false;
            if ((bit & IDA_TUBE_LOADED) != 0u) {
                ida_fc_kilitle(fc, kapat_biti(i), now);
            } else if ((fc->ic == IDA_FC_IC_FIRING) && (fc->tup == i)) {
                ida_fc_ic_yaz(fc, IDA_FC_IC_ASSESS, now);
            } else {
                /* fırlatma görüldü; FC başka nedenle güvenli duruma dönmüştü */
            }
        }
    }
}

static void son_ornekle_degerlendir(ida_fc_t *fc, int64_t now)
{
    if (fc->emn_tmeas_var) {
        izleri_degerlendir(fc, fc->son_tup, fc->emn_tmeas_ns, now);
    }
}

/* ---- Bayatlık (G-Ç29; W5-K4 ve eki) ----------------------------------------------------- */

bool ida_fc_emn_bayat(const ida_fc_t *fc, const ida_emn_t *e, const ida_fc_ortam_t *o)
{
    if (!e->inst0_var) {
        return false; /* ilk kabulden önce alanlar zaten kısıtlayandır (A4-K128) */
    }
    const int64_t sinir = ida_ms_ns(ida_prm_u32(o->prm, IDA_PRM_fc_emn_bayat_ms));
    const int64_t now = o->now.ns;
    /* (i) son kabul edilen inst = 0 örneğinin t_rx'inden bu yana duvar süresi; (ii) t_meas'in
       son ilerlediği örneğin t_rx'inden bu yana duvar süresi (A4-K34'ün eşit t_meas'lı kabulleri
       (i)'yi yeniler, (ii)'yi yenilemez). */
    return ((now - fc->emn_rx_ns) > sinir) || ((now - fc->emn_ilerleme_ns) > sinir);
}

/* ---- (b): bütünlük, bayatlık, daraltmaya tepki ----------------------------------------- */

/* A4-K89, G-Ç26: iç durum bozuksa FAULT_LATCHED (03 §2.6 "geçersiz desen"); yalnız dış durum
   yansıması bozuksa iç durumdan yeniden kurulur. Bozulma çerçevece (b)'den önce saptanır
   ve core_ok = 0 olur (cerceve.c ustune_yazilacak_bozuk). */
static void butunluk_onar(ida_fc_t *fc, const ida_fc_ortam_t *o)
{
    if (ida_fc_tutarli(fc)) {
        return;
    }
    if (!ida_fc_ic_tutarli(fc)) {
        /* İç durum ya da bayraklar bozuk: izler güvenilmez, kısıtlayan değerlerle yeniden. */
        const uint8_t onceki_kip = fc->onceki_kip;
        (void)memset(fc->t, 0, sizeof(fc->t));
        fc->istek_var = false;
        fc->kip_izni_dusuk = true;
        fc->emn_tmeas_var = false;
        fc->emn_gevsedi = false;
        fc->reset_kabul = false;
        fc->kapat = 0u;
        fc->onceki_kip = (onceki_kip <= (uint8_t)IDA_MODE_AUTO) ? onceki_kip : (uint8_t)IDA_MODE_INIT;
        ida_fc_tup_yaz(fc, IDA_FC_TUP_YOK);
        ida_fc_ic_yaz(fc, IDA_FC_IC_FAULT, o->now.ns);
        fc->butunluk_kilidi = true;
        fc->bu_cagri_kilit = true;
        ida_fc_kapat(fc, (uint8_t)IDA_KAPAT_ALL);
    }
    fc->durum = ida_fc_dis_durum(fc, o->now.ns);
}

/* G-Ç29: bayatlık acil durdurmanın etkin değerini kısıtlayana döndürür; bu dönüş bir daraltıcı
   girdidir (A4-K44, K111: SAFETY_CLOSE(IDA_TUBE_ALL), health.narrowed, genişleticiler
   NARROWED) ve aynı çağrıda acil durdurma gevşetilemez. */
static void bayatlik(ida_fc_t *fc, ida_emn_t *e, const ida_fc_ortam_t *o)
{
    if (!ida_fc_emn_bayat(fc, e, o)) {
        return;
    }
    e->cagri_kisit = (uint8_t)(e->cagri_kisit | IDA_EMN_ESTOP);
    if ((e->kisitli & IDA_EMN_ESTOP) != 0u) {
        return; /* zaten kısıtlayan: yeni olay yok (A4-K111) */
    }
    e->kisitli = (uint8_t)(e->kisitli | IDA_EMN_ESTOP);
    e->d.kapanan = (uint8_t)(e->d.kapanan | IDA_EMN_ESTOP);
    e->d.herhangi = true;
    e->d.kapat = (uint8_t)(e->d.kapat | IDA_KAPAT_ALL);
    const ida_log_t k = {.t_w = o->now,
                         .code = (uint16_t)IDA_LOG_SAFETY_INPUT,
                         .arg0 = 1,
                         .arg1 = ida_fc_gunluk_tup_bitleri(e->kisitli)};
    ida_gun_ekle(o->gun, &k);
}

void ida_fc_daralt(ida_fc_t *fc, ida_emn_t *e, const ida_fc_ortam_t *o)
{
    const int64_t now = o->now.ns;
    fc->bu_cagri_kilit = false;
    fc->reset_kabul = false;
    butunluk_onar(fc, o);
    bayatlik(fc, e, o);

    const ida_daralt_t *d = &e->d;
    /* W5-K7, G-Ç32: FC_DISARM ve FC_ABORT bütün tüplere (alanları okunmaz): kapatılacak bir
       şey varken (SAFE dışındaki her durum, FAULT_LATCHED dahil: G-Ç19) SAFETY_CLOSE(ALL). */
    const bool kapatilacak = (fc->ic != IDA_FC_IC_SAFE) || fc->t[0].izin_var || fc->t[1].izin_var;
    if (d->fc_disarm) {
        if (kapatilacak) {
            ida_fc_kapat(fc, (uint8_t)IDA_KAPAT_ALL);
        }
        ida_fc_guvenli(fc, now);
    }
    if (d->fc_abort) {
        if (kapatilacak) {
            ida_fc_kapat(fc, (uint8_t)IDA_KAPAT_ALL);
        }
        if (fc->ic == IDA_FC_IC_FIRE_AUTH) {
            /* 03 §2.6 evrensel tablo satır 2: atış isteği iptal, ARMED. */
            fc->istek_var = false;
            ida_fc_ic_yaz(fc, IDA_FC_IC_ARMED, now);
        }
    }
    /* Araç silahsızlandırması ve oturum sonu (G-Ç18; A4-K73). */
    if (d->disarm || d->oturum_sonu) {
        ida_fc_guvenli(fc, now);
    }
    if (d->oturum_sonu) {
        /* Alıcı oturum sonunda bütün izinleri düşürür ve bekleyen olayları siler (A1-K23):
           geri okuma izleri yeni oturuma taşınmaz. */
        for (uint32_t i = 0u; i < IDA_PORT_N_TUBE; i++) {
            ida_fc_tup_t *t = &fc->t[i];
            t->izin_var = false;
            t->atis_izleme = false;
            t->kapat_izleme = false;
            t->celiski = false;
            t->kapanis_dogrulanmadi = false;
            fc->son_tup[i] = 0u;
        }
        fc->emn_tmeas_var = false;
        fc->oturum_sonu_bekliyor = true;
    }
    /* Emniyet girdisinin kapanması (A4-K111, K116; G-Ç16): acil durdurma bütün FC'yi, fiş ya
       da pim yalnız silahlı tüpü güvenli duruma alır; öteki tüpün kapanması emniyet
       modülünün SAFETY_CLOSE(tüp)'üdür. */
    if ((d->kapanan & IDA_EMN_ESTOP) != 0u) {
        ida_fc_guvenli(fc, now);
    }
    if ((fc->tup == 0u) && ((d->kapanan & (IDA_EMN_FIS_T0 | IDA_EMN_PIM_T0)) != 0u)) {
        ida_fc_guvenli(fc, now);
    }
    if ((fc->tup == 1u) && ((d->kapanan & (IDA_EMN_FIS_T1 | IDA_EMN_PIM_T1)) != 0u)) {
        ida_fc_guvenli(fc, now);
    }
    /* NARROW_MODE (geçerli ya da geçersiz hedef): atış izninin kip koşulu düşer (A4-K46,
       K127); FIRE_AUTHORIZED'dan ARMED'e. */
    if (d->kip_izni_dus) {
        fc->kip_izni_dusuk = true;
        if (fc->ic == IDA_FC_IC_FIRE_AUTH) {
            ida_fc_kapat_tup(fc, fc->tup);
            fc->istek_var = false;
            ida_fc_ic_yaz(fc, IDA_FC_IC_ARMED, now);
        }
    }
    e->d.kapat = (uint8_t)(e->d.kapat | fc->kapat);
    fc->kapat = 0u;
    fc->durum = ida_fc_dis_durum(fc, now);
}

void ida_fc_oturum_basi(ida_fc_t *fc)
{
    fc->emn_tmeas_var = false; /* yeni oturumun ilk örneği ölçüm zamanı tabanını kurar */
}

/* ---- (e): kabul edilen emniyet örneği (G-Ç14, G-Ç17, G-Ç29, G-Ç53) ---------------------- */

void ida_fc_emniyet_ornegi(ida_fc_t *fc, ida_emn_t *e, const ida_sample_t *s,
                           const ida_fc_ortam_t *o)
{
    if (s->hdr.inst != 0u) {
        return; /* emniyet girdisinin tek kaynağı inst = 0 (A4-K116); kısıtlama (b)'de */
    }
    const int64_t now = o->now.ns;
    const int64_t t_meas = s->hdr.t_meas.ns;
    /* Bayatlık izleri (W5-K4 eki). */
    const bool ilk = !e->inst0_var || !fc->emn_tmeas_var;
    fc->emn_rx_ns = s->hdr.t_rx.ns;
    if (ilk || (t_meas > fc->emn_tmeas_ns)) {
        fc->emn_ilerleme_ns = s->hdr.t_rx.ns;
    }
    fc->emn_tmeas_ns = t_meas;
    fc->emn_tmeas_var = true;
    /* LOADED ve PERMIT_OPEN son kabul edilen inst = 0 örneğinden (A4-K116, K128; G-Ç17). */
    e->inst0_var = true;
    for (uint32_t i = 0u; i < IDA_PORT_N_TUBE; i++) {
        e->inst0_tube[i] = (uint8_t)(s->u.safety.tube[i] & TUP_TANIMLI);
        fc->son_tup[i] = e->inst0_tube[i];
    }
    /* G-Ç14 (A4-K116): gevşeten değer yalnız bu kabul edilen inst = 0 örneğiyle ve bu çağrıda
       hiçbir örnek o alanı kısıtlamıyorsa etkin olur; bayatlık sürerken acil durdurma
       gevşemez (G-Ç29). */
    uint8_t gevsek = (uint8_t)((uint8_t)~ida_emn_kisit_icerigi(&s->u.safety) & IDA_EMN_TUMU);
    gevsek = (uint8_t)(gevsek & (uint8_t)~e->cagri_kisit);
    if (ida_fc_emn_bayat(fc, e, o)) {
        gevsek = (uint8_t)(gevsek & (uint8_t)~IDA_EMN_ESTOP);
    }
    const uint8_t once = e->kisitli;
    e->kisitli = (uint8_t)(e->kisitli & (uint8_t)~gevsek);
    if (e->kisitli != once) {
        fc->emn_gevsedi = true;
    }
    izleri_degerlendir(fc, fc->son_tup, t_meas, now);
}

void ida_fc_algilama(ida_fc_t *fc, const ida_in_t *in, ida_wtime_t now)
{
    (void)fc;
    (void)in;
    (void)now; /* kapsam dışı (kullanıcı kararı): algılama girdisi okunmaz */
}

/* ---- Olay sonuçları (G-Ç23, G-Ç42, G-Ç49, G-Ç50; W5-K2, K11) ----------------------------- */

static void kapat_sonucu(ida_fc_t *fc, const ida_olay_bekleyen_t *b, const ida_event_result_t *r,
                         bool bilinmiyor, int64_t now)
{
    const bool hepsi = b->tube == (uint8_t)IDA_TUBE_ALL;
    const uint8_t sonuc = bilinmiyor ? 0u : r->result;
    const bool belirsiz = bilinmiyor || (sonuc == (uint8_t)IDA_EVR_EXPIRED_NO_ACK);
    /* Son SESSION_END'den önce çıkmış kapatma: alıcı oturum sonunda bütün izinleri düşürdü
       (A1-K23; A4-K71), izin durumu belirsiz değildir; kapatma yinelenmez, kilit yine kurulur
       (G-Ç50'nin harfi). */
    const bool eski_oturum = b->event_id <= fc->oturum_siniri_id;
    for (uint8_t i = 0u; i < IDA_PORT_N_TUBE; i++) {
        if (!hepsi && (b->tube != i)) {
            continue;
        }
        ida_fc_tup_t *t = &fc->t[i];
        if ((sonuc == (uint8_t)IDA_EVR_APPLIED) || (sonuc == (uint8_t)IDA_EVR_DUPLICATE)) {
            t->kapanis_dogrulanmadi = false;
            /* G-Ç53 (1)'in izi: bu kapatmadan sonra çıkan bir SAFETY_OPEN yoksa. */
            if (t->acik_id < b->event_id) {
                t->kapat_izleme = true;
                t->kapat_t_apply_ns = r->t_apply.ns;
            }
        } else if (belirsiz) {
            t->kapanis_dogrulanmadi = !eski_oturum;
        } else if (sonuc == (uint8_t)IDA_EVR_SESSION_END) {
            t->kapanis_dogrulanmadi = false; /* oturum sonunda alıcı bütün izinleri düşürür */
        } else {
            /* REJ_*, DROPPED_*: G-Ç50'nin kapsamı dışında; izin alıcıda hold_ns ile düşer */
        }
    }
    son_ornekle_degerlendir(fc, now); /* G-Ç53 (1): örnek sonuçtan önce gelmiş olabilir */
    if (belirsiz) {
        /* G-Ç50 (W5-K11 a): FAULT_LATCHED ve kapatma yinelenir. */
        const uint8_t yinele = hepsi ? (uint8_t)IDA_KAPAT_ALL : kapat_biti(b->tube);
        ida_fc_kilitle(fc, eski_oturum ? 0u : yinele, now);
    }
}

void ida_fc_olay_sonucu(ida_fc_t *fc, const ida_olay_bekleyen_t *olay,
                        const ida_event_result_t *r, const ida_fc_ortam_t *o)
{
    const int64_t now = o->now.ns;
    if (olay->kind == (uint8_t)IDA_EVT_SAFETY_CLOSE) {
        kapat_sonucu(fc, olay, r, false, now);
        return;
    }
    if (olay->tube >= IDA_PORT_N_TUBE) {
        return;
    }
    ida_fc_tup_t *t = &fc->t[olay->tube];
    if (olay->kind == (uint8_t)IDA_EVT_SAFETY_OPEN) {
        const bool son = olay->event_id == t->acik_id;
        if (son) {
            t->acik_bekliyor = false;
        }
        if (r->result == (uint8_t)IDA_EVR_APPLIED) {
            /* G-Ç11: izin bu sonuçla ve üreticinin hold_ns'iyle açık sayılır; yalnız atış
               penceresi sürerken (başka durumda izin kapatılmıştır). */
            if (son && (fc->ic == IDA_FC_IC_FIRE_AUTH) && (fc->tup == olay->tube)) {
                t->izin_var = true;
                t->izin_bitis_ns = olay->cikis_now.ns + (int64_t)t->acik_hold_ns;
                t->izin_t_apply_ns = r->t_apply.ns;
            }
        } else {
            /* G-Ç23 (W5-K2): APPLIED dışındaki her son sonuç → izin kapalı, SAFETY_CLOSE(tüp),
               SAFE (FAULT_LATCHED korunur). */
            ida_fc_kapat_tup(fc, olay->tube);
            ida_fc_guvenli(fc, now);
        }
        return;
    }
    /* FIRE (G-Ç49; W5-K2) */
    t->fire_bekliyor = false;
    switch (r->result) {
    case IDA_EVR_APPLIED:
    case IDA_EVR_DUPLICATE:
        /* 03 §2.6 FIRING: tüp boşaldı geri okuması beklenir (T_tup_bos; G-Ç53 (2)). */
        t->atis_izleme = true;
        t->atis_t_apply_ns = r->t_apply.ns;
        t->atis_izleme_bas_ns = now;
        son_ornekle_degerlendir(fc, now); /* örnek sonuçtan önce gelmiş olabilir */
        break;
    case IDA_EVR_REJ_TUBE_EMPTY:
    case IDA_EVR_EXPIRED_NO_ACK:
        ida_fc_kilitle(fc, kapat_biti(olay->tube), now);
        break;
    default: /* REJ_SAFE, REJ_EXPIRED, REJ_SESSION, DROPPED_GATE, DROPPED_NO_SLOT, SESSION_END */
        ida_fc_kapat_tup(fc, olay->tube);
        ida_fc_guvenli(fc, now);
        break;
    }
}

void ida_fc_olay_bilinmiyor(ida_fc_t *fc, const ida_olay_bekleyen_t *olay,
                            const ida_fc_ortam_t *o)
{
    const int64_t now = o->now.ns;
    /* G-Ç42 (A4-K70, W5-K2): sonucu bilinmeyen genişletici olay hiçbir zaman uygulanmış
       sayılmaz. */
    if (olay->kind == (uint8_t)IDA_EVT_SAFETY_CLOSE) {
        const ida_event_result_t yok = {0};
        kapat_sonucu(fc, olay, &yok, true, now);
        return;
    }
    if (olay->tube >= IDA_PORT_N_TUBE) {
        return;
    }
    ida_fc_tup_t *t = &fc->t[olay->tube];
    if (olay->kind == (uint8_t)IDA_EVT_SAFETY_OPEN) {
        if (olay->event_id == t->acik_id) {
            t->acik_bekliyor = false;
        }
        ida_fc_kapat_tup(fc, olay->tube); /* G-Ç23 */
        ida_fc_guvenli(fc, now);
        return;
    }
    t->fire_bekliyor = false; /* G-Ç49: fırlatıcının durumu bilinmiyor */
    ida_fc_kilitle(fc, kapat_biti(olay->tube), now);
}
