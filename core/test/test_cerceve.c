/*
 * test_cerceve.c — çağrı çerçevesi: init sırası, 3.11 değerlendirme sırası ve öncelik
 * tablosu, 4.14 satırları, 4.15 NULL satırları, A4-K10, K15, K17, K25, K27, K31, K57, K72
 * (b3), K113, K114, K122, K126.
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

/* ---- ida_core_info, init ------------------------------------------------------------------ */

static void test_info(void)
{
    ida_core_info(NULL); /* NULL: hiçbir şey yapmaz */
    ida_core_info_t i;
    (void)memset(&i, 0xA5, sizeof(i));
    ida_core_info(&i);
    TEST_ASSERT_EQUAL_UINT16(1u, i.port_major);
    TEST_ASSERT_EQUAL_UINT16(0u, i.port_minor);
    TEST_ASSERT_EQUAL_UINT32((uint32_t)sizeof(ida_core_t), i.core_size);
    TEST_ASSERT_EQUAL_UINT32((uint32_t)_Alignof(ida_core_t), i.core_align);
    TEST_ASSERT_TRUE(i.param_schema_hash == ida_prm_sema_ozeti());
    TEST_ASSERT_EQUAL_UINT32(IDA_CORE_VERSION, i.core_version);
}

static void test_init_sirasi(void)
{
    ida_init_in_t ii;
    ds_init_girdisi(&ii);
    TEST_ASSERT_EQUAL(IDA_RC_BAD_INPUT, ida_core_init(NULL, &ii));
    TEST_ASSERT_EQUAL(IDA_RC_OK, ds_kur(&C));
    TEST_ASSERT_EQUAL(IDA_RC_BAD_INPUT, ida_core_init(&C, NULL)); /* (2) kurulmamış işaretler */
    ds_girdi(&IN, 2000 * DS_MS, 1u, true);
    TEST_ASSERT_EQUAL(IDA_RC_NOT_INIT, ida_core_step(&C, &IN, &OUT));

    ii.port_minor = 1u; /* (3) */
    TEST_ASSERT_EQUAL(IDA_RC_VERSION, ida_core_init(&C, &ii));
    TEST_ASSERT_EQUAL(IDA_RC_NOT_INIT, ida_core_step(&C, &IN, &OUT));
    ds_init_girdisi(&ii);
    ii.port_major = 2u;
    TEST_ASSERT_EQUAL(IDA_RC_VERSION, ida_core_init(&C, &ii));

    ds_init_girdisi(&ii); /* (4) */
    ii.now_w.ns = -1;
    TEST_ASSERT_EQUAL(IDA_RC_BAD_INPUT, ida_core_init(&C, &ii));
    ii.now_w.ns = IDA_TIME_MAX_NS + 1;
    TEST_ASSERT_EQUAL(IDA_RC_BAD_INPUT, ida_core_init(&C, &ii));
    ds_init_girdisi(&ii);
    ii.param_len = 5u; /* param_blob NULL */
    TEST_ASSERT_EQUAL(IDA_RC_BAD_INPUT, ida_core_init(&C, &ii));
    TEST_ASSERT_EQUAL(IDA_RC_NOT_INIT, ida_core_step(&C, &IN, &OUT));

    ds_init_girdisi(&ii); /* (5) */
    TEST_ASSERT_EQUAL(IDA_RC_PARAM_DEFAULTS, ida_core_init(&C, &ii));
    static uint8_t blok[IDA_PARAM_BLOB_MAX + 8u];
    const uint32_t n = ds_gecerli_blok(blok);
    ii.param_blob = blok;
    ii.param_len = IDA_PARAM_BLOB_MAX + 1u;
    TEST_ASSERT_EQUAL(IDA_RC_PARAM_DEFAULTS, ida_core_init(&C, &ii));
    ii.param_len = n; /* (6) */
    TEST_ASSERT_EQUAL(IDA_RC_OK, ida_core_init(&C, &ii));
    TEST_ASSERT_EQUAL(IDA_RC_OK, ida_core_step(&C, &IN, &OUT));
}

static void test_acilis_gunlugu_ilk_yazilabilir_cagrida(void)
{
    ida_init_in_t ii;
    ds_init_girdisi(&ii);
    ii.reset_cause = (uint8_t)IDA_RESET_WATCHDOG;
    ii.consecutive_faults = 3u;
    ii.param_len = 0u;
    TEST_ASSERT_EQUAL(IDA_RC_PARAM_DEFAULTS, ida_core_init(&C, &ii));
    ds_girdi(&IN, 2000 * DS_MS, 1u, true);
    /* out NULL çağrı yazılabilir değildir; kayıtlar bekler. */
    TEST_ASSERT_EQUAL(IDA_RC_BAD_INPUT, ida_core_step(&C, &IN, NULL));
    IN.n_samples = 17u; /* hata dönüşünde de çıkar (3.15) */
    ds_desen(&OUT);
    TEST_ASSERT_EQUAL(IDA_RC_BAD_INPUT, ida_core_step(&C, &IN, &OUT));
    TEST_ASSERT_EQUAL_UINT8(IDA_LOG_RESET_INFO, OUT.logs[0].code);
    TEST_ASSERT_EQUAL_INT32(IDA_RESET_WATCHDOG, OUT.logs[0].arg0);
    TEST_ASSERT_EQUAL_INT32(3, OUT.logs[0].arg1);
    TEST_ASSERT_EQUAL_UINT8(IDA_SEV_NOTICE, OUT.logs[0].severity);
    TEST_ASSERT_EQUAL_UINT8(IDA_LOG_PARAM_LOAD, OUT.logs[1].code);
    TEST_ASSERT_EQUAL_INT32(IDA_RC_PARAM_DEFAULTS, OUT.logs[1].arg0);
    TEST_ASSERT_EQUAL_INT32(0, OUT.logs[1].arg1); /* blok yok */
    TEST_ASSERT_EQUAL_UINT8(IDA_SEV_WARNING, OUT.logs[1].severity);
    TEST_ASSERT_TRUE((1000 * DS_MS) == (OUT.logs[0].t_w.ns));
    IN.n_samples = 0u;
    TEST_ASSERT_EQUAL(IDA_RC_OK, ida_core_step(&C, &IN, &OUT));
    TEST_ASSERT_EQUAL_UINT32(0u, ds_gunluk_say(&OUT, (uint16_t)IDA_LOG_RESET_INFO));
}

/* ---- 4.15 NULL satırları ------------------------------------------------------------------ */

static void test_null_satirlari(void)
{
    TEST_ASSERT_EQUAL(IDA_RC_OK, ds_kur(&C));
    ds_girdi(&IN, 2000 * DS_MS, 1u, true);
    IN.narrow.mask = IDA_NARROW_DISARM;
    C.emn.armed = true; /* test kancası */
    ds_desen(&OUT);
    ida_out_t desen;
    ds_desen(&desen);
    /* (a1): hiçbir şey okunmaz, yazılmaz, uygulanmaz. */
    TEST_ASSERT_EQUAL(IDA_RC_BAD_INPUT, ida_core_step(NULL, &IN, &OUT));
    TEST_ASSERT_EQUAL(IDA_RC_BAD_INPUT, ida_core_step(&C, NULL, &OUT));
    TEST_ASSERT_EQUAL(IDA_RC_BAD_INPUT, ida_core_tick(NULL, &IN, &OUT));
    TEST_ASSERT_EQUAL(IDA_RC_BAD_INPUT, ida_core_tick(&C, NULL, &OUT));
    TEST_ASSERT_EQUAL_MEMORY(&desen, &OUT, sizeof(OUT));
    TEST_ASSERT_TRUE(C.emn.armed);
    /* (b2): daraltma uygulanır, başka hiçbir şey yazılmaz, cnt.bad_input artmaz. */
    TEST_ASSERT_EQUAL(IDA_RC_BAD_INPUT, ida_core_step(&C, &IN, NULL));
    TEST_ASSERT_FALSE(C.emn.armed);
    TEST_ASSERT_EQUAL_UINT32(0u, C.cer.cnt.bad_input);
    /* Örnek kurulmamış + out NULL: NOT_INIT. */
    (void)memset(&C2, 0, sizeof(C2));
    TEST_ASSERT_EQUAL(IDA_RC_NOT_INIT, ida_core_step(&C2, &IN, NULL));
    TEST_ASSERT_EQUAL(IDA_RC_NOT_INIT, ida_core_tick(&C2, &IN, NULL));
}

