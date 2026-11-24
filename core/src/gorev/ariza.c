/*
 * ariza.c — arıza güvenliği: koşullar, kip × arıza tablosu, mandallar (02 §4.1–§4.3;
 * A4-K39, K52, K55, K76, K108).
 *
 * Eylem yalnız kısıtlayan yöndedir: bu dosya `armed`'a ve `mode`'a hiç yazmaz; yalnız arıza
 * eylemi düzeyini (fs_level) ve nedenlerini kurar. Silahsızlandırma ve kip düşmesi gorev.c'de,
 * yine yalnız kısıtlayan yönde uygulanır. Etkin düzey, etkin ve mandallı koşulların bulunulan
 * kip sütunundaki en ağır hücresidir (02 §4.2: "en ağırı uygulanır").
 *
 * Saat kuralı (iskelet §4): koşulların verisi örneklerden, süreleri duvar saatindedir
 * (kullanılan now_w ve örneklerin alım damgası t_rx). Ölçüm zamanı kullanılmaz; bu yüzden
 * SESSION_START'ta sıfırlanacak bir ölçüm zamanı izi yoktur (A4-K72).
 */
#include "gorev/gorev_ic.h"

#include "zaman/zaman.h"

#define N_ IDA_FS_NONE
#define W_ IDA_FS_WARN
#define H_ IDA_FS_HOLD
#define S_ IDA_FS_SURFACE
#define P_ IDA_FS_PASSIVE_SURFACE

_Static_assert(IDA_GOR_K_SAYI == 14, "arıza tablosunun satır sayısı");
_Static_assert(IDA_GOR_K_SAYI <= 32, "koşul bitleri uint32_t'ye sığar");

/* Sütunlar kip sırasıyla: INIT, DISARMED, MANUAL, STABILIZE, DEPTH_HOLD, STATION_KEEP, GUIDED,
   AUTO. 02 §4.3'ün sütunları: M = MANUAL/STABILIZE, T = DEPTH_HOLD/STATION_KEEP, G, A,
   D = DISARMED. INIT matriste yoktur: INIT'te arm yolu yoktur (ARM NOT_ARMABLE) ve eylem
   yoktur; koşullar sayılır, düzey ve neden üretmez (okuma, rapora). D sütunundaki "arm
   reddi" hücreleri WARN'dur (operatöre görünür). */
static const uint8_t ARIZA_TABLOSU[IDA_GOR_K_SAYI][IDA_GOR_KIP_SAYI] = {
    /*                  INIT DIS MAN STB DH  SK  GD  AT */
    /* SIZINTI       */ {N_, W_, S_, S_, S_, S_, S_, S_},
    /* SIZINTI_BAYAT */ {N_, W_, W_, W_, W_, W_, W_, W_},
    /* GCS_1         */ {N_, N_, H_, H_, W_, W_, H_, N_},
    /* GCS_2         */ {N_, N_, S_, S_, S_, S_, S_, S_}, /* AUTO: yalnız yüzeyde (aşağıda) */
    /* BATT_DUSUK    */ {N_, W_, W_, W_, W_, W_, W_, S_},
    /* BATT_KRIT     */ {N_, W_, S_, S_, S_, S_, S_, S_},
    /* BATT_KRIT_2   */ {N_, W_, P_, P_, P_, P_, P_, P_},
    /* BATT_BAYAT    */ {N_, W_, W_, W_, W_, W_, W_, W_},
    /* EST_BOZUK     */ {N_, N_, N_, N_, N_, N_, N_, H_}, /* yatay yok; SK: DH'ye iner */
    /* EST_BOZUK_UZUN*/ {N_, N_, N_, N_, N_, S_, N_, S_}, /* yalnız yatay kipleri (W3-K5, K7) */
    /* EST_KOTU      */ {N_, W_, W_, P_, P_, P_, P_, P_},
    /* SAYISAL       */ {N_, W_, P_, P_, P_, P_, P_, P_},
    /* IMU_BAYAT     */ {N_, N_, P_, P_, P_, P_, P_, P_},
    /* PARAM         */ {N_, W_, N_, N_, N_, N_, N_, N_},
};

