/*
 * emniyet.h — etkin emniyet değerleri ve daraltma: A4-K113'ün (b) adımı (W2).
 *
 * Bu modül yalnız KISITLAYAN yönde geçiş tanımlar: etkin emniyet değerlerini kısıtlayana,
 * `armed`'ı 0'a götürür, kipi daraltıcı hedefe çevirir. Gevşetme (A4-K116'nın gevşeten yönü,
 * A4-K34'ün değer güncellemesi, inst0 alanları) atış kontrol işinin (W4) dosyasıdır; o kod
 * ida_fc_emniyet_ornegi kancasından bu yapının alanlarına yazar.
 *
 * Alan bitleri (kisitli, cagri_kisit, kapanan): 1 = o alanın değeri kısıtlayan.
 * `armed` ve `mode` bağımsızdır (A4-K127); genişletme yönünde yalnız görev/kip modülü (W3)
 * yazar.
 */
#ifndef IDA_CORE_EMNIYET_H
#define IDA_CORE_EMNIYET_H

#include <stdbool.h>
#include <stdint.h>

#include "ida/port.h"
#include "telemetri/telemetri.h"

#define IDA_EMN_ESTOP  0x01u /* acil durdurma basılı      */
#define IDA_EMN_FIS_T0 0x02u /* tüp 0 fişi çıkık          */
#define IDA_EMN_FIS_T1 0x04u /* tüp 1 fişi çıkık          */
#define IDA_EMN_PIM_T0 0x08u /* tüp 0 pimi takılı         */
#define IDA_EMN_PIM_T1 0x10u /* tüp 1 pimi takılı         */
#define IDA_EMN_TUMU   0x1Fu

/* SAFETY_CLOSE isteği (olay modülüne): A4-K125 birleştirmesinin girdisi. */
#define IDA_KAPAT_T0  0x01u
#define IDA_KAPAT_T1  0x02u
#define IDA_KAPAT_ALL 0x04u

/* Bir çağrının (b) adımında uygulanan daraltmanın kaydı; her çağrının başında sıfırlanır. */
typedef struct {
    bool herhangi;        /* daraltıcı girdi uygulandı: health.narrowed ve A4-K45 tetiği */
    bool disarm;          /* DISARM mandalı / cmds[]'te DISARM ya da SESSION_END           */
    bool kip_izni_dus;    /* NARROW_MODE (geçerli ya da geçersiz hedef; A4-K46, K127)      */
    bool fc_disarm;       /* bütün tüplere (A4-K46)                                        */
    bool fc_abort;        /* bütün tüplere                                                 */
    bool oturum_sonu;     /* plat.link_event = SESSION_END (A4-K73)                         */
    uint8_t kapanan;      /* gevşetenden kısıtlayana dönen alanlar (A4-K111; oturum sonu dahil) */
    uint8_t ornek_kapanan;/* bunlardan emniyet örneğinin içeriğiyle dönenler                */
    uint8_t kapat;        /* IDA_KAPAT_* (A4-K73, K111)                                    */
} ida_daralt_t;

typedef struct {
    uint8_t kisitli;      /* etkin değerler (A4-K116); init'te IDA_EMN_TUMU               */
    bool armed;           /* motor silahlandırması                                        */
    uint8_t mode;         /* IDA_MODE_*                                                   */
    bool inst0_var;       /* son SESSION_END ya da init'ten beri kabul edilmiş inst=0 örneği
                             (A4-K128; W4'ün gevşetme kodu kurar)                          */
    uint8_t inst0_tube[IDA_PORT_N_TUBE]; /* o örneğin tüp bitleri (LOADED, PERMIT_OPEN)    */
    uint8_t cagri_kisit;  /* bu çağrıda yapısal geçerli bir örnekte kısıtlayıcı içerik görülen
                             alanlar (W4'ün gevşetmesi bu alanları gevşetemez)             */
    uint8_t kabul_kisit;  /* bunlardan kabul edilen bir örnekte de görülenler              */
    bool oturum_kapali;   /* SESSION_END işlendi, henüz SESSION_START gelmedi: sonraki
                             SESSION_END yeni kapanma olayı üretmez (A4-K73)               */
    ida_daralt_t d;       /* bu çağrının daraltma kaydı                                    */
} ida_emn_t;

typedef struct {
    ida_gunluk_t *gun;
    ida_wtime_t now;      /* kullanılan now_w (günlük zamanı)                              */
} ida_emn_baglam_t;

/* Init: bütün alanlar kısıtlayan, armed 0, mode INIT (A4-K77, K116, K128). */
void ida_emn_kur(ida_emn_t *e);
/* SESSION_START (3.11 (b3)): yalnız A4-K73'ün "arada SESSION_START" izini siler; etkin
   emniyet değerlerine, armed'a ve kipe dokunmaz. */
void ida_emn_oturum_basi(ida_emn_t *e);

/* (b) adımı (A4-K113): in->narrow'un tanımlı bitleri; cmds[]'in ilk min(n_cmds, 8)
   girdisindeki daraltıcı komutlar; SESSION_END; samples[]'in ilk min(n_samples, 16)
   girdisindeki yapısal geçerli (type SAFETY, inst < 4) her örneğin kısıtlayıcı içeriği.
   e->d'yi doldurur; yalnız kısıtlayan yönde yazar, yinelenince ek etki üretmez.
   MODE_CHANGE, ARM_CHANGE, SAFETY_INPUT günlüklerini üretir. */
void ida_emn_daralt(ida_emn_t *e, const ida_in_t *in, const ida_emn_baglam_t *b);

/* (e): kabul edilen bir SAFETY örneğinin kısıtlayıcı içeriğini işaretler (narrow_from_rejected
   ayrımı için; değer güncellemez). */
void ida_emn_kabul_kisit(ida_emn_t *e, const ida_safety_t *s);

/* Çağrı sonu: örnek içeriğiyle olan bir kapanmaya yalnız reddedilen örnekler yol açtıysa
   true (status.cnt.narrow_from_rejected bir artar; A4-K116). */
bool ida_emn_cagri_sonu(const ida_emn_t *e);

/* Örneğin kısıtlayıcı içeriği (alan bitleri). */
uint8_t ida_emn_kisit_icerigi(const ida_safety_t *s);

/* Tüpün etkin emniyet değerlerinden biri kısıtlayan mı (acil durdurma iki tüpü kapsar). */
bool ida_emn_tup_kisitli(const ida_emn_t *e, uint8_t tup);

/* IDA_FCI_ESTOP, PLUG_T0/T1, PIN_T0/T1, EMPTY_T0/T1 (A4-K116, K128). */
uint32_t ida_emn_fci(const ida_emn_t *e);

#endif /* IDA_CORE_EMNIYET_H */