/* ---- A4-K126 ---------------------------------------------------------------------------- */

static void kurulmamis_icerik(const ida_out_t *o, uint8_t src)
{
    TEST_ASSERT_FALSE(o->thr_valid);
    TEST_ASSERT_TRUE(o->is_safe);
    TEST_ASSERT_FALSE(o->armed);
    TEST_ASSERT_EQUAL_UINT8(IDA_FS_DISARM, o->fs_level); /* A4-K126 */
    TEST_ASSERT_EQUAL_UINT8(src, o->src);
    TEST_ASSERT_EQUAL_UINT32(0u, o->seq);
    TEST_ASSERT_TRUE((0) == (o->t_ref.ns));
    TEST_ASSERT_EQUAL_UINT8(IDA_MODE_INIT, o->status.mode);
    TEST_ASSERT_EQUAL_UINT8(IDA_FS_DISARM, o->status.fs_level);
    TEST_ASSERT_EQUAL_UINT8(IDA_MS_IDLE, o->status.mission_state);
    TEST_ASSERT_FALSE(o->status.param_ok);
    TEST_ASSERT_FALSE(o->status.events_ok);
    TEST_ASSERT_EQUAL_UINT32(0u, o->status.boot_count);
    TEST_ASSERT_TRUE(o->health.core_ok);
    TEST_ASSERT_FALSE(o->health.data_ok);
    TEST_ASSERT_EQUAL_UINT8(IDA_FCS_SAFE, o->fc.state);
    TEST_ASSERT_EQUAL_HEX32(0x00026FFFu, o->fc.inhibit_mask);
    TEST_ASSERT_EQUAL_UINT8(0u, o->n_events);
    TEST_ASSERT_EQUAL_UINT8(0u, o->n_logs);
    TEST_ASSERT_FALSE(o->nav_valid);
}

static void test_k126_kurulmamis_cikti_sabit(void)
{
    ds_girdi(&IN, 2000 * DS_MS, 1u, true);
    IN.narrow.mask = (uint8_t)(IDA_NARROW_DISARM | IDA_NARROW_FC_ABORT);
    IN.narrow.cmd_id[0] = 11u;
    IN.narrow.cmd_id[3] = 14u;
    IN.n_cmds = 2u;
    IN.cmds[0].cmd_id = 21u;
    IN.cmds[0].type = (uint8_t)IDA_CMD_ARM;
    IN.cmds[1].cmd_id = 22u;
    IN.cmds[1].type = (uint8_t)IDA_CMD_PARAM_GET;
    /* Sıfırlanmış ve rastgele doldurulmuş kurulmamış bellek. */
    (void)memset(&C, 0, sizeof(C));
    ds_rastgele_t r = {0x1234567u};
    uint8_t *b = (uint8_t *)&C2;
    for (size_t i = 0u; i < sizeof(C2); i++) {
        b[i] = (uint8_t)ds_rastgele(&r);
    }
    ds_desen(&OUT);
    (void)memset(&OUT2, 0, sizeof(OUT2));
    TEST_ASSERT_EQUAL(IDA_RC_NOT_INIT, ida_core_step(&C, &IN, &OUT));
    TEST_ASSERT_EQUAL(IDA_RC_NOT_INIT, ida_core_step(&C2, &IN, &OUT2));
    TEST_ASSERT_EQUAL_MEMORY(&OUT, &OUT2, sizeof(OUT));
    kurulmamis_icerik(&OUT, (uint8_t)IDA_SRC_STEP);
    ds_cikti_degismezleri(IDA_RC_NOT_INIT, &IN, &OUT);
    /* A4-K114: mandal bitleri ve komutlar REJECTED / NOT_INIT. */
    TEST_ASSERT_EQUAL_UINT8(4u, OUT.n_results);
    TEST_ASSERT_EQUAL_UINT32(11u, OUT.results[0].cmd_id);
    TEST_ASSERT_EQUAL_UINT32(14u, OUT.results[1].cmd_id);
    TEST_ASSERT_EQUAL_UINT32(21u, OUT.results[2].cmd_id);
    TEST_ASSERT_EQUAL_UINT8(IDA_RES_PARAM_VALUE, OUT.results[3].kind);
    for (uint32_t i = 0u; i < 4u; i++) {
        TEST_ASSERT_EQUAL_UINT8(IDA_ACK_REJECTED, OUT.results[i].ack);
        TEST_ASSERT_EQUAL_UINT16(IDA_REASON_NOT_INIT, OUT.results[i].reason);
    }
    /* n_cmds > 8: komutlar için sonuç yok. */
    IN.n_cmds = 9u;
    TEST_ASSERT_EQUAL(IDA_RC_NOT_INIT, ida_core_tick(&C, &IN, &OUT));
    TEST_ASSERT_EQUAL_UINT8(2u, OUT.n_results);
    kurulmamis_icerik(&OUT, (uint8_t)IDA_SRC_TICK);
}

/* ---- 3.11 öncelik tablosunun her dolu hücresi --------------------------------------------- */

static void kurulu_ve_ilk_cagri(void)
{
    TEST_ASSERT_EQUAL(IDA_RC_OK, ds_kur(&C));
    ds_girdi(&IN, 2000 * DS_MS, 100u, true);
    TEST_ASSERT_EQUAL(IDA_RC_OK, ida_core_step(&C, &IN, &OUT));
}

static void girdi_dup(void)
{
    ds_girdi(&IN, 2010 * DS_MS, 100u, true); /* son kabul edilen seq */
}

static void girdi_old(void)
{
    ds_girdi(&IN, 2010 * DS_MS, 99u, true);
}

static void imu_bozuk(void)
{
    IN.imu.imu.gyro[0] = ds_nan();
}

