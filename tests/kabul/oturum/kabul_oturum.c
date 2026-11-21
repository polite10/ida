/*
 * kabul_oturum.c — ida_link üst katmanı kabul testlerinin ortak düzeneği (kabul_oturum.h).
 *
 * Modüle yalnız ust.h'nin işlevleriyle ve sahte platform işlev tablosuyla erişilir. Sim
 * tarafının baytları tel katmanının sim rolündeki kodlayıcısıyla üretilir; kartın gönderdiği
 * çerçeveler sim rolündeki tel alıcısıyla çözülür (tel.h).
 */
#include "kabul_oturum.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "unity.h"

/* ---- Düzenek durumu (testin kendi belleği; kitaplıkta genel durum yoktur) ---------------- */
typedef struct {
    ida_link_ust_t       l;
    ida_link_platform_t  plat;
    ida_link_ust_rol_t   rol;
    int64_t              saat;
    uint32_t             nonce_sayac;
    uint32_t             kuyruk_temizle;
    uint32_t             yeniden_baslat;
    uint32_t             bozuk;

    /* Sim tarafı */
    ida_link_gonderici_t sim_gon;
    ida_link_alici_t     sim_al;
    uint16_t             sess;
    uint32_t             uye;            /* açık penceredeki üye sayısı              */
    int64_t              son_keepalive;

    /* Tutulan çağrı */
    bool                 tutuluyor;
    ida_link_cagri_t     tut_tur;
    size_t               tut_kayit;
    ida_in_t             tut_in;

    /* Sahte çekirdek */
    bool                 olay_planli;
    uint32_t             olay_apply_by;
    bool                 sonu_olayi;
    uint32_t             olay_ctr;
    uint64_t             son_olay_id;
    int64_t              son_olay_now_w;
    uint32_t             son_seq;
    int64_t              son_t;
    uint32_t             tamamlanan_cagri;
} ko_durum_t;

static ko_durum_t ko;
static ida_in_t ko_in;
static ida_out_t ko_out;
static ko_cerceve_t ko_gunluk_d[KO_GUNLUK_AZAMI];
static size_t ko_gunluk_n;
static size_t ko_gunluk_tasan;
static ko_cagri_t ko_cagri_d[KO_CAGRI_AZAMI];
static size_t ko_cagri_n;
static ko_kimlik_t ko_kimlik_d[KO_KIMLIK_AZAMI];
static bool ko_ihlal_izni;

/* ---- Sahte platform işlevleri ------------------------------------------------------------ */
static ida_wtime_t p_zaman(void *ctx)
{
    (void)ctx;
    ida_wtime_t t;
    t.ns = ko.saat;
    return t;
}

static void kimlik_sonuc_ekle(const mavlink_ida_cmd_result_t *r, uint8_t sinif, size_t sira)
{
    if (r->cmd_id >= KO_KIMLIK_AZAMI) {
        ko.bozuk++;
        return;
    }
    ko_kimlik_t *k = &ko_kimlik_d[r->cmd_id];
    if (k->n_sonuc < KO_SONUC_AZAMI) {
        ko_sonuc_t *s = &k->sonuc[k->n_sonuc];
        s->ack = r->ack;
        s->reason = r->reason;
        s->kind = r->kind;
        s->sinif = sinif;
        s->sess = r->sess;
        s->gunluk_sira = sira;
    }
    if (k->n_sonuc < UINT16_MAX) {
        k->n_sonuc++;
    }
}

static bool p_gonder(void *ctx, ida_link_sinif_t sinif, const uint8_t *cerceve, size_t n)
{
    (void)ctx;
    size_t tuk = 0;
    ida_link_cerceve_t c;
    ida_link_rc_t rc = ida_link_al(&ko.sim_al, cerceve, n, &tuk, &c);
    if (rc != IDA_LINK_CERCEVE || tuk != n) {
        ko.bozuk++;
        return true;
    }
    size_t sira = ko_gunluk_n;
    if (ko_gunluk_n < KO_GUNLUK_AZAMI) {
        ko_cerceve_t *g = &ko_gunluk_d[ko_gunluk_n];
        g->msgid = c.msgid;
        g->sinif = (uint8_t)sinif;
        g->len = c.len;
        g->t_ns = ko.saat;
        g->cagri_sayisi = ko.tamamlanan_cagri;
        memcpy(g->yuk, c.yuk, sizeof g->yuk);
        ko_gunluk_n++;
    } else {
        ko_gunluk_tasan++;
    }
    if (c.msgid == MAVLINK_MSG_ID_IDA_CMD_RESULT) {
        mavlink_ida_cmd_result_t r;
        if (ida_link_coz(&c, MAVLINK_MSG_ID_IDA_CMD_RESULT, &r, sizeof r) == IDA_LINK_TAMAM) {
            kimlik_sonuc_ekle(&r, (uint8_t)sinif, sira);
        } else {
            ko.bozuk++;
        }
    }
    return true;
}

