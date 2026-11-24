/*
 * kontrol.h — kontrol döngüleri ve itki tahsisi. Sahibi: W3 kontrol işi.
 *
 * Girdi: görev/güdüm modülünün isteği (ida_kon_istek_t), kestirilen durum (A4-K87 nav) ve
 * çerçeveyle görevden gelen emniyet kararları (ida_kon_girdi_t). Çıktı: sekiz iticinin
 * normalleştirilmiş ESC komutu (A4-K54; A5 itici sırası, [−1, 1]). Eğrinin tersi ve ölü bant
 * bu modüldedir (A4-K54); darbe genişliğine çeviri platformundur.
 *
 * Yapı (02 §2.5; 06 §2.4 fikir kaynağı): dış döngüler derinlik → dikey hız setpoint'i,
 * yön/yalpa/baş-kıç açısı → açısal hız setpoint'i; iç döngüler dikey hız PI (+ yüzerlik ve sönüm ileri
 * beslemesi), açısal hız PI, yatay hız PI. Öteleme kuvvetleri yön çerçevesinde (yalnız yaw ile
 * döndürülmüş NED) hesaplanır ve gövdeye Rᵀ R_ψ ile çevrilir. Bütün döngüler adım hızında
 * (step OK; tahmin 200 Hz) koşar.
 *
 * EMNİYET (W3-kontrol md. 3; W3-K1, W3-K4): silahsızken, kestirim INIT/BAD/FAULT iken, arıza
 * eylemi itkisiz düzeydeyken (PASSIVE_SURFACE ve üstü), istek KAPALI ya da geçersizken sekiz
 * komut tam 0.0f'tır ve döngü durumu sıfırlanır. Kararın girdileri çerçeveden ve görev
 * modülünden gelir (ida_kon_girdi_topla); bu modül yalnız uygular. Sonlu olmayan durum ya da
 * ara sonuçta komut 0 ve *cokme = true (A4-K55).
 *
 * EMNİYET — acil durdurma (W5-kontrol-acil; W5-K26; W3-K1 ve W3-K4'ün "acil durdurma itki
 * kapısında yer almaz" maddesinin yerine geçer). Etkin acil durdurma değeri kısıtlayanken
 * (basılı, bayat ya da init'ten beri hiç gevşememiş; A4-K116, W5-K4; ida_kon_acil_kisitli):
 *  1. Çıktı nötrdür: ida_kon_cikti sekiz komutu tam 0.0f verir (kip, silah durumu ve görevden
 *     bağımsız; değerin kısıtlayana döndüğü çağrı dahil). Bu kapı kontrolcünün iç durumuna
 *     bakmaz; is_safe ve thr_valid değişmez (A4-K55'in koşulları kapalı listedir).
 *  2. Birikimli durum sıfırdır ve birikmez: integraller, derinlik ve yön referansları, itki
 *     değişim hızı belleği (f[]) ve tahsisin doyma belleği her çağrıda (ida_kon_acil_izle;
 *     reddedilen ve out NULL çağrıda da) ve her etkin adımda (ida_kon_hesapla) sıfırlanır.
 *  3. Kalkıştan sonra çıktı sınırlı hızla açılır (mimar kararı: rampa son çıktı düzeyinde,
 *     itici komutu üzerinde; kuvvet düzeyinde değil): değer gevşedikten sonraki ilk step OK
 *     adımı kalkış adımıdır (k = 0; çoğu durumda gevşemenin olduğu çağrının kendisi). Kontrolcü
 *     ve tahsis o adımda sıfırdan başlayıp olağan biçimde koşar; yalnız son komut vektörü
 *     çarpan_k = min(1, (k + 1) · P / T) ile ölçeklenir (P: adım aralığı, T = KON_ACIL_RAMPA,
 *     ms): |çıktı_i| = çarpan · |rampasız_i|. Uygulama zamanla sayar: çarpan = (tf.t − t0) / T,
 *     t0 = kalkıştan önceki son adımın zamanı (aralık 50 ms'den uzunsa kalkış − 50 ms; önceki
 *     adım yoksa kalkış adımının kendisi: çarpan o adımda 0); rampa içinde çarpan 1 − 2⁻²²
 *     ile küçültülür ki zarf yuvarlamayla da aşılmasın. Geçen süre ≥ T olunca çıktı
 *     rampasız değerle bit bit aynıdır. Kontrolcünün iç durumu (integraller, f[], doyma belleği)
 *     rampa boyunca ölçeksiz koşar: T = 0 ile koşan eş kontrolcününkiyle her adımda aynıdır.
 *     Ölçekli komut ESC'nin dur ölü bandına düşebilir (|u| < THR_U_MIN): o iticide itki rampanın
 *     başında yoktur; komut değiştirilmez. Referanslar kalkış adımında ölçülen durumdan kurulur
 *     (d_ref_var = psi_ref_var = 0). T = 0: çarpan her adımda 1. SESSION_START süren rampayı
 *     baştan başlatır (zaman tabanı yenilenir).
 *  4. Sim tarafındaki nötrlemeden (A1-K85) bağımsızdır ve onun yerine geçmez.
 * Silah durumu, kip ve saklı hedefler bu kuralla değişmez. Kuşkuda kısıtlayan: etkin değerin
 * tanımsız biti varsa ya da mandal (acil_mandal) tutarsızsa çıktı nötrdür.
 *
 * MANUAL komutu (A4-K49) bu sürümde uygulanmaz: UNSUPPORTED / NONE (A4-K124).
 */
