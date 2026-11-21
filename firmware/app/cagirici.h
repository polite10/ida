/*
 * cagirici.h — çekirdeğin çağırıcısı (01 §2.1 "Çağırıcı", §3.1; A4-K16 … K23, K89, K98, K109;
 * W3-kart dilim 2).
 *
 * ida_core_t'nin statik sahibidir (A4-K98; 64 KB'lık alan, çekirdeğin bildirdiği boy ve
 * hizalamayla denetlenir). Girdiyi oturum modülü (ida_link_ust_take) kurar; çağırıcı platformun
 * çağrı alanlarını (bg_slices, plat.prev_call_ns, plat.prev_overrun) yazar, ida_core_step ya da
 * ida_core_tick'i çağırır ve süresini DWT ile ölçer (A4-K23). Güvenli beklemede çekirdek
 * çağrılmaz.
 */
#ifndef IDA_FW_CAGIRICI_H
#define IDA_FW_CAGIRICI_H

#include <stdbool.h>
#include <stdint.h>

#include "acilis_kaydi.h"
#include "ida/port.h"
#include "istatistik.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Platform durumu (01 §2.8). */
#define IDA_CAG_BASLATMA 0u
#define IDA_CAG_NORMAL   1u
#define IDA_CAG_GUVENLI  2u /* platform güvenli beklemesi: çekirdek çağrılmaz */

#define IDA_CAG_NEDEN_YOK      0u
#define IDA_CAG_NEDEN_BOYUT    1u /* çekirdek statik alana sığmıyor ya da port sürümü (A4-K98) */
#define IDA_CAG_NEDEN_ARIZA    2u /* ardışık arıza sayacı >= 3 (01 §2.8)                       */
#define IDA_CAG_NEDEN_INIT     3u /* ida_core_init kurulamadı (VERSION, BAD_INPUT)              */
#define IDA_CAG_NEDEN_SAAT     4u /* saat kurulamadı                                           */

typedef struct {
    uint8_t durum;
    uint8_t neden;
    uint8_t init_rc;
    bool core_ok;              /* son çağrının health.core_ok'u (A4-K89; mandallı) */
    ida_core_info_t bilgi;
    ida_ist_t step;            /* DWT çevrimi */
    ida_ist_t tick;
    ida_ist_t arka_plan;
    uint32_t step_butce_asimi; /* > IDA_PORT_STEP_BUDGET_NS */
    uint32_t tick_butce_asimi; /* > IDA_PORT_TICK_BUDGET_NS */
    uint32_t rc_hata;          /* OK dışı dönüş */
    uint8_t son_rc;
    uint32_t bg_more;
    uint64_t son_cagri_us;     /* son çekirdek çağrısının başı (duvar) */
} ida_cagirici_t;

typedef struct {
    const ida_acilis_sonuc_t *acilis;
    bool saat_tamam;
    uint64_t simdi_us;
    uint32_t boot_count;
    bool boot_count_ok;
    uint8_t param_src;          /* IDA_PSRC_* */
    const uint8_t *param_blob;  /* NULL: blok yok */
    uint32_t param_len;
} ida_cagirici_kurulum_t;

void ida_cagirici_kur(const ida_cagirici_kurulum_t *k);
/* Çekirdek çağrısı (yalnız NORMAL'de; değilse IDA_RC_NOT_INIT döner, çekirdek çağrılmaz).
   step: ida_core_step, değilse ida_core_tick. in->bg_slices, plat.prev_call_ns ve
   plat.prev_overrun burada yazılır. */
ida_rc_t ida_cagirici_cagir(bool step, ida_in_t *in, ida_out_t *out, uint64_t simdi_us);
/* Arka plan dilimi (yalnız NORMAL'de). */
void ida_cagirici_arka_plan(uint64_t simdi_us);
const ida_cagirici_t *ida_cagirici_durum(void);

#ifdef __cplusplus
}
#endif

#endif /* IDA_FW_CAGIRICI_H */
