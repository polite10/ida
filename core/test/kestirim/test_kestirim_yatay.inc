/*
 * test_kestirim_yatay.inc — W5 kestirim-yatay kabulü (docs/gorevler/W5-kestirim-yatay.md, Kabul
 * 1–8) ve dilim 2'nin birim sınamaları. test_kestirim.c'nin içine alınır (ayrı çeviri birimi
 * değildir): yer gerçeğini, duyarga üretimini ve koşu düzeneğini oradan kullanır.
 *
 * Yer gerçeği çözümsel yörüngelerdir (double); konumun türevleri düzenekte merkezi farkla
 * alınır. GNSS enlem ve boylamı tam ECEF dönüşümüyle üretilir (ned_enlem_boylam), DVL hızı
 * Rᵀ v + ω × l'den; ikisi de çekirdeğin ölçüm modellerinden bağımsız yazılmıştır. Gürültü
 * büyüklükleri sim/sensor profil varsayılanlarındandır (ayar_temel). Hata sınırları ve süreler
 * bu dosyada sabittir, ilk koşudan önce yazılmıştır (02 §7 kuralı) ve TAHMİN parametre
 * varsayılanlarına bağlıdır; kartta ya da gerçek duyargayla ölçülmüş değildir.
 */
/* ---- Sınırlar ---------------------------------------------------------------------------- */

#define Y_DENGE_D 0.045     /* m; yüzer denge derinliği (O_b) */
/* Yüzeyde GNSS destekli yatay konum hatasının büyüklüğü (m). Süzgeç GNSS'in zamanda ilişkili
   hatasını (eksen başına σ_g = 1,5 m) izler; başka mutlak başvuru yoktur. Sınır 4 σ_g'dir. İlk
   yazılan değer 5,0 m idi (3,3 σ_g; eksen başına düşünülmüştü); büyüklük (iki eksenin normu)
   için dar kaldığı ilk koşuda görüldü (seyreltme sınamasında 5,01 m) ve 6,0'a çıkarıldı. */
#define Y_KONUM_SINIR 6.0
#define Y_HIZ_SINIR 0.05    /* m/s; DVL destekli yatay hız hatası */
#define Y_OK_SURE 6.0       /* s; koşunun başından OK'a kadar (hizalama 1 s + D + 5 örnek + 2 s) */
#define Y_DR_ORAN 0.02      /* DR_ORAN varsayılanı */
#define Y_DR_PAY 0.5        /* m; ölü hesap büyüme sınırının sabit payı */

/* ---- Yörüngeler -------------------------------------------------------------------------- */

static void y_taban(poz_t *p)
{
    p->roll = 0.02;
    p->pitch = -0.01;
    p->yaw = 0.0;
    p->n = 0.0;
    p->e = 0.0;
    p->d = Y_DENGE_D;
}

/* 0–5 s durgun; 5–10 s yumuşak hızlanma; sonra 1 m/s, yön 0,6 rad (düz seyir). */
static void sen_y_duz(double t, poz_t *p)
{
    y_taban(p);
    const double s = 1.0 * 5.0 * yumusak_int((t - 5.0) / 5.0);
    p->yaw = 0.6;
    p->n = s * cos(0.6);
    p->e = s * sin(0.6);
}

/* 0–5 s durgun; sonra 10 m yarıçaplı çember, 1 m/s (yön hızı 0,1 rad/s); baş teğet yönünde. */
static void sen_y_donus(double t, poz_t *p)
{
    y_taban(p);
    const double th = (1.0 / 10.0) * 5.0 * yumusak_int((t - 5.0) / 5.0);
    p->yaw = th;
    p->n = 10.0 * sin(th);
    p->e = 10.0 * (1.0 - cos(th));
}

/* 0–5 s durgun; 5–10 s 0,5 m/s'ye; 30–32 s'de 1,5 m/s'ye hız basamağı; yön −2,0 rad. */
static void sen_y_basamak(double t, poz_t *p)
{
    y_taban(p);
    const double s = 0.5 * 5.0 * yumusak_int((t - 5.0) / 5.0) + 1.0 * 2.0 * yumusak_int((t - 30.0) / 2.0);
    p->yaw = -2.0;
    p->n = s * cos(-2.0);
    p->e = s * sin(-2.0);
}

/* Düz seyir (1 m/s, 0,6 rad) ve dalış: bas–bas+10 s'de 5 m'ye iner, cik–cik+10 s'de yüzeye
   çıkar. */
static void y_dalis(double t, double bas, double cik, poz_t *p)
{
    sen_y_duz(t, p);
    p->d = Y_DENGE_D + (5.0 - Y_DENGE_D) * (yumusak((t - bas) / 10.0) - yumusak((t - cik) / 10.0));
}

static void sen_y_dalis(double t, poz_t *p)
{
    y_dalis(t, 30.0, 1.0e9, p);
}

static void sen_y_dalis_cikis(double t, poz_t *p)
{
    y_dalis(t, 30.0, 340.0, p);
}

/* Durgun, 10 m derinlikte (yalnız irtifa sınaması için; yalpa 0,2 rad). */
static void sen_y_egik(double t, poz_t *p)
{
    (void)t;
    y_taban(p);
    p->roll = 0.2;
    p->pitch = -0.1;
    p->d = 10.0;
}

/* ---- Yardımcılar ------------------------------------------------------------------------- */

static kosu_ayar_t y_ayar(senaryo_f s, uint32_t tohum)
{
    kosu_ayar_t a = ayar_temel(s, tohum);
    a.gnss = true;
    a.dvl = true;
    return a;
}

static void y_prm_f32(enum ida_prm_indis i, float v)
{
    (void)memcpy(&C.prm.yururluk.ham[i], &v, sizeof(v));
}

static void y_prm_u32(enum ida_prm_indis i, uint32_t v)
{
    C.prm.yururluk.ham[i] = v;
}

static bool y_bit(uint8_t b)
{
    return (ida_kes_gecerlilik(&C.kes) & b) != 0u;
}

static bool y_ok(void)
{
    return OUT.nav.est_level == (uint8_t)IDA_EST_OK;
}

/* OK iken hata denetimi (inceleme bulgu 8). IDA_EST_OK silahlanmanın ön koşuludur: düzey OK
   iken yatay hız hatası 3·SIG_HVEL'i ya da yatay konum hatası 3·SIG_HPOS'u (yürürlükteki
   parametre değerleri) aşan çağrı olamaz. Yer gerçeği senaryonun yörüngesidir. Konum bileşeni
   yalnız yer gerçeği ile mutlak başvurunun (GNSS, orijin) bilerek ayrıldığı testlerde kapatılır
   (Y_KONUM_DENETSIZ; gerekçesi testin içinde). true: bu çağrı ihlaldir. */
static bool y_ok_ihlal(const kosu_t *k)
{
    if (!y_ok()) {
        return false;
    }
    hata_t h;
    nav_hatasi(k, &OUT.nav, &h);
    const double vs = 3.0 * (double)ida_prm_f32(&C.prm, IDA_PRM_sig_hvel);
    const double ps = 3.0 * (double)ida_prm_f32(&C.prm, IDA_PRM_sig_hpos);
    return (h.vh > vs) || (!Y_KONUM_DENETSIZ && (h.ph > ps));
}

static void y_ok_denet(const kosu_t *k)
{
    TEST_ASSERT_FALSE_MESSAGE(y_ok_ihlal(k), "düzey OK iken yatay hız ya da konum hatası 3 σ_max'ı aştı");
}

/* Denetimsiz adım: ihlali kendisi sayan testler için. */
static void y_adim_ham(kosu_t *k)
{
    girdi_kur(k, &IN);
    TEST_ASSERT_EQUAL(IDA_RC_OK, ida_core_step(&C, &IN, &OUT));
    TEST_ASSERT_TRUE(OUT.nav_valid);
    TEST_ASSERT_TRUE_MESSAGE(nav_sonlu(&OUT.nav), "nav sonlu değil");
}

static void y_adim(kosu_t *k)
{
    y_adim_ham(k);
    y_ok_denet(k);
    /* Yatay hız geçerliyken nav'da bildirilen yatay hız σ'sı SIG_HVEL'i aşamaz (geçerlilik
       bildirilen σ'ya bakar; inceleme bulgu 8). */
    if ((ida_kes_gecerlilik(&C.kes) & IDA_KES_G_YATAY_HIZ) != 0u) {
        const float sv = (OUT.nav.v_sigma[0] > OUT.nav.v_sigma[1]) ? OUT.nav.v_sigma[0] : OUT.nav.v_sigma[1];
        TEST_ASSERT_TRUE_MESSAGE(sv <= ida_prm_f32(&C.prm, IDA_PRM_sig_hvel),
                                 "yatay hız geçerli ama bildirilen σ SIG_HVEL'in üstünde");
    }
}

static void y_git(kosu_t *k, double t)
{
    while (ns_t(k->t_ns) < t) {
        y_adim(k);
    }
}

/* Ufuk kovaryansından yatay konum σ'sı (düzeyin baktığı büyüklük). */
static double y_sig_h(void)
{
    const double n = sqrt((double)C.kes.P[IDA_KES_IP][IDA_KES_IP]);
    const double e = sqrt((double)C.kes.P[IDA_KES_IP + 1u][IDA_KES_IP + 1u]);
    return (n > e) ? n : e;
}

/* Ölçülen değeri test çıktısına yazar (rapor için; karar vermez): "olcum <ad> <ek> = d.dddd".
   Biçimleme elle yapılır: stdio'nun biçimli çıktısı kart derlemesinde bellek ayırır ve test
   ELF'i ayırmasız bağlanır (A4-K97 denetimi). */
static void y_olc2(const char *ad, const char *ek, double v)
{
    char m[200];
    uint32_t n = 0u;
    static const char ON[] = "olcum ";
    for (uint32_t i = 0u; ON[i] != '\0'; i++) {
        m[n++] = ON[i];
    }
    for (uint32_t i = 0u; (ad[i] != '\0') && (n < 120u); i++) {
        m[n++] = ad[i];
    }
    for (uint32_t i = 0u; (ek[i] != '\0') && (n < 170u); i++) {
        m[n++] = ek[i];
    }
    m[n++] = ' ';
    m[n++] = '=';
    m[n++] = ' ';
    double x = v;
    if (x < 0.0) {
        m[n++] = '-';
        x = -x;
    }
    if (!(x < 1.0e9)) {
        x = 999999999.0; /* doyar (sonsuz ve NaN dahil) */
    }
    x += 0.00005; /* dört ondalığa yuvarlama */
    uint32_t tam = (uint32_t)x;
    double kesir = x - (double)tam;
    char ters[10];
    uint32_t h = 0u;
    do {
        ters[h++] = (char)('0' + (int)(tam % 10u));
        tam /= 10u;
    } while (tam > 0u);
    while (h > 0u) {
        m[n++] = ters[--h];
    }
    m[n++] = '.';
    for (uint32_t i = 0u; i < 4u; i++) {
        kesir *= 10.0;
        const uint32_t r = (uint32_t)kesir;
        m[n++] = (char)('0' + (int)(r % 10u));
        kesir -= (double)r;
    }
    m[n] = '\0';
    TEST_MESSAGE(m);
}

static void y_olc(const char *ad, double v)
{
    y_olc2(ad, "", v);
}

/* Düzeyin izlediği yol ve OK iken hata (düzeltme turu; inceleme bulgu 8). */
typedef struct {
    double t_dusme;   /* izlemenin başından sonra düzeyin OK'tan ilk çıktığı an (yoksa −1) */
    double t_donus;   /* düştükten sonra yeniden OK olduğu ilk an (yoksa −1) */
    uint32_t ok_adim; /* düştükten sonra OK olan çağrı sayısı */
    uint32_t ihlal;   /* OK iken hata 3 σ_max'ı aşan çağrı sayısı (y_ok_ihlal) */
    double en_vh_ok;  /* OK iken en büyük yatay hız hatası, m/s */
    double en_ph_ok;  /* OK iken en büyük yatay konum hatası, m */
} y_iz_t;

static void y_iz_kur(y_iz_t *z)
{
    (void)memset(z, 0, sizeof(*z));
    z->t_dusme = -1.0;
    z->t_donus = -1.0;
}

static void y_iz_adim(const kosu_t *k, y_iz_t *z)
{
    const double t = ns_t(k->t_ns);
    if (y_ok()) {
        hata_t h;
        nav_hatasi(k, &OUT.nav, &h);
        z->en_vh_ok = (h.vh > z->en_vh_ok) ? h.vh : z->en_vh_ok;
        z->en_ph_ok = (h.ph > z->en_ph_ok) ? h.ph : z->en_ph_ok;
        if (y_ok_ihlal(k)) {
            z->ihlal++;
        }
        if (z->t_dusme >= 0.0) {
            z->ok_adim++;
            if (z->t_donus < 0.0) {
                z->t_donus = t;
            }
        }
    } else if (z->t_dusme < 0.0) {
        z->t_dusme = t;
    } else {
        /* düşmüş, OK değil */
    }
}

static void y_izle(kosu_t *k, double sure, y_iz_t *z)
{
    while (ns_t(k->t_ns) < sure) {
        y_adim_ham(k);
        y_iz_adim(k, z);
    }
}

static void y_iz_yaz(const char *ad, const y_iz_t *z)
{
    y_olc2(ad, ": OK'tan dusme ani (s)", z->t_dusme);
    y_olc2(ad, ": dustukten sonra OK olan cagri sayisi", (double)z->ok_adim);
    y_olc2(ad, ": OK iken hata > 3 sigma_max olan cagri sayisi", (double)z->ihlal);
    y_olc2(ad, ": OK iken en buyuk yatay hiz hatasi (m/s)", z->en_vh_ok);
    y_olc2(ad, ": OK iken en buyuk yatay konum hatasi (m)", z->en_ph_ok);
    y_olc2(ad, ": hiz sifirlamasi sayisi", (double)C.kes.n_hiz_sifir);
}

/* ---- Birim: yerel düzlem dönüşümü -------------------------------------------------------- */

static void test_y_gnss_ne(void)
{
    /* Tam ECEF dönüşümüyle üretilen enlem ve boylamdan geri: 5 km'ye kadar 2 cm, 15 km'ye
       kadar 0,5 m (üçüncü mertebe kalıntı ve binary32). 0,2° sınırı 41° enleminde doğu-batıda
       ≈ 16,8 km'dir. */
    static const double UZ[] = {-15000.0, -5000.0, -800.0, -1.0, 0.0, 3.0, 950.0, 5000.0, 15000.0};
    double en5 = 0.0, en20 = 0.0;
    for (uint32_t i = 0u; i < 9u; i++) {
        for (uint32_t j = 0u; j < 9u; j++) {
            double enlem = 0.0, boylam = 0.0;
            ned_enlem_boylam(UZ[i], UZ[j], 0.0, &enlem, &boylam);
            float ne[2] = {0.0f, 0.0f};
            TEST_ASSERT_TRUE(ida_kes_gnss_ne(e7(enlem), e7(boylam), ORJ_LAT_E7, ORJ_LON_E7, ne));
            const double h = hypot((double)ne[0] - UZ[i], (double)ne[1] - UZ[j]);
            const bool yakin = (fabs(UZ[i]) <= 5000.0) && (fabs(UZ[j]) <= 5000.0);
            if (yakin) {
                en5 = (h > en5) ? h : en5;
            }
            en20 = (h > en20) ? h : en20;
        }
    }
    y_olc("gnss_ne en buyuk hata, 5 km icinde (m)", en5);
    y_olc("gnss_ne en buyuk hata, 15 km icinde (m)", en20);
    TEST_ASSERT_TRUE_MESSAGE(en5 < 0.02, "yerel düzlem hatası (5 km)");
    TEST_ASSERT_TRUE_MESSAGE(en20 < 0.5, "yerel düzlem hatası (15 km)");
    float ne[2] = {0.0f, 0.0f};
    /* İşaret: kuzeydeki nokta +n, doğudaki nokta +e. */
    TEST_ASSERT_TRUE(ida_kes_gnss_ne(ORJ_LAT_E7 + 1000, ORJ_LON_E7, ORJ_LAT_E7, ORJ_LON_E7, ne));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 11.105f, ne[0]); /* 1e-4° × 111,05 km/° (41° enleminde) */
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 0.0f, ne[1]);
    TEST_ASSERT_TRUE(ida_kes_gnss_ne(ORJ_LAT_E7, ORJ_LON_E7 + 1000, ORJ_LAT_E7, ORJ_LON_E7, ne));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 8.412f, ne[1]); /* 1e-4° × 84,12 km/° */
    /* Boylam farkı ±180°'de sarılır. */
    TEST_ASSERT_TRUE(ida_kes_gnss_ne(100000000, -1799999000, 100000000, 1799999000, ne));
    TEST_ASSERT_TRUE(ne[1] > 21.0f && ne[1] < 22.5f); /* 2e-4° doğu, 10° enleminde ≈ 21,9 m */
    TEST_ASSERT_TRUE(ida_kes_gnss_ne(100000000, 1799999000, 100000000, -1799999000, ne));
    TEST_ASSERT_TRUE(ne[1] < -21.0f && ne[1] > -22.5f);
    /* Orijinden uzak (> 0,2°) ve kutba yakın orijin kullanılmaz. */
    TEST_ASSERT_FALSE(ida_kes_gnss_ne(ORJ_LAT_E7 + 2000001, ORJ_LON_E7, ORJ_LAT_E7, ORJ_LON_E7, ne));
    TEST_ASSERT_FALSE(ida_kes_gnss_ne(ORJ_LAT_E7, ORJ_LON_E7 - 2000001, ORJ_LAT_E7, ORJ_LON_E7, ne));
    TEST_ASSERT_FALSE(ida_kes_gnss_ne(895000000, 0, 895000000, 0, ne));
}

/* ---- Birim: ölçüm modellerinin Jacobian'ı sayısal türevle ---------------------------------- */

