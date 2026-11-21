/*
 * ornek.c — örnek kabul kapısı (W2; A4-K14, K30, K33, K34, K35, K40, K103, K115, K130).
 */
#include "cerceve/ornek.h"

#include <string.h>

#include "mat/ida_math.h"
#include "zaman/zaman.h"

typedef enum {
    ORN_KABUL,
    ORN_RED,     /* status.cnt.sample_rejected */
    ORN_GELECEK  /* status.cnt.sample_future   */
} orn_karar_t;

typedef union {
    float f;
    uint32_t u;
} bit32_t;

static uint32_t f_bit(float f)
{
    bit32_t b;
    b.f = f;
    return b.u;
}

static bool sonlu_dizi(const float *d, uint32_t n)
{
    for (uint32_t i = 0u; i < n; i++) {
        if (!ida_sonlu(d[i])) {
            return false;
        }
    }
    return true;
}

static bool esit_dizi(const float *lhs, const float *rhs, uint32_t n)
{
    for (uint32_t i = 0u; i < n; i++) {
        if (f_bit(lhs[i]) != f_bit(rhs[i])) {
            return false;
        }
    }
    return true;
}

void ida_orn_kur(ida_ornek_t *o)
{
    (void)memset(o, 0, sizeof(*o));
}

/* A4-K30: samples[]'te kabul edilen türler (IMU ve Ping360 kendi alanlarındadır). */
static bool ornek_turu_gecerli(uint8_t type)
{
    switch (type) {
    case IDA_SENS_MAG:
    case IDA_SENS_BARO:
    case IDA_SENS_GNSS:
    case IDA_SENS_DVL:
    case IDA_SENS_PING1D:
    case IDA_SENS_BATT:
    case IDA_SENS_LEAK:
    case IDA_SENS_SAFETY:
        return true;
    default:
        return false;
    }
}

/* 3.2: IMU, basınç ve batarya aralık ölçümüdür (window_ns > 0); diğerleri anlık (0). */
static bool pencere_dogru(const ida_hdr_t *h)
{
    const bool aralik = (h->type == (uint8_t)IDA_SENS_IMU) || (h->type == (uint8_t)IDA_SENS_BARO) ||
                        (h->type == (uint8_t)IDA_SENS_BATT);
    return aralik ? (h->window_ns > 0u) : (h->window_ns == 0u);
}

/* Gövdenin bütün float alanları sonlu mu (A4-K33); type geçerli olmalıdır. */
static bool govde_sonlu(const ida_sample_t *s)
{
    switch (s->hdr.type) {
    case IDA_SENS_MAG:
        return sonlu_dizi(s->u.mag.field, 3u);
    case IDA_SENS_BARO:
        return ida_sonlu(s->u.baro.p_abs) && ida_sonlu(s->u.baro.temp_c);
    case IDA_SENS_GNSS:
        return sonlu_dizi(s->u.gnss.vel_ned, 3u) && ida_sonlu(s->u.gnss.eph) &&
               ida_sonlu(s->u.gnss.epv);
    case IDA_SENS_DVL:
        return sonlu_dizi(s->u.dvl.vel, 3u) && sonlu_dizi(s->u.dvl.cov, 6u) &&
               ida_sonlu(s->u.dvl.fom) && sonlu_dizi(s->u.dvl.beam_dist, 4u) &&
               ida_sonlu(s->u.dvl.altitude);
    case IDA_SENS_PING1D:
        return ida_sonlu(s->u.ping1d.distance) && ida_sonlu(s->u.ping1d.scan_start) &&
               ida_sonlu(s->u.ping1d.scan_length);
    case IDA_SENS_BATT:
        return ida_sonlu(s->u.batt.voltage) && ida_sonlu(s->u.batt.current) &&
               ida_sonlu(s->u.batt.temp_c);
    default:
        return true; /* LEAK, SAFETY: float alan yok */
    }
}

/* SAFETY değerleri tanımlı mı (A4-K33, K116): estop 0/1, tüp bitleri IDA_TUBE_* içinde. */
static bool emniyet_degerleri_gecerli(const ida_safety_t *s)
{
    const uint8_t tanimli = (uint8_t)(IDA_TUBE_LOADED | IDA_TUBE_PLUG_IN | IDA_TUBE_PIN_OUT |
                                      IDA_TUBE_PERMIT_OPEN);
    return (s->estop <= 1u) && ((s->tube[0] & (uint8_t)~tanimli) == 0u) &&
           ((s->tube[1] & (uint8_t)~tanimli) == 0u);
}

