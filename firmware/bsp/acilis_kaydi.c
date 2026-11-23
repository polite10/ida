/* acilis_kaydi.c — sıfırlama nedeni, ardışık arıza sayacı ve hata kaydı (acilis_kaydi.h). */
#include "acilis_kaydi.h"

#include <string.h>

#include "ida/port.h"

uint32_t ida_fw_crc32(const void *veri, size_t n)
{
    /* CRC-32/ISO-HDLC: yansıtılmış 0xEDB88320, başlangıç ve son XOR 0xFFFFFFFF. */
    const uint8_t *b = (const uint8_t *)veri;
    uint32_t crc = UINT32_C(0xFFFFFFFF);
    for (size_t i = 0u; i < n; i++) {
        crc ^= b[i];
        for (uint32_t j = 0u; j < 8u; j++) {
            const uint32_t maske = (uint32_t)0u - (crc & 1u);
            crc = (crc >> 1) ^ (UINT32_C(0xEDB88320) & maske);
        }
    }
    return ~crc;
}

int ida_kayit_gecerli(const ida_acilis_kaydi_t *k)
{
    return k->magic == IDA_KAYIT_MAGIC && k->surum == IDA_KAYIT_SURUM &&
           k->crc == ida_fw_crc32(k, offsetof(ida_acilis_kaydi_t, crc));
}

void ida_kayit_muhurle(ida_acilis_kaydi_t *k)
{
    k->magic = IDA_KAYIT_MAGIC;
    k->surum = IDA_KAYIT_SURUM;
    k->crc = ida_fw_crc32(k, offsetof(ida_acilis_kaydi_t, crc));
}

void ida_kayit_kur(ida_acilis_kaydi_t *k)
{
    memset(k, 0, sizeof *k);
    ida_kayit_muhurle(k);
}

void ida_kayit_ardisik_temizle(ida_acilis_kaydi_t *k)
{
    if (!ida_kayit_gecerli(k)) {
        ida_kayit_kur(k);
        return;
    }
    k->ardisik_ariza = 0u;
    ida_kayit_muhurle(k);
}

uint8_t ida_sifirlama_nedeni(uint32_t csr, int ariza_bekliyor)
{
    if ((csr & (IDA_CSR_IWDGRSTF | IDA_CSR_WWDGRSTF)) != 0u) {
        return (uint8_t)IDA_RESET_WATCHDOG;
    }
    if ((csr & IDA_CSR_SFTRSTF) != 0u) {
        return (uint8_t)(ariza_bekliyor ? IDA_RESET_FAULT : IDA_RESET_SOFTWARE);
    }
    if ((csr & IDA_CSR_BORRSTF) != 0u) {
        return (uint8_t)IDA_RESET_POWER_ON;
    }
    if ((csr & IDA_CSR_PINRSTF) != 0u) {
        return (uint8_t)IDA_RESET_PIN;
    }
    return (uint8_t)IDA_RESET_UNKNOWN;
}

void ida_kayit_acilis(ida_acilis_kaydi_t *k, uint32_t csr, ida_acilis_sonuc_t *s)
{
    memset(s, 0, sizeof *s);
    s->kayit_gecerliydi = (uint8_t)(ida_kayit_gecerli(k) ? 1u : 0u);
    if (!s->kayit_gecerliydi) {
        ida_kayit_kur(k);
    }
    const int ariza = k->ariza_bekliyor != 0u;
    s->reset_flags = csr & IDA_CSR_BAYRAKLAR;
    s->reset_cause = ida_sifirlama_nedeni(csr, ariza);
    s->onceki_ariza = (uint8_t)(s->reset_cause == IDA_RESET_FAULT ? 1u : 0u);
    switch (s->reset_cause) {
    case IDA_RESET_FAULT:
    case IDA_RESET_WATCHDOG:
        if (k->ardisik_ariza < 255u) {
            k->ardisik_ariza++;
        }
        break;
    case IDA_RESET_POWER_ON:
    case IDA_RESET_PIN:
    case IDA_RESET_SOFTWARE:
        k->ardisik_ariza = 0u;
        break;
    default:
        break;
    }
    if (k->ardisik_ariza > 255u) {
        k->ardisik_ariza = 255u;
    }
    s->ardisik_ariza = (uint8_t)k->ardisik_ariza;
    k->ariza_bekliyor = 0u;
    ida_kayit_muhurle(k);
}