/* Koşulun özellikleri: neden biti, mandallı mı, arm reddi gerektirir mi. */
typedef struct {
    uint32_t neden;
    bool mandalli;
    bool arm_reddi;
} kosul_ozellik_t;

static const kosul_ozellik_t OZELLIK[IDA_GOR_K_SAYI] = {
    /* SIZINTI        */ {IDA_FSR_LEAK, true, true},
    /* SIZINTI_BAYAT  */ {IDA_FSR_LEAK, false, true},     /* W4-K13: veri yokken arm reddi */
    /* GCS_1          */ {IDA_FSR_GCS_LOSS, false, false},
    /* GCS_2          */ {IDA_FSR_GCS_LOSS, false, false},
    /* BATT_DUSUK     */ {IDA_FSR_BATT_LOW, true, true}, /* W3-K3 */
    /* BATT_KRIT      */ {IDA_FSR_BATT_CRIT, true, true},
    /* BATT_KRIT_2    */ {IDA_FSR_BATT_CRIT, true, true},
    /* BATT_BAYAT     */ {IDA_FSR_BATT_LOW, false, true}, /* W4-K13: veri yokken arm reddi */
    /* EST_BOZUK      */ {IDA_FSR_EST_DEGRADED, false, false},
    /* EST_BOZUK_UZUN */ {IDA_FSR_EST_DEGRADED, false, false},
    /* EST_KOTU       */ {IDA_FSR_EST_BAD, true, true},
    /* SAYISAL        */ {IDA_FSR_NUMERIC, true, true},
    /* IMU_BAYAT      */ {IDA_FSR_IMU_STALE, true, false},
    /* PARAM          */ {IDA_FSR_PARAM, false, true},
};

uint8_t ida_gor_ariza_hucre(uint8_t kosul, uint8_t kip)
{
    if ((kosul >= (uint8_t)IDA_GOR_K_SAYI) || (kip >= IDA_GOR_KIP_SAYI)) {
        return (uint8_t)IDA_FS_DISARM; /* EMNİYET: tanımsız → en ağır */
    }
    return ARIZA_TABLOSU[kosul][kip];
}

uint32_t ida_gor_ariza_nedeni(uint8_t kosul)
{
    return (kosul < (uint8_t)IDA_GOR_K_SAYI) ? OZELLIK[kosul].neden : 0u;
}

bool ida_gor_ariza_mandalli(uint8_t kosul)
{
    return (kosul < (uint8_t)IDA_GOR_K_SAYI) && OZELLIK[kosul].mandalli;
}

bool ida_gor_ariza_arm_reddi(uint8_t kosul)
{
    return (kosul >= (uint8_t)IDA_GOR_K_SAYI) || OZELLIK[kosul].arm_reddi;
}

/* ---- Yardımcılar ------------------------------------------------------------------------ */

static bool sure_doldu(int64_t now_ns, int64_t bas_ns, uint32_t ms)
{
    return (now_ns - bas_ns) >= ida_ms_ns(ms);
}

static bool taze(bool var, int64_t now_ns, int64_t rx_ns, uint32_t bayat_ms)
{
    return var && ((now_ns - rx_ns) <= ida_ms_ns(bayat_ms));
}

/* Süreli koşulun sayacı: koşul tutuyorsa başlangıç kurulur (ilk kez), tutmuyorsa silinir. */
static void say(bool kosul, bool *sayiyor, int64_t *bas_ns, int64_t now_ns)
{
    if (kosul) {
        if (!*sayiyor) {
            *sayiyor = true;
            *bas_ns = now_ns;
        }
    } else {
        *sayiyor = false;
    }
}

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

/* ---- Değerlendirme ---------------------------------------------------------------------- */

static void mandallari_kur(ida_gor_t *g, uint8_t kip, int64_t now);

/* Sızıntı algılayıcıları taze mi (sol bulgu 4): en az bir algılayıcıdan örnek alınmış ve
   örneği alınmış her algılayıcı kendi süresi içinde. */
