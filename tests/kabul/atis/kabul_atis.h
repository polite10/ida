/*
 * tests/kabul/atis — atış kontrolün emniyet zincirinin bağımsız kabul testleri: düzenek.
 *
 * Kaynak (yalnız bunlar): mimari Bölüm 03 sürüm 0.3'ün "Ek: uygulama gereksinimleri (emniyet
 * zinciri)" (G-Ç01–G-Ç53) ve §7 arıza tablosu; docs/gorevler/W5-ortak.md (W5-K1…K19);
 * A4 sözleşmesi (docs/sozlesmeler/A4-port.md 1.0.1, core/include/ida/port.h); var olan
 * tests/kabul/cekirdek düzeneği. Çekirdeğin uygulama kaynağı (core/src) ve birim testleri
 * (core/test) okunmadı. Çekirdeğe yalnız A4 portunun işlevleri üzerinden erişilir (kara kutu).
 *
 * Düzenek tests/kabul/cekirdek'in ortak kitaplığının (kc_*) üstüne kurulur: her çağrı o
 * düzenekle yürütülür, böylece onun genel değişmezleri (A4-K24, K27, K28, K43, K45, K55–K58,
 * K61–K65, K90, K91, K114, K120, K121, K125) her çağrıda ayrıca denetlenir. Bu dosyanın
 * eklediği:
 *   - bir "dünya": sensör akışı (IMU, basınç, manyetometre, GNSS, DVL, sızıntı, batarya),
 *     yer istasyonu kalp atışı, sanal emniyet girdileri (inst 0) ve SIL benzeri zaman
 *     (now_w = tf.t, 5 ms periyot);
 *   - sahte bir alıcı (sim sunucusunun olay sonucu ve geri okuma davranışı): her olaya
 *     ayarlanabilir sonuç (APPLIED, ret kodları, sonuçsuz) ve gecikme; uygulanan olaylar
 *     PERMIT_OPEN ve LOADED geri okumasını değiştirir;
 *   - her çağrıda denetlenen emniyet değişmezleri (ka_degismez_*; G-Ç07, G-Ç11, G-Ç16,
 *     G-Ç21, G-Ç24, G-Ç29, G-Ç47 ve W5 değişmezleri (a)–(f));
 *   - hazırlık: oturum, araç silahlandırma, kip seçimi ve FC_ARM (ka_hazir).
 *
 * Kip seçimi: A4 §3.16 "MODE: kip atış izni kümesinde değil" der ama kümeyi vermez; W5-K15
 * kümeyi bu sürümde {GUIDED} yapar. Hazırlık SET_MODE(GUIDED)'ı kabul edilene ve MODE biti
 * kalkana kadar yineler (GUIDED'ın ön koşulu kestirim düzeyidir; W5-K12).
 */
#ifndef KABUL_ATIS_H
#define KABUL_ATIS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "kabul_cekirdek.h"

/* ---- Test tablosu ve ana işlev ------------------------------------------------------ */

typedef void (*ka_test_fn_t)(void);
typedef struct {
    const char *ad;  /* ASCII anahtar (CTest komut satırı) */
    ka_test_fn_t fn;
} ka_test_t;

/* argv[1] verilmişse yalnız o adlı testi, verilmemişse hepsini koşar. */
int ka_main(int argc, char **argv, const char *dosya, const ka_test_t *t, size_t n);

/* ---- Hata iletisi ve bağlam ----------------------------------------------------------- */

void ka_baglam(const char *bicim, ...);
/* Her hata iletisine eklenen ve test düşerse tearDown'da yazdırılan önek (ör. tohum ve adım). */
void ka_baglam_on(const char *on);
_Noreturn void ka_basarisiz(int satir, const char *bicim, ...);

#define KA_DOGRULA(kosul, ...)                     \
    if (!(kosul)) {                                \
        ka_basarisiz(__LINE__, __VA_ARGS__);       \
    } else                                         \
        ((void)0)

/* ---- Sabitler ------------------------------------------------------------------------- */

#define KA_MS KC_MS
#define KA_P KC_P

/* W5-K4: T_emn_bayat varsayılanı 250 ms (tahmin; ölçülmedi). Parametre şemada "T_emn_bayat"
   adıyla bulunursa onun değeri kullanılır (ka_t_emn_bayat). */
#define KA_T_EMN_BAYAT_VARSAYILAN_MS 250
/* A1 §3.9 varsayılanları (A4-K66; tahmin). */
#define KA_APPLY_BY_VARSAYILAN_NS UINT32_C(20000000)
#define KA_HOLD_VARSAYILAN_NS UINT32_C(200000000)

