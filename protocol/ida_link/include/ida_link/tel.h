/*
 * ida_link/tel.h — A1 tel katmanı: MAVLink 2 çerçeveleme, kodlama ve alıcı sayaçları
 *
 * Sözleşme: docs/sozlesmeler/A1-kart-baglantisi.md (1.0 adayı) §3.1, §3.2, §3.3, §3.11,
 * §4.1 (A1-K02, K04–K08), A1-K101; makinece okunur tanım protocol/ida.xml.
 * Görev: docs/gorevler/W2-protokol.md. Bu başlık ida_link modülünün alt katmanıdır;
 * A1 <-> A4 dönüştürücüsü (oturum, parti, alan eşlemesi) bunun üzerine kurulur.
 *
 * Dil: ISO C17 (uzantısız); C++20'den de içerilebilir. Bellek ayırma, işletim sistemi,
 * saat, dosya ve standart giriş/çıkış kullanılmaz. Kitaplıkta değişebilir genel durum
 * yoktur: bütün durum çağıranın verdiği örnek yapısındadır (ida_link_alici_t,
 * ida_link_gonderici_t); farklı örnekler birbirinden bağımsızdır ve aynı süreçte birbirine
 * karışarak kullanılabilir (A1-K101). Tek bir örnek aynı anda iki iş parçacığından
 * kullanılmamalıdır (kilit yoktur).
 *
 * Mesaj yapıları: üretilmiş MAVLink başlığının tipleri (mavlink_ida_<ad>_t; içerme
 * biçimi #include "mavlink/ida/mavlink.h"). Tel yükü, küçük uçlu bir makinede bu
 * yapıların ilk <en büyük yük> baytıdır (MAVLink'in hizalı alan düzeni; alanlar boy
 * sırasındadır). Kitaplık yalnız küçük uçlu hedefler içindir (kart, macOS arm64,
 * Windows x64).
 *
 * Boş gösterici: bu başlıktaki her işlev, gösterici argümanlarından biri NULL ise (aşağıda
 * NULL'a izin verildiği yazılan durum dışında) hiçbir şey yazmadan IDA_LINK_HATA_ARGUMAN
 * döndürür. Kurulmamış (ida_link_*_kur çağrılmamış) örnek önkoşul ihlalidir; örneğin rol
 * alanı geçersizse işlev IDA_LINK_HATA_ROL döndürür, ama rastgele bellek içeriği her
 * durumda yakalanamaz.
 */
#ifndef IDA_LINK_TEL_H
#define IDA_LINK_TEL_H

#include <stddef.h>
#include <stdint.h>

/* Derleme sırasında protocol/ida.xml'den üretilir: IDA_LINK_DIALECT_HASH ve kanonik metnin
   satır sayıları (A1 §3.2). */
#include "ida_link/dialect.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------------------------------------------------ */
/* Sabitler (A1 §3.1, §3.2)                                           */
/* ------------------------------------------------------------------ */

#define IDA_LINK_MAGIC          0xFDu  /* MAVLink 2 başlangıç baytı (A1-K02)            */
#define IDA_LINK_MAGIC_V1       0xFEu  /* MAVLink 1 başlangıç baytı; atılır (A1-K02)    */
#define IDA_LINK_BASLIK_BOYU    10u    /* magic'ten msgid'nin son baytına kadar         */
#define IDA_LINK_EK_BOY         12u    /* imzasız çerçevenin yük dışı baytları           */
#define IDA_LINK_YUK_AZAMI      255u   /* MAVLink 2 yük sınırı                           */
#define IDA_LINK_CERCEVE_AZAMI  267u   /* IDA_LINK_EK_BOY + IDA_LINK_YUK_AZAMI           */

#define IDA_LINK_SIM_SYSID      250u   /* A1 §3.1, A1-K04 */
#define IDA_LINK_SIM_COMPID     25u
#define IDA_LINK_KART_SYSID     1u     /* kart ve SIL koşucusu */
#define IDA_LINK_KART_COMPID    1u

/* ------------------------------------------------------------------ */
/* Tipler                                                             */
/* ------------------------------------------------------------------ */

/* Örneğin bu bağlantıdaki rolü (A1-K04). SIL koşucusu IDA_LINK_ROL_KART kullanır
   (A1-K98: aynı sysid/compid). Rol gönderilen sysid/compid'i, kabul edilen karşı taraf
   kimliğini ve mesaj yönünü (A1 Tablo 3.3) belirler. */
