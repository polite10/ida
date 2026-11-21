/*
 * test_dalis_yasak.c — derinleşme yasağının kontrol çıkışına kadar uygulanması (sol W3-zincir
 * 2. tur bulgu 2; 02 §4.3; EMNİYET).
 *
 * sol'un karşı örneği: yasak sürerken araç 3 m'den 200 adımda (1 s) 2 m'ye yükselir ve durur.
 * Görev modülü derinlik hedefini her adımda bulunulan derinliğe kırpar; kontrolün yumuşatılmış
 * derinlik referansı ise 0,25 m/s ile izler ve 2,75 m'de kalır: eski kodda araç durunca
 * w_sp = 0,8·0,75 − 0,25 = +0,35 m/s ve ≈ 72 N aşağı kuvvet istenir. Ölçüt tahsisin elde ettiği
 * genelleştirilmiş kuvvettir (itki isteğinden): NED aşağı kuvvet hiçbir adımda yüzerlik
 * dengelemesini (BUOY_FF) aşmamalı.
 */
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "destek.h"
#include "kontrol/kontrol.h"
#include "kontrol/tahsis.h"
#include "mat/ida_math.h"
#include "mat/mat.h"
#include "param/param.h"
#include "unity.h"

void setUp(void);
void tearDown(void);

static ida_prm_kume_t P;
static ida_kon_t K;

void setUp(void)
{
    ida_prm_varsayilan(&P);
    ida_kon_kur(&K);
}
void tearDown(void) {}

static float prm(enum ida_prm_indis i)
{
    float f = 0.0f;
    (void)memcpy(&f, &P.ham[i], sizeof(f));
    return f;
}

/* Düz duruşlu dalış isteği; derinlik hedefi bulunulan derinliğe kırpılmış (görev modülünün
   yasak altındaki davranışı). */
static void girdi(ida_kon_girdi_t *g, int64_t t_ns, float derinlik, float w, uint8_t yasak)
{
    (void)memset(g, 0, sizeof(*g));
    g->nav.t.ns = t_ns;
    g->nav.q_nb[0] = 1.0f;
    g->nav.depth = derinlik;
    g->nav.v_ned[2] = w;
    g->nav.est_level = (uint8_t)IDA_EST_DEGRADED;
    g->t_ns = t_ns;
    g->armed = true;
    g->kisit = false;
    g->kestirim_yeterli = true;
    g->istek.kip = (uint8_t)IDA_KON_KIP_DALIS;
    g->istek.dikey = (uint8_t)IDA_KON_DIKEY_DERINLIK;
    g->istek.derinlik = (yasak != 0u) ? derinlik : 3.0f; /* yasak yoksa eski 3 m hedefi */
    g->istek.tutum = (uint8_t)IDA_KON_TUTUM_ACI;
    g->istek.yon_kanali = (uint8_t)IDA_KON_YON_ACI;
    g->istek.yatay = (uint8_t)IDA_KON_YATAY_KUVVET;
    g->istek.dalis_yasak = yasak;
}

/* Akış: 3 m'de 1 s tutma, 1 s'de 2 m'ye yükselme, 3 s durma. Her adımda elde edilen NED aşağı
   kuvvet (düz duruşta gövde Z) döner; en büyüğü *en_buyuk'a. */
static void akis(uint8_t yasak, float *en_buyuk)
{
    *en_buyuk = -1.0e9f;
    int64_t t = 1000 * DS_MS;
    for (uint32_t n = 0u; n < 1000u; n++) {
        float d = 3.0f;
        float w = 0.0f;
        if ((n >= 200u) && (n < 400u)) {
            d = 3.0f - ((float)(n - 200u) * 0.005f); /* 1 m/s yükselme */
            w = -1.0f;
        } else if (n >= 400u) {
            d = 2.0f;
        }
        ida_kon_girdi_t g;
        girdi(&g, t, d, w, yasak);
        ida_kon_hesapla(&K, &P, &g);
        TEST_ASSERT_EQUAL_UINT8(0u, K.cokme);
        float ta[6];
        ida_tah_uret(&K.tah, K.f, ta);
        if (ta[IDA_EKS_Z] > *en_buyuk) {
            *en_buyuk = ta[IDA_EKS_Z];
        }
        t += 5 * DS_MS;
    }
}

