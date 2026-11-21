/*
 * kapanis.h — platformun güvenli kapanış kararı (A1-K22, A1-K110, A1-K13; W3-kart dilim 2
 * düzeltmesi, sol bulgu 1 ve 4).
 *
 * Donanımdan bağımsızdır (ISO C17; PC'de birim ve bütünleşik testle sınanır). Ana döngü her
 * turda, çağrı penceresi dışında çağırır. Şu durumlardan biri platformun güvenli beklemesine
 * girmeyi gerektirir (A1-K22 "platform güvenli beklemeye girdi"; EMNİYET: kısıtlayan okuma):
 *   - kritik bir çerçeve (IDA_ACTUATOR ya da çağrının yeni olayı) gönderim kuyruğuna
 *     alınamadı: kritik çıktının kaybı olağan kuyruk davranışı değildir (A1-K110);
 *   - gerçek rastgele sayı üretilemiyor: sabit nonce ile el sıkışma sürdürülmez (A1-K13).
 * RNG nedeninde gönderim kuyruğunda yazımı başlamamış çerçeveler silinir (sabit nonce taşıyan bir
 * BOARD_HELLO kuyrukta kalmasın). Karar verilince oturum modülü güvenli beklemeye alınır (oturum
 * varsa SESSION_CLOSE(SAFE_STATE) denenir ve oturum biter; sonra SIM_HELLO yanıtlanmaz, duyurular
 * init_rc = NOT_INIT taşır).
 * Geri alınmaz; çekirdeğin çağrılmaması platformun işidir (neden döner).
 */
#ifndef IDA_FW_KAPANIS_H
#define IDA_FW_KAPANIS_H

#include <stdbool.h>
#include <stdint.h>

#include "gonderim.h"
#include "ida_link/ust.h"

#ifdef __cplusplus
extern "C" {
#endif

#define IDA_KAPANIS_YOK    0u
#define IDA_KAPANIS_KRITIK 1u /* kritik çerçeve kuyruğa alınamadı (A1-K110) */
#define IDA_KAPANIS_RNG    2u /* gerçek rastgele sayı üretilemiyor (A1-K13) */

typedef struct {
    uint8_t neden; /* ilk nedeni; IDA_KAPANIS_YOK: kapanış yok */
} ida_kapanis_t;

void ida_kapanis_kur(ida_kapanis_t *k);
/* Kapanış gerekiyorsa (ve çağrı penceresi kapalıysa) oturum modülünü güvenli beklemeye alır.
   Dönüş: bu çağrıda başlayan kapanışın nedeni; başlamadıysa IDA_KAPANIS_YOK. */
uint8_t ida_kapanis_denetle(ida_kapanis_t *k, ida_gonderim_t *g, bool rng_hata,
                            ida_link_ust_t *ust);

#ifdef __cplusplus
}
#endif

#endif /* IDA_FW_KAPANIS_H */
