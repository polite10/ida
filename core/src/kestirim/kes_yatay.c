/*
 * kes_yatay.c — yatay hız ve konum ölçümleri: DVL ve GNSS (02 §2.3.3, §2.3.4). Sahibi: W5
 * kestirim-yatay işi.
 *
 * Durum vektörü büyümez (kes_eskf.c'nin 15 hata durumu); bu dosya yalnız ölçüm modellerini,
 * kaynakların nitelendirilmesini ve sıfırlamaları ekler. Çerçeve ve işaret kabulleri:
 *   - p, v: NED, gövde referans noktası O_b; R = R(q): gövde → NED; tutum hatası sağdan yerel
 *     (R = R̂ (I + [δθ]×)); ω = ω_ham − b_g, b_g = b̂_g + δb_g.
 *   - Montaj kolları l (m) O_b'den sensöre, gövde çerçevesinde (x ileri, y sancak, z aşağı).
 *   - DVL: vel DVL'nin dibe göre hızı, DVL çerçevesinde (m/s); gövdeye R_dvl ile döner.
 *       h_b(x) = Rᵀ v + ω × l_dvl                         (gövde çerçevesi, üç eksen)
 *       H_v = R̂ᵀ,  H_θ = [R̂ᵀ v̂]×,  H_bg = [l_dvl]×
 *   - GNSS: anten konumu ve hızı NED'de (yatay iki bileşen).
 *       h_p(x) = (p + R l_a)_NE,          H_p = I₂ₓ₃,  H_θ = −(R̂ [l_a]×)_NE
 *       h_v(x) = (v + R (ω × l_a))_NE,    H_v = I₂ₓ₃,  H_θ = −(R̂ [ω̂ × l_a]×)_NE,
 *                                         H_bg = (R̂ [l_a]×)_NE
 *
 * Nitelendirme (ölçüm zamanında, ufukta, zaman sırasıyla): bir kaynak ancak KES_N_IYI ardışık
 * iyi örnekten sonra füzyonlanır (tek örnek kestirimi de geçerliliği de değiştirmez).
 * Nitelendirme sürerken iyi olmayan bir örnek sayımı baştan başlatır. Nitelikli kaynakta iyi
 * olmayan örnek yalnız atlanır (tek bir kayıp ping kaynağı düşürmez); iyi örnekler arasındaki
 * boşluk bayatlık sınırını aşarsa nitelik düşer ve sayım baştan başlar.
 *
 * Kapı, ret ve sıfırlama (W5 düzeltme turu; inceleme bulgu 1, 2, 4). IDA_EST_OK silahlanmanın ön
 * koşuludur: hiçbir sıfırlama OK'u korumaz ve destek üretmez.
 *   - Kapıdan (yᵀ S⁻¹ y ≤ γ; vektör bütün olarak) geçemeyen ölçüm füzyonlanmaz ve desteği
 *     yenilemez. Destek yalnız kapıdan geçen örnekten gelir.
 *   - Ret izi (red_*; duvar saati, KES_RED_MS kuralı) ilk rette başlar ve yalnız kabul edilen
 *     örnekte ya da sıfırlamada temizlenir; kaynağın yeniden nitelenmesi onu silmez.
 *   - KES_N_RED ardışık ret kaynağı "kuşkulu" yapar (kusku_*): ilgili geçerlilik süre dolmadan,
 *     örnek sayısıyla düşer. Kuşkuyu yalnız kapıdan geçen örnek kaldırır; sıfırlama kaldırmaz.
 *   - KES_N_RESET (> KES_N_RED) ardışık rette durum ölçüme sıfırlanır (geri dönüş yolu).
 *     Sıfırlama: OK adayını siler (KES_OK_MS baştan), desteği YENİLEMEZ, kaynağı kuşkulu
 *     bırakır (geçerlilik ancak sonraki, kapıdan geçen örnekle döner). Mutlak başvuru yokken
 *     (taze ve kuşkusuz GNSS konum desteği yok) yapılan hız sıfırlaması yatay konumu da
 *     geçersiz kılar: ret dizisi boyunca konum yanlış hızla tümlenmiştir; konum yeniden ancak
 *     GNSS'ten kurulunca geçerli olur. İlk fix'le konum kurulumu da bir sıfırlamadır.
 * Destek zamanı ölçümün zamanı değil füzyon anının duvar saatidir (inceleme bulgu 6; dilim 1 ile
 * aynı kalıp): ölçüm zamanına göre bayatlık sınırı T_*_STALE'den en çok kuyruk gecikmesi
 * (ufuk gecikmesi D ≤ 200 ms; bütçe ertelerse birkaç çağrı daha) ve taşıma süresi kadar uzar.
 */
#include "kestirim/kes_ic.h"

#include "mat/ida_math.h"

#define N IDA_KES_N

