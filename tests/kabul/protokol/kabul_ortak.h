/*
 * kabul_ortak.h — ida_link tel katmanı bağımsız kabul testlerinin ortak yardımcıları.
 *
 * Bu yardımcılar kitaplığın kodlayıcısını ve ayrıştırıcısını kullanmaz; yalnız A1 §3.2'deki
 * tanımdan (MAVLink 2 başlığı, yük kırpma, CRC-16/MCRF4XX, CRC_EXTRA) yazılmış küçük bir
 * referanstır. CRC_EXTRA, en büyük yük boyu ve yön sözleşmeden (Ek B başlık satırları,
 * Tablo 3.3) derleme sırasında çıkarılır (ek_b_cikar.py -> kabul_ek_b.h).
 *
 * Kaynak: docs/sozlesmeler/A1-kart-baglantisi.md §3.1–§3.3, §3.11, §4.1, A1-K101, Ek B;
 * açık başlık protocol/ida_link/include/ida_link/tel.h (yalnız çağrı biçimi).
 */
#ifndef KABUL_ORTAK_H
#define KABUL_ORTAK_H

#include <stddef.h>
#include <stdint.h>

#include "ida_link/tel.h"
#include "kabul_ek_b.h"

/* ---- Sözde rastgele (sabit tohum; saatten tohum alınmaz) ---------------------------------- */
typedef struct {
    uint64_t s;
} kabul_rng_t;

void     kabul_rng_kur(kabul_rng_t *r, uint64_t tohum);
uint32_t kabul_rng_u32(kabul_rng_t *r);
uint32_t kabul_rng_aralik(kabul_rng_t *r, uint32_t n); /* [0, n); n > 0 */
uint8_t  kabul_rng_bayt(kabul_rng_t *r);
uint8_t  kabul_rng_temiz_bayt(kabul_rng_t *r);         /* 0xFD ve 0xFE dışında */

/* ---- CRC-16/MCRF4XX (A1 §3.2) ------------------------------------------------------------- */
uint16_t kabul_crc_bayt(uint16_t crc, uint8_t b);
uint16_t kabul_crc(const uint8_t *p, size_t n, uint16_t crc);

/* ---- Sözleşme tablosu --------------------------------------------------------------------- */
const kabul_mesaj_t *kabul_mesaj_bul(uint32_t msgid);
/* Rolün alabileceği yön (A1 Tablo 3.3 ve A1-K06): kart sim -> kart'ı, sim kart -> sim'i alır. */
uint8_t kabul_rol_alir_yon(ida_link_rol_t rol);
uint8_t kabul_rol_gonderir_yon(ida_link_rol_t rol);
/* Karşı tarafın kimliği (A1-K04): kart için 250/25, sim için 1/1. */
uint8_t kabul_karsi_sysid(ida_link_rol_t rol);
uint8_t kabul_karsi_compid(ida_link_rol_t rol);
uint8_t kabul_kendi_sysid(ida_link_rol_t rol);
uint8_t kabul_kendi_compid(ida_link_rol_t rol);

/* ---- Referans çerçeve kurucu --------------------------------------------------------------- */
/* Tam denetimli ham çerçeve: magic 0xFD; len alanı `len`; yükten `len` bayt yazılır (yuk_boyu
   küçükse eksik kısım sıfır). CRC, magic hariç başlık + yük + crc_extra üzerinden.
   Dönüş: yazılan bayt sayısı (12 + len). */
size_t kabul_cerceve_ham(uint8_t *cikis, uint8_t len, uint8_t incompat, uint8_t compat,
                         uint8_t seq, uint8_t sysid, uint8_t compid, uint32_t msgid,
                         const uint8_t *yuk, size_t yuk_boyu, uint8_t crc_extra);

/* Sözleşmeye uygun çerçeve: msgid tablodan (CRC_EXTRA Ek B'den); yük en büyük yük boyuna
   sıfırla tamamlanır; kirp != 0 ise sondaki sıfırlar ilk bayt korunarak kırpılır (A1-K05),
   değilse len = en büyük yük boyu. Dönüş: çerçeve boyu; msgid tabloda yoksa 0. */
size_t kabul_cerceve_kur(uint8_t *cikis, uint32_t msgid, uint8_t seq, uint8_t sysid,
                         uint8_t compid, const uint8_t *yuk, size_t yuk_boyu, int kirp);

/* "Temiz" çerçeve: magic dışındaki hiçbir baytı 0xFD ya da 0xFE değildir (yük rastgele seçilir,
   CRC baytları da temiz çıkana kadar yeniden seçilir). Başlıkta reddedilen ya da sağlaması
   tutmayan bir adaydan sonra taramanın nereden sürdüğü (A1'de açık değil) bu çerçevelerde
   sonucu değiştirmez: içlerinde yeni aday başlatan bayt yoktur.
   kabul_temiz_ham: len, bayraklar, kimlik ve crc_extra serbest; yükün ilk (len - sifir_kuyruk)
   baytı sıfırdan farklı rastgele, kalanı sıfır. Başarısızsa Unity hatası. */
