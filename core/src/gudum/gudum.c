/*
 * gudum.c — YER TUTUCU (W2). Sahibi: W3 güdüm ve görev işi.
 */
#include "gudum/gudum.h"

void ida_gud_kur(ida_gud_t *g)
{
    g->yer_tutucu = 0u;
}

void ida_gud_oturum_basi(ida_gud_t *g)
{
    g->yer_tutucu = 0u;
}

void ida_gud_komut(ida_gud_t *g, const ida_komut_baglam_t *b, const ida_cmd_t *cmd,
                   ida_cmd_result_t *r)
{
    (void)g;
    (void)b;
    (void)cmd;
    r->ack = (uint8_t)IDA_ACK_UNSUPPORTED; /* bu sürümde güdüm yok (A4-K124) */
    r->reason = (uint16_t)IDA_REASON_NONE;
}

void ida_gud_adim(ida_gud_t *g, const ida_core_t *c)
{
    (void)g;
    (void)c;
}

bool ida_gud_tutarli(const ida_gud_t *g)
{
    return g->yer_tutucu == 0u;
}

uint8_t ida_gud_param_grup(const ida_prm_kume_t *p)
{
    (void)p;
    return 0u;
}
