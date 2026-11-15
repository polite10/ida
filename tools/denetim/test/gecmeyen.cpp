/* İşlev sınırından dönen nesneler, new/delete çağrılarını gözlenebilir tutar. */
int *tek_ayir()
{
    return new int(42);
}

int *dizi_ayir(unsigned boyut)
{
    return new int[boyut];
}

void tek_birak(int *bellek)
{
    delete bellek;
}

void dizi_birak(int *bellek)
{
    delete[] bellek;
}
