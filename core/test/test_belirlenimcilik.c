/*
 * test_belirlenimcilik.c — A4-K24 (aynı girdiyle bit düzeyinde aynı çıktı dizisi; iki örnek
 * aynı süreçte iç içe) ve kabul md.4 (yer tutucu çekirdek hiçbir girdi dizisinde thr_cmd ≠ 0,
 * SAFETY_OPEN ya da FIRE üretmez): sabit tohumlu sözde rastgele, geçerli ve geçersiz karışık
 * girdi dizileri. Test kancası kullanılmaz; yalnız port işlevleri çağrılır.
 */
#include <string.h>

#include "destek.h"
#include "unity.h"

void setUp(void);
void tearDown(void);
void setUp(void) {}
void tearDown(void) {}

#define CAGRI_SAYISI 20000u

static ida_core_t A;
static ida_core_t B;
static ida_in_t IN;
static ida_out_t OA;
static ida_out_t OB;
static ida_ping360_t P;

typedef struct {
    ds_rastgele_t r;
    uint32_t seq;
    int64_t t;
    uint32_t orn_seq[11];
    uint64_t son_olay;
} uretec_t;

static uint32_t zar(uretec_t *u, uint32_t n)
{
    return ds_rastgele(&u->r) % n;
}

static void ornek_uret(uretec_t *u, ida_sample_t *s)
{
    static const uint8_t turler[] = {IDA_SENS_MAG,  IDA_SENS_BARO, IDA_SENS_SAFETY,
                                     IDA_SENS_LEAK, IDA_SENS_BATT, IDA_SENS_GNSS,
                                     IDA_SENS_DVL,  IDA_SENS_PING1D, 0u, IDA_SENS_IMU};
    (void)memset(s, 0, sizeof(*s));
    const uint8_t tur = turler[zar(u, (uint32_t)sizeof(turler))];
    s->hdr.type = tur;
    s->hdr.inst = (uint8_t)((zar(u, 10u) == 0u) ? zar(u, 6u) : 0u);
    const uint32_t ti = (tur < 11u) ? tur : 0u;
    if (zar(u, 8u) != 0u) {
        u->orn_seq[ti]++;
    }
    s->hdr.seq = u->orn_seq[ti];
    s->hdr.t_meas.ns = u->t - (int64_t)(zar(u, 4u) * 1000000u);
    s->hdr.t_rx.ns = (zar(u, 20u) == 0u) ? (u->t + 3000000) : u->t;
    s->hdr.flags = (uint16_t)((zar(u, 12u) == 0u) ? 0u : IDA_HF_VALID);
    const bool aralik = (tur == (uint8_t)IDA_SENS_BARO) || (tur == (uint8_t)IDA_SENS_BATT);
    s->hdr.window_ns = (aralik != (zar(u, 15u) == 0u)) ? 5000000u : 0u;
    s->u.safety.estop = (uint8_t)zar(u, 3u); /* birlik: diğer türlerde de bayt olarak yazılır */
    s->u.safety.tube[0] = (uint8_t)zar(u, 32u);
    s->u.safety.tube[1] = (uint8_t)zar(u, 32u);
    if (tur == (uint8_t)IDA_SENS_MAG) {
        s->u.mag.field[0] = (zar(u, 20u) == 0u) ? ds_nan() : 2.0e-5f;
    }
    if (tur == (uint8_t)IDA_SENS_BARO) {
        s->u.baro.p_abs = 101325.0f;
    }
}

static void komut_uret(uretec_t *u, ida_cmd_t *k)
{
    (void)memset(k, 0, sizeof(*k));
    k->cmd_id = zar(u, 40u);
    k->type = (uint8_t)zar(u, 18u);
    switch (k->type) {
    case IDA_CMD_SET_MODE:
        k->u.set_mode.mode = (uint8_t)zar(u, 10u);
        break;
    case IDA_CMD_FC:
        k->u.fc.op = (uint8_t)zar(u, 10u);
        k->u.fc.tube = (uint8_t)zar(u, 3u);
        k->u.fc.arg0 = (zar(u, 8u) == 0u) ? 1u : 0u;
        break;
    case IDA_CMD_PARAM_SET:
    case IDA_CMD_PARAM_GET:
        k->u.param.id = (uint16_t)(0x0100u + zar(u, 12u));
        k->u.param.type = (uint8_t)zar(u, 4u);
        k->u.param.v.u32 = ds_rastgele(&u->r) % 60000000u;
        break;
    case IDA_CMD_MANUAL:
        k->u.manual.axes[0] = 0.5f;
        break;
    default:
        break;
    }
}

