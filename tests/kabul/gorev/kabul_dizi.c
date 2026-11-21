/*
 * tests/kabul/gorev — görev md.4: komut ve arıza dizilerinde değişmezler.
 *
 *   (a) emniyet kısıtı varken armed hiçbir adımda doğru olmaz. W3-K2'ye göre emniyet kısıtı
 *       çağrıda uygulanan daraltmadır (A4-K44): daraltmalı çağrıda armed 0 → 1 olmaz,
 *       DISARM/SESSION_END'de çağrı sonunda armed = 0 (düzenek D1, D4, D6). W3-K1'e göre
 *       acil durdurma aracı silahsızlandırmaz ve ARM'ı engellemez (düzenek D3, D8),
 *   (b) disarm ile aynı adımda gelen arm etkisizdir (D4, D6; A4-K44, K45, K46, K113, K114),
 *   (c) yinelenen komut ek etki üretmez (D7, D1, D2; A4-K48, K104).
 *
 * Önce belirli vakalar, sonra sabit tohumlu rastgele diziler (dört tohum) ve en sonda
 * dizilerin değişmezleri gerçekten sınadığını gösteren kapsam denetimi.
 */
#include <stdio.h>
#include <string.h>

#include "kabul_gorev.h"
#include "unity.h"

void setUp(void)
{
}

void tearDown(void)
{
}

/* Silahlandırmanın gözlenen ön koşullarının tuttuğu duruma gelir (kip init sonrası ne ise;
   ARM kipten bağımsızdır, A4-K127, 3.17). */
static void hazir_ol(const char *amac)
{
    kg_kur_varsayilan();
    kg_hazir_olmali(KG_HAZIR_AZAMI, amac);
}

/* ---- (b) DISARM ile aynı adımda ARM ----------------------------------------------- */

typedef enum {
    DA_MANDAL,          /* DISARM mandalı + ARM */
    DA_DIZI_ONCE_ARM,   /* cmds[] = {ARM, DISARM} (platform hatası; A4-K46) */
    DA_DIZI_ONCE_DISARM, /* cmds[] = {DISARM, ARM} */
    DA_YINELENEN_PARTI, /* DISARM mandalı + ARM, tf.seq = son kabul (DUP_SEQ) */
    DA_IMU_RET,         /* DISARM mandalı + ARM, IMU reddedilir (IMU_REJECTED) */
    DA_OTURUM_SONU,     /* oturum sonu çağrısı + ARM (A4-K122) */
    DA_N
} da_t;

static const char *const DA_ADI[DA_N] = {"mandal", "cmds ARM,DISARM", "cmds DISARM,ARM",
                                         "DUP_SEQ", "IMU_REJECTED", "SESSION_END"};

