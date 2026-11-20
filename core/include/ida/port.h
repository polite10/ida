/*
 * İDA — ortak çekirdek <-> çevresi: port arayüzü (A4)
 *
 * Sözleşme: docs/sozlesmeler/A4-port.md, sürüm 1.0-aday (dondurma öncesi son tur).
 * Bu dosya sözleşmenin makinece okunur parçasıdır; belgeyle çelişemez.
 * Yalnız tip tanımları, sabitler ve işlev bildirimleri içerir (A4-K01).
 * Her bildirimin üstündeki yorum, ilgili kural numarasını verir.
 *
 * Derleme: ISO C17, -std=c17 -pedantic-errors (A4-K01).
 */
#ifndef IDA_PORT_H
#define IDA_PORT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <float.h>

/* A4-K41: algılama kaydı ve parti başlığı A8 sözleşmesinindir
   (ida_det_record_t, ida_det_batch_t). */
#include "ida/algilama_kaydi.h"

/* A4-K04: f32 = IEEE-754 binary32. */
#if FLT_MANT_DIG != 24 || FLT_MAX_EXP != 128
#error "ida/port.h: float IEEE-754 binary32 olmalidir (A4-K04)"
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------------ */
/* 1. Sürüm (A4-K02)                                                  */
/* ------------------------------------------------------------------ */

#define IDA_PORT_VERSION_MAJOR 1u
#define IDA_PORT_VERSION_MINOR 0u

/* ------------------------------------------------------------------ */
/* 2. Kapasiteler ve sabitler (A4-K31, A4-K43, A4-K46, A4-K47; 3.12)   */
/* ------------------------------------------------------------------ */

#define IDA_PORT_N_THR               8u   /* itici sayısı (iskelet §5)                */
#define IDA_PORT_N_TUBE              2u   /* fırlatıcı tüpü sayısı (iskelet §5)       */
#define IDA_PORT_MAX_INST            4u   /* sensör türü başına örnek (inst) sayısı   */
#define IDA_PORT_MAX_SAMPLES        16u   /* çağrı başına IMU dışı örnek              */
#define IDA_PORT_MAX_CMDS            8u   /* çağrı başına daraltıcı olmayan komut     */
#define IDA_PORT_N_NARROW            4u   /* daraltıcı mandal türü sayısı (A4-K46)    */
#define IDA_PORT_MAX_RESULTS        12u   /* MAX_CMDS + N_NARROW (A4-K43)             */
#define IDA_PORT_MAX_DET_IN          8u   /* çağrı başına gelen algılama kaydı        */
#define IDA_PORT_MAX_DET_BATCH       2u   /* çağrı başına gelen algılama partisi      */
#define IDA_PORT_MAX_DET_OUT         4u   /* çağrı başına çıkan algılama kaydı        */
#define IDA_PORT_MAX_EVENTS          4u   /* çağrı başına yeni olay                   */
#define IDA_PORT_MAX_EVRES           4u   /* çağrı başına olay sonucu                 */
#define IDA_PORT_MAX_LOGS            4u   /* çağrı başına günlük kaydı                */
#define IDA_PORT_CMD_DEDUP          32u   /* yinelenen cmd_id penceresi (A4-K48)      */
#define IDA_PING360_MAX_SAMPLES   1200u   /* Ping360 örnek sayısı üst sınırı (cihaz)  */
#define IDA_PING360_ANGLE_COUNT    400u   /* Ping360 açı birimi: gradyan (cihaz)      */
#define IDA_PARAM_BLOB_MAX        8000u   /* kodlanmış parametre bloğu üst sınırı (B) */
#define IDA_PARAM_NAME_MAX          16u   /* parametre adı (MAVLink param_id sınırı)  */

/* A4-K08: zaman değerlerinin geçerli üst sınırı, 2^62 ns. */
#define IDA_TIME_MAX_NS INT64_C(4611686018427387904)

/* A4-K18, A4-K19, A4-K23, A4-K39, A4-K70: süre sınırları (mimari seçimi, tahmin). */
#define IDA_PORT_TICK_MAX_GAP_NS     UINT32_C(11000000)   /* 11 ms   */
#define IDA_PORT_DISPATCH_MAX_NS     UINT32_C(500000)     /* 0,5 ms  */
#define IDA_PORT_START_MAX_NS        UINT32_C(4000000)    /* 4 ms = step bütçesi + dağıtım */
#define IDA_PORT_STEP_BUDGET_NS      UINT32_C(3500000)    /* 3,5 ms  */
#define IDA_PORT_TICK_BUDGET_NS      UINT32_C(500000)     /* 0,5 ms  */
#define IDA_PORT_BG_BUDGET_NS        UINT32_C(300000)     /* 0,3 ms  */
#define IDA_PORT_STALE_MARGIN_NS     UINT32_C(14500000)   /* 14,5 ms = 11 + 3,5 (A4-K39) */
#define IDA_EVT_RESULT_MARGIN_NS     UINT32_C(100000000)  /* 100 ms  */

/* A4-K58, A4-K66 (W1-uyum U7): süre alanlarının geçerli aralıkları; sahibi A1-K104. */
#define IDA_VALID_FOR_MIN_NS         UINT32_C(60000000)   /* 60 ms   */
#define IDA_VALID_FOR_MAX_NS         UINT32_C(450000000)  /* 450 ms  */
#define IDA_VALID_FOR_DEFAULT_NS     UINT32_C(100000000)  /* 100 ms  */
#define IDA_APPLY_BY_MIN_NS          UINT32_C(16000000)   /* 16 ms   */
#define IDA_APPLY_BY_MAX_NS          UINT32_C(1000000000) /* 1000 ms */
#define IDA_HOLD_MIN_NS              UINT32_C(60000000)   /* 60 ms   */
#define IDA_HOLD_MAX_NS              UINT32_C(450000000)  /* 450 ms  */

/* A4-K39 (W1-uyum U7): IMU bayatlık parametresi T_imu_stale. */
#define IDA_IMU_STALE_MIN_NS         UINT32_C(5000000)    /* 5 ms    */
#define IDA_IMU_STALE_MAX_NS         UINT32_C(30000000)   /* 30 ms   */
#define IDA_IMU_STALE_DEFAULT_NS     UINT32_C(20000000)   /* 20 ms   */

/* A4-K118: algılama akışı bayatlık parametresi T_det_stale (tasarım seçimi). */
#define IDA_DET_STALE_DEFAULT_NS     UINT32_C(2000000000) /* 2 s     */

/* A4-K61, A4-K62: olay kimliği. */
#define IDA_BOOT_COUNT_MIN           UINT32_C(1)
#define IDA_BOOT_COUNT_MAX           UINT32_C(0xFFFFFFFE)
#define IDA_EVT_CTR_FIRST            UINT32_C(1)
#define IDA_EVT_CTR_CLOSE_ONLY       UINT32_C(0xFFFFFFF0)
#define IDA_EVT_CTR_LAST             UINT32_C(0xFFFFFFFF)

/* ------------------------------------------------------------------ */
/* 3. Zaman (A4-K07 … A4-K15)                                         */
/* ------------------------------------------------------------------ */

/* A4-K07, A4-K13: ölçüm zamanı (HIL/SIL'de sim zamanı), tamsayı ns. */
typedef struct { int64_t ns; } ida_stime_t;

/* A4-K07, A4-K09, A4-K11: duvar saati, tamsayı ns; örnek ömrü boyunca azalmaz. */
typedef struct { int64_t ns; } ida_wtime_t;

/* A4-K15, A4-K19, A4-K37, A4-K102: partinin zaman çerçevesi. */
typedef struct {
    bool        present;   /* 0: ızgara tick'i (zaman çerçevesi yok)          */
    uint32_t    seq;       /* partinin sıra numarası (seri aritmetik)          */
    ida_stime_t t;         /* partinin kapanış anı (sim zamanı)                */
    ida_wtime_t t_rx;      /* zaman çerçevesinin alım zamanı (A4-K14)          */
} ida_time_frame_t;

