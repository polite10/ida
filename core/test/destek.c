/*
 * destek.c — çekirdek birim testlerinin ortak yardımcıları.
 */
#include "destek.h"

#include <string.h>

#include "unity.h"

static uint8_t g_blok[IDA_PRM_BLOB_BOYU];

void ds_init_girdisi(ida_init_in_t *ii)
{
    (void)memset(ii, 0, sizeof(*ii));
    ii->port_major = (uint16_t)IDA_PORT_VERSION_MAJOR;
    ii->port_minor = (uint16_t)IDA_PORT_VERSION_MINOR;
    ii->now_w.ns = 1000 * DS_MS;
    ii->boot_count = 7u;
    ii->boot_count_ok = true;
    ii->reset_cause = (uint8_t)IDA_RESET_SIL_START;
    ii->param_src = (uint8_t)IDA_PSRC_SIL;
}

uint32_t ds_gecerli_blok(uint8_t *blok)
{
    ida_prm_kume_t k;
    ida_prm_varsayilan(&k);
    return ida_prm_blok_kodla(&k, ida_prm_sema_ozeti(), blok);
}

ida_rc_t ds_kur(ida_core_t *c)
{
    ida_init_in_t ii;
    ds_init_girdisi(&ii);
    ii.param_len = ds_gecerli_blok(g_blok);
    ii.param_blob = g_blok;
    return ida_core_init(c, &ii);
}

ida_rc_t ds_kur_bloksuz(ida_core_t *c)
{
    ida_init_in_t ii;
    ds_init_girdisi(&ii);
    return ida_core_init(c, &ii);
}

void ds_imu(ida_imu_sample_t *s, int64_t t_ns, uint32_t seq)
{
    (void)memset(s, 0, sizeof(*s));
    s->hdr.t_meas.ns = t_ns;
    s->hdr.t_rx.ns = t_ns;
    s->hdr.seq = seq;
    s->hdr.window_ns = 5000000u;
    s->hdr.flags = (uint16_t)IDA_HF_VALID;
    s->hdr.type = (uint8_t)IDA_SENS_IMU;
    s->hdr.inst = 0u;
    s->imu.accel[2] = -9.81f;
    s->imu.temp_c = 20.0f;
}

void ds_girdi(ida_in_t *in, int64_t t_ns, uint32_t seq, bool step)
{
    (void)memset(in, 0, sizeof(*in));
    in->now_w.ns = t_ns;
    in->tf.present = true;
    in->tf.seq = seq;
    in->tf.t.ns = t_ns;
    in->tf.t_rx.ns = t_ns;
    in->imu_present = step;
    if (step) {
        ds_imu(&in->imu, t_ns, seq);
    }
    in->plat.link_state = (uint8_t)IDA_LINK_RUNNING;
}

void ds_emniyet(ida_sample_t *s, int64_t t_ns, uint32_t seq)
{
    (void)memset(s, 0, sizeof(*s));
    s->hdr.t_meas.ns = t_ns;
    s->hdr.t_rx.ns = t_ns;
    s->hdr.seq = seq;
    s->hdr.flags = (uint16_t)IDA_HF_VALID;
    s->hdr.type = (uint8_t)IDA_SENS_SAFETY;
    s->u.safety.estop = 0u;
    s->u.safety.tube[0] = (uint8_t)(IDA_TUBE_LOADED | IDA_TUBE_PLUG_IN | IDA_TUBE_PIN_OUT);
    s->u.safety.tube[1] = (uint8_t)(IDA_TUBE_LOADED | IDA_TUBE_PLUG_IN | IDA_TUBE_PIN_OUT);
}

void ds_mag(ida_sample_t *s, int64_t t_ns, uint32_t seq)
{
    (void)memset(s, 0, sizeof(*s));
    s->hdr.t_meas.ns = t_ns;
    s->hdr.t_rx.ns = t_ns;
    s->hdr.seq = seq;
    s->hdr.flags = (uint16_t)IDA_HF_VALID;
    s->hdr.type = (uint8_t)IDA_SENS_MAG;
    s->u.mag.field[0] = 2.0e-5f;
}

