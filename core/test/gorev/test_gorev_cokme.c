/*
 * test_gorev_cokme.c — sayısal çökmeden sonra FAILSAFE_RESET (W3-zincir ek iş 2; 02 §2.3.6,
 * §4.3; tablo 3.17 FAILSAFE_RESET satırı).
 *
 * Gerçek kestirim, durgun ve yatay bir aracın duyargalarıyla (IMU, manyetometre, basınç)
 * sürülür. Çökme beyaz kutu olarak enjekte edilir (kovaryansa NaN): kestirim mandalını kurar
 * ve süzgeci yeniden hizalar; görev modülü SAYISAL eylemini uygular. FAILSAFE_RESET ancak
 * süzgeç (mandaldan bağımsız düzeyi, ida_kes_suzgec_duzey) EST_OK_MS boyunca kesintisiz
 * sağlıklı olduktan sonra kabul edilir; öncesinde NOT_ARMABLE (EMNİYET). Kabulden sonra
 * kestirim mandalı kalkar ve araç yeniden silahlanabilir.
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
static uint32_t g_mag_seq;
static uint32_t g_baro_seq;
static uint32_t g_id;

#define P_NS (5 * DS_MS)

/* Bir olağan çağrı (5 ms): IMU, 100 Hz manyetometre, 25 Hz basınç, 1 Hz HEARTBEAT. Çağıran,
   step'ten önce IN'e komut ya da mandal ekleyebilir (sonuna kadar kalırlar). */
static void girdi_kur(void)
{
    g_seq++;
    g_t += P_NS;
    ds_girdi(&IN, g_t, g_seq, true);
    IN.imu.imu.accel[2] = -9.80665f; /* YER_G varsayılanı; durgun ve yatay */
    if ((g_seq % 2u) == 0u) {
        ida_sample_t *s = &IN.samples[IN.n_samples++];
        ds_mag(s, g_t, ++g_mag_seq);
        s->u.mag.field[0] = 25.6e-6f; /* havuz alanı (NED), yön 0 */
        s->u.mag.field[1] = 2.7e-6f;
        s->u.mag.field[2] = 40.3e-6f;
    }
    if ((g_seq % 8u) == 0u) {
        ida_sample_t *s = &IN.samples[IN.n_samples++];
        (void)memset(s, 0, sizeof(*s));
        s->hdr.t_meas.ns = g_t;
        s->hdr.t_rx.ns = g_t;
        s->hdr.seq = ++g_baro_seq;
        s->hdr.window_ns = 18750000u;
        s->hdr.flags = (uint16_t)IDA_HF_VALID;
        s->hdr.type = (uint8_t)IDA_SENS_BARO;
        s->u.baro.p_abs = 106000.0f;
        s->u.baro.temp_c = 20.0f;
    }
    if ((g_seq % 20u) == 0u) { /* batarya ve sızıntı 10 Hz (W4-K13: veri yokken ARM reddi) */
        ida_sample_t *s = &IN.samples[IN.n_samples++];
        (void)memset(s, 0, sizeof(*s));
        s->hdr.t_meas.ns = g_t;
        s->hdr.t_rx.ns = g_t;
        s->hdr.seq = g_seq;
        s->hdr.window_ns = 100000000u;
        s->hdr.flags = (uint16_t)IDA_HF_VALID;
        s->hdr.type = (uint8_t)IDA_SENS_BATT;
        s->u.batt.voltage = 15.6f;
        ida_sample_t *l = &IN.samples[IN.n_samples++];
        (void)memset(l, 0, sizeof(*l));
        l->hdr.t_meas.ns = g_t;
        l->hdr.t_rx.ns = g_t;
        l->hdr.seq = g_seq;
        l->hdr.flags = (uint16_t)IDA_HF_VALID;
        l->hdr.type = (uint8_t)IDA_SENS_LEAK;
    }
    if ((g_seq % 200u) == 0u) {
        ida_cmd_t *k = &IN.cmds[IN.n_cmds++];
        (void)memset(k, 0, sizeof(*k));
        k->cmd_id = ++g_id;
        k->type = (uint8_t)IDA_CMD_HEARTBEAT;
    }
}

static void cagir(void)
{
    const ida_rc_t rc = ida_core_step(&C, &IN, &OUT);
    TEST_ASSERT_EQUAL(IDA_RC_OK, rc);
    ds_cikti_degismezleri(rc, &IN, &OUT);
}