typedef struct {
    const ida_hdr_t *hdr;
    bool govde_sonlu;
    bool emniyet;            /* IDA_SENS_SAFETY: eşit t_meas kuralı (A4-K34)          */
    bool emniyet_degerleri;  /* SAFETY değerleri tanımlı                              */
} orn_aday_t;

/* A4-K33 koşulları listedeki sırayla; ilk tutmayan kararı verir. Kabulde izi günceller. */
static orn_karar_t tek_denetle(ida_ornek_t *o, const orn_aday_t *a, const ida_orn_baglam_t *b)
{
    const ida_hdr_t *h = a->hdr;
    if ((h->flags & IDA_HF_VALID) == 0u) {
        return ORN_RED;
    }
    if (!a->govde_sonlu || !pencere_dogru(h)) {
        return ORN_RED;
    }
    if (!ida_st_gecerli(h->t_meas) || !ida_wt_gecerli(h->t_rx)) {
        return ORN_RED;
    }
    if (h->t_rx.ns > b->now.ns) {
        return ORN_GELECEK;
    }
    if (b->tf->present && (h->t_meas.ns > b->tf->t.ns)) {
        return ORN_GELECEK;
    }
    ida_orn_iz_t *iz = &o->iz[h->type][h->inst];
    bool tazelik = true;
    if (iz->var) {
        if (ida_seri_karsilastir(h->seq, iz->seq) != IDA_SERI_YENI) {
            return ORN_RED;
        }
        if (h->t_meas.ns < iz->t_meas_ns) {
            return ORN_RED;
        }
        if (h->t_meas.ns == iz->t_meas_ns) {
            if (!a->emniyet) {
                return ORN_RED;
            }
            tazelik = false; /* A4-K34: değer güncellenir, tazelik yenilenmez */
        }
    }
    if (a->emniyet && !a->emniyet_degerleri) {
        return ORN_RED;
    }
    iz->var = true;
    iz->seq = h->seq;
    iz->t_meas_ns = h->t_meas.ns;
    if (tazelik) {
        iz->t_rx_ns = h->t_rx.ns;
    }
    return ORN_KABUL;
}

static void karar_say(orn_karar_t k, ida_core_counters_t *cnt)
{
    if (k == ORN_RED) {
        cnt->sample_rejected++;
    } else if (k == ORN_GELECEK) {
        cnt->sample_future++;
    } else {
        /* kabul: sayaç yok */
    }
}

bool ida_orn_imu(ida_ornek_t *o, const ida_imu_sample_t *imu, const ida_orn_baglam_t *b)
{
    if ((imu->hdr.type != (uint8_t)IDA_SENS_IMU) || (imu->hdr.inst >= IDA_PORT_MAX_INST)) {
        b->cnt->sample_rejected++; /* A4-K30; gövde okunmaz */
        return false;
    }
    const orn_aday_t a = {
        .hdr = &imu->hdr,
        .govde_sonlu = sonlu_dizi(imu->imu.gyro, 3u) && sonlu_dizi(imu->imu.accel, 3u) &&
                       ida_sonlu(imu->imu.temp_c),
        .emniyet = false,
        .emniyet_degerleri = true,
    };
    const orn_karar_t k = tek_denetle(o, &a, b);
    karar_say(k, b->cnt);
    return k == ORN_KABUL;
}

/* A4-K35 anahtarı: t_meas, type, inst, seq. */
static int anahtar_karsilastir(const ida_hdr_t *lhs, const ida_hdr_t *rhs)
{
    if (lhs->t_meas.ns != rhs->t_meas.ns) {
        return (lhs->t_meas.ns < rhs->t_meas.ns) ? -1 : 1;
    }
    if (lhs->type != rhs->type) {
        return (lhs->type < rhs->type) ? -1 : 1;
    }
    if (lhs->inst != rhs->inst) {
        return (lhs->inst < rhs->inst) ? -1 : 1;
    }
    if (lhs->seq != rhs->seq) {
        return (lhs->seq < rhs->seq) ? -1 : 1;
    }
    return 0;
}

/* "Bayt bayt aynı" (A4-K115): başlığın bütün alanları ve etkin gövde üyesi (dolgu baytları
   anlamsız olduğu için alan alan; float'lar bit deseniyle). */