/* ------------------------------------------------------------------ */
/* 4. Dönüş kodları ve sensör tür kodları                              */
/* ------------------------------------------------------------------ */

/* A4-K25, A4-K28, A4-K101, A4-K113; değerlendirme sırası ve öncelik
   Bölüm 3.11; çıktılar Bölüm 4.14; işlevler Bölüm 4.15. */
typedef enum {
    IDA_RC_OK             = 0,
    IDA_RC_DUP_SEQ        = 1,  /* tf.seq son kabul edilene eşit (hata)          */
    IDA_RC_OLD_SEQ        = 2,  /* tf.seq son kabul edilenden küçük (hata)       */
    IDA_RC_BAD_INPUT      = 3,  /* yapısal girdi hatası (hata)                   */
    IDA_RC_NOT_INIT       = 4,  /* örnek başlatılmamış (hata)                    */
    IDA_RC_PARAM_DEFAULTS = 5,  /* yalnız init: blok geçersiz, varsayılanlar     */
    IDA_RC_VERSION        = 6,  /* yalnız init: port sürümü uyuşmuyor            */
    IDA_RC_NOT_FOUND      = 7,  /* yalnız parametre işlevleri                    */
    IDA_RC_IMU_REJECTED   = 8   /* yalnız step: IMU reddedildi, tick kuralı (K103) */
} ida_rc_t;

/* A4-K93, A4-K117: arka plan çağrısının dönüşü. Platform yalnız IDLE ve MORE
   dönen çağrıları bg_slices'a sayar (A4-K109). */
typedef enum {
    IDA_BG_IDLE      = 0,       /* bekleyen arka plan işi yok                    */
    IDA_BG_MORE      = 1,       /* bekleyen iş var                               */
    IDA_BG_BAD_INPUT = 2        /* c NULL ya da now_w aralık dışı; iş yapılmadı  */
} ida_bg_rc_t;

/* A4-K30: sensör türü (ida_hdr_t.type). */
#define IDA_SENS_IMU      1u
#define IDA_SENS_MAG      2u
#define IDA_SENS_BARO     3u
#define IDA_SENS_GNSS     4u
#define IDA_SENS_DVL      5u
#define IDA_SENS_PING1D   6u
#define IDA_SENS_PING360  7u
#define IDA_SENS_BATT     8u
#define IDA_SENS_LEAK     9u
#define IDA_SENS_SAFETY  10u

/* A4-K30, A4-K33: ida_hdr_t.flags bitleri. */
#define IDA_HF_VALID         0x0001u  /* sensör değeri kullanılabilir saydı         */
#define IDA_HF_SATURATED     0x0002u  /* en az bir değer doymuş                     */
#define IDA_HF_SENSOR_FAULT  0x0004u  /* sensör kendi arızasını bildirdi            */

/* ------------------------------------------------------------------ */
/* 5. Sensör örnekleri (A4-K30 … A4-K42; Bölüm 3.2, 3.3)              */
/* ------------------------------------------------------------------ */

/* A4-K13, A4-K14, A4-K30: ortak örnek başlığı. */
typedef struct {
    ida_stime_t t_meas;     /* ölçüm zamanı (A4-K13)                              */
    ida_wtime_t t_rx;       /* alım zamanı, platform damgası (A4-K14)             */
    uint32_t    seq;        /* sensörün kendi örnek sayacı (seri aritmetik)        */
    uint32_t    window_ns;  /* aralık ölçümünde aralık süresi; anlık ölçümde 0     */
    uint16_t    flags;      /* IDA_HF_*                                            */
    uint8_t     type;       /* IDA_SENS_*                                          */
    uint8_t     inst;       /* 0 .. IDA_PORT_MAX_INST-1                            */
} ida_hdr_t;

/* A4-K38, A4-K42: IMU; değerler [t_meas - window_ns, t_meas] ortalaması. */
typedef struct {
    float gyro[3];          /* rad/s, IMU çerçevesi                               */
    float accel[3];         /* m/s^2, özgül kuvvet, IMU montaj noktasında          */
    float temp_c;           /* °C                                                 */
} ida_imu_t;

/* Manyetometre; anlık (window_ns = 0). */
typedef struct {
    float field[3];         /* T, sensör çerçevesi                                */
} ida_mag_t;

/* Basınç; t_meas = dönüşüm penceresinin sonu, window_ns = pencere süresi. */
typedef struct {
    float p_abs;            /* Pa, mutlak                                         */
    float temp_c;           /* °C                                                 */
} ida_baro_t;

/* GNSS fix türü (ida_gnss_t.fix_type). */
#define IDA_GNSS_FIX_NONE  0u
#define IDA_GNSS_FIX_2D    1u
#define IDA_GNSS_FIX_3D    2u
#define IDA_GNSS_FIX_DGPS  3u
#define IDA_GNSS_FIX_RTK   4u

/* GNSS; t_meas = çözüm anı (anlık). */
typedef struct {
    int32_t lat_e7;         /* 1e-7 derece, WGS-84                                */
    int32_t lon_e7;         /* 1e-7 derece, WGS-84                                */
    int32_t alt_mm;         /* mm, ortalama deniz seviyesine göre                 */
    float   vel_ned[3];     /* m/s, NED                                           */
    float   eph;            /* m, yatay konum belirsizliği, 1 sigma               */
    float   epv;            /* m, dikey konum belirsizliği, 1 sigma               */
    uint8_t fix_type;       /* IDA_GNSS_FIX_*                                     */
    uint8_t n_sv;           /* kullanılan uydu sayısı                             */
} ida_gnss_t;

/* DVL bayrakları (ida_dvl_t.dvl_flags). */
#define IDA_DVL_VEL_VALID     0x01u
#define IDA_DVL_COV_PRESENT   0x02u
#define IDA_DVL_TRACK_BOTTOM  0x04u   /* 1: dip izi, 0: su izi */

/* DVL; t_meas = geçerlilik anı (yansıma anı), anlık. */
typedef struct {
    float   vel[3];         /* m/s, DVL çerçevesi, DVL'nin dibe göre hızı          */
    float   cov[6];         /* (m/s)^2, vel kovaryansı, DVL çerçevesi, üst üçgen
                               sırası xx, xy, xz, yy, yz, zz                      */
    float   fom;            /* m/s, cihazın başarım ölçüsü                         */
    float   beam_dist[4];   /* m, ışın boyunca mesafe; çözülemeyen ışında -1       */
    float   altitude;       /* m, dibe dik mesafe; bilinmiyorsa -1                 */
    uint8_t dvl_flags;      /* IDA_DVL_*                                          */
    uint8_t beam_valid;     /* bit i: ışın i geçerli (i = 0..3)                    */
} ida_dvl_t;

/* Ping1D; t_meas = yansıma anı; gönderim anı = t_meas - tx_offset_ns. */
typedef struct {
    float    distance;      /* m                                                  */
    float    scan_start;    /* m                                                  */
    float    scan_length;   /* m                                                  */
    uint32_t ping_number;   /* cihaz sayacı                                       */
    uint32_t tx_offset_ns;  /* ns, yansıma anı ile gönderim anı farkı             */
    uint8_t  confidence;    /* %, 0..100                                          */
} ida_ping1d_t;

/* Batarya; t_meas = pencere sonu, window_ns = pencere süresi. */
typedef struct {
    float voltage;          /* V                                                  */
    float current;          /* A, pozitif = boşalma                               */
    float temp_c;           /* °C                                                 */
} ida_batt_t;

/* Sızıntı; anlık. */
typedef struct {
    uint8_t mask;           /* bit i: sızıntı algılayıcısı i ıslak                */
} ida_leak_t;

