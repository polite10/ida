/*
 * gorev_yukle.c — görev yüklemesi ve görev komutları (W5-gorev-yurutucu; A4-K51, tablo 3.17
 * MISSION_* satırları, A1 Tablo 3.6a).
 *
 * Yükleme atomiktir: MISSION_BEGIN(count) ayrı bir tamponu açar, MISSION_ITEM'lar sırayla
 * denetlenip oraya yazılır, MISSION_END öğe sayısı tutuyorsa tamponu yürürlükteki göreve
 * kopyalar. Herhangi bir öğe reddedilirse (Tablo 3.6a, sıra, sayı) yükleme kapanır ve yürürlükteki
 * görev değişmez; sonraki ITEM ve END "açık yükleme yok" ile OUT_OF_RANGE alır. Görev yalnız
 * IDLE, LOADED, PAUSED, COMPLETE, ABORTED durumunda ya da HOLD arıza eylemi sürerken değişir
 * (A4-K51); görev RUNNING'e geçince açık yükleme kapanır, böylece yürüyen görevin üstüne
 * yükleme hiçbir sırayla yürürlüğe giremez.
 *
 * MISSION_CTL (nötr; görev hiçbir koşulda kip ya da silahlandırma genişletmez):
 *   START  — AUTO'da (RUNNING, PAUSED; aynı çağrıda LOADED, COMPLETE, ABORTED) görevi öğe 0'dan
 *            yeniden başlatır; AUTO dışında BAD_MODE (görev yalnız AUTO'da yürür; AUTO'ya giriş
 *            SET_MODE(AUTO) ile, o da yüklü görevi başlatır).
 *   PAUSE  — RUNNING → PAUSED (operatör duraklaması: araç duraklama noktasında konum, derinlik ve
 *            yön tutar); başka durumda BAD_MODE. Silahsızlanma da operatör duraklaması sayılır.
 *   RESUME — PAUSED ve kip AUTO → RUNNING (öteki engeller sürüyorsa adım yeniden duraklatır;
 *            silahsızken verilirse sonraki adım yeniden duraklatır); başka durumda BAD_MODE.
 *   ABORT  — LOADED, RUNNING, PAUSED → ABORTED (operatör iptali: yüzeye çıkış istenmez); başka
 *            durumda BAD_MODE.
 */
#include <string.h>

#include "gorev/gorev_ic.h"
#include "mat/ida_math.h"

/* π'nin altındaki en büyük float (gudum.c ile aynı; (−π, π] aralığı için). */
#define PI_ALT 3.14159250f

static void ret(ida_cmd_result_t *r, uint16_t neden)
{
    r->ack = (uint8_t)IDA_ACK_REJECTED;
    r->reason = neden;
}

static void kabul(ida_cmd_result_t *r)
{
    r->ack = (uint8_t)IDA_ACK_ACCEPTED;
    r->reason = (uint16_t)IDA_REASON_NONE;
}

/* ---- A1 Tablo 3.6a ------------------------------------------------------------------------ */

static bool sifir(float x)
{
    return x == 0.0f; /* NaN değil ve tam 0 */
}

static bool aralik(float x, float en_az, float en_cok)
{
    return ida_sonlu(x) && (x >= en_az) && (x <= en_cok);
}

/* "> 0 – en_cok" */
static bool pozitif(float x, float en_cok)
{
    return ida_sonlu(x) && (x > 0.0f) && (x <= en_cok);
}

/* "tamsayı değerli": kesirli kısmı tam 0 (aralık önceden denetlenir; |x| küçük). */
static bool tamsayi(float x)
{
    return (float)(int32_t)x == x;
}

#define CER_BIT(c) ((uint32_t)1u << (uint32_t)(c))

static bool konum_gecerli(const ida_mission_item_t *m)
{
    switch (m->frame) {
    case IDA_MI_FRAME_NONE:
        return (m->lat_e7 == 0) && (m->lon_e7 == 0) && sifir(m->north_m) && sifir(m->east_m);
    case IDA_MI_FRAME_LOCAL:
        return (m->lat_e7 == 0) && (m->lon_e7 == 0) && aralik(m->north_m, -10000.0f, 10000.0f) &&
               aralik(m->east_m, -10000.0f, 10000.0f);
    case IDA_MI_FRAME_GLOBAL:
        return sifir(m->north_m) && sifir(m->east_m) && (m->lat_e7 >= -900000000) &&
               (m->lat_e7 <= 900000000) && (m->lon_e7 >= -1800000000) && (m->lon_e7 <= 1800000000);
    default:
        return false;
    }
}

