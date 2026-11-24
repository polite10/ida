/*
 * test_gorev_yurut.c — W5-gorev-yurutucu kabul 3 (öğe yürütücüleri: bitiş ölçütü ve zaman aşımı;
 * aşımda ABORTED + yüzeye çıkış), kabul 5 (arıza önceliği: arıza eylemi görevi keser; konum
 * geçerliliği düşünce davranış; ölü hesap yol sınırı; coğrafi sınır; en az irtifa) ve basit
 * kinematik modelle bir görevin baştan sona yürütülmesi.
 *
 * Zaman aşımlarının beklenen değerleri testte A1 Tablo 3.6a alanlarından ve varsayılan
 * parametrelerden (ASIM_KAT 2, ASIM_PAY_S 60, BANT_MS 1000, REF_DEP_RATE 0,25 m/s, GUD_DER_IVME
 * 0,05 m/s²) hesaplanır.
 */
#include "gorev_duzenek.h"
#include "gudum/gudum.h"
#include "mat/ida_math.h"

void setUp(void);
void tearDown(void);
void setUp(void) {}
void tearDown(void) {}

static gd_t O;

#define KAT 2.0f
#define PAY 60.0f
#define IVME 0.05f /* GUD_DER_IVME: referansın hızlanma + yavaşlama süresi w/a */

static void konum(gd_t *o, float n, float e)
{
    o->gi.nav_ne[0] = n;
    o->gi.nav_ne[1] = e;
}

/* Görevi yükler ve AUTO'da başlatır; bir adım ilerler (öğe 0 başlar). */
static void baslat(gd_t *o, const ida_mission_item_t *g, uint16_t n)
{
    gd_yukle(o, g, n);
    gd_auto(o);
    gd_ilerle(o, 100);
}

/* İptal: ABORTED, iptal mandalı, SURFACE; bir adım sonra kip DEPTH_HOLD, yine SURFACE. */
static void iptal_bekle(gd_t *o, uint32_t neden)
{
    TEST_ASSERT_EQUAL_UINT8(IDA_MS_ABORTED, o->g.mission_state);
    TEST_ASSERT_TRUE((o->g.mandal & IDA_GOR_K_BIT(IDA_GOR_K_GOREV_IPTAL)) != 0u);
    TEST_ASSERT_EQUAL_UINT8(IDA_FS_SURFACE, o->g.fs_level);
    TEST_ASSERT_TRUE((o->g.fs_nedenleri & neden) == neden);
    gd_ilerle(o, 100);
    TEST_ASSERT_EQUAL_UINT8(IDA_MODE_DEPTH_HOLD, o->e.mode);
    TEST_ASSERT_EQUAL_UINT8(IDA_FS_SURFACE, o->g.fs_level);
    TEST_ASSERT_TRUE(o->e.armed); /* SURFACE silahsızlandırmaz */
    gd_ilerle(o, 100); /* daraltmalı çağrıdan sonra (genişleticiler NARROWED almasın) */
}

/* `sn` saniye beklenir; son adımdan önce görev RUNNING kalmalı, sonra ABORTED olmalı. */
static void asim_bekle(gd_t *o, float sn, uint32_t neden)
{
    const int64_t ms = (int64_t)(sn * 1000.0f);
    gd_ilerle(o, ms - 500);
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(IDA_MS_RUNNING, o->g.mission_state, "zaman aşımından önce");
    gd_ilerle(o, 1000);
    iptal_bekle(o, neden);
}

/* ---- WAYPOINT ------------------------------------------------------------------------------ */

static void test_waypoint_kabul_yaricapi_ve_gecis(void)
{
    gd_t *o = &O;
    gd_kur(o);
    ida_mission_item_t g[3]; /* MSVC C4204: sabit olmayan başlatıcı yok */
    g[0] = gd_oge((uint8_t)IDA_MI_WAYPOINT, (uint8_t)IDA_MI_FRAME_LOCAL, 0.5f, 0.5f, 10.0f, 0.0f, 0.0f);
    g[1] = gd_oge((uint8_t)IDA_MI_WAYPOINT, (uint8_t)IDA_MI_FRAME_LOCAL, 0.5f, 0.5f, 10.0f, 10.0f, 0.0f);
    g[2] = gd_oge((uint8_t)IDA_MI_HOLD, (uint8_t)IDA_MI_FRAME_NONE, 600.0f, 5.0f, 0.0f, 0.0f, 0.0f);
    baslat(o, g, 3u);
    const ida_gor_hedef_t *h = &o->g.gv.hedef;
    TEST_ASSERT_EQUAL_UINT8(IDA_GOR_HY_YOL, h->yatay);
    TEST_ASSERT_EQUAL_UINT8(IDA_GOR_HD_YUZEY, h->dikey); /* depth 0: yüzey seyri */
    TEST_ASSERT_EQUAL_FLOAT(10.0f, h->hedef_ne[0]);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, h->bas_ne[0]);
    TEST_ASSERT_EQUAL_FLOAT(0.5f, h->hiz);
    TEST_ASSERT_EQUAL_UINT8(IDA_PH_SURFACE_TRANSIT, gd_durum(o).phase);
    konum(o, 9.0f, 0.2f); /* 1,02 m: dışarıda */
    gd_ilerle(o, 100);
    TEST_ASSERT_EQUAL_UINT16(0u, o->g.mission_item);
    konum(o, 9.6f, 0.2f); /* 0,45 m: kabul yarıçapında */
    gd_ilerle(o, 100);
    TEST_ASSERT_EQUAL_UINT16(1u, o->g.mission_item);
    /* İkinci bacak önceki yol noktasından başlar. */
    TEST_ASSERT_EQUAL_FLOAT(10.0f, h->bas_ne[0]);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, h->bas_ne[1]);
    TEST_ASSERT_EQUAL_FLOAT(10.0f, h->hedef_ne[1]);
    /* Yarıçap dışında ama bacağın sonu geçildi (kaçırma 1,12 m): biter. */
    konum(o, 11.0f, 10.5f);
    gd_ilerle(o, 100);
    TEST_ASSERT_EQUAL_UINT16(2u, o->g.mission_item);
    TEST_ASSERT_EQUAL_UINT8(IDA_MS_RUNNING, o->g.mission_state);
}

