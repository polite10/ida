/* istatistik.c — süre sayaçları ve histogram (istatistik.h). */
#include "istatistik.h"

#include <string.h>

static const uint32_t KUTU_UST_US[IDA_IST_KUTU] = IDA_IST_KUTU_UST_US;

void ida_ist_sifirla(ida_ist_t *s)
{
    memset(s, 0, sizeof *s);
}

uint32_t ida_ist_kutu(uint32_t cevrim)
{
    for (uint32_t i = 0u; i + 1u < IDA_IST_KUTU; i++) {
        /* cevrim < ust * 160 ⇔ süre < ust µs; çarpım 64 bitte (taşma yok). */
        if ((uint64_t)cevrim < (uint64_t)KUTU_UST_US[i] * IDA_IST_CEVRIM_US) {
            return i;
        }
    }
    return IDA_IST_KUTU - 1u;
}

void ida_ist_ekle(ida_ist_t *s, uint32_t cevrim)
{
    if (s->sayi == 0u || cevrim < s->en_iyi_cevrim) {
        s->en_iyi_cevrim = cevrim;
    }
    if (cevrim > s->en_kotu_cevrim) {
        s->en_kotu_cevrim = cevrim;
    }
    s->son_cevrim = cevrim;
    s->toplam_cevrim += cevrim;
    s->sayi++;
    s->kutu[ida_ist_kutu(cevrim)]++;
}
