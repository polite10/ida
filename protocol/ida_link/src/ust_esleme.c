/*
 * ust_esleme.c — A1 <-> A4 alan eşlemesi (A1 Ek A, Tablo 3.4, 3.4b, 3.5, 3.6, 3.6a, 3.7, 3.8).
 *
 * Eşlemenin tek yeridir (W3-oturum kapsam md. 7): öteki dosyalar tel mesajı ile A4 yapısı
 * arasında alan kopyalamaz, bu dosyanın işlevlerini çağırır. Değerler dönüşümsüz kopyalanır;
 * istisnalar sözleşmenin yazdıklarıdır (A1-K47 VALID = 0, Tablo 3.6 komut dönüşümü,
 * A4-K54 thr_cmd -> pwm_us, health bitleri).
 */
#include "ust_ic.h"

#include <string.h>

/* ------------------------------------------------------------------ */
/* Yardımcılar                                                        */
/* ------------------------------------------------------------------ */

static bool sonlu(float f)
{
    uint32_t b;
    memcpy(&b, &f, sizeof b);
    return (b & 0x7F800000u) != 0x7F800000u;
}

static bool aralikta(float f, float alt, float ust)
{
    return sonlu(f) && f >= alt && f <= ust;
}

/* Alt sınırı hariç: (alt, ust]. */
static bool aralikta_ah(float f, float alt, float ust)
{
    return sonlu(f) && f > alt && f <= ust;
}

static bool tamsayi_degerli(float f, int32_t alt, int32_t ust)
{
    if (!aralikta(f, (float)alt, (float)ust)) {
        return false;
    }
    return (float)(int32_t)f == f;
}

static void baslik(ida_hdr_t *h, int64_t t_meas, ida_wtime_t t_rx, uint32_t seq,
                   uint32_t window_ns, uint16_t flags, uint8_t type, uint8_t inst)
{
    h->t_meas.ns = t_meas;
    h->t_rx = t_rx;
    h->seq = seq;
    h->window_ns = window_ns;
    h->flags = flags;
    h->type = type;
    h->inst = inst;
}

/* ------------------------------------------------------------------ */
/* sim -> kart                                                        */
/* ------------------------------------------------------------------ */

void ust_esl_tf(const mavlink_ida_time_frame_t *m, ida_wtime_t t_rx, ida_time_frame_t *o)
{
    memset(o, 0, sizeof *o);
    o->present = true;           /* A1-K32 */
    o->seq = m->batch_seq;
    o->t.ns = m->t_sim_ns;
    o->t_rx = t_rx;              /* A1-K48 */
}

void ust_esl_imu(const mavlink_ida_imu_t *m, ida_wtime_t t_rx, ida_imu_sample_t *o)
{
    memset(o, 0, sizeof *o);
    baslik(&o->hdr, m->t_meas_ns, t_rx, m->seq, m->window_ns, m->flags, (uint8_t)IDA_SENS_IMU,
           m->inst);
    memcpy(o->imu.gyro, m->gyro, sizeof o->imu.gyro);
    memcpy(o->imu.accel, m->accel, sizeof o->imu.accel);
    o->imu.temp_c = m->temp_c;
}

