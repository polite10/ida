/*
 * ust_parti.c — pencere ve parti (A1 §4.3, §4.4), bekleyen girdi ve birleştirme (A4-K102),
 * emniyet örneklerinin korunması (A1-K31, K124), Ping360 yeniden birleştirme (A1-K49–K53),
 * ızgara takvimi (A1-K117, A4-K18) ve take.
 */
#include "ust_ic.h"

#include <string.h>

/* ------------------------------------------------------------------ */
/* Pencere yardımcıları                                               */
/* ------------------------------------------------------------------ */

void ust_parti_bosalt(ida_link_ozel_parti_t *p)
{
    /* İçerik sayaçlarla geçersizleşir; dizileri silmeye gerek yoktur. */
    p->n_emn = 0u;
    p->n_diger = 0u;
    p->imu_var = false;
    p->n_det_batch = 0u;
    p->n_det = 0u;
    p->n_uye = 0u;
    p->tasma = false;
    p->dustu = false;
}

static void dustu(ida_link_ust_t *l, ida_link_ozel_parti_t *p)
{
    l->say[IDA_LINK_SAY_MEMBER_OVERFLOW] += 1u;   /* plat.cnt_member_overflow */
    p->dustu = true;                              /* plat.input_dropped */
}

static void bekleyen_bosalt(ida_link_ust_t *l)
{
    ust_parti_bosalt(&l->ozel_bek);
    l->ozel_bek_tf_var = 0u;
    l->ozel_bek_birlesen = 0u;
    l->ozel_bek_eksik = 0u;
    l->ozel_bek_cakisma = 0u;
    l->ozel_bek_ping_var = 0u;
}

static void ping_yuva_bosalt(ida_link_ozel_ping_t *y)
{
    y->dolu = 0u;
    y->bas_var = 0u;
    y->bu_pencerede = 0u;
    y->parca = 0u;
    y->yas = 0u;
}

void ust_parti_oturum_sonu(ida_link_ust_t *l)
{
    ust_parti_bosalt(&l->ozel_acik);
    bekleyen_bosalt(l);
    for (unsigned i = 0; i < IDA_LINK_UST_PING_YUVA; ++i) {
        ping_yuva_bosalt(&l->ozel_ping[i]);
    }
    l->ozel_n_kom = 0u;   /* ertelenmiş ve partideki komutlar (A4-K74; mandal kalır) */
}

void ust_parti_oturum_basi(ida_link_ust_t *l)
{
    ust_parti_bosalt(&l->ozel_acik);
    l->ozel_tf_alindi = 0u;   /* ilk TF her batch_seq ile (A1-K31) */
}

void ust_izgara_mutlak(ida_link_ust_t *l, ida_wtime_t simdi)
{
    const int64_t a = IDA_LINK_UST_IZGARA_NS;
    int64_t k = simdi.ns >= 0 ? simdi.ns / a : 0;
    l->ozel_izgara_sonraki.ns = (k + 1) * a;
    l->ozel_izgara_kurulu = 1u;
}

/* ------------------------------------------------------------------ */
/* Üyeler                                                             */
/* ------------------------------------------------------------------ */

void ust_uye_imu(ida_link_ust_t *l, const mavlink_ida_imu_t *m, ida_wtime_t t_rx)
{
    ida_link_ozel_parti_t *a = &l->ozel_acik;
    if (a->imu_var) {
        dustu(l, a);   /* pencerede ikinci IMU: yenisi kalır (A4-K102 ile aynı kural) */
    }
    ust_esl_imu(m, t_rx, &a->imu);
    a->imu_var = true;
}

