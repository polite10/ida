/*
 * test_oturum.c — alım ve oturum (W3-oturum kabul md. 1: temiz el sıkışma, yanlış sürüm ve
 * dialect, kopma ve yeniden kurulum; A1-K11–K27, K107).
 */
#include "destek_oturum.h"

#include "unity.h"

#include <string.h>

void setUp(void);
void tearDown(void);
void setUp(void) { dt_kur(IDA_LINK_UST_ROL_KART); }
void tearDown(void) {}

static void sim_hello(uint32_t nonce)
{
    mavlink_ida_sim_hello_t h;
    memset(&h, 0, sizeof h);
    h.dialect_hash = IDA_LINK_DIALECT_HASH;
    h.nonce = nonce;
    h.run_id = 7u;
    h.ctrl_period_ns = 5000000u;
    h.proto_major = 1u;
    DT_GONDER(MAVLINK_MSG_ID_IDA_SIM_HELLO, h);
}

static void accept_gonder(uint32_t nonce_sim, uint32_t nonce_board, uint16_t sess, uint8_t sonuc)
{
    mavlink_ida_session_accept_t a;
    memset(&a, 0, sizeof a);
    a.nonce_sim = nonce_sim;
    a.nonce_board = nonce_board;
    a.sess = sess;
    a.result = sonuc;
    DT_GONDER(MAVLINK_MSG_ID_IDA_SESSION_ACCEPT, a);
}

/* Oturum sonu çağrısını yapar (take OTURUM_SONU, put_out). */
static void oturum_sonu_cagrisi(void)
{
    ida_in_t in;
    TEST_ASSERT_EQUAL_INT(IDA_LINK_CAGRI_OTURUM_SONU, dt_take(&in));
    TEST_ASSERT_FALSE(in.tf.present);
    TEST_ASSERT_EQUAL_UINT8(IDA_LINKEV_SESSION_END, in.plat.link_event);
    TEST_ASSERT_EQUAL_UINT8(0u, in.n_samples);
    TEST_ASSERT_EQUAL_UINT8(0u, in.n_cmds);
    dt_put(&in, IDA_LINK_CAGRI_OTURUM_SONU, IDA_RC_OK, NULL, 0u);
}

static void test_kurulum_denetimi(void)
{
    static ida_link_ust_t l;   /* ≈16 KB: yığına konmaz (MSVC /analyze C6262) */
    ida_link_ust_ayar_t a = DT.ayar;
    TEST_ASSERT_EQUAL_INT(IDA_LINK_HATA_ARGUMAN, ida_link_ust_kur(NULL, &a, &DT.plat));
    a.t_bag_ns = 100000000u;
    TEST_ASSERT_EQUAL_INT(IDA_LINK_HATA_ARGUMAN, ida_link_ust_kur(&l, &a, &DT.plat));
    a = DT.ayar;
    a.esc_notr_us = a.esc_ust_us;
    TEST_ASSERT_EQUAL_INT(IDA_LINK_HATA_ARGUMAN, ida_link_ust_kur(&l, &a, &DT.plat));
    a = DT.ayar;
    a.init_rc = (uint8_t)IDA_RC_NOT_INIT;
    TEST_ASSERT_EQUAL_INT(IDA_LINK_HATA_ARGUMAN, ida_link_ust_kur(&l, &a, &DT.plat));
    /* Kurulmamış örnek hiçbir şey yapmaz. */
    {
        ida_in_t in;
        TEST_ASSERT_EQUAL_INT(IDA_LINK_CAGRI_YOK, ida_link_ust_take(&l, &in));
    }
}

