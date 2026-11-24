/*
 * gudum_oto.c — AUTO kipinin güdümü: görev yürütücüsünün hedefinden (ida_gor_hedef_t) kontrol
 * isteğine (W5-gorev-yurutucu; 02 §2.4).
 *
 * Yatay: yol noktası bacağında ILOS (Caharija vd. 2016; Fossen ve Pettersen 2014): istenen rota
 * χ_d = α − atan((y + σ·y_int)/Δ), ẏ_int = Δ·y / ((y + σ·y_int)² + Δ²). İstek NED hızıdır
 * (yol hızı × max(0, cos(χ_d − ψ)): araç rotaya dönmeden ilerlemez) ve yön çerçevesine çevrilir;
 * yön χ_d'ye. Kontrolün yatay hız döngüsü (02 §2.5) yer hızını izler; ILOS integrali hız
 * döngüsünün akıntı altındaki kalıntı hatasını giderir. Konum tutma: hız = GUD_POS_KP·(hedef −
 * konum) (GUIDED_SP'nin konum kanalıyla aynı yasa), yön tutulur.
 *
 * Dikey: derinlik referansı hız ve ivme sınırlı ikinci mertebe modelden geçer (02 §2.4,
 * refModel3'ün sadeleştirilmişi): |w| ≤ dikey_hiz, |ẇ| ≤ GUD_DER_IVME, hedefe durma mesafesiyle
 * yaklaşılır. Hız sınırı kontrolün REF_DEP_RATE'inden büyük olmaz (görev modülü kırpar); böylece
 * kontrolün kendi referans sınırlayıcısı bu referansı gecikmesiz izler. Hız sınırı düşünce (yeni
 * öğe ya da duraklama hedefi daha yavaşsa) |w| yeni sınıra sıçramaz, ivme sınırıyla iner: geçişte
 * en çok (|w| − w_yeni)/a süre yeni sınırın üstünde kalır (ivme sınırı hız sınırının önündedir).
 * Derinleşme kırpması (dalis_yasak, en az irtifa) sürerken referans kırpılmış değerde, sıfır
 * hızda tutulur (düzeltme turu bulgu 6). Yüzey hedefinde, araç
 * yüzeydeyken ve referans yüzey eşiğine varınca yüzey kipine geçilir (02 §2.4 SURFACE_TRANSIT:
 * dikey kapalı).
 *
 * Kontrolcünün iç yapısı değişmez: yalnız var olan istek kanalları (yön açısı, yatay hız,
 * derinlik) kullanılır.
 */
#include <string.h>

#include "cekirdek.h"
#include "gudum/gudum.h"
#include "mat/ida_math.h"

static float mutlak(float x)
{
    return (x < 0.0f) ? -x : x;
}

static float kirp(float x, float s)
{
    if (x > s) {
        return s;
    }
    return (x < -s) ? -s : x;
}

/* ---- Saf işlevler -------------------------------------------------------------------------- */

void ida_gud_ref_adim(ida_gud_ref_t *r, float hedef, float w_max, float a_max, float dt)
{
    if (!(dt > 0.0f) || !(a_max > 0.0f) || !(w_max > 0.0f)) {
        return;
    }
    const float dv = a_max * dt;
    const float e = hedef - r->d;
    if ((mutlak(e) <= (dv * dt)) && (mutlak(r->w) <= dv)) {
        r->d = hedef; /* hedefte dur: |Δw| ≤ a·dt */
        r->w = 0.0f;
        return;
    }
    /* Durma mesafesine uygun istenen hız: w² = 2·a·|e|, w_max ile sınırlı. */
    float w_ist = ida_karekok(2.0f * a_max * mutlak(e));
    if (w_ist > w_max) {
        w_ist = w_max;
    }
    if (e < 0.0f) {
        w_ist = -w_ist;
    }
    r->w += kirp(w_ist - r->w, dv);
    r->d += r->w * dt;
}

