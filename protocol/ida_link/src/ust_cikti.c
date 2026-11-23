/*
 * ust_cikti.c — çekirdek çıktısının A1 çerçevelerine çevrilmesi (A1 §4.6–§4.8): ilk kabul
 * kapısı ve olay tablosu (A4-K67, A1-K80, K120), olay yinelemesi ve sonuç payı (A1-K80,
 * A4-K106), IDA_ACTUATOR (A1-K44, K45, K71, K105; A4-K59), komut sonuçları, telemetri ve
 * çağrı izi (A1-K90–K95), gönderim sınıfları (A1-K110).
 */
#include "ust_ic.h"

#include <string.h>

#define KOVA_BIRIM     INT64_C(5000000)       /* bir jeton = 5 ms × hız birimi */
#define KOVA_ACAN_AZ   (4 * KOVA_BIRIM)       /* izin açan: patlama 4, 5 ms'de 4 */
#define KOVA_KAPAN_AZ  (3 * KOVA_BIRIM)       /* SAFETY_CLOSE: patlama 3, 5 ms'de 3 */

enum { OLAY_BOS = 0, OLAY_KOVA = 1, OLAY_GONDERILDI = 2, OLAY_SONUC = 3 };

static bool sonlu(float f)
{
    uint32_t b;
    memcpy(&b, &f, sizeof b);
    return (b & 0x7F800000u) != 0x7F800000u;
}

/* CRC-32/ISO-HDLC bayt tablosu (W4-param-hiz): UST_CRC32_TABLO[i], i baytının yansıtılmış
   0xEDB88320 (0x04C11DB7) çokterimiyle sekiz adımlık bit bit CRC'sidir; işaretçisiz salt okunur
   dizi (ROM, 1 024 B). Çekirdeğin core/src/param/crc32.c'deki tablosunun kopyasıdır: protokol
   modülü çekirdeğin iç başlıklarına ve arşivine bağlanmaz. İki tablonun aynı olduğunu
   protokol.birim.ust_crc testi 256 tek baytlık girdinin tamamında (her satır bir kez) ve bit bit
   referansla denetler. */
