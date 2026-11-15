/*
 * W0-MP1a ölçüm prototipi: STM32U5A9J-DK, en küçük aygıt yazılımı.
 * Atılabilir prototip (09 §2.1); ürün koduna taşınmaz. Ürün kuralları (bloklamama, HAL_Delay yasağı) burada uygulanmaz.
 *
 * Yaptığı iş:
 *  - Açılışta ham RCC_CSR sıfırlama bayraklarını, DEV_ID/REV_ID'yi kaydeder (salt okunur).
 *  - DBGMCU'da IWDG dondurma bitini açar, IWDG'yi ~1 s ile başlatır (K1 testi).
 *  - Regülatör (derlemede MP1_REG_SMPS), Range 1, HSE 16 MHz + PLL1 (M=4, N=80, R=2) ile SYSCLK 160 MHz.
 *    Dizi ST'nin STM32U5x9J-DK Ux_Host_CDC_ACM örneğinin SystemClock_Config'idir (STM32CubeU5 v1.9.0).
 *    HSE açılmaz ya da PLL kilitlenmezse başka kaynağa dönülmez; durum seri porttan bildirilir.
 *  - SYSCLK kaynağını ve frekansını yazmaçlardan geri okur, seri porta yazar.
 *  - Yeşil LED'i (PE0) her satırda değiştirir; USART1 (ST-LINK VCP, PA9/PA10) 115200 8N1.
 *  - DWT çevrim sayacını 64 bite genişletir; her 160e6 çevrimde bir "T <sıra> <çevrim>" satırı yazar.
 *    PC tarafı (pc/mp1_seri.cpp) satır varış zamanıyla oranı çıkarır (K2).
 */
#include "stm32u5xx_hal.h"
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#ifndef MP1_REG_SMPS
#error "MP1_REG_SMPS tanımlı değil"
#endif

#define MP1_HEDEF_HZ      160000000u
#define MP1_SATIR_CEVRIM  160000000u /* 160 MHz'te nominal 1 s */
#define MP1_DURUM_ARALIK  60u        /* her 60 satırda bir durum satırı */

/* gdb ve PC tarafının okuduğu değişkenler */
volatile uint32_t mp1_sayac;        /* yazılan T satırı sayısı */
volatile uint32_t mp1_acilis_csr;   /* açılıştaki ham RCC_CSR */
volatile uint32_t mp1_saat_adim;    /* 0: tamam; değilse başarısız adım (1..6) */
volatile uint32_t mp1_saat_hal_rc;  /* başarısız adımın HAL dönüş kodu */
volatile uint32_t mp1_dbg_apb1fzr1; /* DBGMCU_APB1FZR1 geri okuması */
volatile uint32_t mp1_uart_hata;    /* HAL_UART_Transmit hata sayısı (P satırlarında raporlanır) */
volatile uint32_t mp1_gecis_hpre;        /* PLL1'e geçişten hemen sonra RCC_CFGR2.HPRE (÷2 beklenir: 0x8) */
volatile uint32_t mp1_gecis_bekleme_cyc; /* ÷2'de beklenen çevrim (DWT ölçümü) */
volatile uint32_t mp1_gecis_hclk_hz;     /* bekleme sırasında HAL'in HCLK değeri */

#define MP1_GECIS_BEKLEME_US 5u

static UART_HandleTypeDef mp1_uart;
static IWDG_HandleTypeDef mp1_iwdg;
static uint64_t cyc_ust;
static uint32_t cyc_son;

/* ---------- küçük biçimlendirici (printf ailesi kullanılmaz) ---------- */
typedef struct { char b[360]; uint32_t n; } satir_t;

