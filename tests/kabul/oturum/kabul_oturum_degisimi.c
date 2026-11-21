/*
 * Oturum değişimindeki temizlik kabul testleri (W3-oturum: kabul md.2 ve md.4'ün oturum
 * değişimi bölümü).
 *
 * Dayanılan kurallar: A1-K22 (bitişte (1) gönderim kuyruğu, ertelenmiş komutlar, açık parti
 * silinir, mandallar silinmez; (2) oturum sonu çağrısı; (3) bu çağrının IDA_CALL_TRACE'i;
 * (4) IDLE), A1-K107 (oturum sonu çağrısı: tf.present = 0, örneksiz, komutsuz,
 * link_event = SESSION_END; dönene kadar yeni SIM_HELLO yanıtlanmaz ve yeni oturumun girdisi
 * çekirdeğe verilmez), A1-K11 (IDLE'da yalnız BOARD_HELLO; oturum sonu izi istisna), A1-K13
 * (yeni nonce'lu SIM_HELLO oturumu bitirir), A1-K16 (SESSION_START yeni oturumun ilk
 * çağrısında, oturum sonu çağrısından sonra), A1-K25 ve Tablo 3.11b #11 (eski sess'li çerçeve
 * atılır, rx_stale_session), A4-K67 (oturum RUNNING değilken çıkan olay taşınmaz, sonucu
 * SESSION_END), A4-K68, A4-K74 (eski oturumun hiçbir başka girdisi çekirdeğe verilmez; mandal
 * bir sonraki çağrıda verilir), A4-K122.
 *
 * W3-K10 gereği take ile put_out arasında rx çağrılmaz; oturum sonu çağrı yokken saptanır.
 */
#include <stdio.h>
#include <string.h>

#include "kabul_oturum.h"
#include "unity.h"

void setUp(void) {}
void tearDown(void) {}

static void bozuk_yok(void)
{
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(0u, ko_bozuk_cerceve(), "kartın çözülemeyen çerçevesi");
}

static void hic_girmedi(uint32_t id)
{
    char ileti[160];
    (void)snprintf(ileti, sizeof ileti,
                   "eski oturumun komutu %u yeni çağrılara girmemeli (A4-K74, A1-K22)", (unsigned)id);
    TEST_ASSERT_EQUAL_UINT16_MESSAGE(0u, ko_kimlik(id)->girdi, ileti);
}

static uint32_t stale_sayaci(void)
{
    uint32_t cnt[IDA_LINK_SAY_N];
    memset(cnt, 0, sizeof cnt);
    ida_link_ust_sayaclar(ko_l(), cnt);
    return cnt[IDA_LINK_SAY_RX_STALE_SESSION];
}

/* Oturum sonu çağrısının girdisi (A1-K107, A4-K122). */
static void oturum_sonu_cagrisi(const ko_cagri_t *c)
{
    TEST_ASSERT_EQUAL_INT_MESSAGE((int)IDA_LINK_CAGRI_OTURUM_SONU, (int)c->tur,
                                  "oturum sonundan sonraki ilk çağrı oturum sonu çağrısı (A1-K107)");
    TEST_ASSERT_FALSE_MESSAGE(c->tf.present, "oturum sonu çağrısında tf.present = 0 (A1-K107)");
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(0u, c->n_samples, "örneksiz (A1-K107)");
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(0u, c->n_cmds, "komutsuz (A1-K107)");
    TEST_ASSERT_FALSE_MESSAGE(c->ping360_var, "ping360 NULL (A4-K122)");
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(IDA_LINKEV_SESSION_END, c->link_event, "link_event (A1-K107)");
}

