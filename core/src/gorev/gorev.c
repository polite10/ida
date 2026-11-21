/*
 * gorev.c — görev modülünün kancaları: adım (kip düşmesi, arıza, silahsızlandırma), komutlar
 * (HEARTBEAT, ARM, SET_MODE, FAILSAFE_RESET), örnekler, durum ve iç tutarlılık
 * (W3-gorev dilim 1; A4-K45, K52, K76, K77, K89, K124, K127; tablo 3.17).
 *
 * EMNİYET ilkesi (W3-gorev): tanımsız kip, tanımsız komut ya da çelişen girdi silahsız ve
 * sıfır itki yönünde çözülür; kısıtlayan her şey adımda, genişleten her şey komutlarda
 * uygulanır; emniyet modülünün (b) daraltması geri alınmaz.
 */
#include "gorev/gorev.h"

#include <stddef.h>
#include <string.h>

#include "cekirdek.h"
#include "gorev/gorev_ic.h"
#include "mat/ida_math.h"
#include "zaman/zaman.h"

static void gunluk_yaz(ida_gunluk_t *gun, int64_t now_ns, uint16_t kod, int32_t a0, int32_t a1)
{
    ida_log_t k;
    k.t_w.ns = now_ns;
    k.code = kod;
    k.severity = 0u;
    k.arg0 = a0;
    k.arg1 = a1;
    ida_gun_ekle(gun, &k);
}

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

static void desteksiz(ida_cmd_result_t *r)
{
    r->ack = (uint8_t)IDA_ACK_UNSUPPORTED;
    r->reason = (uint16_t)IDA_REASON_NONE;
}

/* ---- Kurulum, örnekler ------------------------------------------------------------------ */

void ida_gor_kur(ida_gor_t *g)
{
    (void)memset(g, 0, sizeof(*g));
    g->son_kip = (uint8_t)IDA_MODE_INIT;
    g->arm_engeli = true;
    g->fs_level = (uint8_t)IDA_FS_NONE;
    g->est_level = (uint8_t)IDA_EST_INIT;
    g->mission_state = (uint8_t)IDA_MS_IDLE;
}

/* Bozulma mandalı (sol bulgu 5; EMNİYET): durum yeniden kurulur, `bozuk` izi init'e kadar
   kalır (düzey DISARM: silahsızlandırır, istek KAPALI, arm yok); `bozuk_yeni` bu çağrının
   adımına bozulmanın görüldüğünü taşır (silahsızlandırma ve genişleticilere NARROWED). */
static void bozuk_mandalla(ida_gor_t *g)
{
    ida_gor_kur(g);
    g->bozuk = true;
    g->bozuk_yeni = true;
}

/* Adımdan önce durumu yazan kancalar (oturum başı, örnekler) önce tutarlılığı denetler: bir
   yazımın bozuk alanı geçerli değerle örtmesi bozulmanın kanıtını silmesin (sol 2. tur
   bulgu 2). Çerçevenin bütünlük denetimi aynı çağrıda core_ok'u ayrıca düşürür. */
static void yazmadan_once_denetle(ida_gor_t *g)
{
    if (!g->bozuk_yeni && !ida_gor_tutarli(g)) {
        bozuk_mandalla(g);
    }
}

void ida_gor_oturum_basi(ida_gor_t *g)
{
    yazmadan_once_denetle(g);
    /* A4-K72, 3.11 (b3): mandallı arıza eylemleri, silahlandırma ve kip değişmez. Bu modülün
       süreleri duvar saatindedir; oturumlar arasında süreklidir. Ölçüm zamanına bağlı tek
       durum seyrüsefer görüntüsünden alınan tutma hedefleridir: kestirim yeniden başladığı
       için geçersiz sayılır (EMNİYET; yeniden silahlanma onları yeniden yakalar). */
    g->tut_derinlik_var = false;
    g->tut_yon_var = false;
    g->tut_konum_var = false;
}

