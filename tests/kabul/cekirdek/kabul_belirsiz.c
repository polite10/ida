/*
 * Sözleşme belirsiz — A4'ün iki biçimde okunabildiği vakalar. Her test gözlenen davranışı
 * iletiyle (TEST_MESSAGE) yazar ve kırmızı olmaz; liste raporda kural numarasıyla verilir
 * (A4 dondurulmadan önce girdi). Bu vakalarda düzeneğin genel denetimi yalnız belirsiz
 * noktayı içeren çağrıda kapatılır.
 */
#include <stdio.h>
#include <string.h>

#include "kabul_cekirdek.h"
#include "unity.h"

static ida_in_t g_in;
static char g_msj[512];
static kc_pkayit_t g_k[KC_PARAM_AZAMI];
static uint8_t g_blok[IDA_PARAM_BLOB_MAX + 64u];

void setUp(void)
{
    kc_ortam_kur(KC_N_AZAMI);
}

void tearDown(void)
{
    kc_genel_denetim_ac(true);
}

static void yaz(const char *kod, const char *ne)
{
    (void)snprintf(g_msj, sizeof g_msj, "BELİRSİZ %s — gözlenen: %s", kod, ne);
    TEST_MESSAGE(g_msj);
}

/* B1 (A4-K09, K10): init'in now_w'si geri adım izinin tabanı mı? */
static void test_belirsiz_B1_init_now_w_geri_adim_tabani(void)
{
    char ne[200];
    ida_init_in_t ii;
    kc_init_girdi(&ii);
    ii.now_w.ns = KC_T0 + 50 * KC_MS;
    (void)kc_init(&ii);
    kc_genel_denetim_ac(false);
    kc_in_step(&g_in, 1u, KC_T0 + 10 * KC_MS);
    ida_rc_t rc = kc_step(&g_in);
    (void)snprintf(ne, sizeof ne, "init now_w = T0+50 ms, ilk step now_w = T0+10 ms: dönüş %d, wall_backstep %lu, "
                                  "WALL_BACKSTEP günlüğü %d",
                   (int)rc, (unsigned long)kc_out()->status.cnt.wall_backstep,
                   kc_gunluk_say(kc_out(), (uint16_t)IDA_LOG_WALL_BACKSTEP));
    yaz("B1 A4-K10", ne);
}

/* B2 (3.8 t_w, 3.11 (b2)): bekletilen günlüğün t_w'si (b2) çağrısınınki mi, çıktığı
   çağrınınki mi? */
static void test_belirsiz_B2_bekletilen_gunlugun_t_w(void)
{
    char ne[200];
    kc_hazirla(2);
    kc_in_izgara(&g_in, KC_T0 + 40 * KC_MS);
    TEST_ASSERT_EQUAL_INT(IDA_RC_OK, kc_tick(&g_in));
    kc_in_izgara(&g_in, KC_T0 + 30 * KC_MS);
    (void)kc_cagir_out_null(KC_TICK, &g_in);
    kc_in_izgara(&g_in, KC_T0 + 60 * KC_MS);
    TEST_ASSERT_EQUAL_INT(IDA_RC_OK, kc_tick(&g_in));
    const ida_log_t *l = kc_gunluk_bul(kc_out(), (uint16_t)IDA_LOG_WALL_BACKSTEP);
    (void)snprintf(ne, sizeof ne, "(b2) now_w T0+30 ms (kullanılan T0+40), çıktı çağrısı T0+60 ms: WALL_BACKSTEP t_w = %lld ns",
                   l != NULL ? (long long)(l->t_w.ns - KC_T0) : -1LL);
    yaz("B2 3.8/3.11(b2)", ne);
}

