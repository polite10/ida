/*
 * test_sol.c — sol incelemesinin (docs/gorevler/inceleme/W3-oturum-sol.md) sekiz bulgusunun
 * karşı örnekleri. Her test bulgunun numarasını taşır; düzeltmeden önce kalır, sonra geçer.
 */
#include "destek_oturum.h"

#include "unity.h"

#include <string.h>

void setUp(void);
void tearDown(void);
void setUp(void)
{
    dt_kur(IDA_LINK_UST_ROL_KART);
}
void tearDown(void) {}

#define CEKIRDEK_BELLEK 8192u
typedef union {
    uint64_t hiza;
    long double hiza2;
    uint8_t b[CEKIRDEK_BELLEK];
} cekirdek_bellek_t;
static cekirdek_bellek_t cekirdek_bellek;

static ida_core_t *cekirdek_kur(void)
{
    ida_core_info_t info;
    ida_init_in_t ii;
    ida_core_t *c = (ida_core_t *)(void *)cekirdek_bellek.b;
    ida_core_info(&info);
    TEST_ASSERT_TRUE(info.core_size <= CEKIRDEK_BELLEK);
    TEST_ASSERT_TRUE(info.core_align <= (uint32_t)_Alignof(cekirdek_bellek_t));
    memset(&ii, 0, sizeof ii);
    ii.port_major = (uint16_t)IDA_PORT_VERSION_MAJOR;
    ii.port_minor = (uint16_t)IDA_PORT_VERSION_MINOR;
    ii.now_w.ns = DT.saat;
    ii.boot_count = DT.ayar.boot_count;
    ii.boot_count_ok = true;
    TEST_ASSERT_EQUAL_INT(IDA_RC_PARAM_DEFAULTS, ida_core_init(c, &ii));
    return c;
}

static void oturum_sonu_cagrisi(void)
{
    ida_in_t in;
    TEST_ASSERT_EQUAL_INT(IDA_LINK_CAGRI_OTURUM_SONU, dt_take(&in));
    dt_put(&in, IDA_LINK_CAGRI_OTURUM_SONU, IDA_RC_OK, NULL, 0u);
}

/* Bulgu 1 (W3-K10): ARM -> TF -> take -> rx(DISARM) -> step. take ile put_out arasında rx
   platform yükümlülüğü ihlalidir: baytlar işlenir, oturum PROTOCOL ile biter; sonraki çağrı
   DISARM mandalını taşıyan oturum sonu çağrısıdır ve çekirdek silahsızdır. */
static void test_b1_take_ile_cagri_arasinda_rx(void)
{
    static ida_in_t in;
    static ida_out_t out;
    ida_core_t *c = cekirdek_kur();
    ida_rc_t rc;
    DT.ayar.init_rc = (uint8_t)IDA_RC_PARAM_DEFAULTS;
    TEST_ASSERT_EQUAL_INT(IDA_LINK_TAMAM, ida_link_ust_kur(&DT.l, &DT.ayar, &DT.plat));
    dt_el_sikis(0x31u, 9u);
    dt_cmd_simple(1u, (uint8_t)IDA_CMD_ARM, 0u);
    dt_imu(1u, 5 * DT_MS);
    dt_tf(1u, 5 * DT_MS, 2u, true);
    TEST_ASSERT_FALSE(ida_link_ust_cagri_acik(&DT.l));
    TEST_ASSERT_EQUAL_INT(IDA_LINK_CAGRI_STEP, dt_take(&in));
    TEST_ASSERT_TRUE(ida_link_ust_cagri_acik(&DT.l));
    dt_cmd_simple(2u, (uint8_t)IDA_CMD_DISARM, 0u);            /* ihlal: pencerede rx */
    TEST_ASSERT_EQUAL_INT(IDA_LINK_UST_SONLANIYOR, ida_link_ust_durum(&DT.l));
    TEST_ASSERT_EQUAL_UINT8(IDAW_CLOSE_REASON_PROTOCOL,
                            ((const mavlink_ida_session_close_t *)dt_son(
                                 MAVLINK_MSG_ID_IDA_SESSION_CLOSE))->reason);
    rc = ida_core_step(c, &in, &out);
    ida_link_ust_put_out(&DT.l, &in, rc, &out);
    TEST_ASSERT_FALSE(ida_link_ust_cagri_acik(&DT.l));
    TEST_ASSERT_EQUAL_INT(IDA_LINK_CAGRI_OTURUM_SONU, dt_take(&in));
    TEST_ASSERT_EQUAL_UINT8(IDA_NARROW_DISARM, in.narrow.mask & IDA_NARROW_DISARM);
    TEST_ASSERT_EQUAL_UINT8(IDA_LINKEV_SESSION_END, in.plat.link_event);
    rc = ida_core_tick(c, &in, &out);
    TEST_ASSERT_EQUAL_INT(IDA_RC_OK, rc);
    TEST_ASSERT_FALSE(out.armed);
    TEST_ASSERT_FALSE(out.status.armed);
    ida_link_ust_put_out(&DT.l, &in, rc, &out);
    TEST_ASSERT_EQUAL_INT(IDA_LINK_UST_IDLE, ida_link_ust_durum(&DT.l));
}

