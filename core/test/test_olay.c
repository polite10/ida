/*
 * test_olay.c — olay kimliği, sonuçları, zaman aşımı, bekleyen kümesi ve yalnız silen çıkış
 * kapısı (A4-K60–K66, K69, K70, K123, K125, K129). Genişletici adaylar W4'ün dosyasından
 * gelir; kapı burada adaylar test kaynağından konarak sınanır (çekirdekte üreten yol yok).
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

static ida_rc_t cagri(bool step)
{
    g_seq++;
    g_t += 5 * DS_MS;
    IN.now_w.ns = g_t;
    IN.tf.present = true;
    IN.tf.seq = g_seq;
    IN.tf.t.ns = g_t;
    IN.tf.t_rx.ns = g_t;
    IN.imu_present = step;
    if (step) {
        ds_imu(&IN.imu, g_t, g_seq);
    }
    const ida_rc_t rc = step ? ida_core_step(&C, &IN, &OUT) : ida_core_tick(&C, &IN, &OUT);
    ds_cikti_degismezleri(rc, &IN, &OUT);
    return rc;
}

static void oturum_sonu(void)
{
    if (C.emn.oturum_kapali) {
        /* A4-K73: arada SESSION_START olmadan ikinci SESSION_END olay üretmez. */
        IN.plat.link_event = (uint8_t)IDA_LINKEV_SESSION_START;
        (void)cagri(false);
    }
    IN.plat.link_event = (uint8_t)IDA_LINKEV_SESSION_END;
    (void)cagri(false);
    IN.plat.link_event = (uint8_t)IDA_LINKEV_NONE;
}

static void test_k61_kimlik_ve_k63(void)
{
    hazirla();
    oturum_sonu();
    TEST_ASSERT_EQUAL_UINT8(1u, OUT.n_events);
    TEST_ASSERT_TRUE(OUT.events[0].event_id == ((7ull << 32) | 1ull));
    TEST_ASSERT_EQUAL_UINT32(IDA_APPLY_BY_MIN_NS + 4000000u, OUT.events[0].apply_by_ns); /* 20 ms */
    TEST_ASSERT_TRUE(OUT.fc.last_event_id == OUT.events[0].event_id);
    TEST_ASSERT_EQUAL_UINT8(0u, OUT.fc.last_event_result);
    oturum_sonu();
    TEST_ASSERT_TRUE(OUT.events[0].event_id == ((7ull << 32) | 2ull));
    (void)cagri(false);
    TEST_ASSERT_EQUAL_UINT8(0u, OUT.n_events); /* aynı kimlik bir daha çıkmaz */
}

static void test_k61_olaysiz_calisma(void)
{
    ida_init_in_t ii;
    ds_init_girdisi(&ii);
    ii.boot_count = 0u; /* aralık dışı */
    TEST_ASSERT_EQUAL(IDA_RC_PARAM_DEFAULTS, ida_core_init(&C, &ii));
    g_seq = 0u;
    g_t = 2000 * DS_MS;
    ds_girdi(&IN, g_t, 0u, true);
    oturum_sonu();
    TEST_ASSERT_EQUAL_UINT8(0u, OUT.n_events);
    TEST_ASSERT_FALSE(OUT.status.events_ok);
    TEST_ASSERT_TRUE((OUT.fc.inhibit_mask & IDA_FCI_EVENTS_OFF) != 0u);
}

static void test_k62_sayac_siniri(void)
{
    hazirla();
    C.olay.ctr = IDA_EVT_CTR_CLOSE_ONLY; /* test kancası */
    (void)cagri(true);
    TEST_ASSERT_FALSE(OUT.status.events_ok);
    TEST_ASSERT_TRUE((OUT.fc.inhibit_mask & IDA_FCI_EVENTS_OFF) != 0u);
    oturum_sonu(); /* yalnız SAFETY_CLOSE üretilir */
    TEST_ASSERT_EQUAL_UINT8(1u, OUT.n_events);
    TEST_ASSERT_EQUAL_HEX32(IDA_EVT_CTR_CLOSE_ONLY, (uint32_t)OUT.events[0].event_id);
    C.olay.ctr = IDA_EVT_CTR_LAST;
    oturum_sonu();
    TEST_ASSERT_EQUAL_UINT8(1u, OUT.n_events);
    TEST_ASSERT_EQUAL_HEX32(IDA_EVT_CTR_LAST, (uint32_t)OUT.events[0].event_id);
    oturum_sonu(); /* 0xFFFFFFFF kullanıldı: olay yok, sarmaz */
    TEST_ASSERT_EQUAL_UINT8(0u, OUT.n_events);
}

