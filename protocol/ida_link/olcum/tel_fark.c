/*
 * tel_fark.c — eski ve yeni tel katmanının fark testi (W3-tel-hiz kabul md. 2; Unity).
 *
 * "Eski" W3-tel-hiz öncesi tel.c'nin değiştirilmemiş kopyasıdır (tel_eski.c, ida_link_eski_
 * önekli), "yeni" güncel ida_link'tir. İki uygulamaya aynı girdi verilir ve her çağrıdan
 * sonra şunlar karşılaştırılır:
 *  - alıcı: dönüş kodu, *tuketilen, *cerceve'nin bütün baytları (iki tarafta da çağrıdan önce
 *    aynı desenle doldurulur; böylece "yazılmadı" da karşılaştırılır), bütün sayaçlar ve
 *    ayrıştırıcının iç durumu (durum, dolu, tampon[0, dolu), seq geçmişi; başlığı geçmiş
 *    adayda hedef ve satır);
 *  - ida_link_alici_tasma, ida_link_alici_kur (geçersiz roller dahil);
 *  - ida_link_gonder: dönüş, *yazilan, çıkış tamponunun tamamı, seq;
 *  - ida_link_coz: dönüş ve hedef tamponun tamamı; ida_link_mesaj_bilgisi.
 * Akışlar: A1 Ek B'nin 38 altın çerçevesi ve sabit tohumlu sözde rastgele akışlar (geçerli
 * çerçeveler; bozuk CRC; yanlış sysid/compid; yanlış yön; incompat_flags; dialect dışı kimlik;
 * len = 0 ve aşırı len; kesik çerçeve; 0xFD/0xFE ağırlıklı gürültü; taşma bildirimi). Her
 * akış çok sayıda blok boyu bölünmesiyle (1, 2, 3, 7, 10, 11, 12, 13, 64, 255, 267, 300, tek
 * seferde, rastgele) ve iki rolle beslenir; altın akış 1–300 arası her sabit blok boyuyla,
 * her altın çerçeve her iki parçaya bölünme noktasıyla beslenir.
 *
 * Ek olarak: CRC tablosu (src/tel_crc_tablo.h) her (crc, bayt) çiftinde MAVLink'in
 * crc_accumulate işleviyle aynıdır (65 536 x 256). Karşılaştırıcının duyarlılığı ayrı bir
 * testte gösterilir (bilerek farklı girdiyle fark bulunur).
 *
 * Kullanım: ida_link_tel_fark [senaryo_sayisi]   (varsayılan 300)
 */
#include "ida_link/tel.h"
#include "tel_eski.h"

#include "ida_link_altin_veri.h"
#include "mavlink/checksum.h"
#include "tel_crc_tablo.h"
#include "unity.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void setUp(void);
void tearDown(void);
void setUp(void) {}
void tearDown(void) {}

/* ------------------------------------------------------------------ */
/* Sözde rastgele (sabit tohum) ve sayaçlar                           */
/* ------------------------------------------------------------------ */

static uint32_t g_tohum = 0x1D4A2026u;
static unsigned long g_senaryo = 300ul;
static unsigned long long g_karsilastirma = 0ull;
static unsigned long long g_teslim = 0ull;

static uint32_t rnd(void)
{
    uint32_t x = g_tohum;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    g_tohum = x;
    return x;
}

static uint32_t rnd_aralik(uint32_t alt, uint32_t ust) /* [alt, ust] */
{
    return alt + rnd() % (ust - alt + 1u);
}

static uint8_t rnd_bayt(void)
{
    return (uint8_t)(rnd() >> 24);
}

/* ------------------------------------------------------------------ */
/* Dialect bilgisi (eski uygulamadan; bilgi eşitliği ayrıca sınanır)   */
/* ------------------------------------------------------------------ */

typedef struct {
    ida_link_mesaj_bilgisi_t b[100];
    unsigned n;
} dialect_t;

static dialect_t g_d;

static void dialect_kur(void)
{
    g_d.n = 0;
    for (uint32_t id = 46000u; id < 46100u; ++id) {
        ida_link_mesaj_bilgisi_t b;
        if (ida_link_eski_mesaj_bilgisi(id, &b) == IDA_LINK_TAMAM) {
            g_d.b[g_d.n++] = b;
        }
    }
}

/* Bu rolün alabileceği (yon) ya da alamayacağı mesajdan rastgele biri. */
static const ida_link_mesaj_bilgisi_t *mesaj_sec(uint8_t yon_biti, int yonde)
{
    for (int deneme = 0; deneme < 64; ++deneme) {
        const ida_link_mesaj_bilgisi_t *b = &g_d.b[rnd() % g_d.n];
        if (((b->yon & yon_biti) != 0u) == (yonde != 0)) {
            return b;
        }
    }
    return NULL;
}

/* ------------------------------------------------------------------ */
/* Çerçeve kurucu (CRC checksum.h'in crc_accumulate'iyle; tablodan bağımsız) */
/* ------------------------------------------------------------------ */