typedef enum {
    IDA_LINK_ROL_KART = 1,  /* gönderir 1/1; yalnız 250/25'ten ve sim -> kart yönünü alır */
    IDA_LINK_ROL_SIM  = 2   /* gönderir 250/25; yalnız 1/1'den ve kart -> sim yönünü alır */
} ida_link_rol_t;

/* Mesaj yönü (A1 Tablo 3.3 "Yön" sütunu); bit maskesi. */
typedef enum {
    IDA_LINK_YON_SIM_KART = 1,
    IDA_LINK_YON_KART_SIM = 2,
    IDA_LINK_YON_IKI      = 3
} ida_link_yon_t;

/* Dönüş kodları. Sıfır ve pozitif değerler başarıdır, negatifler hatadır. */
typedef enum {
    IDA_LINK_TAMAM         = 0,   /* işlem yapıldı                                         */
    IDA_LINK_CERCEVE       = 1,   /* ida_link_al: bir çerçeve kabul edildi ve teslim edildi */
    IDA_LINK_BEKLE         = 2,   /* ida_link_al: verilen baytların hepsi tüketildi; çerçeve yok */
    IDA_LINK_HATA_ARGUMAN  = -1,  /* NULL gösterici (izin verilen durum dışında)            */
    IDA_LINK_HATA_ROL      = -2,  /* geçersiz rol ya da kurulmamış örnek                    */
    IDA_LINK_HATA_KIMLIK   = -3,  /* mesaj kimliği dialect'te yok ya da beklenenle aynı değil */
    IDA_LINK_HATA_YON      = -4,  /* bu rol bu mesajı gönderemez (A1 Tablo 3.3)             */
    IDA_LINK_HATA_BOY      = -5,  /* mesaj yapısının boyu bu kimliğin yapısının boyu değil  */
    IDA_LINK_HATA_TAMPON   = -6   /* çıkış tamponu çerçeveye yetmiyor                       */
} ida_link_rc_t;

/* Alıcı sayaçları. Adlar ve anlamlar A1 Tablo 3.11a/3.11b'dekilerdir; her biri 32 bittir
   ve 2^32'de sarar (A1 §3.11: kart sayaçları). Sim tarafı 64 bitlik birikimi ardışık
   okumaların farkıyla kurar ((uint32_t)(yeni - eski) her zaman doğru artıştır, iki okuma
   arasında 2^32'den az artış olduğu sürece). Çağıran okur; sıfırlamak serbesttir
   (kitaplık yalnız artırır). */
typedef struct {
    uint32_t rx_bytes;          /* ida_link_al'a verilip tüketilen her bayt                     */
    uint32_t rx_frames;         /* kabul edilip teslim edilen çerçeve                           */
    uint32_t rx_crc_err;        /* sağlaması tutmayan aday çerçeve (A1-K07)                     */
    uint32_t rx_unknown_msgid;  /* dialect'te olmayan ya da bu rolün yönünde olmayan kimlik (A1-K06) */
    uint32_t rx_iflag;          /* incompat_flags != 0 (A1-K02)                                 */
    uint32_t rx_bad_len;        /* len = 0 ya da mesajın en büyük yükünden büyük (A1-K05)       */
    uint32_t rx_wrong_sys;      /* sysid/compid karşı tarafınki değil (A1-K04)                  */
    uint32_t rx_mav1;           /* boşta görülen MAVLink 1 başlangıç baytı 0xFE (A1-K02)        */
    uint32_t rx_overflow;       /* ida_link_alici_tasma ile bildirilen alım taşması (A1-K09)    */
    uint32_t rx_resync_bytes;   /* hiçbir aday çerçeveye ait olmayan bayt (A1-K07)              */
    uint32_t rx_seq_gap;        /* ardışık iki kabul edilen çerçeve arasındaki seq boşluğu (A1-K08) */
} ida_link_sayaclar_t;

/* Teslim edilen çerçeve. */
typedef struct {
    uint32_t msgid;      /* dialect'teki mesaj kimliği (46000–46099)                      */
    uint8_t  seq;        /* başlık seq                                                     */
    uint8_t  sysid;
    uint8_t  compid;
    uint8_t  len;        /* teldeki yük boyu (kırpılmış olabilir); 1 <= len <= yuk_azami    */
    uint8_t  yuk_azami;  /* mesajın en büyük yük boyu (A1 Tablo 3.3)                        */
    /* yuk[0, len) telden gelen baytlardır; yuk[len, IDA_LINK_YUK_AZAMI) sıfırdır. Böylece
       kırpılmış ve kırpılmamış çerçeve aynı yükü verir (A1-K05). */
    uint8_t  yuk[IDA_LINK_YUK_AZAMI];
} ida_link_cerceve_t;

