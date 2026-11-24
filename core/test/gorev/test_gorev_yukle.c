/*
 * test_gorev_yukle.c — W5-gorev-yurutucu kabul 1 (görev yüklemesi, A4-K51, A1 Tablo 3.6a) ve
 * kabul 2 (görev durum makinesi; her geçiş ve yasak geçiş; status alanları).
 *
 * Beklenen değerler kaynağın tablolarından değil, testteki kurallardan (A1 Tablo 3.6a'nın
 * satırları, A4 3.17'nin MISSION_* satırları) çıkar.
 */
#include "gorev_duzenek.h"

void setUp(void);
void tearDown(void);
void setUp(void) {}
void tearDown(void) {}

static gd_t O;

static float nan_f(void)
{
    const uint32_t u = 0x7FC00000u;
    float f = 0.0f;
    (void)memcpy(&f, &u, sizeof(f));
    return f;
}

#define PI_F_UST 3.14159274f /* float π (0x40490FDB) > π: aralık dışı */
#define PI_ALT 3.14159250f   /* π'nin altındaki en büyük float          */

/* ---- Tablo 3.6a: tür başına sınır değerleri ------------------------------------------------ */

static ida_mission_item_t wp(void)
{
    return gd_oge((uint8_t)IDA_MI_WAYPOINT, (uint8_t)IDA_MI_FRAME_LOCAL, 1.0f, 0.5f, 3.0f, 4.0f,
                  0.0f);
}

typedef struct {
    ida_mission_item_t m;
    bool gecerli;
} vaka_t;

static void vaka_dene(const vaka_t *v, uint32_t n)
{
    for (uint32_t i = 0u; i < n; i++) {
        TEST_ASSERT_EQUAL_MESSAGE(v[i].gecerli, ida_gor_oge_gecerli(&v[i].m),
                                  "Tablo 3.6a vakası (dizideki sırasıyla)");
    }
}

static void test_tablo36a_waypoint(void)
{
    vaka_t v[20];
    uint32_t n = 0u;
    v[n].m = wp(); v[n++].gecerli = true;
    v[n].m = wp(); v[n].m.p[0] = 0.1f; v[n++].gecerli = true;
    v[n].m = wp(); v[n].m.p[0] = 50.0f; v[n++].gecerli = true;
    v[n].m = wp(); v[n].m.p[0] = 0.09f; v[n++].gecerli = false;
    v[n].m = wp(); v[n].m.p[0] = 50.1f; v[n++].gecerli = false;
    v[n].m = wp(); v[n].m.p[1] = 0.05f; v[n++].gecerli = true;
    v[n].m = wp(); v[n].m.p[1] = 3.01f; v[n++].gecerli = false;
    v[n].m = wp(); v[n].m.p[2] = 1.0f; v[n++].gecerli = false;     /* "0" yazan p[2] */
    v[n].m = wp(); v[n].m.depth = 100.0f; v[n++].gecerli = true;
    v[n].m = wp(); v[n].m.depth = 100.5f; v[n++].gecerli = false;
    v[n].m = wp(); v[n].m.depth = -0.1f; v[n++].gecerli = false;
    v[n].m = wp(); v[n].m.frame = (uint8_t)IDA_MI_FRAME_NONE; v[n++].gecerli = false;
    v[n].m = wp(); v[n].m.lat_e7 = 1; v[n++].gecerli = false;      /* LOCAL'de lat 0 değil */
    v[n].m = wp(); v[n].m.north_m = 10000.5f; v[n++].gecerli = false;
    v[n].m = wp(); v[n].m.east_m = nan_f(); v[n++].gecerli = false; /* NaN */
    /* GLOBAL: north/east 0, lat/lon aralıkta */
    v[n].m = wp(); v[n].m.frame = (uint8_t)IDA_MI_FRAME_GLOBAL; v[n].m.north_m = 0.0f;
    v[n].m.east_m = 0.0f; v[n].m.lat_e7 = 900000000; v[n].m.lon_e7 = -1800000000;
    v[n++].gecerli = true;
    vaka_dene(v, n);
    ida_mission_item_t m = wp();
    m.frame = (uint8_t)IDA_MI_FRAME_GLOBAL;
    m.north_m = 0.0f;
    m.east_m = 0.0f;
    m.lat_e7 = 900000001;
    TEST_ASSERT_FALSE(ida_gor_oge_gecerli(&m));
    m.lat_e7 = 0;
    m.east_m = 1.0f; /* GLOBAL'de east 0 değil */
    TEST_ASSERT_FALSE(ida_gor_oge_gecerli(&m));
}

