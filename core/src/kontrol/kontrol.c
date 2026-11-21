/*
 * kontrol.c — kontrol döngüleri ve itki tahsisi (W3 kontrol). Açıklama kontrol.h'tadır.
 */
#include "kontrol/kontrol.h"

#include <string.h>

#include "cekirdek.h"
#include "gorev/gorev.h"
#include "gudum/gudum.h"
#include "kestirim/kestirim.h"
#include "mat/ida_math.h"
#include "mat/mat.h"

#define KANAL_YOK      255u
#define DT_EN_BUYUK_NS 50000000LL /* 50 ms: daha uzun aralık integrale bu kadar girer   */
#define DT_EN_BUYUK    0.05f
#define ACI_EN_BUYUK   0.6f       /* rad: yalpa ve baş-kıç isteğinin kırpma sınırı       */
#define DOYMA_MUTLAK   1.0e-3f    /* kalıntı eşiği: mutlak + göreli · |istek|            */
#define DOYMA_GORELI   1.0e-3f

typedef union {
    uint32_t u;
    float f;
} bit32_t;

static float pf(const ida_prm_kume_t *p, enum ida_prm_indis i)
{
    bit32_t b;
    b.u = p->ham[i];
    return b.f;
}

typedef struct {
    float itki_egim;
    float lim_fxy, lim_fz, lim_mkm, lim_mn, lim_w, lim_v, lim_pq, lim_r;
    float ref_d_hiz, ref_psi_hiz;
    float d_kp, w_kp, w_ki, yuzerlik, w_sonum, v_sonum;
    float tutum_kp, yon_kp, pq_kp, pq_ki, r_kp, r_ki, v_kp, v_ki;
} kazanc_t;

static void kazanc_oku(const ida_prm_kume_t *p, kazanc_t *z)
{
    z->itki_egim = pf(p, IDA_PRM_kon_itki_egim);
    z->lim_fxy = pf(p, IDA_PRM_kon_lim_fxy);
    z->lim_fz = pf(p, IDA_PRM_kon_lim_fz);
    z->lim_mkm = pf(p, IDA_PRM_kon_lim_mkm);
    z->lim_mn = pf(p, IDA_PRM_kon_lim_mn);
    z->lim_w = pf(p, IDA_PRM_kon_lim_w);
    z->lim_v = pf(p, IDA_PRM_kon_lim_v);
    z->lim_pq = pf(p, IDA_PRM_kon_lim_pq);
    z->lim_r = pf(p, IDA_PRM_kon_lim_r);
    z->ref_d_hiz = pf(p, IDA_PRM_kon_ref_derinlik_hizi);
    z->ref_psi_hiz = pf(p, IDA_PRM_kon_ref_yon_hizi);
    z->d_kp = pf(p, IDA_PRM_kon_derinlik_kp);
    z->w_kp = pf(p, IDA_PRM_kon_w_kp);
    z->w_ki = pf(p, IDA_PRM_kon_w_ki);
    z->yuzerlik = pf(p, IDA_PRM_kon_yuzerlik_ff);
    z->w_sonum = pf(p, IDA_PRM_kon_w_sonum);
    z->v_sonum = pf(p, IDA_PRM_kon_v_sonum);
    z->tutum_kp = pf(p, IDA_PRM_kon_tutum_kp);
    z->yon_kp = pf(p, IDA_PRM_kon_yon_kp);
    z->pq_kp = pf(p, IDA_PRM_kon_pq_kp);
    z->pq_ki = pf(p, IDA_PRM_kon_pq_ki);
    z->r_kp = pf(p, IDA_PRM_kon_r_kp);
    z->r_ki = pf(p, IDA_PRM_kon_r_ki);
    z->v_kp = pf(p, IDA_PRM_kon_v_kp);
    z->v_ki = pf(p, IDA_PRM_kon_v_ki);
}

static float kirp(float x, float sinir)
{
    ida_aralik_t a;
    a.alt = -sinir;
    a.ust = sinir;
    return ida_kirp(x, a);
}

/* ---- Durum ------------------------------------------------------------------------------- */

static void cikti_sifirla(ida_kon_t *k)
{
    for (uint32_t i = 0u; i < IDA_PORT_N_THR; i++) {
        k->f[i] = 0.0f;
        k->thr[i] = 0.0f;
    }
    for (uint32_t j = 0u; j < IDA_TAH_EKSEN; j++) {
        k->doyma[j] = 0;
    }
}

static void dongu_sifirla(ida_kon_t *k)
{
    k->son_kip = KANAL_YOK;
    k->son_dikey = KANAL_YOK;
    k->son_tutum = KANAL_YOK;
    k->son_yon = KANAL_YOK;
    k->son_yatay = KANAL_YOK;
    k->d_ref_var = 0u;
    k->psi_ref_var = 0u;
    k->d_ref = 0.0f;
    k->psi_ref = 0.0f;
    k->i_w = 0.0f;
    for (uint32_t j = 0u; j < 3u; j++) {
        k->i_pqr[j] = 0.0f;
    }
    k->i_v[0] = 0.0f;
    k->i_v[1] = 0.0f;
    k->ref_kusak_var = 0u;
    cikti_sifirla(k);
}

