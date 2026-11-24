/*
 * tests/kabul/atis — temel: başlangıç durumu, değer kümeleri, engel bitleri, olay kimliği ve
 * alanları, olay sonuçları, parametreler (FC'nin silahlanmasını gerektirmeyen G-Ç maddeleri).
 */
#include <string.h>

#include "kabul_atis.h"
#include "unity.h"

/* ---- yardımcılar -------------------------------------------------------------------- */

static bool engel(uint32_t bit)
{
    return (ka_out()->fc.inhibit_mask & bit) != 0u;
}

static void gevsek(void)
{
    ka_ayar.estop = 0u;
    ka_ayar.tup[0] = KC_TUP_ACIK;
    ka_ayar.tup[1] = KC_TUP_ACIK;
    ka_ayar.emniyet = true;
}

/* Şema varsayılanlarından geçerli blok (A4 §3.10) ile özel init girdisi. */
static uint8_t g_blok[IDA_PARAM_BLOB_MAX + 64u];
static kc_pkayit_t g_kayit[KC_PARAM_AZAMI];

static void init_girdi_bloklu(ida_init_in_t *ii)
{
    size_t n = kc_varsayilan_kayitlar(g_kayit, KC_PARAM_AZAMI);
    size_t boy = kc_blok_kur(g_blok, sizeof g_blok, g_kayit, n, kc_sema_ozeti());
    kc_init_girdi(ii);
    ii->param_src = (uint8_t)IDA_PSRC_SIL;
    ii->param_blob = g_blok;
    ii->param_len = (uint32_t)boy;
}

/* ---- G-Ç01 ---------------------------------------------------------------------------- */

static void ilk_cikti_denetle(const char *ne)
{
    const ida_out_t *o = ka_out();
    KA_DOGRULA(o->fc.state == IDA_FCS_SAFE, "%s: init sonrası ilk çıktıda fc.state %s (G-Ç01)", ne, ka_durum_adi(o->fc.state));
    KA_DOGRULA(!o->armed && !o->status.armed, "%s: init sonrası ilk çıktıda armed = 1 (G-Ç01; A4-K77)", ne);
    KA_DOGRULA(!ka_genisletici_olay_var(o), "%s: init sonrası ilk çıktıda SAFETY_OPEN/FIRE (G-Ç01)", ne);
    KA_DOGRULA(engel(IDA_FCI_NOT_ARMED), "%s: init sonrası NOT_ARMED biti yok (G-Ç01, G-Ç03)", ne);
}

static void t_gc01_ilk_cikti(void)
{
    ka_kur(2);
    (void)ka_adim();
    ilk_cikti_denetle("ilk init");
    ka_oturum_ac();
    ka_arac_silahla();
    KA_DOGRULA(ka_out()->armed, "araç silahlanamadı");
    {
        ida_in_t *in = ka_girdi();
        (void)ka_fc(in, (uint8_t)IDA_FC_ARM, 0u);
        (void)ka_cagir();
    }
    (void)ka_yeniden_init();
    (void)ka_adim();
    ilk_cikti_denetle("silahlıyken yeniden init");
    /* Silahlandırma taşınmaz: yeni ARM olmadan 1 s boyunca silahsız kalır. */
    for (int k = 0; k < 200; ++k) {
        (void)ka_adim();
        KA_DOGRULA(!ka_out()->armed, "yeniden init sonrası ARM olmadan armed = 1 (G-Ç01; A4-K77)");
        KA_DOGRULA(ka_out()->fc.state != IDA_FCS_ARMED, "yeniden init sonrası FC_ARM olmadan fc.state ARMED (G-Ç01)");
    }
}

static void t_gc01_kilitten_init(void)
{
    ka_kur(2);
    ka_oturum_baslat();
    ka_kilitle();
    (void)ka_yeniden_init();
    (void)ka_adim();
    ilk_cikti_denetle("FAULT_LATCHED iken yeniden init");
}

/* ---- G-Ç02, G-Ç30: değer kümeleri ve kapalı tablolar ----------------------------------- */

static void fc_sonuc_kumesi(const ida_in_t *in)
{
    const ida_out_t *o = ka_out();
    unsigned nlatch = (unsigned)kc_bit_sayisi((uint32_t)(in->narrow.mask & IDA_NARROW_DEFINED));
    unsigned nc = in->n_cmds <= IDA_PORT_MAX_CMDS ? in->n_cmds : 0u;
    for (unsigned j = 0; j < nc && nlatch + j < o->n_results; ++j) {
        const ida_cmd_t *c = &in->cmds[j];
        const ida_cmd_result_t *r = &o->results[nlatch + j];
        if (c->type != IDA_CMD_FC) {
            continue;
        }
        uint16_t z = r->reason;
        bool izinli = z == IDA_REASON_NONE || z == IDA_REASON_CALL_REJECTED || z == IDA_REASON_NOT_ARMABLE ||
                      z == IDA_REASON_OUT_OF_RANGE || z == IDA_REASON_BAD_MODE || z == IDA_REASON_EVENTS_OFF ||
                      z == IDA_REASON_SESSION || z == IDA_REASON_NARROWED || z == IDA_REASON_IMU_LAG ||
                      z == IDA_REASON_IMU_REJECTED;
        KA_DOGRULA(izinli, "FC komutu (op %u) sonucunda reason %u: A4 §3.17'nin FC satırlarında yok (G-Ç30)",
                   (unsigned)c->u.fc.op, (unsigned)z);
        KA_DOGRULA(r->ack != IDA_ACK_IN_PROGRESS, "FC komutu (op %u) IN_PROGRESS (G-Ç30, 3.17)", (unsigned)c->u.fc.op);
    }
}

static void deger_kumesi_denetle(void)
{
    const ida_out_t *o = ka_out();
    KA_DOGRULA(o->fc.state <= IDA_FCS_FAULT_LATCHED, "fc.state %u (G-Ç02)", (unsigned)o->fc.state);
    KA_DOGRULA((o->fc.inhibit_mask & UINT32_C(0xFFFC0000)) == 0u, "inhibit_mask bit 18–31: 0x%08lx (G-Ç02)",
               (unsigned long)o->fc.inhibit_mask);
    for (unsigned i = 0; i < o->n_logs; ++i) {
        KA_DOGRULA(o->logs[i].code >= 1u && o->logs[i].code <= IDA_LOG_CODE_LAST, "günlük kodu %u (G-Ç30)",
                   (unsigned)o->logs[i].code);
        if (o->logs[i].code == IDA_LOG_FC_STATE) {
            KA_DOGRULA(o->logs[i].arg0 >= 0 && o->logs[i].arg0 <= 5 && o->logs[i].arg1 >= 0 && o->logs[i].arg1 <= 5,
                       "FC_STATE günlüğü argümanları %ld/%ld (G-Ç30, 3.15)", (long)o->logs[i].arg0, (long)o->logs[i].arg1);
        }
        if (o->logs[i].code == IDA_LOG_EVENT_RESULT) {
            KA_DOGRULA(o->logs[i].arg0 >= 1 && o->logs[i].arg0 <= 3, "EVENT_RESULT olay türü %ld (3.15)", (long)o->logs[i].arg0);
        }
    }
}

/* Çeşitli girdilerle uzun bir dizi: oturum, emniyet geçişleri, geçersiz ve geçerli FC
   komutları, reddedilen çağrılar, tick'ler. */
