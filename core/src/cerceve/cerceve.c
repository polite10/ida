/*
 * cerceve.c — port işlevleri ve çağrı çerçevesi (W2; A4 §3.11, 4.14, 4.15; A4-K02, K08, K10,
 * K15, K17, K25, K26, K31, K46, K72, K73, K77, K78, K89, K98, K109, K113, K117).
 */
#include "cerceve/cerceve.h"

#include <string.h>

#include "cekirdek.h"
#include "cerceve/cikti.h"
#include "cerceve/komut.h"
#include "zaman/zaman.h"

/* ---- Kurulum, bütünlük ------------------------------------------------------------------ */

bool ida_cer_kurulu(const ida_core_t *c)
{
    return c->kurulu == IDA_KURULU_ISARETI;
}

uint8_t ida_cer_param_grup(const ida_prm_kume_t *k)
{
    /* A4-K37, 3.9: T_lag_clear ≤ T_lag_set (ikisi de ms). */
    return (k->ham[IDA_PRM_lag_clear_ms] <= k->ham[IDA_PRM_lag_set_ms]) ? (uint8_t)0u
                                                                        : (uint8_t)1u;
}

_Static_assert(sizeof(bool) == 1u, "bool tek bayt");

/* bool nesnesinin ham baytı 0 ya da 1 mi (bozuk bellekteki bool değer olarak okunmaz). */
static bool bool_bayti_gecerli(const bool *b)
{
    uint8_t v = 0u;
    (void)memcpy(&v, b, sizeof(v));
    return v <= 1u;
}

static int32_t ustune_yazilacak_bozuk(const ida_core_t *c);

/* A4-K89: 0 tutarlı; 1 koruma sözcüğü, 2 tamamlayıcı kopya, 3 geçersiz durum değeri. */
static int32_t butunluk(const ida_core_t *c)
{
    if ((c->koruma_bas != IDA_KORUMA_BAS) || (c->koruma_son != IDA_KORUMA_SON)) {
        return 1;
    }
    if ((c->kurulu_tumleyen != (uint32_t)~c->kurulu) || !ida_prm_tutarli(&c->prm)) {
        return 2;
    }
    /* Emniyet/FC alanları (b)'den önceki denetimle aynı; ardından modüllerin kendi
       denetimleri. Tanımsız bit denetimleri uint32_t'dedir (tümleyen sabit daraltılmaz,
       MSVC C4310). */
    const bool durum = (ustune_yazilacak_bozuk(c) == 0) && ida_kes_tutarli(&c->kes) &&
                       ida_gud_tutarli(&c->gud) && ida_gor_tutarli(&c->gor) &&
                       ida_kon_tutarli(&c->kon) && ida_son_tutarli(&c->son);
    return durum ? 0 : 3;
}

/* (b) ve (b3)'ün koşulsuz üzerine yazabildiği durum alanları (kip, etkin emniyet değerleri,
   atış kontrol durumu): yazımdan önce geçerlilikleri denetlenir ki bozulmanın kanıtı
   kaybolmasın (A4-K89; sol W2-cekirdek bulgu 1). Yalnız okur; 3 ya da 0 döner. */
static int32_t ustune_yazilacak_bozuk(const ida_core_t *c)
{
    const uint32_t tup_tanimli = IDA_TUBE_LOADED | IDA_TUBE_PLUG_IN | IDA_TUBE_PIN_OUT |
                                 IDA_TUBE_PERMIT_OPEN;
    const bool gecerli = (c->emn.mode <= (uint8_t)IDA_MODE_AUTO) &&
                         (((uint32_t)c->emn.kisitli & ~(uint32_t)IDA_EMN_TUMU) == 0u) &&
                         bool_bayti_gecerli(&c->emn.armed) &&
                         bool_bayti_gecerli(&c->emn.inst0_var) &&
                         (((uint32_t)c->emn.inst0_tube[0] & ~tup_tanimli) == 0u) &&
                         (((uint32_t)c->emn.inst0_tube[1] & ~tup_tanimli) == 0u) &&
                         (c->emn.kusak_tumleyen == (0xFFFFFFFFu ^ c->emn.kusak)) &&
                         (((uint32_t)c->emn.hedef_iptal & ~(uint32_t)IDA_EMN_IPTAL_TUMU) == 0u) &&
                         ida_fc_tutarli(&c->fc);
    return gecerli ? 0 : 3;
}

