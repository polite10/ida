/*
 * test_cikti.c — çıktı (W3-oturum kabul md. 4: olay onaylanana kadar aynı kimlikle yinelenir,
 * onaydan sonra durur, oturum sonunda bekleyenler silinir; A1-K44, K45, K71–K73, K80, K90,
 * K91, K95, K105, K106, K110, K120; A4-K59, K67, K68, K71, K106).
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

static uint64_t olay_kimligi(uint32_t ctr)
{
    return ((uint64_t)3u << 32) | ctr;   /* boot_count = 3 (dt_kur) */
}

static ida_event_t olay(uint32_t ctr, uint8_t kind, uint8_t tube, const ida_in_t *in)
{
    ida_event_t e;
    memset(&e, 0, sizeof e);
    e.event_id = olay_kimligi(ctr);
    e.kind = kind;
    e.tube = tube;
    e.seq = in->tf.seq;
    e.t_decide = in->tf.t;
    e.apply_by_ns = 20000000u;
    e.hold_ns = kind == IDA_EVT_SAFETY_OPEN ? 200000000u : 0u;
    return e;
}

static void step_hazirla(uint32_t seq, ida_in_t *in)
{
    dt_imu(seq, (int64_t)seq * 5 * DT_MS);
    dt_emniyet(seq, (int64_t)seq * 5 * DT_MS, 0u, 42u);
    dt_tf(seq, (int64_t)seq * 5 * DT_MS, 2u, true);
    TEST_ASSERT_EQUAL_INT(IDA_LINK_CAGRI_STEP, dt_take(in));
}

static void sonuc_gonder(uint64_t event_id, uint8_t sonuc)
{
    mavlink_ida_event_result_t r;
    memset(&r, 0, sizeof r);
    r.event_id = event_id;
    r.result = sonuc;
    r.t_apply_ns = 123;
    r.sess = 77u;
    DT_GONDER(MAVLINK_MSG_ID_IDA_EVENT_RESULT, r);
}

/* Sıra (A1-K90): yeni olaylar, ACTUATOR; olaylar KRITIK, ACTUATOR KRITIK, sonuçlar ZORUNLU. */
static void test_olay_actuator_sirasi_ve_alanlar(void)
{
    ida_in_t in;
    ida_event_t e;
    const mavlink_ida_event_t *m;
    const mavlink_ida_actuator_t *a;
    step_hazirla(1u, &in);
    e = olay(1u, (uint8_t)IDA_EVT_SAFETY_OPEN, 0u, &in);
    dt_put(&in, IDA_LINK_CAGRI_STEP, IDA_RC_OK, &e, 1u);
    TEST_ASSERT_EQUAL_UINT32(MAVLINK_MSG_ID_IDA_EVENT, DT.kayit[0].msgid);
    TEST_ASSERT_EQUAL_INT(IDA_LINK_SINIF_KRITIK, DT.kayit[0].sinif);
    TEST_ASSERT_EQUAL_UINT32(MAVLINK_MSG_ID_IDA_ACTUATOR, DT.kayit[1].msgid);
    TEST_ASSERT_EQUAL_INT(IDA_LINK_SINIF_KRITIK, DT.kayit[1].sinif);
    m = (const mavlink_ida_event_t *)(const void *)DT.kayit[0].yuk;
    TEST_ASSERT_TRUE(m->event_id == olay_kimligi(1u));
    TEST_ASSERT_EQUAL_UINT32(42u, m->safety_rev);            /* A1-K106 */
    TEST_ASSERT_EQUAL_UINT8(IDAW_CALL_KIND_STEP, m->call_kind);
    TEST_ASSERT_EQUAL_UINT16(77u, m->sess);
    TEST_ASSERT_EQUAL_UINT32(1u, m->batch_seq);
    a = (const mavlink_ida_actuator_t *)(const void *)DT.kayit[1].yuk;
    TEST_ASSERT_EQUAL_UINT8(1u, a->n_events);
    TEST_ASSERT_EQUAL_UINT32(1u, a->out_seq);
    TEST_ASSERT_EQUAL_UINT16(IDAW_ACT_FLAGS_THR_VALID | IDAW_ACT_FLAGS_IS_SAFE, a->act_flags);
    TEST_ASSERT_EQUAL_UINT32(100000000u, a->valid_for_ns);
    for (unsigned i = 0; i < 8u; ++i) {
        TEST_ASSERT_EQUAL_UINT16(1500u, a->pwm_us[i]);
    }
    TEST_ASSERT_EQUAL_UINT8(IDA_RC_OK, a->core_rc);
    /* Telemetri: STATUS, FC_STATUS, CALL_TRACE (KAYBEDILEBILIR). */
    TEST_ASSERT_EQUAL_UINT(1u, dt_say(MAVLINK_MSG_ID_IDA_STATUS));
    TEST_ASSERT_EQUAL_UINT(1u, dt_say(MAVLINK_MSG_ID_IDA_CALL_TRACE));
    TEST_ASSERT_EQUAL_INT(IDA_LINK_SINIF_KAYBEDILEBILIR,
                          dt_bul(MAVLINK_MSG_ID_IDA_CALL_TRACE, 0u)->sinif);
}

