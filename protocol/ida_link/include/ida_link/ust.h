/*
 * ida_link/ust.h — A1 <-> A4 dönüştürücüsü (ida_link üst katmanı): oturum, parti, komut
 * sırası, olay tablosu ve çıktı çerçeveleri.
 *
 * Görev: docs/gorevler/W3-oturum.md. Sözleşmeler: docs/sozlesmeler/A1-kart-baglantisi.md
 * (1.0; §3, §4, §5, Ek A), docs/sozlesmeler/A4-port.md (platform kuralları),
 * core/include/ida/port.h. Alt katman: ida_link/tel.h (çerçeveleme, kodlama); bu katman
 * onu yalnız dışa açık arayüzünden kullanır.
 *
 * Kart yazılımı ve kartsız (SIL) koşucu bu modülü aynı kaynaktan bağlar. Dil ISO C17
 * (uzantısız); C++20'den de içerilebilir. Bellek ayırmaz; saat, kesme ve seri port bilmez:
 * zaman, gönderim kuyruğu, rastgele sayı, yeniden başlatma ve kalıcı blok bilgisi platformun
 * verdiği işlev tablosundan (ida_link_platform_t) gelir. Kitaplıkta değişebilir genel durum
 * yoktur; bütün durum çağıranın ida_link_ust_t örneğindedir (A1-K101). Tek örnek aynı anda
 * iki iş parçacığından kullanılmaz (kilit yoktur); işlevler platform işlevlerini yalnız
 * çağrıldıkları iş parçacığından çağırır.
 *
 * Akış (bir platform döngüsü):
 *   1. Alınan baytlar alım damgasıyla:           ida_link_ust_rx(l, b, n, t_rx)
 *   2. Zamana bağlı işler (duyuru, canlılık,
 *      olay yinelemesi, T_bağ):                    ida_link_ust_poll_tx(l)
 *   3. Bir çekirdek çağrısı gerekiyor mu:         tur = ida_link_ust_take(l, &in)
 *      tur != IDA_LINK_CAGRI_YOK ise platform in->plat'ın kendi alanlarını (param_store,
 *      param_fail, param_gen, prev_overrun, tx_stalled, prev_call_ns) ve in->bg_slices'ı
 *      yazar; tur STEP ise ida_core_step, değilse ida_core_tick çağırır;
 *   4. Çağrı dönünce (her dönüş kodunda):         ida_link_ust_put_out(l, &in, rc, &out)
 * ÇAĞRI PENCERESİ (W3-K10; platformun yükümlülüğü): take'in YOK dışında bir değer döndürdüğü
 * an girdinin çekirdek çağrısına girdiği andır (D-17'nin "çağrıya girmemiş" ölçütü buna göre
 * okunur). Bu andan put_out'a kadar (ida_link_ust_cagri_acik true) ida_link_ust_rx
 * ÇAĞRILMAZ; alınan baytlar platformun alım tamponunda bekler ve put_out'tan sonra verilir.
 * Aksi halde bir daraltıcı, take'in girdiye koyduğu bir genişleticiden sonra alınıp ondan
 * önce uygulanmış sayılmazdı. Modül ihlali saptar: pencerede rx çağrılırsa baytlar
 * kaybedilmeden işlenir (daraltıcı mandala girer) ve oturum IDA_SESSION_CLOSE(PROTOCOL) ile
 * biter; sonraki çağrı oturum sonu çağrısıdır (silahsızlandırır). poll_tx pencerede
 * çağrılabilir. take, put_out gelmeden yeniden çağrılırsa IDA_LINK_CAGRI_YOK döner.
 * in->ping360 bu örneğin belleğini gösterir ve put_out'a kadar geçerlidir (A4-K99).
 *
 * Zaman: platform saati ve alım damgaları A4-K08 aralığına (0 … 2^62 ns) sınırlanır.
 */
#ifndef IDA_LINK_UST_H
#define IDA_LINK_UST_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "ida/port.h"
#include "ida_link/tel.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------------ */
/* Sabitler (A1 §3.1, §3.9; değerler sözleşmeden, "seçim" yazanlar bu  */
/* uygulamanın seçimidir)                                              */
/* ------------------------------------------------------------------ */

