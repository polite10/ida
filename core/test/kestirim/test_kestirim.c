/*
 * test_kestirim.c — W3 kestirim dilim 1 kabulü (docs/gorevler/W3-kestirim.md, Kabul 1–5) ve
 * modülün birim sınamaları.
 *
 * Yer gerçeği bu dosyada üretilir (double; sabit tohum): senaryo fonksiyonu Euler açılarını
 * ve konumu verir, türevler merkezi farkla alınır. Duyargalar İDA-0 düzenindedir (IMU 200 Hz,
 * 5 ms pencere; manyetometre 100 Hz; basınç 25 Hz, 18,75 ms pencere; montajlar
 * kestirim_param.def varsayılanlarıyla aynı). Çekirdek port üzerinden sürülür (ida_core_step);
 * sapma ve geçerlilik gibi iç büyüklükler cekirdek.h'tan okunur.
 *
 * Hata sınırları bu dosyada sabittir ve ilk koşudan önce yazılmıştır (02 §7 kuralı). Gürültü
 * düzeyleri A5 ürün profillerinden türetilmiş tahminlerdir.
 */
#include <math.h>
#include <string.h>

#include "destek.h"
#include "kestirim/kes_ic.h"
#include "mat/ida_math.h"
#include "unity.h"

void setUp(void);
void tearDown(void);
void setUp(void) {}
void tearDown(void) {}

/* ---- Sabitler ---------------------------------------------------------------------------- */

#define T_ADIM_NS (5 * DS_MS)
#define G0 9.80665
#define RHO 1000.0
#define P_ATM_GERCEK 101100.0 /* senaryonun atmosferi; çekirdek kalibrasyonla bulur */
#define BARO_OFSET 300.0      /* Pa, sensörün mutlak hatası (kalibrasyon soğurur) */
#define PI_D 3.14159265358979323846

static const double B_N[3] = {25.6e-6, 2.7e-6, 40.3e-6}; /* havuz senaryosu, T (NED) */
static const double L_IMU[3] = {0.0, 0.0, -0.03};
static const double L_BARO[3] = {-0.19, 0.0, -0.03};

/* ---- Yer gerçeği ------------------------------------------------------------------------- */

typedef struct {
    double roll;
    double pitch;
    double yaw;  /* sarılmamış */
    double n;
    double e;
    double d;    /* O_b derinliği (NED aşağı) */
} poz_t;

typedef void (*senaryo_f)(double t, poz_t *p);

static void rot(const poz_t *p, double r[3][3])
{
    const double cr = cos(p->roll), sr = sin(p->roll);
    const double cp = cos(p->pitch), sp = sin(p->pitch);
    const double cy = cos(p->yaw), sy = sin(p->yaw);
    r[0][0] = cy * cp;
    r[0][1] = cy * sp * sr - sy * cr;
    r[0][2] = cy * sp * cr + sy * sr;
    r[1][0] = sy * cp;
    r[1][1] = sy * sp * sr + cy * cr;
    r[1][2] = sy * sp * cr - cy * sr;
    r[2][0] = -sp;
    r[2][1] = cp * sr;
    r[2][2] = cp * cr;
}

static void carp_t(double r[3][3], const double v[3], double o[3]) /* Rᵀ v */
{
    for (int i = 0; i < 3; i++) {
        o[i] = r[0][i] * v[0] + r[1][i] * v[1] + r[2][i] * v[2];
    }
}

static void carp(double r[3][3], const double v[3], double o[3]) /* R v */
{
    for (int i = 0; i < 3; i++) {
        o[i] = r[i][0] * v[0] + r[i][1] * v[1] + r[i][2] * v[2];
    }
}

static void capraz(const double a[3], const double b[3], double o[3])
{
    o[0] = a[1] * b[2] - a[2] * b[1];
    o[1] = a[2] * b[0] - a[0] * b[2];
    o[2] = a[0] * b[1] - a[1] * b[0];
}

/* Gövde açısal hızı (Euler türevlerinden, ZYX). */
static void omega(senaryo_f s, double t, double w[3])
{
    const double h = 1.0e-4;
    poz_t a, b, c;
    s(t - h, &a);
    s(t + h, &b);
    s(t, &c);
    const double rd = (b.roll - a.roll) / (2.0 * h);
    const double pd = (b.pitch - a.pitch) / (2.0 * h);
    const double yd = (b.yaw - a.yaw) / (2.0 * h);
    w[0] = rd - yd * sin(c.pitch);
    w[1] = pd * cos(c.roll) + yd * cos(c.pitch) * sin(c.roll);
    w[2] = -pd * sin(c.roll) + yd * cos(c.pitch) * cos(c.roll);
}

/* NED ivme (konumun ikinci türevi). */
static void ivme(senaryo_f s, double t, double a[3])
{
    const double h = 1.0e-3;
    poz_t m, o, p;
    s(t - h, &m);
    s(t, &o);
    s(t + h, &p);
    a[0] = (p.n - 2.0 * o.n + m.n) / (h * h);
    a[1] = (p.e - 2.0 * o.e + m.e) / (h * h);
    a[2] = (p.d - 2.0 * o.d + m.d) / (h * h);
}

static void hiz(senaryo_f s, double t, double v[3])
{
    const double h = 1.0e-3;
    poz_t m, p;
    s(t - h, &m);
    s(t + h, &p);
    v[0] = (p.n - m.n) / (2.0 * h);
    v[1] = (p.e - m.e) / (2.0 * h);
    v[2] = (p.d - m.d) / (2.0 * h);
}

/* ---- Gürültü ----------------------------------------------------------------------------- */

static double gauss(ds_rastgele_t *r)
{
    double s = 0.0;
    for (int i = 0; i < 12; i++) {
        s += (double)ds_rastgele(r) / 4294967296.0;
    }
    return s - 6.0; /* Irwin–Hall: ortalama 0, varyans 1 */
}

/* ---- Koşu --------------------------------------------------------------------------------- */

typedef struct {
    senaryo_f sen;
    uint32_t tohum;
    double bg[3];             /* jiroskop sapması, rad/s */
    double ba[3];             /* ivmeölçer sapması, m/s² */
    double s_gyro, s_acc, s_mag, s_baro;
    double baro_kes_bas, baro_kes_son; /* basınç yok aralığı, s */
    double mag_kes_bas, mag_kes_son;
    double mag_boz_bas, mag_boz_son, mag_boz_aci; /* alan z etrafında döner */
    double mag_olcek_bas, mag_olcek_son, mag_olcek; /* alan normu ölçeklenir (norm kapısı) */
    double baro_gec;          /* s; basınç t_meas'ı bu kadar geride (geç ölçüm) */
    double baro_oran_bas, baro_oran; /* bu andan sonra t_meas'ın ilerleme oranı (0: yok) */
    bool baro_ozel;           /* 0,3 s'ten sonraki ilk üç basınç: 3e38, 1, 1 Pa */
    double inst1_bas, inst1_son; /* tek adımlarda inst = 1 IMU (geri zamanlı) ve inst = 1 basınç */
    double bayrak_bas, bayrak_son; /* IMU SATURATED bayraklı */
    double bayrak_gyro;       /* sıfırdan farklıysa bayraklı örneğin jiroskop x değeri, rad/s */
    double imu_gec1_bas, imu_gec1, imu_gec2_bas, imu_gec2; /* IMU t_meas gecikmesi, s */
} kosu_ayar_t;

typedef struct {
    kosu_ayar_t a;
    ds_rastgele_t r;
    uint32_t seq;
    uint32_t imu_seq, mag_seq, baro_seq, imu1_seq, baro1_seq;
    uint32_t baro_ozel_n;
    int64_t t_ns;
} kosu_t;

static const int64_t T0_NS = 2000 * DS_MS; /* senaryo t = 0 */

static void kosu_kur(kosu_t *k, const kosu_ayar_t *a)
{
    (void)memset(k, 0, sizeof(*k));
    k->a = *a;
    k->r.s = a->tohum;
    k->t_ns = T0_NS;
}