/* WGS-84 (yarı büyük eksen, birinci dış merkezliğin karesi) ve 1e-7 derece → rad. */
#define WGS_A 6378137.0f
#define WGS_E2 6.69437999014e-3f
#define E7_RAD 1.7453292519943295e-9f

/* ---- Yardımcılar ------------------------------------------------------------------------ */

static float kok(float x)
{
    return (x > 0.0f) ? ida_karekok(x) : 0.0f;
}

static void sifirla(float v[N])
{
    for (uint32_t i = 0u; i < N; i++) {
        v[i] = 0.0f;
    }
}

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

static uint8_t doyan_artir(uint8_t x)
{
    return (x < 255u) ? (uint8_t)(x + 1u) : (uint8_t)255u;
}

/* İyi örnek zincirini ilerletir; önceki iyi örnekle arasındaki boşluk sınırı aşıyorsa zincir
   baştan başlar. Dönüş: kaynak nitelikli (ardışık iyi örnek sayısı n_iyi'ye ulaştı). */
static bool nit_iyi(ida_kes_nit_t *n, int64_t t_ns, int64_t bosluk_ns, uint32_t n_iyi)
{
    if (n->son_var && ((t_ns - n->son_ns) > bosluk_ns)) {
        n->iyi_n = 0u;
        n->red_n = 0u;
    }
    n->son_var = true;
    n->son_ns = t_ns;
    n->iyi_n = doyan_artir(n->iyi_n);
    return (uint32_t)n->iyi_n >= n_iyi;
}

/* İyi olmayan örnek: nitelendirme sürüyorsa sayım baştan; nitelikli kaynakta yalnız atlanır. */
static void nit_kotu(ida_kes_nit_t *n, uint32_t n_iyi)
{
    if ((uint32_t)n->iyi_n < n_iyi) {
        n->iyi_n = 0u;
        n->red_n = 0u;
        n->son_var = false;
    }
}

static void nit_ret(ida_kes_nit_t *n, int64_t t_ns)
{
    if (n->red_n == 0u) {
        n->red_bas_ns = t_ns;
    }
    n->red_n = doyan_artir(n->red_n);
}

/* m (2 ya da 3) satırlı vektör ölçüm (02 §2.3.4): kapı S = H P Hᵀ + rk·I ile bütün olarak;
   geçerse sıralı skaler güncelleme (satır başına ölçüm varyansı ru) ve enjeksiyon. Satırların
   gürültüsü eşit ve bağımsızdır (DVL beyazlatılmış, GNSS eksenleri eş). rk ile ru'nun ayrı
   olması kapının bildirilen doğrulukla, kazancın büyütülmüş gürültüyle kurulmasına izin verir.
   dx: uygulanan düzeltme (kabulde). Dönüş: kabul. */
typedef struct {
    uint32_t m;
    float h[3][N];
    float y[3];
    float rk;
    float ru;
    float gamma;
} vek_t;

static bool vektor_fuzyon(ida_kes_t *k, const vek_t *o, float dx[N])
{
    float S[3][3] = {{1.0f, 0.0f, 0.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}};
    for (uint32_t i = 0u; i < o->m; i++) {
        float ph[N];
        for (uint32_t r = 0u; r < N; r++) {
            float t = 0.0f;
            for (uint32_t j = 0u; j < N; j++) {
                t += k->P[r][j] * o->h[i][j];
            }
            ph[r] = t;
        }
        for (uint32_t j = 0u; j < o->m; j++) {
            float t = 0.0f;
            for (uint32_t r = 0u; r < N; r++) {
                t += o->h[j][r] * ph[r];
            }
            S[j][i] = t;
        }
        S[i][i] += o->rk;
    }
    /* m = 2'de üçüncü satır ve sütun birimdir, y[2] = 0: aynı Cholesky iki boyutlu NIS verir. */
    const float y3[3] = {o->y[0], o->y[1], (o->m > 2u) ? o->y[2] : 0.0f};
    float nis = 0.0f;
    if (!ida_kes_nis3(S, y3, &nis) || !(nis <= o->gamma)) {
        return false;
    }
    sifirla(dx);
    for (uint32_t i = 0u; i < o->m; i++) {
        float ni = 0.0f;
        (void)ida_kes_skaler(k->P, o->h[i], o->y[i], o->ru, 0.0f, dx, &ni);
    }
    ida_kes_enjekte(&k->xh, k->P, dx);
    return true;
}

/* Halkadaki çıkış kayıtlarına ve çıkışa konum ya da hız farkı ekler (sıfırlama yumuşatılmaz;
   02 §2.3.2 (d)). */
static void cikisa_konum_ekle(ida_kes_t *k, float dn, float de)
{
    k->p_o.x += dn;
    k->p_o.y += de;
    for (uint32_t i = 0u; i < k->imu_n; i++) {
        ida_kes_imu_kayit_t *r = &k->imu[((uint32_t)k->imu_bas + i) % IDA_KES_IMU_N];
        r->p_o.x += dn;
        r->p_o.y += de;
    }
}

