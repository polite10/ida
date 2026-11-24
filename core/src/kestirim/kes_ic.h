/*
 * kes_ic.h — kestirim modülünün iç işlevleri (yalnız kestirim/ içinden). Sahibi: W3 kestirim.
 *
 * kes_eskf.c: filtre matematiği (yayılım, skaler güncelleme, enjeksiyon, sağlamlık denetimi).
 * kes_ufuk.c: IMU halkası, ölçüm kuyruğu, füzyon ufkunun ilerletilmesi, basınç, manyetometre ve
 *             yerçekimi ölçüm modelleri ve çıkış öngörücüsü.
 * kes_yatay.c: DVL ve GNSS ölçüm modelleri, nitelendirme, konum ve hız sıfırlamaları, yerel
 *             düzlem dönüşümü (W5 kestirim-yatay).
 * kestirim.c: kancalar, parametre görüntüsü, hizalama, sağlık düzeyi, nav.
 */
#ifndef IDA_CORE_KES_IC_H
#define IDA_CORE_KES_IC_H

#include <stdbool.h>
#include <stdint.h>

#include "kestirim/kestirim.h"
#include "mat/mat.h"

/* Bir çağrıda kullanılan parametrelerin float görüntüsü (param tablosundan; birimler SI). */
typedef struct {
    int64_t now_ns;      /* kullanılan duvar saati (A4-K10)                  */
    bool tref_var;       /* bu çağrının zaman çerçevesi kabul edildi          */
    int64_t tref_ns;     /* son kabul edilen tf.t (çıktının t_ref'i)          */
    int64_t d_ns;        /* KES_D_MS                                         */
    float tau_o;
    float hiz_s;
    float hiz_w_max;
    float hiz_a_max;
    ida_v3_t l_imu;
    ida_m3_t m_imu;      /* montaj dönüşü, sensör → gövde                    */
    ida_m3_t m_mag;
    ida_v3_t l_baro;
    float p_atm_prm;
    bool baro_kal;
    float baro_kal_d;
    float rho;
    float g;
    float mag_dec;
    float mag_b;
    float mag_b_tol;
    float q_gyro;
    float q_acc;
    float q_bg;
    float q_ba;
    float r_baro;
    float r_mag;
    float r_grav;
    float grav_esik;
    float kapi_g1;
    float kapi_g3;
    float p0_tilt;
    float p0_yaw;
    float p0_bg;
    float p0_ba;
    float p0_v;
    float p0_p;
    float sig_tilt;
    float sig_yaw;
    float sig_depth;
    float sig_hpos;
    int64_t red_ns;
    int64_t mag_reset_ns;
    int64_t baro_reset_ns;
    int64_t baro_stale_ns;
    int64_t mag_stale_ns;
    int64_t imu_gap_ns;
    /* W5 kestirim-yatay */
    ida_v3_t l_gnss;
    ida_v3_t l_dvl;
    ida_v3_t e_dvl;      /* DVL montaj açıları (roll, pitch, yaw); matris füzyonda kurulur */
    ida_v3_t l_ping;
    int32_t orj_lat_e7;
    int32_t orj_lon_e7;
    float r_dvl;
    float dvl_fom_max;
    float dvl_alt_min;
    float r_gnss_p;
    float gnss_sig_k;
    float r_gnss_v;
    float gnss_eph_max;
    float kapi_g2;
    float yuzey_d;
    float dr_oran;
    float sig_hvel;
    float konum_sicra;
    uint32_t n_iyi;
    uint32_t n_reset;
    uint32_t n_red;      /* KES_N_RED: ardışık retten sonra kaynak kuşkulu */
    int64_t ok_ns;
    int64_t dvl_stale_ns;
    int64_t gnss_stale_ns;
} ida_kes_ayar_t;

/* Parametre görüntüsü (kestirim.c); now_ns = c->cer.now_izi_ns. */
void ida_kes_ayar_oku(const ida_core_t *c, ida_kes_ayar_t *a);

