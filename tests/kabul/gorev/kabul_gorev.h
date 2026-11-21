/*
 * tests/kabul/gorev — W3-gorev işinin (kipler, araç silahlandırması, arıza güvenliği)
 * bağımsız kabul testleri: ortak düzenek.
 *
 * Kaynak: yalnız A4 sözleşmesi (docs/sozlesmeler/A4-port.md, 1.0), core/include/ida/port.h,
 * protocol/komutlar.json ve görev belgesi (docs/gorevler/W3-gorev.md; kabul md.2–4) ile
 * ortak kurallar (docs/gorevler/W3-ortak.md). Çekirdeğin uygulama kaynağı (core/src),
 * uygulayıcının testleri ve tasarım notu (core/TASARIM.md) okunmadı. Çekirdeğe yalnız
 * port.h'nin işlevleriyle erişilir.
 *
 * Düzenek her çağrıda aşağıdaki değişmezleri denetler (kg_cagir; ayrıntı kabul_gorev.c):
 *   D1  armed 0 → 1 ancak aynı çağrıda ACCEPTED bir ARM ile (A4-K104, görev md.3).
 *   D2  kip GUIDED/AUTO'ya ancak aynı çağrıda ACCEPTED bir SET_MODE (o hedefle) ya da
 *       ACCEPTED FAILSAFE_RESET ile girer (A4-K104, görev md.3).
 *   D3  acil durdurma aracı silahsızlandırmaz: kısıtlayanken (A4-K116 modeli), DISARM,
 *       SESSION_END ve arıza eylemi değişimi olmayan çağrıda silahlı araç silahlı kalır (W3-K1).
 *   D4  DISARM (mandal ya da cmds[]) veya SESSION_END taşıyan çağrının sonunda armed = 0 ve
 *       o çağrıdaki hiçbir ARM ACCEPTED değil (A4-K44, K45, K46, K73, K113; görev md.4).
 *   D5  geçerli hedefli daraltıcı kip taşıyan çağrının sonunda kip GUIDED/AUTO değil
 *       (A4-K44, K45, K46, K127).
 *   D6  kabul edilen çağrıda daraltıcı girdi varsa yinelenmeyen her genişletici
 *       REJECTED / NARROWED; reddedilen çağrıda daraltıcı olmayan her komut
 *       REJECTED / CALL_REJECTED (A4-K45, K114).
 *   D7  yineleme penceresi: kesin yinelenen kimlik DUPLICATE, kesin yeni kimlik DUPLICATE
 *       değil (A4-K48; pencerenin iki okuması da modellenir, yalnız ikisinin uyuştuğu
 *       yerde beklenti yazılır).
 *   D8  her ARM sonucunun izinli kümede olması (3.17 satır sırası, 4.14 öncelik; ön koşullar
 *       mimar kararı W3-K3) ve ACCEPTED ARM'dan sonra armed = 1.
 *   D9  out.armed = status.armed, out.fs_level = status.fs_level (3.8).
 */
#ifndef KABUL_GOREV_H
#define KABUL_GOREV_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "ida/port.h"

#define KG_MS INT64_C(1000000)
#define KG_T0 INT64_C(1000000000) /* init'in duvar saati: 1 s */
#define KG_P (5 * KG_MS)          /* kontrol periyodu (iskelet §4) */
#define KG_BOOT UINT32_C(7)

/* Hazır olma için en çok adım (20 s); kestirim yakınsamasının süresi sözleşmede yoktur,
   bu sınır düzeneğin seçimidir. */
#define KG_HAZIR_AZAMI 4000

/* Gevşeten tüp içeriği: dolu, fiş takılı, pim çıkık (A4 §3.3). */
#define KG_TUP_ACIK ((uint8_t)(IDA_TUBE_LOADED | IDA_TUBE_PLUG_IN | IDA_TUBE_PIN_OUT))

/* ---- Hata iletisi ---------------------------------------------------------------- */

_Noreturn void kg_basarisiz(const char *dosya, int satir, const char *bicim, ...);

#define KG_DOGRULA(kosul, ...)               \
    if (!(kosul)) {                          \
        kg_basarisiz(__FILE__, __LINE__, __VA_ARGS__); \
    } else                                   \
        ((void)0)

void kg_baglam(const char *bicim, ...);

/* ---- Sözde rastgele sayı (splitmix64; sabit tohum) -------------------------------- */

typedef struct {
    uint64_t s;
} kg_rng_t;

void kg_rng_kur(kg_rng_t *r, uint64_t tohum);
uint32_t kg_rng(kg_rng_t *r, uint32_t n); /* [0, n) */
bool kg_olasi(kg_rng_t *r, uint32_t binde);

/* ---- Kurulum ---------------------------------------------------------------------- */

typedef enum { KG_BLOK_GECERLI = 0, KG_BLOK_YOK = 1, KG_BLOK_BOZUK = 2 } kg_blok_t;

typedef struct {
    kg_blok_t blok;
    bool boot_ok;
    uint32_t boot_count;
} kg_kurulum_t;

void kg_kurulum_varsayilan(kg_kurulum_t *k);
/* Örneği kurar (init) ve modeli sıfırlar; beklenen dönüşü denetler (A4-K78, K80). Bir
   sonraki çağrı SESSION_START taşır (A4-K72). */
void kg_kur(const kg_kurulum_t *k);
void kg_kur_varsayilan(void);

/* ---- Dünya: sensör akışının ve platformun durumu ---------------------------------- */