static double ns_t(int64_t ns)
{
    return (double)(ns - T0_NS) * 1.0e-9;
}

static int64_t t_ns_d(double t)
{
    return T0_NS + (int64_t)llround(t * 1.0e9);
}

static bool aralikta(double t, double bas, double son)
{
    return (t >= bas) && (t < son);
}

static double imu_gecikmesi(const kosu_ayar_t *a, double t)
{
    if ((a->imu_gec2_bas > 0.0) && (t >= a->imu_gec2_bas)) {
        return a->imu_gec2;
    }
    if ((a->imu_gec1_bas > 0.0) && (t >= a->imu_gec1_bas)) {
        return a->imu_gec1;
    }
    return 0.0;
}

/* IMU örneği: t_meas = ti; değer pencerenin ortasında (aralık ortalamasına ikinci mertebeden
   yakın). */
static void imu_uret(kosu_t *k, int64_t ti, ida_imu_sample_t *o)
{
    senaryo_f s = k->a.sen;
    const double tm = ns_t(ti) - 0.0025;
    poz_t p;
    s(tm, &p);
    double r[3][3];
    rot(&p, r);
    double w[3], wp[3], wm[3], al[3], an[3];
    omega(s, tm, w);
    omega(s, tm + 1.0e-3, wp);
    omega(s, tm - 1.0e-3, wm);
    for (int i = 0; i < 3; i++) {
        al[i] = (wp[i] - wm[i]) / 2.0e-3;
    }
    ivme(s, tm, an);
    const double ag[3] = {an[0], an[1], an[2] - G0};
    double fo[3], c1[3], c2[3], c3[3];
    carp_t(r, ag, fo);
    capraz(w, L_IMU, c1);
    capraz(w, c1, c2);
    capraz(al, L_IMU, c3);
    o->hdr.t_meas.ns = ti;
    for (int i = 0; i < 3; i++) {
        o->imu.gyro[i] = (float)(w[i] + k->a.bg[i] + k->a.s_gyro * gauss(&k->r));
        o->imu.accel[i] = (float)(fo[i] + c2[i] + c3[i] + k->a.ba[i] + k->a.s_acc * gauss(&k->r));
    }
}

/* Bir sonraki partinin girdisi (çağrı başına 5 ms). */
static void girdi_kur(kosu_t *k, ida_in_t *in)
{
    k->seq++;
    k->t_ns += T_ADIM_NS;
    const int64_t T = k->t_ns;
    ds_girdi(in, T, k->seq, true);
    senaryo_f s = k->a.sen;
    const double tT = ns_t(T);

    if (aralikta(tT, k->a.inst1_bas, k->a.inst1_son) && ((k->seq % 2u) == 1u)) {
        /* Başka örnek numaralı IMU: kendi izinde ilerler ama ortak zamanda 20 ms geride;
           değeri anlamsız (2 rad/s). */
        k->imu1_seq++;
        in->imu.hdr.inst = 1u;
        in->imu.hdr.seq = k->imu1_seq;
        in->imu.hdr.t_meas.ns = T - 20 * DS_MS;
        in->imu.imu.gyro[0] = 0.0f;
        in->imu.imu.gyro[1] = 0.0f;
        in->imu.imu.gyro[2] = 2.0f;
    } else {
        const int64_t ti = T - (int64_t)llround(imu_gecikmesi(&k->a, tT) * 1.0e9);
        k->imu_seq++;
        in->imu.hdr.seq = k->imu_seq;
        imu_uret(k, ti, &in->imu);
        if (aralikta(tT, k->a.bayrak_bas, k->a.bayrak_son)) {
            in->imu.hdr.flags = (uint16_t)(IDA_HF_VALID | IDA_HF_SATURATED);
            if (k->a.bayrak_gyro != 0.0) {
                in->imu.imu.gyro[0] = (float)k->a.bayrak_gyro;
            }
        }
    }

    uint8_t n = 0u;
    /* Manyetometre: t_meas = 10 ms ızgarası + 1 ms. */
    for (int64_t tmag = ((T - T_ADIM_NS) / (10 * DS_MS)) * (10 * DS_MS) + DS_MS; tmag <= T;
         tmag += 10 * DS_MS) {
        if (tmag <= T - T_ADIM_NS) {
            continue;
        }
        const double tt = ns_t(tmag);
        if (aralikta(tt, k->a.mag_kes_bas, k->a.mag_kes_son)) {
            continue;
        }
        poz_t q;
        s(tt, &q);
        double rq[3][3];
        rot(&q, rq);
        double bn[3] = {B_N[0], B_N[1], B_N[2]};
        if (aralikta(tt, k->a.mag_boz_bas, k->a.mag_boz_son)) {
            const double c = cos(k->a.mag_boz_aci), sn = sin(k->a.mag_boz_aci);
            bn[0] = c * B_N[0] - sn * B_N[1];
            bn[1] = sn * B_N[0] + c * B_N[1];
        }
        if (aralikta(tt, k->a.mag_olcek_bas, k->a.mag_olcek_son)) {
            for (int i = 0; i < 3; i++) {
                bn[i] *= k->a.mag_olcek;
            }
        }
        double mb[3];
        carp_t(rq, bn, mb);
        ida_sample_t *o = &in->samples[n++];
        ds_mag(o, tmag, ++k->mag_seq);
        o->hdr.t_rx.ns = T;
        for (int i = 0; i < 3; i++) {
            o->u.mag.field[i] = (float)(mb[i] + k->a.s_mag * gauss(&k->r));
        }
    }
    /* Basınç: t_meas = 40 ms ızgarası + 2 ms; pencere 18,75 ms; değer pencere ortasında. */
    const int64_t win = 18750000;
    for (int64_t tb = ((T - T_ADIM_NS) / (40 * DS_MS)) * (40 * DS_MS) + 2 * DS_MS; tb <= T;
         tb += 40 * DS_MS) {
        if (tb <= T - T_ADIM_NS) {
            continue;
        }
        const double tt = ns_t(tb);
        if (aralikta(tt, k->a.baro_kes_bas, k->a.baro_kes_son)) {
            continue;
        }
        const double tmid = ns_t(tb - win / 2);
        poz_t q;
        s(tmid, &q);
        double rq[3][3], rl[3];
        rot(&q, rq);
        carp(rq, L_BARO, rl);
        double pabs =
            P_ATM_GERCEK + BARO_OFSET + RHO * G0 * (q.d + rl[2]) + k->a.s_baro * gauss(&k->r);
        if (k->a.baro_ozel && (tt >= 0.3) && (k->baro_ozel_n < 3u)) {
            pabs = (k->baro_ozel_n == 0u) ? 3.0e38 : 1.0;
            k->baro_ozel_n++;
        }
        int64_t tmo = tb - (int64_t)llround(k->a.baro_gec * 1.0e9);
        if ((k->a.baro_oran_bas > 0.0) && (tt >= k->a.baro_oran_bas)) {
            const int64_t b0 = t_ns_d(k->a.baro_oran_bas);
            tmo = b0 + (int64_t)llround((double)(tb - b0) * k->a.baro_oran);
        }
        ida_sample_t *o = &in->samples[n++];
        (void)memset(o, 0, sizeof(*o));
        o->hdr.t_meas.ns = tmo;
        o->hdr.t_rx.ns = T;
        o->hdr.seq = ++k->baro_seq;
        o->hdr.window_ns = (uint32_t)win;
        o->hdr.flags = (uint16_t)IDA_HF_VALID;
        o->hdr.type = (uint8_t)IDA_SENS_BARO;
        o->u.baro.p_abs = (float)pabs;
        o->u.baro.temp_c = 20.0f;
        if (aralikta(tT, k->a.inst1_bas, k->a.inst1_son)) {
            /* Başka örnek numaralı basınç: +10 kPa (≈ 1 m) farklı. */
            ida_sample_t *o1 = &in->samples[n++];
            *o1 = *o;
            o1->hdr.inst = 1u;
            o1->hdr.seq = ++k->baro1_seq;
            o1->u.baro.p_abs = (float)(pabs + 10000.0);
        }
    }
    in->n_samples = n;
}

