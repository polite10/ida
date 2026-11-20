/*
 * Daraltma ((b) adımı) ve emniyet değişmezleri — A4-K113 (b), (b2), K114, K44, K45, K46,
 * K48, K61, K73, K111, K115, K116, K125, K127–K130; yer tutucu çekirdekte genişletici
 * komutların kabul edilmemesi (A4-K104, K124, 3.17; görev belgesi M4).
 *
 * Genel değişmez (her çağrıda düzenekte): thr_cmd ≠ 0 yok, SAFETY_OPEN ve FIRE yok,
 * armed = 0, hata dönüşünde thr_valid = 0 ve valid_for_ns = 0, genişletici sonucu hiçbir
 * zaman ACCEPTED değil, daraltmalı kabul edilen çağrıda genişleticiler NARROWED.
 *
 * Etkin emniyet değerlerinin gevşetilmesi bu işte yoktur (görev belgesi M9: W4'ün işi).
 * Gevşetmeden gözlenemeyen kapanma vakaları "gevşetme" grubundadır: önce kabul edilen bir
 * inst = 0 örneğiyle gevşetme denenir; etkin değerler gevşemediyse vaka IGNORE edilir
 * (kırmızı değil, rapora yazılır); gevşediyse A4'e göre tam sınanır.
 */
#include <string.h>

#include "kabul_cekirdek.h"
#include "unity.h"

static ida_in_t g_in;
static ida_in_t g_in2;
static ida_out_t g_a;

#define FCI_EMNIYET                                                                                          \
    (IDA_FCI_ESTOP | IDA_FCI_PLUG_T0 | IDA_FCI_PLUG_T1 | IDA_FCI_PIN_T0 | IDA_FCI_PIN_T1)
#define FCI_BOS (IDA_FCI_EMPTY_T0 | IDA_FCI_EMPTY_T1)

void setUp(void)
{
    kc_ortam_kur(KC_N_AZAMI);
}

void tearDown(void)
{
}

static uint32_t g_emniyet_seq;

static ida_sample_t *emniyet(ida_in_t *in, uint8_t inst, uint8_t estop, uint8_t t0, uint8_t t1)
{
    g_emniyet_seq++;
    return kc_emniyet_ekle(in, inst, g_emniyet_seq, in->tf.present ? in->tf.t.ns : in->now_w.ns, estop, t0, t1);
}

static void emniyet_kisitli(const ida_out_t *o, const char *ad)
{
    TEST_ASSERT_EQUAL_HEX32_MESSAGE(FCI_EMNIYET, o->fc.inhibit_mask & FCI_EMNIYET, ad);
}

/* Reddedilen çağrı türleri. */
typedef enum { RET_BAD_N, RET_BAD_NOW, RET_DUP, RET_OLD, RET_OUT_NULL, RET_SAYISI } ret_t;

static const char *ret_ad(int r)
{
    static const char *const ad[RET_SAYISI] = {"BAD_INPUT (n_samples)", "BAD_INPUT (now_w)", "DUP_SEQ", "OLD_SEQ",
                                               "out NULL"};
    return ad[r];
}

/* in'i r türünde reddedilecek biçime getirir; out NULL için in değişmez. */
static void ret_hazirla(ida_in_t *in, int r)
{
    if (r == RET_BAD_N) {
        in->n_evres = 5u;
    } else if (r == RET_BAD_NOW) {
        in->now_w.ns = IDA_TIME_MAX_NS + 1;
    } else if (r == RET_DUP) {
        in->tf.seq = kc_son_seq();
    } else if (r == RET_OLD) {
        in->tf.seq = kc_son_seq() - 3u;
    }
}

static ida_rc_t ret_cagir(kc_islev_t f, ida_in_t *in, int r)
{
    static const ida_rc_t beklenen[RET_SAYISI] = {IDA_RC_BAD_INPUT, IDA_RC_BAD_INPUT, IDA_RC_DUP_SEQ, IDA_RC_OLD_SEQ,
                                                  IDA_RC_BAD_INPUT};
    ida_rc_t rc = r == RET_OUT_NULL ? kc_cagir_out_null(f, in) : kc_cagir(f, in);
    TEST_ASSERT_EQUAL_INT_MESSAGE(beklenen[r], rc, ret_ad(r));
    return rc;
}

/* ===================================================================================
 * (b): mandal bitleri ve cmds[]'teki daraltıcılar her ret türünde uygulanır (A4-K113,
 * K114, K46); reddedilen çağrıda NARROWED verilmez, widen_blocked artmaz (A4-K45).
 * =================================================================================== */

static void test_A4_K113_b_mandal_her_ret_turunde(void)
{
    for (int r = 0; r < RET_SAYISI; ++r) {
        kc_hazirla(5);
        uint8_t kip0 = kc_out()->status.mode;
        kc_in_step(&g_in, kc_sonraki_seq(), kc_sonraki_t());
        kc_mandal_kur(&g_in, (uint8_t)IDA_NARROW_DEFINED, 100u);
        g_in.narrow.mode = (uint8_t)IDA_MODE_MANUAL;
        (void)kc_komut_ekle(&g_in, 110u, (uint8_t)IDA_CMD_ARM);
        (void)kc_komut_ekle(&g_in, 111u, (uint8_t)IDA_CMD_HEARTBEAT);
        (void)kc_fc_ekle(&g_in, 112u, (uint8_t)IDA_FC_FIRE, 1u);
        ret_hazirla(&g_in, r);
        (void)ret_cagir(KC_STEP, &g_in, r);
        if (r == RET_OUT_NULL) {
            TEST_ASSERT_EQUAL_INT(IDA_RC_OK, kc_adim());
        } else {
            TEST_ASSERT_TRUE_MESSAGE(kc_out()->health.narrowed, ret_ad(r));
            TEST_ASSERT_EQUAL_UINT8(7u, kc_out()->n_results);
            TEST_ASSERT_EQUAL_UINT32_MESSAGE(0u, KC_FARK(widen_blocked), "reddedilen çağrıda widen_blocked (A4-K45)");
        }
        const ida_out_t *o = kc_out();
        TEST_ASSERT_FALSE(o->armed);
        /* A4-K127: INIT'te NARROW_MODE kipi değiştirmez; başka kipte daraltıcı hedefe geçer. */
        if (kip0 == IDA_MODE_INIT) {
            TEST_ASSERT_EQUAL_UINT8_MESSAGE(IDA_MODE_INIT, o->status.mode, ret_ad(r));
        } else {
            TEST_ASSERT_EQUAL_UINT8_MESSAGE(IDA_MODE_MANUAL, o->status.mode, ret_ad(r));
        }
    }
}

