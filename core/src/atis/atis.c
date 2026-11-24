/*
 * atis.c — atış kontrol emniyet zincirinin durum makinesi: iç durumlar ve dış eşleme, engel
 * maskesi, FC komutları, süreler ve evrensel geçişler, genişletici olay adayları
 * (W5-atis-emniyet; 03 §2.6, §2.6.1, §2.7; Ek G-Ç maddeleri yorumlarda). Kapsam sınırı
 * atis.h başındadır: atış ölçütü, hedefleme ve algılama yoktur.
 */
#include "atis/atis.h"

#include <string.h>

#include "atis/atis_ic.h"
#include "zaman/zaman.h"

/* ---- İç yardımcılar ---------------------------------------------------------------------- */

void ida_fc_ic_yaz(ida_fc_t *fc, uint32_t ic, int64_t now_ns)
{
    fc->ic = ic;
    fc->ic_tumleyen = ~ic;
    fc->giris_ns = now_ns;
}

void ida_fc_tup_yaz(ida_fc_t *fc, uint8_t tup)
{
    fc->tup = tup;
    fc->tup_tumleyen = (uint8_t)~tup;
}

void ida_fc_kapat(ida_fc_t *fc, uint8_t kapat)
{
    fc->kapat = (uint8_t)(fc->kapat | (kapat & (IDA_KAPAT_T0 | IDA_KAPAT_T1 | IDA_KAPAT_ALL)));
    const bool hepsi = (kapat & IDA_KAPAT_ALL) != 0u;
    if (hepsi || ((kapat & IDA_KAPAT_T0) != 0u)) {
        fc->t[0].izin_var = false;
    }
    if (hepsi || ((kapat & IDA_KAPAT_T1) != 0u)) {
        fc->t[1].izin_var = false;
    }
}

void ida_fc_kapat_tup(ida_fc_t *fc, uint8_t tup)
{
    if (tup == 0u) {
        ida_fc_kapat(fc, (uint8_t)IDA_KAPAT_T0);
    } else if (tup == 1u) {
        ida_fc_kapat(fc, (uint8_t)IDA_KAPAT_T1);
    } else {
        /* silahlı tüp yok */
    }
}

bool ida_fc_etkin(const ida_fc_t *fc)
{
    return (fc->ic == IDA_FC_IC_ARMED) || (fc->ic == IDA_FC_IC_FIRE_AUTH) ||
           (fc->ic == IDA_FC_IC_FIRING) || (fc->ic == IDA_FC_IC_ASSESS) ||
           (fc->ic == IDA_FC_IC_TEST);
}

/* G-Ç18, G-Ç19, G-Ç25: W5-K8 — FAULT_LATCHED korunur; silahlandırma ve izinler kalkar. */
void ida_fc_guvenli(ida_fc_t *fc, int64_t now_ns)
{
    ida_fc_kapat_tup(fc, fc->tup);
    for (uint8_t i = 0u; i < IDA_PORT_N_TUBE; i++) {
        if (fc->t[i].izin_var) {
            ida_fc_kapat_tup(fc, i); /* açık sayılan izin silahlı tüpte değilse de kapanır */
        }
    }
    fc->istek_var = false;
    ida_fc_tup_yaz(fc, IDA_FC_TUP_YOK);
    if ((fc->ic != IDA_FC_IC_FAULT) && (fc->ic != IDA_FC_IC_SAFE)) {
        ida_fc_ic_yaz(fc, IDA_FC_IC_SAFE, now_ns);
    }
}

/* W5-K1, K2, K4, K11 (a); G-Ç49, G-Ç50, G-Ç53. */
void ida_fc_kilitle(ida_fc_t *fc, uint8_t kapat, int64_t now_ns)
{
    ida_fc_kapat(fc, kapat);
    ida_fc_kapat_tup(fc, fc->tup);
    fc->istek_var = false;
    ida_fc_tup_yaz(fc, IDA_FC_TUP_YOK);
    if (fc->ic != IDA_FC_IC_FAULT) {
        ida_fc_ic_yaz(fc, IDA_FC_IC_FAULT, now_ns);
    }
    fc->bu_cagri_kilit = true;
}

