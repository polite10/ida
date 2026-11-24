/*
 * atis_ic.h — atış kontrol modülünün iç yardımcıları (yalnız atis/ altındaki .c dosyaları). Sahibi:
 * W5-atis-emniyet.
 */
#ifndef IDA_CORE_ATIS_IC_H
#define IDA_CORE_ATIS_IC_H

#include <stdbool.h>
#include <stdint.h>

#include "atis/atis.h"

/* İç durumu ve tümleyenini yazar; giriş anını kaydeder. */
void ida_fc_ic_yaz(ida_fc_t *fc, uint32_t ic, int64_t now_ns);
/* Silahlı tüpü ve tümleyenini yazar. */
void ida_fc_tup_yaz(ida_fc_t *fc, uint8_t tup);
/* Tüpün SAFETY_CLOSE isteği (IDA_KAPAT_T0/T1; IDA_FC_TUP_YOK için hiçbir şey) ve çekirdeğin o
   tüp için saydığı iznin kapanması. */
void ida_fc_kapat_tup(ida_fc_t *fc, uint8_t tup);
/* IDA_KAPAT_* isteği: kapsanan tüplerin izni kapalı sayılır. */
void ida_fc_kapat(ida_fc_t *fc, uint8_t kapat);
/* Güvenli duruma dönüş (W5-K8): silahlandırma ve izinler kaldırılır (silahlı tüpe
   SAFETY_CLOSE), FAULT_LATCHED dışındaki durum SAFE olur; FAULT_LATCHED korunur. */
void ida_fc_guvenli(ida_fc_t *fc, int64_t now_ns);
/* Kilitli arıza: kapat'taki ve silahlı tüpün SAFETY_CLOSE'u istenir, durum FAULT_LATCHED
   olur, bu çağrının kilit izi kurulur (W5-K1: FC_RESET'ten sonra yeniden kilitlenme). */
void ida_fc_kilitle(ida_fc_t *fc, uint8_t kapat, int64_t now_ns);
/* Çekirdeğin saatinde tüpün izni açık mı (A4-K66, K107; G-Ç11). */
bool ida_fc_izin_acik(const ida_fc_t *fc, uint8_t tup, int64_t now_ns);
/* İç durumdan A4 §3.16 değeri (03 §2.6.1 md. 1). */
uint8_t ida_fc_dis_durum(const ida_fc_t *fc, int64_t now_ns);
/* Emniyet örneği bayat mı (W5-K4 ve eki; G-Ç29): inst = 0 kabul edilmişse iki ölçüt. */
bool ida_fc_emn_bayat(const ida_fc_t *fc, const ida_emn_t *e, const ida_fc_ortam_t *o);
/* İç durum, silahlı tüp ve bayrakların geçerliliği (dış durum yansıması hariç; A4-K89). */
bool ida_fc_ic_tutarli(const ida_fc_t *fc);
/* Silahlı (ARMED … ASSESS) ya da TEST iç durumunda mı. */
bool ida_fc_etkin(const ida_fc_t *fc);
/* Evrensel geçişler (03 §2.6; G-Ç19, G-Ç25; W5-K8, K11 b). */
void ida_fc_evrensel(ida_fc_t *fc, const ida_fc_baglam_t *b, const ida_fc_ortam_t *o);
/* SAFETY_INPUT günlüğünün tüp bitleri (A4 §3.15 satır 19). */
int32_t ida_fc_gunluk_tup_bitleri(uint8_t kisitli);
/* Tüpün emniyet bitleri (PLUG_T*, PIN_T*, EMPTY_T*). */
uint32_t ida_fc_tup_bitleri(uint8_t tup);

#endif /* IDA_CORE_ATIS_IC_H */
