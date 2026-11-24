/*
 * tests/kabul/atis — emniyet değişmezlerinin özellik testleri (W5-atis-emniyet "Kabul" md. 2):
 * rastgele komut, olay sonucu ve emniyet örneği dizileri; sabit tohum; her test en az 200 000
 * adım. Düşen dizinin tohumu ve adımı hata iletisinde ve tearDown'da yazdırılır.
 *
 * Yedi değişmez:
 *   (a) acil durdurma basılı ya da bayatken hiçbir çağrıda genişletici olay yok;
 *   (b) FIRE yalnız ARMED'de kabul edilmiş operatör FC_FIRE'ından ve o tüpün izni APPLIED
 *       olduktan sonra, izin penceresi (hold_ns) içinde;
 *   (c) FAULT_LATCHED yalnız FC_RESET ile terk edilir;
 *   (d) TEST'te genişletici olay yok;
 *   (e) daraltıcı komut hiçbir durumda ve biçimde reddedilmez, etkisi uygulanır;
 *   (f) sonucu bilinmeyen genişletici olay uygulanmış sayılmaz;
 *   (g) silahlıyken emniyet parametresi değişmez.
 * Değişmezlerin her biri her adımda düzenekte (kabul_atis.c) denetlenir; her test, kendi
 * değişmezini sınayan girdilerin ağırlığını artıran bir profille ve ayrı tohumla koşar. Kapsam
 * koşulu: dizinin ARMED, PERMIT_OPEN, FIRE_PENDING, TEST ve FAULT_LATCHED durumlarına yeterince
 * girmesi ve FIRE üretmesi gerekir (girmeyen dizi değişmezi sınamamıştır).
 */
#include <stdio.h>
#include <string.h>

#include "kabul_atis.h"
#include "unity.h"

#define ADIM_SAYISI 200000
#define YENIDEN_KUR 5000 /* her FIRE tüpü boşaltır (yeniden doldurma W5-K4 çelişkisidir): sık yeniden kurulum */

typedef struct {
    const char *ad;
    uint64_t tohum;
    unsigned estop;    /* binde: acil durdurma darbesi */
    unsigned tup;      /* binde: fiş/pim değişimi */
    unsigned gecersiz; /* binde: VALID = 0 / inst 1 / reddedilen çağrı örneği */
    unsigned bayat;    /* on binde: emniyet akışı kesilmesi ya da t_meas donması */
    unsigned dar;      /* binde: daraltıcı (mandal, cmds[], biçimi bozuk) */
    unsigned kayip;    /* yüzde: olay sonucu kaybı / ret */
    unsigned test;     /* binde: FC_TEST_ENTER */
    unsigned param;    /* binde: B sınıfı parametre yazma */
    unsigned oturum;   /* on binde: oturum sonu */
} profil_t;

static const profil_t PROFIL[7] = {
    {"a_acil_durdurma", UINT64_C(0xA0A0A0A000000001), 12u, 4u, 6u, 8u, 2u, 5u, 2u, 0u, 2u},
    {"b_fire_zinciri", UINT64_C(0xB0B0B0B000000002), 2u, 2u, 2u, 2u, 2u, 8u, 1u, 0u, 2u},
    {"c_kilit", UINT64_C(0xC0C0C0C000000003), 3u, 3u, 3u, 4u, 3u, 25u, 2u, 0u, 3u},
    {"d_test", UINT64_C(0xD0D0D0D000000004), 3u, 3u, 3u, 3u, 3u, 8u, 25u, 0u, 2u},
    {"e_daraltma", UINT64_C(0xE0E0E0E000000005), 3u, 3u, 3u, 3u, 25u, 8u, 2u, 0u, 4u},
    {"f_bilinmeyen", UINT64_C(0xF0F0F0F000000006), 2u, 2u, 2u, 2u, 2u, 30u, 1u, 0u, 2u},
    {"g_parametre", UINT64_C(0x1717171700000007), 2u, 2u, 2u, 2u, 3u, 8u, 1u, 30u, 2u},
};

static kc_rng_t R;

