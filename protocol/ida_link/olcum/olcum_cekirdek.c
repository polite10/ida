/* olcum_cekirdek.c — ölçüm çekirdek döngüsü (olcum_cekirdek.h). Host'a özgü kod içermez. */
#include "olcum_cekirdek.h"
#include "tel_eski.h"

#include "ida_link_altin_veri.h"
#include "mavlink/ida/mavlink.h"

#include <string.h>

static const uint32_t PENCERE_KIMLIKLERI[IDA_LINK_OLCUM_CERCEVE_SAYISI] = {
    46010u, 46020u, 46021u, 46022u, 46023u, 46024u, 46025u, 46026u, 46027u,
    46028u, 46029u, 46030u, 46030u, 46035u, 46036u, 46040u, 46003u,
};
#define OLCUM_PING360_DATA 46030u
#define OLCUM_BUYUK_SIRA 11u /* PENCERE_KIMLIKLERI'nde ilk PING360_DATA */

static const ida_link_altin_t *altin(uint32_t msgid)
{
    for (unsigned i = 0; i < IDA_LINK_ALTIN_SAYISI; ++i) {
        if (ida_link_altin[i].msgid == msgid && !ida_link_altin[i].ek_vaka) {
            return &ida_link_altin[i];
        }
    }
    return NULL;
}

/* Altın çerçeveyi çözüp yapıya yazar. */
static int mesaj_kur(ida_link_olcum_t *o, ida_link_olcum_mesaj_t *m, uint32_t msgid)
{
    const ida_link_altin_t *g = altin(msgid);
    ida_link_mesaj_bilgisi_t b;
    size_t tk = 0;
    if (g == NULL || ida_link_mesaj_bilgisi(msgid, &b) != IDA_LINK_TAMAM ||
        b.yapi_boyu > IDA_LINK_OLCUM_YAPI_AZAMI ||
        ida_link_alici_kur(&o->alici, g->sysid == IDA_LINK_SIM_SYSID ? IDA_LINK_ROL_KART
                                                                     : IDA_LINK_ROL_SIM) !=
            IDA_LINK_TAMAM ||
        ida_link_al(&o->alici, g->bayt, g->cerceve, &tk, &o->cerceve) != IDA_LINK_CERCEVE ||
        ida_link_coz(&o->cerceve, msgid, m->yapi.b, b.yapi_boyu) != IDA_LINK_TAMAM) {
        return -1;
    }
    m->msgid = msgid;
    m->yapi_boyu = b.yapi_boyu;
    if (msgid == OLCUM_PING360_DATA) {
        /* Tam dolu sonar parçası: 240 örnek, son bayt sıfır değil (kırpılmaz). */
        mavlink_ida_ping360_data_t p;
        memcpy(&p, m->yapi.b, sizeof p);
        for (size_t i = 0; i < sizeof p.data; ++i) {
            p.data[i] = (uint8_t)(1u + (i * 7u) % 250u);
        }
        memcpy(m->yapi.b, &p, sizeof p);
    }
    return 0;
}

int ida_link_olcum_hazirla(ida_link_olcum_t *o)
{
    if (o == NULL) {
        return -1;
    }
    memset(o, 0, sizeof *o);
    if (ida_link_gonderici_kur(&o->gonderici, IDA_LINK_ROL_SIM) != IDA_LINK_TAMAM) {
        return -2;
    }
    size_t n = 0;
    for (unsigned i = 0; i < IDA_LINK_OLCUM_CERCEVE_SAYISI; ++i) {
        if (mesaj_kur(o, &o->mesaj[i], PENCERE_KIMLIKLERI[i]) != 0) {
            return -3;
        }
        size_t yz = 0;
        if (ida_link_gonder(&o->gonderici, o->mesaj[i].msgid, o->mesaj[i].yapi.b,
                            o->mesaj[i].yapi_boyu, &o->pencere[n],
                            IDA_LINK_OLCUM_PENCERE_AZAMI - n, &yz) != IDA_LINK_TAMAM) {
            return -4;
        }
        n += yz;
    }
    o->pencere_bayt = n;
    const ida_link_olcum_mesaj_t *b = &o->mesaj[OLCUM_BUYUK_SIRA];
    size_t yz = 0;
    if (ida_link_gonder(&o->gonderici, b->msgid, b->yapi.b, b->yapi_boyu, o->buyuk,
                        sizeof o->buyuk, &yz) != IDA_LINK_TAMAM ||
        yz != IDA_LINK_EK_BOY + IDA_LINK_DIALECT_YUK_AZAMI) {
        return -5;
    }
    o->buyuk_bayt = yz;
    return ida_link_alici_kur(&o->alici, IDA_LINK_ROL_KART) == IDA_LINK_TAMAM ? 0 : -6;
}

/* Ayrıştırma döngüsü; iki uygulama için ayrı ayrı açılır (dolaylı çağrı olmasın: ölçülen
   maliyet yalnız kitaplığınki kalsın). parca 0 ise kalan akışın tamamı verilir. */