/* G-Ç11: izin yalnız o izni veren SAFETY_OPEN APPLIED aldıysa ve olayı çıkaran çağrının
   now_w'sinden bu yana hold_ns dolmadıysa açıktır (duvar saati; bariyer beklemesinde de
   dolar, A4-K66, K107). */
bool ida_fc_izin_acik(const ida_fc_t *fc, uint8_t tup, int64_t now_ns)
{
    if (tup >= IDA_PORT_N_TUBE) {
        return false;
    }
    const ida_fc_tup_t *t = &fc->t[tup];
    return t->izin_var && (now_ns < t->izin_bitis_ns);
}

/* 03 §2.6.1 md. 1 (W5-K3): iç durumların A4 §3.16'ya eşlemesi; G-Ç02. */
uint8_t ida_fc_dis_durum(const ida_fc_t *fc, int64_t now_ns)
{
    switch (fc->ic) {
    case IDA_FC_IC_SAFE:
        return (uint8_t)IDA_FCS_SAFE;
    case IDA_FC_IC_ARMED:
    case IDA_FC_IC_ASSESS:
        return (uint8_t)IDA_FCS_ARMED;
    case IDA_FC_IC_FIRE_AUTH:
        return ida_fc_izin_acik(fc, fc->tup, now_ns) ? (uint8_t)IDA_FCS_PERMIT_OPEN
                                                     : (uint8_t)IDA_FCS_ARMED;
    case IDA_FC_IC_FIRING:
        return (uint8_t)IDA_FCS_FIRE_PENDING;
    case IDA_FC_IC_TEST:
        return (uint8_t)IDA_FCS_TEST;
    default:
        return (uint8_t)IDA_FCS_FAULT_LATCHED; /* FAULT ve tanımsız desen */
    }
}

uint32_t ida_fc_tup_bitleri(uint8_t tup)
{
    return (tup == 0u) ? (IDA_FCI_PLUG_T0 | IDA_FCI_PIN_T0 | IDA_FCI_EMPTY_T0)
                       : (IDA_FCI_PLUG_T1 | IDA_FCI_PIN_T1 | IDA_FCI_EMPTY_T1);
}

/* ---- Kurulum ve bütünlük (G-Ç01, G-Ç26) ------------------------------------------------- */

void ida_fc_kur(ida_fc_t *fc)
{
    (void)memset(fc, 0, sizeof(*fc));
    ida_fc_ic_yaz(fc, IDA_FC_IC_SAFE, 0);
    ida_fc_tup_yaz(fc, IDA_FC_TUP_YOK);
    fc->durum = (uint8_t)IDA_FCS_SAFE; /* G-Ç01: silahlandırma init'ler arasında taşınmaz */
    fc->onceki_kip = (uint8_t)IDA_MODE_INIT;
}

static bool ic_tanimli(uint32_t ic)
{
    return (ic == IDA_FC_IC_SAFE) || (ic == IDA_FC_IC_ARMED) || (ic == IDA_FC_IC_FIRE_AUTH) ||
           (ic == IDA_FC_IC_FIRING) || (ic == IDA_FC_IC_ASSESS) || (ic == IDA_FC_IC_TEST) ||
           (ic == IDA_FC_IC_FAULT);
}

static bool bayt_bool(const bool *b)
{
    uint8_t v = 0u;
    (void)memcpy(&v, b, sizeof(v));
    return v <= 1u;
}

