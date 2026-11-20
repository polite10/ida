/*
 * hatali.c — statik analiz denetiminin karşı örneği (bilerek hatalı; derlemeye girmez).
 *
 * tools/denetim/statik-analiz.sh bu dosyada clang-tidy'nin ve cppcheck'in en az birer
 * bulgu üretmesini bekler; üretmezlerse araç çalışmıyor sayılır ve denetim başarısız olur.
 */
#include <stddef.h>
#include <string.h>

int ida_hatali_dizi(int i);
int ida_hatali_bos(void);
void ida_hatali_kopya(const char *kaynak);

int ida_hatali_dizi(int i)
{
    int dizi[4] = {0, 1, 2, 3};
    (void)i;
    return dizi[4]; /* sınır dışı okuma */
}

int ida_hatali_bos(void)
{
    int *p = NULL;
    return *p; /* boş gösterici */
}

void ida_hatali_kopya(const char *kaynak)
{
    char hedef[4];
    strcpy(hedef, kaynak); /* sınırsız kopya */
    (void)hedef;
}