/* Basıncın fiziksel aralığı (Pa; dışındaki örnek kullanılmaz). Tahmin: 0,1 bar – 200 bar. */
#define IDA_KES_BARO_EN_AZ  1.0e4f
#define IDA_KES_BARO_EN_COK 2.0e7f

/* nav'ın t_ref'e taşınabileceği en büyük süre (ns); daha eski çıkış durumu kendi zamanıyla
   bildirilir ve geçerlilik düşer. */
#define IDA_KES_TASIMA_MAX_NS 100000000

/* Adım başına skaler güncelleme sınırı (02 §2.3.2; tahmin). */
#define IDA_KES_BUTCE 8u

/* Ölçümlerin kuyruğa alınmadaki en kısa ölçüm zamanı aralığı (seyreltme; tahmin). Cihazların
   olağan hızları (manyetometre 100 Hz, basınç 25 Hz, GNSS 5 Hz, DVL 2–15 Hz; sim'in adım
   sınırına bağlamasından doğan ± 1,25 ms'lik oynamayla) geçer; daha sık gelen akış (ör. çağrı
   başına bir örnek) seyreltilir: yoksa kuyruk sürekli dolu kalır, boşalan yeri her çağrıda
   sıralamada önce gelen tür alır ve öteki türler hiç füzyonlanmaz; kartın adım süresi de
   büyür. Seyreltilen örnek akışı canlı sayar ama füzyonlanmaz. Bu aralıklarla ve D ≤ 200 ms
   ile bekleyen ölçüm sayısı kuyruk boylarını aşmaz: manyetometre ≤ 200/8 = 25, basınç ≤
   200/18 → 12, toplam 37 ≤ IDA_KES_OLC_N = 40 (testte çağrı sonunda 36 ölçüldü); DVL ≤ 5 + GNSS ≤ 3 = 8 ≤
   IDA_KES_YOLC_N. Teslimi geciken örnekler bekleme süresini uzatmaz (ufka göre geç kalan
   atılır); bütçe yüzünden ufuk geride kalırsa taşan örnek atılır ve sayılır (n_tasma). */
#define IDA_KES_MAG_ARALIK_NS   8000000 /* ≤ 125 Hz */
#define IDA_KES_BARO_ARALIK_NS 18000000 /* ≤ ≈ 55 Hz */
#define IDA_KES_GNSS_ARALIK_NS 90000000 /* ≤ ≈ 11 Hz */
#define IDA_KES_DVL_ARALIK_NS  45000000 /* ≤ ≈ 22 Hz */

/* GNSS konumunun orijinden en büyük enlem ve boylam farkı (1e-7 derece; 0,2° ≈ 22 km).
   Dışındaki örnek kullanılmaz: yerel düzlem dönüşümünün (ida_kes_gnss_ne) üçüncü mertebe
   hatası bu uzaklıkta ≈ 0,3 m'dir (tahmin; testte 5 km'ye kadar ölçülür). */
#define IDA_KES_GNSS_UZAK_E7 2000000

/* Ping1D: bu güvenin altındaki örnek irtifa vermez (%; tahmin). */
#define IDA_KES_PING_GUVEN_EN_AZ 50u
/* Ping1D: ışın ekseninin düşeyle kosinüsü bunun altındaysa irtifa verilmez (≈ 37°; tahmin). */
#define IDA_KES_PING_COS_EN_AZ 0.8f