static void test_olay_yinelemesi_ve_onay(void)
{
    ida_in_t in;
    ida_event_t e;
    step_hazirla(1u, &in);
    e = olay(1u, (uint8_t)IDA_EVT_SAFETY_OPEN, 1u, &in);
    dt_put(&in, IDA_LINK_CAGRI_STEP, IDA_RC_OK, &e, 1u);
    dt_kayit_sil();
    dt_ilerle(14 * DT_MS);
    ida_link_ust_poll_tx(&DT.l);
    TEST_ASSERT_EQUAL_UINT(0u, dt_say(MAVLINK_MSG_ID_IDA_EVENT));
    dt_ilerle(1 * DT_MS);
    ida_link_ust_poll_tx(&DT.l);
    TEST_ASSERT_EQUAL_UINT(1u, dt_say(MAVLINK_MSG_ID_IDA_EVENT));   /* 15 ms: yineleme */
    {
        const dt_cerceve_t *c = dt_bul(MAVLINK_MSG_ID_IDA_EVENT, 0u);
        const mavlink_ida_event_t *m = (const mavlink_ida_event_t *)(const void *)c->yuk;
        TEST_ASSERT_TRUE(m->event_id == olay_kimligi(1u));
        TEST_ASSERT_EQUAL_INT(IDA_LINK_SINIF_ZORUNLU, c->sinif);
        TEST_ASSERT_EQUAL_UINT32(42u, m->safety_rev);              /* aynı yük */
    }
    sonuc_gonder(olay_kimligi(1u), (uint8_t)IDA_EVR_APPLIED);
    dt_ilerle(15 * DT_MS);
    ida_link_ust_poll_tx(&DT.l);
    TEST_ASSERT_EQUAL_UINT(1u, dt_say(MAVLINK_MSG_ID_IDA_EVENT));   /* onaydan sonra durur */
    /* Sonuç sonraki çağrının evres[]'inde (A4-K68). */
    dt_imu(2u, 10 * DT_MS);
    dt_tf(2u, 10 * DT_MS, 2u, true);
    TEST_ASSERT_EQUAL_INT(IDA_LINK_CAGRI_STEP, dt_take(&in));
    TEST_ASSERT_EQUAL_UINT8(1u, in.n_evres);
    TEST_ASSERT_TRUE(in.evres[0].event_id == olay_kimligi(1u));
    TEST_ASSERT_EQUAL_UINT8(IDA_EVR_APPLIED, in.evres[0].result);
    TEST_ASSERT_EQUAL_INT64(123, in.evres[0].t_apply.ns);
    dt_put(&in, IDA_LINK_CAGRI_STEP, IDA_RC_OK, NULL, 0u);
}

