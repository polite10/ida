/*
 * gorev.h — kip makinesi, araç silahlandırması (genişletme yönü), arıza güvenliği ve kontrol
 * isteği. Sahibi: W3 güdüm ve görev işi (docs/gorevler/W3-gorev.md, dilim 1; mimar kararları
 * W3-K1…K5, docs/gorevler/W3-ortak.md).
 *
 * Yetki: `armed` ve `mode` (ida_emn_t) alanlarına GENİŞLETEN yönde yalnız bu modül yazar
 * (core/TASARIM.md §1): ARM komutu `armed`'ı 1, SET_MODE(GUIDED/AUTO) kipi genişletir. Emniyet
 * modülünün (b) adımında uyguladığı daraltma bu modülce geri alınmaz: genişletici komutlar
 * dağıtıcıdan ancak o çağrıda daraltma yoksa gelir (A4-K45); modül ayrıca kendi arıza
 * daraltmasının uygulandığı çağrıda da genişleticiyi reddeder (W3-K2). Bu modül kısıtlayan
 * yönde de yazar: tanımsız kip ve DISARM arıza eylemi silahsızlandırır; ön koşulu düşen kip alt
 * kipe iner (kip tablosu, kip.c). Sanal emniyet girdileri (acil durdurma, fiş, pim) yalnız atış
 * kontrol izinlerini kapatır; araç silahlandırmasına ve itkiye girmez (W3-K1).
 *
 * Çağrı içi sıra (cerceve.c): ida_gor_adim (kısıtlayan her şey: kip düşmesi, arıza eylemleri,
 * silahsızlandırma, arm ön koşullarının anlık görüntüsü) → komutlar (ida_gor_komut; genişleten
 * her şey) → kontrol isteği (ida_gud_kon_istegi → ida_gor_istek; kontrol modülü okur). Arm ve
 * FAILSAFE_RESET'in kararı adım anındaki görüntüye dayanır; aynı çağrıdaki komutların
 * sırasından bağımsızdır.
 *
 * Komutlar (tablo 3.17): HEARTBEAT (A4-K52), ARM, SET_MODE (GUIDED/AUTO), FAILSAFE_RESET,
 * MISSION_BEGIN/ITEM/END/CTL (W5-gorev-yurutucu; gorev_yukle.c).
 *
 * Görev (W5-gorev-yurutucu; 02 §2.6): atomik yükleme (A4-K51, A1 Tablo 3.6a), durum makinesi
 * IDLE → LOADED → RUNNING ⇄ PAUSED → COMPLETE | ABORTED, öğe yürütücüleri (gorev_yurut.c).
 * Yürütücü yalnız güdüme hedef (ida_gor_hedef_t) üretir; arıza eylemi her zaman görevin
 * önündedir: HOLD görevi duraklatır, SURFACE ve üstü görevi iptal eder. Görev hiçbir koşulda
 * silahlandırma, kip genişletmesi, atış izni ya da FC komutu üretmez.
 */
#ifndef IDA_CORE_GOREV_H
#define IDA_CORE_GOREV_H

#include <stdbool.h>
#include <stdint.h>

#include "cerceve/komut.h"
#include "emniyet/emniyet.h"
#include "ida/port.h"
#include "kontrol/kontrol.h"
#include "param/param.h"

/* Kestirim bileşenlerinin geçerliliği (W3-K5): kestirim/kestirim.h'nin IDA_KES_G_* bitleri
   (ida_kes_gecerlilik). Kontrol isteği: kontrol/kontrol.h'nin ida_kon_istek_t'si (IDA_KON_*). */

/* ---- Görev deposu ve yürütücüsü (W5-gorev-yurutucu; 02 §2.6, A4-K51, A1 Tablo 3.6a) ----- */

#define IDA_GOR_GOREV_AZAMI 128u /* A4 3.17 MISSION_BEGIN: count 1–128 */

/* Öğenin konum alanları: LOCAL'de ne (m, NED, senaryo orijini), GLOBAL'de ll (lat_e7, lon_e7);
   NONE'da kullanılmaz. Yalnız çerçevenin üyesi okunur. */
typedef union {
    float ne[2];
    int32_t ll[2];
} ida_gor_konum_t;