/* ---- Bütün bekleme noktalarında eski oturum girdileri silinir ---------------------------- */
static void temizlik(ida_link_ust_rol_t rol)
{
    const uint16_t s1 = 0x3301u;
    const uint16_t s2 = 0x3302u;
    ko_hazir(rol, s1);

    /* Erteleme tamponu: 4 + 4 + 2 komutlu üç parti birleşir; ilk 8 çağrıya girer, 398 ve 399
       ertelenir (A4-K102, K105). */
    for (uint32_t i = 0; i < 4u; ++i) {
        ko_simple(390u + i, IDAW_CMD_TYPE_HEARTBEAT, 0u);
    }
    ko_tf(2u);
    for (uint32_t i = 4; i < 8u; ++i) {
        ko_simple(390u + i, IDAW_CMD_TYPE_HEARTBEAT, 0u);
    }
    ko_tf(3u);
    ko_simple(398u, IDAW_CMD_TYPE_ARM, 0u);
    ko_simple(399u, IDAW_CMD_TYPE_HEARTBEAT, 0u);
    ko_tf(4u);
    TEST_ASSERT_TRUE(ko_tek_cagri() >= 0);
    TEST_ASSERT_EQUAL_UINT16_MESSAGE(0u, ko_kimlik(398u)->girdi, "önkoşul: 398 ertelendi");
    TEST_ASSERT_EQUAL_UINT16_MESSAGE(0u, ko_kimlik(399u)->girdi, "önkoşul: 399 ertelendi");

    /* Bekleyen parti (kapanmış, take edilmemiş) ve açık parti. */
    ko_simple(400u, IDAW_CMD_TYPE_ARM, 0u);
    ko_simple(401u, IDAW_CMD_TYPE_HEARTBEAT, 0u);
    ko_tf(5u);
    ko_simple(402u, IDAW_CMD_TYPE_HEARTBEAT, 0u);
    ko_fc(403u, IDAW_FC_OP_ARM, 0u, 0u);

    ko_oturum_sonu_olayi(true);
    size_t g0 = ko_gunluk_boy();
    size_t c0 = ko_cagri_boy();
    uint32_t kt0 = ko_kuyruk_temizle_sayisi();
    ko_session_close(s1, IDAW_CLOSE_REASON_RUN_END);
    ko_pompala();
    ko_oturum_sonu_olayi(false);

    TEST_ASSERT_EQUAL_INT_MESSAGE((int)IDA_LINK_UST_IDLE, (int)ida_link_ust_durum(ko_l()),
                                  "oturum sonu çağrısı dönünce IDLE (A1-K22 (4))");
    TEST_ASSERT_EQUAL_UINT16_MESSAGE(0u, ida_link_ust_sess(ko_l()), "oturum yokken sess 0 (ust.h)");
    TEST_ASSERT_TRUE_MESSAGE(ko_kuyruk_temizle_sayisi() > kt0,
                             "gönderim kuyruğu temizlenir (A1-K22 (1))");
    TEST_ASSERT_TRUE_MESSAGE(ko_cagri_boy() > c0, "oturum sonu çağrısı yapılmalı (A1-K107)");
    size_t k_sonu = c0;
    oturum_sonu_cagrisi(ko_cagri(k_sonu));
    for (size_t i = c0; i < ko_cagri_boy(); ++i) {
        const ko_cagri_t *c = ko_cagri(i);
        TEST_ASSERT_FALSE_MESSAGE(c->tf.present && c->tf.seq == 5u,
                                  "eski oturumun bekleyen partisi çağrılmaz (A1-K22, A4-K74)");
        if (i > k_sonu) {
            TEST_ASSERT_NOT_EQUAL_INT((int)IDA_LINK_CAGRI_OTURUM_SONU, (int)c->tur);
        }
    }
    /* Oturum bitince yalnız oturum sonu izi (call_kind = SESSION_END) ve duyuru gider; oturum
       sonu çağrısının olayı oturum RUNNING olmadığı için taşınmaz (A1-K11, K22, A4-K67). */
    uint32_t iz = 0;
    for (size_t i = g0; i < ko_gunluk_boy(); ++i) {
        const ko_cerceve_t *f = ko_gunluk(i);
        if (f->msgid == MAVLINK_MSG_ID_IDA_CALL_TRACE) {
            mavlink_ida_call_trace_t t;
            ko_coz(f, MAVLINK_MSG_ID_IDA_CALL_TRACE, &t, sizeof t);
            TEST_ASSERT_EQUAL_UINT8_MESSAGE(IDAW_CALL_KIND_SESSION_END, t.call_kind,
                                            "oturum bitince yalnız oturum sonu izi (A1-K11, K22 (3))");
            iz++;
        } else {
            char ileti[160];
            (void)snprintf(ileti, sizeof ileti,
                           "oturum bitince gönderilen beklenmeyen çerçeve: msgid %u (A1-K11, K22)",
                           (unsigned)f->msgid);
            TEST_ASSERT_EQUAL_UINT32_MESSAGE(MAVLINK_MSG_ID_IDA_BOARD_HELLO, f->msgid, ileti);
        }
    }
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(1u, iz, "oturum sonu çağrısının izi bir kez (A1-K22 (3), K95)");
    uint64_t E_sonu = ko_son_olay_id();
    TEST_ASSERT_TRUE_MESSAGE(E_sonu != 0u, "önkoşul: oturum sonu çağrısında olay üretildi");

    /* Yeni oturum. */
    size_t c1 = ko_cagri_boy();
    (void)ko_el_sikis(UINT32_C(0x54000001), s2);
    size_t c2 = ko_cagri_boy();
    for (size_t i = c1; i < c2; ++i) {
        TEST_ASSERT_NOT_EQUAL_UINT8(IDA_LINKEV_SESSION_START, ko_cagri(i)->link_event);
    }
    ko_tf(6u);
    ko_pompala();
    TEST_ASSERT_TRUE(ko_cagri_boy() > c2);
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(IDA_LINKEV_SESSION_START, ko_cagri(c2)->link_event,
                                    "yeni oturumun ilk çağrısında SESSION_START (A1-K16)");

    /* Eski sess'li çerçeveler (A1-K25). */
    uint32_t st0 = stale_sayaci();
    uint64_t E_eski = ((uint64_t)KO_BOOT_COUNT << 32) | UINT64_C(999);
    ko_tf_s(s1, 7u);
    ko_simple_s(s1, 404u, IDAW_CMD_TYPE_ARM, 0u);
    ko_simple_s(s1, 405u, IDAW_CMD_TYPE_HEARTBEAT, 0u);
    ko_fc_s(s1, 406u, IDAW_FC_OP_ABORT, 255u, 0u);
    ko_evres_s(s1, E_eski, IDAW_EVENT_RESULT_CODE_APPLIED, 1);
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(5u, stale_sayaci() - st0,
                                     "eski sess'li her çerçeve rx_stale_session'a (A1-K25)");
    ko_tf(7u);
    ko_pompala();
    ko_tf(8u);
    ko_pompala();
    for (size_t i = c2 + 1u; i < ko_cagri_boy(); ++i) {
        TEST_ASSERT_NOT_EQUAL_UINT8_MESSAGE(IDA_LINKEV_SESSION_START, ko_cagri(i)->link_event,
                                            "SESSION_START tek çağrıda (A4-K72)");
    }

    const uint32_t eski[] = {398u, 399u, 400u, 401u, 402u, 403u, 404u, 405u, 406u};
    for (size_t i = 0; i < sizeof eski / sizeof eski[0]; ++i) {
        hic_girmedi(eski[i]);
    }
    /* Eski olay sonucu çekirdeğe verilmez; oturum sonu çağrısının olayı tek SESSION_END sonucu
       alır (A4-K67, K68) ve hiç gönderilmez. */
    uint32_t n_eski = 0;
    uint32_t n_sonu = 0;
    for (size_t i = c0; i < ko_cagri_boy(); ++i) {
        const ko_cagri_t *c = ko_cagri(i);
        uint8_t m = (uint8_t)(c->n_evres <= IDA_PORT_MAX_EVRES ? c->n_evres : IDA_PORT_MAX_EVRES);
        for (uint8_t j = 0; j < m; ++j) {
            if (c->evres[j].event_id == E_eski) {
                n_eski++;
            }
            if (c->evres[j].event_id == E_sonu) {
                TEST_ASSERT_TRUE_MESSAGE(i > k_sonu, "sonuç olayı çıkaran çağrıdan sonra verilir");
                TEST_ASSERT_EQUAL_UINT8_MESSAGE(IDA_EVR_SESSION_END, c->evres[j].result,
                                                "oturumsuz çıkan olayın sonucu SESSION_END (A4-K67)");
                n_sonu++;
            }
        }
    }
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(0u, n_eski, "eski sess'li olay sonucu çekirdeğe verilmez (A1-K25)");
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(1u, n_sonu, "olaya tam bir son sonuç (A4-K68)");
    for (size_t i = g0; i < ko_gunluk_boy(); ++i) {
        const ko_cerceve_t *f = ko_gunluk(i);
        if (f->msgid == MAVLINK_MSG_ID_IDA_EVENT) {
            mavlink_ida_event_t e;
            ko_coz(f, MAVLINK_MSG_ID_IDA_EVENT, &e, sizeof e);
            TEST_ASSERT_TRUE_MESSAGE(e.event_id != E_sonu, "oturumsuz olay taşınmaz (A4-K67)");
            TEST_ASSERT_NOT_EQUAL_UINT16(s1, e.sess);
        }
        if (f->msgid == MAVLINK_MSG_ID_IDA_CMD_RESULT) {
            mavlink_ida_cmd_result_t r;
            ko_coz(f, MAVLINK_MSG_ID_IDA_CMD_RESULT, &r, sizeof r);
            TEST_ASSERT_NOT_EQUAL_UINT16_MESSAGE(s1, r.sess, "eski oturuma sonuç gönderilmez");
        }
    }
    bozuk_yok();
}