static void s_cat(satir_t *s, const char *t)
{
    while (*t != '\0' && s->n < sizeof s->b - 3u) { s->b[s->n++] = *t++; }
}
static void s_u64(satir_t *s, uint64_t v)
{
    char t[21]; uint32_t i = 0;
    do { t[i++] = (char)('0' + (char)(v % 10u)); v /= 10u; } while (v != 0u);
    while (i > 0u && s->n < sizeof s->b - 3u) { s->b[s->n++] = t[--i]; }
}
static void s_hex(satir_t *s, uint32_t v)
{
    static const char h[] = "0123456789ABCDEF";
    s_cat(s, "0x");
    for (int k = 28; k >= 0; k -= 4) {
        if (s->n < sizeof s->b - 3u) { s->b[s->n++] = h[(v >> (uint32_t)k) & 0xFu]; }
    }
}
static void s_kv(satir_t *s, const char *k, uint64_t v) { s_cat(s, " "); s_cat(s, k); s_cat(s, "="); s_u64(s, v); }
static void s_kx(satir_t *s, const char *k, uint32_t v) { s_cat(s, " "); s_cat(s, k); s_cat(s, "="); s_hex(s, v); }

static void s_gonder(satir_t *s)
{
    s->b[s->n++] = '\r';
    s->b[s->n++] = '\n';
    if (HAL_UART_Transmit(&mp1_uart, (uint8_t *)s->b, (uint16_t)s->n, 200u) != HAL_OK) { mp1_uart_hata++; }
    s->n = 0;
}

/* ---------- DWT ---------- */
static void dwt_baslat(void)
{
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0u;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
    cyc_son = 0u;
    cyc_ust = 0u;
}
/* 26,8 s'den sık çağrılmalı (160 MHz'te 32 bit taşma süresi). */
static uint64_t cyc64(void)
{
    uint32_t c = DWT->CYCCNT;
    cyc_ust += (uint32_t)(c - cyc_son);
    cyc_son = c;
    return cyc_ust;
}

/* ---------- saat ---------- */
static uint32_t saat_hata(uint32_t adim, HAL_StatusTypeDef rc)
{
    mp1_saat_adim = adim;
    mp1_saat_hal_rc = (uint32_t)rc;
    return adim;
}

/* Adımlar: 1 regülatör, 2 Range 1, 3 HSE, 4 PLL1, 5 SYSCLK geçişi (AHB ÷2), 6 geri okuma tutmadı,
   7 AHB ÷1'e dönüş. */
static uint32_t saat_kur(void)
{
    HAL_StatusTypeDef rc;
#if MP1_REG_SMPS
    rc = HAL_PWREx_ConfigSupply(PWR_SMPS_SUPPLY);
#else
    rc = HAL_PWREx_ConfigSupply(PWR_LDO_SUPPLY);
#endif
    if (rc != HAL_OK) { return saat_hata(1u, rc); }

    rc = HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1); /* EPOD BOOSTEN'i de kurar */
    if (rc != HAL_OK) { return saat_hata(2u, rc); }

    RCC_OscInitTypeDef osc = {0};
    osc.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    osc.HSEState = RCC_HSE_ON;
    osc.PLL.PLLState = RCC_PLL_NONE;
    rc = HAL_RCC_OscConfig(&osc);
    if (rc != HAL_OK) { return saat_hata(3u, rc); }

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
    rc = HAL_RCC_OscConfig(&osc);
    if (rc != HAL_OK) { return saat_hata(4u, rc); }

    /* STM32U59x/5Ax: Range 1'de 80 MHz altından üstüne geçişte AHB önce ÷2, SYSCLK PLL1'e geçirilir,
       en az 5 µs beklenir, sonra AHB ÷1 (RM0456 §10.5.4 kısıtı; ST moderatör yanıtı 2024-03-08: 5 µs;
       AN5766: 3 µs, bu ortamdan okunamadı; büyüğü kullanılır). HAL bu adımı kendisi yapmaz; ancak
       bölücü büyürken HPRE'yi SYSCLK geçişinden önce yazar (stm32u5xx_hal_rcc.c, HAL_RCC_ClockConfig). */
    RCC_ClkInitTypeDef clk = {0};
    clk.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_PCLK1 |
                    RCC_CLOCKTYPE_PCLK2 | RCC_CLOCKTYPE_PCLK3;
    clk.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    clk.AHBCLKDivider = RCC_SYSCLK_DIV2;
    clk.APB1CLKDivider = RCC_HCLK_DIV1;
    clk.APB2CLKDivider = RCC_HCLK_DIV1;
    clk.APB3CLKDivider = RCC_HCLK_DIV1;
    rc = HAL_RCC_ClockConfig(&clk, FLASH_LATENCY_4);
    if (rc != HAL_OK) { return saat_hata(5u, rc); }
    mp1_gecis_hpre = RCC->CFGR2 & RCC_CFGR2_HPRE_Msk;

    /* En az 5 µs bekleme, DWT ile (HCLK = 80 MHz'te 400 çevrim). */
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
    const uint32_t gerek = (uint32_t)(((uint64_t)SystemCoreClock * MP1_GECIS_BEKLEME_US + 999999u) / 1000000u);
    const uint32_t bas = DWT->CYCCNT;
    while ((uint32_t)(DWT->CYCCNT - bas) < gerek) { }
    mp1_gecis_bekleme_cyc = DWT->CYCCNT - bas;
    mp1_gecis_hclk_hz = SystemCoreClock;

    memset(&clk, 0, sizeof clk);
    clk.ClockType = RCC_CLOCKTYPE_HCLK;
    clk.AHBCLKDivider = RCC_SYSCLK_DIV1;
    rc = HAL_RCC_ClockConfig(&clk, FLASH_LATENCY_4);
    if (rc != HAL_OK) { return saat_hata(7u, rc); }
    return 0u;
}

