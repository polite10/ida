/*
 * test_atis_durum.c — atış kontrol emniyet zincirinin silahlı yolları (düzenek yolu;
 * atis_duzenek.h): durum makinesi, komutların kabul edildiği durumlar, izin açma ve
 * yenileme, FIRE'ın FC_FIRE'a bağlılığı, olay sonuçlarına göre geçişler, pencere ve
 * değerlendirme sonu, TEST, kalp atışı, tekleme, kilitli arıza ve FC_RESET.
 * Test adları G-Ç madde kimliğini taşır (03 Ek).
 */
#include <string.h>

#include "atis_duzenek.h"
#include "atis/atis_ic.h"
#include "destek.h"
#include "unity.h"

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
static dz_kosul_t K;
static uint8_t G_TUP[2];
static uint8_t G_ESTOP;
static bool G_HB; /* her çağrıda kalp atışı tazelenir */
/* Alıcının (sim) taklidi: çıkan SAFETY_CLOSE'lar sonraki çağrıda APPLIED alır ve kapsanan
   tüplerin geri okumasındaki PERMIT_OPEN düşer (A1-K86). */
static bool G_KAPAT_YANIT;
static uint64_t G_KAPAT_ID[8];
static uint8_t G_KAPAT_TUP[8];
static uint32_t G_KAPAT_N;

#define HOLD_NS 200000000LL /* FC_HOLD_NS varsayılanı */

static void kur(void)
{
    TEST_ASSERT_EQUAL(IDA_RC_OK, ds_kur(&C));
    T = 2000 * DZ_MS;
    SEQ = 0u;
    ESEQ = 0u;
    CID = 1000u;
    dz_kosul_olagan(&K, T);
    G_TUP[0] = DZ_TUP_HAZIR;
    G_TUP[1] = DZ_TUP_HAZIR;
    G_ESTOP = 0u;
    G_HB = true;
    G_KAPAT_YANIT = true;
    G_KAPAT_N = 0u;
    C.emn.armed = true; /* araç silahlı (görev modülünün yerine; düzenek) */
    C.emn.mode = (uint8_t)IDA_MODE_GUIDED;
}

static void yeni(void)
{
    SEQ++;
    T += 5 * DZ_MS;
    dz_girdi(&IN, T, SEQ, true);
    if (G_HB) {
        K.gcs_son_ns = T;
    }
    if (G_KAPAT_YANIT) {
        for (uint32_t i = 0u; i < G_KAPAT_N; i++) {
            dz_sonuc(&IN, G_KAPAT_ID[i], (uint8_t)IDA_EVR_APPLIED, T - DZ_MS);
            for (uint8_t t = 0u; t < 2u; t++) {
                if ((G_KAPAT_TUP[i] == t) || (G_KAPAT_TUP[i] == (uint8_t)IDA_TUBE_ALL)) {
                    G_TUP[t] = (uint8_t)(G_TUP[t] & ~IDA_TUBE_PERMIT_OPEN);
                }
            }
        }
    }
    G_KAPAT_N = 0u;
}

static void kapatmalari_al(void)
{
    for (uint32_t i = 0u; i < (uint32_t)OUT.n_events; i++) {
        if ((OUT.events[i].kind == (uint8_t)IDA_EVT_SAFETY_CLOSE) && (G_KAPAT_N < 8u)) {
            G_KAPAT_ID[G_KAPAT_N] = OUT.events[i].event_id;
            G_KAPAT_TUP[G_KAPAT_N] = OUT.events[i].tube;
            G_KAPAT_N++;
        }
    }
}

static void emn(void)
{
    ESEQ++;
    (void)dz_emniyet(&IN, 0u, ESEQ, T, T, G_ESTOP, G_TUP[0], G_TUP[1]);
}

static ida_rc_t cagir(void)
{
    const ida_rc_t rc = dz_cagri(&C, &IN, &OUT, true, DZ_KABUL, &K);
    kapatmalari_al();
    return rc;
}

static ida_rc_t tick(void)
{
    IN.imu_present = false;
    const ida_rc_t rc = dz_cagri(&C, &IN, &OUT, false, DZ_KABUL, &K);
    kapatmalari_al();
    return rc;
}

/* Örnekli olağan step. */
static void adim(void)
{
    yeni();
    emn();
    TEST_ASSERT_EQUAL(IDA_RC_OK, cagir());
}

/* ms boyunca 5 ms'lik step'ler; her 50 ms'de emniyet örneği. */
static void bekle(int64_t ms)
{
    const int64_t son = T + (ms * DZ_MS);
    uint32_t i = 0u;
    while (T < son) {
        yeni();
        if ((i % 10u) == 0u) {
            emn();
        }
        TEST_ASSERT_EQUAL(IDA_RC_OK, cagir());
        i++;
    }
}

static uint32_t komut(uint8_t op, uint8_t tube)
{
    CID++;
    dz_fc(&IN, CID, op, tube);
    return CID;
}

static const ida_cmd_result_t *sonuc(uint32_t id)
{
    const ida_cmd_result_t *r = dz_sonuc_al(&OUT, id);
    TEST_ASSERT_NOT_NULL(r);
    return r;
}

static void kabul_bekle(uint32_t id)
{
    TEST_ASSERT_EQUAL_UINT8(IDA_ACK_ACCEPTED, sonuc(id)->ack);
}

static void ret_bekle(uint32_t id, uint16_t neden)
{
    const ida_cmd_result_t *r = sonuc(id);
    TEST_ASSERT_EQUAL_UINT8(IDA_ACK_REJECTED, r->ack);
    TEST_ASSERT_EQUAL_UINT16(neden, r->reason);
}

static bool kapanma_var(uint8_t tube)
{
    for (uint32_t i = 0u; i < (uint32_t)OUT.n_events; i++) {
        const ida_event_t *e = &OUT.events[i];
        if ((e->kind == (uint8_t)IDA_EVT_SAFETY_CLOSE) &&
            ((e->tube == tube) || (e->tube == (uint8_t)IDA_TUBE_ALL))) {
            return true;
        }
    }
    return false;
}

static uint32_t genis_say(void)
{
    return dz_olay_say(&OUT, (uint8_t)IDA_EVT_SAFETY_OPEN) + dz_olay_say(&OUT, (uint8_t)IDA_EVT_FIRE);
}

/* Gevşetilmiş emniyet ve FC_ARM(tüp) → ARMED. */
static void silahla(uint8_t tup)
{
    kur();
    adim(); /* gevşetme (inst = 0) */
    TEST_ASSERT_EQUAL_HEX32(0u, OUT.fc.inhibit_mask & (IDA_FCI_ESTOP | IDA_FCI_PLUG_T0 |
                                                       IDA_FCI_PLUG_T1 | IDA_FCI_PIN_T0 |
                                                       IDA_FCI_PIN_T1 | IDA_FCI_EMPTY_T0 |
                                                       IDA_FCI_EMPTY_T1));
    yeni();
    emn();
    const uint32_t id = komut((uint8_t)IDA_FC_ARM, tup);
    TEST_ASSERT_EQUAL(IDA_RC_OK, cagir());
    kabul_bekle(id);
    TEST_ASSERT_EQUAL_UINT8(IDA_FCS_ARMED, OUT.fc.state);
    TEST_ASSERT_EQUAL_UINT32(0u, genis_say()); /* G-Ç47: FC_ARM izin açmaz */
}

