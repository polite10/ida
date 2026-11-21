/*
 * destek_oturum.c — ida_link üst katmanı birim testlerinin sahte platformu ve sahte sim'i.
 */
#include "destek_oturum.h"

#include "unity.h"

#include <string.h>

dt_t DT;

/* ------------------------------------------------------------------ */
/* Sahte platform                                                     */
/* ------------------------------------------------------------------ */

static ida_wtime_t p_zaman(void *ctx)
{
    ida_wtime_t w;
    (void)ctx;
    w.ns = DT.saat;
    return w;
}

static bool p_gonder(void *ctx, ida_link_sinif_t sinif, const uint8_t *cerceve, size_t n)
{
    size_t t = 0u;
    ida_link_cerceve_t c;
    ida_link_mesaj_bilgisi_t b;
    (void)ctx;
    TEST_ASSERT_TRUE(sinif >= IDA_LINK_SINIF_KRITIK && sinif <= IDA_LINK_SINIF_KAYBEDILEBILIR);
    if (DT.dolu[sinif]) {
        return false;
    }
    /* Her gönderim tam bir geçerli çerçevedir (A1-K10). */
    TEST_ASSERT_EQUAL_INT(IDA_LINK_CERCEVE, ida_link_al(&DT.sim_alici, cerceve, n, &t, &c));
    TEST_ASSERT_EQUAL_UINT32((uint32_t)n, (uint32_t)t);
    TEST_ASSERT_EQUAL_INT(IDA_LINK_TAMAM, ida_link_mesaj_bilgisi(c.msgid, &b));
    TEST_ASSERT_TRUE(DT.n < DT_KAYIT);
    DT.kayit[DT.n].msgid = c.msgid;
    DT.kayit[DT.n].sinif = sinif;
    TEST_ASSERT_EQUAL_INT(IDA_LINK_TAMAM,
                          ida_link_coz(&c, c.msgid, DT.kayit[DT.n].yuk, b.yapi_boyu));
    DT.n += 1u;
    return true;
}

static void p_temizle(void *ctx)
{
    (void)ctx;
    DT.temizleme += 1u;
    memset(DT.dolu, 0, sizeof DT.dolu);   /* kuyruk boşaldı */
}

static uint32_t p_nonce(void *ctx)
{
    (void)ctx;
    DT.nonce_sayac += 1u;
    return 0x1000u + DT.nonce_sayac;
}

static void p_yeniden(void *ctx)
{
    (void)ctx;
    DT.yeniden_baslatma += 1u;
}

static void p_blok(void *ctx, uint32_t *crc, uint32_t *len, uint32_t *gen)
{
    (void)ctx;
    *crc = DT.blok_crc;
    *len = DT.blok_len;
    *gen = DT.blok_gen;
}

void dt_kur(ida_link_ust_rol_t rol)
{
    memset(&DT, 0, sizeof DT);
    DT.saat = 1000 * DT_MS;
    DT.ayar.rol = rol;
    DT.ayar.boot_count = 3u;
    DT.ayar.boot_count_ok = true;
    DT.ayar.reset_cause = 1u;
    DT.ayar.init_rc = (uint8_t)IDA_RC_OK;
    DT.ayar.cekirdek.port_major = 1u;
    DT.ayar.cekirdek.port_minor = 0u;
    DT.ayar.cekirdek.param_schema_hash = UINT64_C(0x1122334455667788);
    DT.ayar.cekirdek.core_version = 0x000100u;
    DT.ayar.fw_version = 7u;
    DT.ayar.fw_git = 0xABCDEF01u;
    DT.ayar.t_bag_ns = 500000000u;
    DT.ayar.esc_alt_us = 1100u;
    DT.ayar.esc_notr_us = 1500u;
    DT.ayar.esc_ust_us = 1900u;
    DT.plat.ctx = &DT;
    DT.plat.zaman = p_zaman;
    DT.plat.gonder = p_gonder;
    DT.plat.kuyruk_temizle = p_temizle;
    DT.plat.nonce = p_nonce;
    DT.plat.yeniden_baslat = p_yeniden;
    DT.plat.kalici_blok = p_blok;
    DT.blok_crc = 0xCAFEBABEu;
    DT.blok_len = 120u;
    DT.blok_gen = 4u;
    TEST_ASSERT_EQUAL_INT(IDA_LINK_TAMAM, ida_link_alici_kur(&DT.sim_alici, IDA_LINK_ROL_SIM));
    TEST_ASSERT_EQUAL_INT(IDA_LINK_TAMAM, ida_link_gonderici_kur(&DT.sim_g, IDA_LINK_ROL_SIM));
    TEST_ASSERT_EQUAL_INT(IDA_LINK_TAMAM, ida_link_ust_kur(&DT.l, &DT.ayar, &DT.plat));
}