/* ---- Hata ölçümü ------------------------------------------------------------------------- */

typedef struct {
    double roll, pitch, yaw, derinlik, vd;
} hata_t;

static double sar(double a)
{
    while (a > PI_D) {
        a -= 2.0 * PI_D;
    }
    while (a <= -PI_D) {
        a += 2.0 * PI_D;
    }
    return a;
}

static void nav_hatasi(const kosu_t *k, const ida_nav_out_t *nav, hata_t *h)
{
    const double t = ns_t(nav->t.ns);
    poz_t p;
    k->a.sen(t, &p);
    const double w = (double)nav->q_nb[0], x = (double)nav->q_nb[1], y = (double)nav->q_nb[2],
                 z = (double)nav->q_nb[3];
    const double r20 = 2.0 * (x * z - w * y);
    const double r21 = 2.0 * (y * z + w * x);
    const double r22 = w * w - x * x - y * y + z * z;
    const double r10 = 2.0 * (x * y + w * z);
    const double r00 = w * w + x * x - y * y - z * z;
    const double roll = atan2(r21, r22);
    const double pitch = -asin(r20 > 1.0 ? 1.0 : (r20 < -1.0 ? -1.0 : r20));
    const double yaw = atan2(r10, r00);
    double v[3];
    hiz(k->a.sen, t, v);
    h->roll = fabs(sar(roll - p.roll));
    h->pitch = fabs(sar(pitch - p.pitch));
    h->yaw = fabs(sar(yaw - p.yaw));
    h->derinlik = fabs((double)nav->depth - p.d);
    h->vd = fabs((double)nav->v_ned[2] - v[2]);
}

static void en_buyuk(hata_t *m, const hata_t *h)
{
    m->roll = (h->roll > m->roll) ? h->roll : m->roll;
    m->pitch = (h->pitch > m->pitch) ? h->pitch : m->pitch;
    m->yaw = (h->yaw > m->yaw) ? h->yaw : m->yaw;
    m->derinlik = (h->derinlik > m->derinlik) ? h->derinlik : m->derinlik;
    m->vd = (h->vd > m->vd) ? h->vd : m->vd;
}

static bool nav_sonlu(const ida_nav_out_t *n)
{
    const float *f[] = {n->p_ned, n->v_ned, n->q_nb, n->omega_b, n->p_sigma, n->v_sigma, n->att_sigma};
    const int boy[] = {3, 3, 4, 3, 3, 3, 3};
    for (int i = 0; i < 7; i++) {
        for (int j = 0; j < boy[i]; j++) {
            if (!isfinite(f[i][j])) {
                return false;
            }
        }
    }
    return isfinite(n->depth) && isfinite(n->altitude);
}

static ida_core_t C;
static ida_core_t C2;
static ida_in_t IN;
static ida_out_t OUT;
static ida_out_t OUT2;

/* Koşuyu [0, sure) boyunca sürer; [deg_bas, deg_son) aralığında en büyük hatayı toplar. */
static void kos(kosu_t *k, double sure, double deg_bas, double deg_son, hata_t *m)
{
    (void)memset(m, 0, sizeof(*m));
    while (ns_t(k->t_ns) < sure) {
        girdi_kur(k, &IN);
        TEST_ASSERT_EQUAL(IDA_RC_OK, ida_core_step(&C, &IN, &OUT));
        TEST_ASSERT_TRUE(OUT.nav_valid);
        TEST_ASSERT_TRUE(nav_sonlu(&OUT.nav));
        const double t = ns_t(k->t_ns);
        if ((t >= deg_bas) && (t < deg_son)) {
            TEST_ASSERT_NOT_EQUAL(IDA_EST_INIT, OUT.nav.est_level);
            TEST_ASSERT_NOT_EQUAL(IDA_EST_FAULT, OUT.nav.est_level);
            hata_t h;
            nav_hatasi(k, &OUT.nav, &h);
            en_buyuk(m, &h);
        }
    }
}

/* ---- Senaryolar --------------------------------------------------------------------------- */

static void sen_durgun(double t, poz_t *p)
{
    (void)t;
    p->roll = 0.02;
    p->pitch = -0.01;
    p->yaw = 1.0;
    p->n = 0.0;
    p->e = 0.0;
    p->d = 0.0;
}

/* 0 ile 1 arasında yumuşak geçiş (5. derece; hız ve ivme uçlarda 0). */
static double yumusak(double x)
{
    if (x <= 0.0) {
        return 0.0;
    }
    if (x >= 1.0) {
        return 1.0;
    }
    return x * x * x * (10.0 + x * (-15.0 + 6.0 * x));
}

/* Integrali: ∫₀ˣ yumusak. */
static double yumusak_int(double x)
{
    if (x <= 0.0) {
        return 0.0;
    }
    if (x >= 1.0) {
        return 0.5 + (x - 1.0);
    }
    return x * x * x * x * (2.5 + x * (-3.0 + x));
}

/* 0–15 s durgun; 15–45 s sabit dönüş (0,2 rad/s; 1 s'lik yumuşak giriş ve çıkış); 45–75 s
   salınımlı yalpa (±0,15 rad, 0,25 Hz); 75–83 s yumuşak basamakla 0 → 3 m; 83–100 s 3 m. */
static void sen_hareketli(double t, poz_t *p)
{
    const double wz = 0.2;
    p->roll = 0.0;
    p->pitch = 0.0;
    p->yaw = 0.3 + wz * (yumusak_int(t - 15.0) - yumusak_int(t - 44.0));
    if ((t > 45.0) && (t < 75.0)) {
        p->roll = 0.15 * sin(2.0 * PI_D * 0.25 * (t - 45.0));
    }
    /* 100–130 s: eşzamanlı yön dönüşü (0,3 rad/s), yalpa (±0,15 rad, 0,3 Hz) ve yunuslama
       (±0,1 rad, 0,2 Hz); zarf 1 s'lik yumuşak geçişlerle. */
    p->yaw += 0.3 * (yumusak_int(t - 100.0) - yumusak_int(t - 129.0));
    if ((t > 100.0) && (t < 130.0)) {
        const double z = yumusak(t - 100.0) * yumusak(130.0 - t);
        p->roll = z * 0.15 * sin(2.0 * PI_D * 0.3 * (t - 100.0));
        p->pitch = z * 0.1 * sin(2.0 * PI_D * 0.2 * (t - 100.0));
    }
    p->n = 0.0;
    p->e = 0.0;
    p->d = 3.0 * yumusak((t - 75.0) / 8.0);
}

/* 0–5 s yüzeyde durgun; 5–13 s yumuşak basamakla 3 m'ye; sonra 3 m'de durgun. */
static void sen_dalis(double t, poz_t *p)
{
    sen_durgun(t, p);
    p->d = 3.0 * yumusak((t - 5.0) / 8.0);
}

/* 0–3 s durgun; 3–4 s yumuşak girişle 1 rad/s sabit yön dönüşü. */
static void sen_donus(double t, poz_t *p)
{
    sen_durgun(t, p);
    p->yaw = 1.0 + yumusak_int(t - 3.0);
}

/* Durgun; 30–32 s kuzeye +3 m/s², 32–34 s −3 m/s² (uzun süreli yüksek yatay ivme; ||f||
   yalnız ≈ 0,45 m/s² değişir, norm kısmasının altında kalır: yalnız vektör kapısı yakalar). */
static void sen_ivme(double t, poz_t *p)
{
    sen_durgun(t, p);
    const double a = 3.0;
    const double tt = 2.0;
    if (t > 30.0) {
        const double t1 = (t < 30.0 + tt) ? (t - 30.0) : tt;
        p->n = 0.5 * a * t1 * t1;
        if (t > 30.0 + tt) {
            const double t2 = (t < 30.0 + 2.0 * tt) ? (t - 30.0 - tt) : tt;
            p->n += a * tt * t2 - 0.5 * a * t2 * t2;
        }
    }
}