/* ARMED'den FC_FIRE: aynı çağrıda SAFETY_OPEN; kimliği döner. */
static uint64_t ates_iste(uint8_t tup)
{
    yeni();
    emn();
    const uint32_t id = komut((uint8_t)IDA_FC_FIRE, tup);
    TEST_ASSERT_EQUAL(IDA_RC_OK, cagir());
    kabul_bekle(id);
    const ida_event_t *e = dz_olay(&OUT, (uint8_t)IDA_EVT_SAFETY_OPEN);
    TEST_ASSERT_NOT_NULL(e);
    TEST_ASSERT_EQUAL_UINT8(tup, e->tube);
    TEST_ASSERT_EQUAL_UINT32((uint32_t)HOLD_NS, e->hold_ns);
    TEST_ASSERT_EQUAL_UINT8(IDA_FCS_ARMED, OUT.fc.state); /* izin henüz açık sayılmaz */
    TEST_ASSERT_TRUE((OUT.fc.inhibit_mask & IDA_FCI_RESULT_PENDING) != 0u);
    return e->event_id;
}

/* SAFETY_OPEN APPLIED + PERMIT_OPEN geri okuması → FIRE; FIRE kimliği döner. */
static uint64_t izin_ve_ates(uint8_t tup, uint64_t acik)
{
    yeni();
    dz_sonuc(&IN, acik, (uint8_t)IDA_EVR_APPLIED, T - DZ_MS);
    G_TUP[tup] = (uint8_t)(G_TUP[tup] | IDA_TUBE_PERMIT_OPEN);
    emn();
    TEST_ASSERT_EQUAL(IDA_RC_OK, cagir());
    const ida_event_t *f = dz_olay(&OUT, (uint8_t)IDA_EVT_FIRE);
    TEST_ASSERT_NOT_NULL(f);
    TEST_ASSERT_EQUAL_UINT8(tup, f->tube);
    TEST_ASSERT_EQUAL_UINT32(0u, f->hold_ns); /* G-Ç10 */
    TEST_ASSERT_EQUAL_UINT8(IDA_FCS_FIRE_PENDING, OUT.fc.state);
    return f->event_id;
}

/* ---- G-Ç21, G-Ç47, G-Ç11, G-Ç10: atış zinciri ------------------------------------------ */

static void test_gc21_gc47_tam_zincir(void)
{
    silahla(0u);
    const uint64_t acik = ates_iste(0u);
    const uint64_t ates = izin_ve_ates(0u, acik);
    /* FIRE APPLIED ve tüp boşaldı → ASSESS → T_assess sonra SAFETY_CLOSE + SAFE. Dış durum
       A4 §3.16'ya göre: FIRE'ın sonucu alındı (FIRE_PENDING değil); izin hâlâ açık sayılıyor
       (PERMIT_OPEN); izin hold_ns ile kapanınca ARMED. */
    const int64_t izin_cikis = T - (5 * DZ_MS); /* SAFETY_OPEN'ın çıktığı çağrı */
    yeni();
    dz_sonuc(&IN, ates, (uint8_t)IDA_EVR_APPLIED, T - DZ_MS);
    G_TUP[0] = (uint8_t)(IDA_TUBE_PLUG_IN | IDA_TUBE_PIN_OUT);
    emn();
    TEST_ASSERT_EQUAL(IDA_RC_OK, cagir());
    TEST_ASSERT_EQUAL_HEX32(IDA_FC_IC_ASSESS, C.fc.ic);
    TEST_ASSERT_EQUAL_UINT8(IDA_FCS_PERMIT_OPEN, OUT.fc.state);
    TEST_ASSERT_EQUAL_UINT32(0u, genis_say());
    while (T < (izin_cikis + HOLD_NS - (5 * DZ_MS))) {
        adim();
        TEST_ASSERT_EQUAL_UINT8(IDA_FCS_PERMIT_OPEN, OUT.fc.state);
    }
    while (T < (izin_cikis + HOLD_NS)) {
        adim();
    }
    TEST_ASSERT_EQUAL_UINT8(IDA_FCS_ARMED, OUT.fc.state); /* açık izin yok */
    bekle(990 - ((HOLD_NS / DZ_MS) - 10)); /* ASSESS girişinden 990 ms sonrasına */
    TEST_ASSERT_EQUAL_UINT8(IDA_FCS_ARMED, OUT.fc.state);
    bekle(20);
    TEST_ASSERT_EQUAL_UINT8(IDA_FCS_SAFE, OUT.fc.state);
    /* G-Ç48: sonraki atış yeni FC_ARM ister. */
    yeni();
    emn();
    const uint32_t id = komut((uint8_t)IDA_FC_FIRE, 0u);
    TEST_ASSERT_EQUAL(IDA_RC_OK, cagir());
    ret_bekle(id, (uint16_t)IDA_REASON_NOT_ARMABLE); /* tüp 0 boş: satır sırası */
}

static void test_gc21_fc_fire_olmadan_fire_yok(void)
{
    silahla(1u);
    /* ARMED'de izin geri okuması ve olay sonuçları ne olursa olsun olay yok. */
    G_TUP[1] = (uint8_t)(G_TUP[1] | IDA_TUBE_PERMIT_OPEN);
    for (uint32_t i = 0u; i < 50u; i++) {
        yeni();
        emn();
        dz_sonuc(&IN, ((uint64_t)7u << 32) | (uint64_t)(i + 1u), (uint8_t)IDA_EVR_APPLIED, T);
        TEST_ASSERT_EQUAL(IDA_RC_OK, cagir());
        TEST_ASSERT_EQUAL_UINT32(0u, genis_say());
    }
    TEST_ASSERT_EQUAL_UINT8(IDA_FCS_ARMED, OUT.fc.state);
}

static void test_gc21_bir_fc_fire_bir_fire(void)
{
    silahla(0u);
    const uint64_t acik = ates_iste(0u);
    (void)izin_ve_ates(0u, acik);
    /* FIRE'ın sonucu beklenirken ve sonra yeni FIRE yok; ikinci FC_FIRE BAD_MODE ya da
       RESULT_PENDING ile NOT_ARMABLE. */
    for (uint32_t i = 0u; i < 10u; i++) {
        yeni();
        emn();
        const uint32_t id = komut((uint8_t)IDA_FC_FIRE, 0u);
        TEST_ASSERT_EQUAL(IDA_RC_OK, cagir());
        TEST_ASSERT_EQUAL_UINT8(IDA_ACK_REJECTED, sonuc(id)->ack);
        TEST_ASSERT_EQUAL_UINT32(0u, genis_say());
    }
}