typedef struct {
    uint32_t sws, pll1src, m, n, r, rge, mboost, fracen, fracn, ren, latency;
    uint32_t cr, cfgr1, cfgr2, pll1cfgr, pll1divr, pll1fracr, vosr, svmsr, cr3, acr;
    uint64_t hesap_hz;
} saat_okuma_t;

/* SYSCLK'i HAL'e sormadan, doğrudan yazmaçlardan hesaplar (RM0456 RCC_PLL1CFGR/PLL1DIVR alanları). */
static void saat_oku(saat_okuma_t *o)
{
    o->cr = RCC->CR;
    o->cfgr1 = RCC->CFGR1;
    o->pll1cfgr = RCC->PLL1CFGR;
    o->pll1divr = RCC->PLL1DIVR;
    o->pll1fracr = RCC->PLL1FRACR;
    o->vosr = PWR->VOSR;
    o->svmsr = PWR->SVMSR;
    o->cr3 = PWR->CR3;
    o->acr = FLASH->ACR;
    o->cfgr2 = RCC->CFGR2;
    o->sws = (o->cfgr1 & RCC_CFGR1_SWS_Msk) >> RCC_CFGR1_SWS_Pos;
    o->pll1src = (o->pll1cfgr & RCC_PLL1CFGR_PLL1SRC_Msk) >> RCC_PLL1CFGR_PLL1SRC_Pos;
    o->rge = (o->pll1cfgr & RCC_PLL1CFGR_PLL1RGE_Msk) >> RCC_PLL1CFGR_PLL1RGE_Pos;
    o->fracen = (o->pll1cfgr & RCC_PLL1CFGR_PLL1FRACEN_Msk) >> RCC_PLL1CFGR_PLL1FRACEN_Pos;
    o->m = ((o->pll1cfgr & RCC_PLL1CFGR_PLL1M_Msk) >> RCC_PLL1CFGR_PLL1M_Pos) + 1u;
    o->mboost = (o->pll1cfgr & RCC_PLL1CFGR_PLL1MBOOST_Msk) >> RCC_PLL1CFGR_PLL1MBOOST_Pos;
    o->ren = (o->pll1cfgr & RCC_PLL1CFGR_PLL1REN_Msk) >> RCC_PLL1CFGR_PLL1REN_Pos;
    o->n = ((o->pll1divr & RCC_PLL1DIVR_PLL1N_Msk) >> RCC_PLL1DIVR_PLL1N_Pos) + 1u;
    o->r = ((o->pll1divr & RCC_PLL1DIVR_PLL1R_Msk) >> RCC_PLL1DIVR_PLL1R_Pos) + 1u;
    o->latency = (o->acr & FLASH_ACR_LATENCY_Msk) >> FLASH_ACR_LATENCY_Pos;
    o->fracn = (o->fracen != 0u) ? ((o->pll1fracr & RCC_PLL1FRACR_PLL1FRACN_Msk) >> RCC_PLL1FRACR_PLL1FRACN_Pos) : 0u;
    o->hesap_hz = 0u;
    /* Yalnız SYSCLK = PLL1 (SWS = 3) ve PLL1 kaynağı HSE (3) iken hesaplanır.
       f = HSE / M * (N + FRACN / 2^13) / R. HAL, FRACN = 0 olsa da PLL1FRACEN'i kurar. */
    if (o->sws == 3u && o->pll1src == 3u) {
        o->hesap_hz = (uint64_t)HSE_VALUE * ((uint64_t)o->n * 8192u + o->fracn) /
                      ((uint64_t)o->m * o->r * 8192u);
    }
}