/* A4-K75: tüp başına durum bitleri (ida_safety_t.tube). */
#define IDA_TUBE_LOADED       0x01u  /* tüp dolu                                  */
#define IDA_TUBE_PLUG_IN      0x02u  /* sanal silahlandırma fişi takılı            */
#define IDA_TUBE_PIN_OUT      0x04u  /* sanal emniyet pimi çıkarılmış              */
#define IDA_TUBE_PERMIT_OPEN  0x08u  /* fırlatıcının süreli izni şu an açık        */

/* A4-K34, A4-K75, A4-K111: sanal emniyet girdileri ve fırlatıcı durumu (yalnız okunur). */
typedef struct {
    uint8_t estop;                    /* 1: sanal acil durdurma etkin              */
    uint8_t tube[IDA_PORT_N_TUBE];    /* IDA_TUBE_* bitleri                        */
} ida_safety_t;

/* A4-K30, A4-K121: IMU ve Ping360 dışındaki örnekler. Birlik u'nun seçicisi hdr.type:
   MAG->mag, BARO->baro, GNSS->gnss, DVL->dvl, PING1D->ping1d, BATT->batt,
   LEAK->leak, SAFETY->safety. Etkin olmayan üyenin baytları anlamsızdır. */
typedef struct {
    ida_hdr_t hdr;
    union {
        ida_mag_t    mag;
        ida_baro_t   baro;
        ida_gnss_t   gnss;
        ida_dvl_t    dvl;
        ida_ping1d_t ping1d;
        ida_batt_t   batt;
        ida_leak_t   leak;
        ida_safety_t safety;
    } u;
} ida_sample_t;

/* A4-K17, A4-K103: IMU örneği (hdr.type = IDA_SENS_IMU). */
typedef struct {
    ida_hdr_t hdr;
    ida_imu_t imu;
} ida_imu_sample_t;

/* A4-K40: Ping360 tarama satırı; t_meas = darbenin gönderim anı (window_ns = 0).
   Alanlar cihaz birimlerindedir; kutu zamanı A1 sözleşmesinin kuralıdır. */
typedef struct {
    ida_hdr_t hdr;                         /* hdr.type = IDA_SENS_PING360         */
    uint16_t  angle_grad;                  /* 0 .. 399 gradyan                    */
    uint16_t  n_samples;                   /* 1 .. IDA_PING360_MAX_SAMPLES        */
    uint16_t  sample_period;               /* 25 ns birimi, >= 1                  */
    uint16_t  tx_duration_us;              /* µs                                  */
    uint16_t  tx_freq_khz;                 /* kHz                                 */
    uint8_t   gain;                        /* cihaz kazanç kodu                   */
    uint8_t   data[IDA_PING360_MAX_SAMPLES]; /* yalnız ilk n_samples anlamlı      */
} ida_ping360_t;

/* ------------------------------------------------------------------ */
/* 6. Komutlar ve daraltıcı mandallar (A4-K43 … A4-K52, A4-K104, K105) */
/* ------------------------------------------------------------------ */

/* ida_cmd_t.type */
#define IDA_CMD_HEARTBEAT        1u
#define IDA_CMD_ARM              2u   /* genişletici (A4-K104) */
#define IDA_CMD_DISARM           3u   /* daraltıcı: yalnız mandalla gelir (A4-K46) */
#define IDA_CMD_SET_MODE         4u   /* hedef GUIDED/AUTO değilse daraltıcı (mandal) */
#define IDA_CMD_MANUAL           5u
#define IDA_CMD_GUIDED_SP        6u
#define IDA_CMD_MISSION_BEGIN    7u
#define IDA_CMD_MISSION_ITEM     8u
#define IDA_CMD_MISSION_END      9u
#define IDA_CMD_MISSION_CTL     10u
#define IDA_CMD_PARAM_SET       11u
#define IDA_CMD_PARAM_GET       12u
#define IDA_CMD_PARAM_COMMIT    13u
#define IDA_CMD_PARAM_SAVE      14u
#define IDA_CMD_FAILSAFE_RESET  15u   /* genişletici (A4-K104) */
#define IDA_CMD_FC              16u   /* fc.op FC_DISARM/FC_ABORT ise daraltıcı (mandal) */

/* Kipler (set_mode.mode ve ida_status_t.mode). */
#define IDA_MODE_INIT          0u
#define IDA_MODE_DISARMED      1u
#define IDA_MODE_MANUAL        2u
#define IDA_MODE_STABILIZE     3u
#define IDA_MODE_DEPTH_HOLD    4u
#define IDA_MODE_STATION_KEEP  5u
#define IDA_MODE_GUIDED        6u
#define IDA_MODE_AUTO          7u

/* A4-K50: GUIDED setpoint maske bitleri. */
#define IDA_SP_POS_NE     0x01u
#define IDA_SP_DEPTH      0x02u
#define IDA_SP_ALTITUDE   0x04u
#define IDA_SP_VEL_NE     0x08u
#define IDA_SP_VEL_D      0x10u
#define IDA_SP_YAW        0x20u
#define IDA_SP_YAW_RATE   0x40u

/* A4-K50 */
typedef struct {
    uint8_t mask;          /* IDA_SP_*                                           */
    float   pos_ne[2];     /* m, NED, senaryo orijinine göre                     */
    float   depth;         /* m, yüzeyden aşağı pozitif                          */
    float   altitude;      /* m, dipten yukarı pozitif                           */
    float   vel_ned[3];    /* m/s, NED                                           */
    float   yaw;           /* rad, (-pi, pi], kuzeyden doğuya                    */
    float   yaw_rate;      /* rad/s                                              */
} ida_guided_sp_t;

/* Görev öğesi türleri: kapalı liste, A1 Tablo 3.6a (W1-uyum U1); 8 ve üstü yoktur. */
#define IDA_MI_WAYPOINT      1u
#define IDA_MI_DIVE_TO       2u
#define IDA_MI_SURFACE       3u
#define IDA_MI_HOLD          4u
#define IDA_MI_SCAN_LINES    5u
#define IDA_MI_SCAN_POINT    6u
#define IDA_MI_WAIT_GNSS     7u

/* Görev öğesinin konum çerçevesi. */
#define IDA_MI_FRAME_NONE    0u
#define IDA_MI_FRAME_LOCAL   1u   /* north_m, east_m (NED, senaryo orijini) */
#define IDA_MI_FRAME_GLOBAL  2u   /* lat_e7, lon_e7 (WGS-84)                 */

/* A4-K51: görev öğesi; bütün alanların anlamı, birimi ve aralığı A1 Tablo 3.6a. */
typedef struct {
    uint16_t index;        /* 0 .. n-1                                           */
    uint8_t  item_type;    /* IDA_MI_*                                           */
    uint8_t  frame;        /* IDA_MI_FRAME_*                                     */
    float    p[4];         /* türe göre A1 Tablo 3.6a; "0" yazanlar tam 0     */
    int32_t  lat_e7;       /* frame = GLOBAL                                      */
    int32_t  lon_e7;       /* frame = GLOBAL                                      */
    float    north_m;      /* frame = LOCAL                                       */
    float    east_m;       /* frame = LOCAL                                       */
    float    depth;        /* m, yüzeyden aşağı pozitif                           */
} ida_mission_item_t;

/* Görev denetimi (mission_ctl.action). */
#define IDA_MCTL_START   1u
#define IDA_MCTL_PAUSE   2u
#define IDA_MCTL_RESUME  3u
#define IDA_MCTL_ABORT   4u

/* Parametre değer türü. */
#define IDA_PT_I32  1u
#define IDA_PT_U32  2u
#define IDA_PT_F32  3u

