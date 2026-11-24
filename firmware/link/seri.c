/*
 * seri.c — A1 bağlantı sürücüsü: USART1 + GPDMA1 (seri.h).
 *
 * USART ve DMA yazmaç düzeni MP3 prototipinden alındı (tools/olcum/mp3/fw/platform.c; 3 Mbaud
 * 600 s %80 yükte kartta kayıpsız). Bu sürümde farklı olan: alım konumu her alım olayında
 * kesmede alınır (karakter eşleşmesi, hat boşluğu, DMA yarım/tam; A1-K48) ve ana döngü
 * baytları olay sınırlarında bloklar hâlinde alır.
 *
 * Kesme öncelikleri (01 §2.4): USART1, GPDMA1 kanal 0 ve 1: 3 (aynı düzey, birbirini kesmez).
 * Ana bağlam alım halkasının paylaşılan durumuna BASEPRI = 3 altında dokunur.
 */
#include "seri.h"

#include <string.h>

#include "alim.h"
#include "bsp.h"
#include "stm32u5xx_hal.h"

#define RX_CH GPDMA1_Channel0
#define TX_CH GPDMA1_Channel1
#define DMA_ISTEK_USART1_RX 24u /* GPDMA1_REQUEST_USART1_RX */
#define DMA_ISTEK_USART1_TX 25u /* GPDMA1_REQUEST_USART1_TX */
#define DMA_HATA_BAYRAK (DMA_CSR_DTEF | DMA_CSR_ULEF | DMA_CSR_USEF | DMA_CSR_TOF)
#define DMA_TUM_BAYRAK                                                                            \
    (DMA_CFCR_TCF | DMA_CFCR_HTF | DMA_CFCR_DTEF | DMA_CFCR_ULEF | DMA_CFCR_USEF |              \
     DMA_CFCR_SUSPF | DMA_CFCR_TOF)
#define LLI_MASKE (DMA_CLLR_UT1 | DMA_CLLR_UT2 | DMA_CLLR_UB1 | DMA_CLLR_USA | DMA_CLLR_UDA | DMA_CLLR_ULL)
#define FIFOL_AZAMI (DMA_CSR_FIFOL_Msk >> DMA_CSR_FIFOL_Pos)
/* Hat boşken olay gelmemiş bayt için ana döngünün yoklama gecikmesi (µs). */
#define YOKLAMA_US 1000u
/* Hat boşken de konum görüntüsü bu aralıkla tazelenir (µs): görüntüler arası süreden türetilen
   "tur sayısı belirsiz" denetimi (alim.h) boşluktan sonraki ilk baytı yanlışlıkla belirsiz
   saymasın (kartta gözlendi: ≈ 49 ms'den uzun boşluktan sonraki ilk görüntü). */
#define TAZELEME_US 10000u

_Static_assert(DMA_ISTEK_USART1_RX == GPDMA1_REQUEST_USART1_RX, "GPDMA1 istek numarası");
_Static_assert(DMA_ISTEK_USART1_TX == GPDMA1_REQUEST_USART1_TX, "GPDMA1 istek numarası");

static uint8_t rx_halka[IDA_SERI_RX_N] __attribute__((aligned(32)));
static uint8_t tx_halka[IDA_SERI_TX_N] __attribute__((aligned(32)));
static uint32_t rx_dugum[6] __attribute__((aligned(32)));
static ida_alim_t alim;

static ida_seri_sayac_t sayac;
static volatile uint32_t ore_sayisi; /* kesme artırır */
static uint32_t ore_gorulen;         /* ana bağlam    */
/* DMA alım kanalı hata verip durdu (DTEF/ULEF/USEF: donanım kanalı kapatır); kesme mandallar,
   ana bağlam kanalı denetimli olarak yeniden başlatır. Yeniden başlatma tutmazsa mandal kalır
   ve sağlık denetimi IWDG'yi beslemez. */
static volatile uint32_t rx_durdu;
static bool baslatildi;

static volatile uint32_t tx_yazilan; /* ana bağlam    */
static volatile uint32_t tx_okunan;  /* kesme         */
static volatile uint32_t tx_aktif;   /* uçuştaki blok; 0 boşta */

/* ---- baud (MP3 mp3_baud_hesapla'nın sadeleştirilmiş hali) --------------------------------- */

