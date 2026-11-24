/*
 * kestirim.c — kestirim kancaları, hizalama, sağlık düzeyi ve nav (W3 kestirim dilim 1; W5
 * kestirim-yatay dilim 2: DVL, GNSS ve Ping1D örnekleri, yatay geçerlilik, düzey histerezisi,
 * irtifa). Yöntem ve sınırlar kestirim.h başındadır.
 */
#include "kestirim/kestirim.h"

#include <string.h>

#include "cekirdek.h"
#include "kestirim/kes_ic.h"
#include "mat/ida_math.h"
#include "zaman/zaman.h"

#define N IDA_KES_N

/* ---- Parametre görüntüsü --------------------------------------------------------------- */

static ida_m3_t montaj(const ida_param_t *p, enum ida_prm_indis r, enum ida_prm_indis pi,
                       enum ida_prm_indis y)
{
    return ida_q_dcm(ida_kes_q_euler(ida_prm_f32(p, r), ida_prm_f32(p, pi), ida_prm_f32(p, y)));
}

void ida_kes_ayar_oku(const ida_core_t *c, ida_kes_ayar_t *a)
{
    const ida_param_t *p = &c->prm;
    a->now_ns = c->cer.now_izi_ns;
    a->tref_var = c->cer.tf_var;
    a->tref_ns = c->cer.tf_t_ns;
    a->d_ns = ida_ms_ns(ida_prm_u32(p, IDA_PRM_kes_d_ms));
    a->tau_o = ida_prm_f32(p, IDA_PRM_kes_tau_o);
    a->hiz_s = ida_prm_f32(p, IDA_PRM_kes_hiz_s);
    a->hiz_w_max = ida_prm_f32(p, IDA_PRM_hiz_w_max);
    a->hiz_a_max = ida_prm_f32(p, IDA_PRM_hiz_a_max);
    a->l_imu.x = ida_prm_f32(p, IDA_PRM_imu_l_x);
    a->l_imu.y = ida_prm_f32(p, IDA_PRM_imu_l_y);
    a->l_imu.z = ida_prm_f32(p, IDA_PRM_imu_l_z);
    a->m_imu = montaj(p, IDA_PRM_imu_roll, IDA_PRM_imu_pitch, IDA_PRM_imu_yaw);
    a->m_mag = montaj(p, IDA_PRM_mag_roll, IDA_PRM_mag_pitch, IDA_PRM_mag_yaw);
    a->l_baro.x = ida_prm_f32(p, IDA_PRM_baro_l_x);
    a->l_baro.y = ida_prm_f32(p, IDA_PRM_baro_l_y);
    a->l_baro.z = ida_prm_f32(p, IDA_PRM_baro_l_z);
    a->p_atm_prm = ida_prm_f32(p, IDA_PRM_baro_p_atm);
    a->baro_kal = ida_prm_u32(p, IDA_PRM_baro_kal) != 0u;
    a->baro_kal_d = ida_prm_f32(p, IDA_PRM_baro_kal_d);
    a->rho = ida_prm_f32(p, IDA_PRM_su_rho);
    a->g = ida_prm_f32(p, IDA_PRM_yer_g);
    a->mag_dec = ida_prm_f32(p, IDA_PRM_mag_dec);
    a->mag_b = ida_prm_f32(p, IDA_PRM_mag_b);
    a->mag_b_tol = ida_prm_f32(p, IDA_PRM_mag_b_tol);
    a->q_gyro = ida_prm_f32(p, IDA_PRM_q_gyro);
    a->q_acc = ida_prm_f32(p, IDA_PRM_q_acc);
    a->q_bg = ida_prm_f32(p, IDA_PRM_q_bg);
    a->q_ba = ida_prm_f32(p, IDA_PRM_q_ba);
    a->r_baro = ida_prm_f32(p, IDA_PRM_r_baro);
    a->r_mag = ida_prm_f32(p, IDA_PRM_r_mag);
    a->r_grav = ida_prm_f32(p, IDA_PRM_r_grav);
    a->grav_esik = ida_prm_f32(p, IDA_PRM_grav_esik);
    a->kapi_g1 = ida_prm_f32(p, IDA_PRM_kapi_g1);
    a->kapi_g3 = ida_prm_f32(p, IDA_PRM_kapi_g3);
    a->p0_tilt = ida_prm_f32(p, IDA_PRM_p0_tilt);
    a->p0_yaw = ida_prm_f32(p, IDA_PRM_p0_yaw);
    a->p0_bg = ida_prm_f32(p, IDA_PRM_p0_bg);
    a->p0_ba = ida_prm_f32(p, IDA_PRM_p0_ba);
    a->p0_v = ida_prm_f32(p, IDA_PRM_p0_v);
    a->p0_p = ida_prm_f32(p, IDA_PRM_p0_p);
    a->sig_tilt = ida_prm_f32(p, IDA_PRM_sig_tilt);
    a->sig_yaw = ida_prm_f32(p, IDA_PRM_sig_yaw);
    a->sig_depth = ida_prm_f32(p, IDA_PRM_sig_depth);
    a->sig_hpos = ida_prm_f32(p, IDA_PRM_sig_hpos);
    a->red_ns = ida_ms_ns(ida_prm_u32(p, IDA_PRM_kes_red_ms));
    a->mag_reset_ns = ida_ms_ns(ida_prm_u32(p, IDA_PRM_mag_reset_ms));
    a->baro_reset_ns = ida_ms_ns(ida_prm_u32(p, IDA_PRM_baro_reset_ms));
    a->baro_stale_ns = ida_ms_ns(ida_prm_u32(p, IDA_PRM_t_baro_stale));
    a->mag_stale_ns = ida_ms_ns(ida_prm_u32(p, IDA_PRM_t_mag_stale));
    a->imu_gap_ns = ida_ms_ns(ida_prm_u32(p, IDA_PRM_imu_gap_max));
    a->l_gnss.x = ida_prm_f32(p, IDA_PRM_gnss_l_x);
    a->l_gnss.y = ida_prm_f32(p, IDA_PRM_gnss_l_y);
    a->l_gnss.z = ida_prm_f32(p, IDA_PRM_gnss_l_z);
    a->l_dvl.x = ida_prm_f32(p, IDA_PRM_dvl_l_x);
    a->l_dvl.y = ida_prm_f32(p, IDA_PRM_dvl_l_y);
    a->l_dvl.z = ida_prm_f32(p, IDA_PRM_dvl_l_z);
    a->e_dvl.x = ida_prm_f32(p, IDA_PRM_dvl_roll);
    a->e_dvl.y = ida_prm_f32(p, IDA_PRM_dvl_pitch);
    a->e_dvl.z = ida_prm_f32(p, IDA_PRM_dvl_yaw);
    a->l_ping.x = ida_prm_f32(p, IDA_PRM_ping_l_x);
    a->l_ping.y = ida_prm_f32(p, IDA_PRM_ping_l_y);
    a->l_ping.z = ida_prm_f32(p, IDA_PRM_ping_l_z);
    a->orj_lat_e7 = ida_prm_i32(p, IDA_PRM_orijin_lat);
    a->orj_lon_e7 = ida_prm_i32(p, IDA_PRM_orijin_lon);
    a->r_dvl = ida_prm_f32(p, IDA_PRM_r_dvl);
    a->dvl_fom_max = ida_prm_f32(p, IDA_PRM_dvl_fom_max);
    a->dvl_alt_min = ida_prm_f32(p, IDA_PRM_dvl_alt_min);
    a->r_gnss_p = ida_prm_f32(p, IDA_PRM_r_gnss_p);
    a->gnss_sig_k = ida_prm_f32(p, IDA_PRM_gnss_sig_k);
    a->r_gnss_v = ida_prm_f32(p, IDA_PRM_r_gnss_v);
    a->gnss_eph_max = ida_prm_f32(p, IDA_PRM_gnss_eph_max);
    a->kapi_g2 = ida_prm_f32(p, IDA_PRM_kapi_g2);
    a->yuzey_d = ida_prm_f32(p, IDA_PRM_kes_yuzey_d);
    a->dr_oran = ida_prm_f32(p, IDA_PRM_dr_oran);
    a->sig_hvel = ida_prm_f32(p, IDA_PRM_sig_hvel);
    a->konum_sicra = ida_prm_f32(p, IDA_PRM_konum_sicra);
    a->n_iyi = ida_prm_u32(p, IDA_PRM_kes_n_iyi);
    a->n_reset = ida_prm_u32(p, IDA_PRM_kes_n_reset);
    a->ok_ns = ida_ms_ns(ida_prm_u32(p, IDA_PRM_kes_ok_ms));
    a->dvl_stale_ns = ida_ms_ns(ida_prm_u32(p, IDA_PRM_t_dvl_stale));
    a->gnss_stale_ns = ida_ms_ns(ida_prm_u32(p, IDA_PRM_t_gnss_stale));
}

