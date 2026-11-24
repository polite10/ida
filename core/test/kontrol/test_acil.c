/*
 * test_acil.c — acil durdurmada kontrol çıktısı (W5-kontrol-acil; W5-K26; EMNİYET).
 *
 * Kuralın dört maddesi (kontrol.h başlığı):
 *   1. etkin acil durdurma kısıtlayanken sekiz komutun bit deseni tam 0x00000000 (aynı çağrıda);
 *   2. birikimli durum sıfır ve birikmiyor;
 *   3. kalkıştan sonra çıktı KON_ACIL_RAMPA süresince rampayla açılıyor: kalkış adımı k = 0,
 *      k. adımda çıktı = min(1, (k + 1) · P / T) · rampasız çıktı (P adım aralığı; mimar kararı);
 *   4. çekirdek bunu tek başına yapıyor (bu testlerde sim yok: yalnız ida_core_step/tick).
 *
 * İki düzey: (D) kontrol modülü doğrudan (ida_kon_hesapla, ida_kon_acil_izle, ida_kon_cikti);
 * (Ç) gerçek çekirdek üzerinden, emniyet örnekleriyle (A4-K116, W5-K4, W5-K20). Çekirdek
 * düzeneği core/test/gudum/test_gudum_gecis.c'ninkiyle aynıdır: duyargalar 1 m derinlikte durgun
 * bir aracı söyler, GUIDED_SP derinlik 2 m ister; hedef uygulanırken dikey iticiler aşağı iter.
 *
 * Rampanın eş kontrolcüsü: KON_ACIL_RAMPA = 0 ile aynı girdileri gören ikinci bir kontrolcü
 * olağan çıktıyı verir; rampalı kontrolcünün çıktısı her adımda bununla karşılaştırılır.
 */
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "destek.h"
#include "kontrol/kontrol.h"
#include "mat/ida_math.h"
#include "param/param.h"
#include "unity.h"

void setUp(void);
void tearDown(void);

#define ADIM_NS  (5 * DS_MS)
#define RAMPA_MS 500u
#define RAMPA_NS ((int64_t)RAMPA_MS * DS_MS)

static ida_prm_kume_t P;  /* varsayılanlar: KON_ACIL_RAMPA = 500 ms */
static ida_prm_kume_t P0; /* rampasız eş: KON_ACIL_RAMPA = 0        */
static ida_kon_t K1;
static ida_kon_t K2;
static ida_emn_t E;
static ida_core_t C;
static ida_in_t IN;
static ida_out_t OUT;

void setUp(void)
{
    ida_prm_varsayilan(&P);
    P0 = P;
    P0.ham[IDA_PRM_kon_acil_rampa_ms] = 0u;
    ida_kon_kur(&K1);
    ida_kon_kur(&K2);
    ida_emn_kur(&E);
}
void tearDown(void) {}

typedef union {
    uint32_t u;
    float f;
} bit32_t;

static uint32_t bitler(float f)
{
    bit32_t b;
    b.f = f;
    return b.u;
}

static float mutlak(float x)
{
    return (x < 0.0f) ? -x : x;
}

/* İletiye metin ve onluk sayı ekler (kart derlemesinde snprintf yok). */
static void ek(char *m, size_t boy, size_t *k, const char *metin, uint32_t v)
{
    for (size_t i = 0u; (metin[i] != '\0') && ((*k + 1u) < boy); i++) {
        m[*k] = metin[i];
        (*k)++;
    }
    char rakam[10];
    uint32_t n = 0u;
    do {
        rakam[n] = (char)('0' + (char)(v % 10u));
        n++;
        v /= 10u;
    } while ((v != 0u) && (n < 10u));
    while ((n > 0u) && ((*k + 1u) < boy)) {
        n--;
        m[*k] = rakam[n];
        (*k)++;
    }
    m[*k] = '\0';
}

/* ============================ (D) kontrol modülü doğrudan ================================= */

#define TUR_SAYISI 4u

/* Etkin (silahlı, kestirimi yeterli) ve büyük hatalı girdi; tur kanal seçimini değiştirir. */
static void etkin_girdi(ida_kon_girdi_t *g, int64_t t_ns, uint32_t tur)
{
    (void)memset(g, 0, sizeof(*g));
    ida_kon_istek_t *s = &g->istek;
    s->kip = IDA_KON_KIP_DALIS;
    s->derinlik = 30.0f;
    s->yon = 2.0f;
    s->hiz[0] = 0.8f;
    s->hiz[1] = -0.4f;
    switch (tur) {
    case 1u: /* doğrudan kuvvet ve moment */
        s->dikey = IDA_KON_DIKEY_KUVVET;
        s->tutum = IDA_KON_TUTUM_MOMENT;
        s->yon_kanali = IDA_KON_YON_MOMENT;
        s->yatay = IDA_KON_YATAY_KUVVET;
        s->kuvvet[0] = 20.0f;
        s->kuvvet[1] = -10.0f;
        s->kuvvet[2] = 15.0f;
        s->kuvvet[3] = 2.0f;
        s->kuvvet[4] = -2.0f;
        s->kuvvet[5] = 3.0f;
        break;
    case 2u: /* yüzey: yön açısı ve yatay hız */
        s->kip = IDA_KON_KIP_YUZEY;
        s->yon_kanali = IDA_KON_YON_ACI;
        s->yatay = IDA_KON_YATAY_HIZ;
        break;
    case 3u: /* hız kanalları */
        s->dikey = IDA_KON_DIKEY_HIZ;
        s->dikey_hiz = 0.4f;
        s->tutum = IDA_KON_TUTUM_HIZ;
        s->yalpa_hizi = 0.3f;
        s->bas_kic_hizi = -0.2f;
        s->yon_kanali = IDA_KON_YON_HIZ;
        s->yon_hizi = 0.5f;
        s->yatay = IDA_KON_YATAY_HIZ;
        break;
    default: /* derinlik, tutum açısı, yön açısı, yatay hız */
        s->dikey = IDA_KON_DIKEY_DERINLIK;
        s->tutum = IDA_KON_TUTUM_ACI;
        s->yon_kanali = IDA_KON_YON_ACI;
        s->yatay = IDA_KON_YATAY_HIZ;
        break;
    }
    g->nav.t.ns = t_ns;
    g->nav.q_nb[0] = 1.0f;
    g->nav.depth = 0.5f;
    g->nav.est_level = (uint8_t)IDA_EST_OK;
    g->t_ns = t_ns;
    g->armed = true;
    g->kisit = false;
    g->kestirim_yeterli = true;
    g->acil = false;
}

static bool itki_hepsi_sifir(const ida_kon_t *k)
{
    float thr[IDA_PORT_N_THR];
    bool cokme = true;
    ida_kon_itki(k, thr, &cokme);
    bool s = true;
    for (uint32_t i = 0u; i < IDA_PORT_N_THR; i++) {
        s = s && (bitler(thr[i]) == 0u);
    }
    return s;
}

static bool cikti_hepsi_sifir(const ida_kon_t *k, const ida_emn_t *e)
{
    float thr[IDA_PORT_N_THR];
    bool cokme = true;
    for (uint32_t i = 0u; i < IDA_PORT_N_THR; i++) {
        thr[i] = 0.5f; /* yazılmayan kanal yakalanır */
    }
    ida_kon_cikti(k, e, thr, &cokme);
    bool s = true;
    for (uint32_t i = 0u; i < IDA_PORT_N_THR; i++) {
        s = s && (bitler(thr[i]) == 0u);
    }
    return s;
}

static bool itki_var(const ida_kon_t *k)
{
    bool v = false;
    for (uint32_t i = 0u; i < IDA_PORT_N_THR; i++) {
        v = v || (k->thr[i] != 0.0f);
    }
    return v && (k->cokme == 0u);
}

/* Birikimli durumun tamamı sıfır mı (kural 2): integraller, referanslar, itki değişim hızı
   belleği, doyma belleği, son çıktı; kanal izleri "etkin değil". */
static bool durum_sifir(const ida_kon_t *k)
{
    bool s = (bitler(k->i_w) == 0u) && (bitler(k->i_v[0]) == 0u) && (bitler(k->i_v[1]) == 0u) &&
             (bitler(k->d_ref) == 0u) && (bitler(k->psi_ref) == 0u) && (k->d_ref_var == 0u) &&
             (k->psi_ref_var == 0u) && (k->son_kip == 255u) && (k->son_dikey == 255u) &&
             (k->son_tutum == 255u) && (k->son_yon == 255u) && (k->son_yatay == 255u);
    for (uint32_t j = 0u; j < 3u; j++) {
        s = s && (bitler(k->i_pqr[j]) == 0u);
    }
    for (uint32_t i = 0u; i < IDA_PORT_N_THR; i++) {
        s = s && (bitler(k->f[i]) == 0u) && (bitler(k->thr[i]) == 0u);
    }
    for (uint32_t j = 0u; j < IDA_TAH_EKSEN; j++) {
        s = s && (k->doyma[j] == 0);
    }
    return s;
}

/* İki kontrolcünün rampa dışındaki iç durumu bit bit aynı mı (thr[], rampa_var ve rampa_t0_ns
   hariç: rampa yalnız bunları ayırır). */
static bool ic_durum_esit(const ida_kon_t *a, const ida_kon_t *b)
{
    bool s = (bitler(a->i_w) == bitler(b->i_w)) && (bitler(a->i_v[0]) == bitler(b->i_v[0])) &&
             (bitler(a->i_v[1]) == bitler(b->i_v[1])) && (bitler(a->d_ref) == bitler(b->d_ref)) &&
             (bitler(a->psi_ref) == bitler(b->psi_ref)) && (a->d_ref_var == b->d_ref_var) &&
             (a->psi_ref_var == b->psi_ref_var) && (a->son_kip == b->son_kip) &&
             (a->son_dikey == b->son_dikey) && (a->son_tutum == b->son_tutum) &&
             (a->son_yon == b->son_yon) && (a->son_yatay == b->son_yatay) &&
             (a->cokme == b->cokme) && (a->t_var == b->t_var) &&
             (a->t_onceki_ns == b->t_onceki_ns) && (a->acil_mandal == b->acil_mandal);
    for (uint32_t j = 0u; j < 3u; j++) {
        s = s && (bitler(a->i_pqr[j]) == bitler(b->i_pqr[j]));
    }
    for (uint32_t i = 0u; i < IDA_PORT_N_THR; i++) {
        s = s && (bitler(a->f[i]) == bitler(b->f[i]));
    }
    for (uint32_t j = 0u; j < IDA_TAH_EKSEN; j++) {
        s = s && (a->doyma[j] == b->doyma[j]);
    }
    return s;
}