/* ---- kes_eskf.c ------------------------------------------------------------------------- */
/* ns farkı → s (float). Farklar en çok birkaç saniyedir; int64 → float dönüşümü açıktır. */
float ida_kes_ns_s(int64_t ns);
/* Euler (ZYX; yuvarlanma, yunuslama, sapma) → birim kuaterniyon. */
ida_q_t ida_kes_q_euler(float roll, float pitch, float yaw);
/* Birim kuaterniyonun logaritması (dönme vektörü, rad); w < 0 ise önce işaret çevrilir. */
ida_v3_t ida_kes_q_log(ida_q_t q);
/* Nominal yayılım (02 §2.3.1): w, f ham (sapma burada çıkarılır), dt s, g m/s². */
void ida_kes_nominal_yay(ida_kes_nominal_t *x, ida_v3_t w, ida_v3_t f, float dt, float g);
/* Kovaryans yayılımı P ← Φ P Φᵀ + Q_d (birinci mertebe; 02 §2.3.1). x yayılımdan önceki
   nominal durum, w ve f ham. */
void ida_kes_P_yay(float P[IDA_KES_N][IDA_KES_N], const ida_kes_nominal_t *x, ida_v3_t w,
                   ida_v3_t f, float dt, const ida_kes_ayar_t *a);
/* Sıralı skaler güncelleme (02 §2.3.4): y_j ← y − h·δx; kapı (gamma > 0 ise) y_j²/s ≤ gamma;
   geçerse δx ← δx + k y_j ve P ← P − k uᵀ − u kᵀ + s k kᵀ. Kapıyı geçemezse ya da s ≤ 0 ise
   false (P ve δx değişmez). nis: y_j²/s (s ≤ 0'da yazılmaz). */
bool ida_kes_skaler(float P[IDA_KES_N][IDA_KES_N], const float h[IDA_KES_N], float y, float r,
                    float gamma, float dx[IDA_KES_N], float *nis);
/* δx'i nominal duruma ekler, G ile P'yi sıfırlar (02 §2.3.4), simetrikleştirir, alt sınırı
   uygular. */
void ida_kes_enjekte(ida_kes_nominal_t *x, float P[IDA_KES_N][IDA_KES_N], const float dx[IDA_KES_N]);
/* Simetrikleştirme ve köşegen alt sınırı. */
void ida_kes_P_duzelt(float P[IDA_KES_N][IDA_KES_N]);
/* Sayısal sağlamlık (02 §2.3.6): nominal durum sonlu, kuaterniyon birimli, köşegen pozitif ve
   sonlu, ölçeklenmiş LDLᵀ ayrıştırmasının bütün pivotları > ε (ufkun durumu ve P'si). */
bool ida_kes_saglam(const ida_kes_t *k);
/* 3×3 simetrik pozitif tanımlı S için yᵀ S⁻¹ y (Cholesky); S pozitif tanımlı değilse false. */
bool ida_kes_nis3(float S[3][3], const float y[3], float *nis);
/* 3×3 simetrik S = C Cᵀ (alt üçgen C; yalnız S'nin alt üçgeni okunur). c: c00, c10, c11, c20,
   c21, c22. S pozitif tanımlı ve sonlu değilse false. */
bool ida_kes_chol3(float S[3][3], float c[6]);

/* ---- kes_ufuk.c ------------------------------------------------------------------------- */
/* Ufku ve çıkışı x'ten başlatır (hizalama), halkayı boşaltır. */
void ida_kes_ufuk_kur(ida_kes_t *k, const ida_kes_nominal_t *x, int64_t t_ns);
/* Yeni IMU kaydı (gövde çerçevesinde, O_b'ye taşınmış): çıkış öngörücüsünü yayar ve halkaya
   koyar; halka doluysa önce en eski kayıt ufukta işlenir. */
void ida_kes_ufuk_imu(ida_kes_t *k, int64_t t_ns, float dt, ida_v3_t w, ida_v3_t f,
                      const ida_kes_ayar_t *a);
/* Ufku t_imu − D'ye kadar ilerletir; bekleyen ölçümleri zamanlarında füzyonlar; çıkış
   öngörücüsünü düzeltir. */