void ust_uye_ornek(ida_link_ust_t *l, uint32_t msgid, const void *m, ida_wtime_t t_rx)
{
    ida_link_ozel_parti_t *a = &l->ozel_acik;
    ida_sample_t s;
    uint32_t rev = 0u;
    if (!ust_esl_ornek(msgid, m, t_rx, &s, &rev)) {
        return;
    }
    if (s.hdr.type == (uint8_t)IDA_SENS_SAFETY) {
        if (a->n_emn >= IDA_PORT_MAX_SAMPLES) {
            /* Emniyet örneği atılmaz; kapasite aşımı oturum sonudur (A1-K124, E20, H52). */
            ust_oturum_bitir(l, (uint8_t)IDAW_CLOSE_REASON_PROTOCOL);
            return;
        }
        a->emn[a->n_emn] = s;
        a->emn_rev[a->n_emn] = rev;
        a->n_emn = (uint8_t)(a->n_emn + 1u);
        return;
    }
    if (a->n_diger >= IDA_PORT_MAX_SAMPLES) {
        dustu(l, a);
        return;
    }
    a->diger[a->n_diger] = s;
    a->n_diger = (uint8_t)(a->n_diger + 1u);
}

void ust_uye_det_batch(ida_link_ust_t *l, const mavlink_ida_det_batch_t *m, ida_wtime_t t_rx)
{
    ida_link_ozel_parti_t *a = &l->ozel_acik;
    if (a->n_det_batch >= IDA_PORT_MAX_DET_BATCH) {
        dustu(l, a);
        return;
    }
    ust_esl_det_batch(m, &a->det_batch[a->n_det_batch]);
    a->det_batch_t_rx[a->n_det_batch] = t_rx;
    a->n_det_batch = (uint8_t)(a->n_det_batch + 1u);
}

void ust_uye_det_record(ida_link_ust_t *l, const mavlink_ida_det_record_t *m, ida_wtime_t t_rx)
{
    ida_link_ozel_parti_t *a = &l->ozel_acik;
    if (a->n_det >= IDA_PORT_MAX_DET_IN) {
        dustu(l, a);
        return;
    }
    ust_esl_det_record(m, &a->det[a->n_det]);
    a->det_t_rx[a->n_det] = t_rx;
    a->n_det = (uint8_t)(a->n_det + 1u);
}

/* ------------------------------------------------------------------ */
/* Ping360 (A1-K49–K53)                                               */
/* ------------------------------------------------------------------ */

static ida_link_ozel_ping_t *ping_bul(ida_link_ust_t *l, uint8_t inst, uint32_t seq)
{
    for (unsigned i = 0; i < IDA_LINK_UST_PING_YUVA; ++i) {
        ida_link_ozel_ping_t *y = &l->ozel_ping[i];
        if (y->dolu && y->inst == inst && y->seq == seq) {
            return y;
        }
    }
    return NULL;
}

static ida_link_ozel_ping_t *ping_yuva_ac(ida_link_ust_t *l, uint8_t inst, uint32_t seq)
{
    ida_link_ozel_ping_t *y = NULL;
    for (unsigned i = 0; i < IDA_LINK_UST_PING_YUVA; ++i) {
        if (!l->ozel_ping[i].dolu) {
            y = &l->ozel_ping[i];
            break;
        }
    }
    if (y == NULL) {
        /* İki yuva da dolu: en eskisi boşaltılır (A1-K51). */
        y = &l->ozel_ping[0];
        for (unsigned i = 1; i < IDA_LINK_UST_PING_YUVA; ++i) {
            if ((int32_t)(l->ozel_ping[i].sira - y->sira) < 0) {
                y = &l->ozel_ping[i];
            }
        }
        l->say[IDA_LINK_SAY_PING360_ORPHAN] += 1u;
    }
    memset(y, 0, sizeof *y);
    y->dolu = 1u;
    y->inst = inst;
    y->seq = seq;
    l->ozel_ping_sira += 1u;
    y->sira = l->ozel_ping_sira;
    return y;
}

static uint32_t ping_n_frag(uint16_t n_samples)
{
    return ((uint32_t)n_samples + IDA_LINK_UST_PING_PARCA - 1u) / IDA_LINK_UST_PING_PARCA;
}