/* ---- Kurulum ve yeniden başlama -------------------------------------------------------- */

void ida_kes_kur(ida_kes_t *k)
{
    (void)memset(k, 0, sizeof(*k));
    k->q_o.w = 1.0f;
    k->xh.q.w = 1.0f;
    k->duzey = (uint8_t)IDA_EST_INIT;
    k->suzgec_duzey = (uint8_t)IDA_EST_INIT;
    k->irtifa = -1.0f;
}

/* Yatay hız ve konumun bütün desteğini ve nitelendirmesini düşürür (yeniden hizalama). Yatay
   konum hizalamada bir kabuldür: yeniden GNSS'ten kurulana kadar geçersizdir. Akışların alım
   izleri (canlılık), seyreltme izleri ve irtifa kaynakları korunur. */
static void yatay_birak(ida_kes_t *k)
{
    k->yolc_n = 0u;
    k->konum_var = false;
    (void)memset(&k->nit_dvl, 0, sizeof(k->nit_dvl));
    (void)memset(&k->nit_gnss, 0, sizeof(k->nit_gnss));
    (void)memset(&k->nit_gnss_v, 0, sizeof(k->nit_gnss_v));
    k->akis_dvl.destek_var = false;
    k->akis_gnss.destek_var = false;
    k->gnss_hiz_destek_var = false;
    k->red_dvl.var = false;
    k->red_gnss_p.var = false;
    k->red_gnss_v.var = false;
    k->hiz_fuz_var = false;
    k->ok_aday_var = false;
}

/* Filtreyi INIT'e döndürür (IMU boşluğu, sayısal çökme). Basınç referansı, çökme mandalı,
   tazelik izleri ve sayaçlar korunur. */
static void yeniden_hizala(ida_kes_t *k)
{
    k->hizali = false;
    k->yon_var = false;
    (void)memset(&k->hz, 0, sizeof(k->hz));
    k->imu_bas = 0u;
    k->imu_n = 0u;
    k->olc_n = 0u;
    k->red_grav.var = false;
    k->red_baro.var = false;
    k->red_mag.var = false;
    k->akis_mag.destek_var = false;
    k->akis_baro.destek_var = false;
    k->grav_destek_var = false;
    k->cikis_gec = false;
    k->gecerlilik = 0u;
    k->duzey = k->ariza ? (uint8_t)IDA_EST_FAULT : (uint8_t)IDA_EST_INIT;
    k->suzgec_duzey = (uint8_t)IDA_EST_INIT;
    yatay_birak(k);
}

void ida_kes_oturum_basi(ida_kes_t *k)
{
    /* A4-K72: ölçüm zamanı oturumlar arasında sürekli sayılmaz; filtre baştan hizalanır.
       Korunanlar: basınç referansı (mimar kararı: atmosfer kalibrasyonu yalnız init'ten
       sonraki ilk hizalamada; oturum yenilenmesi aracın yüzeyde olduğunu göstermez), sayısal
       çökme mandalı (kısıtlayan okuma), konum sıfırlama sayacı ve bilgi sayaçları. Yatay konum
       korunmaz: oturumlar arasında geçen süre ve yol bilinmez; konum yeniden GNSS'ten kurulana
       kadar geçersizdir. */
    /* Korunan alanlar yerel değişkenlerde (yapının kopyası yığını büyütürdü). */
    const bool ariza = k->ariza;
    const bool kalibre = k->kalibre;
    const float p_atm = k->p_atm;
    const uint8_t reset_sayac = k->reset_sayac;
    const uint32_t sayac[14] = {k->n_gec,       k->n_tasma,      k->n_cokme,     k->n_hizalama,
                                k->n_yon_sifir, k->n_derinlik_sifir, k->n_inst,  k->n_geri,
                                k->n_bayrak,    k->n_seyrek,     k->n_dvl_red,   k->n_gnss_red,
                                k->n_hiz_sifir, k->n_konum_sifir};
    ida_kes_kur(k);
    k->ariza = ariza;
    k->kalibre = kalibre;
    k->p_atm = p_atm;
    k->reset_sayac = reset_sayac;
    k->n_gec = sayac[0];
    k->n_tasma = sayac[1];
    k->n_cokme = sayac[2];
    k->n_hizalama = sayac[3];
    k->n_yon_sifir = sayac[4];
    k->n_derinlik_sifir = sayac[5];
    k->n_inst = sayac[6];
    k->n_geri = sayac[7];
    k->n_bayrak = sayac[8];
    k->n_seyrek = sayac[9];
    k->n_dvl_red = sayac[10];
    k->n_gnss_red = sayac[11];
    k->n_hiz_sifir = sayac[12];
    k->n_konum_sifir = sayac[13];
    k->duzey = k->ariza ? (uint8_t)IDA_EST_FAULT : (uint8_t)IDA_EST_INIT;
    k->suzgec_duzey = (uint8_t)IDA_EST_INIT;
}

