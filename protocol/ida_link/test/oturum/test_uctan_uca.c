/*
 * test_uctan_uca.c — modül + gerçek çekirdek iskeleti (W3-oturum kabul md. 6): bir oturum
 * boyunca (ve oturum sonu ile yeniden kurulumda) üretilen her çağrı girdisi çekirdekçe
 * IDA_RC_OK ile kabul edilir.
 *
 * Çekirdek örneği ida_core_info'nun bildirdiği boy ve hizalamayla statik bellekte kurulur
 * (çekirdeğin iç başlıkları içerilmez).
 */
#include "destek_oturum.h"

#include "unity.h"

#include <string.h>

void setUp(void);
void tearDown(void);
void setUp(void) {}
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
    ii.reset_cause = (uint8_t)IDA_RESET_POWER_ON;
    ii.param_src = (uint8_t)IDA_PSRC_NONE;
    TEST_ASSERT_EQUAL_INT(IDA_RC_PARAM_DEFAULTS, ida_core_init(c, &ii));
    return c;
}

static unsigned n_cagri[5];

/* Bütün bekleyen çağrıları gerçek çekirdekle yapar; her dönüş OK olmalı. */
static void cagrilari_yap(ida_core_t *c)
{
    for (;;) {
        ida_in_t in;
        ida_out_t out;
        ida_rc_t rc;
        const ida_link_cagri_t tur = dt_take(&in);
        if (tur == IDA_LINK_CAGRI_YOK) {
            return;
        }
        in.plat.param_store = (uint8_t)IDA_PSTORE_IDLE;   /* platformun alanları */
        rc = tur == IDA_LINK_CAGRI_STEP ? ida_core_step(c, &in, &out)
                                        : ida_core_tick(c, &in, &out);
        TEST_ASSERT_EQUAL_INT_MESSAGE(IDA_RC_OK, rc, "çekirdek çağrıyı kabul etmedi");
        n_cagri[tur] += 1u;
        ida_link_ust_put_out(&DT.l, &in, rc, &out);
    }
}

static void parti_gonder(uint32_t seq, uint32_t *cmd_id)
{
    const int64_t t = (int64_t)seq * 5 * DT_MS;
    uint8_t n = 0u;
    dt_imu(seq, t);
    n++;
    if (seq % 20u == 0u) {
        dt_emniyet(seq / 20u, t, 1u, 1u);   /* acil durdurma etkin: kısıtlayan */
        n++;
    }
    if (seq % 4u == 0u) {
        dt_mag(seq / 4u, t - 1 * DT_MS);
        n++;
    }
    if (seq % 50u == 7u) {
        dt_cmd_simple((*cmd_id)++, (uint8_t)IDA_CMD_PARAM_GET, 0x0101u);
        dt_cmd_simple((*cmd_id)++, (uint8_t)IDA_CMD_ARM, 0u);   /* çekirdek reddeder */
        dt_cmd_simple((*cmd_id)++, (uint8_t)IDA_CMD_DISARM, 0u);
        n = (uint8_t)(n + 3u);
    }
    dt_tf(seq, t, n, true);
}

