/*
 * W0-MP3a ölçüm prototipi: STM32U5A9J-DK, ST-LINK VCP (USART1) bağlantı ölçümü için kart platformu.
 * Atılabilir prototip (09 §2.1); ürün koduna taşınmaz. Platform kodu gnu17.
 *
 *  - Saat: MP1/MP4'ün düzeltilmiş dizisi (SMPS, Range 1, HSE 16 MHz + PLL1 M=4 N=80 R=2; AHB ÷2 -> ≥ 5 µs
 *    -> ÷1), 160 MHz. ICACHE açık.
 *  - USART1: PA9 TX / PA10 RX, AF7 (BSP stm32u5x9j_discovery.h COM1 tanımları). Çekirdek saati yazmaçtan
 *    (HAL_RCCEx_GetPeriphCLKFreq(RCC_PERIPHCLK_USART1); sıfırlama değeri PCLK2). 8N1, FIFO açık, akış denetimi
 *    yok. BRR seçimi ve etkin baud ortak/mp3_cerceve.c mp3_baud_hesapla: etkin baud yazılan BRR'den geri
 *    çözülür (OVER8'de USARTDIV[0] yazılamaz); seçim ve hesaplanan baud KUR_YANIT'ta bildirilir.
 *    Hata kesmesi (EIE) ORE/FE/NE/PE sayar ve temizler; alım sürer (DDRE = 0, OVRDIS = 0).
 *  - Alım: GPDMA1 kanal 0, istek 24 (GPDMA1_REQUEST_USART1_RX), 16 KB halkaya dairesel. Dairesellik,
 *    kendine bağlanan tek düğümlü bağlı liste ile (HAL'in DMA_LINKEDLIST_CIRCULAR düzeni: düğümde CTR1, CTR2,
 *    CBR1, CSAR, CDAR, CLLR; CLLR güncelleme maskesi UT1|UT2|UB1|USA|UDA|ULL). İlk blok kanal yazmaçlarından
 *    yüklenir. Konum yalnız kararlı yazmaç görüntüsünden (BNDT, FIFOL, BNDT) mod N hesaplanır; HT/TC kesmeleri
 *    kullanılmaz (ortak/mp3_halka.h; düzeltme turu 2, sol Y1). Yalnız RAM'e yazılmış baytlar tüketilir; tur
 *    sayısı belirsizse ya da okunmamış baytın üzerine yazıldıysa bekleyen veri atılır ve hata sayılır.
 *  - Gönderim: GPDMA1 kanal 1, istek 25 (GPDMA1_REQUEST_USART1_TX), DREQ = 1; 16 KB halkadan bitişik
 *    parçalar tek blok olarak gönderilir; TC kesmesi sıradaki parçayı başlatır.
 *  - Zaman: TIM2 (32 bit) PSC = 159 ile 1 MHz serbest sayaç.
 */
#include "stm32u5xx_hal.h"
#include "mp3_halka.h"
#include "mp3_kart.h"

#include <stdint.h>
#include <string.h>

#ifndef MP3_KIMLIK
#define MP3_KIMLIK "yok"
#endif

#define HEDEF_HZ 160000000u
#define RX_N     16384u
#define TX_N     16384u

volatile uint32_t mp3_saat_adim;
volatile uint32_t mp3_saat_hal_rc;
static uint32_t acilis_csr;

/* ---------------- saat (MP1/MP4 dizisi) ---------------- */
static void dwt_baslat(void)
{
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0u;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}