bool ida_gud_ilos(const float bas[2], const float hedef[2], const float p[2], float delta,
                  float sigma, float y_int, float *rota, float *capraz, float *boyuna)
{
    const float ln = hedef[0] - bas[0];
    const float le = hedef[1] - bas[1];
    const float l = ida_karekok((ln * ln) + (le * le));
    if (l < 0.01f) {
        *rota = ida_atan2(hedef[1] - p[1], hedef[0] - p[0]);
        *capraz = 0.0f;
        *boyuna = 0.0f;
        return false;
    }
    const float ca = ln / l;
    const float sa = le / l;
    const float dn = p[0] - bas[0];
    const float de = p[1] - bas[1];
    const float y = (-dn * sa) + (de * ca);
    *boyuna = (dn * ca) + (de * sa);
    *capraz = y;
    const float d = (delta > 0.01f) ? delta : 0.01f;
    *rota = ida_aci_sar(ida_atan2(le, ln) - ida_atan2(y + (sigma * y_int), d));
    return true;
}

void ida_gud_ilos_integral(float *y_int, float capraz, float delta, float sigma, float dt)
{
    if (!(dt > 0.0f)) {
        return;
    }
    const float d = (delta > 0.01f) ? delta : 0.01f;
    const float t = capraz + (sigma * *y_int);
    *y_int += dt * ((d * capraz) / ((t * t) + (d * d)));
    *y_int = kirp(*y_int, 1000.0f); /* savunma: dinamik kendiliğinden sınırlıdır */
}

void ida_gud_oto_yatay(const ida_gor_hedef_t *h, const float p[2], float yon, float y_int,
                       float delta, float sigma, float kp, float hiz_ne[2], float *yon_ist)
{
    hiz_ne[0] = 0.0f;
    hiz_ne[1] = 0.0f;
    *yon_ist = yon;
    if (h->yatay == (uint8_t)IDA_GOR_HY_YOL) {
        float rota = 0.0f;
        float y = 0.0f;
        float s = 0.0f;
        (void)ida_gud_ilos(h->bas_ne, h->hedef_ne, p, delta, sigma, y_int, &rota, &y, &s);
        float c = ida_cos(rota - yon);
        if (c < 0.0f) {
            c = 0.0f;
        }
        const float u = h->hiz * c;
        hiz_ne[0] = u * ida_cos(rota);
        hiz_ne[1] = u * ida_sin(rota);
        *yon_ist = rota;
    } else if (h->yatay == (uint8_t)IDA_GOR_HY_TUT) {
        hiz_ne[0] = kp * (h->hedef_ne[0] - p[0]);
        hiz_ne[1] = kp * (h->hedef_ne[1] - p[1]);
        *yon_ist = h->yon;
    } else {
        /* hedef yok */
    }
}

/* ---- Çekirdek bağlantısı ------------------------------------------------------------------ */

static bool oto_etkin(const ida_core_t *c)
{
    const ida_gor_t *gor = &c->gor;
    return (c->emn.mode == (uint8_t)IDA_MODE_AUTO) && c->emn.armed &&
           (ida_gor_fs_duzey(gor) < (uint8_t)IDA_FS_HOLD) &&
           (gor->gv.hedef.yatay != (uint8_t)IDA_GOR_HY_YOK) &&
           ((gor->mission_state == (uint8_t)IDA_MS_RUNNING) ||
            (gor->mission_state == (uint8_t)IDA_MS_PAUSED));
}

/* AUTO isteğinin derinlik kırpmasının üst sınırı (m, aşağı +): dalis_yasak'ta bulunulan derinlik
   (ida_gud_oto_istek), en az irtifada bulunulan derinlik eksi irtifa açığı (ida_gud_irtifa_kisit,
   AUTO: yükselme). Kırpma yoksa ya da derinlik bilinmiyorsa false. */
static bool oto_derinlik_siniri(const ida_core_t *c, float *sinir)
{
    const ida_gor_t *gor = &c->gor;
    if (!gor->nav_derinlik_var) {
        return false;
    }
    bool var = false;
    float s = 0.0f;
    if (gor->dalis_yasak) {
        s = gor->nav_derinlik;
        var = true;
    }
    if (gor->irtifa_kisit) {
        const float acik = ida_prm_f32(&c->prm, IDA_PRM_gor_alt_min) - gor->nav_irtifa;
        float ust = gor->nav_derinlik - ((acik > 0.0f) ? acik : 0.0f);
        if (ust < 0.0f) {
            ust = 0.0f;
        }
        if (!var || (ust < s)) {
            s = ust;
        }
        var = true;
    }
    *sinir = s;
    return var;
}