/* Bir DISARM + ARM çağrısı kurar, yürütür ve ARM'ın sonucunu denetler. */
static void disarm_arm_cagrisi(da_t v)
{
    ida_in_t *in;
    ida_cmd_t *arm;
    uint16_t beklenen = (uint16_t)IDA_REASON_NARROWED;
    ida_rc_t beklenen_rc = IDA_RC_OK;
    switch (v) {
    case DA_MANDAL:
        in = kg_girdi(KG_OLAGAN);
        kg_mandal(in, (uint8_t)IDA_NARROW_DISARM, 0u);
        arm = kg_komut(in, (uint8_t)IDA_CMD_ARM);
        break;
    case DA_DIZI_ONCE_ARM:
        in = kg_girdi(KG_OLAGAN);
        arm = kg_komut(in, (uint8_t)IDA_CMD_ARM);
        (void)kg_komut(in, (uint8_t)IDA_CMD_DISARM);
        break;
    case DA_DIZI_ONCE_DISARM:
        in = kg_girdi(KG_OLAGAN);
        (void)kg_komut(in, (uint8_t)IDA_CMD_DISARM);
        arm = kg_komut(in, (uint8_t)IDA_CMD_ARM);
        break;
    case DA_YINELENEN_PARTI:
        in = kg_girdi(KG_YINELENEN);
        kg_mandal(in, (uint8_t)IDA_NARROW_DISARM, 0u);
        arm = kg_komut(in, (uint8_t)IDA_CMD_ARM);
        beklenen = (uint16_t)IDA_REASON_CALL_REJECTED;
        beklenen_rc = IDA_RC_DUP_SEQ;
        break;
    case DA_IMU_RET:
        in = kg_girdi(KG_OLAGAN);
        in->imu.hdr.flags = 0u; /* A4-K33: VALID yok */
        kg_mandal(in, (uint8_t)IDA_NARROW_DISARM, 0u);
        arm = kg_komut(in, (uint8_t)IDA_CMD_ARM);
        beklenen_rc = IDA_RC_IMU_REJECTED;
        break;
    case DA_OTURUM_SONU:
    case DA_N:
    default:
        in = kg_girdi(KG_OTURUM_SONU);
        arm = kg_komut(in, (uint8_t)IDA_CMD_ARM);
        break;
    }
    int j = (int)(arm - in->cmds);
    ida_rc_t rc = kg_cagir(in);
    const ida_cmd_result_t *r = kg_sonuc(j);
    KG_DOGRULA(rc == beklenen_rc, "%s: dönüş %d, beklenen %d (A4-K113)", DA_ADI[v], (int)rc, (int)beklenen_rc);
    KG_DOGRULA(r->ack == IDA_ACK_REJECTED && r->reason == beklenen,
               "%s: ARM ack %u neden %u; REJECTED / %u bekleniyordu (A4-K45, K114; görev md.4)", DA_ADI[v],
               (unsigned)r->ack, (unsigned)r->reason, (unsigned)beklenen);
    KG_DOGRULA(!kg_out()->armed, "%s: DISARM ile aynı çağrıda ARM sonrası armed = 1 (görev md.4)", DA_ADI[v]);
    KG_DOGRULA(kg_out()->health.narrowed, "%s: health.narrowed = 0 (A4-K45)", DA_ADI[v]);
}

/* Görev md.4 (b), A4-K45, K46, K73, K113, K114, K122: silahsızken DISARM ile aynı çağrıdaki
   ARM etkisiz; her biçimde (mandal, cmds[]'te iki sıra, reddedilen çağrı, IMU'su reddedilen
   çağrı, oturum sonu çağrısı). Hazır olma gerekmez. */
static void test_md4_disarm_ile_arm_silahsiz(void)
{
    kg_baglam("md4 DISARM+ARM silahsız: hazırlık");
    kg_kur_varsayilan();
    (void)kg_hazirla(KG_HAZIR_AZAMI);
    kg_adimlar(5);
    kg_anlik_al();
    for (int v = 0; v < (int)DA_N; ++v) {
        kg_anlik_don();
        kg_baglam("md4 DISARM+ARM silahsız: %s", DA_ADI[v]);
        disarm_arm_cagrisi((da_t)v);
        kg_adimlar(5);
        KG_DOGRULA(!kg_out()->armed, "%s: sonraki adımlarda armed = 1 (ARM yeniden uygulanmamalı)", DA_ADI[v]);
    }
}

/* Görev md.4 (b), A4-K45, K46, K73, K113: silahlıyken DISARM ile aynı çağrıdaki ARM
   silahlı durumu korumaz; çağrı sonunda armed = 0. */
static void test_md4_disarm_ile_arm_silahli(void)
{
    kg_baglam("md4 DISARM+ARM silahlı: hazırlık");
    hazir_ol("DISARM ile aynı adımda ARM (silahlı)");
    (void)kg_silahlan();
    kg_adimlar(5);
    kg_anlik_al();
    for (int v = 0; v < (int)DA_N; ++v) {
        kg_anlik_don();
        kg_baglam("md4 DISARM+ARM silahlı: %s", DA_ADI[v]);
        KG_DOGRULA(kg_out()->armed, "düzenek: anlık görüntüde silahlı değil");
        disarm_arm_cagrisi((da_t)v);
        kg_adimlar(5);
        KG_DOGRULA(!kg_out()->armed, "%s: sonraki adımlarda armed = 1", DA_ADI[v]);
    }
}

/* ---- (c) Yinelenen komut ---------------------------------------------------------- */

/* Görev md.4 (c), A4-K48: kendi kuralıyla reddedilen ARM'ın kimliği de pencereye girer;
   aynı kimlikle yeniden gönderilen ARM DUPLICATE alır. Hazır olma gerekmez. */