static uint32_t brr_baud(uint32_t hz, uint32_t brr, uint32_t over8)
{
    if (over8) {
        const uint32_t d = (brr & 0xFFF0u) | ((brr & 0x0007u) << 1u);
        return d == 0u ? 0u : (uint32_t)(((uint64_t)hz * 2u + d / 2u) / d);
    }
    return brr == 0u ? 0u : (uint32_t)(((uint64_t)hz + brr / 2u) / brr);
}

static uint32_t mutlak_fark(uint32_t a, uint32_t b)
{
    return a > b ? a - b : b - a;
}

static int baud_sec(uint32_t hz, uint32_t baud)
{
    uint32_t aday_d[4], aday_o8[4];
    aday_d[0] = hz / baud;
    aday_d[1] = (uint32_t)(((uint64_t)hz + baud - 1u) / baud);
    aday_o8[0] = aday_o8[1] = 0u;
    const uint32_t d8 = (uint32_t)((2u * (uint64_t)hz) / baud);
    aday_d[2] = d8 & ~1u;
    aday_d[3] = aday_d[2] + 2u;
    aday_o8[2] = aday_o8[3] = 1u;
    int bulundu = 0;
    uint32_t en_hata = 0u;
    for (uint32_t i = 0u; i < 4u; i++) {
        const uint32_t d = aday_d[i];
        if (d < 16u || d > 0xFFFFu) {
            continue;
        }
        const uint32_t brr = aday_o8[i] ? ((d & 0xFFF0u) | ((d & 0x000Fu) >> 1u)) : d;
        const uint32_t h = brr_baud(hz, brr, aday_o8[i]);
        if (h == 0u) {
            continue;
        }
        const uint32_t e = mutlak_fark(h, baud);
        if (!bulundu || e < en_hata) {
            bulundu = 1;
            en_hata = e;
            sayac.over8 = aday_o8[i];
            sayac.brr = brr;
            sayac.baud_brr = h;
        }
    }
    return bulundu ? 0 : -1;
}

/* ---- alım olayları (kesme bağlamı, öncelik 3) --------------------------------------------- */

/* Kararlı bir görüntü (BNDT, FIFOL, BNDT) alır; en çok 4 deneme. Dönüş: 1 alındı. */
static int goruntu_oku(uint32_t *b1, uint32_t *f, uint32_t *b2)
{
    for (uint32_t d = 0u; d < 4u; d++) {
        *b1 = RX_CH->CBR1 & DMA_CBR1_BNDT_Msk;
        *f = (RX_CH->CSR & DMA_CSR_FIFOL_Msk) >> DMA_CSR_FIFOL_Pos;
        *b2 = RX_CH->CBR1 & DMA_CBR1_BNDT_Msk;
        if (*b1 == *b2) {
            __DMB(); /* yazmaç okumaları, olayın yayımlanmasından önce */
            return 1;
        }
        alim.kararsiz++;
    }
    return 0;
}

/* Kesme bağlamından ya da alım kesmeleri maskeliyken çağrılır. */
static void goruntu_al(uint8_t tur, uint64_t t_us)
{
    uint32_t b1 = 0u, f = 0u, b2 = 0u;
    if (goruntu_oku(&b1, &f, &b2)) {
        (void)ida_alim_goruntu(&alim, b1, f, b2, t_us, tur);
    }
}

void USART1_IRQHandler(void)
{
    const uint32_t isr = USART1->ISR;
    uint32_t icr = 0u;
    uint8_t tur = 0u;
    if ((isr & USART_ISR_ORE) != 0u) {
        ore_sayisi = ore_sayisi + 1u;
        icr |= USART_ICR_ORECF;
    }
    if ((isr & USART_ISR_FE) != 0u) {
        sayac.fe++;
        icr |= USART_ICR_FECF;
    }
    if ((isr & USART_ISR_NE) != 0u) {
        sayac.ne++;
        icr |= USART_ICR_NECF;
    }
    if ((isr & USART_ISR_PE) != 0u) {
        sayac.pe++;
        icr |= USART_ICR_PECF;
    }
    if ((isr & USART_ISR_CMF) != 0u) {
        icr |= USART_ICR_CMCF;
        tur |= IDA_ALIM_OLAY_ESLESME;
    }
    if ((isr & USART_ISR_IDLE) != 0u) {
        icr |= USART_ICR_IDLECF;
        tur |= IDA_ALIM_OLAY_BOSTA;
    }
    USART1->ICR = icr;
    if (tur != 0u && rx_durdu == 0u) {
        goruntu_al(tur, ida_bsp_zaman_us());
    }
}

