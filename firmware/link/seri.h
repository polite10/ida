/*
 * seri.h — A1 bağlantı sürücüsü: ST-LINK sanal seri portu, USART1 + GPDMA1 (K17; A1-K01, K09,
 * K48; 01 §2.3, §3.2; W3-kart dilim 1).
 *
 * USART1 PA9 TX / PA10 RX (AF7), 3 000 000 baud (BRR'den hesap: 3 018 868), 8N1, FIFO açık,
 * akış denetimi yok. Alım: GPDMA1 kanal 0, 16 KB dairesel halka (tek düğümlü bağlı liste; MP3
 * prototipinin kartta kanıtlanmış düzeni). Alım olayları: USART karakter eşleşmesi (0xFD),
 * hat boşluğu, DMA yarım ve tam blok kesmeleri; her olayda halka konumu ve duvar saati
 * kaydedilir (link/alim.h). Ana döngü baytları olay sınırlarında bloklar hâlinde alır; bir
 * bloğun damgası kendisinden sonraki ilk olayınkidir (A1-K48). Gönderim: GPDMA1 kanal 1,
 * 4 KB halka; yalnız tam çerçeve yazılır (A1-K10).
 *
 * Hata sayaçları: USART taşma (ORE), çerçeve (FE), gürültü (NE), eşlik (PE); DMA aktarım
 * hataları; halka taşması (okunmamış baytların üzerine yazılması ya da belirsiz konum).
 */
#ifndef IDA_FW_SERI_H
#define IDA_FW_SERI_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define IDA_SERI_BAUD 3000000u
#define IDA_SERI_RX_N 16384u
#define IDA_SERI_TX_N 4096u

typedef struct {
    uint32_t baud_istenen;
    uint32_t baud_brr;      /* BRR'den hesaplanan etkin hız              */
    uint32_t usart_hz;      /* USART1 çekirdek saati                     */
    uint32_t brr;
    uint32_t over8;
    uint32_t uart_bekle_hata; /* TEACK/REACK zaman aşımı                 */
    /* USART hataları (kesme) */
    uint32_t ore, fe, ne, pe;
    /* DMA */
    uint32_t dma_rx_hata, dma_tx_hata;
    /* alım halkası (link/alim.h'den kopya) */
    uint32_t kararsiz, belirsiz, gecersiz, geri, olay_birlesme;
    uint32_t olay_eslesme, olay_bosta, olay_yarim, olay_tam;
    uint32_t tasma;
    uint32_t tasma_bayt;
    uint32_t halka_azami;
    /* gönderim */
    uint32_t tx_bayt;
    uint32_t tx_ret;        /* yer olmadığı için kabul edilmeyen çerçeve */
    uint32_t tx_doluluk_azami;
    /* DMA alım hatası sonrası */
    uint32_t rx_yeniden;    /* denetimli yeniden başlatma          */
    uint32_t rx_durdu;      /* 1: alım kanalı durmuş (mandal)       */
} ida_seri_sayac_t;

/* Başlatma (saat 160 MHz'e kurulduktan sonra). 0: başarılı; negatif: başarısız (-3: USART
   hazır olmadı; açılan aktarım geri alındı). Başarısızsa sürücü kullanılmaz. */
int ida_seri_baslat(void);

/* Ana döngü: bir alım bloğu. Dönüş: verilen bayt (0: veri yok). *t_rx_us bloğun damgası;
   *kaba: damga bloktan önceki son olayınki (A1-K48 kaba damga). *tasma: bu çağrıda bayt
   atıldıysa (halka taşması, tur belirsizliği, DMA yeniden başlatması ya da USART taşması);
   çağıran ayrıştırıcıyı yeniden eşler (ida_link_alici_tasma). DMA alım kanalı hata verip
   durduysa önce denetimli olarak yeniden başlatılır. */
size_t ida_seri_oku(uint8_t *hedef, size_t azami, uint64_t *t_rx_us, bool *kaba, bool *tasma);

/* Alım kanalı durmuş ve yeniden başlatılamamış (sağlık: besleme yapılmaz). */
bool ida_seri_alim_durdu(void);

/* Bekleyen alım olayı var mı (A geçişinin "yeni bayt" denetimi). */
bool ida_seri_bekleyen(void);

/* Tam çerçeveyi gönderim halkasına yazar; yer yoksa hiçbir şey yazmaz ve false döner. */
bool ida_seri_yaz(const uint8_t *veri, size_t n);
size_t ida_seri_yazma_yeri(void);

void ida_seri_sayaclar(ida_seri_sayac_t *s);

#ifdef __cplusplus
}
#endif

#endif /* IDA_FW_SERI_H */