static void saat_yaz(uint32_t sira)
{
    saat_okuma_t o;
    saat_oku(&o);
    satir_t s = { .n = 0 };
    s_cat(&s, "K");
    s_kv(&s, "sira", sira);
    s_kv(&s, "sws", o.sws);
    s_cat(&s, o.sws == 3u ? " kaynak=PLL1" : " kaynak=PLL1_DEGIL");
    s_kv(&s, "pll1src", o.pll1src);
    s_kv(&s, "m", o.m);
    s_kv(&s, "n", o.n);
    s_kv(&s, "r", o.r);
    s_kv(&s, "rge", o.rge);
    s_kv(&s, "mboost", o.mboost);
    s_kv(&s, "fracen", o.fracen);
    s_kv(&s, "fracn", o.fracn);
    s_kv(&s, "hesap_hz", o.hesap_hz);
    s_kv(&s, "hal_hz", HAL_RCC_GetSysClockFreq());
    s_kv(&s, "latency", o.latency);
    s_kv(&s, "hserdy", (o.cr & RCC_CR_HSERDY) != 0u);
    s_kv(&s, "pll1rdy", (o.cr & RCC_CR_PLL1RDY) != 0u);
    s_gonder(&s);
    s_cat(&s, "P");
    s_kv(&s, "sira", sira);
    s_cat(&s, (o.svmsr & PWR_SVMSR_REGS) != 0u ? " regs=SMPS" : " regs=LDO");
    s_kv(&s, "actvos", (o.svmsr & PWR_SVMSR_ACTVOS_Msk) >> PWR_SVMSR_ACTVOS_Pos);
    s_kv(&s, "vos", (o.vosr & PWR_VOSR_VOS_Msk) >> PWR_VOSR_VOS_Pos);
    s_kv(&s, "vosrdy", (o.vosr & PWR_VOSR_VOSRDY) != 0u);
    s_kv(&s, "boosten", (o.vosr & PWR_VOSR_BOOSTEN) != 0u);
    s_kv(&s, "boostrdy", (o.vosr & PWR_VOSR_BOOSTRDY) != 0u);
    s_kx(&s, "rcc_cr", o.cr);
    s_kx(&s, "cfgr1", o.cfgr1);
    s_kx(&s, "pll1cfgr", o.pll1cfgr);
    s_kx(&s, "pll1divr", o.pll1divr);
    s_kx(&s, "pll1fracr", o.pll1fracr);
    s_kx(&s, "pwr_vosr", o.vosr);
    s_kx(&s, "pwr_svmsr", o.svmsr);
    s_kx(&s, "flash_acr", o.acr);
    s_kx(&s, "cfgr2", o.cfgr2);
    s_kv(&s, "uart_hata", mp1_uart_hata);
    s_gonder(&s);
}