static void gunluk(ida_core_t *c, const ida_log_t *k)
{
    ida_gun_ekle(&c->gun, k);
}

/* A4-K89: saptanan bozulmayı (bozuk ≠ 0) çağrının bütünlük sonucuna taşır: core_ok mandallı
   olarak 0 olur, CORE_INTEGRITY o bozulma için bir kez yazılır. Çağrı başındaki denetim ve
   çağrı içinde (PARAM_SAVE'in özet denetimi) saptanan bozulma aynı yoldan geçer. */
static void butunluk_bildir(ida_core_t *c, ida_cagri_t *cg, int32_t bozuk)
{
    ida_cerceve_t *k = &c->cer;
    if ((bozuk != 0) && k->core_ok) {
        const ida_log_t g = {.t_w = cg->now,
                             .code = (uint16_t)IDA_LOG_CORE_INTEGRITY,
                             .arg0 = bozuk,
                             .arg1 = 0};
        gunluk(c, &g);
    }
    cg->core_ok = (bozuk == 0) && k->core_ok;
    k->core_ok = cg->core_ok;
}

/* A4-K10: kullanılan now_w = max(önceki, verilen); aralık içi her çağrı izi günceller. */
static ida_wtime_t now_guncelle(ida_core_t *c, ida_wtime_t verilen)
{
    ida_cerceve_t *k = &c->cer;
    ida_wtime_t kullanilan;
    kullanilan.ns = k->now_izi_ns;
    if (!ida_wt_gecerli(verilen)) {
        return kullanilan;
    }
    if (verilen.ns < k->now_izi_ns) {
        k->cnt.wall_backstep++;
        const ida_log_t g = {.t_w = kullanilan,
                             .code = (uint16_t)IDA_LOG_WALL_BACKSTEP,
                             .arg0 = ida_ns_us_doyur(k->now_izi_ns - verilen.ns),
                             .arg1 = 0};
        gunluk(c, &g);
        return kullanilan;
    }
    k->now_izi_ns = verilen.ns;
    return verilen;
}

/* ---- ida_core_info, ida_core_init (A4-K02, K77, K78, K86, K98; 3.11 init sırası) ---------- */

void ida_core_info(ida_core_info_t *info)
{
    if (info == NULL) {
        return;
    }
    info->port_major = (uint16_t)IDA_PORT_VERSION_MAJOR;
    info->port_minor = (uint16_t)IDA_PORT_VERSION_MINOR;
    info->core_size = (uint32_t)sizeof(ida_core_t);
    info->core_align = (uint32_t)_Alignof(ida_core_t);
    info->param_schema_hash = ida_prm_sema_ozeti();
    info->core_version = IDA_CORE_VERSION;
}

static void kurulmamis_isaretle(ida_core_t *c)
{
    c->kurulu = 0u;
    c->kurulu_tumleyen = 0u;
}