static void test_oncelik_tablosu(void)
{
    /* c/in NULL satırı: her sütunla BAD_INPUT. */
    (void)memset(&C2, 0, sizeof(C2)); /* NOT_INIT */
    TEST_ASSERT_EQUAL(IDA_RC_BAD_INPUT, ida_core_step(&C2, NULL, &OUT));
    kurulu_ve_ilk_cagri();
    TEST_ASSERT_EQUAL(IDA_RC_BAD_INPUT, ida_core_step(&C, NULL, &OUT)); /* × BAD/DUP/OLD/IMU */
    TEST_ASSERT_EQUAL(IDA_RC_BAD_INPUT, ida_core_step(NULL, &IN, &OUT));

    /* NOT_INIT satırı: BAD_INPUT, DUP_SEQ, OLD_SEQ, IMU_REJECTED ile NOT_INIT. */
    girdi_dup();
    IN.n_samples = 17u;
    TEST_ASSERT_EQUAL(IDA_RC_NOT_INIT, ida_core_step(&C2, &IN, &OUT));
    girdi_dup();
    TEST_ASSERT_EQUAL(IDA_RC_NOT_INIT, ida_core_step(&C2, &IN, &OUT));
    girdi_old();
    TEST_ASSERT_EQUAL(IDA_RC_NOT_INIT, ida_core_step(&C2, &IN, &OUT));
    ds_girdi(&IN, 2010 * DS_MS, 101u, true);
    imu_bozuk();
    TEST_ASSERT_EQUAL(IDA_RC_NOT_INIT, ida_core_step(&C2, &IN, &OUT));

    /* BAD_INPUT satırı: DUP_SEQ, OLD_SEQ, IMU_REJECTED ile; (b2) out NULL dahil. */
    girdi_dup();
    IN.n_samples = 17u;
    TEST_ASSERT_EQUAL(IDA_RC_BAD_INPUT, ida_core_step(&C, &IN, &OUT));
    girdi_old();
    IN.n_cmds = 9u;
    TEST_ASSERT_EQUAL(IDA_RC_BAD_INPUT, ida_core_step(&C, &IN, &OUT));
    ds_girdi(&IN, 2010 * DS_MS, 101u, true);
    imu_bozuk();
    IN.n_evres = 5u;
    TEST_ASSERT_EQUAL(IDA_RC_BAD_INPUT, ida_core_step(&C, &IN, &OUT));
    girdi_dup();
    TEST_ASSERT_EQUAL(IDA_RC_BAD_INPUT, ida_core_step(&C, &IN, NULL));
    ds_girdi(&IN, 2010 * DS_MS, 101u, true);
    imu_bozuk();
    TEST_ASSERT_EQUAL(IDA_RC_BAD_INPUT, ida_core_step(&C, &IN, NULL));

    /* DUP_SEQ × IMU_REJECTED → DUP_SEQ; OLD_SEQ × IMU_REJECTED → OLD_SEQ. */
    girdi_dup();
    imu_bozuk();
    TEST_ASSERT_EQUAL(IDA_RC_DUP_SEQ, ida_core_step(&C, &IN, &OUT));
    girdi_old();
    imu_bozuk();
    TEST_ASSERT_EQUAL(IDA_RC_OLD_SEQ, ida_core_step(&C, &IN, &OUT));
    /* Tek başına IMU_REJECTED. */
    ds_girdi(&IN, 2010 * DS_MS, 101u, true);
    imu_bozuk();
    TEST_ASSERT_EQUAL(IDA_RC_IMU_REJECTED, ida_core_step(&C, &IN, &OUT));
    ds_cikti_degismezleri(IDA_RC_IMU_REJECTED, &IN, &OUT);
}

/* ---- 4.14 satırları ve A4-K27 ------------------------------------------------------------ */

static void test_414_step_ok(void)
{
    TEST_ASSERT_EQUAL(IDA_RC_OK, ds_kur(&C));
    ds_girdi(&IN, 2000 * DS_MS, 5u, true);
    ds_desen(&OUT);
    TEST_ASSERT_EQUAL(IDA_RC_OK, ida_core_step(&C, &IN, &OUT));
    ds_cikti_degismezleri(IDA_RC_OK, &IN, &OUT);
    TEST_ASSERT_TRUE(OUT.thr_valid);
    TEST_ASSERT_TRUE(OUT.is_safe); /* silahsız (A4-K55) */
    TEST_ASSERT_EQUAL_UINT32(IDA_VALID_FOR_DEFAULT_NS, OUT.valid_for_ns);
    TEST_ASSERT_EQUAL_UINT32(5u, OUT.seq);
    TEST_ASSERT_TRUE((2000 * DS_MS) == (OUT.t_ref.ns));
    TEST_ASSERT_TRUE(OUT.nav_valid);
    TEST_ASSERT_EQUAL_UINT8(IDA_EST_INIT, OUT.nav.est_level);
    TEST_ASSERT_TRUE((2000 * DS_MS) == (OUT.nav.t.ns));
    TEST_ASSERT_EQUAL_FLOAT(1.0f, OUT.nav.q_nb[0]);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, OUT.nav.altitude);
    TEST_ASSERT_FALSE(OUT.armed);
    TEST_ASSERT_EQUAL_UINT8(IDA_FS_NONE, OUT.fs_level);
    TEST_ASSERT_EQUAL_UINT8(IDA_SRC_STEP, OUT.src);
    TEST_ASSERT_TRUE(OUT.health.core_ok);
    TEST_ASSERT_TRUE(OUT.health.data_ok);
    TEST_ASSERT_EQUAL_UINT8(IDA_MODE_INIT, OUT.status.mode);
    TEST_ASSERT_TRUE(OUT.status.param_ok);
    TEST_ASSERT_TRUE(OUT.status.events_ok);
    TEST_ASSERT_EQUAL_UINT32(7u, OUT.status.boot_count);
    TEST_ASSERT_EQUAL_UINT8(IDA_FCS_SAFE, OUT.fc.state);
}

static void test_414_tick_ve_izgara(void)
{
    TEST_ASSERT_EQUAL(IDA_RC_OK, ds_kur(&C));
    ds_girdi(&IN, 2000 * DS_MS, 5u, true);
    TEST_ASSERT_EQUAL(IDA_RC_OK, ida_core_step(&C, &IN, &OUT));
    /* zaman çerçeveli tick */
    ds_girdi(&IN, 2005 * DS_MS, 6u, false);
    ds_desen(&OUT);
    TEST_ASSERT_EQUAL(IDA_RC_OK, ida_core_tick(&C, &IN, &OUT));
    ds_cikti_degismezleri(IDA_RC_OK, &IN, &OUT);
    TEST_ASSERT_TRUE(OUT.is_safe);
    TEST_ASSERT_TRUE(OUT.thr_valid); /* A4-K56: thr_valid = is_safe */
    TEST_ASSERT_EQUAL_UINT32(6u, OUT.seq);
    TEST_ASSERT_FALSE(OUT.nav_valid);
    TEST_ASSERT_EQUAL_UINT8(IDA_SRC_TICK, OUT.src);
    /* ızgara tick'i: son geçerli */
    ds_girdi(&IN, 2010 * DS_MS, 0u, false);
    IN.tf.present = false;
    TEST_ASSERT_EQUAL(IDA_RC_OK, ida_core_tick(&C, &IN, &OUT));
    TEST_ASSERT_EQUAL_UINT32(6u, OUT.seq);
    TEST_ASSERT_TRUE((2005 * DS_MS) == (OUT.t_ref.ns));
    ds_cikti_degismezleri(IDA_RC_OK, &IN, &OUT);
}

static void test_414_hata_satirlari(void)
{
    TEST_ASSERT_EQUAL(IDA_RC_OK, ds_kur(&C));
    ds_girdi(&IN, 2000 * DS_MS, 5u, true);
    TEST_ASSERT_EQUAL(IDA_RC_OK, ida_core_step(&C, &IN, &OUT));
    /* BAD_INPUT: geçersiz tf.t; t_ref son geçerli, girdideki zaman yazılmaz (A4-K57). */
    ds_girdi(&IN, 2010 * DS_MS, 6u, true);
    IN.tf.t.ns = -5;
    ds_desen(&OUT);
    TEST_ASSERT_EQUAL(IDA_RC_BAD_INPUT, ida_core_step(&C, &IN, &OUT));
    ds_cikti_degismezleri(IDA_RC_BAD_INPUT, &IN, &OUT);
    TEST_ASSERT_EQUAL_UINT32(5u, OUT.seq);
    TEST_ASSERT_TRUE((2000 * DS_MS) == (OUT.t_ref.ns));
    TEST_ASSERT_TRUE(OUT.is_safe);
    TEST_ASSERT_EQUAL_UINT32(1u, OUT.status.cnt.bad_input);
    /* DUP_SEQ */
    ds_girdi(&IN, 2010 * DS_MS, 5u, true);
    TEST_ASSERT_EQUAL(IDA_RC_DUP_SEQ, ida_core_step(&C, &IN, &OUT));
    ds_cikti_degismezleri(IDA_RC_DUP_SEQ, &IN, &OUT);
    TEST_ASSERT_EQUAL_UINT32(1u, OUT.status.cnt.dup_seq);
    /* OLD_SEQ */
    ds_girdi(&IN, 2010 * DS_MS, 4u, true);
    TEST_ASSERT_EQUAL(IDA_RC_OLD_SEQ, ida_core_step(&C, &IN, &OUT));
    ds_cikti_degismezleri(IDA_RC_OLD_SEQ, &IN, &OUT);
    TEST_ASSERT_EQUAL_UINT32(1u, OUT.status.cnt.old_seq);
    TEST_ASSERT_EQUAL_UINT32(5u, OUT.seq);
    /* Taze IMU'lu hatalı tick: thr_valid = 0 (§7). */
    ds_girdi(&IN, 2010 * DS_MS, 6u, false);
    IN.imu_present = true;
    TEST_ASSERT_EQUAL(IDA_RC_BAD_INPUT, ida_core_tick(&C, &IN, &OUT));
    TEST_ASSERT_FALSE(OUT.thr_valid);
}