static bool sizinti_taze(const ida_gor_t *g, const ida_param_t *p, int64_t now)
{
    const uint32_t bayat_ms = ida_prm_u32(p, IDA_PRM_gor_sizinti_bayat_ms);
    bool herhangi = false;
    for (uint32_t i = 0u; i < IDA_PORT_MAX_INST; i++) {
        if (g->sizinti_var[i]) {
            herhangi = true;
            if (!taze(true, now, g->sizinti_rx_ns[i], bayat_ms)) {
                return false;
            }
        }
    }
    return herhangi;
}

static uint32_t batarya_kosullari(ida_gor_t *g, const ida_param_t *p, int64_t now)
{
    uint32_t e = 0u;
    if (!taze(g->batt_var, now, g->batt_rx_ns, ida_prm_u32(p, IDA_PRM_gor_batt_bayat_ms))) {
        /* Bayat ölçer düşük gerilim sayacını silmez (kısıtlayan yön); yalnız taze ve eşik
           üstündeki bir örnek siler. */
        e |= IDA_GOR_K_BIT(IDA_GOR_K_BATT_BAYAT);
    } else {
        say(g->batt_v < ida_prm_f32(p, IDA_PRM_gor_batt_dusuk_v), &g->batt_dusuk_say,
            &g->batt_dusuk_bas_ns, now);
        say(g->batt_v < ida_prm_f32(p, IDA_PRM_gor_batt_krit_v), &g->batt_krit_say,
            &g->batt_krit_bas_ns, now);
    }
    const uint32_t sure = ida_prm_u32(p, IDA_PRM_gor_batt_sure_ms);
    if (g->batt_dusuk_say && sure_doldu(now, g->batt_dusuk_bas_ns, sure)) {
        e |= IDA_GOR_K_BIT(IDA_GOR_K_BATT_DUSUK);
    }
    if (g->batt_krit_say && sure_doldu(now, g->batt_krit_bas_ns, sure)) {
        e |= IDA_GOR_K_BIT(IDA_GOR_K_BATT_KRIT);
    }
    if (((g->mandal & IDA_GOR_K_BIT(IDA_GOR_K_BATT_KRIT)) != 0u) &&
        sure_doldu(now, g->batt_krit_tetik_ns, ida_prm_u32(p, IDA_PRM_gor_batt_krit2_ms))) {
        e |= IDA_GOR_K_BIT(IDA_GOR_K_BATT_KRIT_2);
    }
    return e;
}

/* Kestirim (W3-K3, K5, K7): düzey yeterliyse (OK ya da DEGRADED) EST_KOTU sayılmaz. Yatay
   geçerlilik kaybının (konum VE yatay hız; ida_gor_yatay_gecerli) zamanlayıcısı yalnız yatay
   geçerlilik gerektiren kipte (AUTO; STATION_KEEP ise DEPTH_HOLD'a iner) işler. GUIDED'ın
   konum hedefi (GUIDED_SP) sonraki iştedir; o gelince bu koşula eklenir. */
