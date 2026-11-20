/*
 * test_emniyet.c — (b) adımı: mandal bitleri, cmds[]'teki daraltıcılar, SESSION_END ve
 * emniyet örneklerinin kısıtlayıcı içeriği (A4-K44, K45, K46, K73, K111, K113, K116, K125,
 * K127, K128, K130). Kapanma yolu gevşetilmiş etkin değerle başlar; gevşetme W4'ün
 * dosyasında olduğu için durum test kaynağından kurulur (M9).
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

/* Bir sonraki parti; girdinin geri kalanı korunur. */
static ida_rc_t step(void)
{
    g_seq++;
    g_t += 5 * DS_MS;
    IN.now_w.ns = g_t;
    IN.tf.present = true;
    IN.tf.seq = g_seq;
    IN.tf.t.ns = g_t;
    IN.tf.t_rx.ns = g_t;
    IN.imu_present = true;
    ds_imu(&IN.imu, g_t, g_seq);
    const ida_rc_t rc = ida_core_step(&C, &IN, &OUT);
    ds_cikti_degismezleri(rc, &IN, &OUT);
    return rc;
}

static void gevset(void)
{
    C.emn.kisitli = 0u; /* test kancası: W4'ün gevşetmesinin yerine */
}

static void emniyet_ornegi(uint32_t i, uint32_t seq)
{
    ds_emniyet(&IN.samples[i], g_t + (5 * DS_MS), seq);
    if (IN.n_samples <= i) {
        IN.n_samples = (uint8_t)(i + 1u);
    }
}

static void arm_komutu(uint32_t i, uint32_t id)
{
    IN.cmds[i].cmd_id = id;
    IN.cmds[i].type = (uint8_t)IDA_CMD_ARM;
    if (IN.n_cmds <= i) {
        IN.n_cmds = (uint8_t)(i + 1u);
    }
}

static void test_ilk_degerler_kisitlayan(void)
{
    hazirla();
    TEST_ASSERT_EQUAL(IDA_RC_OK, step());
    const uint32_t emn = IDA_FCI_ESTOP | IDA_FCI_PLUG_T0 | IDA_FCI_PLUG_T1 | IDA_FCI_PIN_T0 |
                         IDA_FCI_PIN_T1 | IDA_FCI_EMPTY_T0 | IDA_FCI_EMPTY_T1;
    TEST_ASSERT_EQUAL_HEX32(emn, OUT.fc.inhibit_mask & emn); /* A4-K116, K128 */
    TEST_ASSERT_TRUE((OUT.fc.inhibit_mask & IDA_FCI_NOT_ARMED) != 0u);
    TEST_ASSERT_TRUE((OUT.fc.inhibit_mask & IDA_FCI_MODE) != 0u);
    TEST_ASSERT_TRUE((OUT.fc.inhibit_mask & IDA_FCI_DET_STALE) != 0u);
    TEST_ASSERT_FALSE(OUT.health.narrowed);
    /* Gevşeten örnek W2'de etkin değeri gevşetmez (gevşetme W4'ün). */
    emniyet_ornegi(0u, 1u);
    TEST_ASSERT_EQUAL(IDA_RC_OK, step());
    TEST_ASSERT_EQUAL_HEX32(emn, OUT.fc.inhibit_mask & emn);
    TEST_ASSERT_EQUAL_UINT32(0u, OUT.status.cnt.sample_rejected);
}