static bool govde_esit(const ida_sample_t *lhs, const ida_sample_t *rhs)
{
    switch (lhs->hdr.type) {
    case IDA_SENS_MAG:
        return esit_dizi(lhs->u.mag.field, rhs->u.mag.field, 3u);
    case IDA_SENS_BARO:
        return (f_bit(lhs->u.baro.p_abs) == f_bit(rhs->u.baro.p_abs)) &&
               (f_bit(lhs->u.baro.temp_c) == f_bit(rhs->u.baro.temp_c));
    case IDA_SENS_GNSS:
        return (lhs->u.gnss.lat_e7 == rhs->u.gnss.lat_e7) &&
               (lhs->u.gnss.lon_e7 == rhs->u.gnss.lon_e7) &&
               (lhs->u.gnss.alt_mm == rhs->u.gnss.alt_mm) &&
               esit_dizi(lhs->u.gnss.vel_ned, rhs->u.gnss.vel_ned, 3u) &&
               (f_bit(lhs->u.gnss.eph) == f_bit(rhs->u.gnss.eph)) &&
               (f_bit(lhs->u.gnss.epv) == f_bit(rhs->u.gnss.epv)) &&
               (lhs->u.gnss.fix_type == rhs->u.gnss.fix_type) &&
               (lhs->u.gnss.n_sv == rhs->u.gnss.n_sv);
    case IDA_SENS_DVL:
        return esit_dizi(lhs->u.dvl.vel, rhs->u.dvl.vel, 3u) &&
               esit_dizi(lhs->u.dvl.cov, rhs->u.dvl.cov, 6u) &&
               (f_bit(lhs->u.dvl.fom) == f_bit(rhs->u.dvl.fom)) &&
               esit_dizi(lhs->u.dvl.beam_dist, rhs->u.dvl.beam_dist, 4u) &&
               (f_bit(lhs->u.dvl.altitude) == f_bit(rhs->u.dvl.altitude)) &&
               (lhs->u.dvl.dvl_flags == rhs->u.dvl.dvl_flags) &&
               (lhs->u.dvl.beam_valid == rhs->u.dvl.beam_valid);
    case IDA_SENS_PING1D:
        return (f_bit(lhs->u.ping1d.distance) == f_bit(rhs->u.ping1d.distance)) &&
               (f_bit(lhs->u.ping1d.scan_start) == f_bit(rhs->u.ping1d.scan_start)) &&
               (f_bit(lhs->u.ping1d.scan_length) == f_bit(rhs->u.ping1d.scan_length)) &&
               (lhs->u.ping1d.ping_number == rhs->u.ping1d.ping_number) &&
               (lhs->u.ping1d.tx_offset_ns == rhs->u.ping1d.tx_offset_ns) &&
               (lhs->u.ping1d.confidence == rhs->u.ping1d.confidence);
    case IDA_SENS_BATT:
        return (f_bit(lhs->u.batt.voltage) == f_bit(rhs->u.batt.voltage)) &&
               (f_bit(lhs->u.batt.current) == f_bit(rhs->u.batt.current)) &&
               (f_bit(lhs->u.batt.temp_c) == f_bit(rhs->u.batt.temp_c));
    case IDA_SENS_LEAK:
        return lhs->u.leak.mask == rhs->u.leak.mask;
    default: /* IDA_SENS_SAFETY */
        return (lhs->u.safety.estop == rhs->u.safety.estop) &&
               (lhs->u.safety.tube[0] == rhs->u.safety.tube[0]) &&
               (lhs->u.safety.tube[1] == rhs->u.safety.tube[1]);
    }
}

static bool ornek_esit(const ida_sample_t *lhs, const ida_sample_t *rhs)
{
    return (anahtar_karsilastir(&lhs->hdr, &rhs->hdr) == 0) &&
           (lhs->hdr.t_rx.ns == rhs->hdr.t_rx.ns) && (lhs->hdr.window_ns == rhs->hdr.window_ns) &&
           (lhs->hdr.flags == rhs->hdr.flags) && govde_esit(lhs, rhs);
}

static orn_aday_t aday_kur(const ida_sample_t *s)
{
    const bool emn = (s->hdr.type == (uint8_t)IDA_SENS_SAFETY);
    const orn_aday_t a = {
        .hdr = &s->hdr,
        .govde_sonlu = govde_sonlu(s),
        .emniyet = emn,
        .emniyet_degerleri = emn ? emniyet_degerleri_gecerli(&s->u.safety) : true,
    };
    return a;
}

static void kabul_ekle(ida_orn_sonuc_t *s, uint8_t indis)
{
    if (s->n < IDA_PORT_MAX_SAMPLES) { /* en çok min(n_samples, 16) kabul; sınır açık */
        s->sira[s->n] = indis;
        s->n++;
    }
}