static bool cikti_bit_esit(const ida_kon_t *a, const ida_kon_t *b)
{
    bool s = true;
    for (uint32_t i = 0u; i < IDA_PORT_N_THR; i++) {
        s = s && (bitler(a->thr[i]) == bitler(b->thr[i]));
    }
    return s;
}

/* Rampanın tanımı (kontrol.h kural 3): çarpan = Δt / T, Δt ≥ T ise 1; Δt kalkıştan önceki son
   adımdan beri geçen süredir (kalkış adımında P, k. adımda (k + 1) · P). Rampa içinde çarpan
   1 − 2⁻²² ile küçültülür (yuvarlama payı: zarf gerçek sayılarda da aşılmaz). */
#define RAMPA_PAY 0.99999976f
static float oran_hesapla(int64_t gecen_ns, int64_t sure_ns)
{
    if (gecen_ns >= sure_ns) {
        return 1.0f;
    }
    if (gecen_ns <= 0) {
        return 0.0f;
    }
    return ((float)gecen_ns / (float)sure_ns) * RAMPA_PAY;
}

/* Zarfın gerçek sayılardaki karşılığı (double; mimarın tanımı): |çıktı| ≤ min(1, Δt / T) ·
   |rampasız|. Uygulamanın float hesabından bağımsızdır. */
static bool zarf_icinde(float cikti, float rampasiz, int64_t gecen_ns, int64_t sure_ns)
{
    double carpan = 1.0;
    if (gecen_ns < sure_ns) {
        carpan = (gecen_ns <= 0) ? 0.0 : ((double)gecen_ns / (double)sure_ns);
    }
    const double c = (double)mutlak(cikti);
    return c <= (carpan * (double)mutlak(rampasiz));
}

/* Rampalı çıktının beklenen değeri: rampasız · çarpan (son komut düzeyinde ölçek). */
static float rampali(float olagan, float oran)
{
    if (!(oran < 1.0f)) {
        return olagan;
    }
    if (!(oran > 0.0f)) {
        return 0.0f;
    }
    return olagan * oran;
}

static void calis(ida_kon_t *k, const ida_prm_kume_t *p, int64_t *t, uint32_t n, uint32_t tur)
{
    ida_kon_girdi_t g;
    for (uint32_t i = 0u; i < n; i++) {
        *t += ADIM_NS;
        etkin_girdi(&g, *t, tur);
        ida_kon_hesapla(k, p, &g);
    }
}

/* Parametre: ad, kimlik, tip, sınıf, aralık, varsayılan (görev belgesi; ad notu
   kontrol_param.def'te). */
static void test_parametre_tanimi(void)
{
    uint16_t id = 0u;
    TEST_ASSERT_EQUAL(IDA_RC_OK, ida_core_param_find("KON_ACIL_RAMPA", &id));
    TEST_ASSERT_EQUAL_HEX16(0x053Bu, id);
    uint16_t indis = 0u;
    TEST_ASSERT_TRUE(ida_prm_indis(id, &indis));
    TEST_ASSERT_EQUAL_UINT16((uint16_t)IDA_PRM_kon_acil_rampa_ms, indis);
    const ida_param_desc_t *d = ida_prm_tanim(indis);
    TEST_ASSERT_NOT_NULL(d);
    TEST_ASSERT_EQUAL_UINT8(IDA_PT_U32, d->type);
    TEST_ASSERT_EQUAL_UINT8(IDA_PCLS_B, d->cls);
    TEST_ASSERT_EQUAL_UINT32(0u, d->min.v.u32);
    TEST_ASSERT_EQUAL_UINT32(5000u, d->max.v.u32);
    TEST_ASSERT_EQUAL_UINT32(RAMPA_MS, d->def.v.u32);
    TEST_ASSERT_EQUAL_UINT32(RAMPA_MS, P.ham[IDA_PRM_kon_acil_rampa_ms]);
}

/* Kural 1 (kontrolcü katmanı): her kanal seçiminde, itki yürürlükteyken gelen acil durdurma
   adımının çıktısı ve süren her adım tam sıfır; silah ve kip girdisi değişmeden. */
static void test_k1_hesapla_ayni_adimda_tam_sifir(void)
{
    for (uint32_t tur = 0u; tur < TUR_SAYISI; tur++) {
        ida_kon_kur(&K1);
        int64_t t = 1000 * DS_MS;
        calis(&K1, &P, &t, 60u, tur);
        TEST_ASSERT_TRUE_MESSAGE(itki_var(&K1), "önce itki yok");
        ida_kon_girdi_t g;
        for (uint32_t i = 0u; i < 50u; i++) {
            t += ADIM_NS;
            etkin_girdi(&g, t, tur);
            g.acil = true;
            ida_kon_hesapla(&K1, &P, &g);
            TEST_ASSERT_TRUE(itki_hepsi_sifir(&K1));
            TEST_ASSERT_TRUE(durum_sifir(&K1));
            TEST_ASSERT_EQUAL_UINT8(1u, K1.acil_mandal);
            TEST_ASSERT_EQUAL_UINT8(0u, K1.cokme);
            TEST_ASSERT_TRUE(ida_kon_tutarli(&K1));
        }
    }
}

/* Kural 1 (çıktı kapısı): kapı kontrolcünün durumuna bakmaz. Kontrolcü itki önerirken etkin
   değer kısıtlayansa, tanımsız bit taşıyorsa ya da mandal kuruluysa (tutarsız değeri dahil)
   çıktı tam sıfır; hiçbiri yoksa ida_kon_itki'nin değeri. */
static void test_k1_cikti_kapisi_kontrolcuden_bagimsiz(void)
{
    int64_t t = 1000 * DS_MS;
    calis(&K1, &P, &t, 60u, 0u);
    TEST_ASSERT_TRUE(itki_var(&K1));
    TEST_ASSERT_FALSE(itki_hepsi_sifir(&K1));

    E.kisitli = 0u; /* gevşek */
    TEST_ASSERT_FALSE(ida_kon_acil_kisitli(&E));
    float thr[IDA_PORT_N_THR];
    bool cokme = true;
    ida_kon_cikti(&K1, &E, thr, &cokme);
    TEST_ASSERT_FALSE(cokme);
    for (uint32_t i = 0u; i < IDA_PORT_N_THR; i++) {
        TEST_ASSERT_EQUAL_HEX32(bitler(K1.thr[i]), bitler(thr[i]));
    }
    /* Öteki emniyet alanları (fiş, pim) itkiyi kesmez. */
    E.kisitli = (uint8_t)(IDA_EMN_FIS_T0 | IDA_EMN_FIS_T1 | IDA_EMN_PIM_T0 | IDA_EMN_PIM_T1);
    TEST_ASSERT_FALSE(ida_kon_acil_kisitli(&E));
    TEST_ASSERT_FALSE(cikti_hepsi_sifir(&K1, &E));

    E.kisitli = (uint8_t)IDA_EMN_ESTOP;
    TEST_ASSERT_TRUE(ida_kon_acil_kisitli(&E));
    TEST_ASSERT_TRUE(cikti_hepsi_sifir(&K1, &E));
    E.kisitli = (uint8_t)IDA_EMN_TUMU; /* init değeri */
    TEST_ASSERT_TRUE(cikti_hepsi_sifir(&K1, &E));
    /* Kuşkuda kısıtlayan: tanımsız bit (bozuk değer). */
    E.kisitli = 0x80u;
    TEST_ASSERT_TRUE(ida_kon_acil_kisitli(&E));
    TEST_ASSERT_TRUE(cikti_hepsi_sifir(&K1, &E));
    E.kisitli = 0x20u;
    TEST_ASSERT_TRUE(cikti_hepsi_sifir(&K1, &E));

    /* Mandal: etkin değer gevşek okunsa da çıktı nötr (rampa henüz başlamadı). */
    E.kisitli = 0u;
    K1.acil_mandal = 1u;
    TEST_ASSERT_TRUE(cikti_hepsi_sifir(&K1, &E));
    K1.acil_mandal = 0xA5u; /* tutarsız mandal: kurulu sayılır */
    TEST_ASSERT_FALSE(ida_kon_tutarli(&K1));
    TEST_ASSERT_TRUE(cikti_hepsi_sifir(&K1, &E));
    K1.acil_mandal = 0u;
    TEST_ASSERT_FALSE(cikti_hepsi_sifir(&K1, &E));
}

/* Kural 2: 60 s acil durdurma boyunca büyük hata altında durum sıfır kalır; kalkıştan sonraki
   çıktı ve iç durum, aynı süreyi acil durdurmasız ve silahsız geçirip aynı adımda etkinleşen
   (sıfırdan başlayan) bir kontrolcününkiyle bit bit aynıdır (rampasız karşılaştırma). */
