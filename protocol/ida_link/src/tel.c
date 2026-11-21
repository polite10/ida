/*
 * tel.c — A1 tel katmanı (ida_link/tel.h). Sözleşme: A1 §3.2, §4.1, A1-K101.
 *
 * Üretilmiş MAVLink kitaplığından yalnız şunlar kullanılır: mesaj yapı tipleri ve
 * MAVLINK_MSG_ID_* sabitleri (boy, CRC_EXTRA), checksum.h'in durumsuz crc_init işlevi.
 * Kitaplığın ayrıştırıcısı ve kodlayıcısı (kanal dizili olanlar da, *_buffer biçimleri de)
 * kullanılmaz: A1-K05–K07'nin ret ve yeniden eşleme kuralları bu dosyada uygulanır, bütün
 * durum çağıranın örneğindedir.
 *
 * A1-K101 güvencesi (derleme ve bağlama düzeyi):
 *  - MAVLINK_EXTERNAL_RX_STATUS / MAVLINK_EXTERNAL_RX_BUFFER tanımlanır ve kanal dizileri
 *    yalnız extern olarak bildirilir, hiçbir yerde tanımlanmaz. Kanal dizisine dokunan bir
 *    kitaplık işlevi (mavlink_parse_char, mavlink_frame_char, mavlink_finalize_message,
 *    *_pack, *_pack_chan, *_encode, *_send ...) bu dosyada kullanılırsa bağlama
 *    "m_mavlink_status / m_mavlink_buffer tanımsız" hatasıyla durur.
 *  - Ayrıca protokol.simge testi kitaplık arşivinde kanal işlevi ve dizisi simgesi
 *    bulunmadığını nm ile denetler (protocol/ida_link/cmake/simge_denetimi.cmake).
 *
 * Hız (W3-tel-hiz; ölçüm docs/olcumler/MP5-tel-maliyeti.md §9, olcum/mp5-tel-maliyeti
 * dalı). MP5 §9.6'daki değişikliklerden 2–6 alındı (prototipteki `wbt256eh` birleşimi);
 * açık arayüz ve davranış değişmez:
 *  2. memcpy/memset yerine sözcük genişliğinde yerel kopya ve sıfırlama (kopyala, sifirla);
 *  3. alıcıda başlık geçtikten sonra yük ve sağlama baytları blok hâlinde işlenir
 *     (govde_isle): CRC tek geçişte, yük çerçeveye doğrudan kaynaklardan kopyalanır;
 *  4. CRC 256 girdili tabloyla (tel_crc_tablo.h; seçimin gerekçesi orada);
 *  5. kodlayıcıda sondaki sıfırlar sözcük sözcük taranır, yük kopyası ve CRC aynı geçişte;
 *  6. kimlikten tablo satırına doğrudan dizin (ikili arama yerine), rx_bytes çağrı başına
 *     bir kez toplanır, ida_link_coz yapıyı bir kez yazar.
 * Madde 1 (teslim edilen yükün kuyruğunu yalnız yuk_azami'ye kadar sıfırlamak) alınmadı:
 * tel.h'nin "yuk[len, IDA_LINK_YUK_AZAMI) sıfırdır" sözünü değiştirir.
 * Eski ve yeni uygulamanın eşitliği olcum/tel_fark.c'de sınanır (eski kopya olcum/tel_eski.c).
 */
#ifdef MAVLINK_CHECK_MESSAGE_LENGTH
#error "MAVLINK_CHECK_MESSAGE_LENGTH tanimlanmaz (A1 7.3; uzunluk denetimi ida_link'te, A1-K05)"
#endif

#define MAVLINK_EXTERNAL_RX_STATUS 1
#define MAVLINK_EXTERNAL_RX_BUFFER 1
#include "mavlink/mavlink_types.h"
/* Bilerek tanımsız (yukarıdaki açıklama). */
extern mavlink_status_t m_mavlink_status[MAVLINK_COMM_NUM_BUFFERS];
extern mavlink_message_t m_mavlink_buffer[MAVLINK_COMM_NUM_BUFFERS];
#include "mavlink/ida/mavlink.h"

#include "ida_link/tel.h"

#include <string.h>

#include "ida_link/ozel/tablo_uretilen.h"
#include "tel_crc_tablo.h"

/* Tel yükü = yapının ilk baytları varsayımı yalnız küçük uçlu hedefte geçerlidir. */
#if defined(__BYTE_ORDER__) && defined(__ORDER_LITTLE_ENDIAN__)
#if __BYTE_ORDER__ != __ORDER_LITTLE_ENDIAN__
#error "ida_link yalniz kucuk uclu hedefler icindir"
#endif
#endif

