/*
 * A1-K02 kabul testleri (sözleşme §4.1):
 *   "Her iki taraf yalnız magic = 0xFD, incompat_flags = 0 ve compat_flags = 0 olan MAVLink 2
 *    çerçevesi gönderir; alıcı incompat_flags ≠ 0 olan çerçeveyi atar (rx_iflag),
 *    compat_flags'i yok sayar ve 0xFE ile başlayan MAVLink 1 çerçevesini atar (rx_mav1)."
 * Öğeler:
 *   (a) gönderen: magic 0xFD           -> test_A1_K02_a_gonderen_*
 *   (b) gönderen: incompat = compat = 0 -> test_A1_K02_a_gonderen_*
 *   (c) alıcı: incompat ≠ 0 atılır, rx_iflag; ardından gelen geçerli çerçeve kabul
 *   (d) alıcı: compat_flags yok sayılır (her değerde kabul, yük aynı)
 *   (e) alıcı: MAVLink 1 çerçevesi atılır, rx_mav1; ardından gelen geçerli çerçeve kabul
 * Başlıkta reddedilen adaydan sonra taramanın nereden sürdüğü ve MAVLink 1 baytlarının
 * sayımı A1'de açık değildir; burada yalnız iki okumada da aynı olan sonuçlar denetlenir
 * (temiz çerçeveler); o iki konu kabul_belirsiz.c'dedir.
 */
#include <stdio.h>
#include <string.h>

#include "kabul_ortak.h"
#include "unity.h"

void setUp(void) {}
void tearDown(void) {}

static uint8_t akis[8192];
static kabul_sonuc_t sonuc;

static const ida_link_rol_t ROLLER[2] = {IDA_LINK_ROL_KART, IDA_LINK_ROL_SIM};

/* Rolün alabileceği ilk mesaj (temiz geçerli çerçeve kurmak için). */
static uint32_t alinan_ornek(ida_link_rol_t rol, size_t sira)
{
    size_t k = 0;
    for (size_t i = 0; i < KABUL_MESAJ_SAYISI; ++i) {
        if (kabul_mesajlar[i].yon & kabul_rol_alir_yon(rol)) {
            if (k++ == sira) {
                return kabul_mesajlar[i].msgid;
            }
        }
    }
    return kabul_mesajlar[0].msgid;
}

/* (a), (b): her rol, gönderebildiği her mesaj için, rastgele içerikle: bayt 0 = 0xFD,
   bayt 2 = bayt 3 = 0 ve çerçeve referans doğrulayıcıya göre geçerli. */
static void test_A1_K02_a_gonderen_magic_ve_bayraklar_sifir(void)
{
    kabul_rng_t r;
    kabul_rng_kur(&r, 0x4B30320001ull);
    for (size_t ri = 0; ri < 2; ++ri) {
        ida_link_gonderici_t g;
        TEST_ASSERT_EQUAL_INT(IDA_LINK_TAMAM, ida_link_gonderici_kur(&g, ROLLER[ri]));
        size_t sayi = 0;
        for (size_t i = 0; i < KABUL_MESAJ_SAYISI; ++i) {
            const kabul_mesaj_t *m = &kabul_mesajlar[i];
            if ((m->yon & kabul_rol_gonderir_yon(ROLLER[ri])) == 0u) {
                continue;
            }
            for (int tekrar = 0; tekrar < 8; ++tekrar) {
                uint8_t yuk[IDA_LINK_YUK_AZAMI] = {0};
                size_t dolu = kabul_rng_aralik(&r, (uint32_t)m->yuk_azami + 1u);
                for (size_t k = 0; k < dolu; ++k) {
                    yuk[k] = kabul_rng_bayt(&r);
                }
                kabul_yapi_t y;
                kabul_yapi_hazirla(&y, m, yuk);
                uint8_t cikis[IDA_LINK_CERCEVE_AZAMI];
                size_t yazilan = 0;
                TEST_ASSERT_EQUAL_INT_MESSAGE(IDA_LINK_TAMAM,
                    ida_link_gonder(&g, m->msgid, y.b, m->yapi_boyu, cikis, sizeof cikis, &yazilan),
                    m->ad);
                TEST_ASSERT_EQUAL_HEX8_MESSAGE(0xFDu, cikis[0], m->ad);
                TEST_ASSERT_EQUAL_HEX8_MESSAGE(0x00u, cikis[2], m->ad);
                TEST_ASSERT_EQUAL_HEX8_MESSAGE(0x00u, cikis[3], m->ad);
                kabul_cozulen_t c;
                TEST_ASSERT_EQUAL_INT_MESSAGE(0, kabul_cerceve_dogrula(cikis, yazilan, &c), m->ad);
                ++sayi;
            }
        }
        TEST_ASSERT_TRUE(sayi > 0u);
    }
}