static void cesitli_dizi(void (*denetim)(const ida_in_t *))
{
    static const uint8_t oplar[] = {0u, 1u, 2u, 3u, 4u, 5u, 6u, 7u, 8u, 9u, 200u};
    kc_rng_t r;
    kc_rng_kur(&r, UINT64_C(0x6302));
    ka_kur(1);
    ka_oturum_baslat();
    for (int k = 0; k < 3000; ++k) {
        gevsek();
        uint32_t z = kc_rng_aralik(&r, 100u);
        if (z < 3u) {
            ka_ayar.estop = 1u;
        } else if (z < 5u) {
            ka_ayar.tup[kc_rng_aralik(&r, 2u) & 1u] = (uint8_t)(KC_TUP_ACIK & (uint8_t)(0xFFu ^ (IDA_TUBE_PLUG_IN)));
        } else if (z < 7u) {
            ka_ayar.tup[kc_rng_aralik(&r, 2u) & 1u] = (uint8_t)(KC_TUP_ACIK & (uint8_t)(0xFFu ^ (IDA_TUBE_PIN_OUT)));
        }
        ka_ayar.tick = kc_rng_aralik(&r, 20u) == 0u;
        ida_in_t *in;
        bool red = kc_rng_aralik(&r, 40u) == 0u;
        if (red) {
            in = ka_girdi_seq(kc_son_seq());
        } else {
            in = ka_girdi();
        }
        if (kc_rng_aralik(&r, 300u) == 0u) {
            in->plat.link_event = (uint8_t)IDA_LINKEV_SESSION_END;
            in->plat.link_state = (uint8_t)IDA_LINK_NONE;
        } else if (kc_rng_aralik(&r, 300u) == 0u) {
            in->plat.link_event = (uint8_t)IDA_LINKEV_SESSION_START;
        }
        unsigned nfc = kc_rng_aralik(&r, 3u);
        for (unsigned q = 0; q < nfc && in->n_cmds < IDA_PORT_MAX_CMDS; ++q) {
            uint8_t op = oplar[kc_rng_aralik(&r, (uint32_t)sizeof oplar) % sizeof oplar];
            uint8_t tube = (uint8_t)(kc_rng_aralik(&r, 8u) == 0u ? 5u : kc_rng_aralik(&r, 2u));
            uint32_t id = ka_fc(in, op, tube);
            (void)id;
            if (kc_rng_aralik(&r, 10u) == 0u) {
                (*ka_son_komut(in)).u.fc.arg0 = 1u;
            }
        }
        if (kc_rng_aralik(&r, 50u) == 0u) {
            (void)ka_mandal(in, (uint8_t)IDA_NARROW_FC_ABORT);
        }
        (void)ka_cagir();
        deger_kumesi_denetle();
        if (denetim != NULL) {
            denetim(in);
        }
    }
}

static void t_gc02_deger_kumesi(void)
{
    cesitli_dizi(NULL);
}

static void t_gc30_kapali_tablolar(void)
{
    cesitli_dizi(fc_sonuc_kumesi);
}

/* ---- G-Ç31 ------------------------------------------------------------------------------ */

static void t_gc31_not_init(void)
{
    static ida_in_t in;
    kc_ortam_kur(2);
    for (int f = 0; f < 2; ++f) {
        if (f == 0) {
            kc_in_step(&in, 1u, KC_T0 + KC_P);
        } else {
            kc_in_tick(&in, 1u, KC_T0 + KC_P);
        }
        (void)kc_emniyet_ekle(&in, 0u, 1u, KC_T0 + KC_P, 0u, KC_TUP_ACIK, KC_TUP_ACIK);
        (void)kc_fc_ekle(&in, 10u, (uint8_t)IDA_FC_ARM, 0u);
        (void)kc_fc_ekle(&in, 11u, (uint8_t)IDA_FC_FIRE, 0u);
        (void)kc_fc_ekle(&in, 12u, (uint8_t)IDA_FC_RESET, 0u);
        kc_mandal_kur(&in, (uint8_t)IDA_NARROW_FC_DISARM, 20u);
        in.n_evres = 1u;
        in.evres[0].event_id = (UINT64_C(7) << 32) | 1u;
        in.evres[0].result = (uint8_t)IDA_EVR_APPLIED;
        ida_rc_t rc = f == 0 ? kc_step(&in) : kc_tick(&in);
        const ida_out_t *o = kc_out();
        KA_DOGRULA(rc == IDA_RC_NOT_INIT, "kurulmamış örnek: dönüş %d", (int)rc);
        KA_DOGRULA(o->fc.state == IDA_FCS_SAFE, "NOT_INIT çıktısında fc.state %s (G-Ç31)", ka_durum_adi(o->fc.state));
        KA_DOGRULA(o->fc.inhibit_mask == UINT32_C(0x00026FFF), "NOT_INIT çıktısında inhibit_mask 0x%05lx, 0x26FFF beklenir (G-Ç31)",
                   (unsigned long)o->fc.inhibit_mask);
        KA_DOGRULA(o->n_events == 0u && o->n_logs == 0u, "NOT_INIT çıktısında %u olay, %u günlük (G-Ç31)", (unsigned)o->n_events,
                   (unsigned)o->n_logs);
        KA_DOGRULA(o->fc.last_event_id == 0u && o->fc.last_event_result == 0u, "NOT_INIT çıktısında last_event (G-Ç31; A4-K126)");
    }
}

/* ---- G-Ç03: engel bitleri ---------------------------------------------------------------- */

static void t_gc03_not_armed(void)
{
    ka_kur(2);
    ka_oturum_ac();
    KA_DOGRULA(engel(IDA_FCI_NOT_ARMED) == !ka_out()->armed, "NOT_ARMED biti araç silahsızken yok (G-Ç03)");
    for (int tur = 0; tur < 3; ++tur) {
        for (int k = 0; k < 20; ++k) {
            ida_in_t *in = ka_girdi();
            (void)ka_komut(in, (uint8_t)IDA_CMD_ARM);
            (void)ka_cagir();
            if (ka_out()->armed) {
                break;
            }
            ka_adimlar(5);
        }
        KA_DOGRULA(ka_out()->armed, "araç silahlanamadı");
        KA_DOGRULA(!engel(IDA_FCI_NOT_ARMED), "ARM'ın kabul edildiği çağrıda NOT_ARMED biti hâlâ kurulu (G-Ç03: aynı çağrıda)");
        ka_adimlar(3);
        KA_DOGRULA(!engel(IDA_FCI_NOT_ARMED), "silahlıyken NOT_ARMED biti (G-Ç03)");
        ida_in_t *in = ka_girdi();
        (void)ka_mandal(in, (uint8_t)IDA_NARROW_DISARM);
        (void)ka_cagir();
        KA_DOGRULA(!ka_out()->armed && engel(IDA_FCI_NOT_ARMED), "DISARM çağrısında NOT_ARMED biti kurulmadı (G-Ç03)");
        ka_adimlar(3);
    }
}

static void t_gc03_mod(void)
{
    /* MODE kümesi A4'te sayılmaz: bit kipin işlevidir (aynı kipte hep aynı) ve kip değiştiği
       çağrıda yeni kipin değerini alır. */
    int bit_kip[8] = {-1, -1, -1, -1, -1, -1, -1, -1};
    static const uint8_t kipler[] = {IDA_MODE_STABILIZE, IDA_MODE_MANUAL, IDA_MODE_GUIDED, IDA_MODE_DEPTH_HOLD,
                                     IDA_MODE_STATION_KEEP, IDA_MODE_GUIDED, IDA_MODE_MANUAL};
    ka_kur(2);
    ka_oturum_ac();
    ka_arac_silahla();
    for (size_t i = 0; i < sizeof kipler; ++i) {
        ida_in_t *in = ka_girdi();
        if (kipler[i] == IDA_MODE_GUIDED) {
            uint32_t id = ka_yeni_id();
            kc_komut_ekle(in, id, (uint8_t)IDA_CMD_SET_MODE)->u.set_mode.mode = kipler[i];
        } else {
            (void)ka_mandal(in, (uint8_t)IDA_NARROW_MODE);
            in->narrow.mode = kipler[i];
        }
        (void)ka_cagir();
        for (int k = 0; k < 4; ++k) {
            const ida_out_t *o = ka_out();
            uint8_t m = o->status.mode;
            int b = engel(IDA_FCI_MODE) ? 1 : 0;
            if (m < 8u) {
                if (bit_kip[m] < 0) {
                    bit_kip[m] = b;
                }
                KA_DOGRULA(bit_kip[m] == b, "kip %u'da MODE biti %d, aynı kipte önce %d (G-Ç03: bit kip koşulunun işlevi)",
                           (unsigned)m, b, bit_kip[m]);
                /* W5-K15: küme bu sürümde {GUIDED}. */
                KA_DOGRULA(b == (m != IDA_MODE_GUIDED ? 1 : 0), "kip %u'da MODE biti %d (W5-K15: küme {GUIDED})", (unsigned)m, b);
            }
            (void)ka_adim();
        }
    }
}

