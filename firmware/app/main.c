/*
 * main.c — kart yazılımının ana döngüsü (01 §2.4; ida_link/ust.h akışı; W3-kart dilim 2).
 *
 * RTOS'suz tek bağlam (iskelet §8). Her tur:
 *   A  alım: sürücüden olay sınırlı bloklar (en çok B_ayr = 1 KB) ve damgaları oturum modülüne
 *      (ida_link_ust_rx). Çağrı penceresi açıkken (W3-K10) rx çağrılmaz; bu döngüde take ile
 *      put_out aynı turda art arda çağrıldığından pencere hiçbir zaman açık kalmaz, yine de
 *      ida_link_ust_cagri_acik ile denetlenir.
 *   A  poll_tx; take → platform alanları → ida_core_step / ida_core_tick → put_out. Kabul
 *      edilen PARAM_SAVE'in bloğu (A4-K82) kalıcı belleğe kaydedilmeye başlanır.
 *   A  kalıcı bellek kaydının bir adımı (bloklamayan); gönderim yöneticisinin pompası.
 *   A  200 Hz ızgara: periyot sınırı (gönderim bütçesi, arka plan dilimi, ölçüm).
 *   A  sağlık ve IWDG (01 §2.8; app/saglik.c).
 *   B  1 Hz: saat çapraz denetimi, ölçüm bloğu, LED.
 *
 * fw-measure (IDA_OLCUM): ida_fw_sinama_komutu'na SWD ile yazılan değer sınama yapar:
 *   0x49574447 ("IWDG")  ana döngü kasıtlı durur, IWDG beslenmez -> IWDG sıfırlaması
 *   0x48415441 ("HATA")  tanımsız komut -> UsageFault -> hata kaydı ve sıfırlama
 *   0x4B495744 ("KIWD")  bir sonraki parametre kaydı sürerken (1 KB yazıldıktan sonra) ana
 *                        döngü durur -> IWDG; kayıt yarım kalır (W3-kart dilim 2 kabul 5)
 */
#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#include "bsp.h"
#include "cagirici.h"
#include "flash.h"
#include "gonderim.h"
#include "ida/port.h"
#include "ida_link/ust.h"
#include "istatistik.h"
#include "kalici.h"
#include "olcum.h"
#include "saglik.h"
#include "seri.h"

#ifndef IDA_FW_GIT
#define IDA_FW_GIT 0u
#endif
#define IDA_FW_SURUM 0x000200u /* 0.2.0: W3-kart dilim 2 */

#define B_AYR 1024u                  /* A geçişinde bir turda ayrıştırılan en çok bayt (01 §2.4) */
#define OLCUM_ARALIK_US 1000000u     /* 1 Hz işleri                                          */
#define SAGLIK_S_US 20000u           /* ilerleme koşulu S = 20 ms (01 §2.8)                  */
#define T_BAG_NS 500000000u          /* T_bağ 500 ms (A1 §3.9; E2, E4)                       */
#define ESC_ALT_US 1100u             /* 04 §2.7.1 ESC değerleri (01 §3.1)                     */
#define ESC_NOTR_US 1500u
#define ESC_UST_US 1900u

#define SINAMA_IWDG 0x49574447u
#define SINAMA_HATA 0x48415441u
#define SINAMA_KAYIT_IWDG 0x4B495744u

volatile uint32_t ida_fw_olcum[IDA_OL_SAYI] __attribute__((used));
#if defined(IDA_OLCUM)
volatile uint32_t ida_fw_sinama_komutu __attribute__((used));
#endif

static ida_link_ust_t ust;
static ida_link_ust_ayar_t ust_ayar;
static ida_link_platform_t ust_plat;
static ida_gonderim_t gon;
static ida_kalici_t kal;
static ida_in_t girdi;
static ida_out_t cikti;
static uint8_t blok[B_AYR];
static uint32_t saat_adim;

/* Ölçüm sayaçları (01 §2.11) */
static ida_ist_t blok_ist, periyot_ist, skart_ist;
static uint64_t blok_bayt;
static uint32_t blok_en_kotu_bayt;
static uint32_t bb_sayi[16], bb_bayt[16], bb_cevrim[16]; /* blok boyuna (log2) göre */
static uint32_t periyot_cevrim, periyot_bayt, periyot_en_kotu_bayt;
static uint32_t tur_sayisi, tur_en_kotu, cagri_tur_en_kotu;
static ida_izgara_t izgara;
static uint32_t izgara_kacirma, izgara_gecikme_en_kotu, izgara_islenen;
static uint32_t besleme_sayisi, besleme_ret, besleme_ret_bitleri;
static uint64_t son_besleme_us;
static uint32_t besleme_aralik_en_kotu;
static uint32_t capraz_ornek, capraz_son_aralik;
static int32_t capraz_min, capraz_max;
static uint32_t capraz_onceki_cevrim;
static uint64_t capraz_onceki_us;
static ida_rx_gozetim_t rx_gozetim;
static uint32_t nvic_yeniden;
static uint32_t olcum_sira;
static uint32_t rx_bekleme_en_uzun, besleme_ret_son;
static uint32_t stamp_coarse;
static uint32_t kal_acilis_cevrim, kal_kayit_sure_us, kal_kayit_en_kotu_tur;
static uint64_t kal_kayit_bas_us;

/* ---- platform işlevleri (ida_link_platform_t) ------------------------------------------- */

