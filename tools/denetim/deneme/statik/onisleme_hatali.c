/* onisleme_hatali.c — statik analiz denetiminin kendi testi: önişleme hatası. Analiz
 * yapılamaz; bu bulgu değil "araç çalışmadı"dır (beklenen çıkış 70). Karşı örnek yerine
 * verildiğinde de "error" satırı beklenen denetim kimliklerinin yerine geçmemelidir. */
#error denetim_calismadi

int ida_deneme_onisleme(void);
