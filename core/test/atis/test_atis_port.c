/*
 * test_atis_port.c — atış kontrol emniyet zincirinin port yolundan sınanan maddeleri
 * (ida_core_step/tick; silahlandırma gerektirmeyenler: atis_duzenek.h başı). Test adları G-Ç
 * madde kimliğini taşır (03 Ek).
 */
#include <string.h>

#include "atis/atis_ic.h"
#include "destek.h"
#include "unity.h"
#include "zaman/zaman.h"

void setUp(void);
void tearDown(void);
void setUp(void) {}
void tearDown(void) {}

static ida_core_t C;
static ida_in_t IN;
static ida_out_t OUT;
static int64_t T;
static uint32_t SEQ;
static uint32_t ESEQ;
static uint32_t CID;
static bool G_KAPAT_YANIT;
static uint64_t G_KAPAT_ID[8];
static uint32_t G_KAPAT_N;

#define HAZIR ((uint8_t)(IDA_TUBE_LOADED | IDA_TUBE_PLUG_IN | IDA_TUBE_PIN_OUT))
#define EMN_BITLERI                                                                                \
    (IDA_FCI_ESTOP | IDA_FCI_PLUG_T0 | IDA_FCI_PLUG_T1 | IDA_FCI_PIN_T0 | IDA_FCI_PIN_T1 |         \
     IDA_FCI_EMPTY_T0 | IDA_FCI_EMPTY_T1)

static void hazirla(void)
{
    TEST_ASSERT_EQUAL(IDA_RC_OK, ds_kur(&C));
    T = 2000 * DS_MS;
    SEQ = 0u;
    ESEQ = 0u;
    CID = 5000u;
    G_KAPAT_YANIT = true;
    G_KAPAT_N = 0u;
}

static void yeni(bool step)
{
    SEQ++;
    T += 5 * DS_MS;
    ds_girdi(&IN, T, SEQ, step);
    if (G_KAPAT_YANIT) {
        for (uint32_t i = 0u; (i < G_KAPAT_N) && (IN.n_evres < IDA_PORT_MAX_EVRES); i++) {
            ida_event_result_t *r = &IN.evres[IN.n_evres];
            IN.n_evres++;
            r->event_id = G_KAPAT_ID[i];
            r->result = (uint8_t)IDA_EVR_APPLIED;
            r->t_apply.ns = T - DS_MS;
        }
    }
    G_KAPAT_N = 0u;
}

static ida_rc_t cagir(bool step)
{
    const ida_rc_t rc = step ? ida_core_step(&C, &IN, &OUT) : ida_core_tick(&C, &IN, &OUT);
    ds_cikti_degismezleri(rc, &IN, &OUT);
    /* Port yolunda genişletici olay hiçbir zaman yoktur (kestirim OK olamaz; FC_ARM yok). */
    TEST_ASSERT_EQUAL_UINT32(0u, ds_olay_say(&OUT, (uint8_t)IDA_EVT_SAFETY_OPEN));
    TEST_ASSERT_EQUAL_UINT32(0u, ds_olay_say(&OUT, (uint8_t)IDA_EVT_FIRE));
    for (uint32_t i = 0u; i < (uint32_t)OUT.n_events; i++) {
        if (G_KAPAT_N < 8u) {
            G_KAPAT_ID[G_KAPAT_N] = OUT.events[i].event_id;
            G_KAPAT_N++;
        }
    }
    return rc;
}

static ida_sample_t *emn(uint8_t inst, uint8_t estop, uint8_t t0, uint8_t t1)
{
    ESEQ++;
    ida_sample_t *s = &IN.samples[IN.n_samples];
    IN.n_samples++;
    ds_emniyet(s, T, ESEQ);
    s->hdr.inst = inst;
    s->u.safety.estop = estop;
    s->u.safety.tube[0] = t0;
    s->u.safety.tube[1] = t1;
    return s;
}

static uint32_t fc(uint8_t op, uint8_t tube)
{
    CID++;
    ida_cmd_t *k = &IN.cmds[IN.n_cmds];
    IN.n_cmds++;
    (void)memset(k, 0, sizeof(*k));
    k->cmd_id = CID;
    k->type = (uint8_t)IDA_CMD_FC;
    k->u.fc.op = op;
    k->u.fc.tube = tube;
    return CID;
}

static const ida_cmd_result_t *sonuc(uint32_t id)
{
    const ida_cmd_result_t *r = ds_sonuc(&OUT, id);
    TEST_ASSERT_NOT_NULL(r);
    return r;
}

static void ret_bekle(uint32_t id, uint16_t neden)
{
    TEST_ASSERT_EQUAL_UINT8(IDA_ACK_REJECTED, sonuc(id)->ack);
    TEST_ASSERT_EQUAL_UINT16(neden, sonuc(id)->reason);
}

/* Gevşetilmiş emniyet (ardından her 50 ms'de örnek gelmesi gerekir: G-Ç29). */
static void gevset(void)
{
    yeni(true);
    (void)emn(0u, 0u, HAZIR, HAZIR);
    TEST_ASSERT_EQUAL(IDA_RC_OK, cagir(true));
    TEST_ASSERT_EQUAL_HEX32(0u, OUT.fc.inhibit_mask & EMN_BITLERI);
}

static uint32_t kapanma(uint8_t tube)
{
    uint32_t n = 0u;
    for (uint32_t i = 0u; i < (uint32_t)OUT.n_events; i++) {
        if ((OUT.events[i].kind == (uint8_t)IDA_EVT_SAFETY_CLOSE) && (OUT.events[i].tube == tube)) {
            n++;
        }
    }
    return n;
}

/* SAFETY_CLOSE'u yanıtsız bırakıp FC'yi kilitler (G-Ç50). */
static void kilitle(void)
{
    gevset();
    G_KAPAT_YANIT = false;
    yeni(true);
    (void)emn(0u, 1u, HAZIR, HAZIR);
    TEST_ASSERT_EQUAL(IDA_RC_OK, cagir(true));
    TEST_ASSERT_EQUAL_UINT32(1u, kapanma((uint8_t)IDA_TUBE_ALL));
    for (uint32_t i = 0u; (i < 40u) && (OUT.fc.state != (uint8_t)IDA_FCS_FAULT_LATCHED); i++) {
        yeni(true);
        (void)emn(0u, 1u, HAZIR, HAZIR);
        (void)cagir(true);
    }
    TEST_ASSERT_EQUAL_UINT8(IDA_FCS_FAULT_LATCHED, OUT.fc.state);
    G_KAPAT_YANIT = true;
}

/* ---- G-Ç01, G-Ç31 ------------------------------------------------------------------------- */

static void test_gc01_init_ve_yeniden_init(void)
{
    hazirla();
    yeni(true);
    TEST_ASSERT_EQUAL(IDA_RC_OK, cagir(true));
    TEST_ASSERT_EQUAL_UINT8(IDA_FCS_SAFE, OUT.fc.state);
    TEST_ASSERT_FALSE(OUT.armed);
    TEST_ASSERT_EQUAL_UINT8(0u, OUT.n_events);
    /* İç durum silahlıyken (kanca) yeniden init: silahlandırma taşınmaz. */
    ida_fc_tup_yaz(&C.fc, 0u);
    ida_fc_ic_yaz(&C.fc, IDA_FC_IC_FIRE_AUTH, T);
    C.fc.istek_var = true;
    C.emn.armed = true;
    TEST_ASSERT_EQUAL(IDA_RC_OK, ds_kur(&C));
    yeni(true);
    TEST_ASSERT_EQUAL(IDA_RC_OK, cagir(true));
    TEST_ASSERT_EQUAL_UINT8(IDA_FCS_SAFE, OUT.fc.state);
    TEST_ASSERT_FALSE(OUT.armed);
    TEST_ASSERT_EQUAL_UINT8(0u, OUT.n_events);
    TEST_ASSERT_TRUE((OUT.fc.inhibit_mask & IDA_FCI_NOT_ARMED) != 0u);
}

static void test_gc31_kurulmamis(void)
{
    static ida_core_t bos;
    (void)memset(&bos, 0x5A, sizeof(bos));
    ds_girdi(&IN, 2000 * DS_MS, 1u, true);
    ds_desen(&OUT);
    TEST_ASSERT_EQUAL(IDA_RC_NOT_INIT, ida_core_step(&bos, &IN, &OUT));
    TEST_ASSERT_EQUAL_UINT8(IDA_FCS_SAFE, OUT.fc.state);
    TEST_ASSERT_EQUAL_HEX32(0x00026FFFu, OUT.fc.inhibit_mask);
    TEST_ASSERT_EQUAL_UINT8(0u, OUT.n_events);
    TEST_ASSERT_EQUAL_UINT8(0u, OUT.n_logs);
}

/* ---- G-Ç03: engel bitleri koşulla aynı çağrıda ------------------------------------------- */

