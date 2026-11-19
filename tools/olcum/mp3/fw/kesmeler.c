/* W0-MP3a ölçüm prototipi: kesme işleyicileri (atılabilir). USART1 ve GPDMA işleyicileri platform.c'dedir. */
#include "stm32u5xx_hal.h"

void SysTick_Handler(void) { HAL_IncTick(); }

/* Hata durumunda kırmızı LED (PE1) yanar ve işlemci burada kalır. */
static void hata_dongusu(void)
{
    GPIOE->BSRR = GPIO_PIN_1;
    for (;;) { __NOP(); }
}

void NMI_Handler(void) { hata_dongusu(); }
void HardFault_Handler(void) { hata_dongusu(); }
void MemManage_Handler(void) { hata_dongusu(); }
void BusFault_Handler(void) { hata_dongusu(); }
void UsageFault_Handler(void) { hata_dongusu(); }