void ust_uye_ping_veri(ida_link_ust_t *l, const mavlink_ida_ping360_data_t *m)
{
    ida_link_ozel_ping_t *y;
    size_t ofs;
    uint8_t bit;
    if (m->frag_idx >= IDA_LINK_UST_PING_PARCA_AZAMI) {
        l->say[IDA_LINK_SAY_PING360_BAD] += 1u;           /* HEAD gelmeden ≥ 5 */
        return;
    }
    y = ping_bul(l, m->inst, m->seq);
    if (y != NULL && y->bas_var && (uint32_t)m->frag_idx >= ping_n_frag(y->satir.n_samples)) {
        /* A1-K51: HEAD biliniyorsa sınır satırın gerçek n_frag'idir; parça atılır, satır
           bozulmaz (sol bulgu 8). */
        l->say[IDA_LINK_SAY_PING360_BAD] += 1u;
        return;
    }
    if (y == NULL) {
        y = ping_yuva_ac(l, m->inst, m->seq);
    }
    ofs = (size_t)m->frag_idx * IDA_LINK_UST_PING_PARCA;
    bit = (uint8_t)(1u << m->frag_idx);
    y->bu_pencerede = 1u;
    if (y->parca & bit) {
        /* Yineleme: bayt bayt aynıysa yok sayılır, değilse satır atılır (A1-K51). */
        if (memcmp(&y->satir.data[ofs], m->data, IDA_LINK_UST_PING_PARCA) != 0) {
            ping_yuva_bosalt(y);
            l->say[IDA_LINK_SAY_PING360_BAD] += 1u;
        }
        return;
    }
    memcpy(&y->satir.data[ofs], m->data, IDA_LINK_UST_PING_PARCA);
    y->parca = (uint8_t)(y->parca | bit);
}

static bool ping_bas_ayni(const ida_ping360_t *a, const mavlink_ida_ping360_head_t *m)
{
    return a->hdr.t_meas.ns == m->t_meas_ns && a->hdr.flags == m->flags &&
           a->angle_grad == m->angle_grad && a->n_samples == m->n_samples &&
           a->sample_period == m->sample_period && a->tx_duration_us == m->tx_duration_us &&
           a->tx_freq_khz == m->tx_freq_khz && a->gain == m->gain;
}

void ust_uye_ping_bas(ida_link_ust_t *l, const mavlink_ida_ping360_head_t *m, ida_wtime_t t_rx)
{
    ida_link_ozel_ping_t *y = ping_bul(l, m->inst, m->seq);
    const uint32_t beklenen = ((uint32_t)m->n_samples + IDA_LINK_UST_PING_PARCA - 1u) /
                              IDA_LINK_UST_PING_PARCA;
    const bool gecersiz = m->angle_grad > 399u || m->n_samples < 200u || m->n_samples > 1200u ||
                          m->sample_period < 80u || m->sample_period > 40000u ||
                          (uint32_t)m->n_frag != beklenen;
    if (gecersiz) {
        if (y != NULL) {
            ping_yuva_bosalt(y);
        }
        l->say[IDA_LINK_SAY_PING360_BAD] += 1u;           /* A1 §3.4 aralıkları */
        return;
    }
    if (y == NULL) {
        l->say[IDA_LINK_SAY_PING360_INCOMPLETE] += 1u;    /* hiç parça yok */
        return;
    }
    if (y->bas_var) {
        if (!ping_bas_ayni(&y->satir, m)) {
            ping_yuva_bosalt(y);
            l->say[IDA_LINK_SAY_PING360_BAD] += 1u;       /* çelişkili yineleme */
        }
        return;
    }
    if ((uint32_t)y->parca >= (1u << beklenen)) {
        /* HEAD'den önce gelmiş frag_idx ≥ n_frag parçaları: her biri atılır ve ping360_bad
           sayılır; satır korunur (A1-K51 "DATA'yı atar"). */
        for (uint32_t i = beklenen; i < IDA_LINK_UST_PING_PARCA_AZAMI; ++i) {
            if ((y->parca & (1u << i)) != 0u) {
                l->say[IDA_LINK_SAY_PING360_BAD] += 1u;
            }
        }
        y->parca = (uint8_t)(y->parca & ((1u << beklenen) - 1u));
    }
    ust_esl_ping_bas(m, t_rx, &y->satir);   /* veri alanına dokunmaz */
    y->bas_var = 1u;
    y->bu_pencerede = 1u;
}

