/*
 * W0-MP4a ölçüm prototipi: STM32U5A9J-DK'da 400 Hz döngü yükü, yer tutucu çekirdek adımıyla (ön tur).
 * Atılabilir prototip (09 §2.1); ürün koduna taşınmaz. Platform kodu gnu17.
 *
 *  - Saat: MP1'in düzeltilmiş dizisi (SMPS, Range 1, HSE 16 MHz + PLL1 M=4 N=80 R=2; AHB ÷2 -> ≥ 5 µs -> ÷1),
 *    160 MHz. ICACHE açık, flash önden getirme açık (HAL_Init).
 *  - Tetik: TIM2 (32 bit, PSC = 0, saat = PCLK1 = 160 MHz) MP4_HZ (varsayılan 400 Hz) güncelleme kesmesi,
 *    öncelik 1; SysTick 1 kHz öncelik 15 (HAL). Ana döngü her tetikte bir adım koşar.
 *  - Ölçüm: adım süresi DWT CYCCNT ile (kesmeler açık). Tetik gecikmesi = adımın başında, kesme/sayaç
 *    yarışı dışlanarak tutarlı okunan TIM2->CNT (güncelleme olayından bu yana geçen zamanlayıcı saati =
 *    çevrim). Ayrıca kesme girişinde okunan TIM2->CNT (kesme giriş gecikmesi) bildirilir.
 *  - Kaçırılan adım ("asim"): adım bittiğinde yeni bir güncelleme olayı olmuşsa (tetik sayacı değişmiş ya
 *    da UIF bayrağı bekliyorsa). Hiç adım koşulmadan geçen tetikler "atlanan". DWT'ye dayalı çapraz
 *    denetim: gecikme + süre ≥ periyot ("asim_dwt").
 *  - Seri çıktı: USART1 (ST-LINK VCP) 115200 8N1, TX FIFO açık; satırlar halka tampona yazılır, baytlar
 *    yalnız ölçülen bölgenin dışında (boşta yoklamayla ve her adımdan sonra en çok FIFO derinliği kadar)
 *    gönderilir; ölçülen bölgede UART işi yoktur. Kayıt sırasında durum satırı ("D") yalnız halkada yer
 *    varsa üretilir (yoksa üretilmez, sayılır); özet satırları hiç atılmaz (yer açılana dek beklenir;
 *    bu bekleme yalnız kayıt dışındaki geçiş adımlarında olur). Böylece satır sırası hiç boşluk içermez.
 *  - Satır biçimi: "<tip> <sıra> alan=değer ... *<CRC16 hex>"; sıra her satırda bir artar (PC kaybı denetler).
 *  - Fazlar: ısınma, sonra MP4_FAZLAR maskesindeki fazlar sırayla bir kez (A olağan, B en kötü, C 02 §2.2
 *    bütçe tanımı); her faz FAZ_GECIS kayıt dışı geçiş adımıyla başlar, ardından MP4_FAZ_ADIM kayıtlı adım.
 *    Son fazdan sonra "T" satırı basılır ve adım koşturma durur (PC kaydı bu satırla biter).
 *  - Histogramlar ham olarak (sıfır olmayan kutular) gönderilir; yüzdelikleri PC hesaplar.
 */
#include "stm32u5xx_hal.h"
#include "yt.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#ifndef MP4_DERLEME
#define MP4_DERLEME "?"
#endif
#ifndef MP4_KIMLIK
#define MP4_KIMLIK "yok"
#endif
#ifndef MP4_HZ
#define MP4_HZ          400u
#endif
#ifndef MP4_FAZLAR
#define MP4_FAZLAR      7u           /* bit 0 A, bit 1 B, bit 2 C */
#endif
#ifndef MP4_FAZ_ADIM
#define MP4_FAZ_ADIM    241000u      /* 400 Hz'te 602,5 s ≥ 10 dk (≥ 240 000 adım) */
#endif

