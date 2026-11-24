/*
 * kapanis.h — platformun güvenli kapanışı ve çekirdek çağrı kapısı (A1-K22, A1-K107, A1-K110,
 * A1-K13, A4-K72, A4-K73; W3-kart dilim 2 düzeltmesi, sol bulgu 1 ve 4, ikinci denetim).
 *
 * Donanımdan bağımsızdır (ISO C17; PC'de birim ve bütünleşik testle sınanır). Şu durumlar
 * platformun güvenli beklemesine girmeyi gerektirir (A1-K22 "platform güvenli beklemeye girdi";
 * EMNİYET: kısıtlayan okuma):
 *   - kritik bir çerçeve (IDA_ACTUATOR ya da çağrının yeni olayı) gönderim kuyruğuna
 *     alınamadı: kritik çıktının kaybı olağan kuyruk davranışı değildir (A1-K110);
 *   - gerçek rastgele sayı üretilemiyor: sabit nonce ile el sıkışma sürdürülmez (A1-K13).
 *
 * Kapanış sırası (EMNİYET):
 *   1. Karar anında (çağrı penceresi dışında) oturum modülü güvenli beklemeye alınır: yeni
 *      girdiler hemen engellenir (parti, komut, el sıkışma kabul edilmez); hata mandalı korunur.
 *      RNG nedeninde kuyrukta yazımı başlamamış çerçeveler silinir (sabit nonce'lu duyuru
 *      gitmesin).
 *   2. Oturum sürüyorduysa oturum modülü oturum sonu çağrısını (A1-K107; link_event =
 *      SESSION_END) verir; bu çağrı çalışabilir çekirdeğe tam olarak teslim edilir: çekirdek
 *      silahsızlanır ve izinler kapanır (A4-K72, K73). Bu adım gönderimin başarısına bağlı
 *      değildir (çıktının gidip gitmemesi önemsizdir).
 *   3. Ancak bundan sonra kalıcı çağrı yasağı (güvenli bekleme) başlar; geri alınmaz.
 * Çekirdek çalıştırılamıyorsa (çağırıcı zaten güvenli beklemede: başlatılamadı, boyut ya da
 * sürüm uyuşmuyor, ardışık arıza) 2. adım atlanır: o çekirdek hiç çağrılmamıştır ya da
 * çağrılamaz, dolayısıyla silahlı bir durumu yoktur (silahlanma yalnız çağrıyla olur).
 * Oturum yoksa (el sıkışma, IDLE) oturum sonu çağrısı yoktur; çekirdek o durumda zaten
 * oturumsuz, dolayısıyla daraltılmıştır (A4-K73'ün oturum sonu çağrısı önceki oturumun
 * sonunda yapılmıştır) ve yasak hemen başlar.
 */
#ifndef IDA_FW_KAPANIS_H
#define IDA_FW_KAPANIS_H

#include <stdbool.h>
#include <stdint.h>

#include "gonderim.h"
#include "ida/port.h"
#include "ida_link/ust.h"

#ifdef __cplusplus
extern "C" {
#endif

#define IDA_KAPANIS_YOK    0u
#define IDA_KAPANIS_KRITIK 1u /* kritik çerçeve kuyruğa alınamadı (A1-K110) */
#define IDA_KAPANIS_RNG    2u /* gerçek rastgele sayı üretilemiyor (A1-K13) */

#define IDA_KAPANIS_EVRE_YOK         0u /* olağan */
#define IDA_KAPANIS_EVRE_OTURUM_SONU 1u /* girdiler engelli; oturum sonu çağrısı bekleniyor */
#define IDA_KAPANIS_EVRE_YASAK       2u /* kalıcı çağrı yasağı (güvenli bekleme) */

typedef struct {
    uint8_t neden; /* ilk nedeni; IDA_KAPANIS_YOK: kapanış yok */
    uint8_t evre;  /* IDA_KAPANIS_EVRE_* */
    uint32_t oturum_sonu_teslim; /* çekirdeğe teslim edilen oturum sonu çağrısı (tanı) */
} ida_kapanis_t;

/* Çekirdek çağrısı (platformun: alanları yazar, ida_core_step / ida_core_tick'i çağırır). */
typedef ida_rc_t (*ida_kapanis_cekirdek_f)(void *ctx, ida_link_cagri_t tur, ida_in_t *in,
                                          ida_out_t *out);

typedef struct {
    ida_link_cagri_t tur;   /* take'in verdiği; YOK: çağrı yoktu */
    bool cekirdek_cagrildi; /* out geçerli (put_out'a verildi) */
    ida_rc_t rc;
} ida_kapanis_tur_t;

void ida_kapanis_kur(ida_kapanis_t *k);
/* Kapanış gerekiyorsa (ve çağrı penceresi kapalıysa) 1. adımı yapar. Dönüş: bu çağrıda
   başlayan kapanışın nedeni; başlamadıysa IDA_KAPANIS_YOK. */
uint8_t ida_kapanis_denetle(ida_kapanis_t *k, ida_gonderim_t *g, bool rng_hata,
                            ida_link_ust_t *ust);
/* Bir çağrı turu: take → kapı → (çekirdek) → put_out. cekirdek_calisir: çağırıcı çekirdeği
   çalıştırabilir mi. Kapı: olağanda her çağrı; kapanışın 2. adımında yalnız oturum sonu
   çağrısı (teslim edilince yasak başlar); yasakta hiçbiri (put_out çıktısız). */
ida_kapanis_tur_t ida_kapanis_cagri(ida_kapanis_t *k, ida_link_ust_t *ust, bool cekirdek_calisir,
                                    ida_in_t *in, ida_out_t *out, ida_kapanis_cekirdek_f f,
                                    void *ctx);
/* Kalıcı çağrı yasağı başladı mı (platform güvenli beklemeye geçer). */
bool ida_kapanis_yasak(const ida_kapanis_t *k);

#ifdef __cplusplus
}
#endif

#endif /* IDA_FW_KAPANIS_H */