void GPDMA1_Channel0_IRQHandler(void)
{
    const uint32_t sr = RX_CH->CSR;
    RX_CH->CFCR = sr & DMA_TUM_BAYRAK;
    if ((sr & DMA_HATA_BAYRAK) != 0u) {
        /* Donanım kanalı kapattı: alım durdu (mandal); ana döngü yeniden başlatır. */
        sayac.dma_rx_hata++;
        RX_CH->CCR = 0u;
        rx_durdu = 1u;
        return;
    }
    uint8_t tur = 0u;
    if ((sr & DMA_CSR_HTF) != 0u) {
        tur |= IDA_ALIM_OLAY_YARIM;
    }
    if ((sr & DMA_CSR_TCF) != 0u) {
        tur |= IDA_ALIM_OLAY_TAM;
    }
    if (tur != 0u && rx_durdu == 0u) {
        goruntu_al(tur, ida_bsp_zaman_us());
    }
}

/* ---- gönderim ------------------------------------------------------------------------------ */

/* Kesmeler (öncelik >= 3) maskeliyken ya da kesmeden çağrılır. */
static void tx_basla_kilitli(void)
{
    if (tx_aktif != 0u || tx_okunan == tx_yazilan) {
        return;
    }
    const uint32_t bas = tx_okunan % IDA_SERI_TX_N;
    uint32_t n = tx_yazilan - tx_okunan;
    if (n > IDA_SERI_TX_N - bas) {
        n = IDA_SERI_TX_N - bas;
    }
    TX_CH->CCR = 0u;
    TX_CH->CFCR = DMA_TUM_BAYRAK;
    TX_CH->CTR1 = DMA_CTR1_SINC;
    TX_CH->CTR2 = (DMA_ISTEK_USART1_TX << DMA_CTR2_REQSEL_Pos) | DMA_CTR2_DREQ;
    TX_CH->CBR1 = n;
    TX_CH->CSAR = (uint32_t)(uintptr_t)&tx_halka[bas];
    TX_CH->CDAR = (uint32_t)(uintptr_t)&USART1->TDR;
    TX_CH->CLLR = 0u;
    tx_aktif = n;
    TX_CH->CCR = DMA_CCR_TCIE | DMA_CCR_DTEIE | DMA_CCR_ULEIE | DMA_CCR_USEIE | DMA_CCR_TOIE |
                 DMA_CCR_EN;
}

void GPDMA1_Channel1_IRQHandler(void)
{
    const uint32_t sr = TX_CH->CSR;
    TX_CH->CFCR = sr & DMA_TUM_BAYRAK;
    if ((sr & DMA_HATA_BAYRAK) != 0u) {
        sayac.dma_tx_hata++;
        TX_CH->CCR = 0u;
        tx_okunan = tx_okunan + tx_aktif; /* uçuştaki blok kayıp; sayaç hatayı taşır */
        tx_aktif = 0u;
    } else if ((sr & DMA_CSR_TCF) != 0u) {
        tx_okunan = tx_okunan + tx_aktif;
        tx_aktif = 0u;
    }
    tx_basla_kilitli();
}

bool ida_seri_gonderim_bos(void)
{
    return tx_okunan == tx_yazilan && tx_aktif == 0u && (USART1->ISR & USART_ISR_TC) != 0u;
}

size_t ida_seri_yazma_yeri(void)
{
    return IDA_SERI_TX_N - (tx_yazilan - tx_okunan);
}

bool ida_seri_yaz(const uint8_t *veri, size_t n)
{
    if (n == 0u || n > ida_seri_yazma_yeri()) {
        sayac.tx_ret++;
        return false;
    }
    const uint32_t bas = tx_yazilan % IDA_SERI_TX_N;
    const size_t k1 = n < (size_t)(IDA_SERI_TX_N - bas) ? n : (size_t)(IDA_SERI_TX_N - bas);
    memcpy(tx_halka + bas, veri, k1);
    if (n > k1) {
        memcpy(tx_halka, veri + k1, n - k1);
    }
    __DMB();
    const uint32_t m = ida_bsp_maske_ac(IDA_BSP_ONCELIK_SERI);
    tx_yazilan = tx_yazilan + (uint32_t)n;
    const uint32_t dolu = tx_yazilan - tx_okunan;
    tx_basla_kilitli();
    ida_bsp_maske_birak(m);
    if (dolu > sayac.tx_doluluk_azami) {
        sayac.tx_doluluk_azami = dolu;
    }
    sayac.tx_bayt += (uint32_t)n;
    return true;
}