static void test_md4_yinelenen_reddedilmis_arm(void)
{
    kg_kurulum_t k;
    kg_baglam("md4 yinelenen reddedilmiş ARM");
    kg_kurulum_varsayilan(&k);
    k.blok = KG_BLOK_YOK; /* ARM PARAM_INVALID ile reddedilir */
    kg_kur(&k);
    kg_adimlar(100);
    ida_in_t *in = kg_girdi(KG_OLAGAN);
    ida_cmd_t *c = kg_komut(in, (uint8_t)IDA_CMD_ARM);
    uint32_t id = c->cmd_id;
    (void)kg_cagir(in);
    kg_adimlar(3);
    in = kg_girdi(KG_OLAGAN);
    c = kg_komut_id(in, (uint8_t)IDA_CMD_ARM, id);
    int j = (int)(c - in->cmds);
    (void)kg_cagir(in);
    const ida_cmd_result_t *r = kg_sonuc(j);
    KG_DOGRULA(r->ack == IDA_ACK_DUPLICATE && r->reason == IDA_REASON_NONE,
               "yinelenen ARM ack %u neden %u; DUPLICATE / NONE bekleniyordu (A4-K48, K120)", (unsigned)r->ack,
               (unsigned)r->reason);
}

/* Görev md.4 (c), A4-K48: kabul edilmiş ARM'ın kimliği DISARM'dan sonra yeniden gelirse
   DUPLICATE alır ve aracı yeniden silahlandırmaz; yeni kimlikli ARM kabul edilir. */
static void test_md4_yinelenen_arm_yeniden_silahlandirmaz(void)
{
    kg_baglam("md4 yinelenen ARM");
    hazir_ol("yinelenen ARM");
    uint32_t id = kg_silahlan();
    kg_adimlar(3);
    kg_silahsizlan();
    KG_DOGRULA(kg_hazirla(400), "DISARM sonrası hazır değil: %s", kg_hazir_neden());
    for (int tekrar = 0; tekrar < 3; ++tekrar) {
        ida_in_t *in = kg_girdi(KG_OLAGAN);
        ida_cmd_t *c = kg_komut_id(in, (uint8_t)IDA_CMD_ARM, id);
        int j = (int)(c - in->cmds);
        (void)kg_cagir(in);
        const ida_cmd_result_t *r = kg_sonuc(j);
        KG_DOGRULA(r->ack == IDA_ACK_DUPLICATE, "yinelenen ARM (%d. kez) ack %u neden %u; DUPLICATE (A4-K48)", tekrar + 1,
                   (unsigned)r->ack, (unsigned)r->reason);
        KG_DOGRULA(!kg_out()->armed, "yinelenen ARM aracı silahlandırdı (A4-K48; görev md.4)");
        kg_adimlar(2);
    }
    (void)kg_silahlan();
}

/* Görev md.4 (c), A4-K48, K46: kabul edilmiş SET_MODE(GUIDED)'ın kimliği daraltıcı kipten
   sonra yeniden gelirse DUPLICATE alır ve kipi GUIDED'e döndürmez. */
static void test_md4_yinelenen_kip_komutu(void)
{
    kg_baglam("md4 yinelenen SET_MODE");
    hazir_ol("yinelenen SET_MODE");
    (void)kg_silahlan(); /* MANUAL'e geçiş silahlı durumda yapılır (kip ön koşulu uygulamanındır) */
    ida_in_t *in = kg_girdi(KG_OLAGAN);
    ida_cmd_t *c = kg_kip_komutu(in, (uint8_t)IDA_MODE_GUIDED);
    uint32_t id = c->cmd_id;
    int j = (int)(c - in->cmds);
    (void)kg_cagir(in);
    const ida_cmd_result_t *r = kg_sonuc(j);
    KG_DOGRULA(r->ack == IDA_ACK_ACCEPTED && kg_out()->status.mode == IDA_MODE_GUIDED,
               "hazır durumda SET_MODE(GUIDED) ack %u neden %u kip %u (3.17)", (unsigned)r->ack, (unsigned)r->reason,
               (unsigned)kg_out()->status.mode);
    kg_adimlar(3);
    kg_kip_sec((uint8_t)IDA_MODE_MANUAL);
    kg_adimlar(3);
    in = kg_girdi(KG_OLAGAN);
    c = kg_komut_id(in, (uint8_t)IDA_CMD_SET_MODE, id);
    c->u.set_mode.mode = (uint8_t)IDA_MODE_GUIDED;
    j = (int)(c - in->cmds);
    (void)kg_cagir(in);
    r = kg_sonuc(j);
    KG_DOGRULA(r->ack == IDA_ACK_DUPLICATE && kg_out()->status.mode == IDA_MODE_MANUAL,
               "yinelenen SET_MODE(GUIDED) ack %u, kip %u; DUPLICATE ve MANUAL bekleniyordu (A4-K48)", (unsigned)r->ack,
               (unsigned)kg_out()->status.mode);
}

