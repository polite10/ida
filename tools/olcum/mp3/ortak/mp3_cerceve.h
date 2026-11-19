/*
 * W0-MP3a ölçüm prototipi: genel amaçlı seri bağlantı test çerçevesi (atılabilir; ürün protokolü DEĞİL).
 * Kart (C17, arm-none-eabi) ve PC (C olarak derlenir, C++20'den çağrılır) aynı kodu kullanır.
 *
 * Çerçeve (bütün çok baytlı alanlar küçük uçlu):
 *   0  u8   0xA5            eşleme
 *   1  u8   0x5A            eşleme
 *   2  u8   sürüm (MP3_SURUM)
 *   3  u8   tür (mp3_tur)
 *   4  u32  koşu kimliği
 *   8  u32  sıra numarası (yön/tür başına 0'dan)
 *   12 u16  veri yükü uzunluğu (0..MP3_AZAMI_YUK)
 *   14 u16  başlık CRC-16/CCITT-FALSE (bayt 0..13)
 *   16 ...  veri yükü
 *   16+n u32 CRC-32/ISO-HDLC (bayt 0..16+n-1)
 * Ek yük 20 bayt; 256 baytlık veri yükünde çerçeve 276 bayt. Başlık CRC'si bozuk uzunluk alanının
 * ayrıştırıcıyı uzun süre bekletmesini önler (bozuk başlık hemen reddedilir, bir bayt kaydırılıp yeniden aranır).
 */
#ifndef MP3_CERCEVE_H
#define MP3_CERCEVE_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MP3_SURUM          1u
#define MP3_ES0            0xA5u
#define MP3_ES1            0x5Au
#define MP3_BASLIK         16u
#define MP3_KUYRUK         4u
#define MP3_EK             (MP3_BASLIK + MP3_KUYRUK)
#define MP3_AZAMI_YUK      256u
#define MP3_AZAMI_CERCEVE  (MP3_EK + MP3_AZAMI_YUK)
#define MP3_KONTROL_BAUD   115200u
#define MP3_PENCERE        1024u   /* sıra izleyicinin tekrar/sırasız ayrımı penceresi */

enum mp3_tur {
    MP3_T_SORGU = 1,       /* PC -> kart: kimlik sorusu (denetim hızında) */
    MP3_T_KIMLIK = 2,      /* kart -> PC: metin "anahtar=değer ..." */
    MP3_T_KUR = 3,         /* PC -> kart: koşu ayarları (mp3_kur_t) */
    MP3_T_KUR_YANIT = 4,   /* kart -> PC: kabul ve baud hesabı (mp3_kur_yanit_t) */
    MP3_T_ESLE = 5,        /* PC -> kart: yeni hızda eşleme (koşu başlar) */
    MP3_T_ESLE_YANIT = 6,  /* kart -> PC */
    MP3_T_BITIR = 7,       /* PC -> kart: koşu sonu, özet iste */
    MP3_T_OZET = 8,        /* kart -> PC: sayaçlar (MP3_OZET_ALANLARI sırasıyla u32 dizisi) */
    MP3_T_DONUS = 9,       /* PC -> kart: denetim hızına dön */
    MP3_T_VERI_PK = 16,    /* PC -> kart akış verisi */
    MP3_T_VERI_KP = 17,    /* kart -> PC akış verisi */
    MP3_T_ECHO_ISTEK = 18, /* PC -> kart */
    MP3_T_ECHO_YANIT = 19  /* kart -> PC: aynı sıra ve aynı veri yükü */
};

enum mp3_mod { MP3_MOD_AKIS = 1, MP3_MOD_ECHO = 2 };

/* KUR veri yükü: 20 bayt, küçük uçlu (baud, mod, 1 boş bayt, yuk, kart_adet, aralik_ns, esle_ms, bos_ms) */
typedef struct {
    uint32_t baud;
    uint8_t mod;
    uint16_t yuk;          /* veri çerçevesi yükü (akış: iki yön; echo: istek = yanıt) */
    uint32_t kart_adet;    /* akış: kartın göndereceği çerçeve sayısı; echo: 0 */
    uint32_t aralik_ns;    /* akış: kart çerçeveleri arası sabit aralık */
    uint16_t esle_ms;      /* kart yeni hızda ESLE'yi en çok bu kadar bekler, sonra denetim hızına döner */
    uint16_t bos_ms;       /* koşu sırasında bu kadar süre çerçeve gelmezse kart koşuyu keser */
} mp3_kur_t;
#define MP3_KUR_BOY 20u

/* KUR_YANIT veri yükü: 28 bayt */
typedef struct {
    uint8_t kabul;         /* 1 kabul, 0 ret */
    uint8_t neden;         /* ret nedeni (mp3_ret) */
    uint32_t baud;         /* istenen */
    uint32_t usart_hz;     /* USART1 çekirdek saati (yazmaçtan) */
    uint32_t brr;
    uint32_t over8;
    uint32_t hesap_baud;   /* usart_hz / USARTDIV (yuvarlanmış) */
    int32_t hata_ppm;      /* (hesap - istenen) / istenen, milyonda */
} mp3_kur_yanit_t;
#define MP3_KUR_YANIT_BOY 28u

