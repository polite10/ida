/*
 * tests/kabul/kontrol — özellik testi (W5-kontrol-acil, Kabul 4; EMNİYET): rastgele komut,
 * emniyet örneği ve zaman dizileri; sabit tohum; 200 000 adım. Düşen dizinin tohumu ve adımı
 * hata iletisinde ve tearDown'da yazdırılır.
 *
 * Değişmez: testin kendi modeline göre (kabul_kontrol.h; yalnız girdiden hesaplanır: basılı,
 * bayat, reddedilen çağrıdaki ya da inst ≠ 0 / VALID = 0 örnekteki kısıtlayan içerik, oturum
 * sonu, bağlantı olayı çağrısında gevşetme yok — W5-K20) etkin acil durdurma kesin kısıtlayanken
 * nötr dışı çıktı veren çağrı sayısı 0. Denetim her çağrıda düzenekte (kk_cagir) yapılır.
 *
 * Dizi: acil durdurma darbeleri (tek çağrılık darbeden 6 s'lik basılı kalışa), emniyet akışının
 * kesilmesi ve ölçüm zamanının donması (bayatlık süresinin altında ve üstünde), inst 1 ve
 * VALID = 0 emniyet örnekleri, reddedilen çağrılar (DUP_SEQ, BAD_INPUT), tick ve IMU_REJECTED
 * çağrıları, kalkış çağrısında SESSION_START, oturum sonu ve yeniden başlangıç; operatör araç
 * silahsızsa ARM, hedef kipe (GUIDED, MANUAL, STABILIZE, DEPTH_HOLD) geçiş ve her çağrıda
 * sürekli komut (rastgele setpoint'ler ve eksenler) verir.
 *
 * Kapsam koşulu: dizide nötr dışı çıktılı çağrılar ve kesin kısıtlayan çağrılar yeterince
 * bulunmalı; gevşekten kısıtlayana geçişlerin yeterince çoğunda önceki çıktı nötr dışı olmalı ve
 * her kısıtlama nedeni (basılı, bayatlığın iki ölçütü, reddedilen çağrı, inst ≠ 0, VALID = 0,
 * bağlantı olayı, oturum sonu) en az birkaç kez geçişe yol açmalı; değilse test düşer.
 */
#include <stdio.h>
#include <string.h>

#include "kabul_kontrol.h"
#include "unity.h"

#define ADIM_SAYISI 200000
#define TOHUM UINT64_C(0x4B4F4E54524F4C01)

static kc_rng_t R;

static bool olasilik(unsigned pay, unsigned payda)
{
    return kc_rng_aralik(&R, payda) < pay;
}

static float rastgele_aralik(float a, float b)
{
    return a + (b - a) * (float)kc_rng_aralik(&R, 10001u) / 10000.0f;
}

/* Hedef kipler; GUIDED ağırlıklı (bugünkü çekirdek MANUAL komutunu UNSUPPORTED ile yanıtlıyor:
   nötr dışı çıktı yalnız GUIDED'da). */
static const uint8_t HEDEF_KIPLER[8] = {IDA_MODE_GUIDED, IDA_MODE_GUIDED, IDA_MODE_GUIDED,    IDA_MODE_GUIDED,
                                        IDA_MODE_GUIDED, IDA_MODE_MANUAL, IDA_MODE_STABILIZE, IDA_MODE_DEPTH_HOLD};

static int g_basili;     /* > 0: acil durdurmanın basılı kalacağı çağrı sayısı */
static int g_serbest;    /* > 0: sonraki basışa kadar çağrı sayısı */
static bool g_kalkis;    /* bu çağrı basılı dönemden sonraki ilk çağrı */
static int g_kesinti;    /* > 0: emniyet akışı kesik */
static int g_donma;      /* > 0: t_meas donmuş */
static int g_oturumsuz;  /* > 0: oturum kapalı */
static uint8_t g_hedef;  /* operatörün hedef kipi */
static int g_hedef_kalan;

