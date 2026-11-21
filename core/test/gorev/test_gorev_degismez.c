/*
 * test_gorev_degismez.c — W3-gorev kabul md.4 (rastgele komut ve arıza dizilerinde, sabit
 * tohumla, değişmezler) ve md.5 (belirlenimcilik).
 *
 * Düzenek (sol bulgu 6): gerçek çekirdek örneği (ida_core_t, geçerli parametre bloğuyla init),
 * gerçek emniyet modülü ((b) daraltması: DISARM ve NARROW_MODE mandalları, SESSION_END,
 * acil durdurmalı emniyet örnekleri), görev modülünün adımı ve GERÇEK dağıtıcı
 * (ida_kom_isle: A4-K48 yineleme penceresi, A4-K45 / W3-K9 NARROWED > IMU_REJECTED > IMU_LAG,
 * ARM'ın W2 kuralları, görev işleyicisi); reddedilen çağrıda ida_kom_reddet. Port yolundan
 * (ida_core_step) farkı yalnız görev adımının girdisidir: yer tutucu kestirim INIT'ten
 * çıkmadığı için kestirim düzeyi, bileşen geçerliliği ve IMU bayatlığı rastgele verilir ve
 * dağıtıcının bağlamı çerçevedeki gibi (cerceve.c olagan) kurulur.
 *
 * "Emniyet kısıtı" W3-K2'deki anlamıyladır: çağrıda uygulanan daraltma ((b) ya da adımın arıza
 * daraltması). Sanal emniyet girdileri araç silahlandırmasına girmez (W3-K1).
 *
 * Değişmezler (her kabul edilen çağrının sonunda):
 *   D1 daraltma uygulanan çağrıda yinelenmeyen her genişletici REJECTED / NARROWED alır (IMU
 *      reddi ya da IMU_LAG olsa da; W3-K9) ve armed 0 → 1 olmaz, kip GUIDED/AUTO'ya genişlemez;
 *   D2 DISARM ya da SESSION_END ile aynı çağrıdaki ARM etkisiz: armed = 0;
 *   D3 armed 0 → 1 ve kip → GUIDED/AUTO yalnız o çağrıda ACCEPTED almış ARM / SET_MODE ile;
 *      adım (arıza eylemleri, kip düşmesi) hiçbir zaman genişletmez;
 *   D4 yinelenen komut ek etki üretmez: işlenmiş komutlar aynı kimlikle yeniden gelince
 *      DUPLICATE alır, görev ve emniyet durumu bayt bayt aynı kalır, günlük ve sayaç üretilmez;
 *   D5 itki isteği (KAPALI değil) ⇒ silahlı ∧ arıza < PASSIVE ∧ kip itkili;
 *   D6 iç tutarlılık.
 */
#include <string.h>

#include "cekirdek.h"
#include "cerceve/komut.h"
#include "destek.h"
#include "gorev/gorev_ic.h"
#include "unity.h"

void setUp(void);
void tearDown(void);
void setUp(void) {}
void tearDown(void) {}

#define MS 1000000LL
#define CAGRI_SAYISI 20000u

typedef struct {
    ida_core_t *c;
    ida_gor_girdi_t gi;
    int64_t now;
    ds_rastgele_t r;
    bool oturum_basi_bekliyor;
    bool oturum;
    uint32_t ozet; /* FNV-1a: çağrı başına gözlenen çıktılar (md.5) */
    /* kapsama sayaçları */
    uint32_t silahli_cagri;
    uint32_t arm_kabul;
    uint32_t guided_kabul;
    uint32_t daralt_ret;
    uint32_t imu_red_daralt;
    uint32_t yineleme_denetimi;
    uint32_t fs_gorulen[6];
} duzenek_t;

static ida_core_t CA;
static ida_core_t CB;
static duzenek_t A;
static duzenek_t B;
static ida_in_t IN;
static ida_in_t IN2;
static ida_out_t OUT;
static ida_out_t OUT2;
static ida_gor_t G0;
static ida_emn_t E0;
static ida_core_counters_t K0;

static uint32_t zar(duzenek_t *d, uint32_t n)
{
    return ds_rastgele(&d->r) % n;
}

static void ozet_ekle(duzenek_t *d, const void *p, size_t n)
{
    const uint8_t *b = (const uint8_t *)p;
    for (size_t i = 0u; i < n; i++) {
        d->ozet ^= b[i];
        d->ozet *= 16777619u;
    }
}