/* (c): incompat_flags ≠ 0 (tek bit, yüksek bit, bütün bitler) taşıyan, sağlaması doğru
   çerçeve atılır; rx_iflag tam bir artar; ardından gelen geçerli çerçeve kabul edilir. */
static void test_A1_K02_c_incompat_atilir_rx_iflag(void)
{
    static const uint8_t BAYRAK[] = {0x01u, 0x02u, 0x04u, 0x80u, 0x7Fu, 0xFFu};
    kabul_rng_t r;
    kabul_rng_kur(&r, 0x4B30320002ull);
    for (size_t ri = 0; ri < 2; ++ri) {
        ida_link_rol_t rol = ROLLER[ri];
        for (size_t bi = 0; bi < sizeof BAYRAK; ++bi) {
            uint32_t id = alinan_ornek(rol, bi);
            const kabul_mesaj_t *m = kabul_mesaj_bul(id);
            size_t n = kabul_temiz_ham(akis, &r, m->yuk_azami, BAYRAK[bi], 0u, 10u,
                                       kabul_karsi_sysid(rol), kabul_karsi_compid(rol), id,
                                       m->crc_extra, 0u);
            n += kabul_temiz_kur(&akis[n], &r, id, 11u, kabul_karsi_sysid(rol),
                                 kabul_karsi_compid(rol), 5u, 1, NULL);
            kabul_besle_yeni(rol, akis, n, &sonuc);
            TEST_ASSERT_EQUAL_UINT32_MESSAGE(1u, sonuc.sayac.rx_iflag, "rx_iflag");
            TEST_ASSERT_EQUAL_UINT32_MESSAGE(1u, (uint32_t)sonuc.sayi, "teslim sayısı");
            TEST_ASSERT_EQUAL_UINT8_MESSAGE(11u, sonuc.t[0].seq, "teslim edilen geçerli çerçeve değil");
            TEST_ASSERT_EQUAL_UINT32(1u, sonuc.sayac.rx_frames);
            TEST_ASSERT_EQUAL_UINT32_MESSAGE(1u, kabul_ret_toplami(&sonuc.sayac),
                                             "rx_iflag dışında ret sayacı arttı");
            TEST_ASSERT_EQUAL_UINT32((uint32_t)n, sonuc.sayac.rx_bytes);
        }
    }
}

/* (c): imzalı MAVLink 2 çerçevesi (incompat_flags bit 0, 13 baytlık imza bloğuyla; A1 §3.2:
   imza kullanılmaz) atılır, rx_iflag; ardından gelen geçerli çerçeve kabul edilir. */
