/*
 * rng.h — gerçek rastgele sayı üretecinden (RNG) okuma ve hata kurtarma (sol bulgu 4; A1-K13).
 *
 * Donanımdan bağımsızdır (ISO C17; PC'de sahte yazmaçlarla sınanır); bsp/bsp.c gerçek RNG
 * yazmaçlarını işlev tablosuyla verir. Bitler (STM32U5 RNG_SR / RNG_CR; CMSIS ve HAL
 * stm32u5xx_hal_rng.c ile aynı; bsp.c'de _Static_assert):
 *   SR: DRDY [0], CECS [1] saat hatası (şu an), SECS [2] tohum hatası (şu an),
 *       CEIS [5] saat hatası kesme bayrağı, SEIS [6] tohum hatası kesme bayrağı (0 yazılarak
 *       temizlenir)
 *   CR: CONDRST [30] koşullandırma sıfırlaması
 * Okuma sırası ST sürücüsünü izler (HAL_RNG_GenerateRandomNumber, RNG_RecoverSeedError):
 * SECS kuruluysa önce kurtarma; DRDY beklenir; DR okunduktan sonra SEIS kuruluysa değer
 * kullanılmaz (yeterli entropisi olmayabilir). Kurtarma: SECS yoksa (kendiliğinden sıfırlandı)
 * SEIS temizlenir; varsa CONDRST 1 → 0, CONDRST'nin inmesi beklenir, SEIS temizlenir, SECS'in
 * inmesi beklenir. Saat hatası (CECS) sınırlı süre sürerse kurtarılamaz sayılır. 0 değeri nonce
 * olamayacağı için yeniden okunur. Kurtarılamayan hata çağırana bildirilir: kart güvenli
 * beklemeye geçer, sabit nonce ile el sıkışma sürdürülmez.
 */
#ifndef IDA_FW_RNG_H
#define IDA_FW_RNG_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define IDA_RNG_SR_DRDY    UINT32_C(0x01)
#define IDA_RNG_SR_CECS    UINT32_C(0x02)
#define IDA_RNG_SR_SECS    UINT32_C(0x04)
#define IDA_RNG_SR_CEIS    UINT32_C(0x20)
#define IDA_RNG_SR_SEIS    UINT32_C(0x40)
#define IDA_RNG_CR_CONDRST UINT32_C(0x40000000)

#define IDA_RNG_DENEME     4u       /* geçerli değer için en çok okuma turu */
#define IDA_RNG_YOKLAMA    200000u  /* her bekleme için en çok yoklama */

typedef struct {
    void *ctx;
    uint32_t (*sr)(void *ctx);
    uint32_t (*dr)(void *ctx);
    uint32_t (*cr)(void *ctx);
    void (*cr_yaz)(void *ctx, uint32_t v);
    void (*sr_yaz)(void *ctx, uint32_t v); /* SR'ye yazım: CEIS/SEIS 0 yazılarak temizlenir */
} ida_rng_ops_t;

typedef struct {
    uint32_t tohum_hata;   /* SEIS ya da SECS görüldü */
    uint32_t saat_hata;    /* CEIS ya da CECS görüldü */
    uint32_t kurtarma;     /* CONDRST ile kurtarma */
    uint32_t atilan;       /* tohum hatası yüzünden kullanılmayan değer */
    uint32_t sifir;        /* 0 okundu */
    uint32_t zaman_asimi;  /* DRDY ya da kurtarma beklemesi doldu */
    uint32_t kurtarilamaz; /* çağrıya hata dönüldü */
} ida_rng_sayac_t;

/* Geçerli, sıfırdan farklı 32 bitlik değer okur. 0: tamam (*deger yazıldı); < 0: kurtarılamaz. */
int ida_rng_oku(const ida_rng_ops_t *o, ida_rng_sayac_t *s, uint32_t *deger);

#ifdef __cplusplus
}
#endif

#endif /* IDA_FW_RNG_H */
