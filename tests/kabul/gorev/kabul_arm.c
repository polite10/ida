/*
 * tests/kabul/gorev — görev md.2: araç silahlandırmasının (ARM) ön koşulları.
 *
 * Her ön koşul için: koşul tutmazken ARM'ın reddi ve doğru neden; koşul tutarken kabul.
 * Ret nedenleri ve sıraları A4 3.17 ARM satırındandır (EVENTS_OFF > PARAM_INVALID >
 * PARAM_BUSY > NOT_ARMABLE > ACCEPTED); 4.14 son paragrafı genişleticilerde NARROWED >
 * IMU_REJECTED > IMU_LAG önceliğini verir.
 *
 * Ön koşullar mimar kararı W3-K3'tür (docs/gorevler/W3-ortak.md): yalnız 3.17 satırı;
 * kestirim OK ya da DEGRADED yeterli; sızıntı ve düşük batarya ret; bağlantı durumu ayrı ön
 * koşul değil. W3-K1: acil durdurma (fiş, pim) araç ARM'ını engellemez, silahsızlandırmaz.
 * "Kabul" sınamaları, gözlenen ön koşulların tuttuğu duruma (kg_hazir: est_level OK ya da
 * DEGRADED, kip INIT dışı, fs_level en çok WARN, kestirim bozulması dışında arıza nedeni
 * yok, events_ok, param_ok, imu_lag 0, data_ok, kayıt sürmüyor) ulaşılmasını gerektirir.
 */
#include <stdio.h>
#include <string.h>

#include "kabul_gorev.h"
#include "unity.h"

void setUp(void)
{
}

void tearDown(void)
{
}

/* ARM gönderir; sonucu döndürür (çağrının dönüşü OK olmalı). */
static const ida_cmd_result_t *arm_dene(ida_in_t *in)
{
    ida_cmd_t *c = kg_komut(in, (uint8_t)IDA_CMD_ARM);
    int j = (int)(c - in->cmds);
    ida_rc_t rc = kg_cagir(in);
    KG_DOGRULA(rc == IDA_RC_OK, "ARM çağrısı %d döndü, OK bekleniyordu", (int)rc);
    return kg_sonuc(j);
}

static void arm_ret_beklenir(ida_in_t *in, uint16_t neden, const char *kural)
{
    bool armed_once = kg_out()->armed;
    const ida_cmd_result_t *r = arm_dene(in);
    KG_DOGRULA(r->kind == IDA_RES_ACK && r->ack == IDA_ACK_REJECTED && r->reason == neden,
               "ARM sonucu ack %u neden %u; REJECTED / %u bekleniyordu (%s)", (unsigned)r->ack, (unsigned)r->reason,
               (unsigned)neden, kural);
    KG_DOGRULA(kg_out()->armed == armed_once && !kg_out()->armed, "reddedilen ARM'dan sonra armed = %d (%s)",
               (int)kg_out()->armed, kural);
}

/* Silahlandırmanın gözlenen ön koşullarının tuttuğu duruma gelir (kip init sonrası ne ise;
   ARM kipten bağımsızdır, A4-K127, 3.17). */
static void hazir_ol(const char *amac)
{
    kg_kur_varsayilan();
    kg_hazir_olmali(KG_HAZIR_AZAMI, amac);
}

/* ---- EVENTS_OFF ------------------------------------------------------------------- */

/* A4-K61, 3.17 ARM satırı: init'te boot_count_ok = 0 → olay kimliği yok, ARM EVENTS_OFF. */
static void test_md2_events_off_boot_count_ok_0(void)
{
    kg_kurulum_t k;
    kg_baglam("md2 EVENTS_OFF boot_count_ok=0");
    kg_kurulum_varsayilan(&k);
    k.boot_ok = false;
    kg_kur(&k);
    kg_adimlar(400);
    KG_DOGRULA(!kg_out()->status.events_ok, "boot_count_ok = 0 iken events_ok = 1 (A4-K61)");
    arm_ret_beklenir(kg_girdi(KG_OLAGAN), (uint16_t)IDA_REASON_EVENTS_OFF, "A4-K61, 3.17");
    /* Bir sonraki çağrıda da (koşul sürüyor). */
    arm_ret_beklenir(kg_girdi(KG_OLAGAN), (uint16_t)IDA_REASON_EVENTS_OFF, "A4-K61, 3.17");
}

