/* bulgulu.c — statik analiz denetiminin kendi testi: gerçek bulgu içeren, derlenebilir girdi
 * (beklenen çıkış 1). */
#include <stddef.h>

int ida_deneme_bulgulu(void);

int ida_deneme_bulgulu(void)
{
    int *p = NULL;
    return *p; /* boş gösterici */
}