ida_rc_t ida_core_init(ida_core_t *c, const ida_init_in_t *in)
{
    if (c == NULL) {
        return IDA_RC_BAD_INPUT;
    }
    kurulmamis_isaretle(c);
    if (in == NULL) {
        return IDA_RC_BAD_INPUT;
    }
    if ((in->port_major != (uint16_t)IDA_PORT_VERSION_MAJOR) ||
        (in->port_minor != (uint16_t)IDA_PORT_VERSION_MINOR)) {
        return IDA_RC_VERSION;
    }
    if (!ida_wt_gecerli(in->now_w) || ((in->param_blob == NULL) && (in->param_len > 0u))) {
        return IDA_RC_BAD_INPUT;
    }

    (void)memset(c, 0, sizeof(*c));
    c->koruma_bas = IDA_KORUMA_BAS;
    c->koruma_son = IDA_KORUMA_SON;

    /* Parametreler (A4-K78, K80): blok yoksa ya da geçersizse derlenmiş varsayılanlar. */
    ida_prm_kur(&c->prm);
    ida_prm_varsayilan(&c->prm.yururluk);
    uint8_t denetim = 0u;
    if ((in->param_blob != NULL) && (in->param_len > 0u)) {
        if (in->param_len > IDA_PARAM_BLOB_MAX) {
            denetim = 3u;
        } else {
            denetim = ida_prm_blok_coz(in->param_blob, in->param_len, c->prm.sema_ozeti,
                                       &c->prm.yururluk);
        }
        c->prm.param_ok = (denetim == 0u);
    } else {
        c->prm.param_ok = false;
    }
    const ida_rc_t rc = c->prm.param_ok ? IDA_RC_OK : IDA_RC_PARAM_DEFAULTS;

    c->cer.now_izi_ns = in->now_w.ns;
    c->cer.boot_count = in->boot_count;
    c->cer.core_ok = true;
    ida_emn_kur(&c->emn);
    ida_olay_kur(&c->olay, in->boot_count, in->boot_count_ok);
    ida_orn_kur(&c->orn);
    ida_imz_kur(&c->imz);
    ida_gun_kur(&c->gun);
    ida_kes_kur(&c->kes);
    ida_gud_kur(&c->gud);
    ida_gor_kur(&c->gor);
    ida_kon_kur(&c->kon);
    ida_fc_kur(&c->fc, in->now_w);
    ida_son_kur(&c->son);

    /* İlk yazılabilir step/tick'te çıkacak açılış kayıtları (3.15; A4-K77). */
    const uint8_t neden =
        (in->reset_cause <= (uint8_t)IDA_RESET_SIL_REBOOT) ? in->reset_cause : (uint8_t)0u;
    const ida_log_t g1 = {.t_w = in->now_w,
                          .code = (uint16_t)IDA_LOG_RESET_INFO,
                          .arg0 = (int32_t)neden,
                          .arg1 = (int32_t)in->consecutive_faults};
    gunluk(c, &g1);
    const ida_log_t g2 = {.t_w = in->now_w,
                          .code = (uint16_t)IDA_LOG_PARAM_LOAD,
                          .arg0 = (int32_t)rc,
                          .arg1 = (int32_t)denetim};
    gunluk(c, &g2);

    c->kurulu = IDA_KURULU_ISARETI;
    c->kurulu_tumleyen = (uint32_t)~IDA_KURULU_ISARETI;
    return rc;
}

/* ---- (c) yapısal doğrulama, (d) sıra ------------------------------------------------------ */

static bool daraltici_hedef(uint8_t m)
{
    return (m == (uint8_t)IDA_MODE_MANUAL) || (m == (uint8_t)IDA_MODE_STABILIZE) ||
           (m == (uint8_t)IDA_MODE_DEPTH_HOLD) || (m == (uint8_t)IDA_MODE_STATION_KEEP);
}

static bool kapasite_asimi(const ida_in_t *in)
{
    return (in->n_samples > IDA_PORT_MAX_SAMPLES) || (in->n_cmds > IDA_PORT_MAX_CMDS) ||
           (in->n_det > IDA_PORT_MAX_DET_IN) || (in->n_det_batch > IDA_PORT_MAX_DET_BATCH) ||
           (in->n_evres > IDA_PORT_MAX_EVRES);
}

static bool mandal_gecersiz(const ida_in_t *in)
{
    if (((uint32_t)in->narrow.mask & ~(uint32_t)IDA_NARROW_DEFINED) != 0u) {
        return true;
    }
    if (((in->narrow.mask & IDA_NARROW_MODE) != 0u) && !daraltici_hedef(in->narrow.mode)) {
        return true;
    }
    /* cmds[]'te hedefi 0, 1 ya da 8 ve üstü olan SET_MODE (A4-K46). */
    const uint32_t n = (in->n_cmds < IDA_PORT_MAX_CMDS) ? (uint32_t)in->n_cmds : IDA_PORT_MAX_CMDS;
    for (uint32_t i = 0u; i < n; i++) {
        const ida_cmd_t *k = &in->cmds[i];
        if ((k->type == (uint8_t)IDA_CMD_SET_MODE) &&
            ((k->u.set_mode.mode < (uint8_t)IDA_MODE_MANUAL) ||
             (k->u.set_mode.mode > (uint8_t)IDA_MODE_AUTO))) {
            return true;
        }
    }
    return false;
}

static bool tf_gecersiz(const ida_cerceve_t *k, const ida_time_frame_t *tf, ida_wtime_t now)
{
    if (!tf->present) {
        return false;
    }
    if (!ida_st_gecerli(tf->t) || !ida_wt_gecerli(tf->t_rx) || (tf->t_rx.ns > now.ns)) {
        return true;
    }
    /* A4-K15: seq arttıysa t kesin artmış olmalı. */
    return k->tf_var && (ida_seri_karsilastir(tf->seq, k->tf_seq) == IDA_SERI_YENI) &&
           (tf->t.ns <= k->tf_t_ns);
}

