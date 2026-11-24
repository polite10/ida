/*
 * tests/kabul/kontrol — acil durdurmada aktüatör çıktısı: kabul 1–3'ün kara kutu karşılıkları
 * (W5-kontrol-acil; EMNİYET). Kaynaklar ve model kabul_kontrol.h'de.
 *
 *   notr_*        Kural 1 / Kabul 1: etkin acil durdurma kısıtlayanken sekiz thr_cmd tam 0.0f;
 *                 kısıtlayana dönüşün ÇAĞRISINDA (bir çağrı gecikme yok) ve sürdüğü her çağrıda;
 *                 kip ve komutlar ne olursa olsun. Her alt durumda ön koşul olarak acil
 *                 durdurmadan önceki çıktının gerçekten nötr dışı olduğu doğrulanır.
 *   birikim_yok   Kural 2 / Kabul 2: kalkıştan sonraki çıktı dizisi acil durdurmanın süresinden
 *                 bağımsızdır (2 s ve 60 s; aynı kalkış anı ve aynı durum; bayt bayt). Durağan
 *                 dünyada hata acil durdurmadan önce ve sonra farklı seçilir (S0 / S1; aşağıda),
 *                 yoksa hiç sıfırlamayan bir çekirdek de iki koşuda aynı tümlevi biriktirirdi.
 *   rampa_*       Kural 3 / Kabul 3: KON_ACIL_RAMPA_MS zarfı, rampa sonunda eşitlik, rampa
 *                 sırasında yeniden acil durdurma, parametrenin tanımı ve B sınıfı.
 *
 * Her çağrıda ayrıca düzeneğin modeli denetlenir (kk_cagir): model kesin kısıtlayan derken
 * çıktı nötr olmalıdır.
 */
#include <stdio.h>
#include <string.h>

#include "kabul_kontrol.h"
#include "unity.h"

/* Rampa zarfı ve rampa sonu eşitliğinin sayısal payı (thr_cmd birimsiz, [−1, 1]). */
#define KK_PAY 1.0e-5f

/* ---- Yardımcılar ------------------------------------------------------------------------------ */

/* Ön koşul: silahlı ve son çıktı nötr dışı (acil durdurmadan önce gerçekten itki isteniyor). */
static void on_kosul_notr_disi(int satir, const char *ne)
{
    const ida_out_t *o = ka_out();
    if (!o->armed || kk_notr(o)) {
        ka_basarisiz(satir, "ön koşul (%s): silahlı ve nötr dışı komut yürürlükte değil (%s); test sınamadan biterdi", ne,
                     kk_thr_yazi(o));
    }
}

/* n çağrı; her birinde açıkça nötr beklenir (modelin denetimine ek). */
static void notr_kal(int n, const char *ne)
{
    for (int k = 0; k < n; ++k) {
        (void)kk_adim();
        KK_NOTR(ne);
    }
}

/* ---- 1. Nötr çıktı ------------------------------------------------------------------------------ */

/* Basılı acil durdurma: GUIDED'da nötr dışı komut yürürlükteyken estop = 1 örneğinin geldiği
   çağrıda ve 2 s boyunca her çağrıda nötr; bu sürede kip değişimi (MANUAL, GUIDED), yeni
   setpoint'ler, eksen komutları ve ARM gelir. */
static void t_notr_basili(void)
{
    kk_hazir((uint8_t)IDA_MODE_GUIDED);
    kk_adimlar(2 * KK_SN);
    on_kosul_notr_disi(__LINE__, "GUIDED");
    ka_ayar.estop = 1u;
    (void)kk_adim();
    KA_DOGRULA(kk_model_kisitlayan(), "model: estop = 1 örneğinin çağrısında kısıtlayan değil (düzenek)");
    KK_NOTR("estop = 1 örneğinin geldiği çağrı (Kural 1: bir çağrı gecikme yok)");
    for (int k = 0; k < 2 * KK_SN; ++k) {
        ida_in_t *in = kk_girdi();
        if (k % 50 == 10) {
            kk_op.sp.pos_ne[0] = -kk_op.sp.pos_ne[0];
            kk_op.sp.depth = kk_op.sp.depth > 4.0f ? 1.0f : 12.0f;
            kk_op.sp.yaw = -kk_op.sp.yaw;
        }
        if (k == 100) {
            (void)ka_mandal(in, (uint8_t)IDA_NARROW_MODE);
            in->narrow.mode = (uint8_t)IDA_MODE_MANUAL;
        } else if (k == 200 || k == 300) {
            kc_komut_ekle(in, ka_yeni_id(), (uint8_t)IDA_CMD_SET_MODE)->u.set_mode.mode = (uint8_t)IDA_MODE_GUIDED;
        } else if (k == 150 || k == 250) {
            (void)ka_komut(in, (uint8_t)IDA_CMD_ARM);
        }
        kk_operator_ekle(in);
        (void)kk_cagir();
        KK_NOTR("acil durdurma sürerken (kip ve komutlar değişiyor)");
    }
}