static void basis_sec(void)
{
    uint32_t r = kc_rng_aralik(&R, 100u);
    if (r < 20u) {
        g_basili = 1;
    } else if (r < 50u) {
        g_basili = 2 + (int)kc_rng_aralik(&R, 40u);
    } else if (r < 90u) {
        g_basili = 40 + (int)kc_rng_aralik(&R, 360u);
    } else {
        g_basili = 400 + (int)kc_rng_aralik(&R, 800u);
    }
}

static void operator_rastgele(void)
{
    kk_op.sp.mask = (uint8_t)(IDA_SP_POS_NE | IDA_SP_DEPTH | IDA_SP_YAW);
    kk_op.sp.pos_ne[0] = rastgele_aralik(-60.0f, 60.0f);
    kk_op.sp.pos_ne[1] = rastgele_aralik(-60.0f, 60.0f);
    kk_op.sp.depth = rastgele_aralik(0.0f, 20.0f);
    kk_op.sp.yaw = rastgele_aralik(-3.1f, 3.1f);
    for (unsigned j = 0; j < 6u; ++j) {
        kk_op.eksen[j] = rastgele_aralik(-1.0f, 1.0f);
    }
}

static void kip_ekle(ida_in_t *in, uint8_t kip)
{
    if (in->n_cmds >= IDA_PORT_MAX_CMDS) {
        return;
    }
    if (kip == IDA_MODE_GUIDED) {
        kc_komut_ekle(in, ka_yeni_id(), (uint8_t)IDA_CMD_SET_MODE)->u.set_mode.mode = kip;
    } else {
        (void)ka_mandal(in, (uint8_t)IDA_NARROW_MODE);
        in->narrow.mode = kip;
    }
}

