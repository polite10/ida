/*
 * A1-K05 kabul testleri (sözleşme §4.1, §3.2 "Yük kırpma", Tablo 3.3):
 *   "Gönderen yükün sondaki sıfır baytlarını, ilk baytı koruyarak kırpar; alıcı kırpılmış ve
 *    kırpılmamış çerçeveyi kabul eder, eksik kısmı sıfırla doldurur ve len = 0 ya da len
 *    mesajın en büyük yük boyundan büyükse çerçeveyi atar (rx_bad_len)."
 * Öğeler:
 *   (a) gönderen sondaki sıfırları kırpar (len = son sıfır olmayan bayt + 1)
 *   (b) gönderen ilk baytı kırpmaz (bütünü sıfır yükte len = 1)
 *   (c) alıcı kırpılmış çerçeveyi kabul eder (her len, 1..azami)
 *   (d) alıcı kırpılmamış çerçeveyi kabul eder (len = azami, sonu sıfır)
 *   (e) alıcı eksik kısmı sıfırla doldurur (teslim ve ida_link_coz; önceki uzun çerçeveden
 *       artık kalmaz)
 *   (f) len = 0 atılır, rx_bad_len
 *   (g) len > azami atılır, rx_bad_len (azami + 1 ve 255; her mesaj, iki rol)
 * Sınır değerler: len = 1 ve len = azami kabul, len = 0 ve azami + 1 ret.
 */
#include <stdio.h>
#include <string.h>

#include "kabul_ortak.h"
#include "unity.h"

void setUp(void) {}
void tearDown(void) {}

static uint8_t akis[16384];
static kabul_sonuc_t sonuc;
static kabul_sonuc_t sonuc2;

static const ida_link_rol_t ROLLER[2] = {IDA_LINK_ROL_KART, IDA_LINK_ROL_SIM};

/* (a), (b): gönderilen çerçeve, aynı yükün referans kodlayıcıyla kırpılmış çerçevesine bayt
   bayt eşittir ve len beklenen kırpılmış boydur. */
static void gonder_ve_karsilastir(ida_link_gonderici_t *g, ida_link_rol_t rol,
                                  const kabul_mesaj_t *m, const uint8_t *yuk, size_t beklenen_len)
{
    char ileti[128];
    kabul_yapi_t y;
    kabul_yapi_hazirla(&y, m, yuk);
    uint8_t seq = g->seq;
    uint8_t cikis[IDA_LINK_CERCEVE_AZAMI];
    uint8_t ref[IDA_LINK_CERCEVE_AZAMI];
    size_t yazilan = 0;
    TEST_ASSERT_EQUAL_INT_MESSAGE(IDA_LINK_TAMAM,
        ida_link_gonder(g, m->msgid, y.b, m->yapi_boyu, cikis, sizeof cikis, &yazilan), m->ad);
    size_t rn = kabul_cerceve_kur(ref, m->msgid, seq, kabul_kendi_sysid(rol), kabul_kendi_compid(rol),
                                  yuk, m->yuk_azami, 1);
    snprintf(ileti, sizeof ileti, "%s: len (beklenen %u)", m->ad, (unsigned)beklenen_len);
    TEST_ASSERT_EQUAL_UINT8_MESSAGE((uint8_t)beklenen_len, cikis[1], ileti);
    TEST_ASSERT_EQUAL_UINT32_MESSAGE((uint32_t)(12u + beklenen_len), (uint32_t)yazilan, ileti);
    TEST_ASSERT_EQUAL_UINT32_MESSAGE((uint32_t)rn, (uint32_t)yazilan, ileti);
    snprintf(ileti, sizeof ileti, "%s: referans kodlayıcıyla bayt bayt aynı değil", m->ad);
    TEST_ASSERT_EQUAL_HEX8_ARRAY_MESSAGE(ref, cikis, rn, ileti);
}

