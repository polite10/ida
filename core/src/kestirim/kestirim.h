/*
 * kestirim.h — seyrüsefer kestirimi (W3 kestirim dilim 1: yönelim, derinlik, dikey hız; W5
 * kestirim-yatay dilim 2: yatay hız, konum, irtifa, düzey histerezisi).
 *
 * Yöntem 02 §2.3: hata durumlu (çarpımsal) genişletilmiş Kalman filtresi, 15 hata durumu
 * (δp, δv, δθ, δb_g, δb_a; tutum hatası sağdan yerel: q = q̂ ⊗ Exp(δθ)), sabit gecikmeli füzyon
 * ufku ve çıkış öngörücüsü (02 §2.3.2), sıralı skaler güncelleme (02 §2.3.4). Ölçümler: basınç
 * (derinlik), manyetometre (eğim telafili yön), ivmeölçerden yerçekimi yönü (yüksek ivmede
 * kısılır), DVL dip izi hızı (gövde çerçevesinde üç eksen) ve GNSS (yatay konum ve yatay hız;
 * yalnız yüzeyde). Durum vektörü dilim 2'de büyümedi: yatay hız ve konum zaten durumdur.
 * Ping1D menzili ve DVL irtifası filtreye girmez; yalnız nav.altitude'u üretir. Her sensör
 * türünün yalnız inst = 0 örneği kullanılır; öteki örnek numaraları sayılır ve akışı canlı
 * saymaz. Hedef algılama, takip ve atış kontrol bu modülde yoktur; sonar ve kamera algılama
 * kayıtları kullanılmaz.
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
#define IDA_KES_OLC_N 40u /* bekleyen ölçüm kuyruğu (manyetometre, basınç; seyreltilmiş): D = 200 ms
                             ve en kısa aralıklarda 25 + 12 = 37 kayıt + pay (kes_ic.h) */
#define IDA_KES_YOLC_N 8u /* bekleyen yatay yardım ölçümü kuyruğu (DVL, GNSS; seyreltilmiş) */

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
#define IDA_KES_G_YATAY_HIZ 0x10u /* yatay hız (bir kaynak destekliyor; etkin hiçbir kaynak
                                     ret altında değil)                             */
#define IDA_KES_G_KONUM     0x20u /* yatay konum (GNSS'ten kuruldu, σ sınır içinde) */

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

/* Bekleyen yatay yardım ölçümü (DVL ya da GNSS); değerler sensörün verdiği gibi ham. Ayrı
   kuyruktadır: yüksek hızlı manyetometre ve basınç akışı bu ölçümlerin yerini dolduramaz. */
#define IDA_KES_YB_KULLANILMAZ 0x01u /* başlıkta SATURATED ya da SENSOR_FAULT          */
#define IDA_KES_YB_HIZ         0x02u /* DVL: VEL_VALID                                 */
#define IDA_KES_YB_DIP         0x04u /* DVL: TRACK_BOTTOM                              */
#define IDA_KES_YB_KOV         0x08u /* DVL: COV_PRESENT                               */
#define IDA_KES_YB_TANIMLI     0x0Fu

typedef struct {
    int64_t t_ns;
    uint8_t tur;    /* IDA_SENS_DVL ya da IDA_SENS_GNSS */
    uint8_t sira;   /* eşit zamanda sabit sıra: DVL 1, GNSS 3 (02 §2.3.2) */
    uint8_t bayrak; /* IDA_KES_YB_* */
    uint8_t fix;    /* GNSS: fix_type */
    int32_t lat_e7; /* GNSS */
    int32_t lon_e7;
    float v[11];    /* DVL: vel[3] (m/s, DVL çerçevesi), cov[6], fom, altitude;
                       GNSS: vel_ned[0], vel_ned[1] (m/s), eph (m) */
} ida_kes_yolc_t;

/* Bir yardım kaynağının nitelendirme izi: ardışık iyi örnek ve ardışık kapı reddi sayıları
   (ölçüm zamanında; ufukta, zaman sırasıyla güncellenir). */
