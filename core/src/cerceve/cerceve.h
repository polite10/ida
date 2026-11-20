/*
 * cerceve.h — çağrı çerçevesinin durumu ve iç işlevleri (W2; A4 §3.11, A4-K10, K25, K109,
 * K113, K117).
 *
 * Port işlevleri (ida_core_info, init, step, tick, background) cerceve.c'dedir; step ve tick
 * ortak gövde ida_cer_cagri'yı çağırır. Değerlendirme sırası 3.11'dekidir:
 * (a1) c/in NULL, (a2) kurulmamış, (b) daraltma, (b2) out NULL, (b3) SESSION_START,
 * (c) yapısal doğrulama, (d) sıra, (e) olağan işleniş. Modül kancalarının sırası
 * core/TASARIM.md §1.4'tedir.
 */
#ifndef IDA_CORE_CERCEVE_H
#define IDA_CORE_CERCEVE_H

#include <stdbool.h>
#include <stdint.h>

#include "ida/port.h"
#include "param/param.h"

typedef struct {
    int64_t now_izi_ns;      /* kullanılan son now_w (A4-K10); init'te init'in now_w'si   */
    bool tf_var;             /* son kabul edilen zaman çerçevesi var                       */
    uint32_t tf_seq;
    int64_t tf_t_ns;
    bool yeniden_baslat;     /* (b3) sonrası modüllerin yeniden başlaması bekliyor (A4-K72) */
    uint32_t bg_dilim;       /* önceki step/tick'ten beri IDLE/MORE dönen arka plan çağrısı */
    uint32_t bg_kotu;        /* önceki step/tick'ten beri IDA_BG_BAD_INPUT dönen çağrı      */
    bool plat_taban_var;     /* step_miss tabanı (3.8)                                     */
    uint32_t plat_skipped;
    uint32_t plat_merged;
    uint32_t dedup[IDA_PORT_CMD_DEDUP]; /* A4-K48 yineleme penceresi (halka)               */
    uint8_t dedup_n;
    uint8_t dedup_i;
    uint32_t boot_count;     /* status.boot_count                                          */
    bool fc_onceki_var;      /* FC_STATE / FC_INHIBIT günlüklerinin tabanı                 */
    uint8_t fc_onceki_durum;
    uint32_t fc_onceki_engel;
    bool core_ok;            /* son değerlendirilen iç tutarlılık (CORE_INTEGRITY geçişi)  */
    ida_core_counters_t cnt; /* status.cnt                                                 */
} ida_cerceve_t;

/* Bir step/tick çağrısının geçici durumu (çağıranın yığınında). */
typedef struct {
    uint8_t src;             /* IDA_SRC_*                                                  */
    bool step;
    ida_rc_t rc;
    ida_wtime_t now;         /* kullanılan now_w                                           */
    bool imu_kabul;          /* step'in IMU'su kabul edildi                                */
    bool imu_red;
    bool imu_bayat;
    bool imu_lag;
    bool cakisma;            /* health.sample_conflict                                     */
    bool bg_bad_input;       /* health.bg_bad_input                                        */
    bool core_ok;
} ida_cagri_t;

/* Örnek kurulu mu (kurulu işareti ve tümleyeni; A4-K126 için örneğin başka belleği okunmaz). */
bool ida_cer_kurulu(const ida_core_t *c);
/* Çerçeve parametrelerinin grup denetimi: IMU_LAG_CLR_NS ≤ IMU_LAG_SET_NS (A4-K37). */
uint8_t ida_cer_param_grup(const ida_prm_kume_t *k);
/* step ve tick'in ortak gövdesi. */
ida_rc_t ida_cer_cagri(ida_core_t *c, const ida_in_t *in, ida_out_t *out, uint8_t src);

#endif /* IDA_CORE_CERCEVE_H */
