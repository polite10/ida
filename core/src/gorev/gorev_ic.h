/*
 * gorev_ic.h — görev modülünün iç arayüzü (kip tablosu, arıza tablosu, saf adım ve komut
 * işlevleri). Yalnız gorev/ kaynakları ve birim testleri içerir; çerçeve kancaları gorev.h'tedir.
 *
 * Kancalar (gorev.c) girdiyi çekirdekten toplar ve günlüğü, parametreleri bu işlevlere açıkça
 * verir; birim testleri aynı işlevleri yapay girdiyle sürer (kestirim düzeyi, IMU bayatlığı,
 * duvar saati), çünkü yer tutucu kestirim çekirdek yoluyla INIT'ten çıkmaz.
 */
#ifndef IDA_CORE_GOREV_IC_H
#define IDA_CORE_GOREV_IC_H

#include <stdbool.h>
#include <stdint.h>

#include "gorev/gorev.h"
#include "kestirim/kestirim.h"
#include "telemetri/telemetri.h"

/* ---- Kip tablosu (kip.c; 02 §2.6, A4-K127) ---------------------------------------------- */

#define IDA_GOR_KIP_SAYI 8u /* IDA_MODE_INIT … IDA_MODE_AUTO */

/* Kip olayları. DARALT_* olaylarını emniyet modülü (b) adımında uygular (A4-K46, K127);
   tabloda belge ve çapraz sınama için bulunurlar. */
enum ida_gor_olay {
    IDA_GOR_OLAY_HAZIR = 0,           /* çekirdek başlatması bitti: kestirim INIT'ten çıktı */
    IDA_GOR_OLAY_GUIDED,              /* SET_MODE(GUIDED) (genişletici)                      */
    IDA_GOR_OLAY_AUTO,                /* SET_MODE(AUTO) (genişletici)                        */
    IDA_GOR_OLAY_KONUM_YOK,           /* konum geçersiz (W3-K5)                              */
    IDA_GOR_OLAY_GOREV_YOK,           /* AUTO'da yürüyen ya da duraklamış görev yok          */
    IDA_GOR_OLAY_DARALT_MANUAL,       /* NARROW_MODE → MANUAL                                */
    IDA_GOR_OLAY_DARALT_STABILIZE,
    IDA_GOR_OLAY_DARALT_DEPTH_HOLD,
    IDA_GOR_OLAY_DARALT_STATION_KEEP,
    IDA_GOR_OLAY_DARALT_GECERSIZ,     /* geçersiz hedefli NARROW_MODE: kip değişmez          */
    IDA_GOR_OLAY_SAYI
};

/* Tablo hücresinin özel değerleri (geri kalanı hedef kiptir, 0–7). */
#define IDA_GOR_KIP_KAL      0xF0u /* kip değişmez                                         */
#define IDA_GOR_KIP_RET      0xF1u /* geçiş yok: komut REJECTED / BAD_MODE                  */
#define IDA_GOR_KIP_SILAHSIZ 0xF2u /* tanımsız kip ya da olay: silahsızlan, kip DISARMED    */

/* Tablonun hücresi; kip ya da olay tanımsızsa IDA_GOR_KIP_SILAHSIZ (EMNİYET). */
uint8_t ida_gor_kip_gecis(uint8_t kip, uint8_t olay);

/* Hücreyi uygular: kipi değiştirir (MODE_CHANGE), SILAHSIZ'da silahsızlandırır
   (ARM_CHANGE, neden NOT_ARMABLE) ve kipi DISARMED yapar. Uygulanan hücreyi döner. */
uint8_t ida_gor_kip_uygula(ida_emn_t *yetki, uint8_t olay, ida_gunluk_t *gun, int64_t now_ns);

/* ---- Arıza koşulları ve kip × arıza tablosu (ariza.c; 02 §4.2, §4.3) --------------------- */

enum ida_gor_kosul {
    IDA_GOR_K_SIZINTI = 0,   /* SIZINTI_N ardışık ıslak örnek (mandallı)                    */
    IDA_GOR_K_SIZINTI_BAYAT, /* sızıntı algılayıcısı bayat ya da hiç örnek yok              */
    IDA_GOR_K_GCS_1,         /* HEARTBEAT'siz GCS_KAYIP_MS                                  */
    IDA_GOR_K_GCS_2,         /* HEARTBEAT'siz GCS_KAYIP2_MS                                 */
    IDA_GOR_K_BATT_DUSUK,    /* BATT_SURE_MS boyunca < BATT_DUSUK_V (mandallı)              */
    IDA_GOR_K_BATT_KRIT,     /* BATT_SURE_MS boyunca < BATT_KRIT_V (mandallı)               */
    IDA_GOR_K_BATT_KRIT_2,   /* kritikten BATT_KRIT2_MS sonra (mandallı)                    */
    IDA_GOR_K_BATT_BAYAT,    /* batarya ölçeri bayat ya da hiç örnek yok                    */
    IDA_GOR_K_EST_BOZUK,     /* konum geçersiz (W3-K5; anında; EST_OK_MS geçerli konumla temizlenir) */
    IDA_GOR_K_EST_BOZUK_UZUN,/* EST_BOZUK_MS boyunca konum geçersiz                         */
    IDA_GOR_K_EST_KOTU,      /* kestirim BAD (ya da INIT dışı kipte INIT) EST_KOTU_MS (mandallı) */
    IDA_GOR_K_SAYISAL,       /* kestirim FAULT: sayısal çökme, beklemesiz (mandallı)        */
    IDA_GOR_K_IMU_BAYAT,     /* A4-K39 bayatlığı (mandallı)                                 */
    IDA_GOR_K_PARAM,         /* geçerli parametre kümesi yok (A4-K78)                       */
    IDA_GOR_K_COGRAFI,       /* coğrafi sınır: yatay yarıçap ya da toplam görev süresi
                                (02 §4.3; mandallı)                                         */
    IDA_GOR_K_IRTIFA,        /* en az irtifa: nav.altitude < ALT_MIN (02 §4.3; otomatik)     */
    IDA_GOR_K_GOREV_IPTAL,   /* görevin kendi iptali (zaman aşımı, yürütülemez öğe, ölü hesap
                                beklemesi, AUTO'da SURFACE ve üstü arıza): SURFACE (mandallı) */
    IDA_GOR_K_SAYI
};

