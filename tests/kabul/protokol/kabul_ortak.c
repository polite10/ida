/*
 * kabul_ortak.c — ida_link bağımsız kabul testlerinin ortak yardımcıları (kabul_ortak.h).
 *
 * Referans kodlayıcı, doğrulayıcı ve ayrıştırıcı modeli yalnız A1 §3.2'deki tanımdan
 * yazılmıştır; kitaplığın kaynak kodu okunmadan.
 */
#include "kabul_ortak.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "unity.h"

/* ---- Sözde rastgele: splitmix64 ----------------------------------------------------------- */
void kabul_rng_kur(kabul_rng_t *r, uint64_t tohum)
{
    r->s = tohum;
}

uint32_t kabul_rng_u32(kabul_rng_t *r)
{
    uint64_t z = (r->s += UINT64_C(0x9E3779B97F4A7C15));
    z = (z ^ (z >> 30)) * UINT64_C(0xBF58476D1CE4E5B9);
    z = (z ^ (z >> 27)) * UINT64_C(0x94D049BB133111EB);
    z ^= z >> 31;
    return (uint32_t)(z >> 32);
}

uint32_t kabul_rng_aralik(kabul_rng_t *r, uint32_t n)
{
    return (uint32_t)(((uint64_t)kabul_rng_u32(r) * (uint64_t)n) >> 32);
}

uint8_t kabul_rng_bayt(kabul_rng_t *r)
{
    return (uint8_t)(kabul_rng_u32(r) >> 24);
}

uint8_t kabul_rng_temiz_bayt(kabul_rng_t *r)
{
    uint8_t b;
    do {
        b = kabul_rng_bayt(r);
    } while (b == 0xFDu || b == 0xFEu);
    return b;
}

/* ---- CRC-16/MCRF4XX ----------------------------------------------------------------------- */
/* poly 0x1021 yansıtılmış (0x8408), init 0xFFFF, yansıtılmış giriş/çıkış, son XOR 0. Bit bit
   yazıldı (A1 §3.2'deki parametrelerden; tablo ya da MAVLink'in "accumulate" biçimi
   kullanılmadı). */
uint16_t kabul_crc_bayt(uint16_t crc, uint8_t b)
{
    crc = (uint16_t)(crc ^ b);
    for (int i = 0; i < 8; ++i) {
        if (crc & 1u) {
            crc = (uint16_t)((crc >> 1) ^ 0x8408u);
        } else {
            crc = (uint16_t)(crc >> 1);
        }
    }
    return crc;
}

uint16_t kabul_crc(const uint8_t *p, size_t n, uint16_t crc)
{
    for (size_t i = 0; i < n; ++i) {
        crc = kabul_crc_bayt(crc, p[i]);
    }
    return crc;
}

/* ---- Tablo -------------------------------------------------------------------------------- */
const kabul_mesaj_t *kabul_mesaj_bul(uint32_t msgid)
{
    for (size_t i = 0; i < KABUL_MESAJ_SAYISI; ++i) {
        if (kabul_mesajlar[i].msgid == msgid) {
            return &kabul_mesajlar[i];
        }
    }
    return NULL;
}

const kabul_mesaj_t *kabul_mesaj_al(uint32_t msgid)
{
    const kabul_mesaj_t *m = kabul_mesaj_bul(msgid);
    if (m == NULL) {
        char ileti[96];
        snprintf(ileti, sizeof ileti, "msgid %lu sözleşme tablosunda yok", (unsigned long)msgid);
        TEST_FAIL_MESSAGE(ileti);
    }
    /* TEST_FAIL_MESSAGE dönmez; yine de dönüş hiçbir yolda NULL değildir (statik analiz). */
    return m != NULL ? m : &kabul_mesajlar[0];
}

uint8_t kabul_rol_alir_yon(ida_link_rol_t rol)
{
    return (uint8_t)(rol == IDA_LINK_ROL_KART ? KABUL_YON_SIM_KART : KABUL_YON_KART_SIM);
}

uint8_t kabul_rol_gonderir_yon(ida_link_rol_t rol)
{
    return (uint8_t)(rol == IDA_LINK_ROL_KART ? KABUL_YON_KART_SIM : KABUL_YON_SIM_KART);
}

/* A1 §3.1: sim 250/25; kart ve SIL koşucusu 1/1. Sayılar burada sözleşmeden yazılır;
   kitaplığın sabitleri kullanılmaz. */
uint8_t kabul_karsi_sysid(ida_link_rol_t rol)
{
    return (uint8_t)(rol == IDA_LINK_ROL_KART ? 250u : 1u);
}

