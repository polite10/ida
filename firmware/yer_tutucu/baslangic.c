/*
 * baslangic.c — W2 yer tutucu başlangıç dosyası (Cortex-M33).
 *
 * Yalnız kartsız bağlama içindir: vektör tablosunun çekirdek istisna girişleri ve
 * Reset_Handler (FPU erişimini açar, .data'yı kopyalar, .bss'i sıfırlar, main'i çağırır).
 * Saat kurulumu, kesme tablosunun çevre birimi girişleri ve hata işleyicileri W3 kart
 * yazılımı işinindir; o iş ST'nin başlangıç dosyasını firmware/bsp/ altında sahiplenir.
 */
#include <stdint.h>

/* Bağlayıcı betiğinin simgeleri (stm32u5a9_yer_tutucu.ld). Dizi olarak bildirilir:
 * tek nesne bildirimi sınır dışı erişim gibi görünür (GCC -Warray-bounds, -fanalyzer). */
extern uint32_t _sidata[];
extern uint32_t _sdata[];
extern uint32_t _edata[];
extern uint32_t _sbss[];
extern uint32_t _ebss[];
extern uint32_t _estack[];

int main(void);
void Reset_Handler(void);
void ida_varsayilan_isleyici(void);
void ida_fw_bekle(void);

/* Kesme bekleyerek sonsuza dek durur. Döngü bilinçlidir; WFI, GCC -fanalyzer'ın
 * "yan etkisiz sonsuz döngü" uyarısını gerçek bir bekleme yönergesiyle karşılar. */
void ida_fw_bekle(void)
{
    for (;;) {
        __asm volatile("wfi" ::: "memory");
    }
}

typedef void (*ida_vektor_t)(void);

/* SCB->CPACR: CP10 ve CP11 için tam erişim (ARMv8-M, B3.2.20). */
#define IDA_SCB_CPACR (*(volatile uint32_t *)0xE000ED88u)

void Reset_Handler(void)
{
    IDA_SCB_CPACR |= (0xFu << 20);
    __asm volatile("dsb\n\tisb" ::: "memory");

    const uint32_t *kaynak = _sidata;
    for (uint32_t *hedef = _sdata; hedef < _edata; ++hedef) {
        *hedef = *kaynak++;
    }
    for (uint32_t *hedef = _sbss; hedef < _ebss; ++hedef) {
        *hedef = 0u;
    }

    (void)main();
    ida_fw_bekle();
}

void ida_varsayilan_isleyici(void)
{
    ida_fw_bekle();
}

/* Çekirdek istisna girişleri (ARMv8-M ana uzantı, güvenli olmayan durum ayrımı yok). */
__attribute__((section(".isr_vector"), used))
const ida_vektor_t ida_vektor_tablosu[16] = {
    (ida_vektor_t)(uintptr_t)_estack, /* 0: başlangıç yığın göstericisi */
    Reset_Handler,                    /* 1 */
    ida_varsayilan_isleyici,          /* 2: NMI */
    ida_varsayilan_isleyici,          /* 3: HardFault */
    ida_varsayilan_isleyici,          /* 4: MemManage */
    ida_varsayilan_isleyici,          /* 5: BusFault */
    ida_varsayilan_isleyici,          /* 6: UsageFault */
    ida_varsayilan_isleyici,          /* 7: SecureFault */
    0, 0, 0,                          /* 8-10: ayrılmış */
    ida_varsayilan_isleyici,          /* 11: SVCall */
    ida_varsayilan_isleyici,          /* 12: DebugMonitor */
    0,                                /* 13: ayrılmış */
    ida_varsayilan_isleyici,          /* 14: PendSV */
    ida_varsayilan_isleyici,          /* 15: SysTick */
};
