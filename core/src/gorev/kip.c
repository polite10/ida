/*
 * kip.c — kip makinesi: tam geçiş tablosu (02 §2.6; A4-K46, K127). Her kip × olay hücresi
 * tanımlıdır; tablo dışı (tanımsız kip ya da olay) silahsız ve DISARMED yönünde çözülür
 * (EMNİYET; W3-gorev "Emniyet ilkesi").
 *
 * Genişletme yönü yalnız GUIDED ve AUTO olaylarıdır ve yalnız komut yolundan (ida_gor_komut)
 * gelir; dağıtıcı onları o çağrıda daraltma varken buraya getirmez (A4-K45). Kip düşmeleri
 * (KONUM_YOK, GOREV_YOK) adımda, komutlardan önce uygulanır. DARALT_* olaylarını emniyet
 * modülü (b) adımında uygular; burada aynı sonucu veren satırlar belge ve çapraz sınama içindir
 * (INIT'te kip değişmez, A4-K127).
 */
#include "gorev/gorev_ic.h"

#define KAL IDA_GOR_KIP_KAL
#define RET IDA_GOR_KIP_RET
#define DIS IDA_MODE_DISARMED
#define MAN IDA_MODE_MANUAL
#define STB IDA_MODE_STABILIZE
#define DH  IDA_MODE_DEPTH_HOLD
#define SK  IDA_MODE_STATION_KEEP
#define GD  IDA_MODE_GUIDED
#define AT  IDA_MODE_AUTO

_Static_assert(IDA_MODE_AUTO + 1u == IDA_GOR_KIP_SAYI, "kip sayısı");
_Static_assert(IDA_GOR_OLAY_SAYI == 10, "olay sayısı (tablonun sütunları)");

/* Satır: kip (IDA_MODE_*). Sütun: olay (enum ida_gor_olay sırasıyla):
   HAZIR, GUIDED, AUTO, KONUM_YOK, GOREV_YOK, D_MANUAL, D_STABILIZE, D_DEPTH_HOLD,
   D_STATION_KEEP, D_GECERSIZ. */
static const uint8_t KIP_TABLOSU[IDA_GOR_KIP_SAYI][IDA_GOR_OLAY_SAYI] = {
    /* INIT: yalnız başlatma bitince çıkılır; genişletme yok; daraltma kipi değiştirmez. */
    {DIS, RET, RET, KAL, KAL, KAL, KAL, KAL, KAL, KAL},
    /* DISARMED */
    {KAL, GD, AT, KAL, KAL, MAN, STB, DH, SK, KAL},
    /* MANUAL */
    {KAL, GD, AT, KAL, KAL, MAN, STB, DH, SK, KAL},
    /* STABILIZE */
    {KAL, GD, AT, KAL, KAL, MAN, STB, DH, SK, KAL},
    /* DEPTH_HOLD */
    {KAL, GD, AT, KAL, KAL, MAN, STB, DH, SK, KAL},
    /* STATION_KEEP: konum yoksa DEPTH_HOLD'a iner (02 §2.6, §4.3 EST_DEGRADED satırı). */
    {KAL, GD, AT, DH, KAL, MAN, STB, DH, SK, KAL},
    /* GUIDED: kestirim bozulması kipi değil arıza eylemini değiştirir (02 §4.3: HOLD). */
    {KAL, GD, AT, KAL, KAL, MAN, STB, DH, SK, KAL},
    /* AUTO: yürüyen ya da duraklamış görev yoksa DEPTH_HOLD'a iner (02 §2.6; W5-gorev-yurutucu). */
    {KAL, GD, AT, KAL, DH, MAN, STB, DH, SK, KAL},
};

uint8_t ida_gor_kip_gecis(uint8_t kip, uint8_t olay)
{
    if ((kip >= IDA_GOR_KIP_SAYI) || (olay >= (uint8_t)IDA_GOR_OLAY_SAYI)) {
        return (uint8_t)IDA_GOR_KIP_SILAHSIZ; /* EMNİYET: tanımsız → silahsız */
    }
    return KIP_TABLOSU[kip][olay];
}

static void gunluk_yaz(ida_gunluk_t *gun, int64_t now_ns, uint16_t kod, int32_t a0, int32_t a1)
{
    ida_log_t k;
    k.t_w.ns = now_ns;
    k.code = kod;
    k.severity = 0u; /* ida_gun_ekle 3.15'e göre yazar */
    k.arg0 = a0;
    k.arg1 = a1;
    ida_gun_ekle(gun, &k);
}

uint8_t ida_gor_kip_uygula(ida_emn_t *yetki, uint8_t olay, ida_gunluk_t *gun, int64_t now_ns)
{
    const uint8_t h = ida_gor_kip_gecis(yetki->mode, olay);
    if (h == (uint8_t)IDA_GOR_KIP_SILAHSIZ) {
        ida_emn_kusak_artir(yetki); /* EMNİYET: saklı hedefler geçersiz */
        if (yetki->armed) {
            yetki->armed = false;
            gunluk_yaz(gun, now_ns, (uint16_t)IDA_LOG_ARM_CHANGE, 0,
                       (int32_t)IDA_REASON_NOT_ARMABLE);
        }
        if (yetki->mode != (uint8_t)IDA_MODE_DISARMED) {
            const uint8_t onceki = yetki->mode;
            yetki->mode = (uint8_t)IDA_MODE_DISARMED;
            gunluk_yaz(gun, now_ns, (uint16_t)IDA_LOG_MODE_CHANGE, (int32_t)IDA_MODE_DISARMED,
                       (int32_t)onceki);
        }
        return h;
    }
    if ((h < IDA_GOR_KIP_SAYI) && (h != yetki->mode)) {
        const uint8_t onceki = yetki->mode;
        yetki->mode = h;
        ida_emn_kusak_artir(yetki); /* EMNİYET: kip değişimi saklı hedefleri geçersiz kılar */
        gunluk_yaz(gun, now_ns, (uint16_t)IDA_LOG_MODE_CHANGE, (int32_t)h, (int32_t)onceki);
    }
    return h;
}
