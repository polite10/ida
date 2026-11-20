/*
 * test_ornek.c — örnek kabul kapısı ve IMU zamanlaması (A4-K14, K30, K33, K34, K35, K37,
 * K38, K39, K40, K103, K108, K115, K128, K130).
 */
#include <string.h>

#include "destek.h"
#include "unity.h"

void setUp(void);
void tearDown(void);
void setUp(void) {}
void tearDown(void) {}

static ida_core_t C;
static ida_core_t C2;
static ida_in_t IN;
static ida_out_t OUT;
static ida_out_t OUT2;
static uint32_t g_seq;
static int64_t g_t;

static void hazirla(void)
{
    TEST_ASSERT_EQUAL(IDA_RC_OK, ds_kur(&C));
    g_seq = 0u;
    g_t = 2000 * DS_MS;
    ds_girdi(&IN, g_t, 0u, true);
}

static void ilerle(void)
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
}

static ida_rc_t step(void)
{
    ilerle();
    const ida_rc_t rc = ida_core_step(&C, &IN, &OUT);
    ds_cikti_degismezleri(rc, &IN, &OUT);
    return rc;
}

static ida_rc_t tick(void)
{
    g_seq++;
    g_t += 5 * DS_MS;
    IN.now_w.ns = g_t;
    IN.tf.seq = g_seq;
    IN.tf.t.ns = g_t;
    IN.tf.t_rx.ns = g_t;
    IN.imu_present = false;
    const ida_rc_t rc = ida_core_tick(&C, &IN, &OUT);
    ds_cikti_degismezleri(rc, &IN, &OUT);
    return rc;
}

/* Bir sonraki adımın zamanında MAG örneği. */
static void mag(uint32_t i, uint32_t seq)
{
    ds_mag(&IN.samples[i], g_t + (5 * DS_MS), seq);
    if (IN.n_samples <= i) {
        IN.n_samples = (uint8_t)(i + 1u);
    }
}

static void test_k30_tur_ve_inst(void)
{
    hazirla();
    mag(0u, 1u);
    IN.samples[0].hdr.type = 0u;
    mag(1u, 1u);
    IN.samples[1].hdr.type = 11u;
    mag(2u, 1u);
    IN.samples[2].hdr.type = (uint8_t)IDA_SENS_IMU;
    mag(3u, 1u);
    IN.samples[3].hdr.type = (uint8_t)IDA_SENS_PING360;
    mag(4u, 1u);
    IN.samples[4].hdr.inst = 4u;
    TEST_ASSERT_EQUAL(IDA_RC_OK, step());
    TEST_ASSERT_EQUAL_UINT32(5u, OUT.status.cnt.sample_rejected);
}

static void test_k33_kosullari(void)
{
    hazirla();
    mag(0u, 1u);
    IN.samples[0].hdr.flags = 0u;                 /* VALID yok */
    mag(1u, 1u);
    IN.samples[1].hdr.inst = 1u;
    IN.samples[1].u.mag.field[1] = ds_nan();           /* sonlu değil */
    mag(2u, 1u);
    IN.samples[2].hdr.inst = 2u;
    IN.samples[2].hdr.window_ns = 5000000u;       /* anlık ölçümde window 0 olmalı */
    mag(3u, 1u);
    IN.samples[3].hdr.inst = 3u;
    IN.samples[3].hdr.t_meas.ns = -1;             /* aralık dışı */
    TEST_ASSERT_EQUAL(IDA_RC_OK, step());
    TEST_ASSERT_EQUAL_UINT32(4u, OUT.status.cnt.sample_rejected);
    /* gelecek damgalar */
    IN.n_samples = 0u;
    mag(0u, 1u);
    IN.samples[0].hdr.t_rx.ns = g_t + (6 * DS_MS); /* t_rx > now_w */
    mag(1u, 1u);
    IN.samples[1].hdr.inst = 1u;
    IN.samples[1].hdr.t_meas.ns = g_t + (6 * DS_MS); /* t_meas > tf.t */
    TEST_ASSERT_EQUAL(IDA_RC_OK, step());
    TEST_ASSERT_EQUAL_UINT32(2u, OUT.status.cnt.sample_future);
    /* seq ve t_meas sırası; ilk örnek muaf (seq 0 kabul) */
    IN.n_samples = 1u;
    mag(0u, 0u);
    TEST_ASSERT_EQUAL(IDA_RC_OK, step());
    TEST_ASSERT_EQUAL_UINT32(4u, OUT.status.cnt.sample_rejected);
    mag(0u, 0u); /* yinelenen seq */
    TEST_ASSERT_EQUAL(IDA_RC_OK, step());
    TEST_ASSERT_EQUAL_UINT32(5u, OUT.status.cnt.sample_rejected);
    mag(0u, 1u);
    IN.samples[0].hdr.t_meas.ns = g_t - (20 * DS_MS); /* t_meas geri */
    TEST_ASSERT_EQUAL(IDA_RC_OK, step());
    TEST_ASSERT_EQUAL_UINT32(6u, OUT.status.cnt.sample_rejected);
    /* SAFETY: tanımsız tüp biti reddedilir */
    ds_emniyet(&IN.samples[0], g_t + (5 * DS_MS), 1u);
    IN.samples[0].u.safety.tube[0] = (uint8_t)(IN.samples[0].u.safety.tube[0] | 0x10u);
    TEST_ASSERT_EQUAL(IDA_RC_OK, step());
    TEST_ASSERT_EQUAL_UINT32(7u, OUT.status.cnt.sample_rejected);
}