void ida_gud_oto_adim(ida_gud_t *g, const ida_core_t *c, bool iptal)
{
    if (!oto_etkin(c)) {
        g->oto_var = false;
        g->t_var = false;
        return;
    }
    if (iptal) {
        g->oto_var = false; /* daraltma: durum bulunulan derinlikten yeniden kurulur */
    }
    const ida_gor_t *gor = &c->gor;
    const ida_gor_hedef_t *h = &gor->gv.hedef;
    const ida_param_t *p = &c->prm;
    const int64_t t = c->cer.tf_t_ns;
    float dt = 0.0f;
    if (g->t_var && (t > g->t_son_ns)) {
        dt = (float)(t - g->t_son_ns) * 1.0e-9f;
        if (dt > 0.1f) {
            dt = 0.1f; /* boşlukta referans sıçramaz */
        }
    }
    g->t_son_ns = t;
    g->t_var = true;
    if (!g->oto_var) {
        g->ref.d = gor->nav_derinlik_var ? gor->nav_derinlik : 0.0f;
        g->ref.w = 0.0f;
        g->y_int = 0.0f;
        g->oto_kusak = h->kusak;
        g->oto_var = true;
    } else if (g->oto_kusak != h->kusak) {
        g->y_int = 0.0f; /* yeni bacak ya da durum; referans sürekli kalır */
        g->oto_kusak = h->kusak;
    } else {
        /* aynı hedef */
    }
    const float hedef_d = (h->dikey == (uint8_t)IDA_GOR_HD_YUZEY) ? 0.0f : h->hedef_d;
    ida_gud_ref_adim(&g->ref, hedef_d, h->dikey_hiz, ida_prm_f32(p, IDA_PRM_gud_der_ivme), dt);
    /* Derinleşme kırpması sürerken (dalis_yasak; en az irtifa) referans eski hedefe doğru
       ilerlemez: ref.d isteğin kırpılmış değerine, ref.w sıfıra çekilir (düzeltme turu bulgu 6).
       Kırpma kalkınca referans oradan sıfır hızla, ivme sınırıyla sürer. */
    float sinir = 0.0f;
    if (oto_derinlik_siniri(c, &sinir) && (g->ref.d > sinir)) {
        g->ref.d = sinir;
        g->ref.w = 0.0f;
    }
    if ((h->yatay == (uint8_t)IDA_GOR_HY_YOL) && gor->nav_konum_var) {
        float rota = 0.0f;
        float y = 0.0f;
        float s = 0.0f;
        const float delta = ida_prm_f32(p, IDA_PRM_gud_los_delta);
        const float sigma = ida_prm_f32(p, IDA_PRM_gud_ilos_sigma);
        if (ida_gud_ilos(h->bas_ne, h->hedef_ne, gor->nav_ne, delta, sigma, g->y_int, &rota, &y,
                         &s)) {
            ida_gud_ilos_integral(&g->y_int, y, delta, sigma, dt);
        }
    }
}