typedef struct {
    bool son_var;
    int64_t son_ns;     /* son iyi örneğin ölçüm zamanı (ardışıklığın boşluk denetimi)  */
    int64_t red_bas_ns; /* ret dizisinin ilk örneğinin ölçüm zamanı (red_n > 0 iken)    */
    uint8_t iyi_n;      /* ardışık iyi örnek; 255'te doyar                              */
    uint8_t red_n;      /* ardışık kapı reddi (nitelikli örneklerde); 255'te doyar      */
} ida_kes_nit_t;

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
   kapıda elenen örnek desteği yenilemez; sıfırlama da yenilemez (yatay kaynaklarda). Destek
   zamanı ölçümün değil füzyon anının zamanıdır: ölçüm zamanına göre bayatlık sınırı en çok kuyruk
   gecikmesi (ufuk gecikmesi D ≤ 200 ms) ve taşıma süresi kadar uzar (inceleme bulgu 6). */
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

    /* ---- Yatay hız, konum ve irtifa (W5 kestirim-yatay) ---------------------------------- */
    ida_kes_yolc_t yolc[IDA_KES_YOLC_N];
    uint8_t yolc_n;
    bool konum_var;     /* yatay konum GNSS'ten kuruldu ve o günden beri bilinir sayılıyor;
                           hizalama, orijin değişimi ve başvurusuz hız sıfırlaması düşürür      */
    bool dvl_kuy_var;   /* seyreltme: kuyruğa alınan son örneğin ölçüm zamanı (tür başına)    */
    int64_t dvl_kuy_ns;
    bool gnss_kuy_var;
    int64_t gnss_kuy_ns;
    bool mag_kuy_var;
    int64_t mag_kuy_ns;
    bool baro_kuy_var;
    int64_t baro_kuy_ns;
    ida_kes_nit_t nit_dvl;
    ida_kes_nit_t nit_gnss; /* red_n: konum kapısı */
    ida_kes_nit_t nit_gnss_v; /* yalnız red_n ve red_bas_ns: GNSS hız kapısı */
    ida_kes_akis_t akis_dvl;  /* destek: füzyonlanan DVL hızı                               */
    ida_kes_akis_t akis_gnss; /* destek: füzyonlanan GNSS konumu                            */
    ida_kes_akis_t akis_ping; /* yalnız canlılık (irtifa)                                   */
    bool gnss_hiz_destek_var; /* füzyonlanan GNSS hızı (duvar zamanı)                       */
    int64_t gnss_hiz_destek_ns;
    ida_kes_red_t red_dvl;    /* ret izleri: yalnız kabul edilen örnekte ya da sıfırlamada      */
    ida_kes_red_t red_gnss_p; /* temizlenir (yeniden nitelendirme silmez)                       */
    ida_kes_red_t red_gnss_v;
    /* Kuşku: kaynak KES_N_RED ardışık ret aldı ya da durum sıfırlandı (konum sıfırlaması:
       kusku_gnss_p; hız sıfırlaması, hangi kaynağa yapılırsa yapılsın: kusku_dvl ve
       kusku_gnss_v birlikte); yalnız kaynağın kendi kapıdan geçen örneği kaldırır. Kuşkulu
       kaynak desteklemez; etkinken yatay hız geçerliliğini düşürür (kusku_gnss_p: konum
       geçerliliğini). */
    bool kusku_dvl;
    bool kusku_gnss_p;
    bool kusku_gnss_v;
    /* Son kapı kararının (kabul ya da ret; nitelikli örnekte) duvar zamanı: kaynak "etkin"
       (canlı ve nitelikli) sayılırken tazeliği buradan okunur. GNSS'te hız ve konum kapıları
       aynı örnekte çalışır. */
    bool kapi_dvl_var;
    int64_t kapi_dvl_ns;
    bool kapi_gnss_var;
    int64_t kapi_gnss_ns;
    bool hiz_fuz_var;   /* son yatay hız füzyonunun ölçüm zamanı (ölü hesap büyümesi)        */
    int64_t hiz_fuz_ns;
    bool ok_aday_var;   /* OK koşullarının kesintisiz sağlandığı sürenin başı (duvar zamanı) */
    int64_t ok_aday_ns;
    /* İrtifa kaynakları (son örnek; ham, sensörün kendi noktasından) ve O_b'nin dibe mesafesi.
       Tazelik akış izlerinden (akis_dvl, akis_ping) okunur. */
    bool irt_dvl_var;
    float irt_dvl;      /* m, DVL'den dibe dik mesafe (A4 §3.3, W4-K8)                       */
    bool irt_ping_var;
    float irt_ping;     /* m, Ping1D ışını boyunca mesafe                                    */
    float irtifa;       /* m; bilinmiyorsa −1 (nav.altitude)                                 */
    int32_t orj_lat_e7; /* nav için orijin (ORIJIN_LAT / ORIJIN_LON'un son okunan değeri);
                           değişirse konum geçersiz olur ve yeniden GNSS'ten kurulur           */
    int32_t orj_lon_e7;
    uint32_t n_seyrek;  /* en kısa aralıktan sık geldiği için kuyruğa alınmayan örnek        */
    uint32_t n_dvl_red; /* kapıda reddedilen DVL                                             */
    uint32_t n_gnss_red; /* kapıda reddedilen GNSS konumu ya da hızı                         */
    uint32_t n_hiz_sifir; /* hız sıfırlaması (DVL ya da GNSS)                                */
    uint32_t n_konum_sifir; /* konum sıfırlaması ve sıçraması (reset_sayac ile birlikte)     */
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