static uint32_t saat_hata(uint32_t adim, HAL_StatusTypeDef rc)
{
    mp3_saat_adim = adim;
    mp3_saat_hal_rc = (uint32_t)rc;
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

static uint32_t saat_hz(void)
{
    const uint32_t cfgr1 = RCC->CFGR1, pc = RCC->PLL1CFGR, pd = RCC->PLL1DIVR;
    const uint32_t sws = (cfgr1 & RCC_CFGR1_SWS_Msk) >> RCC_CFGR1_SWS_Pos;
    const uint32_t src = (pc & RCC_PLL1CFGR_PLL1SRC_Msk) >> RCC_PLL1CFGR_PLL1SRC_Pos;
    if (sws != 3u || src != 3u) { return 0u; }
    const uint32_t m = ((pc & RCC_PLL1CFGR_PLL1M_Msk) >> RCC_PLL1CFGR_PLL1M_Pos) + 1u;
    const uint32_t n = ((pd & RCC_PLL1DIVR_PLL1N_Msk) >> RCC_PLL1DIVR_PLL1N_Pos) + 1u;
    const uint32_t r = ((pd & RCC_PLL1DIVR_PLL1R_Msk) >> RCC_PLL1DIVR_PLL1R_Pos) + 1u;
    return (uint32_t)((uint64_t)HSE_VALUE * n / ((uint64_t)m * r));
}

/* ---------------- zaman ---------------- */
static void tim2_baslat(void)
{
    __HAL_RCC_TIM2_CLK_ENABLE();
    TIM2->CR1 = 0u;
    TIM2->PSC = (HEDEF_HZ / 1000000u) - 1u;
    TIM2->ARR = 0xFFFFFFFFu;
    TIM2->EGR = TIM_EGR_UG;
    TIM2->SR = 0u;
    TIM2->CR1 = TIM_CR1_CEN;
}

static uint32_t zaman_us(void) { return TIM2->CNT; }

/* ---------------- kesme sayaçları ---------------- */
static volatile uint32_t s_ore, s_fe, s_ne, s_pe, s_dma_rx, s_dma_tx;
static uint32_t s_tasma, s_tasma_bayt, s_halka_azami;

static uint32_t kilitle(void)
{
    const uint32_t m = __get_PRIMASK();
    __disable_irq();
    return m;
}
static void birak(uint32_t m) { __set_PRIMASK(m); }

/* ---------------- USART1 ---------------- */
static uint32_t usart_hz;

static int baud_hesapla(uint32_t baud, mp3_baud_t *b) { return mp3_baud_hesapla(usart_hz, baud, b); }

static uint32_t uart_bekle_hata;
static volatile uint32_t rx_azami_bayt_s = 1u;   /* hatta gelebilecek en çok bayt/s (konum belirsizlik koruması) */

static void baud_uygula(const mp3_baud_t *b)
{
    const uint32_t cr1 = USART_CR1_FIFOEN | USART_CR1_TE | USART_CR1_RE | (b->over8 ? USART_CR1_OVER8 : 0u);
    USART1->CR1 = cr1;                 /* UE = 0: OVER8/FIFOEN/BRR yalnız bu durumda yazılır */
    USART1->BRR = b->brr;
    USART1->ICR = USART_ICR_ORECF | USART_ICR_FECF | USART_ICR_NECF | USART_ICR_PECF;
    USART1->CR1 = cr1 | USART_CR1_UE;
    /* BRR'den hesaplanan hızın %10 fazlası (ST-LINK tarafının bilinmeyen hız hatasına pay); 8N1: 10 bit/bayt */
    rx_azami_bayt_s = (uint32_t)(((uint64_t)b->hesap_baud * 11u) / 100u + 1u);
    const uint32_t t = zaman_us();
    while ((USART1->ISR & (USART_ISR_TEACK | USART_ISR_REACK)) != (USART_ISR_TEACK | USART_ISR_REACK)) {
        if ((uint32_t)(zaman_us() - t) > 10000u) { uart_bekle_hata++; break; }
    }
}

void USART1_IRQHandler(void)
{
    const uint32_t isr = USART1->ISR;
    uint32_t icr = 0u;
    if ((isr & USART_ISR_ORE) != 0u) { s_ore++; icr |= USART_ICR_ORECF; }
    if ((isr & USART_ISR_FE) != 0u) { s_fe++; icr |= USART_ICR_FECF; }
    if ((isr & USART_ISR_NE) != 0u) { s_ne++; icr |= USART_ICR_NECF; }
    if ((isr & USART_ISR_PE) != 0u) { s_pe++; icr |= USART_ICR_PECF; }
    USART1->ICR = icr;
}

static void usart_gpio(void)
{
    __HAL_RCC_GPIOA_CLK_ENABLE();
    GPIO_InitTypeDef g = {0};
    g.Pin = GPIO_PIN_9 | GPIO_PIN_10;
    g.Mode = GPIO_MODE_AF_PP;
    g.Pull = GPIO_PULLUP;
    g.Speed = GPIO_SPEED_FREQ_HIGH;    /* 12 Mbaud adayında 83 ns bit süresi; düşük hız kipi yetmeyebilir */
    g.Alternate = GPIO_AF7_USART1;
    HAL_GPIO_Init(GPIOA, &g);
}

/* ---------------- GPDMA ---------------- */
#define RX_CH GPDMA1_Channel0
#define TX_CH GPDMA1_Channel1
#define DMA_HATA_BAYRAK (DMA_CSR_DTEF | DMA_CSR_ULEF | DMA_CSR_USEF | DMA_CSR_TOF)
#define DMA_TUM_BAYRAK  (DMA_CFCR_TCF | DMA_CFCR_HTF | DMA_CFCR_DTEF | DMA_CFCR_ULEF | DMA_CFCR_USEF | \
                         DMA_CFCR_SUSPF | DMA_CFCR_TOF)
