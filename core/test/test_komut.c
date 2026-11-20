/*
 * test_komut.c — komut sonuçları (A4-K43, K45, K47, K48, K103, K104, K108, K114, K119,
 * K120, K124; tablo 3.17).
 */
#include <string.h>

#include "destek.h"
#include "unity.h"

void setUp(void);
void tearDown(void);
void setUp(void) {}
void tearDown(void) {}

static ida_core_t C;
static ida_in_t IN;
static ida_out_t OUT;
static uint32_t g_seq;
static int64_t g_t;

static void hazirla(void)
{
    TEST_ASSERT_EQUAL(IDA_RC_OK, ds_kur(&C));
    g_seq = 0u;
    g_t = 2000 * DS_MS;
    ds_girdi(&IN, g_t, 0u, true);
}

static ida_rc_t step(void)
{
    g_seq++;
    g_t += 5 * DS_MS;
    IN.now_w.ns = g_t;
    IN.tf.seq = g_seq;
    IN.tf.t.ns = g_t;
    IN.tf.t_rx.ns = g_t;
    ds_imu(&IN.imu, g_t, g_seq);
    const ida_rc_t rc = ida_core_step(&C, &IN, &OUT);
    ds_cikti_degismezleri(rc, &IN, &OUT);
    return rc;
}

static void komut(uint32_t i, uint32_t id, uint8_t tur)
{
    (void)memset(&IN.cmds[i], 0, sizeof(IN.cmds[i]));
    IN.cmds[i].cmd_id = id;
    IN.cmds[i].type = tur;
    if (IN.n_cmds <= i) {
        IN.n_cmds = (uint8_t)(i + 1u);
    }
}

static void fc(uint32_t i, uint32_t id, uint8_t op)
{
    komut(i, id, (uint8_t)IDA_CMD_FC);
    IN.cmds[i].u.fc.op = op;
}

static const ida_cmd_result_t *s(uint32_t id)
{
    const ida_cmd_result_t *r = ds_sonuc(&OUT, id);
    TEST_ASSERT_NOT_NULL(r);
    return r;
}

static void ret_bekle(uint32_t id, uint16_t neden)
{
    TEST_ASSERT_EQUAL_UINT8(IDA_ACK_REJECTED, s(id)->ack);
    TEST_ASSERT_EQUAL_UINT16(neden, s(id)->reason);
}

static void desteksiz_bekle(uint32_t id)
{
    TEST_ASSERT_EQUAL_UINT8(IDA_ACK_UNSUPPORTED, s(id)->ack);
    TEST_ASSERT_EQUAL_UINT16(IDA_REASON_NONE, s(id)->reason);
}

static void test_k43_sira_ve_sayi(void)
{
    hazirla();
    IN.narrow.mask = (uint8_t)(IDA_NARROW_FC_ABORT | IDA_NARROW_DISARM);
    IN.narrow.cmd_id[0] = 100u;
    IN.narrow.cmd_id[3] = 103u;
    for (uint32_t i = 0u; i < 8u; i++) {
        komut(i, 200u + i, (uint8_t)IDA_CMD_HEARTBEAT);
    }
    TEST_ASSERT_EQUAL(IDA_RC_OK, step());
    TEST_ASSERT_EQUAL_UINT8(10u, OUT.n_results); /* A4-K47: hepsi işlenir */
    TEST_ASSERT_EQUAL_UINT32(100u, OUT.results[0].cmd_id);
    TEST_ASSERT_EQUAL_UINT32(103u, OUT.results[1].cmd_id);
    for (uint32_t i = 0u; i < 8u; i++) {
        TEST_ASSERT_EQUAL_UINT32(200u + i, OUT.results[2u + i].cmd_id);
        TEST_ASSERT_EQUAL_UINT8(IDA_RES_ACK, OUT.results[2u + i].kind);
    }
}

