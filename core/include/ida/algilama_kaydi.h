/*
 * algilama_kaydi.h — algılama kaydı ve parti başlığı (A8 sözleşmesi, sürüm 1.0-aday)
 *
 * Sözleşme: docs/sozlesmeler/A8-kamera-algilama.md
 * Bu dosya sözleşmenin parçasıdır; belgeyle çelişemez. Yalnız tip ve sabit
 * tanımları içerir (ISO C17; C++20 ile de derlenir; işlev gövdesi yok).
 * Ortak çekirdek (kart ve SIL) ile PC tarafı (sim sunucusu, kamera algılama
 * bileşeni) aynı tanımı kullanır.
 *
 * Yapı düzeni bir taşıma biçimi değildir: A1 ve A8 üzerindeki bayt düzeni
 * sözleşmenin tablolarında tanımlıdır (A8 §3.5, A1). Aşağıdaki boyut ve konum
 * denetimleri yalnız alanların dolgusuz ve beklenen sırada durduğunu sınar.
 *
 * Birimler iskelet §3.1'e uyar: zaman tamsayı nanosaniye, açı radyan,
 * uzunluk metre.
 */
#ifndef IDA_ALGILAMA_KAYDI_H
#define IDA_ALGILAMA_KAYDI_H

#include <stddef.h>
#include <stdint.h>

/* ---- Şema sürümü (A8-K39) ------------------------------------------------ */

#define IDA_DET_SCHEMA_MAJOR 1u
#define IDA_DET_SCHEMA_MINOR 0u

/* ---- Sınırlar (A8 §3.4) -------------------------------------------------- */

/* Partide en çok kayıt (A8-K18). Mimari seçimi (tahmin): A1 tepe bütçesi. */
#define IDA_DET_MAX_PER_BATCH 4u

/* Mesafe üst sınırı, m (A8 §3.2). Mimari seçimi (tahmin). */
#define IDA_DET_RANGE_MAX_M 1000.0f

/* window_ns ve t_sigma_ns üst sınırı, ns (A8 §3.2). Mimari seçimi. */
#define IDA_DET_WINDOW_MAX_NS 1000000000u
#define IDA_DET_TSIGMA_MAX_NS 1000000000u

/* Açı sınırı: binary32'de pi'ye en yakın değer (3.14159274...).
 * az ve el denetimleri bu değerle kapalı aralıkta yapılır (A8 §3.2). */
#define IDA_DET_PI_F32 3.14159265358979323846f
#define IDA_DET_HALF_PI_F32 1.57079632679489661923f

/* Sonar kaydının zamanının parti zamanına göre izinli aralığı (A8-K13):
 * batch.t_meas_ns - BACK <= rec.t_meas_ns <= batch.t_meas_ns + FWD.
 * Mimari seçimi (tahmin); FWD, IDA_DET_RANGE_MAX_M / 1400 m/s ≈ 0,71 s'yi
 * kapsar (hesap). */
#define IDA_DET_SONAR_REC_BACK_MAX_NS INT64_C(30000000000)
#define IDA_DET_SONAR_REC_FWD_MAX_NS INT64_C(1000000000)

/* Kamera kaynağı için kare hızı ve parti hızı üst sınırı, Hz; çözünürlük
 * üst sınırı, piksel (A8-K26, K68; iskelet §10 madde 25). */
#define IDA_DET_CAM_RATE_MAX_HZ 30u
#define IDA_DET_CAM_WIDTH_MAX 640u
#define IDA_DET_CAM_HEIGHT_MAX 480u

/* Kodlanmış yük üst sınırı (ayrılmış baytlar hariç), bayt (A8-K26). */
#define IDA_DET_RECORD_ENCODED_MAX 78u
#define IDA_DET_BATCH_ENCODED_MAX 27u

/* ---- Kaynak sensör: source (A8 §3.4) ------------------------------------ */

#define IDA_DET_SRC_INVALID 0u
#define IDA_DET_SRC_PING360 1u /* Ping360 taramalı sonar (kartta algılama) */
#define IDA_DET_SRC_CAM_FWD 2u /* öne bakan kamera (PC'de algılama, A8) */

/* ---- Ölçüm türü: kind (A8-K03..K05) -------------------------------------- */
/* z[] yerleri sabittir: z[0] = r (m), z[1] = az (rad), z[2] = el (rad). */