static void p_kuyruk_temizle(void *ctx)
{
    (void)ctx;
    ko.kuyruk_temizle++;
}

static uint32_t p_nonce(void *ctx)
{
    (void)ctx;
    ko.nonce_sayac++;
    return UINT32_C(0xB0A00000) + ko.nonce_sayac;
}

static void p_yeniden_baslat(void *ctx)
{
    (void)ctx;
    ko.yeniden_baslat++;
}

static void p_kalici_blok(void *ctx, uint32_t *crc, uint32_t *len, uint32_t *gen)
{
    (void)ctx;
    if (crc != NULL) {
        *crc = 0u;
    }
    if (len != NULL) {
        *len = 0u;
    }
    if (gen != NULL) {
        *gen = 0u;
    }
}

/* ---- Kurulum ----------------------------------------------------------------------------- */
void ko_kur(ida_link_ust_rol_t rol)
{
    memset(&ko, 0, sizeof ko);
    memset(ko_kimlik_d, 0, sizeof ko_kimlik_d);
    ko_ihlal_izni = false;
    ko_gunluk_n = 0u;
    ko_gunluk_tasan = 0u;
    ko_cagri_n = 0u;
    ko.rol = rol;
    ko.saat = (rol == IDA_LINK_UST_ROL_SIL) ? 0 : KO_T0_DUVAR;

    ko.plat.ctx = &ko;
    ko.plat.zaman = p_zaman;
    ko.plat.gonder = p_gonder;
    ko.plat.kuyruk_temizle = p_kuyruk_temizle;
    ko.plat.nonce = p_nonce;
    ko.plat.yeniden_baslat = p_yeniden_baslat;
    ko.plat.kalici_blok = p_kalici_blok;

    TEST_ASSERT_EQUAL_INT(IDA_LINK_TAMAM, ida_link_gonderici_kur(&ko.sim_gon, IDA_LINK_ROL_SIM));
    TEST_ASSERT_EQUAL_INT(IDA_LINK_TAMAM, ida_link_alici_kur(&ko.sim_al, IDA_LINK_ROL_SIM));

    ida_link_ust_ayar_t a;
    memset(&a, 0, sizeof a);
    a.rol = rol;
    a.boot_count = KO_BOOT_COUNT;
    a.boot_count_ok = true;
    a.reset_cause = (uint8_t)((rol == IDA_LINK_UST_ROL_SIL) ? IDA_RESET_SIL_START : IDA_RESET_POWER_ON);
    a.reset_flags = 0u;
    a.consecutive_faults = 0u;
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
    a.prev_sess = 0u;
    a.sessions_since_boot = 0u;
    a.last_cmd_id = 0u;
    a.t_bag_ns = IDA_LINK_UST_T_BAG_MIN_NS;
    a.esc_alt_us = 1100u;
    a.esc_notr_us = 1500u;
    a.esc_ust_us = 1900u;
    a.hata_kaydi_var = false;

    memset(&ko.l, 0, sizeof ko.l);
    TEST_ASSERT_EQUAL_INT_MESSAGE(IDA_LINK_TAMAM, ida_link_ust_kur(&ko.l, &a, &ko.plat),
                                  "ida_link_ust_kur geçerli ayarla TAMAM dönmeli");
    TEST_ASSERT_EQUAL_INT((int)IDA_LINK_UST_IDLE, (int)ida_link_ust_durum(&ko.l));
}

ida_link_ust_rol_t ko_rol(void)
{
    return ko.rol;
}

ida_link_ust_t *ko_l(void)
{
    return &ko.l;
}

void ko_saat_kur(int64_t ns)
{
    if (ns > ko.saat) {
        ko.saat = ns;
    }
}

int64_t ko_saat(void)
{
    return ko.saat;
}

void ko_bekle_ms(uint32_t ms)
{
    for (uint32_t i = 0; i < ms; ++i) {
        ko.saat += KO_MS;
        if (ida_link_ust_durum(&ko.l) == IDA_LINK_UST_RUNNING && ko.sess != 0u &&
            ko.saat - ko.son_keepalive >= 50 * KO_MS) {
            ko_keepalive(ko.sess);
        }
        ko_pompala();
    }
}

