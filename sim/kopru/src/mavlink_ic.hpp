// Üretilmiş MAVLink başlığının yalnız mesaj yapıları ve kimlik sabitleri için içerilmesi.
// Kitaplığın kanal dizileri tanımlanmaz ve kullanılmaz (A1-K101; ida_link ust_ic.h ve
// sim/sil ile aynı önlem). Çerçeveleme ida_link tel katmanındadır.
#pragma once

#define MAVLINK_EXTERNAL_RX_STATUS 1
#define MAVLINK_EXTERNAL_RX_BUFFER 1
#include "mavlink/mavlink_types.h"
extern "C" {
extern mavlink_status_t m_mavlink_status[MAVLINK_COMM_NUM_BUFFERS];
extern mavlink_message_t m_mavlink_buffer[MAVLINK_COMM_NUM_BUFFERS];
}
#include "mavlink/ida/mavlink.h"
