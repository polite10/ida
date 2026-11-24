/*
 * tests/kabul/atis — emniyet girdileri: etkin değer (A4-K116), kapanma ve günlük, oturum sonu
 * ve başlangıcı, bayatlık (G-Ç29, F14), geri okuma çelişkisi (G-Ç53, F7).
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

static uint8_t tup_eksik(uint8_t bit)
{
    return (uint8_t)(KC_TUP_ACIK & (uint8_t)~bit);
}

static bool engel(uint32_t bit)
{
    return (ka_out()->fc.inhibit_mask & bit) != 0u;
}

#define KA_EMNIYET_BITLERI                                                                                    \
    (IDA_FCI_ESTOP | IDA_FCI_PLUG_T0 | IDA_FCI_PLUG_T1 | IDA_FCI_PIN_T0 | IDA_FCI_PIN_T1)

static void oturum_sonu_cagrisi(void)
{
    ida_in_t *in = ka_girdi();
    in->plat.link_event = (uint8_t)IDA_LINKEV_SESSION_END;
    in->plat.link_state = (uint8_t)IDA_LINK_NONE;
    (void)ka_cagir();
}

/* ---- G-Ç14: etkin değer ----------------------------------------------------------------- */

static void t_gc14_etkin_deger(void)
{
    ka_kur(2);
    ka_oturum_baslat();
    gevsek();
    (void)ka_adim();
    KA_DOGRULA(!engel(IDA_FCI_ESTOP), "(c) kabul edilen gevşek inst 0 örneği acil durdurmayı gevşetmedi (G-Ç14)");
    /* (e) VALID = 0 kısıtlayan örnek: reddedilir ama kısıtlar. */
    ka_ayar.emniyet = false;
    ida_in_t *in = ka_girdi();
    ida_sample_t *s = ka_emniyet_ekle(in, 0u, 1u, KC_TUP_ACIK, KC_TUP_ACIK);
    s->hdr.flags = 0u;
    (void)ka_cagir();
    KA_DOGRULA(engel(IDA_FCI_ESTOP), "(e) VALID = 0, estop = 1 örneği kısıtlamadı (G-Ç14; A4-K130)");
    /* (a) VALID = 0 gevşeten örnek gevşetmez. */
    in = ka_girdi();
    s = ka_emniyet_ekle(in, 0u, 0u, KC_TUP_ACIK, KC_TUP_ACIK);
    s->hdr.flags = 0u;
    (void)ka_cagir();
    KA_DOGRULA(engel(IDA_FCI_ESTOP), "(a) VALID = 0, estop = 0 örneği gevşetti (G-Ç14)");
    /* (b) inst 1 gevşetmez. */
    in = ka_girdi();
    (void)ka_emniyet_ekle(in, 1u, 0u, KC_TUP_ACIK, KC_TUP_ACIK);
    (void)ka_cagir();
    KA_DOGRULA(engel(IDA_FCI_ESTOP), "(b) inst 1'in estop = 0'ı gevşetti (G-Ç14; A4-K116: tek kaynak inst 0)");
    /* (c) inst 0 gevşetir. */
    in = ka_girdi();
    (void)ka_emniyet_ekle(in, 0u, 0u, KC_TUP_ACIK, KC_TUP_ACIK);
    (void)ka_cagir();
    KA_DOGRULA(!engel(IDA_FCI_ESTOP), "(c) inst 0'ın estop = 0'ı gevşetmedi (G-Ç14)");
    /* (d) aynı çağrıda kabul edilen gevşek inst 0 + kısıtlayan inst 1: kısıtlayan. */
    in = ka_girdi();
    (void)ka_emniyet_ekle(in, 0u, 0u, KC_TUP_ACIK, KC_TUP_ACIK);
    (void)ka_emniyet_ekle(in, 1u, 1u, KC_TUP_ACIK, tup_eksik(IDA_TUBE_PLUG_IN));
    (void)ka_cagir();
    KA_DOGRULA(engel(IDA_FCI_ESTOP) && engel(IDA_FCI_PLUG_T1), "(d) inst 1'in kısıtlayan içeriği uygulanmadı (G-Ç14)");
    /* (f) reddedilen çağrıda gevşeten örnek gevşetmez; kısıtlayan kısıtlar. */
    in = ka_girdi_seq(kc_son_seq());
    (void)ka_emniyet_ekle(in, 0u, 0u, KC_TUP_ACIK, KC_TUP_ACIK);
    (void)ka_cagir();
    KA_DOGRULA(engel(IDA_FCI_ESTOP) && engel(IDA_FCI_PLUG_T1), "(f) reddedilen çağrıdaki örnek gevşetti (G-Ç14)");
    in = ka_girdi();
    (void)ka_emniyet_ekle(in, 0u, 0u, KC_TUP_ACIK, KC_TUP_ACIK);
    (void)ka_cagir();
    KA_DOGRULA(!engel(IDA_FCI_ESTOP) && !engel(IDA_FCI_PLUG_T1), "kabul edilen gevşek örnek gevşetmedi (G-Ç14)");
    in = ka_girdi_seq(kc_son_seq());
    (void)ka_emniyet_ekle(in, 0u, 0u, tup_eksik(IDA_TUBE_PIN_OUT), KC_TUP_ACIK);
    (void)ka_cagir();
    KA_DOGRULA(engel(IDA_FCI_PIN_T0), "(f) reddedilen çağrıdaki kısıtlayan örnek (pim) kısıtlamadı (G-Ç14; A4-K113 (b))");
    in = ka_girdi();
    (void)ka_emniyet_ekle(in, 0u, 0u, KC_TUP_ACIK, KC_TUP_ACIK);
    (void)ka_cagir();
    KA_DOGRULA(!engel(KA_EMNIYET_BITLERI), "gevşek örnek gevşetmedi");
    /* (g) estop = 2: reddedilir, kısıtlar. */
    uint32_t red0 = ka_out()->status.cnt.sample_rejected;
    in = ka_girdi();
    (void)ka_emniyet_ekle(in, 0u, 2u, KC_TUP_ACIK, KC_TUP_ACIK);
    (void)ka_cagir();
    KA_DOGRULA(engel(IDA_FCI_ESTOP), "(g) estop = 2 örneğinin kısıtlayıcı içeriği uygulanmadı (G-Ç14; A4-K116)");
    KA_DOGRULA(ka_out()->status.cnt.sample_rejected - red0 >= 1u, "(g) estop = 2 örneği reddedilmedi (A4-K33)");
    /* (h) tanımsız tüp biti: reddedilir, kısıtlayan (estop) bu örnekte yok, gevşetmez. */
    in = ka_girdi();
    (void)ka_emniyet_ekle(in, 0u, 0u, (uint8_t)(KC_TUP_ACIK | 0x10u), KC_TUP_ACIK);
    (void)ka_cagir();
    KA_DOGRULA(engel(IDA_FCI_ESTOP), "(h) tanımsız tüp bitli (reddedilen) örnek gevşetti (G-Ç14)");
    in = ka_girdi();
    (void)ka_emniyet_ekle(in, 0u, 0u, KC_TUP_ACIK, KC_TUP_ACIK);
    (void)ka_cagir();
    KA_DOGRULA(!engel(KA_EMNIYET_BITLERI), "gevşek örnek gevşetmedi");
    /* (i) eşit anahtarlı çelişkili çift: alan bu çağrıda kısıtlayan. */
    in = ka_girdi();
    s = ka_emniyet_ekle(in, 0u, 0u, KC_TUP_ACIK, KC_TUP_ACIK);
    ida_sample_t *u = kc_ornek_ekle(in, (uint8_t)IDA_SENS_SAFETY, 0u, s->hdr.seq, s->hdr.t_meas.ns);
    *u = *s;
    u->u.safety.estop = 1u;
    (void)ka_cagir();
    KA_DOGRULA(engel(IDA_FCI_ESTOP), "(i) çelişkili çiftte acil durdurma kısıtlayan değil (G-Ç14; A4-K115)");
    KA_DOGRULA(ka_out()->health.sample_conflict, "(i) çelişki bildirilmedi (A4-K115)");
    /* (j) eşit t_meas, büyük seq: kabul edilir ve değeri günceller (A4-K34). */
    in = ka_girdi();
    s = ka_emniyet_ekle(in, 0u, 0u, KC_TUP_ACIK, KC_TUP_ACIK);
    int64_t tm = s->hdr.t_meas.ns;
    (void)ka_cagir();
    KA_DOGRULA(!engel(IDA_FCI_ESTOP), "gevşek örnek gevşetmedi");
    in = ka_girdi();
    s = ka_emniyet_ekle(in, 0u, 1u, KC_TUP_ACIK, KC_TUP_ACIK);
    s->hdr.t_meas.ns = tm;
    (void)ka_cagir();
    KA_DOGRULA(engel(IDA_FCI_ESTOP), "(j) eşit t_meas'li büyük seq'li estop = 1 örneği uygulanmadı (A4-K34)");
    in = ka_girdi();
    s = ka_emniyet_ekle(in, 0u, 0u, KC_TUP_ACIK, KC_TUP_ACIK);
    s->hdr.t_meas.ns = tm;
    (void)ka_cagir();
    KA_DOGRULA(!engel(IDA_FCI_ESTOP), "(j) eşit t_meas'li büyük seq'li gevşek örnek kabul edilip gevşetmedi (A4-K34)");
}