static void test_k34_emniyet_esit_zaman(void)
{
    hazirla();
    IN.n_samples = 1u;
    ds_emniyet(&IN.samples[0], g_t + (5 * DS_MS), 10u);
    TEST_ASSERT_EQUAL(IDA_RC_OK, step());
    const int64_t t = IN.samples[0].hdr.t_meas.ns;
    const int64_t rx = C.orn.iz[IDA_SENS_SAFETY][0].t_rx_ns;
    ds_emniyet(&IN.samples[0], t, 11u); /* aynı t_meas, büyük seq */
    IN.samples[0].hdr.t_rx.ns = g_t + (4 * DS_MS);
    TEST_ASSERT_EQUAL(IDA_RC_OK, step());
    TEST_ASSERT_EQUAL_UINT32(0u, OUT.status.cnt.sample_rejected);
    TEST_ASSERT_EQUAL_UINT32(11u, C.orn.iz[IDA_SENS_SAFETY][0].seq);
    TEST_ASSERT_TRUE(rx == C.orn.iz[IDA_SENS_SAFETY][0].t_rx_ns); /* tazelik yenilenmedi */
    /* MAG'de eşit t_meas reddedilir */
    mag(0u, 1u);
    TEST_ASSERT_EQUAL(IDA_RC_OK, step());
    const int64_t tm = IN.samples[0].hdr.t_meas.ns;
    ds_mag(&IN.samples[0], tm, 2u);
    TEST_ASSERT_EQUAL(IDA_RC_OK, step());
    TEST_ASSERT_EQUAL_UINT32(1u, OUT.status.cnt.sample_rejected);
}

static void test_k115_yineleme_ve_celiski(void)
{
    hazirla();
    mag(0u, 1u);
    mag(1u, 1u); /* bayt bayt aynı */
    TEST_ASSERT_EQUAL(IDA_RC_OK, step());
    TEST_ASSERT_EQUAL_UINT32(1u, OUT.status.cnt.sample_dup_dropped);
    TEST_ASSERT_EQUAL_UINT32(0u, OUT.status.cnt.sample_rejected);
    TEST_ASSERT_FALSE(OUT.health.sample_conflict);
    /* aynı anahtar, farklı içerik: hepsi reddedilir; iki sıralamada aynı */
    for (uint32_t sira = 0u; sira < 2u; sira++) {
        hazirla();
        mag(0u, 1u);
        mag(1u, 1u);
        IN.samples[sira].u.mag.field[2] = 1.0e-5f;
        TEST_ASSERT_EQUAL(IDA_RC_OK, step());
        TEST_ASSERT_EQUAL_UINT32(1u, OUT.status.cnt.sample_conflict);
        TEST_ASSERT_EQUAL_UINT32(2u, OUT.status.cnt.sample_rejected);
        TEST_ASSERT_TRUE(OUT.health.sample_conflict);
        TEST_ASSERT_EQUAL_UINT32(1u, ds_gunluk_say(&OUT, (uint16_t)IDA_LOG_SAMPLE_CONFLICT));
        TEST_ASSERT_FALSE(C.orn.iz[IDA_SENS_MAG][0].var);
    }
    /* A4-K130: VALID = 0 olan eşit anahtar karşılaştırmasına girmez. */
    hazirla();
    IN.n_samples = 2u;
    ds_emniyet(&IN.samples[0], g_t + (5 * DS_MS), 11u);
    ds_emniyet(&IN.samples[1], g_t + (5 * DS_MS), 11u);
    IN.samples[1].hdr.flags = 0u;
    IN.samples[1].u.safety.estop = 1u;
    TEST_ASSERT_EQUAL(IDA_RC_OK, step());
    TEST_ASSERT_EQUAL_UINT32(0u, OUT.status.cnt.sample_conflict);
    TEST_ASSERT_EQUAL_UINT32(1u, OUT.status.cnt.sample_rejected);
    TEST_ASSERT_TRUE(C.orn.iz[IDA_SENS_SAFETY][0].var); /* geçerli olan kabul edildi */
}

