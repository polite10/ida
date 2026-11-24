/*
 * tests/kabul/atis — silahlı FC: izin, atış olayı, olay sonuçlarına göre geçişler, TEST, arıza
 * senaryolarının silahlı kısmı (F1, F3, F7, F8, F11, F12, F15). Hepsi ka_hazir ile FC'yi
 * ARMED'e alır (FC_ARM satırının engel bitlerinin kalkması gerekir).
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

static uint8_t durum(void)
{
    return ka_out()->fc.state;
}

static const int ADIM = 40; /* olay bekleme sınırı: 200 ms */

/* Param ayarlı hazırlık (B sınıfı parametreler silahsızken). */
static void hazir_param(uint8_t tube, uint32_t apply_by, uint32_t hold)
{
    ka_kur(2);
    if (apply_by != 0u) {
        ka_param_yaz("FC_APPLY_BY_NS", apply_by);
    }
    if (hold != 0u) {
        ka_param_yaz("FC_HOLD_NS", hold);
    }
    ka_oturum_ac();
    ka_arac_silahla();
    ka_kip_sec();
    ka_fc_hazir_bekle();
    ka_fc_silahla(tube);
}

/* ARMED(tube) → FC_FIRE → SAFETY_OPEN → APPLIED → FIRE; FIRE olayını döndürür. */
static const ka_olay_t *atis_dongusu(uint8_t tube)
{
    (void)ka_izin_iste(tube, ADIM);
    const ka_olay_t *f = ka_son_olay((uint8_t)IDA_EVT_FIRE, (int)tube);
    if (f != NULL && f->cagri == ka_cagri_no()) {
        return f;
    }
    f = ka_olay_bekle((uint8_t)IDA_EVT_FIRE, (int)tube, ADIM);
    KA_DOGRULA(f != NULL, "izin APPLIED olduktan sonra %d adımda FIRE(tüp %u) çıkmadı (değişmez (b); W5-K3)", ADIM, (unsigned)tube);
    return f;
}

/* ---- G-Ç47, G-Ç21, G-Ç48 --------------------------------------------------------------------- */

static void t_gc47_izin_yalniz_fc_fire(void)
{
    ka_hazir(2, 0u);
    for (int k = 0; k < 400; ++k) {
        (void)ka_adim();
        KA_DOGRULA(!ka_genisletici_olay_var(ka_out()), "FC_FIRE olmadan genişletici olay (G-Ç47)");
    }
    const ka_olay_t *o = ka_izin_iste(0u, ADIM);
    KA_DOGRULA(o->e.tube == 0u, "SAFETY_OPEN yanlış tüpte");
}

static void t_gc21_fire_baglantisi(void)
{
    ka_hazir(2, 0u);
    (void)atis_dongusu(0u);
    size_t n0 = 0;
    for (size_t i = 0; i < ka_olay_sayisi(); ++i) {
        if (ka_olay_no(i) != NULL && ka_olay_no(i)->e.kind == IDA_EVT_FIRE) {
            ++n0;
        }
    }
    ka_adimlar(400);
    size_t n1 = 0;
    for (size_t i = 0; i < ka_olay_sayisi(); ++i) {
        if (ka_olay_no(i) != NULL && ka_olay_no(i)->e.kind == IDA_EVT_FIRE) {
            ++n1;
        }
    }
    KA_DOGRULA(n0 == 1u && n1 == 1u, "bir FC_FIRE %lu FIRE üretti (G-Ç21: en çok bir)", (unsigned long)n1);
}

static void t_gc48_pencere_sonu(void)
{
    uint32_t pencere_ms = ka_param_u32("T_pencere", 10000u);
    ka_hazir(2, 0u);
    const ka_olay_t *f = atis_dongusu(0u);
    uint64_t c0 = f->cagri;
    bool kapandi = false;
    int sinir = (int)(pencere_ms / 5u) + 400;
    for (int k = 0; k < sinir && !(kapandi && durum() == IDA_FCS_SAFE); ++k) {
        (void)ka_adim();
        if (ka_kapanma_var(ka_out(), 0u)) {
            kapandi = true;
        }
    }
    KA_DOGRULA(kapandi && durum() == IDA_FCS_SAFE, "FIRE APPLIED'dan sonra %d adımda SAFETY_CLOSE(tüp 0) ve SAFE yok (G-Ç48)", sinir);
    (void)c0;
    for (uint8_t t = 0; t < 2u; ++t) {
        ida_in_t *in = ka_girdi();
        uint32_t id = ka_fc(in, (uint8_t)IDA_FC_FIRE, t);
        (void)ka_cagir();
        const ida_cmd_result_t *r = ka_sonuc(id);
        KA_DOGRULA(r != NULL && r->ack != IDA_ACK_ACCEPTED, "pencere sonrası yeni FC_ARM olmadan FC_FIRE(tüp %u) kabul edildi (G-Ç48)",
                   (unsigned)t);
    }
    ka_fc_silahla(1u);
    (void)atis_dongusu(1u);
}

/* ---- G-Ç11, G-Ç40, G-Ç12, G-Ç10 -------------------------------------------------------------- */

static void t_gc11_izin_suresi(void)
{
    /* (a) APPLIED gelmeden izin açık sayılmaz. */
    ka_hazir(2, 0u);
    ka_ayar.politika[IDA_EVT_SAFETY_OPEN].gecikme = 5u;
    (void)ka_izin_iste(0u, ADIM);
    for (int k = 0; k < 3; ++k) {
        (void)ka_adim();
        KA_DOGRULA(durum() != IDA_FCS_PERMIT_OPEN, "APPLIED gelmeden fc.state PERMIT_OPEN (G-Ç11)");
        KA_DOGRULA(ka_olay_say(ka_out(), (uint8_t)IDA_EVT_FIRE, -1) == 0, "APPLIED gelmeden FIRE (G-Ç11)");
    }
    /* (b) Geç gelen APPLIED (hold_ns doldu): izin duvar saatinde kapalıdır. */
    hazir_param(0u, UINT32_C(300000000), UINT32_C(60000000));
    ka_ayar.politika[IDA_EVT_SAFETY_OPEN].gecikme = 20u; /* 100 ms > hold 60 ms */
    const ka_olay_t *o = ka_izin_iste(0u, ADIM);
    uint64_t id = o->e.event_id;
    for (int k = 0; k < 25; ++k) {
        (void)ka_adim();
        const ka_olay_t *e = ka_olay_kimlikle(id);
        if (e != NULL && e->teslim_edildi && e->teslim_edilen_cagri == ka_cagri_no()) {
            KA_DOGRULA(ka_olay_say(ka_out(), (uint8_t)IDA_EVT_FIRE, -1) == 0,
                       "hold_ns dolduktan sonra gelen APPLIED ile FIRE (G-Ç11: izin duvar saatinde kapalı)");
            KA_DOGRULA(durum() != IDA_FCS_PERMIT_OPEN, "hold_ns dolduktan sonra gelen APPLIED ile PERMIT_OPEN (G-Ç11)");
        }
    }
}

