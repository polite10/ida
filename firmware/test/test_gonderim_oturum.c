/*
 * test_gonderim_oturum.c — gönderim yöneticisi + oturum modülü + platformun kapanış kararı
 * bütünleşik testi (sol bulgu 1; A1-K110, A1-K22, A1-K21).
 *
 * Gerçek ida_link üst katmanı (kart rolü), kartın gönderim yöneticisi (app/gonderim.c) ve ana
 * döngünün kapanış kararı (app/kapanis.c) birlikte koşar; taşıma durdurulabilir. Sim tarafı
 * gerçek A1 baytları üretir (tel katmanının sim rolündeki göndericisi) ve kartın çıkışını sim
 * rolündeki alıcıyla çözer. Çekirdek sahtedir: her zaman çerçeveli çağrıda bir çıktı ve istenirse
 * bir SAFETY_CLOSE olayı üretir.
 *
 * Karşı örnek (düzeltmeden önce): taşıma durmuşken kritik kuyruk dolunca IDA_ACTUATOR ve yeni
 * SAFETY_CLOSE kuyruğa alınamıyor, oturum RUNNING kalıyordu (kayıp sessiz). Derleme sırasında
 * IDA_TEST_KAPANIS_YOK tanımlanırsa platformun kapanış kararı devre dışı kalır ve bu test düşer.
 */
#include "gonderim.h"
#include "kapanis.h"
#include "unity.h"

#include "ida/port.h"
#include "ida_link/tel.h"
#include "ida_link/ust.h"
#include "mavlink/ida/mavlink.h"

#include <string.h>

void setUp(void);
void tearDown(void);

#define P_NS INT64_C(5000000)
#define T0_SIM INT64_C(1000000000)
#define BOOT 7u

static ida_link_ust_t l;
static ida_link_platform_t plat;
static ida_gonderim_t g;
static ida_kapanis_t kap;
static ida_link_gonderici_t sim_gon;
static ida_link_alici_t sim_al;
static ida_in_t girdi;
static ida_out_t cikti;
static int64_t saat;
static bool durdu;      /* taşıma durdu: bos() false */
static uint32_t nonce_ctr;
static uint32_t uye;
static uint32_t olay_ctr;
static bool olay_planli;
static bool rng_bozuk; /* p_nonce gerçek rastgele sayı üretemiyor */
/* Sahte çekirdeğin durumu ve platformun çağırıcı kapısı (main.c'deki çağırıcının taklidi). */
static bool cek_silahli, cek_izin;
static uint32_t cek_cagri, cek_oturum_sonu;
static bool cagirici_normal;

/* Sim'in aldığı çerçeveler (yalnız ilgilenilenler). */
static uint32_t n_actuator, n_event, n_close, n_hello;
static mavlink_ida_session_close_t son_close;
static mavlink_ida_board_hello_t son_hello;
static uint32_t bozuk;

/* ---- platform ---- */
static ida_wtime_t p_zaman(void *ctx)
{
    (void)ctx;
    ida_wtime_t t;
    t.ns = saat;
    return t;
}

static bool p_gonder(void *ctx, ida_link_sinif_t sinif, const uint8_t *c, size_t n)
{
    (void)ctx;
    return ida_gon_ekle(&g, (uint32_t)sinif, c, n, (uint64_t)(saat / 1000));
}

static void p_temizle(void *ctx)
{
    (void)ctx;
    ida_gon_temizle(&g);
}

static uint32_t p_nonce(void *ctx)
{
    (void)ctx;
    if (rng_bozuk) {
        return 0u; /* platform: RNG kurtarılamadı */
    }
    return 0x1000u + (++nonce_ctr);
}

static void p_yeniden(void *ctx)
{
    (void)ctx;
}

static void p_blok(void *ctx, uint32_t *crc, uint32_t *len, uint32_t *gen)
{
    (void)ctx;
    *crc = 0u;
    *len = 0u;
    *gen = 0u;
}

/* ---- taşıma: sim'in alıcısına ---- */
static bool t_bos(void *ctx)
{
    (void)ctx;
    return !durdu;
}

