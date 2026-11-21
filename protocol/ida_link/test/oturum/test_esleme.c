/*
 * test_esleme.c — A1 Ek A eşlemesi (W3-oturum kabul md. 3): her satır için alan değerinin
 * iki yönde korunması.
 *
 * Yöntem. sim -> kart satırları: desenle doldurulmuş tel mesajı -> modülün eşlemesi (A4) ->
 * testin ters eşlemesi (tel) ve özgün mesajla bayt bayt eşitlik (tel yapıları paketlidir).
 * Desenin her baytı sıfırdan farklı olduğundan eşlemenin atladığı her alan farkı yakalanır.
 * kart -> sim satırları: desenli A4 yapısı -> modülün eşlemesi (tel) -> testin ters eşlemesi
 * (A4) -> modülün eşlemesi (tel) ve iki tel mesajının eşitliği; ayrıca ilk tel mesajının hiçbir
 * baytı sıfır değildir (atlanan alan yok).
 */
#include "destek_oturum.h"

#include "unity.h"

#include <string.h>

void setUp(void);
void tearDown(void);
void setUp(void) {}
void tearDown(void) {}

static void desen(void *p, size_t n, unsigned tohum)
{
    uint8_t *b = (uint8_t *)p;
    for (size_t i = 0; i < n; ++i) {
        b[i] = (uint8_t)(0x40u + ((i + tohum) % 0x20u));   /* float'lar sonlu, bayt ≠ 0 */
    }
}

static void sifirsiz(const void *p, size_t n)
{
    const uint8_t *b = (const uint8_t *)p;
    for (size_t i = 0; i < n; ++i) {
        TEST_ASSERT_NOT_EQUAL_MESSAGE(0u, b[i], "eşlenmeyen alan");
    }
}

static void hdr_ters(const ida_hdr_t *h, int64_t *t_meas, uint32_t *seq, uint16_t *flags,
                     uint8_t *inst)
{
    *t_meas = h->t_meas.ns;
    *seq = h->seq;
    *flags = h->flags;
    *inst = h->inst;
}

/* ------------------------------------------------------------------ */
/* sim -> kart                                                        */
/* ------------------------------------------------------------------ */

static void test_tf(void)
{
    mavlink_ida_time_frame_t m, r;
    ida_time_frame_t o;
    ida_wtime_t t;
    desen(&m, sizeof m, 1u);
    t.ns = 777;
    ust_esl_tf(&m, t, &o);
    TEST_ASSERT_TRUE(o.present);
    TEST_ASSERT_EQUAL_INT64(777, o.t_rx.ns);
    r = m;   /* sess, n_members, tf_flags, run_mode A4'te yok (A1 §3.8) */
    r.t_sim_ns = o.t.ns;
    r.batch_seq = o.seq;
    TEST_ASSERT_EQUAL_MEMORY(&m, &r, MAVLINK_MSG_ID_IDA_TIME_FRAME_LEN);
    TEST_ASSERT_EQUAL_INT64(m.t_sim_ns, o.t.ns);
    TEST_ASSERT_EQUAL_UINT32(m.batch_seq, o.seq);
}

static void test_imu(void)
{
    mavlink_ida_imu_t m, r;
    ida_imu_sample_t o;
    ida_wtime_t t = {55};
    desen(&m, sizeof m, 2u);
    ust_esl_imu(&m, t, &o);
    TEST_ASSERT_EQUAL_UINT8(IDA_SENS_IMU, o.hdr.type);
    TEST_ASSERT_EQUAL_INT64(55, o.hdr.t_rx.ns);
    memset(&r, 0, sizeof r);
    hdr_ters(&o.hdr, &r.t_meas_ns, &r.seq, &r.flags, &r.inst);
    r.window_ns = o.hdr.window_ns;
    memcpy(r.gyro, o.imu.gyro, sizeof r.gyro);
    memcpy(r.accel, o.imu.accel, sizeof r.accel);
    r.temp_c = o.imu.temp_c;
    TEST_ASSERT_EQUAL_MEMORY(&m, &r, MAVLINK_MSG_ID_IDA_IMU_LEN);
}

