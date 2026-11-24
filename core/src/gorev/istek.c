/*
 * istek.c — etkin kipe ve arıza eylemine göre kontrol isteği (W3-gorev md.4; 02 §2.6, §4.2;
 * mimar kararları W3-K1, K4, K5).
 *
 * Yapı kontrol modülünün ida_kon_istek_t'sidir (kontrol/kontrol.h). İşlev durumun saf
 * işlevidir; komutlardan sonra (kontrol adımında, güdümün ida_gud_kon_istegi'nden) çağrılır ki
 * aynı çağrıdaki ARM ya da SET_MODE görülsün. GUIDED kipinde güdüm, GUIDED_SP'nin maskeli
 * kanallarını bu isteğin üzerine yazar (gudum.c).
 *
 * KAPALI (itki sıfır): silahsız; kip INIT, DISARMED ("motorlar nötr", 02 §2.6) ya da tanımsız;
 * arıza eylemi PASSIVE_SURFACE ve üstü (A4-K55). Kestirim düzeyine bağlı sıfırlama kontrol
 * modülündedir (W3-K4). Sanal emniyet girdileri itkiye girmez (W3-K1).
 *
 * Kanal seçimi bileşen geçerliliğine bağlıdır (W3-K5): bir hedefin bileşeni (derinlik, yön,
 * tutum, konum) geçersizse o kanal sönümlü ve sıfır hedefli kipe iner (açısal hız 0, dikey
 * kuvvet 0: araç pozitif yüzer; yatay kuvvet 0). Hedefler bu modülün yakaladığı tutma
 * değerleridir (silahlanma, kip ve arıza düzeyi değişiminde; gorev.c).
 */
#include <string.h>

#include "gorev/gorev_ic.h"

/* Dikey kanal: derinliği tut (bileşen ve hedef geçerliyse), değilse kuvvet 0. */
static void dikey_tut(const ida_gor_t *g, ida_kon_istek_t *ist)
{
    if (g->nav_derinlik_var && g->tut_derinlik_var) {
        ist->dikey = (uint8_t)IDA_KON_DIKEY_DERINLIK;
        ist->derinlik = g->tut_derinlik;
        if (g->dalis_yasak && (g->nav_derinlik < ist->derinlik)) {
            ist->derinlik = g->nav_derinlik; /* sızıntı algılayıcısı bayat: derinleşme yok */
        }
    } else {
        ist->dikey = (uint8_t)IDA_KON_DIKEY_KUVVET;
    }
}

/* Yön kanalı: yönü tut (bileşen ve hedef geçerliyse), değilse açısal hız 0. */
static void yon_tut(const ida_gor_t *g, ida_kon_istek_t *ist)
{
    if (g->nav_yon_var && g->tut_yon_var) {
        ist->yon_kanali = (uint8_t)IDA_KON_YON_ACI;
        ist->yon = g->tut_yon;
    } else {
        ist->yon_kanali = (uint8_t)IDA_KON_YON_HIZ;
    }
}

/* Yatay kanal: konum geçerliyse hız 0 (yerinde dur; konum döngüsü güdümde, dilim 2), değilse
   yatay itki sıfır (02 §4.2 HOLD kuralı). */
static void yatay_tut(const ida_gor_t *g, ida_kon_istek_t *ist)
{
    ist->yatay = g->nav_konum_var ? (uint8_t)IDA_KON_YATAY_HIZ
                                  : (uint8_t)IDA_KON_YATAY_KUVVET;
}

void ida_gor_istek(const ida_gor_t *g, const ida_emn_t *yetki, ida_kon_istek_t *ist)
{
    (void)memset(ist, 0, sizeof(*ist));
    ist->kip = (uint8_t)IDA_KON_KIP_KAPALI;
    ist->dalis_yasak = g->dalis_yasak ? 1u : 0u; /* kontrol de uygular (EMNİYET) */
    const uint8_t kip = yetki->mode;
    if (!yetki->armed || (kip == (uint8_t)IDA_MODE_INIT) || (kip == (uint8_t)IDA_MODE_DISARMED) ||
        (kip >= IDA_GOR_KIP_SAYI) || (g->fs_level >= (uint8_t)IDA_FS_PASSIVE_SURFACE)) {
        return;
    }
    ist->kip = (uint8_t)IDA_KON_KIP_DALIS;
    /* Yalpa ve baş-kıç sıfıra (tutum geçersizse açısal hız sıfıra). */
    ist->tutum = g->nav_tutum_var ? (uint8_t)IDA_KON_TUTUM_ACI : (uint8_t)IDA_KON_TUTUM_HIZ;

    if (g->fs_level >= (uint8_t)IDA_FS_HOLD) {
        /* HOLD: derinlik ve yön; SURFACE: yükselme (v_asc) ve yön; ikisinde de yatay HOLD
           kuralı; yüzeyde dikey kapalı (02 §4.2). Operatör girdisi kullanılmaz (arıza >
           operatör, 02 §2.6): kanallar kuvvet ya da moment seçmez. */
        yon_tut(g, ist);
        yatay_tut(g, ist);
        if (g->fs_level == (uint8_t)IDA_FS_SURFACE) {
            if ((g->gecerlilik & IDA_KES_G_DIKEY_HIZ) != 0u) {
                ist->dikey = (uint8_t)IDA_KON_DIKEY_HIZ;
                ist->dikey_hiz = -g->yukselme_hizi;
            } else {
                ist->dikey = (uint8_t)IDA_KON_DIKEY_KUVVET; /* pozitif yüzerlik */
            }
        } else {
            dikey_tut(g, ist);
        }
        if (g->yuzeyde) {
            ist->kip = (uint8_t)IDA_KON_KIP_YUZEY;
        }
        return;
    }

    switch (kip) {
    case IDA_MODE_MANUAL:
        /* A4-K49: operatör doğrudan kuvvet ve moment ister (eksenler kontrol modülünde). */
        ist->dikey = (uint8_t)IDA_KON_DIKEY_KUVVET;
        ist->tutum = (uint8_t)IDA_KON_TUTUM_MOMENT;
        ist->yon_kanali = (uint8_t)IDA_KON_YON_MOMENT;
        ist->yatay = (uint8_t)IDA_KON_YATAY_KUVVET;
        break;
    case IDA_MODE_STABILIZE:
        /* Tutum tutulur; öteleme ve yön hızı operatörden. */
        ist->dikey = (uint8_t)IDA_KON_DIKEY_KUVVET;
        ist->yon_kanali = (uint8_t)IDA_KON_YON_HIZ;
        ist->yatay = (uint8_t)IDA_KON_YATAY_KUVVET;
        break;
    case IDA_MODE_DEPTH_HOLD:
        /* Derinlik ve yön tutulur; X, Y operatörden. */
        dikey_tut(g, ist);
        yon_tut(g, ist);
        ist->yatay = (uint8_t)IDA_KON_YATAY_KUVVET;
        break;
    default:
        /* STATION_KEEP; GUIDED ve AUTO giriş anındaki konumda durur. GUIDED'da GUIDED_SP'nin
           maskeli kanalları güdümde bunun üzerine yazılır (W3-K7); görev hedefleri dilim 2. */
        dikey_tut(g, ist);
        yon_tut(g, ist);
        yatay_tut(g, ist);
        break;
    }
}
