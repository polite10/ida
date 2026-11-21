/*
 * flash.h — kalıcı bellek bölgesinin flash sürücüsü (bsp/flash.c; app/kalici.h arayüzü).
 */
#ifndef IDA_FW_FLASH_H
#define IDA_FW_FLASH_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint32_t silme_sayisi, yazma_sayisi;
    uint32_t silme_en_kotu_cevrim, yazma_en_kotu_cevrim; /* başlatmadan bitişin görülmesine */
    uint32_t hata;
    uint32_t ecc_nmi; /* bölgede çift bit ECC hatası (NMI; yarım yazılmış quad-word) */
    uint32_t eccc_nvm, eccc_diger; /* düzeltilen tek hata: bölgede / başka yerde */
    uint32_t eccd_diger;           /* bölge dışında çift hata (ölümcül) */
    uint32_t ecc_son;              /* son ECC olayının ECCR değeri */
} ida_flash_olcum_t;

extern ida_flash_olcum_t ida_flash_olcum;

const uint8_t *ida_flash_taban(void);
int ida_flash_sil(uint32_t sayfa);                 /* 0..3 */
int ida_flash_yaz16(uint32_t ofs, const uint8_t q[16]);
int ida_flash_durum(void);                         /* 0 bitti, 1 sürüyor, < 0 hata */
/* Açılışta (kalıcı bölge okunmadan önce): bekleyen ECC bayraklarını kaydedip temizler, düzeltme
   kesmesini açar. */
void ida_flash_ecc_baslat(void);
/* NMI işleyicisinden: neden kalıcı bellek bölgesinde çift bit ECC hatası ise bayrağı temizler,
   hatalı quad-word'ü işaretler, ICACHE'i geçersiz kılar ve true döner; değilse false. */
bool ida_flash_ecc_nmi(void);
/* Bölgede [ofs, ofs + n) okunurken çift hata görüldü mü (app/kalici.h ecc_bozuk). */
bool ida_flash_ecc_bozuk(uint32_t ofs, uint32_t n);

#ifdef __cplusplus
}
#endif

#endif /* IDA_FW_FLASH_H */
