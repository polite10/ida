/*
 * param_komut.c — PARAM_SET, PARAM_GET, PARAM_COMMIT, PARAM_SAVE ve kayıt durumunun izlenmesi
 * (W2; A4-K78, K81, K82, K83, K100, K112; tablo 3.17).
 */
#include "param/param.h"

static void deger_bos(uint16_t kimlik, ida_cmd_result_t *r)
{
    r->value.id = kimlik;
    r->value.type = 0u;
    r->value.v.u32 = 0u;
}

static void ret(ida_cmd_result_t *r, uint16_t neden)
{
    r->ack = (uint8_t)IDA_ACK_REJECTED;
    r->reason = neden;
}

static void param_set(ida_param_t *p, const ida_prm_komut_baglam_t *b, const ida_cmd_t *cmd,
                      ida_cmd_result_t *r)
{
    const ida_param_value_t *pv = &cmd->u.param;
    deger_bos(pv->id, r);
    uint16_t indis = 0u;
    if (!ida_prm_indis(pv->id, &indis)) {
        ret(r, (uint16_t)IDA_REASON_PARAM_INVALID);
        return;
    }
    const ida_param_desc_t *d = ida_prm_tanim(indis);
    /* v her zaman 32 bitlik ham değerdir (3.14); type yalnız yorumu seçer. */
    const uint32_t ham = pv->v.u32;
    if ((pv->type != d->type) || !ida_prm_deger_gecerli(d, ham)) {
        ret(r, (uint16_t)IDA_REASON_PARAM_INVALID);
        return;
    }
    if ((d->cls == (uint8_t)IDA_PCLS_B) && b->k->armed) {
        ret(r, (uint16_t)IDA_REASON_ARMED);
        return;
    }
    p->bekleyen.ham[indis] = ham;
    p->bekleyen_var[indis] = true;
    r->ack = (uint8_t)IDA_ACK_ACCEPTED;
    r->reason = (uint16_t)IDA_REASON_NONE;
    ida_prm_deger_yaz(d, ham, &r->value);
}

static void param_get(const ida_param_t *p, const ida_cmd_t *cmd, ida_cmd_result_t *r)
{
    const uint16_t kimlik = cmd->u.param.id; /* yalnız id okunur (3.14) */
    deger_bos(kimlik, r);
    uint16_t indis = 0u;
    if (!ida_prm_indis(kimlik, &indis)) {
        ret(r, (uint16_t)IDA_REASON_PARAM_INVALID);
        return;
    }
    r->ack = (uint8_t)IDA_ACK_ACCEPTED;
    r->reason = (uint16_t)IDA_REASON_NONE;
    ida_prm_deger_yaz(ida_prm_tanim(indis), p->yururluk.ham[indis], &r->value);
}

static void bekleyenleri_at(ida_param_t *p)
{
    for (uint32_t i = 0u; i < (uint32_t)IDA_PRM_SAYI; i++) {
        p->bekleyen_var[i] = false;
    }
}

static void commit_gunluk(const ida_prm_komut_baglam_t *b, bool uygulandi, uint16_t neden)
{
    const ida_log_t k = {.t_w = b->k->now_w,
                         .code = (uint16_t)IDA_LOG_PARAM_COMMIT,
                         .arg0 = uygulandi ? 1 : 0,
                         .arg1 = (int32_t)neden};
    ida_gun_ekle(b->gun, &k);
}

static void param_commit(ida_param_t *p, const ida_prm_komut_baglam_t *b, ida_cmd_result_t *r)
{
    bool bekleyen_var = false;
    bool b_sinifi = false;
    for (uint16_t i = 0u; i < (uint16_t)IDA_PRM_SAYI; i++) {
        if (p->bekleyen_var[i]) {
            bekleyen_var = true;
            if (ida_prm_tanim(i)->cls == (uint8_t)IDA_PCLS_B) {
                b_sinifi = true;
            }
        }
    }
    if (b_sinifi && (b->k->armed || b->k->cagrida_arm)) {
        bekleyenleri_at(p);
        ret(r, (uint16_t)IDA_REASON_ARMED);
        commit_gunluk(b, false, (uint16_t)IDA_REASON_ARMED);
        return;
    }
    ida_prm_kume_t aday = p->commit_hazir ? p->commit_kumesi : p->yururluk;
    for (uint32_t i = 0u; i < (uint32_t)IDA_PRM_SAYI; i++) {
        if (p->bekleyen_var[i]) {
            aday.ham[i] = p->bekleyen.ham[i];
        }
    }
    if (ida_prm_grup(&aday) != 0u) {
        bekleyenleri_at(p);
        ret(r, (uint16_t)IDA_REASON_PARAM_INVALID);
        commit_gunluk(b, false, (uint16_t)IDA_REASON_PARAM_INVALID);
        return;
    }
    r->ack = (uint8_t)IDA_ACK_ACCEPTED;
    r->reason = (uint16_t)IDA_REASON_NONE;
    if (bekleyen_var) {
        p->commit_kumesi = aday;
        p->commit_hazir = true;
        bekleyenleri_at(p);
    }
    p->commit_uygulandi = true;
    commit_gunluk(b, true, (uint16_t)IDA_REASON_NONE);
}

