/*
 * test_cikis.c — kartta bağlanan birim testlerinin karakter çıkışı (yer tutucu).
 * W3'te USART1 ya da ITM'ye yönlendirilir.
 */
#include <stdint.h>

void ida_test_karakter_yaz(int c);

volatile uint8_t ida_test_son_karakter;

void ida_test_karakter_yaz(int c)
{
    ida_test_son_karakter = (uint8_t)c;
}