static kosu_ayar_t ayar_temel(senaryo_f s, uint32_t tohum)
{
    kosu_ayar_t a;
    (void)memset(&a, 0, sizeof(a));
    a.sen = s;
    a.tohum = tohum;
    a.bg[0] = 0.004;
    a.bg[1] = -0.003;
    a.bg[2] = 0.005;
    a.ba[0] = 0.05;
    a.ba[1] = -0.04;
    a.ba[2] = 0.06;
    a.s_gyro = 7.0e-4; /* 4,887e-5 rad/s/√Hz, 5 ms pencere (A5 icm42688p) */
    a.s_acc = 9.0e-3;  /* 6,4e-4 m/s²/√Hz, 5 ms pencere */
    a.s_mag = 1.0e-7;  /* T; profilin 2e-8'inden büyük (itici girişimi payı) */
    a.s_baro = 20.0;   /* Pa (ms5837 profili) */
    a.baro_kes_bas = a.baro_kes_son = -1.0;
    a.mag_kes_bas = a.mag_kes_son = -1.0;
    a.mag_boz_bas = a.mag_boz_son = -1.0;
    a.mag_olcek_bas = a.mag_olcek_son = -1.0;
    a.inst1_bas = a.inst1_son = -1.0;
    a.bayrak_bas = a.bayrak_son = -1.0;
    return a;
}

/* ---- Kabul 1: durgun ---------------------------------------------------------------------- */

static void test_k1_durgun(void)
{
    TEST_ASSERT_EQUAL(IDA_RC_OK, ds_kur(&C));
    kosu_t k;
    const kosu_ayar_t a = ayar_temel(sen_durgun, 0x1234567u);
    kosu_kur(&k, &a);
    hata_t m;
    kos(&k, 60.0, 20.0, 60.0, &m);
    TEST_ASSERT_TRUE_MESSAGE(m.roll < 0.01, "yuvarlanma hatası");
    TEST_ASSERT_TRUE_MESSAGE(m.pitch < 0.01, "yunuslama hatası");
    TEST_ASSERT_TRUE_MESSAGE(m.yaw < 0.03, "yön hatası");
    TEST_ASSERT_TRUE_MESSAGE(m.derinlik < 0.05, "derinlik hatası");
    TEST_ASSERT_TRUE_MESSAGE(m.vd < 0.05, "dikey hız hatası");
    const ida_v3_t bg = C.kes.xh.bg;
    TEST_ASSERT_TRUE_MESSAGE(fabs((double)bg.x - a.bg[0]) < 1.0e-3, "b_g x");
    TEST_ASSERT_TRUE_MESSAGE(fabs((double)bg.y - a.bg[1]) < 1.0e-3, "b_g y");
    TEST_ASSERT_TRUE_MESSAGE(fabs((double)bg.z - a.bg[2]) < 1.0e-3, "b_g z");
    const uint8_t g = ida_kes_gecerlilik(&C.kes);
    TEST_ASSERT_EQUAL_HEX8(IDA_KES_G_TUTUM | IDA_KES_G_YON | IDA_KES_G_DERINLIK | IDA_KES_G_DIKEY_HIZ, g);
    /* DVL/GNSS yok: 02 §2.3.6'ya göre DEGRADED. */
    TEST_ASSERT_EQUAL_UINT8(IDA_EST_DEGRADED, OUT.nav.est_level);
    TEST_ASSERT_EQUAL_UINT32(0u, C.kes.n_cokme);
    TEST_ASSERT_EQUAL_UINT32(1u, C.kes.n_hizalama);
}

/* ---- Kabul 2: hareketli ------------------------------------------------------------------- */

static void k2_denetle(const hata_t *m)
{
    TEST_ASSERT_TRUE_MESSAGE(m->roll < 0.01, "yuvarlanma hatası");
    TEST_ASSERT_TRUE_MESSAGE(m->pitch < 0.01, "yunuslama hatası");
    TEST_ASSERT_TRUE_MESSAGE(m->yaw < 0.02, "yön hatası");
    TEST_ASSERT_TRUE_MESSAGE(m->derinlik < 0.05, "derinlik hatası");
    TEST_ASSERT_TRUE_MESSAGE(m->vd < 0.05, "dikey hız hatası");
    TEST_ASSERT_EQUAL_UINT32(0u, C.kes.n_cokme);
}

static void test_k2_hareketli(void)
{
    static const uint32_t TOHUM[3] = {0xBEEF01u, 0x13579Bu, 0x2468ACu};
    for (uint32_t i = 0u; i < 3u; i++) {
        TEST_ASSERT_EQUAL(IDA_RC_OK, ds_kur(&C));
        kosu_t k;
        const kosu_ayar_t a = ayar_temel(sen_hareketli, TOHUM[i]);
        kosu_kur(&k, &a);
        hata_t m;
        kos(&k, 130.0, 15.0, 130.0, &m);
        k2_denetle(&m);
    }
}


/* ---- Kabul 3: bozulma --------------------------------------------------------------------- */

static void test_k3_manyetik_sicrama(void)
{
    TEST_ASSERT_EQUAL(IDA_RC_OK, ds_kur(&C));
    kosu_t k;
    kosu_ayar_t a = ayar_temel(sen_durgun, 0x5151u);
    a.mag_boz_bas = 20.0;
    a.mag_boz_son = 21.0;
    a.mag_boz_aci = 0.25; /* norm korunur: yalnız NIS kapısı yakalar */
    kosu_kur(&k, &a);
    hata_t m;
    kos(&k, 40.0, 15.0, 40.0, &m);
    /* Sınır kapı kapalıyken ölçülen hatanın (0,048 rad) altında: kapının işini ayırt eder. */
    TEST_ASSERT_TRUE_MESSAGE(m.yaw < 0.02, "manyetik sıçramada yön hatası");
    TEST_ASSERT_TRUE_MESSAGE(m.roll < 0.01, "manyetik sıçramada yuvarlanma hatası");
    TEST_ASSERT_TRUE_MESSAGE(m.pitch < 0.01, "manyetik sıçramada yunuslama hatası");
    TEST_ASSERT_EQUAL_UINT32(0u, C.kes.n_yon_sifir); /* bir saniyelik sıçrama sıfırlamaya varmaz */
}

static void test_k3_yuksek_ivme(void)
{
    TEST_ASSERT_EQUAL(IDA_RC_OK, ds_kur(&C));
    kosu_t k;
    const kosu_ayar_t a = ayar_temel(sen_ivme, 0x7777u);
    kosu_kur(&k, &a);
    hata_t m;
    kos(&k, 40.0, 15.0, 40.0, &m);
    /* Eğim hatasının tabanı ivme sapması / g ≈ 0,005 rad'dır (ayrıştırılamaz). Sınır, vektör
       kapısı kapalıyken ölçülen hatanın (≈ 0,010 rad) altında: kapının işini ayırt eder. */
    TEST_ASSERT_TRUE_MESSAGE(m.roll < 0.008, "yüksek ivmede yuvarlanma hatası");
    TEST_ASSERT_TRUE_MESSAGE(m.pitch < 0.008, "yüksek ivmede yunuslama hatası");
    TEST_ASSERT_TRUE_MESSAGE(m.yaw < 0.02, "yüksek ivmede yön hatası");
}

/* ---- Kabul 4: eksik sensör ---------------------------------------------------------------- */

static void kos_izle(kosu_t *k, double sure, double t_bak, uint8_t *g_bak)
{
    bool alindi = false;
    while (ns_t(k->t_ns) < sure) {
        girdi_kur(k, &IN);
        TEST_ASSERT_EQUAL(IDA_RC_OK, ida_core_step(&C, &IN, &OUT));
        TEST_ASSERT_TRUE(nav_sonlu(&OUT.nav));
        if (!alindi && (ns_t(k->t_ns) >= t_bak)) {
            *g_bak = ida_kes_gecerlilik(&C.kes);
            alindi = true;
        }
    }
}

