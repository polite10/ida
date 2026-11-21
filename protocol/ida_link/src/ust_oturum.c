/*
 * ust_oturum.c — kurulum, alım dağıtımı, el sıkışma ve oturum (A1 §4.1–§4.2, §4.9),
 * zamanlayıcılar (A1-K11, K20, K22) ve gönderim yardımcısı.
 */
#include "ust_ic.h"

#include <string.h>

/* sim -> kart mesajlarının tipli yapıları; ida_link_coz'un hedefi. Örnek mesajları (MAG,
   BARO, GNSS, DVL, PING1D, BATT, LEAK, SAFETY) birliğe üye olarak yazılmaz: yalnız birliğin
   adresiyle ust_uye_ornek'e verilirler (kullanılmayan üye kalmasın); boy ve hiza aşağıdaki
   denetimlerle güvencededir. */
typedef union {
    mavlink_ida_sim_hello_t        sim_hello;
    mavlink_ida_session_accept_t   session_accept;
    mavlink_ida_keepalive_t        keepalive;
    mavlink_ida_session_close_t    session_close;
    mavlink_ida_time_frame_t       time_frame;
    mavlink_ida_imu_t              imu;
    mavlink_ida_ping360_head_t     ping360_head;
    mavlink_ida_ping360_data_t     ping360_data;
    mavlink_ida_det_batch_t        det_batch;
    mavlink_ida_det_record_t       det_record;
    mavlink_ida_cmd_simple_t       cmd_simple;
    mavlink_ida_cmd_manual_t       cmd_manual;
    mavlink_ida_cmd_guided_t       cmd_guided;
    mavlink_ida_cmd_mission_item_t cmd_mission_item;
    mavlink_ida_cmd_param_set_t    cmd_param_set;
    mavlink_ida_cmd_fc_t           cmd_fc;
    mavlink_ida_event_result_t     event_result;
} ust_gelen_t;

