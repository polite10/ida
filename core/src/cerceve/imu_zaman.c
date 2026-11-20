/*
 * imu_zaman.c — IMU boşluğu, gecikme mandalı ve bayatlık (W2; A4-K37, K38, K39, K128).
 */
#include "cerceve/imu_zaman.h"

#include "zaman/zaman.h"

void ida_imz_kur(ida_imz_t *z)
{
    z->var = false;
    z->t_meas_ns = 0;
    z->t_rx_ns = 0;
    z->lag = false;
    z->temiz = 0u;
    z->bayat_izi = true; /* ilk kabulden önce bayat (A4-K128); günlük taban durumu */
}

void ida_imz_imu_kabul(ida_imz_t *z, const ida_imu_sample_t *imu, const ida_imz_baglam_t *b)
{
    if (z->var) {
        /* boşluk = (t_meas − window) − t_meas(önceki); sınır = oran × window (A4-K38) */
        const int64_t bosluk =
            (imu->hdr.t_meas.ns - (int64_t)imu->hdr.window_ns) - z->t_meas_ns;
        const float oran = ida_prm_f32(b->prm, IDA_PRM_imu_gap_oran);
        const int64_t sinir = (int64_t)(oran * (float)imu->hdr.window_ns);
        if (ida_mutlak_i64(bosluk) > sinir) {
            b->cnt->imu_gap++;
            const ida_log_t k = {.t_w = b->now,
                                 .code = (uint16_t)IDA_LOG_IMU_GAP,
                                 .arg0 = ida_ns_us_doyur(ida_mutlak_i64(bosluk)),
                                 .arg1 = 0};
            ida_gun_ekle(b->gun, &k);
        }
    }
    z->var = true;
    z->t_meas_ns = imu->hdr.t_meas.ns;
    z->t_rx_ns = imu->hdr.t_rx.ns;
}

void ida_imz_lag_guncelle(ida_imz_t *z, const ida_time_frame_t *tf, const ida_imz_baglam_t *b)
{
    if (!tf->present || !z->var) {
        return;
    }
    const int64_t lag = tf->t.ns - z->t_meas_ns;
    const int64_t kur = (int64_t)ida_prm_u32(b->prm, IDA_PRM_imu_lag_set_ns);
    const int64_t coz = (int64_t)ida_prm_u32(b->prm, IDA_PRM_imu_lag_clr_ns);
    const uint32_t n_coz = ida_prm_u32(b->prm, IDA_PRM_imu_lag_clr_n);
    if (!z->lag) {
        if (lag > kur) {
            z->lag = true;
            z->temiz = 0u;
            const ida_log_t k = {.t_w = b->now,
                                 .code = (uint16_t)IDA_LOG_IMU_LAG,
                                 .arg0 = 1,
                                 .arg1 = ida_ns_us_doyur(lag)};
            ida_gun_ekle(b->gun, &k);
        }
        return;
    }
    if (lag <= coz) {
        z->temiz++;
        if (z->temiz >= n_coz) {
            z->lag = false;
            z->temiz = 0u;
            const ida_log_t k = {.t_w = b->now,
                                 .code = (uint16_t)IDA_LOG_IMU_LAG,
                                 .arg0 = 0,
                                 .arg1 = ida_ns_us_doyur(lag)};
            ida_gun_ekle(b->gun, &k);
        }
    } else {
        z->temiz = 0u;
    }
}

bool ida_imz_bayat(const ida_imz_t *z, ida_wtime_t now, uint32_t esik_ns)
{
    if (!z->var) {
        return true;
    }
    return (now.ns - z->t_rx_ns) > (int64_t)esik_ns;
}

void ida_imz_bayat_izle(ida_imz_t *z, bool bayat, const ida_imz_baglam_t *b)
{
    if (bayat != z->bayat_izi) {
        const ida_log_t k = {.t_w = b->now,
                             .code = (uint16_t)IDA_LOG_IMU_STALE,
                             .arg0 = bayat ? 1 : 0,
                             .arg1 = 0};
        ida_gun_ekle(b->gun, &k);
        z->bayat_izi = bayat;
    }
}