static void t_gc40_bekleyen_sonuc(void)
{
    ka_hazir(2, 0u);
    ka_ayar.politika[IDA_EVT_SAFETY_OPEN].gecikme = 12u;
    (void)ka_izin_iste(0u, ADIM);
    for (int k = 0; k < 8; ++k) {
        ida_in_t *in = ka_girdi();
        uint32_t f = ka_fc(in, (uint8_t)IDA_FC_FIRE, 0u);
        (void)ka_cagir();
        KA_DOGRULA((ka_out()->fc.inhibit_mask & IDA_FCI_RESULT_PENDING) != 0u, "SAFETY_OPEN sonucu beklenirken RESULT_PENDING yok (G-Ç40)");
        KA_DOGRULA(ka_olay_say(ka_out(), (uint8_t)IDA_EVT_SAFETY_OPEN, 0) == 0 && ka_olay_say(ka_out(), (uint8_t)IDA_EVT_FIRE, 0) == 0,
                   "SAFETY_OPEN sonucu beklenirken aynı tüpe yeni SAFETY_OPEN/FIRE (G-Ç40)");
        const ida_cmd_result_t *r = ka_sonuc(f);
        KA_DOGRULA(r != NULL && r->ack == IDA_ACK_REJECTED && r->reason == IDA_REASON_NOT_ARMABLE,
                   "sonuç beklenirken FC_FIRE: %u/%u, NOT_ARMABLE bekleniyordu (3.17 FC_FIRE satırı RESULT_PENDING)",
                   r != NULL ? (unsigned)r->ack : 0u, r != NULL ? (unsigned)r->reason : 0u);
    }
    /* FIRE'ın sonucu beklenirken de. */
    ka_hazir(2, 1u);
    ka_ayar.politika[IDA_EVT_FIRE].gecikme = 12u;
    (void)atis_dongusu(1u);
    for (int k = 0; k < 8; ++k) {
        (void)ka_adim();
        KA_DOGRULA((ka_out()->fc.inhibit_mask & IDA_FCI_RESULT_PENDING) != 0u, "FIRE sonucu beklenirken RESULT_PENDING yok (G-Ç40)");
        KA_DOGRULA(ka_olay_say(ka_out(), (uint8_t)IDA_EVT_SAFETY_OPEN, 1) == 0 && ka_olay_say(ka_out(), (uint8_t)IDA_EVT_FIRE, 1) == 0,
                   "FIRE sonucu beklenirken aynı tüpe yeni SAFETY_OPEN/FIRE (G-Ç40)");
    }
}

static void t_gc12_yenileme(void)
{
    /* İzin açık tutulurken ardışık SAFETY_OPEN'ların now_w farkı hold/2'yi aşmaz (önceki sonuç
       bu aralıkta alındıysa). Yenileme, çekirdek izni hold/2'den uzun tuttuğunda gözlenir;
       FIRE hemen üretilirse yenileme gözlenmez ve test yalnız ters yönü (sonuç beklenirken
       yenileme yok) sınar. */
    ka_hazir(2, 0u);
    ka_ayar.permit_elle = true; /* alıcının geri okuması izni kapalı gösterir */
    ka_ayar.permit_deger[0] = 0u;
    uint32_t hold = 0;
    int64_t son_open = -1;
    int64_t son_sonuc = -1;
    int n_open = 0;
    (void)ka_izin_iste(0u, ADIM);
    const ka_olay_t *o = ka_son_olay((uint8_t)IDA_EVT_SAFETY_OPEN, 0);
    son_open = o->now_w;
    hold = o->e.hold_ns;
    n_open = 1;
    for (int k = 0; k < 200; ++k) {
        (void)ka_adim();
        const ka_olay_t *p = ka_son_olay((uint8_t)IDA_EVT_SAFETY_OPEN, 0);
        if (p != NULL && p->now_w != son_open && p->cagri == ka_cagri_no()) {
            const ka_olay_t *onceki = NULL;
            for (size_t i = ka_olay_sayisi(); i > 0; --i) {
                const ka_olay_t *q = ka_olay_no(i - 1u);
                if (q != NULL && q->e.kind == IDA_EVT_SAFETY_OPEN && q->e.tube == 0u && q->now_w == son_open) {
                    onceki = q;
                    break;
                }
            }
            if (onceki != NULL && onceki->teslim_edildi && onceki->teslim_now - son_open <= (int64_t)hold / 2) {
                son_sonuc = onceki->teslim_now;
                KA_DOGRULA(p->now_w - son_open <= (int64_t)hold / 2,
                           "ardışık SAFETY_OPEN arası %lld ms > hold/2 = %lld ms (G-Ç12; A4-K107)",
                           (long long)((p->now_w - son_open) / KA_MS), (long long)(hold / 2u / 1000000u));
            }
            son_open = p->now_w;
            ++n_open;
        }
        if (ka_olay_say(ka_out(), (uint8_t)IDA_EVT_FIRE, 0) > 0 || durum() == IDA_FCS_SAFE) {
            break;
        }
    }
    (void)son_sonuc;
    if (n_open < 2) {
        TEST_MESSAGE("G-Ç12: izin hold/2'den uzun tutulmadı; yenileme aralığı gözlenemedi (yalnız G-Ç40 yönü sınandı)");
    }
    /* Ters yön: sonuç gelmeden yenileme yok. */
    ka_hazir(2, 1u);
    ka_ayar.politika[IDA_EVT_SAFETY_OPEN].gecikme = 60u; /* 300 ms > apply_by + 100 ms: bilinmeyen */
    (void)ka_izin_iste(1u, ADIM);
    for (int k = 0; k < 20; ++k) {
        (void)ka_adim();
        KA_DOGRULA(ka_olay_say(ka_out(), (uint8_t)IDA_EVT_SAFETY_OPEN, 1) == 0, "önceki SAFETY_OPEN'ın sonucu gelmeden yenileme (G-Ç12, G-Ç40)");
    }
}

static void t_gc10_izin_alanlari(void)
{
    ka_hazir(2, 0u);
    const ka_olay_t *o = ka_izin_iste(0u, ADIM);
    KA_DOGRULA(o->e.hold_ns == ka_param_u32("FC_HOLD_NS", KA_HOLD_VARSAYILAN_NS) && o->e.hold_ns == KA_HOLD_VARSAYILAN_NS,
               "SAFETY_OPEN hold_ns %lu, FC_HOLD_NS varsayılanı 200 ms (G-Ç10; A1 §3.9)", (unsigned long)o->e.hold_ns);
    KA_DOGRULA(o->e.apply_by_ns == KA_APPLY_BY_VARSAYILAN_NS, "SAFETY_OPEN apply_by_ns %lu (G-Ç10)", (unsigned long)o->e.apply_by_ns);
    const ka_olay_t *f = ka_olay_bekle((uint8_t)IDA_EVT_FIRE, 0, ADIM);
    KA_DOGRULA(f != NULL, "FIRE çıkmadı");
    KA_DOGRULA(f->e.hold_ns == 0u && f->e.apply_by_ns == KA_APPLY_BY_VARSAYILAN_NS, "FIRE hold_ns %lu / apply_by_ns %lu (G-Ç10)",
               (unsigned long)f->e.hold_ns, (unsigned long)f->e.apply_by_ns);
    hazir_param(1u, UINT32_C(300000000), UINT32_C(60000000));
    o = ka_izin_iste(1u, ADIM);
    KA_DOGRULA(o->e.hold_ns == UINT32_C(60000000) && o->e.apply_by_ns == UINT32_C(300000000),
               "parametre değişince SAFETY_OPEN hold/apply_by %lu/%lu (G-Ç10)", (unsigned long)o->e.hold_ns,
               (unsigned long)o->e.apply_by_ns);
}

/* ---- G-Ç23, G-Ç49 ----------------------------------------------------------------------------------- */

