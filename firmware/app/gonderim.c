/* gonderim.c — üç sınıflı gönderim yöneticisi (gonderim.h). */
#include "gonderim.h"

#include <string.h>

static void kuyruk_kur(ida_gon_kuyruk_t *q, ida_gon_yuva_t *y, uint32_t kap)
{
    q->yuva = y;
    q->kap = kap;
    q->bas = 0u;
    q->say = 0u;
    q->azami = 0u;
}

void ida_gon_kur(ida_gonderim_t *g)
{
    memset(g, 0, sizeof *g);
    kuyruk_kur(&g->q[0], g->kritik_y, IDA_GON_KRITIK_N);
    kuyruk_kur(&g->q[1], g->zorunlu_y, IDA_GON_ZORUNLU_N);
    kuyruk_kur(&g->q[2], g->kaybed_y, IDA_GON_KAYBED_N);
}

/* MAVLink v2 çerçevesinin sıra baytı (STX 0xFD, len, incompat, compat, seq). */
static void sira_izle(ida_gon_sira_t *z, const uint8_t *b, size_t n)
{
    if (n < 5u || b[0] != 0xFDu) {
        return;
    }
    const uint8_t s = b[4];
    if (z->var != 0u && s != (uint8_t)(z->son + 1u)) {
        z->kesinti++;
        z->eksik += (uint32_t)(uint8_t)(s - (uint8_t)(z->son + 1u));
    }
    z->son = s;
    z->var = 1u;
}

static ida_gon_yuva_t *on(ida_gon_kuyruk_t *q)
{
    return &q->yuva[q->bas];
}

static void cikar(ida_gon_kuyruk_t *q)
{
    q->bas = (q->bas + 1u) % q->kap;
    q->say--;
}

bool ida_gon_ekle(ida_gonderim_t *g, uint32_t sinif, const uint8_t *b, size_t n, uint64_t t_us)
{
    if (b == NULL || n == 0u || n > IDA_GON_CERCEVE_AZAMI) {
        g->gecersiz_ret++;
        if (sinif == IDA_GON_SINIF_KRITIK) {
            g->kritik_hata = true;
        }
        return false;
    }
    sira_izle(&g->sira_giris, b, n);
    if (sinif < IDA_GON_SINIF_KRITIK || sinif > IDA_GON_SINIF_KAYBED) {
        g->gecersiz_ret++;
        return false;
    }
    ida_gon_kuyruk_t *q = &g->q[sinif - 1u];
    if (q->say == q->kap) {
        if (sinif == IDA_GON_SINIF_KAYBED) {
            cikar(q); /* en eskisi atılır (A1-K110) */
            g->tx_dropped++;
        } else {
            if (sinif == IDA_GON_SINIF_KRITIK) {
                g->kritik_ret++;
                g->kritik_hata = true; /* kritik çıktının kaybı olağan değildir (A1-K110) */
            } else {
                g->zorunlu_ret++;
            }
            return false;
        }
    }
    sira_izle(&g->sira_kuyruk, b, n);
    ida_gon_yuva_t *y = &q->yuva[(q->bas + q->say) % q->kap];
    memcpy(y->b, b, n);
    y->n = (uint16_t)n;
    y->t_us = t_us;
    q->say++;
    if (q->say > q->azami) {
        q->azami = q->say;
    }
    return true;
}

uint32_t ida_gon_pompa(ida_gonderim_t *g, const ida_gon_tasima_t *t, uint64_t simdi_us)
{
    if (!t->bos(t->ctx)) {
        return 0u;
    }
    for (uint32_t i = 0u; i < 3u; i++) {
        ida_gon_kuyruk_t *q = &g->q[i];
        if (q->say == 0u) {
            continue;
        }
        ida_gon_yuva_t *y = on(q);
        if (i == 2u && g->iii_bu_periyot + y->n > IDA_GON_R_III_BAYT) {
            return 0u; /* (iii) bütçesi bu periyotta doldu */
        }
        if (!t->yaz(t->ctx, y->b, y->n)) {
            g->yazma_ret++;
            return 0u;
        }
        sira_izle(&g->sira_cikis, y->b, y->n);
        if (i == 0u) {
            const uint64_t b = simdi_us > y->t_us ? simdi_us - y->t_us : 0u;
            if (b > g->kritik_bekleme_en_kotu_us) {
                g->kritik_bekleme_en_kotu_us = b > UINT32_MAX ? UINT32_MAX : (uint32_t)b;
            }
        }
        if (i == 2u) {
            g->iii_bu_periyot += y->n;
        }
        cikar(q);
        g->tx_frames++;
        return i + 1u;
    }
    return 0u;
}

void ida_gon_periyot(ida_gonderim_t *g, uint64_t simdi_us)
{
    g->iii_bu_periyot = 0u;
    bool bekliyor = false;
    if (g->q[0].say != 0u) {
        const ida_gon_yuva_t *y = on(&g->q[0]);
        /* Bir periyottan uzun bekleyen kritik çerçeve: taşıma boşalmıyor. */
        bekliyor = simdi_us > y->t_us && simdi_us - y->t_us >= 5000u;
    }
    if (bekliyor) {
        g->tx_crit_stall++;
    }
    g->tx_stalled = bekliyor;
}

void ida_gon_temizle(ida_gonderim_t *g)
{
    for (uint32_t i = 0u; i < 3u; i++) {
        g->temizlenen += g->q[i].say;
        g->q[i].bas = 0u;
        g->q[i].say = 0u;
    }
}

bool ida_gon_kritik_hata(const ida_gonderim_t *g)
{
    return g->kritik_hata;
}

uint32_t ida_gon_bekleyen(const ida_gonderim_t *g, uint32_t sinif)
{
    if (sinif < IDA_GON_SINIF_KRITIK || sinif > IDA_GON_SINIF_KAYBED) {
        return 0u;
    }
    return g->q[sinif - 1u].say;
}