static uint32_t kestirim_kosullari(ida_gor_t *g, const ida_emn_t *yetki, const ida_param_t *p,
                                   const ida_gor_girdi_t *gi, int64_t now)
{
    uint32_t e = 0u;
    const uint8_t est = gi->est_level;
    const uint32_t ok_ms = ida_prm_u32(p, IDA_PRM_gor_est_ok_ms);
    /* Süzgecin kesintisiz sağlıklı olduğu süre (EST_KOTU ve SAYISAL mandallarının temizleme
       koşulu). Sayısal çökme mandalı kuruluyken yayımlanan düzey hep FAULT'tur; o durumda
       mandaldan bağımsız süzgeç düzeyi (ida_kes_suzgec_duzey) sayılır: yeniden hizalanmış ve
       sağlıklı süzgeç FAILSAFE_RESET'e izin verir, toparlanmamış süzgeç vermez (EMNİYET). */
    const uint8_t suz = gi->suzgec_duzey;
    const bool yeterli = (est == (uint8_t)IDA_EST_OK) || (est == (uint8_t)IDA_EST_DEGRADED);
    const bool suzgec_yeterli =
        (est == (uint8_t)IDA_EST_FAULT) &&
        ((suz == (uint8_t)IDA_EST_OK) || (suz == (uint8_t)IDA_EST_DEGRADED));
    say(yeterli || suzgec_yeterli, &g->est_ok_say, &g->est_ok_bas_ns, now);
    const bool konum = ida_gor_yatay_gecerli(gi->gecerlilik);
    const bool yatay_gerekli = (yetki->mode == (uint8_t)IDA_MODE_AUTO) ||
                               (yetki->mode == (uint8_t)IDA_MODE_STATION_KEEP);
    say(konum, &g->konum_say, &g->konum_bas_ns, now);
    if (yatay_gerekli && !konum) {
        if (!g->konum_yok_izi) {
            g->konum_yok_izi = true;
            g->konum_yok_bas_ns = now;
        }
    } else if (!yatay_gerekli || (g->konum_say && sure_doldu(now, g->konum_bas_ns, ok_ms))) {
        /* Yatay gerekmiyor ya da 02 §4.3: geçerli konum EST_OK_MS sürdü. */
        g->konum_yok_izi = false;
    }
    if (g->konum_yok_izi) {
        e |= IDA_GOR_K_BIT(IDA_GOR_K_EST_BOZUK);
        if (sure_doldu(now, g->konum_yok_bas_ns, ida_prm_u32(p, IDA_PRM_gor_est_bozuk_ms))) {
            e |= IDA_GOR_K_BIT(IDA_GOR_K_EST_BOZUK_UZUN);
        }
    }
    /* INIT dışı kipte kestirimin yeniden INIT'e dönmesi kötü sayılır (EMNİYET). Tanımsız
       düzey de kötüdür. */
    const bool kotu = (est == (uint8_t)IDA_EST_BAD) || (est >= (uint8_t)IDA_EST_FAULT) ||
                      ((est == (uint8_t)IDA_EST_INIT) && (yetki->mode != (uint8_t)IDA_MODE_INIT));
    say(kotu, &g->est_kotu_say, &g->est_kotu_bas_ns, now);
    if (g->est_kotu_say &&
        sure_doldu(now, g->est_kotu_bas_ns, ida_prm_u32(p, IDA_PRM_gor_est_kotu_ms))) {
        e |= IDA_GOR_K_BIT(IDA_GOR_K_EST_KOTU);
    }
    if (est >= (uint8_t)IDA_EST_FAULT) {
        e |= IDA_GOR_K_BIT(IDA_GOR_K_SAYISAL); /* beklemesiz (02 §4.3) */
    }
    return e;
}

