/*
 * tests/kabul/atis — komutlar: FC komutlarının sonuç kodları ve önceliği, daraltıcı girdi,
 * reddedilen çağrı, IMU_LAG; F1 ve F10'un FC silahlanmadan sınanabilen kısmı.
 */
#include <stdio.h>
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

static uint8_t tup_eksik(uint8_t bit)
{
    return (uint8_t)(KC_TUP_ACIK & (uint8_t)~bit);
}

static bool kabul_rc_mi(ida_rc_t rc)
{
    return rc == IDA_RC_OK || rc == IDA_RC_IMU_REJECTED;
}

static ida_cmd_t *son_komut(ida_in_t *in)
{
    return ka_son_komut(in);
}

/* ---- 3.17 FC satırlarının kuralı (G-Ç05, G-Ç22, G-Ç46) -------------------------------- */

typedef struct {
    bool oturum;
    bool olay;
    bool mod;
    bool silahsiz;
    bool est;
    bool fs;
    bool estop;
    bool imu_bayat;
    bool kilit;
    bool imu_lag;      /* IMU_LAG mandalı (A4-K37; engel biti 12) */
    bool imu_red;      /* çağrı IMU_REJECTED döndü (A4-K103) */
    uint8_t durum;
    bool tup_engel[IDA_PORT_N_TUBE];
} kosul_t;

/* Engel bitlerinden koşul (A4 §3.16'nın tanımıyla). */
static void kosul_maskeden(const ida_out_t *o, kosul_t *k)
{
    uint32_t m = o->fc.inhibit_mask;
    memset(k, 0, sizeof *k);
    k->oturum = (m & IDA_FCI_SESSION) == 0u;
    k->olay = (m & IDA_FCI_EVENTS_OFF) == 0u;
    k->mod = (m & IDA_FCI_MODE) != 0u;
    k->silahsiz = (m & IDA_FCI_NOT_ARMED) != 0u;
    k->est = (m & IDA_FCI_EST) != 0u;
    k->fs = (m & IDA_FCI_FAILSAFE) != 0u;
    k->estop = (m & IDA_FCI_ESTOP) != 0u;
    k->imu_bayat = (m & IDA_FCI_IMU_STALE) != 0u;
    k->kilit = (m & IDA_FCI_FAULT_LATCHED) != 0u;
    k->imu_lag = (m & IDA_FCI_IMU_LAG) != 0u;
    k->durum = o->fc.state;
    k->tup_engel[0] = (m & (IDA_FCI_PLUG_T0 | IDA_FCI_PIN_T0 | IDA_FCI_EMPTY_T0 | IDA_FCI_RESULT_PENDING)) != 0u;
    k->tup_engel[1] = (m & (IDA_FCI_PLUG_T1 | IDA_FCI_PIN_T1 | IDA_FCI_EMPTY_T1 | IDA_FCI_RESULT_PENDING)) != 0u;
}

/* A4 §3.17'nin sırasıyla beklenen neden (NONE = ACCEPTED). Daraltmasız, yinelenmeyen kimlikli
   tek komut: (1) ve (2)'nin NARROWED'ı tutmaz. (2) genişleticide (A4-K104: FC_ARM, FC_FIRE,
   FC_RESET, FC_TEST_ENTER) IMU_REJECTED > IMU_LAG komutun kendi satırından önce gelir (4.14,
   A4-K103, A4-K108); (3) komutun kendi satırı, satırdaki sırayla. */
static uint16_t kural(uint8_t op, uint8_t tube, const kosul_t *k)
{
    if (op == IDA_FC_TEST_EXIT) {
        return IDA_REASON_NONE;
    }
    /* (2): FC_TEST_EXIT dışındaki dört işlem genişleticidir. */
    if (k->imu_red) {
        return IDA_REASON_IMU_REJECTED;
    }
    if (k->imu_lag) {
        return IDA_REASON_IMU_LAG;
    }
    /* (3) */
    if (!k->oturum) {
        return IDA_REASON_SESSION;
    }
    if (op == IDA_FC_RESET) {
        return k->durum == IDA_FCS_FAULT_LATCHED ? IDA_REASON_NONE : IDA_REASON_BAD_MODE;
    }
    if (!k->olay) {
        return IDA_REASON_EVENTS_OFF;
    }
    if (k->mod) {
        return IDA_REASON_BAD_MODE;
    }
    if (k->silahsiz || k->est || k->fs || k->estop || k->imu_bayat || k->kilit) {
        return IDA_REASON_NOT_ARMABLE;
    }
    if (op == IDA_FC_FIRE) {
        if (tube < IDA_PORT_N_TUBE && k->tup_engel[tube]) {
            return IDA_REASON_NOT_ARMABLE;
        }
        return k->durum == IDA_FCS_ARMED ? IDA_REASON_NONE : IDA_REASON_BAD_MODE;
    }
    return k->durum == IDA_FCS_SAFE ? IDA_REASON_NONE : IDA_REASON_BAD_MODE;
}

static unsigned g_kapsanan; /* gözlenen nedenlerin bit kümesi */

/* Tek FC komutu (kalp atışı yok) içeren çağrı; sonucu 3.17'nin kuralıyla karşılaştırır.
   Engel bitleri çağrının "o noktasındaki" değerdir: önceki çıktının ya da bu çıktının
   maskesinden biriyle tutarlı olması yeterlidir. Doğrudan bilinen koşullar (oturum) ayrıca
   girdiden zorlanır. */
static void fc_kurali_sina(uint8_t op, uint8_t tube, const char *ne)
{
    kosul_t once;
    kosul_maskeden(ka_out(), &once);
    bool hb = ka_ayar.heartbeat;
    ka_ayar.heartbeat = false;
    ida_in_t *in = ka_girdi();
    ka_ayar.heartbeat = hb;
    uint32_t id = ka_fc(in, op, tube);
    bool oturum_yok = in->plat.link_state != IDA_LINK_RUNNING;
    ida_rc_t rc = ka_cagir();
    kosul_t sonra;
    kosul_maskeden(ka_out(), &sonra);
    if (oturum_yok) {
        once.oturum = false;
        sonra.oturum = false;
    }
    once.imu_red = rc == IDA_RC_IMU_REJECTED;
    sonra.imu_red = once.imu_red;
    uint16_t b1 = kural(op, tube, &once);
    uint16_t b2 = kural(op, tube, &sonra);
    const ida_cmd_result_t *r = ka_sonuc(id);
    KA_DOGRULA(r != NULL, "%s: sonuç yok", ne);
    uint16_t gercek = r->ack == IDA_ACK_ACCEPTED ? (uint16_t)IDA_REASON_NONE : r->reason;
    bool ack_ok = (gercek == IDA_REASON_NONE) ? r->ack == IDA_ACK_ACCEPTED : r->ack == IDA_ACK_REJECTED;
    KA_DOGRULA(ack_ok && (gercek == b1 || gercek == b2),
               "%s: FC op %u tüp %u sonucu ack %u reason %u; 3.17'nin sırasıyla beklenen %u (önceki maske 0x%05lx) ya da %u "
               "(bu çağrının maskesi 0x%05lx), fc.state %s (G-Ç05, G-Ç22)",
               ne, (unsigned)op, (unsigned)tube, (unsigned)r->ack, (unsigned)r->reason, (unsigned)b1,
               (unsigned long)ka_onceki()->fc.inhibit_mask, (unsigned)b2, (unsigned long)ka_out()->fc.inhibit_mask,
               ka_durum_adi(ka_out()->fc.state));
    if (gercek < 16u) {
        g_kapsanan |= 1u << gercek;
    }
}

