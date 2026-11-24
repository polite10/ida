/*
 * test_rng.c — RNG okuma ve hata kurtarma (firmware/bsp/rng.h; sol bulgu 4; A1-K13).
 * Yazmaçlar sahte bir modeldir; gerçek donanımın hata zamanlaması kanıtı değildir.
 */
#include "rng.h"
#include "unity.h"

#include <string.h>

void setUp(void);
void tearDown(void);

static struct {
    uint32_t sr, cr;
    uint32_t veri[8];
    uint32_t n_veri, i_veri;
    int secs_kalici;     /* CONDRST SECS'i indirmez */
    int dr_sonra_seis;   /* DR okununca SEIS kurulur (bir kez) */
    int cecs_kalici;
    uint32_t condrst;
} m;

static ida_rng_sayac_t say;

static void drdy_guncelle(void)
{
    if (m.i_veri < m.n_veri && (m.sr & (IDA_RNG_SR_SECS | IDA_RNG_SR_CECS)) == 0u) {
        m.sr |= IDA_RNG_SR_DRDY;
    } else {
        m.sr &= ~IDA_RNG_SR_DRDY;
    }
}

static uint32_t f_sr(void *ctx)
{
    (void)ctx;
    if (!m.cecs_kalici) {
        m.sr &= ~IDA_RNG_SR_CECS; /* saat hatası geçicidir */
    }
    drdy_guncelle();
    return m.sr;
}

static uint32_t f_dr(void *ctx)
{
    (void)ctx;
    uint32_t v = 0u;
    if (m.i_veri < m.n_veri) {
        v = m.veri[m.i_veri++];
    }
    if (m.dr_sonra_seis) {
        m.dr_sonra_seis = 0;
        m.sr |= IDA_RNG_SR_SEIS;
    }
    drdy_guncelle();
    return v;
}

static uint32_t f_cr(void *ctx)
{
    (void)ctx;
    return m.cr;
}

static void f_cr_yaz(void *ctx, uint32_t v)
{
    (void)ctx;
    if ((m.cr & IDA_RNG_CR_CONDRST) != 0u && (v & IDA_RNG_CR_CONDRST) == 0u) {
        m.condrst++;
        if (!m.secs_kalici) {
            m.sr &= ~IDA_RNG_SR_SECS;
        }
    }
    m.cr = v;
}

static void f_sr_yaz(void *ctx, uint32_t v)
{
    (void)ctx;
    const uint32_t temizle = ~v & (IDA_RNG_SR_CEIS | IDA_RNG_SR_SEIS);
    m.sr &= ~temizle;
}

static const ida_rng_ops_t O = {NULL, f_sr, f_dr, f_cr, f_cr_yaz, f_sr_yaz};

void setUp(void)
{
    memset(&m, 0, sizeof m);
    memset(&say, 0, sizeof say);
}

void tearDown(void) {}

static void veri(uint32_t a, uint32_t b)
{
    m.veri[0] = a;
    m.veri[1] = b;
    m.n_veri = 2u;
}

static void test_olagan(void)
{
    veri(0x12345678u, 0x9ABCDEF0u);
    uint32_t v = 0u;
    TEST_ASSERT_EQUAL_INT(0, ida_rng_oku(&O, &say, &v));
    TEST_ASSERT_EQUAL_HEX32(0x12345678u, v);
}

/* Sol bulgu 4 (karşı örnek): DRDY kuruluyken de tohum hatası denetlenir. DR okunduktan sonra
   SEIS kurulursa değer kullanılmaz; kurtarma yapılır ve sonraki değer döner. (Önceki kod DRDY
   kuruluysa SR'deki hata bitlerine bakmadan DR'yi döndürüyordu.) */