void ida_gor_ornek(ida_gor_t *g, const ida_sample_t *s)
{
    yazmadan_once_denetle(g);
    switch (s->hdr.type) {
    case IDA_SENS_BATT:
        g->batt_var = true;
        g->batt_v = s->u.batt.voltage;
        g->batt_rx_ns = s->hdr.t_rx.ns;
        break;
    case IDA_SENS_LEAK:
        if (s->hdr.inst < IDA_PORT_MAX_INST) {
            const uint32_t i = (uint32_t)s->hdr.inst;
            uint8_t *n = &g->sizinti_ardisik[i];
            if (s->u.leak.mask != 0u) {
                if (*n < UINT8_MAX) {
                    *n = (uint8_t)(*n + 1u);
                }
            } else {
                *n = 0u;
            }
            if (*n > g->sizinti_tepe[i]) {
                g->sizinti_tepe[i] = *n; /* çağrı içi eşik aşımı korunur (sol bulgu 2) */
            }
            g->sizinti_var[i] = true;
            g->sizinti_rx_ns[i] = s->hdr.t_rx.ns;
        }
        break;
    default:
        break;
    }
}

/* ---- Seyrüsefer görüntüsü ve tutma hedefleri ------------------------------------------- */

static void nav_goruntusu(ida_gor_t *g, const ida_gor_girdi_t *gi)
{
    /* W3-K5: kipler kestirimin genel düzeyine değil bileşenlerin geçerliliğine bakar. */
    const uint8_t v = gi->gecerlilik;
    g->gecerlilik = v;
    g->nav_tutum_var = (v & IDA_KES_G_TUTUM) != 0u;
    g->nav_yon_var = ((v & IDA_KES_G_YON) != 0u) && ida_sonlu(gi->nav_yon);
    g->nav_derinlik_var = ((v & IDA_KES_G_DERINLIK) != 0u) && ida_sonlu(gi->nav_derinlik);
    g->nav_konum_var = ida_gor_yatay_gecerli(v) && ida_sonlu(gi->nav_ne[0]) &&
                       ida_sonlu(gi->nav_ne[1]);
    g->nav_derinlik = g->nav_derinlik_var ? gi->nav_derinlik : 0.0f;
    g->nav_yon = g->nav_yon_var ? ida_aci_sar(gi->nav_yon) : 0.0f;
    g->nav_ne[0] = g->nav_konum_var ? gi->nav_ne[0] : 0.0f;
    g->nav_ne[1] = g->nav_konum_var ? gi->nav_ne[1] : 0.0f;
    g->yuzeyde = g->nav_derinlik_var &&
                 (g->nav_derinlik <= ida_prm_f32(gi->prm, IDA_PRM_gor_yuzey_d));
    g->yukselme_hizi = ida_prm_f32(gi->prm, IDA_PRM_gor_yukselme_hizi);
}

/* Tutma hedefleri bulunulan durumdan yakalanır: silahlanmada, kip değişiminde ve arıza eylemi
   düzeyi değişiminde (her iki yönde; eski hedefe geri dönülmez). */
static void tut_yakala(ida_gor_t *g)
{
    g->ref_kusak = g->ref_kusak + 1u; /* kontrol eski hedefin referansını bırakır */
    g->tut_derinlik_var = g->nav_derinlik_var;
    g->tut_derinlik = g->nav_derinlik;
    g->tut_yon_var = g->nav_yon_var;
    g->tut_yon = g->nav_yon;
    g->tut_konum_var = g->nav_konum_var;
    g->tut_ne[0] = g->nav_ne[0];
    g->tut_ne[1] = g->nav_ne[1];
}

/* Görevin hedef iptal bitini temizler: yalnız tutma hedefleri yeniden yakalandıktan sonra
   çağrılır (EMNİYET; ida_emn_t.hedef_iptal). */
static void tut_iptal_temizle(ida_emn_t *yetki)
{
    yetki->hedef_iptal = (uint8_t)(yetki->hedef_iptal & (uint8_t)(0xFFu ^ IDA_EMN_IPTAL_GOR));
}

/* ---- Arm ön koşulu (A4 §3.17 ARM satırı; W3-K3) ---------------------------------------- */

