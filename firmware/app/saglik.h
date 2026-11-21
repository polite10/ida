/*
 * saglik.h — ana döngünün donanımdan bağımsız kararları (W3-kart dilim 1; 01 §2.8, A4-K18):
 * 200 Hz ızgaradan 10 ms çekirdek tick takvimi, alım ilerleme gözetimi, IWDG besleme kararı ve
 * durum LED'leri. ISO C17; PC'de birim testiyle sınanır (firmware/test/test_saglik.c).
 */
#ifndef IDA_FW_SAGLIK_H
#define IDA_FW_SAGLIK_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ---- ızgara ---------------------------------------------------------------------------------
   200 Hz ızgara sayacı g (kesme artırır, 2^32'de sarar). Çekirdek tick'i 10 ms'lik mutlak
   ızgarada, yani g'nin çift değerlerindedir; 2^32 çift olduğu için çiftlik sarmada korunur. */
typedef struct {
    uint32_t gorulen; /* son işlenen g */
} ida_izgara_t;

typedef struct {
    uint32_t kacan_200hz; /* bu adımda kaçırılan 5 ms anı        */
    uint32_t tick;        /* bu adımda geçen 10 ms anı (0 ya da >= 1) */
    uint32_t kacan_10ms;  /* tick > 1 ise tick − 1               */
} ida_izgara_adim_t;

void ida_izgara_kur(ida_izgara_t *z, uint32_t g);
/* g yeni sayaç değeri. Dönüş: g ilerlediyse 1 (*adim dolu), değilse 0. */
int ida_izgara_ilerle(ida_izgara_t *z, uint32_t g, ida_izgara_adim_t *adim);

/* ---- alım ilerleme gözetimi (01 §2.8 koşul 5) -----------------------------------------------
   Bekleyen alım varken tüketim (teslim edilen ya da atılan bayt) S süresinden uzun ilerlemezse
   koşul tutmaz. Bekleme süresi, beklemenin başladığı ya da son tüketimin olduğu andan sayılır. */
typedef struct {
    uint8_t bekliyordu;
    uint64_t son_ilerleme_us;
} ida_rx_gozetim_t;

void ida_rx_gozetim_kur(ida_rx_gozetim_t *g, uint64_t simdi_us);
/* Her döngü turunda: bekleyen = sürücüde işlenmeyi bekleyen olay ya da kayıp var; ilerledi =
   bu turda bayt teslim edildi ya da atıldı. */
void ida_rx_gozetim_guncelle(ida_rx_gozetim_t *g, bool bekleyen, bool ilerledi,
                             uint64_t simdi_us);
bool ida_rx_gozetim_tamam(const ida_rx_gozetim_t *g, bool bekleyen, uint64_t simdi_us,
                          uint64_t s_us);

/* ---- IWDG besleme kararı -------------------------------------------------------------------- */
#define IDA_SAGLIK_CORE_OK     0x01u /* A4-K89: core_ok = 0                        */
#define IDA_SAGLIK_CAGRI       0x02u /* koşul 2/3: çekirdek çağrısı ilerlemiyor     */
#define IDA_SAGLIK_RX          0x04u /* koşul 5: bekleyen alım tüketilmiyor         */
#define IDA_SAGLIK_ALIM_DURDU  0x08u /* DMA alım kanalı durdu, yeniden başlatılamadı */

typedef struct {
    bool cekirdek_normal; /* platform NORMAL'de (çekirdek çağrılıyor) */
    bool core_ok;
    uint64_t son_cagri_us;
    bool rx_tamam;
    bool alim_durdu;
    uint64_t simdi_us;
    uint64_t s_us;        /* ilerleme koşulu süresi (20 ms) */
} ida_saglik_girdi_t;

/* Tutmayan koşulların bitleri; 0 ise IWDG beslenir. */
uint32_t ida_saglik_degerlendir(const ida_saglik_girdi_t *g);

/* ---- durum LED'leri -------------------------------------------------------------------------- */
typedef struct {
    bool yesil;   /* çalışıyor (1 Hz yanıp söner) */
    bool kirmizi; /* saat, bağlantı ya da çekirdek kurulamadı */
} ida_led_t;

ida_led_t ida_led_durumu(bool saat_tamam, bool baglanti, bool cekirdek_normal);

#ifdef __cplusplus
}
#endif

#endif /* IDA_FW_SAGLIK_H */