static void test_k4_eksik_sensor(void)
{
    TEST_ASSERT_EQUAL(IDA_RC_OK, ds_kur(&C));
    kosu_t k;
    kosu_ayar_t a = ayar_temel(sen_hareketli, 0xA0A0u);
    a.baro_kes_bas = 78.0; /* derinlik değişirken basınç kesilir */
    a.baro_kes_son = 88.0;
    a.mag_kes_bas = 20.0; /* dönerken manyetometre kesilir */
    a.mag_kes_son = 30.0;
    kosu_kur(&k, &a);
    uint8_t g = 0u;
    hata_t m;
    kos(&k, 19.0, 15.0, 19.0, &m);
    TEST_ASSERT_TRUE((ida_kes_gecerlilik(&C.kes) & IDA_KES_G_YON) != 0u);
    kos_izle(&k, 21.0, 20.6, &g);
    TEST_ASSERT_TRUE_MESSAGE((g & IDA_KES_G_YON) == 0u, "manyetometre kesikken yön geçerli");
    TEST_ASSERT_TRUE((g & IDA_KES_G_DERINLIK) != 0u);
    kos_izle(&k, 30.0, 29.0, &g);
    TEST_ASSERT_TRUE((g & IDA_KES_G_YON) == 0u);
    TEST_ASSERT_NOT_EQUAL(IDA_EST_FAULT, OUT.nav.est_level);
    kos_izle(&k, 33.0, 32.0, &g);
    TEST_ASSERT_TRUE_MESSAGE((g & IDA_KES_G_YON) != 0u, "manyetometre dönünce yön toparlanmadı");
    kos(&k, 45.0, 33.0, 45.0, &m);
    TEST_ASSERT_TRUE_MESSAGE(m.yaw < 0.05, "manyetometre dönüşünden sonra yön hatası");
    kos(&k, 78.0, 45.0, 78.0, &m);
    kos_izle(&k, 80.0, 78.5, &g);
    TEST_ASSERT_TRUE_MESSAGE((g & IDA_KES_G_DERINLIK) == 0u, "basınç kesikken derinlik geçerli");
    TEST_ASSERT_TRUE((g & IDA_KES_G_DIKEY_HIZ) == 0u);
    kos_izle(&k, 88.0, 87.0, &g);
    TEST_ASSERT_TRUE((g & IDA_KES_G_DERINLIK) == 0u);
    kos_izle(&k, 90.0, 89.5, &g);
    TEST_ASSERT_TRUE_MESSAGE((g & IDA_KES_G_DERINLIK) != 0u, "basınç dönünce derinlik toparlanmadı");
    kos(&k, 100.0, 91.0, 100.0, &m);
    TEST_ASSERT_TRUE_MESSAGE(m.derinlik < 0.1, "basınç dönüşünden sonra derinlik hatası");
    TEST_ASSERT_EQUAL_UINT32(0u, C.kes.n_cokme);
}

/* ---- Kabul 5: belirlenimcilik (A4-K24) ---------------------------------------------------- */

static void test_k5_belirlenimcilik(void)
{
    TEST_ASSERT_EQUAL(IDA_RC_OK, ds_kur(&C));
    TEST_ASSERT_EQUAL(IDA_RC_OK, ds_kur(&C2));
    kosu_t k;
    const kosu_ayar_t a = ayar_temel(sen_hareketli, 0xD00Du);
    kosu_kur(&k, &a);
    uint32_t say = 0u;
    while (ns_t(k.t_ns) < 30.0) {
        girdi_kur(&k, &IN);
        const ida_rc_t r1 = ida_core_step(&C, &IN, &OUT);
        const ida_rc_t r2 = ida_core_step(&C2, &IN, &OUT2);
        TEST_ASSERT_EQUAL(r1, r2);
        TEST_ASSERT_EQUAL_MEMORY(&OUT, &OUT2, sizeof(OUT));
        say++;
    }
    TEST_ASSERT_EQUAL_MEMORY(&C.kes, &C2.kes, sizeof(C.kes));
    TEST_ASSERT_TRUE(say > 5000u);
    TEST_ASSERT_TRUE(OUT.nav.est_level != IDA_EST_INIT);
}

/* ---- Birim: INIT, hizalama, oturum, boşluk, çökme --------------------------------------- */

static void test_init_nav_sifir(void)
{
    TEST_ASSERT_EQUAL(IDA_RC_OK, ds_kur(&C));
    kosu_t k;
    const kosu_ayar_t a = ayar_temel(sen_durgun, 1u);
    kosu_kur(&k, &a);
    /* 0,5 s: hizalama (1 s) bitmedi. */
    while (ns_t(k.t_ns) < 0.5) {
        girdi_kur(&k, &IN);
        TEST_ASSERT_EQUAL(IDA_RC_OK, ida_core_step(&C, &IN, &OUT));
        ds_cikti_degismezleri(IDA_RC_OK, &IN, &OUT);
        TEST_ASSERT_EQUAL_UINT8(IDA_EST_INIT, OUT.nav.est_level);
        TEST_ASSERT_TRUE(OUT.nav.q_nb[0] == 1.0f);
        TEST_ASSERT_TRUE(OUT.nav.depth == 0.0f && OUT.nav.p_sigma[0] == 0.0f);
    }
    /* Hareket hizalamayı baştan başlatır: büyük ω'lu tek örnek. */
    girdi_kur(&k, &IN);
    IN.imu.imu.gyro[2] = 1.0f;
    TEST_ASSERT_EQUAL(IDA_RC_OK, ida_core_step(&C, &IN, &OUT));
    TEST_ASSERT_EQUAL_UINT32(0u, C.kes.hz.n);
    while (ns_t(k.t_ns) < 1.4) {
        girdi_kur(&k, &IN);
        TEST_ASSERT_EQUAL(IDA_RC_OK, ida_core_step(&C, &IN, &OUT));
    }
    TEST_ASSERT_EQUAL_UINT8(IDA_EST_INIT, OUT.nav.est_level);
    while (ns_t(k.t_ns) < 1.6) {
        girdi_kur(&k, &IN);
        TEST_ASSERT_EQUAL(IDA_RC_OK, ida_core_step(&C, &IN, &OUT));
    }
    TEST_ASSERT_NOT_EQUAL(IDA_EST_INIT, OUT.nav.est_level);
    TEST_ASSERT_EQUAL_UINT32(1u, C.kes.n_hizalama);
}

static void test_oturum_basi_ve_bosluk(void)
{
    TEST_ASSERT_EQUAL(IDA_RC_OK, ds_kur(&C));
    kosu_t k;
    const kosu_ayar_t a = ayar_temel(sen_durgun, 2u);
    kosu_kur(&k, &a);
    hata_t m;
    kos(&k, 3.0, 2.0, 3.0, &m);
    /* SESSION_START: kestirim yeniden başlar (A4-K72). */
    girdi_kur(&k, &IN);
    IN.plat.link_event = (uint8_t)IDA_LINKEV_SESSION_START;
    TEST_ASSERT_EQUAL(IDA_RC_OK, ida_core_step(&C, &IN, &OUT));
    TEST_ASSERT_EQUAL_UINT8(IDA_EST_INIT, OUT.nav.est_level);
    kos(&k, 6.0, 5.0, 6.0, &m);
    TEST_ASSERT_EQUAL_UINT32(2u, C.kes.n_hizalama);
    /* IMU_GAP_MAX (500 ms) aşan boşluk: yeniden hizalama. Zaman çerçeveleri tick'le sürer. */
    for (int i = 0; i < 120; i++) {
        girdi_kur(&k, &IN);
        IN.imu_present = false;
        IN.n_samples = 0u;
        TEST_ASSERT_EQUAL(IDA_RC_OK, ida_core_tick(&C, &IN, &OUT));
        TEST_ASSERT_FALSE(OUT.nav_valid);
    }
    girdi_kur(&k, &IN);
    TEST_ASSERT_EQUAL(IDA_RC_OK, ida_core_step(&C, &IN, &OUT));
    TEST_ASSERT_EQUAL_UINT8(IDA_EST_INIT, OUT.nav.est_level);
    kos(&k, 9.0, 8.5, 9.0, &m);
    TEST_ASSERT_EQUAL_UINT32(3u, C.kes.n_hizalama);
    TEST_ASSERT_TRUE(m.derinlik < 0.05); /* basınç referansı oturum içinde korunur */
}

