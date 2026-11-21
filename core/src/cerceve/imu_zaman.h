/*
 * imu_zaman.h — IMU zamanlaması: boşluk (A4-K38), gecikme mandalı (A4-K37), bayatlık
 * (A4-K39, K128) (W2). Port düzeyindedir ve kestirim algoritmasından bağımsızdır; kestirimin
 * boşluktaki davranışı W3'ündür.
 */
#ifndef IDA_CORE_IMU_ZAMAN_H
#define IDA_CORE_IMU_ZAMAN_H

#include <stdbool.h>
#include <stdint.h>

#include "ida/port.h"
#include "param/param.h"
#include "telemetri/telemetri.h"

typedef struct {
    bool var;            /* init ya da SESSION_START'tan beri kabul edilmiş IMU var  */
    int64_t t_meas_ns;   /* son kabul edilen IMU'nun ölçüm zamanı                    */
    int64_t t_rx_ns;     /* ve alım damgası (A4-K14)                                */
    bool lag;            /* IMU_LAG mandalı (A4-K37)                                */
    uint32_t temiz;      /* art arda lag ≤ T_lag_clear olan zaman çerçeveli çağrı    */
    bool bayat_izi;      /* son kabul edilen çağrıda IMU bayat mıydı (IMU_STALE günlüğü) */
} ida_imz_t;

typedef struct {
    ida_wtime_t now;          /* kullanılan now_w */
    const ida_param_t *prm;
    ida_gunluk_t *gun;
    ida_core_counters_t *cnt;
} ida_imz_baglam_t;

/* Init: IMU yok (bayat), mandal çözük (A4-K128). */
void ida_imz_kur(ida_imz_t *z);
/* SESSION_START (3.11 (b3) (ii)): IMU izi sıfırlanır, IMU bayat sayılır (taze idiyse
   IMU_STALE = 1 günlüğü), IMU_LAG çözülme sayacı sıfırlanır; mandal çözülmez (A4-K37). */
void ida_imz_oturum_basi(ida_imz_t *z, ida_gunluk_t *gun, ida_wtime_t now);
/* Kabul edilen IMU: A4-K38 boşluk denetimi ve iz güncellemesi. */
void ida_imz_imu_kabul(ida_imz_t *z, const ida_imu_sample_t *imu, const ida_imz_baglam_t *b);
/* tf.present = 1 olan kabul edilen çağrıda A4-K37 mandalı. */
void ida_imz_lag_guncelle(ida_imz_t *z, const ida_time_frame_t *tf, const ida_imz_baglam_t *b);
/* A4-K39: now − t_rx(son IMU) > T_imu_stale (ms parametresi, ns'ye int64_t'de çevrilir) ya
   da hiç IMU yoksa bayat. */
bool ida_imz_bayat(const ida_imz_t *z, ida_wtime_t now, const ida_param_t *prm);
/* Kabul edilen çağrıda bayatlık geçişlerini IMU_STALE günlüğüne yazar. */
void ida_imz_bayat_izle(ida_imz_t *z, bool bayat, const ida_imz_baglam_t *b);

#endif /* IDA_CORE_IMU_ZAMAN_H */