static void sonuc_gecisi(uint8_t tur, uint8_t kod, bool kilit, const char *ne)
{
    ka_hazir(2, 0u);
    if (tur == IDA_EVT_SAFETY_OPEN) {
        ka_ayar.politika[IDA_EVT_SAFETY_OPEN].kod = kod;
        ka_ayar.politika[IDA_EVT_SAFETY_OPEN].uygula = false;
        (void)ka_izin_iste(0u, ADIM);
    } else {
        ka_ayar.politika[IDA_EVT_FIRE].kod = kod;
        ka_ayar.politika[IDA_EVT_FIRE].uygula = false;
        (void)atis_dongusu(0u);
    }
    const ka_olay_t *e = ka_son_olay(tur, 0);
    uint64_t id = e->e.event_id;
    int64_t sinir = e->now_w + (int64_t)e->e.apply_by_ns + (int64_t)IDA_EVT_RESULT_MARGIN_NS;
    bool tamam = false;
    for (int k = 0; k < 60 && !tamam; ++k) {
        (void)ka_adim();
        const ka_olay_t *s = ka_olay_kimlikle(id);
        bool karar = kod == KA_SONUC_YOK ? ka_simdi() > sinir : (s != NULL && s->teslim_edildi);
        if (!karar) {
            continue;
        }
        tamam = true;
        uint8_t beklenen = kilit ? (uint8_t)IDA_FCS_FAULT_LATCHED : (uint8_t)IDA_FCS_SAFE;
        KA_DOGRULA(durum() == beklenen || (kod == KA_SONUC_YOK && kilit && durum() == IDA_FCS_FAULT_LATCHED),
                   "%s: sonucun işlendiği çağrıda fc.state %s, %s bekleniyordu", ne, ka_durum_adi(durum()),
                   ka_durum_adi(beklenen));
        bool kapanma = ka_kapanma_var(ka_out(), 0u);
        for (size_t i = ka_olay_sayisi(); !kapanma && i > 0; --i) {
            const ka_olay_t *q = ka_olay_no(i - 1u);
            if (q == NULL || q->e.event_id <= id) {
                break;
            }
            kapanma = q->e.kind == IDA_EVT_SAFETY_CLOSE && (q->e.tube == 0u || q->e.tube == IDA_TUBE_ALL);
        }
        KA_DOGRULA(kapanma, "%s: SAFETY_CLOSE(tüp 0) üretilmedi", ne);
    }
    KA_DOGRULA(tamam, "%s: sonuç işlenmedi", ne);
    for (int k = 0; k < 100; ++k) {
        (void)ka_adim();
        KA_DOGRULA(ka_olay_say(ka_out(), (uint8_t)IDA_EVT_FIRE, -1) == 0, "%s: sonrasında yeni FC_FIRE olmadan FIRE", ne);
        KA_DOGRULA(!kilit || durum() == IDA_FCS_FAULT_LATCHED, "%s: kilit kendiliğinden kalktı", ne);
    }
}

static void t_gc23_izin_sonucu(void)
{
    static const uint8_t kodlar[] = {IDA_EVR_DUPLICATE,      IDA_EVR_REJ_EXPIRED,     IDA_EVR_REJ_SAFE,
                                     IDA_EVR_REJ_TUBE_EMPTY, IDA_EVR_REJ_SESSION,     IDA_EVR_DROPPED_GATE,
                                     IDA_EVR_DROPPED_NO_SLOT, IDA_EVR_EXPIRED_NO_ACK, IDA_EVR_SESSION_END,
                                     KA_SONUC_YOK};
    for (size_t i = 0; i < sizeof kodlar; ++i) {
        char ne[96];
        (void)snprintf(ne, sizeof ne, "SAFETY_OPEN sonucu %u (G-Ç23)", (unsigned)kodlar[i]);
        sonuc_gecisi((uint8_t)IDA_EVT_SAFETY_OPEN, kodlar[i], false, ne);
    }
}

static void t_gc49_fire_sonucu(void)
{
    static const uint8_t kilitli[] = {IDA_EVR_REJ_TUBE_EMPTY, IDA_EVR_EXPIRED_NO_ACK, KA_SONUC_YOK};
    static const uint8_t guvenli[] = {IDA_EVR_REJ_SAFE,      IDA_EVR_REJ_EXPIRED,     IDA_EVR_REJ_SESSION,
                                      IDA_EVR_DROPPED_GATE, IDA_EVR_DROPPED_NO_SLOT, IDA_EVR_SESSION_END};
    char ne[96];
    for (size_t i = 0; i < sizeof kilitli; ++i) {
        (void)snprintf(ne, sizeof ne, "FIRE sonucu %u → FAULT_LATCHED (G-Ç49)", (unsigned)kilitli[i]);
        sonuc_gecisi((uint8_t)IDA_EVT_FIRE, kilitli[i], true, ne);
    }
    for (size_t i = 0; i < sizeof guvenli; ++i) {
        (void)snprintf(ne, sizeof ne, "FIRE sonucu %u → SAFE (G-Ç49)", (unsigned)guvenli[i]);
        sonuc_gecisi((uint8_t)IDA_EVT_FIRE, guvenli[i], false, ne);
    }
}

/* ---- G-Ç53 (dolu), G-Ç35, G-Ç07 ---------------------------------------------------------------- */

static void t_gc53_dolu_celiskisi(void)
{
    ka_hazir(2, 0u);
    ka_ayar.loaded_elle = true; /* alıcı LOADED'ı düşürmez: FIRE APPLIED sonrası tüp dolu görünür */
    (void)atis_dongusu(0u);
    bool kilit = false;
    for (int k = 0; k < 3 && !kilit; ++k) {
        (void)ka_adim();
        kilit = durum() == IDA_FCS_FAULT_LATCHED;
    }
    KA_DOGRULA(kilit, "FIRE APPLIED sonrası t_meas > t_apply örnekte LOADED = 1: FAULT_LATCHED bekleniyordu (G-Ç53, W5-K4)");
}

static void t_gc35_izin_yok(void)
{
    ka_hazir(2, 0u);
    ka_ayar.politika[IDA_EVT_SAFETY_OPEN].gecikme = 8u;
    (void)ka_izin_iste(0u, ADIM);
    ka_ayar.imu_gecikme = 25 * KA_MS;
    for (int k = 0; k < 30; ++k) {
        (void)ka_adim();
        if (ka_out()->health.imu_lag) {
            KA_DOGRULA(!ka_genisletici_olay_var(ka_out()), "IMU_LAG kuruluyken SAFETY_OPEN/FIRE (G-Ç35)");
        }
    }
}

static void t_gc07_yalniz_step_ok(void)
{
    ka_hazir(2, 0u);
    /* FC_FIRE zaman çerçeveli tick'te: tick çıktısı genişletici olay taşımaz. */
    ka_ayar.tick = true;
    ida_in_t *in = ka_girdi();
    (void)ka_fc(in, (uint8_t)IDA_FC_FIRE, 0u);
    (void)ka_cagir();
    KA_DOGRULA(!ka_genisletici_olay_var(ka_out()), "tick çıktısında genişletici olay (G-Ç07)");
    ka_ayar.tick = false;
    const ka_olay_t *o = ka_son_olay((uint8_t)IDA_EVT_SAFETY_OPEN, 0);
    if (o == NULL) {
        o = ka_olay_bekle((uint8_t)IDA_EVT_SAFETY_OPEN, 0, ADIM);
    }
    if (o == NULL) {
        o = ka_izin_iste(0u, ADIM);
    }
    /* APPLIED'ın geldiği çağrı IMU_REJECTED: FIRE yok; sonra step OK'de olabilir. */
    ka_ayar.imu_bozuk = true;
    (void)ka_adim();
    KA_DOGRULA(!ka_genisletici_olay_var(ka_out()), "IMU_REJECTED çıktısında genişletici olay (G-Ç07)");
    ka_ayar.imu_bozuk = false;
    ka_ayar.tick = true;
    ka_adimlar(2);
    ka_ayar.tick = false;
    ka_adimlar(10);
}

