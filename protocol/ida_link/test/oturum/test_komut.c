/*
 * test_komut.c — komut sırası (W3-oturum kabul md. 2; A1-K61–K67, K122; A4-K46, K105; D-17).
 *
 * Genişletici -> daraltıcı sırasıyla gelen komutlarda genişletici çekirdeğe hiç verilmez ve
 * REJECTED / NARROWED alır; atılan kimliğin yinelemesi yeniden işlenmez; daraltıcı her
 * koşulda çekirdeğe ulaşır.
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

static void sonuc_bekle(uint32_t cmd_id, uint8_t ack, uint16_t reason)
{
    const mavlink_ida_cmd_result_t *r = dt_sonuc(cmd_id);
    TEST_ASSERT_NOT_NULL_MESSAGE(r, "sonuç yok");
    TEST_ASSERT_EQUAL_UINT8(ack, r->ack);
    TEST_ASSERT_EQUAL_UINT16(reason, r->reason);
    TEST_ASSERT_EQUAL_UINT16(77u, r->sess);
}

static bool cmds_icinde(const ida_in_t *in, uint32_t cmd_id)
{
    for (unsigned i = 0; i < in->n_cmds; ++i) {
        if (in->cmds[i].cmd_id == cmd_id) {
            return true;
        }
    }
    return false;
}

/* D-17 sırası (A1 7.1 K59–K64): TF 0 ms; ARM 1 ms'de açık partide; DISARM 3 ms'de mandalda;
   ızgara tick'i mandalı uygular; TF 7 ms'de gelir: ARM hiçbir çağrıya girmez. */
static void test_d17_acik_parti(void)
{
    ida_in_t in;
    dt_imu(1u, 5 * DT_MS);
    dt_tf(1u, 5 * DT_MS, 1u, true);
    TEST_ASSERT_EQUAL_INT(IDA_LINK_CAGRI_STEP, dt_take(&in));
    dt_put(&in, IDA_LINK_CAGRI_STEP, IDA_RC_OK, NULL, 0u);
    dt_ilerle(1 * DT_MS);
    dt_cmd_simple(10u, (uint8_t)IDA_CMD_ARM, 0u);                  /* genişletici */
    dt_cmd_simple(11u, (uint8_t)IDA_CMD_HEARTBEAT, 0u);            /* nötr */
    dt_ilerle(2 * DT_MS);
    dt_cmd_simple(12u, (uint8_t)IDA_CMD_DISARM, 0u);               /* daraltıcı */
    sonuc_bekle(10u, (uint8_t)IDA_ACK_REJECTED, (uint16_t)IDA_REASON_NARROWED);
    TEST_ASSERT_NULL(dt_sonuc(11u));                               /* nötr etkilenmez */
    dt_ilerle(3 * DT_MS);                                          /* 6 ms: ızgara tick'i */
    TEST_ASSERT_EQUAL_INT(IDA_LINK_CAGRI_IZGARA, dt_take(&in));
    TEST_ASSERT_EQUAL_UINT8(IDA_NARROW_DISARM, in.narrow.mask);
    TEST_ASSERT_EQUAL_UINT32(12u, in.narrow.cmd_id[0]);
    TEST_ASSERT_EQUAL_UINT8(0u, in.n_cmds);
    dt_put(&in, IDA_LINK_CAGRI_IZGARA, IDA_RC_OK, NULL, 0u);
    dt_ilerle(1 * DT_MS);
    dt_imu(2u, 10 * DT_MS);
    dt_tf(2u, 10 * DT_MS, 4u, true);
    TEST_ASSERT_EQUAL_INT(IDA_LINK_CAGRI_STEP, dt_take(&in));
    TEST_ASSERT_EQUAL_UINT32(0u, in.plat.cnt_batch_incomplete);
    TEST_ASSERT_FALSE(cmds_icinde(&in, 10u));                      /* ARM çekirdeğe gitmez */
    TEST_ASSERT_TRUE(cmds_icinde(&in, 11u));
    TEST_ASSERT_EQUAL_UINT8(0u, in.narrow.mask);                   /* mandal temizlendi */
    dt_put(&in, IDA_LINK_CAGRI_STEP, IDA_RC_OK, NULL, 0u);
}

