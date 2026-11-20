/*
 * A1 Ek B kabul testleri: 38 altın çerçeve (37 mesaj + A1-K51 ek vakası).
 *
 * Diziler, başlık satırları ve "Alanlar" satırları sözleşme metninden derleme sırasında
 * ek_b_cikar.py ile çıkarılır (uygulayıcının veri dosyası kullanılmaz). Her çerçeve için:
 *   (1) sözleşmenin iç tutarlılığı, kitaplıktan bağımsız: bayt sayısı, başlık baytları, CRC
 *       (referans CRC-16/MCRF4XX + Ek B CRC_EXTRA), CRC_EXTRA ve en büyük yük boyunun Tablo 3.3
 *       ve ida.xml (A1 §3.2 tanımıyla hesaplanan) ile eşitliği, "Alanlar" değerlerinin teldeki
 *       baytlarla eşitliği (betiğin hesapladığı tel sırasıyla);
 *   (2) alıcı kabul eder; kimlik, seq, sysid/compid, yük boyu ve yük baytları doğru;
 *   (3) ida_link_coz ile çözülen tipli yapının adlı alanları "Alanlar" satırındaki değerlerdir;
 *   (4) yeniden kodlama bayt bayt aynı (çözülen yapıdan ve referansın kurduğu yapıdan);
 *   (5) kırpılmamış biçim (referans kodlayıcıyla, len = en büyük yük) kabul edilir ve aynı
 *       yükü ve aynı yapıyı verir; iki yönlü mesajlar diğer rolde de kabul edilir;
 *   (6) her iki parçaya bölme ve tek bayt besleme aynı teslimi verir.
 */
#include <stdio.h>
#include <string.h>

#include "kabul_ortak.h"
#include "unity.h"

void setUp(void) {}
void tearDown(void) {}

static kabul_sonuc_t sonuc;
static kabul_sonuc_t sonuc2;
static uint8_t akis[16384];

static ida_link_rol_t alici_rolu(const kabul_altin_t *a)
{
    return a->sysid == 250u ? IDA_LINK_ROL_KART : IDA_LINK_ROL_SIM;
}

static ida_link_rol_t gonderen_rolu(const kabul_altin_t *a)
{
    return a->sysid == 250u ? IDA_LINK_ROL_SIM : IDA_LINK_ROL_KART;
}

static void yuk_al(const kabul_altin_t *a, uint8_t *yuk)
{
    memset(yuk, 0, IDA_LINK_YUK_AZAMI);
    memcpy(yuk, &a->bayt[10], a->yuk_bu);
}