static void test_son_gecerlilik_ve_sonuc_payi(void)
{
    ida_in_t in;
    ida_event_t e;
    step_hazirla(1u, &in);
    e = olay(1u, (uint8_t)IDA_EVT_SAFETY_OPEN, 0u, &in);   /* apply_by 20 ms */
    dt_put(&in, IDA_LINK_CAGRI_STEP, IDA_RC_OK, &e, 1u);
    dt_kayit_sil();
    for (unsigned i = 0; i < 12u; ++i) {
        dt_ilerle(5 * DT_MS);
        ida_link_ust_poll_tx(&DT.l);
    }
    /* 15 ms'de bir yineleme; 20 ms'den sonra yok. */
    TEST_ASSERT_EQUAL_UINT(1u, dt_say(MAVLINK_MSG_ID_IDA_EVENT));
    /* 20 + 95 ms dolunca EXPIRED_NO_ACK çekirdeğe. */
    dt_ilerle(56 * DT_MS);
    TEST_ASSERT_NOT_EQUAL(IDA_LINK_CAGRI_YOK, dt_take(&in));
    TEST_ASSERT_EQUAL_UINT8(1u, in.n_evres);
    TEST_ASSERT_EQUAL_UINT8(IDA_EVR_EXPIRED_NO_ACK, in.evres[0].result);
    dt_put(&in, (ida_link_cagri_t)DT.l.ozel_cagri_tur, IDA_RC_OK, NULL, 0u);
}

/* Oturum sonunda bekleyenler: yineleme durur, SESSION_END sonucu (A4-K71). */
static void test_oturum_sonu_bekleyenler(void)
{
    ida_in_t in;
    ida_event_t e[2];
    mavlink_ida_session_close_t c;
    step_hazirla(1u, &in);
    e[0] = olay(1u, (uint8_t)IDA_EVT_SAFETY_OPEN, 0u, &in);
    e[1] = olay(2u, (uint8_t)IDA_EVT_SAFETY_CLOSE, (uint8_t)IDA_TUBE_ALL, &in);
    dt_put(&in, IDA_LINK_CAGRI_STEP, IDA_RC_OK, e, 2u);
    memset(&c, 0, sizeof c);
    c.sess = 77u;
    DT_GONDER(MAVLINK_MSG_ID_IDA_SESSION_CLOSE, c);
    dt_kayit_sil();
    dt_ilerle(30 * DT_MS);
    ida_link_ust_poll_tx(&DT.l);
    TEST_ASSERT_EQUAL_UINT(0u, dt_say(MAVLINK_MSG_ID_IDA_EVENT));
    TEST_ASSERT_EQUAL_INT(IDA_LINK_CAGRI_OTURUM_SONU, dt_take(&in));
    TEST_ASSERT_EQUAL_UINT8(2u, in.n_evres);
    TEST_ASSERT_EQUAL_UINT8(IDA_EVR_SESSION_END, in.evres[0].result);
    TEST_ASSERT_EQUAL_UINT8(IDA_EVR_SESSION_END, in.evres[1].result);
    /* Oturum sonu çağrısının SAFETY_CLOSE'u iletilmez (A1 4.2 akışı). */
    e[0] = olay(3u, (uint8_t)IDA_EVT_SAFETY_CLOSE, (uint8_t)IDA_TUBE_ALL, &in);
    dt_put(&in, IDA_LINK_CAGRI_OTURUM_SONU, IDA_RC_OK, e, 1u);
    TEST_ASSERT_EQUAL_UINT(0u, dt_say(MAVLINK_MSG_ID_IDA_EVENT));
    TEST_ASSERT_EQUAL_UINT(0u, dt_say(MAVLINK_MSG_ID_IDA_ACTUATOR));
    dt_ilerle(20 * DT_MS);
    TEST_ASSERT_EQUAL_INT(IDA_LINK_CAGRI_IZGARA, dt_take(&in));
    TEST_ASSERT_EQUAL_UINT8(1u, in.n_evres);
    TEST_ASSERT_EQUAL_UINT8(IDA_EVR_SESSION_END, in.evres[0].result);
    dt_put(&in, IDA_LINK_CAGRI_IZGARA, IDA_RC_OK, NULL, 0u);
}