/* Aynı sıra bekleyen (kapanmış, çağrılmamış) partide. */
static void test_d17_bekleyen_parti(void)
{
    ida_in_t in;
    dt_cmd_fc(20u, (uint8_t)IDA_FC_ARM, 0u);
    dt_cmd_simple(21u, (uint8_t)IDA_CMD_SET_MODE, (uint16_t)IDA_MODE_GUIDED);
    dt_cmd_simple(22u, (uint8_t)IDA_CMD_PARAM_GET, 5u);
    dt_imu(1u, 5 * DT_MS);
    dt_tf(1u, 5 * DT_MS, 4u, true);                                /* parti kapandı */
    dt_cmd_fc(23u, (uint8_t)IDA_FC_ABORT, (uint8_t)IDA_TUBE_ALL);  /* daraltıcı */
    sonuc_bekle(20u, (uint8_t)IDA_ACK_REJECTED, (uint16_t)IDA_REASON_NARROWED);
    sonuc_bekle(21u, (uint8_t)IDA_ACK_REJECTED, (uint16_t)IDA_REASON_NARROWED);
    TEST_ASSERT_NULL(dt_sonuc(22u));
    TEST_ASSERT_EQUAL_INT(IDA_LINK_CAGRI_STEP, dt_take(&in));
    TEST_ASSERT_EQUAL_UINT8(1u, in.n_cmds);
    TEST_ASSERT_EQUAL_UINT32(22u, in.cmds[0].cmd_id);
    TEST_ASSERT_EQUAL_UINT8(IDA_NARROW_FC_ABORT, in.narrow.mask);
    TEST_ASSERT_EQUAL_UINT32(23u, in.narrow.cmd_id[3]);
    dt_put(&in, IDA_LINK_CAGRI_STEP, IDA_RC_OK, NULL, 0u);
}

/* Aynı sıra erteleme tamponunda; mandaldan sonra alınan genişletici atılmaz. */
static void test_d17_erteleme_ve_sonra_gelen(void)
{
    ida_in_t in;
    /* 10 nötr + 1 genişletici: 8'i çağrıya, 3'ü ertelenir. */
    for (uint32_t i = 0; i < 10u; ++i) {
        dt_cmd_simple(100u + i, (uint8_t)IDA_CMD_HEARTBEAT, 0u);
    }
    dt_cmd_simple(110u, (uint8_t)IDA_CMD_ARM, 0u);
    dt_tf(1u, 5 * DT_MS, 11u, false);
    TEST_ASSERT_EQUAL_INT(IDA_LINK_CAGRI_TICK_TF, dt_take(&in));
    TEST_ASSERT_EQUAL_UINT8(8u, in.n_cmds);
    TEST_ASSERT_EQUAL_UINT32(3u, in.plat.cnt_cmd_deferred);
    dt_put(&in, IDA_LINK_CAGRI_TICK_TF, IDA_RC_OK, NULL, 0u);
    dt_cmd_simple(111u, (uint8_t)IDA_CMD_DISARM, 0u);
    sonuc_bekle(110u, (uint8_t)IDA_ACK_REJECTED, (uint16_t)IDA_REASON_NARROWED);
    dt_cmd_simple(112u, (uint8_t)IDA_CMD_ARM, 0u);                 /* mandaldan sonra */
    TEST_ASSERT_NULL(dt_sonuc(112u));
    dt_tf(2u, 10 * DT_MS, 2u, false);
    TEST_ASSERT_EQUAL_INT(IDA_LINK_CAGRI_TICK_TF, dt_take(&in));
    TEST_ASSERT_TRUE(cmds_icinde(&in, 108u));
    TEST_ASSERT_TRUE(cmds_icinde(&in, 109u));
    TEST_ASSERT_TRUE(cmds_icinde(&in, 112u));                      /* çekirdek K45 ile reddeder */
    TEST_ASSERT_FALSE(cmds_icinde(&in, 110u));
    TEST_ASSERT_EQUAL_UINT8(IDA_NARROW_DISARM, in.narrow.mask);
    dt_put(&in, IDA_LINK_CAGRI_TICK_TF, IDA_RC_OK, NULL, 0u);
}

