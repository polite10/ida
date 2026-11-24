/*
 * test_gudum_oto.c — W5-gorev-yurutucu kabul 4 (güdüm: sentetik araç modeliyle düz hat, köşe
 * dönüşü ve akıntı altında yol takibi hatası; derinlik referansı hız/ivme sınırını aşmaz),
 * AUTO isteğinin kontrol kanallarına yazımı, en az irtifa kırpması ve md. 6 (GUIDED'da
 * GUD_SP_ASIM_MS yeni hedef yoksa son hedefte kalma).
 *
 * Sentetik araç (testin içinde; basit kinematik): yön kontrolün REF_YAW_RATE'i (0,35 rad/s) ile
 * güdümün yönüne döner; suya göre hız güdümün NED hız isteğini 1 s zaman sabitiyle izler
 * (LIM_V 1 m/s); yere göre hız = suya göre hız + akıntı. Hız döngüsünün integrali modellenmez:
 * akıntı yol takibine kalıcı hata olarak yansır (kötümser durum); ILOS bunu giderir.
 * Sınırlar testin kendi seçimidir (TAHMİN; gerçek araçta ölçülmedi).
 */
#include <string.h>

#include "destek.h"
#include "gudum/gudum.h"
#include "mat/ida_math.h"
#include "unity.h"

void setUp(void);
void tearDown(void);
void setUp(void) {}
void tearDown(void) {}

#define DELTA 3.0f  /* GUD_LOS_DELTA varsayılanı */
#define SIGMA 0.2f  /* GUD_ILOS_SIGMA varsayılanı */
#define KP 0.3f     /* GUD_POS_KP varsayılanı */
#define DT 0.01f

static float mutlak(float x)
{
    return (x < 0.0f) ? -x : x;
}

/* ---- Derinlik referans modeli -------------------------------------------------------------- */

static void ref_dene(float hedef0, float hedef1, float w_max, float a_max, float dt)
{
    ida_gud_ref_t r = {0.5f, 0.0f};
    float hedef = hedef0;
    const float pay = 1.0e-5f;
    int64_t n = 0;
    const int64_t degis = (int64_t)(20.0f / dt); /* 20 s sonra hedef değişir (hareket hâlinde) */
    const int64_t son = (int64_t)(4000.0f / dt);
    bool vardi = false;
    float e_degis = 0.0f;
    float w_degis = 0.0f;
    for (; n < son; n++) {
        if (n == degis) {
            hedef = hedef1;
            e_degis = mutlak(hedef1 - r.d);
            w_degis = mutlak(r.w);
        }
        const float w0 = r.w;
        ida_gud_ref_adim(&r, hedef, w_max, a_max, dt);
        TEST_ASSERT_TRUE_MESSAGE(mutlak(r.w) <= (w_max * (1.0f + pay)) + 1.0e-7f, "hız sınırı");
        TEST_ASSERT_TRUE_MESSAGE(mutlak(r.w - w0) <= (a_max * dt * (1.0f + pay)) + 1.0e-7f,
                                 "ivme sınırı");
        if ((n > degis) && (r.d == hedef) && (r.w == 0.0f)) {
            vardi = true;
            break;
        }
    }
    TEST_ASSERT_TRUE_MESSAGE(vardi, "referans hedefe varır ve durur");
    /* Varış süresi: hedef değiştiğinde ters yönde hareket varsa önce durma (w/a, w²/2a yol),
       sonra hız profili (|e|/w + w/a); ayrık adım payıyla. */
    const float sure = (float)(n - degis) * dt;
    const float ust = ((e_degis + (w_degis * w_degis / a_max)) / w_max) + (w_degis / a_max) +
                      (2.0f * w_max / a_max) + 1.0f + (10.0f * dt);
    TEST_ASSERT_TRUE_MESSAGE(sure <= ust, "varış süresi");
}