#define IDA_LINK_UST_P_NS             INT64_C(5000000)    /* P (A1 §3.9)                 */
#define IDA_LINK_UST_IZGARA_NS        INT64_C(10000000)   /* A4-K18 ızgarası             */
#define IDA_LINK_UST_T_TF_GEC_NS      INT64_C(1000000)    /* A1-K117                     */
#define IDA_LINK_UST_T_CANLI_NS       INT64_C(100000000)  /* A1-K20                      */
#define IDA_LINK_UST_T_DUYURU_NS      INT64_C(200000000)  /* A1-K11                      */
#define IDA_LINK_UST_T_OLAY_YIN_NS    INT64_C(15000000)   /* A1-K80                      */
#define IDA_LINK_UST_T_SONUC_PAY_NS   INT64_C(95000000)   /* A1-K80, E8                  */
#define IDA_LINK_UST_T_DURUM_NS       INT64_C(100000000)  /* A1-K91: STATUS, FC_STATUS   */
#define IDA_LINK_UST_T_SAYAC_NS       INT64_C(1000000000) /* A1-K91: sayaç çerçeveleri   */
#define IDA_LINK_UST_T_BAG_MIN_NS     UINT32_C(500000000) /* A1 §3.9 T_bağ aralığı       */
#define IDA_LINK_UST_T_BAG_MAX_NS     UINT32_C(2000000000)
#define IDA_LINK_UST_T_YETIM          8u   /* A1-K51: parti kapanışı                     */
#define IDA_LINK_UST_N_OLAY           8u   /* A1-K80: olay tablosu (7 izin açan + 1)     */
#define IDA_LINK_UST_N_OLAY_ACAN      7u
#define IDA_LINK_UST_NIHAI_N          32u  /* A1-K63: nihai NARROWED kimlikleri          */
#define IDA_LINK_UST_KOMUT_KUYRUGU    24u  /* seçim: 8 çağrı + 16 erteleme (A4-K105)     */
#define IDA_LINK_UST_KAPI_SONUC_N     8u   /* seçim: ilk kapıda belirlenen sonuçlar      */
#define IDA_LINK_UST_PING_YUVA        2u   /* A1-K51: en az iki yeniden birleştirme yuvası */
#define IDA_LINK_UST_PING_PARCA       240u /* A1 §3.4: PING360_DATA parça boyu           */
#define IDA_LINK_UST_PING_PARCA_AZAMI 5u   /* ⌈1200 / 240⌉                               */

/* IDA_LINK_STATS.cnt[] dizinleri (A1 Tablo 3.11b). */
enum {
    IDA_LINK_SAY_RX_BYTES = 0,
    IDA_LINK_SAY_RX_FRAMES = 1,
    IDA_LINK_SAY_RX_CRC_ERR = 2,
    IDA_LINK_SAY_RX_UNKNOWN_MSGID = 3,
    IDA_LINK_SAY_RX_IFLAG = 4,
    IDA_LINK_SAY_RX_BAD_LEN = 5,
    IDA_LINK_SAY_RX_WRONG_SYS = 6,
    IDA_LINK_SAY_RX_MAV1 = 7,
    IDA_LINK_SAY_RX_OVERFLOW = 8,
    IDA_LINK_SAY_RX_RESYNC_BYTES = 9,
    IDA_LINK_SAY_RX_SEQ_GAP = 10,
    IDA_LINK_SAY_RX_STALE_SESSION = 11,
    IDA_LINK_SAY_RX_OUTSIDE_SESSION = 12,
    IDA_LINK_SAY_STAMP_COARSE = 13,      /* platformun (alım damgası, A1-K48)        */
    IDA_LINK_SAY_BATCH_CLOSED = 14,
    IDA_LINK_SAY_BATCH_INCOMPLETE = 15,
    IDA_LINK_SAY_BATCH_SKIPPED = 16,
    IDA_LINK_SAY_BATCH_OLD = 17,
    IDA_LINK_SAY_BATCH_DUP = 18,
    IDA_LINK_SAY_BATCH_MERGED = 19,
    IDA_LINK_SAY_MEMBER_OVERFLOW = 20,
    IDA_LINK_SAY_PING360_INCOMPLETE = 21,
    IDA_LINK_SAY_PING360_BAD = 22,
    IDA_LINK_SAY_PING360_ORPHAN = 23,
    IDA_LINK_SAY_CMD_DEFERRED = 24,
    IDA_LINK_SAY_CMD_DROPPED = 25,
    IDA_LINK_SAY_NARROW_COALESCED = 26,
    IDA_LINK_SAY_CMD_BAD_FIELD = 27,
    IDA_LINK_SAY_GRID_MISSED = 28,
    IDA_LINK_SAY_OUT_FIXED = 29,
    IDA_LINK_SAY_TX_FRAMES = 30,         /* platformun (gönderim kuyruğu)            */
    IDA_LINK_SAY_TX_DROPPED = 31,        /* platformun                               */
    IDA_LINK_SAY_TX_CRIT_STALL = 32,     /* platformun                               */
    IDA_LINK_SAY_EVENTS_SENT = 33,
    IDA_LINK_SAY_EVENTS_RESENT = 34,
    IDA_LINK_SAY_SESSIONS = 35,
    IDA_LINK_SAY_N = 36
};

