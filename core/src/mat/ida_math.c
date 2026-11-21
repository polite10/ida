/*
 * ida_math.c — float aşkın işlev sarmalayıcıları (W2).
 */
#include "mat/ida_math.h"

#include <math.h>

float ida_karekok(float x)
{
    return sqrtf(x);
}

float ida_sin(float x)
{
    return sinf(x);
}

float ida_cos(float x)
{
    return cosf(x);
}

float ida_asin(float x)
{
    return asinf(x);
}

float ida_atan2(float y, float x)
{
    return atan2f(y, x);
}

bool ida_sonlu(float x)
{
    return isfinite(x) != 0;
}

float ida_aci_sar(float aci)
{
    const float iki_pi = 2.0f * IDA_PI_F;
    float a = fmodf(aci, iki_pi); /* (−2π, 2π) */
    if (a > IDA_PI_F) {
        a -= iki_pi;
    } else if (a <= -IDA_PI_F) {
        a += iki_pi;
    }
    return a;
}

float ida_kirp(float x, ida_aralik_t sinir)
{
    if (!(x >= sinir.alt)) { /* NaN dahil */
        return sinir.alt;
    }
    if (x > sinir.ust) {
        return sinir.ust;
    }
    return x;
}