/* ---- G-Ç22, G-Ç05 (tüp engelleri), G-Ç24, G-Ç51 --------------------------------------------------- */

static void t_gc22_durum_kosulu(void)
{
    ka_hazir(2, 0u);
    ida_in_t *in = ka_girdi();
    uint32_t a = ka_fc(in, (uint8_t)IDA_FC_ARM, 0u);
    (void)ka_cagir();
    KA_SONUC(a, IDA_ACK_REJECTED, IDA_REASON_BAD_MODE, "ARMED'de FC_ARM (G-Ç22)");
    in = ka_girdi();
    uint32_t t = ka_fc(in, (uint8_t)IDA_FC_TEST_ENTER, 0u);
    (void)ka_cagir();
    KA_SONUC(t, IDA_ACK_REJECTED, IDA_REASON_BAD_MODE, "ARMED'de FC_TEST_ENTER (G-Ç22)");
    in = ka_girdi();
    uint32_t f = ka_fc(in, (uint8_t)IDA_FC_FIRE, 1u);
    (void)ka_cagir();
    KA_SONUC(f, IDA_ACK_REJECTED, IDA_REASON_BAD_MODE, "FC_ARM'ın kabul edilmediği tüpe FC_FIRE (G-Ç22)");
    /* SAFE (engeller yok) iken FC_FIRE. */
    in = ka_girdi();
    (void)ka_mandal(in, (uint8_t)IDA_NARROW_FC_DISARM);
    (void)ka_cagir();
    ka_adimlar(2);
    KA_DOGRULA(durum() == IDA_FCS_SAFE, "FC_DISARM sonrası fc.state %s", ka_durum_adi(durum()));
    in = ka_girdi();
    f = ka_fc(in, (uint8_t)IDA_FC_FIRE, 0u);
    (void)ka_cagir();
    KA_SONUC(f, IDA_ACK_REJECTED, IDA_REASON_BAD_MODE, "SAFE'te FC_FIRE (G-Ç22)");
    /* TEST'te FC_ARM ve FC_FIRE. */
    in = ka_girdi();
    t = ka_fc(in, (uint8_t)IDA_FC_TEST_ENTER, 0u);
    (void)ka_cagir();
    KA_SONUC(t, IDA_ACK_ACCEPTED, IDA_REASON_NONE, "SAFE'te FC_TEST_ENTER (G-Ç22)");
    KA_DOGRULA(durum() == IDA_FCS_TEST, "FC_TEST_ENTER sonrası fc.state %s", ka_durum_adi(durum()));
    in = ka_girdi();
    a = ka_fc(in, (uint8_t)IDA_FC_ARM, 0u);
    (void)ka_cagir();
    KA_SONUC(a, IDA_ACK_REJECTED, IDA_REASON_BAD_MODE, "TEST'te FC_ARM (G-Ç22, F15)");
    in = ka_girdi();
    f = ka_fc(in, (uint8_t)IDA_FC_FIRE, 0u);
    (void)ka_cagir();
    KA_SONUC(f, IDA_ACK_REJECTED, IDA_REASON_BAD_MODE, "TEST'te FC_FIRE (G-Ç22, F15)");
}

static void t_gc05_fc_fire_tup_engelleri(void)
{
    /* EMPTY */
    ka_hazir(2, 0u);
    ka_ayar.tup[0] = tup_eksik(IDA_TUBE_LOADED);
    ka_adimlar(2);
    ida_in_t *in = ka_girdi();
    uint32_t f = ka_fc(in, (uint8_t)IDA_FC_FIRE, 0u);
    (void)ka_cagir();
    KA_SONUC(f, IDA_ACK_REJECTED, IDA_REASON_NOT_ARMABLE, "boş tüpe FC_FIRE (3.17 FC_FIRE satırı EMPTY)");
    /* PLUG ve PIN (süregelen) */
    for (int d = 0; d < 2; ++d) {
        ka_hazir(2, 0u);
        ka_ayar.tup[0] = tup_eksik(d == 0 ? IDA_TUBE_PLUG_IN : IDA_TUBE_PIN_OUT);
        ka_adimlar(2);
        in = ka_girdi();
        f = ka_fc(in, (uint8_t)IDA_FC_FIRE, 0u);
        (void)ka_cagir();
        KA_SONUC(f, IDA_ACK_REJECTED, IDA_REASON_NOT_ARMABLE, d == 0 ? "fişi çıkık tüpe FC_FIRE (3.17)" : "pimi takılı tüpe FC_FIRE (3.17)");
    }
}

static void test_kipi(const char *ne)
{
    kc_rng_t r;
    kc_rng_kur(&r, UINT64_C(0xF15));
    ka_hazir_oncesi(2);
    ida_in_t *in = ka_girdi();
    uint32_t t = ka_fc(in, (uint8_t)IDA_FC_TEST_ENTER, 0u);
    (void)ka_cagir();
    KA_SONUC(t, IDA_ACK_ACCEPTED, IDA_REASON_NONE, "SAFE'te FC_TEST_ENTER");
    KA_DOGRULA(durum() == IDA_FCS_TEST, "%s: fc.state %s", ne, ka_durum_adi(durum()));
    for (int k = 0; k < 600; ++k) {
        gevsek();
        uint32_t z = kc_rng_aralik(&r, 10u);
        if (z == 0u) {
            ka_ayar.estop = 1u;
        } else if (z == 1u) {
            ka_ayar.tup[kc_rng_aralik(&r, 2u) & 1u] = tup_eksik(IDA_TUBE_PLUG_IN);
        }
        in = ka_girdi();
        uint32_t f = ka_fc(in, (uint8_t)IDA_FC_FIRE, (uint8_t)kc_rng_aralik(&r, 2u));
        uint32_t a = ka_fc(in, (uint8_t)IDA_FC_ARM, (uint8_t)kc_rng_aralik(&r, 2u));
        if (kc_rng_aralik(&r, 5u) == 0u) {
            ka_sonuc_ekle(in, ((uint64_t)KC_BOOT << 32) | (kc_rng_u64(&r) & 0xFFFFu), (uint8_t)(1u + kc_rng_aralik(&r, 6u)), 0);
        }
        bool testte = durum() == IDA_FCS_TEST;
        (void)ka_cagir();
        if (testte) {
            KA_DOGRULA(!ka_genisletici_olay_var(ka_out()), "%s: TEST'te SAFETY_OPEN/FIRE (G-Ç24)", ne);
            const ida_cmd_result_t *rf = ka_sonuc(f);
            const ida_cmd_result_t *ra = ka_sonuc(a);
            KA_DOGRULA(rf != NULL && rf->ack != IDA_ACK_ACCEPTED && ra != NULL && ra->ack != IDA_ACK_ACCEPTED,
                       "%s: TEST'te FC_FIRE ya da FC_ARM kabul edildi (F15, G-Ç22)", ne);
        }
        if (durum() != IDA_FCS_TEST) {
            /* Güvenli duruma dönüş TEST'ten de çıkarabilir; yeniden gir. */
            gevsek();
            ka_adimlar(2);
            in = ka_girdi();
            (void)ka_fc(in, (uint8_t)IDA_FC_TEST_ENTER, 0u);
            (void)ka_cagir();
        }
    }
}

