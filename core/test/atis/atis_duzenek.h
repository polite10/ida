/*
 * atis_duzenek.h — atış kontrol emniyet zinciri testlerinin ortak düzeneği (test kodu).
 *
 * İki yol vardır:
 *  - Port yolu (ida_core_step/tick): çekirdeğin bütün çerçevesi. Bu sürümde kestirim düzeyi
 *    port üzerinden OK olamaz (kestirim/kestirim.c: yatay hız ve konum geçerliliği sonraki
 *    dilimdedir), bu yüzden A4 §3.17'nin FC_ARM satırı (EST → NOT_ARMABLE) atış kontrolü port
 *    yolunda hiçbir zaman silahlandırmaz. Silahlandırma gerektirmeyen maddeler bu yolla sınanır.
 *  - Düzenek yolu (dz_cagri): gerçek ida_core_t ve gerçek modül işlevleri (emniyet daraltması,
 *    örnek kapısı, olay modülü ve çıkış kapısı, komut dağıtıcısı, atış kontrol kancaları)
 *    cerceve/cerceve.c ve cerceve/cikti.c'deki sırayla çağrılır; yalnız araç silahlandırması,
 *    kip, kestirim düzeyi, arıza düzeyi, IMU bayatlığı/gecikmesi ve yer istasyonu kalp atışı
 *    testin verdiği koşullardır (kestirim, görev, güdüm, kontrol koşturulmaz). Silahlı yollar
 *    (FC_ARM, FC_FIRE, SAFETY_OPEN, FIRE) bu yolla sınanır.
 */
#ifndef IDA_TEST_ATIS_DUZENEK_H
#define IDA_TEST_ATIS_DUZENEK_H

#include <stdbool.h>
#include <stdint.h>

#include "cekirdek.h"
#include "ida/port.h"

#define DZ_MS 1000000LL

/* Testin verdiği çağrı koşulları (düzenek yolu). */
typedef struct {
    uint8_t est;          /* IDA_EST_*                                          */
    uint8_t fs;           /* IDA_FS_*                                           */
    bool imu_bayat;
    bool imu_lag;
    bool imu_red;         /* step'in IMU'su reddedildi (IMU_REJECTED dönüşü)     */
    int64_t gcs_son_ns;   /* son kabul edilen HEARTBEAT'in now_w'si              */
} dz_kosul_t;

/* Reddedilen çağrı türü (düzenek yolu): 0 kabul. */
#define DZ_KABUL 0
#define DZ_RET_BAD_INPUT 1
#define DZ_RET_DUP_SEQ 2

/* Düzeneğin bir çağrısı: cerceve.c'deki (b), (b3), (b2), (e) ve cikti.c sırası. out NULL
   ise (b2). Dönüş kodu kabulde OK ya da IMU_REJECTED. */
ida_rc_t dz_cagri(ida_core_t *c, const ida_in_t *in, ida_out_t *out, bool step, int ret,
                  const dz_kosul_t *k);

/* Silahlı tüp için olağan koşullar: est OK, fs NONE, IMU taze, kalp atışı now. */
void dz_kosul_olagan(dz_kosul_t *k, int64_t now);

/* Girdi kurucuları. */
void dz_girdi(ida_in_t *in, int64_t t_ns, uint32_t seq, bool step);
/* inst = 0 emniyet örneği: estop, tüp bitleri (IDA_TUBE_*). */
ida_sample_t *dz_emniyet(ida_in_t *in, uint8_t inst, uint32_t seq, int64_t t_meas, int64_t t_rx,
                         uint8_t estop, uint8_t t0, uint8_t t1);
/* FC komutu cmds[]'e. */
void dz_fc(ida_in_t *in, uint32_t cmd_id, uint8_t op, uint8_t tube);
/* Olay sonucu evres[]'e. */
void dz_sonuc(ida_in_t *in, uint64_t event_id, uint8_t result, int64_t t_apply);

/* Çıktıda türün sayısı ve ilk olayı (yoksa NULL). */
uint32_t dz_olay_say(const ida_out_t *out, uint8_t kind);
const ida_event_t *dz_olay(const ida_out_t *out, uint8_t kind);
const ida_cmd_result_t *dz_sonuc_al(const ida_out_t *out, uint32_t cmd_id);

/* Tüp bitleri: dolu, fiş takılı, pim çıkık (izin kapalı). */
#define DZ_TUP_HAZIR ((uint8_t)(IDA_TUBE_LOADED | IDA_TUBE_PLUG_IN | IDA_TUBE_PIN_OUT))

#endif /* IDA_TEST_ATIS_DUZENEK_H */
