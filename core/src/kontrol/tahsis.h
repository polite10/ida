/*
 * tahsis.h — itki tahsisi ve itki eğrisinin tersi. Sahibi: W3 kontrol işi.
 *
 * Eksen sırası gövde çerçevesinde X, Y, Z, K, M, N (iskelet §3.1; A4-K49). İtici sırası A5'in
 * kanal sırasıdır (t1…t8 = kanal 0…7; A4-K54). İtkiler N, pozitif yön ε_i.
 *
 * Tahsis matrisi B (6×8): sütun i = [ε_i; r_i × ε_i] (02 §2.5; A5-K25). Dalış rejimi altı
 * ekseni ve etkin bütün iticileri, yüzey rejimi yalnız X, Y, N eksenlerini ve etkin yatay
 * iticileri (|ε_z| < 0,5) kullanır; devre dışı itici sütunları M maskesiyle çıkarılır.
 *
 * Öncelik (02 §2.5): dalışta [K, M] > [Z] > [N] > [X, Y], yüzeyde [N] > [X, Y]. Her grup g
 * için kurulumda, üst öncelikli grupların satırlarının sıfır uzayına izdüşürülmüş ters
 *   P_g = N_h A_gᵀ (A_g N_h A_gᵀ)⁺,   N_h ← N_h − N_h A_gᵀ (A_g N_h A_gᵀ)⁺ A_g N_h
 * hesaplanır (N_∅ = M; sözde tersler simetrik özdeğer ayrışımıyla, küçük özdeğerler kesilerek).
 * P_g'nin ürettiği itki değişimi üst grupların eksenlerini değiştirmez: rank kaybında da üst
 * öncelikli eksende elde edilen korunur; bir grubun üst grupları bozmadan üretilemeyen yönü
 * bırakılır (tanımlı kısıtlama). Tam rankta grubun tersi öteki bütün eksenlerin sıfır uzayından
 * alınır (yan etkisiz). Çalışırken: f çağıranın verdiği başlangıç noktasından (pencereye
 * kırpılmış) başlar; her grup için kalıntı e_g = τ_g − (B f)_g, Δf = P_g e_g, f ← f + α Δf (α ∈ [0, 1],
 * itici pencerelerine uyan en büyük değer). Son olarak elde edilen τ'yu değiştirmeyen sıfır
 * uzayı bileşeni pencere içinde silinir (en küçük norm). Pencereler itici sınırlarının, devre
 * dışı ve rejim kısıtının ve çağıranın verdiği değişim hızı penceresinin kesişimidir.
 *
 * Eğrinin tersi (A4-K54; 02 §2.5): F = sign(u) · F_şekil(|u|) · F_maks,yön; F_şekil A5-K104'ün
 * monoton kırpılmış ve 1001 noktada normalleştirilmiş polinomudur. Ters 33 noktalı tablodan
 * ikili arama ve doğrusal ara değerlemeyle alınır. Ölü bant: istenen şekil değeri
 * F_şekil(u_min)'in yarısından küçükse komut 0, değilse |u| ≥ u_min.
 */
#ifndef IDA_CORE_KONTROL_TAHSIS_H
#define IDA_CORE_KONTROL_TAHSIS_H

#include <stdbool.h>
#include <stdint.h>

#include "ida/port.h"
#include "param/param.h"

#define IDA_TAH_EKSEN 6u
#define IDA_TAH_ITICI IDA_PORT_N_THR
#define IDA_TAH_TABLO 33u /* eğri tablosu nokta sayısı: u_k = k/32 */

/* Parametre denetiminde kabul edilen en büyük koşul sayısı κ₂(B M Bᵀ) = λ_max / λ_min. */
#define IDA_TAH_KOSUL_EN_BUYUK 1.0e4f

/* Eksen indisleri. */
#define IDA_EKS_X 0u
#define IDA_EKS_Y 1u
#define IDA_EKS_Z 2u
#define IDA_EKS_K 3u
#define IDA_EKS_M 4u
#define IDA_EKS_N 5u

/* Tahsis rejimi. */
#define IDA_TAH_DALIS 0u
#define IDA_TAH_YUZEY 1u