/* A4-K61: boot_count 1–0xFFFFFFFE dışında (boot_count_ok = 1 olsa da) → EVENTS_OFF. */
static void test_md2_events_off_boot_count_aralik_disi(void)
{
    static const uint32_t degerler[2] = {0u, 0xFFFFFFFFu};
    for (int i = 0; i < 2; ++i) {
        kg_kurulum_t k;
        kg_baglam("md2 EVENTS_OFF boot_count=0x%08lx", (unsigned long)degerler[i]);
        kg_kurulum_varsayilan(&k);
        k.boot_count = degerler[i];
        kg_kur(&k);
        kg_adimlar(200);
        arm_ret_beklenir(kg_girdi(KG_OLAGAN), (uint16_t)IDA_REASON_EVENTS_OFF, "A4-K61, 3.17");
    }
}

/* ---- PARAM_INVALID ---------------------------------------------------------------- */

/* A4-K78, 3.17: blok yokken (PARAM_DEFAULTS) ARM → PARAM_INVALID; param_ok = 0. */
static void test_md2_param_invalid_blok_yok(void)
{
    kg_kurulum_t k;
    kg_baglam("md2 PARAM_INVALID blok yok");
    kg_kurulum_varsayilan(&k);
    k.blok = KG_BLOK_YOK;
    kg_kur(&k);
    kg_adimlar(400);
    KG_DOGRULA(!kg_out()->status.param_ok, "blok yokken param_ok = 1 (A4-K78)");
    arm_ret_beklenir(kg_girdi(KG_OLAGAN), (uint16_t)IDA_REASON_PARAM_INVALID, "A4-K78, 3.17");
}

/* A4-K78, K80 (4): CRC'si bozuk blok → PARAM_DEFAULTS; ARM → PARAM_INVALID. */
static void test_md2_param_invalid_blok_bozuk(void)
{
    kg_kurulum_t k;
    kg_baglam("md2 PARAM_INVALID blok bozuk");
    kg_kurulum_varsayilan(&k);
    k.blok = KG_BLOK_BOZUK;
    kg_kur(&k);
    kg_adimlar(400);
    arm_ret_beklenir(kg_girdi(KG_OLAGAN), (uint16_t)IDA_REASON_PARAM_INVALID, "A4-K78, K80, 3.17");
}

/* ---- PARAM_BUSY ------------------------------------------------------------------- */

/* A4-K83, 3.17: plat.param_store = BUSY iken ARM → PARAM_BUSY. */
static void test_md2_param_busy_ret(void)
{
    kg_baglam("md2 PARAM_BUSY ret");
    kg_kur_varsayilan();
    kg_adimlar(400);
    kg_dunya()->pstore = (uint8_t)IDA_PSTORE_BUSY;
    arm_ret_beklenir(kg_girdi(KG_OLAGAN), (uint16_t)IDA_REASON_PARAM_BUSY, "A4-K83, 3.17");
    arm_ret_beklenir(kg_girdi(KG_OLAGAN), (uint16_t)IDA_REASON_PARAM_BUSY, "A4-K83, 3.17");
}

/* A4-K83, 3.17 (4): kayıt bitince (IDLE) ön koşul yeniden tutar ve ARM kabul edilir. */
static void test_md2_param_busy_kalkinca_kabul(void)
{
    kg_baglam("md2 PARAM_BUSY kalkınca kabul");
    hazir_ol("PARAM_BUSY sonrası kabul");
    kg_dunya()->pstore = (uint8_t)IDA_PSTORE_BUSY;
    arm_ret_beklenir(kg_girdi(KG_OLAGAN), (uint16_t)IDA_REASON_PARAM_BUSY, "A4-K83, 3.17");
    kg_dunya()->pstore = (uint8_t)IDA_PSTORE_IDLE;
    KG_DOGRULA(kg_hazirla(400), "kayıt bitince hazır olmadı: %s", kg_hazir_neden());
    (void)kg_silahlan();
}