void ida_kon_kur(ida_kon_t *k)
{
    (void)memset(k, 0, sizeof(*k));
    k->onb_var = 0u;
    k->cokme = 0u;
    k->t_var = 0u;
    dongu_sifirla(k);
}

void ida_kon_oturum_basi(ida_kon_t *k)
{
    dongu_sifirla(k);
    k->cokme = 0u;
    k->t_var = 0u;
    k->t_onceki_ns = 0;
}

void ida_kon_komut(ida_kon_t *k, const ida_komut_baglam_t *b, const ida_cmd_t *cmd,
                   ida_cmd_result_t *r)
{
    (void)k;
    (void)b;
    (void)cmd;
    r->ack = (uint8_t)IDA_ACK_UNSUPPORTED; /* MANUAL bu sürümde yok (A4-K124) */
    r->reason = (uint16_t)IDA_REASON_NONE;
}

/* Tahsis önbelleği: geometri, model ya da devre dışı maskesi değiştiyse yeniden kurulur. */
static bool onbellek_guncelle(ida_kon_t *k, const ida_prm_kume_t *p, uint8_t disi)
{
    bool ayni = (k->onb_var == 1u) && (k->onb_disi == disi);
    for (uint32_t i = 0u; ayni && (i < IDA_KON_GEO_SAYI); i++) {
        ayni = k->onb_geo[i] == p->ham[(uint32_t)IDA_PRM_kon_t1_rx + i];
    }
    for (uint32_t i = 0u; ayni && (i < IDA_KON_MODEL_SAYI); i++) {
        ayni = k->onb_model[i] == p->ham[(uint32_t)IDA_PRM_kon_fmax_ileri + i];
    }
    if (ayni) {
        return true;
    }
    k->onb_var = 0u;
    if (ida_tah_kur(&k->tah, p, disi) != IDA_TAH_TAMAM) {
        return false;
    }
    for (uint32_t i = 0u; i < IDA_KON_GEO_SAYI; i++) {
        k->onb_geo[i] = p->ham[(uint32_t)IDA_PRM_kon_t1_rx + i];
    }
    for (uint32_t i = 0u; i < IDA_KON_MODEL_SAYI; i++) {
        k->onb_model[i] = p->ham[(uint32_t)IDA_PRM_kon_fmax_ileri + i];
    }
    k->onb_disi = disi;
    k->onb_var = 1u;
    return true;
}

_Static_assert((uint32_t)IDA_PRM_kon_u_min - (uint32_t)IDA_PRM_kon_fmax_ileri ==
                   IDA_KON_MODEL_SAYI - 1u,
               "itki modeli parametreleri ardışık");

/* İstek biçimce geçerli mi: seçiciler tanımlı, bütün sayılar sonlu. KAPALI etkin değildir. */
static bool istek_etkin(const ida_kon_istek_t *s)
{
    if ((s->kip != IDA_KON_KIP_YUZEY) && (s->kip != IDA_KON_KIP_DALIS)) {
        return false;
    }
    if ((s->dikey > IDA_KON_DIKEY_KUVVET) || (s->tutum > IDA_KON_TUTUM_MOMENT) ||
        (s->yon_kanali > IDA_KON_YON_MOMENT) || (s->yatay > IDA_KON_YATAY_KUVVET)) {
        return false;
    }
    bool sonlu = ida_sonlu(s->derinlik) && ida_sonlu(s->dikey_hiz) && ida_sonlu(s->yalpa) &&
                 ida_sonlu(s->bas_kic) && ida_sonlu(s->yalpa_hizi) &&
                 ida_sonlu(s->bas_kic_hizi) && ida_sonlu(s->yon) && ida_sonlu(s->yon_hizi) &&
                 ida_sonlu(s->hiz[0]) && ida_sonlu(s->hiz[1]);
    for (uint32_t i = 0u; i < 6u; i++) {
        sonlu = sonlu && ida_sonlu(s->kuvvet[i]);
    }
    return sonlu;
}

static bool nav_sonlu(const ida_nav_out_t *n)
{
    bool s = ida_sonlu(n->depth);
    for (uint32_t i = 0u; i < 3u; i++) {
        s = s && ida_sonlu(n->v_ned[i]) && ida_sonlu(n->omega_b[i]);
    }
    for (uint32_t i = 0u; i < 4u; i++) {
        s = s && ida_sonlu(n->q_nb[i]);
    }
    return s;
}