static ida_rc_t yapisal(const ida_cerceve_t *k, const ida_in_t *in, const ida_cagri_t *cg)
{
    const bool imu_dogru = cg->step ? in->imu_present : !in->imu_present; /* A4-K17 */
    if (!ida_wt_gecerli(in->now_w) || tf_gecersiz(k, &in->tf, cg->now) || !imu_dogru ||
        kapasite_asimi(in) || mandal_gecersiz(in) ||
        (in->plat.link_event > (uint8_t)IDA_LINKEV_SESSION_END) ||
        (in->plat.link_state > (uint8_t)IDA_LINK_RUNNING)) {
        return IDA_RC_BAD_INPUT;
    }
    return IDA_RC_OK;
}

static ida_rc_t sira(const ida_cerceve_t *k, const ida_time_frame_t *tf)
{
    if (!tf->present || !k->tf_var) {
        return IDA_RC_OK; /* ızgara tick'i ya da ilk zaman çerçevesi (A4-K25) */
    }
    switch (ida_seri_karsilastir(tf->seq, k->tf_seq)) {
    case IDA_SERI_AYNI:
        return IDA_RC_DUP_SEQ;
    case IDA_SERI_ESKI:
        return IDA_RC_OLD_SEQ;
    default:
        return IDA_RC_OK;
    }
}

/* ---- Çağrı başına izler (A4-K109, K117; 3.8 step_miss) ----------------------------------- */

/* (b2) çağrısında yalnız taban kurulur (init'ten sonraki ilk çağrıysa); sayım yapılmaz. */
static void step_miss_tabani(ida_cerceve_t *k, const ida_in_t *in)
{
    if (!k->plat_taban_var) {
        k->plat_taban_var = true;
        k->plat_skipped = in->plat.cnt_batch_skipped;
        k->plat_merged = in->plat.cnt_batch_merged;
    }
}

static void cagri_izleri(ida_core_t *c, const ida_in_t *in, ida_cagri_t *cg)
{
    ida_cerceve_t *k = &c->cer;
    cg->bg_bad_input = (k->bg_kotu > 0u);
    if (cg->bg_bad_input) {
        const ida_log_t g = {.t_w = cg->now,
                             .code = (uint16_t)IDA_LOG_BG_BAD_INPUT,
                             .arg0 = (k->bg_kotu > (uint32_t)INT32_MAX) ? INT32_MAX
                                                                        : (int32_t)k->bg_kotu,
                             .arg1 = 0};
        gunluk(c, &g);
    }
    if (in->bg_slices != k->bg_dilim) {
        k->cnt.bg_slice_mismatch++;
        const ida_log_t g = {.t_w = cg->now,
                             .code = (uint16_t)IDA_LOG_BG_SLICE_MISMATCH,
                             .arg0 = (int32_t)in->bg_slices,
                             .arg1 = (int32_t)k->bg_dilim};
        gunluk(c, &g);
    }
    k->bg_dilim = 0u;
    k->bg_kotu = 0u;
    /* step_miss (3.8): taban init'ten sonraki ilk step/tick çağrısının (out NULL olsa da)
       plat değerleridir; taban çağrısında görülen prev_overrun = 1 de sayılmaz. */
    if (k->plat_taban_var) {
        k->cnt.step_miss += (in->plat.cnt_batch_skipped - k->plat_skipped) +
                            (in->plat.cnt_batch_merged - k->plat_merged);
        if (in->plat.prev_overrun) {
            k->cnt.step_miss++;
        }
    }
    k->plat_taban_var = true;
    k->plat_skipped = in->plat.cnt_batch_skipped;
    k->plat_merged = in->plat.cnt_batch_merged;
}

/* ---- (e) olağan işleniş ------------------------------------------------------------------- */

