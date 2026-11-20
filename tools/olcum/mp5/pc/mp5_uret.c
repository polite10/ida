/*
 * W0-MP5a ölçüm prototipi (atılabilir): girdi akışı üreteci ve host referansı.
 *
 *   mp5_uret <çıktı-dizini>
 *
 * Ürün kitaplığının kaynağını (protocol/ida_link/src/tel.c) host'ta derleyip kullanır:
 *  - A1 Ek B altın çerçevelerini (veri/altin.json'dan üretilmiş ida_link_altin_veri.c) çözüp
 *    mesaj yapılarının "altin" görüntüsünü kurar; ayrıca "ham" (yükün her baytı sıfırdan farklı:
 *    hiç kırpılmaz) ve "sifir" (yapı sıfır: yük 1 bayta kırpılır) görüntüleri.
 *  - Ayrıştırma akışlarını ida_link_gonder ile seq 0'dan kodlar, ida_link_al/ida_link_coz ile
 *    geri ayrıştırıp kartın karşılaştıracağı referans özetleri hesaplar.
 *  - Kodlama akışlarının bir yinelemedeki çıktı boyunu ve FNV-1a'sını hesaplar.
 * Çıktılar: mp5_akislar.c (kart için sabit veri), beklenen.json, akis-dokum.md.
 * Akışların tanımı (çerçeve kümeleri) ve gerekçesi docs/olcumler/MP5-tel-maliyeti.md'dedir.
 */
#include "ida_link/tel.h"
#include "ida_link_altin_veri.h"
#include "mavlink/ida/mavlink.h"

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../fw/mp5_veri.h"

#define YAPI_AZAMI 256u
#define AKIS_AZAMI 4096u

enum { V_ALTIN = 0, V_HAM = 1, V_SIFIR = 2, V_TAM = 3 };
static const char *const VARYANT_AD[] = {"altin", "ham", "sifir", "tam"};

typedef struct {
    uint32_t msgid;
    int varyant;
    uint16_t boy, yuk_azami;
    uint8_t yon;
    const char *ad;
    union {
        uint64_t hiza;
        uint8_t b[YAPI_AZAMI];
    } y;
} yapi_t;

static yapi_t havuz[256];
static unsigned havuz_n;

static void olum(const char *m, long v)
{
    fprintf(stderr, "mp5_uret: %s (%ld)\n", m, v);
    exit(1);
}

static uint32_t fnv(uint32_t h, const uint8_t *p, size_t n)
{
    for (size_t i = 0; i < n; ++i) {
        h = (h ^ p[i]) * 16777619u;
    }
    return h;
}
#define FNV_BAS 2166136261u

static const ida_link_altin_t *altin_bul(uint32_t msgid)
{
    for (unsigned i = 0; i < IDA_LINK_ALTIN_SAYISI; ++i) {
        if (ida_link_altin[i].msgid == msgid && !ida_link_altin[i].ek_vaka) {
            return &ida_link_altin[i];
        }
    }
    return NULL;
}

