/*
 * gudum.h — güdüm: GUIDED kipinin hedefi (GUIDED_SP) ve kontrol isteği. Sahibi: W3 güdüm ve
 * görev işi (W3-zincir: W3-K7, A4-K50, tablo 3.17).
 *
 * GUIDED_SP (A4-K50): ret koşulları ve sırası yalnız 3.17'nin satırıdır — `mask` = 0,
 * tanımsız maske biti (0x80) ya da maskeli bir alanın 3.4 aralığının ihlali OUT_OF_RANGE;
 * ardından kip GUIDED değilse BAD_MODE. Kip, komutun işlendiği andaki kiptir (aynı çağrıda
 * önce gelen SET_MODE(GUIDED) görülür; W3-K13'ün "ardışık çağrılarla aynı sonuç" ilkesi).
 * Maskesiz alan yok sayılır: o kanalın saklı hedefi değişmez.
 *
 * Kanallar ve öncelik (bir iletide birden çok alan aynı kanalı isterse): dikey DEPTH >
 * ALTITUDE > VEL_D; yön YAW > YAW_RATE; yatay POS_NE > VEL_NE. Kabul edilen hedef güdümde
 * saklanır ve ida_gud_kon_istegi'nde görev modülünün isteğinin (ida_gor_istek) ilgili
 * kanalına yazılır. Kanalın gerektirdiği kestirim bileşeni geçersizse hedef saklanır ama
 * kanal sönümlü, sıfır hedefli davranışa iner (önce hız 0, hız da geçersizse kuvvet/moment 0;
 * W3-K7) ve bileşen geçerli olunca hedefe döner.
 *
 * Hedefin silinmesi: kip GUIDED değil, arıza eylemi HOLD ya da üstü (arıza > operatör,
 * 02 §2.6) ya da hedef iptal mandalının güdüm biti kurulu (silahsızlandırma, kip değişimi ya
 * da daralması, SESSION_END ya da SESSION_START; reddedilen ve out NULL çağrılar dahil, çünkü
 * mandal (b)/(b3)'te kurulur). Mandal kuruluyken saklı hedef ida_gud_adim çalışmasa da
 * hiçbir zaman uygulanmaz; bit yalnız hedef silindikten sonra temizlenir. Aynı çağrıda
 * daraltmadan sonra gelen GUIDED_SP geçerli yeni hedeftir. EMNİYET: silinen hedefe kendiliğinden dönülmez; araç görev modülünün tutma
 * hedefinde kalır ve kontrol referansını bulunulan durumdan yeniden kurar (ref_kusak).
 */
#ifndef IDA_CORE_GUDUM_H
#define IDA_CORE_GUDUM_H

#include <stdbool.h>
#include <stdint.h>

#include "cerceve/komut.h"
#include "gorev/gorev.h"
#include "ida/port.h"
#include "kontrol/kontrol.h"
#include "param/param.h"

/* Kanal hedef türleri (ida_gud_t). */
#define IDA_GUD_DIKEY_YOK      0u
#define IDA_GUD_DIKEY_DERINLIK 1u
#define IDA_GUD_DIKEY_IRTIFA   2u
#define IDA_GUD_DIKEY_HIZ      3u
#define IDA_GUD_YON_YOK        0u
#define IDA_GUD_YON_ACI        1u
#define IDA_GUD_YON_HIZ        2u
#define IDA_GUD_YATAY_YOK      0u
#define IDA_GUD_YATAY_KONUM    1u
#define IDA_GUD_YATAY_HIZ      2u

typedef struct {
    uint8_t dikey;         /* IDA_GUD_DIKEY_*                                               */
    uint8_t yon_turu;      /* IDA_GUD_YON_*                                                 */
    uint8_t yatay;         /* IDA_GUD_YATAY_*                                               */
    uint32_t kusak;        /* hedefin saklandığı daraltma kuşağı (yalnız tanı; karar
                              ida_emn_t.hedef_iptal mandalınındır)                          */
    uint32_t iptal;        /* saklı hedef her silindiğinde artar; kontrol isteğinin referans
                              kuşağına eklenir (sol bulgu 2)                                */
    float derinlik;        /* m, yüzeyden aşağı                                             */
    float irtifa;          /* m, dipten yukarı                                              */
    float dikey_hiz;       /* m/s, NED aşağı +                                              */
    float yon;             /* rad, (−π, π]                                                  */
    float yon_hizi;        /* rad/s, gövde r                                                */
    float konum_ne[2];     /* m, NED                                                        */
    float hiz_ne[2];       /* m/s, NED                                                      */
} ida_gud_t;

void ida_gud_kur(ida_gud_t *g);
void ida_gud_oturum_basi(ida_gud_t *g);
/* GUIDED_SP (tablo 3.17; A4-K50). Yazılan: c->gud; okunan: c->emn.mode (komutun işlendiği
   andaki kip; görev kancalarıyla aynı biçim). */
void ida_gud_komut(ida_core_t *c, const ida_komut_baglam_t *b, const ida_cmd_t *cmd,
                   ida_cmd_result_t *r);
/* Komutlardan sonra, her kabul edilen çağrıda: hedefin silinme kuralları. */
/* Yazılan: g ve c->emn.hedef_iptal'ın IDA_EMN_IPTAL_GUD biti (hedef silindikten sonra). */
void ida_gud_adim(ida_gud_t *g, ida_core_t *c);
bool ida_gud_tutarli(const ida_gud_t *g);
uint8_t ida_gud_param_grup(const ida_prm_kume_t *p);
/* Kontrol modülünün tek istek erişimcisi (kontrol.c istek_al): etkin kipin ve arıza eyleminin
   isteği (ida_gor_istek); GUIDED kipinde, arıza eylemi HOLD'dan hafifken ve araç silahlıyken
   GUIDED_SP'nin saklı kanalları bunun üzerine yazılır. */
void ida_gud_kon_istegi(const ida_gud_t *g, const ida_core_t *c, ida_kon_istek_t *i);

/* GUIDED_SP'nin biçim denetimi (3.17'nin OUT_OF_RANGE koşulları; birim testi için açık):
   true = maske ve maskeli bütün alanlar geçerli. */
bool ida_gud_sp_gecerli(const ida_guided_sp_t *sp);

#endif /* IDA_CORE_GUDUM_H */
