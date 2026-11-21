/*
 * Olay kabul testleri (W3-oturum kabul md.4).
 *
 * Dayanılan kurallar: A1-K80 (olay sonucu gelene ya da çıkaran çağrının now_w'sinden apply_by
 * geçene kadar aynı yükle T_olay_yin = 15 ms aralıkla yinelenir; son geçerlilik dolunca
 * yineleme durur; EXPIRED_NO_ACK çekirdeğe ancak apply_by + T_sonuç_pay (95 ms) sonuçsuz
 * dolunca verilir), A4-K106 (yinelemeler aynı baytlarla), A1-K73 ve A1-K90 (yeni olay o
 * çağrının IDA_ACTUATOR'ından önce; n_events), A1-K110 (yinelemeler zorunlu-güvenilir sınıfta),
 * A4-K68 (her olaya tam bir son sonuç; sonraki çağrının evres[]'i), A4-K71 ve A1-K22 (1)
 * (oturum sonunda onaylanmamış olay yinelenmez, yeni oturumda yeniden iletilmez, SESSION_END
 * sonucu alır), A1-K121 (i) (SIL'de zamanlayıcılar sim zamanıyla).
 *
 * Kart rolünde saat 1 ms adımla ilerletilir ve her adımda poll_tx/take çağrılır; olay yineleme
 * aralığının ölçümü bu 1 ms çözünürlüktedir (aralık 15–16 ms kabul edilir).
 */
#include <stdio.h>
#include <string.h>

#include "kabul_oturum.h"
#include "unity.h"

void setUp(void) {}
void tearDown(void) {}

#define GONDERIM_AZAMI 64u

typedef struct {
    size_t  n;
    size_t  sira[GONDERIM_AZAMI];
    int64_t t[GONDERIM_AZAMI];
} gonderimler_t;

static void olay_gonderimleri(size_t bas, uint64_t event_id, gonderimler_t *g)
{
    memset(g, 0, sizeof *g);
    for (size_t i = bas; i < ko_gunluk_boy(); ++i) {
        const ko_cerceve_t *f = ko_gunluk(i);
        if (f->msgid != MAVLINK_MSG_ID_IDA_EVENT) {
            continue;
        }
        mavlink_ida_event_t e;
        ko_coz(f, MAVLINK_MSG_ID_IDA_EVENT, &e, sizeof e);
        if (e.event_id == event_id && g->n < GONDERIM_AZAMI) {
            g->sira[g->n] = i;
            g->t[g->n] = f->t_ns;
            g->n++;
        }
    }
}

/* evres'te event_id'yi taşıyan çağrılar: sayısını döndürür, ilkinin sırasını ve sonucunu yazar. */
static uint32_t evres_ara(size_t bas, uint64_t event_id, size_t *ilk, uint8_t *sonuc, int64_t *t_apply)
{
    uint32_t n = 0;
    for (size_t i = bas; i < ko_cagri_boy(); ++i) {
        const ko_cagri_t *c = ko_cagri(i);
        uint8_t m = (uint8_t)(c->n_evres <= IDA_PORT_MAX_EVRES ? c->n_evres : IDA_PORT_MAX_EVRES);
        for (uint8_t j = 0; j < m; ++j) {
            if (c->evres[j].event_id == event_id) {
                if (n == 0u) {
                    if (ilk != NULL) {
                        *ilk = i;
                    }
                    if (sonuc != NULL) {
                        *sonuc = c->evres[j].result;
                    }
                    if (t_apply != NULL) {
                        *t_apply = c->evres[j].t_apply.ns;
                    }
                }
                n++;
            }
        }
    }
    return n;
}

static void bozuk_yok(void)
{
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(0u, ko_bozuk_cerceve(), "kartın çözülemeyen çerçevesi");
}

