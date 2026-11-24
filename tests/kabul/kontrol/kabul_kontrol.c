/*
 * tests/kabul/kontrol — ortak düzenek: testin kendi etkin acil durdurma modeli, her çağrıda nötr
 * çıktı denetimi, operatör ve hazırlık. Açıklama başlıkta (kabul_kontrol.h).
 */
#include "kabul_kontrol.h"

#include <stdio.h>
#include <string.h>

#include "unity.h"

/* ---- Model --------------------------------------------------------------------------------- */

typedef struct {
    bool E;               /* etkin değer (bayatlık hariç) kısıtlayan                           */
    kk_neden_t E_neden;
    bool rx_var;          /* init'ten beri (olası) kabul edilmiş inst 0 örneği var            */
    int64_t rx_son;       /* son (olası) kabul edilen inst 0 örneğinin t_rx'i                  */
    bool tm_var;          /* ölçüm zamanı izi (SESSION_START sıfırlar; A4 3.11 (b3))           */
    int64_t tm_son;       /* en büyük kabul edilen t_meas                                       */
    int64_t adv_rx;       /* t_meas'in son ilerlediği örneğin t_rx'i                            */
    int64_t ref;          /* init / son SESSION_START anı (gevşek okuma)                       */
    bool kis;             /* son çağrının sonunda kesin kısıtlayan                              */
    kk_neden_t neden;
} kk_model_t;

static kk_model_t M;
static int64_t g_T;
static bool g_denetim = true;
static kk_sayac_t S;
static ida_in_t *g_in;
static bool g_onceki_notr_disi;

kk_operator_t kk_op;

const char *kk_neden_adi(int n)
{
    static const char *const ad[KK_N_SAYI] = {"yok",         "init",          "basılı (inst 0)", "VALID = 0 örnek",
                                              "inst ≠ 0 örnek", "reddedilen çağrı", "SESSION_END",    "bağlantı olayı (W5-K20 (2))",
                                              "bayat: alım (W5-K4 (i))", "bayat: ölçüm zamanı (W5-K4 (ii))"};
    return n >= 0 && n < (int)KK_N_SAYI ? ad[n] : "?";
}

bool kk_model_kisitlayan(void)
{
    return M.kis;
}

kk_neden_t kk_model_neden(void)
{
    return M.neden;
}

int64_t kk_t_bayat(void)
{
    return g_T;
}

void kk_denetim_ac(bool acik)
{
    g_denetim = acik;
}

const kk_sayac_t *kk_sayac(void)
{
    return &S;
}

static void model_sifirla(void)
{
    memset(&M, 0, sizeof M);
    M.E = true;
    M.E_neden = KK_N_INIT;
    M.ref = KC_T0;
    M.kis = true;
    M.neden = KK_N_INIT;
    memset(&S, 0, sizeof S);
    g_onceki_notr_disi = false;
    g_T = ka_t_emn_bayat();
}

static bool emniyet_yapisal(const ida_sample_t *s)
{
    return s->hdr.type == IDA_SENS_SAFETY && s->hdr.inst < IDA_PORT_MAX_INST;
}

/* inst 0 örneği kabul EDİLEBİLİR mi (gevşek: sıra numarası ve eşit anahtar denetimi yok; bu
   yalnız modeli daha taze ve daha gevşek yapar). A4-K33'ün biçim koşulları. */
static bool inst0_kabul_edilebilir(const ida_in_t *in, const ida_sample_t *s)
{
    return emniyet_yapisal(s) && s->hdr.inst == 0u && (s->hdr.flags & IDA_HF_VALID) != 0u && s->u.safety.estop <= 1u &&
           (s->u.safety.tube[0] & 0xF0u) == 0u && (s->u.safety.tube[1] & 0xF0u) == 0u && s->hdr.window_ns == 0u &&
           s->hdr.t_rx.ns <= in->now_w.ns && s->hdr.t_meas.ns >= 0 && (!in->tf.present || s->hdr.t_meas.ns <= in->tf.t.ns);
}