static bool t_yaz(void *ctx, const uint8_t *b, size_t n)
{
    (void)ctx;
    size_t ofs = 0u;
    while (ofs < n) {
        size_t tuk = 0u;
        ida_link_cerceve_t c;
        const ida_link_rc_t rc = ida_link_al(&sim_al, &b[ofs], n - ofs, &tuk, &c);
        ofs += tuk;
        if (rc != IDA_LINK_CERCEVE) {
            if (tuk == 0u) {
                break;
            }
            continue;
        }
        if (c.msgid == MAVLINK_MSG_ID_IDA_ACTUATOR) {
            n_actuator++;
        } else if (c.msgid == MAVLINK_MSG_ID_IDA_EVENT) {
            n_event++;
        } else if (c.msgid == MAVLINK_MSG_ID_IDA_SESSION_CLOSE) {
            n_close++;
            if (ida_link_coz(&c, c.msgid, &son_close, sizeof son_close) != IDA_LINK_TAMAM) {
                bozuk++;
            }
        } else if (c.msgid == MAVLINK_MSG_ID_IDA_BOARD_HELLO) {
            n_hello++;
            if (ida_link_coz(&c, c.msgid, &son_hello, sizeof son_hello) != IDA_LINK_TAMAM) {
                bozuk++;
            }
        }
    }
    return true;
}

static const ida_gon_tasima_t T = {NULL, t_bos, t_yaz};

/* ---- sahte çekirdek ---- */
static void sahte_cekirdek(const ida_in_t *in, ida_link_cagri_t tur, ida_out_t *out)
{
    memset(out, 0, sizeof *out);
    out->src = (uint8_t)((tur == IDA_LINK_CAGRI_STEP) ? IDA_SRC_STEP : IDA_SRC_TICK);
    out->seq = in->tf.present ? in->tf.seq : 0u;
    out->t_ref = in->tf.t;
    cek_cagri++;
    if (in->plat.link_event == IDA_LINKEV_SESSION_END) {
        /* A4-K72, K73: oturum sonu çağrısı daraltır: silahsızlanma, izinler kapanır. */
        cek_oturum_sonu++;
        cek_silahli = false;
        cek_izin = false;
    }
    out->armed = cek_silahli;
    out->is_safe = !cek_silahli;
    out->fs_level = (uint8_t)IDA_FS_NONE;
    out->thr_valid = in->tf.present;
    out->valid_for_ns = in->tf.present ? IDA_VALID_FOR_DEFAULT_NS : 0u;
    out->health.core_ok = true;
    out->health.data_ok = true;
    out->status.mode = (uint8_t)IDA_MODE_DISARMED;
    out->status.armed = cek_silahli;
    out->status.param_ok = true;
    out->status.events_ok = true;
    out->status.boot_count = BOOT;
    out->fc.state = (uint8_t)(cek_izin ? IDA_FCS_PERMIT_OPEN : (cek_silahli ? IDA_FCS_ARMED : IDA_FCS_SAFE));
    if (olay_planli && in->tf.present) {
        olay_planli = false;
        olay_ctr++;
        ida_event_t *e = &out->events[0];
        e->event_id = ((uint64_t)BOOT << 32) | (uint64_t)olay_ctr;
        e->kind = (uint8_t)IDA_EVT_SAFETY_CLOSE;
        e->tube = (uint8_t)IDA_TUBE_ALL;
        e->seq = out->seq;
        e->t_decide = out->t_ref;
        e->apply_by_ns = 20000000u;
        e->hold_ns = 0u;
        out->n_events = 1u;
    }
}

static ida_rc_t cekirdek_f(void *ctx, ida_link_cagri_t tur_, ida_in_t *in, ida_out_t *out)
{
    (void)ctx;
    sahte_cekirdek(in, tur_, out);
    return IDA_RC_OK;
}

/* Ana döngünün bir turu (main.c ile aynı sıra): poll_tx, çağrılar (take → kapı → çekirdek →
   put_out; yasak başlayınca çağırıcı güvenli beklemeye geçer), kapanış kararı, gönderim pompası. */