/* ---- Satır sırası ----------------------------------------------------------------- */

/* 3.17 ARM satırı: birden çok ön koşul tutmazken ilk satırdaki neden döner. */
static void test_md2_neden_sirasi(void)
{
    kg_kurulum_t k;
    /* (a) EVENTS_OFF + PARAM_INVALID + PARAM_BUSY + acil durdurma → EVENTS_OFF */
    kg_baglam("md2 sıra (a)");
    kg_kurulum_varsayilan(&k);
    k.boot_ok = false;
    k.blok = KG_BLOK_YOK;
    kg_kur(&k);
    kg_adimlar(200);
    kg_dunya()->pstore = (uint8_t)IDA_PSTORE_BUSY;
    kg_dunya()->estop = 1u;
    kg_adimlar(40);
    arm_ret_beklenir(kg_girdi(KG_OLAGAN), (uint16_t)IDA_REASON_EVENTS_OFF, "3.17 sıra");
    /* (b) PARAM_INVALID + PARAM_BUSY + acil durdurma → PARAM_INVALID */
    kg_baglam("md2 sıra (b)");
    kg_kurulum_varsayilan(&k);
    k.blok = KG_BLOK_YOK;
    kg_kur(&k);
    kg_adimlar(200);
    kg_dunya()->pstore = (uint8_t)IDA_PSTORE_BUSY;
    kg_dunya()->estop = 1u;
    kg_adimlar(40);
    arm_ret_beklenir(kg_girdi(KG_OLAGAN), (uint16_t)IDA_REASON_PARAM_INVALID, "3.17 sıra");
    /* (c) PARAM_BUSY + acil durdurma + bağlantı yok → PARAM_BUSY (ikisi ön koşul değil; W3-K1, K3) */
    kg_baglam("md2 sıra (c)");
    kg_kur_varsayilan();
    kg_adimlar(200);
    kg_dunya()->pstore = (uint8_t)IDA_PSTORE_BUSY;
    kg_dunya()->estop = 1u;
    kg_adimlar(40);
    kg_dunya()->link = false;
    arm_ret_beklenir(kg_girdi(KG_OLAGAN), (uint16_t)IDA_REASON_PARAM_BUSY, "3.17 sıra");
}

/* ---- NOT_ARMABLE: kestirim düzeyi --------------------------------------------------- */

/* Görev md.2 (kestirim düzeyi), A4-K128, K39, 3.17: init'ten beri hiç IMU yokken (kestirim
   kurulmamış, IMU bayat) ARM → NOT_ARMABLE. */
static void test_md2_kestirim_yokken_ret(void)
{
    kg_baglam("md2 kestirim yok");
    kg_kur_varsayilan();
    kg_dunya()->imu = false; /* yalnız zaman çerçeveli tick */
    kg_adimlar(200);
    KG_DOGRULA(!kg_kisit_estop(), "düzenek: acil durdurma gevşetilemedi");
    arm_ret_beklenir(kg_girdi(KG_OLAGAN), (uint16_t)IDA_REASON_NOT_ARMABLE, "görev md.2, A4-K128, 3.17");
}

/* ---- Acil durdurma araç ARM'ını engellemez (W3-K1) ---------------------------------- */

/* W3-K1, W3-K2, A4-K116, K44 son cümle, 3.17 (kapalı satır): acil durdurma kısıtlayanken
   (süregelen kısıt; bu çağrıda kapanma yok) öteki ön koşullar tutuyorsa ARM ACCEPTED;
   acil durdurma silahlı aracı silahsızlandırmaz (süregelen kısıtta da, yeniden kapanmada da). */
