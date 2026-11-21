/*
 * test_parti.c — parti (W3-oturum kabul md. 1: tam parti -> doğru çağrı girdisi; yinelenen,
 * eski, eksik parti; üye taşması; A1-K28–K32, K40, K117, K124; A4-K102; Ping360 A1-K49–K51).
 */
#include "destek_oturum.h"

#include "unity.h"

#include <string.h>

void setUp(void);
void tearDown(void);
void setUp(void)
{
    dt_kur(IDA_LINK_UST_ROL_KART);
    dt_el_sikis(0x99u, 77u);
    dt_kayit_sil();
}
void tearDown(void) {}

static void mag_seq(uint32_t seq, int64_t t, uint8_t inst)
{
    mavlink_ida_mag_t m;
    memset(&m, 0, sizeof m);
    m.t_meas_ns = t;
    m.seq = seq;
    m.inst = inst;
    m.flags = (uint16_t)IDA_HF_VALID;
    DT_GONDER(MAVLINK_MSG_ID_IDA_MAG, m);
}

static void test_tam_parti_step(void)
{
    ida_in_t in;
    dt_ilerle(1 * DT_MS);
    dt_imu(10u, 5 * DT_MS);
    dt_mag(4u, 4 * DT_MS);
    dt_emniyet(2u, 3 * DT_MS, 0u, 9u);
    dt_tf(1000u, 5 * DT_MS, 3u, true);
    TEST_ASSERT_EQUAL_INT(IDA_LINK_CAGRI_STEP, dt_take(&in));
    TEST_ASSERT_TRUE(in.tf.present);
    TEST_ASSERT_EQUAL_UINT32(1000u, in.tf.seq);
    TEST_ASSERT_EQUAL_INT64(5 * DT_MS, in.tf.t.ns);
    TEST_ASSERT_EQUAL_INT64(DT.saat, in.tf.t_rx.ns);
    TEST_ASSERT_EQUAL_INT64(DT.saat, in.now_w.ns);
    TEST_ASSERT_TRUE(in.imu_present);
    TEST_ASSERT_EQUAL_UINT32(10u, in.imu.hdr.seq);
    TEST_ASSERT_EQUAL_UINT8(IDA_SENS_IMU, in.imu.hdr.type);
    TEST_ASSERT_EQUAL_UINT32(5000000u, in.imu.hdr.window_ns);
    /* Emniyet örneği başta (A1-K124), sonra öteki örnekler. */
    TEST_ASSERT_EQUAL_UINT8(2u, in.n_samples);
    TEST_ASSERT_EQUAL_UINT8(IDA_SENS_SAFETY, in.samples[0].hdr.type);
    TEST_ASSERT_EQUAL_UINT16(IDA_HF_VALID, in.samples[0].hdr.flags);
    TEST_ASSERT_EQUAL_UINT8(IDA_SENS_MAG, in.samples[1].hdr.type);
    TEST_ASSERT_EQUAL_UINT32(0u, in.samples[1].hdr.window_ns);
    TEST_ASSERT_FALSE(in.plat.batch_merged);
    TEST_ASSERT_FALSE(in.plat.input_dropped);
    TEST_ASSERT_EQUAL_UINT32(0u, in.plat.cnt_batch_incomplete);
    dt_put(&in, IDA_LINK_CAGRI_STEP, IDA_RC_OK, NULL, 0u);
    {
        const mavlink_ida_actuator_t *a =
            (const mavlink_ida_actuator_t *)dt_son(MAVLINK_MSG_ID_IDA_ACTUATOR);
        TEST_ASSERT_EQUAL_UINT32(1000u, a->batch_seq);
        TEST_ASSERT_EQUAL_UINT16(0u, a->act_flags & IDAW_ACT_FLAGS_BATCH_INCOMPLETE);
        TEST_ASSERT_EQUAL_UINT8(0u, a->n_merged);
    }
}

static void test_imusuz_parti_tick(void)
{
    ida_in_t in;
    dt_mag(1u, 1 * DT_MS);
    dt_tf(5u, 5 * DT_MS, 1u, false);
    TEST_ASSERT_EQUAL_INT(IDA_LINK_CAGRI_TICK_TF, dt_take(&in));
    TEST_ASSERT_FALSE(in.imu_present);
    TEST_ASSERT_TRUE(in.tf.present);
    dt_put(&in, IDA_LINK_CAGRI_TICK_TF, IDA_RC_OK, NULL, 0u);
}