/* ---- G-Ç15 -------------------------------------------------------------------------------------- */

static int32_t etkin_tup_bitleri(bool fis0, bool pim0, bool fis1, bool pim1)
{
    return (int32_t)((fis0 ? 0u : 1u) | (pim0 ? 0u : 2u) | (fis1 ? 0u : 4u) | (pim1 ? 0u : 8u));
}

static void gunluk_bekle(int32_t estop, int32_t tup, const char *ne)
{
    const ida_out_t *o = ka_out();
    if (o->status.cnt.log_dropped != ka_onceki()->status.cnt.log_dropped) {
        return; /* günlük düştüyse sayılamaz (A4-K90) */
    }
    KA_DOGRULA(kc_gunluk_say(o, (uint16_t)IDA_LOG_SAFETY_INPUT) == 1, "%s: SAFETY_INPUT günlüğü sayısı %d, 1 bekleniyordu (G-Ç15)", ne,
               kc_gunluk_say(o, (uint16_t)IDA_LOG_SAFETY_INPUT));
    const ida_log_t *l = kc_gunluk_bul(o, (uint16_t)IDA_LOG_SAFETY_INPUT);
    KA_DOGRULA(l->arg0 == estop && l->arg1 == tup, "%s: SAFETY_INPUT(%ld, 0x%lx), beklenen (%ld, 0x%lx) (3.15 satır 19)", ne,
               (long)l->arg0, (long)l->arg1, (long)estop, (long)tup);
}