/* Örnek türleri: msgid, yapı, ters eşleme. */
static void ornek_ters(uint32_t msgid, const ida_sample_t *o, uint32_t rev, void *rp)
{
    switch (msgid) {
    case MAVLINK_MSG_ID_IDA_MAG: {
        mavlink_ida_mag_t *r = (mavlink_ida_mag_t *)rp;
        hdr_ters(&o->hdr, &r->t_meas_ns, &r->seq, &r->flags, &r->inst);
        memcpy(r->field, o->u.mag.field, sizeof r->field);
        break;
    }
    case MAVLINK_MSG_ID_IDA_BARO: {
        mavlink_ida_baro_t *r = (mavlink_ida_baro_t *)rp;
        hdr_ters(&o->hdr, &r->t_meas_ns, &r->seq, &r->flags, &r->inst);
        r->window_ns = o->hdr.window_ns;
        r->p_abs = o->u.baro.p_abs;
        r->temp_c = o->u.baro.temp_c;
        break;
    }
    case MAVLINK_MSG_ID_IDA_GNSS: {
        mavlink_ida_gnss_t *r = (mavlink_ida_gnss_t *)rp;
        hdr_ters(&o->hdr, &r->t_meas_ns, &r->seq, &r->flags, &r->inst);
        r->lat_e7 = o->u.gnss.lat_e7;
        r->lon_e7 = o->u.gnss.lon_e7;
        r->alt_mm = o->u.gnss.alt_mm;
        memcpy(r->vel_ned, o->u.gnss.vel_ned, sizeof r->vel_ned);
        r->eph = o->u.gnss.eph;
        r->epv = o->u.gnss.epv;
        r->fix_type = o->u.gnss.fix_type;
        r->n_sv = o->u.gnss.n_sv;
        break;
    }
    case MAVLINK_MSG_ID_IDA_DVL: {
        mavlink_ida_dvl_t *r = (mavlink_ida_dvl_t *)rp;
        hdr_ters(&o->hdr, &r->t_meas_ns, &r->seq, &r->flags, &r->inst);
        memcpy(r->vel, o->u.dvl.vel, sizeof r->vel);
        memcpy(r->cov, o->u.dvl.cov, sizeof r->cov);
        r->fom = o->u.dvl.fom;
        memcpy(r->beam_dist, o->u.dvl.beam_dist, sizeof r->beam_dist);
        r->altitude = o->u.dvl.altitude;
        r->dvl_flags = o->u.dvl.dvl_flags;
        r->beam_valid = o->u.dvl.beam_valid;
        break;
    }
    case MAVLINK_MSG_ID_IDA_PING1D: {
        mavlink_ida_ping1d_t *r = (mavlink_ida_ping1d_t *)rp;
        hdr_ters(&o->hdr, &r->t_meas_ns, &r->seq, &r->flags, &r->inst);
        r->distance = o->u.ping1d.distance;
        r->scan_start = o->u.ping1d.scan_start;
        r->scan_length = o->u.ping1d.scan_length;
        r->ping_number = o->u.ping1d.ping_number;
        r->tx_offset_ns = o->u.ping1d.tx_offset_ns;
        r->confidence = o->u.ping1d.confidence;
        break;
    }
    case MAVLINK_MSG_ID_IDA_BATT: {
        mavlink_ida_batt_t *r = (mavlink_ida_batt_t *)rp;
        hdr_ters(&o->hdr, &r->t_meas_ns, &r->seq, &r->flags, &r->inst);
        r->window_ns = o->hdr.window_ns;
        r->voltage = o->u.batt.voltage;
        r->current = o->u.batt.current;
        r->temp_c = o->u.batt.temp_c;
        break;
    }
    case MAVLINK_MSG_ID_IDA_LEAK: {
        mavlink_ida_leak_t *r = (mavlink_ida_leak_t *)rp;
        hdr_ters(&o->hdr, &r->t_meas_ns, &r->seq, &r->flags, &r->inst);
        r->mask = o->u.leak.mask;
        break;
    }
    default: {
        mavlink_ida_safety_t *r = (mavlink_ida_safety_t *)rp;
        hdr_ters(&o->hdr, &r->t_meas_ns, &r->seq, &r->flags, &r->inst);
        r->estop = o->u.safety.estop;
        r->tube[0] = o->u.safety.tube[0];
        r->tube[1] = o->u.safety.tube[1];
        r->safety_rev = rev;   /* platform tutar (A1-K106) */
        break;
    }
    }
}

static void test_ornekler(void)
{
    static const struct {
        uint32_t msgid;
        size_t boy;
        uint8_t tur;
        uint32_t pencere;   /* 1: window_ns mesajda var */
    } t[8] = {
        {MAVLINK_MSG_ID_IDA_MAG, MAVLINK_MSG_ID_IDA_MAG_LEN, IDA_SENS_MAG, 0u},
        {MAVLINK_MSG_ID_IDA_BARO, MAVLINK_MSG_ID_IDA_BARO_LEN, IDA_SENS_BARO, 1u},
        {MAVLINK_MSG_ID_IDA_GNSS, MAVLINK_MSG_ID_IDA_GNSS_LEN, IDA_SENS_GNSS, 0u},
        {MAVLINK_MSG_ID_IDA_DVL, MAVLINK_MSG_ID_IDA_DVL_LEN, IDA_SENS_DVL, 0u},
        {MAVLINK_MSG_ID_IDA_PING1D, MAVLINK_MSG_ID_IDA_PING1D_LEN, IDA_SENS_PING1D, 0u},
        {MAVLINK_MSG_ID_IDA_BATT, MAVLINK_MSG_ID_IDA_BATT_LEN, IDA_SENS_BATT, 1u},
        {MAVLINK_MSG_ID_IDA_LEAK, MAVLINK_MSG_ID_IDA_LEAK_LEN, IDA_SENS_LEAK, 0u},
        {MAVLINK_MSG_ID_IDA_SAFETY, MAVLINK_MSG_ID_IDA_SAFETY_LEN, IDA_SENS_SAFETY, 0u},
    };
    for (unsigned i = 0; i < 8u; ++i) {
        union {
            uint64_t hiza;   /* tel yapılarının hizası için */
            uint8_t b[IDA_LINK_YUK_AZAMI + 1u];
        } mu, ru;
        uint8_t *m = mu.b;
        uint8_t *r = ru.b;
        ida_sample_t o;
        uint32_t rev = 0u;
        ida_wtime_t trx = {99};
        desen(m, t[i].boy, i + 3u);
        if (t[i].msgid == MAVLINK_MSG_ID_IDA_GNSS) {
            ((mavlink_ida_gnss_t *)(void *)m)->fix_type = (uint8_t)IDA_GNSS_FIX_DGPS;
        }
        if (t[i].msgid == MAVLINK_MSG_ID_IDA_SAFETY) {
            ((mavlink_ida_safety_t *)(void *)m)->estop = 1u;
        }
        TEST_ASSERT_TRUE(ust_esl_ornek(t[i].msgid, m, trx, &o, &rev));
        TEST_ASSERT_EQUAL_UINT8(t[i].tur, o.hdr.type);
        TEST_ASSERT_EQUAL_INT64(99, o.hdr.t_rx.ns);
        if (!t[i].pencere) {
            TEST_ASSERT_EQUAL_UINT32(0u, o.hdr.window_ns);   /* yalnız IMU, BARO, BATT */
        }
        memset(&ru, 0, sizeof ru);
        ornek_ters(t[i].msgid, &o, rev, r);
        TEST_ASSERT_EQUAL_MEMORY(m, r, t[i].boy);
    }
}