static void test_gc03_engel_bitleri(void)
{
    hazirla();
    yeni(false);
    TEST_ASSERT_EQUAL(IDA_RC_OK, cagir(false));
    /* G-Ç17, G-Ç43: ilk kabulden önce tüpler boş ve IMU bayat. A4-K118 (W5-K21): algılama
       akışı init'ten T_det_stale (2 s) dolmadan bayat sayılmaz. */
    TEST_ASSERT_EQUAL_HEX32(EMN_BITLERI, OUT.fc.inhibit_mask & EMN_BITLERI);
    TEST_ASSERT_TRUE((OUT.fc.inhibit_mask & IDA_FCI_IMU_STALE) != 0u);
    TEST_ASSERT_TRUE((OUT.fc.inhibit_mask & IDA_FCI_DET_STALE) == 0u);
    TEST_ASSERT_TRUE((OUT.fc.inhibit_mask & IDA_FCI_MODE) != 0u); /* kip izin kümesinde değil */
    TEST_ASSERT_TRUE((OUT.fc.inhibit_mask & IDA_FCI_NOT_ARMED) != 0u);
    TEST_ASSERT_TRUE((OUT.fc.inhibit_mask & IDA_FCI_EST) != 0u);
    gevset();
    TEST_ASSERT_EQUAL_UINT32(0u, OUT.fc.inhibit_mask & IDA_FCI_IMU_STALE);
    /* Her emniyet biti kendi koşuluyla aynı çağrıda kurulur ve kalkar. */
    const struct {
        uint8_t estop;
        uint8_t t0;
        uint8_t t1;
        uint32_t bit;
    } v[] = {
        {1u, HAZIR, HAZIR, IDA_FCI_ESTOP},
        {0u, (uint8_t)(HAZIR & ~IDA_TUBE_PLUG_IN), HAZIR, IDA_FCI_PLUG_T0},
        {0u, HAZIR, (uint8_t)(HAZIR & ~IDA_TUBE_PLUG_IN), IDA_FCI_PLUG_T1},
        {0u, (uint8_t)(HAZIR & ~IDA_TUBE_PIN_OUT), HAZIR, IDA_FCI_PIN_T0},
        {0u, HAZIR, (uint8_t)(HAZIR & ~IDA_TUBE_PIN_OUT), IDA_FCI_PIN_T1},
        {0u, (uint8_t)(HAZIR & ~IDA_TUBE_LOADED), HAZIR, IDA_FCI_EMPTY_T0},
        {0u, HAZIR, (uint8_t)(HAZIR & ~IDA_TUBE_LOADED), IDA_FCI_EMPTY_T1},
    };
    for (uint32_t i = 0u; i < (uint32_t)(sizeof(v) / sizeof(v[0])); i++) {
        yeni(true);
        (void)emn(0u, v[i].estop, v[i].t0, v[i].t1);
        TEST_ASSERT_EQUAL(IDA_RC_OK, cagir(true));
        TEST_ASSERT_EQUAL_HEX32(v[i].bit, OUT.fc.inhibit_mask & EMN_BITLERI);
        yeni(true);
        (void)emn(0u, 0u, HAZIR, HAZIR);
        TEST_ASSERT_EQUAL(IDA_RC_OK, cagir(true));
        TEST_ASSERT_EQUAL_HEX32(0u, OUT.fc.inhibit_mask & EMN_BITLERI);
    }
    /* SESSION: link_state ile aynı çağrıda. */
    yeni(true);
    (void)emn(0u, 0u, HAZIR, HAZIR);
    IN.plat.link_state = (uint8_t)IDA_LINK_NONE;
    TEST_ASSERT_EQUAL(IDA_RC_OK, cagir(true));
    TEST_ASSERT_TRUE((OUT.fc.inhibit_mask & IDA_FCI_SESSION) != 0u);
    yeni(true);
    (void)emn(0u, 0u, HAZIR, HAZIR);
    TEST_ASSERT_EQUAL(IDA_RC_OK, cagir(true));
    TEST_ASSERT_EQUAL_UINT32(0u, OUT.fc.inhibit_mask & IDA_FCI_SESSION);
    /* FAULT_LATCHED: kilitlendiği ve FC_RESET'le çıktığı çağrıda. */
    kilitle();
    TEST_ASSERT_TRUE((OUT.fc.inhibit_mask & IDA_FCI_FAULT_LATCHED) != 0u);
    yeni(true);
    (void)emn(0u, 0u, HAZIR, HAZIR);
    const uint32_t id = fc((uint8_t)IDA_FC_RESET, 0u);
    TEST_ASSERT_EQUAL(IDA_RC_OK, cagir(true));
    TEST_ASSERT_EQUAL_UINT8(IDA_ACK_ACCEPTED, sonuc(id)->ack);
    TEST_ASSERT_EQUAL_UINT32(0u, OUT.fc.inhibit_mask & IDA_FCI_FAULT_LATCHED);
    TEST_ASSERT_EQUAL_UINT8(IDA_FCS_SAFE, OUT.fc.state);
}

/* ---- G-Ç04, G-Ç32, G-Ç33, G-Ç34: sonuç kodlarının sırası ---------------------------------- */

static void test_gc04_aralik_disi(void)
{
    hazirla();
    gevset();
    yeni(true);
    const uint32_t a = fc(4u, 0u);
    const uint32_t b = fc((uint8_t)IDA_FC_FIRE, 2u);
    const uint32_t c = fc((uint8_t)IDA_FC_ARM, 0u);
    IN.cmds[2].u.fc.arg0 = 1u;
    const uint32_t d = fc((uint8_t)IDA_FC_TEST_EXIT, 0u);
    IN.cmds[3].u.fc.arg1 = 9u;
    const uint32_t e = fc(9u, 1u);
    TEST_ASSERT_EQUAL(IDA_RC_OK, cagir(true));
    ret_bekle(a, (uint16_t)IDA_REASON_OUT_OF_RANGE);
    ret_bekle(b, (uint16_t)IDA_REASON_OUT_OF_RANGE);
    ret_bekle(c, (uint16_t)IDA_REASON_OUT_OF_RANGE);
    ret_bekle(d, (uint16_t)IDA_REASON_OUT_OF_RANGE);
    ret_bekle(e, (uint16_t)IDA_REASON_OUT_OF_RANGE);
    TEST_ASSERT_EQUAL_UINT8(IDA_FCS_SAFE, OUT.fc.state);
    /* Daraltmalı çağrıda: kapalı liste dışı op genişletici değildir → OUT_OF_RANGE; listedeki
       genişletici NARROWED (OUT_OF_RANGE'den önce). */
    yeni(true);
    IN.narrow.mask = IDA_NARROW_FC_DISARM;
    IN.narrow.cmd_id[2] = 1u;
    const uint32_t f = fc(4u, 0u);
    const uint32_t g = fc((uint8_t)IDA_FC_FIRE, 7u);
    TEST_ASSERT_EQUAL(IDA_RC_OK, cagir(true));
    ret_bekle(f, (uint16_t)IDA_REASON_OUT_OF_RANGE);
    ret_bekle(g, (uint16_t)IDA_REASON_NARROWED);
}

static void test_gc32_daraltici_cmds(void)
{
    hazirla();
    gevset();
    /* cmds[]'te biçimi bozuk FC_DISARM ve FC_ABORT: alanları okunmaz, ACCEPTED, uygulanır. */
    yeni(true);
    (void)emn(0u, 0u, HAZIR, HAZIR);
    const uint32_t a = fc((uint8_t)IDA_FC_DISARM, 0u);
    IN.cmds[0].u.fc.arg0 = 77u;
    const uint32_t b = fc((uint8_t)IDA_FC_ABORT, 200u);
    IN.cmds[1].u.fc.arg1 = 5u;
    const uint32_t c = fc((uint8_t)IDA_FC_FIRE, 0u);
    TEST_ASSERT_EQUAL(IDA_RC_OK, cagir(true));
    TEST_ASSERT_EQUAL_UINT8(IDA_ACK_ACCEPTED, sonuc(a)->ack);
    TEST_ASSERT_EQUAL_UINT8(IDA_ACK_ACCEPTED, sonuc(b)->ack);
    ret_bekle(c, (uint16_t)IDA_REASON_NARROWED);
    TEST_ASSERT_TRUE(OUT.health.narrowed);
    /* Yineleme penceresine girmez: aynı kimlikle daraltıcı yine ACCEPTED; aynı kimlikli nötr
       komut DUPLICATE değildir. */
    yeni(true);
    (void)emn(0u, 0u, HAZIR, HAZIR);
    ida_cmd_t *k = &IN.cmds[IN.n_cmds];
    IN.n_cmds++;
    (void)memset(k, 0, sizeof(*k));
    k->cmd_id = a;
    k->type = (uint8_t)IDA_CMD_FC;
    k->u.fc.op = (uint8_t)IDA_FC_DISARM;
    ida_cmd_t *h = &IN.cmds[IN.n_cmds];
    IN.n_cmds++;
    (void)memset(h, 0, sizeof(*h));
    h->cmd_id = b;
    h->type = (uint8_t)IDA_CMD_HEARTBEAT;
    TEST_ASSERT_EQUAL(IDA_RC_OK, cagir(true));
    TEST_ASSERT_EQUAL_UINT8(IDA_ACK_ACCEPTED, OUT.results[0].ack);
    TEST_ASSERT_EQUAL_UINT8(IDA_ACK_ACCEPTED, OUT.results[1].ack);
    /* Reddedilen çağrıda da uygulanır ve ACCEPTED; n_cmds > 8'de sonuç yok ama uygulanır. */
    yeni(true);
    IN.tf.seq = SEQ - 1u; /* DUP_SEQ */
    const uint32_t d = fc((uint8_t)IDA_FC_ABORT, 1u);
    const uint32_t e = fc((uint8_t)IDA_FC_FIRE, 0u);
    TEST_ASSERT_EQUAL(IDA_RC_DUP_SEQ, cagir(true));
    TEST_ASSERT_EQUAL_UINT8(IDA_ACK_ACCEPTED, sonuc(d)->ack);
    ret_bekle(e, (uint16_t)IDA_REASON_CALL_REJECTED); /* G-Ç34 */
    TEST_ASSERT_TRUE(OUT.health.narrowed);
    SEQ--;
    yeni(true);
    for (uint32_t i = 0u; i < 8u; i++) {
        (void)fc((uint8_t)IDA_FC_TEST_EXIT, 0u);
    }
    IN.cmds[7].u.fc.op = (uint8_t)IDA_FC_DISARM;
    IN.n_cmds = 9u;
    TEST_ASSERT_EQUAL(IDA_RC_BAD_INPUT, cagir(true));
    TEST_ASSERT_EQUAL_UINT8(0u, OUT.n_results);
    TEST_ASSERT_TRUE(OUT.health.narrowed);
}