static void test_waypoint_zaman_asimi_ve_duraklama(void)
{
    gd_t *o = &O;
    gd_kur(o);
    ida_mission_item_t g[1]; /* MSVC C4204: sabit olmayan başlatıcı yok */
    g[0] = gd_oge((uint8_t)IDA_MI_WAYPOINT, (uint8_t)IDA_MI_FRAME_LOCAL, 0.5f, 0.5f, 10.0f, 0.0f, 0.0f);
    baslat(o, g, 1u);
    /* Süre yalnız RUNNING'de sayılır: 200 s operatör duraklaması aşıma girmez. */
    gd_ilerle(o, 10000);
    gd_kabul_bekle(gd_basit(o, (uint8_t)IDA_CMD_MISSION_CTL, (uint16_t)IDA_MCTL_PAUSE));
    gd_ilerle(o, 200000);
    TEST_ASSERT_EQUAL_UINT8(IDA_MS_PAUSED, o->g.mission_state);
    gd_kabul_bekle(gd_basit(o, (uint8_t)IDA_CMD_MISSION_CTL, (uint16_t)IDA_MCTL_RESUME));
    /* KAT·(10 m / 0,5 m/s + 0,1 m / 0,25 m/s + W/A) + PAY = 110,8 s; 10 s geçti. */
    asim_bekle(o, (KAT * ((10.0f / 0.5f) + (0.1f / 0.25f) + (0.25f / IVME))) + PAY - 10.0f,
               IDA_FSR_GEOFENCE);
    /* İptal mandalını FAILSAFE_RESET kaldırır. */
    gd_kabul_bekle(gd_basit(o, (uint8_t)IDA_CMD_FAILSAFE_RESET, 0u));
    gd_ilerle(o, 100);
    TEST_ASSERT_EQUAL_UINT8(IDA_FS_NONE, o->g.fs_level);
}

/* ---- DIVE_TO, SURFACE ---------------------------------------------------------------------- */

static void test_dive_to_bant_ve_zaman_asimi(void)
{
    gd_t *o = &O;
    gd_kur(o);
    ida_mission_item_t g[2]; /* MSVC C4204: sabit olmayan başlatıcı yok */
    g[0] = gd_oge((uint8_t)IDA_MI_DIVE_TO, (uint8_t)IDA_MI_FRAME_NONE, 0.2f, 0.2f, 0.0f, 0.0f, 2.0f);
    g[1] = gd_oge((uint8_t)IDA_MI_HOLD, (uint8_t)IDA_MI_FRAME_NONE, 600.0f, 5.0f, 0.0f, 0.0f, 2.0f);
    konum(o, 1.0f, 2.0f);
    o->gi.nav_yon = 0.7f;
    baslat(o, g, 2u);
    const ida_gor_hedef_t *h = &o->g.gv.hedef;
    TEST_ASSERT_EQUAL_UINT8(IDA_GOR_HY_TUT, h->yatay); /* bulunduğu yerde dalış */
    TEST_ASSERT_EQUAL_FLOAT(1.0f, h->hedef_ne[0]);
    TEST_ASSERT_EQUAL_FLOAT(2.0f, h->hedef_ne[1]);
    TEST_ASSERT_EQUAL_FLOAT(0.7f, h->yon);
    TEST_ASSERT_EQUAL_UINT8(IDA_GOR_HD_DERINLIK, h->dikey);
    TEST_ASSERT_EQUAL_FLOAT(2.0f, h->hedef_d);
    TEST_ASSERT_EQUAL_FLOAT(0.2f, h->dikey_hiz); /* min(p0, REF_DEP_RATE) */
    TEST_ASSERT_EQUAL_UINT8(IDA_PH_DIVE, gd_durum(o).phase);
    /* Bantta 0,5 s, bant dışı, yeniden bantta: süre baştan (BANT_MS = 1 s). */
    o->gi.nav_derinlik = 1.85f;
    gd_ilerle(o, 500);
    o->gi.nav_derinlik = 1.7f;
    gd_ilerle(o, 100);
    o->gi.nav_derinlik = 2.15f;
    gd_ilerle(o, 900);
    TEST_ASSERT_EQUAL_UINT16(0u, o->g.mission_item);
    gd_ilerle(o, 300);
    TEST_ASSERT_EQUAL_UINT16(1u, o->g.mission_item);
    /* Zaman aşımı: |Δd| = 1,9 m, w = 0,2 m/s → KAT·(9,5 + w/a 4) + BANT 1 s + PAY = 88 s. */
    gd_kur(o);
    baslat(o, g, 2u);
    asim_bekle(o, (KAT * ((1.9f / 0.2f) + (0.2f / IVME))) + 1.0f + PAY, IDA_FSR_GEOFENCE);
}