static void test_duyuru_ve_alanlar(void)
{
    const mavlink_ida_board_hello_t *b;
    ida_link_ust_poll_tx(&DT.l);
    TEST_ASSERT_EQUAL_UINT(1u, dt_say(MAVLINK_MSG_ID_IDA_BOARD_HELLO));
    b = (const mavlink_ida_board_hello_t *)dt_son(MAVLINK_MSG_ID_IDA_BOARD_HELLO);
    TEST_ASSERT_EQUAL_UINT32(0u, b->peer_nonce);
    TEST_ASSERT_NOT_EQUAL(0u, b->nonce);
    TEST_ASSERT_TRUE(b->dialect_hash == IDA_LINK_DIALECT_HASH);
    TEST_ASSERT_EQUAL_UINT8(1u, b->proto_major);
    TEST_ASSERT_EQUAL_UINT8(0u, b->proto_minor);
    TEST_ASSERT_EQUAL_UINT32(5000000u, b->ctrl_period_ns);
    TEST_ASSERT_EQUAL_UINT8(IDAW_ROLE_BOARD, b->role);
    TEST_ASSERT_EQUAL_UINT32(3u, b->boot_count);
    TEST_ASSERT_EQUAL_UINT32(0xCAFEBABEu, b->param_blob_crc);
    TEST_ASSERT_EQUAL_UINT16(120u, b->param_blob_len);
    TEST_ASSERT_EQUAL_UINT32(4u, b->param_gen);
    TEST_ASSERT_EQUAL_UINT8(IDA_RC_OK, b->init_rc);
    TEST_ASSERT_EQUAL_UINT16(IDA_DET_SCHEMA_MAJOR, b->det_schema_major);
    /* 200 ms dolmadan yeni duyuru yok; dolunca aynı nonce ile. */
    dt_ilerle(150 * DT_MS);
    ida_link_ust_poll_tx(&DT.l);
    TEST_ASSERT_EQUAL_UINT(1u, dt_say(MAVLINK_MSG_ID_IDA_BOARD_HELLO));
    dt_ilerle(60 * DT_MS);
    ida_link_ust_poll_tx(&DT.l);
    TEST_ASSERT_EQUAL_UINT(2u, dt_say(MAVLINK_MSG_ID_IDA_BOARD_HELLO));
    TEST_ASSERT_EQUAL_UINT32(b->nonce, ((const mavlink_ida_board_hello_t *)dt_son(
                                            MAVLINK_MSG_ID_IDA_BOARD_HELLO))->nonce);
    /* IDLE'da başka çerçeve yok (A1-K11). */
    TEST_ASSERT_EQUAL_UINT(2u, DT.n);
}

static void test_temiz_el_sikisma(void)
{
    const mavlink_ida_keepalive_t *k;
    ida_in_t in;
    dt_el_sikis(0x55u, 0x1234u);
    TEST_ASSERT_EQUAL_UINT16(0x1234u, ida_link_ust_sess(&DT.l));
    k = (const mavlink_ida_keepalive_t *)dt_son(MAVLINK_MSG_ID_IDA_KEEPALIVE);
    TEST_ASSERT_EQUAL_UINT16(0x1234u, k->sess);
    TEST_ASSERT_EQUAL_UINT8(IDAW_LINK_STATE_RUNNING, k->state);
    /* SESSION_START sonraki ilk çağrıda (ızgara tick'i). */
    dt_ilerle(20 * DT_MS);
    TEST_ASSERT_EQUAL_INT(IDA_LINK_CAGRI_IZGARA, dt_take(&in));
    TEST_ASSERT_EQUAL_UINT8(IDA_LINKEV_SESSION_START, in.plat.link_event);
    TEST_ASSERT_EQUAL_UINT8(IDA_LINK_RUNNING, in.plat.link_state);
    dt_put(&in, IDA_LINK_CAGRI_IZGARA, IDA_RC_OK, NULL, 0u);
    dt_ilerle(20 * DT_MS);
    TEST_ASSERT_EQUAL_INT(IDA_LINK_CAGRI_IZGARA, dt_take(&in));
    TEST_ASSERT_EQUAL_UINT8(IDA_LINKEV_NONE, in.plat.link_event);
    dt_put(&in, IDA_LINK_CAGRI_IZGARA, IDA_RC_OK, NULL, 0u);
    /* Bir sonraki BOARD_HELLO'da sessions_since_boot ve prev_sess (yeni el sıkışmada). */
    TEST_ASSERT_EQUAL_UINT32(1u, DT.l.say[IDA_LINK_SAY_SESSIONS]);
}