static void test_gc11_izin_duvar_saatinde(void)
{
    silahla(0u);
    const uint64_t acik = ates_iste(0u);
    /* APPLIED gelir ama geri okuma izni göstermez: izin açık sayılır (PERMIT_OPEN). */
    yeni();
    dz_sonuc(&IN, acik, (uint8_t)IDA_EVR_APPLIED, T - DZ_MS);
    emn();
    TEST_ASSERT_EQUAL(IDA_RC_OK, cagir());
    TEST_ASSERT_EQUAL_UINT8(IDA_FCS_PERMIT_OPEN, OUT.fc.state);
    /* Ölçüm zamanı durur (bariyer beklemesi): yalnız duvar saati ilerler; yenileme yapılmaz
       (sonuç yok) ve hold_ns dolunca izin çekirdekte kapanır. */
    const int64_t t_olcum = T;
    const int64_t bas = T;
    bool kapandi = false;
    while ((T - bas) < (HOLD_NS + (20 * DZ_MS))) {
        SEQ++;
        T += 5 * DZ_MS;
        dz_girdi(&IN, T, SEQ, false);
        IN.tf.present = false;
        K.gcs_son_ns = T;
        (void)t_olcum;
        TEST_ASSERT_EQUAL(IDA_RC_OK, tick());
        TEST_ASSERT_EQUAL_UINT32(0u, genis_say());
        if (OUT.fc.state != (uint8_t)IDA_FCS_PERMIT_OPEN) {
            kapandi = true;
            TEST_ASSERT_TRUE((T - bas) >= (HOLD_NS - (5 * DZ_MS)));
            break;
        }
    }
    TEST_ASSERT_TRUE(kapandi);
}

/* ---- G-Ç12: yenileme ------------------------------------------------------------------- */

static void test_gc12_yenileme_araligi(void)
{
    silahla(0u);
    uint64_t acik = ates_iste(0u);
    int64_t onceki_cikis = T;
    /* Sonuçlar 10 ms içinde gelir; geri okuma izni göstermez (FIRE yok). Pencere (300 ms)
       içinde yenilemeler hold/2'yi aşmaz. */
    uint32_t yenileme = 0u;
    int64_t sonuc_zamani = T + (10 * DZ_MS);
    bool bekliyor = true;
    for (uint32_t i = 0u; i < 50u; i++) {
        yeni();
        emn();
        if (bekliyor && (T >= sonuc_zamani)) {
            dz_sonuc(&IN, acik, (uint8_t)IDA_EVR_APPLIED, T - DZ_MS);
            bekliyor = false;
        }
        TEST_ASSERT_EQUAL(IDA_RC_OK, cagir());
        TEST_ASSERT_EQUAL_UINT32(0u, dz_olay_say(&OUT, (uint8_t)IDA_EVT_FIRE));
        const ida_event_t *e = dz_olay(&OUT, (uint8_t)IDA_EVT_SAFETY_OPEN);
        if (e != NULL) {
            TEST_ASSERT_FALSE(bekliyor); /* G-Ç40: sonuç beklenirken yeni izin olayı yok */
            TEST_ASSERT_TRUE((T - onceki_cikis) <= (HOLD_NS / 2));
            onceki_cikis = T;
            acik = e->event_id;
            bekliyor = true;
            sonuc_zamani = T + (10 * DZ_MS);
            yenileme++;
        }
    }
    TEST_ASSERT_TRUE(yenileme >= 3u);
    /* Sonuç gelmezse yenileme yapılmaz; izin kendi süresiyle kapanır. */
    silahla(0u);
    (void)ates_iste(0u);
    for (uint32_t i = 0u; i < 30u; i++) {
        yeni();
        emn();
        TEST_ASSERT_EQUAL(IDA_RC_OK, cagir());
        TEST_ASSERT_EQUAL_UINT32(0u, genis_say());
    }
}

/* ---- G-Ç22, G-Ç04/05 (FC satırları): kabul edilen durumlar ve satır sırası ---------------- */

static void test_gc22_kabul_durumlari(void)
{
    kur();
    adim();
    /* SAFE'te FC_FIRE → BAD_MODE (engeller temiz). */
    yeni();
    emn();
    uint32_t id = komut((uint8_t)IDA_FC_FIRE, 0u);
    TEST_ASSERT_EQUAL(IDA_RC_OK, cagir());
    ret_bekle(id, (uint16_t)IDA_REASON_BAD_MODE);
    silahla(0u);
    /* ARMED'de FC_ARM ve FC_TEST_ENTER → BAD_MODE; öteki tüpe FC_FIRE → BAD_MODE. */
    yeni();
    emn();
    const uint32_t a = komut((uint8_t)IDA_FC_ARM, 1u);
    const uint32_t b = komut((uint8_t)IDA_FC_TEST_ENTER, 0u);
    const uint32_t f = komut((uint8_t)IDA_FC_FIRE, 1u);
    TEST_ASSERT_EQUAL(IDA_RC_OK, cagir());
    ret_bekle(a, (uint16_t)IDA_REASON_BAD_MODE);
    ret_bekle(b, (uint16_t)IDA_REASON_BAD_MODE);
    ret_bekle(f, (uint16_t)IDA_REASON_BAD_MODE);
    TEST_ASSERT_EQUAL_UINT32(0u, genis_say());
    /* FIRE_AUTHORIZED'da ikinci FC_FIRE BAD_MODE ya da RESULT_PENDING (NOT_ARMABLE); aynı
       cmd_id DUPLICATE. */
    (void)ates_iste(0u);
    const uint32_t ilk = CID;
    yeni();
    emn();
    dz_fc(&IN, ilk, (uint8_t)IDA_FC_FIRE, 0u);
    id = komut((uint8_t)IDA_FC_FIRE, 0u);
    TEST_ASSERT_EQUAL(IDA_RC_OK, cagir());
    TEST_ASSERT_EQUAL_UINT8(IDA_ACK_DUPLICATE, sonuc(ilk)->ack);
    ret_bekle(id, (uint16_t)IDA_REASON_NOT_ARMABLE); /* RESULT_PENDING önce (satır sırası) */
}

