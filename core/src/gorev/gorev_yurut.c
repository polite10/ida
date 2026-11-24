/*
 * gorev_yurut.c — görev yürütücüsü (W5-gorev-yurutucu; 02 §2.4, §2.6, §4.3; W5-K24).
 *
 * Yürütücü yalnız güdüme hedef üretir (ida_gor_hedef_t; 02 §2.6 "yürütücü yalnız setpoint
 * üretir"); kip, silahlandırma ve atış kontrolüne hiçbir yönde yazmaz.
 *
 * Adım sırası (gorev.c ida_gor_adim_ic): görev adımı (bu dosya) → kip düşmeleri → arıza
 * değerlendirmesi → ida_gor_gorev_ariza. Böylece aynı adımda biten son öğe AUTO'yu düşürür,
 * zaman aşımının iptal mandalı aynı adımın arıza düzeyine girer (genişleticiler NARROWED).
 *
 * Engeller (RUNNING ⇄ PAUSED her adımda yeniden değerlendirilir): kip AUTO değil ya da araç
 * silahsız; yatay konum geçersiz; arıza eylemi HOLD ya da üstü (arıza görevin önündedir);
 * operatör duraklaması; ölü hesap yol sınırı (W5-K24: yüzeyde GNSS beklenir). Engel kalkınca
 * görev kaldığı öğeden sürer; operatör duraklaması yalnız RESUME ya da SET_MODE(AUTO) ile kalkar.
 *
 * Saatler (02 §2.6, iskelet §4): öğe zaman aşımları ve toplam görev süresi duvar saatinde
 * (öğe süresi yalnız RUNNING iken sayılır); davranış süreleri (DIVE_TO bandı, HOLD süresi)
 * ölçüm zamanında (son zaman çerçevesi).
 *
 * Öğeler (A1 Tablo 3.6a; bitiş ölçütü / zaman aşımı):
 *   WAYPOINT  yatay uzaklık ≤ p0 ya da bacağın sonu geçildi / KAT·(uzaklık/p1 + |Δd|/w) + PAY
 *   DIVE_TO   |d − depth| ≤ p1, BANT_MS boyunca / KAT·|Δd|/min(p0, REF_DEP_RATE) + PAY
 *   SURFACE   derinlik ≤ YUZEY_D / KAT·d/min(p0, REF_DEP_RATE) + PAY
 *   HOLD      konum ve derinlik p1 içinde olduktan sonra p0 s / p0 + KAT·(uzaklık/LIM_V +
 *             |Δd|/w) + PAY
 *   WAIT_GNSS yükselme: yüzeyde / KAT·d/v_asc + PAY; bekleme: GNSS konumu taze ve istenen fix
 *             doğrulanabilir / p0
 *   SCAN_*    bu dilimde yürütülmez: başlangıcında görev ABORTED + iptal mandalı (SURFACE)
 * Zaman aşımında görev ABORTED olur ve iptal mandalı (IDA_GOR_K_GOREV_IPTAL) SURFACE ister.
 */
#include <string.h>

#include "gorev/gorev_ic.h"
#include "kestirim/kes_ic.h" /* ida_kes_gnss_ne: GNSS ile aynı yerel düzlem */
#include "mat/ida_math.h"
#include "zaman/zaman.h"

/* WAIT_GNSS'in doğrulayabildiği en yüksek fix türü: kestirim yalnız 3D ve üstünü füzyonlar
   (kes_yatay.c), türün kendisini görev modülüne taşımaz; DGPS ve RTK istenirse doğrulanamaz. */
#define FIX_DOGRULANABILIR 2.0f /* IDA_GNSS_FIX_3D */

static float uzaklik(const float a[2], const float b[2])
{
    const float dn = a[0] - b[0];
    const float de = a[1] - b[1];
    return ida_karekok((dn * dn) + (de * de));
}

static float mutlak(float x)
{
    return (x < 0.0f) ? -x : x;
}

static float en_az(float a, float b)
{
    return (a < b) ? a : b;
}

/* s → ns (zaman aşımı; doymalı). */
static int64_t sn_ns(float s)
{
    if (!(s > 0.0f)) {
        return 0;
    }
    const float sinir = 1.0e7f; /* ≈ 116 gün */
    const float x = (s > sinir) ? sinir : s;
    return (int64_t)(x * 1.0e9f);
}

static float ref_hizi(const ida_gor_girdi_t *gi)
{
    return ida_prm_f32(gi->prm, IDA_PRM_kon_ref_derinlik_hizi);
}