static void cikisa_hiz_ekle(ida_kes_t *k, ida_v3_t d)
{
    k->v_o = ida_v3_topla(k->v_o, d);
    for (uint32_t i = 0u; i < k->imu_n; i++) {
        ida_kes_imu_kayit_t *r = &k->imu[((uint32_t)k->imu_bas + i) % IDA_KES_IMU_N];
        r->v_o = ida_v3_topla(r->v_o, d);
    }
}

/* Hata durumunun [ilk, ilk + n) bileşenlerinin çapraz kovaryansını siler, varyanslarını var
   yapar. */
static void blok_sifirla(float P[N][N], uint32_t ilk, uint32_t n, float var)
{
    for (uint32_t b = ilk; b < (ilk + n); b++) {
        for (uint32_t i = 0u; i < N; i++) {
            P[b][i] = 0.0f;
            P[i][b] = 0.0f;
        }
    }
    for (uint32_t b = ilk; b < (ilk + n); b++) {
        P[b][b] = var;
    }
}

/* Yatay konum sıfırlaması (02 §2.3.4): p_NE ölçüme eşitlenir, çapraz kovaryansı silinir,
   varyansı var olur; aynı çağrıda çıkışa ve geçmişe sıçrama olarak uygulanır; konum sıfırlama
   sayacı artar (A4 §3.8 reset_counter). */
static void konum_sifirla(ida_kes_t *k, float n, float e, float var)
{
    const float dn = n - k->xh.p.x;
    const float de = e - k->xh.p.y;
    k->xh.p.x = n;
    k->xh.p.y = e;
    cikisa_konum_ekle(k, dn, de);
    blok_sifirla(k->P, IDA_KES_IP, 2u, var);
    k->reset_sayac = (uint8_t)(k->reset_sayac + 1u);
    k->n_konum_sifir++;
}

/* Hız sıfırlaması (geri dönüş yolu): ilk n_eks (2: yatay; 3: hepsi) bileşen ölçüme eşitlenir,
   çapraz kovaryansı silinir, varyansı var olur; çıkışa ve geçmişe aynı çağrıda uygulanır. Ret
   dizisi boyunca (t_ret, s) hız yanlış sayılır: yatay konum σ'sı |Δv|·t_ret kadar büyütülür. */
static void hiz_sifirla(ida_kes_t *k, ida_v3_t v, uint32_t n_eks, float var, float t_ret)
{
    ida_v3_t d = ida_v3_cikar(v, k->xh.v);
    if (n_eks < 3u) {
        d.z = 0.0f;
    }
    const float dh[2] = {d.x, d.y};
    for (uint32_t i = 0u; i < 2u; i++) {
        const float ek = ((dh[i] < 0.0f) ? -dh[i] : dh[i]) * ((t_ret > 0.0f) ? t_ret : 0.0f);
        const float sg = kok(k->P[IDA_KES_IP + i][IDA_KES_IP + i]) + ek;
        k->P[IDA_KES_IP + i][IDA_KES_IP + i] = sg * sg;
    }
    k->xh.v = ida_v3_topla(k->xh.v, d);
    cikisa_hiz_ekle(k, d);
    blok_sifirla(k->P, IDA_KES_IV, n_eks, var);
    ida_kes_P_duzelt(k->P);
    k->n_hiz_sifir++;
}

static void hiz_destegi(ida_kes_t *k, int64_t t_ns)
{
    k->hiz_fuz_var = true;
    k->hiz_fuz_ns = t_ns;
}

/* Mutlak başvuru: GNSS konumu taze (son kabul edilen fix T_GNSS_STALE içinde, duvar saati) ve
   konum kapısı kuşku altında değil. */
static bool gnss_konum_taze(const ida_kes_t *k, const ida_kes_ayar_t *a)
{
    return k->konum_var && !k->kusku_gnss_p && k->akis_gnss.destek_var &&
           ((a->now_ns - k->akis_gnss.destek_ns) <= a->gnss_stale_ns);
}

/* Geri dönüş yolunun hız sıfırlaması (dosya başı "Kapı, ret ve sıfırlama"): OK adayı silinir;
   mutlak başvuru yoksa yatay konum da geçersiz olur (yeniden GNSS'ten kurulana kadar). Destek
   üretmez. */
static void hiz_geri_donus(ida_kes_t *k, const ida_kes_ayar_t *a, ida_v3_t v, uint32_t n_eks,
                           float var, float t_ret)
{
    hiz_sifirla(k, v, n_eks, var, t_ret);
    k->ok_aday_var = false;
    if (!gnss_konum_taze(k, a)) {
        k->konum_var = false;
    }
}

