/*
 * gudum.c — güdüm: GUIDED_SP ve kontrol isteği (W3-zincir; W3-K7, A4-K50, tablo 3.17).
 * Açıklama gudum.h'tadır.
 */
#include "gudum/gudum.h"

#include <stddef.h>
#include <string.h>

#include "cekirdek.h"
#include "kestirim/kestirim.h"
#include "mat/ida_math.h"
#include "zaman/zaman.h"

/* π'nin altındaki en büyük float: 3,14159250f (bit deseni 0x40490FDA; ondalık yazımın en
   yakın float'ı budur). 3.4: yaw −π < yaw ≤ π, karşılaştırma double'da; float değerler için
   bu, −PI_ALT ≤ yaw ≤ PI_ALT'a eşdeğerdir (float π = 0x40490FDB > π). */
#define PI_ALT 3.14159250f

#define SP_TANIMLI 0x7Fu /* IDA_SP_* birleşimi (3.4) */

void ida_gud_kur(ida_gud_t *g)
{
    (void)memset(g, 0, sizeof(*g));
    g->dikey = (uint8_t)IDA_GUD_DIKEY_YOK;
    g->yon_turu = (uint8_t)IDA_GUD_YON_YOK;
    g->yatay = (uint8_t)IDA_GUD_YATAY_YOK;
    g->kusak = 0u;
}

static bool sakli_hedef_var(const ida_gud_t *g)
{
    return (g->dikey != (uint8_t)IDA_GUD_DIKEY_YOK) || (g->yon_turu != (uint8_t)IDA_GUD_YON_YOK) ||
           (g->yatay != (uint8_t)IDA_GUD_YATAY_YOK);
}

/* GUIDED_SP'nin saklı kanallarını siler (AUTO güdümünün durumu korunur). */
static void sp_sil(ida_gud_t *g)
{
    g->dikey = (uint8_t)IDA_GUD_DIKEY_YOK;
    g->yon_turu = (uint8_t)IDA_GUD_YON_YOK;
    g->yatay = (uint8_t)IDA_GUD_YATAY_YOK;
    g->kusak = 0u;
    g->derinlik = 0.0f;
    g->irtifa = 0.0f;
    g->dikey_hiz = 0.0f;
    g->yon = 0.0f;
    g->yon_hizi = 0.0f;
    g->konum_ne[0] = 0.0f;
    g->konum_ne[1] = 0.0f;
    g->hiz_ne[0] = 0.0f;
    g->hiz_ne[1] = 0.0f;
    g->sp_var = false;
    g->sp_son_ns = 0;
}

/* Saklı hedefi siler; bir hedef varsa kontrolün belleği de eski hedefe aittir: kontrolün
   iptal biti kurulur (emn NULL ise kurulmaz: SESSION_START'ta (b3) zaten kurmuştur). */
static void hedef_sil(ida_gud_t *g, ida_emn_t *emn)
{
    if (sakli_hedef_var(g)) {
        sp_sil(g);
        if (emn != NULL) {
            emn->hedef_iptal = (uint8_t)(emn->hedef_iptal | IDA_EMN_IPTAL_KON);
        }
    }
}

void ida_gud_oturum_basi(ida_gud_t *g)
{
    /* A4-K72: hedef bir önceki oturumun ölçüm zamanına ve kestirimine bağlıdır (EMNİYET). AUTO
       güdümünün referansı ve ILOS integrali de ölçüm zamanına bağlıdır: yeniden kurulur. */
    hedef_sil(g, NULL);
    g->oto_var = false;
    g->t_var = false;
}

/* ---- GUIDED_SP ---------------------------------------------------------------------------- */