static void test_ornek_valid_temizleme(void)
{
    mavlink_ida_gnss_t g;
    mavlink_ida_safety_t s;
    ida_sample_t o;
    ida_wtime_t t = {1};
    memset(&g, 0, sizeof g);
    g.flags = (uint16_t)(IDA_HF_VALID | IDA_HF_SATURATED);
    g.fix_type = 5u;
    TEST_ASSERT_TRUE(ust_esl_ornek(MAVLINK_MSG_ID_IDA_GNSS, &g, t, &o, NULL));
    TEST_ASSERT_EQUAL_UINT16(IDA_HF_SATURATED, o.hdr.flags);   /* A1-K47 */
    TEST_ASSERT_EQUAL_UINT8(5u, o.u.gnss.fix_type);
    memset(&s, 0, sizeof s);
    s.flags = (uint16_t)IDA_HF_VALID;
    s.estop = 2u;
    TEST_ASSERT_TRUE(ust_esl_ornek(MAVLINK_MSG_ID_IDA_SAFETY, &s, t, &o, NULL));
    TEST_ASSERT_EQUAL_UINT16(0u, o.hdr.flags);
    TEST_ASSERT_FALSE(ust_esl_ornek(MAVLINK_MSG_ID_IDA_IMU, &s, t, &o, NULL));
}

static void test_ping_bas(void)
{
    mavlink_ida_ping360_head_t m;
    ida_ping360_t o;
    ida_wtime_t t = {3};
    desen(&m, sizeof m, 11u);
    ust_esl_ping_bas(&m, t, &o);
    TEST_ASSERT_EQUAL_UINT8(IDA_SENS_PING360, o.hdr.type);
    TEST_ASSERT_EQUAL_UINT32(0u, o.hdr.window_ns);
    {
        mavlink_ida_ping360_head_t s;
        memset(&s, 0, sizeof s);
        s.n_frag = m.n_frag;   /* n_frag A4'te yok (birleştirmenin denetimi) */
        hdr_ters(&o.hdr, &s.t_meas_ns, &s.seq, &s.flags, &s.inst);
        s.angle_grad = o.angle_grad;
        s.n_samples = o.n_samples;
        s.sample_period = o.sample_period;
        s.tx_duration_us = o.tx_duration_us;
        s.tx_freq_khz = o.tx_freq_khz;
        s.gain = o.gain;
        TEST_ASSERT_EQUAL_MEMORY(&m, &s, MAVLINK_MSG_ID_IDA_PING360_HEAD_LEN);
    }
}

static void test_algilama(void)
{
    mavlink_ida_det_batch_t b, rb;
    mavlink_ida_det_record_t d, rd;
    ida_det_batch_t ob;
    ida_det_record_t od;
    desen(&b, sizeof b, 13u);
    ust_esl_det_batch(&b, &ob);
    TEST_ASSERT_EQUAL_UINT8(0u, ob.reserved[0]);
    memset(&rb, 0, sizeof rb);
    rb.t_meas_ns = ob.t_meas_ns;
    rb.batch_seq = ob.batch_seq;
    rb.window_ns = ob.window_ns;
    rb.calib_ver = ob.calib_ver;
    rb.producer_ver = ob.producer_ver;
    rb.flags = ob.flags;
    rb.source = ob.source;
    rb.inst = ob.inst;
    rb.status = ob.status;
    rb.count = ob.count;
    rb.producer_id = ob.producer_id;
    TEST_ASSERT_EQUAL_MEMORY(&b, &rb, MAVLINK_MSG_ID_IDA_DET_BATCH_LEN);
    /* Kayıt iki yönde de taşınır (A1 Tablo 3.3: iki yön): kart -> sim eşlemesiyle ters. */
    desen(&d, sizeof d, 17u);
    ust_esl_det_record(&d, &od);
    TEST_ASSERT_EQUAL_UINT8(0u, od.reserved[1]);
    ust_esl_det_tx(&od, &rd);
    TEST_ASSERT_EQUAL_MEMORY(&d, &rd, MAVLINK_MSG_ID_IDA_DET_RECORD_LEN);
}