/* İç durum, silahlı tüp ve bayrakların geçerliliği (yansıma `durum` hariç). */
bool ida_fc_ic_tutarli(const ida_fc_t *fc)
{
    if (!ic_tanimli(fc->ic) || (fc->ic_tumleyen != ~fc->ic)) {
        return false;
    }
    if (fc->tup_tumleyen != (uint8_t)~fc->tup) {
        return false;
    }
    const bool silahli = (fc->ic == IDA_FC_IC_ARMED) || (fc->ic == IDA_FC_IC_FIRE_AUTH) ||
                         (fc->ic == IDA_FC_IC_FIRING) || (fc->ic == IDA_FC_IC_ASSESS);
    if (silahli ? (fc->tup >= IDA_PORT_N_TUBE) : (fc->tup != IDA_FC_TUP_YOK)) {
        return false;
    }
    bool b = bayt_bool(&fc->istek_var) && bayt_bool(&fc->kip_izni_dusuk) &&
             bayt_bool(&fc->emn_tmeas_var) && bayt_bool(&fc->emn_gevsedi) &&
             bayt_bool(&fc->butunluk_kilidi) && bayt_bool(&fc->bu_cagri_kilit) &&
             bayt_bool(&fc->reset_kabul) && bayt_bool(&fc->oturum_sonu_bekliyor);
    for (uint32_t i = 0u; i < IDA_PORT_N_TUBE; i++) {
        const ida_fc_tup_t *t = &fc->t[i];
        b = b && bayt_bool(&t->acik_bekliyor) && bayt_bool(&t->izin_var) &&
            bayt_bool(&t->fire_bekliyor) && bayt_bool(&t->atis_izleme) &&
            bayt_bool(&t->kapat_izleme) && bayt_bool(&t->celiski) &&
            bayt_bool(&t->kapanis_dogrulanmadi);
    }
    const uint32_t tup_tanimli = IDA_TUBE_LOADED | IDA_TUBE_PLUG_IN | IDA_TUBE_PIN_OUT |
                                 IDA_TUBE_PERMIT_OPEN;
    b = b && (((uint32_t)fc->son_tup[0] & ~tup_tanimli) == 0u) &&
        (((uint32_t)fc->son_tup[1] & ~tup_tanimli) == 0u);
    return b && (((uint32_t)fc->kapat & ~(uint32_t)(IDA_KAPAT_T0 | IDA_KAPAT_T1 | IDA_KAPAT_ALL)) == 0u) &&
           (fc->onceki_kip <= (uint8_t)IDA_MODE_AUTO);
}

bool ida_fc_tutarli(const ida_fc_t *fc)
{
    return (fc->durum <= (uint8_t)IDA_FCS_FAULT_LATCHED) && ida_fc_ic_tutarli(fc);
}

/* ---- Engel maskesi (G-Ç02, G-Ç03; A4 §3.16) --------------------------------------------- */

uint32_t ida_fc_engel(const ida_fc_t *fc, const ida_fc_baglam_t *b)
{
    uint32_t m = b->emn_fci; /* ESTOP, PLUG_T*, PIN_T*, EMPTY_T* (A4-K116, K128) */
    if (!b->armed) {
        m |= IDA_FCI_NOT_ARMED;
    }
    /* MODE: kip atış izni kümesinde değil ya da kip koşulu NARROW_MODE ile düştü (A4-K46). */
    if ((b->mode != (uint8_t)IDA_FC_IZIN_KIPI) || fc->kip_izni_dusuk) {
        m |= IDA_FCI_MODE;
    }
    if (b->est_level != (uint8_t)IDA_EST_OK) {
        m |= IDA_FCI_EST;
    }
    if (b->fs_level >= (uint8_t)IDA_FS_HOLD) {
        m |= IDA_FCI_FAILSAFE;
    }
    if (!b->events_ok) {
        m |= IDA_FCI_EVENTS_OFF;
    }
    if (b->imu_lag) {
        m |= IDA_FCI_IMU_LAG;
    }
    if (b->imu_bayat) {
        m |= IDA_FCI_IMU_STALE;
    }
    if (!b->oturum_var) {
        m |= IDA_FCI_SESSION;
    }
    if (b->bekleyen_tupler != 0u) {
        m |= IDA_FCI_RESULT_PENDING;
    }
    if (fc->ic == IDA_FC_IC_FAULT) {
        m |= IDA_FCI_FAULT_LATCHED;
    }
    /* A4-K118: algılama girdisi bu sürümde okunmaz (kapsam dışı); kamera akışından hiç başlık
       kabul edilmediği için akış her zaman bayattır. */
    m |= IDA_FCI_DET_STALE;
    return m & IDA_FCI_DEFINED; /* G-Ç02: bit 18–31 her zaman 0 */
}

