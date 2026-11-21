/*
 * cagirici.h — çekirdeğin çağırıcısı (01 §2.1 "Çağırıcı", §3.1; A4-K16 … K23, K98, K109;
 * W3-kart dilim 1).
 *
 * ida_core_t'nin statik sahibidir (A4-K98). Dilim 1'de A1 oturumu yoktur ve çekirdeğe girdi
 * gelmez: çekirdek yalnız 10 ms'lik mutlak ızgarada ida_core_tick ile çağrılır (A4-K18; tf yok,
 * örneksiz, komutsuz) ve her 5 ms'lik periyotta bir arka plan dilimi alır. Her çağrının süresi
 * DWT ile ölçülür; sonraki çağrının plat.prev_call_ns ve plat.prev_overrun alanlarına girer
 * (A4-K23). Çekirdeğin çıktısı bu dilimde A1'e gönderilmez (A1-K11: IDLE'da yalnız duyuru).
 */
#ifndef IDA_FW_CAGIRICI_H
#define IDA_FW_CAGIRICI_H

#include <stdbool.h>
#include <stdint.h>

#include "acilis_kaydi.h"
#include "istatistik.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Platform durumu (01 §2.8). */
#define IDA_CAG_BASLATMA 0u
#define IDA_CAG_NORMAL   1u
#define IDA_CAG_GUVENLI  2u /* platform güvenli beklemesi: çekirdek çağrılmaz */

/* Güvenli beklemenin nedeni. */
#define IDA_CAG_NEDEN_YOK      0u
#define IDA_CAG_NEDEN_BOYUT    1u /* çekirdek statik alana sığmıyor ya da port sürümü uyuşmuyor (A4-K98) */
#define IDA_CAG_NEDEN_ARIZA    2u /* ardışık arıza sayacı >= 3 (01 §2.8)                       */
#define IDA_CAG_NEDEN_INIT     3u /* ida_core_init kurulamadı (VERSION, BAD_INPUT)              */
#define IDA_CAG_NEDEN_SAAT     4u /* saat kurulamadı                                           */

typedef struct {
    uint8_t durum;      /* IDA_CAG_*                                  */
    uint8_t neden;      /* IDA_CAG_NEDEN_*                            */
    uint8_t init_rc;    /* ida_core_init dönüşü; çağrılmadıysa NOT_INIT */
    bool core_ok;       /* son çağrının health.core_ok'u (A4-K89)     */
    uint32_t core_size;
    uint32_t core_version;
    uint64_t param_schema_hash;
    /* süreler (DWT çevrimi) */
    ida_ist_t tick;
    ida_ist_t arka_plan;
    uint32_t tick_rc_hata;  /* OK dışı dönüş                       */
    uint8_t tick_son_rc;
    uint32_t tick_butce_asimi; /* > IDA_PORT_TICK_BUDGET_NS        */
    uint32_t bg_more;       /* IDA_BG_MORE dönüşü                    */
    uint64_t son_cagri_us;  /* son çekirdek çağrısının başı (duvar)  */
} ida_cagirici_t;

/* Platform girdileri (dilim 1'de yalnız sayaçlar). */
typedef struct {
    uint32_t cnt_grid_missed;
    uint32_t cnt_rx_error;
} ida_cagirici_plat_t;

/* Kurulum: çekirdek bilgisini denetler ve init'i yapar. saat_tamam = 0 ise ya da ardışık arıza
   sayacı >= 3 ise çekirdek kurulmaz (güvenli bekleme). */
void ida_cagirici_kur(const ida_acilis_sonuc_t *acilis, bool saat_tamam, uint64_t simdi_us);

/* 10 ms ızgara tick'i (yalnız NORMAL'de çağrı yapar). */
void ida_cagirici_tick(uint64_t simdi_us, const ida_cagirici_plat_t *plat);

/* Arka plan dilimi (yalnız NORMAL'de). */
void ida_cagirici_arka_plan(uint64_t simdi_us);

const ida_cagirici_t *ida_cagirici_durum(void);

#ifdef __cplusplus
}
#endif

#endif /* IDA_FW_CAGIRICI_H */
