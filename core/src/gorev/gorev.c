/*
 * gorev.c — YER TUTUCU (W2). Sahibi: W3 güdüm ve görev işi. Genişletme yönü yoktur.
 */
#include "gorev/gorev.h"

void ida_gor_kur(ida_gor_t *g)
{
    g->yer_tutucu = 0u;
}

void ida_gor_oturum_basi(ida_gor_t *g)
{
    g->yer_tutucu = 0u;
}

void ida_gor_ornek(ida_gor_t *g, const ida_sample_t *s)
{
    (void)g;
    (void)s;
}

void ida_gor_adim(ida_gor_t *g, ida_emn_t *yetki, const ida_core_t *c)
{
    (void)g;
    (void)yetki;
    (void)c;
}

static void ret(ida_cmd_result_t *r, uint16_t neden)
{
    r->ack = (uint8_t)IDA_ACK_REJECTED;
    r->reason = neden;
}

void ida_gor_komut(ida_gor_t *g, ida_emn_t *yetki, const ida_komut_baglam_t *b,
                   const ida_cmd_t *cmd, ida_cmd_result_t *r)
{
    (void)g;
    (void)yetki;
    switch (cmd->type) {
    case IDA_CMD_ARM:
        /* Silahlandırma ön koşulu (kestirim düzeyi) tutmuyor; bu sürümde arm yolu yok. */
        if ((b->est_level == (uint8_t)IDA_EST_OK) && b->armed) {
            r->ack = (uint8_t)IDA_ACK_ACCEPTED; /* zaten silahlı: etkisiz */
            r->reason = (uint16_t)IDA_REASON_NONE;
        } else {
            ret(r, (uint16_t)IDA_REASON_NOT_ARMABLE);
        }
        break;
    case IDA_CMD_SET_MODE:
        /* Yalnız GUIDED/AUTO buraya gelir: hedef kipin ön koşulu (kestirim düzeyi) tutmuyor. */
        ret(r, (uint16_t)IDA_REASON_BAD_MODE);
        break;
    default:
        /* HEARTBEAT, FAILSAFE_RESET, MISSION_*: bu sürümde uygulanmaz (A4-K124). */
        r->ack = (uint8_t)IDA_ACK_UNSUPPORTED;
        r->reason = (uint16_t)IDA_REASON_NONE;
        break;
    }
}

uint8_t ida_gor_fs_duzey(const ida_gor_t *g)
{
    (void)g;
    return (uint8_t)IDA_FS_NONE;
}

uint32_t ida_gor_fs_nedenleri(const ida_gor_t *g)
{
    (void)g;
    return 0u;
}

void ida_gor_durum(const ida_gor_t *g, ida_status_t *st)
{
    (void)g;
    st->regime = 0u; /* INIT'te 0 ve anlamsız (3.8) */
    st->phase = 0u;
    st->mission_state = (uint8_t)IDA_MS_IDLE;
    st->mission_item = 0u;
}

bool ida_gor_tutarli(const ida_gor_t *g)
{
    return g->yer_tutucu == 0u;
}

uint8_t ida_gor_param_grup(const ida_prm_kume_t *p)
{
    (void)p;
    return 0u;
}