static bool saat_dogru(void)
{
    saat_okuma_t o;
    saat_oku(&o);
    return o.sws == 3u && o.pll1src == 3u && o.hesap_hz == MP1_HEDEF_HZ &&
           (o.cfgr2 & RCC_CFGR2_HPRE_Msk) == 0u &&
           (o.cr & RCC_CR_HSERDY) != 0u && (o.cr & RCC_CR_PLL1RDY) != 0u;
}

/* ---------- çevre birimleri ---------- */
static void led_baslat(void)
{
    __HAL_RCC_GPIOE_CLK_ENABLE();
    GPIO_InitTypeDef g = {0};
    g.Pin = GPIO_PIN_0 | GPIO_PIN_1; /* PE0 yeşil (LD3), PE1 kırmızı (LD4); DK BSP'si, etkin yüksek */
    g.Mode = GPIO_MODE_OUTPUT_PP;
    g.Pull = GPIO_NOPULL;
    g.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOE, &g);
    HAL_GPIO_WritePin(GPIOE, GPIO_PIN_0 | GPIO_PIN_1, GPIO_PIN_RESET);
}

void HAL_UART_MspInit(UART_HandleTypeDef *h)
{
    if (h->Instance != USART1) { return; }
    __HAL_RCC_USART1_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();
    GPIO_InitTypeDef g = {0};
    g.Pin = GPIO_PIN_9 | GPIO_PIN_10;
    g.Mode = GPIO_MODE_AF_PP;
    g.Pull = GPIO_PULLUP;
    g.Speed = GPIO_SPEED_FREQ_LOW;
    g.Alternate = GPIO_AF7_USART1;
    HAL_GPIO_Init(GPIOA, &g);
}

static void uart_baslat(void)
{
    mp1_uart.Instance = USART1;
    mp1_uart.Init.BaudRate = 115200u;
    mp1_uart.Init.WordLength = UART_WORDLENGTH_8B;
    mp1_uart.Init.StopBits = UART_STOPBITS_1;
    mp1_uart.Init.Parity = UART_PARITY_NONE;
    mp1_uart.Init.Mode = UART_MODE_TX_RX;
    mp1_uart.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    mp1_uart.Init.OverSampling = UART_OVERSAMPLING_16;
    mp1_uart.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
    mp1_uart.Init.ClockPrescaler = UART_PRESCALER_DIV1;
    (void)HAL_UART_Init(&mp1_uart);
}

static void iwdg_baslat(void)
{
    /* Hata ayıklama dondurma biti: çekirdek durunca IWDG sayacı da durur (DBGMCU_APB1FZR1.DBG_IWDG_STOP). */
    __HAL_DBGMCU_FREEZE_IWDG();
    mp1_dbg_apb1fzr1 = DBGMCU->APB1FZR1;
    /* LSI ~32 kHz, bölücü 32 -> ~1 kHz; reload 999 -> nominal ~1,0 s (LSI sapması ölçülmedi). */
    mp1_iwdg.Instance = IWDG;
    mp1_iwdg.Init.Prescaler = IWDG_PRESCALER_32;
    mp1_iwdg.Init.Reload = 999u;
    mp1_iwdg.Init.Window = IWDG_WINDOW_DISABLE;
    mp1_iwdg.Init.EWI = 0u;
    (void)HAL_IWDG_Init(&mp1_iwdg);
}

/* Kesme noktası hedefi: main -> satir_gonder -> mp1_isaret (yığın izi denemesi). */
__attribute__((noinline)) void mp1_isaret(uint32_t n)
{
    mp1_sayac = n;
    __asm volatile("" ::: "memory");
}

__attribute__((noinline)) static void satir_gonder(uint32_t sira, uint64_t cyc)
{
    satir_t s = { .n = 0 };
    s_cat(&s, "T ");
    s_u64(&s, sira);
    s_cat(&s, " ");
    s_u64(&s, cyc);
    s_gonder(&s);
    HAL_GPIO_TogglePin(GPIOE, GPIO_PIN_0);
    mp1_isaret(sira);
}

