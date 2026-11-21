/*
 * main.c — kart yazılımının ana döngüsü (01 §2.4; W3-kart dilim 1).
 *
 * RTOS'suz tek bağlam (iskelet §8). Her tur:
 *   A  alım: sürücüden olay sınırlı bloklar (en çok B_ayr = 1 KB) tel katmanına BLOK hâlinde
 *      verilir (ida_link_al; bayt bayt besleme yok); 200 Hz ızgara (TIM2 karşılaştırma)
 *      ilerlediyse kaçırma sayılır, 10 ms'lik mutlak ızgarada çekirdek tick'i (A4-K18)
 *      çağrılır; 1 Hz IDA_LINK_STATS ve 200 ms IDA_BOARD_HELLO duyurusu; sağlık ve IWDG.
 *   B  her 5 ms'lik periyotta bir arka plan dilimi; 1 Hz: saat çapraz denetimi, yığın, ölçüm
 *      bloğu, LED.
 * Bloklayan bekleme yoktur (01 §2.4); taşımaya sığmayan tanılama çerçevesi atılır ve sayılır.
 *
 * Dilim 1'de A1 oturumu yoktur: çözülen çerçeveler yalnız sayılır (içerikleri işlenmez) ve
 * çekirdeğe girdi verilmez. IDA_LINK_STATS yayını bu dilimin duman testine özgüdür ve A1-K11'den
 * sapar (tanilama.h); dilim 2'de oturum modülüne bağlanır.
 *
 * fw-measure (IDA_OLCUM): ida_fw_sinama_komutu'na SWD ile yazılan değer sınama yapar:
 *   0x49574447 ("IWDG")  ana döngü kasıtlı durur, IWDG beslenmez -> IWDG sıfırlaması
 *   0x48415441 ("HATA")  tanımsız komut -> UsageFault -> hata kaydı ve sıfırlama
 */
#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#include "bsp.h"
#include "cagirici.h"
#include "ida/port.h"
#include "ida_link/tel.h"
#include "istatistik.h"
#include "olcum.h"
#include "saglik.h"
#include "seri.h"
#include "tanilama.h"

#ifndef IDA_FW_GIT
#define IDA_FW_GIT 0u
#endif
#define IDA_FW_SURUM 0x000100u /* 0.1.0: W3-kart dilim 1 */

#define B_AYR 1024u               /* A geçişinde bir turda ayrıştırılan en çok bayt (01 §2.4) */
#define TANILAMA_ARALIK_US 1000000u /* IDA_LINK_STATS, 1 Hz (A1-K91)            */
#define DUYURU_ARALIK_US 200000u    /* T_duyuru = 200 ms (A1-K11)               */
#define SAGLIK_S_US 20000u          /* ilerleme koşulu S = 20 ms (01 §2.8)       */

#define SINAMA_IWDG 0x49574447u
#define SINAMA_HATA 0x48415441u

volatile uint32_t ida_fw_olcum[IDA_OL_SAYI] __attribute__((used));
#if defined(IDA_OLCUM)
volatile uint32_t ida_fw_sinama_komutu __attribute__((used));
#endif

static ida_link_alici_t alici;
static ida_link_gonderici_t gonderici;
static ida_link_cerceve_t cerceve;
static uint8_t blok[B_AYR];
static uint8_t cikis[IDA_LINK_CERCEVE_AZAMI];

static ida_fw_duyuru_t duyuru;
static ida_fw_plat_sayac_t plat;
static uint32_t saat_adim;

/* Ölçüm sayaçları (01 §2.11) */
static ida_ist_t blok_ist;
static uint64_t blok_bayt;
static uint32_t blok_en_kotu_bayt;
static ida_ist_t periyot_ist;
static uint32_t periyot_cevrim, periyot_bayt, periyot_en_kotu_bayt;
static uint32_t cerceve_sayisi;
static uint32_t tur_sayisi, tur_en_kotu;
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
static uint32_t olcum_sira; /* ölçüm bloğunun yayın sıra numarası (çift: tutarlı) */
static uint32_t rx_bekleme_en_uzun; /* bekleyen alımın tüketilmeden geçen en uzun süresi (µs) */
static uint32_t besleme_ret_son;    /* son besleme reddinin duvar saati (ms) */

static void olcum_yaz64(uint32_t lo_indis, uint64_t v)
{
    ida_fw_olcum[lo_indis] = (uint32_t)v;
    ida_fw_olcum[lo_indis + 1u] = (uint32_t)(v >> 32);
}

static void olcum_ist(uint32_t kutu_indis, const ida_ist_t *s)
{
    for (uint32_t i = 0u; i < IDA_IST_KUTU; i++) {
        ida_fw_olcum[kutu_indis + i] = s->kutu[i];
    }
}