static void test_gc04_gc05_fc_satir_sirasi(void)
{
    /* FC_ARM: SESSION > EVENTS_OFF > MODE > NOT_ARMABLE > BAD_MODE. */
    kur();
    adim();
    yeni();
    emn();
    IN.plat.link_state = (uint8_t)IDA_LINK_NONE;
    uint32_t id = komut((uint8_t)IDA_FC_ARM, 0u);
    (void)cagir();
    ret_bekle(id, (uint16_t)IDA_REASON_SESSION);
    kur();
    adim();
    C.olay.boot_gecerli = false; /* olay kimliği yok (A4-K61) */
    yeni();
    emn();
    C.emn.mode = (uint8_t)IDA_MODE_MANUAL;
    id = komut((uint8_t)IDA_FC_ARM, 0u);
    (void)cagir();
    ret_bekle(id, (uint16_t)IDA_REASON_EVENTS_OFF);
    kur();
    adim();
    yeni();
    emn();
    C.emn.mode = (uint8_t)IDA_MODE_MANUAL;
    C.emn.armed = false;
    id = komut((uint8_t)IDA_FC_ARM, 0u);
    (void)cagir();
    ret_bekle(id, (uint16_t)IDA_REASON_BAD_MODE); /* MODE, NOT_ARMED'dan önce */
    /* NOT_ARMED, EST, FAILSAFE, ESTOP, IMU_STALE → NOT_ARMABLE */
    for (uint32_t k = 0u; k < 5u; k++) {
        kur();
        adim();
        yeni();
        if (k == 0u) {
            C.emn.armed = false;
        } else if (k == 1u) {
            K.est = (uint8_t)IDA_EST_DEGRADED;
        } else if (k == 2u) {
            K.fs = (uint8_t)IDA_FS_HOLD;
        } else if (k == 3u) {
            C.emn.kisitli = IDA_EMN_ESTOP;
        } else {
            K.imu_bayat = true;
        }
        id = komut((uint8_t)IDA_FC_ARM, 0u);
        const uint32_t te = komut((uint8_t)IDA_FC_TEST_ENTER, 0u);
        (void)cagir();
        ret_bekle(id, (uint16_t)IDA_REASON_NOT_ARMABLE);
        ret_bekle(te, (uint16_t)IDA_REASON_NOT_ARMABLE);
    }
    /* FC_FIRE: tüpün PLUG, PIN, EMPTY bitleri NOT_ARMABLE; durum BAD_MODE'dan önce. */
    for (uint32_t k = 0u; k < 3u; k++) {
        silahla(0u);
        if (k == 0u) {
            G_TUP[0] = (uint8_t)(DZ_TUP_HAZIR & ~IDA_TUBE_PLUG_IN);
        } else if (k == 1u) {
            G_TUP[0] = (uint8_t)(DZ_TUP_HAZIR & ~IDA_TUBE_PIN_OUT);
        } else {
            G_TUP[0] = (uint8_t)(DZ_TUP_HAZIR & ~IDA_TUBE_LOADED);
        }
        adim(); /* fiş/pim kapanması FC'yi SAFE'e alır; boşalma almaz */
        yeni();
        emn();
        id = komut((uint8_t)IDA_FC_FIRE, 0u);
        (void)cagir();
        ret_bekle(id, (uint16_t)IDA_REASON_NOT_ARMABLE);
    }
    /* FC_RESET: SESSION > BAD_MODE (FAULT_LATCHED değil). */
    kur();
    adim();
    yeni();
    IN.plat.link_state = (uint8_t)IDA_LINK_NONE;
    id = komut((uint8_t)IDA_FC_RESET, 0u);
    (void)cagir();
    ret_bekle(id, (uint16_t)IDA_REASON_SESSION);
    yeni();
    id = komut((uint8_t)IDA_FC_RESET, 0u);
    (void)cagir();
    ret_bekle(id, (uint16_t)IDA_REASON_BAD_MODE);
}

/* ---- G-Ç48: pencere sonu ve değerlendirme sonu ----------------------------------------- */

static void test_gc48_pencere_sonu(void)
{
    silahla(0u);
    uint64_t acik = ates_iste(0u);
    const int64_t bas = T;
    bool kapandi = false;
    while (!kapandi) {
        yeni();
        emn();
        if (acik != 0u) {
            dz_sonuc(&IN, acik, (uint8_t)IDA_EVR_APPLIED, T - DZ_MS); /* geri okuma izni göstermez */
            acik = 0u;
        }
        TEST_ASSERT_EQUAL(IDA_RC_OK, cagir());
        const ida_event_t *e = dz_olay(&OUT, (uint8_t)IDA_EVT_SAFETY_OPEN);
        if (e != NULL) {
            acik = e->event_id; /* yenileme */
        }
        if (OUT.fc.state == (uint8_t)IDA_FCS_SAFE) {
            kapandi = true;
            TEST_ASSERT_TRUE((T - bas) >= (300 * DZ_MS));
            TEST_ASSERT_TRUE((T - bas) <= (305 * DZ_MS));
            TEST_ASSERT_TRUE(kapanma_var(0u));
        }
        TEST_ASSERT_TRUE((T - bas) < (400 * DZ_MS));
    }
    yeni();
    emn();
    const uint32_t id = komut((uint8_t)IDA_FC_FIRE, 0u);
    TEST_ASSERT_EQUAL(IDA_RC_OK, cagir());
    TEST_ASSERT_EQUAL_UINT8(IDA_ACK_REJECTED, sonuc(id)->ack); /* yeni FC_ARM gerekir */
    TEST_ASSERT_EQUAL_UINT32(0u, genis_say());
}

/* ---- FC_ABORT: FIRE_AUTHORIZED → ARMED; bütün tüplere (W5-K7) --------------------------- */

static void test_fc_abort_pencereden_armed(void)
{
    silahla(0u);
    (void)ates_iste(0u);
    yeni();
    emn();
    IN.narrow.mask = IDA_NARROW_FC_ABORT;
    IN.narrow.cmd_id[3] = 77u;
    const uint32_t id = komut((uint8_t)IDA_FC_FIRE, 0u);
    TEST_ASSERT_EQUAL(IDA_RC_OK, cagir());
    TEST_ASSERT_EQUAL_UINT8(IDA_FCS_ARMED, OUT.fc.state);
    TEST_ASSERT_TRUE(kapanma_var(0u) && kapanma_var(1u));
    TEST_ASSERT_TRUE(OUT.health.narrowed);
    ret_bekle(id, (uint16_t)IDA_REASON_NARROWED); /* G-Ç33 */
    TEST_ASSERT_EQUAL_UINT32(0u, genis_say());
}

/* ---- G-Ç23: SAFETY_OPEN'ın APPLIED dışı sonuçları ve sonucu bilinmeyen ------------------- */

static void test_gc23_acma_sonuclari(void)
{
    const uint8_t kod[] = {IDA_EVR_DUPLICATE,      IDA_EVR_REJ_EXPIRED,     IDA_EVR_REJ_SAFE,
                           IDA_EVR_REJ_TUBE_EMPTY, IDA_EVR_REJ_SESSION,     IDA_EVR_DROPPED_GATE,
                           IDA_EVR_DROPPED_NO_SLOT, IDA_EVR_EXPIRED_NO_ACK, IDA_EVR_SESSION_END};
    for (uint32_t i = 0u; i < (uint32_t)sizeof(kod); i++) {
        silahla(1u);
        const uint64_t acik = ates_iste(1u);
        yeni();
        emn();
        dz_sonuc(&IN, acik, kod[i], 0);
        TEST_ASSERT_EQUAL(IDA_RC_OK, cagir());
        TEST_ASSERT_EQUAL_UINT8(IDA_FCS_SAFE, OUT.fc.state);
        TEST_ASSERT_TRUE(kapanma_var(1u));
        TEST_ASSERT_EQUAL_UINT32(0u, genis_say());
    }
    /* Sonucu bilinmeyen (A4-K70; G-Ç42): apply_by + 100 ms sonra SAFE. */
    silahla(1u);
    (void)ates_iste(1u);
    bool kapandi = false;
    for (uint32_t i = 0u; (i < 40u) && !kapandi; i++) {
        yeni();
        emn();
        TEST_ASSERT_EQUAL(IDA_RC_OK, cagir());
        TEST_ASSERT_EQUAL_UINT32(0u, genis_say());
        kapandi = OUT.fc.state == (uint8_t)IDA_FCS_SAFE;
    }
    TEST_ASSERT_TRUE(kapandi);
    TEST_ASSERT_TRUE(kapanma_var(1u));
    TEST_ASSERT_EQUAL_UINT32(1u, OUT.status.cnt.event_unknown);
}

/* ---- G-Ç49: FIRE sonuçları ---------------------------------------------------------------- */