static void test_yasakta_asagi_kuvvet_yuzerlik_dengelemesini_asmaz(void)
{
    float en_buyuk = 0.0f;
    akis(1u, &en_buyuk);
    const float tutma = prm(IDA_PRM_kon_yuzerlik_ff);
    TEST_ASSERT_TRUE_MESSAGE(en_buyuk <= (tutma + 0.01f), "yasakta aşağı kuvvet tutma itkisini aştı");
    TEST_ASSERT_TRUE(K.i_w <= 0.0f); /* integral aşağı yönde birikmedi */
    TEST_ASSERT_TRUE(K.d_ref <= 2.0f);
}

/* Düzenek sınaması: yasak yokken aynı akış (hedef 3 m) belirgin aşağı kuvvet üretir; yani ölçüt
   tehlikeyi görür ve korumayı sağlayan yasak işaretidir. */
static void test_yasak_yokken_ayni_akis_asagi_kuvvet_uretir(void)
{
    float en_buyuk = 0.0f;
    akis(0u, &en_buyuk);
    TEST_ASSERT_TRUE(en_buyuk > 20.0f);
}

/* Doğrudan kuvvet kanalı (MANUAL, kestirim düşüşü): yasakta aşağı kuvvet de sınırlı. */
static void test_yasakta_dogrudan_kuvvet_sinirli(void)
{
    ida_kon_girdi_t g;
    int64_t t = 1000 * DS_MS;
    float ta[6];
    for (uint32_t n = 0u; n < 200u; n++) {
        girdi(&g, t, 2.0f, 0.0f, 1u);
        g.istek.dikey = (uint8_t)IDA_KON_DIKEY_KUVVET;
        g.istek.kuvvet[IDA_EKS_Z] = 50.0f;
        ida_kon_hesapla(&K, &P, &g);
        t += 5 * DS_MS;
    }
    ida_tah_uret(&K.tah, K.f, ta);
    TEST_ASSERT_TRUE(ta[IDA_EKS_Z] <= (prm(IDA_PRM_kon_yuzerlik_ff) + 0.01f));
}

/* ---- sol 3. tur bulgu 2: son NED kuvveti (eğik duruş, bütün kanallar) ------------------- */

#define DERECE (3.14159265f / 180.0f)

/* ZYX Euler açılarından kuaterniyon (gövde → NED). */
static void durus(ida_kon_girdi_t *g, float yalpa, float bas_kic, float yon)
{
    const float cr = ida_cos(0.5f * yalpa), sr = ida_sin(0.5f * yalpa);
    const float cp = ida_cos(0.5f * bas_kic), sp = ida_sin(0.5f * bas_kic);
    const float cy = ida_cos(0.5f * yon), sy = ida_sin(0.5f * yon);
    g->nav.q_nb[0] = (cr * cp * cy) + (sr * sp * sy);
    g->nav.q_nb[1] = (sr * cp * cy) - (cr * sp * sy);
    g->nav.q_nb[2] = (cr * sp * cy) + (sr * cp * sy);
    g->nav.q_nb[3] = (cr * cp * sy) - (sr * sp * cy);
}

