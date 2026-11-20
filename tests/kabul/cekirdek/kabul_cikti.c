/*
 * A4 §4.14 — dönüş koduna göre çıktının her alanı (tablonun her sütunu, günlük dahil);
 * A4-K27 (önceki içerik çıktıda kalmaz), A4-K28, A4-K126 (NOT_INIT'in sabit içeriği),
 * A4-K43, K56, K57, K58, K87, K88, K90, K92, K114, K121.
 *
 * Düzenek her çağrıyı dört örnekte, çıktı tamponları 0x00, 0xFF, 0xA5 ve sözde rastgele
 * baytlarla doldurulmuş olarak yürütür; alan alan serileştirilmiş çıktıların eşitliği her
 * çağrıda denetlenir (A4-K27: aynı girdiyle farklı desenlerden aynı çıktı). Bu dosyanın
 * testleri ayrıca sütunları tek tek doğrular.
 */
#include <string.h>

#include "kabul_cekirdek.h"
#include "unity.h"

static ida_in_t g_in;

void setUp(void)
{
    kc_ortam_kur(KC_N_AZAMI);
}

void tearDown(void)
{
}

static void surekli_komut_hata(const ida_out_t *o)
{
    TEST_ASSERT_FALSE(o->thr_valid);
    TEST_ASSERT_TRUE(o->is_safe);
    TEST_ASSERT_EQUAL_UINT32(0u, o->valid_for_ns);
    for (unsigned i = 0; i < IDA_PORT_N_THR; ++i) {
        TEST_ASSERT_TRUE(o->thr_cmd[i] == 0.0f);
    }
}

static void surekli_komut_notr(const ida_out_t *o)
{
    /* Nötr güvenli çıktı (A4-K55): silahsız örnekte is_safe = 1 ve thr_valid = 1. */
    TEST_ASSERT_TRUE(o->thr_valid);
    TEST_ASSERT_TRUE(o->is_safe);
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(IDA_VALID_FOR_DEFAULT_NS, o->valid_for_ns,
                                     "varsayılan parametrelerle valid_for_ns 100 ms (A4-K58)");
    for (unsigned i = 0; i < IDA_PORT_N_THR; ++i) {
        TEST_ASSERT_TRUE(o->thr_cmd[i] == 0.0f);
    }
}

/* Reddedilen çağrı girdisi: mandal, karışık komutlar, bir MAG örneği. */
static void karisik_ekle(ida_in_t *in, uint32_t taban)
{
    const ida_param_desc_t *d = kc_param_desc(0u);
    kc_mandal_kur(in, (uint8_t)IDA_NARROW_DISARM, taban);
    (void)kc_komut_ekle(in, taban + 10u, (uint8_t)IDA_CMD_DISARM);
    (void)kc_komut_ekle(in, taban + 11u, (uint8_t)IDA_CMD_HEARTBEAT);
    (void)kc_komut_ekle(in, taban + 12u, (uint8_t)IDA_CMD_ARM);
    kc_komut_ekle(in, taban + 13u, (uint8_t)IDA_CMD_PARAM_GET)->u.param.id = d->id;
    ida_cmd_t *ps = kc_komut_ekle(in, taban + 14u, (uint8_t)IDA_CMD_PARAM_SET);
    ps->u.param = d->def;
    ps->u.param.id = d->id;
    ps->u.param.type = d->type;
    (void)kc_komut_ekle(in, taban + 15u, (uint8_t)IDA_CMD_PARAM_SAVE);
    (void)kc_fc_ekle(in, taban + 16u, (uint8_t)IDA_FC_FIRE, 0u);
    (void)kc_ornek_ekle(in, (uint8_t)IDA_SENS_MAG, 2u, taban, kc_son_now());
}

/* Bir önceki çağrının MAG örneği (aynı seq, aynı t_meas) bu çağrıda yeniden verilir; ilk
   çağrı kabul ettiyse bu kez reddedilir (A4-K33). */
static uint32_t mag_yinele_ve_reddi_say(uint32_t seq, int64_t t_meas)
{
    kc_in_step(&g_in, kc_sonraki_seq(), kc_sonraki_t());
    (void)kc_ornek_ekle(&g_in, (uint8_t)IDA_SENS_MAG, 2u, seq, t_meas);
    TEST_ASSERT_EQUAL_INT(IDA_RC_OK, kc_step(&g_in));
    return KC_FARK(sample_rejected);
}