static void model_guncelle(const ida_in_t *in, ida_rc_t rc)
{
    bool kabul = rc == IDA_RC_OK || rc == IDA_RC_IMU_REJECTED;
    int64_t now = in->now_w.ns;
    unsigned n = in->n_samples < IDA_PORT_MAX_SAMPLES ? in->n_samples : IDA_PORT_MAX_SAMPLES;
    if (in->plat.link_event == IDA_LINKEV_SESSION_START) {
        M.ref = now;
        M.tm_var = false;
    }
    kk_neden_t kis = KK_N_YOK;
    bool gev = false;
    for (unsigned i = 0; i < n; ++i) {
        const ida_sample_t *s = &in->samples[i];
        if (!emniyet_yapisal(s)) {
            continue;
        }
        if (s->u.safety.estop != 0u && kis == KK_N_YOK) {
            if (!kabul) {
                kis = KK_N_RED;
            } else if (s->hdr.inst != 0u) {
                kis = KK_N_INST;
            } else if ((s->hdr.flags & IDA_HF_VALID) == 0u) {
                kis = KK_N_GECERSIZ;
            } else {
                kis = KK_N_BASILI;
            }
        }
        if (kabul && inst0_kabul_edilebilir(in, s)) {
            if (!M.rx_var || s->hdr.t_rx.ns > M.rx_son) {
                M.rx_son = s->hdr.t_rx.ns;
            }
            M.rx_var = true;
            if (!M.tm_var || s->hdr.t_meas.ns > M.tm_son) {
                M.tm_son = s->hdr.t_meas.ns;
                M.adv_rx = s->hdr.t_rx.ns;
                M.tm_var = true;
            }
            if (s->u.safety.estop == 0u) {
                gev = true;
            }
        }
    }
    if (in->plat.link_event == IDA_LINKEV_SESSION_END) {
        M.E = true;
        M.E_neden = KK_N_OTURUM_SONU;
    } else if (kis != KK_N_YOK) {
        M.E = true;
        M.E_neden = kis;
    } else if (gev) {
        if (in->plat.link_event != IDA_LINKEV_NONE) {
            if (M.E) {
                M.E_neden = KK_N_BAGLANTI; /* W5-K20 (2): bu çağrıda gevşemez */
            }
        } else {
            M.E = false;
            M.E_neden = KK_N_YOK;
        }
    }
    bool bayat_alim = false;
    bool bayat_olcum = false;
    if (M.rx_var) {
        int64_t a = M.rx_son > M.ref ? M.rx_son : M.ref;
        int64_t b = M.adv_rx > M.ref ? M.adv_rx : M.ref;
        bayat_alim = now - a > g_T;
        bayat_olcum = now - b > g_T;
    }
    M.kis = M.E || bayat_alim || bayat_olcum;
    if (M.E) {
        M.neden = M.E_neden;
    } else if (bayat_alim) {
        M.neden = KK_N_BAYAT_ALIM;
    } else if (bayat_olcum) {
        M.neden = KK_N_BAYAT_OLCUM;
    } else {
        M.neden = KK_N_YOK;
    }
}

/* ---- Çıktı ----------------------------------------------------------------------------------- */

float kk_mutlak(float x)
{
    return x < 0.0f ? -x : x;
}

bool kk_notr(const ida_out_t *o)
{
    for (unsigned i = 0; i < IDA_PORT_N_THR; ++i) {
        if (o->thr_cmd[i] != 0.0f) {
            return false;
        }
    }
    return true;
}

float kk_sapma(const ida_out_t *o)
{
    float m = 0.0f;
    for (unsigned i = 0; i < IDA_PORT_N_THR; ++i) {
        float a = kk_mutlak(o->thr_cmd[i]);
        if (a > m) {
            m = a;
        }
    }
    return m;
}

const char *kk_thr_yazi(const ida_out_t *o)
{
    static char b[256];
    (void)snprintf(b, sizeof b,
                   "thr_valid %d, is_safe %d, armed %d, kip %u, fs %u, thr [%.4f %.4f %.4f %.4f %.4f %.4f %.4f %.4f]",
                   (int)o->thr_valid, (int)o->is_safe, (int)o->armed, (unsigned)o->status.mode, (unsigned)o->fs_level,
                   (double)o->thr_cmd[0], (double)o->thr_cmd[1], (double)o->thr_cmd[2], (double)o->thr_cmd[3],
                   (double)o->thr_cmd[4], (double)o->thr_cmd[5], (double)o->thr_cmd[6], (double)o->thr_cmd[7]);
    return b;
}

void kk_notr_bekle(int satir, const char *ne)
{
    const ida_out_t *o = ka_out();
    if (!kk_notr(o)) {
        ka_basarisiz(satir, "%s: çıktı nötr değil (%s); sekiz thr_cmd tam 0.0f bekleniyordu (W5-kontrol-acil Kural 1; A4-K55)",
                     ne, kk_thr_yazi(o));
    }
}

/* ---- Kurulum ve çağrı ----------------------------------------------------------------------- */

void kk_kur(void)
{
    ka_kur(1);
    g_denetim = true;
    g_in = NULL;
    kk_operator_varsayilan();
    model_sifirla();
}

