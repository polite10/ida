/*
 * test_tel.c — ida_link tel katmanı birim testleri (W2-protokol §4; Unity).
 *
 * A1 §7.1'in bu katmana düşen satırları: K02, K04–K08 (başlık hatası türleri, atılan
 * çerçeve, artan sayaç, ardından gelen geçerli çerçevenin kabulü), K05 sınırları, K07
 * (belirlenimci sözde rastgele akışa gömülü çerçeveler), K08, K101 (iki örneğin karışık
 * beslenmesi) ve A1 Ek B'nin 38 altın çerçevesi. Rastgelelik sabit tohumludur.
 *
 * Hatalı çerçeveler testin kendi CRC-16/MCRF4XX uygulamasıyla kurulur (kitaplığınkinden
 * bağımsız); CRC_EXTRA değerleri Ek B'den (altın veri) gelir.
 *
 * PC'de CTest ile koşar; firmware kökünde aynı kaynak arm-none-eabi ile bağlanır.
 */
#include "ida_link/tel.h"

#include "ida_link_altin_veri.h"
#include "mavlink/ida/mavlink.h"
#include "unity.h"

#include <string.h>

void setUp(void);
void tearDown(void);
void setUp(void) {}
void tearDown(void) {}

/* ------------------------------------------------------------------ */
/* Yardımcılar                                                        */
/* ------------------------------------------------------------------ */

static uint16_t test_crc(const uint8_t *p, size_t n, uint8_t ek)
{
    uint16_t crc = 0xFFFFu;
    for (size_t i = 0; i <= n; ++i) {
        const uint8_t b = i < n ? p[i] : ek;
        uint8_t t = (uint8_t)(b ^ (uint8_t)(crc & 0xFFu));
        t = (uint8_t)(t ^ (uint8_t)(t << 4));
        crc = (uint16_t)((crc >> 8) ^ ((uint16_t)t << 8) ^ ((uint16_t)t << 3) ^ (t >> 4));
    }
    return crc;
}

/* Testin bağımsız denetimi: CRC-16/MCRF4XX "123456789" = 0x6F91 (A1 §3.2). */
static void test_crc_denetim_degeri(void)
{
    const uint8_t s[9] = {'1', '2', '3', '4', '5', '6', '7', '8'};
    /* Son baytı CRC_EXTRA yerine besleyerek dokuz baytın CRC'si. */
    TEST_ASSERT_EQUAL_HEX16(0x6F91u, test_crc(s, 8u, (uint8_t)'9'));
}

/* Çerçeve kurar; dönüş toplam boy. crc_bozuk != 0 ise sağlamanın düşük baytı ters çevrilir. */
static size_t cerceve_kur(uint8_t *o, uint8_t len, uint8_t iflag, uint8_t seq, uint8_t sys,
                          uint8_t comp, uint32_t msgid, const uint8_t *yuk, uint8_t crc_extra,
                          int crc_bozuk)
{
    o[0] = 0xFDu;
    o[1] = len;
    o[2] = iflag;
    o[3] = 0u;
    o[4] = seq;
    o[5] = sys;
    o[6] = comp;
    o[7] = (uint8_t)(msgid & 0xFFu);
    o[8] = (uint8_t)((msgid >> 8) & 0xFFu);
    o[9] = (uint8_t)((msgid >> 16) & 0xFFu);
    if (len > 0u) {
        memcpy(&o[10], yuk, len);
    }
    uint16_t c = test_crc(&o[1], 9u + (size_t)len, crc_extra);
    if (crc_bozuk) {
        c ^= 0x00FFu;
    }
    o[10u + len] = (uint8_t)(c & 0xFFu);
    o[11u + len] = (uint8_t)(c >> 8);
    return 12u + (size_t)len;
}

/* Ek B'deki (ek vaka olmayan) çerçeve. Bulunmazsa test başarısız sayılır; kartta Unity
   testi kesmediğinden (setjmp yok) geçerli bir öğe döner, NULL dönmez. */
static const ida_link_altin_t *altin_bul(uint32_t msgid)
{
    for (unsigned i = 0; i < IDA_LINK_ALTIN_SAYISI; ++i) {
        if (ida_link_altin[i].msgid == msgid && !ida_link_altin[i].ek_vaka) {
            return &ida_link_altin[i];
        }
    }
    TEST_FAIL_MESSAGE("Ek B'de beklenen mesaj yok");
    return &ida_link_altin[0];
}

/* Bütün baytları besler; teslim edilen çerçeveleri sırayla kaydeder (en çok kap tane). */
typedef struct {
    uint32_t msgid[64];
    uint8_t seq[64];
    unsigned n;
} teslimler_t;

static void hepsini_besle(ida_link_alici_t *a, const uint8_t *v, size_t n, teslimler_t *t)
{
    static ida_link_cerceve_t c;
    size_t i = 0;
    while (i < n) {
        size_t tk = 0;
        const ida_link_rc_t rc = ida_link_al(a, v + i, n - i, &tk, &c);
        TEST_ASSERT_TRUE(rc == IDA_LINK_CERCEVE || rc == IDA_LINK_BEKLE);
        TEST_ASSERT_TRUE(tk >= 1u && tk <= n - i);
        i += tk;
        if (rc == IDA_LINK_CERCEVE && t != NULL && t->n < 64u) {
            t->msgid[t->n] = c.msgid;
            t->seq[t->n] = c.seq;
            t->n++;
        }
    }
}

static void sayaclar_esit(const ida_link_sayaclar_t *b, const ida_link_sayaclar_t *s)
{
    TEST_ASSERT_EQUAL_MEMORY(b, s, sizeof *b);
}

/* Sim -> kart yönündeki geçerli bir IMU çerçevesi (altın; sysid 250). */
static size_t gecerli_imu(uint8_t *o, uint8_t seq)
{
    const ida_link_altin_t *g = altin_bul(MAVLINK_MSG_ID_IDA_IMU);
    TEST_ASSERT_NOT_NULL(g);
    return cerceve_kur(o, (uint8_t)g->yuk, 0u, seq, 250u, 25u, g->msgid, &g->bayt[10],
                       g->crc_extra, 0);
}

/* Kurulu kart alıcısına bir hatalı çerçeve ve ardından geçerli IMU verir; tek teslim
   beklenir. Dönüş: sayaçlar. */
static ida_link_sayaclar_t hata_sonra_gecerli(const uint8_t *hatali, size_t n_hatali)
{
    static uint8_t akis[600];
    static ida_link_alici_t a;
    TEST_ASSERT_TRUE(n_hatali + 80u <= sizeof akis);
    memcpy(akis, hatali, n_hatali);
    const size_t n = n_hatali + gecerli_imu(&akis[n_hatali], 7u);
    TEST_ASSERT_EQUAL_INT(IDA_LINK_TAMAM, ida_link_alici_kur(&a, IDA_LINK_ROL_KART));
    teslimler_t t = {{0}, {0}, 0};
    hepsini_besle(&a, akis, n, &t);
    TEST_ASSERT_EQUAL_UINT(1u, t.n);
    TEST_ASSERT_EQUAL_UINT32(MAVLINK_MSG_ID_IDA_IMU, t.msgid[0]);
    TEST_ASSERT_EQUAL_UINT32(1u, a.sayac.rx_frames);
    TEST_ASSERT_EQUAL_UINT32((uint32_t)n, a.sayac.rx_bytes);
    return a.sayac;
}

/* ------------------------------------------------------------------ */
/* Altın çerçeveler (A1 Ek B; kabul md. 2)                            */
/* ------------------------------------------------------------------ */

typedef union {
    uint64_t hiza;
    uint8_t b[512];
} yapi_tamponu_t;

