/*
 * A1-K08 kabul testleri (sözleşme §4.1, §7.1 K08 satırı):
 *   "Her gönderen çerçeve başlığındaki seq'i oturumdan bağımsız olarak her çerçevede bir
 *    artırır (mod 256); alıcı ardışık iki geçerli çerçeve arasındaki boşlukları rx_seq_gap'e
 *    sayar ve bu sayaca hiçbir davranış bağlamaz."
 * Öğeler:
 *   (a) gönderen her çerçevede seq'i bir artırır, mod 256 (600 çerçeve, iki rol)
 *   (b) oturumdan bağımsız: farklı sess taşıyan çerçeveler arasında seq sürer
 *   (c) çerçeve üretmeyen (hatalı) gönderim seq tüketmez: üretilen çerçevelerin seq'i ardışık
 *   (d) alıcı: ardışık seq'lerde (255 -> 0 dahil) boşluk sayılmaz
 *   (e) alıcı: tek çerçevelik her boşluk rx_seq_gap'i bir artırır (sarma dahil)
 *   (f) ilk geçerli çerçevenin seq'i ne olursa olsun boşluk sayılmaz (öncesi yok)
 *   (g) sağlaması tutmayan (geçerli olmayan) ara çerçeve boşluk yaratır
 *   (h) davranış değişmez: boşluklu akışta da bütün çerçeveler aynı yükle teslim edilir
 * Birden çok çerçevelik boşluk (olay mı kayıp çerçeve mi sayılır), yinelenen ve geri giden
 * seq, ve kimlik ya da yön yüzünden reddedilen ara çerçevenin "geçerli" sayılıp sayılmadığı
 * A1'de açık değildir: kabul_belirsiz.c.
 */
#include <stdio.h>
#include <string.h>

#include "kabul_ortak.h"
#include "unity.h"

void setUp(void) {}
void tearDown(void) {}

static uint8_t akis[65536];
static kabul_sonuc_t sonuc;
static kabul_sonuc_t sonuc2;

static const ida_link_rol_t ROLLER[2] = {IDA_LINK_ROL_KART, IDA_LINK_ROL_SIM};

static size_t gonder(ida_link_gonderici_t *g, const kabul_mesaj_t *m, const uint8_t *yuk,
                     uint8_t *cikis, size_t kapasite, ida_link_rc_t beklenen)
{
    kabul_yapi_t y;
    kabul_yapi_hazirla(&y, m, yuk);
    size_t yazilan = 77u;
    ida_link_rc_t rc = ida_link_gonder(g, m->msgid, y.b, m->yapi_boyu, cikis, kapasite, &yazilan);
    TEST_ASSERT_EQUAL_INT_MESSAGE(beklenen, rc, m->ad);
    return rc == IDA_LINK_TAMAM ? yazilan : 0u;
}

/* (a), (b): 600 çerçeve, rolün gönderdiği mesajlar arasında dolaşarak; iki yönlü oturum
   mesajları (KEEPALIVE, SESSION_CLOSE) farklı sess değerleriyle araya girer. Her çerçevenin
   seq'i bir öncekinin bir fazlasıdır (mod 256). */
static void test_A1_K08_ab_gonderen_her_cercevede_bir_artirir(void)
{
    kabul_rng_t r;
    kabul_rng_kur(&r, 0x4B30380001ull);
    for (size_t ri = 0; ri < 2; ++ri) {
        ida_link_rol_t rol = ROLLER[ri];
        ida_link_gonderici_t g;
        TEST_ASSERT_EQUAL_INT(IDA_LINK_TAMAM, ida_link_gonderici_kur(&g, rol));
        int onceki = -1;
        size_t mi = 0;
        for (int i = 0; i < 600; ++i) {
            const kabul_mesaj_t *m;
            do {
                m = &kabul_mesajlar[mi++ % KABUL_MESAJ_SAYISI];
            } while ((m->yon & kabul_rol_gonderir_yon(rol)) == 0u);
            uint8_t yuk[IDA_LINK_YUK_AZAMI] = {0};
            for (size_t k = 0; k < m->yuk_azami; ++k) {
                yuk[k] = kabul_rng_bayt(&r);
            }
            if (m->msgid == 46003u || m->msgid == 46004u) {
                /* sess (uint16, yükün ilk iki baytı) her seferinde farklı. */
                yuk[0] = (uint8_t)i;
                yuk[1] = (uint8_t)(i >> 8);
            }
            uint8_t cikis[IDA_LINK_CERCEVE_AZAMI];
            size_t n = gonder(&g, m, yuk, cikis, sizeof cikis, IDA_LINK_TAMAM);
            kabul_cozulen_t c;
            TEST_ASSERT_EQUAL_INT(0, kabul_cerceve_dogrula(cikis, n, &c));
            if (onceki >= 0) {
                char ileti[64];
                snprintf(ileti, sizeof ileti, "çerçeve %d (%s)", i, m->ad);
                TEST_ASSERT_EQUAL_UINT8_MESSAGE((uint8_t)(onceki + 1), c.seq, ileti);
            }
            onceki = c.seq;
        }
    }
}