static void bes_komut(const char *ne)
{
    static const uint8_t oplar[] = {IDA_FC_ARM, IDA_FC_TEST_ENTER, IDA_FC_FIRE, IDA_FC_RESET, IDA_FC_TEST_EXIT};
    for (size_t i = 0; i < sizeof oplar; ++i) {
        fc_kurali_sina(oplar[i], 0u, ne);
        uint8_t s = ka_out()->fc.state;
        if (s != IDA_FCS_SAFE) {
            /* Bir komut kabul edilip durum değiştiyse (ör. ARM, TEST) güvenli duruma dön. Silahı
               indiren FC_DISARM'dır; FC_ABORT ARMED'de ARMED bırakır (W5-K23). */
            ida_in_t *in = ka_girdi();
            (void)ka_mandal(in, (uint8_t)IDA_NARROW_FC_DISARM);
            if (s == IDA_FCS_TEST) {
                (void)ka_fc(in, (uint8_t)IDA_FC_TEST_EXIT, 0u);
            }
            (void)ka_cagir();
            ka_adimlar(2);
        }
    }
}

static void t_gc05_satirlar(void)
{
    g_kapsanan = 0u;
    /* Olay kimliği yok (EVENTS_OFF). */
    {
        static uint8_t blok[IDA_PARAM_BLOB_MAX + 64u];
        static kc_pkayit_t kayit[KC_PARAM_AZAMI];
        size_t n = kc_varsayilan_kayitlar(kayit, KC_PARAM_AZAMI);
        size_t boy = kc_blok_kur(blok, sizeof blok, kayit, n, kc_sema_ozeti());
        ida_init_in_t ii;
        kc_init_girdi(&ii);
        ii.param_src = (uint8_t)IDA_PSRC_SIL;
        ii.param_blob = blok;
        ii.param_len = (uint32_t)boy;
        ii.boot_count_ok = false;
        (void)ka_kur_init(2, &ii);
        ka_oturum_ac();
        bes_komut("EVENTS_OFF");
    }
    ka_kur(2);
    ka_oturum_ac();
    bes_komut("silahsız, INIT/DISARMED kip");
    ka_ayar.link_state = (uint8_t)IDA_LINK_NONE;
    (void)ka_adim();
    bes_komut("oturum yok");
    ka_ayar.link_state = (uint8_t)IDA_LINK_RUNNING;
    (void)ka_adim();
    ka_arac_silahla();
    bes_komut("araç silahlı");
    /* Kip seçimi (kalkmazsa düşer: MODE kümesi boş olamaz). */
    ka_kip_sec();
    bes_komut("atış izni kipinde");
    ka_ayar.estop = 1u;
    ka_adimlar(2);
    bes_komut("acil durdurma");
    gevsek();
    ka_adimlar(2);
    ka_ayar.tick = true;
    ka_adimlar(6);
    bes_komut("IMU bayat");
    ka_ayar.tick = false;
    ka_adimlar(2);
    {
        ida_in_t *in = ka_girdi();
        (void)ka_mandal(in, (uint8_t)IDA_NARROW_DISARM);
        (void)ka_cagir();
    }
    ka_adimlar(2);
    bes_komut("araç devre dışı bırakıldı");
    KA_DOGRULA((g_kapsanan & (1u << IDA_REASON_SESSION)) != 0u && (g_kapsanan & (1u << IDA_REASON_EVENTS_OFF)) != 0u &&
                   (g_kapsanan & (1u << IDA_REASON_BAD_MODE)) != 0u && (g_kapsanan & (1u << IDA_REASON_NOT_ARMABLE)) != 0u &&
                   (g_kapsanan & (1u << IDA_REASON_NONE)) != 0u,
               "3.17 FC satırlarının nedenleri kapsanmadı (gözlenen küme 0x%04x)", g_kapsanan);
}

/* ---- G-Ç04, F10: alan denetimi ------------------------------------------------------------ */

static uint32_t fc_alan(ida_in_t *in, uint8_t op, uint8_t tube, uint32_t a0, uint32_t a1)
{
    uint32_t id = ka_fc(in, op, tube);
    son_komut(in)->u.fc.arg0 = a0;
    son_komut(in)->u.fc.arg1 = a1;
    return id;
}

static void alan_disi_tek(uint8_t op, uint8_t tube, uint32_t a0, uint32_t a1, uint16_t beklenen, const char *ne)
{
    uint8_t durum = ka_out()->fc.state;
    ida_in_t *in = ka_girdi();
    uint32_t id = fc_alan(in, op, tube, a0, a1);
    (void)ka_cagir();
    KA_SONUC(id, IDA_ACK_REJECTED, beklenen, ne);
    KA_DOGRULA(ka_out()->fc.state == durum, "%s: fc.state %s → %s değişti (G-Ç04)", ne, ka_durum_adi(durum),
               ka_durum_adi(ka_out()->fc.state));
}

static void t_gc04_alan_disi(void)
{
    static const uint8_t disarida[] = {0u, 4u, 9u, 200u, 255u};
    static const uint8_t gecerli[] = {IDA_FC_ARM, IDA_FC_FIRE, IDA_FC_RESET, IDA_FC_TEST_ENTER, IDA_FC_TEST_EXIT};
    ka_kur(2);
    ka_oturum_ac();
    for (size_t i = 0; i < sizeof disarida; ++i) {
        alan_disi_tek(disarida[i], 0u, 0u, 0u, IDA_REASON_OUT_OF_RANGE, "kapalı liste dışı op");
    }
    for (size_t i = 0; i < sizeof gecerli; ++i) {
        alan_disi_tek(gecerli[i], 2u, 0u, 0u, IDA_REASON_OUT_OF_RANGE, "tube = 2");
        alan_disi_tek(gecerli[i], 255u, 0u, 0u, IDA_REASON_OUT_OF_RANGE, "tube = 255");
        alan_disi_tek(gecerli[i], 0u, 1u, 0u, IDA_REASON_OUT_OF_RANGE, "arg0 ≠ 0");
        alan_disi_tek(gecerli[i], 1u, 0u, 0x80000000u, IDA_REASON_OUT_OF_RANGE, "arg1 ≠ 0");
    }
    /* Ortak FC satırı FC_ARM satırının oturum koşulundan önce gelir. */
    ka_ayar.link_state = (uint8_t)IDA_LINK_NONE;
    alan_disi_tek(IDA_FC_ARM, 3u, 0u, 0u, IDA_REASON_OUT_OF_RANGE, "oturum yokken tube = 3");
    ka_ayar.link_state = (uint8_t)IDA_LINK_RUNNING;
    (void)ka_adim();
    /* (1) yineleme önce: aynı kimlik ikinci kez DUPLICATE. */
    {
        ida_in_t *in = ka_girdi();
        uint32_t id = fc_alan(in, 4u, 0u, 0u, 0u);
        (void)ka_cagir();
        KA_SONUC(id, IDA_ACK_REJECTED, IDA_REASON_OUT_OF_RANGE, "op 4");
        in = ka_girdi();
        (void)kc_fc_ekle(in, id, 4u, 0u);
        (void)ka_cagir();
        KA_SONUC(id, IDA_ACK_DUPLICATE, IDA_REASON_NONE, "kendi kuralıyla reddedilmiş op 4'ün yinelenmesi (G-Ç04; A4-K48)");
    }
    /* (2) daraltmalı çağrıda genişletici (alanları bozuk) NARROWED; liste dışı op OUT_OF_RANGE. */
    {
        ida_in_t *in = ka_girdi();
        (void)ka_mandal(in, (uint8_t)IDA_NARROW_FC_ABORT);
        uint32_t a = fc_alan(in, IDA_FC_ARM, 5u, 0u, 0u);
        uint32_t f = fc_alan(in, IDA_FC_FIRE, 0u, 3u, 0u);
        uint32_t x = fc_alan(in, 4u, 0u, 0u, 0u);
        uint32_t e = fc_alan(in, IDA_FC_TEST_EXIT, 9u, 0u, 0u);
        (void)ka_cagir();
        KA_SONUC(a, IDA_ACK_REJECTED, IDA_REASON_NARROWED, "daraltmalı çağrıda FC_ARM(tube 5) (G-Ç04: (2) adımı önce)");
        KA_SONUC(f, IDA_ACK_REJECTED, IDA_REASON_NARROWED, "daraltmalı çağrıda FC_FIRE(arg0 3)");
        KA_SONUC(x, IDA_ACK_REJECTED, IDA_REASON_OUT_OF_RANGE, "daraltmalı çağrıda op 4 (genişletici sayılmaz; G-Ç04)");
        KA_SONUC(e, IDA_ACK_REJECTED, IDA_REASON_OUT_OF_RANGE, "daraltmalı çağrıda FC_TEST_EXIT(tube 9) (nötr)");
    }
    /* (3) reddedilen çağrıda CALL_REJECTED. */
    {
        ida_in_t *in = ka_girdi_seq(kc_son_seq());
        uint32_t x = fc_alan(in, 4u, 0u, 0u, 0u);
        (void)ka_cagir();
        KA_SONUC(x, IDA_ACK_REJECTED, IDA_REASON_CALL_REJECTED, "DUP_SEQ çağrısında op 4 (G-Ç04, G-Ç34)");
    }
    /* (4) IMU_LAG: genişletici IMU_LAG, liste dışı op OUT_OF_RANGE. */
    ka_ayar.imu_gecikme = 25 * KA_MS;
    for (int k = 0; k < 10 && !ka_out()->health.imu_lag; ++k) {
        (void)ka_adim();
    }
    KA_DOGRULA(ka_out()->health.imu_lag, "IMU_LAG kurulamadı");
    {
        ida_in_t *in = ka_girdi();
        uint32_t f = fc_alan(in, IDA_FC_FIRE, 9u, 0u, 0u);
        uint32_t x = fc_alan(in, 9u, 0u, 0u, 0u);
        (void)ka_cagir();
        KA_SONUC(f, IDA_ACK_REJECTED, IDA_REASON_IMU_LAG, "IMU_LAG'da FC_FIRE(tube 9) (G-Ç04: (2) adımı)");
        KA_SONUC(x, IDA_ACK_REJECTED, IDA_REASON_OUT_OF_RANGE, "IMU_LAG'da op 9");
    }
}