static void test_olay_sonucu(void)
{
    mavlink_ida_event_result_t m, r;
    ida_event_result_t o;
    desen(&m, sizeof m, 19u);
    ust_esl_evres(&m, &o);
    memset(&r, 0, sizeof r);
    r.sess = m.sess;   /* sess A4'te yok */
    r.event_id = o.event_id;
    r.result = o.result;
    r.t_apply_ns = o.t_apply.ns;
    TEST_ASSERT_EQUAL_MEMORY(&m, &r, MAVLINK_MSG_ID_IDA_EVENT_RESULT_LEN);
}

/* Komutlar (Tablo 3.6). */
static void test_komutlar(void)
{
    ust_komut_t k;
    {
        mavlink_ida_cmd_simple_t m;
        static const struct { uint8_t type; uint16_t arg; } v[5] = {
            {IDA_CMD_SET_MODE, IDA_MODE_AUTO}, {IDA_CMD_MISSION_BEGIN, 77u},
            {IDA_CMD_MISSION_CTL, IDA_MCTL_PAUSE}, {IDA_CMD_PARAM_GET, 0x1234u},
            {IDA_CMD_ARM, 0u}};
        for (unsigned i = 0; i < 5u; ++i) {
            mavlink_ida_cmd_simple_t r;
            desen(&m, sizeof m, 23u + i);
            m.type = v[i].type;
            m.arg = v[i].arg;
            TEST_ASSERT_EQUAL_INT(UST_KOMUT_OLAGAN,
                                  ust_esl_komut(MAVLINK_MSG_ID_IDA_CMD_SIMPLE, &m, &k));
            memset(&r, 0, sizeof r);
            r.sess = m.sess;
            r.cmd_id = k.cmd.cmd_id;
            r.type = k.cmd.type;
            r.source = k.cmd.source;
            switch (k.cmd.type) {
            case IDA_CMD_SET_MODE: r.arg = k.cmd.u.set_mode.mode; break;
            case IDA_CMD_MISSION_BEGIN: r.arg = k.cmd.u.mission_begin.count; break;
            case IDA_CMD_MISSION_CTL: r.arg = k.cmd.u.mission_ctl.action; break;
            case IDA_CMD_PARAM_GET: r.arg = k.cmd.u.param.id; break;
            default: r.arg = 0u; break;
            }
            TEST_ASSERT_EQUAL_MEMORY(&m, &r, MAVLINK_MSG_ID_IDA_CMD_SIMPLE_LEN);
        }
        TEST_ASSERT_EQUAL_UINT8(1u, k.genisletici);   /* ARM */
        m.type = (uint8_t)IDA_CMD_DISARM;
        TEST_ASSERT_EQUAL_INT(UST_KOMUT_DARALTICI,
                              ust_esl_komut(MAVLINK_MSG_ID_IDA_CMD_SIMPLE, &m, &k));
        TEST_ASSERT_EQUAL_UINT8(IDA_NARROW_DISARM, k.bit);
        TEST_ASSERT_EQUAL_UINT32(m.cmd_id, k.cmd_id);
        m.type = (uint8_t)IDA_CMD_SET_MODE;
        m.arg = (uint16_t)IDA_MODE_STATION_KEEP;
        TEST_ASSERT_EQUAL_INT(UST_KOMUT_DARALTICI,
                              ust_esl_komut(MAVLINK_MSG_ID_IDA_CMD_SIMPLE, &m, &k));
        TEST_ASSERT_EQUAL_UINT8(IDA_NARROW_MODE, k.bit);
        TEST_ASSERT_EQUAL_UINT8(IDA_MODE_STATION_KEEP, k.kip);
    }
    {
        mavlink_ida_cmd_manual_t m, r;
        desen(&m, sizeof m, 31u);
        TEST_ASSERT_EQUAL_INT(UST_KOMUT_OLAGAN, ust_esl_komut(MAVLINK_MSG_ID_IDA_CMD_MANUAL, &m, &k));
        TEST_ASSERT_EQUAL_UINT8(IDA_CMD_MANUAL, k.cmd.type);
        memset(&r, 0, sizeof r);
        r.sess = m.sess;
        r.cmd_id = k.cmd.cmd_id;
        r.source = k.cmd.source;
        memcpy(r.axes, k.cmd.u.manual.axes, sizeof r.axes);
        TEST_ASSERT_EQUAL_MEMORY(&m, &r, MAVLINK_MSG_ID_IDA_CMD_MANUAL_LEN);
    }
    {
        mavlink_ida_cmd_guided_t m, r;
        desen(&m, sizeof m, 37u);
        TEST_ASSERT_EQUAL_INT(UST_KOMUT_OLAGAN, ust_esl_komut(MAVLINK_MSG_ID_IDA_CMD_GUIDED, &m, &k));
        TEST_ASSERT_EQUAL_UINT8(IDA_CMD_GUIDED_SP, k.cmd.type);
        memset(&r, 0, sizeof r);
        r.sess = m.sess;
        r.cmd_id = k.cmd.cmd_id;
        r.source = k.cmd.source;
        r.mask = k.cmd.u.guided.mask;
        memcpy(r.pos_ne, k.cmd.u.guided.pos_ne, sizeof r.pos_ne);
        r.depth = k.cmd.u.guided.depth;
        r.altitude = k.cmd.u.guided.altitude;
        memcpy(r.vel_ned, k.cmd.u.guided.vel_ned, sizeof r.vel_ned);
        r.yaw = k.cmd.u.guided.yaw;
        r.yaw_rate = k.cmd.u.guided.yaw_rate;
        TEST_ASSERT_EQUAL_MEMORY(&m, &r, MAVLINK_MSG_ID_IDA_CMD_GUIDED_LEN);
    }
    {
        mavlink_ida_cmd_mission_item_t m, r;
        memset(&m, 0, sizeof m);
        m.cmd_id = 0x01020304u;
        m.sess = 9u;
        m.source = 255u;
        m.index = 17u;
        m.item_type = (uint8_t)IDA_MI_SCAN_LINES;
        m.frame = (uint8_t)IDA_MI_FRAME_GLOBAL;
        m.p[0] = 100.0f;
        m.p[1] = 2.5f;
        m.p[2] = 12.0f;
        m.p[3] = -1.5f;
        m.lat_e7 = 410000000;
        m.lon_e7 = 290000000;
        m.depth = 4.0f;
        TEST_ASSERT_EQUAL_INT(UST_KOMUT_OLAGAN,
                              ust_esl_komut(MAVLINK_MSG_ID_IDA_CMD_MISSION_ITEM, &m, &k));
        memset(&r, 0, sizeof r);
        r.sess = m.sess;
        r.cmd_id = k.cmd.cmd_id;
        r.source = k.cmd.source;
        r.index = k.cmd.u.mission_item.index;
        r.item_type = k.cmd.u.mission_item.item_type;
        r.frame = k.cmd.u.mission_item.frame;
        memcpy(r.p, k.cmd.u.mission_item.p, sizeof r.p);
        r.lat_e7 = k.cmd.u.mission_item.lat_e7;
        r.lon_e7 = k.cmd.u.mission_item.lon_e7;
        r.north_m = k.cmd.u.mission_item.north_m;
        r.east_m = k.cmd.u.mission_item.east_m;
        r.depth = k.cmd.u.mission_item.depth;
        TEST_ASSERT_EQUAL_MEMORY(&m, &r, MAVLINK_MSG_ID_IDA_CMD_MISSION_ITEM_LEN);
        m.p[2] = 12.5f;   /* tamsayı değerli değil */
        TEST_ASSERT_EQUAL_INT(UST_KOMUT_BOZUK,
                              ust_esl_komut(MAVLINK_MSG_ID_IDA_CMD_MISSION_ITEM, &m, &k));
    }
    {
        mavlink_ida_cmd_param_set_t m, r;
        desen(&m, sizeof m, 41u);
        m.param_type = (uint8_t)IDA_PT_F32;
        TEST_ASSERT_EQUAL_INT(UST_KOMUT_OLAGAN,
                              ust_esl_komut(MAVLINK_MSG_ID_IDA_CMD_PARAM_SET, &m, &k));
        memset(&r, 0, sizeof r);
        r.sess = m.sess;
        r.cmd_id = k.cmd.cmd_id;
        r.source = k.cmd.source;
        r.param_id = k.cmd.u.param.id;
        r.param_type = k.cmd.u.param.type;
        r.value_bits = k.cmd.u.param.v.u32;
        TEST_ASSERT_EQUAL_MEMORY(&m, &r, MAVLINK_MSG_ID_IDA_CMD_PARAM_SET_LEN);
    }
    {
        mavlink_ida_cmd_fc_t m, r;
        memset(&m, 0, sizeof m);
        m.cmd_id = 0x0A0B0C0Du;
        m.sess = 4u;
        m.source = 250u;
        m.op = (uint8_t)IDA_FC_TEST_EXIT;
        m.tube = 1u;
        TEST_ASSERT_EQUAL_INT(UST_KOMUT_OLAGAN, ust_esl_komut(MAVLINK_MSG_ID_IDA_CMD_FC, &m, &k));
        TEST_ASSERT_EQUAL_UINT8(0u, k.genisletici);   /* nötr */
        memset(&r, 0, sizeof r);
        r.sess = m.sess;
        r.cmd_id = k.cmd.cmd_id;
        r.source = k.cmd.source;
        r.op = k.cmd.u.fc.op;
        r.tube = k.cmd.u.fc.tube;
        r.arg0 = k.cmd.u.fc.arg0;
        r.arg1 = k.cmd.u.fc.arg1;
        TEST_ASSERT_EQUAL_MEMORY(&m, &r, MAVLINK_MSG_ID_IDA_CMD_FC_LEN);
        m.op = (uint8_t)IDA_FC_FIRE;
        TEST_ASSERT_EQUAL_INT(UST_KOMUT_OLAGAN, ust_esl_komut(MAVLINK_MSG_ID_IDA_CMD_FC, &m, &k));
        TEST_ASSERT_EQUAL_UINT8(1u, k.genisletici);
        m.op = (uint8_t)IDA_FC_DISARM;
        m.tube = (uint8_t)IDA_TUBE_ALL;
        TEST_ASSERT_EQUAL_INT(UST_KOMUT_DARALTICI, ust_esl_komut(MAVLINK_MSG_ID_IDA_CMD_FC, &m, &k));
        TEST_ASSERT_EQUAL_UINT8(IDA_NARROW_FC_DISARM, k.bit);
    }
}