#define LLI_MASKE       (DMA_CLLR_UT1 | DMA_CLLR_UT2 | DMA_CLLR_UB1 | DMA_CLLR_USA | DMA_CLLR_UDA | DMA_CLLR_ULL)

static uint8_t rx_halka[RX_N] __attribute__((aligned(32)));
static uint8_t tx_halka[TX_N] __attribute__((aligned(32)));
static uint32_t rx_dugum[6] __attribute__((aligned(32)));
static mp3_halka_t rx_hk;              /* alım konum muhasebesi (ortak/mp3_halka.c) */
static uint64_t rx_okunan;             /* okuyucunun toplam baytı */
static volatile uint32_t tx_yazilan;   /* ana bağlam */
static volatile uint32_t tx_okunan;    /* kesme */
static volatile uint32_t tx_aktif;     /* uçuştaki blok boyu; 0 boşta */

static void rx_dma_baslat(void)
{
    const uint32_t ctr1 = DMA_CTR1_DINC;                       /* bayt genişliği, kaynak sabit, port 0 */
    const uint32_t ctr2 = (24u << DMA_CTR2_REQSEL_Pos);        /* USART1_RX, DREQ = 0, TCEM = blok */
    rx_dugum[0] = ctr1;
    rx_dugum[1] = ctr2;
    rx_dugum[2] = RX_N;
    rx_dugum[3] = (uint32_t)&USART1->RDR;
    rx_dugum[4] = (uint32_t)rx_halka;
    rx_dugum[5] = ((uint32_t)rx_dugum & DMA_CLLR_LA) | LLI_MASKE;
    RX_CH->CCR = 0u;
    RX_CH->CFCR = DMA_TUM_BAYRAK;
    RX_CH->CTR1 = ctr1;
    RX_CH->CTR2 = ctr2;
    RX_CH->CBR1 = RX_N;
    RX_CH->CSAR = (uint32_t)&USART1->RDR;
    RX_CH->CDAR = (uint32_t)rx_halka;
    RX_CH->CLBAR = (uint32_t)rx_dugum & DMA_CLBAR_LBA;
    RX_CH->CLLR = rx_dugum[5];
    /* HT/TC kesmeleri kullanılmaz (konum yalnız yazmaç görüntüsünden); yalnız hata kesmeleri */
    RX_CH->CCR = (3u << DMA_CCR_PRIO_Pos) | DMA_CCR_DTEIE | DMA_CCR_ULEIE | DMA_CCR_USEIE | DMA_CCR_TOIE;
    mp3_halka_baslat(&rx_hk, RX_N, DMA_CSR_FIFOL_Msk >> DMA_CSR_FIFOL_Pos);
    HAL_NVIC_SetPriority(GPDMA1_Channel0_IRQn, 0u, 0u);
    HAL_NVIC_EnableIRQ(GPDMA1_Channel0_IRQn);
    RX_CH->CCR |= DMA_CCR_EN;
}