static void test_ayni_nonce_yinelemesi(void)
{
    const mavlink_ida_board_hello_t *b;
    uint32_t n1;
    sim_hello(0x77u);
    b = (const mavlink_ida_board_hello_t *)dt_son(MAVLINK_MSG_ID_IDA_BOARD_HELLO);
    n1 = b->nonce;
    sim_hello(0x77u);
    TEST_ASSERT_EQUAL_UINT(2u, dt_say(MAVLINK_MSG_ID_IDA_BOARD_HELLO));
    b = (const mavlink_ida_board_hello_t *)dt_son(MAVLINK_MSG_ID_IDA_BOARD_HELLO);
    TEST_ASSERT_EQUAL_UINT32(n1, b->nonce);
    TEST_ASSERT_EQUAL_UINT32(0x77u, b->peer_nonce);
    /* Yanlış nonce'lu kabul atılır. */
    accept_gonder(0x77u, n1 + 1u, 9u, (uint8_t)IDAW_SESSION_RESULT_OK);
    TEST_ASSERT_EQUAL_INT(IDA_LINK_UST_HANDSHAKE, ida_link_ust_durum(&DT.l));
    accept_gonder(0x78u, n1, 9u, (uint8_t)IDAW_SESSION_RESULT_OK);
    TEST_ASSERT_EQUAL_INT(IDA_LINK_UST_HANDSHAKE, ida_link_ust_durum(&DT.l));
    accept_gonder(0x77u, n1, 9u, (uint8_t)IDAW_SESSION_RESULT_OK);
    TEST_ASSERT_EQUAL_INT(IDA_LINK_UST_RUNNING, ida_link_ust_durum(&DT.l));
    /* RUNNING'de aynı nonce yok sayılır. */
    dt_kayit_sil();
    sim_hello(0x77u);
    TEST_ASSERT_EQUAL_INT(IDA_LINK_UST_RUNNING, ida_link_ust_durum(&DT.l));
    TEST_ASSERT_EQUAL_UINT(0u, dt_say(MAVLINK_MSG_ID_IDA_BOARD_HELLO));
}

static void test_yanlis_surum_ve_dialect(void)
{
    /* Sim REJECT_PROTO ve REJECT_DIALECT gönderir: kart IDLE'a döner, oturum kurulmaz. */
    const uint8_t sonuclar[2] = {(uint8_t)IDAW_SESSION_RESULT_REJECT_PROTO,
                                 (uint8_t)IDAW_SESSION_RESULT_REJECT_DIALECT};
    for (unsigned i = 0; i < 2u; ++i) {
        const mavlink_ida_board_hello_t *b;
        ida_in_t in;
        sim_hello(0x100u + i);
        b = (const mavlink_ida_board_hello_t *)dt_son(MAVLINK_MSG_ID_IDA_BOARD_HELLO);
        accept_gonder(0x100u + i, b->nonce, 5u, sonuclar[i]);
        TEST_ASSERT_EQUAL_INT(IDA_LINK_UST_IDLE, ida_link_ust_durum(&DT.l));
        TEST_ASSERT_EQUAL_UINT16(0u, ida_link_ust_sess(&DT.l));
        /* Oturumsuz üye ve TF atılır (A1-K26). */
        DT.sess = 5u;
        dt_imu(1u, 0);
        dt_tf(1u, 5 * DT_MS, 1u, true);
        TEST_ASSERT_EQUAL_UINT32(2u * (i + 1u), DT.l.say[IDA_LINK_SAY_RX_OUTSIDE_SESSION]);
        dt_ilerle(20 * DT_MS);
        if (dt_take(&in) != IDA_LINK_CAGRI_YOK) {
            TEST_ASSERT_FALSE(in.tf.present);   /* yalnız ızgara tick'i olabilir */
            TEST_ASSERT_EQUAL_UINT8(IDA_LINK_NONE, in.plat.link_state);
            dt_put(&in, IDA_LINK_CAGRI_IZGARA, IDA_RC_OK, NULL, 0u);
        }
    }
    TEST_ASSERT_EQUAL_UINT32(0u, DT.l.say[IDA_LINK_SAY_SESSIONS]);
}

