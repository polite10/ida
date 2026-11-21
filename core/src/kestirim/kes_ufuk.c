/*
 * kes_ufuk.c — sabit gecikmeli füzyon ufku, ölçüm modelleri ve çıkış öngörücüsü
 * (02 §2.3.2–§2.3.4). Sahibi: W3 kestirim.
 *
 * Filtre (durum ve P) t_h = t_imu − D anında yaşar. Her çağrıda ufuk, halkadaki IMU
 * kayıtlarıyla t_imu − D'ye kadar ilerler; bekleyen bir ölçümün zamanı bir IMU aralığının içine
 * düşerse aralık orada bölünür, durum ve P o ana kadar yayılır, ölçüm füzyonlanır ve kalan
 * aralık sonra yayılır. Ufka göre geç kalan ölçüm (t_m < t_h) füzyonlanmaz, sayılır. Adım
 * başına en çok IDA_KES_BUTCE skaler güncelleme yapılır; sınır dolunca ufuk o çağrıda durur.
 *
 * Çıkış öngörücüsü her yeni IMU ile ufkun sapma kestirimleriyle yayılır; ufuk bir kaydın
 * sonuna vardığında e = x_h ⊖ x_o(t_h) hesaplanır (tutumda dünya çerçevesinde, soldan) ve
 * x_o(şimdi) ile halkadaki kayıtlara k = ilerleme / τ_o kazancıyla eklenir. Sıfırlamalar
 * (yön, derinlik) yumuşatılmadan aynı çağrıda çıkışa ve geçmişe uygulanır.
 */
#include "kestirim/kes_ic.h"

#include <string.h>

#include "mat/ida_math.h"

#define N IDA_KES_N

/* ---- Sürekli ret izi ------------------------------------------------------------------- */

static void red_isle(ida_kes_red_t *r, bool ret, int64_t now_ns)
{
    if (!ret) {
        r->var = false;
        return;
    }
    if (!r->var) {
        r->var = true;
        r->bas_ns = now_ns;
    }
}

static bool red_surekli(const ida_kes_red_t *r, int64_t now_ns, int64_t sure_ns)
{
    return r->var && ((now_ns - r->bas_ns) >= sure_ns);
}

/* ---- Kuyruk ---------------------------------------------------------------------------- */

void ida_kes_olc_ekle(ida_kes_t *k, const ida_kes_olc_t *o)
{
    if (k->olc_n >= IDA_KES_OLC_N) {
        k->n_tasma++;
        return;
    }
    /* Sıralı ekleme: (t, sira); eşitte gelen sıra korunur. */
    uint32_t i = k->olc_n;
    while (i > 0u) {
        const ida_kes_olc_t *p = &k->olc[i - 1u];
        const bool once = (p->t_ns < o->t_ns) || ((p->t_ns == o->t_ns) && (p->sira <= o->sira));
        if (once) {
            break;
        }
        k->olc[i] = *p;
        i--;
    }
    k->olc[i] = *o;
    k->olc_n++;
}

void ida_kes_olc_cikar(ida_kes_t *k)
{
    if (k->olc_n == 0u) {
        return;
    }
    for (uint32_t i = 1u; i < k->olc_n; i++) {
        k->olc[i - 1u] = k->olc[i];
    }
    k->olc_n--;
}

/* ---- Kurulum ve IMU -------------------------------------------------------------------- */

void ida_kes_ufuk_kur(ida_kes_t *k, const ida_kes_nominal_t *x, int64_t t_ns)
{
    k->xh = *x;
    k->th_ns = t_ns;
    k->q_o = x->q;
    k->v_o = x->v;
    k->p_o = x->p;
    const ida_v3_t sifir = {0.0f, 0.0f, 0.0f};
    k->w_o = sifir;
    k->to_ns = t_ns;
    k->imu_bas = 0u;
    k->imu_n = 0u;
    k->g_top = sifir;
    k->g_sure = 0.0f;
}