/* Kanal seçimi değişince o kanalın integrali ve referansı sıfırlanır. */
static void kanal_degisimi(ida_kon_t *k, const ida_kon_istek_t *s)
{
    if (k->son_kip != s->kip) {
        dongu_sifirla(k);
        for (uint32_t i = 0u; i < IDA_PORT_N_THR; i++) {
            k->f[i] = 0.0f; /* rejim değişiminde değişim hızı sınırı sıfırdan başlar */
        }
    } else {
        if (k->son_dikey != s->dikey) {
            k->i_w = 0.0f;
            k->d_ref_var = 0u;
        }
        if (k->son_tutum != s->tutum) {
            k->i_pqr[0] = 0.0f;
            k->i_pqr[1] = 0.0f;
        }
        if (k->son_yon != s->yon_kanali) {
            k->i_pqr[2] = 0.0f;
            k->psi_ref_var = 0u;
        }
        if (k->son_yatay != s->yatay) {
            k->i_v[0] = 0.0f;
            k->i_v[1] = 0.0f;
        }
    }
    k->son_kip = s->kip;
    k->son_dikey = s->dikey;
    k->son_tutum = s->tutum;
    k->son_yon = s->yon_kanali;
    k->son_yatay = s->yatay;
}

/* Referans kuşağı değişti (sol bulgu 2; EMNİYET): hedefin kaynağı değişti (arıza eylemine
   geçiş, tutma hedefinin yeniden yakalanması, güdüm hedefinin iptali, derinleşme yasağı).
   Eski hedefe göre yumuşatılmış derinlik ve yön referansları ölçülen durumdan yeniden kurulur;
   eski hedefe giderken biriken öteleme integralleri (dikey, yatay) sıfırlanır. Açısal hız
   integralleri korunur: moment bozucusunu (ör. zayıflamış itici) karşılarlar ve sıfırlanmaları
   tutumda geçici sapma yaratır. */
static void referans_kusagi(ida_kon_t *k, const ida_kon_istek_t *s)
{
    if ((k->ref_kusak_var == 1u) && (k->son_ref_kusak != s->ref_kusak)) {
        k->d_ref_var = 0u;
        k->psi_ref_var = 0u;
        k->i_w = 0.0f;
        k->i_v[0] = 0.0f;
        k->i_v[1] = 0.0f;
    }
    k->son_ref_kusak = s->ref_kusak;
    k->ref_kusak_var = 1u;
}

/* Koşullu integral (sarma önlemi; 02 §2.5): çıkış kendi sınırında ya da tahsis o yönde doymuş
   ve hata aynı yöne itiyorsa integral donar; ters yönde çözülebilir. |i| ≤ sınır.
   Kalıntı ölü bant ve nicemden ÖNCEKİ tahsisten hesaplanır (bilinçli; sol bulgu 6 ve AB-128
   uygulanmadı): ölü bant bir sınır doyması değildir, istek artınca çıktı artar; ölü banttaki
   kalıntıyla integrali dondurmak eşiği yalnız P teriminin geçmesine bırakır ve kalıcı hata
   bırakır (test_dongu yüzey yön tutmasında 0,0555 rad'a karşı 0,0021 rad; W3-kontrol
   düzeltme turu raporu). */
static float integral(float i, float ki, float e, float dt, float ham, float sinir, int8_t doyma)
{
    const bool kendi = ((ham > sinir) && (e > 0.0f)) || ((ham < -sinir) && (e < 0.0f));
    const bool tahsis = ((doyma > 0) && (e > 0.0f)) || ((doyma < 0) && (e < 0.0f));
    if (kendi || tahsis || !(dt > 0.0f)) {
        return i;
    }
    return kirp(i + (ki * e * dt), sinir);
}

static int8_t doyma_isareti(float kalinti, float istek)
{
    const float esik = DOYMA_MUTLAK + (DOYMA_GORELI * ((istek < 0.0f) ? -istek : istek));
    if (kalinti > esik) {
        return 1;
    }
    if (kalinti < -esik) {
        return -1;
    }
    return 0;
}

/* Etkin adımın ara değerleri (integral güncellemesi için). */
typedef struct {
    bool w_pi;
    float e_w, ham_w;
    bool pqr_pi[3];
    float e_pqr[3], ham_pqr[3];
    bool v_pi;
    float e_v[2], ham_v[2];
} ara_t;

/* EMNİYET (sol bulgu 1): her ara sonuç kırpma ve doymadan ÖNCE buradan geçer; sonlu
   olmayan bir değer *ok'u düşürür ve adım çökme sayılır (A4-K55). */
static float sk(bool *ok, float x)
{
    if (!ida_sonlu(x)) {
        *ok = false;
    }
    return x;
}

/* Açısal hız kısıtlarının türü (eksen çifti yalpa/baş-kıç ve yön için ayrı). */
#define KIS_EULER  0u /* Euler hızı verilmiş (açı kanalı)                   */
#define KIS_GOVDE  1u /* gövde hızı verilmiş (hız kanalı)                   */
#define KIS_SERBES 2u /* denetlenmiyor: ölçülen Euler hızı kısıt alınır     */
#define COS_TETA_EN_AZ 0.05f /* |cos θ| altında (≈ 87°) kinematik çözülmez  */