/* ---- G-Ç32, F10: biçimi bozuk daraltıcı ------------------------------------------------------- */

static void daraltici_bicim_tek(uint8_t op, uint8_t tube, uint32_t a0, uint32_t a1, bool once)
{
    ida_in_t *in = ka_girdi();
    uint32_t d = 0;
    if (once) {
        d = fc_alan(in, op, tube, a0, a1);
    }
    uint32_t w1 = ka_fc(in, (uint8_t)IDA_FC_ARM, 0u);
    uint32_t w2 = ka_komut(in, (uint8_t)IDA_CMD_ARM);
    if (!once) {
        d = fc_alan(in, op, tube, a0, a1);
    }
    (void)ka_cagir();
    KA_SONUC(d, IDA_ACK_ACCEPTED, IDA_REASON_NONE, "cmds[]'te biçimi bozuk FC_DISARM/FC_ABORT (G-Ç32, W5-K7)");
    KA_DOGRULA(ka_out()->health.narrowed, "biçimi bozuk FC_DISARM/FC_ABORT (op %u tube %u) uygulanmadı: health.narrowed = 0 (G-Ç32)",
               (unsigned)op, (unsigned)tube);
    KA_SONUC(w1, IDA_ACK_REJECTED, IDA_REASON_NARROWED, "aynı çağrıda FC_ARM");
    KA_SONUC(w2, IDA_ACK_REJECTED, IDA_REASON_NARROWED, "aynı çağrıda ARM");
    KA_DOGRULA(!ka_genisletici_olay_var(ka_out()), "daraltmalı çağrıda genişletici olay");
}

static void t_gc32_daraltici_bicim(void)
{
    ka_kur(2);
    ka_oturum_ac();
    for (int once = 0; once < 2; ++once) {
        daraltici_bicim_tek(IDA_FC_DISARM, 0u, 0u, 0u, once != 0);
        daraltici_bicim_tek(IDA_FC_ABORT, 7u, 5u, 0u, once != 0);
        daraltici_bicim_tek(IDA_FC_DISARM, 255u, 0u, 9u, once != 0);
        daraltici_bicim_tek(IDA_FC_ABORT, 1u, 0xFFFFFFFFu, 0xFFFFFFFFu, once != 0);
    }
    /* Yineleme penceresinden iki yönlü muafiyet (A4-K46, K48). */
    {
        ida_in_t *in = ka_girdi();
        uint32_t x = ka_fc(in, (uint8_t)IDA_FC_ABORT, 255u);
        (void)ka_cagir();
        KA_SONUC(x, IDA_ACK_ACCEPTED, IDA_REASON_NONE, "FC_ABORT");
        in = ka_girdi();
        (void)kc_komut_ekle(in, x, (uint8_t)IDA_CMD_HEARTBEAT);
        (void)ka_cagir();
        KA_DOGRULA(ka_sonuc(x) != NULL && ka_sonuc(x)->ack != IDA_ACK_DUPLICATE,
                   "daraltıcının kimliğini taşıyan sonraki komut DUPLICATE aldı: daraltıcı pencereye yazılmamalı (G-Ç32)");
        ka_adimlar(1);
        in = ka_girdi();
        uint32_t y = ka_komut(in, (uint8_t)IDA_CMD_HEARTBEAT);
        (void)ka_cagir();
        in = ka_girdi();
        (void)kc_fc_ekle(in, y, (uint8_t)IDA_FC_DISARM, 0u);
        (void)ka_cagir();
        KA_SONUC(y, IDA_ACK_ACCEPTED, IDA_REASON_NONE, "penceredeki kimlikle gelen FC_DISARM (G-Ç32: pencereyle karşılaştırılmaz)");
        KA_DOGRULA(ka_out()->health.narrowed, "penceredeki kimlikle gelen FC_DISARM uygulanmadı (G-Ç32)");
    }
    /* Reddedilen çağrıda da uygulanır ve ACCEPTED alır. */
    {
        ida_in_t *in = ka_girdi_seq(kc_son_seq());
        uint32_t x = fc_alan(in, IDA_FC_ABORT, 3u, 1u, 1u);
        uint32_t w = ka_fc(in, (uint8_t)IDA_FC_ARM, 0u);
        ida_rc_t rc = ka_cagir();
        KA_DOGRULA(rc == IDA_RC_DUP_SEQ, "DUP_SEQ bekleniyordu");
        KA_SONUC(x, IDA_ACK_ACCEPTED, IDA_REASON_NONE, "DUP_SEQ çağrısında biçimi bozuk FC_ABORT (G-Ç32)");
        KA_SONUC(w, IDA_ACK_REJECTED, IDA_REASON_CALL_REJECTED, "DUP_SEQ çağrısında FC_ARM (G-Ç34)");
        KA_DOGRULA(ka_out()->health.narrowed, "DUP_SEQ çağrısında FC_ABORT uygulanmadı (G-Ç32; A4-K113 (b))");
    }
    /* n_cmds > 8: etki uygulanır, cmds[] için sonuç yok. */
    {
        ka_ayar.sonuclari_tut = true;
        bool hb = ka_ayar.heartbeat;
        ka_ayar.heartbeat = false;
        ida_in_t *in = ka_girdi();
        ka_ayar.heartbeat = hb;
        ka_ayar.sonuclari_tut = false;
        (void)ka_fc(in, (uint8_t)IDA_FC_DISARM, 6u);
        while (in->n_cmds < IDA_PORT_MAX_CMDS) {
            (void)ka_komut(in, (uint8_t)IDA_CMD_HEARTBEAT);
        }
        in->n_cmds = (uint8_t)(IDA_PORT_MAX_CMDS + 1u);
        ida_rc_t rc = ka_cagir();
        KA_DOGRULA(rc == IDA_RC_BAD_INPUT, "n_cmds = 9 iken %d", (int)rc);
        KA_DOGRULA(ka_out()->n_results == 0u, "n_cmds > 8 iken cmds[] için %u sonuç (G-Ç32, G-Ç34)", (unsigned)ka_out()->n_results);
        KA_DOGRULA(ka_out()->health.narrowed, "n_cmds > 8 çağrısında ilk 8 girdideki FC_DISARM uygulanmadı (G-Ç32)");
    }
}

/* ---- G-Ç05: yineleme ve öncelik ---------------------------------------------------------------- */

