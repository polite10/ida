/*
 * W0-MP5a ölçüm prototipi (atılabilir; ürün koduna taşınmaz). Platform kodu gnu17.
 * Saat kurulumu, UART ve DWT MP4a'nın platform.c'sinden (tools/olcum/mp4/fw/platform.c, main'de)
 * kopyalanıp kısaltıldı: SMPS, Range 1, HSE 16 MHz + PLL1 M=4 N=80 R=2; AHB ÷2 -> >= 5 µs -> ÷1
 * (MP1 bulgusu), 160 MHz. Flash gecikmesi 4, önden getirme açık (HAL_Init), ICACHE açık.
 * Çıktı USART1 (ST-LINK sanal seri portu), 115200 8N1, yoklamalı.
 */
#include "kart.h"

#include <string.h>

static UART_HandleTypeDef uart;
static uint32_t saat_adim, saat_hal_rc, uart_hata;
static uint32_t satir_sira;

static uint32_t saat_hata(uint32_t adim, HAL_StatusTypeDef rc)
{
    saat_adim = adim;
    saat_hal_rc = (uint32_t)rc;
    return adim;
}

static void dwt_baslat(void)
{
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0u;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}

static uint32_t saat_kur(void)
{
    HAL_StatusTypeDef rc = HAL_PWREx_ConfigSupply(PWR_SMPS_SUPPLY);
    if (rc != HAL_OK) { return saat_hata(1u, rc); }
    rc = HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1);
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
    dwt_baslat();
    const uint32_t gerek = (uint32_t)(((uint64_t)SystemCoreClock * 5u + 999999u) / 1000000u);
    const uint32_t bas = DWT->CYCCNT;
    while ((uint32_t)(DWT->CYCCNT - bas) < gerek) { }
    memset(&clk, 0, sizeof clk);
    clk.ClockType = RCC_CLOCKTYPE_HCLK;
    clk.AHBCLKDivider = RCC_SYSCLK_DIV1;
    rc = HAL_RCC_ClockConfig(&clk, FLASH_LATENCY_4);
    if (rc != HAL_OK) { return saat_hata(7u, rc); }
    return 0u;
}

uint64_t kart_saat_hz(void)
{
    const uint32_t cfgr1 = RCC->CFGR1, pc = RCC->PLL1CFGR, pd = RCC->PLL1DIVR;
    const uint32_t sws = (cfgr1 & RCC_CFGR1_SWS_Msk) >> RCC_CFGR1_SWS_Pos;
    const uint32_t src = (pc & RCC_PLL1CFGR_PLL1SRC_Msk) >> RCC_PLL1CFGR_PLL1SRC_Pos;
    if (sws != 3u || src != 3u) { return 0u; }
    const uint32_t m = ((pc & RCC_PLL1CFGR_PLL1M_Msk) >> RCC_PLL1CFGR_PLL1M_Pos) + 1u;
    const uint32_t n = ((pd & RCC_PLL1DIVR_PLL1N_Msk) >> RCC_PLL1DIVR_PLL1N_Pos) + 1u;
    const uint32_t r = ((pd & RCC_PLL1DIVR_PLL1R_Msk) >> RCC_PLL1DIVR_PLL1R_Pos) + 1u;
    return (uint64_t)HSE_VALUE * n / ((uint64_t)m * r);
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

static uint32_t uart_baslat(void)
{
    uart.Instance = USART1;
    uart.Init.BaudRate = 115200u;
    uart.Init.WordLength = UART_WORDLENGTH_8B;
    uart.Init.StopBits = UART_STOPBITS_1;
    uart.Init.Parity = UART_PARITY_NONE;
    uart.Init.Mode = UART_MODE_TX_RX;
    uart.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    uart.Init.OverSampling = UART_OVERSAMPLING_16;
    uart.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
    uart.Init.ClockPrescaler = UART_PRESCALER_DIV1;
    uint32_t hata = 0u;
    if (HAL_UART_Init(&uart) != HAL_OK) { hata |= 1u; }
    if (HAL_UARTEx_EnableFifoMode(&uart) != HAL_OK) { hata |= 2u; }
    return hata;
}

static void led_baslat(void)
{
    __HAL_RCC_GPIOE_CLK_ENABLE();
    GPIO_InitTypeDef g = {0};
    g.Pin = GPIO_PIN_0 | GPIO_PIN_1;   /* PE0 yeşil, PE1 kırmızı */
    g.Mode = GPIO_MODE_OUTPUT_PP;
    g.Pull = GPIO_NOPULL;
    g.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOE, &g);
    HAL_GPIO_WritePin(GPIOE, GPIO_PIN_0 | GPIO_PIN_1, GPIO_PIN_RESET);
}