/* Yapı görüntüsünü kurar ya da havuzda bulur; havuz dizinini döndürür. */
static unsigned yapi_al(uint32_t msgid, int varyant)
{
    for (unsigned i = 0; i < havuz_n; ++i) {
        if (havuz[i].msgid == msgid && havuz[i].varyant == varyant) {
            return i;
        }
    }
    if (havuz_n >= sizeof havuz / sizeof havuz[0]) olum("havuz dolu", (long)havuz_n);
    yapi_t *y = &havuz[havuz_n];
    memset(y, 0, sizeof *y);
    ida_link_mesaj_bilgisi_t b;
    if (ida_link_mesaj_bilgisi(msgid, &b) != IDA_LINK_TAMAM) olum("dialect'te yok", (long)msgid);
    if (b.yapi_boyu > YAPI_AZAMI) olum("yapı büyük", (long)b.yapi_boyu);
    y->msgid = msgid;
    y->varyant = varyant;
    y->boy = b.yapi_boyu;
    y->yuk_azami = b.yuk_azami;
    y->yon = b.yon;
    const ida_link_altin_t *g = altin_bul(msgid);
    if (g == NULL) olum("altın çerçeve yok", (long)msgid);
    y->ad = g->ad;
    if (varyant == V_ALTIN || varyant == V_TAM) {
        ida_link_alici_t a;
        ida_link_cerceve_t c;
        size_t tk = 0;
        const ida_link_rol_t rol = g->sysid == IDA_LINK_SIM_SYSID ? IDA_LINK_ROL_KART : IDA_LINK_ROL_SIM;
        if (ida_link_alici_kur(&a, rol) != IDA_LINK_TAMAM ||
            ida_link_al(&a, g->bayt, g->cerceve, &tk, &c) != IDA_LINK_CERCEVE || tk != g->cerceve ||
            ida_link_coz(&c, msgid, y->y.b, y->boy) != IDA_LINK_TAMAM) {
            olum("altın çerçeve çözülemedi", (long)msgid);
        }
        if (varyant == V_TAM) {
            if (msgid != MAVLINK_MSG_ID_IDA_PING360_DATA) olum("tam yalnız PING360_DATA", (long)msgid);
            /* Dolu sonar parçası: 240 örneğin hepsi sıfırdan farklı (son parça olmayan parça). */
            const size_t o = offsetof(mavlink_ida_ping360_data_t, data);
            for (size_t i = 0; i < 240u; ++i) {
                y->y.b[o + i] = (uint8_t)(1u + (i * 7u) % 250u);
            }
        }
    } else if (varyant == V_HAM) {
        for (unsigned i = 0; i < y->yuk_azami; ++i) {
            y->y.b[i] = (uint8_t)(1u + (i * 7u + msgid) % 251u);
        }
    }
    return havuz_n++;
}

/* ---- Akış tanımları ---------------------------------------------------------------------- */

typedef struct {
    uint32_t msgid;
    int tam; /* altin varyantında PING360_DATA dolu parça */
} giris_t;

#define G(id) {id, 0}
#define GT(id) {id, 1}

enum {
    TF = 46010, IMU = 46020, MAG = 46021, BARO = 46022, GNSS = 46023, DVL = 46024, PING1D = 46025,
    BATT = 46026, LEAK = 46027, SAFETY = 46028, P360H = 46029, P360D = 46030, DETB = 46035,
    DETR = 46036, CSIMPLE = 46040, CMANUAL = 46041, CGUIDED = 46042, CMISSION = 46043,
    CPARAM = 46044, CFC = 46045, EVR = 46046, ACT = 46060, EVT = 46061, CRES = 46062, NAV = 46063,
    STAT = 46064, FCST = 46065, CTRACE = 46069
};

/* (a) E1: 1 050 B (kırpılmamış boylarla), 22 çerçeve. Bir pencerenin sonar dışı en yoğun karışımı:
   toplu ve atanmış üyeler (algılama partisi: başlık + 4 kayıt; 4 daraltıcı olmayan komut), sonra
   kritik üyeler (sensörler, IMU, emniyet, 2 daraltıcı komut, 4 olay sonucu) ve TF (A1-K36, K39,
   K34, K113; kritik 501 B <= B_krit 520 B). */
static const giris_t AKIS_E1[] = {
    G(DETB), G(DETR), G(DETR), G(DETR), G(DETR),
    G(CMANUAL), G(CGUIDED), G(CFC), G(CSIMPLE),
    G(MAG), G(DVL), G(PING1D), G(LEAK),
    G(IMU), G(SAFETY), G(CSIMPLE), G(CFC), G(EVR), G(EVR), G(EVR), G(EVR),
    G(TF),
};

/* (b) PEN: tam pencere 1 200 B (kırpılmamış boylarla) = B_pencere, 20 çerçeve: algılama partisi
   (başlık + 1 kayıt), Ping360 başlığı ve iki veri parçası (altin varyantında biri dolu, biri
   Ek B'nin kısmi parçası), 2 daraltıcı olmayan komut, 5 sensör, IMU, emniyet, 1 daraltıcı komut,
   4 olay sonucu, TF (kritik 465 B). */
static const giris_t AKIS_PEN[] = {
    G(DETB), G(DETR),
    G(P360H), GT(P360D), G(P360D),
    G(CSIMPLE), G(CFC),
    G(MAG), G(BARO), G(PING1D), G(BATT), G(LEAK),
    G(IMU), G(SAFETY), G(CSIMPLE), G(EVR), G(EVR), G(EVR), G(EVR),
    G(TF),
};