static void test_gc33_gc34_daraltmada_genisletici(void)
{
    hazirla();
    gevset();
    yeni(true);
    (void)emn(0u, 0u, HAZIR, HAZIR);
    const uint32_t onceki = OUT.status.cnt.widen_blocked;
    IN.narrow.mask = IDA_NARROW_FC_ABORT;
    IN.narrow.cmd_id[3] = 9u;
    const uint32_t ids[4] = {fc((uint8_t)IDA_FC_FIRE, 0u), fc((uint8_t)IDA_FC_ARM, 1u),
                             fc((uint8_t)IDA_FC_RESET, 0u), fc((uint8_t)IDA_FC_TEST_ENTER, 0u)};
    TEST_ASSERT_EQUAL(IDA_RC_OK, cagir(true));
    for (uint32_t i = 0u; i < 4u; i++) {
        ret_bekle(ids[i], (uint16_t)IDA_REASON_NARROWED);
    }
    TEST_ASSERT_EQUAL_UINT32(onceki + 4u, OUT.status.cnt.widen_blocked);
    /* Görülmüş kimlik DUPLICATE (NARROWED'den önce). */
    yeni(true);
    (void)emn(0u, 0u, HAZIR, HAZIR);
    IN.narrow.mask = IDA_NARROW_FC_ABORT;
    ida_cmd_t *k = &IN.cmds[0];
    (void)memset(k, 0, sizeof(*k));
    IN.n_cmds = 1u;
    k->cmd_id = ids[0];
    k->type = (uint8_t)IDA_CMD_FC;
    k->u.fc.op = (uint8_t)IDA_FC_FIRE;
    TEST_ASSERT_EQUAL(IDA_RC_OK, cagir(true));
    TEST_ASSERT_EQUAL_UINT8(IDA_ACK_DUPLICATE, OUT.results[1].ack);
    /* Reddedilen çağrıda CALL_REJECTED ve pencereye girmez (G-Ç34). */
    yeni(true);
    IN.now_w.ns = IDA_TIME_MAX_NS + 1; /* BAD_INPUT */
    const uint32_t r = fc((uint8_t)IDA_FC_ARM, 0u);
    TEST_ASSERT_EQUAL(IDA_RC_BAD_INPUT, cagir(true));
    ret_bekle(r, (uint16_t)IDA_REASON_CALL_REJECTED);
    yeni(true);
    (void)emn(0u, 0u, HAZIR, HAZIR);
    ida_cmd_t *m = &IN.cmds[0];
    (void)memset(m, 0, sizeof(*m));
    IN.n_cmds = 1u;
    m->cmd_id = r;
    m->type = (uint8_t)IDA_CMD_FC;
    m->u.fc.op = (uint8_t)IDA_FC_ARM;
    TEST_ASSERT_EQUAL(IDA_RC_OK, cagir(true));
    TEST_ASSERT_TRUE(OUT.results[0].ack != (uint8_t)IDA_ACK_DUPLICATE);
}

/* ---- G-Ç22 (port): kip izin kümesinde değil → BAD_MODE; oturum ve olay kimliği ------------- */

static void test_gc22_port_satirlari(void)
{
    hazirla();
    gevset();
    yeni(true);
    (void)emn(0u, 0u, HAZIR, HAZIR);
    const uint32_t a = fc((uint8_t)IDA_FC_ARM, 0u);
    const uint32_t f = fc((uint8_t)IDA_FC_FIRE, 0u);
    const uint32_t t = fc((uint8_t)IDA_FC_TEST_ENTER, 0u);
    const uint32_t r = fc((uint8_t)IDA_FC_RESET, 0u);
    const uint32_t x = fc((uint8_t)IDA_FC_TEST_EXIT, 0u);
    TEST_ASSERT_EQUAL(IDA_RC_OK, cagir(true));
    ret_bekle(a, (uint16_t)IDA_REASON_BAD_MODE);
    ret_bekle(f, (uint16_t)IDA_REASON_BAD_MODE);
    ret_bekle(t, (uint16_t)IDA_REASON_BAD_MODE);
    ret_bekle(r, (uint16_t)IDA_REASON_BAD_MODE); /* G-Ç46: FAULT_LATCHED değil */
    TEST_ASSERT_EQUAL_UINT8(IDA_ACK_ACCEPTED, sonuc(x)->ack); /* G-Ç51: etkisiz */
    TEST_ASSERT_EQUAL_UINT8(IDA_FCS_SAFE, OUT.fc.state);
}

/* ---- G-Ç06, G-Ç15, G-Ç16: daraltıcı girdi ve kapanma ------------------------------------- */

static void test_gc06_gc15_kapanma(void)
{
    hazirla();
    gevset();
    /* Kapanma: bir SAFETY_CLOSE ve bir SAFETY_INPUT; narrowed. */
    yeni(true);
    (void)emn(0u, 0u, (uint8_t)(HAZIR & ~IDA_TUBE_PIN_OUT), HAZIR);
    const uint32_t id = fc((uint8_t)IDA_FC_FIRE, 1u);
    TEST_ASSERT_EQUAL(IDA_RC_OK, cagir(true));
    TEST_ASSERT_TRUE(OUT.health.narrowed);
    TEST_ASSERT_EQUAL_UINT32(1u, kapanma(0u));
    TEST_ASSERT_EQUAL_UINT8(1u, OUT.n_events);
    TEST_ASSERT_EQUAL_UINT32(1u, ds_gunluk_say(&OUT, (uint16_t)IDA_LOG_SAFETY_INPUT));
    ret_bekle(id, (uint16_t)IDA_REASON_NARROWED); /* W5-K10: dönüş çağrısında bütün tüpler */
    /* Süregelen kısıtlama: yeni olay ve daraltma yok; öteki tüpün komutu daraltılmaz. */
    yeni(true);
    (void)emn(0u, 0u, (uint8_t)(HAZIR & ~IDA_TUBE_PIN_OUT), HAZIR);
    const uint32_t id2 = fc((uint8_t)IDA_FC_FIRE, 1u);
    TEST_ASSERT_EQUAL(IDA_RC_OK, cagir(true));
    TEST_ASSERT_FALSE(OUT.health.narrowed);
    TEST_ASSERT_EQUAL_UINT8(0u, OUT.n_events);
    ret_bekle(id2, (uint16_t)IDA_REASON_BAD_MODE); /* kip; NARROWED değil */
    /* Acil durdurma ve pim aynı çağrıda: tek SAFETY_CLOSE(ALL) (G-Ç09). */
    yeni(true);
    (void)emn(0u, 1u, HAZIR, (uint8_t)(HAZIR & ~IDA_TUBE_PIN_OUT));
    TEST_ASSERT_EQUAL(IDA_RC_OK, cagir(true));
    TEST_ASSERT_EQUAL_UINT8(1u, OUT.n_events);
    TEST_ASSERT_EQUAL_UINT32(1u, kapanma((uint8_t)IDA_TUBE_ALL));
    /* Yalnız reddedilen örnekten doğan kapanma narrow_from_rejected'i artırır. */
    gevset();
    const uint32_t nfr = OUT.status.cnt.narrow_from_rejected;
    yeni(true);
    emn(0u, 1u, HAZIR, HAZIR)->hdr.flags = 0u; /* VALID = 0: reddedilir, kısıtlar */
    TEST_ASSERT_EQUAL(IDA_RC_OK, cagir(true));
    TEST_ASSERT_EQUAL_UINT32(nfr + 1u, OUT.status.cnt.narrow_from_rejected);
    TEST_ASSERT_TRUE((OUT.fc.inhibit_mask & IDA_FCI_ESTOP) != 0u);
}

/* ---- G-Ç14: gevşetme --------------------------------------------------------------------- */

