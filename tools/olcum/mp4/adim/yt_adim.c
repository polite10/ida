/*
 * W0-MP4a yer tutucu: adım düzenleyici, mesaj çözme/kodlama (CRC-16/MCRF4XX, MAVLink'in crc_accumulate'i),
 * kip/tazelik/sonluluk denetimi ve sözde ölçüm üreteci (ISO C17, float).
 */
#include "yt.h"
#include "yt_ic.h"
#include "yt_mat.h"

#include <math.h>
#include <string.h>

/* ---------------- CRC ---------------- */
uint16_t yt_crc16(const uint8_t *b, uint32_t n, uint16_t crc)
{
    for (uint32_t i = 0u; i < n; i++) {
        uint8_t t = (uint8_t)(b[i] ^ (uint8_t)(crc & 0xFFu));
        t = (uint8_t)(t ^ (uint8_t)(t << 4));
        crc = (uint16_t)((crc >> 8) ^ ((uint16_t)t << 8) ^ ((uint16_t)t << 3) ^ (uint16_t)(t >> 4));
    }
    return crc;
}

static uint16_t mesaj_crc(const uint8_t *b, uint32_t crc_konum)
{
    uint16_t c = yt_crc16(&b[1], crc_konum - 1u, 0xFFFFu);
    const uint8_t ek = YT_CRC_EKSTRA;
    return yt_crc16(&ek, 1u, c);
}

/* ---------------- mesaj çözme ve kabul ---------------- */
int yt_mesaj_coz(yt_t *s, const uint8_t *b)
{
    yt_girdi_t *in = &s->in;
    uint16_t crc_m;
    memcpy(&crc_m, &b[YT_M_CRC], 2);
    if (mesaj_crc(b, YT_M_CRC) != crc_m) { s->crc_hata++; return 0; }
    in->bayrak = b[YT_M_BAYRAK];
    in->seq = b[YT_M_SEQ];
    memcpy(&in->imu.t_ns, &b[YT_M_IMU], 8);
    memcpy(in->imu.w, &b[YT_M_IMU + 8u], 12);
    memcpy(in->imu.a, &b[YT_M_IMU + 20u], 12);
    memcpy(&in->t_bas, &b[YT_M_BASINC], 8);
    memcpy(&in->basinc, &b[YT_M_BASINC + 8u], 4);
    memcpy(&in->t_dvl, &b[YT_M_DVL], 8);
    memcpy(in->dvl_v, &b[YT_M_DVL + 8u], 12);
    memcpy(&in->dvl_fom, &b[YT_M_DVL + 20u], 4);
    memcpy(&in->dvl_irtifa, &b[YT_M_DVL + 24u], 4);
    memcpy(in->dvl_kov, &b[YT_M_DVL + 28u], 24);
    memcpy(&in->t_man, &b[YT_M_MAN], 8);
    memcpy(in->man, &b[YT_M_MAN + 8u], 12);
    memcpy(&in->t_gnss, &b[YT_M_GNSS], 8);
    memcpy(&in->lat_e7, &b[YT_M_GNSS + 8u], 4);
    memcpy(&in->lon_e7, &b[YT_M_GNSS + 12u], 4);
    memcpy(&in->gnss_vn, &b[YT_M_GNSS + 16u], 4);
    memcpy(&in->gnss_ve, &b[YT_M_GNSS + 20u], 4);
    memcpy(&in->gnss_eph, &b[YT_M_GNSS + 24u], 4);
    memcpy(&in->gnss_alt, &b[YT_M_GNSS + 28u], 4);
    in->kip = b[YT_M_KOMUT];
    memcpy(in->sp, &b[YT_M_KOMUT + 4u], 32);
    memcpy(&in->v_bat, &b[YT_M_BATARYA], 4);
    in->sizinti = b[YT_M_BATARYA + 4u];
    memcpy(in->ham, &b[YT_M_HAM], sizeof in->ham);

    /* kabul (02 §4.1 benzeri): sonlu değer ve zaman ilerlemesi */
    yt_ekf_t *e = &s->e;
    if ((in->bayrak & 1u) != 0u && isfinite(in->imu.w[0]) && isfinite(in->imu.a[2])) {
        yt_ekf_imu_ekle(e, &in->imu);
    }
    yt_olcum_t o;
    if ((in->bayrak & 2u) != 0u && isfinite(in->basinc) && in->t_bas > s->t_son[YT_S_BASINC]) {
        memset(&o, 0, sizeof o);
        o.t_ns = in->t_bas; o.z[0] = in->basinc; o.r[0][0] = 0.01f * 0.01f;
        yt_ekf_olcum_ekle(e, YT_S_BASINC, &o);
        s->t_son[YT_S_BASINC] = in->t_bas;
    }
    if ((in->bayrak & 4u) != 0u && in->dvl_fom < 0.1f && in->t_dvl > s->t_son[YT_S_DVL]) {
        memset(&o, 0, sizeof o);
        o.t_ns = in->t_dvl;
        for (int i = 0; i < 3; i++) { o.z[i] = in->dvl_v[i]; }
        o.r[0][0] = in->dvl_kov[0]; o.r[0][1] = o.r[1][0] = in->dvl_kov[1]; o.r[0][2] = o.r[2][0] = in->dvl_kov[2];
        o.r[1][1] = in->dvl_kov[3]; o.r[1][2] = o.r[2][1] = in->dvl_kov[4]; o.r[2][2] = in->dvl_kov[5];
        yt_ekf_olcum_ekle(e, YT_S_DVL, &o);
        s->t_son[YT_S_DVL] = in->t_dvl;
    }
    if ((in->bayrak & 8u) != 0u && in->t_man > s->t_son[YT_S_MANYETO]) {
        memset(&o, 0, sizeof o);
        o.t_ns = in->t_man;
        for (int i = 0; i < 3; i++) { o.z[i] = in->man[i]; o.r[i][i] = 0.01f * 0.01f; }
        yt_ekf_olcum_ekle(e, YT_S_MANYETO, &o);
        s->t_son[YT_S_MANYETO] = in->t_man;
    }
    if ((in->bayrak & 16u) != 0u && in->gnss_eph < 5.0f && in->t_gnss > s->t_son[YT_S_GNSS]) {
        memset(&o, 0, sizeof o);
        o.t_ns = in->t_gnss;
        o.lat_e7 = in->lat_e7; o.lon_e7 = in->lon_e7;
        o.z[0] = in->gnss_vn; o.z[1] = in->gnss_ve;
        const float sp = 0.7f * in->gnss_eph;
        o.r[0][0] = o.r[1][1] = sp * sp;
        o.r[2][2] = o.r[3][3] = 0.1f * 0.1f;
        yt_ekf_olcum_ekle(e, YT_S_GNSS, &o);
        s->t_son[YT_S_GNSS] = in->t_gnss;
    }
    return 1;
}