static const uint32_t UST_CRC32_TABLO[256] = {
    0x00000000u, 0x77073096u, 0xee0e612cu, 0x990951bau, 0x076dc419u, 0x706af48fu,
    0xe963a535u, 0x9e6495a3u, 0x0edb8832u, 0x79dcb8a4u, 0xe0d5e91eu, 0x97d2d988u,
    0x09b64c2bu, 0x7eb17cbdu, 0xe7b82d07u, 0x90bf1d91u, 0x1db71064u, 0x6ab020f2u,
    0xf3b97148u, 0x84be41deu, 0x1adad47du, 0x6ddde4ebu, 0xf4d4b551u, 0x83d385c7u,
    0x136c9856u, 0x646ba8c0u, 0xfd62f97au, 0x8a65c9ecu, 0x14015c4fu, 0x63066cd9u,
    0xfa0f3d63u, 0x8d080df5u, 0x3b6e20c8u, 0x4c69105eu, 0xd56041e4u, 0xa2677172u,
    0x3c03e4d1u, 0x4b04d447u, 0xd20d85fdu, 0xa50ab56bu, 0x35b5a8fau, 0x42b2986cu,
    0xdbbbc9d6u, 0xacbcf940u, 0x32d86ce3u, 0x45df5c75u, 0xdcd60dcfu, 0xabd13d59u,
    0x26d930acu, 0x51de003au, 0xc8d75180u, 0xbfd06116u, 0x21b4f4b5u, 0x56b3c423u,
    0xcfba9599u, 0xb8bda50fu, 0x2802b89eu, 0x5f058808u, 0xc60cd9b2u, 0xb10be924u,
    0x2f6f7c87u, 0x58684c11u, 0xc1611dabu, 0xb6662d3du, 0x76dc4190u, 0x01db7106u,
    0x98d220bcu, 0xefd5102au, 0x71b18589u, 0x06b6b51fu, 0x9fbfe4a5u, 0xe8b8d433u,
    0x7807c9a2u, 0x0f00f934u, 0x9609a88eu, 0xe10e9818u, 0x7f6a0dbbu, 0x086d3d2du,
    0x91646c97u, 0xe6635c01u, 0x6b6b51f4u, 0x1c6c6162u, 0x856530d8u, 0xf262004eu,
    0x6c0695edu, 0x1b01a57bu, 0x8208f4c1u, 0xf50fc457u, 0x65b0d9c6u, 0x12b7e950u,
    0x8bbeb8eau, 0xfcb9887cu, 0x62dd1ddfu, 0x15da2d49u, 0x8cd37cf3u, 0xfbd44c65u,
    0x4db26158u, 0x3ab551ceu, 0xa3bc0074u, 0xd4bb30e2u, 0x4adfa541u, 0x3dd895d7u,
    0xa4d1c46du, 0xd3d6f4fbu, 0x4369e96au, 0x346ed9fcu, 0xad678846u, 0xda60b8d0u,
    0x44042d73u, 0x33031de5u, 0xaa0a4c5fu, 0xdd0d7cc9u, 0x5005713cu, 0x270241aau,
    0xbe0b1010u, 0xc90c2086u, 0x5768b525u, 0x206f85b3u, 0xb966d409u, 0xce61e49fu,
    0x5edef90eu, 0x29d9c998u, 0xb0d09822u, 0xc7d7a8b4u, 0x59b33d17u, 0x2eb40d81u,
    0xb7bd5c3bu, 0xc0ba6cadu, 0xedb88320u, 0x9abfb3b6u, 0x03b6e20cu, 0x74b1d29au,
    0xead54739u, 0x9dd277afu, 0x04db2615u, 0x73dc1683u, 0xe3630b12u, 0x94643b84u,
    0x0d6d6a3eu, 0x7a6a5aa8u, 0xe40ecf0bu, 0x9309ff9du, 0x0a00ae27u, 0x7d079eb1u,
    0xf00f9344u, 0x8708a3d2u, 0x1e01f268u, 0x6906c2feu, 0xf762575du, 0x806567cbu,
    0x196c3671u, 0x6e6b06e7u, 0xfed41b76u, 0x89d32be0u, 0x10da7a5au, 0x67dd4accu,
    0xf9b9df6fu, 0x8ebeeff9u, 0x17b7be43u, 0x60b08ed5u, 0xd6d6a3e8u, 0xa1d1937eu,
    0x38d8c2c4u, 0x4fdff252u, 0xd1bb67f1u, 0xa6bc5767u, 0x3fb506ddu, 0x48b2364bu,
    0xd80d2bdau, 0xaf0a1b4cu, 0x36034af6u, 0x41047a60u, 0xdf60efc3u, 0xa867df55u,
    0x316e8eefu, 0x4669be79u, 0xcb61b38cu, 0xbc66831au, 0x256fd2a0u, 0x5268e236u,
    0xcc0c7795u, 0xbb0b4703u, 0x220216b9u, 0x5505262fu, 0xc5ba3bbeu, 0xb2bd0b28u,
    0x2bb45a92u, 0x5cb36a04u, 0xc2d7ffa7u, 0xb5d0cf31u, 0x2cd99e8bu, 0x5bdeae1du,
    0x9b64c2b0u, 0xec63f226u, 0x756aa39cu, 0x026d930au, 0x9c0906a9u, 0xeb0e363fu,
    0x72076785u, 0x05005713u, 0x95bf4a82u, 0xe2b87a14u, 0x7bb12baeu, 0x0cb61b38u,
    0x92d28e9bu, 0xe5d5be0du, 0x7cdcefb7u, 0x0bdbdf21u, 0x86d3d2d4u, 0xf1d4e242u,
    0x68ddb3f8u, 0x1fda836eu, 0x81be16cdu, 0xf6b9265bu, 0x6fb077e1u, 0x18b74777u,
    0x88085ae6u, 0xff0f6a70u, 0x66063bcau, 0x11010b5cu, 0x8f659effu, 0xf862ae69u,
    0x616bffd3u, 0x166ccf45u, 0xa00ae278u, 0xd70dd2eeu, 0x4e048354u, 0x3903b3c2u,
    0xa7672661u, 0xd06016f7u, 0x4969474du, 0x3e6e77dbu, 0xaed16a4au, 0xd9d65adcu,
    0x40df0b66u, 0x37d83bf0u, 0xa9bcae53u, 0xdebb9ec5u, 0x47b2cf7fu, 0x30b5ffe9u,
    0xbdbdf21cu, 0xcabac28au, 0x53b39330u, 0x24b4a3a6u, 0xbad03605u, 0xcdd70693u,
    0x54de5729u, 0x23d967bfu, 0xb3667a2eu, 0xc4614ab8u, 0x5d681b02u, 0x2a6f2b94u,
    0xb40bbe37u, 0xc30c8ea1u, 0x5a05df1bu, 0x2d02ef8du,
};

uint32_t ust_crc32(const uint8_t *p, size_t n)
{
    /* CRC-32/ISO-HDLC: yansıtılmış 0x04C11DB7, başlangıç ve son XOR 0xFFFFFFFF; bayt başına
       tablolu (tanım ve sonuç bit bit hesapla aynı). */
    uint32_t c = 0xFFFFFFFFu;
    for (size_t i = 0; i < n; ++i) {
        const uint8_t indis = (uint8_t)(c ^ (uint32_t)p[i]); /* 0–255: tablo sınırında */
        c = (c >> 8) ^ UST_CRC32_TABLO[indis];
    }
    return c ^ 0xFFFFFFFFu;
}

/* ------------------------------------------------------------------ */
/* Olay tablosu ve kovalar                                            */
/* ------------------------------------------------------------------ */

