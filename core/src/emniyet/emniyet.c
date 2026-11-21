/*
 * emniyet.c — etkin emniyet değerleri ve daraltma (W2; A4-K44, K46, K73, K111, K113, K116,
 * K125, K127, K128). Yalnız kısıtlayan yön.
 */
#include "emniyet/emniyet.h"

#include <string.h>

#include "cerceve/komut.h"

void ida_emn_kur(ida_emn_t *e)
{
    e->kisitli = (uint8_t)IDA_EMN_TUMU;
    e->armed = false;
    e->mode = (uint8_t)IDA_MODE_INIT;
    e->inst0_var = false;
    e->inst0_tube[0] = 0u;
    e->inst0_tube[1] = 0u;
    e->cagri_kisit = 0u;
    e->kabul_kisit = 0u;
    e->oturum_kapali = false;
    (void)memset(&e->d, 0, sizeof(e->d));
    e->hedef_iptal = (uint8_t)IDA_EMN_IPTAL_TUMU; /* init'te saklı hedef yok; kurulu güvenli */
    e->kusak = 0u;
    e->kusak_tumleyen = 0xFFFFFFFFu;
}

void ida_emn_kusak_artir(ida_emn_t *e)
{
    e->hedef_iptal = (uint8_t)IDA_EMN_IPTAL_TUMU;
    e->kusak = e->kusak + 1u;
    e->kusak_tumleyen = 0xFFFFFFFFu ^ e->kusak;
}

void ida_emn_oturum_basi(ida_emn_t *e)
{
    e->oturum_kapali = false;
    ida_emn_kusak_artir(e); /* saklı hedefler önceki oturumundur (EMNİYET) */
}

uint8_t ida_emn_kisit_icerigi(const ida_safety_t *s)
{
    /* Biriktirici uint32_t: bileşik atama dar tipe yapılmaz (MSVC C4244); dönüşte açık
       daraltma (değer IDA_EMN_TUMU içindedir). */
    uint32_t k = 0u;
    if (s->estop != 0u) {
        k |= IDA_EMN_ESTOP;
    }
    if ((s->tube[0] & IDA_TUBE_PLUG_IN) == 0u) {
        k |= IDA_EMN_FIS_T0;
    }
    if ((s->tube[1] & IDA_TUBE_PLUG_IN) == 0u) {
        k |= IDA_EMN_FIS_T1;
    }
    if ((s->tube[0] & IDA_TUBE_PIN_OUT) == 0u) {
        k |= IDA_EMN_PIM_T0;
    }
    if ((s->tube[1] & IDA_TUBE_PIN_OUT) == 0u) {
        k |= IDA_EMN_PIM_T1;
    }
    return (uint8_t)k;
}

static bool daraltici_hedef(uint8_t hedef)
{
    return (hedef == (uint8_t)IDA_MODE_MANUAL) || (hedef == (uint8_t)IDA_MODE_STABILIZE) ||
           (hedef == (uint8_t)IDA_MODE_DEPTH_HOLD) || (hedef == (uint8_t)IDA_MODE_STATION_KEEP);
}

/* Kip daraltması isteği: geçerli hedef ve kip INIT değilse kip değişir (A4-K127). */
typedef struct {
    bool var;
    uint8_t hedef;
} ida_kip_istek_t;

static void kip_iste(ida_daralt_t *d, ida_kip_istek_t *istek, uint8_t hedef)
{
    d->herhangi = true;
    d->kip_izni_dus = true;
    if (daraltici_hedef(hedef)) {
        istek->var = true;
        istek->hedef = hedef; /* aynı çağrıda birden çoksa sonuncusu */
    }
}