bool ida_gor_oge_gecerli(const ida_mission_item_t *m)
{
    const float *p = m->p;
    uint32_t izin = 0u; /* türün izin verdiği çerçeveler */
    bool ok = false;
    switch (m->item_type) {
    case IDA_MI_WAYPOINT:
        izin = CER_BIT(IDA_MI_FRAME_LOCAL) | CER_BIT(IDA_MI_FRAME_GLOBAL);
        ok = aralik(m->depth, 0.0f, 100.0f) && aralik(p[0], 0.1f, 50.0f) &&
             aralik(p[1], 0.05f, 3.0f) && sifir(p[2]) && sifir(p[3]);
        break;
    case IDA_MI_DIVE_TO:
        izin = CER_BIT(IDA_MI_FRAME_NONE);
        ok = pozitif(m->depth, 100.0f) && aralik(p[0], 0.05f, 1.0f) && aralik(p[1], 0.05f, 5.0f) &&
             sifir(p[2]) && sifir(p[3]);
        break;
    case IDA_MI_SURFACE:
        izin = CER_BIT(IDA_MI_FRAME_NONE);
        ok = sifir(m->depth) && aralik(p[0], 0.05f, 1.0f) && sifir(p[1]) && sifir(p[2]) &&
             sifir(p[3]);
        break;
    case IDA_MI_HOLD:
        izin = CER_BIT(IDA_MI_FRAME_NONE) | CER_BIT(IDA_MI_FRAME_LOCAL) |
               CER_BIT(IDA_MI_FRAME_GLOBAL);
        ok = aralik(m->depth, 0.0f, 100.0f) && pozitif(p[0], 3600.0f) &&
             aralik(p[1], 0.1f, 50.0f) && sifir(p[2]) && sifir(p[3]);
        break;
    case IDA_MI_SCAN_LINES:
        izin = CER_BIT(IDA_MI_FRAME_LOCAL) | CER_BIT(IDA_MI_FRAME_GLOBAL);
        ok = aralik(m->depth, 0.0f, 100.0f) && aralik(p[0], 1.0f, 500.0f) &&
             aralik(p[1], 0.5f, 100.0f) && aralik(p[2], 1.0f, 100.0f) && tamsayi(p[2]) &&
             aralik(p[3], -PI_ALT, PI_ALT);
        break;
    case IDA_MI_SCAN_POINT:
        izin = CER_BIT(IDA_MI_FRAME_LOCAL) | CER_BIT(IDA_MI_FRAME_GLOBAL);
        ok = aralik(m->depth, 0.0f, 100.0f) && pozitif(p[0], 3600.0f) &&
             aralik(p[1], 0.1f, 50.0f) && sifir(p[2]) && sifir(p[3]);
        break;
    case IDA_MI_WAIT_GNSS:
        izin = CER_BIT(IDA_MI_FRAME_NONE);
        ok = sifir(m->depth) && pozitif(p[0], 3600.0f) && aralik(p[1], 1.0f, 4.0f) &&
             tamsayi(p[1]) && sifir(p[2]) && sifir(p[3]);
        break;
    default:
        return false; /* kapalı liste 1–7 (eski TARGET_ALIGN = 8 dahil) */
    }
    if ((m->frame > (uint8_t)IDA_MI_FRAME_GLOBAL) || ((izin & CER_BIT(m->frame)) == 0u)) {
        return false;
    }
    return ok && konum_gecerli(m);
}

static void oge_sikistir(const ida_mission_item_t *m, ida_gor_oge_t *o)
{
    (void)memset(o, 0, sizeof(*o));
    o->tur = m->item_type;
    o->cerceve = m->frame;
    for (uint32_t i = 0u; i < 4u; i++) {
        o->p[i] = m->p[i];
    }
    if (m->frame == (uint8_t)IDA_MI_FRAME_GLOBAL) {
        o->konum.ll[0] = m->lat_e7;
        o->konum.ll[1] = m->lon_e7;
    } else {
        o->konum.ne[0] = m->north_m;
        o->konum.ne[1] = m->east_m;
    }
    o->derinlik = m->depth;
}