static void test_tablo36a_diger_turler(void)
{
    const uint8_t NONE = (uint8_t)IDA_MI_FRAME_NONE;
    const uint8_t LOC = (uint8_t)IDA_MI_FRAME_LOCAL;
    vaka_t v[40];
    uint32_t n = 0u;
    /* DIVE_TO: NONE; depth > 0 – 100; p0 0,05–1; p1 0,05–5 */
    v[n].m = gd_oge((uint8_t)IDA_MI_DIVE_TO, NONE, 0.05f, 5.0f, 0.0f, 0.0f, 100.0f); v[n++].gecerli = true;
    v[n].m = gd_oge((uint8_t)IDA_MI_DIVE_TO, NONE, 1.0f, 0.05f, 0.0f, 0.0f, 0.01f); v[n++].gecerli = true;
    v[n].m = gd_oge((uint8_t)IDA_MI_DIVE_TO, NONE, 0.5f, 0.5f, 0.0f, 0.0f, 0.0f); v[n++].gecerli = false;
    v[n].m = gd_oge((uint8_t)IDA_MI_DIVE_TO, NONE, 1.01f, 0.5f, 0.0f, 0.0f, 1.0f); v[n++].gecerli = false;
    v[n].m = gd_oge((uint8_t)IDA_MI_DIVE_TO, LOC, 0.5f, 0.5f, 0.0f, 0.0f, 1.0f); v[n++].gecerli = false;
    v[n].m = gd_oge((uint8_t)IDA_MI_DIVE_TO, NONE, 0.5f, 0.5f, 1.0f, 0.0f, 1.0f); v[n++].gecerli = false; /* NONE'da north */
    /* SURFACE: NONE; depth 0; p0 0,05–1; p1..p3 0 */
    v[n].m = gd_oge((uint8_t)IDA_MI_SURFACE, NONE, 0.05f, 0.0f, 0.0f, 0.0f, 0.0f); v[n++].gecerli = true;
    v[n].m = gd_oge((uint8_t)IDA_MI_SURFACE, NONE, 0.25f, 0.1f, 0.0f, 0.0f, 0.0f); v[n++].gecerli = false;
    v[n].m = gd_oge((uint8_t)IDA_MI_SURFACE, NONE, 0.25f, 0.0f, 0.0f, 0.0f, 0.5f); v[n++].gecerli = false;
    v[n].m = gd_oge((uint8_t)IDA_MI_SURFACE, NONE, 0.04f, 0.0f, 0.0f, 0.0f, 0.0f); v[n++].gecerli = false;
    /* HOLD: NONE/LOCAL/GLOBAL; depth 0–100; p0 > 0 – 3600; p1 0,1–50 */
    v[n].m = gd_oge((uint8_t)IDA_MI_HOLD, NONE, 3600.0f, 0.1f, 0.0f, 0.0f, 0.0f); v[n++].gecerli = true;
    v[n].m = gd_oge((uint8_t)IDA_MI_HOLD, LOC, 1.0f, 50.0f, 2.0f, 2.0f, 5.0f); v[n++].gecerli = true;
    v[n].m = gd_oge((uint8_t)IDA_MI_HOLD, NONE, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f); v[n++].gecerli = false;
    v[n].m = gd_oge((uint8_t)IDA_MI_HOLD, NONE, 3600.5f, 1.0f, 0.0f, 0.0f, 0.0f); v[n++].gecerli = false;
    v[n].m = gd_oge((uint8_t)IDA_MI_HOLD, NONE, 10.0f, 0.09f, 0.0f, 0.0f, 0.0f); v[n++].gecerli = false;
    /* SCAN_LINES: LOCAL/GLOBAL; p0 1–500; p1 0,5–100; p2 tamsayı 1–100; p3 (−π, π] */
    v[n].m = gd_oge((uint8_t)IDA_MI_SCAN_LINES, LOC, 1.0f, 0.5f, 1.0f, 1.0f, 0.0f);
    v[n].m.p[2] = 100.0f; v[n].m.p[3] = PI_ALT; v[n++].gecerli = true;
    v[n].m = gd_oge((uint8_t)IDA_MI_SCAN_LINES, LOC, 500.0f, 100.0f, 0.0f, 0.0f, 100.0f);
    v[n].m.p[2] = 1.0f; v[n].m.p[3] = -PI_ALT; v[n++].gecerli = true;
    v[n].m = gd_oge((uint8_t)IDA_MI_SCAN_LINES, LOC, 10.0f, 1.0f, 0.0f, 0.0f, 0.0f);
    v[n].m.p[2] = 2.5f; v[n++].gecerli = false; /* tamsayı değerli değil */
    v[n].m = gd_oge((uint8_t)IDA_MI_SCAN_LINES, LOC, 10.0f, 1.0f, 0.0f, 0.0f, 0.0f);
    v[n].m.p[2] = 2.0f; v[n].m.p[3] = PI_F_UST; v[n++].gecerli = false;
    v[n].m = gd_oge((uint8_t)IDA_MI_SCAN_LINES, LOC, 10.0f, 1.0f, 0.0f, 0.0f, 0.0f);
    v[n].m.p[2] = 0.0f; v[n++].gecerli = false;
    v[n].m = gd_oge((uint8_t)IDA_MI_SCAN_LINES, NONE, 10.0f, 1.0f, 0.0f, 0.0f, 0.0f);
    v[n].m.p[2] = 2.0f; v[n++].gecerli = false;
    /* SCAN_POINT: LOCAL/GLOBAL; p0 > 0 – 3600; p1 0,1–50 */
    v[n].m = gd_oge((uint8_t)IDA_MI_SCAN_POINT, LOC, 60.0f, 1.0f, 3.0f, 3.0f, 2.0f); v[n++].gecerli = true;
    v[n].m = gd_oge((uint8_t)IDA_MI_SCAN_POINT, LOC, 60.0f, 1.0f, 3.0f, 3.0f, 2.0f);
    v[n].m.p[2] = 1.0f; v[n++].gecerli = false;
    /* WAIT_GNSS: NONE; depth 0; p0 > 0 – 3600; p1 tamsayı 1–4 */
    v[n].m = gd_oge((uint8_t)IDA_MI_WAIT_GNSS, NONE, 3600.0f, 4.0f, 0.0f, 0.0f, 0.0f); v[n++].gecerli = true;
    v[n].m = gd_oge((uint8_t)IDA_MI_WAIT_GNSS, NONE, 10.0f, 1.0f, 0.0f, 0.0f, 0.0f); v[n++].gecerli = true;
    v[n].m = gd_oge((uint8_t)IDA_MI_WAIT_GNSS, NONE, 10.0f, 5.0f, 0.0f, 0.0f, 0.0f); v[n++].gecerli = false;
    v[n].m = gd_oge((uint8_t)IDA_MI_WAIT_GNSS, NONE, 10.0f, 1.5f, 0.0f, 0.0f, 0.0f); v[n++].gecerli = false;
    v[n].m = gd_oge((uint8_t)IDA_MI_WAIT_GNSS, NONE, 10.0f, 2.0f, 0.0f, 0.0f, 1.0f); v[n++].gecerli = false;
    /* Kapalı liste: 0, 8 (eski TARGET_ALIGN), 255 */
    v[n].m = wp(); v[n].m.item_type = 0u; v[n++].gecerli = false;
    v[n].m = wp(); v[n].m.item_type = 8u; v[n++].gecerli = false;
    v[n].m = wp(); v[n].m.item_type = 255u; v[n++].gecerli = false;
    v[n].m = wp(); v[n].m.frame = 3u; v[n++].gecerli = false;
    v[n].m = wp(); v[n].m.frame = 200u; v[n++].gecerli = false;
    vaka_dene(v, n);
}

/* ---- Kabul 1: atomik yükleme -------------------------------------------------------------- */