static void test_reboot(void)
{
    const mavlink_ida_board_hello_t *b;
    sim_hello(0x31u);
    b = (const mavlink_ida_board_hello_t *)dt_son(MAVLINK_MSG_ID_IDA_BOARD_HELLO);
    dt_kayit_sil();
    accept_gonder(0x31u, b->nonce, 0u, (uint8_t)IDAW_SESSION_RESULT_REBOOT);
    TEST_ASSERT_EQUAL_UINT(1u, DT.yeniden_baslatma);
    TEST_ASSERT_EQUAL_UINT(0u, DT.n);   /* başka çerçeve yok */
}

static void test_session_close_ve_yeniden_kurulum(void)
{
    mavlink_ida_session_close_t c;
    const mavlink_ida_board_hello_t *b;
    ida_in_t in;
    dt_el_sikis(0x10u, 100u);
    /* Eski sess'li kapanış atılır (A1-K25). */
    memset(&c, 0, sizeof c);
    c.sess = 99u;
    c.reason = (uint8_t)IDAW_CLOSE_REASON_RUN_END;
    DT_GONDER(MAVLINK_MSG_ID_IDA_SESSION_CLOSE, c);
    TEST_ASSERT_EQUAL_INT(IDA_LINK_UST_RUNNING, ida_link_ust_durum(&DT.l));
    TEST_ASSERT_EQUAL_UINT32(1u, DT.l.say[IDA_LINK_SAY_RX_STALE_SESSION]);
    c.sess = 100u;
    DT_GONDER(MAVLINK_MSG_ID_IDA_SESSION_CLOSE, c);
    TEST_ASSERT_EQUAL_INT(IDA_LINK_UST_SONLANIYOR, ida_link_ust_durum(&DT.l));
    TEST_ASSERT_EQUAL_UINT(1u, DT.temizleme);
    oturum_sonu_cagrisi();
    TEST_ASSERT_EQUAL_INT(IDA_LINK_UST_IDLE, ida_link_ust_durum(&DT.l));
    /* Oturum sonu çağrısının izi (call_kind 4). */
    TEST_ASSERT_EQUAL_UINT8(IDAW_CALL_KIND_SESSION_END,
                            ((const mavlink_ida_call_trace_t *)dt_son(
                                 MAVLINK_MSG_ID_IDA_CALL_TRACE))->call_kind);
    /* Yeniden kurulum: prev_sess ve sessions_since_boot. */
    sim_hello(0x11u);
    b = (const mavlink_ida_board_hello_t *)dt_son(MAVLINK_MSG_ID_IDA_BOARD_HELLO);
    TEST_ASSERT_EQUAL_UINT16(100u, b->prev_sess);
    TEST_ASSERT_EQUAL_UINT16(1u, b->sessions_since_boot);
    accept_gonder(0x11u, b->nonce, 101u, (uint8_t)IDAW_SESSION_RESULT_OK);
    TEST_ASSERT_EQUAL_UINT16(101u, ida_link_ust_sess(&DT.l));
    dt_ilerle(20 * DT_MS);
    TEST_ASSERT_EQUAL_INT(IDA_LINK_CAGRI_IZGARA, dt_take(&in));
    TEST_ASSERT_EQUAL_UINT8(IDA_LINKEV_SESSION_START, in.plat.link_event);
    dt_put(&in, IDA_LINK_CAGRI_IZGARA, IDA_RC_OK, NULL, 0u);
}

/* Hızlı yeniden bağlantı (A1-K107, 7.1): step sürerken SESSION_CLOSE ve yeni SIM_HELLO
   gelir. W3-K10 gereği platform bu baytları çağrı dönünce (put_out'tan sonra) verir; oturum
   sonu çağrısı dönmeden SIM_HELLO yanıtlanmaz, yeni oturumun girdisi çekirdeğe gitmez. */
