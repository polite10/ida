/* temiz.c — statik analiz denetiminin kendi testi: bulgusuz girdi (beklenen çıkış 0). */
#include <stdint.h>

uint32_t ida_deneme_topla(uint32_t a, uint32_t b);

uint32_t ida_deneme_topla(uint32_t a, uint32_t b)
{
    return a + b;
}