/* ---- Örnekler -------------------------------------------------------------------------- */

void ida_kes_imu(ida_kes_t *k, const ida_imu_sample_t *imu)
{
    if (imu->hdr.inst != 0u) {
        k->n_inst++; /* bu dilimde yalnız inst = 0 (mimar kararı; sol bulgu 2) */
        return;
    }
    k->imu_bekliyor = true;
    k->imu_t_ns = imu->hdr.t_meas.ns;
    k->imu_win_ns = imu->hdr.window_ns;
    k->imu_bayrak = imu->hdr.flags;
    k->imu_w.x = imu->imu.gyro[0];
    k->imu_w.y = imu->imu.gyro[1];
    k->imu_w.z = imu->imu.gyro[2];
    k->imu_f.x = imu->imu.accel[0];
    k->imu_f.y = imu->imu.accel[1];
    k->imu_f.z = imu->imu.accel[2];
}

/* Akışın izi: son kullanılabilir örneğin alımı ve 1 s pencerede ölçüm zamanı ilerlemesi
   (A4-K36 (b); 02 §4.1: oran < 0,5 ise akış durmuş). */
static void akis_guncelle(ida_kes_akis_t *a, int64_t tm_ns, int64_t rx_ns)
{
    a->var = true;
    a->rx_ns = rx_ns;
    if (!a->pen_var) {
        a->pen_var = true;
        a->pen_rx_ns = rx_ns;
        a->pen_tm_ns = tm_ns;
        return;
    }
    const int64_t drx = rx_ns - a->pen_rx_ns;
    if (drx >= 1000000000) {
        const int64_t dtm = tm_ns - a->pen_tm_ns;
        a->durmus = (2 * dtm) < drx;
        a->pen_rx_ns = rx_ns;
        a->pen_tm_ns = tm_ns;
    }
}

/* Seyreltme (kes_ic.h): kuyruğa alınan son örnekten bu yana ölçüm zamanı en kısa aralık kadar
   ilerlemediyse örnek kuyruğa alınmaz. true: alınır (iz güncellenir). */
static bool seyrelt(bool *var, int64_t *son_ns, int64_t tm_ns, int64_t aralik_ns)
{
    if (*var && ((tm_ns - *son_ns) < aralik_ns)) {
        return false;
    }
    *var = true;
    *son_ns = tm_ns;
    return true;
}

static void dvl_ornegi(ida_kes_t *k, const ida_sample_t *s, bool bayrakli)
{
    const int64_t tm = s->hdr.t_meas.ns;
    if (!bayrakli) {
        akis_guncelle(&k->akis_dvl, tm, s->hdr.t_rx.ns);
    }
    /* İrtifa kaynağı her örnekte yenilenir: bilinmeyen (−1) ya da bayraklı örnek kaynağı siler. */
    k->irt_dvl_var = !bayrakli && (s->u.dvl.altitude >= 0.0f);
    k->irt_dvl = k->irt_dvl_var ? s->u.dvl.altitude : 0.0f;
    if (!seyrelt(&k->dvl_kuy_var, &k->dvl_kuy_ns, tm, (int64_t)IDA_KES_DVL_ARALIK_NS)) {
        k->n_seyrek++;
        return;
    }
    ida_kes_yolc_t y;
    (void)memset(&y, 0, sizeof(y));
    y.t_ns = tm;
    y.tur = (uint8_t)IDA_SENS_DVL;
    y.sira = 1u;
    uint32_t b = bayrakli ? IDA_KES_YB_KULLANILMAZ : 0u;
    b |= ((s->u.dvl.dvl_flags & IDA_DVL_VEL_VALID) != 0u) ? IDA_KES_YB_HIZ : 0u;
    b |= ((s->u.dvl.dvl_flags & IDA_DVL_TRACK_BOTTOM) != 0u) ? IDA_KES_YB_DIP : 0u;
    b |= ((s->u.dvl.dvl_flags & IDA_DVL_COV_PRESENT) != 0u) ? IDA_KES_YB_KOV : 0u;
    y.bayrak = (uint8_t)b;
    for (uint32_t i = 0u; i < 3u; i++) {
        y.v[i] = s->u.dvl.vel[i];
    }
    for (uint32_t i = 0u; i < 6u; i++) {
        y.v[3u + i] = s->u.dvl.cov[i];
    }
    y.v[9] = s->u.dvl.fom;
    y.v[10] = s->u.dvl.altitude;
    ida_kes_yolc_ekle(k, &y);
}

static void gnss_ornegi(ida_kes_t *k, const ida_sample_t *s, bool bayrakli)
{
    const int64_t tm = s->hdr.t_meas.ns;
    if (!bayrakli) {
        akis_guncelle(&k->akis_gnss, tm, s->hdr.t_rx.ns);
    }
    if (!seyrelt(&k->gnss_kuy_var, &k->gnss_kuy_ns, tm, (int64_t)IDA_KES_GNSS_ARALIK_NS)) {
        k->n_seyrek++;
        return;
    }
    ida_kes_yolc_t y;
    (void)memset(&y, 0, sizeof(y));
    y.t_ns = tm;
    y.tur = (uint8_t)IDA_SENS_GNSS;
    y.sira = 3u;
    y.bayrak = bayrakli ? (uint8_t)IDA_KES_YB_KULLANILMAZ : (uint8_t)0u;
    y.fix = s->u.gnss.fix_type;
    y.lat_e7 = s->u.gnss.lat_e7;
    y.lon_e7 = s->u.gnss.lon_e7;
    y.v[0] = s->u.gnss.vel_ned[0];
    y.v[1] = s->u.gnss.vel_ned[1];
    y.v[2] = s->u.gnss.eph;
    ida_kes_yolc_ekle(k, &y);
}

static void ping_ornegi(ida_kes_t *k, const ida_sample_t *s, bool bayrakli)
{
    /* Ping1D yalnız irtifa içindir; filtreye girmez (02 §2.3.3). */
    if (!bayrakli) {
        akis_guncelle(&k->akis_ping, s->hdr.t_meas.ns, s->hdr.t_rx.ns);
    }
    k->irt_ping_var = !bayrakli && (s->u.ping1d.distance >= 0.0f) &&
                      ((uint32_t)s->u.ping1d.confidence >= IDA_KES_PING_GUVEN_EN_AZ);
    k->irt_ping = k->irt_ping_var ? s->u.ping1d.distance : 0.0f;
}