#define IDA_GOR_K_BIT(k) (UINT32_C(1) << (uint32_t)(k))
#define IDA_GOR_K_TANIMLI ((UINT32_C(1) << (uint32_t)IDA_GOR_K_SAYI) - 1u)

/* Yatay geçerliliğin TEK tanımı (sol bulgu 3; W3-K5, K7): konum VE yatay hız. Kip kabulü
   (AUTO), kip düşmesi (STATION_KEEP) ve konum kaybı koşulu bunu kullanır. */
static inline bool ida_gor_yatay_gecerli(uint8_t gecerlilik)
{
    const uint8_t yatay = (uint8_t)(IDA_KES_G_KONUM | IDA_KES_G_YATAY_HIZ);
    return (gecerlilik & yatay) == yatay;
}

/* Etkin koşul kümesi `aktif` ile kip sütunu `kip`te arıza eylemi düzeyi ve nedenleri (saf;
   bozulma izi ve AUTO × GCS kuralları dahil: AUTO'da görev RUNNING değilse GCS satırları
   DEPTH_HOLD sütunuyla). nedenler NULL olabilir. */
uint8_t ida_gor_ariza_duzey(const ida_gor_t *g, uint32_t aktif, uint8_t kip, uint32_t *nedenler);
/* Kip komutla değişti: yeni sütunda mandalları kurar ve düzeyi yayınlar (FAILSAFE_CHANGE). */
void ida_gor_ariza_kip_degisti(ida_gor_t *g, uint8_t kip, ida_gunluk_t *gun, int64_t now_ns);

/* Hücre: kip sütununda koşulun eylem düzeyi (IDA_FS_*). Kip ya da koşul tanımsızsa
   IDA_FS_DISARM (EMNİYET). AUTO × GCS_2 hücresi yüzeyde olmayı ayrıca ister (ariza.c). */
uint8_t ida_gor_ariza_hucre(uint8_t kosul, uint8_t kip);
/* Koşulun neden biti (IDA_FSR_*), mandallı olup olmadığı ve arm reddi gerektirip
   gerektirmediği (kipten bağımsız). */
uint32_t ida_gor_ariza_nedeni(uint8_t kosul);
bool ida_gor_ariza_mandalli(uint8_t kosul);
bool ida_gor_ariza_arm_reddi(uint8_t kosul);

/* ---- Saf adım ve komut işlevleri ------------------------------------------------------- */

/* Bir adımın girdisi (çekirdekten toplanır ya da testte yapay kurulur). */
typedef struct {
    int64_t now_ns;          /* kullanılan now_w (A4-K10)                                   */
    uint8_t est_level;       /* IDA_EST_*                                                   */
    uint8_t suzgec_duzey;    /* IDA_EST_*: çökme mandalından bağımsız süzgeç düzeyi
                                (ida_kes_suzgec_duzey; yalnız est_level FAULT iken okunur) */
    uint8_t gecerlilik;      /* IDA_KES_G_* (W3-K5; ida_kes_gecerlilik)                     */
    bool imu_bayat;          /* A4-K39 (çerçevenin hesabıyla aynı)                          */
    bool imu_lag;            /* A4-K37 mandalı (yalnız günlük için; eylem üretmez, A4-K108) */
    bool param_ok;           /* A4-K78                                                      */
    float nav_derinlik;      /* m (yalnız IDA_KES_G_DERINLIK iken kullanılır)               */
    float nav_yon;           /* rad (yalnız IDA_KES_G_YON iken)                             */
    float nav_ne[2];         /* m (yalnız IDA_KES_G_KONUM iken)                             */
    /* W5-gorev-yurutucu */
    int64_t t_olcum_ns;      /* son kabul edilen zaman çerçevesinin ölçüm zamanı (davranış
                                süreleri; 02 §2.6)                                          */
    bool irtifa_var;         /* nav.altitude biliniyor (≥ 0)                                */
    float irtifa;            /* m, dibe                                                     */
    bool gnss_taze;          /* GNSS konumu füzyonlandı ve T_GNSS_STALE içinde (kestirim)    */
    int32_t orijin_lat_e7;   /* nav orijini (GLOBAL öğelerin yerel düzlemi)                  */
    int32_t orijin_lon_e7;
    const ida_param_t *prm;
} ida_gor_girdi_t;