/* Deponun tamamı (düzeltme turu bulgu 8): yürürlükteki görev, yürütme ve durum alanları, güdüm
   hedefi ve modülün geri kalanı; yalnız açık yükleme tamponu ve sayaçları değişebilir. */
static void depo_esit(const ida_gor_t *a, const ida_gor_t *b)
{
    static ida_gor_t x;
    (void)memcpy(&x, b, sizeof(x));
    (void)memcpy(x.gv.yuk, a->gv.yuk, sizeof(x.gv.yuk));
    x.gv.yuk_n = a->gv.yuk_n;
    x.gv.yuk_alinan = a->gv.yuk_alinan;
    x.gv.yuk_acik = a->gv.yuk_acik;
    TEST_ASSERT_EQUAL_UINT8(a->mission_state, b->mission_state);
    TEST_ASSERT_EQUAL_UINT16(a->mission_item, b->mission_item);
    TEST_ASSERT_EQUAL_UINT16(a->gv.n, b->gv.n);
    TEST_ASSERT_EQUAL_MEMORY(a->gv.oge, b->gv.oge, sizeof(a->gv.oge));
    TEST_ASSERT_EQUAL_MEMORY(a, &x, sizeof(x));
}

static void test_gecerli_yukleme(void)
{
    gd_t *o = &O;
    gd_kur(o);
    TEST_ASSERT_EQUAL_UINT8(IDA_MS_IDLE, gd_durum(o).mission_state);
    ida_mission_item_t g[3]; /* MSVC C4204: sabit olmayan başlatıcı yok */
    g[0] = wp();
    g[1] = gd_oge((uint8_t)IDA_MI_DIVE_TO, (uint8_t)IDA_MI_FRAME_NONE, 0.2f, 0.2f, 0.0f, 0.0f, 2.0f);
    g[2] = gd_oge((uint8_t)IDA_MI_SURFACE, (uint8_t)IDA_MI_FRAME_NONE, 0.25f, 0.0f, 0.0f, 0.0f, 0.0f);
    gd_yukle(o, g, 3u);
    const ida_status_t st = gd_durum(o);
    TEST_ASSERT_EQUAL_UINT8(IDA_MS_LOADED, st.mission_state);
    TEST_ASSERT_EQUAL_UINT16(0u, st.mission_item);
    TEST_ASSERT_EQUAL_UINT16(3u, o->g.gv.n);
    TEST_ASSERT_EQUAL_UINT8(IDA_MI_DIVE_TO, o->g.gv.oge[1].tur);
    TEST_ASSERT_EQUAL_FLOAT(2.0f, o->g.gv.oge[1].derinlik);
    TEST_ASSERT_EQUAL_FLOAT(3.0f, o->g.gv.oge[0].konum.ne[0]);
    TEST_ASSERT_FALSE(o->g.gv.yuk_acik);
    /* Görev komutları nötrdür: kip ve silahlandırma değişmez. */
    TEST_ASSERT_EQUAL_UINT8(IDA_MODE_DISARMED, o->e.mode);
    TEST_ASSERT_FALSE(o->e.armed);
    /* 128 öğe sınırda kabul (GLOBAL dahil). */
    gd_kabul_bekle(gd_basit(o, (uint8_t)IDA_CMD_MISSION_BEGIN, 128u));
    for (uint16_t i = 0u; i < 128u; i++) {
        ida_mission_item_t m = gd_oge((uint8_t)IDA_MI_HOLD, (uint8_t)IDA_MI_FRAME_NONE, 1.0f, 1.0f,
                                      0.0f, 0.0f, 0.0f);
        m.index = i;
        gd_kabul_bekle(gd_oge_gonder(o, &m));
    }
    gd_kabul_bekle(gd_basit(o, (uint8_t)IDA_CMD_MISSION_END, 0u));
    TEST_ASSERT_EQUAL_UINT16(128u, o->g.gv.n);
    TEST_ASSERT_TRUE(ida_gor_tutarli(&o->g));
}

/* Her geçersizlik türü: komut OUT_OF_RANGE (ya da BAD_MODE) alır, yükleme bütünüyle reddedilir
   ve yürürlükteki görev değişmez. */
