/*
 * mat.h — vektör, kuaterniyon ve küçük sabit boyutlu matris yardımcıları (W2; durum yok).
 *
 * Kabuller iskelet §3.1'dir: kuaterniyon (w, x, y, z), Hamilton çarpımı, gövdeden dünyaya
 * (NED) dönüşü temsil eder; v_dunya = q ⊗ v_govde ⊗ q*. Bütün değerler float.
 * Küçük matrisler satır öncelikli düz dizilerdir ve tamponları çağıranındır (bellek ayırma
 * yok; A4-K97). Boyut sınırı IDA_MAT_EN_BUYUK'tur.
 */
#ifndef IDA_CORE_MAT_H
#define IDA_CORE_MAT_H

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    float x;
    float y;
    float z;
} ida_v3_t;

typedef struct {
    float w;
    float x;
    float y;
    float z;
} ida_q_t;

typedef struct {
    float m[3][3]; /* m[satır][sütun] */
} ida_m3_t;

typedef struct {
    float roll;  /* rad, x ekseni */
    float pitch; /* rad, y ekseni */
    float yaw;   /* rad, z ekseni, (−π, π] */
} ida_euler_t;

/* ---- 3 boyutlu vektör ------------------------------------------------------------------ */
ida_v3_t ida_v3_topla(ida_v3_t lhs, ida_v3_t rhs);
ida_v3_t ida_v3_cikar(ida_v3_t lhs, ida_v3_t rhs);
ida_v3_t ida_v3_olcekle(ida_v3_t v, float k);
float ida_v3_ic(ida_v3_t lhs, ida_v3_t rhs);
ida_v3_t ida_v3_dis(ida_v3_t lhs, ida_v3_t rhs);
float ida_v3_norm(ida_v3_t v);
/* Birim vektör; norm 1e-12'den küçükse sıfır vektör ve false döner. */
bool ida_v3_birim(ida_v3_t v, ida_v3_t *sonuc);

/* ---- Kuaterniyon ----------------------------------------------------------------------- */
ida_q_t ida_q_birim_deger(void); /* (1, 0, 0, 0) */
ida_q_t ida_q_carp(ida_q_t lhs, ida_q_t rhs); /* Hamilton: lhs ⊗ rhs */
ida_q_t ida_q_eslenik(ida_q_t q);
/* Birimleme; norm 1e-12'den küçükse (1,0,0,0) ve false döner. */
bool ida_q_birimle(ida_q_t q, ida_q_t *sonuc);
ida_v3_t ida_q_dondur(ida_q_t q, ida_v3_t v); /* q ⊗ v ⊗ q* */
/* Dönme vektöründen (eksen × açı, rad) birim kuaterniyon. */
ida_q_t ida_q_donme_vektorunden(ida_v3_t rv);
ida_m3_t ida_q_dcm(ida_q_t q);       /* gövde → NED dönüşüm matrisi */
ida_euler_t ida_q_euler(ida_q_t q);  /* ZYX sırası (yaw, pitch, roll) */

/* ---- 3×3 matris ------------------------------------------------------------------------ */
ida_m3_t ida_m3_birim_deger(void);
ida_m3_t ida_m3_carp(ida_m3_t lhs, ida_m3_t rhs);
ida_v3_t ida_m3_v3(ida_m3_t m, ida_v3_t v);
ida_m3_t ida_m3_devrik(ida_m3_t m);
ida_m3_t ida_m3_capraz(ida_v3_t v); /* [v]x: ida_m3_v3([v]x, u) = v × u */

/* ---- Küçük sabit boyutlu matris (satır öncelikli) ---------------------------------------- */
#define IDA_MAT_EN_BUYUK 24u

typedef struct {
    uint8_t satir;
    uint8_t sutun;
} ida_mat_boyut_t;

typedef struct {
    const float *veri;
    ida_mat_boyut_t boyut;
} ida_mat_oku_t;

typedef struct {
    float *veri;
    ida_mat_boyut_t boyut;
} ida_mat_yaz_t;

/* sonuc = lhs · rhs. Boyutlar uyuşmazsa ya da sınırı aşarsa hiçbir şey yazmaz, false döner. */
bool ida_mat_carp(ida_mat_oku_t lhs, ida_mat_oku_t rhs, const ida_mat_yaz_t *sonuc);
/* sonuc = lhs + rhs. */
bool ida_mat_topla(ida_mat_oku_t lhs, ida_mat_oku_t rhs, const ida_mat_yaz_t *sonuc);
/* sonuc = aᵀ. */
bool ida_mat_devrik(ida_mat_oku_t a, const ida_mat_yaz_t *sonuc);
/* sonuc = A · P · Aᵀ (P kare, n×n; A m×n; sonuc m×m). ara: en az m·n float'lık tampon. */
bool ida_mat_apat(ida_mat_oku_t a, ida_mat_oku_t p, const ida_mat_yaz_t *ara,
                  const ida_mat_yaz_t *sonuc);

#endif /* IDA_CORE_MAT_H */