/* ------------------------------------------------------------------ */
/* kart -> sim                                                        */
/* ------------------------------------------------------------------ */

static void test_olay(void)
{
    ida_event_t e, e2;
    mavlink_ida_event_t m, m2;
    desen(&e, sizeof e, 43u);
    ust_esl_event(&e, 3u, 0x55667788u, 0x99AAu, &m);
    sifirsiz(&m, MAVLINK_MSG_ID_IDA_EVENT_LEN);
    memset(&e2, 0, sizeof e2);
    e2.event_id = m.event_id;
    e2.t_decide.ns = m.t_decide_ns;
    e2.seq = m.batch_seq;
    e2.apply_by_ns = m.apply_by_ns;
    e2.hold_ns = m.hold_ns;
    e2.kind = m.kind;
    e2.tube = m.tube;
    ust_esl_event(&e2, m.call_kind, m.safety_rev, m.sess, &m2);
    TEST_ASSERT_EQUAL_MEMORY(&m, &m2, MAVLINK_MSG_ID_IDA_EVENT_LEN);
    TEST_ASSERT_TRUE(m.event_id == e.event_id);   /* ilk 4 bayt ctr, sonraki 4 boot_count */
}

static void test_komut_sonucu(void)
{
    ida_cmd_result_t r, r2;
    mavlink_ida_cmd_result_t m, m2;
    static const uint8_t blob[4] = {1u, 2u, 3u, 4u};
    memset(&r, 0, sizeof r);
    r.cmd_id = 0x11223344u;
    r.kind = (uint8_t)IDA_RES_PARAM_SAVE;
    r.ack = (uint8_t)IDA_ACK_ACCEPTED;
    r.reason = 0x0101u;
    r.value.id = 0x2222u;
    r.value.type = 3u;
    r.value.v.u32 = 0x33445566u;
    r.save.gen = 0x77777777u;
    r.save.blob = blob;
    r.save.len = 0x1F40u;
    ust_esl_cmd_result(&r, 0x4444u, 0x88888888u, &m);
    sifirsiz(&m, MAVLINK_MSG_ID_IDA_CMD_RESULT_LEN);
    memset(&r2, 0, sizeof r2);
    r2.cmd_id = m.cmd_id;
    r2.kind = m.kind;
    r2.ack = m.ack;
    r2.reason = m.reason;
    r2.value.id = m.param_id;
    r2.value.type = m.param_type;
    r2.value.v.u32 = m.value_bits;
    r2.save.gen = m.save_gen;
    r2.save.len = m.save_len;
    ust_esl_cmd_result(&r2, m.sess, m.save_crc, &m2);
    TEST_ASSERT_EQUAL_MEMORY(&m, &m2, MAVLINK_MSG_ID_IDA_CMD_RESULT_LEN);
}

