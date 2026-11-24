/*
 * atis.h — atış kontrol modülü: emniyet zinciri (W5-atis-emniyet; mimari Bölüm 03 sürüm 0.3,
 * §2.6, §2.6.1, §2.7, §4 ve "Ek: uygulama gereksinimleri" G-Ç01–G-Ç53; mimar kararları
 * W5-K1…K11, docs/gorevler/W5-ortak.md).
 *
 * KAPSAM SINIRI (kullanıcı kararı): bu modül yalnız emniyet zincirini yürütür: silahlandırma,
 * süreli atış izni, acil durdurma ve sanal emniyet girdilerinin geri okuması, durum makinesi,
 * öncelik ve sonuç kodları, olayların üretimi ve sonuçları, güvenli duruma dönüş, kilitli
 * arıza. Hedefleme çözümü, önleme/vuruş hesabı, atış ölçütü, hedef algılama ve takip yoktur.
 * Çekirdek kendiliğinden atış kararı üretmez: FIRE yalnız kabul edilmiş bir operatör FC_FIRE
 * komutunun taşınmasıdır (W5-K3; G-Ç21). Algılama girdisi okunmaz; TRACKING ve
 * SOLUTION_VALID iç durumları yoktur (03 §2.6; A4 §3.16'nın dış değerleri eşlemede kalır).
 *
 * Çağrı çerçevesindeki kanca sırası (cerceve/cerceve.c, cerceve/cikti.c; core/TASARIM.md §10):
 *   (b)  ida_fc_daralt          her çağrıda (reddedilen ve out NULL dahil): bütünlük onarımı,
 *                               emniyet örneği bayatlığı (W5-K4 ve eki), daraltıcı girdilere
 *                               tepki (güvenli duruma dönüş; W5-K8)
 *   (b3) ida_fc_oturum_basi     SESSION_START
 *   (e)  ida_fc_emniyet_ornegi  kabul edilen her SAFETY örneği: gevşetme (A4-K116), bayatlık
 *                               izleri, geri okuma çelişkisi ve tekleme (W5-K4; G-Ç53)
 *        ida_fc_olay_sonucu / ida_fc_olay_bilinmiyor   olay sonuçları (W5-K2, K11)
 *        ida_fc_cagri           süreler ve evrensel geçişler (komutlardan önce)
 *        ida_fc_komut           FC komutları (A4 §3.17 satırları)
 *        ida_fc_adim            komutlardan sonra: evrensel geçişler yeniden, FC_RESET'in
 *                               yeniden kilitlenmesi (W5-K1), genişletici olay adayları (yalnız
 *                               IDA_RC_OK step), SAFETY_CLOSE isteklerinin olay modülüne
 *                               verilmesi
 *   çıktı ida_fc_cikti          olay modülünün çıkardığı olaylar okunur; dış durum yansıması
 */
#ifndef IDA_CORE_ATIS_H
#define IDA_CORE_ATIS_H

#include <stdbool.h>
#include <stdint.h>

#include "cerceve/komut.h"
#include "emniyet/emniyet.h"
#include "ida/port.h"
#include "olay/olay.h"
#include "param/param.h"
#include "telemetri/telemetri.h"

/* İç durumlar: özel bit desenleri ("hep 0" ya da "hep 1" geçerli değildir) ve tümleyen kopya
   (03 §2.6; A4-K89). Dış değer eşlemesi 03 §2.6.1 md. 1. ARMING ayrı durum değildir: kabul
   edilen FC_ARM aynı çağrıda ARMED'e geçer (A4 §3.17'nin FC_ARM satırı ön denetimdir). */
#define IDA_FC_IC_SAFE          0x1E2D3C4Bu /* → SAFE                                      */
#define IDA_FC_IC_ARMED         0x2B3A4958u /* → ARMED                                     */
#define IDA_FC_IC_FIRE_AUTH     0x3C4B5A69u /* → ARMED; izin açık sayılırken PERMIT_OPEN    */
#define IDA_FC_IC_FIRING        0x4D5C6B7Au /* → FIRE_PENDING                              */
#define IDA_FC_IC_ASSESS        0x5E6D7C8Bu /* → ARMED                                     */
#define IDA_FC_IC_TEST          0x6F7E8D9Cu /* → TEST                                      */
#define IDA_FC_IC_FAULT         0x708F9EADu /* → FAULT_LATCHED                             */

#define IDA_FC_TUP_YOK 0xA5u /* silahlı tüp yok (0 ve 1 tüp numarasıdır)                    */

/* Atış izninin kip kümesi (A4 §3.16 MODE biti): bu sürümde yalnız GUIDED (02 §3
   `mode_ok`: "AUTO'da TARGET_ALIGN ya da GUIDED"; TARGET_ALIGN görev öğesi arayüzde yoktur,
   A1 Tablo 3.6a). Uygulayıcının okuması; core/TASARIM.md §10.2. */
#define IDA_FC_IZIN_KIPI IDA_MODE_GUIDED

