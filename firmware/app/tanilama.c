/* tanilama.c — A1 tanılama ve duyuru çerçevelerinin kodlanması (tanilama.h). */
#include "tanilama.h"

#include <string.h>

/* Üretilmiş MAVLink başlığından yalnız mesaj yapı tipleri ve sabitler kullanılır; kanal
   dizileri tanımlanmaz (protocol/ida_link/src/tel.c ile aynı yöntem, A1-K101). */
#define MAVLINK_EXTERNAL_RX_STATUS 1
#define MAVLINK_EXTERNAL_RX_BUFFER 1
#include "mavlink/mavlink_types.h"
extern mavlink_status_t m_mavlink_status[MAVLINK_COMM_NUM_BUFFERS];
extern mavlink_message_t m_mavlink_buffer[MAVLINK_COMM_NUM_BUFFERS];
#include "mavlink/ida/mavlink.h"

#include "ida/algilama_kaydi.h"
#include "ida/port.h"

_Static_assert(sizeof(((mavlink_ida_link_stats_t *)0)->cnt) == IDA_FW_LINK_STATS_N * sizeof(uint32_t),
               "IDA_LINK_STATS.cnt[36] (A1 Tablo 3.11b)");

void ida_fw_link_stats_dizisi(const ida_link_sayaclar_t *tel, const ida_fw_plat_sayac_t *plat,
                              uint32_t cnt[IDA_FW_LINK_STATS_N])
{
    memset(cnt, 0, IDA_FW_LINK_STATS_N * sizeof(uint32_t));
    cnt[0] = tel->rx_bytes;
    cnt[1] = tel->rx_frames;
    cnt[2] = tel->rx_crc_err;
    cnt[3] = tel->rx_unknown_msgid;
    cnt[4] = tel->rx_iflag;
    cnt[5] = tel->rx_bad_len;
    cnt[6] = tel->rx_wrong_sys;
    cnt[7] = tel->rx_mav1;
    cnt[8] = tel->rx_overflow;
    cnt[9] = tel->rx_resync_bytes;
    cnt[10] = tel->rx_seq_gap;
    cnt[13] = plat->stamp_coarse;
    /* 11, 12, 14–27: oturum, parti, Ping360 ve komut sayaçları; dilim 1'de oturum yok (0). */
    cnt[28] = plat->grid_missed;
    cnt[29] = plat->out_fixed;
    cnt[30] = plat->tx_frames;
    cnt[31] = plat->tx_dropped;
    cnt[32] = plat->tx_crit_stall;
    /* 33–35: olay ve oturum sayaçları (0). */
}

ida_link_rc_t ida_fw_link_stats_kodla(ida_link_gonderici_t *g, const ida_link_sayaclar_t *tel,
                                      const ida_fw_plat_sayac_t *plat, uint8_t *cikis,
                                      size_t kapasite, size_t *yazilan)
{
    if (tel == NULL || plat == NULL) {
        return IDA_LINK_HATA_ARGUMAN;
    }
    mavlink_ida_link_stats_t m;
    memset(&m, 0, sizeof m);
    ida_fw_link_stats_dizisi(tel, plat, m.cnt);
    return ida_link_gonder(g, MAVLINK_MSG_ID_IDA_LINK_STATS, &m, sizeof m, cikis, kapasite,
                           yazilan);
}

ida_link_rc_t ida_fw_duyuru_kodla(ida_link_gonderici_t *g, const ida_fw_duyuru_t *d,
                                  uint8_t *cikis, size_t kapasite, size_t *yazilan)
{
    if (d == NULL) {
        return IDA_LINK_HATA_ARGUMAN;
    }
    mavlink_ida_board_hello_t m;
    memset(&m, 0, sizeof m);
    m.dialect_hash = IDA_LINK_DIALECT_HASH;
    m.param_schema_hash = d->param_schema_hash;
    m.peer_nonce = 0u; /* duyuru */
    m.nonce = d->nonce;
    m.boot_count = d->boot_count;
    m.reset_flags = d->reset_flags;
    m.core_version = d->core_version;
    m.fw_version = d->fw_version;
    m.fw_git = d->fw_git;
    m.param_blob_crc = 0u; /* kalıcı blok yok (dilim 1) */
    m.param_gen = 0u;
    m.last_cmd_id = 0u;
    m.ctrl_period_ns = 5000000u; /* A1-K14: 1.0'da 5 ms */
    m.param_blob_len = 0u;
    m.port_major = (uint16_t)IDA_PORT_VERSION_MAJOR;
    m.port_minor = (uint16_t)IDA_PORT_VERSION_MINOR;
    m.det_schema_major = (uint16_t)IDA_DET_SCHEMA_MAJOR;
    m.det_schema_minor = (uint16_t)IDA_DET_SCHEMA_MINOR;
    m.prev_sess = 0u;
    m.sessions_since_boot = 0u;
    m.proto_major = 1u;
    m.proto_minor = 0u;
    m.role = (uint8_t)IDAW_ROLE_BOARD;
    m.reset_cause = d->reset_cause;
    m.consecutive_faults = d->consecutive_faults;
    m.boot_count_ok = d->boot_count_ok;
    m.param_src = (uint8_t)IDA_PSRC_NONE;
    m.init_rc = d->init_rc;
    return ida_link_gonder(g, MAVLINK_MSG_ID_IDA_BOARD_HELLO, &m, sizeof m, cikis, kapasite,
                           yazilan);
}
