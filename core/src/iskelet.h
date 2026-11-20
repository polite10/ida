/*
 * iskelet.h — boş çekirdeğin yer tutucu bildirimi (W2-depo).
 *
 * Derleme ve test zincirini göstermek içindir; ürün arayüzü değildir. Çekirdek iskeleti
 * işi (W2) bu dosyayı ve iskelet.c'yi kaldırır ya da yerine gerçek kaynakları koyar.
 */
#ifndef IDA_CORE_ISKELET_H
#define IDA_CORE_ISKELET_H

#include <stdint.h>

/* A4 port sürümünü (IDA_PORT_VERSION_MAJOR << 16 | MINOR) döndürür. */
uint32_t ida_core_iskelet_surumu(void);

#endif /* IDA_CORE_ISKELET_H */