bool ust_esl_ornek(uint32_t msgid, const void *mp, ida_wtime_t t_rx, ida_sample_t *o,
                   uint32_t *rev)
{
    ida_sample_t s;
    uint32_t r = 0;
    memset(&s, 0, sizeof s);
    switch (msgid) {
    case MAVLINK_MSG_ID_IDA_MAG: {
        const mavlink_ida_mag_t *m = (const mavlink_ida_mag_t *)mp;
        baslik(&s.hdr, m->t_meas_ns, t_rx, m->seq, 0u, m->flags, (uint8_t)IDA_SENS_MAG, m->inst);
        memcpy(s.u.mag.field, m->field, sizeof s.u.mag.field);
        break;
    }
    case MAVLINK_MSG_ID_IDA_BARO: {
        const mavlink_ida_baro_t *m = (const mavlink_ida_baro_t *)mp;
        baslik(&s.hdr, m->t_meas_ns, t_rx, m->seq, m->window_ns, m->flags,
               (uint8_t)IDA_SENS_BARO, m->inst);
        s.u.baro.p_abs = m->p_abs;
        s.u.baro.temp_c = m->temp_c;
        break;
    }
    case MAVLINK_MSG_ID_IDA_GNSS: {
        const mavlink_ida_gnss_t *m = (const mavlink_ida_gnss_t *)mp;
        uint16_t f = m->flags;
        if (m->fix_type > IDA_GNSS_FIX_RTK) {
            f = (uint16_t)(f & UST_HF_VALID_SIL);   /* Tablo 3.4b; A1-K47 */
        }
        baslik(&s.hdr, m->t_meas_ns, t_rx, m->seq, 0u, f, (uint8_t)IDA_SENS_GNSS, m->inst);
        s.u.gnss.lat_e7 = m->lat_e7;
        s.u.gnss.lon_e7 = m->lon_e7;
        s.u.gnss.alt_mm = m->alt_mm;
        memcpy(s.u.gnss.vel_ned, m->vel_ned, sizeof s.u.gnss.vel_ned);
        s.u.gnss.eph = m->eph;
        s.u.gnss.epv = m->epv;
        s.u.gnss.fix_type = m->fix_type;
        s.u.gnss.n_sv = m->n_sv;
        break;
    }
    case MAVLINK_MSG_ID_IDA_DVL: {
        const mavlink_ida_dvl_t *m = (const mavlink_ida_dvl_t *)mp;
        baslik(&s.hdr, m->t_meas_ns, t_rx, m->seq, 0u, m->flags, (uint8_t)IDA_SENS_DVL, m->inst);
        memcpy(s.u.dvl.vel, m->vel, sizeof s.u.dvl.vel);
        memcpy(s.u.dvl.cov, m->cov, sizeof s.u.dvl.cov);
        s.u.dvl.fom = m->fom;
        memcpy(s.u.dvl.beam_dist, m->beam_dist, sizeof s.u.dvl.beam_dist);
        s.u.dvl.altitude = m->altitude;
        s.u.dvl.dvl_flags = m->dvl_flags;
        s.u.dvl.beam_valid = m->beam_valid;
        break;
    }
    case MAVLINK_MSG_ID_IDA_PING1D: {
        const mavlink_ida_ping1d_t *m = (const mavlink_ida_ping1d_t *)mp;
        baslik(&s.hdr, m->t_meas_ns, t_rx, m->seq, 0u, m->flags, (uint8_t)IDA_SENS_PING1D,
               m->inst);
        s.u.ping1d.distance = m->distance;
        s.u.ping1d.scan_start = m->scan_start;
        s.u.ping1d.scan_length = m->scan_length;
        s.u.ping1d.ping_number = m->ping_number;
        s.u.ping1d.tx_offset_ns = m->tx_offset_ns;
        s.u.ping1d.confidence = m->confidence;
        break;
    }
    case MAVLINK_MSG_ID_IDA_BATT: {
        const mavlink_ida_batt_t *m = (const mavlink_ida_batt_t *)mp;
        baslik(&s.hdr, m->t_meas_ns, t_rx, m->seq, m->window_ns, m->flags,
               (uint8_t)IDA_SENS_BATT, m->inst);
        s.u.batt.voltage = m->voltage;
        s.u.batt.current = m->current;
        s.u.batt.temp_c = m->temp_c;
        break;
    }
    case MAVLINK_MSG_ID_IDA_LEAK: {
        const mavlink_ida_leak_t *m = (const mavlink_ida_leak_t *)mp;
        baslik(&s.hdr, m->t_meas_ns, t_rx, m->seq, 0u, m->flags, (uint8_t)IDA_SENS_LEAK, m->inst);
        s.u.leak.mask = m->mask;
        break;
    }
    case MAVLINK_MSG_ID_IDA_SAFETY: {
        const mavlink_ida_safety_t *m = (const mavlink_ida_safety_t *)mp;
        uint16_t f = m->flags;
        if (m->estop > 1u) {
            f = (uint16_t)(f & UST_HF_VALID_SIL);   /* Tablo 3.4b; A1-K47 */
        }
        baslik(&s.hdr, m->t_meas_ns, t_rx, m->seq, 0u, f, (uint8_t)IDA_SENS_SAFETY, m->inst);
        s.u.safety.estop = m->estop;
        s.u.safety.tube[0] = m->tube[0];
        s.u.safety.tube[1] = m->tube[1];
        r = m->safety_rev;                                  /* A1-K106: platform tutar */
        break;
    }
    default:
        return false;
    }
    *o = s;
    if (rev != NULL) {
        *rev = r;
    }
    return true;
}