static void test_k69_sonuc_ve_k70_zaman_asimi(void)
{
    hazirla();
    oturum_sonu();
    const uint64_t id = OUT.events[0].event_id;
    IN.n_evres = 2u;
    IN.evres[0].event_id = id;
    IN.evres[0].result = (uint8_t)IDA_EVR_APPLIED;
    IN.evres[1].event_id = id + 77u; /* yabancı */
    IN.evres[1].result = (uint8_t)IDA_EVR_APPLIED;
    (void)cagri(true);
    TEST_ASSERT_EQUAL_UINT32(1u, OUT.status.cnt.evres_ignored);
    TEST_ASSERT_EQUAL_UINT32(1u, ds_gunluk_say(&OUT, (uint16_t)IDA_LOG_EVENT_RESULT));
    TEST_ASSERT_EQUAL_UINT8(IDA_EVR_APPLIED, OUT.fc.last_event_result);
    /* aynı sonuç ikinci kez: artık bekleyen değil */
    IN.n_evres = 1u;
    (void)cagri(true);
    TEST_ASSERT_EQUAL_UINT32(2u, OUT.status.cnt.evres_ignored);
    /* yanıtsız olay: apply_by (20 ms) + 100 ms sonra sonucu bilinmeyen */
    IN.n_evres = 0u;
    oturum_sonu();
    for (uint32_t i = 0u; i < 24u; i++) { /* 120 ms: sınıra eşit, aşılmadı */
        (void)cagri(true);
        TEST_ASSERT_EQUAL_UINT32(0u, OUT.status.cnt.event_unknown);
    }
    (void)cagri(true);
    TEST_ASSERT_EQUAL_UINT32(1u, OUT.status.cnt.event_unknown);
    TEST_ASSERT_EQUAL_UINT32(1u, ds_gunluk_say(&OUT, (uint16_t)IDA_LOG_EVENT_UNKNOWN));
}


/* A4-K69 (B20): bekleyen olayın kimliğiyle tanımsız sonuç kodu yok sayılır; olay beklemede
   kalır, last_event_result değişmez. */
static void test_k69_tanimsiz_sonuc_kodu(void)
{
    hazirla();
    oturum_sonu();
    const uint64_t id = OUT.events[0].event_id;
    IN.n_evres = 1u;
    IN.evres[0].event_id = id;
    IN.evres[0].result = 198u;
    (void)cagri(true);
    TEST_ASSERT_EQUAL_UINT32(1u, OUT.status.cnt.evres_ignored);
    TEST_ASSERT_EQUAL_UINT8(0u, OUT.fc.last_event_result);
    TEST_ASSERT_EQUAL_UINT32(0u, ds_gunluk_say(&OUT, (uint16_t)IDA_LOG_EVENT_RESULT));
    const uint8_t tanimsiz[] = {0u, 7u, 15u, 20u, 255u};
    for (uint32_t i = 0u; i < (uint32_t)sizeof(tanimsiz); i++) {
        IN.evres[0].result = tanimsiz[i];
        (void)cagri(true);
    }
    TEST_ASSERT_EQUAL_UINT32(6u, OUT.status.cnt.evres_ignored);
    /* olay hâlâ bekliyor: tanımlı sonuç kabul edilir */
    IN.evres[0].result = (uint8_t)IDA_EVR_SESSION_END;
    (void)cagri(true);
    TEST_ASSERT_EQUAL_UINT32(6u, OUT.status.cnt.evres_ignored);
    TEST_ASSERT_EQUAL_UINT8(IDA_EVR_SESSION_END, OUT.fc.last_event_result);
    TEST_ASSERT_EQUAL_UINT32(1u, ds_gunluk_say(&OUT, (uint16_t)IDA_LOG_EVENT_RESULT));
}

static void test_k129_bekleyen_kumesi(void)
{
    hazirla();
    uint64_t son = 0u;
    for (uint32_t i = 0u; i < 9u; i++) {
        oturum_sonu();
        TEST_ASSERT_EQUAL_UINT8(1u, OUT.n_events); /* küme doluyken de üretilir */
        son = OUT.events[0].event_id;
    }
    IN.n_evres = 1u;
    IN.evres[0].event_id = son; /* dokuzuncu izlenmedi */
    IN.evres[0].result = (uint8_t)IDA_EVR_APPLIED;
    (void)cagri(true);
    TEST_ASSERT_EQUAL_UINT32(1u, OUT.status.cnt.evres_ignored);
}

/* ---- Çıkış kapısı: yalnız siler -------------------------------------------------------- */

static void aday(uint8_t kind, uint8_t tube)
{
    ida_event_t *a = &C.olay.aday[C.olay.n_aday];
    (void)memset(a, 0, sizeof(*a));
    a->kind = kind;
    a->tube = tube;
    a->hold_ns = IDA_HOLD_MIN_NS;
    C.olay.n_aday++;
}