/* A4-K35: aynı örneklerin her sıralaması bit düzeyinde aynı çıktı ve durum. */
static void test_k35_siradan_bagimsizlik(void)
{
    ida_sample_t kume[6];
    const int64_t t = 2005 * DS_MS;
    ds_mag(&kume[0], t - DS_MS, 1u);
    ds_mag(&kume[1], t - DS_MS, 1u); /* yineleme */
    ds_mag(&kume[2], t, 2u);
    ds_emniyet(&kume[3], t - (2 * DS_MS), 1u);
    ds_emniyet(&kume[4], t - (2 * DS_MS), 1u);
    kume[4].u.safety.tube[1] = 0u; /* çelişki */
    ds_mag(&kume[5], t - (3 * DS_MS), 1u);
    kume[5].hdr.inst = 1u;
    ds_rastgele_t r = {0xC0FFEEu};
    for (uint32_t tur = 0u; tur < 200u; tur++) {
        uint8_t sira[6] = {0u, 1u, 2u, 3u, 4u, 5u};
        for (uint32_t i = 5u; i > 0u; i--) { /* Fisher–Yates */
            const uint32_t j = ds_rastgele(&r) % (i + 1u);
            const uint8_t x = sira[i];
            sira[i] = sira[j];
            sira[j] = x;
        }
        ida_core_t *c = (tur == 0u) ? &C2 : &C;
        ida_out_t *o = (tur == 0u) ? &OUT2 : &OUT;
        TEST_ASSERT_EQUAL(IDA_RC_OK, ds_kur(c));
        ds_girdi(&IN, t, 1u, true);
        IN.n_samples = 6u;
        for (uint32_t i = 0u; i < 6u; i++) {
            IN.samples[i] = kume[sira[i]];
        }
        TEST_ASSERT_EQUAL(IDA_RC_OK, ida_core_step(c, &IN, o));
        if (tur > 0u) {
            TEST_ASSERT_EQUAL_MEMORY(&OUT2, &OUT, sizeof(OUT));
            TEST_ASSERT_EQUAL_MEMORY(&C2.orn, &C.orn, sizeof(C.orn));
            TEST_ASSERT_EQUAL_UINT8(C2.emn.kisitli, C.emn.kisitli);
        }
    }
    TEST_ASSERT_EQUAL_UINT32(1u, OUT.status.cnt.sample_dup_dropped);
    TEST_ASSERT_EQUAL_UINT32(1u, OUT.status.cnt.sample_conflict);
}