void dt_ilerle(int64_t ns)
{
    DT.saat += ns;
}

void dt_kayit_sil(void)
{
    DT.n = 0u;
}

/* ------------------------------------------------------------------ */
/* Sahte sim                                                          */
/* ------------------------------------------------------------------ */

void dt_gonder(uint32_t msgid, const void *m, size_t boy)
{
    uint8_t buf[IDA_LINK_CERCEVE_AZAMI];
    size_t n = 0u;
    ida_wtime_t t;
    TEST_ASSERT_EQUAL_INT(IDA_LINK_TAMAM,
                          ida_link_gonder(&DT.sim_g, msgid, m, boy, buf, sizeof buf, &n));
    t.ns = DT.saat;
    ida_link_ust_rx(&DT.l, buf, n, t);
}

unsigned dt_say(uint32_t msgid)
{
    unsigned k = 0u;
    for (unsigned i = 0; i < DT.n; ++i) {
        if (DT.kayit[i].msgid == msgid) {
            k += 1u;
        }
    }
    return k;
}

const dt_cerceve_t *dt_bul(uint32_t msgid, unsigned k)
{
    for (unsigned i = 0; i < DT.n; ++i) {
        if (DT.kayit[i].msgid == msgid) {
            if (k == 0u) {
                return &DT.kayit[i];
            }
            k -= 1u;
        }
    }
    return NULL;
}

const void *dt_son(uint32_t msgid)
{
    for (unsigned i = DT.n; i > 0u; --i) {
        if (DT.kayit[i - 1u].msgid == msgid) {
            return DT.kayit[i - 1u].yuk;
        }
    }
    TEST_FAIL_MESSAGE("beklenen çerçeve gönderilmedi");
    return DT.kayit[0].yuk;   /* kartta Unity kesmez; geçerli bellek döner */
}

const mavlink_ida_cmd_result_t *dt_sonuc(uint32_t cmd_id)
{
    for (unsigned i = DT.n; i > 0u; --i) {
        if (DT.kayit[i - 1u].msgid == MAVLINK_MSG_ID_IDA_CMD_RESULT) {
            const mavlink_ida_cmd_result_t *r =
                (const mavlink_ida_cmd_result_t *)(const void *)DT.kayit[i - 1u].yuk;
            if (r->cmd_id == cmd_id) {
                return r;
            }
        }
    }
    return NULL;
}

void dt_el_sikis(uint32_t sim_nonce, uint16_t sess)
{
    mavlink_ida_sim_hello_t h;
    mavlink_ida_session_accept_t a;
    const mavlink_ida_board_hello_t *b;
    memset(&h, 0, sizeof h);
    h.dialect_hash = IDA_LINK_DIALECT_HASH;
    h.nonce = sim_nonce;
    h.run_id = 7u;
    h.ctrl_period_ns = 5000000u;
    h.proto_major = 1u;
    DT_GONDER(MAVLINK_MSG_ID_IDA_SIM_HELLO, h);
    b = (const mavlink_ida_board_hello_t *)dt_son(MAVLINK_MSG_ID_IDA_BOARD_HELLO);
    TEST_ASSERT_EQUAL_UINT32(sim_nonce, b->peer_nonce);
    memset(&a, 0, sizeof a);
    a.run_id = 7u;
    a.nonce_sim = sim_nonce;
    a.nonce_board = b->nonce;
    a.sess = sess;
    a.result = (uint8_t)IDAW_SESSION_RESULT_OK;
    a.run_mode = (uint8_t)IDAW_RUN_MODE_HIL_1X;
    DT_GONDER(MAVLINK_MSG_ID_IDA_SESSION_ACCEPT, a);
    TEST_ASSERT_EQUAL_INT(IDA_LINK_UST_RUNNING, ida_link_ust_durum(&DT.l));
    DT.sess = sess;
    DT.sim_nonce = sim_nonce;
}

void dt_tf(uint32_t seq, int64_t t, uint8_t n_members, bool imu)
{
    mavlink_ida_time_frame_t m;
    memset(&m, 0, sizeof m);
    m.t_sim_ns = t;
    m.batch_seq = seq;
    m.sess = DT.sess;
    m.n_members = n_members;
    m.tf_flags = (uint8_t)(imu ? IDAW_TF_FLAGS_IMU : 0);
    m.run_mode = (uint8_t)IDAW_RUN_MODE_HIL_1X;
    DT_GONDER(MAVLINK_MSG_ID_IDA_TIME_FRAME, m);
}