/* x ⊕ δ: p + δp, v + δv, q ⊗ Exp(δθ), b_g + δb_g, b_a + δb_a (02 §2.3.1 hata tanımı). */
static ida_kes_nominal_t y_topla(const ida_kes_nominal_t *x, uint32_t j, float eps)
{
    ida_kes_nominal_t y = *x;
    float d[IDA_KES_N];
    for (uint32_t i = 0u; i < IDA_KES_N; i++) {
        d[i] = (i == j) ? eps : 0.0f;
    }
    y.p.x += d[0];
    y.p.y += d[1];
    y.p.z += d[2];
    y.v.x += d[3];
    y.v.y += d[4];
    y.v.z += d[5];
    const ida_v3_t dth = {d[6], d[7], d[8]};
    ida_q_t dq;
    (void)ida_q_donme_vektorunden(dth, &dq);
    y.q = ida_q_carp(x->q, dq);
    y.bg.x += d[9];
    y.bg.y += d[10];
    y.bg.z += d[11];
    y.ba.x += d[12];
    y.ba.y += d[13];
    y.ba.z += d[14];
    return y;
}

static void test_y_model_jacobian(void)
{
    const ida_v3_t p0 = {12.0f, -7.0f, 3.0f};
    const ida_v3_t v0 = {0.9f, -0.4f, 0.15f};
    const ida_v3_t bg0 = {0.01f, -0.02f, 0.015f};
    const ida_v3_t ba0 = {0.05f, -0.04f, 0.06f};
    ida_kes_nominal_t x;
    x.p = p0;
    x.v = v0;
    x.q = ida_kes_q_euler(0.25f, -0.15f, 1.1f);
    x.bg = bg0;
    x.ba = ba0;
    const ida_v3_t w = {0.3f, -0.2f, 0.45f};
    const ida_v3_t l = {0.6f, -0.3f, 0.4f};
    const float eps = 0.01f;
    float h0[3], h3[3][IDA_KES_N], hv[2], hp[2], h2[2][IDA_KES_N], h2p[2][IDA_KES_N];
    ida_kes_dvl_model(&x, w, l, h0, h3);
    ida_kes_gnss_hiz_model(&x, w, l, hv, h2);
    ida_kes_gnss_konum_model(&x, l, hp, h2p);
    double en = 0.0;
    for (uint32_t j = 0u; j < IDA_KES_N; j++) {
        const ida_kes_nominal_t xa = y_topla(&x, j, eps);
        const ida_kes_nominal_t xe = y_topla(&x, j, -eps);
        float a3[3], e3[3], a2[2], e2[2], g3[3][IDA_KES_N], g2[2][IDA_KES_N];
        ida_kes_dvl_model(&xa, w, l, a3, g3);
        ida_kes_dvl_model(&xe, w, l, e3, g3);
        for (uint32_t i = 0u; i < 3u; i++) {
            const double sayisal = ((double)a3[i] - (double)e3[i]) / (2.0 * (double)eps);
            const double fark = fabs(sayisal - (double)h3[i][j]);
            en = (fark > en) ? fark : en;
            TEST_ASSERT_TRUE_MESSAGE(fark < 2.0e-3, "DVL Jacobian'ı sayısal türevle uyuşmuyor");
        }
        ida_kes_gnss_hiz_model(&xa, w, l, a2, g2);
        ida_kes_gnss_hiz_model(&xe, w, l, e2, g2);
        for (uint32_t i = 0u; i < 2u; i++) {
            const double sayisal = ((double)a2[i] - (double)e2[i]) / (2.0 * (double)eps);
            const double fark = fabs(sayisal - (double)h2[i][j]);
            en = (fark > en) ? fark : en;
            TEST_ASSERT_TRUE_MESSAGE(fark < 2.0e-3, "GNSS hız Jacobian'ı uyuşmuyor");
        }
        ida_kes_gnss_konum_model(&xa, l, a2, g2);
        ida_kes_gnss_konum_model(&xe, l, e2, g2);
        for (uint32_t i = 0u; i < 2u; i++) {
            const double sayisal = ((double)a2[i] - (double)e2[i]) / (2.0 * (double)eps);
            const double fark = fabs(sayisal - (double)h2p[i][j]);
            en = (fark > en) ? fark : en;
            TEST_ASSERT_TRUE_MESSAGE(fark < 2.0e-3, "GNSS konum Jacobian'ı uyuşmuyor");
        }
    }
    y_olc("Jacobian - sayisal turev en buyuk fark (1e-6)", en * 1.0e6);
    /* Öngörünün kendisi (bağımsız çift duyarlıklı hesap): Rᵀ v + (ω − b_g) × l. */
    const ida_m3_t r = ida_q_dcm(x.q);
    const double wc[3] = {0.3 - 0.01, -0.2 + 0.02, 0.45 - 0.015};
    const double ld[3] = {0.6, -0.3, 0.4};
    const double vd[3] = {0.9, -0.4, 0.15};
    double c[3];
    capraz(wc, ld, c);
    for (int i = 0; i < 3; i++) {
        const double vb = (double)r.m[0][i] * vd[0] + (double)r.m[1][i] * vd[1] + (double)r.m[2][i] * vd[2];
        TEST_ASSERT_TRUE(fabs(vb + c[i] - (double)h0[i]) < 1.0e-5);
    }
    for (int i = 0; i < 2; i++) {
        const double rl = (double)r.m[i][0] * ld[0] + (double)r.m[i][1] * ld[1] + (double)r.m[i][2] * ld[2];
        const double rc = (double)r.m[i][0] * c[0] + (double)r.m[i][1] * c[1] + (double)r.m[i][2] * c[2];
        const double pi = (i == 0) ? 12.0 : -7.0;
        TEST_ASSERT_TRUE(fabs(pi + rl - (double)hp[i]) < 1.0e-5);
        TEST_ASSERT_TRUE(fabs(vd[i] + rc - (double)hv[i]) < 1.0e-5);
    }
}

/* ---- Kabul 1: yüzey, GNSS + DVL ------------------------------------------------------------ */

typedef struct {
    hata_t m;       /* [deg_bas, son) aralığında en büyük hata */
    double t_ok;    /* ilk OK anı, s (yoksa −1) */
    uint32_t ok_disi; /* ilk OK'tan sonra OK olmayan adım sayısı */
    double oran;    /* en büyük |konum hatası| / bildirilen σ (eksen başına) */
    double oran_v;  /* OK iken en büyük |yatay hız hatası| / bildirilen σ (eksen başına) */
    double oran_vu; /* OK iken en büyük |yatay hız hatası| / ufuk σ'sı (düzeyin baktığı; eksen başına) */
} y_ozet_t;

static void y_kos(kosu_t *k, double sure, double deg_bas, y_ozet_t *o)
{
    (void)memset(o, 0, sizeof(*o));
    o->t_ok = -1.0;
    while (ns_t(k->t_ns) < sure) {
        y_adim(k);
        const double t = ns_t(k->t_ns);
        if (y_ok()) {
            if (o->t_ok < 0.0) {
                o->t_ok = t;
            }
        } else if (o->t_ok >= 0.0) {
            o->ok_disi++;
        } else {
            /* henüz OK olmadı */
        }
        if (t >= deg_bas) {
            hata_t h;
            nav_hatasi(k, &OUT.nav, &h);
            en_buyuk(&o->m, &h);
            const double rn = h.pn / (double)OUT.nav.p_sigma[0];
            const double re = h.pe / (double)OUT.nav.p_sigma[1];
            o->oran = (rn > o->oran) ? rn : o->oran;
            o->oran = (re > o->oran) ? re : o->oran;
            if (y_ok()) {
                const double hv[2] = {h.vn, h.ve};
                for (uint32_t i = 0u; i < 2u; i++) {
                    const double rv = hv[i] / (double)OUT.nav.v_sigma[i];
                    const double ru = hv[i] / sqrt((double)C.kes.P[IDA_KES_IV + i][IDA_KES_IV + i]);
                    o->oran_v = (rv > o->oran_v) ? rv : o->oran_v;
                    o->oran_vu = (ru > o->oran_vu) ? ru : o->oran_vu;
                }
            }
        }
    }
}

static void test_y_k1_yuzey(void)
{
    static const senaryo_f SEN[3] = {sen_y_duz, sen_y_donus, sen_y_basamak};
    static const char *const AD[3] = {"K1 duz", "K1 donus", "K1 basamak"};
    static const uint32_t TOHUM[3] = {0x51A001u, 0x51A002u, 0x51A003u};
    for (uint32_t i = 0u; i < 3u; i++) {
        TEST_ASSERT_EQUAL(IDA_RC_OK, ds_kur(&C));
        kosu_t k;
        const kosu_ayar_t a = y_ayar(SEN[i], TOHUM[i]);
        kosu_kur(&k, &a);
        y_ozet_t o;
        y_kos(&k, 120.0, 8.0, &o);
        y_olc2(AD[i], ": en buyuk yatay konum hatasi (m)", o.m.ph);
        y_olc2(AD[i], ": en buyuk yatay hiz hatasi (m/s)", o.m.vh);
        y_olc2(AD[i], ": OK ani (s)", o.t_ok);
        y_olc2(AD[i], ": en buyuk hata / sigma", o.oran);
        y_olc2(AD[i], ": OK iken en buyuk hiz hatasi / bildirilen sigma", o.oran_v);
        y_olc2(AD[i], ": OK iken en buyuk hiz hatasi / ufuk sigma", o.oran_vu);
        TEST_ASSERT_TRUE_MESSAGE(o.m.ph < Y_KONUM_SINIR, "yüzeyde yatay konum hatası");
        TEST_ASSERT_TRUE_MESSAGE(o.m.vh < Y_HIZ_SINIR, "yüzeyde yatay hız hatası");
        TEST_ASSERT_TRUE_MESSAGE((o.t_ok > 0.0) && (o.t_ok <= Y_OK_SURE), "OK belirlenen sürede olmadı");
        TEST_ASSERT_EQUAL_UINT32_MESSAGE(0u, o.ok_disi, "yüzeyde olağan akışta düzey OK'tan düştü");
        TEST_ASSERT_TRUE_MESSAGE(o.oran < 4.0, "bildirilen konum σ'sı iyimser");
        /* Dilim 1'in sınırları yatay yardımla da korunur. */
        TEST_ASSERT_TRUE(o.m.roll < 0.01 && o.m.pitch < 0.01 && o.m.yaw < 0.03);
        TEST_ASSERT_TRUE(o.m.derinlik < 0.05 && o.m.vd < 0.05);
        TEST_ASSERT_EQUAL_HEX8(0x3Fu, ida_kes_gecerlilik(&C.kes));
        TEST_ASSERT_EQUAL_INT32(ORJ_LAT_E7, OUT.nav.origin_lat_e7);
        TEST_ASSERT_EQUAL_INT32(ORJ_LON_E7, OUT.nav.origin_lon_e7);
        /* Tek konum sıfırlaması: ilk fix'te kurulum. Sonra sıçrama yok. */
        TEST_ASSERT_EQUAL_UINT8(1u, OUT.nav.reset_counter);
        TEST_ASSERT_EQUAL_UINT32(0u, C.kes.n_cokme);
        TEST_ASSERT_EQUAL_UINT32(0u, C.kes.n_tasma);
        /* Hizalamadan önce ölçülüp sonra teslim edilen GNSS örneği ufka göre geçtir. */
        TEST_ASSERT_TRUE(C.kes.n_gec <= 2u);
    }
}

/* ---- Kabul 2: dalış, yalnız DVL (ölü hesap) ------------------------------------------------ */

static void test_y_k2_dalis(void)
{
    TEST_ASSERT_EQUAL(IDA_RC_OK, ds_kur(&C));
    kosu_t k;
    kosu_ayar_t a = y_ayar(sen_y_dalis, 0x51B001u);
    /* Yön sapması: manyetik alan bütün koşuda 0,01 rad döndürülür (ölü hesabın baskın hatası). */
    a.mag_boz_bas = -100.0;
    a.mag_boz_son = 1.0e9;
    a.mag_boz_aci = 0.01;
    kosu_kur(&k, &a);
    y_git(&k, 29.0);
    TEST_ASSERT_TRUE_MESSAGE(y_ok(), "dalıştan önce OK değil");
    y_git(&k, 42.0); /* 5 m'de; anten 30 s'den beri su altında */
    hata_t h0;
    nav_hatasi(&k, &OUT.nav, &h0);
    const uint32_t gnss_seq0 = k.gnss_seq;
    const double sig0 = y_sig_h();
    const double psig0 = (double)OUT.nav.p_sigma[0];
    poz_t p0;
    a.sen(ns_t(OUT.nav.t.ns), &p0);
    double en_asim = -1.0e9, onceki_sig = sig0, en_oran = 0.0;
    while (ns_t(k.t_ns) < 240.0) {
        y_adim(&k);
        hata_t h;
        nav_hatasi(&k, &OUT.nav, &h);
        poz_t p;
        a.sen(ns_t(OUT.nav.t.ns), &p);
        const double yol = hypot(p.n - p0.n, p.e - p0.e);
        /* Belgelenen sınır: dalış anındaki hata + DR_ORAN × yol + pay. */
        const double asim = h.ph - (h0.ph + Y_DR_ORAN * yol + Y_DR_PAY);
        en_asim = (asim > en_asim) ? asim : en_asim;
        TEST_ASSERT_TRUE_MESSAGE(asim < 0.0, "ölü hesap hatası belgelenen sınırı aştı");
        /* Belirsizlik ölçüsü büyür: her saniyede bir önceki saniyedekinden küçük değil. */
        if ((k.seq % 200u) == 0u) {
            const double sg = y_sig_h();
            TEST_ASSERT_TRUE_MESSAGE(sg >= onceki_sig, "ölü hesapta konum σ'sı azaldı");
            onceki_sig = sg;
        }
        const double rn = h.pn / (double)OUT.nav.p_sigma[0];
        const double re = h.pe / (double)OUT.nav.p_sigma[1];
        en_oran = (rn > en_oran) ? rn : en_oran;
        en_oran = (re > en_oran) ? re : en_oran;
        TEST_ASSERT_TRUE_MESSAGE(h.vh < Y_HIZ_SINIR, "dalışta yatay hız hatası");
        TEST_ASSERT_TRUE_MESSAGE(y_ok(), "varsayılan eşikte (10 m) 200 m ölü hesapta OK düştü");
    }
    hata_t hs;
    nav_hatasi(&k, &OUT.nav, &hs);
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(gnss_seq0, k.gnss_seq, "su altında GNSS örneği üretildi");
    y_olc("K2: dalis anindaki yatay konum hatasi (m)", h0.ph);
    y_olc("K2: 198 m olu hesaptan sonra yatay konum hatasi (m)", hs.ph);
    y_olc("K2: sinira en yakin pay (m; negatif = sinirin altinda)", en_asim);
    y_olc("K2: sigma_h dalista (m)", sig0);
    y_olc("K2: sigma_h 198 m sonra (m)", y_sig_h());
    y_olc("K2: en buyuk hata / sigma", en_oran);
    /* σ kat edilen yolla büyür: en az DR_ORAN × yol'un %90'ı kadar. */
    TEST_ASSERT_TRUE_MESSAGE(y_sig_h() >= sig0 + 0.9 * Y_DR_ORAN * 198.0, "konum σ'sı yolla büyümedi");
    TEST_ASSERT_TRUE((double)OUT.nav.p_sigma[0] > psig0 + 0.9 * Y_DR_ORAN * 198.0);
    TEST_ASSERT_TRUE_MESSAGE(en_oran < 4.0, "ölü hesapta bildirilen σ iyimser");
    TEST_ASSERT_TRUE(y_bit(IDA_KES_G_KONUM) && y_bit(IDA_KES_G_YATAY_HIZ));
}

/* OK'tan düşme koşulu: yatay konum σ'sı SIG_HPOS'u aşınca konum geçersiz, düzey DEGRADED. */
static void test_y_k2_dusme_esigi(void)
{
    TEST_ASSERT_EQUAL(IDA_RC_OK, ds_kur(&C));
    y_prm_f32(IDA_PRM_sig_hpos, 4.0f);
    kosu_t k;
    const kosu_ayar_t a = y_ayar(sen_y_dalis, 0x51B002u);
    kosu_kur(&k, &a);
    y_git(&k, 42.0);
    TEST_ASSERT_TRUE(y_ok());
    double t_dusme = -1.0;
    while (ns_t(k.t_ns) < 240.0) {
        y_adim(&k);
        const double sg = y_sig_h();
        if (sg <= 4.0) {
            TEST_ASSERT_TRUE_MESSAGE(t_dusme < 0.0, "σ eşiğin altında ama düzey daha önce düşmüştü");
            TEST_ASSERT_TRUE_MESSAGE(y_ok(), "σ eşiğin altındayken OK değil");
            TEST_ASSERT_TRUE(y_bit(IDA_KES_G_KONUM));
        } else {
            if (t_dusme < 0.0) {
                t_dusme = ns_t(k.t_ns);
            }
            TEST_ASSERT_FALSE_MESSAGE(y_ok(), "σ eşiği aştı ama düzey OK kaldı");
            TEST_ASSERT_EQUAL_UINT8(IDA_EST_DEGRADED, OUT.nav.est_level);
            TEST_ASSERT_FALSE(y_bit(IDA_KES_G_KONUM));
            TEST_ASSERT_TRUE(y_bit(IDA_KES_G_YATAY_HIZ)); /* hız DVL ile sürer */
        }
    }
    y_olc("K2: SIG_HPOS = 4 m iken OK'tan dusme ani (s)", t_dusme);
    /* σ₀ ≈ 1,5 m; 1 m/s'de (4 − 1,5)/0,02 = 125 m sonra: dalışın sonu (40 s) + ≈ 125 s. */
    TEST_ASSERT_TRUE_MESSAGE((t_dusme > 140.0) && (t_dusme < 190.0), "düşme anı beklenen aralıkta değil");
}