static void t_gc15_kapanma_bir_kez(void)
{
    ka_kur(2);
    ka_oturum_baslat();
    gevsek();
    ka_adimlar(2);
    uint32_t nfr0 = ka_out()->status.cnt.narrow_from_rejected;
    /* Acil durdurma: tek ALL + günlük; sürerken yeni olay yok. */
    ka_ayar.estop = 1u;
    (void)ka_adim();
    KA_DOGRULA(ka_out()->n_events == 1u && ka_olay_say(ka_out(), (uint8_t)IDA_EVT_SAFETY_CLOSE, IDA_TUBE_ALL) == 1,
               "acil durdurmada tek SAFETY_CLOSE(ALL) bekleniyordu, %u olay (G-Ç15)", (unsigned)ka_out()->n_events);
    gunluk_bekle(1, etkin_tup_bitleri(false, false, false, false), "acil durdurma");
    KA_DOGRULA(ka_out()->status.cnt.narrow_from_rejected == nfr0, "kabul edilen örnekten dönüşte narrow_from_rejected arttı (G-Ç15)");
    for (int k = 0; k < 5; ++k) {
        (void)ka_adim();
        KA_DOGRULA(ka_out()->n_events == 0u, "acil durdurma sürerken yeni olay (G-Ç15)");
        KA_DOGRULA(kc_gunluk_say(ka_out(), (uint16_t)IDA_LOG_SAFETY_INPUT) == 0, "değer değişmeden SAFETY_INPUT günlüğü (G-Ç15)");
    }
    gevsek();
    (void)ka_adim();
    KA_DOGRULA(ka_out()->n_events == 0u, "gevşemede olay (G-Ç15: kapanma yalnız kısıtlayana dönüşte)");
    gunluk_bekle(0, etkin_tup_bitleri(false, false, false, false), "gevşeme");
    /* Tüp 0 fişi: SAFETY_CLOSE(0). */
    ka_ayar.tup[0] = tup_eksik(IDA_TUBE_PLUG_IN);
    (void)ka_adim();
    KA_DOGRULA(ka_out()->n_events == 1u && ka_olay_say(ka_out(), (uint8_t)IDA_EVT_SAFETY_CLOSE, 0) == 1,
               "tüp 0 fişi çıkınca tek SAFETY_CLOSE(0) bekleniyordu, %u olay (G-Ç15)", (unsigned)ka_out()->n_events);
    gunluk_bekle(0, etkin_tup_bitleri(true, false, false, false), "tüp 0 fişi");
    /* Tüp 1 pimi: SAFETY_CLOSE(1). */
    ka_ayar.tup[1] = tup_eksik(IDA_TUBE_PIN_OUT);
    (void)ka_adim();
    KA_DOGRULA(ka_out()->n_events == 1u && ka_olay_say(ka_out(), (uint8_t)IDA_EVT_SAFETY_CLOSE, 1) == 1,
               "tüp 1 pimi takılınca tek SAFETY_CLOSE(1) bekleniyordu, %u olay (G-Ç15)", (unsigned)ka_out()->n_events);
    gunluk_bekle(0, etkin_tup_bitleri(true, false, false, true), "tüp 1 pimi");
    /* Yalnız reddedilen örnekten dönüş: narrow_from_rejected +1. */
    gevsek();
    ka_adimlar(2);
    nfr0 = ka_out()->status.cnt.narrow_from_rejected;
    ka_ayar.emniyet = false;
    ida_in_t *in = ka_girdi();
    ida_sample_t *s = ka_emniyet_ekle(in, 0u, 1u, KC_TUP_ACIK, KC_TUP_ACIK);
    s->hdr.flags = 0u;
    (void)ka_cagir();
    KA_DOGRULA(ka_kapanma_var(ka_out(), 0u), "reddedilen örnekten acil durdurmada SAFETY_CLOSE yok (G-Ç15)");
    KA_DOGRULA(ka_out()->status.cnt.narrow_from_rejected == nfr0 + 1u,
               "yalnız reddedilen örnekten dönüşte narrow_from_rejected %lu → %lu (G-Ç15)", (unsigned long)nfr0,
               (unsigned long)ka_out()->status.cnt.narrow_from_rejected);
}

/* ---- G-Ç18, G-Ç44, G-Ç45 --------------------------------------------------------------------- */

