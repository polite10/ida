/*
 * A4-K24 — belirlenimcilik; görev belgesi kabul md.4 (yer tutucu çekirdek hiçbir girdi
 * dizisinde thr_cmd ≠ 0, SAFETY_OPEN ya da FIRE üretmez); A4-K20/K21 (aynı süreçte iki
 * örnek bağımsızdır).
 *
 * Sabit tohumlu sözde rastgele (saat yok) uzun diziler: geçerli ve geçersiz girdiler
 * karışık (sıra yinelenmesi ve eskimesi, aralık dışı ve geri adımlı duvar saati, kapasite
 * aşımı, tanımsız mandal biti, rastgele komut türleri ve yükleri, emniyet örnekleri,
 * oturum olayları, out NULL, NULL işaretçiler, arka plan çağrıları, yeniden init ve
 * kurulmamış dönemler). Her çağrı dört örnekte iç içe yürür (dönüş ve alan alan
 * serileştirilmiş çıktı her çağrıda karşılaştırılır; çıktı tamponları farklı desenlerle
 * dolu) ve genel değişmezler her çağrıda denetlenir. Aynı dizi ardından tek örnekle
 * yeniden koşulur; çıktı özetleri iç içe koşununkilerle bayt bayt aynı olmalıdır.
 */
#include <stdio.h>
#include <string.h>

#include "kabul_cekirdek.h"
#include "unity.h"

#define ISLEM_SAYISI 4000

typedef struct {
    kc_rng_t r;
    int64_t now;
    int64_t init_now;
    uint32_t seq;
    int64_t tf_t;
    uint32_t imu_seq;
    uint32_t ornek_seq[16];
    uint32_t cmd_id;
    uint32_t son_cmd[8];
    bool out_null;
    /* istatistik */
    unsigned n_cagri;
    unsigned n_rc[9];
    unsigned n_out_null;
    unsigned n_bg;
    unsigned n_init;
    unsigned n_kapanma;
} uretec_t;

static ida_in_t g_in;
static ida_ping360_t g_ping;
static uint8_t g_ser[32768];
static uint64_t g_ozet[2][ISLEM_SAYISI];

void setUp(void)
{
}

void tearDown(void)
{
}

static uint32_t zar(uretec_t *g, uint32_t n)
{
    return kc_rng_aralik(&g->r, n);
}

static uint64_t fnv(const uint8_t *p, size_t n)
{
    uint64_t h = UINT64_C(0xCBF29CE484222325);
    for (size_t i = 0; i < n; ++i) {
        h ^= p[i];
        h *= UINT64_C(0x100000001B3);
    }
    return h;
}

static void rastgele_bayt(uretec_t *g, void *p, size_t n)
{
    unsigned char *b = (unsigned char *)p;
    for (size_t i = 0; i < n; ++i) {
        b[i] = (unsigned char)(kc_rng_u64(&g->r) & 0xFFu);
    }
}

static float rastgele_f(uretec_t *g)
{
    uint32_t u = zar(g, 100u);
    if (u < 3u) {
        uint32_t nan = 0x7FC00000u;
        float f;
        memcpy(&f, &nan, sizeof f);
        return f;
    }
    return (float)((int32_t)zar(g, 2001u) - 1000) / 500.0f;
}

static uint32_t komut_kimligi(uretec_t *g)
{
    uint32_t id;
    if (zar(g, 100u) < 8u) {
        id = g->son_cmd[zar(g, 8u)]; /* yakın zamanda kullanılmış kimlik */
    } else {
        id = ++g->cmd_id;
    }
    g->son_cmd[zar(g, 8u)] = id;
    return id;
}