static void kova_guncelle(ida_link_ust_t *l, ida_wtime_t simdi)
{
    int64_t dt = simdi.ns - l->ozel_kova_zaman.ns;
    if (dt < 0) {
        dt = 0;
    }
    if (dt > KOVA_BIRIM) {
        dt = KOVA_BIRIM;   /* her iki kova bu sürede dolar; taşmayı önler */
    }
    l->ozel_kova_acan += dt * 4;
    if (l->ozel_kova_acan > KOVA_ACAN_AZ) {
        l->ozel_kova_acan = KOVA_ACAN_AZ;
    }
    l->ozel_kova_kapan += dt * 3;
    if (l->ozel_kova_kapan > KOVA_KAPAN_AZ) {
        l->ozel_kova_kapan = KOVA_KAPAN_AZ;
    }
    l->ozel_kova_zaman = simdi;
}

static void kapi_sonucu(ida_link_ust_t *l, uint64_t event_id, uint8_t sonuc)
{
    /* Bir put_out en çok 4 olay getirir ve take bunları ilk teslim eder; 8 yer yeter. */
    if (l->ozel_n_kapi >= IDA_LINK_UST_KAPI_SONUC_N) {
        return;
    }
    memset(&l->ozel_kapi[l->ozel_n_kapi], 0, sizeof l->ozel_kapi[0]);
    l->ozel_kapi[l->ozel_n_kapi].event_id = event_id;
    l->ozel_kapi[l->ozel_n_kapi].result = sonuc;
    l->ozel_n_kapi = (uint8_t)(l->ozel_n_kapi + 1u);
}

static bool olay_acan(uint8_t kind)
{
    return kind == IDA_EVT_SAFETY_OPEN || kind == IDA_EVT_FIRE;
}

static bool olay_gonder(ida_link_ust_t *l, const ida_link_ozel_olay_t *o, ida_link_sinif_t s)
{
    mavlink_ida_event_t m;
    ust_esl_event(&o->ev, o->call_kind, o->safety_rev, o->sess, &m);
    return ust_gonder(l, s, MAVLINK_MSG_ID_IDA_EVENT, &m, sizeof m);
}

void ust_olay_sonucu(ida_link_ust_t *l, const mavlink_ida_event_result_t *m)
{
    for (unsigned i = 0; i < IDA_LINK_UST_N_OLAY; ++i) {
        ida_link_ozel_olay_t *o = &l->ozel_olay[i];
        if ((o->durum == OLAY_KOVA || o->durum == OLAY_GONDERILDI) &&
            o->ev.event_id == m->event_id) {
            /* Sim yalnız 1–6 gönderir (A1 §3.7); tanımsız kod sonuç sayılmaz, olay bekler
               (A4-K69 ile aynı yön). */
            if (m->result < IDA_EVR_APPLIED || m->result > IDA_EVR_REJ_SESSION ||
                o->bek_sonuc != 0u) {
                return;
            }
            /* Sonuç bir parti üyesidir (A1-K28, K32; A4-K16): açık pencerede bekler,
               yinelemeyi durdurur; partinin olağan kapanışında etkinleşir, DUP/OLD
               penceresinde atılır (sol bulgu 4). */
            o->bek_sonuc = m->result;
            o->bek_t_apply_ns = m->t_apply_ns;
            return;
        }
    }
}

void ust_olay_pencere_kapandi(ida_link_ust_t *l)
{
    for (unsigned i = 0; i < IDA_LINK_UST_N_OLAY; ++i) {
        ida_link_ozel_olay_t *o = &l->ozel_olay[i];
        if ((o->durum == OLAY_KOVA || o->durum == OLAY_GONDERILDI) && o->bek_sonuc != 0u) {
            /* Etkinleşen sonuç teslim edilene kadar yuvada kalır; çağrı başına 4'ten fazlası
               sonraki çağrılara taşınır, atılmaz (A4-K32, K68). */
            o->durum = OLAY_SONUC;
            o->sonuc = o->bek_sonuc;
            o->t_apply_ns = o->bek_t_apply_ns;
            o->bek_sonuc = 0u;
        }
    }
}

void ust_olay_pencere_atildi(ida_link_ust_t *l)
{
    for (unsigned i = 0; i < IDA_LINK_UST_N_OLAY; ++i) {
        l->ozel_olay[i].bek_sonuc = 0u;   /* A1-K31: DUP/OLD penceresinin üyesi atılır */
    }
}

void ust_olay_oturum_sonu(ida_link_ust_t *l)
{
    for (unsigned i = 0; i < IDA_LINK_UST_N_OLAY; ++i) {
        ida_link_ozel_olay_t *o = &l->ozel_olay[i];
        if (o->durum == OLAY_KOVA || o->durum == OLAY_GONDERILDI) {
            o->durum = OLAY_SONUC;                        /* A4-K71 */
            o->sonuc = (uint8_t)IDA_EVR_SESSION_END;
            o->t_apply_ns = 0;
        }
        o->bek_sonuc = 0u;   /* açık pencere silinir (A1-K22) */
    }
}