/* İlk kabul kapısı (A4-K67): kip, kimlik, yuva ve kova. */
static void test_ilk_kabul_kapisi(void)
{
    ida_in_t in;
    ida_event_t e[4];
    step_hazirla(1u, &in);
    e[0] = olay(5u, (uint8_t)IDA_EVT_FIRE, 0u, &in);          /* armed değil: DROPPED_GATE */
    e[1] = olay(6u, (uint8_t)IDA_EVT_SAFETY_OPEN, 0u, &in);   /* geçer */
    e[2] = olay(6u, (uint8_t)IDA_EVT_SAFETY_OPEN, 1u, &in);   /* kimlik artmadı */
    e[3] = e[1];
    e[3].event_id = ((uint64_t)4u << 32) | 7u;                  /* boot_count yanlış */
    dt_put(&in, IDA_LINK_CAGRI_STEP, IDA_RC_OK, e, 4u);
    TEST_ASSERT_EQUAL_UINT(1u, dt_say(MAVLINK_MSG_ID_IDA_EVENT));
    TEST_ASSERT_EQUAL_UINT8(1u, ((const mavlink_ida_actuator_t *)dt_son(
                                    MAVLINK_MSG_ID_IDA_ACTUATOR))->n_events);
    dt_imu(2u, 10 * DT_MS);
    dt_tf(2u, 10 * DT_MS, 1u, true);
    TEST_ASSERT_EQUAL_INT(IDA_LINK_CAGRI_STEP, dt_take(&in));
    TEST_ASSERT_EQUAL_UINT8(3u, in.n_evres);
    for (unsigned i = 0; i < 3u; ++i) {
        TEST_ASSERT_EQUAL_UINT8(IDA_EVR_DROPPED_GATE, in.evres[i].result);
    }
    /* IMU_REJECTED dönen step'te izin açan olay geçmez (A4-K64: yalnız step OK). */
    e[0] = olay(8u, (uint8_t)IDA_EVT_SAFETY_OPEN, 0u, &in);
    dt_put(&in, IDA_LINK_CAGRI_STEP, IDA_RC_IMU_REJECTED, e, 1u);
    TEST_ASSERT_EQUAL_UINT(1u, dt_say(MAVLINK_MSG_ID_IDA_EVENT));
    /* Izgara tick'inde de geçmez. */
    dt_ilerle(20 * DT_MS);
    TEST_ASSERT_EQUAL_INT(IDA_LINK_CAGRI_IZGARA, dt_take(&in));
    TEST_ASSERT_EQUAL_UINT8(IDA_EVR_DROPPED_GATE, in.evres[0].result);
    e[0] = olay(9u, (uint8_t)IDA_EVT_SAFETY_OPEN, 0u, &in);
    dt_put(&in, IDA_LINK_CAGRI_IZGARA, IDA_RC_OK, e, 1u);
    TEST_ASSERT_EQUAL_UINT(1u, dt_say(MAVLINK_MSG_ID_IDA_EVENT));
}