static void kabul_edilen_ornekler(ida_core_t *c, const ida_in_t *in, const ida_orn_sonuc_t *os,
                                  const ida_fc_ortam_t *fo)
{
    for (uint32_t i = 0u; i < (uint32_t)os->n; i++) {
        const ida_sample_t *s = &in->samples[os->sira[i]];
        switch (s->hdr.type) {
        case IDA_SENS_SAFETY:
            ida_emn_kabul_kisit(&c->emn, &s->u.safety);
            ida_fc_emniyet_ornegi(&c->fc, &c->emn, s, fo);
            break;
        case IDA_SENS_BATT:
        case IDA_SENS_LEAK:
            ida_gor_ornek(&c->gor, s);
            break;
        default:
            ida_kes_ornek(&c->kes, s);
            break;
        }
    }
}

static void olay_sonuclari(ida_core_t *c, const ida_in_t *in, const ida_cagri_t *cg,
                           const ida_fc_ortam_t *fo)
{
    ida_core_counters_t *cnt = &c->cer.cnt;
    /* (c) n_evres ≤ 4'ü doğruladı; sınır çözümleyicinin görebilmesi için ayrıca kırpılır. */
    const uint32_t n =
        (in->n_evres < IDA_PORT_MAX_EVRES) ? (uint32_t)in->n_evres : IDA_PORT_MAX_EVRES;
    for (uint32_t i = 0u; i < n; i++) {
        ida_olay_bekleyen_t b;
        if (ida_olay_sonuc(&c->olay, &in->evres[i], &b)) {
            const ida_log_t g = {.t_w = cg->now,
                                 .code = (uint16_t)IDA_LOG_EVENT_RESULT,
                                 .arg0 = (int32_t)b.kind,
                                 .arg1 = (int32_t)in->evres[i].result};
            gunluk(c, &g);
            ida_fc_olay_sonucu(&c->fc, &b, &in->evres[i], fo);
        } else {
            cnt->evres_ignored++;
        }
    }
    ida_olay_bekleyen_t b;
    while (ida_olay_zaman_asimi(&c->olay, cg->now, &b)) {
        cnt->event_unknown++;
        const ida_log_t g = {.t_w = cg->now,
                             .code = (uint16_t)IDA_LOG_EVENT_UNKNOWN,
                             .arg0 = (int32_t)b.kind,
                             .arg1 = (int32_t)b.tube};
        gunluk(c, &g);
        ida_fc_olay_bilinmiyor(&c->fc, &b, fo);
    }
}

static bool cagrida_arm(const ida_in_t *in)
{
    const uint32_t n = (in->n_cmds < IDA_PORT_MAX_CMDS) ? (uint32_t)in->n_cmds : IDA_PORT_MAX_CMDS;
    for (uint32_t i = 0u; i < n; i++) {
        if (in->cmds[i].type == (uint8_t)IDA_CMD_ARM) {
            return true;
        }
    }
    return false;
}