uint8_t kabul_karsi_compid(ida_link_rol_t rol)
{
    return (uint8_t)(rol == IDA_LINK_ROL_KART ? 25u : 1u);
}

uint8_t kabul_kendi_sysid(ida_link_rol_t rol)
{
    return (uint8_t)(rol == IDA_LINK_ROL_KART ? 1u : 250u);
}

uint8_t kabul_kendi_compid(ida_link_rol_t rol)
{
    return (uint8_t)(rol == IDA_LINK_ROL_KART ? 1u : 25u);
}

/* ---- Referans çerçeve kurucu -------------------------------------------------------------- */
size_t kabul_cerceve_ham(uint8_t *cikis, uint8_t len, uint8_t incompat, uint8_t compat,
                         uint8_t seq, uint8_t sysid, uint8_t compid, uint32_t msgid,
                         const uint8_t *yuk, size_t yuk_boyu, uint8_t crc_extra)
{
    cikis[0] = 0xFDu;
    cikis[1] = len;
    cikis[2] = incompat;
    cikis[3] = compat;
    cikis[4] = seq;
    cikis[5] = sysid;
    cikis[6] = compid;
    cikis[7] = (uint8_t)(msgid & 0xFFu);
    cikis[8] = (uint8_t)((msgid >> 8) & 0xFFu);
    cikis[9] = (uint8_t)((msgid >> 16) & 0xFFu);
    for (size_t i = 0; i < len; ++i) {
        cikis[10u + i] = (uint8_t)((yuk != NULL && i < yuk_boyu) ? yuk[i] : 0u);
    }
    uint16_t crc = kabul_crc(&cikis[1], 9u + (size_t)len, 0xFFFFu);
    crc = kabul_crc_bayt(crc, crc_extra);
    cikis[10u + len] = (uint8_t)(crc & 0xFFu);
    cikis[11u + len] = (uint8_t)(crc >> 8);
    return 12u + (size_t)len;
}

size_t kabul_kirpilmis_boy(const uint8_t *yuk, size_t n)
{
    size_t boy = n;
    while (boy > 1u && yuk[boy - 1u] == 0u) {
        --boy;
    }
    return boy == 0u ? 1u : boy;
}

size_t kabul_cerceve_kur(uint8_t *cikis, uint32_t msgid, uint8_t seq, uint8_t sysid,
                         uint8_t compid, const uint8_t *yuk, size_t yuk_boyu, int kirp)
{
    const kabul_mesaj_t *m = kabul_mesaj_bul(msgid);
    if (m == NULL) {
        return 0u;
    }
    uint8_t tam[IDA_LINK_YUK_AZAMI];
    memset(tam, 0, sizeof tam);
    for (size_t i = 0; i < yuk_boyu && i < m->yuk_azami; ++i) {
        tam[i] = yuk[i];
    }
    size_t len = kirp ? kabul_kirpilmis_boy(tam, m->yuk_azami) : m->yuk_azami;
    return kabul_cerceve_ham(cikis, (uint8_t)len, 0u, 0u, seq, sysid, compid, msgid, tam, len,
                             m->crc_extra);
}

static int temiz_mi(const uint8_t *p, size_t n)
{
    for (size_t i = 1u; i < n; ++i) {
        if (p[i] == 0xFDu || p[i] == 0xFEu) {
            return 0;
        }
    }
    return 1;
}

static uint8_t sifirdan_farkli_temiz(kabul_rng_t *r)
{
    uint8_t b;
    do {
        b = kabul_rng_temiz_bayt(r);
    } while (b == 0u);
    return b;
}

size_t kabul_temiz_ham(uint8_t *cikis, kabul_rng_t *r, uint8_t len, uint8_t incompat,
                       uint8_t compat, uint8_t seq, uint8_t sysid, uint8_t compid, uint32_t msgid,
                       uint8_t crc_extra, size_t sifir_kuyruk)
{
    uint8_t yuk[IDA_LINK_YUK_AZAMI];
    size_t bulunan = 0u;
    for (int deneme = 0; deneme < 4096 && bulunan == 0u; ++deneme) {
        memset(yuk, 0, sizeof yuk);
        for (size_t i = 0; i + sifir_kuyruk < len; ++i) {
            yuk[i] = sifirdan_farkli_temiz(r);
        }
        size_t n = kabul_cerceve_ham(cikis, len, incompat, compat, seq, sysid, compid, msgid, yuk,
                                     len, crc_extra);
        if (temiz_mi(cikis, n)) {
            bulunan = n;
        }
    }
    TEST_ASSERT_TRUE_MESSAGE(bulunan != 0u,
                             "kabul_temiz_ham: temiz çerçeve kurulamadı (başlıkta 0xFD/0xFE var mı?)");
    return bulunan;
}

