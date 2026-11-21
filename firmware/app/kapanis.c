/* kapanis.c — platformun güvenli kapanış kararı (kapanis.h). */
#include "kapanis.h"

void ida_kapanis_kur(ida_kapanis_t *k)
{
    k->neden = IDA_KAPANIS_YOK;
}

uint8_t ida_kapanis_denetle(ida_kapanis_t *k, ida_gonderim_t *g, bool rng_hata,
                            ida_link_ust_t *ust)
{
    if (k->neden != IDA_KAPANIS_YOK) {
        return IDA_KAPANIS_YOK; /* zaten güvenli beklemede */
    }
    uint8_t neden = IDA_KAPANIS_YOK;
    if (ida_gon_kritik_hata(g)) {
        neden = IDA_KAPANIS_KRITIK;
    } else if (rng_hata) {
        neden = IDA_KAPANIS_RNG;
    }
    if (neden == IDA_KAPANIS_YOK) {
        return IDA_KAPANIS_YOK;
    }
    /* W3-K10: çağrı penceresi açıkken oturum modülüne dokunulmaz; put_out'tan sonra denenir. */
    if (ida_link_ust_cagri_acik(ust)) {
        return IDA_KAPANIS_YOK;
    }
    k->neden = neden;
    if (neden == IDA_KAPANIS_RNG) {
        ida_gon_temizle(g); /* A1-K13: gerçek rastgele olmayan nonce'lu duyuru gitmesin */
    }
    ida_link_ust_guvenli_bekleme(ust);
    return neden;
}
