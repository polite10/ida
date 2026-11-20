/*
 * zaman.h — ölçüm zamanı, duvar saati ve seri aritmetik yardımcıları (W2; durum yok).
 *
 * İki saat alanı ayrı tiplerdir ve burada da karışmaz (A4-K07): her işlev ya yalnız
 * ida_stime_t ya da yalnız ida_wtime_t alır. Geçerli zaman 0 … IDA_TIME_MAX_NS'tir (A4-K08);
 * iki geçerli zamanın farkı int64_t'de taşmaz.
 * Seri aritmetik iskelet §3.1 "Sıra numaraları"dır: `yeni`, `onceki`'den yenidir ancak ve
 * ancak (yeni − onceki) mod 2^32 ∈ [1, 2^31 − 1]; 0 yineleme, 2^31 ve üstü eski.
 */
#ifndef IDA_CORE_ZAMAN_H
#define IDA_CORE_ZAMAN_H

#include <stdbool.h>
#include <stdint.h>

#include "ida/port.h"

typedef enum {
    IDA_SERI_ESKI = -1, /* fark >= 2^31 */
    IDA_SERI_AYNI = 0,  /* fark 0: yineleme */
    IDA_SERI_YENI = 1   /* fark 1 … 2^31 − 1 */
} ida_seri_t;

static inline ida_seri_t ida_seri_karsilastir(uint32_t yeni, uint32_t onceki)
{
    const uint32_t fark = yeni - onceki; /* mod 2^32 */
    if (fark == 0u) {
        return IDA_SERI_AYNI;
    }
    return (fark < 0x80000000u) ? IDA_SERI_YENI : IDA_SERI_ESKI;
}

static inline bool ida_st_gecerli(ida_stime_t t)
{
    return (t.ns >= 0) && (t.ns <= IDA_TIME_MAX_NS);
}

static inline bool ida_wt_gecerli(ida_wtime_t t)
{
    return (t.ns >= 0) && (t.ns <= IDA_TIME_MAX_NS);
}

/* İki geçerli ölçüm zamanının farkı (sonra − once), ns. */
static inline int64_t ida_st_fark_ns(ida_stime_t sonra, ida_stime_t once)
{
    return sonra.ns - once.ns;
}

/* İki geçerli duvar saatinin farkı (sonra − once), ns. */
static inline int64_t ida_wt_fark_ns(ida_wtime_t sonra, ida_wtime_t once)
{
    return sonra.ns - once.ns;
}

/* ns → µs, int32_t aralığında kırpılmış (günlük argümanlarının "doyar" kuralı; 3.15). */
static inline int32_t ida_ns_us_doyur(int64_t ns)
{
    const int64_t us = ns / 1000;
    if (us > (int64_t)INT32_MAX) {
        return INT32_MAX;
    }
    if (us < (int64_t)INT32_MIN) {
        return INT32_MIN;
    }
    return (int32_t)us;
}

static inline int64_t ida_mutlak_i64(int64_t x)
{
    return (x < 0) ? -x : x; /* girdiler 2^62 ile sınırlı; INT64_MIN oluşmaz */
}

#endif /* IDA_CORE_ZAMAN_H */