static void iptal(ida_gor_t *g, uint32_t neden)
{
    ida_gor_gorev_t *v = &g->gv;
    g->mission_state = (uint8_t)IDA_MS_ABORTED;
    v->iptal_bekliyor = true;
    v->iptal_nedeni |= neden;
    v->operator_durdu = false;
    v->dr_bekliyor = false;
    v->hedef.yatay = (uint8_t)IDA_GOR_HY_YOK;
    v->hedef.kusak++;
}

/* Ölü hesap yolu (W5-K24): GNSS konumu taze değilken kat edilen yatay yol; taze GNSS sıfırlar. */
static void dr_guncelle(ida_gor_t *g, const ida_gor_girdi_t *gi)
{
    ida_gor_gorev_t *v = &g->gv;
    if (gi->gnss_taze) {
        v->dr_yol = 0.0f;
    } else if (g->nav_konum_var && v->dr_son_var) {
        v->dr_yol += uzaklik(g->nav_ne, v->dr_son_ne);
        if (v->dr_yol > 1.0e6f) {
            v->dr_yol = 1.0e6f;
        }
    } else {
        /* konum yok ya da taban yok: yol sayılamaz (konum kaybı ayrı arıza koşuludur) */
    }
    v->dr_son_var = g->nav_konum_var;
    v->dr_son_ne[0] = g->nav_ne[0];
    v->dr_son_ne[1] = g->nav_ne[1];
}

/* Öğenin yatay hedefi; GLOBAL çözülemezse false. */
static bool oge_konumu(const ida_gor_oge_t *o, const ida_gor_girdi_t *gi, const float cur[2],
                       float ne[2])
{
    if (o->cerceve == (uint8_t)IDA_MI_FRAME_LOCAL) {
        ne[0] = o->konum.ne[0];
        ne[1] = o->konum.ne[1];
        return true;
    }
    if (o->cerceve == (uint8_t)IDA_MI_FRAME_GLOBAL) {
        return ida_kes_gnss_ne(o->konum.ll[0], o->konum.ll[1], gi->orijin_lat_e7,
                               gi->orijin_lon_e7, ne);
    }
    ne[0] = cur[0];
    ne[1] = cur[1];
    return true;
}

static void hedef_tut(ida_gor_gorev_t *v, const float ne[2], float yon)
{
    v->hedef.yatay = (uint8_t)IDA_GOR_HY_TUT;
    v->hedef.hedef_ne[0] = ne[0];
    v->hedef.hedef_ne[1] = ne[1];
    v->hedef.bas_ne[0] = ne[0];
    v->hedef.bas_ne[1] = ne[1];
    v->hedef.yon = yon;
}

static void hedef_dikey(ida_gor_gorev_t *v, float d, float w, float yuzey_d)
{
    v->hedef.dikey = (d <= yuzey_d) ? (uint8_t)IDA_GOR_HD_YUZEY : (uint8_t)IDA_GOR_HD_DERINLIK;
    v->hedef.hedef_d = (d <= yuzey_d) ? 0.0f : d;
    v->hedef.dikey_hiz = (w > 0.0f) ? w : 0.05f;
}