size_t kabul_temiz_ham(uint8_t *cikis, kabul_rng_t *r, uint8_t len, uint8_t incompat,
                       uint8_t compat, uint8_t seq, uint8_t sysid, uint8_t compid, uint32_t msgid,
                       uint8_t crc_extra, size_t sifir_kuyruk);
/* kabul_temiz_kur: sözleşmeye uygun geçerli çerçeve; yükün ilk `dolu` baytı sıfırdan farklı
   rastgele, kalan (en büyük yüke kadar) sıfır; kirp != 0 ise len = max(1, dolu), değilse
   len = en büyük yük (kırpılmamış biçim). `yuk_cikis` NULL değilse en büyük yüke kadar sıfırla
   tamamlanmış yük yazılır. */
size_t kabul_temiz_kur(uint8_t *cikis, kabul_rng_t *r, uint32_t msgid, uint8_t seq,
                       uint8_t sysid, uint8_t compid, size_t dolu, int kirp, uint8_t *yuk_cikis);

/* Gönderici yardımcısı: mesaj yapısı için hizalı tampon. Tel yükü, yapının ilk yuk_azami
   baytıdır (açık başlık); yapının kalan (dolgu) baytları 0xA5 ile doldurulur: tele çıkmamalıdır. */
typedef union {
    long double hiza_ld;
    uint64_t    hiza_u64;
    void       *hiza_p;
    uint8_t     b[512];
} kabul_yapi_t;
void kabul_yapi_hazirla(kabul_yapi_t *y, const kabul_mesaj_t *m, const uint8_t *yuk);

/* Kırpılmış yük boyu (A1-K05): son sıfır olmayan baytın sırası + 1, en az 1. */
size_t kabul_kirpilmis_boy(const uint8_t *yuk, size_t n);

/* Referans doğrulayıcı (gönderici testleri; kitaplığın alıcısı kullanılmaz). */
typedef struct {
    uint32_t msgid;
    uint8_t  len, incompat, compat, seq, sysid, compid;
    uint8_t  yuk[IDA_LINK_YUK_AZAMI]; /* en büyük yüke kadar sıfırla tamamlanmış */
} kabul_cozulen_t;
/* 0: p[0, n) tam olarak tek bir geçerli A1 çerçevesidir (magic, len 1..yuk_azami,
   n = 12 + len, CRC tutar, msgid tabloda). Aksi halde negatif bir neden kodu. */
int kabul_cerceve_dogrula(const uint8_t *p, size_t n, kabul_cozulen_t *c);

/* ---- Besleme ------------------------------------------------------------------------------ */
#define KABUL_TESLIM_AZAMI 1024u

typedef struct {
    uint32_t msgid;
    uint8_t  seq, sysid, compid, len, yuk_azami;
    uint8_t  yuk[IDA_LINK_YUK_AZAMI];
} kabul_teslim_t;

typedef struct {
    size_t              sayi;
    kabul_teslim_t      t[KABUL_TESLIM_AZAMI];
    ida_link_sayaclar_t sayac;
} kabul_sonuc_t;

/* Parçalama biçimi. */
typedef enum {
    KABUL_PARCA_TUMU = 0,     /* tek çağrı */
    KABUL_PARCA_SABIT,        /* her çağrıda `k` bayt */
    KABUL_PARCA_RASTGELE,     /* 0..k bayt arası (0 dahil: sıfır uzunluklu çağrı) */
    KABUL_PARCA_IKI           /* `k`. bayttan ikiye bölünür */
} kabul_parca_turu_t;

typedef struct {
    kabul_parca_turu_t tur;
    size_t             k;
    kabul_rng_t        rng;   /* KABUL_PARCA_RASTGELE için */
} kabul_parcalayici_t;

/* Parçalayıcı kurar (sabit olmayan toplu ilkleyici yerine; MSVC C4204). */
kabul_parcalayici_t kabul_parcalayici(kabul_parca_turu_t tur, size_t k, uint64_t tohum);

/* Akışı parçalar halinde ida_link_al'a verir; her teslimi `out`'a ekler (out->sayi sıfırlanmaz,
   çağıran sıfırlar). Her çağrıda dönüş kodunu, *tuketilen'in sınırlarını (BEKLE'de = n,
   CERCEVE'de 1..n) ve teslim edilen yükün len sonrasının sıfır olduğunu Unity ile denetler.
   Bitince out->sayac = a->sayac. */
void kabul_besle(ida_link_alici_t *a, const uint8_t *s, size_t n, kabul_parcalayici_t *p,
                 kabul_sonuc_t *out);

/* Yeni kurulmuş bir alıcıyla tek seferde besler. */
void kabul_besle_yeni(ida_link_rol_t rol, const uint8_t *s, size_t n, kabul_sonuc_t *out);