/* A4-K81, A4-K85, A4-K121: birlik v'nin seçicisi type: I32->i32, U32->u32, F32->f32.
   Üç üye aynı 32 biti paylaşır; kayıtta v her zaman 32 bitlik ham değerdir (belge 3.14). */
typedef struct {
    uint16_t id;
    uint8_t  type;         /* IDA_PT_*                                            */
    union {
        int32_t  i32;
        uint32_t u32;
        float    f32;
    } v;
} ida_param_value_t;

/* Atış kontrol komut işlemleri (ida_fc_cmd_t.op): kapalı liste, A1 Tablo 3.6
   (W1-uyum U1); 4 ve 9 ve üstü yoktur. */
#define IDA_FC_ARM         1u   /* genişletici (A4-K104) */
#define IDA_FC_DISARM      2u   /* daraltıcı (mandal)    */
#define IDA_FC_ABORT       3u   /* daraltıcı (mandal)    */
#define IDA_FC_FIRE        5u   /* genişletici           */
#define IDA_FC_RESET       6u   /* genişletici           */
#define IDA_FC_TEST_ENTER  7u   /* genişletici           */
#define IDA_FC_TEST_EXIT   8u

/* A4-K44, A4-K119: atış kontrol komutu; arg0 ve arg1 ayrılmıştır, bu sürümde 0
   olmak zorundadır (değilse komut reddedilir). */
typedef struct {
    uint8_t  op;           /* IDA_FC_*                                            */
    uint8_t  tube;         /* 0 .. IDA_PORT_N_TUBE-1                              */
    uint32_t arg0;         /* ayrılmış, 0                                         */
    uint32_t arg1;         /* ayrılmış, 0                                         */
} ida_fc_cmd_t;

/* A4-K43, A4-K121: daraltıcı olmayan komut (daraltıcılar ida_narrow_t ile gelir).
   Birlik u'nun seçicisi type: SET_MODE->set_mode, MANUAL->manual, GUIDED_SP->guided,
   MISSION_BEGIN->mission_begin, MISSION_ITEM->mission_item, MISSION_CTL->mission_ctl,
   PARAM_SET->param (id, type, v), PARAM_GET->yalnız param.id (type ve v okunmaz),
   FC->fc; HEARTBEAT, ARM, MISSION_END, PARAM_COMMIT,
   PARAM_SAVE ve FAILSAFE_RESET'te etkin üye yoktur. */
typedef struct {
    uint32_t cmd_id;       /* sonuç aynı kimlikle (A4-K43, A4-K48)               */
    uint8_t  type;         /* IDA_CMD_*                                           */
    uint8_t  source;       /* gönderen sistem kimliği (bilgi; yetki değil)        */
    union {
        struct { uint8_t mode; }      set_mode;      /* IDA_MODE_*              */
        struct { float axes[6]; }     manual;        /* A4-K49                  */
        ida_guided_sp_t               guided;        /* A4-K50                  */
        struct { uint16_t count; }    mission_begin;
        ida_mission_item_t            mission_item;
        struct { uint8_t action; }    mission_ctl;   /* IDA_MCTL_*              */
        ida_param_value_t             param;         /* SET: id+değer; GET: id  */
        ida_fc_cmd_t                  fc;
    } u;
} ida_cmd_t;

/* A4-K44, A4-K46: daraltıcı mandal bitleri (ida_narrow_t.mask). */
#define IDA_NARROW_DISARM     0x01u   /* IDA_CMD_DISARM                          */
#define IDA_NARROW_MODE       0x02u   /* GUIDED/AUTO dışına IDA_CMD_SET_MODE     */
#define IDA_NARROW_FC_DISARM  0x04u   /* IDA_CMD_FC, op = IDA_FC_DISARM          */
#define IDA_NARROW_FC_ABORT   0x08u   /* IDA_CMD_FC, op = IDA_FC_ABORT           */
#define IDA_NARROW_DEFINED    0x0Fu   /* tanımlı bitler; diğerleri BAD_INPUT (K113) */

/* A4-K46: platformun daraltıcı mandalları; tür başına tek bit ve son kimlik. */
typedef struct {
    uint8_t  mask;                        /* IDA_NARROW_*                         */
    uint8_t  mode;                        /* NARROW_MODE: en son hedef kip         */
    uint32_t cmd_id[IDA_PORT_N_NARROW];   /* bit i'nin en son komut kimliği        */
    uint8_t  coalesced[IDA_PORT_N_NARROW];/* bit i'de birleştirilen ek komut sayısı */
} ida_narrow_t;

/* ------------------------------------------------------------------ */
/* 7. Olay sonuçları ve platform durumu (A4-K68, A4-K72; 3.5, 3.6)     */
/* ------------------------------------------------------------------ */

/* A4-K68: olay sonuç kodları. 1..15 alıcının (sim), 16.. platformun. */
#define IDA_EVR_APPLIED             1u
#define IDA_EVR_DUPLICATE           2u
#define IDA_EVR_REJ_EXPIRED         3u
#define IDA_EVR_REJ_SAFE            4u
#define IDA_EVR_REJ_TUBE_EMPTY      5u
#define IDA_EVR_REJ_SESSION         6u
#define IDA_EVR_DROPPED_GATE       16u
#define IDA_EVR_DROPPED_NO_SLOT    17u
#define IDA_EVR_EXPIRED_NO_ACK     18u
#define IDA_EVR_SESSION_END        19u

/* A4-K68, A4-K69 */
typedef struct {
    uint64_t    event_id;
    uint8_t     result;     /* IDA_EVR_*                                          */
    ida_stime_t t_apply;    /* APPLIED/DUPLICATE: uygulandığı an; diğerlerinde 0  */
} ida_event_result_t;

/* A4-K72 */
#define IDA_LINK_NONE     0u   /* oturum yok                       */
#define IDA_LINK_RUNNING  1u   /* oturum KOŞUYOR                   */

#define IDA_LINKEV_NONE           0u
#define IDA_LINKEV_SESSION_START  1u
#define IDA_LINKEV_SESSION_END    2u

/* A4-K83 */
#define IDA_PSTORE_IDLE  0u
#define IDA_PSTORE_BUSY  1u
#define IDA_PSTORE_OK    2u
#define IDA_PSTORE_FAIL  3u

#define IDA_PSFAIL_NONE     0u
#define IDA_PSFAIL_FLASH    1u
#define IDA_PSFAIL_VERIFY   2u
#define IDA_PSFAIL_ABORTED  3u
#define IDA_PSFAIL_TOO_BIG  4u

/* A4-K72, A4-K83, A4-K23, A4-K59, A4-K102, A4-K105; Bölüm 3.6.
   Sayaçlar açılıştan (SIL'de init'ten) beri birikir, 2^32'de sarar. */
typedef struct {
    uint8_t  link_state;            /* IDA_LINK_*                                 */
    uint8_t  link_event;            /* IDA_LINKEV_*; olay yalnız bir çağrıda     */
    uint8_t  param_store;           /* IDA_PSTORE_*                               */
    uint8_t  param_fail;            /* IDA_PSFAIL_*                               */
    uint32_t param_gen;             /* son başarılı kaydın kuşağı                 */
    bool     prev_overrun;          /* önceki çekirdek çağrısı bütçesini aştı     */
    bool     tx_stalled;            /* kritik gönderim kuyruğu boşalmıyor         */
    bool     batch_merged;          /* bu girdi birden çok partiden birleşti      */
    bool     input_dropped;         /* bu girdide kapasite yüzünden üye düştü     */
    uint32_t prev_call_ns;          /* önceki çekirdek çağrısının süresi; 0: yok  */
    uint32_t cnt_batch_skipped;     /* hiç üyesi gelmeden atlanan parti (GAP)     */
    uint32_t cnt_batch_incomplete;  /* eksik kapanan parti                        */
    uint32_t cnt_batch_old;         /* atılan eski parti (OLD)                    */
    uint32_t cnt_batch_merged;      /* birleştirilen parti (A4-K102)              */
    uint32_t cnt_stale_session;     /* eski oturum çerçevesi                      */
    uint32_t cnt_member_overflow;   /* kapasiteyi aşıp düşen örnek/kayıt          */
    uint32_t cnt_cmd_deferred;      /* sonraki çağrıya ertelenen komut            */
    uint32_t cnt_cmd_dropped;       /* atılan (daraltıcı olmayan) komut           */
    uint32_t cnt_narrow_coalesced;  /* mandalda birleştirilen daraltıcı           */
    uint32_t cnt_grid_missed;       /* kaçırılan ızgara anı                       */
    uint32_t cnt_rx_error;          /* CRC ve çerçeveleme hatası                  */
    uint32_t cnt_out_fixed;         /* platformun nötrle değiştirdiği çıktı       */
} ida_plat_t;