/* A4-K27: aynı çağrı dizisi, çıktı tamponu farklı desenlerle doldurulmuşken bayt bayt aynı. */
static void test_k27_desenden_bagimsiz(void)
{
    TEST_ASSERT_EQUAL(IDA_RC_OK, ds_kur(&C));
    TEST_ASSERT_EQUAL(IDA_RC_OK, ds_kur(&C2));
    for (uint32_t i = 0u; i < 12u; i++) {
        ds_girdi(&IN, (2000 + (5 * (int64_t)i)) * DS_MS, 10u + i, (i % 3u) != 1u);
        if (i == 4u) {
            IN.n_samples = 17u; /* BAD_INPUT */
        }
        if (i == 6u) {
            IN.tf.seq = 13u; /* DUP_SEQ */
        }
        if (i == 8u) {
            IN.imu.hdr.flags = 0u; /* IMU_REJECTED */
        }
        if (i == 10u) {
            IN.plat.link_event = (uint8_t)IDA_LINKEV_SESSION_END;
        }
        (void)memset(&OUT, 0xA5, sizeof(OUT));
        (void)memset(&OUT2, 0x00, sizeof(OUT2));
        const ida_rc_t r1 = (IN.imu_present) ? ida_core_step(&C, &IN, &OUT) : ida_core_tick(&C, &IN, &OUT);
        const ida_rc_t r2 =
            (IN.imu_present) ? ida_core_step(&C2, &IN, &OUT2) : ida_core_tick(&C2, &IN, &OUT2);
        TEST_ASSERT_EQUAL(r1, r2);
        TEST_ASSERT_EQUAL_MEMORY(&OUT, &OUT2, sizeof(OUT));
        ds_cikti_degismezleri(r1, &IN, &OUT);
    }
}

/* ---- A4-K10, K15, K17, K25, K31 ---------------------------------------------------------- */

static void test_k10_geri_adim(void)
{
    TEST_ASSERT_EQUAL(IDA_RC_OK, ds_kur(&C));
    ds_girdi(&IN, 2000 * DS_MS, 1u, true);
    TEST_ASSERT_EQUAL(IDA_RC_OK, ida_core_step(&C, &IN, &OUT));
    /* now_w geri gider: önceki değer kullanılır; t_rx = 2000 ms > verilen now ama ≤ kullanılan. */
    ds_girdi(&IN, 2001 * DS_MS, 2u, true);
    IN.now_w.ns = 1990 * DS_MS;
    IN.tf.t_rx.ns = 1995 * DS_MS;
    IN.imu.hdr.t_rx.ns = 1999 * DS_MS;
    TEST_ASSERT_EQUAL(IDA_RC_OK, ida_core_step(&C, &IN, &OUT));
    TEST_ASSERT_EQUAL_UINT32(1u, OUT.status.cnt.wall_backstep);
    TEST_ASSERT_EQUAL_UINT32(0u, OUT.status.cnt.sample_future);
    TEST_ASSERT_EQUAL_UINT32(1u, ds_gunluk_say(&OUT, (uint16_t)IDA_LOG_WALL_BACKSTEP));
    TEST_ASSERT_TRUE((2000 * DS_MS) == (OUT.logs[0].t_w.ns));
    /* tf.t_rx kullanılan now_w'yi aşarsa BAD_INPUT (3.11 (c)). */
    ds_girdi(&IN, 2010 * DS_MS, 3u, true);
    IN.tf.t_rx.ns = 2011 * DS_MS;
    TEST_ASSERT_EQUAL(IDA_RC_BAD_INPUT, ida_core_step(&C, &IN, &OUT));
}

static void test_k15_k17_k31(void)
{
    TEST_ASSERT_EQUAL(IDA_RC_OK, ds_kur(&C));
    ds_girdi(&IN, 2000 * DS_MS, 1u, true);
    TEST_ASSERT_EQUAL(IDA_RC_OK, ida_core_step(&C, &IN, &OUT));
    ds_girdi(&IN, 2000 * DS_MS, 2u, true); /* seq arttı, t artmadı */
    TEST_ASSERT_EQUAL(IDA_RC_BAD_INPUT, ida_core_step(&C, &IN, &OUT));
    ds_girdi(&IN, 2005 * DS_MS, 2u, false); /* step'te IMU yok */
    TEST_ASSERT_EQUAL(IDA_RC_BAD_INPUT, ida_core_step(&C, &IN, &OUT));
    ds_girdi(&IN, 2005 * DS_MS, 2u, true); /* tick'te IMU var */
    TEST_ASSERT_EQUAL(IDA_RC_BAD_INPUT, ida_core_tick(&C, &IN, &OUT));
    const uint32_t onceki = 3u;
    TEST_ASSERT_EQUAL_UINT32(onceki, OUT.status.cnt.bad_input);
    ds_girdi(&IN, 2005 * DS_MS, 2u, true);
    IN.n_det = 9u;
    TEST_ASSERT_EQUAL(IDA_RC_BAD_INPUT, ida_core_step(&C, &IN, &OUT));
    ds_girdi(&IN, 2005 * DS_MS, 2u, true);
    IN.n_det_batch = 3u;
    TEST_ASSERT_EQUAL(IDA_RC_BAD_INPUT, ida_core_step(&C, &IN, &OUT));
    ds_girdi(&IN, 2005 * DS_MS, 2u, true);
    IN.narrow.mask = 0x10u;
    TEST_ASSERT_EQUAL(IDA_RC_BAD_INPUT, ida_core_step(&C, &IN, &OUT));
    ds_girdi(&IN, 2005 * DS_MS, 2u, true);
    IN.plat.link_event = 3u;
    TEST_ASSERT_EQUAL(IDA_RC_BAD_INPUT, ida_core_step(&C, &IN, &OUT));
    ds_girdi(&IN, 2005 * DS_MS, 2u, true);
    IN.plat.link_state = 2u;
    TEST_ASSERT_EQUAL(IDA_RC_BAD_INPUT, ida_core_step(&C, &IN, &OUT));
    TEST_ASSERT_EQUAL_UINT32(8u, OUT.status.cnt.bad_input);
    ds_girdi(&IN, 2005 * DS_MS, 2u, true);
    TEST_ASSERT_EQUAL(IDA_RC_OK, ida_core_step(&C, &IN, &OUT));
}