static void test_gecersiz_yuklemeler_depoyu_degistirmez(void)
{
    gd_t *o = &O;
    gd_kur(o);
    ida_mission_item_t g[2]; /* MSVC C4204: sabit olmayan başlatıcı yok */
    g[0] = wp();
    g[1] = gd_oge((uint8_t)IDA_MI_SURFACE, (uint8_t)IDA_MI_FRAME_NONE, 0.25f, 0.0f, 0.0f, 0.0f, 0.0f);
    gd_yukle(o, g, 2u);
    static ida_gor_t once;
    (void)memcpy(&once, &o->g, sizeof(once));

    /* (a) sayı: 0 ve 129 */
    gd_ret_bekle(gd_basit(o, (uint8_t)IDA_CMD_MISSION_BEGIN, 0u), (uint16_t)IDA_REASON_OUT_OF_RANGE);
    gd_ret_bekle(gd_basit(o, (uint8_t)IDA_CMD_MISSION_BEGIN, 129u),
                 (uint16_t)IDA_REASON_OUT_OF_RANGE);
    /* (b) açık yükleme yokken ITEM ve END */
    ida_mission_item_t m = wp();
    gd_ret_bekle(gd_oge_gonder(o, &m), (uint16_t)IDA_REASON_OUT_OF_RANGE);
    gd_ret_bekle(gd_basit(o, (uint8_t)IDA_CMD_MISSION_END, 0u), (uint16_t)IDA_REASON_OUT_OF_RANGE);
    depo_esit(&once, &o->g);

    /* (c) tür, (d) aralık, (e) sıra, (f) indis ≥ count: öğe reddi yüklemeyi kapatır; sonraki
       ITEM ve END de reddedilir. */
    for (uint32_t tur = 0u; tur < 4u; tur++) {
        gd_kabul_bekle(gd_basit(o, (uint8_t)IDA_CMD_MISSION_BEGIN, 2u));
        m = wp();
        m.index = 0u;
        gd_kabul_bekle(gd_oge_gonder(o, &m));
        m = wp();
        m.index = 1u;
        if (tur == 0u) {
            m.item_type = 8u; /* eski TARGET_ALIGN */
        } else if (tur == 1u) {
            m.p[0] = 60.0f; /* kabul yarıçapı aralık dışı */
        } else if (tur == 2u) {
            m.index = 0u; /* sıra (yinelenen indis) */
        } else {
            m.index = 2u; /* count dışı */
        }
        gd_ret_bekle(gd_oge_gonder(o, &m), (uint16_t)IDA_REASON_OUT_OF_RANGE);
        TEST_ASSERT_FALSE(o->g.gv.yuk_acik);
        m = wp();
        m.index = 1u;
        gd_ret_bekle(gd_oge_gonder(o, &m), (uint16_t)IDA_REASON_OUT_OF_RANGE);
        gd_ret_bekle(gd_basit(o, (uint8_t)IDA_CMD_MISSION_END, 0u),
                     (uint16_t)IDA_REASON_OUT_OF_RANGE);
        TEST_ASSERT_EQUAL_UINT8(IDA_MS_LOADED, o->g.mission_state);
        depo_esit(&once, &o->g);
    }
    /* (g) yarım yükleme: END sayı tutmuyor */
    gd_kabul_bekle(gd_basit(o, (uint8_t)IDA_CMD_MISSION_BEGIN, 3u));
    m = wp();
    m.index = 0u;
    gd_kabul_bekle(gd_oge_gonder(o, &m));
    m.index = 1u;
    gd_kabul_bekle(gd_oge_gonder(o, &m));
    gd_ret_bekle(gd_basit(o, (uint8_t)IDA_CMD_MISSION_END, 0u), (uint16_t)IDA_REASON_OUT_OF_RANGE);
    depo_esit(&once, &o->g);
    /* Yeniden BEGIN önceki açık yüklemeyi bırakır; yürürlükteki görev END'e kadar değişmez. */
    gd_kabul_bekle(gd_basit(o, (uint8_t)IDA_CMD_MISSION_BEGIN, 2u));
    m.index = 0u;
    gd_kabul_bekle(gd_oge_gonder(o, &m));
    gd_kabul_bekle(gd_basit(o, (uint8_t)IDA_CMD_MISSION_BEGIN, 1u));
    depo_esit(&once, &o->g);
    m.index = 0u;
    m.north_m = 7.0f;
    gd_kabul_bekle(gd_oge_gonder(o, &m));
    gd_kabul_bekle(gd_basit(o, (uint8_t)IDA_CMD_MISSION_END, 0u));
    TEST_ASSERT_EQUAL_UINT16(1u, o->g.gv.n);
    TEST_ASSERT_EQUAL_FLOAT(7.0f, o->g.gv.oge[0].konum.ne[0]);
    TEST_ASSERT_TRUE(ida_gor_tutarli(&o->g));
}

/* Yürüyen görev sırasında yükleme (A4-K51): RUNNING'de BEGIN BAD_MODE; açık yükleme RUNNING'e
   geçişte kapanır (END OUT_OF_RANGE, görev değişmez); PAUSED'da ve HOLD sürerken değiştirilir. */
static void test_yuruyen_gorevde_yukleme(void)
{
    gd_t *o = &O;
    gd_kur(o);
    ida_mission_item_t g[1]; /* MSVC C4204: sabit olmayan başlatıcı yok */
    g[0] = gd_oge((uint8_t)IDA_MI_HOLD, (uint8_t)IDA_MI_FRAME_NONE, 600.0f, 5.0f, 0.0f, 0.0f, 0.0f);
    gd_yukle(o, g, 1u);
    /* LOADED'da açılan yükleme, AUTO'ya girişte (RUNNING) kapanır. */
    gd_kabul_bekle(gd_basit(o, (uint8_t)IDA_CMD_MISSION_BEGIN, 1u));
    ida_mission_item_t m = wp();
    m.index = 0u;
    gd_kabul_bekle(gd_oge_gonder(o, &m));
    gd_auto(o);
    gd_ret_bekle(gd_basit(o, (uint8_t)IDA_CMD_MISSION_END, 0u), (uint16_t)IDA_REASON_OUT_OF_RANGE);
    TEST_ASSERT_EQUAL_UINT8(IDA_MI_HOLD, o->g.gv.oge[0].tur);
    gd_ilerle(o, 300);
    TEST_ASSERT_EQUAL_UINT8(IDA_MS_RUNNING, o->g.mission_state);
    /* RUNNING: BEGIN BAD_MODE */
    gd_ret_bekle(gd_basit(o, (uint8_t)IDA_CMD_MISSION_BEGIN, 1u), (uint16_t)IDA_REASON_BAD_MODE);
    /* PAUSED: değiştirilir; yeni görev LOADED olur, AUTO DEPTH_HOLD'a iner. */
    gd_kabul_bekle(gd_basit(o, (uint8_t)IDA_CMD_MISSION_CTL, (uint16_t)IDA_MCTL_PAUSE));
    TEST_ASSERT_EQUAL_UINT8(IDA_MS_PAUSED, o->g.mission_state);
    gd_yukle(o, &m, 1u);
    gd_ilerle(o, 200); /* kip düşmesinin çağrısında genişletici NARROWED alır (W3-K2) */
    TEST_ASSERT_EQUAL_UINT8(IDA_MODE_DEPTH_HOLD, o->e.mode);
    TEST_ASSERT_EQUAL_UINT8(IDA_MI_WAYPOINT, o->g.gv.oge[0].tur);
    /* HOLD arıza eylemi sürerken (yürüyen görev duraklar ve değiştirilebilir). */
    gd_kabul_bekle(gd_basit(o, (uint8_t)IDA_CMD_SET_MODE, (uint16_t)IDA_MODE_AUTO));
    gd_ilerle(o, 100);
    TEST_ASSERT_EQUAL_UINT8(IDA_MS_RUNNING, o->g.mission_state);
    o->gi.gecerlilik = (uint8_t)(gd_gecerlilik_tam() & (uint8_t)(0xFFu ^ IDA_KES_G_KONUM));
    gd_ilerle(o, 100);
    TEST_ASSERT_EQUAL_UINT8(IDA_FS_HOLD, o->g.fs_level);
    TEST_ASSERT_TRUE(ida_gor_gorev_degisebilir(&o->g));
    gd_yukle(o, g, 1u);
}

