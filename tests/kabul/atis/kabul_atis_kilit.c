/*
 * tests/kabul/atis — kilitli arıza: kapatma sonucuyla kilitlenme (G-Ç50), kilidin güvenli
 * duruma dönüşlerle korunması (G-Ç19, F13), FC_RESET (G-Ç20, G-Ç46), günlükler (G-Ç27).
 *
 * FC silahlanmadan FAULT_LATCHED'e iki yol vardır: SAFETY_CLOSE'un EXPIRED_NO_ACK ya da
 * bilinmeyen sonucu (G-Ç50; W5-K11 (a)) ve geri okuma çelişkisi (G-Ç53; ka_kilitle).
 */
#include <string.h>

#include "kabul_atis.h"
#include "unity.h"

static void gevsek(void)
{
    ka_ayar.estop = 0u;
    ka_ayar.tup[0] = KC_TUP_ACIK;
    ka_ayar.tup[1] = KC_TUP_ACIK;
    ka_ayar.emniyet = true;
}

static bool kilitli(void)
{
    return ka_out()->fc.state == IDA_FCS_FAULT_LATCHED;
}

/* ---- G-Ç50 ----------------------------------------------------------------------------------- */

static const ka_olay_t *kapanma_uret(void)
{
    gevsek();
    ka_adimlar(2);
    ka_ayar.tup[1] = (uint8_t)(KC_TUP_ACIK & (uint8_t)~IDA_TUBE_PIN_OUT);
    (void)ka_adim();
    const ka_olay_t *e = ka_son_olay((uint8_t)IDA_EVT_SAFETY_CLOSE, -1);
    KA_DOGRULA(e != NULL && e->cagri == ka_cagri_no(), "tüp 1'in pimi takılınca SAFETY_CLOSE çıkmadı (G-Ç15)");
    return e;
}

static void t_gc50_kapanma_sonucu(void)
{
    /* (a) EXPIRED_NO_ACK: sonucun geldiği çağrıda FAULT_LATCHED ve kapatma yinelenir. */
    ka_kur(2);
    ka_oturum_baslat();
    ka_ayar.politika[IDA_EVT_SAFETY_CLOSE].kod = (uint8_t)IDA_EVR_EXPIRED_NO_ACK;
    (void)kapanma_uret();
    ka_ayar.politika[IDA_EVT_SAFETY_CLOSE].kod = (uint8_t)IDA_EVR_APPLIED;
    (void)ka_adim();
    KA_DOGRULA(kilitli(), "(a) SAFETY_CLOSE sonucu EXPIRED_NO_ACK: fc.state %s, FAULT_LATCHED bekleniyordu (G-Ç50; W5-K11 (a))",
               ka_durum_adi(ka_out()->fc.state));
    bool yineleme = ka_kapanma_var(ka_out(), 1u);
    if (!yineleme) {
        (void)ka_adim();
        yineleme = ka_kapanma_var(ka_out(), 1u);
    }
    KA_DOGRULA(yineleme, "(a) sonucu doğrulanamayan kapatma en geç bir sonraki çağrıda yinelenmedi (W5-K11 (a))");
    /* (b) sonuç hiç gelmez: apply_by + 100 ms sonra FAULT_LATCHED, öncesinde değil. */
    ka_kur(2);
    ka_oturum_baslat();
    ka_ayar.politika[IDA_EVT_SAFETY_CLOSE].kod = KA_SONUC_YOK;
    const ka_olay_t *e = kapanma_uret();
    int64_t t0 = e->now_w;
    int64_t sinir = t0 + (int64_t)e->e.apply_by_ns + (int64_t)IDA_EVT_RESULT_MARGIN_NS;
    ka_ayar.politika[IDA_EVT_SAFETY_CLOSE].kod = (uint8_t)IDA_EVR_APPLIED;
    bool oldu = false;
    for (int k = 0; k < 60 && !oldu; ++k) {
        (void)ka_adim();
        int64_t now = ka_simdi();
        if (kilitli()) {
            oldu = true;
            KA_DOGRULA(now >= sinir - KA_P, "(b) %lld ms'de (sınır %lld ms) FAULT_LATCHED: sonuç süresi dolmadan (G-Ç42, G-Ç50)",
                       (long long)((now - t0) / KA_MS), (long long)((sinir - t0) / KA_MS));
        } else {
            KA_DOGRULA(now <= sinir, "(b) apply_by + 100 ms geçti (%lld ms) ama fc.state %s (G-Ç50)", (long long)((now - t0) / KA_MS),
                       ka_durum_adi(ka_out()->fc.state));
        }
    }
    KA_DOGRULA(oldu, "(b) sonucu gelmeyen kapatma FAULT_LATCHED'e götürmedi (G-Ç50)");
    /* (c) APPLIED: kilit yok. */
    ka_kur(2);
    ka_oturum_baslat();
    (void)kapanma_uret();
    ka_adimlar(40);
    KA_DOGRULA(!kilitli(), "(c) APPLIED kapatma sonrası FAULT_LATCHED");
}

