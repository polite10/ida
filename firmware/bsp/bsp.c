/*
 * bsp.c — başlatma, güç ve saat, LED, RNG, kesme maskeleme, yığın ölçümü (bsp.h).
 *
 * Saat dizisi MP1/MP3/MP4/MP5 prototiplerinde kartta kanıtlanan dizidir
 * (tools/olcum/mp3/fw/platform.c saat_kur): SMPS, Range 1, HSE 16 MHz + PLL1 M=4 N=80 R=2,
 * AHB ÷2 -> >= 5 µs -> ÷1 (MP1 bulgusu), FLASH_LATENCY_4. Bu dizi ayrıca HSI48'i açar (RNG).
 */
#include "bsp.h"

#include <string.h>

#include "stm32u5xx_hal.h"

/* Sıfırlama bayraklarının aygıt başlığıyla eşitliği (acilis_kaydi.h donanımdan bağımsızdır). */
_Static_assert(IDA_CSR_OBLRSTF == RCC_CSR_OBLRSTF, "OBLRSTF");
_Static_assert(IDA_CSR_PINRSTF == RCC_CSR_PINRSTF, "PINRSTF");
_Static_assert(IDA_CSR_BORRSTF == RCC_CSR_BORRSTF, "BORRSTF");
_Static_assert(IDA_CSR_SFTRSTF == RCC_CSR_SFTRSTF, "SFTRSTF");
_Static_assert(IDA_CSR_IWDGRSTF == RCC_CSR_IWDGRSTF, "IWDGRSTF");
_Static_assert(IDA_CSR_WWDGRSTF == RCC_CSR_WWDGRSTF, "WWDGRSTF");
_Static_assert(IDA_CSR_LPWRRSTF == RCC_CSR_LPWRRSTF, "LPWRRSTF");
_Static_assert(__NVIC_PRIO_BITS == 4, "01 §2.4: dört öncelik biti");

/* .noinit'teki açılış ve hata kaydı (bağlayıcı betiği: SRAM2'nin başı). */
__attribute__((section(".noinit"))) ida_acilis_kaydi_t ida_bsp_noinit_kayit;

static ida_acilis_sonuc_t acilis;
static RNG_HandleTypeDef rng;
static uint32_t rng_hata;

volatile uint32_t ida_bsp_adim_sayaci;

extern uint32_t _ida_yigin_tabani[] __attribute__((weak));
extern uint32_t _estack[];

void SysTick_Handler(void)
{
    HAL_IncTick();
}

/* ---- gözetim (01 §2.2 md.4, §2.8) --------------------------------------------------------- */

#define IWDG_ANAHTAR_BASLAT 0xCCCCu
#define IWDG_ANAHTAR_ERISIM 0x5555u
#define IWDG_ANAHTAR_BESLE  0xAAAAu

/* Bölücü ve yeniden yükleme değerini yazar; güncellemenin bitmesini sınırlı bekler. */
static void iwdg_kur(uint32_t pr, uint32_t rlr)
{
    IWDG->KR = IWDG_ANAHTAR_ERISIM;
    IWDG->PR = pr;
    IWDG->RLR = rlr;
    /* PVU/RVU 32 kHz LSI'nin birkaç çevriminde iner; en çok ≈ 10 ms (sınır, tahmin). */
    for (uint32_t i = 0u; i < 2000000u && (IWDG->SR & (IWDG_SR_PVU | IWDG_SR_RVU)) != 0u; i++) {
        __NOP();
    }
    IWDG->KR = IWDG_ANAHTAR_BESLE;
}

void ida_bsp_iwdg_besle(void)
{
    IWDG->KR = IWDG_ANAHTAR_BESLE;
}

/* ---- başlatma ------------------------------------------------------------------------------ */