/* Öğe başlangıcı: konum, derinlik ve yön yakalanır; hedef ve zaman aşımı kurulur. */
static void oge_baslat(ida_gor_t *g, const ida_gor_girdi_t *gi)
{
    ida_gor_gorev_t *v = &g->gv;
    const ida_gor_oge_t *o = &v->oge[g->mission_item];
    const ida_param_t *p = gi->prm;
    v->oge_basladi = true;
    v->oge_gecen_ns = 0;
    v->bant_say = false;
    v->asama = 0u;
    v->hedef.kusak++;
    v->oge_ne[0] = g->nav_ne[0];
    v->oge_ne[1] = g->nav_ne[1];
    v->oge_d = g->nav_derinlik_var ? g->nav_derinlik : 0.0f;
    v->oge_yon = g->nav_yon;
    const float kat = ida_prm_f32(p, IDA_PRM_gor_asim_kat);
    const float pay = (float)ida_prm_u32(p, IDA_PRM_gor_asim_pay_s);
    const float ref = ref_hizi(gi);
    if (!oge_konumu(o, gi, v->oge_ne, v->oge_hedef_ne)) {
        iptal(g, IDA_FSR_GEOFENCE); /* GLOBAL hedef yerel düzleme çözülemedi */
        return;
    }
    const float yatay = uzaklik(v->oge_hedef_ne, v->oge_ne);
    const float dd = mutlak(o->derinlik - v->oge_d);
    float sure = 0.0f;
    switch (o->tur) {
    case IDA_MI_WAYPOINT:
        sure = kat * ((yatay / o->p[1]) + (dd / ref)) + pay;
        break;
    case IDA_MI_DIVE_TO:
        sure = kat * (dd / en_az(o->p[0], ref)) + pay;
        break;
    case IDA_MI_SURFACE:
        sure = kat * (v->oge_d / en_az(o->p[0], ref)) + pay;
        break;
    case IDA_MI_HOLD: {
        const float lim_v = ida_prm_f32(p, IDA_PRM_kon_lim_v);
        const float v_yatay = (lim_v > 0.05f) ? lim_v : 0.05f;
        sure = o->p[0] + (kat * ((yatay / v_yatay) + (dd / ref))) + pay;
        break;
    }
    case IDA_MI_WAIT_GNSS:
        sure = kat * (v->oge_d / ida_prm_f32(p, IDA_PRM_gor_yukselme_hizi)) + pay;
        break;
    default:
        /* SCAN_LINES, SCAN_POINT: tarama deseni sonar işleme dilimiyle gelir; bu dilimde
           yürütülmez (W5-gorev-yurutucu md. 4). Tanımsız tür depoya giremez (savunma). */
        iptal(g, IDA_FSR_GEOFENCE);
        return;
    }
    v->oge_asim_ns = sn_ns(sure);
}

/* Bacak boyunca ilerleme: bas → hedef doğrusunda hedefin dikmesi geçildi mi. */
static bool bacak_gecildi(const float bas[2], const float hedef[2], const float cur[2])
{
    const float ln = hedef[0] - bas[0];
    const float le = hedef[1] - bas[1];
    const float l2 = (ln * ln) + (le * le);
    if (l2 < 1.0e-4f) {
        return false;
    }
    const float s = ((cur[0] - bas[0]) * ln) + ((cur[1] - bas[1]) * le);
    return s >= l2;
}