size_t kabul_temiz_kur(uint8_t *cikis, kabul_rng_t *r, uint32_t msgid, uint8_t seq,
                       uint8_t sysid, uint8_t compid, size_t dolu, int kirp, uint8_t *yuk_cikis)
{
    const kabul_mesaj_t *m = kabul_mesaj_al(msgid);
    if (dolu > m->yuk_azami) {
        dolu = m->yuk_azami;
    }
    uint8_t yuk[IDA_LINK_YUK_AZAMI];
    size_t bulunan = 0u;
    for (int deneme = 0; deneme < 4096 && bulunan == 0u; ++deneme) {
        memset(yuk, 0, sizeof yuk);
        for (size_t i = 0; i < dolu; ++i) {
            yuk[i] = sifirdan_farkli_temiz(r);
        }
        size_t n = kabul_cerceve_kur(cikis, msgid, seq, sysid, compid, yuk, m->yuk_azami, kirp);
        if (temiz_mi(cikis, n)) {
            bulunan = n;
        }
    }
    TEST_ASSERT_TRUE_MESSAGE(bulunan != 0u, "kabul_temiz_kur: temiz çerçeve kurulamadı");
    if (yuk_cikis != NULL) {
        memcpy(yuk_cikis, yuk, sizeof yuk);
    }
    return bulunan;
}

void kabul_yapi_hazirla(kabul_yapi_t *y, const kabul_mesaj_t *m, const uint8_t *yuk)
{
    TEST_ASSERT_TRUE_MESSAGE(m->yapi_boyu >= m->yuk_azami && m->yapi_boyu <= sizeof y->b,
                             "yapı boyu yük boyundan küçük ya da tampondan büyük");
    memset(y->b, 0xA5, sizeof y->b);
    memcpy(y->b, yuk, m->yuk_azami);
}

int kabul_cerceve_dogrula(const uint8_t *p, size_t n, kabul_cozulen_t *c)
{
    memset(c, 0, sizeof *c);
    if (n < 12u) {
        return -1;
    }
    if (p[0] != 0xFDu) {
        return -2;
    }
    c->len = p[1];
    c->incompat = p[2];
    c->compat = p[3];
    c->seq = p[4];
    c->sysid = p[5];
    c->compid = p[6];
    c->msgid = (uint32_t)p[7] | ((uint32_t)p[8] << 8) | ((uint32_t)p[9] << 16);
    const kabul_mesaj_t *m = kabul_mesaj_bul(c->msgid);
    if (m == NULL) {
        return -3;
    }
    if (c->len == 0u || c->len > m->yuk_azami) {
        return -4;
    }
    if (n != 12u + (size_t)c->len) {
        return -5;
    }
    uint16_t crc = kabul_crc(&p[1], 9u + (size_t)c->len, 0xFFFFu);
    crc = kabul_crc_bayt(crc, m->crc_extra);
    if (p[10u + c->len] != (uint8_t)(crc & 0xFFu) || p[11u + c->len] != (uint8_t)(crc >> 8)) {
        return -6;
    }
    memcpy(c->yuk, &p[10], c->len);
    return 0;
}

/* ---- Besleme ------------------------------------------------------------------------------ */
void kabul_sonuc_sifirla(kabul_sonuc_t *o)
{
    o->sayi = 0u;
    memset(&o->sayac, 0, sizeof o->sayac);
}

static void teslim_kaydet(const ida_link_cerceve_t *c, kabul_sonuc_t *out)
{
    char ileti[160];
    TEST_ASSERT_LESS_THAN_UINT32_MESSAGE(KABUL_TESLIM_AZAMI, (uint32_t)out->sayi,
                                         "kabul_besle: teslim kapasitesi doldu");
    const kabul_mesaj_t *m = kabul_mesaj_al(c->msgid); /* teslim edilen kimlik dialect'te olmalı */
    /* A1-K05: 1 <= len <= en büyük yük; eksik kısım sıfır (başlık: yuk[len, 255) sıfır). */
    snprintf(ileti, sizeof ileti, "msgid %lu: len %u geçersiz (azami %u)",
             (unsigned long)c->msgid, (unsigned)c->len, (unsigned)m->yuk_azami);
    TEST_ASSERT_TRUE_MESSAGE(c->len >= 1u && c->len <= m->yuk_azami, ileti);
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(m->yuk_azami, c->yuk_azami, "teslimin yuk_azami'si Tablo 3.3'ten farklı");
    for (size_t i = c->len; i < IDA_LINK_YUK_AZAMI; ++i) {
        if (c->yuk[i] != 0u) {
            snprintf(ileti, sizeof ileti, "msgid %lu: yuk[%u] sıfır değil (len %u)",
                     (unsigned long)c->msgid, (unsigned)i, (unsigned)c->len);
            TEST_FAIL_MESSAGE(ileti);
        }
    }
    kabul_teslim_t *t = &out->t[out->sayi++];
    t->msgid = c->msgid;
    t->seq = c->seq;
    t->sysid = c->sysid;
    t->compid = c->compid;
    t->len = c->len;
    t->yuk_azami = c->yuk_azami;
    memcpy(t->yuk, c->yuk, sizeof t->yuk);
}