/* ---------------- çıkış mesajı ---------------- */
void yt_mesaj_kodla(yt_t *s, uint8_t *b)
{
    memset(b, 0, YT_GIDEN_BOY);
    b[0] = 0xFDu;
    b[1] = (uint8_t)(YT_GIDEN_BOY - 4u);
    b[2] = s->giden_seq++;
    b[3] = (uint8_t)(s->kip | (uint8_t)(s->izin << 4));
    memcpy(&b[4], &s->in.imu.t_ns, 8);
    memcpy(&b[12], s->a.pwm, 16);
    memcpy(&b[28], s->a.tau_a, 24);
    memcpy(&b[52], s->e.xo.p, 12);
    memcpy(&b[64], s->e.xo.v, 12);
    memcpy(&b[76], s->e.xo.q, 16);
    const uint32_t durum = ((uint32_t)s->saglik << 24) | (s->e.kapi_red & 0xFFFFFFu);
    memcpy(&b[92], &durum, 4);
    const uint16_t c = mesaj_crc(b, YT_GIDEN_BOY - 2u);
    memcpy(&b[YT_GIDEN_BOY - 2u], &c, 2);
}

/* ---------------- kip, tazelik, sonluluk ---------------- */
static int sonlu_nom(const yt_nom_t *x)
{
    const float *f = (const float *)x;
    int ok = 1;
    for (uint32_t i = 0u; i < sizeof *x / sizeof(float); i++) { ok &= isfinite(f[i]) ? 1 : 0; }
    return ok;
}

static void kip_adim(yt_t *s)
{
    static const int64_t TAZE_NS[YT_S_SAYI] = { 200000000LL, 500000000LL, 300000000LL, 1000000000LL };
    const int64_t simdi = s->in.imu.t_ns;
    uint8_t taze = 0u;
    for (int k = 0; k < YT_S_SAYI; k++) {
        if (simdi - s->t_son[k] <= TAZE_NS[k]) { taze = (uint8_t)(taze | (1u << k)); }
    }
    int sonlu = sonlu_nom(&s->e.xh) & sonlu_nom(&s->e.xo);
    for (int i = 0; i < YT_N; i++) { sonlu &= isfinite(s->e.P[i][i]) ? 1 : 0; }
    if (!sonlu) {
        /* sayısal çökme: filtre yeniden hizalanır (02 §2.3.6) */
        yt_ekf_baslat(&s->e);
        s->e.yeniden_hiza++;
        s->saglik = 3u;
    } else {
        s->saglik = (uint8_t)((taze & 0x5u) == 0x5u ? 0u : 1u);
    }
    /* basit kip makinesi: istenen kip, ön koşulla */
    const uint8_t istenen = (uint8_t)(s->in.kip % 8u);
    s->kip = (s->saglik == 0u || istenen < 3u) ? istenen : 2u;
    s->izin = (uint8_t)((s->kip == 7u && s->saglik == 0u && s->in.sizinti == 0u) ? 1u : 0u);
}

