/*
 * test_zaman_mat.c — zaman ve seri aritmetik yardımcıları (iskelet §3.1; A4-K07, K08) ve
 * matematik yardımcıları (mat).
 */
#include "mat/ida_math.h"
#include "mat/mat.h"
#include "unity.h"
#include "zaman/zaman.h"

void setUp(void);
void tearDown(void);
void setUp(void) {}
void tearDown(void) {}

#define TOL 1e-5f

static void test_seri_aritmetik(void)
{
    TEST_ASSERT_EQUAL(IDA_SERI_AYNI, ida_seri_karsilastir(5u, 5u));
    TEST_ASSERT_EQUAL(IDA_SERI_YENI, ida_seri_karsilastir(6u, 5u));
    TEST_ASSERT_EQUAL(IDA_SERI_ESKI, ida_seri_karsilastir(4u, 5u));
    TEST_ASSERT_EQUAL(IDA_SERI_YENI, ida_seri_karsilastir(0u, 0xFFFFFFFFu));  /* sarma */
    TEST_ASSERT_EQUAL(IDA_SERI_YENI, ida_seri_karsilastir(0x7FFFFFFFu, 0u)); /* 2^31 − 1 */
    TEST_ASSERT_EQUAL(IDA_SERI_ESKI, ida_seri_karsilastir(0x80000000u, 0u)); /* 2^31 */
    TEST_ASSERT_EQUAL(IDA_SERI_ESKI, ida_seri_karsilastir(0xFFFFFFFFu, 0u));
}

static void test_zaman_araligi(void)
{
    ida_stime_t s = {0};
    TEST_ASSERT_TRUE(ida_st_gecerli(s));
    s.ns = IDA_TIME_MAX_NS;
    TEST_ASSERT_TRUE(ida_st_gecerli(s));
    s.ns = IDA_TIME_MAX_NS + 1;
    TEST_ASSERT_FALSE(ida_st_gecerli(s));
    s.ns = -1;
    TEST_ASSERT_FALSE(ida_st_gecerli(s));
    ida_wtime_t w = {IDA_TIME_MAX_NS};
    TEST_ASSERT_TRUE(ida_wt_gecerli(w));
    w.ns = -1;
    TEST_ASSERT_FALSE(ida_wt_gecerli(w));
    const ida_wtime_t a = {IDA_TIME_MAX_NS};
    const ida_wtime_t b = {0};
    TEST_ASSERT_TRUE((IDA_TIME_MAX_NS) == (ida_wt_fark_ns(a, b)));
    TEST_ASSERT_EQUAL_INT32(INT32_MAX, ida_ns_us_doyur(IDA_TIME_MAX_NS));
    TEST_ASSERT_EQUAL_INT32(INT32_MIN, ida_ns_us_doyur(-IDA_TIME_MAX_NS));
    TEST_ASSERT_EQUAL_INT32(1500, ida_ns_us_doyur(1500999));
}

static void test_vektor(void)
{
    const ida_v3_t x = {1.0f, 0.0f, 0.0f};
    const ida_v3_t y = {0.0f, 1.0f, 0.0f};
    const ida_v3_t z = ida_v3_dis(x, y);
    TEST_ASSERT_FLOAT_WITHIN(TOL, 1.0f, z.z);
    TEST_ASSERT_FLOAT_WITHIN(TOL, 0.0f, ida_v3_ic(x, y));
    const ida_v3_t v = {3.0f, 4.0f, 0.0f};
    TEST_ASSERT_FLOAT_WITHIN(TOL, 5.0f, ida_v3_norm(v));
    ida_v3_t b;
    TEST_ASSERT_TRUE(ida_v3_birim(v, &b));
    TEST_ASSERT_FLOAT_WITHIN(TOL, 0.6f, b.x);
    const ida_v3_t sifir = {0.0f, 0.0f, 0.0f};
    TEST_ASSERT_FALSE(ida_v3_birim(sifir, &b));
    const ida_v3_t t = ida_v3_cikar(ida_v3_topla(v, x), ida_v3_olcekle(x, 2.0f));
    TEST_ASSERT_FLOAT_WITHIN(TOL, 2.0f, t.x);
}

static void test_kuaterniyon(void)
{
    /* z ekseni etrafında +90°: gövde x ekseni NED'de doğuyu (y) gösterir. */
    const ida_v3_t rv = {0.0f, 0.0f, 0.5f * IDA_PI_F};
    const ida_q_t q = ida_q_donme_vektorunden(rv);
    const ida_v3_t x = {1.0f, 0.0f, 0.0f};
    const ida_v3_t d = ida_q_dondur(q, x);
    TEST_ASSERT_FLOAT_WITHIN(TOL, 0.0f, d.x);
    TEST_ASSERT_FLOAT_WITHIN(TOL, 1.0f, d.y);
    const ida_euler_t e = ida_q_euler(q);
    TEST_ASSERT_FLOAT_WITHIN(TOL, 0.5f * IDA_PI_F, e.yaw);
    TEST_ASSERT_FLOAT_WITHIN(TOL, 0.0f, e.roll);
    /* DCM ile döndürme aynı sonucu verir. */
    const ida_v3_t d2 = ida_m3_v3(ida_q_dcm(q), x);
    TEST_ASSERT_FLOAT_WITHIN(TOL, d.y, d2.y);
    /* q ⊗ q* = birim. */
    const ida_q_t i = ida_q_carp(q, ida_q_eslenik(q));
    TEST_ASSERT_FLOAT_WITHIN(TOL, 1.0f, i.w);
    TEST_ASSERT_FLOAT_WITHIN(TOL, 0.0f, i.z);
    ida_q_t b;
    const ida_q_t sifir = {0.0f, 0.0f, 0.0f, 0.0f};
    TEST_ASSERT_FALSE(ida_q_birimle(sifir, &b));
    TEST_ASSERT_FLOAT_WITHIN(TOL, 1.0f, b.w);
    const ida_q_t iki = {2.0f, 0.0f, 0.0f, 0.0f};
    TEST_ASSERT_TRUE(ida_q_birimle(iki, &b));
    TEST_ASSERT_FLOAT_WITHIN(TOL, 1.0f, b.w);
}

