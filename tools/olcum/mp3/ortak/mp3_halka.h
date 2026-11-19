/*
 * W0-MP3a ölçüm prototipi: dairesel DMA alım halkasında konum muhasebesi (donanımdan bağımsız; atılabilir).
 * Kart (fw/platform.c) GPDMA kanal yazmaçlarından görüntü alıp bu işleve verir; PC birim testi aynı işlevi bir DMA
 * modeliyle sınar. (Düzeltme turu 2: kesme sayımına dayalı tur hesabı kaldırıldı; sol Y1.)
 *
 * Görüntü: PRIMASK altında sırayla BNDT (b1), FIFOL (f), BNDT (b2) ve zaman okunur. PRIMASK DMA'yı durdurmaz;
 * b1 != b2 ise okuma sırasında kaynaktan bayt alınmış ya da düğüm yeniden yüklenmiştir: görüntü KARARSIZ, hiçbir
 * şey değişmez, veri tüketilmez. b1 == b2 ise RAM'e yazılmış konum (halka içinde)
 *     p = (n − b − f) mod n
 * dir. Bu ifade düğüm yeniden yüklemesinin öncesinde (b = 0) ve sonrasında (b = n) AYNI değeri verir; TC bayrağı,
 * kesme hizmeti ve LLI yazmaç güncellemesi hangi sırayla olursa olsun konum iki kez sayılmaz.
 * İlerleme d = (p − p_önceki) mod n. Tur sayısı ancak son kabul edilen görüntüden bu yana hatta gelebilecek bayt
 * sayısı yarım halkanın altındaysa bellidir: (geçen_µs × azami_bayt_s / 10⁶ + 1 + fifo_azami) < n/2. Değilse,
 * ya da yazmaç değeri aralık dışıysa (b > n, f > fifo_azami), sonuç BELIRSIZ: toplam d kadar ilerletilir ve referans
 * yenilenir; çağıran bekleyen veriyi atar ve hata sayar (sessiz kayıp yok).
 *
 * Doğrulanmamış donanım varsayımları (RM0456'dan okunmadı; açıkça):
 *  V1  BNDT yalnız kaynaktan okumayla azalır ve düğüm yüklemesiyle n'ye döner; başka anlarda değişmez.
 *  V2  Bir kaynak okuması BNDT azalmasıyla FIFOL artışını aynı anda görünür kılar (biri görünüp öteki
 *      görünmeyen ara durum yok). Bu sağlanmazsa konum en çok bir bayt fazla görünebilir; böyle bir bayt
 *      bayatsa çerçeve CRC'si/içerik denetimi yakalar (sessiz başarı üretmez, koşu geçmez).
 *  V3  Hedef yazımları halka adreslerine sırayla yapılır.
 * FIFOL alanı 8 bittir (stm32u5a9xx.h DMA_CSR_FIFOL_Msk); fifo_azami olarak 255 kullanılır (kanal FIFO
 * derinliğine dayanılmaz).
 */
#ifndef MP3_HALKA_H
#define MP3_HALKA_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint32_t n;            /* halka boyu (çift) */
    uint32_t fifo_azami;   /* FIFOL üst sınırı */
    uint32_t p;            /* son kabul edilen RAM konumu (mod n) */
    uint64_t toplam;       /* açılıştan beri RAM'e yazıldığı kesin bayt */
    uint32_t t_us;         /* son kabul edilen görüntünün zamanı */
    int hazir;
    uint32_t kararsiz;     /* tanı: kararsız görüntü sayısı */
    uint32_t belirsiz;     /* tur sayısı bilinemeyen görüntü sayısı */
} mp3_halka_t;

enum { MP3_HALKA_TAMAM = 0, MP3_HALKA_KARARSIZ = 1, MP3_HALKA_BELIRSIZ = -1 };

void mp3_halka_baslat(mp3_halka_t *h, uint32_t n, uint32_t fifo_azami);
int mp3_halka_guncelle(mp3_halka_t *h, uint32_t b1, uint32_t f, uint32_t b2, uint32_t t_us, uint32_t azami_bayt_s);

#ifdef __cplusplus
}
#endif
#endif