/* Depodaki öğe (ida_mission_item_t'nin index'siz, sıkıştırılmış biçimi; 32 B). */
typedef struct {
    uint8_t tur;     /* IDA_MI_*       */
    uint8_t cerceve; /* IDA_MI_FRAME_* */
    uint8_t bos[2];
    float p[4];
    ida_gor_konum_t konum;
    float derinlik;  /* m, aşağı +     */
} ida_gor_oge_t;

/* Yürütücünün güdüme verdiği hedef (gudum.c ida_gud_adim / ida_gud_kon_istegi okur). */
#define IDA_GOR_HY_YOK 0u /* güdüm hedefi yok: AUTO isteği görev modülünün tutmasıdır      */
#define IDA_GOR_HY_YOL 1u /* bas_ne → hedef_ne doğrusunda yol takibi (ILOS), hız `hiz`      */
#define IDA_GOR_HY_TUT 2u /* hedef_ne'de konum ve `yon`'de yön tutma                        */
#define IDA_GOR_HD_DERINLIK 0u /* derinlik referansı hedef_d'ye, en çok `dikey_hiz`          */
#define IDA_GOR_HD_YUZEY    1u /* yüzeye çıkış (`dikey_hiz`); yüzeyde dikey kapalı (yüzey kipi) */

typedef struct {
    uint8_t yatay;     /* IDA_GOR_HY_*                                                      */
    uint8_t dikey;     /* IDA_GOR_HD_*                                                      */
    uint8_t asama;     /* IDA_PH_* (status.phase; AUTO'da)                                  */
    uint32_t kusak;    /* hedef değişti (öğe, durum): güdüm ILOS integralini sıfırlar        */
    float bas_ne[2];   /* m, NED                                                            */
    float hedef_ne[2]; /* m, NED                                                            */
    float hiz;         /* m/s, yol hızı (YOL)                                               */
    float yon;         /* rad (TUT)                                                         */
    float hedef_d;     /* m (DERINLIK)                                                      */
    float dikey_hiz;   /* m/s, referansın hız sınırı (> 0)                                  */
} ida_gor_hedef_t;

typedef struct {
    /* Yürürlükteki görev */
    ida_gor_oge_t oge[IDA_GOR_GOREV_AZAMI];
    uint16_t n;
    /* Açık yükleme (A4-K51: ya tümü ya hiçbiri; ayrı tampon) */
    ida_gor_oge_t yuk[IDA_GOR_GOREV_AZAMI];
    uint16_t yuk_n;            /* MISSION_BEGIN'in count'u                                    */
    uint16_t yuk_alinan;       /* sırayla kabul edilen öğe                                    */
    bool yuk_acik;
    /* Yürütme */
    bool oge_basladi;          /* etkin öğenin başlangıcı yapıldı                             */
    bool kosuyordu;            /* önceki adımın sonunda RUNNING (öğe süresi sayılır)          */
    bool operator_durdu;       /* MISSION_CTL PAUSE                                           */
    bool dr_bekliyor;          /* ölü hesap yol sınırı: yüzeyde GNSS bekleniyor (W5-K24)      */
    bool iptal_bekliyor;       /* görevin iptal isteği arıza mandalına alınmadı               */
    bool gorev_bas_var;
    bool son_now_var;
    bool onceki_yol;           /* önceki öğe yol noktasıydı: bacak onun hedefinden başlar     */
    bool bant_say;             /* DIVE_TO bandı / HOLD süresi sayılıyor                       */
    bool dr_son_var;
    bool durak_var;            /* duraklama noktası yakalandı                                 */
    uint8_t asama;             /* öğe içi: WAIT_GNSS 0 yükselme, 1 bekleme                     */
    uint32_t iptal_nedeni;     /* IDA_FSR_*: iptal mandalının nedeni                          */
    int64_t gorev_bas_ns;      /* duvar: görevin başladığı an (toplam görev süresi)           */
    int64_t son_now_ns;
    int64_t oge_gecen_ns;      /* duvar: öğede RUNNING geçen süre                             */
    int64_t oge_asim_ns;       /* duvar: öğenin (ya da alt aşamanın) zaman aşımı              */
    int64_t bant_bas_ns;       /* ölçüm zamanı: bant/bekleme sayacının başı                   */
    int64_t dr_bas_ns;         /* duvar: GNSS beklemesinin başı                               */
    float dr_yol;              /* m: GNSS desteği olmadan kat edilen yatay yol                 */
    float dr_son_ne[2];
    float onceki_ne[2];        /* önceki yol noktasının hedefi                                */
    float oge_ne[2];           /* öğe başında yakalanan konum, derinlik ve yön                */
    float oge_d;
    float oge_yon;
    float oge_hedef_ne[2];     /* öğenin yatay hedefi (LOCAL/GLOBAL çözülmüş ya da oge_ne)    */
    float durak_ne[2];         /* duraklamada yakalanan konum, derinlik ve yön                */
    float durak_d;
    float durak_yon;
    ida_gor_hedef_t hedef;
} ida_gor_gorev_t;