static void tur(void)
{
    ida_link_ust_poll_tx(&l);
    for (int i = 0; i < 16; i++) {
        const ida_kapanis_tur_t t =
            ida_kapanis_cagri(&kap, &l, cagirici_normal, &girdi, &cikti, cekirdek_f, NULL);
        if (t.tur == IDA_LINK_CAGRI_YOK) {
            break;
        }
        if (ida_kapanis_yasak(&kap)) {
            cagirici_normal = false;
        }
    }
#if !defined(IDA_TEST_KAPANIS_YOK)
    if (ida_kapanis_denetle(&kap, &g, rng_bozuk, &l) != IDA_KAPANIS_YOK) {
#if defined(IDA_TEST_ESKI_SIRA)
        cagirici_normal = false; /* karşı örnek: eski sıra, çağrı yasağı hemen */
#else
        if (ida_kapanis_yasak(&kap)) {
            cagirici_normal = false;
        }
#endif
    }
#endif
    for (int i = 0; i < 64 && ida_gon_pompa(&g, &T, (uint64_t)(saat / 1000)) != 0u; i++) {
    }
}

static void sim_yaz(uint32_t msgid, const void *m, size_t boy, bool parti_uyesi)
{
    uint8_t buf[IDA_LINK_CERCEVE_AZAMI];
    size_t n = 0u;
    TEST_ASSERT_EQUAL_INT(IDA_LINK_TAMAM, ida_link_gonder(&sim_gon, msgid, m, boy, buf, sizeof buf, &n));
    if (parti_uyesi) {
        uye++;
    }
    ida_wtime_t t;
    t.ns = saat;
    ida_link_ust_rx(&l, buf, n, t);
}

static void tf(uint32_t seq)
{
    mavlink_ida_time_frame_t m;
    memset(&m, 0, sizeof m);
    m.t_sim_ns = T0_SIM + (int64_t)seq * P_NS;
    m.batch_seq = seq;
    m.sess = ida_link_ust_sess(&l);
    m.n_members = (uint8_t)uye;
    m.run_mode = (uint8_t)IDAW_RUN_MODE_HIL_1X;
    uye = 0u;
    sim_yaz(MAVLINK_MSG_ID_IDA_TIME_FRAME, &m, sizeof m, false);
}

static void el_sikis(uint32_t nonce_sim, uint16_t sess)
{
    mavlink_ida_sim_hello_t h;
    memset(&h, 0, sizeof h);
    h.dialect_hash = IDA_LINK_DIALECT_HASH;
    h.nonce = nonce_sim;
    h.run_id = 1u;
    h.ctrl_period_ns = 5000000u;
    h.proto_major = 1u;
    sim_yaz(MAVLINK_MSG_ID_IDA_SIM_HELLO, &h, sizeof h, false);
    tur();
    TEST_ASSERT_EQUAL_UINT32(nonce_sim, son_hello.peer_nonce);
    mavlink_ida_session_accept_t a;
    memset(&a, 0, sizeof a);
    a.run_id = 1u;
    a.nonce_sim = nonce_sim;
    a.nonce_board = son_hello.nonce;
    a.sess = sess;
    a.result = (uint8_t)IDAW_SESSION_RESULT_OK;
    a.run_mode = (uint8_t)IDAW_RUN_MODE_HIL_1X;
    sim_yaz(MAVLINK_MSG_ID_IDA_SESSION_ACCEPT, &a, sizeof a, false);
    TEST_ASSERT_EQUAL_INT((int)IDA_LINK_UST_RUNNING, (int)ida_link_ust_durum(&l));
}

/* Bir periyot: saat ilerler, TF gelir, tur. */
static void periyot(uint32_t seq)
{
    saat += P_NS;
    tf(seq);
    tur();
}