/* Parti kapanışında: HEAD'i bu pencerede gelen satırlar teslim edilir (A1-K50), HEAD'siz
   yuvalar yaşlanır (A1-K51). */
static void ping_kapanis(ida_link_ust_t *l)
{
    for (unsigned i = 0; i < IDA_LINK_UST_PING_YUVA; ++i) {
        ida_link_ozel_ping_t *y = &l->ozel_ping[i];
        if (!y->dolu) {
            continue;
        }
        y->bu_pencerede = 0u;
        if (y->bas_var) {
            const uint32_t n_frag = ((uint32_t)y->satir.n_samples + IDA_LINK_UST_PING_PARCA - 1u) /
                                    IDA_LINK_UST_PING_PARCA;
            if ((uint32_t)y->parca == (1u << n_frag) - 1u) {
                if (l->ozel_bek_ping_var) {
                    dustu(l, &l->ozel_bek);               /* en yeni satır kalır (A4-K102) */
                }
                l->ozel_bek_ping = y->satir;
                memset(&l->ozel_bek_ping.data[y->satir.n_samples], 0,
                       (size_t)(IDA_PING360_MAX_SAMPLES - y->satir.n_samples));
                l->ozel_bek_ping_var = 1u;
            } else {
                l->say[IDA_LINK_SAY_PING360_INCOMPLETE] += 1u;
            }
            ping_yuva_bosalt(y);
            continue;
        }
        y->yas = (uint8_t)(y->yas + 1u);
        if (y->yas >= IDA_LINK_UST_T_YETIM) {
            ping_yuva_bosalt(y);
            l->say[IDA_LINK_SAY_PING360_ORPHAN] += 1u;
        }
    }
}

/* ------------------------------------------------------------------ */
/* Parti kapanışı (A1-K28–K32, K124; A4-K102)                         */
/* ------------------------------------------------------------------ */

/* Bekleyen girdiye emniyet örneği ekler. false: kapasite aşıldı, oturum bitti. */
static bool emn_ekle(ida_link_ust_t *l, const ida_sample_t *s, uint32_t rev, bool gecersiz_yap)
{
    ida_link_ozel_parti_t *p = &l->ozel_bek;
    if (p->n_emn >= IDA_PORT_MAX_SAMPLES) {
        ust_oturum_bitir(l, (uint8_t)IDAW_CLOSE_REASON_PROTOCOL);   /* A1-K124, H52 */
        return false;
    }
    p->emn[p->n_emn] = *s;
    if (gecersiz_yap) {
        /* A4-K130: öteki alanlar değişmez, yalnız VALID = 0 (gevşetemez). */
        p->emn[p->n_emn].hdr.flags = (uint16_t)(p->emn[p->n_emn].hdr.flags &
                                                UST_HF_VALID_SIL);
    }
    p->emn_rev[p->n_emn] = rev;
    p->n_emn = (uint8_t)(p->n_emn + 1u);
    return true;
}

/* DUP ya da OLD pencere (A1-K31): üyeler atılır, geçerli emniyet örnekleri VALID = 0 ile
   sıradaki çağrıya taşınır (A1-K124). */