static void test_scan_ogeleri_yuklenir(void)
{
    gd_t *o = &O;
    gd_kur(o);
    ida_mission_item_t s[2];
    s[0] = gd_oge((uint8_t)IDA_MI_SCAN_LINES, (uint8_t)IDA_MI_FRAME_LOCAL, 20.0f, 2.0f, 1.0f, 1.0f,
                  2.0f);
    s[0].p[2] = 3.0f;
    s[0].p[3] = 0.5f;
    s[1] = gd_oge((uint8_t)IDA_MI_SCAN_POINT, (uint8_t)IDA_MI_FRAME_LOCAL, 30.0f, 1.0f, 2.0f, 2.0f,
                  2.0f);
    gd_yukle(o, s, 2u); /* sözleşme reddi tanımlamaz: yüklenir (yürütmede iptal, yurut testi) */
}

/* ---- Kabul 2: durum makinesi --------------------------------------------------------------- */

static void ctl_bekle(gd_t *o, uint8_t eylem, uint16_t neden)
{
    const ida_cmd_result_t r = gd_basit(o, (uint8_t)IDA_CMD_MISSION_CTL, eylem);
    if (neden == (uint16_t)IDA_REASON_NONE) {
        gd_kabul_bekle(r);
    } else {
        gd_ret_bekle(r, neden);
    }
}

static void uzun_gorev(gd_t *o)
{
    ida_mission_item_t g[2]; /* MSVC C4204: sabit olmayan başlatıcı yok */
    g[0] = gd_oge((uint8_t)IDA_MI_HOLD, (uint8_t)IDA_MI_FRAME_NONE, 600.0f, 5.0f, 0.0f, 0.0f, 0.0f);
    g[1] = gd_oge((uint8_t)IDA_MI_HOLD, (uint8_t)IDA_MI_FRAME_NONE, 600.0f, 5.0f, 0.0f, 0.0f, 0.0f);
    gd_yukle(o, g, 2u);
}

static void test_idle_ve_loaded_gecisleri(void)
{
    gd_t *o = &O;
    gd_kur(o);
    /* IDLE: her eylem BAD_MODE; action 0 ve 5 OUT_OF_RANGE; AUTO BAD_MODE. */
    ctl_bekle(o, (uint8_t)IDA_MCTL_START, (uint16_t)IDA_REASON_BAD_MODE);
    ctl_bekle(o, (uint8_t)IDA_MCTL_PAUSE, (uint16_t)IDA_REASON_BAD_MODE);
    ctl_bekle(o, (uint8_t)IDA_MCTL_RESUME, (uint16_t)IDA_REASON_BAD_MODE);
    ctl_bekle(o, (uint8_t)IDA_MCTL_ABORT, (uint16_t)IDA_REASON_BAD_MODE);
    ctl_bekle(o, (uint8_t)0u, (uint16_t)IDA_REASON_OUT_OF_RANGE);
    ctl_bekle(o, (uint8_t)5u, (uint16_t)IDA_REASON_OUT_OF_RANGE);
    gd_kabul_bekle(gd_basit(o, (uint8_t)IDA_CMD_ARM, 0u));
    gd_ret_bekle(gd_basit(o, (uint8_t)IDA_CMD_SET_MODE, (uint16_t)IDA_MODE_AUTO),
                 (uint16_t)IDA_REASON_BAD_MODE);
    TEST_ASSERT_EQUAL_UINT8(IDA_MS_IDLE, o->g.mission_state);
    /* LOADED: START (AUTO değil), PAUSE, RESUME BAD_MODE; ABORT → ABORTED. */
    uzun_gorev(o);
    ctl_bekle(o, (uint8_t)IDA_MCTL_START, (uint16_t)IDA_REASON_BAD_MODE);
    ctl_bekle(o, (uint8_t)IDA_MCTL_PAUSE, (uint16_t)IDA_REASON_BAD_MODE);
    ctl_bekle(o, (uint8_t)IDA_MCTL_RESUME, (uint16_t)IDA_REASON_BAD_MODE);
    ctl_bekle(o, (uint8_t)IDA_MCTL_ABORT, (uint16_t)IDA_REASON_NONE);
    TEST_ASSERT_EQUAL_UINT8(IDA_MS_ABORTED, o->g.mission_state);
    /* ABORTED: AUTO BAD_MODE; eylemler BAD_MODE; yeniden yükleme LOADED. */
    gd_ret_bekle(gd_basit(o, (uint8_t)IDA_CMD_SET_MODE, (uint16_t)IDA_MODE_AUTO),
                 (uint16_t)IDA_REASON_BAD_MODE);
    ctl_bekle(o, (uint8_t)IDA_MCTL_ABORT, (uint16_t)IDA_REASON_BAD_MODE);
    ctl_bekle(o, (uint8_t)IDA_MCTL_RESUME, (uint16_t)IDA_REASON_BAD_MODE);
    /* Operatör iptali yüzeye çıkış istemez. */
    gd_ilerle(o, 200);
    TEST_ASSERT_EQUAL_UINT8(IDA_FS_NONE, o->g.fs_level);
    uzun_gorev(o);
    TEST_ASSERT_EQUAL_UINT8(IDA_MS_LOADED, o->g.mission_state);
    /* LOADED → RUNNING: SET_MODE(AUTO); konum geçersizse BAD_MODE. */
    o->gi.gecerlilik = (uint8_t)(gd_gecerlilik_tam() & (uint8_t)(0xFFu ^ IDA_KES_G_KONUM));
    gd_ilerle(o, 100);
    gd_ret_bekle(gd_basit(o, (uint8_t)IDA_CMD_SET_MODE, (uint16_t)IDA_MODE_AUTO),
                 (uint16_t)IDA_REASON_BAD_MODE);
    o->gi.gecerlilik = gd_gecerlilik_tam();
    gd_ilerle(o, 100);
    gd_kabul_bekle(gd_basit(o, (uint8_t)IDA_CMD_SET_MODE, (uint16_t)IDA_MODE_AUTO));
    TEST_ASSERT_EQUAL_UINT8(IDA_MS_RUNNING, o->g.mission_state);
    gd_ilerle(o, 100);
    const ida_status_t st = gd_durum(o);
    TEST_ASSERT_EQUAL_UINT8(IDA_MS_RUNNING, st.mission_state);
    TEST_ASSERT_EQUAL_UINT16(0u, st.mission_item);
}