void GPDMA1_Channel0_IRQHandler(void)
{
    const uint32_t sr = RX_CH->CSR;
    if ((sr & DMA_HATA_BAYRAK) != 0u) { s_dma_rx++; }
    RX_CH->CFCR = sr & DMA_TUM_BAYRAK;
}

/* Kararlı görüntü: BNDT, FIFOL, BNDT ve zaman PRIMASK altında (kesme araya girip zamanı bozmasın; DMA durmaz).
   En çok 4 deneme; hepsi kararsızsa bu turda veri tüketilmez. */
static int rx_goruntu(void)
{
    for (int d = 0; d < 4; d++) {
        const uint32_t m = kilitle();
        const uint32_t b1 = RX_CH->CBR1 & DMA_CBR1_BNDT_Msk;
        const uint32_t f = (RX_CH->CSR & DMA_CSR_FIFOL_Msk) >> DMA_CSR_FIFOL_Pos;
        const uint32_t b2 = RX_CH->CBR1 & DMA_CBR1_BNDT_Msk;
        const uint32_t t = zaman_us();
        birak(m);
        const int r = mp3_halka_guncelle(&rx_hk, b1, f, b2, t, rx_azami_bayt_s);
        if (r != MP3_HALKA_KARARSIZ) {
            __DMB();                   /* yazmaç okumaları halka okumasından önce tamamlansın */
            return r;
        }
    }
    return MP3_HALKA_KARARSIZ;
}

/* Belirsiz konum ya da üzerine yazma: bekleyen veri atılır, hata sayılır (koşu geçmez). */
static void rx_at(int belirsiz)
{
    if (belirsiz) { s_dma_rx++; } else { s_tasma++; }
    s_tasma_bayt += (uint32_t)(rx_hk.toplam - rx_okunan);
    rx_okunan = rx_hk.toplam;
}

static uint32_t rx_al(uint8_t *hedef, uint32_t azami)
{
    int r = rx_goruntu();
    if (r == MP3_HALKA_BELIRSIZ) { rx_at(1); return 0u; }
    if (r == MP3_HALKA_KARARSIZ) { return 0u; }
    const uint64_t bek = rx_hk.toplam - rx_okunan;
    if (bek > RX_N) { rx_at(0); return 0u; }       /* okunmamış bayt(lar)ın üzerine yazılmış */
    if (bek > s_halka_azami) { s_halka_azami = (uint32_t)bek; }
    const uint32_t k = (uint32_t)(bek < azami ? bek : azami);
    if (k == 0u) { return 0u; }
    const uint32_t bas = (uint32_t)(rx_okunan % RX_N);
    const uint32_t k1 = k < RX_N - bas ? k : RX_N - bas;
    memcpy(hedef, rx_halka + bas, k1);
    if (k > k1) { memcpy(hedef + k1, rx_halka, k - k1); }
    r = rx_goruntu();
    if (r == MP3_HALKA_BELIRSIZ) { rx_at(1); return 0u; }
    if (r == MP3_HALKA_KARARSIZ) { return 0u; }     /* doğrulanamadı: kopya atılır, sonra yeniden denenir */
    if (rx_hk.toplam - rx_okunan > RX_N) { rx_at(0); return 0u; }   /* kopya sırasında üzerine yazıldı */
    rx_okunan += k;
    return k;
}

/* Kesmeler kapalıyken ya da kesmeden çağrılır. */
static void tx_basla_kilitli(void)
{
    if (tx_aktif != 0u || tx_okunan == tx_yazilan) { return; }
    const uint32_t bas = tx_okunan % TX_N;
    uint32_t n = tx_yazilan - tx_okunan;
    if (n > TX_N - bas) { n = TX_N - bas; }
    TX_CH->CCR = 0u;
    TX_CH->CFCR = DMA_TUM_BAYRAK;
    TX_CH->CTR1 = DMA_CTR1_SINC;                               /* bayt genişliği, hedef sabit */
    TX_CH->CTR2 = (25u << DMA_CTR2_REQSEL_Pos) | DMA_CTR2_DREQ; /* USART1_TX, hedef isteği */
    TX_CH->CBR1 = n;
    TX_CH->CSAR = (uint32_t)&tx_halka[bas];
    TX_CH->CDAR = (uint32_t)&USART1->TDR;
    TX_CH->CLLR = 0u;
    tx_aktif = n;
    TX_CH->CCR = DMA_CCR_TCIE | DMA_CCR_DTEIE | DMA_CCR_ULEIE | DMA_CCR_USEIE | DMA_CCR_TOIE | DMA_CCR_EN;
}