static void test_A1_K02_c_imzali_cerceve_atilir(void)
{
    kabul_rng_t r;
    kabul_rng_kur(&r, 0x4B30320003ull);
    const ida_link_rol_t rol = IDA_LINK_ROL_KART;
    const kabul_mesaj_t *m = kabul_mesaj_bul(46003u);
    size_t n = kabul_temiz_ham(akis, &r, 3u, 0x01u, 0u, 1u, 250u, 25u, 46003u, m->crc_extra, 0u);
    for (int i = 0; i < 13; ++i) {
        akis[n++] = kabul_rng_temiz_bayt(&r);
    }
    n += kabul_temiz_kur(&akis[n], &r, 46010u, 2u, 250u, 25u, 17u, 1, NULL);
    kabul_besle_yeni(rol, akis, n, &sonuc);
    TEST_ASSERT_EQUAL_UINT32(1u, sonuc.sayac.rx_iflag);
    TEST_ASSERT_EQUAL_UINT32(1u, (uint32_t)sonuc.sayi);
    TEST_ASSERT_EQUAL_UINT32(46010u, sonuc.t[0].msgid);
    TEST_ASSERT_EQUAL_UINT32(1u, kabul_ret_toplami(&sonuc.sayac));
}

/* (d): compat_flags'in her değeri yok sayılır: çerçeve kabul edilir, teslim edilen yük
   compat_flags = 0'lı çerçeveninkiyle aynıdır, hiçbir ret sayacı artmaz. */
static void test_A1_K02_d_compat_yok_sayilir(void)
{
    static const uint8_t BAYRAK[] = {0x01u, 0x02u, 0x40u, 0x80u, 0xAAu, 0xFFu};
    kabul_rng_t r;
    kabul_rng_kur(&r, 0x4B30320004ull);
    static kabul_sonuc_t referans;
    for (size_t ri = 0; ri < 2; ++ri) {
        ida_link_rol_t rol = ROLLER[ri];
        for (size_t bi = 0; bi < sizeof BAYRAK; ++bi) {
            uint32_t id = alinan_ornek(rol, bi + 2u);
            const kabul_mesaj_t *m = kabul_mesaj_bul(id);
            uint8_t yuk[IDA_LINK_YUK_AZAMI] = {0};
            for (size_t k = 0; k < m->yuk_azami; ++k) {
                yuk[k] = kabul_rng_temiz_bayt(&r);
            }
            size_t n0 = kabul_cerceve_ham(akis, m->yuk_azami, 0u, 0u, 7u, kabul_karsi_sysid(rol),
                                          kabul_karsi_compid(rol), id, yuk, m->yuk_azami, m->crc_extra);
            kabul_besle_yeni(rol, akis, n0, &referans);
            TEST_ASSERT_EQUAL_UINT32(1u, (uint32_t)referans.sayi);
            size_t n = kabul_cerceve_ham(akis, m->yuk_azami, 0u, BAYRAK[bi], 7u,
                                         kabul_karsi_sysid(rol), kabul_karsi_compid(rol), id, yuk,
                                         m->yuk_azami, m->crc_extra);
            kabul_besle_yeni(rol, akis, n, &sonuc);
            TEST_ASSERT_EQUAL_UINT32_MESSAGE(1u, (uint32_t)sonuc.sayi, "compat_flags'li çerçeve kabul edilmedi");
            TEST_ASSERT_EQUAL_UINT32(0u, kabul_ret_toplami(&sonuc.sayac));
            TEST_ASSERT_EQUAL_UINT32(1u, sonuc.sayac.rx_frames);
            TEST_ASSERT_EQUAL_UINT32(id, sonuc.t[0].msgid);
            TEST_ASSERT_EQUAL_UINT8_ARRAY(referans.t[0].yuk, sonuc.t[0].yuk, IDA_LINK_YUK_AZAMI);
        }
    }
}

/* MAVLink 1 çerçevesi: FE len seq sysid compid msgid yük crc (CRC, magic hariç başlık + yük +
   CRC_EXTRA). Baytları temizdir (FE dışında 0xFD/0xFE yok). */
