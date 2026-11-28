/*
 * ust_ic.h — ida_link üst katmanının iç bildirimleri (yalnız src/ust_*.c ve birim testleri).
 *
 * Dosyalar: ust_oturum.c (kurulum, alım dağıtımı, el sıkışma, oturum sonu, zamanlayıcılar),
 * ust_parti.c (pencere, parti kapanışı, birleştirme, Ping360, ızgara, take), ust_komut.c
 * (komut dönüşümü sonrası mandal, D-17 atımı, nihai kimlikler, kuyruk), ust_cikti.c
 * (put_out, olay tablosu ve kovalar, telemetri), ust_esleme.c (A1 Ek A eşlemesinin tek yeri).
 */
#ifndef IDA_LINK_UST_IC_H
#define IDA_LINK_UST_IC_H

/* Üretilmiş MAVLink başlığından yalnız mesaj yapıları ve kimlik sabitleri kullanılır; kanal
   dizileri ve onlara dokunan işlevler kullanılmaz (A1-K101; src/tel.c ile aynı önlem). */
#define MAVLINK_EXTERNAL_RX_STATUS 1
#define MAVLINK_EXTERNAL_RX_BUFFER 1
#include "mavlink/mavlink_types.h"
extern mavlink_status_t m_mavlink_status[MAVLINK_COMM_NUM_BUFFERS];
extern mavlink_message_t m_mavlink_buffer[MAVLINK_COMM_NUM_BUFFERS];
#include "mavlink/ida/mavlink.h"

#include "ida_link/ust.h"

/* hdr.flags'ten VALID'i silen maske. `~` int üzerinde çalışıp 16 bite sığmayan sabit üretir
   (MSVC C4310); XOR ile değer 0xFFFE'dir ve daraltma yoktur. */
#define UST_HF_VALID_SIL ((uint16_t)(0xFFFFu ^ IDA_HF_VALID))

/* ---- ust_oturum.c ---- */

/* Platform saati, A4-K08 aralığına (0 … 2^62 ns) sınırlanmış (zaman tabanı doğrulaması). */
ida_wtime_t ust_simdi(ida_link_ust_t *l);
ida_wtime_t ust_zaman_sinirla(ida_wtime_t t);
/* Tam bir mesajı kodlar ve sınıfıyla platforma verir. Oturum dışında yalnız BOARD_HELLO,
   KEEPALIVE, SESSION_CLOSE ve CALL_TRACE geçer (A1-K11). ZORUNLU sınıfta kuyruk doluysa
   oturum TX_OVERFLOW ile biter (A1-K110). Dönüş: çerçeve kuyruğa verildi. */
bool ust_gonder(ida_link_ust_t *l, ida_link_sinif_t sinif, uint32_t msgid, const void *mesaj,
                size_t boy);
/* Oturum sonu (A1-K22). neden != 0 ise SESSION_CLOSE(neden) gönderilmeye çalışılır. */
void ust_oturum_bitir(ida_link_ust_t *l, uint8_t neden);
/* Oturum sonu çağrısı döndü: IDLE ve bekleyen SIM_HELLO'nun yanıtı (A1-K107). */
void ust_oturum_sonu_dondu(ida_link_ust_t *l);
/* IDA_LINK_STATS.cnt[] (0–10 tel katmanından) ve plat.cnt_rx_error. */
uint32_t ust_rx_hata(const ida_link_ust_t *l);

/* ---- ust_parti.c ---- */

