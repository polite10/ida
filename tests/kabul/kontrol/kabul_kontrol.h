/*
 * tests/kabul/kontrol — acil durdurmada çekirdeğin aktüatör çıktısının bağımsız kabul testleri
 * (W5-kontrol-acil; EMNİYET): ortak düzenek.
 *
 * Kaynak (yalnız bunlar): docs/gorevler/W5-kontrol-acil.md ("Kural", "Kabul", "Bağımsız kabul
 * testleri"); A4 sözleşmesi (docs/sozlesmeler/A4-port.md: sürekli komut alanları 3.8, 4.6,
 * A4-K28, K53–K59; nötr güvenli çıktı tanımı (Bölüm 2, A4-K55); emniyet örneği ve etkin değerler
 * A4-K34, K36, K116; parametre komutları A4-K81, K85) ve core/include/ida/port.h;
 * docs/gorevler/W5-ortak.md W5-K4 (+ eki), W5-K20, W5-K26; var olan tests/kabul/cekirdek ve
 * tests/kabul/atis düzenekleri. Çekirdeğin uygulama kaynağı (core/src) ve birim testleri
 * (core/test) okunmadı; çekirdeğe yalnız A4 portunun işlevleri üzerinden erişilir (kara kutu).
 *
 * Düzenek tests/kabul/atis'in dünyasının (sensör akışı, kalp atışı, inst 0 emniyet örneği,
 * sahte alıcı, emniyet değişmezleri) ve onun altındaki tests/kabul/cekirdek'in genel
 * değişmezlerinin üstüne kurulur. Bu dosyanın eklediği:
 *
 *   - testin KENDİ etkin acil durdurma modeli (yalnız girdiden hesaplanır; çıktının ESTOP
 *     bitine ya da kipine dayanmaz). Model "kesin kısıtlayan" kümesini verir: sözleşmenin
 *     makul her okumasında kısıtlayan olan çağrılar. Kurallar:
 *       * init'te kısıtlayan (A4-K116; görev belgesi "init'ten sonra hiç gevşememiş durum");
 *       * bir çağrıda yapısal olarak geçerli (type SAFETY, inst < 4) herhangi bir emniyet
 *         örneğinin estop ≠ 0 içeriği — kabul edilen, A4-K33 ile reddedilen (VALID = 0),
 *         inst ≠ 0 ya da reddedilen bir çağrıdaki (DUP_SEQ, BAD_INPUT) — o çağrının sonunda
 *         kısıtlayan yapar (A4-K116);
 *       * SESSION_END kısıtlayana döndürür (A4-K116);
 *       * gevşetme yalnız kabul edilen bir çağrıda, kabul edilebilir bir inst 0 örneğinin
 *         estop = 0 içeriğiyle ve o çağrıda kısıtlayan içerik yokken olur (A4-K116; W5-K20 (1));
 *         plat.link_event taşıyan çağrıda gevşetme yoktur (W5-K20 (2));
 *       * bayatlık (W5-K4 ve eki): son kabul edilen inst 0 örneğinin alımından ya da ölçüm
 *         zamanının son ilerlediği örneğin alımından bu yana geçen duvar süresi T_emn_bayat'ı
 *         KESİN aşarsa (>) acil durdurma basılı sayılır. "Kesin" için model gevşek yönde okur:
 *         süre init/SESSION_START anından önce başlamaz, eşitlik bayat sayılmaz, bayatlık
 *         mandallanmaz (taze örnek gelince kalkar) ve W5-K20 (1)'in "bayatlığın o çağrıda
 *         saptanması" ile aynı çağrıdaki taze gevşek örneğin gevşetmemesi modele girmez. Bu
 *         gevşek okumaların her biri kesin kısıtlayan kümeyi yalnız küçültür (yanlış düşme
 *         üretmez).
 *   - her çağrıda denetlenen kural (görev belgesi, Kural 1): model kesin kısıtlayan derken
 *     çıktının sekiz thr_cmd'si tam 0.0f'tır (A4'ün "nötr güvenli çıktı" değerleri, A4-K55);
 *     kip, silah durumu, komutlar ve dönüş kodu ne olursa olsun. OK dönen step'te thr_valid = 1
 *     olması (4.14) cekirdek düzeneğinin genel denetimindedir; böylece nötr değer platformda
 *     yürürlüğe girer.
 *   - operatör: hedef kipte her çağrıda sürekli komut (GUIDED'da GUIDED_SP, MANUAL / STABILIZE /
 *     DEPTH_HOLD'da MANUAL eksenleri) ve hazırlık yardımcıları (oturum, ARM, kip).
 *
 * Bütün çağrılar kk_cagir'dan geçmelidir (modelin girdiyi görmesi için); atış düzeneğinin
 * kendi içinde adım atan hazırlık yardımcıları (ka_oturum_ac, ka_arac_silahla, ka_kip_sec)
 * kullanılmaz, karşılıkları burada yazıldı.
 */