static void test_A4_4_14_step_OK(void)
{
    const ida_param_desc_t *d = kc_param_desc(0u);
    kc_hazirla(3);
    uint32_t seq = kc_sonraki_seq();
    int64_t t = kc_sonraki_t();
    kc_in_step(&g_in, seq, t);
    (void)kc_ornek_ekle(&g_in, (uint8_t)IDA_SENS_MAG, 2u, 40u, t);
    (void)kc_komut_ekle(&g_in, 500u, (uint8_t)IDA_CMD_HEARTBEAT);
    kc_komut_ekle(&g_in, 501u, (uint8_t)IDA_CMD_PARAM_GET)->u.param.id = d->id;
    TEST_ASSERT_EQUAL_INT(IDA_RC_OK, kc_step(&g_in));
    const ida_out_t *o = kc_out();
    surekli_komut_notr(o);
    TEST_ASSERT_FALSE(o->armed);
    TEST_ASSERT_EQUAL_UINT8(IDA_SRC_STEP, o->src);
    TEST_ASSERT_EQUAL_UINT32(seq, o->seq);
    TEST_ASSERT_TRUE(o->t_ref.ns == t);
    TEST_ASSERT_EQUAL_UINT8(0u, o->n_events);
    TEST_ASSERT_EQUAL_UINT8(2u, o->n_results);
    TEST_ASSERT_TRUE(o->results[0].ack == IDA_ACK_ACCEPTED || o->results[0].ack == IDA_ACK_UNSUPPORTED);
    /* PARAM_GET: çağrı başındaki uygulanmış değer (varsayılan; A4-K81, 3.17). */
    TEST_ASSERT_EQUAL_UINT8(IDA_RES_PARAM_VALUE, o->results[1].kind);
    TEST_ASSERT_EQUAL_UINT8(IDA_ACK_ACCEPTED, o->results[1].ack);
    TEST_ASSERT_EQUAL_UINT16(d->id, o->results[1].value.id);
    TEST_ASSERT_EQUAL_UINT8(d->type, o->results[1].value.type);
    TEST_ASSERT_EQUAL_HEX32(kc_param_ham(&d->def), kc_param_ham(&o->results[1].value));
    TEST_ASSERT_TRUE(o->nav_valid);
    TEST_ASSERT_TRUE(o->n_det_tx <= IDA_PORT_MAX_DET_OUT);
    TEST_ASSERT_TRUE(o->health.core_ok);
    /* Örnek kabul edildi: aynı örnek sonraki çağrıda reddedilir. */
    TEST_ASSERT_EQUAL_UINT32(1u, mag_yinele_ve_reddi_say(40u, t));
}

static void test_A4_4_14_tick_OK_zaman_cerceveli(void)
{
    kc_hazirla(3);
    uint32_t seq = kc_sonraki_seq();
    int64_t t = kc_sonraki_t();
    kc_in_tick(&g_in, seq, t);
    (void)kc_ornek_ekle(&g_in, (uint8_t)IDA_SENS_MAG, 2u, 41u, t);
    TEST_ASSERT_EQUAL_INT(IDA_RC_OK, kc_tick(&g_in));
    const ida_out_t *o = kc_out();
    surekli_komut_notr(o); /* A4-K56: thr_valid = is_safe */
    TEST_ASSERT_EQUAL_UINT8(IDA_SRC_TICK, o->src);
    TEST_ASSERT_EQUAL_UINT32(seq, o->seq);
    TEST_ASSERT_TRUE(o->t_ref.ns == t);
    TEST_ASSERT_FALSE(o->nav_valid);
    TEST_ASSERT_EQUAL_UINT8(0u, o->n_det_tx);
    TEST_ASSERT_FALSE(o->bg.submitted);
    TEST_ASSERT_EQUAL_UINT32(1u, mag_yinele_ve_reddi_say(41u, t));
}