static void test_gc14_gevsetme(void)
{
    hazirla();
    /* inst = 1 gevşetmez. */
    yeni(true);
    (void)emn(1u, 0u, HAZIR, HAZIR);
    TEST_ASSERT_EQUAL(IDA_RC_OK, cagir(true));
    TEST_ASSERT_EQUAL_HEX32(EMN_BITLERI, OUT.fc.inhibit_mask & EMN_BITLERI);
    /* VALID = 0 gevşetmez. */
    yeni(true);
    emn(0u, 0u, HAZIR, HAZIR)->hdr.flags = 0u;
    TEST_ASSERT_EQUAL(IDA_RC_OK, cagir(true));
    TEST_ASSERT_EQUAL_HEX32(EMN_BITLERI, OUT.fc.inhibit_mask & EMN_BITLERI);
    /* Reddedilen çağrıdaki örnek gevşetmez. */
    yeni(true);
    (void)emn(0u, 0u, HAZIR, HAZIR);
    IN.n_evres = 5u; /* BAD_INPUT */
    TEST_ASSERT_EQUAL(IDA_RC_BAD_INPUT, cagir(true));
    TEST_ASSERT_EQUAL_HEX32(EMN_BITLERI & ~(IDA_FCI_EMPTY_T0 | IDA_FCI_EMPTY_T1),
                            OUT.fc.inhibit_mask & EMN_BITLERI & ~(IDA_FCI_EMPTY_T0 | IDA_FCI_EMPTY_T1));
    /* Aynı çağrıda başka bir örnek (inst = 2) alanı kısıtlıyorsa o alan gevşemez, öteki gevşer. */
    yeni(true);
    (void)emn(0u, 0u, HAZIR, HAZIR);
    (void)emn(2u, 1u, HAZIR, HAZIR);
    TEST_ASSERT_EQUAL(IDA_RC_OK, cagir(true));
    TEST_ASSERT_EQUAL_HEX32(IDA_FCI_ESTOP, OUT.fc.inhibit_mask & EMN_BITLERI);
    TEST_ASSERT_EQUAL_UINT32(1u, ds_gunluk_say(&OUT, (uint16_t)IDA_LOG_SAFETY_INPUT));
    yeni(true);
    (void)emn(0u, 0u, HAZIR, HAZIR);
    TEST_ASSERT_EQUAL(IDA_RC_OK, cagir(true));
    TEST_ASSERT_EQUAL_HEX32(0u, OUT.fc.inhibit_mask & EMN_BITLERI);
}

/* ---- G-Ç18, G-Ç44, G-Ç45: oturum sonu ve başı ---------------------------------------------- */

static void test_gc18_gc44_gc45_oturum(void)
{
    hazirla();
    gevset();
    yeni(false);
    IN.tf.present = false;
    IN.plat.link_event = (uint8_t)IDA_LINKEV_SESSION_END;
    IN.plat.link_state = (uint8_t)IDA_LINK_NONE;
    TEST_ASSERT_EQUAL(IDA_RC_OK, cagir(false));
    TEST_ASSERT_EQUAL_UINT8(1u, OUT.n_events);
    TEST_ASSERT_EQUAL_UINT32(1u, kapanma((uint8_t)IDA_TUBE_ALL));
    TEST_ASSERT_EQUAL_HEX32(EMN_BITLERI, OUT.fc.inhibit_mask & EMN_BITLERI);
    TEST_ASSERT_EQUAL_UINT8(IDA_FCS_SAFE, OUT.fc.state);
    /* İkinci SESSION_END yeni olay üretmez. */
    yeni(false);
    IN.tf.present = false;
    IN.plat.link_event = (uint8_t)IDA_LINKEV_SESSION_END;
    IN.plat.link_state = (uint8_t)IDA_LINK_NONE;
    TEST_ASSERT_EQUAL(IDA_RC_OK, cagir(false));
    TEST_ASSERT_EQUAL_UINT8(0u, OUT.n_events);
    /* SESSION_START: durum, etkin değerler değişmez, kapanma yok; FAULT_LATCHED korunur. */
    kilitle();
    yeni(true);
    IN.plat.link_event = (uint8_t)IDA_LINKEV_SESSION_START;
    TEST_ASSERT_EQUAL(IDA_RC_OK, cagir(true));
    TEST_ASSERT_EQUAL_UINT8(IDA_FCS_FAULT_LATCHED, OUT.fc.state);
    TEST_ASSERT_EQUAL_UINT8(0u, OUT.n_events);
    /* G-Ç18: FAULT_LATCHED'te SESSION_END durumu değiştirmez, kapatmayı yine üretir. */
    yeni(false);
    IN.tf.present = false;
    IN.plat.link_event = (uint8_t)IDA_LINKEV_SESSION_END;
    IN.plat.link_state = (uint8_t)IDA_LINK_NONE;
    TEST_ASSERT_EQUAL(IDA_RC_OK, cagir(false));
    TEST_ASSERT_EQUAL_UINT8(IDA_FCS_FAULT_LATCHED, OUT.fc.state);
    TEST_ASSERT_EQUAL_UINT32(1u, kapanma((uint8_t)IDA_TUBE_ALL));
}

/* G-Ç18, G-Ç45; W5-K20 (2): bağlantı olayı taşıyan çağrıda gevşek inst 0 örneği kabul edilse
   de hiçbir emniyet alanı gevşemez; SESSION_END çağrısının sonunda değerler kısıtlayandır ve
   tüpler boştur. Sonraki olağan çağrıdaki örnek gevşetir. */
static void test_gc18_gc45_baglanti_olayinda_gevsetme_yok(void)
{
    hazirla();
    gevset();
    yeni(true);
    (void)emn(0u, 0u, HAZIR, HAZIR);
    IN.plat.link_event = (uint8_t)IDA_LINKEV_SESSION_END;
    IN.plat.link_state = (uint8_t)IDA_LINK_NONE;
    TEST_ASSERT_EQUAL(IDA_RC_OK, cagir(true));
    TEST_ASSERT_EQUAL_HEX32(EMN_BITLERI, OUT.fc.inhibit_mask & EMN_BITLERI);
    TEST_ASSERT_EQUAL_UINT32(1u, kapanma((uint8_t)IDA_TUBE_ALL));
    /* SESSION_START çağrısı: gevşek örnek gevşetmez. */
    yeni(true);
    (void)emn(0u, 0u, HAZIR, HAZIR);
    IN.plat.link_event = (uint8_t)IDA_LINKEV_SESSION_START;
    TEST_ASSERT_EQUAL(IDA_RC_OK, cagir(true));
    TEST_ASSERT_EQUAL_HEX32(EMN_BITLERI & ~(IDA_FCI_EMPTY_T0 | IDA_FCI_EMPTY_T1),
                            OUT.fc.inhibit_mask & EMN_BITLERI & ~(IDA_FCI_EMPTY_T0 | IDA_FCI_EMPTY_T1));
    TEST_ASSERT_EQUAL_UINT8(0u, OUT.n_events);
    yeni(true);
    (void)emn(0u, 0u, HAZIR, HAZIR);
    TEST_ASSERT_EQUAL(IDA_RC_OK, cagir(true));
    TEST_ASSERT_EQUAL_HEX32(0u, OUT.fc.inhibit_mask & EMN_BITLERI);
}

/* ---- G-Ç19, G-Ç50, G-Ç42, G-Ç13, G-Ç52 ---------------------------------------------------- */

static void test_gc50_gc42_kapatma_sonucu(void)
{
    hazirla();
    gevset();
    G_KAPAT_YANIT = false;
    yeni(true);
    (void)emn(0u, 1u, HAZIR, HAZIR);
    TEST_ASSERT_EQUAL(IDA_RC_OK, cagir(true));
    const uint64_t id = OUT.events[0].event_id;
    TEST_ASSERT_EQUAL_UINT32(1u, (uint32_t)(id & 0xFFFFFFFFu)); /* G-Ç08: ctr 1'den */
    TEST_ASSERT_EQUAL_UINT32(7u, (uint32_t)(id >> 32));
    TEST_ASSERT_TRUE(OUT.fc.last_event_id == id); /* G-Ç52 */
    TEST_ASSERT_EQUAL_UINT8(0u, OUT.fc.last_event_result);
    /* G-Ç13: bilinmeyen kimlik ve tanımsız kod yok sayılır. */
    yeni(true);
    (void)emn(0u, 1u, HAZIR, HAZIR);
    IN.n_evres = 2u;
    IN.evres[0].event_id = id + 5u;
    IN.evres[0].result = (uint8_t)IDA_EVR_APPLIED;
    IN.evres[1].event_id = id;
    IN.evres[1].result = 7u;
    const uint32_t ign = OUT.status.cnt.evres_ignored;
    TEST_ASSERT_EQUAL(IDA_RC_OK, cagir(true));
    TEST_ASSERT_EQUAL_UINT32(ign + 2u, OUT.status.cnt.evres_ignored);
    TEST_ASSERT_EQUAL_UINT8(0u, OUT.fc.last_event_result);
    /* G-Ç42, G-Ç50: apply_by + 100 ms içinde sonuç yok → bilinmeyen, FAULT_LATCHED, kapatma
       yinelenir. */
    const int64_t cikis = T - (5 * DS_MS);
    bool kilit = false;
    while (!kilit) {
        yeni(true);
        (void)emn(0u, 1u, HAZIR, HAZIR);
        TEST_ASSERT_EQUAL(IDA_RC_OK, cagir(true));
        kilit = OUT.fc.state == (uint8_t)IDA_FCS_FAULT_LATCHED;
        if (kilit) {
            TEST_ASSERT_TRUE((T - cikis) > (120 * DS_MS));
            TEST_ASSERT_EQUAL_UINT32(1u, OUT.status.cnt.event_unknown);
            TEST_ASSERT_EQUAL_UINT32(1u, kapanma((uint8_t)IDA_TUBE_ALL));
            TEST_ASSERT_EQUAL_UINT32(1u, ds_gunluk_say(&OUT, (uint16_t)IDA_LOG_EVENT_UNKNOWN));
        }
        TEST_ASSERT_TRUE((T - cikis) <= (130 * DS_MS));
    }
    /* EXPIRED_NO_ACK da FAULT_LATCHED. */
    hazirla();
    gevset();
    G_KAPAT_YANIT = false;
    yeni(true);
    (void)emn(0u, 1u, HAZIR, HAZIR);
    TEST_ASSERT_EQUAL(IDA_RC_OK, cagir(true));
    const uint64_t id2 = OUT.events[0].event_id;
    yeni(true);
    (void)emn(0u, 1u, HAZIR, HAZIR);
    IN.n_evres = 1u;
    IN.evres[0].event_id = id2;
    IN.evres[0].result = (uint8_t)IDA_EVR_EXPIRED_NO_ACK;
    TEST_ASSERT_EQUAL(IDA_RC_OK, cagir(true));
    TEST_ASSERT_EQUAL_UINT8(IDA_FCS_FAULT_LATCHED, OUT.fc.state);
    TEST_ASSERT_EQUAL_UINT32(1u, kapanma((uint8_t)IDA_TUBE_ALL)); /* kapatma yinelendi */
    TEST_ASSERT_TRUE(OUT.fc.last_event_id > id2); /* G-Ç52: son çıkan olay yinelenen kapatma */
    /* APPLIED olsaydı kilit yok (G-Ç50 yalnız iki sonuç). */
    hazirla();
    gevset();
    yeni(true);
    (void)emn(0u, 1u, HAZIR, HAZIR);
    TEST_ASSERT_EQUAL(IDA_RC_OK, cagir(true));
    for (uint32_t i = 0u; i < 60u; i++) {
        yeni(true);
        (void)emn(0u, 1u, HAZIR, HAZIR);
        TEST_ASSERT_EQUAL(IDA_RC_OK, cagir(true));
    }
    TEST_ASSERT_EQUAL_UINT8(IDA_FCS_SAFE, OUT.fc.state);
    TEST_ASSERT_EQUAL_UINT8(IDA_EVR_APPLIED, OUT.fc.last_event_result);
}