static void t_gc03_est(void)
{
    ka_kur(2);
    ka_oturum_baslat();
    for (int k = 0; k < 600; ++k) {
        (void)ka_adim();
        const ida_out_t *o = ka_out();
        if (o->nav_valid) {
            KA_DOGRULA(engel(IDA_FCI_EST) == (o->nav.est_level != IDA_EST_OK),
                       "EST biti %d, est_level %u (G-Ç03; A4 §3.16: kestirim düzeyi OK değil)", (int)engel(IDA_FCI_EST),
                       (unsigned)o->nav.est_level);
        }
    }
}

static void t_gc03_failsafe(void)
{
    ka_kur(2);
    ka_oturum_ac();
    ka_arac_silahla();
    {
        /* Arıza eylemi etkin bir kipte sınanır (DEPTH_HOLD; daraltıcı kip mandalı). */
        ida_in_t *in = ka_girdi();
        (void)ka_mandal(in, (uint8_t)IDA_NARROW_MODE);
        in->narrow.mode = (uint8_t)IDA_MODE_DEPTH_HOLD;
        (void)ka_cagir();
    }
    bool goruldu = false;
    for (int k = 0; k < 1200; ++k) {
        ka_ayar.sizinti = (uint8_t)(k >= 100 && k < 600 ? 1u : 0u);
        (void)ka_adim();
        const ida_out_t *o = ka_out();
        bool fs = o->fs_level >= IDA_FS_HOLD;
        goruldu = goruldu || fs;
        KA_DOGRULA(engel(IDA_FCI_FAILSAFE) == fs, "FAILSAFE biti %d, fs_level %u (G-Ç03; A4 §3.16: HOLD ya da üstü)",
                   (int)engel(IDA_FCI_FAILSAFE), (unsigned)o->fs_level);
    }
    KA_DOGRULA(goruldu, "sızıntı ile arıza eylemi HOLD ya da üstüne çıkmadı; FAILSAFE yönü sınanamadı");
}

static void t_gc03_estop(void)
{
    ka_kur(2);
    ka_oturum_baslat();
    gevsek();
    (void)ka_adim();
    KA_DOGRULA(!engel(IDA_FCI_ESTOP), "kabul edilen gevşek inst 0 örneğiyle ESTOP biti kalkmadı (G-Ç03, G-Ç14)");
    for (int tur = 0; tur < 3; ++tur) {
        ka_ayar.estop = 1u;
        (void)ka_adim();
        KA_DOGRULA(engel(IDA_FCI_ESTOP), "estop = 1 örneğinin çağrısında ESTOP biti yok (G-Ç03: aynı çağrıda)");
        ka_adimlar(3);
        KA_DOGRULA(engel(IDA_FCI_ESTOP), "estop sürerken ESTOP biti yok (G-Ç16)");
        ka_ayar.estop = 0u;
        (void)ka_adim();
        KA_DOGRULA(!engel(IDA_FCI_ESTOP), "estop = 0 örneğinin çağrısında ESTOP biti kalkmadı (G-Ç03: aynı çağrıda)");
    }
}

static void tup_biti(uint8_t gerekli_bit, uint32_t fci0, uint32_t fci1, const char *ad)
{
    ka_kur(2);
    ka_oturum_baslat();
    gevsek();
    (void)ka_adim();
    KA_DOGRULA(!engel(fci0) && !engel(fci1), "%s: gevşek örnekle bit kalkmadı (G-Ç03)", ad);
    for (unsigned t = 0; t < 2u; ++t) {
        uint32_t b = t == 0u ? fci0 : fci1;
        uint32_t d = t == 0u ? fci1 : fci0;
        ka_ayar.tup[t] = (uint8_t)(KC_TUP_ACIK & (uint8_t)~gerekli_bit);
        (void)ka_adim();
        KA_DOGRULA(engel(b), "%s tüp %u: kısıtlayan örneğin çağrısında bit yok (G-Ç03)", ad, t);
        KA_DOGRULA(!engel(d), "%s tüp %u: öteki tüpün biti de kuruldu (G-Ç03, G-Ç16: tüpe özgü)", ad, t);
        ka_adimlar(2);
        KA_DOGRULA(engel(b), "%s tüp %u: kısıtlama sürerken bit yok (G-Ç16)", ad, t);
        ka_ayar.tup[t] = KC_TUP_ACIK;
        (void)ka_adim();
        KA_DOGRULA(!engel(b), "%s tüp %u: gevşeten örneğin çağrısında bit kalkmadı (G-Ç03)", ad, t);
    }
}

static void t_gc03_fis(void)
{
    tup_biti(IDA_TUBE_PLUG_IN, IDA_FCI_PLUG_T0, IDA_FCI_PLUG_T1, "PLUG");
}

static void t_gc03_pim(void)
{
    tup_biti(IDA_TUBE_PIN_OUT, IDA_FCI_PIN_T0, IDA_FCI_PIN_T1, "PIN");
}

static void t_gc03_bos(void)
{
    tup_biti(IDA_TUBE_LOADED, IDA_FCI_EMPTY_T0, IDA_FCI_EMPTY_T1, "EMPTY");
}

static void t_gc03_events_off(void)
{
    ida_init_in_t ii;
    init_girdi_bloklu(&ii);
    ii.boot_count_ok = false;
    (void)ka_kur_init(2, &ii);
    for (int k = 0; k < 20; ++k) {
        (void)ka_adim();
        KA_DOGRULA(engel(IDA_FCI_EVENTS_OFF), "boot_count_ok = 0 iken EVENTS_OFF biti yok (G-Ç03, G-Ç36)");
    }
    ka_kur(2);
    for (int k = 0; k < 20; ++k) {
        (void)ka_adim();
        KA_DOGRULA(!engel(IDA_FCI_EVENTS_OFF), "geçerli boot_count ile EVENTS_OFF biti (G-Ç03)");
    }
}

static void t_gc03_imu_lag(void)
{
    ka_kur(2);
    ka_oturum_baslat();
    bool kuruldu = false;
    for (int k = 0; k < 200; ++k) {
        ka_ayar.imu_gecikme = (k >= 20 && k < 80) ? 25 * KA_MS : 0;
        (void)ka_adim();
        const ida_out_t *o = ka_out();
        kuruldu = kuruldu || o->health.imu_lag;
        KA_DOGRULA(engel(IDA_FCI_IMU_LAG) == o->health.imu_lag, "IMU_LAG biti %d, health.imu_lag %d (G-Ç03; A4-K37)",
                   (int)engel(IDA_FCI_IMU_LAG), (int)o->health.imu_lag);
    }
    KA_DOGRULA(kuruldu, "25 ms gecikmeli IMU ile IMU_LAG mandalı kurulmadı (A4-K37)");
}