void ust_esl_ping_bas(const mavlink_ida_ping360_head_t *m, ida_wtime_t t_rx, ida_ping360_t *o)
{
    /* data[] yazılmaz: parçalardan birleştirilir (A1-K49–K51). */
    memset(&o->hdr, 0, sizeof o->hdr);
    baslik(&o->hdr, m->t_meas_ns, t_rx, m->seq, 0u, m->flags, (uint8_t)IDA_SENS_PING360, m->inst);
    o->angle_grad = m->angle_grad;
    o->n_samples = m->n_samples;
    o->sample_period = m->sample_period;
    o->tx_duration_us = m->tx_duration_us;
    o->tx_freq_khz = m->tx_freq_khz;
    o->gain = m->gain;
}

void ust_esl_det_batch(const mavlink_ida_det_batch_t *m, ida_det_batch_t *o)
{
    memset(o, 0, sizeof *o);   /* reserved[] taşınmaz; alıcı 0 yazar (A1 §3.5) */
    o->t_meas_ns = m->t_meas_ns;
    o->batch_seq = m->batch_seq;
    o->window_ns = m->window_ns;
    o->calib_ver = m->calib_ver;
    o->producer_ver = m->producer_ver;
    o->flags = m->flags;
    o->source = m->source;
    o->inst = m->inst;
    o->status = m->status;
    o->count = m->count;
    o->producer_id = m->producer_id;
}

void ust_esl_det_record(const mavlink_ida_det_record_t *m, ida_det_record_t *o)
{
    memset(o, 0, sizeof *o);
    o->t_meas_ns = m->t_meas_ns;
    o->det_seq = m->det_seq;
    o->batch_seq = m->batch_seq;
    o->window_ns = m->window_ns;
    o->t_sigma_ns = m->t_sigma_ns;
    memcpy(o->z, m->z, sizeof o->z);
    memcpy(o->chol, m->chol, sizeof o->chol);
    o->confidence = m->confidence;
    o->calib_ver = m->calib_ver;
    o->producer_ver = m->producer_ver;
    o->flags = m->flags;
    o->source = m->source;
    o->inst = m->inst;
    o->kind = m->kind;
    o->frame = m->frame;
    o->class_id = m->class_id;
    o->producer_id = m->producer_id;
    o->batch_index = m->batch_index;
    o->batch_count = m->batch_count;
}

void ust_esl_evres(const mavlink_ida_event_result_t *m, ida_event_result_t *o)
{
    memset(o, 0, sizeof *o);
    o->event_id = m->event_id;
    o->result = m->result;
    o->t_apply.ns = m->t_apply_ns;
}

/* ------------------------------------------------------------------ */
/* Komutlar (Tablo 3.6, 3.6a; A1-K64, K122)                           */
/* ------------------------------------------------------------------ */

#define UST_PI_F 3.14159274f   /* π'ye en yakın float; (−π, π] sınırı için */

