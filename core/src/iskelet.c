/*
 * iskelet.c — boş çekirdeğin yer tutucu kaynağı (W2-depo).
 *
 * Sözleşme başlıklarını çekirdeğin kendi derleme ayarlarıyla (ISO C17, -pedantic-errors,
 * uyarı hatadır) derler ve test zincirine bağlanacak tek bir işlev sağlar.
 */
#include "iskelet.h"

#include "ida/port.h"

uint32_t ida_core_iskelet_surumu(void)
{
    return ((uint32_t)IDA_PORT_VERSION_MAJOR << 16) | (uint32_t)IDA_PORT_VERSION_MINOR;
}