static void t_gc03_imu_stale(void)
{
    ka_kur(2);
    ka_oturum_baslat();
    gevsek();
    ka_adimlar(5);
    for (int tur = 0; tur < 3; ++tur) {
        KA_DOGRULA(!engel(IDA_FCI_IMU_STALE), "taze IMU ile IMU_STALE biti (G-Ç03)");
        ka_ayar.tick = true;
        ka_adimlar(6); /* 30 ms > T_imu_stale (20 ms) */
        KA_DOGRULA(engel(IDA_FCI_IMU_STALE), "30 ms IMU'suz IMU_STALE biti yok (G-Ç03; A4-K39)");
        ka_ayar.tick = false;
        (void)ka_adim();
        KA_DOGRULA(!engel(IDA_FCI_IMU_STALE), "IMU kabul edilen çağrıda IMU_STALE biti kalkmadı (G-Ç03: aynı çağrıda)");
    }
}

static void t_gc03_oturum(void)
{
    ka_kur(2);
    ka_oturum_baslat();
    for (int tur = 0; tur < 3; ++tur) {
        KA_DOGRULA(!engel(IDA_FCI_SESSION), "oturum RUNNING iken SESSION biti (G-Ç03)");
        ka_ayar.link_state = (uint8_t)IDA_LINK_NONE;
        (void)ka_adim();
        KA_DOGRULA(engel(IDA_FCI_SESSION), "link_state NONE çağrısında SESSION biti yok (G-Ç03: aynı çağrıda)");
        ka_ayar.link_state = (uint8_t)IDA_LINK_RUNNING;
        (void)ka_adim();
        KA_DOGRULA(!engel(IDA_FCI_SESSION), "link_state RUNNING çağrısında SESSION biti kalkmadı (G-Ç03)");
    }
}

static void t_gc03_det_stale(void)
{
    /* Yalnız algılama verisi olmadan sınanabilen yön (kapsam sınırı: algılama verisi
       yazılmaz): init'ten T_det_stale (2 s) dolmadan bit yok, dolunca kurulu (A4-K118). */
    ka_kur(2);
    for (int k = 1; k <= 460; ++k) {
        (void)ka_adim();
        int64_t gecen = ka_simdi() - KC_T0;
        if (gecen < 1990 * KA_MS) {
            KA_DOGRULA(!engel(IDA_FCI_DET_STALE), "init'ten %lld ms sonra DET_STALE biti (T_det_stale 2 s dolmadı; G-Ç03, A4-K118)",
                       (long long)(gecen / KA_MS));
        } else if (gecen > 2010 * KA_MS) {
            KA_DOGRULA(engel(IDA_FCI_DET_STALE), "init'ten %lld ms sonra başlık yokken DET_STALE biti yok (G-Ç03, A4-K118)",
                       (long long)(gecen / KA_MS));
        }
    }
}

/* ---- G-Ç43, G-Ç17 ------------------------------------------------------------------------- */

static void t_gc43_imu_stale_ilk(void)
{
    ka_kur(2);
    ka_ayar.tick = true;
    (void)ka_adim();
    KA_DOGRULA(engel(IDA_FCI_IMU_STALE), "init'ten sonra IMU kabul edilmeden IMU_STALE biti yok (G-Ç43)");
    ka_ayar.tick = false;
    (void)ka_adim();
    KA_DOGRULA(!engel(IDA_FCI_IMU_STALE), "ilk IMU kabulünde IMU_STALE biti kalkmadı (G-Ç43)");
    ka_ayar.tick = true;
    ida_in_t *in = ka_girdi();
    in->plat.link_event = (uint8_t)IDA_LINKEV_SESSION_START;
    (void)ka_cagir();
    KA_DOGRULA(engel(IDA_FCI_IMU_STALE), "SESSION_START'tan sonra IMU kabul edilmeden IMU_STALE biti yok (G-Ç43; 3.11 (b3))");
    ka_ayar.tick = false;
    (void)ka_adim();
    KA_DOGRULA(!engel(IDA_FCI_IMU_STALE), "SESSION_START sonrası ilk IMU kabulünde IMU_STALE biti kalkmadı (G-Ç43)");
}

static void t_gc17_bos_tup(void)
{
    ka_kur(2);
    ka_ayar.emniyet = false;
    (void)ka_adim();
    KA_DOGRULA(engel(IDA_FCI_EMPTY_T0) && engel(IDA_FCI_EMPTY_T1), "init'ten sonra emniyet örneği yokken EMPTY_T0/T1 yok (G-Ç17)");
    /* inst 1 örneği LOADED gösterse de boşluk kalkmaz (LOADED yalnız inst 0'dan; A4-K116). */
    ida_in_t *in = ka_girdi();
    (void)ka_emniyet_ekle(in, 1u, 0u, KC_TUP_ACIK, KC_TUP_ACIK);
    (void)ka_cagir();
    KA_DOGRULA(engel(IDA_FCI_EMPTY_T0) && engel(IDA_FCI_EMPTY_T1), "yalnız inst 1 örneğiyle EMPTY bitleri kalktı (G-Ç17; A4-K116)");
    ka_ayar.emniyet = true;
    gevsek();
    (void)ka_adim();
    KA_DOGRULA(!engel(IDA_FCI_EMPTY_T0) && !engel(IDA_FCI_EMPTY_T1), "LOADED'lı inst 0 örneğiyle EMPTY bitleri kalkmadı (G-Ç17)");
    /* SESSION_END: son kabul edilen LOADED = 1 olsa da tüpler boş sayılır. */
    ka_ayar.emniyet = false;
    in = ka_girdi();
    in->plat.link_event = (uint8_t)IDA_LINKEV_SESSION_END;
    in->plat.link_state = (uint8_t)IDA_LINK_NONE;
    (void)ka_cagir();
    KA_DOGRULA(engel(IDA_FCI_EMPTY_T0) && engel(IDA_FCI_EMPTY_T1), "SESSION_END'den sonra EMPTY_T0/T1 yok (G-Ç17; A4-K128)");
    ka_ayar.link_state = (uint8_t)IDA_LINK_NONE;
    ka_adimlar(3);
    KA_DOGRULA(engel(IDA_FCI_EMPTY_T0) && engel(IDA_FCI_EMPTY_T1), "SESSION_END'den sonra örnek gelmeden EMPTY bitleri kalktı (G-Ç17)");
    ka_ayar.link_state = (uint8_t)IDA_LINK_RUNNING;
    ka_oturum_baslat();
    ka_ayar.emniyet = true;
    (void)ka_adim();
    KA_DOGRULA(!engel(IDA_FCI_EMPTY_T0) && !engel(IDA_FCI_EMPTY_T1), "yeni oturumda LOADED'lı örnekle EMPTY bitleri kalkmadı");
}

/* ---- G-Ç36, G-Ç08 ------------------------------------------------------------------------ */

