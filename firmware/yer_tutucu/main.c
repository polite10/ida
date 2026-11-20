/*
 * main.c — W2 yer tutucu kart uygulaması: boş döngü.
 *
 * Çekirdek kitaplığını bağlamaya sokmak için yer tutucu çekirdek işlevini çağırır; kart
 * yazılımının gerçek uygulaması (kontrol döngüsü, A1 bağlantısı) W3'ündür.
 */
#include <stdint.h>

#include "iskelet.h"

void ida_fw_bekle(void); /* baslangic.c */

volatile uint32_t ida_fw_cekirdek_surumu;

int main(void)
{
    ida_fw_cekirdek_surumu = ida_core_iskelet_surumu();
    ida_fw_bekle();
    return 0;
}