#ifndef IDA_CORE_KONTROL_H
#define IDA_CORE_KONTROL_H

#include <stdbool.h>
#include <stdint.h>

#include "cerceve/komut.h"
#include "emniyet/emniyet.h"
#include "ida/port.h"
#include "kontrol/tahsis.h"
#include "param/param.h"

/* ---- İstek (görev/güdüm → kontrol) ------------------------------------------------------ */

/* Rejim: hangi eksenler denetlenir. */
#define IDA_KON_KIP_KAPALI 0u /* itki sıfır                                                */
#define IDA_KON_KIP_YUZEY  1u /* yüzey: X, Y, N; dikey iticiler kapalı (02 §2.5)           */
#define IDA_KON_KIP_DALIS  2u /* dalış: altı eksen                                         */

/* Dikey kanal (yalnız DALIS). */
#define IDA_KON_DIKEY_DERINLIK 0u /* derinlik tut: derinlik (m)                            */
#define IDA_KON_DIKEY_HIZ      1u /* derinlik değişim hızı: dikey_hiz (m/s, aşağı +)       */
#define IDA_KON_DIKEY_KUVVET   2u /* doğrudan gövde Z kuvveti: kuvvet[2] (N)               */

/* Yalpa ve baş-kıç kanalı (yalnız DALIS). */
#define IDA_KON_TUTUM_ACI    0u /* yalpa, bas_kic (rad)                                    */
#define IDA_KON_TUTUM_HIZ    1u /* yalpa_hizi, bas_kic_hizi: gövde p, q (rad/s)            */
#define IDA_KON_TUTUM_MOMENT 2u /* kuvvet[3], kuvvet[4] (N·m)                              */

/* Yön kanalı. */
#define IDA_KON_YON_ACI    0u /* yon (rad, NED'de kuzeyden saat yönünde)                   */
#define IDA_KON_YON_HIZ    1u /* yon_hizi: gövde r (rad/s)                                 */
#define IDA_KON_YON_MOMENT 2u /* kuvvet[5] (N·m)                                           */

/* Yatay kanal. */
#define IDA_KON_YATAY_HIZ    0u /* hiz[0] ileri, hiz[1] yan (m/s, yön çerçevesinde)        */
#define IDA_KON_YATAY_KUVVET 1u /* kuvvet[0], kuvvet[1] (N, gövde)                         */

typedef struct {
    uint8_t kip;         /* IDA_KON_KIP_*                                                  */
    uint8_t dikey;       /* IDA_KON_DIKEY_*                                                */
    uint8_t tutum;       /* IDA_KON_TUTUM_*                                                */
    uint8_t yon_kanali;  /* IDA_KON_YON_*                                                  */
    uint8_t yatay;       /* IDA_KON_YATAY_*                                                */
    float derinlik;      /* m, yüzeyden aşağı (< 0 ise 0 alınır)                           */
    float dikey_hiz;     /* m/s, aşağı +                                                   */
    float yalpa;         /* rad                                                            */
    float bas_kic;       /* rad                                                            */
    float yalpa_hizi;    /* rad/s, gövde p                                                 */
    float bas_kic_hizi;  /* rad/s, gövde q                                                 */
    float yon;           /* rad                                                            */
    float yon_hizi;      /* rad/s, gövde r                                                 */
    float hiz[2];        /* m/s: ileri, yan (yön çerçevesi)                                */
    float kuvvet[6];     /* N, N·m: gövde X, Y, Z, K, M, N (yalnız KUVVET/MOMENT seçilen
                            eksenlerde okunur)                                             */
    uint8_t dalis_yasak; /* 1: derinleşme yasak (sızıntı algılayıcısı bayat, 02 §4.3; sol
                            W3-zincir 2. tur bulgu 2; EMNİYET): derinlik referansı ölçülen
                            derinlikten derin olamaz, aşağı yönlü dikey hız isteği üretilmez,
                            NED aşağı kuvvet yüzerlik dengelemesiyle (BUOY_FF, ≥ 0) sınırlıdır,
                            dikey integral aşağı yönde birikmez                            */
} ida_kon_istek_t;