static void t_gc05_yineleme(void)
{
    ka_kur(2);
    ka_oturum_ac();
    ida_in_t *in = ka_girdi();
    uint32_t a = ka_fc(in, (uint8_t)IDA_FC_ARM, 0u);
    uint32_t f = ka_fc(in, (uint8_t)IDA_FC_FIRE, 1u);
    (void)ka_cagir();
    /* Aynı kimlikler daraltmalı ve IMU'su reddedilen çağrıda: DUPLICATE her şeyden önce. */
    ka_ayar.imu_bozuk = true;
    in = ka_girdi();
    (void)ka_mandal(in, (uint8_t)IDA_NARROW_FC_DISARM);
    (void)kc_fc_ekle(in, a, (uint8_t)IDA_FC_ARM, 0u);
    (void)kc_fc_ekle(in, f, (uint8_t)IDA_FC_FIRE, 1u);
    uint32_t yeni = ka_fc(in, (uint8_t)IDA_FC_FIRE, 1u);
    uint32_t wb0 = ka_out()->status.cnt.widen_blocked;
    (void)ka_cagir();
    ka_ayar.imu_bozuk = false;
    KA_SONUC(a, IDA_ACK_DUPLICATE, IDA_REASON_NONE, "yinelenen FC_ARM daraltmalı çağrıda (G-Ç05 (1), G-Ç33)");
    KA_SONUC(f, IDA_ACK_DUPLICATE, IDA_REASON_NONE, "yinelenen FC_FIRE daraltmalı çağrıda (G-Ç05 (1))");
    KA_SONUC(yeni, IDA_ACK_REJECTED, IDA_REASON_NARROWED, "yeni FC_FIRE daraltmalı, IMU'su reddedilen çağrıda (G-Ç05 (2))");
    KA_DOGRULA(ka_out()->status.cnt.widen_blocked - wb0 == 1u,
               "widen_blocked farkı %lu, yalnız yeni genişletici sayılmalı (G-Ç33)",
               (unsigned long)(ka_out()->status.cnt.widen_blocked - wb0));
}

static void t_gc05_oncelik(void)
{
    static const uint8_t gen[] = {IDA_FC_ARM, IDA_FC_FIRE, IDA_FC_RESET, IDA_FC_TEST_ENTER};
    ka_kur(2);
    ka_oturum_ac();
    /* IMU_LAG mandalını kur. */
    ka_ayar.imu_gecikme = 25 * KA_MS;
    for (int k = 0; k < 10 && !ka_out()->health.imu_lag; ++k) {
        (void)ka_adim();
    }
    KA_DOGRULA(ka_out()->health.imu_lag, "IMU_LAG kurulamadı");
    for (int d = 0; d < 3; ++d) {
        /* d = 0: daraltma + IMU red + lag → NARROWED; 1: IMU red + lag → IMU_REJECTED; 2: lag → IMU_LAG */
        ka_ayar.imu_bozuk = d <= 1;
        ida_in_t *in = ka_girdi();
        if (d == 0) {
            (void)ka_mandal(in, (uint8_t)IDA_NARROW_MODE);
        }
        uint32_t id[4];
        for (size_t i = 0; i < 4u; ++i) {
            id[i] = ka_fc(in, gen[i], 0u);
        }
        uint32_t cikis = ka_fc(in, (uint8_t)IDA_FC_TEST_EXIT, 0u);
        (void)ka_cagir();
        ka_ayar.imu_bozuk = false;
        uint16_t b = d == 0 ? IDA_REASON_NARROWED : (d == 1 ? IDA_REASON_IMU_REJECTED : IDA_REASON_IMU_LAG);
        for (size_t i = 0; i < 4u; ++i) {
            KA_SONUC(id[i], IDA_ACK_REJECTED, b, "genişletici FC komutu: NARROWED > IMU_REJECTED > IMU_LAG (G-Ç05 (2))");
        }
        KA_SONUC(cikis, IDA_ACK_ACCEPTED, IDA_REASON_NONE, "nötr FC_TEST_EXIT (2) adımına girmez (G-Ç05, G-Ç51)");
    }
}

/* ---- G-Ç06, G-Ç33: daraltıcı girdi ------------------------------------------------------------- */

enum { DT_DISARM, DT_MOD, DT_FCDISARM, DT_FCABORT, DT_CMDS_ABORT, DT_OTURUM, DT_ESTOP, DT_FIS0, DT_PIM1, DT_SAYI };
static const char *const DT_AD[DT_SAYI] = {"DISARM mandalı", "kip mandalı",  "FC_DISARM mandalı",
                                           "FC_ABORT mandalı", "cmds[] FC_ABORT", "SESSION_END",
                                           "acil durdurma",  "tüp 0 fişi",   "tüp 1 pimi"};

static void daraltma_kur(ida_in_t *in, int t)
{
    switch (t) {
    case DT_DISARM: (void)ka_mandal(in, (uint8_t)IDA_NARROW_DISARM); break;
    case DT_MOD: (void)ka_mandal(in, (uint8_t)IDA_NARROW_MODE); break;
    case DT_FCDISARM: (void)ka_mandal(in, (uint8_t)IDA_NARROW_FC_DISARM); break;
    case DT_FCABORT: (void)ka_mandal(in, (uint8_t)IDA_NARROW_FC_ABORT); break;
    case DT_CMDS_ABORT: (void)ka_fc(in, (uint8_t)IDA_FC_ABORT, 255u); break;
    case DT_OTURUM:
        in->plat.link_event = (uint8_t)IDA_LINKEV_SESSION_END;
        in->plat.link_state = (uint8_t)IDA_LINK_NONE;
        break;
    default: break;
    }
}

static void emniyet_daralt(int t)
{
    if (t == DT_ESTOP) {
        ka_ayar.estop = 1u;
    } else if (t == DT_FIS0) {
        ka_ayar.tup[0] = tup_eksik(IDA_TUBE_PLUG_IN);
    } else if (t == DT_PIM1) {
        ka_ayar.tup[1] = tup_eksik(IDA_TUBE_PIN_OUT);
    }
}

enum { DR_OK, DR_IMU, DR_BAD, DR_DUP, DR_OLD, DR_NUL, DR_SAYI };
static const char *const DR_AD[DR_SAYI] = {"OK", "IMU_REJECTED", "BAD_INPUT", "DUP_SEQ", "OLD_SEQ", "out NULL"};