enum mp3_ret { MP3_RET_YOK = 0, MP3_RET_AYAR = 1, MP3_RET_BAUD = 2, MP3_RET_DURUM = 3 };

/* Kartın OZET'te gönderdiği sayaçlar; sıra ikili biçimin parçasıdır (PC aynı listeyle çözer). */
#define MP3_OZET_ALANLARI(X) \
    X(kosu) X(mod) X(baud) X(sure_us) \
    X(rx_bayt) X(rx_iyi) X(rx_atilan_bayt) X(rx_baslik_hata) X(rx_surum_hata) X(rx_uzunluk_hata) X(rx_crc_hata) \
    X(rx_kosu_hata) X(rx_tur_hata) X(rx_icerik_hata) X(rx_alinan) X(rx_beklenen) X(rx_eksik) X(rx_tekrar) \
    X(rx_sirasiz) X(rx_eski) X(rx_kontrol) X(rx_kontrol_bayt) \
    X(uart_ore) X(uart_fe) X(uart_ne) X(uart_pe) X(dma_rx_hata) X(dma_tx_hata) X(rx_tasma) X(rx_tasma_bayt) \
    X(rx_halka_azami) X(rx_halka_boy) \
    X(tx_cerceve) X(tx_bayt) X(tx_plan) X(tx_eksik) X(tx_gec_azami_us) X(tx_son_us) X(tx_bekleme) X(echo_dusen)

/* Sayaç kapsamı (kart): koşu penceresi, koşuyu başlatan ESLE çerçevesinden SONRA başlar ve ilk BITIR dahil biter.
   rx_iyi: penceredeki CRC'si geçen bütün çerçeveler (veri + denetim). rx_kontrol: penceredeki denetim çerçeveleri
   (yinelenen ESLE + BITIR). rx_bayt: pencerede ayrıştırıcının tükettiği bütün baytlar (iyi çerçeveler + atılan).
   Hatasız koşuda rx_iyi = rx_alinan + rx_kontrol ve rx_bayt = rx_alinan × (yük + 20) + rx_kontrol_bayt.
   tx_cerceve/tx_bayt yalnız veri çerçeveleridir (VERI_KP ya da ECHO_YANIT). tx_son_us: akışta son veri çerçevesinin
   gönderilmeye karar verildiği an (t0'dan, µs; gecikme hesabıyla aynı damga; çerçeve hemen ardından gönderim
   halkasına konur; hattan çıkış değil). Planı plan_us(k) = ⌊k · aralik_ns / 1000⌋; kart çerçeveyi geçen ≥ plan_us
   olduğunda gönderir. Echoda son yanıtın halkaya konduğu an. */

#define MP3_X_SAY(a) +1
enum { MP3_OZET_N = 0 MP3_OZET_ALANLARI(MP3_X_SAY) };
#undef MP3_X_SAY

typedef struct {
#define MP3_X_ALAN(a) uint32_t a;
    MP3_OZET_ALANLARI(MP3_X_ALAN)
#undef MP3_X_ALAN
} mp3_ozet_t;

/* OZET veri yükü: u32 alan sayısı + MP3_OZET_N × u32 */
#define MP3_OZET_BOY (4u + 4u * (uint32_t)MP3_OZET_N)

/* USART1 baud hesabı (kart ve PC aynı işlevi kullanır; PC kartın KUR_YANIT'ını bununla doğrular). */
typedef struct {
    uint32_t baud, usart_hz, brr, over8, hesap_baud;
    int32_t hata_ppm;
} mp3_baud_t;

/* Adaylar: OVER16'da USARTDIV = ⌊f/b⌋ ve ⌈f/b⌉; OVER8'de ⌊2f/b⌋ ve ⌈2f/b⌉'ye en yakın ÇİFT bölücüler (OVER8'de
   BRR[3] = 0 olmalı ve USARTDIV[0] BRR'ye yazılamaz). Her aday önce BRR'ye kodlanır, etkin bölücü yazılan BRR'den
   geri çözülür (mp3_brr_baud) ve hata bu değerle hesaplanır. Bölücü 16..0xFFFF. En küçük hata seçilir; eşitlikte
   OVER16. PRESC = 1 (bölmesiz). 0: destekli; -1: aday yok. */
int mp3_baud_hesapla(uint32_t usart_hz, uint32_t baud, mp3_baud_t *b);

/* Yazılmış BRR'den etkin baud (ST LL_USART_GetBaudRate ile aynı geri çözüm, PRESC = 1):
   OVER8: D = (BRR & 0xFFF0) | ((BRR & 7) << 1), baud = 2f / D; OVER16: D = BRR, baud = f / D. Yuvarlanmış tam sayı.
   Geçersiz BRR (D < 16, OVER8'de BRR[3] = 1) için 0. */