static size_t cerceve_yaz(uint8_t *o, uint8_t len, uint8_t iflag, uint8_t seq, uint8_t sys,
                          uint8_t comp, uint32_t msgid, uint8_t crc_extra, int crc_boz)
{
    o[0] = 0xFDu;
    o[1] = len;
    o[2] = iflag;
    o[3] = (uint8_t)(rnd_bayt() & 0x01u);  /* compat_flags yok sayılır */
    o[4] = seq;
    o[5] = sys;
    o[6] = comp;
    o[7] = (uint8_t)(msgid & 0xFFu);
    o[8] = (uint8_t)((msgid >> 8) & 0xFFu);
    o[9] = (uint8_t)((msgid >> 16) & 0xFFu);
    for (unsigned i = 0; i < len; ++i) {
        /* Gerçekçi yük: sıfırlar ve 0xFD/0xFE de içerir. */
        const uint32_t r = rnd() % 8u;
        o[10u + i] = (uint8_t)(r == 0u ? 0u : (r == 1u ? 0xFDu : (r == 2u ? 0xFEu : rnd_bayt())));
    }
    uint16_t crc;
    crc_init(&crc);
    for (unsigned i = 1; i < 10u + len; ++i) {
        crc_accumulate(o[i], &crc);
    }
    crc_accumulate(crc_extra, &crc);
    if (crc_boz) {
        crc = (uint16_t)(crc ^ (uint16_t)rnd_aralik(1u, 0xFFFFu));
    }
    o[10u + len] = (uint8_t)(crc & 0xFFu);
    o[11u + len] = (uint8_t)(crc >> 8);
    return 12u + (size_t)len;
}

/* ------------------------------------------------------------------ */
/* Akış üretimi                                                        */
/* ------------------------------------------------------------------ */

#define AKIS_AZAMI 4096u

typedef struct {
    uint8_t v[AKIS_AZAMI + IDA_LINK_CERCEVE_AZAMI];
    size_t n;
} akis_t;

static void akis_altin(akis_t *a)
{
    a->n = 0;
    for (unsigned i = 0; i < IDA_LINK_ALTIN_SAYISI; ++i) {
        memcpy(&a->v[a->n], ida_link_altin[i].bayt, ida_link_altin[i].cerceve);
        a->n += ida_link_altin[i].cerceve;
    }
}

/* Alıcı rolü için karışık akış (bkz. dosya başı). */
static void akis_rastgele(akis_t *a, ida_link_rol_t rol, size_t hedef)
{
    const int kart = rol == IDA_LINK_ROL_KART;
    const uint8_t yon = kart ? (uint8_t)IDA_LINK_YON_SIM_KART : (uint8_t)IDA_LINK_YON_KART_SIM;
    const uint8_t sys = kart ? (uint8_t)IDA_LINK_SIM_SYSID : (uint8_t)IDA_LINK_KART_SYSID;
    const uint8_t comp = kart ? (uint8_t)IDA_LINK_SIM_COMPID : (uint8_t)IDA_LINK_KART_COMPID;
    uint8_t seq = rnd_bayt();
    a->n = 0;
    while (a->n < hedef) {
        uint8_t *o = &a->v[a->n];
        const uint32_t olay = rnd() % 100u;
        const ida_link_mesaj_bilgisi_t *b = mesaj_sec(yon, 1);
        if (b == NULL) {
            a->n = 0;  /* ulaşılamaz: her rolün alacağı mesaj vardır; çağıran n > 0 denetler */
            return;
        }
        const uint8_t len_t = (uint8_t)(rnd() % 4u == 0u ? b->yuk_azami
                                                          : rnd_aralik(1u, b->yuk_azami));
        const uint8_t sq = rnd() % 16u == 0u ? rnd_bayt() : seq;
        seq = (uint8_t)(sq + 1u);
        if (olay < 40u) {               /* geçerli */
            a->n += cerceve_yaz(o, len_t, 0u, sq, sys, comp, b->msgid, b->crc_extra, 0);
        } else if (olay < 45u) {        /* yanlış sysid ya da compid */
            const int hangisi = (int)(rnd() % 3u);
            const uint8_t s2 = hangisi != 1 ? (uint8_t)(sys ^ rnd_aralik(1u, 255u)) : sys;
            const uint8_t c2 = hangisi != 0 ? (uint8_t)(comp ^ rnd_aralik(1u, 255u)) : comp;
            a->n += cerceve_yaz(o, len_t, 0u, sq, s2, c2, b->msgid, b->crc_extra, 0);
        } else if (olay < 50u) {        /* yanlış yön */
            const ida_link_mesaj_bilgisi_t *y = mesaj_sec(yon, 0);
            if (y != NULL) {
                a->n += cerceve_yaz(o, (uint8_t)rnd_aralik(1u, y->yuk_azami), 0u, sq, sys, comp,
                                    y->msgid, y->crc_extra, 0);
            }
        } else if (olay < 58u) {        /* bozuk CRC ya da bozulmuş bayt */
            const size_t m = cerceve_yaz(o, len_t, 0u, sq, sys, comp, b->msgid, b->crc_extra,
                                         rnd() % 2u == 0u);
            if (rnd() % 2u == 0u) {
                const uint32_t yer = rnd_aralik(1u, (uint32_t)m - 1u);
                o[yer] = (uint8_t)(o[yer] ^ rnd_aralik(1u, 255u));
            }
            a->n += m;
        } else if (olay < 62u) {        /* incompat_flags */
            a->n += cerceve_yaz(o, len_t, (uint8_t)rnd_aralik(1u, 255u), sq, sys, comp,
                                b->msgid, b->crc_extra, 0);
        } else if (olay < 66u) {        /* dialect dışı kimlik */
            uint32_t id = rnd() % 2u == 0u ? rnd() & 0xFFFFFFu : rnd_aralik(45990u, 46110u);
            ida_link_mesaj_bilgisi_t z;
            while (ida_link_eski_mesaj_bilgisi(id, &z) == IDA_LINK_TAMAM) {
                id = (id + 1u) & 0xFFFFFFu;
            }
            a->n += cerceve_yaz(o, (uint8_t)rnd_aralik(1u, 255u), 0u, sq, sys, comp, id,
                                rnd_bayt(), 0);
        } else if (olay < 70u) {        /* len = 0 ya da en büyük yükten büyük */
            uint8_t l = 0u;
            if (rnd() % 2u == 0u && b->yuk_azami < 255u) {
                l = (uint8_t)rnd_aralik((uint32_t)b->yuk_azami + 1u, 255u);
            }
            a->n += cerceve_yaz(o, l, 0u, sq, sys, comp, b->msgid, b->crc_extra, 0);
        } else if (olay < 78u) {        /* kesik çerçeve */
            const size_t m = cerceve_yaz(o, len_t, 0u, sq, sys, comp, b->msgid, b->crc_extra, 0);
            a->n += rnd_aralik(1u, (uint32_t)m - 1u);
        } else if (olay < 93u) {        /* gürültü, 0xFD/0xFE ağırlıklı */
            const uint32_t k = rnd_aralik(1u, 24u);
            for (uint32_t i = 0; i < k; ++i) {
                const uint32_t r = rnd() % 4u;
                o[i] = (uint8_t)(r == 0u ? 0xFDu : (r == 1u ? 0xFEu : rnd_bayt()));
            }
            a->n += k;
        } else {                        /* altın çerçeve */
            const ida_link_altin_t *g = &ida_link_altin[rnd() % IDA_LINK_ALTIN_SAYISI];
            memcpy(o, g->bayt, g->cerceve);
            a->n += g->cerceve;
        }
    }
}

