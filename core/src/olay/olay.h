/*
 * olay.h — tek seferlik olayların kimliği, çıkışı, bekleyen kümesi ve sonuçları
 * (W2; A4-K60–K66, K69, K70, K123, K125, K129).
 *
 * Bu modül yalnız SAFETY_CLOSE üretir (daraltmanın istekleri). SAFETY_OPEN ve FIRE adaylarını
 * atış kontrol işi (W4) kendi dosyasından `aday[]` dizisine koyar; çıkış kapısı
 * (ida_olay_yaz) bu adayları yalnız SİLEBİLİR: tick, IMU_REJECTED, hata dönüşü, daraltma,
 * IMU_LAG, kısıtlı tüp, bekleyen sonuç, olay kimliği yokluğu, yuva ve kapasite (A4-K45, K62,
 * K64, K108, K116, K123, K125, K129) koşullarında aday çıkmaz ve sayaç tüketilmez.
 */
#ifndef IDA_CORE_OLAY_H
#define IDA_CORE_OLAY_H

#include <stdbool.h>
#include <stdint.h>

#include "ida/port.h"

#define IDA_OLAY_BEKLEYEN_MAX 8u /* A4-K129: A1-K80'in olay tablosuyla aynı           */
#define IDA_OLAY_IZIN_MAX     7u /* izin açan olaylar ve FIRE için (1 yuva SAFETY_CLOSE'a) */

typedef struct {
    bool dolu;
    uint8_t kind;
    uint8_t tube;
    uint64_t event_id;
    ida_wtime_t cikis_now;  /* olayı çıkaran çağrının kullanılan now_w'si (A4-K70)  */
    uint32_t apply_by_ns;
} ida_olay_bekleyen_t;

typedef struct {
    bool boot_gecerli;      /* A4-K61                                             */
    bool tukendi;           /* ctr 0xFFFFFFFF kullanıldı (A4-K62)                 */
    uint32_t boot_count;
    uint32_t ctr;           /* sıradaki sayaç değeri; 1'den başlar                */
    uint8_t kapat;          /* bekleyen SAFETY_CLOSE istekleri (IDA_KAPAT_*); (b2) dahil */
    uint8_t n_aday;
    ida_event_t aday[IDA_PORT_MAX_EVENTS]; /* W4'ün genişletici adayları: kind, tube, hold_ns */
    ida_olay_bekleyen_t bekleyen[IDA_OLAY_BEKLEYEN_MAX];
    uint64_t son_id;        /* fc.last_event_id                                  */
    uint8_t son_sonuc;      /* fc.last_event_result                              */
} ida_olay_t;

/* Çıkış kapısının bağlamı. */
typedef struct {
    bool genis_serbest;     /* yalnız IDA_RC_OK dönen step (A4-K64)              */
    bool daraldi;           /* A4-K45                                            */
    bool imu_lag;           /* A4-K108                                           */
    bool fire_serbest;      /* armed = 1 ve is_safe = 0 (A4-K64)                 */
    uint8_t kisitli_tupler; /* bit t: tüp t'nin etkin emniyet değeri kısıtlayan (A4-K116) */
    uint32_t seq;           /* out.seq (A4-K57)                                  */
    ida_stime_t t_ref;      /* out.t_ref                                         */
    ida_wtime_t now;        /* kullanılan now_w                                  */
    uint32_t apply_by_ns;   /* FC_APPLY_BY_NS                                    */
    uint32_t *widen_blocked;/* status.cnt.widen_blocked                          */
} ida_olay_kapi_t;

void ida_olay_kur(ida_olay_t *o, uint32_t boot_count, bool boot_count_ok);
/* status.events_ok: olay kimliği var ve sayaç 0xFFFFFFF0'a ulaşmadı (A4-K61, K62). */
bool ida_olay_kullanilabilir(const ida_olay_t *o);
/* SAFETY_CLOSE isteği ekler (IDA_KAPAT_* bitleri); çıkış ilk yazılabilir çağrıda. */
void ida_olay_kapat_iste(ida_olay_t *o, uint8_t kapat);
/* Sonucu beklenen SAFETY_OPEN/FIRE'ların tüpleri: bit t (A4-K123). */
uint8_t ida_olay_bekleyen_tupler(const ida_olay_t *o);
/* Bekleyen SAFETY_OPEN/FIRE sayısı (A4-K129). */
uint32_t ida_olay_bekleyen_izin_sayisi(const ida_olay_t *o);

/* Sonuç kabulü (A4-K69): event_id bekleyenlerdendir ise bekleyen silinir ve sonucun kaydı
   *bulunan'a yazılır, true döner; değilse false (çağıran evres_ignored sayar). */
bool ida_olay_sonuc(ida_olay_t *o, const ida_event_result_t *r, ida_olay_bekleyen_t *bulunan);
/* Zaman aşımı (A4-K70): now − cikis_now > apply_by + IDA_EVT_RESULT_MARGIN_NS olan ilk
   bekleyeni siler, *bulunan'a yazar ve true döner; yoksa false. */
bool ida_olay_zaman_asimi(ida_olay_t *o, ida_wtime_t now, ida_olay_bekleyen_t *bulunan);

/* events[]'i yazar: önce birleştirilmiş SAFETY_CLOSE'lar (A4-K65, K125), sonra kapıdan geçen
   adaylar; kimlikleri burada ayırır, bekleyenlere ekler, kapat ve adayları temizler. */
void ida_olay_yaz(ida_olay_t *o, const ida_olay_kapi_t *k, ida_out_t *out);

#endif /* IDA_CORE_OLAY_H */
