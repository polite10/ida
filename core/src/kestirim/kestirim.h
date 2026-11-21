/*
 * kestirim.h — seyrüsefer kestirimi (W3 kestirim, dilim 1: yönelim, derinlik, dikey hız).
 *
 * Yöntem 02 §2.3: hata durumlu (çarpımsal) genişletilmiş Kalman filtresi, 15 hata durumu
 * (δp, δv, δθ, δb_g, δb_a; tutum hatası sağdan yerel: q = q̂ ⊗ Exp(δθ)), sabit gecikmeli füzyon
 * ufku ve çıkış öngörücüsü (02 §2.3.2), sıralı skaler güncelleme (02 §2.3.4). Dilimin
 * ölçümleri: basınç (derinlik), manyetometre (eğim telafili yön) ve ivmeölçerden yerçekimi
 * yönü (yüksek ivmede kısılır). DVL, GNSS ve Ping1D bu dilimde kullanılmaz; yatay hız ve konum
 * yalnız IMU ile yayılır ve belirsizlikleri büyür. Bu dilimde her sensör türünün yalnız inst = 0
 * örneği kullanılır; öteki örnek numaraları sayılır ve akışı canlı saymaz.
 *
 * Çağrı çerçevesi kancaları sabit sırayla çağırır (core/TASARIM.md §2): ida_kes_imu ve
 * ida_kes_ornek yalnız örneği saklar; bütün işlem parametrelerin görüldüğü ida_kes_adim /
 * ida_kes_tick'tedir. Durum ida_kes_t'dedir; değişebilir statik nesne yoktur (A4-K22).
 */
#ifndef IDA_CORE_KESTIRIM_H
#define IDA_CORE_KESTIRIM_H

#include <stdbool.h>
#include <stdint.h>

#include "ida/port.h"
#include "mat/mat.h"
#include "param/param.h"

#define IDA_KES_N     15u /* hata durumu boyutu */
#define IDA_KES_IMU_N 48u /* IMU halkası: D en çok 200 ms = 40 kayıt (200 Hz) + pay */
#define IDA_KES_OLC_N 24u /* bekleyen ölçüm kuyruğu */

/* Hata durumu dizinleri. */
#define IDA_KES_IP  0u
#define IDA_KES_IV  3u
#define IDA_KES_ITH 6u
#define IDA_KES_IBG 9u
#define IDA_KES_IBA 12u

/* Geçerlilik bitleri (ida_kes_gecerlilik). Bir bit, o büyüklüğün şu an bir ölçümle
   desteklendiğini ve belirsizliğinin parametre sınırı içinde olduğunu söyler. */
#define IDA_KES_G_TUTUM     0x01u /* yuvarlanma ve yunuslama                       */
#define IDA_KES_G_YON       0x02u /* yön (manyetometre taze ve tutarlı)            */
#define IDA_KES_G_DERINLIK  0x04u /* derinlik (basınç taze, kalibre ve tutarlı)    */
#define IDA_KES_G_DIKEY_HIZ 0x08u /* dikey hız (derinlikle aynı destek)            */
#define IDA_KES_G_YATAY_HIZ 0x10u /* yatay hız (DVL/GNSS; bu dilimde hep 0)        */
#define IDA_KES_G_KONUM     0x20u /* yatay konum (bu dilimde hep 0)                */

/* Nominal durum (02 §2.3.1): p (NED, O_b), v, q (gövde → NED), b_g, b_a. */
typedef struct {
    ida_v3_t p;
    ida_v3_t v;
    ida_q_t q;
    ida_v3_t bg;
    ida_v3_t ba;
} ida_kes_nominal_t;

/* IMU halkasının kaydı: aralığın sonu, süresi, gövde çerçevesine döndürülmüş ham değerler
   (ivme O_b'ye taşınmış) ve çıkış öngörücüsünün kaydın sonundaki durumu. */
typedef struct {
    int64_t t_ns;
    float dt;       /* s; önceki kaydın sonundan bu kaydın sonuna */
    ida_v3_t w;     /* rad/s, gövde, sapma çıkarılmamış */
    ida_v3_t f;     /* m/s², gövde, O_b'de özgül kuvvet, sapma çıkarılmamış */
    ida_q_t q_o;
    ida_v3_t v_o;
    ida_v3_t p_o;
} ida_kes_imu_kayit_t;