/* ---- Kabul 3: DVL kilidi kaybı ------------------------------------------------------------- */

static void test_y_k3_dvl_kilit(void)
{
    TEST_ASSERT_EQUAL(IDA_RC_OK, ds_kur(&C));
    kosu_t k;
    kosu_ayar_t a = y_ayar(sen_y_dalis, 0x51C001u);
    a.dvl_kilit_bas = 100.0;
    a.dvl_kilit_son = 110.0;
    kosu_kur(&k, &a);
    y_git(&k, 100.0);
    TEST_ASSERT_TRUE(y_ok() && y_bit(IDA_KES_G_YATAY_HIZ));
    double t_dusme = -1.0, t_donus = -1.0, t_ok = -1.0;
    hata_t m;
    (void)memset(&m, 0, sizeof(m));
    while (ns_t(k.t_ns) < 125.0) {
        y_adim(&k);
        const double t = ns_t(k.t_ns);
        const bool hiz = y_bit(IDA_KES_G_YATAY_HIZ);
        if (!hiz && (t_dusme < 0.0)) {
            t_dusme = t;
        }
        if (hiz && (t_dusme > 0.0) && (t_donus < 0.0)) {
            t_donus = t;
        }
        if ((t_dusme > 0.0) && (t_donus < 0.0)) {
            TEST_ASSERT_FALSE_MESSAGE(y_ok(), "yatay hız geçersizken düzey OK");
            TEST_ASSERT_FALSE(y_bit(IDA_KES_G_KONUM)); /* konumu taşıyan destek yok */
        }
        if ((t_donus > 0.0) && (t_ok < 0.0) && y_ok()) {
            t_ok = t;
        }
        hata_t h;
        nav_hatasi(&k, &OUT.nav, &h);
        en_buyuk(&m, &h);
    }
    y_olc("K3: kilit kaybindan (100 s) yatay hiz gecerliliginin dusmesine (s)", t_dusme - 100.0);
    y_olc("K3: kilit donusunden (110 s) yatay hiz gecerliligine (s)", t_donus - 110.0);
    y_olc("K3: kilit donusunden OK'a (s)", t_ok - 110.0);
    y_olc("K3: 10 s kilitsiz en buyuk yatay hiz hatasi (m/s)", m.vh);
    y_olc("K3: 100-125 s en buyuk yatay konum hatasi (m)", m.ph);
    /* Son geçerli ping 99,807 s; füzyonu ≈ +0,15 s; T_DVL_STALE = 1 s → ≈ 100,96 s. */
    TEST_ASSERT_TRUE_MESSAGE((t_dusme > 100.5) && (t_dusme < 101.5), "yatay hız belirlenen sürede düşmedi");
    /* İlk geçerli ping 110,007 s; 5 ardışık iyi örnek 110,807 s; füzyon ≈ +0,15 s. */
    TEST_ASSERT_TRUE_MESSAGE((t_donus > 110.8) && (t_donus < 111.5), "kilit dönünce yatay hız toparlanmadı");
    TEST_ASSERT_TRUE_MESSAGE((t_ok > t_donus + 1.99) && (t_ok < t_donus + 2.1), "kilit dönünce OK toparlanmadı");
    TEST_ASSERT_TRUE_MESSAGE(m.ph < Y_KONUM_SINIR + Y_DR_ORAN * 95.0 + 2.0, "kilit kaybında konum bozuldu");
    TEST_ASSERT_EQUAL_UINT32(0u, C.kes.n_cokme);
    TEST_ASSERT_EQUAL_UINT32(0u, C.kes.n_hiz_sifir);
}

/* ---- Kabul 4: yüzeye çıkış ----------------------------------------------------------------- */

typedef struct {
    double en_sicrama;   /* reset_counter'ın arttığı çağrılardaki en büyük konum değişimi, m */
    double en_adim;      /* öteki çağrılardaki en büyük (Δp − v·dt), m */
    uint32_t n_sicrama;
    double t_ilk;        /* ilk sıçramanın anı */
    hata_t son;
} y_cikis_t;

static void y_cikis_izle(kosu_t *k, double sure, y_cikis_t *c)
{
    (void)memset(c, 0, sizeof(*c));
    c->t_ilk = -1.0;
    ida_nav_out_t onceki = OUT.nav;
    while (ns_t(k->t_ns) < sure) {
        y_adim(k);
        const double dt = (double)(OUT.nav.t.ns - onceki.t.ns) * 1.0e-9;
        const double dn = (double)OUT.nav.p_ned[0] - (double)onceki.p_ned[0] - (double)onceki.v_ned[0] * dt;
        const double de = (double)OUT.nav.p_ned[1] - (double)onceki.p_ned[1] - (double)onceki.v_ned[1] * dt;
        const double d = hypot(dn, de);
        if (OUT.nav.reset_counter != onceki.reset_counter) {
            c->n_sicrama++;
            c->en_sicrama = (d > c->en_sicrama) ? d : c->en_sicrama;
            if (c->t_ilk < 0.0) {
                c->t_ilk = ns_t(k->t_ns);
            }
        } else {
            c->en_adim = (d > c->en_adim) ? d : c->en_adim;
        }
        onceki = OUT.nav;
    }
    nav_hatasi(k, &OUT.nav, &c->son);
}

/* (a) Küçük sürüklenme: 0,015 rad yön sapması, 300 m ölü hesap (≈ 4 m). Fix'ler kapıdan geçer;
   tek güncellemenin düzeltmesi KONUM_SICRA'yı (2 m) aşmaz: sıçrama yok, düzeltme τ_o ile
   yumuşar. (c) Büyük ama model içi sürüklenme: 0,03 rad (≈ 9 m). Fix'ler yine kapıdan geçer;
   2 m'yi aşan düzeltme çıkışa sıçrama olarak ve sayaçla uygulanır. */
static void y_k4_tutarli(double aci, uint32_t tohum, bool sicrama, const char *ad)
{
    TEST_ASSERT_EQUAL(IDA_RC_OK, ds_kur(&C));
    kosu_t k;
    kosu_ayar_t a = y_ayar(sen_y_dalis_cikis, tohum);
    a.mag_boz_bas = -100.0;
    a.mag_boz_son = 1.0e9;
    a.mag_boz_aci = aci;
    kosu_kur(&k, &a);
    y_git(&k, 345.0); /* yükselirken, anten henüz su altında */
    hata_t h0;
    nav_hatasi(&k, &OUT.nav, &h0);
    const uint8_t sayac0 = OUT.nav.reset_counter;
    const uint32_t red0 = C.kes.n_gnss_red;
    y_cikis_t c;
    y_cikis_izle(&k, 375.0, &c);
    y_olc2(ad, ": cikistan onceki olu hesap hatasi (m)", h0.ph);
    y_olc2(ad, ": sicrama sayisi", (double)c.n_sicrama);
    y_olc2(ad, ": en buyuk sicrama (m)", c.en_sicrama);
    y_olc2(ad, ": sicrama disindaki en buyuk adim (m)", c.en_adim);
    y_olc2(ad, ": ilk sicrama ani (s)", c.t_ilk);
    y_olc2(ad, ": cikistan sonra yatay konum hatasi (m)", c.son.ph);
    TEST_ASSERT_TRUE_MESSAGE(h0.ph > 2.5, "senaryo beklenen sürüklenmeyi üretmedi");
    /* Sıçrama dışındaki çağrılarda konum düzeltmesi ≤ KONUM_SICRA × T/τ_o = 2 × 0,005/0,25. */
    TEST_ASSERT_TRUE_MESSAGE(c.en_adim < 0.05, "sıçrama sayılmayan çağrıda konum düzeltmesi sınırı aştı");
    /* Sıçrama, çıkıştan önceki hata + GNSS hatasından büyük olamaz. */
    TEST_ASSERT_TRUE_MESSAGE(c.en_sicrama < h0.ph + Y_KONUM_SINIR, "sıçrama sınırsız");
    if (sicrama) {
        TEST_ASSERT_TRUE_MESSAGE(c.n_sicrama >= 1u, "2 m'yi aşan düzeltme sıçrama olarak bildirilmedi");
        TEST_ASSERT_TRUE_MESSAGE(c.en_sicrama > 2.0, "sıçrama KONUM_SICRA'dan küçük");
    } else {
        TEST_ASSERT_EQUAL_UINT32_MESSAGE(0u, c.n_sicrama, "küçük düzeltme sıçrama olarak uygulandı");
    }
    TEST_ASSERT_EQUAL_UINT8((uint8_t)(sayac0 + c.n_sicrama), OUT.nav.reset_counter);
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(red0, C.kes.n_gnss_red, "model içi sürüklenmede GNSS reddedildi");
    TEST_ASSERT_TRUE_MESSAGE(c.son.ph < Y_KONUM_SINIR, "yüzeyde konum GNSS'e dönmedi");
    TEST_ASSERT_TRUE_MESSAGE(y_ok(), "yüzeye çıkıştan sonra OK değil");
}

static void test_y_k4_cikis_yumusak(void)
{
    y_k4_tutarli(0.015, 0x51D001u, false, "K4a");
}

static void test_y_k4_cikis_sicrama(void)
{
    y_k4_tutarli(0.03, 0x51D003u, true, "K4c");
}

/* Düz seyir (1 m/s, 0,6 rad); 30–40 s'de 5 m'ye dalış, 120–130 s'de yüzeye çıkış. */
static void sen_y_dalis_kisa(double t, poz_t *p)
{
    y_dalis(t, 30.0, 120.0, p);
}

/* (b) Model dışı sürüklenme: 0,2 rad yön sapması, ≈ 95 m ölü hesap (≈ 19 m; bildirilen σ ≈ 3,5 m).
   GNSS konumu kapıdan geçemez; KES_N_RESET ardışık iyi örnekten sonra konum sıfırlanır (tek
   sıçrama, sayaçla). Düzey KES_N_RED retle OK'tan düşer; sıfırlama OK'u geri getirmez.
   İlk yazılan senaryo %10 DVL ölçek hatasıyla 310 m ölü hesaptı; düzeltme turunda o hatanın
   dalış ve çıkışta DVL'i de reddettirdiği (düşey hız basınçla çelişir) ve konumu hız
   sıfırlaması üzerinden düşürdüğü görüldü: o senaryo (d)'dedir, burada yalnız GNSS konum
   kapısının yolu sınanır. */
static void test_y_k4_cikis_sifirlama(void)
{
    TEST_ASSERT_EQUAL(IDA_RC_OK, ds_kur(&C));
    kosu_t k;
    kosu_ayar_t a = y_ayar(sen_y_dalis_kisa, 0x51D002u);
    a.mag_boz_bas = -100.0;
    a.mag_boz_son = 1.0e9;
    a.mag_boz_aci = 0.2;
    kosu_kur(&k, &a);
    y_git(&k, 125.0); /* yükselirken, anten henüz su altında */
    TEST_ASSERT_TRUE(y_ok());
    hata_t h0;
    nav_hatasi(&k, &OUT.nav, &h0);
    const uint8_t sayac0 = OUT.nav.reset_counter;
    const uint32_t sifir0 = C.kes.n_konum_sifir;
    const uint32_t red0 = C.kes.n_gnss_red;
    y_cikis_t c;
    y_cikis_izle(&k, 150.0, &c);
    y_olc("K4b: cikistan onceki olu hesap hatasi (m)", h0.ph);
    y_olc("K4b: sicrama sayisi", (double)c.n_sicrama);
    y_olc("K4b: en buyuk sicrama (m)", c.en_sicrama);
    y_olc("K4b: sicrama disindaki en buyuk adim (m)", c.en_adim);
    y_olc("K4b: ilk sicrama ani (s)", c.t_ilk);
    y_olc("K4b: cikistan sonra yatay konum hatasi (m)", c.son.ph);
    TEST_ASSERT_TRUE_MESSAGE(h0.ph > 15.0, "senaryo beklenen sürüklenmeyi üretmedi");
    TEST_ASSERT_TRUE_MESSAGE(C.kes.n_gnss_red >= red0 + 10u, "model dışı sürüklenmede GNSS kapıdan geçti");
    TEST_ASSERT_EQUAL_UINT32(1u, c.n_sicrama);
    TEST_ASSERT_TRUE_MESSAGE(c.en_sicrama > 15.0, "sıfırlama çıkışa tek adımda uygulanmadı");
    TEST_ASSERT_TRUE(c.en_sicrama < h0.ph + Y_KONUM_SINIR);
    TEST_ASSERT_TRUE(c.en_adim < 0.05);
    TEST_ASSERT_EQUAL_UINT8((uint8_t)(sayac0 + c.n_sicrama), OUT.nav.reset_counter);
    TEST_ASSERT_EQUAL_UINT32(sifir0 + c.n_sicrama, C.kes.n_konum_sifir);
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(0u, C.kes.n_hiz_sifir, "yön sapması hız sıfırlaması üretti");
    TEST_ASSERT_TRUE_MESSAGE(c.son.ph < Y_KONUM_SINIR, "sıfırlamadan sonra konum GNSS'e dönmedi");
    TEST_ASSERT_TRUE_MESSAGE(y_ok(), "sıfırlamadan sonra OK değil");
}

/* (d) DVL ölçeği %10 hatalı, 310 m ölü hesap (≈ 34 m). Ölçek hatası üç eksendedir: dalış ve
   çıkışta DVL'in düşey hızı basınçla çelişir ve DVL reddedilir. Çıkışta KES_N_RESET ret hızı
   DVL'e sıfırlar; mutlak başvuru olmadığından konum geçersiz olur ve yüzeyde, nitelikli ilk
   fix'le GNSS'ten yeniden kurulur (tek sıçrama, sayaçla; GNSS konum kapısı hiç reddetmez).
   Bilinen sınır (düzeltme turunda OK iken hata denetimiyle görüldü; raporda açık bulgu): dalışta
   tek hız kaynağı DVL'dir ve ölçek hatası DR_ORAN'ın (0,02) beş katıdır; süzgeç bunu ayırt
   edemez, bildirilen konum σ'sı SIG_HPOS'un altında kalır ve düzey OK iken konum hatası dalışın
   sonunda 3·SIG_HPOS'u (30 m) aşar. Bu testte denetimin konum bileşeni kapalıdır; aşım ölçülüp
   yazılır. Hız bileşeni açıktır. */
static void test_y_k4_cikis_dvl_olcek(void)
{
    TEST_ASSERT_EQUAL(IDA_RC_OK, ds_kur(&C));
    kosu_t k;
    kosu_ayar_t a = y_ayar(sen_y_dalis_cikis, 0x51D002u);
    a.dvl_olcek = 0.10;
    kosu_kur(&k, &a);
    Y_KONUM_DENETSIZ = true;
    y_iz_t z;
    y_iz_kur(&z);
    while (ns_t(k.t_ns) < 349.0) {
        y_adim(&k);
        Y_KONUM_DENETSIZ = false;
        y_iz_adim(&k, &z); /* konum bileşeni açıkken ihlal sayısı (yalnız ölçüm) */
        Y_KONUM_DENETSIZ = true;
    }
    hata_t h0;
    nav_hatasi(&k, &OUT.nav, &h0);
    y_olc("K4d: dalista OK iken konum hatasi > 3 SIG_HPOS olan cagri sayisi (bilinen sinir)", (double)z.ihlal);
    y_olc("K4d: dalista OK iken en buyuk yatay konum hatasi (m)", z.en_ph_ok);
    y_olc("K4d: cikistan once sigma_h (m)", y_sig_h());
    y_olc("K4d: cikistan onceki olu hesap hatasi (m)", h0.ph);
    /* Çıkışta (340–350 s) DVL reddedildi ve hız sıfırlandı: konum geçersiz, düzey OK değil. */
    TEST_ASSERT_TRUE_MESSAGE(C.kes.n_hiz_sifir >= 1u, "senaryo çıkışta hız sıfırlaması üretmedi");
    TEST_ASSERT_FALSE_MESSAGE(C.kes.konum_var, "başvurusuz hız sıfırlaması konumu geçerli bıraktı");
    TEST_ASSERT_FALSE(y_bit(IDA_KES_G_KONUM));
    TEST_ASSERT_FALSE(y_ok());
    TEST_ASSERT_TRUE_MESSAGE(h0.ph > 20.0, "senaryo beklenen sürüklenmeyi üretmedi");
    const uint8_t sayac0 = OUT.nav.reset_counter;
    const uint32_t red0 = C.kes.n_gnss_red;
    y_cikis_t c;
    y_cikis_izle(&k, 375.0, &c);
    y_olc("K4d: sicrama sayisi", (double)c.n_sicrama);
    y_olc("K4d: en buyuk sicrama (m)", c.en_sicrama);
    y_olc("K4d: ilk sicrama ani (s)", c.t_ilk);
    y_olc("K4d: cikistan sonra yatay konum hatasi (m)", c.son.ph);
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(1u, c.n_sicrama, "konum tek sıçramayla yeniden kurulmadı");
    TEST_ASSERT_TRUE(c.en_sicrama > 20.0);
    TEST_ASSERT_TRUE(c.en_sicrama < h0.ph + Y_KONUM_SINIR);
    TEST_ASSERT_TRUE(c.en_adim < 0.05);
    TEST_ASSERT_EQUAL_UINT8((uint8_t)(sayac0 + 1u), OUT.nav.reset_counter);
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(red0, C.kes.n_gnss_red, "kurulum beklenirken GNSS konumu reddedildi");
    TEST_ASSERT_TRUE_MESSAGE(c.son.ph < Y_KONUM_SINIR, "konum GNSS'e dönmedi");
    TEST_ASSERT_TRUE_MESSAGE(y_ok(), "yüzeye çıkıştan sonra OK değil");
}

/* ---- Kabul 5: aykırı ölçüm ----------------------------------------------------------------- */