static void test_altin_38_cerceve(void)
{
    TEST_ASSERT_EQUAL_UINT(38u, IDA_LINK_ALTIN_SAYISI);
    for (unsigned i = 0; i < IDA_LINK_ALTIN_SAYISI; ++i) {
        const ida_link_altin_t *g = &ida_link_altin[i];
        const int simden = g->sysid == IDA_LINK_SIM_SYSID;
        static ida_link_alici_t a;
        static ida_link_cerceve_t c;
        static ida_link_cerceve_t c2;
        static uint8_t tam[IDA_LINK_CERCEVE_AZAMI];
        static uint8_t yeniden[IDA_LINK_CERCEVE_AZAMI];
        static yapi_tamponu_t yapi;
        static yapi_tamponu_t yapi2;
        size_t tk = 0;

        /* Alıcı kabul eder; kimlik, yük boyu ve CRC_EXTRA Ek B'nin başlık satırıyla aynı. */
        TEST_ASSERT_EQUAL_INT(IDA_LINK_TAMAM,
                              ida_link_alici_kur(&a, simden ? IDA_LINK_ROL_KART : IDA_LINK_ROL_SIM));
        TEST_ASSERT_EQUAL_INT_MESSAGE(IDA_LINK_CERCEVE, ida_link_al(&a, g->bayt, g->cerceve, &tk, &c),
                                      g->ad);
        TEST_ASSERT_EQUAL_size_t(g->cerceve, tk);
        TEST_ASSERT_EQUAL_UINT32(g->msgid, c.msgid);
        TEST_ASSERT_EQUAL_UINT8(g->seq, c.seq);
        TEST_ASSERT_EQUAL_UINT8(g->yuk, c.len);
        TEST_ASSERT_EQUAL_UINT8(g->yuk_azami, c.yuk_azami);
        ida_link_mesaj_bilgisi_t b;
        TEST_ASSERT_EQUAL_INT(IDA_LINK_TAMAM, ida_link_mesaj_bilgisi(g->msgid, &b));
        TEST_ASSERT_EQUAL_UINT8(g->crc_extra, b.crc_extra);
        TEST_ASSERT_EQUAL_UINT8(g->yuk_azami, b.yuk_azami);
        TEST_ASSERT_TRUE(b.yapi_boyu <= sizeof yapi.b);

        /* Çözülen mesaj yeniden kodlanınca bayt bayt aynı dizi. */
        TEST_ASSERT_EQUAL_INT(IDA_LINK_TAMAM, ida_link_coz(&c, g->msgid, yapi.b, b.yapi_boyu));
        ida_link_gonderici_t gd;
        TEST_ASSERT_EQUAL_INT(IDA_LINK_TAMAM,
                              ida_link_gonderici_kur(&gd, simden ? IDA_LINK_ROL_SIM : IDA_LINK_ROL_KART));
        gd.seq = g->seq;
        size_t yz = 0;
        TEST_ASSERT_EQUAL_INT(IDA_LINK_TAMAM,
                              ida_link_gonder(&gd, g->msgid, yapi.b, b.yapi_boyu, yeniden,
                                              sizeof yeniden, &yz));
        TEST_ASSERT_EQUAL_size_t(g->cerceve, yz);
        TEST_ASSERT_EQUAL_HEX8_ARRAY_MESSAGE(g->bayt, yeniden, g->cerceve, g->ad);
        TEST_ASSERT_EQUAL_UINT8((uint8_t)(g->seq + 1u), gd.seq);

        /* Kırpılmamış biçim (len = en büyük yük) de kabul edilir ve aynı mesajı verir. */
        uint8_t yuk[IDA_LINK_YUK_AZAMI];
        memset(yuk, 0, sizeof yuk);
        memcpy(yuk, &g->bayt[10], g->yuk);
        const size_t n_tam = cerceve_kur(tam, (uint8_t)g->yuk_azami, 0u, g->seq, g->sysid,
                                         g->compid, g->msgid, yuk, g->crc_extra, 0);
        TEST_ASSERT_EQUAL_size_t((size_t)g->cerceve_azami, n_tam);
        TEST_ASSERT_EQUAL_INT(IDA_LINK_TAMAM,
                              ida_link_alici_kur(&a, simden ? IDA_LINK_ROL_KART : IDA_LINK_ROL_SIM));
        TEST_ASSERT_EQUAL_INT(IDA_LINK_CERCEVE, ida_link_al(&a, tam, n_tam, &tk, &c2));
        TEST_ASSERT_EQUAL_UINT8(g->yuk_azami, c2.len);
        TEST_ASSERT_EQUAL_HEX8_ARRAY(c.yuk, c2.yuk, IDA_LINK_YUK_AZAMI);
        TEST_ASSERT_EQUAL_INT(IDA_LINK_TAMAM, ida_link_coz(&c2, g->msgid, yapi2.b, b.yapi_boyu));
        TEST_ASSERT_EQUAL_HEX8_ARRAY(yapi.b, yapi2.b, b.yapi_boyu);
    }
}

/* Tipli alanlar: Ek B'nin alan satırlarından birkaçı (yapı düzeninin tel düzeniyle aynılığı). */
static void test_altin_tipli_alanlar(void)
{
    const ida_link_altin_t *g = altin_bul(MAVLINK_MSG_ID_IDA_TIME_FRAME);
    static ida_link_alici_t a;
    static ida_link_cerceve_t c;
    size_t tk = 0;
    TEST_ASSERT_EQUAL_INT(IDA_LINK_TAMAM, ida_link_alici_kur(&a, IDA_LINK_ROL_KART));
    TEST_ASSERT_EQUAL_INT(IDA_LINK_CERCEVE, ida_link_al(&a, g->bayt, g->cerceve, &tk, &c));
    mavlink_ida_time_frame_t tf;
    TEST_ASSERT_EQUAL_INT(IDA_LINK_TAMAM, ida_link_coz(&c, g->msgid, &tf, sizeof tf));
    /* Ek B 46010: t_sim_ns = 12345000000; batch_seq = 2469; sess = 6699; n_members = 4;
       tf_flags = 1; run_mode = 3 */
    TEST_ASSERT_TRUE(tf.t_sim_ns == 12345000000LL);
    TEST_ASSERT_EQUAL_UINT32(2469u, tf.batch_seq);
    TEST_ASSERT_EQUAL_UINT16(6699u, tf.sess);
    TEST_ASSERT_EQUAL_UINT8(4u, tf.n_members);
    TEST_ASSERT_EQUAL_UINT8(1u, tf.tf_flags);
    TEST_ASSERT_EQUAL_UINT8(3u, tf.run_mode);

    const ida_link_altin_t *ga = altin_bul(MAVLINK_MSG_ID_IDA_ACTUATOR);
    TEST_ASSERT_EQUAL_INT(IDA_LINK_TAMAM, ida_link_alici_kur(&a, IDA_LINK_ROL_SIM));
    TEST_ASSERT_EQUAL_INT(IDA_LINK_CERCEVE, ida_link_al(&a, ga->bayt, ga->cerceve, &tk, &c));
    mavlink_ida_actuator_t ak;
    TEST_ASSERT_EQUAL_INT(IDA_LINK_TAMAM, ida_link_coz(&c, ga->msgid, &ak, sizeof ak));
    /* Ek B 46060: pwm_us[4] = 1550; out_seq = 1234 (kırpılmış kuyrukta, sıfırla tamamlanır) */
    TEST_ASSERT_EQUAL_UINT16(1550u, ak.pwm_us[4]);
    TEST_ASSERT_EQUAL_UINT16(3210u, ak.rx_to_tx_us);
    TEST_ASSERT_EQUAL_UINT8(0u, ak.n_merged);
}

/* ------------------------------------------------------------------ */
/* A1-K02                                                             */
/* ------------------------------------------------------------------ */

static void test_k02_incompat_flags_reddedilir(void)
{
    const ida_link_altin_t *g = altin_bul(MAVLINK_MSG_ID_IDA_KEEPALIVE);
    uint8_t h[64];
    /* CRC doğru, incompat_flags = 1 (imza biti). seq, sağlama baytları 0xFD/0xFE olmayacak
       biçimde seçilir (bayt sayımı kesin olsun). */
    size_t n = 0;
    for (unsigned seq = 0; seq < 256u; ++seq) {
        n = cerceve_kur(h, (uint8_t)g->yuk, 0x01u, (uint8_t)seq, 250u, 25u, g->msgid,
                        &g->bayt[10], g->crc_extra, 0);
        if (h[n - 1u] < 0xFDu && h[n - 2u] < 0xFDu) {
            break;
        }
    }
    const ida_link_sayaclar_t s = hata_sonra_gecerli(h, n);
    TEST_ASSERT_EQUAL_UINT32(1u, s.rx_iflag);
    TEST_ASSERT_EQUAL_UINT32(0u, s.rx_crc_err);
    TEST_ASSERT_EQUAL_UINT32(0u, s.rx_bad_len);
    /* Reddedilen adayın 0xFD dışındaki 14 baytı yeniden tarandı ve aday başlatmadı. */
    TEST_ASSERT_EQUAL_UINT32((uint32_t)(n - 1u), s.rx_resync_bytes);
}