#define HEDEF_HZ        160000000u
#define PERIYOT_CYC     (HEDEF_HZ / MP4_HZ)
#define FAZ_GECIS       MP4_HZ       /* 1 s'lik tetik sayısı kadar adım */
#define ISINMA          (2u * MP4_HZ)
#define D_ARALIK        MP4_HZ       /* durum satırı: bu kadar adımda bir */
#define INCE_KUTU_CYC   16u          /* 0,1 µs */
#define INCE_KUTU_N     200000u      /* 20 ms'ye kadar */
#define LAT_INCE_N      4096u        /* 1 çevrim kutu, 25,6 µs'ye kadar */
#define LAT_KABA_CYC    16u          /* 0,1 µs kutu, bir periyot boyunca */
#define LAT_KABA_N      ((PERIYOT_CYC + LAT_KABA_CYC - 1u) / LAT_KABA_CYC)
#define YIGIN_DESEN     0xA5C3A5C3u
#define YIGIN_KAYIT_PAY 32768u       /* kayıt öncesi boyanan bölge (main çerçevesinin altında) */
#define HIST_SATIR      16u          /* satır başına kutu */

#if (MP4_FAZLAR & 7u) == 0u
#error "MP4_FAZLAR en az bir faz içermeli"
#endif
#if (HEDEF_HZ % MP4_HZ) != 0u
#error "MP4_HZ 160 MHz'i tam bölmeli"
#endif

/* ---------------- durum ---------------- */
static UART_HandleTypeDef uart;
static yt_t ys;
static uint8_t gelen[YT_GELEN_BOY];
static uint8_t giden[YT_GIDEN_BOY];

static volatile uint32_t tik;          /* TIM2 güncelleme sayısı */
static volatile uint32_t tik_cnt;      /* son güncellemenin kesme girişinde okunan TIM2->CNT */
volatile uint32_t mp4_saat_adim;
volatile uint32_t mp4_saat_hal_rc;

typedef struct {
    uint32_t n, min, max, max_adim;
    uint64_t top;
    uint32_t tasan;                    /* histogram aralığı (20 ms) üstü */
    uint32_t asim, asim_dwt, atlanan;
    uint64_t duvar_cyc;                /* ilk kayıtlı adımın başından son kayıtlı adımın sonuna (çevrim) */
    uint32_t tik_ilk, tik_son;         /* ilk ve son kayıtlı adımın tetik sırası */
    uint32_t c0_onceki;
    uint32_t lat_min, lat_max, lat_tasan;
    uint64_t lat_top;
    uint32_t lat_temiz_n, lat_temiz_max; /* önceki adım aşmadıysa ve tetik atlanmadıysa */
    uint32_t isr_min, isr_max;         /* kesme girişindeki TIM2->CNT */
    uint64_t p_top[YT_P_SAYI];
    uint32_t p_max[YT_P_SAYI];
    uint32_t saglama;                  /* çıktıların FNV-1a'sı */
} faz_ist_t;

static faz_ist_t ist;
static uint32_t ince[INCE_KUTU_N];
static uint32_t lat_ince[LAT_INCE_N];
static uint32_t lat_kaba[LAT_KABA_N];
static uint32_t d_atlanan;             /* halkada yer olmadığı için üretilmeyen durum satırı */

/* ---------------- DWT ---------------- */
uint32_t yt_cevrim(void) { return DWT->CYCCNT; }

static void dwt_baslat(void)
{
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0u;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}

/* ---------------- UART halka tamponu (yoklamalı gönderim) ---------------- */
#define HALKA 16384u
static uint8_t halka[HALKA];
static uint32_t h_bas, h_son;          /* h_son: yazma, h_bas: okuma */
static uint32_t satir_sira;

/* En çok `azami` bayt; TX FIFO doluysa hemen döner. Yalnız ölçülen bölgenin dışında çağrılır. */
static void uart_pompa(uint32_t azami)
{
    while (azami-- > 0u && h_bas != h_son && (USART1->ISR & USART_ISR_TXE_TXFNF) != 0u) {
        USART1->TDR = halka[h_bas];
        h_bas = (h_bas + 1u) % HALKA;
    }
}

static uint32_t halka_bos(void) { return (h_bas + HALKA - h_son - 1u) % HALKA; }