/* Etkin öğenin bitiş ölçütü ve güdüm hedefi. */
static bool oge_yurut(ida_gor_t *g, const ida_gor_girdi_t *gi)
{
    ida_gor_gorev_t *v = &g->gv;
    const ida_gor_oge_t *o = &v->oge[g->mission_item];
    const ida_param_t *p = gi->prm;
    const float yuzey_d = ida_prm_f32(p, IDA_PRM_gor_yuzey_d);
    const float ref = ref_hizi(gi);
    const int64_t t = gi->t_olcum_ns;
    const float d = g->nav_derinlik;
    bool bitti = false;
    switch (o->tur) {
    case IDA_MI_WAYPOINT: {
        const float *bas = v->onceki_yol ? v->onceki_ne : v->oge_ne;
        v->hedef.yatay = (uint8_t)IDA_GOR_HY_YOL;
        v->hedef.bas_ne[0] = bas[0];
        v->hedef.bas_ne[1] = bas[1];
        v->hedef.hedef_ne[0] = v->oge_hedef_ne[0];
        v->hedef.hedef_ne[1] = v->oge_hedef_ne[1];
        v->hedef.hiz = o->p[1];
        v->hedef.yon = v->oge_yon;
        hedef_dikey(v, o->derinlik, ref, yuzey_d);
        bitti = (uzaklik(g->nav_ne, v->oge_hedef_ne) <= o->p[0]) ||
                bacak_gecildi(bas, v->oge_hedef_ne, g->nav_ne);
        break;
    }
    case IDA_MI_DIVE_TO: {
        hedef_tut(v, v->oge_ne, v->oge_yon);
        hedef_dikey(v, o->derinlik, en_az(o->p[0], ref), yuzey_d);
        const bool bantta = g->nav_derinlik_var && (mutlak(d - o->derinlik) <= o->p[1]);
        if (!bantta) {
            v->bant_say = false;
        } else if (!v->bant_say) {
            v->bant_say = true;
            v->bant_bas_ns = t;
        } else {
            bitti = (t - v->bant_bas_ns) >= ida_ms_ns(ida_prm_u32(p, IDA_PRM_gor_bant_ms));
        }
        break;
    }
    case IDA_MI_SURFACE:
        hedef_tut(v, v->oge_ne, v->oge_yon);
        hedef_dikey(v, 0.0f, en_az(o->p[0], ref), yuzey_d);
        bitti = g->yuzeyde;
        break;
    case IDA_MI_HOLD: {
        hedef_tut(v, v->oge_hedef_ne, v->oge_yon);
        hedef_dikey(v, o->derinlik, ref, yuzey_d);
        bool derinlikte = false;
        if (o->derinlik <= yuzey_d) {
            derinlikte = g->yuzeyde;
        } else {
            derinlikte = g->nav_derinlik_var && (mutlak(d - o->derinlik) <= o->p[1]);
        }
        const bool icinde = (uzaklik(g->nav_ne, v->oge_hedef_ne) <= o->p[1]) && derinlikte;
        if (!v->bant_say && icinde) {
            v->bant_say = true; /* süre ilk kez tolerans içine girilince başlar */
            v->bant_bas_ns = t;
        }
        bitti = v->bant_say && ((t - v->bant_bas_ns) >= sn_ns(o->p[0]));
        break;
    }
    case IDA_MI_WAIT_GNSS:
        hedef_tut(v, v->oge_ne, v->oge_yon);
        hedef_dikey(v, 0.0f, ida_prm_f32(p, IDA_PRM_gor_yukselme_hizi), yuzey_d);
        if ((v->asama == 0u) && g->yuzeyde) {
            v->asama = 1u; /* bekleme: süre yüzeye çıkıştan sayılır */
            v->oge_gecen_ns = 0;
            v->oge_asim_ns = sn_ns(o->p[0]);
        }
        bitti = (v->asama == 1u) && g->yuzeyde && gi->gnss_taze && g->nav_konum_var &&
                (o->p[1] <= FIX_DOGRULANABILIR);
        break;
    default:
        break; /* oge_baslat iptal etti */
    }
    /* Aşama (status.phase): yüzey hedefi ve yüzeyde → SURFACE_TRANSIT; DIVE_TO → DIVE;
       yüzey hedefi ve su altında → ASCENT; öteki su altı → SUBMERGED. */
    if (o->tur == (uint8_t)IDA_MI_DIVE_TO) {
        v->hedef.asama = (uint8_t)IDA_PH_DIVE;
    } else if (v->hedef.dikey == (uint8_t)IDA_GOR_HD_YUZEY) {
        v->hedef.asama = g->yuzeyde ? (uint8_t)IDA_PH_SURFACE_TRANSIT : (uint8_t)IDA_PH_ASCENT;
    } else {
        v->hedef.asama = g->yuzeyde ? (uint8_t)IDA_PH_DIVE : (uint8_t)IDA_PH_SUBMERGED;
    }
    return bitti;
}

/* Duraklamadaki hedef: ölü hesap beklemesinde yüzeye çıkış ve konum tutma; operatör
   duraklamasında duraklama noktasında konum, derinlik ve yön; öteki engellerde hedef yok. */
static void durak_hedefi(ida_gor_t *g, const ida_gor_girdi_t *gi, bool yalniz_dr_ya_da_operator)
{
    ida_gor_gorev_t *v = &g->gv;
    const float yuzey_d = ida_prm_f32(gi->prm, IDA_PRM_gor_yuzey_d);
    if (!yalniz_dr_ya_da_operator || !v->durak_var || !g->nav_konum_var) {
        v->hedef.yatay = (uint8_t)IDA_GOR_HY_YOK;
        return;
    }
    hedef_tut(v, v->durak_ne, v->durak_yon);
    if (v->dr_bekliyor) {
        hedef_dikey(v, 0.0f, ida_prm_f32(gi->prm, IDA_PRM_gor_yukselme_hizi), yuzey_d);
        v->hedef.asama = g->yuzeyde ? (uint8_t)IDA_PH_SURFACE_TRANSIT : (uint8_t)IDA_PH_ASCENT;
    } else {
        hedef_dikey(v, v->durak_d, ref_hizi(gi), yuzey_d);
        v->hedef.asama = g->yuzeyde ? (uint8_t)IDA_PH_SURFACE_TRANSIT : (uint8_t)IDA_PH_SUBMERGED;
    }
}

void ida_gor_gorev_durak_yakala(ida_gor_t *g)
{
    ida_gor_gorev_t *v = &g->gv;
    v->durak_var = g->nav_konum_var;
    v->durak_ne[0] = g->nav_ne[0];
    v->durak_ne[1] = g->nav_ne[1];
    v->durak_d = g->nav_derinlik_var ? g->nav_derinlik : 0.0f;
    v->durak_yon = g->nav_yon;
}