static void test_disarm_mandali_ve_k127(void)
{
    hazirla();
    TEST_ASSERT_EQUAL(IDA_RC_OK, step());
    C.emn.armed = true;
    C.emn.mode = (uint8_t)IDA_MODE_STABILIZE; /* test kancası: INIT dışı kip */
    IN.narrow.mask = IDA_NARROW_DISARM;
    IN.narrow.cmd_id[0] = 9u;
    TEST_ASSERT_EQUAL(IDA_RC_OK, step());
    TEST_ASSERT_FALSE(OUT.armed);
    TEST_ASSERT_EQUAL_UINT8(IDA_MODE_STABILIZE, OUT.status.mode); /* kip değişmez */
    TEST_ASSERT_EQUAL_UINT32(1u, ds_gunluk_say(&OUT, (uint16_t)IDA_LOG_ARM_CHANGE));
    TEST_ASSERT_EQUAL_UINT8(IDA_ACK_ACCEPTED, ds_sonuc(&OUT, 9u)->ack);
    TEST_ASSERT_TRUE(OUT.health.narrowed);
    /* NARROW_MODE: STABILIZE → MANUAL */
    IN.narrow.mask = IDA_NARROW_MODE;
    IN.narrow.mode = (uint8_t)IDA_MODE_MANUAL;
    TEST_ASSERT_EQUAL(IDA_RC_OK, step());
    TEST_ASSERT_EQUAL_UINT8(IDA_MODE_MANUAL, OUT.status.mode);
    TEST_ASSERT_EQUAL_UINT32(1u, ds_gunluk_say(&OUT, (uint16_t)IDA_LOG_MODE_CHANGE));
    /* INIT'te NARROW_MODE kipi değiştirmez ve ACCEPTED (A4-K127) */
    C.emn.mode = (uint8_t)IDA_MODE_INIT;
    IN.narrow.cmd_id[1] = 10u;
    TEST_ASSERT_EQUAL(IDA_RC_OK, step());
    TEST_ASSERT_EQUAL_UINT8(IDA_MODE_INIT, OUT.status.mode);
    TEST_ASSERT_EQUAL_UINT8(IDA_ACK_ACCEPTED, ds_sonuc(&OUT, 10u)->ack);
    TEST_ASSERT_TRUE(OUT.health.narrowed);
}

static void test_cmds_daraltici_reddedilen_cagrida(void)
{
    hazirla();
    TEST_ASSERT_EQUAL(IDA_RC_OK, step());
    C.emn.armed = true;
    /* DISARM platform hatasıyla cmds[]'te, çağrı DUP_SEQ: yine uygulanır ve ACCEPTED. */
    IN.n_cmds = 2u;
    IN.cmds[0].cmd_id = 20u;
    IN.cmds[0].type = (uint8_t)IDA_CMD_DISARM;
    IN.cmds[1].cmd_id = 21u;
    IN.cmds[1].type = (uint8_t)IDA_CMD_HEARTBEAT;
    g_seq--; /* aynı seq */
    g_t -= 5 * DS_MS;
    IN.imu.hdr.seq = g_seq;
    TEST_ASSERT_EQUAL(IDA_RC_DUP_SEQ, step());
    TEST_ASSERT_FALSE(OUT.armed);
    TEST_ASSERT_EQUAL_UINT8(IDA_ACK_ACCEPTED, ds_sonuc(&OUT, 20u)->ack);
    TEST_ASSERT_EQUAL_UINT16(IDA_REASON_CALL_REJECTED, ds_sonuc(&OUT, 21u)->reason);
    TEST_ASSERT_TRUE(OUT.health.narrowed);
    /* Hedefi 1 olan SET_MODE cmds[]'te: BAD_INPUT, ACCEPTED, kip değişmez (A4-K46). */
    C.emn.mode = (uint8_t)IDA_MODE_STABILIZE;
    IN.n_cmds = 1u;
    IN.cmds[0].cmd_id = 22u;
    IN.cmds[0].type = (uint8_t)IDA_CMD_SET_MODE;
    IN.cmds[0].u.set_mode.mode = 1u;
    TEST_ASSERT_EQUAL(IDA_RC_BAD_INPUT, step());
    TEST_ASSERT_EQUAL_UINT8(IDA_ACK_ACCEPTED, ds_sonuc(&OUT, 22u)->ack);
    TEST_ASSERT_EQUAL_UINT8(IDA_MODE_STABILIZE, OUT.status.mode);
    /* Daraltıcı cmds[] yineleme penceresinden muaf: aynı kimlik yine ACCEPTED. */
    IN.cmds[0].type = (uint8_t)IDA_CMD_DISARM;
    TEST_ASSERT_EQUAL(IDA_RC_OK, step());
    TEST_ASSERT_EQUAL_UINT8(IDA_ACK_ACCEPTED, ds_sonuc(&OUT, 22u)->ack);
    TEST_ASSERT_EQUAL(IDA_RC_OK, step());
    TEST_ASSERT_EQUAL_UINT8(IDA_ACK_ACCEPTED, ds_sonuc(&OUT, 22u)->ack);
}