/* Tahsisin gerçekleştirdiği kuvvetin (itki isteğinden) NED aşağı bileşeni. */
static float gerceklesen_asagi(const ida_kon_girdi_t *g)
{
    float ta[6];
    ida_tah_uret(&K.tah, K.f, ta);
    ida_q_t q;
    q.w = g->nav.q_nb[0];
    q.x = g->nav.q_nb[1];
    q.y = g->nav.q_nb[2];
    q.z = g->nav.q_nb[3];
    const ida_m3_t r = ida_q_dcm(q);
    return (r.m[2][0] * ta[IDA_EKS_X]) + (r.m[2][1] * ta[IDA_EKS_Y]) + (r.m[2][2] * ta[IDA_EKS_Z]);
}

/* Aynı girdiyle 400 adım (değişim hızı sınırı oturur); her adımda değişmez denetlenir. */
static float tekrarla(ida_kon_girdi_t *g)
{
    const float tutma = prm(IDA_PRM_kon_yuzerlik_ff);
    float en_buyuk = -1.0e9f;
    int64_t t = 1000 * DS_MS;
    for (uint32_t n = 0u; n < 400u; n++) {
        g->t_ns = t;
        g->nav.t.ns = t;
        ida_kon_hesapla(&K, &P, g);
        TEST_ASSERT_EQUAL_UINT8(0u, K.cokme);
        const float a = gerceklesen_asagi(g);
        en_buyuk = (a > en_buyuk) ? a : en_buyuk;
        TEST_ASSERT_TRUE_MESSAGE(a <= (tutma + 1.0e-3f), "yasakta gerçekleşen NED aşağı kuvvet tutmayı aştı");
        t += 5 * DS_MS;
    }
    return en_buyuk;
}

/* sol'un hesabı 1: yalpa = baş-kıç = 30°, yön 0; yatay hız isteği F_NED = (−40, −40, 1,82) N;
   Y ekseni kırpması sonrası eski kodda NED aşağı istek 3,488 N. */
static void test_sol_hiz_kanali_yalpa_bas_kic_30(void)
{
    ida_kon_girdi_t g;
    girdi(&g, 1000 * DS_MS, 2.0f, 0.0f, 1u);
    durus(&g, 30.0f * DERECE, 30.0f * DERECE, 0.0f);
    g.istek.yalpa = 30.0f * DERECE;
    g.istek.bas_kic = 30.0f * DERECE;
    g.istek.yatay = (uint8_t)IDA_KON_YATAY_HIZ;
    g.istek.hiz[0] = -1.0f;
    g.istek.hiz[1] = -1.0f;
    (void)tekrarla(&g);
}

/* sol'un hesabı 2: baş-kıç 30°, yalpa 0; doğrudan gövde X = −40 N, Z = 50 N; eski kodda NED
   aşağı 40·sin30° + 1,82·cos30° = 21,576 N. */
static void test_sol_dogrudan_kuvvet_bas_kic_30(void)
{
    ida_kon_girdi_t g;
    girdi(&g, 1000 * DS_MS, 2.0f, 0.0f, 1u);
    durus(&g, 0.0f, 30.0f * DERECE, 0.0f);
    g.istek.bas_kic = 30.0f * DERECE;
    g.istek.dikey = (uint8_t)IDA_KON_DIKEY_KUVVET;
    g.istek.kuvvet[IDA_EKS_Z] = 50.0f;
    g.istek.yatay = (uint8_t)IDA_KON_YATAY_KUVVET;
    g.istek.kuvvet[IDA_EKS_X] = -40.0f;
    (void)tekrarla(&g);
}

static float aralik(ds_rastgele_t *r, float a, float b)
{
    return a + ((b - a) * ((float)(ds_rastgele(r) >> 8) / 16777216.0f));
}

/* Rastgele duruş ve istek taraması (sabit tohum): |yalpa|, |baş-kıç| ≤ 60°, bütün kanallar;
   kontrol belleği koşular arasında sürer (değişim hızı penceresi ve integraller dahil). */
