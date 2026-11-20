/*
 * A1-K101 kabul testleri (sözleşme §4.9, §7.1 K97–K102 satırı "aynı süreçte iki SIL örneği"):
 *   "protocol/ida_link MAVLink ayrıştırma ve kodlama durumunu örneğin kendi yapısında tutar ve
 *    yalnız çağıranın durumuyla çalışan işlevleri kullanır; kitaplığın kanal dizilerini
 *    kullanan işlevleri kullanmaz; böylece bir süreçte birden çok SIL örneği koşabilir."
 * Öğeler (davranış düzeyinde; simge düzeyindeki güvence uygulayıcının denetimidir):
 *   (a) iki (ve üç) alıcı örneği, farklı akışlarla, rastgele parçalarla iç içe beslenince her
 *       biri tek başına beslenmiş gibi aynı teslimleri ve sayaçları verir (aynı rol ve farklı
 *       roller; aynı akışın iki örneğe bayt bayt dönüşümlü verilmesi dahil)
 *   (b) iki gönderici örneği iç içe kullanılınca her birinin çıktısı (seq dahil) tek başına
 *       kullanılmış gibidir
 *   (c) aynı roldeki bir gönderici ve bir alıcı iç içe kullanılınca birbirini etkilemez
 */
#include <stdio.h>
#include <string.h>

#include "kabul_ortak.h"
#include "unity.h"

void setUp(void) {}
void tearDown(void) {}

#define AKIS_AZAMI 32000u
#define ORNEK 3u

static uint8_t akis[ORNEK][AKIS_AZAMI];
static kabul_sonuc_t tek[ORNEK];
static kabul_sonuc_t ic_ice[ORNEK];
static ida_link_alici_t alici[ORNEK];

/* Örnekleri dönüşümlü besler: sıradaki örnek rastgele seçilir, ona 0..k baytlık parça verilir. */
static void ic_ice_besle(size_t sayi, const ida_link_rol_t *rol, const size_t *n, uint64_t tohum, size_t k)
{
    size_t verilen[ORNEK] = {0};
    kabul_rng_t r;
    kabul_rng_kur(&r, tohum);
    for (size_t i = 0; i < sayi; ++i) {
        TEST_ASSERT_EQUAL_INT(IDA_LINK_TAMAM, ida_link_alici_kur(&alici[i], rol[i]));
        kabul_sonuc_sifirla(&ic_ice[i]);
    }
    for (;;) {
        size_t bitmeyen = 0;
        for (size_t i = 0; i < sayi; ++i) {
            bitmeyen += verilen[i] < n[i] ? 1u : 0u;
        }
        if (bitmeyen == 0u) {
            break;
        }
        size_t i = kabul_rng_aralik(&r, (uint32_t)sayi);
        if (verilen[i] >= n[i]) {
            continue;
        }
        size_t parca = 1u + kabul_rng_aralik(&r, (uint32_t)k);
        if (parca > n[i] - verilen[i]) {
            parca = n[i] - verilen[i];
        }
        kabul_parcalayici_t p = {KABUL_PARCA_TUMU, 0u, {0u}};
        kabul_besle(&alici[i], &akis[i][verilen[i]], parca, &p, &ic_ice[i]);
        verilen[i] += parca;
    }
}