/* Aynı anahtarlı bir koşunun (sırada ardışık) işlenmesi (A4-K115, K130). */
static void kosu_isle(ida_ornek_t *o, const ida_in_t *in, const uint8_t *kosu, uint32_t n,
                      const ida_orn_baglam_t *b, ida_orn_sonuc_t *s)
{
    /* VALID = 0 örnekler eşit anahtar karşılaştırmasına girmez; tek tek reddedilir. */
    uint8_t grup[IDA_PORT_MAX_SAMPLES] = {0u};
    uint32_t g = 0u;
    for (uint32_t i = 0u; i < n; i++) {
        const ida_sample_t *x = &in->samples[kosu[i]];
        if ((x->hdr.flags & IDA_HF_VALID) == 0u) {
            b->cnt->sample_rejected++;
        } else {
            grup[g] = kosu[i];
            g++;
        }
    }
    if (g == 0u) {
        return;
    }
    bool hepsi_ayni = true;
    for (uint32_t i = 1u; i < g; i++) {
        if (!ornek_esit(&in->samples[grup[0]], &in->samples[grup[i]])) {
            hepsi_ayni = false;
        }
    }
    if (!hepsi_ayni) {
        b->cnt->sample_rejected += g;
        b->cnt->sample_conflict++;
        s->cakisma = true;
        const ida_hdr_t *h = &in->samples[grup[0]].hdr;
        const ida_log_t k = {.t_w = b->now,
                             .code = (uint16_t)IDA_LOG_SAMPLE_CONFLICT,
                             .arg0 = (int32_t)h->type,
                             .arg1 = (int32_t)h->inst};
        ida_gun_ekle(b->gun, &k);
        return;
    }
    b->cnt->sample_dup_dropped += g - 1u;
    const orn_aday_t a = aday_kur(&in->samples[grup[0]]);
    const orn_karar_t karar = tek_denetle(o, &a, b);
    karar_say(karar, b->cnt);
    if (karar == ORN_KABUL) {
        kabul_ekle(s, grup[0]);
    }
}

void ida_orn_degerlendir(ida_ornek_t *o, const ida_in_t *in, const ida_orn_baglam_t *b,
                         ida_orn_sonuc_t *s)
{
    s->n = 0u;
    s->cakisma = false;
    const uint32_t n = (in->n_samples < IDA_PORT_MAX_SAMPLES) ? (uint32_t)in->n_samples
                                                              : IDA_PORT_MAX_SAMPLES;
    /* A4-K30: tür ya da inst geçersiz örnekler reddedilir; gövdeleri okunmaz. */
    uint8_t sira[IDA_PORT_MAX_SAMPLES] = {0u};
    uint32_t m = 0u;
    for (uint32_t i = 0u; i < n; i++) {
        const ida_hdr_t *h = &in->samples[i].hdr;
        if (!ornek_turu_gecerli(h->type) || (h->inst >= IDA_PORT_MAX_INST)) {
            b->cnt->sample_rejected++;
        } else {
            sira[m] = (uint8_t)i;
            m++;
        }
    }
    /* A4-K35: anahtara göre kararlı ekleme sıralaması. */
    for (uint32_t i = 1u; i < m; i++) {
        const uint8_t x = sira[i];
        uint32_t j = i;
        while ((j > 0u) &&
               (anahtar_karsilastir(&in->samples[sira[j - 1u]].hdr, &in->samples[x].hdr) > 0)) {
            sira[j] = sira[j - 1u];
            j--;
        }
        sira[j] = x;
    }
    /* Eşit anahtarlı koşular. */
    uint32_t i = 0u;
    while (i < m) {
        uint32_t j = i + 1u;
        while ((j < m) && (anahtar_karsilastir(&in->samples[sira[i]].hdr,
                                               &in->samples[sira[j]].hdr) == 0)) {
            j++;
        }
        kosu_isle(o, in, &sira[i], j - i, b, s);
        i = j;
    }
}

bool ida_orn_ping360(ida_ornek_t *o, const ida_ping360_t *p, const ida_orn_baglam_t *b)
{
    bool gecerli = (p->hdr.type == (uint8_t)IDA_SENS_PING360) && (p->hdr.inst < IDA_PORT_MAX_INST);
    if (gecerli) {
        gecerli = (p->n_samples >= 1u) && (p->n_samples <= IDA_PING360_MAX_SAMPLES) &&
                  (p->angle_grad < IDA_PING360_ANGLE_COUNT) && (p->sample_period >= 1u);
    }
    if (gecerli) {
        const orn_aday_t a = {
            .hdr = &p->hdr, .govde_sonlu = true, .emniyet = false, .emniyet_degerleri = true};
        gecerli = (tek_denetle(o, &a, b) == ORN_KABUL);
    }
    if (!gecerli) {
        b->cnt->ping360_rejected++;
    }
    return gecerli;
}