bool ida_gud_sp_gecerli(const ida_guided_sp_t *sp)
{
    const uint8_t m = sp->mask;
    if ((m == 0u) || ((m & (uint8_t)(0xFFu ^ SP_TANIMLI)) != 0u)) {
        return false;
    }
    if (((m & IDA_SP_POS_NE) != 0u) && !(ida_sonlu(sp->pos_ne[0]) && ida_sonlu(sp->pos_ne[1]))) {
        return false;
    }
    if (((m & IDA_SP_DEPTH) != 0u) && !(ida_sonlu(sp->depth) && (sp->depth >= 0.0f))) {
        return false;
    }
    if (((m & IDA_SP_ALTITUDE) != 0u) && !(ida_sonlu(sp->altitude) && (sp->altitude >= 0.0f))) {
        return false;
    }
    if (((m & IDA_SP_VEL_NE) != 0u) &&
        !(ida_sonlu(sp->vel_ned[0]) && ida_sonlu(sp->vel_ned[1]))) {
        return false;
    }
    if (((m & IDA_SP_VEL_D) != 0u) && !ida_sonlu(sp->vel_ned[2])) {
        return false;
    }
    if (((m & IDA_SP_YAW) != 0u) &&
        !(ida_sonlu(sp->yaw) && (sp->yaw >= -PI_ALT) && (sp->yaw <= PI_ALT))) {
        return false;
    }
    if (((m & IDA_SP_YAW_RATE) != 0u) && !ida_sonlu(sp->yaw_rate)) {
        return false;
    }
    return true;
}

static void hedefi_sakla(ida_gud_t *g, const ida_guided_sp_t *sp)
{
    const uint8_t m = sp->mask;
    /* Maskesiz alan yok sayılır: o kanalın saklı hedefi değişmez. Kanal içinde öncelik:
       DEPTH > ALTITUDE > VEL_D; YAW > YAW_RATE; POS_NE > VEL_NE. */
    if ((m & IDA_SP_DEPTH) != 0u) {
        g->dikey = (uint8_t)IDA_GUD_DIKEY_DERINLIK;
        g->derinlik = sp->depth;
    } else if ((m & IDA_SP_ALTITUDE) != 0u) {
        g->dikey = (uint8_t)IDA_GUD_DIKEY_IRTIFA;
        g->irtifa = sp->altitude;
    } else if ((m & IDA_SP_VEL_D) != 0u) {
        g->dikey = (uint8_t)IDA_GUD_DIKEY_HIZ;
        g->dikey_hiz = sp->vel_ned[2];
    } else {
        /* dikey kanal maskesiz */
    }
    if ((m & IDA_SP_YAW) != 0u) {
        g->yon_turu = (uint8_t)IDA_GUD_YON_ACI;
        g->yon = ida_aci_sar(sp->yaw);
    } else if ((m & IDA_SP_YAW_RATE) != 0u) {
        g->yon_turu = (uint8_t)IDA_GUD_YON_HIZ;
        g->yon_hizi = sp->yaw_rate;
    } else {
        /* yön kanalı maskesiz */
    }
    if ((m & IDA_SP_POS_NE) != 0u) {
        g->yatay = (uint8_t)IDA_GUD_YATAY_KONUM;
        g->konum_ne[0] = sp->pos_ne[0];
        g->konum_ne[1] = sp->pos_ne[1];
    } else if ((m & IDA_SP_VEL_NE) != 0u) {
        g->yatay = (uint8_t)IDA_GUD_YATAY_HIZ;
        g->hiz_ne[0] = sp->vel_ned[0];
        g->hiz_ne[1] = sp->vel_ned[1];
    } else {
        /* yatay kanal maskesiz */
    }
}