static void test_oturum_boyunca_ok(void)
{
    ida_core_t *c;
    uint32_t cmd_id = 1u;
    uint32_t seq = 1u;
    dt_kur(IDA_LINK_UST_ROL_KART);
    c = cekirdek_kur();
    DT.ayar.init_rc = (uint8_t)IDA_RC_PARAM_DEFAULTS;
    TEST_ASSERT_EQUAL_INT(IDA_LINK_TAMAM, ida_link_ust_kur(&DT.l, &DT.ayar, &DT.plat));
    memset(n_cagri, 0, sizeof n_cagri);
    /* Oturumsuz ızgara tick'leri. */
    for (unsigned i = 0; i < 5u; ++i) {
        dt_ilerle(10 * DT_MS);
        ida_link_ust_poll_tx(&DT.l);
        cagrilari_yap(c);
    }
    dt_el_sikis(0xA1u, 31u);
    for (; seq <= 400u; ++seq) {
        parti_gonder(seq, &cmd_id);
        cagrilari_yap(c);
        dt_ilerle(5 * DT_MS);
        ida_link_ust_poll_tx(&DT.l);
        if (seq == 200u) {
            /* TF gecikmesi: ızgara tick'leri araya girer. */
            for (unsigned k = 0; k < 3u; ++k) {
                dt_ilerle(10 * DT_MS);
                cagrilari_yap(c);
            }
        }
        if (DT.n > DT_KAYIT - 64u) {
            dt_kayit_sil();
        }
    }
    TEST_ASSERT_TRUE(n_cagri[IDA_LINK_CAGRI_STEP] == 400u);
    TEST_ASSERT_TRUE(n_cagri[IDA_LINK_CAGRI_IZGARA] >= 8u);
    TEST_ASSERT_EQUAL_UINT8(IDA_RC_OK, ((const mavlink_ida_actuator_t *)dt_son(
                                           MAVLINK_MSG_ID_IDA_ACTUATOR))->core_rc);
    /* Oturum sonu ve yeniden kurulum; yeni oturumun ilk TF'si her batch_seq ile. */
    {
        mavlink_ida_session_close_t sc;
        memset(&sc, 0, sizeof sc);
        sc.sess = 31u;
        DT_GONDER(MAVLINK_MSG_ID_IDA_SESSION_CLOSE, sc);
    }
    cagrilari_yap(c);
    TEST_ASSERT_EQUAL_UINT(1u, n_cagri[IDA_LINK_CAGRI_OTURUM_SONU]);
    dt_el_sikis(0xA2u, 32u);
    for (seq = 5000u; seq < 5100u; ++seq) {
        parti_gonder(seq, &cmd_id);
        cagrilari_yap(c);
        dt_ilerle(5 * DT_MS);
        ida_link_ust_poll_tx(&DT.l);
        if (DT.n > DT_KAYIT - 64u) {
            dt_kayit_sil();
        }
    }
    TEST_ASSERT_TRUE(n_cagri[IDA_LINK_CAGRI_STEP] == 500u);
}

/* SIL rolü: now_w = tf.t, ızgara yok; aynı sonuç. */
static void test_sil_ok(void)
{
    ida_core_t *c;
    uint32_t cmd_id = 1u;
    dt_kur(IDA_LINK_UST_ROL_SIL);
    DT.saat = 0;
    c = cekirdek_kur();
    DT.ayar.init_rc = (uint8_t)IDA_RC_PARAM_DEFAULTS;
    TEST_ASSERT_EQUAL_INT(IDA_LINK_TAMAM, ida_link_ust_kur(&DT.l, &DT.ayar, &DT.plat));
    memset(n_cagri, 0, sizeof n_cagri);
    dt_el_sikis(0xB1u, 41u);
    for (uint32_t seq = 1u; seq <= 200u; ++seq) {
        DT.saat = (int64_t)seq * 5 * DT_MS;   /* SIL saati sim zamanından (A1-K121) */
        parti_gonder(seq, &cmd_id);
        cagrilari_yap(c);
        if (DT.n > DT_KAYIT - 64u) {
            dt_kayit_sil();
        }
    }
    TEST_ASSERT_EQUAL_UINT(200u, n_cagri[IDA_LINK_CAGRI_STEP]);
    TEST_ASSERT_EQUAL_UINT(0u, n_cagri[IDA_LINK_CAGRI_IZGARA]);
    ida_link_ust_akis_kapandi(&DT.l);
    cagrilari_yap(c);
    TEST_ASSERT_EQUAL_UINT(1u, n_cagri[IDA_LINK_CAGRI_OTURUM_SONU]);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_oturum_boyunca_ok);
    RUN_TEST(test_sil_ok);
    return UNITY_END();
}