void ust_parti_bosalt(ida_link_ozel_parti_t *p);
/* Oturum sonunda pencere, bekleyen girdi ve Ping360 yuvaları silinir (A4-K74). */
void ust_parti_oturum_sonu(ida_link_ust_t *l);
/* Oturum kuruldu: pencere ve parti izleri sıfırlanır (A1-K31 ilk TF). */
void ust_parti_oturum_basi(ida_link_ust_t *l);
/* Izgara takvimini oturumsuz (mutlak 10 ms) kipe alır. */
void ust_izgara_mutlak(ida_link_ust_t *l, ida_wtime_t simdi);
/* Üye çerçeveleri (RUNNING ve sess geçerli olduğu denetlenmiş). */
void ust_uye_imu(ida_link_ust_t *l, const mavlink_ida_imu_t *m, ida_wtime_t t_rx);
void ust_uye_ornek(ida_link_ust_t *l, uint32_t msgid, const void *m, ida_wtime_t t_rx);
void ust_uye_ping_veri(ida_link_ust_t *l, const mavlink_ida_ping360_data_t *m);
void ust_uye_ping_bas(ida_link_ust_t *l, const mavlink_ida_ping360_head_t *m, ida_wtime_t t_rx);
void ust_uye_det_batch(ida_link_ust_t *l, const mavlink_ida_det_batch_t *m, ida_wtime_t t_rx);
void ust_uye_det_record(ida_link_ust_t *l, const mavlink_ida_det_record_t *m, ida_wtime_t t_rx);
void ust_parti_tf(ida_link_ust_t *l, const mavlink_ida_time_frame_t *m, ida_wtime_t t_rx);

/* ---- ust_komut.c ---- */

/* Komut üyesi (RUNNING, sess güncel): dönüşüm, mandal, D-17, nihai kimlikler, kuyruk. */
void ust_komut_al(ida_link_ust_t *l, uint32_t msgid, const void *m);
/* Platformun komut sonucu (A1-K63, K64, K67); sonuç türü komut türünden (A4-K43). */
void ust_komut_sonuc(ida_link_ust_t *l, uint32_t cmd_id, uint8_t cmd_type, uint16_t param_id,
                     uint8_t ack, uint16_t reason);
/* last_cmd_id'yi A1-K123 anlamında en yeni kimlikle günceller (A1-K66). */
void ust_last_cmd_id(ida_link_ust_t *l, uint32_t cmd_id);
/* Pencere kapanınca açık komutlar kapanmış sayılır; DUP/OLD'da açık komutlar atılır. */
void ust_komut_pencere_kapandi(ida_link_ust_t *l);
void ust_komut_pencere_atildi(ida_link_ust_t *l);
/* take: kapanmış komutlardan en çok 8'i; kalanlar ertelenir (A4-K105). */
uint8_t ust_komut_cek(ida_link_ust_t *l, ida_cmd_t *cmds);

/* ---- ust_cikti.c ---- */

/* Olay sonucu (sim -> kart; RUNNING, sess güncel). */
void ust_olay_sonucu(ida_link_ust_t *l, const mavlink_ida_event_result_t *m);
/* Olay sonuçları parti üyesidir: olağan kapanışta etkinleşir, DUP/OLD'da atılır. */
void ust_olay_pencere_kapandi(ida_link_ust_t *l);
void ust_olay_pencere_atildi(ida_link_ust_t *l);
/* Oturum sonu: onaylanmamış olaylara SESSION_END (A4-K71). */
void ust_olay_oturum_sonu(ida_link_ust_t *l);
/* Zamana bağlı olay işleri: kova, yineleme, sonuç payı (A1-K80, K120). */
void ust_olay_yokla(ida_link_ust_t *l, ida_wtime_t simdi);
/* take: çekirdeğe verilecek olay sonuçları (A4-K68), en çok IDA_PORT_MAX_EVRES. */
uint8_t ust_olay_sonuclari_cek(ida_link_ust_t *l, ida_event_result_t *evres);
/* CRC-32/ISO-HDLC (A1-K68). */
uint32_t ust_crc32(const uint8_t *p, size_t n);

/* ---- ust_esleme.c: A1 Ek A (tek yer) ---- */

/* sim -> kart (A1 -> A4). */
void ust_esl_tf(const mavlink_ida_time_frame_t *m, ida_wtime_t t_rx, ida_time_frame_t *o);
void ust_esl_imu(const mavlink_ida_imu_t *m, ida_wtime_t t_rx, ida_imu_sample_t *o);
/* MAG, BARO, GNSS, DVL, PING1D, BATT, LEAK, SAFETY -> ida_sample_t (Tablo 3.4b, A1-K47).
   Dönüş false: msgid bu mesajlardan değil (*o yazılmaz). *rev SAFETY'de safety_rev, öteki
   türlerde 0 (NULL olabilir). */
bool ust_esl_ornek(uint32_t msgid, const void *m, ida_wtime_t t_rx, ida_sample_t *o,
                   uint32_t *rev);