/* Seçili kanalların Euler ve gövde hızı kısıtlarını birlikte çözer (sol bulgu 4):
   bilinmeyen Euler hızları ė = (φ̇, θ̇, ψ̇); gövde hızı ω = E ė,
   E = [[1, 0, −sθ], [0, cφ, sφ cθ], [0, −sφ, cφ cθ]]. Her kısıt bir satırdır: Euler kısıtı
   ė_i = v, gövde kısıtı E_j ė = v, serbest eksen ė_i = ölçülen. Sonuç ω_sp = E ė. Kinematik
   tekilse (|cos θ| küçük ya da sistem tekil) false. */
static bool kinematik_coz(const ida_euler_t *e, const float omega[3], uint8_t tutum_tur,
                          uint8_t yon_tur, const float tutum_v[2], float yon_v, float w_sp[3])
{
    const float sf = ida_sin(e->roll);
    const float cf = ida_cos(e->roll);
    const float st = ida_sin(e->pitch);
    const float ct = ida_cos(e->pitch);
    if (!((ct > COS_TETA_EN_AZ) || (ct < -COS_TETA_EN_AZ))) {
        return false;
    }
    float em[3][3];
    em[0][0] = 1.0f;
    em[0][1] = 0.0f;
    em[0][2] = -st;
    em[1][0] = 0.0f;
    em[1][1] = cf;
    em[1][2] = sf * ct;
    em[2][0] = 0.0f;
    em[2][1] = -sf;
    em[2][2] = cf * ct;
    /* Ölçülen Euler hızları (serbest eksenler için). */
    float olc[3];
    olc[2] = ((omega[1] * sf) + (omega[2] * cf)) / ct;
    olc[1] = (omega[1] * cf) - (omega[2] * sf);
    olc[0] = omega[0] + (olc[2] * st);
    float a[3][3];
    float y[3];
    for (uint32_t j = 0u; j < 2u; j++) {
        for (uint32_t c = 0u; c < 3u; c++) {
            a[j][c] = (tutum_tur == KIS_GOVDE) ? em[j][c] : ((c == j) ? 1.0f : 0.0f);
        }
        y[j] = (tutum_tur == KIS_SERBES) ? olc[j] : tutum_v[j];
    }
    for (uint32_t c = 0u; c < 3u; c++) {
        a[2][c] = (yon_tur == KIS_GOVDE) ? em[2][c] : ((c == 2u) ? 1.0f : 0.0f);
    }
    y[2] = (yon_tur == KIS_SERBES) ? olc[2] : yon_v;
    /* Cramer. */
    const float det = (a[0][0] * ((a[1][1] * a[2][2]) - (a[1][2] * a[2][1]))) -
                      (a[0][1] * ((a[1][0] * a[2][2]) - (a[1][2] * a[2][0]))) +
                      (a[0][2] * ((a[1][0] * a[2][1]) - (a[1][1] * a[2][0])));
    if (!((det > 1.0e-3f) || (det < -1.0e-3f))) {
        return false;
    }
    float ed[3];
    for (uint32_t c = 0u; c < 3u; c++) {
        float m[3][3];
        for (uint32_t i = 0u; i < 3u; i++) {
            for (uint32_t j = 0u; j < 3u; j++) {
                m[i][j] = (j == c) ? y[i] : a[i][j];
            }
        }
        const float dc = (m[0][0] * ((m[1][1] * m[2][2]) - (m[1][2] * m[2][1]))) -
                         (m[0][1] * ((m[1][0] * m[2][2]) - (m[1][2] * m[2][0]))) +
                         (m[0][2] * ((m[1][0] * m[2][1]) - (m[1][1] * m[2][0])));
        ed[c] = dc / det;
    }
    bool ok = true;
    for (uint32_t j = 0u; j < 3u; j++) {
        w_sp[j] = sk(&ok, (em[j][0] * ed[0]) + (em[j][1] * ed[1]) + (em[j][2] * ed[2]));
    }
    return ok;
}