static void test_k103_imu_reddi(void)
{
    hazirla();
    TEST_ASSERT_EQUAL(IDA_RC_OK, step());
    mag(0u, 1u);
    IN.n_cmds = 1u;
    IN.cmds[0].cmd_id = 5u;
    IN.cmds[0].type = (uint8_t)IDA_CMD_ARM;
    ilerle();
    IN.imu.imu.accel[1] = ds_nan();
    TEST_ASSERT_EQUAL(IDA_RC_IMU_REJECTED, ida_core_step(&C, &IN, &OUT));
    ds_cikti_degismezleri(IDA_RC_IMU_REJECTED, &IN, &OUT);
    TEST_ASSERT_FALSE(OUT.nav_valid);
    TEST_ASSERT_TRUE(OUT.health.imu_rejected);
    TEST_ASSERT_EQUAL_UINT32(1u, OUT.status.cnt.imu_rejected);
    TEST_ASSERT_TRUE(C.orn.iz[IDA_SENS_MAG][0].var); /* diğer örnekler kabul */
    TEST_ASSERT_EQUAL_UINT16(IDA_REASON_IMU_REJECTED, ds_sonuc(&OUT, 5u)->reason);
    TEST_ASSERT_EQUAL_UINT32(g_seq, OUT.seq);
    /* VALID = 0 ve yinelenen seq'li IMU da reddedilir */
    IN.n_cmds = 0u;
    IN.n_samples = 0u;
    ilerle();
    IN.imu.hdr.flags = 0u;
    TEST_ASSERT_EQUAL(IDA_RC_IMU_REJECTED, ida_core_step(&C, &IN, &OUT));
    ilerle();
    IN.imu.hdr.seq = 1u;
    TEST_ASSERT_EQUAL(IDA_RC_IMU_REJECTED, ida_core_step(&C, &IN, &OUT));
}

static void test_k39_k128_bayatlik(void)
{
    hazirla();
    /* İlk IMU'dan önce bayat (A4-K128). */
    TEST_ASSERT_EQUAL(IDA_RC_OK, tick());
    TEST_ASSERT_FALSE(OUT.health.data_ok);
    TEST_ASSERT_TRUE((OUT.fc.inhibit_mask & IDA_FCI_IMU_STALE) != 0u);
    TEST_ASSERT_TRUE(OUT.thr_valid && OUT.is_safe);
    TEST_ASSERT_EQUAL(IDA_RC_OK, step());
    TEST_ASSERT_TRUE(OUT.health.data_ok);
    TEST_ASSERT_EQUAL_UINT32(1u, ds_gunluk_say(&OUT, (uint16_t)IDA_LOG_IMU_STALE));
    /* IMU kesildi: 20 ms (eşik) içinde taze, kesin büyük olunca bayat. */
    for (uint32_t i = 0u; i < 4u; i++) {
        TEST_ASSERT_EQUAL(IDA_RC_OK, tick()); /* 5, 10, 15, 20 ms */
        TEST_ASSERT_TRUE(OUT.health.data_ok);
    }
    TEST_ASSERT_EQUAL(IDA_RC_OK, tick()); /* 25 ms > 20 ms */
    TEST_ASSERT_FALSE(OUT.health.data_ok);
    TEST_ASSERT_TRUE(OUT.thr_valid && OUT.is_safe); /* nötr güvenli çıktı */
    TEST_ASSERT_EQUAL_UINT32(1u, ds_gunluk_say(&OUT, (uint16_t)IDA_LOG_IMU_STALE));
    TEST_ASSERT_EQUAL_INT32(1, OUT.logs[0].arg0);
}

