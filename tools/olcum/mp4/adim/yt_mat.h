/* W0-MP4a yer tutucu: küçük matris ve kuaterniyon yardımcıları (ISO C17, float). */
#ifndef YT_MAT_H
#define YT_MAT_H

#include <stdint.h>

void yt_q_carp(const float a[4], const float b[4], float o[4]);      /* o = a ⊗ b (w, x, y, z) */
void yt_q_exp(const float v[3], float o[4]);                         /* Exp(v), v rad */
void yt_q_log(const float q[4], float o[3]);
void yt_q_norm(float q[4]);
void yt_q_rot(const float q[4], float R[3][3]);                     /* gövde -> NED */
void yt_q_euler(float roll, float pitch, float yaw, float q[4]);
void yt_capraz(const float a[3], const float b[3], float o[3]);
void yt_m3v(float M[3][3], const float v[3], float o[3]);
void yt_m3tv(float M[3][3], const float v[3], float o[3]);     /* Mᵀ v */
void yt_m3m3(float A[3][3], float B[3][3], float O[3][3]);
void yt_kosegen_skew(const float v[3], float S[3][3]);               /* [v]× */
/* n×n (n ≤ 4) Cholesky; 0: başarısız. L alt üçgen. */
int yt_chol(int n, float A[4][4], float L[4][4]);
void yt_ileri_coz(int n, float L[4][4], const float b[4], float x[4]);
float yt_sar_pi(float a);
float yt_sinirla(float x, float lo, float hi);

#endif