static bool hesapla_etkin(ida_kon_t *k, const kazanc_t *z, const ida_kon_girdi_t *g, float dt)
{
    const ida_kon_istek_t *s = &g->istek;
    const ida_nav_out_t *n = &g->nav;
    const bool dalis = s->kip == IDA_KON_KIP_DALIS;
    bool ok = true;

    ida_q_t q;
    ida_q_t q_ham;
    q_ham.w = n->q_nb[0];
    q_ham.x = n->q_nb[1];
    q_ham.y = n->q_nb[2];
    q_ham.z = n->q_nb[3];
    if (!ida_q_birimle(q_ham, &q)) {
        return false;
    }
    ida_euler_t e;
    if (!ida_q_euler(q, &e)) {
        return false;
    }
    const ida_m3_t r = ida_q_dcm(q);        /* gövde → NED */
    const ida_m3_t rt = ida_m3_devrik(r);   /* NED → gövde */
    const float cpsi = ida_cos(e.yaw);
    const float spsi = ida_sin(e.yaw);

    ara_t a;
    (void)memset(&a, 0, sizeof(a));

    /* Dikey: derinlik → dikey hız setpoint'i → NED Z kuvveti (+ yüzerlik ve sönüm ileri
       beslemesi). */
    float fz_ned = 0.0f;
    /* Derinleşme yasağı (EMNİYET): aşağı yönlü kuvvetin üst sınırı yalnız yüzerlik
       dengelemesidir (BUOY_FF = B − W kestirimi, pozitif yüzer araçta ≥ 0): bu kuvvet aracı
       yerinde tutar, net aşağı ivme üretmez. Fazlası (aşağı hız ve integral) üretilmez. */
    const bool yasak = s->dalis_yasak != 0u;
    const float tutma_z = (z->yuzerlik > 0.0f) ? z->yuzerlik : 0.0f;
    if (dalis && (s->dikey != IDA_KON_DIKEY_KUVVET)) {
        float w_sp = 0.0f;
        if (s->dikey == IDA_KON_DIKEY_DERINLIK) {
            float hedef = (s->derinlik > 0.0f) ? s->derinlik : 0.0f;
            if (yasak && (hedef > n->depth)) {
                hedef = n->depth;
            }
            if (k->d_ref_var == 0u) {
                k->d_ref = n->depth;
                k->d_ref_var = 1u;
            }
            const float adim = kirp(sk(&ok, hedef - k->d_ref), z->ref_d_hiz * dt);
            k->d_ref = sk(&ok, k->d_ref + adim);
            float ileri = (dt > 0.0f) ? (adim / dt) : 0.0f;
            if (yasak) {
                /* Referans ölçülen derinliğin altında kalamaz: araç referansın hız sınırından
                   hızlı yükselse de eski (derin) referans dalış istemez (2. tur bulgu 2). */
                if (k->d_ref > n->depth) {
                    k->d_ref = n->depth;
                }
                if (ileri > 0.0f) {
                    ileri = 0.0f;
                }
            }
            w_sp = kirp(sk(&ok, (z->d_kp * sk(&ok, k->d_ref - n->depth)) + ileri), z->lim_w);
        } else {
            k->d_ref_var = 0u;
            w_sp = kirp(s->dikey_hiz, z->lim_w);
        }
        if (yasak && (w_sp > 0.0f)) {
            w_sp = 0.0f; /* aşağı yönlü dikey hız isteği yok */
        }
        a.w_pi = true;
        a.e_w = sk(&ok, w_sp - n->v_ned[2]);
        a.ham_w = sk(&ok, (z->w_kp * a.e_w) + k->i_w + z->yuzerlik + (z->w_sonum * w_sp));
        fz_ned = kirp(a.ham_w, z->lim_fz);
        if (yasak && (fz_ned > tutma_z)) {
            fz_ned = tutma_z;
        }
    }

    /* Açısal: açı kanallarından Euler hızı, hız kanallarından gövde hızı; birlikte çözülür. */
    uint8_t tutum_tur = KIS_SERBES;
    float tutum_v[2] = {0.0f, 0.0f};
    if (dalis && (s->tutum == IDA_KON_TUTUM_ACI)) {
        tutum_tur = KIS_EULER;
        const float phi_sp = kirp(s->yalpa, ACI_EN_BUYUK);
        const float th_sp = kirp(s->bas_kic, ACI_EN_BUYUK);
        tutum_v[0] = kirp(sk(&ok, z->tutum_kp * ida_aci_sar(phi_sp - e.roll)), z->lim_pq);
        tutum_v[1] = kirp(sk(&ok, z->tutum_kp * ida_aci_sar(th_sp - e.pitch)), z->lim_pq);
    } else if (dalis && (s->tutum == IDA_KON_TUTUM_HIZ)) {
        tutum_tur = KIS_GOVDE;
        tutum_v[0] = kirp(s->yalpa_hizi, z->lim_pq);
        tutum_v[1] = kirp(s->bas_kic_hizi, z->lim_pq);
    }
    uint8_t yon_tur = KIS_SERBES;
    float yon_v = 0.0f;
    if (s->yon_kanali == IDA_KON_YON_ACI) {
        yon_tur = KIS_EULER;
        if (k->psi_ref_var == 0u) {
            k->psi_ref = e.yaw;
            k->psi_ref_var = 1u;
        }
        const float adim = kirp(ida_aci_sar(s->yon - k->psi_ref), z->ref_psi_hiz * dt);
        k->psi_ref = ida_aci_sar(k->psi_ref + adim);
        const float ileri = (dt > 0.0f) ? (adim / dt) : 0.0f;
        yon_v = kirp(sk(&ok, (z->yon_kp * ida_aci_sar(k->psi_ref - e.yaw)) + ileri), z->lim_r);
    } else {
        k->psi_ref_var = 0u;
        if (s->yon_kanali == IDA_KON_YON_HIZ) {
            yon_tur = KIS_GOVDE;
            yon_v = kirp(s->yon_hizi, z->lim_r);
        }
    }
    float w_sp[3];
    if (!kinematik_coz(&e, n->omega_b, tutum_tur, yon_tur, tutum_v, yon_v, w_sp)) {
        return false;
    }
    w_sp[0] = kirp(w_sp[0], z->lim_pq);
    w_sp[1] = kirp(w_sp[1], z->lim_pq);
    w_sp[2] = kirp(w_sp[2], z->lim_r);
    a.pqr_pi[0] = dalis && (s->tutum != IDA_KON_TUTUM_MOMENT);
    a.pqr_pi[1] = a.pqr_pi[0];
    a.pqr_pi[2] = s->yon_kanali != IDA_KON_YON_MOMENT;
    float kp[3];
    float lim_m[3];
    kp[0] = z->pq_kp;
    kp[1] = z->pq_kp;
    kp[2] = z->r_kp;
    lim_m[0] = z->lim_mkm;
    lim_m[1] = z->lim_mkm;
    lim_m[2] = z->lim_mn;
    float moment[3] = {0.0f, 0.0f, 0.0f};
    for (uint32_t j = 0u; j < 3u; j++) {
        if (a.pqr_pi[j]) {
            a.e_pqr[j] = sk(&ok, w_sp[j] - n->omega_b[j]);
            a.ham_pqr[j] = sk(&ok, (kp[j] * a.e_pqr[j]) + k->i_pqr[j]);
            moment[j] = a.ham_pqr[j];
        }
    }
    if (dalis && (s->tutum == IDA_KON_TUTUM_MOMENT)) {
        moment[0] = s->kuvvet[IDA_EKS_K];
        moment[1] = s->kuvvet[IDA_EKS_M];
    }
    if (s->yon_kanali == IDA_KON_YON_MOMENT) {
        moment[2] = s->kuvvet[IDA_EKS_N];
    }

    /* Yatay: yön çerçevesinde hız PI (+ sönüm ileri beslemesi). */
    float fh[2] = {0.0f, 0.0f};
    if (s->yatay == IDA_KON_YATAY_HIZ) {
        float vh[2];
        vh[0] = sk(&ok, (cpsi * n->v_ned[0]) + (spsi * n->v_ned[1]));
        vh[1] = sk(&ok, (-spsi * n->v_ned[0]) + (cpsi * n->v_ned[1]));
        a.v_pi = true;
        for (uint32_t j = 0u; j < 2u; j++) {
            const float v_sp = kirp(s->hiz[j], z->lim_v);
            a.e_v[j] = sk(&ok, v_sp - vh[j]);
            a.ham_v[j] = sk(&ok, (z->v_kp * a.e_v[j]) + k->i_v[j] + (z->v_sonum * v_sp));
            fh[j] = kirp(a.ham_v[j], z->lim_fxy);
        }
    }

    /* Gövde kuvveti: [X Y Z]ᵀ = Rᵀ R_ψ [F_x F_y F_z]ᵀ (02 §2.5). */
    ida_v3_t f_ned;
    f_ned.x = (cpsi * fh[0]) - (spsi * fh[1]);
    f_ned.y = (spsi * fh[0]) + (cpsi * fh[1]);
    f_ned.z = fz_ned;
    const ida_v3_t fb = ida_m3_v3(rt, f_ned);
    float tau[IDA_TAH_EKSEN];
    tau[IDA_EKS_X] = fb.x;
    tau[IDA_EKS_Y] = fb.y;
    tau[IDA_EKS_Z] = dalis ? fb.z : 0.0f;
    if (s->yatay == IDA_KON_YATAY_KUVVET) {
        tau[IDA_EKS_X] += s->kuvvet[IDA_EKS_X];
        tau[IDA_EKS_Y] += s->kuvvet[IDA_EKS_Y];
    }
    if (dalis && (s->dikey == IDA_KON_DIKEY_KUVVET)) {
        float kz = s->kuvvet[IDA_EKS_Z];
        if (yasak && (kz > tutma_z)) {
            kz = tutma_z; /* derinleşme yasağı: doğrudan aşağı kuvvet de sınırlı */
        }
        tau[IDA_EKS_Z] += kz;
    }
    tau[IDA_EKS_K] = dalis ? moment[0] : 0.0f;
    tau[IDA_EKS_M] = dalis ? moment[1] : 0.0f;
    tau[IDA_EKS_N] = moment[2];
    for (uint32_t j = 0u; j < IDA_TAH_EKSEN; j++) {
        (void)sk(&ok, tau[j]);
    }
    if (!ok) {
        return false; /* kırpmadan önce: sonlu olmayan istek gizlenmez */
    }
    tau[IDA_EKS_X] = kirp(tau[IDA_EKS_X], z->lim_fxy);
    tau[IDA_EKS_Y] = kirp(tau[IDA_EKS_Y], z->lim_fxy);
    tau[IDA_EKS_Z] = kirp(tau[IDA_EKS_Z], z->lim_fz);
    tau[IDA_EKS_K] = kirp(tau[IDA_EKS_K], lim_m[0]);
    tau[IDA_EKS_M] = kirp(tau[IDA_EKS_M], lim_m[1]);
    tau[IDA_EKS_N] = kirp(tau[IDA_EKS_N], lim_m[2]);

    /* Tahsis: itici başına değişim hızı penceresi tahsisin sınırlarına katılır (sol bulgu 2);
       öncelik bu sınırlar altında korunur. */
    const float egim = z->itki_egim * dt;
    float alt[IDA_TAH_ITICI];
    float ust[IDA_TAH_ITICI];
    for (uint32_t i = 0u; i < IDA_TAH_ITICI; i++) {
        alt[i] = k->f[i] - egim;
        ust[i] = k->f[i] + egim;
    }
    float f[IDA_TAH_ITICI];
    float tau_a[IDA_TAH_EKSEN];
    for (uint32_t i = 0u; i < IDA_TAH_ITICI; i++) {
        f[i] = k->f[i]; /* başlangıç: önceki tahsis */
    }
    ida_tah_dagit(&k->tah, (uint8_t)(dalis ? IDA_TAH_DALIS : IDA_TAH_YUZEY), tau, alt, ust, f,
                  tau_a);
    float kal[IDA_TAH_EKSEN];
    for (uint32_t j = 0u; j < IDA_TAH_EKSEN; j++) {
        kal[j] = sk(&ok, tau[j] - tau_a[j]);
        k->doyma[j] = doyma_isareti(kal[j], tau[j]);
    }
    for (uint32_t i = 0u; i < IDA_TAH_ITICI; i++) {
        (void)sk(&ok, f[i]);
    }
    if (!ok) {
        return false;
    }

    /* İntegraller: gövde kalıntısı yön çerçevesine ve NED'e taşınır. */
    ida_v3_t kal_b;
    kal_b.x = kal[IDA_EKS_X];
    kal_b.y = kal[IDA_EKS_Y];
    kal_b.z = kal[IDA_EKS_Z];
    const ida_v3_t kal_n = ida_m3_v3(r, kal_b);
    float kal_h[2];
    kal_h[0] = (cpsi * kal_n.x) + (spsi * kal_n.y);
    kal_h[1] = (-spsi * kal_n.x) + (cpsi * kal_n.y);
    if (a.w_pi) {
        k->i_w = integral(k->i_w, z->w_ki, a.e_w, dt, a.ham_w, z->lim_fz,
                          doyma_isareti(kal_n.z, fz_ned));
        if (yasak && (k->i_w > 0.0f)) {
            k->i_w = 0.0f; /* derinleşme yasağı: integral aşağı yönde birikmez */
        }
    }
    float ki[3];
    ki[0] = z->pq_ki;
    ki[1] = z->pq_ki;
    ki[2] = z->r_ki;
    for (uint32_t j = 0u; j < 3u; j++) {
        if (a.pqr_pi[j]) {
            k->i_pqr[j] = integral(k->i_pqr[j], ki[j], a.e_pqr[j], dt, a.ham_pqr[j], lim_m[j],
                                   k->doyma[IDA_EKS_K + j]);
        }
    }
    if (a.v_pi) {
        for (uint32_t j = 0u; j < 2u; j++) {
            k->i_v[j] = integral(k->i_v[j], z->v_ki, a.e_v[j], dt, a.ham_v[j], z->lim_fxy,
                                 doyma_isareti(kal_h[j], fh[j]));
        }
    }

    /* Eğrinin tersi ve ölü bant (A4-K54). */
    ok = ida_sonlu(k->i_w) && ida_sonlu(k->i_v[0]) && ida_sonlu(k->i_v[1]) &&
         ida_sonlu(k->d_ref) && ida_sonlu(k->psi_ref); /* yukarıda !ok dönüldü */
    for (uint32_t j = 0u; j < 3u; j++) {
        ok = ok && ida_sonlu(k->i_pqr[j]);
    }
    for (uint32_t i = 0u; i < IDA_TAH_ITICI; i++) {
        k->f[i] = f[i];
        k->thr[i] = ida_tah_komut(&k->tah, f[i]);
    }
    return ok;
}