static ida_wtime_t p_zaman(void *ctx)
{
    (void)ctx;
    ida_wtime_t w;
    w.ns = (int64_t)(ida_bsp_zaman_us() * 1000u);
    return w;
}

static bool p_gonder(void *ctx, ida_link_sinif_t sinif, const uint8_t *cerceve, size_t n)
{
    (void)ctx;
    return ida_gon_ekle(&gon, (uint32_t)sinif, cerceve, n, ida_bsp_zaman_us());
}

static void p_temizle(void *ctx)
{
    (void)ctx;
    ida_gon_temizle(&gon);
}

static uint32_t p_nonce(void *ctx)
{
    (void)ctx;
    return ida_bsp_rastgele();
}

static void p_yeniden(void *ctx)
{
    (void)ctx;
    ida_bsp_yeniden_baslat();
}

static void p_blok(void *ctx, uint32_t *crc, uint32_t *len, uint32_t *gen)
{
    (void)ctx;
    if (kal.gecerli_sayfa < 0) {
        *crc = 0u;
        *len = 0u;
        *gen = 0u;
        return;
    }
    *crc = kal.blok_crc;
    *len = kal.blok_len;
    *gen = kal.blok_gen;
}

/* ---- kalıcı bellek ve gönderim bağlantıları ---------------------------------------------- */

static const uint8_t *f_taban(void *ctx)
{
    (void)ctx;
    return ida_flash_taban();
}

static int f_sil(void *ctx, uint32_t sayfa)
{
    (void)ctx;
    return ida_flash_sil(sayfa);
}

static int f_yaz16(void *ctx, uint32_t ofs, const uint8_t q[IDA_KAL_QW])
{
    (void)ctx;
    return ida_flash_yaz16(ofs, q);
}

static int f_durum(void *ctx)
{
    (void)ctx;
    return ida_flash_durum();
}

static void f_bekle(void *ctx)
{
    (void)ctx;
    ida_bsp_iwdg_besle();
}

static bool t_bos(void *ctx)
{
    (void)ctx;
    return ida_seri_gonderim_bos();
}

static bool t_yaz(void *ctx, const uint8_t *b, size_t n)
{
    (void)ctx;
    return ida_seri_yaz(b, n);
}

static const ida_gon_tasima_t tasima = {NULL, t_bos, t_yaz};

/* ---- ölçüm bloğu ------------------------------------------------------------------------- */

static void olcum_yaz64(uint32_t lo_indis, uint64_t v)
{
    ida_fw_olcum[lo_indis] = (uint32_t)v;
    ida_fw_olcum[lo_indis + 1u] = (uint32_t)(v >> 32);
}

static void olcum_dizi(uint32_t indis, const uint32_t *d)
{
    for (uint32_t i = 0u; i < 16u; i++) {
        ida_fw_olcum[indis + i] = d[i];
    }
}