static void komut_uret(uretec_t *g, ida_in_t *in)
{
    uint8_t tur = (uint8_t)(zar(g, 100u) < 90u ? 1u + zar(g, 16u) : zar(g, 256u));
    ida_cmd_t *c = kc_komut_ekle(in, komut_kimligi(g), tur);
    rastgele_bayt(g, &c->u, sizeof c->u);
    switch (tur) {
    case IDA_CMD_SET_MODE:
        c->u.set_mode.mode = (uint8_t)zar(g, 10u);
        break;
    case IDA_CMD_FC:
        c->u.fc.op = (uint8_t)zar(g, 10u);
        c->u.fc.tube = (uint8_t)(zar(g, 10u) < 8u ? zar(g, 2u) : zar(g, 256u));
        c->u.fc.arg0 = zar(g, 10u) < 8u ? 0u : zar(g, 5u);
        c->u.fc.arg1 = 0u;
        break;
    case IDA_CMD_PARAM_GET:
    case IDA_CMD_PARAM_SET: {
        uint16_t n = kc_param_sayisi();
        if (n > 0u && zar(g, 10u) < 8u) {
            const ida_param_desc_t *d = kc_param_desc((uint16_t)zar(g, n));
            c->u.param.id = d->id;
            c->u.param.type = zar(g, 10u) < 8u ? d->type : (uint8_t)zar(g, 5u);
            c->u.param.v = zar(g, 2u) == 0u ? d->def.v : d->max.v;
        }
        break;
    }
    case IDA_CMD_MANUAL:
        for (unsigned k = 0; k < 6u; ++k) {
            c->u.manual.axes[k] = rastgele_f(g);
        }
        break;
    default:
        break;
    }
}

static void ornek_uret(uretec_t *g, ida_in_t *in, int64_t t, int64_t now_call)
{
    static const uint8_t turler[5] = {IDA_SENS_SAFETY, IDA_SENS_SAFETY, IDA_SENS_MAG, IDA_SENS_BARO, IDA_SENS_LEAK};
    uint8_t tur = turler[zar(g, 5u)];
    uint8_t inst = (uint8_t)(zar(g, 10u) < 8u ? 0u : zar(g, 5u));
    uint32_t *s = &g->ornek_seq[tur];
    uint32_t seq = zar(g, 10u) < 9u ? ++(*s) : *s;
    int64_t tm = zar(g, 10u) < 9u ? t : t - (int64_t)zar(g, 20u) * KC_MS;
    ida_sample_t *o = kc_ornek_ekle(in, tur, inst, seq, tm < 0 ? 0 : tm);
    o->hdr.t_rx.ns = zar(g, 50u) == 0u ? now_call + KC_MS : now_call;
    if (zar(g, 20u) == 0u) {
        o->hdr.flags = 0u;
    }
    if (tur == IDA_SENS_SAFETY) {
        o->u.safety.estop = (uint8_t)(zar(g, 10u) < 8u ? zar(g, 2u) : zar(g, 4u));
        o->u.safety.tube[0] = (uint8_t)(zar(g, 10u) < 8u ? zar(g, 16u) : zar(g, 256u));
        o->u.safety.tube[1] = (uint8_t)zar(g, 16u);
        if (zar(g, 15u) == 0u && in->n_samples < IDA_PORT_MAX_SAMPLES) {
            /* eşit anahtarlı ikinci örnek: aynı ya da çelişkili içerik */
            in->samples[in->n_samples] = *o;
            if (zar(g, 2u) == 0u) {
                in->samples[in->n_samples].u.safety.estop = (uint8_t)(o->u.safety.estop ^ 1u);
            }
            in->n_samples = (uint8_t)(in->n_samples + 1u);
        }
    } else if (tur == IDA_SENS_LEAK) {
        o->u.leak.mask = (uint8_t)zar(g, 4u);
    }
}