/* ------------------------------------------------------------------ */
/* Tipler                                                             */
/* ------------------------------------------------------------------ */

/* Platformun türü (A1 §1.1). Tel katmanında ikisi de IDA_LINK_ROL_KART'tır (A1-K98). */
typedef enum {
    IDA_LINK_UST_ROL_KART = 1,   /* STM32 kartı: ızgara tick'i, T_bağ, duyuru          */
    IDA_LINK_UST_ROL_SIL  = 2    /* SIL koşucusu: A1-K100, K121 (ızgara ve T_bağ yok)    */
} ida_link_ust_rol_t;

/* Gönderim sınıfı (A1-K110). Bu modül her çerçeveyi etiketler; kuyruğu platform tutar ve
   her çerçeve sınırında KRITIK > ZORUNLU > KAYBEDILEBILIR önceliğiyle seçer. */
typedef enum {
    IDA_LINK_SINIF_KRITIK         = 1, /* (i) IDA_ACTUATOR ve çağrının yeni olayları   */
    IDA_LINK_SINIF_ZORUNLU        = 2, /* (ii) atılmaz; taşma oturum sonudur           */
    IDA_LINK_SINIF_KAYBEDILEBILIR = 3  /* (iii) R_iii bütçeli; dolunca en eskisi atılır */
} ida_link_sinif_t;

/* take'in bildirdiği çağrı türü; değerler IDAW_CALL_KIND ile aynıdır (YOK hariç). */
typedef enum {
    IDA_LINK_CAGRI_YOK         = 0,  /* çağrı gerekmiyor                               */
    IDA_LINK_CAGRI_STEP        = 1,  /* ida_core_step (IMU'lu parti)                    */
    IDA_LINK_CAGRI_TICK_TF     = 2,  /* ida_core_tick, tf.present = 1                   */
    IDA_LINK_CAGRI_IZGARA      = 3,  /* ida_core_tick, ızgara (yalnız kart; A1-K117)    */
    IDA_LINK_CAGRI_OTURUM_SONU = 4   /* ida_core_tick, link_event = SESSION_END (A1-K107) */
} ida_link_cagri_t;

/* Kartın A1 durumu (A1 §4.2); SONLANIYOR: oturum sonu saptandı, oturum sonu çağrısı
   henüz dönmedi (A1-K107). Telde SONLANIYOR yoktur. */
typedef enum {
    IDA_LINK_UST_IDLE       = 0,
    IDA_LINK_UST_HANDSHAKE  = 1,
    IDA_LINK_UST_RUNNING    = 2,
    IDA_LINK_UST_SONLANIYOR = 3
} ida_link_ust_durum_t;