/* 03 §2.6 durum tablosunun iki zaman aşımı (TAHMİN, ölçülmedi). Parametre değil, derlemede
   sabittir: hiçbir koşulda değişmez (atis_param.def başındaki gerekçe). */
#define IDA_FC_T_ASSESS_MS  1000u /* ASSESS → SAFE                                          */
#define IDA_FC_T_TUP_BOS_MS 500u  /* FIRING: tüp boşaldı geri okuması gelmezse FAULT_LATCHED */

/* Tüp başına izin, olay ve geri okuma izleri. */
typedef struct {
    /* SAFETY_OPEN (A4-K66, K107, K123) */
    bool acik_bekliyor;        /* sonucu beklenen SAFETY_OPEN var                          */
    uint64_t acik_id;          /* son çıkan SAFETY_OPEN'ın kimliği                         */
    int64_t acik_cikis_ns;     /* onu çıkaran çağrının now_w'si                            */
    uint32_t acik_hold_ns;     /* onun hold_ns'i                                           */
    bool izin_var;             /* son APPLIED SAFETY_OPEN'ın izni (çekirdeğin saatinde)    */
    int64_t izin_bitis_ns;     /* çıkış now_w + hold_ns (G-Ç11)                            */
    int64_t izin_t_apply_ns;   /* o sonucun t_apply'ı (ölçüm zamanı)                       */
    /* FIRE */
    bool fire_bekliyor;        /* sonucu beklenen FIRE var                                 */
    bool atis_izleme;          /* FIRE APPLIED/DUPLICATE: tüp boşaldı geri okuması bekleniyor */
    int64_t atis_t_apply_ns;
    int64_t atis_izleme_bas_ns;/* duvar saati (T_tup_bos)                                  */
    /* SAFETY_CLOSE (G-Ç50, G-Ç53) */
    bool kapat_izleme;         /* son SAFETY_CLOSE APPLIED; araya yeni SAFETY_OPEN girmedi  */
    int64_t kapat_t_apply_ns;
    bool celiski;              /* G-Ç53 (1): kapatıldıktan sonra PERMIT_OPEN = 1 sürüyor   */
    bool kapanis_dogrulanmadi; /* W5-K11 (a): SAFETY_CLOSE sonucu bilinmiyor/EXPIRED_NO_ACK */
} ida_fc_tup_t;

typedef struct {
    uint8_t durum;             /* dış durum yansıması (IDA_FCS_*; A4 §3.16): çıktı yazımında
                                  ve her daraltmada iç durumdan kurulur                    */
    uint32_t ic;               /* IDA_FC_IC_*                                               */
    uint32_t ic_tumleyen;      /* ~ic                                                       */
    uint8_t tup;               /* silahlı tüp (FC_ARM'ın kabul edildiği): 0, 1 ya da
                                  IDA_FC_TUP_YOK                                            */
    uint8_t tup_tumleyen;      /* ~tup                                                      */
    int64_t giris_ns;          /* iç duruma giriş anı (duvar; T_pencere, T_assess)          */
    bool istek_var;            /* kabul edilmiş ve henüz FIRE üretmemiş FC_FIRE (G-Ç21)     */
    bool kip_izni_dusuk;       /* NARROW_MODE atış iznini kip koşulundan düşürdü (A4-K46);
                                  kip izin kümesine yeniden girişte kalkar                  */
    uint8_t onceki_kip;        /* kip girişini saptamak için                               */
    ida_fc_tup_t t[IDA_PORT_N_TUBE];
    /* Emniyet örneğinin bayatlığı (W5-K4 ve eki): son kabul edilen inst = 0 örneği */
    int64_t emn_rx_ns;         /* (i) alım zamanı                                           */
    int64_t emn_ilerleme_ns;   /* (ii) t_meas'in son ilerlediği örneğin alım zamanı         */
    bool emn_tmeas_var;        /* t_meas tabanı var (SESSION_START siler)                   */
    int64_t emn_tmeas_ns;      /* son kabul edilen inst = 0 örneğinin t_meas'i              */
    uint8_t son_tup[IDA_PORT_N_TUBE]; /* o örneğin tüp bitleri (sonuç sonradan gelirse
                                         G-Ç53 izleri bununla da değerlendirilir)           */
    bool emn_gevsedi;          /* bu çağrıda etkin değer gevşedi (SAFETY_INPUT günlüğü)    */
    /* Kilitli arıza nedenleri (W5-K1) */
    bool butunluk_kilidi;      /* iç durum bozulması görüldü: init'e kadar sürer           */
    bool bu_cagri_kilit;       /* bu çağrıda kilitli arıza nedeni ortaya çıktı              */
    bool reset_kabul;          /* bu çağrıda FC_RESET kabul edildi                          */
    uint8_t kapat;             /* FC'nin SAFETY_CLOSE istekleri (IDA_KAPAT_*)               */
    /* Oturum sınırı: oturum sonundan önce çıkmış olayların kimlik sınırı (W5-K11 a'nın
       yinelemesi yalnız sürmekte olan oturumun kapatmaları için; A1-K23, A4-K71) */
    bool oturum_sonu_bekliyor; /* SESSION_END işlendi; sınır ilk çıktıda kurulur           */
    uint64_t son_cikan_id;     /* çıkan en büyük olay kimliği                               */
    uint64_t oturum_siniri_id; /* son SESSION_END çağrısına kadar çıkan en büyük kimlik     */
} ida_fc_t;

