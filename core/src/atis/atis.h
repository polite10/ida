/*
 * atis.h — atış kontrol modülünün çağrı çerçevesiyle sınırı. Sahibi: W4 atış kontrol işi.
 *
 * Bu başlık yalnız çerçevenin modülden istediklerini tanımlar: daraltmanın bildirimi, emniyet
 * ve algılama girdileri, olay sonuçları, komutlar, durum ve engel maskesi, adım çağrısı.
 * İç durum makinesi, hedefle ilgili hesap ve fırlatma mantığı bu işin (W2) konusu değildir.
 *
 * Bu sürüm yer tutucudur: durum her zaman IDA_FCS_SAFE; engel maskesi her biti 3.16'daki
 * tanımına göre yer tutucu durumdan hesaplanır, MODE (izin kümesi boş) ve DET_STALE
 * (algılama girdisi okunmaz) her zaman kurulu; hiçbir genişletici olay adayı üretilmez;
 * komut yanıtları tablo 3.17'den.
 */
#ifndef IDA_CORE_ATIS_H
#define IDA_CORE_ATIS_H

#include <stdbool.h>
#include <stdint.h>

#include "cerceve/komut.h"
#include "emniyet/emniyet.h"
#include "ida/port.h"
#include "olay/olay.h"
#include "param/param.h"

typedef struct {
    uint8_t durum; /* IDA_FCS_*; yer tutucuda hep SAFE */
} ida_fc_t;

/* Atış kontrolün engel maskesini kurmak için çerçevenin verdiği çağrı durumu. */
typedef struct {
    bool armed;
    uint8_t mode;
    uint8_t est_level;
    uint8_t fs_level;
    bool events_ok;
    bool imu_lag;
    bool imu_bayat;
    bool oturum_var;          /* plat.link_state = RUNNING                          */
    uint32_t emn_fci;         /* ida_emn_fci(): ESTOP, PLUG_T*, PIN_T*, EMPTY_T*    */
    uint8_t bekleyen_tupler;  /* ida_olay_bekleyen_tupler(): RESULT_PENDING (A4-K123) */
} ida_fc_baglam_t;

void ida_fc_kur(ida_fc_t *fc);
/* (b) adımında, reddedilen ve `out` NULL çağrıda da: FC_DISARM, FC_ABORT, SESSION_END,
   kip izninin düşmesi ve emniyet kapanmaları (A4-K46, K73, K111, K122). */
void ida_fc_daralt(ida_fc_t *fc, const ida_daralt_t *d);
/* (e): kabul edilen her SAFETY örneği; gevşetme (A4-K116, K34, K128) burada, W4'ün
   emniyet/emniyet_gevset.c dosyasıyla yapılır; e->cagri_kisit'teki alanlar gevşetilemez. */
void ida_fc_emniyet_ornegi(ida_fc_t *fc, ida_emn_t *e, const ida_sample_t *s);
/* (e): algılama partileri ve kayıtları (A4-K41, K118). */
void ida_fc_algilama(ida_fc_t *fc, const ida_in_t *in, ida_wtime_t now);
/* Kabul edilen olay sonucu (A4-K69) ve sonucu bilinmeyen olay (A4-K70: güvenli duruma alma). */
void ida_fc_olay_sonucu(ida_fc_t *fc, const ida_olay_bekleyen_t *olay, uint8_t sonuc);
void ida_fc_olay_bilinmiyor(ida_fc_t *fc, const ida_olay_bekleyen_t *olay);
/* fc.inhibit_mask (A4-K91, 3.16) ve fc.state. */
uint32_t ida_fc_engel(const ida_fc_t *fc, const ida_fc_baglam_t *b);
uint8_t ida_fc_durum(const ida_fc_t *fc);
/* FC komutu: K119 biçim denetiminden ve genişletici yasaklarından geçmiş (tablo 3.17). */
void ida_fc_komut(ida_fc_t *fc, const ida_komut_baglam_t *b, const ida_fc_cmd_t *cmd,
                  ida_cmd_result_t *r);
/* Yalnız IDA_RC_OK dönen step'te; genişletici adaylar olay->aday[]'a konur (kapı süzer). */
void ida_fc_adim(ida_fc_t *fc, const ida_fc_baglam_t *b, ida_olay_t *olay);
bool ida_fc_tutarli(const ida_fc_t *fc);
uint8_t ida_fc_param_grup(const ida_prm_kume_t *p);

#endif /* IDA_CORE_ATIS_H */