/* ---- Bir adımın girdileri ---------------------------------------------------------------- */

typedef struct {
    ida_kon_istek_t istek;
    ida_nav_out_t nav;       /* kestirilen durum (A4-K87)                                  */
    int64_t t_ns;            /* adımın zaman çerçevesi (tf.t, ns)                          */
    bool armed;              /* emniyet modülünün armed'ı                                  */
    bool kisit;              /* itkisiz arıza eylemi: itki sıfır (ida_kon_girdi_topla:
                                fs_level ≥ PASSIVE_SURFACE)                                */
    bool acil;               /* etkin acil durdurma kısıtlayan (ida_kon_acil_kisitli; W5-K26;
                                EMNİYET): itki sıfır, döngü durumu sıfır, kalkış rampası
                                kurulur                                                    */
    bool kestirim_yeterli;   /* ida_kon_adim: est_level OK ya da DEGRADED                  */
    uint8_t itici_disi;      /* görevden devre dışı itici maskesi (THR_OFF_MASK ile VEYA)  */
    bool ref_yenile;         /* kontrolün iptal biti kurulu (ida_emn_t.hedef_iptal,
                                IDA_EMN_IPTAL_KON; sol bulgu 2 ve 3. tur bulgu 1; EMNİYET):
                                hedefin kaynağı değişti (tutma hedefinin yeniden yakalanması,
                                arıza eylemi geçişi, güdüm hedefinin iptali, derinleşme
                                yasağının başlaması, daraltma); derinlik ve yön referansları
                                ölçülen durumdan yeniden kurulur, öteleme integralleri
                                sıfırlanır. Bit kontrol belleği sıfırlandıktan sonra temizlenir */
} ida_kon_girdi_t;

/* ---- Durum ------------------------------------------------------------------------------- */

#define IDA_KON_GEO_SAYI   48u /* 0x0501–0x0530 */
#define IDA_KON_MODEL_SAYI 8u  /* 0x0531–0x0538 */

typedef struct {
    /* Tahsisin önbelleği: parametreler ya da devre dışı maskesi değişince yeniden kurulur. */
    ida_tah_t tah;
    uint32_t onb_geo[IDA_KON_GEO_SAYI];
    uint32_t onb_model[IDA_KON_MODEL_SAYI];
    uint8_t onb_disi;
    uint8_t onb_var;         /* 1: önbellek geçerli                                        */

    /* Önceki etkin adımın kanal seçimleri (kanal değişince integral ve referans sıfırlanır);
       255: etkin değil. */
    uint8_t son_kip;
    uint8_t son_dikey;
    uint8_t son_tutum;
    uint8_t son_yon;
    uint8_t son_yatay;

    /* Referanslar (basamak yumuşatma). */
    uint8_t d_ref_var;
    uint8_t psi_ref_var;
    float d_ref;             /* m                                                          */
    float psi_ref;           /* rad                                                        */

    /* İntegraller (çıkış biriminde). */
    float i_w;               /* N, NED aşağı                                               */
    float i_pqr[3];          /* N·m, gövde K, M, N                                         */
    float i_v[2];            /* N, yön çerçevesi ileri, yan                                */

    /* Son adımın çıktısı ve tahsis kalıntısı (sarma önlemi). */
    float f[IDA_PORT_N_THR];       /* N                                                    */
    float thr[IDA_PORT_N_THR];     /* normalleştirilmiş ESC komutu                         */
    int8_t doyma[IDA_TAH_EKSEN];   /* −1, 0, +1: istek elde edilenden o yönde büyük        */
    uint8_t cokme;                 /* 1: son adımda sayısal çökme                          */

    uint8_t t_var;
    /* Acil durdurma (W5-kontrol-acil; EMNİYET). acil_mandal: 1 = etkin acil durdurma kısıtlayan
       görüldü ve o günden beri rampa başlatılmadı (0 dışındaki her değer kurulu sayılır);
       çıktı nötrdür. rampa_var: 1 = kalkış rampası sürüyor, rampa_t0_ns rampanın sayıldığı
       başlangıçtır (kalkıştan önceki son adımın zaman çerçevesi; tf.t, ns). */
    uint8_t acil_mandal;
    uint8_t rampa_var;
    int64_t t_onceki_ns;
    int64_t rampa_t0_ns;

} ida_kon_t;