/* B3 (3.8 t_w, 3.15): RESET_INFO ve PARAM_LOAD'ın t_w'si init'in mi ilk çağrının mı? */
static void test_belirsiz_B3_init_gunluklerinin_t_w(void)
{
    char ne[200];
    kc_init_bloksuz();
    kc_in_step(&g_in, 1u, KC_T0 + 5 * KC_MS);
    TEST_ASSERT_EQUAL_INT(IDA_RC_OK, kc_step(&g_in));
    const ida_log_t *r = kc_gunluk_bul(kc_out(), (uint16_t)IDA_LOG_RESET_INFO);
    const ida_log_t *p = kc_gunluk_bul(kc_out(), (uint16_t)IDA_LOG_PARAM_LOAD);
    (void)snprintf(ne, sizeof ne, "init now_w T0, ilk çağrı T0+5 ms: RESET_INFO t_w = T0%+lld ns, PARAM_LOAD t_w = T0%+lld ns",
                   r != NULL ? (long long)(r->t_w.ns - KC_T0) : -1LL, p != NULL ? (long long)(p->t_w.ns - KC_T0) : -1LL);
    yaz("B3 3.8/3.15", ne);
}

/* B4 (4.15): kurulmamış örnek + bilinmeyen id: NOT_INIT mi NOT_FOUND mu (sıra yazılı değil). */
static void test_belirsiz_B4_param_get_kurulmamis_bilinmeyen_id(void)
{
    char ne[160];
    ida_param_value_t v;
    ida_rc_t rc = ida_core_param_get(kc_core(0), 0xFFFFu, &v);
    (void)snprintf(ne, sizeof ne, "param_get(kurulmamış, 0xFFFF) = %d", (int)rc);
    yaz("B4 4.15", ne);
}

/* B5 (4.15): param_desc(index ≥ sayı, NULL): BAD_INPUT mi NOT_FOUND mu. */
static void test_belirsiz_B5_param_desc_null_ve_sinir_disi(void)
{
    char ne[160];
    ida_rc_t rc = ida_core_param_desc(ida_core_param_count(), NULL);
    (void)snprintf(ne, sizeof ne, "param_desc(count, NULL) = %d", (int)rc);
    yaz("B5 4.15", ne);
}

/* B6 (A4-K10): aralık dışı now_w taşıyan çağrı geri adım sayılır mı ("aralık dışı now_w izi
   değiştirmez" yazılı; sayım yazılı değil). */
static void test_belirsiz_B6_aralik_disi_now_w_geri_adim_mi(void)
{
    char ne[200];
    kc_hazirla(2);
    kc_genel_denetim_ac(false);
    kc_in_izgara(&g_in, -1);
    ida_rc_t rc = kc_tick(&g_in);
    uint32_t w = kc_out()->status.cnt.wall_backstep;
    int l = kc_gunluk_say(kc_out(), (uint16_t)IDA_LOG_WALL_BACKSTEP);
    (void)snprintf(ne, sizeof ne, "now_w = -1: dönüş %d, wall_backstep %lu, WALL_BACKSTEP günlüğü %d", (int)rc,
                   (unsigned long)w, l);
    yaz("B6 A4-K10", ne);
}

/* B7 (A4-K80 (6)): şemada olmayan ek kayıt (sıralı): "şemadaki her parametre tam bir kez"
   ek kaydı yasaklamaz. */
static void test_belirsiz_B7_blokta_semada_olmayan_kayit(void)
{
    char ne[200];
    size_t n = kc_varsayilan_kayitlar(g_k, KC_PARAM_AZAMI);
    uint16_t azami = 0u;
    for (size_t i = 0; i < n; ++i) {
        if (g_k[i].id > azami) {
            azami = g_k[i].id;
        }
    }
    g_k[n].id = (uint16_t)(azami + 1u);
    g_k[n].type = (uint8_t)IDA_PT_U32;
    g_k[n].rsv = 0u;
    g_k[n].ham = 1u;
    size_t boy = kc_blok_kur(g_blok, sizeof g_blok, g_k, n + 1u, kc_sema_ozeti());
    ida_init_in_t ii;
    kc_init_girdi(&ii);
    ii.param_blob = g_blok;
    ii.param_len = (uint32_t)boy;
    ida_rc_t rc = kc_init(&ii);
    kc_genel_denetim_ac(false);
    kc_in_izgara(&g_in, KC_T0 + KC_P);
    (void)kc_tick(&g_in);
    const ida_log_t *p = kc_gunluk_bul(kc_out(), (uint16_t)IDA_LOG_PARAM_LOAD);
    (void)snprintf(ne, sizeof ne, "ek kayıt (id %u): init %d, PARAM_LOAD arg1 %ld", (unsigned)(azami + 1u), (int)rc,
                   p != NULL ? (long)p->arg1 : -1L);
    yaz("B7 A4-K80(6)", ne);
}