static void test_yedi_acan_yuva_ve_kova(void)
{
    ida_in_t in;
    ida_event_t e[4];
    uint32_t ctr = 1u;
    /* Kova patlaması 4: ilk çağrıda dört izin açan olay geçer. */
    step_hazirla(1u, &in);
    for (unsigned i = 0; i < 4u; ++i) {
        e[i] = olay(ctr++, (uint8_t)IDA_EVT_SAFETY_OPEN, 0u, &in);
    }
    dt_put(&in, IDA_LINK_CAGRI_STEP, IDA_RC_OK, e, 4u);
    TEST_ASSERT_EQUAL_UINT8(4u, ((const mavlink_ida_actuator_t *)dt_son(
                                    MAVLINK_MSG_ID_IDA_ACTUATOR))->n_events);
    /* Aynı anda dört daha: kova boş -> DROPPED_NO_SLOT. */
    dt_imu(2u, 10 * DT_MS);
    dt_tf(2u, 10 * DT_MS, 1u, true);
    TEST_ASSERT_EQUAL_INT(IDA_LINK_CAGRI_STEP, dt_take(&in));
    for (unsigned i = 0; i < 4u; ++i) {
        e[i] = olay(ctr++, (uint8_t)IDA_EVT_SAFETY_OPEN, 0u, &in);
    }
    dt_put(&in, IDA_LINK_CAGRI_STEP, IDA_RC_OK, e, 4u);
    TEST_ASSERT_EQUAL_UINT8(0u, ((const mavlink_ida_actuator_t *)dt_son(
                                    MAVLINK_MSG_ID_IDA_ACTUATOR))->n_events);
    /* 5 ms sonra kova dolu; 3 yuva kaldı (7 sınırı): 3 geçer, 1 DROPPED_NO_SLOT. */
    dt_ilerle(5 * DT_MS);
    dt_imu(3u, 15 * DT_MS);
    dt_tf(3u, 15 * DT_MS, 1u, true);
    TEST_ASSERT_EQUAL_INT(IDA_LINK_CAGRI_STEP, dt_take(&in));
    TEST_ASSERT_EQUAL_UINT8(4u, in.n_evres);   /* önceki dört NO_SLOT */
    for (unsigned i = 0; i < 4u; ++i) {
        TEST_ASSERT_EQUAL_UINT8(IDA_EVR_DROPPED_NO_SLOT, in.evres[i].result);
        e[i] = olay(ctr++, (uint8_t)IDA_EVT_SAFETY_OPEN, 0u, &in);
    }
    dt_put(&in, IDA_LINK_CAGRI_STEP, IDA_RC_OK, e, 4u);
    TEST_ASSERT_EQUAL_UINT8(3u, ((const mavlink_ida_actuator_t *)dt_son(
                                    MAVLINK_MSG_ID_IDA_ACTUATOR))->n_events);
}

/* Sekiz yuva doluyken SAFETY_CLOSE: oturum EVENT_OVERFLOW ile biter (A1-K80). */
static void test_event_overflow(void)
{
    ida_in_t in;
    ida_event_t e[4];
    uint32_t ctr = 1u;
    uint32_t seq = 1u;
    unsigned dolu = 0u;
    while (dolu < 8u) {
        unsigned n = 0u;
        step_hazirla(seq, &in);
        while (n < 2u && dolu < 8u) {
            e[n++] = olay(ctr++, (uint8_t)(dolu < 7u ? IDA_EVT_SAFETY_OPEN : IDA_EVT_SAFETY_CLOSE),
                          dolu < 7u ? 0u : (uint8_t)IDA_TUBE_ALL, &in);
            dolu += 1u;
        }
        dt_put(&in, IDA_LINK_CAGRI_STEP, IDA_RC_OK, e, (uint8_t)n);
        dt_ilerle(5 * DT_MS);
        seq += 1u;
    }
    TEST_ASSERT_EQUAL_INT(IDA_LINK_UST_RUNNING, ida_link_ust_durum(&DT.l));
    step_hazirla(seq, &in);
    e[0] = olay(ctr, (uint8_t)IDA_EVT_SAFETY_CLOSE, (uint8_t)IDA_TUBE_ALL, &in);
    dt_put(&in, IDA_LINK_CAGRI_STEP, IDA_RC_OK, e, 1u);
    TEST_ASSERT_EQUAL_INT(IDA_LINK_UST_SONLANIYOR, ida_link_ust_durum(&DT.l));
    TEST_ASSERT_EQUAL_UINT8(IDAW_CLOSE_REASON_EVENT_OVERFLOW,
                            ((const mavlink_ida_session_close_t *)dt_son(
                                 MAVLINK_MSG_ID_IDA_SESSION_CLOSE))->reason);
}

