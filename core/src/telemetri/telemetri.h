/*
 * telemetri.h — günlük kayıtlarının biriktirilmesi ve çıktıya yazılması (W2; A4-K90, 3.15).
 *
 * Tek tampon IDA_PORT_MAX_LOGS (4) kayıtlıktır: init'in kayıtları, `out` NULL çağrının
 * (A4-K113 (b2)) ve arka plan çağrılarının kayıtları burada bekler ve ilk yazılabilir step
 * ya da tick çağrısında, o çağrının kayıtlarından önce çıkar. Tampon doluyken eklenen kayıt
 * atılır ve status.cnt.log_dropped'a sayılır (A4-K90). Önem (severity) 3.15'teki kurala göre
 * koddan ve argümanlardan hesaplanır; çağıranın verdiği değer yok sayılır.
 */
#ifndef IDA_CORE_TELEMETRI_H
#define IDA_CORE_TELEMETRI_H

#include <stdint.h>

#include "ida/port.h"

typedef struct {
    ida_log_t kayit[IDA_PORT_MAX_LOGS];
    uint8_t n;
    uint32_t dusen; /* henüz cnt.log_dropped'a eklenmemiş atılan kayıt */
} ida_gunluk_t;

void ida_gun_kur(ida_gunluk_t *g);
/* Kaydı ekler (k->severity yok sayılır, 3.15'e göre yazılır); tampon doluysa atar. */
void ida_gun_ekle(ida_gunluk_t *g, const ida_log_t *k);
/* Tamponu out->logs'a yazar, atılanları cnt->log_dropped'a ekler ve tamponu boşaltır. */
void ida_gun_bosalt(ida_gunluk_t *g, ida_out_t *out, ida_core_counters_t *cnt);
/* 3.15'teki önem kuralı. */
uint8_t ida_gun_onem(const ida_log_t *k);

#endif /* IDA_CORE_TELEMETRI_H */