static void kur(duzenek_t *d, ida_core_t *c, uint32_t tohum)
{
    (void)memset(d, 0, sizeof(*d));
    d->c = c;
    TEST_ASSERT_EQUAL(IDA_RC_OK, ds_kur(c));
    d->now = 2000 * MS;
    d->gi.prm = &c->prm;
    d->gi.est_level = (uint8_t)IDA_EST_INIT;
    d->gi.param_ok = true;
    d->gi.nav_derinlik = 1.0f;
    d->r.s = tohum;
    d->ozet = 2166136261u;
    d->oturum = true;
}

static void ornekler(duzenek_t *d)
{
    ida_gor_t *g = &d->c->gor;
    if (zar(d, 100u) < 90u) {
        ida_sample_t s;
        (void)memset(&s, 0, sizeof(s));
        s.hdr.type = (uint8_t)IDA_SENS_BATT;
        s.hdr.t_rx.ns = d->now;
        const uint32_t z = zar(d, 100u);
        s.u.batt.voltage = (z < 3u) ? 12.0f : ((z < 8u) ? 13.0f : 16.0f);
        ida_gor_ornek(g, &s);
    }
    for (uint32_t inst = 0u; inst < 2u; inst++) {
        if (zar(d, 100u) < 90u) {
            ida_sample_t s;
            (void)memset(&s, 0, sizeof(s));
            s.hdr.type = (uint8_t)IDA_SENS_LEAK;
            s.hdr.inst = (uint8_t)inst;
            s.hdr.t_rx.ns = d->now;
            s.u.leak.mask = (zar(d, 100u) < 2u) ? (uint8_t)1u : (uint8_t)0u;
            ida_gor_ornek(g, &s);
        }
    }
}

static void cevre(duzenek_t *d)
{
    const uint32_t z = zar(d, 1000u);
    if (z < 6u) {
        d->gi.est_level = (uint8_t)IDA_EST_DEGRADED;
    } else if (z < 9u) {
        d->gi.est_level = (uint8_t)IDA_EST_BAD;
    } else if (z < 10u) {
        d->gi.est_level = (uint8_t)IDA_EST_FAULT;
    } else if (z < 12u) {
        d->gi.est_level = (uint8_t)IDA_EST_INIT;
    } else if (z < 120u) {
        d->gi.est_level = (uint8_t)IDA_EST_OK;
    }
    const uint8_t dikey =
        (uint8_t)(IDA_GOR_G_TUTUM | IDA_GOR_G_YON | IDA_GOR_G_DERINLIK | IDA_GOR_G_DIKEY_HIZ);
    const uint8_t est = d->gi.est_level;
    uint8_t v = (est == (uint8_t)IDA_EST_OK)
                    ? (uint8_t)(dikey | IDA_GOR_G_YATAY_HIZ | IDA_GOR_G_KONUM)
                    : ((est == (uint8_t)IDA_EST_DEGRADED) ? dikey : (uint8_t)0u);
    if (zar(d, 100u) < 5u) {
        v = (uint8_t)(v & (uint8_t)~(1u << zar(d, 6u))); /* tek bileşen kaybı */
    }
    d->gi.gecerlilik = v;
    d->gi.imu_bayat = zar(d, 1000u) < 8u;
    d->gi.imu_lag = zar(d, 1000u) < 5u;
    d->gi.param_ok = d->c->prm.param_ok;
    d->gi.nav_derinlik = (float)zar(d, 60u) * 0.1f;
    d->gi.nav_yon = ((float)zar(d, 628u) * 0.01f) - 3.14f;
    d->gi.nav_ne[0] = (float)zar(d, 100u);
    d->gi.nav_ne[1] = (float)zar(d, 100u);
    d->gi.now_ns = d->now;
}

static void komutlar_uret(duzenek_t *d, ida_in_t *in)
{
    const uint32_t n = zar(d, 6u);
    for (uint32_t i = 0u; i < n; i++) {
        ida_cmd_t *c = &in->cmds[i];
        (void)memset(c, 0, sizeof(*c));
        c->cmd_id = zar(d, 400u);
        const uint32_t t = zar(d, 100u);
        if (t < 35u) {
            c->type = (uint8_t)IDA_CMD_HEARTBEAT;
        } else if (t < 65u) {
            c->type = (uint8_t)IDA_CMD_ARM;
        } else if (t < 78u) {
            c->type = (uint8_t)IDA_CMD_SET_MODE;
            c->u.set_mode.mode = (uint8_t)IDA_MODE_GUIDED;
        } else if (t < 82u) {
            c->type = (uint8_t)IDA_CMD_SET_MODE;
            c->u.set_mode.mode = (uint8_t)IDA_MODE_AUTO;
        } else if (t < 95u) {
            c->type = (uint8_t)IDA_CMD_FAILSAFE_RESET;
        } else {
            c->type = (uint8_t)IDA_CMD_MISSION_BEGIN;
        }
    }
    in->n_cmds = (uint8_t)n;
}