static void test_A4_4_14_tick_OK_izgara_son_gecerli(void)
{
    kc_hazirla(3);
    int64_t t_son = kc_son_t();
    kc_in_izgara(&g_in, kc_sonraki_t());
    (void)kc_ornek_ekle(&g_in, (uint8_t)IDA_SENS_MAG, 2u, 42u, kc_son_now());
    TEST_ASSERT_EQUAL_INT(IDA_RC_OK, kc_tick(&g_in));
    const ida_out_t *o = kc_out();
    surekli_komut_notr(o);
    TEST_ASSERT_EQUAL_UINT32(3u, o->seq);
    TEST_ASSERT_TRUE(o->t_ref.ns == t_son);
    TEST_ASSERT_FALSE(o->nav_valid);
    /* Oturum sonu çağrısı da son geçerliyi taşır (A4-K57). */
    kc_in_oturum_sonu(&g_in, kc_son_now() + KC_P);
    TEST_ASSERT_EQUAL_INT(IDA_RC_OK, kc_tick(&g_in));
    TEST_ASSERT_EQUAL_UINT32(3u, kc_out()->seq);
    TEST_ASSERT_TRUE(kc_out()->t_ref.ns == t_son);
}

static void test_A4_4_14_step_IMU_REJECTED(void)
{
    kc_hazirla(3);
    uint32_t seq = kc_sonraki_seq();
    int64_t t = kc_sonraki_t();
    kc_in_step(&g_in, seq, t);
    g_in.imu.hdr.flags = 0u;
    kc_mandal_kur(&g_in, (uint8_t)IDA_NARROW_FC_ABORT, 600u);
    (void)kc_ornek_ekle(&g_in, (uint8_t)IDA_SENS_MAG, 2u, 43u, t);
    (void)kc_komut_ekle(&g_in, 610u, (uint8_t)IDA_CMD_HEARTBEAT);
    TEST_ASSERT_EQUAL_INT(IDA_RC_IMU_REJECTED, kc_step(&g_in));
    const ida_out_t *o = kc_out();
    surekli_komut_notr(o);
    TEST_ASSERT_EQUAL_UINT32(seq, o->seq);
    TEST_ASSERT_TRUE(o->t_ref.ns == t);
    TEST_ASSERT_EQUAL_UINT8(2u, o->n_results);
    TEST_ASSERT_EQUAL_UINT8(IDA_ACK_ACCEPTED, o->results[0].ack);
    TEST_ASSERT_FALSE(o->nav_valid);
    TEST_ASSERT_EQUAL_UINT8(0u, o->n_det_tx);
    TEST_ASSERT_FALSE(o->bg.submitted);
    TEST_ASSERT_TRUE(o->health.imu_rejected);
    TEST_ASSERT_TRUE(o->health.narrowed);
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(1u, mag_yinele_ve_reddi_say(43u, t), "IMU hariç örnekler kabul edilmeli");
}