#define IDA_DET_KIND_INVALID 0u
#define IDA_DET_KIND_RB2 1u /* mesafe + yön açısı (r, az); el yok */
#define IDA_DET_KIND_B2 2u  /* yön açısı + yükseliş (az, el); mesafe yok */
#define IDA_DET_KIND_RB3 3u /* mesafe + yön açısı + yükseliş (r, az, el) */

#define IDA_DET_Z_R 0u
#define IDA_DET_Z_AZ 1u
#define IDA_DET_Z_EL 2u

/* ---- Belirsizlik: chol[] (A8-K07, K09) ----------------------------------- */
/* chol[] = (r, az, el) ölçüm kovaryansının alt üçgen Cholesky çarpanı L,
 * satır sırasıyla: [L_rr, L_ar, L_aa, L_er, L_ea, L_ee]; kovaryans = L·Lᵀ.
 * L'nin satır i'deki elemanları z[i]'nin biriminde (standart sapma boyutu):
 * r satırı m, az ve el satırları rad. */
#define IDA_DET_L_RR 0u
#define IDA_DET_L_AR 1u
#define IDA_DET_L_AA 2u
#define IDA_DET_L_ER 3u
#define IDA_DET_L_EA 4u
#define IDA_DET_L_EE 5u

/* Ölçülen bileşenin köşegen elemanının alt sınırı ve bütün elemanların
 * mutlak değer üst sınırı (A8-K07). Mimari seçimi (tahmin). */
#define IDA_DET_L_DIAG_MIN_R_M 1.0e-3f
#define IDA_DET_L_DIAG_MIN_ANG_RAD 1.0e-6f
#define IDA_DET_L_ABS_MAX_R_M 1000.0f
#define IDA_DET_L_ABS_MAX_ANG_RAD 10.0f

/* ---- Çerçeve: frame (A8-K01) --------------------------------------------- */

#define IDA_DET_FRAME_INVALID 0u
#define IDA_DET_FRAME_SENSOR 1u /* ilk sürümde tek geçerli değer */

/* ---- Kayıt bayrakları: flags (A8-K21, K22, K05) -------------------------- */

#define IDA_DET_F_VALID 0x0001u            /* her kayıtta 1 olmalı */
#define IDA_DET_F_NEAR_BLIND_EDGE 0x0002u  /* kör bölge sınırında */
#define IDA_DET_F_SATURATED 0x0004u        /* doymuş veri içeriyor */
#define IDA_DET_F_IMAGE_EDGE 0x0008u       /* nesne kare kenarında kesik */
#define IDA_DET_F_RANGE_FROM_PRIOR 0x0010u /* mesafe ölçülmedi, önsel bilgiden */
#define IDA_DET_F_SYNTHETIC 0x0020u        /* yer gerçeğinden üretilmiş; bu sürümde 0 */
#define IDA_DET_F_RESERVED_MASK 0xFFC0u    /* ayrılmış; 0 olmalı */

/* ---- Parti durumu: status (A8-K25, K63) ---------------------------------- */

#define IDA_DET_ST_OK 0u              /* işlendi; count = 0 ise algılama yok */
#define IDA_DET_ST_SKIPPED_BUSY 1u    /* kare bileşene sunulmadı ya da atlandı */
#define IDA_DET_ST_LATE 2u            /* sonuç süresinde gelmedi */
#define IDA_DET_ST_COMPONENT_DOWN 3u  /* bileşen bağlı değil */
#define IDA_DET_ST_FRAME_DROPPED 4u   /* kare üretilmedi ya da düştü (A2) */
#define IDA_DET_ST_FRAME_INVALID 5u   /* kare biçimi ya da bütünlüğü geçersiz */
#define IDA_DET_ST_CALIB_MISMATCH 6u  /* kalibrasyon sürümü uyuşmuyor */
#define IDA_DET_ST_CAMERA_OFF 7u      /* kamera kapalı kademe */
#define IDA_DET_ST_COMPONENT_ERROR 8u /* bileşen hatası ya da geçersiz çıktı */
#define IDA_DET_ST_MAX 8u

/* ---- Parti bayrakları (A8-K64) ------------------------------------------- */

#define IDA_DET_BF_NO_FRAME 0x0001u     /* bir kareye bağlı olmayan durum başlığı */
#define IDA_DET_BF_RESERVED_MASK 0xFFFEu

