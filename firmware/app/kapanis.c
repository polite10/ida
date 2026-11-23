/* kapanis.c — platformun güvenli kapanışı ve çekirdek çağrı kapısı (kapanis.h). */
#include "kapanis.h"

#include <stddef.h>

void ida_kapanis_kur(ida_kapanis_t *k)
{
    k->neden = IDA_KAPANIS_YOK;
    k->evre = IDA_KAPANIS_EVRE_YOK;
    k->oturum_sonu_teslim = 0u;
}

uint8_t ida_kapanis_denetle(ida_kapanis_t *k, ida_gonderim_t *g, bool rng_hata,
                            ida_link_ust_t *ust)
{
    if (k->neden != IDA_KAPANIS_YOK) {
        return IDA_KAPANIS_YOK; /* kapanış zaten başladı */
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
    /* 1. adım: girdiler engellenir; oturum varsa biter ve oturum sonu çağrısı beklemeye alınır. */
    ida_link_ust_guvenli_bekleme(ust);
    k->evre = (ida_link_ust_durum(ust) == IDA_LINK_UST_SONLANIYOR) ? IDA_KAPANIS_EVRE_OTURUM_SONU
                                                                     : IDA_KAPANIS_EVRE_YASAK;
    return neden;
}

ida_kapanis_tur_t ida_kapanis_cagri(ida_kapanis_t *k, ida_link_ust_t *ust, bool cekirdek_calisir,
                                    ida_in_t *in, ida_out_t *out, ida_kapanis_cekirdek_f f,
                                    void *ctx)
{
    ida_kapanis_tur_t t;
    t.tur = ida_link_ust_take(ust, in);
    t.cekirdek_cagrildi = false;
    t.rc = IDA_RC_NOT_INIT;
    if (t.tur == IDA_LINK_CAGRI_YOK) {
        return t;
    }
    bool kapi = false;
    if (k->evre == IDA_KAPANIS_EVRE_YOK) {
        kapi = true;
    } else if (k->evre == IDA_KAPANIS_EVRE_OTURUM_SONU) {
        /* 2. adım: yalnız oturum sonu çağrısı (A1-K107; daraltıcı, A4-K73). */
        kapi = t.tur == IDA_LINK_CAGRI_OTURUM_SONU;
    }
    if (kapi && cekirdek_calisir && f != NULL) {
        t.rc = f(ctx, t.tur, in, out);
        t.cekirdek_cagrildi = true;
        ida_link_ust_put_out(ust, in, t.rc, out);
    } else {
        /* Çekirdek çağrılmaz; çıktı yok (ust.h: out NULL). */
        ida_link_ust_put_out(ust, in, IDA_RC_NOT_INIT, NULL);
    }
    if (k->evre == IDA_KAPANIS_EVRE_OTURUM_SONU && t.tur == IDA_LINK_CAGRI_OTURUM_SONU) {
        if (t.cekirdek_cagrildi) {
            k->oturum_sonu_teslim++;
        }
        k->evre = IDA_KAPANIS_EVRE_YASAK; /* 3. adım */
    }
    return t;
}

bool ida_kapanis_yasak(const ida_kapanis_t *k)
{
    return k->evre == IDA_KAPANIS_EVRE_YASAK;
}