static void daraltici_her_donus(int t0, int t1)
{
    for (int t = t0; t < t1; ++t) {
        for (int d = 0; d < DR_SAYI; ++d) {
            ka_kur(2);
            ka_oturum_ac();
            ka_arac_silahla();
            gevsek();
            ka_adimlar(3);
            emniyet_daralt(t);
            ka_ayar.imu_bozuk = d == DR_IMU;
            ida_in_t *in;
            if (d == DR_DUP) {
                in = ka_girdi_seq(kc_son_seq());
            } else if (d == DR_OLD) {
                in = ka_girdi_seq(kc_son_seq() - 2u);
            } else {
                ka_ayar.sonuclari_tut = d == DR_BAD || d == DR_NUL;
                in = ka_girdi();
                ka_ayar.sonuclari_tut = false;
            }
            daraltma_kur(in, t);
            if (d == DR_BAD) {
                in->narrow.mask = (uint8_t)(in->narrow.mask | 0x20u);
            }
            uint32_t w = ka_fc(in, (uint8_t)IDA_FC_ARM, 1u);
            (void)w;
            ida_rc_t rc = d == DR_NUL ? ka_cagir_out_null() : ka_cagir();
            ka_ayar.imu_bozuk = false;
            if (d == DR_NUL) {
                if (t == DT_OTURUM) {
                    ka_ayar.link_state = (uint8_t)IDA_LINK_NONE;
                }
                (void)ka_adim();
                const ida_out_t *o = ka_out();
                if (t == DT_DISARM || t == DT_OTURUM) {
                    KA_DOGRULA(!o->armed, "%s / %s: daraltma geri alındı ya da uygulanmadı: armed = 1 (G-Ç06)", DT_AD[t], DR_AD[d]);
                }
                if (t >= DT_OTURUM) {
                    KA_DOGRULA(ka_out()->n_events >= 1u, "%s / out NULL: bekletilen SAFETY_CLOSE sonraki çağrıda yok (G-Ç06; 3.11 (b2))",
                               DT_AD[t]);
                }
                continue;
            }
            const ida_out_t *o = ka_out();
            KA_DOGRULA(o->health.narrowed, "%s / %s (dönüş %d): health.narrowed = 0 (G-Ç06)", DT_AD[t], DR_AD[d], (int)rc);
            KA_DOGRULA(!ka_genisletici_olay_var(o), "%s / %s: genişletici olay (G-Ç06)", DT_AD[t], DR_AD[d]);
            if (t == DT_DISARM || t == DT_OTURUM) {
                KA_DOGRULA(!o->armed, "%s / %s: armed = 1 (G-Ç06: daraltma dönüş kodundan bağımsız)", DT_AD[t], DR_AD[d]);
            }
            const ida_cmd_result_t *r = ka_sonuc(w);
            KA_DOGRULA(r != NULL && r->ack == IDA_ACK_REJECTED &&
                           r->reason == (kabul_rc_mi(rc) ? IDA_REASON_NARROWED : IDA_REASON_CALL_REJECTED),
                       "%s / %s: FC_ARM sonucu %u/%u (G-Ç33, G-Ç34)", DT_AD[t], DR_AD[d], r != NULL ? (unsigned)r->ack : 0u,
                       r != NULL ? (unsigned)r->reason : 0u);
            /* Geri alınmaz: sonraki olağan çağrıda da kısıtlayıcı etki sürer. */
            ka_ayar.link_state = (uint8_t)(t == DT_OTURUM ? IDA_LINK_NONE : IDA_LINK_RUNNING);
            (void)ka_adim();
            if (t == DT_DISARM || t == DT_OTURUM) {
                KA_DOGRULA(!ka_out()->armed, "%s / %s: sonraki çağrıda armed = 1 (G-Ç06: geri alınmaz)", DT_AD[t], DR_AD[d]);
            }
            if (t == DT_ESTOP) {
                KA_DOGRULA((ka_out()->fc.inhibit_mask & IDA_FCI_ESTOP) != 0u, "acil durdurma / %s: ESTOP biti sonraki çağrıda yok", DR_AD[d]);
            }
        }
    }
}

static void t_gc06_komut_ve_oturum(void)
{
    daraltici_her_donus(DT_DISARM, DT_ESTOP);
}

static void t_gc06_emniyet_girdisi(void)
{
    daraltici_her_donus(DT_ESTOP, DT_SAYI);
}

static void genisleticiler_ekle(ida_in_t *in, uint32_t *id, bool ters)
{
    static const uint8_t fcop[4] = {IDA_FC_ARM, IDA_FC_FIRE, IDA_FC_RESET, IDA_FC_TEST_ENTER};
    uint32_t tmp[7];
    for (int i = 0; i < 7; ++i) {
        int k = ters ? 6 - i : i;
        if (k < 4) {
            tmp[k] = ka_fc(in, fcop[k], (uint8_t)(k & 1));
        } else if (k == 4) {
            tmp[k] = ka_komut(in, (uint8_t)IDA_CMD_ARM);
        } else if (k == 5) {
            tmp[k] = ka_yeni_id();
            kc_komut_ekle(in, tmp[k], (uint8_t)IDA_CMD_SET_MODE)->u.set_mode.mode = (uint8_t)IDA_MODE_GUIDED;
        } else {
            tmp[k] = ka_komut(in, (uint8_t)IDA_CMD_FAILSAFE_RESET);
        }
    }
    memcpy(id, tmp, sizeof tmp);
}

static void genisletici_narrowed(int t0, int t1)
{
    for (int t = t0; t < t1; ++t) {
        for (int ters = 0; ters < 2; ++ters) {
            ka_kur(2);
            ka_oturum_ac();
            gevsek();
            ka_adimlar(3);
            emniyet_daralt(t);
            bool hb = ka_ayar.heartbeat;
            ka_ayar.heartbeat = false;
            ida_in_t *in = ka_girdi();
            ka_ayar.heartbeat = hb;
            uint32_t id[7];
            if (t == DT_CMDS_ABORT && ters == 0) {
                daraltma_kur(in, t);
            }
            if (t != DT_CMDS_ABORT) {
                daraltma_kur(in, t);
            }
            genisleticiler_ekle(in, id, ters != 0);
            if (t == DT_CMDS_ABORT && ters != 0) {
                daraltma_kur(in, t);
            }
            uint32_t wb0 = ka_out()->status.cnt.widen_blocked;
            ida_rc_t rc = ka_cagir();
            KA_DOGRULA(rc == IDA_RC_OK, "%s: dönüş %d", DT_AD[t], (int)rc);
            for (int i = 0; i < 7; ++i) {
                const ida_cmd_result_t *r = ka_sonuc(id[i]);
                KA_DOGRULA(r != NULL && r->ack == IDA_ACK_REJECTED && r->reason == IDA_REASON_NARROWED,
                           "%s (%s sıra): %d. genişletici %u/%u, REJECTED/NARROWED bekleniyordu (G-Ç33)", DT_AD[t],
                           ters ? "ters" : "düz", i, r != NULL ? (unsigned)r->ack : 0u, r != NULL ? (unsigned)r->reason : 0u);
            }
            KA_DOGRULA(ka_out()->status.cnt.widen_blocked - wb0 == 7u, "%s: widen_blocked farkı %lu, 7 bekleniyordu (G-Ç33)",
                       DT_AD[t], (unsigned long)(ka_out()->status.cnt.widen_blocked - wb0));
            KA_DOGRULA(ka_out()->health.narrowed, "%s: health.narrowed = 0 (G-Ç06)", DT_AD[t]);
        }
    }
}

static void t_gc33_komut_ve_oturum(void)
{
    genisletici_narrowed(DT_DISARM, DT_ESTOP);
}

static void t_gc33_emniyet_girdisi(void)
{
    genisletici_narrowed(DT_ESTOP, DT_SAYI);
}


/* W5-K22: çekirdeğin bu çağrıda KENDİ uyguladığı daraltmanın çıktıdaki izi (kip komutu
   içermeyen çağrı için): kip düşmesi (status.mode değişti ya da MODE_CHANGE kaydı), arıza
   eylemi düzeyinin artması (fs_level ya da FAILSAFE_CHANGE kaydı), bütünlük bozulması kaydı. */
static bool kendi_daraltma(uint8_t kip0, uint8_t fs0, const ida_out_t *o)
{
    if (o->status.mode != kip0 || o->fs_level > fs0) {
        return true;
    }
    unsigned n = o->n_logs <= IDA_PORT_MAX_LOGS ? o->n_logs : IDA_PORT_MAX_LOGS;
    for (unsigned i = 0; i < n; ++i) {
        const ida_log_t *g = &o->logs[i];
        if (g->code == IDA_LOG_MODE_CHANGE || g->code == IDA_LOG_CORE_INTEGRITY ||
            (g->code == IDA_LOG_FAILSAFE_CHANGE && g->arg0 > (int32_t)fs0)) {
            return true;
        }
    }
    return false;
}

/* W5-K22: çekirdeğin kendi daraltmasını uyguladığı çağrıda her genişletici REJECTED/NARROWED
   alır (çağrı IMU_REJECTED ise de: A4 §3.17 (2) sırasıyla NARROWED > IMU_REJECTED), hiçbiri
   kabul edilmez; health.narrowed (b) girdisi olmadığı için 0'dır (A4-K45, K113); çıktıda
   genişletici olay yoktur. */