static void test_oturum_sonu(void)
{
    hazirla();
    TEST_ASSERT_EQUAL(IDA_RC_OK, step());
    C.emn.armed = true;
    gevset();
    IN.plat.link_event = (uint8_t)IDA_LINKEV_SESSION_END;
    arm_komutu(0u, 30u);
    TEST_ASSERT_EQUAL(IDA_RC_OK, step());
    TEST_ASSERT_FALSE(OUT.armed);
    TEST_ASSERT_EQUAL_UINT32(1u, ds_olay_say(&OUT, (uint8_t)IDA_EVT_SAFETY_CLOSE));
    TEST_ASSERT_EQUAL_UINT8(IDA_TUBE_ALL, OUT.events[0].tube);
    TEST_ASSERT_EQUAL_UINT16(IDA_REASON_NARROWED, ds_sonuc(&OUT, 30u)->reason);
    TEST_ASSERT_EQUAL_UINT32(1u, ds_gunluk_say(&OUT, (uint16_t)IDA_LOG_SESSION));
    TEST_ASSERT_EQUAL_UINT32(1u, ds_gunluk_say(&OUT, (uint16_t)IDA_LOG_SAFETY_INPUT));
    TEST_ASSERT_EQUAL_UINT32(0u, OUT.status.cnt.narrow_from_rejected);
    TEST_ASSERT_EQUAL_UINT8(0x1Fu, C.emn.kisitli); /* etkin değerler kısıtlayana döndü */
    /* Reddedilen çağrıda SESSION_END: SAFETY_CLOSE son geçerli seq/t ile (A4-K57, K63). */
    const uint32_t son_seq = OUT.seq;
    const int64_t son_t = OUT.t_ref.ns;
    IN.n_cmds = 0u;
    IN.n_samples = 17u;
    TEST_ASSERT_EQUAL(IDA_RC_BAD_INPUT, step());
    TEST_ASSERT_EQUAL_UINT32(1u, ds_olay_say(&OUT, (uint8_t)IDA_EVT_SAFETY_CLOSE));
    TEST_ASSERT_EQUAL_UINT32(son_seq, OUT.events[0].seq);
    TEST_ASSERT_TRUE(son_t == OUT.events[0].t_decide.ns);
}

static void test_kapanma_ve_k45(void)
{
    hazirla();
    TEST_ASSERT_EQUAL(IDA_RC_OK, step());
    gevset();
    emniyet_ornegi(0u, 1u);
    IN.samples[0].u.safety.estop = 1u; /* kabul edilecek örnekte kısıtlayıcı içerik */
    arm_komutu(0u, 40u);
    TEST_ASSERT_EQUAL(IDA_RC_OK, step());
    TEST_ASSERT_EQUAL_UINT32(1u, ds_olay_say(&OUT, (uint8_t)IDA_EVT_SAFETY_CLOSE));
    TEST_ASSERT_EQUAL_UINT8(IDA_TUBE_ALL, OUT.events[0].tube); /* acil durdurma iki tüp */
    TEST_ASSERT_EQUAL_UINT16(IDA_REASON_NARROWED, ds_sonuc(&OUT, 40u)->reason);
    TEST_ASSERT_EQUAL_UINT32(1u, OUT.status.cnt.widen_blocked);
    TEST_ASSERT_TRUE(OUT.health.narrowed);
    TEST_ASSERT_EQUAL_UINT32(1u, ds_gunluk_say(&OUT, (uint16_t)IDA_LOG_SAFETY_INPUT));
    TEST_ASSERT_EQUAL_UINT32(0u, OUT.status.cnt.narrow_from_rejected); /* kabul edilen örnek */
    TEST_ASSERT_TRUE((OUT.fc.inhibit_mask & IDA_FCI_ESTOP) != 0u);
    /* Yinelenen kısıtlama daraltıcı girdi değildir: olay yok, ARM NARROWED değil (A4-K44). */
    emniyet_ornegi(0u, 2u);
    IN.samples[0].u.safety.estop = 1u;
    arm_komutu(0u, 41u);
    TEST_ASSERT_EQUAL(IDA_RC_OK, step());
    TEST_ASSERT_EQUAL_UINT8(0u, OUT.n_events);
    TEST_ASSERT_FALSE(OUT.health.narrowed);
    TEST_ASSERT_EQUAL_UINT16(IDA_REASON_NOT_ARMABLE, ds_sonuc(&OUT, 41u)->reason);
}