/* ---- Sim çerçeveleri ------------------------------------------------------------------- */
void ko_pencere_ihlali_izni(bool izin)
{
    ko_ihlal_izni = izin;
}

void ko_gonder_ham(uint32_t msgid, const void *mesaj, size_t boy, bool uye)
{
    /* W3-K10: take ile put_out arasında rx çağrılmaz (platform yükümlülüğü); yalnız ihlal
       vakasını sınayan test bunu bilerek yapar. */
    TEST_ASSERT_TRUE_MESSAGE(!ko.tutuluyor || ko_ihlal_izni,
                             "düzenek: take ile put_out arasında rx (W3-K10)");
    uint8_t buf[IDA_LINK_CERCEVE_AZAMI];
    size_t n = 0;
    ida_link_rc_t rc = ida_link_gonder(&ko.sim_gon, msgid, mesaj, boy, buf, sizeof buf, &n);
    TEST_ASSERT_EQUAL_INT_MESSAGE(IDA_LINK_TAMAM, rc, "sim kodlayıcısı çerçeve üretemedi");
    if (uye) {
        ko.uye++;
    }
    ida_wtime_t t;
    t.ns = ko.saat;
    ida_link_ust_rx(&ko.l, buf, n, t);
}

void ko_sim_hello(uint32_t nonce)
{
    mavlink_ida_sim_hello_t m;
    memset(&m, 0, sizeof m);
    m.dialect_hash = IDA_LINK_DIALECT_HASH;
    m.nonce = nonce;
    m.run_id = 1u;
    m.ctrl_period_ns = 5000000u;
    m.proto_major = 1u;
    m.proto_minor = 0u;
    ko_gonder_ham(MAVLINK_MSG_ID_IDA_SIM_HELLO, &m, sizeof m, false);
}

void ko_session_accept(uint32_t nonce_sim, uint32_t nonce_board, uint16_t sess, uint8_t sonuc)
{
    mavlink_ida_session_accept_t m;
    memset(&m, 0, sizeof m);
    m.run_id = 1u;
    m.nonce_sim = nonce_sim;
    m.nonce_board = nonce_board;
    m.sess = sess;
    m.result = sonuc;
    m.run_mode = (uint8_t)((ko.rol == IDA_LINK_UST_ROL_SIL) ? IDAW_RUN_MODE_SIL
                                                           : IDAW_RUN_MODE_HIL_BARRIER);
    ko_gonder_ham(MAVLINK_MSG_ID_IDA_SESSION_ACCEPT, &m, sizeof m, false);
}

void ko_session_close(uint16_t sess, uint8_t neden)
{
    mavlink_ida_session_close_t m;
    memset(&m, 0, sizeof m);
    m.sess = sess;
    m.reason = neden;
    ko_gonder_ham(MAVLINK_MSG_ID_IDA_SESSION_CLOSE, &m, sizeof m, false);
}

void ko_keepalive(uint16_t sess)
{
    mavlink_ida_keepalive_t m;
    memset(&m, 0, sizeof m);
    m.sess = sess;
    m.state = (uint8_t)IDAW_LINK_STATE_RUNNING;
    ko.son_keepalive = ko.saat;
    ko_gonder_ham(MAVLINK_MSG_ID_IDA_KEEPALIVE, &m, sizeof m, false);
}

int64_t ko_tf_t(uint32_t seq)
{
    return KO_T0_SIM + (int64_t)seq * KO_P_NS;
}

void ko_tf_s(uint16_t sess, uint32_t seq)
{
    mavlink_ida_time_frame_t m;
    memset(&m, 0, sizeof m);
    m.t_sim_ns = ko_tf_t(seq);
    m.batch_seq = seq;
    m.sess = sess;
    m.n_members = (uint8_t)(ko.uye > 255u ? 255u : ko.uye);
    m.tf_flags = 0u;
    m.run_mode = (uint8_t)((ko.rol == IDA_LINK_UST_ROL_SIL) ? IDAW_RUN_MODE_SIL
                                                           : IDAW_RUN_MODE_HIL_BARRIER);
    ko.uye = 0u;
    if (ko.rol == IDA_LINK_UST_ROL_SIL) {
        ko_saat_kur(m.t_sim_ns); /* A4-K11, A1-K121: SIL'de duvar saati TF'den türer */
    }
    ko_gonder_ham(MAVLINK_MSG_ID_IDA_TIME_FRAME, &m, sizeof m, false);
}