/* A4-K59: sonlu olmayan thr_cmd nötr güvenli çıktıyla değiştirilir, olaylar DROPPED_GATE. */
static void test_cikti_duzeltme(void)
{
    ida_in_t in;
    ida_out_t out;
    const mavlink_ida_actuator_t *a;
    uint32_t nan_bit = 0x7FC00000u;
    step_hazirla(1u, &in);
    dt_cikti(&in, IDA_LINK_CAGRI_STEP, &out);
    out.is_safe = false;
    out.armed = true;
    memcpy(&out.thr_cmd[2], &nan_bit, sizeof nan_bit);
    out.events[0] = olay(1u, (uint8_t)IDA_EVT_SAFETY_OPEN, 0u, &in);
    out.n_events = 1u;
    ida_link_ust_put_out(&DT.l, &in, IDA_RC_OK, &out);
    a = (const mavlink_ida_actuator_t *)dt_son(MAVLINK_MSG_ID_IDA_ACTUATOR);
    TEST_ASSERT_NOT_EQUAL(0u, a->act_flags & IDAW_ACT_FLAGS_OUT_FIXED);
    TEST_ASSERT_NOT_EQUAL(0u, a->act_flags & IDAW_ACT_FLAGS_IS_SAFE);
    for (unsigned i = 0; i < 8u; ++i) {
        TEST_ASSERT_EQUAL_UINT16(1500u, a->pwm_us[i]);
    }
    TEST_ASSERT_EQUAL_UINT(0u, dt_say(MAVLINK_MSG_ID_IDA_EVENT));
    TEST_ASSERT_EQUAL_UINT32(1u, DT.l.say[IDA_LINK_SAY_OUT_FIXED]);
}

static void test_pwm_ve_hata_donusu(void)
{
    ida_in_t in;
    ida_out_t out;
    const mavlink_ida_actuator_t *a;
    step_hazirla(1u, &in);
    dt_cikti(&in, IDA_LINK_CAGRI_STEP, &out);
    out.is_safe = false;
    out.armed = true;
    out.thr_cmd[0] = 1.0f;
    out.thr_cmd[1] = -1.0f;
    out.thr_cmd[2] = 0.5f;
    ida_link_ust_put_out(&DT.l, &in, IDA_RC_OK, &out);
    a = (const mavlink_ida_actuator_t *)dt_son(MAVLINK_MSG_ID_IDA_ACTUATOR);
    TEST_ASSERT_EQUAL_UINT16(1900u, a->pwm_us[0]);
    TEST_ASSERT_EQUAL_UINT16(1100u, a->pwm_us[1]);
    TEST_ASSERT_EQUAL_UINT16(1700u, a->pwm_us[2]);
    TEST_ASSERT_EQUAL_UINT16(1500u, a->pwm_us[3]);
    TEST_ASSERT_NOT_EQUAL(0u, a->act_flags & IDAW_ACT_FLAGS_ARMED);
    /* BAD_INPUT dönen zaman çerçeveli çağrıya da ACTUATOR (A1-K45), THR_VALID = 0. */
    dt_imu(2u, 10 * DT_MS);
    dt_tf(2u, 10 * DT_MS, 1u, true);
    TEST_ASSERT_EQUAL_INT(IDA_LINK_CAGRI_STEP, dt_take(&in));
    memset(&out, 0, sizeof out);
    out.src = (uint8_t)IDA_SRC_STEP;
    ida_link_ust_put_out(&DT.l, &in, IDA_RC_BAD_INPUT, &out);
    a = (const mavlink_ida_actuator_t *)dt_son(MAVLINK_MSG_ID_IDA_ACTUATOR);
    TEST_ASSERT_EQUAL_UINT32(2u, a->batch_seq);
    TEST_ASSERT_EQUAL_UINT8(IDA_RC_BAD_INPUT, a->core_rc);
    TEST_ASSERT_EQUAL_UINT16(0u, a->act_flags & IDAW_ACT_FLAGS_THR_VALID);
    TEST_ASSERT_EQUAL_UINT32(0u, a->valid_for_ns);
    TEST_ASSERT_EQUAL_UINT16(0u, a->pwm_us[0]);
    TEST_ASSERT_EQUAL_UINT32(2u, a->out_seq);
}