/* (b) adımı: rastgele daraltıcılar; acil durdurmanın W4 gevşetmesi yerine test kancası. */
static void daralt(duzenek_t *d)
{
    ida_core_t *c = d->c;
    ida_in_t *in = &IN;
    if (zar(d, 100u) < 4u) {
        in->narrow.mask = (uint8_t)(in->narrow.mask | IDA_NARROW_DISARM);
        in->narrow.cmd_id[0] = 1000u + zar(d, 100u);
    }
    if (zar(d, 100u) < 4u) {
        in->narrow.mask = (uint8_t)(in->narrow.mask | IDA_NARROW_MODE);
        in->narrow.mode = (uint8_t)(IDA_MODE_MANUAL + zar(d, 4u));
        in->narrow.cmd_id[1] = 2000u + zar(d, 100u);
    }
    const uint32_t z = zar(d, 1000u);
    if (z < 3u) {
        in->plat.link_event = (uint8_t)IDA_LINKEV_SESSION_END;
    } else if (z < 6u) {
        in->plat.link_event = (uint8_t)IDA_LINKEV_SESSION_START;
    }
    if (zar(d, 100u) < 3u) {
        ida_sample_t *s = &in->samples[0];
        s->hdr.type = (uint8_t)IDA_SENS_SAFETY;
        s->u.safety.estop = 1u;
        in->n_samples = 1u;
    }
    ida_emn_baglam_t eb;
    eb.gun = &c->gun;
    eb.now.ns = d->now;
    ida_emn_daralt(&c->emn, in, &eb);
    in->n_samples = 0u; /* örnek kapısı bu düzenekte yok */
    if (in->plat.link_event == (uint8_t)IDA_LINKEV_SESSION_START) {
        ida_emn_oturum_basi(&c->emn);
        d->oturum_basi_bekliyor = true;
        d->oturum = true;
    }
    if (in->plat.link_event == (uint8_t)IDA_LINKEV_SESSION_END) {
        d->oturum = false;
    }
    if ((c->emn.d.kapanan == 0u) && (zar(d, 100u) < 20u)) {
        c->emn.kisitli = (uint8_t)(c->emn.kisitli & (uint8_t)~IDA_EMN_ESTOP);
    }
    in->plat.link_state = d->oturum ? (uint8_t)IDA_LINK_RUNNING : (uint8_t)IDA_LINK_NONE;
}

static bool genisletici(const ida_cmd_t *c)
{
    return ida_kom_genisletici(c);
}

/* Çerçevenin kurduğu dağıtıcı bağlamı (cerceve.c olagan; W3-K9 dahil). */
static void baglam_kur(const duzenek_t *d, const ida_in_t *in, bool imu_red,
                       ida_komut_baglam_t *kb)
{
    const ida_core_t *c = d->c;
    (void)memset(kb, 0, sizeof(*kb));
    kb->now_w.ns = d->now;
    kb->daraldi = c->emn.d.herhangi || ida_gor_daraldi(&c->gor);
    kb->imu_red = imu_red;
    kb->imu_lag = d->gi.imu_lag;
    kb->armed = c->emn.armed;
    kb->param_ok = c->prm.param_ok;
    kb->events_ok = ida_olay_kullanilabilir(&c->olay);
    for (uint32_t i = 0u; i < (uint32_t)in->n_cmds; i++) {
        if (in->cmds[i].type == (uint8_t)IDA_CMD_ARM) {
            kb->cagrida_arm = true;
        }
    }
    kb->oturum_var = d->oturum;
    kb->mode = c->emn.mode;
    kb->est_level = d->gi.est_level;
    kb->fs_level = ida_gor_fs_duzey(&c->gor);
    kb->param_store = (uint8_t)IDA_PSTORE_IDLE;
    kb->fc_engel = 0u;
}