/* ---- Doğrulama ret nedenleri (A8-K27..K30, §4.6 tablosu) ----------------- */
/* Sıra önceliktir: birden çok hata varsa en küçük numaralı olan bildirilir.
 * Her hatanın sonucu bütün partinin reddidir. */

#define IDA_DET_RJ_NONE 0u
#define IDA_DET_RJ_STRUCT 1u  /* yapı: count, durum, bayrak, ayrılmış alanlar */
#define IDA_DET_RJ_ECHO 2u    /* yalnız sim: bekleyen kareyle ve HELLO ile eşlik */
#define IDA_DET_RJ_CALIB 3u   /* kalibrasyon sürümü */
#define IDA_DET_RJ_LINK 4u    /* kayıt alanlarının parti başlığıyla eşliği */
#define IDA_DET_RJ_SEQ 5u     /* batch_seq ve det_seq artışı */
#define IDA_DET_RJ_TIME 6u    /* parti zamanının ilerlemesi ve kayıt zamanları */
#define IDA_DET_RJ_KIND 7u    /* çerçeve ve ölçüm türü */
#define IDA_DET_RJ_FLAGS 8u   /* kayıt bayrakları */
#define IDA_DET_RJ_VALUE 9u   /* değer aralıkları */
#define IDA_DET_RJ_UNCERT 10u /* Cholesky çarpanı */
#define IDA_DET_RJ_MAX 10u

/* ---- Algılama kaydı (A8 §3.2) -------------------------------------------- */

typedef struct ida_det_record {
    int64_t t_meas_ns;     /* ölçüm zamanı (iskelet §3.1); A8-K10, K12, K13 */
    uint32_t det_seq;      /* kayıt kimliği; akışta kesin artar; A8-K15 */
    uint32_t batch_seq;    /* ait olduğu parti; A8-K19 */
    uint32_t window_ns;    /* ölçüm aralığı süresi; anlıkta 0; A8-K10 */
    uint32_t t_sigma_ns;   /* geçerlilik anının belirsizliği (1 sigma) */
    float z[3];            /* [r, az, el]; ölçülmeyen 0; A8-K03 */
    float chol[6];         /* L, alt üçgen, satır sırasıyla; A8-K07, K09 */
    float confidence;      /* [0, 1]; A8-K08 */
    uint16_t calib_ver;    /* üreticinin kalibrasyon ve montaj sürümü; A8-K20 */
    uint16_t producer_ver; /* üretici algoritma sürümü; A8-K24 */
    uint16_t flags;        /* IDA_DET_F_*; A8-K21 */
    uint8_t source;        /* IDA_DET_SRC_* */
    uint8_t inst;          /* sensör örneği (kamerada camera_id) */
    uint8_t kind;          /* IDA_DET_KIND_* */
    uint8_t frame;         /* IDA_DET_FRAME_*; A8-K01 */
    uint8_t class_id;      /* 0 = bilinmiyor; anlamı A5'te; A8-K23 */
    uint8_t producer_id;   /* 0 geçersiz; A8-K24 */
    uint8_t batch_index;   /* partideki sıra, 0'dan; A8-K18 */
    uint8_t batch_count;   /* partideki kayıt sayısı; A8-K18 */
    uint8_t reserved[2];   /* 0; taşınmaz */
} ida_det_record_t;

/* ---- Parti başlığı (A8 §3.3) --------------------------------------------- */

typedef struct ida_det_batch {
    int64_t t_meas_ns;     /* girdi biriminin ölçüm zamanı; A8-K11, K64 */
    uint32_t batch_seq;    /* akışta kesin artar; NO_FRAME'de 0; A8-K16 */
    uint32_t window_ns;    /* girdi biriminin aralık süresi; A8-K11 */
    uint16_t calib_ver;    /* A8-K20, K62 */
    uint16_t producer_ver; /* bileşen yoksa 0 */
    uint16_t flags;        /* IDA_DET_BF_* */
    uint8_t source;        /* IDA_DET_SRC_* */
    uint8_t inst;          /* sensör örneği */
    uint8_t status;        /* IDA_DET_ST_*; A8-K25 */
    uint8_t count;         /* ardından gelen kayıt sayısı, 0..IDA_DET_MAX_PER_BATCH */
    uint8_t producer_id;   /* bileşen yoksa 0 */
    uint8_t reserved[5];   /* 0; taşınmaz */
} ida_det_batch_t;

/* ---- Düzen denetimleri --------------------------------------------------- */
/* Başlık C17 (çekirdek) ve C++20 (sim sunucusu, bileşen) tarafından okunur. */

