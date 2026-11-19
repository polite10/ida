/* W0-MP3a ölçüm prototipi: dairesel DMA alım konumu (bkz. mp3_halka.h; atılabilir). */
#include "mp3_halka.h"

void mp3_halka_baslat(mp3_halka_t *h, uint32_t n, uint32_t fifo_azami)
{
    h->n = n;
    h->fifo_azami = fifo_azami;
    h->p = 0u;
    h->toplam = 0u;
    h->t_us = 0u;
    h->hazir = 0;
    h->kararsiz = 0u;
    h->belirsiz = 0u;
}

int mp3_halka_guncelle(mp3_halka_t *h, uint32_t b1, uint32_t f, uint32_t b2, uint32_t t_us, uint32_t azami_bayt_s)
{
    if (b1 != b2) { h->kararsiz++; return MP3_HALKA_KARARSIZ; }
    const uint32_t n = h->n;
    int sonuc = MP3_HALKA_TAMAM;
    if (b1 > n || f > h->fifo_azami || f > n) {
        h->belirsiz++;
        return MP3_HALKA_BELIRSIZ;   /* geçersiz yazmaç değeri: referans değişmez */
    }
    const uint32_t p = (uint32_t)((2u * (uint64_t)n - b1 - f) % n);
    if (!h->hazir) {
        h->hazir = 1;
        h->p = p;
        h->toplam = p;   /* başlatma, kanal açıldıktan hemen sonra: ilk turun içindeyiz */
        h->t_us = t_us;
        return MP3_HALKA_TAMAM;
    }
    const uint32_t d = (p + n - h->p) % n;
    const uint64_t gecen = (uint32_t)(t_us - h->t_us);
    const uint64_t olasi = gecen * azami_bayt_s / 1000000u + 1u + h->fifo_azami;
    if (olasi >= n / 2u || d >= n / 2u) {
        h->belirsiz++;
        sonuc = MP3_HALKA_BELIRSIZ;
    }
    h->toplam += d;
    h->p = p;
    h->t_us = t_us;
    return sonuc;
}