/* Bir çağrının girdisi; dönüş: 0 step, 1 tick; *out_null. */
static uint32_t girdi_uret(uretec_t *u, bool *out_null)
{
    (void)memset(&IN, 0, sizeof(IN));
    const uint32_t s = zar(u, 100u);
    if (s < 4u) {
        /* yinelenen seq */
    } else if (s < 7u) {
        u->seq -= 2u;
    } else {
        u->seq++;
        u->t += 5000000;
    }
    const bool step = zar(u, 10u) < 6u;
    IN.now_w.ns = u->t;
    const uint32_t n = zar(u, 100u);
    if (n < 2u) {
        IN.now_w.ns = -1;
    } else if (n < 5u) {
        IN.now_w.ns = u->t - 7000000;
    } else {
        /* olağan */
    }
    IN.tf.present = (zar(u, 10u) != 0u);
    IN.tf.seq = u->seq;
    IN.tf.t.ns = u->t;
    IN.tf.t_rx.ns = u->t;
    IN.imu_present = (zar(u, 50u) == 0u) ? !step : step;
    if (IN.imu_present) {
        ds_imu(&IN.imu, u->t - (int64_t)(zar(u, 4u) * 10000000u), u->seq);
        if (zar(u, 10u) == 0u) {
            IN.imu.imu.gyro[1] = ds_nan();
        }
    }
    IN.n_samples = (uint8_t)((zar(u, 50u) == 0u) ? 17u : zar(u, 4u));
    for (uint32_t i = 0u; (i < IN.n_samples) && (i < IDA_PORT_MAX_SAMPLES); i++) {
        ornek_uret(u, &IN.samples[i]);
    }
    if (zar(u, 20u) == 0u) {
        (void)memset(&P, 0, sizeof(P));
        P.hdr.type = (uint8_t)IDA_SENS_PING360;
        P.hdr.flags = (uint16_t)IDA_HF_VALID;
        P.hdr.seq = u->seq;
        P.hdr.t_meas.ns = u->t;
        P.hdr.t_rx.ns = u->t;
        P.n_samples = (uint16_t)zar(u, 1300u);
        P.angle_grad = (uint16_t)zar(u, 420u);
        P.sample_period = (uint16_t)zar(u, 3u);
        IN.ping360 = &P;
    }
    if (zar(u, 8u) == 0u) {
        IN.narrow.mask = (uint8_t)zar(u, 32u);
        IN.narrow.mode = (uint8_t)zar(u, 9u);
        IN.narrow.cmd_id[0] = zar(u, 40u);
    }
    IN.n_cmds = (uint8_t)((zar(u, 50u) == 0u) ? 9u : zar(u, 4u));
    for (uint32_t i = 0u; (i < IN.n_cmds) && (i < IDA_PORT_MAX_CMDS); i++) {
        komut_uret(u, &IN.cmds[i]);
    }
    if (zar(u, 6u) == 0u) {
        IN.n_evres = 1u;
        IN.evres[0].event_id = (zar(u, 2u) == 0u) ? u->son_olay : ds_rastgele(&u->r);
        IN.evres[0].result = (uint8_t)(1u + zar(u, 6u));
    }
    const uint32_t l = zar(u, 100u);
    if (l < 3u) {
        IN.plat.link_event = (uint8_t)IDA_LINKEV_SESSION_END;
    } else if (l < 6u) {
        IN.plat.link_event = (uint8_t)IDA_LINKEV_SESSION_START;
    } else if (l < 7u) {
        IN.plat.link_event = 3u; /* tanımsız: BAD_INPUT */
    } else {
        IN.plat.link_event = (uint8_t)IDA_LINKEV_NONE;
    }
    IN.plat.link_state = (uint8_t)((zar(u, 10u) == 0u) ? IDA_LINK_NONE : IDA_LINK_RUNNING);
    IN.plat.param_store = (uint8_t)zar(u, 4u);
    IN.plat.param_gen = zar(u, 3u);
    IN.plat.prev_overrun = (zar(u, 30u) == 0u);
    IN.plat.input_dropped = (zar(u, 30u) == 0u);
    *out_null = (zar(u, 33u) == 0u);
    return step ? 0u : 1u;
}

static ida_rc_t cagir(ida_core_t *c, uint32_t tur, bool out_null, ida_out_t *o)
{
    ida_out_t *hedef = out_null ? NULL : o;
    return (tur == 0u) ? ida_core_step(c, &IN, hedef) : ida_core_tick(c, &IN, hedef);
}

/* Arka plan çağrıları: aynı dizi iki örneğe; bg_slices çoğunlukla doğru. */
static void arka_plan(uretec_t *u, ida_core_t *c1, ida_core_t *c2)
{
    const uint32_t k = zar(u, 3u);
    uint32_t sayi = 0u;
    for (uint32_t i = 0u; i < k; i++) {
        ida_wtime_t w = {u->t};
        if (zar(u, 15u) == 0u) {
            w.ns = -5;
        }
        const ida_bg_rc_t r1 = ida_core_background(c1, w);
        if (c2 != NULL) {
            TEST_ASSERT_EQUAL(r1, ida_core_background(c2, w));
        }
        TEST_ASSERT_TRUE(r1 != IDA_BG_MORE); /* yer tutucuda iş yok */
        if (r1 != IDA_BG_BAD_INPUT) {
            sayi++;
        }
    }
    IN.bg_slices = (zar(u, 20u) == 0u) ? (sayi + 1u) : sayi;
}