/* Atılan kimliğin yinelemesi yeniden işlenmez (A1-K63) ve last_cmd_id'ye girer (A1-K66). */
static void test_nihai_kimlik_yinelemesi(void)
{
    ida_in_t in;
    dt_cmd_simple(500u, (uint8_t)IDA_CMD_ARM, 0u);
    dt_cmd_simple(501u, (uint8_t)IDA_CMD_DISARM, 0u);
    dt_tf(1u, 5 * DT_MS, 2u, false);
    TEST_ASSERT_EQUAL_INT(IDA_LINK_CAGRI_TICK_TF, dt_take(&in));
    dt_put(&in, IDA_LINK_CAGRI_TICK_TF, IDA_RC_OK, NULL, 0u);
    dt_kayit_sil();
    dt_cmd_simple(500u, (uint8_t)IDA_CMD_ARM, 0u);                 /* mandal temizlendikten sonra */
    sonuc_bekle(500u, (uint8_t)IDA_ACK_REJECTED, (uint16_t)IDA_REASON_NARROWED);
    dt_tf(2u, 10 * DT_MS, 1u, false);
    TEST_ASSERT_EQUAL_INT(IDA_LINK_CAGRI_TICK_TF, dt_take(&in));
    TEST_ASSERT_EQUAL_UINT8(0u, in.n_cmds);
    dt_put(&in, IDA_LINK_CAGRI_TICK_TF, IDA_RC_OK, NULL, 0u);
    /* Oturum değişiminden sonra da nihai; last_cmd_id = 501 (500 ve 501'in en yenisi). */
    {
        mavlink_ida_session_close_t c;
        const mavlink_ida_board_hello_t *b;
        memset(&c, 0, sizeof c);
        c.sess = 77u;
        DT_GONDER(MAVLINK_MSG_ID_IDA_SESSION_CLOSE, c);
        TEST_ASSERT_EQUAL_INT(IDA_LINK_CAGRI_OTURUM_SONU, dt_take(&in));
        dt_put(&in, IDA_LINK_CAGRI_OTURUM_SONU, IDA_RC_OK, NULL, 0u);
        dt_el_sikis(0x9Au, 78u);
        b = (const mavlink_ida_board_hello_t *)dt_son(MAVLINK_MSG_ID_IDA_BOARD_HELLO);
        TEST_ASSERT_EQUAL_UINT32(501u, b->last_cmd_id);
        dt_kayit_sil();
        dt_cmd_simple(500u, (uint8_t)IDA_CMD_ARM, 0u);
        TEST_ASSERT_EQUAL_UINT8(IDA_REASON_NARROWED, dt_sonuc(500u)->reason);
    }
}

/* Daraltıcı her koşulda çekirdeğe ulaşır: kuyruk dolu, nihai kimlik, yağmur. */
static void test_daraltici_her_kosulda(void)
{
    ida_in_t in;
    for (uint32_t i = 0; i < 30u; ++i) {
        dt_cmd_simple(200u + i, (uint8_t)IDA_CMD_HEARTBEAT, 0u);   /* 24'ten sonrası atılır */
    }
    sonuc_bekle(229u, (uint8_t)IDA_ACK_REJECTED, (uint16_t)IDAW_REASON_LINK_DROPPED);
    dt_cmd_simple(300u, (uint8_t)IDA_CMD_SET_MODE, (uint16_t)IDA_MODE_MANUAL);
    dt_cmd_simple(301u, (uint8_t)IDA_CMD_SET_MODE, (uint16_t)IDA_MODE_DEPTH_HOLD);
    sonuc_bekle(300u, (uint8_t)IDA_ACK_ACCEPTED, (uint16_t)IDA_REASON_COALESCED);
    dt_tf(1u, 5 * DT_MS, 32u, false);
    TEST_ASSERT_EQUAL_INT(IDA_LINK_CAGRI_TICK_TF, dt_take(&in));
    TEST_ASSERT_EQUAL_UINT8(IDA_NARROW_MODE, in.narrow.mask);
    TEST_ASSERT_EQUAL_UINT8(IDA_MODE_DEPTH_HOLD, in.narrow.mode);
    TEST_ASSERT_EQUAL_UINT32(301u, in.narrow.cmd_id[1]);
    TEST_ASSERT_EQUAL_UINT8(1u, in.narrow.coalesced[1]);
    TEST_ASSERT_EQUAL_UINT32(1u, in.plat.cnt_narrow_coalesced);
    TEST_ASSERT_EQUAL_UINT32(6u, in.plat.cnt_cmd_dropped);
    dt_put(&in, IDA_LINK_CAGRI_TICK_TF, IDA_RC_OK, NULL, 0u);
}