/* Her kipte: kipe geçilir, operatörün sürekli komutu verilir (GUIDED'da setpoint, MANUAL /
   STABILIZE / DEPTH_HOLD'da eksenler), acil durdurma gelir; o çağrıda ve 1 s boyunca nötr. Ön
   koşul (nötr dışı çıktı) GUIDED'da zorunludur (görev belgesi Kabul 1); öteki kiplerde bugünkü
   çekirdek MANUAL komutunu UNSUPPORTED ile yanıtladığı için ön koşul aranmaz, tutup tutmadığı
   iletiyle yazılır (nötr beklentisi yine her kipte geçerlidir). */
static void t_notr_kipler(void)
{
    static const uint8_t kipler[5] = {IDA_MODE_MANUAL, IDA_MODE_STABILIZE, IDA_MODE_DEPTH_HOLD, IDA_MODE_STATION_KEEP,
                                      IDA_MODE_GUIDED};
    char ne[96];
    char m[160];
    m[0] = '\0';
    for (unsigned i = 0; i < 5u; ++i) {
        uint8_t kip = kipler[i];
        kk_hazir(kip);
        kk_adimlar(KK_SN);
        (void)snprintf(ne, sizeof ne, "kip %u", (unsigned)kip);
        if (kip == IDA_MODE_GUIDED) {
            on_kosul_notr_disi(__LINE__, ne);
        }
        size_t u = strlen(m);
        (void)snprintf(m + u, sizeof m - u, "%skip %u: %s", u > 0u ? ", " : "notr_kipler ön koşul: ", (unsigned)kip,
                       kk_notr(ka_out()) ? "nötr" : "nötr dışı");
        ka_ayar.estop = 1u;
        (void)kk_adim();
        (void)snprintf(ne, sizeof ne, "kip %u: estop = 1 örneğinin geldiği çağrı", (unsigned)kip);
        KK_NOTR(ne);
        (void)snprintf(ne, sizeof ne, "kip %u: acil durdurma sürerken", (unsigned)kip);
        notr_kal(KK_SN, ne);
    }
    TEST_MESSAGE(m);
}

/* Bayatlık, ölçüt (i): emniyet akışı kesilir; son örneğin alımından bu yana T_emn_bayat KESİN
   aşıldığı ilk çağrıda ve sonrasında nötr. */
static void t_notr_bayat_alim(void)
{
    kk_hazir((uint8_t)IDA_MODE_GUIDED);
    kk_adimlar(2 * KK_SN);
    on_kosul_notr_disi(__LINE__, "GUIDED");
    int64_t son = ka_simdi(); /* son örneğin t_rx'i bu çağrının now_w'si */
    int64_t T = kk_t_bayat();
    ka_ayar.emniyet = false;
    int kesin = 0;
    for (int k = 0; k < 2 * KK_SN; ++k) {
        (void)kk_adim();
        if (ka_simdi() - son > T) {
            KA_DOGRULA(kk_model_kisitlayan(), "model: alım bayatlığında kısıtlayan değil (düzenek)");
            KK_NOTR("emniyet örneği T_emn_bayat'tan uzun süredir gelmiyor (W5-K4 eki (i))");
            ++kesin;
        }
    }
    KA_DOGRULA(kesin > 0, "düzenek: bayatlık süresi (%lld ns) testte aşılmadı", (long long)T);
}

/* Bayatlık, ölçüt (ii): örnekler gelir (seq ve alım zamanı yenilenir) ama t_meas donar;
   ölçüm zamanının son ilerlediği örneğin alımından bu yana T_emn_bayat KESİN aşılınca nötr. */