/* Bulgu 2: kuyruk [nötr, ARM] iken DISARM; ARM'ın NARROWED sonucu gönderilemez (TX_OVERFLOW).
   Temizlenen kuyruk yeniden doldurulmamalı; yeni oturumun ilk TF'si eski komutu vermez. */
static void test_b2_tasma_eski_komutu_geri_getirmez(void)
{
    ida_in_t in;
    dt_el_sikis(0x41u, 10u);
    dt_cmd_simple(1u, (uint8_t)IDA_CMD_HEARTBEAT, 0u);
    dt_cmd_simple(2u, (uint8_t)IDA_CMD_ARM, 0u);
    DT.dolu[IDA_LINK_SINIF_ZORUNLU] = true;   /* kuyruk_temizle bayrağı kaldırır */
    dt_cmd_simple(3u, (uint8_t)IDA_CMD_DISARM, 0u);
    TEST_ASSERT_EQUAL_INT(IDA_LINK_UST_SONLANIYOR, ida_link_ust_durum(&DT.l));
    TEST_ASSERT_EQUAL_UINT8(0u, DT.l.ozel_n_kom);
    oturum_sonu_cagrisi();
    dt_el_sikis(0x42u, 11u);
    dt_tf(100u, 5 * DT_MS, 0u, false);
    TEST_ASSERT_EQUAL_INT(IDA_LINK_CAGRI_TICK_TF, dt_take(&in));
    TEST_ASSERT_EQUAL_UINT8(0u, in.n_cmds);
    dt_put(&in, IDA_LINK_CAGRI_TICK_TF, IDA_RC_OK, NULL, 0u);
}

/* Bulgu 3: IMU'lu bekleyen parti + IMU'suz yeni parti -> tick (en yeni partinin IMU'su yok);
   eski IMU düşer ve sayılır. */
static void test_b3_imulu_imusuz_birlesme(void)
{
    ida_in_t in;
    dt_el_sikis(0x51u, 12u);
    dt_imu(1u, 5 * DT_MS);
    dt_tf(1u, 5 * DT_MS, 1u, true);
    dt_mag(1u, 9 * DT_MS);
    dt_tf(2u, 10 * DT_MS, 1u, false);
    TEST_ASSERT_EQUAL_INT(IDA_LINK_CAGRI_TICK_TF, dt_take(&in));
    TEST_ASSERT_FALSE(in.imu_present);
    TEST_ASSERT_EQUAL_UINT32(2u, in.tf.seq);
    TEST_ASSERT_EQUAL_UINT32(1u, in.plat.cnt_member_overflow);
    TEST_ASSERT_TRUE(in.plat.input_dropped);
    dt_put(&in, IDA_LINK_CAGRI_TICK_TF, IDA_RC_OK, NULL, 0u);
}

static ida_event_t acan_olay(uint32_t ctr, const ida_in_t *in)
{
    ida_event_t e;
    memset(&e, 0, sizeof e);
    e.event_id = ((uint64_t)3u << 32) | ctr;
    e.kind = (uint8_t)IDA_EVT_SAFETY_OPEN;
    e.seq = in->tf.seq;
    e.t_decide = in->tf.t;
    e.apply_by_ns = 20000000u;
    e.hold_ns = 200000000u;
    return e;
}

static void olay_sonucu(uint32_t ctr)
{
    mavlink_ida_event_result_t r;
    memset(&r, 0, sizeof r);
    r.event_id = ((uint64_t)3u << 32) | ctr;
    r.result = (uint8_t)IDA_EVR_APPLIED;
    r.t_apply_ns = 77;
    r.sess = DT.sess;
    DT_GONDER(MAVLINK_MSG_ID_IDA_EVENT_RESULT, r);
}