static void cagri(duzenek_t *d, bool denetle)
{
    ida_core_t *c = d->c;
    d->now += (int64_t)(5u + zar(d, 200u)) * MS;
    if (zar(d, 2000u) == 0u) {
        d->now += (int64_t)(3000u + zar(d, 70000u)) * MS; /* uzun boşluk: zamanlayıcılar */
    }
    (void)memset(&IN, 0, sizeof(IN));
    daralt(d);
    komutlar_uret(d, &IN);
    const bool disarm = c->emn.d.disarm;
    if (zar(d, 100u) >= 92u) {
        /* (c)/(d)'de reddedilen çağrı (A4-K114): görev adımı ve işleyicisi çalışmaz. */
        ida_kom_reddet(&IN, &c->cer.cnt, &OUT);
        ozet_ekle(d, &c->emn.armed, sizeof(c->emn.armed));
        ida_gun_kur(&c->gun);
        return;
    }
    const bool armed0 = c->emn.armed;
    const uint8_t kip0 = c->emn.mode;
    if (d->oturum_basi_bekliyor) {
        ida_gor_oturum_basi(&c->gor);
        d->oturum_basi_bekliyor = false;
    }
    ornekler(d);
    cevre(d);
    if (zar(d, 3000u) == 0u) {
        c->gor.mission_state = (uint8_t)IDA_MS_LOADED; /* test kancası: AUTO'ya izin */
    }
    const uint8_t kip_adim_oncesi = c->emn.mode;
    ida_gor_adim_ic(&c->gor, &c->emn, &d->gi, &c->gun);
    if (denetle) {
        /* D3: adım genişletmez. */
        TEST_ASSERT_TRUE(!c->emn.armed || armed0);
        TEST_ASSERT_TRUE(((c->emn.mode != (uint8_t)IDA_MODE_GUIDED) &&
                          (c->emn.mode != (uint8_t)IDA_MODE_AUTO)) ||
                         (c->emn.mode == kip_adim_oncesi));
    }
    const bool imu_red = zar(d, 100u) < 3u;
    ida_komut_baglam_t kb;
    baglam_kur(d, &IN, imu_red, &kb);
    ida_kom_isle(c, &IN, &kb, &OUT);
    /* Çağrının daraltılmış olup olmadığı durumdan (çerçevenin bayrağından bağımsız): (b)
       daraltması ya da görev adımının arıza daraltması (W3-K2). */
    const bool cagri_daraldi = c->emn.d.herhangi || c->gor.adim_daralt;

    bool arm_kabul = false;
    bool kip_kabul = false;
    uint32_t ofs = 0u; /* sonuçlar: önce mandal bitleri (dizin sırasıyla), sonra cmds[] (A4-K43) */
    for (uint32_t b = 0u; b < IDA_PORT_N_NARROW; b++) {
        if ((IN.narrow.mask & (1u << b)) != 0u) {
            ofs++;
        }
    }
    TEST_ASSERT_EQUAL_UINT8(ofs + IN.n_cmds, OUT.n_results);
    for (uint32_t i = 0u; i < (uint32_t)IN.n_cmds; i++) {
        const ida_cmd_t *cm = &IN.cmds[i];
        const ida_cmd_result_t *r = &OUT.results[ofs + i];
        TEST_ASSERT_EQUAL_UINT32(cm->cmd_id, r->cmd_id);
        ozet_ekle(d, &r->ack, sizeof(r->ack));
        ozet_ekle(d, &r->reason, sizeof(r->reason));
        if (r->ack == (uint8_t)IDA_ACK_DUPLICATE) {
            continue;
        }
        if (genisletici(cm) && cagri_daraldi) {
            /* D1 (A4-K45 konumu; W3-K9): IMU reddi ya da IMU_LAG olsa da NARROWED. */
            if (denetle) {
                TEST_ASSERT_EQUAL_UINT8(IDA_ACK_REJECTED, r->ack);
                TEST_ASSERT_EQUAL_UINT16(IDA_REASON_NARROWED, r->reason);
            }
            if (!c->emn.d.herhangi) {
                d->daralt_ret++;
                if (kb.imu_red || kb.imu_lag) {
                    d->imu_red_daralt++;
                }
            }
        }
        if (r->ack == (uint8_t)IDA_ACK_ACCEPTED) {
            if (cm->type == (uint8_t)IDA_CMD_ARM) {
                arm_kabul = true;
                d->arm_kabul++;
            }
            if (cm->type == (uint8_t)IDA_CMD_SET_MODE) {
                kip_kabul = true;
                d->guided_kabul++;
            }
        }
    }

    if (denetle && (IN.n_cmds > 0u) && (zar(d, 100u) < 30u)) {
        /* D4: bu çağrıda işlenen (yinelenmeyen) komutlar aynı kimlikle yeniden gelir
           (mandalsız, örneksiz bir sonraki parti gibi). Yalnız bunlar pencerede kesin vardır;
           32'lik pencereden daha önce düşmüş bir kimliğin yeniden işlenmesi A4-K48'e uygundur. */
        (void)memset(&IN2, 0, sizeof(IN2));
        uint32_t n2 = 0u;
        for (uint32_t i = 0u; i < (uint32_t)IN.n_cmds; i++) {
            if (OUT.results[ofs + i].ack != (uint8_t)IDA_ACK_DUPLICATE) {
                IN2.cmds[n2] = IN.cmds[i];
                n2++;
            }
        }
        IN2.n_cmds = (uint8_t)n2;
        (void)memcpy(&G0, &c->gor, sizeof(G0));
        (void)memcpy(&E0, &c->emn, sizeof(E0));
        K0 = c->cer.cnt;
        const uint8_t n_gun = c->gun.n;
        const uint32_t dusen = c->gun.dusen;
        ida_komut_baglam_t kb2;
        baglam_kur(d, &IN2, false, &kb2);
        ida_kom_isle(c, &IN2, &kb2, &OUT2);
        for (uint32_t i = 0u; i < (uint32_t)IN2.n_cmds; i++) {
            TEST_ASSERT_EQUAL_UINT8(IDA_ACK_DUPLICATE, OUT2.results[i].ack);
            TEST_ASSERT_EQUAL_UINT16(IDA_REASON_NONE, OUT2.results[i].reason);
        }
        TEST_ASSERT_EQUAL_MEMORY(&G0, &c->gor, sizeof(G0));
        TEST_ASSERT_EQUAL_MEMORY(&E0, &c->emn, sizeof(E0));
        TEST_ASSERT_EQUAL_MEMORY(&K0, &c->cer.cnt, sizeof(K0));
        TEST_ASSERT_EQUAL_UINT8(n_gun, c->gun.n);
        TEST_ASSERT_EQUAL_UINT32(dusen, c->gun.dusen);
        d->yineleme_denetimi++;
    }

    ida_gor_kon_istek_t ist;
    ida_gor_istek(&c->gor, &c->emn, &ist);
    if (denetle) {
        /* D1 */
        if (cagri_daraldi) {
            TEST_ASSERT_TRUE(!c->emn.armed || armed0);
            TEST_ASSERT_TRUE(((c->emn.mode != (uint8_t)IDA_MODE_GUIDED) &&
                              (c->emn.mode != (uint8_t)IDA_MODE_AUTO)) ||
                             (c->emn.mode == kip0));
        }
        /* D2 */
        TEST_ASSERT_TRUE(!disarm || !c->emn.armed);
        /* D3 */
        if (c->emn.armed && !armed0) {
            TEST_ASSERT_TRUE(arm_kabul);
        }
        if (((c->emn.mode == (uint8_t)IDA_MODE_GUIDED) || (c->emn.mode == (uint8_t)IDA_MODE_AUTO)) &&
            (c->emn.mode != kip0)) {
            TEST_ASSERT_TRUE(kip_kabul);
        }
        /* Bu çağrıda silahlandıysa ve kip komutla değişmediyse arıza eylemi HOLD'un altındadır
           (sol bulgu 1). ARM'dan sonra kabul edilen bir kip değişimi aracı yeni kipin eylemiyle
           silahlı bırakabilir (W3-K13; ardışık iki çağrıyla aynı); sıranın kendisi
           test_gorev_sol.c'nin B1 testlerinde sınanır. */
        if (c->emn.armed && !armed0 && !kip_kabul) {
            TEST_ASSERT_TRUE(c->gor.fs_level < (uint8_t)IDA_FS_HOLD);
        }
        /* D5 */
        if (ist.kip != (uint8_t)IDA_GOR_KON_KIP_KAPALI) {
            TEST_ASSERT_TRUE(c->emn.armed);
            TEST_ASSERT_TRUE(c->gor.fs_level < (uint8_t)IDA_FS_PASSIVE_SURFACE);
            TEST_ASSERT_TRUE((c->emn.mode != (uint8_t)IDA_MODE_INIT) &&
                             (c->emn.mode != (uint8_t)IDA_MODE_DISARMED));
        }
        /* D6 */
        TEST_ASSERT_TRUE(ida_gor_tutarli(&c->gor));
    }
    if (c->emn.armed) {
        d->silahli_cagri++;
    }
    if (c->gor.fs_level < 6u) {
        d->fs_gorulen[c->gor.fs_level]++;
    }
    ozet_ekle(d, &c->emn.armed, sizeof(c->emn.armed));
    ozet_ekle(d, &c->emn.mode, sizeof(c->emn.mode));
    ozet_ekle(d, &c->gor.fs_level, sizeof(c->gor.fs_level));
    ozet_ekle(d, &c->gor.fs_nedenleri, sizeof(c->gor.fs_nedenleri));
    ozet_ekle(d, &ist.kip, sizeof(ist.kip));
    ozet_ekle(d, &ist.dikey, sizeof(ist.dikey));
    ozet_ekle(d, &ist.yon_kanali, sizeof(ist.yon_kanali));
    ozet_ekle(d, &ist.yatay, sizeof(ist.yatay));
    ozet_ekle(d, &ist.derinlik, sizeof(ist.derinlik));
    ozet_ekle(d, &ist.yon, sizeof(ist.yon));
    for (uint32_t i = 0u; i < (uint32_t)c->gun.n; i++) {
        ozet_ekle(d, &c->gun.kayit[i].code, sizeof(c->gun.kayit[i].code));
        ozet_ekle(d, &c->gun.kayit[i].arg0, sizeof(c->gun.kayit[i].arg0));
        ozet_ekle(d, &c->gun.kayit[i].arg1, sizeof(c->gun.kayit[i].arg1));
    }
    ida_gun_kur(&c->gun);
}