/* Platform işlevleri. Hepsi zorunludur (NULL olamaz); ctx her çağrıya aynen verilir. */
typedef struct {
    void *ctx;
    /* Duvar saati, ns; örnek ömrü boyunca azalmaz (A4-K09). Kartta açılıştan beri geçen
       süre; SIL'de A4-K11 ve A1-K121'e göre zaman çerçevesinden türetilmiş sim zamanı. */
    ida_wtime_t (*zaman)(void *ctx);
    /* Tam bir çerçeveyi sınıfıyla gönderim kuyruğuna verir (kopyalar). true: kabul edildi;
       false: kuyrukta yer yok. ZORUNLU sınıfta false oturumu TX_OVERFLOW ile bitirir
       (A1-K110); öteki sınıflarda bu modül yalnız sonucu bilir (tx_* sayaçları platformun). */
    bool (*gonder)(void *ctx, ida_link_sinif_t sinif, const uint8_t *cerceve, size_t n);
    /* Gönderim kuyruğunda yazımı başlamamış bütün çerçeveleri siler (A1-K22 (1)); yazımı
       başlamış çerçeve tamamlanır (A1-K10). */
    void (*kuyruk_temizle)(void *ctx);
    /* Sıfırdan farklı yeni el sıkışma nonce'u (A1-K13: kartta gerçek rastgelelik; SIL'de
       A1-K114). 0 dönerse modül en çok dört kez yeniden ister, sonra 1 kullanır. */
    uint32_t (*nonce)(void *ctx);
    /* Planlı yeniden başlatma (A1-K17, SESSION_ACCEPT sonucu REBOOT). Kartta dönmez
       (reset_cause = SOFTWARE). SIL'de koşucu çekirdeği yeni boot_count ve SIL_REBOOT ile
       yeniden kurar ve bu örneği yeniden kurar (ida_link_ust_kur); dönerse modül IDLE'da kalır. */
    void (*yeniden_baslat)(void *ctx);
    /* Kalıcı parametre bloğunun bu anki CRC-32/ISO-HDLC değeri, boyu ve kuşağı; blok yoksa
       0, 0, 0 (A1-K68; IDA_BOARD_HELLO ve IDA_STATUS.param_blob_crc). */
    void (*kalici_blok)(void *ctx, uint32_t *crc, uint32_t *len, uint32_t *gen);
} ida_link_platform_t;

/* Önceki açılışın hata kaydı (A1-K96; 01 §2.8). */
typedef struct {
    uint32_t cfsr, hfsr, mmfar, bfar, pc, lr, xpsr, step_counter;
    uint16_t fault_seq;
    uint8_t  stacking_valid;
} ida_link_hata_kaydi_t;

/* Kurulum ayarları. ida_link_ust_kur kopyalar. */
typedef struct {
    ida_link_ust_rol_t rol;
    /* Açılış kaydı: ida_init_in_t'nin aynı adlı alanları (IDA_BOARD_HELLO; A1 §3.8). */
    uint32_t boot_count;          /* olay kimliğinin üst 32 biti de budur (A4-K61, K67) */
    bool     boot_count_ok;
    uint8_t  reset_cause;
    uint32_t reset_flags;
    uint8_t  consecutive_faults;
    uint8_t  param_src;
    uint8_t  init_rc;             /* ida_core_init dönüşü: IDA_RC_OK ya da PARAM_DEFAULTS */
    ida_core_info_t cekirdek;     /* ida_core_info'nun çıktısı                          */
    uint32_t fw_version;
    uint32_t fw_git;              /* commit özetinin ilk 32 biti                        */
    /* Bu açılışın kalıcı başlangıç durumu: kartta 0; SIL'de sınama girdisi (A1-K114). */
    uint16_t prev_sess;
    uint16_t sessions_since_boot;
    uint32_t last_cmd_id;
    /* Zamanlama ve ESC (A5 değerleri). */
    uint32_t t_bag_ns;            /* T_bağ, IDA_LINK_UST_T_BAG_MIN_NS … MAX_NS (A1 §3.9) */
    uint16_t esc_alt_us;          /* ESC alt sınırı; alt < nötr < üst                     */
    uint16_t esc_notr_us;
    uint16_t esc_ust_us;
    /* Önceki açılışın hata kaydı (A1-K96); yoksa hata_kaydi_var = false. */
    bool     hata_kaydi_var;
    ida_link_hata_kaydi_t hata_kaydi;
} ida_link_ust_ayar_t;