#define OLCUM_AYRISTIR_TANIMLA(ad, AL, COZ, BILGI)                                           \
    static uint32_t ad(ida_link_olcum_t *o, const uint8_t *v, size_t n, size_t parca,        \
                       int coz, uint32_t yineleme, uint32_t beklenen)                        \
    {                                                                                        \
        uint32_t ozet = 0;                                                                   \
        uint32_t teslim = 0;                                                                 \
        for (uint32_t k = 0; k < yineleme; ++k) {                                            \
            size_t i = 0;                                                                    \
            while (i < n) {                                                                  \
                size_t tk = 0;                                                               \
                const size_t m = (parca == 0u || parca > n - i) ? n - i : parca;             \
                if (AL(&o->alici, &v[i], m, &tk, &o->cerceve) == IDA_LINK_CERCEVE) {         \
                    ozet += o->cerceve.msgid + o->cerceve.yuk[0];                            \
                    teslim++;                                                                \
                    if (coz) {                                                               \
                        ida_link_mesaj_bilgisi_t b;                                          \
                        if (BILGI(o->cerceve.msgid, &b) == IDA_LINK_TAMAM &&                    \
                            b.yapi_boyu <= sizeof o->cozulen.b &&                            \
                            COZ(&o->cerceve, o->cerceve.msgid, o->cozulen.b, b.yapi_boyu) == \
                                IDA_LINK_TAMAM) {                                            \
                            ozet += o->cozulen.b[b.yapi_boyu - 1u];                          \
                        } else {                                                             \
                            return 0u;                                                       \
                        }                                                                    \
                    }                                                                        \
                }                                                                            \
                i += tk;                                                                     \
            }                                                                                \
        }                                                                                    \
        return teslim == beklenen * yineleme ? (ozet | 1u) : 0u;                             \
    }

OLCUM_AYRISTIR_TANIMLA(ayristir_yeni, ida_link_al, ida_link_coz, ida_link_mesaj_bilgisi)
OLCUM_AYRISTIR_TANIMLA(ayristir_eski, ida_link_eski_al, ida_link_eski_coz,
                       ida_link_eski_mesaj_bilgisi)

/* Kodlama döngüsü: pencerenin bütün çerçeveleri (ilk..son) ya da tek çerçeve. */
#define OLCUM_KODLA_TANIMLA(ad, GONDER)                                                      \
    static uint32_t ad(ida_link_olcum_t *o, unsigned ilk, unsigned son, uint32_t yineleme)   \
    {                                                                                        \
        uint32_t ozet = 0;                                                                   \
        for (uint32_t k = 0; k < yineleme; ++k) {                                            \
            size_t n = 0;                                                                    \
            for (unsigned i = ilk; i < son; ++i) {                                           \
                size_t yz = 0;                                                               \
                (void)GONDER(&o->gonderici, o->mesaj[i].msgid, o->mesaj[i].yapi.b,           \
                             o->mesaj[i].yapi_boyu, &o->cikis[n],                            \
                             IDA_LINK_OLCUM_PENCERE_AZAMI - n, &yz);                         \
                n += yz;                                                                     \
            }                                                                                \
            ozet += (uint32_t)n + (n > 0u ? o->cikis[n - 1u] : 0u);                          \
        }                                                                                    \
        return ozet;                                                                         \
    }

OLCUM_KODLA_TANIMLA(kodla_yeni, ida_link_gonder)
OLCUM_KODLA_TANIMLA(kodla_eski, ida_link_eski_gonder)

uint32_t ida_link_olcum_ayristir(ida_link_olcum_t *o, ida_link_olcum_uygulama_t u,
                                 ida_link_olcum_akis_t akis, size_t parca, int coz,
                                 uint32_t yineleme)
{
    const int buyuk = akis == IDA_LINK_OLCUM_BUYUK;
    const uint8_t *v = buyuk ? o->buyuk : o->pencere;
    const size_t n = buyuk ? o->buyuk_bayt : o->pencere_bayt;
    const uint32_t beklenen = buyuk ? 1u : IDA_LINK_OLCUM_CERCEVE_SAYISI;
    return u == IDA_LINK_OLCUM_ESKI ? ayristir_eski(o, v, n, parca, coz, yineleme, beklenen)
                                    : ayristir_yeni(o, v, n, parca, coz, yineleme, beklenen);
}

uint32_t ida_link_olcum_kodla(ida_link_olcum_t *o, ida_link_olcum_uygulama_t u,
                              ida_link_olcum_akis_t akis, uint32_t yineleme)
{
    const int buyuk = akis == IDA_LINK_OLCUM_BUYUK;
    const unsigned ilk = buyuk ? OLCUM_BUYUK_SIRA : 0u;
    const unsigned son = buyuk ? OLCUM_BUYUK_SIRA + 1u : IDA_LINK_OLCUM_CERCEVE_SAYISI;
    return u == IDA_LINK_OLCUM_ESKI ? kodla_eski(o, ilk, son, yineleme)
                                    : kodla_yeni(o, ilk, son, yineleme);
}

uint32_t ida_link_olcum_pencere_ayristir(ida_link_olcum_t *o, uint32_t yineleme)
{
    return ida_link_olcum_ayristir(o, IDA_LINK_OLCUM_YENI, IDA_LINK_OLCUM_PENCERE, 0u, 0,
                                   yineleme);
}

uint32_t ida_link_olcum_buyuk_ayristir(ida_link_olcum_t *o, uint32_t yineleme)
{
    return ida_link_olcum_ayristir(o, IDA_LINK_OLCUM_YENI, IDA_LINK_OLCUM_BUYUK, 0u, 0,
                                   yineleme);
}

uint32_t ida_link_olcum_pencere_kodla(ida_link_olcum_t *o, uint32_t yineleme)
{
    return ida_link_olcum_kodla(o, IDA_LINK_OLCUM_YENI, IDA_LINK_OLCUM_PENCERE, yineleme);
}

uint32_t ida_link_olcum_buyuk_kodla(ida_link_olcum_t *o, uint32_t yineleme)
{
    return ida_link_olcum_kodla(o, IDA_LINK_OLCUM_YENI, IDA_LINK_OLCUM_BUYUK, yineleme);
}