static void kendi_daraltma_red(const uint32_t *id, const char *const *ad, int n, const char *ne)
{
    const ida_out_t *o = ka_out();
    for (int i = 0; i < n; ++i) {
        const ida_cmd_result_t *r = ka_sonuc(id[i]);
        KA_DOGRULA(r != NULL && r->ack != IDA_ACK_ACCEPTED,
                   "%s: genişletici %s KABUL EDİLDİ (%u/%u; gevşetme; W5-K22)", ne, ad[i],
                   r != NULL ? (unsigned)r->ack : 0u, r != NULL ? (unsigned)r->reason : 0u);
        KA_DOGRULA(r->ack == IDA_ACK_REJECTED && r->reason == IDA_REASON_NARROWED,
                   "%s: genişletici %s %u/%u, REJECTED/NARROWED bekleniyordu (W5-K22; A4 §3.17 (2): NARROWED > "
                   "IMU_REJECTED)",
                   ne, ad[i], (unsigned)r->ack, (unsigned)r->reason);
    }
    KA_DOGRULA(!o->health.narrowed, "%s: health.narrowed = 1; (b) girdisi yok (W5-K22; A4-K45, K113)", ne);
    KA_DOGRULA(!ka_genisletici_olay_var(o), "%s: çekirdeğin kendi daraltmasının çağrısında genişletici olay (W5-K22)", ne);
}

static void t_gc33_daraltmasiz_cagri(void)
{
    /* (b) girdisi olmayan çağrı (G-Ç33, G-Ç05 (2); A4-K45, W5-K22): önceki çağrı reddedilmiş ve
       kip mandalı taşımış olsa da health.narrowed = 0'dır. Çekirdek o çağrıda kendi kip
       düşmesini (ör. reddedilen çağrıdaki STATION_KEEP hedefinden konum yokken DEPTH_HOLD'a)
       uyguluyorsa genişleticiler REJECTED/NARROWED alır ve kabul edilmez; uygulamıyorsa
       NARROWED almaz. */
    static const uint8_t hedefler[] = {IDA_MODE_STABILIZE, IDA_MODE_STATION_KEEP, IDA_MODE_MANUAL, IDA_MODE_DEPTH_HOLD};
    static const char *const AD[] = {"ARM", "FAILSAFE_RESET", "FC_ARM"};
    char ne[200];
    for (size_t h = 0; h < sizeof hedefler; ++h) {
        for (int red = 0; red < 2; ++red) {
            ka_kur(2);
            ka_oturum_ac();
            ida_in_t *in = ka_girdi();
            (void)ka_mandal(in, (uint8_t)IDA_NARROW_MODE);
            in->narrow.mode = (uint8_t)IDA_MODE_DEPTH_HOLD;
            (void)ka_cagir();
            ka_adimlar(3);
            in = red != 0 ? ka_girdi_seq(kc_son_seq()) : ka_girdi();
            (void)ka_mandal(in, (uint8_t)IDA_NARROW_MODE);
            in->narrow.mode = hedefler[h];
            (void)ka_komut(in, (uint8_t)IDA_CMD_ARM);
            (void)ka_cagir();
            for (int k = 0; k < 3; ++k) {
                const ida_out_t *o = ka_out();
                uint8_t kip0 = o->status.mode;
                uint8_t fs0 = o->fs_level;
                bool armed0 = o->status.armed;
                uint8_t fcs0 = o->fc.state;
                in = ka_girdi();
                uint32_t id[3];
                id[0] = ka_komut(in, (uint8_t)IDA_CMD_ARM);
                id[1] = ka_komut(in, (uint8_t)IDA_CMD_FAILSAFE_RESET);
                id[2] = ka_fc(in, (uint8_t)IDA_FC_ARM, 0u);
                ida_rc_t rc = ka_cagir();
                o = ka_out();
                bool kendi = kendi_daraltma(kip0, fs0, o);
                (void)snprintf(ne, sizeof ne,
                               "%s kip mandalı (hedef %u) sonrasındaki (b) girdisiz %d. çağrı (dönüş %d, kip %u → %u, "
                               "fs %u → %u, kendi daraltma %d)",
                               red != 0 ? "DUP_SEQ çağrısındaki" : "kabul edilen çağrıdaki", (unsigned)hedefler[h], k + 1,
                               (int)rc, (unsigned)kip0, (unsigned)o->status.mode, (unsigned)fs0, (unsigned)o->fs_level,
                               (int)kendi);
                KA_DOGRULA(kabul_rc_mi(rc), "%s: kabul edilen çağrı bekleniyordu", ne);
                KA_DOGRULA(!o->health.narrowed, "%s: health.narrowed = 1; (b) girdisi yok (W5-K22; A4-K45, K113)", ne);
                if (kendi) {
                    kendi_daraltma_red(id, AD, 3, ne);
                    /* Genişleticinin etkisi oluşmaz. */
                    KA_DOGRULA(armed0 || !o->status.armed, "%s: ARM'ın etkisi oluştu (armed 0 → 1; W5-K22)", ne);
                    KA_DOGRULA(fcs0 == IDA_FCS_ARMED || o->fc.state != IDA_FCS_ARMED,
                               "%s: FC_ARM'ın etkisi oluştu (fc.state %s → ARMED; W5-K22)", ne, ka_durum_adi(fcs0));
                } else {
                    for (int i = 0; i < 3; ++i) {
                        const ida_cmd_result_t *r = ka_sonuc(id[i]);
                        KA_DOGRULA(r != NULL && r->reason != IDA_REASON_NARROWED,
                                   "%s: %s %u/%u; NARROWED yalnız (b) girdili ya da çekirdeğin kendi daraltmasını "
                                   "uyguladığı çağrıda (A4-K45; G-Ç33; W5-K22)",
                                   ne, AD[i], r != NULL ? (unsigned)r->ack : 0u, r != NULL ? (unsigned)r->reason : 0u);
                    }
                }
                in = ka_girdi();
                (void)ka_mandal(in, (uint8_t)IDA_NARROW_DISARM);
                (void)ka_cagir();
            }
        }
    }
}

/* ---- G-Ç34 --------------------------------------------------------------------------------------- */

static void t_gc34_reddedilen_cagri(void)
{
    static const uint8_t oplar[] = {IDA_FC_ARM, IDA_FC_FIRE, IDA_FC_RESET, IDA_FC_TEST_ENTER, IDA_FC_TEST_EXIT, 4u};
    ka_kur(2);
    ka_oturum_ac();
    for (int d = 0; d < 3; ++d) {
        uint32_t id[6];
        ida_in_t *in;
        if (d == 0) {
            in = ka_girdi_seq(kc_son_seq());
        } else if (d == 1) {
            in = ka_girdi_seq(kc_son_seq() - 3u);
        } else {
            ka_ayar.sonuclari_tut = true;
            in = ka_girdi();
            ka_ayar.sonuclari_tut = false;
            in->narrow.mask = 0x40u; /* tanımsız mandal biti */
        }
        for (size_t i = 0; i < sizeof oplar; ++i) {
            id[i] = ka_fc(in, oplar[i], 0u);
        }
        ida_rc_t rc = ka_cagir();
        KA_DOGRULA(!kabul_rc_mi(rc), "reddedilen çağrı bekleniyordu, dönüş %d", (int)rc);
        for (size_t i = 0; i < sizeof oplar; ++i) {
            KA_SONUC(id[i], IDA_ACK_REJECTED, IDA_REASON_CALL_REJECTED, "reddedilen çağrıda FC komutu (G-Ç34)");
        }
        /* Pencereye girmedi: aynı kimlikler sonraki kabul edilen çağrıda DUPLICATE almaz. */
        in = ka_girdi();
        for (size_t i = 0; i < sizeof oplar; ++i) {
            (void)kc_fc_ekle(in, id[i], oplar[i], 0u);
        }
        (void)ka_cagir();
        for (size_t i = 0; i < sizeof oplar; ++i) {
            const ida_cmd_result_t *r = ka_sonuc(id[i]);
            KA_DOGRULA(r != NULL && r->ack != IDA_ACK_DUPLICATE && r->reason != IDA_REASON_CALL_REJECTED,
                       "reddedilen çağrıdaki komutun kimliği pencereye girmiş (DUPLICATE) (G-Ç34; A4-K114)");
        }
    }
}

/* ---- G-Ç35 ----------------------------------------------------------------------------------------- */