void ida_kon_hesapla(ida_kon_t *k, const ida_prm_kume_t *prm, const ida_kon_girdi_t *g)
{
    float dt = 0.0f;
    if (k->t_var == 1u) {
        const int64_t d = g->t_ns - k->t_onceki_ns;
        if (d >= DT_EN_BUYUK_NS) {
            dt = DT_EN_BUYUK;
        } else if (d > 0) {
            dt = (float)d * 1.0e-9f;
        }
    }
    k->t_var = 1u;
    k->t_onceki_ns = g->t_ns;
    k->cokme = 0u;

    const uint8_t disi =
        (uint8_t)(g->itici_disi | (uint8_t)(prm->ham[IDA_PRM_kon_itici_disi] & 0xFFu));
    const bool tahsis_hazir = onbellek_guncelle(k, prm, disi);

    /* EMNİYET: kuşkuda sıfır itki (W3-kontrol md. 3). */
    const bool etkin =
        g->armed && !g->kisit && g->kestirim_yeterli && istek_etkin(&g->istek);
    if (!etkin) {
        dongu_sifirla(k);
        return;
    }
    if (!tahsis_hazir || !nav_sonlu(&g->nav)) {
        dongu_sifirla(k);
        k->cokme = 1u;
        return;
    }
    kanal_degisimi(k, &g->istek);
    referans_kusagi(k, &g->istek);
    kazanc_t z;
    kazanc_oku(prm, &z);
    if (!hesapla_etkin(k, &z, g, dt)) {
        dongu_sifirla(k);
        k->cokme = 1u;
    }
}

