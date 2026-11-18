/* W0-MP4a yer tutucu: modüller arası iç bildirimler (ISO C17). */
#ifndef YT_IC_H
#define YT_IC_H

#include "yt.h"

/* Coğrafi orijin (int32, 1e-7 derece) ve düz dünya ölçeği (02 §2.3.7). */
#define YT_ORIJIN_LAT_E7  410000000
#define YT_ORIJIN_LON_E7  290000000
#define YT_E7_METRE       0.0111319491f  /* 1e-7 derece -> m (6378137 m yarıçap) */
#define YT_COS_LAT0       0.75470958f    /* cos(41°) */

void yt_ekf_baslat(yt_ekf_t *e);
void yt_ekf_imu_ekle(yt_ekf_t *e, const yt_imu_t *m);
void yt_ekf_olcum_ekle(yt_ekf_t *e, int sensor, const yt_olcum_t *o);
void yt_ekf_adim(yt_t *s);

void yt_gudum_baslat(yt_gudum_t *g);
void yt_gudum_adim(yt_t *s);
void yt_kontrol_baslat(yt_kontrol_t *c);
void yt_kontrol_adim(yt_t *s);
void yt_tahsis_baslat(yt_tahsis_t *a);
void yt_tahsis_adim(yt_t *s);

int yt_mesaj_coz(yt_t *s, const uint8_t *b);
void yt_mesaj_kodla(yt_t *s, uint8_t *b);

/* Gelen mesaj yerleşimi (bayt konumları); üreteç ve çözücü ortak kullanır. */
#define YT_M_BAYRAK   3u
#define YT_M_SEQ      2u
#define YT_M_IMU      8u    /* t i64, w 3f, a 3f */
#define YT_M_BASINC   40u   /* t i64, P f */
#define YT_M_DVL      52u   /* t i64, v 3f, fom f, irtifa f, kov 6f */
#define YT_M_MAN      104u  /* t i64, m 3f */
#define YT_M_GNSS     124u  /* t i64, lat i32, lon i32, vn f, ve f, eph f, alt f */
#define YT_M_KOMUT    156u  /* kip u8, 3 dolgu, sp 8f */
#define YT_M_BATARYA  192u  /* v f, sızıntı u8 */
#define YT_M_HAM      200u  /* 98 bayt telemetri/dolgu */
#define YT_M_CRC      298u
#define YT_CRC_EKSTRA 0x5Au

#endif