/* Kurulumun sonucu (0 geçerli). */
#define IDA_TAH_TAMAM       0u
#define IDA_TAH_YON_BIRIM   1u /* ε_i birim değil (A5-K32)                           */
#define IDA_TAH_RANK        2u /* B'nin rankı 6 değil ya da κ > IDA_TAH_KOSUL_EN_BUYUK */
#define IDA_TAH_YUZEY_RANK  3u /* yatay iticiler X, Y, N için aynı ölçüt              */
#define IDA_TAH_SEKIL       4u /* şekil polinomu A5-K104'ü tutmuyor                  */
#define IDA_TAH_SAYISAL     5u /* sonlu olmayan ara sonuç                            */

typedef struct {
    float b[IDA_TAH_EKSEN][IDA_TAH_ITICI];         /* tahsis matrisi                        */
    float p_dalis[IDA_TAH_ITICI][IDA_TAH_EKSEN];   /* sütun a: a'nın grubunun izdüşümlü tersi */
    float p_yuzey[IDA_TAH_ITICI][IDA_TAH_EKSEN];
    float n_dalis[IDA_TAH_ITICI][IDA_TAH_ITICI];   /* bütün grupların sıfır uzayı izdüşümü  */
    float n_yuzey[IDA_TAH_ITICI][IDA_TAH_ITICI];
    float f_ileri;                                 /* N                                     */
    float f_geri;                                  /* N (pozitif büyüklük)                  */
    float sekil[IDA_TAH_TABLO];                    /* F_şekil(k/32), monoton, [0, 1]        */
    float u_min;                                   /* sıfırdan farklı en küçük |u|          */
    float s_min;                                   /* F_şekil(u_min)                        */
    uint8_t yatay;                                 /* yatay itici maskesi (|ε_z| < 0,5)     */
    uint8_t disi;                                  /* kurulumda kullanılan devre dışı maskesi */
} ida_tah_t;

/* Parametre kümesinden kurar; disi: devre dışı itici maskesi (bit i = itici i). Dönüş
   IDA_TAH_*; IDA_TAH_TAMAM değilse t kullanılmamalıdır. */
uint8_t ida_tah_kur(ida_tah_t *t, const ida_prm_kume_t *k, uint8_t disi);

/* Parametre grubunun denetimi (A4-K80 md. 8): ε birim, κ₂(B Bᵀ) ve yüzeyde κ₂ sınır içinde,
   şekil A5-K104'ü tutuyor (bütün iticiler etkin). 0 geçerli. */
uint8_t ida_tah_param_denetle(const ida_prm_kume_t *k);

/* κ₂(B Bᵀ) (rejim IDA_TAH_DALIS) ya da yatay iticilerle X, Y, N için (IDA_TAH_YUZEY).
   ε birim değilse ya da matris tekilse −1. */
float ida_tah_kosul_sayisi(const ida_prm_kume_t *k, uint8_t rejim);

/* Statik itici pencereleri: [−F_geri, +F_ileri]; devre dışı itici [0, 0]. */
void ida_tah_sinirlar(const ida_tah_t *t, float alt[IDA_TAH_ITICI], float ust[IDA_TAH_ITICI]);

/* Öncelikli tahsis: tau (6) → f (8, N); her f_i [alt_i, ust_i] ile statik, devre dışı ve rejim
   sınırlarının kesişiminde. f girişte başlangıç noktasıdır (pencereye kırpılır; kontrol önceki
   adımın tahsisini verir, değişim hızı penceresinin iki yönünde de pay kalsın diye), çıkışta
   sonuçtur. tau_a = B f. rejim IDA_TAH_*. */
void ida_tah_dagit(const ida_tah_t *t, uint8_t rejim, const float tau[IDA_TAH_EKSEN],
                   const float alt[IDA_TAH_ITICI], const float ust[IDA_TAH_ITICI],
                   float f[IDA_TAH_ITICI], float tau_a[IDA_TAH_EKSEN]);

/* tau_a = B f. */
void ida_tah_uret(const ida_tah_t *t, const float f[IDA_TAH_ITICI], float tau_a[IDA_TAH_EKSEN]);

/* İtki (N) → normalleştirilmiş ESC komutu u ∈ [−1, 1] (eğrinin tersi ve ölü bant). */
float ida_tah_komut(const ida_tah_t *t, float itki);

/* Şekil polinomu F(u) = Σ c_j u^(2j+1) (normalleştirilmemiş). */
float ida_tah_polinom(const float c[5], float u);

#endif /* IDA_CORE_KONTROL_TAHSIS_H */