static kc_islev_t girdi_uret(uretec_t *g, ida_in_t *in)
{
    bool step = zar(g, 3u) != 0u;
    g->now += zar(g, 20u) == 0u ? (int64_t)zar(g, 30u) * KC_MS : KC_P;
    int64_t now_call = g->now;
    uint32_t u = zar(g, 100u);
    if (u < 4u) {
        int64_t geri = (int64_t)zar(g, 10u) * KC_MS;
        now_call = g->now - geri < g->init_now ? g->init_now : g->now - geri;
    } else if (u < 6u) {
        now_call = zar(g, 2u) == 0u ? -1 : IDA_TIME_MAX_NS + 1;
    }
    kc_in_bos(in);
    in->now_w.ns = now_call;
    in->tf.present = step ? zar(g, 20u) != 0u : zar(g, 2u) == 0u;
    if (in->tf.present) {
        uint32_t s = zar(g, 100u);
        int64_t t = g->now;
        if (s < 85u) {
            g->seq += 1u + (zar(g, 10u) == 0u ? 1u : 0u);
            in->tf.seq = g->seq;
        } else if (s < 90u) {
            in->tf.seq = g->seq;
        } else if (s < 95u) {
            in->tf.seq = g->seq - 1u - zar(g, 5u);
        } else {
            in->tf.seq = (uint32_t)kc_rng_u64(&g->r);
        }
        if (zar(g, 30u) == 0u) {
            t = g->tf_t; /* A4-K15 ihlali olabilir */
        }
        g->tf_t = t;
        in->tf.t.ns = t;
        in->tf.t_rx.ns = zar(g, 40u) == 0u ? t + KC_MS : t;
    } else {
        in->tf.seq = (uint32_t)kc_rng_u64(&g->r);
    }
    int64_t t_olcum = in->tf.present ? in->tf.t.ns : g->now;
    in->imu_present = step;
    if (zar(g, 50u) == 0u) {
        in->imu_present = !in->imu_present;
    }
    if (in->imu_present) {
        kc_imu_kur(&in->imu, ++g->imu_seq, t_olcum);
        in->imu.hdr.t_rx.ns = now_call < 0 || now_call > IDA_TIME_MAX_NS ? g->now : now_call;
        uint32_t b = zar(g, 40u);
        if (b == 0u) {
            in->imu.hdr.flags = 0u;
        } else if (b == 1u) {
            in->imu.imu.gyro[1] = rastgele_f(g) * 1.0e30f * 1.0e30f;
        } else if (b == 2u) {
            in->imu.hdr.seq = g->imu_seq - 1u;
        } else if (b == 3u) {
            in->imu.hdr.t_meas.ns = t_olcum + KC_MS;
        }
    }
    unsigned n_ornek = zar(g, 4u) == 0u ? zar(g, 5u) : 0u;
    for (unsigned k = 0; k < n_ornek && in->n_samples < IDA_PORT_MAX_SAMPLES; ++k) {
        ornek_uret(g, in, t_olcum, in->imu.hdr.t_rx.ns > 0 ? in->imu.hdr.t_rx.ns : g->now);
    }
    unsigned n_komut = zar(g, 3u) == 0u ? 1u + zar(g, 8u) : 0u;
    for (unsigned k = 0; k < n_komut && in->n_cmds < IDA_PORT_MAX_CMDS; ++k) {
        komut_uret(g, in);
    }
    if (zar(g, 8u) == 0u) {
        uint8_t m = (uint8_t)(zar(g, 30u) == 0u ? zar(g, 256u) : zar(g, 16u));
        kc_mandal_kur(in, (uint8_t)(m & IDA_NARROW_DEFINED), komut_kimligi(g));
        in->narrow.mask = m;
        in->narrow.mode = (uint8_t)(zar(g, 10u) < 8u ? 2u + zar(g, 4u) : zar(g, 9u));
        for (unsigned k = 0; k < IDA_PORT_N_NARROW; ++k) {
            in->narrow.coalesced[k] = (uint8_t)zar(g, 3u);
        }
    }
    uint32_t l = zar(g, 100u);
    if (l < 3u) {
        in->plat.link_event = (uint8_t)IDA_LINKEV_SESSION_START;
    } else if (l < 6u) {
        in->plat.link_event = (uint8_t)IDA_LINKEV_SESSION_END;
        if (zar(g, 2u) == 0u) {
            /* oturum sonu çağrısı biçimi (A4-K122) */
            step = false;
            in->tf.present = false;
            in->imu_present = false;
        }
    } else if (l < 7u) {
        in->plat.link_event = (uint8_t)(3u + zar(g, 250u));
    }
    uint32_t ls = zar(g, 100u);
    in->plat.link_state = (uint8_t)(ls < 90u ? IDA_LINK_RUNNING : (ls < 99u ? IDA_LINK_NONE : 2u + zar(g, 5u)));
    in->plat.param_store = (uint8_t)(zar(g, 10u) < 8u ? IDA_PSTORE_IDLE : zar(g, 4u));
    in->plat.param_gen = zar(g, 4u);
    in->plat.prev_overrun = zar(g, 20u) == 0u;
    in->plat.prev_call_ns = zar(g, 4000000u);
    if (zar(g, 10u) == 0u) {
        in->n_evres = (uint8_t)(1u + zar(g, 2u));
        for (unsigned k = 0; k < in->n_evres; ++k) {
            ida_event_result_t *e = &in->evres[k];
            e->event_id = zar(g, 2u) == 0u ? kc_out()->fc.last_event_id : kc_rng_u64(&g->r);
            static const uint8_t sonuc[10] = {1u, 2u, 3u, 4u, 5u, 6u, 16u, 17u, 18u, 19u};
            /* Tanımsız sonuç kodunda çekirdeğin davranışı A4'te yazılı değil (belirsiz B20). */
            e->result = sonuc[zar(g, 10u)];
            e->t_apply.ns = e->result <= 2u ? t_olcum : 0;
        }
    }
    if (zar(g, 30u) == 0u) {
        in->n_det_batch = (uint8_t)zar(g, 3u);
        in->n_det = (uint8_t)zar(g, 9u);
        rastgele_bayt(g, in->det_batch, sizeof in->det_batch);
        rastgele_bayt(g, in->det, sizeof in->det);
        for (unsigned k = 0; k < IDA_PORT_MAX_DET_IN; ++k) {
            in->det_t_rx[k].ns = g->init_now;
        }
        for (unsigned k = 0; k < IDA_PORT_MAX_DET_BATCH; ++k) {
            in->det_batch_t_rx[k].ns = g->init_now;
        }
    }
    if (zar(g, 30u) == 0u) {
        rastgele_bayt(g, &g_ping, sizeof g_ping);
        g_ping.hdr.type = (uint8_t)IDA_SENS_PING360;
        g_ping.hdr.flags = (uint16_t)IDA_HF_VALID;
        g_ping.hdr.t_meas.ns = t_olcum;
        g_ping.hdr.t_rx.ns = g->now;
        g_ping.hdr.inst = 0u;
        g_ping.hdr.window_ns = 0u;
        in->ping360 = &g_ping;
    }
    uint32_t k = zar(g, 60u);
    if (k == 0u) {
        in->n_samples = (uint8_t)(17u + zar(g, 3u));
    } else if (k == 1u) {
        in->n_cmds = (uint8_t)(9u + zar(g, 3u));
    } else if (k == 2u) {
        in->n_evres = 5u;
    }
    if (zar(g, 30u) == 0u) {
        kc_bg_slices_elle(zar(g, 4u));
    }
    return step ? KC_STEP : KC_TICK;
}