static void test_sayisal_cokme(void)
{
    TEST_ASSERT_EQUAL(IDA_RC_OK, ds_kur(&C));
    kosu_t k;
    const kosu_ayar_t a = ayar_temel(sen_durgun, 3u);
    kosu_kur(&k, &a);
    hata_t m;
    kos(&k, 3.0, 2.0, 3.0, &m);
    /* Kovaryansı pozitif tanımlılıktan çıkar (02 §2.3.6). */
    C.kes.P[IDA_KES_IV][IDA_KES_ITH] = 10.0f;
    C.kes.P[IDA_KES_ITH][IDA_KES_IV] = 10.0f;
    girdi_kur(&k, &IN);
    TEST_ASSERT_EQUAL(IDA_RC_OK, ida_core_step(&C, &IN, &OUT));
    ds_cikti_degismezleri(IDA_RC_OK, &IN, &OUT);
    TEST_ASSERT_EQUAL_UINT8(IDA_EST_FAULT, OUT.nav.est_level);
    TEST_ASSERT_TRUE(OUT.nav.q_nb[0] == 1.0f && OUT.nav.p_ned[2] == 0.0f);
    TEST_ASSERT_EQUAL_UINT32(1u, C.kes.n_cokme);
    TEST_ASSERT_TRUE(OUT.health.core_ok);
    /* Mandal: yeniden hizalandıktan sonra da FAULT; temizleme çağrısıyla kalkar. */
    while (ns_t(k.t_ns) < 6.0) {
        girdi_kur(&k, &IN);
        TEST_ASSERT_EQUAL(IDA_RC_OK, ida_core_step(&C, &IN, &OUT));
        TEST_ASSERT_EQUAL_UINT8(IDA_EST_FAULT, OUT.nav.est_level);
    }
    TEST_ASSERT_TRUE(C.kes.hizali);
    TEST_ASSERT_TRUE(ida_kes_ariza_temizle(&C.kes));
    girdi_kur(&k, &IN);
    TEST_ASSERT_EQUAL(IDA_RC_OK, ida_core_step(&C, &IN, &OUT));
    TEST_ASSERT_EQUAL_UINT8(IDA_EST_DEGRADED, OUT.nav.est_level);
    /* Sonlu olmayan nominal durum da yakalanır. */
    C.kes.xh.v.x = ds_nan();
    girdi_kur(&k, &IN);
    TEST_ASSERT_EQUAL(IDA_RC_OK, ida_core_step(&C, &IN, &OUT));
    TEST_ASSERT_EQUAL_UINT8(IDA_EST_FAULT, OUT.nav.est_level);
    TEST_ASSERT_TRUE(nav_sonlu(&OUT.nav));
}

/* Ufuk: D değişse de (bütün ölçümler zamanında geldiğinde) ufuk aynı sırayla aynı işlemleri
   yapar; iki koşunun ufuk durumları aynı ufuk zamanında bit düzeyinde eşittir. */
static void test_ufuk_d_bagimsizligi(void)
{
    static ida_kes_nominal_t xa[4000];
    static int64_t ta[4000];
    uint32_t na = 0u;
    const uint32_t d_degerleri[2] = {50u, 150u};
    for (uint32_t r = 0u; r < 2u; r++) {
        TEST_ASSERT_EQUAL(IDA_RC_OK, ds_kur(&C));
        ida_param_t *p = &C.prm;
        p->yururluk.ham[IDA_PRM_kes_d_ms] = d_degerleri[r];
        kosu_t k;
        const kosu_ayar_t a = ayar_temel(sen_hareketli, 0x4242u);
        kosu_kur(&k, &a);
        uint32_t esit = 0u;
        uint32_t j = 0u;
        int64_t son_th = -1;
        while (ns_t(k.t_ns) < 25.0) {
            girdi_kur(&k, &IN);
            TEST_ASSERT_EQUAL(IDA_RC_OK, ida_core_step(&C, &IN, &OUT));
            if (!C.kes.hizali || (C.kes.th_ns == son_th) || (C.kes.th_ns % T_ADIM_NS) != 0) {
                continue;
            }
            son_th = C.kes.th_ns;
            if (r == 0u) {
                if (na < 4000u) {
                    xa[na] = C.kes.xh;
                    ta[na] = C.kes.th_ns;
                    na++;
                }
            } else {
                while ((j < na) && (ta[j] < C.kes.th_ns)) {
                    j++;
                }
                if ((j < na) && (ta[j] == C.kes.th_ns)) {
                    TEST_ASSERT_EQUAL_MEMORY(&xa[j], &C.kes.xh, sizeof(ida_kes_nominal_t));
                    esit++;
                }
            }
        }
        if (r == 1u) {
            TEST_ASSERT_TRUE(esit > 3000u);
        }
    }
}

/* ---- sol incelemesi (docs/gorevler/inceleme/W3-kestirim-sol.md) karşı örnekleri ------- */

static double q_fark(ida_q_t a, ida_q_t b)
{
    const ida_v3_t e = ida_kes_q_log(ida_q_carp(ida_q_eslenik(a), b));
    return (double)ida_v3_norm(e);
}

/* Bulgu 1: ufukta 0,02 rad x düzeltmesi, ardından gecikme boyunca 90° z dönüşü. Doğru çıkış
   Exp(δx) ⊗ q_z; sağdan uygulanan yerel düzeltme 0,028 rad yanlış eksen verir. */
static void test_b1_cikis_duzeltme_cercevesi(void)
{
    TEST_ASSERT_EQUAL(IDA_RC_OK, ds_kur(&C));
    ida_kes_ayar_t a;
    ida_kes_ayar_oku(&C, &a);
    a.tau_o = 0.005f; /* kazanç 1: düzeltme tek adımda tam */
    ida_kes_t *k = &C.kes;
    ida_kes_kur(k);
    ida_kes_nominal_t x;
    (void)memset(&x, 0, sizeof(x));
    x.q.w = 1.0f;
    const int64_t t0 = 10000 * DS_MS;
    ida_kes_ufuk_kur(k, &x, t0);
    k->hizali = true;
    k->d_ns = 150 * DS_MS;
    for (uint32_t i = 0u; i < IDA_KES_N; i++) {
        k->P[i][i] = 1.0e-4f;
    }
    const float wz = (0.5f * IDA_PI_F) / 0.15f;
    const ida_v3_t w = {0.0f, 0.0f, wz};
    const ida_v3_t w0 = {0.0f, 0.0f, 0.0f};
    const ida_v3_t f = {0.0f, 0.0f, -9.80665f};
    for (int64_t i = 1; i <= 30; i++) {
        ida_kes_ufuk_imu(k, t0 + i * T_ADIM_NS, 0.005f, w, f, &a);
    }
    const ida_v3_t dx = {0.02f, 0.0f, 0.0f};
    ida_q_t qd;
    TEST_ASSERT_TRUE(ida_q_donme_vektorunden(dx, &qd));
    k->xh.q = ida_q_carp(x.q, qd); /* ufuk t0'da düzeltilmiş */
    ida_kes_ufuk_imu(k, t0 + 31 * T_ADIM_NS, 0.005f, w0, f, &a);
    ida_kes_ufuk_ilerle(k, &a); /* bir kayıt: hedef t0 + 5 ms */
    TEST_ASSERT_TRUE(k->th_ns == t0 + T_ADIM_NS);
    const ida_q_t beklenen = ida_q_carp(qd, ida_kes_q_euler(0.0f, 0.0f, 0.5f * IDA_PI_F));
    TEST_ASSERT_TRUE_MESSAGE(q_fark(k->q_o, beklenen) < 1.0e-3, "çıkış düzeltmesi yanlış çerçevede");
    /* Halkadaki son kayıt da aynı düzeltmeyi almış olmalı. */
    const ida_kes_imu_kayit_t *son =
        &k->imu[((uint32_t)k->imu_bas + (uint32_t)k->imu_n - 1u) % IDA_KES_IMU_N];
    TEST_ASSERT_TRUE(q_fark(son->q_o, beklenen) < 1.0e-3);
}

