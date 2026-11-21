/*
 * gudum.h — güdüm (setpoint üretimi). Sahibi: W3 güdüm ve görev işi.
 *
 * Bu sürüm yer tutucudur: GUIDED_SP bu çekirdek sürümünce uygulanmaz ve
 * IDA_ACK_UNSUPPORTED / IDA_REASON_NONE alır (A4-K124).
 */
#ifndef IDA_CORE_GUDUM_H
#define IDA_CORE_GUDUM_H

#include <stdbool.h>
#include <stdint.h>

#include "cerceve/komut.h"
#include "ida/port.h"
#include "param/param.h"

typedef struct {
    uint8_t yer_tutucu;
} ida_gud_t;

void ida_gud_kur(ida_gud_t *g);
void ida_gud_oturum_basi(ida_gud_t *g);
/* GUIDED_SP (tablo 3.17; A4-K50). */
void ida_gud_komut(ida_gud_t *g, const ida_komut_baglam_t *b, const ida_cmd_t *cmd,
                   ida_cmd_result_t *r);
void ida_gud_adim(ida_gud_t *g, const ida_core_t *c);
bool ida_gud_tutarli(const ida_gud_t *g);
uint8_t ida_gud_param_grup(const ida_prm_kume_t *p);

#endif /* IDA_CORE_GUDUM_H */