/* Engel bitleri: A4 §3.17 FC_ARM satırının bitleri (FC_TEST_ENTER aynı). */
#define KA_FCARM_ENGEL                                                                          \
    (IDA_FCI_SESSION | IDA_FCI_EVENTS_OFF | IDA_FCI_MODE | IDA_FCI_NOT_ARMED | IDA_FCI_EST |      \
     IDA_FCI_FAILSAFE | IDA_FCI_ESTOP | IDA_FCI_IMU_STALE | IDA_FCI_FAULT_LATCHED)

/* Olay türü dizini (IDA_EVT_* 1..3). */
#define KA_TUR_SAYISI 4

/* Alıcının "sonuç yok" politikası (kayıp yanıt). */
#define KA_SONUC_YOK UINT8_C(0)

/* ---- Ayarlar (testler doğrudan değiştirir) ------------------------------------------- */

typedef struct {
    uint8_t kod;      /* IDA_EVR_* ya da KA_SONUC_YOK */
    uint32_t gecikme; /* sonucun kaç çağrı sonra verileceği (>= 1) */
    bool uygula;      /* alıcı olayı uyguladı mı (APPLIED/DUPLICATE'te kendiliğinden) */
} ka_politika_t;

typedef struct {
    /* çağrı türü */
    bool tick;            /* zaman çerçeveli tick (IMU yok) */
    bool izgara;          /* ızgara tick'i (tf yok, örnek ve komut yok) */
    bool imu_bozuk;       /* step'te VALID = 0 IMU (IMU_REJECTED) */
    int64_t imu_gecikme;  /* IMU t_meas'in tf.t'den geriliği (ns; IMU_LAG için) */
    bool sensorler;       /* IMU dışı sensörler */
    uint8_t sizinti;      /* sızıntı örneğinin maskesi (0: kuru) */
    bool heartbeat;       /* her çağrıda HEARTBEAT */
    /* emniyet örneği (inst 0) */
    bool emniyet;
    int64_t emniyet_aralik;   /* > 0: örnek yalnız bu aralıkla (ns) */
    bool tmeas_dondur;        /* örneğin t_meas'i son gönderilende kalır, seq artar */
    uint8_t estop;
    uint8_t tup[IDA_PORT_N_TUBE]; /* LOADED | PLUG_IN | PIN_OUT (PERMIT_OPEN modelden) */
    bool permit_elle;             /* PERMIT_OPEN alıcı modeli yerine permit_deger'den */
    uint8_t permit_deger[IDA_PORT_N_TUBE];
    bool loaded_elle;             /* LOADED alıcı modeliyle değişmez (yalnız tup[]) */
    /* bağlantı */
    uint8_t link_state;
    /* sonuçlar */
    bool sonuclari_tut;           /* bu çağrıda olay sonucu verme */
    ka_politika_t politika[KA_TUR_SAYISI];
} ka_ayar_t;

extern ka_ayar_t ka_ayar;

/* Varsayılan ayarlar: step, sensörler ve kalp atışı açık, emniyet örneği her çağrıda gevşek
   (estop 0, iki tüp KA_TUP_ACIK), oturum RUNNING, her olaya bir çağrı sonra APPLIED. */
void ka_ayar_varsayilan(void);

/* ---- Kurulum ---------------------------------------------------------------------------- */

/* n örnekli ortam, şema varsayılanlarından geçerli blokla init (param_ok = 1), model
   sıfırlanır, ayarlar varsayılana döner. */
void ka_kur(int n_ornek);
/* Özel init girdisiyle (ör. boot_count_ok = 0); dönüş init'in dönüşü. */
ida_rc_t ka_kur_init(int n_ornek, const ida_init_in_t *ii);
/* Aynı örnek belleğinde yeniden init (A4-K77; G-Ç01). */
ida_rc_t ka_yeniden_init(void);

/* ---- Çağrı ------------------------------------------------------------------------------ */

/* Bir sonraki çağrının girdisini ayarlardan kurar; test komut, mandal ya da örnek ekleyebilir. */
ida_in_t *ka_girdi(void);
/* ka_girdi ile kurulan girdiyi çalıştırır (step ya da tick), çıktıyı işler, değişmezleri
   denetler. */
ida_rc_t ka_cagir(void);
/* Girdiyi kurmadan out NULL ile çağırır ((b2); 3.11). */
ida_rc_t ka_cagir_out_null(void);
/* ka_girdi + ka_cagir. */
ida_rc_t ka_adim(void);
void ka_adimlar(int n);
/* Belirli bir zaman çerçevesi sıra numarasıyla (DUP_SEQ / OLD_SEQ üretmek için) girdi. */
ida_in_t *ka_girdi_seq(uint32_t seq);
/* A4-K122 oturum sonu çağrısı: tick, tf.present = 0, link_event = SESSION_END, link_state NONE,
   örnek ve komut yok (olay sonuçları ve mandal bitleri bulunabilir). */