/* Açık başlık: kurulumda seq = 0 ve çağıran seq'i yazabilir; yazılan değerden devam edilir
   (250 -> 255 -> 0 -> 3). */
static void test_A1_K08_baslik_kurulumda_sifir_ve_yazilabilir(void)
{
    ida_link_gonderici_t g;
    TEST_ASSERT_EQUAL_INT(IDA_LINK_TAMAM, ida_link_gonderici_kur(&g, IDA_LINK_ROL_SIM));
    TEST_ASSERT_EQUAL_UINT8(0u, g.seq);
    const kabul_mesaj_t *m = kabul_mesaj_bul(46003u);
    uint8_t yuk[3] = {1u, 0u, 2u};
    uint8_t cikis[IDA_LINK_CERCEVE_AZAMI];
    size_t n = gonder(&g, m, yuk, cikis, sizeof cikis, IDA_LINK_TAMAM);
    TEST_ASSERT_TRUE(n > 4u);
    TEST_ASSERT_EQUAL_UINT8(0u, cikis[4]);
    g.seq = 250u;
    for (unsigned i = 0; i < 10u; ++i) {
        n = gonder(&g, m, yuk, cikis, sizeof cikis, IDA_LINK_TAMAM);
        TEST_ASSERT_EQUAL_UINT8((uint8_t)(250u + i), cikis[4]);
    }
    TEST_ASSERT_EQUAL_UINT8(4u, g.seq);
}

/* (c): yetersiz tampon (gereken − 1, 0), bu rolün göndermediği mesaj ve dialect dışı kimlik
   çerçeve üretmez; aradan sonra üretilen çerçevenin seq'i bir öncekinin bir fazlasıdır ve
   çıkış tamponuna hiçbir bayt yazılmaz (açık başlık). */
static void test_A1_K08_c_hatali_gonderim_seq_tuketmez(void)
{
    ida_link_gonderici_t g;
    TEST_ASSERT_EQUAL_INT(IDA_LINK_TAMAM, ida_link_gonderici_kur(&g, IDA_LINK_ROL_KART));
    const kabul_mesaj_t *m = kabul_mesaj_bul(46063u); /* IDA_NAV, kart -> sim */
    uint8_t yuk[IDA_LINK_YUK_AZAMI];
    memset(yuk, 0x11, sizeof yuk);
    uint8_t cikis[IDA_LINK_CERCEVE_AZAMI + 8u];
    size_t n = gonder(&g, m, yuk, cikis, sizeof cikis, IDA_LINK_TAMAM);
    uint8_t ilk = cikis[4];
    TEST_ASSERT_EQUAL_UINT32(12u + m->yuk_azami, (uint32_t)n);

    uint8_t bekci[IDA_LINK_CERCEVE_AZAMI + 8u];
    memset(cikis, 0xC3, sizeof cikis);
    memcpy(bekci, cikis, sizeof bekci);
    gonder(&g, m, yuk, cikis, n - 1u, IDA_LINK_HATA_TAMPON);
    gonder(&g, m, yuk, cikis, 0u, IDA_LINK_HATA_TAMPON);
    TEST_ASSERT_EQUAL_HEX8_ARRAY_MESSAGE(bekci, cikis, sizeof cikis, "hatalı gönderim tampona yazdı");
    /* Kartın göndermediği sim -> kart mesajı. */
    gonder(&g, kabul_mesaj_bul(46020u), yuk, cikis, sizeof cikis, IDA_LINK_HATA_YON);
    TEST_ASSERT_EQUAL_HEX8_ARRAY_MESSAGE(bekci, cikis, sizeof cikis, "HATA_YON tampona yazdı");
    /* Dialect dışı kimlik (yapı boyu olarak NAV'ınki verilir). */
    {
        kabul_yapi_t y;
        kabul_yapi_hazirla(&y, m, yuk);
        size_t yazilan = 5u;
        TEST_ASSERT_EQUAL_INT(IDA_LINK_HATA_KIMLIK,
                              ida_link_gonder(&g, 46005u, y.b, m->yapi_boyu, cikis, sizeof cikis, &yazilan));
        TEST_ASSERT_EQUAL_UINT32(0u, (uint32_t)yazilan);
        TEST_ASSERT_EQUAL_INT(IDA_LINK_HATA_BOY,
                              ida_link_gonder(&g, m->msgid, y.b, m->yapi_boyu + 1u, cikis, sizeof cikis, &yazilan));
        TEST_ASSERT_EQUAL_UINT32(0u, (uint32_t)yazilan);
    }
    TEST_ASSERT_EQUAL_HEX8_ARRAY_MESSAGE(bekci, cikis, sizeof cikis, "hatalı gönderim tampona yazdı");
    /* Tam yeten tampon: kabul. */
    n = gonder(&g, m, yuk, cikis, 12u + m->yuk_azami, IDA_LINK_TAMAM);
    TEST_ASSERT_EQUAL_UINT32(12u + m->yuk_azami, (uint32_t)n);
    TEST_ASSERT_EQUAL_UINT8((uint8_t)(ilk + 1u), cikis[4]);
}