/* (d) BUY: dialect'in en büyük çerçevesi, kırpılmamış PING360_DATA (258 B). */
static const giris_t AKIS_BUY[] = {G(P360D)};

/* (c) KS: kart -> sim en kötü periyot (A1 E19'un çerçeve kümesi, tam sayıya yukarı yuvarlanmış):
   (i) 2 ACTUATOR (TF yanıtı + ASYNC); (ii) 10 EVENT (473,7 B/periyot üst sınırı), 6 CMD_RESULT
   (234 B); (iii) R_iii = 500 B payı: 2 CALL_TRACE, NAV, STATUS, FC_STATUS (502 B).
   Kırpılmamış boylarla 1 326 B, 23 çerçeve (E19: 1 282,5 B). */
static const giris_t AKIS_KS[] = {
    G(ACT), G(ACT),
    G(EVT), G(EVT), G(EVT), G(EVT), G(EVT), G(EVT), G(EVT), G(EVT), G(EVT), G(EVT),
    G(CRES), G(CRES), G(CRES), G(CRES), G(CRES), G(CRES),
    G(CTRACE), G(CTRACE), G(NAV), G(STAT), G(FCST),
};

/* ---- Çıktı ------------------------------------------------------------------------------ */

static FILE *fc, *fj, *fd;
static uint32_t veri_kimlik = FNV_BAS;

static void dizi_yaz(const char *ad, const uint8_t *p, size_t n)
{
    veri_kimlik = fnv(veri_kimlik, p, n);
    fprintf(fc, "static const uint8_t %s[%zu] = {", ad, n ? n : 1u);
    for (size_t i = 0; i < n; ++i) {
        fprintf(fc, "%s0x%02x,", (i % 16u) ? "" : "\n    ", p[i]);
    }
    if (n == 0) fprintf(fc, "0");
    fprintf(fc, "\n};\n");
}

typedef struct {
    char ad[24];
    int varyant;
    int grup;
    ida_link_rol_t rol_alici;
    uint8_t bayt[AKIS_AZAMI];
    size_t n_bayt;
    uint32_t n_cerceve, ozet_al, ozet_coz, fnv_cerceve, fnv_yapi;
} ayr_t;

typedef struct {
    char ad[24];
    int varyant;
    int grup;
    ida_link_rol_t rol_gonderen;
    uint16_t idx[64];
    size_t n_mesaj;
    size_t n_bayt;
    uint32_t fnv;
} kod_t;

static ayr_t ayrlar[128];
static unsigned ayr_n;
static kod_t kodlar[128];
static unsigned kod_n;

static int etkin_varyant(const giris_t *g, int varyant)
{
    return (varyant == V_ALTIN && g->tam) ? V_TAM : varyant;
}

/* Dökümde ardışık aynı (msgid, boy) çerçeveleri toplar. */
static void dokum_yaz(const char *baslik, const uint8_t *v, size_t n)
{
    fprintf(fd, "\n#### %s\n\n| Kimlik | Ad | Telde boy (B) | Adet |\n|---|---|---:|---:|\n", baslik);
    size_t i = 0;
    uint32_t onceki_id = 0;
    unsigned onceki_boy = 0, adet = 0, toplam_c = 0;
    while (i + IDA_LINK_EK_BOY <= n) {
        const unsigned len = v[i + 1];
        const uint32_t id = (uint32_t)v[i + 7] | ((uint32_t)v[i + 8] << 8) | ((uint32_t)v[i + 9] << 16);
        const unsigned boy = len + IDA_LINK_EK_BOY;
        if (adet > 0 && (id != onceki_id || boy != onceki_boy)) {
            fprintf(fd, "| %u | %s | %u | %u |\n", (unsigned)onceki_id, altin_bul(onceki_id)->ad,
                    onceki_boy, adet);
            adet = 0;
        }
        onceki_id = id;
        onceki_boy = boy;
        adet++;
        toplam_c++;
        i += boy;
    }
    if (adet > 0) {
        fprintf(fd, "| %u | %s | %u | %u |\n", (unsigned)onceki_id, altin_bul(onceki_id)->ad,
                onceki_boy, adet);
    }
    fprintf(fd, "| | **Toplam** | **%zu** | **%u** |\n", n, toplam_c);
}