static void test_A1_EkB_1_sozlesme_ic_tutarlilik(void)
{
    for (size_t i = 0; kabul_ek_b_uyumsuzluk[i] != NULL; ++i) {
        TEST_MESSAGE(kabul_ek_b_uyumsuzluk[i]);
    }
    TEST_ASSERT_NULL_MESSAGE(kabul_ek_b_uyumsuzluk[0], "Ek B ile ida.xml/Tablo 3.3 arasında uyumsuzluk");
    TEST_ASSERT_EQUAL_UINT32(38u, KABUL_ALTIN_SAYISI);
    uint8_t goruldu[KABUL_MESAJ_SAYISI] = {0};
    for (size_t i = 0; i < KABUL_ALTIN_SAYISI; ++i) {
        const kabul_altin_t *a = &kabul_altin[i];
        const kabul_mesaj_t *m = kabul_mesaj_bul(a->msgid);
        TEST_ASSERT_NOT_NULL_MESSAGE(m, a->baslik);
        goruldu[m - kabul_mesajlar] = 1u;
        TEST_ASSERT_EQUAL_UINT8_MESSAGE((uint8_t)i, a->seq, a->baslik);
        TEST_ASSERT_EQUAL_UINT16_MESSAGE(a->cerceve_bu, a->n, a->baslik);
        TEST_ASSERT_EQUAL_UINT16_MESSAGE(12u + a->yuk_bu, a->cerceve_bu, a->baslik);
        TEST_ASSERT_EQUAL_UINT8_MESSAGE(m->yuk_azami, a->yuk_azami, a->baslik);
        TEST_ASSERT_EQUAL_UINT16_MESSAGE(m->cerceve_azami, a->cerceve_azami, a->baslik);
        TEST_ASSERT_EQUAL_UINT8_MESSAGE(m->crc_extra, a->crc_extra, a->baslik);
        TEST_ASSERT_EQUAL_UINT8_MESSAGE(m->crc_extra_xml, a->crc_extra, a->baslik);
        /* Gönderen kimliği yönle tutarlı (Ek B: sim'den çıkanlar 250/25, karttan 1/1). */
        TEST_ASSERT_TRUE_MESSAGE((a->sysid == 250u && a->compid == 25u) || (a->sysid == 1u && a->compid == 1u),
                                 a->baslik);
        TEST_ASSERT_TRUE_MESSAGE((m->yon & kabul_rol_gonderir_yon(gonderen_rolu(a))) != 0u, a->baslik);
        kabul_cozulen_t c;
        TEST_ASSERT_EQUAL_INT_MESSAGE(0, kabul_cerceve_dogrula(a->bayt, a->n, &c), a->baslik);
        TEST_ASSERT_EQUAL_UINT32_MESSAGE(a->msgid, c.msgid, a->baslik);
        TEST_ASSERT_EQUAL_UINT8_MESSAGE(a->yuk_bu, c.len, a->baslik);
        TEST_ASSERT_EQUAL_UINT8_MESSAGE(a->seq, c.seq, a->baslik);
        TEST_ASSERT_EQUAL_UINT8_MESSAGE(a->sysid, c.sysid, a->baslik);
        TEST_ASSERT_EQUAL_UINT8_MESSAGE(a->compid, c.compid, a->baslik);
        TEST_ASSERT_EQUAL_HEX8_MESSAGE(0u, c.incompat, a->baslik);
        TEST_ASSERT_EQUAL_HEX8_MESSAGE(0u, c.compat, a->baslik);
        /* Altın çerçeveler A1-K05'e göre kırpılmıştır: son yük baytı sıfır değil (ya da len 1). */
        TEST_ASSERT_EQUAL_UINT32_MESSAGE((uint32_t)kabul_kirpilmis_boy(c.yuk, a->yuk_azami),
                                         (uint32_t)a->yuk_bu, a->baslik);
        /* "Alanlar" değerleri teldeki baytlarla aynı. */
        for (size_t k = 0; k < a->alan_sayisi; ++k) {
            const kabul_alan_t *f = &a->alanlar[k];
            uint64_t v = 0;
            for (size_t b = 0; b < f->boy; ++b) {
                v |= (uint64_t)c.yuk[f->ofset + b] << (8u * b);
            }
            char ileti[160];
            snprintf(ileti, sizeof ileti, "%s: alan %s telde 0x%llx, Alanlar satırında 0x%llx", a->baslik,
                     f->ad, (unsigned long long)v, (unsigned long long)f->ham);
            TEST_ASSERT_TRUE_MESSAGE(v == f->ham, ileti);
        }
    }
    for (size_t i = 0; i < KABUL_MESAJ_SAYISI; ++i) {
        TEST_ASSERT_TRUE_MESSAGE(goruldu[i], kabul_mesajlar[i].ad);
    }
}

static void tek_teslim_denetle(const kabul_altin_t *a, const kabul_sonuc_t *s, uint8_t len)
{
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(1u, (uint32_t)s->sayi, a->baslik);
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(1u, s->sayac.rx_frames, a->baslik);
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(0u, kabul_ret_toplami(&s->sayac), a->baslik);
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(0u, s->sayac.rx_resync_bytes, a->baslik);
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(0u, s->sayac.rx_seq_gap, a->baslik);
    const kabul_teslim_t *t = &s->t[0];
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(a->msgid, t->msgid, a->baslik);
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(a->seq, t->seq, a->baslik);
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(len, t->len, a->baslik);
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(a->yuk_azami, t->yuk_azami, a->baslik);
    uint8_t yuk[IDA_LINK_YUK_AZAMI];
    yuk_al(a, yuk);
    TEST_ASSERT_EQUAL_HEX8_ARRAY_MESSAGE(yuk, t->yuk, IDA_LINK_YUK_AZAMI, a->baslik);
}

static void cerceve_doldur(ida_link_cerceve_t *c, const kabul_teslim_t *t)
{
    memset(c, 0, sizeof *c);
    c->msgid = t->msgid;
    c->seq = t->seq;
    c->sysid = t->sysid;
    c->compid = t->compid;
    c->len = t->len;
    c->yuk_azami = t->yuk_azami;
    memcpy(c->yuk, t->yuk, sizeof c->yuk);
}