static void olcum_guncelle(uint64_t simdi)
{
    const ida_cagirici_t *c = ida_cagirici_durum();
    const ida_acilis_sonuc_t *a = ida_bsp_acilis();
    const ida_acilis_kaydi_t *k = ida_bsp_kayit();
    ida_seri_sayac_t s;
    ida_seri_sayaclar(&s);
    uint32_t say[IDA_LINK_SAY_N];
    ida_link_ust_sayaclar(&ust, say);
    volatile uint32_t *o = ida_fw_olcum;
    /* Yayın sıra numarası: güncelleme sürerken tek, bitince çift (sıra kilidi; olcum_oku.py). */
    olcum_sira++;
    o[IDA_OL_yayin_sira] = olcum_sira;
    ida_bsp_bariyer();
    o[IDA_OL_magic] = IDA_FW_OLCUM_MAGIC;
    o[IDA_OL_surum] = IDA_FW_OLCUM_SURUM;
    o[IDA_OL_sozcuk_sayisi] = IDA_OL_SAYI;
    o[IDA_OL_fw_git] = IDA_FW_GIT;
#if defined(IDA_OLCUM)
    o[IDA_OL_derleme_olcum] = 1u;
    o[IDA_OL_sinama_komutu] = ida_fw_sinama_komutu;
#else
    o[IDA_OL_derleme_olcum] = 0u;
#endif
    o[IDA_OL_sysclk_hz] = ida_bsp_sysclk_hz();
    o[IDA_OL_usart_hz] = s.usart_hz;
    o[IDA_OL_baud_brr] = s.baud_brr;
    o[IDA_OL_brr] = s.brr;
    o[IDA_OL_saat_adim] = saat_adim;
    olcum_yaz64(IDA_OL_zaman_us_lo, simdi);
    o[IDA_OL_capraz_ornek] = capraz_ornek;
    o[IDA_OL_capraz_sapma_en_kucuk_i] = (uint32_t)capraz_min;
    o[IDA_OL_capraz_sapma_en_buyuk_i] = (uint32_t)capraz_max;
    o[IDA_OL_capraz_son_aralik_us] = capraz_son_aralik;
    o[IDA_OL_tur_sayisi] = tur_sayisi;
    o[IDA_OL_tur_en_kotu_cevrim] = tur_en_kotu;
    o[IDA_OL_izgara_sayisi] = izgara_islenen;
    o[IDA_OL_izgara_kacirma] = izgara_kacirma;
    o[IDA_OL_izgara_gecikme_en_kotu_us] = izgara_gecikme_en_kotu;
    o[IDA_OL_besleme_sayisi] = besleme_sayisi;
    o[IDA_OL_besleme_aralik_en_kotu_us] = besleme_aralik_en_kotu;
    o[IDA_OL_besleme_ret] = besleme_ret;
    o[IDA_OL_besleme_ret_bitleri] = besleme_ret_bitleri;
    o[IDA_OL_besleme_ret_son_ms] = besleme_ret_son;
    o[IDA_OL_rx_bekleme_en_uzun_us] = rx_bekleme_en_uzun;
    o[IDA_OL_cekirdek_durum] = (uint32_t)c->durum | ((uint32_t)c->neden << 8);
    o[IDA_OL_init_rc] = c->init_rc;
    o[IDA_OL_core_size] = c->bilgi.core_size;
    o[IDA_OL_core_ok] = c->core_ok ? 1u : 0u;
    o[IDA_OL_step_sayisi] = c->step.sayi;
    o[IDA_OL_step_son_cevrim] = c->step.son_cevrim;
    o[IDA_OL_step_en_kotu_cevrim] = c->step.en_kotu_cevrim;
    o[IDA_OL_step_en_iyi_cevrim] = c->step.en_iyi_cevrim;
    olcum_yaz64(IDA_OL_step_toplam_cevrim_lo, c->step.toplam_cevrim);
    olcum_dizi(IDA_OL_step_kutu, c->step.kutu);
    o[IDA_OL_step_butce_asimi] = c->step_butce_asimi;
    o[IDA_OL_tick_sayisi] = c->tick.sayi;
    o[IDA_OL_tick_en_kotu_cevrim] = c->tick.en_kotu_cevrim;
    o[IDA_OL_tick_en_iyi_cevrim] = c->tick.en_iyi_cevrim;
    olcum_yaz64(IDA_OL_tick_toplam_cevrim_lo, c->tick.toplam_cevrim);
    olcum_dizi(IDA_OL_tick_kutu, c->tick.kutu);
    o[IDA_OL_tick_butce_asimi] = c->tick_butce_asimi;
    o[IDA_OL_cagri_rc_hata] = c->rc_hata;
    o[IDA_OL_cagri_son_rc] = c->son_rc;
    o[IDA_OL_cagri_tur_en_kotu_cevrim] = cagri_tur_en_kotu;
    o[IDA_OL_bg_sayisi] = c->arka_plan.sayi;
    o[IDA_OL_bg_en_kotu_cevrim] = c->arka_plan.en_kotu_cevrim;
    o[IDA_OL_bg_more] = c->bg_more;
    o[IDA_OL_skart_sayisi] = skart_ist.sayi;
    o[IDA_OL_skart_en_kotu_cevrim] = skart_ist.en_kotu_cevrim;
    o[IDA_OL_skart_en_iyi_cevrim] = skart_ist.en_iyi_cevrim;
    olcum_yaz64(IDA_OL_skart_toplam_cevrim_lo, skart_ist.toplam_cevrim);
    olcum_dizi(IDA_OL_skart_kutu, skart_ist.kutu);
    o[IDA_OL_blok_sayisi] = blok_ist.sayi;
    o[IDA_OL_blok_en_kotu_cevrim] = blok_ist.en_kotu_cevrim;
    o[IDA_OL_blok_en_kotu_bayt] = blok_en_kotu_bayt;
    o[IDA_OL_blok_en_iyi_cevrim] = blok_ist.en_iyi_cevrim;
    olcum_yaz64(IDA_OL_blok_toplam_cevrim_lo, blok_ist.toplam_cevrim);
    olcum_yaz64(IDA_OL_blok_bayt_lo, blok_bayt);
    olcum_dizi(IDA_OL_blok_kutu, blok_ist.kutu);
    olcum_dizi(IDA_OL_bb_sayi, bb_sayi);
    olcum_dizi(IDA_OL_bb_bayt, bb_bayt);
    olcum_dizi(IDA_OL_bb_cevrim, bb_cevrim);
    o[IDA_OL_periyot_sayisi] = periyot_ist.sayi;
    o[IDA_OL_periyot_en_kotu_cevrim] = periyot_ist.en_kotu_cevrim;
    o[IDA_OL_periyot_en_kotu_bayt] = periyot_en_kotu_bayt;
    olcum_dizi(IDA_OL_periyot_kutu, periyot_ist.kutu);
    o[IDA_OL_ore] = s.ore;
    o[IDA_OL_fe] = s.fe;
    o[IDA_OL_ne] = s.ne;
    o[IDA_OL_pe] = s.pe;
    o[IDA_OL_dma_rx_hata] = s.dma_rx_hata;
    o[IDA_OL_dma_tx_hata] = s.dma_tx_hata;
    o[IDA_OL_rx_kararsiz] = s.kararsiz;
    o[IDA_OL_rx_belirsiz] = s.belirsiz;
    o[IDA_OL_rx_gecersiz] = s.gecersiz;
    o[IDA_OL_rx_geri] = s.geri;
    o[IDA_OL_rx_olay_birlesme] = s.olay_birlesme;
    o[IDA_OL_olay_eslesme] = s.olay_eslesme;
    o[IDA_OL_olay_bosta] = s.olay_bosta;
    o[IDA_OL_olay_yarim] = s.olay_yarim;
    o[IDA_OL_olay_tam] = s.olay_tam;
    o[IDA_OL_rx_tasma] = s.tasma;
    o[IDA_OL_rx_tasma_bayt] = s.tasma_bayt;
    o[IDA_OL_rx_halka_azami] = s.halka_azami;
    o[IDA_OL_uart_bekle_hata] = s.uart_bekle_hata;
    o[IDA_OL_tx_bayt] = s.tx_bayt;
    o[IDA_OL_tx_ret] = s.tx_ret;
    o[IDA_OL_tx_doluluk_azami] = s.tx_doluluk_azami;
    o[IDA_OL_gon_tx_frames] = gon.tx_frames;
    o[IDA_OL_gon_tx_dropped] = gon.tx_dropped;
    o[IDA_OL_gon_crit_stall] = gon.tx_crit_stall;
    o[IDA_OL_gon_kritik_ret] = gon.kritik_ret;
    o[IDA_OL_gon_zorunlu_ret] = gon.zorunlu_ret;
    o[IDA_OL_gon_yazma_ret] = gon.yazma_ret;
    o[IDA_OL_gon_kritik_bekleme_en_kotu_us] = gon.kritik_bekleme_en_kotu_us;
    o[IDA_OL_gon_q1_azami] = gon.q[0].azami;
    o[IDA_OL_gon_q2_azami] = gon.q[1].azami;
    o[IDA_OL_gon_q3_azami] = gon.q[2].azami;
    o[IDA_OL_link_durum] = (uint32_t)ida_link_ust_durum(&ust);
    o[IDA_OL_link_sess] = ida_link_ust_sess(&ust);
    o[IDA_OL_say_rx_bytes] = say[IDA_LINK_SAY_RX_BYTES];
    o[IDA_OL_say_rx_frames] = say[IDA_LINK_SAY_RX_FRAMES];
    o[IDA_OL_say_rx_crc_err] = say[IDA_LINK_SAY_RX_CRC_ERR];
    o[IDA_OL_say_rx_unknown_msgid] = say[IDA_LINK_SAY_RX_UNKNOWN_MSGID];
    o[IDA_OL_say_rx_iflag] = say[IDA_LINK_SAY_RX_IFLAG];
    o[IDA_OL_say_rx_bad_len] = say[IDA_LINK_SAY_RX_BAD_LEN];
    o[IDA_OL_say_rx_wrong_sys] = say[IDA_LINK_SAY_RX_WRONG_SYS];
    o[IDA_OL_say_rx_overflow] = say[IDA_LINK_SAY_RX_OVERFLOW];
    o[IDA_OL_say_rx_resync_bytes] = say[IDA_LINK_SAY_RX_RESYNC_BYTES];
    o[IDA_OL_say_rx_seq_gap] = say[IDA_LINK_SAY_RX_SEQ_GAP];
    o[IDA_OL_say_stale_session] = say[IDA_LINK_SAY_RX_STALE_SESSION];
    o[IDA_OL_say_outside_session] = say[IDA_LINK_SAY_RX_OUTSIDE_SESSION];
    o[IDA_OL_say_stamp_coarse] = say[IDA_LINK_SAY_STAMP_COARSE];
    o[IDA_OL_say_batch_closed] = say[IDA_LINK_SAY_BATCH_CLOSED];
    o[IDA_OL_say_batch_incomplete] = say[IDA_LINK_SAY_BATCH_INCOMPLETE];
    o[IDA_OL_say_batch_skipped] = say[IDA_LINK_SAY_BATCH_SKIPPED];
    o[IDA_OL_say_batch_old] = say[IDA_LINK_SAY_BATCH_OLD];
    o[IDA_OL_say_batch_dup] = say[IDA_LINK_SAY_BATCH_DUP];
    o[IDA_OL_say_batch_merged] = say[IDA_LINK_SAY_BATCH_MERGED];
    o[IDA_OL_say_member_overflow] = say[IDA_LINK_SAY_MEMBER_OVERFLOW];
    o[IDA_OL_say_grid_missed] = say[IDA_LINK_SAY_GRID_MISSED];
    o[IDA_OL_say_out_fixed] = say[IDA_LINK_SAY_OUT_FIXED];
    o[IDA_OL_say_events_sent] = say[IDA_LINK_SAY_EVENTS_SENT];
    o[IDA_OL_say_events_resent] = say[IDA_LINK_SAY_EVENTS_RESENT];
    o[IDA_OL_say_sessions] = say[IDA_LINK_SAY_SESSIONS];
    o[IDA_OL_kal_boot_count] = kal.boot_count;
    o[IDA_OL_kal_boot_ok] = kal.boot_count_ok;
    o[IDA_OL_kal_store] = kal.store;
    o[IDA_OL_kal_fail] = kal.fail;
    o[IDA_OL_kal_store_gen] = kal.store_gen;
    o[IDA_OL_kal_blok_gen] = kal.blok_gen;
    o[IDA_OL_kal_blok_len] = kal.blok_len;
    o[IDA_OL_kal_blok_crc] = kal.blok_crc;
    o[IDA_OL_kal_gecerli_sayfa_i] = (uint32_t)(int32_t)kal.gecerli_sayfa;
    o[IDA_OL_kal_kayit_sayisi] = kal.kayit_sayisi;
    o[IDA_OL_kal_kayit_hata] = kal.kayit_hata;
    o[IDA_OL_kal_sayac_deneme] = kal.sayac_yazim_deneme;
    o[IDA_OL_kal_acilis_cevrim] = kal_acilis_cevrim;
    o[IDA_OL_kal_kayit_sure_us] = kal_kayit_sure_us;
    o[IDA_OL_kal_kayit_en_kotu_tur_cevrim] = kal_kayit_en_kotu_tur;
    o[IDA_OL_fl_silme_sayisi] = ida_flash_olcum.silme_sayisi;
    o[IDA_OL_fl_yazma_sayisi] = ida_flash_olcum.yazma_sayisi;
    o[IDA_OL_fl_silme_en_kotu_cevrim] = ida_flash_olcum.silme_en_kotu_cevrim;
    o[IDA_OL_fl_yazma_en_kotu_cevrim] = ida_flash_olcum.yazma_en_kotu_cevrim;
    o[IDA_OL_fl_hata] = ida_flash_olcum.hata;
    o[IDA_OL_fl_ecc_nmi] = ida_flash_olcum.ecc_nmi;
    o[IDA_OL_yigin_boyu] = ida_bsp_yigin_boyu();
    o[IDA_OL_yigin_en_yuksek] = ida_bsp_yigin_en_yuksek();
    o[IDA_OL_reset_cause] = a->reset_cause;
    o[IDA_OL_reset_flags] = a->reset_flags;
    o[IDA_OL_ardisik_ariza] = a->ardisik_ariza;
    o[IDA_OL_kayit_gecerliydi] = a->kayit_gecerliydi;
    o[IDA_OL_fpscr] = ida_bsp_fpscr_acilis;
    o[IDA_OL_onceki_ariza] = a->onceki_ariza;
    o[IDA_OL_ariza_sayisi] = k->ariza_sayisi;
    o[IDA_OL_ariza_cfsr] = k->cfsr;
    o[IDA_OL_ariza_hfsr] = k->hfsr;
    o[IDA_OL_ariza_pc] = k->pc;
    o[IDA_OL_ariza_lr] = k->lr;
    o[IDA_OL_ariza_yigin_gecerli] = k->yigin_gecerli;
    o[IDA_OL_ariza_istisna] = k->istisna;
    o[IDA_OL_nvic_yeniden] = nvic_yeniden;
    o[IDA_OL_rx_yeniden] = s.rx_yeniden;
    o[IDA_OL_rx_durdu] = s.rx_durdu;
    ida_bsp_bariyer();
    olcum_sira++;
    o[IDA_OL_yayin_sira] = olcum_sira;
}