static void ayr_ekle(const char *ad, int grup, const giris_t *gl, size_t gn, int varyant,
                     ida_link_rol_t gonderen, ida_link_rol_t alici)
{
    if (ayr_n >= sizeof ayrlar / sizeof ayrlar[0]) olum("ayr dolu", 0);
    ayr_t *s = &ayrlar[ayr_n++];
    memset(s, 0, sizeof *s);
    snprintf(s->ad, sizeof s->ad, "%s", ad);
    s->varyant = varyant;
    s->grup = grup;
    s->rol_alici = alici;
    ida_link_gonderici_t g;
    if (ida_link_gonderici_kur(&g, gonderen) != IDA_LINK_TAMAM) olum("gönderici", 0);
    for (size_t i = 0; i < gn; ++i) {
        const yapi_t *y = &havuz[yapi_al(gl[i].msgid, etkin_varyant(&gl[i], varyant))];
        size_t yz = 0;
        const ida_link_rc_t rc = ida_link_gonder(&g, y->msgid, y->y.b, y->boy, &s->bayt[s->n_bayt],
                                                 AKIS_AZAMI - s->n_bayt, &yz);
        if (rc != IDA_LINK_TAMAM) olum("kodlama", (long)rc);
        s->n_bayt += yz;
    }
    /* Host'ta geri ayrıştırma: referans özetler. */
    ida_link_alici_t a;
    ida_link_cerceve_t c;
    if (ida_link_alici_kur(&a, alici) != IDA_LINK_TAMAM) olum("alıcı", 0);
    s->fnv_cerceve = FNV_BAS;
    s->fnv_yapi = FNV_BAS;
    size_t i = 0;
    while (i < s->n_bayt) {
        size_t tk = 0;
        const ida_link_rc_t rc = ida_link_al(&a, &s->bayt[i], s->n_bayt - i, &tk, &c);
        if (rc == IDA_LINK_CERCEVE) {
            s->n_cerceve++;
            s->ozet_al += c.msgid + c.len + c.yuk[c.len - 1u];
            uint8_t k[7] = {(uint8_t)c.msgid, (uint8_t)(c.msgid >> 8), (uint8_t)(c.msgid >> 16),
                            (uint8_t)(c.msgid >> 24), c.seq, c.len, c.yuk_azami};
            s->fnv_cerceve = fnv(s->fnv_cerceve, k, sizeof k);
            s->fnv_cerceve = fnv(s->fnv_cerceve, c.yuk, c.yuk_azami);
            uint8_t yapi[YAPI_AZAMI];
            ida_link_mesaj_bilgisi_t b;
            if (ida_link_mesaj_bilgisi(c.msgid, &b) != IDA_LINK_TAMAM ||
                ida_link_coz(&c, c.msgid, yapi, b.yapi_boyu) != IDA_LINK_TAMAM) {
                olum("çözme", (long)c.msgid);
            }
            s->ozet_coz += c.msgid + yapi[0] + yapi[b.yapi_boyu - 1u];
            s->fnv_yapi = fnv(s->fnv_yapi, yapi, b.yapi_boyu);
        } else if (rc != IDA_LINK_BEKLE) {
            olum("ayrıştırma", (long)rc);
        }
        i += tk;
    }
    const ida_link_sayaclar_t *z = &a.sayac;
    if (s->n_cerceve != gn || z->rx_frames != gn || z->rx_bytes != s->n_bayt || z->rx_crc_err ||
        z->rx_unknown_msgid || z->rx_iflag || z->rx_bad_len || z->rx_wrong_sys || z->rx_mav1 ||
        z->rx_resync_bytes || z->rx_seq_gap || z->rx_overflow) {
        olum("host geri ayrıştırma sayaçları beklenen değil", (long)s->n_cerceve);
    }
    if (grup == 0) {
        char b[96];
        snprintf(b, sizeof b, "%s / %s: %zu B, %u çerçeve", ad, VARYANT_AD[varyant], s->n_bayt, s->n_cerceve);
        dokum_yaz(b, s->bayt, s->n_bayt);
    }
}

