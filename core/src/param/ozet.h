/*
 * ozet.h — SHA-256 (FIPS 180-4), yalnız parametre şemasının özeti için (W2; A4 §3.10).
 * Durum çağıranın yığınındadır; küresel durum yok (A4-K22).
 */
#ifndef IDA_CORE_OZET_H
#define IDA_CORE_OZET_H

#include <stdint.h>

typedef struct {
    uint32_t h[8];
    uint8_t blok[64];
    uint32_t blok_n;     /* bloktaki bayt sayısı */
    uint64_t toplam_bit; /* işlenen toplam bit */
} ida_sha256_t;

void ida_sha256_basla(ida_sha256_t *s);
void ida_sha256_ekle(ida_sha256_t *s, const uint8_t *veri, uint32_t uzunluk);
void ida_sha256_bitir(ida_sha256_t *s, uint8_t ozet[32]);

#endif /* IDA_CORE_OZET_H */