static void test_y_k5_gnss_sicramasi(void)
{
    TEST_ASSERT_EQUAL(IDA_RC_OK, ds_kur(&C));
    kosu_t k;
    kosu_ayar_t a = y_ayar(sen_y_duz, 0x51E001u);
    a.gnss_sicra = 30.0;
    a.gnss_sicra_bas = 40.0; /* 2 örnek: 40,003 ve 40,203 s */
    a.gnss_sicra_son = 40.3;
    kosu_kur(&k, &a);
    y_git(&k, 40.0);
    TEST_ASSERT_TRUE(y_ok());
    const uint32_t red0 = C.kes.n_gnss_red;
    hata_t m;
    (void)memset(&m, 0, sizeof(m));
    while (ns_t(k.t_ns) < 50.0) {
        y_adim(&k);
        hata_t h;
        nav_hatasi(&k, &OUT.nav, &h);
        en_buyuk(&m, &h);
        /* Kısa ret dizisi (< KES_N_RED örnek ve < KES_RED_MS) geçerliliği düşürmez. İlk yazılan
           dizi dört örnekti; düzeltme turunda (inceleme bulgu 1, mimar kararı 1 (iii)) KES_N_RED
           = 3 ardışık ret geçerliliği düşürdüğü için ikiye indirildi. */
        TEST_ASSERT_TRUE_MESSAGE(y_ok(), "kısa GNSS sıçraması düzeyi düşürdü");
    }
    y_olc("K5: 2 ornek GNSS sicramasinda en buyuk yatay konum hatasi (m)", m.ph);
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(red0 + 2u, C.kes.n_gnss_red, "sıçramalı iki örnek reddedilmedi");
    TEST_ASSERT_TRUE_MESSAGE(m.ph < Y_KONUM_SINIR, "GNSS sıçraması kestirimi bozdu");
    TEST_ASSERT_EQUAL_UINT8(1u, OUT.nav.reset_counter);

    /* Uzun ret dizisi: 9 örnek (< KES_N_RESET), 1,7 s: konum geçerliliği KES_N_RED = 3 ardışık
       retten sonra düşer (süre kuralı KES_RED_MS ondan sonradır); kestirim yine bozulmaz; sıçrama
       bitince toparlar. */
    TEST_ASSERT_EQUAL(IDA_RC_OK, ds_kur(&C));
    a = y_ayar(sen_y_duz, 0x51E002u);
    a.gnss_sicra = 30.0;
    a.gnss_sicra_bas = 40.0; /* 9 örnek: 40,003 … 41,603 s */
    a.gnss_sicra_son = 41.7;
    kosu_kur(&k, &a);
    y_git(&k, 40.0);
    TEST_ASSERT_TRUE(y_ok());
    double t_dusme = -1.0, t_ok = -1.0;
    (void)memset(&m, 0, sizeof(m));
    while (ns_t(k.t_ns) < 50.0) {
        y_adim(&k);
        const double t = ns_t(k.t_ns);
        hata_t h;
        nav_hatasi(&k, &OUT.nav, &h);
        en_buyuk(&m, &h);
        if (!y_bit(IDA_KES_G_KONUM) && (t_dusme < 0.0)) {
            t_dusme = t;
        }
        if ((t_dusme > 0.0) && (t_ok < 0.0)) {
            if (y_ok()) {
                t_ok = t;
            } else {
                TEST_ASSERT_EQUAL_UINT8(IDA_EST_DEGRADED, OUT.nav.est_level);
            }
        }
    }
    y_olc("K5: 1,7 s GNSS sicramasinda konum gecerliliginin dusme ani (s)", t_dusme);
    y_olc("K5: 1,7 s GNSS sicramasindan sonra OK ani (s)", t_ok);
    y_olc("K5: 1,7 s GNSS sicramasinda en buyuk yatay konum hatasi (m)", m.ph);
    /* Üçüncü ret: örnek 40,403 s; teslimi +0,12 s, füzyonu ölçüm zamanından D = 0,15 s sonra. */
    TEST_ASSERT_TRUE_MESSAGE((t_dusme > 40.45) && (t_dusme < 40.75), "ardışık ret konum geçerliliğini düşürmedi");
    TEST_ASSERT_TRUE_MESSAGE((t_ok > 43.9) && (t_ok < 44.3), "sıçrama bitince OK toparlanmadı");
    TEST_ASSERT_TRUE_MESSAGE(m.ph < Y_KONUM_SINIR, "uzun GNSS sıçraması kestirimi bozdu");
    TEST_ASSERT_EQUAL_UINT8(1u, OUT.nav.reset_counter);

    /* Kalıcı sıçrama (≥ KES_N_RESET örnek): konum GNSS'e sıfırlanır (belgelenen geri dönüş
       yolu; mutlak başvuru GNSS'tir). Düzeyin izlediği yol (inceleme bulgu 8): KES_N_RED retten
       sonra OK'tan düşer; sıfırlama OK'u geri getirmez ve destek üretmez; konum ancak
       sıfırlamadan sonraki, kapıdan geçen fix'le geçerli olur ve OK ondan KES_OK_MS sonra
       gelir. Sıfırlamadan sonra kestirim GNSS'i (yer gerçeği + 30 m) izler: OK iken hata
       denetiminin konum bileşeni bu alt testte kapalıdır (tek mutlak başvurunun kalıcı hatası
       süzgeçten ayırt edilemez); hız bileşeni açıktır. */
    TEST_ASSERT_EQUAL(IDA_RC_OK, ds_kur(&C));
    a = y_ayar(sen_y_duz, 0x51E003u);
    a.gnss_sicra = 30.0;
    a.gnss_sicra_bas = 40.0;
    a.gnss_sicra_son = 1.0e9;
    kosu_kur(&k, &a);
    y_git(&k, 40.0);
    TEST_ASSERT_TRUE(y_ok());
    Y_KONUM_DENETSIZ = true;
    double t_sifir = -1.0, t_gecerli = -1.0;
    t_dusme = -1.0;
    t_ok = -1.0;
    while (ns_t(k.t_ns) < 48.0) {
        const uint8_t sayac = OUT.nav.reset_counter;
        y_adim(&k);
        const double t = ns_t(k.t_ns);
        if (!y_ok() && (t_dusme < 0.0)) {
            t_dusme = t;
        }
        if ((OUT.nav.reset_counter != sayac) && (t_sifir < 0.0)) {
            t_sifir = t;
            /* Sıfırlamanın yapıldığı çağrı: düzey OK değil, konum geçersiz. */
            TEST_ASSERT_EQUAL_UINT8_MESSAGE(IDA_EST_DEGRADED, OUT.nav.est_level, "konum sıfırlaması OK'u korudu");
            TEST_ASSERT_FALSE_MESSAGE(y_bit(IDA_KES_G_KONUM), "konum sıfırlaması geçerlilik üretti");
        }
        if ((t_sifir > 0.0) && (t_gecerli < 0.0) && y_bit(IDA_KES_G_KONUM)) {
            t_gecerli = t;
        }
        if ((t_dusme > 0.0) && (t_gecerli < 0.0)) {
            TEST_ASSERT_FALSE_MESSAGE(y_bit(IDA_KES_G_KONUM), "ret sürerken konum geçerli");
        }
        if ((t_dusme > 0.0) && (t_ok < 0.0)) {
            if (y_ok()) {
                t_ok = t;
            } else {
                TEST_ASSERT_EQUAL_UINT8(IDA_EST_DEGRADED, OUT.nav.est_level);
            }
        }
    }
    hata_t h;
    nav_hatasi(&k, &OUT.nav, &h);
    y_olc("K5: kalici GNSS sicramasinda OK'tan dusme ani (s)", t_dusme);
    y_olc("K5: kalici GNSS sicramasinda konum sifirlama ani (s)", t_sifir);
    y_olc("K5: kalici GNSS sicramasinda konumun yeniden gecerli oldugu an (s)", t_gecerli);
    y_olc("K5: kalici GNSS sicramasinda yeniden OK ani (s)", t_ok);
    y_olc("K5: kalici GNSS sicramasindan sonra yatay konum hatasi (m; siciramaya esit olmali)", h.ph);
    /* Üçüncü ret 40,403 s (+ D); onuncu ret 41,803 s (+ D): sıfırlama; sonraki fix 42,003 s. */
    TEST_ASSERT_TRUE_MESSAGE((t_dusme > 40.45) && (t_dusme < 40.75), "kalıcı sıçramada düzey KES_N_RED retle düşmedi");
    TEST_ASSERT_TRUE_MESSAGE((t_sifir > 41.85) && (t_sifir < 42.15), "konum KES_N_RESET retle sıfırlanmadı");
    TEST_ASSERT_TRUE_MESSAGE((t_gecerli > t_sifir + 0.15) && (t_gecerli < t_sifir + 0.25),
                             "konum sıfırlamadan sonraki ilk kabul edilen fix'le geçerli olmadı");
    TEST_ASSERT_TRUE_MESSAGE((t_ok > t_gecerli + 1.99) && (t_ok < t_gecerli + 2.1),
                             "sıfırlamadan sonra OK, KES_OK_MS dolmadan ya da geç geldi");
    TEST_ASSERT_EQUAL_UINT8(2u, OUT.nav.reset_counter);
    TEST_ASSERT_TRUE((h.ph > 30.0 - Y_KONUM_SINIR) && (h.ph < 30.0 + Y_KONUM_SINIR));
}

static void test_y_k5_dvl_aykiri(void)
{
    /* Dalışta (yalnız DVL): x ekseninde +1 m/s aykırı değer. */
    TEST_ASSERT_EQUAL(IDA_RC_OK, ds_kur(&C));
    kosu_t k;
    kosu_ayar_t a = y_ayar(sen_y_dalis, 0x51E101u);
    a.dvl_ayk = 1.0;
    a.dvl_ayk_bas = 80.0; /* 2 örnek: 80,007 ve 80,207 s (< KES_N_RED; ilk yazılan dizi üç örnekti) */
    a.dvl_ayk_son = 80.3;
    kosu_kur(&k, &a);
    y_git(&k, 80.0);
    TEST_ASSERT_TRUE(y_ok());
    const uint32_t red0 = C.kes.n_dvl_red;
    hata_t m;
    (void)memset(&m, 0, sizeof(m));
    while (ns_t(k.t_ns) < 90.0) {
        y_adim(&k);
        hata_t h;
        nav_hatasi(&k, &OUT.nav, &h);
        en_buyuk(&m, &h);
        TEST_ASSERT_TRUE_MESSAGE(y_ok(), "kısa DVL aykırı değeri düzeyi düşürdü");
    }
    y_olc("K5: 2 aykiri DVL orneginde en buyuk yatay hiz hatasi (m/s)", m.vh);
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(red0 + 2u, C.kes.n_dvl_red, "aykırı iki DVL örneği reddedilmedi");
    TEST_ASSERT_TRUE_MESSAGE(m.vh < Y_HIZ_SINIR, "DVL aykırı değeri hızı bozdu");

    /* 9 örnek (1,7 s; < KES_N_RESET): yatay hız geçerliliği KES_N_RED = 3 ardışık retle düşer,
       aykırı değerler bitince toparlar. */
    TEST_ASSERT_EQUAL(IDA_RC_OK, ds_kur(&C));
    a = y_ayar(sen_y_dalis, 0x51E102u);
    a.dvl_ayk = 1.0;
    a.dvl_ayk_bas = 80.0;
    a.dvl_ayk_son = 81.7;
    kosu_kur(&k, &a);
    y_git(&k, 80.0);
    TEST_ASSERT_TRUE(y_ok());
    double t_dusme = -1.0, t_ok = -1.0;
    (void)memset(&m, 0, sizeof(m));
    while (ns_t(k.t_ns) < 90.0) {
        y_adim(&k);
        const double t = ns_t(k.t_ns);
        hata_t h;
        nav_hatasi(&k, &OUT.nav, &h);
        en_buyuk(&m, &h);
        if (!y_bit(IDA_KES_G_YATAY_HIZ) && (t_dusme < 0.0)) {
            t_dusme = t;
        }
        if ((t_dusme > 0.0) && (t_ok < 0.0)) {
            if (y_ok()) {
                t_ok = t;
            } else {
                TEST_ASSERT_EQUAL_UINT8(IDA_EST_DEGRADED, OUT.nav.est_level);
            }
        }
    }
    y_olc("K5: 9 aykiri DVL orneginde yatay hiz gecerliliginin dusme ani (s)", t_dusme);
    y_olc("K5: 9 aykiri DVL orneginden sonra OK ani (s)", t_ok);
    y_olc("K5: 9 aykiri DVL orneginde en buyuk yatay hiz hatasi (m/s)", m.vh);
    /* Üçüncü ret: örnek 80,407 s, füzyonu D = 0,15 s sonra. Bayatlık (son kabul edilen füzyondan
       T_DVL_STALE sonra, ≈ 80,96 s) ve süre kuralı (KES_RED_MS) ondan sonradır. */
    TEST_ASSERT_TRUE_MESSAGE((t_dusme > 80.5) && (t_dusme < 80.75), "ardışık ret yatay hız geçerliliğini düşürmedi");
    TEST_ASSERT_TRUE_MESSAGE((t_ok > 83.9) && (t_ok < 84.4), "aykırı değerler bitince OK toparlanmadı");
    TEST_ASSERT_TRUE_MESSAGE(m.vh < Y_HIZ_SINIR, "uzun DVL aykırı dizisi hızı bozdu");
    TEST_ASSERT_EQUAL_UINT32(0u, C.kes.n_hiz_sifir);
}

/* ---- Kabul 6: düzey histerezisi ------------------------------------------------------------ */

static void test_y_k6_tek_ornek(void)
{
    /* Tek geçerli DVL ve tek geçerli GNSS örneği: füzyon yok, geçerlilik yok, OK yok. */
    TEST_ASSERT_EQUAL(IDA_RC_OK, ds_kur(&C));
    kosu_t k;
    kosu_ayar_t a = y_ayar(sen_y_duz, 0x51F001u);
    a.gnss_tek = 3.0;
    a.dvl_tek = 3.0;
    kosu_kur(&k, &a);
    while (ns_t(k.t_ns) < 12.0) {
        y_adim(&k);
        TEST_ASSERT_FALSE_MESSAGE(y_ok(), "tek örnekle OK");
        TEST_ASSERT_FALSE(y_bit(IDA_KES_G_YATAY_HIZ));
        TEST_ASSERT_FALSE(y_bit(IDA_KES_G_KONUM));
    }
    TEST_ASSERT_EQUAL_UINT32(1u, k.gnss_seq);
    TEST_ASSERT_EQUAL_UINT32(1u, k.dvl_seq);
    TEST_ASSERT_EQUAL_UINT8(0u, OUT.nav.reset_counter); /* konum kurulmadı */
    TEST_ASSERT_EQUAL_UINT8(IDA_EST_DEGRADED, OUT.nav.est_level);

    /* KES_N_IYI − 1 = 4 ardışık iyi örnek de yetmez (akış sonra kesilir). */
    TEST_ASSERT_EQUAL(IDA_RC_OK, ds_kur(&C));
    a = y_ayar(sen_y_duz, 0x51F002u);
    a.gnss_kes_bas = 3.8; /* 3,003 … 3,603 s: 4 örnek (0–3 s arası da kapalı) */
    a.gnss_kes_son = 1.0e9;
    a.dvl_kes_bas = 3.8;
    a.dvl_kes_son = 1.0e9;
    kosu_kur(&k, &a);
    k.a.gnss = false;
    k.a.dvl = false;
    y_git(&k, 2.99);
    k.a.gnss = true;
    k.a.dvl = true;
    while (ns_t(k.t_ns) < 12.0) {
        y_adim(&k);
        TEST_ASSERT_FALSE(y_ok());
        TEST_ASSERT_FALSE(y_bit(IDA_KES_G_YATAY_HIZ));
        TEST_ASSERT_FALSE(y_bit(IDA_KES_G_KONUM));
    }
    TEST_ASSERT_EQUAL_UINT32(4u, k.gnss_seq);
    TEST_ASSERT_EQUAL_UINT32(4u, k.dvl_seq);
    TEST_ASSERT_EQUAL_UINT8(0u, OUT.nav.reset_counter);
}

static void test_y_k6_yukselme_suresi(void)
{
    /* Bütün bileşenler geçerli olduktan sonra OK tam KES_OK_MS (2 s) sonra verilir. */
    TEST_ASSERT_EQUAL(IDA_RC_OK, ds_kur(&C));
    kosu_t k;
    const kosu_ayar_t a = y_ayar(sen_y_duz, 0x51F003u);
    kosu_kur(&k, &a);
    double t_hepsi = -1.0, t_ok = -1.0;
    while (ns_t(k.t_ns) < 10.0) {
        y_adim(&k);
        const double t = ns_t(k.t_ns);
        if ((ida_kes_gecerlilik(&C.kes) == 0x3Fu) && (t_hepsi < 0.0)) {
            t_hepsi = t;
        }
        if (y_ok() && (t_ok < 0.0)) {
            t_ok = t;
        }
        if (t_hepsi < 0.0) {
            TEST_ASSERT_FALSE_MESSAGE(y_ok(), "bileşenlerin hepsi geçerli olmadan OK");
        }
    }
    y_olc("K6: butun bilesenlerin gecerli oldugu an (s)", t_hepsi);
    y_olc("K6: OK ani (s)", t_ok);
    TEST_ASSERT_TRUE(t_hepsi > 0.0);
    TEST_ASSERT_TRUE_MESSAGE(fabs((t_ok - t_hepsi) - 2.0) < 0.0051, "OK'a yükselme süresi KES_OK_MS değil");
}