static void kod_ekle(const char *ad, int grup, const giris_t *gl, size_t gn, int varyant,
                     ida_link_rol_t gonderen)
{
    if (kod_n >= sizeof kodlar / sizeof kodlar[0]) olum("kod dolu", 0);
    if (gn > 64u) olum("kod akışı uzun", (long)gn);
    kod_t *s = &kodlar[kod_n++];
    memset(s, 0, sizeof *s);
    snprintf(s->ad, sizeof s->ad, "%s", ad);
    s->varyant = varyant;
    s->grup = grup;
    s->rol_gonderen = gonderen;
    ida_link_gonderici_t g;
    if (ida_link_gonderici_kur(&g, gonderen) != IDA_LINK_TAMAM) olum("gönderici", 0);
    static uint8_t cikis[AKIS_AZAMI];
    for (size_t i = 0; i < gn; ++i) {
        const unsigned k = yapi_al(gl[i].msgid, etkin_varyant(&gl[i], varyant));
        s->idx[s->n_mesaj++] = (uint16_t)k;
        size_t yz = 0;
        const ida_link_rc_t rc = ida_link_gonder(&g, havuz[k].msgid, havuz[k].y.b, havuz[k].boy,
                                                 &cikis[s->n_bayt], AKIS_AZAMI - s->n_bayt, &yz);
        if (rc != IDA_LINK_TAMAM) olum("kodlama", (long)rc);
        s->n_bayt += yz;
    }
    s->fnv = fnv(FNV_BAS, cikis, s->n_bayt);
    if (grup == 0) {
        char b[96];
        snprintf(b, sizeof b, "%s / %s: %zu B, %zu çerçeve", ad, VARYANT_AD[varyant], s->n_bayt, s->n_mesaj);
        dokum_yaz(b, cikis, s->n_bayt);
    }
}

#define N(a) (sizeof(a) / sizeof((a)[0]))