static void test_A1_K05_ab_gonderen_kirpar_ilk_bayti_korur(void)
{
    kabul_rng_t r;
    kabul_rng_kur(&r, 0x4B30350001ull);
    for (size_t ri = 0; ri < 2; ++ri) {
        ida_link_rol_t rol = ROLLER[ri];
        ida_link_gonderici_t g;
        TEST_ASSERT_EQUAL_INT(IDA_LINK_TAMAM, ida_link_gonderici_kur(&g, rol));
        for (size_t i = 0; i < KABUL_MESAJ_SAYISI; ++i) {
            const kabul_mesaj_t *m = &kabul_mesajlar[i];
            if ((m->yon & kabul_rol_gonderir_yon(rol)) == 0u) {
                continue;
            }
            uint8_t yuk[IDA_LINK_YUK_AZAMI];
            /* (b) bütünü sıfır: len = 1 (ilk bayt korunur). */
            memset(yuk, 0, sizeof yuk);
            gonder_ve_karsilastir(&g, rol, m, yuk, 1u);
            /* (b) yalnız ilk bayt dolu: len = 1. */
            yuk[0] = 0x5Au;
            gonder_ve_karsilastir(&g, rol, m, yuk, 1u);
            if (m->yuk_azami >= 2u) {
                /* (b) ilk bayt sıfır, ikinci dolu: ilk bayt kırpılmaz, len = 2. */
                memset(yuk, 0, sizeof yuk);
                yuk[1] = 0x01u;
                gonder_ve_karsilastir(&g, rol, m, yuk, 2u);
            }
            /* (a) son bayt dolu: kırpma yok, len = azami. */
            memset(yuk, 0, sizeof yuk);
            yuk[m->yuk_azami - 1u] = 0x80u;
            gonder_ve_karsilastir(&g, rol, m, yuk, m->yuk_azami);
            /* (a) ortada sıfırlar, sonda sıfırlar: len = son sıfır olmayan + 1. */
            for (int tekrar = 0; tekrar < 6; ++tekrar) {
                memset(yuk, 0, sizeof yuk);
                size_t son = kabul_rng_aralik(&r, m->yuk_azami);
                for (size_t k = 0; k <= son; ++k) {
                    yuk[k] = (kabul_rng_aralik(&r, 3u) == 0u) ? 0u : kabul_rng_bayt(&r);
                }
                yuk[son] = (uint8_t)(1u + kabul_rng_aralik(&r, 255u));
                gonder_ve_karsilastir(&g, rol, m, yuk, son + 1u);
            }
        }
    }
}

/* Teslim edilen yükün ida_link_coz ile yapıya çözülmesi: yapının ilk yuk_azami baytı
   beklenen (sıfırla tamamlanmış) yük, kalanı sıfır (açık başlık: yapı önce sıfırlanır). */
static void coz_ve_karsilastir(const kabul_teslim_t *t, const kabul_mesaj_t *m, const uint8_t *beklenen)
{
    ida_link_cerceve_t c;
    memset(&c, 0, sizeof c);
    c.msgid = t->msgid;
    c.seq = t->seq;
    c.sysid = t->sysid;
    c.compid = t->compid;
    c.len = t->len;
    c.yuk_azami = t->yuk_azami;
    memcpy(c.yuk, t->yuk, sizeof c.yuk);
    kabul_yapi_t y;
    memset(y.b, 0xA5, sizeof y.b);
    TEST_ASSERT_EQUAL_INT_MESSAGE(IDA_LINK_TAMAM, ida_link_coz(&c, m->msgid, y.b, m->yapi_boyu), m->ad);
    TEST_ASSERT_EQUAL_HEX8_ARRAY_MESSAGE(beklenen, y.b, m->yuk_azami, m->ad);
    for (size_t k = m->yuk_azami; k < m->yapi_boyu; ++k) {
        TEST_ASSERT_EQUAL_HEX8_MESSAGE(0u, y.b[k], "ida_link_coz: yapının dolgu baytı sıfırlanmadı");
    }
}

/* (c), (d), (e): rolün alabileceği her mesaj için len ∈ {1, 2, azami/2, azami − 1, azami}
   kırpılmış (sonu sıfır olmayan) ve kırpılmamış (len = azami, sonu sıfır) biçimler; her biri
   kabul edilir, teslim edilen yük sıfırla tamamlanmış yüke eşittir ve iki biçim aynı yükü verir. */