/* Çözülen çerçeveyi bekleyen bloğu tel katmanına verir (blok besleme; ida_link_al bir çağrıda
   en çok bir çerçeve teslim eder, kalan baytlarla yeniden çağrılır). */
static void ayristir(const uint8_t *veri, size_t n, bool kaba)
{
    size_t i = 0u;
    while (i < n) {
        size_t t = 0u;
        const ida_link_rc_t rc = ida_link_al(&alici, veri + i, n - i, &t, &cerceve);
        if (rc == IDA_LINK_CERCEVE) {
            cerceve_sayisi++; /* dilim 1: içerik işlenmez (oturum dilim 2'de) */
            if (kaba) {
                /* Son baytı kaba damgalı blokta: A1-K48'in stamp_coarse'u. */
                plat.stamp_coarse++;
            }
        } else if (rc != IDA_LINK_BEKLE) {
            break;
        }
        if (t == 0u) {
            break;
        }
        i += t;
    }
}

static void gonder(size_t n)
{
    if (ida_seri_yaz(cikis, n)) {
        plat.tx_frames++;
    } else {
        plat.tx_dropped++;
    }
}

static uint32_t rx_hata_toplami(void)
{
    const ida_link_sayaclar_t *s = &alici.sayac;
    return s->rx_crc_err + s->rx_iflag + s->rx_bad_len + s->rx_overflow + s->rx_unknown_msgid;
}

static void olcum_guncelle(uint64_t simdi)
{
    const ida_cagirici_t *c = ida_cagirici_durum();
    const ida_acilis_sonuc_t *a = ida_bsp_acilis();
    const ida_acilis_kaydi_t *k = ida_bsp_kayit();
    ida_seri_sayac_t s;
    ida_seri_sayaclar(&s);
    volatile uint32_t *o = ida_fw_olcum;
    /* Yayın sıra numarası: güncelleme sürerken tek, bitince çift. SWD okuyucusu bloğu ve sonra
       sıra numarasını yeniden okur; ikisi aynı ve çift değilse okumayı reddeder
       (firmware/araclar/olcum_oku.py). */
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
    o[IDA_OL_izgara10_kacirma] = plat.grid_missed;
    o[IDA_OL_besleme_sayisi] = besleme_sayisi;
    o[IDA_OL_besleme_aralik_en_kotu_us] = besleme_aralik_en_kotu;
    o[IDA_OL_besleme_ret] = besleme_ret;
    o[IDA_OL_besleme_ret_bitleri] = besleme_ret_bitleri;
    o[IDA_OL_cekirdek_durum] = (uint32_t)c->durum | ((uint32_t)c->neden << 8);
    o[IDA_OL_init_rc] = c->init_rc;
    o[IDA_OL_core_size] = c->core_size;
    o[IDA_OL_core_ok] = c->core_ok ? 1u : 0u;
    o[IDA_OL_tick_sayisi] = c->tick.sayi;
    o[IDA_OL_tick_son_cevrim] = c->tick.son_cevrim;
    o[IDA_OL_tick_en_kotu_cevrim] = c->tick.en_kotu_cevrim;
    o[IDA_OL_tick_en_iyi_cevrim] = c->tick.en_iyi_cevrim;
    olcum_yaz64(IDA_OL_tick_toplam_cevrim_lo, c->tick.toplam_cevrim);
    olcum_ist(IDA_OL_tick_kutu, &c->tick);
    o[IDA_OL_tick_rc_hata] = c->tick_rc_hata;
    o[IDA_OL_tick_son_rc] = c->tick_son_rc;
    o[IDA_OL_tick_butce_asimi] = c->tick_butce_asimi;
    o[IDA_OL_bg_sayisi] = c->arka_plan.sayi;
    o[IDA_OL_bg_en_kotu_cevrim] = c->arka_plan.en_kotu_cevrim;
    o[IDA_OL_bg_more] = c->bg_more;
    o[IDA_OL_blok_sayisi] = blok_ist.sayi;
    o[IDA_OL_blok_en_kotu_cevrim] = blok_ist.en_kotu_cevrim;
    o[IDA_OL_blok_en_kotu_bayt] = blok_en_kotu_bayt;
    o[IDA_OL_blok_en_iyi_cevrim] = blok_ist.en_iyi_cevrim;
    olcum_yaz64(IDA_OL_blok_toplam_cevrim_lo, blok_ist.toplam_cevrim);
    olcum_yaz64(IDA_OL_blok_bayt_lo, blok_bayt);
    olcum_ist(IDA_OL_blok_kutu, &blok_ist);
    o[IDA_OL_periyot_sayisi] = periyot_ist.sayi;
    o[IDA_OL_periyot_en_kotu_cevrim] = periyot_ist.en_kotu_cevrim;
    o[IDA_OL_periyot_en_kotu_bayt] = periyot_en_kotu_bayt;
    olcum_ist(IDA_OL_periyot_kutu, &periyot_ist);
    o[IDA_OL_cerceve_sayisi] = cerceve_sayisi;
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
    o[IDA_OL_tx_cerceve] = plat.tx_frames;
    o[IDA_OL_tx_atilan] = plat.tx_dropped;
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
    o[IDA_OL_nonce] = duyuru.nonce;
    o[IDA_OL_nvic_yeniden] = nvic_yeniden;
    o[IDA_OL_rx_yeniden] = s.rx_yeniden;
    o[IDA_OL_rx_durdu] = s.rx_durdu;
    o[IDA_OL_stamp_coarse] = plat.stamp_coarse;
    o[IDA_OL_rx_bekleme_en_uzun_us] = rx_bekleme_en_uzun;
    o[IDA_OL_besleme_ret_son_ms] = besleme_ret_son;
    ida_bsp_bariyer();
    olcum_sira++;
    o[IDA_OL_yayin_sira] = olcum_sira;
}