/* Alıcı örneği. Yalnız `sayac` alanı açıktır; ozel_ ile başlayan alanlar kitaplığındır,
   çağıran yazmaz. Boyu derleyiciye göre değişebilir (alanlar sabit genişlikli). */
typedef struct {
    ida_link_sayaclar_t sayac;
    uint8_t  ozel_rol;
    uint8_t  ozel_durum;
    uint8_t  ozel_son_seq;
    uint8_t  ozel_seq_var;
    uint16_t ozel_dolu;
    uint16_t ozel_hedef;
    uint16_t ozel_satir;
    uint8_t  ozel_tampon[IDA_LINK_CERCEVE_AZAMI];
} ida_link_alici_t;

/* Gönderici örneği. */
typedef struct {
    /* Bir sonraki çerçevenin başlık seq değeri (A1-K08: her çerçevede bir artar, mod 256,
       oturumdan bağımsız). Kurulumda 0'dır; çağıran okuyabilir ve yazabilir (ör. test ya da
       belirlenimci yeniden üretim). */
    uint8_t seq;
    uint8_t ozel_rol;
} ida_link_gonderici_t;

/* Bir mesajın dialect bilgisi. */
typedef struct {
    uint32_t msgid;
    uint8_t  crc_extra;   /* A1 §3.2                                                  */
    uint8_t  yuk_azami;   /* en büyük (kırpılmamış) yük boyu, B                        */
    uint8_t  yon;         /* ida_link_yon_t                                           */
    uint16_t yapi_boyu;   /* sizeof(mavlink_ida_<ad>_t); ida_link_gonder ve ida_link_coz'un
                             beklediği mesaj_boyu                                      */
} ida_link_mesaj_bilgisi_t;

/* ------------------------------------------------------------------ */
/* Alıcı (A1-K02, K04–K08)                                            */
/* ------------------------------------------------------------------ */

/*
 * ida_link_alici_kur — alıcı örneğini kurar.
 *
 * Önkoşul: a NULL değil. rol IDA_LINK_ROL_KART ya da IDA_LINK_ROL_SIM.
 * Dönüş: IDA_LINK_TAMAM; bütün sayaçlar 0, ayrıştırıcı boşta, seq geçmişi yok.
 *        Geçersiz rolde IDA_LINK_HATA_ROL: örnek sıfırlanır ve rol geçersiz kalır (sonraki
 *        her çağrı IDA_LINK_HATA_ROL döndürür). a NULL ise IDA_LINK_HATA_ARGUMAN.
 */
ida_link_rc_t ida_link_alici_kur(ida_link_alici_t *a, ida_link_rol_t rol);