static void adim(void)
{
    const ida_out_t *o = ka_out();
    bool armed = o->armed;
    uint8_t kip = o->status.mode;
    uint8_t fs = o->fs_level;
    ka_ayar.tick = false;
    ka_ayar.imu_bozuk = false;
    /* Acil durdurma darbeleri. */
    g_kalkis = false;
    if (g_basili > 0) {
        ka_ayar.estop = 1u;
        if (--g_basili == 0) {
            g_serbest = 1 + (int)kc_rng_aralik(&R, 500u);
        }
    } else {
        if (ka_ayar.estop != 0u) {
            g_kalkis = true;
        }
        ka_ayar.estop = 0u;
        if (--g_serbest <= 0) {
            basis_sec();
        }
    }
    /* Emniyet akışının kesilmesi ve ölçüm zamanının donması (T_emn_bayat'ın altında ve üstünde). */
    if (g_kesinti > 0) {
        --g_kesinti;
        ka_ayar.emniyet = false;
    } else {
        ka_ayar.emniyet = true;
        if (olasilik(1u, 1200u)) {
            g_kesinti = 10 + (int)kc_rng_aralik(&R, 110u);
        }
    }
    if (g_donma > 0) {
        --g_donma;
        ka_ayar.tmeas_dondur = true;
    } else {
        ka_ayar.tmeas_dondur = false;
        if (olasilik(1u, 1200u)) {
            g_donma = 10 + (int)kc_rng_aralik(&R, 120u);
        }
    }
    /* Oturum. */
    bool oturum_sonu = false;
    bool oturum_baslangici = false;
    if (g_oturumsuz > 0) {
        --g_oturumsuz;
        ka_ayar.link_state = (uint8_t)IDA_LINK_NONE;
        if (g_oturumsuz == 0) {
            ka_ayar.link_state = (uint8_t)IDA_LINK_RUNNING;
            oturum_baslangici = true;
        }
    } else if (olasilik(1u, 6000u)) {
        oturum_sonu = true;
        g_oturumsuz = 5 + (int)kc_rng_aralik(&R, 60u);
    }
    /* Hedef kip ve operatör girdisi. */
    if (--g_hedef_kalan <= 0) {
        g_hedef = HEDEF_KIPLER[kc_rng_aralik(&R, 8u) & 7u];
        g_hedef_kalan = 1000 + (int)kc_rng_aralik(&R, 3000u);
    }
    if (olasilik(1u, 200u)) {
        operator_rastgele();
    }
    /* Çağrı türü. Reddedilen çağrıların yarısı (gevşek dönemde de) kısıtlayan örnek taşır. */
    uint32_t tur = kc_rng_aralik(&R, 1000u);
    bool red = false;
    uint8_t estop_asil = ka_ayar.estop;
    if (!oturum_sonu && (tur < 6u || (tur >= 30u && tur < 34u)) && olasilik(1u, 2u)) {
        ka_ayar.estop = 1u;
    }
    ida_in_t *in;
    if (oturum_sonu) {
        in = kk_girdi_oturum_sonu();
    } else if (tur < 6u) {
        in = kk_girdi_seq(kc_son_seq());
        red = true;
    } else {
        if (tur < 20u) {
            ka_ayar.tick = true;
        } else if (tur < 30u) {
            ka_ayar.imu_bozuk = true;
        }
        in = kk_girdi();
        if (tur >= 30u && tur < 34u) {
            in->narrow.mask = (uint8_t)(in->narrow.mask | 0x10u); /* BAD_INPUT (A4-K113) */
            red = true;
        }
    }
    ka_ayar.estop = estop_asil;
    if (oturum_baslangici || (g_kalkis && olasilik(1u, 12u) && !oturum_sonu)) {
        in->plat.link_event = (uint8_t)IDA_LINKEV_SESSION_START;
    }
    /* inst 1 ve VALID = 0 emniyet örnekleri. */
    if (!oturum_sonu && in->n_samples < IDA_PORT_MAX_SAMPLES && olasilik(4u, 1000u)) {
        (void)ka_emniyet_ekle(in, 1u, (uint8_t)kc_rng_aralik(&R, 2u), KC_TUP_ACIK, KC_TUP_ACIK);
    }
    if (!oturum_sonu && in->n_samples < IDA_PORT_MAX_SAMPLES && olasilik(3u, 1000u)) {
        ida_sample_t *s = ka_emniyet_ekle(in, 0u, (uint8_t)kc_rng_aralik(&R, 2u), KC_TUP_ACIK, KC_TUP_ACIK);
        s->hdr.flags = 0u;
    }
    /* Operatör: araç düzeyindeki genişleticiler yalnız olağan step'te (kabul_atis_ozellik.c'deki
       nota bakınız). */
    bool temiz = !oturum_sonu && !red && !ka_ayar.tick && !ka_ayar.imu_bozuk && ka_ayar.link_state == IDA_LINK_RUNNING;
    if (temiz && !armed && olasilik(1u, 10u) && in->n_cmds < IDA_PORT_MAX_CMDS) {
        (void)ka_komut(in, (uint8_t)IDA_CMD_ARM);
    } else if (temiz && fs != IDA_FS_NONE && olasilik(1u, 50u) && in->n_cmds < IDA_PORT_MAX_CMDS) {
        (void)ka_komut(in, (uint8_t)IDA_CMD_FAILSAFE_RESET);
    } else if (temiz && armed && kip != g_hedef && olasilik(1u, 10u)) {
        kip_ekle(in, g_hedef);
    }
    if (!oturum_sonu && olasilik(1u, 15000u)) {
        (void)ka_mandal(in, (uint8_t)IDA_NARROW_DISARM);
    }
    if (!oturum_sonu) {
        kk_operator_ekle(in);
    }
    (void)kk_cagir();
}

