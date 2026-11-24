/* kalici.c — açılış sayacı ve parametre A/B blokları (kalici.h). */
#include "kalici.h"

#include <string.h>

#include "ida/port.h"

_Static_assert(IDA_KAL_IDLE == IDA_PSTORE_IDLE && IDA_KAL_BUSY == IDA_PSTORE_BUSY &&
                   IDA_KAL_OK == IDA_PSTORE_OK && IDA_KAL_FAIL == IDA_PSTORE_FAIL,
               "A4 IDA_PSTORE_*");
_Static_assert(IDA_KAL_FAIL_FLASH == IDA_PSFAIL_FLASH && IDA_KAL_FAIL_VERIFY == IDA_PSFAIL_VERIFY &&
                   IDA_KAL_FAIL_ABORTED == IDA_PSFAIL_ABORTED &&
                   IDA_KAL_FAIL_TOO_BIG == IDA_PSFAIL_TOO_BIG,
               "A4 IDA_PSFAIL_*");
_Static_assert(16u + IDA_KAL_BLOB_AZAMI <= IDA_KAL_CMIT_OFS, "blok sayfaya sığar");

#define SAYAC_YUVA (IDA_KAL_SAYFA_BAYT / IDA_KAL_QW)

/* Kayıt durum makinesi adımları */
#define A_BOS 0u
#define A_SIL 1u
#define A_YAZ 2u
#define A_CMIT 3u
#define A_SON_CMIT 4u
#define A_GERIAL 5u /* tamamlanma yazıldıktan sonra iptal: hedef sayfa silinir (A4-K83) */

uint32_t ida_kal_crc32(const void *veri, size_t n)
{
    const uint8_t *b = (const uint8_t *)veri;
    uint32_t crc = UINT32_C(0xFFFFFFFF);
    for (size_t i = 0u; i < n; i++) {
        crc ^= b[i];
        for (uint32_t j = 0u; j < 8u; j++) {
            const uint32_t maske = (uint32_t)0u - (crc & 1u);
            crc = (crc >> 1) ^ (UINT32_C(0xEDB88320) & maske);
        }
    }
    return ~crc;
}

static uint32_t oku32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static void yaz32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)(v >> 16);
    p[3] = (uint8_t)(v >> 24);
}

static const uint8_t *sayfa(const ida_kalici_t *k, uint32_t s)
{
    return k->f.taban(k->f.ctx) + (size_t)s * IDA_KAL_SAYFA_BAYT;
}

/* Bölgede [ofs, ofs + n) okunurken çift ECC hatası görüldü mü (sol bulgu 6): görüldüyse okunan
   baytlar ne olursa olsun kayıt reddedilir. */
static bool ecc_bozuk(const ida_kalici_t *k, uint32_t ofs, uint32_t n)
{
    return k->f.ecc_bozuk(k->f.ctx, ofs, n);
}

static bool silinmis(const uint8_t *q, size_t n)
{
    for (size_t i = 0u; i < n; i++) {
        if (q[i] != 0xFFu) {
            return false;
        }
    }
    return true;
}

/* ---- açılış sayacı ---------------------------------------------------------------------- */

static bool sayac_kaydi(const ida_kalici_t *k, uint32_t ofs, uint32_t *n)
{
    const uint8_t *q = k->f.taban(k->f.ctx) + ofs;
    if (oku32(q) != IDA_KAL_SAYAC_MAGIC) {
        return false;
    }
    const uint32_t v = oku32(q + 4);
    if (oku32(q + 8) != (0xFFFFFFFFu ^ v) || oku32(q + 12) != ida_kal_crc32(q, 12u)) {
        return false;
    }
    if (ecc_bozuk(k, ofs, IDA_KAL_QW)) {
        return false;
    }
    *n = v;
    return true;
}

static int islem_bekle(ida_kalici_t *k, void (*bekle)(void *), void *bctx)
{
    for (uint32_t i = 0u; i < 2000000u; i++) {
        const int d = k->f.durum(k->f.ctx);
        if (d != 1) {
            return d;
        }
        if (bekle != NULL) {
            bekle(bctx);
        }
    }
    return -1;
}

/* Sayfada `bas`tan itibaren ilk silinmiş yuva; yoksa -1. */
static int32_t bos_yuva(const ida_kalici_t *k, uint32_t s, uint32_t bas)
{
    const uint8_t *p = sayfa(k, s);
    for (uint32_t y = bas; y < SAYAC_YUVA; y++) {
        /* ECC'si bozuk yuva (yarım program) silinmiş görünse de kullanılmaz. */
        if (silinmis(p + y * IDA_KAL_QW, IDA_KAL_QW) &&
            !ecc_bozuk(k, s * IDA_KAL_SAYFA_BAYT + y * IDA_KAL_QW, IDA_KAL_QW)) {
            return (int32_t)y;
        }
    }
    return -1;
}