/*
 * ida_link_al — baytları parça parça işler; en fazla bir çerçeve teslim eder.
 *
 * veri[0, n) sırayla tüketilir. Bir çerçeve kabul edildiği anda durulur: *cerceve doldurulur,
 * *tuketilen = o çerçevenin son baytı dahil tüketilen bayt sayısı yazılır ve
 * IDA_LINK_CERCEVE döner; çağıran kalan baytlarla (veri + *tuketilen) yeniden çağırır.
 * Çerçeve kabul edilmeden bütün baytlar tüketilirse *tuketilen = n ve IDA_LINK_BEKLE döner;
 * yarım kalan aday çerçeve örnekte saklanır, sonraki çağrıda sürer. n = 0 geçerlidir
 * (veri NULL olabilir): *tuketilen = 0, IDA_LINK_BEKLE.
 * *cerceve yalnız IDA_LINK_CERCEVE dönüşünde yazılır. Hata dönüşlerinde (ARGUMAN, ROL)
 * hiçbir şey yazılmaz ve bayt tüketilmez. veri ile *cerceve ve *a örtüşmemelidir.
 *
 * Ayrıştırma kuralları (her ret bir sayacı tam bir artırır; tüketilen her bayt rx_bytes'ı):
 *  1. Boşta: 0xFD aday çerçeve başlatır. 0xFE rx_mav1'i artırır ve yalnız o bayt tüketilir
 *     (MAVLink 1 çerçevesinin geri kalanı doğrulanamaz, rx_resync_bytes'a sayılarak taranır;
 *     A1-K02). Başka her bayt rx_resync_bytes'ı artırır (A1-K07).
 *  2. Başlık (ilk 10 bayt) tamamlanınca sırayla: incompat_flags != 0 -> rx_iflag (A1-K02);
 *     msgid dialect'te yok -> rx_unknown_msgid (A1-K06); len = 0 ya da len > mesajın en
 *     büyük yükü -> rx_bad_len (A1-K05). compat_flags yok sayılır. Bu üç retten sonra aday
 *     yalnız 0xFD baytıyla tüketilmiş sayılır ve sonraki 9 bayt baştan (kural 1'den) yeniden
 *     taranır; yani başlığı reddedilen adayın içindeki ya da hemen ardındaki geçerli çerçeve
 *     kaçırılmaz. Bu baytlar yeniden taramada aday başlatmazsa rx_resync_bytes'a sayılır.
 *  3. Aday çerçeve 10 + len + 2 bayta tamamlanınca CRC-16/MCRF4XX (CRC_EXTRA ile) denetlenir;
 *     tutmazsa rx_crc_err artar ve adayın bütün baytları tüketilmiş olur: ayrıştırma adayın
 *     son baytından sonraki bayttan sürer (A1-K07). Sağlaması tutan çerçevede sırayla:
 *     sysid/compid karşı tarafınki değil -> rx_wrong_sys (A1-K04); mesaj bu rolün alacağı
 *     yönde değil -> rx_unknown_msgid (A1-K06). Bu iki retten sonra da çerçevenin bütün
 *     baytları tüketilmiş olur. Yani sağlaması tutmayan çerçeve, başka alanı ne olursa
 *     olsun, rx_crc_err'e sayılır.
 *  4. Kabul: rx_frames artar; bir önceki kabul edilen çerçeve varsa ve seq != önceki + 1
 *     (mod 256) ise rx_seq_gap bir artar (boşluk olayı sayılır, kayıp çerçeve sayısı
 *     değil; davranış değişmez; A1-K08). İlk kabul edilen çerçevede boşluk sayılmaz.
 *     Reddedilen çerçeveler seq geçmişini değiştirmez.
 *
 * Bayt sınıflaması (sınamaya yardımcı değişmez): verilen her bayt tam olarak şunlardan
 * birindedir — kabul edilen çerçeve; sağlaması tutmayan ya da kural 3'te reddedilen aday;
 * kural 2'de reddedilen adayın 0xFD'si; rx_mav1'e sayılan 0xFE; rx_resync_bytes'a sayılan
 * bayt; henüz tamamlanmamış adayda bekleyen bayt.
 *
 * Dönüş: IDA_LINK_CERCEVE, IDA_LINK_BEKLE; a, tuketilen ya da cerceve NULL ise ya da
 * (veri NULL ve n > 0) ise IDA_LINK_HATA_ARGUMAN; örnek kurulmamışsa IDA_LINK_HATA_ROL.
 */
ida_link_rc_t ida_link_al(ida_link_alici_t *a, const uint8_t *veri, size_t n,
                          size_t *tuketilen, ida_link_cerceve_t *cerceve);

/*
 * ida_link_alici_tasma — taşımanın alım tamponu taştığını bildirir (A1-K09).
 *
 * rx_overflow bir artar; yarım kalan aday çerçeve atılır (baytları hiçbir sayaca eklenmez)
 * ve ayrıştırıcı boşta durumuna döner (yeniden eşleme). seq geçmişi korunur. Taşmada atılan
 * baytları taşıma ida_link_al'a vermez; rx_bytes yalnız verilen baytları sayar.
 * Dönüş: IDA_LINK_TAMAM; a NULL ise IDA_LINK_HATA_ARGUMAN; kurulmamışsa IDA_LINK_HATA_ROL.
 */
ida_link_rc_t ida_link_alici_tasma(ida_link_alici_t *a);

/* ------------------------------------------------------------------ */
/* Gönderici (A1-K02, K04, K05, K08)                                  */
/* ------------------------------------------------------------------ */

/*
 * ida_link_gonderici_kur — gönderici örneğini kurar; seq = 0.
 * Dönüş: IDA_LINK_TAMAM; geçersiz rolde IDA_LINK_HATA_ROL (örnek kullanılamaz kalır);
 * g NULL ise IDA_LINK_HATA_ARGUMAN.
 */