static void test_A4_K114_n_cmds_9_ilk_sekiz_uygulanir(void)
{
    /* n_cmds = 9: dizinin 8 girdisinin hepsi (b)'de okunur; 8. girdideki DISARM uygulanır
       (health.narrowed), komutlara sonuç yazılmaz. */
    kc_hazirla(3);
    kc_in_step(&g_in, kc_sonraki_seq(), kc_sonraki_t());
    for (uint32_t j = 0; j < 7u; ++j) {
        (void)kc_komut_ekle(&g_in, 120u + j, (uint8_t)IDA_CMD_HEARTBEAT);
    }
    (void)kc_komut_ekle(&g_in, 127u, (uint8_t)IDA_CMD_DISARM);
    g_in.n_cmds = 9u;
    TEST_ASSERT_EQUAL_INT(IDA_RC_BAD_INPUT, kc_step(&g_in));
    TEST_ASSERT_EQUAL_UINT8(0u, kc_out()->n_results);
    TEST_ASSERT_TRUE(kc_out()->health.narrowed);
    /* Mandal biti sonucu yine yazılır. */
    kc_in_step(&g_in, kc_sonraki_seq(), kc_sonraki_t());
    for (uint32_t j = 0; j < 8u; ++j) {
        (void)kc_komut_ekle(&g_in, 130u + j, (uint8_t)IDA_CMD_HEARTBEAT);
    }
    g_in.n_cmds = 9u;
    kc_mandal_kur(&g_in, (uint8_t)IDA_NARROW_FC_ABORT, 139u);
    TEST_ASSERT_EQUAL_INT(IDA_RC_BAD_INPUT, kc_step(&g_in));
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(1u, kc_out()->n_results, "n_cmds > 8: yalnız mandal sonucu");
    /* Daraltıcısız: health.narrowed = 0. */
    kc_in_step(&g_in, kc_sonraki_seq(), kc_sonraki_t());
    for (uint32_t j = 0; j < 8u; ++j) {
        (void)kc_komut_ekle(&g_in, 140u + j, (uint8_t)IDA_CMD_ARM);
    }
    g_in.n_cmds = 9u;
    TEST_ASSERT_EQUAL_INT(IDA_RC_BAD_INPUT, kc_step(&g_in));
    TEST_ASSERT_FALSE(kc_out()->health.narrowed);
}

/* A4-K114, K48: reddedilen çağrının komutları pencereye girmez; kabul edilen çağrıda
   işlenen her komut (UNSUPPORTED dahil) girer; mandal bitleri ve cmds[]'teki
   daraltıcılar muaftır. */
static void test_A4_K48_K114_yineleme_penceresi(void)
{
    kc_hazirla(3);
    for (int r = 0; r < RET_SAYISI; ++r) {
        kc_in_step(&g_in, kc_sonraki_seq(), kc_sonraki_t());
        (void)kc_komut_ekle(&g_in, 6000u + (uint32_t)r, (uint8_t)IDA_CMD_HEARTBEAT);
        (void)kc_komut_ekle(&g_in, 6010u + (uint32_t)r, (uint8_t)IDA_CMD_ARM);
        ret_hazirla(&g_in, r);
        (void)ret_cagir(KC_STEP, &g_in, r);
    }
    kc_in_step(&g_in, kc_sonraki_seq(), kc_sonraki_t());
    for (uint32_t r = 0; r < 4u; ++r) {
        (void)kc_komut_ekle(&g_in, 6000u + r, (uint8_t)IDA_CMD_HEARTBEAT);
        (void)kc_komut_ekle(&g_in, 6010u + r, (uint8_t)IDA_CMD_ARM);
    }
    TEST_ASSERT_EQUAL_INT(IDA_RC_OK, kc_step(&g_in));
    for (unsigned k = 0; k < 8u; ++k) {
        TEST_ASSERT_TRUE_MESSAGE(kc_out()->results[k].ack != IDA_ACK_DUPLICATE,
                                 "reddedilen çağrının komutu yineleme penceresine girmiş (A4-K114)");
    }
    /* UNSUPPORTED yanıtlanan bilinmeyen tür pencereye girer. */
    kc_in_step(&g_in, kc_sonraki_seq(), kc_sonraki_t());
    (void)kc_komut_ekle(&g_in, 6100u, 99u);
    TEST_ASSERT_EQUAL_INT(IDA_RC_OK, kc_step(&g_in));
    TEST_ASSERT_EQUAL_UINT8(IDA_ACK_UNSUPPORTED, kc_out()->results[0].ack);
    kc_in_step(&g_in, kc_sonraki_seq(), kc_sonraki_t());
    (void)kc_komut_ekle(&g_in, 6100u, 99u);
    (void)kc_komut_ekle(&g_in, 6000u, (uint8_t)IDA_CMD_HEARTBEAT);
    TEST_ASSERT_EQUAL_INT(IDA_RC_OK, kc_step(&g_in));
    TEST_ASSERT_EQUAL_UINT8(IDA_ACK_DUPLICATE, kc_out()->results[0].ack);
    TEST_ASSERT_EQUAL_UINT8(IDA_ACK_DUPLICATE, kc_out()->results[1].ack);
    /* Mandal biti ve cmds[]'teki daraltıcı yinelenen kimlikle de ACCEPTED (düzenek). */
    kc_in_step(&g_in, kc_sonraki_seq(), kc_sonraki_t());
    kc_mandal_kur(&g_in, (uint8_t)IDA_NARROW_DISARM, 6100u);
    (void)kc_komut_ekle(&g_in, 6000u, (uint8_t)IDA_CMD_DISARM);
    TEST_ASSERT_EQUAL_INT(IDA_RC_OK, kc_step(&g_in));
    kc_in_step(&g_in, kc_sonraki_seq(), kc_sonraki_t());
    kc_mandal_kur(&g_in, (uint8_t)IDA_NARROW_DISARM, 6100u);
    TEST_ASSERT_EQUAL_INT(IDA_RC_OK, kc_step(&g_in));
}

/* ===================================================================================
 * A4-K45: kabul edilen çağrıda her daraltıcı girdi bütün genişleticileri NARROWED ile
 * reddeder (dizideki sıradan bağımsız); widen_blocked her biri için artar.
 * =================================================================================== */

typedef enum {
    D_MANDAL_DISARM,
    D_MANDAL_MODE,
    D_MANDAL_FC_DISARM,
    D_MANDAL_FC_ABORT,
    D_SESSION_END,
    D_CMD_DISARM,
    D_CMD_SET_MODE,
    D_CMD_FC_DISARM,
    D_CMD_FC_ABORT,
    D_SAYISI
} daraltici_t;

static unsigned genisleticileri_ekle(ida_in_t *in, uint32_t taban, unsigned atla)
{
    unsigned n = 0;
    for (unsigned g = 0; g < 8u; ++g) {
        if (g == atla) {
            continue;
        }
        uint32_t id = taban + g;
        switch (g) {
        case 0:
            (void)kc_komut_ekle(in, id, (uint8_t)IDA_CMD_ARM);
            break;
        case 1:
            kc_komut_ekle(in, id, (uint8_t)IDA_CMD_SET_MODE)->u.set_mode.mode = (uint8_t)IDA_MODE_GUIDED;
            break;
        case 2:
            kc_komut_ekle(in, id, (uint8_t)IDA_CMD_SET_MODE)->u.set_mode.mode = (uint8_t)IDA_MODE_AUTO;
            break;
        case 3:
            (void)kc_komut_ekle(in, id, (uint8_t)IDA_CMD_FAILSAFE_RESET);
            break;
        case 4:
            (void)kc_fc_ekle(in, id, (uint8_t)IDA_FC_ARM, 0u);
            break;
        case 5:
            (void)kc_fc_ekle(in, id, (uint8_t)IDA_FC_FIRE, 1u);
            break;
        case 6:
            (void)kc_fc_ekle(in, id, (uint8_t)IDA_FC_RESET, 0u);
            break;
        default:
            (void)kc_fc_ekle(in, id, (uint8_t)IDA_FC_TEST_ENTER, 1u);
            break;
        }
        ++n;
    }
    return n;
}