static void test_ref_modeli_hiz_ivme_siniri(void)
{
    ds_rastgele_t rg = {0x1DA5EEDu};
    static const float DTS[3] = {0.005f, 0.01f, 0.1f};
    for (uint32_t i = 0u; i < 24u; i++) {
        const float h0 = (float)(ds_rastgele(&rg) % 2000u) * 0.01f;
        const float h1 = (float)(ds_rastgele(&rg) % 2000u) * 0.01f;
        const float w = 0.05f + ((float)(ds_rastgele(&rg) % 20u) * 0.01f);
        const float a = 0.01f + ((float)(ds_rastgele(&rg) % 20u) * 0.01f);
        ref_dene(h0, h1, w, a, DTS[i % 3u]);
    }
    /* dt ≤ 0 ya da sınır ≤ 0: değişmez. */
    ida_gud_ref_t r = {1.0f, 0.1f};
    ida_gud_ref_adim(&r, 5.0f, 0.2f, 0.05f, 0.0f);
    ida_gud_ref_adim(&r, 5.0f, 0.0f, 0.05f, 0.01f);
    TEST_ASSERT_EQUAL_FLOAT(1.0f, r.d);
    TEST_ASSERT_EQUAL_FLOAT(0.1f, r.w);
}

/* ---- Sentetik araçla yol takibi ------------------------------------------------------------ */

typedef struct {
    float p[2];
    float v[2];
    float psi;
    float y_int;
} arac_t;

static void arac_adim(arac_t *a, const ida_gor_hedef_t *h, const float akinti[2], float sigma)
{
    float hiz[2] = {0.0f, 0.0f};
    float yon = a->psi;
    ida_gud_oto_yatay(h, a->p, a->psi, a->y_int, DELTA, sigma, KP, hiz, &yon);
    float r = 0.0f;
    float y = 0.0f;
    float s = 0.0f;
    if ((h->yatay == (uint8_t)IDA_GOR_HY_YOL) &&
        ida_gud_ilos(h->bas_ne, h->hedef_ne, a->p, DELTA, sigma, a->y_int, &r, &y, &s)) {
        ida_gud_ilos_integral(&a->y_int, y, DELTA, sigma, DT);
    }
    const float dpsi = ida_aci_sar(yon - a->psi);
    const float lim = 0.35f * DT;
    float adim = dpsi;
    if (adim > lim) {
        adim = lim;
    } else if (adim < -lim) {
        adim = -lim;
    } else {
        /* sınır içinde */
    }
    a->psi = ida_aci_sar(a->psi + adim);
    for (uint32_t i = 0u; i < 2u; i++) {
        float k = hiz[i];
        k = (k > 1.0f) ? 1.0f : ((k < -1.0f) ? -1.0f : k);
        a->v[i] += (k - a->v[i]) * DT;
        a->p[i] += (a->v[i] + akinti[i]) * DT;
    }
}

static ida_gor_hedef_t bacak(float n0, float e0, float n1, float e1, float hiz)
{
    ida_gor_hedef_t h;
    (void)memset(&h, 0, sizeof(h));
    h.yatay = (uint8_t)IDA_GOR_HY_YOL;
    h.bas_ne[0] = n0;
    h.bas_ne[1] = e0;
    h.hedef_ne[0] = n1;
    h.hedef_ne[1] = e1;
    h.hiz = hiz;
    h.dikey_hiz = 0.25f;
    return h;
}

static float capraz(const ida_gor_hedef_t *h, const float p[2])
{
    float r = 0.0f;
    float y = 0.0f;
    float s = 0.0f;
    (void)ida_gud_ilos(h->bas_ne, h->hedef_ne, p, DELTA, SIGMA, 0.0f, &r, &y, &s);
    return y;
}

/* Kuzeye 200 m'lik düz hat, 3 m doğudan başlangıç, 0,5 m/s. Sentetik modelde (σ 0,2, Δ 3 m)
   karşı tarafa aşma 0,545 m, 60 s'den sonra |y| 0,008 m çıktı; sınırlar 0,6 m ve 0,05 m. */