void ida_kes_ornek(ida_kes_t *k, const ida_sample_t *s)
{
    /* Kapıdan geçmiş örnek. Her türün yalnız inst = 0 örneği kullanılır; SATURATED ya da
       SENSOR_FAULT bayraklı değer kullanılmaz. İkisi de akışı canlı saymaz. Bayraklı DVL ve
       GNSS örneği yine de kuyruğa "kullanılmaz" olarak girer: nitelendirmenin ardışık iyi
       örnek sayımı onu zaman sırasında görür. */
    const bool bayrakli =
        (s->hdr.flags & (uint16_t)(IDA_HF_SATURATED | IDA_HF_SENSOR_FAULT)) != 0u;
    const bool yardim = (s->hdr.type == (uint8_t)IDA_SENS_DVL) ||
                        (s->hdr.type == (uint8_t)IDA_SENS_GNSS) ||
                        (s->hdr.type == (uint8_t)IDA_SENS_PING1D);
    if (bayrakli && !(yardim && (s->hdr.inst == 0u))) {
        return;
    }
    if (s->hdr.inst != 0u) {
        if ((s->hdr.type == (uint8_t)IDA_SENS_MAG) || (s->hdr.type == (uint8_t)IDA_SENS_BARO) ||
            yardim) {
            k->n_inst++;
        }
        return;
    }
    ida_kes_olc_t o;
    (void)memset(&o, 0, sizeof(o));
    switch (s->hdr.type) {
    case IDA_SENS_MAG:
        o.t_ns = s->hdr.t_meas.ns;
        o.tur = (uint8_t)IDA_SENS_MAG;
        o.sira = 2u;
        o.v[0] = s->u.mag.field[0];
        o.v[1] = s->u.mag.field[1];
        o.v[2] = s->u.mag.field[2];
        akis_guncelle(&k->akis_mag, s->hdr.t_meas.ns, s->hdr.t_rx.ns);
        if (!seyrelt(&k->mag_kuy_var, &k->mag_kuy_ns, s->hdr.t_meas.ns,
                     (int64_t)IDA_KES_MAG_ARALIK_NS)) {
            k->n_seyrek++;
            break;
        }
        ida_kes_olc_ekle(k, &o);
        break;
    case IDA_SENS_BARO:
        /* Aralık ölçümü: tek geçerlilik anı aralığın ortası (iskelet §3.1). */
        o.t_ns = s->hdr.t_meas.ns - (int64_t)(s->hdr.window_ns / 2u);
        o.tur = (uint8_t)IDA_SENS_BARO;
        o.sira = 0u;
        o.v[0] = s->u.baro.p_abs;
        akis_guncelle(&k->akis_baro, s->hdr.t_meas.ns, s->hdr.t_rx.ns);
        if (!seyrelt(&k->baro_kuy_var, &k->baro_kuy_ns, s->hdr.t_meas.ns,
                     (int64_t)IDA_KES_BARO_ARALIK_NS)) {
            k->n_seyrek++;
            break;
        }
        ida_kes_olc_ekle(k, &o);
        break;
    case IDA_SENS_DVL:
        dvl_ornegi(k, s, bayrakli);
        break;
    case IDA_SENS_GNSS:
        gnss_ornegi(k, s, bayrakli);
        break;
    case IDA_SENS_PING1D:
        ping_ornegi(k, s, bayrakli);
        break;
    default:
        break;
    }
}

/* ---- Hizalama (02 §2.3.1) -------------------------------------------------------------- */

static void hizalama_imu(ida_kes_t *k, const ida_kes_ayar_t *a, int64_t t_ns, uint32_t win_ns,
                         ida_v3_t w, ida_v3_t f)
{
    ida_kes_hiz_t *h = &k->hz;
    if ((h->n > 0u) && ((t_ns - h->t_son_ns) > a->imu_gap_ns)) {
        (void)memset(h, 0, sizeof(*h));
    }
    const float sapma = ida_v3_norm(f) - a->g;
    const bool duragan = (ida_v3_norm(w) <= a->hiz_w_max) && (sapma <= a->hiz_a_max) &&
                         (-sapma <= a->hiz_a_max);
    if (!duragan) {
        (void)memset(h, 0, sizeof(*h)); /* hareket: birikim baştan (02 §2.3.1 "durağan") */
        return;
    }
    if (h->n == 0u) {
        h->t0_ns = t_ns - (int64_t)win_ns;
    }
    h->n++;
    h->t_son_ns = t_ns;
    /* Yürüyen ortalama: toplam tutulmaz, taşma olmaz (sol bulgu 9). */
    const float kz = 1.0f / (float)h->n;
    h->w_ort = ida_v3_topla(h->w_ort, ida_v3_olcekle(ida_v3_cikar(w, h->w_ort), kz));
    h->f_ort = ida_v3_topla(h->f_ort, ida_v3_olcekle(ida_v3_cikar(f, h->f_ort), kz));
}

static void hizalama_olcumler(ida_kes_t *k, const ida_kes_ayar_t *a)
{
    ida_kes_hiz_t *h = &k->hz;
    for (uint32_t i = 0u; i < k->olc_n; i++) {
        const ida_kes_olc_t *o = &k->olc[i];
        if (h->n == 0u) {
            continue; /* birikim başlamadan gelen ölçüm durağanlık penceresine ait değil */
        }
        if (o->tur == (uint8_t)IDA_SENS_MAG) {
            const ida_v3_t ms = {o->v[0], o->v[1], o->v[2]};
            const ida_v3_t mb = ida_m3_v3(a->m_mag, ms);
            const float sapma = ida_v3_norm(mb) - a->mag_b;
            if ((sapma <= a->mag_b_tol * a->mag_b) && (-sapma <= a->mag_b_tol * a->mag_b)) {
                h->m_n++;
                const float kz = 1.0f / (float)h->m_n;
                h->m_ort = ida_v3_topla(h->m_ort, ida_v3_olcekle(ida_v3_cikar(mb, h->m_ort), kz));
            }
        } else {
            const float p = o->v[0];
            if ((p >= IDA_KES_BARO_EN_AZ) && (p <= IDA_KES_BARO_EN_COK)) {
                h->b_n++;
                h->b_ort += (p - h->b_ort) / (float)h->b_n;
            }
        }
    }
    k->olc_n = 0u;
}