static void daraltici_ekle(ida_in_t *in, int d, uint32_t id)
{
    switch ((daraltici_t)d) {
    case D_MANDAL_DISARM:
        kc_mandal_kur(in, (uint8_t)IDA_NARROW_DISARM, id);
        break;
    case D_MANDAL_MODE:
        kc_mandal_kur(in, (uint8_t)IDA_NARROW_MODE, id);
        in->narrow.mode = (uint8_t)IDA_MODE_STABILIZE;
        break;
    case D_MANDAL_FC_DISARM:
        kc_mandal_kur(in, (uint8_t)IDA_NARROW_FC_DISARM, id);
        break;
    case D_MANDAL_FC_ABORT:
        kc_mandal_kur(in, (uint8_t)IDA_NARROW_FC_ABORT, id);
        break;
    case D_SESSION_END:
        in->plat.link_event = (uint8_t)IDA_LINKEV_SESSION_END;
        break;
    case D_CMD_DISARM:
        (void)kc_komut_ekle(in, id, (uint8_t)IDA_CMD_DISARM);
        break;
    case D_CMD_SET_MODE:
        kc_komut_ekle(in, id, (uint8_t)IDA_CMD_SET_MODE)->u.set_mode.mode = (uint8_t)IDA_MODE_DEPTH_HOLD;
        break;
    case D_CMD_FC_DISARM:
        (void)kc_fc_ekle(in, id, (uint8_t)IDA_FC_DISARM, 0u);
        break;
    default:
        (void)kc_fc_ekle(in, id, (uint8_t)IDA_FC_ABORT, 1u);
        break;
    }
}

static void test_A4_K45_her_daraltici_her_genisletici(void)
{
    uint32_t taban = 20000u;
    kc_hazirla(3);
    for (int d = 0; d < (int)D_SAYISI; ++d) {
        for (int once = 0; once < 2; ++once) {
            bool cmd = d >= (int)D_CMD_DISARM;
            if (d == (int)D_SESSION_END) {
                /* Oturum sonu çağrısı (A4-K122): komutlar bulunabilir, (c)'de denetlenmez. */
                kc_in_oturum_sonu(&g_in, kc_sonraki_t());
            } else {
                kc_in_step(&g_in, kc_sonraki_seq(), kc_sonraki_t());
            }
            if (once != 0 || !cmd) {
                daraltici_ekle(&g_in, d, taban + 100u);
            }
            unsigned n_gen = genisleticileri_ekle(&g_in, taban, cmd ? (unsigned)d % 8u : 8u);
            if (once == 0 && cmd) {
                daraltici_ekle(&g_in, d, taban + 100u);
            }
            taban += 200u;
            ida_rc_t rc = kc_cagir(d == (int)D_SESSION_END ? KC_TICK : KC_STEP, &g_in);
            TEST_ASSERT_EQUAL_INT(IDA_RC_OK, rc);
            const ida_out_t *o = kc_out();
            TEST_ASSERT_TRUE(o->health.narrowed);
            unsigned nar = 0;
            for (unsigned k = 0; k < o->n_results; ++k) {
                if (o->results[k].reason == IDA_REASON_NARROWED) {
                    ++nar;
                }
            }
            TEST_ASSERT_EQUAL_UINT_MESSAGE(n_gen, nar, "her genişletici NARROWED (A4-K45)");
            TEST_ASSERT_EQUAL_UINT32_MESSAGE(n_gen, KC_FARK(widen_blocked), "widen_blocked (A4-K45)");
            if (d == (int)D_SESSION_END) {
                /* Yeni oturum. */
                kc_in_izgara(&g_in, kc_sonraki_t());
                g_in.plat.link_event = (uint8_t)IDA_LINKEV_SESSION_START;
                TEST_ASSERT_EQUAL_INT(IDA_RC_OK, kc_tick(&g_in));
                TEST_ASSERT_EQUAL_INT(IDA_RC_OK, kc_adim());
            }
        }
    }
}

static void test_A4_K45_IMU_REJECTED_cagrisinda_once_NARROWED(void)
{
    kc_hazirla(3);
    kc_in_step(&g_in, kc_sonraki_seq(), kc_sonraki_t());
    g_in.imu.hdr.flags = 0u;
    unsigned n = genisleticileri_ekle(&g_in, 30000u, 8u);
    kc_mandal_kur(&g_in, (uint8_t)IDA_NARROW_FC_DISARM, 30100u);
    TEST_ASSERT_EQUAL_INT(IDA_RC_IMU_REJECTED, kc_step(&g_in));
    TEST_ASSERT_EQUAL_UINT32(n, KC_FARK(widen_blocked));
    /* Daraltmasız IMU_REJECTED: IMU_REJECTED nedeni (düzenek; 3.17 (2)). */
    kc_in_step(&g_in, kc_sonraki_seq(), kc_sonraki_t());
    g_in.imu.hdr.flags = 0u;
    (void)genisleticileri_ekle(&g_in, 30200u, 8u);
    TEST_ASSERT_EQUAL_INT(IDA_RC_IMU_REJECTED, kc_step(&g_in));
    TEST_ASSERT_EQUAL_UINT16(IDA_REASON_IMU_REJECTED, kc_out()->results[0].reason);
}

static void test_A4_K48_daraltmali_cagrida_yinelenen_genisletici_DUPLICATE(void)
{
    kc_hazirla(3);
    kc_in_step(&g_in, kc_sonraki_seq(), kc_sonraki_t());
    (void)kc_komut_ekle(&g_in, 31000u, (uint8_t)IDA_CMD_ARM);
    TEST_ASSERT_EQUAL_INT(IDA_RC_OK, kc_step(&g_in));
    kc_in_step(&g_in, kc_sonraki_seq(), kc_sonraki_t());
    kc_mandal_kur(&g_in, (uint8_t)IDA_NARROW_DISARM, 31100u);
    (void)kc_komut_ekle(&g_in, 31000u, (uint8_t)IDA_CMD_ARM);
    TEST_ASSERT_EQUAL_INT(IDA_RC_OK, kc_step(&g_in));
    TEST_ASSERT_EQUAL_UINT8(IDA_ACK_DUPLICATE, kc_out()->results[1].ack);
}

/* ===================================================================================
 * SESSION_END (A4-K73, K116, K125, K128, K57, K63): her çağrı türünde tek
 * SAFETY_CLOSE(ALL); reddedilen çağrıda seq/t_decide son geçerli.
 * =================================================================================== */

static void yeni_oturum(void)
{
    kc_in_izgara(&g_in2, kc_sonraki_t());
    g_in2.plat.link_event = (uint8_t)IDA_LINKEV_SESSION_START;
    TEST_ASSERT_EQUAL_INT(IDA_RC_OK, kc_tick(&g_in2));
    TEST_ASSERT_EQUAL_UINT8(0u, kc_out()->n_events); /* SESSION_START kapanma üretmez */
    TEST_ASSERT_EQUAL_INT(IDA_RC_OK, kc_adim());
    TEST_ASSERT_EQUAL_INT(IDA_RC_OK, kc_adim());
}

static void kapanma_tum(const ida_out_t *o, uint32_t seq, int64_t t, const char *ad)
{
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(1u, o->n_events, ad);
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(IDA_EVT_SAFETY_CLOSE, o->events[0].kind, ad);
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(IDA_TUBE_ALL, o->events[0].tube, ad);
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(seq, o->events[0].seq, ad);
    TEST_ASSERT_TRUE_MESSAGE(o->events[0].t_decide.ns == t, ad);
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(KC_BOOT, (uint32_t)(o->events[0].event_id >> 32), ad);
}