static void olagan(ida_core_t *c, const ida_in_t *in, ida_out_t *out, ida_cagri_t *cg)
{
    ida_cerceve_t *k = &c->cer;
    const ida_fc_ortam_t fo = {.now = cg->now, .prm = &c->prm, .gun = &c->gun};
    if (k->yeniden_baslat) {
        /* SESSION_START sonrası modüllerin yeniden başlaması (A4-K72). */
        ida_kes_oturum_basi(&c->kes);
        ida_gud_oturum_basi(&c->gud);
        ida_gor_oturum_basi(&c->gor);
        ida_kon_oturum_basi(&c->kon);
        ida_son_oturum_basi(&c->son);
        k->yeniden_baslat = false;
    }
    ida_prm_cagri_basi(&c->prm);
    ida_prm_depo_izle(&c->prm, &in->plat, &c->gun, cg->now);

    /* Örnek kapısı (A4-K30, K33, K35, K103, K115). */
    const ida_orn_baglam_t ob = {.now = cg->now, .tf = &in->tf, .cnt = &k->cnt, .gun = &c->gun};
    if (cg->step) {
        cg->imu_kabul = ida_orn_imu(&c->orn, &in->imu, &ob);
        cg->imu_red = !cg->imu_kabul;
        if (cg->imu_red) {
            k->cnt.imu_rejected++;
        }
    }
    ida_orn_sonuc_t os;
    ida_orn_degerlendir(&c->orn, in, &ob, &os);
    cg->cakisma = os.cakisma;
    const ida_imz_baglam_t zb = {.now = cg->now, .prm = &c->prm, .gun = &c->gun, .cnt = &k->cnt};
    if (cg->imu_kabul) {
        ida_imz_imu_kabul(&c->imz, &in->imu, &zb);
        ida_kes_imu(&c->kes, &in->imu);
    }
    kabul_edilen_ornekler(c, in, &os, &fo);
    if ((in->ping360 != NULL) && ida_orn_ping360(&c->orn, in->ping360, &ob)) {
        ida_son_ping360(&c->son, in->ping360);
    }
    ida_fc_algilama(&c->fc, in, cg->now);

    /* Zaman çerçevesi kabul edilir (A4-K25). */
    if (in->tf.present) {
        k->tf_var = true;
        k->tf_seq = in->tf.seq;
        k->tf_t_ns = in->tf.t.ns;
    }

    /* IMU zamanlaması (A4-K37, K39). */
    ida_imz_lag_guncelle(&c->imz, &in->tf, &zb);
    cg->imu_lag = c->imz.lag;
    cg->imu_bayat = ida_imz_bayat(&c->imz, cg->now, &c->prm);
    ida_imz_bayat_izle(&c->imz, cg->imu_bayat, &zb);

    olay_sonuclari(c, in, cg, &fo);

    if (cg->step && cg->imu_kabul) {
        ida_kes_adim(&c->kes, c);
    } else {
        ida_kes_tick(&c->kes, c);
    }
    ida_gor_adim(&c->gor, &c->emn, c);

    /* Komutlar (tablo 3.17): engel maskesi bu noktadaki değerdir. */
    const ida_fc_baglam_t fb = {
        .armed = c->emn.armed,
        .mode = c->emn.mode,
        .est_level = ida_kes_duzey(&c->kes),
        .fs_level = ida_gor_fs_duzey(&c->gor),
        .events_ok = ida_olay_kullanilabilir(&c->olay),
        .imu_lag = cg->imu_lag,
        .imu_bayat = cg->imu_bayat,
        .oturum_var = (in->plat.link_state == (uint8_t)IDA_LINK_RUNNING),
        .emn_fci = ida_emn_fci(&c->emn),
        .bekleyen_tupler = ida_olay_bekleyen_tupler(&c->olay),
        .gcs_son_ns = c->gor.gcs_son_ns,
    };
    /* Atış kontrol: süreler ve evrensel geçişler komutlardan önce (W5; TASARIM §10). */
    ida_fc_cagri(&c->fc, &c->emn, &fb, &fo);
    const ida_komut_baglam_t kb = {
        .now_w = cg->now,
        .daraldi = c->emn.d.herhangi || ida_gor_daraldi(&c->gor), /* A4-K45; W3-K9 */
        .imu_red = cg->imu_red,
        .imu_lag = cg->imu_lag,
        .armed = c->emn.armed,
        .param_ok = c->prm.param_ok,
        .events_ok = ida_olay_kullanilabilir(&c->olay),
        .cagrida_arm = cagrida_arm(in),
        .oturum_var = fb.oturum_var,
        .mode = c->emn.mode,
        .est_level = fb.est_level,
        .fs_level = fb.fs_level,
        .param_store = in->plat.param_store,
        .fc_engel = ida_fc_engel(&c->fc, &fb),
    };
    ida_kom_isle(c, in, &kb, out);

    ida_gud_adim(&c->gud, c);
    const bool step_ok = cg->step && cg->imu_kabul;
    /* Atış kontrol: komutlardan sonra, kabul edilen her çağrıda; genişletici aday yalnız
       step_ok (A4-K64). Bağlam komutlardan sonraki değerlerle yeniden kurulur. */
    ida_fc_baglam_t fb2 = fb;
    fb2.armed = c->emn.armed;
    fb2.mode = c->emn.mode;
    fb2.fs_level = ida_gor_fs_duzey(&c->gor);
    fb2.events_ok = ida_olay_kullanilabilir(&c->olay);
    fb2.emn_fci = ida_emn_fci(&c->emn);
    fb2.bekleyen_tupler = ida_olay_bekleyen_tupler(&c->olay);
    fb2.gcs_son_ns = c->gor.gcs_son_ns;
    ida_fc_adim(&c->fc, &c->emn, &fb2, &c->olay, step_ok, &fo);
    if (step_ok) {
        ida_kon_adim(&c->kon, c);
        ida_son_adim(&c->son, c, out);
    }
    /* PARAM_COMMIT ve PARAM_SAVE çağrı sonunda, komutların girdideki sırasından bağımsız
       değerlendirilir; commit adım sınırında uygulanır (A4-K81, K82). */
    ida_prm_komut_baglam_t pb = {
        .k = NULL,
        .plat_param_gen = in->plat.param_gen,
        .gun = &c->gun,
        .sonuc_indisi = 0u,
        .armed = c->emn.armed};
    pb.k = &kb; /* yerel değişkenin adresi küme ilklendiricisine yazılmaz (MSVC C4221) */
    const int32_t save_bozuk = ida_prm_cagri_sonu(&c->prm, &pb, out, &k->cnt);
    if (save_bozuk != 0) {
        butunluk_bildir(c, cg, save_bozuk); /* aynı çağrının health ve günlüğünde */
    }
    if (ida_emn_cagri_sonu(&c->emn)) {
        k->cnt.narrow_from_rejected++;
    }
    cg->rc = (cg->step && !cg->imu_kabul) ? IDA_RC_IMU_REJECTED : IDA_RC_OK;
}