/* ---- G-Ç19, G-Ç25, G-Ç18 (kilitli), F13 ---------------------------------------------------- */

enum {
    GD_FCDISARM,
    GD_FCABORT,
    GD_CMDS_ABORT,
    GD_DISARM,
    GD_OTURUM_SONU,
    GD_ESTOP,
    GD_FIS,
    GD_PIM,
    GD_BAYAT,
    GD_HB,
    GD_IMU,
    GD_SONUC,
    GD_SAYI
};
static const char *const GD_AD[GD_SAYI] = {"FC_DISARM", "FC_ABORT", "cmds[] FC_ABORT", "DISARM", "SESSION_END", "acil durdurma",
                                           "fiş", "pim", "emniyet örneği bayatlığı", "yer istasyonu kalp atışı kaybı",
                                           "IMU bayatlığı", "olay sonucu"};

static void guvenli_donus_uygula(int d)
{
    ida_in_t *in;
    switch (d) {
    case GD_FCDISARM:
        in = ka_girdi();
        (void)ka_mandal(in, (uint8_t)IDA_NARROW_FC_DISARM);
        (void)ka_cagir();
        break;
    case GD_FCABORT:
        in = ka_girdi();
        (void)ka_mandal(in, (uint8_t)IDA_NARROW_FC_ABORT);
        (void)ka_cagir();
        break;
    case GD_CMDS_ABORT:
        in = ka_girdi();
        (void)ka_fc(in, (uint8_t)IDA_FC_ABORT, 1u);
        (void)ka_cagir();
        break;
    case GD_DISARM:
        in = ka_girdi();
        (void)ka_mandal(in, (uint8_t)IDA_NARROW_DISARM);
        (void)ka_cagir();
        break;
    case GD_OTURUM_SONU:
        in = ka_girdi();
        in->plat.link_event = (uint8_t)IDA_LINKEV_SESSION_END;
        in->plat.link_state = (uint8_t)IDA_LINK_NONE;
        (void)ka_cagir();
        KA_DOGRULA(ka_olay_say(ka_out(), (uint8_t)IDA_EVT_SAFETY_CLOSE, IDA_TUBE_ALL) == 1 && !ka_out()->armed,
                   "kilitliyken SESSION_END: SAFETY_CLOSE(ALL) ve armed = 0 bekleniyordu (G-Ç18, G-Ç19)");
        KA_DOGRULA(kilitli(), "kilitliyken SESSION_END çağrısında fc.state %s (G-Ç18, G-Ç19)", ka_durum_adi(ka_out()->fc.state));
        ka_ayar.link_state = (uint8_t)IDA_LINK_NONE;
        ka_adimlar(3);
        ka_ayar.link_state = (uint8_t)IDA_LINK_RUNNING;
        in = ka_girdi();
        in->plat.link_event = (uint8_t)IDA_LINKEV_SESSION_START;
        (void)ka_cagir();
        break;
    case GD_ESTOP:
        ka_ayar.estop = 1u;
        (void)ka_adim();
        KA_DOGRULA(ka_kapanma_var(ka_out(), 0u), "kilitliyken acil durdurma: SAFETY_CLOSE yok (G-Ç19)");
        ka_adimlar(5);
        ka_ayar.estop = 0u;
        break;
    case GD_FIS:
        ka_ayar.tup[1] = (uint8_t)(ka_ayar.tup[1] & (uint8_t)~IDA_TUBE_PLUG_IN);
        (void)ka_adim();
        KA_DOGRULA(ka_kapanma_var(ka_out(), 1u), "kilitliyken fiş: SAFETY_CLOSE yok (G-Ç19)");
        ka_adimlar(5);
        ka_ayar.tup[1] = KC_TUP_ACIK;
        break;
    case GD_PIM:
        ka_ayar.tup[1] = (uint8_t)(ka_ayar.tup[1] & (uint8_t)~IDA_TUBE_PIN_OUT);
        (void)ka_adim();
        KA_DOGRULA(ka_kapanma_var(ka_out(), 1u), "kilitliyken pim: SAFETY_CLOSE yok (G-Ç19)");
        ka_adimlar(5);
        ka_ayar.tup[1] = KC_TUP_ACIK;
        break;
    case GD_BAYAT: {
        ka_ayar.emniyet = false;
        bool kapanma = false;
        for (int k = 0; k < 80; ++k) {
            (void)ka_adim();
            kapanma = kapanma || ka_kapanma_var(ka_out(), 0u);
            KA_DOGRULA(kilitli(), "kilitliyken emniyet örneği bayatlığı: fc.state %s (G-Ç19)", ka_durum_adi(ka_out()->fc.state));
        }
        KA_DOGRULA(kapanma, "kilitliyken bayatlıkta SAFETY_CLOSE yok (G-Ç19, G-Ç29)");
        ka_ayar.emniyet = true;
        break;
    }
    case GD_HB:
        ka_ayar.heartbeat = false;
        for (int k = 0; k < 2400; ++k) { /* 12 s */
            (void)ka_adim();
            KA_DOGRULA(kilitli(), "kilitliyken kalp atışı kaybı: fc.state %s (G-Ç19, G-Ç25)", ka_durum_adi(ka_out()->fc.state));
        }
        ka_ayar.heartbeat = true;
        break;
    case GD_IMU:
        ka_ayar.tick = true;
        ka_adimlar(10);
        ka_ayar.tick = false;
        break;
    case GD_SONUC:
        in = ka_girdi();
        ka_sonuc_ekle(in, ((uint64_t)KC_BOOT << 32) | UINT32_C(0x7FFF0000), (uint8_t)IDA_EVR_REJ_SAFE, 0);
        (void)ka_cagir();
        break;
    default:
        break;
    }
}