uint32_t kart_baslat(void)
{
    HAL_Init();
    __HAL_RCC_PWR_CLK_ENABLE();
    HAL_PWREx_DisableUCPDDeadBattery();
    uint32_t adim = saat_kur();
    if (adim == 0u && kart_saat_hz() != MP5_HEDEF_HZ) { adim = saat_hata(6u, HAL_OK); }
    (void)HAL_ICACHE_Enable();
    led_baslat();
    uart_hata = uart_baslat();
    dwt_baslat();
    return adim;
}

uint32_t kart_uart_hata(void) { return uart_hata; }
uint32_t kart_saat_hal_rc(void) { return saat_hal_rc; }

void kart_icache_ac(void) { (void)HAL_ICACHE_Enable(); }
void kart_icache_kapat(void) { (void)HAL_ICACHE_Disable(); }

/* ---------------- metin çıktısı ---------------- */
static uint16_t crc16(const uint8_t *b, uint32_t n)
{
    uint16_t crc = 0xFFFFu;
    for (uint32_t i = 0u; i < n; i++) {
        uint8_t t = (uint8_t)(b[i] ^ (uint8_t)(crc & 0xFFu));
        t = (uint8_t)(t ^ (uint8_t)(t << 4));
        crc = (uint16_t)((crc >> 8) ^ ((uint16_t)t << 8) ^ ((uint16_t)t << 3) ^ (uint16_t)(t >> 4));
    }
    return crc;
}

uint32_t s_kalan(const satir_t *s) { return (uint32_t)sizeof s->b - 16u - s->n; }

void s_cat(satir_t *s, const char *t)
{
    while (*t != '\0' && s->n < sizeof s->b - 16u) { s->b[s->n++] = *t++; }
}

void s_u64(satir_t *s, uint64_t v)
{
    char t[21];
    uint32_t i = 0;
    do { t[i++] = (char)('0' + (char)(v % 10u)); v /= 10u; } while (v != 0u);
    while (i > 0u && s->n < sizeof s->b - 16u) { s->b[s->n++] = t[--i]; }
}

static void s_hex(satir_t *s, uint32_t v, int hane)
{
    static const char h[] = "0123456789ABCDEF";
    for (int k = (hane - 1) * 4; k >= 0; k -= 4) {
        if (s->n < sizeof s->b - 3u) { s->b[s->n++] = h[(v >> (uint32_t)k) & 0xFu]; }
    }
}

void s_kv(satir_t *s, const char *k, uint64_t v) { s_cat(s, " "); s_cat(s, k); s_cat(s, "="); s_u64(s, v); }
void s_kx(satir_t *s, const char *k, uint32_t v) { s_cat(s, " "); s_cat(s, k); s_cat(s, "=0x"); s_hex(s, v, 8); }
void s_ks(satir_t *s, const char *k, const char *v) { s_cat(s, " "); s_cat(s, k); s_cat(s, "="); s_cat(s, v); }

void s_bas(satir_t *s, const char *tip)
{
    s->n = 0u;
    s_cat(s, tip);
    s_cat(s, " ");
    s_u64(s, satir_sira++);
}

static void uart_bayt(uint8_t b)
{
    while ((USART1->ISR & USART_ISR_TXE_TXFNF) == 0u) { }
    USART1->TDR = b;
}

void s_gonder(satir_t *s)
{
    const uint16_t c = crc16((const uint8_t *)s->b, s->n);
    s->b[s->n++] = ' ';
    s->b[s->n++] = '*';
    s_hex(s, c, 4);
    s->b[s->n++] = '\r';
    s->b[s->n++] = '\n';
    for (uint32_t i = 0u; i < s->n; i++) { uart_bayt((uint8_t)s->b[i]); }
    while ((USART1->ISR & USART_ISR_TC) == 0u) { }
    s->n = 0u;
}