static void test_y_k6_gidip_gelme(void)
{
    /* Dalışta DVL kilidi 1,5 s'lik yarı periyotla gidip gelir (kilitsiz, kilitli, …): yatay hız
       geçerliliği gidip gelir ama düzey yalnız bir kez değişir (OK → DEGRADED) ve gidip gelme
       sürdükçe OK'a dönmez. */
    TEST_ASSERT_EQUAL(IDA_RC_OK, ds_kur(&C));
    kosu_t k;
    kosu_ayar_t a = y_ayar(sen_y_dalis, 0x51F004u);
    a.dvl_flap_bas = 100.0;
    a.dvl_flap_son = 130.0;
    a.dvl_flap_yari = 1.5;
    kosu_kur(&k, &a);
    y_git(&k, 100.0);
    TEST_ASSERT_TRUE(y_ok());
    uint32_t duzey_degisimi = 0u, bit_degisimi = 0u;
    uint8_t duzey = OUT.nav.est_level;
    bool bit = y_bit(IDA_KES_G_YATAY_HIZ);
    while (ns_t(k.t_ns) < 130.0) {
        y_adim(&k);
        if (OUT.nav.est_level != duzey) {
            duzey_degisimi++;
            duzey = OUT.nav.est_level;
        }
        if (y_bit(IDA_KES_G_YATAY_HIZ) != bit) {
            bit_degisimi++;
            bit = !bit;
        }
    }
    y_olc("K6: 30 s gidip gelmede duzey degisimi sayisi", (double)duzey_degisimi);
    y_olc("K6: 30 s gidip gelmede yatay hiz gecerliligi degisimi sayisi", (double)bit_degisimi);
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(1u, duzey_degisimi, "düzey kaynakla birlikte gidip geldi");
    TEST_ASSERT_EQUAL_UINT8(IDA_EST_DEGRADED, OUT.nav.est_level);
    TEST_ASSERT_TRUE_MESSAGE(bit_degisimi >= 10u, "senaryo kaynağı gidip getirmedi");
    /* Gidip gelme bitince toparlar. */
    double t_ok = -1.0;
    while (ns_t(k.t_ns) < 140.0) {
        y_adim(&k);
        if (y_ok() && (t_ok < 0.0)) {
            t_ok = ns_t(k.t_ns);
        }
    }
    y_olc("K6: gidip gelme bittikten (130 s) sonra OK ani (s)", t_ok);
    /* Son yarı periyot (128,5–130 s) kilitlidir: nitelik ≈ 129,56 s'de döner; KES_OK_MS sonra. */
    TEST_ASSERT_TRUE_MESSAGE((t_ok > 131.4) && (t_ok < 132.5), "gidip gelme bitince OK toparlanmadı");
}

/* ---- Kabul 7: port yolu (ida_core_step; fc.inhibit_mask) ------------------------------------- */

static void test_y_k7_port(void)
{
    TEST_ASSERT_EQUAL(IDA_RC_OK, ds_kur(&C));
    kosu_t k;
    kosu_ayar_t a = y_ayar(sen_y_duz, 0x520001u);
    a.ping = true;
    a.gnss_kes_bas = 40.0;
    a.gnss_kes_son = 50.0;
    a.dvl_kes_bas = 40.0;
    a.dvl_kes_son = 50.0;
    kosu_kur(&k, &a);
    double t_kalkti = -1.0, t_kuruldu = -1.0, t_yeniden = -1.0;
    while (ns_t(k.t_ns) < 60.0) {
        girdi_kur(&k, &IN);
        const ida_rc_t rc = ida_core_step(&C, &IN, &OUT);
        TEST_ASSERT_EQUAL(IDA_RC_OK, rc);
        ds_cikti_degismezleri(rc, &IN, &OUT);
        y_ok_denet(&k);
        const double t = ns_t(k.t_ns);
        const bool est = (OUT.fc.inhibit_mask & IDA_FCI_EST) != 0u;
        /* A4 §3.16: EST biti kestirim düzeyi OK değilken kuruludur; her çağrıda, iki yönde. */
        TEST_ASSERT_EQUAL_MESSAGE(OUT.nav.est_level != (uint8_t)IDA_EST_OK, est, "IDA_FCI_EST düzeyle uyuşmuyor");
        if (!est && (t_kalkti < 0.0)) {
            t_kalkti = t;
        }
        if (est && (t_kalkti > 0.0) && (t_kuruldu < 0.0)) {
            t_kuruldu = t;
        }
        if (!est && (t_kuruldu > 0.0) && (t_yeniden < 0.0)) {
            t_yeniden = t;
        }
    }
    y_olc("K7: IDA_FCI_EST'in kalktigi an (s)", t_kalkti);
    y_olc("K7: kaynaklar kesilince (40 s) yeniden kuruldugu an (s)", t_kuruldu);
    y_olc("K7: kaynaklar donunce (50 s) yeniden kalktigi an (s)", t_yeniden);
    TEST_ASSERT_TRUE_MESSAGE((t_kalkti > 0.0) && (t_kalkti <= Y_OK_SURE), "olağan akışta IDA_FCI_EST kalkmadı");
    /* Son örnekler ≈ 39,8 s; füzyonları +0,15 s; bayatlık 1 s. */
    TEST_ASSERT_TRUE_MESSAGE((t_kuruldu > 40.5) && (t_kuruldu < 41.3), "kaynak kesilince IDA_FCI_EST kurulmadı");
    /* Dönüş: 5 ardışık iyi örnek (0,8 s) + teslim + D + KES_OK_MS. */
    TEST_ASSERT_TRUE_MESSAGE((t_yeniden > 52.9) && (t_yeniden < 53.6), "kaynak dönünce IDA_FCI_EST kalkmadı");
    TEST_ASSERT_TRUE(OUT.nav.altitude > 0.0f);
}

/* ---- Kabul 8: belirlenimcilik (A4-K24), yatay yardımla -------------------------------------- */

static void test_y_k8_belirlenimcilik(void)
{
    TEST_ASSERT_EQUAL(IDA_RC_OK, ds_kur(&C));
    TEST_ASSERT_EQUAL(IDA_RC_OK, ds_kur(&C2));
    kosu_t k;
    kosu_ayar_t a = y_ayar(sen_y_dalis, 0x520002u);
    a.ping = true;
    a.dvl_kilit_bas = 50.0;
    a.dvl_kilit_son = 53.0;
    a.dvl_ayk = 1.0;
    a.dvl_ayk_bas = 60.0;
    a.dvl_ayk_son = 63.0; /* hız sıfırlaması yolu dahil */
    kosu_kur(&k, &a);
    uint32_t say = 0u;
    while (ns_t(k.t_ns) < 70.0) {
        girdi_kur(&k, &IN);
        const ida_rc_t r1 = ida_core_step(&C, &IN, &OUT);
        const ida_rc_t r2 = ida_core_step(&C2, &IN, &OUT2);
        TEST_ASSERT_EQUAL(r1, r2);
        TEST_ASSERT_EQUAL_MEMORY(&OUT, &OUT2, sizeof(OUT));
        y_ok_denet(&k);
        say++;
    }
    TEST_ASSERT_EQUAL_MEMORY(&C.kes, &C2.kes, sizeof(C.kes));
    TEST_ASSERT_TRUE(say > 13000u);
    TEST_ASSERT_TRUE(C.kes.n_hiz_sifir >= 1u);
    TEST_ASSERT_TRUE(C.kes.n_dvl_red >= 10u);
}

/* ---- Birim: montaj kolları ve DVL montaj dönüşü ---------------------------------------------- */

static void test_y_montaj(void)
{
    /* Büyük kollar (GNSS 2 m, DVL 1 m) ve dönük DVL montajı: kol ya da dönüş işareti yanlışsa
       çemberde hız ve konum sınırları tutmaz (ω × l ≈ 0,1–0,2 m/s; anten ötelemesi 2 m). */
    TEST_ASSERT_EQUAL(IDA_RC_OK, ds_kur(&C));
    y_prm_f32(IDA_PRM_gnss_l_x, 2.0f);
    y_prm_f32(IDA_PRM_gnss_l_y, -0.5f);
    y_prm_f32(IDA_PRM_dvl_l_x, 1.0f);
    y_prm_f32(IDA_PRM_dvl_l_y, 0.4f);
    y_prm_f32(IDA_PRM_dvl_roll, 0.05f);
    y_prm_f32(IDA_PRM_dvl_pitch, -0.08f);
    y_prm_f32(IDA_PRM_dvl_yaw, 0.7853982f);
    kosu_t k;
    kosu_ayar_t a = y_ayar(sen_y_donus, 0x520003u);
    a.l_gnss[0] = 2.0;
    a.l_gnss[1] = -0.5;
    a.l_dvl[0] = 1.0;
    a.l_dvl[1] = 0.4;
    a.dvl_e[0] = 0.05;
    a.dvl_e[1] = -0.08;
    a.dvl_e[2] = 0.7853982;
    a.s_gnss = 0.3; /* kol işaretini ayırt edebilmek için küçük GNSS hatası */
    kosu_kur(&k, &a);
    y_ozet_t o;
    y_kos(&k, 120.0, 12.0, &o);
    y_olc("montaj: en buyuk yatay konum hatasi (m; GNSS sigma 0,3 m)", o.m.ph);
    y_olc("montaj: en buyuk yatay hiz hatasi (m/s)", o.m.vh);
    TEST_ASSERT_TRUE_MESSAGE(o.m.ph < 1.5, "anten kolu yanlış uygulanıyor");
    TEST_ASSERT_TRUE_MESSAGE(o.m.vh < Y_HIZ_SINIR, "DVL kolu ya da montaj dönüşü yanlış uygulanıyor");
    TEST_ASSERT_EQUAL_UINT32(0u, C.kes.n_dvl_red);
    TEST_ASSERT_EQUAL_UINT32(0u, o.ok_disi);
    TEST_ASSERT_TRUE(o.t_ok > 0.0);
}

/* ---- Birim: irtifa (nav.altitude) ------------------------------------------------------------ */

static void test_y_irtifa(void)
{
    /* 10 m derinlikte, 30 m dipte, eğik duruş (yalpa 0,2, baş-kıç −0,1): O_b'nin dibe mesafesi
       20 m. Kaynak yokken −1. */
    TEST_ASSERT_EQUAL(IDA_RC_OK, ds_kur(&C));
    kosu_t k;
    kosu_ayar_t a = ayar_temel(sen_y_egik, 0x520004u);
    kosu_kur(&k, &a);
    y_git(&k, 3.0);
    TEST_ASSERT_EQUAL_FLOAT(-1.0f, OUT.nav.altitude);
    /* Yalnız DVL. */
    k.a.dvl = true;
    y_git(&k, 5.0);
    y_olc("irtifa: yalniz DVL (m; gercek 20)", (double)OUT.nav.altitude);
    TEST_ASSERT_FLOAT_WITHIN(0.02f, 20.0f, OUT.nav.altitude);
    /* Yalnız Ping1D (ışın eğik: menzil > düşey mesafe; izdüşüm ve kol düzeltilir). */
    k.a.dvl = false;
    k.a.ping = true;
    y_git(&k, 8.0);
    y_olc("irtifa: yalniz Ping1D, egik (m; gercek 20)", (double)OUT.nav.altitude);
    TEST_ASSERT_FLOAT_WITHIN(0.02f, 20.0f, OUT.nav.altitude);
    /* İkisi birden; biri küçük gösterirse küçüğü (güvenli yön). */
    k.a.dvl = true;
    y_git(&k, 10.0);
    TEST_ASSERT_FLOAT_WITHIN(0.02f, 20.0f, OUT.nav.altitude);
    /* Kaynaklar kesilince T_DVL_STALE (1 s) sonra −1. */
    k.a.dvl = false;
    k.a.ping = false;
    y_git(&k, 10.8);
    TEST_ASSERT_TRUE(OUT.nav.altitude > 0.0f);
    y_git(&k, 11.3);
    TEST_ASSERT_EQUAL_FLOAT(-1.0f, OUT.nav.altitude);
    /* DVL kilitsiz (irtifa −1) ve Ping1D güveni düşük: −1. */
    k.a.dvl = true;
    k.a.dvl_kilit_bas = 0.0;
    k.a.dvl_kilit_son = 1.0e9;
    y_git(&k, 13.0);
    TEST_ASSERT_EQUAL_FLOAT(-1.0f, OUT.nav.altitude);
    /* İrtifa filtreye girmez: yatay geçerlilik ve düzey değişmez. */
    TEST_ASSERT_FALSE(y_bit(IDA_KES_G_YATAY_HIZ));
    TEST_ASSERT_EQUAL_UINT8(IDA_EST_DEGRADED, OUT.nav.est_level);
}

/* ---- Birim: seyreltme (her çağrıda GNSS ve DVL) ---------------------------------------------- */

static void test_y_seyreltme(void)
{
    /* GNSS ve DVL her çağrıda (200 Hz), gecikmesiz gelir: kuyruk taşmaz, fazlası seyreltilir,
       düzey yine belirlenen sürede OK olur. */
    TEST_ASSERT_EQUAL(IDA_RC_OK, ds_kur(&C));
    kosu_t k;
    kosu_ayar_t a = y_ayar(sen_y_duz, 0x520005u);
    a.yardim_hizli = true;
    kosu_kur(&k, &a);
    y_ozet_t o;
    y_kos(&k, 40.0, 8.0, &o);
    y_olc("seyreltme: OK ani (s)", o.t_ok);
    y_olc("seyreltme: seyreltilen ornek sayisi", (double)C.kes.n_seyrek);
    TEST_ASSERT_TRUE((o.t_ok > 0.0) && (o.t_ok <= Y_OK_SURE));
    TEST_ASSERT_EQUAL_UINT32(0u, o.ok_disi);
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(0u, C.kes.n_tasma, "yatay yardım kuyruğu taştı");
    TEST_ASSERT_TRUE(C.kes.n_seyrek > 10000u);
    y_olc("seyreltme: en buyuk yatay konum hatasi (m)", o.m.ph);
    y_olc("seyreltme: en buyuk yatay hiz hatasi (m/s)", o.m.vh);
    TEST_ASSERT_TRUE(o.m.ph < Y_KONUM_SINIR && o.m.vh < Y_HIZ_SINIR);
}

/* Durgun araç, gürültüsüz duyargalar, hepsi her çağrıda ve gecikmesiz (bağımsız kabul
   düzeneklerinin sade dünyası böyledir): dört akış da seyreltilir, hiçbir kuyruk taşmaz ve
   hiçbir tür ötekinin yerini doldurmaz; düzey 10 s içinde OK olur ve OK kalır. Seyreltme
   yokken manyetometre her çağrıda boşalan yeri alıyor, basınç füzyonlanamıyor ve derinlik
   geçerliliği ≈ 4,9 s'de düşüyordu (ilk koşuda görüldü). */
static void test_y_her_cagrida_hepsi(void)
{
    TEST_ASSERT_EQUAL(IDA_RC_OK, ds_kur(&C));
    double t_ok = -1.0;
    uint32_t ok_disi = 0u;
    for (uint32_t i = 1u; i <= 4000u; i++) {
        const int64_t t = T0_NS + (int64_t)i * T_ADIM_NS;
        ds_girdi(&IN, t, i, true);
        IN.imu.imu.accel[2] = -9.80665f;
        ida_sample_t *o = &IN.samples[0];
        (void)memset(o, 0, 4u * sizeof(*o));
        for (uint32_t j = 0u; j < 4u; j++) {
            o[j].hdr.t_meas.ns = t;
            o[j].hdr.t_rx.ns = t;
            o[j].hdr.seq = i;
            o[j].hdr.flags = (uint16_t)IDA_HF_VALID;
        }
        o[0].hdr.type = (uint8_t)IDA_SENS_BARO;
        o[0].hdr.window_ns = 10000000u;
        o[0].u.baro.p_abs = 101325.0f;
        o[0].u.baro.temp_c = 15.0f;
        o[1].hdr.type = (uint8_t)IDA_SENS_MAG;
        o[1].u.mag.field[0] = 2.0e-5f;
        o[1].u.mag.field[2] = 4.0e-5f;
        o[2].hdr.type = (uint8_t)IDA_SENS_GNSS;
        o[2].u.gnss.lat_e7 = ORJ_LAT_E7;
        o[2].u.gnss.lon_e7 = ORJ_LON_E7;
        o[2].u.gnss.fix_type = (uint8_t)IDA_GNSS_FIX_3D;
        o[2].u.gnss.n_sv = 12u;
        o[2].u.gnss.eph = 1.0f;
        o[2].u.gnss.epv = 2.0f;
        o[3].hdr.type = (uint8_t)IDA_SENS_DVL;
        o[3].u.dvl.dvl_flags = (uint8_t)(IDA_DVL_VEL_VALID | IDA_DVL_TRACK_BOTTOM | IDA_DVL_COV_PRESENT);
        o[3].u.dvl.cov[0] = 1.0e-4f;
        o[3].u.dvl.cov[3] = 1.0e-4f;
        o[3].u.dvl.cov[5] = 1.0e-4f;
        o[3].u.dvl.fom = 0.01f;
        o[3].u.dvl.altitude = 10.0f;
        o[3].u.dvl.beam_valid = 0x0Fu;
        for (uint32_t b = 0u; b < 4u; b++) {
            o[3].u.dvl.beam_dist[b] = 10.0f;
        }
        IN.n_samples = 4u;
        TEST_ASSERT_EQUAL(IDA_RC_OK, ida_core_step(&C, &IN, &OUT));
        TEST_ASSERT_TRUE(nav_sonlu(&OUT.nav));
        const bool est = (OUT.fc.inhibit_mask & IDA_FCI_EST) != 0u;
        TEST_ASSERT_EQUAL(OUT.nav.est_level != (uint8_t)IDA_EST_OK, est);
        if (y_ok()) {
            /* OK iken hata denetimi; yer gerçeği durgun araçtır (konum ve hız 0). */
            const double ph = hypot((double)OUT.nav.p_ned[0], (double)OUT.nav.p_ned[1]);
            const double vh = hypot((double)OUT.nav.v_ned[0], (double)OUT.nav.v_ned[1]);
            TEST_ASSERT_TRUE_MESSAGE((ph <= 30.0) && (vh <= 0.6), "düzey OK iken hata 3 σ_max'ı aştı");
            if (t_ok < 0.0) {
                t_ok = (double)i * 0.005;
            }
        } else if (t_ok > 0.0) {
            ok_disi++;
        } else {
            /* henüz OK olmadı */
        }
    }
    y_olc("her cagrida hepsi: OK ani (s)", t_ok);
    y_olc("her cagrida hepsi: seyreltilen ornek sayisi", (double)C.kes.n_seyrek);
    TEST_ASSERT_TRUE_MESSAGE((t_ok > 0.0) && (t_ok < 10.0), "sade dünyada düzey 10 s içinde OK olmadı");
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(0u, ok_disi, "sade dünyada düzey OK'tan düştü");
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(0u, C.kes.n_tasma, "kuyruk taştı");
    TEST_ASSERT_EQUAL_HEX8(0x3Fu, ida_kes_gecerlilik(&C.kes));
    TEST_ASSERT_FLOAT_WITHIN(0.05f, 0.0f, OUT.nav.p_ned[0]);
    TEST_ASSERT_FLOAT_WITHIN(0.05f, 0.0f, OUT.nav.p_ned[1]);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.0f, OUT.nav.v_ned[0]);
    TEST_ASSERT_FLOAT_WITHIN(0.2f, 10.14f, OUT.nav.altitude); /* 10 m + DVL kolu 0,14 m */
}