static void adim(void)
{
    girdi_kur();
    cagir();
}

static void adimlar_sure(int64_t ns)
{
    const int64_t son = g_t + ns;
    while (g_t < son) {
        adim();
    }
}

/* Tek komutlu çağrı; sonucu döndürür. */
static ida_cmd_result_t komutla(uint8_t tur)
{
    girdi_kur();
    ida_cmd_t *k = &IN.cmds[IN.n_cmds++];
    (void)memset(k, 0, sizeof(*k));
    const uint32_t id = ++g_id;
    k->cmd_id = id;
    k->type = tur;
    cagir();
    const ida_cmd_result_t *r = ds_sonuc(&OUT, id);
    TEST_ASSERT_NOT_NULL(r);
    return *r;
}

static void mandal(uint8_t bit, uint8_t kip)
{
    girdi_kur();
    IN.narrow.mask = bit;
    IN.narrow.mode = kip;
    IN.narrow.cmd_id[(bit == (uint8_t)IDA_NARROW_DISARM) ? 0u : 1u] = ++g_id;
    cagir();
}

static bool yeterli(uint8_t d)
{
    return (d == (uint8_t)IDA_EST_OK) || (d == (uint8_t)IDA_EST_DEGRADED);
}

static void hazirla(void)
{
    TEST_ASSERT_EQUAL(IDA_RC_OK, ds_kur(&C));
    g_seq = 0u;
    g_t = 2000 * DS_MS;
    g_mag_seq = 0u;
    g_baro_seq = 0u;
    g_id = 0u;
    /* Hizalama (1 s) ve ölçüm ufku; kip INIT'ten DISARMED'a çıkar. */
    adimlar_sure(3000 * DS_MS);
    TEST_ASSERT_TRUE(yeterli(OUT.nav.est_level));
    TEST_ASSERT_EQUAL_UINT8(IDA_MODE_DISARMED, OUT.status.mode);
}

static void cokme_enjekte(void)
{
    C.kes.P[0][0] = ds_nan(); /* beyaz kutu: süzgeç sağlamlık denetiminde çöker */
    adim();
    TEST_ASSERT_EQUAL_UINT8(IDA_EST_FAULT, OUT.nav.est_level);
}