/* ------------------------------------------------------------------ */
/* 8. Çağrı girdisi (A4-K16 … A4-K46, A4-K102 … A4-K109; Bölüm 3.7)    */
/* ------------------------------------------------------------------ */

/* A4-K16, A4-K17, A4-K31, A4-K99: step ve tick'in ortak girdisi. */
typedef struct {
    ida_wtime_t        now_w;                         /* A4-K09 … A4-K11          */
    ida_time_frame_t   tf;                            /* A4-K15, A4-K102          */
    bool               imu_present;                   /* step: 1, tick: 0 (A4-K17) */
    ida_imu_sample_t   imu;
    uint8_t            n_samples;                     /* <= IDA_PORT_MAX_SAMPLES   */
    ida_sample_t       samples[IDA_PORT_MAX_SAMPLES];
    const ida_ping360_t *ping360;                     /* NULL: yok (A4-K40)       */
    uint8_t            n_det_batch;                   /* <= IDA_PORT_MAX_DET_BATCH */
    ida_det_batch_t    det_batch[IDA_PORT_MAX_DET_BATCH]; /* A8; A4-K41          */
    ida_wtime_t        det_batch_t_rx[IDA_PORT_MAX_DET_BATCH];
    uint8_t            n_det;                         /* <= IDA_PORT_MAX_DET_IN    */
    ida_det_record_t   det[IDA_PORT_MAX_DET_IN];      /* A8; A4-K41               */
    ida_wtime_t        det_t_rx[IDA_PORT_MAX_DET_IN]; /* kayıt başına alım zamanı */
    ida_narrow_t       narrow;                        /* A4-K44 … A4-K46          */
    uint8_t            n_cmds;                        /* <= IDA_PORT_MAX_CMDS      */
    ida_cmd_t          cmds[IDA_PORT_MAX_CMDS];
    uint8_t            n_evres;                       /* <= IDA_PORT_MAX_EVRES     */
    ida_event_result_t evres[IDA_PORT_MAX_EVRES];
    uint32_t           bg_slices;                     /* A4-K109                  */
    ida_plat_t         plat;
} ida_in_t;

/* ------------------------------------------------------------------ */
/* 9. Çağrı çıktısı (A4-K53 … A4-K92; Bölüm 3.8, 4.14)                 */
/* ------------------------------------------------------------------ */

/* Çıktının kaynağı (ida_out_t.src). */
#define IDA_SRC_STEP  1u
#define IDA_SRC_TICK  2u

/* A4-K55: arıza eylemi düzeyi (ida_out_t.fs_level), artan ağırlıkta. */
#define IDA_FS_NONE             0u
#define IDA_FS_WARN             1u
#define IDA_FS_HOLD             2u
#define IDA_FS_SURFACE          3u
#define IDA_FS_PASSIVE_SURFACE  4u
#define IDA_FS_DISARM           5u

/* A4-K60: olay türleri. */
#define IDA_EVT_SAFETY_OPEN   1u   /* izin veren, süreli (hold_ns); genişletici  */
#define IDA_EVT_SAFETY_CLOSE  2u   /* izni kaldıran, süresiz                      */
#define IDA_EVT_FIRE          3u   /* atış; genişletici                           */

#define IDA_TUBE_ALL  0xFFu        /* yalnız SAFETY_CLOSE                        */

/* A4-K60 … A4-K66: tek seferlik olay. */
typedef struct {
    uint64_t    event_id;      /* (boot_count << 32) | sayaç (A4-K61)             */
    uint8_t     kind;          /* IDA_EVT_*                                        */
    uint8_t     tube;          /* 0 .. IDA_PORT_N_TUBE-1 ya da IDA_TUBE_ALL         */
    uint32_t    seq;           /* kararın verildiği çağrının ida_out_t.seq'i        */
    ida_stime_t t_decide;      /* kararın ölçüm zamanı (ida_out_t.t_ref)            */
    uint32_t    apply_by_ns;   /* son geçerlilik: t_decide + apply_by_ns (A4-K66)  */
    uint32_t    hold_ns;       /* yalnız SAFETY_OPEN: izin süresi (A4-K66, K107)    */
} ida_event_t;

/* Komut sonucu türü (ida_cmd_result_t.kind). */
#define IDA_RES_ACK          1u   /* yalnız kabul/ret                            */
#define IDA_RES_PARAM_VALUE  2u   /* PARAM_GET ve PARAM_SET: value geçerli       */
#define IDA_RES_PARAM_SAVE   3u   /* PARAM_SAVE: kabulde save geçerli            */

/* Komut sonucu (ida_cmd_result_t.ack). */
#define IDA_ACK_ACCEPTED     1u
#define IDA_ACK_REJECTED     2u
#define IDA_ACK_IN_PROGRESS  3u
#define IDA_ACK_DUPLICATE    4u
#define IDA_ACK_UNSUPPORTED  5u

/* A4-K120: ret nedenleri (ida_cmd_result_t.reason). Çekirdek yalnız 0–15'i üretir;
   16–127 ayrılmış; 128–191 A1 bağlantı katmanınındır, port sonucu olarak üretilmez;
   192 ve üstü (0x100 ve üstü dahil) ayrılmış, kullanılmaz. */
#define IDA_REASON_CORE_LAST    15u
#define IDA_REASON_LINK_FIRST  128u
#define IDA_REASON_LINK_LAST   191u
#define IDA_REASON_NONE          0u
#define IDA_REASON_CALL_REJECTED 1u  /* çağrı hata döndü (A4-K26)                 */
#define IDA_REASON_ARMED         2u  /* silahlıyken ya da aynı çağrıda ARM varken  */
#define IDA_REASON_NOT_ARMABLE   3u  /* genişletmenin durum ön koşulu tutmuyor (arm dahil; belge 3.17) */
#define IDA_REASON_PARAM_BUSY    4u  /* parametre kaydı sürüyor (A4-K83)          */
#define IDA_REASON_PARAM_INVALID 5u
#define IDA_REASON_OUT_OF_RANGE  6u
#define IDA_REASON_BAD_MODE      7u  /* kip, görev ya da atış kontrol durumu uygun değil (3.17) */
#define IDA_REASON_EVENTS_OFF    8u  /* olay kimliği kullanılamaz (A4-K61, K62)    */
#define IDA_REASON_SESSION       9u  /* oturum yok                                */
#define IDA_REASON_NARROWED     10u  /* aynı çağrıda daraltma var (A4-K45)         */
#define IDA_REASON_IMU_LAG      11u  /* IMU gecikmeli (A4-K108)                    */
#define IDA_REASON_IMU_REJECTED 12u  /* çağrının IMU'su reddedildi (A4-K103)       */
#define IDA_REASON_ONE_PER_CALL 13u  /* çağrı başına bir kez (A4-K112)             */
#define IDA_REASON_NOT_INIT     14u  /* örnek kurulmamış                          */
#define IDA_REASON_COALESCED    15u  /* mandalda birleştirildi (A4-K46)            */

