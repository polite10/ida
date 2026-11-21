/*
 * test_dayaniklilik.c — dayanıklılık (W3-oturum kabul md. 5): rastgele ve rastgele bozulmuş
 * bayt akışında (sabit tohum) modül çökmez, bellek dışına yazmaz (host-asan ve UBSan
 * derlemelerinde) ve geçersiz girdiden çekirdek çağrısı üretmez: her zaman çerçeveli çağrının
 * zaman çerçevesi sim'in gerçekten gönderdiği bir TF'dir; her çağrı girdisi A4 kapasitelerine
 * uyar.
 */
#include "destek_oturum.h"

#include "unity.h"

#include <string.h>

void setUp(void);
void tearDown(void);
void setUp(void) { dt_kur(IDA_LINK_UST_ROL_KART); }
void tearDown(void) {}

typedef struct {
    uint32_t s;
} rastgele_t;

static uint32_t rastgele(rastgele_t *r)
{
    uint32_t x = r->s;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    r->s = x;
    return x;
}

static void girdi_degismezleri(const ida_in_t *in, ida_link_cagri_t tur)
{
    TEST_ASSERT_TRUE(in->n_samples <= IDA_PORT_MAX_SAMPLES);
    TEST_ASSERT_TRUE(in->n_cmds <= IDA_PORT_MAX_CMDS);
    TEST_ASSERT_TRUE(in->n_evres <= IDA_PORT_MAX_EVRES);
    TEST_ASSERT_TRUE(in->n_det <= IDA_PORT_MAX_DET_IN);
    TEST_ASSERT_TRUE(in->n_det_batch <= IDA_PORT_MAX_DET_BATCH);
    TEST_ASSERT_TRUE(in->plat.link_event <= IDA_LINKEV_SESSION_END);
    TEST_ASSERT_TRUE(in->plat.link_state <= IDA_LINK_RUNNING);
    TEST_ASSERT_EQUAL(tur == IDA_LINK_CAGRI_STEP || tur == IDA_LINK_CAGRI_TICK_TF, in->tf.present);
    TEST_ASSERT_EQUAL(tur == IDA_LINK_CAGRI_STEP, in->imu_present);
    TEST_ASSERT_EQUAL_UINT8(0u, in->narrow.mask & (uint8_t)~IDA_NARROW_DEFINED);
    {
        bool emn_bitti = false;
        for (unsigned i = 0; i < in->n_samples; ++i) {
            const bool emn = in->samples[i].hdr.type == IDA_SENS_SAFETY;
            TEST_ASSERT_FALSE_MESSAGE(emn && emn_bitti, "emniyet örneği başta değil");
            emn_bitti = emn_bitti || !emn;
        }
    }
    if (in->ping360 != NULL) {
        TEST_ASSERT_TRUE(in->ping360->n_samples >= 200u && in->ping360->n_samples <= 1200u);
    }
}

static void cagri_yap(void)
{
    ida_in_t in;
    const ida_link_cagri_t tur = dt_take(&in);
    if (tur == IDA_LINK_CAGRI_YOK) {
        return;
    }
    girdi_degismezleri(&in, tur);
    dt_put(&in, tur, IDA_RC_OK, NULL, 0u);
}

/* Tamamen rastgele baytlar: oturum kurulamaz, zaman çerçeveli çağrı olmaz. */
static void test_rastgele_baytlar(void)
{
    rastgele_t r = {0x12345678u};
    uint8_t buf[300];
    for (unsigned tur = 0; tur < 4000u; ++tur) {
        const size_t n = (size_t)(rastgele(&r) % sizeof buf);
        ida_in_t in;
        ida_link_cagri_t c;
        ida_wtime_t t;
        for (size_t i = 0; i < n; ++i) {
            buf[i] = (uint8_t)rastgele(&r);
            if ((rastgele(&r) & 31u) == 0u) {
                buf[i] = 0xFDu;   /* aday başlangıçları sık olsun */
            }
        }
        t.ns = DT.saat;
        ida_link_ust_rx(&DT.l, buf, n, t);
        if ((rastgele(&r) & 7u) == 0u) {
            ida_link_ust_rx_tasma(&DT.l);
        }
        dt_ilerle((int64_t)(rastgele(&r) % 3000000u));
        ida_link_ust_poll_tx(&DT.l);
        c = dt_take(&in);
        TEST_ASSERT_TRUE(c == IDA_LINK_CAGRI_YOK || c == IDA_LINK_CAGRI_IZGARA);
        if (c != IDA_LINK_CAGRI_YOK) {
            girdi_degismezleri(&in, c);
            dt_put(&in, c, IDA_RC_OK, NULL, 0u);
        }
        TEST_ASSERT_NOT_EQUAL(IDA_LINK_UST_RUNNING, ida_link_ust_durum(&DT.l));
        if (DT.n > DT_KAYIT - 16u) {
            dt_kayit_sil();
        }
    }
}

/* Geçerli bir oturumun akışı rastgele bozulur: çağrılar yalnız gönderilmiş TF'lerden. */
static uint32_t gonderilen_tf[4096];
static int64_t gonderilen_t[4096];
static unsigned n_gonderilen_tf;