static void md4_denetle(ida_rc_t rc, const ida_out_t *o)
{
    for (uint32_t i = 0u; i < IDA_PORT_N_THR; i++) {
        TEST_ASSERT_TRUE(o->thr_cmd[i] == 0.0f);
    }
    for (uint32_t i = 0u; i < (uint32_t)o->n_events; i++) {
        TEST_ASSERT_EQUAL_UINT8(IDA_EVT_SAFETY_CLOSE, o->events[i].kind);
    }
    TEST_ASSERT_FALSE(o->armed);
    ds_cikti_degismezleri(rc, &IN, o);
}

static uint64_t fnv(uint64_t h, const void *p, size_t n)
{
    const uint8_t *b = (const uint8_t *)p;
    for (size_t i = 0u; i < n; i++) {
        h ^= b[i];
        h *= 1099511628211ull;
    }
    return h;
}

static uint64_t kos(uint32_t tohum, ida_core_t *c1, ida_core_t *c2, uint32_t *thr_valid_sayisi)
{
    uretec_t u;
    (void)memset(&u, 0, sizeof(u));
    u.r.s = tohum;
    u.t = 2000000000;
    TEST_ASSERT_EQUAL(IDA_RC_OK, ds_kur(c1));
    if (c2 != NULL) {
        TEST_ASSERT_EQUAL(IDA_RC_OK, ds_kur(c2));
    }
    uint64_t h = 1469598103934665603ull;
    *thr_valid_sayisi = 0u;
    for (uint32_t i = 0u; i < CAGRI_SAYISI; i++) {
        bool out_null = false;
        const uint32_t tur = girdi_uret(&u, &out_null);
        arka_plan(&u, c1, c2);
        (void)memset(&OA, 0xA5, sizeof(OA));
        (void)memset(&OB, 0x00, sizeof(OB));
        const ida_rc_t r1 = cagir(c1, tur, out_null, &OA);
        h = fnv(h, &r1, sizeof(r1));
        if (c2 != NULL) {
            const ida_rc_t r2 = cagir(c2, tur, out_null, &OB);
            TEST_ASSERT_EQUAL(r1, r2);
            if (!out_null) {
                /* save.blob her örneğin kendi belleğini gösterir (A4-K100): işaretçi değil
                   içerik karşılaştırılır, sonra işaretçiler eşitlenir. */
                for (uint32_t j = 0u; j < (uint32_t)OA.n_results; j++) {
                    if (OA.results[j].save.blob != NULL) {
                        TEST_ASSERT_NOT_NULL(OB.results[j].save.blob);
                        TEST_ASSERT_EQUAL_MEMORY(OA.results[j].save.blob, OB.results[j].save.blob,
                                                 OA.results[j].save.len);
                        OB.results[j].save.blob = OA.results[j].save.blob;
                    }
                }
                TEST_ASSERT_EQUAL_MEMORY(&OA, &OB, sizeof(OA));
            }
        }
        if (!out_null) {
            md4_denetle(r1, &OA);
            for (uint32_t j = 0u; j < (uint32_t)OA.n_results; j++) {
                if (OA.results[j].save.blob != NULL) { /* örneğe özgü işaretçi: içerik özetlenir */
                    h = fnv(h, OA.results[j].save.blob, OA.results[j].save.len);
                    OA.results[j].save.blob = NULL;
                }
            }
            h = fnv(h, &OA, sizeof(OA));
            if (OA.thr_valid) {
                (*thr_valid_sayisi)++;
            }
            if (OA.n_events > 0u) {
                u.son_olay = OA.events[0].event_id;
            }
        }
    }
    return h;
}

static void test_k24_ve_md4(void)
{
    uint32_t n1 = 0u;
    uint32_t n2 = 0u;
    /* İki örnek iç içe (her çağrı önce A'ya sonra B'ye). */
    const uint64_t h1 = kos(0x1DA2026u, &A, &B, &n1);
    /* Aynı dizi tek başına üçüncü bir koşuda: aynı çıktı özeti. */
    const uint64_t h2 = kos(0x1DA2026u, &B, NULL, &n2);
    TEST_ASSERT_TRUE(h1 == h2);
    TEST_ASSERT_EQUAL_UINT32(n1, n2);
    /* Dizi kabul edilen ve reddedilen çağrıları birlikte içerir. */
    TEST_ASSERT_TRUE(n1 > 100u);
    TEST_ASSERT_TRUE(A.cer.cnt.bad_input > 100u);
    TEST_ASSERT_TRUE(A.cer.cnt.dup_seq > 10u);
    TEST_ASSERT_TRUE(A.cer.cnt.sample_rejected > 100u);
    /* Başka bir tohum. */
    (void)kos(0xBEEFu, &A, &B, &n1);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_k24_ve_md4);
    return UNITY_END();
}
