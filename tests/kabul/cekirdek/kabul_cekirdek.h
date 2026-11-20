/*
 * tests/kabul/cekirdek — ortak çekirdeğin bağımsız kabul testleri: ortak düzenek.
 *
 * Kaynak: yalnız A4 sözleşmesi (docs/sozlesmeler/A4-port.md, 130 kurallı son metin),
 * core/include/ida/port.h, core/include/ida/algilama_kaydi.h ve görev belgesi
 * (docs/gorevler/W2-cekirdek.md). Çekirdeğin uygulama kaynağı, uygulayıcının testleri ve
 * tasarım notu okunmadı.
 *
 * Düzenek her çağrıyı KC_N_AZAMI'ye kadar örnekte aynı girdiyle yürütür. Örnek bellekleri
 * init'ten önce farklı desenlerle (0x00, 0xFF, 0x5A, sözde rastgele), çıktı tamponları her
 * çağrıdan önce farklı desenlerle (0x00, 0xFF, 0xA5, sözde rastgele) doldurulur; dönüş
 * kodlarının ve alan alan serileştirilmiş çıktıların eşitliği her çağrıda denetlenir
 * (A4-K24, A4-K27). Ayrıca her yazılabilir çıktıda genel değişmezler denetlenir
 * (kc_genel_denetim; A4-K28, K43, K45, K55–K58, K61–K65, K87–K92, K109, K114, K117,
 * K120, K121, K125, K126 ve yer tutucu çekirdeğin "genişletici yok" ilkesi).
 */
#ifndef KABUL_CEKIRDEK_H
#define KABUL_CEKIRDEK_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "ida/port.h"

#define KC_N_AZAMI 4

#define KC_US INT64_C(1000)
#define KC_MS INT64_C(1000000)
#define KC_T0 INT64_C(1000000000) /* init'in duvar saati: 1 s */
#define KC_P (5 * KC_MS)          /* kontrol periyodu (iskelet §4) */

/* Varsayılan açılış sayacı (A4-K61: 1 … 0xFFFFFFFE). */
#define KC_BOOT UINT32_C(7)

typedef enum { KC_STEP = 1, KC_TICK = 2 } kc_islev_t;

/* ---- Hata iletisi ---------------------------------------------------------------- */

_Noreturn void kc_basarisiz(int satir, const char *bicim, ...);

/* Koşul tutmazsa biçimli iletiyle başarısız olur (Unity; çağrı yerine dönmez). */
#define KC_DOGRULA(kosul, ...)                    \
    if (!(kosul)) {                               \
        kc_basarisiz(__LINE__, __VA_ARGS__);      \
    } else                                        \
        ((void)0)

/* ---- Sözde rastgele sayı (splitmix64; sabit tohum, saat yok) ---------------------- */

typedef struct {
    uint64_t s;
} kc_rng_t;

void kc_rng_kur(kc_rng_t *r, uint64_t tohum);
uint64_t kc_rng_u64(kc_rng_t *r);
uint32_t kc_rng_aralik(kc_rng_t *r, uint32_t n); /* [0, n) */

/* ---- Örnekler ve model ------------------------------------------------------------ */

/* n örnekle yeni ortam; örnek bellekleri desenlerle doldurulur, init yapılmaz. */
void kc_ortam_kur(int n_ornek);
int kc_ornek_sayisi(void);
ida_core_t *kc_core(int i);
unsigned char *kc_bellek(int i);
size_t kc_core_boyu(void);

/* Model: bir sonraki çağrının genel denetimini kapatır/açar (yalnız belirsiz grup). */
void kc_genel_denetim_ac(bool acik);

/* ---- Başlatma --------------------------------------------------------------------- */

/* Geçerli varsayılan init girdisi: sürüm, now_w = KC_T0, boot_count = KC_BOOT, blok yok. */
void kc_init_girdi(ida_init_in_t *ii);
/* Bütün örneklerde init; dönüş kodlarının eşitliği denetlenir, model güncellenir. */
ida_rc_t kc_init(const ida_init_in_t *ii);
/* Bloksuz init (beklenen PARAM_DEFAULTS). */
void kc_init_bloksuz(void);
/* Şemanın varsayılanlarından kurulan geçerli blokla init (beklenen OK). */
void kc_init_gecerli(void);
/* İlk yazılabilir çağrının PARAM_LOAD arg1 beklentisi (A4-K80 denetim numarası). */
void kc_param_load_beklenen(int32_t arg1);

/* ---- Girdi kurucular -------------------------------------------------------------- */

void kc_in_bos(ida_in_t *in);                               /* sıfır; link RUNNING */
void kc_in_step(ida_in_t *in, uint32_t seq, int64_t t);     /* now_w = tf.t = t; IMU taze */
void kc_in_tick(ida_in_t *in, uint32_t seq, int64_t t);     /* zaman çerçeveli tick */
void kc_in_izgara(ida_in_t *in, int64_t now);               /* ızgara tick'i */
void kc_in_oturum_sonu(ida_in_t *in, int64_t now);          /* A4-K122 */
void kc_imu_kur(ida_imu_sample_t *imu, uint32_t seq, int64_t t);
ida_sample_t *kc_ornek_ekle(ida_in_t *in, uint8_t type, uint8_t inst, uint32_t seq, int64_t t);
ida_sample_t *kc_emniyet_ekle(ida_in_t *in, uint8_t inst, uint32_t seq, int64_t t, uint8_t estop,
                              uint8_t tube0, uint8_t tube1);