static void oturum_sonu_denetle(const char *ne)
{
    const ida_out_t *o = ka_out();
    KA_DOGRULA(!o->armed, "%s: SESSION_END çağrısında armed = 1 (G-Ç18)", ne);
    KA_DOGRULA((o->fc.inhibit_mask & KA_EMNIYET_BITLERI) == KA_EMNIYET_BITLERI,
               "%s: SESSION_END'de etkin emniyet değerleri kısıtlayana dönmedi (inhibit 0x%05lx; G-Ç18)", ne,
               (unsigned long)o->fc.inhibit_mask);
    KA_DOGRULA(o->n_events == 1u && ka_olay_say(o, (uint8_t)IDA_EVT_SAFETY_CLOSE, IDA_TUBE_ALL) == 1,
               "%s: tek SAFETY_CLOSE(ALL) bekleniyordu, %u olay (G-Ç18)", ne, (unsigned)o->n_events);
    KA_DOGRULA(o->fc.state == IDA_FCS_SAFE, "%s: SESSION_END sonrası fc.state %s (G-Ç18)", ne, ka_durum_adi(o->fc.state));
}

static void t_gc18_oturum_sonu(void)
{
    ka_kur(2);
    ka_oturum_ac();
    ka_arac_silahla();
    gevsek();
    ka_adimlar(2);
    oturum_sonu_cagrisi();
    oturum_sonu_denetle("silahlı ve gevşek");
    /* Değerler zaten kısıtlayanken de ilk SESSION_END tek ALL üretir. */
    ka_ayar.link_state = (uint8_t)IDA_LINK_RUNNING;
    ka_oturum_baslat();
    ka_ayar.estop = 1u;
    ka_ayar.tup[0] = tup_eksik(IDA_TUBE_PLUG_IN);
    ka_ayar.tup[1] = tup_eksik(IDA_TUBE_PIN_OUT);
    ka_adimlar(3);
    oturum_sonu_cagrisi();
    oturum_sonu_denetle("değerler zaten kısıtlayan");
}

static void t_gc44_ikinci_oturum_sonu(void)
{
    ka_kur(2);
    ka_oturum_baslat();
    gevsek();
    ka_adimlar(2);
    oturum_sonu_cagrisi();
    KA_DOGRULA(ka_out()->n_events == 1u, "ilk SESSION_END'de %u olay", (unsigned)ka_out()->n_events);
    ka_ayar.link_state = (uint8_t)IDA_LINK_NONE;
    ka_ayar.emniyet = false;
    ka_adimlar(2);
    oturum_sonu_cagrisi();
    KA_DOGRULA(ka_out()->n_events == 0u, "arada SESSION_START olmadan ikinci SESSION_END %u olay üretti (G-Ç44)",
               (unsigned)ka_out()->n_events);
    /* Arada gevşetilen alan: ikinci SESSION_END tek ALL üretir. */
    ka_ayar.emniyet = true;
    gevsek();
    ka_adimlar(2);
    KA_DOGRULA(!engel(IDA_FCI_ESTOP), "oturumsuz çağrıda kabul edilen gevşek örnek gevşetmedi");
    oturum_sonu_cagrisi();
    KA_DOGRULA(ka_out()->n_events == 1u && ka_olay_say(ka_out(), (uint8_t)IDA_EVT_SAFETY_CLOSE, IDA_TUBE_ALL) == 1,
               "arada gevşetilen alanla ikinci SESSION_END: tek SAFETY_CLOSE(ALL) bekleniyordu, %u olay (G-Ç44)",
               (unsigned)ka_out()->n_events);
}

static void t_gc45_oturum_baslangici(void)
{
    ka_kur(2);
    ka_oturum_ac();
    ka_arac_silahla();
    gevsek();
    ka_adimlar(2);
    /* (1) koşan oturumda SESSION_START: silahlandırma, etkin değerler ve fc.state değişmez. */
    uint32_t maske = ka_out()->fc.inhibit_mask & KA_EMNIYET_BITLERI;
    uint8_t durum = ka_out()->fc.state;
    ida_in_t *in = ka_girdi();
    in->plat.link_event = (uint8_t)IDA_LINKEV_SESSION_START;
    (void)ka_cagir();
    KA_DOGRULA(ka_out()->armed, "SESSION_START silahlandırmayı kaldırdı (G-Ç45)");
    KA_DOGRULA((ka_out()->fc.inhibit_mask & KA_EMNIYET_BITLERI) == maske, "SESSION_START etkin emniyet değerlerini değiştirdi (G-Ç45)");
    KA_DOGRULA(ka_out()->fc.state == durum, "SESSION_START fc.state'i değiştirdi (G-Ç45)");
    KA_DOGRULA(ka_out()->n_events == 0u, "SESSION_START kapanma üretti (G-Ç45)");
    /* (2) SESSION_END → SESSION_START: kısıtlayan değerler kalır, yeni ARM gerekir. */
    oturum_sonu_cagrisi();
    ka_ayar.emniyet = false;
    in = ka_girdi();
    in->plat.link_event = (uint8_t)IDA_LINKEV_SESSION_START;
    (void)ka_cagir();
    KA_DOGRULA((ka_out()->fc.inhibit_mask & KA_EMNIYET_BITLERI) == KA_EMNIYET_BITLERI,
               "SESSION_START kısıtlayan etkin değerleri gevşetti (G-Ç45)");
    KA_DOGRULA(ka_out()->n_events == 0u && !ka_out()->armed, "SESSION_START olay üretti ya da silahlandırdı (G-Ç45)");
    ka_adimlar(50);
    KA_DOGRULA(!ka_out()->armed, "yeni oturumda ARM olmadan armed = 1 (G-Ç45)");
}