uint8_t ida_fc_durum(const ida_fc_t *fc)
{
    return fc->durum;
}

/* ---- Süreler ve evrensel geçişler (G-Ç25, G-Ç48; 03 §2.6) -------------------------------- */

static int64_t ms_param(const ida_fc_ortam_t *o, enum ida_prm_indis i)
{
    return ida_ms_ns(ida_prm_u32(o->prm, i));
}

void ida_fc_evrensel(ida_fc_t *fc, const ida_fc_baglam_t *b, const ida_fc_ortam_t *o)
{
    if (!ida_fc_etkin(fc)) {
        return; /* SAFE: dönülecek durum yok; FAULT_LATCHED: W5-K8 ile korunur (G-Ç19) */
    }
    const int64_t now = o->now.ns;
    const bool kestirim_kotu = (b->est_level != (uint8_t)IDA_EST_OK) &&
                               (b->est_level != (uint8_t)IDA_EST_DEGRADED); /* EST_BAD */
    bool tup_kisitli = false;
    if (fc->tup < IDA_PORT_N_TUBE) {
        const uint32_t fis_pim = (fc->tup == 0u) ? (IDA_FCI_PLUG_T0 | IDA_FCI_PIN_T0)
                                                 : (IDA_FCI_PLUG_T1 | IDA_FCI_PIN_T1);
        tup_kisitli = (b->emn_fci & fis_pim) != 0u;
    }
    /* G-Ç25: yer istasyonu kalp atışı T_hb_fc boyunca gelmedi (A4-K52; duvar saati). */
    const bool hb_kayip = (now - b->gcs_son_ns) > ms_param(o, IDA_PRM_fc_hb_ms);
    const bool guvenliye = !b->armed || (b->fs_level >= (uint8_t)IDA_FS_HOLD) || kestirim_kotu ||
                           ((b->emn_fci & IDA_FCI_ESTOP) != 0u) || tup_kisitli || hb_kayip ||
                           b->imu_bayat || !b->oturum_var || !b->events_ok; /* W5-K11 (b) */
    if (guvenliye) {
        ida_fc_guvenli(fc, now);
        return;
    }
    /* Kip koşulu (MODE): yalnız FIRE_AUTHORIZED'dan ARMED'e (03 §2.6 evrensel tablo satır 2). */
    if (((b->mode != (uint8_t)IDA_FC_IZIN_KIPI) || fc->kip_izni_dusuk) &&
        (fc->ic == IDA_FC_IC_FIRE_AUTH)) {
        ida_fc_kapat_tup(fc, fc->tup);
        fc->istek_var = false;
        ida_fc_ic_yaz(fc, IDA_FC_IC_ARMED, now);
    }
}