static void test_reddedilenden_daralma(void)
{
    hazirla();
    TEST_ASSERT_EQUAL(IDA_RC_OK, step());
    /* VALID = 0 ile taşınan örnek (A4-K130): reddedilir, kısıtlayıcı içeriği uygulanır. */
    gevset();
    emniyet_ornegi(0u, 1u);
    IN.samples[0].hdr.flags = 0u;
    IN.samples[0].u.safety.tube[1] = (uint8_t)IDA_TUBE_PLUG_IN; /* tüp 1 pim takılı */
    TEST_ASSERT_EQUAL(IDA_RC_OK, step());
    TEST_ASSERT_EQUAL_UINT32(1u, OUT.status.cnt.sample_rejected);
    TEST_ASSERT_EQUAL_UINT32(1u, OUT.status.cnt.narrow_from_rejected);
    TEST_ASSERT_EQUAL_UINT32(1u, ds_olay_say(&OUT, (uint8_t)IDA_EVT_SAFETY_CLOSE));
    TEST_ASSERT_EQUAL_UINT8(1u, OUT.events[0].tube); /* yalnız tüp 1 */
    TEST_ASSERT_TRUE((OUT.fc.inhibit_mask & IDA_FCI_PIN_T1) != 0u);
    TEST_ASSERT_TRUE((OUT.fc.inhibit_mask & IDA_FCI_PIN_T0) == 0u);
    /* DUP_SEQ dönen çağrıdaki estop = 1 örneği: daraltma uygulanır (§7 A4-K115/K116). */
    gevset();
    IN.samples[0].hdr.flags = (uint16_t)IDA_HF_VALID;
    IN.samples[0].u.safety.estop = 1u;
    g_seq--;
    g_t -= 5 * DS_MS;
    TEST_ASSERT_EQUAL(IDA_RC_DUP_SEQ, step());
    TEST_ASSERT_EQUAL_UINT32(2u, OUT.status.cnt.narrow_from_rejected);
    TEST_ASSERT_EQUAL_UINT32(1u, ds_olay_say(&OUT, (uint8_t)IDA_EVT_SAFETY_CLOSE));
    TEST_ASSERT_TRUE(OUT.health.narrowed);
    /* Herhangi bir inst'in kısıtlayıcı içeriği uygulanır (A4-K116). */
    gevset();
    emniyet_ornegi(0u, 3u);
    IN.samples[0].hdr.inst = 2u;
    IN.samples[0].u.safety.estop = 1u;
    TEST_ASSERT_EQUAL(IDA_RC_OK, step());
    TEST_ASSERT_TRUE((C.emn.kisitli & IDA_EMN_ESTOP) != 0u);
    /* estop = 2: örnek reddedilir, kısıtlayıcı içeriği uygulanır (A4-K33, K116). */
    gevset();
    emniyet_ornegi(0u, 4u);
    IN.samples[0].u.safety.estop = 2u;
    TEST_ASSERT_EQUAL(IDA_RC_OK, step());
    TEST_ASSERT_TRUE((C.emn.kisitli & IDA_EMN_ESTOP) != 0u);
    TEST_ASSERT_EQUAL_UINT32(3u, OUT.status.cnt.narrow_from_rejected);
}