void setUp(void)
{
    memset(&l, 0, sizeof l);
    ida_gon_kur(&g);
    ida_kapanis_kur(&kap);
    saat = INT64_C(2000000000);
    durdu = false;
    nonce_ctr = 0u;
    uye = 0u;
    olay_ctr = 0u;
    olay_planli = false;
    rng_bozuk = false;
    cek_silahli = false;
    cek_izin = false;
    cek_cagri = 0u;
    cek_oturum_sonu = 0u;
    cagirici_normal = true;
    n_actuator = n_event = n_close = n_hello = 0u;
    bozuk = 0u;
    memset(&son_close, 0, sizeof son_close);
    memset(&son_hello, 0, sizeof son_hello);
    TEST_ASSERT_EQUAL_INT(IDA_LINK_TAMAM, ida_link_gonderici_kur(&sim_gon, IDA_LINK_ROL_SIM));
    TEST_ASSERT_EQUAL_INT(IDA_LINK_TAMAM, ida_link_alici_kur(&sim_al, IDA_LINK_ROL_SIM));
    plat.ctx = NULL;
    plat.zaman = p_zaman;
    plat.gonder = p_gonder;
    plat.kuyruk_temizle = p_temizle;
    plat.nonce = p_nonce;
    plat.yeniden_baslat = p_yeniden;
    plat.kalici_blok = p_blok;
    ida_link_ust_ayar_t a;
    memset(&a, 0, sizeof a);
    a.rol = IDA_LINK_UST_ROL_KART;
    a.boot_count = BOOT;
    a.boot_count_ok = true;
    a.reset_cause = (uint8_t)IDA_RESET_POWER_ON;
    a.param_src = (uint8_t)IDA_PSRC_NONE;
    a.init_rc = (uint8_t)IDA_RC_OK;
    a.cekirdek.port_major = (uint16_t)IDA_PORT_VERSION_MAJOR;
    a.cekirdek.port_minor = (uint16_t)IDA_PORT_VERSION_MINOR;
    a.cekirdek.core_size = 1024u;
    a.cekirdek.core_align = 8u;
    a.cekirdek.param_schema_hash = UINT64_C(0x1122334455667788);
    a.cekirdek.core_version = 1u;
    a.fw_version = 1u;
    a.fw_git = 0xABCDEF01u;
    a.t_bag_ns = IDA_LINK_UST_T_BAG_MIN_NS;
    a.esc_alt_us = 1100u;
    a.esc_notr_us = 1500u;
    a.esc_ust_us = 1900u;
    TEST_ASSERT_EQUAL_INT(IDA_LINK_TAMAM, ida_link_ust_kur(&l, &a, &plat));
}

void tearDown(void) {}

/* Taşıma akarken her partiye bir IDA_ACTUATOR gider; kritik ret yok. */
static void test_akarken_kayip_yok(void)
{
    el_sikis(0x51000001u, 77u);
    for (uint32_t s = 1u; s <= 20u; s++) {
        periyot(s);
    }
    TEST_ASSERT_EQUAL_UINT32(20u, n_actuator);
    TEST_ASSERT_EQUAL_UINT32(0u, g.kritik_ret);
    TEST_ASSERT_FALSE(ida_gon_kritik_hata(&g));
    TEST_ASSERT_EQUAL_INT((int)IDA_LINK_UST_RUNNING, (int)ida_link_ust_durum(&l));
    TEST_ASSERT_EQUAL_UINT32(0u, bozuk);
}

/* Sol bulgu 1: taşıma durmuşken kritik kuyruk dolar ve yeni bir SAFETY_CLOSE gelir. Kayıp
   sessiz olamaz: platform güvenli kapanışa geçer, oturum biter (SESSION_CLOSE(SAFE_STATE)),
   sonraki partiler çekirdek çağrısı üretmez ve kart el sıkışmayı yanıtlamaz. */