void ust_olay_yokla(ida_link_ust_t *l, ida_wtime_t simdi)
{
    const bool running = l->ozel_durum == (uint8_t)IDA_LINK_UST_RUNNING;
    kova_guncelle(l, simdi);
    for (unsigned i = 0; i < IDA_LINK_UST_N_OLAY; ++i) {
        ida_link_ozel_olay_t *o = &l->ozel_olay[i];
        if (o->durum == OLAY_KOVA && running && l->ozel_kova_kapan >= KOVA_BIRIM) {
            /* A1-K120: kova izin verince gönderilir; n_events'e sayılmaz. */
            l->ozel_kova_kapan -= KOVA_BIRIM;
            o->durum = OLAY_GONDERILDI;
            o->son_gonderim = simdi;
            l->say[IDA_LINK_SAY_EVENTS_SENT] += 1u;
            (void)olay_gonder(l, o, IDA_LINK_SINIF_ZORUNLU);
            if (l->ozel_durum != (uint8_t)IDA_LINK_UST_RUNNING) {
                return;                                   /* TX_OVERFLOW */
            }
            continue;
        }
        if (o->durum == OLAY_GONDERILDI) {
            const int64_t son = o->cikis_w.ns + (int64_t)o->ev.apply_by_ns;
            if (simdi.ns >= son + IDA_LINK_UST_T_SONUC_PAY_NS) {
                /* A4-K106. Açık penceredeki etkinleşmemiş bir sonuç payı uzatmaz (EMNİYET). */
                o->durum = OLAY_SONUC;
                o->sonuc = (uint8_t)IDA_EVR_EXPIRED_NO_ACK;
                o->t_apply_ns = 0;
                o->bek_sonuc = 0u;
            } else if (running && o->bek_sonuc == 0u && simdi.ns < son &&
                       simdi.ns - o->son_gonderim.ns >= IDA_LINK_UST_T_OLAY_YIN_NS) {
                o->son_gonderim = simdi;                  /* A1-K80: aynı yükle yineleme */
                l->say[IDA_LINK_SAY_EVENTS_RESENT] += 1u;
                (void)olay_gonder(l, o, IDA_LINK_SINIF_ZORUNLU);
                if (l->ozel_durum != (uint8_t)IDA_LINK_UST_RUNNING) {
                    return;
                }
            }
        }
    }
}

uint8_t ust_olay_sonuclari_cek(ida_link_ust_t *l, ida_event_result_t *evres)
{
    uint8_t n = 0u;
    uint8_t k = 0u;
    while (n < IDA_PORT_MAX_EVRES && k < l->ozel_n_kapi) {
        evres[n] = l->ozel_kapi[k];
        n = (uint8_t)(n + 1u);
        k = (uint8_t)(k + 1u);
    }
    if (k > 0u) {
        memmove(&l->ozel_kapi[0], &l->ozel_kapi[k],
                (size_t)(l->ozel_n_kapi - k) * sizeof l->ozel_kapi[0]);
        l->ozel_n_kapi = (uint8_t)(l->ozel_n_kapi - k);
    }
    for (unsigned i = 0; i < IDA_LINK_UST_N_OLAY && n < IDA_PORT_MAX_EVRES; ++i) {
        ida_link_ozel_olay_t *o = &l->ozel_olay[i];
        if (o->durum != OLAY_SONUC) {
            continue;
        }
        memset(&evres[n], 0, sizeof evres[n]);
        evres[n].event_id = o->ev.event_id;
        evres[n].result = o->sonuc;
        evres[n].t_apply.ns = o->t_apply_ns;
        n = (uint8_t)(n + 1u);
        o->durum = OLAY_BOS;                              /* yuva sonuç teslim edilince boşalır */
    }
    return n;
}

typedef struct {
    ida_link_cagri_t tur;
    ida_rc_t rc;
    bool duzelt;
    ida_wtime_t now_w;
    ida_wtime_t simdi;
} cagri_bilgisi_t;

static bool oturumda(const ida_link_ust_t *l, ida_link_cagri_t tur)
{
    return l->ozel_cagri_kusak == l->ozel_kusak &&
           l->ozel_durum == (uint8_t)IDA_LINK_UST_RUNNING && tur != IDA_LINK_CAGRI_OTURUM_SONU;
}