static void t_notr_bayat_olcum(void)
{
    kk_hazir((uint8_t)IDA_MODE_GUIDED);
    kk_adimlar(2 * KK_SN);
    on_kosul_notr_disi(__LINE__, "GUIDED");
    int64_t son = ka_simdi(); /* t_meas'in son ilerlediği örnek bu çağrınınki */
    int64_t T = kk_t_bayat();
    ka_ayar.tmeas_dondur = true;
    int kesin = 0;
    for (int k = 0; k < 2 * KK_SN; ++k) {
        (void)kk_adim();
        if (ka_simdi() - son > T) {
            KA_DOGRULA(kk_model_kisitlayan(), "model: ölçüm bayatlığında kısıtlayan değil (düzenek)");
            KK_NOTR("emniyet örneğinin ölçüm zamanı T_emn_bayat'tan uzun süredir ilerlemiyor (W5-K4 eki (ii))");
            ++kesin;
        }
    }
    KA_DOGRULA(kesin > 0, "düzenek: bayatlık süresi (%lld ns) testte aşılmadı", (long long)T);
}

/* Reddedilen çağrı: kısıtlayan örnek reddedilen bir çağrıda gelir (A4-K116: içeriği uygulanır).
   O çağrının çıktısı A4-K28 ile zaten sıfırdır; asıl sınanan, sonraki kabul edilen çağrılarda
   (gevşeten örnek gelmeden, bayatlık süresi dolmadan) çıktının nötr kalmasıdır. */
static void red_sonrasi(int satir, const char *ne)
{
    int64_t T = kk_t_bayat();
    int n = (int)(T / KK_P) - 2;
    if (n > 20) {
        n = 20;
    }
    KA_DOGRULA(n >= 1, "düzenek: T_emn_bayat (%lld ns) çok kısa", (long long)T);
    ka_ayar.estop = 0u;
    ka_ayar.emniyet = false;
    for (int k = 0; k < n; ++k) {
        ida_rc_t rc = kk_adim();
        KA_DOGRULA(rc == IDA_RC_OK, "sonraki çağrı %d döndü", (int)rc);
        if (!kk_notr(ka_out())) {
            ka_basarisiz(satir,
                         "%s: reddedilen çağrıdaki kısıtlayan örnekten %d çağrı sonra (gevşeten örnek yok) çıktı nötr değil (%s; "
                         "A4-K116: reddedilen çağrıdaki örneğin kısıtlayan içeriği etkin değeri kısıtlayan yapar; Kural 1)",
                         ne, k + 1, kk_thr_yazi(ka_out()));
        }
    }
    ka_ayar.emniyet = true;
}

static void t_notr_red_dup(void)
{
    kk_hazir((uint8_t)IDA_MODE_GUIDED);
    kk_adimlar(2 * KK_SN);
    on_kosul_notr_disi(__LINE__, "GUIDED");
    ka_ayar.estop = 1u;
    (void)kk_girdi_seq(kc_son_seq());
    ida_rc_t rc = kk_cagir();
    KA_DOGRULA(rc == IDA_RC_DUP_SEQ, "yinelenen tf.seq'li çağrı %d döndü, DUP_SEQ bekleniyordu", (int)rc);
    KK_NOTR("DUP_SEQ çağrısı (A4-K28)");
    red_sonrasi(__LINE__, "DUP_SEQ");
}

static void t_notr_red_bad(void)
{
    kk_hazir((uint8_t)IDA_MODE_GUIDED);
    kk_adimlar(2 * KK_SN);
    on_kosul_notr_disi(__LINE__, "GUIDED");
    ka_ayar.estop = 1u;
    ida_in_t *in = kk_girdi();
    in->narrow.mask = (uint8_t)(in->narrow.mask | 0x10u); /* tanımsız mandal biti: BAD_INPUT (A4-K113) */
    ida_rc_t rc = kk_cagir();
    KA_DOGRULA(rc == IDA_RC_BAD_INPUT, "tanımsız mandal bitli çağrı %d döndü, BAD_INPUT bekleniyordu", (int)rc);
    KK_NOTR("BAD_INPUT çağrısı (A4-K28)");
    red_sonrasi(__LINE__, "BAD_INPUT");
}

/* inst ≠ 0 örneğin kısıtlayan içeriği: aynı çağrıda gevşek inst 0 örneği olsa da (A4-K116). */
static void t_notr_inst(void)
{
    kk_hazir((uint8_t)IDA_MODE_GUIDED);
    kk_adimlar(2 * KK_SN);
    on_kosul_notr_disi(__LINE__, "GUIDED");
    for (int k = 0; k < 20; ++k) {
        ida_in_t *in = kk_girdi();
        (void)ka_emniyet_ekle(in, 1u, 1u, KC_TUP_ACIK, KC_TUP_ACIK);
        kk_operator_ekle(in);
        (void)kk_cagir();
        KK_NOTR(k == 0 ? "inst 1 estop = 1 örneğinin geldiği çağrı (inst 0 gevşek)" : "inst 1 estop = 1 sürerken");
    }
}