/* dt'yi |ω|dt ≤ 0,05 rad ve dt ≤ 10 ms koşullarına (02 §2.3.1) uyan alt adım sayısına böler. */
static uint32_t alt_adim(float dt, float wn)
{
    uint32_t n = 1u;
    const float a = dt / 0.01f;
    const float b = (wn * dt) / 0.05f;
    const float m = (b > a) ? b : a; /* NaN'da a (sonlu) kalır ya da 1 */
    if ((m > 1.0f) && ida_sonlu(m)) {
        n = (m < 63.0f) ? ((uint32_t)m + 1u) : 64u;
    }
    return n;
}

static void cikis_yay(ida_kes_t *k, ida_v3_t w, ida_v3_t f, float dt, const ida_kes_ayar_t *a)
{
    ida_kes_nominal_t x = {k->p_o, k->v_o, k->q_o, k->xh.bg, k->xh.ba};
    const uint32_t n = alt_adim(dt, ida_v3_norm(ida_v3_cikar(w, x.bg)));
    const float h = dt / (float)n;
    for (uint32_t i = 0u; i < n; i++) {
        ida_kes_nominal_yay(&x, w, f, h, a->g);
    }
    k->p_o = x.p;
    k->v_o = x.v;
    k->q_o = x.q;
    k->w_o = ida_v3_cikar(w, k->xh.bg);
}

static bool kayit_isle(ida_kes_t *k, const ida_kes_ayar_t *a, uint32_t *butce, bool zorla);

void ida_kes_ufuk_imu(ida_kes_t *k, int64_t t_ns, float dt, ida_v3_t w, ida_v3_t f,
                      const ida_kes_ayar_t *a)
{
    if (!(dt > 0.0f)) {
        return; /* ortak zaman çizgisinde geriye yayılım yok (bulgu 2) */
    }
    if (k->imu_n >= IDA_KES_IMU_N) {
        /* Halka dolu: en eski kayıt hedeften önce işlenir (bütçe uygulanmaz). */
        uint32_t sinirsiz = 0xFFFFu;
        (void)kayit_isle(k, a, &sinirsiz, true);
    }
    cikis_yay(k, w, f, dt, a);
    k->to_ns = t_ns;
    const uint32_t yer = ((uint32_t)k->imu_bas + (uint32_t)k->imu_n) % IDA_KES_IMU_N;
    ida_kes_imu_kayit_t *r = &k->imu[yer];
    r->t_ns = t_ns;
    r->dt = dt;
    r->w = w;
    r->f = f;
    r->q_o = k->q_o;
    r->v_o = k->v_o;
    r->p_o = k->p_o;
    k->imu_n++;
}

/* ---- Ufuk yayılımı --------------------------------------------------------------------- */

static void ufuk_yay(ida_kes_t *k, ida_v3_t w, ida_v3_t f, float dt, const ida_kes_ayar_t *a)
{
    if (!(dt > 0.0f)) {
        return;
    }
    const uint32_t n = alt_adim(dt, ida_v3_norm(ida_v3_cikar(w, k->xh.bg)));
    const float h = dt / (float)n;
    for (uint32_t i = 0u; i < n; i++) {
        ida_kes_P_yay(k->P, &k->xh, w, f, h, a);
        ida_kes_nominal_yay(&k->xh, w, f, h, a->g);
    }
}

/* ---- Ölçüm modelleri ------------------------------------------------------------------- */

static void sifirla(float v[N])
{
    for (uint32_t i = 0u; i < N; i++) {
        v[i] = 0.0f;
    }
}

static void destek(ida_kes_akis_t *s, int64_t now_ns)
{
    s->destek_var = true;
    s->destek_ns = now_ns;
}

static float mutlak(float x)
{
    return (x < 0.0f) ? -x : x;
}

