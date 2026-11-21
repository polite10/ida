/*
 * destek_oturum.h — ida_link üst katmanı birim testlerinin ortak yardımcıları (test kodu).
 *
 * Sahte platform: ayarlanabilir saat, gönderilen çerçevelerin bir SIM rolü alıcısıyla çözülüp
 * kaydı (tel katmanının alıcısı; her gönderim tam bir geçerli çerçeve olmalıdır), sınıf
 * başına "kuyruk dolu" anahtarı, nonce sayacı, yeniden başlatma ve kalıcı blok değerleri.
 * Sahte sim: tel katmanının kodlayıcısıyla (SIM rolü) çerçeve kurup modüle verir.
 */
#ifndef IDA_DESTEK_OTURUM_H
#define IDA_DESTEK_OTURUM_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "ust_ic.h"

#define DT_MS INT64_C(1000000)
#define DT_KAYIT 1024u

typedef struct {
    uint32_t msgid;
    ida_link_sinif_t sinif;
    uint8_t yuk[IDA_LINK_YUK_AZAMI];   /* çözülmüş tipli yapı (ida_link_coz) */
} dt_cerceve_t;

typedef struct {
    int64_t saat;
    ida_link_ust_t l;
    ida_link_ust_ayar_t ayar;
    ida_link_platform_t plat;
    ida_link_alici_t sim_alici;
    ida_link_gonderici_t sim_g;
    dt_cerceve_t kayit[DT_KAYIT];
    unsigned n;
    bool dolu[4];                      /* sınıf dizini 1–3: gonder false döner */
    unsigned temizleme;
    unsigned yeniden_baslatma;
    uint32_t nonce_sayac;
    uint32_t blok_crc, blok_len, blok_gen;
    uint16_t sess;
    uint32_t sim_nonce;
} dt_t;

extern dt_t DT;

/* Varsayılan ayarla kurar (boot_count 3, T_bağ 500 ms, ESC 1100/1500/1900). */
void dt_kur(ida_link_ust_rol_t rol);
void dt_ilerle(int64_t ns);
void dt_kayit_sil(void);

/* Sim: tipli yapıyı SIM rolüyle kodlayıp modüle verir (t_rx = saat). */
void dt_gonder(uint32_t msgid, const void *m, size_t boy);
#define DT_GONDER(id, m) dt_gonder((id), &(m), sizeof(m))

/* Kayıt sorguları. */
unsigned dt_say(uint32_t msgid);
/* msgid'nin k'ıncı (0'dan) kaydı; yoksa NULL. */
const dt_cerceve_t *dt_bul(uint32_t msgid, unsigned k);
/* msgid'nin son kaydının yükü; yoksa test başarısız. */
const void *dt_son(uint32_t msgid);
/* Verilen cmd_id için son CMD_RESULT (yoksa NULL). */
const mavlink_ida_cmd_result_t *dt_sonuc(uint32_t cmd_id);

/* El sıkışma: SIM_HELLO(nonce) -> BOARD_HELLO -> SESSION_ACCEPT(OK, sess). */
void dt_el_sikis(uint32_t sim_nonce, uint16_t sess);

/* Üyeler ve TF (güncel DT.sess ile). */
void dt_tf(uint32_t seq, int64_t t, uint8_t n_members, bool imu);
void dt_imu(uint32_t seq, int64_t t);
void dt_mag(uint32_t seq, int64_t t);
void dt_emniyet(uint32_t seq, int64_t t, uint8_t estop, uint32_t rev);
void dt_cmd_simple(uint32_t cmd_id, uint8_t type, uint16_t arg);
void dt_cmd_fc(uint32_t cmd_id, uint8_t op, uint8_t tube);

/* take; YOK değilse in doludur. */
ida_link_cagri_t dt_take(ida_in_t *in);
/* Çekirdeğin yerine geçen basit çıktı: rc OK, thr_valid = is_safe = 1, nötr, her mandal biti
   ve komut için ACCEPTED; ek olaylar (NULL olabilir). put_out çağırır. */
void dt_put(const ida_in_t *in, ida_link_cagri_t tur, ida_rc_t rc, const ida_event_t *olaylar,
            uint8_t n_olay);
/* Varsayılan çıktıyı yazar (dt_put'un kullandığı). */
void dt_cikti(const ida_in_t *in, ida_link_cagri_t tur, ida_out_t *out);

#endif /* IDA_DESTEK_OTURUM_H */
