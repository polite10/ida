/*
 * test_atis_ozellik.c — atış kontrol emniyet zincirinin yedi emniyet değişmezi (görev belgesi
 * kabul md. 2), rastgele komut / olay sonucu / geri okuma dizileri üzerinde, sabit tohumla.
 *
 * Düzenek yolu (atis_duzenek.h): atış kontrol silahlanabilir; basit bir alıcı (sim) taklidi
 * olaylara çoğunlukla APPLIED, bazen ret, bazen hiç yanıt vermez; geri okuma çoğunlukla bu
 * taklidin durumunu, bazen bozuk ya da donmuş içeriği taşır; koşullar (kestirim, arıza,
 * IMU, kalp atışı, kip, araç silahlandırması), daraltıcılar, oturum sonu/başı, reddedilen ve
 * out NULL çağrılar karışır. Değişmezler (a)–(f) her çağrıda denetlenir. (g) port yolunda
 * (parametre commit'i çerçevenin çağrı sonundadır).
 *
 *   (a) acil durdurma basılı ya da bayatken genişletici olay yok
 *   (b) FIRE yalnız ARMED'de kabul edilmiş FC_FIRE'dan, o tüpün izni APPLIED olduktan sonra,
 *       izin penceresi içinde
 *   (c) FAULT_LATCHED yalnız kabul edilen FC_RESET ile terk edilir
 *   (d) TEST'te genişletici olay yok
 *   (e) daraltıcı komut hiçbir durumda ve biçimde reddedilmez
 *   (f) sonucu bilinmeyen genişletici olay uygulanmış sayılmaz (PERMIT_OPEN yalnız APPLIED
 *       izinle)
 *   (g) silahlıyken emniyet parametresi değişmez
 */
#include <stdio.h>
#include <string.h>

#include "atis_duzenek.h"
#include "atis/atis_ic.h"
#include "destek.h"
#include "unity.h"

void setUp(void);
void tearDown(void);
void setUp(void) {}
void tearDown(void) {}

#define OZ_TOHUM 0x5EED2026u
#define OZ_ADIM 200000u
#define HOLD_NS 200000000LL
#define PENCERE_NS 300000000LL

static ida_core_t C;
static ida_in_t IN;
static ida_out_t OUT;
static ds_rastgele_t R;

static uint32_t zar(uint32_t n)
{
    return ds_rastgele(&R) % n;
}

static bool olasi(uint32_t binde)
{
    return zar(1000u) < binde;
}

/* ---- Alıcı taklidi ------------------------------------------------------------------------ */

typedef struct {
    bool dolu;
    uint64_t id;
    uint8_t kind;
    uint8_t tube;
    int64_t cikis;     /* olayı çıkaran çağrının now'u */
    int64_t teslim;    /* sonucun verileceği çağrı zamanı; < 0: hiç verilmez */
    uint8_t sonuc;
} oz_olay_t;

#define OZ_N 32u
static oz_olay_t BEK[OZ_N];
static int64_t SIM_IZIN[2];   /* alıcıdaki izin bitişi (ölçüm zamanı) */
static bool SIM_DOLU[2];
static uint8_t G_ESTOP;
static bool G_FIS[2];
static bool G_PIM_CIKIK[2];

/* Test tarafı izleri (b), (f). */
static bool ISTEK[2];          /* kabul edilmiş, FIRE üretmemiş FC_FIRE */
static int64_t ISTEK_T[2];
static bool UYG_ACIK[2];       /* istekten sonra çıkmış, APPLIED verilmiş SAFETY_OPEN */
static int64_t UYG_ACIK_CIKIS[2];
static uint64_t SON_ACIK_ID[2];

