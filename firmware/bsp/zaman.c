/*
 * zaman.c — duvar saati ve 200 Hz ızgara (01 §2.5; W3-kart dilim 1).
 *
 * TIM2 (32 bit; APB1 zamanlayıcı saati 160 MHz, PSC 159) 1 MHz sayar. Taşma kesmesi (UIF)
 * üst yarıyı artırır; 2^32 µs ≈ 71,6 dk'da bir. Karşılaştırma kanalı 1 her 5 000 µs'de bir
 * ızgara kesmesi üretir: kesme CCR1'i 5 000 ilerletir (mod 2^32), ızgara sayacını artırır ve
 * ızgara anını kaydeder. Izgara mutlak zamandadır (açılıştan beri 5 ms'nin katları);
 * kaçırılan anlar birikmez, ana döngü sayaç farkıyla sayar.
 *
 * 64 bit okuma (01 §2.5, 01-sol md.18): PRIMASK altında üst yarı h ve sayaç l okunur, UIF en
 * son okunur; UIF kalkmışsa ve l < 2^31 ise h + 1 kullanılır.
 */
#include "bsp.h"

#include "stm32u5xx_hal.h"

static volatile uint32_t ust;           /* taşma sayısı                 */
static volatile uint32_t izgara_sayac;  /* 200 Hz ızgara anları         */
static volatile uint64_t izgara_ani;    /* son ızgara anının duvar saati */

/* Kesmeler kapalıyken çağrılır. */
static uint64_t zaman_us_kilitli(void)
{
    uint32_t h = ust;
    const uint32_t l = TIM2->CNT;
    if ((TIM2->SR & TIM_SR_UIF) != 0u && l < 0x80000000u) {
        h++;
    }
    return ((uint64_t)h << 32) | l;
}

uint64_t ida_bsp_zaman_us(void)
{
    const uint32_t m = __get_PRIMASK();
    __disable_irq();
    const uint64_t t = zaman_us_kilitli();
    __set_PRIMASK(m);
    return t;
}

void ida_bsp_cevrim_ve_zaman(uint32_t *cevrim, uint64_t *t_us)
{
    const uint32_t m = __get_PRIMASK();
    __disable_irq();
    *t_us = zaman_us_kilitli();
    *cevrim = DWT->CYCCNT;
    __set_PRIMASK(m);
}

uint32_t ida_bsp_izgara_sayaci(void)
{
    return izgara_sayac;
}

uint64_t ida_bsp_izgara_ani_us(uint32_t *sayac)
{
    const uint32_t m = __get_PRIMASK();
    __disable_irq();
    const uint64_t t = izgara_ani;
    *sayac = izgara_sayac;
    __set_PRIMASK(m);
    return t;
}

void ida_bsp_zaman_baslat(void)
{
    __HAL_RCC_TIM2_CLK_ENABLE();
    /* URS = 1: UIF yalnız sayaç taşmasında kurulur; aşağıdaki UG (bölücüyü yükler) UIF kurmaz
       (kurarsa üst yarı açılışta bir fazla sayılıyordu; kartta gözlendi). */
    TIM2->CR1 = TIM_CR1_URS;
    TIM2->PSC = (IDA_BSP_SYSCLK_HZ / 1000000u) - 1u;
    TIM2->ARR = 0xFFFFFFFFu;
    TIM2->CCMR1 = 0u; /* kanal 1: karşılaştırma, çıkış yok */
    TIM2->EGR = TIM_EGR_UG;
    TIM2->SR = 0u;
    ust = 0u;
    izgara_sayac = 0u;
    TIM2->CCR1 = IDA_BSP_IZGARA_US;
    TIM2->DIER = TIM_DIER_UIE | TIM_DIER_CC1IE;
    ida_bsp_kesme_ac((int32_t)TIM2_IRQn, IDA_BSP_ONCELIK_ZAMAN);
    TIM2->CR1 = TIM_CR1_URS | TIM_CR1_CEN;
}

void TIM2_IRQHandler(void)
{
    const uint32_t sr = TIM2->SR;
    if ((sr & TIM_SR_UIF) != 0u) {
        TIM2->SR = ~TIM_SR_UIF; /* rc_w0: yalnız UIF temizlenir */
        ust = ust + 1u;
    }
    if ((sr & TIM_SR_CC1IF) != 0u) {
        TIM2->SR = ~TIM_SR_CC1IF;
        /* Son geçen ızgara anı `an`; sıradaki an geçmişte kaldıysa (kesme 5 ms'den uzun
           gecikti) geçen bütün anlar sayılır ve karşılaştırma gelecekteki ilk ana kurulur;
           böylece karşılaştırma hiçbir zaman 2^32 µs'lik tura kaçmaz. */
        uint32_t an = TIM2->CCR1;
        uint32_t yeni = an + IDA_BSP_IZGARA_US;
        uint32_t gecen = 1u;
        while ((int32_t)(TIM2->CNT - yeni) >= 0) {
            an = yeni;
            yeni += IDA_BSP_IZGARA_US;
            gecen++;
        }
        TIM2->CCR1 = yeni;
        /* Izgara anının 64 bit değeri: önce şimdiki zamanın üst yarısı (okuma kuralıyla), sonra
           an sayaç taşmadan önceyse (an > l) bir eksiği. */
        uint32_t h = ust;
        const uint32_t l = TIM2->CNT;
        if ((TIM2->SR & TIM_SR_UIF) != 0u && l < 0x80000000u) {
            h = h + 1u;
        }
        if (l < an) {
            h = h - 1u;
        }
        izgara_ani = ((uint64_t)h << 32) | an;
        izgara_sayac = izgara_sayac + gecen;
    }
}