static void t_ozellik(void)
{
    char on[160];
    kc_rng_kur(&R, TOHUM);
    kk_kur();
    operator_rastgele();
    g_basili = 0;
    g_serbest = 400;
    g_kalkis = false;
    g_kesinti = 0;
    g_donma = 0;
    g_oturumsuz = 0;
    g_hedef = (uint8_t)IDA_MODE_GUIDED;
    g_hedef_kalan = 2000;
    /* Oturum başlangıcı da dizinin parçasıdır (hazırlık adımları da denetlenir). */
    ida_in_t *in = kk_girdi();
    in->plat.link_event = (uint8_t)IDA_LINKEV_SESSION_START;
    (void)kk_cagir();
    for (int k = 0; k < ADIM_SAYISI; ++k) {
        (void)snprintf(on, sizeof on, "özellik, tohum 0x%016llx, adım %d", (unsigned long long)TOHUM, k);
        ka_baglam_on(on);
        adim();
    }
    ka_baglam_on(NULL);
    const kk_sayac_t *s = kk_sayac();
    char m[640];
    (void)snprintf(m, sizeof m,
                   "özellik kapsamı (tohum 0x%016llx, %llu çağrı): nötr dışı %llu (gevşekken %llu), kesin kısıtlayan %llu, geçiş %llu (önceki "
                   "çıktı nötr dışı %llu), kalkış %llu; geçiş nedenleri: basılı %llu, VALID=0 %llu, inst≠0 %llu, red %llu, "
                   "oturum sonu %llu, bağlantı %llu, bayat alım %llu, bayat ölçüm %llu; kısıtlayan çağrı: bağlantı %llu",
                   (unsigned long long)TOHUM, (unsigned long long)s->cagri, (unsigned long long)s->notr_disi,
                   (unsigned long long)s->notr_disi_gevsek, (unsigned long long)s->kisitlayan, (unsigned long long)s->gecis, (unsigned long long)s->gecis_notr_disi,
                   (unsigned long long)s->kalkis, (unsigned long long)s->gecis_neden[KK_N_BASILI],
                   (unsigned long long)s->gecis_neden[KK_N_GECERSIZ], (unsigned long long)s->gecis_neden[KK_N_INST],
                   (unsigned long long)s->gecis_neden[KK_N_RED], (unsigned long long)s->gecis_neden[KK_N_OTURUM_SONU],
                   (unsigned long long)s->gecis_neden[KK_N_BAGLANTI], (unsigned long long)s->gecis_neden[KK_N_BAYAT_ALIM],
                   (unsigned long long)s->gecis_neden[KK_N_BAYAT_OLCUM], (unsigned long long)s->kisit_neden[KK_N_BAGLANTI]);
    TEST_MESSAGE(m);
    KA_DOGRULA(s->cagri >= (uint64_t)ADIM_SAYISI && s->notr_disi_gevsek >= 20000u && s->kisitlayan >= 20000u &&
                   s->gecis_notr_disi >= 200u && s->gecis_neden[KK_N_BASILI] >= 50u && s->gecis_neden[KK_N_GECERSIZ] >= 5u &&
                   s->gecis_neden[KK_N_INST] >= 5u && s->gecis_neden[KK_N_RED] >= 5u && s->gecis_neden[KK_N_OTURUM_SONU] >= 2u &&
                   s->gecis_neden[KK_N_BAYAT_ALIM] >= 5u && s->gecis_neden[KK_N_BAYAT_OLCUM] >= 5u &&
                   s->kisit_neden[KK_N_BAGLANTI] >= 5u,
               "kapsam yetersiz: dizi nötr dışı çıktıyla acil durdurmanın kısıtlayan durumlarını yeterince birleştirmedi, "
               "değişmez sınanmadı (%s)",
               m);
}

static const ka_test_t TESTLER[] = {
    {"ozellik", t_ozellik},
};

int main(int argc, char **argv)
{
    return ka_main(argc, argv, __FILE__, TESTLER, sizeof TESTLER / sizeof TESTLER[0]);
}