static void bekleyene_ekle(const ida_event_t *e, int64_t now)
{
    for (uint32_t i = 0u; i < OZ_N; i++) {
        if (!BEK[i].dolu) {
            oz_olay_t *b = &BEK[i];
            b->dolu = true;
            b->id = e->event_id;
            b->kind = e->kind;
            b->tube = e->tube;
            b->cikis = now;
            b->teslim = now + ((int64_t)(1u + zar(4u)) * 5 * DZ_MS);
            const uint32_t z = zar(100u);
            if (e->kind == (uint8_t)IDA_EVT_SAFETY_CLOSE) {
                b->sonuc = (z < 95u) ? (uint8_t)IDA_EVR_APPLIED : (uint8_t)IDA_EVR_EXPIRED_NO_ACK;
                if (z >= 98u) {
                    b->teslim = -1; /* yanıtsız */
                }
            } else {
                static const uint8_t kodlar[] = {
                    IDA_EVR_DUPLICATE,    IDA_EVR_REJ_EXPIRED,     IDA_EVR_REJ_SAFE,
                    IDA_EVR_REJ_TUBE_EMPTY, IDA_EVR_REJ_SESSION,   IDA_EVR_DROPPED_GATE,
                    IDA_EVR_DROPPED_NO_SLOT, IDA_EVR_EXPIRED_NO_ACK, IDA_EVR_SESSION_END};
                b->sonuc = (z < 85u) ? (uint8_t)IDA_EVR_APPLIED : kodlar[zar(9u)];
                if (z >= 96u) {
                    b->teslim = -1;
                }
            }
            return;
        }
    }
}

static void sonuclari_ver(int64_t now)
{
    for (uint32_t i = 0u; i < OZ_N; i++) {
        oz_olay_t *b = &BEK[i];
        if (!b->dolu) {
            continue;
        }
        if (b->teslim < 0) {
            if ((now - b->cikis) > (2000 * DZ_MS)) {
                b->dolu = false;
            }
            continue;
        }
        if ((b->teslim > now) || (IN.n_evres >= IDA_PORT_MAX_EVRES)) {
            continue;
        }
        const int64_t t_apply = now - DZ_MS;
        dz_sonuc(&IN, b->id, b->sonuc, ((b->sonuc == (uint8_t)IDA_EVR_APPLIED) ||
                                        (b->sonuc == (uint8_t)IDA_EVR_DUPLICATE))
                                           ? t_apply
                                           : 0);
        if (b->tube < 2u) {
            if (b->kind == (uint8_t)IDA_EVT_SAFETY_OPEN) {
                if (b->sonuc == (uint8_t)IDA_EVR_APPLIED) {
                    SIM_IZIN[b->tube] = t_apply + HOLD_NS;
                    if (ISTEK[b->tube] && (b->cikis >= ISTEK_T[b->tube])) {
                        UYG_ACIK[b->tube] = true;
                        UYG_ACIK_CIKIS[b->tube] = b->cikis;
                    }
                } else if (b->id == SON_ACIK_ID[b->tube]) {
                    UYG_ACIK[b->tube] = false;
                }
            } else if ((b->kind == (uint8_t)IDA_EVT_FIRE) &&
                       ((b->sonuc == (uint8_t)IDA_EVR_APPLIED) ||
                        (b->sonuc == (uint8_t)IDA_EVR_DUPLICATE))) {
                SIM_DOLU[b->tube] = false;
            } else {
                /* SAFETY_CLOSE(tüp) */
            }
        }
        if ((b->kind == (uint8_t)IDA_EVT_SAFETY_CLOSE) && (b->sonuc == (uint8_t)IDA_EVR_APPLIED)) {
            for (uint8_t t = 0u; t < 2u; t++) {
                if ((b->tube == t) || (b->tube == (uint8_t)IDA_TUBE_ALL)) {
                    SIM_IZIN[t] = 0;
                }
            }
        }
        b->dolu = false;
    }
}

static uint8_t tup_bitleri(uint8_t t, int64_t now)
{
    uint8_t b = 0u;
    if (SIM_DOLU[t]) {
        b = (uint8_t)(b | IDA_TUBE_LOADED);
    }
    if (G_FIS[t]) {
        b = (uint8_t)(b | IDA_TUBE_PLUG_IN);
    }
    if (G_PIM_CIKIK[t]) {
        b = (uint8_t)(b | IDA_TUBE_PIN_OUT);
    }
    if (now < SIM_IZIN[t]) {
        b = (uint8_t)(b | IDA_TUBE_PERMIT_OPEN);
    }
    return b;
}

static void oz_kur(void)
{
    TEST_ASSERT_EQUAL(IDA_RC_OK, ds_kur(&C));
    (void)memset(BEK, 0, sizeof(BEK));
    for (uint8_t t = 0u; t < 2u; t++) {
        SIM_IZIN[t] = 0;
        SIM_DOLU[t] = true;
        G_FIS[t] = true;
        G_PIM_CIKIK[t] = true;
        ISTEK[t] = false;
        UYG_ACIK[t] = false;
        SON_ACIK_ID[t] = 0u;
    }
    G_ESTOP = 0u;
    C.emn.armed = true;
    C.emn.mode = (uint8_t)IDA_MODE_GUIDED;
}