static void init_uret(uretec_t *g)
{
    ida_init_in_t ii;
    uint32_t v = zar(g, 10u);
    g->init_now = g->now;
    if (v < 3u) {
        kc_init_gecerli();
    } else {
        kc_init_girdi(&ii);
        ii.now_w.ns = g->now;
        ii.boot_count = 1u + zar(g, 1000u);
        ii.reset_cause = (uint8_t)zar(g, 9u);
        ii.consecutive_faults = (uint8_t)zar(g, 4u);
        if (v == 3u) {
            ii.port_minor = 9u; /* VERSION: kurulmamış dönem */
        } else if (v == 4u) {
            ii.boot_count_ok = false;
        }
        (void)kc_init(&ii);
    }
    /* kc_init_gecerli now_w = KC_T0 kullanır; duvar saati ondan geri gitmesin */
    if (g->init_now < KC_T0) {
        g->init_now = KC_T0;
    }
    if (g->now < g->init_now) {
        g->now = g->init_now;
    }
    g->n_init++;
}

/* Bir diziyi n örnekle koşar; her işlemin özetini ozet[]'e yazar. */
static void dizi_kos(uint64_t tohum, int n_ornek, bool out_null, uint64_t *ozet, uretec_t *g)
{
    memset(g, 0, sizeof *g);
    kc_rng_kur(&g->r, tohum);
    g->out_null = out_null;
    g->now = KC_T0;
    g->init_now = KC_T0;
    kc_ortam_kur(n_ornek);
    kc_init_bloksuz();
    for (int i = 0; i < ISLEM_SAYISI; ++i) {
        uint32_t secim = zar(g, 100u);
        uint64_t h = 0u;
        if (secim < 75u || (secim < 83u && !g->out_null)) {
            kc_islev_t f = girdi_uret(g, &g_in);
            ida_rc_t rc = kc_cagir(f, &g_in);
            size_t n = kc_serilestir(kc_out(), g_ser, sizeof g_ser);
            h = fnv(g_ser, n) ^ (uint64_t)rc;
            g->n_cagri++;
            g->n_rc[(int)rc]++;
            g->n_kapanma += kc_out()->n_events;
        } else if (secim < 83u) {
            kc_islev_t f = girdi_uret(g, &g_in);
            h = UINT64_C(0xB2000000) + (uint64_t)kc_cagir_out_null(f, &g_in);
            g->n_out_null++;
        } else if (secim < 97u) {
            unsigned n = 1u + zar(g, 3u);
            for (unsigned k = 0; k < n; ++k) {
                int64_t now = zar(g, 10u) == 0u ? (zar(g, 2u) == 0u ? -1 : IDA_TIME_MAX_NS + 1) : g->now;
                h = h * 31u + (uint64_t)kc_bg(now);
                g->n_bg++;
            }
        } else if (secim < 98u) {
            /* (a1): NULL işaretçiler; hiçbir şey okunmaz, yazılmaz. */
            static ida_out_t o;
            kc_islev_t f = girdi_uret(g, &g_in);
            for (int k = 0; k < n_ornek; ++k) {
                ida_rc_t a = f == KC_STEP ? ida_core_step(NULL, &g_in, &o) : ida_core_tick(NULL, &g_in, &o);
                ida_rc_t b = f == KC_STEP ? ida_core_step(kc_core(k), NULL, &o) : ida_core_tick(kc_core(k), NULL, &o);
                TEST_ASSERT_EQUAL_INT(IDA_RC_BAD_INPUT, a);
                TEST_ASSERT_EQUAL_INT(IDA_RC_BAD_INPUT, b);
            }
            h = UINT64_C(0xA1);
        } else {
            init_uret(g);
            h = UINT64_C(0x1A17);
        }
        ozet[i] = h;
    }
}