static void sayac_artir(ida_kalici_t *k, void (*bekle)(void *), void *bctx)
{
    uint32_t enb = 0u;
    bool var = false;
    uint32_t enb_s = 0u, enb_y = 0u;
    for (uint32_t s = 0u; s < 2u; s++) {
        for (uint32_t y = 0u; y < SAYAC_YUVA; y++) {
            uint32_t n = 0u;
            if (sayac_kaydi(k, s * IDA_KAL_SAYFA_BAYT + y * IDA_KAL_QW, &n) && (!var || n > enb)) {
                var = true;
                enb = n;
                enb_s = s;
                enb_y = y;
            }
        }
    }
    k->boot_count = enb;
    k->boot_count_ok = 0u;
    if (var && enb >= IDA_BOOT_COUNT_MAX) {
        return; /* tükendi: geçerli üst sınır A4 IDA_BOOT_COUNT_MAX; sarma yok (01 §2.9) */
    }
    const uint32_t yeni = var ? enb + 1u : 1u;
    uint32_t s = var ? enb_s : 0u;
    uint32_t bas = var ? enb_y + 1u : 0u;
    for (uint32_t deneme = 0u; deneme < 4u; deneme++) {
        int32_t y = bos_yuva(k, s, bas);
        if (y < 0) {
            /* Sayfa doldu: öteki sayfa silinir (en büyük kayıt bu sayfada kalır). */
            const uint32_t oteki = var ? 1u - s : s;
            if (k->f.sil(k->f.ctx, oteki) != 0 || islem_bekle(k, bekle, bctx) != 0) {
                return;
            }
            s = oteki;
            y = 0;
        }
        uint8_t q[IDA_KAL_QW] = {0};
        yaz32(q, IDA_KAL_SAYAC_MAGIC);
        yaz32(q + 4, yeni);
        yaz32(q + 8, 0xFFFFFFFFu ^ yeni);
        yaz32(q + 12, ida_kal_crc32(q, 12u));
        const uint32_t ofs = s * IDA_KAL_SAYFA_BAYT + (uint32_t)y * IDA_KAL_QW;
        if (k->f.yaz16(k->f.ctx, ofs, q) == 0 && islem_bekle(k, bekle, bctx) == 0) {
            uint32_t n = 0u;
            if (sayac_kaydi(k, ofs, &n) && n == yeni) {
                k->boot_count = yeni;
                k->boot_count_ok = 1u;
                return;
            }
        }
        k->sayac_yazim_deneme++;
        bas = (uint32_t)y + 1u; /* yarım kalan yuva yeniden kullanılmaz */
    }
}

/* ---- parametre blokları ----------------------------------------------------------------- */

static bool blok_gecerli(const ida_kalici_t *k, uint32_t s, uint32_t *len, uint32_t *gen,
                         uint32_t *crc)
{
    const uint8_t *p = sayfa(k, s);
    const uint8_t *c = p + IDA_KAL_CMIT_OFS;
    if (oku32(p) != IDA_KAL_BLOK_MAGIC || oku32(c) != IDA_KAL_CMIT_MAGIC) {
        return false;
    }
    const uint32_t l = oku32(p + 4), g = oku32(p + 8), cr = oku32(p + 12);
    if (l > IDA_KAL_BLOB_AZAMI || oku32(c + 4) != g || oku32(c + 8) != (0xFFFFFFFFu ^ g) ||
        oku32(c + 12) != ida_kal_crc32(p, 16u) || ida_kal_crc32(p + 16, l) != cr) {
        return false;
    }
    /* Okunan bütün quad-word'lerde çift ECC hatası yok (sol bulgu 6). */
    const uint32_t taban = s * IDA_KAL_SAYFA_BAYT;
    if (ecc_bozuk(k, taban, 16u + l) || ecc_bozuk(k, taban + IDA_KAL_CMIT_OFS, IDA_KAL_QW)) {
        return false;
    }
    *len = l;
    *gen = g;
    *crc = cr;
    return true;
}

static void gecerli_bul(ida_kalici_t *k)
{
    k->gecerli_sayfa = -1;
    k->blok_len = k->blok_gen = k->blok_crc = 0u;
    for (uint32_t s = 2u; s < 4u; s++) {
        uint32_t l = 0u, g = 0u, c = 0u;
        if (blok_gecerli(k, s, &l, &g, &c) && (k->gecerli_sayfa < 0 || g > k->blok_gen)) {
            k->gecerli_sayfa = (int8_t)s;
            k->blok_len = l;
            k->blok_gen = g;
            k->blok_crc = c;
        }
    }
}