kabul_parcalayici_t kabul_parcalayici(kabul_parca_turu_t tur, size_t k, uint64_t tohum)
{
    kabul_parcalayici_t p;
    memset(&p, 0, sizeof p);
    p.tur = tur;
    p.k = k;
    kabul_rng_kur(&p.rng, tohum);
    return p;
}

static size_t parca_boyu(kabul_parcalayici_t *p, size_t verilen, size_t kalan)
{
    switch (p->tur) {
    case KABUL_PARCA_SABIT:
        return p->k < kalan ? p->k : kalan;
    case KABUL_PARCA_RASTGELE: {
        size_t k = kabul_rng_aralik(&p->rng, (uint32_t)p->k + 1u);
        return k < kalan ? k : kalan;
    }
    case KABUL_PARCA_IKI:
        if (verilen < p->k) {
            size_t k = p->k - verilen;
            return k < kalan ? k : kalan;
        }
        return kalan;
    case KABUL_PARCA_TUMU:
    default:
        return kalan;
    }
}

void kabul_besle(ida_link_alici_t *a, const uint8_t *s, size_t n, kabul_parcalayici_t *p,
                 kabul_sonuc_t *out)
{
    static ida_link_cerceve_t c;
    size_t verilen = 0u;
    unsigned sifir_sayaci = 0u;
    while (verilen < n) {
        size_t k = parca_boyu(p, verilen, n - verilen);
        if (k == 0u) {
            /* Sıfır uzunluklu besleme (başlık: geçerli; veri NULL olabilir). */
            size_t t = 99u;
            const uint8_t *v = (sifir_sayaci++ & 1u) ? NULL : &s[verilen];
            ida_link_rc_t rc = ida_link_al(a, v, 0u, &t, &c);
            TEST_ASSERT_EQUAL_INT_MESSAGE(IDA_LINK_BEKLE, rc, "n = 0 beslemesi BEKLE döndürmedi");
            TEST_ASSERT_EQUAL_UINT32_MESSAGE(0u, (uint32_t)t, "n = 0 beslemesinde tuketilen 0 değil");
            continue;
        }
        size_t i = 0u;
        while (i < k) {
            size_t t = 0u;
            ida_link_rc_t rc = ida_link_al(a, &s[verilen + i], k - i, &t, &c);
            if (rc == IDA_LINK_CERCEVE) {
                TEST_ASSERT_TRUE_MESSAGE(t >= 1u && t <= k - i, "CERCEVE dönüşünde tuketilen sınır dışı");
                teslim_kaydet(&c, out);
                i += t;
            } else {
                TEST_ASSERT_EQUAL_INT_MESSAGE(IDA_LINK_BEKLE, rc, "ida_link_al beklenmeyen dönüş");
                TEST_ASSERT_EQUAL_UINT32_MESSAGE((uint32_t)(k - i), (uint32_t)t,
                                                 "BEKLE dönüşünde bütün baytlar tüketilmedi");
                i = k;
            }
        }
        verilen += k;
    }
    out->sayac = a->sayac;
}

void kabul_besle_yeni(ida_link_rol_t rol, const uint8_t *s, size_t n, kabul_sonuc_t *out)
{
    static ida_link_alici_t a;
    kabul_parcalayici_t p = {KABUL_PARCA_TUMU, 0u, {0u}};
    TEST_ASSERT_EQUAL_INT(IDA_LINK_TAMAM, ida_link_alici_kur(&a, rol));
    kabul_sonuc_sifirla(out);
    kabul_besle(&a, s, n, &p, out);
}