static void test_eksik_parti(void)
{
    ida_in_t in;
    /* n_members 3 ama 2 üye; ve IMU bayrağı kurulu ama IMU yok. */
    dt_mag(1u, 1 * DT_MS);
    dt_mag(2u, 2 * DT_MS);
    dt_tf(1u, 5 * DT_MS, 3u, false);
    TEST_ASSERT_EQUAL_INT(IDA_LINK_CAGRI_TICK_TF, dt_take(&in));
    TEST_ASSERT_EQUAL_UINT32(1u, in.plat.cnt_batch_incomplete);
    TEST_ASSERT_EQUAL_UINT8(2u, in.n_samples);   /* alınanlar yine verilir */
    dt_put(&in, IDA_LINK_CAGRI_TICK_TF, IDA_RC_OK, NULL, 0u);
    TEST_ASSERT_NOT_EQUAL(0u, ((const mavlink_ida_actuator_t *)dt_son(
                                   MAVLINK_MSG_ID_IDA_ACTUATOR))->act_flags &
                                  IDAW_ACT_FLAGS_BATCH_INCOMPLETE);
    dt_mag(3u, 6 * DT_MS);
    dt_tf(2u, 10 * DT_MS, 1u, true);
    TEST_ASSERT_EQUAL_INT(IDA_LINK_CAGRI_TICK_TF, dt_take(&in));
    TEST_ASSERT_EQUAL_UINT32(2u, in.plat.cnt_batch_incomplete);
    dt_put(&in, IDA_LINK_CAGRI_TICK_TF, IDA_RC_OK, NULL, 0u);
}

static void test_yinelenen_ve_eski_parti(void)
{
    ida_in_t in;
    dt_imu(1u, 5 * DT_MS);
    dt_tf(50u, 5 * DT_MS, 1u, true);
    TEST_ASSERT_EQUAL_INT(IDA_LINK_CAGRI_STEP, dt_take(&in));
    dt_put(&in, IDA_LINK_CAGRI_STEP, IDA_RC_OK, NULL, 0u);
    dt_kayit_sil();
    /* DUP: pencere atılır, emniyet örneği VALID = 0 ile sıradaki çağrıya. */
    dt_imu(2u, 6 * DT_MS);
    dt_emniyet(7u, 6 * DT_MS, 1u, 3u);
    dt_tf(50u, 5 * DT_MS, 2u, true);
    TEST_ASSERT_EQUAL_UINT32(1u, DT.l.say[IDA_LINK_SAY_BATCH_DUP]);
    /* OLD: fark 2^31 (tam yarım uzay eski sayılır). */
    dt_emniyet(8u, 7 * DT_MS, 0u, 3u);
    dt_tf(50u + 0x80000000u, 5 * DT_MS, 1u, false);
    TEST_ASSERT_EQUAL_UINT32(1u, DT.l.say[IDA_LINK_SAY_BATCH_OLD]);
    TEST_ASSERT_EQUAL_UINT(0u, dt_say(MAVLINK_MSG_ID_IDA_ACTUATOR));   /* yanıt yok */
    /* Sıradaki çağrı ızgara tick'i: taşınan iki emniyet örneği, VALID = 0, varış sırasıyla. */
    dt_ilerle(10 * DT_MS);
    TEST_ASSERT_EQUAL_INT(IDA_LINK_CAGRI_IZGARA, dt_take(&in));
    TEST_ASSERT_EQUAL_UINT8(2u, in.n_samples);
    TEST_ASSERT_EQUAL_UINT32(7u, in.samples[0].hdr.seq);
    TEST_ASSERT_EQUAL_UINT8(1u, in.samples[0].u.safety.estop);   /* öteki alanlar aynen */
    TEST_ASSERT_EQUAL_UINT16(0u, in.samples[0].hdr.flags & IDA_HF_VALID);
    TEST_ASSERT_EQUAL_UINT32(8u, in.samples[1].hdr.seq);
    TEST_ASSERT_EQUAL_UINT16(0u, in.samples[1].hdr.flags & IDA_HF_VALID);
    TEST_ASSERT_EQUAL_UINT32(1u, in.plat.cnt_batch_old);
    dt_put(&in, IDA_LINK_CAGRI_IZGARA, IDA_RC_OK, NULL, 0u);
    /* Boşluk: 2^31 − 1 ileri sıçrama yeni sayılır. */
    dt_imu(3u, 10 * DT_MS);
    dt_tf(50u + 0x7FFFFFFFu, 10 * DT_MS, 1u, true);
    TEST_ASSERT_EQUAL_INT(IDA_LINK_CAGRI_STEP, dt_take(&in));
    TEST_ASSERT_EQUAL_UINT32(0x7FFFFFFEu, in.plat.cnt_batch_skipped);
    dt_put(&in, IDA_LINK_CAGRI_STEP, IDA_RC_OK, NULL, 0u);
}

