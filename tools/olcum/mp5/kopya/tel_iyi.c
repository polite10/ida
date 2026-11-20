/*
 * W0-MP5a İYİLEŞTİRME DENEMESİ (atılabilir; ürün kodu değildir). Taban: ürünün güncel kaynağı
 * protocol/ida_link/src/tel.c @ origin/is/w2-protokol 0fa569e. Ürün kitaplığı değiştirilmez; bu
 * dosya yalnız tools/olcum/mp5 düzeneğinde (kart ölçümü ve host testleri) derlenir.
 *
 * Her iyileştirme bir derleme bayrağıdır; hepsi 0 iken kod ürünün davranışını ve yapısını korur
 * (denetim derlemesi). Bayraklar:
 *   MP5_K    1: teslim edilen çerçevede yük kuyruğu yalnız mesajın en büyük yük boyuna
 *              (yuk_azami) kadar sıfırlanır; [yuk_azami, 255) yazılmaz (AÇIK ARAYÜZ DEĞİŞİR:
 *              tel.h'deki ida_link_cerceve_t.yuk açıklaması). ida_link_coz yalnız yuk_azami
 *              baytı okuduğu için çözme sonucu değişmez.
 *   MP5_W    1: memcpy/memset çağrıları yerine sözcük genişliğinde yerel kopya ve sıfırlama
 *              (ISO C17; hizasız erişim sabit boylu memcpy ile, takma ad kuralına uygun).
 *   MP5_B    1: alıcıda başlıktan sonraki baytlar (yük + sağlama) bayt bayt durum makinesinden
 *              geçmez: çerçeve parçada tamamlanmıyorsa eldeki baytlar tampona blok olarak
 *              kopyalanır; tamamlanıyorsa CRC tampondaki kısım ve gelen parça üzerinden tek
 *              geçişte hesaplanır, yük çerçeveye doğrudan kaynaklardan kopyalanır.
 *   MP5_CRC  0: bit işlemli (crc_accumulate); 16: 16 girdili yarım bayt tablosu (32 B);
 *            256: 256 girdili tablo (512 B).
 *   MP5_E    1: kodlama: sondaki sıfırların sözcük sözcük taranması; yük kopyası ve CRC aynı
 *              geçişte.
 *   MP5_H    1: mesaj tablosuna kimlikten doğrudan dizin (100 B; ikili arama yerine);
 *              rx_bytes çağrı başına bir kez toplanır; ida_link_coz yapıyı iki kez yazmaz.
 */
#ifdef MAVLINK_CHECK_MESSAGE_LENGTH
#error "MAVLINK_CHECK_MESSAGE_LENGTH tanimlanmaz (A1 7.3; uzunluk denetimi ida_link'te, A1-K05)"
#endif

#ifndef MP5_K
#define MP5_K 0
#endif
#ifndef MP5_W
#define MP5_W 0
#endif
#ifndef MP5_B
#define MP5_B 0
#endif
#ifndef MP5_CRC
#define MP5_CRC 0
#endif
#ifndef MP5_E
#define MP5_E 0
#endif
#ifndef MP5_H
#define MP5_H 0
#endif

#define MAVLINK_EXTERNAL_RX_STATUS 1
#define MAVLINK_EXTERNAL_RX_BUFFER 1
#include "mavlink/mavlink_types.h"
/* Bilerek tanımsız (ürün kaynağındaki açıklama). */
extern mavlink_status_t m_mavlink_status[MAVLINK_COMM_NUM_BUFFERS];
extern mavlink_message_t m_mavlink_buffer[MAVLINK_COMM_NUM_BUFFERS];
#include "mavlink/ida/mavlink.h"

#include "ida_link/tel.h"

#include <string.h>

#include "ida_link/ozel/tablo_uretilen.h"

#if defined(__BYTE_ORDER__) && defined(__ORDER_LITTLE_ENDIAN__)
#if __BYTE_ORDER__ != __ORDER_LITTLE_ENDIAN__
#error "ida_link yalniz kucuk uclu hedefler icindir"
#endif
#endif

/* ------------------------------------------------------------------ */
/* Mesaj tablosu                                                      */
/* ------------------------------------------------------------------ */

typedef struct {
    uint32_t msgid;
    uint8_t  crc_extra;
    uint8_t  yuk_azami;
    uint8_t  yon;
    uint16_t yapi_boyu;
} tablo_satiri_t;

#define IDA_LINK_SATIR(id, crc, len, tip, yon) \
    {(uint32_t)(id), (uint8_t)(crc), (uint8_t)(len), (uint8_t)(yon), (uint16_t)sizeof(tip)},

static const tablo_satiri_t TABLO[] = {IDA_LINK_TABLO_SATIRLARI(IDA_LINK_SATIR)};

