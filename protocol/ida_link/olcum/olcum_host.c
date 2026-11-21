/*
 * olcum_host.c — ida_link'in PC'deki ayrıştırma ve kodlama süresi (W2-protokol §5) ve
 * W3-tel-hiz öncesi/sonrası karşılaştırması.
 *
 * Kullanım: ida_link_olcum [yineleme]   (varsayılan 20000)
 *
 * Her satır iki uygulamayı ölçer: "eski" = W3-tel-hiz öncesi tel katmanının değiştirilmemiş
 * kopyası (tel_eski.c), "yeni" = güncel ida_link. İkisi aynı programda, aynı girdiyle,
 * sırayla (eski, yeni, eski, yeni, ...) 5'er kez koşar; her birinin en kısası yazılır.
 * Ayrıca iki uygulamanın aynı özeti (teslim edilen çerçeveler; kodlama çıktısı) verdiği
 * denetlenir; farklıysa program başarısız döner.
 *
 * Süre ISO C11 timespec_get (TIME_UTC) ile ölçülür. Sonuç yalnız bu makinenin ölçümüdür:
 * kart maliyeti (A1 §8.1 O2, MP5) bu rakamlardan çıkarılamaz. Ayrıştırma bir örneği sürekli
 * besler (seq boşlukları sayılır, davranış değişmez).
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

typedef struct {
    const char *ad;
    int kodlama;                 /* 0: ayrıştırma, 1: kodlama */
    ida_link_olcum_akis_t akis;
    size_t parca;                /* ayrıştırma: 0 tek seferde, 1 bayt bayt */
    int coz;
} olcum_satiri_t;

static const olcum_satiri_t SATIRLAR[] = {
    {"pencere ayr., tek seferde", 0, IDA_LINK_OLCUM_PENCERE, 0u, 0},
    {"pencere ayr., 64 B parça", 0, IDA_LINK_OLCUM_PENCERE, 64u, 0},
    {"pencere ayr., bayt bayt", 0, IDA_LINK_OLCUM_PENCERE, 1u, 0},
    {"pencere ayr. + çözme", 0, IDA_LINK_OLCUM_PENCERE, 0u, 1},
    {"pencere kodlama", 1, IDA_LINK_OLCUM_PENCERE, 0u, 0},
    {"258 B ayr., tek seferde", 0, IDA_LINK_OLCUM_BUYUK, 0u, 0},
    {"258 B ayr., bayt bayt", 0, IDA_LINK_OLCUM_BUYUK, 1u, 0},
    {"258 B kodlama", 1, IDA_LINK_OLCUM_BUYUK, 0u, 0},
};

#define SATIR_SAYISI (sizeof SATIRLAR / sizeof SATIRLAR[0])
#define TUR_SAYISI 5

/* Bir koşu: süre (ns/yineleme) ve özet. Kodlamada gönderici seq'i her koşudan önce aynı
   değere kurulur; iki uygulamanın çıktısı böylece karşılaştırılabilir. */
static double kos(const olcum_satiri_t *s, ida_link_olcum_uygulama_t u, ida_link_olcum_t *o,
                  uint32_t yineleme, uint32_t *ozet)
{
    o->gonderici.seq = 0u;
    const double t0 = simdi_ns();
    const uint32_t r = s->kodlama ? ida_link_olcum_kodla(o, u, s->akis, yineleme)
                                  : ida_link_olcum_ayristir(o, u, s->akis, s->parca, s->coz,
                                                            yineleme);
    const double t1 = simdi_ns();
    *ozet = r;
    return (t1 - t0) / (double)yineleme;
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
    printf("%-28s %12s %12s %9s %10s %10s\n", "ölçüm", "eski ns/yin.", "yeni ns/yin.",
           "yeni/eski", "yeni ns/B", "özet");
    int hata = 0;
    for (size_t k = 0; k < SATIR_SAYISI; ++k) {
        const olcum_satiri_t *s = &SATIRLAR[k];
        double en_kisa[2] = {0.0, 0.0};
        uint32_t ozet[2] = {0u, 0u};
        for (int tur = 0; tur < TUR_SAYISI; ++tur) {
            for (int u = 0; u < 2; ++u) {
                uint32_t r = 0u;
                const double sure = kos(s, u == 0 ? IDA_LINK_OLCUM_ESKI : IDA_LINK_OLCUM_YENI,
                                        &o, yineleme, &r);
                if (tur == 0 || sure < en_kisa[u]) {
                    en_kisa[u] = sure;
                }
                if (tur == 0) {
                    ozet[u] = r;
                } else if (r != ozet[u]) {
                    fprintf(stderr, "%s: özet koşudan koşuya değişti\n", s->ad);
                    hata = 1;
                }
            }
        }
        if (!s->kodlama && (ozet[0] == 0u || ozet[1] == 0u)) {
            fprintf(stderr, "%s: teslim edilen çerçeve sayısı beklenen değil\n", s->ad);
            hata = 1;
        }
        if (ozet[0] != ozet[1]) {
            fprintf(stderr, "%s: eski ve yeni uygulamanın özeti farklı (%08x / %08x)\n", s->ad,
                    (unsigned)ozet[0], (unsigned)ozet[1]);
            hata = 1;
        }
        const double bayt = s->akis == IDA_LINK_OLCUM_BUYUK ? (double)o.buyuk_bayt
                                                             : (double)o.pencere_bayt;
        printf("%-28s %12.1f %12.1f %9.3f %10.2f   %08x\n", s->ad, en_kisa[0], en_kisa[1],
               en_kisa[0] > 0.0 ? en_kisa[1] / en_kisa[0] : 0.0, en_kisa[1] / bayt,
               (unsigned)ozet[1]);
    }
    printf("alıcı sayaçları: rx_frames %u, rx_crc_err %u, rx_resync_bytes %u\n",
           (unsigned)o.alici.sayac.rx_frames, (unsigned)o.alici.sayac.rx_crc_err,
           (unsigned)o.alici.sayac.rx_resync_bytes);
    return hata || o.alici.sayac.rx_crc_err != 0u || o.alici.sayac.rx_resync_bytes != 0u;
}