static void olaysiz_kos(const char *ne)
{
    ka_oturum_baslat();
    for (int tur = 0; tur < 3; ++tur) {
        gevsek();
        ka_adimlar(2);
        ka_ayar.estop = 1u;
        ka_ayar.tup[0] = 0u;
        (void)ka_adim();
        KA_DOGRULA(ka_out()->n_events == 0u, "%s: acil durdurma geçişinde %u olay (G-Ç36)", ne, (unsigned)ka_out()->n_events);
        ida_in_t *in = ka_girdi();
        in->plat.link_event = (uint8_t)IDA_LINKEV_SESSION_END;
        in->plat.link_state = (uint8_t)IDA_LINK_NONE;
        uint32_t arm = ka_komut(in, (uint8_t)IDA_CMD_ARM);
        (void)arm;
        (void)ka_cagir();
        KA_DOGRULA(ka_out()->n_events == 0u, "%s: SESSION_END'de %u olay (G-Ç36)", ne, (unsigned)ka_out()->n_events);
        KA_DOGRULA(!ka_out()->status.events_ok, "%s: events_ok = 1 (G-Ç36)", ne);
        KA_DOGRULA(engel(IDA_FCI_EVENTS_OFF), "%s: EVENTS_OFF biti yok (G-Ç36)", ne);
        ka_oturum_baslat();
    }
    gevsek();
    ida_in_t *in = ka_girdi();
    uint32_t arm = ka_komut(in, (uint8_t)IDA_CMD_ARM);
    uint32_t fcarm = ka_fc(in, (uint8_t)IDA_FC_ARM, 0u);
    (void)ka_cagir();
    KA_SONUC(arm, IDA_ACK_REJECTED, IDA_REASON_EVENTS_OFF, "olay kimliği yokken ARM (G-Ç36)");
    KA_SONUC(fcarm, IDA_ACK_REJECTED, IDA_REASON_EVENTS_OFF, "olay kimliği yokken FC_ARM (G-Ç36; 3.17 FC_ARM satırı)");
}

static void t_gc36_olay_yok(void)
{
    ida_init_in_t ii;
    init_girdi_bloklu(&ii);
    ii.boot_count_ok = false;
    (void)ka_kur_init(2, &ii);
    olaysiz_kos("boot_count_ok = 0");
    init_girdi_bloklu(&ii);
    ii.boot_count = 0u;
    (void)ka_kur_init(2, &ii);
    olaysiz_kos("boot_count = 0");
    init_girdi_bloklu(&ii);
    ii.boot_count = UINT32_C(0xFFFFFFFF);
    (void)ka_kur_init(2, &ii);
    olaysiz_kos("boot_count = 0xFFFFFFFF");
}

static void kimlik_denetle(const ida_out_t *o, uint32_t *beklenen, int tur)
{
    KA_DOGRULA(o->n_events >= 1u, "tur %d: kapanma olayı yok", tur);
    for (unsigned i = 0; i < o->n_events; ++i) {
        uint64_t id = o->events[i].event_id;
        KA_DOGRULA((uint32_t)(id >> 32) == KC_BOOT, "event_id üst 32 bit %lu, boot_count %lu (G-Ç08)",
                   (unsigned long)(id >> 32), (unsigned long)KC_BOOT);
        KA_DOGRULA((uint32_t)(id & 0xFFFFFFFFu) == *beklenen, "olay sayacı %lu, beklenen %lu (G-Ç08: ilk 1, her olayda +1)",
                   (unsigned long)(id & 0xFFFFFFFFu), (unsigned long)*beklenen);
        ++*beklenen;
    }
}

static void t_gc08_olay_kimligi(void)
{
    uint32_t beklenen = 1u;
    ka_kur(2);
    ka_oturum_baslat();
    for (int tur = 0; tur < 6; ++tur) {
        gevsek();
        ka_adimlar(2);
        KA_DOGRULA(ka_out()->n_events == 0u, "tur %d: gevşek adımda beklenmeyen olay", tur);
        if (tur % 2 == 0) {
            ka_ayar.estop = 1u; /* A4-K111: tek SAFETY_CLOSE(ALL) */
            (void)ka_adim();
            kimlik_denetle(ka_out(), &beklenen, tur);
        } else {
            ida_in_t *in = ka_girdi();
            in->plat.link_event = (uint8_t)IDA_LINKEV_SESSION_END;
            in->plat.link_state = (uint8_t)IDA_LINK_NONE;
            (void)ka_cagir();
            kimlik_denetle(ka_out(), &beklenen, tur);
            ka_oturum_baslat();
        }
    }
}

/* ---- G-Ç09, G-Ç38 ------------------------------------------------------------------------ */

static void t_gc09_kapanma_birlesimi(void)
{
    ka_kur(2);
    ka_oturum_baslat();
    gevsek();
    ka_adimlar(2);
    /* (1) SESSION_END + acil durdurma + fiş: tek ALL. */
    ka_ayar.estop = 1u;
    ka_ayar.tup[0] = (uint8_t)(KC_TUP_ACIK & (uint8_t)(0xFFu ^ (IDA_TUBE_PLUG_IN)));
    ida_in_t *in = ka_girdi();
    in->plat.link_event = (uint8_t)IDA_LINKEV_SESSION_END;
    in->plat.link_state = (uint8_t)IDA_LINK_NONE;
    (void)ka_cagir();
    KA_DOGRULA(ka_out()->n_events == 1u && ka_olay_say(ka_out(), (uint8_t)IDA_EVT_SAFETY_CLOSE, IDA_TUBE_ALL) == 1,
               "SESSION_END + estop + fiş: %u olay, tek SAFETY_CLOSE(ALL) bekleniyordu (G-Ç09)", (unsigned)ka_out()->n_events);
    ka_oturum_baslat();
    /* (2) iki tüpün pimi: tüp başına bir kapanma, ALL yok. */
    gevsek();
    ka_adimlar(2);
    ka_ayar.tup[0] = (uint8_t)(KC_TUP_ACIK & (uint8_t)(0xFFu ^ (IDA_TUBE_PIN_OUT)));
    ka_ayar.tup[1] = (uint8_t)(KC_TUP_ACIK & (uint8_t)(0xFFu ^ (IDA_TUBE_PIN_OUT)));
    (void)ka_adim();
    KA_DOGRULA(ka_olay_say(ka_out(), (uint8_t)IDA_EVT_SAFETY_CLOSE, 0) == 1 &&
                   ka_olay_say(ka_out(), (uint8_t)IDA_EVT_SAFETY_CLOSE, 1) == 1 &&
                   ka_olay_say(ka_out(), (uint8_t)IDA_EVT_SAFETY_CLOSE, IDA_TUBE_ALL) == 0,
               "iki tüpün pimi takılınca SAFETY_CLOSE(0) ve SAFETY_CLOSE(1) bekleniyordu (G-Ç09; A4-K125), %u olay",
               (unsigned)ka_out()->n_events);
    /* (3) tüp pimi + acil durdurma: tek ALL. */
    gevsek();
    ka_adimlar(2);
    ka_ayar.estop = 1u;
    ka_ayar.tup[1] = (uint8_t)(KC_TUP_ACIK & (uint8_t)(0xFFu ^ (IDA_TUBE_PIN_OUT)));
    (void)ka_adim();
    KA_DOGRULA(ka_out()->n_events == 1u && ka_olay_say(ka_out(), (uint8_t)IDA_EVT_SAFETY_CLOSE, IDA_TUBE_ALL) == 1,
               "pim + acil durdurma: tek SAFETY_CLOSE(ALL) bekleniyordu (G-Ç09), %u olay", (unsigned)ka_out()->n_events);
}

static void t_gc38_kapanma_dusmez(void)
{
    /* Kapanma nedeni her dönüş kodunda kapanma üretir (SAFETY_CLOSE hiçbir koşulda düşmez). */
    enum { OK, TICK, IMU_RED, BAD, DUP, NUL, SAYI };
    static const char *const ad[SAYI] = {"step OK", "tick", "IMU_REJECTED", "BAD_INPUT", "DUP_SEQ", "out NULL"};
    for (int d = 0; d < SAYI; ++d) {
        ka_kur(2);
        ka_oturum_baslat();
        gevsek();
        ka_adimlar(3);
        ka_ayar.estop = 1u;
        ka_ayar.tick = d == TICK;
        ka_ayar.imu_bozuk = d == IMU_RED;
        ida_in_t *in;
        if (d == DUP) {
            in = ka_girdi_seq(kc_son_seq());
        } else {
            ka_ayar.sonuclari_tut = d == BAD || d == NUL;
            in = ka_girdi();
        }
        if (d == BAD) {
            in->narrow.mask = (uint8_t)(in->narrow.mask | 0x10u); /* tanımsız mandal biti: BAD_INPUT */
        }
        ida_rc_t rc = d == NUL ? ka_cagir_out_null() : ka_cagir();
        (void)rc;
        ka_ayar.estop = 1u;
        ka_ayar.tick = false;
        ka_ayar.imu_bozuk = false;
        ka_ayar.sonuclari_tut = false;
        if (d == NUL) {
            (void)ka_adim();
        }
        KA_DOGRULA(ka_kapanma_var(ka_out(), 0u) && ka_kapanma_var(ka_out(), 1u),
                   "%s: acil durdurma kapanması çıktıda yok (G-Ç38; A4-K125, K28, 3.11 (b2))", ad[d]);
    }
}