static void test_nav(void)
{
    ida_nav_out_t n, n2;
    mavlink_ida_nav_t m, m2;
    desen(&n, sizeof n, 47u);
    ust_esl_nav(&n, 0x12345678u, &m);
    sifirsiz(&m, MAVLINK_MSG_ID_IDA_NAV_LEN);
    memset(&n2, 0, sizeof n2);
    n2.t.ns = m.t_ns;
    memcpy(n2.p_ned, m.p_ned, sizeof n2.p_ned);
    memcpy(n2.v_ned, m.v_ned, sizeof n2.v_ned);
    memcpy(n2.q_nb, m.q_nb, sizeof n2.q_nb);
    memcpy(n2.omega_b, m.omega_b, sizeof n2.omega_b);
    n2.depth = m.depth;
    n2.altitude = m.altitude;
    memcpy(n2.p_sigma, m.p_sigma, sizeof n2.p_sigma);
    memcpy(n2.v_sigma, m.v_sigma, sizeof n2.v_sigma);
    memcpy(n2.att_sigma, m.att_sigma, sizeof n2.att_sigma);
    n2.origin_lat_e7 = m.origin_lat_e7;
    n2.origin_lon_e7 = m.origin_lon_e7;
    n2.est_level = m.est_level;
    n2.reset_counter = m.reset_counter;
    ust_esl_nav(&n2, m.batch_seq, &m2);
    TEST_ASSERT_EQUAL_MEMORY(&m, &m2, MAVLINK_MSG_ID_IDA_NAV_LEN);
}