static void test_gc19_port_kilit_korunur(void)
{
    for (uint32_t k = 0u; k < 6u; k++) {
        hazirla();
        kilitle();
        yeni(true);
        if (k == 0u) {
            IN.narrow.mask = IDA_NARROW_FC_DISARM;
        } else if (k == 1u) {
            IN.narrow.mask = IDA_NARROW_FC_ABORT;
        } else if (k == 2u) {
            IN.narrow.mask = IDA_NARROW_DISARM;
        } else if (k == 3u) {
            IN.narrow.mask = IDA_NARROW_MODE;
            IN.narrow.mode = (uint8_t)IDA_MODE_MANUAL;
        } else if (k == 4u) {
            (void)emn(0u, 1u, (uint8_t)(HAZIR & ~IDA_TUBE_PLUG_IN), HAZIR);
        } else {
            /* örnek yok: bayatlık */
            T += 300 * DS_MS;
            IN.now_w.ns = T;
            IN.tf.t.ns = T;
            IN.tf.t_rx.ns = T;
            ds_imu(&IN.imu, T, SEQ);
        }
        (void)cagir(true);
        TEST_ASSERT_EQUAL_UINT8(IDA_FCS_FAULT_LATCHED, OUT.fc.state);
        if (k <= 1u) {
            TEST_ASSERT_EQUAL_UINT32(1u, kapanma((uint8_t)IDA_TUBE_ALL)); /* G-Ç19 */
        }
    }
}

/* ---- G-Ç20, G-Ç46, G-Ç53 (1): FC_RESET ve süren çelişki ------------------------------------ */

static void test_gc53_gc20_celiski_ve_reset(void)
{
    hazirla();
    gevset();
    /* Acil durdurma: SAFETY_CLOSE(ALL) APPLIED (t_apply = T_k − 1 ms). */
    yeni(true);
    (void)emn(0u, 1u, HAZIR, HAZIR);
    TEST_ASSERT_EQUAL(IDA_RC_OK, cagir(true));
    yeni(true); /* sonuç burada */
    const int64_t t_apply = T - DS_MS;
    /* t_meas ≤ t_apply olan örnekte PERMIT_OPEN çelişki değildir; "fiş takılı ve acil durdurma
       basılı" çelişki değildir. */
    ida_sample_t *s = emn(0u, 1u, (uint8_t)(HAZIR | IDA_TUBE_PERMIT_OPEN), HAZIR);
    s->hdr.t_meas.ns = t_apply;
    TEST_ASSERT_EQUAL(IDA_RC_OK, cagir(true));
    TEST_ASSERT_EQUAL_UINT8(IDA_FCS_SAFE, OUT.fc.state);
    /* t_meas > t_apply ve PERMIT_OPEN = 1 → FAULT_LATCHED. */
    yeni(true);
    (void)emn(0u, 1u, (uint8_t)(HAZIR | IDA_TUBE_PERMIT_OPEN), HAZIR);
    TEST_ASSERT_EQUAL(IDA_RC_OK, cagir(true));
    TEST_ASSERT_EQUAL_UINT8(IDA_FCS_FAULT_LATCHED, OUT.fc.state);
    /* Neden sürerken FC_RESET kabul edilir ama aynı çağrıda yeniden kilitlenir; FC_STATE
       günlüğü yok (G-Ç27). */
    yeni(true);
    (void)emn(0u, 1u, (uint8_t)(HAZIR | IDA_TUBE_PERMIT_OPEN), HAZIR);
    uint32_t id = fc((uint8_t)IDA_FC_RESET, 0u);
    TEST_ASSERT_EQUAL(IDA_RC_OK, cagir(true));
    TEST_ASSERT_EQUAL_UINT8(IDA_ACK_ACCEPTED, sonuc(id)->ack);
    TEST_ASSERT_EQUAL_UINT8(IDA_FCS_FAULT_LATCHED, OUT.fc.state);
    TEST_ASSERT_EQUAL_UINT32(0u, ds_gunluk_say(&OUT, (uint16_t)IDA_LOG_FC_STATE));
    /* Neden kalkınca FC_RESET SAFE yapar (günlük INFO). */
    yeni(true);
    (void)emn(0u, 1u, HAZIR, HAZIR);
    id = fc((uint8_t)IDA_FC_RESET, 0u);
    TEST_ASSERT_EQUAL(IDA_RC_OK, cagir(true));
    TEST_ASSERT_EQUAL_UINT8(IDA_FCS_SAFE, OUT.fc.state);
    TEST_ASSERT_EQUAL_UINT32(1u, ds_gunluk_say(&OUT, (uint16_t)IDA_LOG_FC_STATE));
    TEST_ASSERT_FALSE(OUT.armed);
    /* Oturum yokken FC_RESET → SESSION. */
    kilitle();
    yeni(true);
    IN.plat.link_state = (uint8_t)IDA_LINK_NONE;
    id = fc((uint8_t)IDA_FC_RESET, 0u);
    TEST_ASSERT_EQUAL(IDA_RC_OK, cagir(true));
    ret_bekle(id, (uint16_t)IDA_REASON_SESSION);
    TEST_ASSERT_EQUAL_UINT8(IDA_FCS_FAULT_LATCHED, OUT.fc.state);
}

/* ---- G-Ç26: iç tutarlılık ------------------------------------------------------------------ */

static void test_gc26_butunluk(void)
{
    /* k = 4, 5 (sol bulgu 3): çağrı bayraklarının (reset_kabul, bu_cagri_kilit) ham baytı
       bozuk; FC'nin bütünlük denetimi onları sıfırlamadan önce yapılır, bozulma onarım,
       FAULT_LATCHED ve SAFETY_CLOSE(ALL)'a taşınır. */
    for (uint32_t k = 0u; k < 6u; k++) {
        hazirla();
        yeni(true);
        TEST_ASSERT_EQUAL(IDA_RC_OK, cagir(true));
        if (k == 0u) {
            C.fc.ic = 0u; /* "hep 0" geçerli değil */
        } else if (k == 1u) {
            C.fc.ic_tumleyen ^= 1u;
        } else if (k == 2u) {
            uint8_t bozuk = 3u;
            (void)memcpy(&C.fc.istek_var, &bozuk, 1u);
        } else if (k == 3u) {
            C.fc.tup = 0u; /* SAFE'te silahlı tüp tutarsız */
            C.fc.tup_tumleyen = 0xFFu;
        } else {
            uint8_t bozuk = 3u;
            (void)memcpy((k == 4u) ? &C.fc.reset_kabul : &C.fc.bu_cagri_kilit, &bozuk, 1u);
        }
        yeni(true);
        TEST_ASSERT_EQUAL(IDA_RC_OK, cagir(true));
        TEST_ASSERT_FALSE(OUT.health.core_ok);
        TEST_ASSERT_EQUAL_UINT32(1u, ds_gunluk_say(&OUT, (uint16_t)IDA_LOG_CORE_INTEGRITY));
        TEST_ASSERT_EQUAL_UINT8(IDA_FCS_FAULT_LATCHED, OUT.fc.state);
        TEST_ASSERT_EQUAL_UINT32(1u, kapanma((uint8_t)IDA_TUBE_ALL));
        /* core_ok mandallı, günlük bir kez; FC_RESET kabul edilir ama neden sürer. */
        yeni(true);
        const uint32_t id = fc((uint8_t)IDA_FC_RESET, 0u);
        TEST_ASSERT_EQUAL(IDA_RC_OK, cagir(true));
        TEST_ASSERT_FALSE(OUT.health.core_ok);
        TEST_ASSERT_EQUAL_UINT32(0u, ds_gunluk_say(&OUT, (uint16_t)IDA_LOG_CORE_INTEGRITY));
        TEST_ASSERT_EQUAL_UINT8(IDA_ACK_ACCEPTED, sonuc(id)->ack);
        TEST_ASSERT_EQUAL_UINT8(IDA_FCS_FAULT_LATCHED, OUT.fc.state);
    }
    /* Dış nedenler core_ok'u düşürmez: bayat IMU, oturum yok. */
    hazirla();
    for (uint32_t i = 0u; i < 10u; i++) {
        yeni(false);
        IN.plat.link_state = (uint8_t)IDA_LINK_NONE;
        TEST_ASSERT_EQUAL(IDA_RC_OK, cagir(false));
        TEST_ASSERT_TRUE(OUT.health.core_ok);
    }
}