static void pencere_at(ida_link_ust_t *l)
{
    ida_link_ozel_parti_t *a = &l->ozel_acik;
    for (unsigned i = 0; i < a->n_emn; ++i) {
        if (!emn_ekle(l, &a->emn[i], a->emn_rev[i], true)) {
            return;
        }
    }
    for (unsigned i = 0; i < IDA_LINK_UST_PING_YUVA; ++i) {
        ida_link_ozel_ping_t *y = &l->ozel_ping[i];
        if (y->dolu && y->bu_pencerede) {
            ping_yuva_bosalt(y);                          /* satırın bir parçası atıldı */
            l->say[IDA_LINK_SAY_PING360_INCOMPLETE] += 1u;
        }
    }
    ust_komut_pencere_atildi(l);
    ust_olay_pencere_atildi(l);
    ust_parti_bosalt(a);
}

static void birlestir(ida_link_ust_t *l, const ida_time_frame_t *tf, bool eksik, bool cakisma)
{
    ida_link_ozel_parti_t *p = &l->ozel_bek;
    ida_link_ozel_parti_t *a = &l->ozel_acik;
    if (l->ozel_bek_tf_var) {
        /* A4-K102: en yeni parti dışındaki emniyet örnekleri VALID = 0 (A1-K124). */
        for (unsigned i = 0; i < p->n_emn; ++i) {
            p->emn[i].hdr.flags = (uint16_t)(p->emn[i].hdr.flags & UST_HF_VALID_SIL);
        }
        l->ozel_bek_birlesen = (uint8_t)(l->ozel_bek_birlesen < 255u ? l->ozel_bek_birlesen + 1u
                                                                     : 255u);
        l->say[IDA_LINK_SAY_BATCH_MERGED] += 1u;
    }
    for (unsigned i = 0; i < a->n_emn; ++i) {
        if (!emn_ekle(l, &a->emn[i], a->emn_rev[i], false)) {
            return;
        }
    }
    for (unsigned i = 0; i < a->n_diger; ++i) {
        if (p->n_diger >= IDA_PORT_MAX_SAMPLES) {
            dustu(l, p);
            continue;
        }
        p->diger[p->n_diger] = a->diger[i];
        p->n_diger = (uint8_t)(p->n_diger + 1u);
    }
    if (a->imu_var) {
        if (p->imu_var) {
            dustu(l, p);                                  /* IMU en yeni partininki */
        }
        p->imu = a->imu;
        p->imu_var = true;
    } else if (p->imu_var) {
        /* A4-K102: IMU varlığı da en yeni partiden; yeni partide IMU yoksa eskisi düşer ve
           çağrı tick'tir (A1-K32; sol bulgu 3). */
        dustu(l, p);
        p->imu_var = false;
    }
    for (unsigned i = 0; i < a->n_det_batch; ++i) {
        if (p->n_det_batch >= IDA_PORT_MAX_DET_BATCH) {
            dustu(l, p);
            continue;
        }
        p->det_batch[p->n_det_batch] = a->det_batch[i];
        p->det_batch_t_rx[p->n_det_batch] = a->det_batch_t_rx[i];
        p->n_det_batch = (uint8_t)(p->n_det_batch + 1u);
    }
    for (unsigned i = 0; i < a->n_det; ++i) {
        if (p->n_det >= IDA_PORT_MAX_DET_IN) {
            dustu(l, p);
            continue;
        }
        p->det[p->n_det] = a->det[i];
        p->det_t_rx[p->n_det] = a->det_t_rx[i];
        p->n_det = (uint8_t)(p->n_det + 1u);
    }
    p->dustu = p->dustu || a->dustu;
    l->ozel_bek_tf = *tf;
    l->ozel_bek_tf_var = 1u;
    l->ozel_bek_eksik = (uint8_t)(l->ozel_bek_eksik || eksik);
    l->ozel_bek_cakisma = (uint8_t)(l->ozel_bek_cakisma || cakisma);
}