/* Olayın ilk gönderimi: ACTUATOR'dan önce, n_events = 1, alanlar dönüşümsüz (A1-K73, K90). */
static uint64_t olay_uret(uint16_t sess, uint32_t tf_seq, uint32_t apply_by, size_t *ilk_sira)
{
    ko_olay_planla(apply_by);
    size_t g0 = ko_gunluk_boy();
    ko_tf(tf_seq);
    ko_pompala();
    uint64_t E = ko_son_olay_id();
    TEST_ASSERT_TRUE_MESSAGE(E != 0u, "önkoşul: sahte çekirdek olayı üretti");
    gonderimler_t g;
    olay_gonderimleri(g0, E, &g);
    TEST_ASSERT_EQUAL_size_t_MESSAGE(1u, g.n, "olay çıktığı çağrıda bir kez gönderilir (A1-K80)");
    mavlink_ida_event_t e;
    ko_coz(ko_gunluk(g.sira[0]), MAVLINK_MSG_ID_IDA_EVENT, &e, sizeof e);
    TEST_ASSERT_EQUAL_UINT8(IDAW_EVENT_KIND_SAFETY_CLOSE, e.kind);
    TEST_ASSERT_EQUAL_UINT8(255u, e.tube);
    TEST_ASSERT_EQUAL_UINT16(sess, e.sess);
    TEST_ASSERT_EQUAL_UINT32(tf_seq, e.batch_seq);
    TEST_ASSERT_EQUAL_UINT32(apply_by, e.apply_by_ns);
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(IDAW_CALL_KIND_TICK_TF, e.call_kind,
                                    "call_kind çağrının türü (A1-K73)");
    /* Aynı çağrının ACTUATOR'ı olaydan sonra ve n_events = 1. */
    bool act = false;
    for (size_t i = g0; i < ko_gunluk_boy(); ++i) {
        const ko_cerceve_t *f = ko_gunluk(i);
        if (f->msgid == MAVLINK_MSG_ID_IDA_ACTUATOR) {
            mavlink_ida_actuator_t a;
            ko_coz(f, MAVLINK_MSG_ID_IDA_ACTUATOR, &a, sizeof a);
            if (a.batch_seq == tf_seq &&
                ((uint32_t)a.act_flags & (uint32_t)IDAW_ACT_FLAGS_ASYNC) == 0u) {
                TEST_ASSERT_TRUE_MESSAGE(i > g.sira[0], "yeni olay ACTUATOR'dan önce gider (A1-K73, K90)");
                TEST_ASSERT_EQUAL_UINT8_MESSAGE(1u, a.n_events, "n_events (A1-K73)");
                act = true;
            }
        }
    }
    TEST_ASSERT_TRUE_MESSAGE(act, "çağrının IDA_ACTUATOR'ı gönderilmeli (A1-K45)");
    if (ilk_sira != NULL) {
        *ilk_sira = g.sira[0];
    }
    return E;
}