/* Tek FIRE sonucunu verir; beklenen durumu ve kapanma kaydını doğrular. */
static void gc49_tek_sonuc(uint8_t sonuc, uint8_t beklenen_durum)
{
    silahla(0u);
    const uint64_t ates = izin_ve_ates(0u, ates_iste(0u));
    yeni();
    emn();
    dz_sonuc(&IN, ates, sonuc, 0);
    TEST_ASSERT_EQUAL(IDA_RC_OK, cagir());
    TEST_ASSERT_EQUAL_UINT8(beklenen_durum, OUT.fc.state);
    TEST_ASSERT_TRUE(kapanma_var(0u));
}

static void test_gc49_fire_sonuclari(void)
{
    const uint8_t kilit[] = {IDA_EVR_REJ_TUBE_EMPTY, IDA_EVR_EXPIRED_NO_ACK};
    const uint8_t guvenli[] = {IDA_EVR_REJ_SAFE,       IDA_EVR_REJ_EXPIRED,     IDA_EVR_REJ_SESSION,
                               IDA_EVR_DROPPED_GATE,   IDA_EVR_DROPPED_NO_SLOT, IDA_EVR_SESSION_END};
    for (uint32_t i = 0u; i < (uint32_t)sizeof(kilit); i++) {
        gc49_tek_sonuc(kilit[i], (uint8_t)IDA_FCS_FAULT_LATCHED);
    }
    for (uint32_t i = 0u; i < (uint32_t)sizeof(guvenli); i++) {
        gc49_tek_sonuc(guvenli[i], (uint8_t)IDA_FCS_SAFE);
    }
    /* Sonucu bilinmeyen FIRE → FAULT_LATCHED. */
    silahla(0u);
    (void)izin_ve_ates(0u, ates_iste(0u));
    for (uint32_t i = 0u; i < 40u; i++) {
        yeni();
        emn();
        TEST_ASSERT_EQUAL(IDA_RC_OK, cagir());
    }
    TEST_ASSERT_EQUAL_UINT8(IDA_FCS_FAULT_LATCHED, OUT.fc.state);
    TEST_ASSERT_TRUE((OUT.fc.inhibit_mask & IDA_FCI_FAULT_LATCHED) != 0u);
}

/* ---- G-Ç53 (2): tekleme ve tüp boşaldı zaman aşımı --------------------------------------- */

static void test_gc53_tekleme(void)
{
    silahla(0u);
    const uint64_t ates = izin_ve_ates(0u, ates_iste(0u));
    /* t_meas ≤ t_apply olan dolu örnek tekleme değildir. Dış durum (A4 §3.16): FIRE'ın sonucu
       alındı, izin hâlâ açık sayılıyor → PERMIT_OPEN (FIRE_PENDING değil; sol bulgu 4). */
    yeni();
    const int64_t t_apply = T + DZ_MS;
    dz_sonuc(&IN, ates, (uint8_t)IDA_EVR_APPLIED, t_apply);
    ESEQ++;
    (void)dz_emniyet(&IN, 0u, ESEQ, T, T, 0u, (uint8_t)(DZ_TUP_HAZIR | IDA_TUBE_PERMIT_OPEN),
                     DZ_TUP_HAZIR);
    TEST_ASSERT_EQUAL(IDA_RC_OK, cagir());
    TEST_ASSERT_EQUAL_HEX32(IDA_FC_IC_FIRING, C.fc.ic);
    TEST_ASSERT_EQUAL_UINT8(IDA_FCS_PERMIT_OPEN, OUT.fc.state);
    /* t_meas > t_apply ve LOADED = 1 → FAULT_LATCHED + SAFETY_CLOSE(tüp). */
    yeni();
    emn();
    TEST_ASSERT_EQUAL(IDA_RC_OK, cagir());
    TEST_ASSERT_EQUAL_UINT8(IDA_FCS_FAULT_LATCHED, OUT.fc.state);
    TEST_ASSERT_TRUE(kapanma_var(0u));
    /* Tüp boşaldı geri okuması T_tup_bos (500 ms) içinde gelmezse FAULT_LATCHED. */
    silahla(0u);
    const uint64_t a2 = izin_ve_ates(0u, ates_iste(0u));
    yeni();
    dz_sonuc(&IN, a2, (uint8_t)IDA_EVR_APPLIED, T);
    TEST_ASSERT_EQUAL(IDA_RC_OK, cagir());
    const int64_t bas = T;
    /* örnek gelir ama ölçüm zamanı t_apply'ı geçmez; ölçüm zamanı ilerlemediği için
       T_emn_bayat (250 ms) önce FC'yi SAFE yapar; izleme sürer ve T_tup_bos dolunca kilitlenir */
    while (OUT.fc.state != (uint8_t)IDA_FCS_FAULT_LATCHED) {
        yeni();
        ESEQ++;
        (void)dz_emniyet(&IN, 0u, ESEQ, bas, T, 0u, DZ_TUP_HAZIR, DZ_TUP_HAZIR);
        TEST_ASSERT_EQUAL(IDA_RC_OK, cagir());
        TEST_ASSERT_TRUE((T - bas) <= (510 * DZ_MS));
    }
    TEST_ASSERT_TRUE((T - bas) > (500 * DZ_MS));
}

/* G-Ç20, W5-K1 (sol bulgu 2): tekleme geri okuma çelişkisi sürdükçe süren nedendir. FC_RESET
   kabul edilir ama aynı çağrıda yeniden kilitlenir; çelişki kalkınca (LOADED = 0) FC_RESET
   SAFE yapar. */
static void test_gc20_tekleme_suren_neden(void)
{
    silahla(0u);
    const uint64_t ates = izin_ve_ates(0u, ates_iste(0u));
    yeni();
    dz_sonuc(&IN, ates, (uint8_t)IDA_EVR_APPLIED, T - DZ_MS);
    emn(); /* t_meas > t_apply, LOADED = 1 */
    TEST_ASSERT_EQUAL(IDA_RC_OK, cagir());
    TEST_ASSERT_EQUAL_UINT8(IDA_FCS_FAULT_LATCHED, OUT.fc.state);
    /* Kapanma APPLIED (alıcı taklidi), izin geri okuması kapalı; tüp dolu görünmeye devam
       ediyor. Yeni kimlikli FC_RESET: kabul, aynı çağrıda yeniden FAULT_LATCHED. */
    G_TUP[0] = DZ_TUP_HAZIR;
    for (uint32_t k = 0u; k < 3u; k++) {
        adim();
        yeni();
        emn();
        const uint32_t id = komut((uint8_t)IDA_FC_RESET, 0u);
        TEST_ASSERT_EQUAL(IDA_RC_OK, cagir());
        kabul_bekle(id);
        TEST_ASSERT_EQUAL_UINT8(IDA_FCS_FAULT_LATCHED, OUT.fc.state);
        TEST_ASSERT_TRUE((OUT.fc.inhibit_mask & IDA_FCI_FAULT_LATCHED) != 0u);
        TEST_ASSERT_EQUAL_UINT32(0u, genis_say());
    }
    /* Geri okuma tüpü boş gösterince neden kalkar; aynı çağrıdaki FC_RESET SAFE yapar. */
    G_TUP[0] = (uint8_t)(IDA_TUBE_PLUG_IN | IDA_TUBE_PIN_OUT);
    yeni();
    emn();
    const uint32_t id = komut((uint8_t)IDA_FC_RESET, 0u);
    TEST_ASSERT_EQUAL(IDA_RC_OK, cagir());
    kabul_bekle(id);
    TEST_ASSERT_EQUAL_UINT8(IDA_FCS_SAFE, OUT.fc.state);
    /* T_tup_bos zaman aşımı da süren nedendir: t_meas > t_apply örneği gelene dek. */
    silahla(1u);
    const uint64_t a2 = izin_ve_ates(1u, ates_iste(1u));
    yeni();
    dz_sonuc(&IN, a2, (uint8_t)IDA_EVR_APPLIED, T);
    TEST_ASSERT_EQUAL(IDA_RC_OK, cagir());
    const int64_t t_apply = T;
    while (OUT.fc.state != (uint8_t)IDA_FCS_FAULT_LATCHED) {
        yeni(); /* örnek yok: geri okuma gelmez */
        TEST_ASSERT_EQUAL(IDA_RC_OK, cagir());
        TEST_ASSERT_TRUE((T - t_apply) <= (510 * DZ_MS));
    }
    yeni();
    const uint32_t r1 = komut((uint8_t)IDA_FC_RESET, 0u);
    TEST_ASSERT_EQUAL(IDA_RC_OK, cagir());
    kabul_bekle(r1);
    TEST_ASSERT_EQUAL_UINT8(IDA_FCS_FAULT_LATCHED, OUT.fc.state);
    G_TUP[1] = (uint8_t)(IDA_TUBE_PLUG_IN | IDA_TUBE_PIN_OUT); /* tüp boş */
    yeni();
    emn();
    const uint32_t r2 = komut((uint8_t)IDA_FC_RESET, 0u);
    TEST_ASSERT_EQUAL(IDA_RC_OK, cagir());
    kabul_bekle(r2);
    TEST_ASSERT_EQUAL_UINT8(IDA_FCS_SAFE, OUT.fc.state);
}