/* ---- step ve tick'in ortak gövdesi (A4-K113) ---------------------------------------------- */

ida_rc_t ida_cer_cagri(ida_core_t *c, const ida_in_t *in, ida_out_t *out, uint8_t src)
{
    /* (a1) */
    if ((c == NULL) || (in == NULL)) {
        return IDA_RC_BAD_INPUT;
    }
    /* (a2): örneğin belleği okunmaz (A4-K126). */
    if (!ida_cer_kurulu(c)) {
        if (out != NULL) {
            ida_cik_kurulmamis(in, src, out);
        }
        return IDA_RC_NOT_INIT;
    }
    ida_cerceve_t *k = &c->cer;
    ida_cagri_t cg;
    (void)memset(&cg, 0, sizeof(cg));
    cg.src = src;
    cg.step = (src == (uint8_t)IDA_SRC_STEP);
    cg.now = now_guncelle(c, in->now_w);

    /* (b) ve (b3)'ün üzerine yazacağı alanların bozulması yazımdan önce kaydedilir; kayıt
       sonraki yazılabilir çağrının iç tutarlılık sonucuna taşınır (A4-K89). Daraltma bu
       sonuçtan bağımsız olarak her koşulda uygulanır. */
    const int32_t on_bozuk = ustune_yazilacak_bozuk(c);
    if ((on_bozuk != 0) && (k->bozuk_izi == 0)) {
        k->bozuk_izi = on_bozuk;
    }

    /* (b) daraltma: geri alınmaz, reddedilen ve out NULL çağrıda da. */
    const ida_emn_baglam_t eb = {.gun = &c->gun, .now = cg.now};
    ida_emn_daralt(&c->emn, in, &eb);
    const ida_fc_ortam_t fo = {.now = cg.now, .prm = &c->prm, .gun = &c->gun};
    ida_fc_daralt(&c->fc, &c->emn, &fo);
    ida_olay_kapat_iste(&c->olay, c->emn.d.kapat);
    if (c->emn.d.oturum_sonu) {
        const ida_log_t g = {.t_w = cg.now,
                             .code = (uint16_t)IDA_LOG_SESSION,
                             .arg0 = (int32_t)IDA_LINKEV_SESSION_END,
                             .arg1 = 0};
        gunluk(c, &g);
    }

    /* (b3) SESSION_START (A4-K72; 3.11 (b3)'ün kapalı listesi): (b) gibi her çağrıda, out
       NULL olsa da; geri alınmaz. Sıfırlananlar: her (type, inst) için son kabul edilen
       örneğin izi (IMU ve Ping360 dahil), IMU izi (IMU bayat sayılır, taze idiyse
       IMU_STALE = 1; IMU_LAG'ın çözülme sayacı; mandal çözülmez), zaman çerçevesi izi.
       A4-K118'in süre başlangıcı atış kontrolündedir (ida_fc_oturum_basi; W5-K21).
       Başka hiçbir durum sıfırlanmaz; modüllerin yeniden başlaması bir sonraki kabul edilen
       çağrının (e) adımındadır. */
    if (in->plat.link_event == (uint8_t)IDA_LINKEV_SESSION_START) {
        ida_orn_kur(&c->orn);
        ida_imz_oturum_basi(&c->imz, &c->gun, cg.now);
        ida_emn_oturum_basi(&c->emn);
        ida_fc_oturum_basi(&c->fc, &fo);
        k->tf_var = false;
        k->yeniden_baslat = true;
        const ida_log_t g = {.t_w = cg.now,
                             .code = (uint16_t)IDA_LOG_SESSION,
                             .arg0 = (int32_t)IDA_LINKEV_SESSION_START,
                             .arg1 = 0};
        gunluk(c, &g);
    }

    /* (b2): (b) ve (b3) uygulanmış kalır; olay ve günlük bekletilir; now_w izi ve
       wall_backstep yukarıda güncellendi; bg_slices sayımı karşılaştırılmadan sıfırlanır;
       bg_bad_input bayrağı (bg_kotu) silinmez, ilk yazılabilir çağrıya taşınır (A4-K117);
       init'ten sonraki ilk çağrıysa step_miss tabanı kurulur (3.8); cnt.bad_input artmaz;
       iç tutarlılık denetimi sonraki yazılabilir çağrıdadır; başka hiçbir şey yapılmaz. Bu
       çağrının emniyet örnekleri reddedilmiş sayılır (A4-K116). */
    if (out == NULL) {
        k->bg_dilim = 0u;
        step_miss_tabani(k, in);
        if (ida_emn_cagri_sonu(&c->emn)) {
            k->cnt.narrow_from_rejected++;
        }
        return IDA_RC_BAD_INPUT;
    }
    (void)memset(out, 0, sizeof(*out)); /* A4-K27, K121 */

    /* İç tutarlılık (A4-K89): yazılabilir her çağrıda; üzerine yazılmadan önce kaydedilmiş
       bozulma önce gelir. Saptanan bozulma init'e kadar sürer (core_ok mandallı). */
    int32_t bozuk = k->bozuk_izi;
    k->bozuk_izi = 0;
    if (bozuk == 0) {
        bozuk = butunluk(c);
    }
    butunluk_bildir(c, &cg, bozuk);

    cagri_izleri(c, in, &cg);

    /* (c) ve (d); birlikte tutarsa BAD_INPUT. */
    ida_rc_t rc = yapisal(k, in, &cg);
    if (rc == IDA_RC_OK) {
        rc = sira(k, &in->tf);
    }
    if (rc != IDA_RC_OK) {
        if (rc == IDA_RC_BAD_INPUT) {
            k->cnt.bad_input++;
        } else if (rc == IDA_RC_DUP_SEQ) {
            k->cnt.dup_seq++;
        } else {
            k->cnt.old_seq++;
        }
        ida_kom_reddet(in, &k->cnt, out);
        if (ida_emn_cagri_sonu(&c->emn)) {
            k->cnt.narrow_from_rejected++;
        }
        cg.rc = rc;
        cg.imu_lag = c->imz.lag;
        cg.imu_bayat = ida_imz_bayat(&c->imz, cg.now, &c->prm);
        ida_cik_yaz(c, &cg, in, out);
        return rc;
    }

    /* (e) */
    olagan(c, in, out, &cg);
    ida_cik_yaz(c, &cg, in, out);
    return cg.rc;
}

ida_rc_t ida_core_step(ida_core_t *c, const ida_in_t *in, ida_out_t *out)
{
    return ida_cer_cagri(c, in, out, (uint8_t)IDA_SRC_STEP);
}

ida_rc_t ida_core_tick(ida_core_t *c, const ida_in_t *in, ida_out_t *out)
{
    return ida_cer_cagri(c, in, out, (uint8_t)IDA_SRC_TICK);
}

/* ---- Arka plan (A4-K93, K109, K117; 3.11 sırası) ------------------------------------------ */

ida_bg_rc_t ida_core_background(ida_core_t *c, ida_wtime_t now_w)
{
    if (c == NULL) {
        return IDA_BG_BAD_INPUT;
    }
    if (!ida_cer_kurulu(c)) {
        return IDA_BG_IDLE; /* hiçbir şey yazılmaz; now_w denetlenmez */
    }
    if (!ida_wt_gecerli(now_w)) {
        c->cer.cnt.bg_bad_input++;
        c->cer.bg_kotu++;
        return IDA_BG_BAD_INPUT;
    }
    (void)now_guncelle(c, now_w);
    c->cer.bg_dilim++;
    return ida_son_arka_plan(&c->son);
}