static void test_k02_compat_flags_yok_sayilir(void)
{
    const ida_link_altin_t *g = altin_bul(MAVLINK_MSG_ID_IDA_KEEPALIVE);
    uint8_t h[64];
    const size_t n = cerceve_kur(h, (uint8_t)g->yuk, 0u, 3u, 250u, 25u, g->msgid,
                                 &g->bayt[10], g->crc_extra, 0);
    h[3] = 0x80u;  /* compat_flags; sağlama yeniden hesaplanır */
    const uint16_t c = test_crc(&h[1], n - 3u, g->crc_extra);
    h[n - 2u] = (uint8_t)(c & 0xFFu);
    h[n - 1u] = (uint8_t)(c >> 8);
    static ida_link_alici_t a;
    static ida_link_cerceve_t cc;
    size_t tk = 0;
    TEST_ASSERT_EQUAL_INT(IDA_LINK_TAMAM, ida_link_alici_kur(&a, IDA_LINK_ROL_KART));
    TEST_ASSERT_EQUAL_INT(IDA_LINK_CERCEVE, ida_link_al(&a, h, n, &tk, &cc));
    TEST_ASSERT_EQUAL_UINT32(g->msgid, cc.msgid);
}

static void test_k02_mavlink1_atilir(void)
{
    /* MAVLink 1 HEARTBEAT biçimli 17 bayt: FE, len 9, seq, sys, comp, msgid 0, yük, sağlama.
       Sağlama baytları bilerek FD/FE içermez. */
    const uint8_t v1[17] = {0xFEu, 0x09u, 0x00u, 0xFAu, 0x19u, 0x00u, 0x00u, 0x00u, 0x00u,
                            0x00u, 0x02u, 0x03u, 0x51u, 0x04u, 0x03u, 0x1Cu, 0x7Fu};
    const ida_link_sayaclar_t s = hata_sonra_gecerli(v1, sizeof v1);
    TEST_ASSERT_EQUAL_UINT32(1u, s.rx_mav1);
    TEST_ASSERT_EQUAL_UINT32(16u, s.rx_resync_bytes);
    TEST_ASSERT_EQUAL_UINT32(0u, s.rx_crc_err);
}

static void test_k02_gonderici_bayraklari_sifir(void)
{
    ida_link_gonderici_t g;
    mavlink_ida_keepalive_t k;
    memset(&k, 0, sizeof k);
    k.sess = 5u;
    uint8_t o[32];
    size_t yz = 0;
    TEST_ASSERT_EQUAL_INT(IDA_LINK_TAMAM, ida_link_gonderici_kur(&g, IDA_LINK_ROL_KART));
    TEST_ASSERT_EQUAL_INT(IDA_LINK_TAMAM,
                          ida_link_gonder(&g, MAVLINK_MSG_ID_IDA_KEEPALIVE, &k, sizeof k, o,
                                          sizeof o, &yz));
    TEST_ASSERT_EQUAL_HEX8(0xFDu, o[0]);
    TEST_ASSERT_EQUAL_HEX8(0u, o[2]);
    TEST_ASSERT_EQUAL_HEX8(0u, o[3]);
}

/* ------------------------------------------------------------------ */
/* A1-K04                                                             */
/* ------------------------------------------------------------------ */

static void test_k04_yabanci_sysid_compid(void)
{
    const ida_link_altin_t *g = altin_bul(MAVLINK_MSG_ID_IDA_KEEPALIVE);
    uint8_t h[64];
    size_t n = cerceve_kur(h, (uint8_t)g->yuk, 0u, 3u, 1u, 25u, g->msgid, &g->bayt[10],
                           g->crc_extra, 0);
    n += cerceve_kur(&h[n], (uint8_t)g->yuk, 0u, 4u, 250u, 1u, g->msgid, &g->bayt[10],
                     g->crc_extra, 0);
    const ida_link_sayaclar_t s = hata_sonra_gecerli(h, n);
    TEST_ASSERT_EQUAL_UINT32(2u, s.rx_wrong_sys);
    TEST_ASSERT_EQUAL_UINT32(0u, s.rx_resync_bytes);
    TEST_ASSERT_EQUAL_UINT32(0u, s.rx_crc_err);
}

static void test_k04_sim_rolu_yalniz_karti_kabul_eder(void)
{
    const ida_link_altin_t *g = altin_bul(MAVLINK_MSG_ID_IDA_KEEPALIVE);
    uint8_t h[64];
    static ida_link_alici_t a;
    teslimler_t t = {{0}, {0}, 0};
    size_t n = cerceve_kur(h, (uint8_t)g->yuk, 0u, 0u, 250u, 25u, g->msgid, &g->bayt[10],
                           g->crc_extra, 0);  /* sim'in kendi kimliği: ret */
    n += cerceve_kur(&h[n], (uint8_t)g->yuk, 0u, 1u, 1u, 1u, g->msgid, &g->bayt[10],
                     g->crc_extra, 0);       /* kart: kabul */
    TEST_ASSERT_EQUAL_INT(IDA_LINK_TAMAM, ida_link_alici_kur(&a, IDA_LINK_ROL_SIM));
    hepsini_besle(&a, h, n, &t);
    TEST_ASSERT_EQUAL_UINT(1u, t.n);
    TEST_ASSERT_EQUAL_UINT32(1u, a.sayac.rx_wrong_sys);
}

static void test_k04_gonderici_kimligi_role_gore(void)
{
    mavlink_ida_keepalive_t k;
    memset(&k, 0, sizeof k);
    uint8_t o[32];
    size_t yz = 0;
    ida_link_gonderici_t g;
    TEST_ASSERT_EQUAL_INT(IDA_LINK_TAMAM, ida_link_gonderici_kur(&g, IDA_LINK_ROL_SIM));
    TEST_ASSERT_EQUAL_INT(IDA_LINK_TAMAM, ida_link_gonder(&g, MAVLINK_MSG_ID_IDA_KEEPALIVE, &k,
                                                          sizeof k, o, sizeof o, &yz));
    TEST_ASSERT_EQUAL_UINT8(250u, o[5]);
    TEST_ASSERT_EQUAL_UINT8(25u, o[6]);
    TEST_ASSERT_EQUAL_INT(IDA_LINK_TAMAM, ida_link_gonderici_kur(&g, IDA_LINK_ROL_KART));
    TEST_ASSERT_EQUAL_INT(IDA_LINK_TAMAM, ida_link_gonder(&g, MAVLINK_MSG_ID_IDA_KEEPALIVE, &k,
                                                          sizeof k, o, sizeof o, &yz));
    TEST_ASSERT_EQUAL_UINT8(1u, o[5]);
    TEST_ASSERT_EQUAL_UINT8(1u, o[6]);
}

/* ------------------------------------------------------------------ */
/* A1-K05                                                             */
/* ------------------------------------------------------------------ */

static void test_k05_len_sifir_reddedilir(void)
{
    const ida_link_altin_t *g = altin_bul(MAVLINK_MSG_ID_IDA_KEEPALIVE);
    uint8_t h[64];
    const size_t n = cerceve_kur(h, 0u, 0u, 3u, 250u, 25u, g->msgid, NULL, g->crc_extra, 0);
    const ida_link_sayaclar_t s = hata_sonra_gecerli(h, n);
    TEST_ASSERT_EQUAL_UINT32(1u, s.rx_bad_len);
    TEST_ASSERT_EQUAL_UINT32(0u, s.rx_crc_err);
}

static void test_k05_len_azamiyi_asan_reddedilir(void)
{
    const ida_link_altin_t *g = altin_bul(MAVLINK_MSG_ID_IDA_KEEPALIVE);  /* azami 3 */
    uint8_t yuk[4] = {1u, 2u, 3u, 4u};
    uint8_t h[64];
    const size_t n = cerceve_kur(h, 4u, 0u, 3u, 250u, 25u, g->msgid, yuk, g->crc_extra, 0);
    const ida_link_sayaclar_t s = hata_sonra_gecerli(h, n);
    TEST_ASSERT_EQUAL_UINT32(1u, s.rx_bad_len);
    TEST_ASSERT_EQUAL_UINT32(0u, s.rx_crc_err);
}