ida_in_t *kk_girdi(void)
{
    g_in = ka_girdi();
    return g_in;
}

ida_in_t *kk_girdi_seq(uint32_t seq)
{
    g_in = ka_girdi_seq(seq);
    return g_in;
}

ida_in_t *kk_girdi_oturum_sonu(void)
{
    g_in = ka_girdi_oturum_sonu();
    return g_in;
}

ida_rc_t kk_cagir(void)
{
    KA_DOGRULA(g_in != NULL, "kk_cagir: önce kk_girdi");
    const ida_in_t *in = g_in;
    g_in = NULL;
    bool once = M.kis;
    ida_rc_t rc = ka_cagir();
    model_guncelle(in, rc);
    const ida_out_t *o = ka_out();
    bool notr_disi = !kk_notr(o);
    S.cagri++;
    if (notr_disi) {
        S.notr_disi++;
        if (!M.kis) {
            S.notr_disi_gevsek++;
        }
    }
    if (M.kis) {
        S.kisitlayan++;
        S.kisit_neden[M.neden]++;
        if (!once) {
            S.gecis++;
            S.gecis_neden[M.neden]++;
            if (g_onceki_notr_disi) {
                S.gecis_notr_disi++;
            }
        }
    } else if (once) {
        S.kalkis++;
    }
    if (g_denetim && M.kis) {
        KA_DOGRULA(!notr_disi,
                   "etkin acil durdurma kesin kısıtlayan (neden: %s) iken çıktı nötr değil: %s; dönüş %d; sekiz thr_cmd tam "
                   "0.0f bekleniyordu (W5-kontrol-acil Kural 1; A4-K55, A4-K116, W5-K4, W5-K20)",
                   kk_neden_adi((int)M.neden), kk_thr_yazi(o), (int)rc);
    }
    g_onceki_notr_disi = notr_disi;
    return rc;
}

void kk_operator_varsayilan(void)
{
    memset(&kk_op, 0, sizeof kk_op);
    kk_op.acik = true;
    kk_op.sp.mask = (uint8_t)(IDA_SP_POS_NE | IDA_SP_DEPTH | IDA_SP_YAW);
    kk_op.sp.pos_ne[0] = 40.0f;
    kk_op.sp.pos_ne[1] = -30.0f;
    kk_op.sp.depth = 8.0f;
    kk_op.sp.yaw = 1.5f;
    kk_op.eksen[0] = 0.6f;
    kk_op.eksen[1] = -0.3f;
    kk_op.eksen[2] = 0.4f;
    kk_op.eksen[3] = 0.2f;
    kk_op.eksen[4] = -0.2f;
    kk_op.eksen[5] = 0.5f;
}

void kk_operator_ekle(ida_in_t *in)
{
    if (!kk_op.acik || ka_cagri_no() == 0u || in->n_cmds >= IDA_PORT_MAX_CMDS || !in->tf.present) {
        return;
    }
    uint8_t kip = ka_out()->status.mode;
    if (kip == IDA_MODE_GUIDED) {
        ida_cmd_t *c = kc_komut_ekle(in, ka_yeni_id(), (uint8_t)IDA_CMD_GUIDED_SP);
        c->u.guided = kk_op.sp;
    } else if (kip == IDA_MODE_MANUAL || kip == IDA_MODE_STABILIZE || kip == IDA_MODE_DEPTH_HOLD) {
        ida_cmd_t *c = kc_komut_ekle(in, ka_yeni_id(), (uint8_t)IDA_CMD_MANUAL);
        for (unsigned j = 0; j < 6u; ++j) {
            c->u.manual.axes[j] = kk_op.eksen[j];
        }
    }
}

ida_rc_t kk_adim(void)
{
    ida_in_t *in = kk_girdi();
    kk_operator_ekle(in);
    return kk_cagir();
}

void kk_adimlar(int n)
{
    for (int i = 0; i < n; ++i) {
        (void)kk_adim();
    }
}

/* ---- Hazırlık ------------------------------------------------------------------------------- */

void kk_oturum_ac(void)
{
    ida_in_t *in = kk_girdi();
    in->plat.link_event = (uint8_t)IDA_LINKEV_SESSION_START;
    (void)kk_cagir();
    for (int k = 0; k < 10 * KK_SN; ++k) {
        const ida_out_t *o = ka_out();
        if (o->status.mode != IDA_MODE_INIT && o->nav_valid && o->nav.est_level != IDA_EST_INIT) {
            return;
        }
        (void)kk_adim();
    }
    ka_basarisiz(__LINE__, "hazırlık: 10 s'de kip ya da kestirim INIT'ten çıkmadı (kip %u, est_level %u)",
                 (unsigned)ka_out()->status.mode, (unsigned)ka_out()->nav.est_level);
}