#define UST_GELEN_SIGAR(t)                                                              \
    _Static_assert(sizeof(ust_gelen_t) >= sizeof(t) && _Alignof(ust_gelen_t) >= _Alignof(t), \
                   "ust_gelen_t " #t " yapısını taşımıyor")
UST_GELEN_SIGAR(mavlink_ida_mag_t);
UST_GELEN_SIGAR(mavlink_ida_baro_t);
UST_GELEN_SIGAR(mavlink_ida_gnss_t);
UST_GELEN_SIGAR(mavlink_ida_dvl_t);
UST_GELEN_SIGAR(mavlink_ida_ping1d_t);
UST_GELEN_SIGAR(mavlink_ida_batt_t);
UST_GELEN_SIGAR(mavlink_ida_leak_t);
UST_GELEN_SIGAR(mavlink_ida_safety_t);

/* ------------------------------------------------------------------ */
/* Yardımcılar                                                        */
/* ------------------------------------------------------------------ */

ida_wtime_t ust_zaman_sinirla(ida_wtime_t t)
{
    /* A4-K08: geçerli zaman 0 … 2^62 ns. Bu aralıktaki değerlerin fark ve toplamları (≤ 2^32
       ns'lik sürelerle) int64_t'de taşmaz (sol bulgu 5). */
    if (t.ns < 0) {
        t.ns = 0;
    } else if (t.ns > IDA_TIME_MAX_NS) {
        t.ns = IDA_TIME_MAX_NS;
    }
    return t;
}

ida_wtime_t ust_simdi(ida_link_ust_t *l)
{
    return ust_zaman_sinirla(l->ozel_plat.zaman(l->ozel_plat.ctx));
}

static bool oturum_disinda_gecer(uint32_t msgid)
{
    return msgid == MAVLINK_MSG_ID_IDA_BOARD_HELLO || msgid == MAVLINK_MSG_ID_IDA_KEEPALIVE ||
           msgid == MAVLINK_MSG_ID_IDA_SESSION_CLOSE || msgid == MAVLINK_MSG_ID_IDA_CALL_TRACE;
}

bool ust_gonder(ida_link_ust_t *l, ida_link_sinif_t sinif, uint32_t msgid, const void *mesaj,
                size_t boy)
{
    uint8_t buf[IDA_LINK_CERCEVE_AZAMI];
    size_t n = 0;
    bool ok;
    if (l->ozel_durum != (uint8_t)IDA_LINK_UST_RUNNING && !oturum_disinda_gecer(msgid)) {
        return false;                                     /* A1-K11, K22 */
    }
    if (ida_link_gonder(&l->ozel_gonderici, msgid, mesaj, boy, buf, sizeof buf, &n) !=
        IDA_LINK_TAMAM) {
        return false;
    }
    ok = l->ozel_plat.gonder(l->ozel_plat.ctx, sinif, buf, n);
    if (ok) {
        l->ozel_son_tx = ust_simdi(l);
        l->ozel_tx_var = 1u;
    } else if (sinif == IDA_LINK_SINIF_ZORUNLU &&
               l->ozel_durum == (uint8_t)IDA_LINK_UST_RUNNING) {
        ust_oturum_bitir(l, (uint8_t)IDAW_CLOSE_REASON_TX_OVERFLOW);   /* A1-K110 */
    }
    return ok;
}

uint32_t ust_rx_hata(const ida_link_ust_t *l)
{
    const ida_link_sayaclar_t *s = &l->ozel_alici.sayac;
    /* A1 §3.11: plat.cnt_rx_error. */
    return s->rx_crc_err + s->rx_iflag + s->rx_bad_len + s->rx_overflow + s->rx_unknown_msgid;
}

static uint32_t nonce_cek(ida_link_ust_t *l)
{
    for (unsigned i = 0; i < 5u; ++i) {
        const uint32_t v = l->ozel_plat.nonce(l->ozel_plat.ctx);
        if (v != 0u) {
            return v;
        }
    }
    return 1u;
}

static uint8_t tel_durumu(const ida_link_ust_t *l)
{
    switch (l->ozel_durum) {
    case IDA_LINK_UST_HANDSHAKE:
        return (uint8_t)IDAW_LINK_STATE_HANDSHAKE;
    case IDA_LINK_UST_RUNNING:
        return (uint8_t)IDAW_LINK_STATE_RUNNING;
    default:
        return (uint8_t)IDAW_LINK_STATE_IDLE;
    }
}

static void board_hello_gonder(ida_link_ust_t *l, uint32_t peer_nonce)
{
    mavlink_ida_board_hello_t m;
    uint32_t crc = 0u;
    uint32_t len = 0u;
    uint32_t gen = 0u;
    uint8_t init_rc = l->ozel_ayar.init_rc;
    if (!l->ozel_nonce_var) {
        l->ozel_kendi_nonce = nonce_cek(l);   /* A1-K114: açılıştan sonraki ilk BOARD_HELLO */
        l->ozel_nonce_var = 1u;
    }
    l->ozel_plat.kalici_blok(l->ozel_plat.ctx, &crc, &len, &gen);   /* A1-K68 */
    if (l->ozel_guvenli) {
        init_rc = (uint8_t)IDA_RC_NOT_INIT;   /* H33 */
    }
    ust_esl_board_hello(&l->ozel_ayar, peer_nonce, l->ozel_kendi_nonce, init_rc, crc, len, gen,
                        l->ozel_last_cmd_id, l->ozel_prev_sess, l->ozel_oturum_sayisi, &m);
    (void)ust_gonder(l, IDA_LINK_SINIF_ZORUNLU, MAVLINK_MSG_ID_IDA_BOARD_HELLO, &m, sizeof m);
}

static void keepalive_gonder(ida_link_ust_t *l)
{
    mavlink_ida_keepalive_t m;
    memset(&m, 0, sizeof m);
    m.sess = l->ozel_durum == (uint8_t)IDA_LINK_UST_RUNNING ? l->ozel_sess : 0u;
    m.state = tel_durumu(l);
    (void)ust_gonder(l, IDA_LINK_SINIF_ZORUNLU, MAVLINK_MSG_ID_IDA_KEEPALIVE, &m, sizeof m);
}

/* Yeni el sıkışma yanıtı (A1-K13): yeni nonce, peer_nonce = SIM_HELLO.nonce, HANDSHAKE. */
static void yeni_el_sikisma(ida_link_ust_t *l, uint32_t sim_nonce)
{
    l->ozel_kendi_nonce = nonce_cek(l);
    l->ozel_nonce_var = 1u;
    l->ozel_yanit_nonce = sim_nonce;
    l->ozel_durum = (uint8_t)IDA_LINK_UST_HANDSHAKE;
    board_hello_gonder(l, sim_nonce);
}

/* ------------------------------------------------------------------ */
/* Oturum sonu (A1-K22, K107)                                         */
/* ------------------------------------------------------------------ */

void ust_oturum_bitir(ida_link_ust_t *l, uint8_t neden)
{
    if (l->ozel_durum != (uint8_t)IDA_LINK_UST_RUNNING) {
        return;
    }
    /* (1) Kuyrukta yazımı başlamamış çerçeveler silinir; SESSION_CLOSE ardından denenir. */
    l->ozel_plat.kuyruk_temizle(l->ozel_plat.ctx);
    l->ozel_durum = (uint8_t)IDA_LINK_UST_SONLANIYOR;
    /* Alıcının yarım adayı sayaç üretmeden atılır (A1-K07); aynı seri akışta seq geçmişi
       korunur (A1-K08). Tel katmanının açık işlevi ida_link_alici_tasma bunu yapar ama
       rx_overflow'u artırır; bu bir taşma olmadığından sayaç eski değerine döndürülür
       (sol bulgu 7). Bir çerçevenin işlenmesi sırasında çağrıldığında aday zaten yoktur. */
    {
        const uint32_t tasma = l->ozel_alici.sayac.rx_overflow;
        (void)ida_link_alici_tasma(&l->ozel_alici);
        l->ozel_alici.sayac.rx_overflow = tasma;
    }
    if (neden != 0u) {
        mavlink_ida_session_close_t m;
        memset(&m, 0, sizeof m);
        m.sess = l->ozel_sess;
        m.reason = neden;
        (void)ust_gonder(l, IDA_LINK_SINIF_ZORUNLU, MAVLINK_MSG_ID_IDA_SESSION_CLOSE, &m,
                         sizeof m);
    }
    /* Ertelenmiş komutlar, açık parti, bekleyen parti, Ping360 yuvaları (mandallar kalır;
       A4-K74); onaylanmamış olaylar SESSION_END (A4-K71). */
    ust_parti_oturum_sonu(l);
    ust_olay_oturum_sonu(l);
    l->ozel_sonu_bekliyor = 1u;   /* (2) oturum sonu çağrısı take'te */
    l->ozel_baslat_olayi = 0u;
    l->ozel_kusak += 1u;
    ust_izgara_mutlak(l, ust_simdi(l));
}

void ust_oturum_sonu_dondu(ida_link_ust_t *l)
{
    /* (4) IDLE; bekleyen SIM_HELLO şimdi yanıtlanır (A1-K13, K107). */
    l->ozel_durum = (uint8_t)IDA_LINK_UST_IDLE;
    l->ozel_sess = 0u;
    if (l->ozel_hello_bekliyor && !l->ozel_guvenli) {
        l->ozel_hello_bekliyor = 0u;
        yeni_el_sikisma(l, l->ozel_hello_nonce);
    }
    l->ozel_hello_bekliyor = 0u;
}

/* ------------------------------------------------------------------ */
/* El sıkışma (A1-K13, K16, K17)                                      */
/* ------------------------------------------------------------------ */

static void sim_hello_al(ida_link_ust_t *l, const mavlink_ida_sim_hello_t *m)
{
    if (m->nonce == 0u || l->ozel_guvenli) {
        return;                              /* geçersiz nonce; güvenli beklemede yanıt yok */
    }
    switch (l->ozel_durum) {
    case IDA_LINK_UST_RUNNING:
        if (m->nonce == l->ozel_yanit_nonce) {
            return;                          /* aynı nonce: yok sayılır */
        }
        l->ozel_hello_bekliyor = 1u;
        l->ozel_hello_nonce = m->nonce;
        ust_oturum_bitir(l, 0u);             /* yanıt oturum sonu çağrısından sonra */
        return;
    case IDA_LINK_UST_SONLANIYOR:
        l->ozel_hello_bekliyor = 1u;
        l->ozel_hello_nonce = m->nonce;
        return;
    case IDA_LINK_UST_HANDSHAKE:
        if (m->nonce == l->ozel_yanit_nonce) {
            board_hello_gonder(l, m->nonce); /* aynı BOARD_HELLO yeniden */
            return;
        }
        yeni_el_sikisma(l, m->nonce);
        return;
    default:
        yeni_el_sikisma(l, m->nonce);
        return;
    }
}

static void hata_kaydi_gonder(ida_link_ust_t *l)
{
    mavlink_ida_fault_record_t m;
    const ida_link_hata_kaydi_t *h = &l->ozel_ayar.hata_kaydi;
    if (!l->ozel_ayar.hata_kaydi_var) {
        return;
    }
    memset(&m, 0, sizeof m);
    m.cfsr = h->cfsr;
    m.hfsr = h->hfsr;
    m.mmfar = h->mmfar;
    m.bfar = h->bfar;
    m.pc = h->pc;
    m.lr = h->lr;
    m.xpsr = h->xpsr;
    m.step_counter = h->step_counter;
    m.fault_seq = h->fault_seq;
    m.stacking_valid = h->stacking_valid;
    (void)ust_gonder(l, IDA_LINK_SINIF_KAYBEDILEBILIR, MAVLINK_MSG_ID_IDA_FAULT_RECORD, &m,
                     sizeof m);
}

static void session_accept_al(ida_link_ust_t *l, const mavlink_ida_session_accept_t *m,
                              ida_wtime_t t_rx)
{
    if (l->ozel_durum != (uint8_t)IDA_LINK_UST_HANDSHAKE || m->nonce_sim != l->ozel_yanit_nonce ||
        m->nonce_board != l->ozel_kendi_nonce) {
        return;                                           /* A1-K16 */
    }
    switch (m->result) {
    case IDAW_SESSION_RESULT_OK:
        if (m->sess == 0u) {
            return;                                       /* OK'de sess ≠ 0 (A1 §3.8) */
        }
        l->ozel_durum = (uint8_t)IDA_LINK_UST_RUNNING;
        l->ozel_sess = m->sess;
        l->ozel_prev_sess = m->sess;
        l->ozel_oturum_sayisi = (uint16_t)(l->ozel_oturum_sayisi + 1u);
        l->say[IDA_LINK_SAY_SESSIONS] += 1u;
        l->ozel_run_id = m->run_id;                       /* yalnız bilgi (A1-K126) */
        l->ozel_run_mode = m->run_mode;                   /* yalnız tanı (A1-K24) */
        l->ozel_emn_rev = 0u;                             /* A1-K106 */
        l->ozel_baslat_olayi = 1u;                        /* SESSION_START sonraki çağrıda */
        l->ozel_son_rx = t_rx;
        ust_parti_oturum_basi(l);
        keepalive_gonder(l);                              /* hemen KEEPALIVE(sess, RUNNING) */
        hata_kaydi_gonder(l);                             /* A1-K96 */
        return;
    case IDAW_SESSION_RESULT_REBOOT:
        l->ozel_durum = (uint8_t)IDA_LINK_UST_IDLE;
        l->ozel_plat.yeniden_baslat(l->ozel_plat.ctx);    /* A1-K17: başka çerçeve yok */
        return;
    default:
        l->ozel_durum = (uint8_t)IDA_LINK_UST_IDLE;       /* REJECT_* */
        return;
    }
}

/* ------------------------------------------------------------------ */
/* Alım dağıtımı                                                      */
/* ------------------------------------------------------------------ */

/* Mesajın sess alanı (yoksa false). */
static bool sess_alani(uint32_t msgid, const ust_gelen_t *g, uint16_t *sess)
{
    switch (msgid) {
    case MAVLINK_MSG_ID_IDA_KEEPALIVE: *sess = g->keepalive.sess; return true;
    case MAVLINK_MSG_ID_IDA_SESSION_CLOSE: *sess = g->session_close.sess; return true;
    case MAVLINK_MSG_ID_IDA_TIME_FRAME: *sess = g->time_frame.sess; return true;
    case MAVLINK_MSG_ID_IDA_CMD_SIMPLE: *sess = g->cmd_simple.sess; return true;
    case MAVLINK_MSG_ID_IDA_CMD_MANUAL: *sess = g->cmd_manual.sess; return true;
    case MAVLINK_MSG_ID_IDA_CMD_GUIDED: *sess = g->cmd_guided.sess; return true;
    case MAVLINK_MSG_ID_IDA_CMD_MISSION_ITEM: *sess = g->cmd_mission_item.sess; return true;
    case MAVLINK_MSG_ID_IDA_CMD_PARAM_SET: *sess = g->cmd_param_set.sess; return true;
    case MAVLINK_MSG_ID_IDA_CMD_FC: *sess = g->cmd_fc.sess; return true;
    case MAVLINK_MSG_ID_IDA_EVENT_RESULT: *sess = g->event_result.sess; return true;
    default: return false;
    }
}

/* Parti üyesi mi (A1 §2: TF dışındaki sensör, sonar, algılama, komut, olay sonucu). */
static bool uye_mi(uint32_t msgid)
{
    return msgid >= MAVLINK_MSG_ID_IDA_IMU && msgid <= MAVLINK_MSG_ID_IDA_EVENT_RESULT &&
           msgid != MAVLINK_MSG_ID_IDA_TIME_FRAME;
}

static void cerceve_isle(ida_link_ust_t *l, const ida_link_cerceve_t *c, ida_wtime_t t_rx)
{
    ust_gelen_t g;
    ida_link_mesaj_bilgisi_t b;
    uint16_t sess = 0u;
    const bool running = l->ozel_durum == (uint8_t)IDA_LINK_UST_RUNNING;
    if (ida_link_mesaj_bilgisi(c->msgid, &b) != IDA_LINK_TAMAM || b.yapi_boyu > sizeof g) {
        return;
    }
    if (ida_link_coz(c, c->msgid, &g, b.yapi_boyu) != IDA_LINK_TAMAM) {
        return;
    }
    /* A1-K25: RUNNING'de eski sess'li çerçeve atılır ve sayılır (geçerli çerçeve sayılmaz). */
    if (running && sess_alani(c->msgid, &g, &sess) && sess != l->ozel_sess) {
        l->say[IDA_LINK_SAY_RX_STALE_SESSION] += 1u;
        return;
    }
    l->ozel_son_rx = t_rx;   /* sim'den geçerli çerçeve (A1-K22 T_bağ) */

    switch (c->msgid) {
    case MAVLINK_MSG_ID_IDA_SIM_HELLO:
        sim_hello_al(l, &g.sim_hello);
        return;
    case MAVLINK_MSG_ID_IDA_SESSION_ACCEPT:
        session_accept_al(l, &g.session_accept, t_rx);
        return;
    case MAVLINK_MSG_ID_IDA_KEEPALIVE:
        return;                                           /* canlılık */
    case MAVLINK_MSG_ID_IDA_SESSION_CLOSE:
        if (running) {
            ust_oturum_bitir(l, 0u);                      /* A1-K21 */
        }
        return;
    default:
        break;
    }
    if (c->msgid != MAVLINK_MSG_ID_IDA_TIME_FRAME && !uye_mi(c->msgid)) {
        return;
    }
    if (!running) {
        l->say[IDA_LINK_SAY_RX_OUTSIDE_SESSION] += 1u;    /* A1-K26 */
        return;
    }
    if (c->msgid == MAVLINK_MSG_ID_IDA_TIME_FRAME) {
        ust_parti_tf(l, &g.time_frame, t_rx);
        return;
    }
    l->ozel_acik.n_uye = (uint16_t)(l->ozel_acik.n_uye + 1u);   /* A1-K30 */
    switch (c->msgid) {
    case MAVLINK_MSG_ID_IDA_IMU:
        ust_uye_imu(l, &g.imu, t_rx);
        break;
    case MAVLINK_MSG_ID_IDA_PING360_HEAD:
        ust_uye_ping_bas(l, &g.ping360_head, t_rx);
        break;
    case MAVLINK_MSG_ID_IDA_PING360_DATA:
        ust_uye_ping_veri(l, &g.ping360_data);
        break;
    case MAVLINK_MSG_ID_IDA_DET_BATCH:
        ust_uye_det_batch(l, &g.det_batch, t_rx);
        break;
    case MAVLINK_MSG_ID_IDA_DET_RECORD:
        ust_uye_det_record(l, &g.det_record, t_rx);
        break;
    case MAVLINK_MSG_ID_IDA_CMD_SIMPLE:
    case MAVLINK_MSG_ID_IDA_CMD_MANUAL:
    case MAVLINK_MSG_ID_IDA_CMD_GUIDED:
    case MAVLINK_MSG_ID_IDA_CMD_MISSION_ITEM:
    case MAVLINK_MSG_ID_IDA_CMD_PARAM_SET:
    case MAVLINK_MSG_ID_IDA_CMD_FC:
        ust_komut_al(l, c->msgid, &g);
        break;
    case MAVLINK_MSG_ID_IDA_EVENT_RESULT:
        ust_olay_sonucu(l, &g.event_result);
        break;
    default:
        ust_uye_ornek(l, c->msgid, &g, t_rx);             /* MAG … SAFETY */
        break;
    }
}

/* ------------------------------------------------------------------ */
/* Dışa açık işlevler                                                 */
/* ------------------------------------------------------------------ */

ida_link_rc_t ida_link_ust_kur(ida_link_ust_t *l, const ida_link_ust_ayar_t *ayar,
                               const ida_link_platform_t *plat)
{
    ida_wtime_t simdi;
    if (l == NULL) {
        return IDA_LINK_HATA_ARGUMAN;
    }
    memset(l, 0, sizeof *l);
    if (ayar == NULL || plat == NULL || plat->zaman == NULL || plat->gonder == NULL ||
        plat->kuyruk_temizle == NULL || plat->nonce == NULL || plat->yeniden_baslat == NULL ||
        plat->kalici_blok == NULL) {
        return IDA_LINK_HATA_ARGUMAN;
    }
    if ((ayar->rol != IDA_LINK_UST_ROL_KART && ayar->rol != IDA_LINK_UST_ROL_SIL) ||
        ayar->t_bag_ns < IDA_LINK_UST_T_BAG_MIN_NS || ayar->t_bag_ns > IDA_LINK_UST_T_BAG_MAX_NS ||
        !(ayar->esc_alt_us < ayar->esc_notr_us && ayar->esc_notr_us < ayar->esc_ust_us) ||
        (ayar->init_rc != (uint8_t)IDA_RC_OK && ayar->init_rc != (uint8_t)IDA_RC_PARAM_DEFAULTS)) {
        return IDA_LINK_HATA_ARGUMAN;
    }
    l->ozel_ayar = *ayar;
    l->ozel_plat = *plat;
    if (ida_link_alici_kur(&l->ozel_alici, IDA_LINK_ROL_KART) != IDA_LINK_TAMAM ||
        ida_link_gonderici_kur(&l->ozel_gonderici, IDA_LINK_ROL_KART) != IDA_LINK_TAMAM) {
        return IDA_LINK_HATA_ROL;
    }
    l->ozel_durum = (uint8_t)IDA_LINK_UST_IDLE;
    l->ozel_prev_sess = ayar->prev_sess;
    l->ozel_oturum_sayisi = ayar->sessions_since_boot;
    l->ozel_last_cmd_id = ayar->last_cmd_id;
    simdi = plat->zaman(plat->ctx);
    l->ozel_kova_acan = 4 * INT64_C(5000000);             /* A1-K120: dolu başlar */
    l->ozel_kova_kapan = 3 * INT64_C(5000000);
    l->ozel_kova_zaman = simdi;
    ust_izgara_mutlak(l, simdi);
    l->ozel_kurulu = 1u;
    return IDA_LINK_TAMAM;
}

void ida_link_ust_rx(ida_link_ust_t *l, const uint8_t *veri, size_t n, ida_wtime_t t_rx)
{
    ida_link_cerceve_t c;
    bool ihlal;
    if (l == NULL || !l->ozel_kurulu || (veri == NULL && n > 0u)) {
        return;
    }
    /* W3-K10: take'in YOK dışında döndüğü an çağrıya giriş anıdır; take ile put_out arasında
       rx çağrılmaz (platform yükümlülüğü). İhlalde baytlar kaybedilmeden işlenir (daraltıcı
       mandala girer) ve oturum PROTOCOL ile biter; sonraki çağrı oturum sonu çağrısıdır. */
    ihlal = l->ozel_cagri_suruyor && n > 0u;
    t_rx = ust_zaman_sinirla(t_rx);
    while (n > 0u) {
        size_t t = 0u;
        const ida_link_rc_t rc = ida_link_al(&l->ozel_alici, veri, n, &t, &c);
        if (rc != IDA_LINK_CERCEVE && rc != IDA_LINK_BEKLE) {
            break;
        }
        if (rc == IDA_LINK_CERCEVE) {
            cerceve_isle(l, &c, t_rx);
        }
        if (t == 0u || t > n) {
            break;
        }
        veri += t;
        n -= t;
    }
    if (ihlal) {
        ust_oturum_bitir(l, (uint8_t)IDAW_CLOSE_REASON_PROTOCOL);
    }
}

void ida_link_ust_rx_tasma(ida_link_ust_t *l)
{
    if (l == NULL || !l->ozel_kurulu) {
        return;
    }
    (void)ida_link_alici_tasma(&l->ozel_alici);
    l->ozel_acik.tasma = true;                            /* A1-K09: açık parti eksik */
}

void ida_link_ust_poll_tx(ida_link_ust_t *l)
{
    ida_wtime_t simdi;
    if (l == NULL || !l->ozel_kurulu) {
        return;
    }
    simdi = ust_simdi(l);
    ust_olay_yokla(l, simdi);
    switch (l->ozel_durum) {
    case IDA_LINK_UST_IDLE:
        /* A1-K11 (SIL'de duyuru yok: A1-K121 (iv) tek BOARD_HELLO; Okumalar). */
        if (l->ozel_ayar.rol == IDA_LINK_UST_ROL_KART &&
            (!l->ozel_duyuru_var ||
             simdi.ns - l->ozel_son_duyuru.ns >= IDA_LINK_UST_T_DUYURU_NS)) {
            board_hello_gonder(l, 0u);
            l->ozel_son_duyuru = simdi;
            l->ozel_duyuru_var = 1u;
        }
        return;
    case IDA_LINK_UST_RUNNING:
        if (l->ozel_ayar.rol == IDA_LINK_UST_ROL_KART &&
            simdi.ns - l->ozel_son_rx.ns > (int64_t)l->ozel_ayar.t_bag_ns) {
            ust_oturum_bitir(l, 0u);                      /* A1-K22: T_bağ */
            return;
        }
        break;
    case IDA_LINK_UST_HANDSHAKE:
        break;
    default:
        return;
    }
    if (!l->ozel_tx_var || simdi.ns - l->ozel_son_tx.ns >= IDA_LINK_UST_T_CANLI_NS) {
        keepalive_gonder(l);                              /* A1-K20 */
    }
}

void ida_link_ust_akis_kapandi(ida_link_ust_t *l)
{
    ida_link_sayaclar_t s;
    if (l == NULL || !l->ozel_kurulu) {
        return;
    }
    if (l->ozel_durum == (uint8_t)IDA_LINK_UST_RUNNING) {
        ust_oturum_bitir(l, 0u);                          /* A1-K99: oturumun bitişi */
    } else if (l->ozel_durum == (uint8_t)IDA_LINK_UST_HANDSHAKE) {
        l->ozel_durum = (uint8_t)IDA_LINK_UST_IDLE;
    }
    l->ozel_hello_bekliyor = 0u;
    /* Yarım aday sayaçlara yansımadan atılır (A1-K07); sayaçlar açılıştan beri birikir. */
    s = l->ozel_alici.sayac;
    (void)ida_link_alici_kur(&l->ozel_alici, IDA_LINK_ROL_KART);
    l->ozel_alici.sayac = s;
}

void ida_link_ust_guvenli_bekleme(ida_link_ust_t *l)
{
    if (l == NULL || !l->ozel_kurulu) {
        return;
    }
    l->ozel_guvenli = 1u;
    l->ozel_hello_bekliyor = 0u;
    if (l->ozel_durum == (uint8_t)IDA_LINK_UST_RUNNING) {
        ust_oturum_bitir(l, (uint8_t)IDAW_CLOSE_REASON_SAFE_STATE);   /* A1-K21 */
    } else if (l->ozel_durum == (uint8_t)IDA_LINK_UST_HANDSHAKE) {
        l->ozel_durum = (uint8_t)IDA_LINK_UST_IDLE;
    }
}

bool ida_link_ust_cagri_acik(const ida_link_ust_t *l)
{
    return l != NULL && l->ozel_kurulu && l->ozel_cagri_suruyor;
}

ida_link_ust_durum_t ida_link_ust_durum(const ida_link_ust_t *l)
{
    if (l == NULL || !l->ozel_kurulu) {
        return IDA_LINK_UST_IDLE;
    }
    return (ida_link_ust_durum_t)l->ozel_durum;
}

uint16_t ida_link_ust_sess(const ida_link_ust_t *l)
{
    if (l == NULL || !l->ozel_kurulu || l->ozel_durum != (uint8_t)IDA_LINK_UST_RUNNING) {
        return 0u;
    }
    return l->ozel_sess;
}

void ida_link_ust_sayaclar(const ida_link_ust_t *l, uint32_t cnt[IDA_LINK_SAY_N])
{
    const ida_link_sayaclar_t *s;
    if (l == NULL || cnt == NULL) {
        return;
    }
    memcpy(cnt, l->say, sizeof l->say);
    s = &l->ozel_alici.sayac;
    cnt[IDA_LINK_SAY_RX_BYTES] = s->rx_bytes;
    cnt[IDA_LINK_SAY_RX_FRAMES] = s->rx_frames;
    cnt[IDA_LINK_SAY_RX_CRC_ERR] = s->rx_crc_err;
    cnt[IDA_LINK_SAY_RX_UNKNOWN_MSGID] = s->rx_unknown_msgid;
    cnt[IDA_LINK_SAY_RX_IFLAG] = s->rx_iflag;
    cnt[IDA_LINK_SAY_RX_BAD_LEN] = s->rx_bad_len;
    cnt[IDA_LINK_SAY_RX_WRONG_SYS] = s->rx_wrong_sys;
    cnt[IDA_LINK_SAY_RX_MAV1] = s->rx_mav1;
    cnt[IDA_LINK_SAY_RX_OVERFLOW] = s->rx_overflow;
    cnt[IDA_LINK_SAY_RX_RESYNC_BYTES] = s->rx_resync_bytes;
    cnt[IDA_LINK_SAY_RX_SEQ_GAP] = s->rx_seq_gap;
}