/* ---- Durum yardımcıları -------------------------------------------------------------------- */

static void yukleme_kapat(ida_gor_gorev_t *v)
{
    v->yuk_acik = false;
    v->yuk_n = 0u;
    v->yuk_alinan = 0u;
}

void ida_gor_gorev_yukleme_kapat(ida_gor_t *g)
{
    yukleme_kapat(&g->gv);
}

/* Yürütmenin öğe ve görev izleri (yeni görev ya da baştan başlatma). */
static void yurutme_sifirla(ida_gor_t *g)
{
    ida_gor_gorev_t *v = &g->gv;
    g->mission_item = 0u;
    v->oge_basladi = false;
    v->kosuyordu = false;
    v->operator_durdu = false;
    v->dr_bekliyor = false;
    v->gorev_bas_var = false;
    v->onceki_yol = false;
    v->bant_say = false;
    v->durak_var = false;
    v->asama = 0u;
    v->oge_gecen_ns = 0;
    v->oge_asim_ns = 0;
    v->hold_birikim_ns = 0;
    v->hedef.yatay = (uint8_t)IDA_GOR_HY_YOK;
    v->hedef.kusak++;
}

bool ida_gor_gorev_degisebilir(const ida_gor_t *g)
{
    const uint8_t ms = g->mission_state;
    return (ms == (uint8_t)IDA_MS_IDLE) || (ms == (uint8_t)IDA_MS_LOADED) ||
           (ms == (uint8_t)IDA_MS_PAUSED) || (ms == (uint8_t)IDA_MS_COMPLETE) ||
           (ms == (uint8_t)IDA_MS_ABORTED) || (g->fs_level == (uint8_t)IDA_FS_HOLD);
}

void ida_gor_gorev_kip_auto(ida_gor_t *g)
{
    ida_gor_gorev_t *v = &g->gv;
    yukleme_kapat(v); /* yürüyen görev değiştirilemez (A4-K51) */
    if (g->mission_state == (uint8_t)IDA_MS_LOADED) {
        yurutme_sifirla(g);
        g->mission_state = (uint8_t)IDA_MS_RUNNING;
    } else if (g->mission_state == (uint8_t)IDA_MS_PAUSED) {
        v->operator_durdu = false; /* yeniden giriş operatör duraklamasını kaldırır */
        g->mission_state = (uint8_t)IDA_MS_RUNNING;
        v->hedef.kusak++;
    } else {
        /* RUNNING: etkisiz */
    }
}

void ida_gor_gorev_kip_cikis(ida_gor_t *g)
{
    if (g->mission_state == (uint8_t)IDA_MS_RUNNING) {
        ida_gor_gorev_durak_yakala(g);
        g->mission_state = (uint8_t)IDA_MS_PAUSED;
        g->gv.hedef.yatay = (uint8_t)IDA_GOR_HY_YOK;
        g->gv.hedef.kusak++;
    }
}

/* ---- Komutlar ------------------------------------------------------------------------------ */

static void begin_komutu(ida_gor_t *g, const ida_cmd_t *cmd, ida_cmd_result_t *r)
{
    const uint16_t n = cmd->u.mission_begin.count;
    if ((n < 1u) || (n > IDA_GOR_GOREV_AZAMI)) {
        ret(r, (uint16_t)IDA_REASON_OUT_OF_RANGE);
        return;
    }
    if (!ida_gor_gorev_degisebilir(g)) {
        ret(r, (uint16_t)IDA_REASON_BAD_MODE);
        return;
    }
    /* Yeni BEGIN önceki açık yüklemeyi bırakır (yürürlükteki görev değişmez). */
    g->gv.yuk_acik = true;
    g->gv.yuk_n = n;
    g->gv.yuk_alinan = 0u;
    kabul(r);
}

