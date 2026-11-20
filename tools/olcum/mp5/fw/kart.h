/* W0-MP5a ölçüm prototipi (atılabilir): kart açılışı, saat, DWT, ICACHE ve metin çıktısı. */
#ifndef MP5_KART_H
#define MP5_KART_H

#include "stm32u5xx_hal.h"

#include <stdint.h>

#define MP5_HEDEF_HZ 160000000u

/* HAL, güç, 160 MHz saat (MP1'in düzeltilmiş dizisi), ICACHE açık, LED, USART1 115200 8N1, DWT.
   Dönüş: 0 başarılı; sıfırdan farklıysa saat kurulumunun başarısız adımı. */
uint32_t kart_baslat(void);
uint64_t kart_saat_hz(void);
uint32_t kart_uart_hata(void);
uint32_t kart_saat_hal_rc(void);

/* ICACHE: aç, kapat, boşalt (geçersiz kıl; bitene dek bekler). */
void kart_icache_ac(void);
void kart_icache_kapat(void);
static inline void kart_icache_bosalt(void)
{
    ICACHE->FCR = ICACHE_FCR_CBSYENDF;
    ICACHE->CR |= ICACHE_CR_CACHEINV;
    while ((ICACHE->SR & ICACHE_SR_BSYENDF) == 0u) {
    }
    ICACHE->FCR = ICACHE_FCR_CBSYENDF;
}

static inline uint32_t kart_cevrim(void) { return DWT->CYCCNT; }

/* Metin satırı: "<tip> <sıra> alan=değer ... *<CRC16 hex>\r\n" (MP4a biçimi). Gönderim yoklamalıdır
   ve yalnız ölçülen bölgelerin dışında yapılır. */
typedef struct {
    char b[640];
    uint32_t n;
} satir_t;

void s_bas(satir_t *s, const char *tip);
void s_cat(satir_t *s, const char *t);
void s_u64(satir_t *s, uint64_t v);
void s_kv(satir_t *s, const char *k, uint64_t v);
void s_kx(satir_t *s, const char *k, uint32_t v);
void s_ks(satir_t *s, const char *k, const char *v);
void s_gonder(satir_t *s);
/* Satırın kalan yeri (bayt). */
uint32_t s_kalan(const satir_t *s);

#endif