#ifndef KABUL_KONTROL_H
#define KABUL_KONTROL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "kabul_atis.h"

/* Görev belgesinde sabitlenen rampa parametresi (W5-kontrol-acil, "Bağımsız kabul testleri"):
   varsayılan 500 ms (tahmin), aralık 0–5 000 ms, 0 = rampa yok, sınıf B (Kural 3: IDA_PCLS_B). */
#define KK_RAMPA_AD "KON_ACIL_RAMPA_MS"
#define KK_RAMPA_VARSAYILAN_MS 500u
#define KK_RAMPA_AZAMI_MS 5000u

/* Çağrı başına süre (iskelet §4; düzenekte 5 ms). */
#define KK_P KA_P
/* Saniyedeki çağrı sayısı. */
#define KK_SN 200

/* ---- Etkin acil durdurma modeli -------------------------------------------------------------- */

typedef enum {
    KK_N_YOK = 0,
    KK_N_INIT,         /* init'ten beri gevşetilmedi                                     */
    KK_N_BASILI,       /* kabul edilebilir inst 0 örneği estop = 1                        */
    KK_N_GECERSIZ,     /* A4-K33 ile reddedilen (VALID = 0) örneğin kısıtlayan içeriği     */
    KK_N_INST,         /* inst ≠ 0 örneğin kısıtlayan içeriği                              */
    KK_N_RED,          /* reddedilen çağrıdaki (DUP_SEQ / BAD_INPUT) örneğin içeriği       */
    KK_N_OTURUM_SONU,  /* SESSION_END                                                    */
    KK_N_BAGLANTI,     /* bağlantı olayı çağrısında gevşetme yok (W5-K20 (2))              */
    KK_N_BAYAT_ALIM,   /* W5-K4 eki (i): alımdan beri T_emn_bayat                          */
    KK_N_BAYAT_OLCUM,  /* W5-K4 eki (ii): ölçüm zamanı T_emn_bayat boyunca ilerlemedi      */
    KK_N_SAYI
} kk_neden_t;

const char *kk_neden_adi(int n);

/* Son çağrının sonunda model kesin kısıtlayan mı; nedeni. */
bool kk_model_kisitlayan(void);
kk_neden_t kk_model_neden(void);
/* Bayatlık süresi (ns): şemada "T_emn_bayat" varsa onun değeri, yoksa W5-K4 varsayılanı. */
int64_t kk_t_bayat(void);

/* Denetimi kapat/aç (yalnız tanı için; hiçbir test kapatmıyor). */
void kk_denetim_ac(bool acik);

/* Sayaçlar (özellik testinin kapsam koşulu için). */
typedef struct {
    uint64_t cagri;
    uint64_t kisitlayan;            /* model kesin kısıtlayan                              */
    uint64_t notr_disi;             /* çıktıda en az bir thr_cmd ≠ 0                       */
    uint64_t notr_disi_gevsek;      /* model gevşekken nötr dışı çıktı                     */
    uint64_t gecis;                 /* gevşek → kesin kısıtlayan geçişi                    */
    uint64_t gecis_notr_disi;       /* geçişten önceki çağrının çıktısı nötr dışıydı      */
    uint64_t gecis_neden[KK_N_SAYI];/* geçişin nedeni                                     */
    uint64_t kisit_neden[KK_N_SAYI];/* kesin kısıtlayan çağrının nedeni                    */
    uint64_t kalkis;                /* kesin kısıtlayan → gevşek                           */
} kk_sayac_t;
const kk_sayac_t *kk_sayac(void);