static void item_komutu(ida_gor_t *g, const ida_cmd_t *cmd, ida_cmd_result_t *r)
{
    ida_gor_gorev_t *v = &g->gv;
    const ida_mission_item_t *m = &cmd->u.mission_item;
    if (!v->yuk_acik) {
        ret(r, (uint16_t)IDA_REASON_OUT_OF_RANGE); /* açık yükleme yok */
        return;
    }
    if ((m->index != v->yuk_alinan) || (m->index >= v->yuk_n) || !ida_gor_oge_gecerli(m)) {
        /* Sıra, sayı ya da Tablo 3.6a: öğe ve yükleme bütünüyle reddedilir (A4-K51). */
        yukleme_kapat(v);
        ret(r, (uint16_t)IDA_REASON_OUT_OF_RANGE);
        return;
    }
    oge_sikistir(m, &v->yuk[m->index]);
    v->yuk_alinan++;
    kabul(r);
}

static void end_komutu(ida_gor_t *g, ida_cmd_result_t *r)
{
    ida_gor_gorev_t *v = &g->gv;
    if (!v->yuk_acik) {
        ret(r, (uint16_t)IDA_REASON_OUT_OF_RANGE);
        return;
    }
    if ((v->yuk_alinan != v->yuk_n) || !ida_gor_gorev_degisebilir(g)) {
        /* Sayı tutmuyor (yarım yükleme yürürlüğe girmez). Değiştirilemez durum yalnız savunmadır:
           RUNNING'e geçiş açık yüklemeyi zaten kapatır. */
        yukleme_kapat(v);
        ret(r, (uint16_t)IDA_REASON_OUT_OF_RANGE);
        return;
    }
    const uint16_t n = v->yuk_n;
    (void)memcpy(v->oge, v->yuk, (size_t)n * sizeof(v->oge[0]));
    v->n = n;
    yukleme_kapat(v);
    yurutme_sifirla(g);
    /* iptal_nedeni korunur: iptal mandalı sürerken nedeni yalnız FAILSAFE_RESET temizler (bulgu 4) */
    g->mission_state = (uint8_t)IDA_MS_LOADED;
    kabul(r);
}

static void ctl_komutu(ida_gor_t *g, const ida_emn_t *yetki, const ida_cmd_t *cmd,
                       ida_cmd_result_t *r)
{
    ida_gor_gorev_t *v = &g->gv;
    const uint8_t a = cmd->u.mission_ctl.action;
    const uint8_t ms = g->mission_state;
    const bool auto_kip = yetki->mode == (uint8_t)IDA_MODE_AUTO;
    switch (a) {
    case IDA_MCTL_START:
        if (!auto_kip || (ms == (uint8_t)IDA_MS_IDLE) || (v->n == 0u)) {
            ret(r, (uint16_t)IDA_REASON_BAD_MODE);
            return;
        }
        yukleme_kapat(v);
        yurutme_sifirla(g);
        g->mission_state = (uint8_t)IDA_MS_RUNNING;
        break;
    case IDA_MCTL_PAUSE:
        /* W5-kontrol-acil düzeltme turu (mimar kararı; inceleme bulgu 1): görev AUTO'da
           çekirdeğin kendi koşuluyla (acil durdurma, konum geçersiz, HOLD arıza eylemi, ölü hesap
           beklemesi) PAUSED iken de PAUSE kabul edilir ve operatör duraklaması kurulur: koşul
           kalkınca görev kendiliğinden sürmez, RESUME ya da SET_MODE(AUTO) bekler. Zaten operatör
           duraklamasında (silahsızlanma dahil) ya da AUTO dışında (kip çıkışı) PAUSED iken
           BAD_MODE sürer. */
        if (!((ms == (uint8_t)IDA_MS_RUNNING) ||
              ((ms == (uint8_t)IDA_MS_PAUSED) && auto_kip && !v->operator_durdu))) {
            ret(r, (uint16_t)IDA_REASON_BAD_MODE);
            return;
        }
        v->operator_durdu = true;
        ida_gor_gorev_kip_cikis(g);
        break;
    case IDA_MCTL_RESUME:
        if ((ms != (uint8_t)IDA_MS_PAUSED) || !auto_kip) {
            ret(r, (uint16_t)IDA_REASON_BAD_MODE);
            return;
        }
        ida_gor_gorev_kip_auto(g);
        break;
    case IDA_MCTL_ABORT:
        if ((ms != (uint8_t)IDA_MS_LOADED) && (ms != (uint8_t)IDA_MS_RUNNING) &&
            (ms != (uint8_t)IDA_MS_PAUSED)) {
            ret(r, (uint16_t)IDA_REASON_BAD_MODE);
            return;
        }
        g->mission_state = (uint8_t)IDA_MS_ABORTED;
        v->operator_durdu = false;
        v->dr_bekliyor = false;
        v->hedef.yatay = (uint8_t)IDA_GOR_HY_YOK;
        v->hedef.kusak++;
        break;
    default:
        ret(r, (uint16_t)IDA_REASON_OUT_OF_RANGE); /* action 1–4 dışında */
        return;
    }
    kabul(r);
}

