/*
 * A1-K07 kabul testleri (sözleşme §4.1 doğrulama turu metni, §7.1 K07 satırı):
 *   "Alıcının denetim sırası şudur: başlıkta incompat_flags (A1-K02) → kimliğin dialect'te
 *    olması (A1-K06) → len (A1-K05); ardından sağlama (rx_crc_err); sağlama tuttuktan sonra
 *    sysid/compid (A1-K04) → yön (A1-K06); ilk tutmayan denetim tek ret nedenidir ... Başlık
 *    denetiminde reddedilen adayda ayrıştırma adayın 0xFD'sinden sonraki bayttan, 0xFD
 *    arayarak sürer (adayın arkasındaki geçerli çerçeve kaçırılmaz); sağlaması tutmayan adayda
 *    ise adayın tamamı tüketilir ve ayrıştırma aday çerçevenin son baytından sonraki bayttan
 *    sürer; aday başlatmayan ve bir çerçeveye ait olmayan her bayt rx_resync_bytes'a sayılır."
 * Öğeler:
 *   (a) sağlaması tutmayan çerçeve atılır, yalnız rx_crc_err; aday bütünüyle tüketilir, baytları
 *       resync'e girmez (yük, CRC baytları, seq, compat, yanlış CRC_EXTRA, çok bit)
 *   (b) CRC hatasında ayrıştırma adayın son baytından sonra sürer: adayın içine gömülü geçerli
 *       çerçeve teslim edilmez
 *   (c) 0xFD aranır; (d) aday başlatmayan her bayt rx_resync_bytes'a sayılır
 *   (e) §7.1: bozulan adayın kapladığı bölgeden sonraki ilk geçerli çerçeve yakalanır
 *   (f) denetim sırası: her ardışık denetim çifti için iki hatayı birlikte taşıyan ayırıcı
 *       girdi; yalnız ilk tutmayan denetimin sayacı artar (sol incelemesinin örneği dahil)
 *   (g) başlıkta reddedilen adaydan sonra tarama 0xFD'den sonraki bayttan sürer: adayın içine
 *       gömülü ve hemen arkasındaki geçerli çerçeve kaçırılmaz
 * Bütün vakalarda teslimler ve bütün sayaçlar elle yazılmış beklentiyle ve referans modelle
 * (kabul_model_ayristir; A1'in tek okuması) tam eşitlikle karşılaştırılır.
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

static uint32_t ornek_mesaj(ida_link_rol_t rol, size_t sira)
{
    static const uint32_t KART[] = {46020u, 46030u, 46010u, 46003u, 46036u, 46024u};
    static const uint32_t SIM[] = {46063u, 46068u, 46060u, 46004u, 46036u, 46069u};
    return rol == IDA_LINK_ROL_KART ? KART[sira % 6u] : SIM[sira % 6u];
}

typedef enum { BOZ_YUK, BOZ_CRC_DUSUK, BOZ_CRC_YUKSEK, BOZ_SEQ, BOZ_COMPAT, BOZ_CRC_EXTRA,
               BOZ_TUM_BITLER, BOZ_SAYISI } bozma_t;

static const char *const BOZMA_AD[BOZ_SAYISI] = {
    "yük baytı", "CRC düşük baytı", "CRC yüksek baytı", "seq baytı", "compat baytı",
    "yanlış CRC_EXTRA", "yükün her baytında bir bit",
};

/* Temiz geçerli çerçeveyi kurar ve sağlamasını bozar (temizliği korur: bozulan bayt 0xFD ya da
   0xFE olmaz). Dönüş: çerçeve boyu. */