static void test_surface_bitis_ve_zaman_asimi(void)
{
    gd_t *o = &O;
    gd_kur(o);
    ida_mission_item_t g[2]; /* MSVC C4204: sabit olmayan başlatıcı yok */
    g[0] = gd_oge((uint8_t)IDA_MI_SURFACE, (uint8_t)IDA_MI_FRAME_NONE, 0.5f, 0.0f, 0.0f, 0.0f, 0.0f);
    g[1] = gd_oge((uint8_t)IDA_MI_HOLD, (uint8_t)IDA_MI_FRAME_NONE, 600.0f, 5.0f, 0.0f, 0.0f, 0.0f);
    o->gi.nav_derinlik = 2.0f;
    gd_ilerle(o, 100);
    baslat(o, g, 2u);
    const ida_gor_hedef_t *h = &o->g.gv.hedef;
    TEST_ASSERT_EQUAL_UINT8(IDA_GOR_HD_YUZEY, h->dikey);
    TEST_ASSERT_EQUAL_FLOAT(0.25f, h->dikey_hiz); /* min(0,5; REF_DEP_RATE 0,25) */
    TEST_ASSERT_EQUAL_UINT8(IDA_PH_ASCENT, gd_durum(o).phase);
    o->gi.nav_derinlik = 0.35f;
    gd_ilerle(o, 100);
    TEST_ASSERT_EQUAL_UINT16(0u, o->g.mission_item);
    o->gi.nav_derinlik = 0.25f; /* YUZEY_D = 0,3 m */
    gd_ilerle(o, 100);
    TEST_ASSERT_EQUAL_UINT16(1u, o->g.mission_item);
    /* Zaman aşımı: 2 m / 0,25 m/s → KAT·(8 + w/a 5) + PAY = 86 s. */
    gd_kur(o);
    o->gi.nav_derinlik = 2.0f;
    gd_ilerle(o, 100);
    baslat(o, g, 2u);
    asim_bekle(o, (KAT * ((2.0f / 0.25f) + (0.25f / IVME))) + PAY, IDA_FSR_GEOFENCE);
}

/* ---- HOLD ---------------------------------------------------------------------------------- */

static void test_hold_sure_olcum_zamaninda(void)
{
    gd_t *o = &O;
    gd_kur(o);
    ida_mission_item_t g[2]; /* MSVC C4204: sabit olmayan başlatıcı yok */
    g[0] = gd_oge((uint8_t)IDA_MI_HOLD, (uint8_t)IDA_MI_FRAME_LOCAL, 5.0f, 1.0f, 3.0f, 0.0f, 0.0f);
    g[1] = gd_oge((uint8_t)IDA_MI_HOLD, (uint8_t)IDA_MI_FRAME_NONE, 600.0f, 5.0f, 0.0f, 0.0f, 0.0f);
    baslat(o, g, 2u);
    TEST_ASSERT_EQUAL_UINT8(IDA_GOR_HY_TUT, o->g.gv.hedef.yatay);
    TEST_ASSERT_EQUAL_FLOAT(3.0f, o->g.gv.hedef.hedef_ne[0]);
    /* Tolerans dışında süre başlamaz. */
    gd_ilerle(o, 10000);
    TEST_ASSERT_EQUAL_UINT16(0u, o->g.mission_item);
    konum(o, 2.5f, 0.0f);
    gd_ilerle(o, 3000);
    /* Ölçüm zamanı durursa (duvar ilerler) süre dolmaz. */
    o->olcum_dondur = true;
    gd_ilerle(o, 10000);
    TEST_ASSERT_EQUAL_UINT16(0u, o->g.mission_item);
    o->olcum_dondur = false;
    /* Tolerans dışında süre birikmez ve öğe bitmez; birikmiş süre (2,9 s) sıfırlanmaz
       (düzeltme turu bulgu 3). */
    konum(o, 6.0f, 0.0f);
    gd_ilerle(o, 5000);
    TEST_ASSERT_EQUAL_UINT16(0u, o->g.mission_item);
    konum(o, 2.5f, 0.0f);
    gd_ilerle(o, 2000); /* yeniden giriş adımı + 19 × 0,1 s: 4,8 s */
    TEST_ASSERT_EQUAL_UINT16(0u, o->g.mission_item);
    gd_ilerle(o, 400);
    TEST_ASSERT_EQUAL_UINT16(1u, o->g.mission_item);
    /* Zaman aşımı: p0 + KAT·(3 m / LIM_V 1 m/s + 0,1 m / 0,25 m/s + w/a 5) + PAY = 81,8 s. */
    gd_kur(o);
    baslat(o, g, 2u);
    asim_bekle(o, 5.0f + (KAT * (3.0f + 0.4f + (0.25f / IVME))) + PAY, IDA_FSR_GEOFENCE);
}

/* Düzeltme turu bulgu 3: HOLD süresi yalnız RUNNING adımlarında ve araç tolerans içindeyken
   birikir; duraklamada birikim durur (sıfırlanmaz); öğe ancak birikmiş süre dolduğunda ve araç o
   an tolerans içindeyse biter. Dizi: 60 s HOLD → 1 s sonra PAUSE → MANUAL'da 200 m uzaklaşma →
   70 s sonra SET_MODE(AUTO). */
static void test_hold_suresi_duraklamada_birikmez(void)
{
    gd_t *o = &O;
    gd_kur(o);
    ida_mission_item_t g[2]; /* MSVC C4204: sabit olmayan başlatıcı yok */
    g[0] = gd_oge((uint8_t)IDA_MI_HOLD, (uint8_t)IDA_MI_FRAME_NONE, 60.0f, 5.0f, 0.0f, 0.0f, 0.0f);
    g[1] = gd_oge((uint8_t)IDA_MI_HOLD, (uint8_t)IDA_MI_FRAME_NONE, 600.0f, 5.0f, 0.0f, 0.0f, 0.0f);
    baslat(o, g, 2u);
    gd_ilerle(o, 1000);
    gd_kabul_bekle(gd_basit(o, (uint8_t)IDA_CMD_MISSION_CTL, (uint16_t)IDA_MCTL_PAUSE));
    o->e.mode = (uint8_t)IDA_MODE_MANUAL; /* daraltıcı kip (emniyet modülünün (b) adımı) */
    gd_ilerle(o, 100);
    konum(o, 200.0f, 0.0f);
    gd_ilerle(o, 70000);
    TEST_ASSERT_EQUAL_UINT8(IDA_MS_PAUSED, o->g.mission_state);
    gd_kabul_bekle(gd_basit(o, (uint8_t)IDA_CMD_SET_MODE, (uint16_t)IDA_MODE_AUTO));
    gd_ilerle(o, 1000);
    TEST_ASSERT_EQUAL_UINT8(IDA_MS_RUNNING, o->g.mission_state);
    TEST_ASSERT_EQUAL_UINT16(0u, o->g.mission_item); /* tolerans dışında bitmez */
    /* Toleransa dönüş: birikmiş ~1 s'ye 59 s eklenmeli. */
    konum(o, 0.0f, 0.0f);
    gd_ilerle(o, 57000);
    TEST_ASSERT_EQUAL_UINT16(0u, o->g.mission_item);
    gd_ilerle(o, 3000);
    TEST_ASSERT_EQUAL_UINT16(1u, o->g.mission_item);
    TEST_ASSERT_EQUAL_UINT8(IDA_MS_RUNNING, o->g.mission_state);
}