/* A4-K82: parametre kaydı içeriği (yalnız kind = PARAM_SAVE ve ACCEPTED). */
typedef struct {
    uint32_t       gen;           /* bloğun kuşağı (artan)                        */
    const uint8_t *blob;          /* çekirdeğin belleği; sonraki çağrıya kadar    */
    uint32_t       len;           /* <= IDA_PARAM_BLOB_MAX                        */
} ida_param_save_t;

/* A4-K43, A4-K121: komut kimliğiyle çoklanmış sonuç. Seçici kind: PARAM_VALUE ise
   value, PARAM_SAVE ve ack = ACCEPTED ise save geçerlidir; geçersiz olan çekirdekçe
   sıfırla yazılır. */
typedef struct {
    uint32_t          cmd_id;
    uint8_t           kind;       /* IDA_RES_*                                    */
    uint8_t           ack;        /* IDA_ACK_*                                    */
    uint16_t          reason;     /* IDA_REASON_*                                 */
    ida_param_value_t value;      /* kind = PARAM_VALUE                           */
    ida_param_save_t  save;       /* kind = PARAM_SAVE ve ack = ACCEPTED          */
} ida_cmd_result_t;

/* Kestirim düzeyi (ida_nav_out_t.est_level). */
#define IDA_EST_INIT      0u
#define IDA_EST_OK        1u
#define IDA_EST_DEGRADED  2u
#define IDA_EST_BAD       3u
#define IDA_EST_FAULT     4u

/* A4-K87: seyrüsefer çıkışı. */
typedef struct {
    ida_stime_t t;             /* çıkış durumunun ölçüm zamanı                     */
    float   p_ned[3];          /* m, senaryo orijinine göre, gövde referans noktası */
    float   v_ned[3];          /* m/s                                              */
    float   q_nb[4];           /* (w, x, y, z), Hamilton, gövde -> NED             */
    float   omega_b[3];        /* rad/s, gövde                                     */
    float   depth;             /* m, yüzeyden aşağı pozitif                        */
    float   altitude;          /* m, dibe; bilinmiyorsa -1                         */
    float   p_sigma[3];        /* m, 1 sigma                                       */
    float   v_sigma[3];        /* m/s, 1 sigma                                     */
    float   att_sigma[3];      /* rad, 1 sigma                                     */
    int32_t origin_lat_e7;     /* orijin, WGS-84                                   */
    int32_t origin_lon_e7;
    uint8_t est_level;         /* IDA_EST_*                                        */
    uint8_t reset_counter;     /* konum sıfırlamasında artar, sarar                */
} ida_nav_out_t;

/* Arıza nedeni bitleri (ida_status_t.fs_reasons). */
#define IDA_FSR_LEAK          0x00000001u
#define IDA_FSR_GCS_LOSS      0x00000002u
#define IDA_FSR_BATT_LOW      0x00000004u
#define IDA_FSR_BATT_CRIT     0x00000008u
#define IDA_FSR_DEPTH_LIMIT   0x00000010u
#define IDA_FSR_ALT_MIN       0x00000020u
#define IDA_FSR_GEOFENCE      0x00000040u
#define IDA_FSR_EST_DEGRADED  0x00000080u
#define IDA_FSR_EST_BAD       0x00000100u
#define IDA_FSR_NUMERIC       0x00000200u
#define IDA_FSR_IMU_STALE     0x00000400u
#define IDA_FSR_IMU_LAG       0x00000800u
#define IDA_FSR_STEP_MISS     0x00001000u
#define IDA_FSR_PARAM         0x00002000u
#define IDA_FSR_SESSION       0x00004000u

/* Fiziksel rejim ve güdüm aşaması (ida_status_t). */
#define IDA_REG_SURFACE     0u
#define IDA_REG_TRANSITION  1u
#define IDA_REG_SUBMERGED   2u

#define IDA_PH_SURFACE_TRANSIT  0u
#define IDA_PH_DIVE             1u
#define IDA_PH_SUBMERGED        2u
#define IDA_PH_ASCENT           3u

/* Görev durumu (ida_status_t.mission_state). */
#define IDA_MS_IDLE      0u
#define IDA_MS_LOADED    1u
#define IDA_MS_RUNNING   2u
#define IDA_MS_PAUSED    3u
#define IDA_MS_COMPLETE  4u
#define IDA_MS_ABORTED   5u

/* A4-K88: çekirdek sayaçları; init'ten beri birikir, 2^32'de sarar. */
typedef struct {
    uint32_t sample_rejected;     /* A4-K33 ile reddedilen örnek (bütün türler)  */
    uint32_t sample_future;       /* t_meas > tf.t ya da t_rx > now_w            */
    uint32_t sample_conflict;     /* A4-K115: eşit anahtar, farklı içerik (çift başına 1) */
    uint32_t sample_dup_dropped;  /* A4-K115: bayt bayt aynı yinelenme            */
    uint32_t narrow_from_rejected;/* A4-K116: reddedilen örnekten uygulanan daraltma */
    uint32_t bg_bad_input;        /* A4-K117                                      */
    uint32_t imu_rejected;        /* A4-K103                                      */
    uint32_t imu_gap;             /* A4-K38                                       */
    uint32_t bad_input;           /* BAD_INPUT dönüşü                             */
    uint32_t dup_seq;
    uint32_t old_seq;
    uint32_t wall_backstep;       /* A4-K10                                       */
    uint32_t cmd_rejected;
    uint32_t widen_blocked;       /* A4-K45, A4-K108 ile engellenen genişletici   */
    uint32_t evres_ignored;       /* A4-K69                                       */
    uint32_t event_unknown;       /* A4-K70                                       */
    uint32_t det_rejected;        /* A4-K41                                       */
    uint32_t ping360_rejected;    /* A4-K40                                       */
    uint32_t bg_dropped;          /* A4-K94                                       */
    uint32_t bg_slice_mismatch;   /* A4-K109                                      */
    uint32_t log_dropped;         /* A4-K90                                       */
    uint32_t step_miss;           /* prev_overrun, atlanan ve birleştirilen parti toplamı; taban ilk çağrı */
} ida_core_counters_t;

/* A4-K88: durum. */
typedef struct {
    uint8_t  mode;                /* IDA_MODE_*                                   */
    bool     armed;
    uint8_t  fs_level;            /* IDA_FS_*                                     */
    uint32_t fs_reasons;          /* IDA_FSR_*                                    */
    uint8_t  regime;              /* IDA_REG_*                                    */
    uint8_t  phase;               /* IDA_PH_*                                     */
    uint8_t  mission_state;       /* IDA_MS_*                                     */
    uint16_t mission_item;        /* yürütülen öğe indisi                         */
    bool     param_ok;            /* geçerli parametre kümesi yüklü (A4-K78)      */
    bool     events_ok;           /* olay kimliği kullanılabilir (A4-K61, K62)    */
    uint32_t boot_count;          /* init'te verilen                              */
    ida_core_counters_t cnt;
} ida_status_t;

/* A4-K89, A4-K102, A4-K103: sağlık bitleri. */
typedef struct {
    bool core_ok;                 /* iç tutarlılık; yalnız bu gözetimi besler     */
    bool data_ok;                 /* taze IMU ve oturum; yalnız bildirim          */
    bool imu_rejected;            /* bu çağrının IMU örneği reddedildi            */
    bool imu_lag;                 /* IMU_LAG mandalı kurulu (A4-K37)              */
    bool input_dropped;           /* bu çağrının girdisinde üye düştü (K102)      */
    bool narrowed;                /* bu çağrıda daraltma uygulandı (A4-K45, K113) */
    bool sample_conflict;         /* bu çağrıda çelişkili eşit anahtar (A4-K115)  */
    bool bg_bad_input;            /* önceki step/tick'ten beri IDA_BG_BAD_INPUT (A4-K117) */
} ida_health_t;