static void acilis_yaz(void)
{
    satir_t s = { .n = 0 };
    s_gonder(&s); /* açılıştaki hat gürültüsünü ayrı satırda bırakmak için boş satır */
    s_cat(&s, "B MP1a");
    s_cat(&s, MP1_REG_SMPS ? " reg_istenen=SMPS" : " reg_istenen=LDO");
    s_kx(&s, "rcc_csr", mp1_acilis_csr);
    s_kv(&s, "iwdgrst", (mp1_acilis_csr & RCC_CSR_IWDGRSTF) != 0u);
    s_kv(&s, "pinrst", (mp1_acilis_csr & RCC_CSR_PINRSTF) != 0u);
    s_kv(&s, "borrst", (mp1_acilis_csr & RCC_CSR_BORRSTF) != 0u);
    s_kv(&s, "sftrst", (mp1_acilis_csr & RCC_CSR_SFTRSTF) != 0u);
    s_kx(&s, "idcode", DBGMCU->IDCODE);
    s_kx(&s, "dbg_apb1fzr1", mp1_dbg_apb1fzr1);
    s_kv(&s, "saat_adim", mp1_saat_adim);
    s_kv(&s, "saat_hal_rc", mp1_saat_hal_rc);
    s_kx(&s, "gecis_hpre", mp1_gecis_hpre);
    s_kv(&s, "gecis_bekleme_cyc", mp1_gecis_bekleme_cyc);
    s_kv(&s, "gecis_hclk_hz", mp1_gecis_hclk_hz);
    s_gonder(&s);
}

int main(void)
{
    mp1_acilis_csr = RCC->CSR;
    RCC->CSR |= RCC_CSR_RMVF; /* sıfırlama bayraklarını temizle; bir sonraki açılış yeni nedeni görür */

    HAL_Init();                 /* MSIS 4 MHz üzerinde SysTick 1 ms */
    __HAL_RCC_PWR_CLK_ENABLE(); /* PWR saati, ConfigSupply'dan önce (00 Ç4) */
    HAL_PWREx_DisableUCPDDeadBattery();
    iwdg_baslat();

    uint32_t adim = saat_kur();
    if (adim == 0u && !saat_dogru()) { adim = saat_hata(6u, HAL_OK); }

    led_baslat();
    uart_baslat();              /* BRR, o anki PCLK2'ye göre (saat hatasında MSIS 4 MHz) */
    acilis_yaz();
    saat_yaz(0u);

    if (adim != 0u) {
        /* Sessizce başka kaynağa dönülmez: saat hatası bildirilir, ölçüm yapılmaz. */
        HAL_GPIO_WritePin(GPIOE, GPIO_PIN_1, GPIO_PIN_SET);
        for (uint32_t k = 1u;; k++) {
            satir_t s = { .n = 0 };
            s_cat(&s, "E SAAT_HATASI");
            s_kv(&s, "adim", adim);
            s_kv(&s, "hal_rc", mp1_saat_hal_rc);
            s_kv(&s, "k", k);
            s_gonder(&s);
            (void)HAL_IWDG_Refresh(&mp1_iwdg);
            HAL_Delay(500u);
            (void)HAL_IWDG_Refresh(&mp1_iwdg);
            HAL_Delay(400u);
        }
    }

    dwt_baslat();
    uint64_t sonraki = MP1_SATIR_CEVRIM;
    uint32_t sira = 0u;
    for (;;) {
        (void)HAL_IWDG_Refresh(&mp1_iwdg);
        uint64_t simdi = cyc64();
        if (simdi >= sonraki) {
            sira++;
            satir_gonder(sira, simdi);
            sonraki += MP1_SATIR_CEVRIM;
            if (sira % MP1_DURUM_ARALIK == 0u) {
                saat_yaz(sira);
                if (!saat_dogru()) {
                    HAL_GPIO_WritePin(GPIOE, GPIO_PIN_1, GPIO_PIN_SET);
                    satir_t s = { .n = 0 };
                    s_cat(&s, "E SAAT_DEGISTI");
                    s_kv(&s, "sira", sira);
                    s_gonder(&s);
                }
            }
        }
    }
}