/* ---- Modül durumu ------------------------------------------------------------------------ */

typedef struct {
    /* Kip ve silahlandırma izleri */
    uint8_t son_kip;           /* son adımda görülen kip (emniyetin değiştirdiği kip izi)   */
    bool arm_engeli;           /* adım anında arm ön koşulu tutmuyor                       */
    bool adim_daralt;          /* bu çağrının adımında arıza daraltması uygulandı (W3-K2);
                                  dağıtıcı A4-K45 konumunda NARROWED verir (W3-K9)          */
    bool bozuk;                /* durum bozulması görüldü: init'e kadar DISARM (EMNİYET)    */
    bool taban_var;            /* zamanlayıcı tabanı kuruldu (ilk adım)                    */
    bool bozuk_yeni;           /* bozulma adımdan önce (örnek, oturum başı) görüldü; bu
                                  çağrının adımı onu işler                                  */
    uint8_t itici_disi;        /* devre dışı itici maskesi (bit i: itici i); dilim 1'de 0  */
    /* Arıza */
    uint8_t fs_level;          /* IDA_FS_*                                                 */
    uint32_t fs_nedenleri;     /* IDA_FSR_* (IMU_LAG çerçevece eklenir)                     */
    uint32_t etkin;            /* bu adımda tutan koşullar (1 << IDA_GOR_K_*)              */
    uint32_t mandal;           /* mandallı koşullar; yalnız FAILSAFE_RESET temizler         */
    /* Yer istasyonu canlılığı (A4-K52; duvar saati) */
    bool gcs_var;              /* HEARTBEAT kabul edildi                                   */
    int64_t gcs_son_ns;        /* son HEARTBEAT'in now_w'si; yoksa ilk adımın now_w'si      */
    /* Batarya (A4-K76) */
    bool batt_var;
    float batt_v;              /* V, son kabul edilen örnek                                */
    int64_t batt_rx_ns;
    bool batt_dusuk_say;
    int64_t batt_dusuk_bas_ns;
    bool batt_krit_say;
    int64_t batt_krit_bas_ns;
    int64_t batt_krit_tetik_ns;/* kritik koşulun mandallandığı an                          */
    /* Sızıntı (A4-K76) */
    bool sizinti_var[IDA_PORT_MAX_INST];      /* inst'ten örnek kabul edildi            */
    uint8_t sizinti_ardisik[IDA_PORT_MAX_INST]; /* art arda ıslak örnek (255'te doyar)   */
    uint8_t sizinti_tepe[IDA_PORT_MAX_INST];  /* son değerlendirmeden beri en büyük ardışık
                                                 sayı (çağrı içi eşik aşımı kaybolmaz)     */
    int64_t sizinti_rx_ns[IDA_PORT_MAX_INST]; /* inst başına son kabul edilen örnek     */
    /* Kestirim ve IMU zamanlayıcıları */
    uint8_t est_level;         /* adım anındaki kestirim düzeyi                            */
    uint8_t gecerlilik;        /* adım anındaki bileşen geçerliliği (IDA_KES_G_*)          */
    bool est_ok_say;           /* düzey yeterli (OK ya da DEGRADED) sürüyor                 */
    int64_t est_ok_bas_ns;
    bool konum_say;            /* konum geçerli sürüyor                                    */
    int64_t konum_bas_ns;
    bool konum_yok_izi;        /* konum kaybı görüldü; EST_OK_MS geçerli konumla silinir   */
    int64_t konum_yok_bas_ns;
    bool est_kotu_say;
    int64_t est_kotu_bas_ns;
    bool imu_taze_say;
    int64_t imu_taze_bas_ns;
    bool imu_lag;              /* adım anındaki IMU_LAG mandalı (yalnız FAILSAFE_CHANGE günlüğü) */
    /* Seyrüsefer görüntüsü ve tutma hedefleri (kontrol isteği) */
    bool nav_tutum_var;
    bool nav_yon_var;
    bool nav_derinlik_var;
    bool nav_konum_var;
    float nav_derinlik;
    float nav_yon;
    float nav_ne[2];
    bool yuzeyde;              /* derinlik geçerli ve ≤ YUZEY_D                             */
    bool dalis_yasak;          /* sızıntı algılayıcısı bayat: derinleşme yok (02 §4.3)      */
    float yukselme_hizi;       /* m/s, parametrenin adım anındaki değeri                    */
    bool tut_derinlik_var;
    bool tut_yon_var;
    bool tut_konum_var;
    float tut_derinlik;
    float tut_yon;
    float tut_ne[2];
    /* Kuşaklar (sol bulgular 1, 2; EMNİYET) */
    bool onceki_dalis_yasak;
    /* Konuma dayalı arıza koşulları ve görevin girdileri (W5-gorev-yurutucu) */
    bool nav_irtifa_var;       /* nav.altitude ≥ 0 ve sonlu                                 */
    float nav_irtifa;          /* m                                                         */
    bool gnss_taze;            /* GNSS konumu füzyonlandı ve taze (kestirim)                */
    bool irtifa_kisit;         /* en az irtifa koşulu etkin: güdüm derinleşmeyi keser       */
    int64_t t_olcum_ns;        /* son zaman çerçevesinin ölçüm zamanı                        */
    /* Görev */
    uint8_t mission_state;     /* IDA_MS_*                                                  */
    uint16_t mission_item;
    ida_gor_gorev_t gv;
} ida_gor_t;

