/*
 * komut.h — komut dağıtıcısı ve işleyicilerin ortak bağlamı (W2; A4-K43…K48, K104, K114,
 * K119, K120, K124; tablo 3.17).
 *
 * Dağıtıcı (komut.c) her komut için sırayla: (1) yinelenen cmd_id → DUPLICATE (A4-K48);
 * (2) genişleticiyse NARROWED > IMU_REJECTED > IMU_LAG (A4-K45, K103, K108); FC'de K119
 * biçim denetimi; ARM'da W2'nin kuralları (EVENTS_OFF, PARAM_INVALID, PARAM_BUSY); sonra
 * komutun sahibi modülün işleyicisini çağırır. İşleyiciler yalnız tablo 3.17'deki kendi
 * satırlarını uygular ve r->ack / r->reason (gerekirse value / save) yazar; r->cmd_id ve
 * r->kind dağıtıcıca yazılmıştır. Genişletici bir komut işleyiciye ancak (1)–(2)'den
 * geçmişse ulaşır.
 */
#ifndef IDA_CORE_KOMUT_H
#define IDA_CORE_KOMUT_H

#include <stdbool.h>
#include <stdint.h>

#include "ida/port.h"

/* Bir kabul edilen çağrının komut işleme bağlamı (çağrı boyunca sabit). */
typedef struct {
    ida_wtime_t now_w;    /* kullanılan now_w (A4-K10)                         */
    bool daraldi;         /* (b) adımında daraltıcı girdi ya da görev adımında arıza
                             daraltması uygulandı (A4-K45; W3-K9)              */
    bool imu_red;         /* step'in IMU'su reddedildi (A4-K103)               */
    bool imu_lag;         /* IMU_LAG mandalı kurulu (A4-K108)                  */
    bool armed;
    bool param_ok;
    bool events_ok;
    bool cagrida_arm;     /* cmds[]'te (sırasından bağımsız) ARM var (A4-K81, K82) */
    bool oturum_var;      /* plat.link_state = RUNNING                          */
    uint8_t mode;
    uint8_t est_level;
    uint8_t fs_level;
    uint8_t param_store;  /* plat.param_store                                   */
    uint32_t fc_engel;    /* fc.inhibit_mask'in komutlardan önceki değeri (3.17) */
} ida_komut_baglam_t;

/* Komut türünün sonuç türü (A4-K43). */
uint8_t ida_kom_sonuc_turu(uint8_t cmd_type);
/* A4-K104: genişletici komut mu. */
bool ida_kom_genisletici(const ida_cmd_t *cmd);
/* A4-K44/K46: cmds[]'te bulunan daraltıcı komut mu (platform hatası; (b)'de uygulanır). */
bool ida_kom_daraltici(const ida_cmd_t *cmd);

/* Kabul edilen çağrıda bütün sonuçları yazar (A4-K43, K124): mandal bitleri dizin sırasıyla,
   sonra cmds[] dizi sırasıyla; out->n_results'ı kurar. */
void ida_kom_isle(ida_core_t *c, const ida_in_t *in, const ida_komut_baglam_t *b, ida_out_t *out);
/* Reddedilen çağrı (A4-K114): uygulanan mandal bitleri ve cmds[]'teki daraltıcılar ACCEPTED;
   n_cmds ≤ 8 ise diğer komutlar REJECTED / CALL_REJECTED, > 8 ise cmds[] için sonuç yok. */
void ida_kom_reddet(const ida_in_t *in, ida_core_counters_t *cnt, ida_out_t *out);
/* NOT_INIT (A4-K114): tanımlı mandal bitleri ve (n_cmds ≤ 8 ise) komutlar REJECTED / NOT_INIT. */
void ida_kom_kurulmamis(const ida_in_t *in, ida_out_t *out);

#endif /* IDA_CORE_KOMUT_H */