bool ust_esl_gorev_ogesi_gecerli(const mavlink_ida_cmd_mission_item_t *m)
{
    const float *p = m->p;
    bool cerceve_ok;
    bool ok;
    for (unsigned i = 0; i < 4u; ++i) {
        if (!sonlu(p[i])) {
            return false;
        }
    }
    if (!sonlu(m->depth) || !sonlu(m->north_m) || !sonlu(m->east_m)) {
        return false;
    }
    /* Konum alanları (Tablo 3.6a girişi). */
    switch (m->frame) {
    case IDA_MI_FRAME_NONE:
        cerceve_ok = m->lat_e7 == 0 && m->lon_e7 == 0 && m->north_m == 0.0f && m->east_m == 0.0f;
        break;
    case IDA_MI_FRAME_LOCAL:
        cerceve_ok = m->lat_e7 == 0 && m->lon_e7 == 0 && m->north_m >= -10000.0f &&
                     m->north_m <= 10000.0f && m->east_m >= -10000.0f && m->east_m <= 10000.0f;
        break;
    case IDA_MI_FRAME_GLOBAL:
        cerceve_ok = m->north_m == 0.0f && m->east_m == 0.0f && m->lat_e7 >= -900000000 &&
                     m->lat_e7 <= 900000000 && m->lon_e7 >= -1800000000 &&
                     m->lon_e7 <= 1800000000;
        break;
    default:
        return false;
    }
    if (!cerceve_ok) {
        return false;
    }
    switch (m->item_type) {
    case IDA_MI_WAYPOINT:
        ok = m->frame != IDA_MI_FRAME_NONE && aralikta(m->depth, 0.0f, 100.0f) &&
             aralikta(p[0], 0.1f, 50.0f) && aralikta(p[1], 0.05f, 3.0f) && p[2] == 0.0f &&
             p[3] == 0.0f;
        break;
    case IDA_MI_DIVE_TO:
        ok = m->frame == IDA_MI_FRAME_NONE && aralikta_ah(m->depth, 0.0f, 100.0f) &&
             aralikta(p[0], 0.05f, 1.0f) && aralikta(p[1], 0.05f, 5.0f) && p[2] == 0.0f &&
             p[3] == 0.0f;
        break;
    case IDA_MI_SURFACE:
        ok = m->frame == IDA_MI_FRAME_NONE && m->depth == 0.0f && aralikta(p[0], 0.05f, 1.0f) &&
             p[1] == 0.0f && p[2] == 0.0f && p[3] == 0.0f;
        break;
    case IDA_MI_HOLD:
        ok = aralikta(m->depth, 0.0f, 100.0f) && aralikta_ah(p[0], 0.0f, 3600.0f) &&
             aralikta(p[1], 0.1f, 50.0f) && p[2] == 0.0f && p[3] == 0.0f;
        break;
    case IDA_MI_SCAN_LINES:
        ok = m->frame != IDA_MI_FRAME_NONE && aralikta(m->depth, 0.0f, 100.0f) &&
             aralikta(p[0], 1.0f, 500.0f) && aralikta(p[1], 0.5f, 100.0f) &&
             tamsayi_degerli(p[2], 1, 100) && aralikta_ah(p[3], -UST_PI_F, UST_PI_F);
        break;
    case IDA_MI_SCAN_POINT:
        ok = m->frame != IDA_MI_FRAME_NONE && aralikta(m->depth, 0.0f, 100.0f) &&
             aralikta_ah(p[0], 0.0f, 3600.0f) && aralikta(p[1], 0.1f, 50.0f) && p[2] == 0.0f &&
             p[3] == 0.0f;
        break;
    case IDA_MI_WAIT_GNSS:
        ok = m->frame == IDA_MI_FRAME_NONE && m->depth == 0.0f &&
             aralikta_ah(p[0], 0.0f, 3600.0f) && tamsayi_degerli(p[1], 1, 4) && p[2] == 0.0f &&
             p[3] == 0.0f;
        break;
    default:
        ok = false;   /* 8 (eski TARGET_ALIGN) dahil kapalı liste dışı (A1-K122) */
        break;
    }
    return ok;
}

static bool simple_turu_gecerli(uint8_t t)
{
    switch (t) {
    case IDA_CMD_HEARTBEAT: case IDA_CMD_ARM: case IDA_CMD_DISARM: case IDA_CMD_SET_MODE:
    case IDA_CMD_MISSION_BEGIN: case IDA_CMD_MISSION_END: case IDA_CMD_MISSION_CTL:
    case IDA_CMD_PARAM_GET: case IDA_CMD_PARAM_COMMIT: case IDA_CMD_PARAM_SAVE:
    case IDA_CMD_FAILSAFE_RESET:
        return true;
    default:
        return false;
    }
}

