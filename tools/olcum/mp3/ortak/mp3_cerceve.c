/* W0-MP3a ölçüm prototipi: test çerçevesi, CRC, belirlenimci veri, ayrıştırıcı, sıra izleyici (atılabilir). */
#include "mp3_cerceve.h"

#include <string.h>

/* ---------------- CRC ---------------- */

/* CRC-16/CCITT-FALSE: poly 0x1021, ilk değer 0xFFFF, yansıtmasız, son XOR yok. Kontrol: "123456789" -> 0x29B1 */
uint16_t mp3_crc16(const uint8_t *p, size_t n)
{
    uint16_t c = 0xFFFFu;
    for (size_t i = 0; i < n; i++) {
        c = (uint16_t)(c ^ (uint16_t)((uint16_t)p[i] << 8));
        for (int k = 0; k < 8; k++) {
            c = (c & 0x8000u) != 0u ? (uint16_t)((uint16_t)(c << 1) ^ 0x1021u) : (uint16_t)(c << 1);
        }
    }
    return c;
}

/* CRC-32/ISO-HDLC: yansıtılmış poly 0xEDB88320, ilk 0xFFFFFFFF, son XOR 0xFFFFFFFF. "123456789" -> 0xCBF43926 */
static uint32_t crc32_tablo[256];
static int crc32_hazir;

static void crc32_kur(void)
{
    for (uint32_t i = 0; i < 256u; i++) {
        uint32_t c = i;
        for (int k = 0; k < 8; k++) { c = (c & 1u) != 0u ? (c >> 1) ^ 0xEDB88320u : (c >> 1); }
        crc32_tablo[i] = c;
    }
    crc32_hazir = 1;
}

void mp3_hazirla(void)
{
    if (!crc32_hazir) { crc32_kur(); }
}

uint32_t mp3_crc32(const uint8_t *p, size_t n)
{
    if (!crc32_hazir) { crc32_kur(); }
    uint32_t c = 0xFFFFFFFFu;
    for (size_t i = 0; i < n; i++) { c = crc32_tablo[(c ^ p[i]) & 0xFFu] ^ (c >> 8); }
    return c ^ 0xFFFFFFFFu;
}

/* ---------------- küçük uçlu yardımcılar ---------------- */
void mp3_le16_yaz(uint8_t *p, uint16_t v) { p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); }
void mp3_le32_yaz(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)v; p[1] = (uint8_t)(v >> 8); p[2] = (uint8_t)(v >> 16); p[3] = (uint8_t)(v >> 24);
}
uint16_t mp3_le16(const uint8_t *p) { return (uint16_t)((uint16_t)p[0] | (uint16_t)((uint16_t)p[1] << 8)); }
uint32_t mp3_le32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

/* ---------------- kodlama ---------------- */
size_t mp3_kodla(uint8_t *o, size_t kapasite, uint8_t tur, uint32_t kosu, uint32_t sira, const uint8_t *yuk,
                 uint16_t uzunluk)
{
    const size_t top = (size_t)MP3_EK + uzunluk;
    if (uzunluk > MP3_AZAMI_YUK || kapasite < top) { return 0; }
    o[0] = MP3_ES0;
    o[1] = MP3_ES1;
    o[2] = (uint8_t)MP3_SURUM;
    o[3] = tur;
    mp3_le32_yaz(o + 4, kosu);
    mp3_le32_yaz(o + 8, sira);
    mp3_le16_yaz(o + 12, uzunluk);
    mp3_le16_yaz(o + 14, mp3_crc16(o, 14));
    if (uzunluk > 0u) { memcpy(o + MP3_BASLIK, yuk, uzunluk); }
    mp3_le32_yaz(o + MP3_BASLIK + uzunluk, mp3_crc32(o, MP3_BASLIK + (size_t)uzunluk));
    return top;
}

/* ---------------- belirlenimci veri ---------------- */
static uint32_t tohum(uint8_t tur, uint32_t kosu, uint32_t sira)
{
    uint32_t x = (kosu * 0x9E3779B1u) ^ (sira * 0x85EBCA77u) ^ ((uint32_t)tur * 0xC2B2AE3Du) ^ 0x6A09E667u;
    return x != 0u ? x : 1u;
}