ida_in_t *ka_girdi_oturum_sonu(void);

const ida_out_t *ka_out(void);
const ida_out_t *ka_onceki(void);
uint64_t ka_cagri_no(void);
int64_t ka_simdi(void);          /* son çağrının now_w'si */

/* ---- Komut ekleme --------------------------------------------------------------------- */

uint32_t ka_yeni_id(void);
uint32_t ka_komut(ida_in_t *in, uint8_t type);
uint32_t ka_fc(ida_in_t *in, uint8_t op, uint8_t tube);
uint32_t ka_mandal(ida_in_t *in, uint8_t bit);
/* Girdideki son komut (sınır denetimli). */
ida_cmd_t *ka_son_komut(ida_in_t *in); /* tek mandal biti; NARROW_MODE'da mode = STATION_KEEP */
ida_sample_t *ka_emniyet_ekle(ida_in_t *in, uint8_t inst, uint8_t estop, uint8_t t0, uint8_t t1);
/* Son çıktıda cmd_id'nin sonucu; yoksa NULL. */
const ida_cmd_result_t *ka_sonuc(uint32_t cmd_id);
/* Beklenen ack/reason denetimi. */
void ka_sonuc_bekle(int satir, uint32_t cmd_id, uint8_t ack, uint16_t reason, const char *ne);
#define KA_SONUC(id, ack, reason, ne) ka_sonuc_bekle(__LINE__, (id), (uint8_t)(ack), (uint16_t)(reason), (ne))

/* ---- Çıktı yardımcıları --------------------------------------------------------------- */

/* Çıktıdaki kind türünde olay sayısı; tube < 0 ise her tüp. */
int ka_olay_say(const ida_out_t *o, uint8_t kind, int tube);
const ida_event_t *ka_olay_bul(const ida_out_t *o, uint8_t kind, int tube);
/* Çıktıda tüpü (ya da ALL'u) kapsayan SAFETY_CLOSE var mı. */
bool ka_kapanma_var(const ida_out_t *o, uint8_t tube);
bool ka_genisletici_olay_var(const ida_out_t *o);
const char *ka_durum_adi(uint8_t s);

/* ---- Olay geçmişi ve alıcı ------------------------------------------------------------ */

typedef struct {
    ida_event_t e;
    int64_t now_w;        /* olayı çıkaran çağrının now_w'si */
    uint64_t cagri;       /* olayı çıkaran çağrı */
    uint8_t sonuc;        /* planlanan sonuç (KA_SONUC_YOK: yok) */
    int64_t t_apply;
    uint64_t teslim_cagri;/* sonucun verileceği çağrı */
    bool teslim_edildi;   /* sonuç kabul edilen bir çağrının evres[]'inde verildi */
    uint64_t teslim_edilen_cagri;
    int64_t teslim_now;
    bool kapandi;         /* sonradan çıkan bir SAFETY_CLOSE bu tüpü kapattı (yalnız OPEN) */
    bool bilinmeyen;      /* sonucu apply_by + 100 ms içinde kabul edilen bir çağrıda verilmedi */
} ka_olay_t;

/* En son çıkan, verilen türde ve tüpteki olay (tube < 0: her tüp); yoksa NULL. */
const ka_olay_t *ka_son_olay(uint8_t kind, int tube);
const ka_olay_t *ka_olay_kimlikle(uint64_t event_id);
size_t ka_olay_sayisi(void);
const ka_olay_t *ka_olay_no(size_t i);
/* Bir sonraki çağrıya elle sonuç ekle (yabancı kimlik, tanımsız kod, yinelenen sonuç). */
void ka_sonuc_ekle(ida_in_t *in, uint64_t event_id, uint8_t kod, int64_t t_apply);
/* Bir olayın planlanan sonucunu değiştir (henüz verilmediyse). */
void ka_sonuc_planla(uint64_t event_id, uint8_t kod, uint32_t gecikme);
/* Alıcının izin modeli: tüpün PERMIT_OPEN geri okuması (sim zamanı t'de). */
bool ka_alici_izin(uint8_t tube, int64_t t);

/* ---- Parametreler --------------------------------------------------------------------- */

/* Adla parametre kimliği; yoksa -1. */
int ka_param_id(const char *ad);
/* Örnek 0'daki yürürlükteki değer (U32). */
uint32_t ka_param_u32(const char *ad, uint32_t yoksa);
/* PARAM_SET + PARAM_COMMIT (silahsızken); ikisinin ACCEPTED olması beklenir. */
void ka_param_yaz(const char *ad, uint32_t deger);
/* T_emn_bayat (ns): şemada adıyla varsa onun değeri (ms), yoksa W5-K4 varsayılanı. */
int64_t ka_t_emn_bayat(void);