/* Bekleyen ölçüm: geçerlilik anına göre sıralı; değer sensör çerçevesinde ham. */
typedef struct {
    int64_t t_ns;
    uint8_t tur;    /* IDA_SENS_MAG ya da IDA_SENS_BARO */
    uint8_t sira;   /* eşit zamanda sabit sıra: basınç, DVL, manyetometre, GNSS (02 §2.3.2) */
    float v[3];     /* MAG: alan (T); BARO: v[0] = p_abs (Pa) */
} ida_kes_olc_t;

/* Durağan başlangıç hizalaması (02 §2.3.1). */
typedef struct {
    uint32_t n;        /* biriken IMU örneği */
    int64_t t0_ns;     /* birikimin başı */
    int64_t t_son_ns;  /* son IMU'nun sonu */
    ida_v3_t w_ort;    /* gövde ω'nın yürüyen ortalaması (taşmaz) */
    ida_v3_t f_ort;    /* gövde f'nin yürüyen ortalaması */
    ida_v3_t m_ort;    /* gövde manyetik alanın ortalaması (norm kapısını geçenler) */
    uint32_t m_n;
    float b_ort;       /* basınç ortalaması (fiziksel aralıktakiler), Pa */
    uint32_t b_n;
} ida_kes_hiz_t;

/* Sürekli ret izi: retlerin başladığı duvar zamanı. */
typedef struct {
    bool var;
    int64_t bas_ns;
} ida_kes_red_t;

/* Bir sensör akışının izi (A4-K36). Akış tazeliği: son kullanılabilir örneğin alım yaşı ve
   ölçüm zamanının ilerlemesi (1 s pencerede Δt_meas / Δt_rx ≥ 0,5; 02 §4.1). Ölçüm desteği:
   son füzyonlanan (bütün kapılardan geçen) ölçümün duvar zamanı; geç, taşmada düşen ya da
   kapıda elenen örnek desteği yenilemez. */
typedef struct {
    bool var;
    int64_t rx_ns;
    bool pen_var;
    int64_t pen_rx_ns;
    int64_t pen_tm_ns;
    bool durmus;
    bool destek_var;
    int64_t destek_ns;
} ida_kes_akis_t;

typedef struct {
    /* Genel durum */
    bool hizali;        /* filtre hizalandı (INIT değil)                              */
    bool ariza;         /* sayısal çökme mandalı (EST_FAULT)                          */
    bool yon_var;       /* yön en az bir kez manyetometreden kuruldu                  */
    bool kalibre;       /* basınç referansı (P_atm) kuruldu; oturum boyunca korunur  */
    uint8_t duzey;      /* IDA_EST_*                                                   */
    uint8_t suzgec_duzey; /* çökme mandalından bağımsız süzgeç düzeyi (INIT, OK,
                             DEGRADED, BAD); ida_kes_suzgec_duzey                 */
    uint8_t gecerlilik; /* IDA_KES_G_*                                                 */
    float p_atm;        /* Pa                                                          */
    int64_t d_ns;       /* ufuk gecikmesi D (hizalamada sabitlenir)                   */

    /* Bu çağrının IMU örneği (ida_kes_imu → ida_kes_adim). */
    bool imu_bekliyor;
    int64_t imu_t_ns;
    uint32_t imu_win_ns;
    uint16_t imu_bayrak; /* hdr.flags */
    bool imu_bayrakli;  /* son IMU SATURATED ya da SENSOR_FAULT: yayılım yapılmadı */
    ida_v3_t imu_w;     /* sensör çerçevesi */
    ida_v3_t imu_f;
    bool imu_son_var;
    int64_t imu_son_ns; /* son işlenen IMU'nun t_meas'ı */

    ida_kes_hiz_t hz;

    /* Füzyon ufku: nominal durum, kovaryans, zaman. */
    ida_kes_nominal_t xh;
    float P[IDA_KES_N][IDA_KES_N];
    int64_t th_ns;
    ida_v3_t g_top;     /* yerçekimi ölçümü için f birikimi (f·dt) */
    float g_sure;       /* s */

    /* Çıkış öngörücüsü (şimdiki zaman). */
    ida_q_t q_o;
    ida_v3_t v_o;
    ida_v3_t p_o;
    ida_v3_t w_o;       /* son IMU'nun sapması çıkarılmış ω'sı */
    int64_t to_ns;
    bool cikis_gec;     /* çıkış durumu t_ref'in 100 ms'den fazla gerisinde            */
    ida_v3_t sig_p;     /* çıkış zamanı için muhafazakâr 1σ (gecikme boyunca büyütülmüş) */
    ida_v3_t sig_v;
    ida_v3_t sig_th;
    ida_v3_t sig_bg;    /* nav'ın t_ref'e taşınmasında tutum σ'sının artış hızı         */
    ida_v3_t art_p;     /* çıkışın ufuktan henüz kapanmamış farkı (mutlak)              */
    ida_v3_t art_v;
    ida_v3_t art_th;
    uint8_t reset_sayac; /* nav.reset_counter: konum sıfırlamasında artar, sarar       */

    /* IMU halkası ve ölçüm kuyruğu. */
    ida_kes_imu_kayit_t imu[IDA_KES_IMU_N];
    uint8_t imu_bas;
    uint8_t imu_n;
    ida_kes_olc_t olc[IDA_KES_OLC_N];
    uint8_t olc_n;

    /* Akış ve destek izleri (A4-K36). */
    ida_kes_akis_t akis_mag;
    ida_kes_akis_t akis_baro;
    bool grav_destek_var;
    int64_t grav_destek_ns;

    /* Sürekli ret izleri (NIS kapısı). */
    ida_kes_red_t red_grav;
    ida_kes_red_t red_baro;
    ida_kes_red_t red_mag;

    /* Bilgi sayaçları (iç; telemetriye bu dilimde çıkmaz). */
    uint32_t n_gec;     /* ufka göre geç kalıp atılan ölçüm       */
    uint32_t n_tasma;   /* kuyruk dolu olduğu için atılan ölçüm   */
    uint32_t n_cokme;   /* sayısal çökme                          */
    uint32_t n_hizalama;/* tamamlanan hizalama                    */
    uint32_t n_yon_sifir;/* manyetometreyle yön sıfırlaması       */
    uint32_t n_derinlik_sifir; /* basınçla derinlik sıfırlaması   */
    uint32_t n_inst;    /* inst ≠ 0 olduğu için kullanılmayan örnek */
    uint32_t n_geri;    /* ölçüm zamanı geri giden IMU (işlenmedi)  */
    uint32_t n_bayrak;  /* bayraklı IMU                             */
} ida_kes_t;

