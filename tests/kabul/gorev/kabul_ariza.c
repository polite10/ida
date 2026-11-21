/*
 * tests/kabul/gorev — görev md.3: arıza eylemleri armed ya da mode'u genişletmez.
 *
 * Her başlangıç durumu (silahsız/silahlı × kip) ve her arıza (görev md.3'ün listesi:
 * sensör kaybı, kestirim bozulması, bağlantı kaybı, düşük batarya, sızıntı; ayrıca IMU
 * kaybı, yer istasyonu kaybı ve oturum sonu) için arıza uygulanır, sürdürülür, kaldırılır
 * ve bir FAILSAFE_RESET gönderilir. Sınamada hiçbir ARM ya da SET_MODE gönderilmez; bu
 * yüzden düzeneğin her çağrıdaki değişmezleri (D1: armed 0 → 1 yalnız kabul edilen ARM ile;
 * D2: GUIDED/AUTO'ya giriş yalnız kabul edilen SET_MODE ya da FAILSAFE_RESET ile; A4-K104)
 * arıza eylemlerinin genişletmediğini her adımda denetler. Ek olarak her vakada silahlanma
 * ve kip, başlangıçtakinden genişlememelidir.
 *
 * Arızaların hangi eylemi üreteceği (02 §4.2 ve §4.3 matrisi) sözleşmede değil, uygulama
 * işindedir; bu dosya eylemin kendisini değil, yalnız yönünü (kısıtlayan) sınar.
 */
#include <stdio.h>
#include <string.h>

#include "kabul_gorev.h"
#include "unity.h"

void setUp(void)
{
}

void tearDown(void)
{
}

typedef enum {
    AR_SIZINTI,
    AR_DUSUK_BATARYA,
    AR_IMU_KAYBI,
    AR_BASINC_KAYBI,
    AR_MANYETOMETRE_KAYBI,
    AR_KONUM_KAYBI,
    AR_KESTIRIM_BOZULMASI,
    AR_BAGLANTI_KAYBI,
    AR_YER_ISTASYONU_KAYBI,
    AR_OTURUM_SONU,
    AR_N
} ariza_t;

static const char *const ARIZA_ADI[AR_N] = {
    "sızıntı",          "düşük batarya",  "IMU kaybı",          "basınç kaybı",       "manyetometre kaybı",
    "GNSS ve DVL kaybı", "kestirim bozulması", "bağlantı kaybı", "yer istasyonu kaybı", "oturum sonu",
};

/* Arıza adı; sınır dışında "?" (MSVC /analyze C6385). */
static const char *ariza_adi(ariza_t a)
{
    int i = (int)a;
    if (i < 0 || (size_t)i >= sizeof ARIZA_ADI / sizeof ARIZA_ADI[0]) {
        return "?";
    }
    return ARIZA_ADI[i];
}

static void ariza_ayarla(ariza_t a, bool aktif)
{
    kg_dunya_t *d = kg_dunya();
    switch (a) {
    case AR_SIZINTI:
        d->sizinti = (uint8_t)(aktif ? 1u : 0u);
        break;
    case AR_DUSUK_BATARYA:
        d->batt_v = aktif ? 3.0f : 16.0f;
        break;
    case AR_IMU_KAYBI:
        d->imu = !aktif;
        break;
    case AR_BASINC_KAYBI:
        d->baro = !aktif;
        break;
    case AR_MANYETOMETRE_KAYBI:
        d->mag = !aktif;
        break;
    case AR_KONUM_KAYBI:
        d->gnss = !aktif;
        d->dvl = !aktif;
        break;
    case AR_KESTIRIM_BOZULMASI:
        d->kestirim_bozuk = aktif;
        break;
    case AR_BAGLANTI_KAYBI:
    case AR_OTURUM_SONU:
        d->link = !aktif;
        break;
    case AR_YER_ISTASYONU_KAYBI:
    case AR_N:
    default:
        d->hb = !aktif;
        break;
    }
}

static bool genis_kip(uint8_t m)
{
    return m == IDA_MODE_GUIDED || m == IDA_MODE_AUTO;
}

typedef struct {
    const char *ad;
    bool silahli;
    uint8_t kip; /* 0: değiştirme */
} baslangic_t;