static void test_durmus_tasimada_kritik_dolar_ve_safety_close(void)
{
    el_sikis(0x51000001u, 77u);
    periyot(1u);
    periyot(2u);
    const uint32_t act0 = n_actuator;
    durdu = true;
    uint32_t s = 3u;
    for (uint32_t i = 0u; i < IDA_GON_KRITIK_N; i++) {
        periyot(s++);
    }
    TEST_ASSERT_EQUAL_UINT32(IDA_GON_KRITIK_N, ida_gon_bekleyen(&g, IDA_GON_SINIF_KRITIK));
    TEST_ASSERT_FALSE(ida_gon_kritik_hata(&g));
    /* Kuyruk dolu: bu çağrının ACTUATOR'ı ve yeni SAFETY_CLOSE'u kuyruğa alınamaz. */
    olay_planli = true;
    periyot(s++);
    TEST_ASSERT_TRUE(ida_gon_kritik_hata(&g));
    TEST_ASSERT_TRUE(g.kritik_ret >= 1u);
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(IDA_KAPANIS_KRITIK, kap.neden,
                                    "kritik ret güvenli kapanışa geçmeli (A1-K110)");
    TEST_ASSERT_TRUE_MESSAGE(ida_link_ust_durum(&l) != IDA_LINK_UST_RUNNING,
                             "kritik çıktı kaybından sonra oturum sürmemeli");
    /* Taşıma yeniden akar: kart oturumu SAFE_STATE ile kapattığını bildirir. */
    durdu = false;
    for (int i = 0; i < 4; i++) {
        saat += P_NS;
        tur();
    }
    TEST_ASSERT_TRUE(n_close >= 1u);
    TEST_ASSERT_EQUAL_UINT8((uint8_t)IDAW_CLOSE_REASON_SAFE_STATE, son_close.reason);
    TEST_ASSERT_EQUAL_INT((int)IDA_LINK_UST_IDLE, (int)ida_link_ust_durum(&l));
    /* Sonraki partiler çağrı üretmez; kuyruktaki eski çıktılar silinmiştir. */
    const uint32_t act1 = n_actuator;
    periyot(s++);
    periyot(s++);
    TEST_ASSERT_EQUAL_UINT32(act1, n_actuator);
    TEST_ASSERT_TRUE(act1 >= act0);
    /* Güvenli beklemede SIM_HELLO yanıtlanmaz; duyurular init_rc = NOT_INIT taşır. */
    mavlink_ida_sim_hello_t h;
    memset(&h, 0, sizeof h);
    h.dialect_hash = IDA_LINK_DIALECT_HASH;
    h.nonce = 0x52000002u;
    h.run_id = 2u;
    h.ctrl_period_ns = 5000000u;
    h.proto_major = 1u;
    sim_yaz(MAVLINK_MSG_ID_IDA_SIM_HELLO, &h, sizeof h, false);
    for (int i = 0; i < 100; i++) { /* 500 ms: duyurular */
        saat += P_NS;
        tur();
    }
    TEST_ASSERT_TRUE(n_hello >= 2u);
    TEST_ASSERT_TRUE(son_hello.peer_nonce != 0x52000002u);
    TEST_ASSERT_EQUAL_UINT8((uint8_t)IDA_RC_NOT_INIT, son_hello.init_rc);
    TEST_ASSERT_EQUAL_UINT32(0u, bozuk);
}

/* Sol bulgu 1, ikinci denetim (EMNİYET): çekirdek silahlı ve izin açıkken kritik ret olur.
   Sıra: girdiler engellenir → oturum sonu çağrısı çalışabilir çekirdeğe teslim edilir (çekirdek
   silahsızlanır, izinler kapanır) → ancak sonra çağrı yasağı. Teslim gönderimin başarısına bağlı
   değildir (taşıma durmuş kalır). */