/* Günlük önem düzeyleri (MAVLink MAV_SEVERITY sırası). */
#define IDA_SEV_EMERGENCY  0u
#define IDA_SEV_ALERT      1u
#define IDA_SEV_CRITICAL   2u
#define IDA_SEV_ERROR      3u
#define IDA_SEV_WARNING    4u
#define IDA_SEV_NOTICE     5u
#define IDA_SEV_INFO       6u
#define IDA_SEV_DEBUG      7u

/* A4-K90 (W1-uyum U1): günlük kodları; kapalı tablo, belge Bölüm 3.15. Tabloda
   olmayan kod üretilmez; arg0/arg1 anlamı koda göre tablodadır (kullanılmayan 0). */
#define IDA_LOG_RESET_INFO          1u  /* arg0 reset_cause, arg1 consecutive_faults */
#define IDA_LOG_PARAM_LOAD          2u  /* arg0 init dönüş kodu, arg1 başarısız denetim */
#define IDA_LOG_PARAM_COMMIT        3u  /* arg0 1 uygulandı / 0 reddedildi, arg1 neden */
#define IDA_LOG_PARAM_SAVE          4u  /* arg0 gen, arg1 param_store                  */
#define IDA_LOG_MODE_CHANGE         5u  /* arg0 yeni kip, arg1 önceki kip             */
#define IDA_LOG_ARM_CHANGE          6u  /* arg0 armed, arg1 neden (IDA_REASON_*)       */
#define IDA_LOG_FAILSAFE_CHANGE     7u  /* arg0 yeni fs_level, arg1 fs_reasons         */
#define IDA_LOG_SESSION             8u  /* arg0 link_event, arg1 0                     */
#define IDA_LOG_IMU_GAP             9u  /* arg0 boşluk µs (doyar), arg1 0              */
#define IDA_LOG_IMU_LAG            10u  /* arg0 1 kuruldu / 0 çözüldü, arg1 lag µs      */
#define IDA_LOG_IMU_STALE          11u  /* arg0 1 bayat / 0 taze, arg1 0               */
#define IDA_LOG_WALL_BACKSTEP      12u  /* arg0 geri adım µs (doyar), arg1 0           */
#define IDA_LOG_BG_SLICE_MISMATCH  13u  /* arg0 platform sayısı, arg1 çekirdek sayısı  */
#define IDA_LOG_BG_BAD_INPUT       14u  /* arg0 önceki çağrıdan beri sayı, arg1 0      */
#define IDA_LOG_SAMPLE_CONFLICT    15u  /* arg0 type, arg1 inst                        */
#define IDA_LOG_DET_STREAM         16u  /* arg0 1 bayat / 0 taze, arg1 inst (kamera)    */
#define IDA_LOG_EVENT_RESULT       17u  /* arg0 olay türü, arg1 IDA_EVR_*              */
#define IDA_LOG_EVENT_UNKNOWN      18u  /* arg0 olay türü, arg1 tüp                    */
#define IDA_LOG_SAFETY_INPUT       19u  /* arg0 etkin estop, arg1 etkin tüp bitleri   */
#define IDA_LOG_FC_STATE           20u  /* arg0 yeni fc.state, arg1 önceki fc.state     */
#define IDA_LOG_FC_INHIBIT         21u  /* arg0 yeni inhibit_mask, arg1 önceki          */
#define IDA_LOG_NUMERIC_FAULT      22u  /* arg0 1 kestirim / 2 kontrol, arg1 0          */
#define IDA_LOG_CORE_INTEGRITY     23u  /* arg0 denetim kodu 1–3, arg1 0               */
#define IDA_LOG_CODE_LAST          23u

/* A4-K90: günlük kaydı. */
typedef struct {
    ida_wtime_t t_w;              /* çağrının kullanılan now_w'si                 */
    uint16_t    code;             /* IDA_LOG_*                                    */
    uint8_t     severity;         /* IDA_SEV_*                                    */
    int32_t     arg0;
    int32_t     arg1;
} ida_log_t;

/* A4-K91 (W1-uyum U1): atış kontrol durumu (ida_fc_status_t.state); kapalı tablo. */
#define IDA_FCS_SAFE           0u  /* emniyetli; açık izin yok                     */
#define IDA_FCS_ARMED          1u  /* FC silahlı; açık izin yok                    */
#define IDA_FCS_PERMIT_OPEN    2u  /* en az bir tüpte izin açık (A4-K107)           */
#define IDA_FCS_FIRE_PENDING   3u  /* sonucu beklenen FIRE var                      */
#define IDA_FCS_TEST           4u  /* sınama kipi                                   */
#define IDA_FCS_FAULT_LATCHED  5u  /* mandallı arıza; yalnız FC_RESET çıkarır       */

/* A4-K91 (W1-uyum U1): emniyet zinciri engelleri (inhibit_mask); kapalı tablo.
   Bit 18–31 ayrılmış, 0. */
#define IDA_FCI_NOT_ARMED        0x00000001u /* araç silahsız                      */
#define IDA_FCI_MODE             0x00000002u /* kip izin kümesinde değil            */
#define IDA_FCI_EST              0x00000004u /* kestirim düzeyi OK değil            */
#define IDA_FCI_FAILSAFE         0x00000008u /* arıza eylemi HOLD ya da üstü        */
#define IDA_FCI_ESTOP            0x00000010u /* etkin acil durdurma (A4-K116)       */
#define IDA_FCI_PLUG_T0          0x00000020u /* tüp 0 fişi çıkık (etkin değer)       */
#define IDA_FCI_PLUG_T1          0x00000040u /* tüp 1 fişi çıkık                     */
#define IDA_FCI_PIN_T0           0x00000080u /* tüp 0 pimi takılı (etkin değer)      */
#define IDA_FCI_PIN_T1           0x00000100u /* tüp 1 pimi takılı                    */
#define IDA_FCI_EMPTY_T0         0x00000200u /* tüp 0 boş                           */
#define IDA_FCI_EMPTY_T1         0x00000400u /* tüp 1 boş                           */
#define IDA_FCI_EVENTS_OFF       0x00000800u /* olay kimliği kullanılamaz          */
#define IDA_FCI_IMU_LAG          0x00001000u /* A4-K108                             */
#define IDA_FCI_IMU_STALE        0x00002000u /* A4-K39                              */
#define IDA_FCI_SESSION          0x00004000u /* A1 oturumu yok                      */
#define IDA_FCI_RESULT_PENDING   0x00008000u /* sonucu beklenen olay var (A4-K123)  */
#define IDA_FCI_FAULT_LATCHED    0x00010000u /* FC mandallı arıza                   */
#define IDA_FCI_DET_STALE        0x00020000u /* kamera algılama akışı bayat (K118)  */
#define IDA_FCI_DEFINED          0x0003FFFFu

/* A4-K91: atış kontrol durumu (yalnız durum ve emniyet engeli bilgisi). */
typedef struct {
    uint8_t  state;               /* IDA_FCS_*                                    */
    uint32_t inhibit_mask;        /* IDA_FCI_* bitleri                            */
    uint64_t last_event_id;       /* son çıkan olay; yoksa 0                      */
    uint8_t  last_event_result;   /* IDA_EVR_*; sonuç gelmediyse 0                */
} ida_fc_status_t;

/* A4-K94: arka plan raporu. */
typedef struct {
    bool     submitted;           /* bu çağrıda yeni iş gönderildi                */
    uint8_t  dropped;             /* bu çağrıda süresi dolup düşürülen iş         */
} ida_bg_report_t;