/* A4-K67 ilk kabul kapısı. Dönüş: hemen gönderilecek yuvanın dizini, yoksa -1. */
static int olay_kapisi(ida_link_ust_t *l, const ida_event_t *ev, const ida_out_t *out,
                       const cagri_bilgisi_t *c)
{
    const bool acan = olay_acan(ev->kind);
    const bool kapan = ev->kind == IDA_EVT_SAFETY_CLOSE;
    const uint32_t ctr = (uint32_t)(ev->event_id & 0xFFFFFFFFu);
    int bos = -1;
    unsigned n_acan = 0u;
    ida_link_ozel_olay_t *o;
    /* (1) A4-K64'ün koşulları çıktıyla birlikte; A4-K59 düzeltmesinde hiçbiri geçmez. */
    if (c->duzelt || (!acan && !kapan)) {
        kapi_sonucu(l, ev->event_id, (uint8_t)IDA_EVR_DROPPED_GATE);
        return -1;
    }
    if (acan && (c->tur != IDA_LINK_CAGRI_STEP || c->rc != IDA_RC_OK ||
                 ev->tube >= IDA_PORT_N_TUBE ||
                 (ev->kind == IDA_EVT_FIRE && !(out->armed && !out->is_safe)))) {
        kapi_sonucu(l, ev->event_id, (uint8_t)IDA_EVR_DROPPED_GATE);
        return -1;
    }
    if (kapan && ev->tube >= IDA_PORT_N_TUBE && ev->tube != IDA_TUBE_ALL) {
        kapi_sonucu(l, ev->event_id, (uint8_t)IDA_EVR_DROPPED_GATE);
        return -1;
    }
    /* (2) A1 oturumu RUNNING. */
    if (!oturumda(l, c->tur)) {
        kapi_sonucu(l, ev->event_id, (uint8_t)IDA_EVR_SESSION_END);
        return -1;
    }
    /* (3) Kimlik: üst 32 bit boot_count, alt 32 bit öncekilerden büyük. */
    if ((uint32_t)(ev->event_id >> 32) != l->ozel_ayar.boot_count ||
        (l->ozel_olay_ctr_var && ctr <= l->ozel_olay_ctr)) {
        kapi_sonucu(l, ev->event_id, (uint8_t)IDA_EVR_DROPPED_GATE);
        return -1;
    }
    for (unsigned i = 0; i < IDA_LINK_UST_N_OLAY; ++i) {
        if (l->ozel_olay[i].durum == OLAY_BOS) {
            if (bos < 0) {
                bos = (int)i;
            }
        } else if (olay_acan(l->ozel_olay[i].ev.kind)) {
            n_acan += 1u;
        }
    }
    if (acan) {
        /* (4) İzin açan: yedi yuva ve kova (A1-K80, K120). */
        if (bos < 0 || n_acan >= IDA_LINK_UST_N_OLAY_ACAN || l->ozel_kova_acan < KOVA_BIRIM) {
            kapi_sonucu(l, ev->event_id, (uint8_t)IDA_EVR_DROPPED_NO_SLOT);
            return -1;
        }
        l->ozel_kova_acan -= KOVA_BIRIM;
    } else if (bos < 0) {
        /* Sekiz yuva doluyken SAFETY_CLOSE: atılmaz, oturum biter (A1-K80; EVENT_OVERFLOW). */
        ust_oturum_bitir(l, (uint8_t)IDAW_CLOSE_REASON_EVENT_OVERFLOW);
        kapi_sonucu(l, ev->event_id, (uint8_t)IDA_EVR_SESSION_END);
        return -1;
    }
    o = &l->ozel_olay[bos];
    memset(o, 0, sizeof *o);
    o->ev = *ev;
    o->call_kind = (uint8_t)c->tur;
    o->sess = l->ozel_sess;
    o->safety_rev = acan ? l->ozel_emn_rev : 0u;          /* A1-K106 */
    o->cikis_w = c->now_w;
    o->son_gonderim = c->simdi;
    l->ozel_olay_ctr = ctr;
    l->ozel_olay_ctr_var = 1u;
    if (kapan) {
        if (l->ozel_kova_kapan < KOVA_BIRIM) {
            o->durum = OLAY_KOVA;                         /* kova boş: tabloda bekler */
            return -1;
        }
        l->ozel_kova_kapan -= KOVA_BIRIM;
    }
    o->durum = OLAY_GONDERILDI;
    return bos;
}

/* ------------------------------------------------------------------ */
/* Çıktı çerçeveleri                                                  */
/* ------------------------------------------------------------------ */

static uint32_t yas_ns(ida_wtime_t simdi, ida_wtime_t t)
{
    /* Taşma güvenli fark: simdi > t iken gerçek fark 2^64'ten küçüktür ve işaretsiz
       aritmetikte doğru çıkar (sol bulgu 5; girdi alanları platformca değiştirilebilir). */
    uint64_t d;
    if (simdi.ns <= t.ns) {
        return 0u;
    }
    d = (uint64_t)simdi.ns - (uint64_t)t.ns;
    return d > UINT64_C(0xFFFFFFFF) ? 0xFFFFFFFFu : (uint32_t)d;
}

static uint32_t cagri_batch_seq(const ida_link_ust_t *l, const ida_in_t *in, ida_link_cagri_t tur)
{
    if (tur == IDA_LINK_CAGRI_STEP || tur == IDA_LINK_CAGRI_TICK_TF) {
        return in->tf.seq;                                /* P2-4: in.tf.seq, out.seq değil */
    }
    return l->ozel_tf_alindi ? l->ozel_son_kapanan : 0u;  /* bu oturumda kapanan son parti */
}