/* ---- alım (ana bağlam) --------------------------------------------------------------------- */

static void rx_dma_kanal_kur(void);

/* Durmuş alım kanalını yeniden başlatır (BASEPRI altında): bekleyenler kayıp sayılır, toplam
   bir sonraki halka katına yuvarlanır, kanal halkanın başından yeniden açılır. Kanal açılmazsa
   mandal kalır. */
static void rx_yeniden_baslat(void)
{
    RX_CH->CCR = 0u;
    ida_alim_yeniden_baslat(&alim, ida_bsp_zaman_us());
    rx_dma_kanal_kur();
    if ((RX_CH->CCR & DMA_CCR_EN) != 0u) {
        rx_durdu = 0u;
    }
    sayac.rx_yeniden++;
}

bool ida_seri_bekleyen(void)
{
    const uint32_t m = ida_bsp_maske_ac(IDA_BSP_ONCELIK_SERI);
    const bool b = ida_alim_bekleyen(&alim) != 0;
    ida_bsp_maske_birak(m);
    return b;
}

bool ida_seri_alim_durdu(void)
{
    return baslatildi && rx_durdu != 0u;
}

size_t ida_seri_oku(uint8_t *hedef, size_t azami, uint64_t *t_rx_us, bool *kaba, bool *tasma)
{
    *tasma = false;
    *kaba = false;
    size_t n = 0u;
    uint64_t atilan = 0u;
    uint8_t k = 0u;
    if (!baslatildi) {
        return 0u;
    }
    const uint32_t m = ida_bsp_maske_ac(IDA_BSP_ONCELIK_SERI);

    /* DMA alım hatası: denetimli yeniden başlatma (kanal kendiliğinden açılmaz). */
    if (rx_durdu != 0u) {
        rx_yeniden_baslat();
    }

    /* USART taşması: bayt kaybı (A1-K09); ayrıştırıcı yeniden eşlenir. */
    const uint32_t ore = ore_sayisi;
    if (ore != ore_gorulen) {
        sayac.ore += ore - ore_gorulen;
        ore_gorulen = ore;
        *tasma = true;
    }

    /* Olay gelmemiş bayt (hat boşluğu kesmesi kaçtıysa ya da veri FIFO'da kaldıysa): son
       görüntüden YOKLAMA_US sonra ana bağlam bir yoklama görüntüsü alır; bulunan baytlar önceki
       olayın damgasıyla kaba işaretlenir (A1-K48). Hat boşken de TAZELEME_US'de bir görüntü
       alınır (süre denetimi taze kalsın). */
    if (!ida_alim_bekleyen(&alim) && alim.hazir && rx_durdu == 0u) {
        const uint64_t simdi = ida_bsp_zaman_us();
        const uint64_t gecen = simdi - alim.t_us;
        if (gecen >= YOKLAMA_US) {
            uint32_t b1 = 0u, f = 0u, b2 = 0u;
            if (goruntu_oku(&b1, &f, &b2)) {
                const uint32_t p = (uint32_t)((2u * (uint64_t)IDA_SERI_RX_N - b1 - f) %
                                              IDA_SERI_RX_N);
                if (p != alim.p || gecen >= TAZELEME_US) {
                    (void)ida_alim_goruntu(&alim, b1, f, b2, simdi, IDA_ALIM_OLAY_YOKLAMA);
                }
            }
        }
    }

    ida_alim_durum_t d = ida_alim_oku(&alim, rx_halka, hedef, azami, &n, t_rx_us, &k, &atilan);
    if (d == IDA_ALIM_VERI) {
        /* Kopyadan sonra alınan görüntüyle doğrulama (süre ve tur denetimi dahil). */
        uint32_t b1 = 0u, f = 0u, b2 = 0u;
        if (!goruntu_oku(&b1, &f, &b2)) {
            b2 = b1 + 1u; /* kararsız: alim.c kopyayı taahhüt etmez */
        }
        d = ida_alim_dogrula(&alim, n, b1, f, b2, ida_bsp_zaman_us(), &atilan);
        if (d != IDA_ALIM_VERI) {
            n = 0u;
        }
    }
    if (d == IDA_ALIM_TASMA) {
        *tasma = true;
        n = 0u;
    }
    *kaba = k != 0u;
    ida_bsp_maske_birak(m);
    return n;
}