static void test_k2_uzun_acilde_birikim_yok_ilk_cikti_esit(void)
{
    for (uint32_t tur = 0u; tur < TUR_SAYISI; tur++) {
        ida_kon_kur(&K1);
        ida_kon_kur(&K2);
        int64_t t = 1000 * DS_MS;
        /* K1 önce 2 s etkin: integraller ve referanslar birikir. K2 aynı sürede silahsız. */
        ida_kon_girdi_t g;
        for (uint32_t i = 0u; i < 400u; i++) {
            t += ADIM_NS;
            etkin_girdi(&g, t, tur);
            ida_kon_hesapla(&K1, &P0, &g);
            g.armed = false;
            ida_kon_hesapla(&K2, &P0, &g);
        }
        TEST_ASSERT_TRUE(itki_var(&K1));
        TEST_ASSERT_FALSE(durum_sifir(&K1));
        for (uint32_t i = 0u; i < 12000u; i++) { /* 60 s */
            t += ADIM_NS;
            etkin_girdi(&g, t, tur);
            g.acil = true;
            ida_kon_hesapla(&K1, &P0, &g);
            TEST_ASSERT_TRUE(durum_sifir(&K1));
            g.acil = false;
            g.armed = false;
            ida_kon_hesapla(&K2, &P0, &g);
        }
        /* Kalkış: ikisi de aynı girdiyle etkin. */
        for (uint32_t i = 0u; i < 400u; i++) {
            t += ADIM_NS;
            etkin_girdi(&g, t, tur);
            ida_kon_hesapla(&K1, &P0, &g);
            ida_kon_hesapla(&K2, &P0, &g);
            TEST_ASSERT_TRUE_MESSAGE(cikti_bit_esit(&K1, &K2), "kalkıştan sonra çıktı farklı");
            TEST_ASSERT_TRUE_MESSAGE(ic_durum_esit(&K1, &K2), "kalkıştan sonra iç durum farklı");
            if (i == 0u) {
                TEST_ASSERT_TRUE(itki_var(&K1) || (tur == 0u) || (tur == 2u) || (tur == 3u));
            }
        }
        TEST_ASSERT_TRUE(itki_var(&K1));
    }
}

/* Kural 2 (her çağrının kancası): step OK olmayan çağrılarda da durum sıfırlanır ve mandal
   kurulur; gevşek değerde kanca hiçbir şeye dokunmaz; tutarsız durumda yalnız mandal kurulur
   (bozulmanın kanıtı iç tutarlılık denetimine kalır, A4-K89). */
static void test_k2_izle_kancasi(void)
{
    int64_t t = 1000 * DS_MS;
    calis(&K1, &P, &t, 200u, 0u);
    TEST_ASSERT_FALSE(durum_sifir(&K1));
    K2 = K1;
    E.kisitli = 0u;
    ida_kon_acil_izle(&K1, &E);
    TEST_ASSERT_EQUAL_MEMORY(&K2, &K1, sizeof(K1));

    E.kisitli = (uint8_t)IDA_EMN_ESTOP;
    ida_kon_acil_izle(&K1, &E);
    TEST_ASSERT_TRUE(durum_sifir(&K1));
    TEST_ASSERT_EQUAL_UINT8(1u, K1.acil_mandal);
    TEST_ASSERT_EQUAL_UINT8(0u, K1.rampa_var);
    /* Adım zamanı izi korunur: kalkıştaki ilk dt olağandır. */
    TEST_ASSERT_EQUAL_UINT8(K2.t_var, K1.t_var);
    TEST_ASSERT_TRUE(K1.t_onceki_ns == K2.t_onceki_ns);
    TEST_ASSERT_TRUE(ida_kon_tutarli(&K1));

    /* Tutarsız durum: son_kip tanımsız. */
    K1 = K2;
    K1.son_kip = 9u;
    TEST_ASSERT_FALSE(ida_kon_tutarli(&K1));
    ida_kon_acil_izle(&K1, &E);
    TEST_ASSERT_EQUAL_UINT8(9u, K1.son_kip);
    TEST_ASSERT_FALSE(ida_kon_tutarli(&K1));
    TEST_ASSERT_EQUAL_UINT8(1u, K1.acil_mandal);
    E.kisitli = 0u;
    TEST_ASSERT_TRUE(cikti_hepsi_sifir(&K1, &E)); /* mandal kurulu: çıktı nötr */
}

/* Kural 3: rampa zarfı. K1 (T = 500 ms) ve K2 (T = 0) aynı girdileri görür; kalkış adımı
   j = 0'dır ve j. adımda K1'in çıktısı K2'ninkinin (rampasız çıktının) çarpan_j =
   min(1, (j + 1) · 5 ms / T) katıdır; hiçbir adımda zarfı aşmaz, rampa bitince bit bit
   K2'ninkidir; iç durum baştan sona aynıdır (rampa kontrolcü durumunu etkilemez). */
static void rampa_zarfi(uint32_t tur, uint32_t rampa_ms, uint32_t izlenen)
{
    ida_kon_kur(&K1);
    ida_kon_kur(&K2);
    P.ham[IDA_PRM_kon_acil_rampa_ms] = rampa_ms;
    const int64_t sure = (int64_t)rampa_ms * DS_MS;
    int64_t t = 1000 * DS_MS;
    ida_kon_girdi_t g;
    for (uint32_t i = 0u; i < 150u; i++) {
        t += ADIM_NS;
        etkin_girdi(&g, t, tur);
        g.acil = i >= 100u; /* 0,5 s etkin, 0,25 s acil durdurma */
        ida_kon_hesapla(&K1, &P, &g);
        ida_kon_hesapla(&K2, &P0, &g);
    }
    TEST_ASSERT_TRUE(durum_sifir(&K1));
    TEST_ASSERT_TRUE(durum_sifir(&K2));
    uint32_t rampada_itki = 0u;
    const int64_t t0 = t; /* kalkıştan önceki son (nötr) adım */
    for (uint32_t j = 0u; j < izlenen; j++) {
        t += ADIM_NS;
        etkin_girdi(&g, t, tur);
        ida_kon_hesapla(&K1, &P, &g);
        ida_kon_hesapla(&K2, &P0, &g);
        const float oran = oran_hesapla(t - t0, sure);
        float en_buyuk = 0.0f;
        for (uint32_t i = 0u; i < IDA_PORT_N_THR; i++) {
            const float cikti = K1.thr[i];
            const float olagan = K2.thr[i];
            /* Zarf: |çıktı| ≤ çarpan · |rampasız| ≤ çarpan. */
            TEST_ASSERT_TRUE_MESSAGE(mutlak(cikti) <= (oran * mutlak(olagan)), "zarf aşıldı");
            TEST_ASSERT_TRUE(mutlak(cikti) <= oran);
            TEST_ASSERT_TRUE_MESSAGE(zarf_icinde(cikti, olagan, t - t0, sure),
                                     "zarf gerçek sayı aritmetiğinde aşıldı");
            TEST_ASSERT_EQUAL_HEX32(bitler(rampali(olagan, oran)), bitler(cikti));
            if (mutlak(cikti) > en_buyuk) {
                en_buyuk = mutlak(cikti);
            }
        }
        TEST_ASSERT_TRUE(ic_durum_esit(&K1, &K2));
        if (j == 0u) {
            /* Kalkış adımı: çarpan P / T (T ≤ P ise rampa bu adımda biter). */
            TEST_ASSERT_TRUE(oran == oran_hesapla(ADIM_NS, sure));
            if (sure > ADIM_NS) {
                /* Çarpan P / T'den yuvarlama payı kadar küçük, ondan fazla değil. */
                const float tam = (float)ADIM_NS / (float)sure;
                TEST_ASSERT_TRUE((oran <= tam) && (oran >= (tam * 0.9999995f)));
            }
            TEST_ASSERT_EQUAL_UINT8((sure > ADIM_NS) ? 1u : 0u, K1.rampa_var);
            TEST_ASSERT_EQUAL_UINT8(0u, K1.acil_mandal);
        }
        if ((t - t0) >= sure) {
            TEST_ASSERT_TRUE_MESSAGE(cikti_bit_esit(&K1, &K2), "rampa bitti ama çıktı olağan değil");
            TEST_ASSERT_EQUAL_UINT8(0u, K1.rampa_var);
        } else if (en_buyuk > 0.0f) {
            rampada_itki++;
        } else {
            /* rampasız çıktının da 0 olduğu adım */
        }
    }
    TEST_ASSERT_TRUE(itki_var(&K2));
    if (rampa_ms >= 100u) {
        TEST_ASSERT_TRUE_MESSAGE(rampada_itki > 5u, "rampa boyunca çıktı hiç açılmadı");
    }
    P.ham[IDA_PRM_kon_acil_rampa_ms] = RAMPA_MS;
}

static void test_k3_rampa_zarfi_varsayilan(void)
{
    for (uint32_t tur = 0u; tur < TUR_SAYISI; tur++) {
        rampa_zarfi(tur, RAMPA_MS, 300u);
    }
}

/* Aralığın uçları: 0 (çarpan her adımda 1: kalkış adımında rampasız çıktı), 1 ve 5 ms (bir adım
   ya da daha kısa: rampa kalkış adımında biter), 5 000 ms. */
static void test_k3_rampa_sureleri(void)
{
    rampa_zarfi(0u, 0u, 50u);
    rampa_zarfi(1u, 0u, 50u);
    rampa_zarfi(0u, 5u, 50u);
    rampa_zarfi(1u, 1u, 50u);
    rampa_zarfi(1u, 5000u, 1100u);
    rampa_zarfi(0u, 5000u, 1100u);
}

/* T = 0: kalkış adımında çıktı eş kontrolcüyle aynı ve sıfırdan farklı (doğrudan kuvvet). */
static void test_k3_rampa_sifirken_ilk_adim_olagan(void)
{
    int64_t t = 1000 * DS_MS;
    ida_kon_girdi_t g;
    for (uint32_t i = 0u; i < 20u; i++) {
        t += ADIM_NS;
        etkin_girdi(&g, t, 1u);
        g.acil = true;
        ida_kon_hesapla(&K1, &P0, &g);
    }
    for (uint32_t i = 0u; i < 20u; i++) {
        t += ADIM_NS;
        etkin_girdi(&g, t, 1u);
        ida_kon_hesapla(&K1, &P0, &g);
        TEST_ASSERT_EQUAL_UINT8(0u, K1.rampa_var);
    }
    TEST_ASSERT_TRUE(itki_var(&K1));
}