/* ---- G-Ç27: FC_STATE ve FC_INHIBIT günlükleri ---------------------------------------------- */

static void test_gc27_gunlukler(void)
{
    hazirla();
    yeni(true);
    TEST_ASSERT_EQUAL(IDA_RC_OK, cagir(true));
    TEST_ASSERT_EQUAL_UINT32(0u, ds_gunluk_say(&OUT, (uint16_t)IDA_LOG_FC_STATE)); /* taban */
    TEST_ASSERT_EQUAL_UINT32(0u, ds_gunluk_say(&OUT, (uint16_t)IDA_LOG_FC_INHIBIT));
    gevset();
    TEST_ASSERT_EQUAL_UINT32(1u, ds_gunluk_say(&OUT, (uint16_t)IDA_LOG_FC_INHIBIT));
    G_KAPAT_YANIT = false;
    yeni(true);
    (void)emn(0u, 1u, HAZIR, HAZIR);
    TEST_ASSERT_EQUAL(IDA_RC_OK, cagir(true));
    bool goruldu = false;
    for (uint32_t i = 0u; (i < 40u) && !goruldu; i++) {
        yeni(true);
        (void)emn(0u, 1u, HAZIR, HAZIR);
        TEST_ASSERT_EQUAL(IDA_RC_OK, cagir(true));
        for (uint32_t j = 0u; j < (uint32_t)OUT.n_logs; j++) {
            if (OUT.logs[j].code == (uint16_t)IDA_LOG_FC_STATE) {
                goruldu = true;
                TEST_ASSERT_EQUAL_INT32(IDA_FCS_FAULT_LATCHED, OUT.logs[j].arg0);
                TEST_ASSERT_EQUAL_INT32(IDA_FCS_SAFE, OUT.logs[j].arg1);
                TEST_ASSERT_EQUAL_UINT8(IDA_SEV_ERROR, OUT.logs[j].severity);
            }
        }
    }
    TEST_ASSERT_TRUE(goruldu);
    /* Değişmeyen çağrıda yok. */
    yeni(true);
    (void)emn(0u, 1u, HAZIR, HAZIR);
    TEST_ASSERT_EQUAL(IDA_RC_OK, cagir(true));
    TEST_ASSERT_EQUAL_UINT32(0u, ds_gunluk_say(&OUT, (uint16_t)IDA_LOG_FC_STATE));
}

/* ---- G-Ç28, G-Ç39: parametreler ----------------------------------------------------------- */

static uint16_t param_id(const char *ad)
{
    uint16_t id = 0u;
    TEST_ASSERT_EQUAL(IDA_RC_OK, ida_core_param_find(ad, &id));
    return id;
}

static void param_set(uint32_t cmd_id, uint16_t id, uint32_t deger)
{
    ida_cmd_t *k = &IN.cmds[IN.n_cmds];
    IN.n_cmds++;
    (void)memset(k, 0, sizeof(*k));
    k->cmd_id = cmd_id;
    k->type = (uint8_t)IDA_CMD_PARAM_SET;
    k->u.param.id = id;
    k->u.param.type = (uint8_t)IDA_PT_U32;
    k->u.param.v.u32 = deger;
}

static void test_gc28_gc39_parametreler(void)
{
    const char *const ad[] = {"T_emn_bayat", "T_pencere", "T_hb_fc", "FC_APPLY_BY_NS", "FC_HOLD_NS"};
    for (uint32_t i = 0u; i < 5u; i++) {
        ida_param_desc_t d;
        uint16_t indis = 0u;
        for (uint16_t j = 0u; j < ida_core_param_count(); j++) {
            TEST_ASSERT_EQUAL(IDA_RC_OK, ida_core_param_desc(j, &d));
            if (d.id == param_id(ad[i])) {
                indis = j;
            }
        }
        TEST_ASSERT_EQUAL(IDA_RC_OK, ida_core_param_desc(indis, &d));
        TEST_ASSERT_EQUAL_UINT8(IDA_PCLS_B, d.cls);
        hazirla();
        yeni(true);
        TEST_ASSERT_EQUAL(IDA_RC_OK, cagir(true));
        C.emn.armed = true; /* test kancası: silahlı */
        yeni(true);
        param_set(100u + i, d.id, d.def.v.u32);
        TEST_ASSERT_EQUAL(IDA_RC_OK, cagir(true));
        ret_bekle(100u + i, (uint16_t)IDA_REASON_ARMED);
    }
    /* Silahsızken bekleyen B değeri, ARM ile aynı çağrıda commit edilemez. */
    hazirla();
    yeni(true);
    param_set(200u, param_id("T_emn_bayat"), 300u);
    TEST_ASSERT_EQUAL(IDA_RC_OK, cagir(true));
    TEST_ASSERT_EQUAL_UINT8(IDA_ACK_ACCEPTED, sonuc(200u)->ack);
    yeni(true);
    ida_cmd_t *k = &IN.cmds[0];
    IN.n_cmds = 2u;
    (void)memset(&IN.cmds[0], 0, 2u * sizeof(IN.cmds[0]));
    k->cmd_id = 201u;
    k->type = (uint8_t)IDA_CMD_ARM;
    IN.cmds[1].cmd_id = 202u;
    IN.cmds[1].type = (uint8_t)IDA_CMD_PARAM_COMMIT;
    TEST_ASSERT_EQUAL(IDA_RC_OK, cagir(true));
    ret_bekle(202u, (uint16_t)IDA_REASON_ARMED);
    /* G-Ç39: aralık dışı apply_by ve hold reddedilir; sınırlar kabul. */
    const struct {
        const char *ad;
        uint32_t v;
        bool kabul;
    } a[] = {{"FC_APPLY_BY_NS", 15999999u, false}, {"FC_APPLY_BY_NS", 16000000u, true},
             {"FC_APPLY_BY_NS", 1000000000u, true}, {"FC_APPLY_BY_NS", 1000000001u, false},
             {"FC_HOLD_NS", 59999999u, false},      {"FC_HOLD_NS", 60000000u, true},
             {"FC_HOLD_NS", 450000000u, true},      {"FC_HOLD_NS", 450000001u, false}};
    for (uint32_t i = 0u; i < (uint32_t)(sizeof(a) / sizeof(a[0])); i++) {
        hazirla();
        yeni(true);
        param_set(300u + i, param_id(a[i].ad), a[i].v);
        TEST_ASSERT_EQUAL(IDA_RC_OK, cagir(true));
        if (a[i].kabul) {
            TEST_ASSERT_EQUAL_UINT8(IDA_ACK_ACCEPTED, sonuc(300u + i)->ack);
        } else {
            ret_bekle(300u + i, (uint16_t)IDA_REASON_PARAM_INVALID);
        }
    }
}

/* ---- G-Ç29: emniyet örneğinin bayatlığı ----------------------------------------------------- */