/* Hata satırları: BAD_INPUT, DUP_SEQ, OLD_SEQ. */
static void hata_satiri(ida_rc_t beklenen, int varyant)
{
    kc_hazirla(4);
    uint32_t son_seq = kc_son_seq();
    int64_t son_t = kc_son_t();
    uint32_t seq = beklenen == IDA_RC_DUP_SEQ ? son_seq : (beklenen == IDA_RC_OLD_SEQ ? son_seq - 2u : kc_sonraki_seq());
    kc_in_step(&g_in, seq, kc_sonraki_t());
    uint32_t mag_seq = 44u + (uint32_t)varyant;
    karisik_ekle(&g_in, 700u + 100u * (uint32_t)varyant);
    g_in.samples[0].hdr.seq = mag_seq;
    if (beklenen == IDA_RC_BAD_INPUT) {
        if (varyant == 0) {
            g_in.tf.t.ns = -1; /* girdideki geçersiz zaman çıktıya yazılmaz */
        } else {
            g_in.n_cmds = 9u;  /* komutlar için sonuç yok, mandal sonucu var */
        }
    }
    TEST_ASSERT_EQUAL_INT((int)beklenen, kc_step(&g_in));
    const ida_out_t *o = kc_out();
    surekli_komut_hata(o);
    TEST_ASSERT_EQUAL_UINT8(IDA_SRC_STEP, o->src);
    TEST_ASSERT_EQUAL_UINT32(son_seq, o->seq);
    TEST_ASSERT_TRUE_MESSAGE(o->t_ref.ns == son_t, "hata dönüşünde t_ref son geçerli olmalı (4.14)");
    TEST_ASSERT_EQUAL_UINT8(0u, o->n_events);
    if (g_in.n_cmds <= IDA_PORT_MAX_CMDS) {
        TEST_ASSERT_EQUAL_UINT8(1u + 7u, o->n_results);
        TEST_ASSERT_EQUAL_UINT8(IDA_ACK_ACCEPTED, o->results[0].ack); /* mandal */
        TEST_ASSERT_EQUAL_UINT8(IDA_ACK_ACCEPTED, o->results[1].ack); /* cmds[]'teki DISARM */
        for (unsigned k = 2; k < 8u; ++k) {
            TEST_ASSERT_EQUAL_UINT8(IDA_ACK_REJECTED, o->results[k].ack);
            TEST_ASSERT_EQUAL_UINT16(IDA_REASON_CALL_REJECTED, o->results[k].reason);
        }
        TEST_ASSERT_EQUAL_UINT8(IDA_RES_PARAM_VALUE, o->results[4].kind);
        TEST_ASSERT_EQUAL_UINT8(IDA_RES_PARAM_VALUE, o->results[5].kind);
        TEST_ASSERT_EQUAL_UINT8(IDA_RES_PARAM_SAVE, o->results[6].kind);
        TEST_ASSERT_NULL(o->results[6].save.blob);
    } else {
        TEST_ASSERT_EQUAL_UINT8(1u, o->n_results);
    }
    TEST_ASSERT_FALSE(o->nav_valid);
    TEST_ASSERT_EQUAL_UINT8(0u, o->n_det_tx);
    TEST_ASSERT_FALSE(o->bg.submitted);
    TEST_ASSERT_TRUE(o->health.narrowed);
    TEST_ASSERT_TRUE(o->health.core_ok);
    /* Örnek kabul edilmedi: aynı örnek sonraki çağrıda kabul edilir. */
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(0u, mag_yinele_ve_reddi_say(mag_seq, g_in.samples[0].hdr.t_meas.ns),
                                     "reddedilen çağrının örneği kabul edilmiş (A4-K26)");
}

static void test_A4_4_14_BAD_INPUT_gecersiz_zaman(void)
{
    hata_satiri(IDA_RC_BAD_INPUT, 0);
}

static void test_A4_4_14_BAD_INPUT_n_cmds_9(void)
{
    hata_satiri(IDA_RC_BAD_INPUT, 1);
}

static void test_A4_4_14_DUP_SEQ(void)
{
    hata_satiri(IDA_RC_DUP_SEQ, 2);
}

static void test_A4_4_14_OLD_SEQ(void)
{
    hata_satiri(IDA_RC_OLD_SEQ, 3);
}

static void test_A4_K28_hatali_tick_taze_imu(void)
{
    kc_hazirla(3); /* IMU taze */
    kc_in_tick(&g_in, kc_son_seq(), kc_sonraki_t());
    TEST_ASSERT_EQUAL_INT(IDA_RC_DUP_SEQ, kc_tick(&g_in));
    surekli_komut_hata(kc_out());
    TEST_ASSERT_EQUAL_UINT8(IDA_SRC_TICK, kc_out()->src);
}

/* A4-K126: NOT_INIT çıktısı örnek belleğinden bağımsız sabit içerik (düzenek bayt bayt
   karşılaştırır; dört örneğin belleği 0x00, 0xFF, 0x5A ve sözde rastgele). */