void ko_tf(uint32_t seq)
{
    ko_tf_s(ko.sess, seq);
}

void ko_simple_s(uint16_t sess, uint32_t cmd_id, uint8_t type, uint16_t arg)
{
    mavlink_ida_cmd_simple_t m;
    memset(&m, 0, sizeof m);
    m.cmd_id = cmd_id;
    m.sess = sess;
    m.arg = arg;
    m.type = type;
    m.source = 255u;
    ko_gonder_ham(MAVLINK_MSG_ID_IDA_CMD_SIMPLE, &m, sizeof m, true);
}

void ko_simple(uint32_t cmd_id, uint8_t type, uint16_t arg)
{
    ko_simple_s(ko.sess, cmd_id, type, arg);
}

void ko_fc_s(uint16_t sess, uint32_t cmd_id, uint8_t op, uint8_t tube, uint32_t arg0)
{
    mavlink_ida_cmd_fc_t m;
    memset(&m, 0, sizeof m);
    m.cmd_id = cmd_id;
    m.arg0 = arg0;
    m.arg1 = 0u;
    m.sess = sess;
    m.op = op;
    m.tube = tube;
    m.source = 255u;
    ko_gonder_ham(MAVLINK_MSG_ID_IDA_CMD_FC, &m, sizeof m, true);
}

void ko_fc(uint32_t cmd_id, uint8_t op, uint8_t tube, uint32_t arg0)
{
    ko_fc_s(ko.sess, cmd_id, op, tube, arg0);
}

void ko_evres_s(uint16_t sess, uint64_t event_id, uint8_t sonuc, int64_t t_apply)
{
    mavlink_ida_event_result_t m;
    memset(&m, 0, sizeof m);
    m.event_id = event_id;
    m.t_apply_ns = t_apply;
    m.sess = sess;
    m.result = sonuc;
    ko_gonder_ham(MAVLINK_MSG_ID_IDA_EVENT_RESULT, &m, sizeof m, true);
}

void ko_evres(uint64_t event_id, uint8_t sonuc, int64_t t_apply)
{
    ko_evres_s(ko.sess, event_id, sonuc, t_apply);
}

uint32_t ko_el_sikis(uint32_t nonce_sim, uint16_t sess)
{
    if (ko.tutuluyor) {
        ko_birak(true);
    }
    size_t g0 = ko_gunluk_n;
    ko_sim_hello(nonce_sim);
    ko_pompala();
    bool bulundu = false;
    mavlink_ida_board_hello_t bh;
    memset(&bh, 0, sizeof bh);
    for (size_t i = g0; i < ko_gunluk_n; ++i) {
        if (ko_gunluk_d[i].msgid == MAVLINK_MSG_ID_IDA_BOARD_HELLO) {
            mavlink_ida_board_hello_t b;
            ko_coz(&ko_gunluk_d[i], MAVLINK_MSG_ID_IDA_BOARD_HELLO, &b, sizeof b);
            if (b.peer_nonce == nonce_sim) {
                bh = b;
                bulundu = true;
            }
        }
    }
    TEST_ASSERT_TRUE_MESSAGE(bulundu, "SIM_HELLO'ya peer_nonce'lu BOARD_HELLO yanıtı yok (A1-K13)");
    TEST_ASSERT_NOT_EQUAL_UINT32(0u, bh.nonce);
    ko_session_accept(nonce_sim, bh.nonce, sess, (uint8_t)IDAW_SESSION_RESULT_OK);
    TEST_ASSERT_EQUAL_INT_MESSAGE((int)IDA_LINK_UST_RUNNING, (int)ida_link_ust_durum(&ko.l),
                                  "SESSION_ACCEPT(OK) sonrası RUNNING beklenir (A1-K16)");
    TEST_ASSERT_EQUAL_UINT16(sess, ida_link_ust_sess(&ko.l));
    ko.sess = sess;
    ko.uye = 0u;
    ko.son_keepalive = ko.saat;
    return bh.nonce;
}

uint16_t ko_sess(void)
{
    return ko.sess;
}

void ko_hazir(ida_link_ust_rol_t rol, uint16_t sess)
{
    ko_kur(rol);
    ko_pompala();
    (void)ko_el_sikis(UINT32_C(0x51000001), sess);
    ko_pompala();
    ko_tf(1u);
    ko_pompala();
}