static void test_md2_acil_durdurma_arm_engellemez(void)
{
    ida_in_t *in;
    kg_baglam("md2 acil durdurma ARM'ı engellemez");
    hazir_ol("acil durdurma ARM'ı engellemez");
    kg_dunya()->emniyet = false; /* emniyet örneklerini bu sınama verir */

    /* Kapanma çağrısı: ARM yok. */
    in = kg_girdi(KG_OLAGAN);
    (void)kg_emniyet(in, 0u, 1u, KG_TUP_ACIK, KG_TUP_ACIK, true);
    (void)kg_cagir(in);
    KG_DOGRULA(kg_kisit_estop(), "düzenek: acil durdurma modele işlenmedi");
    if (kg_gevsetme_gozlendi()) {
        /* Gevşetmeyi uygulayan çekirdekte (W4) kapanma daraltıcı girdidir. */
        KG_DOGRULA(kg_out()->health.narrowed, "acil durdurma kapanması daraltma değil (A4-K111, K45)");
    }
    KG_DOGRULA(kg_hazirla(400), "acil durdurma etkinken hazır olmadı: %s", kg_hazir_neden());

    /* (a) Kısıt sürüyor (yinelenen kısıtlayan içerik daraltıcı girdi değil, A4-K44) + ARM. */
    in = kg_girdi(KG_OLAGAN);
    (void)kg_emniyet(in, 0u, 1u, KG_TUP_ACIK, KG_TUP_ACIK, true);
    ida_cmd_t *c = kg_komut(in, (uint8_t)IDA_CMD_ARM);
    int j = (int)(c - in->cmds);
    KG_DOGRULA(kg_cagir(in) == IDA_RC_OK, "ARM çağrısı");
    const ida_cmd_result_t *r = kg_sonuc(j);
    KG_DOGRULA(r->ack == IDA_ACK_ACCEPTED && kg_out()->armed,
               "acil durdurma etkinken ARM ack %u neden %u, armed %d; ACCEPTED bekleniyordu (W3-K1, 3.17)",
               (unsigned)r->ack, (unsigned)r->reason, (int)kg_out()->armed);

    /* (b) Kısıt sürerken silahlı kalır (düzeneğin D3'ü her adımda da denetler). */
    kg_dunya()->estop = 1u;
    kg_dunya()->emniyet = true;
    kg_adimlar(60);
    KG_DOGRULA(kg_out()->armed, "acil durdurma sürerken araç silahsızlandı (W3-K1)");

    /* (c) Gevşetme ve yeniden kapanma silahlı aracı silahsızlandırmaz. */
    kg_dunya()->estop = 0u;
    kg_adimlar(40);
    in = kg_girdi(KG_OLAGAN);
    (void)kg_emniyet(in, 0u, 1u, KG_TUP_ACIK, KG_TUP_ACIK, true);
    (void)kg_cagir(in);
    KG_DOGRULA(kg_out()->armed, "silahlıyken acil durdurma geldiği çağrının sonunda armed = 0 (W3-K1)");
    kg_adimlar(20);
    KG_DOGRULA(kg_out()->armed, "acil durdurmadan sonraki adımlarda armed = 0 (W3-K1)");
}

/* W3-K1, A4-K73, K116, K128: oturum sonu silahsızlandırır (A4-K73); yeni oturumda emniyet
   örneği gelmeden de (etkin değerler kısıtlayan) ön koşullar tutunca ARM kabul edilir. */
static void test_md2_oturum_sonrasi_emniyet_ornegi_gerekmez(void)
{
    kg_baglam("md2 oturum sonrası emniyet");
    hazir_ol("oturum sonrası emniyet");
    (void)kg_silahlan();
    (void)kg_cagir(kg_girdi(KG_OTURUM_SONU));
    KG_DOGRULA(!kg_out()->armed && kg_kisit_estop(), "oturum sonu sonrası armed = 1 (A4-K73)");
    kg_dunya()->emniyet = false;
    kg_hazir_olmali(KG_HAZIR_AZAMI, "yeni oturumda, emniyet örneği yokken");
    KG_DOGRULA(kg_kisit_estop(), "düzenek: emniyet örneği verilmemeliydi");
    (void)kg_silahlan();
}

/* ---- NOT_ARMABLE: arıza eylemi ve arm reddi gerektiren arıza kaydı (W3-K3) ---------- */

