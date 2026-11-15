#include <stddef.h>

#if defined(__APPLE__)
extern void *malloc(size_t boyut) __attribute__((weak_import));
#else
extern void *malloc(size_t boyut) __attribute__((weak));
#endif

void *zayif_ayir(size_t boyut)
{
    return malloc != NULL ? malloc(boyut) : NULL;
}