static void komutlari_uygula(const ida_in_t *in, ida_daralt_t *d, ida_kip_istek_t *istek)
{
    const uint8_t maske = (uint8_t)(in->narrow.mask & IDA_NARROW_DEFINED);
    if ((maske & IDA_NARROW_DISARM) != 0u) {
        d->herhangi = true;
        d->disarm = true;
    }
    if ((maske & IDA_NARROW_MODE) != 0u) {
        kip_iste(d, istek, in->narrow.mode);
    }
    if ((maske & IDA_NARROW_FC_DISARM) != 0u) {
        d->herhangi = true;
        d->fc_disarm = true;
    }
    if ((maske & IDA_NARROW_FC_ABORT) != 0u) {
        d->herhangi = true;
        d->fc_abort = true;
    }
    /* Platform hatasıyla cmds[]'e düşmüş daraltıcılar (A4-K46; 3.11 (b)). */
    const uint32_t n = (in->n_cmds < IDA_PORT_MAX_CMDS) ? (uint32_t)in->n_cmds : IDA_PORT_MAX_CMDS;
    for (uint32_t i = 0u; i < n; i++) {
        const ida_cmd_t *k = &in->cmds[i];
        if (!ida_kom_daraltici(k)) {
            continue;
        }
        if (k->type == (uint8_t)IDA_CMD_DISARM) {
            d->herhangi = true;
            d->disarm = true;
        } else if (k->type == (uint8_t)IDA_CMD_SET_MODE) {
            kip_iste(d, istek, k->u.set_mode.mode);
        } else if (k->u.fc.op == (uint8_t)IDA_FC_DISARM) {
            d->herhangi = true;
            d->fc_disarm = true;
        } else {
            d->herhangi = true;
            d->fc_abort = true;
        }
    }
}

static uint8_t ornek_kisitlari(const ida_in_t *in)
{
    uint8_t k = 0u;
    const uint32_t n =
        (in->n_samples < IDA_PORT_MAX_SAMPLES) ? (uint32_t)in->n_samples : IDA_PORT_MAX_SAMPLES;
    for (uint32_t i = 0u; i < n; i++) {
        const ida_sample_t *s = &in->samples[i];
        /* Yapısal geçerli: type SAFETY ve inst < 4 (her inst; A4-K116). */
        if ((s->hdr.type == (uint8_t)IDA_SENS_SAFETY) && (s->hdr.inst < IDA_PORT_MAX_INST)) {
            k = (uint8_t)(k | ida_emn_kisit_icerigi(&s->u.safety));
        }
    }
    return k;
}

static int32_t gunluk_tup_bitleri(uint8_t kisitli)
{
    int32_t b = 0;
    if ((kisitli & IDA_EMN_FIS_T0) == 0u) {
        b |= 0x1; /* tüp 0 fiş takılı */
    }
    if ((kisitli & IDA_EMN_PIM_T0) == 0u) {
        b |= 0x2; /* tüp 0 pim çıkık */
    }
    if ((kisitli & IDA_EMN_FIS_T1) == 0u) {
        b |= 0x4;
    }
    if ((kisitli & IDA_EMN_PIM_T1) == 0u) {
        b |= 0x8;
    }
    return b;
}