/* ------------------------------------------------------------------ */
/* Karşılaştırıcılar (0: eşit; 1: fark, *neden doldurulur)            */
/* ------------------------------------------------------------------ */

static int alici_farki(const ida_link_alici_t *e, const ida_link_alici_t *y, const char **neden)
{
    g_karsilastirma++;
    if (memcmp(&e->sayac, &y->sayac, sizeof e->sayac) != 0) {
        *neden = "sayaclar";
        return 1;
    }
    if (e->ozel_rol != y->ozel_rol || e->ozel_durum != y->ozel_durum ||
        e->ozel_dolu != y->ozel_dolu || e->ozel_seq_var != y->ozel_seq_var ||
        e->ozel_son_seq != y->ozel_son_seq) {
        *neden = "ic durum";
        return 1;
    }
    if (e->ozel_dolu <= IDA_LINK_CERCEVE_AZAMI &&
        memcmp(e->ozel_tampon, y->ozel_tampon, e->ozel_dolu) != 0) {
        *neden = "aday tamponu";
        return 1;
    }
    if (e->ozel_durum != 0u && e->ozel_dolu >= IDA_LINK_BASLIK_BOYU &&
        (e->ozel_hedef != y->ozel_hedef || e->ozel_satir != y->ozel_satir)) {
        *neden = "aday hedefi";
        return 1;
    }
    return 0;
}

/* Bir akışı iki alıcıya aynı bölünmeyle verir. Akış önce bloklara bölünür (taşımanın bir
   seferde verdiği baytlar); bir blokta çerçeve teslim edildikçe yalnız o bloğun kalanı
   yeniden verilir, bloğun kalanı hiçbir zaman sonraki blokla birleştirilmez. Yeni blok boyu
   yalnız önceki blok tamamen tüketilince seçilir. ilk != 0: ilk bloğun boyu. parca: 0 kalan
   akışın tamamı tek blok; > 0 sabit blok boyu; RASTGELE_* rastgele blok boyu. tasma != 0:
   çağrılar arasında rastgele taşma bildirimi. Dönüş 0: eşit. g_cagri_kaydi.etkin ise her
   ida_link_al çağrısının uzunluğu oraya yazılır. */
#define RASTGELE_GENIS ((size_t)-1)
#define RASTGELE_DAR ((size_t)-2)

typedef struct {
    ida_link_alici_t e;
    ida_link_alici_t y;
    ida_link_cerceve_t ce;
    ida_link_cerceve_t cy;
} cift_t;

static cift_t g_c;

typedef struct {
    size_t uzunluk[64];
    size_t n;
    int etkin;
} cagri_kaydi_t;

static cagri_kaydi_t g_cagri_kaydi;

