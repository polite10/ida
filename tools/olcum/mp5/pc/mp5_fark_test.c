/*
 * W0-MP5a iyileştirme turu: ürün ile kopya çeşidinin farksal sınaması (host; atılabilir).
 *
 * Aynı programa ürün kaynağı (protocol/ida_link/src/tel.c, olduğu gibi) ve kopya çeşidi
 * (kopya/tel_iyi.c; açık işlevleri derleme tanımlarıyla v_ önekine yeniden adlandırılır) bağlanır.
 * Belirlenimci sözde rastgele senaryolarla ikisine aynı girdi verilir ve gözlenebilir her çıktı
 * karşılaştırılır:
 *  - alıcı: her ida_link_al çağrısının dönüşü, *tuketilen, teslim edilen çerçevenin bütün alanları
 *    ve yükü, çağrıdan sonra bütün sayaçlar; ida_link_alici_tasma'nın dönüşü ve sayaçları.
 *    Girdi: geçerli çerçeveler (iki yön, rastgele yük ve kırpma, rastgele seq), bozuk CRC, yanlış
 *    sysid/compid, incompat_flags, sıfır ve aşırı len, dialect dışı kimlik, yanlış yön, kesik
 *    çerçeveler, 0xFD/0xFE dahil gürültü; rastgele parça boylarıyla (1..300) besleme.
 *  - gönderici: ida_link_gonder'in dönüşü, *yazilan, yazılan baytlar ve seq; rastgele yapı içeriği
 *    (rastgele uzunlukta sıfır sonu), yanlış boy, yanlış yön, dar tampon.
 *  - ida_link_coz ve ida_link_mesaj_bilgisi: dönüş ve yazılan baytlar.
 * FARK_K = 1 ise (çeşit MP5_K=1) teslim edilen yük yalnız [0, yuk_azami) aralığında karşılaştırılır
 * (tek bilinçli fark: [yuk_azami, 255) yazılmaz); değilse 255 baytın tamamı.
 * Çıkış kodu 0: hiç fark yok. Kullanım: mp5t_fark_<ad> [senaryo_sayisi]
 */
#include "ida_link/tel.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef FARK_K
#define FARK_K 0
#endif

ida_link_rc_t v_ida_link_alici_kur(ida_link_alici_t *a, ida_link_rol_t rol);
ida_link_rc_t v_ida_link_al(ida_link_alici_t *a, const uint8_t *veri, size_t n, size_t *tuketilen,
                            ida_link_cerceve_t *cerceve);
ida_link_rc_t v_ida_link_alici_tasma(ida_link_alici_t *a);
ida_link_rc_t v_ida_link_gonderici_kur(ida_link_gonderici_t *g, ida_link_rol_t rol);
ida_link_rc_t v_ida_link_gonder(ida_link_gonderici_t *g, uint32_t msgid, const void *mesaj, size_t mesaj_boyu,
                                uint8_t *cikis, size_t kapasite, size_t *yazilan);
ida_link_rc_t v_ida_link_coz(const ida_link_cerceve_t *cerceve, uint32_t msgid, void *mesaj, size_t mesaj_boyu);
ida_link_rc_t v_ida_link_mesaj_bilgisi(uint32_t msgid, ida_link_mesaj_bilgisi_t *b);

static uint64_t rs = 0x9E3779B97F4A7C15u;
static uint32_t rnd(void)
{
    rs ^= rs << 13;
    rs ^= rs >> 7;
    rs ^= rs << 17;
    return (uint32_t)(rs >> 16);
}
static uint32_t ara(uint32_t n) { return n ? rnd() % n : 0u; }

static unsigned long fark_say, karsilastirma;
static void fark(const char *ne, unsigned long senaryo)
{
    if (fark_say < 20u) {
        fprintf(stderr, "FARK: %s (senaryo %lu)\n", ne, senaryo);
    }
    fark_say++;
}

static uint32_t kimlikler[64];
static unsigned kimlik_n;

static void kimlikleri_bul(void)
{
    for (uint32_t id = 46000u; id < 46100u; ++id) {
        ida_link_mesaj_bilgisi_t b;
        if (ida_link_mesaj_bilgisi(id, &b) == IDA_LINK_TAMAM) {
            kimlikler[kimlik_n++] = id;
        }
    }
}

/* Rastgele yapı içeriği: rastgele baytlar, rastgele uzunlukta sıfır sonu (kırpmayı sınar). */
static void yapi_doldur(uint8_t *y, size_t boy)
{
    const uint32_t kip = ara(4u);
    for (size_t i = 0; i < boy; ++i) {
        y[i] = kip == 0u ? 0u : (uint8_t)rnd();
    }
    if (kip >= 2u && boy > 0u) {
        const size_t sifir = ara((uint32_t)boy + 1u);
        memset(&y[boy - sifir], 0, sifir);
    }
}