static void test_duz_hat(void)
{
    const ida_gor_hedef_t h = bacak(0.0f, 0.0f, 200.0f, 0.0f, 0.5f);
    arac_t a;
    (void)memset(&a, 0, sizeof(a));
    a.p[1] = 3.0f;
    const float akinti[2] = {0.0f, 0.0f};
    float asma = 0.0f;
    float son = 0.0f;
    for (int i = 0; i < 12000; i++) { /* 120 s */
        arac_adim(&a, &h, akinti, SIGMA);
        const float y = capraz(&h, a.p);
        if (-y > asma) {
            asma = -y;
        }
        if (i >= 6000) {
            son = (mutlak(y) > son) ? mutlak(y) : son;
        }
    }
    TEST_ASSERT_TRUE_MESSAGE(son <= 0.05f, "düz hat: 60 s'den sonra çapraz hata");
    TEST_ASSERT_TRUE_MESSAGE(asma <= 0.6f, "düz hat: karşı tarafa aşma");
    TEST_ASSERT_TRUE(a.p[0] > 40.0f); /* ilerledi */
}

/* Köşe: (0,0)→(20,0), kabul yarıçapı 1 m ya da bacak sonu geçilince (20,0)→(20,20); 0,5 m/s.
   Ölçülen (sentetik): ikinci bacakta en büyük |y| ≤ 1,5 m; 40 s sonra ≤ 0,05 m. */
static void test_kose_donusu(void)
{
    const ida_gor_hedef_t h1 = bacak(0.0f, 0.0f, 20.0f, 0.0f, 0.5f);
    const ida_gor_hedef_t h2 = bacak(20.0f, 0.0f, 20.0f, 20.0f, 0.5f);
    arac_t a;
    (void)memset(&a, 0, sizeof(a));
    const float akinti[2] = {0.0f, 0.0f};
    int i = 0;
    for (; i < 20000; i++) {
        arac_adim(&a, &h1, akinti, SIGMA);
        const float dn = a.p[0] - 20.0f;
        const float de = a.p[1];
        if (((dn * dn) + (de * de) <= 1.0f) || (a.p[0] >= 20.0f)) {
            break;
        }
    }
    TEST_ASSERT_TRUE(i < 20000);
    a.y_int = 0.0f; /* yeni bacak: güdüm integrali sıfırlar (hedef kuşağı) */
    float en_buyuk = 0.0f;
    float son = 0.0f;
    for (int k = 0; k < 6000; k++) { /* 60 s */
        arac_adim(&a, &h2, akinti, SIGMA);
        const float y = mutlak(capraz(&h2, a.p));
        en_buyuk = (y > en_buyuk) ? y : en_buyuk;
        if (k >= 4000) {
            son = (y > son) ? y : son;
        }
    }
    TEST_ASSERT_TRUE_MESSAGE(en_buyuk <= 1.5f, "köşe: ikinci bacakta en büyük çapraz hata");
    TEST_ASSERT_TRUE_MESSAGE(son <= 0.05f, "köşe: 40 s sonra çapraz hata");
}

/* Akıntı: yola dik 0,15 m/s (doğuya), yol kuzeye, 0,5 m/s. LOS (σ = 0) kalıcı hata bırakır
   (kuramsal Δ·tan(asin(c/u)) ≈ 0,94 m); ILOS (σ = 0,2) 200 s sonra |y| ≤ 0,1 m. */
