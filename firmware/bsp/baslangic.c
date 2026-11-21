/*
 * baslangic.c — başlangıç kodu ve vektör tablosu (STM32U5A9, Cortex-M33; 01 §2.2 md.1, §2.8).
 *
 * Bu nesne bütün kart imajlarına bağlanır (ida_fw_baslangic; kart yazılımı ve kartta bağlanan
 * birim test imajları). Bu yüzden yalnız kendi içinde tanımlı şeylere ve zayıf simgelere
 * dayanır: çevre birimi kesme işleyicileri ve hata kaydı işlevi zayıftır, kart yazılımı güçlü
 * tanımlarını verir; bağlayıcı betiğinin yığın tabanı simgesi zayıftır (W2'nin yer tutucu
 * betiğinde yoktur; yoksa MSPLIM ve boyama atlanır).
 *
 * Reset_Handler: MSPLIM (yığın tabanı), FPU erişimi, FPSCR/FPDSCR (en yakına yuvarlama,
 * FZ = 0, DN = 0; geri okunur), .data/.bss, ana yığının boyanması (en yüksek kullanım için),
 * main.
 *
 * Hata işleyicileri (HardFault, MemManage, BusFault, UsageFault; NMI bsp/hata.c'de): ilk komutlar MSP'yi
 * 1 KB'lık ayrı acil yığına, MSPLIM'i onun tabanına kurar; sonra C işlevi
 * ida_bsp_hata_isle(çerçeve, EXC_RETURN) bu yığında koşar (bsp/hata.c: kayıt ve sıfırlama).
 */
#include <stdint.h>

#include "stm32u5xx.h"

extern uint32_t _sidata[];
extern uint32_t _sdata[];
extern uint32_t _edata[];
extern uint32_t _sbss[];
extern uint32_t _ebss[];
extern uint32_t _estack[];
/* Ana yığının tabanı (firmware/bsp/stm32u5a9_ida.ld); yer tutucu betikte yoktur. */
extern uint32_t _ida_yigin_tabani[] __attribute__((weak));

#define IDA_YIGIN_DESENI 0xA5A5A5A5u
#define IDA_ACIL_YIGIN_BAYT 1024u

int main(void);
void Reset_Handler(void);
void ida_varsayilan_isleyici(void);
void ida_fw_bekle(void);
void ida_bsp_hata_isle(uint32_t *cerceve, uint32_t exc_return);

uint32_t ida_bsp_fpscr_acilis;

/* Acil yığın (hata işleyicileri; 01 §2.8). 8 bayt hizalı. */
__attribute__((aligned(8), used)) uint8_t ida_bsp_acil_yigin[IDA_ACIL_YIGIN_BAYT];

void ida_fw_bekle(void)
{
    for (;;) {
        __WFI();
    }
}

void Reset_Handler(void)
{
    if ((uintptr_t)_ida_yigin_tabani != 0u) {
        __set_MSPLIM((uint32_t)(uintptr_t)_ida_yigin_tabani);
    }
    /* FPU: CP10 ve CP11 tam erişim. */
    SCB->CPACR |= (0xFu << 20);
    __DSB();
    __ISB();
    /* En yakına yuvarlama (RMode 00), FZ = 0, DN = 0, AHP = 0; istisna bağlamının varsayılanı
       da aynı (FPDSCR). */
    __set_FPSCR(__get_FPSCR() & ~((3u << 22) | (1u << 24) | (1u << 25) | (1u << 26)));
    FPU->FPDSCR = FPU->FPDSCR & ~((3u << 22) | (1u << 24) | (1u << 25) | (1u << 26));
    const uint32_t fpscr = __get_FPSCR();

    const uint32_t *kaynak = _sidata;
    for (uint32_t *hedef = _sdata; hedef < _edata; ++hedef) {
        *hedef = *kaynak++;
    }
    for (uint32_t *hedef = _sbss; hedef < _ebss; ++hedef) {
        *hedef = 0u;
    }
    ida_bsp_fpscr_acilis = fpscr;
    /* Ana yığının boyanması: tabandan bu işlevin çerçevesinin 64 B altına kadar. */
    if ((uintptr_t)_ida_yigin_tabani != 0u) {
        const uintptr_t sinir = (uintptr_t)__get_MSP() - 64u;
        for (volatile uint32_t *s = _ida_yigin_tabani; (uintptr_t)s < sinir; ++s) {
            *s = IDA_YIGIN_DESENI;
        }
    }

    (void)main();
    ida_fw_bekle();
}

void ida_varsayilan_isleyici(void)
{
    /* Beklenmeyen kesme: hata kaydıyla aynı yol (istisna numarası kayıtta). */
    ida_bsp_hata_isle((uint32_t *)0, 0u);
}