/* ---- Örneğin iç tipleri: alanlar ozel_ öneklidir, çağıran yazmaz ve okumaz. ---- */

typedef struct {
    uint8_t          n_emn;                                /* IDA_SAFETY örnekleri       */
    ida_sample_t     emn[IDA_PORT_MAX_SAMPLES];
    uint32_t         emn_rev[IDA_PORT_MAX_SAMPLES];        /* örneğin safety_rev'i       */
    uint8_t          n_diger;                              /* öteki sensör örnekleri     */
    ida_sample_t     diger[IDA_PORT_MAX_SAMPLES];
    bool             imu_var;
    ida_imu_sample_t imu;
    uint8_t          n_det_batch;
    ida_det_batch_t  det_batch[IDA_PORT_MAX_DET_BATCH];
    ida_wtime_t      det_batch_t_rx[IDA_PORT_MAX_DET_BATCH];
    uint8_t          n_det;
    ida_det_record_t det[IDA_PORT_MAX_DET_IN];
    ida_wtime_t      det_t_rx[IDA_PORT_MAX_DET_IN];
    uint16_t         n_uye;      /* açık pencerede alınan geçerli üye (A1-K30)          */
    bool             tasma;      /* pencerede alım taşması (A1-K09)                    */
    bool             dustu;      /* kapasite yüzünden üye düştü (plat.input_dropped)   */
} ida_link_ozel_parti_t;

typedef struct {
    ida_cmd_t cmd;
    uint8_t   genisletici;       /* A1 Tablo 3.6 "genişletir"                           */
    uint8_t   kapali;            /* partisi kapandı (bekleyen ya da ertelenmiş)          */
    uint8_t   ertelendi;         /* cnt_cmd_deferred'e sayıldı                          */
} ida_link_ozel_komut_t;

typedef struct {
    ida_event_t ev;
    uint8_t     durum;           /* 0 boş, 1 kova bekliyor, 2 gönderildi, 3 sonuç hazır */
    uint8_t     call_kind;
    uint8_t     sonuc;           /* IDA_EVR_*                                           */
    uint8_t     bek_sonuc;       /* açık penceredeki sim sonucu (parti üyesi); 0: yok   */
    uint16_t    sess;
    int64_t     bek_t_apply_ns;
    uint32_t    safety_rev;
    int64_t     t_apply_ns;
    ida_wtime_t cikis_w;         /* olayı çıkaran çağrının now_w'si                     */
    ida_wtime_t son_gonderim;
} ida_link_ozel_olay_t;

typedef struct {
    uint8_t     dolu;
    uint8_t     inst;
    uint8_t     parca;           /* alınan parçaların bit maskesi                       */
    uint8_t     yas;             /* HEAD'siz geçen parti kapanışı (A1-K51)               */
    uint8_t     bu_pencerede;    /* açık pencerede parça ya da HEAD alındı               */
    uint8_t     bas_var;         /* HEAD açık pencerede alındı, teslim kapanışta         */
    uint32_t    seq;
    uint32_t    sira;            /* yuvanın açılış sırası (en eskiyi bulmak için)        */
    ida_ping360_t satir;         /* HEAD alanları + birleştirilen veri                   */
} ida_link_ozel_ping_t;