void dt_imu(uint32_t seq, int64_t t)
{
    mavlink_ida_imu_t m;
    memset(&m, 0, sizeof m);
    m.t_meas_ns = t;
    m.seq = seq;
    m.window_ns = 5000000u;
    m.accel[2] = -9.81f;
    m.temp_c = 20.0f;
    m.flags = (uint16_t)IDA_HF_VALID;
    DT_GONDER(MAVLINK_MSG_ID_IDA_IMU, m);
}

void dt_mag(uint32_t seq, int64_t t)
{
    mavlink_ida_mag_t m;
    memset(&m, 0, sizeof m);
    m.t_meas_ns = t;
    m.seq = seq;
    m.field[0] = 2.0e-5f;
    m.flags = (uint16_t)IDA_HF_VALID;
    DT_GONDER(MAVLINK_MSG_ID_IDA_MAG, m);
}

void dt_emniyet(uint32_t seq, int64_t t, uint8_t estop, uint32_t rev)
{
    mavlink_ida_safety_t m;
    memset(&m, 0, sizeof m);
    m.t_meas_ns = t;
    m.seq = seq;
    m.safety_rev = rev;
    m.flags = (uint16_t)IDA_HF_VALID;
    m.estop = estop;
    m.tube[0] = (uint8_t)(IDA_TUBE_LOADED | IDA_TUBE_PLUG_IN | IDA_TUBE_PIN_OUT);
    m.tube[1] = (uint8_t)IDA_TUBE_LOADED;
    DT_GONDER(MAVLINK_MSG_ID_IDA_SAFETY, m);
}

void dt_cmd_simple(uint32_t cmd_id, uint8_t type, uint16_t arg)
{
    mavlink_ida_cmd_simple_t m;
    memset(&m, 0, sizeof m);
    m.cmd_id = cmd_id;
    m.sess = DT.sess;
    m.arg = arg;
    m.type = type;
    m.source = 255u;
    DT_GONDER(MAVLINK_MSG_ID_IDA_CMD_SIMPLE, m);
}

void dt_cmd_fc(uint32_t cmd_id, uint8_t op, uint8_t tube)
{
    mavlink_ida_cmd_fc_t m;
    memset(&m, 0, sizeof m);
    m.cmd_id = cmd_id;
    m.sess = DT.sess;
    m.op = op;
    m.tube = tube;
    m.source = 255u;
    DT_GONDER(MAVLINK_MSG_ID_IDA_CMD_FC, m);
}

ida_link_cagri_t dt_take(ida_in_t *in)
{
    return ida_link_ust_take(&DT.l, in);
}

void dt_cikti(const ida_in_t *in, ida_link_cagri_t tur, ida_out_t *out)
{
    memset(out, 0, sizeof *out);
    out->thr_valid = true;
    out->is_safe = true;
    out->src = (uint8_t)(tur == IDA_LINK_CAGRI_STEP ? IDA_SRC_STEP : IDA_SRC_TICK);
    out->seq = in->tf.seq;
    out->t_ref = in->tf.t;
    out->valid_for_ns = 100000000u;
    out->health.core_ok = true;
    for (unsigned i = 0; i < IDA_PORT_N_NARROW; ++i) {
        if ((in->narrow.mask & (1u << i)) != 0u) {
            ida_cmd_result_t *r = &out->results[out->n_results++];
            r->cmd_id = in->narrow.cmd_id[i];
            r->kind = (uint8_t)IDA_RES_ACK;
            r->ack = (uint8_t)IDA_ACK_ACCEPTED;
        }
    }
    for (unsigned i = 0; i < in->n_cmds; ++i) {
        ida_cmd_result_t *r = &out->results[out->n_results++];
        r->cmd_id = in->cmds[i].cmd_id;
        r->kind = (uint8_t)IDA_RES_ACK;
        r->ack = (uint8_t)IDA_ACK_ACCEPTED;
    }
}

void dt_put(const ida_in_t *in, ida_link_cagri_t tur, ida_rc_t rc, const ida_event_t *olaylar,
            uint8_t n_olay)
{
    ida_out_t out;
    dt_cikti(in, tur, &out);
    for (unsigned i = 0; i < n_olay && i < IDA_PORT_MAX_EVENTS; ++i) {
        out.events[i] = olaylar[i];
    }
    out.n_events = n_olay;
    ida_link_ust_put_out(&DT.l, in, rc, &out);
}
