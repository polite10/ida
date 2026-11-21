/*
 * atis.c — YER TUTUCU (W2). Sahibi: W4 atış kontrol işi. Genişletme yönü yoktur: hiçbir
 * komut kabul edilip izin açmaz, hiçbir olay adayı üretilmez.
 */
#include "atis/atis.h"

void ida_fc_kur(ida_fc_t *fc)
{
    fc->durum = (uint8_t)IDA_FCS_SAFE;
}

void ida_fc_daralt(ida_fc_t *fc, const ida_daralt_t *d)
{
    (void)d;
    fc->durum = (uint8_t)IDA_FCS_SAFE; /* yer tutucu zaten SAFE */
}

void ida_fc_emniyet_ornegi(ida_fc_t *fc, ida_emn_t *e, const ida_sample_t *s)
{
    (void)fc;
    (void)e;
    (void)s; /* gevşetme W4'ündür (M9) */
}

void ida_fc_algilama(ida_fc_t *fc, const ida_in_t *in, ida_wtime_t now)
{
    (void)fc;
    (void)in;
    (void)now; /* algılama alımı W4'ündür; yer tutucu okumaz */
}

void ida_fc_olay_sonucu(ida_fc_t *fc, const ida_olay_bekleyen_t *olay, uint8_t sonuc)
{
    (void)fc;
    (void)olay;
    (void)sonuc;
}

void ida_fc_olay_bilinmiyor(ida_fc_t *fc, const ida_olay_bekleyen_t *olay)
{
    (void)olay;
    fc->durum = (uint8_t)IDA_FCS_SAFE;
}

uint32_t ida_fc_engel(const ida_fc_t *fc, const ida_fc_baglam_t *b)
{
    uint32_t m = b->emn_fci;
    if (!b->armed) {
        m |= IDA_FCI_NOT_ARMED;
    }
    m |= IDA_FCI_MODE; /* yer tutucuda atış izni kümesi boştur */
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
    if (fc->durum == (uint8_t)IDA_FCS_FAULT_LATCHED) {
        m |= IDA_FCI_FAULT_LATCHED;
    }
    m |= IDA_FCI_DET_STALE; /* yer tutucu algılama girdisi okumaz (M7) */
    return m & IDA_FCI_DEFINED;
}

uint8_t ida_fc_durum(const ida_fc_t *fc)
{
    return fc->durum;
}

static void ret(ida_cmd_result_t *r, uint16_t neden)
{
    r->ack = (uint8_t)IDA_ACK_REJECTED;
    r->reason = neden;
}

/* FC_ARM ve FC_TEST_ENTER satırının ortak engel koşulları (3.17); tutan nedeni döner. */
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

static uint32_t tup_bitleri(uint8_t tube)
{
    return (tube == 0u) ? (IDA_FCI_PLUG_T0 | IDA_FCI_PIN_T0 | IDA_FCI_EMPTY_T0)
                        : (IDA_FCI_PLUG_T1 | IDA_FCI_PIN_T1 | IDA_FCI_EMPTY_T1);
}

void ida_fc_komut(ida_fc_t *fc, const ida_komut_baglam_t *b, const ida_fc_cmd_t *cmd,
                  ida_cmd_result_t *r)
{
    (void)fc; /* yer tutucu durumu hep SAFE; FAULT_LATCHED ve TEST yok */
    const uint32_t e = b->fc_engel;
    uint16_t neden = (uint16_t)IDA_REASON_NONE;
    switch (cmd->op) {
    case IDA_FC_ARM:
    case IDA_FC_TEST_ENTER:
        neden = arm_engeli(e);
        break;
    case IDA_FC_FIRE:
        neden = arm_engeli(e);
        if ((neden == (uint16_t)IDA_REASON_NONE) &&
            ((e & (tup_bitleri(cmd->tube) | IDA_FCI_RESULT_PENDING)) != 0u)) {
            neden = (uint16_t)IDA_REASON_NOT_ARMABLE;
        }
        break;
    case IDA_FC_RESET:
        /* SESSION; yoksa durum FAULT_LATCHED değil → BAD_MODE (yer tutucu hiç
           FAULT_LATCHED olmaz; mandallı arızanın temizlenmesi W4'ündür). */
        neden = ((e & IDA_FCI_SESSION) != 0u) ? (uint16_t)IDA_REASON_SESSION
                                               : (uint16_t)IDA_REASON_BAD_MODE;
        break;
    default: /* IDA_FC_TEST_EXIT: durum TEST değil → etkisiz kabul */
        r->ack = (uint8_t)IDA_ACK_ACCEPTED;
        r->reason = (uint16_t)IDA_REASON_NONE;
        return;
    }
    /* Yer tutucu hiçbir genişleticiyi kabul etmez: engel yoksa durum işlemi kabul etmiyor. */
    ret(r, (neden != (uint16_t)IDA_REASON_NONE) ? neden : (uint16_t)IDA_REASON_BAD_MODE);
}

void ida_fc_adim(ida_fc_t *fc, const ida_fc_baglam_t *b, ida_olay_t *olay)
{
    (void)fc;
    (void)b;
    (void)olay; /* yer tutucu aday üretmez */
}

bool ida_fc_tutarli(const ida_fc_t *fc)
{
    return fc->durum <= (uint8_t)IDA_FCS_FAULT_LATCHED;
}

uint8_t ida_fc_param_grup(const ida_prm_kume_t *p)
{
    (void)p;
    return 0u;
}