static void test_matris(void)
{
    const ida_m3_t r = ida_q_dcm(ida_q_donme_vektorunden((ida_v3_t){0.1f, -0.2f, 0.3f}));
    const ida_m3_t rrt = ida_m3_carp(r, ida_m3_devrik(r)); /* dikgen: R Rᵀ = I */
    const ida_m3_t i = ida_m3_birim_deger();
    for (uint32_t a = 0u; a < 3u; a++) {
        for (uint32_t b = 0u; b < 3u; b++) {
            TEST_ASSERT_FLOAT_WITHIN(TOL, i.m[a][b], rrt.m[a][b]);
        }
    }
    const ida_v3_t v = {1.0f, 2.0f, 3.0f};
    const ida_v3_t u = {-1.0f, 0.5f, 2.0f};
    const ida_v3_t c1 = ida_m3_v3(ida_m3_capraz(v), u);
    const ida_v3_t c2 = ida_v3_dis(v, u);
    TEST_ASSERT_FLOAT_WITHIN(TOL, c2.x, c1.x);
    TEST_ASSERT_FLOAT_WITHIN(TOL, c2.z, c1.z);

    /* Küçük matris: A (2×3) · P (3×3) · Aᵀ */
    const float a[6] = {1, 2, 0, 0, 1, 1};
    const float p[9] = {2, 0, 0, 0, 1, 0, 0, 0, 3};
    float ara[6] = {0.0f};
    float s[4] = {0.0f};
    float t[6] = {0.0f};
    float top[6] = {0.0f};
    /* Görünümler bildirimden sonra alan alan kurulur: otomatik dizinin adresiyle küme
       ilklendirmesi MSVC'de C4221 uyarısıdır. */
    ida_mat_oku_t ao;
    ao.veri = a;
    ao.boyut.satir = 2u;
    ao.boyut.sutun = 3u;
    ida_mat_oku_t po;
    po.veri = p;
    po.boyut.satir = 3u;
    po.boyut.sutun = 3u;
    ida_mat_yaz_t araw;
    araw.veri = ara;
    araw.boyut.satir = 2u;
    araw.boyut.sutun = 3u;
    ida_mat_yaz_t sw;
    sw.veri = s;
    sw.boyut.satir = 2u;
    sw.boyut.sutun = 2u;
    TEST_ASSERT_TRUE(ida_mat_apat(ao, po, &araw, &sw));
    TEST_ASSERT_FLOAT_WITHIN(TOL, 6.0f, s[0]); /* 1·2·1 + 2·1·2 */
    TEST_ASSERT_FLOAT_WITHIN(TOL, 2.0f, s[1]);
    TEST_ASSERT_FLOAT_WITHIN(TOL, 2.0f, s[2]);
    TEST_ASSERT_FLOAT_WITHIN(TOL, 4.0f, s[3]); /* 1 + 3 */
    ida_mat_yaz_t tw;
    tw.veri = t;
    tw.boyut.satir = 3u;
    tw.boyut.sutun = 2u;
    TEST_ASSERT_TRUE(ida_mat_devrik(ao, &tw));
    TEST_ASSERT_FLOAT_WITHIN(TOL, 2.0f, t[2]);
    ida_mat_yaz_t yanlis;
    yanlis.veri = s;
    yanlis.boyut.satir = 3u;
    yanlis.boyut.sutun = 3u;
    TEST_ASSERT_FALSE(ida_mat_carp(ao, po, &yanlis)); /* boyut uyuşmazlığı: yazmaz */
    ida_mat_yaz_t topw;
    topw.veri = top;
    topw.boyut.satir = 2u;
    topw.boyut.sutun = 3u;
    TEST_ASSERT_TRUE(ida_mat_topla(ao, ao, &topw));
    TEST_ASSERT_FLOAT_WITHIN(TOL, 4.0f, top[1]);
}

static void test_aci_ve_kirpma(void)
{
    TEST_ASSERT_FLOAT_WITHIN(TOL, IDA_PI_F, ida_aci_sar(IDA_PI_F));
    TEST_ASSERT_FLOAT_WITHIN(TOL, IDA_PI_F, ida_aci_sar(-IDA_PI_F)); /* (−π, π] */
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, 0.5f, ida_aci_sar(0.5f + (4.0f * IDA_PI_F)));
    const ida_aralik_t r = {-1.0f, 1.0f};
    TEST_ASSERT_FLOAT_WITHIN(TOL, 1.0f, ida_kirp(3.0f, r));
    TEST_ASSERT_FLOAT_WITHIN(TOL, -1.0f, ida_kirp(-3.0f, r));
    TEST_ASSERT_TRUE(ida_sonlu(1.0f));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_seri_aritmetik);
    RUN_TEST(test_zaman_araligi);
    RUN_TEST(test_vektor);
    RUN_TEST(test_kuaterniyon);
    RUN_TEST(test_matris);
    RUN_TEST(test_aci_ve_kirpma);
    return UNITY_END();
}