#if defined(__cplusplus)
#define IDA_DET_STATIC_ASSERT(c, m) static_assert(c, m)
#else
#define IDA_DET_STATIC_ASSERT(c, m) _Static_assert(c, m)
#endif

IDA_DET_STATIC_ASSERT(sizeof(float) == 4, "f32 4 bayt olmali");
IDA_DET_STATIC_ASSERT(sizeof(ida_det_record_t) == 80, "ida_det_record_t 80 bayt");
IDA_DET_STATIC_ASSERT(offsetof(ida_det_record_t, t_meas_ns) == 0, "konum");
IDA_DET_STATIC_ASSERT(offsetof(ida_det_record_t, det_seq) == 8, "konum");
IDA_DET_STATIC_ASSERT(offsetof(ida_det_record_t, batch_seq) == 12, "konum");
IDA_DET_STATIC_ASSERT(offsetof(ida_det_record_t, window_ns) == 16, "konum");
IDA_DET_STATIC_ASSERT(offsetof(ida_det_record_t, t_sigma_ns) == 20, "konum");
IDA_DET_STATIC_ASSERT(offsetof(ida_det_record_t, z) == 24, "konum");
IDA_DET_STATIC_ASSERT(offsetof(ida_det_record_t, chol) == 36, "konum");
IDA_DET_STATIC_ASSERT(offsetof(ida_det_record_t, confidence) == 60, "konum");
IDA_DET_STATIC_ASSERT(offsetof(ida_det_record_t, calib_ver) == 64, "konum");
IDA_DET_STATIC_ASSERT(offsetof(ida_det_record_t, producer_ver) == 66, "konum");
IDA_DET_STATIC_ASSERT(offsetof(ida_det_record_t, flags) == 68, "konum");
IDA_DET_STATIC_ASSERT(offsetof(ida_det_record_t, source) == 70, "konum");
IDA_DET_STATIC_ASSERT(offsetof(ida_det_record_t, inst) == 71, "konum");
IDA_DET_STATIC_ASSERT(offsetof(ida_det_record_t, kind) == 72, "konum");
IDA_DET_STATIC_ASSERT(offsetof(ida_det_record_t, frame) == 73, "konum");
IDA_DET_STATIC_ASSERT(offsetof(ida_det_record_t, class_id) == 74, "konum");
IDA_DET_STATIC_ASSERT(offsetof(ida_det_record_t, producer_id) == 75, "konum");
IDA_DET_STATIC_ASSERT(offsetof(ida_det_record_t, batch_index) == 76, "konum");
IDA_DET_STATIC_ASSERT(offsetof(ida_det_record_t, batch_count) == 77, "konum");
IDA_DET_STATIC_ASSERT(offsetof(ida_det_record_t, reserved) == 78, "konum");

IDA_DET_STATIC_ASSERT(sizeof(ida_det_batch_t) == 32, "ida_det_batch_t 32 bayt");
IDA_DET_STATIC_ASSERT(offsetof(ida_det_batch_t, t_meas_ns) == 0, "konum");
IDA_DET_STATIC_ASSERT(offsetof(ida_det_batch_t, batch_seq) == 8, "konum");
IDA_DET_STATIC_ASSERT(offsetof(ida_det_batch_t, window_ns) == 12, "konum");
IDA_DET_STATIC_ASSERT(offsetof(ida_det_batch_t, calib_ver) == 16, "konum");
IDA_DET_STATIC_ASSERT(offsetof(ida_det_batch_t, producer_ver) == 18, "konum");
IDA_DET_STATIC_ASSERT(offsetof(ida_det_batch_t, flags) == 20, "konum");
IDA_DET_STATIC_ASSERT(offsetof(ida_det_batch_t, source) == 22, "konum");
IDA_DET_STATIC_ASSERT(offsetof(ida_det_batch_t, inst) == 23, "konum");
IDA_DET_STATIC_ASSERT(offsetof(ida_det_batch_t, status) == 24, "konum");
IDA_DET_STATIC_ASSERT(offsetof(ida_det_batch_t, count) == 25, "konum");
IDA_DET_STATIC_ASSERT(offsetof(ida_det_batch_t, producer_id) == 26, "konum");
IDA_DET_STATIC_ASSERT(offsetof(ida_det_batch_t, reserved) == 27, "konum");

#endif /* IDA_ALGILAMA_KAYDI_H */