static void test_k05_len_sinirlari_kabul(void)
{
    /* len = 1 ve len = azami (246, PING360_DATA) kabul edilir. */
    const ida_link_altin_t *g = altin_bul(MAVLINK_MSG_ID_IDA_PING360_DATA);
    static uint8_t yuk[IDA_LINK_YUK_AZAMI];
    static uint8_t h[600];
    static ida_link_alici_t a;
    teslimler_t t = {{0}, {0}, 0};
    memset(yuk, 0xA5, sizeof yuk);
    size_t n = cerceve_kur(h, 1u, 0u, 0u, 250u, 25u, g->msgid, yuk, g->crc_extra, 0);
    n += cerceve_kur(&h[n], 246u, 0u, 1u, 250u, 25u, g->msgid, yuk, g->crc_extra, 0);
    n += cerceve_kur(&h[n], 247u, 0u, 2u, 250u, 25u, g->msgid, yuk, g->crc_extra, 0);
    TEST_ASSERT_EQUAL_INT(IDA_LINK_TAMAM, ida_link_alici_kur(&a, IDA_LINK_ROL_KART));
    hepsini_besle(&a, h, n, &t);
    TEST_ASSERT_EQUAL_UINT(2u, t.n);
    TEST_ASSERT_EQUAL_UINT32(1u, a.sayac.rx_bad_len);
}

static void test_k05_kirpilmis_yuk_sifirla_doldurulur(void)
{
    /* Ek vaka (A1-K51): 1 baytlık tel yükü; teslim edilen yük 246 bayta kadar sıfır. */
    const ida_link_altin_t *g = &ida_link_altin[IDA_LINK_ALTIN_SAYISI - 1u];
    TEST_ASSERT_TRUE(g->ek_vaka);
    static ida_link_alici_t a;
    static ida_link_cerceve_t c;
    size_t tk = 0;
    memset(&c, 0x5A, sizeof c);
    TEST_ASSERT_EQUAL_INT(IDA_LINK_TAMAM, ida_link_alici_kur(&a, IDA_LINK_ROL_KART));
    TEST_ASSERT_EQUAL_INT(IDA_LINK_CERCEVE, ida_link_al(&a, g->bayt, g->cerceve, &tk, &c));
    TEST_ASSERT_EQUAL_UINT8(1u, c.len);
    TEST_ASSERT_EQUAL_UINT8(246u, c.yuk_azami);
    TEST_ASSERT_EQUAL_UINT8(1u, c.yuk[0]);
    for (unsigned i = 1; i < IDA_LINK_YUK_AZAMI; ++i) {
        TEST_ASSERT_EQUAL_UINT8(0u, c.yuk[i]);
    }
}

static void test_k05_gonderici_kirpar_ilk_bayt_kalir(void)
{
    mavlink_ida_keepalive_t k;
    memset(&k, 0, sizeof k);
    uint8_t o[32];
    size_t yz = 0;
    ida_link_gonderici_t g;
    TEST_ASSERT_EQUAL_INT(IDA_LINK_TAMAM, ida_link_gonderici_kur(&g, IDA_LINK_ROL_SIM));
    TEST_ASSERT_EQUAL_INT(IDA_LINK_TAMAM, ida_link_gonder(&g, MAVLINK_MSG_ID_IDA_KEEPALIVE, &k,
                                                          sizeof k, o, sizeof o, &yz));
    TEST_ASSERT_EQUAL_size_t(13u, yz);
    TEST_ASSERT_EQUAL_UINT8(1u, o[1]);
    TEST_ASSERT_EQUAL_UINT8(0u, o[10]);
    /* Sondaki sıfır kırpılır, ortadaki kalır: sess = 0x0100 -> yük 00 01 [00] -> len 2. */
    k.sess = 0x0100u;
    TEST_ASSERT_EQUAL_INT(IDA_LINK_TAMAM, ida_link_gonder(&g, MAVLINK_MSG_ID_IDA_KEEPALIVE, &k,
                                                          sizeof k, o, sizeof o, &yz));
    TEST_ASSERT_EQUAL_UINT8(2u, o[1]);
    TEST_ASSERT_EQUAL_size_t(14u, yz);
}

/* ------------------------------------------------------------------ */
/* A1-K06                                                             */
/* ------------------------------------------------------------------ */

static void test_k06_dialect_disi_kimlik(void)
{
    uint8_t yuk[3] = {1u, 2u, 3u};
    uint8_t h[64];
    size_t n = cerceve_kur(h, 3u, 0u, 3u, 250u, 25u, 46099u, yuk, 0x11u, 0);
    n += cerceve_kur(&h[n], 3u, 0u, 4u, 250u, 25u, 0u, yuk, 50u, 0);  /* HEARTBEAT: yok */
    const ida_link_sayaclar_t s = hata_sonra_gecerli(h, n);
    TEST_ASSERT_EQUAL_UINT32(2u, s.rx_unknown_msgid);
    TEST_ASSERT_EQUAL_UINT32(0u, s.rx_crc_err);
}

static void test_k06_yonu_yanlis_kimlik(void)
{
    /* Kart alıcısına kart -> sim mesajı (IDA_ACTUATOR), sim kimliğiyle ve doğru CRC'yle. */
    const ida_link_altin_t *g = altin_bul(MAVLINK_MSG_ID_IDA_ACTUATOR);
    uint8_t h[128];
    const size_t n = cerceve_kur(h, (uint8_t)g->yuk, 0u, 3u, 250u, 25u, g->msgid, &g->bayt[10],
                                 g->crc_extra, 0);
    const ida_link_sayaclar_t s = hata_sonra_gecerli(h, n);
    TEST_ASSERT_EQUAL_UINT32(1u, s.rx_unknown_msgid);
    TEST_ASSERT_EQUAL_UINT32(0u, s.rx_resync_bytes);  /* bütün çerçeve tüketildi */

    /* Sim alıcısına sim -> kart mesajı (IDA_IMU), kart kimliğiyle. */
    const ida_link_altin_t *gi = altin_bul(MAVLINK_MSG_ID_IDA_IMU);
    static ida_link_alici_t a;
    teslimler_t t = {{0}, {0}, 0};
    const size_t n2 = cerceve_kur(h, (uint8_t)gi->yuk, 0u, 3u, 1u, 1u, gi->msgid,
                                  &gi->bayt[10], gi->crc_extra, 0);
    TEST_ASSERT_EQUAL_INT(IDA_LINK_TAMAM, ida_link_alici_kur(&a, IDA_LINK_ROL_SIM));
    hepsini_besle(&a, h, n2, &t);
    TEST_ASSERT_EQUAL_UINT(0u, t.n);
    TEST_ASSERT_EQUAL_UINT32(1u, a.sayac.rx_unknown_msgid);
}

static void test_k06_iki_yonlu_mesajlar_iki_rolde(void)
{
    const uint32_t iki[3] = {MAVLINK_MSG_ID_IDA_KEEPALIVE, MAVLINK_MSG_ID_IDA_SESSION_CLOSE,
                             MAVLINK_MSG_ID_IDA_DET_RECORD};
    for (unsigned i = 0; i < 3u; ++i) {
        const ida_link_altin_t *g = altin_bul(iki[i]);
        uint8_t h[128];
        static ida_link_alici_t a;
        teslimler_t t = {{0}, {0}, 0};
        size_t n = cerceve_kur(h, (uint8_t)g->yuk, 0u, 0u, 1u, 1u, g->msgid, &g->bayt[10],
                               g->crc_extra, 0);
        TEST_ASSERT_EQUAL_INT(IDA_LINK_TAMAM, ida_link_alici_kur(&a, IDA_LINK_ROL_SIM));
        hepsini_besle(&a, h, n, &t);
        TEST_ASSERT_EQUAL_UINT(1u, t.n);
        n = cerceve_kur(h, (uint8_t)g->yuk, 0u, 0u, 250u, 25u, g->msgid, &g->bayt[10],
                        g->crc_extra, 0);
        TEST_ASSERT_EQUAL_INT(IDA_LINK_TAMAM, ida_link_alici_kur(&a, IDA_LINK_ROL_KART));
        hepsini_besle(&a, h, n, &t);
        TEST_ASSERT_EQUAL_UINT(2u, t.n);
        ida_link_mesaj_bilgisi_t b;
        TEST_ASSERT_EQUAL_INT(IDA_LINK_TAMAM, ida_link_mesaj_bilgisi(iki[i], &b));
        TEST_ASSERT_EQUAL_UINT8(IDA_LINK_YON_IKI, b.yon);
    }
}

static void test_k06_gonderici_yonu_reddeder(void)
{
    mavlink_ida_imu_t imu;
    memset(&imu, 0, sizeof imu);
    uint8_t o[80];
    memset(o, 0xEE, sizeof o);
    size_t yz = 99u;
    ida_link_gonderici_t g;
    TEST_ASSERT_EQUAL_INT(IDA_LINK_TAMAM, ida_link_gonderici_kur(&g, IDA_LINK_ROL_KART));
    TEST_ASSERT_EQUAL_INT(IDA_LINK_HATA_YON, ida_link_gonder(&g, MAVLINK_MSG_ID_IDA_IMU, &imu,
                                                             sizeof imu, o, sizeof o, &yz));
    TEST_ASSERT_EQUAL_size_t(0u, yz);
    TEST_ASSERT_EQUAL_UINT8(0u, g.seq);
    TEST_ASSERT_EACH_EQUAL_HEX8(0xEEu, o, sizeof o);
}