void ida_gud_komut(ida_core_t *c, const ida_komut_baglam_t *b, const ida_cmd_t *cmd,
                   ida_cmd_result_t *r)
{
    if (cmd->type != (uint8_t)IDA_CMD_GUIDED_SP) {
        r->ack = (uint8_t)IDA_ACK_UNSUPPORTED; /* başka tür buraya gelmez (A4-K124) */
        r->reason = (uint16_t)IDA_REASON_NONE;
        return;
    }
    /* 3.17 GUIDED_SP satırı, sırayla: OUT_OF_RANGE, sonra BAD_MODE (A4-K50). */
    if (!ida_gud_sp_gecerli(&cmd->u.guided)) {
        r->ack = (uint8_t)IDA_ACK_REJECTED;
        r->reason = (uint16_t)IDA_REASON_OUT_OF_RANGE;
        return;
    }
    /* Kip komutun işlendiği andaki kiptir: dağıtıcının bağlamındaki `mode` komutlardan önceki
       değerdir ve aynı çağrıda önce gelen SET_MODE(GUIDED)'ı görmez (W3-K13 ilkesi). */
    if (c->emn.mode != (uint8_t)IDA_MODE_GUIDED) {
        r->ack = (uint8_t)IDA_ACK_REJECTED;
        r->reason = (uint16_t)IDA_REASON_BAD_MODE;
        return;
    }
    if ((c->emn.hedef_iptal & IDA_EMN_IPTAL_GUD) != 0u) {
        /* Daraltmadan önce saklanmış hedef silinir (yeni iletinin maskesiz kanallarında da
           kalmaz); bu komut daraltmadan sonra geldiği için geçerli yeni hedeftir. */
        hedef_sil(&c->gud, &c->emn);
        c->emn.hedef_iptal = (uint8_t)(c->emn.hedef_iptal & (uint8_t)(0xFFu ^ IDA_EMN_IPTAL_GUD));
    }
    hedefi_sakla(&c->gud, &cmd->u.guided);
    c->gud.kusak = c->emn.kusak; /* tanı */
    /* 02 §2.6: GUD_SP_ASIM_MS içinde yeni hedef gelmezse son hedefte kalınır; süre son kabul
       edilen GUIDED_SP'den (duvar saati) sayılır. */
    c->gud.sp_var = true;
    c->gud.sp_son_ns = b->now_w.ns;
    r->ack = (uint8_t)IDA_ACK_ACCEPTED;
    r->reason = (uint16_t)IDA_REASON_NONE;
}

/* 02 §2.6 "GUD_SP_ASIM_MS yeni hedef yoksa son hedefte kalır": süre dolunca hız türü kanallar
   (VEL_D, YAW_RATE, VEL_NE) bulunulan derinliği, yönü ve konumu tutmaya çevrilir; konum türü
   kanallar (DEPTH, YAW, POS_NE) zaten son hedeftir. Bileşen geçersizse kanal görev modülünün
   tutmasına bırakılır. */
static void sp_zaman_asimi(ida_gud_t *g, ida_core_t *c)
{
    if (!g->sp_var || (c->emn.mode != (uint8_t)IDA_MODE_GUIDED)) {
        return;
    }
    const int64_t now = c->cer.now_izi_ns;
    if ((now - g->sp_son_ns) < ida_ms_ns(ida_prm_u32(&c->prm, IDA_PRM_gud_sp_asim_ms))) {
        return;
    }
    const ida_gor_t *gor = &c->gor;
    bool degisti = false;
    if (g->dikey == (uint8_t)IDA_GUD_DIKEY_HIZ) {
        g->dikey = gor->nav_derinlik_var ? (uint8_t)IDA_GUD_DIKEY_DERINLIK : (uint8_t)IDA_GUD_DIKEY_YOK;
        g->derinlik = gor->nav_derinlik;
        degisti = true;
    }
    if (g->yon_turu == (uint8_t)IDA_GUD_YON_HIZ) {
        g->yon_turu = gor->nav_yon_var ? (uint8_t)IDA_GUD_YON_ACI : (uint8_t)IDA_GUD_YON_YOK;
        g->yon = gor->nav_yon;
        degisti = true;
    }
    if (g->yatay == (uint8_t)IDA_GUD_YATAY_HIZ) {
        g->yatay = gor->nav_konum_var ? (uint8_t)IDA_GUD_YATAY_KONUM : (uint8_t)IDA_GUD_YATAY_YOK;
        g->konum_ne[0] = gor->nav_ne[0];
        g->konum_ne[1] = gor->nav_ne[1];
        degisti = true;
    }
    g->sp_var = false; /* yeni GUIDED_SP'ye kadar */
    if (degisti) {
        /* kontrol referansı bulunulan durumdan yenilenir */
        c->emn.hedef_iptal = (uint8_t)(c->emn.hedef_iptal | IDA_EMN_IPTAL_KON);
    }
}

void ida_gud_adim(ida_gud_t *g, ida_core_t *c)
{
    const bool iptal = (c->emn.hedef_iptal & IDA_EMN_IPTAL_GUD) != 0u;
    if (iptal) {
        hedef_sil(g, &c->emn); /* önce sil, sonra kendi bitini temizle (EMNİYET) */
        c->emn.hedef_iptal = (uint8_t)(c->emn.hedef_iptal & (uint8_t)(0xFFu ^ IDA_EMN_IPTAL_GUD));
    }
    if ((c->emn.mode != (uint8_t)IDA_MODE_GUIDED) ||
        (ida_gor_fs_duzey(&c->gor) >= (uint8_t)IDA_FS_HOLD)) {
        hedef_sil(g, &c->emn);
    }
    sp_zaman_asimi(g, c);
    ida_gud_oto_adim(g, c, iptal);
}