static bool arm_engeli_hesapla(const ida_gor_t *g, const ida_emn_t *yetki)
{
    if ((yetki->mode == (uint8_t)IDA_MODE_INIT) || (yetki->mode >= IDA_GOR_KIP_SAYI) ||
        g->bozuk) {
        return true; /* çekirdek başlatması bitmedi (A4-K127) ya da durum bozuldu */
    }
    if ((g->est_level != (uint8_t)IDA_EST_OK) && (g->est_level != (uint8_t)IDA_EST_DEGRADED)) {
        return true; /* kestirim düzeyi (W3-K3: OK ya da DEGRADED yeterli) */
    }
    if (g->fs_level >= (uint8_t)IDA_FS_HOLD) {
        return true; /* arıza eylemi */
    }
    const uint32_t aktif = g->etkin | g->mandal;
    for (uint32_t k = 0u; k < (uint32_t)IDA_GOR_K_SAYI; k++) {
        if (((aktif & IDA_GOR_K_BIT(k)) != 0u) && ida_gor_ariza_arm_reddi((uint8_t)k)) {
            return true; /* arm reddi gerektiren arıza kaydı */
        }
    }
    return false;
}

/* ---- Adım -------------------------------------------------------------------------------- */

void ida_gor_adim_ic(ida_gor_t *g, ida_emn_t *yetki, const ida_gor_girdi_t *gi,
                     ida_gunluk_t *gun)
{
    const int64_t now = gi->now_ns;
    /* Durum bozulması (sol bulgu 5; EMNİYET): tanımsız değer olağan hesapla örtülmez. Durum
       yeniden kurulur ve init'e kadar bozuk izi kalır: düzey DISARM (silahsızlandırır, istek
       kapalı), arm yok; düzeyin yükselmesi bu çağrının genişleticilerini de engeller. */
    const bool bozulma = g->bozuk_yeni || !ida_gor_tutarli(g);
    if (bozulma) {
        bozuk_mandalla(g);
    }
    g->bozuk_yeni = false;
    if (!g->taban_var) {
        g->taban_var = true;
        g->gcs_son_ns = now; /* HEARTBEAT süresinin tabanı */
    }
    g->est_level = gi->est_level;
    g->imu_lag = gi->imu_lag;
    nav_goruntusu(g, gi);
    const bool armed0 = yetki->armed;
    const uint8_t onceki_fs = g->fs_level;

    /* 1. Kip: tanımsız kip silahsız (EMNİYET); INIT'ten çıkış; ön koşul düşmeleri. */
    const uint8_t kip0 = yetki->mode;
    if (yetki->mode >= IDA_GOR_KIP_SAYI) {
        (void)ida_gor_kip_uygula(yetki, (uint8_t)IDA_GOR_OLAY_HAZIR, gun, now);
    } else {
        if ((yetki->mode == (uint8_t)IDA_MODE_INIT) && (gi->est_level != (uint8_t)IDA_EST_INIT)) {
            (void)ida_gor_kip_uygula(yetki, (uint8_t)IDA_GOR_OLAY_HAZIR, gun, now);
        }
        if (!g->nav_konum_var) {
            (void)ida_gor_kip_uygula(yetki, (uint8_t)IDA_GOR_OLAY_KONUM_YOK, gun, now);
        }
        if ((yetki->mode == (uint8_t)IDA_MODE_AUTO) &&
            (g->mission_state != (uint8_t)IDA_MS_RUNNING)) {
            (void)ida_gor_kip_uygula(yetki, (uint8_t)IDA_GOR_OLAY_GOREV_YOK, gun, now);
        }
    }

    /* Bu adımdaki kip değişimi INIT'ten çıkış değilse daraltmadır (W3-K2). */
    const bool kip_dustu = (yetki->mode != kip0) && (kip0 != (uint8_t)IDA_MODE_INIT);

    /* 2. Arıza eylemi (kipin son hâline göre). */
    ida_gor_ariza_degerlendir(g, yetki, gi, gun);
    g->dalis_yasak = (g->etkin & IDA_GOR_K_BIT(IDA_GOR_K_SIZINTI_BAYAT)) != 0u;
    if (g->dalis_yasak && !g->onceki_dalis_yasak) {
        g->ref_kusak = g->ref_kusak + 1u; /* derinlik hedefi bulunulan derinliğe kırpılır */
    }
    g->onceki_dalis_yasak = g->dalis_yasak;

    /* 3. Silahsızlandırma (kısıtlayan): DISARM arıza eylemi. Sanal emniyet girdileri araç
       silahlandırmasına girmez (W3-K1). */
    if (yetki->armed && (g->fs_level >= (uint8_t)IDA_FS_DISARM)) {
        yetki->armed = false;
        ida_emn_kusak_artir(yetki);
        gunluk_yaz(gun, now, (uint16_t)IDA_LOG_ARM_CHANGE, 0, (int32_t)IDA_REASON_NOT_ARMABLE);
    }
    /* Bu adımın arıza daraltması (W3-K2): eylem ağırlaştı, silahsızlandırıldı ya da kip düştü.
       Aynı çağrının genişleticileri etkisizdir (ida_gor_komut_ic). */
    g->adim_daralt =
        (g->fs_level > onceki_fs) || (armed0 && !yetki->armed) || kip_dustu || bozulma;

    /* 4. Tutma hedefleri: kip, arıza düzeyi ya da daraltma kuşağı değiştiyse (reddedilen ya
       da out NULL çağrıdaki silahsızlandırma ve kip daralması dahil; EMNİYET) bulunulan
       durumdan yeniden yakalanır. */
    if ((yetki->mode != g->son_kip) || (g->fs_level != onceki_fs) ||
        ((yetki->hedef_iptal & IDA_EMN_IPTAL_GOR) != 0u)) {
        tut_yakala(g);
        tut_iptal_temizle(yetki);
    }
    g->son_kip = yetki->mode;

    /* 5. Arm ön koşulunun ve kip kararının anlık görüntüsü (aynı çağrının komutları
       sırasından bağımsız). */
    g->arm_engeli = arm_engeli_hesapla(g, yetki);
}