/* Derinlik sıçraması (02 §2.3.2 (d): sıfırlama yumuşatılmaz): ufuktaki fark aynı çağrıda
   çıkışa ve halkadaki çıkış kayıtlarına tam uygulanır; p_z'nin çapraz kovaryansı silinir,
   varyansı r_baro² olur; konum sıfırlama sayacı artar (A4 §3.8 reset_counter). */
static void derinlik_sicra(ida_kes_t *k, const ida_kes_ayar_t *a, float yeni_z)
{
    const float fark = yeni_z - k->xh.p.z;
    k->xh.p.z = yeni_z;
    k->p_o.z += fark;
    for (uint32_t i = 0u; i < k->imu_n; i++) {
        k->imu[((uint32_t)k->imu_bas + i) % IDA_KES_IMU_N].p_o.z += fark;
    }
    for (uint32_t i = 0u; i < N; i++) {
        k->P[IDA_KES_IP + 2u][i] = 0.0f;
        k->P[i][IDA_KES_IP + 2u] = 0.0f;
    }
    k->P[IDA_KES_IP + 2u][IDA_KES_IP + 2u] = a->r_baro * a->r_baro;
    k->reset_sayac = (uint8_t)(k->reset_sayac + 1u);
}

/* Basınç (02 §2.3.3): d = (P − P_atm)/(ρ g); h = p_z + (R l_p)_z;
   H = [e₃ᵀ, 0, −e₃ᵀ R [l_p]×, 0, 0]. */
static void baro_fuzyon(ida_kes_t *k, const ida_kes_ayar_t *a, const ida_kes_olc_t *o)
{
    const ida_m3_t r = ida_q_dcm(k->xh.q);
    const ida_v3_t rl = ida_m3_v3(r, a->l_baro);
    const float p = o->v[0];
    const float rg = a->rho * a->g;
    if (!((p >= IDA_KES_BARO_EN_AZ) && (p <= IDA_KES_BARO_EN_COK))) {
        red_isle(&k->red_baro, true, a->now_ns); /* fiziksel kapı: kullanılmaz, desteklemez */
        return;
    }
    if (!k->kalibre) {
        /* BARO_KAL = 1 ve init'ten sonraki ilk hizalamada basınç yoktu: ilk basınç, O_b'nin
           BARO_KAL_D derinliğinde olduğu varsayımıyla P_atm'yi kurar; derinlik ona sıçrar. */
        k->p_atm = p - rg * (a->baro_kal_d + rl.z);
        k->kalibre = true;
        derinlik_sicra(k, a, a->baro_kal_d);
        destek(&k->akis_baro, a->now_ns);
        red_isle(&k->red_baro, false, a->now_ns);
        return;
    }
    const float d_olc = (p - k->p_atm) / rg;
    const ida_m3_t rl_x = ida_m3_carp(r, ida_m3_capraz(a->l_baro));
    float h[N];
    sifirla(h);
    h[IDA_KES_IP + 2u] = 1.0f;
    for (uint32_t j = 0u; j < 3u; j++) {
        h[IDA_KES_ITH + j] = -rl_x.m[2][j];
    }
    const float y = d_olc - (k->xh.p.z + rl.z);
    float dx[N];
    sifirla(dx);
    float nis = 0.0f;
    const bool kabul = ida_kes_skaler(k->P, h, y, a->r_baro * a->r_baro, a->kapi_g1, dx, &nis);
    if (kabul) {
        ida_kes_enjekte(&k->xh, k->P, dx);
        destek(&k->akis_baro, a->now_ns);
    }
    red_isle(&k->red_baro, !kabul, a->now_ns);
    if (!kabul && red_surekli(&k->red_baro, a->now_ns, a->baro_reset_ns)) {
        /* Uzun süre kapıdan geçemeyen derinlik (ör. uzun basınç kesintisinden sonra):
           derinlik ölçüme sıfırlanır (geri dönüş yolu). */
        derinlik_sicra(k, a, d_olc - rl.z);
        k->n_derinlik_sifir++;
        destek(&k->akis_baro, a->now_ns);
        red_isle(&k->red_baro, false, a->now_ns);
    }
}