void ida_fc_cagri(ida_fc_t *fc, const ida_emn_t *e, const ida_fc_baglam_t *b,
                  const ida_fc_ortam_t *o)
{
    const int64_t now = o->now.ns;
    /* Gevşetmenin günlüğü (A4 §3.15 satır 19): bir çağrıda bir kez. */
    if (fc->emn_gevsedi) {
        fc->emn_gevsedi = false;
        const ida_log_t k = {.t_w = o->now,
                             .code = (uint16_t)IDA_LOG_SAFETY_INPUT,
                             .arg0 = ((e->kisitli & IDA_EMN_ESTOP) != 0u) ? 1 : 0,
                             .arg1 = ida_fc_gunluk_tup_bitleri(e->kisitli)};
        ida_gun_ekle(o->gun, &k);
    }
    /* Kip koşulu: kip izin kümesine yeniden girildiğinde NARROW_MODE'un düşürmesi kalkar. */
    if (b->mode != fc->onceki_kip) {
        if (b->mode == (uint8_t)IDA_FC_IZIN_KIPI) {
            fc->kip_izni_dusuk = false;
        }
        fc->onceki_kip = (b->mode <= (uint8_t)IDA_MODE_AUTO) ? b->mode : (uint8_t)IDA_MODE_INIT;
    }
    for (uint8_t i = 0u; i < IDA_PORT_N_TUBE; i++) {
        ida_fc_tup_t *t = &fc->t[i];
        /* 03 §2.6 FIRING: tüp boşaldı geri okuması T_tup_bos içinde gelmedi → FAULT_LATCHED. */
        if (t->atis_izleme && ((now - t->atis_izleme_bas_ns) > ida_ms_ns(IDA_FC_T_TUP_BOS_MS))) {
            t->atis_izleme = false;
            ida_fc_kilitle(fc, (i == 0u) ? (uint8_t)IDA_KAPAT_T0 : (uint8_t)IDA_KAPAT_T1, now);
        }
        /* Çekirdeğin saatinde süresi dolan izin kendiliğinden kapanır (A4-K107). */
        if (t->izin_var && (now >= t->izin_bitis_ns)) {
            t->izin_var = false;
        }
    }
    /* G-Ç48: T_pencere dolduğunda ve ASSESS bittiğinde SAFETY_CLOSE(tüp) ile SAFE (W5-K3). */
    if ((fc->ic == IDA_FC_IC_FIRE_AUTH) &&
        ((now - fc->giris_ns) >= ms_param(o, IDA_PRM_fc_pencere_ms))) {
        ida_fc_guvenli(fc, now);
    }
    if ((fc->ic == IDA_FC_IC_ASSESS) && ((now - fc->giris_ns) >= ida_ms_ns(IDA_FC_T_ASSESS_MS))) {
        ida_fc_guvenli(fc, now);
    }
    ida_fc_evrensel(fc, b, o);
}

/* ---- Komutlar (A4 §3.17; G-Ç22, G-Ç46, G-Ç20, G-Ç51) ------------------------------------ */

static void yanit(ida_cmd_result_t *r, uint8_t ack, uint16_t neden)
{
    r->ack = ack;
    r->reason = neden;
}

/* FC_ARM ve FC_TEST_ENTER satırının engel koşulları (3.17, satırdaki sırayla). */
static uint16_t arm_engeli(uint32_t e)
{
    if ((e & IDA_FCI_SESSION) != 0u) {
        return (uint16_t)IDA_REASON_SESSION;
    }
    if ((e & IDA_FCI_EVENTS_OFF) != 0u) {
        return (uint16_t)IDA_REASON_EVENTS_OFF;
    }
    if ((e & IDA_FCI_MODE) != 0u) {
        return (uint16_t)IDA_REASON_BAD_MODE;
    }
    const uint32_t kosul = IDA_FCI_NOT_ARMED | IDA_FCI_EST | IDA_FCI_FAILSAFE | IDA_FCI_ESTOP |
                           IDA_FCI_IMU_STALE | IDA_FCI_FAULT_LATCHED;
    if ((e & kosul) != 0u) {
        return (uint16_t)IDA_REASON_NOT_ARMABLE;
    }
    return (uint16_t)IDA_REASON_NONE;
}