/* Alıcı akışı: verilen seq dizisiyle geçerli çerçeveler (her biri farklı yük). seq 0xFD ya
   da 0xFE olabilir: bütün çerçeveler geçerli ya da yalnız sağlaması bozuk olduğundan (A1-K07:
   aday bütünüyle tüketilir) iç baytlar sonucu etkilemez. */
static size_t seq_akisi(uint8_t *s, kabul_rng_t *r, ida_link_rol_t rol, const uint8_t *seq,
                        size_t sayi, const uint8_t *bozuk)
{
    size_t n = 0;
    uint32_t id = rol == IDA_LINK_ROL_KART ? 46026u : 46065u;
    for (size_t i = 0; i < sayi; ++i) {
        size_t bas = n;
        uint8_t yuk[IDA_LINK_YUK_AZAMI];
        uint8_t gecici[IDA_LINK_CERCEVE_AZAMI];
        (void)kabul_temiz_kur(gecici, r, id, 0u, kabul_karsi_sysid(rol), kabul_karsi_compid(rol),
                              6u, 1, yuk);
        n += kabul_cerceve_kur(&s[n], id, seq[i], kabul_karsi_sysid(rol), kabul_karsi_compid(rol),
                               yuk, sizeof yuk, 1);
        if (bozuk != NULL && bozuk[i]) {
            uint8_t v = s[bas + 10u];
            s[bas + 10u] = (uint8_t)(v == 0x01u ? 0x02u : 0x01u);
        }
    }
    return n;
}

static void seq_dene(const uint8_t *seq, size_t sayi, const uint8_t *bozuk, uint32_t beklenen_bosluk,
                     const char *ad)
{
    kabul_rng_t r;
    kabul_rng_kur(&r, 0x4B30380010ull);
    for (size_t ri = 0; ri < 2; ++ri) {
        ida_link_rol_t rol = ROLLER[ri];
        size_t n = seq_akisi(akis, &r, rol, seq, sayi, bozuk);
        kabul_besle_yeni(rol, akis, n, &sonuc);
        size_t gecerli = 0;
        for (size_t i = 0; i < sayi; ++i) {
            gecerli += (bozuk == NULL || !bozuk[i]) ? 1u : 0u;
        }
        TEST_ASSERT_EQUAL_UINT32_MESSAGE((uint32_t)gecerli, (uint32_t)sonuc.sayi, ad);
        TEST_ASSERT_EQUAL_UINT32_MESSAGE(beklenen_bosluk, sonuc.sayac.rx_seq_gap, ad);
    }
}

/* (d): ardışık seq'ler, 250'den başlayıp 255 -> 0'dan geçerek: boşluk yok. */
static void test_A1_K08_d_ardisik_seq_bosluk_yok(void)
{
    uint8_t seq[300];
    for (size_t i = 0; i < 300; ++i) {
        seq[i] = (uint8_t)(250u + i);
    }
    seq_dene(seq, 300, NULL, 0u, "ardışık (sarma dahil)");
}

/* (e): tek çerçevelik boşluklar: 0,1,3 (2 yok); 254 -> 0 (255 yok); 255 -> 1 (0 yok);
   ve 10 ayrı tek boşluk. Her biri tam bir artış (olay ve kayıp çerçeve okuması aynı sonucu
   verir). */
static void test_A1_K08_e_tek_bosluk_bir_artis(void)
{
    static const uint8_t A[] = {0u, 1u, 3u, 4u};
    seq_dene(A, sizeof A, NULL, 1u, "0,1,3,4");
    static const uint8_t B[] = {252u, 253u, 254u, 0u, 1u};
    seq_dene(B, sizeof B, NULL, 1u, "254 -> 0");
    static const uint8_t C[] = {254u, 255u, 1u, 2u};
    seq_dene(C, sizeof C, NULL, 1u, "255 -> 1");
    uint8_t D[60];
    uint8_t s = 7u;
    for (size_t i = 0; i < 60; ++i) {
        D[i] = s;
        s = (uint8_t)(s + ((i % 6u) == 5u ? 2u : 1u));
    }
    seq_dene(D, sizeof D, NULL, 9u, "her altı çerçevede bir tek boşluk");
}