static void t_gc19_kilitli_guvenli_donus(void)
{
    for (int d = 0; d < GD_SAYI; ++d) {
        ka_kur(2);
        ka_oturum_ac();
        ka_arac_silahla();
        ka_kilitle();
        ka_kilit_nedeni_kaldir();
        guvenli_donus_uygula(d);
        KA_DOGRULA(kilitli(), "%s FAULT_LATCHED'i temizledi: fc.state %s (G-Ç19; W5-K8)", GD_AD[d], ka_durum_adi(ka_out()->fc.state));
        KA_DOGRULA(!ka_genisletici_olay_var(ka_out()), "%s: genişletici olay", GD_AD[d]);
        ka_adimlar(5);
        KA_DOGRULA(kilitli(), "%s sonrası FAULT_LATCHED kendiliğinden temizlendi (G-Ç19)", GD_AD[d]);
        KA_DOGRULA(!ka_out()->armed || (d != GD_DISARM && d != GD_OTURUM_SONU), "%s: silahlandırma kaldırılmadı", GD_AD[d]);
    }
}

static void t_f13_kilit_korunur(void)
{
    /* §7 F13: kilitliyken sırayla oturum sonu, kalp atışı kaybı, emniyet bayatlığı, acil
       durdurma, FC_DISARM, IMU bayatlığı; yalnız kabul edilen FC_RESET çıkarır, neden sürüyorsa
       aynı çağrıda yeniden kilitlenir. */
    static const int sira[] = {GD_OTURUM_SONU, GD_HB, GD_BAYAT, GD_ESTOP, GD_FCDISARM, GD_IMU};
    ka_kur(2);
    ka_oturum_ac();
    ka_arac_silahla();
    ka_kilitle();
    for (size_t i = 0; i < sizeof sira / sizeof sira[0]; ++i) {
        guvenli_donus_uygula(sira[i]);
        KA_DOGRULA(kilitli(), "F13: %s sonrası fc.state %s", GD_AD[sira[i]], ka_durum_adi(ka_out()->fc.state));
    }
    gevsek();
    ka_adimlar(3);
    /* IMU bayatlığı adımı (zaman çerçeveli tick'ler) IMU_LAG mandalını kurar (A4-K37); mandal
       kuruluyken FC_RESET 3.17 (2) gereği IMU_LAG alır (FC_RESET genişleticidir; A4-K104,
       K108). Mandalın çözülmesi beklenir (N_lag_clear ardışık çağrı). */
    for (int k = 0; k < 400 && ka_out()->health.imu_lag; ++k) {
        (void)ka_adim();
        KA_DOGRULA(kilitli(), "F13: IMU_LAG çözülürken fc.state %s", ka_durum_adi(ka_out()->fc.state));
    }
    KA_DOGRULA(!ka_out()->health.imu_lag, "F13: IMU_LAG mandalı 2 s'de çözülmedi (A4-K37)");
    /* Neden (PERMIT_OPEN = 1 geri okuması) sürüyor: FC_RESET kabul edilir, aynı çağrıda yeniden kilit. */
    ida_in_t *in = ka_girdi();
    uint32_t r = ka_fc(in, (uint8_t)IDA_FC_RESET, 0u);
    (void)ka_cagir();
    KA_SONUC(r, IDA_ACK_ACCEPTED, IDA_REASON_NONE, "F13: neden sürerken FC_RESET (W5-K1: kabul edilir)");
    KA_DOGRULA(kilitli(), "F13: neden sürerken FC_RESET sonrası fc.state %s, aynı çağrıda FAULT_LATCHED bekleniyordu (G-Ç20)",
               ka_durum_adi(ka_out()->fc.state));
    ka_kilit_nedeni_kaldir();
    ka_adimlar(3);
    in = ka_girdi();
    r = ka_fc(in, (uint8_t)IDA_FC_RESET, 0u);
    (void)ka_cagir();
    KA_SONUC(r, IDA_ACK_ACCEPTED, IDA_REASON_NONE, "F13: neden kalkınca FC_RESET");
    KA_DOGRULA(ka_out()->fc.state == IDA_FCS_SAFE, "F13: FC_RESET sonrası fc.state %s, SAFE bekleniyordu (G-Ç20)",
               ka_durum_adi(ka_out()->fc.state));
}

