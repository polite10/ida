/*
 * olcum.h — kartın ölçüm bloğu (W3-kart dilim 1; 01 §2.11'in "Adım", "Bağlantı", "Döngü ve
 * sistem" gruplarının bu dilimdeki karşılığı).
 *
 * A1'de bu değerleri taşıyan mesaj yoktur (sözleşmede olmayan alan uydurulmaz); blok RAM'dedir
 * ve PC'den hata ayıklama bağlantısıyla (aynı ST-LINK kablosu, SWD; işlemci durdurulmadan)
 * okunur: firmware/araclar/olcum_oku.py. Blok her sürüm derlemesinde vardır; ana döngü onu
 * 1 Hz'te günceller (okuma en çok 1 s bayattır).
 *
 * Tutarlılık: yayin_sira güncelleme sürerken tek, bitince çifttir (sıra kilidi). Okuyucu bloğu
 * okur, sonra yayin_sira'yı yeniden okur; ikisi eşit ve çift değilse okumayı yineler.
 *
 * Düzen: 32 bitlik sözcük dizisi; sözcüklerin adı ve sırası aşağıdaki X-makrosundadır. PC aracı
 * bu dosyayı okuyup aynı sırayı çıkarır (tek kaynak). Adı `_i` ile biten sözcük işaretlidir;
 * `_lo`/`_hi` çiftleri 64 bitlik değerin alt ve üst yarısıdır; XK(ad) 16 sözcüklük histogramdır
 * (kutu sınırları istatistik.h). Süreler, adı `_cevrim` ile bitiyorsa DWT çevrimidir (160 MHz).
 */
#ifndef IDA_FW_OLCUM_H
#define IDA_FW_OLCUM_H

#include <stdint.h>

#define IDA_FW_OLCUM_MAGIC UINT32_C(0x4F414449) /* "IDAO" */
#define IDA_FW_OLCUM_SURUM 1u

/* clang-format off */
#define IDA_FW_OLCUM_ALANLARI(X, XK) \
    X(magic) X(surum) X(sozcuk_sayisi) X(yayin_sira) X(fw_git) X(derleme_olcum) \
    X(sysclk_hz) X(usart_hz) X(baud_brr) X(brr) X(saat_adim) \
    X(zaman_us_lo) X(zaman_us_hi) \
    X(capraz_ornek) X(capraz_sapma_en_kucuk_i) X(capraz_sapma_en_buyuk_i) X(capraz_son_aralik_us) \
    X(tur_sayisi) X(tur_en_kotu_cevrim) \
    X(izgara_sayisi) X(izgara_kacirma) X(izgara_gecikme_en_kotu_us) X(izgara10_kacirma) \
    X(besleme_sayisi) X(besleme_aralik_en_kotu_us) X(besleme_ret) X(besleme_ret_bitleri) \
    X(cekirdek_durum) X(init_rc) X(core_size) X(core_ok) \
    X(tick_sayisi) X(tick_son_cevrim) X(tick_en_kotu_cevrim) X(tick_en_iyi_cevrim) \
    X(tick_toplam_cevrim_lo) X(tick_toplam_cevrim_hi) XK(tick_kutu) \
    X(tick_rc_hata) X(tick_son_rc) X(tick_butce_asimi) \
    X(bg_sayisi) X(bg_en_kotu_cevrim) X(bg_more) \
    X(blok_sayisi) X(blok_en_kotu_cevrim) X(blok_en_kotu_bayt) X(blok_en_iyi_cevrim) \
    X(blok_toplam_cevrim_lo) X(blok_toplam_cevrim_hi) X(blok_bayt_lo) X(blok_bayt_hi) XK(blok_kutu) \
    X(periyot_sayisi) X(periyot_en_kotu_cevrim) X(periyot_en_kotu_bayt) XK(periyot_kutu) \
    X(cerceve_sayisi) \
    X(ore) X(fe) X(ne) X(pe) X(dma_rx_hata) X(dma_tx_hata) \
    X(rx_kararsiz) X(rx_belirsiz) X(rx_gecersiz) X(rx_geri) X(rx_olay_birlesme) \
    X(olay_eslesme) X(olay_bosta) X(olay_yarim) X(olay_tam) \
    X(rx_tasma) X(rx_tasma_bayt) X(rx_halka_azami) X(uart_bekle_hata) \
    X(tx_bayt) X(tx_ret) X(tx_doluluk_azami) X(tx_cerceve) X(tx_atilan) \
    X(yigin_boyu) X(yigin_en_yuksek) \
    X(reset_cause) X(reset_flags) X(ardisik_ariza) X(kayit_gecerliydi) X(fpscr) \
    X(onceki_ariza) X(ariza_sayisi) X(ariza_cfsr) X(ariza_hfsr) X(ariza_pc) X(ariza_lr) \
    X(ariza_yigin_gecerli) X(ariza_istisna) \
    X(nonce) X(sinama_komutu) X(nvic_yeniden) X(rx_yeniden) X(rx_durdu) X(stamp_coarse) \
    X(rx_bekleme_en_uzun_us) X(besleme_ret_son_ms)
/* clang-format on */

#define IDA_FW_OLCUM_X(ad) IDA_OL_##ad,
#define IDA_FW_OLCUM_XK(ad) IDA_OL_##ad, IDA_OL_##ad##_son = IDA_OL_##ad + 15,
enum { IDA_FW_OLCUM_ALANLARI(IDA_FW_OLCUM_X, IDA_FW_OLCUM_XK) IDA_OL_SAYI };
#undef IDA_FW_OLCUM_X
#undef IDA_FW_OLCUM_XK

/* Blok (app/main.c tanımlar; SWD ile okunur). */
extern volatile uint32_t ida_fw_olcum[IDA_OL_SAYI];

#endif /* IDA_FW_OLCUM_H */
