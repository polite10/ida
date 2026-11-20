/* A1-K03 derleme denetimi (ISO C17): üretilmiş MAVLink başlığı ile port.h aynı çeviri
 * biriminde; protocol/ayna.json'dan üretilmiş her satır bir IDAW_ = IDA_ eşitliğini
 * _Static_assert ile denetler. Eşitlik tutmazsa bu dosya derlenmez ("ayna_esitsizligi"). */
#include "ida/port.h"
#include "mavlink/ida/mavlink.h"

#include "ida_link_ayna_esitlik.h"

unsigned ida_link_ayna_esitlik_sayisi_c17(void);
unsigned ida_link_ayna_esitlik_sayisi_c17(void)
{
    return (unsigned)IDA_LINK_AYNA_ESITLIK_SAYISI;
}