static void test_A1_K05_cde_kirpilmis_ve_kirpilmamis_kabul(void)
{
    kabul_rng_t r;
    kabul_rng_kur(&r, 0x4B30350002ull);
    for (size_t ri = 0; ri < 2; ++ri) {
        ida_link_rol_t rol = ROLLER[ri];
        for (size_t i = 0; i < KABUL_MESAJ_SAYISI; ++i) {
            const kabul_mesaj_t *m = &kabul_mesajlar[i];
            if ((m->yon & kabul_rol_alir_yon(rol)) == 0u) {
                continue;
            }
            const size_t boylar[5] = {1u, 2u, m->yuk_azami / 2u, m->yuk_azami - 1u, m->yuk_azami};
            for (size_t b = 0; b < 5; ++b) {
                size_t dolu = boylar[b] == 0u ? 1u : boylar[b];
                if (dolu > m->yuk_azami) {
                    continue;
                }
                uint8_t yuk[IDA_LINK_YUK_AZAMI];
                size_t n1 = kabul_temiz_kur(akis, &r, m->msgid, 1u, kabul_karsi_sysid(rol),
                                            kabul_karsi_compid(rol), dolu, 1, yuk);
                TEST_ASSERT_EQUAL_UINT32(12u + dolu, (uint32_t)n1);
                kabul_besle_yeni(rol, akis, n1, &sonuc);
                TEST_ASSERT_EQUAL_UINT32_MESSAGE(1u, (uint32_t)sonuc.sayi, m->ad);
                TEST_ASSERT_EQUAL_UINT8_MESSAGE((uint8_t)dolu, sonuc.t[0].len, m->ad);
                TEST_ASSERT_EQUAL_HEX8_ARRAY_MESSAGE(yuk, sonuc.t[0].yuk, IDA_LINK_YUK_AZAMI, m->ad);
                coz_ve_karsilastir(&sonuc.t[0], m, yuk);

                /* Aynı yükün kırpılmamış biçimi (len = azami). */
                size_t n2 = kabul_cerceve_kur(akis, m->msgid, 1u, kabul_karsi_sysid(rol),
                                              kabul_karsi_compid(rol), yuk, m->yuk_azami, 0);
                TEST_ASSERT_EQUAL_UINT32(12u + m->yuk_azami, (uint32_t)n2);
                kabul_besle_yeni(rol, akis, n2, &sonuc2);
                TEST_ASSERT_EQUAL_UINT32_MESSAGE(1u, (uint32_t)sonuc2.sayi, m->ad);
                TEST_ASSERT_EQUAL_UINT8(m->yuk_azami, sonuc2.t[0].len);
                TEST_ASSERT_EQUAL_HEX8_ARRAY_MESSAGE(sonuc.t[0].yuk, sonuc2.t[0].yuk,
                                                     IDA_LINK_YUK_AZAMI, m->ad);
                TEST_ASSERT_EQUAL_UINT32(0u, kabul_ret_toplami(&sonuc.sayac));
                TEST_ASSERT_EQUAL_UINT32(0u, kabul_ret_toplami(&sonuc2.sayac));
            }
        }
    }
}

/* (c): kısmen kırpılmış çerçeve (1 <= len < azami, telde son bayt sıfır) da kabul edilir:
   sözleşme yalnız len = 0 ve len > azami'yi reddeder. */
static void test_A1_K05_c_kismen_kirpilmis_kabul(void)
{
    kabul_rng_t r;
    kabul_rng_kur(&r, 0x4B30350003ull);
    const kabul_mesaj_t *m = kabul_mesaj_bul(46024u); /* IDA_DVL, 77 B */
    for (uint8_t len = 2u; len < m->yuk_azami; len = (uint8_t)(len + 7u)) {
        size_t n = kabul_temiz_ham(akis, &r, len, 0u, 0u, 3u, 250u, 25u, m->msgid, m->crc_extra, 1u);
        TEST_ASSERT_EQUAL_HEX8(0u, akis[10u + len - 1u]);
        kabul_besle_yeni(IDA_LINK_ROL_KART, akis, n, &sonuc);
        TEST_ASSERT_EQUAL_UINT32(1u, (uint32_t)sonuc.sayi);
        TEST_ASSERT_EQUAL_UINT8(len, sonuc.t[0].len);
        TEST_ASSERT_EQUAL_HEX8_ARRAY(&akis[10], sonuc.t[0].yuk, len);
    }
}

/* (e): aynı alıcıda önce uzun, sonra kısa (kırpılmış) çerçeve: kısa çerçevenin yükünün len
   sonrası sıfırdır (önceki çerçeveden artık kalmaz). kabul_besle her teslimde len sonrasını
   denetler; burada ayrıca beklenen yük karşılaştırılır. */