static void hizalama_bitir(ida_kes_t *k, const ida_kes_ayar_t *a)
{
    const ida_kes_hiz_t *h = &k->hz;
    const ida_v3_t fo = h->f_ort;
    const ida_v3_t wo = h->w_ort;
    const float roll = ida_atan2(-fo.y, -fo.z);
    const float pitch = ida_atan2(fo.x, ida_karekok(fo.y * fo.y + fo.z * fo.z));
    float yaw = 0.0f;
    bool yon = false;
    if (h->m_n > 0u) {
        const ida_v3_t mo = h->m_ort;
        const ida_v3_t mt = ida_m3_v3(ida_q_dcm(ida_kes_q_euler(roll, pitch, 0.0f)), mo);
        yaw = ida_aci_sar(a->mag_dec - ida_atan2(mt.y, mt.x));
        yon = true;
    }
    const ida_q_t q0 = ida_kes_q_euler(roll, pitch, yaw);
    const ida_v3_t rl = ida_m3_v3(ida_q_dcm(q0), a->l_baro);
    const float rg = a->rho * a->g;
    float d0 = a->baro_kal_d;
    float dvar = a->p0_p * a->p0_p;
    const bool baro = h->b_n > 0u;
    const float p_ort = h->b_ort;
    if (!k->kalibre) {
        if (!a->baro_kal) {
            k->p_atm = a->p_atm_prm;
            k->kalibre = true;
        } else if (baro) {
            k->p_atm = p_ort - rg * (a->baro_kal_d + rl.z);
            k->kalibre = true;
            dvar = a->r_baro * a->r_baro;
        } else {
            /* ilk basınç örneğinde kurulur (kes_ufuk.c baro_fuzyon) */
        }
    } else {
        d0 = 0.0f;
    }
    if (k->kalibre && baro) {
        d0 = (p_ort - k->p_atm) / rg - rl.z;
        dvar = a->r_baro * a->r_baro;
    }
    ida_kes_nominal_t x;
    const ida_v3_t sifir = {0.0f, 0.0f, 0.0f};
    x.p = sifir;
    x.p.z = d0;
    x.v = sifir;
    x.q = q0;
    x.bg = wo;
    x.ba = sifir;
    ida_kes_ufuk_kur(k, &x, h->t_son_ns);
    for (uint32_t i = 0u; i < N; i++) {
        for (uint32_t j = 0u; j < N; j++) {
            k->P[i][j] = 0.0f;
        }
    }
    const float pp = a->p0_p * a->p0_p;
    const float yv = yon ? (a->p0_yaw * a->p0_yaw) : (IDA_PI_F * IDA_PI_F);
    const float diag[N] = {pp,
                           pp,
                           dvar,
                           a->p0_v * a->p0_v,
                           a->p0_v * a->p0_v,
                           a->p0_v * a->p0_v,
                           a->p0_tilt * a->p0_tilt,
                           a->p0_tilt * a->p0_tilt,
                           yv,
                           a->p0_bg * a->p0_bg,
                           a->p0_bg * a->p0_bg,
                           a->p0_bg * a->p0_bg,
                           a->p0_ba * a->p0_ba,
                           a->p0_ba * a->p0_ba,
                           a->p0_ba * a->p0_ba};
    for (uint32_t i = 0u; i < N; i++) {
        k->P[i][i] = diag[i];
    }
    k->hizali = true;
    k->yon_var = yon;
    k->d_ns = a->d_ns;
    k->red_grav.var = false;
    k->red_baro.var = false;
    k->red_mag.var = false;
    k->n_hizalama++;
    (void)memset(&k->hz, 0, sizeof(k->hz));
}

/* ---- Adım ------------------------------------------------------------------------------- */

static void imu_isle(ida_kes_t *k, const ida_kes_ayar_t *a)
{
    if (!k->imu_bekliyor) {
        return;
    }
    k->imu_bekliyor = false;
    const int64_t t = k->imu_t_ns;
    if (k->imu_son_var && (t <= k->imu_son_ns)) {
        k->n_geri++; /* ortak zaman çizgisinde geri giden örnek işlenmez (bulgu 2) */
        return;
    }
    if ((k->imu_bayrak & (uint16_t)(IDA_HF_SATURATED | IDA_HF_SENSOR_FAULT)) != 0u) {
        /* Bayraklı IMU (sol bulgu 3): hizalamaya ve yayılıma girmez. Hizalama birikimi baştan
           başlar; hizalıyken aralık bir sonraki kullanılabilir örneğe kalır (boşluk kuralı). */
        k->n_bayrak++;
        k->imu_bayrakli = true;
        if (!k->hizali) {
            (void)memset(&k->hz, 0, sizeof(k->hz));
        }
        return;
    }
    k->imu_bayrakli = false;
    const ida_v3_t w = ida_m3_v3(a->m_imu, k->imu_w);
    const ida_v3_t fb = ida_m3_v3(a->m_imu, k->imu_f);
    /* Dönme kolu (A4-K42): f_O = f_imu − ω × (ω × l); açısal ivme terimi α × l ihmal edilir
       (jiroskop farkından türetmek gürültüyü büyütür; etkisi R_GRAV içinde). */
    /* Ham ω kullanılır: sapmanın katkısı |b_g|²·|l| mertebesindedir ve ufkun o anki
       kestirimine bağlanmaması kaydı D'den bağımsız kılar. */
    const ida_v3_t f = ida_v3_cikar(fb, ida_v3_dis(w, ida_v3_dis(w, a->l_imu)));
    if (k->hizali) {
        const int64_t dt_ns = k->imu_son_var ? (t - k->imu_son_ns) : (int64_t)k->imu_win_ns;
        if (dt_ns > a->imu_gap_ns) {
            /* A4-K38: boşluktaki filtre davranışı. IMU_GAP_MAX'ı aşan boşlukta yayılım
               güvenilmez; filtre yeniden hizalanır. Daha kısa boşluk (ve örtüşme) kaydın
               etkin aralığına girer: ölçüm boşluk boyunca sabit sayılır. */
            yeniden_hizala(k);
            hizalama_imu(k, a, t, k->imu_win_ns, w, f);
        } else {
            ida_kes_ufuk_imu(k, t, ida_kes_ns_s(dt_ns), w, f, a);
        }
    } else {
        hizalama_imu(k, a, t, k->imu_win_ns, w, f);
    }
    k->imu_son_var = true;
    k->imu_son_ns = t;
}

static bool cikis_sonlu(const ida_kes_t *k)
{
    const float d[25] = {k->q_o.w,   k->q_o.x,   k->q_o.y,   k->q_o.z,    k->v_o.x,
                         k->v_o.y,   k->v_o.z,   k->p_o.x,   k->p_o.y,    k->p_o.z,
                         k->w_o.x,   k->w_o.y,   k->w_o.z,   k->sig_p.x,  k->sig_p.y,
                         k->sig_p.z, k->sig_v.x, k->sig_v.y, k->sig_v.z,  k->sig_th.x,
                         k->sig_th.y, k->sig_th.z, k->sig_bg.x, k->sig_bg.y, k->sig_bg.z};
    for (uint32_t i = 0u; i < 25u; i++) {
        if (!ida_sonlu(d[i])) {
            return false;
        }
    }
    return true;
}

/* Akış canlı: son kullanılabilir örneğin alım yaşı sınır içinde ve ölçüm zamanı ilerliyor. */
static bool canli(const ida_kes_akis_t *s, int64_t now_ns, int64_t sinir_ns)
{
    return s->var && ((now_ns - s->rx_ns) <= sinir_ns) && !s->durmus;
}