void ida_gor_ariza_degerlendir(ida_gor_t *g, const ida_emn_t *yetki, const ida_gor_girdi_t *gi,
                               ida_gunluk_t *gun)
{
    const ida_param_t *p = gi->prm;
    const int64_t now = gi->now_ns;
    uint32_t e = 0u;

    /* Sızıntı: herhangi bir algılayıcıda SIZINTI_N ardışık ıslak örnek. Son değerlendirmeden
       beri görülen en büyük ardışık sayı (tepe) kullanılır: aynı çağrıdaki sonraki kuru örnek
       eşik aşımını silmez (sol bulgu 2). */
    const uint32_t sizinti_n = ida_prm_u32(p, IDA_PRM_gor_sizinti_n);
    for (uint32_t i = 0u; i < IDA_PORT_MAX_INST; i++) {
        if ((uint32_t)g->sizinti_tepe[i] >= sizinti_n) {
            e |= IDA_GOR_K_BIT(IDA_GOR_K_SIZINTI);
        }
    }
    if (!sizinti_taze(g, p, now)) {
        e |= IDA_GOR_K_BIT(IDA_GOR_K_SIZINTI_BAYAT);
    }
    /* Yer istasyonu (A4-K52): HEARTBEAT hiç gelmediyse taban ilk adımın now_w'sidir. */
    if (sure_doldu(now, g->gcs_son_ns, ida_prm_u32(p, IDA_PRM_gor_gcs1_ms))) {
        e |= IDA_GOR_K_BIT(IDA_GOR_K_GCS_1);
    }
    if (sure_doldu(now, g->gcs_son_ns, ida_prm_u32(p, IDA_PRM_gor_gcs2_ms))) {
        e |= IDA_GOR_K_BIT(IDA_GOR_K_GCS_2);
    }
    e |= batarya_kosullari(g, p, now);
    e |= kestirim_kosullari(g, yetki, p, gi, now);
    /* IMU (A4-K39; çerçevenin bayatlık hesabı) */
    if (gi->imu_bayat) {
        e |= IDA_GOR_K_BIT(IDA_GOR_K_IMU_BAYAT);
    }
    say(!gi->imu_bayat, &g->imu_taze_say, &g->imu_taze_bas_ns, now);
    if (!gi->param_ok) {
        e |= IDA_GOR_K_BIT(IDA_GOR_K_PARAM);
    }
    g->etkin = e;

    /* Mandal: mandallı koşul, bulunulan kip sütununda bir eylem (≥ WARN) ürettiğinde kurulur
       ve yalnız FAILSAFE_RESET'le temizlenir (02 §4.3 "Temizleme" sütunu). */
    const uint8_t kip = yetki->mode;
    mandallari_kur(g, kip, now);
    /* Tepe, eşik aşımı değerlendirildikten sonra bugünkü sayıya iner; aşım mandala alınamadıysa
       (INIT sütunu) korunur ki INIT'ten çıkınca mandala alınsın (EMNİYET). */
    const bool sizinti_bekliyor = ((e & IDA_GOR_K_BIT(IDA_GOR_K_SIZINTI)) != 0u) &&
                                  ((g->mandal & IDA_GOR_K_BIT(IDA_GOR_K_SIZINTI)) == 0u);
    if (!sizinti_bekliyor) {
        for (uint32_t i = 0u; i < IDA_PORT_MAX_INST; i++) {
            g->sizinti_tepe[i] = g->sizinti_ardisik[i];
        }
    }
    ida_gor_ariza_yayinla(g, kip, gun, now);
}

static void mandallari_kur(ida_gor_t *g, uint8_t kip, int64_t now)
{
    for (uint32_t k = 0u; k < (uint32_t)IDA_GOR_K_SAYI; k++) {
        const uint32_t bit = IDA_GOR_K_BIT(k);
        if (((g->etkin & bit) != 0u) && OZELLIK[k].mandalli &&
            (ida_gor_ariza_hucre((uint8_t)k, kip) >= (uint8_t)IDA_FS_WARN) &&
            ((g->mandal & bit) == 0u)) {
            g->mandal |= bit;
            if (k == (uint32_t)IDA_GOR_K_BATT_KRIT) {
                g->batt_krit_tetik_ns = now;
            }
        }
    }
}

void ida_gor_ariza_kip_degisti(ida_gor_t *g, uint8_t kip, ida_gunluk_t *gun, int64_t now_ns)
{
    mandallari_kur(g, kip, now_ns);
    ida_gor_ariza_yayinla(g, kip, gun, now_ns);
}

uint8_t ida_gor_ariza_duzey(const ida_gor_t *g, uint32_t aktif, uint8_t kip, uint32_t *nedenler)
{
    uint8_t duzey = (uint8_t)IDA_FS_NONE;
    uint32_t neden = 0u;
    for (uint32_t k = 0u; k < (uint32_t)IDA_GOR_K_SAYI; k++) {
        if ((aktif & IDA_GOR_K_BIT(k)) == 0u) {
            continue;
        }
        uint8_t h = ida_gor_ariza_hucre((uint8_t)k, kip);
        /* AUTO × GCS_2: "su altında yok; yüzeyde SURFACE" (02 §4.3). Derinlik bilinmiyorsa
           yüzeyde sayılır (EMNİYET: eylem uygulanır). */
        if ((k == (uint32_t)IDA_GOR_K_GCS_2) && (kip == (uint8_t)IDA_MODE_AUTO) &&
            g->nav_derinlik_var && !g->yuzeyde) {
            h = (uint8_t)IDA_FS_NONE;
        }
        if (h >= (uint8_t)IDA_FS_WARN) {
            neden |= OZELLIK[k].neden;
            if (h > duzey) {
                duzey = h;
            }
        }
    }
    if ((kip >= IDA_GOR_KIP_SAYI) || g->bozuk) {
        duzey = (uint8_t)IDA_FS_DISARM; /* EMNİYET: tanımsız kip ya da durum bozulması */
    }
    if (nedenler != NULL) {
        *nedenler = neden;
    }
    return duzey;
}