/* ---- G-Ç20, G-Ç46 (kilitli), G-Ç27, G-Ç03, G-Ç51 ------------------------------------------------ */

static void t_gc20_fc_reset(void)
{
    ka_kur(2);
    ka_oturum_ac();
    ka_arac_silahla();
    ka_kilitle(); /* neden sürer: PERMIT_OPEN = 1 */
    for (int k = 0; k < 3; ++k) {
        ida_in_t *in = ka_girdi();
        uint32_t r = ka_fc(in, (uint8_t)IDA_FC_RESET, 1u);
        (void)ka_cagir();
        KA_SONUC(r, IDA_ACK_ACCEPTED, IDA_REASON_NONE, "neden sürerken FC_RESET (G-Ç20; W5-K1)");
        KA_DOGRULA(kilitli(), "neden sürerken FC_RESET sonrası fc.state %s, aynı çağrıda FAULT_LATCHED (G-Ç20)",
                   ka_durum_adi(ka_out()->fc.state));
        KA_DOGRULA(kc_gunluk_say(ka_out(), (uint16_t)IDA_LOG_FC_STATE) == 0,
                   "aynı çağrıdaki FAULT_LATCHED → SAFE → FAULT_LATCHED FC_STATE günlüğü üretti (G-Ç27)");
    }
    ka_kilit_nedeni_kaldir();
    ka_adimlar(3);
    KA_DOGRULA(kilitli(), "neden kalkınca kilit kendiliğinden temizlendi (G-Ç19)");
    ida_in_t *in = ka_girdi();
    uint32_t r = ka_fc(in, (uint8_t)IDA_FC_RESET, 0u);
    (void)ka_cagir();
    KA_SONUC(r, IDA_ACK_ACCEPTED, IDA_REASON_NONE, "FC_RESET");
    const ida_out_t *o = ka_out();
    KA_DOGRULA(o->fc.state == IDA_FCS_SAFE, "kabul edilen FC_RESET sonrası fc.state %s, SAFE bekleniyordu (G-Ç20)",
               ka_durum_adi(o->fc.state));
    KA_DOGRULA((o->fc.inhibit_mask & IDA_FCI_FAULT_LATCHED) == 0u, "FC_RESET sonrası FAULT_LATCHED biti (G-Ç03)");
    KA_DOGRULA(!ka_genisletici_olay_var(o), "FC_RESET izin açtı (G-Ç20)");
    for (int k = 0; k < 100; ++k) {
        (void)ka_adim();
        KA_DOGRULA(ka_out()->fc.state == IDA_FCS_SAFE, "FC_RESET sonrası FC_ARM olmadan fc.state %s (G-Ç20: silahlandırmaz)",
                   ka_durum_adi(ka_out()->fc.state));
    }
}