/* Konumun GNSS'e sıfırlanması (ilk kurulum ve geri dönüş yolu): OK adayı silinir; destek
   üretmez, konum kapısı sonraki kabul edilen fix'e kadar kuşkuludur. */
static void konum_geri_donus(ida_kes_t *k, float n, float e, float var)
{
    konum_sifirla(k, n, e, var);
    k->konum_var = true;
    k->ok_aday_var = false;
    k->kusku_gnss_p = true;
}

/* Kapı kararının izi (nitelikli örnekte). Kabul: ret izi ve kuşku kalkar. Ret: iz başlar ya da
   sürer; ardışık ret KES_N_RED'e varınca kaynak kuşkulu olur. */
static void kapi_isle(ida_kes_red_t *red, bool *kusku, ida_kes_nit_t *nit, bool kabul,
                      const ida_kes_ayar_t *a, int64_t t_ns)
{
    red_isle(red, !kabul, a->now_ns);
    if (kabul) {
        nit->red_n = 0u;
        *kusku = false;
        return;
    }
    nit_ret(nit, t_ns);
    if ((uint32_t)nit->red_n >= a->n_red) {
        *kusku = true;
    }
}

/* ---- Ölçüm modelleri (öngörü ve Jacobian; birim testi sayısal türevle karşılaştırır) ------ */

void ida_kes_dvl_model(const ida_kes_nominal_t *x, ida_v3_t w_ham, ida_v3_t l, float hb[3],
                       float h[3][IDA_KES_N])
{
    const ida_m3_t rt = ida_m3_devrik(ida_q_dcm(x->q));
    const ida_v3_t vb = ida_m3_v3(rt, x->v);
    const ida_v3_t kol = ida_v3_dis(ida_v3_cikar(w_ham, x->bg), l);
    hb[0] = vb.x + kol.x;
    hb[1] = vb.y + kol.y;
    hb[2] = vb.z + kol.z;
    const ida_m3_t vx = ida_m3_capraz(vb);
    const ida_m3_t lx = ida_m3_capraz(l);
    for (uint32_t i = 0u; i < 3u; i++) {
        sifirla(h[i]);
        for (uint32_t j = 0u; j < 3u; j++) {
            h[i][IDA_KES_IV + j] = rt.m[i][j];
            h[i][IDA_KES_ITH + j] = vx.m[i][j];
            h[i][IDA_KES_IBG + j] = lx.m[i][j];
        }
    }
}

void ida_kes_gnss_hiz_model(const ida_kes_nominal_t *x, ida_v3_t w_ham, ida_v3_t l, float hv[2],
                            float h[2][IDA_KES_N])
{
    const ida_m3_t r = ida_q_dcm(x->q);
    const ida_v3_t kol_b = ida_v3_dis(ida_v3_cikar(w_ham, x->bg), l); /* ω̂ × l, gövde */
    const ida_v3_t kol_n = ida_m3_v3(r, kol_b);
    const ida_m3_t rcx = ida_m3_carp(r, ida_m3_capraz(kol_b));
    const ida_m3_t rlx = ida_m3_carp(r, ida_m3_capraz(l));
    hv[0] = x->v.x + kol_n.x;
    hv[1] = x->v.y + kol_n.y;
    for (uint32_t i = 0u; i < 2u; i++) {
        sifirla(h[i]);
        h[i][IDA_KES_IV + i] = 1.0f;
        for (uint32_t j = 0u; j < 3u; j++) {
            h[i][IDA_KES_ITH + j] = -rcx.m[i][j];
            h[i][IDA_KES_IBG + j] = rlx.m[i][j];
        }
    }
}

void ida_kes_gnss_konum_model(const ida_kes_nominal_t *x, ida_v3_t l, float hp[2],
                              float h[2][IDA_KES_N])
{
    const ida_m3_t r = ida_q_dcm(x->q);
    const ida_v3_t rl = ida_m3_v3(r, l);
    const ida_m3_t rlx = ida_m3_carp(r, ida_m3_capraz(l));
    hp[0] = x->p.x + rl.x;
    hp[1] = x->p.y + rl.y;
    for (uint32_t i = 0u; i < 2u; i++) {
        sifirla(h[i]);
        h[i][IDA_KES_IP + i] = 1.0f;
        for (uint32_t j = 0u; j < 3u; j++) {
            h[i][IDA_KES_ITH + j] = -rlx.m[i][j];
        }
    }
}

/* ---- Yerel düzlem ------------------------------------------------------------------------ */