typedef struct {
    bool link;           /* plat.link_state RUNNING */
    uint8_t pstore;      /* plat.param_store */
    uint32_t pgen;       /* plat.param_gen */
    bool imu;            /* olağan çağrı step (IMU'lu); değilse zaman çerçeveli tick */
    bool mag, baro, gnss, dvl, ping1d, batt, sizinti_olcer;
    bool emniyet;        /* periyodik inst 0 emniyet örneği */
    bool hb;             /* periyodik HEARTBEAT */
    uint8_t estop;       /* periyodik emniyet örneğindeki estop */
    uint8_t tube[2];     /* periyodik emniyet örneğindeki tüp bitleri */
    uint8_t sizinti;     /* sızıntı maskesi */
    float batt_v;        /* V */
    bool kestirim_bozuk; /* IMU ile manyetometre ve basınç birbiriyle çelişir */
} kg_dunya_t;

kg_dunya_t *kg_dunya(void);
void kg_dunya_olagan(void);

/* ---- Girdi kurma ------------------------------------------------------------------ */

typedef enum {
    KG_OLAGAN = 0,    /* dünyaya göre step ya da zaman çerçeveli tick */
    KG_TICK_TF = 1,   /* zaman çerçeveli tick */
    KG_IZGARA = 2,    /* ızgara tick'i */
    KG_YINELENEN = 3, /* tf.seq = son kabul (DUP_SEQ) */
    KG_ESKI = 4,      /* tf.seq son kabulden eski (OLD_SEQ) */
    KG_OTURUM_SONU = 5 /* A4-K122 oturum sonu çağrısı */
} kg_tur_t;

/* Girdiyi düzeneğin tamponunda kurar ve döndürür (dünyanın periyodik örnekleri ve
   HEARTBEAT'i dahil). */
ida_in_t *kg_girdi(kg_tur_t tur);
/* Yeni kimlikli komut ekler. */
ida_cmd_t *kg_komut(ida_in_t *in, uint8_t type);
ida_cmd_t *kg_komut_id(ida_in_t *in, uint8_t type, uint32_t cmd_id);
ida_cmd_t *kg_kip_komutu(ida_in_t *in, uint8_t mode);
uint32_t kg_yeni_id(void);
/* Mandal biti kurar (kimlik ayrı aralıktan). */
void kg_mandal(ida_in_t *in, uint8_t bit, uint8_t mode);
/* Emniyet örneği ekler (gecerli = 0: hdr.flags'te VALID yok). */
ida_sample_t *kg_emniyet(ida_in_t *in, uint8_t inst, uint8_t estop, uint8_t tube0, uint8_t tube1,
                         bool gecerli);
ida_sample_t *kg_ornek(ida_in_t *in, uint8_t type, uint8_t inst);
/* Girdide o türün inst 0 örneği varsa onu, yoksa yeni örneği döndürür. */
ida_sample_t *kg_ornek_bul_ya_da_ekle(ida_in_t *in, uint8_t type);

/* ---- Çağrı ------------------------------------------------------------------------ */

ida_rc_t kg_cagir(ida_in_t *in);
ida_rc_t kg_adim(void); /* bir olağan çağrı */
void kg_adimlar(int n);
const ida_out_t *kg_out(void);
const ida_out_t *kg_onceki(void);
const ida_in_t *kg_son_girdi(void);
ida_rc_t kg_son_rc(void);
/* Son çağrıda cmds[j]'nin sonucu (A4-K43: mandal bitleri önce, sonra komutlar). */
const ida_cmd_result_t *kg_sonuc(int j);
/* Son çağrıda type türündeki ilk komutun dizini; yoksa -1. */
int kg_komut_dizini(uint8_t type);

/* ---- Model ------------------------------------------------------------------------ */

bool kg_kisit_estop(void);   /* etkin acil durdurma kısıtlayan (A4-K116 modeli) */
/* Çekirdek bir emniyet gevşetmesini günlükle gösterdi mi (gevşetme W4'te; W2-cekirdek M9). */
bool kg_gevsetme_gozlendi(void);
bool kg_hazir(void);         /* son çıktıda silahlandırmanın gözlenen ön koşulları */
const char *kg_hazir_neden(void);
/* Hazır olana (art arda 10 çağrı) kadar olağan adım; en çok azami adım. */
bool kg_hazirla(int azami);
/* Hazır değilse ayrıntılı iletiyle başarısız olur. */
void kg_hazir_olmali(int azami, const char *amac);

/* Daraltıcı kip seçimi (NARROW_MODE) ve sonucunun denetimi (hazır durumda). */
void kg_kip_sec(uint8_t mode);
/* GUIDED'e geçiş (genişletici SET_MODE); ACCEPTED bekler (hazır durumda; 3.17). */
void kg_guided(void);
/* ARM gönderir; ACCEPTED ve armed = 1 bekler. Kimliği döndürür. */
uint32_t kg_silahlan(void);
/* DISARM mandalı gönderir; armed = 0 bekler. */
void kg_silahsizlan(void);

/* Anlık görüntü: örnek belleği aynı adreste geri yüklenir, model ve dünya ile birlikte. */
void kg_anlik_al(void);
void kg_anlik_don(void);

/* Sayaçlar (kapsam). */
typedef struct {
    uint32_t cagri;
    uint32_t silahli_cagri;
    uint32_t arm_kabul;
    uint32_t arm_ret;
    uint32_t estop_kisitli_arm;   /* kısıt varken ARM içeren çağrı */
    uint32_t estop_kisitli_silahli_once; /* kısıt geldiğinde araç silahlıydı */
    uint32_t disarm_ve_arm;       /* DISARM ile aynı çağrıda ARM */
    uint32_t yinelenen;           /* kesin yinelenen kimlik */
    uint32_t guided_giris;
} kg_sayac_t;

kg_sayac_t *kg_sayac(void);

#endif /* KABUL_GOREV_H */