/* Yönü dünya z ekseni etrafında −e döndürür (ufuk, çıkış ve halkadaki çıkış kayıtları; 02
   §2.3.2 (d): sıfırlama yumuşatılmaz). P'nin dünya-yön doğrultusuna r_mag² eklenir. */
static void yon_sifirla(ida_kes_t *k, const ida_kes_ayar_t *a, float e)
{
    const ida_q_t qz = ida_kes_q_euler(0.0f, 0.0f, -e);
    ida_q_t b;
    k->xh.q = ida_q_birimle(ida_q_carp(qz, k->xh.q), &b) ? b : k->xh.q;
    k->q_o = ida_q_birimle(ida_q_carp(qz, k->q_o), &b) ? b : k->q_o;
    for (uint32_t i = 0u; i < k->imu_n; i++) {
        ida_kes_imu_kayit_t *r = &k->imu[((uint32_t)k->imu_bas + i) % IDA_KES_IMU_N];
        r->q_o = ida_q_birimle(ida_q_carp(qz, r->q_o), &b) ? b : r->q_o;
    }
    const ida_m3_t r = ida_q_dcm(k->xh.q);
    const float s2 = a->r_mag * a->r_mag;
    for (uint32_t i = 0u; i < 3u; i++) {
        for (uint32_t j = 0u; j < 3u; j++) {
            k->P[IDA_KES_ITH + i][IDA_KES_ITH + j] += r.m[2][i] * r.m[2][j] * s2;
        }
    }
    ida_kes_P_duzelt(k->P);
    k->n_yon_sifir++;
}

/* Manyetometre (02 §2.3.3): eğim telafili yön. v = R̂ m_b; yenilik −wrap(atan2(v_y, v_x) − δ);
   H yalnız dünya-yön doğrultusundadır (R̂'nin üçüncü satırı): manyetik bozulma yuvarlanma ve
   yunuslamaya sızmaz. Alan normu beklenenin ±MAG_B_TOL'u dışındaysa örnek kullanılmaz. */
static void mag_fuzyon(ida_kes_t *k, const ida_kes_ayar_t *a, const ida_kes_olc_t *o)
{
    const ida_v3_t ms = {o->v[0], o->v[1], o->v[2]};
    const ida_v3_t mb = ida_m3_v3(a->m_mag, ms);
    const float n = ida_v3_norm(mb);
    const float sapma = n - a->mag_b;
    if (!((sapma <= a->mag_b_tol * a->mag_b) && (-sapma <= a->mag_b_tol * a->mag_b))) {
        red_isle(&k->red_mag, true, a->now_ns); /* norm kapısı: ret, desteklemez */
        return;
    }
    const ida_m3_t r = ida_q_dcm(k->xh.q);
    const ida_v3_t v = ida_m3_v3(r, mb);
    const float hn = ida_karekok(v.x * v.x + v.y * v.y);
    if (!(hn > 0.1f * n)) {
        red_isle(&k->red_mag, true, a->now_ns); /* yatay bileşen yetersiz */
        return;
    }
    const float e = ida_aci_sar(ida_atan2(v.y, v.x) - a->mag_dec);
    if (!k->yon_var) {
        yon_sifirla(k, a, e);
        k->yon_var = true;
        destek(&k->akis_mag, a->now_ns);
        red_isle(&k->red_mag, false, a->now_ns);
        return;
    }
    float h[N];
    sifirla(h);
    for (uint32_t j = 0u; j < 3u; j++) {
        h[IDA_KES_ITH + j] = r.m[2][j];
    }
    float dx[N];
    sifirla(dx);
    float nis = 0.0f;
    const bool kabul = ida_kes_skaler(k->P, h, -e, a->r_mag * a->r_mag, a->kapi_g1, dx, &nis);
    if (kabul) {
        ida_kes_enjekte(&k->xh, k->P, dx);
        destek(&k->akis_mag, a->now_ns);
    }
    red_isle(&k->red_mag, !kabul, a->now_ns);
    if (!kabul && red_surekli(&k->red_mag, a->now_ns, a->mag_reset_ns)) {
        /* Uzun süre kapıdan geçemeyen yön: manyetometreye sıfırlanır (geri dönüş yolu). */
        yon_sifirla(k, a, e);
        destek(&k->akis_mag, a->now_ns);
        red_isle(&k->red_mag, false, a->now_ns);
    }
}