/* DWT ile duvar saatinin birbirini doğrulaması (dilim 1; ±160 çevrim okuma farkı). */
static void capraz_denetim(void)
{
    uint32_t cevrim = 0u;
    uint64_t t = 0u;
    ida_bsp_cevrim_ve_zaman(&cevrim, &t);
    if (capraz_onceki_us != 0u) {
        const uint64_t dt = t - capraz_onceki_us;
        const uint32_t dc = cevrim - capraz_onceki_cevrim;
        if (dt < 20000000u) { /* DWT 26,8 s'de sarar */
            const int64_t sapma = (int64_t)dc - (int64_t)(dt * IDA_IST_CEVRIM_US);
            const int32_t s = sapma > INT32_MAX ? INT32_MAX
                              : sapma < INT32_MIN ? INT32_MIN
                                                  : (int32_t)sapma;
            if (capraz_ornek == 0u || s < capraz_min) {
                capraz_min = s;
            }
            if (capraz_ornek == 0u || s > capraz_max) {
                capraz_max = s;
            }
            capraz_ornek++;
            capraz_son_aralik = (uint32_t)dt;
        }
    }
    capraz_onceki_cevrim = cevrim;
    capraz_onceki_us = t;
}

/* 01 §2.8 ilerleme koşulları (karar app/saglik.c); hepsi tutarsa IWDG beslenir. */
static void saglik(uint64_t simdi)
{
    const ida_cagirici_t *c = ida_cagirici_durum();
    const bool bekleyen = ida_seri_bekleyen();
    ida_saglik_girdi_t g;
    g.cekirdek_normal = c->durum == IDA_CAG_NORMAL;
    g.core_ok = c->core_ok;
    g.son_cagri_us = c->son_cagri_us;
    g.rx_tamam = ida_rx_gozetim_tamam(&rx_gozetim, bekleyen, simdi, SAGLIK_S_US);
    if (bekleyen && rx_gozetim.bekliyordu &&
        simdi - rx_gozetim.son_ilerleme_us > rx_bekleme_en_uzun) {
        const uint64_t b = simdi - rx_gozetim.son_ilerleme_us;
        rx_bekleme_en_uzun = b > UINT32_MAX ? UINT32_MAX : (uint32_t)b;
    }
    g.alim_durdu = ida_seri_alim_durdu();
    g.simdi_us = simdi;
    g.s_us = SAGLIK_S_US;
    const uint32_t ret = ida_saglik_degerlendir(&g);
    if (ret != 0u) {
        besleme_ret++;
        besleme_ret_bitleri |= ret;
        besleme_ret_son = (uint32_t)(simdi / 1000u);
        return;
    }
    ida_bsp_iwdg_besle();
    if (son_besleme_us != 0u) {
        const uint64_t aralik = simdi - son_besleme_us;
        if (aralik > besleme_aralik_en_kotu) {
            besleme_aralik_en_kotu = aralik > UINT32_MAX ? UINT32_MAX : (uint32_t)aralik;
        }
    }
    son_besleme_us = simdi;
    besleme_sayisi++;
}