static void t_gc35_imu_lag(void)
{
    ka_kur(2);
    ka_oturum_ac();
    ka_arac_silahla();
    ka_ayar.imu_gecikme = 25 * KA_MS;
    for (int k = 0; k < 10 && !ka_out()->health.imu_lag; ++k) {
        (void)ka_adim();
    }
    KA_DOGRULA(ka_out()->health.imu_lag, "IMU_LAG kurulamadı");
    for (int k = 0; k < 20; ++k) {
        ida_in_t *in = ka_girdi();
        uint32_t a = ka_fc(in, (uint8_t)IDA_FC_ARM, (uint8_t)(k & 1));
        uint32_t f = ka_fc(in, (uint8_t)IDA_FC_FIRE, (uint8_t)(k & 1));
        uint32_t r = ka_fc(in, (uint8_t)IDA_FC_RESET, 0u);
        uint32_t t = ka_fc(in, (uint8_t)IDA_FC_TEST_ENTER, 0u);
        (void)ka_cagir();
        if (!ka_out()->health.imu_lag) {
            break;
        }
        KA_SONUC(a, IDA_ACK_REJECTED, IDA_REASON_IMU_LAG, "IMU_LAG'da FC_ARM (G-Ç35)");
        KA_SONUC(f, IDA_ACK_REJECTED, IDA_REASON_IMU_LAG, "IMU_LAG'da FC_FIRE (G-Ç35)");
        KA_SONUC(r, IDA_ACK_REJECTED, IDA_REASON_IMU_LAG, "IMU_LAG'da FC_RESET (G-Ç35)");
        KA_SONUC(t, IDA_ACK_REJECTED, IDA_REASON_IMU_LAG, "IMU_LAG'da FC_TEST_ENTER (G-Ç35)");
        KA_DOGRULA(!ka_genisletici_olay_var(ka_out()), "IMU_LAG'da genişletici olay (G-Ç35)");
    }
}

/* ---- G-Ç46 (kilitsiz), G-Ç51 (TEST dışı) -------------------------------------------------------- */

static void t_gc46_kilitsiz(void)
{
    ka_kur(2);
    ka_oturum_ac();
    ka_ayar.link_state = (uint8_t)IDA_LINK_NONE;
    (void)ka_adim();
    ida_in_t *in = ka_girdi();
    uint32_t r = ka_fc(in, (uint8_t)IDA_FC_RESET, 0u);
    (void)ka_cagir();
    KA_SONUC(r, IDA_ACK_REJECTED, IDA_REASON_SESSION, "oturum yokken FC_RESET (G-Ç46)");
    KA_DOGRULA(ka_out()->fc.state == IDA_FCS_SAFE, "FC_RESET reddi durumu değiştirdi (G-Ç46)");
    ka_ayar.link_state = (uint8_t)IDA_LINK_RUNNING;
    (void)ka_adim();
    in = ka_girdi();
    r = ka_fc(in, (uint8_t)IDA_FC_RESET, 1u);
    (void)ka_cagir();
    KA_SONUC(r, IDA_ACK_REJECTED, IDA_REASON_BAD_MODE, "FAULT_LATCHED değilken FC_RESET (G-Ç46)");
    KA_DOGRULA(ka_out()->fc.state == IDA_FCS_SAFE, "FC_RESET reddi durumu değiştirdi (G-Ç46)");
    in = ka_girdi();
    (void)ka_mandal(in, (uint8_t)IDA_NARROW_FC_DISARM);
    r = ka_fc(in, (uint8_t)IDA_FC_RESET, 0u);
    (void)ka_cagir();
    KA_SONUC(r, IDA_ACK_REJECTED, IDA_REASON_NARROWED, "daraltmalı çağrıda FC_RESET (G-Ç46)");
    in = ka_girdi_seq(kc_son_seq());
    r = ka_fc(in, (uint8_t)IDA_FC_RESET, 0u);
    (void)ka_cagir();
    KA_SONUC(r, IDA_ACK_REJECTED, IDA_REASON_CALL_REJECTED, "reddedilen çağrıda FC_RESET (G-Ç46)");
    KA_DOGRULA(ka_out()->fc.state == IDA_FCS_SAFE, "FC_RESET reddi durumu değiştirdi (G-Ç46)");
}

static void t_gc51_test_disi(void)
{
    ka_kur(2);
    ka_oturum_ac();
    for (int k = 0; k < 3; ++k) {
        uint32_t maske = ka_out()->fc.inhibit_mask;
        ida_in_t *in = ka_girdi();
        uint32_t x = ka_fc(in, (uint8_t)IDA_FC_TEST_EXIT, (uint8_t)(k & 1));
        (void)ka_cagir();
        KA_SONUC(x, IDA_ACK_ACCEPTED, IDA_REASON_NONE, "TEST dışında FC_TEST_EXIT (G-Ç51)");
        KA_DOGRULA(ka_out()->fc.state == IDA_FCS_SAFE && ka_out()->fc.inhibit_mask == maske,
                   "TEST dışında FC_TEST_EXIT etkili oldu (G-Ç51: etkisiz)");
    }
}

/* ---- G-Ç16, F1: geçiş çağrısı ve tüp kapsamı (gerekçe düzeyi) ----------------------------------- */

static void t_gc16_gecis_cagrisi(void)
{
    for (int tur = 0; tur < 2; ++tur) {
        ka_kur(2);
        ka_oturum_ac();
        gevsek();
        ka_adimlar(3);
        ka_ayar.tup[0] = tup_eksik(tur == 0 ? IDA_TUBE_PLUG_IN : IDA_TUBE_PIN_OUT);
        ida_in_t *in = ka_girdi();
        uint32_t f = ka_fc(in, (uint8_t)IDA_FC_FIRE, 1u);
        uint32_t a = ka_fc(in, (uint8_t)IDA_FC_ARM, 1u);
        (void)ka_cagir();
        KA_SONUC(f, IDA_ACK_REJECTED, IDA_REASON_NARROWED, "tüp 0 kısıtlayana döndüğü çağrıda tüp 1'e FC_FIRE (G-Ç16, W5-K10)");
        KA_SONUC(a, IDA_ACK_REJECTED, IDA_REASON_NARROWED, "tüp 0 kısıtlayana döndüğü çağrıda tüp 1'e FC_ARM (G-Ç16)");
        for (int k = 0; k < 3; ++k) {
            in = ka_girdi();
            f = ka_fc(in, (uint8_t)IDA_FC_FIRE, 1u);
            a = ka_fc(in, (uint8_t)IDA_FC_ARM, 1u);
            (void)ka_cagir();
            const ida_cmd_result_t *rf = ka_sonuc(f);
            const ida_cmd_result_t *ra = ka_sonuc(a);
            KA_DOGRULA(rf != NULL && rf->reason != IDA_REASON_NARROWED && ra != NULL && ra->reason != IDA_REASON_NARROWED,
                       "süregelen tüp 0 kısıtlaması tüp 1'in komutlarını NARROWED ile engelledi (G-Ç16: sonraki çağrıda tüpe özgü)");
            KA_DOGRULA(!ka_out()->health.narrowed, "süregelen kısıtlama daraltıcı girdi sayıldı (A4-K44)");
        }
    }
}