static void test_A4_K73_session_end_her_cagri_turunde(void)
{
    kc_hazirla(4);
    for (int r = 0; r < RET_SAYISI; ++r) {
        uint32_t seq0 = kc_son_seq();
        int64_t t0 = kc_son_t();
        kc_in_tick(&g_in, kc_sonraki_seq(), kc_sonraki_t());
        g_in.plat.link_event = (uint8_t)IDA_LINKEV_SESSION_END;
        ret_hazirla(&g_in, r);
        (void)ret_cagir(KC_TICK, &g_in, r);
        if (r == RET_OUT_NULL) {
            /* Bekletilen kapanma ilk yazılabilir çağrıda, o çağrının seq/t_ref'iyle. */
            TEST_ASSERT_EQUAL_INT(IDA_RC_OK, kc_adim());
            kapanma_tum(kc_out(), kc_son_seq(), kc_son_t(), "out NULL sonrası ilk yazılabilir çağrı");
            TEST_ASSERT_EQUAL_INT(IDA_RC_OK, kc_adim());
            TEST_ASSERT_EQUAL_UINT8_MESSAGE(0u, kc_out()->n_events, "bekletilen kapanma bir kez çıkmalı");
        } else {
            const ida_out_t *o = kc_out();
            kapanma_tum(o, seq0, t0, ret_ad(r));
            TEST_ASSERT_TRUE(o->health.narrowed);
            TEST_ASSERT_EQUAL_HEX32_MESSAGE(FCI_BOS, o->fc.inhibit_mask & FCI_BOS, "SESSION_END sonrası tüpler boş (A4-K128)");
            emniyet_kisitli(o, ret_ad(r));
        }
        yeni_oturum();
    }
    /* Kabul edilen oturum sonu çağrısı ve IMU_REJECTED step'i. */
    kc_in_oturum_sonu(&g_in, kc_sonraki_t());
    (void)kc_komut_ekle(&g_in, 32000u, (uint8_t)IDA_CMD_ARM);
    TEST_ASSERT_EQUAL_INT(IDA_RC_OK, kc_tick(&g_in));
    kapanma_tum(kc_out(), kc_son_seq(), kc_son_t(), "oturum sonu çağrısı");
    TEST_ASSERT_EQUAL_UINT16(IDA_REASON_NARROWED, kc_out()->results[0].reason);
    yeni_oturum();
    kc_in_step(&g_in, kc_sonraki_seq(), kc_sonraki_t());
    g_in.imu.hdr.flags = 0u;
    g_in.plat.link_event = (uint8_t)IDA_LINKEV_SESSION_END;
    TEST_ASSERT_EQUAL_INT(IDA_RC_IMU_REJECTED, kc_step(&g_in));
    kapanma_tum(kc_out(), g_in.tf.seq, g_in.tf.t.ns, "IMU_REJECTED");
}

static void test_A4_K125_bekletilen_ve_yeni_kapanma_birlesir(void)
{
    kc_hazirla(3);
    kc_in_oturum_sonu(&g_in, kc_sonraki_t());
    TEST_ASSERT_EQUAL_INT(IDA_RC_BAD_INPUT, kc_cagir_out_null(KC_TICK, &g_in));
    kc_in_oturum_sonu(&g_in, kc_sonraki_t());
    (void)emniyet(&g_in, 0u, 1u, 0u, 0u);
    kc_mandal_kur(&g_in, (uint8_t)IDA_NARROW_DEFINED, 33000u);
    TEST_ASSERT_EQUAL_INT(IDA_RC_OK, kc_tick(&g_in));
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(1u, kc_out()->n_events, "bütün kapanmalar tek ALL'da birleşmeli (A4-K125)");
    TEST_ASSERT_EQUAL_UINT8(IDA_TUBE_ALL, kc_out()->events[0].tube);
    TEST_ASSERT_EQUAL_UINT32(0u, KC_FARK(narrow_from_rejected));
}

static void test_A4_K125_bekletilen_kapanma_her_donus_kodunda(void)
{
    for (int r = 0; r < RET_SAYISI - 1; ++r) {
        kc_hazirla(4);
        kc_in_oturum_sonu(&g_in, kc_sonraki_t());
        TEST_ASSERT_EQUAL_INT(IDA_RC_BAD_INPUT, kc_cagir_out_null(KC_TICK, &g_in));
        kc_in_step(&g_in, kc_sonraki_seq(), kc_sonraki_t());
        ret_hazirla(&g_in, r);
        (void)ret_cagir(KC_STEP, &g_in, r);
        kapanma_tum(kc_out(), kc_out()->seq, kc_out()->t_ref.ns, ret_ad(r));
        TEST_ASSERT_EQUAL_UINT32_MESSAGE(r <= RET_BAD_NOW ? 1u : 0u, KC_FARK(bad_input), "(b2) çağrısı sayılmamalı");
        TEST_ASSERT_EQUAL_INT(IDA_RC_OK, kc_adim());
        TEST_ASSERT_EQUAL_UINT8(0u, kc_out()->n_events);
    }
}

static void test_A4_K61_olay_kimligi_yoksa_kapanma_da_yok(void)
{
    ida_init_in_t ii;
    kc_init_girdi(&ii);
    ii.boot_count_ok = false;
    TEST_ASSERT_EQUAL_INT(IDA_RC_PARAM_DEFAULTS, kc_init(&ii));
    TEST_ASSERT_EQUAL_INT(IDA_RC_OK, kc_adim());
    kc_in_oturum_sonu(&g_in, kc_sonraki_t());
    TEST_ASSERT_EQUAL_INT(IDA_RC_OK, kc_tick(&g_in));
    TEST_ASSERT_EQUAL_UINT8(0u, kc_out()->n_events);
    TEST_ASSERT_TRUE(kc_out()->health.narrowed);
    TEST_ASSERT_FALSE(kc_out()->status.events_ok);
}

/* A4-K129: SAFETY_CLOSE bekleyen olay kümesi doluyken de üretilir; kimlik kesin artar. */
static void test_A4_K129_kapanma_kume_doluyken_de_uretilir(void)
{
    kc_hazirla(2);
    uint32_t onceki = 0u;
    for (int k = 0; k < 12; ++k) {
        kc_in_oturum_sonu(&g_in, kc_sonraki_t());
        TEST_ASSERT_EQUAL_INT(IDA_RC_OK, kc_tick(&g_in));
        TEST_ASSERT_EQUAL_UINT8(1u, kc_out()->n_events);
        uint32_t ctr = (uint32_t)(kc_out()->events[0].event_id & 0xFFFFFFFFu);
        TEST_ASSERT_TRUE(ctr > onceki);
        if (k == 0) {
            TEST_ASSERT_EQUAL_UINT32_MESSAGE(IDA_EVT_CTR_FIRST, ctr, "ctr init'ten sonra 1'den başlar (A4-K61)");
        } else {
            TEST_ASSERT_EQUAL_UINT32_MESSAGE(onceki + 1u, ctr, "her yeni olayda bir artar (A4-K61)");
        }
        onceki = ctr;
        yeni_oturum();
    }
}

/* ===================================================================================
 * Emniyet örnekleri, etkin değer kısıtlayıcıyken (init'ten; A4-K116, K128).
 * =================================================================================== */

static void test_A4_K116_init_kisitlayici_ve_bos(void)
{
    kc_init_bloksuz();
    TEST_ASSERT_EQUAL_INT(IDA_RC_OK, kc_adim());
    emniyet_kisitli(kc_out(), "init'te etkin değerler kısıtlayıcı (A4-K116)");
    TEST_ASSERT_EQUAL_HEX32_MESSAGE(FCI_BOS, kc_out()->fc.inhibit_mask & FCI_BOS, "A4-K128");
}

/* Zaten kısıtlayan alanın yinelenen kısıtlayıcı içeriği daraltıcı girdi değildir (A4-K44):
   kapanma yok, narrow_from_rejected yok, health.narrowed = 0 — her çağrı türünde. */