static void t_gc24_test_kipi(void)
{
    test_kipi("G-Ç24");
}

static void t_f15_test_kipi(void)
{
    test_kipi("F15");
}

static void t_gc51_testten_cikis(void)
{
    ka_hazir_oncesi(2);
    ida_in_t *in = ka_girdi();
    (void)ka_fc(in, (uint8_t)IDA_FC_TEST_ENTER, 0u);
    (void)ka_cagir();
    KA_DOGRULA(durum() == IDA_FCS_TEST, "FC_TEST_ENTER sonrası fc.state %s", ka_durum_adi(durum()));
    in = ka_girdi();
    uint32_t x = ka_fc(in, (uint8_t)IDA_FC_TEST_EXIT, 0u);
    (void)ka_cagir();
    KA_SONUC(x, IDA_ACK_ACCEPTED, IDA_REASON_NONE, "TEST'te FC_TEST_EXIT (G-Ç51)");
    KA_DOGRULA(durum() == IDA_FCS_SAFE, "FC_TEST_EXIT'in çağrısında fc.state %s, SAFE bekleniyordu (G-Ç51)", ka_durum_adi(durum()));
}

/* ---- G-Ç16, G-Ç32, G-Ç06 (etki), G-Ç25 ------------------------------------------------------------- */

static void t_gc16_diger_tup(void)
{
    ka_hazir_oncesi(2);
    ka_ayar.tup[0] = tup_eksik(IDA_TUBE_PLUG_IN);
    ka_adimlar(3);
    ka_fc_silahla(1u);
    (void)atis_dongusu(1u);
    for (size_t i = 0; i < ka_olay_sayisi(); ++i) {
        const ka_olay_t *o = ka_olay_no(i);
        KA_DOGRULA(o == NULL || o->e.kind == IDA_EVT_SAFETY_CLOSE || o->e.tube == 1u, "fişi çıkık tüp 0'a genişletici olay (G-Ç16)");
    }
}

/* FC_ABORT'un etkisi (W5-K23): atış dizisinin iptali; silahı indirmez. Çağrının sonunda açık
   sayılan izin yok (model ve dış durum), dış durum PERMIT_OPEN ya da FIRE_PENDING değil, o
   çağrıda genişletici olay yok ve önceden açık sayılan her izin için SAFETY_CLOSE çıktı. Önceden
   silahlıysa ARMED kalabilir (FC_DISARM'dan farkı). */
static void abort_etkisi_dogrula(const char *ne, const bool *izin_once)
{
    const ida_out_t *o = ka_out();
    /* FIRE_PENDING yalnız daha önce çıkmış bir FIRE'ın sonucu hâlâ bekleniyorsa (A4 §3.16 tanımı;
       çıkmış FIRE geri alınamaz). */
    KA_DOGRULA(o->fc.state != IDA_FCS_PERMIT_OPEN &&
                   (o->fc.state != IDA_FCS_FIRE_PENDING || ka_onceki_fire_sonucu_bekleniyor()),
               "%s çağrısının sonunda fc.state %s: atış dizisi iptal edilmedi (W5-K23, G-Ç06, G-Ç32)", ne,
               ka_durum_adi(o->fc.state));
    KA_DOGRULA(!ka_genisletici_olay_var(o), "%s çağrısında genişletici olay (W5-K23, G-Ç06)", ne);
    for (unsigned t = 0; t < IDA_PORT_N_TUBE; ++t) {
        KA_DOGRULA(!izin_once[t] || ka_kapanma_var(o, (uint8_t)t),
                   "%s: açık izinli tüp %u için SAFETY_CLOSE yok (W5-K23: SAFETY_CLOSE(ALL); G-Ç06, G-Ç32)", ne, t);
        KA_DOGRULA(!ka_model_izin_acik((uint8_t)t, ka_simdi()),
                   "%s çağrısının sonunda tüp %u'nun izni çekirdekçe açık sayılıyor (W5-K23)", ne, t);
    }
}

static void t_gc32_butun_tuplere(void)
{
    ka_hazir(2, 0u);
    (void)ka_izin_iste(0u, ADIM);
    ida_in_t *in = ka_girdi();
    bool izin_once[IDA_PORT_N_TUBE];
    for (unsigned t = 0; t < IDA_PORT_N_TUBE; ++t) {
        izin_once[t] = ka_model_izin_acik((uint8_t)t, in->now_w.ns);
    }
    uint32_t x = ka_fc(in, (uint8_t)IDA_FC_ABORT, 1u);
    (*ka_son_komut(in)).u.fc.arg0 = 7u;
    (void)ka_cagir();
    KA_SONUC(x, IDA_ACK_ACCEPTED, IDA_REASON_NONE, "FC_ABORT(tüp 1, arg0 7)");
    /* Tüp 1'e yazılmış (ve alanı bozuk) FC_ABORT tüp 0'ın iznini de kapatır (W5-K7: bütün
       tüplere). SAFETY_OPEN(tüp 0)'ın APPLIED sonucu bu çağrıda gelir: izin alıcıda açıktır ve
       çağrının sonunda çekirdekçe açık sayılmamalıdır. */
    abort_etkisi_dogrula("FC_ABORT(tüp 1)", izin_once);
    KA_DOGRULA(ka_kapanma_var(ka_out(), 0u), "FC_ABORT(tüp 1): tüp 0'ın izni için SAFETY_CLOSE yok (W5-K7, W5-K23)");
    /* Yeni bir kabul edilmiş FC_FIRE olmadan izin yeniden açılmaz ve atış olmaz (W5-K23). Yetkinin
       kesildiği düzenek modelinde de denetlenir (G-Ç21, G-Ç47). */
    for (int k = 0; k < 60; ++k) {
        (void)ka_adim();
        KA_DOGRULA(ka_olay_say(ka_out(), (uint8_t)IDA_EVT_FIRE, -1) == 0, "FC_ABORT sonrası FIRE (G-Ç32, W5-K23)");
        KA_DOGRULA(ka_olay_say(ka_out(), (uint8_t)IDA_EVT_SAFETY_OPEN, -1) == 0,
                   "FC_ABORT sonrası yeni FC_FIRE olmadan SAFETY_OPEN (W5-K23)");
        KA_DOGRULA(durum() != IDA_FCS_PERMIT_OPEN && durum() != IDA_FCS_FIRE_PENDING,
                   "FC_ABORT sonrası yeni FC_FIRE olmadan fc.state %s (W5-K23)", ka_durum_adi(durum()));
    }
}

