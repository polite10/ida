/*
 * alim.h — dairesel DMA alım halkasının konum muhasebesi ve alım olayları (W3-kart, dilim 1).
 *
 * Donanımdan bağımsızdır (ISO C17; PC'de birim testiyle sınanır: firmware/test/). Kart
 * sürücüsü (link/seri.c) GPDMA kanalının yazmaç görüntüsünü ve duvar saatini verir; bu modül
 * halkaya yazılmış toplam bayt sayısını (açılıştan beri, 64 bit) ve her alım olayının (01 §2.3,
 * A1-K48: 0xFD karakter eşleşmesi, hat boşluğu, DMA yarım/tam) konumunu ve damgasını tutar.
 * Ana döngü baytları olay sınırlarında bloklar hâlinde alır.
 *
 * Damga (A1-K48): bir baytın damgası, o bayt belleğe yazıldıktan sonraki ilk alım olayının
 * duvar saatidir (kesin damga). Baytı izleyen olay kaydedilemediyse (olay halkası dolu) ya da
 * bayt olaysız bulunduysa (ana bağlamın yoklaması), damga bayttan ÖNCEKİ en son olayınkidir ve
 * blok "kaba" işaretlenir; kart A1'in stamp_coarse sayacını bu çerçeveler için artırır. Hiçbir
 * durumda olay anından daha geç bir zaman damga olarak verilmez.
 *
 * Konum (MP3 prototipinin kartta kanıtlanmış yöntemi): görüntü sırayla BNDT (b1), FIFOL (f),
 * BNDT (b2) okumasıdır; b1 != b2 ise kararsızdır ve kullanılmaz. Kararlı görüntüde RAM'e yazılmış
 * konum p = (2n − b − f) mod n'dir. İlerleme d = (p − p_önceki) mod n gerçek ilerlemeye ancak
 * gerçek ilerleme n'den küçükse eşittir. Denetim sırası:
 *   1. Süre: son kabul edilen görüntüden bu yana hatta gelebilecek bayt (geçen süre × azami hız
 *      + 1 + FIFO payı) n'ye ulaşıyorsa tur sayısı BELİRSİZDİR (d ne olursa olsun).
 *   2. Küçük geri gidiş: konum önceki görüntüden en çok min(64, n/8) bayt geride görünüyorsa
 *      ilerleme sayılmaz. Kartta görülen geri gidişlerin hepsi, programlayıcının bağlanırken NVIC
 *      önceliklerini sıfırlayıp seri kesmelerini ana bağlamın maskesinden çıkardığı (yarış)
 *      koşularındaydı (bsp.h NVIC bekçisi); bekçi öncelikleri geri yükledikten sonra 0. Bant,
 *      BNDT ile FIFOL'ün aynı anda güncellenmediği olası ara duruma karşı güvence olarak kalır
 *      (RM0456'dan doğrulanmadı).
 *   3. 3n/4 ve üstü ilerleme gerçek olamaz (DMA yarım/tam kesmeleri her n/2 baytta görüntü
 *      alır): BELİRSİZ.
 * Tur belirsizliği halkadaki BÜTÜN bekleyen veriyi geçersiz kılar (A1-K09): kayıp konumu o anki
 * toplamdır; ana bağlam bir sonraki okumada o konuma kadar her şeyi atar ve TASMA bildirir
 * (çağıran ayrıştırıcıyı yeniden eşler); kuyrukta bekleyen eski olayların blokları teslim
 * edilmez. Kopya doğrulaması da aynı denetimden geçen yeni bir görüntüyle yapılır.
 *
 * Eşzamanlılık: ida_alim_goruntu kesme bağlamından ya da kesmeler maskeliyken ana bağlamdan
 * çağrılır (tek yazıcı). ida_alim_oku, ida_alim_dogrula, ida_alim_yeniden_baslat ve
 * ida_alim_bekleyen ana bağlamdan, alım kesmeleri maskeliyken çağrılır; bu modül kesme
 * maskelemez (çağıranın işidir).
 */
#ifndef IDA_FW_ALIM_H
#define IDA_FW_ALIM_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define IDA_ALIM_OLAY_N 64u /* olay halkası (2'nin kuvveti) */

/* Olay türleri (bit maskesi). YOKLAMA gerçek bir alım olayı değildir: konumu tazeler; yeni
   bayt bulursa onları önceki olayın damgasıyla, kaba olarak kaydeder. */
#define IDA_ALIM_OLAY_ESLESME 0x01u /* USART karakter eşleşmesi (0xFD) */
#define IDA_ALIM_OLAY_BOSTA   0x02u /* hat boşluğu                      */
#define IDA_ALIM_OLAY_YARIM   0x04u /* DMA yarım blok                   */
#define IDA_ALIM_OLAY_TAM     0x08u /* DMA tam blok                     */
#define IDA_ALIM_OLAY_YOKLAMA 0x10u /* ana bağlamın yoklaması           */

/* Olay kaydı: (önceki kaydın konumu, kesin_konum] baytları t_us ile kesin, (kesin_konum, konum]
   baytları t_us ile kaba damgalıdır (t_us onlardan önceki son olayınkidir). */