void kabul_sayac_esit(const ida_link_sayaclar_t *b, const ida_link_sayaclar_t *g,
                      const char *baglam)
{
    char ileti[200];
#define KABUL_SAYAC(ad)                                                                       \
    snprintf(ileti, sizeof ileti, "%s: sayaç " #ad " beklenen %lu, görülen %lu", baglam,      \
             (unsigned long)b->ad, (unsigned long)g->ad);                                     \
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(b->ad, g->ad, ileti)
    KABUL_SAYAC(rx_bytes);
    KABUL_SAYAC(rx_frames);
    KABUL_SAYAC(rx_crc_err);
    KABUL_SAYAC(rx_unknown_msgid);
    KABUL_SAYAC(rx_iflag);
    KABUL_SAYAC(rx_bad_len);
    KABUL_SAYAC(rx_wrong_sys);
    KABUL_SAYAC(rx_mav1);
    KABUL_SAYAC(rx_overflow);
    KABUL_SAYAC(rx_resync_bytes);
    KABUL_SAYAC(rx_seq_gap);
#undef KABUL_SAYAC
}

void kabul_sonuc_esit(const kabul_sonuc_t *b, const kabul_sonuc_t *g, const char *baglam)
{
    char ileti[200];
    snprintf(ileti, sizeof ileti, "%s: teslim sayısı", baglam);
    TEST_ASSERT_EQUAL_UINT32_MESSAGE((uint32_t)b->sayi, (uint32_t)g->sayi, ileti);
    for (size_t i = 0; i < b->sayi; ++i) {
        const kabul_teslim_t *x = &b->t[i];
        const kabul_teslim_t *y = &g->t[i];
        snprintf(ileti, sizeof ileti, "%s: %u. teslim (msgid %lu seq %u) farklı", baglam,
                 (unsigned)i, (unsigned long)x->msgid, (unsigned)x->seq);
        TEST_ASSERT_EQUAL_UINT32_MESSAGE(x->msgid, y->msgid, ileti);
        TEST_ASSERT_EQUAL_UINT8_MESSAGE(x->seq, y->seq, ileti);
        TEST_ASSERT_EQUAL_UINT8_MESSAGE(x->sysid, y->sysid, ileti);
        TEST_ASSERT_EQUAL_UINT8_MESSAGE(x->compid, y->compid, ileti);
        TEST_ASSERT_EQUAL_UINT8_MESSAGE(x->len, y->len, ileti);
        TEST_ASSERT_EQUAL_UINT8_MESSAGE(x->yuk_azami, y->yuk_azami, ileti);
        TEST_ASSERT_EQUAL_UINT8_ARRAY_MESSAGE(x->yuk, y->yuk, IDA_LINK_YUK_AZAMI, ileti);
    }
    kabul_sayac_esit(&b->sayac, &g->sayac, baglam);
}

uint32_t kabul_ret_toplami(const ida_link_sayaclar_t *s)
{
    return s->rx_crc_err + s->rx_unknown_msgid + s->rx_iflag + s->rx_bad_len + s->rx_wrong_sys +
           s->rx_mav1 + s->rx_overflow;
}

/* ---- Model -------------------------------------------------------------------------------- */
void kabul_model_ayristir(ida_link_rol_t rol, const uint8_t *s, size_t n, kabul_model_t *m)
{
    kabul_sonuc_sifirla(&m->sonuc);
    ida_link_sayaclar_t *c = &m->sonuc.sayac;
    c->rx_bytes = (uint32_t)n;
    int onceki_var = 0;
    uint8_t onceki = 0u;
    size_t i = 0u;
    while (i < n) {
        uint8_t b = s[i];
        if (b == 0xFEu) {
            /* A1-K02: aday dışındaki her 0xFE rx_mav1'e ve (aday başlatmadığı için)
               rx_resync_bytes'a sayılır; len'e göre atlama yoktur. */
            c->rx_mav1++;
            c->rx_resync_bytes++;
            i += 1u;
            continue;
        }
        if (b != 0xFDu) {
            c->rx_resync_bytes++;
            i += 1u;
            continue;
        }
        if (n - i < 10u) {
            break; /* bekleyen başlık */
        }
        uint8_t len = s[i + 1u];
        uint32_t id = (uint32_t)s[i + 7u] | ((uint32_t)s[i + 8u] << 8) | ((uint32_t)s[i + 9u] << 16);
        const kabul_mesaj_t *mm = kabul_mesaj_bul(id);
        /* A1-K07: başlıkta incompat_flags -> kimlik -> len; ilk tutmayan tek neden; tarama
           0xFD'den sonraki bayttan sürer. */
        if (s[i + 2u] != 0u) {
            c->rx_iflag++;
            i += 1u;
            continue;
        }
        if (mm == NULL) {
            c->rx_unknown_msgid++;
            i += 1u;
            continue;
        }
        if (len == 0u || len > mm->yuk_azami) {
            c->rx_bad_len++;
            i += 1u;
            continue;
        }
        size_t boy = 12u + (size_t)len;
        if (n - i < boy) {
            break; /* bekleyen aday */
        }
        uint16_t crc = kabul_crc(&s[i + 1u], 9u + (size_t)len, 0xFFFFu);
        crc = kabul_crc_bayt(crc, mm->crc_extra);
        if (s[i + 10u + len] != (uint8_t)(crc & 0xFFu) || s[i + 11u + len] != (uint8_t)(crc >> 8)) {
            c->rx_crc_err++; /* aday bütünüyle tüketilir */
            i += boy;
            continue;
        }
        if (s[i + 5u] != kabul_karsi_sysid(rol) || s[i + 6u] != kabul_karsi_compid(rol)) {
            c->rx_wrong_sys++;
            i += boy;
            continue;
        }
        if ((mm->yon & kabul_rol_alir_yon(rol)) == 0u) {
            c->rx_unknown_msgid++;
            i += boy;
            continue;
        }
        c->rx_frames++;
        uint8_t seq = s[i + 4u];
        if (onceki_var && seq != (uint8_t)(onceki + 1u)) {
            c->rx_seq_gap++;
        }
        onceki = seq;
        onceki_var = 1;
        TEST_ASSERT_LESS_THAN_UINT32(KABUL_TESLIM_AZAMI, (uint32_t)m->sonuc.sayi);
        kabul_teslim_t *t = &m->sonuc.t[m->sonuc.sayi++];
        t->msgid = id;
        t->seq = seq;
        t->sysid = s[i + 5u];
        t->compid = s[i + 6u];
        t->len = len;
        t->yuk_azami = mm->yuk_azami;
        memset(t->yuk, 0, sizeof t->yuk);
        memcpy(t->yuk, &s[i + 10u], len);
        i += boy;
    }
}