/* ---- G-Ç18, G-Ç44, G-Ç45: A4-K122 oturum sonu çağrısıyla -------------------------------------------
 * Yukarıdaki üç test SESSION_END'i aynı çağrıda gevşek bir inst 0 emniyet örneği de taşıyan bir
 * step'le verir; o çağrıda örneğin gevşetip gevşetmeyeceğini sözleşme açıkça sıralamaz (mimar
 * kararı bekleniyor). Aşağıdakiler aynı maddeleri platformun gerçekte yaptığı oturum sonu
 * çağrısıyla (A4-K122, G-K08: tick, tf yok, örnek ve komut yok) sınar. */

static void oturum_sonu_k122(void)
{
    (void)ka_girdi_oturum_sonu();
    (void)ka_cagir();
}

static void t_gc18_oturum_sonu_k122(void)
{
    ka_kur(2);
    ka_oturum_ac();
    ka_arac_silahla();
    gevsek();
    ka_adimlar(2);
    KA_DOGRULA((ka_out()->fc.inhibit_mask & KA_EMNIYET_BITLERI) == 0u, "hazırlık: emniyet alanları gevşemedi (inhibit 0x%05lx)",
               (unsigned long)ka_out()->fc.inhibit_mask);
    oturum_sonu_k122();
    oturum_sonu_denetle("A4-K122 çağrısı, silahlı ve gevşek");
    /* Dönüş kalıcıdır: örneksiz sonraki çağrıda da kısıtlayan. */
    ka_ayar.link_state = (uint8_t)IDA_LINK_NONE;
    ka_ayar.emniyet = false;
    (void)ka_adim();
    KA_DOGRULA((ka_out()->fc.inhibit_mask & KA_EMNIYET_BITLERI) == KA_EMNIYET_BITLERI,
               "SESSION_END'den sonraki örneksiz çağrıda etkin değerler gevşek (inhibit 0x%05lx; G-Ç18, A4-K116)",
               (unsigned long)ka_out()->fc.inhibit_mask);
    /* Değerler zaten kısıtlayanken de ilk SESSION_END tek ALL üretir. */
    ka_ayar.link_state = (uint8_t)IDA_LINK_RUNNING;
    ka_ayar.emniyet = true;
    ka_oturum_baslat();
    ka_ayar.estop = 1u;
    ka_ayar.tup[0] = tup_eksik(IDA_TUBE_PLUG_IN);
    ka_ayar.tup[1] = tup_eksik(IDA_TUBE_PIN_OUT);
    ka_adimlar(3);
    oturum_sonu_k122();
    oturum_sonu_denetle("A4-K122 çağrısı, değerler zaten kısıtlayan");
}

static void t_gc44_ikinci_oturum_sonu_k122(void)
{
    ka_kur(2);
    ka_oturum_baslat();
    gevsek();
    ka_adimlar(2);
    oturum_sonu_k122();
    KA_DOGRULA(ka_out()->n_events == 1u && ka_olay_say(ka_out(), (uint8_t)IDA_EVT_SAFETY_CLOSE, IDA_TUBE_ALL) == 1,
               "ilk SESSION_END'de tek SAFETY_CLOSE(ALL) bekleniyordu, %u olay (G-Ç18)", (unsigned)ka_out()->n_events);
    ka_ayar.link_state = (uint8_t)IDA_LINK_NONE;
    ka_ayar.emniyet = false;
    ka_adimlar(2);
    oturum_sonu_k122();
    KA_DOGRULA(ka_out()->n_events == 0u, "arada SESSION_START olmadan ikinci SESSION_END %u olay üretti (G-Ç44)",
               (unsigned)ka_out()->n_events);
    /* Arada gevşetilen alan: sonraki SESSION_END tek ALL üretir. */
    ka_ayar.emniyet = true;
    gevsek();
    ka_adimlar(2);
    KA_DOGRULA(!engel(IDA_FCI_ESTOP), "oturumsuz çağrıda kabul edilen gevşek örnek gevşetmedi (A4-K116)");
    oturum_sonu_k122();
    KA_DOGRULA(ka_out()->n_events == 1u && ka_olay_say(ka_out(), (uint8_t)IDA_EVT_SAFETY_CLOSE, IDA_TUBE_ALL) == 1,
               "arada gevşetilen alanla sonraki SESSION_END: tek SAFETY_CLOSE(ALL) bekleniyordu, %u olay (G-Ç44)",
               (unsigned)ka_out()->n_events);
}