static void test_A4_K44_yinelenen_kisitlayici_icerik_daraltici_degil(void)
{
    kc_hazirla(3);
    for (int r = -1; r < RET_SAYISI; ++r) {
        kc_in_step(&g_in, kc_sonraki_seq(), kc_sonraki_t());
        (void)emniyet(&g_in, 0u, 1u, 0u, 0u);                      /* hepsi kısıtlayan */
        (void)emniyet(&g_in, 1u, 3u, (uint8_t)IDA_TUBE_LOADED, 0u); /* estop ≥ 2: reddedilir */
        if (r >= 0) {
            ret_hazirla(&g_in, r);
            (void)ret_cagir(KC_STEP, &g_in, r);
        } else {
            TEST_ASSERT_EQUAL_INT(IDA_RC_OK, kc_step(&g_in));
        }
        if (r == RET_OUT_NULL) {
            TEST_ASSERT_EQUAL_INT(IDA_RC_OK, kc_adim());
        }
        const ida_out_t *o = kc_out();
        TEST_ASSERT_EQUAL_UINT8_MESSAGE(0u, o->n_events, r < 0 ? "OK" : ret_ad(r));
        TEST_ASSERT_FALSE_MESSAGE(o->health.narrowed, r < 0 ? "OK" : ret_ad(r));
        TEST_ASSERT_EQUAL_UINT32(0u, KC_FARK(narrow_from_rejected));
        TEST_ASSERT_EQUAL_INT(0, kc_gunluk_say(o, (uint16_t)IDA_LOG_SAFETY_INPUT));
        emniyet_kisitli(o, "kısıtlayıcı kalır");
    }
}

/* Reddedilen girdinin gevşetici içeriği uygulanmaz (A4-K113 (b), K116, K130). */
static void test_A4_K116_reddedilen_girdinin_gevseten_icerigi_uygulanmaz(void)
{
    kc_hazirla(3);
    for (int r = 0; r < RET_SAYISI; ++r) {
        kc_in_step(&g_in, kc_sonraki_seq(), kc_sonraki_t());
        (void)emniyet(&g_in, 0u, 0u, KC_TUP_ACIK, KC_TUP_ACIK);
        ret_hazirla(&g_in, r);
        (void)ret_cagir(KC_STEP, &g_in, r);
        if (r != RET_OUT_NULL) {
            emniyet_kisitli(kc_out(), ret_ad(r));
            TEST_ASSERT_EQUAL_HEX32(FCI_BOS, kc_out()->fc.inhibit_mask & FCI_BOS);
        }
        TEST_ASSERT_EQUAL_INT(IDA_RC_OK, kc_adim());
        emniyet_kisitli(kc_out(), ret_ad(r));
    }
}

/* Gevşetme yalnız kabul edilen inst = 0 örneğinden; VALID = 0 (A4-K130), estop ≥ 2 ve
   tanımsız tüp biti taşıyan örnekler reddedilir ve gevşetmez (A4-K33, K116). */
static void test_A4_K116_K130_gevsetemeyen_ornekler(void)
{
    kc_hazirla(3);
    for (int v = 0; v < 4; ++v) {
        kc_in_step(&g_in, kc_sonraki_seq(), kc_sonraki_t());
        ida_sample_t *s = emniyet(&g_in, v == 0 ? 1u : 0u, 0u, KC_TUP_ACIK, KC_TUP_ACIK);
        if (v == 1) {
            s->hdr.flags = 0u; /* taşınan örnek */
        } else if (v == 2) {
            s->u.safety.estop = 2u;
        } else if (v == 3) {
            s->u.safety.tube[1] = (uint8_t)(KC_TUP_ACIK | 0x40u);
        }
        TEST_ASSERT_EQUAL_INT(IDA_RC_OK, kc_step(&g_in));
        emniyet_kisitli(kc_out(), "gevşetmemeli");
        if (v >= 1) {
            TEST_ASSERT_EQUAL_UINT32(1u, KC_FARK(sample_rejected));
            TEST_ASSERT_EQUAL_UINT32_MESSAGE(0u, KC_FARK(sample_conflict), "VALID = 0 eşit anahtar karşılaştırmasına girmez");
        }
    }
    /* Aynı çağrıda inst = 0'ın gevşeten ve inst = 1'in kısıtlayan örneği: kısıtlayıcı. */
    kc_in_step(&g_in, kc_sonraki_seq(), kc_sonraki_t());
    (void)emniyet(&g_in, 0u, 0u, KC_TUP_ACIK, KC_TUP_ACIK);
    (void)emniyet(&g_in, 1u, 1u, KC_TUP_ACIK, KC_TUP_ACIK);
    TEST_ASSERT_EQUAL_INT(IDA_RC_OK, kc_step(&g_in));
    TEST_ASSERT_TRUE_MESSAGE((kc_out()->fc.inhibit_mask & IDA_FCI_ESTOP) != 0u, "A4-K116");
}

/* A4-K115: eşit anahtarlı çelişkili çift sıradan bağımsız reddedilir; bayt bayt aynı
   çiftin biri değerlendirilir (A4-K35). */
static void cift_kos(int sira, ida_out_t *sonuc)
{
    kc_ortam_kur(2);
    kc_hazirla(3);
    kc_in_step(&g_in, kc_sonraki_seq(), kc_sonraki_t());
    ida_sample_t *a = kc_emniyet_ekle(&g_in, 0u, 50u, g_in.tf.t.ns, 0u, KC_TUP_ACIK, KC_TUP_ACIK);
    ida_sample_t *b = kc_emniyet_ekle(&g_in, 0u, 50u, g_in.tf.t.ns, 1u, KC_TUP_ACIK, KC_TUP_ACIK);
    if (sira != 0) {
        ida_sample_t t = *a;
        *a = *b;
        *b = t;
    }
    TEST_ASSERT_EQUAL_INT(IDA_RC_OK, kc_step(&g_in));
    *sonuc = *kc_out();
    TEST_ASSERT_EQUAL_UINT32(1u, KC_FARK(sample_conflict));
    TEST_ASSERT_EQUAL_UINT32(2u, KC_FARK(sample_rejected));
    TEST_ASSERT_TRUE(sonuc->health.sample_conflict);
    const ida_log_t *l = kc_gunluk_bul(sonuc, (uint16_t)IDA_LOG_SAMPLE_CONFLICT);
    TEST_ASSERT_NOT_NULL(l);
    TEST_ASSERT_EQUAL_INT32((int32_t)IDA_SENS_SAFETY, l->arg0);
    TEST_ASSERT_EQUAL_INT32(0, l->arg1);
    emniyet_kisitli(sonuc, "çelişkili çift kısıtlayıcı");
}

static void test_A4_K115_celiskili_cift_siradan_bagimsiz(void)
{
    cift_kos(0, &g_a);
    static ida_out_t b;
    cift_kos(1, &b);
    TEST_ASSERT_TRUE_MESSAGE(kc_out_esit(&g_a, &b), "çıktı örnek sırasına bağlı (A4-K35, K115)");
    /* Bayt bayt aynı çift. */
    kc_ortam_kur(KC_N_AZAMI);
    kc_hazirla(3);
    kc_in_step(&g_in, kc_sonraki_seq(), kc_sonraki_t());
    (void)kc_emniyet_ekle(&g_in, 1u, 60u, g_in.tf.t.ns, 1u, 0u, 0u);
    g_in.samples[1] = g_in.samples[0];
    g_in.n_samples = 2u;
    TEST_ASSERT_EQUAL_INT(IDA_RC_OK, kc_step(&g_in));
    TEST_ASSERT_EQUAL_UINT32(1u, KC_FARK(sample_dup_dropped));
    TEST_ASSERT_EQUAL_UINT32(0u, KC_FARK(sample_rejected));
    TEST_ASSERT_EQUAL_UINT32(0u, KC_FARK(sample_conflict));
}

/* ===================================================================================
 * Gevşetmeye bağlı vakalar (M9: gevşetme bu işte yok; gevşemediyse IGNORE).
 * =================================================================================== */

