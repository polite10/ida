/*
 * gorev.h — görev yönetimi, kip yöneticisi (genişletme yönü) ve arıza güvenliği.
 * Sahibi: W3 güdüm ve görev işi.
 *
 * Bu sürüm yer tutucudur. Görünen davranışı (tablo 3.17'ye göre):
 *   ARM (çerçevenin EVENTS_OFF/PARAM_INVALID/PARAM_BUSY denetiminden sonra): kestirim düzeyi
 *     OK değil → REJECTED / NOT_ARMABLE (silahlandırma ön koşulu tutmuyor).
 *   SET_MODE (GUIDED/AUTO): hedef kipin ön koşulu tutmuyor → REJECTED / BAD_MODE.
 *   HEARTBEAT, FAILSAFE_RESET, MISSION_*: bu sürümde uygulanmaz → UNSUPPORTED / NONE.
 *   Arıza eylemi her zaman IDA_FS_NONE, nedenler 0; görev IDA_MS_IDLE.
 * `yetki` (emniyet modülünün armed/mode alanları) genişletme yönünde yalnız bu modülce
 * yazılır; yer tutucu yazmaz.
 */
#ifndef IDA_CORE_GOREV_H
#define IDA_CORE_GOREV_H

#include <stdbool.h>
#include <stdint.h>

#include "cerceve/komut.h"
#include "emniyet/emniyet.h"
#include "ida/port.h"
#include "param/param.h"

typedef struct {
    uint8_t yer_tutucu;
} ida_gor_t;

void ida_gor_kur(ida_gor_t *g);
void ida_gor_oturum_basi(ida_gor_t *g);
/* Kabul edilen BATT ve LEAK örnekleri (A4-K76). */
void ida_gor_ornek(ida_gor_t *g, const ida_sample_t *s);
/* Kip yöneticisi ve arıza değerlendirmesi; her kabul edilen çağrıda. */
void ida_gor_adim(ida_gor_t *g, ida_emn_t *yetki, const ida_core_t *c);
/* HEARTBEAT, ARM, SET_MODE (GUIDED/AUTO), FAILSAFE_RESET, MISSION_* (tablo 3.17). */
void ida_gor_komut(ida_gor_t *g, ida_emn_t *yetki, const ida_komut_baglam_t *b,
                   const ida_cmd_t *cmd, ida_cmd_result_t *r);
uint8_t ida_gor_fs_duzey(const ida_gor_t *g);   /* IDA_FS_* */
uint32_t ida_gor_fs_nedenleri(const ida_gor_t *g); /* IDA_FSR_* (IMU_LAG çerçevece eklenir) */
/* status'un görev alanları: regime, phase, mission_state, mission_item. */
void ida_gor_durum(const ida_gor_t *g, ida_status_t *st);
bool ida_gor_tutarli(const ida_gor_t *g);
uint8_t ida_gor_param_grup(const ida_prm_kume_t *p);

#endif /* IDA_CORE_GOREV_H */