static void actuator_gonder(ida_link_ust_t *l, const ida_in_t *in, const ida_out_t *out,
                            const cagri_bilgisi_t *c, uint8_t n_yeni)
{
    mavlink_ida_actuator_t m;
    const bool thr_valid = out->thr_valid || c->duzelt;
    const bool is_safe = out->is_safe || c->duzelt;      /* A4-K59: nötr güvenli çıktı */
    uint16_t f;
    if (c->tur == IDA_LINK_CAGRI_IZGARA && !(thr_valid && is_safe)) {
        return;   /* A1-K44: ızgara çıktısı yalnız THR_VALID ve IS_SAFE ile (EMNİYET) */
    }
    memset(&m, 0, sizeof m);
    f = (uint16_t)(thr_valid ? IDAW_ACT_FLAGS_THR_VALID : 0);
    f = (uint16_t)(f | (is_safe ? IDAW_ACT_FLAGS_IS_SAFE : 0));
    f = (uint16_t)(f | (out->armed ? IDAW_ACT_FLAGS_ARMED : 0));
    f = (uint16_t)(f | (out->src == IDA_SRC_TICK ? IDAW_ACT_FLAGS_SRC_TICK : 0));
    f = (uint16_t)(f | (c->tur == IDA_LINK_CAGRI_IZGARA ? IDAW_ACT_FLAGS_ASYNC : 0));
    f = (uint16_t)(f | (out->health.input_dropped ? IDAW_ACT_FLAGS_INPUT_DROPPED : 0));
    f = (uint16_t)(f | (out->health.narrowed ? IDAW_ACT_FLAGS_NARROWED : 0));
    f = (uint16_t)(f | (c->duzelt ? IDAW_ACT_FLAGS_OUT_FIXED : 0));
    f = (uint16_t)(f | (l->ozel_cagri_eksik ? IDAW_ACT_FLAGS_BATCH_INCOMPLETE : 0));
    f = (uint16_t)(f | (l->ozel_cagri_cakisma ? IDAW_ACT_FLAGS_CALL_CONFLICT : 0));
    m.act_flags = f;
    m.batch_seq = cagri_batch_seq(l, in, c->tur);
    if (thr_valid) {
        m.valid_for_ns = out->valid_for_ns;
        for (unsigned i = 0; i < IDA_PORT_N_THR; ++i) {
            /* IS_SAFE ⇒ her pwm_us nötr (Tablo 4.6 V4; Okumalar, EMNİYET). */
            m.pwm_us[i] = is_safe ? l->ozel_ayar.esc_notr_us
                                  : ust_esl_pwm(out->thr_cmd[i], l->ozel_ayar.esc_alt_us,
                                                l->ozel_ayar.esc_notr_us, l->ozel_ayar.esc_ust_us);
        }
    }
    m.sess = l->ozel_sess;
    if (c->tur != IDA_LINK_CAGRI_IZGARA) {
        const uint32_t y = yas_ns(c->simdi, in->tf.t_rx) / 1000u;
        m.rx_to_tx_us = (uint16_t)(y > 0xFFFFu ? 0xFFFFu : y);
    }
    m.fs_level = out->fs_level;
    m.core_rc = (uint8_t)c->rc;
    m.n_events = n_yeni;
    m.n_merged = l->ozel_cagri_birlesen;
    l->ozel_out_seq += 1u;                                /* A1-K105 */
    m.out_seq = l->ozel_out_seq;
    (void)ust_gonder(l, IDA_LINK_SINIF_KRITIK, MAVLINK_MSG_ID_IDA_ACTUATOR, &m, sizeof m);
}

static void cagri_izi_gonder(ida_link_ust_t *l, const ida_in_t *in, const cagri_bilgisi_t *c)
{
    mavlink_ida_call_trace_t m;
    uint8_t n_rx = 0u;
    uint8_t pf;
    memset(&m, 0, sizeof m);
    m.now_w_ns = in->now_w.ns;
    m.batch_seq = cagri_batch_seq(l, in, c->tur);
    m.bg_slices = in->bg_slices;
    m.prev_call_ns = in->plat.prev_call_ns;
    /* rx_age_ns: 0 tf, 1 imu, 2–17 samples, 18–19 det_batch, 20–27 det (A1 §3.7). */
    if (in->tf.present) {
        m.rx_age_ns[0] = yas_ns(in->now_w, in->tf.t_rx);
        n_rx = 1u;
    }
    if (in->imu_present) {
        m.rx_age_ns[1] = yas_ns(in->now_w, in->imu.hdr.t_rx);
        n_rx = 2u;
    }
    for (unsigned i = 0; i < in->n_samples && i < IDA_PORT_MAX_SAMPLES; ++i) {
        m.rx_age_ns[2u + i] = yas_ns(in->now_w, in->samples[i].hdr.t_rx);
        n_rx = (uint8_t)(3u + i);
    }
    for (unsigned i = 0; i < in->n_det_batch && i < IDA_PORT_MAX_DET_BATCH; ++i) {
        m.rx_age_ns[18u + i] = yas_ns(in->now_w, in->det_batch_t_rx[i]);
        n_rx = (uint8_t)(19u + i);
    }
    for (unsigned i = 0; i < in->n_det && i < IDA_PORT_MAX_DET_IN; ++i) {
        m.rx_age_ns[20u + i] = yas_ns(in->now_w, in->det_t_rx[i]);
        n_rx = (uint8_t)(21u + i);
    }
    m.n_rx = n_rx;
    m.call_kind = (uint8_t)c->tur;
    m.core_rc = (uint8_t)c->rc;
    pf = (uint8_t)(in->plat.prev_overrun ? IDAW_PLAT_FLAGS_PREV_OVERRUN : 0);
    pf = (uint8_t)(pf | (in->plat.tx_stalled ? IDAW_PLAT_FLAGS_TX_STALLED : 0));
    pf = (uint8_t)(pf | (in->plat.batch_merged ? IDAW_PLAT_FLAGS_BATCH_MERGED : 0));
    pf = (uint8_t)(pf | (in->plat.input_dropped ? IDAW_PLAT_FLAGS_INPUT_DROPPED : 0));
    pf = (uint8_t)(pf | (in->plat.link_event == IDA_LINKEV_SESSION_START
                             ? IDAW_PLAT_FLAGS_SESSION_START : 0));
    pf = (uint8_t)(pf | (in->plat.link_event == IDA_LINKEV_SESSION_END
                             ? IDAW_PLAT_FLAGS_SESSION_END : 0));
    m.plat_flags = pf;
    m.n_merged = l->ozel_cagri_birlesen;
    (void)ust_gonder(l, IDA_LINK_SINIF_KAYBEDILEBILIR, MAVLINK_MSG_ID_IDA_CALL_TRACE, &m,
                     sizeof m);
}