static void t_gc46_kilitli(void)
{
    ka_kur(2);
    ka_oturum_ac();
    ka_kilitle();
    ka_kilit_nedeni_kaldir();
    ka_adimlar(2);
    ida_in_t *in = ka_girdi();
    (void)ka_mandal(in, (uint8_t)IDA_NARROW_FC_ABORT);
    uint32_t r = ka_fc(in, (uint8_t)IDA_FC_RESET, 0u);
    (void)ka_cagir();
    KA_SONUC(r, IDA_ACK_REJECTED, IDA_REASON_NARROWED, "kilitliyken daraltmalı çağrıda FC_RESET (G-Ç46)");
    KA_DOGRULA(kilitli(), "NARROWED FC_RESET kilidi kaldırdı (G-Ç46)");
    in = ka_girdi_seq(kc_son_seq());
    r = ka_fc(in, (uint8_t)IDA_FC_RESET, 0u);
    (void)ka_cagir();
    KA_SONUC(r, IDA_ACK_REJECTED, IDA_REASON_CALL_REJECTED, "kilitliyken reddedilen çağrıda FC_RESET (G-Ç46)");
    KA_DOGRULA(kilitli(), "CALL_REJECTED FC_RESET kilidi kaldırdı (G-Ç46)");
    ka_ayar.link_state = (uint8_t)IDA_LINK_NONE;
    (void)ka_adim();
    in = ka_girdi();
    r = ka_fc(in, (uint8_t)IDA_FC_RESET, 0u);
    (void)ka_cagir();
    KA_SONUC(r, IDA_ACK_REJECTED, IDA_REASON_SESSION, "kilitliyken oturum yokken FC_RESET (G-Ç46)");
    KA_DOGRULA(kilitli(), "SESSION reddi kilidi kaldırdı (G-Ç46)");
}

