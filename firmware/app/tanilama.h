/*
 * tanilama.h — kartın A1 tanılama ve duyuru çerçevelerinin kodlanması (W3-kart dilim 1).
 *
 * İki A1 mesajı (protocol/ida.xml; A1 §3.7, Tablo 3.11b, A1-K11):
 *  - IDA_LINK_STATS (46068): Tablo 3.11b'nin 36 sayacı, 1 Hz. Dilim 1'de oturum yoktur; bu
 *    yayın yalnız bu dilimin duman testine özgüdür (görev belgesi W3-kart md.5) ve A1-K11'in
 *    "IDLE'da yalnız BOARD_HELLO" kuralından sapar; dilim 2'de oturum kurallarına bağlanır.
 *  - IDA_BOARD_HELLO (46001): IDLE duyurusu (peer_nonce = 0), T_duyuru = 200 ms (A1-K11);
 *    açılış kaydını taşır (sıfırlama nedeni, ham bayraklar, ardışık arıza sayacı, init sonucu).
 *
 * Kodlama yalnız tel katmanının kodlayıcısıyladır (ida_link_gonder). Donanımdan bağımsızdır;
 * PC'de birim testiyle sınanır.
 */
#ifndef IDA_FW_TANILAMA_H
#define IDA_FW_TANILAMA_H

#include <stddef.h>
#include <stdint.h>

#include "ida_link/tel.h"

#ifdef __cplusplus
extern "C" {
#endif

#define IDA_FW_LINK_STATS_N 36u

/* Tablo 3.11b'nin platform ve gönderim sayaçları (tel katmanınınkiler ida_link_sayaclar_t'de).
   Dilim 1'de oturum, parti, komut ve olay yoktur; onların sayaçları 0 gönderilir. */
typedef struct {
    uint32_t grid_missed;   /* #28: kaçırılan 10 ms ızgara anı (plat.cnt_grid_missed) */
    uint32_t out_fixed;     /* #29                                                     */
    uint32_t tx_frames;     /* #30: taşımaya verilen çerçeve                           */
    uint32_t tx_dropped;    /* #31: yer olmadığı için atılan kaybedilebilir çerçeve    */
    uint32_t tx_crit_stall; /* #32                                                     */
    uint32_t stamp_coarse;  /* #13: kaba alım damgalı çerçeve (A1-K48)                 */
} ida_fw_plat_sayac_t;

/* BOARD_HELLO duyurusunun içeriği. Dilim 1'de kalıcı bellek yoktur: boot_count 0 ve
   boot_count_ok 0 (A4-K86'nın "ayrılamadı" durumu), parametre bloğu yok. */
typedef struct {
    uint64_t param_schema_hash; /* ida_core_info                                   */
    uint32_t nonce;             /* açılışta gerçek rastgelelikten (0 olamaz)       */
    uint32_t boot_count;
    uint32_t reset_flags;       /* ham RCC_CSR bayrakları                          */
    uint32_t core_version;      /* ida_core_info                                   */
    uint32_t fw_version;
    uint32_t fw_git;
    uint8_t reset_cause;        /* A4 IDA_RESET_*                                  */
    uint8_t consecutive_faults;
    uint8_t boot_count_ok;
    uint8_t init_rc;            /* ida_core_init dönüşü; çağrılmadıysa NOT_INIT     */
} ida_fw_duyuru_t;

/* Kart tarafı Tablo 3.11b dizisini kurar (sıra sözleşmededir). */
void ida_fw_link_stats_dizisi(const ida_link_sayaclar_t *tel, const ida_fw_plat_sayac_t *plat,
                              uint32_t cnt[IDA_FW_LINK_STATS_N]);

/* IDA_LINK_STATS çerçevesini kodlar (ida_link_gonder'in dönüşleri). */
ida_link_rc_t ida_fw_link_stats_kodla(ida_link_gonderici_t *g, const ida_link_sayaclar_t *tel,
                                      const ida_fw_plat_sayac_t *plat, uint8_t *cikis,
                                      size_t kapasite, size_t *yazilan);

/* IDA_BOARD_HELLO duyurusunu (peer_nonce = 0) kodlar. */
ida_link_rc_t ida_fw_duyuru_kodla(ida_link_gonderici_t *g, const ida_fw_duyuru_t *d,
                                  uint8_t *cikis, size_t kapasite, size_t *yazilan);

#ifdef __cplusplus
}
#endif

#endif /* IDA_FW_TANILAMA_H */
