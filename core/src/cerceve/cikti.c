/*
 * cikti.c — ida_out_t'nin dönüş koduna göre yazımı (W2; A4-K27, K28, K53–K58, K64, K87–K91,
 * K121, K126; tablo 4.14).
 */
#include "cerceve/cikti.h"

#include <string.h>

#include "cekirdek.h"
#include "cerceve/komut.h"
#include "mat/ida_math.h"

/* A4-K126: NOT_INIT'in sabit engel maskesi. */
#define IDA_FCI_KURULMAMIS 0x00026FFFu
_Static_assert(IDA_FCI_KURULMAMIS ==
                   (IDA_FCI_NOT_ARMED | IDA_FCI_MODE | IDA_FCI_EST | IDA_FCI_FAILSAFE |
                    IDA_FCI_ESTOP | IDA_FCI_PLUG_T0 | IDA_FCI_PLUG_T1 | IDA_FCI_PIN_T0 |
                    IDA_FCI_PIN_T1 | IDA_FCI_EMPTY_T0 | IDA_FCI_EMPTY_T1 | IDA_FCI_EVENTS_OFF |
                    IDA_FCI_IMU_STALE | IDA_FCI_SESSION | IDA_FCI_DET_STALE),
               "A4-K126 maskesi");

void ida_cik_kurulmamis(const ida_in_t *in, uint8_t src, ida_out_t *out)
{
    (void)memset(out, 0, sizeof(*out));
    out->thr_valid = false;
    out->is_safe = true;
    out->armed = false;
    out->src = src;
    out->fs_level = (uint8_t)IDA_FS_DISARM;
    ida_kom_kurulmamis(in, out);
    out->status.mode = (uint8_t)IDA_MODE_INIT;
    out->status.fs_level = (uint8_t)IDA_FS_DISARM;
    out->status.mission_state = (uint8_t)IDA_MS_IDLE;
    out->health.core_ok = true;
    out->fc.state = (uint8_t)IDA_FCS_SAFE;
    out->fc.inhibit_mask = IDA_FCI_KURULMAMIS;
}

/* Kontrol önerisi sonlu ve [−1, 1] içinde mi; değilse sayısal çökme sayılır. */
static bool itki_gecerli(const float thr[IDA_PORT_N_THR])
{
    for (uint32_t i = 0u; i < IDA_PORT_N_THR; i++) {
        if (!ida_sonlu(thr[i]) || (thr[i] < -1.0f) || (thr[i] > 1.0f)) {
            return false;
        }
    }
    return true;
}

static ida_fc_baglam_t fc_baglam(const ida_core_t *c, const ida_cagri_t *cg, const ida_in_t *in)
{
    const ida_fc_baglam_t b = {
        .armed = c->emn.armed,
        .mode = c->emn.mode,
        .est_level = ida_kes_duzey(&c->kes),
        .fs_level = ida_gor_fs_duzey(&c->gor),
        .events_ok = ida_olay_kullanilabilir(&c->olay),
        .imu_lag = cg->imu_lag,
        .imu_bayat = cg->imu_bayat,
        .oturum_var = (in->plat.link_state == (uint8_t)IDA_LINK_RUNNING),
        .emn_fci = ida_emn_fci(&c->emn),
        .bekleyen_tupler = ida_olay_bekleyen_tupler(&c->olay),
    };
    return b;
}

static void fc_yaz(ida_core_t *c, const ida_cagri_t *cg, const ida_in_t *in, ida_out_t *out)
{
    const ida_fc_baglam_t b = fc_baglam(c, cg, in);
    ida_cerceve_t *k = &c->cer;
    const uint8_t durum = ida_fc_durum(&c->fc);
    const uint32_t engel = ida_fc_engel(&c->fc, &b) & IDA_FCI_DEFINED; /* A4-K91 */
    if (k->fc_onceki_var) {
        if (durum != k->fc_onceki_durum) {
            const ida_log_t g = {.t_w = cg->now,
                                 .code = (uint16_t)IDA_LOG_FC_STATE,
                                 .arg0 = (int32_t)durum,
                                 .arg1 = (int32_t)k->fc_onceki_durum};
            ida_gun_ekle(&c->gun, &g);
        }
        if (engel != k->fc_onceki_engel) {
            const ida_log_t g = {.t_w = cg->now,
                                 .code = (uint16_t)IDA_LOG_FC_INHIBIT,
                                 .arg0 = (int32_t)engel,
                                 .arg1 = (int32_t)k->fc_onceki_engel};
            ida_gun_ekle(&c->gun, &g);
        }
    }
    k->fc_onceki_var = true;
    k->fc_onceki_durum = durum;
    k->fc_onceki_engel = engel;
    out->fc.state = (durum <= (uint8_t)IDA_FCS_FAULT_LATCHED) ? durum : (uint8_t)IDA_FCS_SAFE;
    out->fc.inhibit_mask = engel;
    out->fc.last_event_id = c->olay.son_id;
    out->fc.last_event_result = c->olay.son_sonuc;
}