ust_komut_tur_t ust_esl_komut(uint32_t msgid, const void *mp, ust_komut_t *k)
{
    memset(k, 0, sizeof *k);
    switch (msgid) {
    case MAVLINK_MSG_ID_IDA_CMD_SIMPLE: {
        const mavlink_ida_cmd_simple_t *m = (const mavlink_ida_cmd_simple_t *)mp;
        k->cmd_id = m->cmd_id;
        k->cmd_type = m->type;
        k->cmd.cmd_id = m->cmd_id;
        k->cmd.type = m->type;
        k->cmd.source = m->source;
        if (!simple_turu_gecerli(m->type)) {
            return UST_KOMUT_BOZUK;                       /* A1-K64 */
        }
        switch (m->type) {
        case IDA_CMD_DISARM:
            k->bit = (uint8_t)IDA_NARROW_DISARM;
            return UST_KOMUT_DARALTICI;
        case IDA_CMD_SET_MODE:
            if (m->arg < IDA_MODE_MANUAL || m->arg > IDA_MODE_AUTO) {
                return UST_KOMUT_BOZUK;                   /* P2-2: hedef 2–7 dışı */
            }
            if (m->arg <= IDA_MODE_STATION_KEEP) {
                k->bit = (uint8_t)IDA_NARROW_MODE;
                k->kip = (uint8_t)m->arg;
                return UST_KOMUT_DARALTICI;
            }
            k->cmd.u.set_mode.mode = (uint8_t)m->arg;
            k->genisletici = 1u;                          /* GUIDED, AUTO */
            return UST_KOMUT_OLAGAN;
        case IDA_CMD_ARM:
        case IDA_CMD_FAILSAFE_RESET:
            k->genisletici = 1u;
            return UST_KOMUT_OLAGAN;
        case IDA_CMD_MISSION_BEGIN:
            k->cmd.u.mission_begin.count = m->arg;
            return UST_KOMUT_OLAGAN;
        case IDA_CMD_MISSION_CTL:
            if (m->arg > 0xFFu) {
                return UST_KOMUT_BOZUK;   /* A4 tipine (uint8_t) çevrilemez (A1-K64) */
            }
            k->cmd.u.mission_ctl.action = (uint8_t)m->arg;
            return UST_KOMUT_OLAGAN;
        case IDA_CMD_PARAM_GET:
            k->cmd.u.param.id = m->arg;
            k->param_id = m->arg;
            return UST_KOMUT_OLAGAN;
        default:                          /* HEARTBEAT, MISSION_END, PARAM_COMMIT, PARAM_SAVE */
            return UST_KOMUT_OLAGAN;
        }
    }
    case MAVLINK_MSG_ID_IDA_CMD_MANUAL: {
        const mavlink_ida_cmd_manual_t *m = (const mavlink_ida_cmd_manual_t *)mp;
        k->cmd_id = m->cmd_id;
        k->cmd_type = (uint8_t)IDA_CMD_MANUAL;
        k->cmd.cmd_id = m->cmd_id;
        k->cmd.type = (uint8_t)IDA_CMD_MANUAL;
        k->cmd.source = m->source;
        memcpy(k->cmd.u.manual.axes, m->axes, sizeof k->cmd.u.manual.axes);
        return UST_KOMUT_OLAGAN;
    }
    case MAVLINK_MSG_ID_IDA_CMD_GUIDED: {
        const mavlink_ida_cmd_guided_t *m = (const mavlink_ida_cmd_guided_t *)mp;
        k->cmd_id = m->cmd_id;
        k->cmd_type = (uint8_t)IDA_CMD_GUIDED_SP;
        k->cmd.cmd_id = m->cmd_id;
        k->cmd.type = (uint8_t)IDA_CMD_GUIDED_SP;
        k->cmd.source = m->source;
        k->cmd.u.guided.mask = m->mask;
        memcpy(k->cmd.u.guided.pos_ne, m->pos_ne, sizeof k->cmd.u.guided.pos_ne);
        k->cmd.u.guided.depth = m->depth;
        k->cmd.u.guided.altitude = m->altitude;
        memcpy(k->cmd.u.guided.vel_ned, m->vel_ned, sizeof k->cmd.u.guided.vel_ned);
        k->cmd.u.guided.yaw = m->yaw;
        k->cmd.u.guided.yaw_rate = m->yaw_rate;
        return UST_KOMUT_OLAGAN;
    }
    case MAVLINK_MSG_ID_IDA_CMD_MISSION_ITEM: {
        const mavlink_ida_cmd_mission_item_t *m = (const mavlink_ida_cmd_mission_item_t *)mp;
        ida_mission_item_t *o = &k->cmd.u.mission_item;
        k->cmd_id = m->cmd_id;
        k->cmd_type = (uint8_t)IDA_CMD_MISSION_ITEM;
        k->cmd.cmd_id = m->cmd_id;
        k->cmd.type = (uint8_t)IDA_CMD_MISSION_ITEM;
        k->cmd.source = m->source;
        if (!ust_esl_gorev_ogesi_gecerli(m)) {
            return UST_KOMUT_BOZUK;                       /* A1-K122 */
        }
        o->index = m->index;
        o->item_type = m->item_type;
        o->frame = m->frame;
        memcpy(o->p, m->p, sizeof o->p);
        o->lat_e7 = m->lat_e7;
        o->lon_e7 = m->lon_e7;
        o->north_m = m->north_m;
        o->east_m = m->east_m;
        o->depth = m->depth;
        return UST_KOMUT_OLAGAN;
    }
    case MAVLINK_MSG_ID_IDA_CMD_PARAM_SET: {
        const mavlink_ida_cmd_param_set_t *m = (const mavlink_ida_cmd_param_set_t *)mp;
        k->cmd_id = m->cmd_id;
        k->cmd_type = (uint8_t)IDA_CMD_PARAM_SET;
        k->param_id = m->param_id;
        k->cmd.cmd_id = m->cmd_id;
        k->cmd.type = (uint8_t)IDA_CMD_PARAM_SET;
        k->cmd.source = m->source;
        if (m->param_type < IDA_PT_I32 || m->param_type > IDA_PT_F32) {
            return UST_KOMUT_BOZUK;
        }
        k->cmd.u.param.id = m->param_id;
        k->cmd.u.param.type = m->param_type;
        k->cmd.u.param.v.u32 = m->value_bits;             /* 32 bit aynen */
        return UST_KOMUT_OLAGAN;
    }
    case MAVLINK_MSG_ID_IDA_CMD_FC: {
        const mavlink_ida_cmd_fc_t *m = (const mavlink_ida_cmd_fc_t *)mp;
        k->cmd_id = m->cmd_id;
        k->cmd_type = (uint8_t)IDA_CMD_FC;
        k->cmd.cmd_id = m->cmd_id;
        k->cmd.type = (uint8_t)IDA_CMD_FC;
        k->cmd.source = m->source;
        if (m->arg0 != 0u || m->arg1 != 0u) {
            return UST_KOMUT_BOZUK;                       /* A1-K64 */
        }
        switch (m->op) {
        case IDA_FC_DISARM:
        case IDA_FC_ABORT:
            if (m->tube != IDA_TUBE_ALL) {
                return UST_KOMUT_BOZUK;                   /* P2-3 */
            }
            k->bit = (uint8_t)(m->op == IDA_FC_DISARM ? IDA_NARROW_FC_DISARM : IDA_NARROW_FC_ABORT);
            return UST_KOMUT_DARALTICI;
        case IDA_FC_ARM:
        case IDA_FC_FIRE:
        case IDA_FC_RESET:
        case IDA_FC_TEST_ENTER:
        case IDA_FC_TEST_EXIT:
            if (m->tube >= IDA_PORT_N_TUBE) {
                return UST_KOMUT_BOZUK;
            }
            k->genisletici = (uint8_t)(m->op != IDA_FC_TEST_EXIT);
            k->cmd.u.fc.op = m->op;
            k->cmd.u.fc.tube = m->tube;
            k->cmd.u.fc.arg0 = 0u;
            k->cmd.u.fc.arg1 = 0u;
            return UST_KOMUT_OLAGAN;
        default:
            return UST_KOMUT_BOZUK;                       /* kapalı liste dışı */
        }
    }
    default:
        return UST_KOMUT_BOZUK;
    }
}

