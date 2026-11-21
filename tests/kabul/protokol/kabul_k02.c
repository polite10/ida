/*
 * A1-K02 kabul testleri (sözleşme §4.1, doğrulama turu metni):
 *   "Her iki taraf yalnız magic = 0xFD, incompat_flags = 0 ve compat_flags = 0 olan MAVLink 2
 *    çerçevesi gönderir; alıcı incompat_flags ≠ 0 olan adayı atar (rx_iflag) ve
 *    compat_flags'i yok sayar. MAVLink 1 çerçevesi doğrulanamaz ve bütün olarak atlanmaz:
 *    aday dışındayken görülen her 0xFE baytı rx_mav1'i bir artırır (ve aday başlatmadığı için
 *    rx_resync_bytes'a da sayılır); ardından gelen baytlar olağan 0xFD taramasına girer
 *    (len'e göre atlama yapılmaz)."
 * Öğeler:
 *   (a) gönderen: magic 0xFD; (b) gönderen: incompat = compat = 0
 *   (c) incompat ≠ 0 olan aday atılır, yalnız rx_iflag; tarama 0xFD'den sonra sürer (K07)
 *   (d) compat_flags yok sayılır
 *   (e) aday dışındaki her 0xFE: rx_mav1 +1 ve rx_resync_bytes +1
 *   (f) MAVLink 1 çerçevesi len'e göre atlanmaz: içindeki geçerli MAVLink 2 çerçevesi teslim
 *       edilir
 *   (g) aday içindeki 0xFE sayılmaz (geçerli çerçevenin yükünde, sağlaması bozuk adayda)
 * Her vakada teslimler ve bütün sayaçlar elle yazılmış beklentiyle ve referans modelle
 * (kabul_model_ayristir) tam eşitlikle karşılaştırılır.
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

/* Rolün alabileceği `sira`. mesaj (temiz geçerli çerçeve kurmak için). */
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

/* Besle; teslim sayısı ve bütün sayaçlar beklenenle, ayrıca modelle tam eşit. */
static void besle_denetle(ida_link_rol_t rol, size_t n, size_t teslim, const ida_link_sayaclar_t *b,
                          const char *baglam)
{
    kabul_besle_yeni(rol, akis, n, &sonuc);
    TEST_ASSERT_EQUAL_UINT32_MESSAGE((uint32_t)teslim, (uint32_t)sonuc.sayi, baglam);
    kabul_sayac_esit(b, &sonuc.sayac, baglam);
    kabul_modelle_esit(rol, akis, n, &sonuc, baglam);
}

/* (a), (b) */
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
                    ida_link_gonder(&g, m->msgid, &y, m->yapi_boyu, cikis, sizeof cikis, &yazilan),
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

/* (c): incompat_flags ≠ 0 (tek bit, yüksek bit, bütün bitler), sağlaması doğru, temiz aday;
   ardından geçerli çerçeve. Beklenen: rx_iflag = 1; adayın 0xFD dışındaki baytları yeniden
   taranır ve hiçbiri aday başlatmadığı için rx_resync_bytes = aday boyu − 1; sonraki çerçeve
   teslim edilir. */
static void test_A1_K02_c_incompat_atilir_rx_iflag(void)
{
    static const uint8_t BAYRAK[] = {0x01u, 0x02u, 0x04u, 0x80u, 0x7Fu, 0xFFu};
    kabul_rng_t r;
    kabul_rng_kur(&r, 0x4B30320002ull);
    for (size_t ri = 0; ri < 2; ++ri) {
        ida_link_rol_t rol = ROLLER[ri];
        for (size_t bi = 0; bi < sizeof BAYRAK; ++bi) {
            uint32_t id = alinan_ornek(rol, bi);
            const kabul_mesaj_t *m = kabul_mesaj_al(id);
            size_t aday = kabul_temiz_ham(akis, &r, m->yuk_azami, BAYRAK[bi], 0u, 10u,
                                          kabul_karsi_sysid(rol), kabul_karsi_compid(rol), id,
                                          m->crc_extra, 0u);
            size_t n = aday + kabul_temiz_kur(&akis[aday], &r, id, 11u, kabul_karsi_sysid(rol),
                                              kabul_karsi_compid(rol), 5u, 1, NULL);
            ida_link_sayaclar_t b = kabul_sayac_sifir();
            b.rx_bytes = (uint32_t)n;
            b.rx_frames = 1u;
            b.rx_iflag = 1u;
            b.rx_resync_bytes = (uint32_t)(aday - 1u);
            besle_denetle(rol, n, 1u, &b, "incompat");
            TEST_ASSERT_EQUAL_UINT8(11u, sonuc.t[0].seq);
        }
    }
}