/* Ölçüm desteği taze: son füzyonlanan ölçüm sınır içinde. */
static bool destekli(bool var, int64_t destek_ns, int64_t now_ns, int64_t sinir_ns)
{
    return var && ((now_ns - destek_ns) <= sinir_ns);
}

static bool surekli(const ida_kes_red_t *r, int64_t now_ns, int64_t sure_ns)
{
    return r->var && ((now_ns - r->bas_ns) >= sure_ns);
}

/* Sağlık düzeyi ve geçerlilik (02 §2.3.6; tazelik 02 §4.1, A4-K36), çökme mandalı hariç.
 *
 * Yatay hız geçerli: tutum ve yön geçerli; yatay hız σ'sı ≤ SIG_HVEL; ve bir hız kaynağı
 *   destekliyor: DVL (akış canlı, son füzyon T_DVL_STALE içinde, kapı reddi KES_RED_MS'tir
 *   sürmüyor) ya da GNSS hızı (aynı koşullar, T_GNSS_STALE).
 * Konum geçerli: yatay konum hizalamadan beri GNSS'ten kuruldu; yatay konum σ'sı ≤ SIG_HPOS;
 *   konumu taşıyan bir destek var (geçerli yatay hızla ölü hesap ya da canlı ve taze GNSS
 *   konumu); GNSS konum kapısının reddi KES_RED_MS'tir sürmüyor.
 * Düzey (güvenli yöne yaslanır; IDA_EST_OK silahlanmanın ön koşuludur, A4 §3.16):
 *   BAD: bayraklı IMU, tutum σ'sı eşik üstü, yerçekimi ya da basınç kapısının sürekli reddi.
 *   OK: tutum, yön, derinlik, yatay hız ve konum geçerli, çıkış gecikmesi yok VE bu koşullar
 *     KES_OK_MS boyunca (duvar saati) kesintisiz sağlandı. Koşullardan biri bir çağrıda
 *     bozulursa düzey aynı çağrıda DEGRADED olur ve süre baştan sayılır: kaynakların sınırda
 *     gidip gelmesi düzeyi her çağrıda değiştirmez, OK'a dönüş her seferinde KES_OK_MS ister.
 *   DEGRADED: ötekiler.
 */
static void suzgec_duzeyi(ida_kes_t *k, const ida_kes_ayar_t *a)
{
    if (!k->hizali) {
        k->duzey = (uint8_t)IDA_EST_INIT;
        k->gecerlilik = 0u;
        k->ok_aday_var = false;
        return;
    }
    const int64_t now = a->now_ns;
    const float sx = ida_karekok(k->P[IDA_KES_ITH][IDA_KES_ITH]);
    const float sy = ida_karekok(k->P[IDA_KES_ITH + 1u][IDA_KES_ITH + 1u]);
    const float stilt = (sx > sy) ? sx : sy;
    const ida_m3_t r = ida_q_dcm(k->xh.q);
    float yv = 0.0f;
    for (uint32_t i = 0u; i < 3u; i++) {
        for (uint32_t j = 0u; j < 3u; j++) {
            yv += r.m[2][i] * k->P[IDA_KES_ITH + i][IDA_KES_ITH + j] * r.m[2][j];
        }
    }
    const float syaw = ida_karekok(yv);
    const float sd = ida_karekok(k->P[IDA_KES_IP + 2u][IDA_KES_IP + 2u]);
    const float sn = ida_karekok(k->P[IDA_KES_IP][IDA_KES_IP]);
    const float se = ida_karekok(k->P[IDA_KES_IP + 1u][IDA_KES_IP + 1u]);
    const float sh = (sn > se) ? sn : se;

    const bool grav_kotu = surekli(&k->red_grav, now, a->red_ns);
    const bool baro_kotu = surekli(&k->red_baro, now, a->red_ns);
    const bool tutum = (stilt <= a->sig_tilt) && !grav_kotu &&
                       destekli(k->grav_destek_var, k->grav_destek_ns, now, a->red_ns);
    const bool yon = k->yon_var && (syaw <= a->sig_yaw) &&
                     canli(&k->akis_mag, now, a->mag_stale_ns) &&
                     destekli(k->akis_mag.destek_var, k->akis_mag.destek_ns, now, a->mag_stale_ns) &&
                     !surekli(&k->red_mag, now, a->red_ns);
    const bool derinlik =
        k->kalibre && (sd <= a->sig_depth) && canli(&k->akis_baro, now, a->baro_stale_ns) &&
        destekli(k->akis_baro.destek_var, k->akis_baro.destek_ns, now, a->baro_stale_ns) &&
        !baro_kotu;
    const float svn = ida_karekok(k->P[IDA_KES_IV][IDA_KES_IV]);
    const float sve = ida_karekok(k->P[IDA_KES_IV + 1u][IDA_KES_IV + 1u]);
    const float sv = (svn > sve) ? svn : sve;
    const bool hiz_dvl =
        canli(&k->akis_dvl, now, a->dvl_stale_ns) &&
        destekli(k->akis_dvl.destek_var, k->akis_dvl.destek_ns, now, a->dvl_stale_ns) &&
        !surekli(&k->red_dvl, now, a->red_ns);
    const bool gnss_canli = canli(&k->akis_gnss, now, a->gnss_stale_ns);
    const bool hiz_gnss =
        gnss_canli &&
        destekli(k->gnss_hiz_destek_var, k->gnss_hiz_destek_ns, now, a->gnss_stale_ns) &&
        !surekli(&k->red_gnss_v, now, a->red_ns);
    const bool yatay_hiz = tutum && yon && (sv <= a->sig_hvel) && (hiz_dvl || hiz_gnss);
    const bool kon_gnss =
        gnss_canli && destekli(k->akis_gnss.destek_var, k->akis_gnss.destek_ns, now, a->gnss_stale_ns);
    const bool konum = k->konum_var && (sh <= a->sig_hpos) && (yatay_hiz || kon_gnss) &&
                       !surekli(&k->red_gnss_p, now, a->red_ns);

    uint32_t g = 0u;
    g |= tutum ? IDA_KES_G_TUTUM : 0u;
    g |= yon ? IDA_KES_G_YON : 0u;
    g |= derinlik ? (IDA_KES_G_DERINLIK | IDA_KES_G_DIKEY_HIZ) : 0u;
    g |= yatay_hiz ? IDA_KES_G_YATAY_HIZ : 0u;
    g |= konum ? IDA_KES_G_KONUM : 0u;
    k->gecerlilik = (uint8_t)g;

    if (k->imu_bayrakli || !(stilt <= a->sig_tilt) || grav_kotu || baro_kotu) {
        k->duzey = (uint8_t)IDA_EST_BAD;
        k->ok_aday_var = false;
        if (k->imu_bayrakli) {
            k->gecerlilik = 0u; /* bayraklı IMU ile yayılım yapılamaz (bulgu 3) */
        }
    } else if (!(sh <= a->sig_hpos) || !tutum || !yon || !derinlik || !yatay_hiz || !konum ||
               k->cikis_gec) {
        k->duzey = (uint8_t)IDA_EST_DEGRADED;
        k->ok_aday_var = false;
    } else {
        /* Koşullar sağlanıyor: OK ancak KES_OK_MS kesintisiz sürdükten sonra. */
        if (!k->ok_aday_var) {
            k->ok_aday_var = true;
            k->ok_aday_ns = now;
        }
        k->duzey = ((now - k->ok_aday_ns) >= a->ok_ns) ? (uint8_t)IDA_EST_OK
                                                       : (uint8_t)IDA_EST_DEGRADED;
    }
    if (k->cikis_gec) {
        k->gecerlilik = 0u; /* çıkış durumu t_ref'in 100 ms'den fazla gerisinde (bulgu 8) */
    }
}