/* ------------------------------------------------------------------ */
/* kart -> sim                                                        */
/* ------------------------------------------------------------------ */

uint16_t ust_esl_pwm(float thr, uint16_t alt, uint16_t notr, uint16_t ust)
{
    /* A4-K54: thr_cmd ∈ [−1, 1]; platformun ESC darbe genişliğine çevirmesi (A5 alt, nötr,
       üst değerleriyle parça parça doğrusal; 01 §3.1'deki 1100/1500/1900 µs ile aynı eğri). */
    float v;
    int32_t r;
    if (!sonlu(thr)) {
        return notr;
    }
    if (thr > 1.0f) {
        thr = 1.0f;
    }
    if (thr < -1.0f) {
        thr = -1.0f;
    }
    if (thr >= 0.0f) {
        v = (float)notr + thr * (float)(ust - notr);
    } else {
        v = (float)notr + thr * (float)(notr - alt);
    }
    r = (int32_t)(v + 0.5f);
    if (r < (int32_t)alt) {
        r = (int32_t)alt;
    }
    if (r > (int32_t)ust) {
        r = (int32_t)ust;
    }
    return (uint16_t)r;
}

uint8_t ust_esl_health(const ida_health_t *h)
{
    uint8_t b = (uint8_t)(h->core_ok ? IDAW_HEALTH_CORE_OK : 0);
    b = (uint8_t)(b | (h->data_ok ? IDAW_HEALTH_DATA_OK : 0));
    b = (uint8_t)(b | (h->imu_rejected ? IDAW_HEALTH_IMU_REJECTED : 0));
    b = (uint8_t)(b | (h->imu_lag ? IDAW_HEALTH_IMU_LAG : 0));
    b = (uint8_t)(b | (h->input_dropped ? IDAW_HEALTH_INPUT_DROPPED : 0));
    b = (uint8_t)(b | (h->narrowed ? IDAW_HEALTH_NARROWED : 0));
    b = (uint8_t)(b | (h->sample_conflict ? IDAW_HEALTH_SAMPLE_CONFLICT : 0));
    b = (uint8_t)(b | (h->bg_bad_input ? IDAW_HEALTH_BG_BAD_INPUT : 0));
    return b;
}