static void test_k37_k108_gecikme(void)
{
    hazirla();
    /* IMU tf'nin 25 ms gerisinde (ilk IMU sıra koşulundan muaf): mandal kurulur. */
    ilerle();
    IN.imu.hdr.t_meas.ns = g_t - (25 * DS_MS);
    IN.n_cmds = 1u;
    IN.cmds[0].cmd_id = 50u;
    IN.cmds[0].type = (uint8_t)IDA_CMD_ARM;
    TEST_ASSERT_EQUAL(IDA_RC_OK, ida_core_step(&C, &IN, &OUT));
    TEST_ASSERT_TRUE(OUT.health.imu_lag);
    TEST_ASSERT_TRUE((OUT.status.fs_reasons & IDA_FSR_IMU_LAG) != 0u);
    TEST_ASSERT_TRUE((OUT.fc.inhibit_mask & IDA_FCI_IMU_LAG) != 0u);
    TEST_ASSERT_EQUAL_UINT16(IDA_REASON_IMU_LAG, ds_sonuc(&OUT, 50u)->reason);
    TEST_ASSERT_EQUAL_UINT32(1u, OUT.status.cnt.widen_blocked);
    TEST_ASSERT_EQUAL_UINT32(1u, ds_gunluk_say(&OUT, (uint16_t)IDA_LOG_IMU_LAG));
    TEST_ASSERT_EQUAL_UINT8(IDA_FS_NONE, OUT.fs_level); /* mandal arıza düzeyini değiştirmez */
    TEST_ASSERT_TRUE(OUT.thr_valid);                    /* kontrol çıktısı sürer */
    /* Gecikme 5 ms: 19 çağrıda çözülmez, 20.'de çözülür. */
    IN.n_cmds = 0u;
    for (uint32_t i = 1u; i <= 20u; i++) {
        ilerle();
        IN.imu.hdr.t_meas.ns = g_t - (5 * DS_MS);
        TEST_ASSERT_EQUAL(IDA_RC_OK, ida_core_step(&C, &IN, &OUT));
        if (i < 20u) {
            TEST_ASSERT_TRUE(OUT.health.imu_lag);
        } else {
            TEST_ASSERT_FALSE(OUT.health.imu_lag);
            TEST_ASSERT_EQUAL_UINT32(1u, ds_gunluk_say(&OUT, (uint16_t)IDA_LOG_IMU_LAG));
        }
    }
}

static void test_k38_bosluk(void)
{
    hazirla();
    TEST_ASSERT_EQUAL(IDA_RC_OK, step());
    TEST_ASSERT_EQUAL(IDA_RC_OK, step());
    TEST_ASSERT_EQUAL_UINT32(0u, OUT.status.cnt.imu_gap);
    g_t += 5 * DS_MS; /* bir aralık atlandı */
    TEST_ASSERT_EQUAL(IDA_RC_OK, step());
    TEST_ASSERT_EQUAL_UINT32(1u, OUT.status.cnt.imu_gap);
    TEST_ASSERT_EQUAL_UINT32(1u, ds_gunluk_say(&OUT, (uint16_t)IDA_LOG_IMU_GAP));
    TEST_ASSERT_EQUAL(IDA_RC_OK, step());
    TEST_ASSERT_EQUAL_UINT32(1u, OUT.status.cnt.imu_gap);
}

static void test_k40_ping360(void)
{
    static ida_ping360_t p;
    hazirla();
    (void)memset(&p, 0, sizeof(p));
    p.hdr.type = (uint8_t)IDA_SENS_PING360;
    p.hdr.flags = (uint16_t)IDA_HF_VALID;
    p.hdr.t_meas.ns = g_t + (4 * DS_MS);
    p.hdr.t_rx.ns = g_t + (4 * DS_MS);
    p.n_samples = 1200u;
    p.angle_grad = 399u;
    p.sample_period = 1u;
    IN.ping360 = &p;
    TEST_ASSERT_EQUAL(IDA_RC_OK, step());
    TEST_ASSERT_EQUAL_UINT32(0u, OUT.status.cnt.ping360_rejected);
    p.hdr.seq = 2u;
    p.hdr.t_meas.ns = g_t + (4 * DS_MS);
    p.hdr.t_rx.ns = g_t + (4 * DS_MS);
    p.n_samples = 0u;
    TEST_ASSERT_EQUAL(IDA_RC_OK, step());
    p.n_samples = 1u;
    p.angle_grad = 400u;
    TEST_ASSERT_EQUAL(IDA_RC_OK, step());
    p.angle_grad = 0u;
    p.sample_period = 0u;
    TEST_ASSERT_EQUAL(IDA_RC_OK, step());
    TEST_ASSERT_EQUAL_UINT32(3u, OUT.status.cnt.ping360_rejected);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_k30_tur_ve_inst);
    RUN_TEST(test_k33_kosullari);
    RUN_TEST(test_k34_emniyet_esit_zaman);
    RUN_TEST(test_k115_yineleme_ve_celiski);
    RUN_TEST(test_k35_siradan_bagimsizlik);
    RUN_TEST(test_k103_imu_reddi);
    RUN_TEST(test_k39_k128_bayatlik);
    RUN_TEST(test_k37_k108_gecikme);
    RUN_TEST(test_k38_bosluk);
    RUN_TEST(test_k40_ping360);
    return UNITY_END();
}
