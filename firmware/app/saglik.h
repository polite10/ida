/*
 * saglik.h — ana döngünün donanımdan bağımsız kararları (W3-kart dilim 1; 01 §2.8, A4-K18):
 * 200 Hz ızgaradan 10 ms çekirdek tick takvimi, alım ilerleme gözetimi, IWDG besleme kararı ve
 * durum LED'leri. ISO C17; PC'de birim testiyle sınanır (firmware/test/test_saglik.c).
 */
#ifndef IDA_FW_SAGLIK_H
#define IDA_FW_SAGLIK_H

#include <stdbool.h>
#include <stddef.h>
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

/* ---- flash işlem gözetimi (01 §2.8 koşul 7; sol genel bulgu 5) -----------------------------
   Her donanım işleminin (sayfa silme, quad-word yazımı) başlangıcı ve bitişi izlenir; süren bir
   işlem sınırı aşarsa sağlık koşulu tutmaz (IWDG beslenmez, kart sıfırlanır; kayıt yarım kalır
   ve eski blok geçerli kalır). Sınır: kartta ölçülen en uzun işlemler (üst sınır ölçümleri,
   W3-kart dilim 2: silme ≤ 1,74 ms, quad-word ≤ 0,49 ms; yoklama aralığı dahil) üzerine geniş
   pay. Veri sayfasının (DS13543) en büyük işlem süreleri bu çalışmada doğrulanmadı. */
#define IDA_FLASH_ISLEM_SINIR_US 20000u

typedef struct {
    uint8_t var;          /* işlem sürüyor */
    uint64_t bas_us;
    uint32_t en_uzun_us;  /* tanı: biten en uzun işlem */
} ida_islem_gozetim_t;

void ida_islem_gozetim_kur(ida_islem_gozetim_t *g);
void ida_islem_basladi(ida_islem_gozetim_t *g, uint64_t simdi_us);
void ida_islem_bitti(ida_islem_gozetim_t *g, uint64_t simdi_us);
/* İşlem yoksa ya da süresi sınırı aşmadıysa true. */
bool ida_islem_tamam(const ida_islem_gozetim_t *g, uint64_t simdi_us, uint64_t sinir_us);

/* ---- kararlı çalışma (01 §2.8: ardışık arıza sayacının temizlenmesi; sol genel bulgu 8) -----
   Koşul (NORMAL ve oturum RUNNING) kesintisiz sinir_us sürünce bir kez true döner; koşul
   bozulunca süre baştan sayılır. */
typedef struct {
    uint8_t suruyor;
    uint8_t bildirildi;
    uint64_t bas_us;
} ida_kararli_t;

void ida_kararli_kur(ida_kararli_t *k);
bool ida_kararli_guncelle(ida_kararli_t *k, bool kosul, uint64_t simdi_us, uint64_t sinir_us);

/* ---- alım turu (01 §2.4; sol genel bulgu 7) --------------------------------------------------
   Bir turda en çok b_ayr bayt: her okumaya kalan kapasite verilir; oku() 0 dönünce tur biter.
   Dönüş: bu turda işlenen bayt. */
typedef size_t (*ida_tur_oku_f)(void *ctx, uint8_t *hedef, size_t azami);
typedef void (*ida_tur_isle_f)(void *ctx, const uint8_t *b, size_t n);
size_t ida_alim_turu(uint8_t *tampon, size_t b_ayr, ida_tur_oku_f oku, ida_tur_isle_f isle,
                     void *ctx);

/* ---- IWDG besleme kararı -------------------------------------------------------------------- */
#define IDA_SAGLIK_CORE_OK     0x01u /* A4-K89: core_ok = 0                        */
#define IDA_SAGLIK_CAGRI       0x02u /* koşul 2/3: çekirdek çağrısı ilerlemiyor     */
#define IDA_SAGLIK_RX          0x04u /* koşul 5: bekleyen alım tüketilmiyor         */
#define IDA_SAGLIK_ALIM_DURDU  0x08u /* DMA alım kanalı durdu, yeniden başlatılamadı */
#define IDA_SAGLIK_FLASH       0x10u /* koşul 7: süren flash işlemi sınırı aştı      */

typedef struct {
    bool cekirdek_normal; /* platform NORMAL'de (çekirdek çağrılıyor) */
    bool core_ok;
    uint64_t son_cagri_us;
    bool rx_tamam;
    bool alim_durdu;
    uint64_t simdi_us;
    uint64_t s_us;        /* ilerleme koşulu süresi (20 ms) */
    bool flash_asim;      /* süren flash işlemi IDA_FLASH_ISLEM_SINIR_US'yi aştı */
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