/* A4-K33 ile reddedilen (VALID = 0) inst 0 örneğinin kısıtlayan içeriği (A4-K116). */
static void t_notr_gecersiz(void)
{
    kk_hazir((uint8_t)IDA_MODE_GUIDED);
    kk_adimlar(2 * KK_SN);
    on_kosul_notr_disi(__LINE__, "GUIDED");
    ka_ayar.emniyet = false;
    for (int k = 0; k < 20; ++k) {
        ida_in_t *in = kk_girdi();
        ida_sample_t *s = ka_emniyet_ekle(in, 0u, 1u, KC_TUP_ACIK, KC_TUP_ACIK);
        s->hdr.flags = 0u;
        kk_operator_ekle(in);
        (void)kk_cagir();
        KK_NOTR(k == 0 ? "VALID = 0, estop = 1 örneğinin geldiği çağrı" : "VALID = 0, estop = 1 örnekler sürerken");
    }
}

/* init'ten beri hiç gevşememiş: emniyet örneği baştan basılı (ya da hiç gelmiyor); 5 s boyunca
   ARM, GUIDED, MANUAL ve sürekli komutlar istenir; her çağrı nötr. */
static void init_kosu(bool orneksiz)
{
    kk_kur();
    ka_ayar.estop = 1u;
    ka_ayar.emniyet = !orneksiz;
    kk_oturum_ac();
    bool silahli = false;
    for (int k = 0; k < 5 * KK_SN; ++k) {
        ida_in_t *in = kk_girdi();
        if (k % 50 == 0) {
            (void)ka_komut(in, (uint8_t)IDA_CMD_ARM);
        } else if (k % 100 == 25) {
            kc_komut_ekle(in, ka_yeni_id(), (uint8_t)IDA_CMD_SET_MODE)->u.set_mode.mode = (uint8_t)IDA_MODE_GUIDED;
        } else if (k % 100 == 75) {
            (void)ka_mandal(in, (uint8_t)IDA_NARROW_MODE);
            in->narrow.mode = (uint8_t)IDA_MODE_MANUAL;
        }
        kk_operator_ekle(in);
        (void)kk_cagir();
        KK_NOTR(orneksiz ? "init'ten beri hiç emniyet örneği gelmedi" : "init'ten beri acil durdurma basılı");
        silahli = silahli || ka_out()->armed;
    }
    char m[96];
    (void)snprintf(m, sizeof m, "init_%s: araç bu sürede %s", orneksiz ? "orneksiz" : "basili",
                   silahli ? "silahlandı" : "hiç silahlanmadı");
    TEST_MESSAGE(m);
}

static void t_notr_init(void)
{
    init_kosu(false);
}

static void t_notr_init_orneksiz(void)
{
    init_kosu(true);
}

/* ---- 2. Birikim yok ----------------------------------------------------------------------------- */

typedef struct {
    bool valid;
    bool safe;
    bool armed;
    uint8_t kip;
    float thr[IDA_PORT_N_THR];
} kk_iz_t;

typedef struct {
    bool armed;
    uint8_t kip;
    uint8_t fs;
    uint8_t est;
    uint8_t faz;
    uint8_t rejim;
    ida_nav_out_t nav;
    int64_t t;
    float once_sapma;
} kk_durum_t;

#define BK_KAYIT 600              /* kalkıştan sonra 3 s */
#define BK_TOPLAM (61 * KK_SN)    /* hazırlıktan kalkışa kadar */
#define RP_KAYIT_AZAMI (KK_RAMPA_AZAMI_MS / 5u + 2u * KK_SN)

static kk_iz_t g_iz_a[RP_KAYIT_AZAMI];
static kk_iz_t g_iz_b[RP_KAYIT_AZAMI];

static void iz_al(kk_iz_t *z, const ida_out_t *o)
{
    memset(z, 0, sizeof *z);
    z->valid = o->thr_valid;
    z->safe = o->is_safe;
    z->armed = o->armed;
    z->kip = o->status.mode;
    memcpy(z->thr, o->thr_cmd, sizeof z->thr);
}

static void durum_al(kk_durum_t *d, const ida_out_t *o)
{
    memset(d, 0, sizeof *d);
    d->armed = o->armed;
    d->kip = o->status.mode;
    d->fs = o->fs_level;
    d->est = o->nav.est_level;
    d->faz = o->status.phase;
    d->rejim = o->status.regime;
    d->nav = o->nav;
    d->t = ka_simdi();
}