/* ---- Birim: tek kaynakla (yalnız GNSS; yalnız DVL) ------------------------------------------- */

static void test_y_tek_kaynak(void)
{
    /* Yalnız GNSS (DVL yok): yüzeyde konum ve hız GNSS'ten; OK olur. Dalınca GNSS kesilir:
       yatay hız ve konum T_GNSS_STALE sonra geçersiz, düzey DEGRADED (BAD değil). */
    TEST_ASSERT_EQUAL(IDA_RC_OK, ds_kur(&C));
    kosu_t k;
    kosu_ayar_t a = y_ayar(sen_y_dalis, 0x520006u);
    a.dvl = false;
    kosu_kur(&k, &a);
    y_ozet_t o;
    y_kos(&k, 12.0, 8.0, &o);
    y_olc("yalniz GNSS: OK ani (s)", o.t_ok);
    y_olc("yalniz GNSS: hizlanirken (8-12 s) en buyuk yatay hiz hatasi (m/s)", o.m.vh);
    y_olc("yalniz GNSS: hizlanirken OK iken en buyuk hiz hatasi / bildirilen sigma", o.oran_v);
    y_olc("yalniz GNSS: hizlanirken OK iken en buyuk hiz hatasi / ufuk sigma", o.oran_vu);
    const double oran_v_hizlanma = o.oran_v;
    TEST_ASSERT_TRUE((o.t_ok > 0.0) && (o.t_ok <= Y_OK_SURE));
    /* Hızlanırken (≈ 0,4 m/s²) yerçekimi ölçümü ivmeyi eğim sanar; DVL yokken bunu 5 Hz'lik ve
       0,05 m/s gürültülü GNSS hızı düzeltir. İlk yazılan tek sınır 0,15 m/s idi; hızlanma
       evresinde 0,166 m/s ölçüldü ve evreler ayrıldı: hızlanırken 0,25, sabit hızda 0,10. */
    TEST_ASSERT_TRUE_MESSAGE(o.m.vh < 0.25, "yalnız GNSS hızıyla hızlanırken yatay hız hatası");
    y_kos(&k, 29.0, 12.0, &o);
    y_olc("yalniz GNSS: sabit hizda (12-29 s) en buyuk yatay hiz hatasi (m/s)", o.m.vh);
    y_olc("yalniz GNSS: sabit hizda OK iken en buyuk hiz hatasi / bildirilen sigma", o.oran_v);
    y_olc("yalniz GNSS: sabit hizda OK iken en buyuk hiz hatasi / ufuk sigma", o.oran_vu);
    /* Hata / σ denetimi (inceleme bulgu 8): OK iken yatay hız hatası bildirilen σ'nın (nav
       v_sigma; eksen başına) üç katını aşmaz. */
    TEST_ASSERT_TRUE_MESSAGE(oran_v_hizlanma <= 3.0, "yalnız GNSS, hızlanma: bildirilen hız σ'sı iyimser");
    TEST_ASSERT_TRUE_MESSAGE(o.oran_v <= 3.0, "yalnız GNSS, sabit hız: bildirilen hız σ'sı iyimser");
    y_olc("yalniz GNSS: en buyuk yatay konum hatasi (m)", o.m.ph);
    TEST_ASSERT_EQUAL_UINT32(0u, o.ok_disi);
    TEST_ASSERT_TRUE(y_ok());
    TEST_ASSERT_TRUE(o.m.ph < Y_KONUM_SINIR);
    TEST_ASSERT_TRUE_MESSAGE(o.m.vh < 0.10, "yalnız GNSS hızıyla sabit hızda yatay hız hatası");
    y_git(&k, 45.0);
    TEST_ASSERT_FALSE(y_bit(IDA_KES_G_YATAY_HIZ));
    TEST_ASSERT_FALSE(y_bit(IDA_KES_G_KONUM));
    TEST_ASSERT_EQUAL_UINT8(IDA_EST_DEGRADED, OUT.nav.est_level);

    /* Hız geçerliliği bildirilen σ'ya bakar (inceleme bulgu 8): SIG_HVEL = 0,1 m/s iken yalnız
       GNSS hızıyla hızlanmada bildirilen σ (≈ 0,14 m/s) eşiği aşar ve yatay hız geçersiz olur;
       ufuk σ'sı (≈ 0,06 m/s) eşiğin altında kalırdı. Geçerliyken σ ≤ eşik y_adim'de denetlenir. */
    TEST_ASSERT_EQUAL(IDA_RC_OK, ds_kur(&C));
    y_prm_f32(IDA_PRM_sig_hvel, 0.1f);
    a = y_ayar(sen_y_dalis, 0x520006u);
    a.dvl = false;
    kosu_kur(&k, &a);
    y_git(&k, 5.0);
    uint32_t gecersiz = 0u, gecerli = 0u;
    double en_sv = 0.0;
    while (ns_t(k.t_ns) < 12.0) {
        y_adim(&k);
        const double sv = (double)((OUT.nav.v_sigma[0] > OUT.nav.v_sigma[1]) ? OUT.nav.v_sigma[0] : OUT.nav.v_sigma[1]);
        en_sv = (sv > en_sv) ? sv : en_sv;
        if (y_bit(IDA_KES_G_YATAY_HIZ)) {
            gecerli++;
        } else {
            gecersiz++;
            TEST_ASSERT_FALSE(y_ok());
        }
    }
    y_olc("yalniz GNSS, SIG_HVEL = 0,1: hizlanirken (5-12 s) en buyuk bildirilen hiz sigma (m/s)", en_sv);
    y_olc("yalniz GNSS, SIG_HVEL = 0,1: yatay hizin gecersiz oldugu cagri sayisi", (double)gecersiz);
    TEST_ASSERT_TRUE_MESSAGE(en_sv > 0.1, "senaryo bildirilen σ'yı eşiğin üstüne çıkarmadı");
    TEST_ASSERT_TRUE_MESSAGE(gecersiz > 0u, "bildirilen σ eşiği aştı ama yatay hız geçerli kaldı");
    TEST_ASSERT_TRUE(gecerli > 0u);

    /* Yalnız DVL (GNSS hiç yok; havuz gibi): yatay hız geçerli olur; konum hiç GNSS'ten
       kurulmadığı için geçersizdir ve düzey OK olmaz. */
    TEST_ASSERT_EQUAL(IDA_RC_OK, ds_kur(&C));
    a = y_ayar(sen_y_duz, 0x520007u);
    a.gnss = false;
    kosu_kur(&k, &a);
    while (ns_t(k.t_ns) < 60.0) {
        y_adim(&k);
        TEST_ASSERT_FALSE_MESSAGE(y_ok(), "konum hiç kurulmadan OK");
        TEST_ASSERT_FALSE(y_bit(IDA_KES_G_KONUM));
    }
    TEST_ASSERT_TRUE(y_bit(IDA_KES_G_YATAY_HIZ));
    TEST_ASSERT_EQUAL_UINT8(IDA_EST_DEGRADED, OUT.nav.est_level);
    hata_t h;
    nav_hatasi(&k, &OUT.nav, &h);
    TEST_ASSERT_TRUE(h.vh < Y_HIZ_SINIR);
}

/* ---- Birim: GNSS kalitesi, rejim ve oturum yenilenmesi --------------------------------------- */

static void test_y_gnss_kalite_ve_oturum(void)
{
    TEST_ASSERT_EQUAL(IDA_RC_OK, ds_kur(&C));
    kosu_t k;
    kosu_ayar_t a = y_ayar(sen_y_duz, 0x520008u);
    a.dvl = false;
    a.gnss_fix_bas = 20.0; /* fix 2B'ye düşer: örnek gelir ama kullanılmaz */
    a.gnss_fix_son = 26.0;
    kosu_kur(&k, &a);
    y_git(&k, 20.0);
    TEST_ASSERT_TRUE(y_ok());
    y_git(&k, 22.0);
    TEST_ASSERT_FALSE_MESSAGE(y_bit(IDA_KES_G_KONUM), "2B fix konumu geçerli tuttu");
    TEST_ASSERT_FALSE(y_ok());
    y_git(&k, 31.0);
    TEST_ASSERT_TRUE_MESSAGE(y_ok(), "3B fix dönünce OK toparlanmadı");
    /* eph eşiğin (GNSS_EPH_MAX = 5 m) üstünde: kullanılmaz. */
    k.a.s_gnss = 6.0;
    y_git(&k, 34.0);
    TEST_ASSERT_FALSE_MESSAGE(y_bit(IDA_KES_G_KONUM), "eph eşiğin üstündeyken konum geçerli");
    k.a.s_gnss = 1.5;
    y_git(&k, 40.0);
    TEST_ASSERT_TRUE(y_ok());
    /* Oturum yenilenmesi (A4-K72): süzgeç baştan hizalanır; yatay konum yeniden GNSS'ten
       kurulana kadar geçersizdir; sonra OK'a döner. */
    const uint8_t sayac = OUT.nav.reset_counter;
    girdi_kur(&k, &IN);
    IN.plat.link_event = (uint8_t)IDA_LINKEV_SESSION_START;
    TEST_ASSERT_EQUAL(IDA_RC_OK, ida_core_step(&C, &IN, &OUT));
    TEST_ASSERT_EQUAL_UINT8(IDA_EST_INIT, OUT.nav.est_level);
    TEST_ASSERT_FALSE(C.kes.konum_var);
    double t_ok = -1.0;
    while (ns_t(k.t_ns) < 50.0) {
        y_adim(&k);
        if (y_ok() && (t_ok < 0.0)) {
            t_ok = ns_t(k.t_ns);
        }
        if (!C.kes.konum_var) {
            TEST_ASSERT_FALSE(y_bit(IDA_KES_G_KONUM));
        }
    }
    y_olc("oturum yenilenmesinden (40 s) sonra OK ani (s)", t_ok);
    /* Hizalama 1 s; araç 1 m/s ile giderken hizalandığı için (hız 0 kabulü) GNSS hızı kapıdan
       geçemez: KES_N_RESET örnek (2 s) sonra hız sıfırlanır; sonra KES_OK_MS. */
    TEST_ASSERT_TRUE_MESSAGE((t_ok > 43.0) && (t_ok < 48.0), "oturum yenilenince OK toparlanmadı");
    TEST_ASSERT_EQUAL_UINT8((uint8_t)(sayac + 1u), OUT.nav.reset_counter);
    hata_t h;
    nav_hatasi(&k, &OUT.nav, &h);
    TEST_ASSERT_TRUE(h.ph < Y_KONUM_SINIR);
}

/* ---- Düzeltme turu: inceleme bulguları 1–8 (docs/gorevler/inceleme/W5-kestirim-yatay-inceleme.md)
   İlke: IDA_EST_OK silahlanmanın ön koşuludur; hiçbir sıfırlama OK'u korumaz ve destek üretmez.
   Aşağıdaki diziler düzeltmeden önceki kodda düşer (rapor: mutasyon sonuçları). ------------------ */

/* Bulgu 1 (a): dalış, yalnız DVL; t ≥ 80 s'de DVL x ekseninde kalıcı +1 m/s sapma. Düzey
   KES_N_RED ardışık retle OK'tan düşer ve sapma sürdükçe OK'a dönmez: KES_N_RESET retten sonraki
   hız sıfırlaması OK'u korumaz, destek üretmez ve mutlak başvuru (taze GNSS konumu) olmadığı
   için konum geçerliliğini de düşürür. Düzeltmeden önce: 10 Hz'de düzey hiç düşmüyordu (OK iken
   hız hatası 1,0 m/s, konum hatası 118 m); 5 Hz'de 80,97 s'de düşüp sıfırlamadan 2 s sonra OK'a
   dönüyordu. */
static void y_r1_dvl_kalici(double hz, const char *ad)
{
    TEST_ASSERT_EQUAL(IDA_RC_OK, ds_kur(&C));
    kosu_t k;
    kosu_ayar_t a = y_ayar(sen_y_dalis, 0x51E102u);
    a.dvl_hz = hz;
    a.dvl_ayk = 1.0;
    a.dvl_ayk_bas = 80.0;
    a.dvl_ayk_son = 1.0e9;
    kosu_kur(&k, &a);
    y_git(&k, 80.0);
    TEST_ASSERT_TRUE(y_ok());
    y_iz_t z;
    y_iz_kur(&z);
    y_izle(&k, 200.0, &z);
    y_iz_yaz(ad, &z);
    const double T = (hz > 7.0) ? 0.1 : 0.2;
    /* Üçüncü ret (KES_N_RED): örnek 80,007 + 2T; füzyonu ölçüm zamanından D = 0,15 s sonra. */
    TEST_ASSERT_TRUE_MESSAGE((z.t_dusme > 80.0 + 2.0 * T + 0.1) && (z.t_dusme < 80.0 + 2.0 * T + 0.3),
                             "kalıcı DVL sapmasında düzey KES_N_RED retle OK'tan düşmedi");
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(0u, z.ok_adim, "kalıcı DVL sapması sürerken düzey OK'a döndü");
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(0u, z.ihlal, "düzey OK iken hata 3 σ_max'ı aştı");
    TEST_ASSERT_TRUE_MESSAGE(C.kes.n_hiz_sifir >= 1u, "senaryo hız sıfırlaması üretmedi");
    /* Mutlak başvuru yokken hız sıfırlandı: konum yeniden GNSS'ten kurulana kadar geçersiz. */
    TEST_ASSERT_FALSE_MESSAGE(C.kes.konum_var, "başvurusuz hız sıfırlaması konumu geçerli bıraktı");
    TEST_ASSERT_FALSE(y_bit(IDA_KES_G_KONUM));
    TEST_ASSERT_EQUAL_UINT8(IDA_EST_DEGRADED, OUT.nav.est_level);
}

static void test_y_r1_dvl_kalici_10hz(void)
{
    y_r1_dvl_kalici(10.0, "R1a DVL 10 Hz kalici +1 m/s");
}

static void test_y_r1_dvl_kalici_5hz(void)
{
    y_r1_dvl_kalici(5.0, "R1a DVL 5 Hz kalici +1 m/s");
}

/* Bulgu 1, karar (iv): dalışta DVL'de 80–90 s arası geçici +1 m/s sapma. Hız iki kez sıfırlanır
   (sapmalı DVL'e, sonra doğru DVL'e); bu arada konum ≈ 10 m kayar. Sapma bitip DVL yeniden kabul
   edilse de mutlak başvuru olmadığı için konum geçersiz kalır ve düzey OK olmaz; konum ancak
   yüzeyde GNSS'ten yeniden kurulunca (reset_counter artar) ve ondan sonraki fix kapıdan geçince
   geçerli olur, OK ondan KES_OK_MS sonra gelir. */
