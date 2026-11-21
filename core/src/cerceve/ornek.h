/*
 * ornek.h — örnek kabul kapısı (W2; A4-K14, K30, K33, K34, K35, K40 doğrulaması, K103'ün IMU
 * denetimi, K115, K130; 3.2 window_ns kuralı).
 *
 * Kapı port düzeyindedir ve algoritmadan bağımsızdır: yalnız biçim, sonluluk, zaman ve sıra
 * koşullarını denetler; fiziksel aralık denetimi tüketicinin (W3) işidir (3.3). Her
 * (type, inst) için son kabul edilen seq, t_meas ve t_rx izini tutar; init'te ve
 * SESSION_START'ta (A4-K72, (b3)) sıfırlanır. Kabul edilen örnekler A4-K35'in sabit sırasıyla
 * (t_meas, type, inst, seq) döner.
 */
#ifndef IDA_CORE_ORNEK_H
#define IDA_CORE_ORNEK_H

#include <stdbool.h>
#include <stdint.h>

#include "ida/port.h"
#include "telemetri/telemetri.h"

#define IDA_ORN_TUR_SAYISI 11u /* IDA_SENS_* 1–10; dizin 0 kullanılmaz */

typedef struct {
    bool var;
    uint32_t seq;
    int64_t t_meas_ns;
    int64_t t_rx_ns;  /* tazelik izi (A4-K36): eşit t_meas'lı SAFETY kabulünde yenilenmez */
} ida_orn_iz_t;

typedef struct {
    ida_orn_iz_t iz[IDA_ORN_TUR_SAYISI][IDA_PORT_MAX_INST];
} ida_ornek_t;

typedef struct {
    ida_wtime_t now;               /* kullanılan now_w (A4-K10)               */
    const ida_time_frame_t *tf;
    ida_core_counters_t *cnt;
    ida_gunluk_t *gun;
} ida_orn_baglam_t;

typedef struct {
    uint8_t n;                          /* kabul edilen örnek sayısı                    */
    uint8_t sira[IDA_PORT_MAX_SAMPLES]; /* kabul edilenlerin samples[] indisleri, işlem sırası */
    bool cakisma;                       /* health.sample_conflict (A4-K115)             */
} ida_orn_sonuc_t;

void ida_orn_kur(ida_ornek_t *o);
/* step'in IMU örneği (A4-K30, K33): kabul edilirse true; ret sayaçları (sample_rejected ya da
   sample_future) yazılır, imu_rejected'i çağıran sayar. */
bool ida_orn_imu(ida_ornek_t *o, const ida_imu_sample_t *imu, const ida_orn_baglam_t *b);
/* samples[] (A4-K30, K33, K34, K35, K115, K130). */
void ida_orn_degerlendir(ida_ornek_t *o, const ida_in_t *in, const ida_orn_baglam_t *b,
                         ida_orn_sonuc_t *s);
/* Ping360 satırının doğrulaması (A4-K40): ret ping360_rejected'e sayılır. */
bool ida_orn_ping360(ida_ornek_t *o, const ida_ping360_t *p, const ida_orn_baglam_t *b);

#endif /* IDA_CORE_ORNEK_H */