static void test_drdy_kuruluyken_seis(void)
{
    veri(0x11111111u, 0x22222222u);
    m.dr_sonra_seis = 1;
    uint32_t v = 0u;
    TEST_ASSERT_EQUAL_INT(0, ida_rng_oku(&O, &say, &v));
    TEST_ASSERT_EQUAL_HEX32(0x22222222u, v);
    TEST_ASSERT_EQUAL_UINT32(1u, say.atilan);
    TEST_ASSERT_TRUE(say.tohum_hata >= 1u);
}

/* SECS (şu an) kurulu: CONDRST ile kurtarılır. */
static void test_secs_kurtarilir(void)
{
    veri(0x33333333u, 0u);
    m.sr = IDA_RNG_SR_SECS | IDA_RNG_SR_SEIS;
    uint32_t v = 0u;
    TEST_ASSERT_EQUAL_INT(0, ida_rng_oku(&O, &say, &v));
    TEST_ASSERT_EQUAL_HEX32(0x33333333u, v);
    TEST_ASSERT_EQUAL_UINT32(1u, m.condrst);
    TEST_ASSERT_EQUAL_UINT32(0u, m.sr & IDA_RNG_SR_SEIS);
}

/* Kurtarılamayan tohum hatası: hata döner (çağıran güvenli beklemeye geçer). */
static void test_secs_kalici_kurtarilamaz(void)
{
    veri(0x44444444u, 0x55555555u);
    m.sr = IDA_RNG_SR_SECS | IDA_RNG_SR_SEIS;
    m.secs_kalici = 1;
    uint32_t v = 0xDEADu;
    TEST_ASSERT_EQUAL_INT(-1, ida_rng_oku(&O, &say, &v));
    TEST_ASSERT_EQUAL_HEX32(0xDEADu, v); /* yazılmadı */
    TEST_ASSERT_EQUAL_UINT32(1u, say.kurtarilamaz);
}

/* Geçici saat hatası: CEIS temizlenir, değer döner. Kalıcı saat hatası: kurtarılamaz. */
static void test_saat_hatasi(void)
{
    veri(0x66666666u, 0u);
    m.sr = IDA_RNG_SR_CEIS | IDA_RNG_SR_CECS;
    uint32_t v = 0u;
    TEST_ASSERT_EQUAL_INT(0, ida_rng_oku(&O, &say, &v));
    TEST_ASSERT_EQUAL_HEX32(0x66666666u, v);
    TEST_ASSERT_EQUAL_UINT32(0u, m.sr & IDA_RNG_SR_CEIS);
    setUp();
    veri(0x77777777u, 0u);
    m.sr = IDA_RNG_SR_CEIS | IDA_RNG_SR_CECS;
    m.cecs_kalici = 1;
    TEST_ASSERT_EQUAL_INT(-1, ida_rng_oku(&O, &say, &v));
}

/* Hep 0 ya da hiç veri yok: sabit değer dönmez, hata döner. */
static void test_sifir_ve_veri_yok(void)
{
    veri(0u, 0u);
    m.veri[2] = 0u;
    m.veri[3] = 0u;
    m.n_veri = 4u;
    uint32_t v = 0xBEEFu;
    TEST_ASSERT_EQUAL_INT(-1, ida_rng_oku(&O, &say, &v));
    TEST_ASSERT_EQUAL_HEX32(0xBEEFu, v);
    TEST_ASSERT_EQUAL_UINT32(4u, say.sifir);
    setUp();
    TEST_ASSERT_EQUAL_INT(-1, ida_rng_oku(&O, &say, &v)); /* DRDY hiç kurulmaz */
    TEST_ASSERT_EQUAL_UINT32(IDA_RNG_DENEME, say.zaman_asimi);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_olagan);
    RUN_TEST(test_drdy_kuruluyken_seis);
    RUN_TEST(test_secs_kurtarilir);
    RUN_TEST(test_secs_kalici_kurtarilamaz);
    RUN_TEST(test_saat_hatasi);
    RUN_TEST(test_sifir_ve_veri_yok);
    return UNITY_END();
}