static void test_birlestirme(void)
{
    ida_in_t in;
    /* İki parti çağrı yapılmadan kapanır (A4-K102, A1-K40). */
    dt_imu(1u, 5 * DT_MS);
    dt_emniyet(1u, 4 * DT_MS, 0u, 2u);
    dt_mag(1u, 4 * DT_MS);
    dt_tf(1u, 5 * DT_MS, 3u, true);
    dt_ilerle(1 * DT_MS);
    dt_imu(2u, 10 * DT_MS);
    dt_emniyet(2u, 9 * DT_MS, 0u, 3u);
    dt_tf(2u, 10 * DT_MS, 2u, true);
    TEST_ASSERT_EQUAL_INT(IDA_LINK_CAGRI_STEP, dt_take(&in));
    TEST_ASSERT_EQUAL_UINT32(2u, in.tf.seq);
    TEST_ASSERT_EQUAL_UINT32(2u, in.imu.hdr.seq);   /* en yeni IMU */
    TEST_ASSERT_TRUE(in.plat.batch_merged);
    TEST_ASSERT_EQUAL_UINT32(1u, in.plat.cnt_batch_merged);
    TEST_ASSERT_EQUAL_UINT8(3u, in.n_samples);
    /* Eski partinin emniyet örneği VALID = 0, en yeninin VALID = 1; ikisi de başta. */
    TEST_ASSERT_EQUAL_UINT8(IDA_SENS_SAFETY, in.samples[0].hdr.type);
    TEST_ASSERT_EQUAL_UINT16(0u, in.samples[0].hdr.flags & IDA_HF_VALID);
    TEST_ASSERT_EQUAL_UINT8(IDA_SENS_SAFETY, in.samples[1].hdr.type);
    TEST_ASSERT_EQUAL_UINT16(IDA_HF_VALID, in.samples[1].hdr.flags & IDA_HF_VALID);
    TEST_ASSERT_EQUAL_UINT8(IDA_SENS_MAG, in.samples[2].hdr.type);
    dt_put(&in, IDA_LINK_CAGRI_STEP, IDA_RC_OK, NULL, 0u);
    TEST_ASSERT_EQUAL_UINT8(1u, ((const mavlink_ida_actuator_t *)dt_son(
                                    MAVLINK_MSG_ID_IDA_ACTUATOR))->n_merged);
    TEST_ASSERT_EQUAL_UINT(1u, dt_say(MAVLINK_MSG_ID_IDA_ACTUATOR));   /* eskiye yanıt yok */
}

static void test_uye_tasmasi(void)
{
    ida_in_t in;
    /* 20 MAG: 16'sı sığar, 4'ü düşer (cnt_member_overflow, input_dropped). */
    for (uint32_t i = 0; i < 20u; ++i) {
        mag_seq(i + 1u, 1 * DT_MS, (uint8_t)(i % 4u));
    }
    dt_tf(1u, 5 * DT_MS, 20u, false);
    TEST_ASSERT_EQUAL_INT(IDA_LINK_CAGRI_TICK_TF, dt_take(&in));
    TEST_ASSERT_EQUAL_UINT8(16u, in.n_samples);
    TEST_ASSERT_EQUAL_UINT32(4u, in.plat.cnt_member_overflow);
    TEST_ASSERT_TRUE(in.plat.input_dropped);
    dt_put(&in, IDA_LINK_CAGRI_TICK_TF, IDA_RC_OK, NULL, 0u);
}

static void test_emniyet_ornegi_kapasite_ile_dusmez(void)
{
    ida_in_t in;
    /* 15 MAG + 3 emniyet: 3 emniyet başta, 13 MAG; düşen yalnız MAG. */
    for (uint32_t i = 0; i < 15u; ++i) {
        mag_seq(i + 1u, 1 * DT_MS, 0u);
    }
    dt_emniyet(1u, 1 * DT_MS, 0u, 1u);
    dt_emniyet(2u, 2 * DT_MS, 0u, 1u);
    dt_emniyet(3u, 3 * DT_MS, 1u, 2u);
    dt_tf(1u, 5 * DT_MS, 18u, false);
    TEST_ASSERT_EQUAL_INT(IDA_LINK_CAGRI_TICK_TF, dt_take(&in));
    TEST_ASSERT_EQUAL_UINT8(16u, in.n_samples);
    for (unsigned i = 0; i < 3u; ++i) {
        TEST_ASSERT_EQUAL_UINT8(IDA_SENS_SAFETY, in.samples[i].hdr.type);
    }
    TEST_ASSERT_EQUAL_UINT32(2u, in.plat.cnt_member_overflow);
    dt_put(&in, IDA_LINK_CAGRI_TICK_TF, IDA_RC_OK, NULL, 0u);
}