static void test_akinti_altinda(void)
{
    const ida_gor_hedef_t h = bacak(0.0f, 0.0f, 1000.0f, 0.0f, 0.5f);
    const float akinti[2] = {0.0f, 0.15f};
    float son_ilos = 0.0f;
    float son_los = 0.0f;
    for (uint32_t tur = 0u; tur < 2u; tur++) {
        arac_t a;
        (void)memset(&a, 0, sizeof(a));
        const float sigma = (tur == 0u) ? SIGMA : 0.0f;
        float son = 0.0f;
        for (int i = 0; i < 30000; i++) { /* 300 s */
            arac_adim(&a, &h, akinti, sigma);
            if (i >= 20000) {
                const float y = mutlak(capraz(&h, a.p));
                son = (y > son) ? y : son;
            }
        }
        if (tur == 0u) {
            son_ilos = son;
        } else {
            son_los = son;
        }
    }
    TEST_ASSERT_TRUE_MESSAGE(son_ilos <= 0.1f, "akıntı: ILOS kalıcı çapraz hata");
    TEST_ASSERT_TRUE_MESSAGE(son_los >= 0.5f, "akıntı: LOS kalıcı hata bırakır (karşılaştırma)");
}

/* Konum tutma: 4 m uzaktaki noktaya P yasasıyla yaklaşır ve orada kalır. */
static void test_konum_tutma(void)
{
    ida_gor_hedef_t h;
    (void)memset(&h, 0, sizeof(h));
    h.yatay = (uint8_t)IDA_GOR_HY_TUT;
    h.hedef_ne[0] = 3.0f;
    h.hedef_ne[1] = -2.0f;
    h.yon = 1.0f;
    arac_t a;
    (void)memset(&a, 0, sizeof(a));
    const float akinti[2] = {0.0f, 0.0f};
    for (int i = 0; i < 6000; i++) {
        arac_adim(&a, &h, akinti, SIGMA);
    }
    TEST_ASSERT_FLOAT_WITHIN(0.05f, 3.0f, a.p[0]);
    TEST_ASSERT_FLOAT_WITHIN(0.05f, -2.0f, a.p[1]);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 1.0f, a.psi);
}

/* ---- Çekirdek: AUTO isteği ----------------------------------------------------------------- */

static ida_core_t C;

static void auto_kur(void)
{
    TEST_ASSERT_EQUAL(IDA_RC_OK, ds_kur(&C));
    C.emn.armed = true;
    C.emn.mode = (uint8_t)IDA_MODE_AUTO;
    C.emn.hedef_iptal = 0u;
    C.gor.son_kip = (uint8_t)IDA_MODE_AUTO;
    C.gor.fs_level = (uint8_t)IDA_FS_NONE;
    C.gor.mission_state = (uint8_t)IDA_MS_RUNNING;
    C.gor.gecerlilik = (uint8_t)(IDA_KES_G_TUTUM | IDA_KES_G_YON | IDA_KES_G_DERINLIK |
                                 IDA_KES_G_DIKEY_HIZ | IDA_KES_G_YATAY_HIZ | IDA_KES_G_KONUM);
    C.gor.nav_tutum_var = true;
    C.gor.nav_yon_var = true;
    C.gor.nav_derinlik_var = true;
    C.gor.nav_konum_var = true;
    C.gor.nav_derinlik = 1.0f;
    C.gor.yuzeyde = false;
    C.gor.nav_yon = 0.0f;
    C.gor.nav_ne[0] = 0.0f;
    C.gor.nav_ne[1] = 1.0f;
    C.gor.gv.hedef = bacak(0.0f, 0.0f, 10.0f, 0.0f, 0.5f);
    C.gor.gv.hedef.dikey = (uint8_t)IDA_GOR_HD_DERINLIK;
    C.gor.gv.hedef.hedef_d = 2.0f;
    C.gor.gv.hedef.dikey_hiz = 0.2f;
    C.gor.gv.hedef.kusak = 7u;
    C.cer.tf_t_ns = 1000 * DS_MS;
}

