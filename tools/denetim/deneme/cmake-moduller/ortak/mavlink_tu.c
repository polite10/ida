/* Deneme kaynağı: üretilmiş MAVLink başlığını içerir (iki dizin aynı üretimi paylaşır). */
#include "mavlink/ida/mavlink.h"

unsigned ida_deneme_mavlink(void);
unsigned ida_deneme_mavlink(void) { return (unsigned)MAVLINK_MSG_ID_IDA_IMU; }