/* İvmeölçerden yerçekimi yönü: ölçüm f̄ (≈ 20 ms ortalaması), model h = −R̂ᵀ g_n + b_a (araç
   ivmesi sıfır varsayılır; R_GRAV bunu kapsar). ||f̄|| − g sapması GRAV_ESIK'i aşarsa ölçüm
   kullanılmaz; altında σ sapmayla büyütülür (yüksek ivmede kısma, Ö1). Vektör kapısı bütün
   olarak (02 §2.3.4). Maliyeti üç skalerdir. */
static void grav_fuzyon(ida_kes_t *k, const ida_kes_ayar_t *a)
{
    const ida_v3_t fo = ida_v3_olcekle(k->g_top, 1.0f / k->g_sure);
    const float sapma = ida_v3_norm(fo) - a->g;
    const float asim = mutlak(sapma);
    if (!(asim <= a->grav_esik)) {
        return;
    }
    const float sig = a->r_grav * (1.0f + asim / a->grav_esik);
    const float rr = sig * sig;
    const ida_m3_t r = ida_q_dcm(k->xh.q);
    const ida_v3_t u = {a->g * r.m[2][0], a->g * r.m[2][1], a->g * r.m[2][2]};
    const float y[3] = {fo.x + u.x - k->xh.ba.x, fo.y + u.y - k->xh.ba.y, fo.z + u.z - k->xh.ba.z};
    float h[3][N];
    for (uint32_t i = 0u; i < 3u; i++) {
        sifirla(h[i]);
        h[i][IDA_KES_IBA + i] = 1.0f;
    }
    /* H_θ = −[u]× */
    h[0][IDA_KES_ITH + 1u] = u.z;
    h[0][IDA_KES_ITH + 2u] = -u.y;
    h[1][IDA_KES_ITH + 0u] = -u.z;
    h[1][IDA_KES_ITH + 2u] = u.x;
    h[2][IDA_KES_ITH + 0u] = u.y;
    h[2][IDA_KES_ITH + 1u] = -u.x;
    float S[3][3];
    for (uint32_t i = 0u; i < 3u; i++) {
        float ph[N];
        for (uint32_t m = 0u; m < N; m++) {
            float t = 0.0f;
            for (uint32_t j = 0u; j < N; j++) {
                t += k->P[m][j] * h[i][j];
            }
            ph[m] = t;
        }
        for (uint32_t j = 0u; j < 3u; j++) {
            float t = 0.0f;
            for (uint32_t m = 0u; m < N; m++) {
                t += h[j][m] * ph[m];
            }
            S[j][i] = t;
        }
        S[i][i] += rr;
    }
    float nis = 0.0f;
    const bool kabul = ida_kes_nis3(S, y, &nis) && (nis <= a->kapi_g3);
    red_isle(&k->red_grav, !kabul, a->now_ns);
    if (!kabul) {
        return;
    }
    /* R köşegen olduğundan beyazlatma birimdir; sıralı skalerler (kapı yukarıda). */
    float dx[N];
    sifirla(dx);
    for (uint32_t i = 0u; i < 3u; i++) {
        float ni = 0.0f;
        (void)ida_kes_skaler(k->P, h[i], y[i], rr, 0.0f, dx, &ni);
    }
    ida_kes_enjekte(&k->xh, k->P, dx);
    k->grav_destek_var = true;
    k->grav_destek_ns = a->now_ns;
}

/* ---- Kaydın işlenmesi ------------------------------------------------------------------ */