void ida_kes_kur(ida_kes_t *k);
/* SESSION_START'tan sonraki ilk kabul edilen çağrının (e) adımında (A4-K72). */
void ida_kes_oturum_basi(ida_kes_t *k);
/* Kabul edilen IMU (yalnız step OK). */
void ida_kes_imu(ida_kes_t *k, const ida_imu_sample_t *imu);
/* Kabul edilen MAG, BARO, GNSS, DVL, PING1D örnekleri, A4-K35 sırasıyla. */
void ida_kes_ornek(ida_kes_t *k, const ida_sample_t *s);
/* step OK'ta (IMU ile yayılım) ve tick/IMU_REJECTED'de (yayılımsız) çağrılır. */
void ida_kes_adim(ida_kes_t *k, const ida_core_t *c);
void ida_kes_tick(ida_kes_t *k, const ida_core_t *c);
uint8_t ida_kes_duzey(const ida_kes_t *k); /* IDA_EST_* */
/* step OK'taki nav (A4-K87); t = çıktının t_ref'i. */
void ida_kes_nav(const ida_kes_t *k, ida_stime_t t, ida_nav_out_t *nav);
bool ida_kes_tutarli(const ida_kes_t *k);
uint8_t ida_kes_param_grup(const ida_prm_kume_t *p);

/* ---- Öteki modüller için (W3 güdüm, görev, kontrol) ------------------------------------ */
/* IDA_KES_G_* bitleri; son adım/tick'teki değer. */
uint8_t ida_kes_gecerlilik(const ida_kes_t *k);
/* Sayısal çökme mandalını kaldırır (görev modülü FAILSAFE_RESET'te çağırır; 02 §2.3.6).
   Mandal, filtre yeniden hizalanmışsa kalkar; true: kalktı. */
bool ida_kes_ariza_temizle(ida_kes_t *k);
/* Süzgecin çökme mandalından bağımsız düzeyi: hizalanmamışsa INIT, hizalanmışsa sağlığına
   göre OK, DEGRADED ya da BAD (mandal yokken ida_kes_duzey ile aynı). Görev modülü
   FAILSAFE_RESET'in sayısal çökme temizleme koşulunu buna bağlar: mandal kuruluyken
   ida_kes_duzey hep FAULT'tur ve yeniden hizalanmış sağlıklı süzgeci göstermez. */
uint8_t ida_kes_suzgec_duzey(const ida_kes_t *k);

#endif /* IDA_CORE_KESTIRIM_H */