/* Modül örneği. Boyu derleyiciye göre değişebilir; platform statik olarak ayırır. */
typedef struct {
    /* Sayaçlar (A1 Tablo 3.11b). 0–10 tel katmanından okunur (ida_link_ust_sayaclar);
       13, 30, 31, 32 platformundur ve platform bu diziye doğrudan ekler; ötekileri modül
       artırır. Hepsi 32 bittir ve sarar. */
    uint32_t say[IDA_LINK_SAY_N];

    ida_link_ust_ayar_t  ozel_ayar;
    ida_link_platform_t  ozel_plat;
    ida_link_alici_t     ozel_alici;
    ida_link_gonderici_t ozel_gonderici;
    uint8_t  ozel_kurulu;

    /* Oturum */
    uint8_t  ozel_durum;               /* ida_link_ust_durum_t                         */
    uint8_t  ozel_guvenli;             /* platform güvenli beklemede (A1-K13, H33)      */
    uint8_t  ozel_baslat_olayi;        /* SESSION_START sonraki çağrıda (A1-K16)        */
    uint8_t  ozel_sonu_bekliyor;       /* oturum sonu çağrısı yapılmadı (A1-K107)       */
    uint8_t  ozel_hello_bekliyor;      /* oturum sonu dönünce yanıtlanacak SIM_HELLO    */
    uint8_t  ozel_nonce_var;
    uint8_t  ozel_duyuru_var;
    uint8_t  ozel_tx_var;
    uint8_t  ozel_run_mode;
    uint16_t ozel_sess;
    uint16_t ozel_prev_sess;
    uint16_t ozel_oturum_sayisi;       /* sessions_since_boot                          */
    uint32_t ozel_kusak;               /* her oturum sonunda artar                     */
    uint32_t ozel_yanit_nonce;         /* yanıtlanan son SIM_HELLO.nonce               */
    uint32_t ozel_hello_nonce;         /* bekleyen SIM_HELLO'nun nonce'u               */
    uint32_t ozel_kendi_nonce;
    uint32_t ozel_run_id;
    ida_wtime_t ozel_son_rx;           /* sim'den son geçerli çerçeve (T_bağ)          */
    ida_wtime_t ozel_son_tx;           /* son gönderim (T_canlı)                       */
    ida_wtime_t ozel_son_duyuru;
    uint32_t ozel_emn_rev;             /* bu oturumda çekirdeğe verilen son safety_rev  */

    /* Parti ve zaman çerçevesi */
    uint8_t  ozel_tf_alindi;           /* bu oturumda TF kapandı                        */
    uint8_t  ozel_tf_hic;              /* açılıştan beri TF kabul edildi (SIL now_w)    */
    uint32_t ozel_son_kapanan;         /* son kapanan parti                            */
    int64_t  ozel_son_tf_t;            /* son kapanan partinin t_sim_ns'i               */
    ida_wtime_t ozel_son_tf_rx;
    ida_wtime_t ozel_izgara_sonraki;
    uint8_t  ozel_izgara_kurulu;
    ida_link_ozel_parti_t ozel_acik;   /* açık pencere                                 */
    ida_link_ozel_parti_t ozel_bek;    /* kapanmış, çağrılmamış girdi (A4-K102)         */
    uint8_t  ozel_bek_tf_var;
    uint8_t  ozel_bek_birlesen;        /* bekleyende birleşen eski parti sayısı         */
    uint8_t  ozel_bek_eksik;
    uint8_t  ozel_bek_cakisma;
    uint8_t  ozel_bek_ping_var;
    ida_time_frame_t ozel_bek_tf;
    ida_ping360_t    ozel_bek_ping;
    ida_link_ozel_ping_t ozel_ping[IDA_LINK_UST_PING_YUVA];
    uint32_t ozel_ping_sira;

    /* Süren çağrı (take ile put_out arası) ve son çağrı */
    uint8_t  ozel_cagri_suruyor;
    uint8_t  ozel_cagri_tur;
    uint8_t  ozel_cagri_birlesen;
    uint8_t  ozel_cagri_eksik;
    uint8_t  ozel_cagri_cakisma;
    uint8_t  ozel_son_cagri_var;
    uint16_t ozel_cagri_sess;
    uint32_t ozel_cagri_kusak;
    ida_wtime_t ozel_cagri_bas;
    ida_wtime_t ozel_son_cagri_bas;
    ida_wtime_t ozel_son_cagri_bitis;
    ida_ping360_t ozel_cagri_ping;

    /* Komutlar */
    ida_narrow_t ozel_mandal;
    uint8_t  ozel_n_kom;
    ida_link_ozel_komut_t ozel_kom[IDA_LINK_UST_KOMUT_KUYRUGU];
    uint8_t  ozel_n_nihai;
    uint8_t  ozel_nihai_bas;
    uint32_t ozel_nihai[IDA_LINK_UST_NIHAI_N];
    uint32_t ozel_last_cmd_id;

    /* Olaylar */
    ida_link_ozel_olay_t ozel_olay[IDA_LINK_UST_N_OLAY];
    uint8_t  ozel_n_kapi;
    ida_event_result_t ozel_kapi[IDA_LINK_UST_KAPI_SONUC_N];
    uint8_t  ozel_olay_ctr_var;
    uint32_t ozel_olay_ctr;
    int64_t  ozel_kova_acan;
    int64_t  ozel_kova_kapan;
    ida_wtime_t ozel_kova_zaman;

    /* Çıktı */
    uint32_t ozel_out_seq;
    uint8_t  ozel_durum_var;
    uint8_t  ozel_sayac_var;
    ida_wtime_t ozel_son_durum;
    ida_wtime_t ozel_son_sayac;
} ida_link_ust_t;