static size_t bozuk_kur(uint8_t *c, kabul_rng_t *r, ida_link_rol_t rol, uint32_t id, uint8_t seq,
                        bozma_t b)
{
    const kabul_mesaj_t *m = kabul_mesaj_al(id);
    size_t n;
    if (b == BOZ_CRC_EXTRA) {
        n = kabul_temiz_ham(c, r, m->yuk_azami, 0u, 0u, seq, kabul_karsi_sysid(rol),
                            kabul_karsi_compid(rol), id, (uint8_t)(m->crc_extra ^ 0x5Au), 0u);
        return n;
    }
    n = kabul_temiz_kur(c, r, id, seq, kabul_karsi_sysid(rol), kabul_karsi_compid(rol),
                        m->yuk_azami, 1, NULL);
    size_t len = n - 12u;
    for (;;) {
        uint8_t *hedef = NULL;
        switch (b) {
        case BOZ_YUK: hedef = &c[10u + kabul_rng_aralik(r, (uint32_t)len)]; break;
        case BOZ_CRC_DUSUK: hedef = &c[10u + len]; break;
        case BOZ_CRC_YUKSEK: hedef = &c[11u + len]; break;
        case BOZ_SEQ: hedef = &c[4]; break;
        case BOZ_COMPAT: hedef = &c[3]; break;
        case BOZ_TUM_BITLER:
        default:
            for (size_t i = 0; i < len; ++i) {
                uint8_t v = (uint8_t)(c[10u + i] ^ (1u << (i % 8u)));
                c[10u + i] = (uint8_t)((v == 0xFDu || v == 0xFEu) ? (c[10u + i] ^ 0x40u) : v);
            }
            return n;
        }
        uint8_t eski = *hedef;
        uint8_t yeni = (uint8_t)(eski ^ (uint8_t)(1u + kabul_rng_aralik(r, 255u)));
        if (yeni != 0xFDu && yeni != 0xFEu && yeni != eski) {
            *hedef = yeni;
            return n;
        }
    }
}

/* (a): her bozma türü, iki rol, birkaç mesaj: bozuk çerçeve atılır (rx_crc_err = 1, teslim
   yok), hemen ardından gelen geçerli çerçeve kabul edilir; başka ret sayacı artmaz. */
static void test_A1_K07_a_sagmasi_tutmayan_atilir(void)
{
    kabul_rng_t r;
    kabul_rng_kur(&r, 0x4B30370001ull);
    for (size_t ri = 0; ri < 2; ++ri) {
        ida_link_rol_t rol = ROLLER[ri];
        for (int b = 0; b < (int)BOZ_SAYISI; ++b) {
            for (size_t mi = 0; mi < 6; ++mi) {
                char ileti[128];
                uint32_t id = ornek_mesaj(rol, mi);
                size_t n = bozuk_kur(akis, &r, rol, id, 70u, (bozma_t)b);
                n += kabul_temiz_kur(&akis[n], &r, ornek_mesaj(rol, mi + 1u), 71u,
                                     kabul_karsi_sysid(rol), kabul_karsi_compid(rol), 5u, 1, NULL);
                kabul_besle_yeni(rol, akis, n, &sonuc);
                snprintf(ileti, sizeof ileti, "rol %d, %s, msgid %lu", (int)rol, BOZMA_AD[b],
                         (unsigned long)id);
                ida_link_sayaclar_t b = kabul_sayac_sifir();
                b.rx_bytes = (uint32_t)n;
                b.rx_frames = 1u;
                b.rx_crc_err = 1u;
                kabul_sayac_esit(&b, &sonuc.sayac, ileti);
                TEST_ASSERT_EQUAL_UINT32_MESSAGE(1u, (uint32_t)sonuc.sayi, ileti);
                TEST_ASSERT_EQUAL_UINT8_MESSAGE(71u, sonuc.t[0].seq, ileti);
                kabul_modelle_esit(rol, akis, n, &sonuc, ileti);
            }
        }
    }
}

/* (b): sağlaması bozuk IDA_PING360_DATA adayının yükünün içine tam bir geçerli KEEPALIVE
   çerçevesi gömülüdür. Ayrıştırma adayın son baytından sonra sürdüğü için gömülü çerçeve
   teslim edilmez; adaydan hemen sonraki çerçeve teslim edilir. Aynı akışta aday bozuk
   değilse (karşı örnek) PING360_DATA teslim edilir, gömülü çerçeve yine edilmez. */