static size_t kodla(uint32_t msgid, const void *m, size_t boy, uint8_t *o, size_t kap)
{
    size_t n = 0u;
    TEST_ASSERT_EQUAL_INT(IDA_LINK_TAMAM, ida_link_gonder(&DT.sim_g, msgid, m, boy, o, kap, &n));
    return n;
}

static void test_bozulmus_oturum_akisi(void)
{
    rastgele_t r = {0x0BADC0DEu};
    uint8_t akis[2048];
    dt_el_sikis(0x1234u, 4242u);
    n_gonderilen_tf = 0u;
    for (uint32_t seq = 1u; seq <= 3000u; ++seq) {
        size_t n = 0u;
        const int64_t t = (int64_t)seq * 5 * DT_MS;
        mavlink_ida_imu_t imu;
        mavlink_ida_safety_t emn;
        mavlink_ida_cmd_simple_t cmd;
        mavlink_ida_time_frame_t tf;
        memset(&imu, 0, sizeof imu);
        imu.t_meas_ns = t;
        imu.seq = seq;
        imu.window_ns = 5000000u;
        imu.flags = (uint16_t)IDA_HF_VALID;
        n += kodla(MAVLINK_MSG_ID_IDA_IMU, &imu, sizeof imu, akis + n, sizeof akis - n);
        memset(&emn, 0, sizeof emn);
        emn.t_meas_ns = t;
        emn.seq = seq;
        emn.safety_rev = 1u;
        emn.flags = (uint16_t)IDA_HF_VALID;
        n += kodla(MAVLINK_MSG_ID_IDA_SAFETY, &emn, sizeof emn, akis + n, sizeof akis - n);
        memset(&cmd, 0, sizeof cmd);
        cmd.cmd_id = seq;
        cmd.sess = 4242u;
        cmd.type = (uint8_t)(1u + rastgele(&r) % 16u);   /* geçerli ve geçersiz türler */
        cmd.arg = (uint16_t)(rastgele(&r) % 10u);
        n += kodla(MAVLINK_MSG_ID_IDA_CMD_SIMPLE, &cmd, sizeof cmd, akis + n, sizeof akis - n);
        memset(&tf, 0, sizeof tf);
        tf.t_sim_ns = t;
        tf.batch_seq = (rastgele(&r) % 50u) == 0u ? seq - 1u : seq;   /* ara sıra DUP */
        tf.sess = 4242u;
        tf.n_members = 3u;
        tf.tf_flags = (uint8_t)IDAW_TF_FLAGS_IMU;
        n += kodla(MAVLINK_MSG_ID_IDA_TIME_FRAME, &tf, sizeof tf, akis + n, sizeof akis - n);
        if (n_gonderilen_tf < 4096u) {
            gonderilen_tf[n_gonderilen_tf] = tf.batch_seq;
            gonderilen_t[n_gonderilen_tf] = tf.t_sim_ns;
            n_gonderilen_tf += 1u;
        }
        /* Bozma: bayt çevirme, bayt silme ve çöp ekleme yerine bayt değiştirme. */
        for (size_t i = 0; i < n; ++i) {
            if ((rastgele(&r) % 400u) == 0u) {
                akis[i] = (uint8_t)rastgele(&r);
            }
        }
        {
            size_t k = 0u;
            while (k < n) {
                size_t parca = 1u + (size_t)(rastgele(&r) % 64u);
                ida_wtime_t trx;
                if (parca > n - k) {
                    parca = n - k;
                }
                trx.ns = DT.saat;
                ida_link_ust_rx(&DT.l, akis + k, parca, trx);
                k += parca;
            }
        }
        dt_ilerle(5 * DT_MS);
        ida_link_ust_poll_tx(&DT.l);
        {
            ida_in_t in;
            const ida_link_cagri_t c = dt_take(&in);
            if (c != IDA_LINK_CAGRI_YOK) {
                girdi_degismezleri(&in, c);
                if (in.tf.present) {
                    bool bulundu = false;
                    for (unsigned i = 0; i < n_gonderilen_tf && !bulundu; ++i) {
                        bulundu = gonderilen_tf[i] == in.tf.seq && gonderilen_t[i] == in.tf.t.ns;
                    }
                    TEST_ASSERT_TRUE_MESSAGE(bulundu, "gönderilmemiş TF'den çağrı");
                }
                dt_put(&in, c, IDA_RC_OK, NULL, 0u);
            }
        }
        if (DT.n > DT_KAYIT - 64u) {
            dt_kayit_sil();
        }
        if (ida_link_ust_durum(&DT.l) != IDA_LINK_UST_RUNNING) {
            cagri_yap();   /* bozulma oturumu bitirdiyse oturum sonu çağrısı */
            break;
        }
    }
    {
        uint32_t cnt[IDA_LINK_SAY_N];
        ida_link_ust_sayaclar(&DT.l, cnt);
        TEST_ASSERT_TRUE(cnt[IDA_LINK_SAY_RX_CRC_ERR] > 0u);   /* bozulma gerçekten oldu */
        TEST_ASSERT_TRUE(cnt[IDA_LINK_SAY_BATCH_CLOSED] > 100u);
    }
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_rastgele_baytlar);
    RUN_TEST(test_bozulmus_oturum_akisi);
    return UNITY_END();
}