static void test_cokme_sonrasi_failsafe_reset_ve_yeniden_arm(void)
{
    hazirla();
    ida_cmd_result_t r = komutla((uint8_t)IDA_CMD_ARM);
    TEST_ASSERT_EQUAL_UINT8(IDA_ACK_ACCEPTED, r.ack);
    mandal((uint8_t)IDA_NARROW_MODE, (uint8_t)IDA_MODE_MANUAL); /* SAYISAL: itkisiz eylem */
    TEST_ASSERT_EQUAL_UINT8(IDA_MODE_MANUAL, OUT.status.mode);
    TEST_ASSERT_TRUE(OUT.fs_level <= (uint8_t)IDA_FS_WARN);

    cokme_enjekte();
    adim();
    TEST_ASSERT_EQUAL_UINT8(IDA_FS_PASSIVE_SURFACE, OUT.fs_level); /* arıza eylemi */
    TEST_ASSERT_TRUE((OUT.status.fs_reasons & IDA_FSR_NUMERIC) != 0u);
    TEST_ASSERT_TRUE(OUT.is_safe);
    for (uint32_t i = 0u; i < IDA_PORT_N_THR; i++) {
        TEST_ASSERT_TRUE(OUT.thr_cmd[i] == 0.0f);
    }
    mandal((uint8_t)IDA_NARROW_DISARM, 0u);
    TEST_ASSERT_FALSE(OUT.armed);

    /* Süzgeç toparlanmadan: NOT_ARMABLE; ARM da reddedilir. */
    r = komutla((uint8_t)IDA_CMD_FAILSAFE_RESET);
    TEST_ASSERT_EQUAL_UINT8(IDA_ACK_REJECTED, r.ack);
    TEST_ASSERT_EQUAL_UINT16(IDA_REASON_NOT_ARMABLE, r.reason);
    r = komutla((uint8_t)IDA_CMD_ARM);
    TEST_ASSERT_EQUAL_UINT8(IDA_ACK_REJECTED, r.ack);
    TEST_ASSERT_EQUAL_UINT16(IDA_REASON_NOT_ARMABLE, r.reason);

    /* Süzgeç yeniden hizalanır; yayımlanan düzey mandal yüzünden FAULT kalır. */
    int n = 0;
    while (!yeterli(ida_kes_suzgec_duzey(&C.kes)) && (n < 1000)) {
        adim();
        n++;
    }
    TEST_ASSERT_TRUE(yeterli(ida_kes_suzgec_duzey(&C.kes)));
    TEST_ASSERT_EQUAL_UINT8(IDA_EST_FAULT, OUT.nav.est_level);
    const uint32_t ok_ms = ida_prm_u32(&C.prm, IDA_PRM_gor_est_ok_ms);

    /* EST_OK_MS dolmadan: hâlâ NOT_ARMABLE (EMNİYET). */
    adimlar_sure(((int64_t)ok_ms - 200) * DS_MS);
    r = komutla((uint8_t)IDA_CMD_FAILSAFE_RESET);
    TEST_ASSERT_EQUAL_UINT8(IDA_ACK_REJECTED, r.ack);
    TEST_ASSERT_EQUAL_UINT16(IDA_REASON_NOT_ARMABLE, r.reason);

    /* EST_OK_MS kesintisiz sağlıklı: kabul; kestirim mandalı kalkar. */
    adimlar_sure(300 * DS_MS);
    r = komutla((uint8_t)IDA_CMD_FAILSAFE_RESET);
    TEST_ASSERT_EQUAL_UINT8(IDA_ACK_ACCEPTED, r.ack);
    TEST_ASSERT_FALSE(C.kes.ariza);
    adim();
    TEST_ASSERT_TRUE(yeterli(OUT.nav.est_level));
    TEST_ASSERT_TRUE(OUT.fs_level <= (uint8_t)IDA_FS_WARN);
    TEST_ASSERT_TRUE((OUT.status.fs_reasons & IDA_FSR_NUMERIC) == 0u);
    r = komutla((uint8_t)IDA_CMD_ARM);
    TEST_ASSERT_EQUAL_UINT8(IDA_ACK_ACCEPTED, r.ack);
    TEST_ASSERT_TRUE(OUT.armed);
}

/* Sağlıklı süre kesintisiz olmalı: pencere içinde ikinci çökme sayacı baştan başlatır. */
static void test_kesintili_saglik_sayilmaz(void)
{
    hazirla();
    cokme_enjekte();
    int n = 0;
    while (!yeterli(ida_kes_suzgec_duzey(&C.kes)) && (n < 1000)) {
        adim();
        n++;
    }
    const uint32_t ok_ms = ida_prm_u32(&C.prm, IDA_PRM_gor_est_ok_ms);
    adimlar_sure(((int64_t)ok_ms / 2) * DS_MS);
    cokme_enjekte(); /* süzgeç yeniden INIT'e döner */
    TEST_ASSERT_FALSE(yeterli(ida_kes_suzgec_duzey(&C.kes)));
    n = 0;
    while (!yeterli(ida_kes_suzgec_duzey(&C.kes)) && (n < 1000)) {
        adim();
        n++;
    }
    /* İlk sağlıklı dönemle toplam EST_OK_MS'yi aşsa da ikinci dönem kısa: ret. */
    adimlar_sure(((int64_t)ok_ms / 2 + 100) * DS_MS);
    ida_cmd_result_t r = komutla((uint8_t)IDA_CMD_FAILSAFE_RESET);
    TEST_ASSERT_EQUAL_UINT8(IDA_ACK_REJECTED, r.ack);
    TEST_ASSERT_EQUAL_UINT16(IDA_REASON_NOT_ARMABLE, r.reason);
    TEST_ASSERT_TRUE(C.kes.ariza);
    adimlar_sure(((int64_t)ok_ms / 2) * DS_MS);
    r = komutla((uint8_t)IDA_CMD_FAILSAFE_RESET);
    TEST_ASSERT_EQUAL_UINT8(IDA_ACK_ACCEPTED, r.ack);
    TEST_ASSERT_FALSE(C.kes.ariza);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_cokme_sonrasi_failsafe_reset_ve_yeniden_arm);
    RUN_TEST(test_kesintili_saglik_sayilmaz);
    return UNITY_END();
}