static void t_f1_daraltma(void)
{
    /* §7 F1 (FC silahlanmadan gözlenebilen kısmı): her enjeksiyonda FC_FIRE'ın sonucu ve
       genişletici olay yokluğu. Silahlı durumdaki karşılığı izin dosyasındadır. */
    for (int d = 0; d < 7; ++d) {
        ka_kur(2);
        ka_oturum_ac();
        ka_arac_silahla();
        gevsek();
        ka_adimlar(3);
        uint32_t f = 0;
        uint16_t b = IDA_REASON_NARROWED;
        ida_in_t *in;
        if (d == 5) {
            in = ka_girdi_seq(kc_son_seq());
            b = IDA_REASON_CALL_REJECTED;
        } else {
            if (d == 3) {
                ka_ayar.estop = 1u;
            }
            in = ka_girdi();
        }
        switch (d) {
        case 0: (void)ka_mandal(in, (uint8_t)IDA_NARROW_DISARM); break;
        case 1: (void)ka_mandal(in, (uint8_t)IDA_NARROW_MODE); break;
        case 2: (void)ka_mandal(in, (uint8_t)IDA_NARROW_FC_ABORT); break;
        case 5: (void)ka_mandal(in, (uint8_t)IDA_NARROW_DISARM); break;
        default: break;
        }
        if (d == 4) {
            /* Yinelenen kimlik: önce bir FC_FIRE, aynı kimlikle daraltmalı çağrıda. */
            uint32_t id = ka_fc(in, (uint8_t)IDA_FC_FIRE, 0u);
            (void)ka_cagir();
            in = ka_girdi();
            (void)ka_mandal(in, (uint8_t)IDA_NARROW_DISARM);
            (void)kc_fc_ekle(in, id, (uint8_t)IDA_FC_FIRE, 0u);
            (void)ka_cagir();
            KA_SONUC(id, IDA_ACK_DUPLICATE, IDA_REASON_NONE, "F1 (i): yinelenen FC_FIRE");
            continue;
        }
        if (d == 6) {
            ka_ayar.tup[0] = tup_eksik(IDA_TUBE_PLUG_IN);
            in = ka_girdi();
            f = ka_fc(in, (uint8_t)IDA_FC_FIRE, 1u);
            (void)ka_cagir();
            KA_SONUC(f, IDA_ACK_REJECTED, IDA_REASON_NARROWED, "F1 (iii): tüp 0 fişi çıkarken tüp 1'e FC_FIRE");
            in = ka_girdi();
            f = ka_fc(in, (uint8_t)IDA_FC_FIRE, 1u);
            (void)ka_cagir();
            KA_DOGRULA(ka_sonuc(f) != NULL && ka_sonuc(f)->reason != IDA_REASON_NARROWED,
                       "F1 (iii): sonraki çağrıda tüp 1'e FC_FIRE tüp 0'ın fişinden etkilendi");
            continue;
        }
        f = ka_fc(in, (uint8_t)IDA_FC_FIRE, 0u);
        (void)ka_cagir();
        KA_SONUC(f, IDA_ACK_REJECTED, b, "F1: daraltmayla aynı çağrıdaki FC_FIRE");
        KA_DOGRULA(!ka_genisletici_olay_var(ka_out()), "F1: genişletici olay");
    }
}


static void t_f1_ariza_kenari(void)
{
    /* §7 F1'in "arıza eylemi kenarı + FC_FIRE" enjeksiyonu (W5-K22; W5-K17 geri alındı).
       Arıza eylemi çekirdeğin o çağrıda KENDİ uyguladığı daraltmadır: aynı çağrıdaki
       genişleticiler REJECTED/NARROWED alır (çağrı IMU_REJECTED ise de: A4 §3.17 (2) sırasıyla
       NARROWED > IMU_REJECTED), hiçbiri kabul edilmez, etkisi oluşmaz, genişletici olay yoktur;
       arıza eylemi kenarı A4-K113 (b) girdisi olmadığından health.narrowed = 0'dır. Arıza
       eylemi kenarı: DEPTH_HOLD'da IMU'nun kesilmesi (IMU bayatlığı → arıza eylemi). */
    static const char *const AD[] = {"ARM", "FAILSAFE_RESET", "FC_ARM(1)", "FC_FIRE(0)"};
    char ne[160];
    ka_kur(2);
    ka_oturum_ac();
    ka_arac_silahla();
    ida_in_t *in = ka_girdi();
    (void)ka_mandal(in, (uint8_t)IDA_NARROW_MODE);
    in->narrow.mode = (uint8_t)IDA_MODE_DEPTH_HOLD;
    (void)ka_cagir();
    ka_adimlar(5);
    uint8_t fs0 = ka_out()->fs_level;
    bool kenar = false;
    ka_ayar.imu_bozuk = true;
    for (int k = 0; k < 20 && !kenar; ++k) {
        uint8_t fcs0 = ka_out()->fc.state;
        in = ka_girdi();
        uint32_t id[4];
        id[0] = ka_komut(in, (uint8_t)IDA_CMD_ARM);
        id[1] = ka_komut(in, (uint8_t)IDA_CMD_FAILSAFE_RESET);
        id[2] = ka_fc(in, (uint8_t)IDA_FC_ARM, 1u);
        id[3] = ka_fc(in, (uint8_t)IDA_FC_FIRE, 0u);
        ida_rc_t rc = ka_cagir();
        if (ka_out()->fs_level != fs0) {
            kenar = true;
            (void)snprintf(ne, sizeof ne, "arıza eylemi kenarı (fs %u → %u, dönüş %d) (F1)", (unsigned)fs0,
                           (unsigned)ka_out()->fs_level, (int)rc);
            KA_DOGRULA(ka_out()->fs_level > fs0, "%s: arıza eylemi düzeyi düştü; kenar ağırlaşma bekleniyordu", ne);
            kendi_daraltma_red(id, AD, 4, ne);
            KA_DOGRULA(fcs0 == IDA_FCS_ARMED || ka_out()->fc.state != IDA_FCS_ARMED,
                       "%s: FC_ARM'ın etkisi oluştu (fc.state %s → ARMED; W5-K22)", ne, ka_durum_adi(fcs0));
            KA_DOGRULA(ka_out()->fc.state != IDA_FCS_PERMIT_OPEN, "%s: FC_FIRE'ın etkisi oluştu (PERMIT_OPEN; W5-K22)", ne);
        }
    }
    ka_ayar.imu_bozuk = false;
    KA_DOGRULA(kenar, "IMU kesilmesiyle arıza eylemi düzeyi değişmedi; kenar sınanamadı");
}

static void t_f10_bicim(void)
{
    /* §7 F10: liste dışı op, daraltıcı olmayanda yanlış tüp/arg → OUT_OF_RANGE, durum değişmez;
       biçimi bozuk FC_DISARM/FC_ABORT reddedilmez, bütün tüplere uygulanır. */
    ka_kur(2);
    ka_oturum_ac();
    alan_disi_tek(4u, 0u, 0u, 0u, IDA_REASON_OUT_OF_RANGE, "F10: op 4");
    alan_disi_tek(IDA_FC_FIRE, 0u, 0u, 1u, IDA_REASON_OUT_OF_RANGE, "F10: FC_FIRE arg1 1");
    alan_disi_tek(IDA_FC_ARM, 2u, 0u, 0u, IDA_REASON_OUT_OF_RANGE, "F10: FC_ARM tube 2");
    daraltici_bicim_tek(IDA_FC_DISARM, 0u, 0u, 0u, true);
    daraltici_bicim_tek(IDA_FC_ABORT, 1u, 2u, 3u, false);
}

static const ka_test_t TESTLER[] = {
    {"satirlar", t_gc05_satirlar},
    {"alan_disi", t_gc04_alan_disi},
    {"daraltici_bicim", t_gc32_daraltici_bicim},
    {"yineleme", t_gc05_yineleme},
    {"oncelik", t_gc05_oncelik},
    {"daraltici_komut_ve_oturum", t_gc06_komut_ve_oturum},
    {"daraltici_emniyet_girdisi", t_gc06_emniyet_girdisi},
    {"genisletici_komut_ve_oturum", t_gc33_komut_ve_oturum},
    {"genisletici_emniyet_girdisi", t_gc33_emniyet_girdisi},
    {"daraltmasiz_cagri", t_gc33_daraltmasiz_cagri},
    {"reddedilen_cagri", t_gc34_reddedilen_cagri},
    {"imu_lag", t_gc35_imu_lag},
    {"reset_kilitsiz", t_gc46_kilitsiz},
    {"test_disi", t_gc51_test_disi},
    {"gecis_cagrisi", t_gc16_gecis_cagrisi},
    {"f1_daraltma", t_f1_daraltma},
    {"f1_ariza_kenari", t_f1_ariza_kenari},
    {"f10_bicim", t_f10_bicim},
};

int main(int argc, char **argv)
{
    return ka_main(argc, argv, __FILE__, TESTLER, sizeof TESTLER / sizeof TESTLER[0]);
}
