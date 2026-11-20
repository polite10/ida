/*
 * saat_okuyan.c — BİLEREK KURALA AYKIRI deneme nesnesi (A4-K12 denetiminin karşı örneği):
 * saat okur. Çekirdeğe girmez; yalnız dis_basvurular.cmake'in bulgu üretebildiğini gösterir.
 */
#include <time.h>

long ida_deneme_saat(void);

long ida_deneme_saat(void)
{
    return (long)clock();
}