static void girdi_topla(const ida_core_t *c, ida_gor_girdi_t *gi)
{
    ida_wtime_t now;
    now.ns = c->cer.now_izi_ns; /* kullanılan now_w (A4-K10; cerceve.c now_guncelle) */
    gi->now_ns = now.ns;
    gi->est_level = ida_kes_duzey(&c->kes);
    gi->suzgec_duzey = ida_kes_suzgec_duzey(&c->kes);
    gi->gecerlilik = ida_kes_gecerlilik(&c->kes); /* W3-K5 */
    gi->imu_bayat = ida_imz_bayat(&c->imz, now, &c->prm);
    gi->imu_lag = c->imz.lag;
    gi->param_ok = c->prm.param_ok;
    ida_nav_out_t nav;
    (void)memset(&nav, 0, sizeof(nav));
    ida_stime_t t;
    t.ns = c->cer.tf_t_ns;
    ida_kes_nav(&c->kes, t, &nav);
    gi->nav_derinlik = nav.depth;
    /* Yön: q_nb (w, x, y, z; gövde → NED) → ψ = atan2(2(wz + xy), 1 − 2(y² + z²)). */
    const float w = nav.q_nb[0];
    const float x = nav.q_nb[1];
    const float y = nav.q_nb[2];
    const float z = nav.q_nb[3];
    gi->nav_yon = ida_atan2(2.0f * ((w * z) + (x * y)), 1.0f - (2.0f * ((y * y) + (z * z))));
    gi->nav_ne[0] = nav.p_ned[0];
    gi->nav_ne[1] = nav.p_ned[1];
    gi->prm = &c->prm;
}

void ida_gor_adim(ida_gor_t *g, ida_emn_t *yetki, ida_core_t *c)
{
    ida_gor_girdi_t gi;
    girdi_topla(c, &gi);
    ida_gor_adim_ic(g, yetki, &gi, &c->gun);
}

/* ---- Komutlar (tablo 3.17) --------------------------------------------------------------- */

static void arm_komutu(ida_gor_t *g, ida_emn_t *yetki, const ida_komut_baglam_t *b,
                       ida_cmd_result_t *r, ida_gunluk_t *gun)
{
    (void)b;
    if (g->arm_engeli) {
        ret(r, (uint16_t)IDA_REASON_NOT_ARMABLE);
        return;
    }
    if (!yetki->armed) {
        yetki->armed = true;
        gunluk_yaz(gun, b->now_w.ns, (uint16_t)IDA_LOG_ARM_CHANGE, 1, 0);
        tut_yakala(g);
    }
    kabul(r); /* zaten silahlıysa etkisiz (3.17) */
}

