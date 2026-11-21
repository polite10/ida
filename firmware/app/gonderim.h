/*
 * gonderim.h — kartın gönderim yöneticisi (A1-K10, A1-K110; 01 §2.6; W3-kart dilim 2).
 *
 * Donanımdan bağımsızdır (ISO C17; PC'de birim testiyle sınanır). Oturum modülü
 * (ida_link/ust.h) her çerçeveyi sınıfıyla verir; yönetici üç kuyruk tutar:
 *   (i)   KRİTİK         IDA_ACTUATOR ve çağrının yeni olayları; en yüksek öncelik.
 *   (ii)  ZORUNLU        Q_ii = 64 çerçeve; hiçbir çerçeve atılmaz; yer yoksa false döner
 *                        (oturum modülü oturumu TX_OVERFLOW ile bitirir).
 *   (iii) KAYBEDİLEBİLİR periyot (5 ms) başına en çok R_iii = 500 B taşımaya verilir; kuyruk
 *                        dolunca en eski çerçeve atılır ve tx_dropped sayılır.
 * Kuyruğa yalnız tam çerçeve girer. Taşıma sürücüsüne aynı anda en çok bir çerçeve verilir:
 * önceki çerçevenin son baytı sürücüden çıkmadan yenisi verilmez; böylece her çerçeve sınırında
 * (i) > (ii) > (iii) önceliği uygulanır ve kritik çıktının önünde en çok bir çerçeve bulunur.
 */
#ifndef IDA_FW_GONDERIM_H
#define IDA_FW_GONDERIM_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define IDA_GON_CERCEVE_AZAMI 267u /* IDA_LINK_CERCEVE_AZAMI */
#define IDA_GON_KRITIK_N 8u
#define IDA_GON_ZORUNLU_N 64u      /* Q_ii (A1 §3.9) */
#define IDA_GON_KAYBED_N 32u
#define IDA_GON_R_III_BAYT 500u    /* R_iii: 100 B/ms × 5 ms (A1 §3.9, E19) */

#define IDA_GON_SINIF_KRITIK 1u
#define IDA_GON_SINIF_ZORUNLU 2u
#define IDA_GON_SINIF_KAYBED 3u

typedef struct {
    uint16_t n;
    uint64_t t_us; /* kuyruğa girdiği an */
    uint8_t b[IDA_GON_CERCEVE_AZAMI];
} ida_gon_yuva_t;

typedef struct {
    ida_gon_yuva_t *yuva;
    uint32_t kap;
    uint32_t bas; /* sıradaki çıkacak */
    uint32_t say; /* dolu yuva */
    uint32_t azami;
} ida_gon_kuyruk_t;

/* Taşıma sürücüsü: bos() bir önceki çerçeve tümüyle çıktıysa true; yaz() tam çerçeveyi alır. */
typedef struct {
    void *ctx;
    bool (*bos)(void *ctx);
    bool (*yaz)(void *ctx, const uint8_t *b, size_t n);
} ida_gon_tasima_t;

typedef struct {
    ida_gon_yuva_t kritik_y[IDA_GON_KRITIK_N];
    ida_gon_yuva_t zorunlu_y[IDA_GON_ZORUNLU_N];
    ida_gon_yuva_t kaybed_y[IDA_GON_KAYBED_N];
    ida_gon_kuyruk_t q[3];
    uint32_t iii_bu_periyot; /* bu periyotta taşımaya verilen (iii) baytı */
    /* sayaçlar (A1 Tablo 3.11b'nin platform sayaçları ve tanı) */
    uint32_t tx_frames;      /* #30 */
    uint32_t tx_dropped;     /* #31: atılan (iii) */
    uint32_t tx_crit_stall;  /* #32: kritik çerçevenin bekleyip verilemediği periyot */
    uint32_t kritik_ret;     /* kritik kuyruk dolu (olmamalı) */
    uint32_t zorunlu_ret;    /* zorunlu kuyruk dolu (oturum sonu) */
    uint32_t yazma_ret;      /* sürücü çerçeveyi kabul etmedi */
    uint32_t kritik_bekleme_en_kotu_us; /* kuyruktan sürücüye en uzun kritik bekleme */
    bool tx_stalled;         /* son periyot sonunda kritik çerçeve bekliyordu */
} ida_gonderim_t;

void ida_gon_kur(ida_gonderim_t *g);
/* Çerçeveyi sınıfıyla kuyruğa koyar (kopyalar). Dönüş: kabul edildi mi. */
bool ida_gon_ekle(ida_gonderim_t *g, uint32_t sinif, const uint8_t *b, size_t n, uint64_t t_us);
/* Sürücü boşsa öncelik sırasıyla bir çerçeve verir. Dönüş: verilen çerçevenin sınıfı, yoksa 0. */
uint32_t ida_gon_pompa(ida_gonderim_t *g, const ida_gon_tasima_t *t, uint64_t simdi_us);
/* Periyot sınırı (5 ms): (iii) bütçesi yenilenir; kritik çerçeve bekliyorsa tx_crit_stall. */
void ida_gon_periyot(ida_gonderim_t *g, uint64_t simdi_us);
/* Yazımı başlamamış bütün çerçeveleri siler (A1-K22 (1); sürücüdekiler tamamlanır). */
void ida_gon_temizle(ida_gonderim_t *g);
uint32_t ida_gon_bekleyen(const ida_gonderim_t *g, uint32_t sinif);

#ifdef __cplusplus
}
#endif

#endif /* IDA_FW_GONDERIM_H */