static bool b_gecerli(const bool *b)
{
    uint8_t x = 0u;
    (void)memcpy(&x, b, sizeof(x));
    return x <= 1u;
}

bool ida_gud_tutarli(const ida_gud_t *g)
{
    return (g->dikey <= (uint8_t)IDA_GUD_DIKEY_HIZ) && (g->yon_turu <= (uint8_t)IDA_GUD_YON_HIZ) &&
           (g->yatay <= (uint8_t)IDA_GUD_YATAY_HIZ) && b_gecerli(&g->sp_var) &&
           b_gecerli(&g->oto_var) && b_gecerli(&g->t_var) && ida_sonlu(g->ref.d) &&
           ida_sonlu(g->ref.w) && ida_sonlu(g->y_int);
}

uint8_t ida_gud_param_grup(const ida_prm_kume_t *p)
{
    (void)p;
    return 0u;
}

/* ---- Kontrol isteği ---------------------------------------------------------------------- */

/* Dikey kanalın düşüşü (W3-K7): dikey hız geçerliyse hız 0, değilse kuvvet 0. */
static void dikey_sonum(uint8_t v, ida_kon_istek_t *i)
{
    if ((v & IDA_KES_G_DIKEY_HIZ) != 0u) {
        i->dikey = (uint8_t)IDA_KON_DIKEY_HIZ;
        i->dikey_hiz = 0.0f;
    } else {
        i->dikey = (uint8_t)IDA_KON_DIKEY_KUVVET;
        i->kuvvet[IDA_EKS_Z] = 0.0f;
    }
}

static void dikey_yaz(const ida_gud_t *g, const ida_gor_t *gor, uint8_t v, ida_kon_istek_t *i)
{
    switch (g->dikey) {
    case IDA_GUD_DIKEY_DERINLIK:
        if (gor->nav_derinlik_var) {
            i->dikey = (uint8_t)IDA_KON_DIKEY_DERINLIK;
            i->derinlik = g->derinlik;
            if (gor->dalis_yasak && (gor->nav_derinlik < i->derinlik)) {
                /* Sızıntı algılayıcısı bayat: derinleşme yok (02 §4.3; görev modülüyle aynı). */
                i->derinlik = gor->nav_derinlik;
            }
        } else {
            dikey_sonum(v, i);
        }
        break;
    case IDA_GUD_DIKEY_HIZ:
        if (((v & IDA_KES_G_DIKEY_HIZ) != 0u) &&
            !(gor->dalis_yasak && (g->dikey_hiz > 0.0f))) {
            i->dikey = (uint8_t)IDA_KON_DIKEY_HIZ;
            i->dikey_hiz = g->dikey_hiz;
        } else {
            dikey_sonum(v, i);
        }
        break;
    case IDA_GUD_DIKEY_IRTIFA:
        /* Dibe uzaklık kestirimi bu sürümde yok (W3-kestirim dilim 1): sönümlü, sıfır hedefli. */
        dikey_sonum(v, i);
        break;
    default:
        break; /* maskesiz: görev modülünün tutma hedefi */
    }
}

static void yon_yaz(const ida_gud_t *g, const ida_gor_t *gor, uint8_t v, ida_kon_istek_t *i)
{
    if (g->yon_turu == (uint8_t)IDA_GUD_YON_ACI) {
        i->yon_kanali = (uint8_t)IDA_KON_YON_HIZ;
        i->yon_hizi = 0.0f;
        if (gor->nav_yon_var) {
            i->yon_kanali = (uint8_t)IDA_KON_YON_ACI;
            i->yon = g->yon;
        }
    } else if (g->yon_turu == (uint8_t)IDA_GUD_YON_HIZ) {
        i->yon_kanali = (uint8_t)IDA_KON_YON_HIZ;
        i->yon_hizi = ((v & IDA_KES_G_TUTUM) != 0u) ? g->yon_hizi : 0.0f;
    } else {
        /* maskesiz: görev modülünün tutma hedefi */
    }
}

