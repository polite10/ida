#include <stdlib.h>

/* Dışarıdan gözlenebilir erişimler, ayırma/serbest bırakmanın silinmesini önler. */
void bellek_kullan(size_t boyut)
{
    if (boyut == 0) {
        return;
    }
    volatile unsigned char *bellek = malloc(boyut);
    if (bellek != NULL) {
        bellek[0] = 42;
        free((void *)bellek);
    }
}
