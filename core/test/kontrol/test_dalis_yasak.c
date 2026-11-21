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

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_yasakta_asagi_kuvvet_yuzerlik_dengelemesini_asmaz);
    RUN_TEST(test_yasak_yokken_ayni_akis_asagi_kuvvet_uretir);
    RUN_TEST(test_yasakta_dogrudan_kuvvet_sinirli);
    return UNITY_END();
}
