/*
 * cppcheck-hedef.h — yalnız cppcheck'e verilen hedef tanımları (statik-analiz.sh,
 * --include ile). Derlemeye girmez.
 *
 * cppcheck sistem başlıklarını içermez (missingIncludeSystem bastırılır); bu yüzden
 * port.h'in <float.h>'tan beklediği makrolar tanımsız kalır ve A4-K04 denetimi
 * (#if FLT_MANT_DIG != 24 || FLT_MAX_EXP != 128 -> #error) yanlışlıkla tetiklenir.
 * Burada, üç hedefin (arm-none-eabi Cortex-M33, macOS arm64, Windows x64) <float.h>'ında
 * bulunan IEEE-754 binary32 değerleri verilir; aynı koşulu gerçek derleyiciler aynı CI
 * koşusunda kendi <float.h>'larıyla denetler (host ve fw derlemeleri).
 *
 * Neden -D değil: cppcheck -D verildiğinde yalnız o yapılandırmayı denetler ve öteki
 * #ifdef dallarını atlar (--force gerekir); --include ile verilen tanımlar yapılandırma
 * taramasını daraltmaz. Neden sistem <float.h>'ı değil: Clang ve GCC'nin <float.h>'ı
 * değerleri derleyici iç makrolarından (__FLT_MANT_DIG__) alır, cppcheck bunları bilmez
 * ve sonuç işletim sistemine göre değişirdi; bu dosya iki işletim sisteminde aynıdır.
 */
#ifndef IDA_CPPCHECK_HEDEF_H
#define IDA_CPPCHECK_HEDEF_H

#define FLT_RADIX 2
#define FLT_MANT_DIG 24
#define FLT_MAX_EXP 128

#endif /* IDA_CPPCHECK_HEDEF_H */