/* ---- WAIT_GNSS ----------------------------------------------------------------------------- */

static void test_wait_gnss(void)
{
    gd_t *o = &O;
    gd_kur(o);
    ida_mission_item_t g[2]; /* MSVC C4204: sabit olmayan başlatıcı yok */
    g[0] = gd_oge((uint8_t)IDA_MI_WAIT_GNSS, (uint8_t)IDA_MI_FRAME_NONE, 30.0f, 2.0f, 0.0f, 0.0f, 0.0f);
    g[1] = gd_oge((uint8_t)IDA_MI_HOLD, (uint8_t)IDA_MI_FRAME_NONE, 600.0f, 5.0f, 0.0f, 0.0f, 0.0f);
    o->gi.nav_derinlik = 1.0f;
    o->gi.gnss_taze = false;
    gd_ilerle(o, 100);
    baslat(o, g, 2u);
    TEST_ASSERT_EQUAL_UINT8(IDA_GOR_HD_YUZEY, o->g.gv.hedef.dikey);
    TEST_ASSERT_EQUAL_FLOAT(0.25f, o->g.gv.hedef.dikey_hiz); /* v_asc */
    TEST_ASSERT_EQUAL_UINT8(0u, o->g.gv.asama);
    o->gi.nav_derinlik = 0.1f;
    gd_ilerle(o, 20000); /* yüzeyde 20 s GNSS yok */
    TEST_ASSERT_EQUAL_UINT8(1u, o->g.gv.asama);
    TEST_ASSERT_EQUAL_UINT16(0u, o->g.mission_item);
    o->gi.gnss_taze = true;
    gd_ilerle(o, 100);
    TEST_ASSERT_EQUAL_UINT16(1u, o->g.mission_item);
    /* Zaman aşımı: yüzeye çıkıştan p0 = 30 s. */
    gd_kur(o);
    o->gi.gnss_taze = false;
    baslat(o, g, 2u);
    asim_bekle(o, 30.0f, IDA_FSR_GEOFENCE);
    /* İstenen fix DGPS (3): kestirim fix türünü taşımaz, doğrulanamaz → aşımda iptal. */
    gd_kur(o);
    ida_mission_item_t d = g[0];
    d.p[1] = 3.0f;
    baslat(o, &d, 1u);
    asim_bekle(o, 30.0f, IDA_FSR_GEOFENCE);
}

/* ---- SCAN_* (bu dilimde yürütülmez) -------------------------------------------------------- */

static void test_scan_yurutulmez(void)
{
    gd_t *o = &O;
    gd_kur(o);
    ida_mission_item_t s = gd_oge((uint8_t)IDA_MI_SCAN_LINES, (uint8_t)IDA_MI_FRAME_LOCAL, 20.0f,
                                  2.0f, 1.0f, 1.0f, 0.0f);
    s.p[2] = 3.0f;
    gd_yukle(o, &s, 1u);
    gd_auto(o);
    gd_ilerle(o, 100);
    iptal_bekle(o, IDA_FSR_GEOFENCE);
    gd_kur(o);
    s = gd_oge((uint8_t)IDA_MI_SCAN_POINT, (uint8_t)IDA_MI_FRAME_LOCAL, 30.0f, 1.0f, 2.0f, 2.0f,
               0.0f);
    gd_yukle(o, &s, 1u);
    gd_auto(o);
    gd_ilerle(o, 100);
    iptal_bekle(o, IDA_FSR_GEOFENCE);
}

/* ---- Kabul 5: arıza önceliği --------------------------------------------------------------- */

static const ida_mission_item_t UZUN = {0u, (uint8_t)IDA_MI_HOLD, (uint8_t)IDA_MI_FRAME_NONE,
                                        {600.0f, 5.0f, 0.0f, 0.0f}, 0, 0, 0.0f, 0.0f, 0.0f};

static void test_ariza_sizinti_ve_batarya_gorevi_iptal_eder(void)
{
    gd_t *o = &O;
    /* Sızıntı (AUTO: SURFACE): görev iptal, kip DEPTH_HOLD, SURFACE sürer. */
    gd_kur(o);
    baslat(o, &UZUN, 1u);
    o->besle_leak = false;
    for (int i = 0; i < 2; i++) {
        ida_sample_t s;
        (void)memset(&s, 0, sizeof(s));
        s.hdr.type = (uint8_t)IDA_SENS_LEAK;
        s.hdr.t_rx.ns = o->now;
        s.u.leak.mask = 1u;
        ida_gor_ornek(&o->g, &s);
    }
    gd_ilerle(o, 100);
    iptal_bekle(o, IDA_FSR_LEAK);
    /* Düşük batarya (02 §4.3 A: görev iptali + SURFACE). */
    gd_kur(o);
    baslat(o, &UZUN, 1u);
    o->batt_v = 13.0f;
    gd_ilerle(o, 10000);
    TEST_ASSERT_EQUAL_UINT8(IDA_MS_RUNNING, o->g.mission_state);
    gd_ilerle(o, 600);
    iptal_bekle(o, IDA_FSR_BATT_LOW);
    /* GCS_1 (AUTO: eylem yok): görev sürer. */
    gd_kur(o);
    baslat(o, &UZUN, 1u);
    o->besle_hb = false;
    gd_ilerle(o, 5000);
    TEST_ASSERT_EQUAL_UINT8(IDA_MS_RUNNING, o->g.mission_state);
    TEST_ASSERT_EQUAL_UINT8(IDA_FS_NONE, o->g.fs_level);
}