static void t_gc45_oturum_baslangici_k122(void)
{
    ka_kur(2);
    ka_oturum_ac();
    ka_arac_silahla();
    gevsek();
    ka_adimlar(2);
    oturum_sonu_k122();
    ka_ayar.emniyet = false;
    ida_in_t *in = ka_girdi();
    in->plat.link_event = (uint8_t)IDA_LINKEV_SESSION_START;
    (void)ka_cagir();
    KA_DOGRULA((ka_out()->fc.inhibit_mask & KA_EMNIYET_BITLERI) == KA_EMNIYET_BITLERI,
               "SESSION_START kısıtlayan etkin değerleri gevşetti (inhibit 0x%05lx; G-Ç45)", (unsigned long)ka_out()->fc.inhibit_mask);
    KA_DOGRULA(ka_out()->n_events == 0u && !ka_out()->armed, "SESSION_START olay üretti ya da silahlandırdı (G-Ç45)");
    ka_adimlar(50);
    KA_DOGRULA(!ka_out()->armed, "yeni oturumda ARM olmadan armed = 1 (G-Ç45)");
    KA_DOGRULA((ka_out()->fc.inhibit_mask & KA_EMNIYET_BITLERI) == KA_EMNIYET_BITLERI,
               "örneksiz yeni oturumda etkin değerler gevşedi (G-Ç45, A4-K116)");
}

/* ---- G-Ç29, F14: bayatlık ----------------------------------------------------------------------- */

static void bayatlik(bool olcum, const char *ne)
{
    int64_t T = ka_t_emn_bayat();
    ka_kur(2);
    ka_oturum_baslat();
    gevsek();
    ka_ayar.emniyet_aralik = 100 * KA_MS; /* T_emniyet (A1-K56) */
    for (int k = 0; k < 200; ++k) {
        (void)ka_adim();
        KA_DOGRULA(!engel(IDA_FCI_ESTOP), "%s: 100 ms aralıklı gevşek örneklerle ESTOP kuruldu (yanlış bayatlık; W5-K4)", ne);
    }
    /* Son örneğin alım anı: bir sonraki örneğin hemen öncesine kadar ilerle, sonra kes. */
    int64_t son = -1;
    for (int k = 0; k < 25; ++k) {
        size_t n_once = ka_out()->status.cnt.sample_rejected;
        (void)n_once;
        ida_in_t *in = ka_girdi();
        bool var = false;
        for (unsigned i = 0; i < in->n_samples; ++i) {
            if (in->samples[i].hdr.type == IDA_SENS_SAFETY) {
                var = true;
            }
        }
        (void)ka_cagir();
        if (var) {
            son = ka_simdi();
            break;
        }
    }
    KA_DOGRULA(son > 0, "emniyet örneği gönderilmedi");
    if (olcum) {
        ka_ayar.tmeas_dondur = true; /* t_meas donar, seq ve t_rx yenilenir */
        ka_ayar.emniyet_aralik = 0;
    } else {
        ka_ayar.emniyet = false;
    }
    bool kuruldu = false;
    for (int k = 0; k < 120 && !kuruldu; ++k) {
        (void)ka_adim();
        int64_t gecen = ka_simdi() - son;
        if (engel(IDA_FCI_ESTOP)) {
            kuruldu = true;
            KA_DOGRULA(gecen > T - KA_P, "%s: %lld ms'de (T_emn_bayat %lld ms) ESTOP kuruldu (G-Ç29: süre dolmadan)", ne,
                       (long long)(gecen / KA_MS), (long long)(T / KA_MS));
            /* Kurulduğu çağrıda tek SAFETY_CLOSE(ALL); FC güvenli (G-Ç29, F14). */
            KA_DOGRULA(ka_olay_say(ka_out(), (uint8_t)IDA_EVT_SAFETY_CLOSE, IDA_TUBE_ALL) == 1,
                       "%s: ESTOP'un kurulduğu çağrıda SAFETY_CLOSE(ALL) yok (G-Ç29, F14)", ne);
            KA_DOGRULA(ka_out()->fc.state == IDA_FCS_SAFE, "%s: bayatlıkta fc.state %s (G-Ç29)", ne,
                       ka_durum_adi(ka_out()->fc.state));
        } else {
            KA_DOGRULA(gecen <= T, "%s: %lld ms'de (T_emn_bayat %lld ms aşıldı) ESTOP yok (G-Ç29, %s)", ne,
                       (long long)(gecen / KA_MS), (long long)(T / KA_MS), olcum ? "ölçüt (ii)" : "ölçüt (i)");
            KA_DOGRULA(ka_out()->n_events == 0u, "%s: bayatlık dolmadan olay", ne);
        }
    }
    KA_DOGRULA(kuruldu, "%s: bayatlık sınanamadı", ne);
    ka_adimlar(10);
    KA_DOGRULA(engel(IDA_FCI_ESTOP), "%s: bayatlık sürerken ESTOP kalktı", ne);
    /* Taze örnekle gevşer. */
    ka_ayar.tmeas_dondur = false;
    ka_ayar.emniyet = true;
    ka_ayar.emniyet_aralik = 0;
    (void)ka_adim();
    KA_DOGRULA(!engel(IDA_FCI_ESTOP), "%s: taze gevşek örnekle ESTOP kalkmadı", ne);
}

static void t_gc29_bayatlik_alim(void)
{
    bayatlik(false, "alım ölçütü");
}

static void t_gc29_bayatlik_olcum(void)
{
    bayatlik(true, "ölçüm zamanı ölçütü");
}

static void t_f14_akis_kesilmesi(void)
{
    bayatlik(false, "F14 akış kesilmesi");
}

static void t_f14_olcum_donmasi(void)
{
    bayatlik(true, "F14 t_meas donması");
}