bool ida_kes_gnss_ne(int32_t lat_e7, int32_t lon_e7, int32_t lat0_e7, int32_t lon0_e7, float ne[2])
{
    const int64_t dlat = (int64_t)lat_e7 - (int64_t)lat0_e7;
    int64_t dlon = (int64_t)lon_e7 - (int64_t)lon0_e7;
    if (dlon > 1800000000) {
        dlon -= INT64_C(3600000000);
    } else if (dlon < -1800000000) {
        dlon += INT64_C(3600000000);
    } else {
        /* sarma gerekmiyor */
    }
    const int64_t uzak = (int64_t)IDA_KES_GNSS_UZAK_E7;
    if ((dlat > uzak) || (dlat < -uzak) || (dlon > uzak) || (dlon < -uzak) ||
        (lat0_e7 > 890000000) || (lat0_e7 < -890000000)) {
        return false;
    }
    const float lat0 = (float)lat0_e7 * E7_RAD;
    const float s = ida_sin(lat0);
    const float c = ida_cos(lat0);
    const float w2 = 1.0f - WGS_E2 * s * s;
    const float w = ida_karekok(w2);
    const float rn = WGS_A / w;                          /* doğu-batı eğrilik yarıçapı N */
    const float rm = (WGS_A * (1.0f - WGS_E2)) / (w2 * w); /* meridyen eğrilik yarıçapı M */
    /* Farklar en çok 2·10⁶ birimdir: float'a dönüşüm tamdır (< 2²⁴). */
    const float n0 = rm * ((float)dlat * E7_RAD);
    const float e0 = rn * c * ((float)dlon * E7_RAD);
    const float t = s / c; /* |φ₀| ≤ 89°: c ≥ 0,017 */
    ne[0] = n0 + (e0 * e0 * t) / (2.0f * rn) + (1.5f * WGS_E2 * s * c * n0 * n0) / (w2 * rm);
    ne[1] = e0 - (e0 * n0 * t) / rn;
    return ida_sonlu(ne[0]) && ida_sonlu(ne[1]);
}

/* ---- Ölü hesap büyümesi ----------------------------------------------------------------- */

void ida_kes_dr_buyut(ida_kes_t *k, const ida_kes_ayar_t *a, float dt)
{
    /* Yatay hız ölçümle destekliyken konum hatası kat edilen yolla orantılı büyür (yön hatası
       ve DVL ölçek hatası sapma gibidir; beyaz süreç gürültüsü bunu vermez). P'nin köşegeni
       büyütülür: σ ← σ + DR_ORAN·|v_yatay|·dt. Eklenen matris köşegen ve negatif olmayandır;
       P pozitif tanımlı kalır. Hız desteksizken uygulanmaz: o durumda P zaten ivme gürültüsü
       ve sapma belirsizliğiyle büyür. */
    if (!k->hiz_fuz_var || ((k->th_ns - k->hiz_fuz_ns) > a->dvl_stale_ns)) {
        return;
    }
    const float vh = kok(k->xh.v.x * k->xh.v.x + k->xh.v.y * k->xh.v.y);
    const float ds = a->dr_oran * vh * dt;
    if (!(ds > 0.0f)) {
        return;
    }
    for (uint32_t i = 0u; i < 2u; i++) {
        const float sg = kok(k->P[IDA_KES_IP + i][IDA_KES_IP + i]) + ds;
        k->P[IDA_KES_IP + i][IDA_KES_IP + i] = sg * sg;
    }
}

/* ---- DVL (02 §2.3.3) -------------------------------------------------------------------- */