static void test_A1_K07_b_aday_icindeki_cerceve_teslim_edilmez(void)
{
    kabul_rng_t r;
    kabul_rng_kur(&r, 0x4B30370002ull);
    for (int bozuk = 1; bozuk >= 0; --bozuk) {
        const kabul_mesaj_t *m = kabul_mesaj_al(46030u);
        uint8_t ic[32];
        size_t ic_n = kabul_temiz_kur(ic, &r, 46003u, 99u, 250u, 25u, 3u, 1, NULL);
        uint8_t yuk[IDA_LINK_YUK_AZAMI];
        for (int deneme = 0;; ++deneme) {
            TEST_ASSERT_TRUE(deneme < 4096);
            for (size_t i = 0; i < m->yuk_azami; ++i) {
                yuk[i] = (uint8_t)(1u + kabul_rng_aralik(&r, 0xFCu));
            }
            memcpy(&yuk[100], ic, ic_n);
            size_t n = kabul_cerceve_ham(akis, m->yuk_azami, 0u, 0u, 5u, 250u, 25u, m->msgid, yuk,
                                         m->yuk_azami, m->crc_extra);
            if (akis[n - 2u] != 0xFDu && akis[n - 2u] != 0xFEu && akis[n - 1u] != 0xFDu &&
                akis[n - 1u] != 0xFEu) {
                break;
            }
        }
        size_t n = 12u + m->yuk_azami;
        if (bozuk) {
            uint8_t v = akis[n - 1u];
            uint8_t x = 0x01u;
            while ((uint8_t)(v ^ x) == 0xFDu || (uint8_t)(v ^ x) == 0xFEu) {
                x = (uint8_t)(x << 1);
            }
            akis[n - 1u] = (uint8_t)(v ^ x);
        }
        n += kabul_temiz_kur(&akis[n], &r, 46010u, 6u, 250u, 25u, 17u, 1, NULL);
        kabul_besle_yeni(IDA_LINK_ROL_KART, akis, n, &sonuc);
        kabul_modelle_esit(IDA_LINK_ROL_KART, akis, n, &sonuc, "aday içindeki çerçeve");
        TEST_ASSERT_EQUAL_UINT32(0u, sonuc.sayac.rx_resync_bytes);
        if (bozuk) {
            TEST_ASSERT_EQUAL_UINT32_MESSAGE(1u, (uint32_t)sonuc.sayi, "gömülü çerçeve teslim edildi");
            TEST_ASSERT_EQUAL_UINT32(46010u, sonuc.t[0].msgid);
            TEST_ASSERT_EQUAL_UINT32(1u, sonuc.sayac.rx_crc_err);
            TEST_ASSERT_EQUAL_UINT32(1u, kabul_ret_toplami(&sonuc.sayac));
        } else {
            TEST_ASSERT_EQUAL_UINT32(2u, (uint32_t)sonuc.sayi);
            TEST_ASSERT_EQUAL_UINT32(46030u, sonuc.t[0].msgid);
            TEST_ASSERT_EQUAL_UINT32(46010u, sonuc.t[1].msgid);
            TEST_ASSERT_EQUAL_UINT32(0u, kabul_ret_toplami(&sonuc.sayac));
        }
    }
}

/* (c), (d): geçerli çerçeveler arasına 0xFD ve 0xFE içermeyen baytlar (önde, arada, sonda;
   0 ile 300 bayt) konur: bütün çerçeveler teslim edilir, rx_resync_bytes aradaki baytların
   toplamına eşittir, rx_bytes akış boyuna eşittir. */
static void test_A1_K07_cd_aradaki_baytlar_atlanir_ve_sayilir(void)
{
    kabul_rng_t r;
    kabul_rng_kur(&r, 0x4B30370003ull);
    for (size_t ri = 0; ri < 2; ++ri) {
        ida_link_rol_t rol = ROLLER[ri];
        for (int tur = 0; tur < 8; ++tur) {
            size_t n = 0;
            uint32_t gurultu = 0;
            uint32_t cerceve = 0;
            for (uint8_t s = 0; s < 20u; ++s) {
                size_t g = (tur == 0) ? 0u : kabul_rng_aralik(&r, 300u);
                for (size_t k = 0; k < g; ++k) {
                    akis[n++] = kabul_rng_temiz_bayt(&r);
                }
                gurultu += (uint32_t)g;
                n += kabul_temiz_kur(&akis[n], &r, ornek_mesaj(rol, s), s, kabul_karsi_sysid(rol),
                                     kabul_karsi_compid(rol), kabul_rng_aralik(&r, 60u), 1, NULL);
                ++cerceve;
            }
            size_t g = kabul_rng_aralik(&r, 50u);
            for (size_t k = 0; k < g; ++k) {
                akis[n++] = kabul_rng_temiz_bayt(&r);
            }
            gurultu += (uint32_t)g;
            kabul_besle_yeni(rol, akis, n, &sonuc);
            TEST_ASSERT_EQUAL_UINT32(cerceve, (uint32_t)sonuc.sayi);
            TEST_ASSERT_EQUAL_UINT32(cerceve, sonuc.sayac.rx_frames);
            TEST_ASSERT_EQUAL_UINT32(gurultu, sonuc.sayac.rx_resync_bytes);
            TEST_ASSERT_EQUAL_UINT32((uint32_t)n, sonuc.sayac.rx_bytes);
            TEST_ASSERT_EQUAL_UINT32(0u, kabul_ret_toplami(&sonuc.sayac));
            kabul_modelle_esit(rol, akis, n, &sonuc, "ara baytlar");
            for (size_t k = 0; k < sonuc.sayi; ++k) {
                TEST_ASSERT_EQUAL_UINT8((uint8_t)k, sonuc.t[k].seq);
            }
        }
    }
}