/* Acil durdurmadan sonra kalkış ve kayıt (n çağrı; ilki gevşeten örneğin çağrısı). */
static void kalkis_kaydet(kk_iz_t *iz, int n, kk_durum_t *d)
{
    ka_ayar.estop = 0u;
    for (int k = 0; k < n; ++k) {
        (void)kk_adim();
        if (k == 0) {
            durum_al(d, ka_out());
            KA_DOGRULA(!kk_model_kisitlayan(), "model: kalkış çağrısında hâlâ kısıtlayan (düzenek; neden %s)",
                       kk_neden_adi((int)kk_model_neden()));
        }
        iz_al(&iz[k], ka_out());
    }
}

/* Durağan dünyada (sensörler sabit) kontrolcünün girdisi acil durdurmadan etkilenmez; bu yüzden
   acil durdurma boyunca hata acil durdurmadan önceki hatadan farklı seçilir: önce S0 (S1'in
   aynası: ters yönde büyük konum, yön hatası ve yüzey), acil durdurmanın başladığı çağrıdan
   kalkıştan sonrasına kadar S1 (büyük derinlik ve yön hatası). Kural 2'ye uyan çekirdekte kalkış
   anındaki kontrolcü durumu yalnız o anki ölçüme ve S1'e bağlıdır; acil durdurma boyunca (ya da
   ondan önce) biriktiren bir çekirdekte 2 s'lik ve 60 s'lik koşuların tümlevleri farklıdır. */
static void sp_s0(void)
{
    kk_operator_varsayilan();
    kk_op.sp.pos_ne[0] = -kk_op.sp.pos_ne[0];
    kk_op.sp.pos_ne[1] = -kk_op.sp.pos_ne[1];
    kk_op.sp.depth = 0.0f;
    kk_op.sp.yaw = -kk_op.sp.yaw;
}

static void birikim_kosu(int normal, int acil, kk_iz_t *iz, kk_durum_t *d)
{
    kk_hazir((uint8_t)IDA_MODE_GUIDED);
    sp_s0();
    kk_adimlar(normal);
    on_kosul_notr_disi(__LINE__, "acil durdurmadan hemen önce GUIDED'da büyük konum/yön hatası (S0)");
    float once = kk_sapma(ka_out());
    kk_operator_varsayilan(); /* S1 */
    ka_ayar.estop = 1u;
    kk_adimlar(acil); /* her çağrıda modelle nötr denetlenir */
    kalkis_kaydet(iz, BK_KAYIT, d);
    d->once_sapma = once;
}

/* Seyrüsefer çıkışının alanları (yapı dolgusu karşılaştırılmaz). */
static bool nav_esit(const ida_nav_out_t *a, const ida_nav_out_t *b)
{
    return a->t.ns == b->t.ns && memcmp(a->p_ned, b->p_ned, sizeof a->p_ned) == 0 &&
           memcmp(a->v_ned, b->v_ned, sizeof a->v_ned) == 0 && memcmp(a->q_nb, b->q_nb, sizeof a->q_nb) == 0 &&
           memcmp(a->omega_b, b->omega_b, sizeof a->omega_b) == 0 && memcmp(&a->depth, &b->depth, sizeof a->depth) == 0 &&
           memcmp(&a->altitude, &b->altitude, sizeof a->altitude) == 0 && a->est_level == b->est_level;
}

static void durum_esit(const kk_durum_t *a, const kk_durum_t *b, const char *ne)
{
    bool nav = nav_esit(&a->nav, &b->nav);
    KA_DOGRULA(a->t == b->t && a->armed == b->armed && a->kip == b->kip && a->fs == b->fs && a->est == b->est &&
                   a->faz == b->faz && a->rejim == b->rejim && nav,
               "ön koşul (%s): iki koşunun kalkış anındaki durumu aynı değil (t %lld/%lld, armed %d/%d, kip %u/%u, fs %u/%u, "
               "est %u/%u, faz %u/%u, rejim %u/%u, nav %s); karşılaştırma anlamsız olurdu",
               ne, (long long)a->t, (long long)b->t, (int)a->armed, (int)b->armed, (unsigned)a->kip, (unsigned)b->kip,
               (unsigned)a->fs, (unsigned)b->fs, (unsigned)a->est, (unsigned)b->est, (unsigned)a->faz, (unsigned)b->faz,
               (unsigned)a->rejim, (unsigned)b->rejim, nav ? "aynı" : "farklı");
}

static bool iz_notr_disi(const kk_iz_t *z)
{
    for (unsigned i = 0; i < IDA_PORT_N_THR; ++i) {
        if (z->thr[i] != 0.0f) {
            return true;
        }
    }
    return false;
}