static void not_init_dene(kc_islev_t f, int varyant)
{
    if (f == KC_STEP) {
        kc_in_step(&g_in, 9u, KC_T0 + 9 * KC_P);
    } else {
        kc_in_izgara(&g_in, KC_T0 + 9 * KC_P);
    }
    if (varyant >= 1) {
        karisik_ekle(&g_in, 800u);
        kc_mandal_kur(&g_in, (uint8_t)IDA_NARROW_DEFINED, 810u);
        g_in.plat.link_event = (uint8_t)IDA_LINKEV_SESSION_END;
    }
    if (varyant == 2) {
        g_in.n_cmds = 9u;
    }
    TEST_ASSERT_EQUAL_INT(IDA_RC_NOT_INIT, kc_cagir(f, &g_in));
    const ida_out_t *o = kc_out();
    surekli_komut_hata(o);
    TEST_ASSERT_TRUE(o->is_safe);
    TEST_ASSERT_EQUAL_UINT8(IDA_FS_DISARM, o->fs_level);
    TEST_ASSERT_EQUAL_HEX32(0x00026FFFu, o->fc.inhibit_mask);
    TEST_ASSERT_TRUE(o->health.core_ok);
    TEST_ASSERT_FALSE(o->health.narrowed);
    TEST_ASSERT_EQUAL_UINT8(0u, o->n_events);
    TEST_ASSERT_EQUAL_UINT8(0u, o->n_logs);
    if (varyant == 1) {
        TEST_ASSERT_EQUAL_UINT8(4u + 7u, o->n_results);
    } else if (varyant == 2) {
        TEST_ASSERT_EQUAL_UINT8(4u, o->n_results);
    }
}

static void test_A4_K126_NOT_INIT_hic_kurulmamis_bellek(void)
{
    for (int v = 0; v < 3; ++v) {
        not_init_dene(KC_STEP, v);
        not_init_dene(KC_TICK, v);
    }
}

static void test_A4_K126_NOT_INIT_kurulumu_bozulmus_ornek(void)
{
    ida_init_in_t ii;
    kc_hazirla(5);
    kc_init_girdi(&ii);
    ii.port_major = 3u;
    TEST_ASSERT_EQUAL_INT(IDA_RC_VERSION, kc_init(&ii));
    for (int v = 0; v < 3; ++v) {
        not_init_dene(KC_STEP, v);
        not_init_dene(KC_TICK, v);
    }
    kc_hazirla(2);
    TEST_ASSERT_EQUAL_INT(IDA_RC_BAD_INPUT, kc_init(NULL));
    not_init_dene(KC_STEP, 1);
}

/* A4-K27: her dönüş kodunda önceki içerik ya da desen çıktıda kalmaz. Düzenek dört farklı
   desenle doldurulmuş tamponların alan alan eşitliğini denetler; burada her dönüş kodu
   sırayla üretilir. */
static void test_A4_K27_desen_her_donus_kodunda(void)
{
    not_init_dene(KC_STEP, 1);
    kc_hazirla(3);
    TEST_ASSERT_EQUAL_INT(IDA_RC_OK, kc_adim());
    kc_in_tick(&g_in, kc_sonraki_seq(), kc_sonraki_t());
    TEST_ASSERT_EQUAL_INT(IDA_RC_OK, kc_tick(&g_in));
    kc_in_izgara(&g_in, kc_sonraki_t());
    TEST_ASSERT_EQUAL_INT(IDA_RC_OK, kc_tick(&g_in));
    kc_in_step(&g_in, kc_sonraki_seq(), kc_sonraki_t());
    g_in.imu.hdr.inst = 7u;
    TEST_ASSERT_EQUAL_INT(IDA_RC_IMU_REJECTED, kc_step(&g_in));
    kc_in_step(&g_in, kc_sonraki_seq(), kc_sonraki_t());
    g_in.n_det_batch = 3u;
    TEST_ASSERT_EQUAL_INT(IDA_RC_BAD_INPUT, kc_step(&g_in));
    kc_in_tick(&g_in, kc_son_seq(), kc_sonraki_t());
    TEST_ASSERT_EQUAL_INT(IDA_RC_DUP_SEQ, kc_tick(&g_in));
    kc_in_step(&g_in, kc_son_seq() - 1u, kc_sonraki_t());
    TEST_ASSERT_EQUAL_INT(IDA_RC_OLD_SEQ, kc_step(&g_in));
    kc_in_oturum_sonu(&g_in, kc_sonraki_t());
    TEST_ASSERT_EQUAL_INT(IDA_RC_OK, kc_tick(&g_in));
}

/* A4-K87: step OK'de nav_valid = 1; kestirim INIT ise t ve est_level dışındaki alanlar 0,
   q_nb = (1,0,0,0) (düzenek); tick, IMU_REJECTED ve hata dönüşlerinde 0 (düzenek). */
