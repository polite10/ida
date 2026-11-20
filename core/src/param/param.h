/*
 * param.h — parametre tablosu, kodlanmış blok ve parametre komutları (W2; A4 §3.10, §4.9).
 *
 * Tablo X-makrodan kurulur (param/param_listesi.inc): her parametrenin bir indisi
 * (IDA_PRM_<alan>) vardır ve değerler indisle tutulan ham 32 bittir (I32 ikiye tümleyen,
 * U32, F32 binary32 bitleri; blok kaydıyla aynı; A4-K79). Tablo kimliğe göre kesin artandır.
 * Değer okumak: ida_prm_u32(&c->prm, IDA_PRM_imu_stale_ms) gibi.
 */
#ifndef IDA_CORE_PARAM_H
#define IDA_CORE_PARAM_H

#include <stdbool.h>
#include <stdint.h>

#include "cerceve/komut.h"
#include "ida/port.h"
#include "telemetri/telemetri.h"

enum ida_prm_indis {
#define IDA_PARAM(kimlik, ad, tip, sinif, en_az, en_cok, varsayilan, alan) IDA_PRM_##alan,
#include "param/param_listesi.inc"
#undef IDA_PARAM
    IDA_PRM_SAYI
};

#define IDA_PRM_BLOB_BOYU (IDA_PBLOB_HDR_SIZE + (IDA_PBLOB_REC_SIZE * (uint32_t)IDA_PRM_SAYI))
_Static_assert(IDA_PRM_BLOB_BOYU <= IDA_PARAM_BLOB_MAX, "parametre bloğu 8000 B'yi aşıyor");

/* Bir parametre kümesi: tablo sırasıyla ham değerler. */
typedef struct {
    uint32_t ham[IDA_PRM_SAYI];
} ida_prm_kume_t;

typedef struct {
    ida_prm_kume_t yururluk;              /* uygulanan değerler                       */
    ida_prm_kume_t bekleyen;              /* PARAM_SET ile bekletilen (A4-K81)       */
    bool bekleyen_var[IDA_PRM_SAYI];
    bool param_ok;                        /* A4-K78                                   */
    bool commit_uygulandi;                /* init'ten beri ACCEPTED olmuş PARAM_COMMIT */
    bool kayit_bekliyor;                  /* kabul edilmiş PARAM_SAVE'in sonucu bekleniyor */
    uint32_t kayit_gen;                   /* son kabul edilen PARAM_SAVE'in gen'i; 0: yok */
    bool son_store_var;
    uint8_t son_store;                    /* PARAM_SAVE günlüğü için plat.param_store izi */
    /* Çağrı içi (her kabul edilen çağrının başında sıfırlanır): PARAM_COMMIT ve PARAM_SAVE
       sonuçları çağrı sonunda yazılır (A4-K81, K82); burada sonuç indisleri tutulur. */
    uint8_t cagri_save_sayisi;            /* bu çağrıda (3)'e ulaşan PARAM_SAVE sayısı */
    uint8_t n_commit;                     /* bu çağrıda (3)'e ulaşan PARAM_COMMIT sayısı */
    uint8_t commit_indis[IDA_PORT_MAX_CMDS]; /* out->results[] indisleri              */
    bool save_bekliyor;                   /* ilk PARAM_SAVE ön koşulları geçti, karar sonda */
    uint8_t save_indis;
    uint8_t blob[IDA_PRM_BLOB_BOYU];      /* son PARAM_SAVE bloğu (A4-K82, A4-K100)  */
} ida_param_t;

/* ---- Tablo (örneksiz) -------------------------------------------------------------------- */
uint16_t ida_prm_sayi(void);
/* indis < sayı değilse NULL. */
const ida_param_desc_t *ida_prm_tanim(uint16_t indis);
bool ida_prm_indis(uint16_t kimlik, uint16_t *indis);
void ida_prm_varsayilan(ida_prm_kume_t *k);
/* Ham değer tanımın min–max aralığında mı (F32 ayrıca sonlu). */
bool ida_prm_deger_gecerli(const ida_param_desc_t *d, uint32_t ham);
/* Ham değeri tanımın kimliği ve türüyle ida_param_value_t'ye yazar. */
void ida_prm_deger_yaz(const ida_param_desc_t *d, uint32_t ham, ida_param_value_t *v);
/* Kanonik şema metninin SHA-256 özetinin ilk 8 baytı, küçük uçlu (A4 §3.10).
   Metin: kimlik artan sırada her parametre için "<id>\t<ad>\t<type>\n" (ondalık, ASCII). */