static void test_gc29_bayatlik(void)
{
    /* (i) örnek akışı kesilir. */
    hazirla();
    gevset();
    const int64_t son = T;
    bool basildi = false;
    while (!basildi) {
        yeni(true);
        TEST_ASSERT_EQUAL(IDA_RC_OK, cagir(true));
        basildi = (OUT.fc.inhibit_mask & IDA_FCI_ESTOP) != 0u;
        if (basildi) {
            TEST_ASSERT_TRUE((T - son) > (250 * DS_MS));
            TEST_ASSERT_TRUE((T - son) <= (255 * DS_MS));
            TEST_ASSERT_EQUAL_UINT32(1u, kapanma((uint8_t)IDA_TUBE_ALL));
            TEST_ASSERT_TRUE(OUT.health.narrowed);
            TEST_ASSERT_EQUAL_UINT32(1u, ds_gunluk_say(&OUT, (uint16_t)IDA_LOG_SAFETY_INPUT));
        }
    }
    /* Acil durdurma zaten bayatlıktan kısıtlayanken gelen taze gevşek örnek izleri tazeler ve
       aynı çağrıda gevşetir (A4-K116: gevşetme kabul edilen örnekle; bayatlık örnek içeriği
       değildir). */
    yeni(true);
    (void)emn(0u, 0u, HAZIR, HAZIR);
    TEST_ASSERT_EQUAL(IDA_RC_OK, cagir(true));
    TEST_ASSERT_EQUAL_UINT32(0u, OUT.fc.inhibit_mask & IDA_FCI_ESTOP);
    /* Bayatlığın kısıtlayana döndürdüğü çağrıda gelen taze örnek gevşetmez (kısıtlama
       hâkimdir: kapanma ve daraltma bu çağrıdadır); sonraki çağrıdaki taze örnek gevşetir. */
    const int64_t son2 = T;
    while (((T + (5 * DS_MS)) - son2) <= (250 * DS_MS)) { /* 250 ms'ye dek taze */
        yeni(true);
        TEST_ASSERT_EQUAL(IDA_RC_OK, cagir(true));
        TEST_ASSERT_EQUAL_UINT32(0u, OUT.fc.inhibit_mask & IDA_FCI_ESTOP);
    }
    yeni(true);
    (void)emn(0u, 0u, HAZIR, HAZIR);
    TEST_ASSERT_EQUAL(IDA_RC_OK, cagir(true));
    TEST_ASSERT_TRUE((OUT.fc.inhibit_mask & IDA_FCI_ESTOP) != 0u);
    TEST_ASSERT_EQUAL_UINT32(1u, kapanma((uint8_t)IDA_TUBE_ALL));
    TEST_ASSERT_TRUE(OUT.health.narrowed);
    yeni(true);
    (void)emn(0u, 0u, HAZIR, HAZIR);
    TEST_ASSERT_EQUAL(IDA_RC_OK, cagir(true));
    TEST_ASSERT_EQUAL_UINT32(0u, OUT.fc.inhibit_mask & IDA_FCI_ESTOP);
    /* (ii) t_meas donar; seq artar, t_rx yenilenir (A4-K34 ile kabul). */
    hazirla();
    gevset();
    const int64_t dondu = T;
    basildi = false;
    while (!basildi) {
        yeni(true);
        ida_sample_t *s = emn(0u, 0u, HAZIR, HAZIR);
        s->hdr.t_meas.ns = dondu;
        const uint32_t red = OUT.status.cnt.sample_rejected;
        TEST_ASSERT_EQUAL(IDA_RC_OK, cagir(true));
        TEST_ASSERT_EQUAL_UINT32(red, OUT.status.cnt.sample_rejected); /* kabul edildi */
        basildi = (OUT.fc.inhibit_mask & IDA_FCI_ESTOP) != 0u;
        if (basildi) {
            TEST_ASSERT_TRUE((T - dondu) > (250 * DS_MS));
            TEST_ASSERT_TRUE((T - dondu) <= (255 * DS_MS));
            TEST_ASSERT_EQUAL_UINT32(1u, kapanma((uint8_t)IDA_TUBE_ALL));
        }
    }
    /* Ölçüm zamanı donmuşken gevşeten örnek acil durdurmayı gevşetmez. */
    for (uint32_t i = 0u; i < 5u; i++) {
        yeni(true);
        ida_sample_t *s = emn(0u, 0u, HAZIR, HAZIR);
        s->hdr.t_meas.ns = dondu;
        TEST_ASSERT_EQUAL(IDA_RC_OK, cagir(true));
        TEST_ASSERT_TRUE((OUT.fc.inhibit_mask & IDA_FCI_ESTOP) != 0u);
        TEST_ASSERT_EQUAL_UINT8(0u, OUT.n_events); /* zaten kısıtlayan: yeni olay yok */
    }
    /* Olağan 100 ms aralıklı akış bayatlamaz. */
    hazirla();
    gevset();
    for (uint32_t i = 0u; i < 400u; i++) {
        yeni(true);
        if ((i % 20u) == 19u) {
            (void)emn(0u, 0u, HAZIR, HAZIR);
        }
        TEST_ASSERT_EQUAL(IDA_RC_OK, cagir(true));
        TEST_ASSERT_EQUAL_UINT32(0u, OUT.fc.inhibit_mask & IDA_FCI_ESTOP);
    }
}

/* G-Ç29, W5-K4 eki (sol bulgu 1): kabul edilen örnek izleri bayatlatırsa (geri giden t_rx)
   acil durdurma AYNI çağrıda kısıtlayana döner: ESTOP, SAFETY_CLOSE(ALL), daraltma, günlük.
   Dizi sol'unkidir: now_w 2000 ms'de t_rx 2000 ms, t_meas 100 ms gevşek örnek; now_w 2015
   ms'de t_meas 101 ms, t_rx 1700 ms. */
static void test_gc29_bayatlik_ayni_cagri(void)
{
    hazirla();
    T = 1995 * DS_MS;
    yeni(true); /* now_w 2000 ms */
    ida_sample_t *s = emn(0u, 0u, HAZIR, HAZIR);
    s->hdr.t_meas.ns = 100 * DS_MS;
    s->hdr.t_rx.ns = 2000 * DS_MS;
    TEST_ASSERT_EQUAL(IDA_RC_OK, cagir(true));
    TEST_ASSERT_EQUAL_HEX32(0u, OUT.fc.inhibit_mask & EMN_BITLERI);
    for (uint32_t i = 0u; i < 2u; i++) {
        yeni(true); /* 2005, 2010 ms: örnek yok, iz 10 ms yaşında */
        TEST_ASSERT_EQUAL(IDA_RC_OK, cagir(true));
        TEST_ASSERT_EQUAL_UINT32(0u, OUT.fc.inhibit_mask & IDA_FCI_ESTOP);
        TEST_ASSERT_EQUAL_UINT8(0u, OUT.n_events);
    }
    yeni(true); /* 2015 ms */
    TEST_ASSERT_EQUAL_INT64(2015 * DS_MS, T);
    s = emn(0u, 0u, HAZIR, HAZIR);
    s->hdr.t_meas.ns = 101 * DS_MS;
    s->hdr.t_rx.ns = 1700 * DS_MS;
    const uint32_t red = OUT.status.cnt.sample_rejected;
    TEST_ASSERT_EQUAL(IDA_RC_OK, cagir(true));
    TEST_ASSERT_EQUAL_UINT32(red, OUT.status.cnt.sample_rejected); /* örnek kabul edildi */
    TEST_ASSERT_TRUE((OUT.fc.inhibit_mask & IDA_FCI_ESTOP) != 0u);
    TEST_ASSERT_EQUAL_UINT32(1u, kapanma((uint8_t)IDA_TUBE_ALL));
    TEST_ASSERT_TRUE(OUT.health.narrowed);
    TEST_ASSERT_EQUAL_UINT32(1u, ds_gunluk_say(&OUT, (uint16_t)IDA_LOG_SAFETY_INPUT));
    TEST_ASSERT_EQUAL_UINT8(IDA_FCS_SAFE, OUT.fc.state);
    /* Donuk t_meas'li akıştan sonra (ölçüt (ii) dolmadan) ölçüm zamanı ilerleyen ama t_rx'i
       eski örnek: çağrı başında iz taze, örnekten sonra iki ölçüt de bayat. */
    hazirla();
    gevset();
    const int64_t t0 = T;
    s = NULL;
    while ((T - t0) < (240 * DS_MS)) {
        yeni(true);
        s = emn(0u, 0u, HAZIR, HAZIR);
        s->hdr.t_meas.ns = t0; /* donuk (A4-K34 ile kabul; (i)'yi yeniler) */
        TEST_ASSERT_EQUAL(IDA_RC_OK, cagir(true));
        TEST_ASSERT_EQUAL_UINT32(0u, OUT.fc.inhibit_mask & IDA_FCI_ESTOP);
    }
    yeni(true);
    s = emn(0u, 0u, HAZIR, HAZIR);
    s->hdr.t_meas.ns = T;
    s->hdr.t_rx.ns = T - (260 * DS_MS);
    TEST_ASSERT_EQUAL(IDA_RC_OK, cagir(true));
    TEST_ASSERT_TRUE((OUT.fc.inhibit_mask & IDA_FCI_ESTOP) != 0u);
    TEST_ASSERT_EQUAL_UINT32(1u, kapanma((uint8_t)IDA_TUBE_ALL));
}

/* G-Ç03, A4-K118 (W5-K21): DET_STALE init'ten, oturum sonu çağrısından ve SESSION_START'tan
   T_det_stale (varsayılan 2000 ms) dolmadan kurulmaz; dolduğu çağrıda kurulur. */