/* Görev/güdümün isteği: güdümün tek erişimcisi (etkin kip, arıza eylemi ve GUIDED_SP). */
static void istek_al(const ida_core_t *c, ida_kon_istek_t *i)
{
    ida_gud_kon_istegi(&c->gud, c, i);
}

void ida_kon_girdi_topla(const ida_core_t *c, ida_kon_girdi_t *g)
{
    (void)memset(g, 0, sizeof(*g));
    istek_al(c, &g->istek);
    ida_stime_t t;
    t.ns = c->cer.tf_t_ns;
    ida_kes_nav(&c->kes, t, &g->nav);
    g->t_ns = c->cer.tf_t_ns;
    g->armed = c->emn.armed;
    /* W3-K1, W3-K4: kısıt yalnız itkisiz arıza eylemidir (PASSIVE_SURFACE ve üstü). Acil
       durdurmanın etkin değeri itki kapısında yer almaz: yalnız atış kontrol izinlerini kapatır
       (A4-K116, engel biti ESTOP); iticileri acil durdurmada sim keser (iskelet §5). */
    const uint8_t fs = ida_gor_fs_duzey(&c->gor);
    g->kisit = fs >= (uint8_t)IDA_FS_PASSIVE_SURFACE;
    const uint8_t est = ida_kes_duzey(&c->kes);
    g->kestirim_yeterli = (est == (uint8_t)IDA_EST_OK) || (est == (uint8_t)IDA_EST_DEGRADED);
    g->itici_disi = ida_gor_itici_disi(&c->gor); /* görev modülünün itici arıza maskesi */
}

