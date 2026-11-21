/*
 * ust_komut.c — komut sırası (A1 §4.5): daraltıcının mandala alınması (A4-K46, A1-K63),
 * kendisinden önce alınmış ve çağrıya girmemiş genişleticilerin atılması (D-17; A1-K63,
 * A4-K105), atılan kimliklerin nihailiği (son 32), erteleme ve atma (A4-K105), platformun
 * komut sonuçları (A1-K64, K67) ve last_cmd_id (A1-K66).
 *
 * Emniyet ilkesi: kısıtlayan her şey genişletenden önce; kuşkuda genişletici atılır ve
 * reddedilir. Atış kontrol komutları burada yalnız taşınır ve sıralanır.
 */
#include "ust_ic.h"

#include <string.h>

/* ------------------------------------------------------------------ */
/* Yardımcılar                                                        */
/* ------------------------------------------------------------------ */

void ust_last_cmd_id(ida_link_ust_t *l, uint32_t cmd_id)
{
    if (cmd_id == 0u) {
        return;
    }
    /* A1-K66: A1-K123 anlamında en yeni kimlik; 0 "komut yok". */
    if (l->ozel_last_cmd_id == 0u) {
        l->ozel_last_cmd_id = cmd_id;
        return;
    }
    {
        const uint32_t d = cmd_id - l->ozel_last_cmd_id;
        if (d >= 1u && d <= UINT32_C(0x7FFFFFFF)) {
            l->ozel_last_cmd_id = cmd_id;
        }
    }
}

static bool nihai_mi(const ida_link_ust_t *l, uint32_t cmd_id)
{
    for (unsigned i = 0; i < l->ozel_n_nihai; ++i) {
        if (l->ozel_nihai[(l->ozel_nihai_bas + i) % IDA_LINK_UST_NIHAI_N] == cmd_id) {
            return true;
        }
    }
    return false;
}

static void nihai_ekle(ida_link_ust_t *l, uint32_t cmd_id)
{
    if (nihai_mi(l, cmd_id)) {
        return;
    }
    if (l->ozel_n_nihai < IDA_LINK_UST_NIHAI_N) {
        l->ozel_nihai[(l->ozel_nihai_bas + l->ozel_n_nihai) % IDA_LINK_UST_NIHAI_N] = cmd_id;
        l->ozel_n_nihai = (uint8_t)(l->ozel_n_nihai + 1u);
        return;
    }
    l->ozel_nihai[l->ozel_nihai_bas] = cmd_id;   /* en eskisinin yerine */
    l->ozel_nihai_bas = (uint8_t)((l->ozel_nihai_bas + 1u) % IDA_LINK_UST_NIHAI_N);
}

void ust_komut_sonuc(ida_link_ust_t *l, uint32_t cmd_id, uint8_t cmd_type, uint16_t param_id,
                     uint8_t ack, uint16_t reason)
{
    mavlink_ida_cmd_result_t m;
    memset(&m, 0, sizeof m);
    m.cmd_id = cmd_id;
    /* Sonuç türü yalnız komut türüne bağlıdır (A4-K43); PARAM_GET/SET'in ret sonucunda
       param_id yansıtılır, param_type = value_bits = 0 ("değer yok"; A4 3.14). */
    if (cmd_type == IDA_CMD_PARAM_GET || cmd_type == IDA_CMD_PARAM_SET) {
        m.kind = (uint8_t)IDA_RES_PARAM_VALUE;
        m.param_id = param_id;
    } else if (cmd_type == IDA_CMD_PARAM_SAVE) {
        m.kind = (uint8_t)IDA_RES_PARAM_SAVE;
    } else {
        m.kind = (uint8_t)IDA_RES_ACK;
    }
    m.ack = ack;
    m.reason = reason;
    m.sess = l->ozel_sess;
    (void)ust_gonder(l, IDA_LINK_SINIF_ZORUNLU, MAVLINK_MSG_ID_IDA_CMD_RESULT, &m, sizeof m);
}

