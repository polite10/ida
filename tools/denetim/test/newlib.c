#include <stddef.h>

struct _reent;
extern void *_malloc_r(struct _reent *durum, size_t boyut);
extern void *_sbrk(ptrdiff_t artis);

void *newlib_ayir(struct _reent *durum, size_t boyut)
{
    return _malloc_r(durum, boyut);
}

void *yigin_buyut(ptrdiff_t artis)
{
    return _sbrk(artis);
}