static uint32_t xs32(uint32_t x)
{
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    return x;
}

void mp3_veri_uret(uint8_t *p, uint16_t n, uint8_t tur, uint32_t kosu, uint32_t sira)
{
    uint32_t x = tohum(tur, kosu, sira);
    for (uint16_t i = 0; i < n; i++) {
        if ((i & 3u) == 0u) { x = xs32(x); }
        p[i] = (uint8_t)(x >> (8u * (i & 3u)));
    }
}

int mp3_veri_dogru(const uint8_t *p, uint16_t n, uint8_t tur, uint32_t kosu, uint32_t sira)
{
    uint32_t x = tohum(tur, kosu, sira);
    for (uint16_t i = 0; i < n; i++) {
        if ((i & 3u) == 0u) { x = xs32(x); }
        if (p[i] != (uint8_t)(x >> (8u * (i & 3u)))) { return 0; }
    }
    return 1;
}

/* ---------------- denetim yükleri ---------------- */
void mp3_kur_yaz(uint8_t *p, const mp3_kur_t *k)
{
    mp3_le32_yaz(p + 0, k->baud);
    p[4] = k->mod;
    p[5] = 0u;
    mp3_le16_yaz(p + 6, k->yuk);
    mp3_le32_yaz(p + 8, k->kart_adet);
    mp3_le32_yaz(p + 12, k->aralik_ns);
    mp3_le16_yaz(p + 16, k->esle_ms);
    mp3_le16_yaz(p + 18, k->bos_ms);
}

void mp3_kur_oku(const uint8_t *p, mp3_kur_t *k)
{
    k->baud = mp3_le32(p + 0);
    k->mod = p[4];
    k->yuk = mp3_le16(p + 6);
    k->kart_adet = mp3_le32(p + 8);
    k->aralik_ns = mp3_le32(p + 12);
    k->esle_ms = mp3_le16(p + 16);
    k->bos_ms = mp3_le16(p + 18);
}

void mp3_kur_yanit_yaz(uint8_t *p, const mp3_kur_yanit_t *k)
{
    p[0] = k->kabul;
    p[1] = k->neden;
    p[2] = 0u;
    p[3] = 0u;
    mp3_le32_yaz(p + 4, k->baud);
    mp3_le32_yaz(p + 8, k->usart_hz);
    mp3_le32_yaz(p + 12, k->brr);
    mp3_le32_yaz(p + 16, k->over8);
    mp3_le32_yaz(p + 20, k->hesap_baud);
    mp3_le32_yaz(p + 24, (uint32_t)k->hata_ppm);
}

void mp3_kur_yanit_oku(const uint8_t *p, mp3_kur_yanit_t *k)
{
    k->kabul = p[0];
    k->neden = p[1];
    k->baud = mp3_le32(p + 4);
    k->usart_hz = mp3_le32(p + 8);
    k->brr = mp3_le32(p + 12);
    k->over8 = mp3_le32(p + 16);
    k->hesap_baud = mp3_le32(p + 20);
    k->hata_ppm = (int32_t)mp3_le32(p + 24);
}

static const char *const ozet_ad[MP3_OZET_N] = {
#define MP3_X_AD(a) #a,
    MP3_OZET_ALANLARI(MP3_X_AD)
#undef MP3_X_AD
};

const char *mp3_ozet_ad(unsigned i) { return i < (unsigned)MP3_OZET_N ? ozet_ad[i] : "?"; }

uint32_t mp3_ozet_deger(const mp3_ozet_t *o, unsigned i)
{
    unsigned k = 0;
#define MP3_X_DEGER(a) if (k++ == i) { return o->a; }
    MP3_OZET_ALANLARI(MP3_X_DEGER)
#undef MP3_X_DEGER
    return 0u;
}

void mp3_ozet_yaz(uint8_t *p, const mp3_ozet_t *o)
{
    mp3_le32_yaz(p, (uint32_t)MP3_OZET_N);
    uint8_t *q = p + 4;
#define MP3_X_YAZ(a) mp3_le32_yaz(q, o->a); q += 4;
    MP3_OZET_ALANLARI(MP3_X_YAZ)
#undef MP3_X_YAZ
}