/* G-Ç29, W5-K4 eki (sol bulgu 1), silahlı düzenek: izin açan SAFETY_OPEN'ın APPLIED sonucu
   ve PERMIT_OPEN geri okuması aynı çağrıda gelse de, kabul edilen örnek izleri bayatlattıysa
   (geri giden t_rx) FIRE üretilmez; ESTOP, SAFETY_CLOSE(ALL), FC SAFE aynı çağrıda. */
static void test_gc29_bayatlik_ayni_cagri_silahli(void)
{
    silahla(0u);
    const uint64_t acik = ates_iste(0u);
    yeni();
    dz_sonuc(&IN, acik, (uint8_t)IDA_EVR_APPLIED, T - DZ_MS);
    G_TUP[0] = (uint8_t)(G_TUP[0] | IDA_TUBE_PERMIT_OPEN);
    ESEQ++;
    (void)dz_emniyet(&IN, 0u, ESEQ, T, T - (300 * DZ_MS), 0u, G_TUP[0], G_TUP[1]);
    TEST_ASSERT_EQUAL(IDA_RC_OK, cagir());
    TEST_ASSERT_EQUAL_UINT32(0u, genis_say());
    TEST_ASSERT_TRUE((OUT.fc.inhibit_mask & IDA_FCI_ESTOP) != 0u);
    TEST_ASSERT_TRUE(kapanma_var(0u) && kapanma_var(1u));
    TEST_ASSERT_TRUE(OUT.health.narrowed);
    TEST_ASSERT_EQUAL_UINT8(IDA_FCS_SAFE, OUT.fc.state);
    /* Aynı dizi taze t_rx ile: FIRE (karşı sınama). */
    silahla(0u);
    const uint64_t acik2 = ates_iste(0u);
    (void)izin_ve_ates(0u, acik2);
}

/* ---- G-Ç24, G-Ç51: TEST ------------------------------------------------------------------ */

static void test_gc24_gc51_test(void)
{
    kur();
    adim();
    yeni();
    emn();
    uint32_t id = komut((uint8_t)IDA_FC_TEST_ENTER, 0u);
    TEST_ASSERT_EQUAL(IDA_RC_OK, cagir());
    kabul_bekle(id);
    TEST_ASSERT_EQUAL_UINT8(IDA_FCS_TEST, OUT.fc.state);
    /* Bütün genişletici girdiler: FC_FIRE, FC_ARM, gevşeyen örnekler, olay sonuçları. */
    G_TUP[0] = (uint8_t)(DZ_TUP_HAZIR | IDA_TUBE_PERMIT_OPEN);
    for (uint32_t i = 0u; i < 40u; i++) {
        yeni();
        emn();
        const uint32_t f = komut((uint8_t)IDA_FC_FIRE, 0u);
        const uint32_t a = komut((uint8_t)IDA_FC_ARM, 1u);
        dz_sonuc(&IN, ((uint64_t)7u << 32) | (uint64_t)(i + 1u), (uint8_t)IDA_EVR_APPLIED, T);
        TEST_ASSERT_EQUAL(IDA_RC_OK, cagir());
        ret_bekle(f, (uint16_t)IDA_REASON_BAD_MODE);
        ret_bekle(a, (uint16_t)IDA_REASON_BAD_MODE);
        TEST_ASSERT_EQUAL_UINT32(0u, genis_say());
        TEST_ASSERT_EQUAL_UINT8(IDA_FCS_TEST, OUT.fc.state);
    }
    /* G-Ç51: FC_TEST_EXIT → SAFE; TEST dışında ACCEPTED, etkisiz. */
    yeni();
    id = komut((uint8_t)IDA_FC_TEST_EXIT, 0u);
    TEST_ASSERT_EQUAL(IDA_RC_OK, cagir());
    kabul_bekle(id);
    TEST_ASSERT_EQUAL_UINT8(IDA_FCS_SAFE, OUT.fc.state);
    silahla(0u);
    yeni();
    emn();
    id = komut((uint8_t)IDA_FC_TEST_EXIT, 0u);
    TEST_ASSERT_EQUAL(IDA_RC_OK, cagir());
    kabul_bekle(id);
    TEST_ASSERT_EQUAL_UINT8(IDA_FCS_ARMED, OUT.fc.state);
}

/* ---- G-Ç25: kalp atışı kaybı ------------------------------------------------------------- */

static void test_gc25_kalp_atisi(void)
{
    silahla(1u);
    G_HB = false;
    const int64_t son = K.gcs_son_ns;
    bool kapandi = false;
    for (uint32_t i = 0u; (i < 700u) && !kapandi; i++) {
        adim();
        if (OUT.fc.state == (uint8_t)IDA_FCS_SAFE) {
            kapandi = true;
            TEST_ASSERT_TRUE((T - son) > (2500 * DZ_MS)); /* T_hb_fc (W5-K16) */
            TEST_ASSERT_TRUE((T - son) <= (2505 * DZ_MS));
            TEST_ASSERT_TRUE(kapanma_var(1u));
        }
    }
    TEST_ASSERT_TRUE(kapandi);
    /* FAULT_LATCHED'te kalp atışı kaybı durumu değiştirmez (W5-K8). */
    silahla(0u);
    (void)izin_ve_ates(0u, ates_iste(0u));
    yeni();
    emn();
    dz_sonuc(&IN, C.fc.t[0].acik_id + 1u, (uint8_t)IDA_EVR_REJ_TUBE_EMPTY, 0);
    TEST_ASSERT_EQUAL(IDA_RC_OK, cagir());
    TEST_ASSERT_EQUAL_UINT8(IDA_FCS_FAULT_LATCHED, OUT.fc.state);
    G_HB = false;
    bekle(3000);
    TEST_ASSERT_EQUAL_UINT8(IDA_FCS_FAULT_LATCHED, OUT.fc.state);
}

