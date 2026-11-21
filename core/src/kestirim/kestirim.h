/*
 * kestirim.h — durum kestirimi (seyrüsefer). Sahibi: W3 kestirim işi.
 *
 * Bu sürüm yer tutucudur: kestirim düzeyi her zaman IDA_EST_INIT'tir ve nav yalnız A4-K87'nin
 * INIT içeriğini taşır (t, est_level, birim kuaterniyon; diğer alanlar 0). Çağrı çerçevesi
 * bu kancaları sabit sırayla çağırır (core/TASARIM.md §1.4); W3 yalnız gövdeleri ve kendi
 * durum yapısını doldurur.
 */
#ifndef IDA_CORE_KESTIRIM_H
#define IDA_CORE_KESTIRIM_H

#include <stdbool.h>
#include <stdint.h>

#include "ida/port.h"
#include "param/param.h"

typedef struct {
    uint8_t yer_tutucu; /* C17 boş yapıya izin vermez */
} ida_kes_t;

void ida_kes_kur(ida_kes_t *k);
/* SESSION_START'tan sonraki ilk kabul edilen çağrının (e) adımında (A4-K72). */
void ida_kes_oturum_basi(ida_kes_t *k);
/* Kabul edilen IMU (yalnız step OK). */
void ida_kes_imu(ida_kes_t *k, const ida_imu_sample_t *imu);
/* Kabul edilen MAG, BARO, GNSS, DVL, PING1D örnekleri, A4-K35 sırasıyla. */
void ida_kes_ornek(ida_kes_t *k, const ida_sample_t *s);
/* step OK'ta (IMU ile yayılım) ve tick/IMU_REJECTED'de (yayılımsız) çağrılır. */
void ida_kes_adim(ida_kes_t *k, const ida_core_t *c);
void ida_kes_tick(ida_kes_t *k, const ida_core_t *c);
uint8_t ida_kes_duzey(const ida_kes_t *k); /* IDA_EST_* */
/* step OK'taki nav (A4-K87); t = çıktının t_ref'i. */
void ida_kes_nav(const ida_kes_t *k, ida_stime_t t, ida_nav_out_t *nav);
bool ida_kes_tutarli(const ida_kes_t *k);
uint8_t ida_kes_param_grup(const ida_prm_kume_t *p);

#endif /* IDA_CORE_KESTIRIM_H */