static void kip_komutu(ida_gor_t *g, ida_emn_t *yetki, const ida_komut_baglam_t *b,
                       const ida_cmd_t *cmd, ida_cmd_result_t *r, ida_gunluk_t *gun)
{
    const uint8_t hedef = cmd->u.set_mode.mode;
    if ((hedef != (uint8_t)IDA_MODE_GUIDED) && (hedef != (uint8_t)IDA_MODE_AUTO)) {
        /* Daraltıcı hedefler buraya gelmez ((b)'de uygulanır, A4-K46). Tanımsız komut
           etkisizdir (EMNİYET). */
        ret(r, (uint16_t)IDA_REASON_BAD_MODE);
        return;
    }
    const uint8_t olay = (hedef == (uint8_t)IDA_MODE_GUIDED) ? (uint8_t)IDA_GOR_OLAY_GUIDED
                                                              : (uint8_t)IDA_GOR_OLAY_AUTO;
    const uint8_t h = ida_gor_kip_gecis(yetki->mode, olay);
    if (h == (uint8_t)IDA_GOR_KIP_SILAHSIZ) {
        (void)ida_gor_kip_uygula(yetki, olay, gun, b->now_w.ns); /* tanımsız kip: silahsız */
        g->son_kip = yetki->mode;
        tut_yakala(g);
        tut_iptal_temizle(yetki);
        ret(r, (uint16_t)IDA_REASON_BAD_MODE);
        return;
    }
    const bool gorev_hazir = (g->mission_state == (uint8_t)IDA_MS_LOADED) ||
                             (g->mission_state == (uint8_t)IDA_MS_PAUSED);
    /* W3-K7: GUIDED tutum, yön ve derinlik ister (yatay istemez); AUTO ayrıca yatay
       geçerlilik (ida_gor_yatay_gecerli) ve yüklü görev ister. */
    const bool dikey = g->nav_tutum_var && g->nav_yon_var && g->nav_derinlik_var;
    const bool auto_kosul = g->nav_konum_var && gorev_hazir;
    /* Ret koşulları yalnız §3.17'nin SET_MODE satırıdır (A4-K124; W3-K13). */
    if ((h == (uint8_t)IDA_GOR_KIP_RET) || !dikey ||
        ((olay == (uint8_t)IDA_GOR_OLAY_AUTO) && !auto_kosul)) {
        ret(r, (uint16_t)IDA_REASON_BAD_MODE); /* hedef kipin ön koşulu (3.17) */
        return;
    }
    const uint8_t onceki = yetki->mode;
    (void)ida_gor_kip_uygula(yetki, olay, gun, b->now_w.ns);
    if (yetki->mode != onceki) {
        /* W3-K13 (sol bulgu 1): arıza düzeyi yeni kipin sütununa göre o anda yeniden
           hesaplanır (mandallar, düzey, FAILSAFE_CHANGE) ve arm ön koşulunun görüntüsü
           güncellenir; aynı çağrıda sonra gelen komutlar güncel düzeyi görür (sonraki ARM,
           yeni kipte arıza eylemi varsa NOT_ARMABLE). Daha önce kabul edilmiş ARM kendi
           anındaki koşullarla kalır: sonuç ardışık iki çağrıdakiyle aynıdır. */
        ida_gor_ariza_kip_degisti(g, yetki->mode, gun, b->now_w.ns);
        g->arm_engeli = arm_engeli_hesapla(g, yetki);
        tut_yakala(g);
        tut_iptal_temizle(yetki); /* kendi kip değişiminin mandalı: hedefler yeniden yakalandı */
    }
    g->son_kip = yetki->mode;
    kabul(r);
}

static void ariza_sifirla_komutu(ida_gor_t *g, const ida_emn_t *yetki, const ida_komut_baglam_t *b,
                                 ida_cmd_result_t *r, const ida_param_t *prm, ida_gunluk_t *gun)
{
    if (g->mandal == 0u) {
        kabul(r); /* temizlenecek mandallı eylem yok: etkisiz (3.17) */
        return;
    }
    /* EMNİYET: ya bütün mandallar temizlenir ya hiçbiri. */
    if (!ida_gor_ariza_temizlenebilir(g, yetki, prm, b->now_w.ns)) {
        ret(r, (uint16_t)IDA_REASON_NOT_ARMABLE); /* neden sürüyor (3.17) */
        return;
    }
    const uint8_t onceki = g->fs_level;
    g->mandal = 0u;
    ida_gor_ariza_yayinla(g, yetki->mode, gun, b->now_w.ns);
    if (g->fs_level != onceki) {
        tut_yakala(g);
    }
    kabul(r);
}