static bool gevset(void)
{
    kc_in_step(&g_in, kc_sonraki_seq(), kc_sonraki_t());
    (void)emniyet(&g_in, 0u, 0u, KC_TUP_ACIK, KC_TUP_ACIK);
    TEST_ASSERT_EQUAL_INT(IDA_RC_OK, kc_step(&g_in));
    return (kc_out()->fc.inhibit_mask & (FCI_EMNIYET | FCI_BOS)) == 0u;
}

#define GEVSETME_YOKSA_ATLA(gevsedi)                                                                          \
    if (!(gevsedi)) {                                                                                         \
        TEST_IGNORE_MESSAGE("etkin emniyet değerleri port üzerinden gevşemiyor (M9: gevşetme W4'te); "      \
                            "kapanma yolu gözlenemedi");                                                    \
    } else

static void test_A4_K116_gevsetme_ve_SAFETY_INPUT(void)
{
    kc_hazirla(3);
    bool g = gevset();
    GEVSETME_YOKSA_ATLA(g)
    {
        const ida_log_t *l = kc_gunluk_bul(kc_out(), (uint16_t)IDA_LOG_SAFETY_INPUT);
        TEST_ASSERT_NOT_NULL(l);
        TEST_ASSERT_EQUAL_INT32(0, l->arg0);
        TEST_ASSERT_EQUAL_INT32(0x0F, l->arg1);
        /* SESSION_START etkin değerleri değiştirmez, kapanma üretmez (A4-K72). */
        kc_in_izgara(&g_in, kc_sonraki_t());
        g_in.plat.link_event = (uint8_t)IDA_LINKEV_SESSION_START;
        TEST_ASSERT_EQUAL_INT(IDA_RC_OK, kc_tick(&g_in));
        TEST_ASSERT_EQUAL_UINT8(0u, kc_out()->n_events);
        TEST_ASSERT_EQUAL_HEX32(0u, kc_out()->fc.inhibit_mask & FCI_EMNIYET);
    }
}

typedef enum {
    K_ESTOP_INST1,
    K_PIN_T0,
    K_IKI_PIN,
    K_ESTOP_VE_FIS,
    K_VALID0,
    K_ESTOP2,
    K_CELISKI,
    K_SAYISI
} kapanma_t;

static void test_A4_K111_kapanma_turleri(void)
{
    for (int k = 0; k < (int)K_SAYISI; ++k) {
        kc_ortam_kur(KC_N_AZAMI);
        kc_hazirla(3);
        bool g = gevset();
        GEVSETME_YOKSA_ATLA(g)
        {
            kc_in_step(&g_in, kc_sonraki_seq(), kc_sonraki_t());
            uint8_t acik = KC_TUP_ACIK;
            bool reddedilen = false;
            if (k == K_ESTOP_INST1) {
                (void)emniyet(&g_in, 1u, 1u, acik, acik);
            } else if (k == K_PIN_T0) {
                (void)emniyet(&g_in, 0u, 0u, (uint8_t)(acik & ~IDA_TUBE_PIN_OUT), acik);
            } else if (k == K_IKI_PIN) {
                (void)emniyet(&g_in, 0u, 0u, (uint8_t)(acik & ~IDA_TUBE_PIN_OUT), (uint8_t)(acik & ~IDA_TUBE_PIN_OUT));
            } else if (k == K_ESTOP_VE_FIS) {
                (void)emniyet(&g_in, 0u, 1u, (uint8_t)(acik & ~IDA_TUBE_PLUG_IN), acik);
            } else if (k == K_VALID0) {
                emniyet(&g_in, 0u, 1u, acik, acik)->hdr.flags = 0u;
                reddedilen = true;
            } else if (k == K_ESTOP2) {
                (void)emniyet(&g_in, 0u, 2u, acik, acik);
                reddedilen = true;
            } else {
                ida_sample_t *a = emniyet(&g_in, 0u, 0u, acik, acik);
                ida_sample_t *b = kc_emniyet_ekle(&g_in, 0u, a->hdr.seq, a->hdr.t_meas.ns, 1u, acik, acik);
                (void)b;
                reddedilen = true;
            }
            (void)kc_komut_ekle(&g_in, 40000u, (uint8_t)IDA_CMD_ARM);
            TEST_ASSERT_EQUAL_INT(IDA_RC_OK, kc_step(&g_in));
            const ida_out_t *o = kc_out();
            TEST_ASSERT_TRUE(o->health.narrowed);
            TEST_ASSERT_EQUAL_UINT16_MESSAGE(IDA_REASON_NARROWED, o->results[0].reason, "A4-K111 kapanması daraltıcıdır");
            TEST_ASSERT_EQUAL_INT(1, kc_gunluk_say(o, (uint16_t)IDA_LOG_SAFETY_INPUT));
            TEST_ASSERT_EQUAL_UINT32(reddedilen ? 1u : 0u, KC_FARK(narrow_from_rejected));
            if (k == K_PIN_T0) {
                TEST_ASSERT_EQUAL_UINT8(1u, o->n_events);
                TEST_ASSERT_EQUAL_INT(1, kc_kapanma_sayisi(o, 0u));
                TEST_ASSERT_EQUAL_HEX32(IDA_FCI_PIN_T0, o->fc.inhibit_mask & FCI_EMNIYET);
            } else if (k == K_IKI_PIN) {
                TEST_ASSERT_EQUAL_UINT8(2u, o->n_events);
                TEST_ASSERT_EQUAL_INT(1, kc_kapanma_sayisi(o, 0u));
                TEST_ASSERT_EQUAL_INT(1, kc_kapanma_sayisi(o, 1u));
            } else {
                TEST_ASSERT_EQUAL_UINT8(1u, o->n_events);
                TEST_ASSERT_EQUAL_INT(1, kc_kapanma_sayisi(o, (uint8_t)IDA_TUBE_ALL));
                TEST_ASSERT_TRUE((o->fc.inhibit_mask & IDA_FCI_ESTOP) != 0u);
            }
            /* Aynı kısıtlayıcı içeriğin yinelenmesi yeni kapanma üretmez (A4-K44, K111). */
            kc_in_step(&g_in2, kc_sonraki_seq(), kc_sonraki_t());
            for (unsigned i = 0; i < g_in.n_samples; ++i) {
                g_in2.samples[i] = g_in.samples[i];
                g_in2.samples[i].hdr.seq += 1000u;
                g_in2.samples[i].hdr.t_meas = g_in2.tf.t;
                g_in2.samples[i].hdr.t_rx = g_in2.now_w;
            }
            g_in2.n_samples = g_in.n_samples;
            TEST_ASSERT_EQUAL_INT(IDA_RC_OK, kc_step(&g_in2));
            TEST_ASSERT_EQUAL_UINT8(0u, kc_out()->n_events);
            TEST_ASSERT_FALSE(kc_out()->health.narrowed);
        }
    }
}

static void test_A4_K116_reddedilen_cagride_kapanma(void)
{
    for (int r = 0; r < RET_SAYISI; ++r) {
        kc_ortam_kur(KC_N_AZAMI);
        kc_hazirla(3);
        bool g = gevset();
        GEVSETME_YOKSA_ATLA(g)
        {
            uint32_t seq0 = kc_son_seq();
            int64_t t0 = kc_son_t();
            kc_in_step(&g_in, kc_sonraki_seq(), kc_sonraki_t());
            (void)emniyet(&g_in, 0u, 1u, KC_TUP_ACIK, KC_TUP_ACIK);
            ret_hazirla(&g_in, r);
            (void)ret_cagir(KC_STEP, &g_in, r);
            if (r == RET_OUT_NULL) {
                TEST_ASSERT_EQUAL_INT(IDA_RC_OK, kc_adim());
                kapanma_tum(kc_out(), kc_son_seq(), kc_son_t(), "bekletilen A4-K111 kapanması");
                TEST_ASSERT_EQUAL_INT(IDA_RC_OK, kc_adim());
                TEST_ASSERT_EQUAL_UINT8(0u, kc_out()->n_events);
            } else {
                kapanma_tum(kc_out(), seq0, t0, ret_ad(r));
                TEST_ASSERT_TRUE(kc_out()->health.narrowed);
                TEST_ASSERT_EQUAL_UINT32(1u, KC_FARK(narrow_from_rejected));
                /* Reddedilen çağrının gevşetici içeriği geri açmaz. */
                kc_in_step(&g_in, kc_son_seq(), kc_sonraki_t());
                (void)emniyet(&g_in, 0u, 0u, KC_TUP_ACIK, KC_TUP_ACIK);
                TEST_ASSERT_EQUAL_INT(IDA_RC_DUP_SEQ, kc_step(&g_in));
                emniyet_kisitli(kc_out(), "reddedilen çağrı gevşetmemeli");
            }
        }
    }
}