void kabul_sonuc_sifirla(kabul_sonuc_t *o);
/* İki sonucun teslimleri ve sayaçları aynı mı (Unity iddiası; baglam iletide yazılır). */
void kabul_sonuc_esit(const kabul_sonuc_t *beklenen, const kabul_sonuc_t *gercek,
                      const char *baglam);
void kabul_sayac_esit(const ida_link_sayaclar_t *beklenen, const ida_link_sayaclar_t *gercek,
                      const char *baglam);
/* Ret sayaçlarının (rx_crc_err, rx_unknown_msgid, rx_iflag, rx_bad_len, rx_wrong_sys,
   rx_mav1, rx_overflow) toplamı. */
uint32_t kabul_ret_toplami(const ida_link_sayaclar_t *s);

/* ---- Referans ayrıştırıcı modeli (sözleşme okumalarıyla) -------------------------------- */
/* A1 metninin açık bıraktığı iki nokta parametredir (raporda "sözleşme belirsiz"):
   bas_okuma: başlıkta reddedilen aday (incompat_flags, bilinmeyen msgid, len) sonrası
     0 = yalnız 0xFD tüketilir, tarama sonraki bayttan sürer;
     1 = adayın len'e göre bütün baytları (12 + len) atlanır.
   mav1_okuma: 0xFE görülünce
     0 = yalnız 0xFE tüketilir (sayılır), geri kalan baytlar taranır;
     1 = MAVLink 1 çerçevesinin bütünü (8 + len) atlanır, bir kez sayılır.
   Sağlaması tutan ama kimlik (K04) ya da yön (K06) yüzünden reddedilen çerçeve bütünüyle
   tüketilir; CRC hatasında aday bütünüyle tüketilir (A1-K07). Bilinmeyen kimlik
   rx_unknown_msgid'e sayılır (A1-K06). */
typedef struct {
    int bas_okuma;
    int mav1_okuma;
} kabul_okuma_t;

typedef struct {
    kabul_sonuc_t sonuc;          /* teslimler ve sayaçlar; rx_seq_gap olay sayısıdır */
    uint32_t      seq_gap_kayip;  /* aynı akışta kayıp çerçeve sayısı okuması */
} kabul_model_t;

void kabul_model_ayristir(ida_link_rol_t rol, kabul_okuma_t okuma, const uint8_t *s, size_t n,
                          kabul_model_t *m);

/* ---- Sözde rastgele akış üreteçleri (sabit tohum) -------------------------------------- */
typedef enum {
    KABUL_AKIS_DUZ_RASTGELE = 0, /* tekdüze rastgele baytlar (0xFD, 0xFE dahil)                */
    KABUL_AKIS_FD_YOGUN,         /* %12 0xFD, %4 0xFE, kalanı rastgele: başlık yollarını zorlar */
    KABUL_AKIS_GOMULU_RASTGELE,  /* tekdüze rastgele baytların arasına gömülü geçerli çerçeveler */
    KABUL_AKIS_TEMIZ_BOZUK,      /* temiz geçerli çerçeveler, temiz ara baytlar; bir kısmının
                                    yükü/CRC'si bozuk ya da sonu kesik (başlık bozulmaz)        */
    KABUL_AKIS_KARISIK,          /* her tür: geçerli, sağlaması bozuk, kesik, incompat, kötü
                                    len, bilinmeyen kimlik, yanlış kimlik, yanlış yön,
                                    MAVLink 1, rastgele ara baytlar                            */
    KABUL_AKIS_TUR_SAYISI
} kabul_akis_turu_t;

/* Akışa gömülü, bozulmamış geçerli çerçeve. */
typedef struct {
    size_t   bas;
    size_t   boy;
    uint32_t msgid;
    uint8_t  seq;
} kabul_gomulu_t;

#define KABUL_GOMULU_AZAMI 512u

/* `kapasite`ye sığan en uzun akışı (en çok `hedef_boy` civarı) üretir; bozulmamış geçerli
   çerçeveleri `gomulu`ya yazar (NULL olabilir). Dönüş: akış boyu. */
size_t kabul_akis_uret(kabul_akis_turu_t tur, ida_link_rol_t rol, uint64_t tohum, uint8_t *s,
                       size_t kapasite, size_t hedef_boy, kabul_gomulu_t *gomulu, size_t *gomulu_sayi);
extern const char *const KABUL_AKIS_AD[KABUL_AKIS_TUR_SAYISI];

/* ---- Karşılaştırma (üretilmiş Ek B denetimleri kullanır) -------------------------------- */
void kabul_esit_u64(const char *ad, uint64_t beklenen, uint64_t gercek);
void kabul_esit_i64(const char *ad, int64_t beklenen, int64_t gercek);
void kabul_esit_f32(const char *ad, float beklenen, float gercek);

/* Biçimli bilgi iletisi (Unity TEST_MESSAGE ile). */
void kabul_bilgi(const char *bicim, ...);

#endif /* KABUL_ORTAK_H */
