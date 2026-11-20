/*
 * kestirim.c — YER TUTUCU (W2). Sahibi: W3 kestirim işi.
 */
#include "kestirim/kestirim.h"

#include <string.h>

void ida_kes_kur(ida_kes_t *k)
{
    k->yer_tutucu = 0u;
}

void ida_kes_oturum_basi(ida_kes_t *k)
{
    k->yer_tutucu = 0u;
}

void ida_kes_imu(ida_kes_t *k, const ida_imu_sample_t *imu)
{
    (void)k;
    (void)imu;
}

void ida_kes_ornek(ida_kes_t *k, const ida_sample_t *s)
{
    (void)k;
    (void)s;
}

void ida_kes_adim(ida_kes_t *k, const ida_core_t *c)
{
    (void)k;
    (void)c;
}

void ida_kes_tick(ida_kes_t *k, const ida_core_t *c)
{
    (void)k;
    (void)c;
}

uint8_t ida_kes_duzey(const ida_kes_t *k)
{
    (void)k;
    return (uint8_t)IDA_EST_INIT;
}

void ida_kes_nav(const ida_kes_t *k, ida_stime_t t, ida_nav_out_t *nav)
{
    (void)k;
    (void)memset(nav, 0, sizeof(*nav));
    nav->t = t;
    nav->est_level = (uint8_t)IDA_EST_INIT;
    nav->q_nb[0] = 1.0f; /* A4-K87: INIT'te birim kuaterniyon, diğerleri 0 */
}

bool ida_kes_tutarli(const ida_kes_t *k)
{
    return k->yer_tutucu == 0u;
}

uint8_t ida_kes_param_grup(const ida_prm_kume_t *p)
{
    (void)p;
    return 0u;
}