static void test_317_yer_tutucu_satirlari(void)
{
    hazirla();
    komut(0u, 1u, 0u);                                 /* bilinmeyen tür */
    komut(1u, 2u, 17u);
    komut(2u, 3u, (uint8_t)IDA_CMD_HEARTBEAT);
    komut(3u, 4u, (uint8_t)IDA_CMD_MANUAL);
    komut(4u, 5u, (uint8_t)IDA_CMD_GUIDED_SP);
    komut(5u, 6u, (uint8_t)IDA_CMD_MISSION_BEGIN);
    komut(6u, 7u, (uint8_t)IDA_CMD_FAILSAFE_RESET);
    komut(7u, 8u, (uint8_t)IDA_CMD_SET_MODE);
    IN.cmds[7].u.set_mode.mode = (uint8_t)IDA_MODE_GUIDED;
    TEST_ASSERT_EQUAL(IDA_RC_OK, step());
    for (uint32_t id = 1u; id <= 7u; id++) {
        desteksiz_bekle(id);
    }
    ret_bekle(8u, (uint16_t)IDA_REASON_BAD_MODE);
    /* MISSION_ITEM, MISSION_END, MISSION_CTL; SET_MODE(AUTO) */
    IN.n_cmds = 0u;
    komut(0u, 11u, (uint8_t)IDA_CMD_MISSION_ITEM);
    komut(1u, 12u, (uint8_t)IDA_CMD_MISSION_END);
    komut(2u, 13u, (uint8_t)IDA_CMD_MISSION_CTL);
    komut(3u, 14u, (uint8_t)IDA_CMD_SET_MODE);
    IN.cmds[3].u.set_mode.mode = (uint8_t)IDA_MODE_AUTO;
    TEST_ASSERT_EQUAL(IDA_RC_OK, step());
    desteksiz_bekle(11u);
    desteksiz_bekle(12u);
    desteksiz_bekle(13u);
    ret_bekle(14u, (uint16_t)IDA_REASON_BAD_MODE);
    /* UNSUPPORTED pencereye girer (A4-K48) */
    IN.n_cmds = 0u;
    komut(0u, 1u, 0u);
    TEST_ASSERT_EQUAL(IDA_RC_OK, step());
    TEST_ASSERT_EQUAL_UINT8(IDA_ACK_DUPLICATE, s(1u)->ack);
}

static void test_317_arm_sirasi(void)
{
    /* olay kimliği yok → EVENTS_OFF */
    ida_init_in_t ii;
    ds_init_girdisi(&ii);
    ii.boot_count_ok = false;
    static uint8_t blok[IDA_PARAM_BLOB_MAX];
    ii.param_len = ds_gecerli_blok(blok);
    ii.param_blob = blok;
    TEST_ASSERT_EQUAL(IDA_RC_OK, ida_core_init(&C, &ii));
    g_seq = 0u;
    g_t = 2000 * DS_MS;
    ds_girdi(&IN, g_t, 0u, true);
    komut(0u, 1u, (uint8_t)IDA_CMD_ARM);
    TEST_ASSERT_EQUAL(IDA_RC_OK, step());
    ret_bekle(1u, (uint16_t)IDA_REASON_EVENTS_OFF);
    TEST_ASSERT_FALSE(OUT.status.events_ok);
    /* boot_count aralık dışı da olaysız çalışma (A4-K61) */
    ii.boot_count_ok = true;
    ii.boot_count = 0xFFFFFFFFu;
    TEST_ASSERT_EQUAL(IDA_RC_OK, ida_core_init(&C, &ii));
    g_seq = 0u;
    komut(0u, 2u, (uint8_t)IDA_CMD_ARM);
    TEST_ASSERT_EQUAL(IDA_RC_OK, step());
    ret_bekle(2u, (uint16_t)IDA_REASON_EVENTS_OFF);
    /* varsayılanlar → PARAM_INVALID (A4-K78) */
    TEST_ASSERT_EQUAL(IDA_RC_PARAM_DEFAULTS, ds_kur_bloksuz(&C));
    g_seq = 0u;
    komut(0u, 3u, (uint8_t)IDA_CMD_ARM);
    IN.plat.param_store = (uint8_t)IDA_PSTORE_BUSY;
    TEST_ASSERT_EQUAL(IDA_RC_OK, step());
    ret_bekle(3u, (uint16_t)IDA_REASON_PARAM_INVALID);
    /* geçerli: BUSY → PARAM_BUSY; değilse NOT_ARMABLE */
    hazirla();
    komut(0u, 4u, (uint8_t)IDA_CMD_ARM);
    IN.plat.param_store = (uint8_t)IDA_PSTORE_BUSY;
    TEST_ASSERT_EQUAL(IDA_RC_OK, step());
    ret_bekle(4u, (uint16_t)IDA_REASON_PARAM_BUSY);
    komut(0u, 5u, (uint8_t)IDA_CMD_ARM);
    IN.plat.param_store = (uint8_t)IDA_PSTORE_IDLE;
    TEST_ASSERT_EQUAL(IDA_RC_OK, step());
    ret_bekle(5u, (uint16_t)IDA_REASON_NOT_ARMABLE);
    TEST_ASSERT_FALSE(OUT.armed);
}