static void yatay_yaz(const ida_gud_t *g, const ida_core_t *c, uint8_t v, ida_kon_istek_t *i)
{
    if (g->yatay == (uint8_t)IDA_GUD_YATAY_YOK) {
        return; /* maskesiz: görev modülünün tutma hedefi */
    }
    const ida_gor_t *gor = &c->gor;
    const bool hiz_var = ((v & IDA_KES_G_YATAY_HIZ) != 0u) && gor->nav_yon_var;
    float hiz_ne[2] = {0.0f, 0.0f};
    bool hedef_var = false;
    if ((g->yatay == (uint8_t)IDA_GUD_YATAY_KONUM) && gor->nav_konum_var && hiz_var) {
        const float kp = ida_prm_f32(&c->prm, IDA_PRM_gud_konum_kp);
        hiz_ne[0] = kp * (g->konum_ne[0] - gor->nav_ne[0]);
        hiz_ne[1] = kp * (g->konum_ne[1] - gor->nav_ne[1]);
        hedef_var = true;
    } else if ((g->yatay == (uint8_t)IDA_GUD_YATAY_HIZ) && hiz_var) {
        hiz_ne[0] = g->hiz_ne[0];
        hiz_ne[1] = g->hiz_ne[1];
        hedef_var = true;
    } else {
        /* geçerlilik yok: sönümlü, sıfır hedefli */
    }
    (void)memset(i->hiz, 0, sizeof(i->hiz));
    i->kuvvet[IDA_EKS_X] = 0.0f;
    i->kuvvet[IDA_EKS_Y] = 0.0f;
    if (!((v & IDA_KES_G_YATAY_HIZ) != 0u)) {
        i->yatay = (uint8_t)IDA_KON_YATAY_KUVVET;
        return;
    }
    i->yatay = (uint8_t)IDA_KON_YATAY_HIZ;
    if (hedef_var) {
        /* NED → yön çerçevesi (ileri, yan). */
        const float cp = ida_cos(gor->nav_yon);
        const float sp = ida_sin(gor->nav_yon);
        i->hiz[0] = (cp * hiz_ne[0]) + (sp * hiz_ne[1]);
        i->hiz[1] = (-sp * hiz_ne[0]) + (cp * hiz_ne[1]);
    }
}

static void guided_yaz(const ida_gud_t *g, const ida_core_t *c, ida_kon_istek_t *i);

void ida_gud_kon_istegi(const ida_gud_t *g, const ida_core_t *c, ida_kon_istek_t *i)
{
    ida_gor_istek(&c->gor, &c->emn, i);
    if ((c->emn.mode == (uint8_t)IDA_MODE_AUTO) &&
        ((c->emn.hedef_iptal & IDA_EMN_IPTAL_GUD) == 0u)) {
        ida_gud_oto_istek(g, c, i); /* görev yürütücüsünün hedefi (arıza HOLD'dan hafifken) */
    }
    guided_yaz(g, c, i);
    ida_gud_irtifa_kisit(c, i); /* en az irtifa (02 §4.3): her kipte en son */
}

/* GUIDED_SP'nin saklı kanalları. */
static void guided_yaz(const ida_gud_t *g, const ida_core_t *c, ida_kon_istek_t *i)
{
    /* GUIDED_SP yalnız GUIDED kipinde, silahlıyken, arıza eylemi HOLD'dan hafifken, istek
       etkinken ve hedef saklandığından beri daraltma kuşağı değişmemişken uygulanır (arıza >
       operatör, 02 §2.6; sol bulgu 1). */
    if ((i->kip != (uint8_t)IDA_KON_KIP_DALIS) || !c->emn.armed ||
        (c->emn.mode != (uint8_t)IDA_MODE_GUIDED) ||
        (ida_gor_fs_duzey(&c->gor) >= (uint8_t)IDA_FS_HOLD) ||
        ((c->emn.hedef_iptal & IDA_EMN_IPTAL_GUD) != 0u)) {
        return;
    }
    const uint8_t v = c->gor.gecerlilik; /* adım anındaki bileşen geçerliliği (W3-K5) */
    dikey_yaz(g, &c->gor, v, i);
    yon_yaz(g, &c->gor, v, i);
    yatay_yaz(g, c, v, i);
}