void ida_bsp_erken_baslat(void)
{
    /* NVIC öncelik gruplaması 4 bit önalım (HAL_Init); SysTick 1 ms, MSIS 4 MHz üzerinde. */
    (void)HAL_Init();
    /* MemManage, BusFault, UsageFault ayrı ayrı açılır (açılmazsa HardFault'a yükselir); öncelik
       0 (01 §2.4 tablosu). */
    HAL_NVIC_SetPriority(MemoryManagement_IRQn, 0u, 0u);
    HAL_NVIC_SetPriority(BusFault_IRQn, 0u, 0u);
    HAL_NVIC_SetPriority(UsageFault_IRQn, 0u, 0u);
    SCB->SHCSR |= SCB_SHCSR_MEMFAULTENA_Msk | SCB_SHCSR_BUSFAULTENA_Msk | SCB_SHCSR_USGFAULTENA_Msk;

    /* Sıfırlama nedeni: ham bayraklar saklanır ve temizlenir; .noinit kaydı okunur. */
    const uint32_t csr = RCC->CSR;
    RCC->CSR |= RCC_CSR_RMVF;
    ida_kayit_acilis(&ida_bsp_noinit_kayit, csr, &acilis);

#if defined(IDA_FW_IWDG_DONDUR)
    /* Hata ayıklama derlemesi: çekirdek durunca IWDG de durur (MP1 bulgusu 7; 01 §2.2 md.4). */
    DBGMCU->APB1FZR1 |= DBGMCU_APB1FZR1_DBG_IWDG_STOP;
#endif
    /* IWDG: bölücü 16 (PR = 2), yeniden yükleme 3999 -> nominal 2,0 s (32 kHz LSI). Güç ve PLL
       beklemelerinden önce başlar (01 §2.2 md.4). */
    IWDG->KR = IWDG_ANAHTAR_BASLAT;
    iwdg_kur(2u, 3999u);
}

void ida_bsp_calisma_kipi(void)
{
    /* IWDG: bölücü 4 (PR = 0), yeniden yükleme 799 -> nominal 100 ms (01 §2.8). */
    iwdg_kur(0u, 799u);
    /* Başlatmadan sonra HAL zaman tabanı kullanılmaz: SysTick durur (01 §2.4 bloklayan çağrı
       yasağı; kontrol döngüsünde ek kesme olmaz). */
    SysTick->CTRL = 0u;
    SCB->ICSR = SCB_ICSR_PENDSTCLR_Msk;
}

static void dwt_baslat(void)
{
    DCB->DEMCR |= DCB_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0u;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}

uint32_t ida_bsp_sysclk_hz(void)
{
    const uint32_t cfgr1 = RCC->CFGR1, pc = RCC->PLL1CFGR, pd = RCC->PLL1DIVR;
    const uint32_t sws = (cfgr1 & RCC_CFGR1_SWS_Msk) >> RCC_CFGR1_SWS_Pos;
    const uint32_t src = (pc & RCC_PLL1CFGR_PLL1SRC_Msk) >> RCC_PLL1CFGR_PLL1SRC_Pos;
    if (sws != 3u || src != 3u) {
        return 0u;
    }
    const uint32_t m = ((pc & RCC_PLL1CFGR_PLL1M_Msk) >> RCC_PLL1CFGR_PLL1M_Pos) + 1u;
    const uint32_t n = ((pd & RCC_PLL1DIVR_PLL1N_Msk) >> RCC_PLL1DIVR_PLL1N_Pos) + 1u;
    const uint32_t r = ((pd & RCC_PLL1DIVR_PLL1R_Msk) >> RCC_PLL1DIVR_PLL1R_Pos) + 1u;
    return (uint32_t)((uint64_t)HSE_VALUE * n / ((uint64_t)m * r));
}