/* Mimar kararı W3-K11: nihai (atılmış) kimlik listesindeki bir kimlik biçimce geçerli bir
   daraltıcı olarak yeniden gelirse daraltıcı yine mandala alınır ve kendi olağan sonucunu
   alır (A4-K46, D-20: daraltıcı yineleme denetimlerinden muaftır). Öteki biçimler için
   test_sol.c test_b6_nihai_kimlik_butun_yollar. */
static void test_nihai_kimlikli_daraltici_mandala(void)
{
    ida_in_t in;
    dt_cmd_simple(600u, (uint8_t)IDA_CMD_ARM, 0u);
    dt_cmd_simple(601u, (uint8_t)IDA_CMD_DISARM, 0u);   /* 600 nihai NARROWED */
    dt_cmd_fc(600u, (uint8_t)IDA_FC_DISARM, (uint8_t)IDA_TUBE_ALL);
    dt_tf(1u, 5 * DT_MS, 3u, false);
    TEST_ASSERT_EQUAL_INT(IDA_LINK_CAGRI_TICK_TF, dt_take(&in));
    TEST_ASSERT_EQUAL_UINT8(IDA_NARROW_DISARM | IDA_NARROW_FC_DISARM, in.narrow.mask);
    dt_put(&in, IDA_LINK_CAGRI_TICK_TF, IDA_RC_OK, NULL, 0u);
}

