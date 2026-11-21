/*
 * alim.c — dairesel DMA alım halkasının konum muhasebesi (alim.h).
 * Donanımdan bağımsız, ISO C17; bellek ayırma yok.
 */
#include "alim.h"

#include <string.h>

/* Küçük geri gidiş bandının üst sınırı (bayt); ayrıca n/8 ile sınırlı (alim.h, denetim 2). */
#define IDA_ALIM_GERI_AZAMI 64u

/* Geçen süre bu değeri aşarsa (µs) tur sayısı hesaplanmadan belirsiz sayılır (çarpım taşmasın). */
#define IDA_ALIM_GECEN_AZAMI_US UINT64_C(1000000000)

#define KONUM_GECERSIZ 0
#define KONUM_TAMAM 1
#define KONUM_BELIRSIZ 2

static int iki_kuvveti(uint32_t n)
{
    return n != 0u && (n & (n - 1u)) == 0u;
}

int ida_alim_kur(ida_alim_t *a, uint32_t n, uint32_t fifo_azami, uint32_t azami_bayt_s)
{
    if (a == NULL) {
        return 0;
    }
    memset(a, 0, sizeof *a);
    if (!iki_kuvveti(n) || n < 64u || fifo_azami >= n || azami_bayt_s == 0u) {
        return 0;
    }
    a->n = n;
    a->fifo_azami = fifo_azami;
    a->azami_bayt_s = azami_bayt_s;
    return 1;
}

static int geri_gidis(const ida_alim_t *a, uint32_t d)
{
    const uint32_t bant = a->n / 8u < IDA_ALIM_GERI_AZAMI ? a->n / 8u : IDA_ALIM_GERI_AZAMI;
    return d != 0u && a->n - d <= bant;
}

static void kayip_kaydet(ida_alim_t *a)
{
    a->kayip_konum = a->toplam;
    a->kayip_sayaci = a->kayip_sayaci + 1u;
}

/* Görüntüyü konum muhasebesine uygular (alim.h'deki denetim sırasıyla). */
static int konum_ilerle(ida_alim_t *a, uint32_t b1, uint32_t f, uint32_t b2, uint64_t t_us)
{
    if (b1 != b2) {
        a->kararsiz++;
        return KONUM_GECERSIZ;
    }
    if (b1 > a->n || f > a->fifo_azami) {
        a->gecersiz++;
        return KONUM_GECERSIZ;
    }
    uint32_t p = (uint32_t)((2u * (uint64_t)a->n - b1 - f) % a->n);
    if (!a->hazir) {
        /* Kanal açıldıktan hemen sonraki ilk görüntü: ilk turun içindeyiz. */
        a->hazir = 1u;
        a->toplam = p;
        a->p = p;
        a->t_us = t_us;
        a->son_olay_us = t_us;
        return KONUM_TAMAM;
    }
    int bel = 0;
    /* 1. Süre denetimi (geri gidiş istisnasından önce). */
    const uint64_t gecen = t_us > a->t_us ? t_us - a->t_us : 0u;
    if (gecen > IDA_ALIM_GECEN_AZAMI_US) {
        bel = 1;
    } else {
        const uint64_t olasi = gecen * a->azami_bayt_s / UINT64_C(1000000) + 1u + a->fifo_azami;
        if (olasi >= a->n) {
            bel = 1;
        }
    }
    uint32_t d = (p + a->n - a->p) % a->n;
    if (!bel) {
        if (geri_gidis(a, d)) {
            /* 2. Küçük geri gidiş: konum ve toplam değişmez. */
            a->geri++;
            p = a->p;
            d = 0u;
        } else if (d >= a->n - a->n / 4u) {
            bel = 1; /* 3. */
        }
    }
    a->toplam += d; /* belirsizlikte de: toplam mod n = p korunur */
    a->p = p;
    a->t_us = t_us;
    if (bel) {
        a->belirsiz++;
        kayip_kaydet(a);
        return KONUM_BELIRSIZ;
    }
    return KONUM_TAMAM;
}

static void olay_say(ida_alim_t *a, uint8_t tur)
{
    for (uint32_t i = 0u; i < 5u; i++) {
        if ((tur & (1u << i)) != 0u) {
            a->olay_sayisi[i]++;
        }
    }
}

/* Olay kaydı. Halka doluysa en yeni kayıt kaba olarak uzatılır (damgası değişmez). */
static void kaydet(ida_alim_t *a, uint64_t kesin_konum, uint64_t t_us, uint8_t tur)
{
    const uint32_t bas = a->bas;
    if ((uint32_t)(bas - a->son) >= IDA_ALIM_OLAY_N) {
        ida_alim_olay_t *e = &a->olay[(bas - 1u) & (IDA_ALIM_OLAY_N - 1u)];
        e->konum = a->toplam;
        e->tur = (uint8_t)(e->tur | tur);
        a->olay_birlesme++;
        return;
    }
    ida_alim_olay_t *e = &a->olay[bas & (IDA_ALIM_OLAY_N - 1u)];
    e->konum = a->toplam;
    e->kesin_konum = kesin_konum;
    e->t_us = t_us;
    e->tur = tur;
    a->bas = bas + 1u;
}

