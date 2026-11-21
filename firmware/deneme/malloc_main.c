/*
 * malloc_main.c — malloc çağıran deneme imajının main'i (W2-depo kabul md.2).
 * Bu imaj bağlanmamalıdır: __wrap_malloc tanımsızdır.
 */
#include <stddef.h>

void *ida_deneme_ayir(size_t n);

volatile void *ida_deneme_sonuc;

int main(void)
{
    ida_deneme_sonuc = ida_deneme_ayir(16u);
    return 0;
}
