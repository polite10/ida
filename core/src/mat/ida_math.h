/*
 * ida_math.h — tek duyarlıklı (float) aşkın işlev sarmalayıcıları ve sayısal yardımcılar (W2).
 *
 * Çekirdek yalnız float kullanır (A4-K04; 02 §2.7); aşkın işlevler buradan çağrılır ki
 * kart ve PC arasındaki libm farkı tek yerde görünsün. Durum yoktur.
 */
#ifndef IDA_CORE_IDA_MATH_H
#define IDA_CORE_IDA_MATH_H

#include <stdbool.h>

#define IDA_PI_F 3.14159265358979323846f

float ida_karekok(float x);
float ida_sin(float x);
float ida_cos(float x);
float ida_asin(float x);
float ida_atan2(float y, float x);

/* NaN ve sonsuz değilse true. */
bool ida_sonlu(float x);

/* Açıyı (−π, π] aralığına sarar. */
float ida_aci_sar(float aci);

/* x'i [sinir.alt, sinir.ust] aralığına kırpar; NaN girdi alt sınıra gider. */
typedef struct {
    float alt;
    float ust;
} ida_aralik_t;
float ida_kirp(float x, ida_aralik_t sinir);

#endif /* IDA_CORE_IDA_MATH_H */