size_t kabul_temiz_kuyruk(uint8_t *s, size_t n, size_t kapasite, uint64_t tohum)
{
    kabul_rng_t r;
    kabul_rng_kur(&r, tohum);
    TEST_ASSERT_TRUE_MESSAGE(kapasite - n >= KABUL_KUYRUK, "kabul_temiz_kuyruk: yer yok");
    for (size_t k = 0; k < KABUL_KUYRUK; ++k) {
        s[n + k] = kabul_rng_temiz_bayt(&r);
    }
    return n + KABUL_KUYRUK;
}

void kabul_modelle_esit(ida_link_rol_t rol, const uint8_t *s, size_t n, const kabul_sonuc_t *gercek,
                        const char *baglam)
{
    static kabul_model_t model;
    kabul_model_ayristir(rol, s, n, &model);
    kabul_sonuc_esit(&model.sonuc, gercek, baglam);
}

ida_link_sayaclar_t kabul_sayac_sifir(void)
{
    ida_link_sayaclar_t z;
    memset(&z, 0, sizeof z);
    return z;
}

/* ---- Karşılaştırma ------------------------------------------------------------------------ */
void kabul_esit_u64(const char *ad, uint64_t beklenen, uint64_t gercek)
{
    if (beklenen != gercek) {
        char ileti[200];
        snprintf(ileti, sizeof ileti, "alan %s: beklenen %llu, görülen %llu", ad,
                 (unsigned long long)beklenen, (unsigned long long)gercek);
        TEST_FAIL_MESSAGE(ileti);
    }
}

void kabul_esit_i64(const char *ad, int64_t beklenen, int64_t gercek)
{
    if (beklenen != gercek) {
        char ileti[200];
        snprintf(ileti, sizeof ileti, "alan %s: beklenen %lld, görülen %lld", ad,
                 (long long)beklenen, (long long)gercek);
        TEST_FAIL_MESSAGE(ileti);
    }
}

void kabul_esit_f32(const char *ad, float beklenen, float gercek)
{
    uint32_t b;
    uint32_t g;
    memcpy(&b, &beklenen, sizeof b);
    memcpy(&g, &gercek, sizeof g);
    if (b != g) {
        char ileti[200];
        snprintf(ileti, sizeof ileti, "alan %s: beklenen %.9g (0x%08lx), görülen %.9g (0x%08lx)", ad,
                 (double)beklenen, (unsigned long)b, (double)gercek, (unsigned long)g);
        TEST_FAIL_MESSAGE(ileti);
    }
}

void kabul_bilgi(const char *bicim, ...)
{
    char ileti[512];
    va_list ap;
    va_start(ap, bicim);
    vsnprintf(ileti, sizeof ileti, bicim, ap);
    va_end(ap);
    TEST_MESSAGE(ileti);
}