/* ---- G-Ç16, W5-K10: tüpe özgü süregelen kısıtlama ve dönüş çağrısı ---------------------- */

static void test_gc16_tupe_ozgu(void)
{
    silahla(0u);
    /* Tüp 1'in pimi süregelen kısıtlama: tüp 0'ın atış zinciri engellenmez. */
    G_TUP[1] = (uint8_t)(DZ_TUP_HAZIR & ~IDA_TUBE_PIN_OUT);
    adim();
    TEST_ASSERT_TRUE(kapanma_var(1u));
    TEST_ASSERT_EQUAL_UINT8(IDA_FCS_ARMED, OUT.fc.state); /* silahlı tüp 0 */
    const uint64_t acik = ates_iste(0u);
    (void)izin_ve_ates(0u, acik);
    /* Dönüş çağrısı: tüp 1'in fişi bu çağrıda çıkarılırken tüp 0'a yeni FC_FIRE NARROWED. */
    silahla(0u);
    yeni();
    G_TUP[1] = (uint8_t)(DZ_TUP_HAZIR & ~IDA_TUBE_PLUG_IN);
    emn();
    uint32_t id = komut((uint8_t)IDA_FC_FIRE, 0u);
    TEST_ASSERT_EQUAL(IDA_RC_OK, cagir());
    ret_bekle(id, (uint16_t)IDA_REASON_NARROWED);
    TEST_ASSERT_EQUAL_UINT32(0u, genis_say());
    TEST_ASSERT_EQUAL_UINT8(IDA_FCS_ARMED, OUT.fc.state);
    /* Sonraki çağrıda kapsam tüpe özgü: tüp 0'a FC_FIRE kabul. */
    yeni();
    emn();
    id = komut((uint8_t)IDA_FC_FIRE, 0u);
    TEST_ASSERT_EQUAL(IDA_RC_OK, cagir());
    kabul_bekle(id);
    TEST_ASSERT_EQUAL_UINT32(1u, dz_olay_say(&OUT, (uint8_t)IDA_EVT_SAFETY_OPEN));
    /* Silahlı tüpün pimi takılınca FC SAFE (evrensel geçiş). */
    yeni();
    G_TUP[0] = (uint8_t)(DZ_TUP_HAZIR & ~IDA_TUBE_PIN_OUT);
    emn();
    TEST_ASSERT_EQUAL(IDA_RC_OK, cagir());
    TEST_ASSERT_EQUAL_UINT8(IDA_FCS_SAFE, OUT.fc.state);
    TEST_ASSERT_TRUE(kapanma_var(0u));
}

/* ---- G-Ç35, G-Ç07: IMU_LAG, tick ve IMU_REJECTED çıktısında genişletici yok -------------- */

static void test_gc35_gc07_genisletici_yasaklari(void)
{
    silahla(0u);
    yeni();
    emn();
    K.imu_lag = true;
    uint32_t id = komut((uint8_t)IDA_FC_FIRE, 0u);
    TEST_ASSERT_EQUAL(IDA_RC_OK, cagir());
    ret_bekle(id, (uint16_t)IDA_REASON_IMU_LAG);
    K.imu_lag = false;
    (void)ates_iste(0u);
    const uint64_t acik = C.fc.t[0].acik_id;
    /* İzin APPLIED, geri okuma açık; ama IMU_LAG kurulu: FIRE yok. */
    yeni();
    dz_sonuc(&IN, acik, (uint8_t)IDA_EVR_APPLIED, T - DZ_MS);
    G_TUP[0] = (uint8_t)(G_TUP[0] | IDA_TUBE_PERMIT_OPEN);
    emn();
    K.imu_lag = true;
    TEST_ASSERT_EQUAL(IDA_RC_OK, cagir());
    TEST_ASSERT_EQUAL_UINT32(0u, genis_say());
    K.imu_lag = false;
    /* tick ve IMU_REJECTED: FIRE yok */
    yeni();
    emn();
    TEST_ASSERT_EQUAL(IDA_RC_OK, tick());
    TEST_ASSERT_EQUAL_UINT32(0u, genis_say());
    yeni();
    emn();
    K.imu_red = true;
    TEST_ASSERT_EQUAL(IDA_RC_IMU_REJECTED, cagir());
    TEST_ASSERT_EQUAL_UINT32(0u, genis_say());
    K.imu_red = false;
    /* olağan step: FIRE */
    yeni();
    emn();
    TEST_ASSERT_EQUAL(IDA_RC_OK, cagir());
    TEST_ASSERT_EQUAL_UINT32(1u, dz_olay_say(&OUT, (uint8_t)IDA_EVT_FIRE));
    (void)id;
}

/* ---- G-Ç19, G-Ç20, G-Ç46: kilitli arıza ve FC_RESET ---------------------------------------- */

static void kilitle_fire_ile(void)
{
    silahla(0u);
    const uint64_t ates = izin_ve_ates(0u, ates_iste(0u));
    yeni();
    emn();
    dz_sonuc(&IN, ates, (uint8_t)IDA_EVR_REJ_TUBE_EMPTY, 0);
    TEST_ASSERT_EQUAL(IDA_RC_OK, cagir());
    TEST_ASSERT_EQUAL_UINT8(IDA_FCS_FAULT_LATCHED, OUT.fc.state);
}

static void test_gc19_kilit_korunur(void)
{
    for (uint32_t k = 0u; k < 9u; k++) {
        kilitle_fire_ile();
        yeni();
        emn();
        if (k == 0u) {
            IN.narrow.mask = IDA_NARROW_FC_DISARM;
        } else if (k == 1u) {
            IN.narrow.mask = IDA_NARROW_FC_ABORT;
        } else if (k == 2u) {
            IN.narrow.mask = IDA_NARROW_DISARM;
        } else if (k == 3u) {
            IN.plat.link_event = (uint8_t)IDA_LINKEV_SESSION_END;
            IN.plat.link_state = (uint8_t)IDA_LINK_NONE;
        } else if (k == 4u) {
            IN.samples[0].u.safety.estop = 1u;
        } else if (k == 5u) {
            K.imu_bayat = true;
        } else if (k == 6u) {
            K.fs = (uint8_t)IDA_FS_SURFACE;
        } else if (k == 7u) {
            K.gcs_son_ns = T - (2000 * DZ_MS);
        } else {
            C.olay.boot_gecerli = false; /* EVENTS_OFF */
        }
        (void)cagir();
        TEST_ASSERT_EQUAL_UINT8(IDA_FCS_FAULT_LATCHED, OUT.fc.state);
        TEST_ASSERT_EQUAL_UINT32(0u, genis_say());
        K.imu_bayat = false;
        K.fs = (uint8_t)IDA_FS_NONE;
    }
}