uint32_t ida_bsp_saat_kur(void)
{
    __HAL_RCC_PWR_CLK_ENABLE();
    HAL_PWREx_DisableUCPDDeadBattery();
    if (HAL_PWREx_ConfigSupply(PWR_SMPS_SUPPLY) != HAL_OK) {
        return 1u;
    }
    if (HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1) != HAL_OK) {
        return 2u;
    }
    RCC_OscInitTypeDef osc;
    memset(&osc, 0, sizeof osc);
    osc.OscillatorType = RCC_OSCILLATORTYPE_HSE | RCC_OSCILLATORTYPE_HSI48;
    osc.HSEState = RCC_HSE_ON;
    osc.HSI48State = RCC_HSI48_ON;
    osc.PLL.PLLState = RCC_PLL_NONE;
    if (HAL_RCC_OscConfig(&osc) != HAL_OK) {
        return 3u;
    }
    memset(&osc, 0, sizeof osc);
    osc.OscillatorType = RCC_OSCILLATORTYPE_NONE;
    osc.PLL.PLLState = RCC_PLL_ON;
    osc.PLL.PLLSource = RCC_PLLSOURCE_HSE;
    osc.PLL.PLLMBOOST = RCC_PLLMBOOST_DIV1;
    osc.PLL.PLLM = 4;
    osc.PLL.PLLN = 80;
    osc.PLL.PLLP = 8;
    osc.PLL.PLLQ = 2;
    osc.PLL.PLLR = 2;
    osc.PLL.PLLRGE = RCC_PLLVCIRANGE_0;
    osc.PLL.PLLFRACN = 0;
    if (HAL_RCC_OscConfig(&osc) != HAL_OK) {
        return 4u;
    }
    RCC_ClkInitTypeDef clk;
    memset(&clk, 0, sizeof clk);
    clk.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_PCLK1 |
                    RCC_CLOCKTYPE_PCLK2 | RCC_CLOCKTYPE_PCLK3;
    clk.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    clk.AHBCLKDivider = RCC_SYSCLK_DIV2; /* geçici ÷2 (STM32U59x/5Ax kısıtı, MP1) */
    clk.APB1CLKDivider = RCC_HCLK_DIV1;
    clk.APB2CLKDivider = RCC_HCLK_DIV1;
    clk.APB3CLKDivider = RCC_HCLK_DIV1;
    if (HAL_RCC_ClockConfig(&clk, FLASH_LATENCY_4) != HAL_OK) {
        return 5u;
    }
    dwt_baslat();
    /* >= 5 µs bekleme (SystemCoreClock şu anda AHB = 80 MHz'i gösterir). */
    const uint32_t gerek = (uint32_t)(((uint64_t)SystemCoreClock * 5u + 999999u) / 1000000u);
    const uint32_t bas = DWT->CYCCNT;
    while ((uint32_t)(DWT->CYCCNT - bas) < gerek) {
    }
    memset(&clk, 0, sizeof clk);
    clk.ClockType = RCC_CLOCKTYPE_HCLK;
    clk.AHBCLKDivider = RCC_SYSCLK_DIV1;
    if (HAL_RCC_ClockConfig(&clk, FLASH_LATENCY_4) != HAL_OK) {
        return 7u;
    }
    if (ida_bsp_sysclk_hz() != IDA_BSP_SYSCLK_HZ || SystemCoreClock != IDA_BSP_SYSCLK_HZ) {
        return 6u;
    }
    return 0u;
}

void ida_bsp_yeniden_baslat(void)
{
    __DSB();
    NVIC_SystemReset();
}

/* ---- LED ----------------------------------------------------------------------------------- */

static void led_baslat(void)
{
    __HAL_RCC_GPIOE_CLK_ENABLE();
    GPIO_InitTypeDef g;
    memset(&g, 0, sizeof g);
    g.Pin = GPIO_PIN_0 | GPIO_PIN_1; /* PE0 yeşil, PE1 kırmızı (BSP: LED etkin düşük değil) */
    g.Mode = GPIO_MODE_OUTPUT_PP;
    g.Pull = GPIO_NOPULL;
    g.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOE, &g);
    HAL_GPIO_WritePin(GPIOE, GPIO_PIN_0 | GPIO_PIN_1, GPIO_PIN_RESET);
}

void ida_bsp_led_yesil(bool yak)
{
    GPIOE->BSRR = yak ? GPIO_PIN_0 : ((uint32_t)GPIO_PIN_0 << 16);
}

void ida_bsp_led_kirmizi(bool yak)
{
    GPIOE->BSRR = yak ? GPIO_PIN_1 : ((uint32_t)GPIO_PIN_1 << 16);
}

void ida_bsp_led_yesil_cevir(void)
{
    ida_bsp_led_yesil((GPIOE->ODR & GPIO_PIN_0) == 0u);
}

/* ---- RNG ----------------------------------------------------------------------------------- */

static void rng_baslat(void)
{
    __HAL_RCC_RNG_CLK_ENABLE(); /* çekirdek saati HSI48 (RCC_CCIPR2.RNGSEL sıfırlama değeri) */
    memset(&rng, 0, sizeof rng);
    rng.Instance = RNG;
    rng.Init.ClockErrorDetection = RNG_CED_ENABLE;
    if (HAL_RNG_Init(&rng) != HAL_OK) {
        rng_hata++;
    }
}

uint32_t ida_bsp_rastgele(void)
{
    /* Yazmaç düzeyinde, sınırlı yoklama (SysTick durduktan sonra da çağrılır; HAL'in zaman
       aşımı HAL_GetTick'e dayanır). Saat ya da tohum hatasında 0 döner. */
    for (uint32_t deneme = 0u; deneme < 4u; deneme++) {
        uint32_t i = 0u;
        while ((RNG->SR & RNG_SR_DRDY) == 0u && i < 200000u) {
            if ((RNG->SR & (RNG_SR_SECS | RNG_SR_CECS)) != 0u) {
                rng_hata++;
                return 0u;
            }
            i++;
        }
        if ((RNG->SR & RNG_SR_DRDY) == 0u) {
            rng_hata++;
            return 0u;
        }
        const uint32_t s = RNG->DR;
        if (s != 0u) {
            return s;
        }
    }
    return 0u;
}