static unsigned mandal_dizini(uint8_t bit)
{
    switch (bit) {
    case IDA_NARROW_DISARM: return 0u;
    case IDA_NARROW_MODE: return 1u;
    case IDA_NARROW_FC_DISARM: return 2u;
    default: return 3u;                                   /* IDA_NARROW_FC_ABORT */
    }
}

static uint8_t mandal_turu(unsigned i)
{
    return (uint8_t)(i == 0u ? IDA_CMD_DISARM : (i == 1u ? IDA_CMD_SET_MODE : IDA_CMD_FC));
}

/* D-17 (A1-K63, A4-K105): çekirdek çağrısına girmemiş bütün genişleticiler — açık partide,
   bekleyen partide ve erteleme tamponunda; hepsi bu kuyruktadır — atılır ve her biri
   REJECTED / NARROWED alır; nötrler sırasını korur. */
static void genisleticileri_at(ida_link_ust_t *l)
{
    uint8_t j = 0u;
    for (unsigned i = 0; i < l->ozel_n_kom; ++i) {
        const ida_link_ozel_komut_t *k = &l->ozel_kom[i];
        if (k->genisletici) {
            ust_komut_sonuc(l, k->cmd.cmd_id, k->cmd.type, 0u, (uint8_t)IDA_ACK_REJECTED,
                            (uint16_t)IDA_REASON_NARROWED);
            if (l->ozel_durum != (uint8_t)IDA_LINK_UST_RUNNING) {
                /* Sonuç gönderilemedi, oturum bitti (TX_OVERFLOW) ve kuyruk silindi
                   (A4-K74): sıkıştırma bırakılır, silinmiş sayı geri yazılmaz (sol bulgu 2). */
                return;
            }
            nihai_ekle(l, k->cmd.cmd_id);
            ust_last_cmd_id(l, k->cmd.cmd_id);
            continue;
        }
        if (j != i) {
            l->ozel_kom[j] = *k;
        }
        j = (uint8_t)(j + 1u);
    }
    l->ozel_n_kom = j;
}

static void mandala_al(ida_link_ust_t *l, const ust_komut_t *k)
{
    const unsigned i = mandal_dizini(k->bit);
    ida_narrow_t *m = &l->ozel_mandal;
    if ((m->mask & k->bit) != 0u) {
        /* Üzerine yazılan önceki daraltıcı: ACCEPTED / COALESCED (A1-K67, A4-K46). */
        ust_komut_sonuc(l, m->cmd_id[i], mandal_turu(i), 0u, (uint8_t)IDA_ACK_ACCEPTED,
                        (uint16_t)IDA_REASON_COALESCED);
        if (m->coalesced[i] < 255u) {
            m->coalesced[i] = (uint8_t)(m->coalesced[i] + 1u);
        }
        l->say[IDA_LINK_SAY_NARROW_COALESCED] += 1u;
    }
    m->mask = (uint8_t)(m->mask | k->bit);
    m->cmd_id[i] = k->cmd_id;
    if (k->bit == (uint8_t)IDA_NARROW_MODE) {
        m->mode = k->kip;
    }
    genisleticileri_at(l);
}

/* ------------------------------------------------------------------ */
/* Komut üyesi                                                        */
/* ------------------------------------------------------------------ */