/* ---- Rastgele diziler ------------------------------------------------------------- */

#define SON_KOMUT_N 8

typedef struct {
    ida_cmd_t c[SON_KOMUT_N];
    int n;
    int bas;
} son_komut_t;

static void son_ekle(son_komut_t *s, const ida_cmd_t *c)
{
    if (s->n < SON_KOMUT_N) {
        s->c[(s->bas + s->n) % SON_KOMUT_N] = *c;
        s->n++;
    } else {
        s->c[s->bas] = *c;
        s->bas = (s->bas + 1) % SON_KOMUT_N;
    }
}

static bool cagrida_id_var(const ida_in_t *in, uint32_t id)
{
    for (unsigned j = 0; j < in->n_cmds; ++j) {
        if (in->cmds[j].cmd_id == id) {
            return true;
        }
    }
    return false;
}

/* Kalıcı bozulmanın açılıp kapanması: kapalıyken binde ac, açıkken binde kapa olasılıkla. */
static bool degistir(kg_rng_t *r, bool acik, uint32_t ac, uint32_t kapa)
{
    return acik ? !kg_olasi(r, kapa) : kg_olasi(r, ac);
}

static void rastgele_dizi(uint64_t tohum, int n_cagri)
{
    kg_rng_t r;
    son_komut_t son;
    static const uint8_t KIPLER[4] = {(uint8_t)IDA_MODE_MANUAL, (uint8_t)IDA_MODE_STABILIZE,
                                      (uint8_t)IDA_MODE_DEPTH_HOLD, (uint8_t)IDA_MODE_STATION_KEEP};
    memset(&son, 0, sizeof son);
    kg_rng_kur(&r, tohum);
    kg_baglam("md4 rastgele tohum 0x%llx: hazırlık", (unsigned long long)tohum);
    kg_kur_varsayilan();
    (void)kg_hazirla(KG_HAZIR_AZAMI);
    for (int n = 0; n < n_cagri; ++n) {
        kg_dunya_t *d = kg_dunya();
        kg_baglam("md4 rastgele tohum 0x%llx adım %d", (unsigned long long)tohum, n);
        /* Dünya: kalıcı bozulmalar (çoğu zaman kapalı). */
        d->estop = (uint8_t)(degistir(&r, d->estop != 0u, 4u, 25u) ? 1u : 0u);
        d->link = !degistir(&r, !d->link, 2u, 30u);
        d->sizinti = (uint8_t)(degistir(&r, d->sizinti != 0u, 2u, 30u) ? 1u : 0u);
        d->batt_v = degistir(&r, d->batt_v < 10.0f, 1u, 30u) ? 3.0f : 16.0f;
        d->pstore = (uint8_t)(degistir(&r, d->pstore == IDA_PSTORE_BUSY, 3u, 60u) ? IDA_PSTORE_BUSY : IDA_PSTORE_IDLE);
        d->baro = !degistir(&r, !d->baro, 2u, 40u);
        d->mag = !degistir(&r, !d->mag, 2u, 40u);
        d->hb = !degistir(&r, !d->hb, 1u, 50u);
        d->imu = !degistir(&r, !d->imu, 2u, 100u);

        /* Çağrı türü. */
        uint32_t tur = kg_rng(&r, 1000u);
        ida_in_t *in;
        if (tur < 15u) {
            in = kg_girdi(KG_YINELENEN);
        } else if (tur < 25u) {
            in = kg_girdi(KG_ESKI);
        } else if (tur < 40u) {
            in = kg_girdi(KG_IZGARA);
        } else if (tur < 55u) {
            in = kg_girdi(KG_TICK_TF);
        } else if (tur < 58u) {
            in = kg_girdi(KG_OTURUM_SONU);
        } else {
            in = kg_girdi(KG_OLAGAN);
        }
        if (in->imu_present && kg_olasi(&r, 15u)) {
            in->imu.hdr.flags = 0u; /* IMU_REJECTED */
        }
        if (kg_olasi(&r, 8u)) {
            in->narrow.mask = (uint8_t)(in->narrow.mask | 0x10u); /* BAD_INPUT (A4-K46) */
        }
        /* Emniyet örnekleri (oturum sonu çağrısında da bulunabilir; A4-K122). */
        if (kg_olasi(&r, 15u)) {
            (void)kg_emniyet(in, 0u, 1u, KG_TUP_ACIK, KG_TUP_ACIK, true);
        } else if (kg_olasi(&r, 10u)) {
            (void)kg_emniyet(in, 1u, (uint8_t)kg_rng(&r, 2u), KG_TUP_ACIK, KG_TUP_ACIK, true);
        } else if (kg_olasi(&r, 10u)) {
            (void)kg_emniyet(in, 0u, 0u, KG_TUP_ACIK, KG_TUP_ACIK, false);
        }
        /* Mandallar. */
        if (kg_olasi(&r, 50u)) {
            kg_mandal(in, (uint8_t)IDA_NARROW_DISARM, 0u);
        }
        if (kg_olasi(&r, 50u)) {
            kg_mandal(in, (uint8_t)IDA_NARROW_MODE, KIPLER[kg_rng(&r, 4u)]);
        }
        if (kg_olasi(&r, 10u)) {
            kg_mandal(in, (uint8_t)IDA_NARROW_FC_DISARM, 0u);
        }
        if (kg_olasi(&r, 10u)) {
            kg_mandal(in, (uint8_t)IDA_NARROW_FC_ABORT, 0u);
        }
        /* Komutlar (en çok 8). */
        if (in->n_cmds < 8u && kg_olasi(&r, 150u)) {
            (void)kg_komut(in, (uint8_t)IDA_CMD_ARM);
        }
        if (in->n_cmds < 8u && kg_olasi(&r, 40u)) {
            (void)kg_kip_komutu(in, (uint8_t)IDA_MODE_GUIDED);
        }
        if (in->n_cmds < 8u && kg_olasi(&r, 15u)) {
            (void)kg_kip_komutu(in, (uint8_t)IDA_MODE_AUTO);
        }
        if (in->n_cmds < 8u && kg_olasi(&r, 40u)) {
            (void)kg_komut(in, (uint8_t)IDA_CMD_FAILSAFE_RESET);
        }
        if (in->n_cmds < 8u && kg_olasi(&r, 10u)) {
            ida_cmd_t *c = kg_komut(in, (uint8_t)IDA_CMD_MISSION_CTL);
            c->u.mission_ctl.action = (uint8_t)(1u + kg_rng(&r, 4u));
        }
        if (in->n_cmds < 8u && kg_olasi(&r, 5u)) {
            (void)kg_komut(in, (uint8_t)IDA_CMD_DISARM); /* platform hatası (A4-K46) */
        }
        if (in->n_cmds < 8u && kg_olasi(&r, 5u)) {
            (void)kg_kip_komutu(in, KIPLER[kg_rng(&r, 4u)]); /* daraltıcı hedef; platform hatası */
        }
        /* Yinelenen komutlar: son kabul edilen çağrıların son 8 komutundan. */
        for (int y = 0; y < 2; ++y) {
            if (in->n_cmds < 8u && son.n > 0 && kg_olasi(&r, 60u)) {
                const ida_cmd_t *eski = &son.c[(son.bas + (int)kg_rng(&r, (uint32_t)son.n)) % SON_KOMUT_N];
                if (!cagrida_id_var(in, eski->cmd_id)) {
                    ida_cmd_t *c = kg_komut_id(in, eski->type, eski->cmd_id);
                    *c = *eski;
                }
            }
        }
        ida_rc_t rc = kg_cagir(in);
        if (rc == IDA_RC_OK || rc == IDA_RC_IMU_REJECTED) {
            const ida_in_t *gi = kg_son_girdi();
            for (unsigned j = 0; j < gi->n_cmds; ++j) {
                const ida_cmd_t *c = &gi->cmds[j];
                bool dar = c->type == IDA_CMD_DISARM ||
                           (c->type == IDA_CMD_SET_MODE && c->u.set_mode.mode != IDA_MODE_GUIDED &&
                            c->u.set_mode.mode != IDA_MODE_AUTO);
                if (!dar && c->type != IDA_CMD_HEARTBEAT) {
                    son_ekle(&son, c);
                }
            }
        }
    }
}

