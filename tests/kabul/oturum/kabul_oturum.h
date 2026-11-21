/*
 * kabul_oturum.h — ida_link üst katmanının (oturum, parti, komut sırası, olay) bağımsız kabul
 * testlerinin ortak düzeneği.
 *
 * Kaynaklar (yalnız bunlar): docs/sozlesmeler/A1-kart-baglantisi.md (§3.6, §3.8, §4.2–§4.8),
 * docs/sozlesmeler/A4-port.md (platform kuralları: A4-K16, K43–K48, K67, K68, K71–K74,
 * K102, K104, K105, K122), core/include/ida/port.h, docs/gorevler/W3-oturum.md (kabul md.2,
 * md.4), docs/gorevler/W1-uyum.md (D-17), modülün dışa açık başlığı
 * protocol/ida_link/include/ida_link/ust.h (yalnız arayüz) ve tel katmanının dışa açık başlığı
 * (ida_link/tel.h; test baytlarını üretmek ve kartın çerçevelerini çözmek için). Modülün
 * uygulama kaynağı (protocol/ida_link/src/ust_*), iç başlığı ve uygulayıcının testleri
 * okunmadı.
 *
 * Düzenek:
 *  - Sahte platform işlev tablosu (ida_link_platform_t): denetlenen saat, her gönderilen
 *    çerçeveyi sim rolündeki bir tel alıcısıyla çözüp günlüğe yazan gönderici, sayaçlı
 *    kuyruk_temizle, artan nonce, boş kalıcı blok.
 *  - Sim tarafı: sim rolündeki tel göndericisiyle (ida_link_gonder) gerçek A1 baytları üretilir
 *    ve ida_link_ust_rx ile verilir. TIME_FRAME'in n_members'ı pencerede gönderilen üye
 *    sayısından kurulur (A1-K30).
 *  - Sahte çekirdek: take'in verdiği girdiyi kaydeder ve A4-K43'e göre sonuçlar, istenirse bir
 *    SAFETY_CLOSE olayı (A4-K60, K64) üreten bir çıktı kurar; put_out'a verir.
 *  - Komut kimliği tablosu: her cmd_id için alınan IDA_CMD_RESULT'lar (sınıflarıyla) ve
 *    çağrı girdilerine kaç kez girdiği.
 *
 * Çağrı akışı ust.h'nin dosya başındaki tariftir: take → (platform alanları) → çekirdek →
 * put_out; take ile put_out arasında rx ve poll_tx çağrılabilir.
 */
#ifndef KABUL_OTURUM_H
#define KABUL_OTURUM_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "ida/port.h"
#include "ida_link/tel.h"
#include "ida_link/ust.h"

#include "mavlink/ida/mavlink.h"

/* ---- Sabitler --------------------------------------------------------------------------- */
#define KO_BOOT_COUNT     UINT32_C(7)
#define KO_P_NS           INT64_C(5000000)          /* A1 §3.9                             */
#define KO_MS             INT64_C(1000000)
#define KO_T0_SIM         INT64_C(1000000000)       /* TF(0)'ın sim zamanı (1 s)           */
#define KO_T0_DUVAR       INT64_C(2000000000)       /* kart rolünde başlangıç saati         */
#define KO_KIMLIK_AZAMI   65536u                    /* cmd_id tablosu (testler bunun altında) */
#define KO_GUNLUK_AZAMI   8192u
#define KO_CAGRI_AZAMI    8192u
#define KO_SONUC_AZAMI    8u

/* ---- Kayıtlar --------------------------------------------------------------------------- */
typedef struct {
    uint32_t msgid;
    uint8_t  sinif;          /* ida_link_sinif_t                                          */
    uint8_t  len;
    int64_t  t_ns;           /* gönderildiği andaki sahte saat                            */
    uint32_t cagri_sayisi;   /* gönderildiği anda tamamlanmış (put_out) çağrı sayısı       */
    uint8_t  yuk[IDA_LINK_YUK_AZAMI];
} ko_cerceve_t;