#if defined(IDA_OLCUM)
static void sinama(void)
{
    const uint32_t k = ida_fw_sinama_komutu;
    if (k == SINAMA_IWDG ||
        (k == SINAMA_KAYIT_IWDG && ida_kal_mesgul(&kal) && kal.sonraki_ofs > 1024u)) {
        ida_bsp_led_kirmizi(true);
        for (;;) {
            /* Kasıtlı durma: IWDG beslenmez (W3-kart kabul md. 6; dilim 2 kabul 5). */
        }
    }
    if (k == SINAMA_HATA) {
        ida_fw_sinama_komutu = 0u;
        __builtin_trap(); /* UDF -> UsageFault (UNDEFINSTR) */
    }
}
#endif

/* Alım: olay sınırlı bloklar oturum modülüne (blok besleme). */
static void alim(void)
{
    if (ida_link_ust_cagri_acik(&ust)) {
        return; /* W3-K10: çağrı penceresinde rx çağrılmaz; baytlar sürücüde bekler */
    }
    size_t bu_tur = 0u;
    bool ilerledi = false;
    while (bu_tur < B_AYR) {
        uint64_t t_rx = 0u;
        bool kaba = false;
        bool tasma = false;
        const size_t n = ida_seri_oku(blok, sizeof blok, &t_rx, &kaba, &tasma);
        if (tasma) {
            ida_link_ust_rx_tasma(&ust); /* A1-K09 */
            ilerledi = true;
        }
        if (n == 0u) {
            break;
        }
        ilerledi = true;
        uint32_t say_once[IDA_LINK_SAY_N];
        if (kaba) {
            ida_link_ust_sayaclar(&ust, say_once);
        }
        ida_wtime_t t;
        t.ns = (int64_t)(t_rx * 1000u);
        const uint32_t c0 = ida_bsp_cevrim();
        ida_link_ust_rx(&ust, blok, n, t);
        const uint32_t c = ida_bsp_cevrim() - c0;
        if (kaba) {
            /* Kaba damgalı blokta biten çerçeveler: A1-K48'in stamp_coarse'u (platformun). */
            uint32_t say_sonra[IDA_LINK_SAY_N];
            ida_link_ust_sayaclar(&ust, say_sonra);
            stamp_coarse += say_sonra[IDA_LINK_SAY_RX_FRAMES] - say_once[IDA_LINK_SAY_RX_FRAMES];
        }
        ida_ist_ekle(&blok_ist, c);
        if (c >= blok_ist.en_kotu_cevrim) {
            blok_en_kotu_bayt = (uint32_t)n;
        }
        uint32_t kutu = 0u;
        while (kutu < 15u && (n >> (kutu + 1u)) != 0u) {
            kutu++;
        }
        bb_sayi[kutu]++;
        bb_bayt[kutu] += (uint32_t)n;
        bb_cevrim[kutu] += c;
        blok_bayt += n;
        periyot_cevrim += c;
        periyot_bayt += (uint32_t)n;
        bu_tur += n;
    }
    ida_rx_gozetim_guncelle(&rx_gozetim, ida_seri_bekleyen(), ilerledi, ida_bsp_zaman_us());
}