/* Kural 3: rampa sırasında yeniden acil durdurma aynı adımda nötr; yeni kalkışta rampa baştan. */
static void test_k3_rampa_sirasinda_yeniden_acil(void)
{
    int64_t t = 1000 * DS_MS;
    ida_kon_girdi_t g;
    calis(&K1, &P, &t, 100u, 1u);
    t += ADIM_NS;
    etkin_girdi(&g, t, 1u);
    g.acil = true;
    ida_kon_hesapla(&K1, &P, &g);
    calis(&K1, &P, &t, 61u, 1u); /* kalkış + 60 adım: çarpan 0,61 */
    TEST_ASSERT_EQUAL_UINT8(1u, K1.rampa_var);
    TEST_ASSERT_TRUE(itki_var(&K1));
    t += ADIM_NS;
    etkin_girdi(&g, t, 1u);
    g.acil = true;
    ida_kon_hesapla(&K1, &P, &g);
    TEST_ASSERT_TRUE(itki_hepsi_sifir(&K1));
    TEST_ASSERT_TRUE(durum_sifir(&K1));
    TEST_ASSERT_EQUAL_UINT8(0u, K1.rampa_var);
    TEST_ASSERT_EQUAL_UINT8(1u, K1.acil_mandal);
    /* Yeni kalkış: rampa baştan; j. adımda zarf (j + 1) · 5 ms / T. */
    for (uint32_t j = 0u; j < 100u; j++) {
        t += ADIM_NS;
        etkin_girdi(&g, t, 1u);
        ida_kon_hesapla(&K1, &P, &g);
        const float oran = oran_hesapla((int64_t)(j + 1u) * ADIM_NS, RAMPA_NS);
        for (uint32_t i = 0u; i < IDA_PORT_N_THR; i++) {
            TEST_ASSERT_TRUE(mutlak(K1.thr[i]) <= oran);
        }
    }
}

/* Rampa zamanla ilerler: kalkıştan sonra T boyunca etkin olmayan (silahsız) kontrolcü
   etkinleşince rampasız çalışır; T dolmadan etkinleşen kalan rampayı görür. */
static void test_k3_rampa_zamanla_ilerler(void)
{
    int64_t t = 1000 * DS_MS;
    ida_kon_girdi_t g;
    for (uint32_t i = 0u; i < 300u; i++) {
        t += ADIM_NS;
        etkin_girdi(&g, t, 1u);
        g.acil = i < 100u;       /* 0,5 s acil durdurma */
        g.armed = false;         /* sonra 1 s silahsız (rampa 0,5 s'de biter) */
        ida_kon_hesapla(&K1, &P, &g);
        ida_kon_hesapla(&K2, &P0, &g);
        TEST_ASSERT_TRUE(itki_hepsi_sifir(&K1));
    }
    TEST_ASSERT_EQUAL_UINT8(0u, K1.rampa_var);
    for (uint32_t i = 0u; i < 50u; i++) {
        t += ADIM_NS;
        etkin_girdi(&g, t, 1u);
        ida_kon_hesapla(&K1, &P, &g);
        ida_kon_hesapla(&K2, &P0, &g);
        TEST_ASSERT_TRUE(cikti_bit_esit(&K1, &K2));
    }
    TEST_ASSERT_TRUE(itki_var(&K1));

    /* T dolmadan (kalkıştan 0,3 s sonra) etkinleşme: çarpan 0,6'dan sürer. */
    ida_kon_kur(&K1);
    ida_kon_kur(&K2);
    int64_t t0 = 0;
    for (uint32_t i = 0u; i < 160u; i++) {
        t += ADIM_NS;
        etkin_girdi(&g, t, 1u);
        g.acil = i < 100u;
        g.armed = false;
        if (i == 99u) {
            t0 = t; /* kalkıştan (i = 100) önceki son adım */
        }
        ida_kon_hesapla(&K1, &P, &g);
        ida_kon_hesapla(&K2, &P0, &g);
    }
    for (uint32_t i = 0u; i < 80u; i++) {
        t += ADIM_NS;
        etkin_girdi(&g, t, 1u);
        ida_kon_hesapla(&K1, &P, &g);
        ida_kon_hesapla(&K2, &P0, &g);
        const float oran = oran_hesapla(t - t0, RAMPA_NS);
        for (uint32_t k = 0u; k < IDA_PORT_N_THR; k++) {
            TEST_ASSERT_EQUAL_HEX32(bitler(rampali(K2.thr[k], oran)),
                                    bitler(K1.thr[k]));
        }
    }
}

/* SESSION_START süren rampayı baştan başlatır (zaman tabanı yenilenir; A4-K72): yeni oturumun
   zamanı eski başlangıçtan küçük olsa da rampa baştan açılır ve T sonra biter; oturum başı adım
   zamanı izini de sildiği için kalkış adımının çarpanı 0'dır (önceki adım yok). Bitmiş rampa ya
   da rampasız durum oturum başında mandal kurmaz. */
static void test_k3_oturum_basi_rampayi_bastan_baslatir(void)
{
    int64_t t = 100000 * DS_MS;
    ida_kon_girdi_t g;
    t += ADIM_NS;
    etkin_girdi(&g, t, 1u);
    g.acil = true;
    ida_kon_hesapla(&K1, &P, &g);
    calis(&K1, &P, &t, 41u, 1u); /* rampanın %40'ı */
    TEST_ASSERT_EQUAL_UINT8(1u, K1.rampa_var);
    ida_kon_oturum_basi(&K1);
    TEST_ASSERT_EQUAL_UINT8(0u, K1.rampa_var);
    TEST_ASSERT_EQUAL_UINT8(1u, K1.acil_mandal);
    t = 1000 * DS_MS; /* yeni oturum: zaman tabanı geride */
    for (uint32_t j = 0u; j < 150u; j++) {
        t += ADIM_NS;
        etkin_girdi(&g, t, 1u);
        ida_kon_hesapla(&K1, &P, &g);
        const float oran = oran_hesapla((int64_t)j * ADIM_NS, RAMPA_NS);
        for (uint32_t i = 0u; i < IDA_PORT_N_THR; i++) {
            TEST_ASSERT_TRUE(mutlak(K1.thr[i]) <= oran);
        }
        if (j == 0u) {
            TEST_ASSERT_TRUE(itki_hepsi_sifir(&K1));
        }
    }
    TEST_ASSERT_EQUAL_UINT8(0u, K1.rampa_var);
    TEST_ASSERT_TRUE(itki_var(&K1));
    /* Rampa bitmişken oturum başı: mandal kurulmaz. */
    ida_kon_oturum_basi(&K1);
    TEST_ASSERT_EQUAL_UINT8(0u, K1.acil_mandal);
    TEST_ASSERT_EQUAL_UINT8(0u, K1.rampa_var);
}

/* Kuşkuda kısıtlayan: rampanın başlangıç izi tutarsızsa (gelecekte ya da negatif) oran 0'dır. */
static void test_k3_tutarsiz_rampa_izi_notr(void)
{
    int64_t t = 1000 * DS_MS;
    ida_kon_girdi_t g;
    t += ADIM_NS;
    etkin_girdi(&g, t, 1u);
    g.acil = true;
    ida_kon_hesapla(&K1, &P, &g);
    calis(&K1, &P, &t, 41u, 1u);
    TEST_ASSERT_TRUE(itki_var(&K1));
    K1.rampa_t0_ns = t + (1000 * DS_MS); /* gelecekte */
    calis(&K1, &P, &t, 1u, 1u);
    TEST_ASSERT_TRUE(itki_hepsi_sifir(&K1));
    K1.rampa_t0_ns = -1;
    calis(&K1, &P, &t, 1u, 1u);
    TEST_ASSERT_TRUE(itki_hepsi_sifir(&K1));
    /* Yeni alanların tutarlılık denetimi (A4-K89). */
    K1.rampa_var = 2u;
    TEST_ASSERT_FALSE(ida_kon_tutarli(&K1));
    K1.rampa_var = 1u;
    TEST_ASSERT_TRUE(ida_kon_tutarli(&K1));
    K1.acil_mandal = 2u;
    TEST_ASSERT_FALSE(ida_kon_tutarli(&K1));
}

/* Özellik testi (D): 200 000 adım, sabit tohum. Rastgele girdi (silah, kısıt, kestirim, kanal
   seçimi, hedefler, durum), rastgele acil durdurma dönemleri, kanca çağrıları, oturum başları
   ve geri giden zaman. Değişmezler: (1) acil durdurma adımında çıktı ve durum sıfır; (2) rampalı
   kontrolcünün çıktısı her adımda eşin (T = 0) çıktısı · rampa modelinin çarpanı; (3) iç durum
   eşle bit bit aynı. */
static float rastgele_aralik(ds_rastgele_t *r, float a, float b)
{
    const float x = (float)(ds_rastgele(r) % 100001u) / 100000.0f;
    return a + ((b - a) * x);
}

static void rastgele_girdi(ds_rastgele_t *r, ida_kon_girdi_t *g, int64_t *t)
{
    const uint32_t z = ds_rastgele(r) % 1000u;
    if (z < 5u) {
        *t -= 2 * DS_MS; /* geri giden zaman */
    } else if (z < 10u) {
        *t += 200 * DS_MS;
    } else {
        *t += (int64_t)(1u + (ds_rastgele(r) % 10u)) * DS_MS;
    }
    etkin_girdi(g, *t, ds_rastgele(r) % TUR_SAYISI);
    g->armed = (ds_rastgele(r) % 20u) != 0u;
    g->kisit = (ds_rastgele(r) % 50u) == 0u;
    g->kestirim_yeterli = (ds_rastgele(r) % 50u) != 0u;
    ida_kon_istek_t *s = &g->istek;
    s->derinlik = rastgele_aralik(r, 0.0f, 40.0f);
    s->yon = rastgele_aralik(r, -3.0f, 3.0f);
    s->hiz[0] = rastgele_aralik(r, -2.0f, 2.0f);
    s->hiz[1] = rastgele_aralik(r, -2.0f, 2.0f);
    for (uint32_t j = 0u; j < 6u; j++) {
        s->kuvvet[j] = rastgele_aralik(r, -60.0f, 60.0f);
    }
    ida_nav_out_t *n = &g->nav;
    n->depth = rastgele_aralik(r, 0.0f, 20.0f);
    for (uint32_t j = 0u; j < 3u; j++) {
        n->v_ned[j] = rastgele_aralik(r, -1.0f, 1.0f);
        n->omega_b[j] = rastgele_aralik(r, -1.0f, 1.0f);
    }
    if ((ds_rastgele(r) % 500u) == 0u) {
        n->depth = ds_nan(); /* sayısal çökme yolu */
    }
}