static size_t mav1_kur(uint8_t *c, kabul_rng_t *r, uint8_t len, uint8_t msgid, uint8_t crc_extra,
                       uint8_t sysid, uint8_t compid)
{
    int temiz = 0;
    for (int deneme = 0; deneme < 4096 && !temiz; ++deneme) {
        c[0] = 0xFEu;
        c[1] = len;
        c[2] = 3u;
        c[3] = sysid;
        c[4] = compid;
        c[5] = msgid;
        for (size_t i = 0; i < len; ++i) {
            c[6u + i] = kabul_rng_temiz_bayt(r);
        }
        uint16_t crc = kabul_crc(&c[1], 5u + (size_t)len, 0xFFFFu);
        crc = kabul_crc_bayt(crc, crc_extra);
        c[6u + len] = (uint8_t)(crc & 0xFFu);
        c[7u + len] = (uint8_t)(crc >> 8);
        temiz = 1;
        for (size_t i = 1; i < 8u + (size_t)len; ++i) {
            if (c[i] == 0xFDu || c[i] == 0xFEu) {
                temiz = 0;
            }
        }
    }
    TEST_ASSERT_TRUE_MESSAGE(temiz, "MAVLink 1 çerçevesi kurulamadı");
    return 8u + (size_t)len;
}

/* (e): 0xFE ile başlayan MAVLink 1 çerçeveleri (HEARTBEAT 0/9 B, ATTITUDE 30/28 B; karşı
   tarafın kimliğiyle ve karşı tarafın olmayan kimlikle) atılır; tek 0xFE taşıyan her MAVLink 1
   çerçevesi rx_mav1'i artırır; hiçbiri teslim edilmez; ardından gelen geçerli çerçeve kabul
   edilir. Baytlar temiz olduğundan MAVLink 1 başına bayt ya da çerçeve sayımı aynı sonucu verir. */
static void test_A1_K02_e_mavlink1_atilir_rx_mav1(void)
{
    kabul_rng_t r;
    kabul_rng_kur(&r, 0x4B30320005ull);
    for (size_t ri = 0; ri < 2; ++ri) {
        ida_link_rol_t rol = ROLLER[ri];
        size_t n = 0;
        n += mav1_kur(&akis[n], &r, 9u, 0u, 50u, kabul_karsi_sysid(rol), kabul_karsi_compid(rol));
        n += kabul_temiz_kur(&akis[n], &r, alinan_ornek(rol, 0), 1u, kabul_karsi_sysid(rol),
                             kabul_karsi_compid(rol), 3u, 1, NULL);
        n += mav1_kur(&akis[n], &r, 28u, 30u, 39u, 1u, 1u);
        n += mav1_kur(&akis[n], &r, 9u, 0u, 50u, 250u, 25u);
        n += kabul_temiz_kur(&akis[n], &r, alinan_ornek(rol, 1), 2u, kabul_karsi_sysid(rol),
                             kabul_karsi_compid(rol), 3u, 1, NULL);
        kabul_besle_yeni(rol, akis, n, &sonuc);
        TEST_ASSERT_EQUAL_UINT32_MESSAGE(3u, sonuc.sayac.rx_mav1, "rx_mav1");
        TEST_ASSERT_EQUAL_UINT32_MESSAGE(2u, (uint32_t)sonuc.sayi, "teslim sayısı");
        TEST_ASSERT_EQUAL_UINT8(1u, sonuc.t[0].seq);
        TEST_ASSERT_EQUAL_UINT8(2u, sonuc.t[1].seq);
        TEST_ASSERT_EQUAL_UINT32(3u, kabul_ret_toplami(&sonuc.sayac));
        TEST_ASSERT_EQUAL_UINT32((uint32_t)n, sonuc.sayac.rx_bytes);
    }
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_A1_K02_a_gonderen_magic_ve_bayraklar_sifir);
    RUN_TEST(test_A1_K02_c_incompat_atilir_rx_iflag);
    RUN_TEST(test_A1_K02_c_imzali_cerceve_atilir);
    RUN_TEST(test_A1_K02_d_compat_yok_sayilir);
    RUN_TEST(test_A1_K02_e_mavlink1_atilir_rx_mav1);
    return UNITY_END();
}
