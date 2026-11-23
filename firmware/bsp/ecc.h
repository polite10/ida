/*
 * ecc.h — flash ECC olaylarının işlenmesi (sol bulgu 2 ve 6; W3-kart dilim 2 düzeltmesi).
 *
 * Donanımdan bağımsızdır (ISO C17; PC'de birim testiyle sınanır). bsp/flash.c, FLASH_ECCR
 * yazmacının değerini buraya verir ve dönen değeri temizlik için yazmaca yazar.
 *
 * Yazmaç (STM32U5, FLASH_ECCR; CMSIS stm32u5a9xx.h ve HAL stm32u5xx_hal_flash.h tanımlarıyla
 * aynı bitler; flash.c'de _Static_assert):
 *   ADDR_ECC [20:0] hatalı erişimin bank içi adresi, BK_ECC [21] bank, SYSF_ECC [22] sistem
 *   flash'ı, ECCIE [24] düzeltme kesmesi açık, ECCC [30] tek bit hata düzeltildi, ECCD [31] çift
 *   hata saptandı (NMI). ECCC ve ECCD yazılan 1 ile temizlenir.
 * ST'nin güvenlik kılavuzu (UM2875, FLASH_SM_7; sol incelemesinin aktarımı, bu çalışmada
 * belgeden ayrıca doğrulanmadı): ECCC kurulu kalırsa sonraki çift hata için NMI üretilmeyebilir.
 * Bu yüzden ECCC açılıştan itibaren yönetilir: açılışta bekleyen bayrak kaydedilip temizlenir,
 * düzeltme kesmesi açılır ve her ECCC hemen kaydedilip temizlenir.
 *
 * Kalıcı bölgedeki (Bank 2, 0x083F8000'den 32 KB) çift hata yutulur (bölge okuyucusu kaydı
 * reddeder); başka yerdeki çift hata ölümcüldür (hata işleyicisi). Kalıcı bölgede çift hata
 * görülen her quad-word bir bit dizisinde işaretlenir; işaret sayfası silinene kadar kalır.
 */
#ifndef IDA_FW_ECC_H
#define IDA_FW_ECC_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define IDA_ECC_ECCD      UINT32_C(0x80000000)
#define IDA_ECC_ECCC      UINT32_C(0x40000000)
#define IDA_ECC_ECCIE     UINT32_C(0x01000000)
#define IDA_ECC_SYSF      UINT32_C(0x00400000)
#define IDA_ECC_BK        UINT32_C(0x00200000)
#define IDA_ECC_ADDR_MASK UINT32_C(0x001FFFFF)

#define IDA_ECC_NVM_OFS   UINT32_C(0x001F8000) /* Bank 2 içinde 0x083F8000 - 0x08200000 */
#define IDA_ECC_NVM_BAYT  UINT32_C(32768)
#define IDA_ECC_SAYFA     UINT32_C(8192)
#define IDA_ECC_QW        16u
#define IDA_ECC_QW_N      (IDA_ECC_NVM_BAYT / IDA_ECC_QW)

typedef struct {
    uint32_t eccc_nvm;      /* kalıcı bölgede düzeltilen tek hata */
    uint32_t eccc_diger;    /* başka yerde (kod, sistem flash'ı) */
    uint32_t eccd_nvm;      /* kalıcı bölgede çift hata (yutuldu, okuma reddedilir) */
    uint32_t eccd_diger;    /* başka yerde (ölümcül) */
    uint32_t son_eccc;      /* son ECCC'nin ECCR değeri (adres ve bank) */
    uint32_t son_eccd;      /* son ECCD'nin ECCR değeri */
    uint8_t bozuk[IDA_ECC_QW_N / 8u]; /* kalıcı bölgede çift hata görülen quad-word'ler */
} ida_ecc_t;

void ida_ecc_kur(ida_ecc_t *e);
/* ECCR değerini işler. Dönüş: temizlik için ECCR'ye yazılacak değer (ECCIE açık tutulur, yalnız
   işlenen ECCC/ECCD bitleri 1). *ecc_cift: ECCD vardı; *nvm: o çift hata kalıcı bölgedeydi
   (yutulabilir). İkisi de NULL olabilir. */
uint32_t ida_ecc_isle(ida_ecc_t *e, uint32_t eccr, bool *ecc_cift, bool *nvm);
/* Kalıcı bölgede [ofs, ofs + n) (bölge başından bayt) çift hata görüldü mü. Bölge dışı ya da
   n = 0: true (güvenli yön). */
bool ida_ecc_bozuk(const ida_ecc_t *e, uint32_t ofs, uint32_t n);
/* Sayfa silindi: o sayfanın işaretleri kalkar. */
void ida_ecc_sayfa_sil(ida_ecc_t *e, uint32_t sayfa);

#ifdef __cplusplus
}
#endif

#endif /* IDA_FW_ECC_H */