int ida_alim_goruntu(ida_alim_t *a, uint32_t b1, uint32_t f, uint32_t b2, uint64_t t_us,
                     uint8_t tur)
{
    if (a == NULL || a->n == 0u) {
        return 0;
    }
    const int ilk = !a->hazir;
    const uint64_t onceki = a->toplam;
    const int r = konum_ilerle(a, b1, f, b2, t_us);
    if (r == KONUM_GECERSIZ) {
        return 0;
    }
    olay_say(a, tur);
    if (r == KONUM_BELIRSIZ || ilk) {
        if (tur != IDA_ALIM_OLAY_YOKLAMA) {
            a->son_olay_us = t_us;
        }
        return 1; /* bekleyenler kayıp konumuna kadar atılacak; olay kaydı gerekmez */
    }
    if (tur != IDA_ALIM_OLAY_YOKLAMA) {
        a->son_olay_us = t_us;
        kaydet(a, a->toplam, t_us, tur);
    } else if (a->toplam > onceki) {
        /* Olaysız bulunan baytlar: önceki son olayın damgası, kaba. */
        kaydet(a, onceki, a->son_olay_us, tur);
    }
    return 1;
}

/* Bildirilmemiş kayıp varsa atar. Dönüş: 1 atıldı (TASMA bildirilecek). */
static int kayip_isle(ida_alim_t *a, uint64_t *tasma_bayt)
{
    const uint32_t k = a->kayip_sayaci;
    if (k == a->kayip_gorulen) {
        return 0;
    }
    a->kayip_gorulen = k;
    uint64_t atilan = 0u;
    if (a->kayip_konum > a->okunan) {
        atilan = a->kayip_konum - a->okunan;
        a->okunan = a->kayip_konum;
    }
    *tasma_bayt = atilan;
    a->tasma++;
    a->tasma_bayt += atilan;
    return 1;
}

ida_alim_durum_t ida_alim_oku(ida_alim_t *a, const uint8_t *halka, uint8_t *hedef, size_t azami,
                              size_t *n, uint64_t *t_us, uint8_t *kaba, uint64_t *tasma_bayt)
{
    *n = 0u;
    *kaba = 0u;
    *tasma_bayt = 0u;
    if (kayip_isle(a, tasma_bayt)) {
        return IDA_ALIM_TASMA;
    }
    for (;;) {
        if (a->son == a->bas) {
            return IDA_ALIM_BOS;
        }
        const ida_alim_olay_t *e = &a->olay[a->son & (IDA_ALIM_OLAY_N - 1u)];
        if (e->konum <= a->okunan) {
            a->son = a->son + 1u; /* yeni bayt kapsamayan (ya da atılmış) olay */
            continue;
        }
        const uint64_t bek = e->konum - a->okunan;
        if (bek > a->halka_azami) {
            a->halka_azami = bek > UINT32_MAX ? UINT32_MAX : (uint32_t)bek;
        }
        if (bek > a->n) {
            /* Okunmamış baytların üzerine yazıldı: bekleyen her şey atılır. */
            a->kayip_konum = e->konum > a->kayip_konum ? e->konum : a->kayip_konum;
            a->kayip_sayaci = a->kayip_sayaci + 1u;
            (void)kayip_isle(a, tasma_bayt);
            return IDA_ALIM_TASMA;
        }
        if (azami == 0u) {
            return IDA_ALIM_BOS;
        }
        uint64_t sinir = e->konum;
        if (a->okunan < e->kesin_konum) {
            sinir = e->kesin_konum;
        } else if (e->kesin_konum < e->konum) {
            *kaba = 1u;
        }
        const uint64_t uzun = sinir - a->okunan;
        const size_t k = uzun < azami ? (size_t)uzun : azami;
        const uint32_t bas = (uint32_t)(a->okunan & (a->n - 1u));
        const size_t k1 = k < (size_t)(a->n - bas) ? k : (size_t)(a->n - bas);
        memcpy(hedef, halka + bas, k1);
        if (k > k1) {
            memcpy(hedef + k1, halka, k - k1);
        }
        *n = k;
        *t_us = e->t_us;
        return IDA_ALIM_VERI;
    }
}

ida_alim_durum_t ida_alim_dogrula(ida_alim_t *a, size_t k, uint32_t b1, uint32_t f, uint32_t b2,
                                  uint64_t t_us, uint64_t *tasma_bayt)
{
    *tasma_bayt = 0u;
    const int r = konum_ilerle(a, b1, f, b2, t_us);
    if (r == KONUM_GECERSIZ) {
        return IDA_ALIM_BOS; /* doğrulanamadı: kopya atılır, sonra yeniden denenir */
    }
    if (r == KONUM_TAMAM && a->toplam > a->okunan && a->toplam - a->okunan > a->n) {
        /* Kopya sürerken kopyalanan baytların (en azından ilkinin) üzerine yazıldı. */
        kayip_kaydet(a);
    }
    if (kayip_isle(a, tasma_bayt)) {
        return IDA_ALIM_TASMA;
    }
    if (k == 0u) {
        return IDA_ALIM_BOS;
    }
    a->okunan += k;
    if (a->son != a->bas) {
        const ida_alim_olay_t *e = &a->olay[a->son & (IDA_ALIM_OLAY_N - 1u)];
        if (e->konum <= a->okunan) {
            a->son = a->son + 1u;
        }
    }
    return IDA_ALIM_VERI;
}

void ida_alim_yeniden_baslat(ida_alim_t *a, uint64_t t_us)
{
    const uint64_t n = a->n;
    a->toplam = (a->toplam + n - 1u) / n * n;
    a->p = 0u;
    a->t_us = t_us;
    a->son_olay_us = t_us;
    a->hazir = 1u;
    a->yeniden++;
    kayip_kaydet(a);
}

int ida_alim_bekleyen(const ida_alim_t *a)
{
    return a->son != a->bas || a->kayip_sayaci != a->kayip_gorulen;
}
