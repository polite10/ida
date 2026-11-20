// ida_link/tel.h C++20'den içerilebilir (W2-protokol §2): açık başlık, üretilmiş MAVLink
// başlığı ve port.h aynı çeviri biriminde. Yalnız derlenir; C++ sarmalayıcı bu işte yok.
#include "ida_link/tel.h"

#include <cstddef>

#include "ida/port.h"
#include "mavlink/ida/mavlink.h"

static_assert(IDA_LINK_CERCEVE_AZAMI == IDA_LINK_EK_BOY + IDA_LINK_YUK_AZAMI);
static_assert(IDA_LINK_DIALECT_YUK_AZAMI <= IDA_LINK_YUK_AZAMI);
static_assert(static_cast<int>(IDA_LINK_HATA_TAMPON) < 0);

int ida_link_cxx20_ornek(const unsigned char *veri, std::size_t n);
int ida_link_cxx20_ornek(const unsigned char *veri, std::size_t n)
{
    ida_link_alici_t alici{};
    if (ida_link_alici_kur(&alici, IDA_LINK_ROL_SIM) != IDA_LINK_TAMAM) {
        return -1;
    }
    ida_link_cerceve_t cerceve{};
    std::size_t tuketilen = 0;
    const ida_link_rc_t rc = ida_link_al(&alici, veri, n, &tuketilen, &cerceve);
    if (rc != IDA_LINK_CERCEVE || cerceve.msgid != MAVLINK_MSG_ID_IDA_ACTUATOR) {
        return 0;
    }
    mavlink_ida_actuator_t mesaj{};
    return ida_link_coz(&cerceve, MAVLINK_MSG_ID_IDA_ACTUATOR, &mesaj, sizeof mesaj) ==
                   IDA_LINK_TAMAM
               ? static_cast<int>(mesaj.out_seq)
               : -1;
}