/* Görev md.4, A4-K44–K48, K104, K113, K114, K116: sabit tohumlu rastgele komut ve arıza
   dizileri; değişmezler düzenekte her adımda denetlenir. */
static void test_md4_rastgele_tohum_1(void)
{
    rastgele_dizi(UINT64_C(0x1DA0000000000001), 6000);
}

static void test_md4_rastgele_tohum_2(void)
{
    rastgele_dizi(UINT64_C(0x1DA0000000000002), 6000);
}

static void test_md4_rastgele_tohum_3(void)
{
    rastgele_dizi(UINT64_C(0x1DA0000000000003), 6000);
}

static void test_md4_rastgele_tohum_4(void)
{
    rastgele_dizi(UINT64_C(0x1DA0000000000004), 6000);
}

/* Görev md.4: değişmezlerin boş geçmediğinin kanıtı. Önceki bütün sınamaların toplamında:
   silahlı adımlar, kabul edilen ARM'lar, acil durdurma kısıtı varken gelen ARM'lar,
   silahlıyken gelen acil durdurma, DISARM ile aynı çağrıdaki ARM'lar ve kesin yinelenen
   kimlikler bulunmalıdır. */
static void test_md4_kapsam(void)
{
    const kg_sayac_t *s = kg_sayac();
    kg_baglam("md4 kapsam");
    (void)printf("  md4 kapsam: çağrı %lu, silahlı çağrı %lu, ARM kabul %lu, ARM ret %lu, kısıtlıyken ARM %lu, "
                 "silahlıyken acil durdurma %lu, DISARM+ARM %lu, yinelenen %lu, GUIDED girişi %lu\n",
                 (unsigned long)s->cagri, (unsigned long)s->silahli_cagri, (unsigned long)s->arm_kabul,
                 (unsigned long)s->arm_ret, (unsigned long)s->estop_kisitli_arm,
                 (unsigned long)s->estop_kisitli_silahli_once, (unsigned long)s->disarm_ve_arm,
                 (unsigned long)s->yinelenen, (unsigned long)s->guided_giris);
    KG_DOGRULA(s->estop_kisitli_arm > 0u && s->disarm_ve_arm > 0u && s->yinelenen > 0u,
               "rastgele diziler kısıtlıyken ARM, DISARM+ARM ya da yinelenen kimlik üretmedi (düzenek hatası)");
    KG_DOGRULA(s->arm_kabul > 0u && s->silahli_cagri > 0u,
               "hiçbir ARM kabul edilmedi: değişmezler silahlı durumda sınanamadı (görev md.4)");
    KG_DOGRULA(s->estop_kisitli_silahli_once > 0u,
               "silahlıyken acil durdurma gelen adım oluşmadı: D3 (W3-K1) sınanamadı");
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_md4_disarm_ile_arm_silahsiz);
    RUN_TEST(test_md4_yinelenen_reddedilmis_arm);
    RUN_TEST(test_md4_disarm_ile_arm_silahli);
    RUN_TEST(test_md4_yinelenen_arm_yeniden_silahlandirmaz);
    RUN_TEST(test_md4_yinelenen_kip_komutu);
    RUN_TEST(test_md4_rastgele_tohum_1);
    RUN_TEST(test_md4_rastgele_tohum_2);
    RUN_TEST(test_md4_rastgele_tohum_3);
    RUN_TEST(test_md4_rastgele_tohum_4);
    RUN_TEST(test_md4_kapsam);
    return UNITY_END();
}