static void test_md4_rastgele_degismezler(void)
{
    static const uint32_t tohumlar[3] = {0x1234567u, 0xC0FFEEu, 0x9E3779B9u};
    for (uint32_t t = 0u; t < 3u; t++) {
        kur(&A, &CA, tohumlar[t]);
        for (uint32_t i = 0u; i < CAGRI_SAYISI; i++) {
            cagri(&A, true);
        }
        /* Kapsama: düzenek değişmezlerin sınandığı durumlara gerçekten ulaştı. */
        TEST_ASSERT_TRUE(A.arm_kabul > 10u);
        TEST_ASSERT_TRUE(A.silahli_cagri > 1000u);
        TEST_ASSERT_TRUE(A.guided_kabul > 5u);
        TEST_ASSERT_TRUE(A.daralt_ret > 0u);
        TEST_ASSERT_TRUE(A.imu_red_daralt > 0u); /* W3-K9 önceliği gerçekten sınandı */
        TEST_ASSERT_TRUE(A.yineleme_denetimi > 1000u);
        TEST_ASSERT_TRUE(A.fs_gorulen[IDA_FS_WARN] > 0u);
        TEST_ASSERT_TRUE(A.fs_gorulen[IDA_FS_HOLD] + A.fs_gorulen[IDA_FS_SURFACE] > 0u);
        TEST_ASSERT_TRUE(A.fs_gorulen[IDA_FS_PASSIVE_SURFACE] > 0u);
    }
}