void ida_emn_daralt(ida_emn_t *e, const ida_in_t *in, const ida_emn_baglam_t *b)
{
    ida_daralt_t d = {0};
    ida_kip_istek_t istek = {false, 0u};
    const uint8_t onceki = e->kisitli;

    komutlari_uygula(in, &d, &istek);

    /* Emniyet örneklerinin kısıtlayıcı içeriği (kabul edilecek ya da reddedilecek). */
    e->cagri_kisit = ornek_kisitlari(in);
    e->kabul_kisit = 0u;
    d.ornek_kapanan = (uint8_t)(e->cagri_kisit & (uint8_t)~onceki);
    e->kisitli = (uint8_t)(e->kisitli | e->cagri_kisit);

    /* Oturum sonu: silahsızlandırma, etkin değerler kısıtlayana, tüpler boş (A4-K73, K116,
       K128). */
    bool ilk_oturum_sonu = false;
    if (in->plat.link_event == (uint8_t)IDA_LINKEV_SESSION_END) {
        ilk_oturum_sonu = !e->oturum_kapali;
        e->oturum_kapali = true;
        d.herhangi = true;
        d.disarm = true;
        d.oturum_sonu = true;
        e->kisitli = (uint8_t)IDA_EMN_TUMU;
        e->inst0_var = false;
        e->inst0_tube[0] = 0u;
        e->inst0_tube[1] = 0u;
    }

    d.kapanan = (uint8_t)(e->kisitli & (uint8_t)~onceki);
    if (d.kapanan != 0u) {
        d.herhangi = true; /* emniyet girdisinin kapanması (A4-K111) */
    }

    /* SAFETY_CLOSE istekleri; birleştirme A4-K125 (olay modülü). Arada SESSION_START
       olmadan gelen sonraki SESSION_END, bir alan yeniden kapanmadıkça yeni olay üretmez
       (A4-K73); yine daraltıcı girdidir. */
    if ((d.oturum_sonu && (ilk_oturum_sonu || (d.kapanan != 0u))) ||
        ((d.kapanan & IDA_EMN_ESTOP) != 0u)) {
        d.kapat = (uint8_t)(d.kapat | IDA_KAPAT_ALL);
    }
    if ((d.kapanan & (IDA_EMN_FIS_T0 | IDA_EMN_PIM_T0)) != 0u) {
        d.kapat = (uint8_t)(d.kapat | IDA_KAPAT_T0);
    }
    if ((d.kapanan & (IDA_EMN_FIS_T1 | IDA_EMN_PIM_T1)) != 0u) {
        d.kapat = (uint8_t)(d.kapat | IDA_KAPAT_T1);
    }

    if (d.disarm && e->armed) {
        e->armed = false;
        const ida_log_t k = {.t_w = b->now,
                             .code = (uint16_t)IDA_LOG_ARM_CHANGE,
                             .arg0 = 0,
                             .arg1 = d.oturum_sonu ? (int32_t)IDA_REASON_SESSION : 0};
        ida_gun_ekle(b->gun, &k);
    }
    if (istek.var && (e->mode != (uint8_t)IDA_MODE_INIT) && (e->mode != istek.hedef)) {
        const ida_log_t k = {.t_w = b->now,
                             .code = (uint16_t)IDA_LOG_MODE_CHANGE,
                             .arg0 = (int32_t)istek.hedef,
                             .arg1 = (int32_t)e->mode};
        e->mode = istek.hedef;
        ida_gun_ekle(b->gun, &k);
    }
    if (e->kisitli != onceki) {
        const ida_log_t k = {.t_w = b->now,
                             .code = (uint16_t)IDA_LOG_SAFETY_INPUT,
                             .arg0 = ((e->kisitli & IDA_EMN_ESTOP) != 0u) ? 1 : 0,
                             .arg1 = gunluk_tup_bitleri(e->kisitli)};
        ida_gun_ekle(b->gun, &k);
    }
    if (d.disarm || d.kip_izni_dus) {
        ida_emn_kusak_artir(e); /* silahsızlandırma ya da kip daralması (SESSION_END dahil) */
    }
    e->d = d;
}

void ida_emn_kabul_kisit(ida_emn_t *e, const ida_safety_t *s)
{
    e->kabul_kisit = (uint8_t)(e->kabul_kisit | ida_emn_kisit_icerigi(s));
}

bool ida_emn_cagri_sonu(const ida_emn_t *e)
{
    if (e->d.oturum_sonu) {
        return false; /* oturum sonunun dönüşü ayrıca sayılmaz (A4-K116) */
    }
    return (e->d.ornek_kapanan & (uint8_t)~e->kabul_kisit) != 0u;
}

bool ida_emn_tup_kisitli(const ida_emn_t *e, uint8_t tup)
{
    if ((e->kisitli & IDA_EMN_ESTOP) != 0u) {
        return true;
    }
    const uint8_t alanlar = (tup == 0u) ? (uint8_t)(IDA_EMN_FIS_T0 | IDA_EMN_PIM_T0)
                                        : (uint8_t)(IDA_EMN_FIS_T1 | IDA_EMN_PIM_T1);
    return (e->kisitli & alanlar) != 0u;
}

uint32_t ida_emn_fci(const ida_emn_t *e)
{
    uint32_t m = 0u;
    if ((e->kisitli & IDA_EMN_ESTOP) != 0u) {
        m |= IDA_FCI_ESTOP;
    }
    if ((e->kisitli & IDA_EMN_FIS_T0) != 0u) {
        m |= IDA_FCI_PLUG_T0;
    }
    if ((e->kisitli & IDA_EMN_FIS_T1) != 0u) {
        m |= IDA_FCI_PLUG_T1;
    }
    if ((e->kisitli & IDA_EMN_PIM_T0) != 0u) {
        m |= IDA_FCI_PIN_T0;
    }
    if ((e->kisitli & IDA_EMN_PIM_T1) != 0u) {
        m |= IDA_FCI_PIN_T1;
    }
    if (!e->inst0_var || ((e->inst0_tube[0] & IDA_TUBE_LOADED) == 0u)) {
        m |= IDA_FCI_EMPTY_T0;
    }
    if (!e->inst0_var || ((e->inst0_tube[1] & IDA_TUBE_LOADED) == 0u)) {
        m |= IDA_FCI_EMPTY_T1;
    }
    return m;
}