static void test_izgara_ancak_guvenli(void)
{
    ida_in_t in;
    ida_out_t out;
    dt_ilerle(20 * DT_MS);
    TEST_ASSERT_EQUAL_INT(IDA_LINK_CAGRI_IZGARA, dt_take(&in));
    memset(&out, 0, sizeof out);
    out.src = (uint8_t)IDA_SRC_TICK;   /* thr_valid = 0: ASYNC gönderilmez (A1-K44) */
    ida_link_ust_put_out(&DT.l, &in, IDA_RC_OK, &out);
    TEST_ASSERT_EQUAL_UINT(0u, dt_say(MAVLINK_MSG_ID_IDA_ACTUATOR));
    dt_ilerle(10 * DT_MS);
    TEST_ASSERT_EQUAL_INT(IDA_LINK_CAGRI_IZGARA, dt_take(&in));
    out.thr_valid = true;              /* is_safe = 0: EMNİYET okuması, gönderilmez */
    ida_link_ust_put_out(&DT.l, &in, IDA_RC_OK, &out);
    TEST_ASSERT_EQUAL_UINT(0u, dt_say(MAVLINK_MSG_ID_IDA_ACTUATOR));
}

static void test_komut_sonucu_ve_save_crc(void)
{
    ida_in_t in;
    ida_out_t out;
    static const uint8_t blok[9] = {'1', '2', '3', '4', '5', '6', '7', '8', '9'};
    const mavlink_ida_cmd_result_t *r;
    step_hazirla(1u, &in);
    dt_cikti(&in, IDA_LINK_CAGRI_STEP, &out);
    out.n_results = 1u;
    out.results[0].cmd_id = 55u;
    out.results[0].kind = (uint8_t)IDA_RES_PARAM_SAVE;
    out.results[0].ack = (uint8_t)IDA_ACK_ACCEPTED;
    out.results[0].save.gen = 6u;
    out.results[0].save.blob = blok;
    out.results[0].save.len = 9u;
    ida_link_ust_put_out(&DT.l, &in, IDA_RC_OK, &out);
    r = dt_sonuc(55u);
    TEST_ASSERT_NOT_NULL(r);
    TEST_ASSERT_EQUAL_UINT32(0xCBF43926u, r->save_crc);   /* CRC-32/ISO-HDLC denetim değeri */
    TEST_ASSERT_EQUAL_UINT32(6u, r->save_gen);
    TEST_ASSERT_EQUAL_UINT16(9u, r->save_len);
    TEST_ASSERT_EQUAL_INT(IDA_LINK_SINIF_ZORUNLU, dt_bul(MAVLINK_MSG_ID_IDA_CMD_RESULT, 0u)->sinif);
}