/* take → çekirdek → put_out. */
static void cagri(void)
{
    const ida_link_cagri_t tur = ida_link_ust_take(&ust, &girdi);
    if (tur == IDA_LINK_CAGRI_YOK) {
        return;
    }
    const uint32_t c0 = ida_bsp_cevrim();
    /* Platformun alanları (ust.h akış adımı 3; A4 §3.6). */
    girdi.plat.param_store = kal.store;
    girdi.plat.param_fail = kal.fail;
    girdi.plat.param_gen = kal.store_gen;
    girdi.plat.tx_stalled = gon.tx_stalled;
    const ida_cagirici_t *c = ida_cagirici_durum();
    uint64_t t_out;
    if (c->durum == IDA_CAG_NORMAL) {
        const ida_rc_t rc = ida_cagirici_cagir(tur == IDA_LINK_CAGRI_STEP, &girdi, &cikti,
                                               ida_bsp_zaman_us());
        ida_link_ust_put_out(&ust, &girdi, rc, &cikti);
        t_out = ida_bsp_zaman_us();
        /* Kabul edilen PARAM_SAVE (A4-K82): blob bir sonraki çekirdek çağrısına kadar geçerli;
           kalıcı bellek modülü kopyalar ve kaydı adım adım yürütür. */
        for (uint32_t i = 0u; i < cikti.n_results && i < IDA_PORT_MAX_RESULTS; i++) {
            const ida_cmd_result_t *r = &cikti.results[i];
            if (r->kind == IDA_RES_PARAM_SAVE && r->ack == IDA_ACK_ACCEPTED &&
                r->save.blob != NULL) {
                ida_kal_kaydet(&kal, r->save.blob, r->save.len, r->save.gen);
                kal_kayit_bas_us = ida_bsp_zaman_us();
            }
        }
    } else {
        /* Güvenli bekleme: çekirdek çağrılmaz; çıktı yok (ust.h: out NULL). */
        ida_link_ust_put_out(&ust, &girdi, IDA_RC_NOT_INIT, NULL);
        t_out = ida_bsp_zaman_us();
    }
    if (tur == IDA_LINK_CAGRI_STEP || tur == IDA_LINK_CAGRI_TICK_TF) {
        /* S_kart: partiyi kapatan TF'nin alım damgasından aktüatör çerçevesinin gönderim
           kuyruğuna verilmesine (put_out dönüşü) kadar; put_out'tan sonraki kayıt başlatma
           (blob kopyası ve CRC) dahil değildir. */
        const uint64_t t_rx = (uint64_t)girdi.tf.t_rx.ns / 1000u;
        if (t_out >= t_rx) {
            const uint64_t us = t_out - t_rx;
            const uint64_t cv = us * IDA_IST_CEVRIM_US;
            ida_ist_ekle(&skart_ist, cv > UINT32_MAX ? UINT32_MAX : (uint32_t)cv);
        }
    }
    const uint32_t ct = ida_bsp_cevrim() - c0;
    if (ct > cagri_tur_en_kotu) {
        cagri_tur_en_kotu = ct;
    }
}