/* Kancaların ortak bağlamı (çağrı boyunca sabit). */
typedef struct {
    ida_wtime_t now;           /* kullanılan now_w (A4-K10)                                 */
    const ida_param_t *prm;
    ida_gunluk_t *gun;
} ida_fc_ortam_t;

/* Atış kontrolün engel maskesini ve evrensel geçişlerini kurmak için çerçevenin verdiği
   çağrı durumu. */
typedef struct {
    bool armed;
    uint8_t mode;
    uint8_t est_level;
    uint8_t fs_level;
    bool events_ok;
    bool imu_lag;
    bool imu_bayat;
    bool oturum_var;          /* plat.link_state = RUNNING                          */
    uint32_t emn_fci;         /* ida_emn_fci(): ESTOP, PLUG_T*, PIN_T*, EMPTY_T*    */
    uint8_t bekleyen_tupler;  /* ida_olay_bekleyen_tupler(): RESULT_PENDING (A4-K123) */
    int64_t gcs_son_ns;       /* son kabul edilen HEARTBEAT'in now_w'si (A4-K52; görev
                                 modülünün izi; hiç gelmediyse ilk adımın now_w'si)  */
} ida_fc_baglam_t;

void ida_fc_kur(ida_fc_t *fc);
/* (b): her çağrıda (reddedilen ve out NULL dahil), ida_emn_daralt'tan hemen sonra. Kendi
   SAFETY_CLOSE isteklerini e->d.kapat'a, bayatlıktan doğan kapanmayı e->d'ye ve etkin
   değere yazar. */
void ida_fc_daralt(ida_fc_t *fc, ida_emn_t *e, const ida_fc_ortam_t *o);
/* (b3) SESSION_START: emniyet örneğinin ölçüm zamanı tabanı silinir; durum değişmez (A4-K72). */
void ida_fc_oturum_basi(ida_fc_t *fc);
/* (e): kabul edilen her SAFETY örneği (her inst; yalnız inst = 0 gevşetir; A4-K116, K34,
   K128). */
void ida_fc_emniyet_ornegi(ida_fc_t *fc, ida_emn_t *e, const ida_sample_t *s,
                           const ida_fc_ortam_t *o);
/* (e): algılama girdisi bu sürümde okunmaz (kapsam dışı; A4-K118 akışı hep bayat). */
void ida_fc_algilama(ida_fc_t *fc, const ida_in_t *in, ida_wtime_t now);
/* Kabul edilen olay sonucu (A4-K69) ve sonucu bilinmeyen olay (A4-K70). */
void ida_fc_olay_sonucu(ida_fc_t *fc, const ida_olay_bekleyen_t *olay,
                        const ida_event_result_t *r, const ida_fc_ortam_t *o);
void ida_fc_olay_bilinmiyor(ida_fc_t *fc, const ida_olay_bekleyen_t *olay,
                            const ida_fc_ortam_t *o);
/* (e), komutlardan önce: süre aşımları ve evrensel geçişler. */
void ida_fc_cagri(ida_fc_t *fc, const ida_emn_t *e, const ida_fc_baglam_t *b,
                  const ida_fc_ortam_t *o);
/* fc.inhibit_mask (A4-K91, 3.16). */
uint32_t ida_fc_engel(const ida_fc_t *fc, const ida_fc_baglam_t *b);
/* fc.state: son yazılan dış durum yansıması. */
uint8_t ida_fc_durum(const ida_fc_t *fc);
/* FC komutu: K119 biçim denetiminden ve genişletici yasaklarından geçmiş (tablo 3.17). */
void ida_fc_komut(ida_fc_t *fc, const ida_komut_baglam_t *b, const ida_fc_cmd_t *cmd,
                  ida_cmd_result_t *r);
/* Kabul edilen her çağrıda komutlardan sonra; genişletici adaylar yalnız step_ok iken
   (IDA_RC_OK dönen step) olay->aday[]'a konur (kapı süzer). */
void ida_fc_adim(ida_fc_t *fc, const ida_emn_t *e, const ida_fc_baglam_t *b, ida_olay_t *olay,
                 bool step_ok, const ida_fc_ortam_t *o);
/* Çıktı yazımında, ida_olay_yaz'dan hemen sonra: çıkan SAFETY_OPEN ve FIRE'lar kaydedilir,
   dış durum yansıması kurulur. */
void ida_fc_cikti(ida_fc_t *fc, const ida_out_t *out, ida_wtime_t now);
bool ida_fc_tutarli(const ida_fc_t *fc);
uint8_t ida_fc_param_grup(const ida_prm_kume_t *p);

#endif /* IDA_CORE_ATIS_H */
