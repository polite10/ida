/* W0-MP4a yer tutucu: küçük matris ve kuaterniyon yardımcıları (ISO C17, float). */
#include "yt_mat.h"

#include <math.h>

void yt_q_carp(const float a[4], const float b[4], float o[4])
{
    const float w = a[0] * b[0] - a[1] * b[1] - a[2] * b[2] - a[3] * b[3];
    const float x = a[0] * b[1] + a[1] * b[0] + a[2] * b[3] - a[3] * b[2];
    const float y = a[0] * b[2] - a[1] * b[3] + a[2] * b[0] + a[3] * b[1];
    const float z = a[0] * b[3] + a[1] * b[2] - a[2] * b[1] + a[3] * b[0];
    o[0] = w; o[1] = x; o[2] = y; o[3] = z;
}

void yt_q_exp(const float v[3], float o[4])
{
    const float t = sqrtf(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
    if (t < 1e-6f) {
        o[0] = 1.0f; o[1] = 0.5f * v[0]; o[2] = 0.5f * v[1]; o[3] = 0.5f * v[2];
        return;
    }
    const float h = 0.5f * t;
    const float k = sinf(h) / t;
    o[0] = cosf(h); o[1] = k * v[0]; o[2] = k * v[1]; o[3] = k * v[2];
}

void yt_q_log(const float q[4], float o[3])
{
    const float n = sqrtf(q[1] * q[1] + q[2] * q[2] + q[3] * q[3]);
    float k;
    if (n < 1e-6f) {
        k = 2.0f;
    } else {
        const float w = q[0] < 0.0f ? -q[0] : q[0];
        const float s = q[0] < 0.0f ? -1.0f : 1.0f;
        k = s * 2.0f * atan2f(n, w) / n;
    }
    o[0] = k * q[1]; o[1] = k * q[2]; o[2] = k * q[3];
}

void yt_q_norm(float q[4])
{
    const float n = sqrtf(q[0] * q[0] + q[1] * q[1] + q[2] * q[2] + q[3] * q[3]);
    const float k = n > 1e-12f ? 1.0f / n : 1.0f;
    q[0] *= k; q[1] *= k; q[2] *= k; q[3] *= k;
}

void yt_q_rot(const float q[4], float R[3][3])
{
    const float w = q[0], x = q[1], y = q[2], z = q[3];
    R[0][0] = 1.0f - 2.0f * (y * y + z * z);
    R[0][1] = 2.0f * (x * y - w * z);
    R[0][2] = 2.0f * (x * z + w * y);
    R[1][0] = 2.0f * (x * y + w * z);
    R[1][1] = 1.0f - 2.0f * (x * x + z * z);
    R[1][2] = 2.0f * (y * z - w * x);
    R[2][0] = 2.0f * (x * z - w * y);
    R[2][1] = 2.0f * (y * z + w * x);
    R[2][2] = 1.0f - 2.0f * (x * x + y * y);
}

void yt_q_euler(float roll, float pitch, float yaw, float q[4])
{
    const float cr = cosf(0.5f * roll), sr = sinf(0.5f * roll);
    const float cp = cosf(0.5f * pitch), sp = sinf(0.5f * pitch);
    const float cy = cosf(0.5f * yaw), sy = sinf(0.5f * yaw);
    q[0] = cr * cp * cy + sr * sp * sy;
    q[1] = sr * cp * cy - cr * sp * sy;
    q[2] = cr * sp * cy + sr * cp * sy;
    q[3] = cr * cp * sy - sr * sp * cy;
}

void yt_capraz(const float a[3], const float b[3], float o[3])
{
    const float x = a[1] * b[2] - a[2] * b[1];
    const float y = a[2] * b[0] - a[0] * b[2];
    const float z = a[0] * b[1] - a[1] * b[0];
    o[0] = x; o[1] = y; o[2] = z;
}

void yt_m3v(float M[3][3], const float v[3], float o[3])
{
    float t[3];
    for (int i = 0; i < 3; i++) { t[i] = M[i][0] * v[0] + M[i][1] * v[1] + M[i][2] * v[2]; }
    o[0] = t[0]; o[1] = t[1]; o[2] = t[2];
}

void yt_m3tv(float M[3][3], const float v[3], float o[3])
{
    float t[3];
    for (int i = 0; i < 3; i++) { t[i] = M[0][i] * v[0] + M[1][i] * v[1] + M[2][i] * v[2]; }
    o[0] = t[0]; o[1] = t[1]; o[2] = t[2];
}

void yt_m3m3(float A[3][3], float B[3][3], float O[3][3])
{
    float T[3][3];
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 3; j++) { T[i][j] = A[i][0] * B[0][j] + A[i][1] * B[1][j] + A[i][2] * B[2][j]; }
    }
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 3; j++) { O[i][j] = T[i][j]; }
    }
}

void yt_kosegen_skew(const float v[3], float S[3][3])
{
    S[0][0] = 0.0f;  S[0][1] = -v[2]; S[0][2] = v[1];
    S[1][0] = v[2];  S[1][1] = 0.0f;  S[1][2] = -v[0];
    S[2][0] = -v[1]; S[2][1] = v[0];  S[2][2] = 0.0f;
}

int yt_chol(int n, float A[4][4], float L[4][4])
{
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) { L[i][j] = 0.0f; }
    }
    for (int j = 0; j < n; j++) {
        float d = A[j][j];
        for (int k = 0; k < j; k++) { d -= L[j][k] * L[j][k]; }
        if (!(d > 1e-12f)) { return 0; }
        const float l = sqrtf(d);
        L[j][j] = l;
        const float il = 1.0f / l;
        for (int i = j + 1; i < n; i++) {
            float s = A[i][j];
            for (int k = 0; k < j; k++) { s -= L[i][k] * L[j][k]; }
            L[i][j] = s * il;
        }
    }
    return 1;
}

void yt_ileri_coz(int n, float L[4][4], const float b[4], float x[4])
{
    for (int i = 0; i < n; i++) {
        float s = b[i];
        for (int k = 0; k < i; k++) { s -= L[i][k] * x[k]; }
        x[i] = s / L[i][i];
    }
}

float yt_sar_pi(float a)
{
    const float pi = 3.14159265f;
    if (!(a > -100.0f && a < 100.0f)) { return 0.0f; } /* sonlu olmayan ya da aşırı değer: döngü sınırlı kalsın */
    while (a > pi) { a -= 2.0f * pi; }
    while (a <= -pi) { a += 2.0f * pi; }
    return a;
}

float yt_sinirla(float x, float lo, float hi)
{
    return x < lo ? lo : (x > hi ? hi : x);
}