/* ---- Kurulum ve çağrı ----------------------------------------------------------------------- */

/* Tek örnekli ortam, geçerli blokla init (ka_kur), model ve operatör sıfırlanır. */
void kk_kur(void);

/* Girdi kurucular (ka_girdi* sarmalayıcıları); çağrı kk_cagir ile yapılır. */
ida_in_t *kk_girdi(void);
ida_in_t *kk_girdi_seq(uint32_t seq);
ida_in_t *kk_girdi_oturum_sonu(void);
/* Son kk_girdi*'nin girdisini çağırır; modeli günceller; kesin kısıtlayansa nötr denetler. */
ida_rc_t kk_cagir(void);
/* kk_girdi + operatör komutları (kk_operator_ekle) + kk_cagir. */
ida_rc_t kk_adim(void);
void kk_adimlar(int n);

/* ---- Operatör ------------------------------------------------------------------------------ */

typedef struct {
    bool acik;               /* sürekli komut gönderilsin mi                              */
    ida_guided_sp_t sp;      /* GUIDED'da her çağrıda                                     */
    float eksen[6];          /* MANUAL / STABILIZE / DEPTH_HOLD'da her çağrıda            */
} kk_operator_t;

extern kk_operator_t kk_op;

/* Varsayılan operatör: büyük konum, derinlik ve yön hatası veren GUIDED setpoint'i; eksenler
   nötr dışı. */
void kk_operator_varsayilan(void);
/* Çıktının kipine göre sürekli komutu girdiye ekler (yer varsa). */
void kk_operator_ekle(ida_in_t *in);

/* ---- Hazırlık ------------------------------------------------------------------------------- */

/* SESSION_START ve kip/kestirim INIT'ten çıkana kadar adım (en çok 10 s); çıkmazsa düşer. */
void kk_oturum_ac(void);
/* ARM ister; en çok `deneme` kez; silahlanırsa true. */
bool kk_silahla(int deneme);
/* Kip: GUIDED için SET_MODE (genişletici), diğerleri için kip mandalı (A4-K46); en çok 10 s.
   Kipe geçilirse true. */
bool kk_kip(uint8_t kip);
/* kk_kur + oturum + ARM + kip; ARM ya da kip olmazsa düşer (ön koşul). */
void kk_hazir(uint8_t kip);

/* ---- Çıktı ----------------------------------------------------------------------------------- */

/* Sekiz thr_cmd tam 0.0f mı (A4-K55 nötr güvenli çıktı değerleri). */
bool kk_notr(const ida_out_t *o);
/* En büyük |thr_cmd|. */
float kk_sapma(const ida_out_t *o);
float kk_mutlak(float x);
/* Son çıktı nötr değilse kanallarıyla düşer. */
void kk_notr_bekle(int satir, const char *ne);
#define KK_NOTR(ne) kk_notr_bekle(__LINE__, (ne))
/* Çıktının sürekli komut özeti (hata iletileri için). */
const char *kk_thr_yazi(const ida_out_t *o);

/* ---- Parametre ------------------------------------------------------------------------------- */

/* Adla tanımlayıcı; yoksa false. */
bool kk_param_bul(const char *ad, ida_param_desc_t *d);
/* Değeri tamsayıya çevirir (U32, I32, F32). */
int64_t kk_param_tamsayi(const ida_param_value_t *v);
/* Tanımlayıcının türünde değer. */
ida_param_value_t kk_param_deger(const ida_param_desc_t *d, int64_t x);
/* Örnek 0'daki yürürlükteki değer (tamsayı). */
int64_t kk_param_oku(const ida_param_desc_t *d);
/* PARAM_SET + PARAM_COMMIT tek çağrıda; iki komutun kimliklerini döndürür. */
void kk_param_gonder(const ida_param_desc_t *d, int64_t x, uint32_t *set_id, uint32_t *commit_id);

#endif /* KABUL_KONTROL_H */