/* DWT ile duvar saatinin birbirini doğrulaması: iki 1 Hz örnek arasında çevrim farkı ile
   (µs farkı × 160) arasındaki sapma (çevrim). Aynı saat kaynağından türedikleri için sapma
   yalnız okuma anı farkından (±1 µs = ±160 çevrim) gelmelidir. */
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

/* 01 §2.8 ilerleme koşulları (dilim 1'in alt kümesi; karar app/saglik.c); hepsi tutarsa IWDG
   beslenir. */
static void saglik(uint64_t simdi)
{
    const ida_cagirici_t *c = ida_cagirici_durum();
    const bool bekleyen = ida_seri_bekleyen();
    ida_saglik_girdi_t g;
    g.cekirdek_normal = c->durum == IDA_CAG_NORMAL;
    g.core_ok = c->core_ok;
    g.son_cagri_us = c->son_cagri_us;
    g.rx_tamam = ida_rx_gozetim_tamam(&rx_gozetim, bekleyen, simdi, SAGLIK_S_US);
    if (bekleyen && rx_gozetim.bekliyordu && simdi - rx_gozetim.son_ilerleme_us > rx_bekleme_en_uzun) {
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
        besleme_ret_son = (uint32_t)(simdi / 1000u); /* ms */
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
    if (k == SINAMA_IWDG) {
        ida_bsp_led_kirmizi(true);
        for (;;) {
            /* Kasıtlı durma: IWDG beslenmez (W3-kart kabul md. 6). */
        }
    }
    if (k == SINAMA_HATA) {
        ida_fw_sinama_komutu = 0u;
        __builtin_trap(); /* UDF -> UsageFault (UNDEFINSTR) */
    }
}
#endif

int main(void)
{
    ida_bsp_erken_baslat();
    saat_adim = ida_bsp_saat_kur();
    ida_bsp_cevre_baslat();
    const bool saat_tamam = saat_adim == 0u;
    const uint64_t t0 = ida_bsp_zaman_us();

    ida_ist_sifirla(&blok_ist);
    ida_ist_sifirla(&periyot_ist);
    (void)ida_link_alici_kur(&alici, IDA_LINK_ROL_KART);
    (void)ida_link_gonderici_kur(&gonderici, IDA_LINK_ROL_KART);
    bool baglanti = false;
    if (saat_tamam) {
        baglanti = ida_seri_baslat() == 0;
    }

    ida_cagirici_kur(ida_bsp_acilis(), saat_tamam, t0);
    const ida_cagirici_t *cag = ida_cagirici_durum();
    const ida_acilis_sonuc_t *acilis = ida_bsp_acilis();
    memset(&duyuru, 0, sizeof duyuru);
    duyuru.param_schema_hash = cag->param_schema_hash;
    duyuru.nonce = ida_bsp_rastgele(); /* açılıştan sonraki ilk duyuruda çekilir (A1-K13) */
    if (duyuru.nonce == 0u) {
        duyuru.nonce = 1u; /* RNG hata verdi; 0 olamaz (tanılama: ölçüm bloğu nonce = 1) */
    }
    duyuru.boot_count = 0u;
    duyuru.boot_count_ok = 0u;
    duyuru.reset_flags = acilis->reset_flags;
    duyuru.reset_cause = acilis->reset_cause;
    duyuru.consecutive_faults = acilis->ardisik_ariza;
    duyuru.core_version = cag->core_version;
    duyuru.fw_version = IDA_FW_SURUM;
    duyuru.fw_git = IDA_FW_GIT;
    /* Güvenli beklemede duyuru init_rc = NOT_INIT taşır (A1-K13). */
    duyuru.init_rc = cag->durum == IDA_CAG_NORMAL ? cag->init_rc : (uint8_t)IDA_RC_NOT_INIT;

    ida_bsp_calisma_kipi();
    const ida_led_t led = ida_led_durumu(saat_tamam, baglanti, cag->durum == IDA_CAG_NORMAL);
    ida_bsp_led_yesil(led.yesil);
    ida_bsp_led_kirmizi(led.kirmizi);

    uint64_t sonraki_tanilama = ida_bsp_zaman_us() + TANILAMA_ARALIK_US;
    uint64_t sonraki_duyuru = ida_bsp_zaman_us();
    ida_izgara_kur(&izgara, ida_bsp_izgara_sayaci());
    ida_rx_gozetim_kur(&rx_gozetim, ida_bsp_zaman_us());
    olcum_guncelle(ida_bsp_zaman_us());

    for (;;) {
        const uint32_t tur_bas = ida_bsp_cevrim();

        /* ---- A: alım ve ayrıştırma (blok besleme) ---- */
        if (baglanti) {
            size_t bu_tur = 0u;
            bool ilerledi = false;
            while (bu_tur < B_AYR) {
                uint64_t t_rx = 0u;
                bool kaba = false;
                bool tasma = false;
                const size_t n = ida_seri_oku(blok, sizeof blok, &t_rx, &kaba, &tasma);
                if (tasma) {
                    (void)ida_link_alici_tasma(&alici); /* A1-K09: yeniden eşleme */
                    ilerledi = true;                    /* atılan bayt da tüketimdir */
                }
                if (n == 0u) {
                    break;
                }
                ilerledi = true;
                const uint32_t c0 = ida_bsp_cevrim();
                ayristir(blok, n, kaba);
                const uint32_t c = ida_bsp_cevrim() - c0;
                ida_ist_ekle(&blok_ist, c);
                if (c >= blok_ist.en_kotu_cevrim) {
                    blok_en_kotu_bayt = (uint32_t)n;
                }
                blok_bayt += n;
                periyot_cevrim += c;
                periyot_bayt += (uint32_t)n;
                bu_tur += n;
            }
            /* Alım ilerleme gözetimi: süre yalnız gerçek tüketimde ve yeni beklemenin başında
               yenilenir (01 §2.8 koşul 5). */
            ida_rx_gozetim_guncelle(&rx_gozetim, ida_seri_bekleyen(), ilerledi, ida_bsp_zaman_us());
        }

        /* ---- A: ızgara ---- */
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
            /* Periyot başına ayrıştırma toplamı (5 ms). */
            ida_ist_ekle(&periyot_ist, periyot_cevrim);
            if (periyot_cevrim >= periyot_ist.en_kotu_cevrim) {
                periyot_en_kotu_bayt = periyot_bayt;
            }
            periyot_cevrim = 0u;
            periyot_bayt = 0u;

            /* 10 ms mutlak ızgara (g'nin çift değerleri): çekirdek tick'i (A4-K18). */
            if (adim.tick != 0u) {
                plat.grid_missed += adim.kacan_10ms;
                ida_cagirici_plat_t p;
                p.cnt_grid_missed = plat.grid_missed;
                p.cnt_rx_error = rx_hata_toplami();
                ida_cagirici_tick(ida_bsp_zaman_us(), &p);
            }

            /* ---- B: periyot başına bir arka plan dilimi ---- */
            ida_cagirici_arka_plan(ida_bsp_zaman_us());
        }

        /* ---- A: tanılama ve duyuru ---- */
        uint64_t simdi = ida_bsp_zaman_us();
        if (baglanti && simdi >= sonraki_duyuru) {
            size_t n = 0u;
            if (ida_fw_duyuru_kodla(&gonderici, &duyuru, cikis, sizeof cikis, &n) == IDA_LINK_TAMAM) {
                gonder(n);
            }
            sonraki_duyuru += DUYURU_ARALIK_US;
            if (simdi >= sonraki_duyuru) {
                sonraki_duyuru = simdi + DUYURU_ARALIK_US; /* birikmez */
            }
        }
        if (simdi >= sonraki_tanilama) {
            if (baglanti) {
                size_t n = 0u;
                if (ida_fw_link_stats_kodla(&gonderici, &alici.sayac, &plat, cikis, sizeof cikis,
                                            &n) == IDA_LINK_TAMAM) {
                    gonder(n);
                }
            }
            sonraki_tanilama += TANILAMA_ARALIK_US;
            if (simdi >= sonraki_tanilama) {
                sonraki_tanilama = simdi + TANILAMA_ARALIK_US;
            }
            /* ---- B: 1 Hz işleri ---- */
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