static void test_gc20_gc46_reset(void)
{
    kilitle_fire_ile();
    /* Daraltıcı girdili çağrıda NARROWED, mandal kalır. */
    yeni();
    emn();
    IN.narrow.mask = IDA_NARROW_FC_DISARM;
    uint32_t id = komut((uint8_t)IDA_FC_RESET, 0u);
    TEST_ASSERT_EQUAL(IDA_RC_OK, cagir());
    ret_bekle(id, (uint16_t)IDA_REASON_NARROWED);
    TEST_ASSERT_EQUAL_UINT8(IDA_FCS_FAULT_LATCHED, OUT.fc.state);
    /* Reddedilen çağrıda CALL_REJECTED. */
    yeni();
    id = komut((uint8_t)IDA_FC_RESET, 0u);
    TEST_ASSERT_EQUAL(IDA_RC_DUP_SEQ, dz_cagri(&C, &IN, &OUT, true, DZ_RET_DUP_SEQ, &K));
    ret_bekle(id, (uint16_t)IDA_REASON_CALL_REJECTED);
    TEST_ASSERT_EQUAL_UINT8(IDA_FCS_FAULT_LATCHED, OUT.fc.state);
    /* Kabul: SAFE; izin açmaz, silahlandırmaz; aynı çağrıdaki FC_ARM FAULT_LATCHED engeliyle
       reddedilir. */
    yeni();
    emn();
    id = komut((uint8_t)IDA_FC_RESET, 0u);
    const uint32_t a = komut((uint8_t)IDA_FC_ARM, 0u);
    TEST_ASSERT_EQUAL(IDA_RC_OK, cagir());
    kabul_bekle(id);
    ret_bekle(a, (uint16_t)IDA_REASON_NOT_ARMABLE);
    TEST_ASSERT_EQUAL_UINT8(IDA_FCS_SAFE, OUT.fc.state);
    TEST_ASSERT_EQUAL_UINT32(0u, genis_say());
    /* Kabul edilen FC_RESET'ten sonra yeniden silahlandırma yeni FC_ARM ister ve kabul edilir. */
    yeni();
    emn();
    id = komut((uint8_t)IDA_FC_ARM, 1u);
    TEST_ASSERT_EQUAL(IDA_RC_OK, cagir());
    kabul_bekle(id);
    TEST_ASSERT_EQUAL_UINT8(IDA_FCS_ARMED, OUT.fc.state);
}

/* Aynı çağrıda ortaya çıkan kilit nedeni (W5-K1): SAFETY_CLOSE'un EXPIRED_NO_ACK sonucu ve
   FC_RESET aynı çağrıda → FAULT_LATCHED kalır, FC_STATE günlüğü yok. */
static void test_gc20_ayni_cagrida_yeniden_kilit(void)
{
    kilitle_fire_ile();
    const ida_event_t *k = dz_olay(&OUT, (uint8_t)IDA_EVT_SAFETY_CLOSE);
    TEST_ASSERT_NOT_NULL(k);
    const uint64_t kapat = k->event_id;
    G_KAPAT_YANIT = false;
    yeni();
    emn();
    dz_sonuc(&IN, kapat, (uint8_t)IDA_EVR_EXPIRED_NO_ACK, 0);
    const uint32_t id = komut((uint8_t)IDA_FC_RESET, 0u);
    TEST_ASSERT_EQUAL(IDA_RC_OK, cagir());
    kabul_bekle(id);
    TEST_ASSERT_EQUAL_UINT8(IDA_FCS_FAULT_LATCHED, OUT.fc.state);
    TEST_ASSERT_TRUE(kapanma_var(0u)); /* G-Ç50: kapatma yinelenir */
}

/* ---- W5-K8: güvenli duruma dönüş nedenleri (FAULT dışı) ----------------------------------- */

static void test_gc18_evrensel_gecisler(void)
{
    for (uint32_t k = 0u; k < 10u; k++) {
        silahla(0u);
        (void)ates_iste(0u);
        yeni();
        emn();
        if (k == 0u) {
            IN.narrow.mask = IDA_NARROW_FC_DISARM;
        } else if (k == 1u) {
            IN.narrow.mask = IDA_NARROW_DISARM;
        } else if (k == 2u) {
            IN.plat.link_event = (uint8_t)IDA_LINKEV_SESSION_END;
            IN.plat.link_state = (uint8_t)IDA_LINK_NONE;
        } else if (k == 3u) {
            IN.samples[0].u.safety.estop = 1u;
        } else if (k == 4u) {
            K.imu_bayat = true;
        } else if (k == 5u) {
            K.fs = (uint8_t)IDA_FS_HOLD;
        } else if (k == 6u) {
            K.est = (uint8_t)IDA_EST_BAD;
        } else if (k == 7u) {
            C.emn.armed = false;
        } else if (k == 8u) {
            C.olay.boot_gecerli = false; /* W5-K11 (b) */
        } else {
            IN.plat.link_state = (uint8_t)IDA_LINK_NONE;
        }
        (void)cagir();
        TEST_ASSERT_EQUAL_UINT8(IDA_FCS_SAFE, OUT.fc.state);
        TEST_ASSERT_EQUAL_UINT32(0u, genis_say());
        TEST_ASSERT_TRUE(kapanma_var(0u) || (k == 8u)); /* EVENTS_OFF: kapatma ctr ile */
        K.imu_bayat = false;
        K.fs = (uint8_t)IDA_FS_NONE;
        K.est = (uint8_t)IDA_EST_OK;
    }
    /* Kip izin kümesinden çıkış: FIRE_AUTHORIZED → ARMED (tüp kapanır). */
    silahla(0u);
    (void)ates_iste(0u);
    yeni();
    emn();
    C.emn.mode = (uint8_t)IDA_MODE_STATION_KEEP;
    TEST_ASSERT_EQUAL(IDA_RC_OK, cagir());
    TEST_ASSERT_EQUAL_UINT8(IDA_FCS_ARMED, OUT.fc.state);
    TEST_ASSERT_TRUE(kapanma_var(0u));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_gc21_gc47_tam_zincir);
    RUN_TEST(test_gc21_fc_fire_olmadan_fire_yok);
    RUN_TEST(test_gc21_bir_fc_fire_bir_fire);
    RUN_TEST(test_gc11_izin_duvar_saatinde);
    RUN_TEST(test_gc12_yenileme_araligi);
    RUN_TEST(test_gc22_kabul_durumlari);
    RUN_TEST(test_gc04_gc05_fc_satir_sirasi);
    RUN_TEST(test_gc48_pencere_sonu);
    RUN_TEST(test_fc_abort_pencereden_armed);
    RUN_TEST(test_gc23_acma_sonuclari);
    RUN_TEST(test_gc49_fire_sonuclari);
    RUN_TEST(test_gc53_tekleme);
    RUN_TEST(test_gc20_tekleme_suren_neden);
    RUN_TEST(test_gc29_bayatlik_ayni_cagri_silahli);
    RUN_TEST(test_gc24_gc51_test);
    RUN_TEST(test_gc25_kalp_atisi);
    RUN_TEST(test_gc16_tupe_ozgu);
    RUN_TEST(test_gc35_gc07_genisletici_yasaklari);
    RUN_TEST(test_gc19_kilit_korunur);
    RUN_TEST(test_gc20_gc46_reset);
    RUN_TEST(test_gc20_ayni_cagrida_yeniden_kilit);
    RUN_TEST(test_gc18_evrensel_gecisler);
    return UNITY_END();
}