typedef struct {
    ida_link_cagri_t   tur;
    ida_wtime_t        now_w;
    ida_time_frame_t   tf;
    uint8_t            link_event;
    uint8_t            link_state;
    uint8_t            n_samples;
    ida_narrow_t       narrow;
    uint8_t            n_cmds;
    ida_cmd_t          cmds[IDA_PORT_MAX_CMDS];
    uint8_t            n_evres;
    ida_event_result_t evres[IDA_PORT_MAX_EVRES];
    bool               ping360_var;
    size_t             gunluk_take;   /* take anında günlük boyu                           */
    size_t             gunluk_put;    /* put_out çağrılmadan hemen önce günlük boyu        */
    bool               bitti;         /* put_out yapıldı                                   */
} ko_cagri_t;

typedef struct {
    uint16_t reason;
    uint16_t sess;
    uint8_t  ack;
    uint8_t  kind;
    uint8_t  sinif;
    size_t   gunluk_sira;    /* günlükteki sırası (günlük taşmadıysa)                    */
} ko_sonuc_t;

typedef struct {
    uint16_t   girdi;        /* çağrı girdilerine giriş sayısı (cmds ya da narrow yuvası) */
    uint32_t   ilk_cagri;    /* ilk girdiği çağrının sırası (girdi > 0 ise)               */
    uint16_t   n_sonuc;
    ko_sonuc_t sonuc[KO_SONUC_AZAMI];
} ko_kimlik_t;

/* ---- Düzenek ---------------------------------------------------------------------------- */
/* Örneği ve bütün kayıtları baştan kurar; modül kurulumunun TAMAM döndüğünü iddia eder. */
void ko_kur(ida_link_ust_rol_t rol);
ida_link_ust_rol_t ko_rol(void);
ida_link_ust_t *ko_l(void);

/* Sahte saat (platformun zaman işlevi). */
void    ko_saat_kur(int64_t ns);
int64_t ko_saat(void);
/* Kart rolü: `ms` milisaniye, 1 ms adımla ilerler; her adımda pompalar; oturum varsa sim her
   50 ms'de bir IDA_KEEPALIVE gönderir (T_bağ dolmasın; A1-K20, K22). */
void    ko_bekle_ms(uint32_t ms);

/* ---- Sim tarafı çerçeveleri (rx) -------------------------------------------------------- */
/* W3-K10: take (YOK dışı) ile put_out arasında rx çağrılmaz. Düzenek bunu iddia eder; yalnız
   ihlal vakasını sınayan test izin verir. */
void ko_pencere_ihlali_izni(bool izin);
void ko_gonder_ham(uint32_t msgid, const void *mesaj, size_t boy, bool uye);
void ko_sim_hello(uint32_t nonce);
void ko_session_accept(uint32_t nonce_sim, uint32_t nonce_board, uint16_t sess, uint8_t sonuc);
void ko_session_close(uint16_t sess, uint8_t neden);
void ko_keepalive(uint16_t sess);
/* TIME_FRAME(seq): t_sim = KO_T0_SIM + seq·P; n_members penceredeki üye sayısı; SIL rolünde
   saat t_sim'e kurulur (A4-K11). _s: verilen sess ile (eski oturum denemesi). */
void ko_tf(uint32_t seq);
void ko_tf_s(uint16_t sess, uint32_t seq);
int64_t ko_tf_t(uint32_t seq);
/* Komutlar (güncel oturumla; _s: verilen sess ile). */
void ko_simple(uint32_t cmd_id, uint8_t type, uint16_t arg);
void ko_simple_s(uint16_t sess, uint32_t cmd_id, uint8_t type, uint16_t arg);
void ko_fc(uint32_t cmd_id, uint8_t op, uint8_t tube, uint32_t arg0);
void ko_fc_s(uint16_t sess, uint32_t cmd_id, uint8_t op, uint8_t tube, uint32_t arg0);
void ko_evres(uint64_t event_id, uint8_t sonuc, int64_t t_apply);
void ko_evres_s(uint16_t sess, uint64_t event_id, uint8_t sonuc, int64_t t_apply);

/* El sıkışma (A1-K13, K16): SIM_HELLO(nonce) → BOARD_HELLO(peer_nonce = nonce) → ACCEPT(OK).
   Durumun RUNNING ve sess'in verilen değer olduğunu iddia eder; kartın nonce'unu döndürür.
   Tutulan çağrı varsa önce bırakılır. */
