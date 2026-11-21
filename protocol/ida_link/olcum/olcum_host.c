/*
 * olcum_host.c — ida_link'in PC'deki ayrıştırma ve kodlama süresi (W2-protokol §5).
 *
 * Kullanım: ida_link_olcum [yineleme]   (varsayılan 20000)
 *
 * Süre ISO C11 timespec_get (TIME_UTC) ile ölçülür; her ölçüm 5 kez yinelenir, en kısası
 * yazılır. Sonuç yalnız bu makinenin ölçümüdür: kart maliyeti (A1 §8.1 O2, MP5) bu
 * rakamlardan çıkarılamaz. Ayrıştırma bir örneği sürekli besler (seq boşlukları sayılır,
 * davranış değişmez).
 */
#include "olcum_cekirdek.h"

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

static double simdi_ns(void)
{
    struct timespec t;
    if (timespec_get(&t, TIME_UTC) != TIME_UTC) {
        return 0.0;
    }
    return (double)t.tv_sec * 1e9 + (double)t.tv_nsec;
}

typedef uint32_t (*olcum_islevi_t)(ida_link_olcum_t *, uint32_t);

static int olc(const char *ad, olcum_islevi_t f, int ayristirma, ida_link_olcum_t *o,
               uint32_t yineleme, double birim_bayt, double birim_cerceve)
{
    double en_kisa = 0.0;
    uint32_t ozet = 0;
    for (int tur = 0; tur < 5; ++tur) {
        const double t0 = simdi_ns();
        const uint32_t r = f(o, yineleme);
        const double t1 = simdi_ns();
        if (ayristirma && r == 0u) {
            fprintf(stderr, "%s: teslim edilen çerçeve sayısı beklenen değil\n", ad);
            return 1;
        }
        ozet ^= r;
        const double sure = (t1 - t0) / (double)yineleme;
        if (tur == 0 || sure < en_kisa) {
            en_kisa = sure;
        }
    }
    printf("%-26s %10.1f ns/yineleme  %8.2f ns/B  %8.1f ns/çerçeve  (özet %08x)\n", ad,
           en_kisa, en_kisa / birim_bayt, en_kisa / birim_cerceve, (unsigned)ozet);
    return 0;
}

int main(int argc, char **argv)
{
    uint32_t yineleme = 20000u;
    if (argc > 1) {
        const long v = strtol(argv[1], NULL, 10);
        if (v < 1 || v > 100000000L) {
            fprintf(stderr, "yineleme 1..100000000 olmalı\n");
            return 2;
        }
        yineleme = (uint32_t)v;
    }
    static ida_link_olcum_t o;
    const int rc = ida_link_olcum_hazirla(&o);
    if (rc != 0) {
        fprintf(stderr, "ölçüm kurulamadı (%d)\n", rc);
        return 1;
    }
    printf("ida_link host ölçümü: yineleme %u, pencere %zu B / %u çerçeve, en büyük çerçeve %zu B\n",
           (unsigned)yineleme, o.pencere_bayt, (unsigned)IDA_LINK_OLCUM_CERCEVE_SAYISI,
           o.buyuk_bayt);
    int hata = 0;
    hata |= olc("pencere ayrıştırma", ida_link_olcum_pencere_ayristir, 1, &o, yineleme,
                (double)o.pencere_bayt, (double)IDA_LINK_OLCUM_CERCEVE_SAYISI);
    hata |= olc("pencere kodlama", ida_link_olcum_pencere_kodla, 0, &o, yineleme,
                (double)o.pencere_bayt, (double)IDA_LINK_OLCUM_CERCEVE_SAYISI);
    hata |= olc("en büyük çerçeve ayrıştırma", ida_link_olcum_buyuk_ayristir, 1, &o, yineleme,
                (double)o.buyuk_bayt, 1.0);
    hata |= olc("en büyük çerçeve kodlama", ida_link_olcum_buyuk_kodla, 0, &o, yineleme,
                (double)o.buyuk_bayt, 1.0);
    printf("alıcı sayaçları: rx_frames %u, rx_crc_err %u, rx_resync_bytes %u\n",
           (unsigned)o.alici.sayac.rx_frames, (unsigned)o.alici.sayac.rx_crc_err,
           (unsigned)o.alici.sayac.rx_resync_bytes);
    return hata || o.alici.sayac.rx_crc_err != 0u || o.alici.sayac.rx_resync_bytes != 0u;
}
