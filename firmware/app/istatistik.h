/*
 * istatistik.h — süre ölçümlerinin sayaçları: son, en kötü, toplam ve 16 kutulu histogram
 * (01 §2.11 "Adım" grubu; W3-kart dilim 1).
 *
 * Donanımdan bağımsızdır (ISO C17; PC'de birim testiyle sınanır). Süreler DWT çevrimi olarak
 * verilir; kutu sınırları mikrosaniyedir ve çevrime IDA_IST_CEVRIM_US ile çevrilir (160 MHz).
 * Bütün alanlar 32 bittir (SWD ile okunan ölçüm bloğuna aynen girer), toplam 64 bittir.
 */
#ifndef IDA_FW_ISTATISTIK_H
#define IDA_FW_ISTATISTIK_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define IDA_IST_KUTU 16u
#define IDA_IST_CEVRIM_US 160u /* 160 MHz'te 1 µs */

/* Kutu üst sınırları (µs, dışlayıcı); son kutu taşmadır (>= 5000 µs). Kutu i: [ust[i-1], ust[i]). */
#define IDA_IST_KUTU_UST_US \
    {10u, 20u, 30u, 50u, 75u, 100u, 150u, 200u, 300u, 500u, 750u, 1000u, 2000u, 3500u, 5000u, 0u}

typedef struct {
    uint32_t sayi;              /* ölçüm sayısı (sarar)         */
    uint32_t son_cevrim;        /* son ölçüm                    */
    uint32_t en_kotu_cevrim;    /* en büyük                     */
    uint32_t en_iyi_cevrim;     /* en küçük (sayi = 0'da 0)     */
    uint64_t toplam_cevrim;     /* toplam (ortalama için)       */
    uint32_t kutu[IDA_IST_KUTU];
} ida_ist_t;

void ida_ist_sifirla(ida_ist_t *s);
void ida_ist_ekle(ida_ist_t *s, uint32_t cevrim);
/* Bir sürenin kutu indisi (0..15). */
uint32_t ida_ist_kutu(uint32_t cevrim);

#ifdef __cplusplus
}
#endif

#endif /* IDA_FW_ISTATISTIK_H */