static int besle(const uint8_t *v, size_t n, ida_link_rol_t rol, size_t ilk, size_t parca,
                 int tasma, char *aciklama, size_t aciklama_boyu)
{
    const char *neden = "";
    if (ida_link_eski_alici_kur(&g_c.e, rol) != ida_link_alici_kur(&g_c.y, rol) ||
        alici_farki(&g_c.e, &g_c.y, &neden)) {
        snprintf(aciklama, aciklama_boyu, "kurulum farkı");
        return 1;
    }
    size_t i = 0;
    unsigned long cagri = 0;
    while (i < n) {
        /* Yeni blok: [i, blok_sonu). */
        size_t m;
        if (i == 0u && ilk != 0u) {
            m = ilk;
        } else if (parca == 0u) {
            m = n - i;
        } else if (parca == RASTGELE_GENIS) {
            m = rnd_aralik(1u, 300u);
        } else if (parca == RASTGELE_DAR) {
            m = rnd_aralik(1u, 16u);
        } else {
            m = parca;
        }
        if (m > n - i) {
            m = n - i;
        }
        const size_t blok_sonu = i + m;
        while (i < blok_sonu) {
            const size_t k = blok_sonu - i;  /* bloğun kalanı */
            if (tasma && rnd() % 50u == 0u) {
                if (ida_link_eski_alici_tasma(&g_c.e) != ida_link_alici_tasma(&g_c.y) ||
                    alici_farki(&g_c.e, &g_c.y, &neden)) {
                    snprintf(aciklama, aciklama_boyu, "taşma sonrası fark (%s), bayt %zu",
                             neden, i);
                    return 1;
                }
            }
            /* Çerçeve tamponları aynı desenle dolu: yazılan ve yazılmayan baytlar
               karşılaştırılır. */
            const uint8_t desen = rnd_bayt();
            memset(&g_c.ce, desen, sizeof g_c.ce);
            memset(&g_c.cy, desen, sizeof g_c.cy);
            size_t te = 0xA5A5u;
            size_t ty = 0xA5A5u;
            const ida_link_rc_t re = ida_link_eski_al(&g_c.e, &v[i], k, &te, &g_c.ce);
            const ida_link_rc_t ry = ida_link_al(&g_c.y, &v[i], k, &ty, &g_c.cy);
            ++cagri;
            if (g_cagri_kaydi.etkin && g_cagri_kaydi.n < 64u) {
                g_cagri_kaydi.uzunluk[g_cagri_kaydi.n++] = k;
            }
            if (re != ry || te != ty) {
                snprintf(aciklama, aciklama_boyu,
                         "dönüş farkı: eski %d/%zu yeni %d/%zu (bayt %zu, parça %zu, çağrı %lu)",
                         (int)re, te, (int)ry, ty, i, k, cagri);
                return 1;
            }
            g_karsilastirma++;
            if (memcmp(&g_c.ce, &g_c.cy, sizeof g_c.ce) != 0) {
                snprintf(aciklama, aciklama_boyu, "çerçeve farkı (bayt %zu, parça %zu, dönüş %d)",
                         i, k, (int)re);
                return 1;
            }
            if (alici_farki(&g_c.e, &g_c.y, &neden)) {
                snprintf(aciklama, aciklama_boyu, "alıcı farkı: %s (bayt %zu, parça %zu)", neden,
                         i, k);
                return 1;
            }
            if (re == IDA_LINK_CERCEVE) {
                if (te == 0u || te > k) {
                    snprintf(aciklama, aciklama_boyu, "geçersiz tüketilen %zu", te);
                    return 1;
                }
                g_teslim++;
            } else if (re != IDA_LINK_BEKLE || te != k) {
                snprintf(aciklama, aciklama_boyu, "beklenmeyen dönüş %d", (int)re);
                return 1;
            }
            i += te;
        }
    }
    return 0;
}

#define BESLE_ESIT(v, n, rol, parca, tasma)                                          \
    do {                                                                              \
        char _a[192];                                                                 \
        if (besle((v), (n), (rol), 0u, (parca), (tasma), _a, sizeof _a) != 0) {       \
            TEST_FAIL_MESSAGE(_a);                                                    \
        }                                                                             \
    } while (0)

static const size_t PARCALAR[] = {0u, 1u, 2u, 3u, 7u, 10u, 11u, 12u, 13u, 64u,
                                  255u, 267u, 300u, RASTGELE_GENIS, RASTGELE_DAR};
#define PARCA_SAYISI (sizeof PARCALAR / sizeof PARCALAR[0])

static const ida_link_rol_t ROLLER[2] = {IDA_LINK_ROL_KART, IDA_LINK_ROL_SIM};

static akis_t g_akis;

/* ------------------------------------------------------------------ */
/* Testler                                                            */
/* ------------------------------------------------------------------ */

static void test_crc_tablosu_crc_accumulate_ile_ayni(void)
{
    unsigned long fark = 0;
    for (uint32_t c = 0; c <= 0xFFFFu; ++c) {
        for (uint32_t b = 0; b <= 0xFFu; ++b) {
            uint16_t bit = (uint16_t)c;
            crc_accumulate((uint8_t)b, &bit);
            if (tel_crc_adim((uint16_t)c, (uint8_t)b) != bit) {
                ++fark;
            }
        }
    }
    TEST_ASSERT_EQUAL_UINT32(0u, fark);
    /* A1 §3.2 denetim değeri. */
    uint16_t crc;
    crc_init(&crc);
    const char *s = "123456789";
    for (unsigned i = 0; i < 9u; ++i) {
        crc = tel_crc_adim(crc, (uint8_t)s[i]);
    }
    TEST_ASSERT_EQUAL_HEX16(0x6F91u, crc);
}

static void test_altin_akis_her_sabit_blok_boyuyla(void)
{
    akis_altin(&g_akis);
    for (int r = 0; r < 2; ++r) {
        BESLE_ESIT(g_akis.v, g_akis.n, ROLLER[r], 0u, 0);
        for (size_t p = 1; p <= 300u; ++p) {
            BESLE_ESIT(g_akis.v, g_akis.n, ROLLER[r], p, 0);
        }
    }
}