static void t_gc06_etki(void)
{
    enum { E_FCDISARM, E_FCABORT, E_OTURUM, E_ESTOP, E_FIS, E_PIM, E_RED_ABORT, E_SAYI };
    static const char *const ad[E_SAYI] = {"FC_DISARM", "FC_ABORT", "SESSION_END", "acil durdurma", "tüp 0 fişi", "tüp 0 pimi",
                                           "DUP_SEQ çağrısında FC_ABORT"};
    for (int d = 0; d < E_SAYI; ++d) {
        ka_hazir(2, 0u);
        (void)ka_izin_iste(0u, ADIM);
        ka_ayar.politika[IDA_EVT_FIRE].gecikme = 1u;
        if (d == E_ESTOP) {
            ka_ayar.estop = 1u;
        } else if (d == E_FIS) {
            ka_ayar.tup[0] = tup_eksik(IDA_TUBE_PLUG_IN);
        } else if (d == E_PIM) {
            ka_ayar.tup[0] = tup_eksik(IDA_TUBE_PIN_OUT);
        }
        ida_in_t *in = d == E_RED_ABORT ? ka_girdi_seq(kc_son_seq()) : ka_girdi();
        bool izin_once[IDA_PORT_N_TUBE];
        for (unsigned t = 0; t < IDA_PORT_N_TUBE; ++t) {
            izin_once[t] = ka_model_izin_acik((uint8_t)t, in->now_w.ns);
        }
        if (d == E_FCDISARM) {
            (void)ka_mandal(in, (uint8_t)IDA_NARROW_FC_DISARM);
        } else if (d == E_FCABORT || d == E_RED_ABORT) {
            (void)ka_mandal(in, (uint8_t)IDA_NARROW_FC_ABORT);
        } else if (d == E_OTURUM) {
            in->plat.link_event = (uint8_t)IDA_LINKEV_SESSION_END;
            in->plat.link_state = (uint8_t)IDA_LINK_NONE;
        }
        (void)ka_cagir();
        KA_DOGRULA(!ka_genisletici_olay_var(ka_out()), "%s: genişletici olay (G-Ç06)", ad[d]);
        if (d == E_FCABORT || d == E_RED_ABORT) {
            /* W5-K23: FC_ABORT silahı indirmez; atış dizisini iptal eder (ARMED kalabilir). */
            abort_etkisi_dogrula(ad[d], izin_once);
        } else {
            KA_DOGRULA(durum() != IDA_FCS_ARMED && durum() != IDA_FCS_PERMIT_OPEN,
                       "%s çağrısının sonunda fc.state %s: daraltma uygulanmadı (G-Ç06, G-Ç19)", ad[d], ka_durum_adi(durum()));
        }
        KA_DOGRULA(ka_kapanma_var(ka_out(), 0u), "%s: açık izin varken SAFETY_CLOSE(tüp 0) yok (G-Ç06)", ad[d]);
        if (d == E_OTURUM) {
            ka_ayar.link_state = (uint8_t)IDA_LINK_NONE;
        }
        ka_adimlar(60);
    }
}

static void t_gc25_hb_kaybi(void)
{
    uint32_t hb_ms = ka_param_u32("T_hb_fc", 30000u);
    ka_hazir(2, 0u);
    (void)ka_izin_iste(0u, ADIM);
    ka_ayar.heartbeat = false;
    int sinir = (int)(hb_ms / 5u) + 2;
    bool kapanma = false;
    for (int k = 0; k < sinir && durum() != IDA_FCS_SAFE; ++k) {
        (void)ka_adim();
        kapanma = kapanma || ka_kapanma_var(ka_out(), 0u);
    }
    KA_DOGRULA(durum() == IDA_FCS_SAFE, "kalp atışı %lu ms gelmeyince fc.state %s, SAFE bekleniyordu (G-Ç25)", (unsigned long)hb_ms,
               ka_durum_adi(durum()));
    KA_DOGRULA(kapanma, "kalp atışı kaybında silahlı tüpe SAFETY_CLOSE yok (G-Ç25)");
}

/* ---- §7 arıza senaryolarının silahlı kısmı ------------------------------------------------------------ */

static void t_f1_silahli(void)
{
    for (int d = 0; d < 5; ++d) {
        ka_hazir(2, 0u);
        if (d == 2) {
            ka_ayar.estop = 1u;
        }
        ida_in_t *in = d == 4 ? ka_girdi_seq(kc_son_seq()) : ka_girdi();
        if (d == 0 || d == 4) {
            (void)ka_mandal(in, (uint8_t)IDA_NARROW_DISARM);
        } else if (d == 1) {
            (void)ka_mandal(in, (uint8_t)IDA_NARROW_MODE);
        } else if (d == 3) {
            (void)ka_mandal(in, (uint8_t)IDA_NARROW_FC_DISARM);
        }
        uint32_t f = ka_fc(in, (uint8_t)IDA_FC_FIRE, 0u);
        (void)ka_cagir();
        const ida_cmd_result_t *r = ka_sonuc(f);
        KA_DOGRULA(r != NULL && r->ack == IDA_ACK_REJECTED, "F1 (%d): daraltmayla aynı çağrıdaki FC_FIRE kabul edildi", d);
        ka_ayar.estop = 0u;
        for (int k = 0; k < 100; ++k) {
            (void)ka_adim();
            KA_DOGRULA(!ka_genisletici_olay_var(ka_out()), "F1 (%d): FC_FIRE reddedildikten sonra genişletici olay", d);
        }
    }
    /* (iii) tüp 0'ın fişi çıkarken tüp 1'e FC_FIRE; sonraki çağrıda tüp 1 etkilenmez. */
    ka_hazir_oncesi(2);
    ka_fc_silahla(1u);
    ka_ayar.tup[0] = tup_eksik(IDA_TUBE_PLUG_IN);
    ida_in_t *in = ka_girdi();
    uint32_t f = ka_fc(in, (uint8_t)IDA_FC_FIRE, 1u);
    (void)ka_cagir();
    KA_SONUC(f, IDA_ACK_REJECTED, IDA_REASON_NARROWED, "F1 (iii)");
    if (durum() != IDA_FCS_ARMED) {
        ka_fc_silahla(1u);
    }
    (void)atis_dongusu(1u);
}

static void t_f3_kopma(void)
{
    for (int d = 0; d < 3; ++d) {
        /* 0: izin açık (FIRE_AUTHORIZED), 1: FIRE sonucu beklenirken, 2: FIRE APPLIED sonrası (ASSESS) */
        ka_hazir(2, 0u);
        if (d == 0) {
            ka_ayar.politika[IDA_EVT_FIRE].gecikme = 30u;
            ka_ayar.politika[IDA_EVT_SAFETY_OPEN].gecikme = 1u;
            (void)ka_izin_iste(0u, ADIM);
        } else {
            ka_ayar.politika[IDA_EVT_FIRE].gecikme = d == 1 ? 15u : 1u;
            (void)atis_dongusu(0u);
            if (d == 2) {
                ka_adimlar(2);
            }
        }
        ida_in_t *in = ka_girdi();
        in->plat.link_event = (uint8_t)IDA_LINKEV_SESSION_END;
        in->plat.link_state = (uint8_t)IDA_LINK_NONE;
        (void)ka_cagir();
        KA_DOGRULA(ka_olay_say(ka_out(), (uint8_t)IDA_EVT_SAFETY_CLOSE, IDA_TUBE_ALL) == 1 && !ka_out()->armed,
                   "F3 (%d): kopmada SAFETY_CLOSE(ALL) ve armed = 0 bekleniyordu", d);
        ka_ayar.link_state = (uint8_t)IDA_LINK_NONE;
        for (int k = 0; k < 40; ++k) {
            (void)ka_adim();
            KA_DOGRULA(!ka_genisletici_olay_var(ka_out()), "F3 (%d): kopmadan sonra genişletici olay", d);
        }
        KA_DOGRULA(durum() == IDA_FCS_SAFE, "F3 (%d): kopmadan sonra fc.state %s, SAFE bekleniyordu (bekleyen olaylar SESSION_END aldı)",
                   d, ka_durum_adi(durum()));
    }
}

