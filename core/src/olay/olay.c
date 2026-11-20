/*
 * olay.c — olay kimliği, SAFETY_CLOSE birleştirmesi, bekleyen kümesi, sonuçlar ve yalnız
 * silen çıkış kapısı (W2; A4-K60–K66, K69, K70, K123, K125, K129).
 */
#include "olay/olay.h"

#include <string.h>

#include "emniyet/emniyet.h"
#include "zaman/zaman.h"

void ida_olay_kur(ida_olay_t *o, uint32_t boot_count, bool boot_count_ok)
{
    (void)memset(o, 0, sizeof(*o));
    o->boot_gecerli =
        boot_count_ok && (boot_count >= IDA_BOOT_COUNT_MIN) && (boot_count <= IDA_BOOT_COUNT_MAX);
    o->boot_count = boot_count;
    o->ctr = IDA_EVT_CTR_FIRST;
}

bool ida_olay_kullanilabilir(const ida_olay_t *o)
{
    return o->boot_gecerli && !o->tukendi && (o->ctr < IDA_EVT_CTR_CLOSE_ONLY);
}

void ida_olay_kapat_iste(ida_olay_t *o, uint8_t kapat)
{
    o->kapat |= kapat;
}

static bool izin_olayi(uint8_t kind)
{
    return (kind == (uint8_t)IDA_EVT_SAFETY_OPEN) || (kind == (uint8_t)IDA_EVT_FIRE);
}

uint8_t ida_olay_bekleyen_tupler(const ida_olay_t *o)
{
    uint8_t t = 0u;
    for (uint32_t i = 0u; i < IDA_OLAY_BEKLEYEN_MAX; i++) {
        const ida_olay_bekleyen_t *b = &o->bekleyen[i];
        if (b->dolu && izin_olayi(b->kind) && (b->tube < IDA_PORT_N_TUBE)) {
            t |= (uint8_t)(1u << b->tube);
        }
    }
    return t;
}

uint32_t ida_olay_bekleyen_izin_sayisi(const ida_olay_t *o)
{
    uint32_t n = 0u;
    for (uint32_t i = 0u; i < IDA_OLAY_BEKLEYEN_MAX; i++) {
        if (o->bekleyen[i].dolu && izin_olayi(o->bekleyen[i].kind)) {
            n++;
        }
    }
    return n;
}

bool ida_olay_sonuc(ida_olay_t *o, const ida_event_result_t *r, ida_olay_bekleyen_t *bulunan)
{
    for (uint32_t i = 0u; i < IDA_OLAY_BEKLEYEN_MAX; i++) {
        ida_olay_bekleyen_t *b = &o->bekleyen[i];
        if (b->dolu && (b->event_id == r->event_id)) {
            *bulunan = *b;
            b->dolu = false;
            if (o->son_id == r->event_id) {
                o->son_sonuc = r->result;
            }
            return true;
        }
    }
    return false;
}

bool ida_olay_zaman_asimi(ida_olay_t *o, ida_wtime_t now, ida_olay_bekleyen_t *bulunan)
{
    for (uint32_t i = 0u; i < IDA_OLAY_BEKLEYEN_MAX; i++) {
        ida_olay_bekleyen_t *b = &o->bekleyen[i];
        if (!b->dolu) {
            continue;
        }
        const int64_t sinir = (int64_t)b->apply_by_ns + (int64_t)IDA_EVT_RESULT_MARGIN_NS;
        if (ida_wt_fark_ns(now, b->cikis_now) > sinir) {
            *bulunan = *b;
            b->dolu = false;
            return true;
        }
    }
    return false;
}

/* Bir sonraki olay kimliğini ayırır; yalnız_kapat ise 0xFFFFFFF0 ve sonrası da kullanılır. */
static bool kimlik_ayir(ida_olay_t *o, bool yalniz_kapat, uint64_t *kimlik)
{
    if (!o->boot_gecerli || o->tukendi) {
        return false;
    }
    if (!yalniz_kapat && (o->ctr >= IDA_EVT_CTR_CLOSE_ONLY)) {
        return false;
    }
    *kimlik = ((uint64_t)o->boot_count << 32) | (uint64_t)o->ctr;
    if (o->ctr == IDA_EVT_CTR_LAST) {
        o->tukendi = true; /* sarmaz (A4-K62) */
    } else {
        o->ctr++;
    }
    return true;
}

static bool bos_yuva_var(const ida_olay_t *o)
{
    for (uint32_t i = 0u; i < IDA_OLAY_BEKLEYEN_MAX; i++) {
        if (!o->bekleyen[i].dolu) {
            return true;
        }
    }
    return false;
}

static ida_olay_bekleyen_t *bos_yuva(ida_olay_t *o)
{
    for (uint32_t i = 0u; i < IDA_OLAY_BEKLEYEN_MAX; i++) {
        if (!o->bekleyen[i].dolu) {
            return &o->bekleyen[i];
        }
    }
    return NULL;
}

