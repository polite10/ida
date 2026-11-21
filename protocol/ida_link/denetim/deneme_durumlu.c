/* Simge denetiminin deneme girdisi: değişebilir static durum taşıyan nesne. Denetim bunu
 * "[SIMGE_DENETIMI] değişebilir durum simgesi" ile reddetmelidir (protokol.simge.deneme_*). */
unsigned ida_link_deneme_sayac_artir(void);
unsigned ida_link_deneme_sayac_artir(void)
{
    static unsigned sayac;
    return ++sayac;
}