/* (c): bozuk çerçeve + 0xFD içermeyen baytlar + geçerli çerçeve: sonraki geçerli çerçeve
   yakalanır; aradaki baytlar rx_resync_bytes'a girer (en az onlar kadar). */
static void test_A1_K07_c_bozuk_aday_sonrasi_fd_aranir(void)
{
    kabul_rng_t r;
    kabul_rng_kur(&r, 0x4B30370004ull);
    for (size_t g = 0; g < 40u; g += 3u) {
        size_t n = bozuk_kur(akis, &r, IDA_LINK_ROL_KART, 46020u, 1u, BOZ_YUK);
        for (size_t k = 0; k < g; ++k) {
            akis[n++] = kabul_rng_temiz_bayt(&r);
        }
        n += kabul_temiz_kur(&akis[n], &r, 46021u, 2u, 250u, 25u, 9u, 1, NULL);
        kabul_besle_yeni(IDA_LINK_ROL_KART, akis, n, &sonuc);
        TEST_ASSERT_EQUAL_UINT32(1u, (uint32_t)sonuc.sayi);
        TEST_ASSERT_EQUAL_UINT32(46021u, sonuc.t[0].msgid);
        ida_link_sayaclar_t b = kabul_sayac_sifir();
        b.rx_bytes = (uint32_t)n;
        b.rx_frames = 1u;
        b.rx_crc_err = 1u;
        b.rx_resync_bytes = (uint32_t)g; /* aday baytları değil, yalnız aradaki baytlar */
        kabul_sayac_esit(&b, &sonuc.sayac, "bozuk aday + ara baytlar");
        kabul_modelle_esit(IDA_LINK_ROL_KART, akis, n, &sonuc, "bozuk aday + ara baytlar");
    }
}

/* (e) §7.1: kesik çerçeve (sonu kesilmiş, başlığı tam) ardından gelen çerçeveleri len'i kadar
   yutar; adayın kapladığı bölgeden sonra başlayan ilk geçerli çerçeve ve sonrakiler teslim
   edilir. Yutulan çerçevelerin bölge dışında kalan baytları temizdir (0xFD/0xFE yok). */
static void test_A1_K07_e_kesik_cerceve_sonrasi_ilk_gecerli_yakalanir(void)
{
    kabul_rng_t r;
    kabul_rng_kur(&r, 0x4B30370005ull);
    for (int tur = 0; tur < 60; ++tur) {
        size_t baslangic[64];
        size_t n = 0;
        /* Kesilecek çerçeve: IDA_PING360_DATA (uzun), len = 120..246. */
        const kabul_mesaj_t *m = kabul_mesaj_al(46030u);
        size_t dolu = 120u + kabul_rng_aralik(&r, 127u);
        size_t a_n = kabul_temiz_kur(akis, &r, m->msgid, 0u, 250u, 25u, dolu, 1, NULL);
        size_t kalan = 10u + kabul_rng_aralik(&r, (uint32_t)(a_n - 10u)); /* başlık tam, kesik */
        n = kalan;
        size_t bolge_sonu = a_n; /* adayın kapladığı bölge: [0, 12 + len) */
        uint8_t son = 0;
        while (n < bolge_sonu + 80u) {
            ++son;
            TEST_ASSERT_TRUE(son < 64u);
            baslangic[son] = n;
            n += kabul_temiz_kur(&akis[n], &r, 46027u, son, 250u, 25u, 1u + kabul_rng_aralik(&r, 15u), 1, NULL);
        }
        /* Kesik adayın sağlaması yuttuğu baytlarla tesadüfen tutarsa bu tur geçersizdir. */
        uint16_t crc = kabul_crc(&akis[1], a_n - 3u, 0xFFFFu);
        crc = kabul_crc_bayt(crc, m->crc_extra);
        if (akis[a_n - 2u] == (uint8_t)(crc & 0xFFu) && akis[a_n - 1u] == (uint8_t)(crc >> 8)) {
            continue;
        }
        kabul_besle_yeni(IDA_LINK_ROL_KART, akis, n, &sonuc);
        uint8_t ilk = 0;
        for (uint8_t s = 1; s <= son; ++s) {
            if (baslangic[s] >= bolge_sonu) {
                ilk = s;
                break;
            }
        }
        TEST_ASSERT_TRUE(ilk != 0u);
        char ileti[96];
        snprintf(ileti, sizeof ileti, "tur %d: kesik %u/%u bayt, ilk bölge dışı çerçeve seq %u", tur,
                 (unsigned)kalan, (unsigned)a_n, (unsigned)ilk);
        TEST_ASSERT_EQUAL_UINT32_MESSAGE((uint32_t)(son + 1u - ilk), (uint32_t)sonuc.sayi, ileti);
        TEST_ASSERT_EQUAL_UINT8_MESSAGE(ilk, sonuc.t[0].seq, ileti);
        TEST_ASSERT_EQUAL_UINT32_MESSAGE(1u, sonuc.sayac.rx_crc_err, ileti);
        TEST_ASSERT_EQUAL_UINT32_MESSAGE(1u, kabul_ret_toplami(&sonuc.sayac), ileti);
        /* Yutulan çerçevenin aday bölgesi dışında kalan (aday başlatmayan) baytları resync. */
        TEST_ASSERT_EQUAL_UINT32_MESSAGE((uint32_t)(baslangic[ilk] - bolge_sonu), sonuc.sayac.rx_resync_bytes, ileti);
        kabul_modelle_esit(IDA_LINK_ROL_KART, akis, n, &sonuc, ileti);
    }
}