/* ---- G-Ç10, G-Ç39 ------------------------------------------------------------------------ */

static void kapanma_alanlari(uint32_t apply_by)
{
    gevsek();
    ka_adimlar(2);
    ka_ayar.estop = 1u;
    (void)ka_adim();
    const ida_event_t *e = ka_olay_bul(ka_out(), (uint8_t)IDA_EVT_SAFETY_CLOSE, -1);
    KA_DOGRULA(e != NULL, "acil durdurmada SAFETY_CLOSE yok");
    KA_DOGRULA(e->hold_ns == 0u, "SAFETY_CLOSE hold_ns %lu (G-Ç10)", (unsigned long)e->hold_ns);
    KA_DOGRULA(e->apply_by_ns == apply_by, "SAFETY_CLOSE apply_by_ns %lu, parametre %lu (G-Ç10; A4-K66)",
               (unsigned long)e->apply_by_ns, (unsigned long)apply_by);
}

static void t_gc10_kapanma_alanlari(void)
{
    ka_kur(2);
    ka_oturum_baslat();
    uint32_t ab = ka_param_u32("FC_APPLY_BY_NS", KA_APPLY_BY_VARSAYILAN_NS);
    KA_DOGRULA(ab == KA_APPLY_BY_VARSAYILAN_NS, "FC_APPLY_BY_NS varsayılanı %lu, A1 §3.9: 20 ms", (unsigned long)ab);
    kapanma_alanlari(ab);
    ka_param_yaz("FC_APPLY_BY_NS", UINT32_C(50000000));
    kapanma_alanlari(UINT32_C(50000000));
}

static void param_set_bekle(const char *ad, uint32_t deger, bool gecerli)
{
    int id = ka_param_id(ad);
    KA_DOGRULA(id >= 0, "parametre %s yok (A4 §3.9)", ad);
    ida_in_t *in = ka_girdi();
    uint32_t cid = ka_yeni_id();
    ida_cmd_t *c = kc_komut_ekle(in, cid, (uint8_t)IDA_CMD_PARAM_SET);
    c->u.param.id = (uint16_t)id;
    c->u.param.type = (uint8_t)IDA_PT_U32;
    c->u.param.v.u32 = deger;
    (void)ka_cagir();
    const ida_cmd_result_t *r = ka_sonuc(cid);
    KA_DOGRULA(r != NULL, "PARAM_SET sonucu yok");
    if (gecerli) {
        KA_DOGRULA(r->ack == IDA_ACK_ACCEPTED, "%s = %lu reddedildi (%u/%u); aralık içinde (G-Ç39)", ad, (unsigned long)deger,
                   (unsigned)r->ack, (unsigned)r->reason);
    } else {
        KA_DOGRULA(r->ack == IDA_ACK_REJECTED && r->reason == IDA_REASON_PARAM_INVALID,
                   "%s = %lu kabul edildi ya da yanlış nedenle reddedildi (%u/%u); PARAM_INVALID bekleniyordu (G-Ç39)", ad,
                   (unsigned long)deger, (unsigned)r->ack, (unsigned)r->reason);
    }
}

static void t_gc39_param_dogrulama(void)
{
    ka_kur(2);
    param_set_bekle("FC_APPLY_BY_NS", UINT32_C(15999999), false);
    param_set_bekle("FC_APPLY_BY_NS", UINT32_C(16000000), true);
    param_set_bekle("FC_APPLY_BY_NS", UINT32_C(1000000000), true);
    param_set_bekle("FC_APPLY_BY_NS", UINT32_C(1000000001), false);
    param_set_bekle("FC_HOLD_NS", UINT32_C(59999999), false);
    param_set_bekle("FC_HOLD_NS", UINT32_C(60000000), true);
    param_set_bekle("FC_HOLD_NS", UINT32_C(450000000), true);
    param_set_bekle("FC_HOLD_NS", UINT32_C(450000001), false);
    /* Aralık dışı değer taşıyan blok init'te reddedilir (A4-K80 (7)). */
    static uint8_t blok[IDA_PARAM_BLOB_MAX + 64u];
    static kc_pkayit_t kayit[KC_PARAM_AZAMI];
    for (int d = 0; d < 2; ++d) {
        size_t n = kc_varsayilan_kayitlar(kayit, KC_PARAM_AZAMI);
        int id = ka_param_id(d == 0 ? "FC_APPLY_BY_NS" : "FC_HOLD_NS");
        for (size_t i = 0; i < n; ++i) {
            if ((int)kayit[i].id == id) {
                kayit[i].ham = d == 0 ? UINT32_C(1000000001) : UINT32_C(59999999);
            }
        }
        size_t boy = kc_blok_kur(blok, sizeof blok, kayit, n, kc_sema_ozeti());
        ida_init_in_t ii;
        kc_init_girdi(&ii);
        ii.param_src = (uint8_t)IDA_PSRC_SIL;
        ii.param_blob = blok;
        ii.param_len = (uint32_t)boy;
        kc_ortam_kur(1);
        ida_rc_t rc = kc_init(&ii);
        KA_DOGRULA(rc == IDA_RC_PARAM_DEFAULTS, "aralık dışı %s'li blokla init %d, PARAM_DEFAULTS bekleniyordu (G-Ç39; A4-K80 (7))",
                   d == 0 ? "apply_by" : "hold", (int)rc);
    }
}

/* ---- G-Ç13, G-Ç42, G-Ç52 ------------------------------------------------------------------ */

/* Bir SAFETY_CLOSE ürettirir (acil durdurma geçişi); olayı döndürür. */
static const ka_olay_t *kapanma_uret(void)
{
    gevsek();
    ka_adimlar(2);
    ka_ayar.estop = 1u;
    (void)ka_adim();
    const ka_olay_t *e = ka_son_olay((uint8_t)IDA_EVT_SAFETY_CLOSE, -1);
    KA_DOGRULA(e != NULL && e->cagri == ka_cagri_no(), "acil durdurma geçişinde SAFETY_CLOSE çıkmadı (G-Ç15)");
    return e;
}