/* ---- Sahte çekirdek ------------------------------------------------------------------- */
bool ko_genisletici(const ida_cmd_t *c)
{
    switch (c->type) {
    case IDA_CMD_ARM:
    case IDA_CMD_FAILSAFE_RESET:
        return true;
    case IDA_CMD_SET_MODE:
        return c->u.set_mode.mode == IDA_MODE_GUIDED || c->u.set_mode.mode == IDA_MODE_AUTO;
    case IDA_CMD_FC:
        return c->u.fc.op == IDA_FC_ARM || c->u.fc.op == IDA_FC_FIRE ||
               c->u.fc.op == IDA_FC_RESET || c->u.fc.op == IDA_FC_TEST_ENTER;
    default:
        return false;
    }
}

static uint8_t sonuc_turu(uint8_t type)
{
    if (type == IDA_CMD_PARAM_GET || type == IDA_CMD_PARAM_SET) {
        return (uint8_t)IDA_RES_PARAM_VALUE;
    }
    if (type == IDA_CMD_PARAM_SAVE) {
        return (uint8_t)IDA_RES_PARAM_SAVE;
    }
    return (uint8_t)IDA_RES_ACK;
}

static void sonuc_yaz(ida_out_t *out, uint32_t cmd_id, uint8_t kind, uint8_t ack, uint16_t neden)
{
    if (out->n_results >= IDA_PORT_MAX_RESULTS) {
        return;
    }
    ida_cmd_result_t *r = &out->results[out->n_results];
    memset(r, 0, sizeof *r);
    r->cmd_id = cmd_id;
    r->kind = kind;
    r->ack = ack;
    r->reason = neden;
    out->n_results++;
}

static void sahte_cekirdek(const ida_in_t *in, ida_link_cagri_t tur, ida_out_t *out)
{
    memset(out, 0, sizeof *out);
    if (in->tf.present) {
        ko.son_seq = in->tf.seq;
        ko.son_t = in->tf.t.ns;
    }
    out->src = (uint8_t)((tur == IDA_LINK_CAGRI_STEP) ? IDA_SRC_STEP : IDA_SRC_TICK);
    out->seq = ko.son_seq;
    out->t_ref.ns = ko.son_t;
    out->armed = false;
    out->is_safe = true;
    out->fs_level = (uint8_t)IDA_FS_NONE;
    if (tur == IDA_LINK_CAGRI_STEP) {
        out->thr_valid = true;
        out->valid_for_ns = IDA_VALID_FOR_DEFAULT_NS;
    } else {
        out->thr_valid = false;
        out->valid_for_ns = 0u;
    }
    /* A4-K43: önce mandal bitleri (dizin sırasıyla), sonra cmds[] (dizi sırasıyla). */
    for (uint32_t i = 0; i < IDA_PORT_N_NARROW; ++i) {
        if ((in->narrow.mask & (1u << i)) != 0u) {
            sonuc_yaz(out, in->narrow.cmd_id[i], (uint8_t)IDA_RES_ACK, (uint8_t)IDA_ACK_ACCEPTED,
                      (uint16_t)IDA_REASON_NONE);
        }
    }
    bool daraltma = ((in->narrow.mask & IDA_NARROW_DEFINED) != 0u) ||
                    in->plat.link_event == IDA_LINKEV_SESSION_END;
    uint8_t n = (uint8_t)(in->n_cmds <= IDA_PORT_MAX_CMDS ? in->n_cmds : IDA_PORT_MAX_CMDS);
    for (uint8_t i = 0; i < n; ++i) {
        const ida_cmd_t *c = &in->cmds[i];
        if (daraltma && ko_genisletici(c)) {
            /* A4-K45: aynı çağrıda daraltma varsa genişletici NARROWED (çekirdeğin sonucu). */
            sonuc_yaz(out, c->cmd_id, sonuc_turu(c->type), (uint8_t)IDA_ACK_REJECTED,
                      (uint16_t)IDA_REASON_NARROWED);
        } else {
            sonuc_yaz(out, c->cmd_id, sonuc_turu(c->type), (uint8_t)IDA_ACK_ACCEPTED,
                      (uint16_t)IDA_REASON_NONE);
        }
    }
    out->health.core_ok = true;
    out->health.data_ok = true;
    out->health.narrowed = daraltma;
    out->status.mode = (uint8_t)IDA_MODE_DISARMED;
    out->status.armed = false;
    out->status.param_ok = true;
    out->status.events_ok = true;
    out->status.boot_count = KO_BOOT_COUNT;
    out->fc.state = (uint8_t)IDA_FCS_SAFE;

    bool olay = false;
    uint32_t apply_by = 0u;
    if (ko.olay_planli && in->tf.present &&
        (tur == IDA_LINK_CAGRI_STEP || tur == IDA_LINK_CAGRI_TICK_TF)) {
        olay = true;
        apply_by = ko.olay_apply_by;
        ko.olay_planli = false;
    } else if (ko.sonu_olayi && tur == IDA_LINK_CAGRI_OTURUM_SONU) {
        /* A4-K73: oturum sonu çağrısı SAFETY_CLOSE(ALL) üretir. */
        olay = true;
        apply_by = 20000000u;
    }
    if (olay) {
        ko.olay_ctr++;
        ida_event_t *e = &out->events[0];
        e->event_id = ((uint64_t)KO_BOOT_COUNT << 32) | (uint64_t)ko.olay_ctr;
        e->kind = (uint8_t)IDA_EVT_SAFETY_CLOSE;
        e->tube = (uint8_t)IDA_TUBE_ALL;
        e->seq = out->seq;
        e->t_decide = out->t_ref;
        e->apply_by_ns = apply_by;
        e->hold_ns = 0u;
        out->n_events = 1u;
        ko.son_olay_id = e->event_id;
        ko.son_olay_now_w = in->now_w.ns;
    }
}