int mp3_ozet_oku(const uint8_t *p, uint16_t n, mp3_ozet_t *o)
{
    if (n != MP3_OZET_BOY || mp3_le32(p) != (uint32_t)MP3_OZET_N) { return -1; }
    const uint8_t *q = p + 4;
#define MP3_X_OKU(a) o->a = mp3_le32(q); q += 4;
    MP3_OZET_ALANLARI(MP3_X_OKU)
#undef MP3_X_OKU
    return 0;
}

/* ---------------- ayrıştırıcı ---------------- */
void mp3_ayr_sayac_sifirla(mp3_ayr_t *a)
{
    a->iyi = a->atilan_bayt = a->baslik_hata = a->surum_hata = a->uzunluk_hata = a->crc_hata = a->tuketilen = 0u;
}

void mp3_ayr_sifirla(mp3_ayr_t *a)
{
    a->n = 0u;
    mp3_ayr_sayac_sifirla(a);
}

static void at(mp3_ayr_t *a, uint32_t k, int hata)
{
    if (k > a->n) { k = a->n; }
    memmove(a->b, a->b + k, a->n - k);
    a->n -= k;
    a->tuketilen += k;
    if (hata) { a->atilan_bayt += k; }
}

static void isle(mp3_ayr_t *a, mp3_cerceve_cb cb, void *ctx)
{
    for (;;) {
        if (a->n == 0u) { return; }
        if (a->b[0] != MP3_ES0) {
            const uint8_t *s = (const uint8_t *)memchr(a->b, (int)MP3_ES0, a->n);
            at(a, s != NULL ? (uint32_t)(s - a->b) : a->n, 1);
            continue;
        }
        if (a->n < 2u) { return; }
        if (a->b[1] != MP3_ES1) { at(a, 1u, 1); continue; }
        if (a->n < MP3_BASLIK) { return; }
        if (mp3_crc16(a->b, 14) != mp3_le16(a->b + 14)) { a->baslik_hata++; at(a, 1u, 1); continue; }
        if (a->b[2] != MP3_SURUM) { a->surum_hata++; at(a, 1u, 1); continue; }
        const uint16_t uz = mp3_le16(a->b + 12);
        if (uz > MP3_AZAMI_YUK) { a->uzunluk_hata++; at(a, 1u, 1); continue; }
        const uint32_t top = MP3_EK + (uint32_t)uz;
        if (a->n < top) { return; }
        if (mp3_crc32(a->b, MP3_BASLIK + (size_t)uz) != mp3_le32(a->b + MP3_BASLIK + uz)) {
            /* Yalnız bir bayt atılır: başlık kesik bir çerçeveye aitse ardından gelen sağlam çerçeveler
               iddia edilen uzunluğun içinde kalmış olabilir; yeniden arama onları bulur. */
            a->crc_hata++;
            at(a, 1u, 1);
            continue;
        }
        a->iyi++;
        mp3_cerceve_t c;
        c.tur = a->b[3];
        c.kosu = mp3_le32(a->b + 4);
        c.sira = mp3_le32(a->b + 8);
        c.uzunluk = uz;
        c.yuk = a->b + MP3_BASLIK;
        cb(ctx, &c);
        at(a, top, 0);
    }
}

void mp3_ayr_besle(mp3_ayr_t *a, const uint8_t *p, size_t n, mp3_cerceve_cb cb, void *ctx)
{
    while (n > 0u) {
        size_t k = sizeof a->b - a->n;   /* isle() dönünce n < sizeof b (bekleyen çerçeve tampona sığar) */
        if (k > n) { k = n; }
        memcpy(a->b + a->n, p, k);
        a->n += (uint32_t)k;
        p += k;
        n -= k;
        isle(a, cb, ctx);
    }
}

/* ---------------- sıra izleyici ---------------- */
static void bit_yaz(mp3_sira_t *s, uint32_t q, int v)
{
    const uint32_t i = q % MP3_PENCERE;
    if (v) { s->bit[i / 32u] |= 1u << (i % 32u); } else { s->bit[i / 32u] &= ~(1u << (i % 32u)); }
}