static void test_auto_istegi_kanallar(void)
{
    auto_kur();
    ida_gud_adim(&C.gud, &C); /* referans ölçülen derinlikten kurulur */
    TEST_ASSERT_TRUE(C.gud.oto_var);
    TEST_ASSERT_EQUAL_FLOAT(1.0f, C.gud.ref.d);
    /* 10 s, 10 ms adımla: referans 0,2 m/s'yi, 0,05 m/s²'yi aşmaz. */
    float d0 = C.gud.ref.d;
    float w0 = 0.0f;
    for (int i = 0; i < 1000; i++) {
        C.cer.tf_t_ns += 10 * DS_MS;
        ida_gud_adim(&C.gud, &C);
        const float w = (C.gud.ref.d - d0) / 0.01f;
        TEST_ASSERT_TRUE(w <= 0.2f + 1.0e-3f);
        TEST_ASSERT_TRUE(mutlak(w - w0) <= (0.05f * 0.01f) + 1.0e-3f);
        d0 = C.gud.ref.d;
        w0 = w;
    }
    TEST_ASSERT_TRUE(C.gud.ref.d > 1.5f);
    ida_kon_istek_t i;
    ida_gud_kon_istegi(&C.gud, &C, &i);
    TEST_ASSERT_EQUAL_UINT8(IDA_KON_KIP_DALIS, i.kip);
    TEST_ASSERT_EQUAL_UINT8(IDA_KON_DIKEY_DERINLIK, i.dikey);
    TEST_ASSERT_EQUAL_FLOAT(C.gud.ref.d, i.derinlik);
    TEST_ASSERT_EQUAL_UINT8(IDA_KON_YON_ACI, i.yon_kanali);
    TEST_ASSERT_TRUE(i.yon < 0.0f); /* yolun doğusunda: batıya (sola) döner */
    TEST_ASSERT_EQUAL_UINT8(IDA_KON_YATAY_HIZ, i.yatay);
    TEST_ASSERT_TRUE(i.hiz[0] > 0.3f);
    TEST_ASSERT_TRUE(i.hiz[1] < 0.0f);
    /* Yüzey hedefi, yüzeyde: yüzey kipi (dikey kapalı). */
    C.gor.gv.hedef.dikey = (uint8_t)IDA_GOR_HD_YUZEY;
    C.gor.yuzeyde = true;
    C.gor.nav_derinlik = 0.1f;
    C.gud.ref.d = 0.1f;
    C.gud.ref.w = 0.0f;
    ida_gud_kon_istegi(&C.gud, &C, &i);
    TEST_ASSERT_EQUAL_UINT8(IDA_KON_KIP_YUZEY, i.kip);
    /* Arıza eylemi HOLD: görev hedefi yazılmaz (arıza görevin önündedir). */
    C.gor.gv.hedef.dikey = (uint8_t)IDA_GOR_HD_DERINLIK;
    C.gor.yuzeyde = false;
    C.gor.fs_level = (uint8_t)IDA_FS_HOLD;
    C.gor.tut_yon_var = true;
    C.gor.tut_yon = 0.9f;
    ida_gud_kon_istegi(&C.gud, &C, &i);
    TEST_ASSERT_EQUAL_FLOAT(0.9f, i.yon);
    ida_gud_adim(&C.gud, &C);
    TEST_ASSERT_FALSE(C.gud.oto_var);
    /* Silahsız: KAPALI. */
    C.gor.fs_level = (uint8_t)IDA_FS_NONE;
    C.emn.armed = false;
    ida_gud_kon_istegi(&C.gud, &C, &i);
    TEST_ASSERT_EQUAL_UINT8(IDA_KON_KIP_KAPALI, i.kip);
}