void ust_komut_al(ida_link_ust_t *l, uint32_t msgid, const void *mp)
{
    ust_komut_t k;
    const ust_komut_tur_t tur = ust_esl_komut(msgid, mp, &k);
    if (tur != UST_KOMUT_DARALTICI && nihai_mi(l, k.cmd_id)) {
        /* W3-K11 (A1-K63): nihai NARROWED kimliği, biçimce geçerli bir daraltıcı dışında hiçbir
           biçimde (genişletici, nötr, alanı bozuk) yeniden işlenmez; önceki nihai sonuç
           yinelenir, LINK_BAD_FIELD üretilmez (sol bulgu 6). */
        ust_komut_sonuc(l, k.cmd_id, k.cmd_type, k.param_id, (uint8_t)IDA_ACK_REJECTED,
                        (uint16_t)IDA_REASON_NARROWED);
        return;
    }
    if (tur == UST_KOMUT_BOZUK) {
        /* A1-K64: çekirdeğe ve mandala verilmez. */
        l->say[IDA_LINK_SAY_CMD_BAD_FIELD] += 1u;
        ust_komut_sonuc(l, k.cmd_id, k.cmd_type, k.param_id, (uint8_t)IDA_ACK_REJECTED,
                        (uint16_t)IDAW_REASON_LINK_BAD_FIELD);
        return;
    }
    if (tur == UST_KOMUT_DARALTICI) {
        /* A1-K63: ayrıştırıldığı anda mandala; hiçbir koşulda atılmaz. W3-K11: kimliği nihai
           NARROWED listesinde olsa da biçimce geçerli daraltıcı mandala alınır ve kendi
           olağan sonucunu alır (A4-K46, D-20: daraltıcı yineleme denetimlerinden muaftır). */
        mandala_al(l, &k);
        return;
    }
    if (l->ozel_n_kom >= IDA_LINK_UST_KOMUT_KUYRUGU) {
        /* A4-K105: erteleme tamponu dolu. */
        l->say[IDA_LINK_SAY_CMD_DROPPED] += 1u;
        ust_komut_sonuc(l, k.cmd_id, k.cmd_type, k.param_id, (uint8_t)IDA_ACK_REJECTED,
                        (uint16_t)IDAW_REASON_LINK_DROPPED);
        return;
    }
    l->ozel_kom[l->ozel_n_kom].cmd = k.cmd;
    l->ozel_kom[l->ozel_n_kom].genisletici = k.genisletici;
    l->ozel_kom[l->ozel_n_kom].kapali = 0u;
    l->ozel_kom[l->ozel_n_kom].ertelendi = 0u;
    l->ozel_n_kom = (uint8_t)(l->ozel_n_kom + 1u);
}

void ust_komut_pencere_kapandi(ida_link_ust_t *l)
{
    for (unsigned i = 0; i < l->ozel_n_kom; ++i) {
        l->ozel_kom[i].kapali = 1u;
    }
}

void ust_komut_pencere_atildi(ida_link_ust_t *l)
{
    /* A1-K31: DUP/OLD penceresinin üyeleri atılır (açık partideki komutlar dahil). */
    uint8_t j = 0u;
    for (unsigned i = 0; i < l->ozel_n_kom; ++i) {
        if (!l->ozel_kom[i].kapali) {
            continue;
        }
        if (j != i) {
            l->ozel_kom[j] = l->ozel_kom[i];
        }
        j = (uint8_t)(j + 1u);
    }
    l->ozel_n_kom = j;
}

uint8_t ust_komut_cek(ida_link_ust_t *l, ida_cmd_t *cmds)
{
    uint8_t n = 0u;
    uint8_t j = 0u;
    for (unsigned i = 0; i < l->ozel_n_kom; ++i) {
        ida_link_ozel_komut_t *k = &l->ozel_kom[i];
        if (k->kapali && n < IDA_PORT_MAX_CMDS) {
            cmds[n] = k->cmd;
            n = (uint8_t)(n + 1u);
            ust_last_cmd_id(l, k->cmd.cmd_id);
            continue;
        }
        if (k->kapali && !k->ertelendi) {
            k->ertelendi = 1u;
            l->say[IDA_LINK_SAY_CMD_DEFERRED] += 1u;      /* A4-K105 */
        }
        if (j != i) {
            l->ozel_kom[j] = *k;
        }
        j = (uint8_t)(j + 1u);
    }
    l->ozel_n_kom = j;
    return n;
}