/* (2), (3), (4). */
static void test_A1_EkB_2_3_4_kabul_cozum_ve_yeniden_kodlama(void)
{
    for (size_t i = 0; i < KABUL_ALTIN_SAYISI; ++i) {
        const kabul_altin_t *a = &kabul_altin[i];
        const kabul_mesaj_t *m = kabul_mesaj_bul(a->msgid);
        /* (2) */
        kabul_besle_yeni(alici_rolu(a), a->bayt, a->n, &sonuc);
        tek_teslim_denetle(a, &sonuc, a->yuk_bu);
        TEST_ASSERT_EQUAL_UINT8_MESSAGE(a->sysid, sonuc.t[0].sysid, a->baslik);
        TEST_ASSERT_EQUAL_UINT8_MESSAGE(a->compid, sonuc.t[0].compid, a->baslik);
        TEST_ASSERT_EQUAL_UINT32_MESSAGE(a->n, sonuc.sayac.rx_bytes, a->baslik);
        /* (3) */
        ida_link_cerceve_t c;
        cerceve_doldur(&c, &sonuc.t[0]);
        kabul_yapi_t y;
        memset(y.b, 0x5C, sizeof y.b);
        TEST_ASSERT_EQUAL_INT_MESSAGE(IDA_LINK_TAMAM, ida_link_coz(&c, a->msgid, y.b, m->yapi_boyu), a->baslik);
        a->yapi_denetle(y.b);
        uint8_t yuk[IDA_LINK_YUK_AZAMI];
        yuk_al(a, yuk);
        TEST_ASSERT_EQUAL_HEX8_ARRAY_MESSAGE(yuk, y.b, m->yuk_azami, a->baslik);
        /* (4a) çözülen yapıdan yeniden kodlama. */
        ida_link_gonderici_t g;
        TEST_ASSERT_EQUAL_INT(IDA_LINK_TAMAM, ida_link_gonderici_kur(&g, gonderen_rolu(a)));
        g.seq = a->seq;
        uint8_t cikis[IDA_LINK_CERCEVE_AZAMI];
        size_t yazilan = 0;
        TEST_ASSERT_EQUAL_INT_MESSAGE(IDA_LINK_TAMAM,
            ida_link_gonder(&g, a->msgid, y.b, m->yapi_boyu, cikis, sizeof cikis, &yazilan), a->baslik);
        TEST_ASSERT_EQUAL_UINT32_MESSAGE(a->n, (uint32_t)yazilan, a->baslik);
        TEST_ASSERT_EQUAL_HEX8_ARRAY_MESSAGE(a->bayt, cikis, a->n, a->baslik);
        /* (4b) referansın kurduğu yapıdan (yük = yapının ilk yuk_azami baytı; dolgu 0xA5). */
        kabul_yapi_t y2;
        kabul_yapi_hazirla(&y2, m, yuk);
        g.seq = a->seq;
        memset(cikis, 0, sizeof cikis);
        TEST_ASSERT_EQUAL_INT_MESSAGE(IDA_LINK_TAMAM,
            ida_link_gonder(&g, a->msgid, y2.b, m->yapi_boyu, cikis, sizeof cikis, &yazilan), a->baslik);
        TEST_ASSERT_EQUAL_UINT32_MESSAGE(a->n, (uint32_t)yazilan, a->baslik);
        TEST_ASSERT_EQUAL_HEX8_ARRAY_MESSAGE(a->bayt, cikis, a->n, a->baslik);
    }
}

/* (5) */
static void test_A1_EkB_5_kirpilmamis_bicim_ve_iki_yon(void)
{
    for (size_t i = 0; i < KABUL_ALTIN_SAYISI; ++i) {
        const kabul_altin_t *a = &kabul_altin[i];
        const kabul_mesaj_t *m = kabul_mesaj_bul(a->msgid);
        uint8_t yuk[IDA_LINK_YUK_AZAMI];
        yuk_al(a, yuk);
        size_t n = kabul_cerceve_kur(akis, a->msgid, a->seq, a->sysid, a->compid, yuk, sizeof yuk, 0);
        TEST_ASSERT_EQUAL_UINT32_MESSAGE(12u + a->yuk_azami, (uint32_t)n, a->baslik);
        kabul_besle_yeni(alici_rolu(a), akis, n, &sonuc);
        tek_teslim_denetle(a, &sonuc, a->yuk_azami);
        kabul_besle_yeni(alici_rolu(a), a->bayt, a->n, &sonuc2);
        TEST_ASSERT_EQUAL_HEX8_ARRAY_MESSAGE(sonuc2.t[0].yuk, sonuc.t[0].yuk, IDA_LINK_YUK_AZAMI, a->baslik);
        /* Aynı yapı. */
        ida_link_cerceve_t c1;
        ida_link_cerceve_t c2;
        cerceve_doldur(&c1, &sonuc.t[0]);
        cerceve_doldur(&c2, &sonuc2.t[0]);
        kabul_yapi_t y1;
        kabul_yapi_t y2;
        memset(y1.b, 0x11, sizeof y1.b);
        memset(y2.b, 0x22, sizeof y2.b);
        TEST_ASSERT_EQUAL_INT(IDA_LINK_TAMAM, ida_link_coz(&c1, a->msgid, y1.b, m->yapi_boyu));
        TEST_ASSERT_EQUAL_INT(IDA_LINK_TAMAM, ida_link_coz(&c2, a->msgid, y2.b, m->yapi_boyu));
        TEST_ASSERT_EQUAL_HEX8_ARRAY_MESSAGE(y2.b, y1.b, m->yapi_boyu, a->baslik);
        a->yapi_denetle(y1.b);
        /* İki yönlü mesaj: diğer rol, kendi karşı tarafının kimliğiyle kurulan aynı yükü kabul eder. */
        if (m->yon == (KABUL_YON_SIM_KART | KABUL_YON_KART_SIM)) {
            ida_link_rol_t diger = alici_rolu(a) == IDA_LINK_ROL_KART ? IDA_LINK_ROL_SIM : IDA_LINK_ROL_KART;
            n = kabul_cerceve_kur(akis, a->msgid, a->seq, kabul_karsi_sysid(diger), kabul_karsi_compid(diger),
                                  yuk, sizeof yuk, 1);
            kabul_besle_yeni(diger, akis, n, &sonuc);
            tek_teslim_denetle(a, &sonuc, a->yuk_bu);
        }
    }
}