/* G-Ç29 ölçüt (i), W5-K4 eki: kabul edildiği çağrıda izleri bayatlatan örnek. Alım zamanı geri
   giden (t_rx < önceki örneğinki) ama seq'i büyük, t_meas'i ilerlemiş gevşek örnek A4-K33'e göre
   kabul edilir (alım zamanının artması koşul değildir); kabulden sonra "son kabul edilen inst 0
   örneğinin t_rx'i" bu örneğinkidir ve now − t_rx > T_emn_bayat: acil durdurma AYNI çağrıda
   basılı sayılır. Örnek: now 2000 ms'de taze örnek, now 2015 ms'de t_rx = 1700 ms'lik örnek
   (T_emn_bayat 250 ms). */
static void t_gc29_geri_alim_zamani(void)
{
    int64_t T = ka_t_emn_bayat();
    ka_kur(2);
    ka_oturum_baslat();
    gevsek();
    int64_t hedef = KC_T0 + 1000 * KA_MS; /* 2000 ms */
    while (ka_simdi() + KA_P < hedef) {
        (void)ka_adim();
    }
    (void)ka_adim();
    KA_DOGRULA(ka_simdi() == hedef, "zamanlama: now %lld ms, 2000 ms bekleniyordu", (long long)(ka_simdi() / KA_MS));
    KA_DOGRULA(!engel(IDA_FCI_ESTOP), "taze gevşek örnekle ESTOP kurulu (G-Ç14)");
    ka_ayar.emniyet = false;
    ka_adimlar(2); /* 2005, 2010 ms: örnek yok, son alım 10 ms önce */
    KA_DOGRULA(!engel(IDA_FCI_ESTOP), "son örnekten 10 ms sonra ESTOP (G-Ç29: T_emn_bayat dolmadı)");
    uint32_t red0 = ka_out()->status.cnt.sample_rejected;
    uint32_t fut0 = ka_out()->status.cnt.sample_future;
    ida_in_t *in = ka_girdi(); /* 2015 ms */
    ida_sample_t *s = ka_emniyet_ekle(in, 0u, 0u, KC_TUP_ACIK, KC_TUP_ACIK); /* seq büyük, t_meas = 2015 ms */
    s->hdr.t_rx.ns = in->now_w.ns - (T + 65 * KA_MS);                          /* 1700 ms (T = 250 ms) */
    (void)ka_cagir();
    const ida_out_t *o = ka_out();
    KA_DOGRULA(o->status.cnt.sample_rejected == red0 && o->status.cnt.sample_future == fut0,
               "geri giden t_rx'li örnek reddedildi (A4-K33'te alım zamanının artması koşul değil; sample_rejected %lu → %lu)",
               (unsigned long)red0, (unsigned long)o->status.cnt.sample_rejected);
    KA_DOGRULA((o->fc.inhibit_mask & IDA_FCI_ESTOP) != 0u,
               "kabul edilen örneğin t_rx'inden %lld ms geçmişken (T_emn_bayat %lld ms) aynı çağrıda ESTOP yok "
               "(G-Ç29 ölçüt (i); W5-K4 eki)",
               (long long)((T + 65 * KA_MS) / KA_MS), (long long)(T / KA_MS));
    KA_DOGRULA(!ka_genisletici_olay_var(o), "bayatlatan örneğin çağrısında genişletici olay (G-Ç29, değişmez (a))");
    KA_DOGRULA(ka_olay_say(o, (uint8_t)IDA_EVT_SAFETY_CLOSE, IDA_TUBE_ALL) == 1,
               "bayatlatan örneğin çağrısında SAFETY_CLOSE(ALL) yok (G-Ç29; A4-K111)");
    KA_DOGRULA(o->fc.state == IDA_FCS_SAFE, "bayatlıkta fc.state %s (G-Ç29)", ka_durum_adi(o->fc.state));
}

/* ---- G-Ç53, F7: geri okuma -------------------------------------------------------------------------- */