static bool daraltici_mi(const ida_cmd_t *k)
{
    return (k->type == (uint8_t)IDA_CMD_FC) &&
           ((k->u.fc.op == (uint8_t)IDA_FC_DISARM) || (k->u.fc.op == (uint8_t)IDA_FC_ABORT));
}

static uint32_t genis_say(void)
{
    uint32_t n = 0u;
    for (uint32_t i = 0u; i < (uint32_t)OUT.n_events; i++) {
        if (OUT.events[i].kind != (uint8_t)IDA_EVT_SAFETY_CLOSE) {
            n++;
        }
    }
    return n;
}

/* Sayaçlar: değişmezlerin gerçekten sınandığını göstermek için. */
static uint32_t S_FIRE;
static uint32_t S_ACIK;
static uint32_t S_FAULT_CIKIS;
static uint32_t S_TEST;
static uint32_t S_DARALT;
static uint32_t S_ESTOP;
static uint32_t S_PERMIT;

static void test_ozellik_degismezler_a_f(void)
{
    R.s = OZ_TOHUM;
    oz_kur();
    int64_t T = 2000 * DZ_MS;
    uint32_t seq = 0u;
    uint32_t eseq = 0u;
    uint32_t cid = 1u;
    int64_t gcs = T;
    int64_t tmeas_donuk = -1;
    bool oturum_basi = false;
    dz_kosul_t K;
    dz_kosul_olagan(&K, T);
    S_FIRE = S_ACIK = S_FAULT_CIKIS = S_TEST = S_DARALT = S_ESTOP = S_PERMIT = 0u;

    for (uint32_t adim = 0u; adim < OZ_ADIM; adim++) {
        seq++;
        T += olasi(2u) ? (int64_t)(5u + zar(400u)) * DZ_MS : 5 * DZ_MS;
        const bool step = !olasi(100u);
        dz_girdi(&IN, T, seq, step);
        /* koşullar */
        const uint32_t e = zar(1000u);
        K.est = (e < 970u) ? (uint8_t)IDA_EST_OK
                           : ((e < 990u) ? (uint8_t)IDA_EST_DEGRADED : (uint8_t)IDA_EST_BAD);
        K.fs = olasi(10u) ? (uint8_t)(1u + zar(5u)) : (uint8_t)IDA_FS_NONE;
        K.imu_bayat = olasi(10u);
        K.imu_lag = olasi(10u);
        K.imu_red = step && olasi(10u);
        if (!olasi(5u)) {
            gcs = T;
        }
        K.gcs_son_ns = gcs;
        if (olasi(20u)) {
            C.emn.armed = true; /* ARM komutunun yerine (düzenek) */
        }
        if (olasi(20u) && (C.emn.mode != (uint8_t)IDA_MODE_INIT)) {
            C.emn.mode = (uint8_t)IDA_MODE_GUIDED;
        }
        if (oturum_basi) {
            IN.plat.link_event = (uint8_t)IDA_LINKEV_SESSION_START;
            oturum_basi = false;
        }
        /* sanal girdiler */
        /* Girdiler çoğunlukla gevşek durumdadır (zincirin silahlı yolları sık görülsün);
           kısıtlayan geçişler seyrek, gevşetmeler daha sık. */
        if (olasi((G_ESTOP != 0u) ? 50u : 2u)) {
            G_ESTOP = (uint8_t)(1u - G_ESTOP);
        }
        for (uint8_t t = 0u; t < 2u; t++) {
            if (olasi(G_FIS[t] ? 2u : 50u)) {
                G_FIS[t] = !G_FIS[t];
            }
            if (olasi(G_PIM_CIKIK[t] ? 2u : 50u)) {
                G_PIM_CIKIK[t] = !G_PIM_CIKIK[t];
            }
            if (!SIM_DOLU[t] && olasi(2u)) {
                SIM_DOLU[t] = true; /* yeniden doldurma */
            }
        }
        if (G_ESTOP != 0u) {
            SIM_IZIN[0] = 0;
            SIM_IZIN[1] = 0;
        }
        sonuclari_ver(T);
        bool estop_icerik = false;
        if (olasi(400u)) {
            eseq++;
            if (olasi(10u)) {
                tmeas_donuk = (tmeas_donuk < 0) ? T : tmeas_donuk;
            } else if (olasi(30u)) {
                tmeas_donuk = -1;
            } else {
                /* sürer */
            }
            const int64_t tm = (tmeas_donuk >= 0) ? tmeas_donuk : T;
            ida_sample_t *s = dz_emniyet(&IN, 0u, eseq, tm, T, G_ESTOP, tup_bitleri(0u, T),
                                         tup_bitleri(1u, T));
            estop_icerik = G_ESTOP != 0u;
            if (olasi(20u)) {
                s->u.safety.tube[zar(2u)] = (uint8_t)zar(16u); /* bozuk geri okuma */
            }
            if (olasi(10u)) {
                s->hdr.flags = 0u; /* VALID = 0: yalnız kısıtlar */
            }
        }
        if (olasi(15u)) {
            eseq++;
            ida_sample_t *s = dz_emniyet(&IN, (uint8_t)(1u + zar(3u)), eseq, T, T,
                                         (uint8_t)zar(2u), (uint8_t)zar(16u), (uint8_t)zar(16u));
            estop_icerik = estop_icerik || (s->u.safety.estop != 0u);
        }
        /* komutlar */
        static const uint8_t ops[] = {IDA_FC_ARM, IDA_FC_FIRE, IDA_FC_FIRE, IDA_FC_FIRE,
                                      IDA_FC_RESET, IDA_FC_TEST_ENTER, IDA_FC_TEST_EXIT,
                                      IDA_FC_DISARM, IDA_FC_ABORT, 4u};
        const uint32_t nk = olasi(300u) ? (1u + zar(3u)) : 0u;
        for (uint32_t j = 0u; j < nk; j++) {
            const uint8_t op = ops[zar((uint32_t)sizeof(ops))];
            const uint8_t tube = (C.fc.tup < 2u) && olasi(800u) ? C.fc.tup : (uint8_t)zar(2u);
            dz_fc(&IN, olasi(20u) ? (cid - 1u) : cid, op, tube);
            cid++;
            if ((op == (uint8_t)IDA_FC_DISARM) || (op == (uint8_t)IDA_FC_ABORT)) {
                IN.cmds[IN.n_cmds - 1u].u.fc.tube = (uint8_t)zar(256u); /* biçim bozuk olabilir */
                IN.cmds[IN.n_cmds - 1u].u.fc.arg0 = zar(3u);
            }
        }
        if (olasi(10u)) {
            IN.narrow.mask = (uint8_t)(IN.narrow.mask | IDA_NARROW_FC_DISARM);
            IN.narrow.cmd_id[2] = cid++;
        }
        if (olasi(10u)) {
            IN.narrow.mask = (uint8_t)(IN.narrow.mask | IDA_NARROW_FC_ABORT);
            IN.narrow.cmd_id[3] = cid++;
        }
        if (olasi(3u)) {
            IN.narrow.mask = (uint8_t)(IN.narrow.mask | IDA_NARROW_DISARM);
            IN.narrow.cmd_id[0] = cid++;
        }
        if (olasi(3u)) {
            IN.narrow.mask = (uint8_t)(IN.narrow.mask | IDA_NARROW_MODE);
            IN.narrow.mode = (uint8_t)IDA_MODE_MANUAL;
            IN.narrow.cmd_id[1] = cid++;
        }
        if (olasi(1u)) {
            IN.plat.link_event = (uint8_t)IDA_LINKEV_SESSION_END;
            IN.plat.link_state = (uint8_t)IDA_LINK_NONE;
            oturum_basi = true;
            for (uint8_t t = 0u; t < 2u; t++) {
                SIM_IZIN[t] = 0;
            }
            for (uint32_t i = 0u; i < OZ_N; i++) {
                if (BEK[i].dolu) {
                    BEK[i].sonuc = (uint8_t)IDA_EVR_SESSION_END; /* A4-K71 */
                    if (BEK[i].teslim < 0) {
                        BEK[i].teslim = T + (5 * DZ_MS);
                    }
                }
            }
        }
        int ret = DZ_KABUL;
        if (olasi(10u)) {
            ret = DZ_RET_DUP_SEQ;
        }
        const bool out_null = olasi(5u);

        const uint32_t onceki_ic = C.fc.ic;
        const uint8_t onceki_tup = C.fc.tup;
        const ida_rc_t rc = dz_cagri(&C, &IN, out_null ? NULL : &OUT, step, ret, &K);
        if (out_null) {
            /* (c): out NULL çağrıda kilit terk edilmez */
            if (onceki_ic == IDA_FC_IC_FAULT) {
                TEST_ASSERT_EQUAL_HEX32(IDA_FC_IC_FAULT, C.fc.ic);
            }
            continue;
        }
        const uint32_t genis = genis_say();
        /* G-Ç02, G-Ç30: kapalı tablolar. */
        TEST_ASSERT_TRUE(OUT.fc.state <= (uint8_t)IDA_FCS_FAULT_LATCHED);
        TEST_ASSERT_EQUAL_HEX32(0u, OUT.fc.inhibit_mask & ~(uint32_t)IDA_FCI_DEFINED);
        for (uint32_t i = 0u; i < (uint32_t)OUT.n_results; i++) {
            TEST_ASSERT_TRUE(OUT.results[i].reason <= IDA_REASON_CORE_LAST);
        }
        for (uint32_t i = 0u; i < (uint32_t)OUT.n_logs; i++) {
            TEST_ASSERT_TRUE((OUT.logs[i].code >= 1u) && (OUT.logs[i].code <= IDA_LOG_CODE_LAST));
        }
        /* G-Ç07: genişletici yalnız OK dönen step'te; kapanmalar önce, en çok iki. */
        if (genis > 0u) {
            TEST_ASSERT_TRUE(step && (rc == IDA_RC_OK));
        }
        uint32_t kapanma = 0u;
        bool kapanma_bitti = false;
        for (uint32_t i = 0u; i < (uint32_t)OUT.n_events; i++) {
            if (OUT.events[i].kind == (uint8_t)IDA_EVT_SAFETY_CLOSE) {
                TEST_ASSERT_FALSE(kapanma_bitti);
                kapanma++;
            } else {
                kapanma_bitti = true;
            }
        }
        TEST_ASSERT_TRUE(kapanma <= 2u);
        /* (a) */
        if (((OUT.fc.inhibit_mask & IDA_FCI_ESTOP) != 0u) || estop_icerik) {
            TEST_ASSERT_EQUAL_UINT32(0u, genis);
            S_ESTOP++;
        }
        /* (d) */
        if ((onceki_ic == IDA_FC_IC_TEST) || (C.fc.ic == IDA_FC_IC_TEST)) {
            TEST_ASSERT_EQUAL_UINT32(0u, genis);
            S_TEST++;
        }
        /* (e) */
        const uint32_t n_mandal_bit = (uint32_t)((IN.narrow.mask & 1u) + ((IN.narrow.mask >> 1) & 1u) +
                                                 ((IN.narrow.mask >> 2) & 1u) + ((IN.narrow.mask >> 3) & 1u));
        for (uint32_t i = 0u; i < (uint32_t)OUT.n_results; i++) {
            const ida_cmd_result_t *r = &OUT.results[i];
            bool daraltici = i < n_mandal_bit;
            if (!daraltici) {
                const ida_cmd_t *k = &IN.cmds[i - n_mandal_bit];
                daraltici = daraltici_mi(k);
            }
            if (daraltici) {
                TEST_ASSERT_EQUAL_UINT8(IDA_ACK_ACCEPTED, r->ack);
                S_DARALT++;
            }
        }
        /* (c) */
        if ((onceki_ic == IDA_FC_IC_FAULT) && (C.fc.ic != IDA_FC_IC_FAULT)) {
            bool reset = false;
            for (uint32_t i = n_mandal_bit; i < (uint32_t)OUT.n_results; i++) {
                const ida_cmd_t *k = &IN.cmds[i - n_mandal_bit];
                if ((k->type == (uint8_t)IDA_CMD_FC) && (k->u.fc.op == (uint8_t)IDA_FC_RESET) &&
                    (OUT.results[i].ack == (uint8_t)IDA_ACK_ACCEPTED)) {
                    reset = true;
                }
            }
            TEST_ASSERT_TRUE(reset);
            TEST_ASSERT_FALSE(C.fc.butunluk_kilidi || C.fc.t[0].celiski || C.fc.t[1].celiski ||
                              C.fc.t[0].kapanis_dogrulanmadi || C.fc.t[1].kapanis_dogrulanmadi);
            S_FAULT_CIKIS++;
        }
        /* Kabul edilen FC_FIRE: yalnız ARMED iç durumunda (aynı çağrıda önce FC_ARM olabilir). */
        bool arm_bu_cagri = false;
        for (uint32_t i = n_mandal_bit; i < (uint32_t)OUT.n_results; i++) {
            const ida_cmd_t *k = &IN.cmds[i - n_mandal_bit];
            if ((k->type != (uint8_t)IDA_CMD_FC) || (OUT.results[i].ack != (uint8_t)IDA_ACK_ACCEPTED)) {
                continue;
            }
            if (k->u.fc.op == (uint8_t)IDA_FC_ARM) {
                arm_bu_cagri = true;
            }
            if (k->u.fc.op == (uint8_t)IDA_FC_FIRE) {
                TEST_ASSERT_TRUE((onceki_ic == IDA_FC_IC_ARMED) || arm_bu_cagri);
                TEST_ASSERT_TRUE((onceki_tup == k->u.fc.tube) || arm_bu_cagri);
                ISTEK[k->u.fc.tube] = true;
                ISTEK_T[k->u.fc.tube] = T;
                UYG_ACIK[k->u.fc.tube] = false;
            }
        }
        /* (b), (f) */
        for (uint32_t i = 0u; i < (uint32_t)OUT.n_events; i++) {
            const ida_event_t *ev = &OUT.events[i];
            if (ev->kind == (uint8_t)IDA_EVT_FIRE) {
                const uint8_t t = ev->tube;
                TEST_ASSERT_TRUE(ISTEK[t]);
                TEST_ASSERT_TRUE((T - ISTEK_T[t]) < PENCERE_NS);
                TEST_ASSERT_TRUE(UYG_ACIK[t]);
                TEST_ASSERT_TRUE(T < (UYG_ACIK_CIKIS[t] + HOLD_NS));
                ISTEK[t] = false;
                UYG_ACIK[t] = false;
                S_FIRE++;
            } else if (ev->kind == (uint8_t)IDA_EVT_SAFETY_OPEN) {
                TEST_ASSERT_TRUE(ISTEK[ev->tube]);           /* G-Ç47 */
                TEST_ASSERT_TRUE((T - ISTEK_T[ev->tube]) < PENCERE_NS);
                SON_ACIK_ID[ev->tube] = ev->event_id;
                S_ACIK++;
            } else {
                /* SAFETY_CLOSE */
            }
            bekleyene_ekle(ev, T);
        }
        if (OUT.fc.state == (uint8_t)IDA_FCS_PERMIT_OPEN) {
            const uint8_t t = C.fc.tup;
            TEST_ASSERT_TRUE(t < 2u);
            TEST_ASSERT_TRUE(UYG_ACIK[t] && (T < (UYG_ACIK_CIKIS[t] + HOLD_NS)));
            S_PERMIT++;
        }
        /* İstek, FC iç durumu pencereden çıkınca düşer. */
        for (uint8_t t = 0u; t < 2u; t++) {
            if (ISTEK[t] && !((C.fc.ic == IDA_FC_IC_FIRE_AUTH) && (C.fc.tup == t))) {
                ISTEK[t] = false;
                UYG_ACIK[t] = false;
            }
        }
        /* Uzun süren kilit: arada sıfırla (FC_RESET'in rastgele gelmesine ek). */
        if ((C.fc.ic == IDA_FC_IC_FAULT) && C.fc.butunluk_kilidi) {
            oz_kur();
        }
    }
    char ileti[256];
    (void)snprintf(ileti, sizeof(ileti),
                   "tohum 0x%08X, %u adım: FIRE %u, SAFETY_OPEN %u, PERMIT_OPEN çağrısı %u, kilitten "
                   "çıkış %u, TEST çağrısı %u, daraltıcı sonucu %u, acil durdurmalı çağrı %u",
                   OZ_TOHUM, OZ_ADIM, S_FIRE, S_ACIK, S_PERMIT, S_FAULT_CIKIS, S_TEST, S_DARALT,
                   S_ESTOP);
    TEST_MESSAGE(ileti);
    /* Değişmezler boş geçmedi. */
    TEST_ASSERT_TRUE(S_FIRE > 10u);
    TEST_ASSERT_TRUE(S_ACIK > 100u);
    TEST_ASSERT_TRUE(S_FAULT_CIKIS > 10u);
    TEST_ASSERT_TRUE(S_TEST > 100u);
    TEST_ASSERT_TRUE(S_DARALT > 100u);
    TEST_ASSERT_TRUE(S_PERMIT > 10u);
}