void ida_gor_komut_ic(ida_gor_t *g, ida_emn_t *yetki, const ida_komut_baglam_t *b,
                      const ida_cmd_t *cmd, ida_cmd_result_t *r, const ida_param_t *prm,
                      ida_gunluk_t *gun)
{
    const bool genisletici = (cmd->type == (uint8_t)IDA_CMD_ARM) ||
                             (cmd->type == (uint8_t)IDA_CMD_SET_MODE) ||
                             (cmd->type == (uint8_t)IDA_CMD_FAILSAFE_RESET);
    if (genisletici && (yetki->d.herhangi || b->daraldi || g->adim_daralt)) {
        /* (b) daraltması: dağıtıcı bu çağrıda genişleticiyi buraya getirmez (A4-K45; savunma).
           Bu çağrının adımındaki arıza daraltması (W3-K2): genişletici etkisiz. */
        ret(r, (uint16_t)IDA_REASON_NARROWED);
        return;
    }
    switch (cmd->type) {
    case IDA_CMD_HEARTBEAT:
        /* A4-K52: canlılık yalnız kabul edilen HEARTBEAT'in now_w'sinden izlenir. */
        g->gcs_var = true;
        g->gcs_son_ns = b->now_w.ns;
        kabul(r);
        break;
    case IDA_CMD_ARM:
        arm_komutu(g, yetki, b, r, gun);
        break;
    case IDA_CMD_SET_MODE:
        kip_komutu(g, yetki, b, cmd, r, gun);
        break;
    case IDA_CMD_FAILSAFE_RESET:
        ariza_sifirla_komutu(g, yetki, b, r, prm, gun);
        break;
    default:
        /* MISSION_* dilim 2'dedir; bilinmeyen tür etkisizdir (A4-K124). */
        desteksiz(r);
        break;
    }
}

void ida_gor_komut(ida_core_t *c, const ida_komut_baglam_t *b, const ida_cmd_t *cmd,
                   ida_cmd_result_t *r)
{
    const uint32_t mandal0 = c->gor.mandal;
    ida_gor_komut_ic(&c->gor, &c->emn, b, cmd, r, &c->prm, &c->gun);
    /* FAILSAFE_RESET kabulü kestirimin sayısal çökme mandalını da kaldırır (02 §2.3.6;
       kestirim.h ida_kes_ariza_temizle). EMNİYET: yalnız bu komut mandallı arıza eylemlerini
       gerçekten temizlediyse (etkisiz kabulde değil); kestirim mandalı ancak filtre yeniden
       hizalanmışsa kalkar. */
    if ((cmd->type == (uint8_t)IDA_CMD_FAILSAFE_RESET) &&
        (r->ack == (uint8_t)IDA_ACK_ACCEPTED) && (mandal0 != 0u) && (c->gor.mandal == 0u)) {
        (void)ida_kes_ariza_temizle(&c->kes);
    }
}

/* ---- Durum, tutarlılık, parametre grubu ------------------------------------------------- */

bool ida_gor_daraldi(const ida_gor_t *g)
{
    return g->adim_daralt;
}

uint8_t ida_gor_fs_duzey(const ida_gor_t *g)
{
    return g->fs_level;
}

uint32_t ida_gor_fs_nedenleri(const ida_gor_t *g)
{
    return g->fs_nedenleri;
}

uint8_t ida_gor_itici_disi(const ida_gor_t *g)
{
    return g->itici_disi;
}