/* B8 (3.2 / A4-K33 / A4-K103): window_ns = 0 olan IMU: 3.2 "örnek reddedilir" der, A4-K33'ün
   koşul listesinde ve A4-K103'ün IMU_REJECTED tanımında (K30/K33) window_ns yok. */
static void test_belirsiz_B8_imu_window_ns_sifir(void)
{
    char ne[160];
    kc_hazirla(2);
    kc_genel_denetim_ac(false);
    kc_in_step(&g_in, 3u, KC_T0 + 3 * KC_P);
    g_in.imu.hdr.window_ns = 0u;
    ida_rc_t rc = kc_step(&g_in);
    (void)snprintf(ne, sizeof ne, "IMU window_ns = 0: dönüş %d, sample_rejected %lu", (int)rc,
                   (unsigned long)kc_out()->status.cnt.sample_rejected);
    yaz("B8 3.2/A4-K33/A4-K103", ne);
}

/* B9 (3.8 reason): ACCEPTED sonuçta reason değeri (0 olması yazılı değil). */
static void test_belirsiz_B9_kabul_sonucunun_reason_degeri(void)
{
    char ne[200];
    kc_hazirla(2);
    kc_in_step(&g_in, 3u, KC_T0 + 3 * KC_P);
    kc_mandal_kur(&g_in, (uint8_t)IDA_NARROW_DISARM, 9u);
    (void)kc_komut_ekle(&g_in, 10u, (uint8_t)IDA_CMD_DISARM);
    TEST_ASSERT_EQUAL_INT(IDA_RC_OK, kc_step(&g_in));
    (void)snprintf(ne, sizeof ne, "mandal ACCEPTED reason %u; cmds[] DISARM ACCEPTED reason %u",
                   (unsigned)kc_out()->results[0].reason, (unsigned)kc_out()->results[1].reason);
    yaz("B9 3.8/A4-K120", ne);
}

/* B10 (3.8 cmd_rejected): reddedilen çağrının CALL_REJECTED sonuçları sayılır mı. */
static void test_belirsiz_B10_cmd_rejected_call_rejected_sayar_mi(void)
{
    char ne[160];
    kc_hazirla(2);
    kc_in_step(&g_in, 2u, KC_T0 + 3 * KC_P);
    (void)kc_komut_ekle(&g_in, 11u, (uint8_t)IDA_CMD_HEARTBEAT);
    (void)kc_komut_ekle(&g_in, 12u, (uint8_t)IDA_CMD_ARM);
    TEST_ASSERT_EQUAL_INT(IDA_RC_DUP_SEQ, kc_step(&g_in));
    (void)snprintf(ne, sizeof ne, "DUP_SEQ çağrısında iki CALL_REJECTED: cmd_rejected farkı %lu",
                   (unsigned long)KC_FARK(cmd_rejected));
    yaz("B10 3.8", ne);
}

/* B11 (A4-K73): SESSION_START olmadan ikinci SESSION_END yeni SAFETY_CLOSE üretir mi. */
static void test_belirsiz_B11_ikinci_session_end(void)
{
    char ne[160];
    kc_hazirla(2);
    kc_in_oturum_sonu(&g_in, KC_T0 + 3 * KC_P);
    TEST_ASSERT_EQUAL_INT(IDA_RC_OK, kc_tick(&g_in));
    kc_genel_denetim_ac(false);
    kc_in_oturum_sonu(&g_in, KC_T0 + 4 * KC_P);
    ida_rc_t rc = kc_tick(&g_in);
    (void)snprintf(ne, sizeof ne, "art arda ikinci SESSION_END: dönüş %d, olay %u", (int)rc,
                   (unsigned)kc_out()->n_events);
    yaz("B11 A4-K73", ne);
}

