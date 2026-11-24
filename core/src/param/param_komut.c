/*
 * param_komut.c — PARAM_SET, PARAM_GET, PARAM_COMMIT, PARAM_SAVE ve kayıt durumunun izlenmesi
 * (W2; A4-K78, K81, K82, K83, K100, K112; tablo 3.17).
 *
 * PARAM_SET ve PARAM_GET komut anında yanıtlanır (GET çağrı başındaki uygulanmış değeri
 * okur). PARAM_COMMIT ve PARAM_SAVE'in kararı çağrının sonundadır, girdideki sıra sonucu
 * değiştirmez (A4-K81): commit o anda bekleyen bütün değerleri (aynı çağrının sonraki
 * PARAM_SET'leri dahil) doğrular, aynı çağrıdaki bütün commit'ler aynı sonucu alır, SAVE
 * commit sonrası kümeyi kaydeder ve aynı çağrının kabul edilen commit'i A4-K78 için sayılır.
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
    const ida_param_desc_t *d = ida_prm_indis(pv->id, &indis) ? ida_prm_tanim(indis) : NULL;
    if (d == NULL) {
        ret(r, (uint16_t)IDA_REASON_PARAM_INVALID);
        return;
    }
    /* v her zaman 32 bitlik ham değerdir (3.14); type yalnız yorumu seçer. */
    const uint32_t ham = pv->v.u32;
    if ((pv->type != d->type) || !ida_prm_deger_gecerli(d, ham)) {
        ret(r, (uint16_t)IDA_REASON_PARAM_INVALID);
        return;
    }
    if ((d->cls == (uint8_t)IDA_PCLS_B) && b->armed) {
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
    const ida_param_desc_t *d = ida_prm_indis(kimlik, &indis) ? ida_prm_tanim(indis) : NULL;
    if (d == NULL) {
        ret(r, (uint16_t)IDA_REASON_PARAM_INVALID);
        return;
    }
    r->ack = (uint8_t)IDA_ACK_ACCEPTED;
    r->reason = (uint16_t)IDA_REASON_NONE;
    ida_prm_deger_yaz(d, p->yururluk.ham[indis], &r->value);
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

/* PARAM_COMMIT: karar çağrı sonundadır (A4-K81); burada yalnız sonucun indisi kaydedilir. */
static void param_commit(ida_param_t *p, const ida_prm_komut_baglam_t *b, ida_cmd_result_t *r)
{
    r->ack = (uint8_t)IDA_ACK_ACCEPTED; /* geçici; ida_prm_cagri_sonu yazar */
    r->reason = (uint16_t)IDA_REASON_NONE;
    if (b->armed) {
        p->commit_armed = true; /* A4-K81: komutun işlendiği andaki armed */
    }
    if (p->n_commit < IDA_PORT_MAX_CMDS) {
        p->commit_indis[p->n_commit] = b->sonuc_indisi;
        p->n_commit++;
    }
}

/* PARAM_SAVE: girdideki sırayla belirlenen koşullar burada (3.17 sırası: ONE_PER_CALL,
   ARMED, PARAM_BUSY); commit'e bağlı PARAM_INVALID ve gen doyması çağrı sonunda (A4-K82). */
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
    if (b->armed || b->k->cagrida_arm) {
        ret(r, (uint16_t)IDA_REASON_ARMED);
        return;
    }
    if (b->k->param_store == (uint8_t)IDA_PSTORE_BUSY) {
        ret(r, (uint16_t)IDA_REASON_PARAM_BUSY);
        return;
    }
    r->ack = (uint8_t)IDA_ACK_ACCEPTED; /* geçici; ida_prm_cagri_sonu yazar */
    r->reason = (uint16_t)IDA_REASON_NONE;
    p->save_bekliyor = true;
    p->save_indis = b->sonuc_indisi;
}