uint32_t mp3_brr_baud(uint32_t usart_hz, uint32_t brr, uint32_t over8);

typedef struct {
    uint8_t tur;
    uint32_t kosu;
    uint32_t sira;
    uint16_t uzunluk;
    const uint8_t *yuk;
} mp3_cerceve_t;

/* CRC tablosunu kurar; iş parçacıkları başlamadan bir kez çağrılır (tembel kurulumun yarışını önler). */
void mp3_hazirla(void);
uint16_t mp3_crc16(const uint8_t *p, size_t n);
uint32_t mp3_crc32(const uint8_t *p, size_t n);

/* Çerçeveyi `cikti`ya yazar; toplam boyu döndürür (kapasite yetmezse ya da uzunluk aşarsa 0). */
size_t mp3_kodla(uint8_t *cikti, size_t kapasite, uint8_t tur, uint32_t kosu, uint32_t sira,
                 const uint8_t *yuk, uint16_t uzunluk);

/* Belirlenimci veri yükü (koşu, tür, sıra ve konumdan); doğrulama aynı üreteçle. */
void mp3_veri_uret(uint8_t *p, uint16_t n, uint8_t tur, uint32_t kosu, uint32_t sira);
int mp3_veri_dogru(const uint8_t *p, uint16_t n, uint8_t tur, uint32_t kosu, uint32_t sira);

void mp3_le16_yaz(uint8_t *p, uint16_t v);
void mp3_le32_yaz(uint8_t *p, uint32_t v);
uint16_t mp3_le16(const uint8_t *p);
uint32_t mp3_le32(const uint8_t *p);

void mp3_kur_yaz(uint8_t *p, const mp3_kur_t *k);
void mp3_kur_oku(const uint8_t *p, mp3_kur_t *k);
void mp3_kur_yanit_yaz(uint8_t *p, const mp3_kur_yanit_t *k);
void mp3_kur_yanit_oku(const uint8_t *p, mp3_kur_yanit_t *k);
void mp3_ozet_yaz(uint8_t *p, const mp3_ozet_t *o);
/* 0: tamam; -1: uzunluk ya da alan sayısı tutmuyor */
int mp3_ozet_oku(const uint8_t *p, uint16_t n, mp3_ozet_t *o);
/* Alan adı (0..MP3_OZET_N-1) ve değeri (adla sıra aynı) */
const char *mp3_ozet_ad(unsigned i);
uint32_t mp3_ozet_deger(const mp3_ozet_t *o, unsigned i);

/* ---- akış ayrıştırıcı ---- */
typedef struct {
    uint8_t b[MP3_AZAMI_CERCEVE];
    uint32_t n;
    uint32_t iyi;          /* CRC'si geçen çerçeve */
    uint32_t atilan_bayt;  /* eşleme aranırken ya da hatalı çerçevede atılan bayt */
    uint32_t baslik_hata;  /* başlık CRC'si tutmadı */
    uint32_t surum_hata;   /* başlık CRC'si tuttu, sürüm farklı */
    uint32_t uzunluk_hata; /* başlık CRC'si tuttu, uzunluk sınır dışı */
    uint32_t crc_hata;     /* başlık tuttu, CRC-32 tutmadı (bir bayt atılıp yeniden aranır) */
    uint32_t tuketilen;    /* tampondan çıkan bütün baytlar (iyi çerçeveler + atılanlar) */
} mp3_ayr_t;

typedef void (*mp3_cerceve_cb)(void *ctx, const mp3_cerceve_t *c);

void mp3_ayr_sifirla(mp3_ayr_t *a);          /* tampon ve sayaçlar */
void mp3_ayr_sayac_sifirla(mp3_ayr_t *a);    /* yalnız sayaçlar */
void mp3_ayr_besle(mp3_ayr_t *a, const uint8_t *p, size_t n, mp3_cerceve_cb cb, void *ctx);

/* ---- sıra izleyici ---- */
typedef struct {
    uint32_t beklenen;     /* görülen en büyük sıra + 1 */
    uint32_t alinan;       /* tekil alınan */
    uint32_t eksik;        /* beklenen'den küçük olup gelmemiş */
    uint32_t tekrar;
    uint32_t sirasiz;      /* geç gelen (önce eksik sayılmıştı) */
    uint32_t eski;         /* pencerenin gerisinde; tekrar/sırasız ayrılamaz */
    uint32_t bit[MP3_PENCERE / 32u];
} mp3_sira_t;

enum { MP3_SIRA_YENI = 0, MP3_SIRA_BOSLUK = 1, MP3_SIRA_GEC = 2, MP3_SIRA_TEKRAR = 3, MP3_SIRA_ESKI = 4 };

void mp3_sira_sifirla(mp3_sira_t *s);
int mp3_sira_isle(mp3_sira_t *s, uint32_t sira);

#ifdef __cplusplus
}
#endif
#endif