void ida_fc_komut(ida_fc_t *fc, const ida_komut_baglam_t *b, const ida_fc_cmd_t *cmd,
                  ida_cmd_result_t *r)
{
    const uint32_t e = b->fc_engel; /* komutlardan önceki engel maskesi (3.17) */
    const int64_t now = b->now_w.ns;
    uint16_t neden = (uint16_t)IDA_REASON_NONE;
    switch (cmd->op) {
    case IDA_FC_ARM:
        /* G-Ç22: yalnız SAFE'te; FC_ARM izin açmaz (W5-K3, G-Ç47). */
        neden = arm_engeli(e);
        if ((neden == (uint16_t)IDA_REASON_NONE) && (fc->ic != IDA_FC_IC_SAFE)) {
            neden = (uint16_t)IDA_REASON_BAD_MODE;
        }
        if (neden == (uint16_t)IDA_REASON_NONE) {
            ida_fc_tup_yaz(fc, cmd->tube);
            ida_fc_ic_yaz(fc, IDA_FC_IC_ARMED, now);
        }
        break;
    case IDA_FC_TEST_ENTER:
        /* G-Ç22: yalnız SAFE'te; TEST'te izin açan olay yoktur (W5-K9, G-Ç24). */
        neden = arm_engeli(e);
        if ((neden == (uint16_t)IDA_REASON_NONE) && (fc->ic != IDA_FC_IC_SAFE)) {
            neden = (uint16_t)IDA_REASON_BAD_MODE;
        }
        if (neden == (uint16_t)IDA_REASON_NONE) {
            ida_fc_ic_yaz(fc, IDA_FC_IC_TEST, now);
        }
        break;
    case IDA_FC_FIRE:
        /* G-Ç22: yalnız ARMED'de ve FC_ARM'ın kabul edildiği tüp için; G-Ç21: kabul, tek bir
           FIRE'a bağlanan operatör isteğidir; G-Ç47: izin penceresi bu kabulle açılır. */
        neden = arm_engeli(e);
        if ((neden == (uint16_t)IDA_REASON_NONE) &&
            ((e & (ida_fc_tup_bitleri(cmd->tube) | IDA_FCI_RESULT_PENDING)) != 0u)) {
            neden = (uint16_t)IDA_REASON_NOT_ARMABLE;
        }
        if ((neden == (uint16_t)IDA_REASON_NONE) &&
            ((fc->ic != IDA_FC_IC_ARMED) || (fc->tup != cmd->tube))) {
            neden = (uint16_t)IDA_REASON_BAD_MODE;
        }
        if (neden == (uint16_t)IDA_REASON_NONE) {
            fc->istek_var = true;
            ida_fc_ic_yaz(fc, IDA_FC_IC_FIRE_AUTH, now);
        }
        break;
    case IDA_FC_RESET:
        /* G-Ç46, G-Ç20 (W5-K1): kabul durumu SAFE yapar; neden çağrı sonunda sürüyorsa
           ida_fc_adim yeniden kilitler. İzin açmaz, silahlandırmaz. */
        if ((e & IDA_FCI_SESSION) != 0u) {
            neden = (uint16_t)IDA_REASON_SESSION;
        } else if (fc->ic != IDA_FC_IC_FAULT) {
            neden = (uint16_t)IDA_REASON_BAD_MODE;
        } else {
            fc->istek_var = false;
            ida_fc_tup_yaz(fc, IDA_FC_TUP_YOK);
            ida_fc_ic_yaz(fc, IDA_FC_IC_SAFE, now);
            fc->reset_kabul = true;
        }
        break;
    default:
        /* IDA_FC_TEST_EXIT (G-Ç51): TEST'te SAFE'e geçer, başka durumda etkisiz kabul. */
        if (fc->ic == IDA_FC_IC_TEST) {
            ida_fc_ic_yaz(fc, IDA_FC_IC_SAFE, now);
        }
        break;
    }
    if (neden == (uint16_t)IDA_REASON_NONE) {
        yanit(r, (uint8_t)IDA_ACK_ACCEPTED, (uint16_t)IDA_REASON_NONE);
    } else {
        yanit(r, (uint8_t)IDA_ACK_REJECTED, neden);
    }
}