/* Düzeltme turu bulgu 4: iptal mandalının nedeni yalnız FAILSAFE_RESET'te temizlenir; yeni görevin
   yüklenmesi (MISSION_END) mandal sürerken nedeni varsayılana (GEOFENCE) çevirmez. */
static void test_iptal_nedeni_yeni_yuklemede_korunur(void)
{
    gd_t *o = &O;
    gd_kur(o);
    baslat(o, &UZUN, 1u); /* yüzeyde (0,1 m): AUTO × GCS_2 SURFACE */
    o->besle_hb = false;
    gd_ilerle(o, 61000);
    TEST_ASSERT_EQUAL_UINT8(IDA_MS_ABORTED, o->g.mission_state);
    o->besle_hb = true;
    gd_ilerle(o, 300);
    TEST_ASSERT_EQUAL_UINT8(IDA_FS_SURFACE, o->g.fs_level);
    TEST_ASSERT_EQUAL_HEX32(IDA_FSR_GCS_LOSS, o->g.fs_nedenleri);
    gd_yukle(o, &UZUN, 1u);
    gd_ilerle(o, 100);
    TEST_ASSERT_EQUAL_UINT8(IDA_FS_SURFACE, o->g.fs_level);
    TEST_ASSERT_EQUAL_HEX32(IDA_FSR_GCS_LOSS, o->g.fs_nedenleri);
    gd_kabul_bekle(gd_basit(o, (uint8_t)IDA_CMD_FAILSAFE_RESET, 0u));
    TEST_ASSERT_EQUAL_UINT32(0u, o->g.gv.iptal_nedeni);
    gd_ilerle(o, 100);
    TEST_ASSERT_EQUAL_UINT8(IDA_FS_NONE, o->g.fs_level);
}

/* Düzeltme turu bulgu 5: zaman aşımı BANT_MS'i ve ivme sınırlı referansın hızlanma ve
   yavaşlama süresini (w/a) sayar: ASIM_PAY_S = 0 iken yakın hedefli DIVE_TO bandını tamamlar. */
static void test_dive_to_yakin_hedef_paysiz(void)
{
    gd_t *o = &O;
    gd_kur(o);
    gd_param_u32(o, IDA_PRM_gor_asim_pay_s, 0u);
    ida_mission_item_t g[2]; /* MSVC C4204: sabit olmayan başlatıcı yok */
    g[0] = gd_oge((uint8_t)IDA_MI_DIVE_TO, (uint8_t)IDA_MI_FRAME_NONE, 1.0f, 0.2f, 0.0f, 0.0f, 0.55f);
    g[1] = gd_oge((uint8_t)IDA_MI_HOLD, (uint8_t)IDA_MI_FRAME_NONE, 600.0f, 5.0f, 0.0f, 0.0f, 0.55f);
    o->gi.nav_derinlik = 0.5f; /* hedef bandı (0,55 ± 0,2) içinde; |Δd| = 0,05 m */
    gd_ilerle(o, 100);
    baslat(o, g, 2u);
    gd_ilerle(o, 2000);
    TEST_ASSERT_EQUAL_UINT8(IDA_MS_RUNNING, o->g.mission_state);
    TEST_ASSERT_EQUAL_UINT16(1u, o->g.mission_item);
    /* Aşımın süresi: KAT·(|Δd|/w + w/a) + BANT; w = min(p0, REF_DEP_RATE) = 0,25, a = 0,05. */
    gd_kur(o);
    gd_param_u32(o, IDA_PRM_gor_asim_pay_s, 0u);
    o->gi.nav_derinlik = 1.5f; /* bant dışı (|1,5 − 0,55| > 0,2) ve orada kalır */
    gd_ilerle(o, 100);
    baslat(o, g, 2u);
    asim_bekle(o, (KAT * ((0.95f / 0.25f) + (0.25f / 0.05f))) + 1.0f, IDA_FSR_GEOFENCE);
}

static void test_ariza_konum_kaybi(void)
{
    gd_t *o = &O;
    gd_kur(o);
    baslat(o, &UZUN, 1u);
    /* Konum geçersiz: HOLD (görev durur, AUTO sürer, güdüm hedefi yok). */
    o->gi.gecerlilik = (uint8_t)(gd_gecerlilik_tam() & (uint8_t)(0xFFu ^ IDA_KES_G_KONUM));
    gd_ilerle(o, 100);
    TEST_ASSERT_EQUAL_UINT8(IDA_FS_HOLD, o->g.fs_level);
    TEST_ASSERT_EQUAL_UINT8(IDA_MS_PAUSED, o->g.mission_state);
    TEST_ASSERT_EQUAL_UINT8(IDA_MODE_AUTO, o->e.mode);
    TEST_ASSERT_EQUAL_UINT8(IDA_GOR_HY_YOK, o->g.gv.hedef.yatay);
    /* Konum döner: EST_OK_MS (2 s) sonra arıza kalkar, görev kaldığı yerden sürer. */
    o->gi.gecerlilik = gd_gecerlilik_tam();
    gd_ilerle(o, 1000);
    TEST_ASSERT_EQUAL_UINT8(IDA_MS_PAUSED, o->g.mission_state);
    gd_ilerle(o, 1500);
    TEST_ASSERT_EQUAL_UINT8(IDA_FS_NONE, o->g.fs_level);
    TEST_ASSERT_EQUAL_UINT8(IDA_MS_RUNNING, o->g.mission_state);
    /* 30 s kayıp: SURFACE, görev iptal. */
    o->gi.gecerlilik = (uint8_t)(gd_gecerlilik_tam() & (uint8_t)(0xFFu ^ IDA_KES_G_KONUM));
    gd_ilerle(o, 29500);
    TEST_ASSERT_EQUAL_UINT8(IDA_MS_PAUSED, o->g.mission_state);
    gd_ilerle(o, 700);
    iptal_bekle(o, IDA_FSR_EST_DEGRADED);
}

