// A1-K03 derleme denetimi (C++20): ayna_c17.c'nin C++ karşılığı (static_assert).
#include "ida/port.h"
#include "mavlink/ida/mavlink.h"

#include "ida_link_ayna_esitlik.h"

unsigned ida_link_ayna_esitlik_sayisi_cxx20();
unsigned ida_link_ayna_esitlik_sayisi_cxx20()
{
    return static_cast<unsigned>(IDA_LINK_AYNA_ESITLIK_SAYISI);
}