/* ------------------------------------------------------------------ */
/* Mesaj tablosu (protocol/ida.xml + A1 Tablo 3.3; üretilmiş)         */
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

/* Yapı en az yük kadar uzun ve 16 bite sığar; tablo üreteci ile kitaplık aynı değerleri
   verir (üretecin kendi CRC_EXTRA ve boy hesabı, pymavlink'inkiyle). */
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

/* Kimlikten satıra doğrudan dizin (MP5 §9.6 md. 6). Dialect'in bütün kimlikleri A1 §3.2'nin
   46000–46099 aralığındadır (derleme zamanında denetlenir); DIZIN[kimlik - 46000] satırın
   sırası + 1, 0 ise kimlik dialect'te yok. Dizin üretilmiş tablo makrosundan kurulur. */
#define IDA_LINK_KIMLIK_TABAN 46000u
#define IDA_LINK_KIMLIK_ARALIK 100u
#define IDA_LINK_SIRA(id, crc, len, tip, yon) IDA_LINK_SIRA_##tip,
enum { IDA_LINK_TABLO_SATIRLARI(IDA_LINK_SIRA) IDA_LINK_SIRA_SAYISI };
#define IDA_LINK_ARALIK_DENETIMI(id, crc, len, tip, yon)                     \
    _Static_assert((id) >= IDA_LINK_KIMLIK_TABAN &&                           \
                       (id) < IDA_LINK_KIMLIK_TABAN + IDA_LINK_KIMLIK_ARALIK, \
                   "kimlik araligi: " #tip);
IDA_LINK_TABLO_SATIRLARI(IDA_LINK_ARALIK_DENETIMI)
_Static_assert(IDA_LINK_SIRA_SAYISI < 255, "dizin bir bayta sigar");
_Static_assert(IDA_LINK_SIRA_SAYISI == IDA_LINK_TABLO_SAYISI, "dizin ile tablo ayni satirlar");

#define IDA_LINK_DIZIN(id, crc, len, tip, yon) \
    [(id) - IDA_LINK_KIMLIK_TABAN] = (uint8_t)(IDA_LINK_SIRA_##tip + 1),
static const uint8_t DIZIN[IDA_LINK_KIMLIK_ARALIK] = {IDA_LINK_TABLO_SATIRLARI(IDA_LINK_DIZIN)};

static const tablo_satiri_t *satir_bul(uint32_t msgid)
{
    const uint32_t k = msgid - IDA_LINK_KIMLIK_TABAN;  /* aralık altı sarar, büyük olur */
    if (k >= IDA_LINK_KIMLIK_ARALIK) {
        return NULL;
    }
    const uint8_t s = DIZIN[k];
    return s != 0u ? &TABLO[s - 1u] : NULL;
}

/* ------------------------------------------------------------------ */
/* Kopya, sıfırlama, CRC                                              */
/* ------------------------------------------------------------------ */

/* Sözcük genişliğinde kopya (MP5 §9.6 md. 2). Sabit boylu memcpy derleyicide tek yükleme ya
   da saklama komutuna iner (hizasız erişim Cortex-M33'te ve host'ta serbesttir; takma ad
   kuralı korunur). Kuyruk döngüsüzdür: derleyici döngüyü kitaplık memcpy çağrısına geri
   çevirmesin (kartın newlib-nano memcpy/memset'i bayt döngüsüdür; MP5 §6). d ile s
   örtüşmez. */
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

static uint16_t crc_bas(void)
{
    uint16_t crc;
    crc_init(&crc);
    return crc;
}

static uint16_t crc_blok(uint16_t crc, const uint8_t *p, size_t n)
{
    for (size_t i = 0; i < n; ++i) {
        crc = tel_crc_adim(crc, p[i]);
    }
    return crc;
}

/* CRC_EXTRA ilk argümandır: boy ile yan yana karışabilecek iki tamsayı olmasın. */
static uint16_t crc_hesapla(uint8_t crc_extra, const uint8_t *p, size_t n)
{
    return tel_crc_adim(crc_blok(crc_bas(), p, n), crc_extra);
}

/* ------------------------------------------------------------------ */
/* Ortak                                                              */
/* ------------------------------------------------------------------ */

enum { DURUM_BOSTA = 0, DURUM_ADAY = 1 };

/* Çağırandan gelen rol, özgün (enum) genişliğiyle denetlenir; daraltma yalnız doğrulamadan
   sonra, saklamak için yapılır (ör. (ida_link_rol_t)257 kart rolüne dönüşmesin). */
static int rol_degeri_gecerli(ida_link_rol_t rol)
{
    return rol == IDA_LINK_ROL_KART || rol == IDA_LINK_ROL_SIM;
}

/* Örnekte saklanan rol. */
static int rol_gecerli(uint8_t rol)
{
    return rol == (uint8_t)IDA_LINK_ROL_KART || rol == (uint8_t)IDA_LINK_ROL_SIM;
}

/* Bu rolün alacağı yön biti ve beklediği karşı taraf kimliği. */
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
    if (!rol_degeri_gecerli(rol)) {
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
        /* A1-K02, K07: aday başlatmayan bayt; 0xFE ayrıca rx_mav1'e sayılır. */
        a->sayac.rx_resync_bytes++;
        if (b == IDA_LINK_MAGIC_V1) {
            a->sayac.rx_mav1++;
        }
    }
}

/* Başlığı reddedilen adayın 0xFD'sinden sonraki baytlarını baştan tarar (kural 2). Bu
   baytlar en çok 9 tanedir; yeni aday 10 bayta ulaşamayacağından burada karar doğmaz. */
static void yeniden_tara(ida_link_alici_t *a)
{
    uint8_t gecici[IDA_LINK_BASLIK_BOYU - 1u];
    uint16_t n = (uint16_t)(a->ozel_dolu - 1u);
    if (n > sizeof gecici) {
        n = (uint16_t)sizeof gecici;  /* ulaşılamaz: başlık kararı 10. baytta verilir */
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

/* Tamamlanan adayın sağlama sonrası denetimi ve teslimi (kural 3, 4). Adayın baytları:
   ozel_tampon[0, dolu) ve ardından p[0, ...) (dolu + p'den okunan = 12 + len; dolu >= 10).
   hesaplanan: [1, 10 + len) baytlarının CRC'si + CRC_EXTRA. 1: kabul edildi ve *c
   dolduruldu; reddedilirse *c'ye dokunulmaz. */
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
    /* Yük: önce tampondaki kısmı, sonra gelen parçadaki kısmı. */
    size_t tamp_yuk = dolu - IDA_LINK_BASLIK_BOYU;
    if (tamp_yuk > len) {
        tamp_yuk = len;
    }
    kopyala(c->yuk, &t[IDA_LINK_BASLIK_BOYU], tamp_yuk);
    if (tamp_yuk < len) {
        /* tamp_yuk < len iken dolu = 10 + tamp_yuk: kalan yük p[0]'dan başlar. */
        kopyala(&c->yuk[tamp_yuk], p, (size_t)len - tamp_yuk);
    }
    sifirla(&c->yuk[len], (size_t)IDA_LINK_YUK_AZAMI - len);
    return 1;
}

/* Tamamlanan aday, bütün baytları tamponda (bayt bayt yol). */
static int cerceve_bitir(ida_link_alici_t *a, ida_link_cerceve_t *c)
{
    const uint8_t *t = a->ozel_tampon;
    if (a->ozel_satir >= IDA_LINK_TABLO_N) {
        return 0;  /* ulaşılamaz: satır başlık denetiminde kurulur */
    }
    const tablo_satiri_t *s = &TABLO[a->ozel_satir];
    const size_t govde = IDA_LINK_BASLIK_BOYU + (size_t)t[1];
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
        /* Ulaşılamaz (aday en çok 12 + 255 bayttır); bozulmuş örnek durumunda tampon dışına
           yazmak yerine yeniden eşlenir. */
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

/* Blok yolu açık mı: başlığı geçmiş, gövdesi eksik ve tutarlı bir aday. Tutarlılık
   (ozel_hedef = 12 + len <= tampon boyu) iyi kurulmuş örnekte hep sağlanır; bozulmuş örnek
   bayt bayt yola düşer, orada tampon sınırı denetlenir (tampon dışına yazılmaz). */
static int govde_bekliyor(const ida_link_alici_t *a)
{
    return a->ozel_durum == DURUM_ADAY && a->ozel_dolu >= IDA_LINK_BASLIK_BOYU &&
           a->ozel_dolu < a->ozel_hedef &&
           a->ozel_hedef == (uint16_t)(IDA_LINK_EK_BOY + a->ozel_tampon[1]);
}

/* Başlığı geçmiş adayın kalan baytları (yük + sağlama) blok hâlinde (MP5 §9.6 md. 3).
   veri[0, n) eldeki baytlardır (n >= 1); govde_bekliyor(a) doğrudur. Tüketilen bayt
   sayısını döndürür; *kabul = 1 ise bu baytlarla bir çerçeve teslim edildi. Bayt bayt yolla
   aynı sonucu verir: aday parçada tamamlanmıyorsa baytlar tampona eklenir; tamamlanıyorsa
   aday kapanır (durum boşta) ve kural 3–4 bayt bayt yoldaki sırayla uygulanır. */
static size_t govde_isle(ida_link_alici_t *a, const uint8_t *veri, size_t n,
                         ida_link_cerceve_t *c, int *kabul)
{
    const size_t dolu = a->ozel_dolu;
    const size_t kalan = (size_t)a->ozel_hedef - dolu;
    if (n < kalan) {
        kopyala(&a->ozel_tampon[dolu], veri, n);
        a->ozel_dolu = (uint16_t)(dolu + n);
        *kabul = 0;
        return n;
    }
    a->ozel_durum = DURUM_BOSTA;
    a->ozel_dolu = 0u;
    if (a->ozel_satir >= IDA_LINK_TABLO_N) {
        *kabul = 0;  /* ulaşılamaz: satır başlık denetiminde kurulur */
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
    crc = tel_crc_adim(crc, s->crc_extra);
    *kabul = cerceve_teslim(a, c, s, dolu, veri, crc);
    return kalan;
}

ida_link_rc_t ida_link_al(ida_link_alici_t *a, const uint8_t *veri, size_t n,
                          size_t *tuketilen, ida_link_cerceve_t *cerceve)
{
    if (a == NULL || tuketilen == NULL || cerceve == NULL || (veri == NULL && n > 0u)) {
        return IDA_LINK_HATA_ARGUMAN;
    }
    if (!rol_gecerli(a->ozel_rol)) {
        return IDA_LINK_HATA_ROL;
    }
    /* rx_bytes tüketilen bayt sayısı kadar, çağrı başına bir kez artar (MP5 §9.6 md. 6);
       çağıran sayacı yalnız dönüşten sonra görür, sonuç bayt bayt saymakla aynıdır. */
    size_t i = 0;
    while (i < n) {
        int kabul = 0;
        if (govde_bekliyor(a)) {
            i += govde_isle(a, &veri[i], n - i, cerceve, &kabul);
        } else {
            kabul = bayt_isle(a, veri[i], cerceve);
            ++i;
        }
        if (kabul) {
            a->sayac.rx_bytes += (uint32_t)i;
            *tuketilen = i;
            return IDA_LINK_CERCEVE;
        }
    }
    a->sayac.rx_bytes += (uint32_t)n;
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
    if (!rol_degeri_gecerli(rol)) {
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

    /* Kırpma (A1-K05): sondaki sıfırlar atılır, ilk bayt kalır. Önce dört baytlık sözcükler
       (len >= 5 iken; en az bir bayt kalır), sonra bayt bayt (MP5 §9.6 md. 5). */
    const uint8_t *yuk = (const uint8_t *)mesaj;
    uint8_t len = s->yuk_azami;
    while (len >= 5u) {
        uint32_t w;
        memcpy(&w, &yuk[len - 4u], 4u);
        if (w != 0u) {
            break;
        }
        len = (uint8_t)(len - 4u);
    }
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
    /* Yük kopyası ve CRC aynı geçişte. */
    uint16_t crc = crc_blok(crc_bas(), &cikis[1], IDA_LINK_BASLIK_BOYU - 1u);
    uint8_t *o = &cikis[IDA_LINK_BASLIK_BOYU];
    for (size_t i = 0; i < len; ++i) {
        const uint8_t b = yuk[i];
        o[i] = b;
        crc = tel_crc_adim(crc, b);
    }
    crc = tel_crc_adim(crc, s->crc_extra);
    const size_t govde = IDA_LINK_BASLIK_BOYU + (size_t)len;
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
    /* Yapı bir kez yazılır: ilk yuk_azami bayt yükten, kalanı (varsa) sıfır. Sonuç "önce
       sıfırla, sonra yuk_azami bayt kopyala" ile aynıdır (yapi_boyu >= yuk_azami). */
    uint8_t *m = (uint8_t *)mesaj;
    kopyala(m, cerceve->yuk, s->yuk_azami);
    if (mesaj_boyu > s->yuk_azami) {
        sifirla(&m[s->yuk_azami], mesaj_boyu - s->yuk_azami);
    }
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
