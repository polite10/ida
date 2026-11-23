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
#define IDA_FW_OLCUM_SURUM 3u

/* clang-format off */
#define IDA_FW_OLCUM_ALANLARI(X, XK) \
    X(magic) X(surum) X(sozcuk_sayisi) X(yayin_sira) X(fw_git) X(derleme_olcum) \
    X(sysclk_hz) X(usart_hz) X(baud_brr) X(brr) X(saat_adim) \
    X(zaman_us_lo) X(zaman_us_hi) \
    X(capraz_ornek) X(capraz_sapma_en_kucuk_i) X(capraz_sapma_en_buyuk_i) X(capraz_son_aralik_us) \
    X(tur_sayisi) X(tur_en_kotu_cevrim) \
    X(izgara_sayisi) X(izgara_kacirma) X(izgara_gecikme_en_kotu_us) \
    X(besleme_sayisi) X(besleme_aralik_en_kotu_us) X(besleme_ret) X(besleme_ret_bitleri) \
    X(besleme_ret_son_ms) X(rx_bekleme_en_uzun_us) \
    X(cekirdek_durum) X(init_rc) X(core_size) X(core_ok) \
    X(step_sayisi) X(step_son_cevrim) X(step_en_kotu_cevrim) X(step_en_iyi_cevrim) \
    X(step_toplam_cevrim_lo) X(step_toplam_cevrim_hi) XK(step_kutu) X(step_butce_asimi) \
    X(tick_sayisi) X(tick_en_kotu_cevrim) X(tick_en_iyi_cevrim) \
    X(tick_toplam_cevrim_lo) X(tick_toplam_cevrim_hi) XK(tick_kutu) X(tick_butce_asimi) \
    X(cagri_rc_hata) X(cagri_son_rc) X(cagri_tur_en_kotu_cevrim) \
    X(cagri_turu_sayi) X(cagri_turu_asim) X(cagri_turu_asim_ps) X(cagri_turu_en_kotu_us) \
    X(cagri_turu_ps_en_kotu_us) X(cagri_turu_sinir_us) \
    X(ps_sayi) X(ps_son_tur_cevrim) X(ps_son_step_cevrim) X(ps_son_skart_us) X(ps_son_dongu_cevrim) \
    X(ol_fark_sayi) X(ol_fark_en_kotu_cevrim) X(ol_fark_toplam_cevrim_lo) X(ol_fark_toplam_cevrim_hi) \
    X(bg_sayisi) X(bg_en_kotu_cevrim) X(bg_more) \
    X(skart_sayisi) X(skart_en_kotu_cevrim) X(skart_en_iyi_cevrim) \
    X(skart_toplam_cevrim_lo) X(skart_toplam_cevrim_hi) XK(skart_kutu) \
    X(blok_sayisi) X(blok_en_kotu_cevrim) X(blok_en_kotu_bayt) X(blok_en_iyi_cevrim) \
    X(blok_toplam_cevrim_lo) X(blok_toplam_cevrim_hi) X(blok_bayt_lo) X(blok_bayt_hi) XK(blok_kutu) \
    XK(bb_sayi) XK(bb_bayt) XK(bb_cevrim) \
    X(periyot_sayisi) X(periyot_en_kotu_cevrim) X(periyot_en_kotu_bayt) XK(periyot_kutu) \
    X(ore) X(fe) X(ne) X(pe) X(dma_rx_hata) X(dma_tx_hata) \
    X(rx_kararsiz) X(rx_belirsiz) X(rx_gecersiz) X(rx_geri) X(rx_olay_birlesme) \
    X(olay_eslesme) X(olay_bosta) X(olay_yarim) X(olay_tam) \
    X(rx_tasma) X(rx_tasma_bayt) X(rx_halka_azami) X(uart_bekle_hata) \
    X(tx_bayt) X(tx_ret) X(tx_doluluk_azami) \
    X(gon_tx_frames) X(gon_tx_dropped) X(gon_crit_stall) X(gon_kritik_ret) X(gon_zorunlu_ret) \
    X(gon_yazma_ret) X(gon_kritik_bekleme_en_kotu_us) X(gon_q1_azami) X(gon_q2_azami) X(gon_q3_azami) \
    X(gon_gecersiz_ret) X(gon_temizlenen) X(gon_kritik_hata) X(kapanis_neden) \
    X(kapanis_evre) X(kapanis_oturum_sonu_teslim) X(son_armed) X(son_mode) X(son_fc_state) \
    X(son_fc_inhibit) X(son_link_event) \
    X(rng_tohum_hata) X(rng_saat_hata) X(rng_kurtarma) X(rng_atilan) X(rng_kurtarilamaz) \
    X(gon_sira_giris_kesinti) X(gon_sira_giris_eksik) X(gon_sira_kuyruk_kesinti) \
    X(gon_sira_kuyruk_eksik) X(gon_sira_cikis_kesinti) X(gon_sira_cikis_eksik) \
    X(link_durum) X(link_sess) \
    X(say_rx_bytes) X(say_rx_frames) X(say_rx_crc_err) X(say_rx_unknown_msgid) X(say_rx_iflag) \
    X(say_rx_bad_len) X(say_rx_wrong_sys) X(say_rx_overflow) X(say_rx_resync_bytes) X(say_rx_seq_gap) \
    X(say_stale_session) X(say_outside_session) X(say_stamp_coarse) X(say_batch_closed) \
    X(say_batch_incomplete) X(say_batch_skipped) X(say_batch_old) X(say_batch_dup) \
    X(say_batch_merged) X(say_member_overflow) X(say_grid_missed) X(say_out_fixed) \
    X(say_events_sent) X(say_events_resent) X(say_sessions) \
    X(kal_boot_count) X(kal_boot_ok) X(kal_store) X(kal_fail) X(kal_store_gen) X(kal_blok_gen) \
    X(kal_blok_len) X(kal_blok_crc) X(kal_gecerli_sayfa_i) X(kal_kayit_sayisi) X(kal_kayit_hata) \
    X(kal_sayac_deneme) X(kal_acilis_cevrim) X(kal_kayit_sure_us) X(kal_kayit_en_kotu_tur_cevrim) \
    X(fl_silme_sayisi) X(fl_yazma_sayisi) X(fl_silme_en_kotu_cevrim) X(fl_yazma_en_kotu_cevrim) \
    X(fl_hata) X(fl_ecc_nmi) X(fl_eccc_nvm) X(fl_eccc_diger) X(fl_eccd_diger) X(fl_ecc_son) \
    X(fl_islem_en_uzun_us) X(ardisik_kayit) X(ardisik_temizleme) \
    X(yigin_boyu) X(yigin_en_yuksek) \
    X(reset_cause) X(reset_flags) X(ardisik_ariza) X(kayit_gecerliydi) X(fpscr) \
    X(onceki_ariza) X(ariza_sayisi) X(ariza_cfsr) X(ariza_hfsr) X(ariza_pc) X(ariza_lr) \
    X(ariza_yigin_gecerli) X(ariza_istisna) \
    X(sinama_komutu) X(sinama_sonuc) X(nvic_yeniden) X(rx_yeniden) X(rx_durdu)
/* clang-format on */

#define IDA_FW_OLCUM_X(ad) IDA_OL_##ad,
#define IDA_FW_OLCUM_XK(ad) IDA_OL_##ad, IDA_OL_##ad##_son = IDA_OL_##ad + 15,
enum { IDA_FW_OLCUM_ALANLARI(IDA_FW_OLCUM_X, IDA_FW_OLCUM_XK) IDA_OL_SAYI };
#undef IDA_FW_OLCUM_X
#undef IDA_FW_OLCUM_XK

/* Blok (app/main.c tanımlar; SWD ile okunur). */
extern volatile uint32_t ida_fw_olcum[IDA_OL_SAYI];

#endif /* IDA_FW_OLCUM_H */
