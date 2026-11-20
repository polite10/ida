/*
 * sonar.c — YER TUTUCU (W2). Sahibi: W4 sonar işi.
 */
#include "sonar/sonar.h"

void ida_son_kur(ida_son_t *s)
{
    s->yer_tutucu = 0u;
}

void ida_son_oturum_basi(ida_son_t *s)
{
    s->yer_tutucu = 0u;
}

void ida_son_ping360(ida_son_t *s, const ida_ping360_t *p)
{
    (void)s;
    (void)p;
}

void ida_son_adim(ida_son_t *s, const ida_core_t *c, ida_out_t *out)
{
    (void)s;
    (void)c;
    out->n_det_tx = 0u;
    out->bg.submitted = false;
    out->bg.dropped = 0u;
}

ida_bg_rc_t ida_son_arka_plan(ida_son_t *s)
{
    (void)s;
    return IDA_BG_IDLE;
}

bool ida_son_tutarli(const ida_son_t *s)
{
    return s->yer_tutucu == 0u;
}

uint8_t ida_son_param_grup(const ida_prm_kume_t *p)
{
    (void)p;
    return 0u;
}