/* ---- Akış üreteçleri ---------------------------------------------------------------------- */
const char *const KABUL_AKIS_AD[KABUL_AKIS_TUR_SAYISI] = {
    "düz rastgele", "0xFD yoğun", "rastgeleye gömülü", "temiz + bozuk", "karışık",
};

static const kabul_mesaj_t *rastgele_mesaj(kabul_rng_t *r, uint8_t yon_maskesi)
{
    for (;;) {
        const kabul_mesaj_t *m = &kabul_mesajlar[kabul_rng_aralik(r, KABUL_MESAJ_SAYISI)];
        if (m->yon & yon_maskesi) {
            return m;
        }
    }
}

static void gomulu_ekle(kabul_gomulu_t *g, size_t *sayi, size_t bas, size_t boy, uint32_t msgid,
                        uint8_t seq)
{
    if (g != NULL && *sayi < KABUL_GOMULU_AZAMI) {
        g[*sayi].bas = bas;
        g[*sayi].boy = boy;
        g[*sayi].msgid = msgid;
        g[*sayi].seq = seq;
        ++*sayi;
    }
}

size_t kabul_akis_uret(kabul_akis_turu_t tur, ida_link_rol_t rol, uint64_t tohum, uint8_t *s,
                       size_t kapasite, size_t hedef_boy, kabul_gomulu_t *gomulu, size_t *gomulu_sayi)
{
    kabul_rng_t r;
    kabul_rng_kur(&r, tohum);
    size_t gs = 0;
    size_t n = 0;
    if (hedef_boy > kapasite) {
        hedef_boy = kapasite;
    }
    const uint8_t alir = kabul_rol_alir_yon(rol);
    const uint8_t ks = kabul_karsi_sysid(rol);
    const uint8_t kc = kabul_karsi_compid(rol);
    uint8_t seq = 0;
    switch (tur) {
    case KABUL_AKIS_DUZ_RASTGELE:
        for (; n < hedef_boy; ++n) {
            s[n] = kabul_rng_bayt(&r);
        }
        break;
    case KABUL_AKIS_FD_YOGUN:
        for (; n < hedef_boy; ++n) {
            uint32_t z = kabul_rng_aralik(&r, 100u);
            s[n] = (uint8_t)(z < 12u ? 0xFDu : (z < 16u ? 0xFEu : kabul_rng_bayt(&r)));
        }
        break;
    case KABUL_AKIS_GOMULU_RASTGELE:
        while (n + 300u + IDA_LINK_CERCEVE_AZAMI < hedef_boy) {
            size_t g = kabul_rng_aralik(&r, 300u);
            for (size_t k = 0; k < g; ++k) {
                s[n++] = kabul_rng_bayt(&r);
            }
            const kabul_mesaj_t *m = rastgele_mesaj(&r, alir);
            uint8_t yuk[IDA_LINK_YUK_AZAMI] = {0};
            size_t dolu = kabul_rng_aralik(&r, (uint32_t)m->yuk_azami + 1u);
            for (size_t k = 0; k < dolu; ++k) {
                yuk[k] = kabul_rng_bayt(&r);
            }
            size_t boy = kabul_cerceve_kur(&s[n], m->msgid, seq, ks, kc, yuk, sizeof yuk, 1);
            gomulu_ekle(gomulu, &gs, n, boy, m->msgid, seq);
            n += boy;
            ++seq;
        }
        break;
    case KABUL_AKIS_TEMIZ_BOZUK:
        /* seq 0xFD'ye varmadan durur (temizlik: başlıkta 0xFD/0xFE yok). */
        while (n + 64u + IDA_LINK_CERCEVE_AZAMI < hedef_boy && seq < 250u) {
            size_t g = kabul_rng_aralik(&r, 4u) == 0u ? kabul_rng_aralik(&r, 64u) : 0u;
            for (size_t k = 0; k < g; ++k) {
                s[n++] = kabul_rng_temiz_bayt(&r);
            }
            const kabul_mesaj_t *m = rastgele_mesaj(&r, alir);
            size_t bas = n;
            size_t boy = kabul_temiz_kur(&s[n], &r, m->msgid, seq, ks, kc,
                                         1u + kabul_rng_aralik(&r, (uint32_t)m->yuk_azami), 1, NULL);
            uint32_t z = kabul_rng_aralik(&r, 100u);
            size_t len = boy - 12u;
            if (z < 10u) {
                /* Yük ya da CRC baytı bozulur (temiz kalır). */
                size_t i = n + 10u + kabul_rng_aralik(&r, (uint32_t)(len + 2u));
                uint8_t v;
                do {
                    v = kabul_rng_temiz_bayt(&r);
                } while (v == s[i]);
                s[i] = v;
                n += boy;
            } else if (z < 18u) {
                /* Sonu kesilir; başlık tam kalır. */
                n += 10u + kabul_rng_aralik(&r, (uint32_t)(boy - 10u));
            } else {
                gomulu_ekle(gomulu, &gs, bas, boy, m->msgid, seq);
                n += boy;
            }
            ++seq;
        }
        break;
    case KABUL_AKIS_KARISIK:
    default:
        while (n + 64u + 2u * IDA_LINK_CERCEVE_AZAMI < hedef_boy) {
            uint32_t z = kabul_rng_aralik(&r, 100u);
            const kabul_mesaj_t *m = rastgele_mesaj(&r, alir);
            uint8_t yuk[IDA_LINK_YUK_AZAMI] = {0};
            size_t dolu = kabul_rng_aralik(&r, (uint32_t)m->yuk_azami + 1u);
            for (size_t k = 0; k < dolu; ++k) {
                yuk[k] = kabul_rng_bayt(&r);
            }
            size_t bas = n;
            if (z < 40u) {
                size_t boy = kabul_cerceve_kur(&s[n], m->msgid, seq, ks, kc, yuk, sizeof yuk, 1);
                gomulu_ekle(gomulu, &gs, bas, boy, m->msgid, seq);
                n += boy;
            } else if (z < 48u) { /* sağlaması bozuk */
                size_t boy = kabul_cerceve_kur(&s[n], m->msgid, seq, ks, kc, yuk, sizeof yuk, 1);
                size_t j = n + 10u + kabul_rng_aralik(&r, (uint32_t)(boy - 10u));
                s[j] = (uint8_t)(s[j] ^ (1u + kabul_rng_aralik(&r, 255u)));
                n += boy;
            } else if (z < 54u) { /* kesik */
                size_t boy = kabul_cerceve_kur(&s[n], m->msgid, seq, ks, kc, yuk, sizeof yuk, 1);
                n += 1u + kabul_rng_aralik(&r, (uint32_t)(boy - 1u));
            } else if (z < 60u) { /* incompat */
                n += kabul_cerceve_ham(&s[n], m->yuk_azami, (uint8_t)(1u + kabul_rng_aralik(&r, 255u)), 0u,
                                       seq, ks, kc, m->msgid, yuk, sizeof yuk, m->crc_extra);
            } else if (z < 66u) { /* kötü len */
                uint8_t len = (uint8_t)(kabul_rng_aralik(&r, 2u) ? 0u
                    : m->yuk_azami + 1u + kabul_rng_aralik(&r, 255u - m->yuk_azami));
                n += kabul_cerceve_ham(&s[n], len, 0u, 0u, seq, ks, kc, m->msgid, yuk, sizeof yuk, m->crc_extra);
            } else if (z < 72u) { /* bilinmeyen kimlik */
                uint32_t id = 46000u + 71u + kabul_rng_aralik(&r, 29u);
                n += kabul_cerceve_ham(&s[n], (uint8_t)(1u + kabul_rng_aralik(&r, 60u)), 0u, 0u, seq, ks, kc,
                                       id, yuk, sizeof yuk, kabul_rng_bayt(&r));
            } else if (z < 78u) { /* yanlış kimlik */
                n += kabul_cerceve_kur(&s[n], m->msgid, seq, kabul_kendi_sysid(rol), kabul_kendi_compid(rol),
                                       yuk, sizeof yuk, 1);
            } else if (z < 84u) { /* yanlış yön */
                const kabul_mesaj_t *y = rastgele_mesaj(&r, kabul_rol_gonderir_yon(rol));
                if ((y->yon & alir) == 0u) {
                    n += kabul_cerceve_kur(&s[n], y->msgid, seq, ks, kc, yuk, sizeof yuk, 1);
                }
            } else if (z < 90u) { /* MAVLink 1 */
                uint8_t len = (uint8_t)kabul_rng_aralik(&r, 40u);
                s[n++] = 0xFEu;
                s[n++] = len;
                for (size_t k = 0; k < 4u + (size_t)len + 2u; ++k) {
                    s[n++] = kabul_rng_bayt(&r);
                }
            } else { /* rastgele ara baytlar */
                size_t g = kabul_rng_aralik(&r, 80u);
                for (size_t k = 0; k < g; ++k) {
                    s[n++] = kabul_rng_bayt(&r);
                }
            }
            ++seq;
        }
        break;
    }
    if (gomulu_sayi != NULL) {
        *gomulu_sayi = gs;
    }
    return n;
}
