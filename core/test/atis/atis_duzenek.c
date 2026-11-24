/*
 * atis_duzenek.c — atış kontrol emniyet zinciri testlerinin düzeneği (açıklama başlıkta).
 * Kanca sırası cerceve/cerceve.c (ida_cer_cagri, olagan) ve cerceve/cikti.c (ida_cik_yaz)
 * ile aynıdır; değişirse burası da değişir.
 */
#include "atis_duzenek.h"

#include <string.h>

#include "cerceve/komut.h"
#include "cerceve/ornek.h"
#include "telemetri/telemetri.h"

void dz_kosul_olagan(dz_kosul_t *k, int64_t now)
{
    (void)memset(k, 0, sizeof(*k));
    k->est = (uint8_t)IDA_EST_OK;
    k->fs = (uint8_t)IDA_FS_NONE;
    k->gcs_son_ns = now;
}

void dz_girdi(ida_in_t *in, int64_t t_ns, uint32_t seq, bool step)
{
    (void)memset(in, 0, sizeof(*in));
    in->now_w.ns = t_ns;
    in->tf.present = true;
    in->tf.seq = seq;
    in->tf.t.ns = t_ns;
    in->tf.t_rx.ns = t_ns;
    in->imu_present = step;
    if (step) {
        in->imu.hdr.t_meas.ns = t_ns;
        in->imu.hdr.t_rx.ns = t_ns;
        in->imu.hdr.seq = seq;
        in->imu.hdr.window_ns = 2500000u;
        in->imu.hdr.flags = (uint16_t)IDA_HF_VALID;
        in->imu.hdr.type = (uint8_t)IDA_SENS_IMU;
        in->imu.imu.accel[2] = -9.81f;
        in->imu.imu.temp_c = 20.0f;
    }
    in->plat.link_state = (uint8_t)IDA_LINK_RUNNING;
}

ida_sample_t *dz_emniyet(ida_in_t *in, uint8_t inst, uint32_t seq, int64_t t_meas, int64_t t_rx,
                         uint8_t estop, uint8_t t0, uint8_t t1)
{
    ida_sample_t *s = &in->samples[in->n_samples];
    in->n_samples++;
    (void)memset(s, 0, sizeof(*s));
    s->hdr.t_meas.ns = t_meas;
    s->hdr.t_rx.ns = t_rx;
    s->hdr.seq = seq;
    s->hdr.flags = (uint16_t)IDA_HF_VALID;
    s->hdr.type = (uint8_t)IDA_SENS_SAFETY;
    s->hdr.inst = inst;
    s->u.safety.estop = estop;
    s->u.safety.tube[0] = t0;
    s->u.safety.tube[1] = t1;
    return s;
}

void dz_fc(ida_in_t *in, uint32_t cmd_id, uint8_t op, uint8_t tube)
{
    ida_cmd_t *k = &in->cmds[in->n_cmds];
    in->n_cmds++;
    (void)memset(k, 0, sizeof(*k));
    k->cmd_id = cmd_id;
    k->type = (uint8_t)IDA_CMD_FC;
    k->u.fc.op = op;
    k->u.fc.tube = tube;
}

void dz_sonuc(ida_in_t *in, uint64_t event_id, uint8_t result, int64_t t_apply)
{
    ida_event_result_t *r = &in->evres[in->n_evres];
    in->n_evres++;
    r->event_id = event_id;
    r->result = result;
    r->t_apply.ns = t_apply;
}

uint32_t dz_olay_say(const ida_out_t *out, uint8_t kind)
{
    uint32_t n = 0u;
    for (uint32_t i = 0u; i < (uint32_t)out->n_events; i++) {
        if (out->events[i].kind == kind) {
            n++;
        }
    }
    return n;
}

const ida_event_t *dz_olay(const ida_out_t *out, uint8_t kind)
{
    for (uint32_t i = 0u; i < (uint32_t)out->n_events; i++) {
        if (out->events[i].kind == kind) {
            return &out->events[i];
        }
    }
    return NULL;
}

