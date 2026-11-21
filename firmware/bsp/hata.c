/*
 * hata.c — işlemci hata işleyicisinin kaydı (01 §2.8 "Hata işleyicileri"; W3-kart dilim 1).
 *
 * baslangic.c'deki hata girişleri bu işleve acil yığında dallanır (r0 = yığına itilmiş çerçeve,
 * r1 = EXC_RETURN). İşlev CFSR, HFSR, MMFAR, BFAR'ı, çekirdek çağrısı sayacını ve ham duvar
 * sayacını .noinit kaydına yazar; yığına itilmiş PC/LR/xPSR'yi yalnız itme hatası yoksa ve
 * çerçeve RAM'de, hizalıysa okur (yigin_gecerli). Kaydı mühürler, "bekleyen arıza" işaretini
 * kurar ve sistemi sıfırlar; bir sonraki açılış nedeni FAULT olarak çözer ve ardışık arıza
 * sayacını artırır (acilis_kaydi.c). Duvar saatini okumaz (01 §2.5). İşleyicinin kendisi hata
 * verirse işlemci kilitlenir ve IWDG sıfırlar.
 */
#include "bsp.h"

#include "stm32u5xx.h"

extern ida_acilis_kaydi_t ida_bsp_noinit_kayit; /* bsp.c */

/* SRAM1–SRAM5 bitişik aralığı (01 §2.10). */
#define IDA_RAM_BAS 0x20000000u
#define IDA_RAM_SON 0x20270000u

__attribute__((noreturn)) void ida_bsp_hata_isle(uint32_t *cerceve, uint32_t exc_return)
{
    (void)exc_return;
    ida_acilis_kaydi_t *k = &ida_bsp_noinit_kayit;
    if (!ida_kayit_gecerli(k)) {
        ida_kayit_kur(k);
    }
    const uint32_t cfsr = SCB->CFSR;
    k->cfsr = cfsr;
    k->hfsr = SCB->HFSR;
    k->mmfar = SCB->MMFAR;
    k->bfar = SCB->BFAR;
    k->adim = ida_bsp_adim_sayaci;
    k->duvar_ham = TIM2->CNT;
    k->istisna = __get_IPSR();
    const uint32_t itme_hatasi = SCB_CFSR_MSTKERR_Msk | SCB_CFSR_STKERR_Msk | SCB_CFSR_STKOF_Msk;
    const uintptr_t a = (uintptr_t)cerceve;
    if (cerceve != (uint32_t *)0 && (cfsr & itme_hatasi) == 0u && (a & 7u) == 0u &&
        a >= IDA_RAM_BAS && a + 32u <= IDA_RAM_SON) {
        k->lr = cerceve[5];
        k->pc = cerceve[6];
        k->xpsr = cerceve[7];
        k->yigin_gecerli = 1u;
    } else {
        k->lr = 0u;
        k->pc = 0u;
        k->xpsr = 0u;
        k->yigin_gecerli = 0u;
    }
    k->ariza_var = 1u;
    k->ariza_sayisi = k->ariza_sayisi + 1u;
    k->ariza_bekliyor = 1u;
    ida_kayit_muhurle(k);
    __DSB();
    NVIC_SystemReset();
}