/* (e): akıştaki rastgele bir 0xFD'den başlayan sahte aday (geçerli başlıklı ama sağlaması
   tutmayan; başlığı bir KEEPALIVE adayı gibi) arkasındaki geçerli çerçevenin başını yutar;
   adayın bölgesinden sonraki ilk geçerli çerçeve yakalanır. */
static void test_A1_K07_e_sahte_aday_sonrasi_ilk_gecerli_yakalanir(void)
{
    kabul_rng_t r;
    kabul_rng_kur(&r, 0x4B30370006ull);
    for (int tur = 0; tur < 40; ++tur) {
        size_t n = 0;
        /* Sahte aday: FD 03 00 00 seq 250 25 msgid(46003) + 2 temiz bayt; adayın bölgesi 15 B,
           yani sonraki çerçevenin ilk 3 baytını da kapsar. */
        akis[n++] = 0xFDu;
        akis[n++] = 3u;
        akis[n++] = 0u;
        akis[n++] = 0u;
        akis[n++] = 9u;
        akis[n++] = 250u;
        akis[n++] = 25u;
        akis[n++] = (uint8_t)(46003u & 0xFFu);
        akis[n++] = (uint8_t)(46003u >> 8);
        akis[n++] = 0u;
        akis[n++] = kabul_rng_temiz_bayt(&r);
        akis[n++] = kabul_rng_temiz_bayt(&r);
        size_t b_bas = n;
        n += kabul_temiz_kur(&akis[n], &r, 46003u, 10u, 250u, 25u, 3u, 1, NULL);
        /* Sahte adayın sağlaması (ilk 15 bayt) tesadüfen tutarsa bu tur geçersizdir. */
        uint16_t crc = kabul_crc(&akis[1], 12u, 0xFFFFu);
        crc = kabul_crc_bayt(crc, kabul_mesaj_al(46003u)->crc_extra);
        if (akis[13] == (uint8_t)(crc & 0xFFu) && akis[14] == (uint8_t)(crc >> 8)) {
            continue;
        }
        TEST_ASSERT_TRUE(b_bas < 15u);
        n += kabul_temiz_kur(&akis[n], &r, 46010u, 11u, 250u, 25u, 17u, 1, NULL);
        kabul_besle_yeni(IDA_LINK_ROL_KART, akis, n, &sonuc);
        TEST_ASSERT_EQUAL_UINT32_MESSAGE(1u, (uint32_t)sonuc.sayi, "sahte adayın yuttuğu çerçeve teslim edildi");
        TEST_ASSERT_EQUAL_UINT8(11u, sonuc.t[0].seq);
        TEST_ASSERT_EQUAL_UINT32(1u, sonuc.sayac.rx_crc_err);
        kabul_modelle_esit(IDA_LINK_ROL_KART, akis, n, &sonuc, "sahte aday");
    }
}

/* ---- (f) denetim sırası ------------------------------------------------------------------- */
typedef enum { S_IFLAG, S_UNKNOWN, S_BADLEN, S_CRC, S_WSYS } sayac_t;