static void test_kapi(void)
{
    hazirla();
    (void)cagri(true);
    /* Bütün engeller kalkmış varsayılırsa geçer: gevşetilmiş emniyet, silahlı, taze IMU. */
    C.emn.kisitli = 0u;
    C.emn.armed = true;
    aday((uint8_t)IDA_EVT_SAFETY_OPEN, 0u);
    TEST_ASSERT_EQUAL(IDA_RC_OK, cagri(true));
    TEST_ASSERT_EQUAL_UINT32(1u, ds_olay_say(&OUT, (uint8_t)IDA_EVT_SAFETY_OPEN));
    TEST_ASSERT_TRUE((OUT.fc.inhibit_mask & IDA_FCI_RESULT_PENDING) != 0u);
    const uint64_t id0 = OUT.events[0].event_id;
    /* Sonucu beklenirken aynı tüpe yeni izin olayı yok (A4-K123); öteki tüpe olur. */
    aday((uint8_t)IDA_EVT_SAFETY_OPEN, 0u);
    aday((uint8_t)IDA_EVT_SAFETY_OPEN, 1u);
    TEST_ASSERT_EQUAL(IDA_RC_OK, cagri(true));
    TEST_ASSERT_EQUAL_UINT8(1u, OUT.n_events);
    TEST_ASSERT_EQUAL_UINT8(1u, OUT.events[0].tube);
    const uint64_t id1 = OUT.events[0].event_id;
    /* tick'te genişletici yok (A4-K64): bekleyen sonuçlar aynı çağrıda gelir, tüp serbest,
       silahlı ve IMU taze; adayı yalnız "IDA_RC_OK dönen step" koşulu siler. */
    IN.n_evres = 2u;
    IN.evres[0].event_id = id0;
    IN.evres[0].result = (uint8_t)IDA_EVR_APPLIED;
    IN.evres[1].event_id = id1;
    IN.evres[1].result = (uint8_t)IDA_EVR_APPLIED;
    aday((uint8_t)IDA_EVT_FIRE, 0u);
    TEST_ASSERT_EQUAL(IDA_RC_OK, cagri(false));
    TEST_ASSERT_EQUAL_UINT8(0u, OUT.n_events);
    TEST_ASSERT_FALSE(OUT.is_safe);
    TEST_ASSERT_EQUAL_UINT32(0u, OUT.fc.inhibit_mask & IDA_FCI_RESULT_PENDING);
    IN.n_evres = 0u;
    /* aynı koşullarda step OK'ta geçer */
    aday((uint8_t)IDA_EVT_FIRE, 0u);
    TEST_ASSERT_EQUAL(IDA_RC_OK, cagri(true));
    TEST_ASSERT_EQUAL_UINT32(1u, ds_olay_say(&OUT, (uint8_t)IDA_EVT_FIRE));
    /* daraltmalı çağrıda yok ve widen_blocked sayılır (A4-K45) */
    hazirla();
    (void)cagri(true);
    C.emn.kisitli = 0u;
    C.emn.armed = true;
    aday((uint8_t)IDA_EVT_SAFETY_OPEN, 0u);
    IN.narrow.mask = IDA_NARROW_FC_ABORT;
    TEST_ASSERT_EQUAL(IDA_RC_OK, cagri(true));
    TEST_ASSERT_EQUAL_UINT8(0u, OUT.n_events);
    TEST_ASSERT_EQUAL_UINT32(1u, OUT.status.cnt.widen_blocked);
    IN.narrow.mask = 0u;
    /* kısıtlı tüp (A4-K116) ve silahsızken FIRE (A4-K64) */
    C.emn.kisitli = IDA_EMN_PIM_T0;
    aday((uint8_t)IDA_EVT_SAFETY_OPEN, 0u);
    C.emn.armed = false;
    aday((uint8_t)IDA_EVT_FIRE, 1u);
    TEST_ASSERT_EQUAL(IDA_RC_OK, cagri(true));
    TEST_ASSERT_EQUAL_UINT8(0u, OUT.n_events);
    /* sığmayan aday üretilmez, sayaç tüketilmez (A4-K125) */
    C.emn.kisitli = 0u;
    C.emn.armed = true;
    const uint32_t ctr = C.olay.ctr;
    for (uint32_t i = 0u; i < 4u; i++) {
        aday((uint8_t)IDA_EVT_FIRE, (uint8_t)(i % 2u));
    }
    TEST_ASSERT_EQUAL(IDA_RC_OK, cagri(true));
    TEST_ASSERT_EQUAL_UINT8(2u, OUT.n_events); /* tüp başına bir (bekleyen sonuç) */
    TEST_ASSERT_EQUAL_UINT32(ctr + 2u, C.olay.ctr);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_k61_kimlik_ve_k63);
    RUN_TEST(test_k61_olaysiz_calisma);
    RUN_TEST(test_k62_sayac_siniri);
    RUN_TEST(test_k69_sonuc_ve_k70_zaman_asimi);
    RUN_TEST(test_k69_tanimsiz_sonuc_kodu);
    RUN_TEST(test_k129_bekleyen_kumesi);
    RUN_TEST(test_kapi);
    return UNITY_END();
}