/* W5-K24: GNSS'siz kat edilen yol DR_YOL_MAX'ı aşınca görev duraklar, araç yüzeye çıkıp GNSS
   bekler; taze GNSS'le sürer; DR_BEKLE_S aşılırsa iptal. */
static void test_olu_hesap_yol_siniri(void)
{
    gd_t *o = &O;
    gd_kur(o);
    ida_mission_item_t g[1]; /* MSVC C4204: sabit olmayan başlatıcı yok */
    g[0] = gd_oge((uint8_t)IDA_MI_WAYPOINT, (uint8_t)IDA_MI_FRAME_LOCAL, 1.0f, 3.0f, 500.0f, 0.0f, 3.0f);
    baslat(o, g, 1u);
    o->gi.gnss_taze = false;
    o->gi.nav_derinlik = 3.0f;
    float n = 0.0f;
    while (n < 199.0f) {
        n += 5.0f;
        konum(o, n, 0.0f);
        gd_ilerle(o, 100);
        TEST_ASSERT_EQUAL_UINT8(IDA_MS_RUNNING, o->g.mission_state);
    }
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 200.0f, o->g.gv.dr_yol);
    konum(o, 205.0f, 0.0f);
    gd_ilerle(o, 100);
    TEST_ASSERT_EQUAL_UINT8(IDA_MS_PAUSED, o->g.mission_state);
    TEST_ASSERT_TRUE(o->g.gv.dr_bekliyor);
    const ida_gor_hedef_t *h = &o->g.gv.hedef;
    TEST_ASSERT_EQUAL_UINT8(IDA_GOR_HY_TUT, h->yatay);
    TEST_ASSERT_EQUAL_UINT8(IDA_GOR_HD_YUZEY, h->dikey);
    TEST_ASSERT_EQUAL_FLOAT(205.0f, h->hedef_ne[0]);
    TEST_ASSERT_EQUAL_UINT8(IDA_PH_ASCENT, gd_durum(o).phase);
    TEST_ASSERT_EQUAL_UINT8(IDA_FS_NONE, o->g.fs_level); /* arıza eylemi değil, görev davranışı */
    /* Su altında GNSS gelse de (yüzeyde değil) bekleme sürer. */
    o->gi.gnss_taze = true;
    gd_ilerle(o, 100);
    TEST_ASSERT_TRUE(o->g.gv.dr_bekliyor);
    o->gi.nav_derinlik = 0.1f;
    gd_ilerle(o, 100);
    TEST_ASSERT_FALSE(o->g.gv.dr_bekliyor);
    TEST_ASSERT_EQUAL_UINT8(IDA_MS_RUNNING, o->g.mission_state);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, o->g.gv.dr_yol);
    /* Bekleme aşımı (300 s): iptal, neden EST_DEGRADED. */
    o->gi.gnss_taze = false;
    o->gi.nav_derinlik = 3.0f;
    while (n < 410.0f) {
        n += 5.0f;
        konum(o, n, 0.0f);
        gd_ilerle(o, 100);
    }
    TEST_ASSERT_EQUAL_UINT8(IDA_MS_PAUSED, o->g.mission_state);
    gd_ilerle(o, 299500);
    TEST_ASSERT_EQUAL_UINT8(IDA_MS_PAUSED, o->g.mission_state);
    gd_ilerle(o, 700);
    iptal_bekle(o, IDA_FSR_EST_DEGRADED);
}

/* Ölü hesap beklemesi yalnız AUTO'da sürer: kipten çıkınca bırakılır, süre dolsa da iptal yok;
   AUTO'ya dönüşte yol hâlâ sınırın üstündeyse bekleme yeniden başlar. */
static void test_olu_hesap_kipten_cikis(void)
{
    gd_t *o = &O;
    gd_kur(o);
    ida_mission_item_t g[1]; /* MSVC C4204: sabit olmayan başlatıcı yok */
    g[0] = gd_oge((uint8_t)IDA_MI_WAYPOINT, (uint8_t)IDA_MI_FRAME_LOCAL, 1.0f, 3.0f, 500.0f, 0.0f, 3.0f);
    baslat(o, g, 1u);
    o->gi.gnss_taze = false;
    o->gi.nav_derinlik = 3.0f;
    for (float n = 5.0f; n <= 210.0f; n += 5.0f) {
        konum(o, n, 0.0f);
        gd_ilerle(o, 100);
    }
    TEST_ASSERT_TRUE(o->g.gv.dr_bekliyor);
    gd_kabul_bekle(gd_basit(o, (uint8_t)IDA_CMD_SET_MODE, (uint16_t)IDA_MODE_GUIDED));
    gd_ilerle(o, 400000);
    TEST_ASSERT_FALSE(o->g.gv.dr_bekliyor);
    TEST_ASSERT_EQUAL_UINT8(IDA_MS_PAUSED, o->g.mission_state);
    TEST_ASSERT_EQUAL_UINT8(IDA_FS_NONE, o->g.fs_level);
    gd_kabul_bekle(gd_basit(o, (uint8_t)IDA_CMD_SET_MODE, (uint16_t)IDA_MODE_AUTO));
    gd_ilerle(o, 100);
    TEST_ASSERT_TRUE(o->g.gv.dr_bekliyor);
    TEST_ASSERT_EQUAL_UINT8(IDA_MS_PAUSED, o->g.mission_state);
}