/* ------------------------------------------------------------------ */
/* A1-K07                                                             */
/* ------------------------------------------------------------------ */

static void test_k07_crc_hatasi_aday_tuketilir(void)
{
    /* Bozuk sağlamalı adayın yükünde geçerli bir çerçeve başlangıcı (0xFD) olsa da aday
       bütünüyle tüketilir; ardından gelen geçerli çerçeve kabul edilir. */
    const ida_link_altin_t *g = altin_bul(MAVLINK_MSG_ID_IDA_MAG);  /* yükünde "fd 03" var */
    uint8_t h[128];
    const size_t n = cerceve_kur(h, (uint8_t)g->yuk, 0u, 3u, 250u, 25u, g->msgid, &g->bayt[10],
                                 g->crc_extra, 1);
    const ida_link_sayaclar_t s = hata_sonra_gecerli(h, n);
    TEST_ASSERT_EQUAL_UINT32(1u, s.rx_crc_err);
    TEST_ASSERT_EQUAL_UINT32(0u, s.rx_resync_bytes);
    TEST_ASSERT_EQUAL_UINT32(0u, s.rx_iflag + s.rx_bad_len + s.rx_unknown_msgid);
}

static void test_k07_cerceve_disi_baytlar_sayilir(void)
{
    /* FD/FE içermeyen 25 bayt çöp, sonra geçerli çerçeve. */
    uint8_t cop[25];
    for (unsigned i = 0; i < sizeof cop; ++i) {
        cop[i] = (uint8_t)(0x10u + i);
    }
    const ida_link_sayaclar_t s = hata_sonra_gecerli(cop, sizeof cop);
    TEST_ASSERT_EQUAL_UINT32(25u, s.rx_resync_bytes);
}

static void test_k07_reddedilen_baslik_sonrasi_cerceve_kacirilmaz(void)
{
    /* Tek başına 0xFD ve hemen ardından geçerli çerçeve: sahte aday başlıkta reddedilir,
       baytları yeniden taranır ve geçerli çerçeve bulunur. */
    const uint8_t on[2] = {0x33u, 0xFDu};
    const ida_link_sayaclar_t s = hata_sonra_gecerli(on, sizeof on);
    TEST_ASSERT_EQUAL_UINT32(1u, s.rx_resync_bytes);
    TEST_ASSERT_EQUAL_UINT32(1u, s.rx_iflag);  /* sahte adayın incompat baytı = IMU'nun len'i */
}

static void test_k07_buyutulmus_len_bolgesi(void)
{
    /* Adayın len baytı bozulup (azamiyi aşmadan) büyüyünce aday sonraki çerçevenin
       baytlarını da kaplar; sağlama tutmaz; kapladığı bölgeden sonraki ilk geçerli çerçeve
       yakalanır (A1 §7.1 K07). */
    const ida_link_altin_t *gk = altin_bul(MAVLINK_MSG_ID_IDA_KEEPALIVE);  /* 15 B */
    const ida_link_altin_t *gt = altin_bul(MAVLINK_MSG_ID_IDA_TIME_FRAME); /* azami 17 */
    uint8_t h[256];
    static ida_link_alici_t a;
    teslimler_t t = {{0}, {0}, 0};
    size_t n = cerceve_kur(h, (uint8_t)gt->yuk, 0u, 0u, 250u, 25u, gt->msgid, &gt->bayt[10],
                           gt->crc_extra, 0);
    const size_t bozuk = n;
    n += cerceve_kur(&h[n], 3u, 0u, 1u, 250u, 25u, gt->msgid, &gt->bayt[10], gt->crc_extra, 0);
    h[bozuk + 1u] = 17u;  /* len 3 -> 17: aday 29 bayt, ardından gelen KEEPALIVE'ı kaplar */
    n += cerceve_kur(&h[n], (uint8_t)gk->yuk, 0u, 2u, 250u, 25u, gk->msgid, &gk->bayt[10],
                     gk->crc_extra, 0);
    const size_t ikinci_keepalive = n;
    n += cerceve_kur(&h[n], (uint8_t)gk->yuk, 0u, 3u, 250u, 25u, gk->msgid, &gk->bayt[10],
                     gk->crc_extra, 0);
    (void)ikinci_keepalive;
    TEST_ASSERT_EQUAL_INT(IDA_LINK_TAMAM, ida_link_alici_kur(&a, IDA_LINK_ROL_KART));
    hepsini_besle(&a, h, n, &t);
    /* Aday: 15 B bozuk TF + ilk KEEPALIVE'ın 14 baytı; KEEPALIVE'ın son baytı çerçeve dışı. */
    TEST_ASSERT_EQUAL_UINT(2u, t.n);
    TEST_ASSERT_EQUAL_UINT32(MAVLINK_MSG_ID_IDA_TIME_FRAME, t.msgid[0]);
    TEST_ASSERT_EQUAL_UINT32(MAVLINK_MSG_ID_IDA_KEEPALIVE, t.msgid[1]);
    TEST_ASSERT_EQUAL_UINT8(3u, t.seq[1]);
    TEST_ASSERT_EQUAL_UINT32(1u, a.sayac.rx_crc_err);
    TEST_ASSERT_EQUAL_UINT32(1u, a.sayac.rx_resync_bytes);
}

/* Sabit tohumlu xorshift32. */
static uint32_t prng_durum;
static uint32_t prng(void)
{
    uint32_t x = prng_durum;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    prng_durum = x;
    return x;
}

static void test_k07_sozde_rastgele_akis(void)
{
    /* Rastgele çöp (FD ve FE dahil her bayt değeri), araya gömülü geçerli altın çerçeveler
       ve sağlaması bozulmuş adaylar. Beklenen: her geçerli çerçeve sırasıyla teslim edilir;
       her bozuk aday rx_crc_err'i bir artırır. Tohum sabittir; sonuç belirlenimcidir. */
    static uint8_t akis[16384];
    static uint32_t beklenen[96];
    static ida_link_alici_t a;
    static ida_link_cerceve_t c;
    unsigned n_beklenen = 0;
    unsigned n_bozuk = 0;
    size_t n = 0;
    prng_durum = 0x1DA2026u;

    while (n + 400u < sizeof akis && n_beklenen < 96u) {
        const unsigned cop = prng() % 40u;
        for (unsigned i = 0; i < cop; ++i) {
            akis[n++] = (uint8_t)prng();
        }
        /* Kart rolünün alacağı altın çerçevelerden biri (sim kimlikli). */
        const ida_link_altin_t *g;
        do {
            g = &ida_link_altin[prng() % IDA_LINK_ALTIN_SAYISI];
        } while (g->sysid != IDA_LINK_SIM_SYSID);
        const int boz = (prng() % 4u) == 0u;
        n += cerceve_kur(&akis[n], (uint8_t)g->yuk, 0u, (uint8_t)n_beklenen, 250u, 25u,
                         g->msgid, &g->bayt[10], g->crc_extra, boz);
        if (boz) {
            n_bozuk++;
        } else {
            beklenen[n_beklenen++] = g->msgid;
        }
    }

    TEST_ASSERT_EQUAL_INT(IDA_LINK_TAMAM, ida_link_alici_kur(&a, IDA_LINK_ROL_KART));
    unsigned alinan = 0;
    size_t i = 0;
    while (i < n) {
        size_t tk = 0;
        /* Değişken boylu parçalar (1–64 bayt). */
        size_t parca = 1u + (size_t)(prng() % 64u);
        if (parca > n - i) {
            parca = n - i;
        }
        const ida_link_rc_t rc = ida_link_al(&a, &akis[i], parca, &tk, &c);
        i += tk;
        if (rc == IDA_LINK_CERCEVE) {
            TEST_ASSERT_TRUE(alinan < n_beklenen);
            TEST_ASSERT_EQUAL_UINT32(beklenen[alinan], c.msgid);
            alinan++;
        }
    }
    TEST_ASSERT_TRUE(n_bozuk > 0u);
    TEST_ASSERT_EQUAL_UINT(n_beklenen, alinan);
    TEST_ASSERT_EQUAL_UINT32(n_beklenen, a.sayac.rx_frames);
    TEST_ASSERT_EQUAL_UINT32(n_bozuk, a.sayac.rx_crc_err);
    TEST_ASSERT_EQUAL_UINT32((uint32_t)n, a.sayac.rx_bytes);
}