/* (c): imzalı MAVLink 2 çerçevesi (incompat bit 0 + 13 baytlık imza bloğu; A1 §3.2: imza
   kullanılmaz): rx_iflag = 1; adayın 0xFD dışındaki baytları ve imza baytları resync. */
static void test_A1_K02_c_imzali_cerceve_atilir(void)
{
    kabul_rng_t r;
    kabul_rng_kur(&r, 0x4B30320003ull);
    const kabul_mesaj_t *m = kabul_mesaj_al(46003u);
    size_t n = kabul_temiz_ham(akis, &r, 3u, 0x01u, 0u, 1u, 250u, 25u, 46003u, m->crc_extra, 0u);
    for (int i = 0; i < 13; ++i) {
        akis[n++] = kabul_rng_temiz_bayt(&r);
    }
    size_t on = n;
    n += kabul_temiz_kur(&akis[n], &r, 46010u, 2u, 250u, 25u, 17u, 1, NULL);
    ida_link_sayaclar_t b = kabul_sayac_sifir();
    b.rx_bytes = (uint32_t)n;
    b.rx_frames = 1u;
    b.rx_iflag = 1u;
    b.rx_resync_bytes = (uint32_t)(on - 1u);
    besle_denetle(IDA_LINK_ROL_KART, n, 1u, &b, "imzalı");
    TEST_ASSERT_EQUAL_UINT32(46010u, sonuc.t[0].msgid);
}

/* (d): compat_flags'in her değeri yok sayılır: kabul, yük aynı, ret ve resync yok. */
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
            const kabul_mesaj_t *m = kabul_mesaj_al(id);
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
            ida_link_sayaclar_t b = kabul_sayac_sifir();
            b.rx_bytes = (uint32_t)n;
            b.rx_frames = 1u;
            besle_denetle(rol, n, 1u, &b, "compat");
            TEST_ASSERT_EQUAL_UINT32(id, sonuc.t[0].msgid);
            TEST_ASSERT_EQUAL_UINT8_ARRAY(referans.t[0].yuk, sonuc.t[0].yuk, IDA_LINK_YUK_AZAMI);
        }
    }
}

/* MAVLink 1 çerçevesi: FE len seq sysid compid msgid yük crc. Baytları temizdir (FE dışında
   0xFD/0xFE yok). Dönüş: 8 + len. */
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

/* (e): üç temiz MAVLink 1 çerçevesi (HEARTBEAT 0/9 B, ATTITUDE 30/28 B) iki geçerli çerçeveyle
   karışık: rx_mav1 = 3 (her 0xFE); MAVLink 1 çerçevelerinin bütün baytları (0xFE dahil)
   rx_resync_bytes'a girer; geçerli iki çerçeve teslim edilir. */
static void test_A1_K02_e_mavlink1_baytlari(void)
{
    kabul_rng_t r;
    kabul_rng_kur(&r, 0x4B30320005ull);
    for (size_t ri = 0; ri < 2; ++ri) {
        ida_link_rol_t rol = ROLLER[ri];
        size_t n = 0;
        size_t m1 = 0;
        size_t k;
        k = mav1_kur(&akis[n], &r, 9u, 0u, 50u, kabul_karsi_sysid(rol), kabul_karsi_compid(rol));
        n += k;
        m1 += k;
        n += kabul_temiz_kur(&akis[n], &r, alinan_ornek(rol, 0), 1u, kabul_karsi_sysid(rol),
                             kabul_karsi_compid(rol), 3u, 1, NULL);
        k = mav1_kur(&akis[n], &r, 28u, 30u, 39u, 1u, 1u);
        n += k;
        m1 += k;
        k = mav1_kur(&akis[n], &r, 9u, 0u, 50u, 250u, 25u);
        n += k;
        m1 += k;
        n += kabul_temiz_kur(&akis[n], &r, alinan_ornek(rol, 1), 2u, kabul_karsi_sysid(rol),
                             kabul_karsi_compid(rol), 3u, 1, NULL);
        ida_link_sayaclar_t b = kabul_sayac_sifir();
        b.rx_bytes = (uint32_t)n;
        b.rx_frames = 2u;
        b.rx_mav1 = 3u;
        b.rx_resync_bytes = (uint32_t)m1;
        besle_denetle(rol, n, 2u, &b, "MAVLink 1 çerçeveleri");
        TEST_ASSERT_EQUAL_UINT8(1u, sonuc.t[0].seq);
        TEST_ASSERT_EQUAL_UINT8(2u, sonuc.t[1].seq);
    }
}