static void test_hizli_yeniden_baglanti(void)
{
    ida_in_t in;
    ida_in_t in2;
    mavlink_ida_session_close_t c;
    dt_el_sikis(0x20u, 200u);
    dt_imu(1u, 5 * DT_MS);
    dt_tf(1u, 5 * DT_MS, 1u, true);
    TEST_ASSERT_EQUAL_INT(IDA_LINK_CAGRI_STEP, dt_take(&in));   /* step sürüyor */
    TEST_ASSERT_EQUAL_INT(IDA_LINK_CAGRI_YOK, dt_take(&in2));   /* pencere açık */
    dt_put(&in, IDA_LINK_CAGRI_STEP, IDA_RC_OK, NULL, 0u);
    memset(&c, 0, sizeof c);
    c.sess = 200u;
    c.reason = (uint8_t)IDAW_CLOSE_REASON_RUN_END;
    DT_GONDER(MAVLINK_MSG_ID_IDA_SESSION_CLOSE, c);
    dt_kayit_sil();
    sim_hello(0x21u);
    TEST_ASSERT_EQUAL_UINT(0u, dt_say(MAVLINK_MSG_ID_IDA_BOARD_HELLO));   /* yanıt yok */
    DT.sess = 200u;
    dt_imu(2u, 10 * DT_MS);                                  /* oturum dışı: atılır */
    TEST_ASSERT_EQUAL_UINT32(1u, DT.l.say[IDA_LINK_SAY_RX_OUTSIDE_SESSION]);
    oturum_sonu_cagrisi();
    TEST_ASSERT_EQUAL_UINT(1u, dt_say(MAVLINK_MSG_ID_IDA_BOARD_HELLO));
    TEST_ASSERT_EQUAL_UINT32(0x21u, ((const mavlink_ida_board_hello_t *)dt_son(
                                         MAVLINK_MSG_ID_IDA_BOARD_HELLO))->peer_nonce);
    TEST_ASSERT_EQUAL_INT(IDA_LINK_UST_HANDSHAKE, ida_link_ust_durum(&DT.l));
}

/* RUNNING'de yeni nonce: oturum biter, oturum sonu çağrısından sonra yanıt (A1-K13). */
static void test_yeni_nonce_oturumu_bitirir(void)
{
    dt_el_sikis(0x40u, 300u);
    dt_kayit_sil();
    sim_hello(0x41u);
    TEST_ASSERT_EQUAL_INT(IDA_LINK_UST_SONLANIYOR, ida_link_ust_durum(&DT.l));
    TEST_ASSERT_EQUAL_UINT(0u, dt_say(MAVLINK_MSG_ID_IDA_BOARD_HELLO));
    TEST_ASSERT_EQUAL_UINT(0u, dt_say(MAVLINK_MSG_ID_IDA_SESSION_CLOSE));
    oturum_sonu_cagrisi();
    TEST_ASSERT_EQUAL_UINT(1u, dt_say(MAVLINK_MSG_ID_IDA_BOARD_HELLO));
    TEST_ASSERT_EQUAL_INT(IDA_LINK_UST_HANDSHAKE, ida_link_ust_durum(&DT.l));
}

static void test_canlilik_ve_t_bag(void)
{
    dt_el_sikis(0x50u, 400u);
    dt_kayit_sil();
    dt_ilerle(99 * DT_MS);
    ida_link_ust_poll_tx(&DT.l);
    TEST_ASSERT_EQUAL_UINT(0u, dt_say(MAVLINK_MSG_ID_IDA_KEEPALIVE));
    dt_ilerle(1 * DT_MS);
    ida_link_ust_poll_tx(&DT.l);
    TEST_ASSERT_EQUAL_UINT(1u, dt_say(MAVLINK_MSG_ID_IDA_KEEPALIVE));
    /* Sim'den 500 ms boyunca çerçeve yok: oturum biter. */
    dt_ilerle(400 * DT_MS);
    ida_link_ust_poll_tx(&DT.l);
    TEST_ASSERT_EQUAL_INT(IDA_LINK_UST_RUNNING, ida_link_ust_durum(&DT.l));
    dt_ilerle(1 * DT_MS);
    ida_link_ust_poll_tx(&DT.l);
    TEST_ASSERT_EQUAL_INT(IDA_LINK_UST_SONLANIYOR, ida_link_ust_durum(&DT.l));
    oturum_sonu_cagrisi();
    TEST_ASSERT_EQUAL_INT(IDA_LINK_UST_IDLE, ida_link_ust_durum(&DT.l));
}