/* ------------------------------------------------------------------ */
/* İşlevler                                                           */
/* ------------------------------------------------------------------ */

/*
 * ida_link_ust_kur — örneği kurar (açılışta bir kez; SIL'de REBOOT'tan sonra yeniden).
 * Önkoşul: platform ida_core_init'i yapmıştır (ayar->init_rc onun dönüşüdür).
 * Denetim: l, ayar, plat NULL değil; plat'ın bütün işlevleri NULL değil; rol geçerli;
 * t_bag_ns 3.9 aralığında; esc_alt_us < esc_notr_us < esc_ust_us; init_rc OK ya da
 * PARAM_DEFAULTS. Dönüş: IDA_LINK_TAMAM; aksi halde IDA_LINK_HATA_ARGUMAN ve örnek kurulmamış
 * kalır (öteki işlevler hiçbir şey yapmaz, take YOK döner).
 * Kurulumda durum IDLE'dır; ızgara takvimi platformun saatinden başlar.
 */
ida_link_rc_t ida_link_ust_kur(ida_link_ust_t *l, const ida_link_ust_ayar_t *ayar,
                               const ida_link_platform_t *plat);

/*
 * ida_link_ust_rx — taşımadan alınan baytları işler. t_rx bu bloktaki baytların alım
 * damgasıdır (A1-K48; blok tek damga taşır); bloğun içinde biten her çerçeve bu damgayı
 * alır. Çerçeveleri tel katmanıyla çözer, oturuma göre işler (A1 §4.2–§4.7), partiye bağlar,
 * daraltıcıyı mandala alır ve platformun komut sonuçlarını hemen gönderir. n = 0 geçerlidir.
 * Çağrı penceresi açıkken (take ile put_out arası) çağrılmaz; çağrılırsa (n > 0) baytlar
 * işlenir ve oturum PROTOCOL ile biter (W3-K10; dosya başı).
 */
void ida_link_ust_rx(ida_link_ust_t *l, const uint8_t *veri, size_t n, ida_wtime_t t_rx);

/*
 * ida_link_ust_rx_tasma — alım tamponu taştı (A1-K09): tel katmanında rx_overflow artar,
 * yarım aday atılır ve açık parti eksik sayılır.
 */
void ida_link_ust_rx_tasma(ida_link_ust_t *l);

/*
 * ida_link_ust_poll_tx — zamana bağlı işler (her döngüde çağrılır): IDLE'da duyuru (yalnız
 * kart rolünde; A1-K11), canlılık (A1-K20), T_bağ (yalnız kart rolünde; A1-K22), olay
 * yinelemesi ve sonuç payı (A1-K80), kova bekleyen SAFETY_CLOSE'un gönderimi (A1-K120).
 */
void ida_link_ust_poll_tx(ida_link_ust_t *l);