static void olay_ureten_step(uint32_t seq, uint32_t ctr)
{
    ida_in_t in;
    ida_event_t e;
    dt_imu(seq, (int64_t)seq * 5 * DT_MS);
    dt_emniyet(seq, (int64_t)seq * 5 * DT_MS, 0u, 5u);
    dt_tf(seq, (int64_t)seq * 5 * DT_MS, 2u, true);
    TEST_ASSERT_EQUAL_INT(IDA_LINK_CAGRI_STEP, dt_take(&in));
    e = acan_olay(ctr, &in);
    dt_put(&in, IDA_LINK_CAGRI_STEP, IDA_RC_OK, &e, 1u);
}

/* Bulgu 4a: açık penceredeki sonuç, TF'den önce araya giren ızgara çağrısına girmez; parti
   olağan kapanınca girer. */
static void test_b4_sonuc_acik_pencerede_beklemeli(void)
{
    ida_in_t in;
    dt_el_sikis(0x61u, 13u);
    olay_ureten_step(1u, 1u);
    olay_sonucu(1u);
    dt_ilerle(7 * DT_MS);                                      /* ızgara tick'i */
    TEST_ASSERT_EQUAL_INT(IDA_LINK_CAGRI_IZGARA, dt_take(&in));
    TEST_ASSERT_EQUAL_UINT8(0u, in.n_evres);
    dt_put(&in, IDA_LINK_CAGRI_IZGARA, IDA_RC_OK, NULL, 0u);
    dt_imu(2u, 10 * DT_MS);
    dt_tf(2u, 10 * DT_MS, 2u, true);
    TEST_ASSERT_EQUAL_INT(IDA_LINK_CAGRI_STEP, dt_take(&in));
    TEST_ASSERT_EQUAL_UINT8(1u, in.n_evres);
    TEST_ASSERT_EQUAL_UINT8(IDA_EVR_APPLIED, in.evres[0].result);
    dt_put(&in, IDA_LINK_CAGRI_STEP, IDA_RC_OK, NULL, 0u);
}

/* Bulgu 4b/4c: DUP ya da OLD penceresindeki sonuç atılır; olay yeniden beklenir (yineleme
   sürer) ve süre dolunca EXPIRED_NO_ACK alır. */
static void dup_old_sonuc(uint32_t tf_seq_farki)
{
    ida_in_t in;
    unsigned olay_say;
    dt_el_sikis(0x71u, 14u);
    olay_ureten_step(1u, 1u);
    dt_kayit_sil();
    olay_sonucu(1u);
    dt_tf(1u + tf_seq_farki, 5 * DT_MS, 1u, false);           /* DUP (0) ya da OLD */
    dt_ilerle(16 * DT_MS);
    ida_link_ust_poll_tx(&DT.l);
    olay_say = dt_say(MAVLINK_MSG_ID_IDA_EVENT);
    TEST_ASSERT_EQUAL_UINT(1u, olay_say);                      /* yineleme yeniden başladı */
    dt_imu(2u, 10 * DT_MS);
    dt_tf(2u, 10 * DT_MS, 1u, true);
    TEST_ASSERT_EQUAL_INT(IDA_LINK_CAGRI_STEP, dt_take(&in));
    TEST_ASSERT_EQUAL_UINT8(0u, in.n_evres);                   /* atılan sonuç verilmez */
    dt_put(&in, IDA_LINK_CAGRI_STEP, IDA_RC_OK, NULL, 0u);
    dt_ilerle(100 * DT_MS);
    TEST_ASSERT_NOT_EQUAL(IDA_LINK_CAGRI_YOK, dt_take(&in));
    TEST_ASSERT_EQUAL_UINT8(1u, in.n_evres);
    TEST_ASSERT_EQUAL_UINT8(IDA_EVR_EXPIRED_NO_ACK, in.evres[0].result);
    dt_put(&in, (ida_link_cagri_t)DT.l.ozel_cagri_tur, IDA_RC_OK, NULL, 0u);
}

static void test_b4_sonuc_dup_penceresinde_atilir(void)
{
    dup_old_sonuc(0u);
}

static void test_b4_sonuc_old_penceresinde_atilir(void)
{
    dup_old_sonuc(0x80000000u);
}

/* Bulgu 5: SIL'de CRC'si doğru, t_sim_ns = INT64_MAX TF ve aynı pencerede geçerli bir
   daraltıcı; çekirdek BAD_INPUT ile SAFETY_CLOSE çıkarır. Olay zamanlayıcısı işaretli
   taşmaya ulaşmamalı (UBSan derlemesinde sınanır). */