static void test_cografi_sinir(void)
{
    gd_t *o = &O;
    gd_kur(o);
    baslat(o, &UZUN, 1u);
    /* Yarıçap (1000 m) dışı: AUTO'da SURFACE, görev iptal; mandallı. */
    konum(o, 800.0f, 700.0f);
    gd_ilerle(o, 100);
    iptal_bekle(o, IDA_FSR_GEOFENCE);
    TEST_ASSERT_TRUE((o->g.mandal & IDA_GOR_K_BIT(IDA_GOR_K_COGRAFI)) != 0u);
    /* Sınır dışındayken FAILSAFE_RESET reddedilir; içeride kabul. */
    gd_ret_bekle(gd_basit(o, (uint8_t)IDA_CMD_FAILSAFE_RESET, 0u),
                 (uint16_t)IDA_REASON_NOT_ARMABLE);
    konum(o, 100.0f, 0.0f);
    gd_ilerle(o, 100);
    gd_kabul_bekle(gd_basit(o, (uint8_t)IDA_CMD_FAILSAFE_RESET, 0u));
    gd_ilerle(o, 100);
    TEST_ASSERT_EQUAL_UINT8(IDA_FS_NONE, o->g.fs_level);
    /* Toplam görev süresi (duvar; GOREV_AZAMI_S = 60 s): duraklama da sayılır. */
    gd_kur(o);
    gd_param_u32(o, IDA_PRM_gor_gorev_azami_s, 60u);
    baslat(o, &UZUN, 1u);
    gd_kabul_bekle(gd_basit(o, (uint8_t)IDA_CMD_MISSION_CTL, (uint16_t)IDA_MCTL_PAUSE));
    gd_ilerle(o, 59000);
    TEST_ASSERT_EQUAL_UINT8(IDA_MS_PAUSED, o->g.mission_state);
    TEST_ASSERT_EQUAL_UINT8(IDA_FS_NONE, o->g.fs_level);
    gd_ilerle(o, 1000);
    iptal_bekle(o, IDA_FSR_GEOFENCE);
    /* MANUAL/DEPTH_HOLD sütunu WARN: kip düşmez, görev yok. */
    gd_kur(o);
    gd_kabul_bekle(gd_basit(o, (uint8_t)IDA_CMD_ARM, 0u));
    o->e.mode = (uint8_t)IDA_MODE_DEPTH_HOLD;
    konum(o, 1500.0f, 0.0f);
    gd_ilerle(o, 100);
    TEST_ASSERT_EQUAL_UINT8(IDA_FS_WARN, o->g.fs_level);
    TEST_ASSERT_EQUAL_HEX32(IDA_FSR_GEOFENCE, o->g.fs_nedenleri);
}

static void test_en_az_irtifa(void)
{
    gd_t *o = &O;
    gd_kur(o);
    baslat(o, &UZUN, 1u);
    o->gi.irtifa_var = true;
    o->gi.irtifa = 0.1f; /* ALT_MIN 0,3 m */
    gd_ilerle(o, 100);
    TEST_ASSERT_EQUAL_UINT8(IDA_FS_WARN, o->g.fs_level);
    TEST_ASSERT_EQUAL_HEX32(IDA_FSR_ALT_MIN, o->g.fs_nedenleri);
    TEST_ASSERT_TRUE(o->g.irtifa_kisit);
    TEST_ASSERT_EQUAL_UINT8(IDA_MS_RUNNING, o->g.mission_state); /* WARN görevi durdurmaz */
    o->gi.irtifa = 1.0f;
    gd_ilerle(o, 100);
    TEST_ASSERT_EQUAL_UINT8(IDA_FS_NONE, o->g.fs_level); /* otomatik temizlenir */
    TEST_ASSERT_FALSE(o->g.irtifa_kisit);
    /* İrtifa bilinmiyor (−1): koşul yok. */
    o->gi.irtifa = -1.0f;
    gd_ilerle(o, 100);
    TEST_ASSERT_FALSE(o->g.irtifa_kisit);
}

/* Görev hiçbir koşulda kip ya da silahlandırma genişletmez: silahsız ve AUTO dışında yürüyen
   görev yoktur; görev komutları kipi değiştirmez. */
static void test_gorev_genisletmez(void)
{
    gd_t *o = &O;
    gd_kur(o);
    gd_yukle(o, &UZUN, 1u);
    for (uint8_t a = 1u; a <= 4u; a++) {
        (void)gd_basit(o, (uint8_t)IDA_CMD_MISSION_CTL, a);
        gd_ilerle(o, 100);
        TEST_ASSERT_FALSE(o->e.armed);
        TEST_ASSERT_EQUAL_UINT8(IDA_MODE_DISARMED, o->e.mode);
        TEST_ASSERT_TRUE(o->g.mission_state != (uint8_t)IDA_MS_RUNNING);
    }
}

/* ---- Basit kinematik modelle görev (yüzeyde iki yol noktası, dalış, su altında yol noktası,
   yüzeye çıkış). Model: yön en çok 0,35 rad/s ile güdümün yönüne döner; yatay hız güdümün
   isteğini 1 s zaman sabitiyle izler (1 m/s sınırı); derinlik güdümün referansını 1 s zaman
   sabitiyle izler. Güdüm testteki saf işlevlerle hesaplanır (gudum_oto.c). ---- */

typedef struct {
    float v[2];
    ida_gud_ref_t ref;
    bool ref_var;
    float y_int;
    uint32_t kusak;
} model_t;