/* Bir vakayı yürütür; hiçbir genişletici komut göndermez (FAILSAFE_RESET dışında). */
static void vaka(const baslangic_t *b, ariza_t a, uint8_t *fs_azami)
{
    bool armed0 = kg_out()->armed;
    uint8_t kip0 = kg_out()->status.mode;
    /* Süreler düzeneğin seçimidir; eylem gecikmeleri parametredir ve sözleşmede değeri yoktur.
       Batarya ve yer istasyonu kaybında eylemin görülebilmesi için uzun tutulur. */
    int sure = (a == AR_YER_ISTASYONU_KAYBI || a == AR_DUSUK_BATARYA) ? 6000 : 400; /* 30 s ya da 2 s */
    uint8_t fs = 0u;
    bool kip_genis_oldu = false;

    kg_baglam("md3 %s / %s: arıza", b->ad, ariza_adi((ariza_t)a));
    if (a == AR_OTURUM_SONU) {
        (void)kg_cagir(kg_girdi(KG_OTURUM_SONU));
    }
    ariza_ayarla(a, true);
    for (int i = 0; i < sure; ++i) {
        (void)kg_adim();
        const ida_out_t *o = kg_out();
        if (o->fs_level > fs) {
            fs = o->fs_level;
        }
        KG_DOGRULA(!o->armed || armed0, "silahsız başlangıçta arıza sırasında armed = 1 (görev md.3)");
        if (!genis_kip(kip0) && genis_kip(o->status.mode)) {
            kip_genis_oldu = true;
        }
    }
    kg_baglam("md3 %s / %s: kaldırıldı", b->ad, ariza_adi((ariza_t)a));
    ariza_ayarla(a, false);
    for (int i = 0; i < 400; ++i) {
        (void)kg_adim();
        const ida_out_t *o = kg_out();
        KG_DOGRULA(!o->armed || armed0, "silahsız başlangıçta arıza kalktıktan sonra armed = 1 (görev md.3)");
        if (!genis_kip(kip0) && genis_kip(o->status.mode)) {
            kip_genis_oldu = true;
        }
    }
    /* FAILSAFE_RESET (genişletici ama ARM değil): silahlandırmaz (A4-K104). */
    kg_baglam("md3 %s / %s: FAILSAFE_RESET", b->ad, ariza_adi((ariza_t)a));
    ida_in_t *in = kg_girdi(KG_OLAGAN);
    (void)kg_komut(in, (uint8_t)IDA_CMD_FAILSAFE_RESET);
    (void)kg_cagir(in);
    for (int i = 0; i < 200; ++i) {
        (void)kg_adim();
        KG_DOGRULA(!kg_out()->armed || armed0, "FAILSAFE_RESET sonrası silahsız başlangıçta armed = 1 (A4-K104)");
    }
    KG_DOGRULA(!kip_genis_oldu, "kip GUIDED/AUTO'ya genişledi (başlangıç kipi %u; görev md.3)", (unsigned)kip0);
    (void)printf("  md3 %-22s %-22s fs_azami=%u armed %d->%d kip %u->%u\n", b->ad, ariza_adi((ariza_t)a), (unsigned)fs,
                 (int)armed0, (int)kg_out()->armed, (unsigned)kip0, (unsigned)kg_out()->status.mode);
    *fs_azami = fs;
}

/* Görev md.3, A4-K104: silahsız başlangıç (init sonrası kip ne ise), her arıza. Hazır
   olma gerekmez; yer tutucu çekirdekte de anlamlıdır. */
static void test_md3_silahsiz_baslangic(void)
{
    static const baslangic_t b = {"silahsız/başlangıç", false, 0u};
    kg_baglam("md3 silahsız başlangıç");
    kg_kur_varsayilan();
    (void)kg_hazirla(KG_HAZIR_AZAMI);
    kg_anlik_al();
    for (int a = 0; a < (int)AR_N; ++a) {
        uint8_t fs = 0u;
        kg_anlik_don();
        vaka(&b, (ariza_t)a, &fs);
    }
}

/* Görev md.3, A4-K104, K127: silahlı ve silahsız başlangıç × kip (init sonrası ilk kip,
   MANUAL, STABILIZE, DEPTH_HOLD, STATION_KEEP, GUIDED) × her arıza. Hazır olma gerekir (silahlanmak için). */
static void test_md3_kip_ariza_matrisi(void)
{
    static const baslangic_t baslangiclar[] = {
        {"silahsız/ilk kip", false, 0u},
        {"silahlı/ilk kip", true, 0u},
        {"silahlı/MANUAL", true, (uint8_t)IDA_MODE_MANUAL},
        {"silahlı/STABILIZE", true, (uint8_t)IDA_MODE_STABILIZE},
        {"silahlı/DEPTH_HOLD", true, (uint8_t)IDA_MODE_DEPTH_HOLD},
        {"silahlı/STATION_KEEP", true, (uint8_t)IDA_MODE_STATION_KEEP},
        {"silahlı/GUIDED", true, (uint8_t)IDA_MODE_GUIDED},
        {"silahsız/GUIDED", false, (uint8_t)IDA_MODE_GUIDED},
    };
    int n_baslangic = (int)(sizeof baslangiclar / sizeof baslangiclar[0]);
    int eylemli = 0;
    kg_baglam("md3 matris: hazırlık");
    kg_kur_varsayilan();
    kg_hazir_olmali(KG_HAZIR_AZAMI, "arıza matrisi");
    kg_anlik_al();
    for (int s = 0; s < n_baslangic; ++s) {
        const baslangic_t *b = &baslangiclar[s];
        for (int a = 0; a < (int)AR_N; ++a) {
            uint8_t fs = 0u;
            kg_anlik_don();
            kg_baglam("md3 %s / %s: başlangıç", b->ad, ariza_adi((ariza_t)a));
            /* Önce silahlanma (kipten bağımsız; A4-K127), sonra kip seçimi. MANUAL gibi kiplerin
               silahlı olmayı ön koşul sayıp saymadığı uygulamanındır; bu yüzden sıra böyle. */
            if (b->silahli) {
                (void)kg_silahlan();
            }
            uint8_t ilk_kip = kg_out()->status.mode;
            bool konumlu = b->kip == IDA_MODE_STATION_KEEP;
            if (b->kip == IDA_MODE_GUIDED) {
                kg_guided();
            } else if (konumlu) {
                /* W3-K5: yatay geçerlilik bu dilimde yok; hedef kip sabitlenmez. */
                kg_kip_sec_konumlu(b->kip);
            } else if (b->kip != 0u) {
                kg_kip_sec(b->kip);
            }
            kg_adimlar(20);
            uint8_t beklenen_kip = b->kip != 0u ? b->kip : ilk_kip;
            KG_DOGRULA(kg_out()->armed == b->silahli && (konumlu || kg_out()->status.mode == beklenen_kip),
                       "%s: başlangıç durumu arızasız 20 adımda korunmadı (armed %d, kip %u)", b->ad,
                       (int)kg_out()->armed, (unsigned)kg_out()->status.mode);
            vaka(b, (ariza_t)a, &fs);
            if (fs != IDA_FS_NONE) {
                eylemli++;
            }
        }
    }
    (void)printf("  md3 arıza eylemi gözlenen vaka: %d / %d\n", eylemli, n_baslangic * (int)AR_N);
}

