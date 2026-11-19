/*
 * W0-MP3a ölçüm prototipi: kartın test mantığı (donanımdan bağımsız; atılabilir).
 * Aynı kod kartta (fw/platform.c altında) ve PC birim testlerindeki kart öykünücüsünde koşar.
 *
 * Durumlar:
 *   KONTROL  denetim hızı (MP3_KONTROL_BAUD). SORGU -> KIMLIK; KUR -> KUR_YANIT; kabulde gönderim boşalınca
 *            yeni hıza geçilir, ESLEME'ye girilir.
 *   ESLEME   yeni hızda ESLE beklenir (esle_ms). Gelmezse denetim hızına dönülür (esle_zaman_asimi++).
 *            ESLE gelince koşu sayaçları sıfırlanır (hız geçişindeki çöp sayılmaz), ESLE_YANIT gönderilir,
 *            koşu başlar.
 *   KOSU     akış: VERI_PK doğrulanır; kart kendi VERI_KP çerçevelerini sabit aralıkla gönderir.
 *            echo: ECHO_ISTEK doğrulanır, aynı sıra ve yükle ECHO_YANIT gönderilir.
 *            BITIR gelince sayaçlar dondurulur, OZET gönderilir. bos_ms boyunca çerçeve gelmezse koşu kesilir.
 *   BITTI    BITIR yinelenirse aynı OZET yeniden gönderilir; DONUS ya da 3 s sessizlikte denetim hızına dönülür.
 */
#ifndef MP3_KART_H
#define MP3_KART_H

#include "mp3_cerceve.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Kesme/sürücü katmanının tuttuğu sayaçlar (yalnız platform bilir). */
typedef struct {
    uint32_t uart_ore, uart_fe, uart_ne, uart_pe;
    uint32_t dma_rx_hata, dma_tx_hata;
    uint32_t rx_tasma, rx_tasma_bayt, rx_halka_azami, rx_halka_boy;
} mp3_hw_t;

typedef struct {
    uint32_t (*zaman_us)(void);                          /* serbest koşan 32 bit µs sayacı */
    uint32_t (*tx_bos)(void);                            /* gönderim halkasındaki boş bayt */
    void (*tx_yaz)(const uint8_t *p, uint32_t n);        /* çağıran yer olduğunu denetler */
    int (*tx_bosaldi)(void);                             /* kuyruk boş ve son bayt hattan çıktı */
    int (*baud_hesapla)(uint32_t baud, mp3_baud_t *b);   /* 0: destekli; uygulamaz */
    void (*baud_uygula)(const mp3_baud_t *b);
    void (*hw_oku)(mp3_hw_t *h, int sifirla);            /* sifirla: okuduktan sonra sıfırla (atomik) */
    void (*kimlik_ek)(char *s, uint32_t kap);            /* KIMLIK metnine eklenecek platform alanları */
} mp3_platform_t;

enum mp3_durum { MP3_D_KONTROL = 0, MP3_D_ESLEME = 1, MP3_D_KOSU = 2, MP3_D_BITTI = 3 };

void mp3_kart_baslat(const mp3_platform_t *p, const char *kimlik);
void mp3_kart_besle(const uint8_t *p, uint32_t n);   /* alınan baytlar */
void mp3_kart_adim(void);                            /* ana döngüden sık çağrılır */
int mp3_kart_durum(void);

#ifdef __cplusplus
}
#endif
#endif