/* Dönüşüm hataları (A1-K64, K122; 7.1 K59–K64 satırı). */
static void test_donusum_hatalari(void)
{
    ida_in_t in;
    uint32_t id = 700u;
    const uint16_t modlar[3] = {0u, 1u, 8u};
    for (unsigned i = 0; i < 3u; ++i, ++id) {
        dt_cmd_simple(id, (uint8_t)IDA_CMD_SET_MODE, modlar[i]);
        sonuc_bekle(id, (uint8_t)IDA_ACK_REJECTED, (uint16_t)IDAW_REASON_LINK_BAD_FIELD);
    }
    dt_cmd_simple(id, 6u, 0u);   /* GUIDED_SP CMD_SIMPLE'da yok */
    sonuc_bekle(id++, (uint8_t)IDA_ACK_REJECTED, (uint16_t)IDAW_REASON_LINK_BAD_FIELD);
    dt_cmd_fc(id, (uint8_t)IDA_FC_ARM, (uint8_t)IDA_TUBE_ALL);
    sonuc_bekle(id++, (uint8_t)IDA_ACK_REJECTED, (uint16_t)IDAW_REASON_LINK_BAD_FIELD);
    /* W5-K7/G-K14: FC_DISARM'ın biçim hatası artık reddetmez; test_gk14_bozuk_daraltici'de. */
    dt_cmd_fc(id, 4u, 0u);       /* eski FC_SELECT */
    sonuc_bekle(id++, (uint8_t)IDA_ACK_REJECTED, (uint16_t)IDAW_REASON_LINK_BAD_FIELD);
    {
        mavlink_ida_cmd_fc_t m;
        memset(&m, 0, sizeof m);
        m.cmd_id = id;
        m.sess = 77u;
        m.op = (uint8_t)IDA_FC_ARM;
        m.arg1 = 1u;
        DT_GONDER(MAVLINK_MSG_ID_IDA_CMD_FC, m);
        sonuc_bekle(id++, (uint8_t)IDA_ACK_REJECTED, (uint16_t)IDAW_REASON_LINK_BAD_FIELD);
    }
    {
        mavlink_ida_cmd_param_set_t m;
        const mavlink_ida_cmd_result_t *r;
        memset(&m, 0, sizeof m);
        m.cmd_id = id;
        m.sess = 77u;
        m.param_id = 33u;
        m.param_type = 4u;
        DT_GONDER(MAVLINK_MSG_ID_IDA_CMD_PARAM_SET, m);
        r = dt_sonuc(id++);
        TEST_ASSERT_NOT_NULL(r);
        TEST_ASSERT_EQUAL_UINT8(IDA_RES_PARAM_VALUE, r->kind);
        TEST_ASSERT_EQUAL_UINT16(33u, r->param_id);
        TEST_ASSERT_EQUAL_UINT8(0u, r->param_type);
        TEST_ASSERT_EQUAL_UINT32(0u, r->value_bits);
    }
    {
        /* Görev öğesi: tür 8 (eski TARGET_ALIGN), sıfır olmayan tanımsız p[2], yanlış frame,
           aralık dışı depth. */
        mavlink_ida_cmd_mission_item_t m;
        memset(&m, 0, sizeof m);
        m.sess = 77u;
        m.item_type = 1u;
        m.frame = (uint8_t)IDA_MI_FRAME_LOCAL;
        m.p[0] = 1.0f;
        m.p[1] = 1.0f;
        m.depth = 3.0f;
        m.cmd_id = id;
        DT_GONDER(MAVLINK_MSG_ID_IDA_CMD_MISSION_ITEM, m);
        TEST_ASSERT_NULL(dt_sonuc(id++));                          /* geçerli */
        m.item_type = 8u;
        m.cmd_id = id;
        DT_GONDER(MAVLINK_MSG_ID_IDA_CMD_MISSION_ITEM, m);
        sonuc_bekle(id++, (uint8_t)IDA_ACK_REJECTED, (uint16_t)IDAW_REASON_LINK_BAD_FIELD);
        m.item_type = 1u;
        m.p[2] = 0.5f;
        m.cmd_id = id;
        DT_GONDER(MAVLINK_MSG_ID_IDA_CMD_MISSION_ITEM, m);
        sonuc_bekle(id++, (uint8_t)IDA_ACK_REJECTED, (uint16_t)IDAW_REASON_LINK_BAD_FIELD);
        m.p[2] = 0.0f;
        m.frame = (uint8_t)IDA_MI_FRAME_NONE;
        m.cmd_id = id;
        DT_GONDER(MAVLINK_MSG_ID_IDA_CMD_MISSION_ITEM, m);
        sonuc_bekle(id++, (uint8_t)IDA_ACK_REJECTED, (uint16_t)IDAW_REASON_LINK_BAD_FIELD);
        m.frame = (uint8_t)IDA_MI_FRAME_LOCAL;
        m.depth = 101.0f;
        m.cmd_id = id;
        DT_GONDER(MAVLINK_MSG_ID_IDA_CMD_MISSION_ITEM, m);
        sonuc_bekle(id++, (uint8_t)IDA_ACK_REJECTED, (uint16_t)IDAW_REASON_LINK_BAD_FIELD);
    }
    dt_tf(1u, 5 * DT_MS, (uint8_t)(id - 700u), false);
    TEST_ASSERT_EQUAL_INT(IDA_LINK_CAGRI_TICK_TF, dt_take(&in));
    TEST_ASSERT_EQUAL_UINT8(1u, in.n_cmds);                        /* yalnız geçerli öğe */
    TEST_ASSERT_EQUAL_UINT8(0u, in.narrow.mask);                   /* bozuk daraltıcı mandala girmez */
    TEST_ASSERT_EQUAL_UINT32(0u, in.plat.cnt_batch_incomplete);
    dt_put(&in, IDA_LINK_CAGRI_TICK_TF, IDA_RC_OK, NULL, 0u);
    TEST_ASSERT_EQUAL_UINT32(id - 701u, DT.l.say[IDA_LINK_SAY_CMD_BAD_FIELD]);
}