static void test_altin_cerceve_her_bolunme_noktasiyla(void)
{
    for (unsigned k = 0; k < IDA_LINK_ALTIN_SAYISI; ++k) {
        const ida_link_altin_t *g = &ida_link_altin[k];
        const ida_link_rol_t rol =
            g->sysid == IDA_LINK_SIM_SYSID ? IDA_LINK_ROL_KART : IDA_LINK_ROL_SIM;
        for (size_t p = 0; p <= g->cerceve; ++p) {
            char a[192];
            /* İki parça: [0, p) ve [p, n). İki çerçeve arka arkaya (seq geçmişi de sınanır). */
            uint8_t v[2u * IDA_LINK_CERCEVE_AZAMI];
            memcpy(v, g->bayt, g->cerceve);
            memcpy(&v[g->cerceve], g->bayt, g->cerceve);
            if (besle(v, 2u * g->cerceve, rol, p, 0u, 0, a, sizeof a) != 0) {
                TEST_FAIL_MESSAGE(a);
            }
        }
    }
}

static void test_rastgele_akislar_her_bolunmeyle(void)
{
    for (unsigned long s = 0; s < g_senaryo; ++s) {
        for (int r = 0; r < 2; ++r) {
            akis_rastgele(&g_akis, ROLLER[r], rnd_aralik(64u, AKIS_AZAMI / 2u));
            TEST_ASSERT_TRUE(g_akis.n > 0u);
            for (size_t k = 0; k < PARCA_SAYISI; ++k) {
                BESLE_ESIT(g_akis.v, g_akis.n, ROLLER[r], PARCALAR[k], 0);
            }
            /* Taşma bildirimiyle (rastgele bölünme). */
            BESLE_ESIT(g_akis.v, g_akis.n, ROLLER[r], RASTGELE_GENIS, 1);
            BESLE_ESIT(g_akis.v, g_akis.n, ROLLER[r], RASTGELE_DAR, 1);
        }
    }
}

static void test_tamamen_rastgele_baytlar(void)
{
    for (unsigned long s = 0; s < g_senaryo; ++s) {
        g_akis.n = rnd_aralik(1u, AKIS_AZAMI);
        for (size_t i = 0; i < g_akis.n; ++i) {
            g_akis.v[i] = rnd_bayt();
        }
        const ida_link_rol_t rol = ROLLER[s % 2u];
        BESLE_ESIT(g_akis.v, g_akis.n, rol, 0u, 0);
        BESLE_ESIT(g_akis.v, g_akis.n, rol, RASTGELE_GENIS, 0);
    }
}

static void test_alici_argumanlari_ve_rol(void)
{
    static const int ROL_DEGERLERI[] = {0, 1, 2, 3, 255, 257, 258, -1};
    for (size_t k = 0; k < sizeof ROL_DEGERLERI / sizeof ROL_DEGERLERI[0]; ++k) {
        memset(&g_c, 0x5A, sizeof g_c);
        const ida_link_rol_t rol = (ida_link_rol_t)ROL_DEGERLERI[k];
        TEST_ASSERT_EQUAL_INT(ida_link_eski_alici_kur(&g_c.e, rol), ida_link_alici_kur(&g_c.y, rol));
        TEST_ASSERT_EQUAL_MEMORY(&g_c.e, &g_c.y, sizeof g_c.e);
        const uint8_t v[3] = {0xFDu, 0x01u, 0x00u};
        size_t te = 7u;
        size_t ty = 7u;
        TEST_ASSERT_EQUAL_INT(ida_link_eski_al(&g_c.e, v, 3u, &te, &g_c.ce),
                              ida_link_al(&g_c.y, v, 3u, &ty, &g_c.cy));
        TEST_ASSERT_EQUAL_size_t(te, ty);
        TEST_ASSERT_EQUAL_MEMORY(&g_c.e, &g_c.y, sizeof g_c.e);
        TEST_ASSERT_EQUAL_INT(ida_link_eski_alici_tasma(&g_c.e), ida_link_alici_tasma(&g_c.y));
        TEST_ASSERT_EQUAL_MEMORY(&g_c.e, &g_c.y, sizeof g_c.e);
    }
    TEST_ASSERT_EQUAL_INT(ida_link_eski_alici_kur(NULL, IDA_LINK_ROL_KART),
                          ida_link_alici_kur(NULL, IDA_LINK_ROL_KART));
    TEST_ASSERT_EQUAL_INT(ida_link_eski_alici_tasma(NULL), ida_link_alici_tasma(NULL));
    (void)ida_link_eski_alici_kur(&g_c.e, IDA_LINK_ROL_KART);
    (void)ida_link_alici_kur(&g_c.y, IDA_LINK_ROL_KART);
    size_t te = 0u;
    size_t ty = 0u;
    const uint8_t v[1] = {0xFDu};
    TEST_ASSERT_EQUAL_INT(ida_link_eski_al(&g_c.e, NULL, 1u, &te, &g_c.ce),
                          ida_link_al(&g_c.y, NULL, 1u, &ty, &g_c.cy));
    TEST_ASSERT_EQUAL_INT(ida_link_eski_al(&g_c.e, NULL, 0u, &te, &g_c.ce),
                          ida_link_al(&g_c.y, NULL, 0u, &ty, &g_c.cy));
    TEST_ASSERT_EQUAL_size_t(te, ty);
    TEST_ASSERT_EQUAL_INT(ida_link_eski_al(&g_c.e, v, 1u, NULL, &g_c.ce),
                          ida_link_al(&g_c.y, v, 1u, NULL, &g_c.cy));
    TEST_ASSERT_EQUAL_INT(ida_link_eski_al(&g_c.e, v, 1u, &te, NULL),
                          ida_link_al(&g_c.y, v, 1u, &ty, NULL));
    TEST_ASSERT_EQUAL_INT(ida_link_eski_al(NULL, v, 1u, &te, &g_c.ce),
                          ida_link_al(NULL, v, 1u, &ty, &g_c.cy));
    TEST_ASSERT_EQUAL_MEMORY(&g_c.e, &g_c.y, sizeof g_c.e);
}