static void test_running_paused_gecisleri(void)
{
    gd_t *o = &O;
    gd_kur(o);
    uzun_gorev(o);
    gd_auto(o);
    gd_ilerle(o, 300);
    /* RUNNING: RESUME BAD_MODE; PAUSE → PAUSED (AUTO sürer, araç durak noktasında tutar). */
    ctl_bekle(o, (uint8_t)IDA_MCTL_RESUME, (uint16_t)IDA_REASON_BAD_MODE);
    ctl_bekle(o, (uint8_t)IDA_MCTL_PAUSE, (uint16_t)IDA_REASON_NONE);
    gd_ilerle(o, 1000);
    TEST_ASSERT_EQUAL_UINT8(IDA_MS_PAUSED, o->g.mission_state);
    TEST_ASSERT_EQUAL_UINT8(IDA_MODE_AUTO, o->e.mode);
    TEST_ASSERT_EQUAL_UINT8(IDA_GOR_HY_TUT, o->g.gv.hedef.yatay);
    ctl_bekle(o, (uint8_t)IDA_MCTL_PAUSE, (uint16_t)IDA_REASON_BAD_MODE);
    /* PAUSED → RUNNING: RESUME */
    ctl_bekle(o, (uint8_t)IDA_MCTL_RESUME, (uint16_t)IDA_REASON_NONE);
    gd_ilerle(o, 100);
    TEST_ASSERT_EQUAL_UINT8(IDA_MS_RUNNING, o->g.mission_state);
    /* AUTO'dan çıkış (SET_MODE GUIDED): PAUSED; RESUME AUTO dışında BAD_MODE; AUTO'ya dönüş sürer. */
    gd_kabul_bekle(gd_basit(o, (uint8_t)IDA_CMD_SET_MODE, (uint16_t)IDA_MODE_GUIDED));
    TEST_ASSERT_EQUAL_UINT8(IDA_MS_PAUSED, o->g.mission_state);
    gd_ilerle(o, 200);
    TEST_ASSERT_EQUAL_UINT8(IDA_MS_PAUSED, o->g.mission_state);
    ctl_bekle(o, (uint8_t)IDA_MCTL_RESUME, (uint16_t)IDA_REASON_BAD_MODE);
    ctl_bekle(o, (uint8_t)IDA_MCTL_START, (uint16_t)IDA_REASON_BAD_MODE);
    gd_kabul_bekle(gd_basit(o, (uint8_t)IDA_CMD_SET_MODE, (uint16_t)IDA_MODE_AUTO));
    gd_ilerle(o, 100);
    TEST_ASSERT_EQUAL_UINT8(IDA_MS_RUNNING, o->g.mission_state);
    /* Daraltıcı kip (emniyet modülünün (b) adımı): PAUSED; kip DEPTH_HOLD. */
    o->e.mode = (uint8_t)IDA_MODE_DEPTH_HOLD;
    gd_ilerle(o, 100);
    TEST_ASSERT_EQUAL_UINT8(IDA_MS_PAUSED, o->g.mission_state);
    gd_kabul_bekle(gd_basit(o, (uint8_t)IDA_CMD_SET_MODE, (uint16_t)IDA_MODE_AUTO));
    gd_ilerle(o, 100);
    TEST_ASSERT_EQUAL_UINT8(IDA_MS_RUNNING, o->g.mission_state);
    /* Silahsızlanma (AUTO kipi sürer): PAUSED, operatör duraklaması gibi (düzeltme turu bulgu 2):
       yeniden silahlanma tek başına görevi sürdürmez; RESUME ya da SET_MODE(AUTO) gerekir. */
    o->e.armed = false;
    gd_ilerle(o, 100);
    TEST_ASSERT_EQUAL_UINT8(IDA_MS_PAUSED, o->g.mission_state);
    TEST_ASSERT_EQUAL_UINT8(IDA_GOR_HY_YOK, o->g.gv.hedef.yatay);
    gd_kabul_bekle(gd_basit(o, (uint8_t)IDA_CMD_ARM, 0u));
    gd_ilerle(o, 1000);
    TEST_ASSERT_EQUAL_UINT8(IDA_MS_PAUSED, o->g.mission_state);
    TEST_ASSERT_EQUAL_UINT8(IDA_GOR_HY_TUT, o->g.gv.hedef.yatay); /* duraklama noktasında tutar */
    ctl_bekle(o, (uint8_t)IDA_MCTL_RESUME, (uint16_t)IDA_REASON_NONE);
    gd_ilerle(o, 100);
    TEST_ASSERT_EQUAL_UINT8(IDA_MS_RUNNING, o->g.mission_state);
    /* SET_MODE(AUTO) da sürdürür. */
    o->e.armed = false;
    gd_ilerle(o, 100);
    gd_kabul_bekle(gd_basit(o, (uint8_t)IDA_CMD_ARM, 0u));
    gd_ilerle(o, 100);
    TEST_ASSERT_EQUAL_UINT8(IDA_MS_PAUSED, o->g.mission_state);
    gd_kabul_bekle(gd_basit(o, (uint8_t)IDA_CMD_SET_MODE, (uint16_t)IDA_MODE_AUTO));
    gd_ilerle(o, 100);
    TEST_ASSERT_EQUAL_UINT8(IDA_MS_RUNNING, o->g.mission_state);
    /* START (AUTO): öğe 0'dan yeniden. */
    o->g.mission_item = 1u;
    o->g.gv.oge_basladi = false;
    gd_ilerle(o, 100);
    TEST_ASSERT_EQUAL_UINT16(1u, gd_durum(o).mission_item);
    ctl_bekle(o, (uint8_t)IDA_MCTL_START, (uint16_t)IDA_REASON_NONE);
    gd_ilerle(o, 100);
    TEST_ASSERT_EQUAL_UINT16(0u, gd_durum(o).mission_item);
    TEST_ASSERT_EQUAL_UINT8(IDA_MS_RUNNING, o->g.mission_state);
    /* RUNNING → ABORTED (operatör): AUTO DEPTH_HOLD'a iner, yüzeye çıkış istenmez. */
    ctl_bekle(o, (uint8_t)IDA_MCTL_ABORT, (uint16_t)IDA_REASON_NONE);
    gd_ilerle(o, 100);
    TEST_ASSERT_EQUAL_UINT8(IDA_MS_ABORTED, o->g.mission_state);
    TEST_ASSERT_EQUAL_UINT8(IDA_MODE_DEPTH_HOLD, o->e.mode);
    TEST_ASSERT_EQUAL_UINT8(IDA_FS_NONE, o->g.fs_level);
}

