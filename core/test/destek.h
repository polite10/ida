/*
 * destek.h — çekirdek birim testlerinin ortak yardımcıları (test kodu; çekirdeğe girmez).
 *
 * Testler yalnız port.h üzerinden çağırır; iç durumu görmek ya da kurmak gereken yerde
 * cekirdek.h'ın yapı alanlarına doğrudan yazar (M9: çekirdekte test kipi yoktur).
 */
#ifndef IDA_TEST_DESTEK_H
#define IDA_TEST_DESTEK_H

#include <stdbool.h>
#include <stdint.h>

#include "cekirdek.h"
#include "ida/port.h"

#define DS_MS 1000000LL /* 1 ms, ns */

/* Geçerli init girdisi: sürüm doğru, now_w = 1 s, boot_count = 7, blok yok. */
void ds_init_girdisi(ida_init_in_t *ii);
/* Varsayılan değerlerle geçerli parametre bloğu (uzunluğu döner). */
uint32_t ds_gecerli_blok(uint8_t *blok);
/* Geçerli blokla init (IDA_RC_OK beklenir). */
ida_rc_t ds_kur(ida_core_t *c);
/* Bloksuz init (IDA_RC_PARAM_DEFAULTS beklenir). */
ida_rc_t ds_kur_bloksuz(ida_core_t *c);

/* Olağan girdi: hepsi sıfır, now_w = t_ns, tf (present, seq, t = t_ns, t_rx = t_ns),
   step ise taze ve geçerli IMU (seq = imu_seq, t_meas = t_ns, t_rx = t_ns, window 5 ms),
   oturum RUNNING. */
void ds_girdi(ida_in_t *in, int64_t t_ns, uint32_t seq, bool step);
/* Geçerli IMU örneği. */
void ds_imu(ida_imu_sample_t *s, int64_t t_ns, uint32_t seq);
/* Emniyet örneği: gevşetilmiş (estop 0, fiş takılı, pim çıkık, tüp dolu) değerler. */
void ds_emniyet(ida_sample_t *s, int64_t t_ns, uint32_t seq);
/* Geçerli MAG örneği. */
void ds_mag(ida_sample_t *s, int64_t t_ns, uint32_t seq);

/* Çıktıyı bilinen desenle doldurur (A4-K27 sınaması). */
void ds_desen(ida_out_t *out);
/* Çıktının sözleşme değişmezleri (A4-K28, K53–K58, K64, K65, K87, K90, K91, K120, K121):
   ihlalde Unity hatası. rc çağrının dönüşü. */
void ds_cikti_degismezleri(ida_rc_t rc, const ida_in_t *in, const ida_out_t *out);

/* Sonuç dizisinde cmd_id'si verilen sonucu bulur (yoksa NULL). */
const ida_cmd_result_t *ds_sonuc(const ida_out_t *out, uint32_t cmd_id);
/* Günlükte kodun kaç kez bulunduğu. */
uint32_t ds_gunluk_say(const ida_out_t *out, uint16_t kod);
/* Belirli türde olay sayısı. */
uint32_t ds_olay_say(const ida_out_t *out, uint8_t kind);

/* Belirlenimci sözde rastgele üreteç (xorshift32). */
typedef struct {
    uint32_t s;
} ds_rastgele_t;
uint32_t ds_rastgele(ds_rastgele_t *r);

#endif /* IDA_TEST_DESTEK_H */