ida_link_rc_t ida_link_gonderici_kur(ida_link_gonderici_t *g, ida_link_rol_t rol);

/*
 * ida_link_gonder — tipli mesaj yapısından çağıranın tamponuna tam bir çerçeve yazar.
 *
 * mesaj: msgid'nin üretilmiş yapısını (mavlink_ida_<ad>_t) gösterir; mesaj_boyu
 * sizeof(o yapı) olmalıdır (ida_link_mesaj_bilgisi_t.yapi_boyu). Yazılan çerçeve:
 * magic 0xFD, incompat_flags = compat_flags = 0 (A1-K02), seq = g->seq (A1-K08),
 * role göre sysid/compid (A1-K04), yükün sonundaki sıfır baytlar ilk bayt korunarak
 * kırpılır (A1-K05), CRC-16/MCRF4XX + CRC_EXTRA. Gereken boy 12 + kırpılmış yük boyudur
 * (en çok IDA_LINK_EK_BOY + yuk_azami; her durumda <= IDA_LINK_CERCEVE_AZAMI).
 *
 * Başarıda: cikis[0, *yazilan) çerçevedir, g->seq bir artar (mod 256), IDA_LINK_TAMAM.
 * Hatada cikis'e hiçbir bayt yazılmaz, g->seq değişmez, *yazilan = 0 yazılır (yazilan NULL
 * değilse) ve şu döner: msgid dialect'te yok -> IDA_LINK_HATA_KIMLIK; bu rol bu mesajı
 * göndermez (A1 Tablo 3.3) -> IDA_LINK_HATA_YON; mesaj_boyu yanlış -> IDA_LINK_HATA_BOY;
 * kapasite < gereken boy -> IDA_LINK_HATA_TAMPON; g, mesaj, cikis ya da yazilan NULL ->
 * IDA_LINK_HATA_ARGUMAN (yazilan NULL ise hiçbir şey yazılmaz); kurulmamış örnek ->
 * IDA_LINK_HATA_ROL. mesaj ile cikis örtüşmemelidir. Taşımaya yazım, kuyruk ve öncelik
 * bu katmanın işi değildir (A1-K10, K110).
 */
ida_link_rc_t ida_link_gonder(ida_link_gonderici_t *g, uint32_t msgid,
                              const void *mesaj, size_t mesaj_boyu,
                              uint8_t *cikis, size_t kapasite, size_t *yazilan);

/* ------------------------------------------------------------------ */
/* Çözme ve dialect bilgisi                                           */
/* ------------------------------------------------------------------ */

/*
 * ida_link_coz — teslim edilen çerçevenin yükünü tipli mesaj yapısına kopyalar.
 *
 * Önkoşul: cerceve->msgid == msgid. mesaj, msgid'nin üretilmiş yapısını gösterir ve
 * mesaj_boyu sizeof(o yapı) olmalıdır. Başarıda yapı sıfırlanır, ardından yükün ilk
 * yuk_azami baytı kopyalanır (kırpılmış alanlar sıfır olur; A1-K05) ve IDA_LINK_TAMAM döner.
 * Hatada mesaj'a hiçbir şey yazılmaz: msgid dialect'te yok ya da cerceve->msgid'den farklı
 * -> IDA_LINK_HATA_KIMLIK; mesaj_boyu yanlış -> IDA_LINK_HATA_BOY; NULL ->
 * IDA_LINK_HATA_ARGUMAN. Alan değerlerinin anlamı denetlenmez (dönüştürücünün işidir).
 */
ida_link_rc_t ida_link_coz(const ida_link_cerceve_t *cerceve, uint32_t msgid,
                           void *mesaj, size_t mesaj_boyu);

/*
 * ida_link_mesaj_bilgisi — msgid'nin dialect bilgisini yazar.
 * Dönüş: IDA_LINK_TAMAM; dialect'te yoksa IDA_LINK_HATA_KIMLIK (*b yazılmaz); b NULL ise
 * IDA_LINK_HATA_ARGUMAN.
 */
ida_link_rc_t ida_link_mesaj_bilgisi(uint32_t msgid, ida_link_mesaj_bilgisi_t *b);

#ifdef __cplusplus
}
#endif

#endif /* IDA_LINK_TEL_H */