void ida_gor_gorev_adim(ida_gor_t *g, const ida_emn_t *yetki, const ida_gor_girdi_t *gi)
{
    ida_gor_gorev_t *v = &g->gv;
    const int64_t now = gi->now_ns;
    const int64_t dt = v->son_now_var ? (now - v->son_now_ns) : 0;
    v->son_now_ns = now;
    v->son_now_var = true;
    dr_guncelle(g, gi);
    const uint8_t ms0 = g->mission_state;
    if ((ms0 != (uint8_t)IDA_MS_RUNNING) && (ms0 != (uint8_t)IDA_MS_PAUSED)) {
        v->kosuyordu = false;
        v->hedef.yatay = (uint8_t)IDA_GOR_HY_YOK;
        return;
    }
    if (!v->gorev_bas_var) {
        v->gorev_bas_var = true;
        v->gorev_bas_ns = now;
    }
    const bool kip_engel = (yetki->mode != (uint8_t)IDA_MODE_AUTO) || !yetki->armed;
    /* Ölü hesap yol sınırı (W5-K24): koşan görevde aşılırsa görev duraklar, araç yüzeye çıkıp
       GNSS bekler; taze GNSS konumu beklemeyi bitirir, DR_BEKLE_S aşılırsa görev iptal edilir.
       Bekleme yalnız AUTO'da ve silahlıyken sürer: kipten çıkınca bırakılır (yeniden girişte yol
       hâlâ sınırın üstündeyse yeniden başlar). */
    if (kip_engel) {
        v->dr_bekliyor = false;
    }
    if ((ms0 == (uint8_t)IDA_MS_RUNNING) && !kip_engel && !v->dr_bekliyor &&
        (v->dr_yol > ida_prm_f32(gi->prm, IDA_PRM_gor_dr_yol_max))) {
        v->dr_bekliyor = true;
        v->dr_bas_ns = now;
        v->durak_var = false; /* duraklama noktası bu adımda yakalanır */
    }
    if (v->dr_bekliyor) {
        if (gi->gnss_taze && g->yuzeyde && g->nav_konum_var) {
            v->dr_bekliyor = false;
        } else if ((now - v->dr_bas_ns) >=
                   ida_ms_ns(1000u * ida_prm_u32(gi->prm, IDA_PRM_gor_dr_bekle_s))) {
            iptal(g, IDA_FSR_EST_DEGRADED);
            v->kosuyordu = false;
            return;
        } else {
            /* bekleniyor */
        }
    }
    const bool ariza_engel = g->fs_level >= (uint8_t)IDA_FS_HOLD;
    const bool konum_engel = !g->nav_konum_var;
    if (kip_engel || ariza_engel || konum_engel || v->operator_durdu || v->dr_bekliyor) {
        if (ms0 == (uint8_t)IDA_MS_RUNNING) {
            ida_gor_gorev_durak_yakala(g);
            g->mission_state = (uint8_t)IDA_MS_PAUSED;
            v->hedef.kusak++;
        } else if (v->dr_bekliyor && !v->durak_var) {
            ida_gor_gorev_durak_yakala(g);
        } else {
            /* duraklama noktası korunur */
        }
        durak_hedefi(g, gi, !kip_engel && !ariza_engel && !konum_engel);
        v->kosuyordu = false;
        return;
    }
    if (ms0 == (uint8_t)IDA_MS_PAUSED) {
        g->mission_state = (uint8_t)IDA_MS_RUNNING;
        v->hedef.kusak++;
    }
    if (v->oge_basladi && v->kosuyordu) {
        v->oge_gecen_ns += dt; /* öğe süresi yalnız RUNNING'de */
    }
    v->kosuyordu = true;
    if (g->mission_item >= v->n) {
        g->mission_state = (uint8_t)IDA_MS_COMPLETE; /* boş görev (savunma) */
        v->hedef.yatay = (uint8_t)IDA_GOR_HY_YOK;
        return;
    }
    if (!v->oge_basladi) {
        oge_baslat(g, gi);
        if (g->mission_state != (uint8_t)IDA_MS_RUNNING) {
            return; /* iptal */
        }
    }
    const bool bitti = oge_yurut(g, gi);
    if (bitti) {
        const ida_gor_oge_t *o = &v->oge[g->mission_item];
        v->onceki_yol = o->tur == (uint8_t)IDA_MI_WAYPOINT;
        v->onceki_ne[0] = v->oge_hedef_ne[0];
        v->onceki_ne[1] = v->oge_hedef_ne[1];
        if ((uint32_t)g->mission_item + 1u >= (uint32_t)v->n) {
            g->mission_state = (uint8_t)IDA_MS_COMPLETE;
            v->hedef.yatay = (uint8_t)IDA_GOR_HY_YOK;
            v->hedef.kusak++;
            v->kosuyordu = false;
            return;
        }
        g->mission_item++;
        v->oge_basladi = false;
        oge_baslat(g, gi); /* sonraki öğenin hedefi aynı adımda */
        if (g->mission_state == (uint8_t)IDA_MS_RUNNING) {
            (void)oge_yurut(g, gi); /* hedef kurulur; bitiş sonraki adımda değerlendirilir */
        }
        return;
    }
    if (v->oge_gecen_ns > v->oge_asim_ns) {
        iptal(g, IDA_FSR_GEOFENCE); /* öğe zaman aşımı: görev durur, SURFACE (02 §2.6) */
        v->kosuyordu = false;
    }
}