/* A4-K27, A4-K28, A4-K53 … A4-K92: step ve tick'in ortak çıktısı. */
typedef struct {
    /* sürekli komut (A4-K53 … A4-K59) */
    bool         thr_valid;
    bool         is_safe;
    bool         armed;
    uint8_t      src;                            /* IDA_SRC_*                    */
    uint8_t      fs_level;                       /* IDA_FS_*                     */
    uint32_t     seq;                            /* A4-K57                       */
    ida_stime_t  t_ref;                          /* A4-K57                       */
    uint32_t     valid_for_ns;                   /* A4-K58                       */
    float        thr_cmd[IDA_PORT_N_THR];        /* A4-K54                       */
    /* tek seferlik olaylar (A4-K60 … A4-K65) */
    uint8_t      n_events;
    ida_event_t  events[IDA_PORT_MAX_EVENTS];
    /* komut sonuçları (A4-K43) */
    uint8_t      n_results;
    ida_cmd_result_t results[IDA_PORT_MAX_RESULTS];
    /* telemetri ve durum (A4-K87 … A4-K92) */
    bool         nav_valid;
    ida_nav_out_t nav;
    ida_status_t status;
    ida_health_t health;
    uint8_t      n_logs;
    ida_log_t    logs[IDA_PORT_MAX_LOGS];
    ida_fc_status_t fc;
    uint8_t      n_det_tx;                       /* <= IDA_PORT_MAX_DET_OUT      */
    ida_det_record_t det_tx[IDA_PORT_MAX_DET_OUT]; /* A4-K92                     */
    ida_bg_report_t bg;                          /* A4-K94                       */
} ida_out_t;

/* ------------------------------------------------------------------ */
/* 10. Başlatma (A4-K77 … A4-K86, A4-K98; Bölüm 3.9)                    */
/* ------------------------------------------------------------------ */

/* Sıfırlama nedeni (ida_init_in_t.reset_cause). */
#define IDA_RESET_UNKNOWN     0u
#define IDA_RESET_POWER_ON    1u
#define IDA_RESET_PIN         2u
#define IDA_RESET_SOFTWARE    3u   /* planlı yeniden başlatma                    */
#define IDA_RESET_WATCHDOG    4u
#define IDA_RESET_FAULT       5u   /* işlemci hata işleyicisi                    */
#define IDA_RESET_BROWNOUT    6u
#define IDA_RESET_SIL_START   7u
#define IDA_RESET_SIL_REBOOT  8u

/* Parametre bloğunun kaynağı. */
#define IDA_PSRC_NONE   0u
#define IDA_PSRC_FLASH  1u
#define IDA_PSRC_SIL    2u   /* SIL koşucusunun A5'ten ürettiği blok */

/* A4-K02, A4-K77, A4-K78, A4-K86 */
typedef struct {
    uint16_t       port_major;          /* IDA_PORT_VERSION_MAJOR (platformun)   */
    uint16_t       port_minor;          /* IDA_PORT_VERSION_MINOR (platformun)   */
    ida_wtime_t    now_w;
    uint32_t       boot_count;          /* A4-K86                                 */
    bool           boot_count_ok;       /* 0: sayaç ayrılamadı ya da tükendi      */
    uint8_t        reset_cause;         /* IDA_RESET_*                            */
    uint32_t       reset_flags;         /* platformun ham bayrakları; yalnız kayıt */
    uint8_t        consecutive_faults;  /* platformun ardışık arıza sayacı        */
    uint8_t        param_src;           /* IDA_PSRC_*                             */
    const uint8_t *param_blob;          /* NULL: blok yok (param_len = 0)         */
    uint32_t       param_len;
} ida_init_in_t;

/* A4-K98 */
typedef struct {
    uint16_t port_major;
    uint16_t port_minor;
    uint32_t core_size;                 /* sizeof(ida_core_t)                     */
    uint32_t core_align;                /* _Alignof(ida_core_t)                   */
    uint64_t param_schema_hash;         /* A4-K79                                 */
    uint32_t core_version;              /* çekirdek uygulamasının sürümü          */
} ida_core_info_t;

/* Parametre sınıfları (ida_param_desc_t.cls). */
#define IDA_PCLS_A  1u   /* çalışırken değişebilir                            */
#define IDA_PCLS_B  2u   /* yalnız devre dışıyken (A4-K81)                    */

/* A4-K85 */
typedef struct {
    uint16_t id;
    uint8_t  type;                      /* IDA_PT_*                               */
    uint8_t  cls;                       /* IDA_PCLS_*                             */
    char     name[IDA_PARAM_NAME_MAX + 1]; /* NUL ile biter                       */
    ida_param_value_t min;
    ida_param_value_t max;
    ida_param_value_t def;
} ida_param_desc_t;

/* ------------------------------------------------------------------ */
/* 11. Kodlanmış parametre bloğu (A4-K79; bayt düzeni Bölüm 3.10)       */
/* ------------------------------------------------------------------ */

#define IDA_PBLOB_MAGIC        UINT32_C(0x50414449)  /* "IDAP", küçük uçlu */
#define IDA_PBLOB_FMT_VER      1u
#define IDA_PBLOB_HDR_SIZE     24u
#define IDA_PBLOB_REC_SIZE     8u

/* ------------------------------------------------------------------ */
/* 12. İşlevler (önkoşullar ve dönüşler: Bölüm 4.15, A4-K101)           */
/* ------------------------------------------------------------------ */

/* A4-K98: çekirdek örneği. Tam tip çekirdek uygulamasının başlığındadır;
   A4 yalnız adını ve sahiplik kuralını sabitler. */
typedef struct ida_core ida_core_t;

/* A4-K02, A4-K98: örneksiz; info NULL ise hiçbir şey yapmaz. */
void ida_core_info(ida_core_info_t *info);

/* A4-K02, A4-K77, A4-K78, A4-K86, A4-K101: örneği kurar ya da yeniden kurar.
   Dönüş: OK, PARAM_DEFAULTS (kuruldu, arm engelli), VERSION, BAD_INPUT. */
ida_rc_t ida_core_init(ida_core_t *c, const ida_init_in_t *in);

/* A4-K16 … A4-K29, A4-K103: IMU içeren partinin çağrısı.
   Dönüş: OK, IMU_REJECTED, DUP_SEQ, OLD_SEQ, BAD_INPUT, NOT_INIT. */
ida_rc_t ida_core_step(ida_core_t *c, const ida_in_t *in, ida_out_t *out);

/* A4-K16 … A4-K29, A4-K56: IMU'suz partinin ya da ızgaranın çağrısı.
   Dönüş: OK, DUP_SEQ, OLD_SEQ, BAD_INPUT, NOT_INIT. */
ida_rc_t ida_core_tick(ida_core_t *c, const ida_in_t *in, ida_out_t *out);

/* A4-K93 … A4-K96, A4-K109, A4-K110, A4-K117: arka plan işinin bir dilimi.
   Sıra (belge 3.11): c NULL ise IDA_BG_BAD_INPUT; örnek kurulmamışsa IDA_BG_IDLE
   (hiçbir şey yazılmaz); now_w aralık dışıysa IDA_BG_BAD_INPUT. İlk ikisinde ve
   üçüncüsünde iş yapılmaz. */
ida_bg_rc_t ida_core_background(ida_core_t *c, ida_wtime_t now_w);

/* A4-K85: parametre tablosu (örneksiz, salt okunur). */
uint16_t ida_core_param_count(void);
ida_rc_t ida_core_param_desc(uint16_t index, ida_param_desc_t *d);
ida_rc_t ida_core_param_find(const char *name, uint16_t *id);

/* A4-K85, A4-K20: örneğin yürürlükteki değeri; örneği değiştirmez. */
ida_rc_t ida_core_param_get(const ida_core_t *c, uint16_t id, ida_param_value_t *v);

#ifdef __cplusplus
}
#endif

#endif /* IDA_PORT_H */