static void test_guvenli_bekleme(void)
{
    const mavlink_ida_session_close_t *c;
    dt_el_sikis(0x60u, 500u);
    ida_link_ust_guvenli_bekleme(&DT.l);
    c = (const mavlink_ida_session_close_t *)dt_son(MAVLINK_MSG_ID_IDA_SESSION_CLOSE);
    TEST_ASSERT_EQUAL_UINT8(IDAW_CLOSE_REASON_SAFE_STATE, c->reason);
    TEST_ASSERT_EQUAL_UINT16(500u, c->sess);
    oturum_sonu_cagrisi();
    dt_kayit_sil();
    sim_hello(0x61u);
    TEST_ASSERT_EQUAL_UINT(0u, dt_say(MAVLINK_MSG_ID_IDA_BOARD_HELLO));   /* yanıt yok */
    dt_ilerle(300 * DT_MS);
    ida_link_ust_poll_tx(&DT.l);
    TEST_ASSERT_EQUAL_UINT8(IDA_RC_NOT_INIT, ((const mavlink_ida_board_hello_t *)dt_son(
                                                 MAVLINK_MSG_ID_IDA_BOARD_HELLO))->init_rc);
}

static void test_sil_akis_kapandi(void)
{
    ida_in_t in;
    dt_kur(IDA_LINK_UST_ROL_SIL);
    ida_link_ust_poll_tx(&DT.l);
    TEST_ASSERT_EQUAL_UINT(0u, dt_say(MAVLINK_MSG_ID_IDA_BOARD_HELLO));   /* SIL: duyuru yok */
    dt_el_sikis(0x70u, 600u);
    TEST_ASSERT_EQUAL_UINT8(IDAW_ROLE_SIL, ((const mavlink_ida_board_hello_t *)dt_son(
                                               MAVLINK_MSG_ID_IDA_BOARD_HELLO))->role);
    dt_imu(1u, 5 * DT_MS);
    dt_tf(1u, 5 * DT_MS, 1u, true);
    TEST_ASSERT_EQUAL_INT(IDA_LINK_CAGRI_STEP, dt_take(&in));
    TEST_ASSERT_EQUAL_INT64(5 * DT_MS, in.now_w.ns);   /* A4-K11 */
    dt_put(&in, IDA_LINK_CAGRI_STEP, IDA_RC_OK, NULL, 0u);
    /* Yarım bir çerçeve verilir, sonra akış kapanır: yarım aday sayaçlara yansımaz. */
    {
        const uint8_t yarim[4] = {0xFDu, 0x03u, 0x00u, 0x00u};
        ida_wtime_t t;
        uint32_t cnt[IDA_LINK_SAY_N];
        t.ns = DT.saat;
        ida_link_ust_rx(&DT.l, yarim, sizeof yarim, t);
        ida_link_ust_akis_kapandi(&DT.l);
        ida_link_ust_sayaclar(&DT.l, cnt);
        TEST_ASSERT_EQUAL_UINT32(0u, cnt[IDA_LINK_SAY_RX_CRC_ERR]);
        TEST_ASSERT_EQUAL_UINT32(0u, cnt[IDA_LINK_SAY_RX_RESYNC_BYTES]);
    }
    TEST_ASSERT_EQUAL_INT(IDA_LINK_CAGRI_OTURUM_SONU, dt_take(&in));
    TEST_ASSERT_EQUAL_INT64(5 * DT_MS, in.now_w.ns);   /* son kabul edilen TF */
    dt_put(&in, IDA_LINK_CAGRI_OTURUM_SONU, IDA_RC_OK, NULL, 0u);
    /* SIL'de ızgara tick'i yok. */
    dt_ilerle(100 * DT_MS);
    TEST_ASSERT_EQUAL_INT(IDA_LINK_CAGRI_YOK, dt_take(&in));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_kurulum_denetimi);
    RUN_TEST(test_duyuru_ve_alanlar);
    RUN_TEST(test_temiz_el_sikisma);
    RUN_TEST(test_ayni_nonce_yinelemesi);
    RUN_TEST(test_yanlis_surum_ve_dialect);
    RUN_TEST(test_reboot);
    RUN_TEST(test_session_close_ve_yeniden_kurulum);
    RUN_TEST(test_hizli_yeniden_baglanti);
    RUN_TEST(test_yeni_nonce_oturumu_bitirir);
    RUN_TEST(test_canlilik_ve_t_bag);
    RUN_TEST(test_guvenli_bekleme);
    RUN_TEST(test_sil_akis_kapandi);
    return UNITY_END();
}
