/*
 * komut.c — komut dağıtıcısı (W2; A4-K43, K45, K47, K48, K103, K104, K108, K114, K119,
 * K120, K124; tablo 3.17).
 */
#include "cerceve/komut.h"

#include <string.h>

#include "atis/atis.h"
#include "cekirdek.h"
#include "gorev/gorev.h"
#include "gudum/gudum.h"
#include "kontrol/kontrol.h"
#include "param/param.h"

uint8_t ida_kom_sonuc_turu(uint8_t cmd_type)
{
    if ((cmd_type == (uint8_t)IDA_CMD_PARAM_GET) || (cmd_type == (uint8_t)IDA_CMD_PARAM_SET)) {
        return (uint8_t)IDA_RES_PARAM_VALUE;
    }
    if (cmd_type == (uint8_t)IDA_CMD_PARAM_SAVE) {
        return (uint8_t)IDA_RES_PARAM_SAVE;
    }
    return (uint8_t)IDA_RES_ACK;
}

static bool genis_kip(uint8_t mode)
{
    return (mode == (uint8_t)IDA_MODE_GUIDED) || (mode == (uint8_t)IDA_MODE_AUTO);
}

bool ida_kom_genisletici(const ida_cmd_t *cmd)
{
    switch (cmd->type) {
    case IDA_CMD_ARM:
    case IDA_CMD_FAILSAFE_RESET:
        return true;
    case IDA_CMD_SET_MODE:
        return genis_kip(cmd->u.set_mode.mode);
    case IDA_CMD_FC:
        return (cmd->u.fc.op == (uint8_t)IDA_FC_ARM) || (cmd->u.fc.op == (uint8_t)IDA_FC_FIRE) ||
               (cmd->u.fc.op == (uint8_t)IDA_FC_RESET) ||
               (cmd->u.fc.op == (uint8_t)IDA_FC_TEST_ENTER);
    default:
        return false;
    }
}

bool ida_kom_daraltici(const ida_cmd_t *cmd)
{
    switch (cmd->type) {
    case IDA_CMD_DISARM:
        return true;
    case IDA_CMD_SET_MODE:
        return !genis_kip(cmd->u.set_mode.mode);
    case IDA_CMD_FC:
        return (cmd->u.fc.op == (uint8_t)IDA_FC_DISARM) || (cmd->u.fc.op == (uint8_t)IDA_FC_ABORT);
    default:
        return false;
    }
}

/* A4-K119: kapalı liste (daraltıcı 2 ve 3 hariç), tüp 0–1, ayrılmış argümanlar 0. */
static bool fc_bicimi_gecerli(const ida_fc_cmd_t *fc)
{
    const bool op = (fc->op == (uint8_t)IDA_FC_ARM) || (fc->op == (uint8_t)IDA_FC_FIRE) ||
                    (fc->op == (uint8_t)IDA_FC_RESET) || (fc->op == (uint8_t)IDA_FC_TEST_ENTER) ||
                    (fc->op == (uint8_t)IDA_FC_TEST_EXIT);
    return op && (fc->tube < IDA_PORT_N_TUBE) && (fc->arg0 == 0u) && (fc->arg1 == 0u);
}

static bool dedup_gordu(const ida_cerceve_t *k, uint32_t cmd_id)
{
    for (uint32_t i = 0u; i < (uint32_t)k->dedup_n; i++) {
        if (k->dedup[i] == cmd_id) {
            return true;
        }
    }
    return false;
}

static void dedup_ekle(ida_cerceve_t *k, uint32_t cmd_id)
{
    k->dedup[k->dedup_i] = cmd_id;
    k->dedup_i = (uint8_t)((k->dedup_i + 1u) % IDA_PORT_CMD_DEDUP);
    if (k->dedup_n < IDA_PORT_CMD_DEDUP) {
        k->dedup_n++;
    }
}

/* Bir sonucun yanıt kısmı (ack ve neden). */
typedef struct {
    uint8_t ack;
    uint16_t neden;
} yanit_t;