void ida_gor_gorev_ariza(ida_gor_t *g, uint8_t kip, ida_gunluk_t *gun, int64_t now_ns)
{
    const uint8_t ms = g->mission_state;
    if ((ms != (uint8_t)IDA_MS_RUNNING) && (ms != (uint8_t)IDA_MS_PAUSED)) {
        return;
    }
    if (g->fs_level >= (uint8_t)IDA_FS_SURFACE) {
        /* SURFACE ve üstü görevi iptal eder (02 §4.3 düşük batarya: görev iptali + SURFACE); iptal
           mandalı kip AUTO'dan düşse de yüzeye çıkışı sürdürür (EMNİYET). */
        iptal(g, g->fs_nedenleri);
        g->gv.kosuyordu = false;
        ida_gor_ariza_iptal_uygula(g, kip, gun, now_ns);
    } else if ((g->fs_level >= (uint8_t)IDA_FS_HOLD) && (ms == (uint8_t)IDA_MS_RUNNING)) {
        /* HOLD: görev durur (02 §4.3 EST_DEGRADED "HOLD (görev durur)"); arıza kalkınca sürer. */
        ida_gor_gorev_durak_yakala(g);
        g->mission_state = (uint8_t)IDA_MS_PAUSED;
        g->gv.hedef.yatay = (uint8_t)IDA_GOR_HY_YOK;
        g->gv.hedef.kusak++;
        g->gv.kosuyordu = false;
    } else {
        /* arıza eylemi yok ya da WARN */
    }
}

uint32_t ida_gor_gorev_kosullari(const ida_gor_t *g, const ida_gor_girdi_t *gi)
{
    const ida_gor_gorev_t *v = &g->gv;
    const ida_param_t *p = gi->prm;
    uint32_t e = 0u;
    /* Coğrafi sınır (02 §4.3): orijinden yatay yarıçap ya da toplam görev süresi. */
    if (g->nav_konum_var) {
        const float r = ida_prm_f32(p, IDA_PRM_gor_geo_yaricap);
        if (((g->nav_ne[0] * g->nav_ne[0]) + (g->nav_ne[1] * g->nav_ne[1])) > (r * r)) {
            e |= IDA_GOR_K_BIT(IDA_GOR_K_COGRAFI);
        }
    }
    const bool etkin = (g->mission_state == (uint8_t)IDA_MS_RUNNING) ||
                       (g->mission_state == (uint8_t)IDA_MS_PAUSED);
    if (etkin && v->gorev_bas_var &&
        ((gi->now_ns - v->gorev_bas_ns) >=
         ida_ms_ns(1000u * ida_prm_u32(p, IDA_PRM_gor_gorev_azami_s)))) {
        e |= IDA_GOR_K_BIT(IDA_GOR_K_COGRAFI);
    }
    /* En az irtifa (02 §4.3): irtifa biliniyorsa (nav.altitude ≥ 0). ALT_MIN = 0 kapalıdır. */
    if (g->nav_irtifa_var && (g->nav_irtifa < ida_prm_f32(p, IDA_PRM_gor_alt_min))) {
        e |= IDA_GOR_K_BIT(IDA_GOR_K_IRTIFA);
    }
    if (v->iptal_bekliyor) {
        e |= IDA_GOR_K_BIT(IDA_GOR_K_GOREV_IPTAL);
    }
    return e;
}