int main(int argc, char **argv)
{
    if (argc != 2) {
        fprintf(stderr, "kullanım: mp5_uret <çıktı-dizini>\n");
        return 2;
    }
    char yol[1024];
    snprintf(yol, sizeof yol, "%s/mp5_akislar.c", argv[1]);
    fc = fopen(yol, "w");
    snprintf(yol, sizeof yol, "%s/beklenen.json", argv[1]);
    fj = fopen(yol, "w");
    snprintf(yol, sizeof yol, "%s/akis-dokum.md", argv[1]);
    fd = fopen(yol, "w");
    if (!fc || !fj || !fd) olum("çıktı açılamadı", 0);

    /* Altın çerçevelerin bütünlüğü: her mesajın altın yapısı yeniden kodlanınca Ek B baytlarını
       (seq ve rol aynıyken) verir. */
    for (unsigned i = 0; i < IDA_LINK_ALTIN_SAYISI; ++i) {
        const ida_link_altin_t *a = &ida_link_altin[i];
        if (a->ek_vaka) continue;
        const yapi_t *y = &havuz[yapi_al(a->msgid, V_ALTIN)];
        ida_link_gonderici_t g;
        (void)ida_link_gonderici_kur(&g, a->sysid == IDA_LINK_SIM_SYSID ? IDA_LINK_ROL_SIM : IDA_LINK_ROL_KART);
        g.seq = a->seq;
        uint8_t c[IDA_LINK_CERCEVE_AZAMI];
        size_t yz = 0;
        if (ida_link_gonder(&g, a->msgid, y->y.b, y->boy, c, sizeof c, &yz) != IDA_LINK_TAMAM ||
            yz != a->cerceve || memcmp(c, a->bayt, yz) != 0) {
            olum("altın çerçeve yeniden kodlanınca Ek B ile aynı değil", (long)a->msgid);
        }
    }

    fprintf(fd, "# MP5 girdi akışlarının çerçeve dökümü (mp5_uret; üretildi, elle değiştirmeyin)\n");
    fprintf(fd, "\nTelde boy = 12 + kırpılmış yük. Varyantlar: altin = A1 Ek B alan desenleri "
                "(PEN'de bir Ping360 parçası dolu), ham = yükün her baytı sıfırdan farklı (kırpılmaz), "
                "sifir = yapı sıfır (yük 1 bayta kırpılır).\n");

    const int VARS[3] = {V_ALTIN, V_HAM, V_SIFIR};
    for (int v = 0; v < 3; ++v) {
        ayr_ekle("E1", 0, AKIS_E1, N(AKIS_E1), VARS[v], IDA_LINK_ROL_SIM, IDA_LINK_ROL_KART);
        ayr_ekle("PEN", 0, AKIS_PEN, N(AKIS_PEN), VARS[v], IDA_LINK_ROL_SIM, IDA_LINK_ROL_KART);
    }
    ayr_ekle("BUY", 0, AKIS_BUY, N(AKIS_BUY), V_HAM, IDA_LINK_ROL_SIM, IDA_LINK_ROL_KART);
    for (int v = 0; v < 3; ++v) {
        kod_ekle("KS", 0, AKIS_KS, N(AKIS_KS), VARS[v], IDA_LINK_ROL_KART);
    }
    kod_ekle("BUYK", 0, AKIS_BUY, N(AKIS_BUY), V_HAM, IDA_LINK_ROL_SIM);

    /* Doğrusal uyum: dialect'in her mesajı, kırpılmamış, tek çerçeve. Sim -> kart ve iki yönlü
       mesajlar sim rolünden gönderilip kart rolünde alınır; kart -> sim mesajları tersine. */
    for (unsigned i = 0; i < IDA_LINK_ALTIN_SAYISI; ++i) {
        const ida_link_altin_t *a = &ida_link_altin[i];
        if (a->ek_vaka) continue;
        ida_link_mesaj_bilgisi_t b;
        (void)ida_link_mesaj_bilgisi(a->msgid, &b);
        const int simden = (b.yon & IDA_LINK_YON_SIM_KART) != 0;
        const giris_t gi[1] = {G(a->msgid)};
        char ad[24];
        snprintf(ad, sizeof ad, "FIT-%u", (unsigned)a->msgid);
        ayr_ekle(ad, 1, gi, 1, V_HAM, simden ? IDA_LINK_ROL_SIM : IDA_LINK_ROL_KART,
                 simden ? IDA_LINK_ROL_KART : IDA_LINK_ROL_SIM);
        kod_ekle(ad, 1, gi, 1, V_HAM, simden ? IDA_LINK_ROL_SIM : IDA_LINK_ROL_KART);
    }

    /* ---- mp5_akislar.c ---- */
    fprintf(fc, "/* Üretilmiş dosya (tools/olcum/mp5/pc/mp5_uret.c); değiştirmeyin. */\n");
    fprintf(fc, "#include \"mp5_veri.h\"\n\n");
    for (unsigned i = 0; i < havuz_n; ++i) {
        char ad[32];
        snprintf(ad, sizeof ad, "yapi_%u", i);
        dizi_yaz(ad, havuz[i].y.b, havuz[i].boy);
    }
    fprintf(fc, "const mp5_yapi_t mp5_yapilar[] = {\n");
    for (unsigned i = 0; i < havuz_n; ++i) {
        fprintf(fc, "    {%uu, %uu, %uu, yapi_%u}, /* %s %s */\n", (unsigned)havuz[i].msgid, havuz[i].boy,
                havuz[i].yuk_azami, i, havuz[i].ad, VARYANT_AD[havuz[i].varyant]);
    }
    fprintf(fc, "};\nconst uint32_t MP5_YAPI_N = %uu;\n\n", havuz_n);
    for (unsigned i = 0; i < ayr_n; ++i) {
        char ad[32];
        snprintf(ad, sizeof ad, "akis_%u", i);
        dizi_yaz(ad, ayrlar[i].bayt, ayrlar[i].n_bayt);
    }
    fprintf(fc, "const mp5_ayr_t mp5_ayr[] = {\n");
    for (unsigned i = 0; i < ayr_n; ++i) {
        const ayr_t *s = &ayrlar[i];
        fprintf(fc, "    {\"%s\", \"%s\", %du, %du, akis_%u, %zuu, %uu, %uu, %uu, 0x%08xu, 0x%08xu},\n", s->ad,
                VARYANT_AD[s->varyant], s->grup, (int)s->rol_alici, i, s->n_bayt, s->n_cerceve, s->ozet_al,
                s->ozet_coz, s->fnv_cerceve, s->fnv_yapi);
        uint8_t k[24];
        memcpy(k, &s->ozet_al, 4);
        memcpy(k + 4, &s->ozet_coz, 4);
        memcpy(k + 8, &s->fnv_cerceve, 4);
        memcpy(k + 12, &s->fnv_yapi, 4);
        memcpy(k + 16, &s->n_cerceve, 4);
        k[20] = (uint8_t)s->grup;
        k[21] = (uint8_t)s->rol_alici;
        k[22] = (uint8_t)s->varyant;
        k[23] = 0;
        veri_kimlik = fnv(veri_kimlik, k, sizeof k);
    }
    fprintf(fc, "};\nconst uint32_t MP5_AYR_N = %uu;\n\n", ayr_n);
    for (unsigned i = 0; i < kod_n; ++i) {
        const kod_t *s = &kodlar[i];
        fprintf(fc, "static const uint16_t kidx_%u[%zu] = {", i, s->n_mesaj);
        for (size_t j = 0; j < s->n_mesaj; ++j) {
            fprintf(fc, "%u,", s->idx[j]);
        }
        fprintf(fc, "};\n");
        veri_kimlik = fnv(veri_kimlik, (const uint8_t *)s->idx, s->n_mesaj * sizeof s->idx[0]);
        veri_kimlik = fnv(veri_kimlik, (const uint8_t *)&s->fnv, sizeof s->fnv);
    }
    fprintf(fc, "const mp5_kod_t mp5_kod[] = {\n");
    for (unsigned i = 0; i < kod_n; ++i) {
        const kod_t *s = &kodlar[i];
        fprintf(fc, "    {\"%s\", \"%s\", %du, %du, kidx_%u, %zuu, %zuu, 0x%08xu},\n", s->ad,
                VARYANT_AD[s->varyant], s->grup, (int)s->rol_gonderen, i, s->n_mesaj, s->n_bayt, s->fnv);
    }
    fprintf(fc, "};\nconst uint32_t MP5_KOD_N = %uu;\n\n", kod_n);
    fprintf(fc, "const uint16_t mp5_boy_tablosu[100] = {");
    for (uint32_t id = 46000u; id < 46100u; ++id) {
        ida_link_mesaj_bilgisi_t b;
        const unsigned boy = ida_link_mesaj_bilgisi(id, &b) == IDA_LINK_TAMAM ? b.yapi_boyu : 0u;
        fprintf(fc, "%s%u,", ((id - 46000u) % 16u) ? "" : "\n    ", boy);
    }
    fprintf(fc, "\n};\n");
    fprintf(fc, "const uint32_t MP5_VERI_KIMLIK = 0x%08xu;\n", veri_kimlik);

    /* ---- beklenen.json ---- */
    fprintf(fj, "{\n  \"veri_kimlik\": \"%08x\",\n  \"ayr\": [\n", veri_kimlik);
    for (unsigned i = 0; i < ayr_n; ++i) {
        const ayr_t *s = &ayrlar[i];
        fprintf(fj,
                "    {\"ad\": \"%s\", \"varyant\": \"%s\", \"grup\": %d, \"bayt\": %zu, \"cerceve\": %u, "
                "\"ozet_al\": %u, \"ozet_coz\": %u, \"fnv_cerceve\": \"%08x\", \"fnv_yapi\": \"%08x\"}%s\n",
                s->ad, VARYANT_AD[s->varyant], s->grup, s->n_bayt, s->n_cerceve, s->ozet_al, s->ozet_coz,
                s->fnv_cerceve, s->fnv_yapi, i + 1 < ayr_n ? "," : "");
    }
    fprintf(fj, "  ],\n  \"kod\": [\n");
    for (unsigned i = 0; i < kod_n; ++i) {
        const kod_t *s = &kodlar[i];
        fprintf(fj, "    {\"ad\": \"%s\", \"varyant\": \"%s\", \"grup\": %d, \"bayt\": %zu, \"cerceve\": %zu, "
                    "\"fnv\": \"%08x\"}%s\n",
                s->ad, VARYANT_AD[s->varyant], s->grup, s->n_bayt, s->n_mesaj, s->fnv, i + 1 < kod_n ? "," : "");
    }
    fprintf(fj, "  ]\n}\n");
    fclose(fc);
    fclose(fj);
    fclose(fd);
    printf("mp5_uret: %u yapı, %u ayrıştırma akışı, %u kodlama akışı, veri kimliği %08x\n", havuz_n, ayr_n,
           kod_n, veri_kimlik);
    return 0;
}