static void param_save(ida_param_t *p, const ida_prm_komut_baglam_t *b, ida_cmd_result_t *r)
{
    const uint8_t sira = p->cagri_save_sayisi;
    if (p->cagri_save_sayisi < UINT8_MAX) {
        p->cagri_save_sayisi++;
    }
    if (sira > 0u) {
        ret(r, (uint16_t)IDA_REASON_ONE_PER_CALL);
        return;
    }
    if (b->k->armed || b->k->cagrida_arm) {
        ret(r, (uint16_t)IDA_REASON_ARMED);
        return;
    }
    if (b->k->param_store == (uint8_t)IDA_PSTORE_BUSY) {
        ret(r, (uint16_t)IDA_REASON_PARAM_BUSY);
        return;
    }
    if (!p->param_ok && !p->commit_uygulandi) {
        ret(r, (uint16_t)IDA_REASON_PARAM_INVALID);
        return;
    }
    const ida_prm_kume_t *k = p->commit_hazir ? &p->commit_kumesi : &p->yururluk;
    const uint32_t len = ida_prm_blok_kodla(k, p->blob);
    const uint32_t taban = (b->plat_param_gen > p->kayit_gen) ? b->plat_param_gen : p->kayit_gen;
    p->kayit_gen = taban + 1u;
    p->kayit_bekliyor = true;
    r->ack = (uint8_t)IDA_ACK_ACCEPTED;
    r->reason = (uint16_t)IDA_REASON_NONE;
    r->save.gen = p->kayit_gen;
    r->save.blob = p->blob;
    r->save.len = len;
}

void ida_prm_cagri_basi(ida_param_t *p)
{
    p->cagri_save_sayisi = 0u;
}

void ida_prm_komut(ida_param_t *p, const ida_prm_komut_baglam_t *b, const ida_cmd_t *cmd,
                   ida_cmd_result_t *r)
{
    switch (cmd->type) {
    case IDA_CMD_PARAM_SET:
        param_set(p, b, cmd, r);
        break;
    case IDA_CMD_PARAM_GET:
        param_get(p, cmd, r);
        break;
    case IDA_CMD_PARAM_COMMIT:
        param_commit(p, b, r);
        break;
    case IDA_CMD_PARAM_SAVE:
        param_save(p, b, r);
        break;
    default:
        r->ack = (uint8_t)IDA_ACK_UNSUPPORTED;
        r->reason = (uint16_t)IDA_REASON_NONE;
        break;
    }
}

void ida_prm_cagri_sonu(ida_param_t *p)
{
    if (p->commit_hazir) {
        p->yururluk = p->commit_kumesi;
        p->commit_hazir = false;
    }
}

void ida_prm_depo_izle(ida_param_t *p, const ida_plat_t *plat, ida_gunluk_t *gun, ida_wtime_t now)
{
    const uint8_t store = plat->param_store;
    const bool sonuc = (store == (uint8_t)IDA_PSTORE_OK) || (store == (uint8_t)IDA_PSTORE_FAIL);
    if (p->son_store_var && (store != p->son_store) && sonuc) {
        const ida_log_t k = {.t_w = now,
                             .code = (uint16_t)IDA_LOG_PARAM_SAVE,
                             .arg0 = (int32_t)plat->param_gen,
                             .arg1 = (int32_t)store};
        ida_gun_ekle(gun, &k);
        if (p->kayit_bekliyor) {
            if ((store == (uint8_t)IDA_PSTORE_OK) && (plat->param_gen == p->kayit_gen)) {
                p->param_ok = true; /* A4-K78: geçerli küme commit edildi ve kaydedildi */
            }
            p->kayit_bekliyor = false;
        }
    }
    p->son_store = store;
    p->son_store_var = true;
}