static void t_f8_acil_durdurma_atista(void)
{
    /* FIRE sonucu beklenirken acil durdurma: kapatma; FIRE REJ_SAFE alır → SAFE (kilit yok). */
    ka_hazir(2, 0u);
    ka_ayar.politika[IDA_EVT_FIRE].kod = (uint8_t)IDA_EVR_REJ_SAFE;
    ka_ayar.politika[IDA_EVT_FIRE].gecikme = 4u;
    ka_ayar.politika[IDA_EVT_FIRE].uygula = false;
    (void)atis_dongusu(0u);
    ka_ayar.estop = 1u;
    (void)ka_adim();
    KA_DOGRULA(ka_olay_say(ka_out(), (uint8_t)IDA_EVT_SAFETY_CLOSE, IDA_TUBE_ALL) == 1, "F8: atış beklenirken acil durdurmada SAFETY_CLOSE(ALL) yok");
    ka_adimlar(10);
    KA_DOGRULA(durum() == IDA_FCS_SAFE, "F8: FIRE REJ_SAFE sonrası fc.state %s, SAFE bekleniyordu (G-Ç49)", ka_durum_adi(durum()));
    /* FIRE uygulandıktan sonra acil durdurma: kapatma, kilit yok. */
    ka_hazir(2, 1u);
    (void)atis_dongusu(1u);
    ka_adimlar(2);
    ka_ayar.estop = 1u;
    (void)ka_adim();
    KA_DOGRULA(ka_kapanma_var(ka_out(), 1u), "F8: FIRE sonrası acil durdurmada SAFETY_CLOSE yok");
    ka_adimlar(10);
    KA_DOGRULA(durum() != IDA_FCS_FAULT_LATCHED, "F8: uygulanmış FIRE sonrası acil durdurma kilitledi");
}

static void t_f11_revizyon(void)
{
    ka_hazir(2, 0u);
    ka_ayar.politika[IDA_EVT_SAFETY_OPEN].kod = (uint8_t)IDA_EVR_REJ_SAFE;
    ka_ayar.politika[IDA_EVT_SAFETY_OPEN].gecikme = 6u;
    ka_ayar.politika[IDA_EVT_SAFETY_OPEN].uygula = false;
    (void)ka_izin_iste(0u, ADIM);
    ka_ayar.estop = 1u;
    (void)ka_adim();
    KA_DOGRULA(ka_olay_say(ka_out(), (uint8_t)IDA_EVT_SAFETY_CLOSE, IDA_TUBE_ALL) == 1, "F11: bekleyen izinde acil durdurma kapanması yok");
    ka_adimlar(2);
    ka_ayar.estop = 0u;
    for (int k = 0; k < 200; ++k) {
        (void)ka_adim();
        KA_DOGRULA(!ka_genisletici_olay_var(ka_out()), "F11: gevşemeden sonra eski yetki geri geldi (genişletici olay)");
    }
    KA_DOGRULA(durum() == IDA_FCS_SAFE, "F11: fc.state %s, SAFE bekleniyordu", ka_durum_adi(durum()));
}

static void t_f12_tek_cikis(void)
{
    ka_hazir(2, 0u);
    (void)atis_dongusu(0u);
    /* Aynı sonucu yinele: yok sayılır, olay yeniden çıkmaz. */
    const ka_olay_t *f = ka_son_olay((uint8_t)IDA_EVT_FIRE, 0);
    uint64_t id = f->e.event_id;
    int64_t td = f->e.t_decide.ns;
    ka_adimlar(2);
    uint32_t ign = ka_out()->status.cnt.evres_ignored;
    ida_in_t *in = ka_girdi();
    ka_sonuc_ekle(in, id, (uint8_t)IDA_EVR_APPLIED, td + KA_MS);
    (void)ka_cagir();
    KA_DOGRULA(ka_out()->status.cnt.evres_ignored == ign + 1u, "F12: yinelenen olay sonucu yok sayılmadı (G-Ç13)");
    ka_adimlar(200);
    for (size_t i = 1; i < ka_olay_sayisi(); ++i) {
        KA_DOGRULA(ka_olay_no(i)->e.event_id > ka_olay_no(i - 1u)->e.event_id, "F12: olay kimliği yinelendi ya da azaldı (G-Ç08)");
    }
}

static void t_f7_silahli(void)
{
    /* Olay yanıtı kaybı (FIRE) → FAULT_LATCHED; FIRE sonrası tüp dolu görünür → FAULT_LATCHED. */
    sonuc_gecisi((uint8_t)IDA_EVT_FIRE, KA_SONUC_YOK, true, "F7: FIRE yanıtı kaybı");
    t_gc53_dolu_celiskisi();
}

/* ---- G-Ç29 (W5-K4 eki), G-Ç20 (G-Ç53 ikinci çelişki), A4 §3.16 durum eşlemesi ------------------- */

static void t_gc29_geri_alim_zamani_silahli(void)
{
    /* İzin açıkken, kabul edildiği çağrıda izleri bayatlatan örnek (alım zamanı geri gider;
       seq büyük, t_meas ilerlemiş): aynı çağrıda ESTOP kurulu, genişletici olay yok, izin o
       çağrıda kapanır ve FC silahlı ya da izin açık durumda kalmaz (G-Ç29 ölçüt (i); W5-K4
       eki). */
    int64_t T = ka_t_emn_bayat();
    ka_hazir(2, 0u);
    ka_ayar.politika[IDA_EVT_FIRE].gecikme = 30u; /* FIRE'ın sonucu geç gelir; izin açık kalır */
    const ka_olay_t *o = ka_izin_iste(0u, ADIM);
    uint64_t oid = o->e.event_id;
    for (int k = 0; k < 10; ++k) {
        const ka_olay_t *e = ka_olay_kimlikle(oid);
        if (e != NULL && e->teslim_edildi) {
            break;
        }
        (void)ka_adim();
    }
    KA_DOGRULA(ka_model_izin_acik(0u, ka_simdi()), "hazırlık: SAFETY_OPEN(tüp 0) APPLIED alınmadı ya da izin açık değil");
    /* Olağan geri okumalı (PERMIT_OPEN, LOADED alıcı modelinden) inst 0 örneğinin yalnız alım
       zamanı geri alınır. */
    ida_in_t *in = ka_girdi();
    ida_sample_t *s = NULL;
    for (unsigned i = 0; i < in->n_samples; ++i) {
        if (in->samples[i].hdr.type == IDA_SENS_SAFETY && in->samples[i].hdr.inst == 0u) {
            s = &in->samples[i];
        }
    }
    KA_DOGRULA(s != NULL, "düzenek: emniyet örneği yok");
    s->hdr.t_rx.ns = in->now_w.ns - (T + 65 * KA_MS);
    (void)ka_cagir();
    const ida_out_t *out = ka_out();
    KA_DOGRULA((out->fc.inhibit_mask & IDA_FCI_ESTOP) != 0u,
               "izin açıkken bayatlatan örneğin çağrısında ESTOP yok (G-Ç29 ölçüt (i); W5-K4 eki)");
    KA_DOGRULA(!ka_genisletici_olay_var(out), "bayatlatan örneğin çağrısında genişletici olay (G-Ç29, değişmez (a))");
    KA_DOGRULA(ka_kapanma_var(out, 0u), "izin açıkken bayatlatan örneğin çağrısında SAFETY_CLOSE(tüp 0 ya da ALL) yok (G-Ç29)");
    KA_DOGRULA(durum() != IDA_FCS_ARMED && durum() != IDA_FCS_PERMIT_OPEN,
               "bayatlıkta fc.state %s (G-Ç29: FC güvenli duruma)", ka_durum_adi(durum()));
}