static void karsilastir_ve_raporla(const uretec_t *g, const char *ad)
{
    for (int i = 0; i < ISLEM_SAYISI; ++i) {
        char msj[96];
        (void)snprintf(msj, sizeof msj, "%s: işlem %d iç içe ve tek başına koşuda farklı (A4-K24, K20)", ad, i);
        TEST_ASSERT_TRUE_MESSAGE(g_ozet[0][i] == g_ozet[1][i], msj);
    }
    char rapor[256];
    (void)snprintf(rapor, sizeof rapor,
                   "%s: %u step/tick (OK %u, IMU_REJ %u, DUP %u, OLD %u, BAD %u, NOT_INIT %u), out NULL %u, "
                   "arka plan %u, init %u, SAFETY_CLOSE %u",
                   ad, g->n_cagri, g->n_rc[0], g->n_rc[8], g->n_rc[1], g->n_rc[2], g->n_rc[3], g->n_rc[4],
                   g->n_out_null, g->n_bg, g->n_init, g->n_kapanma);
    TEST_MESSAGE(rapor);
}

static uretec_t g_u0;
static uretec_t g_u1;

/* Bütün dağılım (out NULL çağrıları dahil). */
static void test_A4_K24_karisik_dizi_out_null_dahil(void)
{
    dizi_kos(UINT64_C(0x1DA0C0DE), KC_N_AZAMI, true, g_ozet[0], &g_u0);
    dizi_kos(UINT64_C(0x1DA0C0DE), 1, true, g_ozet[1], &g_u1);
    karsilastir_ve_raporla(&g_u0, "dizi 1");
}

/* İkinci tohum; out NULL çağrısı yok ((b2) yolu ayrı testlerde): dizinin geri kalanı,
   (b2) ile ilgili bir uyuşmazlık ilk dizide görülse de sonuna kadar koşar. */
static void test_A4_K24_karisik_dizi_out_null_yok(void)
{
    dizi_kos(UINT64_C(0x5EED2026), KC_N_AZAMI, false, g_ozet[0], &g_u0);
    dizi_kos(UINT64_C(0x5EED2026), 1, false, g_ozet[1], &g_u1);
    karsilastir_ve_raporla(&g_u0, "dizi 2");
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_A4_K24_karisik_dizi_out_null_dahil);
    RUN_TEST(test_A4_K24_karisik_dizi_out_null_yok);
    return UNITY_END();
}