/* (e): tek 0xFE baytı (sol incelemesinin ayırıcı girdisi): rx_mav1 = 1, rx_resync_bytes = 1;
   akışın başında, ortasında ve sonunda; iki rol. */
static void test_A1_K02_e_tek_fe_bayti(void)
{
    kabul_rng_t r;
    kabul_rng_kur(&r, 0x4B30320006ull);
    for (size_t ri = 0; ri < 2; ++ri) {
        ida_link_rol_t rol = ROLLER[ri];
        akis[0] = 0xFEu;
        ida_link_sayaclar_t b = kabul_sayac_sifir();
        b.rx_bytes = 1u;
        b.rx_mav1 = 1u;
        b.rx_resync_bytes = 1u;
        besle_denetle(rol, 1u, 0u, &b, "yalnız 0xFE");
        for (int yer = 0; yer < 3; ++yer) {
            size_t n = 0;
            if (yer == 0) {
                akis[n++] = 0xFEu;
            }
            n += kabul_temiz_kur(&akis[n], &r, alinan_ornek(rol, 0), 0u, kabul_karsi_sysid(rol),
                                 kabul_karsi_compid(rol), 3u, 1, NULL);
            if (yer == 1) {
                akis[n++] = 0xFEu;
            }
            n += kabul_temiz_kur(&akis[n], &r, alinan_ornek(rol, 1), 1u, kabul_karsi_sysid(rol),
                                 kabul_karsi_compid(rol), 3u, 1, NULL);
            if (yer == 2) {
                akis[n++] = 0xFEu;
            }
            b = kabul_sayac_sifir();
            b.rx_bytes = (uint32_t)n;
            b.rx_frames = 2u;
            b.rx_mav1 = 1u;
            b.rx_resync_bytes = 1u;
            besle_denetle(rol, n, 2u, &b, "tek 0xFE");
        }
    }
}

/* (e): yükünde ikinci bir 0xFE bulunan MAVLink 1 çerçevesi: iki 0xFE de aday dışındadır,
   rx_mav1 = 2; çerçevenin bütün baytları rx_resync_bytes'a girer. */
static void test_A1_K02_e_ikinci_fe_de_sayilir(void)
{
    kabul_rng_t r;
    kabul_rng_kur(&r, 0x4B30320007ull);
    size_t k = mav1_kur(akis, &r, 40u, 0u, 50u, 250u, 25u);
    akis[6u + 10u] = 0xFEu; /* MAVLink 1 sağlaması bu katmanda denetlenmez */
    size_t n = k + kabul_temiz_kur(&akis[k], &r, 46010u, 6u, 250u, 25u, 17u, 1, NULL);
    ida_link_sayaclar_t b = kabul_sayac_sifir();
    b.rx_bytes = (uint32_t)n;
    b.rx_frames = 1u;
    b.rx_mav1 = 2u;
    b.rx_resync_bytes = (uint32_t)k;
    besle_denetle(IDA_LINK_ROL_KART, n, 1u, &b, "ikinci 0xFE");
}

/* (f): MAVLink 1 çerçevesi len'e göre atlanmaz: yükündeki tam bir geçerli MAVLink 2
   çerçevesi teslim edilir; MAVLink 1'in geri kalan baytları resync. */