void ida_kon_kur(ida_kon_t *k);
/* SESSION_START: döngü durumu sıfırlanır (önbellek korunur); süren kalkış rampası baştan
   başlar (acil durdurma mandalı ve rampa başka türlü değişmez). */
void ida_kon_oturum_basi(ida_kon_t *k);
/* MANUAL (tablo 3.17; A4-K49): bu sürümde UNSUPPORTED. */
void ida_kon_komut(ida_kon_t *k, const ida_komut_baglam_t *b, const ida_cmd_t *cmd,
                   ida_cmd_result_t *r);
/* Bir adımın girdilerini çekirdekten toplar (istek, nav, armed, kısıt, kestirim düzeyi). */
void ida_kon_girdi_topla(const ida_core_t *c, ida_kon_girdi_t *g);
/* Yalnız step OK'ta: ida_kon_girdi_topla + ida_kon_hesapla; ardından kontrolün iptal biti
   (c->emn.hedef_iptal, IDA_EMN_IPTAL_KON) temizlenir: bellek o adımda sıfırlanmıştır. */
void ida_kon_adim(ida_kon_t *k, ida_core_t *c);
/* Bir adım (çekirdekten bağımsız; testler doğrudan çağırır). prm: yürürlükteki küme. */
void ida_kon_hesapla(ida_kon_t *k, const ida_prm_kume_t *prm, const ida_kon_girdi_t *g);
/* Son adımın itki önerisi (A5 itici sırası, [−1, 1]); sayısal çökmede *cokme = true. Acil
   durdurma kapısını İÇERMEZ: çerçeve ida_kon_cikti'yi kullanır. */
void ida_kon_itki(const ida_kon_t *k, float thr[IDA_PORT_N_THR], bool *cokme);

/* ---- Acil durdurma (W5-kontrol-acil; EMNİYET) -------------------------------------------- */

/* Etkin acil durdurma değeri kısıtlayan mı (A4-K116; bayatlık W5-K4 ile aynı bite düşer).
   Tanımsız bit taşıyan (bozuk) değer de kısıtlayandır. Yalnız okur. */
bool ida_kon_acil_kisitli(const ida_emn_t *e);
/* Her step/tick çağrısında, (b) ve (b3)'ten sonra (reddedilen ve out NULL çağrıda da): etkin
   değer kısıtlayansa döngü durumunu sıfırlar ve mandalı kurar. Kısıtlayan her dönem ya bu
   kancada ya da aynı çağrının ida_kon_adim'inde görülür ((e)'de başlayan dönem o çağrıda
   gevşeyemez, W5-K20 (1); sonraki çağrının kancası görür). Döngü durumu tutarsızsa
   (ida_kon_tutarli) yalnız mandal kurulur: bozulmanın kanıtı çağrının iç tutarlılık denetimine
   kalır (A4-K89). */
void ida_kon_acil_izle(ida_kon_t *k, const ida_emn_t *e);
/* Çerçevenin sürekli komut kaynağı: ida_kon_itki + acil durdurma kapısı. Etkin değer
   kısıtlayansa ya da mandal kuruluysa sekiz komut tam 0.0f'tır (kontrolcünün öteki durumundan
   bağımsız). *cokme ida_kon_itki'ninkidir. */
void ida_kon_cikti(const ida_kon_t *k, const ida_emn_t *e, float thr[IDA_PORT_N_THR],
                   bool *cokme);
bool ida_kon_tutarli(const ida_kon_t *k);
uint8_t ida_kon_param_grup(const ida_prm_kume_t *p);

#endif /* IDA_CORE_KONTROL_H */