/* ---- Hazırlık ------------------------------------------------------------------------- */

/* SESSION_START, sensör akışı; kip INIT'ten ve kestirim INIT'ten çıkana kadar (en çok 10 s). */
void ka_oturum_ac(void);
/* Yalnız SESSION_START taşıyan bir çağrı ve bir gevşek adım (bekleme yok). */
void ka_oturum_baslat(void);
/* FC'yi silahlandırmadan FAULT_LATCHED'e alır (G-Ç53'ün birinci durumu): tüp 0'ın pimi takılır,
   SAFETY_CLOSE APPLIED olur, sonraki örnekte PERMIT_OPEN = 1 (pim takılıyken izin açık görünür).
   Çıkışta ka_ayar.permit_elle açık kalır (neden sürer); gevşetmek için ka_kilit_nedeni_kaldir. */
void ka_kilitle(void);
void ka_kilit_nedeni_kaldir(void);
/* Araç ARM (A4 §3.17 ARM satırı); armed = 1 olana kadar. */
void ka_arac_silahla(void);
/* GUIDED'a geçer ve MODE bitinin kalkmasını bekler (W5-K15; yukarıdaki açıklama). */
void ka_kip_sec(void);
/* FC_ARM satırının engel bitleri kalkana kadar bekler (en çok 10 s); kalkmazsa ayrıntılı
   iletiyle düşer. */
void ka_fc_hazir_bekle(void);
/* FC_ARM(tube): ACCEPTED ve fc.state = ARMED beklenir. */
void ka_fc_silahla(uint8_t tube);
/* ka_kur(n) + oturum + araç + kip + bekleme (FC_ARM öncesi). */
void ka_hazir_oncesi(int n_ornek);
/* ka_hazir_oncesi + ka_fc_silahla(tube). */
void ka_hazir(int n_ornek, uint8_t tube);
/* ARMED'de FC_FIRE(tube): ACCEPTED beklenir; SAFETY_OPEN(tube) çıkana kadar adım atar (en çok
   max_adim) ve olayı döndürür. */
const ka_olay_t *ka_izin_iste(uint8_t tube, int max_adim);
/* Verilen türde ve tüpte yeni bir olay çıkana kadar adım atar; çıkmazsa NULL. */
const ka_olay_t *ka_olay_bekle(uint8_t kind, int tube, int max_adim);

/* ---- Model (değişmezler için) ---------------------------------------------------------- */

/* Etkin emniyet değerlerinin modeli (A4-K116): kısıtlayan mı. */
bool ka_model_estop(void);
bool ka_model_fis_cikik(uint8_t tube);
bool ka_model_pim_takili(uint8_t tube);
/* Model: acil durdurma bu çağrıda kesinlikle bayat (G-Ç29'un iki ölçütünden biri aşıldı). */
bool ka_model_bayat(void);
/* Model (G-Ç11): tüpün izni duvar saati now'da çekirdekçe açık sayılabilir mi (APPLIED alınmış,
   olayı çıkaran çağrının now_w'sinden hold_ns dolmamış, sonradan kapatılmamış SAFETY_OPEN). */
bool ka_model_izin_acik(uint8_t tube, int64_t now);
/* Model (A4 §3.16 FIRE_PENDING): son çağrıdan ÖNCEKİ bir çağrıda çıkmış ve sonucu henüz kabul
   edilen bir çağrıda verilmemiş FIRE var mı. FC_ABORT çıkmış bir FIRE'ı geri alamaz (W5-K23
   bekleyen FIRE isteklerini iptal eder); böyle bir FIRE'ın sonucu beklenirken dış durum §3.16'nın
   tanımıyla FIRE_PENDING'dir. */
bool ka_onceki_fire_sonucu_bekleniyor(void);

/* Değişmez denetimlerini kapat/aç (yalnız bilerek tutarsız alıcı davranışı kuran testler). */
void ka_degismez_ac(bool acik);
/* Modelden çıktıya engel biti denetimi (özellik testleri): kabul edilen her çağrının sonunda,
   girdiden bağımsız hesaplanan modelde kısıtlayan (ya da kesin bayat) olan her emniyet alanının
   IDA_FCI_* biti kurulu olmalıdır (A4-K116, A4 §3.16, G-Ç29). Varsayılan kapalı. */
void ka_model_bit_denetimi(bool acik);

/* Kapsam sayaçları (özellik testleri). */
typedef struct {
    uint64_t durum[6];
    uint64_t open;
    uint64_t fire;
    uint64_t close;
    uint64_t fc_fire_kabul;
    uint64_t fc_arm_kabul;
} ka_kapsam_t;
const ka_kapsam_t *ka_kapsam(void);

#endif /* KABUL_ATIS_H */