#define IDA_LINK_TABLO_N (sizeof TABLO / sizeof TABLO[0])

#define IDA_LINK_BOY_DENETIMI(id, crc, len, tip, yon)                            \
    _Static_assert(sizeof(tip) >= (len) && sizeof(tip) <= 0xFFFFu && (len) >= 1 && \
                       (len) <= IDA_LINK_YUK_AZAMI,                                 \
                   "yapi boyu: " #tip);
IDA_LINK_TABLO_SATIRLARI(IDA_LINK_BOY_DENETIMI)

#define IDA_LINK_CAPRAZ(kosul, ad) _Static_assert(kosul, "uretec ile pymavlink farkli: " #ad);
IDA_LINK_TABLO_CAPRAZ_DENETIM(IDA_LINK_CAPRAZ)

_Static_assert(IDA_LINK_TABLO_SAYISI == IDA_LINK_DIALECT_MESAJ_SATIRI, "tablo eksik");
_Static_assert(IDA_LINK_DIALECT_YUK_AZAMI <= IDA_LINK_YUK_AZAMI, "yuk siniri");
_Static_assert(IDA_LINK_CERCEVE_AZAMI == IDA_LINK_EK_BOY + IDA_LINK_YUK_AZAMI, "cerceve siniri");

#if MP5_H
/* Kimlikten satıra doğrudan dizin: A1 §3.2'nin 46000–46099 aralığı; 0 = dialect'te yok. */
#define IDA_LINK_KIMLIK_TABAN 46000u
#define IDA_LINK_KIMLIK_ARALIK 100u
#define IDA_LINK_SIRA(id, crc, len, tip, yon) SIRA_##tip,
enum { IDA_LINK_TABLO_SATIRLARI(IDA_LINK_SIRA) SIRA_SAYISI };
#define IDA_LINK_ARALIK_DENETIMI(id, crc, len, tip, yon)                                    \
    _Static_assert((id) >= IDA_LINK_KIMLIK_TABAN &&                                          \
                       (id) < IDA_LINK_KIMLIK_TABAN + IDA_LINK_KIMLIK_ARALIK,                \
                   "kimlik araligi: " #tip);
IDA_LINK_TABLO_SATIRLARI(IDA_LINK_ARALIK_DENETIMI)
#define IDA_LINK_DIZIN(id, crc, len, tip, yon) [(id) - IDA_LINK_KIMLIK_TABAN] = (uint8_t)(SIRA_##tip + 1),
static const uint8_t DIZIN[IDA_LINK_KIMLIK_ARALIK] = {IDA_LINK_TABLO_SATIRLARI(IDA_LINK_DIZIN)};
_Static_assert(SIRA_SAYISI < 255, "dizin bir bayta sigar");

static const tablo_satiri_t *satir_bul(uint32_t msgid)
{
    const uint32_t k = msgid - IDA_LINK_KIMLIK_TABAN;
    if (k >= IDA_LINK_KIMLIK_ARALIK) {
        return NULL;
    }
    const uint8_t s = DIZIN[k];
    return s != 0u ? &TABLO[s - 1u] : NULL;
}
#else
/* Tablo kimliğe göre artan sıradadır (üreteç sıralar); ikili arama. */
static const tablo_satiri_t *satir_bul(uint32_t msgid)
{
    size_t alt = 0;
    size_t ust = IDA_LINK_TABLO_N;
    while (alt < ust) {
        const size_t orta = alt + (ust - alt) / 2u;
        if (TABLO[orta].msgid == msgid) {
            return &TABLO[orta];
        }
        if (TABLO[orta].msgid < msgid) {
            alt = orta + 1u;
        } else {
            ust = orta;
        }
    }
    return NULL;
}
#endif

/* ------------------------------------------------------------------ */
/* Kopya, sıfırlama, CRC                                              */
/* ------------------------------------------------------------------ */

#if MP5_W
/* Sözcük genişliğinde kopya: sabit boylu memcpy derleyicide tek yükleme/saklama komutuna
   iner (hizasız erişim Cortex-M33'te ve host'ta serbesttir; takma ad kuralı korunur). Kuyruk
   döngüsüzdür (derleyicinin döngüyü kitaplık çağrısına geri çevirmemesi için). */
static void kopyala(uint8_t *d, const uint8_t *s, size_t n)
{
    while (n >= 8u) {
        uint32_t w0;
        uint32_t w1;
        memcpy(&w0, s, 4u);
        memcpy(&w1, s + 4, 4u);
        memcpy(d, &w0, 4u);
        memcpy(d + 4, &w1, 4u);
        d += 8;
        s += 8;
        n -= 8u;
    }
    if (n >= 4u) {
        uint32_t w;
        memcpy(&w, s, 4u);
        memcpy(d, &w, 4u);
        d += 4;
        s += 4;
        n -= 4u;
    }
    if (n >= 2u) {
        uint16_t h;
        memcpy(&h, s, 2u);
        memcpy(d, &h, 2u);
        d += 2;
        s += 2;
        n -= 2u;
    }
    if (n != 0u) {
        *d = *s;
    }
}

static void sifirla(uint8_t *d, size_t n)
{
    const uint32_t z = 0u;
    while (n >= 8u) {
        memcpy(d, &z, 4u);
        memcpy(d + 4, &z, 4u);
        d += 8;
        n -= 8u;
    }
    if (n >= 4u) {
        memcpy(d, &z, 4u);
        d += 4;
        n -= 4u;
    }
    if (n >= 2u) {
        const uint16_t h = 0u;
        memcpy(d, &h, 2u);
        d += 2;
        n -= 2u;
    }
    if (n != 0u) {
        *d = 0u;
    }
}
#define KOPYALA(d, s, n) kopyala((d), (s), (n))
#define SIFIRLA(d, n) sifirla((d), (n))
#else
#define KOPYALA(d, s, n) memcpy((d), (s), (n))
#define SIFIRLA(d, n) memset((d), 0, (n))
#endif

#if MP5_CRC == 256
/* CRC-16/MCRF4XX (yansıtılmış 0x8408), bayt tablosu; crc_accumulate ile aynı sonuç. */
static const uint16_t CRC_TABLO[256] = {
    0x0000u, 0x1189u, 0x2312u, 0x329Bu, 0x4624u, 0x57ADu, 0x6536u, 0x74BFu,
    0x8C48u, 0x9DC1u, 0xAF5Au, 0xBED3u, 0xCA6Cu, 0xDBE5u, 0xE97Eu, 0xF8F7u,
    0x1081u, 0x0108u, 0x3393u, 0x221Au, 0x56A5u, 0x472Cu, 0x75B7u, 0x643Eu,
    0x9CC9u, 0x8D40u, 0xBFDBu, 0xAE52u, 0xDAEDu, 0xCB64u, 0xF9FFu, 0xE876u,
    0x2102u, 0x308Bu, 0x0210u, 0x1399u, 0x6726u, 0x76AFu, 0x4434u, 0x55BDu,
    0xAD4Au, 0xBCC3u, 0x8E58u, 0x9FD1u, 0xEB6Eu, 0xFAE7u, 0xC87Cu, 0xD9F5u,
    0x3183u, 0x200Au, 0x1291u, 0x0318u, 0x77A7u, 0x662Eu, 0x54B5u, 0x453Cu,
    0xBDCBu, 0xAC42u, 0x9ED9u, 0x8F50u, 0xFBEFu, 0xEA66u, 0xD8FDu, 0xC974u,
    0x4204u, 0x538Du, 0x6116u, 0x709Fu, 0x0420u, 0x15A9u, 0x2732u, 0x36BBu,
    0xCE4Cu, 0xDFC5u, 0xED5Eu, 0xFCD7u, 0x8868u, 0x99E1u, 0xAB7Au, 0xBAF3u,
    0x5285u, 0x430Cu, 0x7197u, 0x601Eu, 0x14A1u, 0x0528u, 0x37B3u, 0x263Au,
    0xDECDu, 0xCF44u, 0xFDDFu, 0xEC56u, 0x98E9u, 0x8960u, 0xBBFBu, 0xAA72u,
    0x6306u, 0x728Fu, 0x4014u, 0x519Du, 0x2522u, 0x34ABu, 0x0630u, 0x17B9u,
    0xEF4Eu, 0xFEC7u, 0xCC5Cu, 0xDDD5u, 0xA96Au, 0xB8E3u, 0x8A78u, 0x9BF1u,
    0x7387u, 0x620Eu, 0x5095u, 0x411Cu, 0x35A3u, 0x242Au, 0x16B1u, 0x0738u,
    0xFFCFu, 0xEE46u, 0xDCDDu, 0xCD54u, 0xB9EBu, 0xA862u, 0x9AF9u, 0x8B70u,
    0x8408u, 0x9581u, 0xA71Au, 0xB693u, 0xC22Cu, 0xD3A5u, 0xE13Eu, 0xF0B7u,
    0x0840u, 0x19C9u, 0x2B52u, 0x3ADBu, 0x4E64u, 0x5FEDu, 0x6D76u, 0x7CFFu,
    0x9489u, 0x8500u, 0xB79Bu, 0xA612u, 0xD2ADu, 0xC324u, 0xF1BFu, 0xE036u,
    0x18C1u, 0x0948u, 0x3BD3u, 0x2A5Au, 0x5EE5u, 0x4F6Cu, 0x7DF7u, 0x6C7Eu,
    0xA50Au, 0xB483u, 0x8618u, 0x9791u, 0xE32Eu, 0xF2A7u, 0xC03Cu, 0xD1B5u,
    0x2942u, 0x38CBu, 0x0A50u, 0x1BD9u, 0x6F66u, 0x7EEFu, 0x4C74u, 0x5DFDu,
    0xB58Bu, 0xA402u, 0x9699u, 0x8710u, 0xF3AFu, 0xE226u, 0xD0BDu, 0xC134u,
    0x39C3u, 0x284Au, 0x1AD1u, 0x0B58u, 0x7FE7u, 0x6E6Eu, 0x5CF5u, 0x4D7Cu,
    0xC60Cu, 0xD785u, 0xE51Eu, 0xF497u, 0x8028u, 0x91A1u, 0xA33Au, 0xB2B3u,
    0x4A44u, 0x5BCDu, 0x6956u, 0x78DFu, 0x0C60u, 0x1DE9u, 0x2F72u, 0x3EFBu,
    0xD68Du, 0xC704u, 0xF59Fu, 0xE416u, 0x90A9u, 0x8120u, 0xB3BBu, 0xA232u,
    0x5AC5u, 0x4B4Cu, 0x79D7u, 0x685Eu, 0x1CE1u, 0x0D68u, 0x3FF3u, 0x2E7Au,
    0xE70Eu, 0xF687u, 0xC41Cu, 0xD595u, 0xA12Au, 0xB0A3u, 0x8238u, 0x93B1u,
    0x6B46u, 0x7ACFu, 0x4854u, 0x59DDu, 0x2D62u, 0x3CEBu, 0x0E70u, 0x1FF9u,
    0xF78Fu, 0xE606u, 0xD49Du, 0xC514u, 0xB1ABu, 0xA022u, 0x92B9u, 0x8330u,
    0x7BC7u, 0x6A4Eu, 0x58D5u, 0x495Cu, 0x3DE3u, 0x2C6Au, 0x1EF1u, 0x0F78u,
};
static inline uint16_t crc_adim(uint16_t c, uint8_t b)
{
    return (uint16_t)((c >> 8) ^ CRC_TABLO[(uint8_t)(c ^ b)]);
}
#elif MP5_CRC == 16
/* Yarım bayt tablosu: her bayt için iki adım (önce düşük, sonra yüksek dört bit). */
static const uint16_t CRC_YARIM[16] = {
    0x0000u, 0x1081u, 0x2102u, 0x3183u, 0x4204u, 0x5285u, 0x6306u, 0x7387u,
    0x8408u, 0x9489u, 0xA50Au, 0xB58Bu, 0xC60Cu, 0xD68Du, 0xE70Eu, 0xF78Fu,
};
static inline uint16_t crc_adim(uint16_t c, uint8_t b)
{
    c = (uint16_t)((c >> 4) ^ CRC_YARIM[(c ^ b) & 0x0Fu]);
    c = (uint16_t)((c >> 4) ^ CRC_YARIM[(c ^ (uint16_t)(b >> 4)) & 0x0Fu]);
    return c;
}
#elif MP5_CRC == 0
static inline uint16_t crc_adim(uint16_t c, uint8_t b)
{
    crc_accumulate(b, &c);
    return c;
}
#else
#error "MP5_CRC 0, 16 ya da 256"
#endif

static uint16_t crc_blok(uint16_t c, const uint8_t *p, size_t n)
{
    for (size_t i = 0; i < n; ++i) {
        c = crc_adim(c, p[i]);
    }
    return c;
}

static uint16_t crc_bas(void)
{
    uint16_t crc;
    crc_init(&crc);
    return crc;
}

/* CRC_EXTRA ilk argümandır: boy ile yan yana karışabilecek iki tamsayı olmasın. */
static uint16_t crc_hesapla(uint8_t crc_extra, const uint8_t *p, size_t n)
{
    return crc_adim(crc_blok(crc_bas(), p, n), crc_extra);
}

/* ------------------------------------------------------------------ */
/* Ortak                                                              */
/* ------------------------------------------------------------------ */

enum { DURUM_BOSTA = 0, DURUM_ADAY = 1 };

static int rol_gecerli(uint8_t rol)
{
    return rol == (uint8_t)IDA_LINK_ROL_KART || rol == (uint8_t)IDA_LINK_ROL_SIM;
}

static uint8_t alinan_yon(uint8_t rol)
{
    return rol == (uint8_t)IDA_LINK_ROL_KART ? (uint8_t)IDA_LINK_YON_SIM_KART
                                             : (uint8_t)IDA_LINK_YON_KART_SIM;
}

static uint8_t gonderilen_yon(uint8_t rol)
{
    return rol == (uint8_t)IDA_LINK_ROL_KART ? (uint8_t)IDA_LINK_YON_KART_SIM
                                             : (uint8_t)IDA_LINK_YON_SIM_KART;
}

/* ------------------------------------------------------------------ */
/* Alıcı                                                              */
/* ------------------------------------------------------------------ */

ida_link_rc_t ida_link_alici_kur(ida_link_alici_t *a, ida_link_rol_t rol)
{
    if (a == NULL) {
        return IDA_LINK_HATA_ARGUMAN;
    }
    memset(a, 0, sizeof *a);
    if (!rol_gecerli((uint8_t)rol)) {
        return IDA_LINK_HATA_ROL;
    }
    a->ozel_rol = (uint8_t)rol;
    a->ozel_durum = DURUM_BOSTA;
    return IDA_LINK_TAMAM;
}

/* Boşta durumundaki bayt (kural 1). */
static void bosta_bayt(ida_link_alici_t *a, uint8_t b)
{
    if (b == IDA_LINK_MAGIC) {
        a->ozel_durum = DURUM_ADAY;
        a->ozel_tampon[0] = b;
        a->ozel_dolu = 1u;
    } else {
        a->sayac.rx_resync_bytes++;
        if (b == IDA_LINK_MAGIC_V1) {
            a->sayac.rx_mav1++;
        }
    }
}

/* Başlığı reddedilen adayın 0xFD'sinden sonraki baytlarını baştan tarar (kural 2). */
static void yeniden_tara(ida_link_alici_t *a)
{
    uint8_t gecici[IDA_LINK_BASLIK_BOYU - 1u];
    uint16_t n = (uint16_t)(a->ozel_dolu - 1u);
    if (n > sizeof gecici) {
        n = (uint16_t)sizeof gecici;
    }
    memcpy(gecici, &a->ozel_tampon[1], n);
    a->ozel_durum = DURUM_BOSTA;
    a->ozel_dolu = 0u;
    for (uint16_t i = 0; i < n; ++i) {
        if (a->ozel_durum == DURUM_BOSTA) {
            bosta_bayt(a, gecici[i]);
        } else {
            a->ozel_tampon[a->ozel_dolu++] = gecici[i];
        }
    }
}

/* Başlık denetimi (kural 2). 1: geçti (ozel_hedef ve ozel_satir kuruldu), 0: reddedildi. */
static int baslik_denetle(ida_link_alici_t *a)
{
    const uint8_t *t = a->ozel_tampon;
    if (t[2] != 0u) {
        a->sayac.rx_iflag++;
        return 0;
    }
    const uint32_t msgid = (uint32_t)t[7] | ((uint32_t)t[8] << 8) | ((uint32_t)t[9] << 16);
    const tablo_satiri_t *s = satir_bul(msgid);
    if (s == NULL) {
        a->sayac.rx_unknown_msgid++;
        return 0;
    }
    const uint8_t len = t[1];
    if (len == 0u || len > s->yuk_azami) {
        a->sayac.rx_bad_len++;
        return 0;
    }
    a->ozel_hedef = (uint16_t)(IDA_LINK_EK_BOY + len);
    a->ozel_satir = (uint16_t)(s - TABLO);
    return 1;
}

/* Kural 3 ve 4'ün CRC sonrası kısmı ve teslim. Çerçevenin baytları: tampon[0, dolu) ve ardından
   p[0, k) (dolu + k = 12 + len). hesaplanan: [1, 10 + len) baytlarının CRC'si + CRC_EXTRA.
   1: kabul edildi ve *c dolduruldu. */
static int cerceve_teslim(ida_link_alici_t *a, ida_link_cerceve_t *c, const tablo_satiri_t *s,
                          size_t dolu, const uint8_t *p, uint16_t hesaplanan)
{
    const uint8_t *t = a->ozel_tampon;
    const uint8_t len = t[1];
    const size_t govde = IDA_LINK_BASLIK_BOYU + (size_t)len;
    const uint8_t g0 = govde < dolu ? t[govde] : p[govde - dolu];
    const uint8_t g1 = govde + 1u < dolu ? t[govde + 1u] : p[govde + 1u - dolu];
    const uint16_t gelen = (uint16_t)((uint16_t)g0 | (uint16_t)((uint16_t)g1 << 8));
    if (hesaplanan != gelen) {
        a->sayac.rx_crc_err++;
        return 0;
    }
    const int kart = a->ozel_rol == (uint8_t)IDA_LINK_ROL_KART;
    const uint8_t bek_sys = kart ? (uint8_t)IDA_LINK_SIM_SYSID : (uint8_t)IDA_LINK_KART_SYSID;
    const uint8_t bek_comp = kart ? (uint8_t)IDA_LINK_SIM_COMPID : (uint8_t)IDA_LINK_KART_COMPID;
    if (t[5] != bek_sys || t[6] != bek_comp) {
        a->sayac.rx_wrong_sys++;
        return 0;
    }
    if ((s->yon & alinan_yon(a->ozel_rol)) == 0u) {
        a->sayac.rx_unknown_msgid++;
        return 0;
    }
    const uint8_t seq = t[4];
    if (a->ozel_seq_var != 0u && seq != (uint8_t)(a->ozel_son_seq + 1u)) {
        a->sayac.rx_seq_gap++;
    }
    a->ozel_son_seq = seq;
    a->ozel_seq_var = 1u;
    a->sayac.rx_frames++;

    c->msgid = s->msgid;
    c->seq = seq;
    c->sysid = t[5];
    c->compid = t[6];
    c->len = len;
    c->yuk_azami = s->yuk_azami;
    /* Yük: tampondaki kısmı, sonra gelen parçadaki kısmı. */
    size_t tamp_yuk = dolu > IDA_LINK_BASLIK_BOYU ? dolu - IDA_LINK_BASLIK_BOYU : 0u;
    if (tamp_yuk > len) {
        tamp_yuk = len;
    }
    if (tamp_yuk != 0u) {
        KOPYALA(c->yuk, &t[IDA_LINK_BASLIK_BOYU], tamp_yuk);
    }
    if (tamp_yuk < len) {
        KOPYALA(&c->yuk[tamp_yuk], &p[IDA_LINK_BASLIK_BOYU + tamp_yuk - dolu], (size_t)len - tamp_yuk);
    }
#if MP5_K
    SIFIRLA(&c->yuk[len], (size_t)s->yuk_azami - len);
#else
    SIFIRLA(&c->yuk[len], (size_t)IDA_LINK_YUK_AZAMI - len);
#endif
    return 1;
}

/* Tamamlanan aday (bütün baytları tamponda). */
static int cerceve_bitir(ida_link_alici_t *a, ida_link_cerceve_t *c)
{
    const uint8_t *t = a->ozel_tampon;
    if (a->ozel_satir >= IDA_LINK_TABLO_N) {
        return 0;
    }
    const tablo_satiri_t *s = &TABLO[a->ozel_satir];
    const uint8_t len = t[1];
    const size_t govde = IDA_LINK_BASLIK_BOYU + (size_t)len;
    const uint16_t crc = crc_hesapla(s->crc_extra, &t[1], govde - 1u);
    return cerceve_teslim(a, c, s, govde + 2u, t, crc);
}

/* Tek bayt; 1: bu baytla bir çerçeve kabul edildi. */
static int bayt_isle(ida_link_alici_t *a, uint8_t b, ida_link_cerceve_t *c)
{
    if (a->ozel_durum == DURUM_BOSTA) {
        bosta_bayt(a, b);
        return 0;
    }
    if (a->ozel_dolu >= IDA_LINK_CERCEVE_AZAMI) {
        a->ozel_durum = DURUM_BOSTA;
        a->ozel_dolu = 0u;
        bosta_bayt(a, b);
        return 0;
    }
    a->ozel_tampon[a->ozel_dolu++] = b;
    if (a->ozel_dolu == IDA_LINK_BASLIK_BOYU) {
        if (!baslik_denetle(a)) {
            yeniden_tara(a);
        }
        return 0;
    }
    if (a->ozel_dolu > IDA_LINK_BASLIK_BOYU && a->ozel_dolu == a->ozel_hedef) {
        a->ozel_durum = DURUM_BOSTA;
        a->ozel_dolu = 0u;
        return cerceve_bitir(a, c);
    }
    return 0;
}

#if MP5_B
/* Başlığı geçmiş adayın kalan baytları (yük + sağlama). veri[0, n) eldeki baytlar (n >= 1).
   Tüketilen bayt sayısını döndürür; *kabul = 1 ise bu baytlarla bir çerçeve teslim edildi. */
static size_t govde_isle(ida_link_alici_t *a, const uint8_t *veri, size_t n, ida_link_cerceve_t *c,
                         int *kabul)
{
    const size_t dolu = a->ozel_dolu;
    const size_t kalan = (size_t)a->ozel_hedef - dolu;
    if (n < kalan) {
        KOPYALA(&a->ozel_tampon[dolu], veri, n);
        a->ozel_dolu = (uint16_t)(dolu + n);
        *kabul = 0;
        return n;
    }
    /* Çerçeve bu parçada tamamlanır: aday kapanır (kural 3), CRC tek geçişte. */
    a->ozel_durum = DURUM_BOSTA;
    a->ozel_dolu = 0u;
    if (a->ozel_satir >= IDA_LINK_TABLO_N) {
        *kabul = 0;
        return kalan;
    }
    const tablo_satiri_t *s = &TABLO[a->ozel_satir];
    const size_t govde = IDA_LINK_BASLIK_BOYU + (size_t)a->ozel_tampon[1];
    uint16_t crc;
    if (dolu >= govde) {
        crc = crc_blok(crc_bas(), &a->ozel_tampon[1], govde - 1u);
    } else {
        crc = crc_blok(crc_bas(), &a->ozel_tampon[1], dolu - 1u);
        crc = crc_blok(crc, veri, govde - dolu);
    }
    crc = crc_adim(crc, s->crc_extra);
    *kabul = cerceve_teslim(a, c, s, dolu, veri, crc);
    return kalan;
}
#endif

ida_link_rc_t ida_link_al(ida_link_alici_t *a, const uint8_t *veri, size_t n,
                          size_t *tuketilen, ida_link_cerceve_t *cerceve)
{
    if (a == NULL || tuketilen == NULL || cerceve == NULL || (veri == NULL && n > 0u)) {
        return IDA_LINK_HATA_ARGUMAN;
    }
    if (!rol_gecerli(a->ozel_rol)) {
        return IDA_LINK_HATA_ROL;
    }
    size_t i = 0;
    while (i < n) {
#if MP5_B
        if (a->ozel_durum == DURUM_ADAY && a->ozel_dolu >= IDA_LINK_BASLIK_BOYU &&
            a->ozel_dolu < a->ozel_hedef) {
            int kabul = 0;
            const size_t k = govde_isle(a, &veri[i], n - i, cerceve, &kabul);
#if !MP5_H
            a->sayac.rx_bytes += (uint32_t)k;
#endif
            i += k;
            if (kabul) {
#if MP5_H
                a->sayac.rx_bytes += (uint32_t)i;
#endif
                *tuketilen = i;
                return IDA_LINK_CERCEVE;
            }
            continue;
        }
#endif
#if !MP5_H
        a->sayac.rx_bytes++;
#endif
        const int kabul = bayt_isle(a, veri[i], cerceve);
        ++i;
        if (kabul) {
#if MP5_H
            a->sayac.rx_bytes += (uint32_t)i;
#endif
            *tuketilen = i;
            return IDA_LINK_CERCEVE;
        }
    }
#if MP5_H
    a->sayac.rx_bytes += (uint32_t)n;
#endif
    *tuketilen = n;
    return IDA_LINK_BEKLE;
}

ida_link_rc_t ida_link_alici_tasma(ida_link_alici_t *a)
{
    if (a == NULL) {
        return IDA_LINK_HATA_ARGUMAN;
    }
    if (!rol_gecerli(a->ozel_rol)) {
        return IDA_LINK_HATA_ROL;
    }
    a->sayac.rx_overflow++;
    a->ozel_durum = DURUM_BOSTA;
    a->ozel_dolu = 0u;
    return IDA_LINK_TAMAM;
}

/* ------------------------------------------------------------------ */
/* Gönderici                                                          */
/* ------------------------------------------------------------------ */

ida_link_rc_t ida_link_gonderici_kur(ida_link_gonderici_t *g, ida_link_rol_t rol)
{
    if (g == NULL) {
        return IDA_LINK_HATA_ARGUMAN;
    }
    memset(g, 0, sizeof *g);
    if (!rol_gecerli((uint8_t)rol)) {
        return IDA_LINK_HATA_ROL;
    }
    g->ozel_rol = (uint8_t)rol;
    return IDA_LINK_TAMAM;
}

ida_link_rc_t ida_link_gonder(ida_link_gonderici_t *g, uint32_t msgid,
                              const void *mesaj, size_t mesaj_boyu,
                              uint8_t *cikis, size_t kapasite, size_t *yazilan)
{
    if (yazilan == NULL) {
        return IDA_LINK_HATA_ARGUMAN;
    }
    *yazilan = 0u;
    if (g == NULL || mesaj == NULL || cikis == NULL) {
        return IDA_LINK_HATA_ARGUMAN;
    }
    if (!rol_gecerli(g->ozel_rol)) {
        return IDA_LINK_HATA_ROL;
    }
    const tablo_satiri_t *s = satir_bul(msgid);
    if (s == NULL) {
        return IDA_LINK_HATA_KIMLIK;
    }
    if ((s->yon & gonderilen_yon(g->ozel_rol)) == 0u) {
        return IDA_LINK_HATA_YON;
    }
    if (mesaj_boyu != s->yapi_boyu) {
        return IDA_LINK_HATA_BOY;
    }

    /* Kırpma (A1-K05): sondaki sıfırlar atılır, ilk bayt kalır. */
    const uint8_t *yuk = (const uint8_t *)mesaj;
    uint8_t len = s->yuk_azami;
#if MP5_E
    while (len >= 5u) {
        uint32_t w;
        memcpy(&w, &yuk[len - 4u], 4u);
        if (w != 0u) {
            break;
        }
        len = (uint8_t)(len - 4u);
    }
#endif
    while (len > 1u && yuk[len - 1u] == 0u) {
        --len;
    }
    const size_t gereken = IDA_LINK_EK_BOY + (size_t)len;
    if (kapasite < gereken) {
        return IDA_LINK_HATA_TAMPON;
    }

    const int kart = g->ozel_rol == (uint8_t)IDA_LINK_ROL_KART;
    cikis[0] = (uint8_t)IDA_LINK_MAGIC;
    cikis[1] = len;
    cikis[2] = 0u;
    cikis[3] = 0u;
    cikis[4] = g->seq;
    cikis[5] = kart ? (uint8_t)IDA_LINK_KART_SYSID : (uint8_t)IDA_LINK_SIM_SYSID;
    cikis[6] = kart ? (uint8_t)IDA_LINK_KART_COMPID : (uint8_t)IDA_LINK_SIM_COMPID;
    cikis[7] = (uint8_t)(msgid & 0xFFu);
    cikis[8] = (uint8_t)((msgid >> 8) & 0xFFu);
    cikis[9] = (uint8_t)((msgid >> 16) & 0xFFu);
    const size_t govde = IDA_LINK_BASLIK_BOYU + (size_t)len;
#if MP5_E
    /* Yük kopyası ve CRC aynı geçişte. */
    uint16_t crc = crc_blok(crc_bas(), &cikis[1], IDA_LINK_BASLIK_BOYU - 1u);
    uint8_t *o = &cikis[IDA_LINK_BASLIK_BOYU];
    for (size_t i = 0; i < len; ++i) {
        const uint8_t b = yuk[i];
        o[i] = b;
        crc = crc_adim(crc, b);
    }
    crc = crc_adim(crc, s->crc_extra);
#else
    KOPYALA(&cikis[IDA_LINK_BASLIK_BOYU], yuk, len);
    const uint16_t crc = crc_hesapla(s->crc_extra, &cikis[1], govde - 1u);
#endif
    cikis[govde] = (uint8_t)(crc & 0xFFu);
    cikis[govde + 1u] = (uint8_t)(crc >> 8);

    g->seq = (uint8_t)(g->seq + 1u);
    *yazilan = gereken;
    return IDA_LINK_TAMAM;
}

/* ------------------------------------------------------------------ */
/* Çözme ve bilgi                                                     */
/* ------------------------------------------------------------------ */

ida_link_rc_t ida_link_coz(const ida_link_cerceve_t *cerceve, uint32_t msgid,
                           void *mesaj, size_t mesaj_boyu)
{
    if (cerceve == NULL || mesaj == NULL) {
        return IDA_LINK_HATA_ARGUMAN;
    }
    const tablo_satiri_t *s = satir_bul(msgid);
    if (s == NULL || cerceve->msgid != msgid) {
        return IDA_LINK_HATA_KIMLIK;
    }
    if (mesaj_boyu != s->yapi_boyu) {
        return IDA_LINK_HATA_BOY;
    }
#if MP5_H
    /* Yapı bir kez yazılır: ilk yuk_azami bayt yükten, kalanı (varsa) sıfır. */
    KOPYALA((uint8_t *)mesaj, cerceve->yuk, s->yuk_azami);
    if (mesaj_boyu > s->yuk_azami) {
        SIFIRLA((uint8_t *)mesaj + s->yuk_azami, mesaj_boyu - s->yuk_azami);
    }
#else
    SIFIRLA((uint8_t *)mesaj, mesaj_boyu);
    KOPYALA((uint8_t *)mesaj, cerceve->yuk, s->yuk_azami);
#endif
    return IDA_LINK_TAMAM;
}

ida_link_rc_t ida_link_mesaj_bilgisi(uint32_t msgid, ida_link_mesaj_bilgisi_t *b)
{
    if (b == NULL) {
        return IDA_LINK_HATA_ARGUMAN;
    }
    const tablo_satiri_t *s = satir_bul(msgid);
    if (s == NULL) {
        return IDA_LINK_HATA_KIMLIK;
    }
    b->msgid = s->msgid;
    b->crc_extra = s->crc_extra;
    b->yuk_azami = s->yuk_azami;
    b->yon = s->yon;
    b->yapi_boyu = s->yapi_boyu;
    return IDA_LINK_TAMAM;
}