static bool olasilik(unsigned pay, unsigned payda)
{
    return kc_rng_aralik(&R, payda) < pay;
}

static uint8_t tup_eksik(uint8_t bit)
{
    return (uint8_t)(KC_TUP_ACIK & (uint8_t)~bit);
}

static const uint8_t KIPLER[] = {IDA_MODE_GUIDED, IDA_MODE_STATION_KEEP, IDA_MODE_DEPTH_HOLD, IDA_MODE_STABILIZE, IDA_MODE_MANUAL};
static uint8_t g_kip;        /* MODE bitinin kalktığı bilinen kip (0: bilinmiyor) */
static unsigned g_kip_dene;  /* sıradaki aday */
static int g_oturumsuz;      /* > 0: oturum kapalı kalan adım sayısı */
static int g_kesinti;        /* > 0: emniyet akışının kesik kaldığı adım */
static int g_donma;          /* > 0: t_meas donmuş */
static int g_hb_kayip;       /* > 0: kalp atışı yok */
static int g_lag;            /* > 0: IMU gecikmeli */
static uint32_t g_son_id[8];

static void hazirla(void)
{
    ka_kur(1);
    /* Etkin değer ve bayatlık modeli girdiden hesaplanır; çıktının engel bitleri ona göre
       denetlenir (çıktının ESTOP bitine dayanmaz). */
    ka_model_bit_denetimi(true);
    ka_oturum_baslat();
    g_oturumsuz = 0;
    g_kesinti = 0;
    g_donma = 0;
    g_hb_kayip = 0;
    g_lag = 0;
}

static void kip_ekle(ida_in_t *in, uint8_t kip)
{
    if (kip == IDA_MODE_GUIDED) {
        kc_komut_ekle(in, ka_yeni_id(), (uint8_t)IDA_CMD_SET_MODE)->u.set_mode.mode = kip;
    } else {
        (void)ka_mandal(in, (uint8_t)IDA_NARROW_MODE);
        in->narrow.mode = kip;
    }
}

static void komut_yer_var(ida_in_t *in, void (*f)(ida_in_t *))
{
    if (in->n_cmds < IDA_PORT_MAX_CMDS) {
        f(in);
    }
}

static uint8_t g_tup;

static void fc_arm_ekle(ida_in_t *in)
{
    g_tup = (uint8_t)kc_rng_aralik(&R, 2u);
    g_son_id[kc_rng_aralik(&R, 8u) & 7u] = ka_fc(in, (uint8_t)IDA_FC_ARM, g_tup);
}

static void fc_fire_ekle(ida_in_t *in)
{
    uint8_t t = olasilik(8u, 10u) ? g_tup : (uint8_t)(1u - g_tup);
    g_son_id[kc_rng_aralik(&R, 8u) & 7u] = ka_fc(in, (uint8_t)IDA_FC_FIRE, t);
}

static void gurultu_ekle(ida_in_t *in)
{
    uint8_t op = (uint8_t)kc_rng_aralik(&R, 10u);
    if (op == IDA_FC_DISARM || op == IDA_FC_ABORT) {
        op = IDA_FC_TEST_EXIT;
    }
    uint32_t id = ka_fc(in, op, (uint8_t)kc_rng_aralik(&R, 3u));
    if (olasilik(1u, 5u)) {
        (*ka_son_komut(in)).u.fc.arg0 = 1u;
    }
    if (olasilik(1u, 3u)) {
        /* yinelenen kimlik (A4-K48) */
        (*ka_son_komut(in)).cmd_id = g_son_id[kc_rng_aralik(&R, 8u) & 7u];
    } else {
        g_son_id[kc_rng_aralik(&R, 8u) & 7u] = id;
    }
}