static void t_gc13_sonuc_kabulu(void)
{
    ka_kur(2);
    ka_oturum_baslat();
    ka_ayar.politika[IDA_EVT_SAFETY_CLOSE].kod = KA_SONUC_YOK; /* sonuçları elle veririz */
    const ka_olay_t *e = kapanma_uret();
    uint64_t id = e->e.event_id;
    int64_t td = e->e.t_decide.ns;
    uint32_t ign0 = ka_out()->status.cnt.evres_ignored;
    /* (1) yabancı kimlik: yok sayılır. */
    ida_in_t *in = ka_girdi();
    ka_sonuc_ekle(in, id + 1000u, (uint8_t)IDA_EVR_APPLIED, td + KA_MS);
    (void)ka_cagir();
    KA_DOGRULA(ka_out()->status.cnt.evres_ignored == ign0 + 1u, "yabancı event_id'li sonuç evres_ignored'u artırmadı (G-Ç13)");
    KA_DOGRULA(ka_out()->fc.last_event_result == 0u, "yabancı sonuç last_event_result'ı değiştirdi (G-Ç13)");
    /* (2) bekleyen olayın kimliğiyle tanımsız kod: yok sayılır, olay beklemeyi sürdürür. */
    in = ka_girdi();
    ka_sonuc_ekle(in, id, 7u, 0);
    (void)ka_cagir();
    KA_DOGRULA(ka_out()->status.cnt.evres_ignored == ign0 + 2u, "tanımsız sonuç kodu (7) evres_ignored'u artırmadı (G-Ç13)");
    KA_DOGRULA(ka_out()->fc.last_event_result == 0u, "tanımsız sonuç kodu last_event_result'ı değiştirdi (G-Ç13)");
    /* (3) geçerli sonuç kabul edilir. */
    in = ka_girdi();
    ka_sonuc_ekle(in, id, (uint8_t)IDA_EVR_APPLIED, td + KA_MS);
    (void)ka_cagir();
    KA_DOGRULA(ka_out()->status.cnt.evres_ignored == ign0 + 2u, "bekleyen olayın geçerli sonucu yok sayıldı (G-Ç13)");
    KA_DOGRULA(ka_out()->fc.last_event_result == IDA_EVR_APPLIED, "last_event_result %u, APPLIED bekleniyordu (G-Ç13, G-Ç52)",
               (unsigned)ka_out()->fc.last_event_result);
    /* (4) aynı olaya ikinci sonuç: artık beklenmiyor, yok sayılır. */
    in = ka_girdi();
    ka_sonuc_ekle(in, id, (uint8_t)IDA_EVR_REJ_SAFE, 0);
    (void)ka_cagir();
    KA_DOGRULA(ka_out()->status.cnt.evres_ignored == ign0 + 3u, "sonucu alınmış olaya ikinci sonuç yok sayılmadı (G-Ç13)");
    KA_DOGRULA(ka_out()->fc.last_event_result == IDA_EVR_APPLIED, "ikinci sonuç last_event_result'ı değiştirdi (G-Ç13)");
}

static void t_gc42_sonuc_bilinmeyen(void)
{
    ka_kur(2);
    ka_oturum_baslat();
    ka_ayar.politika[IDA_EVT_SAFETY_CLOSE].kod = KA_SONUC_YOK;
    ka_degismez_ac(false); /* bilinmeyen kapanmanın FAULT_LATCHED sonucu G-Ç50'nin testindedir */
    const ka_olay_t *e = kapanma_uret();
    uint64_t kimlik = e->e.event_id;
    int64_t t0 = e->now_w;
    uint32_t apply_by = e->e.apply_by_ns;
    uint32_t unk0 = ka_out()->status.cnt.event_unknown;
    int64_t sinir = t0 + (int64_t)apply_by + (int64_t)IDA_EVT_RESULT_MARGIN_NS;
    bool goruldu = false;
    bool log = false;
    for (int k = 0; k < 60; ++k) {
        size_t n_once = ka_olay_sayisi();
        (void)ka_adim();
        /* W5-K11 (a): sonucu bilinmeyen kapatma yinelenir; yinelenen kapatma ayrı bir olaydır.
           Onun sonucu verilir ki sayaç yalnız sınanan olayı saysın. */
        for (size_t i = n_once; i < ka_olay_sayisi(); ++i) {
            const ka_olay_t *y = ka_olay_no(i);
            if (y != NULL && y->e.event_id != kimlik && y->e.kind == IDA_EVT_SAFETY_CLOSE) {
                ka_sonuc_planla(y->e.event_id, (uint8_t)IDA_EVR_APPLIED, 1u);
            }
        }
        int64_t now = ka_simdi();
        uint32_t unk = ka_out()->status.cnt.event_unknown;
        if (now < sinir - KA_P) {
            KA_DOGRULA(unk == unk0, "%lld ms'de (sınır %lld ms) event_unknown arttı (G-Ç42: apply_by + 100 ms içinde değil)",
                       (long long)((now - t0) / KA_MS), (long long)((sinir - t0) / KA_MS));
        }
        if (unk == unk0 + 1u && !goruldu) {
            goruldu = true;
            log = kc_gunluk_say(ka_out(), (uint16_t)IDA_LOG_EVENT_UNKNOWN) == 1;
        }
        if (now > sinir) {
            KA_DOGRULA(goruldu, "apply_by + 100 ms (%lld ms) geçtiği halde event_unknown artmadı (G-Ç42; A4-K70)",
                       (long long)((sinir - t0) / KA_MS));
        }
        KA_DOGRULA(unk <= unk0 + 1u,
                   "tek olay için event_unknown birden çok arttı (G-Ç42; W5-K11 (a)'nın yinelediği kapatmalar APPLIED aldı)");
    }
    KA_DOGRULA(goruldu && log, "bilinmeyen olay için EVENT_UNKNOWN günlüğü yok (3.15; A4-K70)");
    ka_degismez_ac(true);
}

static void t_gc52_son_olay(void)
{
    ka_kur(2);
    ka_oturum_baslat();
    ka_ayar.politika[IDA_EVT_SAFETY_CLOSE].kod = KA_SONUC_YOK;
    const ka_olay_t *e1 = kapanma_uret();
    uint64_t id1 = e1->e.event_id;
    int64_t td1 = e1->e.t_decide.ns;
    KA_DOGRULA(ka_out()->fc.last_event_id == id1 && ka_out()->fc.last_event_result == 0u,
               "ilk olaydan sonra last_event_id/result (G-Ç52)");
    const ka_olay_t *e2 = kapanma_uret();
    uint64_t id2 = e2->e.event_id;
    KA_DOGRULA(ka_out()->fc.last_event_id == id2 && ka_out()->fc.last_event_result == 0u,
               "ikinci olaydan sonra last_event_id %llx / result %u (G-Ç52: son çıkan olayınkiler)",
               (unsigned long long)ka_out()->fc.last_event_id, (unsigned)ka_out()->fc.last_event_result);
    ida_in_t *in = ka_girdi();
    ka_sonuc_ekle(in, id1, (uint8_t)IDA_EVR_APPLIED, td1 + KA_MS);
    (void)ka_cagir();
    KA_DOGRULA(ka_out()->fc.last_event_id == id2 && ka_out()->fc.last_event_result == 0u,
               "önceki olayın sonucu last_event_result'ı değiştirdi (G-Ç52: son çıkan olayın sonucu)");
    in = ka_girdi();
    ka_sonuc_ekle(in, id2, (uint8_t)IDA_EVR_REJ_EXPIRED, 0);
    (void)ka_cagir();
    KA_DOGRULA(ka_out()->fc.last_event_id == id2 && ka_out()->fc.last_event_result == IDA_EVR_REJ_EXPIRED,
               "son olayın sonucu last_event_result'a yazılmadı (G-Ç52)");
}

/* ---- G-Ç26 ----------------------------------------------------------------------------------- */

static void t_gc26_core_ok_dis_neden(void)
{
    ka_kur(2);
    ka_oturum_ac();
    ka_arac_silahla();
    ka_degismez_ac(false);
    for (int k = 0; k < 2400; ++k) {
        int faz = k / 300;
        ka_ayar.emniyet = faz != 1;              /* emniyet örneği kesilir (bayatlık) */
        ka_ayar.tick = faz == 2 && (k % 2 == 0);  /* IMU kesintisi */
        ka_ayar.imu_bozuk = faz == 3 && (k % 3 == 0);
        ka_ayar.imu_gecikme = faz == 4 ? 25 * KA_MS : 0;
        ka_ayar.heartbeat = faz != 5;              /* yer istasyonu kaybı */
        ka_ayar.link_state = (uint8_t)(faz == 6 ? IDA_LINK_NONE : IDA_LINK_RUNNING);
        ka_ayar.politika[IDA_EVT_SAFETY_CLOSE].kod = faz == 7 ? KA_SONUC_YOK : (uint8_t)IDA_EVR_APPLIED;
        ka_ayar.estop = (uint8_t)(faz == 7 && (k % 40) < 20 ? 1u : 0u);
        (void)ka_adim();
        KA_DOGRULA(ka_out()->health.core_ok, "dış neden (faz %d) core_ok'u düşürdü (G-Ç26; A4-K89)", faz);
        KA_DOGRULA(kc_gunluk_say(ka_out(), (uint16_t)IDA_LOG_CORE_INTEGRITY) == 0, "dış nedenle CORE_INTEGRITY günlüğü (G-Ç26)");
    }
    ka_degismez_ac(true);
}