void ust_esl_event(const ida_event_t *e, uint8_t call_kind, uint32_t safety_rev, uint16_t sess,
                   mavlink_ida_event_t *m)
{
    memset(m, 0, sizeof *m);
    m->event_id = e->event_id;
    m->t_decide_ns = e->t_decide.ns;
    m->batch_seq = e->seq;
    m->apply_by_ns = e->apply_by_ns;
    m->hold_ns = e->hold_ns;
    m->safety_rev = safety_rev;
    m->sess = sess;
    m->kind = e->kind;
    m->tube = e->tube;
    m->call_kind = call_kind;
}

void ust_esl_cmd_result(const ida_cmd_result_t *r, uint16_t sess, uint32_t save_crc,
                        mavlink_ida_cmd_result_t *m)
{
    memset(m, 0, sizeof *m);
    m->cmd_id = r->cmd_id;
    m->kind = r->kind;
    m->ack = r->ack;
    m->reason = r->reason;
    m->param_id = r->value.id;
    m->param_type = r->value.type;
    m->value_bits = r->value.v.u32;
    if (r->kind == IDA_RES_PARAM_SAVE && r->ack == IDA_ACK_ACCEPTED) {
        m->save_gen = r->save.gen;
        m->save_len = (uint16_t)(r->save.len > 0xFFFFu ? 0xFFFFu : r->save.len);
        m->save_crc = save_crc;
    }
    m->sess = sess;
}

void ust_esl_nav(const ida_nav_out_t *n, uint32_t batch_seq, mavlink_ida_nav_t *m)
{
    memset(m, 0, sizeof *m);
    m->t_ns = n->t.ns;
    m->batch_seq = batch_seq;
    memcpy(m->p_ned, n->p_ned, sizeof m->p_ned);
    memcpy(m->v_ned, n->v_ned, sizeof m->v_ned);
    memcpy(m->q_nb, n->q_nb, sizeof m->q_nb);
    memcpy(m->omega_b, n->omega_b, sizeof m->omega_b);
    m->depth = n->depth;
    m->altitude = n->altitude;
    memcpy(m->p_sigma, n->p_sigma, sizeof m->p_sigma);
    memcpy(m->v_sigma, n->v_sigma, sizeof m->v_sigma);
    memcpy(m->att_sigma, n->att_sigma, sizeof m->att_sigma);
    m->origin_lat_e7 = n->origin_lat_e7;
    m->origin_lon_e7 = n->origin_lon_e7;
    m->est_level = n->est_level;
    m->reset_counter = n->reset_counter;
}

void ust_esl_status(const ida_status_t *s, const ida_health_t *h, const ida_plat_t *p,
                    uint8_t link_state, uint32_t param_blob_crc, uint32_t batch_seq,
                    uint16_t sess, mavlink_ida_status_t *m)
{
    memset(m, 0, sizeof *m);
    m->boot_count = s->boot_count;
    m->fs_reasons = s->fs_reasons;
    m->mission_item = s->mission_item;
    m->mode = s->mode;
    m->armed = (uint8_t)(s->armed ? 1u : 0u);
    m->fs_level = s->fs_level;
    m->regime = s->regime;
    m->phase = s->phase;
    m->mission_state = s->mission_state;
    m->param_ok = (uint8_t)(s->param_ok ? 1u : 0u);
    m->events_ok = (uint8_t)(s->events_ok ? 1u : 0u);
    m->health = ust_esl_health(h);
    m->link_state = link_state;
    m->param_store = p->param_store;
    m->param_fail = p->param_fail;
    m->param_gen = p->param_gen;          /* plat.param_gen (A1-K68) */
    m->param_blob_crc = param_blob_crc;
    m->batch_seq = batch_seq;
    m->sess = sess;
}

void ust_esl_fc_status(const ida_fc_status_t *f, mavlink_ida_fc_status_t *m)
{
    memset(m, 0, sizeof *m);
    m->state = f->state;
    m->inhibit_mask = f->inhibit_mask;
    m->last_event_id = f->last_event_id;
    m->last_event_result = f->last_event_result;
}

void ust_esl_log(const ida_log_t *g, mavlink_ida_log_t *m)
{
    memset(m, 0, sizeof *m);
    m->t_w_ns = g->t_w.ns;
    m->code = g->code;
    m->severity = g->severity;
    m->arg0 = g->arg0;
    m->arg1 = g->arg1;
}