/* Görev md.2 ("kısıtlayan her şey aynı adımda genişletenden önce"), 3.17, W3-K3: arızanın
   ilk göründüğü çağrıda gelen ARM, o çağrının sonunda arıza eylemi (HOLD ve üstü) ya da
   sızıntı/batarya nedeni varsa kabul edilmez. Düzeneğin D8 denetimi uygular; burada vakalar kurulur. */
static void test_md3_ayni_adimda_ariza_ve_arm(void)
{
    kg_baglam("md3 aynı adım: hazırlık");
    kg_kur_varsayilan();
    kg_hazir_olmali(KG_HAZIR_AZAMI, "aynı adımda arıza ve ARM");
    kg_anlik_al();

    /* Sızıntı örneği ve ARM aynı çağrıda. */
    {
        kg_baglam("md3 aynı adım: sızıntı");
        kg_anlik_don();
        kg_dunya()->sizinti = 1u;
        ida_in_t *in = kg_girdi(KG_OLAGAN);
        ida_sample_t *s = kg_ornek_bul_ya_da_ekle(in, (uint8_t)IDA_SENS_LEAK);
        s->u.leak.mask = 1u;
        (void)kg_komut(in, (uint8_t)IDA_CMD_ARM);
        (void)kg_cagir(in);
        if (kg_out()->armed) {
            /* Arıza bu çağrıda henüz tanınmadıysa sonraki çağrılarda eylem kısıtlayan yöndedir. */
            kg_adimlar(400);
        }
    }
    /* Acil durdurma kapanması ve ARM aynı çağrıda (A4-K44, K45, K111; W3-K2): kapanma
       daraltıcı girdidir, ARM NARROWED. Kapanma ancak çekirdek gevşetmeyi uyguluyorsa
       oluşur (gevşetme W4'te; W2-cekirdek M9); uygulamıyorsa acil durdurma zaten
       kısıtlayandır, kapanma yoktur ve ARM öteki ön koşullarla değerlendirilir (W3-K1). */
    {
        kg_baglam("md3 aynı adım: acil durdurma");
        kg_anlik_don();
        ida_in_t *in = kg_girdi(KG_OLAGAN);
        (void)kg_emniyet(in, 0u, 1u, KG_TUP_ACIK, KG_TUP_ACIK, true);
        ida_cmd_t *c = kg_komut(in, (uint8_t)IDA_CMD_ARM);
        int j = (int)(c - in->cmds);
        (void)kg_cagir(in);
        const ida_cmd_result_t *r = kg_sonuc(j);
        if (kg_gevsetme_gozlendi()) {
            KG_DOGRULA(r->ack == IDA_ACK_REJECTED && r->reason == IDA_REASON_NARROWED && !kg_out()->armed,
                       "acil durdurma kapanmasıyla aynı çağrıda ARM ack %u neden %u (A4-K45)", (unsigned)r->ack,
                       (unsigned)r->reason);
        } else {
            KG_DOGRULA((r->ack == IDA_ACK_REJECTED && r->reason == IDA_REASON_NARROWED) || r->ack == IDA_ACK_ACCEPTED,
                       "acil durdurmayla aynı çağrıda ARM ack %u neden %u; NARROWED ya da ACCEPTED (A4-K45; W3-K1)",
                       (unsigned)r->ack, (unsigned)r->reason);
        }
    }
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_md3_silahsiz_baslangic);
    RUN_TEST(test_md3_kip_ariza_matrisi);
    RUN_TEST(test_md3_ayni_adimda_ariza_ve_arm);
    return UNITY_END();
}
