/*
 * W0-MP4a yer tutucu çekirdek adımı (atılabilir ölçüm prototipi; ürün koduna taşınmaz, 09 §2.1).
 *
 * Amaç: 02'nin öngördüğü ida_core_step'in hesap hacmini ve bellek erişim düzenini taklit etmek.
 * Sayısal doğruluk aranmaz. ISO C17, yalnız float; CMSIS ya da HAL başlığı içermez.
 *
 * Yapı (02 §2.2 "Adım içindeki sıra"):
 *   mesaj çözme (CRC-16/MCRF4XX + alan kopyalama) -> kestirim (sabit gecikmeli ufuk, 15 hata durumlu EKF,
 *   ölçüm füzyonu, enjeksiyon/sıfırlama, LDLᵀ denetimi; çıkış öngörücüsü ve geçmiş tamponu düzeltmesi)
 *   -> kip/tazelik/sonluluk -> güdüm (LOS/ILOS, derinlik referans modeli) -> kademeli kontrol (6 eksen)
 *   -> itki tahsisi (8 itici, öncelik gruplu doyma) -> çıkış mesajı kodlama (CRC-16).
 */
#ifndef YT_H
#define YT_H

#include <stdint.h>

#define YT_N            15
#define YT_ITICI        8
#define YT_EKSEN        6
#define YT_IMU_FIFO     80u   /* (D + 50 ms) / 2,5 ms; 02 §2.3.2 */
#define YT_CIKIS_FIFO   80u
#define YT_UFUK_ADIM    60u   /* D = 150 ms = 60 periyot */
#define YT_SENSOR_FIFO  64u   /* sensör başına bekleyen ölçüm halkası */
#define YT_GELEN_BOY    300u
#define YT_GIDEN_BOY    100u
#define YT_PERIYOT_NS   2500000LL
#define YT_UFUK_NS      150000000LL
#define YT_ROTA_NOKTA   8
#define YT_TABLO_N      33

enum { YT_S_BASINC = 0, YT_S_DVL, YT_S_MANYETO, YT_S_GNSS, YT_S_SAYI };

enum {
    YT_P_MESAJ_GIRIS = 0,
    YT_P_NOMINAL,     /* ufuk ve çıkış nominal yayılımı, IMU/çıkış tamponları, çıkış düzeltmesi */
    YT_P_KOVARYANS,   /* Φ, P⁻ = Φ P Φᵀ + Q_d */
    YT_P_FUZYON,      /* H, S, kapı, beyazlatma, sıralı skaler güncellemeler */
    YT_P_SIFIRLAMA,   /* enjeksiyon, G P Gᵀ, simetri, alt sınır, LDLᵀ */
    YT_P_KIP,
    YT_P_GUDUM,
    YT_P_KONTROL,
    YT_P_TAHSIS,
    YT_P_MESAJ_CIKIS,
    YT_P_SAYI
};

/* Yük durumu (görev belgesi md.4 ve rapordaki C yorumu). */
typedef enum { YT_YUK_OLAGAN = 0, YT_YUK_EN_KOTU = 1, YT_YUK_02 = 2, YT_YUK_SAYI } yt_yuk_t;

typedef struct {
    uint8_t her_adim_dis_dongu; /* güdüm ve dış döngüler her adımda (yoksa 10 Hz / 100 Hz) */
    uint8_t her_adim_ldl;       /* LDLᵀ denetimi her adımda (yoksa 100 Hz) */
    uint8_t fuzyon_oncesi_kov;  /* ufukta her füzyon zamanından önce kovaryans yayılımı */
    uint8_t adim_sonu_kov;      /* adım sonunda kalan birikimle kovaryans yayılımı (her adım) */
    uint8_t skaler_sinir;       /* adım başına skaler güncelleme sınırı; 0 = sınırsız */
} yt_ayar_t;

typedef struct { float p[3], v[3], q[4], bg[3], ba[3]; } yt_nom_t;
typedef struct { int64_t t_ns; float w[3], a[3]; } yt_imu_t;
typedef struct { int64_t t_ns; yt_nom_t x; } yt_cikis_kayit_t;

typedef struct {
    int64_t t_ns;
    float z[4];
    float r[4][4];   /* ölçüm kovaryansı (DVL tam 3×3; diğerleri köşegen) */
    int32_t lat_e7, lon_e7;
} yt_olcum_t;

typedef struct {
    yt_olcum_t k[YT_SENSOR_FIFO];
    uint32_t bas, n;
    uint32_t tasma;
} yt_sfifo_t;