void ida_gor_gorev_komut(ida_gor_t *g, const ida_emn_t *yetki, const ida_cmd_t *cmd,
                         ida_cmd_result_t *r)
{
    switch (cmd->type) {
    case IDA_CMD_MISSION_BEGIN:
        begin_komutu(g, cmd, r);
        break;
    case IDA_CMD_MISSION_ITEM:
        item_komutu(g, cmd, r);
        break;
    case IDA_CMD_MISSION_END:
        end_komutu(g, r);
        break;
    case IDA_CMD_MISSION_CTL:
        ctl_komutu(g, yetki, cmd, r);
        break;
    default:
        r->ack = (uint8_t)IDA_ACK_UNSUPPORTED;
        r->reason = (uint16_t)IDA_REASON_NONE;
        break;
    }
}

void ida_gor_gorev_oturum_basi(ida_gor_t *g)
{
    /* A4-K72: oturum başında açık yükleme kapanır; ölçüm zamanına bağlı izler (bant sayacı,
       ölü hesap yolunun son konumu, güdüm referansı) geçersizdir. Kestirim yeniden başladığı
       için görevin yakaladığı yerel düzlem noktaları da geçersizdir (bulgu 2): öğe başındaki
       konum, önceki yol noktasının hedefi ve duraklama noktası bırakılır, etkin öğe yeniden
       başlar (konumlar ilk geçerli seyrüsefer görüntüsünden yeniden yakalanır). Görev durumu,
       yürürlükteki görev, etkin öğenin indisi ve duvar saatindeki toplam görev süresi korunur. */
    ida_gor_gorev_t *v = &g->gv;
    yukleme_kapat(v);
    v->bant_say = false;
    v->dr_son_var = false;
    v->oge_basladi = false;
    v->kosuyordu = false;
    v->onceki_yol = false;
    v->durak_var = false;
    v->asama = 0u;
    v->oge_gecen_ns = 0;
    v->hold_birikim_ns = 0;
    v->hedef.yatay = (uint8_t)IDA_GOR_HY_YOK;
    v->hedef.kusak++;
}

static bool b_gecerli(const bool *b)
{
    uint8_t x = 0u;
    (void)memcpy(&x, b, sizeof(x));
    return x <= 1u;
}

bool ida_gor_gorev_tutarli(const ida_gor_t *g)
{
    const ida_gor_gorev_t *v = &g->gv;
    const bool bools = b_gecerli(&v->yuk_acik) && b_gecerli(&v->oge_basladi) &&
                       b_gecerli(&v->kosuyordu) && b_gecerli(&v->operator_durdu) &&
                       b_gecerli(&v->dr_bekliyor) && b_gecerli(&v->iptal_bekliyor) &&
                       b_gecerli(&v->gorev_bas_var) && b_gecerli(&v->son_now_var) &&
                       b_gecerli(&v->onceki_yol) && b_gecerli(&v->bant_say) &&
                       b_gecerli(&v->dr_son_var) && b_gecerli(&v->durak_var) &&
                       b_gecerli(&g->nav_irtifa_var) && b_gecerli(&g->gnss_taze) &&
                       b_gecerli(&g->irtifa_kisit);
    return bools && (v->hold_birikim_ns >= 0) && (v->n <= IDA_GOR_GOREV_AZAMI) &&
           (v->yuk_n <= IDA_GOR_GOREV_AZAMI) && (v->yuk_alinan <= v->yuk_n) && (g->mission_item <= v->n) &&
           (v->hedef.yatay <= (uint8_t)IDA_GOR_HY_TUT) &&
           (v->hedef.dikey <= (uint8_t)IDA_GOR_HD_YUZEY) && (v->asama <= 1u);
}