static void test_emniyet_kapasite_asimi_oturum_sonu(void)
{
    /* 17 emniyet taşıyan birleştirilmiş girdi (E20, H52): oturum PROTOCOL ile biter. */
    for (uint32_t k = 0; k < 17u; ++k) {
        dt_emniyet(k + 1u, (int64_t)(k + 1u) * 5 * DT_MS, 0u, 1u);
        dt_tf(k + 1u, (int64_t)(k + 1u) * 5 * DT_MS, 1u, false);
    }
    TEST_ASSERT_EQUAL_INT(IDA_LINK_UST_SONLANIYOR, ida_link_ust_durum(&DT.l));
    TEST_ASSERT_EQUAL_UINT8(IDAW_CLOSE_REASON_PROTOCOL,
                            ((const mavlink_ida_session_close_t *)dt_son(
                                 MAVLINK_MSG_ID_IDA_SESSION_CLOSE))->reason);
}

/* TF'nin alım damgası süren çağrının içinde; W3-K10 gereği baytlar çağrı dönünce verilir. */
static void test_cagri_cakismasi(void)
{
    ida_in_t in;
    ida_in_t in2;
    uint8_t buf[2u * IDA_LINK_CERCEVE_AZAMI];
    size_t n = 0u;
    size_t k = 0u;
    ida_wtime_t trx;
    mavlink_ida_imu_t imu;
    mavlink_ida_time_frame_t tf;
    dt_imu(1u, 5 * DT_MS);
    dt_tf(1u, 5 * DT_MS, 1u, true);
    TEST_ASSERT_EQUAL_INT(IDA_LINK_CAGRI_STEP, dt_take(&in));
    trx.ns = DT.saat + 2 * DT_MS;                    /* step sürerken alındı */
    dt_ilerle(3 * DT_MS);
    dt_put(&in, IDA_LINK_CAGRI_STEP, IDA_RC_OK, NULL, 0u);
    memset(&imu, 0, sizeof imu);
    imu.t_meas_ns = 10 * DT_MS;
    imu.seq = 2u;
    imu.window_ns = 5000000u;
    imu.flags = (uint16_t)IDA_HF_VALID;
    TEST_ASSERT_EQUAL_INT(IDA_LINK_TAMAM, ida_link_gonder(&DT.sim_g, MAVLINK_MSG_ID_IDA_IMU, &imu,
                                                          sizeof imu, buf, sizeof buf, &n));
    memset(&tf, 0, sizeof tf);
    tf.t_sim_ns = 10 * DT_MS;
    tf.batch_seq = 2u;
    tf.sess = DT.sess;
    tf.n_members = 1u;
    tf.tf_flags = (uint8_t)IDAW_TF_FLAGS_IMU;
    TEST_ASSERT_EQUAL_INT(IDA_LINK_TAMAM, ida_link_gonder(&DT.sim_g, MAVLINK_MSG_ID_IDA_TIME_FRAME,
                                                          &tf, sizeof tf, buf + n,
                                                          sizeof buf - n, &k));
    ida_link_ust_rx(&DT.l, buf, n + k, trx);
    TEST_ASSERT_EQUAL_INT(IDA_LINK_CAGRI_STEP, dt_take(&in2));
    dt_put(&in2, IDA_LINK_CAGRI_STEP, IDA_RC_OK, NULL, 0u);
    TEST_ASSERT_NOT_EQUAL(0u, ((const mavlink_ida_actuator_t *)dt_son(
                                   MAVLINK_MSG_ID_IDA_ACTUATOR))->act_flags &
                                  IDAW_ACT_FLAGS_CALL_CONFLICT);
}