static void t_birikim_yok(void)
{
    kk_durum_t da;
    kk_durum_t db;
    memset(&da, 0, sizeof da);
    memset(&db, 0, sizeof db);
    /* A: 59 s olağan, 2 s acil durdurma; B: 1 s olağan, 60 s acil durdurma; kalkış aynı çağrıda. */
    birikim_kosu(BK_TOPLAM - 2 * KK_SN, 2 * KK_SN, g_iz_a, &da);
    birikim_kosu(BK_TOPLAM - 60 * KK_SN, 60 * KK_SN, g_iz_b, &db);
    durum_esit(&da, &db, "2 s / 60 s");
    bool notr_disi = false;
    for (int k = 0; k < BK_KAYIT; ++k) {
        notr_disi = notr_disi || iz_notr_disi(&g_iz_a[k]);
    }
    KA_DOGRULA(notr_disi, "ön koşul: kalkıştan sonra 3 s içinde nötr dışı çıktı yok (karşılaştırma anlamsız olurdu)");
    for (int k = 0; k < BK_KAYIT; ++k) {
        const kk_iz_t *a = &g_iz_a[k];
        const kk_iz_t *b = &g_iz_b[k];
        if (memcmp(a, b, sizeof *a) != 0) {
            ka_basarisiz(__LINE__,
                         "kalkıştan %d ms sonra (çağrı %d) çıktı acil durdurmanın süresine bağlı: 2 s'lik koşu thr [%.5f %.5f %.5f "
                         "%.5f %.5f %.5f %.5f %.5f] (valid %d, safe %d, armed %d, kip %u), 60 s'lik koşu thr [%.5f %.5f %.5f %.5f "
                         "%.5f %.5f %.5f %.5f] (valid %d, safe %d, armed %d, kip %u); acil durdurma sürerken birikim var "
                         "(Kural 2; Kabul 2)",
                         k * 5, k, (double)a->thr[0], (double)a->thr[1], (double)a->thr[2], (double)a->thr[3],
                         (double)a->thr[4], (double)a->thr[5], (double)a->thr[6], (double)a->thr[7], (int)a->valid, (int)a->safe,
                         (int)a->armed, (unsigned)a->kip, (double)b->thr[0], (double)b->thr[1], (double)b->thr[2],
                         (double)b->thr[3], (double)b->thr[4], (double)b->thr[5], (double)b->thr[6], (double)b->thr[7],
                         (int)b->valid, (int)b->safe, (int)b->armed, (unsigned)b->kip);
        }
    }
    char m[160];
    (void)snprintf(m, sizeof m, "birikim_yok: acil durdurmadan önceki en büyük |thr| 2 s koşusu %.4f, 60 s koşusu %.4f",
                   (double)da.once_sapma, (double)db.once_sapma);
    TEST_MESSAGE(m);
}

/* ---- 3. Rampa ------------------------------------------------------------------------------------ */

static void rampa_param(ida_param_desc_t *d)
{
    if (!kk_param_bul(KK_RAMPA_AD, d)) {
        ka_basarisiz(__LINE__, "parametre bulunamadı: şemada '%s' yok (W5-kontrol-acil \"Bağımsız kabul testleri\": ad "
                               "sabitlendi)",
                     KK_RAMPA_AD);
    }
}

/* Rampa parametresi kurulup (silahsızken) silahlanır, GUIDED'da 2 s, 2 s acil durdurma, kalkış ve
   n çağrı kayıt. Zaman çizelgesi parametre değerinden bağımsızdır (sabit sayılı adımlar). */
static void rampa_kosu(const ida_param_desc_t *d, int64_t ms, kk_iz_t *iz, int n, kk_durum_t *ds)
{
    uint32_t sid = 0;
    uint32_t cid = 0;
    kk_kur();
    kk_oturum_ac();
    kk_param_gonder(d, ms, &sid, &cid);
    KA_SONUC(sid, IDA_ACK_ACCEPTED, IDA_REASON_NONE, "silahsızken KON_ACIL_RAMPA_MS PARAM_SET");
    KA_SONUC(cid, IDA_ACK_ACCEPTED, IDA_REASON_NONE, "silahsızken KON_ACIL_RAMPA_MS PARAM_COMMIT");
    KA_DOGRULA(kk_param_oku(d) == ms, "commit sonrası KON_ACIL_RAMPA_MS %lld, beklenen %lld", (long long)kk_param_oku(d),
               (long long)ms);
    KA_DOGRULA(kk_silahla(20), "ön koşul: araç silahlanamadı");
    KA_DOGRULA(kk_kip((uint8_t)IDA_MODE_GUIDED), "ön koşul: GUIDED'a geçilemedi");
    kk_adimlar(2 * KK_SN);
    on_kosul_notr_disi(__LINE__, "rampa koşusu, acil durdurmadan önce");
    ka_ayar.estop = 1u;
    kk_adimlar(2 * KK_SN);
    kalkis_kaydet(iz, n, ds);
}