void ust_parti_tf(ida_link_ust_t *l, const mavlink_ida_time_frame_t *m, ida_wtime_t t_rx)
{
    ida_time_frame_t tf;
    const ida_link_ozel_parti_t *a = &l->ozel_acik;
    bool eksik;
    bool cakisma;
    ust_esl_tf(m, t_rx, &tf);
    if (l->ozel_tf_alindi) {
        const uint32_t d = m->batch_seq - l->ozel_son_kapanan;   /* A1-K31, K123 */
        if (d == 0u) {
            l->say[IDA_LINK_SAY_BATCH_DUP] += 1u;
            pencere_at(l);
            return;
        }
        if (d >= UINT32_C(0x80000000)) {
            l->say[IDA_LINK_SAY_BATCH_OLD] += 1u;
            pencere_at(l);
            return;
        }
        if (d >= 2u) {
            l->say[IDA_LINK_SAY_BATCH_SKIPPED] += d - 1u;
        }
    }
    /* A1-K30 */
    eksik = a->n_uye != (uint16_t)m->n_members ||
            ((m->tf_flags & IDAW_TF_FLAGS_IMU) != 0u && !a->imu_var) || a->tasma;
    if (eksik) {
        l->say[IDA_LINK_SAY_BATCH_INCOMPLETE] += 1u;
    }
    l->say[IDA_LINK_SAY_BATCH_CLOSED] += 1u;
    /* A1-K117: TF geldiğinde başka bir çekirdek çağrısı sürüyordu. */
    cakisma = l->ozel_cagri_suruyor ||
              (l->ozel_son_cagri_var && t_rx.ns >= l->ozel_son_cagri_bas.ns &&
               t_rx.ns < l->ozel_son_cagri_bitis.ns);
    l->ozel_tf_alindi = 1u;
    l->ozel_son_kapanan = m->batch_seq;
    if (m->t_sim_ns >= 0 && m->t_sim_ns <= IDA_TIME_MAX_NS) {
        /* SIL oturum sonu çağrısının now_w'si: son kabul edilen geçerli zaman (A4-K08, K11). */
        l->ozel_tf_hic = 1u;
        l->ozel_son_tf_t = m->t_sim_ns;
    }
    l->ozel_son_tf_rx = t_rx;
    /* Izgara: E = son TF + P; TF E + T_tf_gec'e kadar gelmezse tick (A1-K117). */
    l->ozel_izgara_sonraki.ns = t_rx.ns + IDA_LINK_UST_P_NS + IDA_LINK_UST_T_TF_GEC_NS;
    l->ozel_izgara_kurulu = 1u;
    ping_kapanis(l);
    birlestir(l, &tf, eksik, cakisma);
    if (l->ozel_durum != (uint8_t)IDA_LINK_UST_RUNNING) {
        return;                                           /* PROTOCOL ile bitti */
    }
    ust_komut_pencere_kapandi(l);
    ust_olay_pencere_kapandi(l);
    ust_parti_bosalt(&l->ozel_acik);
}

/* ------------------------------------------------------------------ */
/* take                                                               */
/* ------------------------------------------------------------------ */

static bool izgara_vakti(ida_link_ust_t *l, ida_wtime_t simdi)
{
    int64_t k;
    if (!l->ozel_izgara_kurulu) {
        ust_izgara_mutlak(l, simdi);
    }
    if (simdi.ns < l->ozel_izgara_sonraki.ns) {
        return false;
    }
    k = (simdi.ns - l->ozel_izgara_sonraki.ns) / IDA_LINK_UST_IZGARA_NS;
    if (k > 0) {
        l->say[IDA_LINK_SAY_GRID_MISSED] += (uint32_t)(k > INT64_C(0xFFFFFFFF) ? 0xFFFFFFFF : k);
    }
    l->ozel_izgara_sonraki.ns += (k + 1) * IDA_LINK_UST_IZGARA_NS;
    return true;
}