/* Bulgu 2: inst = 1 IMU (ortak zamanda 20 ms geride, anlamsız değer) ve +1 m farklı inst = 1
   basınç kestirimi bozmamalı; negatif dt hiç işlenmemeli. */
static void test_b2_inst_ve_geri_zaman(void)
{
    TEST_ASSERT_EQUAL(IDA_RC_OK, ds_kur(&C));
    kosu_t k;
    kosu_ayar_t a = ayar_temel(sen_durgun, 0xB2u);
    a.inst1_bas = 4.0;
    a.inst1_son = 6.0;
    kosu_kur(&k, &a);
    hata_t m;
    kos(&k, 4.0, 3.0, 4.0, &m);
    kos(&k, 8.0, 4.0, 8.0, &m);
    TEST_ASSERT_TRUE_MESSAGE(m.yaw < 0.01, "inst 1 IMU yönü bozdu");
    TEST_ASSERT_TRUE_MESSAGE(m.roll < 0.01 && m.pitch < 0.01, "inst 1 IMU eğimi bozdu");
    TEST_ASSERT_TRUE_MESSAGE(m.derinlik < 0.05, "inst 1 basınç derinliği bozdu");
    TEST_ASSERT_TRUE(C.kes.n_inst > 200u); /* 2 s: tek adımlarda 200 IMU + 50 basınç */
    for (uint32_t i = 0u; i < C.kes.imu_n; i++) {
        TEST_ASSERT_TRUE(C.kes.imu[((uint32_t)C.kes.imu_bas + i) % IDA_KES_IMU_N].dt > 0.0f);
    }
}

/* Bulgu 3: SATURATED IMU ile hizalama ve yayılım yapılmaz; kullanılabilirlik düşer. */
static void test_b3_imu_bayraklari(void)
{
    TEST_ASSERT_EQUAL(IDA_RC_OK, ds_kur(&C));
    kosu_t k;
    kosu_ayar_t a = ayar_temel(sen_durgun, 0xB3u);
    a.bayrak_bas = 0.0;
    a.bayrak_son = 1.5; /* değerler makul, yalnız bayrak */
    kosu_kur(&k, &a);
    while (ns_t(k.t_ns) < 1.45) {
        girdi_kur(&k, &IN);
        TEST_ASSERT_EQUAL(IDA_RC_OK, ida_core_step(&C, &IN, &OUT));
    }
    TEST_ASSERT_EQUAL_UINT8_MESSAGE(IDA_EST_INIT, OUT.nav.est_level, "bayraklı IMU ile hizalandı");
    TEST_ASSERT_EQUAL_UINT32(0u, C.kes.hz.n);

    TEST_ASSERT_EQUAL(IDA_RC_OK, ds_kur(&C));
    a.bayrak_bas = 5.0;
    a.bayrak_son = 5.2;
    a.bayrak_gyro = 8.0;
    kosu_kur(&k, &a);
    hata_t m;
    kos(&k, 5.0, 3.0, 5.0, &m);
    while (ns_t(k.t_ns) < 5.19) {
        girdi_kur(&k, &IN);
        TEST_ASSERT_EQUAL(IDA_RC_OK, ida_core_step(&C, &IN, &OUT));
        TEST_ASSERT_EQUAL_HEX8_MESSAGE(0u, ida_kes_gecerlilik(&C.kes), "bayraklı IMU'da geçerlilik");
        TEST_ASSERT_TRUE(OUT.nav.est_level != IDA_EST_OK && OUT.nav.est_level != IDA_EST_DEGRADED);
    }
    kos(&k, 8.0, 5.3, 8.0, &m);
    TEST_ASSERT_TRUE(C.kes.n_bayrak >= 39u);
    TEST_ASSERT_TRUE_MESSAGE(m.roll < 0.01, "bayraklı IMU yuvarlanmayı bozdu");
    TEST_ASSERT_TRUE((ida_kes_gecerlilik(&C.kes) & IDA_KES_G_TUTUM) != 0u);
}

/* Bulgu 4: 3 m'de oturum yenilenince basınç referansı korunur. */
static void test_b4_oturum_derinlikte(void)
{
    TEST_ASSERT_EQUAL(IDA_RC_OK, ds_kur(&C));
    kosu_t k;
    const kosu_ayar_t a = ayar_temel(sen_dalis, 0xB4u);
    kosu_kur(&k, &a);
    hata_t m;
    kos(&k, 20.0, 15.0, 20.0, &m);
    TEST_ASSERT_TRUE(m.derinlik < 0.05);
    girdi_kur(&k, &IN);
    IN.plat.link_event = (uint8_t)IDA_LINKEV_SESSION_START;
    TEST_ASSERT_EQUAL(IDA_RC_OK, ida_core_step(&C, &IN, &OUT));
    TEST_ASSERT_EQUAL_UINT8(IDA_EST_INIT, OUT.nav.est_level);
    kos(&k, 25.0, 22.5, 25.0, &m);
    TEST_ASSERT_EQUAL_UINT32(2u, C.kes.n_hizalama);
    TEST_ASSERT_TRUE_MESSAGE(m.derinlik < 0.05, "oturum yenilenince derinlik referansı kayboldu");
}

/* Bulgu 5: kullanılamayan ölçüm (norm kapısı, geç, ölçüm zamanı ilerlemiyor) desteği
   canlı tutmaz. */
static void test_b5_destek_ve_ilerleme(void)
{
    kosu_t k;
    hata_t m;
    /* (a) norm kapısında elenen manyetometre */
    TEST_ASSERT_EQUAL(IDA_RC_OK, ds_kur(&C));
    kosu_ayar_t a = ayar_temel(sen_durgun, 0xB51u);
    a.mag_olcek_bas = 5.0;
    a.mag_olcek_son = 8.0;
    a.mag_olcek = 0.5;
    kosu_kur(&k, &a);
    kos(&k, 5.0, 3.0, 5.0, &m);
    TEST_ASSERT_TRUE((ida_kes_gecerlilik(&C.kes) & IDA_KES_G_YON) != 0u);
    kos(&k, 7.0, 5.0, 7.0, &m);
    TEST_ASSERT_TRUE_MESSAGE((ida_kes_gecerlilik(&C.kes) & IDA_KES_G_YON) == 0u,
                             "norm kapısında elenen manyetometre yönü geçerli tuttu");
    kos(&k, 9.5, 5.0, 9.5, &m);
    TEST_ASSERT_TRUE((ida_kes_gecerlilik(&C.kes) & IDA_KES_G_YON) != 0u);

    /* (b) ufka göre hep geç kalan basınç (t_meas 400 ms geride) */
    TEST_ASSERT_EQUAL(IDA_RC_OK, ds_kur(&C));
    a = ayar_temel(sen_durgun, 0xB52u);
    a.baro_gec = 0.4;
    kosu_kur(&k, &a);
    kos(&k, 2.0, 1.9, 2.0, &m);
    TEST_ASSERT_TRUE_MESSAGE((ida_kes_gecerlilik(&C.kes) & IDA_KES_G_DERINLIK) == 0u,
                             "geç basınç derinliği geçerli tuttu");
    TEST_ASSERT_TRUE(C.kes.n_gec > 10u);

    /* (c) ölçüm zamanı alım zamanının 0,3 katı hızla ilerleyen basınç (A4-K36 (b)) */
    TEST_ASSERT_EQUAL(IDA_RC_OK, ds_kur(&C));
    a = ayar_temel(sen_durgun, 0xB53u);
    a.baro_oran_bas = 3.0;
    a.baro_oran = 0.3;
    kosu_kur(&k, &a);
    kos(&k, 3.0, 2.0, 3.0, &m);
    TEST_ASSERT_TRUE((ida_kes_gecerlilik(&C.kes) & IDA_KES_G_DERINLIK) != 0u);
    kos(&k, 4.5, 3.0, 4.5, &m);
    TEST_ASSERT_TRUE_MESSAGE((ida_kes_gecerlilik(&C.kes) & IDA_KES_G_DERINLIK) == 0u,
                             "ilerlemeyen basınç akışı derinliği geçerli tuttu");
}