/* ------------------------------------------------------------------ */
/* A1-K08                                                             */
/* ------------------------------------------------------------------ */

static void test_k08_seq_boslugu_sayilir(void)
{
    const ida_link_altin_t *g = altin_bul(MAVLINK_MSG_ID_IDA_KEEPALIVE);
    const uint8_t seqler[7] = {254u, 255u, 0u, 1u, 5u, 6u, 6u};  /* sarma; 1->5; 6->6 */
    uint8_t h[7 * 15];
    size_t n = 0;
    for (unsigned i = 0; i < 7u; ++i) {
        n += cerceve_kur(&h[n], (uint8_t)g->yuk, 0u, seqler[i], 250u, 25u, g->msgid,
                         &g->bayt[10], g->crc_extra, 0);
    }
    static ida_link_alici_t a;
    teslimler_t t = {{0}, {0}, 0};
    TEST_ASSERT_EQUAL_INT(IDA_LINK_TAMAM, ida_link_alici_kur(&a, IDA_LINK_ROL_KART));
    hepsini_besle(&a, h, n, &t);
    TEST_ASSERT_EQUAL_UINT(7u, t.n);  /* davranış değişmez: hepsi teslim */
    TEST_ASSERT_EQUAL_UINT32(2u, a.sayac.rx_seq_gap);
}

static void test_k08_reddedilen_cerceve_seq_gecmisini_degistirmez(void)
{
    const ida_link_altin_t *g = altin_bul(MAVLINK_MSG_ID_IDA_KEEPALIVE);
    uint8_t h[45];
    size_t n = cerceve_kur(h, (uint8_t)g->yuk, 0u, 10u, 250u, 25u, g->msgid, &g->bayt[10],
                           g->crc_extra, 0);
    n += cerceve_kur(&h[n], (uint8_t)g->yuk, 0u, 11u, 1u, 1u, g->msgid, &g->bayt[10],
                     g->crc_extra, 0);  /* yabancı kimlik: ret */
    n += cerceve_kur(&h[n], (uint8_t)g->yuk, 0u, 11u, 250u, 25u, g->msgid, &g->bayt[10],
                     g->crc_extra, 0);
    static ida_link_alici_t a;
    TEST_ASSERT_EQUAL_INT(IDA_LINK_TAMAM, ida_link_alici_kur(&a, IDA_LINK_ROL_KART));
    hepsini_besle(&a, h, n, NULL);
    TEST_ASSERT_EQUAL_UINT32(2u, a.sayac.rx_frames);
    TEST_ASSERT_EQUAL_UINT32(0u, a.sayac.rx_seq_gap);
}

static void test_k08_gonderici_seq_artar_ve_sarar(void)
{
    mavlink_ida_keepalive_t k;
    memset(&k, 0, sizeof k);
    uint8_t o[32];
    size_t yz = 0;
    ida_link_gonderici_t g;
    TEST_ASSERT_EQUAL_INT(IDA_LINK_TAMAM, ida_link_gonderici_kur(&g, IDA_LINK_ROL_KART));
    TEST_ASSERT_EQUAL_UINT8(0u, g.seq);
    for (unsigned i = 0; i < 300u; ++i) {
        TEST_ASSERT_EQUAL_INT(IDA_LINK_TAMAM,
                              ida_link_gonder(&g, MAVLINK_MSG_ID_IDA_KEEPALIVE, &k, sizeof k, o,
                                              sizeof o, &yz));
        TEST_ASSERT_EQUAL_UINT8((uint8_t)(i & 0xFFu), o[4]);
    }
    TEST_ASSERT_EQUAL_UINT8((uint8_t)(300u & 0xFFu), g.seq);
}

/* ------------------------------------------------------------------ */
/* A1-K101: iki örnek aynı süreçte                                    */
/* ------------------------------------------------------------------ */

typedef struct {
    ida_link_alici_t a;
    ida_link_cerceve_t c;
    uint32_t imza;      /* teslim edilen kimlik ve yüklerin birikimli özeti */
    unsigned teslim;
    size_t konum;
} k101_akis_t;

static void k101_ozetle(k101_akis_t *s)
{
    uint32_t h = s->imza ^ s->c.msgid;
    for (unsigned i = 0; i < IDA_LINK_YUK_AZAMI; ++i) {
        h = (h ^ s->c.yuk[i]) * 16777619u;
    }
    s->imza = h ^ s->c.seq;
    s->teslim++;
}

static void k101_parca(k101_akis_t *s, const uint8_t *v, size_t n, size_t parca)
{
    if (s->konum >= n) {
        return;
    }
    if (parca > n - s->konum) {
        parca = n - s->konum;
    }
    size_t tk = 0;
    const ida_link_rc_t rc = ida_link_al(&s->a, &v[s->konum], parca, &tk, &s->c);
    s->konum += tk;
    if (rc == IDA_LINK_CERCEVE) {
        k101_ozetle(s);
    }
}

static size_t k101_akis_kur(uint8_t *o, size_t kap, int rol_sim, uint32_t tohum)
{
    size_t n = 0;
    prng_durum = tohum;
    for (unsigned k = 0; k < 60u && n + 300u < kap; ++k) {
        const unsigned cop = prng() % 8u;
        for (unsigned i = 0; i < cop; ++i) {
            o[n++] = (uint8_t)prng();
        }
        const ida_link_altin_t *g;
        do {
            g = &ida_link_altin[prng() % IDA_LINK_ALTIN_SAYISI];
        } while ((g->sysid == IDA_LINK_SIM_SYSID) == (rol_sim != 0));
        n += cerceve_kur(&o[n], (uint8_t)g->yuk, 0u, (uint8_t)prng(), g->sysid, g->compid,
                         g->msgid, &g->bayt[10], g->crc_extra, (prng() % 5u) == 0u);
    }
    return n;
}

static void test_k101_iki_ornek_karisik_besleme(void)
{
    static uint8_t akis1[8192];
    static uint8_t akis2[8192];
    static k101_akis_t ayri1, ayri2, karisik1, karisik2;
    const size_t n1 = k101_akis_kur(akis1, sizeof akis1, 0, 0xA1u);  /* kart rolüne */
    const size_t n2 = k101_akis_kur(akis2, sizeof akis2, 1, 0xB2u);  /* sim rolüne */

    memset(&ayri1, 0, sizeof ayri1);
    memset(&ayri2, 0, sizeof ayri2);
    memset(&karisik1, 0, sizeof karisik1);
    memset(&karisik2, 0, sizeof karisik2);
    TEST_ASSERT_EQUAL_INT(IDA_LINK_TAMAM, ida_link_alici_kur(&ayri1.a, IDA_LINK_ROL_KART));
    TEST_ASSERT_EQUAL_INT(IDA_LINK_TAMAM, ida_link_alici_kur(&ayri2.a, IDA_LINK_ROL_SIM));
    TEST_ASSERT_EQUAL_INT(IDA_LINK_TAMAM, ida_link_alici_kur(&karisik1.a, IDA_LINK_ROL_KART));
    TEST_ASSERT_EQUAL_INT(IDA_LINK_TAMAM, ida_link_alici_kur(&karisik2.a, IDA_LINK_ROL_SIM));

    /* Ayrı ayrı: her akış tek başına, büyük parçalarla. */
    while (ayri1.konum < n1) {
        k101_parca(&ayri1, akis1, n1, 4096u);
    }
    while (ayri2.konum < n2) {
        k101_parca(&ayri2, akis2, n2, 4096u);
    }
    /* Karışık: iki örnek sırayla, küçük ve farklı boylu parçalarla. */
    prng_durum = 0xC3u;
    while (karisik1.konum < n1 || karisik2.konum < n2) {
        k101_parca(&karisik1, akis1, n1, 1u + (size_t)(prng() % 7u));
        k101_parca(&karisik2, akis2, n2, 1u + (size_t)(prng() % 11u));
    }

    TEST_ASSERT_TRUE(ayri1.teslim > 10u && ayri2.teslim > 10u);
    TEST_ASSERT_EQUAL_UINT(ayri1.teslim, karisik1.teslim);
    TEST_ASSERT_EQUAL_UINT(ayri2.teslim, karisik2.teslim);
    TEST_ASSERT_EQUAL_HEX32(ayri1.imza, karisik1.imza);
    TEST_ASSERT_EQUAL_HEX32(ayri2.imza, karisik2.imza);
    sayaclar_esit(&ayri1.a.sayac, &karisik1.a.sayac);
    sayaclar_esit(&ayri2.a.sayac, &karisik2.a.sayac);
}