static void test_A4_K87_nav(void)
{
    kc_hazirla(2);
    TEST_ASSERT_EQUAL_INT(IDA_RC_OK, kc_adim());
    TEST_ASSERT_TRUE(kc_out()->nav_valid);
    TEST_ASSERT_TRUE(kc_out()->nav.est_level <= IDA_EST_FAULT);
}

/* 3.15, A4-K90: ilk yazılabilir çağrının RESET_INFO ve PARAM_LOAD'ı hata dönüşünde de
   çıkar; out NULL çağrısı yazılabilir değildir. */
static void test_A4_K90_ilk_yazilabilir_cagri_hata_donusu(void)
{
    kc_init_bloksuz();
    kc_in_step(&g_in, 1u, KC_T0 + KC_P);
    g_in.n_evres = 5u;
    TEST_ASSERT_EQUAL_INT(IDA_RC_BAD_INPUT, kc_cagir_out_null(KC_STEP, &g_in));
    TEST_ASSERT_EQUAL_INT(IDA_RC_BAD_INPUT, kc_step(&g_in));
    TEST_ASSERT_EQUAL_INT(1, kc_gunluk_say(kc_out(), (uint16_t)IDA_LOG_RESET_INFO));
    const ida_log_t *p = kc_gunluk_bul(kc_out(), (uint16_t)IDA_LOG_PARAM_LOAD);
    TEST_ASSERT_NOT_NULL(p);
    TEST_ASSERT_EQUAL_INT32((int32_t)IDA_RC_PARAM_DEFAULTS, p->arg0);
    TEST_ASSERT_EQUAL_INT32(0, p->arg1);
    TEST_ASSERT_EQUAL_UINT8(IDA_SEV_WARNING, p->severity);
    TEST_ASSERT_EQUAL_INT(IDA_RC_OK, kc_adim());
    TEST_ASSERT_EQUAL_INT(0, kc_gunluk_say(kc_out(), (uint16_t)IDA_LOG_RESET_INFO));
}

/* A4-K90: 4'ten fazla kayıtta önce önemi en düşük olanlar atılır; bekletilen kayıtlar o
   çağrıda üretilenlerden önce üretilmiş sayılır; kalanlar üretim sırasıyla yazılır. */
static void test_A4_K90_tasma_onem_sirasi(void)
{
    kc_init_bloksuz();
    TEST_ASSERT_TRUE(kc_bg(KC_T0 + 10 * KC_MS) != IDA_BG_BAD_INPUT);
    /* A: out NULL, 5 ms geri adım -> bekletilen WALL_BACKSTEP (ERROR). */
    kc_in_step(&g_in, 1u, KC_T0 + 5 * KC_MS);
    TEST_ASSERT_EQUAL_INT(IDA_RC_BAD_INPUT, kc_cagir_out_null(KC_STEP, &g_in));
    TEST_ASSERT_EQUAL_INT(IDA_BG_BAD_INPUT, kc_bg(-1));
    /* B: ilk yazılabilir çağrı: RESET_INFO (NOTICE), PARAM_LOAD (WARNING), bekletilen
       WALL_BACKSTEP, BG_BAD_INPUT, BG_SLICE_MISMATCH ve kendi WALL_BACKSTEP'i (ERROR). */
    kc_in_step(&g_in, 1u, KC_T0 + 7 * KC_MS);
    kc_bg_slices_elle(5u);
    TEST_ASSERT_EQUAL_INT(IDA_RC_OK, kc_step(&g_in));
    const ida_out_t *o = kc_out();
    TEST_ASSERT_EQUAL_UINT8(4u, o->n_logs);
    TEST_ASSERT_TRUE_MESSAGE(o->status.cnt.log_dropped >= 2u, "en az iki kayıt atılmalıydı");
    for (unsigned i = 0; i < 4u; ++i) {
        TEST_ASSERT_EQUAL_UINT8_MESSAGE(IDA_SEV_ERROR, o->logs[i].severity, "önemi en düşük kayıtlar atılmalı");
    }
    TEST_ASSERT_EQUAL_UINT16_MESSAGE(IDA_LOG_WALL_BACKSTEP, o->logs[0].code, "bekletilen kayıt ilk sırada olmalı");
    TEST_ASSERT_EQUAL_INT32(5000, o->logs[0].arg0);
    TEST_ASSERT_EQUAL_INT(2, kc_gunluk_say(o, (uint16_t)IDA_LOG_WALL_BACKSTEP));
    TEST_ASSERT_EQUAL_INT(1, kc_gunluk_say(o, (uint16_t)IDA_LOG_BG_BAD_INPUT));
    TEST_ASSERT_EQUAL_INT(1, kc_gunluk_say(o, (uint16_t)IDA_LOG_BG_SLICE_MISMATCH));
    TEST_ASSERT_EQUAL_INT(0, kc_gunluk_say(o, (uint16_t)IDA_LOG_RESET_INFO));
    TEST_ASSERT_EQUAL_INT(0, kc_gunluk_say(o, (uint16_t)IDA_LOG_PARAM_LOAD));
    TEST_ASSERT_EQUAL_UINT32(2u, o->status.cnt.wall_backstep);
    /* Atılan kayıtlar sonraki çağrıda yeniden üretilmez (düzenek). */
    TEST_ASSERT_EQUAL_INT(IDA_RC_OK, kc_adim());
}