/* B12 (A4-K126 "bayt bayt"): dolgu baytları da sabit mi (alan alan eşitlik ayrı sınandı). */
static ida_out_t g_o[2];

static void test_belirsiz_B12_not_init_dolgu_baytlari(void)
{
    char ne[160];
    kc_in_izgara(&g_in, KC_T0);
    memset(&g_o[0], 0x00, sizeof g_o[0]);
    memset(&g_o[1], 0xFF, sizeof g_o[1]);
    (void)ida_core_tick(kc_core(0), &g_in, &g_o[0]);
    (void)ida_core_tick(kc_core(1), &g_in, &g_o[1]);
    size_t fark = 0;
    const unsigned char *a = (const unsigned char *)&g_o[0];
    const unsigned char *b = (const unsigned char *)&g_o[1];
    for (size_t i = 0; i < sizeof g_o[0]; ++i) {
        if (a[i] != b[i]) {
            ++fark;
        }
    }
    (void)snprintf(ne, sizeof ne, "NOT_INIT çıktısında ham bayt farkı (dolgu dahil) %lu / %lu",
                   (unsigned long)fark, (unsigned long)sizeof g_o[0]);
    yaz("B12 A4-K126", ne);
}

/* B13 (A4-K46 "muaf"): cmds[]'teki daraltıcı komut yineleme penceresine girer mi. */
static void test_belirsiz_B13_daraltici_komut_pencereye_girer_mi(void)
{
    char ne[160];
    kc_hazirla(2);
    kc_in_step(&g_in, 3u, KC_T0 + 3 * KC_P);
    (void)kc_komut_ekle(&g_in, 777u, (uint8_t)IDA_CMD_DISARM);
    TEST_ASSERT_EQUAL_INT(IDA_RC_OK, kc_step(&g_in));
    kc_in_step(&g_in, 4u, KC_T0 + 4 * KC_P);
    (void)kc_komut_ekle(&g_in, 777u, (uint8_t)IDA_CMD_HEARTBEAT);
    TEST_ASSERT_EQUAL_INT(IDA_RC_OK, kc_step(&g_in));
    (void)snprintf(ne, sizeof ne, "DISARM(777) sonrası HEARTBEAT(777): ack %u", (unsigned)kc_out()->results[0].ack);
    yaz("B13 A4-K46/K48", ne);
}

/* B14 (A4-K89 input_dropped): hata dönüşünde health.input_dropped. */
static void test_belirsiz_B14_input_dropped_hata_donusunde(void)
{
    char ne[160];
    kc_hazirla(2);
    kc_in_step(&g_in, 2u, KC_T0 + 3 * KC_P);
    g_in.plat.input_dropped = true;
    TEST_ASSERT_EQUAL_INT(IDA_RC_DUP_SEQ, kc_step(&g_in));
    bool d = kc_out()->health.input_dropped;
    kc_in_step(&g_in, 3u, KC_T0 + 4 * KC_P);
    g_in.plat.input_dropped = true;
    TEST_ASSERT_EQUAL_INT(IDA_RC_OK, kc_step(&g_in));
    (void)snprintf(ne, sizeof ne, "plat.input_dropped = 1: DUP_SEQ'te %d, OK'de %d", (int)d,
                   (int)kc_out()->health.input_dropped);
    yaz("B14 A4-K89/K102", ne);
}

/* B15 (A4-K127): yer tutucuda init sonrası kip (INIT ya da DISARMED) ve NARROW_MODE etkisi. */
static void test_belirsiz_B15_kip(void)
{
    char ne[160];
    kc_hazirla(2);
    uint8_t m0 = kc_out()->status.mode;
    kc_in_step(&g_in, 3u, KC_T0 + 3 * KC_P);
    kc_mandal_kur(&g_in, (uint8_t)IDA_NARROW_MODE, 5u);
    g_in.narrow.mode = (uint8_t)IDA_MODE_MANUAL;
    TEST_ASSERT_EQUAL_INT(IDA_RC_OK, kc_step(&g_in));
    (void)snprintf(ne, sizeof ne, "init sonrası kip %u; NARROW_MODE(MANUAL) sonrası %u", (unsigned)m0,
                   (unsigned)kc_out()->status.mode);
    yaz("B15 A4-K127 (bilgi)", ne);
}

