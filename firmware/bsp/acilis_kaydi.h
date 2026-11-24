/*
 * acilis_kaydi.h — sıfırlama nedeni, ardışık arıza sayacı ve hata kaydı (01 §2.2 md.3, §2.8;
 * W3-kart dilim 1).
 *
 * Donanımdan bağımsızdır (ISO C17; PC'de birim testiyle sınanır). Kayıt kartta `.noinit`
 * bölümündedir (SRAM2; sıfırlamada silinmez, CRC ile korunur): hata işleyicisi arızayı yazar ve
 * sistemi sıfırlar; açılışta platform kaydı okur, sıfırlama nedenini RCC_CSR bayraklarıyla
 * birlikte çözer ve ardışık arıza sayacını günceller.
 *
 * Sıfırlama nedeni (A4 IDA_RESET_*; bayraklar STM32U5 RCC_CSR, RM0456):
 *   IWDGRSTF ya da WWDGRSTF                     -> WATCHDOG
 *   SFTRSTF ve kayıtta bekleyen arıza işareti  -> FAULT (hata işleyicisinin sıfırlaması)
 *   SFTRSTF (bekleyen arıza yok)               -> SOFTWARE (planlı yeniden başlatma ya da
 *                                                 hata ayıklayıcının/programlayıcının sistem
 *                                                 sıfırlaması; ikisi bayraklardan ayrılamaz)
 *   BORRSTF                                    -> POWER_ON (STM32U5'te ayrı POR bayrağı yoktur;
 *                                                 güç verme BOR bayrağını kurar, gerçek bir
 *                                                 gerilim düşmesi bundan ayrılamaz)
 *   PINRSTF                                    -> PIN
 *   başka (OBLRSTF, LPWRRSTF, bayraksız)       -> UNKNOWN
 * Sıra önemlidir: yazılım sıfırlaması NRST hattını da sürdüğü için PINRSTF'yi de kurar; güç
 * vermede de PINRSTF kurulabilir. Ham bayraklar ayrıca bildirilir (BOARD_HELLO.reset_flags).
 *
 * Ardışık arıza sayacı (01 §2.8): FAULT ve WATCHDOG bir artırır (255'te doyar); POWER_ON, PIN
 * ve SOFTWARE sıfırlar; UNKNOWN değiştirmez. Kararlı çalışmayla (NORMAL ve RUNNING oturumla
 * kesintisiz 30 s) ana döngü ida_kayit_ardisik_temizle ile sıfırlar.
 */
#ifndef IDA_FW_ACILIS_KAYDI_H
#define IDA_FW_ACILIS_KAYDI_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* RCC_CSR sıfırlama bayrakları (STM32U5; bsp/sifirlama.c aygıt başlığıyla eşitliğini denetler). */
#define IDA_CSR_OBLRSTF  (UINT32_C(1) << 25)
#define IDA_CSR_PINRSTF  (UINT32_C(1) << 26)
#define IDA_CSR_BORRSTF  (UINT32_C(1) << 27)
#define IDA_CSR_SFTRSTF  (UINT32_C(1) << 28)
#define IDA_CSR_IWDGRSTF (UINT32_C(1) << 29)
#define IDA_CSR_WWDGRSTF (UINT32_C(1) << 30)
#define IDA_CSR_LPWRRSTF (UINT32_C(1) << 31)
#define IDA_CSR_BAYRAKLAR UINT32_C(0xFE000000)

#define IDA_KAYIT_MAGIC UINT32_C(0x4B414449) /* "IDAK", küçük uçlu */
#define IDA_KAYIT_SURUM 1u

/* Kayıt; bütün alanlar 32 bit, son alan önceki alanların CRC-32/ISO-HDLC değeri. */
typedef struct {
    uint32_t magic;
    uint32_t surum;
    uint32_t ardisik_ariza;   /* ardışık arıza sayacı                                 */
    uint32_t ariza_bekliyor;  /* 1: hata işleyicisi sıfırladı, açılış henüz görmedi   */
    uint32_t ariza_sayisi;    /* yazılan hata kaydı sayısı (fault_seq; sarar)          */
    uint32_t ariza_var;       /* 1: aşağıdaki hata alanları geçerli bir kayıttır      */
    uint32_t cfsr;
    uint32_t hfsr;
    uint32_t mmfar;
    uint32_t bfar;
    uint32_t pc;              /* yığına itilmiş; yigin_gecerli = 0 ise anlamsız        */
    uint32_t lr;
    uint32_t xpsr;
    uint32_t adim;            /* arıza anındaki çekirdek çağrısı sayacı                 */
    uint32_t duvar_ham;       /* arıza anındaki ham duvar sayacı (µs, alt 32 bit)      */
    uint32_t yigin_gecerli;   /* 1: PC/LR/xPSR yığından güvenle okundu                 */
    uint32_t istisna;         /* etkin istisna numarası (IPSR)                          */
    uint32_t crc;
} ida_acilis_kaydi_t;

/* Açılıştaki çözümleme sonucu. */
typedef struct {
    uint8_t reset_cause;        /* A4 IDA_RESET_*                         */
    uint32_t reset_flags;       /* ham RCC_CSR (yalnız bayrak bitleri)    */
    uint8_t ardisik_ariza;      /* güncellenmiş sayaç (255'te doyar)      */
    uint8_t kayit_gecerliydi;   /* 1: açılışta kayıt CRC'si tutuyordu     */
    uint8_t onceki_ariza;       /* 1: bu açılış bir hata sıfırlamasından  */
} ida_acilis_sonuc_t;

uint32_t ida_fw_crc32(const void *veri, size_t n);
int ida_kayit_gecerli(const ida_acilis_kaydi_t *k);
void ida_kayit_muhurle(ida_acilis_kaydi_t *k);
/* Geçersiz kaydı sıfır içerikle kurar ve mühürler. */
void ida_kayit_kur(ida_acilis_kaydi_t *k);

/* Kararlı çalışmadan sonra (01 §2.8: NORMAL ve RUNNING oturumla kesintisiz 30 s) ardışık arıza
   sayacını sıfırlar ve yeniden mühürler; hata alanları korunur. Kayıt geçersizse kurulur. */
void ida_kayit_ardisik_temizle(ida_acilis_kaydi_t *k);

/* Sıfırlama nedeni (A4 IDA_RESET_*). */
uint8_t ida_sifirlama_nedeni(uint32_t csr, int ariza_bekliyor);

/* Açılış: kaydı doğrular (geçersizse kurar), nedeni çözer, ardışık arıza sayacını günceller,
   bekleyen arıza işaretini temizler ve kaydı yeniden mühürler. Hata alanları korunur. */
void ida_kayit_acilis(ida_acilis_kaydi_t *k, uint32_t csr, ida_acilis_sonuc_t *s);

#ifdef __cplusplus
}
#endif

#endif /* IDA_FW_ACILIS_KAYDI_H */