/* ---- Yineleme ve onay (kart rolü) ------------------------------------------------------- */
static void test_yineleme_ve_onay(void)
{
    ko_hazir(IDA_LINK_UST_ROL_KART, 0x2201u);
    size_t ilk = 0;
    uint64_t E = olay_uret(0x2201u, 2u, 200000000u, &ilk);
    int64_t te = ko_son_olay_now_w();
    const ko_cerceve_t *f0 = ko_gunluk(ilk);
    TEST_ASSERT_EQUAL_INT64_MESSAGE(te, f0->t_ns, "ilk gönderim olayı çıkaran çağrıda");

    ko_bekle_ms(70u);
    gonderimler_t g;
    olay_gonderimleri(ilk, E, &g);
    TEST_ASSERT_TRUE_MESSAGE(g.n >= 5u, "70 ms içinde en az 4 yineleme (T_olay_yin = 15 ms; A1-K80)");
    for (size_t i = 1; i < g.n; ++i) {
        int64_t ara = g.t[i] - g.t[i - 1];
        char ileti[160];
        (void)snprintf(ileti, sizeof ileti, "yineleme aralığı %lld ns, 15–16 ms beklenir (A1-K80)",
                       (long long)ara);
        TEST_ASSERT_TRUE_MESSAGE(ara >= 15 * KO_MS && ara <= 16 * KO_MS, ileti);
        const ko_cerceve_t *f = ko_gunluk(g.sira[i]);
        TEST_ASSERT_EQUAL_UINT8_MESSAGE(f0->len, f->len, "yineleme aynı yükle (A1-K80, A4-K106)");
        TEST_ASSERT_EQUAL_MEMORY_MESSAGE(f0->yuk, f->yuk, f0->len, "yineleme aynı yükle (A4-K106)");
        TEST_ASSERT_EQUAL_UINT8_MESSAGE(IDA_LINK_SINIF_ZORUNLU, f->sinif,
                                        "olay yinelemesi zorunlu-güvenilir sınıfta (A1-K110)");
    }

    /* Sonuç gelir: yineleme durur; sonuç çekirdeğe verilir. */
    size_t g1 = ko_gunluk_boy();
    size_t c1 = ko_cagri_boy();
    int64_t t_apply = ko_tf_t(2u) + 10 * KO_MS;
    ko_evres(E, IDAW_EVENT_RESULT_CODE_APPLIED, t_apply);
    ko_tf(3u);
    ko_pompala();
    uint8_t sonuc = 0;
    int64_t ta = 0;
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(1u, evres_ara(c1, E, NULL, &sonuc, &ta),
                                     "olay sonucu tam bir kez çekirdeğe verilir (A4-K68)");
    TEST_ASSERT_EQUAL_UINT8(IDA_EVR_APPLIED, sonuc);
    TEST_ASSERT_EQUAL_INT64(t_apply, ta);
    ko_bekle_ms(100u);
    olay_gonderimleri(g1, E, &g);
    TEST_ASSERT_EQUAL_size_t_MESSAGE(0u, g.n, "sonuçtan sonra olay yinelenmez (A1-K80)");
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(1u, evres_ara(c1, E, NULL, NULL, NULL),
                                     "olaya tek son sonuç (A4-K68)");
    bozuk_yok();
}

/* ---- Son geçerlilik ve EXPIRED_NO_ACK (kart rolü) -------------------------------------- */
static void test_son_gecerlilik(void)
{
    ko_hazir(IDA_LINK_UST_ROL_KART, 0x2202u);
    const uint32_t apply_by = 100000000u;
    size_t ilk = 0;
    size_t c0 = ko_cagri_boy();
    uint64_t E = olay_uret(0x2202u, 2u, apply_by, &ilk);
    int64_t te = ko_son_olay_now_w();
    ko_bekle_ms(300u);
    gonderimler_t g;
    olay_gonderimleri(ilk, E, &g);
    TEST_ASSERT_TRUE_MESSAGE(g.n >= 6u, "son geçerliliğe kadar yinelenir (A1-K80)");
    TEST_ASSERT_TRUE_MESSAGE(g.t[g.n - 1] <= te + (int64_t)apply_by,
                             "son geçerlilik dolunca yineleme durur (A1-K80)");
    size_t k = 0;
    uint8_t sonuc = 0;
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(1u, evres_ara(c0, E, &k, &sonuc, NULL),
                                     "sonuçsuz olaya tam bir son sonuç (A4-K68, K106)");
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(IDA_EVR_EXPIRED_NO_ACK, sonuc, "EXPIRED_NO_ACK (A1-K80)");
    TEST_ASSERT_TRUE_MESSAGE(ko_cagri(k)->now_w.ns >= te + (int64_t)apply_by + 95 * KO_MS,
                             "EXPIRED_NO_ACK ancak apply_by + T_sonuç_pay dolunca (A1-K80, A4-K106)");
    bozuk_yok();
}

