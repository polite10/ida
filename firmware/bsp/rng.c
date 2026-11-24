/* rng.c — RNG okuma ve hata kurtarma (rng.h). */
#include "rng.h"

#include <stdbool.h>

/* SEIS'i temizler (0 yazılır), CEIS'e dokunmaz (1 yazılır; rc_w0). */
static void seis_temizle(const ida_rng_ops_t *o)
{
    o->sr_yaz(o->ctx, IDA_RNG_SR_CEIS);
}

static void ceis_temizle(const ida_rng_ops_t *o)
{
    o->sr_yaz(o->ctx, IDA_RNG_SR_SEIS);
}

/* Tohum hatasından kurtarma (ST RNG_RecoverSeedError sırası). 0 tamam, < 0 kurtarılamaz. */
static int tohum_kurtar(const ida_rng_ops_t *o, ida_rng_sayac_t *s)
{
    if ((o->sr(o->ctx) & IDA_RNG_SR_SECS) == 0u) {
        seis_temizle(o); /* donanım kendiliğinden sıfırladı */
        return 0;
    }
    s->kurtarma++;
    o->cr_yaz(o->ctx, o->cr(o->ctx) | IDA_RNG_CR_CONDRST);
    o->cr_yaz(o->ctx, o->cr(o->ctx) & ~IDA_RNG_CR_CONDRST);
    uint32_t i = 0u;
    while ((o->cr(o->ctx) & IDA_RNG_CR_CONDRST) != 0u) {
        if (++i >= IDA_RNG_YOKLAMA) {
            s->zaman_asimi++;
            return -1;
        }
    }
    if ((o->sr(o->ctx) & IDA_RNG_SR_SEIS) != 0u) {
        seis_temizle(o);
    }
    i = 0u;
    while ((o->sr(o->ctx) & IDA_RNG_SR_SECS) != 0u) {
        if (++i >= IDA_RNG_YOKLAMA) {
            s->zaman_asimi++;
            return -1;
        }
    }
    return 0;
}

/* Saat hatası: CEIS temizlenir; CECS sınırlı süre içinde inmezse kurtarılamaz. */
static int saat_bekle(const ida_rng_ops_t *o, ida_rng_sayac_t *s)
{
    s->saat_hata++;
    ceis_temizle(o);
    uint32_t i = 0u;
    while ((o->sr(o->ctx) & IDA_RNG_SR_CECS) != 0u) {
        if (++i >= IDA_RNG_YOKLAMA) {
            s->zaman_asimi++;
            return -1;
        }
    }
    return 0;
}

int ida_rng_oku(const ida_rng_ops_t *o, ida_rng_sayac_t *s, uint32_t *deger)
{
    for (uint32_t deneme = 0u; deneme < IDA_RNG_DENEME; deneme++) {
        uint32_t sr = o->sr(o->ctx);
        if ((sr & (IDA_RNG_SR_SECS | IDA_RNG_SR_SEIS)) != 0u) {
            s->tohum_hata++;
            if (tohum_kurtar(o, s) != 0) {
                break;
            }
            continue;
        }
        if ((sr & (IDA_RNG_SR_CECS | IDA_RNG_SR_CEIS)) != 0u) {
            if (saat_bekle(o, s) != 0) {
                break;
            }
            continue;
        }
        /* DRDY beklenir; bekleme sırasında tohum hatası çıkarsa tur yinelenir. */
        uint32_t i = 0u;
        bool tohum = false;
        while (((sr = o->sr(o->ctx)) & IDA_RNG_SR_DRDY) == 0u) {
            if ((sr & (IDA_RNG_SR_SECS | IDA_RNG_SR_SEIS | IDA_RNG_SR_CECS)) != 0u) {
                tohum = true;
                break;
            }
            if (++i >= IDA_RNG_YOKLAMA) {
                break;
            }
        }
        if (tohum) {
            continue; /* turun başı hatayı işler */
        }
        if ((sr & IDA_RNG_SR_DRDY) == 0u) {
            s->zaman_asimi++;
            continue;
        }
        const uint32_t v = o->dr(o->ctx);
        /* DR okunduktan sonra da: tohum hatası varsa değer kullanılmaz (ST sürücüsü). */
        if ((o->sr(o->ctx) & (IDA_RNG_SR_SEIS | IDA_RNG_SR_SECS)) != 0u) {
            s->atilan++;
            continue;
        }
        if (v == 0u) {
            s->sifir++;
            continue;
        }
        *deger = v;
        return 0;
    }
    s->kurtarilamaz++;
    return -1;
}