static void test_k125_birlestirme(void)
{
    hazirla();
    TEST_ASSERT_EQUAL(IDA_RC_OK, step());
    /* SESSION_END + acil durdurma + fiş çıkması: tek ALL. */
    gevset();
    emniyet_ornegi(0u, 1u);
    IN.samples[0].u.safety.estop = 1u;
    IN.samples[0].u.safety.tube[0] = (uint8_t)IDA_TUBE_PIN_OUT; /* fiş çıkık */
    IN.plat.link_event = (uint8_t)IDA_LINKEV_SESSION_END;
    TEST_ASSERT_EQUAL(IDA_RC_OK, step());
    TEST_ASSERT_EQUAL_UINT8(1u, OUT.n_events);
    TEST_ASSERT_EQUAL_UINT8(IDA_TUBE_ALL, OUT.events[0].tube);
    /* Yalnız iki tüpün pimleri: iki SAFETY_CLOSE. */
    IN.plat.link_event = (uint8_t)IDA_LINKEV_NONE;
    gevset();
    emniyet_ornegi(0u, 2u);
    IN.samples[0].u.safety.tube[0] = (uint8_t)(IDA_TUBE_LOADED | IDA_TUBE_PLUG_IN);
    IN.samples[0].u.safety.tube[1] = (uint8_t)(IDA_TUBE_LOADED | IDA_TUBE_PLUG_IN);
    TEST_ASSERT_EQUAL(IDA_RC_OK, step());
    TEST_ASSERT_EQUAL_UINT8(2u, OUT.n_events);
    TEST_ASSERT_EQUAL_UINT8(0u, OUT.events[0].tube);
    TEST_ASSERT_EQUAL_UINT8(1u, OUT.events[1].tube);
    TEST_ASSERT_EQUAL_UINT8(IDA_EVT_SAFETY_CLOSE, OUT.events[1].kind);
}

static void test_out_null_bekletme(void)
{
    hazirla();
    TEST_ASSERT_EQUAL(IDA_RC_OK, step());
    /* out NULL çağrısında oturum sonu: kapanma ve günlük bekletilir, sonra bir kez çıkar. */
    g_seq++;
    g_t += 5 * DS_MS;
    IN.now_w.ns = g_t;
    IN.tf.seq = g_seq;
    IN.tf.t.ns = g_t;
    IN.tf.t_rx.ns = g_t;
    ds_imu(&IN.imu, g_t, g_seq);
    IN.plat.link_event = (uint8_t)IDA_LINKEV_SESSION_END;
    TEST_ASSERT_EQUAL(IDA_RC_BAD_INPUT, ida_core_step(&C, &IN, NULL));
    IN.plat.link_event = (uint8_t)IDA_LINKEV_NONE;
    TEST_ASSERT_EQUAL(IDA_RC_OK, step());
    TEST_ASSERT_EQUAL_UINT32(1u, ds_olay_say(&OUT, (uint8_t)IDA_EVT_SAFETY_CLOSE));
    TEST_ASSERT_EQUAL_UINT32(OUT.seq, OUT.events[0].seq); /* bu çağrının seq'i (A4-K63) */
    TEST_ASSERT_EQUAL_UINT32(1u, ds_gunluk_say(&OUT, (uint16_t)IDA_LOG_SESSION));
    TEST_ASSERT_EQUAL_UINT32(0u, OUT.status.cnt.bad_input);
    TEST_ASSERT_EQUAL(IDA_RC_OK, step());
    TEST_ASSERT_EQUAL_UINT8(0u, OUT.n_events);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_ilk_degerler_kisitlayan);
    RUN_TEST(test_disarm_mandali_ve_k127);
    RUN_TEST(test_cmds_daraltici_reddedilen_cagrida);
    RUN_TEST(test_oturum_sonu);
    RUN_TEST(test_kapanma_ve_k45);
    RUN_TEST(test_reddedilenden_daralma);
    RUN_TEST(test_k125_birlestirme);
    RUN_TEST(test_out_null_bekletme);
    return UNITY_END();
}