static void sonuclari_gonder(ida_link_ust_t *l, const ida_out_t *out)
{
    const unsigned n = out->n_results < IDA_PORT_MAX_RESULTS ? out->n_results
                                                             : IDA_PORT_MAX_RESULTS;
    for (unsigned i = 0; i < n; ++i) {
        const ida_cmd_result_t *r = &out->results[i];
        mavlink_ida_cmd_result_t m;
        uint32_t crc = 0u;
        if (r->kind == IDA_RES_PARAM_SAVE && r->ack == IDA_ACK_ACCEPTED && r->save.blob != NULL &&
            r->save.len <= IDA_PARAM_BLOB_MAX) {
            crc = ust_crc32(r->save.blob, r->save.len);   /* A1-K68 save_crc */
        }
        ust_esl_cmd_result(r, l->ozel_sess, crc, &m);
        if (!ust_gonder(l, IDA_LINK_SINIF_ZORUNLU, MAVLINK_MSG_ID_IDA_CMD_RESULT, &m, sizeof m) &&
            l->ozel_durum != (uint8_t)IDA_LINK_UST_RUNNING) {
            return;
        }
    }
}

static void telemetri_gonder(ida_link_ust_t *l, const ida_in_t *in, const ida_out_t *out,
                             const cagri_bilgisi_t *c)
{
    /* A1-K91: STATUS ve FC_STATUS 100 ms'de bir son çıktıdan. */
    if (!l->ozel_durum_var || c->simdi.ns - l->ozel_son_durum.ns >= IDA_LINK_UST_T_DURUM_NS) {
        mavlink_ida_status_t st;
        mavlink_ida_fc_status_t fc;
        uint32_t crc = 0u;
        uint32_t len = 0u;
        uint32_t gen = 0u;
        l->ozel_plat.kalici_blok(l->ozel_plat.ctx, &crc, &len, &gen);
        ust_esl_status(&out->status, &out->health, &in->plat, (uint8_t)IDAW_LINK_STATE_RUNNING,
                       crc, out->seq, l->ozel_sess, &st);
        (void)ust_gonder(l, IDA_LINK_SINIF_KAYBEDILEBILIR, MAVLINK_MSG_ID_IDA_STATUS, &st,
                         sizeof st);
        ust_esl_fc_status(&out->fc, &fc);
        (void)ust_gonder(l, IDA_LINK_SINIF_KAYBEDILEBILIR, MAVLINK_MSG_ID_IDA_FC_STATUS, &fc,
                         sizeof fc);
        l->ozel_son_durum = c->simdi;
        l->ozel_durum_var = 1u;
    }
    cagri_izi_gonder(l, in, c);                           /* A1-K95 */
    if (c->tur == IDA_LINK_CAGRI_STEP && c->rc == IDA_RC_OK && out->nav_valid &&
        (in->tf.seq % 4u) == 0u) {
        mavlink_ida_nav_t nav;                            /* A1-K91: 50 Hz */
        ust_esl_nav(&out->nav, in->tf.seq, &nav);
        (void)ust_gonder(l, IDA_LINK_SINIF_KAYBEDILEBILIR, MAVLINK_MSG_ID_IDA_NAV, &nav,
                         sizeof nav);
    }
    for (unsigned i = 0; i < out->n_logs && i < IDA_PORT_MAX_LOGS; ++i) {
        mavlink_ida_log_t g;                              /* A1-K93 */
        ust_esl_log(&out->logs[i], &g);
        (void)ust_gonder(l, IDA_LINK_SINIF_KAYBEDILEBILIR, MAVLINK_MSG_ID_IDA_LOG, &g, sizeof g);
    }
    if (!l->ozel_sayac_var || c->simdi.ns - l->ozel_son_sayac.ns >= IDA_LINK_UST_T_SAYAC_NS) {
        mavlink_ida_core_counters_t cc;
        mavlink_ida_link_stats_t ls;
        ust_esl_core_counters(&out->status.cnt, &cc);
        (void)ust_gonder(l, IDA_LINK_SINIF_KAYBEDILEBILIR, MAVLINK_MSG_ID_IDA_CORE_COUNTERS, &cc,
                         sizeof cc);
        memset(&ls, 0, sizeof ls);
        ida_link_ust_sayaclar(l, ls.cnt);
        (void)ust_gonder(l, IDA_LINK_SINIF_KAYBEDILEBILIR, MAVLINK_MSG_ID_IDA_LINK_STATS, &ls,
                         sizeof ls);
        l->ozel_son_sayac = c->simdi;
        l->ozel_sayac_var = 1u;
    }
    for (unsigned i = 0; i < out->n_det_tx && i < IDA_PORT_MAX_DET_OUT; ++i) {
        mavlink_ida_det_record_t d;                       /* A1-K94 */
        ust_esl_det_tx(&out->det_tx[i], &d);
        (void)ust_gonder(l, IDA_LINK_SINIF_KAYBEDILEBILIR, MAVLINK_MSG_ID_IDA_DET_RECORD, &d,
                         sizeof d);
    }
}