typedef struct {
    const char *ad;
    uint8_t     incompat;
    uint32_t    msgid;
    int         len_kotu; /* 0: azami; 1: azami + 1; 2: 0 */
    int         yabanci;  /* sysid yabancı (7) */
    int         crc_boz;
    sayac_t     beklenen;
} sira_t;

/* Kart alıcısı (karşı taraf 250/25). 46020 IDA_IMU doğru yön; 46060 IDA_ACTUATOR yönü yanlış;
   46050 dialect dışı. Her satır iki (ya da üç) hatayı birlikte taşır; yalnız sıradaki ilk
   tutmayan denetimin sayacı artar. */
static const sira_t SIRA[] = {
    {"incompat + bilinmeyen kimlik", 1u, 46050u, 0, 0, 0, S_IFLAG},
    {"incompat + len > azami", 1u, 46020u, 1, 0, 0, S_IFLAG},
    {"incompat + len = 0", 0x80u, 46020u, 2, 0, 0, S_IFLAG},
    {"incompat + CRC bozuk", 1u, 46020u, 0, 0, 1, S_IFLAG},
    {"incompat + yabancı sysid", 1u, 46020u, 0, 1, 0, S_IFLAG},
    {"incompat + yönü yanlış", 1u, 46060u, 0, 0, 0, S_IFLAG},
    {"bilinmeyen kimlik + len = 0", 0u, 46050u, 2, 0, 0, S_UNKNOWN},
    {"bilinmeyen kimlik + CRC bozuk", 0u, 46050u, 0, 0, 1, S_UNKNOWN},
    {"bilinmeyen kimlik + yabancı sysid", 0u, 46050u, 0, 1, 0, S_UNKNOWN},
    {"len > azami + CRC bozuk", 0u, 46020u, 1, 0, 1, S_BADLEN},
    {"len > azami + yabancı sysid (CRC o len'le doğru)", 0u, 46020u, 1, 1, 0, S_BADLEN},
    {"len > azami + yönü yanlış", 0u, 46060u, 1, 0, 0, S_BADLEN},
    {"len = 0 + yabancı sysid", 0u, 46020u, 2, 1, 0, S_BADLEN},
    {"CRC bozuk + yabancı sysid", 0u, 46020u, 0, 1, 1, S_CRC},
    {"CRC bozuk + yönü yanlış", 0u, 46060u, 0, 0, 1, S_CRC},
    {"CRC bozuk + yabancı sysid + yönü yanlış", 0u, 46060u, 0, 1, 1, S_CRC},
    {"yabancı sysid + yönü yanlış (CRC doğru)", 0u, 46060u, 0, 1, 0, S_WSYS},
};

static void test_A1_K07_f_denetim_sirasi(void)
{
    kabul_rng_t r;
    kabul_rng_kur(&r, 0x4B30370007ull);
    for (size_t i = 0; i < sizeof SIRA / sizeof SIRA[0]; ++i) {
        const sira_t *v = &SIRA[i];
        const kabul_mesaj_t *m = kabul_mesaj_bul(v->msgid);
        uint8_t azami = (uint8_t)(m != NULL ? m->yuk_azami : 20u);
        uint8_t crc_extra = (uint8_t)(m != NULL ? m->crc_extra : 0x33u);
        uint8_t len = (uint8_t)(v->len_kotu == 1 ? azami + 1u : (v->len_kotu == 2 ? 0u : azami));
        size_t aday = kabul_temiz_ham(akis, &r, len, v->incompat, 0u, 3u, (uint8_t)(v->yabanci ? 7u : 250u),
                                      25u, v->msgid, crc_extra, 0u);
        if (v->crc_boz) {
            akis[aday - 1u] = (uint8_t)(akis[aday - 1u] == 0x01u ? 0x02u : 0x01u);
        }
        size_t n = aday + kabul_temiz_kur(&akis[aday], &r, 46010u, 4u, 250u, 25u, 17u, 1, NULL);
        kabul_besle_yeni(IDA_LINK_ROL_KART, akis, n, &sonuc);
        ida_link_sayaclar_t b = kabul_sayac_sifir();
        b.rx_bytes = (uint32_t)n;
        b.rx_frames = 1u;
        switch (v->beklenen) {
        case S_IFLAG:
            b.rx_iflag = 1u;
            break;
        case S_UNKNOWN:
            b.rx_unknown_msgid = 1u;
            break;
        case S_BADLEN:
            b.rx_bad_len = 1u;
            break;
        case S_CRC:
            b.rx_crc_err = 1u;
            break;
        case S_WSYS:
        default:
            b.rx_wrong_sys = 1u;
            break;
        }
        /* Başlıkta ret: 0xFD dışındaki (temiz) aday baytları yeniden taranır ve resync'e girer;
           sağlama ve sonrasındaki retlerde aday bütünüyle tüketilir. */
        if (v->beklenen == S_IFLAG || v->beklenen == S_UNKNOWN || v->beklenen == S_BADLEN) {
            b.rx_resync_bytes = (uint32_t)(aday - 1u);
        }
        kabul_sayac_esit(&b, &sonuc.sayac, v->ad);
        TEST_ASSERT_EQUAL_UINT32_MESSAGE(1u, (uint32_t)sonuc.sayi, v->ad);
        TEST_ASSERT_EQUAL_UINT8_MESSAGE(4u, sonuc.t[0].seq, v->ad);
        kabul_modelle_esit(IDA_LINK_ROL_KART, akis, n, &sonuc, v->ad);
    }
}