/* ---- G-Ç28 ----------------------------------------------------------------------------------- */

static int param_sinifi(const char *ad, uint8_t *tur)
{
    int id = ka_param_id(ad);
    if (id < 0) {
        return -1;
    }
    uint16_t n = ida_core_param_count();
    for (uint16_t i = 0; i < n; ++i) {
        ida_param_desc_t d;
        memset(&d, 0, sizeof d);
        if (ida_core_param_desc(i, &d) == IDA_RC_OK && d.id == (uint16_t)id) {
            *tur = d.type;
            return (int)d.cls;
        }
    }
    return -1;
}

static void t_gc28_param_sinifi(void)
{
    /* Adlar: FC_APPLY_BY_NS ve FC_HOLD_NS A4 §3.9'da; T_emn_bayat W5-ortak'ın A4 parametre tablosu
       notunda (W5-K4, K6) adıyla geçer. T_hb_fc ve T_pencere'nin şema adı sözleşmede
       sabitlenmedi: şemada bu adlarla varsa onlar da sınanır (rapor). */
    static const char *const zorunlu[] = {"FC_APPLY_BY_NS", "FC_HOLD_NS", "T_emn_bayat"};
    static const char *const istege[] = {"T_hb_fc", "T_pencere"};
    const char *adlar[5];
    size_t n = 0;
    for (size_t i = 0; i < 3u; ++i) {
        uint8_t tur = 0;
        int cls = param_sinifi(zorunlu[i], &tur);
        KA_DOGRULA(cls >= 0, "emniyet zinciri parametresi '%s' şemada yok (G-Ç28; W5-K4, W5-K6)", zorunlu[i]);
        KA_DOGRULA(cls == IDA_PCLS_B, "'%s' sınıfı %d, IDA_PCLS_B bekleniyordu (G-Ç28; W5-K6)", zorunlu[i], cls);
        adlar[n++] = zorunlu[i];
    }
    for (size_t i = 0; i < 2u; ++i) {
        uint8_t tur = 0;
        int cls = param_sinifi(istege[i], &tur);
        if (cls >= 0) {
            KA_DOGRULA(cls == IDA_PCLS_B, "'%s' sınıfı %d, IDA_PCLS_B bekleniyordu (G-Ç28)", istege[i], cls);
            adlar[n++] = istege[i];
        } else {
            TEST_MESSAGE("G-Ç28: şemada bu adla parametre yok; sınıfı sınanamadı");
            TEST_MESSAGE(istege[i]);
        }
    }
    /* Silahlıyken PARAM_SET ve bekleyen değeri taşıyan PARAM_COMMIT REJECTED / ARMED. */
    ka_kur(2);
    ka_oturum_ac();
    /* Silahsızken bekleyen bir değer bırak, sonra silahlan: commit ARMED alır. */
    {
        ida_in_t *in = ka_girdi();
        uint32_t cid = ka_yeni_id();
        ida_cmd_t *c = kc_komut_ekle(in, cid, (uint8_t)IDA_CMD_PARAM_SET);
        c->u.param.id = (uint16_t)ka_param_id("FC_HOLD_NS");
        c->u.param.type = (uint8_t)IDA_PT_U32;
        c->u.param.v.u32 = UINT32_C(100000000);
        (void)ka_cagir();
        KA_SONUC(cid, IDA_ACK_ACCEPTED, IDA_REASON_NONE, "silahsızken PARAM_SET(FC_HOLD_NS)");
    }
    ka_arac_silahla();
    for (size_t i = 0; i < n; ++i) {
        uint8_t tur = (uint8_t)IDA_PT_U32;
        (void)param_sinifi(adlar[i], &tur);
        uint32_t once = ka_param_u32(adlar[i], 0u);
        ida_in_t *in = ka_girdi();
        uint32_t cid = ka_yeni_id();
        ida_cmd_t *c = kc_komut_ekle(in, cid, (uint8_t)IDA_CMD_PARAM_SET);
        c->u.param.id = (uint16_t)ka_param_id(adlar[i]);
        c->u.param.type = tur;
        c->u.param.v.u32 = once;
        (void)ka_cagir();
        KA_DOGRULA(ka_out()->armed, "araç silahsızlandı");
        KA_SONUC(cid, IDA_ACK_REJECTED, IDA_REASON_ARMED, adlar[i]);
    }
    ida_in_t *in = ka_girdi();
    uint32_t com = ka_komut(in, (uint8_t)IDA_CMD_PARAM_COMMIT);
    (void)ka_cagir();
    KA_SONUC(com, IDA_ACK_REJECTED, IDA_REASON_ARMED, "silahlıyken B sınıfı bekleyen değerli PARAM_COMMIT (G-Ç28; A4-K81)");
    KA_DOGRULA(ka_param_u32("FC_HOLD_NS", 0u) == KA_HOLD_VARSAYILAN_NS, "silahlıyken FC_HOLD_NS değişti (G-Ç28)");
}

/* ---- tablo -------------------------------------------------------------------------------------- */

static const ka_test_t TESTLER[] = {
    {"ilk_cikti", t_gc01_ilk_cikti},
    {"kilitten_init", t_gc01_kilitten_init},
    {"deger_kumesi", t_gc02_deger_kumesi},
    {"kapali_tablolar", t_gc30_kapali_tablolar},
    {"not_init", t_gc31_not_init},
    {"not_armed", t_gc03_not_armed},
    {"mod", t_gc03_mod},
    {"est", t_gc03_est},
    {"failsafe", t_gc03_failsafe},
    {"estop", t_gc03_estop},
    {"fis", t_gc03_fis},
    {"pim", t_gc03_pim},
    {"bos", t_gc03_bos},
    {"events_off", t_gc03_events_off},
    {"imu_lag", t_gc03_imu_lag},
    {"imu_stale", t_gc03_imu_stale},
    {"oturum", t_gc03_oturum},
    {"det_stale", t_gc03_det_stale},
    {"imu_stale_ilk", t_gc43_imu_stale_ilk},
    {"bos_tup", t_gc17_bos_tup},
    {"olay_yok", t_gc36_olay_yok},
    {"olay_kimligi", t_gc08_olay_kimligi},
    {"kapanma_birlesimi", t_gc09_kapanma_birlesimi},
    {"kapanma_dusmez", t_gc38_kapanma_dusmez},
    {"kapanma_alanlari", t_gc10_kapanma_alanlari},
    {"param_dogrulama", t_gc39_param_dogrulama},
    {"sonuc_kabulu", t_gc13_sonuc_kabulu},
    {"sonuc_bilinmeyen", t_gc42_sonuc_bilinmeyen},
    {"son_olay", t_gc52_son_olay},
    {"core_ok_dis_neden", t_gc26_core_ok_dis_neden},
    {"param_sinifi", t_gc28_param_sinifi},
};

int main(int argc, char **argv)
{
    return ka_main(argc, argv, __FILE__, TESTLER, sizeof TESTLER / sizeof TESTLER[0]);
}