static void test_en_az_irtifa_kirpmasi(void)
{
    auto_kur();
    ida_gud_adim(&C.gud, &C);
    C.gud.ref.d = 2.5f;
    C.gor.nav_derinlik = 2.0f;
    C.gor.irtifa_kisit = true;
    C.gor.nav_irtifa_var = true;
    C.gor.nav_irtifa = 0.1f; /* ALT_MIN 0,3: açık 0,2 m */
    ida_kon_istek_t i;
    ida_gud_kon_istegi(&C.gud, &C, &i);
    TEST_ASSERT_EQUAL_UINT8(1u, i.dalis_yasak);
    TEST_ASSERT_EQUAL_UINT8(IDA_KON_DIKEY_DERINLIK, i.dikey);
    TEST_ASSERT_FLOAT_WITHIN(1.0e-5f, 1.8f, i.derinlik); /* T/G/A: yükselme */
    /* MANUAL: yalnız aşağı girdi kırpılır. */
    C.emn.mode = (uint8_t)IDA_MODE_MANUAL;
    ida_gud_kon_istegi(&C.gud, &C, &i);
    TEST_ASSERT_EQUAL_UINT8(1u, i.dalis_yasak);
    TEST_ASSERT_EQUAL_UINT8(IDA_KON_DIKEY_KUVVET, i.dikey);
    /* Koşul yokken dokunulmaz. */
    C.emn.mode = (uint8_t)IDA_MODE_AUTO;
    C.gor.irtifa_kisit = false;
    ida_gud_kon_istegi(&C.gud, &C, &i);
    TEST_ASSERT_EQUAL_UINT8(0u, i.dalis_yasak);
    TEST_ASSERT_EQUAL_FLOAT(2.5f, i.derinlik);
}

/* ---- md. 6: GUIDED'da 3 s yeni hedef yoksa son hedefte kalınır ---------------------------- */

static void test_guided_sp_zaman_asimi(void)
{
    auto_kur();
    C.emn.mode = (uint8_t)IDA_MODE_GUIDED;
    C.gor.son_kip = (uint8_t)IDA_MODE_GUIDED;
    C.gor.nav_derinlik = 1.5f;
    C.gor.nav_yon = 0.4f;
    C.gor.nav_ne[0] = 4.0f;
    C.gor.nav_ne[1] = 5.0f;
    C.gud.dikey = (uint8_t)IDA_GUD_DIKEY_HIZ;
    C.gud.dikey_hiz = 0.2f;
    C.gud.yon_turu = (uint8_t)IDA_GUD_YON_HIZ;
    C.gud.yon_hizi = 0.1f;
    C.gud.yatay = (uint8_t)IDA_GUD_YATAY_HIZ;
    C.gud.hiz_ne[0] = 0.3f;
    C.gud.sp_var = true;
    C.gud.sp_son_ns = 50000 * DS_MS;
    C.cer.now_izi_ns = C.gud.sp_son_ns + (2999 * DS_MS);
    ida_gud_adim(&C.gud, &C);
    TEST_ASSERT_EQUAL_UINT8(IDA_GUD_DIKEY_HIZ, C.gud.dikey);
    TEST_ASSERT_EQUAL_UINT8(IDA_GUD_YON_HIZ, C.gud.yon_turu);
    TEST_ASSERT_EQUAL_UINT8(IDA_GUD_YATAY_HIZ, C.gud.yatay);
    C.cer.now_izi_ns = C.gud.sp_son_ns + (3000 * DS_MS);
    ida_gud_adim(&C.gud, &C);
    TEST_ASSERT_EQUAL_UINT8(IDA_GUD_DIKEY_DERINLIK, C.gud.dikey);
    TEST_ASSERT_EQUAL_FLOAT(1.5f, C.gud.derinlik);
    TEST_ASSERT_EQUAL_UINT8(IDA_GUD_YON_ACI, C.gud.yon_turu);
    TEST_ASSERT_EQUAL_FLOAT(0.4f, C.gud.yon);
    TEST_ASSERT_EQUAL_UINT8(IDA_GUD_YATAY_KONUM, C.gud.yatay);
    TEST_ASSERT_EQUAL_FLOAT(4.0f, C.gud.konum_ne[0]);
    TEST_ASSERT_EQUAL_FLOAT(5.0f, C.gud.konum_ne[1]);
    TEST_ASSERT_FALSE(C.gud.sp_var);
    TEST_ASSERT_TRUE((C.emn.hedef_iptal & IDA_EMN_IPTAL_KON) != 0u);
    /* Konum türü kanallar (DEPTH, YAW, POS_NE) son hedefte kalır. */
    C.gud.dikey = (uint8_t)IDA_GUD_DIKEY_DERINLIK;
    C.gud.derinlik = 3.0f;
    C.gud.yon_turu = (uint8_t)IDA_GUD_YON_ACI;
    C.gud.yon = -1.0f;
    C.gud.sp_var = true;
    C.cer.now_izi_ns += 10000 * DS_MS;
    ida_gud_adim(&C.gud, &C);
    TEST_ASSERT_EQUAL_FLOAT(3.0f, C.gud.derinlik);
    TEST_ASSERT_EQUAL_FLOAT(-1.0f, C.gud.yon);
    TEST_ASSERT_EQUAL_FLOAT(4.0f, C.gud.konum_ne[0]);
    /* Bileşen geçersizse hız kanalı görev modülünün tutmasına bırakılır. */
    C.gud.dikey = (uint8_t)IDA_GUD_DIKEY_HIZ;
    C.gud.sp_var = true;
    C.gor.nav_derinlik_var = false;
    ida_gud_adim(&C.gud, &C);
    TEST_ASSERT_EQUAL_UINT8(IDA_GUD_DIKEY_YOK, C.gud.dikey);
}