/* PING360_HEAD'in başlık alanları; o->data'ya dokunmaz (veri ayrı birleştirilir; A1-K49–K51). */
void ust_esl_ping_bas(const mavlink_ida_ping360_head_t *m, ida_wtime_t t_rx, ida_ping360_t *o);
void ust_esl_det_batch(const mavlink_ida_det_batch_t *m, ida_det_batch_t *o);
void ust_esl_det_record(const mavlink_ida_det_record_t *m, ida_det_record_t *o);
void ust_esl_evres(const mavlink_ida_event_result_t *m, ida_event_result_t *o);

/* Komut dönüşümü (Tablo 3.6, 3.6a; A1-K64, K122). */
typedef enum {
    UST_KOMUT_OLAGAN   = 0,  /* *cmd dolu; cmds[]'e gider                              */
    UST_KOMUT_DARALTICI = 1, /* *bit (IDA_NARROW_*) ve *kip (NARROW_MODE'da) dolu       */
    UST_KOMUT_BOZUK    = 2   /* LINK_BAD_FIELD (A1-K64)                                */
} ust_komut_tur_t;
typedef struct {
    uint32_t cmd_id;
    uint8_t  cmd_type;       /* IDA_CMD_* (bozukta mesajdaki ham tür)                   */
    uint16_t param_id;       /* PARAM_* sonuçlarında yansıtılan kimlik                  */
    uint8_t  genisletici;    /* A1 Tablo 3.6                                           */
    uint8_t  bit;            /* DARALTICI: IDA_NARROW_*                                 */
    uint8_t  kip;            /* DARALTICI ve NARROW_MODE: hedef kip                     */
    uint8_t  bicim_hatali;   /* DARALTICI: biçim hatası (W5-K7/G-K14); uygulama sürer   */
    ida_cmd_t cmd;           /* OLAGAN                                                 */
} ust_komut_t;
/* msgid bir komut mesajı değilse BOZUK döner ve cmd_id 0 olur. */
ust_komut_tur_t ust_esl_komut(uint32_t msgid, const void *m, ust_komut_t *k);
/* A1-K122: görev öğesi Tablo 3.6a'ya uyuyor mu. */
bool ust_esl_gorev_ogesi_gecerli(const mavlink_ida_cmd_mission_item_t *m);

/* kart -> sim (A4 -> A1). */
uint16_t ust_esl_pwm(float thr, uint16_t alt, uint16_t notr, uint16_t ust);
uint8_t ust_esl_health(const ida_health_t *h);
void ust_esl_event(const ida_event_t *e, uint8_t call_kind, uint32_t safety_rev, uint16_t sess,
                   mavlink_ida_event_t *m);
void ust_esl_cmd_result(const ida_cmd_result_t *r, uint16_t sess, uint32_t save_crc,
                        mavlink_ida_cmd_result_t *m);
void ust_esl_nav(const ida_nav_out_t *n, uint32_t batch_seq, mavlink_ida_nav_t *m);
/* STATUS: çekirdeğin durumu + platformun kayıt alanları (A1 §3.7). */
void ust_esl_status(const ida_status_t *s, const ida_health_t *h, const ida_plat_t *p,
                    uint8_t link_state, uint32_t param_blob_crc, uint32_t batch_seq,
                    uint16_t sess, mavlink_ida_status_t *m);
void ust_esl_fc_status(const ida_fc_status_t *f, mavlink_ida_fc_status_t *m);
void ust_esl_log(const ida_log_t *g, mavlink_ida_log_t *m);
void ust_esl_core_counters(const ida_core_counters_t *c, mavlink_ida_core_counters_t *m);
void ust_esl_det_tx(const ida_det_record_t *d, mavlink_ida_det_record_t *m);
/* BOARD_HELLO: açılış kaydı ve ida_core_info (A1 §3.8). */
void ust_esl_board_hello(const ida_link_ust_ayar_t *a, uint32_t peer_nonce, uint32_t nonce,
                         uint8_t init_rc, uint32_t blob_crc, uint32_t blob_len,
                         uint32_t blob_gen, uint32_t last_cmd_id, uint16_t prev_sess,
                         uint16_t sessions_since_boot, mavlink_ida_board_hello_t *m);

#endif /* IDA_LINK_UST_IC_H */