/* ---- çevre --------------------------------------------------------------------------------- */

void ida_bsp_zaman_baslat(void); /* zaman.c */

void ida_bsp_cevre_baslat(void)
{
    (void)HAL_ICACHE_Enable();
    led_baslat();
    dwt_baslat();
    ida_bsp_zaman_baslat();
    rng_baslat();
}

const ida_acilis_sonuc_t *ida_bsp_acilis(void)
{
    return &acilis;
}

const ida_acilis_kaydi_t *ida_bsp_kayit(void)
{
    return &ida_bsp_noinit_kayit;
}

/* ---- kesme kaydı ve NVIC bekçisi --------------------------------------------------------- */

#define IDA_KESME_AZAMI 8u
static IRQn_Type kesmeler[IDA_KESME_AZAMI];
static uint32_t oncelikler[IDA_KESME_AZAMI];
static uint32_t kesme_sayisi;
static uint32_t nvic_yeniden;

#define IDA_HATA_ETKIN (SCB_SHCSR_MEMFAULTENA_Msk | SCB_SHCSR_BUSFAULTENA_Msk | SCB_SHCSR_USGFAULTENA_Msk)

void ida_bsp_kesme_ac(int32_t irqn, uint32_t oncelik)
{
    HAL_NVIC_SetPriority((IRQn_Type)irqn, oncelik, 0u);
    HAL_NVIC_EnableIRQ((IRQn_Type)irqn);
    if (kesme_sayisi < IDA_KESME_AZAMI) {
        oncelikler[kesme_sayisi] = oncelik;
        kesmeler[kesme_sayisi++] = (IRQn_Type)irqn;
    }
}

uint32_t ida_bsp_nvic_bekci(void)
{
    for (uint32_t i = 0u; i < kesme_sayisi; i++) {
        /* Öncelik de denetlenir: programlayıcı bağlanınca öncelikler 0'a dönüyor (kartta
           gözlendi); seri kesmeleri 0'da kalırsa ana bağlamın BASEPRI = 3 maskesi onları
           tutmaz ve alım halkasının paylaşılan durumu yarışa açılır (veri bozulması). */
        if (NVIC_GetPriority(kesmeler[i]) != oncelikler[i]) {
            NVIC_DisableIRQ(kesmeler[i]);
            NVIC_SetPriority(kesmeler[i], oncelikler[i]);
            __DSB();
            __ISB();
            nvic_yeniden++;
        }
        if (NVIC_GetEnableIRQ(kesmeler[i]) == 0u) {
            NVIC_EnableIRQ(kesmeler[i]);
            nvic_yeniden++;
        }
    }
    if ((SCB->SHCSR & IDA_HATA_ETKIN) != IDA_HATA_ETKIN) {
        SCB->SHCSR |= IDA_HATA_ETKIN;
        nvic_yeniden++;
    }
    return nvic_yeniden;
}

/* ---- maskeleme ----------------------------------------------------------------------------- */

uint32_t ida_bsp_maske_ac(uint32_t oncelik)
{
    const uint32_t onceki = __get_BASEPRI();
    __set_BASEPRI_MAX((oncelik << (8u - __NVIC_PRIO_BITS)) & 0xFFu);
    __ISB();
    return onceki;
}

void ida_bsp_maske_birak(uint32_t onceki)
{
    __set_BASEPRI(onceki);
}

/* ---- yığın --------------------------------------------------------------------------------- */

uint32_t ida_bsp_yigin_boyu(void)
{
    if ((uintptr_t)_ida_yigin_tabani == 0u) {
        return 0u;
    }
    return (uint32_t)((uintptr_t)_estack - (uintptr_t)_ida_yigin_tabani);
}

uint32_t ida_bsp_yigin_en_yuksek(void)
{
    if ((uintptr_t)_ida_yigin_tabani == 0u) {
        return 0u;
    }
    const volatile uint32_t *s = _ida_yigin_tabani;
    const volatile uint32_t *son = _estack;
    while (s < son && *s == 0xA5A5A5A5u) {
        ++s;
    }
    return (uint32_t)((uintptr_t)son - (uintptr_t)s);
}