/* ---- Oturum sonu (kart rolü, saat ilerler) --------------------------------------------- */
static void test_oturum_sonu_kart(void)
{
    ko_hazir(IDA_LINK_UST_ROL_KART, 0x2203u);
    size_t ilk = 0;
    uint64_t E = olay_uret(0x2203u, 2u, 200000000u, &ilk);
    ko_bekle_ms(20u);
    gonderimler_t g;
    olay_gonderimleri(ilk, E, &g);
    TEST_ASSERT_TRUE_MESSAGE(g.n >= 2u, "önkoşul: oturum sonundan önce en az bir yineleme");

    size_t g1 = ko_gunluk_boy();
    size_t c1 = ko_cagri_boy();
    ko_session_close(0x2203u, IDAW_CLOSE_REASON_RUN_END);
    ko_pompala();
    TEST_ASSERT_EQUAL_INT((int)IDA_LINK_UST_IDLE, (int)ida_link_ust_durum(ko_l()));
    ko_bekle_ms(30u);
    (void)ko_el_sikis(UINT32_C(0x53000002), 0x2204u);
    ko_tf(3u);
    ko_pompala();
    ko_bekle_ms(100u);
    ko_tf(4u);
    ko_pompala();

    olay_gonderimleri(g1, E, &g);
    TEST_ASSERT_EQUAL_size_t_MESSAGE(0u, g.n,
                                     "oturum sonunda olay yinelenmez, yeni oturumda iletilmez (A4-K71)");
    size_t k = 0;
    uint8_t sonuc = 0;
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(1u, evres_ara(c1, E, &k, &sonuc, NULL),
                                     "onaylanmamış olaya tam bir son sonuç (A4-K68, K71)");
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(IDA_EVR_SESSION_END, sonuc, "SESSION_END sonucu (A4-K71)");
    /* Eski oturumun kimliğini taşıyan olay çerçevesi yok. */
    for (size_t i = g1; i < ko_gunluk_boy(); ++i) {
        const ko_cerceve_t *f = ko_gunluk(i);
        if (f->msgid == MAVLINK_MSG_ID_IDA_EVENT) {
            mavlink_ida_event_t e;
            ko_coz(f, MAVLINK_MSG_ID_IDA_EVENT, &e, sizeof e);
            TEST_ASSERT_NOT_EQUAL_UINT16(0x2203u, e.sess);
        }
    }
    bozuk_yok();
}

/* ---- Oturum sonu (SIL rolü; zaman TF'lerle ilerler, A1-K121 (i)) ----------------------- */
static void test_oturum_sonu_sil(void)
{
    ko_hazir(IDA_LINK_UST_ROL_SIL, 0x2205u);
    size_t ilk = 0;
    uint64_t E = olay_uret(0x2205u, 2u, 200000000u, &ilk);
    /* Sim zamanı 20 ms ilerler: en az bir yineleme (A1-K80, K121 (i)). */
    for (uint32_t s = 3u; s <= 6u; ++s) {
        ko_tf(s);
        ko_pompala();
    }
    gonderimler_t g;
    olay_gonderimleri(ilk, E, &g);
    TEST_ASSERT_TRUE_MESSAGE(g.n >= 2u,
                             "SIL'de T_olay_yin sim zamanıyla sayılır; 20 ms'de yineleme (A1-K121)");

    size_t g1 = ko_gunluk_boy();
    size_t c1 = ko_cagri_boy();
    ko_session_close(0x2205u, IDAW_CLOSE_REASON_RUN_END);
    ko_pompala();
    (void)ko_el_sikis(UINT32_C(0x53000003), 0x2206u);
    for (uint32_t s = 7u; s <= 40u; ++s) {
        ko_tf(s);
        ko_pompala();
    }
    olay_gonderimleri(g1, E, &g);
    TEST_ASSERT_EQUAL_size_t_MESSAGE(0u, g.n,
                                     "oturum sonunda olay yinelenmez, yeni oturumda iletilmez (A4-K71)");
    uint8_t sonuc = 0;
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(1u, evres_ara(c1, E, NULL, &sonuc, NULL),
                                     "onaylanmamış olaya tam bir son sonuç (A4-K68, K71)");
    TEST_ASSERT_EQUAL_UINT8(IDA_EVR_SESSION_END, sonuc);
    bozuk_yok();
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_yineleme_ve_onay);
    RUN_TEST(test_son_gecerlilik);
    RUN_TEST(test_oturum_sonu_kart);
    RUN_TEST(test_oturum_sonu_sil);
    return UNITY_END();
}