uint32_t ko_el_sikis(uint32_t nonce_sim, uint16_t sess);
uint16_t ko_sess(void);
/* ko_kur + el sıkışma + TF(1) + pompala. */
void ko_hazir(ida_link_ust_rol_t rol, uint16_t sess);

/* ---- Çağrı akışı ------------------------------------------------------------------------ */
/* poll_tx; tutulan çağrı yoksa take → sahte çekirdek → put_out döngüsü (YOK'a kadar). */
void ko_pompala(void);
/* poll_tx ve tek bir take; çağrı gelirse kaydeder ve tutar (put_out yapılmaz). Çağrı geldiyse
   kaydının sırasını, gelmediyse -1 döndürür. Zaten tutulan varsa iddia hatası. */
long ko_tut(void);
/* Tutulan çağrının put_out'u; pompala true ise ardından ko_pompala. */
void ko_birak(bool pompala);
bool ko_tutuluyor(void);
/* Tek bir take + put_out (arada rx yok); çağrı geldiyse kaydının sırası, yoksa -1. */
long ko_tek_cagri(void);

/* Sahte çekirdek: sıradaki zaman çerçeveli çağrının (STEP ya da TICK_TF) çıktısına bir
   SAFETY_CLOSE olayı ekler (tube = IDA_TUBE_ALL, hold 0). Olay kimliği ko_son_olay_id(),
   çıkaran çağrının now_w'si ko_son_olay_now_w(). */
void     ko_olay_planla(uint32_t apply_by_ns);
/* Sahte çekirdek oturum sonu çağrısında da bir SAFETY_CLOSE üretsin mi (A4-K73). */
void     ko_oturum_sonu_olayi(bool uret);
uint64_t ko_son_olay_id(void);
int64_t  ko_son_olay_now_w(void);

/* ---- Kayıtlara erişim ------------------------------------------------------------------- */
size_t              ko_gunluk_boy(void);
const ko_cerceve_t *ko_gunluk(size_t i);
size_t              ko_cagri_boy(void);
const ko_cagri_t   *ko_cagri(size_t i);
const ko_kimlik_t  *ko_kimlik(uint32_t cmd_id);
uint32_t            ko_kuyruk_temizle_sayisi(void);
/* Bozuk ya da çözülemeyen gönderilmiş çerçeve sayısı (0 olmalı). */
uint32_t            ko_bozuk_cerceve(void);

/* Günlükteki çerçeveyi tipli yapıya çözer (msgid uyuşmazsa iddia hatası). */
void ko_coz(const ko_cerceve_t *c, uint32_t msgid, void *mesaj, size_t boy);
/* Günlükte `bas`tan itibaren peer_nonce'u verilen BOARD_HELLO'yu arar (ilkini). */
bool ko_board_hello_bul(size_t bas, uint32_t peer_nonce, mavlink_ida_board_hello_t *bh,
                        size_t *sira);

/* Yardımcı sorgular. */
bool     ko_cagri_komut_iceriyor(const ko_cagri_t *c, uint32_t cmd_id);
bool     ko_cagri_mandal_iceriyor(const ko_cagri_t *c, uint32_t cmd_id);
/* İlk çağrı sırası `bas`tan itibaren, cmd_id'yi cmds'te ya da mandal yuvasında taşıyan
   çağrının sırası; yoksa -1. */
long     ko_komutu_tasiyan_cagri(size_t bas, uint32_t cmd_id);
/* cmd_id için alınan sonuçlardan (ack, reason) eşleşenlerin sayısı. */
uint32_t ko_sonuc_say(uint32_t cmd_id, uint8_t ack, uint16_t reason);
/* A4-K104: komut genişletici mi. */
bool     ko_genisletici(const ida_cmd_t *c);

/* ---- Sözde rastgele (sabit tohum; splitmix64) ------------------------------------------- */
typedef struct {
    uint64_t s;
} ko_rng_t;
void     ko_rng_kur(ko_rng_t *r, uint64_t tohum);
uint32_t ko_rng_u32(ko_rng_t *r);
uint32_t ko_rng_aralik(ko_rng_t *r, uint32_t n);   /* [0, n), n > 0 */

/* Biçimli Unity iletisi. */
void ko_bilgi(const char *bicim, ...);

#endif /* KABUL_OTURUM_H */
