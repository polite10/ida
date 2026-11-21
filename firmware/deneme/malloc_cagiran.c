/*
 * malloc_cagiran.c — bilerek bozulmuş deneme nesnesi (W2-depo kabul md.2).
 *
 * Bellek ayırma denetimi bu nesnede malloc başvurusunu bulmalı; bu nesneyle bağlanan
 * imaj ise --wrap=malloc yüzünden bağlanmamalıdır. Ürün koduna girmez.
 */
#include <stddef.h>
#include <stdlib.h>

void *ida_deneme_ayir(size_t n);

void *ida_deneme_ayir(size_t n)
{
    return malloc(n);
}