static void test_gonderici_rastgele(void)
{
    uint8_t yapi[512];
    uint8_t ce[IDA_LINK_CERCEVE_AZAMI + 16u];
    uint8_t cy[IDA_LINK_CERCEVE_AZAMI + 16u];
    unsigned long tamam = 0;
    for (unsigned long s = 0; s < 200ul * g_senaryo; ++s) {
        const ida_link_rol_t rol = ROLLER[rnd() % 2u];
        ida_link_gonderici_t ge;
        ida_link_gonderici_t gy;
        TEST_ASSERT_EQUAL_INT(ida_link_eski_gonderici_kur(&ge, rol), ida_link_gonderici_kur(&gy, rol));
        TEST_ASSERT_EQUAL_MEMORY(&ge, &gy, sizeof ge);
        ge.seq = gy.seq = rnd_bayt();

        uint32_t msgid;
        size_t boy;
        const uint32_t secim = rnd() % 20u;
        const ida_link_mesaj_bilgisi_t *b = &g_d.b[rnd() % g_d.n];
        if (secim == 0u) {
            msgid = rnd_aralik(45990u, 46110u);
            boy = rnd_aralik(1u, IDA_LINK_YUK_AZAMI);
        } else {
            msgid = b->msgid;
            boy = secim == 1u ? rnd_aralik(1u, IDA_LINK_YUK_AZAMI) : b->yapi_boyu;
        }
        TEST_ASSERT_TRUE(boy <= sizeof yapi);
        /* Yapı: rastgele baytlar, rastgele bir noktadan sonrası sıfır (kırpmayı sınar). */
        for (size_t i = 0; i < sizeof yapi; ++i) {
            yapi[i] = rnd_bayt();
        }
        const uint32_t kirp = rnd() % 4u;
        const size_t sinir = kirp == 0u ? 0u : (kirp == 1u ? 1u : rnd_aralik(0u, (uint32_t)boy));
        if (kirp != 3u || rnd() % 2u == 0u) {
            memset(&yapi[sinir], 0, sizeof yapi - sinir);
        }
        if (rnd() % 8u == 0u) {
            yapi[rnd() % boy] = 0u;
        }
        size_t kap;
        const uint32_t ks = rnd() % 4u;
        if (ks == 0u) {
            kap = rnd_aralik(0u, IDA_LINK_CERCEVE_AZAMI);
        } else {
            kap = IDA_LINK_CERCEVE_AZAMI + 16u;
        }
        const uint8_t desen = rnd_bayt();
        memset(ce, desen, sizeof ce);
        memset(cy, desen, sizeof cy);
        size_t ye = 99u;
        size_t yy = 99u;
        const ida_link_rc_t re = ida_link_eski_gonder(&ge, msgid, yapi, boy, ce, kap, &ye);
        const ida_link_rc_t ry = ida_link_gonder(&gy, msgid, yapi, boy, cy, kap, &yy);
        g_karsilastirma++;
        TEST_ASSERT_EQUAL_INT(re, ry);
        TEST_ASSERT_EQUAL_size_t(ye, yy);
        TEST_ASSERT_EQUAL_MEMORY(ce, cy, sizeof ce);
        TEST_ASSERT_EQUAL_MEMORY(&ge, &gy, sizeof ge);
        if (re == IDA_LINK_TAMAM) {
            ++tamam;
            /* Kapasite tam gereken kadar olduğunda da aynı (sınır). */
            ge.seq = gy.seq;
            memset(ce, desen, sizeof ce);
            memset(cy, desen, sizeof cy);
            const size_t tam = ye;
            TEST_ASSERT_EQUAL_INT(ida_link_eski_gonder(&ge, msgid, yapi, boy, ce, tam, &ye),
                                  ida_link_gonder(&gy, msgid, yapi, boy, cy, tam, &yy));
            TEST_ASSERT_EQUAL_MEMORY(ce, cy, sizeof ce);
            TEST_ASSERT_EQUAL_INT(ida_link_eski_gonder(&ge, msgid, yapi, boy, ce, tam - 1u, &ye),
                                  ida_link_gonder(&gy, msgid, yapi, boy, cy, tam - 1u, &yy));
            TEST_ASSERT_EQUAL_size_t(ye, yy);
            TEST_ASSERT_EQUAL_MEMORY(ce, cy, sizeof ce);
            TEST_ASSERT_EQUAL_MEMORY(&ge, &gy, sizeof ge);
        }
    }
    TEST_ASSERT_TRUE(tamam > 0u);
    /* NULL argümanlar. */
    ida_link_gonderici_t ge;
    ida_link_gonderici_t gy;
    (void)ida_link_eski_gonderici_kur(&ge, IDA_LINK_ROL_SIM);
    (void)ida_link_gonderici_kur(&gy, IDA_LINK_ROL_SIM);
    size_t ye = 5u;
    size_t yy = 5u;
    TEST_ASSERT_EQUAL_INT(ida_link_eski_gonder(&ge, 46010u, yapi, 8u, ce, sizeof ce, NULL),
                          ida_link_gonder(&gy, 46010u, yapi, 8u, cy, sizeof cy, NULL));
    TEST_ASSERT_EQUAL_INT(ida_link_eski_gonder(NULL, 46010u, yapi, 8u, ce, sizeof ce, &ye),
                          ida_link_gonder(NULL, 46010u, yapi, 8u, cy, sizeof cy, &yy));
    TEST_ASSERT_EQUAL_size_t(ye, yy);
    TEST_ASSERT_EQUAL_INT(ida_link_eski_gonder(&ge, 46010u, NULL, 8u, ce, sizeof ce, &ye),
                          ida_link_gonder(&gy, 46010u, NULL, 8u, cy, sizeof cy, &yy));
    TEST_ASSERT_EQUAL_INT(ida_link_eski_gonder(&ge, 46010u, yapi, 8u, NULL, sizeof ce, &ye),
                          ida_link_gonder(&gy, 46010u, yapi, 8u, NULL, sizeof cy, &yy));
    TEST_ASSERT_EQUAL_size_t(ye, yy);
    ge.ozel_rol = gy.ozel_rol = 9u;
    TEST_ASSERT_EQUAL_INT(ida_link_eski_gonder(&ge, 46010u, yapi, 8u, ce, sizeof ce, &ye),
                          ida_link_gonder(&gy, 46010u, yapi, 8u, cy, sizeof cy, &yy));
    TEST_ASSERT_EQUAL_INT(ida_link_eski_gonderici_kur(NULL, IDA_LINK_ROL_SIM),
                          ida_link_gonderici_kur(NULL, IDA_LINK_ROL_SIM));
    TEST_ASSERT_EQUAL_INT(ida_link_eski_gonderici_kur(&ge, (ida_link_rol_t)257),
                          ida_link_gonderici_kur(&gy, (ida_link_rol_t)257));
    TEST_ASSERT_EQUAL_MEMORY(&ge, &gy, sizeof ge);
}