void ida_kal_acilis(ida_kalici_t *k, const ida_kal_flash_t *f, void (*bekle)(void *ctx),
                    void *bekle_ctx)
{
    memset(k, 0, sizeof *k);
    k->f = *f;
    gecerli_bul(k);
    k->store = IDA_KAL_IDLE;
    k->store_gen = k->blok_gen; /* init'te kalıcı bloğun kuşağı (A1-K68) */
    sayac_artir(k, bekle, bekle_ctx);
}

const uint8_t *ida_kal_blob(const ida_kalici_t *k, uint32_t *len)
{
    if (k->gecerli_sayfa < 0) {
        *len = 0u;
        return NULL;
    }
    *len = k->blok_len;
    return sayfa(k, (uint32_t)k->gecerli_sayfa) + 16;
}

static void bitir(ida_kalici_t *k, uint8_t store, uint8_t fail)
{
    k->store = store;
    k->fail = fail;
    k->store_gen = k->gen;
    k->adim = A_BOS;
    k->iptal_istegi = 0u;
    if (store == IDA_KAL_FAIL) {
        k->kayit_hata++;
    } else {
        k->kayit_sayisi++;
    }
}

void ida_kal_kaydet(ida_kalici_t *k, const uint8_t *blob, uint32_t len, uint32_t gen)
{
    k->gen = gen;
    if (k->adim != A_BOS) {
        k->fail = IDA_KAL_FAIL_FLASH; /* olmamalı: çekirdek BUSY'de PARAM_SAVE'i reddeder */
        k->kayit_hata++;
        return;
    }
    if (blob == NULL || len > IDA_KAL_BLOB_AZAMI) {
        bitir(k, IDA_KAL_FAIL, IDA_KAL_FAIL_TOO_BIG);
        return;
    }
    memcpy(k->blob, blob, len);
    k->len = len;
    k->crc = ida_kal_crc32(blob, len);
    k->hedef = (uint8_t)(k->gecerli_sayfa == 2 ? 3u : 2u);
    k->store = IDA_KAL_BUSY;
    k->fail = IDA_KAL_FAIL_YOK;
    k->iptal_istegi = 0u;
    k->adim = A_SIL;
    k->sonraki_ofs = 0u;
    if (k->f.sil(k->f.ctx, k->hedef) != 0) {
        bitir(k, IDA_KAL_FAIL, IDA_KAL_FAIL_FLASH);
    }
}

/* Yazılacak sıradaki quad-word (başlık ya da blob; son blok 0xFF ile doldurulur). */
static void qw_kur(const ida_kalici_t *k, uint32_t ofs, uint8_t q[IDA_KAL_QW])
{
    if (ofs == 0u) {
        yaz32(q, IDA_KAL_BLOK_MAGIC);
        yaz32(q + 4, k->len);
        yaz32(q + 8, k->gen);
        yaz32(q + 12, k->crc);
        return;
    }
    memset(q, 0xFF, IDA_KAL_QW);
    const uint32_t b = ofs - 16u;
    /* Açık sınır (MSVC /analyze C6385): blob dizisinin ve kayıt boyunun içinde kalınır. */
    if (ofs < 16u || k->len > sizeof k->blob || b >= k->len) {
        return;
    }
    const uint32_t n = k->len - b < IDA_KAL_QW ? k->len - b : IDA_KAL_QW;
    memcpy(q, &k->blob[b], n);
}

static uint32_t yazilacak_son(const ida_kalici_t *k)
{
    return 16u + ((k->len + IDA_KAL_QW - 1u) / IDA_KAL_QW) * IDA_KAL_QW;
}

static bool icerik_dogru(const ida_kalici_t *k)
{
    if (k->len > sizeof k->blob) {
        return false;
    }
    const uint8_t *p = sayfa(k, k->hedef);
    uint8_t q[IDA_KAL_QW] = {0};
    qw_kur(k, 0u, q);
    return memcmp(p, q, IDA_KAL_QW) == 0 && memcmp(p + 16, k->blob, k->len) == 0 &&
           ida_kal_crc32(p + 16, k->len) == k->crc &&
           !ecc_bozuk(k, (uint32_t)k->hedef * IDA_KAL_SAYFA_BAYT, 16u + k->len);
}