typedef struct {
    bool var;
    ida_v3_t ep;
    ida_v3_t ev;
    ida_v3_t eth;
    float ilerleme; /* s */
} duzeltme_t;

/* Ufkun bitirdiği kaydın çıkış durumuyla farkı; ki ilerletme döngüsünden sonra uygulanır. */
static void hata_kaydet(const ida_kes_t *k, const ida_kes_imu_kayit_t *r, duzeltme_t *d)
{
    d->var = true;
    d->ep = ida_v3_cikar(k->xh.p, r->p_o);
    d->ev = ida_v3_cikar(k->xh.v, r->v_o);
    /* Tutum farkı dünya çerçevesinde (soldan): q_h = Exp(e) ⊗ q_o(t_h). Aynı IMU dizisiyle
       yayılan sonraki çıkış kayıtlarında sol hata değişmez (q_o(t) = q_o(t_h) ⊗ ΔR, gerçek
       q_h ⊗ ΔR); yerel (sağ) fark ise gövde döndükçe yanlış eksene düşerdi (sol bulgu 1). */
    d->eth = ida_kes_q_log(ida_q_carp(k->xh.q, ida_q_eslenik(r->q_o)));
    d->ilerleme += r->dt;
}

/* Halkanın en eski kaydını işler: içine düşen ölçümler zamanlarında füzyonlanır. Bütçe
   yetmezse ölçüm beklemede kalır, kayıt yarım kalır ve false döner. */
static bool kayit_isle_d(ida_kes_t *k, const ida_kes_ayar_t *a, uint32_t *butce, bool zorla,
                         duzeltme_t *d)
{
    const uint32_t yer = k->imu_bas;
    const ida_kes_imu_kayit_t r = k->imu[yer]; /* w, f, t, dt (sıfırlamalar değiştirmez) */
    while ((k->olc_n > 0u) && (k->olc[0].t_ns <= r.t_ns)) {
        const ida_kes_olc_t o = k->olc[0];
        if (o.t_ns < k->th_ns) {
            k->n_gec++; /* ufka göre geç (02 §2.3.2) */
            ida_kes_olc_cikar(k);
            continue;
        }
        if (!zorla && (*butce < 1u)) {
            return false;
        }
        ufuk_yay(k, r.w, r.f, ida_kes_ns_s(o.t_ns - k->th_ns), a);
        k->th_ns = o.t_ns;
        if (o.tur == (uint8_t)IDA_SENS_BARO) {
            baro_fuzyon(k, a, &o);
        } else {
            mag_fuzyon(k, a, &o);
        }
        if (*butce > 0u) {
            (*butce)--;
        }
        ida_kes_olc_cikar(k);
    }
    ufuk_yay(k, r.w, r.f, ida_kes_ns_s(r.t_ns - k->th_ns), a);
    k->th_ns = r.t_ns;
    k->imu_bas = (uint8_t)(((uint32_t)k->imu_bas + 1u) % IDA_KES_IMU_N);
    k->imu_n--;

    /* Yerçekimi ölçümü birikimi; 20 ms dolunca (bütçe izin verirse) füzyon. */
    k->g_top = ida_v3_topla(k->g_top, ida_v3_olcekle(r.f, r.dt));
    k->g_sure += r.dt;
    if (k->g_sure >= 0.02f) {
        if (zorla || (*butce >= 3u)) {
            grav_fuzyon(k, a);
            *butce = (*butce >= 3u) ? (*butce - 3u) : 0u;
            const ida_v3_t sifir = {0.0f, 0.0f, 0.0f};
            k->g_top = sifir;
            k->g_sure = 0.0f;
        } else if (k->g_sure > 0.1f) {
            const ida_v3_t sifir = {0.0f, 0.0f, 0.0f};
            k->g_top = sifir; /* çok bekledi: atılır */
            k->g_sure = 0.0f;
        } else {
            /* sonraki çağrıya ertelenir */
        }
    }
    /* Çıkış durumu halkadaki güncel kayıttan okunur: bu kaydın işlenmesi sırasındaki
       sıfırlamalar (yön, derinlik) onu da değiştirmiştir. Kayıt çıkarılmış ama üzerine
       yazılmamıştır (yeni kayıt bu işlev döndükten sonra eklenir). */
    hata_kaydet(k, &k->imu[yer], d);
    return true;
}