void GPDMA1_Channel1_IRQHandler(void)
{
    const uint32_t sr = TX_CH->CSR;
    TX_CH->CFCR = sr & DMA_TUM_BAYRAK;
    if ((sr & DMA_HATA_BAYRAK) != 0u) {
        s_dma_tx++;
        TX_CH->CCR = 0u;
        tx_okunan += tx_aktif;     /* uçuştaki blok kayıp sayılır; kilitlenme olmaz, sayaç hatayı taşır */
        tx_aktif = 0u;
    } else if ((sr & DMA_CSR_TCF) != 0u) {
        tx_okunan += tx_aktif;
        tx_aktif = 0u;
    }
    tx_basla_kilitli();
}

static void tx_dma_baslat(void)
{
    TX_CH->CCR = 0u;
    TX_CH->CFCR = DMA_TUM_BAYRAK;
    HAL_NVIC_SetPriority(GPDMA1_Channel1_IRQn, 2u, 0u);
    HAL_NVIC_EnableIRQ(GPDMA1_Channel1_IRQn);
}

static uint32_t tx_bos(void) { return TX_N - (tx_yazilan - tx_okunan); }

static void tx_yaz(const uint8_t *p, uint32_t n)
{
    const uint32_t bas = tx_yazilan % TX_N;
    const uint32_t k1 = n < TX_N - bas ? n : TX_N - bas;
    memcpy(tx_halka + bas, p, k1);
    if (n > k1) { memcpy(tx_halka, p + k1, n - k1); }
    __DMB();
    const uint32_t m = kilitle();
    tx_yazilan += n;
    tx_basla_kilitli();
    birak(m);
}

static int tx_bosaldi(void)
{
    const uint32_t m = kilitle();
    const int bos = tx_okunan == tx_yazilan && tx_aktif == 0u;
    birak(m);
    return bos && (USART1->ISR & USART_ISR_TC) != 0u;
}

static void hw_oku(mp3_hw_t *h, int sifirla)
{
    const uint32_t m = kilitle();
    h->uart_ore = s_ore;
    h->uart_fe = s_fe;
    h->uart_ne = s_ne;
    h->uart_pe = s_pe;
    h->dma_rx_hata = s_dma_rx;
    h->dma_tx_hata = s_dma_tx;
    h->rx_tasma = s_tasma;
    h->rx_tasma_bayt = s_tasma_bayt;
    h->rx_halka_azami = s_halka_azami;
    h->rx_halka_boy = RX_N;
    if (sifirla) { s_ore = s_fe = s_ne = s_pe = s_dma_rx = s_dma_tx = s_tasma = s_tasma_bayt = s_halka_azami = 0u; }
    birak(m);
}

static void s_ekle(char *s, uint32_t kap, uint32_t *n, const char *t)
{
    while (*t != '\0' && *n + 1u < kap) { s[(*n)++] = *t++; }
    s[*n] = '\0';
}
static void s_sayi(char *s, uint32_t kap, uint32_t *n, uint32_t v)
{
    char d[11];
    int k = 0;
    do { d[k++] = (char)('0' + (char)(v % 10u)); v /= 10u; } while (v != 0u);
    while (k > 0 && *n + 1u < kap) { s[(*n)++] = d[--k]; }
    s[*n] = '\0';
}
static void s_hex(char *s, uint32_t kap, uint32_t *n, uint32_t v)
{
    static const char h[] = "0123456789ABCDEF";
    s_ekle(s, kap, n, "0x");
    for (int k = 28; k >= 0; k -= 4) { if (*n + 1u < kap) { s[(*n)++] = h[(v >> (uint32_t)k) & 0xFu]; } }
    s[*n] = '\0';
}

