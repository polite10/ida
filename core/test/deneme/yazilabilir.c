/*
 * yazilabilir.c — BİLEREK KURALA AYKIRI deneme nesnesi (A4-K22 denetiminin karşı örneği).
 * Çekirdeğe girmez; yalnız yazilabilir_veri.cmake'in bulgu üretebildiğini gösterir.
 */
#include <stdint.h>

uint32_t ida_deneme_artir(void);

static uint32_t ida_deneme_sayac;

uint32_t ida_deneme_artir(void)
{
    ida_deneme_sayac++;
    return ida_deneme_sayac;
}