typedef struct {
    uint64_t konum;
    uint64_t kesin_konum;
    uint64_t t_us;
    uint8_t tur;
} ida_alim_olay_t;

typedef enum {
    IDA_ALIM_VERI = 0,  /* *n > 0 bayt verildi (ida_alim_oku); kopya taahhüt edildi (dogrula) */
    IDA_ALIM_BOS = 1,   /* veri yok ya da kopya bu kez doğrulanamadı (yeniden denenir)       */
    IDA_ALIM_TASMA = 2  /* bekleyen baytlar atıldı (taşma ya da tur belirsizliği); çağıran
                           ayrıştırıcıyı yeniden eşler                                       */
} ida_alim_durum_t;

typedef struct {
    /* yapılandırma */
    uint32_t n;
    uint32_t fifo_azami;
    uint32_t azami_bayt_s;

    /* konum muhasebesi (görüntü yazıcısı) */
    uint32_t p;
    uint64_t toplam;
    uint64_t t_us;        /* son kabul edilen görüntünün zamanı */
    uint64_t son_olay_us; /* son gerçek alım olayının zamanı    */
    uint8_t hazir;

    /* olay halkası: bas görüntü yazıcısında, son ana bağlamda artar */
    volatile uint32_t bas;
    volatile uint32_t son;
    ida_alim_olay_t olay[IDA_ALIM_OLAY_N];

    /* kayıp (tur belirsizliği, üzerine yazma, yeniden başlatma): yazıcı sayacı artırır ve
       konumu kaydeder; ana bağlam gördüğünü ayrıca tutar */
    volatile uint32_t kayip_sayaci;
    uint64_t kayip_konum;
    uint32_t kayip_gorulen;

    /* ana bağlam */
    uint64_t okunan;

    /* sayaçlar */
    uint32_t kararsiz;
    uint32_t belirsiz;
    uint32_t gecersiz;
    uint32_t geri;
    uint32_t olay_birlesme;  /* halka doluyken kaydedilemeyen olay */
    uint32_t olay_sayisi[5]; /* tür başına (eşleşme, boşta, yarım, tam, yoklama) */
    uint32_t tasma;
    uint64_t tasma_bayt;
    uint32_t halka_azami;
    uint32_t yeniden;        /* ida_alim_yeniden_baslat çağrısı */
} ida_alim_t;

/* Kurulum. n 2'nin kuvveti ve >= 64, fifo_azami < n, azami_bayt_s > 0 değilse 0 döner. */
int ida_alim_kur(ida_alim_t *a, uint32_t n, uint32_t fifo_azami, uint32_t azami_bayt_s);

/* Görüntü yazıcısı: bir görüntüyü işler. Dönüş: 1 görüntü kabul edildi (gerçek olaysa olay
   kaydedildi), 0 kararsız ya da geçersiz (hiçbir şey değişmedi). */
int ida_alim_goruntu(ida_alim_t *a, uint32_t b1, uint32_t f, uint32_t b2, uint64_t t_us,
                     uint8_t tur);

/* Ana bağlam: önce bildirilmemiş bir kayıp varsa o konuma kadar atar ve TASMA döner
   (*tasma_bayt). Yoksa en eski olayın kapsadığı baytlardan, aynı damga ve aynı kesinlikteki en
   çok `azami` kadarını hedef'e kopyalar: *n, *t_us, *kaba (1: kaba damga). Kopya, çağıran
   ida_alim_dogrula'yı başarıyla çağırana kadar taahhüt edilmez. */
ida_alim_durum_t ida_alim_oku(ida_alim_t *a, const uint8_t *halka, uint8_t *hedef, size_t azami,
                              size_t *n, uint64_t *t_us, uint8_t *kaba, uint64_t *tasma_bayt);

/* Ana bağlam (kesmeler maskeli): son ida_alim_oku'nun k baytlık kopyasını, kopyadan sonra alınmış
   görüntüyle doğrular. Görüntü kararsızsa BOS (kopya atılır, yeniden denenir). Görüntü tur
   belirsizliği gösterirse ya da kopyanın başından bu yana n'den fazla bayt yazılmışsa kayıp
   kaydedilir, bekleyenler atılır ve TASMA döner. Değilse kopya taahhüt edilir (VERI). */
ida_alim_durum_t ida_alim_dogrula(ida_alim_t *a, size_t k, uint32_t b1, uint32_t f, uint32_t b2,
                                  uint64_t t_us, uint64_t *tasma_bayt);

/* Ana bağlam (DMA kanalı durmuşken, kesmeler maskeli): alım yeniden başlatılacak. Toplam bir
   sonraki n katına yuvarlanır (yeni kanal halkanın başından yazar), bekleyen her şey kayıp
   sayılır; bir sonraki ida_alim_oku TASMA döner. */
void ida_alim_yeniden_baslat(ida_alim_t *a, uint64_t t_us);

/* Ana bağlam: işlenmeyi bekleyen olay ya da bildirilmemiş kayıp var mı. */
int ida_alim_bekleyen(const ida_alim_t *a);

#ifdef __cplusplus
}
#endif

#endif /* IDA_FW_ALIM_H */