static void test_b5_bozuk_sil_zamani(void)
{
    static ida_in_t in;
    static ida_out_t out;
    mavlink_ida_time_frame_t tf;
    ida_event_t e;
    dt_kur(IDA_LINK_UST_ROL_SIL);
    dt_el_sikis(0x81u, 15u);
    dt_cmd_simple(1u, (uint8_t)IDA_CMD_DISARM, 0u);
    memset(&tf, 0, sizeof tf);
    tf.t_sim_ns = INT64_MAX;
    tf.batch_seq = 1u;
    tf.sess = 15u;
    tf.n_members = 1u;
    DT_GONDER(MAVLINK_MSG_ID_IDA_TIME_FRAME, tf);
    TEST_ASSERT_EQUAL_INT(IDA_LINK_CAGRI_TICK_TF, dt_take(&in));
    memset(&out, 0, sizeof out);
    out.src = (uint8_t)IDA_SRC_TICK;
    memset(&e, 0, sizeof e);
    e.event_id = ((uint64_t)3u << 32) | 1u;
    e.kind = (uint8_t)IDA_EVT_SAFETY_CLOSE;
    e.tube = (uint8_t)IDA_TUBE_ALL;
    e.apply_by_ns = 20000000u;
    out.events[0] = e;
    out.n_events = 1u;
    ida_link_ust_put_out(&DT.l, &in, IDA_RC_BAD_INPUT, &out);
    for (unsigned i = 0; i < 50u; ++i) {
        dt_ilerle(5 * DT_MS);
        ida_link_ust_poll_tx(&DT.l);
    }
    /* Olay süre sonunda EXPIRED_NO_ACK alır (zaman tabanı platformun saati). */
    dt_imu(2u, 10 * DT_MS);
    dt_tf(2u, 10 * DT_MS, 1u, true);
    TEST_ASSERT_EQUAL_INT(IDA_LINK_CAGRI_STEP, dt_take(&in));
    TEST_ASSERT_EQUAL_UINT8(1u, in.n_evres);
    TEST_ASSERT_EQUAL_UINT8(IDA_EVR_EXPIRED_NO_ACK, in.evres[0].result);
    dt_put(&in, IDA_LINK_CAGRI_STEP, IDA_RC_OK, NULL, 0u);
}

/* Bulgu 6 (W3-K11): nihai kimlik biçimce geçerli daraltıcı dışında hiçbir biçimde yeniden
   işlenmez; NARROWED yinelenir, LINK_BAD_FIELD üretilmez. */
static void test_b6_nihai_kimlik_butun_yollar(void)
{
    ida_in_t in;
    const mavlink_ida_cmd_result_t *r;
    dt_el_sikis(0x91u, 16u);
    dt_cmd_simple(600u, (uint8_t)IDA_CMD_ARM, 0u);
    dt_cmd_simple(601u, (uint8_t)IDA_CMD_DISARM, 0u);          /* 600 nihai NARROWED */
    dt_kayit_sil();
    dt_cmd_simple(600u, 99u, 0u);                              /* bozuk tür */
    r = dt_sonuc(600u);
    TEST_ASSERT_NOT_NULL(r);
    TEST_ASSERT_EQUAL_UINT16(IDA_REASON_NARROWED, r->reason);
    dt_kayit_sil();
    dt_cmd_fc(600u, (uint8_t)IDA_FC_DISARM, 0u);               /* bozuk daraltıcı (tube) */
    r = dt_sonuc(600u);
    TEST_ASSERT_NOT_NULL(r);
    TEST_ASSERT_EQUAL_UINT16(IDA_REASON_NARROWED, r->reason);
    dt_kayit_sil();
    dt_cmd_simple(600u, (uint8_t)IDA_CMD_SET_MODE, 0u);        /* bozuk SET_MODE */
    r = dt_sonuc(600u);
    TEST_ASSERT_NOT_NULL(r);
    TEST_ASSERT_EQUAL_UINT16(IDA_REASON_NARROWED, r->reason);
    dt_kayit_sil();
    dt_cmd_simple(600u, (uint8_t)IDA_CMD_HEARTBEAT, 0u);       /* nötr */
    TEST_ASSERT_EQUAL_UINT16(IDA_REASON_NARROWED, dt_sonuc(600u)->reason);
    TEST_ASSERT_EQUAL_UINT32(0u, DT.l.say[IDA_LINK_SAY_CMD_BAD_FIELD]);
    dt_tf(1u, 5 * DT_MS, 6u, false);
    TEST_ASSERT_EQUAL_INT(IDA_LINK_CAGRI_TICK_TF, dt_take(&in));
    TEST_ASSERT_EQUAL_UINT8(0u, in.n_cmds);
    TEST_ASSERT_EQUAL_UINT8(IDA_NARROW_DISARM, in.narrow.mask);  /* yalnız 601 */
    dt_put(&in, IDA_LINK_CAGRI_TICK_TF, IDA_RC_OK, NULL, 0u);
}