void ida_seri_sayaclar(ida_seri_sayac_t *s)
{
    const uint32_t m = ida_bsp_maske_ac(IDA_BSP_ONCELIK_SERI);
    sayac.kararsiz = alim.kararsiz;
    sayac.belirsiz = alim.belirsiz;
    sayac.gecersiz = alim.gecersiz;
    sayac.geri = alim.geri;
    sayac.olay_birlesme = alim.olay_birlesme;
    sayac.olay_eslesme = alim.olay_sayisi[0];
    sayac.olay_bosta = alim.olay_sayisi[1];
    sayac.olay_yarim = alim.olay_sayisi[2];
    sayac.olay_tam = alim.olay_sayisi[3];
    sayac.tasma = alim.tasma;
    sayac.tasma_bayt = alim.tasma_bayt > UINT32_MAX ? UINT32_MAX : (uint32_t)alim.tasma_bayt;
    sayac.halka_azami = alim.halka_azami;
    sayac.rx_durdu = rx_durdu;
    *s = sayac;
    ida_bsp_maske_birak(m);
}

/* ---- başlatma ------------------------------------------------------------------------------ */

static void usart_gpio(void)
{
    __HAL_RCC_GPIOA_CLK_ENABLE();
    GPIO_InitTypeDef g;
    memset(&g, 0, sizeof g);
    g.Pin = GPIO_PIN_9 | GPIO_PIN_10;
    g.Mode = GPIO_MODE_AF_PP;
    g.Pull = GPIO_PULLUP;
    g.Speed = GPIO_SPEED_FREQ_HIGH;
    g.Alternate = GPIO_AF7_USART1;
    HAL_GPIO_Init(GPIOA, &g);
}

/* Alım kanalının yazmaçlarını kurar ve açar (BASEPRI altında ya da başlatmada). */
static void rx_dma_kanal_kur(void)
{
    const uint32_t ctr1 = DMA_CTR1_DINC; /* bayt genişliği, kaynak sabit, port 0 */
    const uint32_t ctr2 = (DMA_ISTEK_USART1_RX << DMA_CTR2_REQSEL_Pos);
    rx_dugum[0] = ctr1;
    rx_dugum[1] = ctr2;
    rx_dugum[2] = IDA_SERI_RX_N;
    rx_dugum[3] = (uint32_t)(uintptr_t)&USART1->RDR;
    rx_dugum[4] = (uint32_t)(uintptr_t)rx_halka;
    rx_dugum[5] = ((uint32_t)(uintptr_t)rx_dugum & DMA_CLLR_LA) | LLI_MASKE;
    RX_CH->CCR = 0u;
    RX_CH->CFCR = DMA_TUM_BAYRAK;
    RX_CH->CTR1 = ctr1;
    RX_CH->CTR2 = ctr2;
    RX_CH->CBR1 = IDA_SERI_RX_N;
    RX_CH->CSAR = (uint32_t)(uintptr_t)&USART1->RDR;
    RX_CH->CDAR = (uint32_t)(uintptr_t)rx_halka;
    RX_CH->CLBAR = (uint32_t)(uintptr_t)rx_dugum & DMA_CLBAR_LBA;
    RX_CH->CLLR = rx_dugum[5];
    RX_CH->CCR = (3u << DMA_CCR_PRIO_Pos) | DMA_CCR_HTIE | DMA_CCR_TCIE | DMA_CCR_DTEIE |
                 DMA_CCR_ULEIE | DMA_CCR_USEIE | DMA_CCR_TOIE;
    RX_CH->CCR |= DMA_CCR_EN;
}