float ds_nan(void)
{
    const uint32_t bit = 0x7FC00000u;
    float f = 0.0f;
    (void)memcpy(&f, &bit, sizeof(f));
    return f;
}

float ds_sonsuz(void)
{
    const uint32_t bit = 0x7F800000u;
    float f = 0.0f;
    (void)memcpy(&f, &bit, sizeof(f));
    return f;
}

void ds_desen(ida_out_t *out)
{
    (void)memset(out, 0xA5, sizeof(*out));
}

static bool sifir_mi(const void *p, size_t n)
{
    const uint8_t *b = (const uint8_t *)p;
    for (size_t i = 0u; i < n; i++) {
        if (b[i] != 0u) {
            return false;
        }
    }
    return true;
}

void ds_cikti_degismezleri(ida_rc_t rc, const ida_in_t *in, const ida_out_t *out)
{
    const bool hata = (rc == IDA_RC_BAD_INPUT) || (rc == IDA_RC_DUP_SEQ) ||
                      (rc == IDA_RC_OLD_SEQ) || (rc == IDA_RC_NOT_INIT);
    /* A4-K28 */
    if (hata) {
        TEST_ASSERT_FALSE(out->thr_valid);
        TEST_ASSERT_EQUAL_UINT32(0u, out->valid_for_ns);
    }
    /* A4-K55, K56, K58 */
    for (uint32_t i = 0u; i < IDA_PORT_N_THR; i++) {
        if (!out->thr_valid || out->is_safe) {
            TEST_ASSERT_EQUAL_UINT32(0u, (uint32_t)(out->thr_cmd[i] != 0.0f));
        }
    }
    if (out->thr_valid) {
        TEST_ASSERT_TRUE(out->valid_for_ns >= IDA_VALID_FOR_MIN_NS);
        TEST_ASSERT_TRUE(out->valid_for_ns <= IDA_VALID_FOR_MAX_NS);
    } else {
        TEST_ASSERT_EQUAL_UINT32(0u, out->valid_for_ns);
    }
    TEST_ASSERT_TRUE((out->src == (uint8_t)IDA_SRC_STEP) || (out->src == (uint8_t)IDA_SRC_TICK));
    TEST_ASSERT_TRUE(out->fs_level <= (uint8_t)IDA_FS_DISARM);
    /* A4-K64, K65: genişletici yalnız step OK'ta; SAFETY_CLOSE'lar önce. */
    TEST_ASSERT_TRUE(out->n_events <= IDA_PORT_MAX_EVENTS);
    bool kapat_bitti = false;
    for (uint32_t i = 0u; i < (uint32_t)out->n_events; i++) {
        const ida_event_t *e = &out->events[i];
        TEST_ASSERT_TRUE((e->kind >= 1u) && (e->kind <= 3u));
        if (e->kind == (uint8_t)IDA_EVT_SAFETY_CLOSE) {
            TEST_ASSERT_FALSE(kapat_bitti);
            TEST_ASSERT_EQUAL_UINT32(0u, e->hold_ns);
        } else {
            kapat_bitti = true;
            TEST_ASSERT_TRUE((rc == IDA_RC_OK) && (out->src == (uint8_t)IDA_SRC_STEP));
        }
        TEST_ASSERT_EQUAL_UINT32(out->seq, e->seq); /* A4-K57 */
        TEST_ASSERT_TRUE(e->t_decide.ns == out->t_ref.ns);
    }
    for (uint32_t i = out->n_events; i < IDA_PORT_MAX_EVENTS; i++) {
        TEST_ASSERT_TRUE(sifir_mi(&out->events[i], sizeof(out->events[i]))); /* A4-K121 */
    }
    /* A4-K43, K120, K121 */
    TEST_ASSERT_TRUE(out->n_results <= IDA_PORT_MAX_RESULTS);
    for (uint32_t i = 0u; i < (uint32_t)out->n_results; i++) {
        const ida_cmd_result_t *r = &out->results[i];
        TEST_ASSERT_TRUE(r->reason <= IDA_REASON_CORE_LAST);
        TEST_ASSERT_TRUE((r->ack >= 1u) && (r->ack <= 5u));
        TEST_ASSERT_TRUE((r->kind >= 1u) && (r->kind <= 3u));
        if (r->kind != (uint8_t)IDA_RES_PARAM_VALUE) {
            TEST_ASSERT_TRUE(sifir_mi(&r->value, sizeof(r->value)));
        }
        if ((r->kind != (uint8_t)IDA_RES_PARAM_SAVE) || (r->ack != (uint8_t)IDA_ACK_ACCEPTED)) {
            TEST_ASSERT_TRUE(r->save.blob == NULL);
            TEST_ASSERT_EQUAL_UINT32(0u, r->save.len);
        }
    }
    for (uint32_t i = out->n_results; i < IDA_PORT_MAX_RESULTS; i++) {
        TEST_ASSERT_TRUE(sifir_mi(&out->results[i], sizeof(out->results[i])));
    }
    /* A4-K87 */
    if ((rc == IDA_RC_OK) && (out->src == (uint8_t)IDA_SRC_STEP)) {
        TEST_ASSERT_TRUE(out->nav_valid);
    } else {
        TEST_ASSERT_FALSE(out->nav_valid);
        TEST_ASSERT_TRUE(sifir_mi(&out->nav, sizeof(out->nav)));
    }
    /* A4-K90 */
    TEST_ASSERT_TRUE(out->n_logs <= IDA_PORT_MAX_LOGS);
    for (uint32_t i = 0u; i < (uint32_t)out->n_logs; i++) {
        TEST_ASSERT_TRUE((out->logs[i].code >= 1u) && (out->logs[i].code <= IDA_LOG_CODE_LAST));
        TEST_ASSERT_TRUE(out->logs[i].severity <= (uint8_t)IDA_SEV_DEBUG);
    }
    for (uint32_t i = out->n_logs; i < IDA_PORT_MAX_LOGS; i++) {
        TEST_ASSERT_TRUE(sifir_mi(&out->logs[i], sizeof(out->logs[i])));
    }
    /* A4-K91 */
    TEST_ASSERT_TRUE(out->fc.state <= (uint8_t)IDA_FCS_FAULT_LATCHED);
    TEST_ASSERT_EQUAL_UINT32(0u, out->fc.inhibit_mask & ~(uint32_t)IDA_FCI_DEFINED);
    /* A4-K92 */
    if (!((rc == IDA_RC_OK) && (out->src == (uint8_t)IDA_SRC_STEP))) {
        TEST_ASSERT_EQUAL_UINT8(0u, out->n_det_tx);
        TEST_ASSERT_FALSE(out->bg.submitted);
    }
    for (uint32_t i = out->n_det_tx; i < IDA_PORT_MAX_DET_OUT; i++) {
        TEST_ASSERT_TRUE(sifir_mi(&out->det_tx[i], sizeof(out->det_tx[i])));
    }
    (void)in;
}

const ida_cmd_result_t *ds_sonuc(const ida_out_t *out, uint32_t cmd_id)
{
    for (uint32_t i = 0u; i < (uint32_t)out->n_results; i++) {
        if (out->results[i].cmd_id == cmd_id) {
            return &out->results[i];
        }
    }
    return NULL;
}

uint32_t ds_gunluk_say(const ida_out_t *out, uint16_t kod)
{
    uint32_t n = 0u;
    for (uint32_t i = 0u; i < (uint32_t)out->n_logs; i++) {
        if (out->logs[i].code == kod) {
            n++;
        }
    }
    return n;
}

uint32_t ds_olay_say(const ida_out_t *out, uint8_t kind)
{
    uint32_t n = 0u;
    for (uint32_t i = 0u; i < (uint32_t)out->n_events; i++) {
        if (out->events[i].kind == kind) {
            n++;
        }
    }
    return n;
}

uint32_t ds_rastgele(ds_rastgele_t *r)
{
    uint32_t x = r->s;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    r->s = x;
    return x;
}
