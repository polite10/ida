// Sözleşme başlıkları denetimi: port.h + üretilen MAVLink başlığı, C++20 (ida_link'in C++
// tarafı; W1-uyum U15).
#include "ida/port.h"
#include "mavlink/ida/mavlink.h"

static_assert(static_cast<unsigned>(IDAW_ROLE_BOARD) == 1u, "IDAW_ önekli tel enumu");
static_assert(IDA_PORT_N_THR == 8u, "IDA_ önekli port sabiti");

unsigned ida_denetim_cxx20_port_mavlink();
unsigned ida_denetim_cxx20_port_mavlink()
{
    return static_cast<unsigned>(MAVLINK_MSG_ID_IDA_IMU) + IDA_PORT_N_TUBE;
}