void ko_olay_planla(uint32_t apply_by_ns)
{
    ko.olay_planli = true;
    ko.olay_apply_by = apply_by_ns;
}

void ko_oturum_sonu_olayi(bool uret)
{
    ko.sonu_olayi = uret;
}

uint64_t ko_son_olay_id(void)
{
    return ko.son_olay_id;
}

int64_t ko_son_olay_now_w(void)
{
    return ko.son_olay_now_w;
}

/* ---- Çağrı akışı --------------------------------------------------------------------- */
static size_t cagri_kaydet(ida_link_cagri_t tur, const ida_in_t *in)
{
    TEST_ASSERT_TRUE_MESSAGE(ko_cagri_n < KO_CAGRI_AZAMI, "çağrı kaydı doldu (düzenek)");
    size_t sira = ko_cagri_n++;
    ko_cagri_t *c = &ko_cagri_d[sira];
    memset(c, 0, sizeof *c);
    c->tur = tur;
    c->now_w = in->now_w;
    c->tf = in->tf;
    c->link_event = in->plat.link_event;
    c->link_state = in->plat.link_state;
    c->n_samples = in->n_samples;
    c->narrow = in->narrow;
    c->n_cmds = in->n_cmds;
    uint8_t nc = (uint8_t)(in->n_cmds <= IDA_PORT_MAX_CMDS ? in->n_cmds : IDA_PORT_MAX_CMDS);
    for (uint8_t i = 0; i < nc; ++i) {
        c->cmds[i] = in->cmds[i];
    }
    c->n_evres = in->n_evres;
    uint8_t ne = (uint8_t)(in->n_evres <= IDA_PORT_MAX_EVRES ? in->n_evres : IDA_PORT_MAX_EVRES);
    for (uint8_t i = 0; i < ne; ++i) {
        c->evres[i] = in->evres[i];
    }
    c->ping360_var = in->ping360 != NULL;
    c->gunluk_take = ko_gunluk_n;
    c->bitti = false;

    /* Komut kimliği tablosu: çağrı girdisine giriş. */
    for (uint8_t i = 0; i < nc; ++i) {
        uint32_t id = in->cmds[i].cmd_id;
        if (id < KO_KIMLIK_AZAMI) {
            ko_kimlik_t *k = &ko_kimlik_d[id];
            if (k->girdi == 0u) {
                k->ilk_cagri = (uint32_t)sira;
            }
            k->girdi++;
        }
    }
    for (uint32_t i = 0; i < IDA_PORT_N_NARROW; ++i) {
        if ((in->narrow.mask & (1u << i)) != 0u) {
            uint32_t id = in->narrow.cmd_id[i];
            if (id < KO_KIMLIK_AZAMI) {
                ko_kimlik_t *k = &ko_kimlik_d[id];
                if (k->girdi == 0u) {
                    k->ilk_cagri = (uint32_t)sira;
                }
                k->girdi++;
            }
        }
    }
    return sira;
}

/* Platformun kendi alanları (ust.h dosya başı md. 3). */
static void platform_alanlari(ida_in_t *in)
{
    in->plat.param_store = (uint8_t)IDA_PSTORE_IDLE;
    in->plat.param_fail = (uint8_t)IDA_PSFAIL_NONE;
    in->plat.param_gen = 0u;
    in->plat.prev_overrun = false;
    in->plat.tx_stalled = false;
    in->plat.prev_call_ns = 0u;
    in->bg_slices = 0u;
}