typedef struct { char b[400]; uint32_t n; } satir_t;

static void s_cat(satir_t *s, const char *t)
{
    while (*t != '\0' && s->n < sizeof s->b - 12u) { s->b[s->n++] = *t++; }
}
static void s_u64(satir_t *s, uint64_t v)
{
    char t[21]; uint32_t i = 0;
    do { t[i++] = (char)('0' + (char)(v % 10u)); v /= 10u; } while (v != 0u);
    while (i > 0u && s->n < sizeof s->b - 12u) { s->b[s->n++] = t[--i]; }
}
static void s_hex(satir_t *s, uint32_t v, int hane)
{
    static const char h[] = "0123456789ABCDEF";
    for (int k = (hane - 1) * 4; k >= 0; k -= 4) {
        if (s->n < sizeof s->b - 3u) { s->b[s->n++] = h[(v >> (uint32_t)k) & 0xFu]; }
    }
}
static void s_kv(satir_t *s, const char *k, uint64_t v) { s_cat(s, " "); s_cat(s, k); s_cat(s, "="); s_u64(s, v); }
static void s_kx(satir_t *s, const char *k, uint32_t v) { s_cat(s, " "); s_cat(s, k); s_cat(s, "=0x"); s_hex(s, v, 8); }
static void s_ks(satir_t *s, const char *k, const char *v) { s_cat(s, " "); s_cat(s, k); s_cat(s, "="); s_cat(s, v); }

static void s_bas(satir_t *s, const char *tip)
{
    s->n = 0u;
    s_cat(s, tip);
    s_cat(s, " ");
    s_u64(s, satir_sira++);
}

/* Satırı " *CRC" ile bitirip halkaya koyar. Yer yoksa yer açılana dek pompalar (satır atılmaz).
   Kayıt sırasında yalnız önceden yer denetimi yapılmış durum satırı için çağrılır; bekleme olmaz. */
static void s_gonder(satir_t *s)
{
    const uint16_t c = yt_crc16((const uint8_t *)s->b, s->n, 0xFFFFu);
    s->b[s->n++] = ' ';
    s->b[s->n++] = '*';
    s_hex(s, c, 4);
    s->b[s->n++] = '\r';
    s->b[s->n++] = '\n';
    while (halka_bos() < s->n) { uart_pompa(8u); }
    for (uint32_t i = 0u; i < s->n; i++) { halka[h_son] = (uint8_t)s->b[i]; h_son = (h_son + 1u) % HALKA; }
    s->n = 0u;
}

/* Halka boşalana dek bekler (yalnız açılışta ve faz sonu dökümünde; ölçülen bölgede değil). */
static void halka_bosalt(void)
{
    while (h_bas != h_son) { uart_pompa(8u); }
    while ((USART1->ISR & USART_ISR_TC) == 0u) { }
}