static void test_A1_K05_e_onceki_cerceveden_artik_kalmaz(void)
{
    kabul_rng_t r;
    kabul_rng_kur(&r, 0x4B30350004ull);
    uint8_t y1[IDA_LINK_YUK_AZAMI];
    uint8_t y2[IDA_LINK_YUK_AZAMI];
    size_t n = kabul_temiz_kur(akis, &r, 46030u, 0u, 250u, 25u, 246u, 1, y1);
    n += kabul_temiz_kur(&akis[n], &r, 46030u, 1u, 250u, 25u, 3u, 1, y2);
    n += kabul_temiz_kur(&akis[n], &r, 46030u, 2u, 250u, 25u, 0u, 1, NULL);
    kabul_besle_yeni(IDA_LINK_ROL_KART, akis, n, &sonuc);
    TEST_ASSERT_EQUAL_UINT32(3u, (uint32_t)sonuc.sayi);
    TEST_ASSERT_EQUAL_HEX8_ARRAY(y1, sonuc.t[0].yuk, IDA_LINK_YUK_AZAMI);
    TEST_ASSERT_EQUAL_HEX8_ARRAY(y2, sonuc.t[1].yuk, IDA_LINK_YUK_AZAMI);
    TEST_ASSERT_EQUAL_UINT8(1u, sonuc.t[2].len);
    for (size_t k = 0; k < IDA_LINK_YUK_AZAMI; ++k) {
        TEST_ASSERT_EQUAL_HEX8(0u, sonuc.t[2].yuk[k]);
    }
}

/* Kötü len'li çerçeve (sağlaması o len'le doğru, baytları temiz) + ardından geçerli çerçeve. */
static void kotu_len(ida_link_rol_t rol, const kabul_mesaj_t *m, uint8_t len, kabul_rng_t *r)
{
    char ileti[96];
    size_t n = kabul_temiz_ham(akis, r, len, 0u, 0u, 20u, kabul_karsi_sysid(rol),
                               kabul_karsi_compid(rol), m->msgid, m->crc_extra, 0u);
    n += kabul_temiz_kur(&akis[n], r, m->msgid, 21u, kabul_karsi_sysid(rol), kabul_karsi_compid(rol),
                         1u, 1, NULL);
    kabul_besle_yeni(rol, akis, n, &sonuc);
    snprintf(ileti, sizeof ileti, "%s len %u (azami %u)", m->ad, (unsigned)len, (unsigned)m->yuk_azami);
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(1u, sonuc.sayac.rx_bad_len, ileti);
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(1u, kabul_ret_toplami(&sonuc.sayac), ileti);
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(1u, (uint32_t)sonuc.sayi, ileti);
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(21u, sonuc.t[0].seq, ileti);
}

/* (f): len = 0, rolün alabileceği her mesaj. */
static void test_A1_K05_f_len_sifir_atilir(void)
{
    kabul_rng_t r;
    kabul_rng_kur(&r, 0x4B30350005ull);
    for (size_t ri = 0; ri < 2; ++ri) {
        for (size_t i = 0; i < KABUL_MESAJ_SAYISI; ++i) {
            const kabul_mesaj_t *m = &kabul_mesajlar[i];
            if (m->yon & kabul_rol_alir_yon(ROLLER[ri])) {
                kotu_len(ROLLER[ri], m, 0u, &r);
            }
        }
    }
}

/* (g): len = azami + 1, azami + 2 ve 255, rolün alabileceği her mesaj. */
static void test_A1_K05_g_len_azamiyi_asan_atilir(void)
{
    kabul_rng_t r;
    kabul_rng_kur(&r, 0x4B30350006ull);
    for (size_t ri = 0; ri < 2; ++ri) {
        for (size_t i = 0; i < KABUL_MESAJ_SAYISI; ++i) {
            const kabul_mesaj_t *m = &kabul_mesajlar[i];
            if ((m->yon & kabul_rol_alir_yon(ROLLER[ri])) == 0u) {
                continue;
            }
            TEST_ASSERT_TRUE(m->yuk_azami < 254u);
            kotu_len(ROLLER[ri], m, (uint8_t)(m->yuk_azami + 1u), &r);
            kotu_len(ROLLER[ri], m, (uint8_t)(m->yuk_azami + 2u), &r);
            kotu_len(ROLLER[ri], m, 255u, &r);
        }
    }
}