/* W5-K7/G-K14: FC_DISARM/FC_ABORT biçim hatası daraltıcıyı reddetmez; mandala alınır, bütün
   tüplere uygulanır ve sonuç ACCEPTED olur; biçim hatası CMD_BAD_FIELD sayacında görünür. */
static void test_gk14_bozuk_daraltici(void)
{
    ida_in_t in;
    dt_kayit_sil();
    dt_cmd_fc(820u, (uint8_t)IDA_FC_DISARM, 0u);     /* tube = ALL değil */
    TEST_ASSERT_NULL_MESSAGE(dt_sonuc(820u), "biçim hatası daraltıcıyı reddetmez");
    TEST_ASSERT_EQUAL_UINT32(1u, DT.l.say[IDA_LINK_SAY_CMD_BAD_FIELD]);
    dt_imu(900u, 5 * DT_MS);
    dt_tf(900u, 5 * DT_MS, 1u, true);
    TEST_ASSERT_EQUAL_INT(IDA_LINK_CAGRI_STEP, dt_take(&in));
    TEST_ASSERT_EQUAL_UINT8(IDA_NARROW_FC_DISARM, in.narrow.mask);
    TEST_ASSERT_EQUAL_UINT32(820u, in.narrow.cmd_id[2]);
    dt_put(&in, IDA_LINK_CAGRI_STEP, IDA_RC_OK, NULL, 0u);
    sonuc_bekle(820u, (uint8_t)IDA_ACK_ACCEPTED, (uint16_t)IDA_REASON_NONE);
}

static void test_eski_oturum_komutu(void)
{
    ida_in_t in;
    DT.sess = 76u;
    dt_cmd_simple(800u, (uint8_t)IDA_CMD_DISARM, 0u);
    DT.sess = 77u;
    TEST_ASSERT_NULL(dt_sonuc(800u));
    TEST_ASSERT_EQUAL_UINT32(1u, DT.l.say[IDA_LINK_SAY_RX_STALE_SESSION]);
    dt_tf(1u, 5 * DT_MS, 0u, false);
    TEST_ASSERT_EQUAL_INT(IDA_LINK_CAGRI_TICK_TF, dt_take(&in));
    TEST_ASSERT_EQUAL_UINT8(0u, in.narrow.mask);
    dt_put(&in, IDA_LINK_CAGRI_TICK_TF, IDA_RC_OK, NULL, 0u);
}

/* DUP penceresinin komutları atılır (A1-K31). */
static void test_dup_penceresi_komutlari(void)
{
    ida_in_t in;
    dt_tf(1u, 5 * DT_MS, 0u, false);
    TEST_ASSERT_EQUAL_INT(IDA_LINK_CAGRI_TICK_TF, dt_take(&in));
    dt_put(&in, IDA_LINK_CAGRI_TICK_TF, IDA_RC_OK, NULL, 0u);
    dt_cmd_simple(900u, (uint8_t)IDA_CMD_ARM, 0u);
    dt_tf(1u, 5 * DT_MS, 1u, false);                               /* DUP */
    dt_tf(2u, 10 * DT_MS, 0u, false);
    TEST_ASSERT_EQUAL_INT(IDA_LINK_CAGRI_TICK_TF, dt_take(&in));
    TEST_ASSERT_EQUAL_UINT8(0u, in.n_cmds);
    dt_put(&in, IDA_LINK_CAGRI_TICK_TF, IDA_RC_OK, NULL, 0u);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_d17_acik_parti);
    RUN_TEST(test_d17_bekleyen_parti);
    RUN_TEST(test_d17_erteleme_ve_sonra_gelen);
    RUN_TEST(test_nihai_kimlik_yinelemesi);
    RUN_TEST(test_daraltici_her_kosulda);
    RUN_TEST(test_nihai_kimlikli_daraltici_mandala);
    RUN_TEST(test_donusum_hatalari);
    RUN_TEST(test_gk14_bozuk_daraltici);
    RUN_TEST(test_eski_oturum_komutu);
    RUN_TEST(test_dup_penceresi_komutlari);
    return UNITY_END();
}