static void plat_doldur(ida_link_ust_t *l, ida_in_t *in, ida_link_cagri_t tur, bool birlesik,
                        bool dustu_var)
{
    ida_plat_t *p = &in->plat;
    p->link_state = (uint8_t)(l->ozel_durum == (uint8_t)IDA_LINK_UST_RUNNING ? IDA_LINK_RUNNING
                                                                             : IDA_LINK_NONE);
    if (tur == IDA_LINK_CAGRI_OTURUM_SONU) {
        p->link_event = (uint8_t)IDA_LINKEV_SESSION_END;
    } else if (l->ozel_baslat_olayi && l->ozel_durum == (uint8_t)IDA_LINK_UST_RUNNING) {
        p->link_event = (uint8_t)IDA_LINKEV_SESSION_START;   /* A1-K16, A4-K72 */
        l->ozel_baslat_olayi = 0u;
    } else {
        p->link_event = (uint8_t)IDA_LINKEV_NONE;
    }
    p->batch_merged = birlesik;
    p->input_dropped = dustu_var;
    p->cnt_batch_skipped = l->say[IDA_LINK_SAY_BATCH_SKIPPED];
    p->cnt_batch_incomplete = l->say[IDA_LINK_SAY_BATCH_INCOMPLETE];
    p->cnt_batch_old = l->say[IDA_LINK_SAY_BATCH_OLD];
    p->cnt_batch_merged = l->say[IDA_LINK_SAY_BATCH_MERGED];
    p->cnt_stale_session = l->say[IDA_LINK_SAY_RX_STALE_SESSION];
    p->cnt_member_overflow = l->say[IDA_LINK_SAY_MEMBER_OVERFLOW];
    p->cnt_cmd_deferred = l->say[IDA_LINK_SAY_CMD_DEFERRED];
    p->cnt_cmd_dropped = l->say[IDA_LINK_SAY_CMD_DROPPED];
    p->cnt_narrow_coalesced = l->say[IDA_LINK_SAY_NARROW_COALESCED];
    p->cnt_grid_missed = l->say[IDA_LINK_SAY_GRID_MISSED];
    p->cnt_rx_error = ust_rx_hata(l);
    p->cnt_out_fixed = l->say[IDA_LINK_SAY_OUT_FIXED];
}

/* Bekleyen girdinin emniyet örneklerini (başta) ve öteki örneklerini (kapasiteye kadar)
   samples[]'e koyar (A1-K124, A4-K102). */
static void ornekleri_koy(ida_link_ust_t *l, ida_in_t *in)
{
    ida_link_ozel_parti_t *p = &l->ozel_bek;
    uint8_t n = 0u;
    for (unsigned i = 0; i < p->n_emn; ++i) {
        in->samples[n] = p->emn[i];
        n = (uint8_t)(n + 1u);
        l->ozel_emn_rev = p->emn_rev[i];   /* A1-K106: verilen son IDA_SAFETY'nin revizyonu */
    }
    for (unsigned i = 0; i < p->n_diger; ++i) {
        if (n >= IDA_PORT_MAX_SAMPLES) {
            dustu(l, p);
            continue;
        }
        in->samples[n] = p->diger[i];
        n = (uint8_t)(n + 1u);
    }
    in->n_samples = n;
}