/* ------------------------------------------------------------------ */
/* Arayüz: parça parça besleme, taşma, hata yolları                   */
/* ------------------------------------------------------------------ */

static void test_tek_bayt_besleme_ayni_sonuc(void)
{
    static uint8_t h[2048];
    size_t n = 0;
    for (unsigned i = 0; i < IDA_LINK_ALTIN_SAYISI; ++i) {
        const ida_link_altin_t *g = &ida_link_altin[i];
        if (g->sysid == IDA_LINK_SIM_SYSID) {
            TEST_ASSERT_TRUE(n + g->cerceve <= sizeof h);
            memcpy(&h[n], g->bayt, g->cerceve);
            n += g->cerceve;
        }
    }
    static ida_link_alici_t a1, a2;
    teslimler_t t1 = {{0}, {0}, 0};
    teslimler_t t2 = {{0}, {0}, 0};
    TEST_ASSERT_EQUAL_INT(IDA_LINK_TAMAM, ida_link_alici_kur(&a1, IDA_LINK_ROL_KART));
    TEST_ASSERT_EQUAL_INT(IDA_LINK_TAMAM, ida_link_alici_kur(&a2, IDA_LINK_ROL_KART));
    hepsini_besle(&a1, h, n, &t1);
    for (size_t i = 0; i < n; ++i) {
        hepsini_besle(&a2, &h[i], 1u, &t2);
    }
    TEST_ASSERT_EQUAL_UINT(26u, t1.n);
    TEST_ASSERT_EQUAL_UINT(t1.n, t2.n);
    TEST_ASSERT_EQUAL_UINT32_ARRAY(t1.msgid, t2.msgid, t1.n);
    sayaclar_esit(&a1.sayac, &a2.sayac);
    /* Altın akışta sim kimlikli çerçevelerin seq'leri 0, 2..25 ve 37: iki boşluk. */
    TEST_ASSERT_EQUAL_UINT32(2u, a1.sayac.rx_seq_gap);
}

static void test_tasma_yarim_adayi_atar(void)
{
    const ida_link_altin_t *g = altin_bul(MAVLINK_MSG_ID_IDA_IMU);
    static ida_link_alici_t a;
    static ida_link_cerceve_t c;
    size_t tk = 0;
    TEST_ASSERT_EQUAL_INT(IDA_LINK_TAMAM, ida_link_alici_kur(&a, IDA_LINK_ROL_KART));
    TEST_ASSERT_EQUAL_INT(IDA_LINK_BEKLE, ida_link_al(&a, g->bayt, 30u, &tk, &c));
    TEST_ASSERT_EQUAL_INT(IDA_LINK_TAMAM, ida_link_alici_tasma(&a));
    TEST_ASSERT_EQUAL_UINT32(1u, a.sayac.rx_overflow);
    /* Yeniden eşleme: tam çerçeve baştan kabul edilir. */
    TEST_ASSERT_EQUAL_INT(IDA_LINK_CERCEVE, ida_link_al(&a, g->bayt, g->cerceve, &tk, &c));
    TEST_ASSERT_EQUAL_UINT32(0u, a.sayac.rx_crc_err);
}

static void test_bos_besleme_ve_null(void)
{
    static ida_link_alici_t a;
    static ida_link_cerceve_t c;
    size_t tk = 77u;
    TEST_ASSERT_EQUAL_INT(IDA_LINK_TAMAM, ida_link_alici_kur(&a, IDA_LINK_ROL_KART));
    TEST_ASSERT_EQUAL_INT(IDA_LINK_BEKLE, ida_link_al(&a, NULL, 0u, &tk, &c));
    TEST_ASSERT_EQUAL_size_t(0u, tk);
    const uint8_t b = 0x00u;
    TEST_ASSERT_EQUAL_INT(IDA_LINK_HATA_ARGUMAN, ida_link_al(&a, NULL, 1u, &tk, &c));
    TEST_ASSERT_EQUAL_INT(IDA_LINK_HATA_ARGUMAN, ida_link_al(NULL, &b, 1u, &tk, &c));
    TEST_ASSERT_EQUAL_INT(IDA_LINK_HATA_ARGUMAN, ida_link_al(&a, &b, 1u, NULL, &c));
    TEST_ASSERT_EQUAL_INT(IDA_LINK_HATA_ARGUMAN, ida_link_al(&a, &b, 1u, &tk, NULL));
    TEST_ASSERT_EQUAL_UINT32(0u, a.sayac.rx_bytes);
    TEST_ASSERT_EQUAL_INT(IDA_LINK_HATA_ARGUMAN, ida_link_alici_kur(NULL, IDA_LINK_ROL_KART));
    TEST_ASSERT_EQUAL_INT(IDA_LINK_HATA_ARGUMAN, ida_link_alici_tasma(NULL));
    TEST_ASSERT_EQUAL_INT(IDA_LINK_HATA_ARGUMAN, ida_link_gonderici_kur(NULL, IDA_LINK_ROL_KART));
    TEST_ASSERT_EQUAL_INT(IDA_LINK_HATA_ARGUMAN, ida_link_mesaj_bilgisi(46000u, NULL));
    TEST_ASSERT_EQUAL_INT(IDA_LINK_HATA_ARGUMAN, ida_link_coz(NULL, 46000u, &a, 1u));
}

static void test_gecersiz_rol(void)
{
    static ida_link_alici_t a;
    static ida_link_cerceve_t c;
    ida_link_gonderici_t g;
    size_t tk = 0;
    const uint8_t b = 0xFDu;
    TEST_ASSERT_EQUAL_INT(IDA_LINK_HATA_ROL, ida_link_alici_kur(&a, (ida_link_rol_t)0));
    TEST_ASSERT_EQUAL_INT(IDA_LINK_HATA_ROL, ida_link_al(&a, &b, 1u, &tk, &c));
    TEST_ASSERT_EQUAL_INT(IDA_LINK_HATA_ROL, ida_link_alici_tasma(&a));
    TEST_ASSERT_EQUAL_INT(IDA_LINK_HATA_ROL, ida_link_gonderici_kur(&g, (ida_link_rol_t)7));
    mavlink_ida_keepalive_t k;
    memset(&k, 0, sizeof k);
    uint8_t o[32];
    size_t yz = 5u;
    TEST_ASSERT_EQUAL_INT(IDA_LINK_HATA_ROL, ida_link_gonder(&g, MAVLINK_MSG_ID_IDA_KEEPALIVE, &k,
                                                             sizeof k, o, sizeof o, &yz));
    TEST_ASSERT_EQUAL_size_t(0u, yz);
}

static void test_gonderici_hata_yollari(void)
{
    mavlink_ida_ping360_data_t p;
    memset(&p, 0, sizeof p);
    p.data[239] = 1u;  /* kırpılmaz: 246 B yük, 258 B çerçeve */
    static uint8_t o[IDA_LINK_CERCEVE_AZAMI];
    size_t yz = 0;
    ida_link_gonderici_t g;
    TEST_ASSERT_EQUAL_INT(IDA_LINK_TAMAM, ida_link_gonderici_kur(&g, IDA_LINK_ROL_SIM));

    /* Tampon bir bayt eksik: hiçbir bayt yazılmaz, seq değişmez. */
    memset(o, 0xEE, sizeof o);
    TEST_ASSERT_EQUAL_INT(IDA_LINK_HATA_TAMPON,
                          ida_link_gonder(&g, MAVLINK_MSG_ID_IDA_PING360_DATA, &p, sizeof p, o,
                                          257u, &yz));
    TEST_ASSERT_EACH_EQUAL_HEX8(0xEEu, o, sizeof o);
    TEST_ASSERT_EQUAL_size_t(0u, yz);
    TEST_ASSERT_EQUAL_UINT8(0u, g.seq);
    /* Tam yetiyor. */
    TEST_ASSERT_EQUAL_INT(IDA_LINK_TAMAM,
                          ida_link_gonder(&g, MAVLINK_MSG_ID_IDA_PING360_DATA, &p, sizeof p, o,
                                          258u, &yz));
    TEST_ASSERT_EQUAL_size_t(258u, yz);
    TEST_ASSERT_EQUAL_UINT8(1u, g.seq);
    /* Sıfır kapasite. */
    TEST_ASSERT_EQUAL_INT(IDA_LINK_HATA_TAMPON,
                          ida_link_gonder(&g, MAVLINK_MSG_ID_IDA_PING360_DATA, &p, sizeof p, o,
                                          0u, &yz));
    /* Yanlış yapı boyu, bilinmeyen kimlik, NULL. */
    TEST_ASSERT_EQUAL_INT(IDA_LINK_HATA_BOY,
                          ida_link_gonder(&g, MAVLINK_MSG_ID_IDA_PING360_DATA, &p, sizeof p - 1u,
                                          o, sizeof o, &yz));
    TEST_ASSERT_EQUAL_INT(IDA_LINK_HATA_KIMLIK,
                          ida_link_gonder(&g, 46099u, &p, sizeof p, o, sizeof o, &yz));
    TEST_ASSERT_EQUAL_INT(IDA_LINK_HATA_ARGUMAN,
                          ida_link_gonder(&g, MAVLINK_MSG_ID_IDA_PING360_DATA, NULL, sizeof p, o,
                                          sizeof o, &yz));
    TEST_ASSERT_EQUAL_INT(IDA_LINK_HATA_ARGUMAN,
                          ida_link_gonder(&g, MAVLINK_MSG_ID_IDA_PING360_DATA, &p, sizeof p, NULL,
                                          sizeof o, &yz));
    TEST_ASSERT_EQUAL_INT(IDA_LINK_HATA_ARGUMAN,
                          ida_link_gonder(&g, MAVLINK_MSG_ID_IDA_PING360_DATA, &p, sizeof p, o,
                                          sizeof o, NULL));
    TEST_ASSERT_EQUAL_UINT8(1u, g.seq);
}