static void ariza_eylemi_ret(const char *ad, void (*uygula)(kg_dunya_t *), uint32_t neden_bitleri)
{
    kg_baglam("md2 arıza eylemi: %s", ad);
    hazir_ol(ad);
    uygula(kg_dunya());
    int i = 0;
    while (i < 400 && (kg_out()->status.fs_reasons & neden_bitleri) == 0u && kg_out()->fs_level < IDA_FS_HOLD) {
        (void)kg_adim();
        ++i;
    }
    /* Görev md.3: bu arızanın bir eylemi vardır; 2 s içinde nedeni ya da eylemi görünmeli. */
    KG_DOGRULA((kg_out()->status.fs_reasons & neden_bitleri) != 0u || kg_out()->fs_level >= IDA_FS_HOLD,
               "%s 2 s içinde ne arıza nedeni ne arıza eylemi üretti (fs_level %u, fs_reasons 0x%lx; görev md.3)", ad,
               (unsigned)kg_out()->fs_level, (unsigned long)kg_out()->status.fs_reasons);
    arm_ret_beklenir(kg_girdi(KG_OLAGAN), (uint16_t)IDA_REASON_NOT_ARMABLE, "3.17, W3-K3: sızıntı ve düşük batarya");
}

static void sizinti_uygula(kg_dunya_t *d)
{
    d->sizinti = 1u;
}

static void batarya_uygula(kg_dunya_t *d)
{
    d->batt_v = 3.0f;
}

/* 3.17 ARM satırı, W3-K3, görev md.2–3: sızıntı sürerken ARM → NOT_ARMABLE. */
static void test_md2_ariza_eylemi_sizinti(void)
{
    ariza_eylemi_ret("sızıntı", sizinti_uygula, IDA_FSR_LEAK);
}

/* 3.17 ARM satırı, W3-K3, görev md.2–3: düşük batarya sürerken ARM → NOT_ARMABLE. */
static void test_md2_ariza_eylemi_dusuk_batarya(void)
{
    ariza_eylemi_ret("düşük batarya", batarya_uygula, IDA_FSR_BATT_LOW | IDA_FSR_BATT_CRIT);
}

/* ---- Kabul ------------------------------------------------------------------------ */

/* 3.17 (4), A4 3.15 ARM_CHANGE: bütün ön koşullar tutarken ARM ACCEPTED, armed = 1,
   ARM_CHANGE günlüğü (arg0 1, arg1 0); silahlıyken ikinci ARM ACCEPTED ve etkisiz. */
static void test_md2_kabul(void)
{
    kg_baglam("md2 kabul");
    hazir_ol("kabul");
    uint32_t dusen_once = kg_out()->status.cnt.log_dropped;
    (void)kg_silahlan();
    const ida_out_t *o = kg_out();
    bool bulundu = false;
    for (unsigned i = 0; i < o->n_logs; ++i) {
        if (o->logs[i].code == IDA_LOG_ARM_CHANGE) {
            KG_DOGRULA(o->logs[i].arg0 == 1 && o->logs[i].arg1 == 0, "ARM_CHANGE arg0 %ld arg1 %ld (3.15)",
                       (long)o->logs[i].arg0, (long)o->logs[i].arg1);
            bulundu = true;
        }
    }
    KG_DOGRULA(bulundu || o->status.cnt.log_dropped != dusen_once, "silahlanma çağrısında ARM_CHANGE günlüğü yok (3.15)");
    (void)kg_silahlan(); /* zaten silahlı: ACCEPTED (etkisiz) */
    kg_adimlar(20);
    KG_DOGRULA(kg_out()->armed, "silahlı durum kendiliğinden düştü (arıza yokken)");
}

/* A4 §5 (K116 satırı), iskelet §3.2 ("süregelen kısıtlama ilgisiz komutları reddetmez"):
   bir tüpün pimi takılıyken (etkin) araç ARM'ı kabul edilir. */
static void test_md2_kabul_pim_takili(void)
{
    ida_in_t *in;
    kg_baglam("md2 kabul pim takılı");
    hazir_ol("pim takılı");
    kg_dunya()->tube[0] = (uint8_t)(IDA_TUBE_LOADED | IDA_TUBE_PLUG_IN); /* pim takılı */
    in = kg_girdi(KG_OLAGAN);
    (void)kg_emniyet(in, 0u, 0u, kg_dunya()->tube[0], KG_TUP_ACIK, true);
    (void)kg_cagir(in); /* kapanma çağrısı (A4-K111) */
    KG_DOGRULA(kg_hazirla(400), "pim takıldıktan sonra hazır olmadı: %s", kg_hazir_neden());
    (void)kg_silahlan();
}