ida_link_cagri_t ida_link_ust_take(ida_link_ust_t *l, ida_in_t *in)
{
    ida_link_cagri_t tur = IDA_LINK_CAGRI_YOK;
    ida_wtime_t simdi;
    bool birlesik = false;
    bool dustu_var = false;
    if (l == NULL || in == NULL || !l->ozel_kurulu || l->ozel_cagri_suruyor) {
        return IDA_LINK_CAGRI_YOK;
    }
    simdi = ust_simdi(l);
    ust_olay_yokla(l, simdi);   /* sonuç payı dolan olaylar bu çağrıya (A4-K106) */
    if (l->ozel_sonu_bekliyor) {
        tur = IDA_LINK_CAGRI_OTURUM_SONU;
    } else if (l->ozel_bek_tf_var) {
        tur = l->ozel_bek.imu_var ? IDA_LINK_CAGRI_STEP : IDA_LINK_CAGRI_TICK_TF;
    } else if (l->ozel_ayar.rol == IDA_LINK_UST_ROL_KART && izgara_vakti(l, simdi)) {
        tur = IDA_LINK_CAGRI_IZGARA;
    } else {
        return IDA_LINK_CAGRI_YOK;
    }
    memset(in, 0, sizeof *in);
    in->now_w = simdi;
    l->ozel_cagri_birlesen = 0u;
    l->ozel_cagri_eksik = 0u;
    l->ozel_cagri_cakisma = 0u;
    switch (tur) {
    case IDA_LINK_CAGRI_OTURUM_SONU:
        /* A1-K107: tf.present = 0, örneksiz, komutsuz; SIL'de now_w son kabul edilen TF. */
        if (l->ozel_ayar.rol == IDA_LINK_UST_ROL_SIL) {
            in->now_w.ns = l->ozel_tf_hic ? l->ozel_son_tf_t : 0;
        }
        l->ozel_sonu_bekliyor = 0u;
        break;
    case IDA_LINK_CAGRI_IZGARA:
        /* Örneksiz ve komutsuz (A4-K18); tek istisna DUP/OLD penceresinden taşınan emniyet
           örnekleridir: sıradaki çağrıya verilir (A1-K31, K124; Okumalar, EMNİYET). */
        ornekleri_koy(l, in);
        dustu_var = l->ozel_bek.dustu;
        bekleyen_bosalt(l);
        break;
    default:
        in->tf = l->ozel_bek_tf;
        if (l->ozel_ayar.rol == IDA_LINK_UST_ROL_SIL) {
            in->now_w.ns = l->ozel_bek_tf.t.ns;           /* A4-K11 */
        }
        in->imu_present = l->ozel_bek.imu_var;
        in->imu = l->ozel_bek.imu;
        ornekleri_koy(l, in);
        if (l->ozel_bek_ping_var) {
            l->ozel_cagri_ping = l->ozel_bek_ping;
            in->ping360 = &l->ozel_cagri_ping;
        }
        in->n_det_batch = l->ozel_bek.n_det_batch;
        for (unsigned i = 0; i < l->ozel_bek.n_det_batch; ++i) {
            in->det_batch[i] = l->ozel_bek.det_batch[i];
            in->det_batch_t_rx[i] = l->ozel_bek.det_batch_t_rx[i];
        }
        in->n_det = l->ozel_bek.n_det;
        for (unsigned i = 0; i < l->ozel_bek.n_det; ++i) {
            in->det[i] = l->ozel_bek.det[i];
            in->det_t_rx[i] = l->ozel_bek.det_t_rx[i];
        }
        in->n_cmds = ust_komut_cek(l, in->cmds);
        l->ozel_cagri_birlesen = l->ozel_bek_birlesen;
        l->ozel_cagri_eksik = l->ozel_bek_eksik;
        l->ozel_cagri_cakisma = l->ozel_bek_cakisma;
        birlesik = l->ozel_bek_birlesen > 0u;
        dustu_var = l->ozel_bek.dustu;
        bekleyen_bosalt(l);
        break;
    }
    /* Mandallar bu çağrıda verilir ve temizlenir (A4-K46). */
    in->narrow = l->ozel_mandal;
    for (unsigned i = 0; i < IDA_PORT_N_NARROW; ++i) {
        if ((l->ozel_mandal.mask & (1u << i)) != 0u) {
            ust_last_cmd_id(l, l->ozel_mandal.cmd_id[i]);
        }
    }
    memset(&l->ozel_mandal, 0, sizeof l->ozel_mandal);
    in->n_evres = ust_olay_sonuclari_cek(l, in->evres);
    plat_doldur(l, in, tur, birlesik, dustu_var);

    l->ozel_cagri_suruyor = 1u;
    l->ozel_cagri_tur = (uint8_t)tur;
    l->ozel_cagri_bas = simdi;
    l->ozel_cagri_kusak = l->ozel_kusak;
    l->ozel_cagri_sess = l->ozel_sess;
    return tur;
}