static void test_A1_K02_f_mavlink1_icindeki_cerceve_teslim_edilir(void)
{
    kabul_rng_t r;
    kabul_rng_kur(&r, 0x4B30320008ull);
    uint8_t ic[32];
    size_t ic_n = kabul_temiz_kur(ic, &r, 46003u, 5u, 250u, 25u, 3u, 1, NULL);
    size_t k = mav1_kur(akis, &r, 40u, 0u, 50u, 250u, 25u);
    memcpy(&akis[6u + 5u], ic, ic_n);
    size_t n = k + kabul_temiz_kur(&akis[k], &r, 46010u, 6u, 250u, 25u, 17u, 1, NULL);
    ida_link_sayaclar_t b = kabul_sayac_sifir();
    b.rx_bytes = (uint32_t)n;
    b.rx_frames = 2u;
    b.rx_mav1 = 1u;
    b.rx_resync_bytes = (uint32_t)(k - ic_n);
    besle_denetle(IDA_LINK_ROL_KART, n, 2u, &b, "MAVLink 1 içinde MAVLink 2");
    TEST_ASSERT_EQUAL_UINT32(46003u, sonuc.t[0].msgid);
    TEST_ASSERT_EQUAL_UINT8(5u, sonuc.t[0].seq);
    TEST_ASSERT_EQUAL_UINT32(46010u, sonuc.t[1].msgid);
}

/* (g): aday içindeki 0xFE sayılmaz: yükünde 0xFE olan geçerli çerçeve (rx_mav1 = 0) ve
   yükünde 0xFE olan, sağlaması bozuk aday (yalnız rx_crc_err; aday bütünüyle tüketilir). */
static void test_A1_K02_g_aday_icindeki_fe_sayilmaz(void)
{
    kabul_rng_t r;
    kabul_rng_kur(&r, 0x4B30320009ull);
    for (int bozuk = 0; bozuk < 2; ++bozuk) {
        const kabul_mesaj_t *m = kabul_mesaj_al(46020u);
        uint8_t yuk[IDA_LINK_YUK_AZAMI] = {0};
        size_t n = 0;
        for (int deneme = 0; deneme < 4096; ++deneme) {
            for (size_t i = 0; i < m->yuk_azami; ++i) {
                yuk[i] = (uint8_t)(1u + kabul_rng_aralik(&r, 0xFCu));
            }
            yuk[5] = 0xFEu;
            yuk[20] = 0xFEu;
            n = kabul_cerceve_ham(akis, m->yuk_azami, 0u, 0u, 1u, 250u, 25u, m->msgid, yuk, m->yuk_azami,
                                  m->crc_extra);
            if (akis[n - 1u] < 0xFDu && akis[n - 2u] < 0xFDu) {
                break;
            }
        }
        if (bozuk) {
            akis[n - 1u] = (uint8_t)(akis[n - 1u] == 0x01u ? 0x02u : 0x01u);
        }
        n += kabul_temiz_kur(&akis[n], &r, 46021u, 2u, 250u, 25u, 9u, 1, NULL);
        ida_link_sayaclar_t b = kabul_sayac_sifir();
        b.rx_bytes = (uint32_t)n;
        b.rx_frames = bozuk ? 1u : 2u;
        b.rx_crc_err = bozuk ? 1u : 0u;
        b.rx_seq_gap = 0u;
        besle_denetle(IDA_LINK_ROL_KART, n, bozuk ? 1u : 2u, &b, bozuk ? "bozuk aday içinde 0xFE"
                                                                       : "geçerli çerçeve içinde 0xFE");
    }
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_A1_K02_a_gonderen_magic_ve_bayraklar_sifir);
    RUN_TEST(test_A1_K02_c_incompat_atilir_rx_iflag);
    RUN_TEST(test_A1_K02_c_imzali_cerceve_atilir);
    RUN_TEST(test_A1_K02_d_compat_yok_sayilir);
    RUN_TEST(test_A1_K02_e_mavlink1_baytlari);
    RUN_TEST(test_A1_K02_e_tek_fe_bayti);
    RUN_TEST(test_A1_K02_e_ikinci_fe_de_sayilir);
    RUN_TEST(test_A1_K02_f_mavlink1_icindeki_cerceve_teslim_edilir);
    RUN_TEST(test_A1_K02_g_aday_icindeki_fe_sayilmaz);
    return UNITY_END();
}