/* (6): her bölme noktası ve tek bayt besleme aynı teslimi ve sayaçları verir. */
static void test_A1_EkB_6_parcali_besleme(void)
{
    static ida_link_alici_t al;
    for (size_t i = 0; i < KABUL_ALTIN_SAYISI; ++i) {
        const kabul_altin_t *a = &kabul_altin[i];
        kabul_besle_yeni(alici_rolu(a), a->bayt, a->n, &sonuc);
        for (size_t k = 0; k <= a->n; ++k) {
            kabul_parcalayici_t p = {KABUL_PARCA_IKI, k, {0u}};
            TEST_ASSERT_EQUAL_INT(IDA_LINK_TAMAM, ida_link_alici_kur(&al, alici_rolu(a)));
            kabul_sonuc_sifirla(&sonuc2);
            kabul_besle(&al, a->bayt, a->n, &p, &sonuc2);
            kabul_sonuc_esit(&sonuc, &sonuc2, a->baslik);
        }
        kabul_parcalayici_t p1 = {KABUL_PARCA_SABIT, 1u, {0u}};
        TEST_ASSERT_EQUAL_INT(IDA_LINK_TAMAM, ida_link_alici_kur(&al, alici_rolu(a)));
        kabul_sonuc_sifirla(&sonuc2);
        kabul_besle(&al, a->bayt, a->n, &p1, &sonuc2);
        kabul_sonuc_esit(&sonuc, &sonuc2, a->baslik);
    }
}

/* Aynı role giden bütün altın çerçeveler art arda tek akışta: hepsi sırasıyla teslim edilir,
   ret ve rx_resync_bytes yoktur. */
static void test_A1_EkB_7_art_arda_akis(void)
{
    static const ida_link_rol_t ROLLER[2] = {IDA_LINK_ROL_KART, IDA_LINK_ROL_SIM};
    for (size_t ri = 0; ri < 2; ++ri) {
        size_t n = 0;
        size_t beklenen = 0;
        for (size_t i = 0; i < KABUL_ALTIN_SAYISI; ++i) {
            if (alici_rolu(&kabul_altin[i]) == ROLLER[ri]) {
                memcpy(&akis[n], kabul_altin[i].bayt, kabul_altin[i].n);
                n += kabul_altin[i].n;
                ++beklenen;
            }
        }
        kabul_besle_yeni(ROLLER[ri], akis, n, &sonuc);
        TEST_ASSERT_EQUAL_UINT32((uint32_t)beklenen, (uint32_t)sonuc.sayi);
        TEST_ASSERT_EQUAL_UINT32(0u, kabul_ret_toplami(&sonuc.sayac));
        TEST_ASSERT_EQUAL_UINT32(0u, sonuc.sayac.rx_resync_bytes);
        TEST_ASSERT_EQUAL_UINT32((uint32_t)n, sonuc.sayac.rx_bytes);
        size_t k = 0;
        for (size_t i = 0; i < KABUL_ALTIN_SAYISI; ++i) {
            if (alici_rolu(&kabul_altin[i]) == ROLLER[ri]) {
                TEST_ASSERT_EQUAL_UINT32_MESSAGE(kabul_altin[i].msgid, sonuc.t[k].msgid, kabul_altin[i].baslik);
                TEST_ASSERT_EQUAL_UINT8_MESSAGE(kabul_altin[i].seq, sonuc.t[k].seq, kabul_altin[i].baslik);
                ++k;
            }
        }
    }
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_A1_EkB_1_sozlesme_ic_tutarlilik);
    RUN_TEST(test_A1_EkB_2_3_4_kabul_cozum_ve_yeniden_kodlama);
    RUN_TEST(test_A1_EkB_5_kirpilmamis_bicim_ve_iki_yon);
    RUN_TEST(test_A1_EkB_6_parcali_besleme);
    RUN_TEST(test_A1_EkB_7_art_arda_akis);
    return UNITY_END();
}