const ida_cmd_result_t *dz_sonuc_al(const ida_out_t *out, uint32_t cmd_id)
{
    for (uint32_t i = 0u; i < (uint32_t)out->n_results; i++) {
        if (out->results[i].cmd_id == cmd_id) {
            return &out->results[i];
        }
    }
    return NULL;
}

static ida_fc_baglam_t baglam(const ida_core_t *c, const ida_in_t *in, const dz_kosul_t *k)
{
    const ida_fc_baglam_t b = {
        .armed = c->emn.armed,
        .mode = c->emn.mode,
        .est_level = k->est,
        .fs_level = k->fs,
        .events_ok = ida_olay_kullanilabilir(&c->olay),
        .imu_lag = k->imu_lag,
        .imu_bayat = k->imu_bayat,
        .oturum_var = (in->plat.link_state == (uint8_t)IDA_LINK_RUNNING),
        .emn_fci = ida_emn_fci(&c->emn),
        .bekleyen_tupler = ida_olay_bekleyen_tupler(&c->olay),
        .gcs_son_ns = k->gcs_son_ns,
    };
    return b;
}

/* cerceve.c olay_sonuclari ile aynı. */
static void olay_sonuclari(ida_core_t *c, const ida_in_t *in, const ida_fc_ortam_t *fo)
{
    const uint32_t n =
        (in->n_evres < IDA_PORT_MAX_EVRES) ? (uint32_t)in->n_evres : IDA_PORT_MAX_EVRES;
    for (uint32_t i = 0u; i < n; i++) {
        ida_olay_bekleyen_t b;
        if (ida_olay_sonuc(&c->olay, &in->evres[i], &b)) {
            const ida_log_t g = {.t_w = fo->now,
                                 .code = (uint16_t)IDA_LOG_EVENT_RESULT,
                                 .arg0 = (int32_t)b.kind,
                                 .arg1 = (int32_t)in->evres[i].result};
            ida_gun_ekle(&c->gun, &g);
            ida_fc_olay_sonucu(&c->fc, &b, &in->evres[i], fo);
        } else {
            c->cer.cnt.evres_ignored++;
        }
    }
    ida_olay_bekleyen_t b;
    while (ida_olay_zaman_asimi(&c->olay, fo->now, &b)) {
        c->cer.cnt.event_unknown++;
        const ida_log_t g = {.t_w = fo->now,
                             .code = (uint16_t)IDA_LOG_EVENT_UNKNOWN,
                             .arg0 = (int32_t)b.kind,
                             .arg1 = (int32_t)b.tube};
        ida_gun_ekle(&c->gun, &g);
        ida_fc_olay_bilinmiyor(&c->fc, &b, fo);
    }
}