static void test_izgara_takvimi(void)
{
    ida_in_t in;
    dt_imu(1u, 5 * DT_MS);
    dt_tf(1u, 5 * DT_MS, 1u, true);
    TEST_ASSERT_EQUAL_INT(IDA_LINK_CAGRI_STEP, dt_take(&in));
    dt_put(&in, IDA_LINK_CAGRI_STEP, IDA_RC_OK, NULL, 0u);
    /* E = TF + 5 ms; E + 1 ms'ye kadar tick yok (A1-K117). */
    dt_ilerle(5 * DT_MS);
    TEST_ASSERT_EQUAL_INT(IDA_LINK_CAGRI_YOK, dt_take(&in));
    dt_ilerle(999999);
    TEST_ASSERT_EQUAL_INT(IDA_LINK_CAGRI_YOK, dt_take(&in));
    dt_ilerle(1);
    TEST_ASSERT_EQUAL_INT(IDA_LINK_CAGRI_IZGARA, dt_take(&in));
    TEST_ASSERT_FALSE(in.tf.present);
    TEST_ASSERT_EQUAL_UINT8(0u, in.n_samples);
    TEST_ASSERT_EQUAL_UINT8(0u, in.n_cmds);
    dt_put(&in, IDA_LINK_CAGRI_IZGARA, IDA_RC_OK, NULL, 0u);
    {
        const mavlink_ida_actuator_t *a =
            (const mavlink_ida_actuator_t *)dt_son(MAVLINK_MSG_ID_IDA_ACTUATOR);
        TEST_ASSERT_NOT_EQUAL(0u, a->act_flags & IDAW_ACT_FLAGS_ASYNC);
        TEST_ASSERT_EQUAL_UINT32(1u, a->batch_seq);   /* son kapanan parti */
    }
    /* Sonraki tick 10 ms sonra. */
    dt_ilerle(9 * DT_MS);
    TEST_ASSERT_EQUAL_INT(IDA_LINK_CAGRI_YOK, dt_take(&in));
    dt_ilerle(1 * DT_MS);
    TEST_ASSERT_EQUAL_INT(IDA_LINK_CAGRI_IZGARA, dt_take(&in));
    dt_put(&in, IDA_LINK_CAGRI_IZGARA, IDA_RC_OK, NULL, 0u);
}

static void ping_veri(uint32_t seq, uint8_t frag, uint8_t deger)
{
    mavlink_ida_ping360_data_t m;
    memset(&m, 0, sizeof m);
    m.seq = seq;
    m.frag_idx = frag;
    memset(m.data, deger, sizeof m.data);
    DT_GONDER(MAVLINK_MSG_ID_IDA_PING360_DATA, m);
}

static void ping_bas(uint32_t seq, uint16_t n_samples)
{
    mavlink_ida_ping360_head_t m;
    memset(&m, 0, sizeof m);
    m.t_meas_ns = 2 * DT_MS;
    m.seq = seq;
    m.angle_grad = 100u;
    m.n_samples = n_samples;
    m.sample_period = 100u;
    m.tx_duration_us = 50u;
    m.tx_freq_khz = 750u;
    m.flags = (uint16_t)IDA_HF_VALID;
    m.gain = 1u;
    m.n_frag = (uint8_t)((n_samples + 239u) / 240u);
    DT_GONDER(MAVLINK_MSG_ID_IDA_PING360_HEAD, m);
}

static void test_ping360_birlestirme(void)
{
    ida_in_t in;
    /* 600 örnek = 3 parça, iki pencereye yayılır; teslim HEAD'in partisinde. */
    ping_veri(5u, 0u, 1u);
    ping_veri(5u, 1u, 2u);
    dt_tf(1u, 5 * DT_MS, 2u, false);
    TEST_ASSERT_EQUAL_INT(IDA_LINK_CAGRI_TICK_TF, dt_take(&in));
    TEST_ASSERT_NULL(in.ping360);
    dt_put(&in, IDA_LINK_CAGRI_TICK_TF, IDA_RC_OK, NULL, 0u);
    ping_veri(5u, 2u, 3u);
    ping_veri(5u, 2u, 3u);   /* aynı yineleme yok sayılır */
    ping_bas(5u, 600u);
    dt_tf(2u, 10 * DT_MS, 3u, false);
    TEST_ASSERT_EQUAL_INT(IDA_LINK_CAGRI_TICK_TF, dt_take(&in));
    TEST_ASSERT_NOT_NULL(in.ping360);
    TEST_ASSERT_EQUAL_UINT16(600u, in.ping360->n_samples);
    TEST_ASSERT_EQUAL_UINT8(IDA_SENS_PING360, in.ping360->hdr.type);
    TEST_ASSERT_EQUAL_UINT8(1u, in.ping360->data[0]);
    TEST_ASSERT_EQUAL_UINT8(2u, in.ping360->data[240]);
    TEST_ASSERT_EQUAL_UINT8(3u, in.ping360->data[599]);
    TEST_ASSERT_EQUAL_UINT8(0u, in.ping360->data[600]);   /* mantıksal alan dışı sıfır */
    dt_put(&in, IDA_LINK_CAGRI_TICK_TF, IDA_RC_OK, NULL, 0u);
}