/* Düzeltme turu bulgu 2, genel ilke: duraklamadan kendiliğinden çıkış yalnız duraklamanın nedeni
   çekirdeğin kendi koşuluysa ve o koşul kalktıysa olur. Konum kaybıyla (HOLD) duraklamış görev
   bu sırada silahsızlanıp yeniden silahlanırsa konum dönünce de sürmez. Silah, çekirdek
   koşulundan kaynaklanan (HOLD) duraklamada inmedikçe görev kendiliğinden sürer (kontrol). */
static void test_silahsizlanma_kendiliginden_surmez(void)
{
    gd_t *o = &O;
    gd_kur(o);
    uzun_gorev(o);
    gd_auto(o);
    gd_ilerle(o, 300);
    const uint8_t konumsuz = (uint8_t)(gd_gecerlilik_tam() & (uint8_t)(0xFFu ^ IDA_KES_G_KONUM));
    /* Kontrol: konum kaybı, HOLD, konum döner: kendiliğinden sürer. */
    o->gi.gecerlilik = konumsuz;
    gd_ilerle(o, 300);
    TEST_ASSERT_EQUAL_UINT8(IDA_MS_PAUSED, o->g.mission_state);
    o->gi.gecerlilik = gd_gecerlilik_tam();
    gd_ilerle(o, 3000);
    TEST_ASSERT_EQUAL_UINT8(IDA_MS_RUNNING, o->g.mission_state);
    /* Konum kaybı sırasında silahsızlanma ve yeniden silahlanma. */
    o->gi.gecerlilik = konumsuz;
    gd_ilerle(o, 300);
    TEST_ASSERT_EQUAL_UINT8(IDA_MS_PAUSED, o->g.mission_state);
    o->e.armed = false;
    gd_ilerle(o, 300);
    o->gi.gecerlilik = gd_gecerlilik_tam();
    gd_ilerle(o, 3000); /* HOLD kalktı, ARM reddi yok */
    TEST_ASSERT_EQUAL_UINT8(IDA_FS_NONE, o->g.fs_level);
    gd_kabul_bekle(gd_basit(o, (uint8_t)IDA_CMD_ARM, 0u));
    gd_ilerle(o, 3000);
    TEST_ASSERT_EQUAL_UINT8(IDA_MS_PAUSED, o->g.mission_state);
    TEST_ASSERT_EQUAL_UINT8(IDA_MODE_AUTO, o->e.mode);
    ctl_bekle(o, (uint8_t)IDA_MCTL_RESUME, (uint16_t)IDA_REASON_NONE);
    gd_ilerle(o, 100);
    TEST_ASSERT_EQUAL_UINT8(IDA_MS_RUNNING, o->g.mission_state);
}

/* Düzeltme turu bulgu 2: SESSION_START'ta görevin yakaladığı konumlar ve öğenin başlangıç durumu
   sıfırlanır (kestirim yeniden başlar; eski yerel düzlem noktası kullanılmaz): öğe yeniden
   başlar, bacak yeni konumdan kurulur, duraklama noktası yeniden yakalanır. */
static void test_oturum_basi_yakalanan_konumlar(void)
{
    gd_t *o = &O;
    gd_kur(o);
    ida_mission_item_t g[1]; /* MSVC C4204: sabit olmayan başlatıcı yok */
    g[0] = gd_oge((uint8_t)IDA_MI_WAYPOINT, (uint8_t)IDA_MI_FRAME_LOCAL, 0.5f, 0.5f, 100.0f, 0.0f,
                  0.0f);
    gd_yukle(o, g, 1u);
    gd_auto(o);
    gd_ilerle(o, 100);
    TEST_ASSERT_EQUAL_FLOAT(0.0f, o->g.gv.hedef.bas_ne[0]);
    o->gi.nav_ne[0] = 2.0f;
    o->gi.nav_ne[1] = 1.0f;
    gd_ilerle(o, 100);
    /* Operatör duraklaması: duraklama noktası (2, 1). */
    ctl_bekle(o, (uint8_t)IDA_MCTL_PAUSE, (uint16_t)IDA_REASON_NONE);
    gd_ilerle(o, 100);
    TEST_ASSERT_EQUAL_UINT8(IDA_GOR_HY_TUT, o->g.gv.hedef.yatay);
    TEST_ASSERT_EQUAL_FLOAT(2.0f, o->g.gv.hedef.hedef_ne[0]);
    /* Oturum yeniden başlar; kestirimin yeni yerel düzleminde araç (50, 50). */
    ida_gor_oturum_basi(&o->g);
    o->gi.nav_ne[0] = 50.0f;
    o->gi.nav_ne[1] = 50.0f;
    gd_ilerle(o, 100);
    TEST_ASSERT_EQUAL_UINT8(IDA_MS_PAUSED, o->g.mission_state);
    TEST_ASSERT_EQUAL_UINT8(IDA_GOR_HY_TUT, o->g.gv.hedef.yatay);
    TEST_ASSERT_EQUAL_FLOAT(50.0f, o->g.gv.hedef.hedef_ne[0]);
    TEST_ASSERT_EQUAL_FLOAT(50.0f, o->g.gv.hedef.hedef_ne[1]);
    /* Sürdürülünce öğe yeniden başlar: bacak bulunulan konumdan. */
    ctl_bekle(o, (uint8_t)IDA_MCTL_RESUME, (uint16_t)IDA_REASON_NONE);
    gd_ilerle(o, 100);
    TEST_ASSERT_EQUAL_UINT8(IDA_MS_RUNNING, o->g.mission_state);
    TEST_ASSERT_EQUAL_UINT8(IDA_GOR_HY_YOL, o->g.gv.hedef.yatay);
    TEST_ASSERT_EQUAL_FLOAT(50.0f, o->g.gv.hedef.bas_ne[0]);
    TEST_ASSERT_EQUAL_FLOAT(50.0f, o->g.gv.hedef.bas_ne[1]);
    TEST_ASSERT_EQUAL_FLOAT(100.0f, o->g.gv.hedef.hedef_ne[0]);
}