static int bit_oku(const mp3_sira_t *s, uint32_t q)
{
    const uint32_t i = q % MP3_PENCERE;
    return (s->bit[i / 32u] >> (i % 32u)) & 1u;
}

void mp3_sira_sifirla(mp3_sira_t *s) { memset(s, 0, sizeof *s); }

int mp3_sira_isle(mp3_sira_t *s, uint32_t q)
{
    if (q >= s->beklenen) {
        const uint32_t bosluk = q - s->beklenen;
        if (bosluk >= MP3_PENCERE) {
            memset(s->bit, 0, sizeof s->bit);
        } else {
            for (uint32_t k = s->beklenen; k != q; k++) { bit_yaz(s, k, 0); }
        }
        bit_yaz(s, q, 1);
        s->eksik += bosluk;
        s->alinan++;
        s->beklenen = q + 1u;
        return bosluk != 0u ? MP3_SIRA_BOSLUK : MP3_SIRA_YENI;
    }
    if (s->beklenen - q > MP3_PENCERE) { s->eski++; return MP3_SIRA_ESKI; }
    if (bit_oku(s, q)) { s->tekrar++; return MP3_SIRA_TEKRAR; }
    bit_yaz(s, q, 1);
    s->sirasiz++;
    if (s->eksik > 0u) { s->eksik--; }
    s->alinan++;
    return MP3_SIRA_GEC;
}

/* ---------------- baud ---------------- */
uint32_t mp3_brr_baud(uint32_t usart_hz, uint32_t brr, uint32_t over8)
{
    uint64_t pay;
    uint32_t d;
    if (over8 != 0u) {
        if ((brr & 0x8u) != 0u) { return 0u; }
        d = (brr & 0xFFF0u) | ((brr & 0x7u) << 1);
        pay = 2u * (uint64_t)usart_hz;
    } else {
        d = brr;
        pay = usart_hz;
    }
    if (d < 16u || d > 0xFFFFu) { return 0u; }
    return (uint32_t)((pay + d / 2u) / d);
}

static uint32_t mutlak_fark(uint32_t a, uint32_t b) { return a > b ? a - b : b - a; }

int mp3_baud_hesapla(uint32_t usart_hz, uint32_t baud, mp3_baud_t *b)
{
    memset(b, 0, sizeof *b);
    b->baud = baud;
    b->usart_hz = usart_hz;
    if (usart_hz == 0u || baud == 0u) { return -1; }
    const uint64_t f = usart_hz;
    uint32_t aday_d[4], aday_o8[4];
    /* OVER16 */
    aday_d[0] = (uint32_t)(f / baud);
    aday_d[1] = (uint32_t)((f + baud - 1u) / baud);
    aday_o8[0] = aday_o8[1] = 0u;
    /* OVER8: çift bölücüler */
    const uint64_t d8 = (2u * f) / baud;
    aday_d[2] = (uint32_t)(d8 & ~(uint64_t)1u);
    aday_d[3] = aday_d[2] + 2u;
    aday_o8[2] = aday_o8[3] = 1u;
    int bulundu = 0;
    uint32_t en_hata = 0u;
    for (unsigned i = 0; i < 4u; i++) {
        const uint32_t d = aday_d[i];
        if (d < 16u || d > 0xFFFFu) { continue; }
        const uint32_t brr = aday_o8[i] ? ((d & 0xFFF0u) | ((d & 0x000Fu) >> 1u)) : d;
        const uint32_t h = mp3_brr_baud(usart_hz, brr, aday_o8[i]);
        if (h == 0u) { continue; }
        const uint32_t e = mutlak_fark(h, baud);
        if (!bulundu || e < en_hata) {   /* eşitlikte önceki (OVER16 ve küçük bölücü) korunur */
            bulundu = 1;
            en_hata = e;
            b->over8 = aday_o8[i];
            b->brr = brr;
            b->hesap_baud = h;
        }
    }
    if (!bulundu) { return -1; }
    b->hata_ppm = (int32_t)(((int64_t)b->hesap_baud - (int64_t)baud) * 1000000 / (int64_t)baud);
    return 0;
}