/* ---- (g): silahlıyken emniyet parametresi değişmez (port yolu) ---------------------------- */

static void test_ozellik_degismez_g(void)
{
    static const char *const ad[] = {"T_emn_bayat", "T_pencere", "T_hb_fc", "FC_APPLY_BY_NS",
                                     "FC_HOLD_NS"};
    static const uint32_t deger[5][2] = {{200u, 400u},
                                         {150u, 500u},
                                         {500u, 2000u},
                                         {16000000u, 50000000u},
                                         {60000000u, 300000000u}};
    uint16_t id[5];
    for (uint32_t i = 0u; i < 5u; i++) {
        TEST_ASSERT_EQUAL(IDA_RC_OK, ida_core_param_find(ad[i], &id[i]));
    }
    R.s = OZ_TOHUM ^ 0x0000A5A5u;
    TEST_ASSERT_EQUAL(IDA_RC_OK, ds_kur(&C));
    int64_t T = 2000 * DS_MS;
    uint32_t seq = 0u;
    uint32_t cid = 1u;
    uint32_t degisim = 0u;
    uint32_t silahli_cagri = 0u;
    for (uint32_t adim = 0u; adim < OZ_ADIM; adim++) {
        seq++;
        T += 5 * DS_MS;
        ds_girdi(&IN, T, seq, true);
        if (olasi(20u)) {
            C.emn.armed = !C.emn.armed; /* test kancası */
        }
        const uint32_t nk = olasi(300u) ? (1u + zar(4u)) : 0u;
        for (uint32_t j = 0u; j < nk; j++) {
            ida_cmd_t *k = &IN.cmds[IN.n_cmds];
            IN.n_cmds++;
            (void)memset(k, 0, sizeof(*k));
            k->cmd_id = cid++;
            const uint32_t s = zar(10u);
            if (s < 6u) {
                const uint32_t p = zar(5u);
                k->type = (uint8_t)IDA_CMD_PARAM_SET;
                k->u.param.id = id[p];
                k->u.param.type = (uint8_t)IDA_PT_U32;
                k->u.param.v.u32 = deger[p][zar(2u)];
            } else if (s < 9u) {
                k->type = (uint8_t)IDA_CMD_PARAM_COMMIT;
            } else {
                k->type = (uint8_t)IDA_CMD_ARM;
            }
        }
        ida_param_value_t once[5];
        for (uint32_t i = 0u; i < 5u; i++) {
            TEST_ASSERT_EQUAL(IDA_RC_OK, ida_core_param_get(&C, id[i], &once[i]));
        }
        const bool silahli = C.emn.armed;
        const ida_rc_t rc = ida_core_step(&C, &IN, &OUT);
        TEST_ASSERT_TRUE((rc == IDA_RC_OK) || (rc == IDA_RC_IMU_REJECTED));
        bool degisti = false;
        for (uint32_t i = 0u; i < 5u; i++) {
            ida_param_value_t sonra;
            TEST_ASSERT_EQUAL(IDA_RC_OK, ida_core_param_get(&C, id[i], &sonra));
            degisti = degisti || (sonra.v.u32 != once[i].v.u32);
        }
        if (silahli && OUT.status.armed) {
            TEST_ASSERT_FALSE(degisti);
            silahli_cagri++;
        }
        if (degisti) {
            degisim++;
        }
    }
    char ileti[160];
    (void)snprintf(ileti, sizeof(ileti), "%u adım: silahlı çağrı %u, parametre değişimi %u", OZ_ADIM,
                   silahli_cagri, degisim);
    TEST_MESSAGE(ileti);
    TEST_ASSERT_TRUE(degisim > 100u);
    TEST_ASSERT_TRUE(silahli_cagri > 1000u);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_ozellik_degismezler_a_f);
    RUN_TEST(test_ozellik_degismez_g);
    return UNITY_END();
}