/* Akışa bir öğe ekler; dönüş eklenen bayt sayısı. */
static size_t oge_ekle(uint8_t *akis, size_t bos, ida_link_gonderici_t *g_sim, ida_link_gonderici_t *g_kart)
{
    uint8_t c[IDA_LINK_CERCEVE_AZAMI + 8];
    size_t n = 0;
    const uint32_t tur = ara(16u);
    if (tur <= 9u) {
        /* geçerli çerçeve (çoğunlukla), sonra bozulabilir */
        const uint32_t id = kimlikler[ara(kimlik_n)];
        ida_link_mesaj_bilgisi_t b;
        (void)ida_link_mesaj_bilgisi(id, &b);
        ida_link_gonderici_t *g = (b.yon & IDA_LINK_YON_SIM_KART) ? g_sim : g_kart;
        if (b.yon == IDA_LINK_YON_IKI && ara(2u)) {
            g = g_kart;
        }
        if (ara(8u) == 0u) {
            g = (g == g_sim) ? g_kart : g_sim; /* yanlış yön / yanlış kimlik */
        }
        uint8_t y[256];
        yapi_doldur(y, b.yapi_boyu);
        if (ara(6u) == 0u) {
            g->seq = (uint8_t)rnd(); /* seq boşluğu */
        }
        if (ida_link_gonder(g, id, y, b.yapi_boyu, c, sizeof c, &n) != IDA_LINK_TAMAM) {
            n = 0;
        }
        const uint32_t boz = ara(12u);
        if (n > 0u && boz == 0u) {
            c[1u + ara((uint32_t)n - 1u)] ^= (uint8_t)(1u + ara(255u)); /* bir bayt bozulur */
        } else if (n > 0u && boz == 1u) {
            n = 1u + ara((uint32_t)n - 1u); /* kesik */
        } else if (n > 0u && boz == 2u) {
            c[2] = (uint8_t)(1u + ara(255u)); /* incompat_flags */
        } else if (n > 0u && boz == 3u) {
            c[1] = ara(2u) ? 0u : (uint8_t)(b.yuk_azami + 1u + ara(255u - b.yuk_azami)); /* len */
        } else if (n > 0u && boz == 4u) {
            c[5] = (uint8_t)rnd(); /* sysid */
        } else if (n > 0u && boz == 5u) {
            c[8] = (uint8_t)(c[8] + 1u); /* dialect dışı kimlik (46256+) */
        }
    } else if (tur <= 12u) {
        n = 1u + ara(24u); /* gürültü, 0xFD/0xFE yoğun */
        for (size_t i = 0; i < n; ++i) {
            const uint32_t k = ara(4u);
            c[i] = k == 0u ? 0xFDu : (k == 1u ? 0xFEu : (uint8_t)rnd());
        }
    } else {
        /* sahte başlık: 0xFD + rastgele 9 bayt */
        n = 10u;
        c[0] = 0xFDu;
        for (size_t i = 1; i < n; ++i) {
            c[i] = (uint8_t)rnd();
        }
    }
    if (n > bos) {
        n = bos;
    }
    memcpy(akis, c, n);
    return n;
}

static void sayac_karsilastir(const ida_link_alici_t *a, const ida_link_alici_t *v, unsigned long s)
{
    karsilastirma++;
    if (memcmp(&a->sayac, &v->sayac, sizeof a->sayac) != 0) {
        fark("sayaçlar", s);
    }
}

