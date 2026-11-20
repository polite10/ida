/*
 * A1-K06 kabul testleri (sözleşme §4.1, Tablo 3.3 "Yön"):
 *   "Alıcı, dialect'te olmayan ya da kendi yönünde alınmaması gereken (Tablo 3.3 "Yön"
 *    sütunu) mesaj kimliğini taşıyan çerçeveyi atar (rx_unknown_msgid)."
 * Öğeler:
 *   (a) dialect'te olmayan kimlik atılır; ardından gelen geçerli çerçeve kabul edilir
 *       (sayaç: K06 rx_unknown_msgid der, Tablo 3.11b rx_crc_err için "bilinmeyen kimlik
 *       dahil" der; hangisinin arttığı kabul_belirsiz.c'de gözlenir, burada yalnız atılma ve
 *       toplam denetlenir)
 *   (b) yönü yanlış kimlik atılır, rx_unknown_msgid (Tablo 3.11b de aynı sayacı verir)
 *   (c) Tablo 3.3'ün her satırı, iki rol: yalnız rolün yönündeki ve iki yönlü mesajlar kabul
 *   (d) iki yönlü mesajlar (46003, 46004, 46036) iki rolde de kabul
 *   (e) 24 bitlik kimliğin üst baytı da denetlenir (46000 + 2^16 dialect'te değildir)
 */
#include <stdio.h>
#include <string.h>

#include "kabul_ortak.h"
#include "unity.h"

void setUp(void) {}
void tearDown(void) {}

static uint8_t akis[16384];
static kabul_sonuc_t sonuc;

static const ida_link_rol_t ROLLER[2] = {IDA_LINK_ROL_KART, IDA_LINK_ROL_SIM};

static void bilinmeyen(uint32_t id, uint8_t len, uint8_t crc_extra, kabul_rng_t *r)
{
    char ileti[96];
    snprintf(ileti, sizeof ileti, "bilinmeyen msgid %lu", (unsigned long)id);
    for (size_t ri = 0; ri < 2; ++ri) {
        ida_link_rol_t rol = ROLLER[ri];
        uint32_t gecerli = rol == IDA_LINK_ROL_KART ? 46010u : 46064u;
        size_t n = kabul_temiz_ham(akis, r, len, 0u, 0u, 50u, kabul_karsi_sysid(rol),
                                   kabul_karsi_compid(rol), id, crc_extra, 0u);
        n += kabul_temiz_kur(&akis[n], r, gecerli, 51u, kabul_karsi_sysid(rol), kabul_karsi_compid(rol),
                             4u, 1, NULL);
        kabul_besle_yeni(rol, akis, n, &sonuc);
        TEST_ASSERT_EQUAL_UINT32_MESSAGE(1u, (uint32_t)sonuc.sayi, ileti);
        TEST_ASSERT_EQUAL_UINT32_MESSAGE(gecerli, sonuc.t[0].msgid, ileti);
        TEST_ASSERT_EQUAL_UINT8_MESSAGE(51u, sonuc.t[0].seq, ileti);
        TEST_ASSERT_EQUAL_UINT32_MESSAGE(1u, sonuc.sayac.rx_frames, ileti);
        /* Atılma bir ret sayacına girer: rx_unknown_msgid (K06) ya da rx_crc_err (Tablo 3.11b);
           başka bir ret sayacı artmaz. */
        TEST_ASSERT_TRUE_MESSAGE(sonuc.sayac.rx_unknown_msgid + sonuc.sayac.rx_crc_err >= 1u, ileti);
        TEST_ASSERT_EQUAL_UINT32_MESSAGE(0u, sonuc.sayac.rx_iflag + sonuc.sayac.rx_bad_len +
                                             sonuc.sayac.rx_wrong_sys + sonuc.sayac.rx_mav1, ileti);
    }
}

static void test_A1_K06_a_dialect_disi_kimlik_atilir(void)
{
    static const uint32_t ID[] = {
        0u, 1u, 30u, 255u, 256u, 45999u, 46005u, 46009u, 46011u, 46019u, 46031u, 46034u, 46037u,
        46039u, 46047u, 46059u, 46071u, 46099u, 46100u, 0x00FFFFu, 0xFFFFFFu,
    };
    kabul_rng_t r;
    kabul_rng_kur(&r, 0x4B30360001ull);
    for (size_t i = 0; i < sizeof ID / sizeof ID[0]; ++i) {
        bilinmeyen(ID[i], (uint8_t)(1u + (i * 13u) % 60u), (uint8_t)(i * 37u), &r);
    }
}

/* (e): 46000 + 0x10000 (bayt 9 = 1) ve 46060 + 0x20000: alt iki bayt dialect'teki bir mesajla
   aynı, üst bayt değil. Sağlama, alt iki baytın mesajının CRC_EXTRA'sıyla hesaplanır: kimliğin
   üst baytı göz ardı edilirse çerçeve geçerli görünür. */
static void test_A1_K06_e_kimligin_ust_bayti(void)
{
    kabul_rng_t r;
    kabul_rng_kur(&r, 0x4B30360002ull);
    bilinmeyen(46000u + 0x10000u, 22u, kabul_mesaj_bul(46000u)->crc_extra, &r);
    bilinmeyen(46020u + 0x10000u, 47u, kabul_mesaj_bul(46020u)->crc_extra, &r);
    bilinmeyen(46060u + 0x20000u, 38u, kabul_mesaj_bul(46060u)->crc_extra, &r);
    bilinmeyen(46063u + 0x800000u, 118u, kabul_mesaj_bul(46063u)->crc_extra, &r);
}