static void param_ekle(ida_in_t *in)
{
    static const char *const adlar[] = {"FC_HOLD_NS", "FC_APPLY_BY_NS", "T_emn_bayat"};
    size_t i = kc_rng_aralik(&R, 3u);
    int id = ka_param_id(adlar[i]);
    if (id < 0 || in->n_cmds + 2u > IDA_PORT_MAX_CMDS) {
        return;
    }
    uint32_t deger;
    if (i == 0u) {
        deger = UINT32_C(60000000) + kc_rng_aralik(&R, 390u) * UINT32_C(1000000);
    } else if (i == 1u) {
        deger = UINT32_C(16000000) + kc_rng_aralik(&R, 100u) * UINT32_C(1000000);
    } else {
        deger = 150u + kc_rng_aralik(&R, 200u);
    }
    ida_cmd_t *c = kc_komut_ekle(in, ka_yeni_id(), (uint8_t)IDA_CMD_PARAM_SET);
    c->u.param.id = (uint16_t)id;
    c->u.param.type = (uint8_t)IDA_PT_U32;
    c->u.param.v.u32 = deger;
    (void)ka_komut(in, (uint8_t)IDA_CMD_PARAM_COMMIT);
}

static void politika_sec(const profil_t *p)
{
    static const uint8_t kodlar[] = {IDA_EVR_REJ_EXPIRED,  IDA_EVR_REJ_SAFE,        IDA_EVR_REJ_TUBE_EMPTY,
                                     IDA_EVR_REJ_SESSION,  IDA_EVR_DROPPED_GATE,    IDA_EVR_DROPPED_NO_SLOT,
                                     IDA_EVR_EXPIRED_NO_ACK, IDA_EVR_DUPLICATE,     KA_SONUC_YOK};
    for (unsigned k = 1; k < KA_TUR_SAYISI; ++k) {
        ka_politika_t *q = &ka_ayar.politika[k];
        if (olasilik(p->kayip, 100u)) {
            q->kod = kodlar[kc_rng_aralik(&R, (uint32_t)sizeof kodlar) % sizeof kodlar];
            q->uygula = olasilik(1u, 2u);
        } else {
            q->kod = (uint8_t)IDA_EVR_APPLIED;
            q->uygula = true;
        }
        q->gecikme = 1u + kc_rng_aralik(&R, 12u);
    }
}