/* (f) sol incelemesinin ayırıcı girdisi: kart alıcısına fd 01 00 00 02 07 19 ec b3 00 00 89 93
   (46060 IDA_ACTUATOR, len 1, sysid 7, compid 25; CRC doğru; kimlik çifti ve yön yanlış).
   Beklenen yalnız rx_wrong_sys = 1 (kimlik denetimi yön denetiminden önce). */
static void test_A1_K07_f_sol_ornegi_kimlik_yonden_once(void)
{
    static const uint8_t ORNEK[13] = {0xfdu, 0x01u, 0x00u, 0x00u, 0x02u, 0x07u, 0x19u,
                                      0xecu, 0xb3u, 0x00u, 0x00u, 0x89u, 0x93u};
    kabul_cozulen_t c;
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, kabul_cerceve_dogrula(ORNEK, sizeof ORNEK, &c),
                                  "örneğin CRC'si referansla doğru değil");
    kabul_besle_yeni(IDA_LINK_ROL_KART, ORNEK, sizeof ORNEK, &sonuc);
    ida_link_sayaclar_t b = kabul_sayac_sifir();
    b.rx_bytes = 13u;
    b.rx_wrong_sys = 1u;
    kabul_sayac_esit(&b, &sonuc.sayac, "sol örneği");
    TEST_ASSERT_EQUAL_UINT32(0u, (uint32_t)sonuc.sayi);
    kabul_modelle_esit(IDA_LINK_ROL_KART, ORNEK, sizeof ORNEK, &sonuc, "sol örneği");
}

/* (g) başlıkta reddedilen adaydan sonra yeniden tarama. Üç ret türünde, adayın yükünün içine
   tam bir geçerli KEEPALIVE gömülür: gömülü çerçeve teslim edilir; adayın geri kalan temiz
   baytları resync'e girer. */
static void test_A1_K07_g_baslik_reddi_sonrasi_yeniden_tarama(void)
{
    kabul_rng_t r;
    kabul_rng_kur(&r, 0x4B30370008ull);
    static const char *const TUR[3] = {"incompat_flags = 1", "bilinmeyen kimlik 46050", "len = azami + 1"};
    for (int t = 0; t < 3; ++t) {
        const kabul_mesaj_t *m = kabul_mesaj_al(46030u);
        uint8_t ic[32];
        size_t ic_n = kabul_temiz_kur(ic, &r, 46003u, 77u, 250u, 25u, 3u, 1, NULL);
        uint8_t len = (uint8_t)(t == 2 ? m->yuk_azami + 1u : 100u);
        uint8_t incompat = (uint8_t)(t == 0 ? 1u : 0u);
        uint32_t id = t == 1 ? 46050u : m->msgid;
        uint8_t yuk[IDA_LINK_YUK_AZAMI];
        size_t aday = 0;
        int temiz = 0;
        for (int deneme = 0; deneme < 4096 && !temiz; ++deneme) {
            for (size_t i = 0; i < len; ++i) {
                yuk[i] = (uint8_t)(1u + kabul_rng_aralik(&r, 0xFCu));
            }
            memcpy(&yuk[40], ic, ic_n);
            aday = kabul_cerceve_ham(akis, len, incompat, 0u, 76u, 250u, 25u, id, yuk, len, m->crc_extra);
            temiz = akis[aday - 1u] < 0xFDu && akis[aday - 2u] < 0xFDu;
        }
        TEST_ASSERT_TRUE(temiz);
        size_t n = aday + kabul_temiz_kur(&akis[aday], &r, 46010u, 78u, 250u, 25u, 17u, 1, NULL);
        kabul_besle_yeni(IDA_LINK_ROL_KART, akis, n, &sonuc);
        ida_link_sayaclar_t b = kabul_sayac_sifir();
        b.rx_bytes = (uint32_t)n;
        b.rx_frames = 2u;
        b.rx_iflag = t == 0 ? 1u : 0u;
        b.rx_unknown_msgid = t == 1 ? 1u : 0u;
        b.rx_bad_len = t == 2 ? 1u : 0u;
        b.rx_resync_bytes = (uint32_t)(aday - 1u - ic_n);
        kabul_sayac_esit(&b, &sonuc.sayac, TUR[t]);
        TEST_ASSERT_EQUAL_UINT32_MESSAGE(2u, (uint32_t)sonuc.sayi, TUR[t]);
        TEST_ASSERT_EQUAL_UINT8_MESSAGE(77u, sonuc.t[0].seq, TUR[t]);
        TEST_ASSERT_EQUAL_UINT8_MESSAGE(78u, sonuc.t[1].seq, TUR[t]);
        kabul_modelle_esit(IDA_LINK_ROL_KART, akis, n, &sonuc, TUR[t]);
    }
}