static void test_coz_ve_bilgi_rastgele(void)
{
    uint8_t me[512];
    uint8_t my[512];
    ida_link_cerceve_t c;
    for (unsigned long s = 0; s < 200ul * g_senaryo; ++s) {
        for (size_t i = 0; i < sizeof c; ++i) {
            ((uint8_t *)&c)[i] = rnd_bayt();
        }
        const ida_link_mesaj_bilgisi_t *b = &g_d.b[rnd() % g_d.n];
        const uint32_t secim = rnd() % 16u;
        uint32_t msgid = b->msgid;
        size_t boy = b->yapi_boyu;
        if (secim == 0u) {
            msgid = rnd_aralik(45990u, 46110u);
        } else if (secim == 1u) {
            boy = rnd_aralik(0u, IDA_LINK_YUK_AZAMI);
        }
        TEST_ASSERT_TRUE(boy <= sizeof me);
        c.msgid = secim == 2u ? g_d.b[rnd() % g_d.n].msgid : msgid;
        const uint8_t desen = rnd_bayt();
        memset(me, desen, sizeof me);
        memset(my, desen, sizeof my);
        g_karsilastirma++;
        TEST_ASSERT_EQUAL_INT(ida_link_eski_coz(&c, msgid, me, boy), ida_link_coz(&c, msgid, my, boy));
        TEST_ASSERT_EQUAL_MEMORY(me, my, sizeof me);
    }
    TEST_ASSERT_EQUAL_INT(ida_link_eski_coz(NULL, 46010u, me, 8u), ida_link_coz(NULL, 46010u, my, 8u));
    TEST_ASSERT_EQUAL_INT(ida_link_eski_coz(&c, 46010u, NULL, 8u), ida_link_coz(&c, 46010u, NULL, 8u));

    /* Bilgi: kimlik aralığının çevresi ve rastgele 32 bitlik kimlikler. */
    for (uint32_t k = 0; k < 4096u; ++k) {
        const uint32_t id = k < 1024u ? 45500u + k : (k < 1100u ? 0xFFFFFFFFu - k : rnd());
        ida_link_mesaj_bilgisi_t be;
        ida_link_mesaj_bilgisi_t by;
        memset(&be, 0x3C, sizeof be);
        memset(&by, 0x3C, sizeof by);
        TEST_ASSERT_EQUAL_INT(ida_link_eski_mesaj_bilgisi(id, &be), ida_link_mesaj_bilgisi(id, &by));
        TEST_ASSERT_EQUAL_MEMORY(&be, &by, sizeof be);
    }
    TEST_ASSERT_EQUAL_INT(ida_link_eski_mesaj_bilgisi(46010u, NULL),
                          ida_link_mesaj_bilgisi(46010u, NULL));
}

/* sol incelemesinin karşı örneği: 33 B'lık çerçeveyi 94 B'lık çerçeve izler; 64 B'lık
   bloklarla çağrı uzunlukları 64 (ilk çerçeve 33. baytta teslim), 31 (ilk bloğun kalanı),
   63 (ikinci blok, akışın sonu) olmalıdır: bir bloğun kalanı sonraki blokla birleşmez. */