void ida_cik_yaz(ida_core_t *c, const ida_cagri_t *cg, const ida_in_t *in, ida_out_t *out)
{
    const ida_cerceve_t *k = &c->cer;
    const bool kabul = (cg->rc == IDA_RC_OK) || (cg->rc == IDA_RC_IMU_REJECTED);
    const bool step_ok = cg->step && (cg->rc == IDA_RC_OK);
    const bool armed = c->emn.armed;
    const uint8_t fs = ida_gor_fs_duzey(&c->gor);

    /* Sürekli komut (A4-K28, K53–K58). */
    float thr[IDA_PORT_N_THR] = {0.0f};
    bool cokme = false;
    if (step_ok) {
        ida_kon_cikti(&c->kon, &c->emn, thr, &cokme); /* acil durdurma kapısıyla (W5-K26) */
        if (!itki_gecerli(thr)) {
            cokme = true;
        }
    }
    const bool is_safe =
        !armed || (fs >= (uint8_t)IDA_FS_PASSIVE_SURFACE) || cg->imu_bayat || cokme;
    bool thr_valid = false;
    if (step_ok) {
        thr_valid = true;
    } else if (kabul) {
        thr_valid = is_safe; /* A4-K56 */
    } else {
        thr_valid = false; /* hata dönüşü (A4-K28) */
    }
    out->thr_valid = thr_valid;
    out->is_safe = is_safe;
    out->armed = armed;
    out->src = cg->src;
    out->fs_level = fs;
    for (uint32_t i = 0u; i < IDA_PORT_N_THR; i++) {
        out->thr_cmd[i] = (thr_valid && !is_safe) ? thr[i] : 0.0f;
    }
    out->valid_for_ns = thr_valid ? ida_prm_u32(&c->prm, IDA_PRM_valid_for_ns) : 0u;

    /* seq, t_ref (A4-K57; 4.14 "son geçerli"). */
    if (kabul && in->tf.present) {
        out->seq = in->tf.seq;
        out->t_ref = in->tf.t;
    } else if (k->tf_var) {
        out->seq = k->tf_seq;
        out->t_ref.ns = k->tf_t_ns;
    } else {
        out->seq = 0u;
        out->t_ref.ns = 0;
    }

    /* Olaylar (A4-K63–K65, K125; genişletici yalnız step OK'ta, kapı yalnız siler). */
    uint8_t kisitli = 0u;
    for (uint8_t t = 0u; t < IDA_PORT_N_TUBE; t++) {
        if (ida_emn_tup_kisitli(&c->emn, t)) {
            kisitli = (uint8_t)(kisitli | (1u << t));
        }
    }
    const ida_olay_kapi_t kapi = {
        .genis_serbest = step_ok,
        .daraldi = c->emn.d.herhangi || (kabul && ida_gor_daraldi(&c->gor)), /* A4-K45; W3-K9 */
        .imu_lag = cg->imu_lag,
        .fire_serbest = armed && !is_safe,
        .kisitli_tupler = kisitli,
        .seq = out->seq,
        .t_ref = out->t_ref,
        .now = cg->now,
        .apply_by_ns = ida_prm_u32(&c->prm, IDA_PRM_fc_apply_by_ns),
        .widen_blocked = &c->cer.cnt.widen_blocked,
    };
    ida_olay_yaz(&c->olay, &kapi, out);
    ida_fc_cikti(&c->fc, out, cg->now); /* çıkan olaylar ve fc.state yansıması (W5) */

    /* Seyrüsefer (A4-K87). */
    if (step_ok) {
        out->nav_valid = true;
        ida_kes_nav(&c->kes, out->t_ref, &out->nav);
    }

    /* Atış kontrol durumu (A4-K91) ve geçiş günlükleri. */
    fc_yaz(c, cg, in, out);

    /* Günlük (A4-K90): bekletilenler ve bu çağrınınkiler; atılanlar sayaca. */
    ida_gun_bosalt(&c->gun, out, &c->cer.cnt);

    /* Durum (A4-K88). */
    ida_status_t *st = &out->status;
    st->mode = c->emn.mode;
    st->armed = armed;
    st->fs_level = fs;
    st->fs_reasons = ida_gor_fs_nedenleri(&c->gor) | (cg->imu_lag ? IDA_FSR_IMU_LAG : 0u);
    ida_gor_durum(&c->gor, st);
    if (ida_kes_duzey(&c->kes) == (uint8_t)IDA_EST_INIT) {
        st->regime = 0u; /* INIT'te 0 ve anlamsız (3.8) */
        st->phase = 0u;
    }
    st->param_ok = c->prm.param_ok;
    st->events_ok = ida_olay_kullanilabilir(&c->olay);
    st->boot_count = k->boot_count;
    st->cnt = k->cnt;

    /* Sağlık (A4-K89). */
    ida_health_t *h = &out->health;
    h->core_ok = cg->core_ok;
    h->data_ok = !cg->imu_bayat && (in->plat.link_state == (uint8_t)IDA_LINK_RUNNING);
    h->imu_rejected = cg->imu_red;
    h->imu_lag = cg->imu_lag;
    h->input_dropped = in->plat.input_dropped;
    h->narrowed = c->emn.d.herhangi;
    h->sample_conflict = cg->cakisma;
    h->bg_bad_input = cg->bg_bad_input;
}