/* B16 (3.8): boot_count_ok = 0 iken status.boot_count. */
static void test_belirsiz_B16_boot_count_ok_0(void)
{
    char ne[160];
    ida_init_in_t ii;
    kc_init_girdi(&ii);
    ii.boot_count = 1234u;
    ii.boot_count_ok = false;
    (void)kc_init(&ii);
    kc_in_izgara(&g_in, KC_T0 + KC_P);
    TEST_ASSERT_EQUAL_INT(IDA_RC_OK, kc_tick(&g_in));
    (void)snprintf(ne, sizeof ne, "boot_count 1234, boot_count_ok 0: status.boot_count %lu",
                   (unsigned long)kc_out()->status.boot_count);
    yaz("B16 3.8/A4-K61", ne);
}

/* B17 (A4-K85): tanımlayıcının min/max/def'inin type alanı. */
static void test_belirsiz_B17_desc_deger_tipleri(void)
{
    char ne[200];
    unsigned esit = 0;
    unsigned n = kc_param_sayisi();
    for (unsigned iu = 0; iu < (unsigned)(n); ++iu) {
        uint16_t i = (uint16_t)iu;
        const ida_param_desc_t *d = kc_param_desc(i);
        if (d->min.type == d->type && d->max.type == d->type && d->def.type == d->type && d->def.id == d->id) {
            ++esit;
        }
    }
    (void)snprintf(ne, sizeof ne, "%u/%u tanımlayıcıda min/max/def.type = type ve def.id = id", esit, n);
    yaz("B17 A4-K85", ne);
}

/* B18 (A4-K39, 3.15): IMU hiç gelmeden ilk çağrılarda IMU_STALE günlüğü (bayatlık init'ten
   başlar; "başladığında" günlüğü üretilir mi). */
static void test_belirsiz_B18_ilk_cagrilarda_imu_stale_gunlugu(void)
{
    char ne[200];
    int n1 = 0;
    int n2 = 0;
    kc_init_bloksuz();
    kc_in_izgara(&g_in, KC_T0 + KC_P);
    TEST_ASSERT_EQUAL_INT(IDA_RC_OK, kc_tick(&g_in));
    n1 = kc_gunluk_say(kc_out(), (uint16_t)IDA_LOG_IMU_STALE);
    kc_in_step(&g_in, 1u, KC_T0 + 2 * KC_P);
    TEST_ASSERT_EQUAL_INT(IDA_RC_OK, kc_step(&g_in));
    n2 = kc_gunluk_say(kc_out(), (uint16_t)IDA_LOG_IMU_STALE);
    (void)snprintf(ne, sizeof ne, "IMU'suz ilk tick'te IMU_STALE %d; ilk IMU kabulünde IMU_STALE %d; IMU_STALE biti %d",
                   n1, n2, (int)((kc_out()->fc.inhibit_mask & IDA_FCI_IMU_STALE) != 0u));
    yaz("B18 A4-K39/K128", ne);
}

/* B19 (A4-K122): oturum sonu çağrısındaki örnekler işlenir mi (alan kısıtı ihlali BAD değil;
   işlenip işlenmedikleri yazılı değil). */