static void test_gc03_det_stale(void)
{
    hazirla(); /* init now_w = 1000 ms; T = 2000 ms */
    bool kuruldu = false;
    while (!kuruldu) {
        yeni(true);
        TEST_ASSERT_EQUAL(IDA_RC_OK, cagir(true));
        kuruldu = (OUT.fc.inhibit_mask & IDA_FCI_DET_STALE) != 0u;
        TEST_ASSERT_TRUE(kuruldu == ((T - (1000 * DS_MS)) > (2000 * DS_MS)));
    }
    /* SESSION_START: taban yenilenir, bit aynı çağrıda kalkar. */
    yeni(true);
    IN.plat.link_event = (uint8_t)IDA_LINKEV_SESSION_START;
    TEST_ASSERT_EQUAL(IDA_RC_OK, cagir(true));
    TEST_ASSERT_EQUAL_UINT32(0u, OUT.fc.inhibit_mask & IDA_FCI_DET_STALE);
    const int64_t bas = T;
    while ((T - bas) < (2000 * DS_MS)) {
        yeni(true);
        TEST_ASSERT_EQUAL(IDA_RC_OK, cagir(true));
        TEST_ASSERT_EQUAL_UINT32(0u, OUT.fc.inhibit_mask & IDA_FCI_DET_STALE);
    }
    yeni(true);
    TEST_ASSERT_EQUAL(IDA_RC_OK, cagir(true));
    TEST_ASSERT_TRUE((OUT.fc.inhibit_mask & IDA_FCI_DET_STALE) != 0u);
    /* Oturum sonu çağrısı da tabanı sıfırlar (A4-K122). */
    yeni(false);
    IN.tf.present = false;
    IN.plat.link_event = (uint8_t)IDA_LINKEV_SESSION_END;
    IN.plat.link_state = (uint8_t)IDA_LINK_NONE;
    TEST_ASSERT_EQUAL(IDA_RC_OK, cagir(false));
    TEST_ASSERT_EQUAL_UINT32(0u, OUT.fc.inhibit_mask & IDA_FCI_DET_STALE);
}

/* ---- G-Ç36, G-Ç37: olay kimliği ------------------------------------------------------------- */

static void test_gc36_gc37_olay_kimligi(void)
{
    ida_init_in_t ii;
    ds_init_girdisi(&ii);
    ii.boot_count_ok = false;
    TEST_ASSERT_EQUAL(IDA_RC_PARAM_DEFAULTS, ida_core_init(&C, &ii));
    T = 2000 * DS_MS;
    SEQ = 0u;
    G_KAPAT_N = 0u;
    yeni(false);
    IN.tf.present = false;
    IN.plat.link_event = (uint8_t)IDA_LINKEV_SESSION_END;
    TEST_ASSERT_EQUAL(IDA_RC_OK, cagir(false));
    TEST_ASSERT_EQUAL_UINT8(0u, OUT.n_events);
    TEST_ASSERT_FALSE(OUT.status.events_ok);
    TEST_ASSERT_TRUE((OUT.fc.inhibit_mask & IDA_FCI_EVENTS_OFF) != 0u);
    yeni(true);
    const uint32_t id = fc((uint8_t)IDA_FC_ARM, 0u);
    TEST_ASSERT_EQUAL(IDA_RC_OK, cagir(true));
    ret_bekle(id, (uint16_t)IDA_REASON_EVENTS_OFF);
    /* G-Ç37: ctr 0xFFFFFFF0'a ulaştı: yalnız SAFETY_CLOSE, events_ok = 0; 0xFFFFFFFF'ten sonra
       olay yok. */
    hazirla();
    gevset();
    C.olay.ctr = 0xFFFFFFEFu;
    yeni(true);
    (void)emn(0u, 1u, HAZIR, HAZIR);
    TEST_ASSERT_EQUAL(IDA_RC_OK, cagir(true));
    TEST_ASSERT_EQUAL_UINT32(0xFFFFFFEFu, (uint32_t)(OUT.events[0].event_id & 0xFFFFFFFFu));
    TEST_ASSERT_FALSE(OUT.status.events_ok);
    for (uint32_t i = 0u; i < 40u; i++) {
        yeni(true);
        (void)emn(0u, (uint8_t)(i & 1u), HAZIR, HAZIR);
        TEST_ASSERT_EQUAL(IDA_RC_OK, cagir(true));
        for (uint32_t j = 0u; j < (uint32_t)OUT.n_events; j++) {
            TEST_ASSERT_EQUAL_UINT8(IDA_EVT_SAFETY_CLOSE, OUT.events[j].kind);
        }
    }
    TEST_ASSERT_TRUE(C.olay.tukendi);
    yeni(true);
    (void)emn(0u, 1u, HAZIR, HAZIR);
    TEST_ASSERT_EQUAL(IDA_RC_OK, cagir(true));
    yeni(true);
    (void)emn(0u, 0u, HAZIR, HAZIR);
    TEST_ASSERT_EQUAL(IDA_RC_OK, cagir(true));
    yeni(true);
    (void)emn(0u, 1u, HAZIR, HAZIR);
    TEST_ASSERT_EQUAL(IDA_RC_OK, cagir(true));
    TEST_ASSERT_EQUAL_UINT8(0u, OUT.n_events);
}

/* ---- G-Ç38, G-Ç41: olay kümesinin sınırları (çıkış kapısı; aday kancası) ------------------ */

static void aday(uint8_t kind, uint8_t tube)
{
    ida_event_t *a = &C.olay.aday[C.olay.n_aday];
    (void)memset(a, 0, sizeof(*a));
    a->kind = kind;
    a->tube = tube;
    a->hold_ns = 200000000u;
    C.olay.n_aday++;
}

static void test_gc38_gc41_olay_kumesi(void)
{
    /* G-Ç38: iki SAFETY_CLOSE ve dört aday: yalnız iki aday sığar, sığmayanlar için sayaç
       tüketilmez; SAFETY_CLOSE düşmez. (Port yolunda atış kontrol aday üretmez; aday kanca.) */
    hazirla();
    yeni(true);
    TEST_ASSERT_EQUAL(IDA_RC_OK, cagir(true));
    C.emn.kisitli = 0u;
    C.emn.armed = true;
    const uint32_t ctr0 = C.olay.ctr;
    ida_olay_kapat_iste(&C.olay, (uint8_t)(IDA_KAPAT_T0 | IDA_KAPAT_T1));
    aday((uint8_t)IDA_EVT_SAFETY_OPEN, 0u);
    aday((uint8_t)IDA_EVT_SAFETY_OPEN, 1u);
    aday((uint8_t)IDA_EVT_SAFETY_OPEN, 0u);
    aday((uint8_t)IDA_EVT_SAFETY_OPEN, 1u);
    yeni(true);
    TEST_ASSERT_EQUAL(IDA_RC_OK, ida_core_step(&C, &IN, &OUT));
    TEST_ASSERT_EQUAL_UINT8(4u, OUT.n_events);
    TEST_ASSERT_EQUAL_UINT8(IDA_EVT_SAFETY_CLOSE, OUT.events[0].kind);
    TEST_ASSERT_EQUAL_UINT8(IDA_EVT_SAFETY_CLOSE, OUT.events[1].kind);
    TEST_ASSERT_EQUAL_UINT32(ctr0 + 4u, C.olay.ctr);
    /* G-Ç41: bekleyen izin açan olay ve FIRE sayısı 7 iken yenisi yok; SAFETY_CLOSE yine
       üretilir. */
    hazirla();
    yeni(true);
    TEST_ASSERT_EQUAL(IDA_RC_OK, cagir(true));
    C.emn.kisitli = 0u;
    C.emn.armed = true;
    for (uint32_t i = 0u; i < 7u; i++) {
        ida_olay_bekleyen_t *b = &C.olay.bekleyen[i];
        b->dolu = true;
        b->kind = (uint8_t)IDA_EVT_FIRE;
        b->tube = 0xEEu; /* tüp dışı: A4-K123 değil, yalnız A4-K129 engellesin */
        b->event_id = 0x700000000ull + i;
        b->cikis_now.ns = T + 10000 * DS_MS;
        b->apply_by_ns = 20000000u;
    }
    aday((uint8_t)IDA_EVT_SAFETY_OPEN, 0u);
    ida_olay_kapat_iste(&C.olay, (uint8_t)IDA_KAPAT_T1);
    yeni(true);
    TEST_ASSERT_EQUAL(IDA_RC_OK, ida_core_step(&C, &IN, &OUT));
    TEST_ASSERT_EQUAL_UINT8(1u, OUT.n_events);
    TEST_ASSERT_EQUAL_UINT8(IDA_EVT_SAFETY_CLOSE, OUT.events[0].kind);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_gc01_init_ve_yeniden_init);
    RUN_TEST(test_gc31_kurulmamis);
    RUN_TEST(test_gc03_engel_bitleri);
    RUN_TEST(test_gc04_aralik_disi);
    RUN_TEST(test_gc32_daraltici_cmds);
    RUN_TEST(test_gc33_gc34_daraltmada_genisletici);
    RUN_TEST(test_gc22_port_satirlari);
    RUN_TEST(test_gc06_gc15_kapanma);
    RUN_TEST(test_gc14_gevsetme);
    RUN_TEST(test_gc18_gc44_gc45_oturum);
    RUN_TEST(test_gc18_gc45_baglanti_olayinda_gevsetme_yok);
    RUN_TEST(test_gc50_gc42_kapatma_sonucu);
    RUN_TEST(test_gc19_port_kilit_korunur);
    RUN_TEST(test_gc53_gc20_celiski_ve_reset);
    RUN_TEST(test_gc26_butunluk);
    RUN_TEST(test_gc27_gunlukler);
    RUN_TEST(test_gc28_gc39_parametreler);
    RUN_TEST(test_gc29_bayatlik);
    RUN_TEST(test_gc29_bayatlik_ayni_cagri);
    RUN_TEST(test_gc03_det_stale);
    RUN_TEST(test_gc36_gc37_olay_kimligi);
    RUN_TEST(test_gc38_gc41_olay_kumesi);
    return UNITY_END();
}