/* ---- Adım: yeniden kilitlenme ve genişletici adaylar (G-Ç20, G-Ç21, G-Ç47, G-Ç12) -------- */

/* W5-K1: çağrı sonunda süren kilitli arıza nedenleri. */
static bool kalici_neden(const ida_fc_t *fc)
{
    bool n = fc->butunluk_kilidi;
    for (uint32_t i = 0u; i < IDA_PORT_N_TUBE; i++) {
        n = n || fc->t[i].celiski || fc->t[i].kapanis_dogrulanmadi;
    }
    return n;
}

static void aday_ekle(ida_olay_t *olay, uint8_t kind, uint8_t tup, uint32_t hold_ns)
{
    if (olay->n_aday >= IDA_PORT_MAX_EVENTS) {
        return;
    }
    ida_event_t *a = &olay->aday[olay->n_aday];
    (void)memset(a, 0, sizeof(*a));
    a->kind = kind;
    a->tube = tup;
    a->hold_ns = hold_ns;
    olay->n_aday++;
}

/* FIRE_AUTHORIZED'da tek aday: önce FIRE (izin açık ve geri okumada izin açık), değilse izni
   açan ya da yenileyen SAFETY_OPEN. Kapı (ida_olay_yaz) yalnız siler. */
static void aday_uret(ida_fc_t *fc, const ida_emn_t *e, const ida_fc_baglam_t *b,
                      ida_olay_t *olay, const ida_fc_ortam_t *o)
{
    const uint8_t tp = fc->tup;
    if ((tp >= IDA_PORT_N_TUBE) || !fc->istek_var) {
        return;
    }
    const int64_t now = o->now.ns;
    /* FC_FIRE satırının bütün koşulları ve IMU_LAG sürerken genişletici olay yok (A4-K108,
       K116, K123; G-Ç16, G-Ç35). */
    const uint32_t engel = ida_fc_engel(fc, b);
    const uint32_t yasak = IDA_FCI_SESSION | IDA_FCI_EVENTS_OFF | IDA_FCI_MODE |
                           IDA_FCI_NOT_ARMED | IDA_FCI_EST | IDA_FCI_FAILSAFE | IDA_FCI_ESTOP |
                           IDA_FCI_IMU_STALE | IDA_FCI_IMU_LAG | IDA_FCI_FAULT_LATCHED |
                           ida_fc_tup_bitleri(tp);
    if ((engel & yasak) != 0u) {
        return;
    }
    const ida_fc_tup_t *t = &fc->t[tp];
    if (t->acik_bekliyor || t->fire_bekliyor) {
        return; /* G-Ç40 (A4-K123) */
    }
    const bool izin = ida_fc_izin_acik(fc, tp, now);
    if (izin) {
        /* 03 §2.6 FIRE_AUTHORIZED: izin açık sayıldığında ve PERMIT_OPEN geri okuması (izni
           veren sonucun t_apply'ından sonraki ölçümde) kuruluyken tek FIRE. */
        const bool okuma = e->inst0_var && ((e->inst0_tube[tp] & IDA_TUBE_PERMIT_OPEN) != 0u) &&
                           fc->emn_tmeas_var && (fc->emn_tmeas_ns >= t->izin_t_apply_ns);
        if (okuma) {
            aday_ekle(olay, (uint8_t)IDA_EVT_FIRE, tp, 0u);
            return;
        }
        /* G-Ç12: izni açık tutarken ardışık iki SAFETY_OPEN arası hold_ns / 2'yi aşmaz; yenileme
           hold_ns / 4'ten itibaren, yalnız önceki SAFETY_OPEN'ın sonucu alındıysa. Aralık
           kaçırıldıysa yenilenmez: izin kendi süresiyle kapanır. */
        const int64_t gecen = now - t->acik_cikis_ns;
        const int64_t hold = (int64_t)t->acik_hold_ns;
        if ((gecen >= (hold / 4)) && (gecen <= (hold / 2))) {
            aday_ekle(olay, (uint8_t)IDA_EVT_SAFETY_OPEN, tp,
                      ida_prm_u32(o->prm, IDA_PRM_fc_hold_ns));
        }
        return;
    }
    /* İzin açık değil: izni açan SAFETY_OPEN (G-Ç47). */
    aday_ekle(olay, (uint8_t)IDA_EVT_SAFETY_OPEN, tp, ida_prm_u32(o->prm, IDA_PRM_fc_hold_ns));
}

