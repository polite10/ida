/*
 * kalici.h — kalıcı bellek: açılış sayacı ve parametre A/B blokları (01 §2.9; A4-K78…K86;
 * A1-K68; W3-kart dilim 2).
 *
 * Donanımdan bağımsızdır (ISO C17; PC'de RAM'deki bir flash modeliyle sınanır). Flash'ın
 * ayrılan 32 KB'ı (Bank 2'nin son dört 8 KB sayfası) dört sayfadır:
 *   sayfa 0, 1  açılış sayacı kayıtları (16 B: magic, n, ~n, CRC-32(ilk 12 B))
 *   sayfa 2, 3  parametre blokları A ve B
 * Flash yazma birimi 16 B'lık quad-word'dür; silinmiş bayt 0xFF'tir.
 *
 * Açılış sayacı: açılışta iki sayfa taranır; geçerli kayıtların en büyüğü N. N + 1, en büyük
 * kaydın sayfasında ondan sonraki ilk silinmiş yuvaya (yoksa öteki sayfa silinip başına)
 * yazılır ve geri okunarak doğrulanır; ancak o zaman boot_count = N + 1 ve boot_count_ok = 1
 * (A4-K86). Yarım kalan kayıt geçersiz okunur ve atlanır; yuvası yeniden kullanılmaz. Sayaç
 * geri gitmez; geçerli üst sınıra (A4 IDA_BOOT_COUNT_MAX = 0xFFFFFFFE) ulaşmışsa ya da ayrılamazsa
 * boot_count_ok = 0. Açılış yazımı
 * bloklayıcıdır (başlatmada izinli; 01 §2.9).
 *
 * Parametre bloğu sayfası:
 *   [0, 16)          başlık: magic "IDPB", len, gen, CRC-32(blob)
 *   [16, 16 + len)   blob (A4 §3.10; çekirdeğin ürettiği bayt dizisi, yorumlanmaz; A4-K84)
 *   [8176, 8192)     tamamlanma: magic "CMIT", gen, ~gen, CRC-32(başlık)
 * Tamamlanma kaydı en son yazılır; olmayan sayfa geçersizdir. İki sayfa da geçerliyse kuşağı
 * büyük olan kullanılır. Okunurken çift ECC hatası görülen (ecc_bozuk) sayaç kaydı, yuvası ve
 * blok, baytları geçerli görünse de reddedilir.
 * Kayıt, geçerli (en yeni) bloğu taşımayan sayfaya yapılır: sil →
 * başlık ve blob → geri okuma ve CRC → tamamlanma. Güç kesintisi ya da iptal eski bloğu bozmaz.
 * Kayıt adım adım ilerler (her adım tek bir donanım işlemi; döngüyü bloklamaz): durum
 * A4-K83'ün param_store'u (BUSY, sonra OK ya da FAIL) ve kuşağıdır.
 */
#ifndef IDA_FW_KALICI_H
#define IDA_FW_KALICI_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define IDA_KAL_SAYFA_BAYT 8192u
#define IDA_KAL_SAYFA_N 4u
#define IDA_KAL_QW 16u
#define IDA_KAL_SAYAC_MAGIC UINT32_C(0x544F4F42) /* "BOOT" */
#define IDA_KAL_BLOK_MAGIC  UINT32_C(0x42504449) /* "IDPB" */
#define IDA_KAL_CMIT_MAGIC  UINT32_C(0x54494D43) /* "CMIT" */
#define IDA_KAL_BLOB_AZAMI  8000u                /* A4-K80 */
#define IDA_KAL_CMIT_OFS    (IDA_KAL_SAYFA_BAYT - IDA_KAL_QW)