uint64_t ida_prm_sema_ozeti(void);
/* İlişkili grup denetimleri (A4-K80 md. 8): 0 geçerli; modüllerin kancalarını da çağırır. */
uint8_t ida_prm_grup(const ida_prm_kume_t *k);

/* ---- Değer okuma ------------------------------------------------------------------------- */
uint32_t ida_prm_u32(const ida_param_t *p, enum ida_prm_indis indis);
int32_t ida_prm_i32(const ida_param_t *p, enum ida_prm_indis indis);
float ida_prm_f32(const ida_param_t *p, enum ida_prm_indis indis);

/* ---- Kodlanmış blok (A4 §3.10, A4-K79, A4-K80) ------------------------------------------ */
/* Bloğu çözer ve doğrular; 0 geçerli (k yazılır), 1–8 ilk başarısız denetimin numarası
   (k yazılmaz). */
uint8_t ida_prm_blok_coz(const uint8_t *blok, uint32_t uzunluk, ida_prm_kume_t *k);
/* k'yi blok biçiminde yazar (blok en az IDA_PRM_BLOB_BOYU bayt); uzunluğu döner. */
uint32_t ida_prm_blok_kodla(const ida_prm_kume_t *k, uint8_t *blok);
/* CRC-32/ISO-HDLC. */
uint32_t ida_crc32(const uint8_t *veri, uint32_t uzunluk);

/* ---- Komutlar (A4-K81, K82, K112; tablo 3.17) ------------------------------------------- */
typedef struct {
    const ida_komut_baglam_t *k;
    uint32_t plat_param_gen;
    ida_gunluk_t *gun;
    uint8_t sonuc_indisi;                 /* komutun out->results[] indisi             */
} ida_prm_komut_baglam_t;

void ida_prm_cagri_basi(ida_param_t *p);
/* PARAM_SET, PARAM_GET hemen; PARAM_COMMIT ve PARAM_SAVE'in sırasından bağımsız kararı
   ida_prm_cagri_sonu'dadır (bu işlev yalnız sırayla belirlenen ret nedenlerini — SAVE'de
   ONE_PER_CALL, ARMED, PARAM_BUSY — yazar, diğerlerini geçici ACCEPTED bırakıp indisini
   kaydeder). r->cmd_id ve r->kind çağıranca yazılmıştır. */
void ida_prm_komut(ida_param_t *p, const ida_prm_komut_baglam_t *b, const ida_cmd_t *cmd,
                   ida_cmd_result_t *r);
/* Kabul edilen çağrının sonu (adım sınırı; A4-K81, K82): bu çağrının PARAM_COMMIT'lerini tek
   commit olarak değerlendirir, aynı sonucu hepsine yazar, PARAM_COMMIT günlüklerini üretir ve
   kabul edilirse uygular; sonra bekleyen PARAM_SAVE'in PARAM_INVALID ve gen doyması
   koşullarını değerlendirir ve kabul edilirse commit sonrası kümeyi kodlar. Reddedilen her
   sonuç için cnt->cmd_rejected bir artar. */
void ida_prm_cagri_sonu(ida_param_t *p, const ida_prm_komut_baglam_t *b, ida_out_t *out,
                        ida_core_counters_t *cnt);
/* plat.param_store geçişleri: PARAM_SAVE günlüğü ve A4-K78 engelinin kalkması. */
void ida_prm_depo_izle(ida_param_t *p, const ida_plat_t *plat, ida_gunluk_t *gun, ida_wtime_t now);

#endif /* IDA_CORE_PARAM_H */