static void test_md5_belirlenimcilik(void)
{
    kur(&A, &CA, 0xBADC0DEu);
    kur(&B, &CB, 0xBADC0DEu);
    for (uint32_t i = 0u; i < CAGRI_SAYISI; i++) {
        cagri(&A, false);
        cagri(&B, false); /* iç içe: iki örnek aynı süreçte */
        TEST_ASSERT_EQUAL_HEX32(A.ozet, B.ozet);
    }
    TEST_ASSERT_EQUAL_MEMORY(&CA.gor, &CB.gor, sizeof(CA.gor));
    TEST_ASSERT_EQUAL_MEMORY(&CA.emn, &CB.emn, sizeof(CA.emn));
    TEST_ASSERT_EQUAL_MEMORY(&CA.cer.cnt, &CB.cer.cnt, sizeof(CA.cer.cnt));
    /* Farklı tohum farklı dizi verir (özetin duyarlılığı). */
    kur(&B, &CB, 0xBADC0DFu);
    for (uint32_t i = 0u; i < CAGRI_SAYISI; i++) {
        cagri(&B, false);
    }
    TEST_ASSERT_TRUE(A.ozet != B.ozet);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_md4_rastgele_degismezler);
    RUN_TEST(test_md5_belirlenimcilik);
    return UNITY_END();
}