static void t_rampa_zarf(void)
{
    ida_param_desc_t d;
    rampa_param(&d);
    int64_t R = kk_param_tamsayi(&d.def);
    KA_DOGRULA(R > 0 && R <= (int64_t)KK_RAMPA_AZAMI_MS, "KON_ACIL_RAMPA_MS varsayılanı %lld; zarf sınanamaz", (long long)R);
    int n = (int)(R / 5) + 2 * KK_SN;
    kk_durum_t dz;
    kk_durum_t dr;
    memset(&dz, 0, sizeof dz);
    memset(&dr, 0, sizeof dr);
    rampa_kosu(&d, 0, g_iz_a, n, &dz);  /* rampasız */
    rampa_kosu(&d, R, g_iz_b, n, &dr);  /* varsayılan rampa */
    durum_esit(&dz, &dr, "rampasız / rampalı");
    int64_t R_ns = R * KA_MS;
    bool rampada_notr_disi = false;
    bool notr_disi = false;
    for (int k = 0; k < n; ++k) {
        const kk_iz_t *z = &g_iz_a[k];
        const kk_iz_t *r = &g_iz_b[k];
        /* t: kalkış çağrısı (k = 0) bir periyot sayılır (gevşek; rampa o çağrıda başlar). */
        int64_t t = (int64_t)(k + 1) * KK_P;
        float f = t >= R_ns ? 1.0f : (float)t / (float)R_ns;
        if (iz_notr_disi(z)) {
            notr_disi = true;
            if (t < R_ns) {
                rampada_notr_disi = true;
            }
        }
        for (unsigned i = 0; i < IDA_PORT_N_THR; ++i) {
            float zarf = f * kk_mutlak(z->thr[i]) + KK_PAY;
            if (kk_mutlak(r->thr[i]) > zarf) {
                ka_basarisiz(__LINE__,
                             "kalkıştan %lld ms sonra (çağrı %d) kanal %u: rampalı %.6f, rampasız %.6f; zarf min(1, t/%lld ms) × "
                             "|rampasız| = %.6f aşıldı (Kural 3; Kabul 3)",
                             (long long)(k * 5), k, i, (double)r->thr[i], (double)z->thr[i], (long long)R,
                             (double)(zarf - KK_PAY));
            }
            if (t >= R_ns + KK_P && kk_mutlak(r->thr[i] - z->thr[i]) > KK_PAY) {
                ka_basarisiz(__LINE__,
                             "rampa bittikten sonra (kalkıştan %lld ms, çağrı %d) kanal %u: rampalı %.6f, rampasız %.6f; eşit "
                             "olmalıydı (Kural 3; Kabul 3: rampa bitince olağan çıktı)",
                             (long long)(k * 5), k, i, (double)r->thr[i], (double)z->thr[i]);
            }
        }
    }
    KA_DOGRULA(notr_disi && rampada_notr_disi,
               "ön koşul: rampasız koşuda kalkıştan sonra (rampa süresi içinde) nötr dışı çıktı yok; zarf sınanmadı");
}

static void t_rampa_yeniden(void)
{
    ida_param_desc_t d;
    rampa_param(&d);
    kk_hazir((uint8_t)IDA_MODE_GUIDED);
    int64_t R = kk_param_oku(&d);
    KA_DOGRULA(R >= 20, "yürürlükteki KON_ACIL_RAMPA_MS %lld ms; rampa ortası sınanamaz", (long long)R);
    kk_adimlar(2 * KK_SN);
    on_kosul_notr_disi(__LINE__, "GUIDED");
    ka_ayar.estop = 1u;
    kk_adimlar(KK_SN);
    ka_ayar.estop = 0u;
    int yarim = (int)(R / 10); /* R/2 ms = R/10 çağrı */
    bool gordu = false;
    for (int k = 0; k < yarim; ++k) {
        (void)kk_adim();
        gordu = gordu || !kk_notr(ka_out());
    }
    KA_DOGRULA(gordu, "ön koşul: kalkıştan sonra rampanın ilk yarısında (%lld ms) nötr dışı çıktı yok", (long long)(R / 2));
    ka_ayar.estop = 1u;
    (void)kk_adim();
    KK_NOTR("rampa sırasında yeniden gelen acil durdurmanın çağrısı (Kabul 3)");
    notr_kal(KK_SN, "rampa sırasında yeniden gelen acil durdurma sürerken");
}

