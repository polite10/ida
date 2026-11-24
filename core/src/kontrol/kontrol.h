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
 * komut tam 0.0f'tır ve döngü durumu sıfırlanır. Acil durdurma itki kapısında yer almaz (yalnız
 * atış kontrol izinlerini kapatır; iticileri sim keser). Kararın girdileri çerçeveden ve görev
 * modülünden gelir (ida_kon_girdi_topla); bu modül yalnız uygular. Sonlu olmayan durum ya da
 * ara sonuçta komut 0 ve *cokme = true (A4-K55).
 *
 * MANUAL komutu (A4-K49) bu sürümde uygulanmaz: UNSUPPORTED / NONE (A4-K124).
 */
#ifndef IDA_CORE_KONTROL_H
#define IDA_CORE_KONTROL_H

#include <stdbool.h>
#include <stdint.h>

#include "cerceve/komut.h"
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
                                fs_level ≥ PASSIVE_SURFACE; acil durdurma DEĞİL, W3-K1/K4) */
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
    int64_t t_onceki_ns;

} ida_kon_t;

void ida_kon_kur(ida_kon_t *k);
/* SESSION_START: döngü durumu sıfırlanır (önbellek korunur). */
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
/* Son adımın itki önerisi (A5 itici sırası, [−1, 1]); sayısal çökmede *cokme = true. */
void ida_kon_itki(const ida_kon_t *k, float thr[IDA_PORT_N_THR], bool *cokme);
bool ida_kon_tutarli(const ida_kon_t *k);
uint8_t ida_kon_param_grup(const ida_prm_kume_t *p);

#endif /* IDA_CORE_KONTROL_H */