/* ---------------- adım ---------------- */
void yt_yuk_ayarla(yt_t *s, yt_yuk_t yuk)
{
    yt_ayar_t *a = &s->ayar;
    switch (yuk) {
    case YT_YUK_EN_KOTU:
        a->her_adim_dis_dongu = 1u; a->her_adim_ldl = 1u; a->fuzyon_oncesi_kov = 1u; a->adim_sonu_kov = 1u;
        a->skaler_sinir = 0u;
        break;
    case YT_YUK_02:
        a->her_adim_dis_dongu = 1u; a->her_adim_ldl = 1u; a->fuzyon_oncesi_kov = 1u; a->adim_sonu_kov = 0u;
        a->skaler_sinir = 8u;
        break;
    default:
        a->her_adim_dis_dongu = 0u; a->her_adim_ldl = 0u; a->fuzyon_oncesi_kov = 1u; a->adim_sonu_kov = 1u;
        a->skaler_sinir = 0u;
        break;
    }
}

void yt_baslat(yt_t *s, yt_yuk_t yuk)
{
    memset(s, 0, sizeof *s);
    yt_yuk_ayarla(s, yuk);
    yt_ekf_baslat(&s->e);
    yt_gudum_baslat(&s->g);
    yt_kontrol_baslat(&s->c);
    yt_tahsis_baslat(&s->a);
}

void yt_adim(yt_t *s, const uint8_t *gelen, uint8_t *giden)
{
    for (int p = 0; p < YT_P_SAYI; p++) { s->parca_cyc[p] = 0u; }
    yt_parca_bas(s);
    (void)yt_mesaj_coz(s, gelen);
    yt_parca(s, YT_P_MESAJ_GIRIS);
    yt_ekf_adim(s);
    kip_adim(s);
    yt_parca(s, YT_P_KIP);
    yt_gudum_adim(s);
    yt_parca(s, YT_P_GUDUM);
    yt_kontrol_adim(s);
    yt_parca(s, YT_P_KONTROL);
    yt_tahsis_adim(s);
    yt_parca(s, YT_P_TAHSIS);
    yt_mesaj_kodla(s, giden);
    yt_parca(s, YT_P_MESAJ_CIKIS);
    s->n_adim++;
}

/* ---------------- sözde ölçüm üreteci (ölçülen bölgenin dışında) ---------------- */
static uint32_t rs = 0x12345678u;
static float rnd(void)   /* [-1, 1) */
{
    rs ^= rs << 13; rs ^= rs >> 17; rs ^= rs << 5;
    return (float)(int32_t)rs * (1.0f / 2147483648.0f);
}

static int var_mi(uint32_t n, uint32_t hz) { return ((n + 1u) * hz / 400u) != (n * hz / 400u); }