/* ------------------------------------------------------------------ */
/* put_out                                                            */
/* ------------------------------------------------------------------ */

void ida_link_ust_put_out(ida_link_ust_t *l, const ida_in_t *in, ida_rc_t rc,
                          const ida_out_t *out)
{
    cagri_bilgisi_t c;
    int gonderilecek[IDA_PORT_MAX_EVENTS];
    uint8_t n_gon = 0u;
    if (l == NULL || !l->ozel_kurulu || !l->ozel_cagri_suruyor) {
        return;
    }
    memset(&c, 0, sizeof c);
    c.tur = (ida_link_cagri_t)l->ozel_cagri_tur;
    c.rc = rc;
    c.simdi = ust_simdi(l);
    /* Olay zamanlayıcısının tabanı çağrının now_w'sidir (A1-K80); A4-K08 aralığı dışındaysa
       (ör. SIL'de bozuk TF zamanı; çekirdek BAD_INPUT döner) geçersiz zaman zamanlayıcıya
       taşınmaz, çağrının başladığı platform saati kullanılır (sol bulgu 5). */
    c.now_w = l->ozel_cagri_bas;
    if (in != NULL && in->now_w.ns >= 0 && in->now_w.ns <= IDA_TIME_MAX_NS) {
        c.now_w = in->now_w;
    }
    l->ozel_cagri_suruyor = 0u;
    l->ozel_son_cagri_bas = l->ozel_cagri_bas;
    l->ozel_son_cagri_bitis = c.simdi;
    l->ozel_son_cagri_var = 1u;
    kova_guncelle(l, c.simdi);

    if (out != NULL && out->thr_valid) {
        for (unsigned i = 0; i < IDA_PORT_N_THR; ++i) {
            const float t = out->thr_cmd[i];
            if (!sonlu(t) || t > 1.0f || t < -1.0f) {
                c.duzelt = true;                          /* A4-K59 */
            }
        }
        if (c.duzelt) {
            l->say[IDA_LINK_SAY_OUT_FIXED] += 1u;
        }
    }
    if (out != NULL) {
        const unsigned n_ev = out->n_events < IDA_PORT_MAX_EVENTS ? out->n_events
                                                                   : IDA_PORT_MAX_EVENTS;
        for (unsigned i = 0; i < n_ev; ++i) {
            const int y = olay_kapisi(l, &out->events[i], out, &c);
            if (y >= 0) {
                gonderilecek[n_gon] = y;
                n_gon = (uint8_t)(n_gon + 1u);
            }
        }
    }
    if (out != NULL && in != NULL && oturumda(l, c.tur)) {
        /* A1-K90: yeni olaylar, IDA_ACTUATOR, (ii) kalanı, (iii). */
        for (unsigned i = 0; i < n_gon; ++i) {
            l->say[IDA_LINK_SAY_EVENTS_SENT] += 1u;
            (void)olay_gonder(l, &l->ozel_olay[gonderilecek[i]], IDA_LINK_SINIF_KRITIK);
        }
        if (c.tur != IDA_LINK_CAGRI_OTURUM_SONU) {
            actuator_gonder(l, in, out, &c, n_gon);
        }
        sonuclari_gonder(l, out);
        if (oturumda(l, c.tur)) {
            telemetri_gonder(l, in, out, &c);
        }
        return;
    }
    if (c.tur == IDA_LINK_CAGRI_OTURUM_SONU) {
        if (in != NULL) {
            cagri_izi_gonder(l, in, &c);                  /* A1-K22 (3): en iyi çabayla */
        }
        ust_oturum_sonu_dondu(l);
    }
    /* Çağrı sürerken oturum bitti: çıktı gönderilmez; olaylar SESSION_END aldı (A4-K71). */
}