static void duzeltme_uygula(ida_kes_t *k, const ida_kes_ayar_t *a, const duzeltme_t *d)
{
    if (!d->var) {
        return;
    }
    float kz = d->ilerleme / a->tau_o;
    if (!(kz <= 1.0f)) {
        kz = 1.0f;
    }
    const ida_v3_t dp = ida_v3_olcekle(d->ep, kz);
    const ida_v3_t dv = ida_v3_olcekle(d->ev, kz);
    ida_q_t dq;
    if (!ida_q_donme_vektorunden(ida_v3_olcekle(d->eth, kz), &dq)) {
        return; /* sonlu olmayan fark: sağlamlık denetimi ufukta yakalar */
    }
    ida_q_t b;
    k->p_o = ida_v3_topla(k->p_o, dp);
    k->v_o = ida_v3_topla(k->v_o, dv);
    k->q_o = ida_q_birimle(ida_q_carp(dq, k->q_o), &b) ? b : k->q_o;
    for (uint32_t i = 0u; i < k->imu_n; i++) {
        ida_kes_imu_kayit_t *r = &k->imu[((uint32_t)k->imu_bas + i) % IDA_KES_IMU_N];
        r->p_o = ida_v3_topla(r->p_o, dp);
        r->v_o = ida_v3_topla(r->v_o, dv);
        r->q_o = ida_q_birimle(ida_q_carp(dq, r->q_o), &b) ? b : r->q_o;
    }
    /* Kapanmamış fark (çıkış belirsizliğine eklenir; bulgu 7). */
    const float kalan = 1.0f - kz;
    k->art_p.x = kalan * mutlak(d->ep.x);
    k->art_p.y = kalan * mutlak(d->ep.y);
    k->art_p.z = kalan * mutlak(d->ep.z);
    k->art_v.x = kalan * mutlak(d->ev.x);
    k->art_v.y = kalan * mutlak(d->ev.y);
    k->art_v.z = kalan * mutlak(d->ev.z);
    k->art_th.x = kalan * mutlak(d->eth.x);
    k->art_th.y = kalan * mutlak(d->eth.y);
    k->art_th.z = kalan * mutlak(d->eth.z);
}

static bool kayit_isle(ida_kes_t *k, const ida_kes_ayar_t *a, uint32_t *butce, bool zorla)
{
    duzeltme_t d;
    (void)memset(&d, 0, sizeof(d));
    const bool bitti = kayit_isle_d(k, a, butce, zorla, &d);
    duzeltme_uygula(k, a, &d);
    return bitti;
}

void ida_kes_ufuk_ilerle(ida_kes_t *k, const ida_kes_ayar_t *a)
{
    duzeltme_t d;
    (void)memset(&d, 0, sizeof(d));
    if (k->imu_n > 0u) {
        const uint32_t son = ((uint32_t)k->imu_bas + (uint32_t)k->imu_n - 1u) % IDA_KES_IMU_N;
        const int64_t hedef = k->imu[son].t_ns - k->d_ns;
        uint32_t butce = IDA_KES_BUTCE;
        while ((k->imu_n > 0u) && (k->imu[k->imu_bas].t_ns <= hedef)) {
            if (!kayit_isle_d(k, a, &butce, false, &d)) {
                break;
            }
        }
    }
    /* Ufkun gerisinde kalmış ölçümler (ör. halka boşken) atılır ve sayılır. */
    while ((k->olc_n > 0u) && (k->olc[0].t_ns < k->th_ns)) {
        k->n_gec++;
        ida_kes_olc_cikar(k);
    }
    duzeltme_uygula(k, a, &d);
}
