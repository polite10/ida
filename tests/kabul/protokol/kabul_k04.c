/*
 * A1-K04 kabul testleri (sözleşme §4.1, §3.1):
 *   "Sim her çerçeveyi sysid = 250, compid = 25; kart ve SIL koşucusu sysid = 1, compid = 1 ile
 *    gönderir; her alıcı karşı tarafın kimliğini taşımayan çerçeveyi atar (rx_wrong_sys)."
 * Öğeler:
 *   (a) sim gönderir 250/25                       -> test_A1_K04_a_sim_gonderir_250_25
 *   (b) kart (ve SIL koşucusu) gönderir 1/1       -> test_A1_K04_b_kart_gonderir_1_1
 *   (c) alıcı karşı tarafın kimliğini kabul eder  -> test_A1_K04_c_karsi_kimlik_kabul
 *   (d) alıcı başka kimliği atar, rx_wrong_sys; ardından gelen geçerli çerçeve kabul
 *       (sysid ya da compid'den biri yanlış, ikisi yanlış, kendi kimliği) -> test_A1_K04_d_*
 * Çerçeveler sağlaması doğru ve yönü doğru kurulur: yalnız kimlik koşulu sınanır. Kimlik ile
 * başka bir hatanın birlikte olduğu vakalar (denetim sırası) kabul_belirsiz.c'dedir.
 */
#include <stdio.h>
#include <string.h>

#include "kabul_ortak.h"
#include "unity.h"

void setUp(void) {}
void tearDown(void) {}

static uint8_t akis[8192];
static kabul_sonuc_t sonuc;

static void gonderen_kimligi(ida_link_rol_t rol, uint8_t sysid, uint8_t compid)
{
    kabul_rng_t r;
    kabul_rng_kur(&r, 0x4B30340001ull + (uint64_t)rol);
    ida_link_gonderici_t g;
    TEST_ASSERT_EQUAL_INT(IDA_LINK_TAMAM, ida_link_gonderici_kur(&g, rol));
    size_t sayi = 0;
    for (size_t i = 0; i < KABUL_MESAJ_SAYISI; ++i) {
        const kabul_mesaj_t *m = &kabul_mesajlar[i];
        if ((m->yon & kabul_rol_gonderir_yon(rol)) == 0u) {
            continue;
        }
        for (int tekrar = 0; tekrar < 4; ++tekrar) {
            uint8_t yuk[IDA_LINK_YUK_AZAMI] = {0};
            for (size_t k = 0; k < m->yuk_azami; ++k) {
                yuk[k] = kabul_rng_bayt(&r);
            }
            kabul_yapi_t y;
            kabul_yapi_hazirla(&y, m, yuk);
            uint8_t cikis[IDA_LINK_CERCEVE_AZAMI];
            size_t yazilan = 0;
            TEST_ASSERT_EQUAL_INT_MESSAGE(IDA_LINK_TAMAM,
                ida_link_gonder(&g, m->msgid, y.b, m->yapi_boyu, cikis, sizeof cikis, &yazilan), m->ad);
            kabul_cozulen_t c;
            TEST_ASSERT_EQUAL_INT_MESSAGE(0, kabul_cerceve_dogrula(cikis, yazilan, &c), m->ad);
            TEST_ASSERT_EQUAL_UINT8_MESSAGE(sysid, c.sysid, m->ad);
            TEST_ASSERT_EQUAL_UINT8_MESSAGE(compid, c.compid, m->ad);
            ++sayi;
        }
    }
    TEST_ASSERT_TRUE(sayi > 0u);
}

static void test_A1_K04_a_sim_gonderir_250_25(void)
{
    gonderen_kimligi(IDA_LINK_ROL_SIM, 250u, 25u);
}

static void test_A1_K04_b_kart_gonderir_1_1(void)
{
    gonderen_kimligi(IDA_LINK_ROL_KART, 1u, 1u);
}

/* Rolün alabileceği her mesaj için karşı tarafın kimliğiyle kurulan çerçeve kabul edilir ve
   teslim edilen sysid/compid karşı tarafınkidir. */
static void test_A1_K04_c_karsi_kimlik_kabul(void)
{
    static const ida_link_rol_t ROLLER[2] = {IDA_LINK_ROL_KART, IDA_LINK_ROL_SIM};
    kabul_rng_t r;
    kabul_rng_kur(&r, 0x4B30340003ull);
    for (size_t ri = 0; ri < 2; ++ri) {
        ida_link_rol_t rol = ROLLER[ri];
        size_t n = 0;
        size_t beklenen = 0;
        for (size_t i = 0; i < KABUL_MESAJ_SAYISI; ++i) {
            const kabul_mesaj_t *m = &kabul_mesajlar[i];
            if ((m->yon & kabul_rol_alir_yon(rol)) == 0u) {
                continue;
            }
            n += kabul_temiz_kur(&akis[n], &r, m->msgid, (uint8_t)beklenen, kabul_karsi_sysid(rol),
                                 kabul_karsi_compid(rol), m->yuk_azami / 2u, 1, NULL);
            ++beklenen;
        }
        kabul_besle_yeni(rol, akis, n, &sonuc);
        TEST_ASSERT_EQUAL_UINT32((uint32_t)beklenen, (uint32_t)sonuc.sayi);
        TEST_ASSERT_EQUAL_UINT32(0u, sonuc.sayac.rx_wrong_sys);
        TEST_ASSERT_EQUAL_UINT32(0u, kabul_ret_toplami(&sonuc.sayac));
        for (size_t k = 0; k < sonuc.sayi; ++k) {
            TEST_ASSERT_EQUAL_UINT8(kabul_karsi_sysid(rol), sonuc.t[k].sysid);
            TEST_ASSERT_EQUAL_UINT8(kabul_karsi_compid(rol), sonuc.t[k].compid);
        }
    }
}