static void cagri_bitir(size_t sira, ida_in_t *in, ida_link_cagri_t tur)
{
    sahte_cekirdek(in, tur, &ko_out);
    ko_cagri_d[sira].gunluk_put = ko_gunluk_n;
    TEST_ASSERT_TRUE_MESSAGE(ida_link_ust_cagri_acik(&ko.l),
                             "take ile put_out arasında çağrı penceresi açık olmalı (W3-K10)");
    ida_link_ust_put_out(&ko.l, in, IDA_RC_OK, &ko_out);
    TEST_ASSERT_FALSE_MESSAGE(ida_link_ust_cagri_acik(&ko.l),
                              "put_out'tan sonra çağrı penceresi kapanmalı (W3-K10)");
    ko_cagri_d[sira].bitti = true;
    ko.tamamlanan_cagri++;
}

void ko_pompala(void)
{
    ida_link_ust_poll_tx(&ko.l);
    if (ko.tutuluyor) {
        return;
    }
    for (int i = 0; i < 64; ++i) {
        ida_link_cagri_t tur = ida_link_ust_take(&ko.l, &ko_in);
        if (tur == IDA_LINK_CAGRI_YOK) {
            return;
        }
        platform_alanlari(&ko_in);
        size_t sira = cagri_kaydet(tur, &ko_in);
        cagri_bitir(sira, &ko_in, tur);
        ida_link_ust_poll_tx(&ko.l);
    }
    TEST_FAIL_MESSAGE("take 64 kez üst üste çağrı verdi (düzenek sınırı)");
}

long ko_tut(void)
{
    TEST_ASSERT_FALSE_MESSAGE(ko.tutuluyor, "zaten tutulan çağrı var (düzenek)");
    ida_link_ust_poll_tx(&ko.l);
    ida_link_cagri_t tur = ida_link_ust_take(&ko.l, &ko.tut_in);
    if (tur == IDA_LINK_CAGRI_YOK) {
        return -1;
    }
    platform_alanlari(&ko.tut_in);
    ko.tut_kayit = cagri_kaydet(tur, &ko.tut_in);
    ko.tut_tur = tur;
    ko.tutuluyor = true;
    return (long)ko.tut_kayit;
}

void ko_birak(bool pompala)
{
    TEST_ASSERT_TRUE_MESSAGE(ko.tutuluyor, "tutulan çağrı yok (düzenek)");
    ko.tutuluyor = false;
    cagri_bitir(ko.tut_kayit, &ko.tut_in, ko.tut_tur);
    if (pompala) {
        ko_pompala();
    }
}

bool ko_tutuluyor(void)
{
    return ko.tutuluyor;
}

long ko_tek_cagri(void)
{
    long k = ko_tut();
    if (k >= 0) {
        ko_birak(false);
    }
    return k;
}

/* ---- Kayıtlara erişim ------------------------------------------------------------------ */
size_t ko_gunluk_boy(void)
{
    TEST_ASSERT_EQUAL_size_t_MESSAGE(0u, ko_gunluk_tasan, "çerçeve günlüğü taştı (düzenek)");
    return ko_gunluk_n;
}

const ko_cerceve_t *ko_gunluk(size_t i)
{
    TEST_ASSERT_TRUE(i < ko_gunluk_n);
    return &ko_gunluk_d[i];
}

size_t ko_cagri_boy(void)
{
    return ko_cagri_n;
}

const ko_cagri_t *ko_cagri(size_t i)
{
    TEST_ASSERT_TRUE(i < ko_cagri_n);
    return &ko_cagri_d[i];
}

const ko_kimlik_t *ko_kimlik(uint32_t cmd_id)
{
    TEST_ASSERT_TRUE(cmd_id < KO_KIMLIK_AZAMI);
    return &ko_kimlik_d[cmd_id];
}

uint32_t ko_kuyruk_temizle_sayisi(void)
{
    return ko.kuyruk_temizle;
}

uint32_t ko_bozuk_cerceve(void)
{
    return ko.bozuk;
}