/* 3.8 step_miss: artışlar init'ten sonraki ilk step/tick çağrısının plat değerleri taban
   alınarak sayılır; taban çağrısında görülen prev_overrun = 1 sayılmaz. */
static void test_A4_K88_step_miss_taban(void)
{
    kc_init_bloksuz();
    kc_in_step(&g_in, 1u, KC_T0 + KC_P);
    g_in.plat.prev_overrun = true;
    g_in.plat.cnt_batch_skipped = 5u;
    g_in.plat.cnt_batch_merged = 3u;
    TEST_ASSERT_EQUAL_INT(IDA_RC_OK, kc_step(&g_in));
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(0u, kc_out()->status.cnt.step_miss, "taban çağrısı sayılmamalı");
    kc_in_step(&g_in, 2u, KC_T0 + 2 * KC_P);
    g_in.plat.prev_overrun = true;
    g_in.plat.cnt_batch_skipped = 7u;
    g_in.plat.cnt_batch_merged = 3u;
    TEST_ASSERT_EQUAL_INT(IDA_RC_OK, kc_step(&g_in));
    TEST_ASSERT_EQUAL_UINT32(3u, kc_out()->status.cnt.step_miss);
    kc_in_step(&g_in, 3u, KC_T0 + 3 * KC_P);
    g_in.plat.prev_overrun = false;
    g_in.plat.cnt_batch_skipped = 7u;
    g_in.plat.cnt_batch_merged = 4u;
    TEST_ASSERT_EQUAL_INT(IDA_RC_OK, kc_step(&g_in));
    TEST_ASSERT_EQUAL_UINT32(4u, kc_out()->status.cnt.step_miss);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_A4_4_14_step_OK);
    RUN_TEST(test_A4_4_14_tick_OK_zaman_cerceveli);
    RUN_TEST(test_A4_4_14_tick_OK_izgara_son_gecerli);
    RUN_TEST(test_A4_4_14_step_IMU_REJECTED);
    RUN_TEST(test_A4_4_14_BAD_INPUT_gecersiz_zaman);
    RUN_TEST(test_A4_4_14_BAD_INPUT_n_cmds_9);
    RUN_TEST(test_A4_4_14_DUP_SEQ);
    RUN_TEST(test_A4_4_14_OLD_SEQ);
    RUN_TEST(test_A4_K28_hatali_tick_taze_imu);
    RUN_TEST(test_A4_K126_NOT_INIT_hic_kurulmamis_bellek);
    RUN_TEST(test_A4_K126_NOT_INIT_kurulumu_bozulmus_ornek);
    RUN_TEST(test_A4_K27_desen_her_donus_kodunda);
    RUN_TEST(test_A4_K87_nav);
    RUN_TEST(test_A4_K90_ilk_yazilabilir_cagri_hata_donusu);
    RUN_TEST(test_A4_K90_tasma_onem_sirasi);
    RUN_TEST(test_A4_K88_step_miss_taban);
    return UNITY_END();
}