/* Çekirdek yolu: GUIDED_SP kabulü süreyi komutun now_w'sinden başlatır. */
static ida_in_t IN;
static ida_out_t OUT;

static void test_guided_sp_zaman_asimi_cekirdek_yolu(void)
{
    TEST_ASSERT_EQUAL(IDA_RC_OK, ds_kur(&C));
    const int64_t t = 2005 * DS_MS;
    ds_girdi(&IN, t, 1u, true);
    IN.tf.seq = 1u;
    IN.tf.t.ns = t;
    IN.tf.t_rx.ns = t;
    ds_imu(&IN.imu, t, 1u);
    C.emn.mode = (uint8_t)IDA_MODE_GUIDED; /* beyaz kutu */
    C.gor.son_kip = (uint8_t)IDA_MODE_GUIDED;
    IN.n_cmds = 1u;
    (void)memset(&IN.cmds[0], 0, sizeof(IN.cmds[0]));
    IN.cmds[0].cmd_id = 1u;
    IN.cmds[0].type = (uint8_t)IDA_CMD_GUIDED_SP;
    IN.cmds[0].u.guided.mask = (uint8_t)IDA_SP_VEL_D;
    IN.cmds[0].u.guided.vel_ned[2] = 0.1f;
    TEST_ASSERT_EQUAL(IDA_RC_OK, ida_core_step(&C, &IN, &OUT));
    TEST_ASSERT_EQUAL_UINT8(IDA_ACK_ACCEPTED, ds_sonuc(&OUT, 1u)->ack);
    TEST_ASSERT_EQUAL_UINT8(IDA_GUD_DIKEY_HIZ, C.gud.dikey);
    TEST_ASSERT_TRUE(C.gud.sp_var);
    TEST_ASSERT_TRUE(C.gud.sp_son_ns == t);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_ref_modeli_hiz_ivme_siniri);
    RUN_TEST(test_duz_hat);
    RUN_TEST(test_kose_donusu);
    RUN_TEST(test_akinti_altinda);
    RUN_TEST(test_konum_tutma);
    RUN_TEST(test_auto_istegi_kanallar);
    RUN_TEST(test_en_az_irtifa_kirpmasi);
    RUN_TEST(test_guided_sp_zaman_asimi);
    RUN_TEST(test_guided_sp_zaman_asimi_cekirdek_yolu);
    return UNITY_END();
}