static void test_A4_K116_session_end_donusu_tek_kapanmada(void)
{
    kc_hazirla(3);
    bool g = gevset();
    GEVSETME_YOKSA_ATLA(g)
    {
        kc_in_oturum_sonu(&g_in, kc_sonraki_t());
        TEST_ASSERT_EQUAL_INT(IDA_RC_OK, kc_tick(&g_in));
        TEST_ASSERT_EQUAL_UINT8(1u, kc_out()->n_events);
        TEST_ASSERT_EQUAL_UINT8(IDA_TUBE_ALL, kc_out()->events[0].tube);
        TEST_ASSERT_EQUAL_UINT32(0u, KC_FARK(narrow_from_rejected));
        emniyet_kisitli(kc_out(), "SESSION_END etkin değerleri kısıtlayana döndürür");
    }
}

/* ===================================================================================
 * Genişleticiler kabul edilmez (A4-K124, 3.17, M4); kabul edilen daraltmasız çağrıda.
 * =================================================================================== */

static bool neden_kumede(uint16_t r, const uint16_t *kume, size_t n)
{
    for (size_t i = 0; i < n; ++i) {
        if (kume[i] == r) {
            return true;
        }
    }
    return false;
}

/* UNSUPPORTED/NONE ya da kümedeki bir nedenle REJECTED. */
static void kabul_edilmedi(const ida_cmd_result_t *r, const uint16_t *kume, size_t n, const char *ad)
{
    bool uns = r->ack == IDA_ACK_UNSUPPORTED && r->reason == IDA_REASON_NONE;
    bool red = r->ack == IDA_ACK_REJECTED && neden_kumede(r->reason, kume, n);
    char msj[128];
    (void)snprintf(msj, sizeof msj, "%s: %u/%u (3.17)", ad, (unsigned)r->ack, (unsigned)r->reason);
    TEST_ASSERT_TRUE_MESSAGE(uns || red, msj);
}

static const uint16_t N_ARM[4] = {IDA_REASON_EVENTS_OFF, IDA_REASON_PARAM_INVALID, IDA_REASON_PARAM_BUSY,
                                  IDA_REASON_NOT_ARMABLE};
static const uint16_t N_KIP[1] = {IDA_REASON_BAD_MODE};
static const uint16_t N_FSR[1] = {IDA_REASON_NOT_ARMABLE};
static const uint16_t N_FC_ARM[4] = {IDA_REASON_SESSION, IDA_REASON_EVENTS_OFF, IDA_REASON_BAD_MODE,
                                     IDA_REASON_NOT_ARMABLE};
static const uint16_t N_FC_RESET[2] = {IDA_REASON_SESSION, IDA_REASON_BAD_MODE};
static const uint16_t N_ARALIK[1] = {IDA_REASON_OUT_OF_RANGE};

static void genisleticiler_kabul_edilmez(kc_islev_t f, uint8_t link_state, bool olay_acik)
{
    ida_init_in_t ii;
    kc_init_girdi(&ii);
    ii.boot_count_ok = olay_acik;
    (void)kc_init(&ii);
    for (int tekrar = 0; tekrar < 3; ++tekrar) {
        if (f == KC_STEP) {
            kc_in_step(&g_in, kc_sonraki_seq(), kc_sonraki_t());
        } else {
            kc_in_tick(&g_in, kc_sonraki_seq(), kc_sonraki_t());
        }
        g_in.plat.link_state = link_state;
        uint32_t taban = 50000u + 100u * (uint32_t)tekrar + (f == KC_STEP ? 0u : 5000u);
        (void)genisleticileri_ekle(&g_in, taban, 8u);
        TEST_ASSERT_EQUAL_INT(IDA_RC_OK, kc_cagir(f, &g_in));
        const ida_out_t *o = kc_out();
        TEST_ASSERT_EQUAL_UINT8(8u, o->n_results);
        kabul_edilmedi(&o->results[0], N_ARM, 4, "ARM");
        kabul_edilmedi(&o->results[1], N_KIP, 1, "SET_MODE(GUIDED)");
        kabul_edilmedi(&o->results[2], N_KIP, 1, "SET_MODE(AUTO)");
        kabul_edilmedi(&o->results[3], N_FSR, 1, "FAILSAFE_RESET");
        kabul_edilmedi(&o->results[4], N_FC_ARM, 4, "FC_ARM");
        kabul_edilmedi(&o->results[5], N_FC_ARM, 4, "FC_FIRE");
        kabul_edilmedi(&o->results[6], N_FC_RESET, 2, "FC_RESET");
        kabul_edilmedi(&o->results[7], N_FC_ARM, 4, "FC_TEST_ENTER");
        if (!olay_acik && o->results[0].ack == IDA_ACK_REJECTED) {
            TEST_ASSERT_EQUAL_UINT16_MESSAGE(IDA_REASON_EVENTS_OFF, o->results[0].reason, "ARM: EVENTS_OFF ilk koşul");
        }
        /* FC işlemlerinde ilk koşul oturum (engel biti SESSION), sonra olay kimliği. */
        if ((o->fc.inhibit_mask & IDA_FCI_SESSION) != 0u && (kc_onceki_out()->fc.inhibit_mask & IDA_FCI_SESSION) != 0u &&
            tekrar > 0) {
            for (unsigned k = 4; k < 8u; ++k) {
                if (o->results[k].ack == IDA_ACK_REJECTED) {
                    TEST_ASSERT_EQUAL_UINT16_MESSAGE(IDA_REASON_SESSION, o->results[k].reason, "FC: SESSION ilk koşul");
                }
            }
        } else if ((o->fc.inhibit_mask & IDA_FCI_EVENTS_OFF) != 0u && tekrar > 0) {
            for (unsigned k = 4; k < 8u; ++k) {
                if (k != 6u && o->results[k].ack == IDA_ACK_REJECTED) {
                    TEST_ASSERT_EQUAL_UINT16_MESSAGE(IDA_REASON_EVENTS_OFF, o->results[k].reason, "FC: EVENTS_OFF");
                }
            }
        }
        /* Etkisizlik. */
        TEST_ASSERT_FALSE(o->armed);
        TEST_ASSERT_TRUE(o->status.mode != IDA_MODE_GUIDED && o->status.mode != IDA_MODE_AUTO);
        TEST_ASSERT_TRUE_MESSAGE(o->fc.state == IDA_FCS_SAFE || o->fc.state == IDA_FCS_FAULT_LATCHED,
                                 "FC_ARM/FC_TEST_ENTER kabul edilmemişken fc.state değişti");
        TEST_ASSERT_EQUAL_UINT8(0u, o->n_events);
    }
}

static void test_A4_K124_genisleticiler_step_oturum_var(void)
{
    genisleticiler_kabul_edilmez(KC_STEP, (uint8_t)IDA_LINK_RUNNING, true);
}