void ida_fc_adim(ida_fc_t *fc, const ida_emn_t *e, const ida_fc_baglam_t *b, ida_olay_t *olay,
                 bool step_ok, const ida_fc_ortam_t *o)
{
    const int64_t now = o->now.ns;
    /* Aynı çağrıdaki komutlardan sonra evrensel geçişler yeniden (03 §2.6: "aynı adımda"). */
    ida_fc_evrensel(fc, b, o);
    /* G-Ç20 (W5-K1): kabul edilen FC_RESET'ten sonra neden sürüyorsa aynı çağrıda yeniden. */
    if (fc->reset_kabul && (fc->bu_cagri_kilit || kalici_neden(fc))) {
        ida_fc_kilitle(fc, 0u, now);
    }
    if (step_ok && (fc->ic == IDA_FC_IC_FIRE_AUTH)) {
        aday_uret(fc, e, b, olay, o);
    }
    if (fc->kapat != 0u) {
        ida_olay_kapat_iste(olay, fc->kapat);
        fc->kapat = 0u;
    }
}

/* ---- Çıktı: çıkan olaylar ve dış durum (G-Ç21, G-Ç40, G-Ç53) ---------------------------- */

void ida_fc_cikti(ida_fc_t *fc, const ida_out_t *out, ida_wtime_t now)
{
    const uint32_t n =
        (out->n_events < IDA_PORT_MAX_EVENTS) ? (uint32_t)out->n_events : IDA_PORT_MAX_EVENTS;
    for (uint32_t i = 0u; i < n; i++) {
        const ida_event_t *ev = &out->events[i];
        if (ev->event_id > fc->son_cikan_id) {
            fc->son_cikan_id = ev->event_id;
        }
        if (ev->tube >= IDA_PORT_N_TUBE) {
            continue; /* SAFETY_CLOSE(IDA_TUBE_ALL) */
        }
        ida_fc_tup_t *t = &fc->t[ev->tube];
        if (ev->kind == (uint8_t)IDA_EVT_SAFETY_OPEN) {
            t->acik_bekliyor = true;
            t->acik_id = ev->event_id;
            t->acik_cikis_ns = now.ns;
            t->acik_hold_ns = ev->hold_ns;
            /* G-Ç53 (1): araya FC'nin çıkardığı yeni bir SAFETY_OPEN girdi. */
            t->kapat_izleme = false;
            t->celiski = false;
        } else if (ev->kind == (uint8_t)IDA_EVT_FIRE) {
            t->fire_bekliyor = true;
            fc->istek_var = false; /* G-Ç21: bir FC_FIRE en çok bir FIRE */
            if ((fc->ic == IDA_FC_IC_FIRE_AUTH) && (fc->tup == ev->tube)) {
                ida_fc_ic_yaz(fc, IDA_FC_IC_FIRING, now.ns);
            }
        } else {
            /* SAFETY_CLOSE(tüp): sonucu ida_fc_olay_sonucu'da */
        }
    }
    if (fc->oturum_sonu_bekliyor) {
        fc->oturum_sonu_bekliyor = false;
        fc->oturum_siniri_id = fc->son_cikan_id; /* oturum sonunun SAFETY_CLOSE'u dahil */
    }
    fc->durum = ida_fc_dis_durum(fc, now.ns);
}

uint8_t ida_fc_param_grup(const ida_prm_kume_t *p)
{
    (void)p; /* emniyet zinciri parametrelerinin grup koşulu yok */
    return 0u;
}