static void test_317_fc(void)
{
    hazirla();
    /* A4-K119: kapalı liste dışı op, tüp, ayrılmış argümanlar */
    fc(0u, 1u, 4u);
    fc(1u, 2u, 9u);
    fc(2u, 3u, (uint8_t)IDA_FC_TEST_EXIT);
    IN.cmds[2].u.fc.tube = 2u;
    fc(3u, 4u, (uint8_t)IDA_FC_TEST_EXIT);
    IN.cmds[3].u.fc.arg0 = 1u;
    fc(4u, 5u, (uint8_t)IDA_FC_TEST_EXIT);
    IN.cmds[4].u.fc.arg1 = 1u;
    fc(5u, 6u, (uint8_t)IDA_FC_TEST_EXIT);
    TEST_ASSERT_EQUAL(IDA_RC_OK, step());
    for (uint32_t id = 1u; id <= 5u; id++) {
        ret_bekle(id, (uint16_t)IDA_REASON_OUT_OF_RANGE);
    }
    TEST_ASSERT_EQUAL_UINT8(IDA_ACK_ACCEPTED, s(6u)->ack); /* TEST değil: etkisiz */
    /* oturum yok → SESSION (FC_ARM, FC_TEST_ENTER, FC_FIRE, FC_RESET) */
    IN.n_cmds = 0u;
    IN.plat.link_state = (uint8_t)IDA_LINK_NONE;
    fc(0u, 11u, (uint8_t)IDA_FC_ARM);
    fc(1u, 12u, (uint8_t)IDA_FC_TEST_ENTER);
    fc(2u, 13u, (uint8_t)IDA_FC_FIRE);
    fc(3u, 14u, (uint8_t)IDA_FC_RESET);
    TEST_ASSERT_EQUAL(IDA_RC_OK, step());
    for (uint32_t id = 11u; id <= 14u; id++) {
        ret_bekle(id, (uint16_t)IDA_REASON_SESSION);
    }
    /* oturum var: izin kümesi boş → BAD_MODE; FC_RESET FAULT_LATCHED değil → BAD_MODE */
    IN.n_cmds = 0u;
    IN.plat.link_state = (uint8_t)IDA_LINK_RUNNING;
    fc(0u, 21u, (uint8_t)IDA_FC_ARM);
    fc(1u, 22u, (uint8_t)IDA_FC_FIRE);
    fc(2u, 23u, (uint8_t)IDA_FC_RESET);
    TEST_ASSERT_EQUAL(IDA_RC_OK, step());
    ret_bekle(21u, (uint16_t)IDA_REASON_BAD_MODE);
    ret_bekle(22u, (uint16_t)IDA_REASON_BAD_MODE);
    ret_bekle(23u, (uint16_t)IDA_REASON_BAD_MODE);
}

static void test_317_fc_olay_kimligi_yok(void)
{
    ida_init_in_t ii;
    ds_init_girdisi(&ii);
    ii.boot_count_ok = false;
    TEST_ASSERT_EQUAL(IDA_RC_PARAM_DEFAULTS, ida_core_init(&C, &ii));
    g_seq = 0u;
    g_t = 2000 * DS_MS;
    ds_girdi(&IN, g_t, 0u, true);
    fc(0u, 1u, (uint8_t)IDA_FC_ARM);
    TEST_ASSERT_EQUAL(IDA_RC_OK, step());
    ret_bekle(1u, (uint16_t)IDA_REASON_EVENTS_OFF);
}