/* (f): ilk geçerli çerçevenin seq'i rastgele: boşluk sayılmaz. */
static void test_A1_K08_f_ilk_cerceve_bosluk_degil(void)
{
    static const uint8_t ILK[] = {0u, 1u, 77u, 128u, 200u, 255u};
    for (size_t i = 0; i < sizeof ILK; ++i) {
        uint8_t seq[3] = {ILK[i], (uint8_t)(ILK[i] + 1u), (uint8_t)(ILK[i] + 2u)};
        seq_dene(seq, 3, NULL, 0u, "ilk çerçeve");
        seq_dene(seq, 1, NULL, 0u, "tek çerçeve");
    }
}

/* (g): sağlaması tutmayan ara çerçeve geçerli değildir: 5, [6 bozuk], 7 -> bir boşluk;
   bozukların her biri tek boşluk yaratacak biçimde aralıklı. */
static void test_A1_K08_g_bozuk_ara_cerceve_bosluk_yaratir(void)
{
    static const uint8_t SEQ[] = {5u, 6u, 7u, 8u, 9u, 10u, 11u, 12u};
    static const uint8_t BOZUK[] = {0u, 1u, 0u, 0u, 1u, 0u, 1u, 0u};
    seq_dene(SEQ, sizeof SEQ, BOZUK, 3u, "bozuk ara çerçeveler");
}

/* (h): aynı çerçeveler ardışık seq'le ve boşluklu seq'le: teslim edilen yükler, kimlikler ve
   len aynıdır (yalnız seq ve rx_seq_gap farklı). */
static void test_A1_K08_h_bosluk_davranisi_degistirmez(void)
{
    uint8_t s1[40];
    uint8_t s2[40];
    for (size_t i = 0; i < 40; ++i) {
        s1[i] = (uint8_t)i;
        s2[i] = (uint8_t)(i * 2u + (i % 3u)); /* çoğunlukla boşluklu */
    }
    for (size_t ri = 0; ri < 2; ++ri) {
        ida_link_rol_t rol = ROLLER[ri];
        kabul_rng_t r1;
        kabul_rng_t r2;
        kabul_rng_kur(&r1, 0x4B30380020ull);
        kabul_rng_kur(&r2, 0x4B30380020ull);
        size_t n1 = seq_akisi(akis, &r1, rol, s1, 40, NULL);
        kabul_besle_yeni(rol, akis, n1, &sonuc);
        size_t n2 = seq_akisi(akis, &r2, rol, s2, 40, NULL);
        kabul_besle_yeni(rol, akis, n2, &sonuc2);
        TEST_ASSERT_EQUAL_UINT32(40u, (uint32_t)sonuc.sayi);
        TEST_ASSERT_EQUAL_UINT32(40u, (uint32_t)sonuc2.sayi);
        TEST_ASSERT_EQUAL_UINT32(0u, sonuc.sayac.rx_seq_gap);
        TEST_ASSERT_TRUE(sonuc2.sayac.rx_seq_gap > 0u);
        for (size_t i = 0; i < 40; ++i) {
            TEST_ASSERT_EQUAL_UINT32(sonuc.t[i].msgid, sonuc2.t[i].msgid);
            TEST_ASSERT_EQUAL_UINT8(sonuc.t[i].len, sonuc2.t[i].len);
            TEST_ASSERT_EQUAL_HEX8_ARRAY(sonuc.t[i].yuk, sonuc2.t[i].yuk, IDA_LINK_YUK_AZAMI);
            TEST_ASSERT_EQUAL_UINT8(s2[i], sonuc2.t[i].seq);
        }
        TEST_ASSERT_EQUAL_UINT32(0u, kabul_ret_toplami(&sonuc2.sayac));
        TEST_ASSERT_EQUAL_UINT32(sonuc.sayac.rx_resync_bytes, sonuc2.sayac.rx_resync_bytes);
    }
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_A1_K08_ab_gonderen_her_cercevede_bir_artirir);
    RUN_TEST(test_A1_K08_baslik_kurulumda_sifir_ve_yazilabilir);
    RUN_TEST(test_A1_K08_c_hatali_gonderim_seq_tuketmez);
    RUN_TEST(test_A1_K08_d_ardisik_seq_bosluk_yok);
    RUN_TEST(test_A1_K08_e_tek_bosluk_bir_artis);
    RUN_TEST(test_A1_K08_f_ilk_cerceve_bosluk_degil);
    RUN_TEST(test_A1_K08_g_bozuk_ara_cerceve_bosluk_yaratir);
    RUN_TEST(test_A1_K08_h_bosluk_davranisi_degistirmez);
    return UNITY_END();
}