static const yanit_t KABUL = {(uint8_t)IDA_ACK_ACCEPTED, (uint16_t)IDA_REASON_NONE};
static const yanit_t YINELEME = {(uint8_t)IDA_ACK_DUPLICATE, (uint16_t)IDA_REASON_NONE};
static const yanit_t DESTEKSIZ = {(uint8_t)IDA_ACK_UNSUPPORTED, (uint16_t)IDA_REASON_NONE};

static yanit_t ret(uint16_t neden)
{
    const yanit_t y = {(uint8_t)IDA_ACK_REJECTED, neden};
    return y;
}

/* Sonucu sıfırlar ve kimliğini yazar; türü cmd'den (mandal için NULL: ACK). */
static void sonuc_kur(ida_cmd_result_t *r, uint32_t cmd_id, const ida_cmd_t *cmd)
{
    (void)memset(r, 0, sizeof(*r));
    r->cmd_id = cmd_id;
    r->kind = (cmd != NULL) ? ida_kom_sonuc_turu(cmd->type) : (uint8_t)IDA_RES_ACK;
}

static void ack(ida_cmd_result_t *r, yanit_t y)
{
    r->ack = y.ack;
    r->reason = y.neden;
}

/* PARAM_GET/PARAM_SET sonucunun value'su retde komutun kimliğini taşır (3.14: value geçerli). */
static void deger_kimligi(ida_cmd_result_t *r, const ida_cmd_t *cmd)
{
    if (r->kind == (uint8_t)IDA_RES_PARAM_VALUE) {
        r->value.id = cmd->u.param.id;
    }
}

static uint32_t mandal_sonuclari(const ida_in_t *in, yanit_t y, ida_out_t *out)
{
    uint32_t n = 0u;
    for (uint32_t i = 0u; i < IDA_PORT_N_NARROW; i++) {
        if ((in->narrow.mask & (1u << i)) != 0u) {
            ida_cmd_result_t *r = &out->results[n];
            sonuc_kur(r, in->narrow.cmd_id[i], NULL);
            ack(r, y);
            n++;
        }
    }
    return n;
}

/* (3) adımı: komutun sahibi modül (tablo 3.17). */
static void sahibine_ver(ida_core_t *c, const ida_in_t *in, const ida_komut_baglam_t *b,
                         const ida_cmd_t *cmd, ida_cmd_result_t *r)
{
    switch (cmd->type) {
    case IDA_CMD_ARM:
        /* W2'nin kuralları önce (A4-K61, K78, K83), sonra kip modülünün ön koşulu. */
        if (!b->events_ok) {
            ack(r, ret((uint16_t)IDA_REASON_EVENTS_OFF));
        } else if (!b->param_ok) {
            ack(r, ret((uint16_t)IDA_REASON_PARAM_INVALID));
        } else if (b->param_store == (uint8_t)IDA_PSTORE_BUSY) {
            ack(r, ret((uint16_t)IDA_REASON_PARAM_BUSY));
        } else {
            ida_gor_komut(&c->gor, &c->emn, b, cmd, r);
        }
        break;
    case IDA_CMD_HEARTBEAT:
    case IDA_CMD_SET_MODE:
    case IDA_CMD_FAILSAFE_RESET:
    case IDA_CMD_MISSION_BEGIN:
    case IDA_CMD_MISSION_ITEM:
    case IDA_CMD_MISSION_END:
    case IDA_CMD_MISSION_CTL:
        ida_gor_komut(&c->gor, &c->emn, b, cmd, r);
        break;
    case IDA_CMD_MANUAL:
        ida_kon_komut(&c->kon, b, cmd, r);
        break;
    case IDA_CMD_GUIDED_SP:
        ida_gud_komut(&c->gud, b, cmd, r);
        break;
    case IDA_CMD_PARAM_SET:
    case IDA_CMD_PARAM_GET:
    case IDA_CMD_PARAM_COMMIT:
    case IDA_CMD_PARAM_SAVE: {
        const ida_prm_komut_baglam_t pb = {
            .k = b, .plat_param_gen = in->plat.param_gen, .gun = &c->gun};
        ida_prm_komut(&c->prm, &pb, cmd, r);
        break;
    }
    case IDA_CMD_FC:
        if (!fc_bicimi_gecerli(&cmd->u.fc)) {
            ack(r, ret((uint16_t)IDA_REASON_OUT_OF_RANGE));
        } else {
            ida_fc_komut(&c->fc, b, &cmd->u.fc, r);
        }
        break;
    default:
        /* Bilinmeyen tür: gövde okunmaz (3.14). */
        ack(r, DESTEKSIZ);
        break;
    }
}