static void t_gc20_dolu_celiskisi_reset(void)
{
    /* G-Ç53'ün ikinci çelişkisi (FIRE APPLIED sonrası LOADED = 1) süren nedendir: çelişki
       sürerken FC_RESET kabul edilir ama aynı çağrıda yeniden FAULT_LATCHED olunur; çelişki
       kalkınca FC_RESET kilitten çıkarır (W5-K1, G-Ç20). */
    ka_hazir(2, 0u);
    ka_ayar.loaded_elle = true; /* alıcı LOADED'ı düşürmez */
    (void)atis_dongusu(0u);
    bool kilit = false;
    for (int k = 0; k < 3 && !kilit; ++k) {
        (void)ka_adim();
        kilit = durum() == IDA_FCS_FAULT_LATCHED;
    }
    KA_DOGRULA(kilit, "FIRE APPLIED sonrası LOADED = 1: FAULT_LATCHED bekleniyordu (G-Ç53, W5-K4)");
    for (int k = 0; k < 3; ++k) {
        ida_in_t *in = ka_girdi();
        uint32_t r = ka_fc(in, (uint8_t)IDA_FC_RESET, 0u);
        (void)ka_cagir();
        KA_SONUC(r, IDA_ACK_ACCEPTED, IDA_REASON_NONE, "LOADED çelişkisi sürerken FC_RESET (W5-K1: kabul edilir)");
        KA_DOGRULA(durum() == IDA_FCS_FAULT_LATCHED,
                   "LOADED çelişkisi sürerken FC_RESET sonrası fc.state %s, aynı çağrıda FAULT_LATCHED bekleniyordu (G-Ç20)",
                   ka_durum_adi(durum()));
        KA_DOGRULA(!ka_genisletici_olay_var(ka_out()), "FC_RESET genişletici olay üretti (G-Ç20)");
    }
    ka_ayar.loaded_elle = false; /* geri okuma tüpü boş gösterir: çelişki kalktı */
    ka_adimlar(3);
    KA_DOGRULA(durum() == IDA_FCS_FAULT_LATCHED, "çelişki kalkınca kilit kendiliğinden temizlendi (G-Ç19)");
    ida_in_t *in = ka_girdi();
    uint32_t r = ka_fc(in, (uint8_t)IDA_FC_RESET, 0u);
    (void)ka_cagir();
    KA_SONUC(r, IDA_ACK_ACCEPTED, IDA_REASON_NONE, "çelişki kalkınca FC_RESET");
    KA_DOGRULA(durum() == IDA_FCS_SAFE, "çelişki kalkınca FC_RESET sonrası fc.state %s, SAFE bekleniyordu (G-Ç20)",
               ka_durum_adi(durum()));
    /* "Silahlandırmaz" atış kontrol durumunu söyler (fc.state SAFE; ARMED değil); aracın armed
       bayrağı (ARM / DISARM, A4 §3.17 ARM satırı) hazırlıkta kurulmuştur ve G-Ç20'nin konusu
       değildir. */
    KA_DOGRULA(!ka_genisletici_olay_var(ka_out()), "FC_RESET izin açtı (G-Ç20)");
}

static void t_gc02_durum_eslemesi(void)
{
    /* A4 §3.16: FIRE_PENDING yalnız sonucu beklenen FIRE varken; ARMED yalnız açık izin yokken.
       FIRE'ın sonucu geç verilir; APPLIED geldikten sonra izin (hold_ns) dolana dek fc.state ne
       ARMED ne FIRE_PENDING olabilir. (İki eşleme her çağrıda düzenekte de denetlenir.) */
    ka_hazir(2, 0u);
    ka_ayar.politika[IDA_EVT_FIRE].gecikme = 6u;
    const ka_olay_t *f = atis_dongusu(0u);
    uint64_t fid = f->e.event_id;
    bool bekleme_goruldu = false;
    bool pencere_goruldu = false;
    for (int k = 0; k < 120; ++k) {
        const ka_olay_t *e = ka_olay_kimlikle(fid);
        bool bekliyor = e != NULL && !e->teslim_edildi;
        uint8_t st = durum();
        bool izin = ka_model_izin_acik(0u, ka_simdi());
        if (bekliyor) {
            bekleme_goruldu = true;
        }
        KA_DOGRULA(st != IDA_FCS_FIRE_PENDING || bekliyor, "FIRE'ın sonucu alındıktan sonra fc.state FIRE_PENDING (A4 §3.16)");
        KA_DOGRULA(st != IDA_FCS_ARMED || !izin, "tüp 0'ın izni açıkken fc.state ARMED (A4 §3.16)");
        if (!bekliyor && e != NULL && e->sonuc == IDA_EVR_APPLIED && izin) {
            pencere_goruldu = true;
            KA_DOGRULA(st != IDA_FCS_ARMED && st != IDA_FCS_FIRE_PENDING,
                       "FIRE APPLIED sonrası izin süresi dolmadan fc.state %s (A4 §3.16)", ka_durum_adi(st));
        }
        (void)ka_adim();
    }
    KA_DOGRULA(bekleme_goruldu && pencere_goruldu,
               "senaryo kapsanmadı: FIRE sonucu beklenmesi %d, APPLIED sonrası açık izin penceresi %d", (int)bekleme_goruldu,
               (int)pencere_goruldu);
}

static const ka_test_t TESTLER[] = {
    {"izin_yalniz_fc_fire", t_gc47_izin_yalniz_fc_fire},
    {"fire_baglantisi", t_gc21_fire_baglantisi},
    {"pencere_sonu", t_gc48_pencere_sonu},
    {"izin_suresi", t_gc11_izin_suresi},
    {"bekleyen_sonuc", t_gc40_bekleyen_sonuc},
    {"yenileme", t_gc12_yenileme},
    {"izin_alanlari", t_gc10_izin_alanlari},
    {"izin_sonucu", t_gc23_izin_sonucu},
    {"fire_sonucu", t_gc49_fire_sonucu},
    {"dolu_celiskisi", t_gc53_dolu_celiskisi},
    {"izin_yok_imu_lag", t_gc35_izin_yok},
    {"yalniz_step_ok", t_gc07_yalniz_step_ok},
    {"durum_kosulu", t_gc22_durum_kosulu},
    {"fc_fire_tup_engelleri", t_gc05_fc_fire_tup_engelleri},
    {"test_kipi", t_gc24_test_kipi},
    {"f15_test_kipi", t_f15_test_kipi},
    {"testten_cikis", t_gc51_testten_cikis},
    {"diger_tup", t_gc16_diger_tup},
    {"butun_tuplere", t_gc32_butun_tuplere},
    {"daraltma_etkisi", t_gc06_etki},
    {"hb_kaybi", t_gc25_hb_kaybi},
    {"f1_silahli", t_f1_silahli},
    {"f3_kopma", t_f3_kopma},
    {"f8_acil_durdurma_atista", t_f8_acil_durdurma_atista},
    {"f11_revizyon", t_f11_revizyon},
    {"f12_tek_cikis", t_f12_tek_cikis},
    {"f7_silahli", t_f7_silahli},
    {"geri_alim_zamani_silahli", t_gc29_geri_alim_zamani_silahli},
    {"dolu_celiskisi_reset", t_gc20_dolu_celiskisi_reset},
    {"durum_eslemesi", t_gc02_durum_eslemesi},
};

int main(int argc, char **argv)
{
    return ka_main(argc, argv, __FILE__, TESTLER, sizeof TESTLER / sizeof TESTLER[0]);
}