static void t_rampa_parametre(void)
{
    ida_param_desc_t d;
    rampa_param(&d);
    KA_DOGRULA(d.cls == IDA_PCLS_B, "KON_ACIL_RAMPA_MS sınıfı %u, IDA_PCLS_B bekleniyordu (Kural 3)", (unsigned)d.cls);
    KA_DOGRULA(d.type == IDA_PT_U32 || d.type == IDA_PT_I32 || d.type == IDA_PT_F32, "KON_ACIL_RAMPA_MS türü %u",
               (unsigned)d.type);
    KA_DOGRULA(kk_param_tamsayi(&d.min) == 0 && kk_param_tamsayi(&d.max) == (int64_t)KK_RAMPA_AZAMI_MS &&
                   kk_param_tamsayi(&d.def) == (int64_t)KK_RAMPA_VARSAYILAN_MS,
               "KON_ACIL_RAMPA_MS aralığı %lld–%lld, varsayılanı %lld; 0–5000 ve 500 bekleniyordu (görev belgesi)",
               (long long)kk_param_tamsayi(&d.min), (long long)kk_param_tamsayi(&d.max), (long long)kk_param_tamsayi(&d.def));
    /* Silahsızken: aralık dışı değer uygulanmaz, sınır değer uygulanır. */
    kk_kur();
    kk_oturum_ac();
    int64_t once = kk_param_oku(&d);
    uint32_t sid = 0;
    uint32_t cid = 0;
    kk_param_gonder(&d, (int64_t)KK_RAMPA_AZAMI_MS + 1, &sid, &cid);
    KA_DOGRULA(kk_param_oku(&d) == once, "aralık dışı KON_ACIL_RAMPA_MS (5001) uygulandı (A4-K80, K81)");
    kk_param_gonder(&d, (int64_t)KK_RAMPA_AZAMI_MS, &sid, &cid);
    KA_SONUC(sid, IDA_ACK_ACCEPTED, IDA_REASON_NONE, "silahsızken KON_ACIL_RAMPA_MS = 5000 PARAM_SET");
    KA_SONUC(cid, IDA_ACK_ACCEPTED, IDA_REASON_NONE, "silahsızken KON_ACIL_RAMPA_MS = 5000 PARAM_COMMIT");
    KA_DOGRULA(kk_param_oku(&d) == (int64_t)KK_RAMPA_AZAMI_MS, "commit sonrası değer %lld", (long long)kk_param_oku(&d));
    /* Silahlıyken: B sınıfı parametreye PARAM_SET ARMED ile reddedilir, değer değişmez (A4-K81). */
    KA_DOGRULA(kk_silahla(20), "ön koşul: araç silahlanamadı");
    kk_param_gonder(&d, 100, &sid, &cid);
    KA_SONUC(sid, IDA_ACK_REJECTED, IDA_REASON_ARMED, "silahlıyken KON_ACIL_RAMPA_MS PARAM_SET (A4-K81)");
    KA_DOGRULA(kk_param_oku(&d) == (int64_t)KK_RAMPA_AZAMI_MS, "silahlıyken KON_ACIL_RAMPA_MS değişti: %lld",
               (long long)kk_param_oku(&d));
}

/* ---- Tablo ------------------------------------------------------------------------------------ */

static const ka_test_t TESTLER[] = {
    {"notr_basili", t_notr_basili},
    {"notr_kipler", t_notr_kipler},
    {"notr_bayat_alim", t_notr_bayat_alim},
    {"notr_bayat_olcum", t_notr_bayat_olcum},
    {"notr_red_dup", t_notr_red_dup},
    {"notr_red_bad", t_notr_red_bad},
    {"notr_inst", t_notr_inst},
    {"notr_gecersiz", t_notr_gecersiz},
    {"notr_init", t_notr_init},
    {"notr_init_orneksiz", t_notr_init_orneksiz},
    {"birikim_yok", t_birikim_yok},
    {"rampa_zarf", t_rampa_zarf},
    {"rampa_yeniden", t_rampa_yeniden},
    {"rampa_parametre", t_rampa_parametre},
};

int main(int argc, char **argv)
{
    return ka_main(argc, argv, __FILE__, TESTLER, sizeof TESTLER / sizeof TESTLER[0]);
}