void ida_kes_dvl_fuzyon(ida_kes_t *k, const ida_kes_ayar_t *a, const ida_kes_yolc_t *o,
                        ida_v3_t w_ham)
{
    /* Kullanılabilirlik: başlık bayrağı temiz, hız geçerli, dip izi, FOM ve irtifa sınır içinde.
       Su izi (TRACK_BOTTOM = 0) suya göre hızdır; akıntı durumu olmadığından kullanılmaz.
       İrtifası bilinmeyen (−1) örnek en az irtifa koşulunu sağlamış sayılmaz. */
    const float fom = o->v[9];
    const float irtifa = o->v[10];
    const uint8_t gerek = (uint8_t)(IDA_KES_YB_HIZ | IDA_KES_YB_DIP);
    /* Sensörün bildirdiği kovaryansın en büyük köşegeni DVL_FOM_MAX²'yi aşıyorsa örnek bilgi
       taşımaz (inceleme bulgu 5): beyazlatma NIS'i ≈ 0 yapar, örnek kapıdan geçer ve hiçbir şey
       düzeltmeden desteği yenilerdi. FOM ile aynı eşik: ikisi de hız hatasının 1σ ölçüsüdür.
       Karşılaştırmalar NaN'da yanlıştır (örnek iyi sayılmaz). */
    const float kov_max = a->dvl_fom_max * a->dvl_fom_max;
    const bool kov_iyi = ((o->bayrak & IDA_KES_YB_KOV) == 0u) ||
                         ((o->v[3] <= kov_max) && (o->v[6] <= kov_max) && (o->v[8] <= kov_max));
    const bool iyi = ((o->bayrak & IDA_KES_YB_KULLANILMAZ) == 0u) && ((o->bayrak & gerek) == gerek) &&
                     (fom <= a->dvl_fom_max) && (irtifa >= a->dvl_alt_min) && kov_iyi;
    if (!iyi) {
        nit_kotu(&k->nit_dvl, a->n_iyi);
        return;
    }
    if (!nit_iyi(&k->nit_dvl, o->t_ns, a->dvl_stale_ns, a->n_iyi)) {
        /* Henüz nitelikli değil: füzyon yok. Süren ret izi ve kuşku korunur (inceleme bulgu 4):
           onları yalnız kabul edilen örnek (ret izini ayrıca sıfırlama) temizler. */
        return;
    }
    const ida_m3_t m_dvl = ida_q_dcm(ida_kes_q_euler(a->e_dvl.x, a->e_dvl.y, a->e_dvl.z));
    const ida_v3_t zd = {o->v[0], o->v[1], o->v[2]};
    const ida_v3_t zb = ida_m3_v3(m_dvl, zd);
    vek_t q;
    float hb[3];
    ida_kes_dvl_model(&k->xh, w_ham, a->l_dvl, hb, q.h);
    const ida_v3_t yv = {zb.x - hb[0], zb.y - hb[1], zb.z - hb[2]};

    /* R_b = R_dvl C R_dvlᵀ + R_DVL² I. Sensör kovaryansı yoksa ya da sonlu ve pozitif tanımlı
       değilse yalnız parametre (02 §2.3.3). R_DVL modellenmeyen hataların (ölçek, hizalama,
       zaman) tabanıdır ve sensörün kovaryansına eklenir. */
    const float rr = a->r_dvl * a->r_dvl;
    float rb[3][3] = {{rr, 0.0f, 0.0f}, {0.0f, rr, 0.0f}, {0.0f, 0.0f, rr}};
    if ((o->bayrak & IDA_KES_YB_KOV) != 0u) {
        float cs[3][3] = {{o->v[3], o->v[4], o->v[5]},
                          {o->v[4], o->v[6], o->v[7]},
                          {o->v[5], o->v[7], o->v[8]}};
        float cc[6];
        if (ida_kes_chol3(cs, cc)) {
            ida_m3_t cm = ida_m3_birim_deger();
            for (uint32_t i = 0u; i < 3u; i++) {
                for (uint32_t j = 0u; j < 3u; j++) {
                    cm.m[i][j] = cs[i][j];
                }
            }
            const ida_m3_t cb = ida_m3_carp(ida_m3_carp(m_dvl, cm), ida_m3_devrik(m_dvl));
            for (uint32_t i = 0u; i < 3u; i++) {
                for (uint32_t j = 0u; j < 3u; j++) {
                    /* Simetrik ortalama: dönüşün yuvarlaması simetriyi bozmasın. */
                    rb[i][j] += 0.5f * (cb.m[i][j] + cb.m[j][i]);
                }
            }
        }
    }
    float c[6];
    if (!ida_kes_chol3(rb, c)) {
        /* Taban pozitif olduğundan beklenmez (taşma); parametre R'ye dönülür. */
        const float kr = kok(rr);
        c[0] = kr;
        c[1] = 0.0f;
        c[2] = kr;
        c[3] = 0.0f;
        c[4] = 0.0f;
        c[5] = kr;
    }

    /* Beyazlatma: y' = L⁻¹ y, H' = L⁻¹ H (02 §2.3.4); beyazlatılmış ölçümün gürültüsü birimdir. */
    q.m = 3u;
    q.gamma = a->kapi_g3;
    q.rk = 1.0f;
    q.ru = 1.0f;
    q.y[0] = yv.x / c[0];
    q.y[1] = (yv.y - c[1] * q.y[0]) / c[2];
    q.y[2] = (yv.z - c[3] * q.y[0] - c[4] * q.y[1]) / c[5];
    for (uint32_t j = 0u; j < N; j++) {
        const float h0 = q.h[0][j] / c[0];
        const float h1 = (q.h[1][j] - c[1] * h0) / c[2];
        const float h2 = (q.h[2][j] - c[3] * h0 - c[4] * h1) / c[5];
        q.h[0][j] = h0;
        q.h[1][j] = h1;
        q.h[2][j] = h2;
    }
    float dx[N];
    const bool kabul = vektor_fuzyon(k, &q, dx);
    k->kapi_dvl_var = true;
    k->kapi_dvl_ns = a->now_ns;
    kapi_isle(&k->red_dvl, &k->kusku_dvl, &k->nit_dvl, kabul, a, o->t_ns);
    if (kabul) {
        k->akis_dvl.destek_var = true;
        k->akis_dvl.destek_ns = a->now_ns;
        hiz_destegi(k, o->t_ns);
        return;
    }
    k->n_dvl_red++;
    if ((uint32_t)k->nit_dvl.red_n >= a->n_reset) {
        /* Geri dönüş yolu: KES_N_RESET ardışık nitelikli örnek kapıdan geçemedi. Hız DVL'e
           sıfırlanır: v = R̂ (z_b − ω̂ × l). Varyans: R_b'nin en büyük köşegeni + tutum
           belirsizliğinin katkısı (|z_b|·σ_θ)². */
        const ida_v3_t kol = ida_v3_dis(ida_v3_cikar(w_ham, k->xh.bg), a->l_dvl);
        const ida_v3_t v_yeni = ida_m3_v3(ida_q_dcm(k->xh.q), ida_v3_cikar(zb, kol));
        float rmax = rb[0][0];
        rmax = (rb[1][1] > rmax) ? rb[1][1] : rmax;
        rmax = (rb[2][2] > rmax) ? rb[2][2] : rmax;
        float pth = k->P[IDA_KES_ITH][IDA_KES_ITH];
        for (uint32_t i = 1u; i < 3u; i++) {
            const float pt = k->P[IDA_KES_ITH + i][IDA_KES_ITH + i];
            pth = (pt > pth) ? pt : pth;
        }
        const float zn = ida_v3_norm(zb);
        const float t_ret = ida_kes_ns_s(o->t_ns - k->nit_dvl.red_bas_ns);
        hiz_geri_donus(k, a, v_yeni, 3u, rmax + zn * zn * pth, t_ret);
        k->nit_dvl.red_n = 0u;
        red_isle(&k->red_dvl, false, a->now_ns);
        k->kusku_dvl = true; /* destek yok: geçerlilik sonraki, kapıdan geçen örnekle döner */
    }
}