static void test_telemetri_hizlari_ve_iz(void)
{
    ida_in_t in;
    ida_out_t out;
    const mavlink_ida_call_trace_t *t;
    for (uint32_t s = 1u; s <= 8u; ++s) {
        step_hazirla(s, &in);
        dt_cikti(&in, IDA_LINK_CAGRI_STEP, &out);
        out.nav_valid = true;
        ida_link_ust_put_out(&DT.l, &in, IDA_RC_OK, &out);
        dt_ilerle(5 * DT_MS);
    }
    /* NAV yalnız batch_seq 4'ün katında; STATUS 100 ms'de bir; çağrı başına bir iz. */
    TEST_ASSERT_EQUAL_UINT(2u, dt_say(MAVLINK_MSG_ID_IDA_NAV));
    TEST_ASSERT_EQUAL_UINT(1u, dt_say(MAVLINK_MSG_ID_IDA_STATUS));
    TEST_ASSERT_EQUAL_UINT(8u, dt_say(MAVLINK_MSG_ID_IDA_CALL_TRACE));
    TEST_ASSERT_EQUAL_UINT(1u, dt_say(MAVLINK_MSG_ID_IDA_LINK_STATS));
    t = (const mavlink_ida_call_trace_t *)dt_son(MAVLINK_MSG_ID_IDA_CALL_TRACE);
    TEST_ASSERT_EQUAL_UINT32(8u, t->batch_seq);
    TEST_ASSERT_EQUAL_UINT8(IDAW_CALL_KIND_STEP, t->call_kind);
    TEST_ASSERT_EQUAL_UINT8(3u, t->n_rx);   /* tf, imu, samples[0] */
    {
        const mavlink_ida_status_t *s = (const mavlink_ida_status_t *)dt_son(MAVLINK_MSG_ID_IDA_STATUS);
        TEST_ASSERT_EQUAL_UINT8(IDAW_LINK_STATE_RUNNING, s->link_state);
        TEST_ASSERT_EQUAL_UINT32(0xCAFEBABEu, s->param_blob_crc);
        TEST_ASSERT_EQUAL_UINT8(IDAW_HEALTH_CORE_OK, s->health);
    }
}

/* (ii) sınıfının taşması oturumu TX_OVERFLOW ile bitirir (A1-K110). */
static void test_zorunlu_tasma(void)
{
    ida_in_t in;
    step_hazirla(1u, &in);
    dt_put(&in, IDA_LINK_CAGRI_STEP, IDA_RC_OK, NULL, 0u);
    DT.dolu[IDA_LINK_SINIF_ZORUNLU] = true;
    dt_cmd_simple(1u, 99u, 0u);   /* LINK_BAD_FIELD sonucu gönderilemez */
    DT.dolu[IDA_LINK_SINIF_ZORUNLU] = false;
    TEST_ASSERT_EQUAL_INT(IDA_LINK_UST_SONLANIYOR, ida_link_ust_durum(&DT.l));
    TEST_ASSERT_EQUAL_UINT8(IDAW_CLOSE_REASON_TX_OVERFLOW,
                            ((const mavlink_ida_session_close_t *)dt_son(
                                 MAVLINK_MSG_ID_IDA_SESSION_CLOSE))->reason);
    TEST_ASSERT_EQUAL_INT(IDA_LINK_CAGRI_OTURUM_SONU, dt_take(&in));
    dt_put(&in, IDA_LINK_CAGRI_OTURUM_SONU, IDA_RC_OK, NULL, 0u);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_olay_actuator_sirasi_ve_alanlar);
    RUN_TEST(test_olay_yinelemesi_ve_onay);
    RUN_TEST(test_son_gecerlilik_ve_sonuc_payi);
    RUN_TEST(test_oturum_sonu_bekleyenler);
    RUN_TEST(test_ilk_kabul_kapisi);
    RUN_TEST(test_yedi_acan_yuva_ve_kova);
    RUN_TEST(test_event_overflow);
    RUN_TEST(test_cikti_duzeltme);
    RUN_TEST(test_pwm_ve_hata_donusu);
    RUN_TEST(test_izgara_ancak_guvenli);
    RUN_TEST(test_komut_sonucu_ve_save_crc);
    RUN_TEST(test_telemetri_hizlari_ve_iz);
    RUN_TEST(test_zorunlu_tasma);
    return UNITY_END();
}