/* Bulgu 6: derinlik sıfırlaması aynı çağrıda çıkışa geçer; reset_counter artar. */
static void test_b6_derinlik_sifirlama_cikis(void)
{
    TEST_ASSERT_EQUAL(IDA_RC_OK, ds_kur(&C));
    kosu_t k;
    const kosu_ayar_t a = ayar_temel(sen_durgun, 0xB6u);
    kosu_kur(&k, &a);
    hata_t m;
    kos(&k, 5.0, 3.0, 5.0, &m);
    TEST_ASSERT_EQUAL_UINT8(0u, OUT.nav.reset_counter);
    C.kes.xh.p.z += 3.0f; /* ufukta 3 m derinlik hatası: basınç kapıdan geçemez */
    bool goruldu = false;
    while (!goruldu && (ns_t(k.t_ns) < 10.0)) {
        const uint32_t once = C.kes.n_derinlik_sifir;
        girdi_kur(&k, &IN);
        TEST_ASSERT_EQUAL(IDA_RC_OK, ida_core_step(&C, &IN, &OUT));
        if (C.kes.n_derinlik_sifir != once) {
            goruldu = true;
            TEST_ASSERT_TRUE_MESSAGE(fabs((double)OUT.nav.depth) < 0.05,
                                     "sıfırlama aynı çağrıda çıkışa geçmedi");
            TEST_ASSERT_EQUAL_UINT8_MESSAGE(1u, OUT.nav.reset_counter, "reset_counter artmadı");
        }
    }
    TEST_ASSERT_TRUE(goruldu);
}

/* Bulgu 7: gecikmiş ufkun kovaryansı çıkış zamanına taşınmalı: σp = 0,02 m, σv = 0,1 m/s,
   gecikme L ⇒ σp(çıkış) ≥ √(0,02² + L²·0,1²). */
static void test_b7_cikis_belirsizligi(void)
{
    TEST_ASSERT_EQUAL(IDA_RC_OK, ds_kur(&C));
    kosu_t k;
    const kosu_ayar_t a = ayar_temel(sen_durgun, 0xB7u);
    kosu_kur(&k, &a);
    do {
        girdi_kur(&k, &IN);
        TEST_ASSERT_EQUAL(IDA_RC_OK, ida_core_step(&C, &IN, &OUT));
    } while (OUT.nav.est_level == IDA_EST_INIT);
    const int64_t th = C.kes.th_ns;
    for (int i = 0; i < 25; i++) {
        girdi_kur(&k, &IN);
        TEST_ASSERT_EQUAL(IDA_RC_OK, ida_core_step(&C, &IN, &OUT));
        TEST_ASSERT_TRUE(C.kes.th_ns == th); /* ufuk ilk D boyunca bekler */
        const double L = (double)(OUT.nav.t.ns - th) * 1.0e-9;
        const double alt = sqrt(0.02 * 0.02 + L * L * 0.01);
        TEST_ASSERT_TRUE_MESSAGE((double)OUT.nav.p_sigma[2] >= 0.999 * alt, "çıkış σp iyimser");
        TEST_ASSERT_TRUE((double)OUT.nav.v_sigma[2] >= 0.0999);
    }
}

/* Bulgu 8: 1 rad/s dönüşte IMU 50 ms gecikince nav t_ref'e tam taşınır; 200 ms gecikince
   durum kendi zamanıyla bildirilir ve geçerlilik düşer. */
static void test_b8_gecikmeli_imu_zamani(void)
{
    TEST_ASSERT_EQUAL(IDA_RC_OK, ds_kur(&C));
    kosu_t k;
    kosu_ayar_t a = ayar_temel(sen_donus, 0xB8u);
    a.imu_gec1_bas = 5.0;
    a.imu_gec1 = 0.05;
    a.imu_gec2_bas = 8.0;
    a.imu_gec2 = 0.2;
    kosu_kur(&k, &a);
    uint32_t n1 = 0u, n2 = 0u;
    while (ns_t(k.t_ns) < 12.0) {
        girdi_kur(&k, &IN);
        const ida_rc_t rc = ida_core_step(&C, &IN, &OUT);
        if (rc != IDA_RC_OK) {
            TEST_ASSERT_EQUAL(IDA_RC_IMU_REJECTED, rc); /* gecikme artarken geri giden t_meas */
            continue;
        }
        const double t = ns_t(k.t_ns);
        hata_t h;
        if ((t >= 6.0) && (t < 8.0)) {
            nav_hatasi(&k, &OUT.nav, &h);
            TEST_ASSERT_TRUE(OUT.nav.t.ns == k.t_ns);
            TEST_ASSERT_TRUE_MESSAGE(h.yaw < 0.02, "50 ms gecikmede yön");
            n1++;
        }
        if ((t >= 9.5) && (t < 12.0)) {
            nav_hatasi(&k, &OUT.nav, &h);
            TEST_ASSERT_TRUE_MESSAGE(h.yaw < 0.02, "200 ms gecikmede durum zamanı yanlış");
            TEST_ASSERT_TRUE(OUT.nav.t.ns == IN.imu.hdr.t_meas.ns);
            TEST_ASSERT_EQUAL_HEX8(0u, ida_kes_gecerlilik(&C.kes));
            TEST_ASSERT_TRUE(OUT.nav.est_level != IDA_EST_OK);
            n2++;
        }
    }
    TEST_ASSERT_TRUE(n1 > 300u && n2 > 400u);
}

/* Bulgu 9: hizalama penceresinde [3e38, 1, 1] Pa: birikim taşmamalı, NaN çıkmamalı. */
static void test_b9_hizalama_tasma(void)
{
    TEST_ASSERT_EQUAL(IDA_RC_OK, ds_kur(&C));
    kosu_t k;
    kosu_ayar_t a = ayar_temel(sen_durgun, 0xB9u);
    a.baro_ozel = true;
    kosu_kur(&k, &a);
    hata_t m;
    kos(&k, 5.0, 2.0, 5.0, &m); /* kos her adımda nav'ın sonlu olduğunu denetler */
    TEST_ASSERT_TRUE(m.derinlik < 0.05);
    TEST_ASSERT_EQUAL_UINT32(0u, C.kes.n_cokme);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_init_nav_sifir);
    RUN_TEST(test_k1_durgun);
    RUN_TEST(test_k2_hareketli);
    RUN_TEST(test_k3_manyetik_sicrama);
    RUN_TEST(test_k3_yuksek_ivme);
    RUN_TEST(test_k4_eksik_sensor);
    RUN_TEST(test_k5_belirlenimcilik);
    RUN_TEST(test_oturum_basi_ve_bosluk);
    RUN_TEST(test_sayisal_cokme);
    RUN_TEST(test_ufuk_d_bagimsizligi);
    RUN_TEST(test_b1_cikis_duzeltme_cercevesi);
    RUN_TEST(test_b2_inst_ve_geri_zaman);
    RUN_TEST(test_b3_imu_bayraklari);
    RUN_TEST(test_b4_oturum_derinlikte);
    RUN_TEST(test_b5_destek_ve_ilerleme);
    RUN_TEST(test_b6_derinlik_sifirlama_cikis);
    RUN_TEST(test_b7_cikis_belirsizligi);
    RUN_TEST(test_b8_gecikmeli_imu_zamani);
    RUN_TEST(test_b9_hizalama_tasma);
    return UNITY_END();
}