static void model_adim(gd_t *o, model_t *m, float dt)
{
    const ida_gor_hedef_t *h = &o->g.gv.hedef;
    float hiz[2] = {0.0f, 0.0f};
    float yon = o->gi.nav_yon;
    if ((h->yatay != (uint8_t)IDA_GOR_HY_YOK) && (o->g.mission_state == (uint8_t)IDA_MS_RUNNING)) {
        if (!m->ref_var) {
            m->ref.d = o->gi.nav_derinlik;
            m->ref.w = 0.0f;
            m->ref_var = true;
        }
        if (m->kusak != h->kusak) {
            m->y_int = 0.0f;
            m->kusak = h->kusak;
        }
        ida_gud_oto_yatay(h, o->gi.nav_ne, o->gi.nav_yon, m->y_int, 3.0f, 0.2f, 0.3f, hiz, &yon);
        float r = 0.0f;
        float y = 0.0f;
        float s = 0.0f;
        if ((h->yatay == (uint8_t)IDA_GOR_HY_YOL) &&
            ida_gud_ilos(h->bas_ne, h->hedef_ne, o->gi.nav_ne, 3.0f, 0.2f, m->y_int, &r, &y, &s)) {
            ida_gud_ilos_integral(&m->y_int, y, 3.0f, 0.2f, dt);
        }
        const float hd = (h->dikey == (uint8_t)IDA_GOR_HD_YUZEY) ? 0.0f : h->hedef_d;
        ida_gud_ref_adim(&m->ref, hd, h->dikey_hiz, IVME, dt);
    }
    const float dpsi = ida_aci_sar(yon - o->gi.nav_yon);
    const float lim = 0.35f * dt;
    o->gi.nav_yon = ida_aci_sar(o->gi.nav_yon + ((dpsi > lim) ? lim : ((dpsi < -lim) ? -lim : dpsi)));
    for (uint32_t i = 0u; i < 2u; i++) {
        float hz = hiz[i];
        hz = (hz > 1.0f) ? 1.0f : ((hz < -1.0f) ? -1.0f : hz);
        m->v[i] += (hz - m->v[i]) * dt;
        o->gi.nav_ne[i] += m->v[i] * dt;
    }
    if (m->ref_var) {
        o->gi.nav_derinlik += (m->ref.d - o->gi.nav_derinlik) * dt;
    }
}

static void test_model_ile_gorev(void)
{
    gd_t *o = &O;
    gd_kur(o);
    ida_mission_item_t g[5]; /* MSVC C4204: sabit olmayan başlatıcı yok */
    g[0] = gd_oge((uint8_t)IDA_MI_WAYPOINT, (uint8_t)IDA_MI_FRAME_LOCAL, 0.5f, 0.4f, 6.0f, 0.0f, 0.0f);
    g[1] = gd_oge((uint8_t)IDA_MI_WAYPOINT, (uint8_t)IDA_MI_FRAME_LOCAL, 0.5f, 0.4f, 6.0f, 4.0f, 0.0f);
    g[2] = gd_oge((uint8_t)IDA_MI_DIVE_TO, (uint8_t)IDA_MI_FRAME_NONE, 0.2f, 0.2f, 0.0f, 0.0f, 1.5f);
    g[3] = gd_oge((uint8_t)IDA_MI_WAYPOINT, (uint8_t)IDA_MI_FRAME_LOCAL, 0.5f, 0.3f, 0.0f, 4.0f, 1.5f);
    g[4] = gd_oge((uint8_t)IDA_MI_SURFACE, (uint8_t)IDA_MI_FRAME_NONE, 0.2f, 0.0f, 0.0f, 0.0f, 0.0f);
    gd_yukle(o, g, 5u);
    gd_auto(o);
    model_t m;
    (void)memset(&m, 0, sizeof(m));
    uint16_t en_cok = 0u;
    float kacirma[5] = {99.0f, 99.0f, 99.0f, 99.0f, 99.0f};
    int adim = 0;
    while ((o->g.mission_state == (uint8_t)IDA_MS_RUNNING) && (adim < 3000)) {
        model_adim(o, &m, 0.1f);
        const uint16_t i = o->g.mission_item;
        if (g[i].item_type == (uint8_t)IDA_MI_WAYPOINT) {
            const float dn = o->gi.nav_ne[0] - g[i].north_m;
            const float de = o->gi.nav_ne[1] - g[i].east_m;
            const float d = ida_karekok((dn * dn) + (de * de));
            if (d < kacirma[i]) {
                kacirma[i] = d;
            }
        }
        gd_ilerle(o, 100);
        en_cok = (o->g.mission_item > en_cok) ? o->g.mission_item : en_cok;
        adim++;
    }
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(IDA_MS_COMPLETE, o->g.mission_state, "görev tamamlanmalı");
    TEST_ASSERT_EQUAL_UINT16(4u, en_cok);
    TEST_ASSERT_TRUE(kacirma[0] <= 0.5f);
    TEST_ASSERT_TRUE(kacirma[1] <= 0.5f);
    TEST_ASSERT_TRUE(kacirma[3] <= 0.5f);
    TEST_ASSERT_TRUE(o->gi.nav_derinlik <= 0.3f);
    TEST_ASSERT_EQUAL_UINT8(IDA_FS_NONE, o->g.fs_level);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_waypoint_kabul_yaricapi_ve_gecis);
    RUN_TEST(test_waypoint_zaman_asimi_ve_duraklama);
    RUN_TEST(test_dive_to_bant_ve_zaman_asimi);
    RUN_TEST(test_surface_bitis_ve_zaman_asimi);
    RUN_TEST(test_hold_sure_olcum_zamaninda);
    RUN_TEST(test_hold_suresi_duraklamada_birikmez);
    RUN_TEST(test_wait_gnss);
    RUN_TEST(test_scan_yurutulmez);
    RUN_TEST(test_ariza_sizinti_ve_batarya_gorevi_iptal_eder);
    RUN_TEST(test_iptal_nedeni_yeni_yuklemede_korunur);
    RUN_TEST(test_dive_to_yakin_hedef_paysiz);
    RUN_TEST(test_ariza_konum_kaybi);
    RUN_TEST(test_olu_hesap_yol_siniri);
    RUN_TEST(test_olu_hesap_kipten_cikis);
    RUN_TEST(test_cografi_sinir);
    RUN_TEST(test_en_az_irtifa);
    RUN_TEST(test_gorev_genisletmez);
    RUN_TEST(test_model_ile_gorev);
    return UNITY_END();
}
