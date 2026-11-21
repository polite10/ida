/*
 * sonar.h — kart içi sonar algılaması ve arka plan işi (A4-K40 tüketimi, K92–K94, K110).
 * Sahibi: W4 sonar işi.
 *
 * Bu sürüm yer tutucudur: Ping360 satırı (kapıdan geçse de) kullanılmaz, arka plan işi
 * yoktur (ida_core_background IDA_BG_IDLE döner), det_tx boştur, bg raporu sıfırdır.
 */
#ifndef IDA_CORE_SONAR_H
#define IDA_CORE_SONAR_H

#include <stdbool.h>
#include <stdint.h>

#include "ida/port.h"
#include "param/param.h"

typedef struct {
    uint8_t yer_tutucu;
} ida_son_t;

void ida_son_kur(ida_son_t *s);
void ida_son_oturum_basi(ida_son_t *s);
/* Kapıdan geçen Ping360 satırı; kullanılacak içerik çağrı içinde kopyalanır (A4-K40, K99). */
void ida_son_ping360(ida_son_t *s, const ida_ping360_t *p);
/* Yalnız step OK'ta: arka plan sonucunun tüketimi (A4-K94), det_tx ve bg raporu yazımı. */
void ida_son_adim(ida_son_t *s, const ida_core_t *c, ida_out_t *out);
/* Bir arka plan dilimi (A4-K93, K110): iş birimiyle sınırlı, zamandan bağımsız. */
ida_bg_rc_t ida_son_arka_plan(ida_son_t *s);
bool ida_son_tutarli(const ida_son_t *s);
uint8_t ida_son_param_grup(const ida_prm_kume_t *p);

#endif /* IDA_CORE_SONAR_H */
