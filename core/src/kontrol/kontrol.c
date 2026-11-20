/*
 * kontrol.c — YER TUTUCU (W2). Sahibi: W3 kontrol işi.
 */
#include "kontrol/kontrol.h"

void ida_kon_kur(ida_kon_t *k)
{
    k->yer_tutucu = 0u;
}

void ida_kon_oturum_basi(ida_kon_t *k)
{
    k->yer_tutucu = 0u;
}

void ida_kon_komut(ida_kon_t *k, const ida_komut_baglam_t *b, const ida_cmd_t *cmd,
                   ida_cmd_result_t *r)
{
    (void)k;
    (void)b;
    (void)cmd;
    r->ack = (uint8_t)IDA_ACK_UNSUPPORTED; /* bu sürümde kontrol yok (A4-K124) */
    r->reason = (uint16_t)IDA_REASON_NONE;
}

void ida_kon_adim(ida_kon_t *k, const ida_core_t *c)
{
    (void)k;
    (void)c;
}

void ida_kon_itki(const ida_kon_t *k, float thr[IDA_PORT_N_THR], bool *cokme)
{
    (void)k;
    for (uint32_t i = 0u; i < IDA_PORT_N_THR; i++) {
        thr[i] = 0.0f;
    }
    *cokme = false;
}

bool ida_kon_tutarli(const ida_kon_t *k)
{
    return k->yer_tutucu == 0u;
}

uint8_t ida_kon_param_grup(const ida_prm_kume_t *p)
{
    (void)p;
    return 0u;
}
