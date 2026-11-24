/*
 * gorev_duzenek.h — görev yürütücüsü testlerinin ortak düzeneği (W5-gorev-yurutucu).
 *
 * Görev modülü saf iç işlevleriyle (gorev_ic.h) yapay girdiyle sürülür (test_gorev_kip_ariza.c
 * ile aynı yöntem): kestirim düzeyi, bileşen geçerliliği, konum, derinlik, yön, GNSS tazeliği
 * ve saatler testte kurulur. Komutlar adımdan sonra işlenir (cerceve.c sırası). Ölçüm zamanı
 * varsayılan olarak duvar saatiyle birlikte ilerler (gd_t.olcum_dondur ile durdurulur).
 */
#ifndef IDA_TEST_GOREV_DUZENEK_H
#define IDA_TEST_GOREV_DUZENEK_H

#include <string.h>

#include "cekirdek.h"
#include "gorev/gorev_ic.h"
#include "unity.h"

#define GD_MS 1000000LL

typedef struct {
    ida_gor_t g;
    ida_emn_t e;
    ida_param_t p;
    ida_gunluk_t gun;
    ida_gor_girdi_t gi;
    int64_t now;
    int64_t olcum;       /* ölçüm zamanı (gi.t_olcum_ns)        */
    bool olcum_dondur;   /* true: ölçüm zamanı ilerlemez         */
    bool besle_hb;
    bool besle_batt;
    bool besle_leak;
    float batt_v;
    uint32_t seq;
} gd_t;

static inline uint8_t gd_gecerlilik_tam(void)
{
    return (uint8_t)(IDA_KES_G_TUTUM | IDA_KES_G_YON | IDA_KES_G_DERINLIK | IDA_KES_G_DIKEY_HIZ |
                     IDA_KES_G_YATAY_HIZ | IDA_KES_G_KONUM);
}

static inline ida_cmd_result_t gd_komut(gd_t *o, const ida_cmd_t *c0)
{
    ida_komut_baglam_t b;
    (void)memset(&b, 0, sizeof(b));
    b.now_w.ns = o->now;
    b.oturum_var = true;
    b.armed = o->e.armed;
    b.mode = o->e.mode;
    b.est_level = o->gi.est_level;
    b.daraldi = o->e.d.herhangi;
    b.events_ok = true;
    b.param_ok = true;
    ida_cmd_t c = *c0;
    o->seq++;
    c.cmd_id = o->seq;
    ida_cmd_result_t r;
    (void)memset(&r, 0, sizeof(r));
    r.cmd_id = c.cmd_id;
    r.kind = (uint8_t)IDA_RES_ACK;
    ida_gor_komut_ic(&o->g, &o->e, &b, &c, &r, &o->p, &o->gun);
    return r;
}

/* Yüksüz ya da tek baytlık yüklü komut (SET_MODE, MISSION_CTL, MISSION_BEGIN). */
static inline ida_cmd_result_t gd_basit(gd_t *o, uint8_t tur, uint16_t arg)
{
    ida_cmd_t c;
    (void)memset(&c, 0, sizeof(c));
    c.type = tur;
    if (tur == (uint8_t)IDA_CMD_SET_MODE) {
        c.u.set_mode.mode = (uint8_t)arg;
    } else if (tur == (uint8_t)IDA_CMD_MISSION_CTL) {
        c.u.mission_ctl.action = (uint8_t)arg;
    } else if (tur == (uint8_t)IDA_CMD_MISSION_BEGIN) {
        c.u.mission_begin.count = arg;
    } else {
        /* yüksüz */
    }
    return gd_komut(o, &c);
}

static inline ida_cmd_result_t gd_oge_gonder(gd_t *o, const ida_mission_item_t *m)
{
    ida_cmd_t c;
    (void)memset(&c, 0, sizeof(c));
    c.type = (uint8_t)IDA_CMD_MISSION_ITEM;
    c.u.mission_item = *m;
    return gd_komut(o, &c);
}

static inline ida_mission_item_t gd_oge(uint8_t tur, uint8_t cerceve, float p0, float p1, float n,
                                        float e, float d)
{
    ida_mission_item_t m;
    (void)memset(&m, 0, sizeof(m));
    m.item_type = tur;
    m.frame = cerceve;
    m.p[0] = p0;
    m.p[1] = p1;
    m.north_m = n;
    m.east_m = e;
    m.depth = d;
    return m;
}

static inline void gd_kabul_bekle(ida_cmd_result_t r)
{
    TEST_ASSERT_EQUAL_UINT8(IDA_ACK_ACCEPTED, r.ack);
    TEST_ASSERT_EQUAL_UINT16(IDA_REASON_NONE, r.reason);
}

static inline void gd_ret_bekle(ida_cmd_result_t r, uint16_t neden)
{
    TEST_ASSERT_EQUAL_UINT8(IDA_ACK_REJECTED, r.ack);
    TEST_ASSERT_EQUAL_UINT16(neden, r.reason);
}