void ida_kom_isle(ida_core_t *c, const ida_in_t *in, const ida_komut_baglam_t *b, ida_out_t *out)
{
    ida_core_counters_t *cnt = &c->cer.cnt;
    uint32_t n = mandal_sonuclari(in, KABUL, out);
    for (uint32_t j = 0u; j < (uint32_t)in->n_cmds; j++) {
        const ida_cmd_t *cmd = &in->cmds[j];
        ida_cmd_result_t *r = &out->results[n];
        n++;
        sonuc_kur(r, cmd->cmd_id, cmd);
        if (ida_kom_daraltici(cmd)) {
            /* (b)'de uygulandı; yineleme penceresinden muaf (A4-K46). */
            ack(r, KABUL);
            continue;
        }
        /* (1) yineleme (A4-K48). */
        if (dedup_gordu(&c->cer, cmd->cmd_id)) {
            ack(r, YINELEME);
            deger_kimligi(r, cmd);
            continue;
        }
        dedup_ekle(&c->cer, cmd->cmd_id);
        /* (2) genişleticilerin genel önceliği (4.14). */
        if (ida_kom_genisletici(cmd)) {
            if (b->daraldi) {
                ack(r, ret((uint16_t)IDA_REASON_NARROWED));
                cnt->widen_blocked++;
            } else if (b->imu_red) {
                ack(r, ret((uint16_t)IDA_REASON_IMU_REJECTED));
            } else if (b->imu_lag) {
                ack(r, ret((uint16_t)IDA_REASON_IMU_LAG));
                cnt->widen_blocked++;
            } else {
                sahibine_ver(c, in, b, cmd, r);
            }
        } else {
            sahibine_ver(c, in, b, cmd, r);
        }
        if (r->ack == (uint8_t)IDA_ACK_REJECTED) {
            deger_kimligi(r, cmd);
            cnt->cmd_rejected++;
        }
    }
    out->n_results = (uint8_t)n;
}

void ida_kom_reddet(const ida_in_t *in, ida_core_counters_t *cnt, ida_out_t *out)
{
    uint32_t n = mandal_sonuclari(in, KABUL, out);
    if (in->n_cmds <= IDA_PORT_MAX_CMDS) {
        for (uint32_t j = 0u; j < (uint32_t)in->n_cmds; j++) {
            const ida_cmd_t *cmd = &in->cmds[j];
            ida_cmd_result_t *r = &out->results[n];
            n++;
            sonuc_kur(r, cmd->cmd_id, cmd);
            if (ida_kom_daraltici(cmd)) {
                ack(r, KABUL);
            } else {
                ack(r, ret((uint16_t)IDA_REASON_CALL_REJECTED));
                deger_kimligi(r, cmd);
                cnt->cmd_rejected++;
            }
        }
    }
    out->n_results = (uint8_t)n;
}

void ida_kom_kurulmamis(const ida_in_t *in, ida_out_t *out)
{
    uint32_t n =
        mandal_sonuclari(in, ret((uint16_t)IDA_REASON_NOT_INIT), out);
    if (in->n_cmds <= IDA_PORT_MAX_CMDS) {
        for (uint32_t j = 0u; j < (uint32_t)in->n_cmds; j++) {
            const ida_cmd_t *cmd = &in->cmds[j];
            ida_cmd_result_t *r = &out->results[n];
            n++;
            sonuc_kur(r, cmd->cmd_id, cmd);
            ack(r, ret((uint16_t)IDA_REASON_NOT_INIT));
            deger_kimligi(r, cmd);
        }
    }
    out->n_results = (uint8_t)n;
}