ida_cmd_t *kc_komut_ekle(ida_in_t *in, uint32_t cmd_id, uint8_t type);
ida_cmd_t *kc_fc_ekle(ida_in_t *in, uint32_t cmd_id, uint8_t op, uint8_t tube);
void kc_mandal_kur(ida_in_t *in, uint8_t bitler, uint32_t cmd_id_taban);

/* Gevşeten emniyet içeriği: estop 0, iki tüpte LOADED | PLUG_IN | PIN_OUT. */
#define KC_TUP_ACIK ((uint8_t)(IDA_TUBE_LOADED | IDA_TUBE_PLUG_IN | IDA_TUBE_PIN_OUT))

/* ---- Çağrılar --------------------------------------------------------------------- */

ida_rc_t kc_cagir(kc_islev_t f, ida_in_t *in);          /* out yazılabilir */
ida_rc_t kc_cagir_out_null(kc_islev_t f, ida_in_t *in); /* (b2) */
ida_rc_t kc_step(ida_in_t *in);
ida_rc_t kc_tick(ida_in_t *in);
ida_bg_rc_t kc_bg(int64_t now);
void kc_bg_slices_elle(uint32_t deger); /* bir sonraki step/tick için bg_slices */

/* Model: son kabul edilen zaman çerçevesi (yoksa 0) ve kullanılan en büyük now_w. */
uint32_t kc_son_seq(void);
int64_t kc_son_t(void);
int64_t kc_son_now(void);
/* Bloksuz init + n olağan step (seq 1..n, t = KC_T0 + k·KC_P); her biri OK olmalı. */
void kc_hazirla(int n_step);
/* Bir olağan step daha (seq = son + 1, t = en büyük zaman + KC_P); dönüş kodu. */
ida_rc_t kc_adim(void);
/* Bir sonraki olağan adımın seq ve zamanı. */
uint32_t kc_sonraki_seq(void);
int64_t kc_sonraki_t(void);

/* Son yazılabilir çağrının (örnek 0) çıktısı ve ondan önceki yazılabilir çıktı. */
const ida_out_t *kc_out(void);
const ida_out_t *kc_onceki_out(void);
/* Bir önceki yazılabilir çıktıdan bu yana sayaç farkı. */
uint32_t kc_fark(size_t alan_konumu);
#define KC_FARK(alan) kc_fark(offsetof(ida_core_counters_t, alan))

/* ---- Çıktı yardımcıları ----------------------------------------------------------- */

size_t kc_serilestir(const ida_out_t *o, uint8_t *buf, size_t cap);
bool kc_out_esit(const ida_out_t *a, const ida_out_t *b);
void kc_not_init_beklenen(const ida_in_t *in, uint8_t src, ida_out_t *b);
uint8_t kc_sonuc_turu(uint8_t cmd_type);
bool kc_daraltici_komut(const ida_cmd_t *c);
bool kc_genisletici_komut(const ida_cmd_t *c);
int kc_kapanma_sayisi(const ida_out_t *o, uint8_t tube);
int kc_gunluk_say(const ida_out_t *o, uint16_t code);
const ida_log_t *kc_gunluk_bul(const ida_out_t *o, uint16_t code);
int kc_bit_sayisi(uint32_t x);

/* ---- Parametre şeması ve blok (A4 §3.10, A4-K80) ---------------------------------- */

typedef struct {
    uint16_t id;
    uint8_t type;
    uint8_t rsv;
    uint32_t ham; /* 32 bitlik ham değer */
} kc_pkayit_t;

#define KC_PARAM_AZAMI 997

uint16_t kc_param_sayisi(void);
const ida_param_desc_t *kc_param_desc(uint16_t i);
/* Şemanın varsayılanları, id'ye göre artan sırada. */
size_t kc_varsayilan_kayitlar(kc_pkayit_t *k, size_t cap);
/* Kayıtlardan blok; dönüş blok boyu. */
size_t kc_blok_kur(uint8_t *buf, size_t cap, const kc_pkayit_t *k, size_t n, uint64_t hash);
uint64_t kc_sema_ozeti(void);
uint32_t kc_crc32(const uint8_t *p, size_t n);
uint32_t kc_param_ham(const ida_param_value_t *v);
/* (type, def, min, max) imzasıyla şemada parametre arar; yoksa -1 (A4 §3.9 tablosu). */
int kc_param_imza(uint8_t type, uint32_t def, uint32_t min, uint32_t max);

void kc_le16(uint8_t *p, uint16_t v);
void kc_le32(uint8_t *p, uint32_t v);
void kc_le64(uint8_t *p, uint64_t v);

#endif /* KABUL_CEKIRDEK_H */