static void test_k25_seri_aritmetik(void)
{
    TEST_ASSERT_EQUAL(IDA_RC_OK, ds_kur(&C));
    ds_girdi(&IN, 2000 * DS_MS, 0xFFFFFFFEu, true); /* ilk zaman çerçevesi her seq ile */
    TEST_ASSERT_EQUAL(IDA_RC_OK, ida_core_step(&C, &IN, &OUT));
    ds_girdi(&IN, 2005 * DS_MS, 1u, true); /* sarma: fark 3 */
    TEST_ASSERT_EQUAL(IDA_RC_OK, ida_core_step(&C, &IN, &OUT));
    ds_girdi(&IN, 2010 * DS_MS, 0x80000001u, true); /* fark 2^31: eski */
    TEST_ASSERT_EQUAL(IDA_RC_OLD_SEQ, ida_core_step(&C, &IN, &OUT));
    ds_girdi(&IN, 2010 * DS_MS, 0x80000000u, true); /* fark 2^31 − 1: yeni */
    TEST_ASSERT_EQUAL(IDA_RC_OK, ida_core_step(&C, &IN, &OUT));
    ds_girdi(&IN, 2015 * DS_MS, 0x80000000u, false);
    IN.tf.present = false; /* ızgara tick'i DUP/OLD dönmez */
    TEST_ASSERT_EQUAL(IDA_RC_OK, ida_core_tick(&C, &IN, &OUT));
}

/* ---- (b3) SESSION_START ve A4-K122 ------------------------------------------------------- */

static void test_b3_reddedilen_cagride_oturum_basi(void)
{
    TEST_ASSERT_EQUAL(IDA_RC_OK, ds_kur(&C));
    ds_girdi(&IN, 2000 * DS_MS, 500u, true);
    TEST_ASSERT_EQUAL(IDA_RC_OK, ida_core_step(&C, &IN, &OUT));
    ds_girdi(&IN, 2005 * DS_MS, 501u, true);
    IN.plat.link_event = (uint8_t)IDA_LINKEV_SESSION_START;
    IN.n_samples = 17u;
    TEST_ASSERT_EQUAL(IDA_RC_BAD_INPUT, ida_core_step(&C, &IN, &OUT));
    TEST_ASSERT_EQUAL_UINT32(0u, OUT.seq); /* (b3) izi sıfırladı */
    TEST_ASSERT_TRUE((0) == (OUT.t_ref.ns));
    TEST_ASSERT_EQUAL_UINT32(1u, ds_gunluk_say(&OUT, (uint16_t)IDA_LOG_SESSION));
    /* Yeni oturumda daha küçük seq'li tf ve IMU kabul edilir. */
    ds_girdi(&IN, 2010 * DS_MS, 3u, true);
    TEST_ASSERT_EQUAL(IDA_RC_OK, ida_core_step(&C, &IN, &OUT));
    TEST_ASSERT_EQUAL_UINT32(3u, OUT.seq);
}

static void test_k122_oturum_sonu_cagrisi(void)
{
    TEST_ASSERT_EQUAL(IDA_RC_OK, ds_kur(&C));
    ds_girdi(&IN, 2000 * DS_MS, 1u, true);
    TEST_ASSERT_EQUAL(IDA_RC_OK, ida_core_step(&C, &IN, &OUT));
    ds_girdi(&IN, 2005 * DS_MS, 0u, false);
    IN.tf.present = false;
    IN.plat.link_event = (uint8_t)IDA_LINKEV_SESSION_END;
    IN.plat.link_state = (uint8_t)IDA_LINK_NONE;
    ds_mag(&IN.samples[0], 2004 * DS_MS, 1u); /* alan kısıtının ihlali BAD_INPUT değildir */
    IN.n_samples = 1u;
    TEST_ASSERT_EQUAL(IDA_RC_OK, ida_core_tick(&C, &IN, &OUT));
    ds_cikti_degismezleri(IDA_RC_OK, &IN, &OUT);
    TEST_ASSERT_EQUAL_UINT32(1u, ds_olay_say(&OUT, (uint8_t)IDA_EVT_SAFETY_CLOSE));
    TEST_ASSERT_EQUAL_UINT8(IDA_TUBE_ALL, OUT.events[0].tube);
    TEST_ASSERT_EQUAL_UINT32(1u, OUT.seq); /* son geçerli */
    TEST_ASSERT_TRUE(OUT.health.narrowed);
    TEST_ASSERT_TRUE((OUT.fc.inhibit_mask & IDA_FCI_SESSION) != 0u);
}

/* ---- A4-K113, K114 (§7 satırının (1)–(4)) ------------------------------------------------- */

static void test_k113_k114(void)
{
    TEST_ASSERT_EQUAL(IDA_RC_OK, ds_kur(&C));
    ds_girdi(&IN, 2000 * DS_MS, 40u, true);
    TEST_ASSERT_EQUAL(IDA_RC_OK, ida_core_step(&C, &IN, &OUT));
    C.emn.armed = true; /* test kancası */
    /* (1) DISARM + ARM + yinelenen tf.seq */
    ds_girdi(&IN, 2005 * DS_MS, 40u, true);
    IN.narrow.mask = IDA_NARROW_DISARM;
    IN.narrow.cmd_id[0] = 70u;
    IN.n_cmds = 1u;
    IN.cmds[0].cmd_id = 71u;
    IN.cmds[0].type = (uint8_t)IDA_CMD_ARM;
    TEST_ASSERT_EQUAL(IDA_RC_DUP_SEQ, ida_core_step(&C, &IN, &OUT));
    TEST_ASSERT_FALSE(OUT.armed);
    TEST_ASSERT_TRUE(OUT.health.narrowed);
    TEST_ASSERT_EQUAL_UINT8(IDA_ACK_ACCEPTED, ds_sonuc(&OUT, 70u)->ack);
    TEST_ASSERT_EQUAL_UINT8(IDA_ACK_REJECTED, ds_sonuc(&OUT, 71u)->ack);
    TEST_ASSERT_EQUAL_UINT16(IDA_REASON_CALL_REJECTED, ds_sonuc(&OUT, 71u)->reason);
    /* aynı girdi yeni tf.seq ile: OK, ARM NARROWED */
    IN.tf.seq = 41u;
    IN.imu.hdr.seq = 41u;
    IN.narrow.cmd_id[0] = 72u;
    IN.cmds[0].cmd_id = 73u;
    TEST_ASSERT_EQUAL(IDA_RC_OK, ida_core_step(&C, &IN, &OUT));
    TEST_ASSERT_EQUAL_UINT16(IDA_REASON_NARROWED, ds_sonuc(&OUT, 73u)->reason);
    TEST_ASSERT_EQUAL_UINT32(1u, OUT.status.cnt.widen_blocked);
    /* (2) n_samples = 17 ve yinelenen seq: BAD_INPUT */
    ds_girdi(&IN, 2010 * DS_MS, 41u, true);
    IN.n_samples = 17u;
    TEST_ASSERT_EQUAL(IDA_RC_BAD_INPUT, ida_core_step(&C, &IN, &OUT));
    /* (3) n_cmds = 9: komutlar için sonuç yok, mandal sonuçları var, ilk 8 girdideki
       daraltıcı SET_MODE uygulanmış (kip INIT değilken; A4-K127). */
    C.emn.mode = (uint8_t)IDA_MODE_GUIDED; /* test kancası */
    ds_girdi(&IN, 2010 * DS_MS, 42u, true);
    IN.narrow.mask = (uint8_t)(IDA_NARROW_DISARM | IDA_NARROW_FC_DISARM);
    IN.n_cmds = 9u;
    IN.cmds[3].cmd_id = 80u;
    IN.cmds[3].type = (uint8_t)IDA_CMD_SET_MODE;
    IN.cmds[3].u.set_mode.mode = (uint8_t)IDA_MODE_STATION_KEEP;
    TEST_ASSERT_EQUAL(IDA_RC_BAD_INPUT, ida_core_step(&C, &IN, &OUT));
    TEST_ASSERT_EQUAL_UINT8(2u, OUT.n_results);
    TEST_ASSERT_EQUAL_UINT8(IDA_MODE_STATION_KEEP, OUT.status.mode);
    ds_cikti_degismezleri(IDA_RC_BAD_INPUT, &IN, &OUT);
    /* (4) tanımsız mandal biti + DISARM: BAD_INPUT, armed = 0 */
    C.emn.armed = true;
    ds_girdi(&IN, 2010 * DS_MS, 42u, true);
    IN.narrow.mask = (uint8_t)(0x10u | IDA_NARROW_DISARM);
    TEST_ASSERT_EQUAL(IDA_RC_BAD_INPUT, ida_core_step(&C, &IN, &OUT));
    TEST_ASSERT_FALSE(OUT.armed);
    TEST_ASSERT_EQUAL_UINT8(1u, OUT.n_results);
    TEST_ASSERT_EQUAL_UINT8(IDA_ACK_ACCEPTED, OUT.results[0].ack);
}

