/*
 * cekirdek.h — ida_core_t'nin tam tanımı: modül durumlarının bileşimi (W2; A4-K22, K98).
 *
 * Bütün çekirdek durumu buradadır; çekirdekte değişebilir küresel ya da statik nesne yoktur
 * (A4-K22). Her modülün durum tipi kendi başlığındadır; bir modülün durumu büyüdüğünde yalnız
 * o başlık değişir. Koruma sözcükleri iki uçtadır; kurulu işareti ve tümleyeni init'in
 * başarısını kaydeder (A4-K89, K126).
 */
#ifndef IDA_CORE_CEKIRDEK_H
#define IDA_CORE_CEKIRDEK_H

#include <stdint.h>

#include "atis/atis.h"
#include "cerceve/cerceve.h"
#include "cerceve/imu_zaman.h"
#include "cerceve/ornek.h"
#include "emniyet/emniyet.h"
#include "gorev/gorev.h"
#include "gudum/gudum.h"
#include "ida/port.h"
#include "kestirim/kestirim.h"
#include "kontrol/kontrol.h"
#include "olay/olay.h"
#include "param/param.h"
#include "sonar/sonar.h"
#include "telemetri/telemetri.h"

/* Çekirdek uygulamasının sürümü (ida_core_info.core_version): ana << 16 | ara << 8 | yama. */
#define IDA_CORE_VERSION 0x000100u /* 0.1.0: W2 iskeleti */

#define IDA_KORUMA_BAS     0x49444142u /* "IDAB" */
#define IDA_KORUMA_SON     0x49444153u /* "IDAS" */
#define IDA_KURULU_ISARETI 0x4B55524Cu /* "KURL" */

struct ida_core {
    uint32_t koruma_bas;
    uint32_t kurulu;          /* IDA_KURULU_ISARETI: kurulu; başka her değer kurulmamış  */
    uint32_t kurulu_tumleyen; /* ~kurulu (A4-K89 tamamlayıcı kopya)                      */
    ida_cerceve_t cer;
    ida_emn_t emn;
    ida_olay_t olay;
    ida_ornek_t orn;
    ida_imz_t imz;
    ida_param_t prm;
    ida_gunluk_t gun;
    ida_kes_t kes;
    ida_gud_t gud;
    ida_gor_t gor;
    ida_kon_t kon;
    ida_fc_t fc;
    ida_son_t son;
    uint32_t koruma_son;
};

_Static_assert(_Alignof(ida_core_t) >= _Alignof(int64_t), "ida_core_t hizalaması");

#endif /* IDA_CORE_CEKIRDEK_H */