void ust_esl_core_counters(const ida_core_counters_t *c, mavlink_ida_core_counters_t *m)
{
    memset(m, 0, sizeof *m);
    /* Eşleme ada dayanır (A1 §3.7), sıraya değil. */
    m->sample_rejected = c->sample_rejected;
    m->sample_future = c->sample_future;
    m->sample_conflict = c->sample_conflict;
    m->sample_dup_dropped = c->sample_dup_dropped;
    m->narrow_from_rejected = c->narrow_from_rejected;
    m->bg_bad_input = c->bg_bad_input;
    m->imu_rejected = c->imu_rejected;
    m->imu_gap = c->imu_gap;
    m->bad_input = c->bad_input;
    m->dup_seq = c->dup_seq;
    m->old_seq = c->old_seq;
    m->wall_backstep = c->wall_backstep;
    m->cmd_rejected = c->cmd_rejected;
    m->widen_blocked = c->widen_blocked;
    m->evres_ignored = c->evres_ignored;
    m->event_unknown = c->event_unknown;
    m->det_rejected = c->det_rejected;
    m->ping360_rejected = c->ping360_rejected;
    m->bg_dropped = c->bg_dropped;
    m->bg_slice_mismatch = c->bg_slice_mismatch;
    m->log_dropped = c->log_dropped;
    m->step_miss = c->step_miss;
}

void ust_esl_det_tx(const ida_det_record_t *d, mavlink_ida_det_record_t *m)
{
    memset(m, 0, sizeof *m);
    m->t_meas_ns = d->t_meas_ns;
    m->det_seq = d->det_seq;
    m->batch_seq = d->batch_seq;
    m->window_ns = d->window_ns;
    m->t_sigma_ns = d->t_sigma_ns;
    memcpy(m->z, d->z, sizeof m->z);
    memcpy(m->chol, d->chol, sizeof m->chol);
    m->confidence = d->confidence;
    m->calib_ver = d->calib_ver;
    m->producer_ver = d->producer_ver;
    m->flags = d->flags;
    m->source = d->source;
    m->inst = d->inst;
    m->kind = d->kind;
    m->frame = d->frame;
    m->class_id = d->class_id;
    m->producer_id = d->producer_id;
    m->batch_index = d->batch_index;
    m->batch_count = d->batch_count;
}

void ust_esl_board_hello(const ida_link_ust_ayar_t *a, uint32_t peer_nonce, uint32_t nonce,
                         uint8_t init_rc, uint32_t blob_crc, uint32_t blob_len,
                         uint32_t blob_gen, uint32_t last_cmd_id, uint16_t prev_sess,
                         uint16_t sessions_since_boot, mavlink_ida_board_hello_t *m)
{
    memset(m, 0, sizeof *m);
    m->dialect_hash = IDA_LINK_DIALECT_HASH;
    m->param_schema_hash = a->cekirdek.param_schema_hash;
    m->peer_nonce = peer_nonce;
    m->nonce = nonce;
    m->boot_count = a->boot_count;
    m->reset_flags = a->reset_flags;
    m->core_version = a->cekirdek.core_version;
    m->fw_version = a->fw_version;
    m->fw_git = a->fw_git;
    m->param_blob_crc = blob_crc;
    m->param_gen = blob_gen;
    m->last_cmd_id = last_cmd_id;
    m->ctrl_period_ns = (uint32_t)IDA_LINK_UST_P_NS;
    m->param_blob_len = (uint16_t)(blob_len > 0xFFFFu ? 0xFFFFu : blob_len);
    m->port_major = a->cekirdek.port_major;
    m->port_minor = a->cekirdek.port_minor;
    m->det_schema_major = (uint16_t)IDA_DET_SCHEMA_MAJOR;
    m->det_schema_minor = (uint16_t)IDA_DET_SCHEMA_MINOR;
    m->prev_sess = prev_sess;
    m->sessions_since_boot = sessions_since_boot;
    m->proto_major = 1u;
    m->proto_minor = 0u;
    m->role = (uint8_t)(a->rol == IDA_LINK_UST_ROL_SIL ? IDAW_ROLE_SIL : IDAW_ROLE_BOARD);
    m->reset_cause = a->reset_cause;
    m->consecutive_faults = a->consecutive_faults;
    m->boot_count_ok = (uint8_t)(a->boot_count_ok ? 1u : 0u);
    m->param_src = a->param_src;
    m->init_rc = init_rc;
}