static void test_silahli_kritik_ret_oturum_sonu_teslim(void)
{
    el_sikis(0x51000001u, 77u);
    cek_silahli = true;
    cek_izin = true;
    periyot(1u);
    periyot(2u);
    TEST_ASSERT_TRUE(cikti.armed);
    durdu = true; /* taşıma durur ve öyle kalır */
    uint32_t s = 3u;
    for (uint32_t i = 0u; i < IDA_GON_KRITIK_N; i++) {
        periyot(s++);
    }
    TEST_ASSERT_FALSE(ida_gon_kritik_hata(&g));
    periyot(s++); /* kritik ret ve kapanışın 1. adımı */
    TEST_ASSERT_TRUE(ida_gon_kritik_hata(&g));
    TEST_ASSERT_EQUAL_UINT8(IDA_KAPANIS_KRITIK, kap.neden);
    saat += P_NS;
    tur(); /* oturum sonu çağrısı */
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(1u, cek_oturum_sonu,
                                     "oturum sonu çağrısı çekirdeğe teslim edilmeli (A1-K107, A4-K73)");
    TEST_ASSERT_FALSE_MESSAGE(cek_silahli, "çekirdek silahsızlanmalı");
    TEST_ASSERT_FALSE_MESSAGE(cek_izin, "izinler kapanmalı");
    TEST_ASSERT_FALSE(cikti.status.armed);
    TEST_ASSERT_EQUAL_UINT8((uint8_t)IDA_FCS_SAFE, cikti.fc.state);
    TEST_ASSERT_EQUAL_UINT32(1u, kap.oturum_sonu_teslim);
    TEST_ASSERT_TRUE(ida_kapanis_yasak(&kap));
    TEST_ASSERT_FALSE(cagirici_normal);
    /* Sonra hiçbir çekirdek çağrısı yok (taşıma akınca da, yeni partilerde de). */
    const uint32_t cagri0 = cek_cagri;
    durdu = false;
    periyot(s++);
    periyot(s++);
    for (int i = 0; i < 50; i++) {
        saat += P_NS;
        tur();
    }
    TEST_ASSERT_EQUAL_UINT32(cagri0, cek_cagri);
    TEST_ASSERT_TRUE(ida_link_ust_durum(&l) != IDA_LINK_UST_RUNNING);
}

/* Sol bulgu 4: RNG kurtarılamazsa sabit nonce ile el sıkışma sürdürülmez. Oturum modülü
   nonce 0'ı birkaç kez alınca sabit bir değer kullanır; platform aynı turda güvenli beklemeye
   geçer ve o duyuruyu kuyruktan siler; SESSION_ACCEPT oturum kurmaz. */
static void test_rng_hatasi_el_sikismayi_durdurur(void)
{
    rng_bozuk = true;
    mavlink_ida_sim_hello_t h;
    memset(&h, 0, sizeof h);
    h.dialect_hash = IDA_LINK_DIALECT_HASH;
    h.nonce = 0x53000003u;
    h.run_id = 1u;
    h.ctrl_period_ns = 5000000u;
    h.proto_major = 1u;
    sim_yaz(MAVLINK_MSG_ID_IDA_SIM_HELLO, &h, sizeof h, false);
    tur();
    TEST_ASSERT_EQUAL_UINT8(IDA_KAPANIS_RNG, kap.neden);
    TEST_ASSERT_TRUE_MESSAGE(son_hello.peer_nonce != 0x53000003u,
                             "sabit nonce'lu el sıkışma yanıtı gönderilmemeli (A1-K13)");
    mavlink_ida_session_accept_t a;
    memset(&a, 0, sizeof a);
    a.run_id = 1u;
    a.nonce_sim = 0x53000003u;
    a.nonce_board = 1u;
    a.sess = 5u;
    a.result = (uint8_t)IDAW_SESSION_RESULT_OK;
    a.run_mode = (uint8_t)IDAW_RUN_MODE_HIL_1X;
    sim_yaz(MAVLINK_MSG_ID_IDA_SESSION_ACCEPT, &a, sizeof a, false);
    for (int i = 0; i < 60; i++) {
        saat += P_NS;
        tur();
    }
    TEST_ASSERT_TRUE(ida_link_ust_durum(&l) != IDA_LINK_UST_RUNNING);
    TEST_ASSERT_TRUE(n_hello >= 1u);
    TEST_ASSERT_EQUAL_UINT8((uint8_t)IDA_RC_NOT_INIT, son_hello.init_rc);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_akarken_kayip_yok);
    RUN_TEST(test_durmus_tasimada_kritik_dolar_ve_safety_close);
    RUN_TEST(test_rng_hatasi_el_sikismayi_durdurur);
    RUN_TEST(test_silahli_kritik_ret_oturum_sonu_teslim);
    return UNITY_END();
}