typedef struct {
    /* ufuk (filtre) */
    yt_nom_t xh;
    float P[YT_N][YT_N];
    float dx[YT_N];
    int64_t t_h;
    float dt_kov;           /* kovaryans için birikmiş aralık (s) */
    yt_imu_t imu[YT_IMU_FIFO];
    uint32_t imu_son, imu_n;
    yt_sfifo_t sf[YT_S_SAYI];
    /* çıkış öngörücüsü */
    yt_nom_t xo;
    yt_cikis_kayit_t cikis[YT_CIKIS_FIFO];
    uint32_t cikis_son, cikis_n;
    float w_son[3];         /* son IMU açısal hızı (sapmasız) */
    /* sayaçlar */
    uint32_t gec_ornek, kapi_red, kapi_gecti, skaler_say, ldl_hata, kov_say, sifirlama_say, yeniden_hiza;
} yt_ekf_t;

typedef struct {
    /* LOS/ILOS */
    float rota[YT_ROTA_NOKTA][3];
    uint32_t rota_i;
    float y_int;
    float chi_d, d_ref[3], irtifa_f, u_d;
    /* çıkış */
    float sp_kuat[4], sp_derinlik, sp_konum[3];
} yt_gudum_t;

typedef struct {
    float w_sp[3], vz_sp, vxy_sp[2];
    float i_w[3], i_vz, i_vxy[2];
    float d_onceki[3], d_filtre[3];
    float harman;           /* rejim kazanç karışımı 0..1 */
    float kalinti[YT_EKSEN];
    float tau_req[YT_EKSEN];
} yt_kontrol_t;

typedef struct {
    float B[YT_EKSEN][YT_ITICI];
    float Bp[YT_ITICI][YT_EKSEN]; /* ağırlıklı en küçük norm tersi (başlatmada) */
    float tablo_u[YT_TABLO_N], tablo_f[YT_TABLO_N];
    float u[YT_ITICI];
    float tau_a[YT_EKSEN];
    float v_bat_f;
    uint16_t pwm[YT_ITICI];
    uint32_t doyma_say;
} yt_tahsis_t;

/* Çözülmüş gelen mesaj alanları */
typedef struct {
    uint8_t bayrak;          /* bit0 IMU, 1 basınç, 2 DVL, 3 manyeto, 4 GNSS */
    uint8_t seq;
    yt_imu_t imu;
    int64_t t_bas; float basinc;
    int64_t t_dvl; float dvl_v[3], dvl_fom, dvl_irtifa, dvl_kov[6];
    int64_t t_man; float man[3];
    int64_t t_gnss; int32_t lat_e7, lon_e7; float gnss_vn, gnss_ve, gnss_eph, gnss_alt;
    uint8_t kip; float sp[8];
    float v_bat; uint8_t sizinti;
    uint8_t ham[98];
} yt_girdi_t;

typedef struct {
    yt_ayar_t ayar;
    yt_ekf_t e;
    yt_gudum_t g;
    yt_kontrol_t c;
    yt_tahsis_t a;
    yt_girdi_t in;
    uint32_t n_adim;
    uint32_t crc_hata;
    uint8_t kip, izin, saglik;
    uint8_t giden_seq;
    int64_t t_son[YT_S_SAYI];
    /* parça zamanlaması (platformun yt_cevrim()'i ile) */
    uint32_t parca_cyc[YT_P_SAYI];
    uint32_t parca_t0;
} yt_t;

/* Platformun sağladığı çevrim sayacı (kartta DWT->CYCCNT). */
uint32_t yt_cevrim(void);

static inline void yt_parca_bas(yt_t *s) { s->parca_t0 = yt_cevrim(); }
static inline void yt_parca(yt_t *s, int p)
{
    const uint32_t t = yt_cevrim();
    s->parca_cyc[p] += t - s->parca_t0;
    s->parca_t0 = t;
}

void yt_baslat(yt_t *s, yt_yuk_t yuk);
void yt_yuk_ayarla(yt_t *s, yt_yuk_t yuk);
/* Bir kontrol adımı. gelen: YT_GELEN_BOY bayt; giden: YT_GIDEN_BOY bayt yazılır. */
void yt_adim(yt_t *s, const uint8_t *gelen, uint8_t *giden);

/* Sözde ölçüm üreteci (ölçülen bölgenin dışında çağrılır). n: üretilecek adımın sırası. */
void yt_uret(const yt_t *s, yt_yuk_t yuk, uint32_t n, uint8_t *gelen);

/* Ortak yardımcılar */
uint16_t yt_crc16(const uint8_t *b, uint32_t n, uint16_t crc);

#endif