static void t_gc53_izin_celiskisi(void)
{
    /* (1) Kapatma APPLIED olduktan sonra t_meas > t_apply örnekte PERMIT_OPEN = 1: FAULT_LATCHED,
       sonucun ve örneğin geldiği çağrıda. */
    ka_kur(2);
    ka_oturum_baslat();
    gevsek();
    ka_adimlar(2);
    ka_ayar.tup[0] = tup_eksik(IDA_TUBE_PIN_OUT);
    (void)ka_adim();
    KA_DOGRULA(ka_kapanma_var(ka_out(), 0u), "pim takılınca SAFETY_CLOSE yok");
    ka_ayar.permit_elle = true;
    ka_ayar.permit_deger[0] = 1u;
    (void)ka_adim();
    KA_DOGRULA(ka_out()->fc.state == IDA_FCS_FAULT_LATCHED,
               "(1) SAFETY_CLOSE APPLIED, sonraki örnekte PERMIT_OPEN = 1: fc.state %s, FAULT_LATCHED bekleniyordu (G-Ç53)",
               ka_durum_adi(ka_out()->fc.state));
    /* (2) t_meas = t_apply olan örnekte PERMIT_OPEN = 1 çelişki değildir. */
    ka_kur(2);
    ka_oturum_baslat();
    gevsek();
    ka_adimlar(2);
    ka_ayar.tup[0] = tup_eksik(IDA_TUBE_PIN_OUT);
    (void)ka_adim();
    const ka_olay_t *c = ka_son_olay((uint8_t)IDA_EVT_SAFETY_CLOSE, -1);
    KA_DOGRULA(c != NULL && c->cagri == ka_cagri_no(), "pim takılınca SAFETY_CLOSE yok");
    int64_t t_apply = c->t_apply;
    ka_ayar.emniyet = false;
    ida_in_t *in = ka_girdi();
    ida_sample_t *s = ka_emniyet_ekle(in, 0u, 0u, (uint8_t)(tup_eksik(IDA_TUBE_PIN_OUT) | IDA_TUBE_PERMIT_OPEN), KC_TUP_ACIK);
    s->hdr.t_meas.ns = t_apply;
    (void)ka_cagir();
    KA_DOGRULA(ka_out()->fc.state != IDA_FCS_FAULT_LATCHED, "(2) t_meas = t_apply örnekte PERMIT_OPEN çelişki sayıldı (G-Ç53: büyük)");
    ka_ayar.emniyet = true;
    ka_adimlar(5);
    KA_DOGRULA(ka_out()->fc.state != IDA_FCS_FAULT_LATCHED, "(2) PERMIT_OPEN = 0 örneklerle FAULT_LATCHED");
    /* (3) Fiş takılı ve acil durdurma basılı çelişki değildir. */
    ka_ayar.estop = 1u;
    ka_ayar.tup[0] = KC_TUP_ACIK;
    for (int k = 0; k < 200; ++k) {
        (void)ka_adim();
        KA_DOGRULA(ka_out()->fc.state != IDA_FCS_FAULT_LATCHED, "(3) fiş takılı + acil durdurma basılı FAULT_LATCHED'e götürdü (G-Ç53)");
    }
}

static void t_f7_geri_okuma_bozulmasi(void)
{
    /* §7 F7 (FC silahlanmadan gözlenebilen kısmı): kısıtlayan yönde tek örneklik bozulmalar
       çelişki değildir, kısıtlayıcı içerik olarak uygulanır (SAFETY_CLOSE); PERMIT_OPEN'ın
       W5-K4'ün birinci durumuna uyan bozulması FAULT_LATCHED. */
    static const char *const ad[3] = {"acil durdurma", "tüp 1 fişi", "tüp 0 pimi"};
    ka_kur(2);
    ka_oturum_baslat();
    for (int d = 0; d < 3; ++d) {
        for (int tekrar = 0; tekrar < 3; ++tekrar) {
            gevsek();
            ka_adimlar(3);
            if (d == 0) {
                ka_ayar.estop = 1u;
            } else if (d == 1) {
                ka_ayar.tup[1] = tup_eksik(IDA_TUBE_PLUG_IN);
            } else {
                ka_ayar.tup[0] = tup_eksik(IDA_TUBE_PIN_OUT);
            }
            (void)ka_adim();
            KA_DOGRULA(ka_kapanma_var(ka_out(), (uint8_t)(d == 1 ? 1u : 0u)), "F7 %s bozulması: SAFETY_CLOSE yok", ad[d]);
            KA_DOGRULA(ka_out()->fc.state == IDA_FCS_SAFE, "F7 %s bozulması: fc.state %s (çelişki değil)", ad[d],
                       ka_durum_adi(ka_out()->fc.state));
            gevsek();
            ka_adimlar(2);
            KA_DOGRULA(ka_out()->fc.state == IDA_FCS_SAFE, "F7 %s bozulması sonrası fc.state %s", ad[d], ka_durum_adi(ka_out()->fc.state));
        }
    }
    ka_kilitle(); /* PERMIT_OPEN bozulması → FAULT_LATCHED (G-Ç53) */
}

static const ka_test_t TESTLER[] = {
    {"etkin_deger", t_gc14_etkin_deger},
    {"kapanma_bir_kez", t_gc15_kapanma_bir_kez},
    {"oturum_sonu", t_gc18_oturum_sonu},
    {"ikinci_oturum_sonu", t_gc44_ikinci_oturum_sonu},
    {"oturum_baslangici", t_gc45_oturum_baslangici},
    {"oturum_sonu_k122", t_gc18_oturum_sonu_k122},
    {"ikinci_oturum_sonu_k122", t_gc44_ikinci_oturum_sonu_k122},
    {"oturum_baslangici_k122", t_gc45_oturum_baslangici_k122},
    {"bayatlik_alim", t_gc29_bayatlik_alim},
    {"bayatlik_olcum", t_gc29_bayatlik_olcum},
    {"f14_akis_kesilmesi", t_f14_akis_kesilmesi},
    {"f14_olcum_donmasi", t_f14_olcum_donmasi},
    {"geri_alim_zamani", t_gc29_geri_alim_zamani},
    {"izin_celiskisi", t_gc53_izin_celiskisi},
    {"f7_geri_okuma", t_f7_geri_okuma_bozulmasi},
};

int main(int argc, char **argv)
{
    return ka_main(argc, argv, __FILE__, TESTLER, sizeof TESTLER / sizeof TESTLER[0]);
}
