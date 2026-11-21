/*
 * test_tanilama.c — IDA_LINK_STATS ve IDA_BOARD_HELLO kodlaması (firmware/app/tanilama.h) birim
 * testleri. Kodlanan çerçeve tel katmanının sim rolündeki alıcısıyla çözülür ve alanlar
 * karşılaştırılır (A1 Tablo 3.11b sırası, A1-K11 duyurusu).
 */
#include "tanilama.h"

#define MAVLINK_EXTERNAL_RX_STATUS 1
#define MAVLINK_EXTERNAL_RX_BUFFER 1
#include "mavlink/mavlink_types.h"
extern mavlink_status_t m_mavlink_status[MAVLINK_COMM_NUM_BUFFERS];
extern mavlink_message_t m_mavlink_buffer[MAVLINK_COMM_NUM_BUFFERS];
#include "mavlink/ida/mavlink.h"

#include "ida/port.h"
#include "unity.h"

#include <string.h>

void setUp(void);
void tearDown(void);
void setUp(void) {}
void tearDown(void) {}

static ida_link_gonderici_t g;
static ida_link_alici_t al;
static ida_link_cerceve_t c;
static uint8_t tampon[IDA_LINK_CERCEVE_AZAMI];

static void coz(size_t n, uint32_t msgid)
{
    TEST_ASSERT_EQUAL_INT(IDA_LINK_TAMAM, ida_link_alici_kur(&al, IDA_LINK_ROL_SIM));
    size_t t = 0u;
    TEST_ASSERT_EQUAL_INT(IDA_LINK_CERCEVE, ida_link_al(&al, tampon, n, &t, &c));
    TEST_ASSERT_EQUAL_UINT(n, t);
    TEST_ASSERT_EQUAL_UINT32(msgid, c.msgid);
    TEST_ASSERT_EQUAL_UINT8(IDA_LINK_KART_SYSID, c.sysid);
    TEST_ASSERT_EQUAL_UINT8(IDA_LINK_KART_COMPID, c.compid);
}

static void test_link_stats(void)
{
    ida_link_sayaclar_t tel;
    memset(&tel, 0, sizeof tel);
    tel.rx_bytes = 1000u;
    tel.rx_frames = 20u;
    tel.rx_crc_err = 2u;
    tel.rx_unknown_msgid = 3u;
    tel.rx_iflag = 4u;
    tel.rx_bad_len = 5u;
    tel.rx_wrong_sys = 6u;
    tel.rx_mav1 = 7u;
    tel.rx_overflow = 8u;
    tel.rx_resync_bytes = 9u;
    tel.rx_seq_gap = 10u;
    ida_fw_plat_sayac_t p = {28u, 29u, 30u, 31u, 32u, 13u};
    TEST_ASSERT_EQUAL_INT(IDA_LINK_TAMAM, ida_link_gonderici_kur(&g, IDA_LINK_ROL_KART));
    size_t n = 0u;
    TEST_ASSERT_EQUAL_INT(IDA_LINK_TAMAM,
                          ida_fw_link_stats_kodla(&g, &tel, &p, tampon, sizeof tampon, &n));
    coz(n, MAVLINK_MSG_ID_IDA_LINK_STATS);
    mavlink_ida_link_stats_t m;
    TEST_ASSERT_EQUAL_INT(IDA_LINK_TAMAM,
                          ida_link_coz(&c, MAVLINK_MSG_ID_IDA_LINK_STATS, &m, sizeof m));
    const uint32_t beklenen[IDA_FW_LINK_STATS_N] = {
        1000u, 20u, 2u, 3u, 4u, 5u, 6u, 7u, 8u, 9u, 10u, 0u, 0u, 13u, 0u, 0u, 0u, 0u,
        0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 0u, 28u, 29u, 30u, 31u, 32u, 0u, 0u, 0u};
    TEST_ASSERT_EQUAL_UINT32_ARRAY(beklenen, m.cnt, IDA_FW_LINK_STATS_N);
}

static void test_duyuru(void)
{
    ida_fw_duyuru_t d;
    memset(&d, 0, sizeof d);
    d.param_schema_hash = UINT64_C(0x1122334455667788);
    d.nonce = 0xCAFEF00Du;
    d.reset_flags = 0x24000000u;
    d.core_version = 0x000100u;
    d.fw_version = 0x000100u;
    d.fw_git = 0x920B7175u;
    d.reset_cause = (uint8_t)IDA_RESET_WATCHDOG;
    d.consecutive_faults = 1u;
    d.init_rc = (uint8_t)IDA_RC_PARAM_DEFAULTS;
    TEST_ASSERT_EQUAL_INT(IDA_LINK_TAMAM, ida_link_gonderici_kur(&g, IDA_LINK_ROL_KART));
    size_t n = 0u;
    TEST_ASSERT_EQUAL_INT(IDA_LINK_TAMAM, ida_fw_duyuru_kodla(&g, &d, tampon, sizeof tampon, &n));
    coz(n, MAVLINK_MSG_ID_IDA_BOARD_HELLO);
    mavlink_ida_board_hello_t m;
    TEST_ASSERT_EQUAL_INT(IDA_LINK_TAMAM,
                          ida_link_coz(&c, MAVLINK_MSG_ID_IDA_BOARD_HELLO, &m, sizeof m));
    TEST_ASSERT_TRUE(m.dialect_hash == IDA_LINK_DIALECT_HASH);
    TEST_ASSERT_TRUE(m.param_schema_hash == UINT64_C(0x1122334455667788));
    TEST_ASSERT_EQUAL_UINT32(0u, m.peer_nonce);
    TEST_ASSERT_EQUAL_HEX32(0xCAFEF00Du, m.nonce);
    TEST_ASSERT_EQUAL_HEX32(0x24000000u, m.reset_flags);
    TEST_ASSERT_EQUAL_UINT32(5000000u, m.ctrl_period_ns);
    TEST_ASSERT_EQUAL_UINT16(IDA_PORT_VERSION_MAJOR, m.port_major);
    TEST_ASSERT_EQUAL_UINT8(1u, m.proto_major);
    TEST_ASSERT_EQUAL_UINT8(0u, m.proto_minor);
    TEST_ASSERT_EQUAL_UINT8(IDAW_ROLE_BOARD, m.role);
    TEST_ASSERT_EQUAL_UINT8(IDA_RESET_WATCHDOG, m.reset_cause);
    TEST_ASSERT_EQUAL_UINT8(1u, m.consecutive_faults);
    TEST_ASSERT_EQUAL_UINT8(0u, m.boot_count_ok);
    TEST_ASSERT_EQUAL_UINT8(IDA_RC_PARAM_DEFAULTS, m.init_rc);
    TEST_ASSERT_EQUAL_HEX32(0x920B7175u, m.fw_git);
}

static void test_bos_gosterici(void)
{
    size_t n = 0u;
    TEST_ASSERT_EQUAL_INT(IDA_LINK_HATA_ARGUMAN,
                          ida_fw_duyuru_kodla(&g, NULL, tampon, sizeof tampon, &n));
    ida_fw_plat_sayac_t p = {0u, 0u, 0u, 0u, 0u, 0u};
    TEST_ASSERT_EQUAL_INT(IDA_LINK_HATA_ARGUMAN,
                          ida_fw_link_stats_kodla(&g, NULL, &p, tampon, sizeof tampon, &n));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_link_stats);
    RUN_TEST(test_duyuru);
    RUN_TEST(test_bos_gosterici);
    return UNITY_END();
}