int main(void)
{
    ida_bsp_erken_baslat();
    saat_adim = ida_bsp_saat_kur();
    ida_bsp_cevre_baslat();
    const bool saat_tamam = saat_adim == 0u;

    ida_ist_sifirla(&blok_ist);
    ida_ist_sifirla(&periyot_ist);
    ida_ist_sifirla(&skart_ist);
    ida_gon_kur(&gon);

    /* Kalıcı bellek: geçerli parametre bloğu ve açılış sayacı (bloklayıcı; 2 s IWDG içinde). */
    const ida_kal_flash_t fl = {NULL, f_taban, f_sil, f_yaz16, f_durum};
    const uint32_t k0 = ida_bsp_cevrim();
    if (saat_tamam) {
        ida_kal_acilis(&kal, &fl, f_bekle, NULL);
    } else {
        kal.gecerli_sayfa = -1; /* flash'a dokunulmaz: blok yok, sayaç ayrılamadı */
    }
    kal_acilis_cevrim = ida_bsp_cevrim() - k0;

    bool baglanti = false;
    if (saat_tamam) {
        baglanti = ida_seri_baslat() == 0;
    }

    uint32_t blob_len = 0u;
    const uint8_t *blob = saat_tamam ? ida_kal_blob(&kal, &blob_len) : NULL;
    ida_cagirici_kurulum_t ck;
    memset(&ck, 0, sizeof ck);
    ck.acilis = ida_bsp_acilis();
    ck.saat_tamam = saat_tamam;
    ck.simdi_us = ida_bsp_zaman_us();
    ck.boot_count = kal.boot_count;
    ck.boot_count_ok = kal.boot_count_ok != 0u;
    ck.param_src = (uint8_t)(blob != NULL ? IDA_PSRC_FLASH : IDA_PSRC_NONE);
    ck.param_blob = blob;
    ck.param_len = blob_len;
    ida_cagirici_kur(&ck);
    const ida_cagirici_t *cag = ida_cagirici_durum();
    const ida_acilis_sonuc_t *acilis = ida_bsp_acilis();
    const ida_acilis_kaydi_t *kayit = ida_bsp_kayit();

    /* Oturum modülü (ust.h). */
    memset(&ust_ayar, 0, sizeof ust_ayar);
    ust_ayar.rol = IDA_LINK_UST_ROL_KART;
    ust_ayar.boot_count = kal.boot_count;
    ust_ayar.boot_count_ok = kal.boot_count_ok != 0u;
    ust_ayar.reset_cause = acilis->reset_cause;
    ust_ayar.reset_flags = acilis->reset_flags;
    ust_ayar.consecutive_faults = acilis->ardisik_ariza;
    ust_ayar.param_src = ck.param_src;
    /* Güvenli beklemede modül kurulur ve hemen güvenli beklemeye alınır; duyurular
       init_rc = NOT_INIT taşır (A1-K13). */
    ust_ayar.init_rc = cag->durum == IDA_CAG_NORMAL ? cag->init_rc : (uint8_t)IDA_RC_PARAM_DEFAULTS;
    ust_ayar.cekirdek = cag->bilgi;
    ust_ayar.fw_version = IDA_FW_SURUM;
    ust_ayar.fw_git = IDA_FW_GIT;
    ust_ayar.t_bag_ns = T_BAG_NS;
    ust_ayar.esc_alt_us = ESC_ALT_US;
    ust_ayar.esc_notr_us = ESC_NOTR_US;
    ust_ayar.esc_ust_us = ESC_UST_US;
    if (acilis->onceki_ariza && kayit->ariza_var) {
        ust_ayar.hata_kaydi_var = true;
        ust_ayar.hata_kaydi.cfsr = kayit->cfsr;
        ust_ayar.hata_kaydi.hfsr = kayit->hfsr;
        ust_ayar.hata_kaydi.mmfar = kayit->mmfar;
        ust_ayar.hata_kaydi.bfar = kayit->bfar;
        ust_ayar.hata_kaydi.pc = kayit->pc;
        ust_ayar.hata_kaydi.lr = kayit->lr;
        ust_ayar.hata_kaydi.xpsr = kayit->xpsr;
        ust_ayar.hata_kaydi.step_counter = kayit->adim;
        ust_ayar.hata_kaydi.fault_seq = (uint16_t)kayit->ariza_sayisi;
        ust_ayar.hata_kaydi.stacking_valid = (uint8_t)kayit->yigin_gecerli;
    }
    ust_plat.ctx = NULL;
    ust_plat.zaman = p_zaman;
    ust_plat.gonder = p_gonder;
    ust_plat.kuyruk_temizle = p_temizle;
    ust_plat.nonce = p_nonce;
    ust_plat.yeniden_baslat = p_yeniden;
    ust_plat.kalici_blok = p_blok;
    const bool ust_tamam = ida_link_ust_kur(&ust, &ust_ayar, &ust_plat) == IDA_LINK_TAMAM;
    if (cag->durum != IDA_CAG_NORMAL) {
        ida_link_ust_guvenli_bekleme(&ust);
    }

    ida_bsp_calisma_kipi();
    const ida_led_t led =
        ida_led_durumu(saat_tamam, baglanti && ust_tamam, cag->durum == IDA_CAG_NORMAL);
    ida_bsp_led_yesil(led.yesil);
    ida_bsp_led_kirmizi(led.kirmizi);

    uint64_t sonraki_olcum = ida_bsp_zaman_us() + OLCUM_ARALIK_US;
    ida_izgara_kur(&izgara, ida_bsp_izgara_sayaci());
    ida_rx_gozetim_kur(&rx_gozetim, ida_bsp_zaman_us());
    ida_link_ust_durum_t onceki_durum = ida_link_ust_durum(&ust);
    olcum_guncelle(ida_bsp_zaman_us());

    for (;;) {
        const uint32_t tur_bas = ida_bsp_cevrim();

        /* ---- A: alım, zamana bağlı işler, çekirdek çağrısı ---- */
        if (baglanti && ust_tamam) {
            alim();
            ida_link_ust_poll_tx(&ust);
            cagri();
            /* Platformun Tablo 3.11b sayaçları (ust.h: platform diziye doğrudan yazar). */
            ust.say[IDA_LINK_SAY_STAMP_COARSE] = stamp_coarse;
            ust.say[IDA_LINK_SAY_TX_FRAMES] = gon.tx_frames;
            ust.say[IDA_LINK_SAY_TX_DROPPED] = gon.tx_dropped;
            ust.say[IDA_LINK_SAY_TX_CRIT_STALL] = gon.tx_crit_stall;
        }

        /* ---- A: kalıcı bellek kaydı (oturum sonunda iptal; A4-K83) ---- */
        const ida_link_ust_durum_t durum = ida_link_ust_durum(&ust);
        if (onceki_durum == IDA_LINK_UST_RUNNING && durum != IDA_LINK_UST_RUNNING) {
            ida_kal_iptal(&kal);
        }
        onceki_durum = durum;
        if (ida_kal_mesgul(&kal)) {
            const uint32_t k1 = ida_bsp_cevrim();
            ida_kal_adim(&kal);
            const uint32_t kd = ida_bsp_cevrim() - k1;
            if (kd > kal_kayit_en_kotu_tur) {
                kal_kayit_en_kotu_tur = kd;
            }
            if (!ida_kal_mesgul(&kal)) {
                const uint64_t s = ida_bsp_zaman_us() - kal_kayit_bas_us;
                kal_kayit_sure_us = s > UINT32_MAX ? UINT32_MAX : (uint32_t)s;
            }
        }

        /* ---- A: gönderim (sürücüye aynı anda tek çerçeve; A1-K110) ---- */
        if (baglanti) {
            (void)ida_gon_pompa(&gon, &tasima, ida_bsp_zaman_us());
        }

        /* ---- A: 200 Hz ızgara: periyot sınırı ---- */
        uint32_t g = 0u;
        const uint64_t g_ani = ida_bsp_izgara_ani_us(&g);
        ida_izgara_adim_t adim;
        if (ida_izgara_ilerle(&izgara, g, &adim)) {
            izgara_kacirma += adim.kacan_200hz;
            izgara_islenen++;
            const uint64_t simdi = ida_bsp_zaman_us();
            const uint64_t gecikme = simdi > g_ani ? simdi - g_ani : 0u;
            if (gecikme > izgara_gecikme_en_kotu) {
                izgara_gecikme_en_kotu = gecikme > UINT32_MAX ? UINT32_MAX : (uint32_t)gecikme;
            }
            ida_ist_ekle(&periyot_ist, periyot_cevrim);
            if (periyot_cevrim >= periyot_ist.en_kotu_cevrim) {
                periyot_en_kotu_bayt = periyot_bayt;
            }
            periyot_cevrim = 0u;
            periyot_bayt = 0u;
            ida_gon_periyot(&gon, simdi);
            /* ---- B: periyot başına bir arka plan dilimi ---- */
            ida_cagirici_arka_plan(ida_bsp_zaman_us());
        }

        /* ---- B: 1 Hz işleri ---- */
        uint64_t simdi = ida_bsp_zaman_us();
        if (simdi >= sonraki_olcum) {
            sonraki_olcum += OLCUM_ARALIK_US;
            if (simdi >= sonraki_olcum) {
                sonraki_olcum = simdi + OLCUM_ARALIK_US;
            }
            capraz_denetim();
            olcum_guncelle(simdi);
            if (led.yesil) {
                ida_bsp_led_yesil_cevir();
            }
        }

        /* ---- A: NVIC bekçisi, sağlık ve gözetim ---- */
        nvic_yeniden = ida_bsp_nvic_bekci();
        simdi = ida_bsp_zaman_us();
        saglik(simdi);
#if defined(IDA_OLCUM)
        sinama();
#endif
        const uint32_t tur = ida_bsp_cevrim() - tur_bas;
        if (tur > tur_en_kotu) {
            tur_en_kotu = tur;
        }
        tur_sayisi++;
    }
}