/* ---- A4-K89, K90 ------------------------------------------------------------------------ */

static void test_k89_ic_tutarlilik(void)
{
    TEST_ASSERT_EQUAL(IDA_RC_OK, ds_kur(&C));
    ds_girdi(&IN, 2000 * DS_MS, 1u, true);
    TEST_ASSERT_EQUAL(IDA_RC_OK, ida_core_step(&C, &IN, &OUT));
    TEST_ASSERT_TRUE(OUT.health.core_ok);
    /* IMU reddi, oturum sonu: core_ok düşmez */
    ds_girdi(&IN, 2005 * DS_MS, 2u, true);
    IN.imu.hdr.flags = 0u;
    IN.plat.link_event = (uint8_t)IDA_LINKEV_SESSION_END;
    TEST_ASSERT_EQUAL(IDA_RC_IMU_REJECTED, ida_core_step(&C, &IN, &OUT));
    TEST_ASSERT_TRUE(OUT.health.core_ok);
    /* koruma sözcüğü bozuldu: core_ok 0, CORE_INTEGRITY(1) bir kez */
    C.koruma_son ^= 1u;
    ds_girdi(&IN, 2010 * DS_MS, 3u, true);
    TEST_ASSERT_EQUAL(IDA_RC_OK, ida_core_step(&C, &IN, &OUT));
    TEST_ASSERT_FALSE(OUT.health.core_ok);
    TEST_ASSERT_EQUAL_UINT32(1u, ds_gunluk_say(&OUT, (uint16_t)IDA_LOG_CORE_INTEGRITY));
    TEST_ASSERT_EQUAL_INT32(1, OUT.logs[0].arg0);
    TEST_ASSERT_EQUAL_UINT8(IDA_SEV_CRITICAL, OUT.logs[0].severity);
    ds_girdi(&IN, 2015 * DS_MS, 4u, true);
    TEST_ASSERT_EQUAL(IDA_RC_OK, ida_core_step(&C, &IN, &OUT));
    TEST_ASSERT_FALSE(OUT.health.core_ok);
    TEST_ASSERT_EQUAL_UINT32(0u, ds_gunluk_say(&OUT, (uint16_t)IDA_LOG_CORE_INTEGRITY));
    /* tamamlayıcı kopya ve geçersiz durum değeri */
    TEST_ASSERT_EQUAL(IDA_RC_OK, ds_kur(&C));
    C.kurulu_tumleyen ^= 1u;
    ds_girdi(&IN, 2000 * DS_MS, 1u, true);
    TEST_ASSERT_EQUAL(IDA_RC_OK, ida_core_step(&C, &IN, &OUT));
    TEST_ASSERT_FALSE(OUT.health.core_ok);
    TEST_ASSERT_EQUAL_UINT32(1u, ds_gunluk_say(&OUT, (uint16_t)IDA_LOG_CORE_INTEGRITY));
    TEST_ASSERT_EQUAL(IDA_RC_OK, ds_kur(&C));
    C.emn.mode = 9u;
    TEST_ASSERT_EQUAL(IDA_RC_OK, ida_core_step(&C, &IN, &OUT));
    TEST_ASSERT_FALSE(OUT.health.core_ok);
}


/* sol W2-cekirdek bulgu 1: (b)'nin koşulsuz üzerine yazdığı bozuk durum (FC durumu, kip)
   kanıtını kaybettirmez; core_ok init'e kadar 0 kalır; out NULL çağrısındaki bilgi sonraki
   yazılabilir çağrıya taşınır (A4-K89, 3.11 (b2)). */
static void test_k89_ustune_yazilan_bozulma(void)
{
    /* (1) fc.durum = 255 + daraltıcısız olağan tick */
    TEST_ASSERT_EQUAL(IDA_RC_OK, ds_kur(&C));
    ds_girdi(&IN, 2000 * DS_MS, 1u, false);
    TEST_ASSERT_EQUAL(IDA_RC_OK, ida_core_tick(&C, &IN, &OUT));
    TEST_ASSERT_TRUE(OUT.health.core_ok);
    C.fc.durum = 255u; /* hata enjeksiyonu */
    ds_girdi(&IN, 2005 * DS_MS, 2u, false);
    TEST_ASSERT_EQUAL(IDA_RC_OK, ida_core_tick(&C, &IN, &OUT));
    TEST_ASSERT_FALSE(OUT.health.core_ok);
    TEST_ASSERT_EQUAL_UINT32(1u, ds_gunluk_say(&OUT, (uint16_t)IDA_LOG_CORE_INTEGRITY));
    TEST_ASSERT_EQUAL_UINT8(IDA_FCS_SAFE, OUT.fc.state); /* daraltma yine uygulandı */
    ds_girdi(&IN, 2010 * DS_MS, 3u, false);
    TEST_ASSERT_EQUAL(IDA_RC_OK, ida_core_tick(&C, &IN, &OUT));
    TEST_ASSERT_FALSE(OUT.health.core_ok); /* mandallı; günlük bir kez */
    TEST_ASSERT_EQUAL_UINT32(0u, ds_gunluk_say(&OUT, (uint16_t)IDA_LOG_CORE_INTEGRITY));
    /* (2) out NULL çağrısında üzerine yazılan bozulma sonraki yazılabilir çağrıda */
    TEST_ASSERT_EQUAL(IDA_RC_OK, ds_kur(&C));
    ds_girdi(&IN, 2000 * DS_MS, 1u, false);
    TEST_ASSERT_EQUAL(IDA_RC_OK, ida_core_tick(&C, &IN, &OUT));
    C.fc.durum = 200u;
    ds_girdi(&IN, 2005 * DS_MS, 2u, false);
    TEST_ASSERT_EQUAL(IDA_RC_BAD_INPUT, ida_core_tick(&C, &IN, NULL));
    TEST_ASSERT_EQUAL_UINT8(IDA_FCS_SAFE, C.fc.durum);
    ds_girdi(&IN, 2010 * DS_MS, 3u, false);
    TEST_ASSERT_EQUAL(IDA_RC_OK, ida_core_tick(&C, &IN, &OUT));
    TEST_ASSERT_FALSE(OUT.health.core_ok);
    TEST_ASSERT_EQUAL_UINT32(1u, ds_gunluk_say(&OUT, (uint16_t)IDA_LOG_CORE_INTEGRITY));
    /* (3) geçersiz kip + NARROW_MODE mandalı (daraltma kipi üzerine yazar) */
    TEST_ASSERT_EQUAL(IDA_RC_OK, ds_kur(&C));
    C.emn.mode = 9u;
    ds_girdi(&IN, 2000 * DS_MS, 1u, false);
    IN.narrow.mask = IDA_NARROW_MODE;
    IN.narrow.mode = (uint8_t)IDA_MODE_MANUAL;
    TEST_ASSERT_EQUAL(IDA_RC_OK, ida_core_tick(&C, &IN, &OUT));
    TEST_ASSERT_EQUAL_UINT8(IDA_MODE_MANUAL, C.emn.mode);
    TEST_ASSERT_FALSE(OUT.health.core_ok);
    /* (4) SESSION_END etkin değerleri koşulsuz kısıtlayana yazar: tanımsız bit kanıtı */
    TEST_ASSERT_EQUAL(IDA_RC_OK, ds_kur(&C));
    C.emn.kisitli = 0xE0u;
    ds_girdi(&IN, 2000 * DS_MS, 1u, false);
    IN.plat.link_event = (uint8_t)IDA_LINKEV_SESSION_END;
    TEST_ASSERT_EQUAL(IDA_RC_OK, ida_core_tick(&C, &IN, &OUT));
    TEST_ASSERT_FALSE(OUT.health.core_ok);
    /* (5) inst0 tüp bitlerinde tanımsız bit + SESSION_END (oturum sonu 0 yazar). armed'ın
       ham bayt denetimi ayrıca vardır; 0/1 dışı bool'u çekirdeğe okutmak UBSan'da tanımsız
       davranış olduğu için burada enjekte edilmez. */
    TEST_ASSERT_EQUAL(IDA_RC_OK, ds_kur(&C));
    C.emn.inst0_tube[0] = 0x80u;
    ds_girdi(&IN, 2000 * DS_MS, 1u, false);
    IN.plat.link_event = (uint8_t)IDA_LINKEV_SESSION_END;
    TEST_ASSERT_EQUAL(IDA_RC_OK, ida_core_tick(&C, &IN, &OUT));
    TEST_ASSERT_EQUAL_UINT8(0u, C.emn.inst0_tube[0]);
    TEST_ASSERT_FALSE(OUT.health.core_ok);
}