void ida_prm_cagri_basi(ida_param_t *p)
{
    p->cagri_save_sayisi = 0u;
    p->n_commit = 0u;
    p->commit_armed = false;
    p->save_bekliyor = false;
    p->save_indis = 0u;
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

/* Bu çağrının commit'lerinin ortak kararı (A4-K81): ret nedeni ya da IDA_REASON_NONE. Karar
   ne olursa olsun bekleyen değerler atılır; kabulde uygulanır. */
static uint16_t commit_karari(ida_param_t *p, const ida_prm_komut_baglam_t *b)
{
    bool b_sinifi = false;
    ida_prm_kume_t aday = p->yururluk;
    for (uint16_t i = 0u; i < (uint16_t)IDA_PRM_SAYI; i++) {
        if (p->bekleyen_var[i]) {
            aday.ham[i] = p->bekleyen.ham[i];
            const ida_param_desc_t *d = ida_prm_tanim(i);
            if ((d != NULL) && (d->cls == (uint8_t)IDA_PCLS_B)) {
                b_sinifi = true;
            }
        }
    }
    bekleyenleri_at(p);
    if (b_sinifi && (p->commit_armed || b->k->cagrida_arm)) {
        return (uint16_t)IDA_REASON_ARMED;
    }
    if (ida_prm_grup(&aday) != 0u) {
        return (uint16_t)IDA_REASON_PARAM_INVALID;
    }
    p->yururluk = aday; /* adım sınırında birlikte uygulanır; boş commit kümeyi değiştirmez */
    p->commit_uygulandi = true;
    return (uint16_t)IDA_REASON_NONE;
}

static void sonuc_reddet(ida_cmd_result_t *r, uint16_t neden, ida_core_counters_t *cnt)
{
    ret(r, neden);
    cnt->cmd_rejected++;
}

/* Bekleyen PARAM_SAVE'in kararı (A4-K78, K82, 3.17): commit sonrası küme kodlanır. Dönüş
   true: özet önbelleği tümleyeniyle tutarsız (bütünlük hatası; blok üretilmedi). */
static bool save_karari(ida_param_t *p, const ida_prm_komut_baglam_t *b, ida_cmd_result_t *r,
                        ida_core_counters_t *cnt)
{
    /* Özet ve tümleyeni bir kez okunur; blok yalnız bu doğrulanmış yerel değerle kodlanır
       (A4 §3.10, K89). Tutarsızlıkta blok üretilmez ve gen tüketilmez; çağıran bozulmayı aynı
       çağrının bütünlük sonucuna taşır. Komut sonucu: A4 bütünlük hatalı çağrıdaki komut
       sonucunu tanımlamaz; ACCEPTED blob ister (K82), REJECTED 3.17'den bir neden ister
       (K120, K124); PARAM_SAVE satırının nedenlerinden PARAM_INVALID seçildi (seçilen okuma;
       sözleşme boşluğu, küçük sürüm notu). */
    uint64_t ozet = 0u;
    if (!ida_prm_ozet_al(p, &ozet)) {
        sonuc_reddet(r, (uint16_t)IDA_REASON_PARAM_INVALID, cnt);
        return true;
    }
    if (!p->param_ok && !p->commit_uygulandi) {
        sonuc_reddet(r, (uint16_t)IDA_REASON_PARAM_INVALID, cnt);
        return false;
    }
    const uint32_t taban = (b->plat_param_gen > p->kayit_gen) ? b->plat_param_gen : p->kayit_gen;
    if (taban == UINT32_MAX) {
        sonuc_reddet(r, (uint16_t)IDA_REASON_OUT_OF_RANGE, cnt); /* gen doyar, sarmaz */
        return false;
    }
    const uint32_t len = ida_prm_blok_kodla(&p->yururluk, ozet, p->blob);
    p->kayit_gen = taban + 1u;
    p->kayit_bekliyor = true;
    r->ack = (uint8_t)IDA_ACK_ACCEPTED;
    r->reason = (uint16_t)IDA_REASON_NONE;
    r->save.gen = p->kayit_gen;
    r->save.blob = p->blob;
    r->save.len = len;
    return false;
}

int32_t ida_prm_cagri_sonu(ida_param_t *p, const ida_prm_komut_baglam_t *b, ida_out_t *out,
                           ida_core_counters_t *cnt)
{
    int32_t bozuk = 0;
    if (p->n_commit > 0u) {
        const uint16_t neden = commit_karari(p, b);
        const uint32_t n =
            (p->n_commit < IDA_PORT_MAX_CMDS) ? (uint32_t)p->n_commit : IDA_PORT_MAX_CMDS;
        for (uint32_t i = 0u; i < n; i++) {
            const uint8_t j = p->commit_indis[i];
            if (j < IDA_PORT_MAX_RESULTS) {
                ida_cmd_result_t *r = &out->results[j];
                if (neden != (uint16_t)IDA_REASON_NONE) {
                    sonuc_reddet(r, neden, cnt);
                }
            }
            commit_gunluk(b, neden == (uint16_t)IDA_REASON_NONE, neden);
        }
    }
    if (p->save_bekliyor && (p->save_indis < IDA_PORT_MAX_RESULTS)) {
        if (save_karari(p, b, &out->results[p->save_indis], cnt)) {
            bozuk = 2; /* A4-K89: tamamlayıcı kopya */
        }
    }
    p->n_commit = 0u;
    p->commit_armed = false;
    p->save_bekliyor = false;
    return bozuk;
}

void ida_prm_depo_izle(ida_param_t *p, const ida_plat_t *plat, ida_gunluk_t *gun, ida_wtime_t now)
{
    const uint8_t store = plat->param_store;
    const bool sonuc = (store == (uint8_t)IDA_PSTORE_OK) || (store == (uint8_t)IDA_PSTORE_FAIL);
    const bool gecis = p->son_store_var && (store != p->son_store) && sonuc;
    /* Bekleyen kaydın tamamlanması (OK ya da FAIL) beklenen gen ile eşleştirilir: param_gen,
       param_store'un bildirdiği son tamamlanan kayıt denemesinin kuşağıdır (OK'te kalıcı
       bloğun, FAIL'de başarısız denemenin). Durum kodunun değişmesi (aradaki BUSY'nin
       görülmesi) gerekmez; art arda iki FAIL ayrı ayrı saptanır (A4-K83; sol bulgu 2). */
    const bool tamam = p->kayit_bekliyor && sonuc && (plat->param_gen == p->kayit_gen);
    if (gecis || tamam) {
        const ida_log_t k = {.t_w = now,
                             .code = (uint16_t)IDA_LOG_PARAM_SAVE,
                             .arg0 = (int32_t)plat->param_gen,
                             .arg1 = (int32_t)store};
        ida_gun_ekle(gun, &k); /* geçiş ve tamamlanma aynı çağrıdaysa tek kayıt */
    }
    if (tamam) {
        if (store == (uint8_t)IDA_PSTORE_OK) {
            p->param_ok = true; /* A4-K78: geçerli küme commit edildi ve kaydedildi */
        }
        p->kayit_bekliyor = false; /* FAIL'de gen tabanı yükselmiş kalır (A4-K82) */
    }
    /* Aksi halde sonuç henüz yok ya da başka bir kuşağın sonucu: bekleme sürer. */
    p->son_store = store;
    p->son_store_var = true;
}
