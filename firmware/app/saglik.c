/* saglik.c — ana döngünün donanımdan bağımsız kararları (saglik.h). */
#include "saglik.h"

#include <stddef.h>

void ida_izgara_kur(ida_izgara_t *z, uint32_t g)
{
    z->gorulen = g;
}

int ida_izgara_ilerle(ida_izgara_t *z, uint32_t g, ida_izgara_adim_t *adim)
{
    adim->kacan_200hz = 0u;
    adim->tick = 0u;
    adim->kacan_10ms = 0u;
    if (g == z->gorulen) {
        return 0;
    }
    /* Geçen anlar gorulen+1 … gorulen+fark (mod 2^32). Aralıktaki çift değer sayısı
       (fark + gorulen'in tekliği) / 2'dir; 2^32 çift olduğundan sarmada da doğrudur (g / 2'nin
       31 bitlik sarması burada yoktur; sol bulgu 5). */
    const uint32_t fark = g - z->gorulen;
    if (fark > 1u) {
        adim->kacan_200hz = fark - 1u;
    }
    const uint64_t ciftler = ((uint64_t)fark + (z->gorulen & 1u)) / 2u;
    adim->tick = (uint32_t)ciftler;
    if (adim->tick > 1u) {
        adim->kacan_10ms = adim->tick - 1u;
    }
    z->gorulen = g;
    return 1;
}

void ida_rx_gozetim_kur(ida_rx_gozetim_t *g, uint64_t simdi_us)
{
    g->bekliyordu = 0u;
    g->son_ilerleme_us = simdi_us;
}

void ida_rx_gozetim_guncelle(ida_rx_gozetim_t *g, bool bekleyen, bool ilerledi,
                             uint64_t simdi_us)
{
    /* Süre yalnız gerçek tüketimde ve yeni bir beklemenin başında yenilenir (sol bulgu 3). */
    if (ilerledi || (bekleyen && !g->bekliyordu)) {
        g->son_ilerleme_us = simdi_us;
    }
    g->bekliyordu = (uint8_t)(bekleyen ? 1u : 0u);
}

bool ida_rx_gozetim_tamam(const ida_rx_gozetim_t *g, bool bekleyen, uint64_t simdi_us,
                          uint64_t s_us)
{
    /* Gözetimin henüz görmediği yeni bir bekleme süreyi başlatmamıştır: tutar (kartta gözlendi:
       uzun boşluktan sonra gelen ilk olay, son tüketimden beri geçen süreyle bir kez ret). */
    if (!bekleyen || !g->bekliyordu) {
        return true;
    }
    return simdi_us - g->son_ilerleme_us <= s_us;
}

uint32_t ida_saglik_degerlendir(const ida_saglik_girdi_t *g)
{
    uint32_t ret = 0u;
    if (g->cekirdek_normal) {
        if (!g->core_ok) {
            ret |= IDA_SAGLIK_CORE_OK;
        }
        if (g->simdi_us - g->son_cagri_us > g->s_us) {
            ret |= IDA_SAGLIK_CAGRI;
        }
    }
    if (!g->rx_tamam) {
        ret |= IDA_SAGLIK_RX;
    }
    if (g->alim_durdu) {
        ret |= IDA_SAGLIK_ALIM_DURDU;
    }
    if (g->flash_asim) {
        ret |= IDA_SAGLIK_FLASH;
    }
    return ret;
}

ida_led_t ida_led_durumu(bool saat_tamam, bool baglanti, bool cekirdek_normal)
{
    ida_led_t l;
    l.yesil = saat_tamam && baglanti && cekirdek_normal;
    l.kirmizi = !l.yesil;
    return l;
}

void ida_islem_gozetim_kur(ida_islem_gozetim_t *g)
{
    g->var = 0u;
    g->bas_us = 0u;
    g->en_uzun_us = 0u;
}

void ida_islem_basladi(ida_islem_gozetim_t *g, uint64_t simdi_us)
{
    g->var = 1u;
    g->bas_us = simdi_us;
}

void ida_islem_bitti(ida_islem_gozetim_t *g, uint64_t simdi_us)
{
    if (g->var != 0u && simdi_us >= g->bas_us) {
        const uint64_t s = simdi_us - g->bas_us;
        if (s > g->en_uzun_us) {
            g->en_uzun_us = s > UINT32_MAX ? UINT32_MAX : (uint32_t)s;
        }
    }
    g->var = 0u;
}

bool ida_islem_tamam(const ida_islem_gozetim_t *g, uint64_t simdi_us, uint64_t sinir_us)
{
    if (g->var == 0u || simdi_us < g->bas_us) {
        return true;
    }
    return simdi_us - g->bas_us <= sinir_us;
}

void ida_kararli_kur(ida_kararli_t *k)
{
    k->suruyor = 0u;
    k->bildirildi = 0u;
    k->bas_us = 0u;
}

bool ida_kararli_guncelle(ida_kararli_t *k, bool kosul, uint64_t simdi_us, uint64_t sinir_us)
{
    if (!kosul) {
        k->suruyor = 0u;
        k->bildirildi = 0u;
        return false;
    }
    if (k->suruyor == 0u) {
        k->suruyor = 1u;
        k->bildirildi = 0u;
        k->bas_us = simdi_us;
        return false;
    }
    if (k->bildirildi == 0u && simdi_us >= k->bas_us && simdi_us - k->bas_us >= sinir_us) {
        k->bildirildi = 1u;
        return true;
    }
    return false;
}

size_t ida_alim_turu(uint8_t *tampon, size_t b_ayr, ida_tur_oku_f oku, ida_tur_isle_f isle,
                     void *ctx)
{
    size_t bu_tur = 0u;
    while (bu_tur < b_ayr) {
        const size_t n = oku(ctx, tampon, b_ayr - bu_tur);
        if (n == 0u || n > b_ayr - bu_tur) {
            break;
        }
        isle(ctx, tampon, n);
        bu_tur += n;
    }
    return bu_tur;
}