static void test_blok_kalani_korunur(void)
{
    const ida_link_mesaj_bilgisi_t *b = &g_d.b[0];
    int bulundu = 0;
    for (unsigned k = 0; k < g_d.n; ++k) {
        if ((g_d.b[k].yon & (uint8_t)IDA_LINK_YON_SIM_KART) != 0u && g_d.b[k].yuk_azami >= 82u) {
            b = &g_d.b[k];
            bulundu = 1;
            break;
        }
    }
    TEST_ASSERT_TRUE(bulundu);
    uint8_t v[2u * IDA_LINK_CERCEVE_AZAMI];
    size_t n = cerceve_yaz(v, 21u, 0u, 0u, (uint8_t)IDA_LINK_SIM_SYSID,
                           (uint8_t)IDA_LINK_SIM_COMPID, b->msgid, b->crc_extra, 0);
    TEST_ASSERT_EQUAL_size_t(33u, n);
    n += cerceve_yaz(&v[n], 82u, 0u, 1u, (uint8_t)IDA_LINK_SIM_SYSID,
                     (uint8_t)IDA_LINK_SIM_COMPID, b->msgid, b->crc_extra, 0);
    TEST_ASSERT_EQUAL_size_t(127u, n);

    memset(&g_cagri_kaydi, 0, sizeof g_cagri_kaydi);
    g_cagri_kaydi.etkin = 1;
    const unsigned long long teslim_once = g_teslim;
    char a[192];
    const int sonuc = besle(v, n, IDA_LINK_ROL_KART, 0u, 64u, 0, a, sizeof a);
    g_cagri_kaydi.etkin = 0;
    TEST_ASSERT_EQUAL_INT_MESSAGE(0, sonuc, a);
    TEST_ASSERT_EQUAL_UINT32(2u, (uint32_t)(g_teslim - teslim_once));
    TEST_ASSERT_EQUAL_UINT32(2u, g_c.y.sayac.rx_frames);
    TEST_ASSERT_EQUAL_size_t(3u, g_cagri_kaydi.n);
    TEST_ASSERT_EQUAL_size_t(64u, g_cagri_kaydi.uzunluk[0]);
    TEST_ASSERT_EQUAL_size_t(31u, g_cagri_kaydi.uzunluk[1]);
    TEST_ASSERT_EQUAL_size_t(63u, g_cagri_kaydi.uzunluk[2]);
}

/* Duyarlılık: karşılaştırıcı farkı görür (bir bayt farklı akışlar; bir bayt farklı çerçeve). */
static void test_deneme_karsilastirici_farki_gorur(void)
{
    const ida_link_altin_t *g = &ida_link_altin[0];
    const ida_link_rol_t rol =
        g->sysid == IDA_LINK_SIM_SYSID ? IDA_LINK_ROL_KART : IDA_LINK_ROL_SIM;
    uint8_t bozuk[IDA_LINK_CERCEVE_AZAMI];
    memcpy(bozuk, g->bayt, g->cerceve);
    bozuk[g->cerceve - 1u] = (uint8_t)(bozuk[g->cerceve - 1u] ^ 0x01u);
    (void)ida_link_eski_alici_kur(&g_c.e, rol);
    (void)ida_link_alici_kur(&g_c.y, rol);
    size_t te = 0;
    size_t ty = 0;
    (void)ida_link_eski_al(&g_c.e, g->bayt, g->cerceve, &te, &g_c.ce);
    (void)ida_link_al(&g_c.y, bozuk, g->cerceve, &ty, &g_c.cy);
    const char *neden = "";
    TEST_ASSERT_EQUAL_INT(1, alici_farki(&g_c.e, &g_c.y, &neden));
    TEST_ASSERT_EQUAL_STRING("sayaclar", neden);
    /* Aynı iç durum, tek bayt farklı aday tamponu. */
    (void)ida_link_eski_alici_kur(&g_c.e, rol);
    (void)ida_link_alici_kur(&g_c.y, rol);
    (void)ida_link_eski_al(&g_c.e, g->bayt, 9u, &te, &g_c.ce);
    memcpy(bozuk, g->bayt, 9u);
    bozuk[8] = (uint8_t)(bozuk[8] ^ 0x01u);
    (void)ida_link_al(&g_c.y, bozuk, 9u, &ty, &g_c.cy);
    TEST_ASSERT_EQUAL_INT(1, alici_farki(&g_c.e, &g_c.y, &neden));
    TEST_ASSERT_EQUAL_STRING("aday tamponu", neden);
}

int main(int argc, char **argv)
{
    if (argc > 1) {
        const long v = strtol(argv[1], NULL, 10);
        if (v < 1 || v > 1000000L) {
            fprintf(stderr, "senaryo_sayisi 1..1000000 olmalı\n");
            return 2;
        }
        g_senaryo = (unsigned long)v;
    }
    dialect_kur();
    if (g_d.n == 0u) {
        fprintf(stderr, "dialect boş\n");
        return 1;
    }
    UNITY_BEGIN();
    RUN_TEST(test_crc_tablosu_crc_accumulate_ile_ayni);
    RUN_TEST(test_blok_kalani_korunur);
    RUN_TEST(test_altin_akis_her_sabit_blok_boyuyla);
    RUN_TEST(test_altin_cerceve_her_bolunme_noktasiyla);
    RUN_TEST(test_rastgele_akislar_her_bolunmeyle);
    RUN_TEST(test_tamamen_rastgele_baytlar);
    RUN_TEST(test_alici_argumanlari_ve_rol);
    RUN_TEST(test_gonderici_rastgele);
    RUN_TEST(test_coz_ve_bilgi_rastgele);
    RUN_TEST(test_deneme_karsilastirici_farki_gorur);
    printf("fark testi: %lu senaryo, %llu karşılaştırma, %llu teslim edilen çerçeve\n",
           g_senaryo, g_karsilastirma, g_teslim);
    return UNITY_END();
}