/* Bütün yükleme dizisi; her ileti ACCEPTED beklenir. */
static inline void gd_yukle(gd_t *o, const ida_mission_item_t *ogeler, uint16_t n)
{
    gd_kabul_bekle(gd_basit(o, (uint8_t)IDA_CMD_MISSION_BEGIN, n));
    for (uint16_t i = 0u; i < n; i++) {
        ida_mission_item_t m = ogeler[i];
        m.index = i;
        gd_kabul_bekle(gd_oge_gonder(o, &m));
    }
    gd_kabul_bekle(gd_basit(o, (uint8_t)IDA_CMD_MISSION_END, 0u));
    TEST_ASSERT_EQUAL_UINT8(IDA_MS_LOADED, o->g.mission_state);
}

static inline void gd_param_f32(gd_t *o, enum ida_prm_indis i, float v)
{
    (void)memcpy(&o->p.yururluk.ham[i], &v, sizeof(v));
}

static inline void gd_param_u32(gd_t *o, enum ida_prm_indis i, uint32_t v)
{
    o->p.yururluk.ham[i] = v;
}

/* Bir adım: örnekler adımdan önce, HEARTBEAT adımdan sonra (cerceve.c). */
static inline void gd_adim(gd_t *o)
{
    if (o->besle_batt) {
        ida_sample_t s;
        (void)memset(&s, 0, sizeof(s));
        s.hdr.type = (uint8_t)IDA_SENS_BATT;
        s.hdr.t_rx.ns = o->now;
        s.u.batt.voltage = o->batt_v;
        ida_gor_ornek(&o->g, &s);
    }
    if (o->besle_leak) {
        ida_sample_t s;
        (void)memset(&s, 0, sizeof(s));
        s.hdr.type = (uint8_t)IDA_SENS_LEAK;
        s.hdr.t_rx.ns = o->now;
        ida_gor_ornek(&o->g, &s);
    }
    o->gi.now_ns = o->now;
    o->gi.t_olcum_ns = o->olcum;
    (void)memset(&o->e.d, 0, sizeof(o->e.d));
    ida_gor_adim_ic(&o->g, &o->e, &o->gi, &o->gun);
    ida_gun_kur(&o->gun); /* günlük taşmasın */
    if (o->besle_hb) {
        (void)gd_basit(o, (uint8_t)IDA_CMD_HEARTBEAT, 0u);
    }
}

static inline void gd_ilerle(gd_t *o, int64_t ms)
{
    const int64_t son = o->now + (ms * GD_MS);
    while (o->now < son) {
        o->now += 100 * GD_MS;
        if (!o->olcum_dondur) {
            o->olcum += 100 * GD_MS;
        }
        gd_adim(o);
    }
}

/* Sağlıklı ortam: kestirim OK, bütün bileşenler geçerli, yüzeyde (0,1 m), konum (0, 0), yön 0,
   GNSS taze, batarya 16 V, sızıntı kuru, HEARTBEAT; kip DISARMED. */
static inline void gd_kur(gd_t *o)
{
    (void)memset(o, 0, sizeof(*o));
    ida_gor_kur(&o->g);
    ida_emn_kur(&o->e);
    ida_prm_varsayilan(&o->p.yururluk);
    o->p.param_ok = true;
    ida_gun_kur(&o->gun);
    o->now = 10000 * GD_MS;
    o->olcum = 5000 * GD_MS;
    o->gi.est_level = (uint8_t)IDA_EST_OK;
    o->gi.suzgec_duzey = (uint8_t)IDA_EST_OK;
    o->gi.gecerlilik = gd_gecerlilik_tam();
    o->gi.param_ok = true;
    o->gi.nav_derinlik = 0.1f;
    o->gi.nav_yon = 0.0f;
    o->gi.gnss_taze = true;
    o->gi.orijin_lat_e7 = 410000000;
    o->gi.orijin_lon_e7 = 290000000;
    o->gi.prm = &o->p;
    o->besle_batt = true;
    o->besle_leak = true;
    o->besle_hb = true;
    o->batt_v = 16.0f;
    o->e.kisitli = 0u;
    gd_ilerle(o, 500);
    TEST_ASSERT_EQUAL_UINT8(IDA_MODE_DISARMED, o->e.mode);
}

/* Silahlanır ve yüklü görevle AUTO'ya girer (görev RUNNING). */
static inline void gd_auto(gd_t *o)
{
    gd_kabul_bekle(gd_basit(o, (uint8_t)IDA_CMD_ARM, 0u));
    gd_kabul_bekle(gd_basit(o, (uint8_t)IDA_CMD_SET_MODE, (uint16_t)IDA_MODE_AUTO));
    TEST_ASSERT_EQUAL_UINT8(IDA_MODE_AUTO, o->e.mode);
    TEST_ASSERT_EQUAL_UINT8(IDA_MS_RUNNING, o->g.mission_state);
}

static inline ida_status_t gd_durum(const gd_t *o)
{
    ida_status_t st;
    (void)memset(&st, 0, sizeof(st));
    ida_gor_durum(&o->g, &st);
    return st;
}

#endif /* IDA_TEST_GOREV_DUZENEK_H */
