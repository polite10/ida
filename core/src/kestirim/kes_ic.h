/*
 * kes_ic.h — kestirim modülünün iç işlevleri (yalnız kestirim/ içinden). Sahibi: W3 kestirim.
 *
 * kes_eskf.c: filtre matematiği (yayılım, skaler güncelleme, enjeksiyon, sağlamlık denetimi).
 * kes_ufuk.c: IMU halkası, ölçüm kuyruğu, füzyon ufkunun ilerletilmesi, ölçüm modelleri ve
 *             çıkış öngörücüsü.
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

#endif /* IDA_CORE_KES_IC_H */