/* Düzeltme turu bulgu 7: engel kalkınca yapılan kendiliğinden PAUSED → RUNNING geçişi de açık
   yüklemeyi kapatır (yürüyen görev değiştirilemez, A4-K51). */
static void test_kendiliginden_surme_yuklemeyi_kapatir(void)
{
    gd_t *o = &O;
    gd_kur(o);
    uzun_gorev(o);
    gd_auto(o);
    gd_ilerle(o, 300);
    o->gi.gecerlilik = (uint8_t)(gd_gecerlilik_tam() & (uint8_t)(0xFFu ^ IDA_KES_G_KONUM));
    gd_ilerle(o, 300);
    TEST_ASSERT_EQUAL_UINT8(IDA_MS_PAUSED, o->g.mission_state);
    gd_kabul_bekle(gd_basit(o, (uint8_t)IDA_CMD_MISSION_BEGIN, 2u));
    ida_mission_item_t m = wp();
    m.index = 0u;
    gd_kabul_bekle(gd_oge_gonder(o, &m));
    o->gi.gecerlilik = gd_gecerlilik_tam();
    gd_ilerle(o, 3000);
    TEST_ASSERT_EQUAL_UINT8(IDA_MS_RUNNING, o->g.mission_state);
    TEST_ASSERT_FALSE(o->g.gv.yuk_acik);
    m.index = 1u;
    gd_ret_bekle(gd_oge_gonder(o, &m), (uint16_t)IDA_REASON_OUT_OF_RANGE);
    gd_ret_bekle(gd_basit(o, (uint8_t)IDA_CMD_MISSION_END, 0u), (uint16_t)IDA_REASON_OUT_OF_RANGE);
    TEST_ASSERT_EQUAL_UINT8(IDA_MI_HOLD, o->g.gv.oge[0].tur);
}

static void test_complete_gecisi(void)
{
    gd_t *o = &O;
    gd_kur(o);
    ida_mission_item_t g[1]; /* MSVC C4204: sabit olmayan başlatıcı yok */
    g[0] = gd_oge((uint8_t)IDA_MI_HOLD, (uint8_t)IDA_MI_FRAME_NONE, 1.0f, 5.0f, 0.0f, 0.0f, 0.0f);
    gd_yukle(o, g, 1u);
    gd_auto(o);
    gd_ilerle(o, 1500);
    TEST_ASSERT_EQUAL_UINT8(IDA_MS_COMPLETE, o->g.mission_state);
    TEST_ASSERT_EQUAL_UINT8(IDA_MODE_DEPTH_HOLD, o->e.mode); /* yürüyen görev yok (02 §2.6) */
    TEST_ASSERT_EQUAL_UINT8(IDA_FS_NONE, o->g.fs_level);
    TEST_ASSERT_EQUAL_UINT8(IDA_MS_COMPLETE, gd_durum(o).mission_state);
    /* COMPLETE: yasak geçişler */
    ctl_bekle(o, (uint8_t)IDA_MCTL_PAUSE, (uint16_t)IDA_REASON_BAD_MODE);
    ctl_bekle(o, (uint8_t)IDA_MCTL_RESUME, (uint16_t)IDA_REASON_BAD_MODE);
    ctl_bekle(o, (uint8_t)IDA_MCTL_ABORT, (uint16_t)IDA_REASON_BAD_MODE);
    ctl_bekle(o, (uint8_t)IDA_MCTL_START, (uint16_t)IDA_REASON_BAD_MODE);
    gd_ret_bekle(gd_basit(o, (uint8_t)IDA_CMD_SET_MODE, (uint16_t)IDA_MODE_AUTO),
                 (uint16_t)IDA_REASON_BAD_MODE);
    /* COMPLETE → LOADED (yeniden yükleme) → RUNNING */
    gd_yukle(o, g, 1u);
    gd_kabul_bekle(gd_basit(o, (uint8_t)IDA_CMD_SET_MODE, (uint16_t)IDA_MODE_AUTO));
    TEST_ASSERT_EQUAL_UINT8(IDA_MS_RUNNING, o->g.mission_state);
}

/* Görev komutları genişletici değildir; daraltmalı çağrıda da kendi satırlarına göre işlenir,
   ama SET_MODE(AUTO) (genişletici) NARROWED alır ve görev başlamaz. */
static void test_daraltmali_cagri(void)
{
    gd_t *o = &O;
    gd_kur(o);
    uzun_gorev(o);
    gd_kabul_bekle(gd_basit(o, (uint8_t)IDA_CMD_ARM, 0u));
    o->e.d.herhangi = true;
    gd_ret_bekle(gd_basit(o, (uint8_t)IDA_CMD_SET_MODE, (uint16_t)IDA_MODE_AUTO),
                 (uint16_t)IDA_REASON_NARROWED);
    TEST_ASSERT_EQUAL_UINT8(IDA_MS_LOADED, o->g.mission_state);
    ctl_bekle(o, (uint8_t)IDA_MCTL_ABORT, (uint16_t)IDA_REASON_NONE);
    o->e.d.herhangi = false;
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_tablo36a_waypoint);
    RUN_TEST(test_tablo36a_diger_turler);
    RUN_TEST(test_gecerli_yukleme);
    RUN_TEST(test_gecersiz_yuklemeler_depoyu_degistirmez);
    RUN_TEST(test_yuruyen_gorevde_yukleme);
    RUN_TEST(test_scan_ogeleri_yuklenir);
    RUN_TEST(test_idle_ve_loaded_gecisleri);
    RUN_TEST(test_running_paused_gecisleri);
    RUN_TEST(test_silahsizlanma_kendiliginden_surmez);
    RUN_TEST(test_oturum_basi_yakalanan_konumlar);
    RUN_TEST(test_kendiliginden_surme_yuklemeyi_kapatir);
    RUN_TEST(test_complete_gecisi);
    RUN_TEST(test_daraltmali_cagri);
    return UNITY_END();
}