static void t_gc27_durum_gunlugu(void)
{
    ka_kur(2);
    ka_oturum_baslat();
    gevsek();
    ka_adimlar(2);
    ka_ayar.tup[0] = (uint8_t)(KC_TUP_ACIK & (uint8_t)~IDA_TUBE_PIN_OUT);
    (void)ka_adim();
    ka_ayar.permit_elle = true;
    ka_ayar.permit_deger[0] = 1u;
    bool goruldu = false;
    for (int k = 0; k < 3 && !goruldu; ++k) {
        (void)ka_adim();
        if (kilitli()) {
            goruldu = true;
            const ida_out_t *o = ka_out();
            if (o->status.cnt.log_dropped == ka_onceki()->status.cnt.log_dropped) {
                const ida_log_t *l = kc_gunluk_bul(o, (uint16_t)IDA_LOG_FC_STATE);
                KA_DOGRULA(l != NULL && l->arg0 == (int32_t)IDA_FCS_FAULT_LATCHED && l->arg1 == (int32_t)IDA_FCS_SAFE &&
                               l->severity == IDA_SEV_ERROR,
                           "SAFE → FAULT_LATCHED: FC_STATE(5, 0) ERROR günlüğü bekleniyordu (G-Ç27; 3.15 satır 20)");
            }
        }
    }
    KA_DOGRULA(goruldu, "çelişkiyle FAULT_LATCHED'e geçilmedi (G-Ç53)");
    ka_kilit_nedeni_kaldir();
    ka_adimlar(3);
    ida_in_t *in = ka_girdi();
    (void)ka_fc(in, (uint8_t)IDA_FC_RESET, 0u);
    (void)ka_cagir();
    const ida_out_t *o = ka_out();
    KA_DOGRULA(o->fc.state == IDA_FCS_SAFE, "FC_RESET sonrası fc.state %s", ka_durum_adi(o->fc.state));
    if (o->status.cnt.log_dropped == ka_onceki()->status.cnt.log_dropped) {
        const ida_log_t *l = kc_gunluk_bul(o, (uint16_t)IDA_LOG_FC_STATE);
        KA_DOGRULA(l != NULL && l->arg0 == (int32_t)IDA_FCS_SAFE && l->arg1 == (int32_t)IDA_FCS_FAULT_LATCHED &&
                       l->severity == IDA_SEV_INFO,
                   "FAULT_LATCHED → SAFE: FC_STATE(0, 5) INFO günlüğü bekleniyordu (G-Ç27)");
    }
}

static void t_gc03_fault_latched(void)
{
    ka_kur(2);
    ka_oturum_baslat();
    KA_DOGRULA((ka_out()->fc.inhibit_mask & IDA_FCI_FAULT_LATCHED) == 0u, "SAFE'te FAULT_LATCHED biti (G-Ç03)");
    ka_kilitle();
    KA_DOGRULA((ka_out()->fc.inhibit_mask & IDA_FCI_FAULT_LATCHED) != 0u, "FAULT_LATCHED'te bit yok (G-Ç03)");
    ka_kilit_nedeni_kaldir();
    ka_adimlar(2);
    ida_in_t *in = ka_girdi();
    (void)ka_fc(in, (uint8_t)IDA_FC_RESET, 0u);
    (void)ka_cagir();
    KA_DOGRULA((ka_out()->fc.inhibit_mask & IDA_FCI_FAULT_LATCHED) == 0u, "FC_RESET'in çağrısında FAULT_LATCHED biti kalkmadı (G-Ç03)");
}

static void t_gc51_kilitliyken(void)
{
    ka_kur(2);
    ka_oturum_baslat();
    ka_kilitle();
    ida_in_t *in = ka_girdi();
    uint32_t x = ka_fc(in, (uint8_t)IDA_FC_TEST_EXIT, 0u);
    (void)ka_cagir();
    KA_SONUC(x, IDA_ACK_ACCEPTED, IDA_REASON_NONE, "FAULT_LATCHED'te FC_TEST_EXIT (G-Ç51: TEST dışında ACCEPTED)");
    KA_DOGRULA(kilitli(), "FC_TEST_EXIT kilidi kaldırdı (G-Ç51: etkisiz)");
}

static const ka_test_t TESTLER[] = {
    {"kapanma_sonucu", t_gc50_kapanma_sonucu},
    {"kilitli_guvenli_donus", t_gc19_kilitli_guvenli_donus},
    {"f13_kilit_korunur", t_f13_kilit_korunur},
    {"fc_reset", t_gc20_fc_reset},
    {"reset_kilitli", t_gc46_kilitli},
    {"durum_gunlugu", t_gc27_durum_gunlugu},
    {"fault_latched", t_gc03_fault_latched},
    {"test_exit_kilitli", t_gc51_kilitliyken},
};

int main(int argc, char **argv)
{
    return ka_main(argc, argv, __FILE__, TESTLER, sizeof TESTLER / sizeof TESTLER[0]);
}