/* Yayımlanan düzey: süzgeç düzeyi; sayısal çökme mandalı kuruluyken FAULT ve geçerlilik 0
   (02 §2.3.6). Süzgeç düzeyi mandaldan bağımsız ayrıca tutulur (ida_kes_suzgec_duzey). */
static void duzey_guncelle(ida_kes_t *k, const ida_kes_ayar_t *a)
{
    suzgec_duzeyi(k, a);
    k->suzgec_duzey = k->duzey;
    if (k->ariza) {
        k->duzey = (uint8_t)IDA_EST_FAULT;
        k->gecerlilik = 0u;
    }
}

static float kok(float x)
{
    return (x > 0.0f) ? ida_karekok(x) : 0.0f;
}

/* Çıkış zamanı için muhafazakâr 1σ (sol bulgu 7). Ufuk P'si t_h'dedir; çıkış L = t_o − t_h
   ileridedir. Eksen başına, korelasyonlar yok sayılıp üçgen eşitsizliğiyle üstten sınırlanır:
     σv(t_o) = √(P_vv + q_a² L) + L·a_σ,          a_σ = g·σ_eğim + σ_ba
     σp(t_o) = √P_pp + L·√P_vv + ½L²·a_σ + √(q_a² L³/3)
     σθ(t_o) = √(P_θθ + q_g² L) + L·σ_bg
   ve çıkışın ufuktan henüz kapanmamış farkı (|e|·(1 − k)) eklenir. Bu bir alt sınır değildir
   (iyimser değildir); tam kovaryans yayılımı değildir. */
static void cikis_sigma(ida_kes_t *k, const ida_kes_ayar_t *a)
{
    float L = ida_kes_ns_s(k->to_ns - k->th_ns);
    if (!(L > 0.0f)) {
        L = 0.0f;
    }
    const float sx = kok(k->P[IDA_KES_ITH][IDA_KES_ITH]);
    const float sy = kok(k->P[IDA_KES_ITH + 1u][IDA_KES_ITH + 1u]);
    const float stilt = (sx > sy) ? sx : sy;
    const float art_p[3] = {k->art_p.x, k->art_p.y, k->art_p.z};
    const float art_v[3] = {k->art_v.x, k->art_v.y, k->art_v.z};
    const float art_t[3] = {k->art_th.x, k->art_th.y, k->art_th.z};
    float sp[3];
    float sv[3];
    float st[3];
    float sb[3];
    const float qa2 = a->q_acc * a->q_acc;
    const float qg2 = a->q_gyro * a->q_gyro;
    for (uint32_t i = 0u; i < 3u; i++) {
        const float pv = k->P[IDA_KES_IV + i][IDA_KES_IV + i];
        const float au = a->g * stilt + kok(k->P[IDA_KES_IBA + i][IDA_KES_IBA + i]);
        sb[i] = kok(k->P[IDA_KES_IBG + i][IDA_KES_IBG + i]);
        sv[i] = kok(pv + qa2 * L) + L * au + art_v[i];
        sp[i] = kok(k->P[IDA_KES_IP + i][IDA_KES_IP + i]) + L * kok(pv) + 0.5f * L * L * au +
                kok(qa2 * L * L * L / 3.0f) + art_p[i];
        st[i] = kok(k->P[IDA_KES_ITH + i][IDA_KES_ITH + i] + qg2 * L) + L * sb[i] + art_t[i];
    }
    const ida_v3_t vp = {sp[0], sp[1], sp[2]};
    const ida_v3_t vv = {sv[0], sv[1], sv[2]};
    const ida_v3_t vt = {st[0], st[1], st[2]};
    const ida_v3_t vb = {sb[0], sb[1], sb[2]};
    k->sig_p = vp;
    k->sig_v = vv;
    k->sig_th = vt;
    k->sig_bg = vb;
}

/* nav.altitude: O_b'nin dibe mesafesi. Kaynaklar DVL irtifası (dönüştürücüden dibe dik mesafe;
   A4 §3.3, W4-K8) ve Ping1D menzilidir (ışın gövde +z ekseninde sayılır; düz dip kabulüyle
   düşeye izdüşümü). Her ikisi de montaj kolunun düşey bileşeniyle O_b'ye taşınır. Kaynak canlı
   ve son örneği kullanılabilir olmalıdır; ikisi de varsa küçüğü verilir (en az irtifa
   denetiminde güvenli yön). Son örnekten beri derinlik değişimi eklenmez: değer en çok bir
   örnek aralığı eskidir. Kaynak yoksa −1. */
static void irtifa_guncelle(ida_kes_t *k, const ida_kes_ayar_t *a)
{
    k->irtifa = -1.0f;
    if (!k->hizali) {
        return;
    }
    const bool dvl = k->irt_dvl_var && canli(&k->akis_dvl, a->now_ns, a->dvl_stale_ns);
    const bool ping = k->irt_ping_var && canli(&k->akis_ping, a->now_ns, a->dvl_stale_ns);
    if (!dvl && !ping) {
        return;
    }
    const ida_m3_t r = ida_q_dcm(k->q_o);
    bool var = false;
    float alt = 0.0f;
    if (dvl) {
        alt = k->irt_dvl + (r.m[2][0] * a->l_dvl.x + r.m[2][1] * a->l_dvl.y + r.m[2][2] * a->l_dvl.z);
        var = true;
    }
    if (ping && (r.m[2][2] >= IDA_KES_PING_COS_EN_AZ)) {
        const float x = k->irt_ping * r.m[2][2] +
                        (r.m[2][0] * a->l_ping.x + r.m[2][1] * a->l_ping.y + r.m[2][2] * a->l_ping.z);
        if (!var || (x < alt)) {
            alt = x;
        }
        var = true;
    }
    if (var && ida_sonlu(alt)) {
        k->irtifa = (alt > 0.0f) ? alt : 0.0f;
    }
}