/* A4-K78, K81, K82, K83: blok yokken commit + kayıt, platformun OK bildirmesiyle param_ok
   1 olur; kayıt sürerken ARM PARAM_INVALID (satır sırası); sonra ARM kabul. */
static void test_md2_parametre_kurtarma(void)
{
    kg_kurulum_t k;
    ida_in_t *in;
    kg_baglam("md2 parametre kurtarma");
    kg_kurulum_varsayilan(&k);
    k.blok = KG_BLOK_YOK;
    kg_kur(&k);
    kg_adimlar(200);
    in = kg_girdi(KG_OLAGAN);
    ida_cmd_t *cc = kg_komut(in, (uint8_t)IDA_CMD_PARAM_COMMIT);
    int jc = (int)(cc - in->cmds);
    ida_cmd_t *cs = kg_komut(in, (uint8_t)IDA_CMD_PARAM_SAVE);
    int js = (int)(cs - in->cmds);
    KG_DOGRULA(kg_cagir(in) == IDA_RC_OK, "commit/kayıt çağrısı");
    const ida_cmd_result_t *r_commit = kg_sonuc(jc);
    KG_DOGRULA(r_commit->ack == IDA_ACK_ACCEPTED, "bekleyen değersiz PARAM_COMMIT ack %u neden %u (A4-K78, K81)",
               (unsigned)r_commit->ack, (unsigned)r_commit->reason);
    const ida_cmd_result_t *rs = kg_sonuc(js);
    KG_DOGRULA(rs->kind == IDA_RES_PARAM_SAVE && rs->ack == IDA_ACK_ACCEPTED && rs->save.gen == 1u &&
                   rs->save.len > 0u,
               "PARAM_SAVE kind %u ack %u neden %u gen %lu len %lu; ACCEPTED, gen 1 bekleniyordu (A4-K82)",
               (unsigned)rs->kind, (unsigned)rs->ack, (unsigned)rs->reason, (unsigned long)rs->save.gen,
               (unsigned long)rs->save.len);
    /* Kayıt sürüyor: param_ok hâlâ 0, ARM PARAM_INVALID (PARAM_INVALID satırı PARAM_BUSY'den önce). */
    kg_dunya()->pstore = (uint8_t)IDA_PSTORE_BUSY;
    arm_ret_beklenir(kg_girdi(KG_OLAGAN), (uint16_t)IDA_REASON_PARAM_INVALID, "A4-K78, 3.17 sıra");
    /* Kayıt tamamlandı (OK, gen 1). */
    kg_dunya()->pstore = (uint8_t)IDA_PSTORE_OK;
    kg_dunya()->pgen = 1u;
    kg_adimlar(2);
    KG_DOGRULA(kg_out()->status.param_ok, "kayıt OK/1 bildirildikten sonra param_ok = 0 (A4-K78, K83)");
    kg_hazir_olmali(KG_HAZIR_AZAMI, "parametre kurtarma sonrası kabul");
    (void)kg_silahlan();
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_md2_events_off_boot_count_ok_0);
    RUN_TEST(test_md2_events_off_boot_count_aralik_disi);
    RUN_TEST(test_md2_param_invalid_blok_yok);
    RUN_TEST(test_md2_param_invalid_blok_bozuk);
    RUN_TEST(test_md2_param_busy_ret);
    RUN_TEST(test_md2_neden_sirasi);
    RUN_TEST(test_md2_kestirim_yokken_ret);
    RUN_TEST(test_md2_param_busy_kalkinca_kabul);
    RUN_TEST(test_md2_acil_durdurma_arm_engellemez);
    RUN_TEST(test_md2_oturum_sonrasi_emniyet_ornegi_gerekmez);
    RUN_TEST(test_md2_ariza_eylemi_sizinti);
    RUN_TEST(test_md2_ariza_eylemi_dusuk_batarya);
    RUN_TEST(test_md2_kabul);
    RUN_TEST(test_md2_kabul_pim_takili);
    RUN_TEST(test_md2_parametre_kurtarma);
    return UNITY_END();
}