/* Bulgu 7: yarım çerçeve -> T_bağ -> oturum sonu -> yeni el sıkışma. Yarım aday silinir
   (CRC hatası yok, SIM_HELLO yanıtlanır), seq geçmişi korunur (A1-K08: bir boşluk). */
static void test_b7_oturum_sonu_yarim_adayi_siler(void)
{
    uint8_t buf[IDA_LINK_CERCEVE_AZAMI];
    size_t n = 0u;
    mavlink_ida_keepalive_t k;
    ida_wtime_t t;
    uint32_t cnt[IDA_LINK_SAY_N];
    uint32_t gap_once;
    dt_el_sikis(0xA1u, 17u);
    memset(&k, 0, sizeof k);
    k.sess = 17u;
    TEST_ASSERT_EQUAL_INT(IDA_LINK_TAMAM, ida_link_gonder(&DT.sim_g, MAVLINK_MSG_ID_IDA_KEEPALIVE,
                                                          &k, sizeof k, buf, sizeof buf, &n));
    t.ns = DT.saat;
    /* Başlığı tam, yükü eksik aday (11 B / 15 B): CRC aşamasında bayt yutar. */
    ida_link_ust_rx(&DT.l, buf, 11u, t);
    ida_link_ust_sayaclar(&DT.l, cnt);
    gap_once = cnt[IDA_LINK_SAY_RX_SEQ_GAP];
    dt_ilerle(600 * DT_MS);
    ida_link_ust_poll_tx(&DT.l);                               /* T_bağ */
    TEST_ASSERT_EQUAL_INT(IDA_LINK_UST_SONLANIYOR, ida_link_ust_durum(&DT.l));
    oturum_sonu_cagrisi();
    dt_kayit_sil();
    dt_el_sikis(0xA2u, 18u);                                   /* SIM_HELLO yanıtlanmalı */
    ida_link_ust_sayaclar(&DT.l, cnt);
    TEST_ASSERT_EQUAL_UINT32(0u, cnt[IDA_LINK_SAY_RX_CRC_ERR]);
    TEST_ASSERT_EQUAL_UINT32(gap_once + 1u, cnt[IDA_LINK_SAY_RX_SEQ_GAP]);
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

/* Bulgu 8: HEAD bilindikten sonra frag_idx ≥ n_frag olan DATA ping360_bad sayılır ve
   tamamlanmış satırı bozmaz. */
static void test_b8_head_sonrasi_parca_siniri(void)
{
    ida_in_t in;
    mavlink_ida_ping360_head_t h;
    dt_el_sikis(0xB1u, 19u);
    ping_veri(5u, 0u, 7u);
    memset(&h, 0, sizeof h);
    h.seq = 5u;
    h.angle_grad = 10u;
    h.n_samples = 200u;
    h.sample_period = 100u;
    h.n_frag = 1u;
    h.flags = (uint16_t)IDA_HF_VALID;
    DT_GONDER(MAVLINK_MSG_ID_IDA_PING360_HEAD, h);
    ping_veri(5u, 2u, 9u);
    TEST_ASSERT_EQUAL_UINT32(1u, DT.l.say[IDA_LINK_SAY_PING360_BAD]);
    dt_tf(1u, 5 * DT_MS, 3u, false);
    TEST_ASSERT_EQUAL_INT(IDA_LINK_CAGRI_TICK_TF, dt_take(&in));
    TEST_ASSERT_NOT_NULL(in.ping360);
    TEST_ASSERT_EQUAL_UINT8(7u, in.ping360->data[0]);
    TEST_ASSERT_EQUAL_UINT32(0u, DT.l.say[IDA_LINK_SAY_PING360_INCOMPLETE]);
    dt_put(&in, IDA_LINK_CAGRI_TICK_TF, IDA_RC_OK, NULL, 0u);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_b1_take_ile_cagri_arasinda_rx);
    RUN_TEST(test_b2_tasma_eski_komutu_geri_getirmez);
    RUN_TEST(test_b3_imulu_imusuz_birlesme);
    RUN_TEST(test_b4_sonuc_acik_pencerede_beklemeli);
    RUN_TEST(test_b4_sonuc_dup_penceresinde_atilir);
    RUN_TEST(test_b4_sonuc_old_penceresinde_atilir);
    RUN_TEST(test_b5_bozuk_sil_zamani);
    RUN_TEST(test_b6_nihai_kimlik_butun_yollar);
    RUN_TEST(test_b7_oturum_sonu_yarim_adayi_siler);
    RUN_TEST(test_b8_head_sonrasi_parca_siniri);
    return UNITY_END();
}