/* A4-K66, K57: son geçerli zaman çerçevesi yokken hata dönüşünde çıkan SAFETY_CLOSE numarasızdır. */
static void test_k66_numarasiz_kapanma(void)
{
    TEST_ASSERT_EQUAL(IDA_RC_OK, ds_kur(&C));
    ds_girdi(&IN, 2000 * DS_MS, 9u, true);
    IN.plat.link_event = (uint8_t)IDA_LINKEV_SESSION_END;
    IN.n_samples = 17u;
    TEST_ASSERT_EQUAL(IDA_RC_BAD_INPUT, ida_core_step(&C, &IN, &OUT));
    ds_cikti_degismezleri(IDA_RC_BAD_INPUT, &IN, &OUT);
    TEST_ASSERT_EQUAL_UINT8(1u, OUT.n_events);
    TEST_ASSERT_EQUAL_UINT8(IDA_EVT_SAFETY_CLOSE, OUT.events[0].kind);
    TEST_ASSERT_EQUAL_UINT32(0u, OUT.events[0].seq);
    TEST_ASSERT_TRUE(OUT.events[0].t_decide.ns == 0);
    TEST_ASSERT_EQUAL_UINT32(IDA_APPLY_BY_MIN_NS + 4000000u, OUT.events[0].apply_by_ns);
}

static void gunluk_ekle(uint16_t kod, int32_t arg0)
{
    const ida_log_t g = {.t_w = {1}, .code = kod, .arg0 = arg0, .arg1 = 0};
    ida_gun_ekle(&C.gun, &g); /* test kancası: üretim sırasıyla bekletilen kayıt */
}

/* A4-K90 taşması: önce önemi en düşük (severity sayısı en büyük), eşitlerde sonra üretilen
   atılır; bekletilenler bu çağrınınkilerden önce üretilmiş sayılır; kalanlar üretim
   sırasıyla; atılan her kayıt log_dropped'a bir. */
static void test_k90_gunluk_tasmasi(void)
{
    TEST_ASSERT_EQUAL(IDA_RC_OK, ds_kur(&C)); /* RESET_INFO (NOTICE), PARAM_LOAD (INFO) */
    gunluk_ekle((uint16_t)IDA_LOG_FC_INHIBIT, 1);    /* DEBUG                         */
    gunluk_ekle((uint16_t)IDA_LOG_SESSION, 1);       /* NOTICE: tampon dolu           */
    gunluk_ekle((uint16_t)IDA_LOG_WALL_BACKSTEP, 0); /* ERROR: FC_INHIBIT atılır      */
    gunluk_ekle((uint16_t)IDA_LOG_FC_INHIBIT, 2);    /* DEBUG: kendisi atılır         */
    gunluk_ekle((uint16_t)IDA_LOG_SESSION, 2);       /* NOTICE: PARAM_LOAD atılır     */
    gunluk_ekle((uint16_t)IDA_LOG_SESSION, 3);       /* NOTICE, eşitlerin en sonu: atılır */
    ds_girdi(&IN, 2000 * DS_MS, 1u, false);
    TEST_ASSERT_EQUAL(IDA_RC_OK, ida_core_tick(&C, &IN, &OUT));
    ds_cikti_degismezleri(IDA_RC_OK, &IN, &OUT);
    TEST_ASSERT_EQUAL_UINT8(4u, OUT.n_logs);
    TEST_ASSERT_EQUAL_UINT16(IDA_LOG_RESET_INFO, OUT.logs[0].code);
    TEST_ASSERT_EQUAL_UINT16(IDA_LOG_SESSION, OUT.logs[1].code);
    TEST_ASSERT_EQUAL_INT32(1, OUT.logs[1].arg0);
    TEST_ASSERT_EQUAL_UINT16(IDA_LOG_WALL_BACKSTEP, OUT.logs[2].code);
    TEST_ASSERT_EQUAL_UINT16(IDA_LOG_SESSION, OUT.logs[3].code);
    TEST_ASSERT_EQUAL_INT32(2, OUT.logs[3].arg0);
    TEST_ASSERT_EQUAL_UINT32(4u, OUT.status.cnt.log_dropped);
    /* Bu çağrının daha önemli kaydı bekletilen daha önemsizini atar. */
    TEST_ASSERT_EQUAL(IDA_RC_OK, ds_kur(&C));
    gunluk_ekle((uint16_t)IDA_LOG_FC_INHIBIT, 1); /* DEBUG */
    gunluk_ekle((uint16_t)IDA_LOG_FC_INHIBIT, 2); /* DEBUG */
    ds_girdi(&IN, 2000 * DS_MS, 1u, false);
    IN.plat.link_event = (uint8_t)IDA_LINKEV_SESSION_START; /* SESSION (NOTICE) üretir */
    TEST_ASSERT_EQUAL(IDA_RC_OK, ida_core_tick(&C, &IN, &OUT));
    TEST_ASSERT_EQUAL_UINT8(4u, OUT.n_logs);
    TEST_ASSERT_EQUAL_UINT16(IDA_LOG_RESET_INFO, OUT.logs[0].code);
    TEST_ASSERT_EQUAL_UINT16(IDA_LOG_PARAM_LOAD, OUT.logs[1].code);
    TEST_ASSERT_EQUAL_UINT16(IDA_LOG_FC_INHIBIT, OUT.logs[2].code);
    TEST_ASSERT_EQUAL_INT32(1, OUT.logs[2].arg0);
    TEST_ASSERT_EQUAL_UINT16(IDA_LOG_SESSION, OUT.logs[3].code);
    TEST_ASSERT_EQUAL_UINT32(1u, OUT.status.cnt.log_dropped);
}

