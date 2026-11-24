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
 * Komutlar (tablo 3.17): HEARTBEAT (A4-K52), ARM, SET_MODE (GUIDED/AUTO), FAILSAFE_RESET bu
 * sürümde uygulanır; MISSION_* dilim 2'dedir ve UNSUPPORTED / NONE alır (A4-K124).
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
    /* Görev (dilim 2) */
    uint8_t mission_state;     /* IDA_MS_*                                                  */
    uint16_t mission_item;
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