typedef struct {
    uint8_t sysid, compid;
} kimlik_t;

static void yanlis_kimlikler(ida_link_rol_t rol, const kimlik_t *k, size_t sayi, uint64_t tohum)
{
    kabul_rng_t r;
    kabul_rng_kur(&r, tohum);
    /* Rolün alabileceği bir mesaj: kart için IDA_IMU (sim -> kart), sim için IDA_NAV
       (kart -> sim); iki yönlü IDA_KEEPALIVE de denenir. */
    const uint32_t mesaj[2] = {rol == IDA_LINK_ROL_KART ? 46020u : 46063u, 46003u};
    for (size_t mi = 0; mi < 2; ++mi) {
        for (size_t i = 0; i < sayi; ++i) {
            const kabul_mesaj_t *m = kabul_mesaj_bul(mesaj[mi]);
            size_t n = kabul_temiz_ham(akis, &r, m->yuk_azami, 0u, 0u, 40u, k[i].sysid, k[i].compid,
                                       m->msgid, m->crc_extra, 0u);
            n += kabul_temiz_kur(&akis[n], &r, m->msgid, 41u, kabul_karsi_sysid(rol),
                                 kabul_karsi_compid(rol), 2u, 1, NULL);
            kabul_besle_yeni(rol, akis, n, &sonuc);
            char ileti[96];
            snprintf(ileti, sizeof ileti, "%s, sysid %u compid %u", m->ad, (unsigned)k[i].sysid,
                     (unsigned)k[i].compid);
            TEST_ASSERT_EQUAL_UINT32_MESSAGE(1u, sonuc.sayac.rx_wrong_sys, ileti);
            TEST_ASSERT_EQUAL_UINT32_MESSAGE(1u, kabul_ret_toplami(&sonuc.sayac), ileti);
            TEST_ASSERT_EQUAL_UINT32_MESSAGE(1u, (uint32_t)sonuc.sayi, ileti);
            TEST_ASSERT_EQUAL_UINT8_MESSAGE(41u, sonuc.t[0].seq, ileti);
            TEST_ASSERT_EQUAL_UINT32_MESSAGE(1u, sonuc.sayac.rx_frames, ileti);
        }
    }
}

static void test_A1_K04_d_kart_alicisi_yanlis_kimligi_atar(void)
{
    static const kimlik_t K[] = {
        {1u, 1u},     /* kendi kimliği (geri döngü) */
        {250u, 1u},   /* compid yanlış */
        {250u, 24u},  {250u, 26u}, {250u, 0u},
        {1u, 25u},    /* sysid yanlış */
        {249u, 25u},  {251u, 25u}, {0u, 25u}, {255u, 25u},
        {0u, 0u},     {255u, 190u}, {25u, 250u},
    };
    yanlis_kimlikler(IDA_LINK_ROL_KART, K, sizeof K / sizeof K[0], 0x4B30340004ull);
}

static void test_A1_K04_d_sim_alicisi_yanlis_kimligi_atar(void)
{
    static const kimlik_t K[] = {
        {250u, 25u},  /* kendi kimliği */
        {1u, 0u},     {1u, 2u}, {1u, 25u}, {1u, 255u},
        {0u, 1u},     {2u, 1u}, {250u, 1u}, {255u, 1u},
        {0u, 0u},     {255u, 190u},
    };
    yanlis_kimlikler(IDA_LINK_ROL_SIM, K, sizeof K / sizeof K[0], 0x4B30340005ull);
}

/* Karışık akış: geçerli ve yanlış kimlikli çerçeveler sırayla; yalnız geçerliler teslim
   edilir, rx_wrong_sys yanlışların sayısıdır. */
static void test_A1_K04_d_karisik_akis(void)
{
    kabul_rng_t r;
    kabul_rng_kur(&r, 0x4B30340006ull);
    size_t n = 0;
    uint32_t yanlis = 0;
    uint32_t dogru = 0;
    for (uint8_t s = 0; s < 60u; ++s) {
        const kabul_mesaj_t *m = kabul_mesaj_bul(46022u);
        if (kabul_rng_aralik(&r, 3u) == 0u) {
            n += kabul_temiz_ham(&akis[n], &r, m->yuk_azami, 0u, 0u, s, 1u, 1u, m->msgid,
                                 m->crc_extra, 0u);
            ++yanlis;
        } else {
            n += kabul_temiz_kur(&akis[n], &r, m->msgid, s, 250u, 25u, 9u, 1, NULL);
            ++dogru;
        }
    }
    kabul_besle_yeni(IDA_LINK_ROL_KART, akis, n, &sonuc);
    TEST_ASSERT_EQUAL_UINT32(yanlis, sonuc.sayac.rx_wrong_sys);
    TEST_ASSERT_EQUAL_UINT32(dogru, (uint32_t)sonuc.sayi);
    TEST_ASSERT_EQUAL_UINT32(dogru, sonuc.sayac.rx_frames);
    TEST_ASSERT_EQUAL_UINT32(yanlis, kabul_ret_toplami(&sonuc.sayac));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_A1_K04_a_sim_gonderir_250_25);
    RUN_TEST(test_A1_K04_b_kart_gonderir_1_1);
    RUN_TEST(test_A1_K04_c_karsi_kimlik_kabul);
    RUN_TEST(test_A1_K04_d_kart_alicisi_yanlis_kimligi_atar);
    RUN_TEST(test_A1_K04_d_sim_alicisi_yanlis_kimligi_atar);
    RUN_TEST(test_A1_K04_d_karisik_akis);
    return UNITY_END();
}