static void test_k4_ozellik_kontrolcu(void)
{
    ds_rastgele_t r = {0x5EED2026u};
    int64_t t = 1000 * DS_MS;
    ida_kon_girdi_t g;
    bool acil = false;
    /* Rampa modeli (kontrol.h kural 3). */
    bool m_mandal = false;
    bool m_rampa = false;
    int64_t m_t0 = 0;
    uint32_t s_acil = 0u;
    uint32_t s_ihlal = 0u;
    uint32_t s_kalkis = 0u;
    uint32_t s_rampada = 0u;
    uint32_t s_rampada_itki = 0u;
    uint32_t s_olagan_itki = 0u;
    uint32_t s_kanca = 0u;
    uint32_t s_oturum = 0u;
    for (uint32_t n = 0u; n < 200000u; n++) {
        const uint32_t z = ds_rastgele(&r) % 1000u;
        if (acil) {
            acil = z >= 8u; /* ortalama 125 adım */
        } else {
            acil = z < 3u;
        }
        if ((ds_rastgele(&r) % 400u) == 0u) {
            /* Step OK olmayan bir çağrı: kanca etkin değeri görür. */
            E.kisitli = acil ? (uint8_t)IDA_EMN_TUMU : (uint8_t)0u;
            if ((ds_rastgele(&r) % 4u) == 0u) {
                E.kisitli = (uint8_t)IDA_EMN_ESTOP; /* bu çağrıda kısa bir kısıtlayan dönem */
            }
            ida_kon_acil_izle(&K1, &E);
            ida_kon_acil_izle(&K2, &E);
            if (ida_kon_acil_kisitli(&E)) {
                m_mandal = true;
                m_rampa = false;
                TEST_ASSERT_TRUE(durum_sifir(&K1));
            }
            s_kanca++;
        }
        if ((ds_rastgele(&r) % 5000u) == 0u) {
            ida_kon_oturum_basi(&K1);
            ida_kon_oturum_basi(&K2);
            if (m_rampa) {
                m_rampa = false;
                m_mandal = true;
            }
            /* Eşte (T = 0) rampa hiç sürmez: mandal kurulmaz; K1'de kurulabilir. İç durum
               karşılaştırması bu yüzden bir sonraki adıma kadar mandalı ayrı tutar. */
            if (m_mandal && (K2.acil_mandal == 0u)) {
                K2.acil_mandal = 1u; /* eş de aynı adımda sıfırdan başlar (zaten sıfır) */
            }
            s_oturum++;
        }
        rastgele_girdi(&r, &g, &t);
        g.acil = acil;
        /* Kalkış adımının öne alınan aralığı: önceki adımdan beri geçen süre, [0, 50 ms]. */
        int64_t adim_ns = 0;
        if (K1.t_var == 1u) {
            const int64_t d = t - K1.t_onceki_ns;
            if (d >= (50 * DS_MS)) {
                adim_ns = 50 * DS_MS;
            } else if (d > 0) {
                adim_ns = d;
            } else {
                adim_ns = 0;
            }
        }
        ida_kon_hesapla(&K1, &P, &g);
        ida_kon_hesapla(&K2, &P0, &g);
        E.kisitli = acil ? (uint8_t)IDA_EMN_ESTOP : (uint8_t)0u;

        float oran = 1.0f;
        if (acil) {
            m_mandal = true;
            m_rampa = false;
            s_acil++;
            if (!itki_hepsi_sifir(&K1) || !cikti_hepsi_sifir(&K1, &E) || !durum_sifir(&K1)) {
                s_ihlal++;
            }
        } else {
            if (m_mandal) {
                m_mandal = false;
                m_rampa = true;
                m_t0 = t - adim_ns;
                s_kalkis++;
            }
            if (m_rampa) {
                if ((t < m_t0) || (m_t0 < 0)) {
                    oran = 0.0f;
                } else if ((t - m_t0) >= RAMPA_NS) {
                    m_rampa = false;
                } else {
                    oran = oran_hesapla(t - m_t0, RAMPA_NS);
                }
            }
        }
        float thr[IDA_PORT_N_THR];
        bool cokme = false;
        ida_kon_cikti(&K1, &E, thr, &cokme);
        bool itki = false;
        for (uint32_t i = 0u; i < IDA_PORT_N_THR; i++) {
            const float beklenen = acil ? 0.0f : rampali(K2.thr[i], oran);
            if (bitler(thr[i]) != bitler(beklenen)) {
                s_ihlal++;
            }
            if (mutlak(thr[i]) > oran) {
                s_ihlal++;
            }
            if (!acil && m_rampa && (t >= m_t0) &&
                !zarf_icinde(thr[i], K2.thr[i], t - m_t0, RAMPA_NS)) {
                s_ihlal++; /* zarf gerçek sayılarda da aşılmaz */
            }
            itki = itki || (thr[i] != 0.0f);
        }
        if (!ic_durum_esit(&K1, &K2) || !ida_kon_tutarli(&K1)) {
            s_ihlal++;
        }
        if (!acil && (oran < 1.0f)) {
            s_rampada++;
            if (itki) {
                s_rampada_itki++;
            }
        } else if (!acil && itki) {
            s_olagan_itki++;
        } else {
            /* acil durdurma ya da itkisiz adım */
        }
        TEST_ASSERT_EQUAL_UINT32_MESSAGE(0u, s_ihlal, "değişmez ihlali");
    }
    char ileti[320];
    size_t k = 0u;
    ileti[0] = '\0';
    ek(ileti, sizeof(ileti), &k, "kontrolcü, 200000 adım (tohum 0x5EED2026): acil durdurmalı adım ", s_acil);
    ek(ileti, sizeof(ileti), &k, ", ihlal ", s_ihlal);
    ek(ileti, sizeof(ileti), &k, ", kalkış ", s_kalkis);
    ek(ileti, sizeof(ileti), &k, ", rampa içi adım ", s_rampada);
    ek(ileti, sizeof(ileti), &k, " (itkili ", s_rampada_itki);
    ek(ileti, sizeof(ileti), &k, "), rampa dışı itkili adım ", s_olagan_itki);
    ek(ileti, sizeof(ileti), &k, ", kanca çağrısı ", s_kanca);
    ek(ileti, sizeof(ileti), &k, ", oturum başı ", s_oturum);
    TEST_MESSAGE(ileti);
    TEST_ASSERT_TRUE(s_acil > 20000u);
    TEST_ASSERT_TRUE(s_kalkis > 200u);
    TEST_ASSERT_TRUE(s_rampada_itki > 5000u);
    TEST_ASSERT_TRUE(s_olagan_itki > 50000u);
}

/* ============================ (Ç) gerçek çekirdek üzerinden =============================== */

static uint32_t g_seq;
static int64_t g_t;
static uint32_t g_sq[16];
static uint32_t g_id;
static bool g_emn;      /* 10 Hz emniyet örneği verilsin mi                              */
static uint8_t g_estop; /* emniyet örneklerinin taşıdığı acil durdurma değeri            */

static ida_sample_t *ornek(uint8_t tur, uint32_t pencere)
{
    ida_sample_t *s = &IN.samples[IN.n_samples++];
    (void)memset(s, 0, sizeof(*s));
    s->hdr.t_meas.ns = g_t;
    s->hdr.t_rx.ns = g_t;
    s->hdr.seq = ++g_sq[tur];
    s->hdr.window_ns = pencere;
    s->hdr.flags = (uint16_t)IDA_HF_VALID;
    s->hdr.type = tur;
    return s;
}

/* Girdiye bir emniyet örneği ekler (inst ve geçerlilik seçilebilir). */
static ida_sample_t *emniyet_ekle(uint8_t estop, uint8_t inst, bool gecerli)
{
    ida_sample_t *s = &IN.samples[IN.n_samples++];
    ds_emniyet(s, g_t, ++g_sq[IDA_SENS_SAFETY]);
    s->hdr.inst = inst;
    s->u.safety.estop = estop;
    if (!gecerli) {
        s->hdr.flags = 0u; /* IDA_HF_VALID yok: A4-K33 ile reddedilir, kısıtlayıcı içerik uygulanır */
    }
    return s;
}

/* Duyargalar ve canlılık (zamanı ilerletmez). */
static void duyargalar(void)
{
    if ((g_seq % 2u) == 0u) {
        ida_sample_t *s = ornek((uint8_t)IDA_SENS_MAG, 0u);
        s->u.mag.field[0] = 25.6e-6f;
        s->u.mag.field[1] = 2.7e-6f;
        s->u.mag.field[2] = 40.3e-6f;
    }
    if ((g_seq % 8u) == 0u) {
        ida_sample_t *s = ornek((uint8_t)IDA_SENS_BARO, 18750000u);
        s->u.baro.p_abs = 111132.0f; /* sabit; BARO_KAL_D = 1 m ile kalibre edilir */
        s->u.baro.temp_c = 20.0f;
    }
    if ((g_seq % 20u) == 0u) {
        ida_sample_t *s = ornek((uint8_t)IDA_SENS_BATT, 100000000u);
        s->u.batt.voltage = 15.6f;
        s->u.batt.current = 1.0f;
    }
    if ((g_seq % 20u) == 10u) {
        (void)ornek((uint8_t)IDA_SENS_LEAK, 0u);
    }
    if (g_emn && ((g_seq % 20u) == 5u)) {
        (void)emniyet_ekle(g_estop, 0u, true);
    }
    if ((g_seq % 200u) == 100u) {
        ida_cmd_t *k = &IN.cmds[IN.n_cmds++];
        (void)memset(k, 0, sizeof(*k));
        k->cmd_id = ++g_id;
        k->type = (uint8_t)IDA_CMD_HEARTBEAT;
    }
}

static void girdi_kur(bool step)
{
    g_seq++;
    g_t += ADIM_NS;
    ds_girdi(&IN, g_t, g_seq, step);
    if (step) {
        IN.imu.imu.accel[2] = -9.80665f;
    }
    duyargalar();
}

static ida_rc_t cagir(void)
{
    const ida_rc_t rc = ida_core_step(&C, &IN, &OUT);
    ds_cikti_degismezleri(rc, &IN, &OUT);
    return rc;
}

static void adim(void)
{
    girdi_kur(true);
    TEST_ASSERT_EQUAL(IDA_RC_OK, cagir());
}

static void adimlar(uint32_t n)
{
    for (uint32_t i = 0u; i < n; i++) {
        adim();
    }
}