/* Sınır: azami (kabul) ile azami + 1 (ret) aynı akışta; rx_bad_len yalnız ret için artar. */
static void test_A1_K05_g_sinir_azami_ve_bir_fazlasi(void)
{
    kabul_rng_t r;
    kabul_rng_kur(&r, 0x4B30350007ull);
    const kabul_mesaj_t *m = kabul_mesaj_bul(46030u);
    size_t n = kabul_temiz_ham(akis, &r, m->yuk_azami, 0u, 0u, 0u, 250u, 25u, m->msgid, m->crc_extra, 0u);
    n += kabul_temiz_ham(&akis[n], &r, (uint8_t)(m->yuk_azami + 1u), 0u, 0u, 1u, 250u, 25u, m->msgid,
                         m->crc_extra, 0u);
    n += kabul_temiz_ham(&akis[n], &r, 1u, 0u, 0u, 2u, 250u, 25u, m->msgid, m->crc_extra, 0u);
    kabul_besle_yeni(IDA_LINK_ROL_KART, akis, n, &sonuc);
    TEST_ASSERT_EQUAL_UINT32(2u, (uint32_t)sonuc.sayi);
    TEST_ASSERT_EQUAL_UINT8(m->yuk_azami, sonuc.t[0].len);
    TEST_ASSERT_EQUAL_UINT8(1u, sonuc.t[1].len);
    TEST_ASSERT_EQUAL_UINT32(1u, sonuc.sayac.rx_bad_len);
    TEST_ASSERT_EQUAL_UINT32(1u, kabul_ret_toplami(&sonuc.sayac));
}

/* Sözleşme tablosu ile kitaplığın dialect bilgisi (en büyük yük boyu, CRC_EXTRA, yön) aynıdır:
   K05'in "mesajın en büyük yük boyu" ve K06'nın yönü bu tabloya dayanır. Ayrıca A1'in kendi
   içinde Tablo 3.3, Ek B başlık satırı ve ida.xml (A1 §3.2 tanımıyla hesaplanan) aynıdır. */
static void test_A1_K05_tablo_sozlesme_ile_ayni(void)
{
    for (size_t i = 0; i < KABUL_MESAJ_SAYISI; ++i) {
        const kabul_mesaj_t *m = &kabul_mesajlar[i];
        TEST_ASSERT_EQUAL_UINT8_MESSAGE(m->yuk_azami, m->yuk_azami_xml, m->ad);
        TEST_ASSERT_EQUAL_UINT8_MESSAGE(m->yuk_azami, m->yuk_azami_ek_b, m->ad);
        TEST_ASSERT_EQUAL_UINT16_MESSAGE(12u + m->yuk_azami, m->cerceve_azami, m->ad);
        TEST_ASSERT_EQUAL_UINT8_MESSAGE(m->crc_extra, m->crc_extra_xml, m->ad);
        ida_link_mesaj_bilgisi_t b;
        memset(&b, 0, sizeof b);
        TEST_ASSERT_EQUAL_INT_MESSAGE(IDA_LINK_TAMAM, ida_link_mesaj_bilgisi(m->msgid, &b), m->ad);
        TEST_ASSERT_EQUAL_UINT32_MESSAGE(m->msgid, b.msgid, m->ad);
        TEST_ASSERT_EQUAL_UINT8_MESSAGE(m->yuk_azami, b.yuk_azami, m->ad);
        TEST_ASSERT_EQUAL_UINT8_MESSAGE(m->crc_extra, b.crc_extra, m->ad);
        TEST_ASSERT_EQUAL_UINT8_MESSAGE(m->yon, b.yon, m->ad);
        TEST_ASSERT_EQUAL_UINT16_MESSAGE(m->yapi_boyu, b.yapi_boyu, m->ad);
    }
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_A1_K05_tablo_sozlesme_ile_ayni);
    RUN_TEST(test_A1_K05_ab_gonderen_kirpar_ilk_bayti_korur);
    RUN_TEST(test_A1_K05_cde_kirpilmis_ve_kirpilmamis_kabul);
    RUN_TEST(test_A1_K05_c_kismen_kirpilmis_kabul);
    RUN_TEST(test_A1_K05_e_onceki_cerceveden_artik_kalmaz);
    RUN_TEST(test_A1_K05_f_len_sifir_atilir);
    RUN_TEST(test_A1_K05_g_len_azamiyi_asan_atilir);
    RUN_TEST(test_A1_K05_g_sinir_azami_ve_bir_fazlasi);
    return UNITY_END();
}