static void kimlik_ek(char *s, uint32_t kap)
{
    uint32_t n = 0u;
    s[0] = '\0';
    s_ekle(s, kap, &n, " saat_hz="); s_sayi(s, kap, &n, saat_hz());
    s_ekle(s, kap, &n, " saat_adim="); s_sayi(s, kap, &n, mp3_saat_adim);
    s_ekle(s, kap, &n, " usart_hz="); s_sayi(s, kap, &n, usart_hz);
    s_ekle(s, kap, &n, " ccipr1="); s_hex(s, kap, &n, RCC->CCIPR1);
    s_ekle(s, kap, &n, " rcc_csr="); s_hex(s, kap, &n, acilis_csr);
    s_ekle(s, kap, &n, " idcode="); s_hex(s, kap, &n, DBGMCU->IDCODE);
    /* KIMLIK yükü 256 B ile sınırlı: halka boyları burada değil OZET'te (rx_halka_boy) */
    s_ekle(s, kap, &n, " uart_bekle_hata="); s_sayi(s, kap, &n, uart_bekle_hata);
    s_ekle(s, kap, &n, " kararsiz="); s_sayi(s, kap, &n, rx_hk.kararsiz);
    s_ekle(s, kap, &n, " belirsiz="); s_sayi(s, kap, &n, rx_hk.belirsiz);
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

static const mp3_platform_t platform = {
    zaman_us, tx_bos, tx_yaz, tx_bosaldi, baud_hesapla, baud_uygula, hw_oku, kimlik_ek
};

int main(void)
{
    acilis_csr = RCC->CSR;
    RCC->CSR |= RCC_CSR_RMVF;
    HAL_Init();
    __HAL_RCC_PWR_CLK_ENABLE();
    HAL_PWREx_DisableUCPDDeadBattery();
    uint32_t adim = saat_kur();
    if (adim == 0u && saat_hz() != HEDEF_HZ) { adim = saat_hata(6u, HAL_OK); }
    (void)HAL_ICACHE_Enable();
    led_baslat();
    dwt_baslat();
    tim2_baslat();
    if (adim != 0u) {
        /* Saat kurulamadı: kırmızı LED; seri çıktı yok (ölçüm 160 MHz varsayar). Durum gdb ile mp3_saat_adim'den. */
        HAL_GPIO_WritePin(GPIOE, GPIO_PIN_1, GPIO_PIN_SET);
        for (;;) { }
    }

    __HAL_RCC_USART1_CLK_ENABLE();
    __HAL_RCC_GPDMA1_CLK_ENABLE();
    usart_hz = HAL_RCCEx_GetPeriphCLKFreq(RCC_PERIPHCLK_USART1);
    usart_gpio();
    USART1->CR1 = 0u;
    USART1->CR2 = 0u;
    USART1->CR3 = USART_CR3_DMAR | USART_CR3_DMAT | USART_CR3_EIE;
    USART1->PRESC = 0u;
    rx_dma_baslat();
    tx_dma_baslat();
    HAL_NVIC_SetPriority(USART1_IRQn, 1u, 0u);
    HAL_NVIC_EnableIRQ(USART1_IRQn);
    mp3_baud_t b;
    if (baud_hesapla(MP3_KONTROL_BAUD, &b) != 0) {
        HAL_GPIO_WritePin(GPIOE, GPIO_PIN_1, GPIO_PIN_SET);
        for (;;) { }
    }
    baud_uygula(&b);
    HAL_GPIO_WritePin(GPIOE, GPIO_PIN_0, GPIO_PIN_SET);

    mp3_kart_baslat(&platform, MP3_KIMLIK);
    static uint8_t parca[512];
    for (;;) {
        const uint32_t n = rx_al(parca, sizeof parca);
        if (n != 0u) { mp3_kart_besle(parca, n); }
        mp3_kart_adim();
    }
}