static void test_A1_K101_a_alicilar_ic_ice(void)
{
    static const struct {
        size_t sayi;
        ida_link_rol_t rol[ORNEK];
        kabul_akis_turu_t tur[ORNEK];
    } DURUM[] = {
        {2u, {IDA_LINK_ROL_KART, IDA_LINK_ROL_KART, IDA_LINK_ROL_KART},
         {KABUL_AKIS_KARISIK, KABUL_AKIS_KARISIK, KABUL_AKIS_KARISIK}},
        {2u, {IDA_LINK_ROL_KART, IDA_LINK_ROL_SIM, IDA_LINK_ROL_KART},
         {KABUL_AKIS_KARISIK, KABUL_AKIS_GOMULU_RASTGELE, KABUL_AKIS_KARISIK}},
        {2u, {IDA_LINK_ROL_SIM, IDA_LINK_ROL_SIM, IDA_LINK_ROL_KART},
         {KABUL_AKIS_TEMIZ_BOZUK, KABUL_AKIS_FD_YOGUN, KABUL_AKIS_KARISIK}},
        {3u, {IDA_LINK_ROL_KART, IDA_LINK_ROL_SIM, IDA_LINK_ROL_KART},
         {KABUL_AKIS_KARISIK, KABUL_AKIS_KARISIK, KABUL_AKIS_TEMIZ_BOZUK}},
    };
    static const size_t K[] = {1u, 5u, 40u, 300u};
    for (size_t d = 0; d < sizeof DURUM / sizeof DURUM[0]; ++d) {
        size_t n[ORNEK] = {0};
        for (size_t i = 0; i < DURUM[d].sayi; ++i) {
            n[i] = kabul_akis_uret(DURUM[d].tur[i], DURUM[d].rol[i], 0x10100ull + d * 16u + i, akis[i],
                                   AKIS_AZAMI, 20000u, NULL, NULL);
            kabul_besle_yeni(DURUM[d].rol[i], akis[i], n[i], &tek[i]);
            TEST_ASSERT_TRUE(tek[i].sayi > 0u || DURUM[d].tur[i] == KABUL_AKIS_FD_YOGUN);
        }
        for (size_t ki = 0; ki < sizeof K / sizeof K[0]; ++ki) {
            ic_ice_besle(DURUM[d].sayi, DURUM[d].rol, n, 0xABCDull + d * 7u + ki, K[ki]);
            for (size_t i = 0; i < DURUM[d].sayi; ++i) {
                char ileti[96];
                snprintf(ileti, sizeof ileti, "durum %u, örnek %u, parça <= %u", (unsigned)d, (unsigned)i,
                         (unsigned)K[ki]);
                kabul_sonuc_esit(&tek[i], &ic_ice[i], ileti);
            }
        }
    }
}

/* (a): aynı akış iki örneğe bayt bayt dönüşümlü verilir (bir bayt A'ya, aynı bayt B'ye):
   paylaşılan gizli durum olsaydı ikisi de bozulurdu. */
static void test_A1_K101_a_ayni_akis_bayt_bayt_donusumlu(void)
{
    size_t n = kabul_akis_uret(KABUL_AKIS_KARISIK, IDA_LINK_ROL_KART, 0x5A5Aull, akis[0], AKIS_AZAMI, 20000u,
                               NULL, NULL);
    kabul_besle_yeni(IDA_LINK_ROL_KART, akis[0], n, &tek[0]);
    TEST_ASSERT_EQUAL_INT(IDA_LINK_TAMAM, ida_link_alici_kur(&alici[0], IDA_LINK_ROL_KART));
    TEST_ASSERT_EQUAL_INT(IDA_LINK_TAMAM, ida_link_alici_kur(&alici[1], IDA_LINK_ROL_KART));
    kabul_sonuc_sifirla(&ic_ice[0]);
    kabul_sonuc_sifirla(&ic_ice[1]);
    kabul_parcalayici_t p = {KABUL_PARCA_TUMU, 0u, {0u}};
    for (size_t i = 0; i < n; ++i) {
        kabul_besle(&alici[0], &akis[0][i], 1u, &p, &ic_ice[0]);
        kabul_besle(&alici[1], &akis[0][i], 1u, &p, &ic_ice[1]);
    }
    kabul_sonuc_esit(&tek[0], &ic_ice[0], "örnek A");
    kabul_sonuc_esit(&tek[0], &ic_ice[1], "örnek B");
}