/* ---------------- saat (MP1'in düzeltilmiş dizisi) ---------------- */
static uint32_t saat_hata(uint32_t adim, HAL_StatusTypeDef rc)
{
    mp4_saat_adim = adim;
    mp4_saat_hal_rc = (uint32_t)rc;
    return adim;
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

static uint64_t saat_hz(void)
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

/* ---------------- çevre birimleri ---------------- */
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
    /* TX FIFO (8 bayt): her adımdan sonra birkaç bayt birden bırakılabilsin (ağır yükte boşta zaman yok). */
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

void TIM2_IRQHandler(void)
{
    const uint32_t c = TIM2->CNT;      /* kesme giriş gecikmesi (güncelleme olayından bu yana) */
    if ((TIM2->SR & TIM_SR_UIF) != 0u) {
        TIM2->SR = ~TIM_SR_UIF;
        tik_cnt = c;
        tik++;
    }
}

static void tim2_baslat(void)
{
    __HAL_RCC_TIM2_CLK_ENABLE();
    TIM2->CR1 = 0u;
    TIM2->PSC = 0u;
    TIM2->ARR = PERIYOT_CYC - 1u;
    TIM2->EGR = TIM_EGR_UG;            /* PSC/ARR yüklensin */
    TIM2->SR = 0u;
    TIM2->DIER = TIM_DIER_UIE;
    HAL_NVIC_SetPriority(TIM2_IRQn, 1u, 0u);
    HAL_NVIC_EnableIRQ(TIM2_IRQn);
    TIM2->CR1 = TIM_CR1_CEN;
}

/* Tetik sırası ile TIM2->CNT'yi tutarlı okur: okuma sırasında güncelleme olduysa ya da kesme henüz
   işlenmediyse (UIF bekliyor) yeniden dener. Böylece gecikme her zaman `t`'nin güncellemesine göredir. */
static inline void tetik_oku(uint32_t *t, uint32_t *cnt, uint32_t *icnt)
{
    for (;;) {
        const uint32_t t1 = tik;
        const uint32_t ic = tik_cnt;
        const uint32_t c = TIM2->CNT;
        const uint32_t sr = TIM2->SR;
        const uint32_t t2 = tik;
        if (t1 == t2 && (sr & TIM_SR_UIF) == 0u) { *t = t1; *cnt = c; *icnt = ic; return; }
    }
}

/* ---------------- yığın boyama ---------------- */
extern uint32_t _ebss;
extern uint32_t _estack;
#define YIGIN_TABAN ((uint32_t *)((uint32_t)&_ebss + 256u))   /* bss sonu + 256 B */

static uint32_t sp_oku(void)
{
    uint32_t sp;
    __asm volatile("mov %0, sp" : "=r"(sp));
    return sp;
}

/* Tabandan (bss sonu + 256 B) bu işlevin yığın işaretçisinin 512 B altına kadar boyar (açılışta). */
static void __attribute__((noinline)) yigin_boya(void)
{
    const uint32_t sp = sp_oku();
    for (uint32_t *p = YIGIN_TABAN; (uint32_t)p < sp - 512u; p++) { *p = YIGIN_DESEN; }
}

/* Açılıştan bu yana en derin kullanım (main + bütün çağrılar + kesmeler): _estack'e kadar bayt. */
static uint32_t yigin_tepe(void)
{
    const uint32_t *p = YIGIN_TABAN;
    while ((uint32_t)p < (uint32_t)&_estack && *p == YIGIN_DESEN) { p++; }
    return (uint32_t)&_estack - (uint32_t)p;
}

/* Kayıt öncesi: main döngüsünün çerçevesinin altında YIGIN_KAYIT_PAY baytlık bölgeyi yeniden boyar. */
static uint32_t yk_alt, yk_ust;        /* boyanan bölge [alt, ust) */
static void __attribute__((noinline)) yigin_kayit_boya(void)
{
    const uint32_t sp = sp_oku();
    yk_ust = (sp - 256u) & ~3u;
    yk_alt = yk_ust - YIGIN_KAYIT_PAY;
    if (yk_alt < (uint32_t)YIGIN_TABAN) { yk_alt = (uint32_t)YIGIN_TABAN; }
    for (uint32_t *p = (uint32_t *)yk_alt; (uint32_t)p < yk_ust; p++) { *p = YIGIN_DESEN; }
}

/* Kayıt sonunda: boyanan bölgedeki en düşük bozulmuş adres; `tasti` = bölgenin tabanı da bozulmuş. */
static uint32_t yigin_kayit_olc(uint32_t sp_main, uint32_t *tasti)
{
    const uint32_t *p = (const uint32_t *)yk_alt;
    while ((uint32_t)p < yk_ust && *p == YIGIN_DESEN) { p++; }
    *tasti = ((uint32_t)p == yk_alt) ? 1u : 0u;
    return sp_main - (uint32_t)p;
}

/* ---------------- istatistik ---------------- */
static void ist_sifirla(void)
{
    memset(&ist, 0, sizeof ist);
    ist.min = UINT32_MAX;
    ist.lat_min = UINT32_MAX;
    ist.isr_min = UINT32_MAX;
    ist.saglama = 2166136261u;
    memset(ince, 0, sizeof ince);
    memset(lat_ince, 0, sizeof lat_ince);
    memset(lat_kaba, 0, sizeof lat_kaba);
}

static const char *const FAZ_AD[YT_YUK_SAYI] = { "A", "B", "C" };

/* Sıfır olmayan kutuları "kutu:sayı" olarak HIST_SATIR'lık satırlarla basar; satır sayısını döndürür. */
static uint32_t hist_bas(const char *tip, uint32_t faz, const uint32_t *h, uint32_t kutu_n)
{
    satir_t s;
    uint32_t k = 0u, satir = 0u;
    for (uint32_t i = 0u; i < kutu_n; i++) {
        if (h[i] == 0u) { continue; }
        if (k == 0u) { s_bas(&s, tip); s_ks(&s, "faz", FAZ_AD[faz]); }
        s_cat(&s, " "); s_u64(&s, i); s_cat(&s, ":"); s_u64(&s, h[i]);
        if (++k == HIST_SATIR) { s_gonder(&s); satir++; k = 0u; }
    }
    if (k != 0u) { s_gonder(&s); satir++; }
    return satir;
}

static void faz_rapor(uint32_t faz, uint32_t yigin_kayit, uint32_t yigin_tasti)
{
    satir_t s;
    const uint32_t n = ist.n;
    s_bas(&s, "F");
    s_ks(&s, "faz", FAZ_AD[faz]);
    s_ks(&s, "derleme", MP4_DERLEME);
    s_kv(&s, "n", n);
    s_kv(&s, "min", ist.min);
    s_kv(&s, "max", ist.max);
    s_kv(&s, "max_adim", ist.max_adim);
    s_kv(&s, "top", ist.top);
    s_kv(&s, "tasan", ist.tasan);
    s_kv(&s, "asim", ist.asim);
    s_kv(&s, "asim_dwt", ist.asim_dwt);
    s_kv(&s, "atlanan", ist.atlanan);
    s_kv(&s, "duvar_cyc", ist.duvar_cyc);
    s_kv(&s, "tik_aralik", (uint64_t)(ist.tik_son - ist.tik_ilk) + 1u);
    s_kx(&s, "saglama", ist.saglama);
    s_gonder(&s);

    s_bas(&s, "G");
    s_ks(&s, "faz", FAZ_AD[faz]);
    s_kv(&s, "lat_min", ist.lat_min);
    s_kv(&s, "lat_max", ist.lat_max);
    s_kv(&s, "lat_top", ist.lat_top);
    s_kv(&s, "lat_tasan", ist.lat_tasan);
    s_kv(&s, "lat_temiz_n", ist.lat_temiz_n);
    s_kv(&s, "lat_temiz_max", ist.lat_temiz_max);
    s_kv(&s, "isr_min", ist.isr_min);
    s_kv(&s, "isr_max", ist.isr_max);
    s_gonder(&s);

    s_bas(&s, "Q");
    s_ks(&s, "faz", FAZ_AD[faz]);
    for (int p = 0; p < YT_P_SAYI; p++) {
        s_cat(&s, " p"); s_u64(&s, (uint64_t)p); s_cat(&s, "=");
        s_u64(&s, ist.p_top[p]); s_cat(&s, "/"); s_u64(&s, ist.p_max[p]);
    }
    s_gonder(&s);

    s_bas(&s, "S");
    s_ks(&s, "faz", FAZ_AD[faz]);
    s_kv(&s, "kov", ys.e.kov_say);
    s_kv(&s, "skaler", ys.e.skaler_say);
    s_kv(&s, "sifirlama", ys.e.sifirlama_say);
    s_kv(&s, "kapi_gecti", ys.e.kapi_gecti);
    s_kv(&s, "kapi_red", ys.e.kapi_red);
    s_kv(&s, "gec", ys.e.gec_ornek);
    s_kv(&s, "ldl_hata", ys.e.ldl_hata);
    s_kv(&s, "yeniden_hiza", ys.e.yeniden_hiza);
    s_kv(&s, "crc_hata", ys.crc_hata);
    s_kv(&s, "doyma", ys.a.doyma_say);
    s_kv(&s, "tasma", ys.e.sf[0].tasma + ys.e.sf[1].tasma + ys.e.sf[2].tasma + ys.e.sf[3].tasma);
    s_kv(&s, "yigin_kayit", yigin_kayit);
    s_kv(&s, "yigin_kayit_tasti", yigin_tasti);
    s_kv(&s, "yigin_tepe", yigin_tepe());
    s_kv(&s, "d_atlanan", d_atlanan);
    s_gonder(&s);

    const uint32_t h = hist_bas("H", faz, ince, INCE_KUTU_N);
    const uint32_t l = hist_bas("L", faz, lat_ince, LAT_INCE_N);
    const uint32_t m = hist_bas("M", faz, lat_kaba, LAT_KABA_N);
    s_bas(&s, "E");
    s_ks(&s, "faz", FAZ_AD[faz]);
    s_kv(&s, "h", h);
    s_kv(&s, "l", l);
    s_kv(&s, "m", m);
    s_gonder(&s);
    halka_bosalt();
}

static uint32_t sonraki_faz(uint32_t faz)
{
    for (uint32_t f = faz + 1u; f < (uint32_t)YT_YUK_SAYI; f++) {
        if ((MP4_FAZLAR & (1u << f)) != 0u) { return f; }
    }
    return (uint32_t)YT_YUK_SAYI;
}

/* ---------------- ana ---------------- */
int main(void)
{
    const uint32_t csr = RCC->CSR;
    RCC->CSR |= RCC_CSR_RMVF;
    HAL_Init();
    __HAL_RCC_PWR_CLK_ENABLE();
    HAL_PWREx_DisableUCPDDeadBattery();
    uint32_t adim = saat_kur();
    if (adim == 0u && saat_hz() != HEDEF_HZ) { adim = saat_hata(6u, HAL_OK); }
    (void)HAL_ICACHE_Enable();
    led_baslat();
    const uint32_t uart_hata = uart_baslat();
    dwt_baslat();
    yigin_boya();

    satir_t s;
    s.n = 0u;
    s_gonder(&s);   /* boş satır */
    s_bas(&s, "B");
    s_ks(&s, "derleme", MP4_DERLEME);
    s_ks(&s, "kimlik", MP4_KIMLIK);
    s_kv(&s, "hz", MP4_HZ);
    s_kv(&s, "periyot_cyc", PERIYOT_CYC);
    s_kv(&s, "fazlar", MP4_FAZLAR);
    s_kv(&s, "faz_adim", MP4_FAZ_ADIM);
    s_kv(&s, "gecis", FAZ_GECIS);
    s_kv(&s, "ince_kutu_cyc", INCE_KUTU_CYC);
    s_kv(&s, "ince_kutu_n", INCE_KUTU_N);
    s_kv(&s, "lat_ince_n", LAT_INCE_N);
    s_kv(&s, "lat_kaba_cyc", LAT_KABA_CYC);
    s_kv(&s, "lat_kaba_n", LAT_KABA_N);
    s_gonder(&s);
    s_bas(&s, "B2");
    s_kx(&s, "rcc_csr", csr);
    s_kx(&s, "idcode", DBGMCU->IDCODE);
    s_kv(&s, "saat_adim", mp4_saat_adim);
    s_kv(&s, "hesap_hz", saat_hz());
    s_kv(&s, "hal_hz", HAL_RCC_GetSysClockFreq());
    s_kx(&s, "cfgr2", RCC->CFGR2);
    s_kx(&s, "cfgr3", RCC->CFGR3);
    s_kx(&s, "flash_acr", FLASH->ACR);
    s_kx(&s, "icache_cr", ICACHE->CR);
    s_kx(&s, "pwr_svmsr", PWR->SVMSR);
    s_kx(&s, "fpscr", __get_FPSCR());
    s_kx(&s, "cpacr", SCB->CPACR);
    s_kx(&s, "fpccr", FPU->FPCCR);
    s_kx(&s, "usart_cr1", USART1->CR1);
    s_kv(&s, "uart_hata", uart_hata);
    s_gonder(&s);
    halka_bosalt();
    if (adim != 0u) {
        HAL_GPIO_WritePin(GPIOE, GPIO_PIN_1, GPIO_PIN_SET);
        for (;;) {
            s_bas(&s, "X");
            s_ks(&s, "neden", "SAAT_HATASI");
            s_kv(&s, "adim", adim);
            s_kv(&s, "hal_rc", mp4_saat_hal_rc);
            s_gonder(&s);
            halka_bosalt();
            HAL_Delay(1000u);
        }
    }

    /* Damga çağrısının maliyeti (adım içindeki yt_parca payı için) */
    {
        const uint32_t c0 = DWT->CYCCNT;
        for (int i = 0; i < 100; i++) { yt_parca(&ys, YT_P_KIP); }
        const uint32_t c1 = DWT->CYCCNT;
        s_bas(&s, "K");
        s_kv(&s, "damga_100_cyc", c1 - c0);
        s_gonder(&s);
    }

    yt_baslat(&ys, YT_YUK_OLAGAN);
    tim2_baslat();
    /* TIM2 periyodunun DWT ile denetimi: iki tetik arası çevrim */
    {
        const uint32_t t0 = tik;
        while (tik == t0) { }
        const uint32_t c0 = DWT->CYCCNT;
        while (tik == t0 + 1u) { }
        const uint32_t c1 = DWT->CYCCNT;
        s_bas(&s, "K");
        s_kv(&s, "tim2_periyot_cyc", c1 - c0);
        s_kv(&s, "beklenen", PERIYOT_CYC);
        s_kv(&s, "sizeof_yt", sizeof ys);
        s_kv(&s, "yigin_tepe_acilis", yigin_tepe());
        s_gonder(&s);
        halka_bosalt();
    }

    enum { D_ISINMA, D_GECIS, D_KAYIT, D_BITTI } durum = D_ISINMA;
    uint32_t faz = 0u;
    while ((MP4_FAZLAR & (1u << faz)) == 0u) { faz++; }
    uint32_t n = 0u;               /* toplam adım */
    uint32_t sayac = 0u;           /* bulunulan durumdaki adım */
    uint32_t sonraki_d = D_ARALIK;
    uint32_t sp_main = sp_oku();
    bool onceki_asim = false;
    uint32_t son_tik = tik;        /* son koşulan adımın tetik sırası */
    yt_uret(&ys, YT_YUK_OLAGAN, 0u, gelen);
    ist_sifirla();

    for (;;) {
        if (durum == D_BITTI) {
            uart_pompa(8u);
            continue;
        }
        while (tik == son_tik) { uart_pompa(1u); }
        uint32_t t_bas, lat, isr_c;
        tetik_oku(&t_bas, &lat, &isr_c);
        /* ---- ölçülen bölge ---- */
        const uint32_t c0 = DWT->CYCCNT;
        yt_adim(&ys, gelen, giden);
        const uint32_t c1 = DWT->CYCCNT;
        /* ---- ölçülen bölge sonu ---- */
        const uint32_t sr1 = TIM2->SR;
        const uint32_t t_bit = tik;
        const uint32_t sure = c1 - c0;
        const bool asim = (t_bit != t_bas) || ((sr1 & TIM_SR_UIF) != 0u);
        const uint32_t atl = t_bas - son_tik - 1u;
        if (durum == D_KAYIT) {
            if (ist.n == 0u) { ist.tik_ilk = t_bas; } else { ist.duvar_cyc += (uint32_t)(c0 - ist.c0_onceki); }
            ist.c0_onceki = c0;
            ist.tik_son = t_bas;
            ist.n++;
            ist.top += sure;
            if (sure < ist.min) { ist.min = sure; }
            if (sure > ist.max) { ist.max = sure; ist.max_adim = sayac; }
            const uint32_t kb = sure / INCE_KUTU_CYC;
            if (kb < INCE_KUTU_N) { ince[kb]++; } else { ist.tasan++; }
            ist.lat_top += lat;
            if (lat < ist.lat_min) { ist.lat_min = lat; }
            if (lat > ist.lat_max) { ist.lat_max = lat; }
            if (lat < LAT_INCE_N) { lat_ince[lat]++; }
            else if (lat / LAT_KABA_CYC < LAT_KABA_N) { lat_kaba[lat / LAT_KABA_CYC]++; }
            else { ist.lat_tasan++; }
            if (!onceki_asim && atl == 0u) {
                ist.lat_temiz_n++;
                if (lat > ist.lat_temiz_max) { ist.lat_temiz_max = lat; }
            }
            if (isr_c < ist.isr_min) { ist.isr_min = isr_c; }
            if (isr_c > ist.isr_max) { ist.isr_max = isr_c; }
            if (asim) { ist.asim++; }
            if ((uint64_t)lat + sure >= PERIYOT_CYC) { ist.asim_dwt++; }
            ist.atlanan += atl;
            for (int p = 0; p < YT_P_SAYI; p++) {
                ist.p_top[p] += ys.parca_cyc[p];
                if (ys.parca_cyc[p] > ist.p_max[p]) { ist.p_max[p] = ys.parca_cyc[p]; }
            }
            for (uint32_t i = 0u; i < YT_GIDEN_BOY; i++) { ist.saglama = (ist.saglama ^ giden[i]) * 16777619u; }
        }
        onceki_asim = asim;
        son_tik = t_bas;
        n++;
        sayac++;

        /* durum yönetimi (kayıt dışı işler yalnız geçişlerde) */
        if (durum == D_ISINMA && sayac >= ISINMA) {
            durum = D_GECIS; sayac = 0u;
            yt_yuk_ayarla(&ys, (yt_yuk_t)faz);
            ist_sifirla();
            sp_main = sp_oku();
            yigin_kayit_boya();
        } else if (durum == D_GECIS && sayac >= FAZ_GECIS) {
            durum = D_KAYIT; sayac = 0u;
        } else if (durum == D_KAYIT && sayac >= MP4_FAZ_ADIM) {
            uint32_t tasti = 0u;
            const uint32_t yk = yigin_kayit_olc(sp_main, &tasti);
            HAL_GPIO_TogglePin(GPIOE, GPIO_PIN_0);
            faz_rapor(faz, yk, tasti);
            faz = sonraki_faz(faz);
            if (faz >= (uint32_t)YT_YUK_SAYI) {
                s_bas(&s, "T");
                s_kv(&s, "adim", n);
                s_kv(&s, "fazlar", MP4_FAZLAR);
                s_kv(&s, "d_atlanan", d_atlanan);
                s_gonder(&s);
                halka_bosalt();
                durum = D_BITTI;
                continue;
            }
            durum = D_GECIS; sayac = 0u;
            yt_yuk_ayarla(&ys, (yt_yuk_t)faz);
            ist_sifirla();
            sp_main = sp_oku();
            yigin_kayit_boya();
            son_tik = tik;               /* döküm sırasında geçen tetikler geçişte; kayıt dışı */
        }

        /* durum satırı: D_ARALIK adımda bir; halkada yer yoksa üretilmez (sıra boşluğu oluşmaz) */
        if (n >= sonraki_d) {
            sonraki_d += D_ARALIK;
            if (halka_bos() >= sizeof s.b + 8u) {
                s_bas(&s, "D");
                s_ks(&s, "faz", FAZ_AD[faz]);
                s_kv(&s, "durum", (uint32_t)durum);
                s_kv(&s, "n", ist.n);
                s_kv(&s, "max", ist.n ? ist.max : 0u);
                s_kv(&s, "son", sure);
                s_kv(&s, "asim", ist.asim);
                s_kv(&s, "atlanan", ist.atlanan);
                s_kv(&s, "lat_max", ist.n ? ist.lat_max : 0u);
                s_kv(&s, "d_atlanan", d_atlanan);
                s_gonder(&s);
            } else {
                d_atlanan++;
            }
        }
        /* bir sonraki adımın girdisi (ölçülen bölgenin dışında) */
        yt_uret(&ys, (yt_yuk_t)faz, n, gelen);
        /* Adım periyodu aşıyorsa boşta yoklama hiç dönmez; her adımdan sonra TX FIFO doldurulur. */
        uart_pompa(8u);
    }
}