void ko_coz(const ko_cerceve_t *c, uint32_t msgid, void *mesaj, size_t boy)
{
    TEST_ASSERT_EQUAL_UINT32(msgid, c->msgid);
    ida_link_cerceve_t f;
    memset(&f, 0, sizeof f);
    f.msgid = c->msgid;
    f.len = c->len;
    f.sysid = (uint8_t)IDA_LINK_KART_SYSID;
    f.compid = (uint8_t)IDA_LINK_KART_COMPID;
    ida_link_mesaj_bilgisi_t b;
    TEST_ASSERT_EQUAL_INT(IDA_LINK_TAMAM, ida_link_mesaj_bilgisi(msgid, &b));
    f.yuk_azami = b.yuk_azami;
    memcpy(f.yuk, c->yuk, sizeof f.yuk);
    TEST_ASSERT_EQUAL_INT(IDA_LINK_TAMAM, ida_link_coz(&f, msgid, mesaj, boy));
}

bool ko_board_hello_bul(size_t bas, uint32_t peer_nonce, mavlink_ida_board_hello_t *bh,
                        size_t *sira)
{
    for (size_t i = bas; i < ko_gunluk_n; ++i) {
        if (ko_gunluk_d[i].msgid == MAVLINK_MSG_ID_IDA_BOARD_HELLO) {
            mavlink_ida_board_hello_t b;
            ko_coz(&ko_gunluk_d[i], MAVLINK_MSG_ID_IDA_BOARD_HELLO, &b, sizeof b);
            if (b.peer_nonce == peer_nonce) {
                if (bh != NULL) {
                    *bh = b;
                }
                if (sira != NULL) {
                    *sira = i;
                }
                return true;
            }
        }
    }
    return false;
}

bool ko_cagri_komut_iceriyor(const ko_cagri_t *c, uint32_t cmd_id)
{
    uint8_t nc = (uint8_t)(c->n_cmds <= IDA_PORT_MAX_CMDS ? c->n_cmds : IDA_PORT_MAX_CMDS);
    for (uint8_t i = 0; i < nc; ++i) {
        if (c->cmds[i].cmd_id == cmd_id) {
            return true;
        }
    }
    return false;
}

bool ko_cagri_mandal_iceriyor(const ko_cagri_t *c, uint32_t cmd_id)
{
    for (uint32_t i = 0; i < IDA_PORT_N_NARROW; ++i) {
        if ((c->narrow.mask & (1u << i)) != 0u && c->narrow.cmd_id[i] == cmd_id) {
            return true;
        }
    }
    return false;
}

long ko_komutu_tasiyan_cagri(size_t bas, uint32_t cmd_id)
{
    for (size_t i = bas; i < ko_cagri_n; ++i) {
        if (ko_cagri_komut_iceriyor(&ko_cagri_d[i], cmd_id) ||
            ko_cagri_mandal_iceriyor(&ko_cagri_d[i], cmd_id)) {
            return (long)i;
        }
    }
    return -1;
}

uint32_t ko_sonuc_say(uint32_t cmd_id, uint8_t ack, uint16_t reason)
{
    if (cmd_id >= KO_KIMLIK_AZAMI) {
        return 0u;
    }
    const ko_kimlik_t *k = &ko_kimlik_d[cmd_id];
    uint32_t n = 0;
    uint16_t m = (uint16_t)(k->n_sonuc < KO_SONUC_AZAMI ? k->n_sonuc : KO_SONUC_AZAMI);
    for (uint16_t i = 0; i < m; ++i) {
        if (k->sonuc[i].ack == ack && k->sonuc[i].reason == reason) {
            n++;
        }
    }
    return n;
}

/* ---- Sözde rastgele ------------------------------------------------------------------- */
void ko_rng_kur(ko_rng_t *r, uint64_t tohum)
{
    r->s = tohum;
}

uint32_t ko_rng_u32(ko_rng_t *r)
{
    uint64_t z = (r->s += UINT64_C(0x9E3779B97F4A7C15));
    z = (z ^ (z >> 30)) * UINT64_C(0xBF58476D1CE4E5B9);
    z = (z ^ (z >> 27)) * UINT64_C(0x94D049BB133111EB);
    z ^= z >> 31;
    return (uint32_t)(z >> 32);
}

uint32_t ko_rng_aralik(ko_rng_t *r, uint32_t n)
{
    return (uint32_t)(((uint64_t)ko_rng_u32(r) * (uint64_t)n) >> 32);
}

void ko_bilgi(const char *bicim, ...)
{
    char buf[512];
    va_list ap;
    va_start(ap, bicim);
    (void)vsnprintf(buf, sizeof buf, bicim, ap);
    va_end(ap);
    TEST_MESSAGE(buf);
}