/* ---- Mandal oturum sonunda silinmez, bir sonraki çağrıda verilir (A4-K74, A1-K22 (1)) --- */
static void mandal_korunur(ida_link_ust_rol_t rol)
{
    ko_hazir(rol, 0x3303u);
    ko_simple(410u, IDAW_CMD_TYPE_ARM, 0u);
    ko_tf(2u);
    ko_simple(411u, IDAW_CMD_TYPE_DISARM, 0u);
    size_t c0 = ko_cagri_boy();
    ko_session_close(0x3303u, IDAW_CLOSE_REASON_RUN_END);
    ko_pompala();
    TEST_ASSERT_TRUE(ko_cagri_boy() > c0);
    const ko_cagri_t *c = ko_cagri(c0);
    oturum_sonu_cagrisi(c);
    TEST_ASSERT_TRUE_MESSAGE((c->narrow.mask & IDA_NARROW_DISARM) != 0u && c->narrow.cmd_id[0] == 411u,
                             "mandal oturum sonu çağrısında verilir (A4-K74, A1-K22 (1))");
    TEST_ASSERT_EQUAL_UINT16_MESSAGE(0u, ko_kimlik(410u)->girdi, "atılan ARM çağrıya girmez");
    (void)ko_el_sikis(UINT32_C(0x54000002), 0x3304u);
    ko_tf(3u);
    ko_pompala();
    TEST_ASSERT_EQUAL_UINT16(0u, ko_kimlik(410u)->girdi);
    TEST_ASSERT_EQUAL_UINT16_MESSAGE(1u, ko_kimlik(411u)->girdi, "mandal bir kez verilir (A4-K46)");
    bozuk_yok();
}