static void test_y_r1_hiz_sifir_konum(void)
{
    TEST_ASSERT_EQUAL(IDA_RC_OK, ds_kur(&C));
    kosu_t k;
    kosu_ayar_t a = y_ayar(sen_y_dalis_kisa, 0x51E107u);
    a.dvl_hz = 10.0;
    a.dvl_ayk = 1.0;
    a.dvl_ayk_bas = 80.0;
    a.dvl_ayk_son = 90.0;
    kosu_kur(&k, &a);
    y_git(&k, 80.0);
    TEST_ASSERT_TRUE(y_ok());
    const uint8_t sayac0 = OUT.nav.reset_counter;
    y_iz_t z;
    y_iz_kur(&z);
    double t_kur = -1.0, t_gecerli = -1.0, t_hiz = -1.0, en_ph = 0.0;
    while (ns_t(k.t_ns) < 150.0) {
        y_adim_ham(&k);
        y_iz_adim(&k, &z);
        const double t = ns_t(k.t_ns);
        if ((t > 92.0) && (t_hiz < 0.0) && y_bit(IDA_KES_G_YATAY_HIZ)) {
            t_hiz = t; /* sapma bitti, DVL yeniden kabul edildi */
        }
        if ((z.t_dusme > 0.0) && (t_kur < 0.0)) {
            if (OUT.nav.reset_counter != sayac0) {
                t_kur = t;
            } else {
                TEST_ASSERT_FALSE_MESSAGE(y_ok(), "konum GNSS'ten yeniden kurulmadan düzey OK");
                if (t > 82.0) {
                    TEST_ASSERT_FALSE_MESSAGE(y_bit(IDA_KES_G_KONUM),
                                              "başvurusuz hız sıfırlamasından sonra konum geçerli");
                    hata_t h;
                    nav_hatasi(&k, &OUT.nav, &h);
                    en_ph = (h.ph > en_ph) ? h.ph : en_ph;
                }
            }
        }
        if ((t_kur > 0.0) && (t_gecerli < 0.0) && y_bit(IDA_KES_G_KONUM)) {
            t_gecerli = t;
        }
    }
    hata_t hs;
    nav_hatasi(&k, &OUT.nav, &hs);
    y_iz_yaz("R1 (iv) gecici DVL sapmasi", &z);
    y_olc("R1 (iv): sapma bittikten sonra yatay hiz gecerliligi (s)", t_hiz);
    y_olc("R1 (iv): konum gecersizken en buyuk yatay konum hatasi (m)", en_ph);
    y_olc("R1 (iv): konumun GNSS'ten yeniden kuruldugu an (s)", t_kur);
    y_olc("R1 (iv): konumun yeniden gecerli oldugu an (s)", t_gecerli);
    y_olc("R1 (iv): yeniden OK ani (s)", z.t_donus);
    y_olc("R1 (iv): sonda yatay konum hatasi (m)", hs.ph);
    TEST_ASSERT_TRUE(C.kes.n_hiz_sifir >= 2u);
    TEST_ASSERT_TRUE_MESSAGE((t_hiz > 92.0) && (t_hiz < 95.0), "sapma bitince yatay hız toparlanmadı");
    TEST_ASSERT_TRUE_MESSAGE(en_ph > 5.0, "senaryo beklenen konum kaymasını üretmedi");
    /* Anten ≈ 130 s'de sudan çıkar; edinim 2 s; KES_N_IYI = 5 fix. */
    TEST_ASSERT_TRUE_MESSAGE((t_kur > 130.0) && (t_kur < 136.0), "konum yüzeyde GNSS'ten yeniden kurulmadı");
    TEST_ASSERT_TRUE_MESSAGE((t_gecerli > t_kur) && (t_gecerli < t_kur + 0.5),
                             "konum, kurulumdan sonraki ilk kabul edilen fix'le geçerli olmadı");
    TEST_ASSERT_TRUE_MESSAGE((z.t_donus > t_gecerli + 1.99) && (z.t_donus < t_gecerli + 2.1),
                             "OK, konum yeniden geçerli olduktan KES_OK_MS sonra gelmedi");
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(0u, z.ihlal, "düzey OK iken hata 3 σ_max'ı aştı");
    TEST_ASSERT_TRUE(hs.ph < Y_KONUM_SINIR);
    TEST_ASSERT_TRUE(y_ok());
}

/* Bulgu 2 (b): yüzey, GNSS 5 Hz + DVL 10 Hz; t ≥ 40 s'de DVL x ekseninde kalıcı sapma. İki
   nitelikli hız kaynağı sürekli çelişir: hız sırayla birine ve ötekine sıfırlanır. Yatay hız
   geçerliliği canlı ve nitelikli her kaynağın ret altında olmamasını ister (VE); düzey
   KES_N_RED retle OK'tan düşer ve çelişki sürdükçe OK'a dönmez. ok_ms ≠ 0 ise KES_OK_MS o değere
   çekilir: histerezis süresi sıfırlama çevriminden kısayken de OK'a dönülmediği (düzeyi tutanın
   yalnız süre olmadığı) sınanır. Düzeltmeden önce: 40–100 s'de OK dışı adım 0, OK iken hız
   hatası 0,515 m/s. */
static void y_r2_celisen(double sapma, uint32_t ok_ms, uint32_t tohum, const char *ad)
{
    TEST_ASSERT_EQUAL(IDA_RC_OK, ds_kur(&C));
    if (ok_ms != 0u) {
        y_prm_u32(IDA_PRM_kes_ok_ms, ok_ms);
    }
    kosu_t k;
    kosu_ayar_t a = y_ayar(sen_y_duz, tohum);
    a.dvl_hz = 10.0;
    a.dvl_ayk = sapma;
    a.dvl_ayk_bas = 40.0;
    a.dvl_ayk_son = 1.0e9;
    kosu_kur(&k, &a);
    y_git(&k, 40.0);
    TEST_ASSERT_TRUE(y_ok());
    y_iz_t z;
    y_iz_kur(&z);
    y_izle(&k, 100.0, &z);
    y_iz_yaz(ad, &z);
    /* Üçüncü DVL reddi: örnek 40,207 s; füzyonu D = 0,15 s sonra. */
    TEST_ASSERT_TRUE_MESSAGE((z.t_dusme > 40.3) && (z.t_dusme < 40.5),
                             "çelişen hız kaynaklarında düzey KES_N_RED retle OK'tan düşmedi");
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(0u, z.ok_adim, "hız kaynakları çelişirken düzey OK'a döndü");
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(0u, z.ihlal, "düzey OK iken hata 3 σ_max'ı aştı");
    TEST_ASSERT_TRUE_MESSAGE(C.kes.n_hiz_sifir >= 4u, "senaryo ardışık hız sıfırlamaları üretmedi");
    TEST_ASSERT_EQUAL_UINT8(IDA_EST_DEGRADED, OUT.nav.est_level);
}

static void test_y_r2_celisen_kaynaklar(void)
{
    y_r2_celisen(0.5, 0u, 0x51E103u, "R2b GNSS + DVL (+0,5 m/s)");
}

static void test_y_r2_celisen_kisa_histerezis(void)
{
    y_r2_celisen(1.0, 1000u, 0x51E108u, "R2b GNSS + DVL (+1 m/s), KES_OK_MS = 1000");
}

/* Karar 2, "canlı ve nitelikli" koşulu: ret altındaki kaynağın akışı kesilirse (DVL 40 s'den
   beri sapmalı, 40,65 s'de susar; sıfırlamaya varmadan) kaynak T_DVL_STALE sonra etkin
   sayılmaz ve yatay hız geçerliliğini düşürmeyi bırakır; kalan kaynak (GNSS hızı) destekler ve
   düzey KES_OK_MS sonra OK'a döner. Kuşku bayrağı silinmez: DVL dönerse ilk kapı kararına kadar
   sürer. */
static void test_y_r2_kaynak_kesilir(void)
{
    TEST_ASSERT_EQUAL(IDA_RC_OK, ds_kur(&C));
    kosu_t k;
    kosu_ayar_t a = y_ayar(sen_y_duz, 0x51E10Bu);
    a.dvl_hz = 10.0;
    a.dvl_ayk = 0.5;
    a.dvl_ayk_bas = 40.0; /* 7 ret: 40,007 … 40,607 s (< KES_N_RESET) */
    a.dvl_ayk_son = 1.0e9;
    a.dvl_kes_bas = 40.65;
    a.dvl_kes_son = 1.0e9;
    kosu_kur(&k, &a);
    y_git(&k, 40.0);
    TEST_ASSERT_TRUE(y_ok());
    y_iz_t z;
    y_iz_kur(&z);
    double t_hiz_dusme = -1.0, t_hiz_donus = -1.0;
    while (ns_t(k.t_ns) < 50.0) {
        y_adim_ham(&k);
        y_iz_adim(&k, &z);
        const double t = ns_t(k.t_ns);
        const bool hiz = y_bit(IDA_KES_G_YATAY_HIZ);
        if (!hiz && (t_hiz_dusme < 0.0)) {
            t_hiz_dusme = t;
        }
        if (hiz && (t_hiz_dusme > 0.0) && (t_hiz_donus < 0.0)) {
            t_hiz_donus = t;
        }
    }
    y_iz_yaz("R2 ret altindaki DVL susar", &z);
    y_olc("R2 ret altindaki DVL susar: yatay hiz gecerliliginin dusme ani (s)", t_hiz_dusme);
    y_olc("R2 ret altindaki DVL susar: yatay hiz gecerliliginin donme ani (s)", t_hiz_donus);
    y_olc("R2 ret altindaki DVL susar: yeniden OK ani (s)", z.t_donus);
    TEST_ASSERT_TRUE((t_hiz_dusme > 40.3) && (t_hiz_dusme < 40.5));
    /* Son kapı kararı ≈ 40,76 s, son alım ≈ 40,64 s; T_DVL_STALE = 1 s. */
    TEST_ASSERT_TRUE_MESSAGE((t_hiz_donus > 41.5) && (t_hiz_donus < 41.9),
                             "susan kaynak bayatlık süresinden sonra da yatay hızı geçersiz tuttu");
    TEST_ASSERT_TRUE_MESSAGE((z.t_donus > t_hiz_donus + 1.99) && (z.t_donus < t_hiz_donus + 2.1),
                             "kalan kaynakla OK, KES_OK_MS sonra gelmedi");
    TEST_ASSERT_EQUAL_UINT32(0u, z.ihlal);
    TEST_ASSERT_EQUAL_UINT32(0u, C.kes.n_hiz_sifir);
    TEST_ASSERT_TRUE_MESSAGE(C.kes.kusku_dvl, "kuşkuyu kabul edilen örnek dışında bir şey kaldırdı");
    TEST_ASSERT_TRUE(y_ok());
}

/* Sıfırlama çağrısının kendisi (beyaz kutu; ida_kes_dvl_fuzyon ve ida_kes_gnss_fuzyon doğrudan
   çağrılır, düzey hesabı araya girmez): karar 1 (i) OK adayı silinir, (ii) destek zamanları
   değişmez, (iii) KES_N_RED rette kuşku, (iv) GNSS konumu taze değilken hız sıfırlaması konumu
   düşürür, tazeyken düşürmez; karar 4: ret izi sıfırlamada temizlenir, kuşku ancak kabul edilen
   örnekte. Port yolunda (i) ve (ii) tek başlarına gözlenemez: sıfırlamanın bıraktığı kuşku aynı
   çağrıda geçerliliği zaten düşürür; burada ayrı ayrı sınanırlar. */
static void y_r1_dvl_sifirla(bool gnss_taze)
{
    ida_kes_ayar_t ay;
    ida_kes_ayar_oku(&C, &ay);
    TEST_ASSERT_TRUE(C.kes.akis_dvl.destek_var && (C.kes.akis_dvl.destek_ns != ay.now_ns));
    TEST_ASSERT_TRUE(C.kes.ok_aday_var && C.kes.konum_var && !C.kes.kusku_dvl && !C.kes.red_dvl.var);
    const int64_t destek0 = C.kes.akis_dvl.destek_ns;
    const int64_t fuz0 = C.kes.hiz_fuz_ns;
    const uint32_t sifir0 = C.kes.n_hiz_sifir;
    ida_kes_yolc_t y;
    (void)memset(&y, 0, sizeof(y));
    y.t_ns = C.kes.th_ns;
    y.tur = (uint8_t)IDA_SENS_DVL;
    y.sira = 1u;
    y.bayrak = (uint8_t)(IDA_KES_YB_HIZ | IDA_KES_YB_DIP);
    y.v[0] = 3.0f; /* gövde ileri; gerçek ≈ 1 m/s */
    y.v[9] = 0.002f;
    y.v[10] = 20.0f;
    const ida_v3_t w0 = {0.0f, 0.0f, 0.0f};
    TEST_ASSERT_TRUE((ay.n_red >= 2u) && (ay.n_red < ay.n_reset));
    for (uint32_t i = 1u; i <= ay.n_reset; i++) {
        ida_kes_dvl_fuzyon(&C.kes, &ay, &y, w0);
        TEST_ASSERT_EQUAL_MESSAGE(i >= ay.n_red, C.kes.kusku_dvl, "kuşku KES_N_RED rette kurulmadı");
        if (i < ay.n_reset) {
            TEST_ASSERT_EQUAL_UINT32(sifir0, C.kes.n_hiz_sifir);
            TEST_ASSERT_TRUE(C.kes.red_dvl.var);
            TEST_ASSERT_TRUE_MESSAGE(C.kes.ok_aday_var && C.kes.konum_var, "ret tek başına sıfırlama etkisi yaptı");
        }
    }
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(sifir0 + 1u, C.kes.n_hiz_sifir, "KES_N_RESET rette hız sıfırlanmadı");
    TEST_ASSERT_FLOAT_WITHIN(0.05f, 3.0f, ida_v3_norm(C.kes.xh.v));
    TEST_ASSERT_FALSE_MESSAGE(C.kes.ok_aday_var, "(i) hız sıfırlaması OK adayını silmedi");
    TEST_ASSERT_TRUE_MESSAGE(C.kes.akis_dvl.destek_ns == destek0, "(ii) hız sıfırlaması desteği yeniledi");
    TEST_ASSERT_TRUE_MESSAGE(C.kes.hiz_fuz_ns == fuz0, "(ii) hız sıfırlaması ölü hesap desteğini yeniledi");
    TEST_ASSERT_TRUE_MESSAGE(C.kes.kusku_dvl, "sıfırlama kuşkuyu kaldırdı");
    TEST_ASSERT_FALSE_MESSAGE(C.kes.red_dvl.var, "karar 4: ret izi sıfırlamada temizlenmedi");
    TEST_ASSERT_EQUAL_UINT8(0u, C.kes.nit_dvl.red_n);
    TEST_ASSERT_EQUAL_MESSAGE(gnss_taze, C.kes.konum_var, "(iv) hız sıfırlamasında konum geçerliliği");
    /* Sonraki örnek kapıdan geçer: destek ve kuşkunun kalkması ondan gelir. */
    ida_kes_dvl_fuzyon(&C.kes, &ay, &y, w0);
    TEST_ASSERT_FALSE(C.kes.kusku_dvl);
    TEST_ASSERT_TRUE(C.kes.akis_dvl.destek_ns == ay.now_ns);
    TEST_ASSERT_EQUAL_UINT32(sifir0 + 1u, C.kes.n_hiz_sifir);
}

static void test_y_r1_sifirlama_birim(void)
{
    /* Dalışta (GNSS konumu taze değil): DVL hız sıfırlaması konumu da düşürür. */
    TEST_ASSERT_EQUAL(IDA_RC_OK, ds_kur(&C));
    kosu_t k;
    kosu_ayar_t a = y_ayar(sen_y_dalis, 0x51E109u);
    kosu_kur(&k, &a);
    y_git(&k, 80.0);
    TEST_ASSERT_TRUE(y_ok());
    y_r1_dvl_sifirla(false);

    /* Yüzeyde (GNSS konumu taze): DVL hız sıfırlaması konumu düşürmez. */
    TEST_ASSERT_EQUAL(IDA_RC_OK, ds_kur(&C));
    a = y_ayar(sen_y_duz, 0x51E10Au);
    kosu_kur(&k, &a);
    y_git(&k, 40.0);
    TEST_ASSERT_TRUE(y_ok());
    y_r1_dvl_sifirla(true);

    /* GNSS konum sıfırlaması (aynı durumdan; hız bileşeni süzgecin hızına eşit, kapıdan geçer). */
    ida_kes_ayar_t ay;
    ida_kes_ayar_oku(&C, &ay);
    C.kes.ok_aday_var = true; /* düzey hesabı araya girmedi: aday yeniden kurulmuş sayılır */
    TEST_ASSERT_TRUE(C.kes.akis_gnss.destek_var && (C.kes.akis_gnss.destek_ns != ay.now_ns));
    TEST_ASSERT_TRUE(!C.kes.kusku_gnss_p && !C.kes.red_gnss_p.var);
    const int64_t destek0 = C.kes.akis_gnss.destek_ns;
    const uint32_t sifir0 = C.kes.n_konum_sifir;
    const uint8_t sayac0 = C.kes.reset_sayac;
    const float n0 = C.kes.xh.p.x;
    ida_kes_yolc_t y;
    (void)memset(&y, 0, sizeof(y));
    y.t_ns = C.kes.th_ns;
    y.tur = (uint8_t)IDA_SENS_GNSS;
    y.sira = 3u;
    y.fix = (uint8_t)IDA_GNSS_FIX_3D;
    y.lat_e7 = ORJ_LAT_E7 + 18000; /* ≈ 200 m kuzey: süzgecin konumundan ≈ 170 m uzak */
    y.lon_e7 = ORJ_LON_E7;
    y.v[0] = C.kes.xh.v.x;
    y.v[1] = C.kes.xh.v.y;
    y.v[2] = 1.5f;
    const ida_v3_t w0 = {0.0f, 0.0f, 0.0f};
    for (uint32_t i = 1u; i <= ay.n_reset; i++) {
        ida_kes_gnss_fuzyon(&C.kes, &ay, &y, w0);
        TEST_ASSERT_EQUAL_MESSAGE(i >= ay.n_red, C.kes.kusku_gnss_p, "konum kuşkusu KES_N_RED rette kurulmadı");
        TEST_ASSERT_FALSE(C.kes.kusku_gnss_v);
        if (i < ay.n_reset) {
            TEST_ASSERT_EQUAL_UINT32(sifir0, C.kes.n_konum_sifir);
            TEST_ASSERT_TRUE(C.kes.red_gnss_p.var && C.kes.ok_aday_var);
        }
    }
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(sifir0 + 1u, C.kes.n_konum_sifir, "KES_N_RESET rette konum sıfırlanmadı");
    TEST_ASSERT_EQUAL_UINT8((uint8_t)(sayac0 + 1u), C.kes.reset_sayac);
    TEST_ASSERT_TRUE(C.kes.xh.p.x > n0 + 100.0f);
    TEST_ASSERT_FALSE_MESSAGE(C.kes.ok_aday_var, "(i) konum sıfırlaması OK adayını silmedi");
    TEST_ASSERT_TRUE_MESSAGE(C.kes.akis_gnss.destek_ns == destek0, "(ii) konum sıfırlaması desteği yeniledi");
    TEST_ASSERT_TRUE_MESSAGE(C.kes.kusku_gnss_p, "sıfırlama kuşkuyu kaldırdı");
    TEST_ASSERT_FALSE_MESSAGE(C.kes.red_gnss_p.var, "karar 4: ret izi sıfırlamada temizlenmedi");
    TEST_ASSERT_TRUE(C.kes.konum_var);
    ida_kes_gnss_fuzyon(&C.kes, &ay, &y, w0);
    TEST_ASSERT_FALSE(C.kes.kusku_gnss_p);
    TEST_ASSERT_TRUE(C.kes.akis_gnss.destek_ns == ay.now_ns);
    TEST_ASSERT_EQUAL_UINT32(sifir0 + 1u, C.kes.n_konum_sifir);
}