static void rx_dma_baslat(void)
{
    rx_dma_kanal_kur();
    ida_bsp_kesme_ac((int32_t)GPDMA1_Channel0_IRQn, IDA_BSP_ONCELIK_SERI);
    /* İlk görüntü (kanal açıldıktan hemen sonra; ilk turun içindeyiz). */
    const uint32_t m = ida_bsp_maske_ac(IDA_BSP_ONCELIK_SERI);
    goruntu_al(IDA_ALIM_OLAY_YOKLAMA, ida_bsp_zaman_us());
    ida_bsp_maske_birak(m);
}

/* Kısmen açılan aktarımı geri alır (başlatma başarısız). */
static void seri_kapat(void)
{
    NVIC_DisableIRQ(USART1_IRQn);
    NVIC_DisableIRQ(GPDMA1_Channel0_IRQn);
    NVIC_DisableIRQ(GPDMA1_Channel1_IRQn);
    USART1->CR1 = 0u;
    USART1->CR3 = 0u;
    RX_CH->CCR = 0u;
    TX_CH->CCR = 0u;
    RX_CH->CFCR = DMA_TUM_BAYRAK;
    TX_CH->CFCR = DMA_TUM_BAYRAK;
}

static void tx_dma_baslat(void)
{
    TX_CH->CCR = 0u;
    TX_CH->CFCR = DMA_TUM_BAYRAK;
    ida_bsp_kesme_ac((int32_t)GPDMA1_Channel1_IRQn, IDA_BSP_ONCELIK_SERI);
}

int ida_seri_baslat(void)
{
    baslatildi = false;
    rx_durdu = 0u;
    memset(&sayac, 0, sizeof sayac);
    sayac.baud_istenen = IDA_SERI_BAUD;
    /* Hatta gelebilecek en çok bayt/s: BRR'den hesaplanan hızın %10 fazlası (karşı tarafın
       bilinmeyen hız hatasına pay; 8N1, 10 bit/bayt). */
    __HAL_RCC_USART1_CLK_ENABLE();
    __HAL_RCC_GPDMA1_CLK_ENABLE();
    sayac.usart_hz = (uint32_t)HAL_RCCEx_GetPeriphCLKFreq(RCC_PERIPHCLK_USART1);
    if (sayac.usart_hz == 0u || baud_sec(sayac.usart_hz, IDA_SERI_BAUD) != 0) {
        return -1;
    }
    const uint32_t azami_bayt_s = (uint32_t)(((uint64_t)sayac.baud_brr * 11u) / 100u + 1u);
    if (!ida_alim_kur(&alim, IDA_SERI_RX_N, FIFOL_AZAMI, azami_bayt_s)) {
        return -2;
    }
    usart_gpio();
    USART1->CR1 = 0u;
    /* Karakter eşleşmesi: çalışma kipinde 8 bitin tamamı ADD[7:0] ile karşılaştırılır. */
    USART1->CR2 = (0xFDu << USART_CR2_ADD_Pos) | USART_CR2_ADDM7;
    USART1->CR3 = USART_CR3_DMAR | USART_CR3_DMAT | USART_CR3_EIE;
    USART1->PRESC = 0u;
    rx_dma_baslat();
    tx_dma_baslat();
    ida_bsp_kesme_ac((int32_t)USART1_IRQn, IDA_BSP_ONCELIK_SERI);
    const uint32_t cr1 = USART_CR1_FIFOEN | USART_CR1_TE | USART_CR1_RE | USART_CR1_CMIE |
                         USART_CR1_IDLEIE | (sayac.over8 ? USART_CR1_OVER8 : 0u);
    USART1->CR1 = cr1; /* UE = 0: OVER8/FIFOEN/BRR yalnız bu durumda yazılır */
    USART1->BRR = sayac.brr;
    USART1->ICR = USART_ICR_ORECF | USART_ICR_FECF | USART_ICR_NECF | USART_ICR_PECF |
                  USART_ICR_IDLECF | USART_ICR_CMCF;
    USART1->CR1 = cr1 | USART_CR1_UE;
    const uint64_t t = ida_bsp_zaman_us();
    while ((USART1->ISR & (USART_ISR_TEACK | USART_ISR_REACK)) !=
           (USART_ISR_TEACK | USART_ISR_REACK)) {
        if (ida_bsp_zaman_us() - t > 10000u) {
            /* USART hazır olmadı: başarısızlık; açılan aktarım geri alınır. */
            sayac.uart_bekle_hata++;
            seri_kapat();
            return -3;
        }
    }
    baslatildi = true;
    return 0;
}