static void test_rastgele_durus_ve_istek_taramasi(void)
{
    ds_rastgele_t r;
    r.s = 0x5A31D1u;
    const float tutma = prm(IDA_PRM_kon_yuzerlik_ff);
    int64_t t = 1000 * DS_MS;
    ida_kon_girdi_t g;
    float en_buyuk = -1.0e9f;
    for (uint32_t blok = 0u; blok < 300u; blok++) {
        girdi(&g, t, aralik(&r, 0.5f, 3.0f), aralik(&r, -0.5f, 0.5f), 1u);
        durus(&g, aralik(&r, -60.0f, 60.0f) * DERECE, aralik(&r, -60.0f, 60.0f) * DERECE,
              aralik(&r, -3.1f, 3.1f));
        g.nav.v_ned[0] = aralik(&r, -0.5f, 0.5f);
        g.nav.v_ned[1] = aralik(&r, -0.5f, 0.5f);
        g.nav.omega_b[0] = aralik(&r, -0.3f, 0.3f);
        g.nav.omega_b[1] = aralik(&r, -0.3f, 0.3f);
        g.nav.omega_b[2] = aralik(&r, -0.3f, 0.3f);
        g.istek.dikey = (uint8_t)(ds_rastgele(&r) % 3u);
        g.istek.tutum = (uint8_t)(ds_rastgele(&r) % 3u);
        g.istek.yon_kanali = (uint8_t)(ds_rastgele(&r) % 3u);
        g.istek.yatay = (uint8_t)(ds_rastgele(&r) % 2u);
        g.istek.derinlik = aralik(&r, 0.0f, 5.0f);
        g.istek.dikey_hiz = aralik(&r, -1.0f, 1.0f);
        g.istek.yalpa = aralik(&r, -0.6f, 0.6f);
        g.istek.bas_kic = aralik(&r, -0.6f, 0.6f);
        g.istek.yalpa_hizi = aralik(&r, -1.0f, 1.0f);
        g.istek.bas_kic_hizi = aralik(&r, -1.0f, 1.0f);
        g.istek.yon = aralik(&r, -3.1f, 3.1f);
        g.istek.yon_hizi = aralik(&r, -1.0f, 1.0f);
        g.istek.hiz[0] = aralik(&r, -1.5f, 1.5f);
        g.istek.hiz[1] = aralik(&r, -1.5f, 1.5f);
        for (uint32_t j = 0u; j < 6u; j++) {
            g.istek.kuvvet[j] = (j < 3u) ? aralik(&r, -100.0f, 100.0f) : aralik(&r, -15.0f, 15.0f);
        }
        if ((ds_rastgele(&r) % 8u) == 0u) {
            g.istek.kip = (uint8_t)IDA_KON_KIP_YUZEY;
        }
        for (uint32_t n = 0u; n < 20u; n++) {
            g.t_ns = t;
            g.nav.t.ns = t;
            ida_kon_hesapla(&K, &P, &g);
            const float a = gerceklesen_asagi(&g);
            en_buyuk = (a > en_buyuk) ? a : en_buyuk;
            TEST_ASSERT_TRUE_MESSAGE((K.cokme != 0u) || (a <= (tutma + 1.0e-3f)),
                                     "rastgele taramada gerçekleşen NED aşağı kuvvet tutmayı aştı");
            t += 5 * DS_MS;
        }
    }
    TEST_ASSERT_TRUE(en_buyuk > 0.0f); /* tarama aşağı kuvvet sınırına gerçekten dayandı */
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_yasakta_asagi_kuvvet_yuzerlik_dengelemesini_asmaz);
    RUN_TEST(test_yasak_yokken_ayni_akis_asagi_kuvvet_uretir);
    RUN_TEST(test_yasakta_dogrudan_kuvvet_sinirli);
    RUN_TEST(test_sol_hiz_kanali_yalpa_bas_kic_30);
    RUN_TEST(test_sol_dogrudan_kuvvet_bas_kic_30);
    RUN_TEST(test_rastgele_durus_ve_istek_taramasi);
    return UNITY_END();
}