static void test_belirsiz_B19_oturum_sonu_cagrisinin_ornekleri(void)
{
    char ne[160];
    kc_hazirla(2);
    kc_in_oturum_sonu(&g_in, KC_T0 + 3 * KC_P);
    (void)kc_ornek_ekle(&g_in, (uint8_t)IDA_SENS_MAG, 1u, 5u, KC_T0 + 2 * KC_P);
    TEST_ASSERT_EQUAL_INT(IDA_RC_OK, kc_tick(&g_in));
    kc_in_step(&g_in, 3u, KC_T0 + 4 * KC_P);
    (void)kc_ornek_ekle(&g_in, (uint8_t)IDA_SENS_MAG, 1u, 5u, KC_T0 + 2 * KC_P);
    TEST_ASSERT_EQUAL_INT(IDA_RC_OK, kc_step(&g_in));
    (void)snprintf(ne, sizeof ne, "aynı MAG örneği sonraki çağrıda: sample_rejected farkı %lu (1: oturum sonunda kabul edilmişti)",
                   (unsigned long)KC_FARK(sample_rejected));
    yaz("B19 A4-K122", ne);
}

/* B20 (A4-K69, 3.5, 3.8): bekleyen bir olayın kimliğiyle tanımsız sonuç kodu (ör. 198)
   gelirse çekirdeğin davranışı yazılı değil ((c) bunu denetlemez; fc.last_event_result'ın
   değer kümesi IDA_EVR_*). */
static void test_belirsiz_B20_tanimsiz_olay_sonucu(void)
{
    char ne[200];
    kc_hazirla(2);
    kc_in_oturum_sonu(&g_in, KC_T0 + 3 * KC_P);
    TEST_ASSERT_EQUAL_INT(IDA_RC_OK, kc_tick(&g_in));
    uint64_t id = kc_out()->fc.last_event_id;
    kc_in_izgara(&g_in, KC_T0 + 4 * KC_P);
    g_in.plat.link_event = (uint8_t)IDA_LINKEV_SESSION_START;
    g_in.n_evres = 1u;
    g_in.evres[0].event_id = id;
    g_in.evres[0].result = 198u;
    kc_genel_denetim_ac(false);
    ida_rc_t rc = kc_tick(&g_in);
    (void)snprintf(ne, sizeof ne, "SAFETY_CLOSE kimliğiyle result = 198: dönüş %d, fc.last_event_result %u, evres_ignored %lu",
                   (int)rc, (unsigned)kc_out()->fc.last_event_result,
                   (unsigned long)kc_out()->status.cnt.evres_ignored);
    yaz("B20 A4-K69/3.5", ne);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_belirsiz_B1_init_now_w_geri_adim_tabani);
    RUN_TEST(test_belirsiz_B2_bekletilen_gunlugun_t_w);
    RUN_TEST(test_belirsiz_B3_init_gunluklerinin_t_w);
    RUN_TEST(test_belirsiz_B4_param_get_kurulmamis_bilinmeyen_id);
    RUN_TEST(test_belirsiz_B5_param_desc_null_ve_sinir_disi);
    RUN_TEST(test_belirsiz_B6_aralik_disi_now_w_geri_adim_mi);
    RUN_TEST(test_belirsiz_B7_blokta_semada_olmayan_kayit);
    RUN_TEST(test_belirsiz_B8_imu_window_ns_sifir);
    RUN_TEST(test_belirsiz_B9_kabul_sonucunun_reason_degeri);
    RUN_TEST(test_belirsiz_B10_cmd_rejected_call_rejected_sayar_mi);
    RUN_TEST(test_belirsiz_B11_ikinci_session_end);
    RUN_TEST(test_belirsiz_B12_not_init_dolgu_baytlari);
    RUN_TEST(test_belirsiz_B13_daraltici_komut_pencereye_girer_mi);
    RUN_TEST(test_belirsiz_B14_input_dropped_hata_donusunde);
    RUN_TEST(test_belirsiz_B15_kip);
    RUN_TEST(test_belirsiz_B16_boot_count_ok_0);
    RUN_TEST(test_belirsiz_B17_desc_deger_tipleri);
    RUN_TEST(test_belirsiz_B18_ilk_cagrilarda_imu_stale_gunlugu);
    RUN_TEST(test_belirsiz_B19_oturum_sonu_cagrisinin_ornekleri);
    RUN_TEST(test_belirsiz_B20_tanimsiz_olay_sonucu);
    return UNITY_END();
}