static ida_cmd_t *komut_ekle(uint8_t tur)
{
    ida_cmd_t *k = &IN.cmds[IN.n_cmds++];
    (void)memset(k, 0, sizeof(*k));
    k->cmd_id = ++g_id;
    k->type = tur;
    return k;
}

static uint8_t komutla(uint8_t tur, uint8_t kip)
{
    girdi_kur(true);
    ida_cmd_t *k = komut_ekle(tur);
    if (tur == (uint8_t)IDA_CMD_SET_MODE) {
        k->u.set_mode.mode = kip;
    }
    const uint32_t id = k->cmd_id;
    TEST_ASSERT_EQUAL(IDA_RC_OK, cagir());
    const ida_cmd_result_t *r = ds_sonuc(&OUT, id);
    TEST_ASSERT_NOT_NULL(r);
    return r->ack;
}

static void hedef_ekle(float derinlik, float yon)
{
    ida_cmd_t *k = komut_ekle((uint8_t)IDA_CMD_GUIDED_SP);
    k->u.guided.mask = (uint8_t)(IDA_SP_DEPTH | IDA_SP_YAW);
    k->u.guided.depth = derinlik;
    k->u.guided.yaw = yon;
}

static bool cikti_notr(void)
{
    bool s = true;
    for (uint32_t i = 0u; i < IDA_PORT_N_THR; i++) {
        s = s && (bitler(OUT.thr_cmd[i]) == 0u);
    }
    return s;
}

static float cikti_en_buyuk(void)
{
    float m = 0.0f;
    for (uint32_t i = 0u; i < IDA_PORT_N_THR; i++) {
        const float a = mutlak(OUT.thr_cmd[i]);
        if (a > m) {
            m = a;
        }
    }
    return m;
}

static bool estop_engeli(void)
{
    return (OUT.fc.inhibit_mask & IDA_FCI_ESTOP) != 0u;
}

/* n adım boyunca en büyük |komut|. */
static float en_buyuk_itki(uint32_t n)
{
    float m = 0.0f;
    for (uint32_t i = 0u; i < n; i++) {
        adim();
        const float a = cikti_en_buyuk();
        if (a > m) {
            m = a;
        }
    }
    return m;
}

/* Hizalanmış, silahlı, GUIDED. emniyet = false: hiç emniyet örneği verilmez (etkin acil
   durdurma init'ten beri kısıtlayan). */
static void silahli_guided(bool emniyet)
{
    TEST_ASSERT_EQUAL(IDA_RC_OK, ds_kur(&C));
    g_seq = 0u;
    g_t = 2000 * DS_MS;
    (void)memset(g_sq, 0, sizeof(g_sq));
    g_id = 0u;
    g_emn = emniyet;
    g_estop = 0u;
    /* Beyaz kutu: basınç kalibrasyonu 1 m'de (test_gudum_gecis.c ile aynı). */
    const float bir = 1.0f;
    (void)memcpy(&C.prm.yururluk.ham[IDA_PRM_baro_kal_d], &bir, sizeof(bir));
    adimlar(600u); /* hizalama ve ölçüm ufku */
    TEST_ASSERT_FLOAT_WITHIN(0.05f, 1.0f, OUT.nav.depth);
    TEST_ASSERT_EQUAL_UINT8(IDA_ACK_ACCEPTED, komutla((uint8_t)IDA_CMD_ARM, 0u));
    TEST_ASSERT_EQUAL_UINT8(IDA_ACK_ACCEPTED,
                            komutla((uint8_t)IDA_CMD_SET_MODE, (uint8_t)IDA_MODE_GUIDED));
    adimlar(400u);
    girdi_kur(true);
    hedef_ekle(2.0f, 0.0f);
    TEST_ASSERT_EQUAL(IDA_RC_OK, cagir());
}

/* Silahlı, GUIDED, 2 m hedefi uygulanıyor: nötr dışı komut yürürlükte. */
static void itki_yururlukte(void)
{
    silahli_guided(true);
    TEST_ASSERT_TRUE(en_buyuk_itki(800u) > 0.3f);
    TEST_ASSERT_FALSE(estop_engeli());
    TEST_ASSERT_TRUE(cikti_en_buyuk() > 0.1f); /* son adımda da itki var */
}

/* Kalkış çağrısı (gevşek örneğin kabul edildiği step; k = 0): etkin değer gevşer, silah durmaz,
   çıktı çarpan_0 = (t − t_onceki) / T zarfındadır (t_onceki: kalkıştan önceki son step OK). */
static void kalkis_cagrisi(int64_t t_onceki)
{
    g_estop = 0u; /* girdi_kur'un 10 Hz örneği de gevşek olsun (aynı çağrıda kısıtlayan içerik yok) */
    g_emn = true;
    girdi_kur(true);
    (void)emniyet_ekle(0u, 0u, true);
    TEST_ASSERT_EQUAL(IDA_RC_OK, cagir());
    TEST_ASSERT_FALSE(estop_engeli());
    TEST_ASSERT_TRUE(OUT.armed);
    TEST_ASSERT_EQUAL_UINT8(IDA_MODE_GUIDED, OUT.status.mode);
    const float oran = oran_hesapla(g_t - t_onceki, RAMPA_NS);
    TEST_ASSERT_TRUE(oran <= 0.021f); /* en çok iki adım aralığı */
    TEST_ASSERT_TRUE_MESSAGE(cikti_en_buyuk() <= oran, "kalkış çağrısında çıktı rampa zarfını aştı");
}

/* Kalkıştan sonra n adım: zarf |komut| ≤ (t − t_onceki) / T (kalkış çağrısı k = 0 iken k. çağrıda
   (k + 1) · P / T); rampa içinde çıktının açıldığı, rampadan sonra itkinin döndüğü görülür. */
static void kalkis_zarfi(int64_t t_onceki, uint32_t n)
{
    uint32_t rampada_itki = 0u;
    float sonra = 0.0f;
    for (uint32_t i = 0u; i < n; i++) {
        adim();
        TEST_ASSERT_FALSE(estop_engeli());
        const float oran = oran_hesapla(g_t - t_onceki, RAMPA_NS);
        const float m = cikti_en_buyuk();
        TEST_ASSERT_TRUE_MESSAGE(m <= oran, "kalkıştan sonra çıktı rampa zarfını aştı");
        if ((oran < 1.0f) && (m > 0.0f)) {
            rampada_itki++;
        }
        if (!(oran < 1.0f) && (m > sonra)) {
            sonra = m;
        }
    }
    TEST_ASSERT_TRUE_MESSAGE(rampada_itki > 5u, "rampa içinde çıktı hiç açılmadı");
    TEST_ASSERT_TRUE_MESSAGE(sonra > 0.1f, "rampadan sonra itki dönmedi");
}

/* Kabul 1 ve 3: nötr dışı komut yürürlükteyken gelen acil durdurma örneğinin ÇAĞRISINDA çıktı
   nötr; sürdükçe nötr; silah, kip, thr_valid ve is_safe değişmez; kalkış çağrısı ve sonrası
   rampa zarfında. */
static void test_c_ornek_ayni_cagrida_notr_ve_rampa(void)
{
    itki_yururlukte();
    girdi_kur(true);
    (void)emniyet_ekle(1u, 0u, true);
    g_estop = 1u;
    TEST_ASSERT_EQUAL(IDA_RC_OK, cagir());
    TEST_ASSERT_TRUE(estop_engeli());
    TEST_ASSERT_TRUE_MESSAGE(cikti_notr(), "acil durdurma örneğinin çağrısında çıktı nötr değil");
    TEST_ASSERT_TRUE(OUT.armed);
    TEST_ASSERT_EQUAL_UINT8(IDA_MODE_GUIDED, OUT.status.mode);
    TEST_ASSERT_TRUE(OUT.thr_valid);
    TEST_ASSERT_FALSE(OUT.is_safe);
    TEST_ASSERT_TRUE(durum_sifir(&C.kon));
    for (uint32_t i = 0u; i < 600u; i++) { /* 3 s */
        adim();
        TEST_ASSERT_TRUE(estop_engeli());
        TEST_ASSERT_TRUE(cikti_notr());
        TEST_ASSERT_TRUE(durum_sifir(&C.kon));
        TEST_ASSERT_TRUE(OUT.armed);
        TEST_ASSERT_EQUAL_UINT8(IDA_MODE_GUIDED, OUT.status.mode);
        TEST_ASSERT_TRUE(OUT.health.core_ok);
    }
    /* Kalkış: gevşek örneğin kabul edildiği çağrı. */
    const int64_t t_onceki = g_t;
    kalkis_cagrisi(t_onceki);
    kalkis_zarfi(t_onceki, 300u);
}

/* Kabul 1: bayatlıkla kısıtlayana dönüş (W5-K4'ün iki ölçütü). (i) emniyet örneği kesilir;
   (ii) örnekler sürer ama ölçüm zamanı ilerlemez. Engel bitinin kurulduğu çağrının çıktısı ve
   sonrası nötr; taze örnekle kalkış rampalı. */
static void bayatlik(bool olcum_zamani_donuk)
{
    itki_yururlukte();
    g_emn = false;
    const int64_t t_meas = g_t;
    bool kisitli = false;
    uint32_t itkili = 0u;
    for (uint32_t i = 0u; (i < 120u) && !kisitli; i++) {
        girdi_kur(true);
        if (olcum_zamani_donuk && ((g_seq % 4u) == 1u)) {
            ida_sample_t *s = emniyet_ekle(0u, 0u, true);
            s->hdr.t_meas.ns = t_meas; /* seq ve alım zamanı yeni, ölçüm zamanı aynı */
        }
        TEST_ASSERT_EQUAL(IDA_RC_OK, cagir());
        kisitli = estop_engeli();
        if (kisitli) {
            TEST_ASSERT_TRUE_MESSAGE(cikti_notr(), "bayatlığın saptandığı çağrıda çıktı nötr değil");
        } else if (cikti_en_buyuk() > 0.0f) {
            itkili++;
        } else {
            /* itkisiz adım */
        }
    }
    TEST_ASSERT_TRUE_MESSAGE(kisitli, "T_emn_bayat içinde bayatlık saptanmadı");
    TEST_ASSERT_TRUE(itkili > 20u); /* bayatlıktan önce itki sürüyordu */
    for (uint32_t i = 0u; i < 200u; i++) {
        girdi_kur(true);
        if (olcum_zamani_donuk && ((g_seq % 4u) == 1u)) {
            ida_sample_t *s = emniyet_ekle(0u, 0u, true);
            s->hdr.t_meas.ns = t_meas;
        }
        TEST_ASSERT_EQUAL(IDA_RC_OK, cagir());
        TEST_ASSERT_TRUE(estop_engeli());
        TEST_ASSERT_TRUE(cikti_notr());
        TEST_ASSERT_TRUE(durum_sifir(&C.kon));
        TEST_ASSERT_TRUE(OUT.armed);
    }
    /* Taze örnek: kabul edildiği çağrıda gevşer (W5-K20 (3)); çıktı rampayla açılır. */
    const int64_t t_onceki = g_t;
    kalkis_cagrisi(t_onceki);
    kalkis_zarfi(t_onceki, 300u);
}