static void test_A4_K124_genisleticiler_tick_oturum_var(void)
{
    genisleticiler_kabul_edilmez(KC_TICK, (uint8_t)IDA_LINK_RUNNING, true);
}

static void test_A4_K124_genisleticiler_oturum_yok(void)
{
    genisleticiler_kabul_edilmez(KC_STEP, (uint8_t)IDA_LINK_NONE, true);
}

static void test_A4_K124_genisleticiler_olay_kimligi_yok(void)
{
    genisleticiler_kabul_edilmez(KC_STEP, (uint8_t)IDA_LINK_RUNNING, false);
}

/* A4-K119: kapalı liste dışı FC işlemi, tüp ya da ayrılmış argüman: OUT_OF_RANGE
   (yer tutucuda UNSUPPORTED olabilir); FC_TEST_EXIT nötrdür. */
static void test_A4_K119_fc_gecersiz_alanlar(void)
{
    kc_hazirla(3);
    kc_in_step(&g_in, kc_sonraki_seq(), kc_sonraki_t());
    (void)kc_fc_ekle(&g_in, 60000u, 4u, 0u);
    (void)kc_fc_ekle(&g_in, 60001u, 9u, 0u);
    (void)kc_fc_ekle(&g_in, 60002u, (uint8_t)IDA_FC_ARM, 2u);
    kc_fc_ekle(&g_in, 60003u, (uint8_t)IDA_FC_FIRE, 0u)->u.fc.arg0 = 1u;
    kc_fc_ekle(&g_in, 60004u, (uint8_t)IDA_FC_TEST_ENTER, 1u)->u.fc.arg1 = 0x80000000u;
    (void)kc_fc_ekle(&g_in, 60005u, (uint8_t)IDA_FC_TEST_EXIT, 0u);
    TEST_ASSERT_EQUAL_INT(IDA_RC_OK, kc_step(&g_in));
    const ida_out_t *o = kc_out();
    for (unsigned k = 0; k < 5u; ++k) {
        kabul_edilmedi(&o->results[k], N_ARALIK, 1, "geçersiz FC");
    }
    TEST_ASSERT_TRUE(o->results[5].ack == IDA_ACK_ACCEPTED || o->results[5].ack == IDA_ACK_UNSUPPORTED);
}

static void test_A4_3_17_bilinmeyen_tur_UNSUPPORTED(void)
{
    static const uint8_t turler[4] = {0u, 17u, 128u, 255u};
    kc_hazirla(3);
    kc_in_step(&g_in, kc_sonraki_seq(), kc_sonraki_t());
    for (unsigned k = 0; k < 4u; ++k) {
        ida_cmd_t *c = kc_komut_ekle(&g_in, 61000u + k, turler[k]);
        memset(&c->u, 0xFF, sizeof c->u); /* gövde okunmaz (A4-K121, 3.14) */
    }
    TEST_ASSERT_EQUAL_INT(IDA_RC_OK, kc_step(&g_in));
    for (unsigned k = 0; k < 4u; ++k) {
        TEST_ASSERT_EQUAL_UINT8(IDA_ACK_UNSUPPORTED, kc_out()->results[k].ack);
        TEST_ASSERT_EQUAL_UINT16(IDA_REASON_NONE, kc_out()->results[k].reason);
    }
}

/* 3.17 (2): IMU_LAG mandalı kuruluyken genişletici IMU_LAG ile reddedilir (yer tutucunun
   UNSUPPORTED'ı (3)'tür, (2)'den sonra gelir); A4-K37, K108. */
static void test_A4_K108_IMU_LAG_genisleticiyi_reddeder(void)
{
    kc_init_bloksuz();
    for (int k = 0; k < 3; ++k) {
        kc_in_step(&g_in, (uint32_t)k + 1u, KC_T0 + 100 * KC_MS + (int64_t)k * KC_P);
        g_in.imu.hdr.t_meas.ns = g_in.tf.t.ns - 25 * KC_MS;
        g_in.imu.hdr.t_rx.ns = g_in.now_w.ns;
        if (k == 2) {
            (void)kc_komut_ekle(&g_in, 62000u, (uint8_t)IDA_CMD_ARM);
            (void)kc_fc_ekle(&g_in, 62001u, (uint8_t)IDA_FC_ARM, 0u);
        }
        TEST_ASSERT_EQUAL_INT(IDA_RC_OK, kc_step(&g_in));
    }
    const ida_out_t *o = kc_out();
    TEST_ASSERT_TRUE_MESSAGE(o->health.imu_lag, "lag 25 ms > T_lag_set 20 ms: IMU_LAG mandalı (A4-K37)");
    TEST_ASSERT_TRUE((o->status.fs_reasons & IDA_FSR_IMU_LAG) != 0u);
    TEST_ASSERT_EQUAL_UINT8(IDA_ACK_REJECTED, o->results[0].ack);
    TEST_ASSERT_EQUAL_UINT16(IDA_REASON_IMU_LAG, o->results[0].reason);
    TEST_ASSERT_EQUAL_UINT16(IDA_REASON_IMU_LAG, o->results[1].reason);
    TEST_ASSERT_EQUAL_UINT32(2u, KC_FARK(widen_blocked));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_A4_K113_b_mandal_her_ret_turunde);
    RUN_TEST(test_A4_K114_n_cmds_9_ilk_sekiz_uygulanir);
    RUN_TEST(test_A4_K48_K114_yineleme_penceresi);
    RUN_TEST(test_A4_K45_her_daraltici_her_genisletici);
    RUN_TEST(test_A4_K45_IMU_REJECTED_cagrisinda_once_NARROWED);
    RUN_TEST(test_A4_K48_daraltmali_cagrida_yinelenen_genisletici_DUPLICATE);
    RUN_TEST(test_A4_K73_session_end_her_cagri_turunde);
    RUN_TEST(test_A4_K125_bekletilen_ve_yeni_kapanma_birlesir);
    RUN_TEST(test_A4_K125_bekletilen_kapanma_her_donus_kodunda);
    RUN_TEST(test_A4_K61_olay_kimligi_yoksa_kapanma_da_yok);
    RUN_TEST(test_A4_K129_kapanma_kume_doluyken_de_uretilir);
    RUN_TEST(test_A4_K116_init_kisitlayici_ve_bos);
    RUN_TEST(test_A4_K44_yinelenen_kisitlayici_icerik_daraltici_degil);
    RUN_TEST(test_A4_K116_reddedilen_girdinin_gevseten_icerigi_uygulanmaz);
    RUN_TEST(test_A4_K116_K130_gevsetemeyen_ornekler);
    RUN_TEST(test_A4_K115_celiskili_cift_siradan_bagimsiz);
    RUN_TEST(test_A4_K116_gevsetme_ve_SAFETY_INPUT);
    RUN_TEST(test_A4_K111_kapanma_turleri);
    RUN_TEST(test_A4_K116_reddedilen_cagride_kapanma);
    RUN_TEST(test_A4_K116_session_end_donusu_tek_kapanmada);
    RUN_TEST(test_A4_K124_genisleticiler_step_oturum_var);
    RUN_TEST(test_A4_K124_genisleticiler_tick_oturum_var);
    RUN_TEST(test_A4_K124_genisleticiler_oturum_yok);
    RUN_TEST(test_A4_K124_genisleticiler_olay_kimligi_yok);
    RUN_TEST(test_A4_K119_fc_gecersiz_alanlar);
    RUN_TEST(test_A4_3_17_bilinmeyen_tur_UNSUPPORTED);
    RUN_TEST(test_A4_K108_IMU_LAG_genisleticiyi_reddeder);
    return UNITY_END();
}