static void test_coz_hata_yollari(void)
{
    const ida_link_altin_t *g = altin_bul(MAVLINK_MSG_ID_IDA_IMU);
    static ida_link_alici_t a;
    static ida_link_cerceve_t c;
    size_t tk = 0;
    TEST_ASSERT_EQUAL_INT(IDA_LINK_TAMAM, ida_link_alici_kur(&a, IDA_LINK_ROL_KART));
    TEST_ASSERT_EQUAL_INT(IDA_LINK_CERCEVE, ida_link_al(&a, g->bayt, g->cerceve, &tk, &c));
    mavlink_ida_imu_t imu;
    mavlink_ida_mag_t mag;
    memset(&mag, 0x77, sizeof mag);
    TEST_ASSERT_EQUAL_INT(IDA_LINK_HATA_KIMLIK,
                          ida_link_coz(&c, MAVLINK_MSG_ID_IDA_MAG, &mag, sizeof mag));
    TEST_ASSERT_EACH_EQUAL_HEX8(0x77u, &mag, sizeof mag);
    TEST_ASSERT_EQUAL_INT(IDA_LINK_HATA_BOY,
                          ida_link_coz(&c, MAVLINK_MSG_ID_IDA_IMU, &imu, sizeof imu + 1u));
    TEST_ASSERT_EQUAL_INT(IDA_LINK_HATA_KIMLIK, ida_link_coz(&c, 1u, &imu, sizeof imu));
    TEST_ASSERT_EQUAL_INT(IDA_LINK_TAMAM, ida_link_coz(&c, MAVLINK_MSG_ID_IDA_IMU, &imu, sizeof imu));
    TEST_ASSERT_EQUAL_UINT32(100011u, imu.seq);  /* Ek B 46020 */
}

static void test_sayaclar_sarar(void)
{
    /* A1 §3.11: kart sayaçları 32 bittir ve 2^32'de sarar. */
    static ida_link_alici_t a;
    static ida_link_cerceve_t c;
    size_t tk = 0;
    const uint8_t b = 0x00u;
    TEST_ASSERT_EQUAL_INT(IDA_LINK_TAMAM, ida_link_alici_kur(&a, IDA_LINK_ROL_KART));
    a.sayac.rx_bytes = UINT32_MAX;
    a.sayac.rx_resync_bytes = UINT32_MAX;
    TEST_ASSERT_EQUAL_INT(IDA_LINK_BEKLE, ida_link_al(&a, &b, 1u, &tk, &c));
    TEST_ASSERT_EQUAL_UINT32(0u, a.sayac.rx_bytes);
    TEST_ASSERT_EQUAL_UINT32(0u, a.sayac.rx_resync_bytes);
}

static void test_mesaj_bilgisi_tablo(void)
{
    /* 37 mesajın her biri tabloda; yön sütunu A1 Tablo 3.3 ile (örnekler). */
    unsigned n = 0;
    for (uint32_t id = 46000u; id <= 46099u; ++id) {
        ida_link_mesaj_bilgisi_t b;
        if (ida_link_mesaj_bilgisi(id, &b) == IDA_LINK_TAMAM) {
            TEST_ASSERT_EQUAL_UINT32(id, b.msgid);
            TEST_ASSERT_TRUE(b.yapi_boyu >= b.yuk_azami);
            n++;
        }
    }
    TEST_ASSERT_EQUAL_UINT(IDA_LINK_DIALECT_MESAJ_SATIRI, n);
    ida_link_mesaj_bilgisi_t b;
    TEST_ASSERT_EQUAL_INT(IDA_LINK_TAMAM, ida_link_mesaj_bilgisi(MAVLINK_MSG_ID_IDA_SIM_HELLO, &b));
    TEST_ASSERT_EQUAL_UINT8(IDA_LINK_YON_SIM_KART, b.yon);
    TEST_ASSERT_EQUAL_INT(IDA_LINK_TAMAM, ida_link_mesaj_bilgisi(MAVLINK_MSG_ID_IDA_BOARD_HELLO, &b));
    TEST_ASSERT_EQUAL_UINT8(IDA_LINK_YON_KART_SIM, b.yon);
    TEST_ASSERT_EQUAL_INT(IDA_LINK_HATA_KIMLIK, ida_link_mesaj_bilgisi(46005u, &b));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_crc_denetim_degeri);
    RUN_TEST(test_altin_38_cerceve);
    RUN_TEST(test_altin_tipli_alanlar);
    RUN_TEST(test_k02_incompat_flags_reddedilir);
    RUN_TEST(test_k02_compat_flags_yok_sayilir);
    RUN_TEST(test_k02_mavlink1_atilir);
    RUN_TEST(test_k02_gonderici_bayraklari_sifir);
    RUN_TEST(test_k04_yabanci_sysid_compid);
    RUN_TEST(test_k04_sim_rolu_yalniz_karti_kabul_eder);
    RUN_TEST(test_k04_gonderici_kimligi_role_gore);
    RUN_TEST(test_k05_len_sifir_reddedilir);
    RUN_TEST(test_k05_len_azamiyi_asan_reddedilir);
    RUN_TEST(test_k05_len_sinirlari_kabul);
    RUN_TEST(test_k05_kirpilmis_yuk_sifirla_doldurulur);
    RUN_TEST(test_k05_gonderici_kirpar_ilk_bayt_kalir);
    RUN_TEST(test_k06_dialect_disi_kimlik);
    RUN_TEST(test_k06_yonu_yanlis_kimlik);
    RUN_TEST(test_k06_iki_yonlu_mesajlar_iki_rolde);
    RUN_TEST(test_k06_gonderici_yonu_reddeder);
    RUN_TEST(test_k07_crc_hatasi_aday_tuketilir);
    RUN_TEST(test_k07_cerceve_disi_baytlar_sayilir);
    RUN_TEST(test_k07_reddedilen_baslik_sonrasi_cerceve_kacirilmaz);
    RUN_TEST(test_k07_buyutulmus_len_bolgesi);
    RUN_TEST(test_k07_sozde_rastgele_akis);
    RUN_TEST(test_k08_seq_boslugu_sayilir);
    RUN_TEST(test_k08_reddedilen_cerceve_seq_gecmisini_degistirmez);
    RUN_TEST(test_k08_gonderici_seq_artar_ve_sarar);
    RUN_TEST(test_k101_iki_ornek_karisik_besleme);
    RUN_TEST(test_tek_bayt_besleme_ayni_sonuc);
    RUN_TEST(test_tasma_yarim_adayi_atar);
    RUN_TEST(test_bos_besleme_ve_null);
    RUN_TEST(test_gecersiz_rol);
    RUN_TEST(test_gonderici_hata_yollari);
    RUN_TEST(test_coz_hata_yollari);
    RUN_TEST(test_sayaclar_sarar);
    RUN_TEST(test_mesaj_bilgisi_tablo);
    return UNITY_END();
}