void ida_gor_kur(ida_gor_t *g);
void ida_gor_oturum_basi(ida_gor_t *g);
/* Kabul edilen BATT ve LEAK örnekleri (A4-K76). */
void ida_gor_ornek(ida_gor_t *g, const ida_sample_t *s);
/* Kip yöneticisi ve arıza değerlendirmesi; her kabul edilen çağrıda, komutlardan önce.
   c yalnız okunur, tek yazılan c->gun'dur (günlük). */
void ida_gor_adim(ida_gor_t *g, ida_emn_t *yetki, ida_core_t *c);
/* HEARTBEAT, ARM, SET_MODE (GUIDED/AUTO), FAILSAFE_RESET, MISSION_* (tablo 3.17). Yazılanlar:
   c->gor, c->emn (armed, mode; genişletme yönü), c->gun (günlük); c->prm yalnız okunur. */
void ida_gor_komut(ida_core_t *c, const ida_komut_baglam_t *b, const ida_cmd_t *cmd,
                   ida_cmd_result_t *r);
/* Bu çağrının adımında arıza daraltması uygulandı (W3-K2, K9): çerçeve dağıtıcının daraltma
   bayrağına (A4-K45 konumu) ve olay kapısına katar. */
bool ida_gor_daraldi(const ida_gor_t *g);
uint8_t ida_gor_fs_duzey(const ida_gor_t *g);      /* IDA_FS_* */
uint32_t ida_gor_fs_nedenleri(const ida_gor_t *g); /* IDA_FSR_* (IMU_LAG çerçevece eklenir) */
/* status'un görev alanları: regime, phase, mission_state, mission_item. */
void ida_gor_durum(const ida_gor_t *g, ida_status_t *st);
bool ida_gor_tutarli(const ida_gor_t *g);
uint8_t ida_gor_param_grup(const ida_prm_kume_t *p);

/* Kontrol isteği: durumun saf işlevi; komutlardan sonra (kontrol adımında) çağrılır. Kontrol
   modülü onu güdüm erişimcisi ida_gud_kon_istegi (gudum.h) üzerinden okur. */
void ida_gor_istek(const ida_gor_t *g, const ida_emn_t *yetki, ida_kon_istek_t *ist);
/* Devre dışı itici maskesi (bit i: A5 itici sırasında itici i); kontrol tahsisi okur
   (kontrol.c ida_kon_girdi_topla). Dilim 1'de itici arızası saptanmaz: 0. */
uint8_t ida_gor_itici_disi(const ida_gor_t *g);

#endif /* IDA_CORE_GOREV_H */
