/*
 * tip_karisimi.c — DERLENMEMESİ GEREKEN deneme (A4-K07): ölçüm zamanı duvar saatine atanamaz.
 * cekirdek.k07_tip_ayrimi testi bu dosyanın derleme hatası verdiğini doğrular.
 */
#include "ida/port.h"

ida_wtime_t ida_deneme_karisim(ida_stime_t t);

ida_wtime_t ida_deneme_karisim(ida_stime_t t)
{
    ida_wtime_t w = {0};
    w = t; /* derleme hatası beklenir: uyumsuz tipler */
    return w;
}
