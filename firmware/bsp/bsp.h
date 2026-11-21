/*
 * bsp.h — kart destek katmanı (STM32U5A9J-DK; W3-kart dilim 1; 01 §2.1, §2.2, §2.5, §2.8).
 *
 * Kart değişiminde yeniden yazılan yer burasıdır; üst katmanlar ST başlığı içermez. Başlatma
 * sırası (01 §2.2, tek kablo K17 ile USB adımları yok):
 *   ida_bsp_erken_baslat  HAL_Init (NVIC: 4 bit önalım), sıfırlama nedeni ve .noinit kaydı,
 *                         IWDG nominal 2 s (güç ve PLL beklemelerinden önce)
 *   ida_bsp_saat_kur      SMPS, Range 1, HSE 16 MHz + PLL1 (M 4, N 80, R 2) -> 160 MHz;
 *                         geçişte geçici AHB ÷2 ve >= 5 µs (MP1 bulgusu); HSI48 (RNG için);
 *                         SYSCLK geri okunur; MSIS'e sessizce dönülmez
 *   ida_bsp_cevre_baslat  ICACHE, LED'ler, DWT, duvar saati ve 200 Hz ızgara (TIM2), RNG
 *   ida_bsp_calisma_kipi  IWDG nominal 100 ms (01 §2.8), SysTick durur
 */
#ifndef IDA_FW_BSP_H
#define IDA_FW_BSP_H

#include <stdbool.h>
#include <stdint.h>

#include "acilis_kaydi.h"

#ifdef __cplusplus
extern "C" {
#endif

#define IDA_BSP_SYSCLK_HZ 160000000u
#define IDA_BSP_IZGARA_US 5000u /* 200 Hz kontrol ızgarası (iskelet §4) */

/* ---- başlatma ---------------------------------------------------------------------------- */
void ida_bsp_erken_baslat(void);
/* 0: başarılı; değilse başarısız adım (1 güç kaynağı, 2 gerilim ölçeği, 3 HSE/HSI48,
   4 PLL1, 5 SYSCLK geçişi, 6 geri okuma, 7 AHB ÷1). */
uint32_t ida_bsp_saat_kur(void);
/* RCC yazmaçlarından geri okunan SYSCLK (Hz); PLL1 seçili değilse 0. */
uint32_t ida_bsp_sysclk_hz(void);
void ida_bsp_cevre_baslat(void);
void ida_bsp_calisma_kipi(void);

/* Açılış kaydının çözümü (ida_bsp_erken_baslat doldurur). */
const ida_acilis_sonuc_t *ida_bsp_acilis(void);
/* .noinit'teki kayıt (salt okunur; hata alanları önceki açılışın arızası olabilir). */
const ida_acilis_kaydi_t *ida_bsp_kayit(void);

/* ---- zaman (01 §2.5) ---------------------------------------------------------------------- */
/* Duvar saati: açılıştan beri µs (TIM2 1 MHz + taşma sayacı); azalmaz. */
uint64_t ida_bsp_zaman_us(void);
/* DWT çevrim sayacı (160 MHz; 26,8 s'de sarar; yalnız süre farkı için). */
static inline uint32_t ida_bsp_cevrim(void)
{
    return *(volatile const uint32_t *)0xE0001004u; /* DWT->CYCCNT */
}
/* 200 Hz ızgara sayacı (kesme artırır) ve son ızgara anının duvar saati (µs). */
uint32_t ida_bsp_izgara_sayaci(void);
uint64_t ida_bsp_izgara_ani_us(uint32_t *sayac);
/* Aynı anda alınmış (DWT, duvar µs) çifti; saat çapraz denetimi için. */
void ida_bsp_cevrim_ve_zaman(uint32_t *cevrim, uint64_t *t_us);

/* ---- gözetim (01 §2.8) -------------------------------------------------------------------- */
void ida_bsp_iwdg_besle(void);

/* ---- LED (PE0 yeşil, PE1 kırmızı) --------------------------------------------------------- */
void ida_bsp_led_yesil(bool yak);
void ida_bsp_led_kirmizi(bool yak);
void ida_bsp_led_yesil_cevir(void);

/* ---- rastgelelik -------------------------------------------------------------------------- */
/* Sıfırdan farklı 32 bit gerçek rastgele sayı; RNG hata verirse 0 döner. */
uint32_t ida_bsp_rastgele(void);

/* ---- hata kaydı --------------------------------------------------------------------------- */
/* Hata işleyicisinin kaydına girecek çekirdek çağrısı sayacı (çağırıcı artırır). */
extern volatile uint32_t ida_bsp_adim_sayaci;

/* ---- kesme maskeleme (01 §2.4: BASEPRI = n, n ve sayısal olarak büyük öncelikleri maskeler) */
uint32_t ida_bsp_maske_ac(uint32_t oncelik); /* önceki BASEPRI'yi döndürür */
void ida_bsp_maske_birak(uint32_t onceki);

/* Çevre birimi kesmesini önceliğiyle açar ve NVIC bekçisinin listesine kaydeder. */
void ida_bsp_kesme_ac(int32_t irqn, uint32_t oncelik);
/* NVIC bekçisi: kayıtlı kesmelerin önceliğini ve açıklığını, hata işleyicilerinin SHCSR
   bitlerini geri yükler; açılıştan beri yapılan düzeltme sayısını döndürür.
   STM32_Programmer_CLI bağlanırken ("Clear all interrupts") NVIC_ISER'i temizliyor ve
   öncelikleri 0'a döndürüyor (kartta gözlendi): bekçi olmadan ızgara durur ve IWDG kartı
   sıfırlar; öncelik geri yüklenmezse seri kesmeleri BASEPRI maskesinden çıkar ve alım verisi
   bozulur. */
uint32_t ida_bsp_nvic_bekci(void);

/* Kesme öncelikleri (01 §2.4 tablosu; 0 en acil). */
#define IDA_BSP_ONCELIK_ZAMAN 1u
#define IDA_BSP_ONCELIK_SERI  3u

/* ---- yığın (01 §2.10) --------------------------------------------------------------------- */
/* Ana yığının boyu ve açılıştan beri en yüksek kullanımı (boyama deseniyle; bayt). */
uint32_t ida_bsp_yigin_boyu(void);
uint32_t ida_bsp_yigin_en_yuksek(void);

/* Veri bellek bariyeri (DMB; derleyici bariyeri de). */
static inline void ida_bsp_bariyer(void)
{
    __asm volatile("dmb" ::: "memory");
}

/* Açılışta FPSCR'nin geri okunan değeri (01 §2.2 md.1). */
extern uint32_t ida_bsp_fpscr_acilis;

#ifdef __cplusplus
}
#endif

#endif /* IDA_FW_BSP_H */