static void test_ping360_eksik_bozuk_yetim(void)
{
    ida_in_t in;
    ping_veri(6u, 0u, 1u);
    ping_bas(6u, 600u);                    /* 3 parçadan 1'i */
    dt_tf(1u, 5 * DT_MS, 2u, false);
    TEST_ASSERT_EQUAL_INT(IDA_LINK_CAGRI_TICK_TF, dt_take(&in));
    TEST_ASSERT_NULL(in.ping360);
    TEST_ASSERT_EQUAL_UINT32(1u, DT.l.say[IDA_LINK_SAY_PING360_INCOMPLETE]);
    dt_put(&in, IDA_LINK_CAGRI_TICK_TF, IDA_RC_OK, NULL, 0u);
    /* Çelişkili yineleme ve frag_idx ≥ 5. */
    ping_veri(7u, 0u, 1u);
    ping_veri(7u, 0u, 9u);
    ping_veri(7u, 5u, 1u);
    TEST_ASSERT_EQUAL_UINT32(2u, DT.l.say[IDA_LINK_SAY_PING360_BAD]);
    /* Yetim: HEAD'siz yuva 8 parti kapanışında boşalır. */
    ping_veri(8u, 0u, 1u);
    for (uint32_t k = 0; k < 8u; ++k) {
        dt_tf(2u + k, (int64_t)(2u + k) * 5 * DT_MS, k == 0u ? 4u : 0u, false);
        TEST_ASSERT_EQUAL_INT(IDA_LINK_CAGRI_TICK_TF, dt_take(&in));
        dt_put(&in, IDA_LINK_CAGRI_TICK_TF, IDA_RC_OK, NULL, 0u);
    }
    TEST_ASSERT_EQUAL_UINT32(1u, DT.l.say[IDA_LINK_SAY_PING360_ORPHAN]);
}

static void test_algilama(void)
{
    ida_in_t in;
    mavlink_ida_det_batch_t b;
    mavlink_ida_det_record_t r;
    memset(&b, 0, sizeof b);
    b.batch_seq = 9u;
    b.count = 1u;
    memset(&r, 0, sizeof r);
    r.det_seq = 44u;
    r.batch_seq = 9u;
    r.confidence = 0.5f;
    DT_GONDER(MAVLINK_MSG_ID_IDA_DET_BATCH, b);
    DT_GONDER(MAVLINK_MSG_ID_IDA_DET_RECORD, r);
    dt_tf(1u, 5 * DT_MS, 2u, false);
    TEST_ASSERT_EQUAL_INT(IDA_LINK_CAGRI_TICK_TF, dt_take(&in));
    TEST_ASSERT_EQUAL_UINT8(1u, in.n_det_batch);
    TEST_ASSERT_EQUAL_UINT8(1u, in.n_det);
    TEST_ASSERT_EQUAL_UINT32(9u, in.det_batch[0].batch_seq);
    TEST_ASSERT_EQUAL_UINT32(44u, in.det[0].det_seq);
    TEST_ASSERT_EQUAL_INT64(DT.saat, in.det_t_rx[0].ns);
    dt_put(&in, IDA_LINK_CAGRI_TICK_TF, IDA_RC_OK, NULL, 0u);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_tam_parti_step);
    RUN_TEST(test_imusuz_parti_tick);
    RUN_TEST(test_eksik_parti);
    RUN_TEST(test_yinelenen_ve_eski_parti);
    RUN_TEST(test_birlestirme);
    RUN_TEST(test_uye_tasmasi);
    RUN_TEST(test_emniyet_ornegi_kapasite_ile_dusmez);
    RUN_TEST(test_emniyet_kapasite_asimi_oturum_sonu);
    RUN_TEST(test_cagri_cakismasi);
    RUN_TEST(test_izgara_takvimi);
    RUN_TEST(test_ping360_birlestirme);
    RUN_TEST(test_ping360_eksik_bozuk_yetim);
    RUN_TEST(test_algilama);
    return UNITY_END();
}