/* (b): yönü yanlış mesaj (sağlaması, kimliği ve len'i doğru) atılır, rx_unknown_msgid tam bir
   artar; ardından gelen geçerli çerçeve kabul edilir. */
static void test_A1_K06_b_yonu_yanlis_kimlik_atilir(void)
{
    kabul_rng_t r;
    kabul_rng_kur(&r, 0x4B30360003ull);
    for (size_t ri = 0; ri < 2; ++ri) {
        ida_link_rol_t rol = ROLLER[ri];
        uint32_t gecerli = rol == IDA_LINK_ROL_KART ? 46020u : 46060u;
        size_t sayi = 0;
        for (size_t i = 0; i < KABUL_MESAJ_SAYISI; ++i) {
            const kabul_mesaj_t *m = &kabul_mesajlar[i];
            if (m->yon & kabul_rol_alir_yon(rol)) {
                continue;
            }
            size_t n = kabul_temiz_kur(akis, &r, m->msgid, 60u, kabul_karsi_sysid(rol),
                                       kabul_karsi_compid(rol), m->yuk_azami, 1, NULL);
            n += kabul_temiz_kur(&akis[n], &r, gecerli, 61u, kabul_karsi_sysid(rol),
                                 kabul_karsi_compid(rol), 7u, 1, NULL);
            kabul_besle_yeni(rol, akis, n, &sonuc);
            TEST_ASSERT_EQUAL_UINT32_MESSAGE(1u, sonuc.sayac.rx_unknown_msgid, m->ad);
            TEST_ASSERT_EQUAL_UINT32_MESSAGE(1u, kabul_ret_toplami(&sonuc.sayac), m->ad);
            TEST_ASSERT_EQUAL_UINT32_MESSAGE(1u, (uint32_t)sonuc.sayi, m->ad);
            TEST_ASSERT_EQUAL_UINT32_MESSAGE(gecerli, sonuc.t[0].msgid, m->ad);
            ++sayi;
        }
        /* Tablo 3.3: 22 mesaj yalnız sim -> kart, 12 mesaj yalnız kart -> sim, 3 mesaj iki yönlü;
           kart 12, sim 22 mesajı almaz. */
        TEST_ASSERT_EQUAL_UINT32(rol == IDA_LINK_ROL_KART ? 12u : 22u, (uint32_t)sayi);
    }
}

/* (c), (d): Tablo 3.3'ün 37 mesajı tek akışta, karşı tarafın kimliğiyle; rolün yönündeki
   ve iki yönlü mesajlar teslim edilir, diğerleri rx_unknown_msgid'e sayılır. */
static void test_A1_K06_cd_tablo_3_3_her_satir(void)
{
    kabul_rng_t r;
    kabul_rng_kur(&r, 0x4B30360004ull);
    for (size_t ri = 0; ri < 2; ++ri) {
        ida_link_rol_t rol = ROLLER[ri];
        size_t n = 0;
        uint32_t kabul = 0;
        uint32_t ret = 0;
        uint32_t iki_yon = 0;
        for (size_t i = 0; i < KABUL_MESAJ_SAYISI; ++i) {
            const kabul_mesaj_t *m = &kabul_mesajlar[i];
            n += kabul_temiz_kur(&akis[n], &r, m->msgid, (uint8_t)i, kabul_karsi_sysid(rol),
                                 kabul_karsi_compid(rol), m->yuk_azami / 3u + 1u, 1, NULL);
            if (m->yon & kabul_rol_alir_yon(rol)) {
                ++kabul;
            } else {
                ++ret;
            }
            if (m->yon == (KABUL_YON_SIM_KART | KABUL_YON_KART_SIM)) {
                ++iki_yon;
            }
        }
        TEST_ASSERT_EQUAL_UINT32(3u, iki_yon);
        kabul_besle_yeni(rol, akis, n, &sonuc);
        TEST_ASSERT_EQUAL_UINT32(kabul, (uint32_t)sonuc.sayi);
        TEST_ASSERT_EQUAL_UINT32(ret, sonuc.sayac.rx_unknown_msgid);
        TEST_ASSERT_EQUAL_UINT32(ret, kabul_ret_toplami(&sonuc.sayac));
        size_t k = 0;
        for (size_t i = 0; i < KABUL_MESAJ_SAYISI; ++i) {
            const kabul_mesaj_t *m = &kabul_mesajlar[i];
            if (m->yon & kabul_rol_alir_yon(rol)) {
                TEST_ASSERT_EQUAL_UINT32_MESSAGE(m->msgid, sonuc.t[k].msgid, m->ad);
                ++k;
            }
        }
    }
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_A1_K06_a_dialect_disi_kimlik_atilir);
    RUN_TEST(test_A1_K06_e_kimligin_ust_bayti);
    RUN_TEST(test_A1_K06_b_yonu_yanlis_kimlik_atilir);
    RUN_TEST(test_A1_K06_cd_tablo_3_3_her_satir);
    return UNITY_END();
}