/* Zayıf hata kaydı: kart yazılımı bsp/hata.c'de güçlüsünü verir. */
__attribute__((weak, noreturn)) void ida_bsp_hata_isle(uint32_t *cerceve, uint32_t exc_return)
{
    (void)cerceve;
    (void)exc_return;
    NVIC_SystemReset();
}

/* Hata girişi: MSP'yi acil yığına geçirir ve C işlevine dallanır (r0 = çerçeve, r1 = EXC_RETURN).
   MSPLIM önce acil yığının tabanına iner (acil yığın ana yığının altındaki SRAM1'dedir, yani o
   anki SP'nin altında kalır), sonra MSP acil yığının tepesine kurulur. */
#define IDA_HATA_GIRISI(ad)                                                                       \
    __attribute__((naked, noreturn)) void ad(void)                                                \
    {                                                                                             \
        __asm volatile("mrs r0, msp\n\t"                                                          \
                       "tst lr, #4\n\t"                                                           \
                       "it ne\n\t"                                                                \
                       "mrsne r0, psp\n\t"                                                        \
                       "mov r1, lr\n\t"                                                           \
                       "movw r2, #:lower16:ida_bsp_acil_yigin\n\t"                                \
                       "movt r2, #:upper16:ida_bsp_acil_yigin\n\t"                                \
                       "msr msplim, r2\n\t"                                                       \
                       "add r3, r2, %0\n\t"                                                       \
                       "msr msp, r3\n\t"                                                          \
                       "b ida_bsp_hata_isle\n\t" ::"i"(IDA_ACIL_YIGIN_BAYT));                    \
    }

IDA_HATA_GIRISI(HardFault_Handler)
IDA_HATA_GIRISI(MemManage_Handler)
IDA_HATA_GIRISI(BusFault_Handler)
IDA_HATA_GIRISI(UsageFault_Handler)

/* Kart yazılımının kullandığı kesmeler: zayıf, varsayılan işleyiciye bağlı. */
#define IDA_ZAYIF(ad) void ad(void) __attribute__((weak, alias("ida_varsayilan_isleyici")))
IDA_ZAYIF(NMI_Handler); /* güçlüsü bsp/hata.c (flash ECC ayrımı) */
IDA_ZAYIF(SecureFault_Handler);
IDA_ZAYIF(SVC_Handler);
IDA_ZAYIF(DebugMon_Handler);
IDA_ZAYIF(PendSV_Handler);
IDA_ZAYIF(SysTick_Handler);
IDA_ZAYIF(TIM2_IRQHandler);
IDA_ZAYIF(USART1_IRQHandler);
IDA_ZAYIF(GPDMA1_Channel0_IRQHandler);
IDA_ZAYIF(GPDMA1_Channel1_IRQHandler);

typedef void (*ida_vektor_t)(void);

#define IDA_IRQ_SAYISI ((uint32_t)DCACHE2_IRQn + 1u) /* STM32U5A9: 139 çevre birimi kesmesi */
#define IDA_VEKTOR_SAYISI (16u + IDA_IRQ_SAYISI)

/* Vektör tablosu: bütün girdiler önce varsayılan işleyiciye kurulur, sonra kullanılanlar
   yazılır (GNU aralık başlatıcısı; platform kodu gnu17). */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Woverride-init"
__attribute__((section(".isr_vector"), used))
const ida_vektor_t ida_vektor_tablosu[IDA_VEKTOR_SAYISI] = {
    [0 ... IDA_VEKTOR_SAYISI - 1u] = ida_varsayilan_isleyici,
    [0] = (ida_vektor_t)(uintptr_t)_estack,
    [1] = Reset_Handler,
    [2] = NMI_Handler,
    [3] = HardFault_Handler,
    [4] = MemManage_Handler,
    [5] = BusFault_Handler,
    [6] = UsageFault_Handler,
    [7] = SecureFault_Handler,
    [8] = 0,
    [9] = 0,
    [10] = 0,
    [11] = SVC_Handler,
    [12] = DebugMon_Handler,
    [13] = 0,
    [14] = PendSV_Handler,
    [15] = SysTick_Handler,
    [16 + TIM2_IRQn] = TIM2_IRQHandler,
    [16 + USART1_IRQn] = USART1_IRQHandler,
    [16 + GPDMA1_Channel0_IRQn] = GPDMA1_Channel0_IRQHandler,
    [16 + GPDMA1_Channel1_IRQn] = GPDMA1_Channel1_IRQHandler,
};
#pragma GCC diagnostic pop
