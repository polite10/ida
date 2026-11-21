/* Sözleşme başlıkları denetimi: A4 port.h, ISO C17 (gerçek algilama_kaydi.h ile). */
#include "ida/port.h"

_Static_assert(IDA_PORT_VERSION_MAJOR == 1u, "A4 ana surum");
_Static_assert(IDA_PORT_MAX_RESULTS == IDA_PORT_MAX_CMDS + IDA_PORT_N_NARROW, "A4-K43");

size_t ida_denetim_c17_port(void);
size_t ida_denetim_c17_port(void)
{
    return sizeof(ida_in_t) + sizeof(ida_out_t) + sizeof(ida_init_in_t);
}