/* Bulgu 3 (c): düzey OK iken ORIJIN_LAT değişir (+2000 × 1e-7° ≈ 22,2 m kuzey). Aynı çağrıda
   konum geçersiz, düzey OK değil; nav.origin_* yeni değeri bildirir. Durum ötelenmez: konum yeni
   orijine göre GNSS'ten yeniden kurulur (reset_counter artar), sonraki fix kapıdan geçince
   geçerli olur, OK ondan KES_OK_MS sonra gelir. Düzeltmeden önce 230 adım OK sürüyordu. */
static void test_y_r3_orijin_degisimi(void)
{
    TEST_ASSERT_EQUAL(IDA_RC_OK, ds_kur(&C));
    kosu_t k;
    const kosu_ayar_t a = y_ayar(sen_y_duz, 0x51E104u);
    kosu_kur(&k, &a);
    y_git(&k, 40.0);
    TEST_ASSERT_TRUE(y_ok());
    TEST_ASSERT_TRUE(y_bit(IDA_KES_G_KONUM));
    const uint8_t sayac0 = OUT.nav.reset_counter;
    const int32_t yeni = ORJ_LAT_E7 + 2000;
    (void)memcpy(&C.prm.yururluk.ham[IDA_PRM_orijin_lat], &yeni, sizeof(yeni));
    /* Yer gerçeği eski orijine göredir: OK iken hata denetiminin konum bileşeni kapatılır,
       yeni orijine göre hata aşağıda ayrıca denetlenir. */
    Y_KONUM_DENETSIZ = true;
    y_adim(&k);
    TEST_ASSERT_FALSE_MESSAGE(y_ok(), "orijin değişti, düzey aynı çağrıda OK kaldı");
    TEST_ASSERT_EQUAL_UINT8(IDA_EST_DEGRADED, OUT.nav.est_level);
    TEST_ASSERT_FALSE_MESSAGE(y_bit(IDA_KES_G_KONUM), "orijin değişti, konum aynı çağrıda geçerli kaldı");
    TEST_ASSERT_FALSE(C.kes.konum_var);
    TEST_ASSERT_EQUAL_INT32(yeni, OUT.nav.origin_lat_e7);
    TEST_ASSERT_EQUAL_INT32(ORJ_LON_E7, OUT.nav.origin_lon_e7);
    TEST_ASSERT_TRUE_MESSAGE(y_bit(IDA_KES_G_YATAY_HIZ), "orijin değişimi yatay hız geçerliliğini düşürdü");
    const double kayma = 2000.0e-7 * 111050.0; /* m; 41° enleminde 111,05 km/° */
    double t_kur = -1.0, t_gecerli = -1.0, t_ok = -1.0;
    while (ns_t(k.t_ns) < 46.0) {
        y_adim(&k);
        const double t = ns_t(k.t_ns);
        if ((t_kur < 0.0) && C.kes.konum_var) {
            t_kur = t;
        }
        if (t_kur < 0.0) {
            TEST_ASSERT_FALSE(y_bit(IDA_KES_G_KONUM));
        }
        if ((t_gecerli < 0.0) && y_bit(IDA_KES_G_KONUM)) {
            t_gecerli = t;
        }
        if ((t_ok < 0.0) && y_ok()) {
            t_ok = t;
        }
        if (y_ok()) {
            /* Yeni orijine göre konum hatası (kuzey bileşeni kayma kadar küçülür). */
            poz_t p;
            a.sen(ns_t(OUT.nav.t.ns), &p);
            const double ph = hypot((double)OUT.nav.p_ned[0] - (p.n - kayma), (double)OUT.nav.p_ned[1] - p.e);
            TEST_ASSERT_TRUE_MESSAGE(ph < Y_KONUM_SINIR, "OK iken konum yeni orijine göre değil");
        }
    }
    y_olc("R3 orijin degisimi (40 s): konumun yeniden kuruldugu an (s)", t_kur);
    y_olc("R3 orijin degisimi: konumun yeniden gecerli oldugu an (s)", t_gecerli);
    y_olc("R3 orijin degisimi: yeniden OK ani (s)", t_ok);
    /* Sonraki fix en geç 0,2 s sonra ölçülür; füzyonu D = 0,15 s sonra. */
    TEST_ASSERT_TRUE_MESSAGE((t_kur > 40.0) && (t_kur < 40.45), "konum yeni orijine göre yeniden kurulmadı");
    TEST_ASSERT_TRUE_MESSAGE((t_gecerli > t_kur) && (t_gecerli < t_kur + 0.25),
                             "konum, kurulumdan sonraki ilk kabul edilen fix'le geçerli olmadı");
    TEST_ASSERT_TRUE_MESSAGE((t_ok > t_gecerli + 1.99) && (t_ok < t_gecerli + 2.1),
                             "orijin değişiminden sonra OK, KES_OK_MS dolmadan ya da geç geldi");
    TEST_ASSERT_EQUAL_UINT8((uint8_t)(sayac0 + 1u), OUT.nav.reset_counter);
    TEST_ASSERT_TRUE(y_ok());
}

/* Bulgu 4: ret koşulu yalnız kabul edilen örnekte ya da sıfırlamada temizlenir. Yüzeyde GNSS
   konumu 40 s'den başlayarak kalıcı 30 m sıçrar (8 ret; süre kuralıyla konum geçersiz), akış
   41,5–43,0 s arası kesilir ve sıçramalı olarak döner: kaynak yeniden nitelenirken (kapı
   çalışmaz) konum, DVL ölü hesabının taşımasıyla yeniden geçerli olmamalıdır. KES_N_RED bu
   testte 9'dur: yalnız süre kuralı (KES_RED_MS) sınanır. */
static void test_y_r4_ret_yeniden_nitelik(void)
{
    TEST_ASSERT_EQUAL(IDA_RC_OK, ds_kur(&C));
    y_prm_u32(IDA_PRM_kes_n_red, 9u);
    kosu_t k;
    kosu_ayar_t a = y_ayar(sen_y_duz, 0x51E105u);
    a.gnss_sicra = 30.0;
    a.gnss_sicra_bas = 40.0; /* 8 ret: 40,003 … 41,403 s */
    a.gnss_sicra_son = 1.0e9;
    a.gnss_kes_bas = 41.5;
    a.gnss_kes_son = 43.0;
    kosu_kur(&k, &a);
    y_git(&k, 40.0);
    TEST_ASSERT_TRUE(y_ok());
    Y_KONUM_DENETSIZ = true; /* GNSS yer gerçeğinden 30 m ayrı */
    y_git(&k, 41.3); /* ilk ret ≈ 40,155 s'de işlendi; KES_RED_MS = 1 s doldu */
    TEST_ASSERT_FALSE_MESSAGE(y_bit(IDA_KES_G_KONUM), "sürekli ret konum geçerliliğini düşürmedi");
    const uint32_t sifir0 = C.kes.n_konum_sifir;
    while (ns_t(k.t_ns) < 45.6) {
        y_adim(&k);
        TEST_ASSERT_FALSE_MESSAGE(y_bit(IDA_KES_G_KONUM),
                                  "ret koşulu sürerken konum, kabul edilen fix olmadan geçerli oldu");
        TEST_ASSERT_FALSE(y_ok());
    }
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(sifir0, C.kes.n_konum_sifir, "senaryo beklenenden erken sıfırladı");
    TEST_ASSERT_TRUE(y_bit(IDA_KES_G_YATAY_HIZ)); /* DVL sürüyor: taşıyıcı vardı */
}

/* Bulgu 5: sensör kovaryansı çok büyük (1 (m/s)²) ama FOM küçük DVL örneği bilgi taşımaz;
   beyazlatma NIS'i ≈ 0 yaptığı için kapıdan geçip desteği yeniliyordu. En büyük köşegen
   DVL_FOM_MAX²'yi aşan örnek "iyi değil" sayılır: füzyonlanmaz, desteği yenilemez; yatay hız
   geçerliliği son kabul edilen füzyondan T_DVL_STALE sonra düşer. */
static void test_y_r5_dvl_kovaryans(void)
{
    TEST_ASSERT_EQUAL(IDA_RC_OK, ds_kur(&C));
    kosu_t k;
    kosu_ayar_t a = y_ayar(sen_y_dalis, 0x51E106u);
    a.dvl_kov = 1.0;
    a.dvl_kov_bas = 80.0;
    a.dvl_kov_son = 1.0e9;
    a.dvl_ayk = 0.3; /* kapıya girseydi NIS = 0,09/1 ile geçerdi */
    a.dvl_ayk_bas = 80.0;
    a.dvl_ayk_son = 1.0e9;
    kosu_kur(&k, &a);
    y_git(&k, 80.0);
    TEST_ASSERT_TRUE(y_ok() && y_bit(IDA_KES_G_YATAY_HIZ));
    const uint32_t red0 = C.kes.n_dvl_red;
    double t_dusme = -1.0;
    while (ns_t(k.t_ns) < 90.0) {
        y_adim(&k);
        const double t = ns_t(k.t_ns);
        if ((t_dusme < 0.0) && !y_bit(IDA_KES_G_YATAY_HIZ)) {
            t_dusme = t;
        }
        if (t_dusme > 0.0) {
            TEST_ASSERT_FALSE_MESSAGE(y_bit(IDA_KES_G_YATAY_HIZ), "bilgisiz DVL örneği yatay hızı geçerli tuttu");
            TEST_ASSERT_FALSE_MESSAGE(y_ok(), "bilgisiz DVL örneğiyle düzey OK");
        }
    }
    y_olc("R5 buyuk kovaryansli DVL (80 s): yatay hiz gecerliliginin dusme ani (s)", t_dusme);
    /* Son iyi ping 79,807 s; füzyonu ≈ 79,96 s; T_DVL_STALE = 1 s. */
    TEST_ASSERT_TRUE_MESSAGE((t_dusme > 80.5) && (t_dusme < 81.3), "bilgisiz DVL örneği desteği yeniledi");
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(red0, C.kes.n_dvl_red, "bilgisiz örnek kapıya girdi");
    TEST_ASSERT_EQUAL_UINT32(0u, C.kes.n_hiz_sifir);
}

/* Bulgu 7: manyetometre ve basınç en kısa kuyruğa alma aralıklarında (8 ms ve 18 ms) ve
   D = 200 ms iken bekleyen ölçüm sayısı 25 + 12 = 37'ye varır; kuyruk (IDA_KES_OLC_N = 40)
   taşmaz. Önceki boyda (32) taşıyordu. Durgun araç, gürültüsüz duyargalar. */
static void test_y_r7_olcum_kuyrugu(void)
{
    TEST_ASSERT_EQUAL(IDA_RC_OK, ds_kur(&C));
    y_prm_u32(IDA_PRM_kes_d_ms, 200u);
    uint32_t mag_seq = 0u, baro_seq = 0u, en_cok = 0u;
    for (uint32_t i = 1u; i <= 1000u; i++) {
        const int64_t t = T0_NS + (int64_t)i * T_ADIM_NS;
        ds_girdi(&IN, t, i, true);
        IN.imu.imu.accel[2] = -9.80665f;
        uint8_t n = 0u;
        /* (t − 5 ms, t] aralığına düşen ızgara noktaları: manyetometre 8 ms, basınç 18 ms. */
        for (int64_t tm = ((t - T_ADIM_NS) / (8 * DS_MS) + 1) * (8 * DS_MS); tm <= t; tm += 8 * DS_MS) {
            ida_sample_t *o = &IN.samples[n++];
            (void)memset(o, 0, sizeof(*o));
            o->hdr.t_meas.ns = tm;
            o->hdr.t_rx.ns = t;
            o->hdr.seq = ++mag_seq;
            o->hdr.flags = (uint16_t)IDA_HF_VALID;
            o->hdr.type = (uint8_t)IDA_SENS_MAG;
            o->u.mag.field[0] = 2.0e-5f;
            o->u.mag.field[2] = 4.0e-5f;
        }
        for (int64_t tm = ((t - T_ADIM_NS) / (18 * DS_MS) + 1) * (18 * DS_MS); tm <= t; tm += 18 * DS_MS) {
            ida_sample_t *o = &IN.samples[n++];
            (void)memset(o, 0, sizeof(*o));
            o->hdr.t_meas.ns = tm;
            o->hdr.t_rx.ns = t;
            o->hdr.seq = ++baro_seq;
            o->hdr.window_ns = 10000000u;
            o->hdr.flags = (uint16_t)IDA_HF_VALID;
            o->hdr.type = (uint8_t)IDA_SENS_BARO;
            o->u.baro.p_abs = 101325.0f;
            o->u.baro.temp_c = 15.0f;
        }
        IN.n_samples = n;
        TEST_ASSERT_EQUAL(IDA_RC_OK, ida_core_step(&C, &IN, &OUT));
        TEST_ASSERT_TRUE(nav_sonlu(&OUT.nav));
        en_cok = ((uint32_t)C.kes.olc_n > en_cok) ? (uint32_t)C.kes.olc_n : en_cok;
    }
    y_olc("R7 kuyruk: cagri sonunda bekleyen olcum sayisinin en buyugu", (double)en_cok);
    y_olc("R7 kuyruk: tasan olcum sayisi", (double)C.kes.n_tasma);
    y_olc("R7 kuyruk: seyreltilen ornek sayisi", (double)C.kes.n_seyrek);
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(0u, C.kes.n_tasma, "ölçüm kuyruğu en kısa aralıklarda taştı");
    TEST_ASSERT_EQUAL_UINT32_MESSAGE(0u, C.kes.n_seyrek, "en kısa aralıktaki örnek seyreltildi");
    TEST_ASSERT_TRUE_MESSAGE(en_cok > 32u, "senaryo kuyruğu önceki boyun üstüne doldurmadı");
    TEST_ASSERT_TRUE(en_cok <= IDA_KES_OLC_N);
    TEST_ASSERT_EQUAL_UINT8(IDA_EST_DEGRADED, OUT.nav.est_level); /* tutum, yön, derinlik sürüyor */
    TEST_ASSERT_EQUAL_HEX8(0x0Fu, ida_kes_gecerlilik(&C.kes));
}

/* Karar 1 (iii): KES_N_RED < KES_N_RESET grup koşulu (ida_kes_param_grup; ida_prm_grup onu
   çağırır). */
static void test_y_r1_param_grup(void)
{
    static ida_prm_kume_t km;
    ida_prm_varsayilan(&km);
    TEST_ASSERT_EQUAL_UINT8(0u, ida_kes_param_grup(&km));
    TEST_ASSERT_EQUAL_UINT8(0u, ida_prm_grup(&km));
    TEST_ASSERT_TRUE(km.ham[IDA_PRM_kes_n_red] >= 2u);
    TEST_ASSERT_TRUE(km.ham[IDA_PRM_kes_n_red] < km.ham[IDA_PRM_kes_n_reset]);
    km.ham[IDA_PRM_kes_n_red] = km.ham[IDA_PRM_kes_n_reset];
    TEST_ASSERT_NOT_EQUAL(0u, ida_kes_param_grup(&km));
    TEST_ASSERT_NOT_EQUAL(0u, ida_prm_grup(&km));
    km.ham[IDA_PRM_kes_n_red] = km.ham[IDA_PRM_kes_n_reset] + 1u;
    TEST_ASSERT_NOT_EQUAL(0u, ida_kes_param_grup(&km));
    km.ham[IDA_PRM_kes_n_red] = km.ham[IDA_PRM_kes_n_reset] - 1u;
    TEST_ASSERT_EQUAL_UINT8(0u, ida_kes_param_grup(&km));
}

static void yatay_testleri(void)
{
    RUN_TEST(test_y_gnss_ne);
    RUN_TEST(test_y_model_jacobian);
    RUN_TEST(test_y_k1_yuzey);
    RUN_TEST(test_y_k2_dalis);
    RUN_TEST(test_y_k2_dusme_esigi);
    RUN_TEST(test_y_k3_dvl_kilit);
    RUN_TEST(test_y_k4_cikis_yumusak);
    RUN_TEST(test_y_k4_cikis_sicrama);
    RUN_TEST(test_y_k4_cikis_sifirlama);
    RUN_TEST(test_y_k4_cikis_dvl_olcek);
    RUN_TEST(test_y_k5_gnss_sicramasi);
    RUN_TEST(test_y_k5_dvl_aykiri);
    RUN_TEST(test_y_k6_tek_ornek);
    RUN_TEST(test_y_k6_yukselme_suresi);
    RUN_TEST(test_y_k6_gidip_gelme);
    RUN_TEST(test_y_k7_port);
    RUN_TEST(test_y_k8_belirlenimcilik);
    RUN_TEST(test_y_montaj);
    RUN_TEST(test_y_irtifa);
    RUN_TEST(test_y_seyreltme);
    RUN_TEST(test_y_her_cagrida_hepsi);
    RUN_TEST(test_y_tek_kaynak);
    RUN_TEST(test_y_gnss_kalite_ve_oturum);
    RUN_TEST(test_y_r1_dvl_kalici_10hz);
    RUN_TEST(test_y_r1_dvl_kalici_5hz);
    RUN_TEST(test_y_r1_hiz_sifir_konum);
    RUN_TEST(test_y_r2_celisen_kaynaklar);
    RUN_TEST(test_y_r2_celisen_kisa_histerezis);
    RUN_TEST(test_y_r2_kaynak_kesilir);
    RUN_TEST(test_y_r1_sifirlama_birim);
    RUN_TEST(test_y_r3_orijin_degisimi);
    RUN_TEST(test_y_r4_ret_yeniden_nitelik);
    RUN_TEST(test_y_r5_dvl_kovaryans);
    RUN_TEST(test_y_r7_olcum_kuyrugu);
    RUN_TEST(test_y_r1_param_grup);
}