static void test_c_bayatlik_ornek_kesildi(void)
{
    bayatlik(false);
}

static void test_c_bayatlik_olcum_zamani_ilerlemiyor(void)
{
    bayatlik(true);
}

/* Kabul 1 (A4-K116): kısıtlayan örnek step OK olmayan bir çağrıda gelir (0: DUP_SEQ ile
   reddedilen çağrı, 1: out NULL, 2: tick, 3: reddedilen (VALID'siz) örnek, 4: inst = 1 örneği)
   ve hemen sonraki step çağrısında gevşek örnek kabul edilir: kontrol adımı kısıtlayan değeri
   hiç görmez. Kalkış çağrısı yine rampanın başıdır (çarpan ≤ 0,02; rampasız olsaydı > 0,1), durum
   sıfırdan başlar ve sonrası rampa zarfındadır (her çağrının kancası). */
static void adim_disi_kisitlayan(uint32_t yol)
{
    itki_yururlukte();
    int64_t t_onceki = g_t; /* kalkıştan önceki son step OK */
    switch (yol) {
    case 0u:
        ds_girdi(&IN, g_t, g_seq, true); /* aynı tf.seq: DUP_SEQ */
        (void)emniyet_ekle(1u, 0u, true);
        TEST_ASSERT_EQUAL(IDA_RC_DUP_SEQ, cagir());
        TEST_ASSERT_TRUE(estop_engeli());
        TEST_ASSERT_TRUE(cikti_notr());
        TEST_ASSERT_FALSE(OUT.thr_valid);
        break;
    case 1u:
        girdi_kur(true);
        (void)emniyet_ekle(1u, 0u, true);
        TEST_ASSERT_EQUAL(IDA_RC_BAD_INPUT, ida_core_step(&C, &IN, NULL));
        break;
    case 2u:
        girdi_kur(false);
        (void)emniyet_ekle(1u, 0u, true);
        TEST_ASSERT_EQUAL(IDA_RC_OK, ida_core_tick(&C, &IN, &OUT));
        TEST_ASSERT_TRUE(estop_engeli());
        TEST_ASSERT_TRUE(cikti_notr());
        break;
    case 3u:
        girdi_kur(true);
        (void)emniyet_ekle(1u, 0u, false);
        TEST_ASSERT_EQUAL(IDA_RC_OK, cagir());
        TEST_ASSERT_TRUE(estop_engeli());
        TEST_ASSERT_TRUE_MESSAGE(cikti_notr(), "reddedilen örneğin çağrısında çıktı nötr değil");
        t_onceki = g_t;
        break;
    default:
        girdi_kur(true);
        (void)emniyet_ekle(1u, 1u, true);
        TEST_ASSERT_EQUAL(IDA_RC_OK, cagir());
        TEST_ASSERT_TRUE(estop_engeli());
        TEST_ASSERT_TRUE_MESSAGE(cikti_notr(), "inst = 1 örneğinin çağrısında çıktı nötr değil");
        t_onceki = g_t;
        break;
    }
    TEST_ASSERT_TRUE(ida_kon_acil_kisitli(&C.emn));
    TEST_ASSERT_TRUE(durum_sifir(&C.kon));
    /* Hemen sonraki step: gevşek inst = 0 örneği kabul edilir, etkin değer bu çağrıda gevşer. */
    kalkis_cagrisi(t_onceki);
    kalkis_zarfi(t_onceki, 300u);
}

static void test_c_reddedilen_cagrida_kisitlayan_ornek(void)
{
    adim_disi_kisitlayan(0u);
}

static void test_c_out_null_cagrida_kisitlayan_ornek(void)
{
    adim_disi_kisitlayan(1u);
}

static void test_c_tick_cagrisinda_kisitlayan_ornek(void)
{
    adim_disi_kisitlayan(2u);
}

static void test_c_reddedilen_ornekte_kisitlayan_icerik(void)
{
    adim_disi_kisitlayan(3u);
    adim_disi_kisitlayan(4u);
}

/* Kabul 1: init'ten beri hiç gevşememiş etkin değer. Hiç emniyet örneği verilmez; araç
   silahlanır, GUIDED'da hedef alır; çıktı baştan sona nötrdür. İlk gevşek örnekle rampa. */
static void test_c_init_ten_beri_hic_gevsememis(void)
{
    silahli_guided(false);
    for (uint32_t i = 0u; i < 800u; i++) {
        adim();
        TEST_ASSERT_TRUE(estop_engeli());
        TEST_ASSERT_TRUE(cikti_notr());
        TEST_ASSERT_TRUE(durum_sifir(&C.kon));
        TEST_ASSERT_TRUE(OUT.armed);
        TEST_ASSERT_EQUAL_UINT8(IDA_MODE_GUIDED, OUT.status.mode);
    }
    const int64_t t_onceki = g_t;
    kalkis_cagrisi(t_onceki);
    kalkis_zarfi(t_onceki, 300u);
}

/* Kabul 3: rampa sırasında yeniden acil durdurma aynı çağrıda nötr; SESSION_END de (etkin
   değerler kısıtlayana döner, A4-K73). */
static void test_c_rampa_sirasinda_yeniden_acil(void)
{
    itki_yururlukte();
    girdi_kur(true);
    (void)emniyet_ekle(1u, 0u, true);
    g_estop = 1u;
    TEST_ASSERT_EQUAL(IDA_RC_OK, cagir());
    adimlar(40u);
    kalkis_cagrisi(g_t);
    TEST_ASSERT_TRUE(en_buyuk_itki(60u) > 0.0f); /* rampa açılıyor (çarpan 0,61'e kadar) */
    girdi_kur(true);
    (void)emniyet_ekle(1u, 0u, true);
    g_estop = 1u;
    TEST_ASSERT_EQUAL(IDA_RC_OK, cagir());
    TEST_ASSERT_TRUE(estop_engeli());
    TEST_ASSERT_TRUE_MESSAGE(cikti_notr(), "rampa sırasında gelen acil durdurmada çıktı nötr değil");
    TEST_ASSERT_TRUE(durum_sifir(&C.kon));
    adimlar(20u);
    TEST_ASSERT_TRUE(cikti_notr());
    const int64_t t_onceki = g_t;
    kalkis_cagrisi(t_onceki);
    kalkis_zarfi(t_onceki, 300u);
}

/* Özellik testi (Ç; kabul 4): 200 000 çağrı, sabit tohum. Rastgele acil durdurma düğmesi,
   emniyet örneği kesintileri (bayatlık), reddedilen ve inst ≠ 0 örnekler, tick, reddedilen
   çağrı, out NULL çağrı, DISARM, oturum sonu/başı, GUIDED_SP hedefleri. Değişmezler:
   (1) çıktının ESTOP engel biti kuruluyken nötr dışı komut veren çağrı sayısı 0; (2) kabul
   edilen çağrıda kontrol durumu sıfır; (3) kalkış adımı (ESTOP biti düştükten sonraki ilk step
   OK) k = 0 iken |komut| ≤ Δt / T, Δt kalkıştan önceki son step OK'tan beri geçen süre (en çok
   50 ms öne alınır; düzgün aralıkta (k + 1) · P); kalkış adımından önceki çağrılarda komut 0. */