/* (b), (c) */
static void test_A1_K101_bc_gondericiler_ve_alici_ic_ice(void)
{
    static uint8_t tek_cikis[2][AKIS_AZAMI];
    static uint8_t ic_cikis[2][AKIS_AZAMI];
    static const ida_link_rol_t ROL[2] = {IDA_LINK_ROL_SIM, IDA_LINK_ROL_SIM};
    size_t tek_n[2] = {0, 0};
    size_t ic_n[2] = {0, 0};
    /* Her gönderici için 120 mesajlık sabit dizi (yükler tohumla belirlenir). */
    for (int tur = 0; tur < 2; ++tur) {
        ida_link_gonderici_t g[2];
        kabul_rng_t r[2];
        TEST_ASSERT_EQUAL_INT(IDA_LINK_TAMAM, ida_link_gonderici_kur(&g[0], ROL[0]));
        TEST_ASSERT_EQUAL_INT(IDA_LINK_TAMAM, ida_link_gonderici_kur(&g[1], ROL[1]));
        g[1].seq = 100u; /* ikinci örnek başka bir seq'ten başlar */
        kabul_rng_kur(&r[0], 0x6701ull);
        kabul_rng_kur(&r[1], 0x6702ull);
        kabul_rng_t sira;
        kabul_rng_kur(&sira, 0x5152ull);
        size_t kalan[2] = {120u, 120u};
        /* tur 0: önce tamamen örnek 0, sonra örnek 1; tur 1: rastgele iç içe, araya alıcı. */
        TEST_ASSERT_EQUAL_INT(IDA_LINK_TAMAM, ida_link_alici_kur(&alici[0], IDA_LINK_ROL_KART));
        kabul_sonuc_sifirla(&ic_ice[0]);
        while (kalan[0] + kalan[1] > 0u) {
            size_t i;
            if (tur == 0) {
                i = kalan[0] > 0u ? 0u : 1u;
            } else {
                i = kabul_rng_aralik(&sira, 2u);
                if (kalan[i] == 0u) {
                    i = 1u - i;
                }
            }
            const kabul_mesaj_t *m;
            do {
                m = &kabul_mesajlar[kabul_rng_aralik(&r[i], KABUL_MESAJ_SAYISI)];
            } while ((m->yon & kabul_rol_gonderir_yon(ROL[i])) == 0u);
            uint8_t yuk[IDA_LINK_YUK_AZAMI] = {0};
            size_t dolu = kabul_rng_aralik(&r[i], (uint32_t)m->yuk_azami + 1u);
            for (size_t k = 0; k < dolu; ++k) {
                yuk[k] = kabul_rng_bayt(&r[i]);
            }
            kabul_yapi_t y;
            kabul_yapi_hazirla(&y, m, yuk);
            uint8_t *hedef = tur == 0 ? tek_cikis[i] : ic_cikis[i];
            size_t *hn = tur == 0 ? &tek_n[i] : &ic_n[i];
            size_t yazilan = 0;
            TEST_ASSERT_EQUAL_INT(IDA_LINK_TAMAM, ida_link_gonder(&g[i], m->msgid, y.b, m->yapi_boyu, &hedef[*hn],
                                                                  AKIS_AZAMI - *hn, &yazilan));
            if (tur == 1) {
                /* (c) Aynı süreçte bir kart alıcısı, örnek 0'ın ürettiği çerçeveyi hemen alır. */
                if (i == 0u) {
                    kabul_parcalayici_t p = {KABUL_PARCA_SABIT, 3u, {0u}};
                    kabul_besle(&alici[0], &hedef[*hn], yazilan, &p, &ic_ice[0]);
                }
            }
            *hn += yazilan;
            --kalan[i];
        }
    }
    for (size_t i = 0; i < 2; ++i) {
        TEST_ASSERT_EQUAL_UINT32((uint32_t)tek_n[i], (uint32_t)ic_n[i]);
        TEST_ASSERT_EQUAL_HEX8_ARRAY_MESSAGE(tek_cikis[i], ic_cikis[i], tek_n[i], "gönderici çıktısı iç içe kullanımda değişti");
    }
    /* (c) Alıcı örnek 0'ın 120 çerçevesini teslim etti; tek başına beslenenle aynı. */
    kabul_besle_yeni(IDA_LINK_ROL_KART, tek_cikis[0], tek_n[0], &tek[0]);
    TEST_ASSERT_EQUAL_UINT32(120u, (uint32_t)tek[0].sayi);
    kabul_sonuc_esit(&tek[0], &ic_ice[0], "gönderici ile iç içe alıcı");
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_A1_K101_a_alicilar_ic_ice);
    RUN_TEST(test_A1_K101_a_ayni_akis_bayt_bayt_donusumlu);
    RUN_TEST(test_A1_K101_bc_gondericiler_ve_alici_ic_ice);
    return UNITY_END();
}
