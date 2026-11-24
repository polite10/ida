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
 * Kritik çerçevenin kuyruğa alınamaması olağan kuyruk davranışı değildir (A1-K110): ret sayılır
 * ve kalıcı `kritik_hata` kurulur; platform bunu güvenli kapanış yoluna taşır (oturum biter,
 * platform güvenli beklemeye girer; A1-K22). Reddedilen her çerçeve sınıfına göre sayılır.
 *
 * Sıra numarası denetimi (tanı): MAVLink v2 çerçevesinin sıra baytı (bayt 4) üç noktada izlenir:
 * yöneticiye verilen her çerçeve (ret dahil; boşluk = üst katmanın sıra verip yöneticiye
 * vermediği çerçeve), kuyruğa giren çerçeve (boşluk = ret) ve taşımaya verilen çerçeve
 * (boşluk = atma ya da temizleme). Her nokta kesinti sayısını (alıcının rx_seq_gap'i gibi) ve
 * eksik çerçeve toplamını tutar.
 *
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

/* Sıra sürekliliği izleyicisi (tanı). */
typedef struct {
    uint8_t son;
    uint8_t var;
    uint32_t kesinti; /* süreksizlik sayısı */
    uint32_t eksik;   /* atlanan sıra numarası toplamı */
} ida_gon_sira_t;

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
    uint32_t kritik_ret;     /* kritik kuyruk dolu: çerçeve kuyruğa alınamadı (hata) */
    uint32_t zorunlu_ret;    /* zorunlu kuyruk dolu (oturum sonu) */
    uint32_t gecersiz_ret;   /* sınıf, boy ya da çerçeve geçersiz */
    uint32_t yazma_ret;      /* sürücü çerçeveyi kabul etmedi (çerçeve kuyrukta kalır) */
    uint32_t temizlenen;     /* ida_gon_temizle'nin sildiği çerçeve */
    bool kritik_hata;        /* kalıcı: bir kritik çerçeve kuyruğa alınamadı (A1-K110) */
    ida_gon_sira_t sira_giris;  /* ida_gon_ekle'ye verilen her çerçeve */
    ida_gon_sira_t sira_kuyruk; /* kuyruğa giren çerçeve */
    ida_gon_sira_t sira_cikis;  /* taşımaya verilen çerçeve */
    uint32_t kritik_bekleme_en_kotu_us; /* kuyruktan sürücüye en uzun kritik bekleme */
    bool tx_stalled;         /* son periyot sonunda kritik çerçeve bekliyordu */
} ida_gonderim_t;

void ida_gon_kur(ida_gonderim_t *g);
/* Çerçeveyi sınıfıyla kuyruğa koyar (kopyalar). Dönüş: kabul edildi mi. Kritik çerçeve
   reddedilirse kritik_hata kurulur (platform güvenli kapanışa geçmelidir). */
bool ida_gon_ekle(ida_gonderim_t *g, uint32_t sinif, const uint8_t *b, size_t n, uint64_t t_us);
/* Sürücü boşsa öncelik sırasıyla bir çerçeve verir. Dönüş: verilen çerçevenin sınıfı, yoksa 0. */
uint32_t ida_gon_pompa(ida_gonderim_t *g, const ida_gon_tasima_t *t, uint64_t simdi_us);
/* Periyot sınırı (5 ms): (iii) bütçesi yenilenir; kritik çerçeve bekliyorsa tx_crit_stall. */
void ida_gon_periyot(ida_gonderim_t *g, uint64_t simdi_us);
/* Yazımı başlamamış bütün çerçeveleri siler (A1-K22 (1); sürücüdekiler tamamlanır). */
void ida_gon_temizle(ida_gonderim_t *g);
uint32_t ida_gon_bekleyen(const ida_gonderim_t *g, uint32_t sinif);
/* Kritik ret oldu mu (kalıcı; yalnız ida_gon_kur sıfırlar). */
bool ida_gon_kritik_hata(const ida_gonderim_t *g);

#ifdef __cplusplus
}
#endif

#endif /* IDA_FW_GONDERIM_H */