static void ortak(ida_kes_t *k, const ida_kes_ayar_t *a)
{
    k->orj_lat_e7 = a->orj_lat_e7;
    k->orj_lon_e7 = a->orj_lon_e7;
    if (!k->hizali) {
        k->yolc_n = 0u; /* hizalamadan önceki yatay yardım ölçümleri kullanılmaz */
        hizalama_olcumler(k, a);
        const ida_kes_hiz_t *h = &k->hz;
        if ((h->n >= 2u) && (ida_kes_ns_s(h->t_son_ns - h->t0_ns) >= a->hiz_s)) {
            hizalama_bitir(k, a);
        }
    } else {
        ida_kes_ufuk_ilerle(k, a);
    }
    if (k->hizali) {
        /* Hizalamanın bittiği çağrı dahil bütün yollar, çıktıdan önce aynı denetimden geçer
           (sol bulgu 9). */
        cikis_sigma(k, a);
        k->cikis_gec = a->tref_var && ((a->tref_ns - k->to_ns) > (int64_t)IDA_KES_TASIMA_MAX_NS);
        if (!ida_kes_saglam(k) || !cikis_sonlu(k)) {
            /* Sayısal çökme (02 §2.3.6): mandal aynı çağrıda kurulur, filtre yeniden hizalanır. */
            k->ariza = true;
            k->n_cokme++;
            yeniden_hizala(k);
        }
    }
    irtifa_guncelle(k, a);
    duzey_guncelle(k, a);
}

void ida_kes_adim(ida_kes_t *k, const ida_core_t *c)
{
    ida_kes_ayar_t a;
    ida_kes_ayar_oku(c, &a);
    imu_isle(k, &a);
    ortak(k, &a);
}

void ida_kes_tick(ida_kes_t *k, const ida_core_t *c)
{
    ida_kes_ayar_t a;
    ida_kes_ayar_oku(c, &a);
    ortak(k, &a);
}

uint8_t ida_kes_duzey(const ida_kes_t *k)
{
    return k->duzey;
}

uint8_t ida_kes_suzgec_duzey(const ida_kes_t *k)
{
    return k->suzgec_duzey;
}

uint8_t ida_kes_gecerlilik(const ida_kes_t *k)
{
    return k->gecerlilik;
}

bool ida_kes_ariza_temizle(ida_kes_t *k)
{
    if (k->ariza && k->hizali) {
        k->ariza = false;
        return true;
    }
    return false;
}

void ida_kes_nav(const ida_kes_t *k, ida_stime_t t, ida_nav_out_t *nav)
{
    (void)memset(nav, 0, sizeof(*nav));
    nav->t = t;
    nav->est_level = k->duzey;
    nav->q_nb[0] = 1.0f;
    if ((k->duzey == (uint8_t)IDA_EST_INIT) || (k->duzey == (uint8_t)IDA_EST_FAULT) || !k->hizali) {
        return; /* A4-K87: INIT'te t ve est_level dışındakiler 0, q = (1,0,0,0); FAULT'ta da */
    }
    /* Çıkış durumu son IMU'nun sonundadır. t_ref'in en çok 100 ms gerisindeyse t_ref'e sabit
       ω ve v ile tam taşınır; daha eskiyse taşınmaz, durum kendi zamanıyla bildirilir
       (A4-K87 "çıkış durumunun ölçüm zamanı") ve geçerlilik düşer (sol bulgu 8). */
    const int64_t gec_ns = t.ns - k->to_ns;
    float dt = 0.0f;
    if ((gec_ns > 0) && (gec_ns <= (int64_t)IDA_KES_TASIMA_MAX_NS)) {
        dt = ida_kes_ns_s(gec_ns);
    } else {
        /* 100 ms'den eski (taşınmaz) ya da t_ref'te/sonrasında (IMU t_ref'i geçmez, A4 3.2;
           savunmacı): durum kendi zamanıyla bildirilir. */
        nav->t.ns = k->to_ns;
    }
    ida_q_t dq;
    ida_q_t q = k->q_o;
    if (ida_q_donme_vektorunden(ida_v3_olcekle(k->w_o, dt), &dq)) {
        ida_q_t b;
        q = ida_q_birimle(ida_q_carp(k->q_o, dq), &b) ? b : k->q_o;
    }
    const ida_v3_t p = ida_v3_topla(k->p_o, ida_v3_olcekle(k->v_o, dt));
    nav->p_ned[0] = p.x;
    nav->p_ned[1] = p.y;
    nav->p_ned[2] = p.z;
    nav->v_ned[0] = k->v_o.x;
    nav->v_ned[1] = k->v_o.y;
    nav->v_ned[2] = k->v_o.z;
    nav->q_nb[0] = q.w;
    nav->q_nb[1] = q.x;
    nav->q_nb[2] = q.y;
    nav->q_nb[3] = q.z;
    nav->omega_b[0] = k->w_o.x;
    nav->omega_b[1] = k->w_o.y;
    nav->omega_b[2] = k->w_o.z;
    nav->depth = p.z;
    nav->altitude = k->irtifa; /* bilinmiyorsa −1 */
    nav->origin_lat_e7 = k->orj_lat_e7; /* p_ned bu noktaya göredir (ORIJIN_LAT, ORIJIN_LON) */
    nav->origin_lon_e7 = k->orj_lon_e7;
    /* Çıkış zamanının muhafazakâr σ'sı (cikis_sigma) ve t_ref'e taşımanın katkısı. */
    nav->p_sigma[0] = k->sig_p.x + dt * k->sig_v.x;
    nav->p_sigma[1] = k->sig_p.y + dt * k->sig_v.y;
    nav->p_sigma[2] = k->sig_p.z + dt * k->sig_v.z;
    nav->v_sigma[0] = k->sig_v.x;
    nav->v_sigma[1] = k->sig_v.y;
    nav->v_sigma[2] = k->sig_v.z;
    nav->att_sigma[0] = k->sig_th.x + dt * k->sig_bg.x;
    nav->att_sigma[1] = k->sig_th.y + dt * k->sig_bg.y;
    nav->att_sigma[2] = k->sig_th.z + dt * k->sig_bg.z;
    nav->reset_counter = k->reset_sayac; /* A4 §3.8: konum sıfırlamasında artar */
}

bool ida_kes_tutarli(const ida_kes_t *k)
{
    return (k->duzey <= (uint8_t)IDA_EST_FAULT) && (k->suzgec_duzey <= (uint8_t)IDA_EST_BAD) &&
           (k->imu_n <= IDA_KES_IMU_N) &&
           (k->imu_bas < IDA_KES_IMU_N) && (k->olc_n <= IDA_KES_OLC_N) &&
           (k->yolc_n <= IDA_KES_YOLC_N) && (k->gecerlilik <= 0x3Fu);
}

uint8_t ida_kes_param_grup(const ida_prm_kume_t *p)
{
    (void)p; /* bu dilimde ilişkili grup koşulu yok; her parametre kendi aralığıyla geçerli */
    return 0u;
}