/* (7) out NULL + SESSION_START (3.11: (b3) (b2)'den önce). */
static void test_b3_out_null_cagride(void)
{
    TEST_ASSERT_EQUAL(IDA_RC_OK, ds_kur(&C));
    ds_girdi(&IN, 2000 * DS_MS, 500u, true);
    TEST_ASSERT_EQUAL(IDA_RC_OK, ida_core_step(&C, &IN, &OUT));
    /* Arka plan dilimi say, sonra out NULL + SESSION_START + yanlış bg_slices + geri giden
       now_w. */
    const ida_wtime_t w = {2001 * DS_MS};
    TEST_ASSERT_EQUAL(IDA_BG_IDLE, ida_core_background(&C, w));
    ds_girdi(&IN, 1990 * DS_MS, 501u, true);
    IN.plat.link_event = (uint8_t)IDA_LINKEV_SESSION_START;
    IN.bg_slices = 7u;
    TEST_ASSERT_EQUAL(IDA_RC_BAD_INPUT, ida_core_step(&C, &IN, NULL));
    TEST_ASSERT_FALSE(C.cer.tf_var); /* zaman çerçevesi izi sıfırlandı */
    /* Eski tf.seq artık DUP_SEQ vermez; bg_slices karşılaştırılmadı ve sayım sıfırlandı;
       wall_backstep sayıldı, WALL_BACKSTEP ve SESSION kayıtları bu çağrıda verilir. */
    ds_girdi(&IN, 2010 * DS_MS, 500u, true);
    IN.bg_slices = 0u;
    TEST_ASSERT_EQUAL(IDA_RC_OK, ida_core_step(&C, &IN, &OUT));
    ds_cikti_degismezleri(IDA_RC_OK, &IN, &OUT);
    TEST_ASSERT_EQUAL_UINT32(0u, OUT.status.cnt.bg_slice_mismatch);
    TEST_ASSERT_EQUAL_UINT32(0u, ds_gunluk_say(&OUT, (uint16_t)IDA_LOG_BG_SLICE_MISMATCH));
    TEST_ASSERT_EQUAL_UINT32(1u, OUT.status.cnt.wall_backstep);
    TEST_ASSERT_EQUAL_UINT32(1u, ds_gunluk_say(&OUT, (uint16_t)IDA_LOG_WALL_BACKSTEP));
    TEST_ASSERT_EQUAL_UINT32(1u, ds_gunluk_say(&OUT, (uint16_t)IDA_LOG_SESSION));
    TEST_ASSERT_EQUAL_UINT32(0u, OUT.status.cnt.bad_input);
    TEST_ASSERT_EQUAL_UINT32(500u, OUT.seq);
}

/* 3.8 step_miss: taban çağrısındaki prev_overrun ve birikmiş sayılar sayılmaz. */
static void test_step_miss_tabani(void)
{
    TEST_ASSERT_EQUAL(IDA_RC_OK, ds_kur(&C));
    ds_girdi(&IN, 2000 * DS_MS, 1u, true);
    IN.plat.prev_overrun = true;
    IN.plat.cnt_batch_skipped = 5u;
    IN.plat.cnt_batch_merged = 2u;
    /* Init'ten sonraki ilk çağrı out NULL olsa da tabandır (3.8, 3.11 (b2)); tabanın
       prev_overrun'u sayılmaz. */
    TEST_ASSERT_EQUAL(IDA_RC_BAD_INPUT, ida_core_step(&C, &IN, NULL));
    IN.plat.prev_overrun = false;
    TEST_ASSERT_EQUAL(IDA_RC_OK, ida_core_step(&C, &IN, &OUT));
    TEST_ASSERT_EQUAL_UINT32(0u, OUT.status.cnt.step_miss);
    ds_girdi(&IN, 2005 * DS_MS, 2u, true);
    IN.plat.prev_overrun = true;
    IN.plat.cnt_batch_skipped = 7u;
    IN.plat.cnt_batch_merged = 3u;
    TEST_ASSERT_EQUAL(IDA_RC_OK, ida_core_step(&C, &IN, &OUT));
    TEST_ASSERT_EQUAL_UINT32(4u, OUT.status.cnt.step_miss); /* 2 + 1 + 1 */
    /* Sonraki out NULL çağrısı sayım yapmaz, artışlar tabana göre sonra sayılır. */
    ds_girdi(&IN, 2010 * DS_MS, 3u, true);
    IN.plat.prev_overrun = true;
    IN.plat.cnt_batch_skipped = 9u;
    IN.plat.cnt_batch_merged = 3u;
    TEST_ASSERT_EQUAL(IDA_RC_BAD_INPUT, ida_core_step(&C, &IN, NULL));
    IN.plat.prev_overrun = false;
    TEST_ASSERT_EQUAL(IDA_RC_OK, ida_core_step(&C, &IN, &OUT));
    TEST_ASSERT_EQUAL_UINT32(6u, OUT.status.cnt.step_miss);
}

/* A4-K90: init sonrası ilk yazılabilir çağrıda FC_STATE/FC_INHIBIT yok; sonra değişimde var. */
static void test_k90_fc_gunluk_tabani(void)
{
    TEST_ASSERT_EQUAL(IDA_RC_OK, ds_kur(&C));
    ds_girdi(&IN, 2000 * DS_MS, 1u, false);
    TEST_ASSERT_EQUAL(IDA_RC_OK, ida_core_tick(&C, &IN, &OUT));
    TEST_ASSERT_EQUAL_UINT32(0u, ds_gunluk_say(&OUT, (uint16_t)IDA_LOG_FC_STATE));
    TEST_ASSERT_EQUAL_UINT32(0u, ds_gunluk_say(&OUT, (uint16_t)IDA_LOG_FC_INHIBIT));
    const uint32_t onceki = OUT.fc.inhibit_mask;
    ds_girdi(&IN, 2005 * DS_MS, 2u, false);
    IN.plat.link_state = (uint8_t)IDA_LINK_NONE; /* SESSION engeli kurulur */
    TEST_ASSERT_EQUAL(IDA_RC_OK, ida_core_tick(&C, &IN, &OUT));
    TEST_ASSERT_EQUAL_UINT32(1u, ds_gunluk_say(&OUT, (uint16_t)IDA_LOG_FC_INHIBIT));
    TEST_ASSERT_EQUAL_UINT32(0u, ds_gunluk_say(&OUT, (uint16_t)IDA_LOG_FC_STATE));
    TEST_ASSERT_EQUAL_UINT32(onceki | IDA_FCI_SESSION, OUT.fc.inhibit_mask);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_k89_ic_tutarlilik);
    RUN_TEST(test_k89_ustune_yazilan_bozulma);
    RUN_TEST(test_k90_gunluk_tasmasi);
    RUN_TEST(test_k66_numarasiz_kapanma);
    RUN_TEST(test_info);
    RUN_TEST(test_init_sirasi);
    RUN_TEST(test_acilis_gunlugu_ilk_yazilabilir_cagrida);
    RUN_TEST(test_null_satirlari);
    RUN_TEST(test_k126_kurulmamis_cikti_sabit);
    RUN_TEST(test_oncelik_tablosu);
    RUN_TEST(test_414_step_ok);
    RUN_TEST(test_414_tick_ve_izgara);
    RUN_TEST(test_414_hata_satirlari);
    RUN_TEST(test_k27_desenden_bagimsiz);
    RUN_TEST(test_k10_geri_adim);
    RUN_TEST(test_k15_k17_k31);
    RUN_TEST(test_k25_seri_aritmetik);
    RUN_TEST(test_b3_reddedilen_cagride_oturum_basi);
    RUN_TEST(test_k122_oturum_sonu_cagrisi);
    RUN_TEST(test_k113_k114);
    RUN_TEST(test_b3_out_null_cagride);
    RUN_TEST(test_step_miss_tabani);
    RUN_TEST(test_k90_fc_gunluk_tabani);
    return UNITY_END();
}