static void test_durum(void)
{
    ida_status_t s, s2;
    ida_health_t h, h2;
    ida_plat_t p, p2;
    mavlink_ida_status_t m, m2;
    memset(&s, 0, sizeof s);
    s.mode = 6u;
    s.armed = true;
    s.fs_level = 2u;
    s.fs_reasons = 0x01020304u;
    s.regime = 2u;
    s.phase = 3u;
    s.mission_state = 4u;
    s.mission_item = 0x0505u;
    s.param_ok = true;
    s.events_ok = true;
    s.boot_count = 0x06060606u;
    memset(&h, 0, sizeof h);
    h.core_ok = h.data_ok = h.imu_rejected = h.imu_lag = true;
    h.input_dropped = h.narrowed = h.sample_conflict = h.bg_bad_input = true;
    memset(&p, 0, sizeof p);
    p.param_store = 2u;
    p.param_fail = 3u;
    p.param_gen = 0x07070707u;
    ust_esl_status(&s, &h, &p, 2u, 0x08080808u, 0x09090909u, 0x0A0Au, &m);
    sifirsiz(&m, MAVLINK_MSG_ID_IDA_STATUS_LEN);
    TEST_ASSERT_EQUAL_UINT8(0xFFu, m.health);
    memset(&s2, 0, sizeof s2);
    memset(&h2, 0, sizeof h2);
    memset(&p2, 0, sizeof p2);
    s2.mode = m.mode;
    s2.armed = m.armed != 0u;
    s2.fs_level = m.fs_level;
    s2.fs_reasons = m.fs_reasons;
    s2.regime = m.regime;
    s2.phase = m.phase;
    s2.mission_state = m.mission_state;
    s2.mission_item = m.mission_item;
    s2.param_ok = m.param_ok != 0u;
    s2.events_ok = m.events_ok != 0u;
    s2.boot_count = m.boot_count;
    h2.core_ok = (m.health & 1u) != 0u;
    h2.data_ok = (m.health & 2u) != 0u;
    h2.imu_rejected = (m.health & 4u) != 0u;
    h2.imu_lag = (m.health & 8u) != 0u;
    h2.input_dropped = (m.health & 16u) != 0u;
    h2.narrowed = (m.health & 32u) != 0u;
    h2.sample_conflict = (m.health & 64u) != 0u;
    h2.bg_bad_input = (m.health & 128u) != 0u;
    p2.param_store = m.param_store;
    p2.param_fail = m.param_fail;
    p2.param_gen = m.param_gen;
    ust_esl_status(&s2, &h2, &p2, m.link_state, m.param_blob_crc, m.batch_seq, m.sess, &m2);
    TEST_ASSERT_EQUAL_MEMORY(&m, &m2, MAVLINK_MSG_ID_IDA_STATUS_LEN);
    /* health bitleri tek tek (A1 §3.7). */
    memset(&h2, 0, sizeof h2);
    h2.narrowed = true;
    TEST_ASSERT_EQUAL_UINT8(IDAW_HEALTH_NARROWED, ust_esl_health(&h2));
}

static void test_fc_gunluk_sayaclar(void)
{
    {
        ida_fc_status_t f, f2;
        mavlink_ida_fc_status_t m, m2;
        desen(&f, sizeof f, 53u);
        ust_esl_fc_status(&f, &m);
        sifirsiz(&m, MAVLINK_MSG_ID_IDA_FC_STATUS_LEN);
        memset(&f2, 0, sizeof f2);
        f2.state = m.state;
        f2.inhibit_mask = m.inhibit_mask;
        f2.last_event_id = m.last_event_id;
        f2.last_event_result = m.last_event_result;
        ust_esl_fc_status(&f2, &m2);
        TEST_ASSERT_EQUAL_MEMORY(&m, &m2, MAVLINK_MSG_ID_IDA_FC_STATUS_LEN);
    }
    {
        ida_log_t g, g2;
        mavlink_ida_log_t m, m2;
        desen(&g, sizeof g, 59u);
        ust_esl_log(&g, &m);
        sifirsiz(&m, MAVLINK_MSG_ID_IDA_LOG_LEN);
        memset(&g2, 0, sizeof g2);
        g2.t_w.ns = m.t_w_ns;
        g2.code = m.code;
        g2.severity = m.severity;
        g2.arg0 = m.arg0;
        g2.arg1 = m.arg1;
        ust_esl_log(&g2, &m2);
        TEST_ASSERT_EQUAL_MEMORY(&m, &m2, MAVLINK_MSG_ID_IDA_LOG_LEN);
    }
    {
        /* 22 sayaç ada göre; A4 yapısı ve tel yapısı aynı ad sırasında 22 uint32. */
        ida_core_counters_t c;
        mavlink_ida_core_counters_t m;
        uint32_t v[22];
        for (uint32_t i = 0; i < 22u; ++i) {
            v[i] = 0x01010101u * (i + 1u);   /* her bayt ≠ 0 */
        }
        TEST_ASSERT_EQUAL_UINT(sizeof v, sizeof c);
        memcpy(&c, v, sizeof c);
        ust_esl_core_counters(&c, &m);
        sifirsiz(&m, MAVLINK_MSG_ID_IDA_CORE_COUNTERS_LEN);
        TEST_ASSERT_EQUAL_UINT32(c.sample_rejected, m.sample_rejected);
        TEST_ASSERT_EQUAL_UINT32(c.narrow_from_rejected, m.narrow_from_rejected);
        TEST_ASSERT_EQUAL_UINT32(c.widen_blocked, m.widen_blocked);
        TEST_ASSERT_EQUAL_UINT32(c.step_miss, m.step_miss);
        TEST_ASSERT_EQUAL_MEMORY(&c, &m, MAVLINK_MSG_ID_IDA_CORE_COUNTERS_LEN);   /* tel sırası da aynı (hepsi u32) */
    }
}