void ida_gor_durum(const ida_gor_t *g, ida_status_t *st)
{
    if (g->nav_derinlik_var) {
        /* Geçici rejim ve aşama: derinlik eşiğinden (YUZEY_D); güdüm aşamaları dilim 2. */
        st->regime = g->yuzeyde ? (uint8_t)IDA_REG_SURFACE : (uint8_t)IDA_REG_SUBMERGED;
        if (g->yuzeyde) {
            st->phase = (uint8_t)IDA_PH_SURFACE_TRANSIT;
        } else if (g->fs_level == (uint8_t)IDA_FS_SURFACE) {
            st->phase = (uint8_t)IDA_PH_ASCENT;
        } else {
            st->phase = (uint8_t)IDA_PH_SUBMERGED;
        }
    } else {
        st->regime = 0u;
        st->phase = 0u;
    }
    st->mission_state = g->mission_state;
    st->mission_item = g->mission_item;
}

_Static_assert(sizeof(bool) == 1u, "bool tek bayt");

/* bool nesnesinin ham baytı 0 ya da 1 mi (A4-K89). */
static bool b_gecerli(const bool *b)
{
    uint8_t v = 0u;
    (void)memcpy(&v, b, sizeof(v));
    return v <= 1u;
}

bool ida_gor_tutarli(const ida_gor_t *g)
{
    const bool bool_gecerli =
        b_gecerli(&g->arm_engeli) && b_gecerli(&g->taban_var) && b_gecerli(&g->gcs_var) &&
        b_gecerli(&g->batt_var) && b_gecerli(&g->batt_dusuk_say) &&
        b_gecerli(&g->batt_krit_say) && b_gecerli(&g->sizinti_var[0]) &&
        b_gecerli(&g->sizinti_var[1]) && b_gecerli(&g->sizinti_var[2]) &&
        b_gecerli(&g->sizinti_var[3]) && b_gecerli(&g->bozuk) && b_gecerli(&g->bozuk_yeni) &&
        b_gecerli(&g->est_ok_say) && b_gecerli(&g->konum_say) &&
        b_gecerli(&g->konum_yok_izi) && b_gecerli(&g->adim_daralt) &&
        b_gecerli(&g->est_kotu_say) && b_gecerli(&g->imu_taze_say) && b_gecerli(&g->imu_lag) &&
        b_gecerli(&g->nav_tutum_var) && b_gecerli(&g->nav_yon_var) &&
        b_gecerli(&g->nav_derinlik_var) && b_gecerli(&g->nav_konum_var) &&
        b_gecerli(&g->yuzeyde) && b_gecerli(&g->dalis_yasak) &&
        b_gecerli(&g->tut_derinlik_var) && b_gecerli(&g->tut_yon_var) &&
        b_gecerli(&g->tut_konum_var) && b_gecerli(&g->onceki_dalis_yasak);
    if (!bool_gecerli) {
        return false;
    }
    const uint32_t nedenler = IDA_FSR_LEAK | IDA_FSR_GCS_LOSS | IDA_FSR_BATT_LOW |
                              IDA_FSR_BATT_CRIT | IDA_FSR_EST_DEGRADED | IDA_FSR_EST_BAD |
                              IDA_FSR_NUMERIC | IDA_FSR_IMU_STALE | IDA_FSR_PARAM;
    return (g->son_kip < IDA_GOR_KIP_SAYI) && (g->fs_level <= (uint8_t)IDA_FS_DISARM) &&
           ((g->fs_nedenleri & ~nedenler) == 0u) && ((g->etkin & ~IDA_GOR_K_TANIMLI) == 0u) &&
           ((g->mandal & ~IDA_GOR_K_TANIMLI) == 0u) && (g->est_level <= (uint8_t)IDA_EST_FAULT) &&
           (g->mission_state <= (uint8_t)IDA_MS_ABORTED);
}

static float ham_f32(uint32_t ham)
{
    float f = 0.0f;
    (void)memcpy(&f, &ham, sizeof(f));
    return f;
}

uint8_t ida_gor_param_grup(const ida_prm_kume_t *p)
{
    const bool batt = ham_f32(p->ham[IDA_PRM_gor_batt_krit_v]) <
                      ham_f32(p->ham[IDA_PRM_gor_batt_dusuk_v]);
    const bool gcs = p->ham[IDA_PRM_gor_gcs1_ms] < p->ham[IDA_PRM_gor_gcs2_ms];
    return (batt && gcs) ? (uint8_t)0u : (uint8_t)1u;
}