bool kk_silahla(int deneme)
{
    for (int k = 0; k < deneme; ++k) {
        ida_in_t *in = kk_girdi();
        (void)ka_komut(in, (uint8_t)IDA_CMD_ARM);
        (void)kk_cagir();
        if (ka_out()->armed) {
            return true;
        }
        kk_adimlar(10);
    }
    return ka_out()->armed;
}

bool kk_kip(uint8_t kip)
{
    for (int k = 0; k < 200; ++k) {
        if (ka_out()->status.mode == kip) {
            return true;
        }
        ida_in_t *in = kk_girdi();
        if (kip == IDA_MODE_GUIDED || kip == IDA_MODE_AUTO) {
            kc_komut_ekle(in, ka_yeni_id(), (uint8_t)IDA_CMD_SET_MODE)->u.set_mode.mode = kip;
        } else {
            (void)ka_mandal(in, (uint8_t)IDA_NARROW_MODE);
            in->narrow.mode = kip;
        }
        (void)kk_cagir();
        kk_adimlar(2);
        if (ka_out()->status.mode == kip) {
            return true;
        }
        kk_adimlar(7);
    }
    return ka_out()->status.mode == kip;
}

void kk_hazir(uint8_t kip)
{
    kk_kur();
    kk_oturum_ac();
    KA_DOGRULA(kk_silahla(20), "ön koşul: araç silahlanamadı (est_level %u, fs_level %u, fs_reasons 0x%lx)",
               (unsigned)ka_out()->nav.est_level, (unsigned)ka_out()->fs_level, (unsigned long)ka_out()->status.fs_reasons);
    KA_DOGRULA(kk_kip(kip), "ön koşul: kip %u'ya geçilemedi (kip %u, est_level %u)", (unsigned)kip,
               (unsigned)ka_out()->status.mode, (unsigned)ka_out()->nav.est_level);
}

/* ---- Parametre ------------------------------------------------------------------------------- */

bool kk_param_bul(const char *ad, ida_param_desc_t *d)
{
    uint16_t id = 0;
    memset(d, 0, sizeof *d);
    if (ida_core_param_find(ad, &id) != IDA_RC_OK) {
        return false;
    }
    uint16_t n = ida_core_param_count();
    for (uint16_t i = 0; i < n; ++i) {
        if (ida_core_param_desc(i, d) == IDA_RC_OK && d->id == id) {
            return true;
        }
    }
    memset(d, 0, sizeof *d);
    return false;
}

int64_t kk_param_tamsayi(const ida_param_value_t *v)
{
    switch (v->type) {
    case IDA_PT_I32:
        return (int64_t)v->v.i32;
    case IDA_PT_U32:
        return (int64_t)v->v.u32;
    case IDA_PT_F32: {
        float f = v->v.f32;
        return (int64_t)(f < 0.0f ? f - 0.5f : f + 0.5f);
    }
    default:
        return INT64_MIN;
    }
}

ida_param_value_t kk_param_deger(const ida_param_desc_t *d, int64_t x)
{
    ida_param_value_t v;
    memset(&v, 0, sizeof v);
    v.id = d->id;
    v.type = d->type;
    if (d->type == IDA_PT_I32) {
        v.v.i32 = (int32_t)x;
    } else if (d->type == IDA_PT_F32) {
        v.v.f32 = (float)x;
    } else {
        v.v.u32 = (uint32_t)x;
    }
    return v;
}

int64_t kk_param_oku(const ida_param_desc_t *d)
{
    ida_param_value_t v;
    memset(&v, 0, sizeof v);
    KA_DOGRULA(ida_core_param_get(kc_core(0), d->id, &v) == IDA_RC_OK, "param_get(%u) OK dönmedi", (unsigned)d->id);
    if (v.type == 0u) {
        v.type = d->type;
    }
    return kk_param_tamsayi(&v);
}

void kk_param_gonder(const ida_param_desc_t *d, int64_t x, uint32_t *set_id, uint32_t *commit_id)
{
    ida_in_t *in = kk_girdi();
    uint32_t sid = ka_yeni_id();
    ida_cmd_t *c = kc_komut_ekle(in, sid, (uint8_t)IDA_CMD_PARAM_SET);
    c->u.param = kk_param_deger(d, x);
    uint32_t cid = ka_komut(in, (uint8_t)IDA_CMD_PARAM_COMMIT);
    (void)kk_cagir();
    if (set_id != NULL) {
        *set_id = sid;
    }
    if (commit_id != NULL) {
        *commit_id = cid;
    }
}