void ida_kal_adim(ida_kalici_t *k)
{
    if (k->adim == A_BOS) {
        return;
    }
    const int d = k->f.durum(k->f.ctx);
    if (d == 1) {
        return; /* donanım işlemi sürüyor */
    }
    if (d < 0) {
        if (k->adim == A_SON_CMIT || k->adim == A_GERIAL) {
            /* Tamamlanma yazımı ya da geri alma silmesi hata verdi: RAM flash'taki gerçek durumla
               eşitlenir; hedef geçerli kaldıysa kayıt yapılmıştır ve FAIL bildirilmez (A4-K83:
               FAIL'de kalıcı blok değişmemiştir). */
            gecerli_bul(k);
            if (k->gecerli_sayfa == (int8_t)k->hedef && k->blok_gen == k->gen) {
                bitir(k, IDA_KAL_OK, IDA_KAL_FAIL_YOK);
                return;
            }
        }
        bitir(k, IDA_KAL_FAIL, IDA_KAL_FAIL_FLASH);
        return;
    }
    if (k->iptal_istegi && k->adim == A_SON_CMIT) {
        /* Sol bulgu 3: tamamlanma kaydı yazıldı (ya da yazımı başladı); iptalin kesinleşmesi
           için hedef sayfa silinir. Ancak silme bitince FAIL/ABORTED bildirilir; böylece FAIL'de
           kalıcı blok değişmemiş olur (A4-K83). Silme kesilirse yarım silinmiş sayfa geçersizdir
           (tamamlanma sayfanın sonunda, başlık başında). */
        if (k->f.sil(k->f.ctx, k->hedef) != 0) {
            /* Geri alınamadı: RAM flash'taki gerçek durumla eşitlenir; hedef geçerliyse kayıt
               yapılmıştır (FAIL bildirilmez). */
            gecerli_bul(k);
            if (k->gecerli_sayfa == (int8_t)k->hedef && k->blok_gen == k->gen) {
                bitir(k, IDA_KAL_OK, IDA_KAL_FAIL_YOK);
            } else {
                bitir(k, IDA_KAL_FAIL, IDA_KAL_FAIL_FLASH);
            }
            return;
        }
        k->adim = A_GERIAL;
        return;
    }
    if (k->iptal_istegi && k->adim != A_GERIAL) {
        bitir(k, IDA_KAL_FAIL, IDA_KAL_FAIL_ABORTED); /* tamamlanma yazılmadı: eski blok geçerli */
        return;
    }
    switch (k->adim) {
    case A_SIL:
        k->adim = A_YAZ;
        k->sonraki_ofs = 0u;
        /* fall through */
    case A_YAZ:
        if (k->sonraki_ofs < yazilacak_son(k)) {
            uint8_t q[IDA_KAL_QW] = {0};
            qw_kur(k, k->sonraki_ofs, q);
            if (k->f.yaz16(k->f.ctx, (uint32_t)k->hedef * IDA_KAL_SAYFA_BAYT + k->sonraki_ofs, q) != 0) {
                bitir(k, IDA_KAL_FAIL, IDA_KAL_FAIL_FLASH);
                return;
            }
            k->sonraki_ofs += IDA_KAL_QW;
            return;
        }
        if (!icerik_dogru(k)) {
            bitir(k, IDA_KAL_FAIL, IDA_KAL_FAIL_VERIFY);
            return;
        }
        k->adim = A_CMIT;
        /* fall through */
    case A_CMIT: {
        uint8_t q[IDA_KAL_QW] = {0};
        uint8_t bas[IDA_KAL_QW] = {0};
        qw_kur(k, 0u, bas);
        yaz32(q, IDA_KAL_CMIT_MAGIC);
        yaz32(q + 4, k->gen);
        yaz32(q + 8, 0xFFFFFFFFu ^ k->gen);
        yaz32(q + 12, ida_kal_crc32(bas, 16u));
        if (k->f.yaz16(k->f.ctx, (uint32_t)k->hedef * IDA_KAL_SAYFA_BAYT + IDA_KAL_CMIT_OFS, q) != 0) {
            bitir(k, IDA_KAL_FAIL, IDA_KAL_FAIL_FLASH);
            return;
        }
        k->adim = A_SON_CMIT;
        return;
    }
    case A_SON_CMIT: {
        uint32_t l = 0u, g = 0u, c = 0u;
        if (!blok_gecerli(k, k->hedef, &l, &g, &c) || g != k->gen) {
            bitir(k, IDA_KAL_FAIL, IDA_KAL_FAIL_VERIFY);
            return;
        }
        k->gecerli_sayfa = (int8_t)k->hedef;
        k->blok_len = l;
        k->blok_gen = g;
        k->blok_crc = c;
        bitir(k, IDA_KAL_OK, IDA_KAL_FAIL_YOK);
        return;
    }
    case A_GERIAL:
        bitir(k, IDA_KAL_FAIL, IDA_KAL_FAIL_ABORTED); /* hedef silindi: eski blok geçerli */
        return;
    default:
        bitir(k, IDA_KAL_FAIL, IDA_KAL_FAIL_FLASH);
        return;
    }
}

void ida_kal_iptal(ida_kalici_t *k)
{
    if (k->adim != A_BOS) {
        k->iptal_istegi = 1u;
    }
}

bool ida_kal_mesgul(const ida_kalici_t *k)
{
    return k->adim != A_BOS;
}