/* ---- Yeni nonce'lu SIM_HELLO ile bitiş (A1-K13, K107, H42) ------------------------------ */
static void yeni_hello(ida_link_ust_rol_t rol)
{
    ko_hazir(rol, 0x3305u);
    ko_simple(420u, IDAW_CMD_TYPE_HEARTBEAT, 0u);
    ko_tf(2u);
    ko_simple(421u, IDAW_CMD_TYPE_ARM, 0u);
    const uint32_t n2 = UINT32_C(0x55000001);
    size_t g0 = ko_gunluk_boy();
    size_t c0 = ko_cagri_boy();
    ko_sim_hello(n2);
    TEST_ASSERT_FALSE_MESSAGE(ko_board_hello_bul(g0, n2, NULL, NULL),
                              "oturum sonu çağrısı dönmeden SIM_HELLO yanıtlanmaz (A1-K13, K107)");
    ko_pompala();
    TEST_ASSERT_TRUE(ko_cagri_boy() > c0);
    const ko_cagri_t *c = ko_cagri(c0);
    oturum_sonu_cagrisi(c);
    mavlink_ida_board_hello_t bh;
    size_t sira = 0;
    TEST_ASSERT_TRUE_MESSAGE(ko_board_hello_bul(g0, n2, &bh, &sira),
                             "oturum sonu çağrısından sonra SIM_HELLO yanıtlanır (A1-K107)");
    TEST_ASSERT_TRUE_MESSAGE(sira >= c->gunluk_put,
                             "yanıt oturum sonu çağrısı döndükten sonra (A1-K107, H42)");
    ko_session_accept(n2, bh.nonce, 0x3306u, (uint8_t)IDAW_SESSION_RESULT_OK);
    TEST_ASSERT_EQUAL_INT((int)IDA_LINK_UST_RUNNING, (int)ida_link_ust_durum(ko_l()));
    size_t c1 = ko_cagri_boy();
    for (size_t i = c0; i < ko_cagri_boy(); ++i) {
        TEST_ASSERT_NOT_EQUAL_UINT8(IDA_LINKEV_SESSION_START, ko_cagri(i)->link_event);
    }
    /* ko_tf güncel sess'i kullanır; düzenek el sıkışmayı ko_el_sikis dışında yaptı. */
    ko_tf_s(0x3306u, 3u);
    ko_pompala();
    TEST_ASSERT_TRUE(ko_cagri_boy() > c1);
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(IDA_LINKEV_SESSION_START, ko_cagri(c1)->link_event,
                                    "SESSION_START oturum sonu çağrısından sonra (A1-K16)");
    TEST_ASSERT_EQUAL_UINT16(0u, ko_kimlik(420u)->girdi);
    TEST_ASSERT_EQUAL_UINT16(0u, ko_kimlik(421u)->girdi);
    for (size_t i = c0; i < ko_cagri_boy(); ++i) {
        TEST_ASSERT_FALSE_MESSAGE(ko_cagri(i)->tf.present && ko_cagri(i)->tf.seq == 2u,
                                  "eski oturumun bekleyen partisi çağrılmaz (A4-K74)");
    }
    bozuk_yok();
}

#define IKI_ROL(ad)                                                                          \
    static void test_##ad##_kart(void) { ad(IDA_LINK_UST_ROL_KART); }                         \
    static void test_##ad##_sil(void) { ad(IDA_LINK_UST_ROL_SIL); }

IKI_ROL(temizlik)
IKI_ROL(mandal_korunur)
IKI_ROL(yeni_hello)

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_temizlik_kart);
    RUN_TEST(test_temizlik_sil);
    RUN_TEST(test_mandal_korunur_kart);
    RUN_TEST(test_mandal_korunur_sil);
    RUN_TEST(test_yeni_hello_kart);
    RUN_TEST(test_yeni_hello_sil);
    return UNITY_END();
}