static void yuvaya_al(ida_olay_bekleyen_t *y, const ida_event_t *e, ida_wtime_t now)
{
    y->dolu = true;
    y->kind = e->kind;
    y->tube = e->tube;
    y->event_id = e->event_id;
    y->cikis_now = now;
    y->apply_by_ns = e->apply_by_ns;
}

/* Olay out->events[]'in sıradaki (sıfırlanmış) yuvasına alan alan yazılır; dolgu baytları
   çıktının sıfırlamasından kalır (A4-K24: bayt düzeyinde belirlenimci). */
static ida_event_t *olay_yuvasi(ida_out_t *out)
{
    ida_event_t *e = &out->events[out->n_events];
    out->n_events++;
    return e;
}

static bool kapat_cikar(ida_olay_t *o, const ida_olay_kapi_t *k, uint8_t tube, ida_out_t *out)
{
    uint64_t kimlik = 0u;
    if (!kimlik_ayir(o, true, &kimlik)) {
        return false; /* olay kimliği yok: hiç olay üretilmez (A4-K61, K62) */
    }
    ida_event_t *e = olay_yuvasi(out);
    e->event_id = kimlik;
    e->kind = (uint8_t)IDA_EVT_SAFETY_CLOSE;
    e->tube = tube;
    e->seq = k->seq;
    e->t_decide = k->t_ref;
    e->apply_by_ns = k->apply_by_ns;
    e->hold_ns = 0u;
    o->son_id = kimlik;
    o->son_sonuc = 0u;
    ida_olay_bekleyen_t *y = bos_yuva(o);
    if (y != NULL) {
        yuvaya_al(y, e, k->now); /* küme doluysa izlenmez (A4-K129) */
    }
    return true;
}

/* Aday kapıdan geçer mi; geçmezse A4-K45/K108 engeli widen_blocked'a sayılır. */
static bool aday_gecer(const ida_olay_t *o, const ida_olay_kapi_t *k, const ida_event_t *a)
{
    if (!izin_olayi(a->kind) || (a->tube >= IDA_PORT_N_TUBE) || !k->genis_serbest) {
        return false;
    }
    if (k->daraldi || k->imu_lag) {
        (*k->widen_blocked)++;
        return false;
    }
    if (((k->kisitli_tupler >> a->tube) & 1u) != 0u) {
        return false; /* A4-K116 */
    }
    if (((ida_olay_bekleyen_tupler(o) >> a->tube) & 1u) != 0u) {
        return false; /* A4-K123 */
    }
    if ((a->kind == (uint8_t)IDA_EVT_FIRE) && !k->fire_serbest) {
        return false; /* A4-K64 */
    }
    if (a->kind == (uint8_t)IDA_EVT_SAFETY_OPEN) {
        if ((a->hold_ns < IDA_HOLD_MIN_NS) || (a->hold_ns > IDA_HOLD_MAX_NS)) {
            return false; /* A4-K66 aralığı */
        }
    }
    if ((ida_olay_bekleyen_izin_sayisi(o) >= IDA_OLAY_IZIN_MAX) || !bos_yuva_var(o)) {
        return false; /* A4-K129 */
    }
    return true;
}

void ida_olay_yaz(ida_olay_t *o, const ida_olay_kapi_t *k, ida_out_t *out)
{
    out->n_events = 0u;
    /* A4-K125: iki tüpü kapsayan bir neden varsa tek ALL, yoksa tüp başına en çok bir. */
    if ((o->kapat & IDA_KAPAT_ALL) != 0u) {
        (void)kapat_cikar(o, k, (uint8_t)IDA_TUBE_ALL, out);
    } else {
        if ((o->kapat & IDA_KAPAT_T0) != 0u) {
            (void)kapat_cikar(o, k, 0u, out);
        }
        if ((o->kapat & IDA_KAPAT_T1) != 0u) {
            (void)kapat_cikar(o, k, 1u, out);
        }
    }
    o->kapat = 0u;

    /* Genişletici adaylar: kalan yuvalara, kapıdan geçenler (A4-K125: sığmayan üretilmez,
       sayaç tüketilmez). */
    for (uint32_t i = 0u; i < (uint32_t)o->n_aday; i++) {
        if (out->n_events >= IDA_PORT_MAX_EVENTS) {
            break;
        }
        const ida_event_t *a = &o->aday[i];
        if (!aday_gecer(o, k, a)) {
            continue;
        }
        uint64_t kimlik = 0u;
        if (!kimlik_ayir(o, false, &kimlik)) {
            continue;
        }
        ida_event_t *e = olay_yuvasi(out);
        e->event_id = kimlik;
        e->kind = a->kind;
        e->tube = a->tube;
        e->seq = k->seq;
        e->t_decide = k->t_ref;
        e->apply_by_ns = k->apply_by_ns;
        e->hold_ns = (a->kind == (uint8_t)IDA_EVT_SAFETY_OPEN) ? a->hold_ns : 0u;
        o->son_id = kimlik;
        o->son_sonuc = 0u;
        ida_olay_bekleyen_t *y = bos_yuva(o);
        if (y != NULL) {
            yuvaya_al(y, e, k->now);
        }
    }
    o->n_aday = 0u;
}
