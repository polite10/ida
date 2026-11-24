/*
 * param.c — parametre tablosu, şema özeti, kodlanmış blok ve örneksiz port işlevleri
 * (W2; A4 §3.9, §3.10, A4-K79, K80, K85, 4.15).
 */
#include "param/param.h"

#include "atis/atis.h"
#include "cekirdek.h"
#include "cerceve/cerceve.h"
#include "gorev/gorev.h"
#include "gudum/gudum.h"
#include "kestirim/kestirim.h"
#include "kontrol/kontrol.h"
#include "mat/ida_math.h"
#include "param/ozet.h"
#include "sonar/sonar.h"

/* ---- Tablo ------------------------------------------------------------------------------ */

#define IDA_PRM_DEGER_U32(kimlik, x) {.id = (kimlik), .type = IDA_PT_U32, .v = {.u32 = (x)}}
#define IDA_PRM_DEGER_I32(kimlik, x) {.id = (kimlik), .type = IDA_PT_I32, .v = {.i32 = (x)}}
#define IDA_PRM_DEGER_F32(kimlik, x) {.id = (kimlik), .type = IDA_PT_F32, .v = {.f32 = (x)}}

/* İşaretçisiz salt okunur tablo: adlar gömülü dizidir (A4-K22; yazılabilir bölüm yok). */
static const ida_param_desc_t TABLO[IDA_PRM_SAYI] = {
#define IDA_PARAM(kimlik, ad, tip, sinif, en_az, en_cok, varsayilan, alan)                          \
    {.id = (kimlik),                                                                               \
     .type = IDA_PT_##tip,                                                                         \
     .cls = IDA_PCLS_##sinif,                                                                      \
     .name = #ad,                                                                                  \
     .min = IDA_PRM_DEGER_##tip(kimlik, en_az),                                                    \
     .max = IDA_PRM_DEGER_##tip(kimlik, en_cok),                                                   \
     .def = IDA_PRM_DEGER_##tip(kimlik, varsayilan)},
#include "param/param_listesi.inc"
#undef IDA_PARAM
};

typedef union {
    uint32_t u;
    int32_t i;
    float f;
} ida_bit32_t;

static uint32_t deger_ham(const ida_param_value_t *v)
{
    ida_bit32_t b;
    b.u = 0u;
    switch (v->type) {
    case IDA_PT_I32:
        b.i = v->v.i32;
        break;
    case IDA_PT_F32:
        b.f = v->v.f32;
        break;
    default:
        b.u = v->v.u32;
        break;
    }
    return b.u;
}

void ida_prm_deger_yaz(const ida_param_desc_t *d, uint32_t ham, ida_param_value_t *v)
{
    ida_bit32_t b;
    b.u = ham;
    v->id = d->id;
    v->type = d->type;
    switch (d->type) {
    case IDA_PT_I32:
        v->v.i32 = b.i;
        break;
    case IDA_PT_F32:
        v->v.f32 = b.f;
        break;
    default:
        v->v.u32 = b.u;
        break;
    }
}

uint16_t ida_prm_sayi(void)
{
    return (uint16_t)IDA_PRM_SAYI;
}

const ida_param_desc_t *ida_prm_tanim(uint16_t indis)
{
    if (indis >= (uint16_t)IDA_PRM_SAYI) {
        return NULL;
    }
    return &TABLO[indis];
}

bool ida_prm_indis(uint16_t kimlik, uint16_t *indis)
{
    for (uint16_t i = 0u; i < (uint16_t)IDA_PRM_SAYI; i++) {
        if (TABLO[i].id == kimlik) {
            *indis = i;
            return true;
        }
    }
    return false;
}

void ida_prm_varsayilan(ida_prm_kume_t *k)
{
    for (uint16_t i = 0u; i < (uint16_t)IDA_PRM_SAYI; i++) {
        k->ham[i] = deger_ham(&TABLO[i].def);
    }
}

bool ida_prm_deger_gecerli(const ida_param_desc_t *d, uint32_t ham)
{
    ida_bit32_t b;
    b.u = ham;
    switch (d->type) {
    case IDA_PT_I32:
        return (b.i >= d->min.v.i32) && (b.i <= d->max.v.i32);
    case IDA_PT_F32:
        return ida_sonlu(b.f) && (b.f >= d->min.v.f32) && (b.f <= d->max.v.f32);
    case IDA_PT_U32:
        return (b.u >= d->min.v.u32) && (b.u <= d->max.v.u32);
    default:
        return false;
    }
}

uint32_t ida_prm_u32(const ida_param_t *p, enum ida_prm_indis indis)
{
    return p->yururluk.ham[indis];
}

int32_t ida_prm_i32(const ida_param_t *p, enum ida_prm_indis indis)
{
    ida_bit32_t b;
    b.u = p->yururluk.ham[indis];
    return b.i;
}

float ida_prm_f32(const ida_param_t *p, enum ida_prm_indis indis)
{
    ida_bit32_t b;
    b.u = p->yururluk.ham[indis];
    return b.f;
}

/* ---- Şema özeti (A4 §3.10) --------------------------------------------------------------- */

static void ozet_ondalik(ida_sha256_t *s, uint32_t x)
{
    uint8_t rakam[10] = {0u};
    uint32_t n = 0u;
    uint32_t kalan = x;
    do {
        rakam[n] = (uint8_t)('0' + (uint8_t)(kalan % 10u));
        n++;
        kalan /= 10u;
    } while (kalan != 0u);
    while (n > 0u) {
        n--;
        ida_sha256_ekle(s, &rakam[n], 1u);
    }
}

uint64_t ida_prm_sema_ozeti_tablo(const ida_param_desc_t *tablo, uint16_t sayi)
{
    ida_sha256_t s;
    ida_sha256_basla(&s);
    const uint8_t sekme = (uint8_t)'\t';
    const uint8_t satir = (uint8_t)'\n';
    for (uint16_t i = 0u; i < sayi; i++) {
        const ida_param_desc_t *d = &tablo[i];
        ozet_ondalik(&s, d->id);
        ida_sha256_ekle(&s, &sekme, 1u);
        for (uint32_t j = 0u; (j < IDA_PARAM_NAME_MAX) && (d->name[j] != '\0'); j++) {
            const uint8_t ch = (uint8_t)d->name[j];
            ida_sha256_ekle(&s, &ch, 1u);
        }
        ida_sha256_ekle(&s, &sekme, 1u);
        ozet_ondalik(&s, d->type);
        ida_sha256_ekle(&s, &satir, 1u);
    }
    uint8_t ozet[32];
    ida_sha256_bitir(&s, ozet);
    uint64_t sonuc = 0u;
    for (uint32_t i = 0u; i < 8u; i++) {
        sonuc |= (uint64_t)ozet[i] << (8u * i); /* ilk 8 bayt, küçük uçlu */
    }
    return sonuc;
}

uint64_t ida_prm_sema_ozeti(void)
{
    return ida_prm_sema_ozeti_tablo(TABLO, (uint16_t)IDA_PRM_SAYI);
}

void ida_prm_kur(ida_param_t *p)
{
    p->sema_ozeti = ida_prm_sema_ozeti(); /* tablo derlemede sabit: çağrı başına yeniden yok */
    p->sema_ozeti_tumleyen = UINT64_MAX ^ p->sema_ozeti;
}

bool ida_prm_ozet_al(const ida_param_t *p, uint64_t *ozet)
{
    /* Her alan tam bir kez okunur (volatile): denetlenen değer ile kullanılan değer aynıdır. */
    const volatile uint64_t *o = &p->sema_ozeti;
    const volatile uint64_t *t = &p->sema_ozeti_tumleyen;
    const uint64_t okunan = *o;
    const uint64_t tumleyen = *t;
    *ozet = okunan;
    return tumleyen == (UINT64_MAX ^ okunan);
}

bool ida_prm_tutarli(const ida_param_t *p)
{
    uint64_t ozet = 0u;
    return ida_prm_ozet_al(p, &ozet);
}

/* ---- Grup denetimleri (A4-K80 md. 8) ----------------------------------------------------- */

uint8_t ida_prm_grup(const ida_prm_kume_t *k)
{
    if (ida_cer_param_grup(k) != 0u) {
        return 1u;
    }
    if ((ida_kes_param_grup(k) != 0u) || (ida_gud_param_grup(k) != 0u) ||
        (ida_gor_param_grup(k) != 0u) || (ida_kon_param_grup(k) != 0u) ||
        (ida_fc_param_grup(k) != 0u) || (ida_son_param_grup(k) != 0u)) {
        return 1u;
    }
    return 0u;
}

/* ---- Kodlanmış blok (A4 §3.10) ----------------------------------------------------------- */

static uint32_t oku16(const uint8_t *b)
{
    return (uint32_t)b[0] | ((uint32_t)b[1] << 8);
}

static uint32_t oku32(const uint8_t *b)
{
    return (uint32_t)b[0] | ((uint32_t)b[1] << 8) | ((uint32_t)b[2] << 16) | ((uint32_t)b[3] << 24);
}

static uint64_t oku64(const uint8_t *b)
{
    return (uint64_t)oku32(b) | ((uint64_t)oku32(&b[4]) << 32);
}

static void yaz16(uint8_t *b, uint32_t x)
{
    b[0] = (uint8_t)x;
    b[1] = (uint8_t)(x >> 8);
}

static void yaz32(uint8_t *b, uint32_t x)
{
    b[0] = (uint8_t)x;
    b[1] = (uint8_t)(x >> 8);
    b[2] = (uint8_t)(x >> 16);
    b[3] = (uint8_t)(x >> 24);
}

uint8_t ida_prm_blok_coz(const uint8_t *blok, uint32_t uzunluk, uint64_t sema_ozeti,
                         ida_prm_kume_t *k)
{
    /* (1) başlık okunabilir, magic, fmt_ver = 1, rsv = 0 */
    if ((blok == NULL) || (uzunluk < IDA_PBLOB_HDR_SIZE)) {
        return 1u;
    }
    if ((oku32(&blok[0]) != IDA_PBLOB_MAGIC) || (oku16(&blok[4]) != IDA_PBLOB_FMT_VER) ||
        (oku16(&blok[6]) != 0u)) {
        return 1u;
    }
    /* (2) şema özeti */
    if (oku64(&blok[8]) != sema_ozeti) {
        return 2u;
    }
    /* (3) boy */
    const uint32_t yuk = oku32(&blok[16]);
    if (((uint64_t)IDA_PBLOB_HDR_SIZE + (uint64_t)yuk != (uint64_t)uzunluk) ||
        (uzunluk > IDA_PARAM_BLOB_MAX) || ((yuk % IDA_PBLOB_REC_SIZE) != 0u)) {
        return 3u;
    }
    /* (4) CRC */
    if (ida_crc32(&blok[IDA_PBLOB_HDR_SIZE], yuk) != oku32(&blok[20])) {
        return 4u;
    }
    /* (5) kesin artan param_id ve kayıt rsv = 0 */
    const uint32_t kayit_sayisi = yuk / IDA_PBLOB_REC_SIZE;
    for (uint32_t r = 0u; r < kayit_sayisi; r++) {
        const uint8_t *kay = &blok[IDA_PBLOB_HDR_SIZE + (r * IDA_PBLOB_REC_SIZE)];
        if (kay[3] != 0u) {
            return 5u;
        }
        if ((r > 0u) && (oku16(kay) <= oku16(kay - IDA_PBLOB_REC_SIZE))) {
            return 5u;
        }
    }
    /* (6) şemadaki her parametre tam bir kez, doğru type ile (iki dizi de artan sıralı) */
    if (kayit_sayisi != (uint32_t)IDA_PRM_SAYI) {
        return 6u;
    }
    for (uint32_t r = 0u; r < kayit_sayisi; r++) {
        const uint8_t *kay = &blok[IDA_PBLOB_HDR_SIZE + (r * IDA_PBLOB_REC_SIZE)];
        if ((oku16(kay) != TABLO[r].id) || (kay[2] != TABLO[r].type)) {
            return 6u;
        }
    }
    /* (7) aralık */
    ida_prm_kume_t aday;
    ida_prm_varsayilan(&aday); /* her eleman aşağıda yazılır; ilk değer derleyici için */
    for (uint32_t r = 0u; r < kayit_sayisi; r++) {
        const uint8_t *kay = &blok[IDA_PBLOB_HDR_SIZE + (r * IDA_PBLOB_REC_SIZE)];
        aday.ham[r] = oku32(&kay[4]);
        if (!ida_prm_deger_gecerli(&TABLO[r], aday.ham[r])) {
            return 7u;
        }
    }
    /* (8) grup denetimleri */
    if (ida_prm_grup(&aday) != 0u) {
        return 8u;
    }
    *k = aday;
    return 0u;
}

uint32_t ida_prm_blok_kodla(const ida_prm_kume_t *k, uint64_t sema_ozeti, uint8_t *blok)
{
    return ida_prm_blok_kodla_tablo(TABLO, (uint16_t)IDA_PRM_SAYI, k->ham, sema_ozeti, blok);
}

uint32_t ida_prm_blok_kodla_tablo(const ida_param_desc_t *tablo, uint16_t sayi,
                                  const uint32_t *ham, uint64_t sema_ozeti, uint8_t *blok)
{
    const uint32_t yuk = IDA_PBLOB_REC_SIZE * (uint32_t)sayi;
    yaz32(&blok[0], IDA_PBLOB_MAGIC);
    yaz16(&blok[4], IDA_PBLOB_FMT_VER);
    yaz16(&blok[6], 0u);
    yaz32(&blok[8], (uint32_t)sema_ozeti);
    yaz32(&blok[12], (uint32_t)(sema_ozeti >> 32));
    yaz32(&blok[16], yuk);
    for (uint32_t r = 0u; r < (uint32_t)sayi; r++) {
        uint8_t *kay = &blok[IDA_PBLOB_HDR_SIZE + (r * IDA_PBLOB_REC_SIZE)];
        yaz16(kay, tablo[r].id);
        kay[2] = tablo[r].type;
        kay[3] = 0u;
        yaz32(&kay[4], ham[r]);
    }
    yaz32(&blok[20], ida_crc32(&blok[IDA_PBLOB_HDR_SIZE], yuk));
    return IDA_PBLOB_HDR_SIZE + yuk;
}

/* ---- Port işlevleri (A4-K85; 4.15) ------------------------------------------------------- */

uint16_t ida_core_param_count(void)
{
    return ida_prm_sayi();
}

ida_rc_t ida_core_param_desc(uint16_t index, ida_param_desc_t *d)
{
    if (d == NULL) {
        return IDA_RC_BAD_INPUT;
    }
    const ida_param_desc_t *t = ida_prm_tanim(index);
    if (t == NULL) {
        return IDA_RC_NOT_FOUND;
    }
    *d = *t;
    return IDA_RC_OK;
}

ida_rc_t ida_core_param_find(const char *name, uint16_t *id)
{
    if ((name == NULL) || (id == NULL)) {
        return IDA_RC_BAD_INPUT;
    }
    /* En çok 17 bayt okunur: 16 karakter + NUL. */
    uint32_t uzunluk = 0u;
    while ((uzunluk <= IDA_PARAM_NAME_MAX) && (name[uzunluk] != '\0')) {
        uzunluk++;
    }
    if (uzunluk > IDA_PARAM_NAME_MAX) {
        return IDA_RC_NOT_FOUND;
    }
    for (uint16_t i = 0u; i < (uint16_t)IDA_PRM_SAYI; i++) {
        uint32_t j = 0u;
        while ((j < uzunluk) && (TABLO[i].name[j] == name[j])) {
            j++;
        }
        if ((j == uzunluk) && (TABLO[i].name[j] == '\0')) {
            *id = TABLO[i].id;
            return IDA_RC_OK;
        }
    }
    return IDA_RC_NOT_FOUND;
}

ida_rc_t ida_core_param_get(const ida_core_t *c, uint16_t id, ida_param_value_t *v)
{
    if ((c == NULL) || (v == NULL)) {
        return IDA_RC_BAD_INPUT;
    }
    if (!ida_cer_kurulu(c)) {
        return IDA_RC_NOT_INIT;
    }
    uint16_t indis = 0u;
    if (!ida_prm_indis(id, &indis)) {
        return IDA_RC_NOT_FOUND;
    }
    ida_prm_deger_yaz(&TABLO[indis], c->prm.yururluk.ham[indis], v);
    return IDA_RC_OK;
}