void yt_uret(const yt_t *s, yt_yuk_t yuk, uint32_t n, uint8_t *b)
{
    static const float M_N[3] = { 0.20f, 0.01f, 0.45f };
    const int64_t t = (int64_t)(n + 1u) * YT_PERIYOT_NS;
    const float ts = (float)(n % 400000u) * 0.0025f;
    const yt_nom_t *x = &s->e.xo;
    float R[3][3], gb[3], w[3], a[3], v[3];
    const float gn[3] = { 0.0f, 0.0f, -9.80665f };
    yt_q_rot(x->q, R);
    yt_m3tv(R, gn, gb);
    w[0] = 0.05f * sinf(0.5f * ts) + 0.002f * rnd();
    w[1] = 0.04f * cosf(0.3f * ts) + 0.002f * rnd();
    w[2] = 0.10f * sinf(0.1f * ts) + 0.002f * rnd();
    a[0] = gb[0] + 0.05f * sinf(0.2f * ts) + 0.02f * rnd();
    a[1] = gb[1] + 0.03f * cosf(0.15f * ts) + 0.02f * rnd();
    a[2] = gb[2] + 0.02f * rnd();

    uint8_t bayrak = 1u;
    int bas, dvl, man, gnss;
    if (yuk == YT_YUK_OLAGAN) {
        bas = var_mi(n, 25u); dvl = var_mi(n, 15u); man = var_mi(n, 10u); gnss = var_mi(n, 5u);
    } else if (yuk == YT_YUK_EN_KOTU) {
        bas = dvl = man = gnss = 1;
    } else {
        bas = dvl = gnss = 1; man = 0;
    }
    /* gecikmeler: A ve B'de farklı alt-periyot konumları (ayrı füzyon zamanları), C'de ortak 50 ms */
    const int64_t gec_bas = (yuk == YT_YUK_02) ? 50000000LL : 20300000LL;
    const int64_t gec_dvl = (yuk == YT_YUK_02) ? 50000000LL : 50700000LL;
    const int64_t gec_man = 10900000LL;
    const int64_t gec_gnss = (yuk == YT_YUK_02) ? 50000000LL : 120100000LL;

    memset(b, 0, YT_GELEN_BOY);
    b[0] = 0xFDu;
    b[1] = (uint8_t)(YT_GELEN_BOY - 4u);
    b[YT_M_SEQ] = (uint8_t)n;
    memcpy(&b[YT_M_IMU], &t, 8);
    memcpy(&b[YT_M_IMU + 8u], w, 12);
    memcpy(&b[YT_M_IMU + 20u], a, 12);
    if (bas) {
        bayrak |= 2u;
        const int64_t tm = t - gec_bas;
        const float P = 101325.0f + 9806.65f * (x->p[2] + 0.05f) + 5.0f * rnd();
        memcpy(&b[YT_M_BASINC], &tm, 8);
        memcpy(&b[YT_M_BASINC + 8u], &P, 4);
    }
    if (dvl) {
        bayrak |= 4u;
        const int64_t tm = t - gec_dvl;
        yt_m3tv(R, x->v, v);
        for (int i = 0; i < 3; i++) { v[i] += 0.01f * rnd(); }
        const float fom = 0.01f, irt = 5.0f + 0.5f * sinf(0.05f * ts);
        const float kov[6] = { 1e-4f, 1e-5f, 0.0f, 1e-4f, 1e-5f, 2e-4f };
        memcpy(&b[YT_M_DVL], &tm, 8);
        memcpy(&b[YT_M_DVL + 8u], v, 12);
        memcpy(&b[YT_M_DVL + 20u], &fom, 4);
        memcpy(&b[YT_M_DVL + 24u], &irt, 4);
        memcpy(&b[YT_M_DVL + 28u], kov, 24);
    }
    if (man) {
        bayrak |= 8u;
        const int64_t tm = t - gec_man;
        float m[3];
        yt_m3tv(R, M_N, m);
        for (int i = 0; i < 3; i++) { m[i] += 0.005f * rnd(); }
        memcpy(&b[YT_M_MAN], &tm, 8);
        memcpy(&b[YT_M_MAN + 8u], m, 12);
    }
    if (gnss) {
        bayrak |= 16u;
        const int64_t tm = t - gec_gnss;
        const int32_t lat = YT_ORIJIN_LAT_E7 + (int32_t)((x->p[0] + 0.5f * rnd()) / YT_E7_METRE);
        const int32_t lon = YT_ORIJIN_LON_E7 + (int32_t)((x->p[1] + 0.5f * rnd()) / (YT_E7_METRE * YT_COS_LAT0));
        const float vn = x->v[0] + 0.05f * rnd(), ve = x->v[1] + 0.05f * rnd(), eph = 1.5f, alt = 0.3f;
        memcpy(&b[YT_M_GNSS], &tm, 8);
        memcpy(&b[YT_M_GNSS + 8u], &lat, 4);
        memcpy(&b[YT_M_GNSS + 12u], &lon, 4);
        memcpy(&b[YT_M_GNSS + 16u], &vn, 4);
        memcpy(&b[YT_M_GNSS + 20u], &ve, 4);
        memcpy(&b[YT_M_GNSS + 24u], &eph, 4);
        memcpy(&b[YT_M_GNSS + 28u], &alt, 4);
    }
    b[YT_M_BAYRAK] = bayrak;
    b[YT_M_KOMUT] = 7u;
    for (uint32_t i = 0u; i < 8u; i++) {
        const float sp = rnd();
        memcpy(&b[YT_M_KOMUT + 4u + 4u * i], &sp, 4);
    }
    const float vb = 15.8f + 0.2f * sinf(0.01f * ts);
    memcpy(&b[YT_M_BATARYA], &vb, 4);
    for (uint32_t i = 0u; i < 98u; i++) { b[YT_M_HAM + i] = (uint8_t)(rs >> 24); (void)rnd(); }
    const uint16_t c = mesaj_crc(b, YT_M_CRC);
    memcpy(&b[YT_M_CRC], &c, 2);
}
