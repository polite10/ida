/*
 * unity_config.h — Unity'nin kart yapılandırması (yalnız fw kökünde; UNITY_INCLUDE_CONFIG_H).
 *
 * Kartta stdio kullanılmaz (newlib-nano stdio'su ayırma yapar ve bağlama --wrap ile
 * reddedilir). Çıktı ida_test_karakter_yaz()'a gider; bu sürümde yalnız bir değişkene
 * yazar, kartta koşturma ve gerçek çıkış (USART1 / ITM) W3'tedir.
 */
#ifndef IDA_UNITY_CONFIG_H
#define IDA_UNITY_CONFIG_H

#define UNITY_OUTPUT_CHAR(c) ida_test_karakter_yaz(c)
#define UNITY_OUTPUT_CHAR_HEADER_DECLARATION ida_test_karakter_yaz(int c)

/* newlib-nano'nun setjmp.o'su libgcc'nin ARM yığın çözücüsünü (unwind-arm) ve onun
 * üzerinden abort/raise/_kill'i bağlamaya çeker; nosys saplarının "_kill is not
 * implemented" uyarısı --fatal-warnings ile hatadır. Kartta setjmp kullanılmaz: başarısız
 * doğrulama testi kesmez, test sonuna kadar sürer ve hata sayılır. */
#define UNITY_EXCLUDE_SETJMP_H

#endif /* IDA_UNITY_CONFIG_H */