static void test_k4_ozellik_cekirdek(void)
{
    ds_rastgele_t r = {0x5EED2026u};
    silahli_guided(true);
    bool dugme = false;
    uint32_t sessiz = 0u;
    uint32_t oturum_basi_kalan = 0u;
    bool onceki_kisitli = false;
    bool bekliyor = false;    /* kısıtlayan dönem görüldü, kalkış adımı henüz gelmedi */
    bool kalkis_var = false;
    int64_t t_kalkis = 0;     /* rampanın sayıldığı başlangıç */
    int64_t t_son_adim = g_t; /* son step OK çağrısının zamanı */
    uint32_t s_kisitli = 0u;
    uint32_t s_ihlal = 0u;
    uint32_t s_durum_ihlal = 0u;
    uint32_t s_zarf_ihlal = 0u;
    uint32_t s_kalkis = 0u;
    uint32_t s_rampada_itki = 0u;
    uint32_t s_itki = 0u;
    uint32_t s_bayat = 0u;
    uint32_t s_tick = 0u;
    uint32_t s_red = 0u;
    uint32_t s_null = 0u;
    uint32_t s_oturum = 0u;
    for (uint32_t n = 0u; n < 200000u; n++) {
        const uint32_t z = ds_rastgele(&r) % 10000u;
        uint32_t tur = 0u; /* 0 step, 1 tick, 2 DUP_SEQ, 3 out NULL */
        if (z < 200u) {
            tur = 1u;
        } else if (z < 230u) {
            tur = 2u;
        } else if (z < 250u) {
            tur = 3u;
        } else {
            tur = 0u;
        }
        g_emn = sessiz == 0u;
        if (sessiz > 0u) {
            sessiz--;
        }
        if (tur == 2u) {
            ds_girdi(&IN, g_t, g_seq, true); /* yinelenen tf.seq */
        } else {
            girdi_kur(tur != 1u);
        }
        /* Düğme: basılı değilken %0,2 basılır, basılıyken %0,5 bırakılır; değişim anında
           örnek hemen verilir (sim'in değişim örneği gibi). */
        const uint32_t d = ds_rastgele(&r) % 1000u;
        const bool yeni = dugme ? (d >= 5u) : (d < 2u);
        if (yeni != dugme) {
            dugme = yeni;
            g_estop = dugme ? (uint8_t)1u : (uint8_t)0u;
            if (g_emn && (IN.n_samples < 8u)) {
                (void)emniyet_ekle(g_estop, 0u, true);
            }
        }
        const uint32_t e = ds_rastgele(&r) % 10000u;
        if (e < 5u) {
            sessiz = 30u + (ds_rastgele(&r) % 120u); /* 150–750 ms örnek yok */
        } else if ((e < 15u) && (IN.n_samples < 8u)) {
            (void)emniyet_ekle(1u, (uint8_t)(1u + (ds_rastgele(&r) % 3u)), true); /* inst ≠ 0 */
        } else if ((e < 25u) && (IN.n_samples < 8u)) {
            (void)emniyet_ekle(1u, 0u, false); /* reddedilen örnek, kısıtlayıcı içerik */
        } else {
            /* olağan */
        }
        /* Komutlar (yalnız kabul edilecek çağrılarda). */
        if ((tur == 0u) || (tur == 1u)) {
            if (!OUT.armed && ((n % 50u) == 0u)) {
                (void)komut_ekle((uint8_t)IDA_CMD_ARM);
            } else if (OUT.armed && (OUT.status.mode != (uint8_t)IDA_MODE_GUIDED) &&
                       ((n % 50u) == 10u)) {
                komut_ekle((uint8_t)IDA_CMD_SET_MODE)->u.set_mode.mode = (uint8_t)IDA_MODE_GUIDED;
            } else if ((n % 400u) == 200u) {
                hedef_ekle(rastgele_aralik(&r, 1.5f, 3.0f), rastgele_aralik(&r, -3.0f, 3.0f));
            } else {
                /* komut yok */
            }
            const uint32_t k = ds_rastgele(&r) % 10000u;
            if (k < 3u) {
                IN.narrow.mask = (uint8_t)IDA_NARROW_DISARM;
                IN.narrow.cmd_id[0] = ++g_id;
            } else if ((k < 4u) && (oturum_basi_kalan == 0u)) {
                IN.plat.link_event = (uint8_t)IDA_LINKEV_SESSION_END;
                oturum_basi_kalan = 5u + (ds_rastgele(&r) % 20u);
                s_oturum++;
            } else if (oturum_basi_kalan == 1u) {
                IN.plat.link_event = (uint8_t)IDA_LINKEV_SESSION_START;
                oturum_basi_kalan = 0u;
            } else if (oturum_basi_kalan > 1u) {
                oturum_basi_kalan--;
            } else {
                /* bağlantı olayı yok */
            }
        }

        bool kisitli = false;
        bool yazildi = true;
        ida_rc_t rc = IDA_RC_OK;
        if (tur == 3u) {
            rc = ida_core_step(&C, &IN, NULL);
            TEST_ASSERT_EQUAL(IDA_RC_BAD_INPUT, rc);
            yazildi = false;
            kisitli = ida_kon_acil_kisitli(&C.emn); /* çıktı yok: beyaz kutu */
            s_null++;
        } else if (tur == 1u) {
            rc = ida_core_tick(&C, &IN, &OUT);
            ds_cikti_degismezleri(rc, &IN, &OUT);
            TEST_ASSERT_EQUAL(IDA_RC_OK, rc);
            s_tick++;
        } else {
            rc = cagir();
            if (tur == 2u) {
                TEST_ASSERT_EQUAL(IDA_RC_DUP_SEQ, rc);
                s_red++;
            } else {
                TEST_ASSERT_EQUAL(IDA_RC_OK, rc);
            }
        }
        if (yazildi) {
            kisitli = estop_engeli();
            /* Kontrolün okuduğu değer ile çıktının engel biti aynı şeyi söyler. */
            TEST_ASSERT_TRUE(kisitli == ida_kon_acil_kisitli(&C.emn));
            const float m = cikti_en_buyuk();
            if (kisitli) {
                s_kisitli++;
                if (!cikti_notr()) {
                    s_ihlal++;
                }
                if ((rc == IDA_RC_OK) && !durum_sifir(&C.kon)) {
                    s_durum_ihlal++;
                }
                if (!dugme && (sessiz > 0u)) {
                    s_bayat++; /* düğme basılı değil, örnek kesik: bayatlık (ya da gevşetilemeyen
                                  inst ≠ 0 / reddedilen örnek içeriği) */
                }
                bekliyor = true;
            } else {
                if (bekliyor && (tur == 0u)) {
                    int64_t one = g_t - t_son_adim;
                    if (one > (50 * DS_MS)) {
                        one = 50 * DS_MS;
                    }
                    bekliyor = false;
                    kalkis_var = true;
                    t_kalkis = g_t - one;
                    s_kalkis++;
                }
                float oran = 1.0f;
                if (bekliyor) {
                    oran = 0.0f; /* kalkış adımından önce (tick): komut yok */
                } else if (kalkis_var) {
                    oran = oran_hesapla(g_t - t_kalkis, RAMPA_NS);
                } else {
                    oran = 1.0f;
                }
                if (m > oran) {
                    s_zarf_ihlal++;
                }
                if (m > 0.0f) {
                    if (oran < 1.0f) {
                        s_rampada_itki++;
                    } else {
                        s_itki++;
                    }
                }
            }
            TEST_ASSERT_TRUE(OUT.health.core_ok);
        } else if (!kisitli && onceki_kisitli) {
            /* out NULL çağrısında gevşeme olmaz (örnekler reddedilmiş sayılır, A4-K116). */
            TEST_FAIL_MESSAGE("out NULL çağrısında etkin acil durdurma gevşedi");
        } else {
            bekliyor = bekliyor || kisitli; /* out NULL: çıktı yok */
        }
        if ((tur == 0u) && (rc == IDA_RC_OK)) {
            t_son_adim = g_t;
        }
        onceki_kisitli = kisitli;
        TEST_ASSERT_EQUAL_UINT32_MESSAGE(0u, s_ihlal, "acil durdurma kısıtlayanken nötr dışı çıktı");
        TEST_ASSERT_EQUAL_UINT32_MESSAGE(0u, s_durum_ihlal, "acil durdurmada kontrol durumu sıfır değil");
        TEST_ASSERT_EQUAL_UINT32_MESSAGE(0u, s_zarf_ihlal, "kalkıştan sonra rampa zarfı aşıldı");
    }
    char ileti[400];
    size_t k = 0u;
    ileti[0] = '\0';
    ek(ileti, sizeof(ileti), &k, "çekirdek, 200000 çağrı (tohum 0x5EED2026): ESTOP kısıtlayan çağrı ", s_kisitli);
    ek(ileti, sizeof(ileti), &k, " (düğme basılı değil, örnek kesik: ", s_bayat);
    ek(ileti, sizeof(ileti), &k, "), nötr dışı çıktı ", s_ihlal);
    ek(ileti, sizeof(ileti), &k, ", durum ihlali ", s_durum_ihlal);
    ek(ileti, sizeof(ileti), &k, ", zarf ihlali ", s_zarf_ihlal);
    ek(ileti, sizeof(ileti), &k, ", kalkış ", s_kalkis);
    ek(ileti, sizeof(ileti), &k, ", rampa içi itkili çağrı ", s_rampada_itki);
    ek(ileti, sizeof(ileti), &k, ", rampa dışı itkili çağrı ", s_itki);
    ek(ileti, sizeof(ileti), &k, ", tick ", s_tick);
    ek(ileti, sizeof(ileti), &k, ", DUP_SEQ ", s_red);
    ek(ileti, sizeof(ileti), &k, ", out NULL ", s_null);
    ek(ileti, sizeof(ileti), &k, ", oturum sonu ", s_oturum);
    TEST_MESSAGE(ileti);
    /* Değişmezler boş geçmedi. */
    TEST_ASSERT_TRUE(s_kisitli > 20000u);
    TEST_ASSERT_TRUE(s_bayat > 100u);
    TEST_ASSERT_TRUE(s_kalkis > 100u);
    TEST_ASSERT_TRUE(s_rampada_itki > 1000u);
    TEST_ASSERT_TRUE(s_itki > 20000u);
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_parametre_tanimi);
    RUN_TEST(test_k1_hesapla_ayni_adimda_tam_sifir);
    RUN_TEST(test_k1_cikti_kapisi_kontrolcuden_bagimsiz);
    RUN_TEST(test_k2_uzun_acilde_birikim_yok_ilk_cikti_esit);
    RUN_TEST(test_k2_izle_kancasi);
    RUN_TEST(test_k3_rampa_zarfi_varsayilan);
    RUN_TEST(test_k3_rampa_sureleri);
    RUN_TEST(test_k3_rampa_sifirken_ilk_adim_olagan);
    RUN_TEST(test_k3_rampa_sirasinda_yeniden_acil);
    RUN_TEST(test_k3_rampa_zamanla_ilerler);
    RUN_TEST(test_k3_oturum_basi_rampayi_bastan_baslatir);
    RUN_TEST(test_k3_tutarsiz_rampa_izi_notr);
    RUN_TEST(test_k4_ozellik_kontrolcu);
    RUN_TEST(test_c_ornek_ayni_cagrida_notr_ve_rampa);
    RUN_TEST(test_c_bayatlik_ornek_kesildi);
    RUN_TEST(test_c_bayatlik_olcum_zamani_ilerlemiyor);
    RUN_TEST(test_c_reddedilen_cagrida_kisitlayan_ornek);
    RUN_TEST(test_c_out_null_cagrida_kisitlayan_ornek);
    RUN_TEST(test_c_tick_cagrisinda_kisitlayan_ornek);
    RUN_TEST(test_c_reddedilen_ornekte_kisitlayan_icerik);
    RUN_TEST(test_c_init_ten_beri_hic_gevsememis);
    RUN_TEST(test_c_rampa_sirasinda_yeniden_acil);
    RUN_TEST(test_k4_ozellik_cekirdek);
    return UNITY_END();
}