static void test_board_hello(void)
{
    ida_link_ust_ayar_t a;
    mavlink_ida_board_hello_t m;
    memset(&a, 0, sizeof a);
    a.rol = IDA_LINK_UST_ROL_SIL;
    a.boot_count = 11u;
    a.boot_count_ok = true;
    a.reset_cause = (uint8_t)IDA_RESET_SIL_START;
    a.reset_flags = 0xF0F0u;
    a.consecutive_faults = 2u;
    a.param_src = (uint8_t)IDA_PSRC_SIL;
    a.cekirdek.port_major = 1u;
    a.cekirdek.port_minor = 0u;
    a.cekirdek.param_schema_hash = UINT64_C(0xDEADBEEF01020304);
    a.cekirdek.core_version = 0x000100u;
    a.fw_version = 5u;
    a.fw_git = 0x1234ABCDu;
    ust_esl_board_hello(&a, 7u, 8u, (uint8_t)IDA_RC_PARAM_DEFAULTS, 9u, 10u, 12u, 13u, 14u, 15u,
                        &m);
    TEST_ASSERT_TRUE(m.dialect_hash == IDA_LINK_DIALECT_HASH);
    TEST_ASSERT_TRUE(m.param_schema_hash == a.cekirdek.param_schema_hash);
    TEST_ASSERT_EQUAL_UINT32(7u, m.peer_nonce);
    TEST_ASSERT_EQUAL_UINT32(8u, m.nonce);
    TEST_ASSERT_EQUAL_UINT32(11u, m.boot_count);
    TEST_ASSERT_EQUAL_UINT8(1u, m.boot_count_ok);
    TEST_ASSERT_EQUAL_UINT8(IDA_RESET_SIL_START, m.reset_cause);
    TEST_ASSERT_EQUAL_UINT32(0xF0F0u, m.reset_flags);
    TEST_ASSERT_EQUAL_UINT8(2u, m.consecutive_faults);
    TEST_ASSERT_EQUAL_UINT8(IDA_PSRC_SIL, m.param_src);
    TEST_ASSERT_EQUAL_UINT8(IDA_RC_PARAM_DEFAULTS, m.init_rc);
    TEST_ASSERT_EQUAL_UINT16(1u, m.port_major);
    TEST_ASSERT_EQUAL_UINT32(0x000100u, m.core_version);
    TEST_ASSERT_EQUAL_UINT32(5u, m.fw_version);
    TEST_ASSERT_EQUAL_UINT32(0x1234ABCDu, m.fw_git);
    TEST_ASSERT_EQUAL_UINT32(9u, m.param_blob_crc);
    TEST_ASSERT_EQUAL_UINT16(10u, m.param_blob_len);
    TEST_ASSERT_EQUAL_UINT32(12u, m.param_gen);
    TEST_ASSERT_EQUAL_UINT32(13u, m.last_cmd_id);
    TEST_ASSERT_EQUAL_UINT16(14u, m.prev_sess);
    TEST_ASSERT_EQUAL_UINT16(15u, m.sessions_since_boot);
    TEST_ASSERT_EQUAL_UINT8(IDAW_ROLE_SIL, m.role);
    TEST_ASSERT_EQUAL_UINT16(IDA_DET_SCHEMA_MAJOR, m.det_schema_major);
    TEST_ASSERT_EQUAL_UINT16(IDA_DET_SCHEMA_MINOR, m.det_schema_minor);
}

static void test_pwm(void)
{
    TEST_ASSERT_EQUAL_UINT16(1500u, ust_esl_pwm(0.0f, 1100u, 1500u, 1900u));
    TEST_ASSERT_EQUAL_UINT16(1900u, ust_esl_pwm(1.0f, 1100u, 1500u, 1900u));
    TEST_ASSERT_EQUAL_UINT16(1100u, ust_esl_pwm(-1.0f, 1100u, 1500u, 1900u));
    TEST_ASSERT_EQUAL_UINT16(1300u, ust_esl_pwm(-0.5f, 1100u, 1500u, 1900u));
    TEST_ASSERT_EQUAL_UINT16(1540u, ust_esl_pwm(0.1f, 1100u, 1500u, 1900u));
    TEST_ASSERT_EQUAL_UINT16(1600u, ust_esl_pwm(0.5f, 1000u, 1400u, 1800u));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_tf);
    RUN_TEST(test_imu);
    RUN_TEST(test_ornekler);
    RUN_TEST(test_ornek_valid_temizleme);
    RUN_TEST(test_ping_bas);
    RUN_TEST(test_algilama);
    RUN_TEST(test_olay_sonucu);
    RUN_TEST(test_komutlar);
    RUN_TEST(test_olay);
    RUN_TEST(test_komut_sonucu);
    RUN_TEST(test_nav);
    RUN_TEST(test_durum);
    RUN_TEST(test_fc_gunluk_sayaclar);
    RUN_TEST(test_board_hello);
    RUN_TEST(test_pwm);
    return UNITY_END();
}