void ida_gud_oto_istek(const ida_gud_t *g, const ida_core_t *c, ida_kon_istek_t *i)
{
    if (!g->oto_var || !oto_etkin(c) || (i->kip == (uint8_t)IDA_KON_KIP_KAPALI)) {
        return; /* görev modülünün AUTO tutması kalır */
    }
    const ida_gor_t *gor = &c->gor;
    const ida_gor_hedef_t *h = &gor->gv.hedef;
    const ida_param_t *p = &c->prm;
    const uint8_t v = gor->gecerlilik;
    float hiz_ne[2] = {0.0f, 0.0f};
    float yon_ist = h->yon;
    if (gor->nav_konum_var) {
        ida_gud_oto_yatay(h, gor->nav_ne, gor->nav_yon, g->y_int,
                          ida_prm_f32(p, IDA_PRM_gud_los_delta),
                          ida_prm_f32(p, IDA_PRM_gud_ilos_sigma),
                          ida_prm_f32(p, IDA_PRM_gud_konum_kp), hiz_ne, &yon_ist);
    }
    /* Yön */
    if (gor->nav_yon_var) {
        i->yon_kanali = (uint8_t)IDA_KON_YON_ACI;
        i->yon = ida_aci_sar(yon_ist);
    } else {
        i->yon_kanali = (uint8_t)IDA_KON_YON_HIZ;
        i->yon_hizi = 0.0f;
    }
    /* Yatay: NED → yön çerçevesi (ileri, yan). Hız geçerliliği yoksa itki sıfır. */
    (void)memset(i->hiz, 0, sizeof(i->hiz));
    i->kuvvet[IDA_EKS_X] = 0.0f;
    i->kuvvet[IDA_EKS_Y] = 0.0f;
    if ((v & IDA_KES_G_YATAY_HIZ) != 0u) {
        i->yatay = (uint8_t)IDA_KON_YATAY_HIZ;
        if (gor->nav_konum_var && gor->nav_yon_var) {
            const float cp = ida_cos(gor->nav_yon);
            const float sp = ida_sin(gor->nav_yon);
            i->hiz[0] = (cp * hiz_ne[0]) + (sp * hiz_ne[1]);
            i->hiz[1] = (-sp * hiz_ne[0]) + (cp * hiz_ne[1]);
        }
    } else {
        i->yatay = (uint8_t)IDA_KON_YATAY_KUVVET;
    }
    /* Dikey */
    const float yuzey_d = ida_prm_f32(p, IDA_PRM_gor_yuzey_d);
    if ((h->dikey == (uint8_t)IDA_GOR_HD_YUZEY) && gor->yuzeyde && (g->ref.d <= yuzey_d)) {
        i->kip = (uint8_t)IDA_KON_KIP_YUZEY; /* SURFACE_TRANSIT: dikey kapalı */
        i->dikey = (uint8_t)IDA_KON_DIKEY_KUVVET;
        i->kuvvet[IDA_EKS_Z] = 0.0f;
    } else if (gor->nav_derinlik_var) {
        float d = (g->ref.d > 0.0f) ? g->ref.d : 0.0f;
        if (gor->dalis_yasak && (gor->nav_derinlik < d)) {
            d = gor->nav_derinlik; /* sızıntı algılayıcısı bayat: derinleşme yok */
        }
        i->dikey = (uint8_t)IDA_KON_DIKEY_DERINLIK;
        i->derinlik = d;
    } else if ((v & IDA_KES_G_DIKEY_HIZ) != 0u) {
        i->dikey = (uint8_t)IDA_KON_DIKEY_HIZ;
        i->dikey_hiz = 0.0f;
    } else {
        i->dikey = (uint8_t)IDA_KON_DIKEY_KUVVET;
        i->kuvvet[IDA_EKS_Z] = 0.0f;
    }
}

void ida_gud_irtifa_kisit(const ida_core_t *c, ida_kon_istek_t *i)
{
    const ida_gor_t *gor = &c->gor;
    if (!gor->irtifa_kisit || (i->kip != (uint8_t)IDA_KON_KIP_DALIS)) {
        return;
    }
    i->dalis_yasak = 1u; /* aşağı girdi kırpılır (bütün kipler; kontrol de uygular) */
    if (!gor->nav_derinlik_var) {
        return;
    }
    const uint8_t kip = c->emn.mode;
    const bool yukselme = (kip == (uint8_t)IDA_MODE_DEPTH_HOLD) ||
                          (kip == (uint8_t)IDA_MODE_STATION_KEEP) ||
                          (kip == (uint8_t)IDA_MODE_GUIDED) || (kip == (uint8_t)IDA_MODE_AUTO);
    const float acik = ida_prm_f32(&c->prm, IDA_PRM_gor_alt_min) - gor->nav_irtifa;
    float ust = gor->nav_derinlik - ((acik > 0.0f) ? acik : 0.0f);
    if (ust < 0.0f) {
        ust = 0.0f;
    }
    if (!yukselme) {
        ust = gor->nav_derinlik; /* M sütunu: yalnız derinleşme yok */
    }
    if (i->dikey == (uint8_t)IDA_KON_DIKEY_DERINLIK) {
        if (i->derinlik > ust) {
            i->derinlik = ust;
        }
    } else if ((i->dikey == (uint8_t)IDA_KON_DIKEY_HIZ) && (i->dikey_hiz > 0.0f)) {
        if (yukselme) {
            i->dikey = (uint8_t)IDA_KON_DIKEY_DERINLIK;
            i->derinlik = ust;
        } else {
            i->dikey_hiz = 0.0f;
        }
    } else {
        /* kuvvet kanalı ya da yukarı hız: kontrolün derinleşme yasağı yeter */
    }
}
