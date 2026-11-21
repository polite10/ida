/*
 * kontrol.h — kontrol döngüleri ve itki tahsisi. Sahibi: W3 kontrol işi.
 *
 * Bu sürüm yer tutucudur: itki önerisi sekiz 0.0f'tır; MANUAL bu sürümde uygulanmaz
 * (UNSUPPORTED). Çıkış kapısı öneriyi yalnız armed = 1 ve is_safe = 0 iken kullanır
 * (A4-K55); yer tutucu çekirdekte armed hiçbir yolla 1 olmaz.
 */
#ifndef IDA_CORE_KONTROL_H
#define IDA_CORE_KONTROL_H

#include <stdbool.h>
#include <stdint.h>

#include "cerceve/komut.h"
#include "ida/port.h"
#include "param/param.h"

typedef struct {
    uint8_t yer_tutucu;
} ida_kon_t;

void ida_kon_kur(ida_kon_t *k);
void ida_kon_oturum_basi(ida_kon_t *k);
/* MANUAL (tablo 3.17; A4-K49). */
void ida_kon_komut(ida_kon_t *k, const ida_komut_baglam_t *b, const ida_cmd_t *cmd,
                   ida_cmd_result_t *r);
/* Yalnız step OK'ta. */
void ida_kon_adim(ida_kon_t *k, const ida_core_t *c);
/* Son adımın itki önerisi (A5 itici sırası, [−1, 1]); sayısal çökmede *cokme = true. */
void ida_kon_itki(const ida_kon_t *k, float thr[IDA_PORT_N_THR], bool *cokme);
bool ida_kon_tutarli(const ida_kon_t *k);
uint8_t ida_kon_param_grup(const ida_prm_kume_t *p);

#endif /* IDA_CORE_KONTROL_H */