static void alici_senaryosu(unsigned long s)
{
    static uint8_t akis[8192];
    size_t n = 0;
    ida_link_gonderici_t g_sim, g_kart;
    (void)ida_link_gonderici_kur(&g_sim, IDA_LINK_ROL_SIM);
    (void)ida_link_gonderici_kur(&g_kart, IDA_LINK_ROL_KART);
    g_sim.seq = (uint8_t)rnd();
    g_kart.seq = (uint8_t)rnd();
    const size_t hedef = 64u + ara(4000u);
    while (n < hedef) {
        n += oge_ekle(&akis[n], sizeof akis - n, &g_sim, &g_kart);
    }
    const ida_link_rol_t rol = ara(2u) ? IDA_LINK_ROL_KART : IDA_LINK_ROL_SIM;
    static ida_link_alici_t a, v;
    if (ida_link_alici_kur(&a, rol) != v_ida_link_alici_kur(&v, rol)) {
        fark("alici_kur", s);
    }
    const uint32_t parca_kip = ara(4u); /* 0: bayt bayt, 1: küçük, 2: 64, 3: rastgele büyük */
    size_t i = 0;
    while (i < n) {
        size_t p = parca_kip == 0u ? 1u : (parca_kip == 1u ? 1u + ara(16u) : (parca_kip == 2u ? 64u : 1u + ara(300u)));
        if (p > n - i) {
            p = n - i;
        }
        size_t j = 0;
        while (j < p) {
            static ida_link_cerceve_t ca, cv;
            memset(&ca, 0xA5, sizeof ca);
            memset(&cv, 0xA5, sizeof cv);
            size_t ta = 12345u, tv = 54321u;
            const ida_link_rc_t ra = ida_link_al(&a, &akis[i + j], p - j, &ta, &ca);
            const ida_link_rc_t rv = v_ida_link_al(&v, &akis[i + j], p - j, &tv, &cv);
            karsilastirma++;
            if (ra != rv || ta != tv) {
                fark("al dönüşü / tüketilen", s);
                return;
            }
            if (ra == IDA_LINK_CERCEVE) {
                if (ca.msgid != cv.msgid || ca.seq != cv.seq || ca.sysid != cv.sysid || ca.compid != cv.compid ||
                    ca.len != cv.len || ca.yuk_azami != cv.yuk_azami) {
                    fark("çerçeve alanları", s);
                }
                const size_t kars = FARK_K ? ca.yuk_azami : IDA_LINK_YUK_AZAMI;
                if (memcmp(ca.yuk, cv.yuk, kars) != 0) {
                    fark("çerçeve yükü", s);
                }
                /* çözme */
                ida_link_mesaj_bilgisi_t ba, bv;
                const ida_link_rc_t ma = ida_link_mesaj_bilgisi(ca.msgid, &ba);
                const ida_link_rc_t mv = v_ida_link_mesaj_bilgisi(ca.msgid, &bv);
                /* alan alan (yapının dolgu baytları belirsizdir) */
                if (ma != mv || ba.msgid != bv.msgid || ba.crc_extra != bv.crc_extra ||
                    ba.yuk_azami != bv.yuk_azami || ba.yon != bv.yon || ba.yapi_boyu != bv.yapi_boyu) {
                    fark("mesaj_bilgisi", s);
                }
                uint8_t ya[256], yv[256];
                memset(ya, 0x77, sizeof ya);
                memset(yv, 0x77, sizeof yv);
                const ida_link_rc_t za = ida_link_coz(&ca, ca.msgid, ya, ba.yapi_boyu);
                const ida_link_rc_t zv = v_ida_link_coz(&cv, cv.msgid, yv, ba.yapi_boyu);
                if (za != zv || memcmp(ya, yv, sizeof ya) != 0) {
                    fark("çözme", s);
                }
            }
            sayac_karsilastir(&a, &v, s);
            j += ta;
            if (ta == 0u) {
                break;
            }
        }
        i += p;
        if (ara(97u) == 0u) {
            if (ida_link_alici_tasma(&a) != v_ida_link_alici_tasma(&v)) {
                fark("tasma", s);
            }
            sayac_karsilastir(&a, &v, s);
        }
    }
}

static void gonderici_senaryosu(unsigned long s)
{
    ida_link_gonderici_t a, v;
    const ida_link_rol_t rol = ara(2u) ? IDA_LINK_ROL_KART : IDA_LINK_ROL_SIM;
    if (ida_link_gonderici_kur(&a, rol) != v_ida_link_gonderici_kur(&v, rol)) {
        fark("gonderici_kur", s);
    }
    a.seq = v.seq = (uint8_t)rnd();
    for (int k = 0; k < 64; ++k) {
        const uint32_t id = ara(20u) == 0u ? 46000u + ara(200u) : kimlikler[ara(kimlik_n)];
        ida_link_mesaj_bilgisi_t b;
        size_t boy = 32u;
        if (ida_link_mesaj_bilgisi(id, &b) == IDA_LINK_TAMAM) {
            boy = b.yapi_boyu;
        }
        if (ara(20u) == 0u) {
            boy = ara(256u);
        }
        uint8_t y[256];
        yapi_doldur(y, sizeof y);
        if (boy < sizeof y) {
            uint8_t yy[256];
            memcpy(yy, y, sizeof y);
            const size_t sifir = ara((uint32_t)boy + 1u);
            memset(&yy[boy - sifir], 0, sifir);
            memcpy(y, yy, sizeof y);
        }
        const size_t kap = ara(10u) == 0u ? ara(IDA_LINK_CERCEVE_AZAMI) : IDA_LINK_CERCEVE_AZAMI;
        uint8_t ca[IDA_LINK_CERCEVE_AZAMI], cv[IDA_LINK_CERCEVE_AZAMI];
        memset(ca, 0x3C, sizeof ca);
        memset(cv, 0x3C, sizeof cv);
        size_t ya = 999u, yv = 888u;
        const ida_link_rc_t ra = ida_link_gonder(&a, id, y, boy, ca, kap, &ya);
        const ida_link_rc_t rv = v_ida_link_gonder(&v, id, y, boy, cv, kap, &yv);
        karsilastirma++;
        if (ra != rv || ya != yv || a.seq != v.seq || memcmp(ca, cv, sizeof ca) != 0) {
            fark("gonder", s);
        }
    }
}

int main(int argc, char **argv)
{
    const unsigned long senaryo = argc > 1 ? strtoul(argv[1], NULL, 10) : 20000u;
    kimlikleri_bul();
    for (unsigned long s = 0; s < senaryo; ++s) {
        alici_senaryosu(s);
        gonderici_senaryosu(s);
    }
    printf("fark sınaması (FARK_K=%d): %lu senaryo, %lu karşılaştırma, %lu fark\n", FARK_K, senaryo, karsilastirma,
           fark_say);
    return fark_say == 0u ? 0 : 1;
}