/* ida_gor_adim'in gövdesi. */
void ida_gor_adim_ic(ida_gor_t *g, ida_emn_t *yetki, const ida_gor_girdi_t *gi,
                     ida_gunluk_t *gun);
/* ida_gor_komut'un gövdesi. */
void ida_gor_komut_ic(ida_gor_t *g, ida_emn_t *yetki, const ida_komut_baglam_t *b,
                      const ida_cmd_t *cmd, ida_cmd_result_t *r, const ida_param_t *prm,
                      ida_gunluk_t *gun);

/* ariza.c: koşulları değerlendirir, mandalları kurar, düzeyi ve nedenleri yazar,
   değiştiyse FAILSAFE_CHANGE üretir. */
void ida_gor_ariza_degerlendir(ida_gor_t *g, const ida_emn_t *yetki, const ida_gor_girdi_t *gi,
                               ida_gunluk_t *gun);
/* Mandal ve etkin koşullardan düzeyi ve nedenleri yeniden hesaplar (kip sütunu `kip`);
   değiştiyse FAILSAFE_CHANGE üretir. */
void ida_gor_ariza_yayinla(ida_gor_t *g, uint8_t kip, ida_gunluk_t *gun, int64_t now_ns);
/* FAILSAFE_RESET'in temizleme koşulu: bütün mandallı koşullar temizlenebiliyorsa true. */
bool ida_gor_ariza_temizlenebilir(const ida_gor_t *g, const ida_emn_t *yetki,
                                  const ida_param_t *prm, int64_t now_ns);
/* Görevin bekleyen iptal isteğini aynı adımda mandala alır ve düzeyi yeniden yayımlar. */
void ida_gor_ariza_iptal_uygula(ida_gor_t *g, uint8_t kip, ida_gunluk_t *gun, int64_t now_ns);

/* ---- Görev: yükleme ve komutlar (gorev_yukle.c), yürütme (gorev_yurut.c) ------------------ */

/* A1 Tablo 3.6a'nın bütün koşulları (A4-K51): tür, çerçeve, konum alanları, derinlik, p[i]. */
bool ida_gor_oge_gecerli(const ida_mission_item_t *m);
/* MISSION_BEGIN, MISSION_ITEM, MISSION_END, MISSION_CTL (tablo 3.17). */
void ida_gor_gorev_komut(ida_gor_t *g, const ida_emn_t *yetki, const ida_cmd_t *cmd,
                         ida_cmd_result_t *r);
/* Görevin değiştirilebildiği durum (A4-K51): IDLE, LOADED, PAUSED, COMPLETE, ABORTED ya da HOLD
   arıza eylemi sürerken. */
bool ida_gor_gorev_degisebilir(const ida_gor_t *g);
/* AUTO'ya giriş ya da yeniden giriş kabul edildi: LOADED → RUNNING (öğe 0), PAUSED → RUNNING.
   Açık yükleme kapanır (yürüyen görev değiştirilemez). */
void ida_gor_gorev_kip_auto(ida_gor_t *g);
/* AUTO'dan çıkış: RUNNING → PAUSED (duraklama noktası yakalanır). */
void ida_gor_gorev_kip_cikis(ida_gor_t *g);
/* Açık yüklemeyi kapatır (RUNNING'e her geçişte; A4-K51). */
void ida_gor_gorev_yukleme_kapat(ida_gor_t *g);
/* Adımın görev kısmı (kip değerlendirmesinden önce): ölü hesap yolu, engeller (RUNNING ⇄
   PAUSED), öğe başlangıcı, bitiş ölçütü, zaman aşımı, güdüm hedefi. */
void ida_gor_gorev_adim(ida_gor_t *g, const ida_emn_t *yetki, const ida_gor_girdi_t *gi);
/* Arıza değerlendirmesinden sonra (arıza görevin önündedir): HOLD → PAUSED; SURFACE ve üstü →
   ABORTED (iptal mandalı aynı adımda). */
void ida_gor_gorev_ariza(ida_gor_t *g, uint8_t kip, ida_gunluk_t *gun, int64_t now_ns);
/* Coğrafi sınır, en az irtifa ve iptal koşullarının bitleri (ariza.c). */
uint32_t ida_gor_gorev_kosullari(const ida_gor_t *g, const ida_gor_girdi_t *gi);
void ida_gor_gorev_oturum_basi(ida_gor_t *g);
bool ida_gor_gorev_tutarli(const ida_gor_t *g);
/* Duraklama noktası: bulunulan konum, derinlik ve yön (son adımın seyrüsefer görüntüsü). */
void ida_gor_gorev_durak_yakala(ida_gor_t *g);

#endif /* IDA_CORE_GOREV_IC_H */