/* Flash erişimi (kartta bsp/flash.c; testte RAM modeli). Hepsi zorunlu. */
typedef struct {
    void *ctx;
    /* Bölgenin belleğe eşlenmiş başlangıcı (okuma; 4 × 8 KB). */
    const uint8_t *(*taban)(void *ctx);
    /* Sayfa silmeyi başlatır. 0: başladı, < 0: başlatılamadı. */
    int (*sil)(void *ctx, uint32_t sayfa);
    /* ofs'ye (16 B hizalı) 16 baytı yazmayı başlatır. 0: başladı, < 0: başlatılamadı. */
    int (*yaz16)(void *ctx, uint32_t ofs, const uint8_t q[IDA_KAL_QW]);
    /* Son işlemin durumu: 0 bitti (başarılı), 1 sürüyor, < 0 hata. */
    int (*durum)(void *ctx);
    /* [ofs, ofs + n) okunurken çift ECC hatası (yutulan NMI) görüldü mü (sol bulgu 6). Bilgi,
       ilgili sayfa silinene kadar kalır; okuma bundan önce yapılmış olmalıdır. */
    bool (*ecc_bozuk)(void *ctx, uint32_t ofs, uint32_t n);
} ida_kal_flash_t;

/* A4 IDA_PSTORE_* / IDA_PSFAIL_* ile aynı değerler (kalici.c'de _Static_assert). */
#define IDA_KAL_IDLE 0u
#define IDA_KAL_BUSY 1u
#define IDA_KAL_OK   2u
#define IDA_KAL_FAIL 3u
#define IDA_KAL_FAIL_YOK     0u
#define IDA_KAL_FAIL_FLASH   1u
#define IDA_KAL_FAIL_VERIFY  2u
#define IDA_KAL_FAIL_ABORTED 3u
#define IDA_KAL_FAIL_TOO_BIG 4u

typedef struct {
    ida_kal_flash_t f;
    /* açılış sayacı */
    uint32_t boot_count;
    uint8_t boot_count_ok;
    uint32_t sayac_yazim_deneme; /* doğrulanamayan yazım sayısı */
    /* geçerli blok */
    int8_t gecerli_sayfa;        /* 2, 3 ya da -1 */
    uint32_t blok_len, blok_gen, blok_crc;
    /* kayıt */
    uint8_t store;               /* IDA_KAL_* */
    uint8_t fail;
    uint32_t store_gen;          /* son tamamlanan denemenin kuşağı (plat.param_gen) */
    uint8_t adim;                /* iç durum makinesi */
    uint8_t hedef;               /* hedef sayfa */
    uint32_t sonraki_ofs;
    uint32_t gen, len, crc;
    uint8_t iptal_istegi;
    uint8_t blob[IDA_KAL_BLOB_AZAMI];
    /* tanı */
    uint32_t kayit_sayisi, kayit_hata;
} ida_kalici_t;

uint32_t ida_kal_crc32(const void *veri, size_t n);

/* Açılış: flash erişimini kurar, geçerli parametre bloğunu bulur, açılış sayacını artırıp
   yazar (bloklayıcı; durum() 1 iken bekle() çağrılır, ör. IWDG beslemesi). */
void ida_kal_acilis(ida_kalici_t *k, const ida_kal_flash_t *f, void (*bekle)(void *ctx),
                    void *bekle_ctx);

/* Geçerli bloğun belleğe eşlenmiş blob'u (yoksa NULL ve *len = 0). */
const uint8_t *ida_kal_blob(const ida_kalici_t *k, uint32_t *len);

/* Kayıt başlat (çekirdeğin PARAM_SAVE sonucu; A4-K82). Blob kopyalanır. Kayıt sürüyorsa ya da
   blob çok büyükse hemen FAIL. */
void ida_kal_kaydet(ida_kalici_t *k, const uint8_t *blob, uint32_t len, uint32_t gen);
/* Ana döngüde her turda: donanım boştaysa sıradaki tek işlemi başlatır. */
void ida_kal_adim(ida_kalici_t *k);
/* Oturum sonunda ya da güvenli duruma girişte süren kaydı iptal eder (A4-K83): yeni işlem
   başlatılmaz; süren işlem bitince FAIL / ABORTED. Tamamlanma kaydının yazımı başlamışsa önce
   hedef sayfa silinir; FAIL / ABORTED ancak silme bitince bildirilir (FAIL'de kalıcı blok
   değişmemiştir). */
void ida_kal_iptal(ida_kalici_t *k);
bool ida_kal_mesgul(const ida_kalici_t *k);

#ifdef __cplusplus
}
#endif

#endif /* IDA_FW_KALICI_H */