static void adim(const profil_t *p)
{
    const ida_out_t *o = ka_out();
    uint8_t st = o->fc.state;
    uint32_t m = o->fc.inhibit_mask;
    /* Emniyet girdileri: çoğunlukla gevşek. */
    ka_ayar.estop = 0u;
    ka_ayar.tup[0] = KC_TUP_ACIK;
    ka_ayar.tup[1] = KC_TUP_ACIK;
    ka_ayar.tick = false;
    ka_ayar.imu_bozuk = false;
    if (olasilik(p->estop, 1000u)) {
        ka_ayar.estop = 1u;
    }
    if (olasilik(p->tup, 1000u)) {
        static const uint8_t bitler[3] = {IDA_TUBE_PLUG_IN, IDA_TUBE_PIN_OUT, IDA_TUBE_LOADED};
        ka_ayar.tup[kc_rng_aralik(&R, 2u) & 1u] = tup_eksik(bitler[kc_rng_aralik(&R, 3u) % 3u]);
    }
    if (g_kesinti > 0) {
        --g_kesinti;
        ka_ayar.emniyet = false;
    } else {
        ka_ayar.emniyet = true;
        if (olasilik(p->bayat, 10000u)) {
            g_kesinti = 40 + (int)kc_rng_aralik(&R, 40u);
        }
    }
    if (g_donma > 0) {
        --g_donma;
        ka_ayar.tmeas_dondur = true;
    } else {
        ka_ayar.tmeas_dondur = false;
        if (olasilik(p->bayat, 10000u)) {
            g_donma = 40 + (int)kc_rng_aralik(&R, 40u);
        }
    }
    if (g_hb_kayip > 0) {
        --g_hb_kayip;
        ka_ayar.heartbeat = false;
    } else {
        ka_ayar.heartbeat = true;
        if (olasilik(1u, 20000u)) {
            g_hb_kayip = 200 + (int)kc_rng_aralik(&R, 1000u);
        }
    }
    if (g_lag > 0) {
        /* IMU gecikmesi yavaş artar (çağrı başına 1 ms, 25 ms'de durur): IMU kabul edilmeye
           devam eder (t_meas artar), IMU_LAG mandalı kurulur, bayatlık oluşmaz. */
        --g_lag;
        ka_ayar.imu_gecikme = ka_ayar.imu_gecikme + KA_MS > 25 * KA_MS ? 25 * KA_MS : ka_ayar.imu_gecikme + KA_MS;
    } else {
        ka_ayar.imu_gecikme = 0;
        if (olasilik(1u, 5000u)) {
            g_lag = 30 + (int)kc_rng_aralik(&R, 30u);
        }
    }
    politika_sec(p);
    /* Oturum. */
    bool oturum_sonu = false;
    bool oturum_baslangici = false;
    if (g_oturumsuz > 0) {
        --g_oturumsuz;
        ka_ayar.link_state = (uint8_t)IDA_LINK_NONE;
        if (g_oturumsuz == 0) {
            ka_ayar.link_state = (uint8_t)IDA_LINK_RUNNING;
            oturum_baslangici = true;
        }
    } else if (olasilik(p->oturum, 10000u)) {
        oturum_sonu = true;
        g_oturumsuz = 5 + (int)kc_rng_aralik(&R, 60u);
    }
    /* Çağrı türü. Oturum sonu platformun A4-K122 çağrısıyla verilir (örnek ve komut yok):
       SESSION_END'i aynı çağrıda gevşek emniyet örneğiyle birleştiren girdide örneğin etkisi
       sözleşmede açıkça sıralanmadığından (mimar kararı bekleniyor; kabul.atis.G-Ç18) özellik
       testleri ona dayanmaz. */
    uint32_t tur = kc_rng_aralik(&R, 1000u);
    bool red = false;
    bool nul = false;
    ida_in_t *in;
    if (oturum_sonu) {
        nul = tur >= 995u;
        ka_ayar.sonuclari_tut = nul;
        in = ka_girdi_oturum_sonu();
        ka_ayar.sonuclari_tut = false;
    } else if (tur < 20u) {
        in = ka_girdi_seq(kc_son_seq() - (uint32_t)kc_rng_aralik(&R, 3u));
        red = true;
    } else {
        if (tur < 40u) {
            ka_ayar.tick = true;
        } else if (tur < 55u) {
            ka_ayar.imu_bozuk = true;
        }
        nul = tur >= 995u;
        ka_ayar.sonuclari_tut = nul;
        in = ka_girdi();
        ka_ayar.sonuclari_tut = false;
    }
    if (oturum_baslangici) {
        in->plat.link_event = (uint8_t)IDA_LINKEV_SESSION_START;
    }
    /* Geçersiz / inst 1 emniyet örnekleri. */
    if (!oturum_sonu && olasilik(p->gecersiz, 1000u) && in->n_samples < IDA_PORT_MAX_SAMPLES) {
        uint32_t g = kc_rng_aralik(&R, 3u);
        ida_sample_t *s = ka_emniyet_ekle(in, (uint8_t)(g == 1u ? 1u : 0u), (uint8_t)kc_rng_aralik(&R, 3u), KC_TUP_ACIK,
                                          tup_eksik(IDA_TUBE_PIN_OUT));
        if (g == 0u) {
            s->hdr.flags = 0u;
        } else if (g == 2u) {
            s->u.safety.tube[0] = (uint8_t)(KC_TUP_ACIK | 0x10u);
        }
    }
    /* Yönetici: durumu ilerleten komutlar. Araç düzeyindeki genişleticiler (ARM, SET_MODE)
       yalnız IMU'su olağan, kabul edilecek çağrılarda verilir: bugünkü çekirdeğin kip
       modülünde atış kontrol dışı bir sapma var (IMU_REJECTED çağrısında SET_MODE(GUIDED)
       IMU_REJECTED yerine NARROWED alıyor; A4-K45, K103; rapor). */
    bool arac_temiz = !red && !nul && !ka_ayar.tick && !ka_ayar.imu_bozuk && g_lag == 0 && ka_ayar.imu_gecikme == 0;
    /* Kalp atışı gelmezken yer istasyonundan komut da gelmez (bağlantı yok); oturum sonu
       çağrısında komut yoktur (A4-K122). */
    bool yer_ist = ka_ayar.heartbeat && !oturum_sonu;
    if (yer_ist && arac_temiz && (m & IDA_FCI_NOT_ARMED) != 0u && olasilik(1u, 5u) && in->n_cmds < IDA_PORT_MAX_CMDS) {
        (void)ka_komut(in, (uint8_t)IDA_CMD_ARM);
    }
    if (yer_ist && arac_temiz && (m & IDA_FCI_MODE) != 0u && (m & IDA_FCI_NOT_ARMED) == 0u && olasilik(1u, 20u) &&
        in->n_cmds < IDA_PORT_MAX_CMDS) {
        uint8_t kip = g_kip != 0u ? g_kip : KIPLER[g_kip_dene++ % sizeof KIPLER];
        kip_ekle(in, kip);
    } else if ((m & IDA_FCI_MODE) == 0u && g_kip == 0u) {
        g_kip = o->status.mode;
    }
    if (yer_ist && st == IDA_FCS_FAULT_LATCHED && olasilik(1u, 15u) && in->n_cmds < IDA_PORT_MAX_CMDS) {
        (void)ka_fc(in, (uint8_t)IDA_FC_RESET, (uint8_t)kc_rng_aralik(&R, 2u));
    }
    if (yer_ist && st == IDA_FCS_SAFE && (m & KA_FCARM_ENGEL) == 0u) {
        if (olasilik(p->test, 1000u) && in->n_cmds < IDA_PORT_MAX_CMDS) {
            (void)ka_fc(in, (uint8_t)IDA_FC_TEST_ENTER, 0u);
        } else if (olasilik(3u, 10u)) {
            komut_yer_var(in, fc_arm_ekle);
        }
    }
    if (yer_ist && st == IDA_FCS_ARMED && olasilik(2u, 5u)) {
        komut_yer_var(in, fc_fire_ekle);
    }
    if (yer_ist && st == IDA_FCS_TEST) {
        if (olasilik(1u, 30u) && in->n_cmds < IDA_PORT_MAX_CMDS) {
            (void)ka_fc(in, (uint8_t)IDA_FC_TEST_EXIT, 0u);
        } else if (olasilik(1u, 4u)) {
            komut_yer_var(in, fc_fire_ekle);
        }
    }
    if (yer_ist && olasilik(20u, 1000u)) {
        komut_yer_var(in, gurultu_ekle);
    }
    if (yer_ist && olasilik(p->param, 1000u)) {
        param_ekle(in);
    }
    /* Daraltıcılar (biçimi bozuk olanlar dahil). */
    if (olasilik(p->dar, 1000u)) {
        uint32_t d = kc_rng_aralik(&R, 6u);
        if (d < 4u) {
            static const uint8_t bit[4] = {IDA_NARROW_FC_DISARM, IDA_NARROW_FC_ABORT, IDA_NARROW_DISARM, IDA_NARROW_MODE};
            (void)ka_mandal(in, bit[d]);
        } else if (!oturum_sonu && in->n_cmds < IDA_PORT_MAX_CMDS) {
            (void)ka_fc(in, (uint8_t)(d == 4u ? IDA_FC_DISARM : IDA_FC_ABORT), (uint8_t)kc_rng_aralik(&R, 256u));
            (*ka_son_komut(in)).u.fc.arg1 = (uint32_t)kc_rng_aralik(&R, 2u);
        }
    }
    /* Daraltıcının etkisi için çağrı öncesi model: çekirdekçe açık sayılan izinler (G-Ç11). */
    bool izin_once[IDA_PORT_N_TUBE];
    for (unsigned t = 0; t < IDA_PORT_N_TUBE; ++t) {
        izin_once[t] = ka_model_izin_acik((uint8_t)t, in->now_w.ns);
    }
    ida_rc_t rc;
    if (nul) {
        rc = ka_cagir_out_null();
    } else {
        rc = ka_cagir();
    }
    /* (e) Daraltıcı komutun etkisi (yalnız ACCEPTED sonucu değil). */
    if (!nul) {
        bool fcdisarm = (in->narrow.mask & IDA_NARROW_FC_DISARM) != 0u;
        bool fcabort = (in->narrow.mask & IDA_NARROW_FC_ABORT) != 0u;
        unsigned nc = in->n_cmds < IDA_PORT_MAX_CMDS ? in->n_cmds : IDA_PORT_MAX_CMDS;
        for (unsigned j = 0; j < nc; ++j) {
            if (in->cmds[j].type == IDA_CMD_FC && in->cmds[j].u.fc.op == IDA_FC_DISARM) {
                fcdisarm = true;
            }
            if (in->cmds[j].type == IDA_CMD_FC && in->cmds[j].u.fc.op == IDA_FC_ABORT) {
                fcabort = true;
            }
        }
        const ida_out_t *so = ka_out();
        if (fcdisarm) {
            /* FC_DISARM her tüpe ve her dönüş kodunda uygulanır: çağrı sonunda ARMED ya da
               PERMIT_OPEN yok (W5-K7, W5-K8, W5-K23, G-Ç06, G-Ç19, G-Ç32). */
            KA_DOGRULA(so->fc.state != IDA_FCS_ARMED && so->fc.state != IDA_FCS_PERMIT_OPEN,
                       "FC_DISARM çağrısının sonunda fc.state %s (değişmez (e); W5-K7, G-Ç06)", ka_durum_adi(so->fc.state));
        } else if (fcabort) {
            /* FC_ABORT atış dizisini iptal eder, silahı indirmez (W5-K23): çağrı sonunda
               PERMIT_OPEN yok; önceden silahlıysa ARMED kalabilir. FIRE_PENDING yalnız daha önce
               çıkmış bir FIRE'ın sonucu hâlâ bekleniyorsa (A4 §3.16 tanımı; çıkmış FIRE geri
               alınamaz; W5-K23'ün iptal ettiği bekleyen FIRE isteğidir). */
            KA_DOGRULA(so->fc.state != IDA_FCS_PERMIT_OPEN &&
                           (so->fc.state != IDA_FCS_FIRE_PENDING || ka_onceki_fire_sonucu_bekleniyor()),
                       "FC_ABORT çağrısının sonunda fc.state %s (değişmez (e); W5-K23, W5-K7, G-Ç06)",
                       ka_durum_adi(so->fc.state));
        }
        if (fcdisarm || fcabort) {
            /* İkisi de bütün tüplere uygulanır: o çağrıda genişletici olay yok, çekirdekçe açık
               sayılan her izin aynı çağrıda kapatılır ve çağrı sonunda açık sayılan izin kalmaz
               (W5-K8, W5-K23, G-Ç06, G-Ç19). Sonraki çağrılarda yeni bir kabul edilmiş FC_FIRE
               olmadan SAFETY_OPEN/FIRE olmaması düzeneğin yetki modelinde denetlenir (G-Ç21). */
            KA_DOGRULA(!ka_genisletici_olay_var(so), "FC_DISARM/FC_ABORT çağrısında genişletici olay (dönüş %d; W5-K23, G-Ç06)",
                       (int)rc);
            for (unsigned t = 0; t < IDA_PORT_N_TUBE; ++t) {
                KA_DOGRULA(!izin_once[t] || ka_kapanma_var(so, (uint8_t)t),
                           "FC_DISARM/FC_ABORT çağrısında açık izinli tüp %u için SAFETY_CLOSE yok (dönüş %d; değişmez (e); "
                           "W5-K8, W5-K23, G-Ç19)",
                           t, (int)rc);
                KA_DOGRULA(!ka_model_izin_acik((uint8_t)t, in->now_w.ns),
                           "FC_DISARM/FC_ABORT çağrısının sonunda tüp %u'nun izni çekirdekçe açık sayılıyor (dönüş %d; W5-K23)", t,
                           (int)rc);
            }
        }
        /* Kip mandalı: kabul edilen çağrının sonunda kip atış izni kümesinin ({GUIDED}; W5-K15)
           dışındadır, MODE biti kurulu (A4 §3.16, A4-K44, A4-K46). */
        if ((in->narrow.mask & IDA_NARROW_MODE) != 0u && (rc == IDA_RC_OK || rc == IDA_RC_IMU_REJECTED)) {
            KA_DOGRULA((ka_out()->fc.inhibit_mask & IDA_FCI_MODE) != 0u,
                       "kip mandalının (hedef %u) kabul edilen çağrısında MODE biti yok (değişmez (e); W5-K15, A4 §3.16)",
                       (unsigned)in->narrow.mode);
        }
    }
}