/* (g) "adayın arkasındaki geçerli çerçeve kaçırılmaz": geçerli çerçevenin hemen önünde tek
   başıboş 0xFD. Başıboş adayın başlığı geçerli çerçevenin ilk dokuz baytıdır; incompat
   konumuna geçerli çerçevenin len'i (≠ 0) düşer: yalnız rx_iflag = 1; başıboş 0xFD aday
   başlattığı için resync'e girmez; geçerli çerçeve teslim edilir. */
static void test_A1_K07_g_basibos_fd_arkasindaki_cerceve(void)
{
    kabul_rng_t r;
    kabul_rng_kur(&r, 0x4B30370009ull);
    for (size_t ri = 0; ri < 2; ++ri) {
        ida_link_rol_t rol = ROLLER[ri];
        size_t n = 0;
        akis[n++] = 0xFDu;
        n += kabul_temiz_kur(&akis[n], &r, ornek_mesaj(rol, 0), 5u, kabul_karsi_sysid(rol),
                             kabul_karsi_compid(rol), 9u, 1, NULL);
        kabul_besle_yeni(rol, akis, n, &sonuc);
        ida_link_sayaclar_t b = kabul_sayac_sifir();
        b.rx_bytes = (uint32_t)n;
        b.rx_frames = 1u;
        b.rx_iflag = 1u;
        kabul_sayac_esit(&b, &sonuc.sayac, "başıboş 0xFD");
        TEST_ASSERT_EQUAL_UINT32(1u, (uint32_t)sonuc.sayi);
        TEST_ASSERT_EQUAL_UINT8(5u, sonuc.t[0].seq);
        kabul_modelle_esit(rol, akis, n, &sonuc, "başıboş 0xFD");
    }
}


/* CRC-16/MCRF4XX denetim değeri (A1 §3.2): "123456789" -> 0x6F91 (referansın doğruluğu). */
static void test_A1_K07_referans_crc_denetim_degeri(void)
{
    static const uint8_t ornek[9] = {'1', '2', '3', '4', '5', '6', '7', '8', '9'};
    TEST_ASSERT_EQUAL_HEX16(0x6F91u, kabul_crc(ornek, sizeof ornek, 0xFFFFu));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_A1_K07_referans_crc_denetim_degeri);
    RUN_TEST(test_A1_K07_a_sagmasi_tutmayan_atilir);
    RUN_TEST(test_A1_K07_b_aday_icindeki_cerceve_teslim_edilmez);
    RUN_TEST(test_A1_K07_cd_aradaki_baytlar_atlanir_ve_sayilir);
    RUN_TEST(test_A1_K07_c_bozuk_aday_sonrasi_fd_aranir);
    RUN_TEST(test_A1_K07_e_kesik_cerceve_sonrasi_ilk_gecerli_yakalanir);
    RUN_TEST(test_A1_K07_e_sahte_aday_sonrasi_ilk_gecerli_yakalanir);
    RUN_TEST(test_A1_K07_f_denetim_sirasi);
    RUN_TEST(test_A1_K07_f_sol_ornegi_kimlik_yonden_once);
    RUN_TEST(test_A1_K07_g_baslik_reddi_sonrasi_yeniden_tarama);
    RUN_TEST(test_A1_K07_g_basibos_fd_arkasindaki_cerceve);
    return UNITY_END();
}