/*
 * ida_link_ust_take — sıradaki çekirdek çağrısının girdisini kurar.
 * Öncelik: oturum sonu çağrısı (A1-K107) > kapanmış parti (A1-K32; birleştirilmiş, A4-K102)
 * > ızgara tick'i (yalnız kart; A1-K117, A4-K18). Dönüş IDA_LINK_CAGRI_YOK değilse *in
 * baştan yazılmıştır (sıfırlanır, sonra doldurulur): now_w (kartta platform saati; SIL'de
 * A4-K11), tf, imu, samples (emniyet örnekleri başta; A1-K124), ping360, algılama, narrow
 * (mandal verilir ve temizlenir; A4-K46), cmds (≤ 8; fazlası ertelenir; A4-K105), evres
 * (≤ 4; A4-K68), plat'ın bu modüle ait alanları (link_state, link_event, batch_merged,
 * input_dropped ve cnt_* sayaçları). Platform kendi alanlarını sonra yazar (dosya başı).
 * YOK dönüşünde *in yazılmaz. l ya da in NULL ise YOK.
 */
ida_link_cagri_t ida_link_ust_take(ida_link_ust_t *l, ida_in_t *in);

/*
 * ida_link_ust_put_out — take'in verdiği çağrı döndü. in take'in doldurup çekirdeğe verdiği
 * girdi, rc çağrının dönüşü, out çıktısıdır. Çıktıyı A1 çerçevelerine çevirip sınıfıyla
 * gönderir (A1-K90 sırası): ilk kabul kapısından (A4-K67) geçen yeni IDA_EVENT'ler ve
 * IDA_ACTUATOR (KRITIK), IDA_CMD_RESULT'lar (ZORUNLU), IDA_STATUS/FC_STATUS (100 ms),
 * IDA_CALL_TRACE, IDA_NAV, IDA_LOG, sayaç çerçeveleri (1 s), IDA_DET_RECORD
 * (KAYBEDILEBILIR). Çağrı sürerken oturum bittiyse çıktı gönderilmez, olaylarına
 * SESSION_END döner (A4-K71). Oturum sonu çağrısında yalnız IDA_CALL_TRACE gönderilir
 * (A1-K22 (3)) ve durum IDLE olur; bekleyen SIM_HELLO o zaman yanıtlanır (A1-K107).
 * take'siz çağrılırsa hiçbir şey yapmaz. out NULL ise çıktı gönderilmez, olay yoktur.
 */
void ida_link_ust_put_out(ida_link_ust_t *l, const ida_in_t *in, ida_rc_t rc,
                          const ida_out_t *out);

/*
 * ida_link_ust_akis_kapandi — SIL: TCP bağlantısı kapandı ya da yeni bağlantı açıldı
 * (A1-K99). Oturum varsa biter (A1-K22; SESSION_CLOSE gönderilmez), alıcının yarım adayı
 * sayaçlara yansımadan atılır (A1-K07), seq geçmişi yeni akışa göre sıfırlanır.
 */
void ida_link_ust_akis_kapandi(ida_link_ust_t *l);

/*
 * ida_link_ust_guvenli_bekleme — platform güvenli beklemeye girdi (A1-K13, K21, H33):
 * oturum varsa IDA_SESSION_CLOSE(SAFE_STATE) gönderilmeye çalışılır ve oturum biter; bundan
 * sonra SIM_HELLO yanıtlanmaz ve duyurular init_rc = NOT_INIT taşır. Geri alınmaz.
 */
void ida_link_ust_guvenli_bekleme(ida_link_ust_t *l);

/*
 * ida_link_ust_cagri_acik — çağrı penceresi açık mı: take IDA_LINK_CAGRI_YOK dışında bir değer
 * döndürdükten sonra put_out çağrılana kadar true (W3-K10). Pencere açıkken
 * ida_link_ust_rx çağrılmaz (dosya başı); l NULL ya da kurulmamışsa false.
 */
bool ida_link_ust_cagri_acik(const ida_link_ust_t *l);

/* Kartın A1 durumu ve güncel oturum kimliği (oturum yoksa 0). */
ida_link_ust_durum_t ida_link_ust_durum(const ida_link_ust_t *l);
uint16_t ida_link_ust_sess(const ida_link_ust_t *l);

/* IDA_LINK_STATS.cnt[] (A1 Tablo 3.11b) değerlerini yazar; 0–10 tel katmanından. */
void ida_link_ust_sayaclar(const ida_link_ust_t *l, uint32_t cnt[IDA_LINK_SAY_N]);

#ifdef __cplusplus
}
#endif

#endif /* IDA_LINK_UST_H */