void ida_kes_ufuk_ilerle(ida_kes_t *k, const ida_kes_ayar_t *a);
/* Ölçümü kuyruğa koyar (sıralı); kuyruk doluysa atar ve sayar. */
void ida_kes_olc_ekle(ida_kes_t *k, const ida_kes_olc_t *o);
/* Kuyruğun başındakini atar. */
void ida_kes_olc_cikar(ida_kes_t *k);
/* Yatay yardım ölçümünü (DVL, GNSS) kendi kuyruğuna koyar (sıralı); doluysa atar ve sayar. */
void ida_kes_yolc_ekle(ida_kes_t *k, const ida_kes_yolc_t *o);

/* ---- kes_yatay.c ------------------------------------------------------------------------ */
/* Ölçüm modelleri: x'teki öngörü ve hata durumuna göre Jacobian (kabuller kes_yatay.c başında).
   w_ham: ham açısal hız (sapma burada çıkarılır); l: O_b'den sensöre kol, gövde çerçevesinde. */
/* DVL, gövde çerçevesinde üç eksen: hb = R̂ᵀ v̂ + ω̂ × l. */
void ida_kes_dvl_model(const ida_kes_nominal_t *x, ida_v3_t w_ham, ida_v3_t l, float hb[3],
                       float h[3][IDA_KES_N]);
/* GNSS yatay hız (kuzey, doğu): hv = (v̂ + R̂ (ω̂ × l))_NE. */
void ida_kes_gnss_hiz_model(const ida_kes_nominal_t *x, ida_v3_t w_ham, ida_v3_t l, float hv[2],
                            float h[2][IDA_KES_N]);
/* GNSS yatay konum (kuzey, doğu): hp = (p̂ + R̂ l)_NE. */
void ida_kes_gnss_konum_model(const ida_kes_nominal_t *x, ida_v3_t l, float hp[2],
                              float h[2][IDA_KES_N]);
/* Ufuk o.t_ns anındayken DVL ya da GNSS ölçümünü işler: nitelik, kapı, füzyon, sıfırlama.
   w_ham: ölçüm anını içeren IMU kaydının ham açısal hızı (sapma burada çıkarılır). */
void ida_kes_dvl_fuzyon(ida_kes_t *k, const ida_kes_ayar_t *a, const ida_kes_yolc_t *o,
                        ida_v3_t w_ham);
void ida_kes_gnss_fuzyon(ida_kes_t *k, const ida_kes_ayar_t *a, const ida_kes_yolc_t *o,
                         ida_v3_t w_ham);
/* WGS-84 enlem ve boylamından (1e-7 derece) orijine göre yerel teğet düzlem kuzey ve doğu
   koordinatları (m; NED'in ilk iki bileşeni). İkinci mertebeye kadar:
     n₀ = M Δφ, e₀ = N cos φ₀ Δλ,
     n = n₀ + e₀² tan φ₀ / (2N) + (3/2) e² sin φ₀ cos φ₀ n₀² / ((1 − e² sin² φ₀) M),
     e = e₀ − e₀ n₀ tan φ₀ / N
   (M, N: orijin enlemindeki meridyen ve doğu-batı eğrilik yarıçapları). Farklar int64'te
   alınır, boylam farkı ±180°'ye sarılır (02 §2.3.4). Fark IDA_KES_GNSS_UZAK_E7'yi aşarsa ya da
   |φ₀| > 89° ise false. */
bool ida_kes_gnss_ne(int32_t lat_e7, int32_t lon_e7, int32_t lat0_e7, int32_t lon0_e7, float ne[2]);
/* Ölü hesap büyümesi: yatay hız ölçümle destekliyken (son hız füzyonu T_DVL_STALE içinde, ölçüm
   zamanında) dt boyunca yatay konum σ'sını DR_ORAN × kat edilen yol kadar büyütür. */
void ida_kes_dr_buyut(ida_kes_t *k, const ida_kes_ayar_t *a, float dt);

#endif /* IDA_CORE_KES_IC_H */