static void test_genisletici_onceligi_ve_yineleme(void)
{
    hazirla();
    komut(0u, 5u, (uint8_t)IDA_CMD_ARM);
    TEST_ASSERT_EQUAL(IDA_RC_OK, step());
    /* yineleme denetimi NARROWED'den önce (A4-K48) */
    IN.narrow.mask = IDA_NARROW_DISARM;
    komut(0u, 5u, (uint8_t)IDA_CMD_ARM);
    komut(1u, 6u, (uint8_t)IDA_CMD_ARM);
    fc(2u, 7u, (uint8_t)IDA_FC_FIRE);
    IN.cmds[2].u.fc.arg0 = 9u; /* genişletici: NARROWED, K119'dan önce */
    komut(3u, 8u, (uint8_t)IDA_CMD_PARAM_GET); /* nötr: daraltmadan etkilenmez */
    IN.cmds[3].u.param.id = 0x0101u;
    TEST_ASSERT_EQUAL(IDA_RC_OK, step());
    TEST_ASSERT_EQUAL_UINT8(IDA_ACK_DUPLICATE, s(5u)->ack);
    ret_bekle(6u, (uint16_t)IDA_REASON_NARROWED);
    ret_bekle(7u, (uint16_t)IDA_REASON_NARROWED);
    TEST_ASSERT_EQUAL_UINT8(IDA_ACK_ACCEPTED, s(8u)->ack);
    TEST_ASSERT_EQUAL_UINT32(2u, OUT.status.cnt.widen_blocked);
    /* NARROWED > IMU_REJECTED */
    IN.n_cmds = 0u;
    komut(0u, 9u, (uint8_t)IDA_CMD_FAILSAFE_RESET);
    g_seq++;
    g_t += 5 * DS_MS;
    IN.now_w.ns = g_t;
    IN.tf.seq = g_seq;
    IN.tf.t.ns = g_t;
    IN.tf.t_rx.ns = g_t;
    ds_imu(&IN.imu, g_t, g_seq);
    IN.imu.hdr.flags = 0u;
    TEST_ASSERT_EQUAL(IDA_RC_IMU_REJECTED, ida_core_step(&C, &IN, &OUT));
    ret_bekle(9u, (uint16_t)IDA_REASON_NARROWED);
    /* daraltma yokken IMU_REJECTED */
    IN.narrow.mask = 0u;
    komut(0u, 10u, (uint8_t)IDA_CMD_FAILSAFE_RESET);
    g_seq++;
    g_t += 5 * DS_MS;
    IN.now_w.ns = g_t;
    IN.tf.seq = g_seq;
    IN.tf.t.ns = g_t;
    IN.tf.t_rx.ns = g_t;
    ds_imu(&IN.imu, g_t, g_seq);
    IN.imu.hdr.flags = 0u;
    TEST_ASSERT_EQUAL(IDA_RC_IMU_REJECTED, ida_core_step(&C, &IN, &OUT));
    ret_bekle(10u, (uint16_t)IDA_REASON_IMU_REJECTED);
}

static void test_reddedilen_cagri_pencereye_girmez(void)
{
    hazirla();
    TEST_ASSERT_EQUAL(IDA_RC_OK, step());
    komut(0u, 9u, (uint8_t)IDA_CMD_HEARTBEAT);
    g_seq--;
    g_t -= 5 * DS_MS;
    TEST_ASSERT_EQUAL(IDA_RC_DUP_SEQ, step());
    ret_bekle(9u, (uint16_t)IDA_REASON_CALL_REJECTED);
    TEST_ASSERT_EQUAL_UINT32(1u, OUT.status.cnt.cmd_rejected);
    TEST_ASSERT_EQUAL(IDA_RC_OK, step());
    desteksiz_bekle(9u); /* DUPLICATE değil (A4-K114) */
    /* reddedilen çağrıda PARAM_GET sonucu PARAM_VALUE türünde */
    komut(0u, 10u, (uint8_t)IDA_CMD_PARAM_GET);
    IN.n_samples = 17u;
    TEST_ASSERT_EQUAL(IDA_RC_BAD_INPUT, step());
    TEST_ASSERT_EQUAL_UINT8(IDA_RES_PARAM_VALUE, s(10u)->kind);
    ret_bekle(10u, (uint16_t)IDA_REASON_CALL_REJECTED);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_k43_sira_ve_sayi);
    RUN_TEST(test_317_yer_tutucu_satirlari);
    RUN_TEST(test_317_arm_sirasi);
    RUN_TEST(test_317_fc);
    RUN_TEST(test_317_fc_olay_kimligi_yok);
    RUN_TEST(test_genisletici_onceligi_ve_yineleme);
    RUN_TEST(test_reddedilen_cagri_pencereye_girmez);
    return UNITY_END();
}