/* ---- GNSS (02 §2.3.3, §2.3.4) ------------------------------------------------------------ */

/* Yüzey: basınç destekli derinlik KES_YUZEY_D'nin altında (02 §2.3.5: GNSS yalnız YÜZEY).
   Derinlik basınçla desteklenmiyorsa rejim bilinmez ve GNSS kullanılmaz. */
static bool yuzeyde(const ida_kes_t *k, const ida_kes_ayar_t *a)
{
    return k->kalibre && k->akis_baro.destek_var &&
           ((a->now_ns - k->akis_baro.destek_ns) <= a->baro_stale_ns) && (k->xh.p.z < a->yuzey_d);
}

void ida_kes_gnss_fuzyon(ida_kes_t *k, const ida_kes_ayar_t *a, const ida_kes_yolc_t *o,
                         ida_v3_t w_ham)
{
    const float eph = o->v[2];
    float ne[2] = {0.0f, 0.0f};
    const bool iyi = ((o->bayrak & IDA_KES_YB_KULLANILMAZ) == 0u) &&
                     (o->fix >= (uint8_t)IDA_GNSS_FIX_3D) && (o->fix <= (uint8_t)IDA_GNSS_FIX_RTK) &&
                     (eph >= 0.0f) && (eph <= a->gnss_eph_max) && yuzeyde(k, a) &&
                     ida_kes_gnss_ne(o->lat_e7, o->lon_e7, a->orj_lat_e7, a->orj_lon_e7, ne);
    if (!iyi) {
        nit_kotu(&k->nit_gnss, a->n_iyi);
        return;
    }
    if (!nit_iyi(&k->nit_gnss, o->t_ns, a->gnss_stale_ns, a->n_iyi)) {
        /* Fix henüz kabul edilmedi (02 §2.3.4: ardışık iyi örnek). Ardışık ret sayımı baştan
           başlar; süren ret izleri ve kuşku korunur (inceleme bulgu 4). */
        k->nit_gnss_v.red_n = 0u;
        return;
    }
    k->kapi_gnss_var = true;
    k->kapi_gnss_ns = a->now_ns;
    const float sg = (eph > a->r_gnss_p) ? eph : a->r_gnss_p; /* σ_g */
    const float vg = sg * sg;

    /* ---- Hız (yatay iki bileşen) ---- */
    {
        vek_t q;
        float hv[2];
        ida_kes_gnss_hiz_model(&k->xh, w_ham, a->l_gnss, hv, q.h);
        q.m = 2u;
        q.gamma = a->kapi_g2;
        q.rk = a->r_gnss_v * a->r_gnss_v;
        q.ru = q.rk;
        q.y[0] = o->v[0] - hv[0];
        q.y[1] = o->v[1] - hv[1];
        q.y[2] = 0.0f;
        sifirla(q.h[2]);
        float dx[N];
        const bool kabul = vektor_fuzyon(k, &q, dx);
        kapi_isle(&k->red_gnss_v, &k->kusku_gnss_v, &k->nit_gnss_v, kabul, a, o->t_ns);
        if (kabul) {
            k->gnss_hiz_destek_var = true;
            k->gnss_hiz_destek_ns = a->now_ns;
            hiz_destegi(k, o->t_ns);
        } else {
            k->n_gnss_red++;
            if ((uint32_t)k->nit_gnss_v.red_n >= a->n_reset) {
                /* Geri dönüş yolu (02 §2.3.4): KES_N_RESET ardışık iyi örneğin hızı kapıdan
                   geçemedi; yatay hız GNSS hızına sıfırlanır: v_NE = v_a − (R̂ (ω̂ × l_a))_NE. */
                const ida_v3_t v_yeni = {k->xh.v.x + q.y[0], k->xh.v.y + q.y[1], k->xh.v.z};
                const float t_ret = ida_kes_ns_s(o->t_ns - k->nit_gnss_v.red_bas_ns);
                hiz_geri_donus(k, a, v_yeni, 2u, a->r_gnss_v * a->r_gnss_v, t_ret);
                k->nit_gnss_v.red_n = 0u;
                red_isle(&k->red_gnss_v, false, a->now_ns);
                k->kusku_gnss_v = true; /* destek yok: sonraki, kapıdan geçen örneğe kadar */
            }
        }
    }

    /* ---- Konum (yatay iki bileşen); model hız güncellemesinden sonraki durumda kurulur ---- */
    vek_t q;
    float hp[2];
    ida_kes_gnss_konum_model(&k->xh, a->l_gnss, hp, q.h);
    const float zn = ne[0] - (hp[0] - k->xh.p.x); /* O_b'nin ölçülen konumu (p = p_a − R l_a) */
    const float ze = ne[1] - (hp[1] - k->xh.p.y);
    if (!k->konum_var) {
        /* Konum bilinmiyor (hizalamadaki yatay konum bir kabuldür; orijin değişti; başvurusuz
           hız sıfırlaması yapıldı): nitelikli fix konumu kapısız kurar. Bu bir sıfırlamadır:
           destek üretmez; konum sonraki, kapıdan geçen fix'le geçerli olur. */
        konum_geri_donus(k, zn, ze, vg);
        k->nit_gnss.red_n = 0u;
        red_isle(&k->red_gnss_p, false, a->now_ns);
        return;
    }
    q.m = 2u;
    q.gamma = a->kapi_g2;
    /* Kapı alıcının bildirdiği doğrulukla (σ_g), kazanç büyütülmüş gürültüyle (GNSS_SIG_K·σ_g)
       kurulur: GNSS konum hatası zamanda ilişkilidir; beyaz sayılıp σ_g ile füzyonlanırsa süzgeç
       ortalamayla küçülmeyen bir hatayı küçülmüş sayar. */
    const float ru = (a->gnss_sig_k * sg) * (a->gnss_sig_k * sg);
    q.rk = vg;
    q.ru = ru;
    q.y[0] = ne[0] - hp[0];
    q.y[1] = ne[1] - hp[1];
    q.y[2] = 0.0f;
    sifirla(q.h[2]);
    float dx[N];
    const bool kabul = vektor_fuzyon(k, &q, dx);
    kapi_isle(&k->red_gnss_p, &k->kusku_gnss_p, &k->nit_gnss, kabul, a, o->t_ns);
    if (kabul) {
        /* Varyans tabanı: yatay konum, alıcının bildirdiği doğruluktan iyi sayılmaz. */
        for (uint32_t i = 0u; i < 2u; i++) {
            if (k->P[IDA_KES_IP + i][IDA_KES_IP + i] < vg) {
                k->P[IDA_KES_IP + i][IDA_KES_IP + i] = vg;
            }
        }
        /* Büyük düzeltme (ör. dalıştan sonraki ilk fix'ler) çıkışa yumuşatılmadan, sıçrama
           olarak ve sayaçla bildirilerek uygulanır; küçük düzeltme τ_o ile yumuşar. */
        const float dn = dx[IDA_KES_IP];
        const float de = dx[IDA_KES_IP + 1u];
        if (kok(dn * dn + de * de) > a->konum_sicra) {
            cikisa_konum_ekle(k, dn, de);
            k->reset_sayac = (uint8_t)(k->reset_sayac + 1u);
            k->n_konum_sifir++;
        }
        k->akis_gnss.destek_var = true;
        k->akis_gnss.destek_ns = a->now_ns;
        return;
    }
    k->n_gnss_red++;
    if ((uint32_t)k->nit_gnss.red_n >= a->n_reset) {
        /* Sıfırlama (02 §2.3.4): kalite KES_N_RESET ardışık örnek boyunca iyi ama konum kapıdan
           geçemiyor. Hızın kendi kapısı ve kendi sıfırlaması yukarıdadır (aynı sayı kuralı).
           Destek üretmez; konum sonraki, kapıdan geçen fix'le geçerli olur. */
        konum_geri_donus(k, zn, ze, vg);
        ida_kes_P_duzelt(k->P);
        k->nit_gnss.red_n = 0u;
        red_isle(&k->red_gnss_p, false, a->now_ns);
    }
}