static void ozellik(int i)
{
    if (i < 0 || i >= (int)(sizeof PROFIL / sizeof PROFIL[0])) {
        ka_basarisiz(__LINE__, "profil %d", i);
    }
    const profil_t *p = &PROFIL[i];
    char on[160];
    kc_rng_kur(&R, p->tohum);
    g_kip = 0u;
    g_kip_dene = 0u;
    memset(g_son_id, 0, sizeof g_son_id);
    ka_kapsam_t top;
    memset(&top, 0, sizeof top);
    hazirla();
    for (int k = 0; k < ADIM_SAYISI; ++k) {
        (void)snprintf(on, sizeof on, "özellik %s, tohum 0x%016llx, adım %d", p->ad, (unsigned long long)p->tohum, k);
        ka_baglam_on(on);
        if (k > 0 && k % YENIDEN_KUR == 0) {
            const ka_kapsam_t *c = ka_kapsam();
            for (int s = 0; s < 6; ++s) {
                top.durum[s] += c->durum[s];
            }
            top.open += c->open;
            top.fire += c->fire;
            top.fc_fire_kabul += c->fc_fire_kabul;
            hazirla();
        }
        adim(p);
    }
    const ka_kapsam_t *c = ka_kapsam();
    for (int s = 0; s < 6; ++s) {
        top.durum[s] += c->durum[s];
    }
    top.open += c->open;
    top.fire += c->fire;
    top.fc_fire_kabul += c->fc_fire_kabul;
    ka_baglam_on(NULL);
    char msj[256];
    (void)snprintf(msj, sizeof msj,
                   "%s kapsamı: SAFE %llu, ARMED %llu, PERMIT_OPEN %llu, FIRE_PENDING %llu, TEST %llu, FAULT_LATCHED %llu, "
                   "SAFETY_OPEN %llu, FIRE %llu, kabul edilen FC_FIRE %llu",
                   p->ad, (unsigned long long)top.durum[0], (unsigned long long)top.durum[1], (unsigned long long)top.durum[2],
                   (unsigned long long)top.durum[3], (unsigned long long)top.durum[4], (unsigned long long)top.durum[5],
                   (unsigned long long)top.open, (unsigned long long)top.fire, (unsigned long long)top.fc_fire_kabul);
    TEST_MESSAGE(msj);
    KA_DOGRULA(top.durum[IDA_FCS_ARMED] >= 200u && top.durum[IDA_FCS_PERMIT_OPEN] >= 10u && top.durum[IDA_FCS_TEST] >= 10u &&
                   top.durum[IDA_FCS_FAULT_LATCHED] >= 10u && top.fire >= 10u && top.open >= 20u,
                   "kapsam yetersiz: dizi emniyet zincirinin durumlarına girmedi, değişmez sınanmadı (%s)", msj);
}

static void t_a(void) { ozellik(0); }
static void t_b(void) { ozellik(1); }
static void t_c(void) { ozellik(2); }
static void t_d(void) { ozellik(3); }
static void t_e(void) { ozellik(4); }
static void t_f(void) { ozellik(5); }
static void t_g(void) { ozellik(6); }

static const ka_test_t TESTLER[] = {
    {"a_acil_durdurma", t_a}, {"b_fire_zinciri", t_b}, {"c_kilit", t_c},     {"d_test", t_d},
    {"e_daraltma", t_e},      {"f_bilinmeyen", t_f},   {"g_parametre", t_g},
};

int main(int argc, char **argv)
{
    return ka_main(argc, argv, __FILE__, TESTLER, sizeof TESTLER / sizeof TESTLER[0]);
}
