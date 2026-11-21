/* cagirici.c — çekirdeğin çağırıcısı (cagirici.h). */
#include "cagirici.h"

#include <string.h>

#include "bsp.h"
#include <stddef.h>
#include "ida/port.h"

/* Çekirdek örneğinin statik alanı (A4-K98). Platform çekirdeğin iç tipini bilmez; çekirdeğin
   bildirdiği boy ve hizalama (ida_core_info) bu alana sığmıyorsa çekirdek çağrılmaz ve platform
   güvenli beklemeye geçer. Alan cömerttir: çekirdek (görev ve öteki modüllerle) büyüyecek. */
#define IDA_CEKIRDEK_ALAN_BAYT (64u * 1024u)
#define IDA_CEKIRDEK_ALAN_HIZA 16u
static union {
    max_align_t hiza;
    uint8_t b[IDA_CEKIRDEK_ALAN_BAYT];
} cekirdek_alani __attribute__((aligned(IDA_CEKIRDEK_ALAN_HIZA)));
static ida_core_t *const cekirdek = (ida_core_t *)(void *)&cekirdek_alani;
static ida_in_t girdi;
static ida_out_t cikti;
static ida_cagirici_t cag;

static uint32_t bg_dilim; /* önceki step/tick'ten beri IDLE ya da MORE dönen arka plan çağrısı */
static uint32_t onceki_cagri_ns;
static bool onceki_asim;

/* 160 MHz'te çevrim -> ns (6,25 ns/çevrim). */
static uint32_t cevrim_ns(uint32_t cevrim)
{
    const uint64_t ns = ((uint64_t)cevrim * 25u) / 4u;
    return ns > UINT32_MAX ? UINT32_MAX : (uint32_t)ns;
}

static ida_wtime_t duvar(uint64_t us)
{
    ida_wtime_t w;
    w.ns = (int64_t)(us * 1000u);
    return w;
}

void ida_cagirici_kur(const ida_acilis_sonuc_t *acilis, bool saat_tamam, uint64_t simdi_us)
{
    memset(&cag, 0, sizeof cag);
    ida_ist_sifirla(&cag.tick);
    ida_ist_sifirla(&cag.arka_plan);
    cag.durum = IDA_CAG_BASLATMA;
    cag.init_rc = (uint8_t)IDA_RC_NOT_INIT;
    cag.core_ok = true;

    ida_core_info_t bilgi;
    memset(&bilgi, 0, sizeof bilgi);
    ida_core_info(&bilgi);
    cag.core_size = bilgi.core_size;
    cag.core_version = bilgi.core_version;
    cag.param_schema_hash = bilgi.param_schema_hash;

    if (!saat_tamam) {
        cag.durum = IDA_CAG_GUVENLI;
        cag.neden = IDA_CAG_NEDEN_SAAT;
        return;
    }
    if (bilgi.core_size == 0u || bilgi.core_size > IDA_CEKIRDEK_ALAN_BAYT ||
        bilgi.core_align == 0u || IDA_CEKIRDEK_ALAN_HIZA % bilgi.core_align != 0u ||
        bilgi.port_major != IDA_PORT_VERSION_MAJOR || bilgi.port_minor != IDA_PORT_VERSION_MINOR) {
        cag.durum = IDA_CAG_GUVENLI;
        cag.neden = IDA_CAG_NEDEN_BOYUT;
        return;
    }
    if (acilis->ardisik_ariza >= 3u) {
        cag.durum = IDA_CAG_GUVENLI;
        cag.neden = IDA_CAG_NEDEN_ARIZA;
        return;
    }

    ida_init_in_t in;
    memset(&in, 0, sizeof in);
    in.port_major = (uint16_t)IDA_PORT_VERSION_MAJOR;
    in.port_minor = (uint16_t)IDA_PORT_VERSION_MINOR;
    in.now_w = duvar(simdi_us);
    /* Kalıcı açılış sayacı dilim 2'dedir: sayaç ayrılamadı (A4-K86; olay üretimi kapalı). */
    in.boot_count = 0u;
    in.boot_count_ok = false;
    in.reset_cause = acilis->reset_cause;
    in.reset_flags = acilis->reset_flags;
    in.consecutive_faults = acilis->ardisik_ariza;
    in.param_src = (uint8_t)IDA_PSRC_NONE;
    in.param_blob = NULL;
    in.param_len = 0u;
    const ida_rc_t rc = ida_core_init(cekirdek, &in);
    cag.init_rc = (uint8_t)rc;
    if (rc == IDA_RC_OK || rc == IDA_RC_PARAM_DEFAULTS) {
        cag.durum = IDA_CAG_NORMAL;
        cag.son_cagri_us = simdi_us;
    } else {
        cag.durum = IDA_CAG_GUVENLI;
        cag.neden = IDA_CAG_NEDEN_INIT;
    }
}

void ida_cagirici_tick(uint64_t simdi_us, const ida_cagirici_plat_t *plat)
{
    if (cag.durum != IDA_CAG_NORMAL) {
        return;
    }
    memset(&girdi, 0, sizeof girdi);
    girdi.now_w = duvar(simdi_us);
    girdi.tf.present = false; /* ızgara tick'i (A4-K18) */
    girdi.imu_present = false;
    girdi.ping360 = NULL;
    girdi.bg_slices = bg_dilim;
    girdi.plat.link_state = (uint8_t)IDA_LINK_NONE;
    girdi.plat.link_event = (uint8_t)IDA_LINKEV_NONE;
    girdi.plat.param_store = (uint8_t)IDA_PSTORE_IDLE;
    girdi.plat.param_fail = (uint8_t)IDA_PSFAIL_NONE;
    girdi.plat.prev_overrun = onceki_asim;
    girdi.plat.prev_call_ns = onceki_cagri_ns;
    girdi.plat.cnt_grid_missed = plat->cnt_grid_missed;
    girdi.plat.cnt_rx_error = plat->cnt_rx_error;

    cag.son_cagri_us = simdi_us;
    const uint32_t c0 = ida_bsp_cevrim();
    const ida_rc_t rc = ida_core_tick(cekirdek, &girdi, &cikti);
    const uint32_t c = ida_bsp_cevrim() - c0;
    ida_bsp_adim_sayaci = ida_bsp_adim_sayaci + 1u;
    bg_dilim = 0u;

    ida_ist_ekle(&cag.tick, c);
    onceki_cagri_ns = cevrim_ns(c);
    onceki_asim = onceki_cagri_ns > IDA_PORT_TICK_BUDGET_NS;
    if (onceki_asim) {
        cag.tick_butce_asimi++;
    }
    cag.tick_son_rc = (uint8_t)rc;
    if (rc != IDA_RC_OK) {
        cag.tick_rc_hata++;
    }
    /* A4-K89: core_ok = 0 mandallıdır; platform onu gördüğü sürece IWDG'yi beslemez. */
    cag.core_ok = cikti.health.core_ok;
}

void ida_cagirici_arka_plan(uint64_t simdi_us)
{
    if (cag.durum != IDA_CAG_NORMAL) {
        return;
    }
    const uint32_t c0 = ida_bsp_cevrim();
    const ida_bg_rc_t rc = ida_core_background(cekirdek, duvar(simdi_us));
    const uint32_t c = ida_bsp_cevrim() - c0;
    ida_ist_ekle(&cag.arka_plan, c);
    if (rc == IDA_BG_IDLE || rc == IDA_BG_MORE) {
        bg_dilim++;
    }
    if (rc == IDA_BG_MORE) {
        cag.bg_more++;
    }
}

const ida_cagirici_t *ida_cagirici_durum(void)
{
    return &cag;
}
