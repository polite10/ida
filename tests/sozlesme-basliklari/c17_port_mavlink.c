/* Sözleşme başlıkları denetimi: port.h ile protocol/ida.xml'den üretilen MAVLink başlığı
 * aynı çeviri biriminde, ISO C17 (W1-uyum U15: tel enumları IDAW_, port sabitleri IDA_). */
#include "ida/port.h"
#include "mavlink/ida/mavlink.h"

/* İki öneki birlikte kullanan ifade: ad çakışması derlemeyi bozardı. */
_Static_assert((unsigned)IDAW_ROLE_BOARD == 1u, "IDAW_ onekli tel enumu");
_Static_assert(IDA_PORT_N_THR == 8u, "IDA_ onekli port sabiti");

unsigned ida_denetim_c17_port_mavlink(void);
unsigned ida_denetim_c17_port_mavlink(void)
{
    return (unsigned)MAVLINK_MSG_ID_IDA_IMU + (unsigned)IDA_PORT_N_TUBE;
}