void ida_kon_adim(ida_kon_t *k, const ida_core_t *c)
{
    ida_kon_girdi_t g;
    ida_kon_girdi_topla(c, &g);
    ida_kon_hesapla(k, &c->prm.yururluk, &g);
}

void ida_kon_itki(const ida_kon_t *k, float thr[IDA_PORT_N_THR], bool *cokme)
{
    for (uint32_t i = 0u; i < IDA_PORT_N_THR; i++) {
        thr[i] = (k->cokme == 0u) ? k->thr[i] : 0.0f;
    }
    *cokme = k->cokme != 0u;
}

static bool kanal_gecerli(uint8_t v, uint8_t en_cok)
{
    return (v <= en_cok) || (v == KANAL_YOK);
}

bool ida_kon_tutarli(const ida_kon_t *k)
{
    bool t = (k->onb_var <= 1u) && (k->cokme <= 1u) && (k->t_var <= 1u) &&
             (k->ref_kusak_var <= 1u) &&
             (k->d_ref_var <= 1u) && (k->psi_ref_var <= 1u) &&
             kanal_gecerli(k->son_kip, IDA_KON_KIP_DALIS) &&
             kanal_gecerli(k->son_dikey, IDA_KON_DIKEY_KUVVET) &&
             kanal_gecerli(k->son_tutum, IDA_KON_TUTUM_MOMENT) &&
             kanal_gecerli(k->son_yon, IDA_KON_YON_MOMENT) &&
             kanal_gecerli(k->son_yatay, IDA_KON_YATAY_KUVVET);
    for (uint32_t j = 0u; j < IDA_TAH_EKSEN; j++) {
        t = t && (k->doyma[j] >= -1) && (k->doyma[j] <= 1);
    }
    return t;
}

uint8_t ida_kon_param_grup(const ida_prm_kume_t *p)
{
    return (ida_tah_param_denetle(p) == IDA_TAH_TAMAM) ? (uint8_t)0u : (uint8_t)1u;
}
