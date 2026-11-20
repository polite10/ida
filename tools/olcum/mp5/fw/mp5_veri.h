/*
 * W0-MP5a ölçüm prototipi (atılabilir; ürün koduna taşınmaz, 09 §2.1).
 * Derleme zamanında host'ta üretilen girdi akışlarının tipleri (pc/mp5_uret.c yazar,
 * fw/main.c okur). Üretilen veri kartta sabittir (flash); ölçümden önce SRAM'e kopyalanır.
 */
#ifndef MP5_VERI_H
#define MP5_VERI_H

#include <stdint.h>

/* Bir mesajın yapı görüntüsü (mavlink_ida_<ad>_t'nin baytları, küçük uçlu). */
typedef struct {
    uint32_t msgid;
    uint16_t boy;        /* sizeof(yapı) = ida_link_mesaj_bilgisi().yapi_boyu */
    uint16_t yuk_azami;
    const uint8_t *yapi; /* boy bayt */
} mp5_yapi_t;

/* Ayrıştırma akışı: telde baytlar (host'ta ida_link_gonder ile seq 0'dan kodlandı). */
typedef struct {
    const char *ad;       /* E1, PEN, BUY, FIT-<msgid> */
    const char *varyant;  /* altin, ham, sifir */
    uint8_t grup;         /* 0: ana akış, 1: doğrusal uyum (tek çerçeve) */
    uint8_t rol_alici;    /* ida_link_rol_t */
    const uint8_t *bayt;
    uint32_t n_bayt;
    uint32_t n_cerceve;
    uint32_t ozet_al;     /* yineleme başına beklenen: Σ (msgid + len + yuk[len-1])            */
    uint32_t ozet_coz;    /* yineleme başına beklenen: Σ (msgid + yapi[0] + yapi[boy-1])       */
    uint32_t fnv_cerceve; /* teslim edilen çerçevelerin FNV-1a'sı (msgid, seq, len, yuk[0..yuk_azami)) */
    uint32_t fnv_yapi;    /* ida_link_coz çıktılarının FNV-1a'sı (yapı baytları)              */
} mp5_ayr_t;

/* Kodlama akışı: yapı görüntülerinin sırası; seq her yinelemede 0'dan başlar. */
typedef struct {
    const char *ad;       /* KS, BUYK, FIT-<msgid> */
    const char *varyant;
    uint8_t grup;
    uint8_t rol_gonderen;
    const uint16_t *yapi_idx; /* mp5_yapilar[] dizinleri */
    uint32_t n_mesaj;
    uint32_t n_bayt;      /* bir yinelemede yazılan bayt (host referansı) */
    uint32_t fnv;         /* yazılan baytların FNV-1a'sı (host referansı) */
} mp5_kod_t;

extern const mp5_yapi_t mp5_yapilar[];
extern const uint32_t MP5_YAPI_N;
extern const mp5_ayr_t mp5_ayr[];
extern const uint32_t MP5_AYR_N;
extern const mp5_kod_t mp5_kod[];
extern const uint32_t MP5_KOD_N;
/* msgid - 46000 -> yapı boyu (dialect'te yoksa 0). */
extern const uint16_t mp5_boy_tablosu[100];
/* Üretilen bütün verinin FNV-1a'sı (koşuyu üretilen veriye bağlar). */
extern const uint32_t MP5_VERI_KIMLIK;

#endif /* MP5_VERI_H */