ida_rc_t dz_cagri(ida_core_t *c, const ida_in_t *in, ida_out_t *out, bool step, int ret,
                  const dz_kosul_t *k)
{
    const ida_wtime_t now = in->now_w;
    const ida_fc_ortam_t fo = {.now = now, .prm = &c->prm, .gun = &c->gun};
    /* (b) */
    const ida_emn_baglam_t eb = {.gun = &c->gun, .now = now};
    ida_emn_daralt(&c->emn, in, &eb);
    ida_fc_daralt(&c->fc, &c->emn, &fo);
    ida_olay_kapat_iste(&c->olay, c->emn.d.kapat);
    /* (b3) */
    if (in->plat.link_event == (uint8_t)IDA_LINKEV_SESSION_START) {
        ida_orn_kur(&c->orn);
        ida_emn_oturum_basi(&c->emn);
        ida_fc_oturum_basi(&c->fc);
    }
    /* (b2) */
    if (out == NULL) {
        return IDA_RC_BAD_INPUT;
    }
    (void)memset(out, 0, sizeof(*out));
    ida_rc_t rc = IDA_RC_OK;
    bool step_ok = false;
    if (ret != DZ_KABUL) {
        rc = (ret == DZ_RET_DUP_SEQ) ? IDA_RC_DUP_SEQ : IDA_RC_BAD_INPUT;
        ida_kom_reddet(in, &c->cer.cnt, out);
    } else {
        /* (e): örnek kapısı ve kabul edilen emniyet örnekleri */
        const ida_orn_baglam_t ob = {.now = now, .tf = &in->tf, .cnt = &c->cer.cnt, .gun = &c->gun};
        ida_orn_sonuc_t os;
        ida_orn_degerlendir(&c->orn, in, &ob, &os);
        for (uint32_t i = 0u; i < (uint32_t)os.n; i++) {
            const ida_sample_t *s = &in->samples[os.sira[i]];
            if (s->hdr.type == (uint8_t)IDA_SENS_SAFETY) {
                ida_emn_kabul_kisit(&c->emn, &s->u.safety);
                ida_fc_emniyet_ornegi(&c->fc, &c->emn, s, &fo);
            }
        }
        olay_sonuclari(c, in, &fo);
        ida_fc_baglam_t fb = baglam(c, in, k);
        ida_fc_cagri(&c->fc, &c->emn, &fb, &fo);
        const ida_komut_baglam_t kb = {
            .now_w = now,
            .daraldi = c->emn.d.herhangi,
            .imu_red = step && k->imu_red,
            .imu_lag = k->imu_lag,
            .armed = c->emn.armed,
            .param_ok = c->prm.param_ok,
            .events_ok = ida_olay_kullanilabilir(&c->olay),
            .cagrida_arm = false,
            .oturum_var = fb.oturum_var,
            .mode = c->emn.mode,
            .est_level = k->est,
            .fs_level = k->fs,
            .param_store = in->plat.param_store,
            .fc_engel = ida_fc_engel(&c->fc, &fb),
        };
        ida_kom_isle(c, in, &kb, out);
        step_ok = step && !k->imu_red;
        fb = baglam(c, in, k);
        ida_fc_adim(&c->fc, &c->emn, &fb, &c->olay, step_ok, &fo);
        rc = (step && k->imu_red) ? IDA_RC_IMU_REJECTED : IDA_RC_OK;
    }
    /* Çıktı (cikti.c): sürekli komut alanlarından yalnız olay kapısının okudukları. */
    const bool armed = c->emn.armed;
    const bool is_safe = !armed || (k->fs >= (uint8_t)IDA_FS_PASSIVE_SURFACE) || k->imu_bayat;
    out->armed = armed;
    out->is_safe = is_safe;
    out->src = step ? (uint8_t)IDA_SRC_STEP : (uint8_t)IDA_SRC_TICK;
    out->seq = in->tf.seq;
    out->t_ref = in->tf.t;
    uint8_t kisitli = 0u;
    for (uint8_t t = 0u; t < IDA_PORT_N_TUBE; t++) {
        if (ida_emn_tup_kisitli(&c->emn, t)) {
            kisitli = (uint8_t)(kisitli | (1u << t));
        }
    }
    const ida_olay_kapi_t kapi = {
        .genis_serbest = step_ok && (rc == IDA_RC_OK),
        .daraldi = c->emn.d.herhangi,
        .imu_lag = k->imu_lag,
        .fire_serbest = armed && !is_safe,
        .kisitli_tupler = kisitli,
        .seq = out->seq,
        .t_ref = out->t_ref,
        .now = now,
        .apply_by_ns = ida_prm_u32(&c->prm, IDA_PRM_fc_apply_by_ns),
        .widen_blocked = &c->cer.cnt.widen_blocked,
    };
    ida_olay_yaz(&c->olay, &kapi, out);
    ida_fc_cikti(&c->fc, out, now);
    const ida_fc_baglam_t fb = baglam(c, in, k);
    out->fc.state = ida_fc_durum(&c->fc);
    out->fc.inhibit_mask = ida_fc_engel(&c->fc, &fb);
    out->fc.last_event_id = c->olay.son_id;
    out->fc.last_event_result = c->olay.son_sonuc;
    out->health.narrowed = c->emn.d.herhangi;
    out->status.armed = armed;
    out->status.events_ok = ida_olay_kullanilabilir(&c->olay);
    ida_gun_bosalt(&c->gun, out, &c->cer.cnt);
    out->status.cnt = c->cer.cnt;
    return rc;
}