void ida_gor_ariza_yayinla(ida_gor_t *g, uint8_t kip, ida_gunluk_t *gun, int64_t now_ns)
{
    uint32_t neden = 0u;
    const uint8_t duzey = ida_gor_ariza_duzey(g, g->etkin | g->mandal, kip, &neden);
    const bool degisti = (duzey != g->fs_level);
    g->fs_level = duzey;
    g->fs_nedenleri = neden;
    if (degisti) {
        const uint32_t tum = neden | (g->imu_lag ? IDA_FSR_IMU_LAG : 0u);
        gunluk_yaz(gun, now_ns, (uint16_t)IDA_LOG_FAILSAFE_CHANGE, (int32_t)duzey, (int32_t)tum);
    }
}

bool ida_gor_ariza_temizlenebilir(const ida_gor_t *g, const ida_emn_t *yetki,
                                  const ida_param_t *p, int64_t now_ns)
{
    const uint32_t m = g->mandal;
    if ((m & IDA_GOR_K_BIT(IDA_GOR_K_SIZINTI)) != 0u) {
        /* 02 §4.3: devre dışı + FAILSAFE_RESET; ayrıca son taze örnek kuru olmalı. */
        if (yetki->armed || !sizinti_taze(g, p, now_ns)) {
            return false;
        }
        for (uint32_t i = 0u; i < IDA_PORT_MAX_INST; i++) {
            if ((g->sizinti_ardisik[i] != 0u) || (g->sizinti_tepe[i] != 0u)) {
                return false;
            }
        }
    }
    const bool batt_taze =
        taze(g->batt_var, now_ns, g->batt_rx_ns, ida_prm_u32(p, IDA_PRM_gor_batt_bayat_ms));
    const float hist = ida_prm_f32(p, IDA_PRM_gor_batt_hist_v);
    if ((m & IDA_GOR_K_BIT(IDA_GOR_K_BATT_DUSUK)) != 0u) {
        if (!batt_taze || (g->batt_v < (ida_prm_f32(p, IDA_PRM_gor_batt_dusuk_v) + hist))) {
            return false;
        }
    }
    if ((m & (IDA_GOR_K_BIT(IDA_GOR_K_BATT_KRIT) | IDA_GOR_K_BIT(IDA_GOR_K_BATT_KRIT_2))) != 0u) {
        if (!batt_taze || (g->batt_v < (ida_prm_f32(p, IDA_PRM_gor_batt_krit_v) + hist))) {
            return false;
        }
    }
    if ((m & (IDA_GOR_K_BIT(IDA_GOR_K_EST_KOTU) | IDA_GOR_K_BIT(IDA_GOR_K_SAYISAL))) != 0u) {
        if (!(g->est_ok_say &&
              sure_doldu(now_ns, g->est_ok_bas_ns, ida_prm_u32(p, IDA_PRM_gor_est_ok_ms)))) {
            return false;
        }
    }
    if ((m & IDA_GOR_K_BIT(IDA_GOR_K_IMU_BAYAT)) != 0u) {
        if (!(g->imu_taze_say &&
              sure_doldu(now_ns, g->imu_taze_bas_ns, ida_prm_u32(p, IDA_PRM_gor_imu_taze_ms)))) {
            return false;
        }
    }
    return true;
}
